#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
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
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>

extern struct device *sec_key;

struct sliding_up_hall_drvdata {
	struct input_dev *input;
	int gpio_sliding_up_hall;
#if defined(EMULATE_sliding_up_hall_IC)	
	int gpio_sliding_up_hall_key1;
	int gpio_sliding_up_hall_key2;
	int emulated_sliding_up_hall_status;
#endif	
	int irq_sliding_up_hall;
	bool sliding_up_hall;
	struct work_struct work;
	struct delayed_work sliding_up_hall_dwork;
	struct wake_lock sliding_up_hall_wake_lock;
};

static bool sliding_up_hall_status = 1;

static ssize_t sliding_up_hall_detect_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (sliding_up_hall_status)
		sprintf(buf, "OPEN\n");
	else
		sprintf(buf, "CLOSE\n");

	return strlen(buf);
}
static DEVICE_ATTR(sliding_up_hall_detect, 0444, sliding_up_hall_detect_show, NULL);

#ifdef CONFIG_SEC_FACTORY
static void sliding_up_hall_work(struct work_struct *work)
{
	bool first, second;
	struct sliding_up_hall_drvdata *ddata =
		container_of(work, struct sliding_up_hall_drvdata,
				sliding_up_hall_dwork.work);

	first = gpio_get_value(ddata->gpio_sliding_up_hall);

	pr_info("keys:%s #1 : %d\n", __func__, first);

	msleep(50);

	second = gpio_get_value(ddata->gpio_sliding_up_hall);

	pr_info("keys:%s #2 : %d\n", __func__, second);

	if (first == second) {
		sliding_up_hall_status = first;
		input_report_switch(ddata->input, SW_SLIDINGUPHALL, sliding_up_hall_status);
		input_sync(ddata->input);
	}
}
#else
static void sliding_up_hall_work(struct work_struct *work)
{
	bool first;
	struct sliding_up_hall_drvdata *ddata =
		container_of(work, struct sliding_up_hall_drvdata,
				sliding_up_hall_dwork.work);

#if !defined(EMULATE_sliding_up_hall_IC)
	ddata->sliding_up_hall = gpio_get_value(ddata->gpio_sliding_up_hall);
	first = gpio_get_value(ddata->gpio_sliding_up_hall);

	pr_info("keys:%s #1 : %d\n", __func__, first);

	sliding_up_hall_status = first;
	input_report_switch(ddata->input,
			SW_SLIDINGUPHALL, ddata->sliding_up_hall);
	input_sync(ddata->input);
#else
	ddata->sliding_up_hall = !gpio_get_value(ddata->gpio_sliding_up_hall_key1) & !gpio_get_value(ddata->gpio_sliding_up_hall_key2);
	first = !gpio_get_value(ddata->gpio_sliding_up_hall_key1) & !gpio_get_value(ddata->gpio_sliding_up_hall_key2);	
	pr_info("keys:%s #1 key1=%d,key2=%d : %d\n", __func__, gpio_get_value(ddata->gpio_sliding_up_hall_key1),gpio_get_value(ddata->gpio_sliding_up_hall_key2),first);	
	if(sliding_up_hall_status != first) {
		if(first)
		{		
			ddata->emulated_sliding_up_hall_status = !ddata->emulated_sliding_up_hall_status;
			pr_info("keys:%s #1 : %d , %d\n", __func__, first, ddata->emulated_sliding_up_hall_status);	
			input_report_switch(ddata->input,
					SW_SLIDINGUPHALL, ddata->emulated_sliding_up_hall_status);
			input_sync(ddata->input);
		}
		sliding_up_hall_status = first;
	}
	schedule_delayed_work(&ddata->sliding_up_hall_dwork, msecs_to_jiffies(1000));
#endif

}
#endif

static void __sliding_up_hall_detect(struct sliding_up_hall_drvdata *ddata, bool sliding_up_hall_status)
{
	cancel_delayed_work_sync(&ddata->sliding_up_hall_dwork);
#ifdef CONFIG_SEC_FACTORY
	schedule_delayed_work(&ddata->sliding_up_hall_dwork, HZ / 20);
#else
	if (sliding_up_hall_status)	{
		wake_lock_timeout(&ddata->sliding_up_hall_wake_lock, HZ * 5 / 100); /* 50ms */
		schedule_delayed_work(&ddata->sliding_up_hall_dwork, HZ * 1 / 100); /* 10ms */
	} else {
		wake_unlock(&ddata->sliding_up_hall_wake_lock);
		schedule_delayed_work(&ddata->sliding_up_hall_dwork, 0);
	}
#endif
}

static irqreturn_t sliding_up_hall_detect(int irq, void *dev_id)
{
	bool sliding_up_hall_status;
	struct sliding_up_hall_drvdata *ddata = dev_id;

#if !defined(EMULATE_sliding_up_hall_IC)
	sliding_up_hall_status = gpio_get_value(ddata->gpio_sliding_up_hall);
#else
	sliding_up_hall_status = gpio_get_value(ddata->gpio_sliding_up_hall_key1) & gpio_get_value(ddata->gpio_sliding_up_hall_key2);
#endif


	pr_info("keys:%s sliding_up_hall_status : %d\n",
		 __func__, sliding_up_hall_status);


	__sliding_up_hall_detect(ddata, sliding_up_hall_status);

	return IRQ_HANDLED;
}

static int sliding_up_hall_open(struct input_dev *input)
{
	struct sliding_up_hall_drvdata *ddata = input_get_drvdata(input);
	/* update the current status */
	schedule_delayed_work(&ddata->sliding_up_hall_dwork, HZ / 2);
	/* Report current state of buttons that are connected to GPIOs */
	input_sync(input);

	return 0;
}

static void sliding_up_hall_close(struct input_dev *input)
{
}


static void init_sliding_up_hall_ic_irq(struct input_dev *input)
{
	struct sliding_up_hall_drvdata *ddata = input_get_drvdata(input);

	int ret = 0;
	int irq = ddata->irq_sliding_up_hall;

#if !defined(EMULATE_sliding_up_hall_IC)
	sliding_up_hall_status = gpio_get_value(ddata->gpio_sliding_up_hall);
#else
	sliding_up_hall_status = 0;
#endif

	INIT_DELAYED_WORK(&ddata->sliding_up_hall_dwork, sliding_up_hall_work);

	ret =
		request_threaded_irq(
		irq, NULL,
		sliding_up_hall_detect,
		IRQF_TRIGGER_RISING |
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"sliding_up_hall", ddata);
	if (ret < 0) {
		pr_info("keys: failed to request sliding_up_hall irq %d gpio %d\n",
			irq, ddata->gpio_sliding_up_hall);
	} else {
		pr_info("%s : success\n", __func__);
	}
}

#ifdef CONFIG_OF
static int of_sliding_up_hall_data_parsing_dt(struct device *dev, struct sliding_up_hall_drvdata *ddata)
{
	struct device_node *np = dev->of_node;
	int gpio;
	enum of_gpio_flags flags;

	gpio = of_get_named_gpio_flags(np, "sliding_up_hall,gpio_sliding_up_hall", 0, &flags);
	ddata->gpio_sliding_up_hall = gpio;

	gpio = gpio_to_irq(gpio);
	ddata->irq_sliding_up_hall = gpio;
	pr_info("%s : gpio_sliding_up_hall=%d , irq_sliding_up_hall=%d\n", __func__, ddata->gpio_sliding_up_hall, ddata->irq_sliding_up_hall);

#if defined(EMULATE_sliding_up_hall_IC)
	gpio = of_get_named_gpio_flags(np, "sliding_up_hall,gpio_sliding_up_hall_key1", 0, &flags);
	ddata->gpio_sliding_up_hall_key1 = gpio;
	
	gpio = of_get_named_gpio_flags(np, "sliding_up_hall,gpio_sliding_up_hall_key2", 0, &flags);
	ddata->gpio_sliding_up_hall_key2 = gpio;
	pr_info("%s : sliding_up_hall_key1=%d , sliding_up_hall_key2=%d\n", __func__, ddata->gpio_sliding_up_hall_key1, ddata->gpio_sliding_up_hall_key2);
#endif
	return 0;
}
#endif

static int sliding_up_hall_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sliding_up_hall_drvdata *ddata;
	struct input_dev *input;
	int error;
	int wakeup = 0;

	pr_info("%s called", __func__);
	ddata = kzalloc(sizeof(struct sliding_up_hall_drvdata), GFP_KERNEL);
	if (!ddata) {
		dev_err(dev, "failed to allocate state\n");
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	if (dev->of_node) {
		error = of_sliding_up_hall_data_parsing_dt(dev, ddata);
		if (error < 0) {
			pr_info("%s : fail to get the dt (sliding_up_hall)\n", __func__);
			goto fail1;
		}
	}
#endif

#if defined(EMULATE_sliding_up_hall_IC)
	ddata->emulated_sliding_up_hall_status = 0;
#endif

	input = input_allocate_device();
	if (!input) {
		dev_err(dev, "failed to allocate state\n");
		error = -ENOMEM;
		goto fail1;
	}

	ddata->input = input;

	wake_lock_init(&ddata->sliding_up_hall_wake_lock, WAKE_LOCK_SUSPEND,
		"sliding_up_hall wake lock");

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	input->name = "sliding_up_hall";
	input->phys = "sliding_up_hall";
	input->dev.parent = &pdev->dev;

	input->evbit[0] |= BIT_MASK(EV_SW);
	input_set_capability(input, EV_SW, SW_SLIDINGUPHALL);

	input->open = sliding_up_hall_open;
	input->close = sliding_up_hall_close;

	/* Enable auto repeat feature of Linux input subsystem */
	__set_bit(EV_REP, input->evbit);

	init_sliding_up_hall_ic_irq(input);

	if (ddata->gpio_sliding_up_hall != 0) {
		error = device_create_file(sec_key, &dev_attr_sliding_up_hall_detect);
		if (error < 0) {
			pr_err("Failed to create device file(%s)!, error: %d\n",
			dev_attr_sliding_up_hall_detect.attr.name, error);
		}
	}

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			error);
		goto fail1;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	pr_info("%s end", __func__);
	return 0;

 fail1:
	kfree(ddata);

	return error;
}

static int sliding_up_hall_remove(struct platform_device *pdev)
{
	struct sliding_up_hall_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;

	pr_info("%s start\n", __func__);

	device_init_wakeup(&pdev->dev, 0);

	input_unregister_device(input);

	wake_lock_destroy(&ddata->sliding_up_hall_wake_lock);

	kfree(ddata);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id sliding_up_hall_dt_ids[] = {
	{ .compatible = "sliding_up_hall" },
	{ },
};
MODULE_DEVICE_TABLE(of, sliding_up_hall_dt_ids);
#endif /* CONFIG_OF */

#ifdef CONFIG_PM_SLEEP
static int sliding_up_hall_suspend(struct device *dev)
{
	struct sliding_up_hall_drvdata *ddata = dev_get_drvdata(dev);
	struct input_dev *input = ddata->input;

	pr_info("%s start\n", __func__);

	enable_irq_wake(ddata->irq_sliding_up_hall);

	if (device_may_wakeup(dev)) {
		enable_irq_wake(ddata->irq_sliding_up_hall);
	} else {
		mutex_lock(&input->mutex);
		if (input->users)
			sliding_up_hall_close(input);
		mutex_unlock(&input->mutex);
	}

	return 0;
}

static int sliding_up_hall_resume(struct device *dev)
{
	struct sliding_up_hall_drvdata *ddata = dev_get_drvdata(dev);
	struct input_dev *input = ddata->input;

	pr_info("%s start\n", __func__);
	input_sync(input);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sliding_up_hall_pm_ops, sliding_up_hall_suspend, sliding_up_hall_resume);

static struct platform_driver sliding_up_hall_device_driver = {
	.probe		= sliding_up_hall_probe,
	.remove		= sliding_up_hall_remove,
	.driver		= {
		.name	= "sliding_up_hall",
		.owner	= THIS_MODULE,
		.pm	= &sliding_up_hall_pm_ops,
#if defined(CONFIG_OF)
		.of_match_table	= sliding_up_hall_dt_ids,
#endif /* CONFIG_OF */
	}
};

static int __init sliding_up_hall_init(void)
{
	pr_info("%s start\n", __func__);
	return platform_driver_register(&sliding_up_hall_device_driver);
}

static void __exit sliding_up_hall_exit(void)
{
	pr_info("%s start\n", __func__);
	platform_driver_unregister(&sliding_up_hall_device_driver);
}

late_initcall(sliding_up_hall_init);
module_exit(sliding_up_hall_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>");
MODULE_DESCRIPTION("Keyboard driver for GPIOs");
MODULE_ALIAS("platform:gpio-keys");
