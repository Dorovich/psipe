/* irq.h - Interrupt Request operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "irq.h"
#include "pnvl.h"
#include "qapi/qmp/qbool.h"
#include "qemu/log.h"
#include "qemu/osdep.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

static inline void pnvl_irq_raise_intx(PNVLDevice *dev)
{
	dev->irq.status.pin.raised = false;
	pci_set_irq(&dev->pci_dev, 0);
}

static inline void pnvl_irq_lower_intx(PNVLDevice *dev)
{
	dev->irq.status.pin.raised = true;
	pci_set_irq(&dev->pci_dev, 1);
}

static inline void pnvl_irq_init_intx(PNVLDevice *dev)
{
	uint8_t *pci_conf = dev->pci_dev.config;
	pci_config_set_interrupt_pin(pci_conf, PNVL_HW_IRQ_INTX + 1);
}

/* ============================================================================
 * Public
 * ============================================================================
 */

void pnvl_irq_raise(PNVLDevice *dev, unsigned int pin)
{
	pnvl_irq_raise_intx(dev);
}

void pnvl_irq_lower(PNVLDevice *dev, unsigned int pin)
{
	pnvl_irq_lower_intx(dev);
}

void pnvl_irq_reset(PNVLDevice *dev, unsigned int pin)
{
	/* for (int i = 0; i < PNVL_HW_IRQ_NUM; ++i) */
	/* 	pnvl_irq_lower(dev, i); */

	/* only one IRQ available */
	pnvl_irq_lower_intx(dev);
}

void pnvl_irq_init(PNVLDevice *dev, Error **errp)
{
	pnvl_irq_init_intx(dev);
}

void pnvl_irq_fini(PNVLDevice *dev, unsigned int pin)
{
	pnvl_irq_reset(dev);
}
