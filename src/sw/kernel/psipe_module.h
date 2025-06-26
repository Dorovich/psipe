/* psipe_module.h - Definitions for the kernel module
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#ifndef _PSIPE_MODULE_H_
#define _PSIPE_MODULE_H_

#define DEBUG_CHECK_VMA 0

#include "hw/psipe_hw.h"
#include "sw/module/psipe_ioctl.h"
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

#define PSIPE_MODE_ACTIVE 1
#define PSIPE_MODE_PASSIVE 0
#define PSIPE_MODE_OFF -1

struct psipe_bar {
	u64 start;
	u64 end;
	u64 len;
	void __iomem *mmio;
};

struct psipe_irq {
	void __iomem *mmio_ack_irq;
	void __iomem *mmio_check_irq;
	int irq_num;
	spinlock_t lock;
};

struct psipe_dma {
	int mode;
	enum dma_data_direction direction;
	struct page **pages;
	struct sg_table sgt; /* dma page distribution */
	unsigned long npages;
	unsigned long nmapped;
	unsigned long addr;
	unsigned long len;
};

struct psipe_ops {
	psipe_handle_t next_id; // to identify an op
	spinlock_t lock; // to lock queue access
	struct list_head active;
	struct list_head inactive;
};

struct psipe_dev {
	struct pci_dev *pdev;
	struct psipe_bar bar;
	struct psipe_irq irq;
	struct psipe_ops ops;
	dev_t minor, major;
	struct cdev cdev;
};

struct psipe_op {
	struct list_head list;
	wait_queue_head_t waitq;
	atomic_t nwaiting;
	int flag; // for the wait queue
	psipe_handle_t id;
	long retval;
	long (*ioctl_fn)(struct psipe_dev *, struct psipe_dma *);
	struct psipe_dma dma;
};

long psipe_ioctl_send(struct psipe_dev *psipe_dev, struct psipe_dma *dma);
long psipe_ioctl_recv(struct psipe_dev *psipe_dev, struct psipe_dma *dma);

int psipe_dma_pin_pages(struct psipe_dma *dma);
void psipe_dma_unpin_pages(struct psipe_dma *dma);
int psipe_dma_map_pages(struct psipe_dma *dma, struct pci_dev *pdev);
void psipe_dma_unmap_pages(struct psipe_dma *dma, struct pci_dev *pdev);
void psipe_dma_write_setup(struct psipe_dma *dma, struct psipe_bar *bar, int mode, enum dma_data_direction dir);
void psipe_dma_write_maps(struct psipe_dma *dma, struct psipe_bar *bar);
void psipe_dma_doorbell_ring(struct psipe_bar *bar);

struct psipe_op *psipe_ops_new(unsigned int cmd, unsigned long uarg);
psipe_handle_t psipe_ops_init(struct psipe_dev *psipe_dev, struct psipe_op *op);
struct psipe_op *psipe_ops_current(struct psipe_ops *ops);
long psipe_ops_wait(struct psipe_op *op);
void psipe_ops_next(struct psipe_dev *psipe_dev);
struct psipe_op *psipe_ops_get(struct psipe_ops *ops, psipe_handle_t id);
int psipe_ops_flush(struct psipe_dev *psipe_dev);

int psipe_irq_enable(struct psipe_dev *psipe_dev);

#endif /* _PSIPE_MODULE_H_ */
