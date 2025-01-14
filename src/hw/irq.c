/* irq.h - Interrupt Request operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "qemu/osdep.h"
#include "hw/pci/msi.h"
#include "pnvl.h"
#include "irq.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

static inline void pnvl_irq_init_intx(PNVLDevice *dev, Error **errp)
{
	uint8_t *conf = dev->pci_dev.config;
	pci_config_set_interrupt_pin(conf, PNVL_HW_IRQ_INTX + 1);
}

static inline void pnvl_irq_init_msi(PNVLDevice *dev, Error **errp)
{
	msi_init(&dev->pci_dev, 0, PNVL_HW_IRQ_CNT, true, false, errp);
}

static inline void pnvl_irq_raise_intx(PNVLDevice *dev)
{
	dev->irq.pin.raised = true;
	pci_set_irq(&dev->pci_dev, 1);
}

static inline void pnvl_irq_lower_intx(PNVLDevice *dev)
{
	dev->irq.pin.raised = false;
	pci_set_irq(&dev->pci_dev, 0);
}

static inline void pnvl_irq_raise_msi(PNVLDevice *dev, unsigned int vector)
{
	MSIVector *vec;

	if (vector >= PNVL_IRQ_MAX_VECS)
		return;

	vec = &dev->irq.msi.msi_vectors[vector];
	vec->raised = true;
	msi_notify(&dev->pci_dev, vector);
}

static inline void pnvl_irq_lower_msi(PNVLDevice *dev, unsigned int vector)
{
	MSIVector *vec;

	if (vector >= PNVL_IRQ_MAX_VECS)
		return;

	vec = &dev->irq.msi.msi_vectors[vector];
	if (vec->raised)
		vec->raised = false;
}

/* ============================================================================
 * Public
 * ============================================================================
 */

void pnvl_irq_raise(PNVLDevice *dev, unsigned int vector)
{
	if (msi_enabled(&dev->pci_dev))
		pnvl_irq_raise_msi(dev, vector);
	else
		pnvl_irq_raise_intx(dev);
}

void pnvl_irq_lower(PNVLDevice *dev, unsigned int vector)
{
	if (msi_enabled(&dev->pci_dev))
		pnvl_irq_lower_msi(dev, vector);
	else
		pnvl_irq_lower_intx(dev);
}

void pnvl_irq_reset(PNVLDevice *dev)
{
	for (int i = 0; i < PNVL_HW_IRQ_CNT; ++i)
		pnvl_irq_lower(dev, i);
}

void pnvl_irq_init(PNVLDevice *dev, Error **errp)
{
	pnvl_irq_init_intx(dev, errp);
	pnvl_irq_init_msi(dev, errp);
}

void pnvl_irq_fini(PNVLDevice *dev)
{
	pnvl_irq_reset(dev);
	msi_uninit(&dev->pci_dev);
}
