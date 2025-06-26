/* psipe_queue.c - psipe ioctl operations queue management
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "psipe_module.h"

struct psipe_op *psipe_ops_new(unsigned int cmd, unsigned long uarg)
{
	int rv;
	struct psipe_data data;

	struct psipe_op *op = kmalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		goto out;

	switch(cmd) {
	case PSIPE_IOCTL_SEND:
		rv = copy_from_user(&data, (void *)uarg, sizeof(data));
		if (rv < 0)
			goto clean;
		op->dma.addr = data.addr;
		op->dma.len = data.len;
		op->dma.mode = PSIPE_MODE_OFF;
		op->ioctl_fn = psipe_ioctl_send;
		break;
	case PSIPE_IOCTL_RECV:
		rv = copy_from_user(&data, (void *)uarg, sizeof(data));
		if (rv < 0)
			goto clean;
		op->dma.addr = data.addr;
		op->dma.len = data.len;
		op->dma.mode = PSIPE_MODE_OFF;
		op->ioctl_fn = psipe_ioctl_recv;
		break;
	default:
		goto clean;
	}

	init_waitqueue_head(&op->waitq);
	atomic_set(&op->nwaiting, 0);
	op->flag = 0;

out:
	return op;

clean:
	kfree(op);
	return NULL;
}

psipe_handle_t psipe_ops_init(struct psipe_dev *psipe_dev, struct psipe_op *op)
{
	struct psipe_ops *ops = &psipe_dev->ops;
	unsigned long flags;
	long rv = 0;
	int empty;

	if (!op)
		return -EINVAL;

	rv = psipe_dma_pin_pages(&op->dma);
	if (rv < 0)
		return rv;

	//pr_info("psipe_dma_pin_pages - success\n");

	spin_lock_irqsave(&ops->lock, flags);
	empty = list_empty(&ops->active); 
	list_add_tail(&op->list, &ops->active);
	op->id = ops->next_id++;
	spin_unlock_irqrestore(&ops->lock, flags);

	if (empty) {
		//pr_info("psipe_ops_init - running op %lu\n", op->id);
		rv = op->ioctl_fn(psipe_dev, &op->dma);
		if (rv < 0)
			return rv;
	}

	return op->id;
}

struct psipe_op *psipe_ops_current(struct psipe_ops *ops)
{
	/* ops->lock must be taken */
	return list_empty(&ops->active) ?
		NULL : list_first_entry(&ops->active, struct psipe_op, list);
}

static void psipe_ops_fini(struct psipe_dev *psipe_dev, struct psipe_op *op)
{
	psipe_dma_unmap_pages(&op->dma, psipe_dev->pdev);
	psipe_dma_unpin_pages(&op->dma);

	/* ops->lock must be taken */
	list_move_tail(&op->list, &psipe_dev->ops.inactive);

	op->flag = 1;
	wake_up_all(&op->waitq);
}

long psipe_ops_wait(struct psipe_op *op)
{
	long rv = -EINVAL;

	if (!op)
		goto out;

	atomic_add(1, &op->nwaiting);
	wait_event(op->waitq, op->flag == 1);
	rv = op->retval;

	if (!atomic_sub_return(1, &op->nwaiting)) {
		list_del(&op->list);
		kfree(op);
	}

out:
	return rv;
}

void psipe_ops_next(struct psipe_dev *psipe_dev)
{
	struct psipe_ops *ops = &psipe_dev->ops;
	struct psipe_op *op;
	unsigned long flags;

	spin_lock_irqsave(&ops->lock, flags);

	op = psipe_ops_current(ops);
	if (!op)
		goto unlock;
	psipe_ops_fini(psipe_dev, op);
	op = psipe_ops_current(ops);
	if (op) {
		//pr_info("psipe_ops_next - running op %lu\n", op->id);
		op->retval = op->ioctl_fn(psipe_dev, &op->dma);
	}

unlock:
	spin_unlock_irqrestore(&ops->lock, flags);
}

struct psipe_op *psipe_ops_get(struct psipe_ops *ops, psipe_handle_t id)
{
	struct psipe_op *cur_op, *op = NULL;
	struct list_head *entry;
	unsigned long flags;

	if (id >= ops->next_id)
		goto out;

	spin_lock_irqsave(&ops->lock, flags);
	list_for_each(entry, &ops->active) {
		cur_op = list_entry(entry, struct psipe_op, list);
		if (cur_op->id == id) {
			op = cur_op;
			goto unlock;
		}
	}
	list_for_each(entry, &ops->inactive) {
		cur_op = list_entry(entry, struct psipe_op, list);
		if (cur_op->id == id) {
			op = cur_op;
			goto unlock;
		}
	}
unlock:
	spin_unlock_irqrestore(&ops->lock, flags);
out:
	return op;
}

int psipe_ops_flush(struct psipe_dev *psipe_dev)
{
	struct psipe_ops *ops = &psipe_dev->ops;
	struct psipe_op *op;
	struct list_head *entry, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&ops->lock, flags);
	list_for_each_safe(entry, tmp, &ops->active) {
		list_del(entry);
		op = list_entry(entry, struct psipe_op, list);
		psipe_dma_unpin_pages(&op->dma);
		psipe_dma_unmap_pages(&op->dma, psipe_dev->pdev);
		kfree(op);
	}
	list_for_each_safe(entry, tmp, &ops->inactive) {
		list_del(entry);
		kfree(list_entry(entry, struct psipe_op, list));
	}
	spin_unlock_irqrestore(&ops->lock, flags);

	return 0;
}
