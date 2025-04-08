/* pnvl_queue.c - pnvl ioctl operations queue management
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "pnvl_module.h"

int pnvl_op_add(struct pnvl_ops *ops, unsigned int cmd, unsigned long udata)
{

	struct pnvl_op *op = kmalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		return -ENOMEM;

	init_waitqueue_head(&op->waitq);
	op->flag = 0;
	op->command = cmd;
	op->data_ptr = (struct pnvl_data __user *)udata;

	mutex_lock(&ops->lock);
	list_add_tail(op, &ops->queue);
	mutex_unlock(&ops->lock);

	return 0;
}

struct pnvl_op *pnvl_op_first(struct pnvl_ops *ops)
{
	if (list_empty(&pnvl_ops_queue))
		return NULL;
	return list_entry(&pnvl_ops_queue.next, struct pnvl_op, list);
}

int pnvl_op_step(struct pnvl_ops *ops, struct pnvl_op *op)
{
	mutex_lock(&ops->lock);
	list_del(&op->list);	
	mutex_unlock(&ops->lock);

	free(op);

	return 0;
}
