/* psipe_hw.h - Hardware resources description
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#pragma once

/* ============================================================================
 * Device info
 * ============================================================================
 */

#define PSIPE_HW_VENDOR_ID 0x1b36
#define PSIPE_HW_DEVICE_ID 0x1100
#define PSIPE_HW_REVISION 0x01

#define PSIPE_HW_BAR0 0
#define PSIPE_HW_BAR_CNT 1

/* ============================================================================
 * MMIO
 * ============================================================================
 */

#define PSIPE_HW_BAR0_IRQ_0_RAISE 0x00
#define PSIPE_HW_BAR0_IRQ_0_LOWER 0x08
#define PSIPE_HW_BAR0_DMA_CFG_LEN 0x10
#define PSIPE_HW_BAR0_DMA_CFG_PGS 0x18
#define PSIPE_HW_BAR0_DMA_CFG_MOD 0x20
#define PSIPE_HW_BAR0_DMA_CFG_LEN_AVAIL 0x28
#define PSIPE_HW_BAR0_DMA_DOORBELL_RING 0x30
#define PSIPE_HW_BAR0_DMA_HANDLES 0x38
/* 512 for a space of 2MB (if PAGE_SIZE is 4KB), or
 * 131072 (1MB) for a space of 512MB (if PAGE_SIZE is 4KB)
 * ... and 1 more if offset exists */
#define PSIPE_HW_BAR0_DMA_HANDLES_CNT (131072+1)

#define PSIPE_HW_BAR0_START PSIPE_HW_BAR0_IRQ_0_RAISE
#define PSIPE_HW_BAR0_END \
	(PSIPE_HW_BAR0_DMA_HANDLES + 64 * PSIPE_HW_BAR0_DMA_HANDLES_CNT)

#define PSIPE_HW_DMA_ADDR_CAPABILITY 32
#define PSIPE_HW_DMA_AREA_START (PSIPE_HW_BAR0_END + 0x1000)
#define PSIPE_HW_DMA_AREA_SIZE 0x1000

/* ============================================================================
 * IRQs
 * ============================================================================
 */

#define PSIPE_HW_IRQ_CNT 1
#define PSIPE_HW_IRQ_VECTOR_START 0
#define PSIPE_HW_IRQ_VECTOR_END 0
#define PSIPE_HW_IRQ_INTX 0

#define PSIPE_HW_IRQ_WORK_ENDED_VECTOR 0
#define PSIPE_HW_IRQ_WORK_ENDED_ADDR PSIPE_HW_BAR0_IRQ_0_RAISE
#define PSIPE_HW_IRQ_WORK_ENDED_ACK_ADDR PSIPE_HW_BAR0_IRQ_0_LOWER
