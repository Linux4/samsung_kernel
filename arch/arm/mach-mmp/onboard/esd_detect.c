#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include "esd_detect.h"

#define ESD_INTERVAL	(5000)
#define ESD_BOOT_INTERVAL (30000)

static void esd_dwork_func(struct work_struct *work)
{
	struct esd_det_info *info =
		container_of(work, struct esd_det_info, dwork);

	mutex_lock(info->lock);
	if (!info->is_active())
		goto out;

	if (info->is_normal) {
		if(info->is_normal(info->pdata) == ESD_DETECTED) {
			pr_info("%s, esd detected!!\n", info->name);
			info->recover(info->pdata);
		}
	}
#if 0 
    else {
		if (info->level == gpio_get_value(info->gpio))
			info->recover(info->pdata);
	}
#endif    

	schedule_delayed_work(&info->dwork,
			msecs_to_jiffies(ESD_INTERVAL));
out:
	mutex_unlock(info->lock);
	return;
}

static void esd_work_func(struct work_struct *work)
{
	struct esd_det_info *info = 
		container_of(work, struct esd_det_info, work);
	int esd_irq = gpio_to_irq(info->gpio);
	int retry = 3;

	pr_info("%s, gpio[%d] = %s\n", __func__,
			info->gpio, gpio_get_value(info->gpio) ? "high" : "low");

	mutex_lock(info->lock);
	if (!info->is_active())
		goto out;

	do {
		info->recover(info->pdata);
		msleep(ESD_INTERVAL);
	} while(--retry && info->level == gpio_get_value(info->gpio));

	enable_irq(esd_irq);
	irq_set_irq_type(esd_irq, IRQF_TRIGGER_RISING);
out:
	mutex_unlock(info->lock);

	return;
}

static irqreturn_t esd_irq_handler(int irq, void *dev_id)
{
	struct esd_det_info *info = (struct esd_det_info *)dev_id;
	int esd_irq = gpio_to_irq(info->gpio);

	irq_set_irq_type(esd_irq, IRQF_TRIGGER_NONE);
	disable_irq_nosync(esd_irq);
	queue_work(info->wq, &info->work);
	pr_info("%s, esd detected\n", __func__);

	return IRQ_HANDLED;
}

int esd_det_disable(struct esd_det_info *info)
{
	if (info->state == ESD_DET_NOT_INITIALIZED ||
		info->state == ESD_DET_OFF)
		return;

	if (info->mode == ESD_DET_INTERRUPT) {
		int esd_irq = gpio_to_irq(info->gpio);
		/*
		 * Block pending interrupt after irq disabled.
		 * if CONFIG_HARDIRQS_SW_RESEND is set,
		 * pending irq is re-send when irq is enabled.
		 */
		irq_set_irq_type(esd_irq, IRQF_TRIGGER_NONE);
		disable_irq(esd_irq);
		pr_info("%s, disable irq : %d\n", __func__, esd_irq);
	} else if (info->mode == ESD_DET_POLLING) {
		cancel_delayed_work(&info->dwork);
	}
	info->state = ESD_DET_OFF;

	return 0;
}

int esd_det_enable(struct esd_det_info *info)
{
	if (info->state == ESD_DET_NOT_INITIALIZED ||
		info->state == ESD_DET_ON)
		return;

	if (info->mode == ESD_DET_INTERRUPT) {
		int esd_irq = gpio_to_irq(info->gpio);

		pr_info("%s, enable irq : %d\n", __func__, esd_irq);
		enable_irq(esd_irq);
		if (info->level == ESD_DET_HIGH)
			irq_set_irq_type(esd_irq, IRQF_TRIGGER_RISING);
		else
			irq_set_irq_type(esd_irq, IRQF_TRIGGER_FALLING);
	} else if (info->mode == ESD_DET_POLLING) {
		schedule_delayed_work(&info->dwork,
				msecs_to_jiffies(ESD_INTERVAL));
	}
	info->state = ESD_DET_ON;

	return 0;
}

int esd_det_init(struct esd_det_info *info)
{
	int esd_irq = gpio_to_irq(info->gpio);
	char *gpio_name[256];

	snprintf(gpio_name, sizeof(gpio_name), "esd_det_gpio_%d", info->gpio);
	if (gpio_request(info->gpio, gpio_name)) {
		printk(KERN_ERR "failed to request gpio %d\n", info->gpio);
		return;
	}
	gpio_direction_input(info->gpio);
	gpio_free(info->gpio);

	if (info->mode == ESD_DET_POLLING) {
		INIT_DELAYED_WORK(&info->dwork, esd_dwork_func); // ESD self protect
		schedule_delayed_work(&info->dwork,
				msecs_to_jiffies(ESD_BOOT_INTERVAL));
	} else if (info->mode == ESD_DET_INTERRUPT) {
		if (request_irq(esd_irq, esd_irq_handler,
					IRQF_TRIGGER_NONE,
					gpio_name, info)) {
			pr_err("%s, request_irq failed for esd\n", __func__);
			return -1;
		}
		pr_info("%s, request irq success : %d\n", __func__, esd_irq);
		if (info->level == ESD_DET_HIGH) {
			pr_info("%s, rising edge\n", __func__);
			irq_set_irq_type(esd_irq, IRQF_TRIGGER_RISING);
		} else {
			pr_info("%s, falling edge\n", __func__);
			irq_set_irq_type(esd_irq, IRQF_TRIGGER_FALLING);
		}

		if (!(info->wq = create_singlethread_workqueue(gpio_name))) {
			pr_err("%s, fail to create workqueue!!\n", __func__);
			return -1;
		}
		INIT_WORK(&info->work, esd_work_func);
	}
	info->state = ESD_DET_ON;

	return 0;
}
