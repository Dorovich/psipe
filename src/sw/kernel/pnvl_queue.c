/* pnvl_queue.c - pnvl ioctl operations queue management
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "pnvl_module.h"

int pnvl_op_add(struct pnvl_ops *ops, unsigned int cmd, unsigned long uarg)
{

	struct pnvl_op *op = kmalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		return -ENOMEM;

	init_waitqueue_head(&op->waitq);
	op->flag = 0;
	op->command = cmd;
	op->uarg = uarg;

	mutex_lock(&ops->lock);
	list_add_tail(op, &ops->queue);
	mutex_unlock(&ops->lock);

	return 0;
}

struct pnvl_op *pnvl_op_first(struct pnvl_ops *ops)
{
	struct pnvl_op *op = NULL;

	mutex_lock(&ops->lock);
	if (!list_empty(&ops->queue))
		op = list_entry(&ops->queue.next, struct pnvl_op, list);
	mutex_unlock(&ops->lock);

	return op;
}

int pnvl_op_step(struct pnvl_ops *ops, struct pnvl_op *op)
{
	mutex_lock(&ops->lock);
	list_del(&op->list);	
	op->flag = 1;
	wake_up_all(&op->waitq);
	mutex_unlock(&ops->lock);

	free(op);

	return 0;
}

void pnvl_op_wait(struct pnvl_op *op)
{
	if (!op->flag)
		wait_event(op->waitq, op->flag == 1);
}

int pnvl_op_setup(struct pnvl_dev *pnvl_dev, struct pnvl_op *op)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;
	struct pnvl_data data;

	switch(op->cmd) {
	case PNVL_IOCTL_SEND:
		if (copy_from_user(&data, (struct pnvl_data __user *)uarg,
					sizeof(data)) > 0)
			return -EFAULT;
		dma->mode = PNVL_MODE_ACTIVE;
		dma->direction = DMA_FROM_DEVICE;
		dma->addr = data.addr;
		dma->len = data.len;
		break;
	case PNVL_IOCTL_RECV:
		if (copy_from_user(&data, (struct pnvl_data __user *)uarg,
					sizeof(data)) > 0)
			return -EFAULT;
		dma->mode = PNVL_MODE_PASSIVE;
		dma->direction = DMA_TO_DEVICE;
		dma->addr = data.addr;
		dma->len = data.len;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
