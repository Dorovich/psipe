/* pnvl_queue.c - pnvl ioctl operations queue management
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "pnvl_module.h"

struct pnvl_op *pnvl_ops_new(unsigned int cmd, unsigned long uarg)
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
	long rv = -EINVAL;
	int empty;

	if (!op)
		goto out;

	spin_lock_irqsave(&ops->lock, flags);
	empty = list_empty(&ops->active); 
	list_add_tail(&op->list, &ops->active);
	op->id = ops->next_id++;
	spin_unlock_irqrestore(&ops->lock, flags);

	if (empty) {
		pnvl_dev->dma.addr = op->data.addr;
		pnvl_dev->dma.len = op->data.len;
		rv = op->ioctl_fn(pnvl_dev);
		//pr_info("pnvl_ops_init - set up id %lu\n", op->id);
	}

	//pr_info("pnvl_ops_init - id %lu active\n", op->id);

out:
	return rv < 0 ? rv : op->id;
}

struct pnvl_op *pnvl_ops_current(struct pnvl_ops *ops)
{
	/* ops->lock must be taken */
	return list_empty(&ops->active) ?
		NULL : list_first_entry(&ops->active, struct pnvl_op, list);
}

static void pnvl_ops_fini(struct pnvl_ops *ops, struct pnvl_op *op)
{
	/* ops->lock must be taken */
	list_move_tail(&op->list, &ops->inactive);

	op->flag = 1;
	wake_up_all(&op->waitq);

	//pr_info("pnvl_ops_fini - id %lu inactive\n", op->id);
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

	pnvl_dma_unmap_pages(pnvl_dev);
	pnvl_dma_unpin_pages(pnvl_dev);
	pnvl_ops_fini(ops, op);
	op = pnvl_ops_current(ops);
	if (!op)
		goto unlock;

	spin_unlock_irqrestore(&ops->lock, flags);
	pnvl_dev->dma.addr = op->data.addr;
	pnvl_dev->dma.len = op->data.len;
	op->retval = op->ioctl_fn(pnvl_dev);

	//pr_info("pnvl_ops_next - set up id %lu\n", op->id);

unlock:
	spin_unlock_irqrestore(&ops->lock, flags);
}

struct pnvl_op *pnvl_ops_get(struct pnvl_ops *ops, pnvl_handle_t id)
{
	unsigned long flags;
	struct list_head *entry;
	struct pnvl_op *cur_op, *op = NULL;

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

int pnvl_ops_clean(struct pnvl_ops *ops)
{
	unsigned long flags;
	struct list_head *entry, *tmp;

	spin_lock_irqsave(&ops->lock, flags);
	list_for_each_safe(entry, tmp, &ops->active) {
		list_del(entry);
		kfree(list_entry(entry, struct pnvl_op, list));
	}
	list_for_each_safe(entry, tmp, &ops->inactive) {
		list_del(entry);
		kfree(list_entry(entry, struct pnvl_op, list));
	}
	spin_unlock_irqrestore(&ops->lock, flags);

	return 0;
}
