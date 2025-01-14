/* irq.h - Interrupt Request operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#ifndef PNVL_IRQ_H
#define PNVL_IRQ_H

#include "qemu/osdep.h"
#include "hw/pci/pci.h"

#define PNVL_IRQ_MAX_VECS 4

/* Forward declaration */
typedef struct PNVLDevice PNVLDevice;

typedef struct MSIVector {
	PNVLDevice *dev;
	bool raised;
} MSIVector;

typedef struct IRQStatusMSI {
	MSIVector msi_vectors[PNVL_IRQ_MAX_VECS];
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

void pnvl_irq_raise(PNVLDevice *dev, unsigned int vector);
void pnvl_irq_lower(PNVLDevice *dev, unsigned int vector);

void pnvl_irq_reset(PNVLDevice *dev);
void pnvl_irq_init(PNVLDevice *dev, Error **errp);
void pnvl_irq_fini(PNVLDevice *dev);

#endif /* PNVL_IRQ_H */
