/*
 * cyttsp5_bus.c
 * Cypress TrueTouch(TM) Standard Product V5 Bus Driver.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov aleksej.makarov@sonyericsson.com
 * Modified by: Cypress Semiconductor for complete set of TTSP Bus interfaces.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/cyttsp5/cyttsp5_bus.h>

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/limits.h>

static DEFINE_MUTEX(core_lock);
static LIST_HEAD(adapter_list);
static LIST_HEAD(core_dev_list);
static LIST_HEAD(cyttsp5_dev_list);

struct bus_type cyttsp5_bus_type;

static void cyttsp5_dev_release(struct device *dev)
{
	put_device(dev->parent);
}

static struct device_type cyttsp5_dev_type = {
	.release = cyttsp5_dev_release
};

static struct device_type cyttsp5_core_type = {
	.release = cyttsp5_dev_release
};

static void cyttsp5_initialize_device(struct cyttsp5_device *dev,
		struct cyttsp5_device_info const *dev_info)
{
	dev->name = dev_info->name;
	dev->core_id = dev_info->core_id;
	dev->dev.platform_data = dev_info->platform_data;
}

static void _cyttsp5_reinitialize_device(struct cyttsp5_device *dev)
{
	void *platform_data = dev->dev.platform_data;

	memset(&dev->dev, 0, sizeof(dev->dev));
	dev->dev.platform_data = platform_data;
	dev->core = NULL;
}

static void cyttsp5_initialize_core(struct cyttsp5_core *core,
		struct cyttsp5_core_info const *core_info)
{
	core->name = core_info->name;
	core->id = core_info->id;
	core->adap_id = core_info->adap_id;
	core->dev.platform_data = core_info->platform_data;
}

static void _cyttsp5_reinitialize_core(struct cyttsp5_core *core)
{
	void *platform_data = core->dev.platform_data;

	memset(&core->dev, 0, sizeof(core->dev));
	core->dev.platform_data = platform_data;
	core->adap = NULL;
}

static int _cyttsp5_register_dev(struct cyttsp5_device *pdev,
		struct cyttsp5_core *core)
{
	int ret;

	/* Check if the device is registered with the system */
	if (device_is_registered(&pdev->dev))
		return -EEXIST;

	pdev->core = core;
	pdev->dev.parent = get_device(&core->dev);
	pdev->dev.bus = &cyttsp5_bus_type;
	pdev->dev.type = &cyttsp5_dev_type;
	dev_set_name(&pdev->dev, "%s.%s", pdev->name,  core->id);

	ret = device_register(&pdev->dev);
	dev_dbg(&pdev->dev,
		"%s: Registering device '%s'. Parent at '%s', err = %d\n",
		 __func__, dev_name(&pdev->dev),
		 dev_name(pdev->dev.parent), ret);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to register device, err %d\n",
			__func__, ret);
		pdev->core = NULL;
	}
	return ret;
}

static void _cyttsp5_unregister_dev(struct cyttsp5_device *pdev)
{
	/* Check if the device is registered with the system */
	if (!device_is_registered(&pdev->dev))
		return;

	dev_dbg(&pdev->dev, "%s: Unregistering device '%s'.\n",
		__func__, dev_name(&pdev->dev));
	device_unregister(&pdev->dev);
}

static int _cyttsp5_register_core(struct cyttsp5_core *pdev,
		struct cyttsp5_adapter *adap)
{
	int ret;

	/* Check if the device is registered with the system */
	if (device_is_registered(&pdev->dev))
		return -EEXIST;

	pdev->adap = adap;
	pdev->dev.parent = get_device(adap->dev);
	pdev->dev.bus = &cyttsp5_bus_type;
	pdev->dev.type = &cyttsp5_core_type;
	dev_set_name(&pdev->dev, "%s.%s", pdev->id,  adap->id);

	ret = device_register(&pdev->dev);
	dev_dbg(&pdev->dev,
		"%s: Registering device '%s'. Parent at '%s', err = %d\n",
		 __func__, dev_name(&pdev->dev),
		 dev_name(pdev->dev.parent), ret);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to register device, err %d\n",
			__func__, ret);
		pdev->adap = NULL;
	}
	return ret;
}

static void _cyttsp5_unregister_core(struct cyttsp5_core *pdev)
{
	/* Check if the core is registered with the system */
	if (!device_is_registered(&pdev->dev))
		return;

	dev_dbg(&pdev->dev, "%s: Unregistering core '%s'.\n",
		__func__, dev_name(&pdev->dev));
	device_unregister(&pdev->dev);
}

static void _cyttsp5_unregister_and_reinitialize_devices(
		struct cyttsp5_core *core)
{
	struct cyttsp5_device *dev;

	list_for_each_entry(dev, &cyttsp5_dev_list, node)
		if (dev->core == core) {
			_cyttsp5_unregister_dev(dev);
			_cyttsp5_reinitialize_device(dev);
		}
}

static struct cyttsp5_adapter *find_adapter(char const *adap_id)
{
	struct cyttsp5_adapter *a;

	list_for_each_entry(a, &adapter_list, node)
		if (!strncmp(a->id, adap_id, NAME_MAX))
			return a;
	return NULL;
}

static struct cyttsp5_core *find_core(char const *core_id)
{
	struct cyttsp5_core *d;

	list_for_each_entry(d, &core_dev_list, node)
		if (!strncmp(d->id, core_id, NAME_MAX))
			return d;
	return NULL;
}

static struct cyttsp5_core *find_core_with_driver(char const *core_id)
{
	struct cyttsp5_core *d;

	d = find_core(core_id);
	if (d && d->dev.driver)
		return d;
	return NULL;
}

static struct cyttsp5_device *find_device(char const *name,
		char const *core_id)
{
	struct cyttsp5_device *d;

	list_for_each_entry(d, &cyttsp5_dev_list, node)
		if (!strncmp(d->name, name, NAME_MAX) &&
				!strncmp(d->core_id, core_id, NAME_MAX))
			return d;
	return NULL;
}

static void rescan_devices(struct cyttsp5_core *core)
{
	struct cyttsp5_device *d;

	list_for_each_entry(d, &cyttsp5_dev_list, node)
		if (!d->core && !strncmp(core->id, d->core_id, NAME_MAX))
			_cyttsp5_register_dev(d, core);
}

static void rescan_cores(struct cyttsp5_adapter *adap)
{
	struct cyttsp5_core *d;

	list_for_each_entry(d, &core_dev_list, node)
		if (!d->adap && !strncmp(adap->id, d->adap_id, NAME_MAX))
			_cyttsp5_register_core(d, adap);
}

static int cyttsp5_check_device_info(
	struct cyttsp5_device_info const *dev_info)
{
	int len;

	if (!dev_info->name)
		return -EINVAL;
	if (!dev_info->core_id)
		return -EINVAL;

	len = strnlen(dev_info->name, NAME_MAX);
	if (len == 0 || len == NAME_MAX)
		return -EINVAL;

	len = strnlen(dev_info->core_id, NAME_MAX);
	if (len == 0 || len == NAME_MAX)
		return -EINVAL;

	return 0;
}

static int cyttsp5_check_core_info(
	struct cyttsp5_core_info const *core_info)
{
	int len;

	if (!core_info->name)
		return -EINVAL;
	if (!core_info->id)
		return -EINVAL;
	if (!core_info->adap_id)
		return -EINVAL;

	len = strnlen(core_info->name, NAME_MAX);
	if (len == 0 || len == NAME_MAX)
		return -EINVAL;

	len = strnlen(core_info->id, NAME_MAX);
	if (len == 0 || len == NAME_MAX)
		return -EINVAL;

	len = strnlen(core_info->adap_id, NAME_MAX);
	if (len == 0 || len == NAME_MAX)
		return -EINVAL;

	return 0;
}

int cyttsp5_register_device(struct cyttsp5_device_info const *dev_info)
{
	struct cyttsp5_device *dev;
	struct cyttsp5_core *core;
	int ret;

	if (!dev_info) {
		ret = -EINVAL;
		goto fail;
	}

	ret = cyttsp5_check_device_info(dev_info);
	if (ret) {
		pr_debug("%s: dev_info is invalid\n", __func__);
		goto fail;
	}

	mutex_lock(&core_lock);
	if (find_device(dev_info->name, dev_info->core_id)) {
		pr_debug("%s: device '%s' with core id '%s' already exists\n",
			__func__, dev_info->name, dev_info->core_id);
		ret = -EEXIST;
		goto fail_unlock;
	}
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		pr_err("%s: failed to allocate device '%s'\n",
			__func__, dev_info->name);
		ret = -ENOMEM;
		goto fail_unlock;
	}
	cyttsp5_initialize_device(dev, dev_info);
	list_add(&dev->node, &cyttsp5_dev_list);
	pr_debug("%s: '%s' added to cyttsp5_dev_list\n", __func__, dev->name);
	core = find_core_with_driver(dev->core_id);
	if (core)
		ret = _cyttsp5_register_dev(dev, core);
fail_unlock:
	mutex_unlock(&core_lock);
fail:
	return ret;
}
EXPORT_SYMBOL_GPL(cyttsp5_register_device);

int cyttsp5_unregister_device(char const *name, char const *core_id)
{
	struct cyttsp5_device *dev;
	int ret = 0;

	if (!name || !core_id) {
		ret = -EINVAL;
		goto fail;
	}

	mutex_lock(&core_lock);
	dev = find_device(name, core_id);
	if (!dev) {
		pr_err("%s: device '%s' could not be found\n", __func__, name);
		ret = -ENODEV;
		goto fail_unlock;
	}
	_cyttsp5_unregister_dev(dev);
	list_del(&dev->node);
	pr_debug("%s: '%s' removed from cyttsp5_dev_list\n", __func__,
		dev->name);
	kfree(dev);
fail_unlock:
	mutex_unlock(&core_lock);
fail:
	return ret;
}
EXPORT_SYMBOL_GPL(cyttsp5_unregister_device);

int cyttsp5_register_core_device(struct cyttsp5_core_info const *core_info)
{
	struct cyttsp5_core *core;
	struct cyttsp5_adapter *adap;
	int ret;

	if (!core_info) {
		ret = -EINVAL;
		goto fail;
	}

	ret = cyttsp5_check_core_info(core_info);
	if (ret) {
		pr_debug("%s: core_info is invalid\n", __func__);
		goto fail;
	}

	mutex_lock(&core_lock);
	if (find_core(core_info->id)) {
		pr_debug("%s: core id '%s' already exists\n",
				__func__, core_info->id);
		ret = -EEXIST;
		goto fail_unlock;
	}
	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core) {
		pr_err("%s: failed to allocate core device '%s'\n",
			__func__, core_info->name);
		ret = -ENOMEM;
		goto fail_unlock;
	}
	cyttsp5_initialize_core(core, core_info);
	list_add(&core->node, &core_dev_list);
	pr_debug("%s: '%s' added to core_dev_list\n", __func__, core->name);
	adap = find_adapter(core->adap_id);
	if (adap) {
		pr_debug("%s: adapter for '%s' is '%s'\n", __func__,
				core->id, dev_name(adap->dev));
		ret = _cyttsp5_register_core(core, adap);
		if (!ret)
			rescan_devices(core);
	}
fail_unlock:
	mutex_unlock(&core_lock);
fail:
	return ret;
}
EXPORT_SYMBOL_GPL(cyttsp5_register_core_device);

int cyttsp5_add_adapter(char const *id, struct cyttsp5_ops const *ops,
		struct device *parent)
{
	int rc = 0;
	struct cyttsp5_adapter *a;

	if (!parent) {
		dev_err(parent, "%s: need parent for '%s'\n", __func__, id);
		return -EINVAL;
	}
	mutex_lock(&core_lock);
	if (find_adapter(id)) {
		dev_err(parent, "%s: adapter '%s' already exists\n",
				__func__, id);
		rc = -EEXIST;
		goto fail;
	}
	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a) {
		dev_err(parent, "%s: failed to allocate adapter '%s'\n",
				__func__, id);
		rc = -ENOMEM;
		goto fail;
	}
	memcpy(a->id, id, sizeof(a->id));
	a->id[sizeof(a->id) - 1] = 0;
	a->read_default = ops->read_default;
	a->read_default_nosize = ops->read_default_nosize;
	a->write_read_specific = ops->write_read_specific;
	a->dev = parent;
	list_add(&a->node, &adapter_list);
	dev_dbg(parent, "%s: '%s' added to adapter_list\n", __func__, id);
	rescan_cores(a);
fail:
	mutex_unlock(&core_lock);
	return rc;
}
EXPORT_SYMBOL_GPL(cyttsp5_add_adapter);

int cyttsp5_del_adapter(char const *id)
{
	int rc = 0;
	struct cyttsp5_adapter *adap;
	struct cyttsp5_core *core;

	mutex_lock(&core_lock);
	adap = find_adapter(id);
	if (!adap) {
		pr_err("%s: adapter '%s' does not exist\n",
			__func__, id);
		rc = -ENODEV;
		goto fail;
	}

	/* Unregister core and devices linked to this adapter
	 * This is to prevent core and devices get probed until
	 * their corresponding adapter is re-added
	 */
	list_for_each_entry(core, &core_dev_list, node) {
		if (core->adap != adap)
			continue;
		_cyttsp5_unregister_and_reinitialize_devices(core);
		_cyttsp5_unregister_core(core);
		_cyttsp5_reinitialize_core(core);
	}

	list_del(&adap->node);
	kfree(adap);
	pr_debug("%s: '%s' removed from adapter_list\n", __func__, id);
fail:
	mutex_unlock(&core_lock);
	return rc;
}
EXPORT_SYMBOL_GPL(cyttsp5_del_adapter);

static struct cyttsp5_device *verify_device_type(struct device *dev)
{
	return dev->type == &cyttsp5_dev_type ? to_cyttsp5_device(dev) : NULL;
}

static struct cyttsp5_core *verify_core_type(struct device *dev)
{
	return dev->type == &cyttsp5_core_type ? to_cyttsp5_core(dev) : NULL;
}

static int cyttsp5_device_match(struct device *dev, struct device_driver *drv)
{
	struct cyttsp5_device *cyttsp5_dev = verify_device_type(dev);
	struct cyttsp5_core *cyttsp5_core;
	int match;

	if (cyttsp5_dev) {
		match = strncmp(cyttsp5_dev->name, drv->name, NAME_MAX) == 0;
		goto exit;
	}
	cyttsp5_core = verify_core_type(dev);
	if (cyttsp5_core) {
		match = strncmp(cyttsp5_core->name, drv->name, NAME_MAX) == 0;
		goto exit;
	}
	match = 0;
exit:
	dev_dbg(dev, "%s: %s matching '%s' driver\n", __func__,
			match ? "is" : "isn't", drv->name);
	return match;
}

static ssize_t modalias_show(struct device *dev, struct device_attribute *a,
			     char *buf)
{
	struct cyttsp5_device *cyttsp5_dev = verify_device_type(dev);
	struct cyttsp5_core *cyttsp5_core;

	char const *name;
	u32 len;

	if (cyttsp5_dev) {
		name = cyttsp5_dev->name;
		goto exit;
	}
	cyttsp5_core = verify_core_type(dev);
	if (cyttsp5_core) {
		name = cyttsp5_core->id;
		goto exit;
	}
	name = "none";
exit:
	len = snprintf(buf, PAGE_SIZE, "ttsp5:%s\n", name);
	return (len >= PAGE_SIZE) ? (PAGE_SIZE - 1) : len;
}

static struct device_attribute cyttsp5_dev_attrs[] = {
	__ATTR_RO(modalias),
	__ATTR_NULL,
};

#ifdef CONFIG_SUSPEND
static int cyttsp5_pm_suspend(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	if (drv && drv->pm && drv->pm->suspend)
		return drv->pm->suspend(dev);
	return 0;
}

static int cyttsp5_pm_resume(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	if (drv && drv->pm && drv->pm->resume)
		return drv->pm->resume(dev);
	return 0;
}
#else /* !CONFIG_SUSPEND */
#define cyttsp5_pm_suspend		NULL
#define cyttsp5_pm_resume		NULL
#endif /* !CONFIG_SUSPEND */

#ifdef CONFIG_PM_RUNTIME
#define cyttsp5_pm_rt_suspend		pm_generic_runtime_suspend
#define cyttsp5_pm_rt_resume		pm_generic_runtime_resume
#define cyttsp5_pm_rt_idle		pm_generic_runtime_idle
#else /* !CONFIG_PM_RUNTIME */
#define cyttsp5_pm_rt_suspend		NULL
#define cyttsp5_pm_rt_resume		NULL
#define cyttsp5_pm_rt_idle		NULL
#endif /* !CONFIG_PM_RUNTIME */

static const struct dev_pm_ops cyttsp5_dev_pm_ops = {
	.suspend = cyttsp5_pm_suspend,
	.resume = cyttsp5_pm_resume,
	.runtime_suspend = cyttsp5_pm_rt_suspend,
	.runtime_resume = cyttsp5_pm_rt_resume,
	.runtime_idle = cyttsp5_pm_rt_idle,
};

struct bus_type cyttsp5_bus_type = {
	.name		= "ttsp5",
	.dev_attrs	= cyttsp5_dev_attrs,
	.match		= cyttsp5_device_match,
	.uevent		= NULL,
	.pm		= &cyttsp5_dev_pm_ops,
};
EXPORT_SYMBOL_GPL(cyttsp5_bus_type);

static int cyttsp5_drv_remove(struct device *_dev)
{
	struct cyttsp5_driver *drv = to_cyttsp5_driver(_dev->driver);
	struct cyttsp5_device *dev = to_cyttsp5_device(_dev);
	struct cyttsp5_core *core = dev->core;
	int ret;

	ret = drv->remove(dev);
	/* Decrease usage count of the core driver */
	module_put(core->dev.driver->owner);
	return ret;
}

static int cyttsp5_core_drv_remove(struct device *_dev)
{
	struct cyttsp5_core_driver *drv = to_cyttsp5_core_driver(_dev->driver);
	struct cyttsp5_core *core = to_cyttsp5_core(_dev);
	struct cyttsp5_adapter *adap = core->adap;
	int ret;

	ret = drv->remove(core);
	/* Decrease usage count of the adapter driver */
	module_put(adap->dev->driver->owner);

	mutex_lock(&core_lock);
	/* Unregister devices linked to this core
	 * This is to prevent devices get probed until
	 * their corresponding core driver is re-added
	 */
	_cyttsp5_unregister_and_reinitialize_devices(core);
	mutex_unlock(&core_lock);

	return ret;
}

static int cyttsp5_drv_probe(struct device *_dev)
{
	struct cyttsp5_driver *drv = to_cyttsp5_driver(_dev->driver);
	struct cyttsp5_device *dev = to_cyttsp5_device(_dev);
	struct cyttsp5_core *core = dev->core;
	int rc;

	if (!core || !core->dev.driver)
		return -ENODEV;

	/* Increase usage count of the core driver*/
	__module_get(core->dev.driver->owner);

	rc = drv->probe(dev);
	if (rc)
		module_put(core->dev.driver->owner);
	dev_dbg(_dev, "%s: for %s = %d\n", __func__, dev->name, rc);
	return rc;
}

static int cyttsp5_core_drv_probe(struct device *_dev)
{
	struct cyttsp5_core_driver *drv = to_cyttsp5_core_driver(_dev->driver);
	struct cyttsp5_core *dev = to_cyttsp5_core(_dev);
	struct cyttsp5_adapter *adap = dev->adap;
	int rc;

	if (!adap || !adap->dev->driver)
		return -ENODEV;

	/* Increase usage count of the adapter driver*/
	__module_get(adap->dev->driver->owner);

	rc = drv->probe(dev);
	dev_dbg(_dev, "%s: for %s = %d\n", __func__, dev->name, rc);
	if (!rc) {
		/*
		 * already has core_lock here
		 * cyttsp4_core_drv_probe() called by API device_register()
		 * device_register() by _cyttsp4_register_core()
		 * and _cyttsp4_register_core() always called core_lock held
		 */
		rescan_devices(dev);
	} else {
		module_put(adap->dev->driver->owner);
	}
	return rc;
}

int cyttsp5_register_driver(struct cyttsp5_driver *drv)
{
	int ret;

	drv->driver.bus = &cyttsp5_bus_type;
	if (drv->probe)
		drv->driver.probe = cyttsp5_drv_probe;
	if (drv->remove)
		drv->driver.remove = cyttsp5_drv_remove;
	ret = driver_register(&drv->driver);
	pr_debug("%s: '%s' returned %d\n", __func__, drv->driver.name, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(cyttsp5_register_driver);

int cyttsp5_register_core_driver(struct cyttsp5_core_driver *drv)
{
	int ret;

	drv->driver.bus = &cyttsp5_bus_type;
	if (drv->probe)
		drv->driver.probe = cyttsp5_core_drv_probe;
	if (drv->remove)
		drv->driver.remove = cyttsp5_core_drv_remove;
	ret = driver_register(&drv->driver);
	pr_debug("%s: '%s' returned %d\n", __func__, drv->driver.name, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(cyttsp5_register_core_driver);

void cyttsp5_unregister_driver(struct cyttsp5_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL_GPL(cyttsp5_unregister_driver);

void cyttsp5_unregister_core_driver(struct cyttsp5_core_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL_GPL(cyttsp5_unregister_core_driver);

static int __init cyttsp5_bus_init(void)
{
	int error;
	error =  bus_register(&cyttsp5_bus_type);
	if (error)
		pr_err("%s: error %d\n", __func__, error);
	else
		pr_debug("%s: ok\n", __func__);
	return error;
}

static void __exit cyttsp5_bus_exit(void)
{
}

subsys_initcall(cyttsp5_bus_init);
module_exit(cyttsp5_bus_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
