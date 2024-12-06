/* pnvl_module.h - Definitions for the kernel module
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#pragma once

#include "hw/pnvl_hw.h"
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/wait.h>

#define PNVL_MODE_WORK 0x01
#define PNVL_MODE_WAIT 0x02

/* forward declaration */
struct pnvl_dev;

struct pnvl_bar {
	u64 start;
	u64 end;
	u64 len;
	void __iomem *mmio;
};

struct pnvl_dma {
	dma_addr_t *dma_handles;
	size_t offset;
	size_t len;
	size_t npages;
	enum dma_data_direction direction;
	struct page **pages;
};

struct pnvl_irq {
	void __iomem *mmio_ack_irq;
	int irq_num;
};

struct pnvl_dev {
	struct pci_dev *pdev;
	struct pnvl_bar bar;
	struct pnvl_irq irq;
	struct pnvl_dma dma;
	struct pnvl_data data;
	dev_t minor;
	dev_t major;
	struct cdev cdev;
	atomic_t wq_flag;
	wait_queue_head_t wq;
};

/* ============================================================================
 * Public
 * ============================================================================
 */

int pnvl_dma_work(struct pnvl_dev *pnvl_dev);
int pnvl_dma_wait(struct pnvl_dev *pnvl_dev);

int pnvl_irq_enable(struct pnvl_dev *pnvl_dev);
