/* dma.h - Direct Memory Access (DMA) operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#pragma once

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "pnvl_hw.h"

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))

/* forward declaration */
typedef struct PNVLDevice PNVLDevice;

typedef uint64_t dma_mode_t;
typedef dma_addr_t dma_size_t;
typedef uint64_t dma_mask_t;

typedef struct DMAConfig {
	size_t npages;
	size_t offset;
	dma_size_t len;
	dma_addr_t handles[PNVL_HW_BAR0_DMA_WORK_AREA_SIZE];
	dma_mode_t mode;
	dma_mask_t mask;
} DMAConfig;

typedef enum DMAStatus {
	DMA_STATUS_IDLE,
	DMA_STATUS_EXECUTING,
	DMA_STATUS_OFF,
} DMAStatus;

typedef struct DMAEngine {
	DMAConfig config;
	DMAStatus status;
	uint8_t buff[PNVL_HW_DMA_AREA_SIZE];
} DMAEngine;

/* ============================================================================
 * Public
 * ============================================================================
 */

int pnvl_dma_rx_page(PNVLDevice *dev, dma_addr_t src, size_t *len);
int pnvl_dma_tx_page(PNVLDevice *dev, dma_addr_t dst, size_t ofs);

void pnvl_dma_add_handle(PNVLDevice *dev, dma_addr_t handle);

bool pnvl_dma_is_idle(PNVLDevice *dev);

void pnvl_dma_reset(PNVLDevice *dev);
void pnvl_dma_init(PNVLDevice *dev, Error **errp);
void pnvl_dma_fini(PNVLDevice *dev);
