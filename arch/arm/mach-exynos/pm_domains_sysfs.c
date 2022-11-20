/* linux/arch/arm/mach-exynos/dev-runtime_pm.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS - Runtime PM Test Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <mach/pm_domains_v2.h>

struct platform_device exynos_device_runtime_pm = {
	.name	= "runtime_pm_test",
	.id	= -1,
};

static ssize_t show_power_domain(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exynos_pm_domain *pd_index = NULL;
	int ret = 0;

#ifdef CONFIG_SOC_EXYNOS5260
	if(soc_is_exynos5260())
		pd_index = exynos5260_pm_domain();
#endif
	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		/* skip unmanaged power domains */
		if (!pd_index->enable)
			continue;

		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%-13s - %-3s, ",
				pd_index->name,
				pd_index->check_status(pd_index) ? "on" : "off");
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%08x %08x %08x\n",
					__raw_readl(pd_index->base+0x0),
					__raw_readl(pd_index->base+0x4),
					__raw_readl(pd_index->base+0x8));
	}

	return ret;
}

static int exynos_pd_power_on(struct device *dev, const char * device_name)
{
	struct exynos_pm_domain *pd_index = NULL;
	int ret = 0;
	struct gpd_timing_data gpd_td = {
		.stop_latency_ns = 50000,
		.start_latency_ns = 50000,
		.save_state_latency_ns = 500000,
		.restore_state_latency_ns = 500000,
	};

#ifdef CONFIG_SOC_EXYNOS5260
	if(soc_is_exynos5260())
		pd_index = exynos5260_pm_domain();
#endif
	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		/* skip unmanaged power domains */
		if (!pd_index->enable)
			continue;

		if (strcmp(pd_index->name, device_name)) {
			continue;
		}

		if (pd_index->check_status(pd_index)) {
			pr_err("PM DOMAIN: %s is already on.\n", pd_index->name);
			break;
		}

		while (1) {
			ret = __pm_genpd_add_device(&pd_index->genpd, dev, &gpd_td);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (!ret) {
			pm_genpd_dev_need_restore(dev, true);
			pr_info("PM DOMAIN: %s, Device : %s Registered\n", pd_index->name, dev_name(dev));
		} else
			pr_err("PM DOMAIN: %s cannot add device %s\n", pd_index->name, dev_name(dev));

		pm_runtime_enable(dev);
		pm_runtime_get_sync(dev);
		pr_info("%s: power on.\n", pd_index->name);
	}

	return ret;

}

static int exynos_pd_power_off(struct device *dev, const char * device_name)
{
	struct exynos_pm_domain *pd_index = NULL;
	int ret = 0;

#ifdef CONFIG_SOC_EXYNOS5260
	if(soc_is_exynos5260())
		pd_index = exynos5260_pm_domain();
#endif
	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		/* skip unmanaged power domains */
		if (!pd_index->enable)
			continue;

		if (strcmp(pd_index->name, device_name)) {
			continue;
		}

		if (!pd_index->check_status(pd_index)) {
			pr_err("PM DOMAIN: %s is already off.\n", pd_index->name);
			break;
		}

		pm_runtime_put_sync(dev);
		pm_runtime_disable(dev);

		while (1) {
			ret = pm_genpd_remove_device(&pd_index->genpd, dev);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (ret)
			pr_err("PM DOMAIN: %s cannot remove device %s\n", pd_index->name, dev_name(dev));
		pr_info("%s: power off.\n", pd_index->name);
	}

	return ret;

}

static int exynos_pd_longrun_test(struct device *dev, const char * device_name)
{
	struct exynos_pm_domain *pd_index = NULL;
	int i, ret = 0;
	struct gpd_timing_data gpd_td = {
		.stop_latency_ns = 50000,
		.start_latency_ns = 50000,
		.save_state_latency_ns = 500000,
		.restore_state_latency_ns = 500000,
	};

#ifdef CONFIG_SOC_EXYNOS5260
	if(soc_is_exynos5260())
		pd_index = exynos5260_pm_domain();
#endif
	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		/* skip unmanaged power domains */
		if (!pd_index->enable)
			continue;

		if (strcmp(pd_index->name, device_name)) {
			continue;
		}

		if (pd_index->check_status(pd_index)) {
			pr_err("PM DOMAIN: %s is working. Stop testing\n", pd_index->genpd.name);
			break;
		}

		while (1) {
			ret = __pm_genpd_add_device(&pd_index->genpd, dev, &gpd_td);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (!ret) {
			pm_genpd_dev_need_restore(dev, true);
			pr_info("PM DOMAIN: %s, Device : %s Registered\n", pd_index->genpd.name, dev_name(dev));
		} else
			pr_err("PM DOMAIN: %s cannot add device %s\n", pd_index->genpd.name, dev_name(dev));

		pr_info("%s: test start.\n", pd_index->genpd.name);
		pm_runtime_enable(dev);
		for (i=0; i<100; i++) {
			pm_runtime_get_sync(dev);
			mdelay(50);
			pm_runtime_put_sync(dev);
			mdelay(50);
		}
		pr_info("%s: test done.\n", pd_index->genpd.name);
		pm_runtime_disable(dev);

		while (1) {
			ret = pm_genpd_remove_device(&pd_index->genpd, dev);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (ret)
			pr_err("PM DOMAIN: %s cannot remove device %s\n", pd_index->name, dev_name(dev));
	}

	return ret;
}

static ssize_t store_power_domain_test(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char device_name[32], test_name[32];

	sscanf(buf, "%s %s", device_name, test_name);

	switch (test_name[0]) {
	case '1':
		exynos_pd_power_on(dev, device_name);
		break;

	case '0':
		exynos_pd_power_off(dev, device_name);
		break;

	case 't':
		exynos_pd_longrun_test(dev, device_name);
		break;

	default:
		printk("echo \"device\" \"test\" > control\n");
	}

	return count;
}

static DEVICE_ATTR(control, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, show_power_domain, store_power_domain_test);

static struct attribute *control_device_attrs[] = {
	&dev_attr_control.attr,
	NULL,
};

static const struct attribute_group control_device_attr_group = {
	.attrs = control_device_attrs,
};

static int runtime_pm_test_probe(struct platform_device *pdev)
{
	struct class *runtime_pm_class;
	struct device *runtime_pm_dev;
	int ret;

	runtime_pm_class = class_create(THIS_MODULE, "runtime_pm");
	runtime_pm_dev = device_create(runtime_pm_class, NULL, 0, NULL, "test");
	ret = sysfs_create_group(&runtime_pm_dev->kobj, &control_device_attr_group);
	if (ret) {
		pr_err("Runtime PM Test : error to create sysfs\n");
		return -EINVAL;
	}

	pm_runtime_enable(&pdev->dev);

	return 0;
}

static int runtime_pm_test_runtime_suspend(struct device *dev)
{
	pr_info("Runtime PM Test : Runtime_Suspend\n");
	return 0;
}

static int runtime_pm_test_runtime_resume(struct device *dev)
{
	pr_info("Runtime PM Test : Runtime_Resume\n");
	return 0;
}

static struct dev_pm_ops pm_ops = {
	.runtime_suspend = runtime_pm_test_runtime_suspend,
	.runtime_resume = runtime_pm_test_runtime_resume,
};

static struct platform_driver runtime_pm_test_driver = {
	.probe		= runtime_pm_test_probe,
	.driver		= {
		.name	= "runtime_pm_test",
		.owner	= THIS_MODULE,
		.pm	= &pm_ops,
	},
};

static int __init runtime_pm_test_driver_init(void)
{
	platform_device_register(&exynos_device_runtime_pm);

	return platform_driver_register(&runtime_pm_test_driver);
}
arch_initcall_sync(runtime_pm_test_driver_init);
