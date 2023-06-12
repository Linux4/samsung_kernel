#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include "esd_detect.h"

#define ESD_INTERVAL	(1000)
#define ESD_BOOT_INTERVAL (30000)

static void esd_dwork_func(struct work_struct *work)
{
	struct esd_det_info *info =
		container_of(work, struct esd_det_info, dwork.work);
    static int count = 0;

//	mutex_lock(info->lock);
	if (!info->is_active(info->pdata))
		goto out;

	if (gpio_get_value(info->gpio) == 1)
        count++;
    else
        count = 0;

    if(count >= 10)
    {
		info->recover(info->pdata);
        count = 0;
    }

	schedule_delayed_work(&info->dwork,
			msecs_to_jiffies(ESD_INTERVAL));
out:
//	mutex_unlock(info->lock);
	return;
}

static void esd_work_func(struct work_struct *work)
{
	struct esd_det_info *info = 
		container_of(work, struct esd_det_info, work);
	int esd_irq = gpio_to_irq(info->gpio);
	int retry = 3;

	printk("%s, gpio[%d] = %s\n", __func__,
			info->gpio, gpio_get_value(info->gpio) ? "high" : "low");

//	mutex_lock(info->lock);
	if (!info->is_active(info->pdata))
		goto out;

	info->recover(info->pdata);
	enable_irq(esd_irq);
out:
//	mutex_unlock(info->lock);

	return;
}

static irqreturn_t esd_irq_handler(int irq, void *dev_id)
{
	struct esd_det_info *info = (struct esd_det_info *)dev_id;
	int esd_irq = gpio_to_irq(info->gpio);
    static int count_irq = 0;

    if(info->state == ESD_DET_ON) {
		disable_irq_nosync(esd_irq);
		queue_work(info->wq, &info->work);
		printk("%s, esd detected %d\n", __func__, count_irq);
    }
	return IRQ_HANDLED;
}

int esd_det_disable(struct esd_det_info *info)
{
	printk("%s, disable mode : %d state : %d\n", __func__, info->mode, info->state);

	if (info->state == ESD_DET_NOT_INITIALIZED ||
		info->state == ESD_DET_OFF)
		return 0;

	if (info->mode == ESD_DET_INTERRUPT) {
		int esd_irq = gpio_to_irq(info->gpio);
		/*
		 * Block pending interrupt after irq disabled.
		 * if CONFIG_HARDIRQS_SW_RESEND is set,
		 * pending irq is re-send when irq is enabled.
		 */
		disable_irq(esd_irq);
		printk("%s, disable irq : %d\n", __func__, esd_irq);
	} else if (info->mode == ESD_DET_POLLING) {
		cancel_delayed_work(&info->dwork);
	}
	info->state = ESD_DET_OFF;

	return 0;
}

int esd_det_enable(struct esd_det_info *info)
{
	printk("%s, enable mode : %d state : %d, gpio : %d\n", __func__, info->mode, info->state, gpio_get_value(info->gpio));

    if (info->state == ESD_DET_NOT_INITIALIZED ||
		info->state == ESD_DET_ON)
		return 0;

	if (info->mode == ESD_DET_INTERRUPT) {
		int esd_irq = gpio_to_irq(info->gpio);

		printk("%s, enable irq : %d\n", __func__, esd_irq);
		enable_irq(esd_irq);
	} else if (info->mode == ESD_DET_POLLING) {
		schedule_delayed_work(&info->dwork,
				msecs_to_jiffies(ESD_INTERVAL));
	}
	info->state = ESD_DET_ON;

	return 0;
}

int esd_det_init(struct esd_det_info *info)
{
	if (info->mode == ESD_DET_POLLING) {
		char gpio_name[256];

		snprintf(gpio_name, sizeof(gpio_name), "esd_det_gpio_%d", info->gpio);
		if (gpio_request(info->gpio, gpio_name)) {
			printk(KERN_ERR "failed to request gpio %d\n", info->gpio);
			return -1;
		}
		gpio_direction_input(info->gpio);

		INIT_DELAYED_WORK(&info->dwork, esd_dwork_func); // ESD self protect
		schedule_delayed_work(&info->dwork,
				msecs_to_jiffies(ESD_BOOT_INTERVAL));
	} else if (info->mode == ESD_DET_INTERRUPT) {
		int esd_irq = gpio_to_irq(info->gpio);
		char gpio_name[256];

		snprintf(gpio_name, sizeof(gpio_name), "esd_det_gpio_%d", info->gpio);
		if (gpio_request(info->gpio, gpio_name)) {
			printk(KERN_ERR "\n failed to request gpio %d\n", info->gpio);
			return -1;
		}
		gpio_direction_input(info->gpio);
		gpio_free(info->gpio);

		if (info->level == ESD_DET_HIGH) {
			printk("%s, rising edge\n", __func__);
			if (request_irq(esd_irq, esd_irq_handler,
    					IRQF_TRIGGER_RISING,
    					gpio_name, info)) {
    			printk("%s, request_irq failed for esd\n", __func__);
    			return -1;
    		}
		} else {
			printk("%s, falling edge\n", __func__);
    		if (request_irq(esd_irq, esd_irq_handler,
    					IRQF_TRIGGER_FALLING,
    					gpio_name, info)) {
    			printk("%s, request_irq failed for esd\n", __func__);
    			return -1;
    		}
		}
		printk("%s, request irq success : %d\n", __func__, esd_irq);

		if (!(info->wq = create_singlethread_workqueue(gpio_name))) {
			printk("%s, fail to create workqueue!!\n", __func__);
			return -1;
		}
		INIT_WORK(&info->work, esd_work_func);
		printk("%s, create workqueue, %s\n", __func__, gpio_name);
	}
	info->state = ESD_DET_ON;

	return 0;
}
