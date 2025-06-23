/* psipe_module.c - Kernel module to control the psipe virtual device
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "hw/psipe_hw.h"
#include "psipe_module.h"
#include "sw/module/psipe_ioctl.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_VERSION("2.0");
MODULE_DESCRIPTION("Kernel module to control the psipe virtual device");
MODULE_AUTHOR("David Cañadas López <david.canadas@estudiantat.upc.edu>");

static struct class *psipe_class;

static struct pci_device_id psipe_id_table[] = {
	{ PCI_DEVICE(PSIPE_HW_VENDOR_ID, PSIPE_HW_DEVICE_ID) },
	{},
};

MODULE_DEVICE_TABLE(pci, psipe_id_table);

static int psipe_open(struct inode *inode, struct file *fp)
{
	unsigned int bar = iminor(inode);
	struct psipe_dev *psipe_dev;

	psipe_dev = container_of(inode->i_cdev, struct psipe_dev, cdev);

	if (bar != 0)
		return -ENXIO;
	if (psipe_dev->bar.len == 0)
		return -EIO;

	fp->private_data = psipe_dev;

	return 0;
}

static bool psipe_check_size_avail(struct psipe_dma *dma, struct psipe_bar *bar)
{
	return dma->len <= ioread32(bar->mmio + PSIPE_HW_BAR0_DMA_CFG_LEN_AVAIL);
}

static void psipe_set_size_avail(struct psipe_dma *dma, struct psipe_bar *bar)
{
	iowrite32((u32)dma->len, bar->mmio + PSIPE_HW_BAR0_DMA_CFG_LEN_AVAIL);
}

long psipe_ioctl_send(struct psipe_dev *psipe_dev, struct psipe_dma *dma)
{
	struct psipe_bar *bar = &psipe_dev->bar;
	int rv = 0;

	psipe_dma_write_setup(dma, bar, PSIPE_MODE_ACTIVE, DMA_TO_DEVICE);
	if (!psipe_check_size_avail(dma, bar))
		return -EMSGSIZE;

	rv = psipe_dma_map_pages(dma, psipe_dev->pdev);
	if (rv < 0) {
		psipe_dma_unpin_pages(dma); /* there will be no irq */
		return rv;
	}

	pr_info("psipe_dma_map_pages - success\n");

	psipe_dma_write_maps(dma, bar);
	psipe_dma_doorbell_ring(bar);

	pr_info("psipe_ioctl_send - success\n");

	return (long)rv;
}

long psipe_ioctl_recv(struct psipe_dev *psipe_dev, struct psipe_dma *dma)
{
	struct psipe_bar *bar = &psipe_dev->bar;
	int rv = 0;

	psipe_dma_write_setup(dma, bar, PSIPE_MODE_PASSIVE, DMA_FROM_DEVICE);
	psipe_set_size_avail(dma, bar);

	rv = psipe_dma_map_pages(dma, psipe_dev->pdev);
	if (rv < 0) {
		psipe_dma_unpin_pages(dma); /* there will be no irq */
		return rv;
	}

	pr_info("psipe_dma_map_pages - success\n");

	psipe_dma_write_maps(dma, bar);
	psipe_dma_doorbell_ring(bar);

	pr_info("psipe_ioctl_recv - success\n");

	return (long)rv;
}

static long psipe_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct psipe_dev *psipe_dev = fp->private_data;
	struct psipe_op *op;
	psipe_handle_t id;
	long rv = -ENOTTY;

	switch(cmd) {
	case PSIPE_IOCTL_SEND:
	case PSIPE_IOCTL_RECV:
		op = psipe_ops_new(cmd, arg);
		id = psipe_ops_init(psipe_dev, op);
		rv = (long)id;
		break;
	case PSIPE_IOCTL_WAIT:
		id = (psipe_handle_t)arg;
		op = psipe_ops_get(&psipe_dev->ops, id);
		rv = psipe_ops_wait(op);
		break;
	case PSIPE_IOCTL_FLUSH:
		rv = psipe_ops_flush(psipe_dev);
		break;
	}

	return rv;
}

static const struct file_operations psipe_fops = {
	.owner = THIS_MODULE,
	.open = psipe_open,
	.unlocked_ioctl = psipe_ioctl,
};

static void psipe_dev_clean(struct psipe_dev *psipe_dev)
{
	psipe_dev->bar.start = 0;
	psipe_dev->bar.end = 0;
	psipe_dev->bar.len = 0;
	if (psipe_dev->bar.mmio)
		pci_iounmap(psipe_dev->pdev, psipe_dev->bar.mmio);
}

static int psipe_dev_init(struct psipe_dev *psipe_dev, struct pci_dev *pdev)
{
	const unsigned int bar = PSIPE_HW_BAR0;
	psipe_dev->pdev = pdev;

	/* Initialize struct with BAR 0 info */
	psipe_dev->bar.start = pci_resource_start(pdev, bar);
	psipe_dev->bar.end = pci_resource_end(pdev, bar);
	psipe_dev->bar.len = pci_resource_len(pdev, bar);
	psipe_dev->bar.mmio = pci_iomap(pdev, bar, psipe_dev->bar.len);
	if (!psipe_dev->bar.mmio) {
		dev_err(&pdev->dev, "cannot map BAR %u\n", bar);
		psipe_dev_clean(psipe_dev);
		return -ENOMEM;
	}
	pci_set_drvdata(pdev, psipe_dev);

	spin_lock_init(&psipe_dev->irq.lock);
	spin_lock_init(&psipe_dev->ops.lock);
	INIT_LIST_HEAD(&psipe_dev->ops.active);
	INIT_LIST_HEAD(&psipe_dev->ops.inactive);
	psipe_dev->ops.next_id = 0;

	return 0;
}

static struct psipe_dev *psipe_alloc_dev(void)
{
	return kmalloc(sizeof(struct psipe_dev), GFP_KERNEL);
}

static int psipe_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int err;
	int mem_bars;
	struct psipe_dev *psipe_dev;
	dev_t dev_num;
	struct device *dev;

	psipe_dev = psipe_alloc_dev();
	if (psipe_dev == NULL) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "psipe_alloc_device failed\n");
		goto err_psipe_alloc;
	}

	/* - Wake up the device if it was in suspended state,
	 * - Allocate I/O and memory regions of the device (if BIOS did not),
	 * - Allocate an IRQ (if BIOS did not).
	 */
	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "psipe_enable_device failed\n");
		goto err_pci_enable;
	}

	/* Enable DMA (set the bus master bit in the PCI_COMMAND register) */
	pci_set_master(pdev);

	/* Set the DMA mask */
	err = dma_set_mask_and_coherent(
		&pdev->dev, DMA_BIT_MASK(PSIPE_HW_DMA_ADDR_CAPABILITY));
	if (err) {
		dev_err(&pdev->dev, "dma_set_mask_and_coherent\n");
		goto err_dma_set_mask;
	}

	/* verify no other device is already using the same address resource */
	mem_bars = pci_select_bars(pdev, IORESOURCE_MEM);
	if ((mem_bars & (1 << PSIPE_HW_BAR0)) == 0) {
		dev_err(&pdev->dev, "pci_select_bars: bar0 not available\n");
		goto err_select_region;
	}
	err = pci_request_selected_regions(pdev, mem_bars,
					   "psipe_device_bars");
	if (err) {
		dev_err(&pdev->dev, "pci_request_region: bars being used\n");
		goto err_req_region;
	}

	err = psipe_dev_init(psipe_dev, pdev);
	if (err) {
		dev_err(&pdev->dev, "psipe_dev_init failed\n");
		goto err_dev_init;
	}

	/* Get device number range (base_minor = bar0 and count = nbr of bars)*/
	err = alloc_chrdev_region(&dev_num, PSIPE_HW_BAR0, PSIPE_HW_BAR_CNT,
			"psipe");
	if (err) {
		dev_err(&pdev->dev, "alloc_chrdev_region failed\n");
		goto err_alloc_chrdev;
	}

	psipe_dev->minor = MINOR(dev_num);
	psipe_dev->major = MAJOR(dev_num);

	cdev_init(&psipe_dev->cdev, &psipe_fops);

	err = cdev_add(&psipe_dev->cdev,
			MKDEV(psipe_dev->major, psipe_dev->minor),
			PSIPE_HW_BAR_CNT);
	if (err) {
		dev_err(&pdev->dev, "cdev_add failed\n");
		goto err_cdev_add;
	}

	dev = device_create(psipe_class, &pdev->dev,
			MKDEV(psipe_dev->major, psipe_dev->minor),
			psipe_dev, "d%xb%xd%xf%x_bar%u",
			pci_domain_nr(pdev->bus), pdev->bus->number,
			PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),
			PSIPE_HW_BAR0);
	if (IS_ERR(dev)) {
		err = PTR_ERR(dev);
		dev_err(&pdev->dev, "device_create failed\n");
		goto err_device_create;
	}

	err = psipe_irq_enable(psipe_dev);
	if (err) {
		dev_err(&pdev->dev, "psipe_irq_enable failed\n");
		goto err_irq_enable;
	}

	dev_info(&pdev->dev, "psipe probe - success\n");

	return 0;

err_irq_enable:
	device_destroy(psipe_class,
		       MKDEV(psipe_dev->major, psipe_dev->minor));

err_device_create:
	cdev_del(&psipe_dev->cdev);

err_cdev_add:
	unregister_chrdev_region(MKDEV(psipe_dev->major, psipe_dev->minor),
			PSIPE_HW_BAR_CNT);

err_alloc_chrdev:
	psipe_dev_clean(psipe_dev);

err_dev_init:
	pci_release_selected_regions(pdev,
			pci_select_bars(pdev, IORESOURCE_MEM));

err_req_region:
err_select_region:
	pci_clear_master(pdev);

err_dma_set_mask:
	pci_disable_device(pdev);

err_pci_enable:
	kfree(psipe_dev);

err_psipe_alloc:
	dev_err(&pdev->dev, "psipe_probe failed with error=%d\n", err);
	return err;
}

static void psipe_remove(struct pci_dev *pdev)
{
	struct psipe_dev *psipe_dev = pci_get_drvdata(pdev);

	device_destroy(psipe_class, MKDEV(psipe_dev->major,
				psipe_dev->minor));
	cdev_del(&psipe_dev->cdev);
	unregister_chrdev_region(MKDEV(psipe_dev->major, psipe_dev->minor),
			PSIPE_HW_BAR_CNT);
	psipe_dev_clean(psipe_dev);
	pci_clear_master(pdev);
	free_irq(psipe_dev->irq.irq_num, psipe_dev);
	pci_release_selected_regions(pdev, pci_select_bars(pdev,
				IORESOURCE_MEM));
	pci_disable_device(pdev);

	kfree(psipe_dev);

	dev_info(&pdev->dev, "psipe remove - success\n");
}

static struct pci_driver psipe_pci_driver = {
	.name = "psipe",
	.id_table = psipe_id_table,
	.probe = psipe_probe,
	.remove = psipe_remove,
};

static void psipe_module_exit(void)
{
	pci_unregister_driver(&psipe_pci_driver);
	class_destroy(psipe_class);
	pr_debug("psipe_module_exit finished successfully\n");
}

static char *psipe_devnode(const struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;
	return kasprintf(GFP_KERNEL, "psipe/%s", dev_name(dev));
}

static int __init psipe_module_init(void)
{
	int err;
	psipe_class = class_create("psipe");
	if (IS_ERR(psipe_class)) {
		pr_err("class_create error\n");
		err = PTR_ERR(psipe_class);
		return err;
	}
	psipe_class->devnode = psipe_devnode;
	err = pci_register_driver(&psipe_pci_driver);
	if (err) {
		pr_err("pci_register_driver error\n");
		goto err_pci;
	}
	pr_debug("psipe_module_init finished successfully\n");
	return 0;
err_pci:
	class_destroy(psipe_class);
	pr_err("psipe_module_init failed with err=%d\n", err);
	return err;
}

module_init(psipe_module_init);
module_exit(psipe_module_exit);
