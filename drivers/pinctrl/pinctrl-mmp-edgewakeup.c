/*
 * edge wakeup interface for MMP
 *
 * The GPIO Edge is the edge detect signals coming from the I/O pads.
 * Although the name of this module is the GPIO Edge Unit, it can be
 * used by other I/Os as it is not necessarily for use only by the
 * GPIOs. It's normally used to wake up the system from low power mode.
 *
 * Copyright:   (C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/edge_wakeup_mmp.h>
#include "core.h"
/*
 * struct edge_wakeup_desc - edge wakeup source descriptor, used for drivers
 * whose pin is used to wakeup system.
 * @list:	list control of the descriptors
 * @gpio:	the gpio number of the wakeup source
 * @dev:	the device that gpio attaches to
 * @data:	optional, any kind private data passed to the handler
 * @handler:	optional, the handler for the certain wakeup source detected
 * @state:	used to save/restore pin state in LPM enter/exit
 */
struct edge_wakeup_desc {
	struct list_head	list;
	int			gpio;
	struct device		*dev;
	void			*data;
	edge_handler		handler;
	const char		*state_name;
};

struct edge_wakeup {
	struct list_head list;
	spinlock_t	lock;
	int num;
	void __iomem *base;
	int enabled;
};

static struct edge_wakeup *info;
static BLOCKING_NOTIFIER_HEAD(mfp_edge_wakeup_notifier);

int register_mfp_edge_wakup_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mfp_edge_wakeup_notifier, nb);
}
EXPORT_SYMBOL_GPL(register_mfp_edge_wakup_notifier);

int unregister_mfp_edge_wakeup_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mfp_edge_wakeup_notifier,
						  nb);
}
EXPORT_SYMBOL_GPL(unregister_mfp_edge_wakeup_notifier);

int mfp_edge_wakeup_notifier_call_chain(unsigned long val)
{
	int ret = blocking_notifier_call_chain(&mfp_edge_wakeup_notifier,
					       val, NULL);

	return notifier_to_errno(ret);
}

/*
 * mmp_request/remove_edge_wakeup is called by common device driver.
 *
 * Drivers use it to set one or several pins as wakeup sources in deep low
 * power modes.
 */
int request_mfp_edge_wakeup(int gpio, edge_handler handler, \
			void *data, struct device *dev)
{
	struct edge_wakeup_desc *desc, *e;
	unsigned long flags;

	if (dev == NULL) {
		pr_err("error: edge wakeup: unknown device!\n");
		return -EINVAL;
	}

	if (gpio < 0 || gpio > info->num) {
		pr_err("error: edge wakeup: add invalid gpio num!\n");
		return -EINVAL;
	}

	desc = kzalloc(sizeof(struct edge_wakeup_desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->gpio = gpio;
	desc->dev = dev;
	desc->data = data;
	desc->handler = handler;

	spin_lock_irqsave(&info->lock, flags);

	list_for_each_entry(e, &info->list, list) {
		if (e->gpio == gpio) {
			dev_err(dev, "Adding exist gpio%d to edge wakeup!\n", desc->gpio);
			spin_unlock_irqrestore(&info->lock, flags);
			kfree(desc);
			return -EEXIST;
		}
	}

	list_add(&desc->list, &info->list);

	spin_unlock_irqrestore(&info->lock, flags);

	/* Notify there is one GPIO added for wakeup */
	mfp_edge_wakeup_notifier_call_chain(gpio);

	return 0;
}
EXPORT_SYMBOL_GPL(request_mfp_edge_wakeup);

int edge_wakeup_mfp_status(unsigned long *edge_reg)
{
	int i;

	for (i = 0; i < (info->num / BITS_PER_LONG); i++) {
		/*
		 * 64 and 32bits have different bytes length for "unsigned
		 * *long". For 64bits, need to combine regs value for edgew
		 * _rer[i]. Otherwise, the upper 32bits of edgew_rer[i]
		 * would be zero and test_and_clear_bit(e->gpio, edgew_rer)
		 * will find the wrong bit.
		 */
#ifdef CONFIG_64BIT
		unsigned long low_bits, high_bits;
		low_bits = readl_relaxed(info->base + (2 * i) * 4);
		high_bits = readl_relaxed(info->base + (2 * i + 1) * 4);
		edge_reg[i] = low_bits | high_bits << 32;
#else
		edge_reg[i] = readl_relaxed(info->base + i * 4);
#endif
	}

	return info->num / BITS_PER_LONG;
}
EXPORT_SYMBOL_GPL(edge_wakeup_mfp_status);

int remove_mfp_edge_wakeup(int gpio)
{
	struct edge_wakeup_desc *e;
	unsigned long flags;

	if (gpio < 0 || gpio > info->num) {
		pr_err("error: edge wakeup: remove invalid gpio num!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&info->lock, flags);

	list_for_each_entry(e, &info->list, list) {
		if (e->gpio == gpio) {
			list_del(&e->list);
			spin_unlock_irqrestore(&info->lock, flags);
			kfree(e);
			return 0;
		}
	}

	spin_unlock_irqrestore(&info->lock, flags);

	pr_err("error: edge wakeup: del none exist gpio:%d!\n", gpio);
	return -ENXIO;
}
EXPORT_SYMBOL_GPL(remove_mfp_edge_wakeup);

/*
 * edge_wakeup_mfp_enable/disable is called by low power mode driver.
 *
 * edge_wakeup_mfp_enable Enable each gpio edge wakeup source in the list. The
 * corresponing interrupt and wakeup port also should be set, which is done in
 * low power mode driver.
 */
void edge_wakeup_mfp_enable(void)
{
	struct edge_wakeup_desc *e;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_sleep;
	int ret;

	spin_lock(&info->lock);

	if (list_empty(&info->list)) {
		spin_unlock(&info->lock);
		return;
	}

	list_for_each_entry(e, &info->list, list) {
		pinctrl = pinctrl_get(e->dev);
		if (IS_ERR(pinctrl)) {
			dev_err(e->dev, "Could not get pinctrl!\n");
			continue;
		}

		/*
		 * If the pin has already set to its sleep state, (done by
		 * other pins in its group), then should not configure it
		 * again or its e->state_name will get the "sleep" value
		 * which is not expected.
		 */
		if (!strcmp(pinctrl->state->name, "sleep")) {
			pinctrl_put(pinctrl);
			continue;
		}

		pin_sleep = pinctrl_lookup_state(pinctrl, PINCTRL_STATE_SLEEP);
		if (IS_ERR(pin_sleep)) {
			dev_err(e->dev, "Could not get sleep pinstate!\n");
			pinctrl_put(pinctrl);
			continue;
		}
		e->state_name = pinctrl->state->name;

		ret = pinctrl_select_state(pinctrl, pin_sleep);
		if (ret)
			dev_err(e->dev, "Could not set pins to sleep state!\n");
		pinctrl_put(pinctrl);
	}

	info->enabled = 1;

	spin_unlock(&info->lock);
}
EXPORT_SYMBOL_GPL(edge_wakeup_mfp_enable);

void edge_wakeup_mfp_disable(void)
{
	struct edge_wakeup_desc *e;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_def;
	unsigned long *edgew_rer;
	int i, ret;
	/*
	 * check info when board boots up, at which point edge wakeup driver
	 * is not ready yet.
	 */
	if (info == NULL) {
		pr_err("Edge wakeup driver is not initilized!\n");
		return;
	}

	edgew_rer = kzalloc(info->num / 8 , GFP_KERNEL);
	if (!edgew_rer) {
		pr_err("Can't allocate enough memory for edge detection status!\n");
		return;
	}

	spin_lock(&info->lock);

	if (!info->enabled) {
		spin_unlock(&info->lock);
		kfree(edgew_rer);
		return;
	}

	edge_wakeup_mfp_status(edgew_rer);

	list_for_each_entry(e, &info->list, list) {
		if (test_and_clear_bit(e->gpio, edgew_rer) && e->handler)
			e->handler(e->gpio, e->data);

		pinctrl = pinctrl_get(e->dev);
		if (IS_ERR(pinctrl)) {
			dev_err(e->dev, "Could not get pinctrl!\n");
			continue;
		}
		/*
		 * If the pin has restored to its previous state before lpm
		 * (done by other pins in its group), then should not restore
		 * it again or it'll overwrite other pins' configurations in
		 * its group.
		 */
		if (strcmp(pinctrl->state->name, "sleep")) {
			pinctrl_put(pinctrl);
			continue;
		}

		pin_def  = pinctrl_lookup_state(pinctrl, e->state_name);
		if (IS_ERR(pin_def)) {
			dev_err(e->dev, "Could not get default pinstate!\n");
			pinctrl_put(pinctrl);
			continue;
		}
		ret = pinctrl_select_state(pinctrl, pin_def);
		if (ret)
			dev_err(e->dev, "Could not set pins to default state!\n");
		pinctrl_put(pinctrl);
	}

	info->enabled = 0;

	spin_unlock(&info->lock);

	i = find_first_bit(edgew_rer, info->num);
	while (i < info->num) {
		pr_err("error: edge wakeup: unexpected detect gpio%d wakeup!\n", i);
		i = find_next_bit(edgew_rer, info->num, i + 1);
	}

	kfree(edgew_rer);
	return;
}
EXPORT_SYMBOL_GPL(edge_wakeup_mfp_disable);

static int edge_wakeup_mfp_probe(struct platform_device *pdev)
{
	void __iomem *base;
	int size;

	if (IS_ENABLED(CONFIG_OF)) {
		struct device_node *np = pdev->dev.of_node;
		const __be32 *prop = of_get_property(np, "reg", NULL);

		base = of_iomap(np, 0);
		if (!base) {
			dev_err(&pdev->dev, "Fail to map base address\n");
			return -EINVAL;
		}

		size = be32_to_cpup(prop + 1);
	} else {
		struct resource *res;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (res == NULL) {
			dev_err(&pdev->dev, "no memory resource defined\n");
			return -ENODEV;
		}

		base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(base))
			return PTR_ERR(base);

		size = resource_size(res);
	}

	info = devm_kzalloc(&pdev->dev, sizeof(struct edge_wakeup), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->base = base;
	/* size represents bytes num, and each bit represents a mfp */
	info->num = size * 8;
	info->enabled = 0;
	spin_lock_init(&info->lock);
	INIT_LIST_HEAD(&info->list);

	platform_set_drvdata(pdev, info);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id edge_wakeup_mfp_dt_ids[] = {
	{ .compatible = "mrvl,mmp-edge-wakeup", },
	{}
};
MODULE_DEVICE_TABLE(of, edge_wakeup_mfp_dt_ids);
#endif

static struct platform_driver edge_wakeup_driver = {
	.probe		= edge_wakeup_mfp_probe,
	.driver		= {
		.name	= "mmp-edge-wakeup",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(edge_wakeup_mfp_dt_ids),
	},
};

static int __init edge_wakeup_driver_init(void)
{
	return platform_driver_register(&edge_wakeup_driver);
}

subsys_initcall(edge_wakeup_driver_init);

MODULE_AUTHOR("Fangsuo Wu <fswu@marvell.com>");
MODULE_DESCRIPTION("MMP Edge Wakeup Driver");
MODULE_LICENSE("GPL v2");
