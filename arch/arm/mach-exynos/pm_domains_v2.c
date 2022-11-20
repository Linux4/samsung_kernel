/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mach/pm_domains_v2.h>
#include <mach/bts.h>

/* when latency is over pre-defined value warning message will be shown.
 * default values are:
 * start/stop latency - 50us
 * state save/restore latency - 500us
 */
static struct gpd_timing_data gpd_td = {
	.stop_latency_ns = 50000,
	.start_latency_ns = 50000,
	.save_state_latency_ns = 500000,
	.restore_state_latency_ns = 500000,
};

static void exynos_add_device_to_pd(struct exynos_pm_domain *pd,
					 struct device *dev)
{
	int ret;

	if (unlikely(!pd)) {
		pr_err(PM_DOMAIN_PREFIX "can't add device, domain is empty\n");
		return;
	}

	if (unlikely(!dev)) {
		pr_err(PM_DOMAIN_PREFIX "can't add device, device is empty\n");
		return;
	}

	DEBUG_PRINT_INFO("adding %s to power domain %s\n", dev_name(dev), pd->genpd.name);

	while (1) {
		ret = __pm_genpd_add_device(&pd->genpd, dev, &gpd_td);
		if (ret != -EAGAIN)
			break;
		cond_resched();
	}

	if (!ret) {
		pm_genpd_dev_need_restore(dev, true);
		pr_info(PM_DOMAIN_PREFIX "%-13s, Device : %s Registered\n", pd->genpd.name, dev_name(dev));
	} else
		pr_err(PM_DOMAIN_PREFIX "%s cannot add device %s\n", pd->genpd.name, dev_name(dev));
}

/* Sub-domain does not have power on/off features.
 * dummy power on/off function is required.
 */
int exynos_pd_power_dummy(struct exynos_pm_domain *pd,
					int power_flags)
{
	DEBUG_PRINT_INFO("%s: dummy power %s\n", pd->genpd.name, power_flags ? "on":"off");
	pd->status = power_flags;

	return 0;
}

int exynos_pd_status(struct exynos_pm_domain *pd)
{
	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		/* check STATUS register */
		pd->status = (__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN);
	}
	mutex_unlock(&pd->access_lock);

	return pd->status;
}

int exynos_pd_power(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		/* sc_feedback to OPTION register */
		__raw_writel(0x0102, pd->base+0x8);

		/* on/off value to CONFIGURATION register */
		__raw_writel(power_flags, pd->base);

		/* Wait max 1ms */
		timeout = 100;
		/* check STATUS register */
		while ((__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
			if (timeout == 0) {
				pr_err("%s@%p: %08x, %08x, %08x\n",
						pd->genpd.name,
						pd->base,
						__raw_readl(pd->base),
						__raw_readl(pd->base+4),
						__raw_readl(pd->base+8));
				pr_err(PM_DOMAIN_PREFIX "%s cannot control power, timeout\n", pd->name);
				mutex_unlock(&pd->access_lock);
				return -ETIMEDOUT;
			}
			--timeout;
			cpu_relax();
			usleep_range(8, 10);
		}
		if (unlikely(timeout < 50)) {
			pr_warn(PM_DOMAIN_PREFIX "long delay found during %s is %s\n", pd->name, power_flags ? "on":"off");
			pr_warn("%s@%p: %08x, %08x, %08x\n",
					pd->name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
		}
	}
	pd->status = power_flags;
	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s@%p: %08x, %08x, %08x\n",
				pd->genpd.name, pd->base,
				__raw_readl(pd->base),
				__raw_readl(pd->base+4),
				__raw_readl(pd->base+8));

	return 0;
}

/* exynos_genpd_power_on - power-on callback function for genpd.
 * @genpd: generic power domain.
 *
 * main function of power on sequence.
 * 1. clock on
 * 2. reconfiguration of clock
 * 3. pd power on
 * 4. set bts configuration
 * 5. restore clock sources.
 */
int exynos_genpd_power_on(struct generic_pm_domain *genpd)
{
	struct exynos_pm_domain *pd = container_of(genpd, struct exynos_pm_domain, genpd);
	int ret;

	DEBUG_PRINT_INFO("%s genpd on\n", genpd->name);
	if (unlikely(!pd->on)) {
		pr_err(PM_DOMAIN_PREFIX "%s cannot support power on\n", pd->name);
		return -EINVAL;
	}

	/* clock enable before pd on */
	if (pd->cb && pd->cb->on_pre)
		pd->cb->on_pre(pd);

	/* power domain on */
	ret = pd->on(pd, EXYNOS_INT_LOCAL_PWR_EN);
	if (unlikely(ret)) {
		pr_err(PM_DOMAIN_PREFIX "%s found error at power on!\n", genpd->name);
		return ret;
	}

#ifdef CONFIG_EXYNOS5260_BTS
	/* enable bts features if exists */
	if (pd->bts)
		bts_initialize(pd->name, true);
#endif

	if (pd->cb && pd->cb->on_post)
		pd->cb->on_post(pd);

	return 0;
}

/* exynos_genpd_power_off - power-off callback function for genpd.
 * @genpd: generic power domain.
 *
 * main function of power off sequence.
 * 1. clock on
 * 2. reset IPs when necessary
 * 3. pd power off
 * 4. change clock sources to OSC.
 */
int exynos_genpd_power_off(struct generic_pm_domain *genpd)
{
	struct exynos_pm_domain *pd = container_of(genpd, struct exynos_pm_domain, genpd);
	int ret;

	DEBUG_PRINT_INFO("%s genpd off\n", genpd->name);
	if (unlikely(!pd->off)) {
		pr_err(PM_DOMAIN_PREFIX "%s cannot support power off!\n", genpd->name);
		return -EINVAL;
	}

	if (pd->cb && pd->cb->off_pre)
		pd->cb->off_pre(pd);

#ifdef CONFIG_EXYNOS5260_BTS
	/* disable bts features if exists */
	if (pd->bts)
		bts_initialize(pd->name, false);
#endif

	ret = pd->off(pd, 0);
	if (unlikely(ret)) {
		pr_err(PM_DOMAIN_PREFIX "%s found error at power off!\n", genpd->name);
		return ret;
	}

	if (pd->cb && pd->cb->off_post)
		pd->cb->off_post(pd);

	return 0;
}

static void exynos_pm_powerdomain_init(struct exynos_pm_domain *pd)
{
	/* In case of Device Tree, many fields are not initialized.
	 * Additional initialization should be done in function.
	 */
#ifdef CONFIG_OF
	pd->genpd.power_off = exynos_genpd_power_off;
	pd->genpd.power_on = exynos_genpd_power_on;

	/* pd power on/off latency is less than 1ms */
	pd->genpd.power_on_latency_ns = 1000000;
	pd->genpd.power_off_latency_ns = 1000000;

	pd->status = true;
	pd->check_status = exynos_pd_status;
#endif

#ifdef CONFIG_EXYNOS5260_BTS
	do {

		/* bts feature is enabled if exists */
		if (pd->bts) {
			bts_initialize(pd->name, true);
			DEBUG_PRINT_INFO("%s - bts feature is enabled\n", pd->name);
		}
	} while(0);
#endif

	mutex_init(&pd->access_lock);

	pm_genpd_init(&pd->genpd, NULL, !pd->status);
}

#ifdef CONFIG_OF

#ifdef CONFIG_EXYNOS5260_BTS
/**
 *  of_device_bts_is_available - check if bts feature is enabled or not
 *
 *  @device: Node to check for availability, with locks already held
 *
 *  Returns 1 if the status property is "enabled" or "ok",
 *  0 otherwise
 */
static int of_device_bts_is_available(const struct device_node *device)
{
	const char *status;
	int statlen;

	status = of_get_property(device, "bts-status", &statlen);
	if (status == NULL)
		return 0;

	if (statlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "ok"))
			return 1;
	}

	return 0;
}
#endif

/* show_power_domain - show current power domain status.
 *
 * read the status of power domain and show it.
 */
static void show_power_domain(void)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, "samsung,exynos5430-pd") {
		struct platform_device *pdev;
		struct exynos_pm_domain *pd;

		if (of_device_is_available(np)) {
			pdev = of_find_device_by_node(np);
			if (!pdev)
				continue;
			pd = platform_get_drvdata(pdev);
			pr_info("   %-9s - %-3s\n", pd->genpd.name,
					pd->check_status(pd) ? "on" : "off");
		} else
			pr_info("   %-9s - %s\n",
						kstrdup(np->name, GFP_KERNEL),
						"on,  always");
	}

	return;
}

static void exynos_remove_device_from_pd(struct device *dev)
{
	struct generic_pm_domain *genpd = dev_to_genpd(dev);
	int ret;

	DEBUG_PRINT_INFO("removing %s from power domain %s\n", dev_name(dev), genpd->name);

	while (1) {
		ret = pm_genpd_remove_device(genpd, dev);
		if (ret != -EAGAIN)
			break;
		cond_resched();
	}
}

static void exynos_read_domain_from_dt(struct device *dev)
{
	struct platform_device *pd_pdev;
	struct exynos_pm_domain *pd;
	struct device_node *node;

	/* add platform device to power domain
	 * 1. get power domain node for given platform device
	 * 2. get power domain device from power domain node
	 * 3. get power domain structure from power domain device
	 * 4. add given platform device to power domain structure.
	 */
	node = of_parse_phandle(dev->of_node, "samsung,power-domain", 0);
	if (!node)
		return;

	pd_pdev = of_find_device_by_node(node);
	if (!pd_pdev)
		return;

	pd = platform_get_drvdata(pd_pdev);
	exynos_add_device_to_pd(pd, dev);
}

static int exynos_pm_notifier_call(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct device *dev = data;

	switch (event) {
	case BUS_NOTIFY_BIND_DRIVER:
		if (dev->of_node)
			exynos_read_domain_from_dt(dev);
		break;

	case BUS_NOTIFY_UNBOUND_DRIVER:
		exynos_remove_device_from_pd(dev);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block platform_nb = {
	.notifier_call = exynos_pm_notifier_call,
};

static __init int exynos_pm_dt_parse_domains(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;

	for_each_compatible_node(np, NULL, "samsung,exynos5430-pd") {
		struct exynos_pm_domain *pd;
		struct device_node *children;

		/* skip unmanaged power domain */
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);

		pd = kzalloc(sizeof(*pd), GFP_KERNEL);
		if (!pd) {
			pr_err(PM_DOMAIN_PREFIX "%s: failed to allocate memory for domain\n",
					__func__);
			return -ENOMEM;
		}

		pd->genpd.name = kstrdup(np->name, GFP_KERNEL);
		pd->name = pd->genpd.name;
		pd->genpd.of_node = np;
		pd->base = of_iomap(np, 0);
		pd->on = exynos_pd_power;
		pd->off = exynos_pd_power;
		pd->cb = exynos_pd_find_callback(pd);

		platform_set_drvdata(pdev, pd);

		exynos_pm_powerdomain_init(pd);

		/* add LOGICAL sub-domain
		 * It is not assumed that there is REAL sub-domain.
		 * Power on/off functions are not defined here.
		 */
		for_each_child_of_node(np, children) {
			struct exynos_pm_domain *sub_pd;
			struct platform_device *sub_pdev;

			if (!children)
				break;

			sub_pd = kzalloc(sizeof(*sub_pd), GFP_KERNEL);
			if (!sub_pd) {
				pr_err("%s: failed to allocate memory for power domain\n",
						__func__);
				return -ENOMEM;
			}

			sub_pd->genpd.name = kstrdup(children->name, GFP_KERNEL);
			sub_pd->name = sub_pd->genpd.name;
			sub_pd->genpd.of_node = children;
			sub_pd->on = exynos_pd_power_dummy;
			sub_pd->off = exynos_pd_power_dummy;
			sub_pd->cb = NULL;

			/* kernel does not create sub-domain pdev. */
			sub_pdev = of_find_device_by_node(children);
			if (!sub_pdev)
				sub_pdev = of_platform_device_create(children, NULL, &pdev->dev);
			if (!sub_pdev) {
				pr_err(PM_DOMAIN_PREFIX "sub domain allocation failed: %s\n",
							kstrdup(children->name, GFP_KERNEL));
				continue;
			}
			platform_set_drvdata(sub_pdev, sub_pd);

			exynos_pm_powerdomain_init(sub_pd);

			if (pm_genpd_add_subdomain(&pd->genpd, &sub_pd->genpd))
				pr_err("PM DOMAIN: %s can't add subdomain %s\n",
						pd->genpd.name, sub_pd->genpd.name);
		}
	}

	bus_register_notifier(&platform_bus_type, &platform_nb);

	pr_info("EXYNOS5: PM Domain Initialize\n");
	/* show information of power domain registration */
	show_power_domain();

	return 0;
}
#elif defined(CONFIG_RUNTIME_PM_V2)

/* show_power_domain - show current power domain status.
 *
 * read the status of power domain and show it.
 */
static void show_power_domain(void)
{
	struct exynos_pm_domain *pd_index = NULL;

#ifdef CONFIG_SOC_EXYNOS5260
	if(soc_is_exynos5260())
		pd_index = exynos5260_pm_domain();
#endif
        if (!pd_index){
                BUG();
                return;
        }
        
	for_each_power_domain(pd_index) {
		if (pd_index->enable) {
			pr_info("   %-9s - %-3s\n", pd_index->genpd.name,
					pd_index->check_status(pd_index) ? "on" : "off");
		} else
			pr_info("   %-9s - %s\n",
						kstrdup(pd_index->name, GFP_KERNEL),
						"on,  always");
	}

	return;
}

struct exynos_pm_domain * exynos_get_power_domain(const char * pd_name)
{
	struct exynos_pm_domain *pd_index = NULL;

#ifdef CONFIG_SOC_EXYNOS5260
	if (soc_is_exynos5260())
		pd_index = exynos5260_pm_domain();
#endif
        if (!pd_index){
                BUG();
                return ERR_PTR(-EINVAL);
        }

	for_each_power_domain(pd_index)
		if (!strcmp(pd_index->name, pd_name)) {
			DEBUG_PRINT_INFO("found %s\n", pd_index->name);
			return pd_index;
		}

	return ERR_PTR(-EINVAL);
}

static __init int exynos_pm_struct_parse_domains(void)
{
	struct exynos_pm_domain *pd_index = NULL;
	struct exynos_device_pd_link * dev_pd_link = NULL;

#ifdef CONFIG_SOC_EXYNOS5260
	if(soc_is_exynos5260())
		pd_index = exynos5260_pm_domain();
#endif
	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* master pd registration */
	for_each_power_domain(pd_index) {
		struct exynos_pm_domain *spd_index = NULL;

		if (!pd_index->enable) {
			/* disable sub domains when master is diabled */
			if (pd_index->sub_pd) {
				spd_index = pd_index->sub_pd;
				for_each_power_domain(spd_index) {
					spd_index->enable = false;
					pr_info(PM_DOMAIN_PREFIX "   disable spd: %s\n", spd_index->name);
				}
			}
			continue;
		}

		if (pd_index->cb) {
			if (pd_index->cb->on)
				pd_index->on = pd_index->cb->on;
			if (pd_index->cb->off)
				pd_index->off = pd_index->cb->off;
		}

		pr_info(PM_DOMAIN_PREFIX "pd: %s\n", pd_index->name);
		exynos_pm_powerdomain_init(pd_index);

		/* needs to build device list for each power domain */
		if (pd_index->sub_pd)
			spd_index = pd_index->sub_pd;
		else
			continue;

		/* sub pd registration */
		for_each_power_domain(spd_index) {
			if (!spd_index->enable)
				continue;

			if (spd_index->cb) {
				if (spd_index->cb->on)
					spd_index->on = spd_index->cb->on;
				if (spd_index->cb->off)
					spd_index->off = spd_index->cb->off;
			}

			pr_info(PM_DOMAIN_PREFIX "   spd: %s\n", spd_index->name);
			exynos_pm_powerdomain_init(spd_index);
			if (pm_genpd_add_subdomain(&pd_index->genpd, &spd_index->genpd))
				pr_err(PM_DOMAIN_PREFIX "%s cannot add subdomain %s\n",
						pd_index->genpd.name, spd_index->genpd.name);
		}

	}

	/* device registration */
#ifdef CONFIG_SOC_EXYNOS5260
	if(soc_is_exynos5260())
		dev_pd_link = exynos5260_device_pd_link();
#endif
	if (!dev_pd_link) {
		pr_err(PM_DOMAIN_PREFIX "data of device to pd link is not found\n");
		return -EINVAL;
	}

	pr_info(PM_DOMAIN_PREFIX "power domain to device\n");
	for_each_dev_pd_link(dev_pd_link) {
		if (dev_pd_link->status && dev_pd_link->pd->enable) {
			exynos_add_device_to_pd(dev_pd_link->pd, &dev_pd_link->pdev->dev);
		}
	}

	pr_info("EXYNOS5: PM Domain Initialize\n");
	/* show information of power domain registration */
	show_power_domain();

	return 0;
}

#endif

static int __init exynos5_pm_domain_init(void)
{
#ifdef CONFIG_OF
	if (of_have_populated_dt())
		return exynos_pm_dt_parse_domains();
#elif defined(CONFIG_RUNTIME_PM_V2)
	return exynos_pm_struct_parse_domains();
#endif

	pr_err(PM_DOMAIN_PREFIX "PM Domain works along with Device Tree\n");
	return -EPERM;
}
arch_initcall(exynos5_pm_domain_init);

static __init int exynos_pm_domain_idle(void)
{
	pr_info(PM_DOMAIN_PREFIX "Power off unused power domains.\n");
	pm_genpd_poweroff_unused();
	show_power_domain();

	return 0;
}
late_initcall_sync(exynos_pm_domain_idle);
