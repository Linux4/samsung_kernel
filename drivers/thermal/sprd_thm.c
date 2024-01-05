/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/thermal.h>
#include <linux/sprd_thm.h>
#include "thm.h"

///#define COOLING_TEST
#ifdef COOLING_TEST		//test cooling
static int get_max_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret = -EINVAL;
	*state = 2;
	ret = 0;

	//TODO
	printk(KERN_NOTICE " ------------------get_max_state \n");
	return ret;
}

static int get_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret = -EINVAL;
	*state = 1;
	ret = 0;

	//TODO
	printk(KERN_NOTICE " ---------------get_cur_state \n");
	return ret;
}

static int set_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long state)
{
	int ret = -EINVAL;
	ret = 0;

	//TODO
	printk(KERN_NOTICE " -----------------set_cur_state %ld\n", state);
	return ret;
}

static struct thermal_cooling_device_ops const cooling_ops = {
	.get_max_state = get_max_state,
	.get_cur_state = get_cur_state,
	.set_cur_state = set_cur_state,
};

#endif /*  */

/* Local function to check if thermal zone matches cooling devices */
static int sprd_thermal_match_cdev(struct thermal_cooling_device *cdev,
				   struct sprd_trip_point *trip_point)
{
	int i;
	if (!strlen(cdev->type))
		return -EINVAL;
	for (i = 0; i < COOLING_DEV_MAX; i++) {
		if (!strcmp(trip_point->cdev_name[i], cdev->type))
			return 0;
	}
	return -ENODEV;
}

/* Callback to bind cooling device to thermal zone */
static int sprd_cdev_bind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	int i, ret = -EINVAL;

	printk(KERN_INFO "sprd_cdev_bind--%d \n", pzone->sensor_id);
	for (i = 0; i < ptrips->num_trips; i++) {
		if (sprd_thermal_match_cdev(cdev, &ptrips->trip_points[i]))
			continue;
		ret = thermal_zone_bind_cooling_device(thermal, i, cdev, THERMAL_NO_LIMIT, THERMAL_NO_LIMIT);
		dev_info(&cdev->device, "%s bind to %d: %d-%s\n", cdev->type,
			 i, ret, ret ? "fail" : "succeed");
	}
	return 0;
}

/* Callback to unbind cooling device from thermal zone */
static int sprd_cdev_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	int i, ret = -EINVAL;

	for (i = 0; i < ptrips->num_trips; i++) {
		if (sprd_thermal_match_cdev(cdev, &ptrips->trip_points[i]))
			continue;
		ret = thermal_zone_unbind_cooling_device(thermal, i, cdev);
		dev_info(&cdev->device, "%s unbind from %d: %s\n",
			 cdev->type, i, ret ? "fail" : "succeed");
	}
	return ret;
}

/* Callback to get current temperature */
static int sprd_sys_get_temp(struct thermal_zone_device *thermal,
			     unsigned long *temp)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	*temp = sprd_thm_temp_read(pzone->sensor_id);
	return 0;
}

/* Callback to get thermal zone mode */
static int sprd_sys_get_mode(struct thermal_zone_device *thermal,
			     enum thermal_device_mode *mode)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	mutex_lock(&pzone->th_lock);
	*mode = pzone->mode;
	mutex_unlock(&pzone->th_lock);
	return 0;
}

/* Callback to set thermal zone mode */
static int sprd_sys_set_mode(struct thermal_zone_device *thermal,
			     enum thermal_device_mode mode)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	mutex_lock(&pzone->th_lock);
	pzone->mode = mode;
	if (mode == THERMAL_DEVICE_ENABLED)
		schedule_work(&pzone->therm_work);
	mutex_unlock(&pzone->th_lock);
	return 0;
}

/* Callback to get trip point type */
static int sprd_sys_get_trip_type(struct thermal_zone_device *thermal,
				  int trip, enum thermal_trip_type *type)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	if (trip >= ptrips->num_trips)
		return -EINVAL;
	*type = ptrips->trip_points[trip].type;
	return 0;
}

/* Callback to get trip point temperature */
static int sprd_sys_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip, unsigned long *temp)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	if (trip >= ptrips->num_trips)
		return -EINVAL;
	*temp = ptrips->trip_points[trip].temp;
	return 0;
}

/* Callback to get critical trip point temperature */
static int sprd_sys_get_crit_temp(struct thermal_zone_device *thermal,
				  unsigned long *temp)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	int i;
	for (i = ptrips->num_trips - 1; i > 0; i--) {
		if (ptrips->trip_points[i].type == THERMAL_TRIP_CRITICAL) {
			*temp = ptrips->trip_points[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}

static struct thermal_zone_device_ops thdev_ops = {
	.bind = sprd_cdev_bind,
	.unbind = sprd_cdev_unbind,
	.get_temp = sprd_sys_get_temp,
	.get_mode = sprd_sys_get_mode,
	.set_mode = sprd_sys_set_mode,
	.get_trip_type = sprd_sys_get_trip_type,
	.get_trip_temp = sprd_sys_get_trip_temp,
	.get_crit_temp = sprd_sys_get_crit_temp,
};

static irqreturn_t sprd_thm_irq_handler(int irq, void *irq_data)
{
	struct sprd_thermal_zone *pzone = irq_data;

	dev_dbg(&pzone->therm_dev->device, "sprd_thm_irq_handler\n");
	if (!sprd_thm_hw_irq_handle(pzone)) {
		schedule_work(&pzone->therm_work);
	}
	return IRQ_HANDLED;
}

static void sprd_thermal_work(struct work_struct *work)
{
	enum thermal_device_mode cur_mode;
	struct sprd_thermal_zone *pzone;

	pzone = container_of(work, struct sprd_thermal_zone, therm_work);
	mutex_lock(&pzone->th_lock);
	cur_mode = pzone->mode;
	mutex_unlock(&pzone->th_lock);
	if (cur_mode == THERMAL_DEVICE_DISABLED)
		return;
	thermal_zone_device_update(pzone->therm_dev);
	dev_dbg(&pzone->therm_dev->device, "thermal work finished.\n");
}

static int sprd_thermal_probe(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = NULL;
	struct sprd_thm_platform_data *ptrips = NULL;
	struct resource *regs;
	int thm_irq, ret = 0;

	printk(KERN_INFO "sprd_thermal_probe id:%d\n", pdev->id);
	ptrips = dev_get_platdata(&pdev->dev);
	if (!ptrips)
		return -EINVAL;
	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	if (!pzone)
		return -ENOMEM;
	mutex_init(&pzone->th_lock);
	mutex_lock(&pzone->th_lock);
	pzone->mode = THERMAL_DEVICE_DISABLED;
	pzone->trip_tab = ptrips;
	pzone->sensor_id = pdev->id;
	INIT_WORK(&pzone->therm_work, sprd_thermal_work);
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		return -ENXIO;
	}
	pzone->reg_base = (void __iomem *)regs->start;
	printk(KERN_INFO "sprd_thermal_probe id:%d,base:0x%x\n", pdev->id,
	       (unsigned int)pzone->reg_base);
	thm_irq = platform_get_irq(pdev, 0);
	if (thm_irq < 0) {
		dev_err(&pdev->dev, "Get IRQ_THM_INT failed.\n");
		return thm_irq;
	}
	ret =
	    devm_request_threaded_irq(&pdev->dev, thm_irq, NULL,
				      sprd_thm_irq_handler,
				      IRQF_NO_SUSPEND | IRQF_ONESHOT,
				      "sprd_thm_irq", pzone);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to allocate temp low irq.\n");
		return ret;
	}
	pzone->therm_dev =
	    thermal_zone_device_register("sprd_thermal_zone",
					 ptrips->num_trips, 0, pzone,
					 &thdev_ops, 0, 0, 0); 
	if (IS_ERR_OR_NULL(pzone->therm_dev)) {
		dev_err(&pdev->dev, "Register thermal zone device failed.\n");
		return PTR_ERR(pzone->therm_dev);
	}
	dev_info(&pdev->dev, "Thermal zone device registered.\n");

	//config hardware
	sprd_thm_hw_init(pzone);
	platform_set_drvdata(pdev, pzone);
	pzone->mode = THERMAL_DEVICE_ENABLED;
	mutex_unlock(&pzone->th_lock);

#ifdef COOLING_TEST		//test
	if (!pzone->sensor_id) {
		thermal_cooling_device_register("thermal-cpufreq-0", 0,
						&cooling_ops);
	}
#endif /*  */
	return 0;
}

static int sprd_thermal_remove(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	thermal_zone_device_unregister(pzone->therm_dev);
	cancel_work_sync(&pzone->therm_work);
	mutex_destroy(&pzone->th_lock);
	return 0;
}

static int sprd_thermal_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	flush_work(&pzone->therm_work);
	sprd_thm_hw_suspend(pzone);
	return 0;
}

static int sprd_thermal_resume(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	sprd_thm_hw_resume(pzone);

	return 0;
}

static struct platform_driver sprd_thermal_driver = {
	.probe = sprd_thermal_probe,
	.suspend = sprd_thermal_suspend,
	.resume = sprd_thermal_resume,
	.remove = sprd_thermal_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sprd-thermal",
		   },
};
static int __init sprd_thermal_init(void)
{
	if (0 == sprd_thm_chip_id_check()) {
		return platform_driver_register(&sprd_thermal_driver);
	} else {
		return 0;
	}
}

static void __exit sprd_thermal_exit(void)
{
	if (0 == sprd_thm_chip_id_check()) {
		platform_driver_unregister(&sprd_thermal_driver);
	}
}

module_init(sprd_thermal_init);
module_exit(sprd_thermal_exit);

// module_platform_driver(sprd_thermal_driver);
MODULE_AUTHOR("Mingwei Zhang <mingwei.zhang@spreadtrum.com>");
MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL");
