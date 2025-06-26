/* psipe.c - PCIe offload device emulation.
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "psipe.h"
#include "psipe_hw.h"
#include "dma.h"
#include "irq.h"
#include "mmio.h"
#include "proxy.h"
#include "qom/object.h"

/* ============================================================================
 * Object
 * ============================================================================
 */

static void psipe_device_init(PCIDevice *pci_dev, Error **errp)
{
	PSIPEDevice *dev = PSIPE_DEVICE(pci_dev);
	psipe_irq_init(dev, errp);
	psipe_dma_init(dev, errp);
	psipe_mmio_init(dev, errp);
	psipe_proxy_init(dev, errp);
}

static void psipe_device_fini(PCIDevice *pci_dev)
{
	PSIPEDevice *dev = PSIPE_DEVICE(pci_dev);
	psipe_irq_fini(dev);
	psipe_dma_fini(dev);
	psipe_mmio_fini(dev);
	psipe_proxy_fini(dev);
}

static void psipe_device_reset(DeviceState *dev_st)
{
	PSIPEDevice *dev = PSIPE_DEVICE(dev_st);
	psipe_irq_reset(dev);
	psipe_dma_reset(dev);
	psipe_mmio_reset(dev);
	psipe_proxy_reset(dev);
}

/* ============================================================================
 * Class
 * ============================================================================
 */

static void psipe_class_init(ObjectClass *klass, void *class_data)
{
	DeviceClass *dev_class = DEVICE_CLASS(klass);
	PCIDeviceClass *pci_dev_class = PCI_DEVICE_CLASS(klass);

	pci_dev_class->realize = psipe_device_init;
	pci_dev_class->exit = psipe_device_fini;
	pci_dev_class->vendor_id = PSIPE_HW_VENDOR_ID;
	pci_dev_class->device_id = PSIPE_HW_DEVICE_ID;
	pci_dev_class->revision = PSIPE_HW_REVISION;
	pci_dev_class->class_id = PCI_CLASS_OTHERS;

	set_bit(DEVICE_CATEGORY_MISC, dev_class->categories);
	dev_class->desc = PSIPE_DEVICE_DESC;
	dev_class->reset = psipe_device_reset;
}

static void psipe_instance_init(Object *obj)
{
	PSIPEDevice *dev = PSIPE(obj);

	dev->proxy.server_mode = true;
	object_property_add_bool(obj, "server_mode", psipe_proxy_get_mode,
				psipe_proxy_set_mode);

	dev->proxy.port = PSIPE_PROXY_PORT;
	object_property_add_uint16_ptr(obj, "port", &dev->proxy.port,
				OBJ_PROP_FLAG_READWRITE);
}

/* ============================================================================
 * Type information
 * ============================================================================
 */

static const TypeInfo psipe_info = {
	.name = TYPE_PSIPE_DEVICE,
	.parent = TYPE_PCI_DEVICE,
	.instance_size = sizeof(PSIPEDevice),
	.instance_init = psipe_instance_init,
	.class_init = psipe_class_init,
	.interfaces = (InterfaceInfo[]){
		{ INTERFACE_PCIE_DEVICE },
		{},
	},
};

static void psipe_register_types(void)
{
	type_register_static(&psipe_info);
}

type_init(psipe_register_types)

/* ============================================================================
 * Private
 * ============================================================================
 */

static void psipe_transfer_pages(PSIPEDevice *dev)
{
	int ret, len;

	//printf("(TX) beginning - %lu\n", dev->dma.config.len);
	if (psipe_dma_begin_run(dev) < 0)
		return;

	do {
		printf("TX:\t%lu / %lu bytes left\n", dev->dma.current.len_left,
				dev->dma.config.len);
		len = psipe_dma_rx_page(dev);
		ret = psipe_proxy_tx_page(dev, dev->dma.buff, len);
	} while (ret != PSIPE_FAILURE && !psipe_dma_is_finished(dev));

	psipe_dma_end_run(dev);
	//printf("(TX) finished - %d\n", ret);
}

static void psipe_receive_pages(PSIPEDevice *dev)
{
	int ret, len;

	//printf("(RX) beginning - %lu\n", dev->dma.config.len);
	if (psipe_dma_begin_run(dev) < 0)
		return;

	do {
		printf("RX:\t%lu / %lu bytes left\n", dev->dma.current.len_left,
				dev->dma.config.len);
		len = psipe_proxy_rx_page(dev, dev->dma.buff);
		ret = psipe_dma_tx_page(dev, len);
	} while (ret != PSIPE_FAILURE && !psipe_dma_is_finished(dev));

	psipe_dma_end_run(dev);
	//printf("(RX) finished - %d\n", ret);
}

/* ============================================================================
 * Public
 * ============================================================================
 */

void psipe_execute(PSIPEDevice *dev)
{
	printf(">>>>>>>>>> START RUN\n");
	switch(dev->dma.mode) {
	case DMA_MODE_ACTIVE:
		psipe_transfer_pages(dev);
		break;
	case DMA_MODE_PASSIVE:
		psipe_proxy_await_req(dev, PSIPE_REQ_SLN);
		psipe_receive_pages(dev);
		break;
	default:
		return;
	}
	printf("<<<<<<<<<< END RUN\n");
	psipe_irq_raise(dev, PSIPE_HW_IRQ_WORK_ENDED_VECTOR);
}
