/* dma.h - Direct Memory Access (DMA) operations
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#ifndef PSIPE_DMA_H
#define PSIPE_DMA_H

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "psipe_hw.h"

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))

/* forward declaration */
typedef struct PSIPEDevice PSIPEDevice;

typedef dma_addr_t dma_size_t;
typedef uint64_t dma_mask_t;

typedef struct DMAConfig {
	dma_size_t npages;
	dma_size_t len;
	dma_size_t len_avail;
	dma_mask_t mask;
	size_t page_size;
	dma_addr_t handles[PSIPE_HW_BAR0_DMA_HANDLES_CNT];
} DMAConfig;

typedef struct DMACurrent {
	dma_size_t len_left;
	dma_addr_t addr;
	int hnd_pos;
} DMACurrent;

typedef enum DMAStatus {
	DMA_STATUS_IDLE,
	DMA_STATUS_EXECUTING,
	DMA_STATUS_OFF,
} DMAStatus;

typedef enum DMAMode {
	DMA_MODE_ACTIVE,
	DMA_MODE_PASSIVE,
} DMAMode;

typedef struct DMAEngine {
	DMAConfig config;
	DMACurrent current;
	DMAStatus status;
	DMAMode mode;
	uint8_t buff[PSIPE_HW_DMA_AREA_SIZE];
} DMAEngine;

/* ============================================================================
 * Public
 * ============================================================================
 */

int psipe_dma_rx_page(PSIPEDevice *dev);
int psipe_dma_tx_page(PSIPEDevice *dev, int len_want);

int psipe_dma_begin_run(PSIPEDevice *dev);
void psipe_dma_end_run(PSIPEDevice *dev);
void psipe_dma_add_handle(PSIPEDevice *dev, dma_addr_t handle);
bool psipe_dma_is_idle(PSIPEDevice *dev);
bool psipe_dma_is_finished(PSIPEDevice *dev);

void psipe_dma_reset(PSIPEDevice *dev);
void psipe_dma_init(PSIPEDevice *dev, Error **errp);
void psipe_dma_fini(PSIPEDevice *dev);

#endif /* PSIPE_DMA_H */
