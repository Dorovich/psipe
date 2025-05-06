/* pnvl_irq.c - pnvl virtual device DMA operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "hw/pnvl_hw.h"
#include "pnvl_module.h"
#include <linux/dma-mapping.h>

int pnvl_dma_pin_pages(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;
	int first_page, last_page, npages, pinned, rv;
	unsigned ofs;

	ofs = dma->addr & ~PAGE_MASK;
	first_page = (dma->addr & PAGE_MASK) >> PAGE_SHIFT;
	last_page = ((dma->addr + dma->len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	npages = last_page - first_page + 1;
	dma->npages = npages;

	if (npages <= 0 || npages > PNVL_HW_BAR0_DMA_HANDLES_CNT)
		return -EMSGSIZE;

	dma->pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!dma->pages)
		return -ENOMEM;

	pinned = pin_user_pages_fast(dma->addr, npages,
			FOLL_LONGTERM | FOLL_WRITE, dma->pages);
	if (pinned < 0) {
		rv = pinned;
		goto free_pages;
	} else if (pinned != npages) {
		rv = -EFAULT;
		goto unpin_pages;
	}

	rv = sg_alloc_table_from_pages_segment(&dma->sgt, dma->pages, npages,
			ofs, dma->len, PAGE_SIZE, GFP_KERNEL);
	if (rv < 0)
		goto free_table;

	return 0;

free_table:
	sg_free_table(&dma->sgt);
unpin_pages:
	unpin_user_pages(dma->pages, pinned);
free_pages:
	kfree(dma->pages);
	return rv;
}

int pnvl_dma_map_pages(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;

	dma->nmapped = dma_map_sg(&pnvl_dev->pdev->dev, dma->sgt.sgl,
			dma->sgt.nents, dma->direction);

	//pr_info("dma_map_sg: using %lu mappings\n", dma->nmapped);

	return (int)dma->nmapped;
}

void pnvl_dma_write_setup(struct pnvl_dev *pnvl_dev, int mode,
		enum dma_data_direction dir)
{
	void __iomem *mmio = pnvl_dev->bar.mmio;
	struct pnvl_dma *dma = &pnvl_dev->dma;
	dma->mode = mode;
	dma->direction = dir;
	iowrite32((u32)dma->mode, mmio + PNVL_HW_BAR0_DMA_CFG_MOD);
}

void pnvl_dma_write_maps(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;
	void __iomem *mmio = pnvl_dev->bar.mmio;
	dma_addr_t handle;
	struct scatterlist *sg;
	unsigned ofs = 0;
	int i;

	iowrite32((u32)dma->len, mmio + PNVL_HW_BAR0_DMA_CFG_LEN);
	iowrite32((u32)dma->nmapped, mmio + PNVL_HW_BAR0_DMA_CFG_PGS);

	for_each_sg(dma->sgt.sgl, sg, dma->nmapped, i) {
		handle = sg_dma_address(sg);
		iowrite32((u32)handle, mmio + PNVL_HW_BAR0_DMA_HANDLES + ofs);
		ofs += sizeof(u32);
	}
}

void pnvl_dma_doorbell_ring(struct pnvl_dev *pnvl_dev)
{
	void __iomem *mmio = pnvl_dev->bar.mmio;
	iowrite32(1, mmio + PNVL_HW_BAR0_DMA_DOORBELL_RING);
}

void pnvl_dma_unmap_pages(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;

	dma_unmap_sg(&pnvl_dev->pdev->dev, dma->sgt.sgl, dma->sgt.nents,
			dma->direction);
}

void pnvl_dma_unpin_pages(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;

	unpin_user_pages(dma->pages, dma->npages);
	kfree(dma->pages);
}
