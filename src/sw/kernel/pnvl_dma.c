/* pnvl_irq.c - pnvl virtual device DMA operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "hw/pnvl_hw.h"
#include "pnvl_module.h"
#include <linux/dma-mapping.h>

bool pnvl_dma_check_size_avail(struct pnvl_dev *pnvl_dev)
{
	void __iomem *mmio = pnvl_dev->bar.mmio;
	size_t len_avail = ioread32(mmio + PNVL_HW_BAR0_DMA_CFG_LEN_AVAIL);
	return pnvl_dev->data.len <= len_avail;
}

int pnvl_dma_pin_pages(struct pnvl_dev *pnvl_dev)
{
	int first_page, last_page, npages, npages_pinned = 0;
	struct pnvl_data *data = &pnvl_dev->data;

	first_page = (data->addr & PAGE_MASK) >> PAGE_SHIFT;
	last_page = ((data->addr + data->len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	npages = last_page - first_page + 1;
	npages_pinned = pin_user_pages_fast(data->addr, npages,
					FOLL_LONGTERM, pnvl_dev->dma.pages);

	pnvl_dev->dma.offset = data->addr & ~PAGE_MASK;
	pnvl_dev->dma.npages = npages;
	pnvl_dev->dma.len = data->len;

	return npages_pinned - npages;
}

int pnvl_dma_get_handles(struct pnvl_dev *pnvl_dev)
{
	struct pci_dev *pdev = pnvl_dev->pdev;
	struct pnvl_dma *dma = &pnvl_dev->dma;
	dma_addr_t *handles;
	size_t len, len_map;
	int err = 0;

	handles = kmalloc(dma->npages * sizeof(dma_addr_t), GFP_KERNEL);

	len = dma->len;
	len_map = PAGE_SIZE - dma->offset;
	if (len_map > len)
		len_map = len;
	len -= len_map;
	handles[0] = dma_map_page(&pdev->dev, dma->pages[0], dma->offset,
			len_map, dma->direction);
	if (dma_mapping_error(&pdev->dev, handles[0])) {
		err = -ENOMEM;
		goto err_dma_map;
	}

	for (int i = 1; i < dma->npages; ++i) {
		len_map = len > PAGE_SIZE ?  PAGE_SIZE : len;
		len -= len_map;
		handles[i] = dma_map_page(&pdev->dev, dma->pages[i], 0,
				len_map, dma->direction);
		if (dma_mapping_error(&pdev->dev, handles[i])) {
			err = -ENOMEM;
			goto err_dma_map;
		}
	}

	dma->dma_handles = handles;
	return 0;

err_dma_map:
	kfree(handles);
	return err;
}

void pnvl_dma_write_params(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;
	void __iomem *mmio = pnvl_dev->bar.mmio;
	size_t ofs = 0;

	if (dma->mode == PNVL_MODE_PASSIVE) {
		iowrite32((u32)pnvl_dev->data.len,
				mmio + PNVL_HW_BAR0_DMA_CFG_LEN_AVAIL);
	}

	iowrite32((u32)dma->len, mmio + PNVL_HW_BAR0_DMA_CFG_LEN);
	iowrite32((u32)dma->npages, mmio + PNVL_HW_BAR0_DMA_CFG_PGS);
	iowrite32((u32)dma->mode, mmio + PNVL_HW_BAR0_DMA_CFG_MOD);

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

void pnvl_dma_mode_active(struct pnvl_dev *pnvl_dev)
{
	pnvl_dev->dma.mode = PNVL_MODE_ACTIVE;
	pnvl_dev->sending = true;
	pnvl_dev->recving = false;
}

void pnvl_dma_mode_passive(struct pnvl_dev *pnvl_dev)
{
	pnvl_dev->dma.mode = PNVL_MODE_PASSIVE;
	pnvl_dev->sending = false;
	pnvl_dev->recving = true;
}

void pnvl_dma_mode_off(struct pnvl_dev *pnvl_dev)
{
	pnvl_dev->dma.mode = -1;
	pnvl_dev->sending = false;
	pnvl_dev->recving = false;
}

void pnvl_dma_dismantle(struct pnvl_dev *pnvl_dev)
{
	struct pnvl_dma *dma = &pnvl_dev->dma;

	for (int i = 0; i < dma->npages; ++i) {
		dma_unmap_page(&pnvl_dev->pdev->dev, dma->dma_handles[i],
			dma->len, dma->direction);
		unpin_user_page(dma->pages[i]);
	}
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
