/* pnvl_irq.c - pnvl virtual device DMA operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "hw/pnvl_hw.h"
#include "pnvl_module.h"
#include <linux/dma-mapping.h>

int pnvl_dma_pin_pages(struct pnvl_dma *dma)
{
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

	/* BEGIN VMA CHECK */
	struct vm_area_struct *vma;

	down_read(&current->mm->mmap_lock);
	vma = find_vma(current->mm, dma->addr);
	up_read(&current->mm->mmap_lock);

	if (!vma || dma->addr < vma->vm_start) {
		pr_err("Address 0x%lx not mapped in process.\n", dma->addr);
		return -EFAULT;
	} else if (!(vma->vm_flags & VM_WRITE)) {
		pr_err("Address 0x%lx is not writable", dma->addr);
		return -EFAULT;
	}
	/* END VMA CHECK */

	pinned = pin_user_pages_fast(dma->addr, npages,
			FOLL_LONGTERM | FOLL_WRITE, dma->pages);
	if (pinned == -EFAULT || pinned == -EAGAIN) { /* we can retry */
		//pr_info("pin_user_pages - recoverable error, retrying\n");
		down_read(&current->mm->mmap_lock);
		pinned = pin_user_pages(dma->addr, npages,
				FOLL_LONGTERM | FOLL_WRITE, dma->pages);
		up_read(&current->mm->mmap_lock);
	}

	if (pinned < 0) {
		//pr_info("pin_user_pages - error\n");
		rv = pinned;
		goto free_pages;
	} else if (pinned != npages) {
		//pr_info("pin_user_pages - too short (%d/%d)\n", pinned, npages);
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

int pnvl_dma_map_pages(struct pnvl_dma *dma, struct pci_dev *pdev)
{
	dma->nmapped = dma_map_sg(&pdev->dev, dma->sgt.sgl, dma->sgt.nents,
			dma->direction);

	return (int)dma->nmapped;
}

void pnvl_dma_write_setup(struct pnvl_dma *dma, struct pnvl_bar *bar, int mode,
		enum dma_data_direction dir)
{
	dma->mode = mode;
	dma->direction = dir;
	iowrite32((u32)dma->mode, bar->mmio + PNVL_HW_BAR0_DMA_CFG_MOD);
}

void pnvl_dma_write_maps(struct pnvl_dma *dma, struct pnvl_bar *bar)
{
	dma_addr_t handle;
	struct scatterlist *sg;
	unsigned ofs = 0;
	int i;

	iowrite32((u32)dma->len, bar->mmio + PNVL_HW_BAR0_DMA_CFG_LEN);
	iowrite32((u32)dma->nmapped, bar->mmio + PNVL_HW_BAR0_DMA_CFG_PGS);

	for_each_sg(dma->sgt.sgl, sg, dma->nmapped, i) {
		handle = sg_dma_address(sg);
		iowrite32((u32)handle,
				bar->mmio + PNVL_HW_BAR0_DMA_HANDLES + ofs);
		ofs += sizeof(u32);
	}
}

void pnvl_dma_doorbell_ring(struct pnvl_bar *bar)
{
	iowrite32(1, bar->mmio + PNVL_HW_BAR0_DMA_DOORBELL_RING);
}

void pnvl_dma_unmap_pages(struct pnvl_dma *dma, struct pci_dev *pdev)
{
	dma_unmap_sg(&pdev->dev, dma->sgt.sgl, dma->sgt.nents, dma->direction);
}

void pnvl_dma_unpin_pages(struct pnvl_dma *dma)
{
	unpin_user_pages(dma->pages, dma->npages);
	kfree(dma->pages);
}
