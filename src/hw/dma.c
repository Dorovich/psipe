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
 */
int pnvl_dma_rx_page(PNVLDevice *dev, dma_addr_t src, size_t *len)
{
	int err;
	size_t len, pgsize;
	unsigned long mask;

	pgsize = qemu_target_page_size();
	mask = pgsize-1;
	src = pnvl_dma_mask(src);

	if ((src & mask) + len > pgsize)
		len = pgsize

	err = pci_dma_read(&dev->pci_dev, src, dev->dma.buff, len);
	if (err)
		return PNVL_FAILURE;

	return PNVL_SUCCESS;
}

/*
 * Transmit page: DMA buffer --> RAM
 */
int pnvl_dma_tx_page(PNVLDevice *dev, dma_addr_t dst, size_t ofs)
{
	int err;
	size_t pgsize;

	pgsize = qemu_target_page_size();
	dst = pnvl_dma_mask(dst);
	err = pci_dma_write(&dev->pci_dev, dst, dev->dma.buff, len);
	if (err)
		return PNVL_FAILURE;

	return PNVL_SUCCESS;
}

void pnvl_dma_add_handle(PNVLDevice *dev, dma_addr_t handle)
{
	DMAEngine *dma = &dev->dma;
	dma->handles[dma->config.npages++] = handle;
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
