/* dma.c - Direct Memory Access (DMA) operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "qemu/osdep.h"
#include "exec/target_page.h"
#include "qemu/log.h"
#include "pnvl.h"
#include "dma.h"

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
size_t pnvl_dma_rx_page(PNVLDevice *dev)
{
	int err;
	size_t ofs, max_page, len = 0;
	unsigned long mask;
	DMAEngine *dma = &dev->dma;
	dma_addr_t addr = dma->current.addr;
	size_t len_left = dma->current.len_left;

	mask = pnvl_dma_mask(dev, dma->config.page_size - 1);
	ofs = addr & mask;
	max_page = dma->config.page_size - ofs;

	len = MIN(len_left, max_page);
	printf("DMA_RX: len=%lu, ofs=%lu, max=%lu\n", len, ofs, max_page);
	err = pci_dma_read(&dev->pci_dev, addr, dma->buff, len);
	if (err)
		return PNVL_FAILURE;
	addr += len;
	len_left -= len;

	if (!len_left)
		goto dma_rx_end;

	addr = dma->config.handles[++dma->current.hnd_pos];
	ofs = dma->config.page_size - len;
	printf("DMA_RX: len=%lu, ofs=%lu, max=%lu\n", len, ofs, max_page);
	err = pci_dma_read(&dev->pci_dev, addr, dma->buff + len, ofs);
	if (err)
		return PNVL_FAILURE;
	addr += ofs;
	len_left -= ofs;

dma_rx_end:
	dma->current.addr = addr;
	dma->current.len_left = len_left;
	printf("DMA_RX: len=%lu, len_left=%lu\n", len, len_left);
	return len;
}

/*
 * Transmit page: DMA buffer --> RAM
 */
int pnvl_dma_tx_page(PNVLDevice *dev, size_t len_in)
{
	int err;
	size_t ofs, max_page, len = 0;
	unsigned long mask;
	DMAEngine *dma = &dev->dma;
	dma_addr_t addr = dma->current.addr;
	size_t len_left = dma->current.len_left;

	mask = pnvl_dma_mask(dev, dma->config.page_size - 1);
	ofs = addr & mask;
	max_page = dma->config.page_size - ofs;

	len = MIN(len_in, max_page);
	printf("DMA_TX: len=%lu, ofs=%lu, max=%lu\n", len, ofs, max_page);
	err = pci_dma_write(&dev->pci_dev, addr, dma->buff, len);
	if (err)
		return PNVL_FAILURE;
	addr += len;
	len_left -= len;

	if (len == len_in)
		goto dma_tx_end;

	addr = dma->config.handles[++dma->current.hnd_pos];
	ofs = len_in - len; // because max_page < len_in
	printf("DMA_TX: len=%lu, ofs=%lu, max=%lu\n", len, ofs, max_page);
	err = pci_dma_write(&dev->pci_dev, addr, dma->buff + len, ofs);
	if (err)
		return PNVL_FAILURE;
	addr += ofs;
	len_left -= ofs;

dma_tx_end:
	dma->current.addr = addr;
	dma->current.len_left = len_left;
	printf("DMA_TX: len=%lu\n", len);
	return PNVL_SUCCESS;
}

void pnvl_dma_init_current(PNVLDevice *dev)
{
	DMAEngine *dma = &dev->dma;

	dma->current.len_left = dma->config.len;
	dma->current.addr = dma->config.handles[0];
	dma->current.hnd_pos = 0;
}

void pnvl_dma_add_handle(PNVLDevice *dev, dma_addr_t handle)
{
	DMAEngine *dma = &dev->dma;
	dma_addr_t new_hnd = pnvl_dma_mask(dev, handle);
	dma->config.handles[dma->config.npages++] = new_hnd;
}

bool pnvl_dma_is_idle(PNVLDevice *dev)
{
	return (qatomic_read(&dev->dma.status) == DMA_STATUS_IDLE);
}

bool pnvl_dma_is_finished(PNVLDevice *dev)
{
	return dev->dma.current.len_left == 0;
}

void pnvl_dma_reset(PNVLDevice *dev)
{
	DMAEngine *dma = &dev->dma;
	dev->dma.ret = false;
	dma->status = DMA_STATUS_IDLE;
	dma->config.npages = 0;
	dma->config.len = 0;
	dma->config.page_size = qemu_target_page_size();
	memset(dma->config.handles, 0,
			sizeof(dma_addr_t) * PNVL_HW_BAR0_DMA_HANDLES_CNT);
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
