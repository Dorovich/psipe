/* psipe_irq.c - psipe virtual device DMA operations
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "hw/psipe_hw.h"
#include "psipe_module.h"
#include <linux/dma-mapping.h>

int psipe_dma_pin_pages(struct psipe_dma *dma)
{
	int first_page, last_page, npages, pinned, rv;
	unsigned ofs;

	ofs = dma->addr & ~PAGE_MASK;
	first_page = (dma->addr & PAGE_MASK) >> PAGE_SHIFT;
	last_page = ((dma->addr + dma->len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	npages = last_page - first_page + 1;
	dma->npages = npages;

	if (npages <= 0 || npages > PSIPE_HW_BAR0_DMA_HANDLES_CNT)
		return -EMSGSIZE;

	dma->pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!dma->pages)
		return -ENOMEM;

#if DEBUG_CHECK_VMA
#pragma message("Compiling with VMA checks.")
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
#endif /* DEBUG_CHECK_VMA */

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

int psipe_dma_map_pages(struct psipe_dma *dma, struct pci_dev *pdev)
{
	dma->nmapped = dma_map_sg(&pdev->dev, dma->sgt.sgl, dma->sgt.nents,
			dma->direction);

	return (int)dma->nmapped;
}

void psipe_dma_write_setup(struct psipe_dma *dma, struct psipe_bar *bar, int mode,
		enum dma_data_direction dir)
{
	dma->mode = mode;
	dma->direction = dir;
	iowrite32((u32)dma->mode, bar->mmio + PSIPE_HW_BAR0_DMA_CFG_MOD);
}

void psipe_dma_write_maps(struct psipe_dma *dma, struct psipe_bar *bar)
{
	dma_addr_t handle;
	struct scatterlist *sg;
	unsigned ofs = 0;
	int i;

	iowrite32((u32)dma->len, bar->mmio + PSIPE_HW_BAR0_DMA_CFG_LEN);
	iowrite32((u32)dma->nmapped, bar->mmio + PSIPE_HW_BAR0_DMA_CFG_PGS);

	for_each_sg(dma->sgt.sgl, sg, dma->nmapped, i) {
		handle = sg_dma_address(sg);
		iowrite32((u32)handle,
				bar->mmio + PSIPE_HW_BAR0_DMA_HANDLES + ofs);
		ofs += sizeof(u32);
	}
}

void psipe_dma_doorbell_ring(struct psipe_bar *bar)
{
	iowrite32(1, bar->mmio + PSIPE_HW_BAR0_DMA_DOORBELL_RING);
}

void psipe_dma_unmap_pages(struct psipe_dma *dma, struct pci_dev *pdev)
{
	dma_unmap_sg(&pdev->dev, dma->sgt.sgl, dma->sgt.nents, dma->direction);
}

void psipe_dma_unpin_pages(struct psipe_dma *dma)
{
	unpin_user_pages(dma->pages, dma->npages);
	kfree(dma->pages);
}
