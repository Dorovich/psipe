/* irq.h - Interrupt Request operations
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#ifndef PSIPE_IRQ_H
#define PSIPE_IRQ_H

#include "qemu/osdep.h"
#include "hw/pci/pci.h"

#define PSIPE_IRQ_MAX_VECS 4

/* Forward declaration */
typedef struct PSIPEDevice PSIPEDevice;

typedef struct MSIVector {
	PSIPEDevice *dev;
	bool raised;
} MSIVector;

typedef struct IRQStatusMSI {
	MSIVector msi_vectors[PSIPE_IRQ_MAX_VECS];
} IRQStatusMSI;

typedef struct IRQStatusPin {
	bool raised;
} IRQStatusPin;

typedef union IRQStatus {
	IRQStatusMSI msi;
	IRQStatusPin pin;
} IRQStatus;

/* ============================================================================
 * Public
 * ============================================================================
 */

void psipe_irq_raise(PSIPEDevice *dev, unsigned int vector);
void psipe_irq_lower(PSIPEDevice *dev, unsigned int vector);
int psipe_irq_check(PSIPEDevice *dev, unsigned int vector);

void psipe_irq_reset(PSIPEDevice *dev);
void psipe_irq_init(PSIPEDevice *dev, Error **errp);
void psipe_irq_fini(PSIPEDevice *dev);

#endif /* PSIPE_IRQ_H */
