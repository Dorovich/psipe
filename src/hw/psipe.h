/* psipe.h
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#ifndef PSIPE_H
#define PSIPE_H

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "psipe_hw.h"
#include "dma.h"
#include "irq.h"
#include "proxy.h"

#define TYPE_PSIPE_DEVICE "psipe"
#define PSIPE_DEVICE_DESC "Proto-SIPE Device"

#define PSIPE_SUCCESS 0
#define PSIPE_FAILURE -1

OBJECT_DECLARE_TYPE(PSIPEDevice, PSIPEDeviceClass, PSIPE_DEVICE);
DECLARE_INSTANCE_CHECKER(PSIPEDevice, PSIPE, TYPE_PSIPE_DEVICE);

typedef struct PSIPEDeviceClass {
	PCIDeviceClass parent_class;
} PSIPEDeviceClass;

typedef struct PSIPEDevice {
	PCIDevice pci_dev;
	IRQStatus irq;
	DMAEngine dma;
	MemoryRegion mmio;
	PSIPEProxy proxy;
} PSIPEDevice;


/* ============================================================================
 * Public
 * ============================================================================
 */

void psipe_execute(PSIPEDevice *dev);

#endif /* PSIPE_H */
