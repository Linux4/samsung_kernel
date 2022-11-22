/*
 * Core driver for PXA DS1WM chip.
 *
 * Copyright (C) 2010 Marvell  <jtzhou@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/ds1wm.h>


struct pxa_w1_info {
	unsigned int bus_shift;
	struct clk *clk;
};
/*
 * DS1WM
 */
static int ds1wm_enable(struct platform_device *pdev)
{
	struct device *dev = pdev->dev.parent;
	struct pxa_w1_info *pxa_w1 = dev_get_drvdata(dev);

	clk_enable(pxa_w1->clk);

	dev_dbg(dev, "pxa DS1WM clk (active)\n");
	return 0;
}

static int ds1wm_disable(struct platform_device *pdev)
{
	struct device *dev = pdev->dev.parent;
	struct pxa_w1_info *pxa_w1 = dev_get_drvdata(dev);

	clk_disable(pxa_w1->clk);

	dev_dbg(dev, "pxa DS1WM clk (in-active)\n");
	return 0;
}

static struct ds1wm_driver_data ds1wm_pdata = {
	.active_high = 0,
};

static struct resource ds1wm_resources[] = {
	[0] = {
	       .start = 0,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ,
	       },
};

static struct mfd_cell ds1wm_cell = {
	.name = "ds1wm",
	.enable = ds1wm_enable,
	.disable = ds1wm_disable,
	.platform_data = &ds1wm_pdata,
	.pdata_size    = sizeof(ds1wm_pdata),
	.num_resources = ARRAY_SIZE(ds1wm_resources),
	.resources = ds1wm_resources,
};

static int __devinit pxa_w1_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pxa_w1_info *pxa_w1;
	struct resource *r;
	int ret;
	int irq = 0;

	pxa_w1 = kzalloc(sizeof(struct pxa_w1_info), GFP_KERNEL);
	if (!pxa_w1)
		return -ENOMEM;

	r = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (r) {
		ds1wm_resources[1].flags = IORESOURCE_IRQ | (r->flags &
			(IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE));
		irq = r->start;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		ret = -ENXIO;
		goto out;
	}

	pxa_w1->clk = clk_get(&pdev->dev, "PXA-W1");
	if (IS_ERR(pxa_w1->clk)) {
		ret = PTR_ERR(pxa_w1->clk);
		goto out;
	}
	platform_set_drvdata(pdev, pxa_w1);

	/* calculate bus shift from mem resource */
	pxa_w1->bus_shift = 2;

	ds1wm_pdata.clock_rate = clk_get_rate(pxa_w1->clk);
	ds1wm_pdata.active_high = 1;
	ds1wm_resources[0].end = (5 << pxa_w1->bus_shift) - 1;
	ret = mfd_add_devices(&pdev->dev, pdev->id,
			      &ds1wm_cell, 1, r, irq);
	if (ret < 0)
		dev_warn(dev, "failed to register pxa DS1WM\n");

	return 0;
out:
	kfree(pxa_w1);
	return ret;
}

static int __devexit pxa_w1_remove(struct platform_device *pdev)
{
	struct pxa_w1_info *pxa_w1 = platform_get_drvdata(pdev);

	mfd_remove_devices(&pdev->dev);
	kfree(pxa_w1);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver pxa_w1_driver = {
	.driver = {
		   .name	= "pxa-w1",
		   .owner	= THIS_MODULE,
	},
	.probe	= pxa_w1_probe,
	.remove = __devexit_p(pxa_w1_remove),
};

static int __init pxa_w1_base_init(void)
{
	return platform_driver_register(&pxa_w1_driver);
}

static void __exit pxa_w1_base_exit(void)
{
	platform_driver_unregister(&pxa_w1_driver);
}

module_init(pxa_w1_base_init);
module_exit(pxa_w1_base_exit);

MODULE_AUTHOR("Jett Zhou <jtzhou@marvell.com>");
MODULE_DESCRIPTION("one wire driver for PXA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pxa-w1");
