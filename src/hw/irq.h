/* irq.h - Interrupt Request operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#pragma once

#include "hw/pci/pci.h"
#include "qemu/osdep.h"

/* Forward declaration */
typedef struct PNVLDevice PNVLDevice;

typedef struct IRQStatusPin {
	bool raised;
} IRQStatusPin;

typedef struct IRQStatus {
	IRQStatusPin pin;
} IRQStatus;

/* ============================================================================
 * Public
 * ============================================================================
 */

void pnvl_irq_raise(PNVLDevice *dev, unsigned int pin);
void pnvl_irq_lower(PNVLDevice *dev, unsigned int pin);

void pnvl_irq_reset(PNVLDevice *dev);
void pnvl_irq_init(PNVLDevice *dev, Error **errp);
void pnvl_irq_fini(PNVLDevice *dev);
