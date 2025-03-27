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
	int first_page, last_page, npages, pinned;

	first_page = (data->addr & PAGE_MASK) >> PAGE_SHIFT;
	last_page = ((data->addr + data->len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	npages = last_page - first_page + 1;
	if (npages <= 0 || npages > PNVL_HW_BAR0_DMA_HANDLES_CNT)
		return -1;
	dma->npages = npages;

	/* TESTING BEGIN */
	unsigned long end;
	check_add_overflow(start, len, &end);
	int test1 = end > TASK_SIZE_MAX; // gup.c - L3207
	printk(KERN_INFO "test1 - task size exceeded = %d\n", test1);

	unsigned long start = untagged_addr(data->addr) & PAGE_MASK;
	unsigned long len = npages << PAGE_SHIFT;
	int test2 = !access_ok((void __user *)start, len); // gup.c - L3209
	printk(KERN_INFO "test2 - bad access = %d\n", test2);

	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = vma_lookup(mm, start);
	int test3 = !!vma; // gup.c - L1124
	printk(KERN_INFO "test3 - vma bound crossed = %d\n", test3);
	/* TESTING END */

	pinned = pin_user_pages_fast(data->addr, npages, FOLL_LONGTERM,
			dma->pages);

	printk(KERN_INFO "pinning userspace pages - returned %d\n", pinned);

	return -(pinned != npages);
}

int pnvl_dma_get_handles(struct pnvl_dev *pnvl_dev)
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
		goto err_dma_map;

	for (int i = 1; i < dma->npages; ++i) {
		len_map = len > PAGE_SIZE ?  PAGE_SIZE : len;
		len -= len_map;
		handles[i] = dma_map_page(&pdev->dev, dma->pages[i], 0,
				len_map, dma->direction);
		if (dma_mapping_error(&pdev->dev, handles[i]))
			goto err_dma_map;
	}

	dma->dma_handles = handles;
	return 0;

err_dma_map:
	kfree(handles);
	return -ENOMEM;
}

void pnvl_dma_write_params(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;
	struct pnvl_data *data = &pnvl_dev->data;
	void __iomem *mmio = pnvl_dev->bar.mmio;
	bool ret_data = pnvl_dev->sending || pnvl_dev->recving;
	unsigned int ofs = 0;

	if (dma->mode == PNVL_MODE_PASSIVE) {
		iowrite32((u32)data->len,
				mmio + PNVL_HW_BAR0_DMA_CFG_LEN_AVAIL);
	}

	iowrite32((u32)data->len, mmio + PNVL_HW_BAR0_DMA_CFG_LEN);
	iowrite32((u32)dma->npages, mmio + PNVL_HW_BAR0_DMA_CFG_PGS);
	iowrite32((u32)dma->mode, mmio + PNVL_HW_BAR0_DMA_CFG_MOD);
	iowrite32((u32)ret_data, mmio + PNVL_HW_BAR0_RETURN);

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

void pnvl_dma_release_handles(struct pnvl_dev *pnvl_dev)
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
	for (int i = 0; i < dma->npages; ++i)
		unpin_user_page(dma->pages[i]);
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
