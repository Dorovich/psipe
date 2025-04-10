/* pnvl_queue.c - pnvl ioctl operations queue management
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "pnvl_module.h"

struct pnvl_op *pnvl_op_new(unsigned int cmd, unsigned long uarg)
{
	struct pnvl_op *op = kmalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		goto out;

	init_waitqueue_head(&op->waitq);
	op->nwaiting = 0;
	op->flag = 0;
	op->command = cmd;
	op->uarg = uarg;

out:
	return op;
}

pnvl_handle_t pnvl_op_add(struct pnvl_ops *ops, struct pnvl_op *op)
{
	if (!op)
		return -EINVAL;

	mutex_lock(&ops->lock);
	list_add_tail(&op->list, &ops->queue);
	mutex_unlock(&ops->lock);

	op->id = ops->next_id++;

	return op->id;
}

struct pnvl_op *pnvl_op_first(struct pnvl_ops *ops)
{
	struct pnvl_op *op = NULL;

	mutex_lock(&ops->lock);
	if (!list_empty(&ops->queue))
		op = list_first_entry(&ops->queue, struct pnvl_op, list);
	mutex_unlock(&ops->lock);

	return op;
}

static void pnvl_op_step(struct pnvl_ops *ops, struct pnvl_op *op)
{
	mutex_lock(&ops->lock);
	list_del(&op->list);	
	mutex_unlock(&ops->lock);

	op->flag = 1;
	wake_up_all(&op->waitq);

	if (!op->nwaiting)
		kfree(op);
}

void pnvl_op_wait(struct pnvl_op *op)
{
	if (!op || op->flag)
		return;

	op->nwaiting++;
	wait_event(op->waitq, op->flag == 1);
	if (!(--op->nwaiting))
		kfree(op);
}

int pnvl_op_setup(struct pnvl_dev *pnvl_dev, struct pnvl_op *op)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;
	struct pnvl_data data;

	switch(op->command) {
	case PNVL_IOCTL_SEND:
		if (copy_from_user(&data, (void *)op->uarg, sizeof(data)) > 0)
			return -EFAULT;
		dma->mode = PNVL_MODE_ACTIVE;
		dma->direction = DMA_TO_DEVICE;
		dma->addr = data.addr;
		dma->len = data.len;
		break;
	case PNVL_IOCTL_RECV:
		if (copy_from_user(&data, (void *)op->uarg, sizeof(data)) > 0)
			return -EFAULT;
		dma->mode = PNVL_MODE_PASSIVE;
		dma->direction = DMA_FROM_DEVICE;
		dma->addr = data.addr;
		dma->len = data.len;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void pnvl_op_next(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_ops *ops = &pnvl_dev->ops;
	struct pnvl_op *op;
	int rv;

	op = pnvl_op_first(ops); // current running op
	if (!op) {
		rv = 0;
		goto out;
	}

	pnvl_dma_unmap_pages(pnvl_dev);
	pnvl_dma_unpin_pages(pnvl_dev);
	pnvl_dev->dma.mode = PNVL_MODE_OFF;

	pnvl_op_step(ops, op);

	op = pnvl_op_first(ops); // next op to run
	if (!op) {
		rv = 0;
		goto out;
	}

	rv = pnvl_op_setup(pnvl_dev, op);
	if (rv < 0)
		goto out;

	rv = pnvl_ioctl_run(pnvl_dev, op->command);

out:
	return; /* rv could be useful for debug */
}

struct pnvl_op *pnvl_op_get(struct pnvl_ops *ops, pnvl_handle_t id)
{
	struct list_head *entry;
	struct pnvl_op *cur_op, *op = NULL;

	mutex_lock(&ops->lock);
	list_for_each(entry, &ops->queue) {
		cur_op = list_entry(entry, struct pnvl_op, list);
		if (cur_op->id == id) {
			op = cur_op;
			break;
		}
	}
	mutex_unlock(&ops->lock);

	return op;
}
