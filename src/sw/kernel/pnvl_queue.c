/* pnvl_queue.c - pnvl ioctl operations queue management
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "pnvl_module.h"

struct pnvl_op *pnvl_ops_new(unsigned int cmd, unsigned long uarg)
{
	int rv;
	struct pnvl_data data;

	struct pnvl_op *op = kmalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		goto out;

	switch(cmd) {
	case PNVL_IOCTL_SEND:
		rv = copy_from_user(&data, (void *)uarg, sizeof(data));
		if (rv < 0)
			goto clean;
		op->dma.addr = data.addr;
		op->dma.len = data.len;
		op->dma.mode = PNVL_MODE_OFF;
		op->ioctl_fn = pnvl_ioctl_send;
		break;
	case PNVL_IOCTL_RECV:
		rv = copy_from_user(&data, (void *)uarg, sizeof(data));
		if (rv < 0)
			goto clean;
		op->dma.addr = data.addr;
		op->dma.len = data.len;
		op->dma.mode = PNVL_MODE_OFF;
		op->ioctl_fn = pnvl_ioctl_recv;
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

pnvl_handle_t pnvl_ops_init(struct pnvl_dev *pnvl_dev, struct pnvl_op *op)
{
	struct pnvl_ops *ops = &pnvl_dev->ops;
	unsigned long flags;
	long rv = 0;
	int empty;

	if (!op)
		return -EINVAL;

	rv = pnvl_dma_pin_pages(&op->dma);
	if (rv < 0)
		return rv;

	//pr_info("pnvl_dma_pin_pages - success\n");

	spin_lock_irqsave(&ops->lock, flags);
	empty = list_empty(&ops->active); 
	list_add_tail(&op->list, &ops->active);
	op->id = ops->next_id++;
	spin_unlock_irqrestore(&ops->lock, flags);

	if (empty) {
		//pr_info("pnvl_ops_init - running op %lu\n", op->id);
		rv = op->ioctl_fn(pnvl_dev, &op->dma);
		if (rv < 0)
			return rv;
	}

	return op->id;
}

struct pnvl_op *pnvl_ops_current(struct pnvl_ops *ops)
{
	/* ops->lock must be taken */
	return list_empty(&ops->active) ?
		NULL : list_first_entry(&ops->active, struct pnvl_op, list);
}

static void pnvl_ops_fini(struct pnvl_dev *pnvl_dev, struct pnvl_op *op)
{
	pnvl_dma_unmap_pages(&op->dma, pnvl_dev->pdev);
	pnvl_dma_unpin_pages(&op->dma);

	/* ops->lock must be taken */
	list_move_tail(&op->list, &pnvl_dev->ops.inactive);

	op->flag = 1;
	wake_up_all(&op->waitq);
}

long pnvl_ops_wait(struct pnvl_op *op)
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

void pnvl_ops_next(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_ops *ops = &pnvl_dev->ops;
	struct pnvl_op *op;
	unsigned long flags;

	spin_lock_irqsave(&ops->lock, flags);

	op = pnvl_ops_current(ops);
	if (!op)
		goto unlock;
	pnvl_ops_fini(pnvl_dev, op);
	op = pnvl_ops_current(ops);
	if (op) {
		//pr_info("pnvl_ops_next - running op %lu\n", op->id);
		op->retval = op->ioctl_fn(pnvl_dev, &op->dma);
	}

unlock:
	spin_unlock_irqrestore(&ops->lock, flags);
}

struct pnvl_op *pnvl_ops_get(struct pnvl_ops *ops, pnvl_handle_t id)
{
	struct pnvl_op *cur_op, *op = NULL;
	struct list_head *entry;
	unsigned long flags;

	if (id >= ops->next_id)
		goto out;

	spin_lock_irqsave(&ops->lock, flags);
	list_for_each(entry, &ops->active) {
		cur_op = list_entry(entry, struct pnvl_op, list);
		if (cur_op->id == id) {
			op = cur_op;
			goto unlock;
		}
	}
	list_for_each(entry, &ops->inactive) {
		cur_op = list_entry(entry, struct pnvl_op, list);
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

int pnvl_ops_flush(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_ops *ops = &pnvl_dev->ops;
	struct pnvl_op *op;
	struct list_head *entry, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&ops->lock, flags);
	list_for_each_safe(entry, tmp, &ops->active) {
		list_del(entry);
		op = list_entry(entry, struct pnvl_op, list);
		pnvl_dma_unpin_pages(&op->dma);
		pnvl_dma_unmap_pages(&op->dma, pnvl_dev->pdev);
		kfree(op);
	}
	list_for_each_safe(entry, tmp, &ops->inactive) {
		list_del(entry);
		kfree(list_entry(entry, struct pnvl_op, list));
	}
	spin_unlock_irqrestore(&ops->lock, flags);

	return 0;
}
