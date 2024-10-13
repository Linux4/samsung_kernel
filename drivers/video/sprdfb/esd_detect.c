#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include "esd_detect.h"

static inline int esd_det_clear_irq(int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irq_data *idata = irq_desc_get_irq_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);

	/*
	 * Block pending interrupt after irq disabled.
	 * if CONFIG_HARDIRQS_SW_RESEND is set,
	 * pending irq is re-send when irq is enabled.
	 * irq_ack clears pending interrupt.
	 */
	if (chip->irq_ack)
		chip->irq_ack(idata);

	return 0;
}

static inline int esd_status(struct esd_det_info *esd)
{
	int gpio = esd->gpio, type = esd->type, status;

	if (unlikely(gpio < 0)) {
		pr_warn("%s: vgh pin unused\n", __func__);
		return -ENODEV;
	}

	if (type == ESD_TRIGGER_RISING || type == ESD_TRIGGER_HIGH)
		status = gpio_get_value(gpio) ?
			ESD_STATUS_NG : ESD_STATUS_OK;
	else if (type == ESD_TRIGGER_FALLING || type == ESD_TRIGGER_LOW)
		status = !gpio_get_value(gpio) ?
			ESD_STATUS_NG : ESD_STATUS_OK;
	else
		status = ESD_STATUS_NG;

	return status;
}

static void esd_work(struct work_struct *work)
{
	struct esd_det_info *esd = (struct esd_det_info *)
		container_of(work, struct esd_det_info, work.work);
	int retry = 5;

	if (esd->type == ESD_POLLING) {
		/* Todo : ESD_POLLING */
		goto out;
	}

	do {
		if (esd->recover)
			esd->recover(esd->pdata);
	} while ((esd_status(esd) != ESD_STATUS_OK) && --retry);

	pr_info("%s, %s to recover\n", __func__,
			retry ? "succeeded" : "failed");
out:
	esd_det_enable(esd);

	return;
}

static irqreturn_t esd_irq_handler(int irq, void *dev_id)
{
	struct esd_det_info *esd = (struct esd_det_info *)dev_id;

	disable_irq_nosync(irq);
	queue_delayed_work(esd->wq, &esd->work, 0);
	pr_info("%s, esd detected!!\n", __func__);

	return IRQ_HANDLED;
}

int esd_det_disable(struct esd_det_info *esd)
{
	if (esd->type == ESD_POLLING)
		cancel_delayed_work_sync(&esd->work);
	else
		disable_irq(gpio_to_irq(esd->gpio));

	return 0;
}

int esd_det_enable(struct esd_det_info *esd)
{
	if (esd->type == ESD_POLLING)
		queue_delayed_work(esd->wq, &esd->work, HZ);
	else {
		int irq = gpio_to_irq(esd->gpio);
		esd_det_clear_irq(irq);
		enable_irq(irq);
	}

	return 0;
}

int esd_det_init(struct esd_det_info *esd)
{
	int ret, irq = gpio_to_irq(esd->gpio);

	if (gpio_request(esd->gpio, "dsi-esd")) {
		pr_err("%s, failed to request gpio %d\n",
				__func__, esd->gpio);
		return -1;
	}
	gpio_direction_input(esd->gpio);

	INIT_DELAYED_WORK(&esd->work, esd_work);
	esd->wq = alloc_workqueue("dsi-esd", WQ_HIGHPRI
				| WQ_UNBOUND | WQ_MEM_RECLAIM, 1);

	irq_set_status_flags(irq, IRQ_NOAUTOEN);
	ret = request_irq(irq, esd_irq_handler, esd->type,
			"dsi-esd", esd);
	if (unlikely(ret)) {
		pr_err("%s, request_irq (%d) failed(%d)\n",
				__func__, irq, ret);
		return ret;
	}
	pr_info("%s: type:%s, gpio:%d\n", __func__,
			esd->type != ESD_POLLING ?
			"intr" : "poll", esd->gpio);

	return 0;
}
