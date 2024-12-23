/* mmio.h - Memory Mapped IO operations
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#ifndef PNVL_MMIO_H
#define PNVL_MMIO_H

#include "pnvl.h"

/* forward declaration */
typedef struct PNVLDevice PNVLDevice;

/* ============================================================================
 * Public
 * ============================================================================
 */

void pnvl_mmio_reset(PNVLDevice *dev);
void pnvl_mmio_init(PNVLDevice *dev, Error **errp);
void pnvl_mmio_fini(PNVLDevice *dev);

extern const MemoryRegionOps pnvl_mmio_ops;

#endif /* PNVL_MMIO_H */
