/* dma.c - Direct Memory Access (DMA) operations
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "qemu/osdep.h"
#include "exec/target_page.h"
#include "qemu/log.h"
#include "psipe.h"
#include "dma.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

static inline dma_addr_t psipe_dma_mask(DMAEngine *dma, dma_addr_t addr)
{
	dma_addr_t masked_addr = addr & dma->config.mask;
	if (masked_addr != addr) {
		qemu_log_mask(LOG_GUEST_ERROR,
				"masked_addr (%" PRIx64 ") != addr (%" PRIx64 ")",
				masked_addr, addr);
	}
	return masked_addr;
}

static inline bool psipe_dma_inside_dev_boundaries(dma_addr_t addr)
{
	return (PSIPE_HW_DMA_AREA_START <= addr &&
			addr <= PSIPE_HW_DMA_AREA_START + PSIPE_HW_DMA_AREA_SIZE);
}

static inline void psipe_dma_init_current(DMAEngine *dma)
{
	dma->current.len_left = dma->config.len;
	dma->current.addr = dma->config.handles[0];
	dma->current.hnd_pos = 0;
}

static int psipe_dma_read(PSIPEDevice *dev, dma_addr_t addr, int len, int ofs)
{
	//printf("DMA RD: %d bytes @ %#010lx\n", len, addr);
	return pci_dma_read(&dev->pci_dev, addr, dev->dma.buff + ofs, len);
}

static int psipe_dma_write(PSIPEDevice *dev, dma_addr_t addr, int len, int ofs)
{
	//printf("DMA WR: %d bytes @ %#010lx\n", len, addr);
	return pci_dma_write(&dev->pci_dev, addr, dev->dma.buff + ofs, len);
}


/* ============================================================================
 * Public
 * ============================================================================
 */

/*
 * Receive page: DMA buffer <-- RAM
 */
int psipe_dma_rx_page(PSIPEDevice *dev)
{
	DMAEngine *dma = &dev->dma;
	unsigned long mask = psipe_dma_mask(dma, dma->config.page_size - 1);
	dma_addr_t addr = dma->current.addr;
	size_t ofs, len_want, len_have;

	ofs = addr & mask;
	len_have = dma->config.page_size - ofs;
	len_want = MIN(dma->config.page_size, dma->current.len_left);

	if (len_want <= len_have) {
		if (psipe_dma_read(dev, addr, len_want, 0) < 0)
			return PSIPE_FAILURE;
		addr += len_want;
	} else {
		if (psipe_dma_read(dev, addr, len_have, 0) < 0)
			return PSIPE_FAILURE;
		addr = dma->config.handles[++dma->current.hnd_pos];

		if (psipe_dma_read(dev, addr, len_want-len_have, len_have) < 0)
			return PSIPE_FAILURE;
		addr += len_want - len_have;
	}

	dma->current.addr = addr;
	dma->current.len_left -= len_want;
	return len_want;
}

/*
 * Transmit page: DMA buffer --> RAM
 */
int psipe_dma_tx_page(PSIPEDevice *dev, int len_want)
{
	DMAEngine *dma = &dev->dma;
	unsigned long mask = psipe_dma_mask(dma, dma->config.page_size - 1);
	dma_addr_t addr = dma->current.addr;
	size_t ofs, len_have;

	ofs = addr & mask;
	len_have = dma->config.page_size - ofs;

	if (len_want == PSIPE_FAILURE)
		return PSIPE_FAILURE;

	if (len_want <= len_have) {
		if (psipe_dma_write(dev, addr, len_want, 0) < 0)
			return PSIPE_FAILURE;
		addr += len_want;
	} else {
		if (psipe_dma_write(dev, addr, len_have, 0) < 0)
			return PSIPE_FAILURE;
		addr = dma->config.handles[++dma->current.hnd_pos];

		if (psipe_dma_write(dev, addr, len_want-len_have, len_have) < 0)
			return PSIPE_FAILURE;
		addr += len_want - len_have;
	}

	dma->current.addr = addr;
	dma->current.len_left -= len_want;
	return PSIPE_SUCCESS;
}

int psipe_dma_begin_run(PSIPEDevice *dev)
{
	DMAStatus status;

	psipe_dma_init_current(&dev->dma);
	status = qatomic_cmpxchg(&dev->dma.status, DMA_STATUS_IDLE,
			DMA_STATUS_EXECUTING);
	if (status == DMA_STATUS_EXECUTING)
		return PSIPE_FAILURE;

	return PSIPE_SUCCESS;
}

void psipe_dma_end_run(PSIPEDevice *dev)
{
	qatomic_set(&dev->dma.status, DMA_STATUS_IDLE);
}

void psipe_dma_add_handle(PSIPEDevice *dev, dma_addr_t handle)
{
	DMAEngine *dma = &dev->dma;
	dma_addr_t new_hnd = psipe_dma_mask(dma, handle);
	dma->config.handles[dma->config.npages++] = new_hnd;
}

bool psipe_dma_is_idle(PSIPEDevice *dev)
{
	return qatomic_read(&dev->dma.status) == DMA_STATUS_IDLE;
}

bool psipe_dma_is_finished(PSIPEDevice *dev)
{
	return !dev->dma.current.len_left;
}

void psipe_dma_reset(PSIPEDevice *dev)
{
	DMAEngine *dma = &dev->dma;
	dma->status = DMA_STATUS_IDLE;
	dma->config.npages = 0;
	dma->config.len = 0;
	dma->config.page_size = qemu_target_page_size();
	memset(dma->buff, 0, PSIPE_HW_DMA_AREA_SIZE);
	memset(dma->config.handles, 0,
			sizeof(dma_addr_t) * PSIPE_HW_BAR0_DMA_HANDLES_CNT);
}

void psipe_dma_init(PSIPEDevice *dev, Error **errp)
{
	psipe_dma_reset(dev);
	dev->dma.config.mask = DMA_BIT_MASK(PSIPE_HW_DMA_ADDR_CAPABILITY);
}

void psipe_dma_fini(PSIPEDevice *dev)
{
	psipe_dma_reset(dev);
	dev->dma.status = DMA_STATUS_OFF;
}
