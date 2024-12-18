/* dma.c - Direct Memory Access (DMA) operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "dma.h"
#include "qemu/log.h"
#include "qemu/osdep.h"
#include <sys/param.h>

/* ============================================================================
 * Private
 * ============================================================================
 */

static inline dma_addr_t pnvl_dma_mask(PNVLDevice *dev, dma_addr_t addr)
{
	dma_addr_t masked_addr = addr & dev->dma.config.mask;
	if (masked_addr != addr) {
		qemu_log_mask(LOG_GUEST_ERROR,
			"masked_addr (%" PRIx64 ") != addr (%" PRIx64 ")",
			masked_addr, addr);
	}
	return masked_addr;
}

static inline bool pnvl_dma_inside_dev_boundaries(dma_addr_t addr)
{
	return (PNVL_HW_DMA_AREA_START <= addr &&
		addr <= PNVL_HW_DMA_AREA_START + PNVL_HW_DMA_AREA_SIZE);
}

/* ============================================================================
 * Public
 * ============================================================================
 */

/*
 * Receive page: DMA buffer <-- RAM
 */
int pnvl_dma_rx_page(PNVLDevice *dev)
{
	int err;
	size_t ofs, len, len_max;
	unsigned long mask;
	DMAEngine *dma = &dev->dma;

	mask = pnvl_dma_mask(dma->config.page_size - 1);
	ofs = dma->current.addr & mask;

	len_max = MIN(dma->current.len_left, dma->config.page_size);
	len = len_max - ofs;
	err = pci_dma_read(&dev->pci_dev, dma->current.addr, dma->buff, len);
	if (err)
		return PNVL_FAILURE;
	dma->current.len_total -= len;

	if (!ofs)
		return len;

	dma->current.hnd_pos++;
	dma->current.addr = dma->config.handles[dma->current.hnd_pos];
	err = pci_dma_read(&dev->pci_dev, dma->current.addr,
			dma->buff + len, ofs);
	if (err)
		return PNVL_FAILURE;
	dma->current.addr += ofs;
	dma->current.len_total -= ofs;

	return len_max;
}

/*
 * Transmit page: DMA buffer --> RAM
 */
int pnvl_dma_tx_page(PNVLDevice *dev, size_t len_in)
{
	int err;
	size_t ofs, len_max;
	unsigned long mask;
	DMAEngine *dma = &dev->dma;

	if (len <= 0)
		return PNVL_FAILURE;

	mask = pnvl_dma_mask(dma->config.page_size - 1);
	ofs = dma->current.addr & mask;

	len_max = MIN(dma->current.len_left, dma->config.page_size);
	len = MIN(len_in, len_max) - ofs;
	err = pci_dma_write(&dev->pci_dev, dma->current.addr, dma->buff, len);
	if (err)
		return PNVL_FAILURE;
	dma->current.len_total -= len;

	if (!ofs)
		return PNVL_SUCCESS;

	dma->current.hnd_pos++;
	dma->current.addr = dma->config.handles[dma->current.hnd_pos];
	err = pci_dma_read(&dev->pci_dev, dma->current.addr,
			dma->buff + len, ofs);
	if (err)
		return PNVL_FAILURE;
	dma->current.addr += ofs;
	dma->current.len_total -= ofs;

	return PNVL_SUCCESS;
}

void pnvl_dma_init_current(PNVLDevice *dev)
{
	dev->dma.current.len_left = dev->dma.config.len;
	dev->dma.current.addr = NULL;
	dev->dma.current.hnd_pos = 0;
}

void pnvl_dma_add_handle(PNVLDevice *dev, dma_addr_t handle)
{
	DMAEngine *dma = &dev->dma;
	dma->handles[dma->config.npages++] = pnvl_dma_mask(handle);
}

bool pnvl_dma_is_idle(PNVLDevice *dev)
{
	return (qatomic_read(&dev->dma.status) == DMA_IDLE);
}

void pnvl_dma_reset(PNVLDevice *dev)
{
	DMAEngine *dma = &dev->dma;
	dma->status = DMA_STATUS_IDLE;
	dma->config.npages = 0;
	dma->config.len = 0;
	dma->config.page_size = qemu_target_page_size();
	memset(dma->config.handles, 0, PNVL_HW_BAR0_DMA_WORK_AREA_SIZE);
	memset(dma->buff, 0, PNVL_HW_DMA_AREA_SIZE);
}

void pnvl_dma_init(PNVLDevice *dev, Error **errp)
{
	pnvl_dma_reset(dev);
	dev->dma.config.mask = DMA_BIT_MASK(PNVL_HW_DMA_ADDR_CAPABILITY);
}

void pnvl_dma_fini(PNVLDevice *dev)
{
	pnvl_dma_reset(dev);
	dev->dma.status = DMA_STATUS_OFF;
}
