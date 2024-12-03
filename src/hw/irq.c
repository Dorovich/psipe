/* irq.h - Interrupt Request operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "irq.h"
#include "pnvl.h"
#include "qemu/log.h"
#include "qemu/osdep.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

void pnvl_irq_raise_intx(PNVLDevice *dev)
{
	return; /* TODO */
}

void pnvl_irq_lower_intx(PNVLDevice *dev)
{
	return; /* TODO */
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
	for (int i = 0; i < PNVL_HW_IRQ_NUM; ++i)
		pnvl_irq_lower(dev, i);
}

void pnvl_irq_init(PNVLDevice *dev, Error **errp)
{
	pnvl_irq_init_intx(dev);
}

void pnvl_irq_fini(PNVLDevice *dev, unsigned int pin)
{
	pnvl_irq_reset(dev);
}
