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
	struct pnvl_data *data = &pnvl_dev->data;
	int first_page, last_page, npages, pinned, rv;

	first_page = (data->addr & PAGE_MASK) >> PAGE_SHIFT;
	last_page = ((data->addr + data->len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	npages = last_page - first_page + 1;
	dma->npages = npages;

	if (npages <= 0 || npages > PNVL_HW_BAR0_DMA_HANDLES_CNT)
		return -EMSGSIZE;

	rv = account_locked_vm(current->mm, npages, true);
	if (rv)
		return rv;

	dma->pages = kcalloc(npages, sizeof(struct page *), GFP_KERNEL);
	if (!dma->pages) {
		rv = -ENOMEM;
		goto unlock_vm;
	}

	pinned = pin_user_pages_fast(data->addr, npages,
			FOLL_LONGTERM | FOLL_WRITE, dma->pages);
	if (pinned < 0) {
		rv = pinned;
		goto free_pages;
	} else if (pinned != npages) {
		rv = -EFAULT;
		goto unpin_pages;
	}

	printk(KERN_INFO "GUP: addr=%#010lx, npages=%d, pages=%p, rv=%d\n",
			data->addr, npages, dma->pages, rv);

	return 0;

unpin_pages:
	unpin_user_pages(dma->pages, pinned);
free_pages:
	kfree(dma->pages);
unlock_vm:
	account_locked_vm(current->mm, npages, false);
	return rv;
}

int pnvl_dma_map_pages(struct pnvl_dev *pnvl_dev)
{
	struct pci_dev *pdev = pnvl_dev->pdev;
	struct pnvl_dma *dma = &pnvl_dev->dma;
	dma_addr_t *handles;
	size_t len, len_map, ofs;

	handles = kcalloc(dma->npages, sizeof(*handles), GFP_KERNEL);

	len = dma->len;
	ofs = pnvl_dev->data.addr & ~PAGE_MASK;
	len_map = PAGE_SIZE - ofs;
	if (len_map > len)
		len_map = len;
	len -= len_map;
	handles[0] = dma_map_page(&pdev->dev, dma->pages[0], ofs, len_map,
			dma->direction);
	if (dma_mapping_error(&pdev->dev, handles[0]))
		goto free_handles;

	for (int i = 1; i < dma->npages; ++i) {
		len_map = len > PAGE_SIZE ?  PAGE_SIZE : len;
		len -= len_map;
		handles[i] = dma_map_page(&pdev->dev, dma->pages[i], 0,
				len_map, dma->direction);
		if (dma_mapping_error(&pdev->dev, handles[i]))
			goto free_handles;
	}

	dma->dma_handles = handles;

	return 0;

free_handles:
	kfree(handles);
	return -ENOMEM;
}

void pnvl_dma_write_config(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;
	struct pnvl_data *data = &pnvl_dev->data;
	void __iomem *mmio = pnvl_dev->bar.mmio;
	unsigned int ofs = 0;

	iowrite32((u32)data->len, mmio + PNVL_HW_BAR0_DMA_CFG_LEN);
	iowrite32((u32)dma->mode, mmio + PNVL_HW_BAR0_DMA_CFG_MOD);
	iowrite32((u32)dma->npages, mmio + PNVL_HW_BAR0_DMA_CFG_PGS);

	for (int i = 0; i < dma->npages; ++i) {
		iowrite32((u32)dma->dma_handles[i],
			mmio + PNVL_HW_BAR0_DMA_HANDLES + ofs);
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

	for (int i = 0; i < dma->npages; ++i) {
		dma_unmap_page(&pnvl_dev->pdev->dev, dma->dma_handles[i],
			dma->len, dma->direction);
	}

	kfree(dma->dma_handles);
}

void pnvl_dma_unpin_pages(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;
	// unpin_user_pages_dirty_lock(dma->pages, dma->npages, true);
	unpin_user_pages(dma->pages, dma->npages);
	kfree(dma->pages);
	account_locked_vm(current->mm, dma->npages, false);
}

void pnvl_dma_wait(struct pnvl_dev *pnvl_dev)
{
	if (!pnvl_dev->wq_flag)
		wait_event(pnvl_dev->wq, pnvl_dev->wq_flag == 1);
	pnvl_dev->wq_flag = 0;
}

void pnvl_dma_wake(struct pnvl_dev *pnvl_dev)
{
	pnvl_dev->wq_flag = 1;
	wake_up(&pnvl_dev->wq);
}
