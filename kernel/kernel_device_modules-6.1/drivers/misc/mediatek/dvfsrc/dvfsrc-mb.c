// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "dvfsrc-mb.h"

static struct mtk_dvfsrc_mb *dvfsrc_drv;
static uint32_t data_version;

static inline u32 dvfsrc_read(struct mtk_dvfsrc_mb *dvfs, u32 offset)
{
	return readl(dvfs->regs + offset);
}

void dvfsrc_mt6989_get_data(struct mtk_dvfsrc_header *header)
{
	header->module_id = dvfsrc_drv->dvd->module_id;
	header->data_offset = dvfsrc_drv->dvd->data_offset;
	header->data_length = dvfsrc_drv->dvd->data_length;
	header->version = data_version;

	header->data[0] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_1);
	header->data[1] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_2);
	header->data[2] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_3);
	header->data[3] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_4);
	header->data[4] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_5);
	header->data[5] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_6);

	dev_info(dvfsrc_drv->dev, "%s %d-%d-%d-%d\n", __func__,
		header->module_id, header->version,
		header->data_offset,header->data_length);

	dev_info(dvfsrc_drv->dev, "%s %x-%x-%x-%x-%x-%x\n", __func__,
		header->data[0], header->data[1], header->data[2],
		header->data[3], header->data[4], header->data[5]);
}

void dvfsrc_get_data(struct mtk_dvfsrc_header *header)
{
	if (dvfsrc_drv == NULL) {
		header->data_length = 0;
		return;
	}

	dvfsrc_drv->dvd->config->get_data(header);
}
EXPORT_SYMBOL(dvfsrc_get_data);

const struct mtk_dvfsrc_config mt6989_config = {
	.get_data = &dvfsrc_mt6989_get_data,
};

static const struct mtk_dvfsrc_data mt6989_data = {
	.module_id = 2,
	.data_offset = 0,
	.data_length = sizeof(struct mtk_dvfsrc_header),
	.config = &mt6989_config,
};

static const struct of_device_id dvfsrc_mdv_of_match[] = {
	{
		.compatible = "mediatek,mt6989-dvfsrc",
		.data = &mt6989_data,
	}, {
		/* sentinel */
	},
};

static int dvfsrc_mb_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct platform_device *parent_dev;
	struct resource *res;
	struct mtk_dvfsrc_mb *dvfsrc;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dev = &pdev->dev;

	parent_dev = to_platform_device(dev->parent);

	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "dvfsrc");
	if (!res) {
		dev_info(dev, "dvfsrc debug resource not found\n");
		return -ENODEV;
	}

	dvfsrc->regs = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	platform_set_drvdata(pdev, dvfsrc);

	match = of_match_node(dvfsrc_mdv_of_match, dev->parent->of_node);
	if (!match) {
		dvfsrc_drv = NULL;
		dev_info(dev, "invalid compatible string\n");
		return -ENODEV;
	}

	dvfsrc->dvd = match->data;

	dvfsrc_drv = dvfsrc;

	data_version = dvfsrc_read(dvfsrc, DVFSRC_RSV_0);

	return 0;
}

static const struct of_device_id mtk_dvfsrc_mb_of_match[] = {
	{
		.compatible = "mediatek,dvfsrc-mb",
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_mb_driver = {
	.probe	= dvfsrc_mb_probe,
	.driver = {
		.name = "mtk-mb",
		.of_match_table = of_match_ptr(mtk_dvfsrc_mb_of_match),
	},
};

int __init mtk_dvfsrc_mb_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_mb_driver);
}

void __exit mtk_dvfsrc_mb_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_mb_driver);
}
