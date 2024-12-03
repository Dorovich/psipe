/* dma.c - Direct Memory Access (DMA) operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "dma.h"

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
	src = pnvl_dma_addr_mask(src);

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
	dst = pnvl_dma_addr_mask(dst);
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
