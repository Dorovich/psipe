/* irq.c - Interrupt Request operations
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "qemu/osdep.h"
#include "hw/pci/msi.h"
#include "psipe.h"
#include "irq.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

static inline void psipe_irq_init_intx(PSIPEDevice *dev, Error **errp)
{
	uint8_t *conf = dev->pci_dev.config;
	pci_config_set_interrupt_pin(conf, PSIPE_HW_IRQ_INTX + 1);
}

static inline void psipe_irq_init_msi(PSIPEDevice *dev, Error **errp)
{
	msi_init(&dev->pci_dev, 0, PSIPE_HW_IRQ_CNT, true, false, errp);
}

static inline void psipe_irq_raise_intx(PSIPEDevice *dev)
{
	dev->irq.pin.raised = true;
	pci_set_irq(&dev->pci_dev, 1);
}

static inline void psipe_irq_lower_intx(PSIPEDevice *dev)
{
	dev->irq.pin.raised = false;
	pci_set_irq(&dev->pci_dev, 0);
}

static inline void psipe_irq_raise_msi(PSIPEDevice *dev, unsigned int vector)
{
	MSIVector *vec;

	if (vector >= PSIPE_IRQ_MAX_VECS)
		return;

	vec = &dev->irq.msi.msi_vectors[vector];
	vec->raised = true;
	msi_notify(&dev->pci_dev, vector);
}

static inline void psipe_irq_lower_msi(PSIPEDevice *dev, unsigned int vector)
{
	MSIVector *vec;

	if (vector >= PSIPE_IRQ_MAX_VECS)
		return;

	vec = &dev->irq.msi.msi_vectors[vector];
	if (vec->raised)
		vec->raised = false;
}

/* ============================================================================
 * Public
 * ============================================================================
 */

void psipe_irq_raise(PSIPEDevice *dev, unsigned int vector)
{
	if (msi_enabled(&dev->pci_dev))
		psipe_irq_raise_msi(dev, vector);
	else
		psipe_irq_raise_intx(dev);
}

void psipe_irq_lower(PSIPEDevice *dev, unsigned int vector)
{
	if (msi_enabled(&dev->pci_dev))
		psipe_irq_lower_msi(dev, vector);
	else
		psipe_irq_lower_intx(dev);
}

void psipe_irq_reset(PSIPEDevice *dev)
{
	for (int i = 0; i < PSIPE_HW_IRQ_CNT; ++i)
		psipe_irq_lower(dev, i);
}

void psipe_irq_init(PSIPEDevice *dev, Error **errp)
{
	psipe_irq_init_intx(dev, errp);
	psipe_irq_init_msi(dev, errp);
}

void psipe_irq_fini(PSIPEDevice *dev)
{
	psipe_irq_reset(dev);
	msi_uninit(&dev->pci_dev);
}
