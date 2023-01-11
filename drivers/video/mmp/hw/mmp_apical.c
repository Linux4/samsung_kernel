/*
 * linux/drivers/video/mmp/hw/mmp_apical.c
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Guoqing Li <ligq@marvell.com>
 *           Zhou Zhu <zzhu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include "mmp_ctrl.h"
#include "mmp_apical.h"

static struct mmp_apical *apical;

static void apical_set_on(struct mmp_apical_info *apical_info, int on)
{
	/* FIXME: needed for configuration */
	u32 val;

	mutex_lock(&apical_info->access_ok);
	val = readl_relaxed(apical->lcd_reg_base + dma_ctrl2(apical_info->id));
	if (on)
		val |= APICAL_EN;
	else
		val &= ~APICAL_EN;
	writel_relaxed(val, apical->lcd_reg_base + dma_ctrl2(apical_info->id));
	apical_info->status = on;
	mutex_unlock(&apical_info->access_ok);
}

struct mmp_apical_ops apical_ops = {
	.set_on = apical_set_on,
};

struct mmp_apical_info *mmp_apical_alloc(int path_id)
{
	static int version_init;

	if (!apical)
		return NULL;
	if (!version_init) {
		apical->version = readl_relaxed(apical->lcd_reg_base +
				LCD_VERSION);
		version_init = 1;
	}

	if (!APICAL_GEN4)
		return NULL;

	if (path_id >= apical->apical_channel_num) {
		dev_warn(apical->dev,
			"%s: no apical available for the path:%d\n",
			__func__, path_id);
		return NULL;
	}
	apical->apical_info[path_id].id = path_id;

	return &apical->apical_info[path_id];
}

static void apical_init(struct mmp_mach_apical_info *mi)
{
	int i;

	apical->apical_channel_num = mi->apical_channel_num;
	apical->name = mi->name;
	for (i = 0; i < mi->apical_channel_num; i++) {
		apical->apical_info[i].ops = &apical_ops;
		mutex_init(&apical->apical_info[i].access_ok);
	}
}

static int mmp_apical_probe(struct platform_device *pdev)
{
	struct mmp_mach_apical_info *mi;
	struct mmp_mach_apical_info dt_mi;
	struct device_node *np = pdev->dev.of_node;
	struct resource *res0, *res1;
	int ret = 0;

	/* get resources from platform data */
	res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res0 == NULL || res1 == NULL) {
		dev_err(&pdev->dev, "%s: no IO memory defined\n", __func__);
		return -ENOENT;
	}

	if (IS_ENABLED(CONFIG_OF)) {
		if (of_property_read_u32(np, "marvell,apical-num",
					&dt_mi.apical_channel_num)) {
			dev_err(&pdev->dev, "%s: apical get num fail\n",
					__func__);
			return -EINVAL;
		}
		mi = &dt_mi;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "%s: no platform data defined\n",
					__func__);
			return -EINVAL;
		}
	}

	apical = devm_kzalloc(&pdev->dev, sizeof(struct mmp_apical) +
			sizeof(struct mmp_apical_info) * mi->apical_channel_num,
			GFP_KERNEL);
	if (apical == NULL) {
		dev_err(&pdev->dev, "apical alloc fail\n");
		return -ENOMEM;
	}
	apical_init(mi);
	apical->dev = &pdev->dev;

	/* map registers.*/
	if (!devm_request_mem_region(apical->dev, res0->start,
			resource_size(res0), apical->name)) {
		dev_err(apical->dev,
			"can't request region for resource %pR\n", res0);
		ret = -EINVAL;
		goto mem_fail;
	}

	apical->reg_base = devm_ioremap_nocache(apical->dev,
			res0->start, resource_size(res0));
	if (apical->reg_base == NULL) {
		dev_err(apical->dev, "%s: res0%lx - %lx map failed\n", __func__,
			(unsigned long)res0->start, (unsigned long)res0->end);
		ret = -ENOMEM;
		goto ioremap_fail;
	}

	apical->lcd_reg_base = devm_ioremap_nocache(apical->dev,
			res1->start, resource_size(res1));
	if (apical->lcd_reg_base == NULL) {
		dev_err(apical->dev, "%s: res1%lx - %lx map failed\n", __func__,
			(unsigned long)res1->start, (unsigned long)res1->end);
		ret = -ENOMEM;
		goto ioremap1_fail;
	}
	platform_set_drvdata(pdev, apical);
	ret = apical_dbg_init(apical->dev);
	if (ret < 0) {
		dev_err(apical->dev, "%s: Failed to register apical dbg interface\n", __func__);
		goto ioremap1_fail;
	}

	dev_info(&pdev->dev, "apical probe succeed\n");

	return 0;

ioremap1_fail:
	devm_iounmap(&pdev->dev, apical->reg_base);
ioremap_fail:
	devm_release_mem_region(&pdev->dev, res0->start,
			resource_size(res0));
mem_fail:
	devm_kfree(&pdev->dev, apical);
	dev_err(&pdev->dev, "apical device init failed\n");

	return ret;
}

static int mmp_apical_remove(struct platform_device *pdev)
{
	struct mmp_apical *apical = platform_get_drvdata(pdev);
	struct resource *res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct resource *res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (res0 == NULL || res1 == NULL) {
		dev_err(&pdev->dev, "%s: no IO memory defined\n", __func__);
		return -ENOENT;
	}

	apical_dbg_uninit(apical->dev);
	devm_iounmap(apical->dev, apical->lcd_reg_base);
	devm_iounmap(apical->dev, apical->reg_base);
	devm_release_mem_region(apical->dev, res0->start,
			resource_size(res0));
	devm_kfree(apical->dev, apical);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_apical_dt_match[] = {
	{ .compatible = "marvell,mmp-apical" },
	{},
};
#endif

static struct platform_driver mmp_apical_driver = {
	.probe          = mmp_apical_probe,
	.remove         = mmp_apical_remove,
	.driver         = {
		.name   = "mmp-apical",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_apical_dt_match),
	},
};

int mmp_apical_register(void)
{
	return platform_driver_register(&mmp_apical_driver);
}

void mmp_apical_unregister(void)
{
	return platform_driver_unregister(&mmp_apical_driver);
}

MODULE_AUTHOR("Guoqing Li<ligq@marvell.com>");
MODULE_DESCRIPTION("mmp apical driver");
MODULE_LICENSE("GPL");
