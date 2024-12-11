/* dma.c - Direct Memory Access (DMA) operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "dma.h"
#include "qemu/osdep.h"
#include "qemu/log.h"

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
 *   pos: inout
 * Return:
 */
size_t pnvl_dma_rx_page(PNVLDevice *dev, dma_addr_t addr, int *pos)
{
	int err;
	size_t ofs, len, len_max;
	unsigned long mask;
	const size_t pgsize = qemu_target_page_size();
	const size_t len_total = dev->dma.config.len;

	mask = pnvl_dma_mask(pgsize-1);
	ofs = addr & mask;

	len_max = len_total > pgsize ? pgsize : len_total;
	len = len_max - ofs;
	err = pci_dma_read(&dev->pci_dev, addr, dev->dma.buff, len);
	if (err)
		return PNVL_FAILURE;

	if (!ofs)
		return len;

	*pos++;
	addr = dev->dma.handles[*pos];
	err = pci_dma_read(&dev->pci_dev, addr, dev->dma.buff + len, ofs);
	if (err)
		return PNVL_FAILURE;

	return len_max; // len + ofs
}

/*
 * Transmit page: DMA buffer --> RAM
 */
/* TODO: fix. see pnvl_dma_rx_page */
int pnvl_dma_tx_page(PNVLDevice *dev, dma_addr_t addr, size_t len)
{
	int err;
	const size_t pgsize = qemu_target_page_size();

	if (len <= 0)
		return PNVL_FAILURE;

	addr = pnvl_dma_mask(addr);
	err = pci_dma_write(&dev->pci_dev, addr, dev->dma.buff, len);
	if (err)
		return PNVL_FAILURE;

	return PNVL_SUCCESS;
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

dma_addr_t pnvl_dma_next_page(PNVLDevice *dev, int pos, size_t len_sent)
{
	
}

void pnvl_dma_reset(PNVLDevice *dev)
{
	DMAEngine *dma = &dev->dma;
	dma->status = DMA_STATUS_IDLE;
	dma->config.npages = 0;
	dma->config.len = 0;
	dma->config.mode = 0;
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
