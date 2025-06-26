/* psipe_irq.c - psipe virtual device IRQ operations
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com> (pciemu)
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "hw/psipe_hw.h"
#include "psipe_module.h"
#include <linux/pci.h>

static inline bool psipe_irq_check_and_ack(struct psipe_dev *psipe_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&psipe_dev->irq.lock, flags);
	bool active = (bool)ioread32(psipe_dev->irq.mmio_check_irq);
	if (active)
		iowrite32(1, psipe_dev->irq.mmio_ack_irq);
	spin_unlock_irqrestore(&psipe_dev->irq.lock, flags);

	return active;
}

/*
static inline void psipe_irq_ack(struct psipe_dev *psipe_dev)
{
	iowrite32(1, psipe_dev->irq.mmio_ack_irq);
}
*/

static irqreturn_t psipe_irq_handler(int irq, void *data)
{
	struct psipe_dev *psipe_dev = data;

	if (!psipe_irq_check_and_ack(psipe_dev))
		return IRQ_NONE;

	psipe_ops_next(psipe_dev);

	/*
	dev_dbg(&psipe_dev->pdev->dev, "irq_handler irq = %d dev = %d\n", irq,
		psipe_dev->major);
	*/

	//pr_info("irq handled (%d)\n", psipe_dev->major);

	return IRQ_HANDLED;
}

static int psipe_irq_enable_vectors(struct psipe_dev *psipe_dev)
{
	int irq_vecs_req, irq_vecs, err = 0;

	irq_vecs_req = min_t(int, pci_msi_vec_count(psipe_dev->pdev),
			     num_online_cpus() + 1);
	irq_vecs = pci_alloc_irq_vectors(psipe_dev->pdev, 1,
					 irq_vecs_req, PCI_IRQ_ALL_TYPES);

	if (irq_vecs < 0)
		return -ENOSPC;
	if (irq_vecs != irq_vecs_req) {
		err = -ENOSPC;
		goto err_clean_irqs;
	}

	psipe_dev->irq.irq_num = pci_irq_vector(psipe_dev->pdev,
			PSIPE_HW_IRQ_WORK_ENDED_VECTOR);
	if (psipe_dev->irq.irq_num < 0) {
		err = -EINVAL;
		goto err_clean_irqs;
	}

	/*
	dev_info(&psipe_dev->pdev->dev,
			"Probing device at %02x:%02x.%x with irq %d\n",
			pci_domain_nr(psipe_dev->pdev->bus),
			psipe_dev->pdev->devfn >> 3,
			psipe_dev->pdev->devfn & 0x7,
			psipe_dev->irq.irq_num);
	*/

	err = request_irq(psipe_dev->irq.irq_num, psipe_irq_handler,
			  IRQF_SHARED, "psipe_dma_fini", psipe_dev);
	if (err)
		goto err_clean_irqs;

	psipe_dev->irq.mmio_check_irq =
		psipe_dev->bar.mmio + PSIPE_HW_IRQ_WORK_ENDED_ADDR;
	psipe_dev->irq.mmio_ack_irq =
		psipe_dev->bar.mmio + PSIPE_HW_IRQ_WORK_ENDED_ACK_ADDR;
	return 0;

err_clean_irqs:
	pci_free_irq_vectors(psipe_dev->pdev);
	return err;
}

int psipe_irq_enable(struct psipe_dev *psipe_dev)
{
	if (!pci_msi_enabled())
		return -1;
	return psipe_irq_enable_vectors(psipe_dev);
}
