/* mmio.c - Memory Mapped IO operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "exec/target_page.h"
#include "irq.h"
#include "mmio.h"
#include "pnvl_hw.h"
#include "qemu/error.h"
#include "qemu/log.h"
#include "qemu/osdep.h"
#include "qemu/osdep.h"
#include "qemu/units.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

static inline bool pnvl_mmio_valid_access(hwaddr addr, size_t size)
{
	return (PNVL_HW_BAR0_START <= addr && addr <= PNVL_HW_BAR0_END);
}

static uint64_t pnvl_mmio_read(void *opaque, hwaddr addr, size_t size)
{
	PNVLDevice *dev = opaque;
	uint64_t val = ~0ULL;

	if (!pnvl_mmio_valid_access(addr, size))
		return val;

	switch(addr) {
	case PNVL_HW_BAR0_DMA_CFG_LEN:
		val = dev->dma.config.len;
		break;
	case PNVL_HW_BAR0_DMA_CFG_PGS:
		val = dev->dma.config.npages;
		break;
	case PNVL_HW_BAR0_DMA_CFG_OFS:
		val = dev->dma.config.offset;
		break;
	case PNVL_HW_BAR0_DMA_CFG_MODE:
		val = dev->dma.config.mode;
		break;
	}

	return val;
}

static void pnvl_mmio_write(void *opaque, hwaddr addr, uint64_t val,
				size_t size)
{
	PNVLDevice *dev = opaque;

	if (!pnvl_mmio_valid_access(addr, size))
		return;

	switch(addr) {
	case PNVL_HW_BAR0_IRQ_0_RAISE:
		pnvl_irq_raise(dev, 0);
		break;
	case PNVL_HW_BAR0_IRQ_0_LOWER:
		pnvl_irq_lower(dev, 0);
		break;
	case PNVL_HW_BAR0_DMA_CFG_LEN:
		if (pnvl_dma_is_idle(dev))
			dev->dma.config.len = val;
		break;
	case PNVL_HW_BAR0_DMA_CFG_PGS:
		if (pnvl_dma_is_idle(dev))
			dev->dma.config.npages = val;
		break;
	case PNVL_HW_BAR0_DMA_CFG_OFS:
		if (pnvl_dma_is_idle(dev))
			dev->dma.config.offset = val;
		break;
	case PNVL_HW_BAR0_DMA_CFG_MODE:
		if (pnvl_dma_is_idle(dev))
			dev->dma.config.mode = val;
		break;
	case PNVL_HW_BAR0_DMA_DOORBELL_RING:
		pnvl_transfer_pages(dev);
		break;
	default: /* DMA handles area */
		if (pnvl_dma_is_idle(dev)) {
			int pos = (addr-PNVL_HW_BAR0_HANDLES)/sizeof(addr);
			dev->dma.handle[pos] = val;
		}
		break;
	}
}

/* ============================================================================
 * Public
 * ============================================================================
 */

void pnvl_mmio_reset(PNVLDevice *dev)
{
	return;
}

void pnvl_mmio_init(PNVLDevice *dev, Error **errp)
{
	memory_region_init_io(&dev->mmio, OBJECT(dev), &pnvl_mmio_ops, dev,
			"pnvl-mmio", qemu_target_page_size());

	pci_register_bar(&dev->pci_dev, 0, PCI_BASE_ADRESS_SPACE_MEMORY,
			&dev->mmio);
}

void pnvl_mmio_fini(PNVLDevice *dev)
{
	pnvl_mmio_reset();
}

const MemoryRegionOps pnvl_mmio_ops = {
	.read = pnvl_mmio_read,
	.write = pnvl_mmio_write,
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
