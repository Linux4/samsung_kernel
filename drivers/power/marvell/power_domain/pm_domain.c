#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/clk.h>
#include <linux/clk-private.h>

#include "pm_domain.h"

struct mmp_pd_dev_map {
	struct device *dev;
	struct device_node *pd_np;
	struct generic_pm_domain *genpd;
	struct list_head node;
};

struct mmp_pd_link {
	struct generic_pm_domain *genpd;
	struct device_node *pd_np;
	struct list_head node;
};

static LIST_HEAD(mmp_pd_dev_list);
static DEFINE_MUTEX(mmp_pd_dev_list_lock);

static LIST_HEAD(mmp_pd_link_list);
static DEFINE_MUTEX(mmp_pd_link_list_lock);

struct generic_pm_domain *clk_to_genpd(const char *name)
{
	struct generic_pm_domain *genpd = NULL, *gpd;
	struct mmp_pd_common *pd = NULL;

	if (IS_ERR_OR_NULL(name))
		return NULL;

	mutex_lock(&gpd_list_lock);
	list_for_each_entry(gpd, &gpd_list, gpd_list_node) {
		pd = container_of(gpd, struct mmp_pd_common, genpd);
		if (pd->tag == PD_TAG) {
			if (!strcmp(pd->clk->name, name)) {
				genpd = gpd;
				break;
			}
		}
	}
	mutex_unlock(&gpd_list_lock);

	return genpd;
}

static struct mmp_pd_link *mmp_pd_link_list_find(struct device_node *pd_np)
{
	struct mmp_pd_link *tmp, *link = NULL;

	mutex_lock(&mmp_pd_link_list_lock);
	list_for_each_entry(tmp, &mmp_pd_link_list, node) {
		if (tmp->pd_np == pd_np) {
			link = tmp;
			break;
		}
	}
	mutex_unlock(&mmp_pd_link_list_lock);

	return link;
}

static void mmp_pd_link_list_add(struct mmp_pd_link *link)
{
	mutex_lock(&mmp_pd_link_list_lock);
	list_add(&link->node, &mmp_pd_link_list);
	mutex_unlock(&mmp_pd_link_list_lock);
}

static struct mmp_pd_dev_map *mmp_pd_dev_list_del(struct device *dev)
{
	struct mmp_pd_dev_map *tmp, *map = NULL;

	mutex_lock(&mmp_pd_dev_list_lock);
	list_for_each_entry(tmp, &mmp_pd_dev_list, node) {
		if (tmp->dev == dev) {
			list_del(&tmp->node);
			map = tmp;
			break;
		}
	}
	mutex_unlock(&mmp_pd_dev_list_lock);

	return map;
}

static struct mmp_pd_dev_map *mmp_pd_dev_list_del_pending_by_pd(
					struct device_node *pd_np)
{
	struct mmp_pd_dev_map *tmp, *map = NULL;

	mutex_lock(&mmp_pd_dev_list_lock);
	list_for_each_entry(tmp, &mmp_pd_dev_list, node) {
		if (tmp->pd_np == pd_np && tmp->genpd == NULL) {
			list_del(&tmp->node);
			map = tmp;
			break;
		}
	}
	mutex_unlock(&mmp_pd_dev_list_lock);

	return map;
}


static int mmp_pd_dev_list_add(struct mmp_pd_dev_map *map)
{
	struct mmp_pd_dev_map *tmp;

	mutex_lock(&mmp_pd_dev_list_lock);
	list_for_each_entry(tmp, &mmp_pd_dev_list, node) {
		if (tmp->dev == map->dev) {
			mutex_unlock(&mmp_pd_dev_list_lock);
			return -EEXIST;
		}
	}
	list_add(&map->node, &mmp_pd_dev_list);
	mutex_unlock(&mmp_pd_dev_list_lock);

	return 0;
}


static int mmp_pd_add_device(struct platform_device *pdev)
{
	struct device_node *pd_np;
	struct generic_pm_domain *genpd;
	struct mmp_pd_dev_map *map;
	struct mmp_pd_link *link;
	int ret;

	/* Get device node of pd. */
	pd_np = of_parse_phandle(pdev->dev.of_node, "marvell,power-domain", 0);
	if (!pd_np)
		return -ENODEV;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return -ENOMEM;

	link = mmp_pd_link_list_find(pd_np);
	if (link)
		genpd = link->genpd;
	else
		genpd = NULL;

	map->dev = &pdev->dev;
	map->pd_np = pd_np;
	map->genpd = genpd;
	ret = mmp_pd_dev_list_add(map);
	if (ret) {
		dev_err(&pdev->dev,
			"Fail to add to dev list when do %s\n", __func__);
		kfree(map);
		return ret;
	}
	if (genpd) {
		ret = __pm_genpd_add_device(genpd, &pdev->dev, NULL);
		if (ret) {
			dev_err(&pdev->dev,
				"Fail to add to power domain when do %s\n",
				__func__);
			return ret;
		}
	}

	dev_info(map->dev, "Add to power domain %s\n",
		 genpd ? genpd->name : "Pending");

	return 0;
}

static int mmp_pd_remove_device(struct platform_device *pdev)
{
	struct generic_pm_domain *genpd = NULL;
	struct mmp_pd_dev_map *map = NULL;

	map = mmp_pd_dev_list_del(&pdev->dev);
	if (map) {
		genpd = map->genpd;
		kfree(map);
		/* The device has been added because genpd is not NULL. */
		if (genpd)
			return pm_genpd_remove_device(genpd, &pdev->dev);
	}

	return 0;
}


static int mmp_pd_notifier_call(struct notifier_block *nb,
					unsigned long event, void *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	switch (event) {
	case BUS_NOTIFY_UNBOUND_DRIVER:
		mmp_pd_remove_device(pdev);
		break;
	case BUS_NOTIFY_BIND_DRIVER:
		mmp_pd_add_device(pdev);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block mp_pd_platform_nb = {
	.notifier_call = mmp_pd_notifier_call,
};

int mmp_pd_init(struct generic_pm_domain *genpd,
		struct dev_power_governor *gov, bool is_off)
{
	struct device_node *pd_np, *pd_np_parent;
	struct generic_pm_domain *genpd_parent = NULL;
	struct mmp_pd_dev_map *map = NULL;
	struct mmp_pd_link *link;
	int ret;

	if (!genpd || !genpd->of_node)
		return -EINVAL;

	pd_np = genpd->of_node;
	/* Check whether it is a subdomain. */
	pd_np_parent = of_parse_phandle(pd_np,
					"marvell,power-domain-parent", 0);
	if (pd_np_parent) {
		link = mmp_pd_link_list_find(pd_np_parent);
		if (!link)
			return -EPROBE_DEFER;
		genpd_parent = link->genpd;
	}

	pm_genpd_init(genpd, gov, is_off);

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (!link)
		return -ENOMEM;

	link->genpd = genpd;
	link->pd_np = pd_np;
	mmp_pd_link_list_add(link);

	if (genpd_parent) {
		ret = pm_genpd_add_subdomain(genpd_parent, genpd);
		/*
		 * Do nothing for error because pm_genpd_init can not be
		 * reverted.
		 */
		if (ret) {
			pr_err("%s: fail to add to pm domain %s, error %d\n",
			       genpd->name, genpd_parent->name, ret);
			return ret;
		}
		pr_info("%s: add to pm domain %s\n",
			genpd->name, genpd_parent->name);
	}

	do {
		map = mmp_pd_dev_list_del_pending_by_pd(pd_np);
		if (!map)
			break;

		map->genpd = genpd;
		ret = mmp_pd_dev_list_add(map);
		/* Someone has add the device when we release the lock. */
		if (ret)
			continue;

		ret = __pm_genpd_add_device(genpd, map->dev, NULL);
		if (ret)
			return ret;
		pr_info("%s: add device %s lately\n",
			genpd->name, dev_name(map->dev));
	} while (map);

	return 0;
}

static int __init mmp_pd_initcall(void)
{
	bus_register_notifier(&platform_bus_type, &mp_pd_platform_nb);

	return 0;
}

subsys_initcall(mmp_pd_initcall);
