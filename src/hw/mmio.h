/* mmio.h - Memory Mapped IO operations
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#ifndef PSIPE_MMIO_H
#define PSIPE_MMIO_H

#include "psipe.h"

/* forward declaration */
typedef struct PSIPEDevice PSIPEDevice;

/* ============================================================================
 * Public
 * ============================================================================
 */

void psipe_mmio_reset(PSIPEDevice *dev);
void psipe_mmio_init(PSIPEDevice *dev, Error **errp);
void psipe_mmio_fini(PSIPEDevice *dev);

extern const MemoryRegionOps psipe_mmio_ops;

#endif /* PSIPE_MMIO_H */
