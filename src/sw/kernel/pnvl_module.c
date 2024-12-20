/* pnvl_module.c - Kernel module to control the pnvl virtual device
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "hw/pnvl_hw.h"
#include "pnvl_module.h"
#include "sw/module/pnvl_ioctl.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Kernel module to control the pnvl virtual device");
MODULE_AUTHOR("David Cañadas López <dcanadas@bsc.es>");

static struct class *pnvl_class;

static struct pci_device_id pnvl_id_table[] = {
	{ PCI_DEVICE(PNVL_HW_VENDOR_ID, PNVL_HW_DEVICE_ID) },
	{},
};

MODULE_DEVICE_TABLE(pci, pnvl_id_table);

static int pnvl_open(struct inode *inode, struct file *fp)
{
	unsigned int bar = iminor(inode);
	struct pnvl_dev *pnvl_dev = container_of(inode->i_cdev,
						struct pnvl_dev, cdev);

	if (bar != 0)
		return -ENXIO;
	if (pnvl_dev->bar.len == 0)
		return -EIO;
	fp->private_data = pnvl_dev;

	return 0;
}

static int pnvl_ioctl_work(struct pnvl_dev *pnvl_dev)
{
	int ret;

	if (pnvl_dev->running)
		return -EBUSY;

	ret = pnvl_dma_setup(pnvl_dev, PNVL_MODE_ACTIVE);
	if (ret < 0)
		return ret;
	pnvl_dma_doorbell_ring(pnvl_dev);

	pnvl_dev->running = true;

	return 0;
}

static int pnvl_ioctl_wait(struct pnvl_dev *pnvl_dev)
{
	int ret;

	if (!pnvl_dev->running) {
		ret = pnvl_dma_setup(pnvl_dev, PNVL_MODE_PASSIVE);
		if (ret < 0)
			return ret;
	}
	pnvl_dma_wait(pnvl_dev);

	return 0;
}

static long pnvl_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct pnvl_dev *pnvl_dev = fp->private_data;
	struct pnvl_data __user *udata = (struct pnvl_data *)arg;
	unsigned long len;

	if (!udata && !pnvl_dev->data.addr)
		return -EINVAL;
	len = copy_from_user(&pnvl_dev->data, udata, sizeof(pnvl_dev->data));
	if (len < sizeof(pnvl_dev->data))
		return -EINVAL;

	switch(cmd) {
	case PNVL_IOCTL_WORK:
		return pnvl_ioctl_work(pnvl_dev);
	case PNVL_IOCTL_WAIT:
		return pnvl_ioctl_wait(pnvl_dev);
	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations pnvl_fops = {
	.owner = THIS_MODULE,
	.open = pnvl_open,
	.unlocked_ioctl = pnvl_ioctl,
};

static void pnvl_dev_clean(struct pnvl_dev *pnvl_dev)
{
	pnvl_dev->bar.start = 0;
	pnvl_dev->bar.end = 0;
	pnvl_dev->bar.len = 0;
	if (pnvl_dev->bar.mmio)
		pci_iounmap(pnvl_dev->pdev, pnvl_dev->bar.mmio);
}

static int pnvl_dev_init(struct pnvl_dev *pnvl_dev, struct pci_dev *pdev)
{
	const unsigned int bar = PNVL_HW_BAR0;
	pnvl_dev->pdev = pdev;

	/* Initialize struct with BAR 0 info */
	pnvl_dev->bar.start = pci_resource_start(pdev, bar);
	pnvl_dev->bar.end = pci_resource_end(pdev, bar);
	pnvl_dev->bar.len = pci_resource_len(pdev, bar);
	pnvl_dev->bar.mmio = pci_iomap(pdev, bar, pnvl_dev->bar.len);
	if (!pnvl_dev->bar.mmio) {
		dev_err(&pdev->dev, "cannot map BAR %u\n", bar);
		pnvl_dev_clean(pnvl_dev);
		return -ENOMEM;
	}
	pci_set_drvdata(pdev, pnvl_dev);

	pnvl_dev->wq_flag = 0;
	init_waitqueue_head(&pnvl_dev->wq);

	pnvl_dev->running = false;

	return 0;
}

static struct pnvl_dev *pnvl_alloc_dev(void)
{
	return kmalloc(sizeof(struct pnvl_dev), GFP_KERNEL);
}

static int pnvl_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int err;
	int mem_bars;
	struct pnvl_dev *pnvl_dev;
	dev_t dev_num;
	struct device *dev;

	pnvl_dev = pnvl_alloc_dev();
	if (pnvl_dev == NULL) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "pnvl_alloc_device failed\n");
		goto err_pnvl_alloc;
	}

	/* - Wake up the device if it was in suspended state,
	 * - Allocate I/O and memory regions of the device (if BIOS did not),
	 * - Allocate an IRQ (if BIOS did not).
	 */
	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "pnvl_enable_device failed\n");
		goto err_pci_enable;
	}

	/* Enable DMA (set the bus master bit in the PCI_COMMAND register) */
	pci_set_master(pdev);

	/* Set the DMA mask */
	err = dma_set_mask_and_coherent(
		&pdev->dev, DMA_BIT_MASK(PNVL_HW_DMA_ADDR_CAPABILITY));
	if (err) {
		dev_err(&pdev->dev, "dma_set_mask_and_coherent\n");
		goto err_dma_set_mask;
	}

	/* verify no other device is already using the same address resource */
	mem_bars = pci_select_bars(pdev, IORESOURCE_MEM);
	if ((mem_bars & (1 << PNVL_HW_BAR0)) == 0) {
		dev_err(&pdev->dev, "pci_select_bars: bar0 not available\n");
		goto err_select_region;
	}
	err = pci_request_selected_regions(pdev, mem_bars,
					   "pnvl_device_bars");
	if (err) {
		dev_err(&pdev->dev, "pci_request_region: bars being used\n");
		goto err_req_region;
	}

	err = pnvl_dev_init(pnvl_dev, pdev);
	if (err) {
		dev_err(&pdev->dev, "pnvl_dev_init failed\n");
		goto err_dev_init;
	}

	/* Get device number range (base_minor = bar0 and count = nbr of bars)*/
	err = alloc_chrdev_region(&dev_num, PNVL_HW_BAR0, PNVL_HW_BAR_CNT,
			"pnvl");
	if (err) {
		dev_err(&pdev->dev, "alloc_chrdev_region failed\n");
		goto err_alloc_chrdev;
	}

	pnvl_dev->minor = MINOR(dev_num);
	pnvl_dev->major = MAJOR(dev_num);

	cdev_init(&pnvl_dev->cdev, &pnvl_fops);

	err = cdev_add(&pnvl_dev->cdev, MKDEV(pnvl_dev->major,
				pnvl_dev->minor), PNVL_HW_BAR_CNT);
	if (err) {
		dev_err(&pdev->dev, "cdev_add failed\n");
		goto err_cdev_add;
	}

	dev = device_create(pnvl_class, &pdev->dev,
			MKDEV(pnvl_dev->major, pnvl_dev->minor),
			pnvl_dev, "d%xb%xd%xf%x_bar%u",
			pci_domain_nr(pdev->bus), pdev->bus->number,
			PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),
			PNVL_HW_BAR0);
	if (IS_ERR(dev)) {
		err = PTR_ERR(dev);
		dev_err(&pdev->dev, "device_create failed\n");
		goto err_device_create;
	}

	err = pnvl_irq_enable(pnvl_dev);
	if (err) {
		dev_err(&pdev->dev, "pnvl_irq_enable failed\n");
		goto err_irq_enable;
	}

	memset(&pnvl_dev->data, 0, sizeof(pnvl_dev->data));

	dev_info(&pdev->dev, "pnvl probe - success\n");

	return 0;

err_irq_enable:
	device_destroy(pnvl_class,
		       MKDEV(pnvl_dev->major, pnvl_dev->minor));

err_device_create:
	cdev_del(&pnvl_dev->cdev);

err_cdev_add:
	unregister_chrdev_region(MKDEV(pnvl_dev->major, pnvl_dev->minor),
			PNVL_HW_BAR_CNT);

err_alloc_chrdev:
	pnvl_dev_clean(pnvl_dev);

err_dev_init:
	pci_release_selected_regions(pdev,
			pci_select_bars(pdev, IORESOURCE_MEM));

err_req_region:
err_select_region:
	pci_clear_master(pdev);

err_dma_set_mask:
	pci_disable_device(pdev);

err_pci_enable:
	kfree(pnvl_dev);

err_pnvl_alloc:
	dev_err(&pdev->dev, "pnvl_probe failed with error=%d\n", err);
	return err;
}

static void pnvl_remove(struct pci_dev *pdev)
{
	struct pnvl_dev *pnvl_dev = pci_get_drvdata(pdev);

	device_destroy(pnvl_class, MKDEV(pnvl_dev->major,
				pnvl_dev->minor));
	cdev_del(&pnvl_dev->cdev);
	unregister_chrdev_region(MKDEV(pnvl_dev->major, pnvl_dev->minor),
			PNVL_HW_BAR_CNT);
	pnvl_dev_clean(pnvl_dev);
	pci_clear_master(pdev);
	free_irq(pnvl_dev->irq.irq_num, pnvl_dev);
	pci_release_selected_regions(pdev, pci_select_bars(pdev,
				IORESOURCE_MEM));
	pci_disable_device(pdev);
	kfree(pnvl_dev);
	dev_info(&pdev->dev, "pnvl remove - success\n");
}

static struct pci_driver pnvl_pci_driver = {
	.name = "pnvl",
	.id_table = pnvl_id_table,
	.probe = pnvl_probe,
	.remove = pnvl_remove,
};

static void pnvl_module_exit(void)
{
	pci_unregister_driver(&pnvl_pci_driver);
	class_destroy(pnvl_class);
	pr_debug("pnvl_module_exit finished successfully\n");
}

static char *pnvl_devnode(const struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;
	return kasprintf(GFP_KERNEL, "pnvl/%s", dev_name(dev));
}

static int __init pnvl_module_init(void)
{
	int err;
	pnvl_class = class_create("pnvl");
	if (IS_ERR(pnvl_class)) {
		pr_err("class_create error\n");
		err = PTR_ERR(pnvl_class);
		return err;
	}
	pnvl_class->devnode = pnvl_devnode;
	err = pci_register_driver(&pnvl_pci_driver);
	if (err) {
		pr_err("pci_register_driver error\n");
		goto err_pci;
	}
	pr_debug("pnvl_module_init finished successfully\n");
	return 0;
err_pci:
	class_destroy(pnvl_class);
	pr_err("pnvl_module_init failed with err=%d\n", err);
	return err;
}

module_init(pnvl_module_init);
module_exit(pnvl_module_exit);
