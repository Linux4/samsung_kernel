/*
 * drivers/input/misc/flip_cover.c
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/irqdesc.h>
#include <linux/wakelock.h>
#include <linux/input/flip_cover.h>

#ifdef CONFIG_SEC_SYSFS
static ssize_t hall_detect_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct device *pdev = dev->parent;
	struct flip_cover_drvdata *ddata = dev_get_drvdata(pdev);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		printk(KERN_DEBUG "[flip_cover] %s %s\n", __func__,
			ddata->flip_cover_open ? "Open" : "Close");
#endif

	if (ddata->flip_cover_open)
		sprintf(buf, "OPEN");
	else
		sprintf(buf, "CLOSE");

	return strlen(buf);
}

static DEVICE_ATTR(hall_detect, 0664, hall_detect_show, NULL);

static struct attribute *flip_cover_attrs[] = {
	&dev_attr_hall_detect.attr,
	NULL,
};

static struct attribute_group flip_cover_attr_group = {
	.attrs = flip_cover_attrs,
};
#endif

static void flip_cover_report_event(struct input_dev *input)
{
	struct flip_cover_drvdata *ddata = input_get_drvdata(input);
	int state = gpio_get_value_cansleep(ddata->pdata->gpio);

	ddata->flip_cover_open =  state ^ ddata->pdata->active_low;

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	printk(KERN_DEBUG "[flip_cover] %s\n",
		ddata->flip_cover_open ? "Open" : "Close");
#endif

#if 0
	if (state == ddata->prev_level) {
#else
	if (ddata->input_enabled) {
#endif
		input_report_switch(input,
			ddata->pdata->code, ddata->flip_cover_open);
		input_sync(input);
	}
}

static void flip_cover_debounce(struct work_struct *work)
{
	struct flip_cover_drvdata *ddata = container_of(work,
			struct flip_cover_drvdata, dwork.work);

	flip_cover_report_event(ddata->input);
}

static irqreturn_t flip_cover_isr(int irq, void *dev_id)
{
	struct flip_cover_drvdata *ddata = dev_id;

	ddata->prev_level = gpio_get_value_cansleep(ddata->pdata->gpio);

	if (ddata->prev_level ^ ddata->pdata->active_low)
		wake_lock_timeout(&ddata->wlock, HZ / 10);

	schedule_delayed_work(&ddata->dwork, ddata->pdata->debounce);

	return IRQ_HANDLED;
}

static int flip_cover_open(struct input_dev *input)
{
	struct flip_cover_drvdata *ddata = input_get_drvdata(input);

	ddata->input_enabled = true;
	schedule_delayed_work(&ddata->dwork, ddata->pdata->debounce);

	return 0;
}

static void flip_cover_close(struct input_dev *input)
{
	struct flip_cover_drvdata *ddata = input_get_drvdata(input);

	ddata->input_enabled = false;
}

static int __devinit flip_cover_probe(struct platform_device *pdev)
{
	struct flip_cover_pdata *pdata = pdev->dev.platform_data;
	struct flip_cover_drvdata *ddata;
	struct input_dev *input;
	int gpio = 0, irq = 0, ret = 0;
	u64 irqflags = 0;

	if (!pdata) {
		printk(KERN_ERR "[flip_cover] no pdata\n");
		ret = -ENODEV;
		goto err_pdata;
	} else
		gpio = pdata->gpio;

	ddata = kzalloc(sizeof(struct flip_cover_drvdata), GFP_KERNEL);
	if (!ddata) {
		printk(KERN_ERR "[flip_cover] failed to alloc ddata.\n");
		goto err_free_ddata;
	}

	input = input_allocate_device();
	if (!input) {
		printk(KERN_ERR "[flip_cover] failed to allocate input device.\n");
		ret = -ENOMEM;
		goto err_free_input_dev;
	}

	ddata->pdata = pdata;
	ddata->input = input;
	ddata->input_enabled = false;

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	mutex_init(&ddata->lock);
	INIT_DELAYED_WORK(&ddata->dwork, flip_cover_debounce);
	wake_lock_init(&ddata->wlock, WAKE_LOCK_SUSPEND,
		"flip wake lock");

	input->name = pdev->name;
	input->phys = "flip_cover";
	input->dev.parent = &pdev->dev;

	input->open = flip_cover_open;
	input->close = flip_cover_close;
	input->id.bustype = BUS_HOST;

	input_set_capability(input, EV_SW, SW_FLIP);

	ret = input_register_device(input);
	if (ret) {
		printk(KERN_ERR
			"[flip_cover] failed to register input device, ret: %d\n",
			ret);
		goto err_reg_input;
	}

	if (pdata->gpio_init)
		pdata->gpio_init();
	else {
		ret = gpio_request(gpio, pdata->name);
		if (ret < 0) {
			printk(KERN_ERR "[flip_cover] failed to request GPIO %d, ret %d\n",
				gpio, ret);
			goto err_gpio;
		}

		ret = gpio_direction_input(gpio);
		if (ret < 0) {
			printk(KERN_ERR
				"[flip_cover] failed to configure direction for GPIO %d, ret %d\n",
				gpio, ret);
			goto err_irq;
		}
	}

	/* for checking the init state */
	flip_cover_report_event(ddata->input);

	irq = gpio_to_irq(gpio);
	if (irq < 0) {
		ret = irq;
		printk(KERN_ERR
			"[flip_cover] failed to get irq number for GPIO %s, ret %d\n",
			pdata->name, ret);
		goto err_irq;
	}

	irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	if (pdata->wakeup)
		irqflags |= IRQF_NO_SUSPEND;

	ret = request_any_context_irq(irq, flip_cover_isr,
			irqflags, pdata->name, ddata);
	if (ret < 0) {
		printk(KERN_ERR
			"[flip_cover] Unable to claim irq %d; ret %d\n",
			irq, ret);
		goto err_irq;
	}

	/* check the flip stste */
	schedule_work(&ddata->dwork.work);

#ifdef CONFIG_SEC_SYSFS
	ddata->dev = sec_dev_get_by_name(SEC_KEY_PATH);
	if (!ddata->dev)
		printk(KERN_ERR
			"[flip_cover] failed to find %s path\n",
			SEC_KEY_PATH);
	else {
		ddata->dev->parent = &pdev->dev;
		ret = sysfs_create_group(&ddata->dev->kobj,
				&flip_cover_attr_group);
		if (ret) {
			printk(KERN_ERR
				"[flip_cover] failed to create the test sysfs: %d\n",
				ret);
			goto err_sysfs;
		}
	}
#endif

	device_init_wakeup(&pdev->dev, pdata->wakeup);

	return 0;

#ifdef CONFIG_SEC_SYSFS
err_sysfs:
	sysfs_remove_group(&ddata->dev->kobj, &flip_cover_attr_group);
	free_irq(irq, &ddata);
#endif
err_irq:
	gpio_free(gpio);
err_gpio:
err_reg_input:
	platform_set_drvdata(pdev, NULL);
	wake_lock_destroy(&ddata->wlock);
	cancel_delayed_work_sync(&ddata->dwork);
	input_free_device(input);
err_free_input_dev:
	kfree(ddata);
err_free_ddata:
err_pdata:
	return ret;
}

static int __devexit flip_cover_remove(struct platform_device *pdev)
{
	struct flip_cover_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int irq = gpio_to_irq(ddata->pdata->gpio);

	sysfs_remove_group(&ddata->dev->kobj, &flip_cover_attr_group);
	device_init_wakeup(&pdev->dev, 0);
	free_irq(irq, &ddata);
	cancel_delayed_work_sync(&ddata->dwork);
	gpio_free(ddata->pdata->gpio);

	input_unregister_device(input);

	return 0;
}

#ifdef CONFIG_PM
static int flip_cover_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct flip_cover_drvdata *ddata = platform_get_drvdata(pdev);
	int irq = gpio_to_irq(ddata->pdata->gpio);

	enable_irq_wake(irq);
	return 0;
}

static int flip_cover_resume(struct platform_device *pdev)
{
	struct flip_cover_drvdata *ddata = platform_get_drvdata(pdev);
	int irq = gpio_to_irq(ddata->pdata->gpio);

	disable_irq_wake(irq);
#if 0
	schedule_delayed_work(&ddata->dwork, ddata->pdata->debounce);
#else
	flip_cover_report_event(ddata->input);
#endif
	return 0;
}
#endif	/* CONFIG_PM */

static struct platform_driver flip_cover_device_driver = {
	.probe		= flip_cover_probe,
	.remove		= __devexit_p(flip_cover_remove),
#ifdef CONFIG_PM
	.suspend		= flip_cover_suspend,
	.resume		= flip_cover_resume,
#endif	/* CONFIG_PM */
	.driver		= {
		.name	= "flip_cover",
		.owner	= THIS_MODULE,
	}
};

static int __init flip_cover_init(void)
{
	return platform_driver_register(&flip_cover_device_driver);
}

static void __exit flip_cover_exit(void)
{
	platform_driver_unregister(&flip_cover_device_driver);
}

late_initcall(flip_cover_init);
module_exit(flip_cover_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Junki Min <junki671.min@samsung.com>");
MODULE_DESCRIPTION("flip cover driver for sec product");
