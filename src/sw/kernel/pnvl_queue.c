/* pnvl_queue.c - pnvl ioctl operations queue management
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "pnvl_module.h"

struct pnvl_op *pnvl_op_new(unsigned int cmd, unsigned long uarg)
{
	int rv;

	struct pnvl_op *op = kmalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		goto out;

	switch(cmd) {
	case PNVL_IOCTL_SEND:
		rv = copy_from_user(&op->data, (void *)uarg, sizeof(op->data));
		if (rv < 0)
			goto clean;
		op->ioctl_fn = pnvl_ioctl_send;
		break;
	case PNVL_IOCTL_RECV:
		rv = copy_from_user(&op->data, (void *)uarg, sizeof(op->data));
		if (rv < 0)
			goto clean;
		op->ioctl_fn = pnvl_ioctl_recv;
		break;
	default:
		goto clean;
	}

	init_waitqueue_head(&op->waitq);
	spin_lock_init(&op->lock);
	op->nwaiting = 0;
	op->flag = 0;

out:
	return op;

clean:
	kfree(op);
	return NULL;
}

pnvl_handle_t pnvl_op_add(struct pnvl_ops *ops, struct pnvl_op *op)
{
	unsigned long flags;

	if (!op)
		return -EINVAL;

	spin_lock_irqsave(&ops->lock, flags);
	list_add_tail(&op->list, &ops->queue);
	spin_unlock_irqrestore(&ops->lock, flags);

	op->id = ops->next_id++;

	//printk(KERN_INFO "operation added to queue (id=%lu)\n", op->id);

	return op->id;
}

struct pnvl_op *pnvl_op_first(struct pnvl_ops *ops)
{
	unsigned long flags;
	struct pnvl_op *op = NULL;

	spin_lock_irqsave(&ops->lock, flags);
	if (!list_empty(&ops->queue))
		op = list_first_entry(&ops->queue, struct pnvl_op, list);
	spin_unlock_irqrestore(&ops->lock, flags);

	return op;
}

static void pnvl_op_step(struct pnvl_ops *ops, struct pnvl_op *op)
{
	unsigned long flags;

	spin_lock_irqsave(&ops->lock, flags);
	list_del(&op->list);	
	spin_unlock_irqrestore(&ops->lock, flags);

	op->flag = 1;
	wake_up_all(&op->waitq);

	spin_lock_irqsave(&op->lock, flags);
	if (!op->nwaiting)
		kfree(op);
	spin_unlock_irqrestore(&op->lock, flags);
}

void pnvl_op_wait(struct pnvl_op *op)
{
	unsigned long flags;

	if (!op || op->flag) /* op not found or has finished */
		return;

	spin_lock_irqsave(&op->lock, flags);
	op->nwaiting++;
	spin_unlock_irqrestore(&op->lock, flags);

	wait_event(op->waitq, op->flag == 1);

	spin_lock_irqsave(&op->lock, flags);
	if (!(--op->nwaiting))
		kfree(op);
	spin_unlock_irqrestore(&op->lock, flags);
}

void pnvl_op_next(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_ops *ops = &pnvl_dev->ops;
	struct pnvl_op *op;
	int rv;

	op = pnvl_op_first(ops); /* current running op */
	if (!op) {
		rv = 0;
		goto out;
	}

	pnvl_dma_unmap_pages(pnvl_dev);
	pnvl_dma_unpin_pages(pnvl_dev);
	pnvl_dev->dma.mode = PNVL_MODE_OFF;
	pnvl_op_step(ops, op);

	op = pnvl_op_first(ops); /* next op to run */
	if (!op) {
		rv = 0;
		goto out;
	}

	pnvl_dev->dma.addr = op->data.addr;
	pnvl_dev->dma.len = op->data.len;
	rv = op->ioctl_fn(pnvl_dev);
	/* rv could be useful for debug */
out:
	return;
}

struct pnvl_op *pnvl_op_get(struct pnvl_ops *ops, pnvl_handle_t id)
{
	unsigned long flags;
	struct list_head *entry;
	struct pnvl_op *cur_op, *op = NULL;

	spin_lock_irqsave(&ops->lock, flags);
	list_for_each(entry, &ops->queue) {
		cur_op = list_entry(entry, struct pnvl_op, list);
		if (cur_op->id == id) {
			op = cur_op;
			break;
		}
	}
	spin_unlock_irqrestore(&ops->lock, flags);

	return op;
}
