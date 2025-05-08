/* pnvl_module.h - Definitions for the kernel module
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#ifndef _PNVL_MODULE_H_
#define _PNVL_MODULE_H_

#include "hw/pnvl_hw.h"
#include "sw/module/pnvl_ioctl.h"
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

#define PNVL_MODE_ACTIVE 1
#define PNVL_MODE_PASSIVE 0
#define PNVL_MODE_OFF -1

//struct pnvl_dev; /* forward declaration */

struct pnvl_bar {
	u64 start;
	u64 end;
	u64 len;
	void __iomem *mmio;
};

struct pnvl_irq {
	void __iomem *mmio_ack_irq;
	int irq_num;
};

struct pnvl_dma {
	int mode;
	enum dma_data_direction direction;
	struct page **pages;
	struct sg_table sgt; /* dma page distribution */
	unsigned long npages;
	unsigned long nmapped;
	unsigned long addr;
	unsigned long len;
};

struct pnvl_ops {
	pnvl_handle_t next_id; // to identify an op
	spinlock_t lock; // to lock queue access
	struct list_head active;
	struct list_head inactive;
};

struct pnvl_dev {
	struct pci_dev *pdev;
	struct pnvl_bar bar;
	struct pnvl_irq irq;
	struct pnvl_dma dma;
	struct pnvl_ops ops;
	dev_t minor, major;
	struct cdev cdev;
};

struct pnvl_op {
	struct list_head list;
	wait_queue_head_t waitq;
	atomic_t nwaiting;
	int flag; // for the wait queue
	pnvl_handle_t id;
	long retval;
	long (*ioctl_fn)(struct pnvl_dev *);
	union {
		struct pnvl_data data;
	};
};

long pnvl_ioctl_send(struct pnvl_dev *pnvl_dev);
long pnvl_ioctl_recv(struct pnvl_dev *pnvl_dev);

int pnvl_dma_pin_pages(struct pnvl_dev *pnvl_dev);
void pnvl_dma_unpin_pages(struct pnvl_dev *pnvl_dev);
int pnvl_dma_map_pages(struct pnvl_dev *pnvl_dev);
void pnvl_dma_unmap_pages(struct pnvl_dev *pnvl_dev);
void pnvl_dma_write_setup(struct pnvl_dev *pnvl_dev, int mode, enum dma_data_direction dir);
void pnvl_dma_write_maps(struct pnvl_dev *pnvl_dev);
void pnvl_dma_doorbell_ring(struct pnvl_dev *pnvl_dev);

struct pnvl_op *pnvl_ops_new(unsigned int cmd, unsigned long uarg);
pnvl_handle_t pnvl_ops_init(struct pnvl_dev *pnvl_dev, struct pnvl_op *op);
struct pnvl_op *pnvl_ops_current(struct pnvl_ops *ops);
long pnvl_ops_wait(struct pnvl_op *op);
void pnvl_ops_next(struct pnvl_dev *pnvl_dev);
struct pnvl_op *pnvl_ops_get(struct pnvl_ops *ops, pnvl_handle_t id);
int pnvl_ops_flush(struct pnvl_ops *ops);

int pnvl_irq_enable(struct pnvl_dev *pnvl_dev);

#endif /* _PNVL_MODULE_H_ */
