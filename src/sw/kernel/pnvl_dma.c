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
	unsigned int flags;

	first_page = (data->addr & PAGE_MASK) >> PAGE_SHIFT;
	last_page = ((data->addr + data->len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	npages = last_page - first_page + 1;
	if (npages <= 0 || npages > PNVL_HW_BAR0_DMA_HANDLES_CNT)
		return -1;
	dma->npages = npages;

	/* TESTING BEGIN */
	/*
	 * Las llamadas a las functiones del kernel que pueden devolver EFAULT
	 * cuando se usa la funcion pin_user_pages_fast son:
	 *
	 * 	1. pin_user_pages_fast			(gup.c - L3320)
	 * 	2. internal_pin_user_pages_fast		(gup.c - L3181)
	 * 	3. __gup_longterm_locked		(gup.c - L2108)
	 * 	4. __get_user_pages_locked		(gup.c - L1391)
	 * 	5. __get_user_pages			(gup.c - L1096)
	 *
	 * Hay que investigar más. Por ahora la función vma_lookup parecer
	 * devolver un error.
	 */
	/*
	{
		unsigned long start, start_u, start_ur, end, len;
		int t1, t2, t3, t4;

		t1 = check_add_overflow(data->addr, data->len, &end);
		t2 = end > TASK_SIZE_MAX; // gup.c - L3207

		start_u = untagged_addr(data->addr) & PAGE_MASK;
		len = npages << PAGE_SHIFT;
		t3 = !access_ok((void __user *)start_u, len); // gup.c - L3209

		struct mm_struct *mm = current->mm;
		start_ur = untagged_addr_remote(mm, start);
		struct vm_area_struct *vma = vma_lookup(mm, start_ur);
		t4 = !vma; // gup.c - L1124

		printk(KERN_INFO "test errors: 1=%d, 2=%d, 3=%d, 4=%d\n",
				t1, t2, t3, t4);
	}
	*/
	/* TESTING END */

	flags = (pnvl_dev->sending || pnvl_dev->recving) ? FOLL_LONGTERM : 0;
	pinned = pin_user_pages_fast(data->addr, npages, flags, dma->pages);

	printk(KERN_INFO "pin_user_pages_fast(%d)\n", pinned);

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
