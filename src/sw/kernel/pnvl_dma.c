/* pnvl_irq.c - pnvl virtual device DMA operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "hw/pnvl_hw.h"
#include "pnvl_module.h"
#include <linux/dma-mapping.h>

static void pnvl_dma_struct_init(struct pnvl_dma *dma, size_t ofs, size_t len,
	enum dma_data_direction dir)
{
	dma->offset = ofs;
	dma->len = len;
	dma->direction = dir;
}

int pnvl_dma_setup_handles(struct pnvl_dev *pnvl_dev)
{
	struct pci_dev *pdev;
	struct pnvl_dma *dma;
	size_t len, len_map;

	pdev = &pnvl_dev->pdev;
	dma = &pnvl_dev->dma;

	dma->dma_handles = kmalloc(dma->npages * sizeof(dma_addr_t),
				GPF_KERNEL);

	len = dma->len;
	len_map = PAGE_SIZE - dma->offset;
	if (len_map > len)
		len_map = len;
	len -= len_map;
	dma->dma_handles[0] = dma_map_page(pdev, pages[0], dma->offset, len_map,
					dma->direction);
	if (dma_mapping_error(pdev, dma->dma_handle[0]))
		return -ENOMEM;

	for (int i = 1; i < dma->npages; ++i) {
		len_map = len > PAGE_SIZE ?  PAGE_SIZE : len;
		len -= len_map;
		dma->dma_handles[i] = dma_map_page(pdev, pages[i], dma->offset,
						len_map, dma->direction);
		if (dma_mapping_error(pdev, dma->dma_handle[i]))
			return -ENOMEM;
	}

	return 0;
}

void pnvl_dma_setup_conclude(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma;
	void __iomem *mmio;
	size_t ofs;

	mmio = pnvl_dev->bar.mmio;
	dma = &pnvl_dev->dma;

	iowrite32(dma->len, mmio + PNVL_HW_BAR0_DMA_CFG_LEN);
	iowrite32(dma->npages, mmio + PNVL_HW_BAR0_DMA_CFG_PGS);
	iowrite32(dma->offset, mmio + PNVL_HW_BAR0_DMA_CFG_OFS);

	ofs = 0;
	for (int i = 0; i < dma->npages; ++i) {
		iowrite32((u32)dma->handle[i],
			mmio + PNVL_HW_BAR0_DMA_HANDLES + ofs);
		ofs += sizeof(u32);
	}

	iowrite32(1, mmio + PNVL_HW_BAR0_DMA_DOORBELL_RING);
}

int pnvl_dma_setup(struct pnvl_dev *pnvl_dev, int mode)
{
	int ret;

	switch(mode) {
	case PNVL_MODE_WORK:
		pnvl_dev->dma.direction = DMA_TO_DEVICE;
		break;
	case PNVL_MODE_WATCH:
		pnvl_dev->dma.direction = DMA_FROM_DEVICE;
		break;
	default:
		return -EINVAL;
	}

	ret = pnvl_dma_setup_handles(pnvl_dev);
	if (ret < 0)
		return ret;

	pnvl_dma_setup_conclude(pnvl_dev);

	return 0;
}

void pnvl_dma_wait(struct pnvl_dev *pnvl_dev)
{
	if (READ_ONCE(&pnvl_dev->wq_flag) == 1)
		return;

	wait_event(&pnvl_dev->wq, pnvl_dev->wq_flag == 1);
	WRITE_ONCE(&pnvl_dev->wq_flag, 0);
}
