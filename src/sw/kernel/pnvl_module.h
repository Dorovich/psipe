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
#include <linux/mutex.h>

#define PNVL_MODE_ACTIVE 1
#define PNVL_MODE_PASSIVE 0
#define PNVL_MODE_OFF -1

#define is_busy(pnvl_dev) (pnvl_dev->dma.mode != PNVL_MODE_OFF)

struct pnvl_op {
	struct list_head list;
	wait_queue_head_t waitq;
	int flag; // for the wait queue
	unsigned int command; // PNVL_IOCTL_SEND or PNVL_IOCTL_RECV (for now)
	struct pnvl_data __user *data_ptr;
};

struct pnvl_dev; /* forward declaration */

struct pnvl_bar {
	u64 start;
	u64 end;
	u64 len;
	void __iomem *mmio;
};

struct pnvl_dma {
	int mode;
	enum dma_data_direction direction;
	struct page **pages;
	struct sg_table *sgt; /* dma page distribution */
	unsigned long npages;
	unsigned long nmapped;
	unsigned long addr;
	unsigned long len;
};

struct pnvl_irq {
	void __iomem *mmio_ack_irq;
	int irq_num;
};

struct pnvl_ops {
	struct list_head queue;
	struct mutex lock;
}

struct pnvl_dev {
	struct pci_dev *pdev;
	struct pnvl_bar bar;
	struct pnvl_irq irq;
	struct pnvl_dma dma;
	struct pnvl_ops ops;
	dev_t minor, major;
	struct cdev cdev;
};

int pnvl_dma_pin_pages(struct pnvl_dev *pnvl_dev);
void pnvl_dma_unpin_pages(struct pnvl_dev *pnvl_dev);
int pnvl_dma_map_pages(struct pnvl_dev *pnvl_dev);
void pnvl_dma_unmap_pages(struct pnvl_dev *pnvl_dev);
void pnvl_dma_write_config(struct pnvl_dev *pnvl_dev);
void pnvl_dma_doorbell_ring(struct pnvl_dev *pnvl_dev);
void pnvl_dma_wait(struct pnvl_dev *pnvl_dev);
void pnvl_dma_wake(struct pnvl_dev *pnvl_dev);

int pnvl_queue_new(unsigned int cmd, struct pnvl_data __user *data);
struct pnvl_op *pnvl_queue_first(void);
int pnvl_queue_step(struct pnvl_op *op);

int pnvl_irq_enable(struct pnvl_dev *pnvl_dev);

#endif /* _PNVL_MODULE_H_ */
