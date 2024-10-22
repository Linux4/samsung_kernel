// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <dbgtop.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>
#include "sda.h"

#define DRM_REG_INVALID_INDEX 0xDEADDEAD

/* global pointer for exported functions */
static struct dbgtop_drm *global_dbgtop_drm;
static unsigned int is_sec_write;

static unsigned long rg_white_list[] = {
	MTK_DBGTOP_MODE,
	MTK_DBGTOP_DEBUG_CTL,
	MTK_DBGTOP_DEBUG_CTL2,
	MTK_DBGTOP_DEBUG_CTL3,
	MTK_DBGTOP_LATCH_CTL,
	MTK_DBGTOP_LATCH_CTL2,
	MTK_DBGTOP_LATCH_CTL3,
	MTK_DBGTOP_LATCH_CTL4,
	MTK_DBGTOP_LATCH_CTL5,
	MTK_DBGTOP_MFG_REG
};

static unsigned long mtk_drm_get_white_list_index(unsigned long offset)
{
	unsigned long index;

	for (index = 0; index < ARRAY_SIZE(rg_white_list); index++) {
		if (rg_white_list[index] == offset)
			return index;
	}
	return DRM_REG_INVALID_INDEX;
}

static int mtk_dbgtop_drm_update_status(unsigned long offset, unsigned int val)
{
	unsigned long index;
	struct arm_smccc_res res;

	if (!global_dbgtop_drm) {
		pr_info("%s: global_dbgtop_drm has not been initialized yet.\n", __func__);
		return -EFAULT;
	}
	if (is_sec_write) {
		index = mtk_drm_get_white_list_index(offset);
		if (index >= ARRAY_SIZE(rg_white_list)) {
			pr_info("%s: not support writing to (base + 0x%lx).\n", __func__, offset);
			return -1;
		}
		arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_DBGTOP_DRM, DRM_SET_STATUS,
				index, val, 0, 0, 0, &res);
		if (res.a0) {
			pr_info("%s: drm reg update failed.\n", __func__);
			return -EFAULT;
		}
	} else {
		writel(val, global_dbgtop_drm->base + offset);
	}

	return 0;
}

/* For GPU DFD */
int mtk_dbgtop_mfg_pwr_on(int value)
{
	struct dbgtop_drm *drm;
	unsigned int tmp;

	if (!global_dbgtop_drm)
		return -1;

	drm = global_dbgtop_drm;

	if (value == 1) {
		/* set mfg pwr on */
		tmp = readl(drm->base + MTK_DBGTOP_MFG_REG);
		tmp |= MTK_DBGTOP_MFG_PWR_ON;
		tmp |= MTK_DBGTOP_MFG_REG_KEY;
		mtk_dbgtop_drm_update_status(MTK_DBGTOP_MFG_REG, tmp);
	} else if (value == 0) {
		tmp = readl(drm->base + MTK_DBGTOP_MFG_REG);
		tmp &= ~MTK_DBGTOP_MFG_PWR_ON;
		tmp |= MTK_DBGTOP_MFG_REG_KEY;
		mtk_dbgtop_drm_update_status(MTK_DBGTOP_MFG_REG, tmp);
	} else
		return -1;

	pr_info("%s: MTK_DBGTOP_MFG_REG(0x%x)\n", __func__,
			readl(drm->base + MTK_DBGTOP_MFG_REG));
	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_mfg_pwr_on);

/* For GPU DFD */
int mtk_dbgtop_mfg_pwr_en(int value)
{
	struct dbgtop_drm *drm;
	unsigned int tmp;

	if (!global_dbgtop_drm)
		return -1;

	drm = global_dbgtop_drm;

	if (value == 1) {
		/* set mfg pwr en */
		tmp = readl(drm->base + MTK_DBGTOP_MFG_REG);
		tmp |= MTK_DBGTOP_MFG_PWR_EN;
		tmp |= MTK_DBGTOP_MFG_REG_KEY;
		mtk_dbgtop_drm_update_status(MTK_DBGTOP_MFG_REG, tmp);
	} else if (value == 0) {
		tmp = readl(drm->base + MTK_DBGTOP_MFG_REG);
		tmp &= ~MTK_DBGTOP_MFG_PWR_EN;
		tmp |= MTK_DBGTOP_MFG_REG_KEY;
		mtk_dbgtop_drm_update_status(MTK_DBGTOP_MFG_REG, tmp);
	} else
		return -1;

	pr_info("%s: MTK_DBGTOP_MFG_REG(0x%x)\n", __func__,
		readl(drm->base + MTK_DBGTOP_MFG_REG));
	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_mfg_pwr_en);

/*
 * Set the required timeout value of each caller before RGU reset,
 * and take the maximum as timeout value.
 * @value_abnormal: timeout value for exception reboot
 * @value_normal: timeout value for normal reboot
 * Note: caller needs to set normal timeout value to 0 by default
 */
int mtk_dbgtop_dfd_timeout(int value_abnormal, int value_normal)
{
	struct dbgtop_drm *drm;
	unsigned int tmp;

	if (!global_dbgtop_drm)
		return -1;

	drm = global_dbgtop_drm;

	value_abnormal <<= MTK_DBGTOP_DFD_TIMEOUT_SHIFT;
	value_abnormal &= MTK_DBGTOP_DFD_TIMEOUT_MASK;

	/* break if dfd timeout >= target value_abnormal */
	tmp = readl(drm->base + MTK_DBGTOP_LATCH_CTL2);
	if ((tmp & MTK_DBGTOP_DFD_TIMEOUT_MASK) >=
		(unsigned int)value_abnormal)
		return 0;

	/* set dfd timeout */
	tmp &= ~MTK_DBGTOP_DFD_TIMEOUT_MASK;
	tmp |= value_abnormal | MTK_DBGTOP_LATCH_CTL2_KEY;
	mtk_dbgtop_drm_update_status(MTK_DBGTOP_LATCH_CTL2, tmp);

	pr_debug("%s: MTK_DBGTOP_LATCH_CTL2(0x%x)\n", __func__,
		readl(drm->base + MTK_DBGTOP_LATCH_CTL2));

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_dfd_timeout);

static int mtk_dbgtop_drm_probe(struct platform_device *pdev)
{
	struct dbgtop_drm *drm;
	struct device_node *node = pdev->dev.of_node;

	dev_info(&pdev->dev, "driver probed\n");

	if (!node)
		return -ENXIO;

	is_sec_write = 0;
	if (of_property_read_u32(node, "sec-write", &is_sec_write))
		dev_info(&pdev->dev, "%s: sec-write not found, use default value 0.\n", __func__);
	else
		dev_info(&pdev->dev, "%s: found sec-write = %d.\n", __func__, is_sec_write);

	drm = devm_kmalloc(&pdev->dev, sizeof(struct dbgtop_drm), GFP_KERNEL);
	if (!drm)
		return -ENOMEM;

	drm->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(drm->base))
		return PTR_ERR(drm->base);

	global_dbgtop_drm = drm;

	dev_info(&pdev->dev, "base %p\n", drm->base);

	return 0;
}

static int mtk_dbgtop_drm_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");

	global_dbgtop_drm = NULL;

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mtk_dbgtop_drm_of_ids[] = {
	{.compatible = "mediatek,dbgtop-drm",},
	{}
};
#endif

static struct platform_driver mtk_dbgtop_drm = {
	.probe = mtk_dbgtop_drm_probe,
	.remove = mtk_dbgtop_drm_remove,
	.driver = {
		.name = "mtk_dbgtop_drm",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = mtk_dbgtop_drm_of_ids,
#endif
	},
};

static int __init mtk_dbgtop_drm_init(void)
{
	int ret;

	pr_info("mtk_dbgtop_drm was loaded\n");

	ret = platform_driver_register(&mtk_dbgtop_drm);
	if (ret) {
		pr_info("mtk_dbgtop_drm: failed to register driver");
		return ret;
	}

	return 0;
}

module_init(mtk_dbgtop_drm_init);

MODULE_DESCRIPTION("MediaTek DBGTOP-DRM Driver");
MODULE_LICENSE("GPL v2");
