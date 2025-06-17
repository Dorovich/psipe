/* mmio.c - Memory Mapped IO operations
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "qemu/osdep.h"
#include "exec/target_page.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/units.h"
#include "mmio.h"
#include "irq.h"
#include "psipe_hw.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

static inline bool psipe_mmio_valid_access(hwaddr addr, unsigned int size)
{
	return (PSIPE_HW_BAR0_START <= addr && addr <= PSIPE_HW_BAR0_END);
}

static inline int psipe_mmio_handle_pos(hwaddr addr)
{
	return ((addr - PSIPE_HW_BAR0_DMA_HANDLES) / sizeof(uint32_t));
}

static void psipe_mmio_write_handle(DMAEngine *dma, hwaddr addr, uint64_t hnd)
{
	int pos = psipe_mmio_handle_pos(addr);

	if (pos >= dma->config.npages || !dma->config.handles)
		return;

	dma->config.handles[pos] = hnd;
	printf("+ %#010lx at %#06lx (pos=%d)\n", hnd, addr, pos);
}

static uint64_t psipe_mmio_read(void *opaque, hwaddr addr, unsigned int size)
{
	PSIPEDevice *dev = opaque;
	uint64_t val = ~0ULL;

	if (!psipe_mmio_valid_access(addr, size))
		goto mmio_read_end;

	switch(addr) {
	case PSIPE_HW_BAR0_DMA_CFG_LEN:
		val = dev->dma.config.len;
		break;
	case PSIPE_HW_BAR0_DMA_CFG_PGS:
		val = dev->dma.config.npages;
		break;
	case PSIPE_HW_BAR0_DMA_CFG_MOD:
		val = dev->dma.mode;
		break;
	case PSIPE_HW_BAR0_DMA_CFG_LEN_AVAIL:
		if (dev->dma.mode == DMA_MODE_ACTIVE) {
			psipe_proxy_issue_req(dev, PSIPE_REQ_SLN);
			psipe_proxy_await_req(dev, PSIPE_REQ_RLN);
		}
		val = dev->dma.config.len_avail;
		break;
	}

mmio_read_end:
	return val;
}

static void psipe_mmio_write(void *opaque, hwaddr addr, uint64_t val,
				unsigned int size)
{
	PSIPEDevice *dev = opaque;
	DMAEngine *dma = &dev->dma;

	if (!psipe_mmio_valid_access(addr, size))
		return;

	if (!psipe_dma_is_idle(dev))
		return;

	switch(addr) {
	case PSIPE_HW_BAR0_IRQ_0_RAISE:
		psipe_irq_raise(dev, 0);
		break;
	case PSIPE_HW_BAR0_IRQ_0_LOWER:
		psipe_irq_lower(dev, 0);
		break;
	case PSIPE_HW_BAR0_DMA_CFG_LEN:
		dma->config.len = val;
		break;
	case PSIPE_HW_BAR0_DMA_CFG_PGS:
		dma->config.npages = val;
		break;
	case PSIPE_HW_BAR0_DMA_CFG_MOD:
		dma->mode = val > 0 ? DMA_MODE_ACTIVE : DMA_MODE_PASSIVE;
		break;
	case PSIPE_HW_BAR0_DMA_CFG_LEN_AVAIL:
		dma->config.len_avail = val;
		break;
	case PSIPE_HW_BAR0_DMA_DOORBELL_RING:
		psipe_execute(dev);
		break;
	default: /* DMA handles area */
		psipe_mmio_write_handle(dma, addr, val);
		break;
	}
}

/* ============================================================================
 * Public
 * ============================================================================
 */

void psipe_mmio_reset(PSIPEDevice *dev)
{
	return;
}

void psipe_mmio_init(PSIPEDevice *dev, Error **errp)
{
	memory_region_init_io(&dev->mmio, OBJECT(dev), &psipe_mmio_ops, dev,
			"psipe-mmio", qemu_target_page_size());

	pci_register_bar(&dev->pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY,
			&dev->mmio);
}

void psipe_mmio_fini(PSIPEDevice *dev)
{
	psipe_mmio_reset(dev);
}

const MemoryRegionOps psipe_mmio_ops = {
	.read = psipe_mmio_read,
	.write = psipe_mmio_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
	.valid = {
		.min_access_size = 4,
		.max_access_size = 8,
	},
	.impl = {
		.min_access_size = 4,
		.max_access_size = 8,
	},
};
