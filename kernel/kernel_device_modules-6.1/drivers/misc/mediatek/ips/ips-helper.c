// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "ips-helper.h"
#include <linux/soc/mediatek/mtk_sip_svc.h>

#define IPS_E_01                 (IPS_E_BASE + 0x0)
#define IPS_E_05                 (IPS_E_BASE + 0x10)
#define IPS_E_06                 (IPS_E_BASE + 0x14)
#define IPS_E_10                 (IPS_E_BASE + 0x24)
#define IPS_E_12                 (IPS_E_BASE + 0x2C)
#define IPS_E_13                 (IPS_E_BASE + 0x30)

#define MTK_SIP_VCOREFS_IPS_ENABLE  0x90
#define MTK_SIP_VCOREFS_IPS_GET_VMIN  0x91
#define MTK_SIP_VCOREFS_IPS_GET_VMIN_CLEAR  0x92

enum ips_regs_id {
	IPS_01,
	IPS_02,
	IPS_03,
	IPS_04,
	IPS_05,
	IPS_06,
	IPS_07,
	IPS_08,
	IPS_09,
	IPS_10,
	IPS_11,
	IPS_12,
	IPS_13,
};

static const int mt6985_regs[] = {
	[IPS_01] = 0x0,
	[IPS_02] = 0x4,
	[IPS_03] = 0x8,
	[IPS_04] = 0xC,
	[IPS_05] = 0x10,
	[IPS_06] = 0x14,
	[IPS_07] = 0x18,
	[IPS_08] = 0x1C,
	[IPS_09] = 0x20,
	[IPS_10] = 0x24,
	[IPS_11] = 0x28,
	[IPS_12] = 0x2C,
	[IPS_13] = 0x30,
};

static u32 ips_read(struct ips_drv_data *ips, u32 offset)
{
	return readl(ips->regs + ips->dvd->regs[offset]);
}

static u32 dvfsrc_read_rsrv5(struct ips_drv_data *ips)
{
	return readl(ips->dvfsrc_regs + 0x294);
}

static void ips_write(struct ips_drv_data *ips, u32 offset, u32 val)
{
	writel(val, ips->regs + ips->dvd->regs[offset]);
}

#define ips_rmw(dvfs, offset, val, mask, shift) \
	ips_write(dvfs, offset, \
		(ips_read(dvfs, offset) & ~(mask << shift)) | (val << shift))

static int set_ips_ck_enable(struct ips_drv_data *drv_data)
{
	struct clk *ips_clk = drv_data->ips_clk;
	int err = 0;

	if (drv_data->dvd->clk_bypass)
		return 0;

	mutex_lock(&drv_data->pw_clk_mutex);
	if (drv_data->pwclkcnt == 0) {
		err = clk_prepare_enable(ips_clk);
		if (err)
			pr_info("enable ips_clk fail:%d\n", err);
	}
	drv_data->pwclkcnt++;
	mutex_unlock(&drv_data->pw_clk_mutex);

	return err;
}

static int set_ips_ck_disable(struct ips_drv_data *drv_data)
{
	struct clk *ips_clk = drv_data->ips_clk;
	int err = 0;

	if (drv_data->dvd->clk_bypass)
		return 0;

	mutex_lock(&drv_data->pw_clk_mutex);
	drv_data->pwclkcnt--;
	if (drv_data->pwclkcnt == 0)
		clk_disable_unprepare(ips_clk);

	if (drv_data->pwclkcnt < 0)
		drv_data->pwclkcnt = 0;

	mutex_unlock(&drv_data->pw_clk_mutex);

	return err;
}

static int __mtkips_enable(struct ips_drv_data *ips_data, bool en)
{
	struct arm_smccc_res ares;

	if (ips_data->dvd->id == 0) {
		if (en) {
			ips_write(ips_data, IPS_01, 0);
			ips_write(ips_data, IPS_13, 0x400000);
			ips_write(ips_data, IPS_01, 0x1A400);
			ips_write(ips_data, IPS_10, 0x44050FE);
			ips_write(ips_data, IPS_05, 0);
			ips_write(ips_data, IPS_02, 0x1000);
			ips_write(ips_data, IPS_11, 0x100);
			ips_write(ips_data, IPS_01, 0x1A500);
		} else {
			ips_write(ips_data, IPS_01, 0);
		}
		return 0;
	}

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_IPS_ENABLE,
	ips_data->dvd->id, en, 0, 0, 0, 0,
	&ares);
	if (!ares.a0)
		return 0;
	else
		return -1;
}

int mtkips_enable(struct ips_drv_data *ips_data)
{
	if (!ips_data->enable) {
		set_ips_ck_enable(ips_data);
		if (__mtkips_enable(ips_data, true)) {
			set_ips_ck_disable(ips_data);
			return -1;
		}
		ips_data->enable = true;
	}
	return 0;
}

int mtkips_disable(struct ips_drv_data *ips_data)
{
	if (ips_data->enable) {
		__mtkips_enable(ips_data, false);
		set_ips_ck_disable(ips_data);
		ips_data->enable = false;
	}

	return 0;
}

int __mtkips_getvmin(struct ips_drv_data *ips_data)
{
	struct arm_smccc_res ares;

	if (ips_data->dvd->id == 0)
		return ips_read(ips_data, IPS_06);

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_IPS_GET_VMIN,
	ips_data->dvd->id, 0, 0, 0, 0, 0,
	&ares);
	if (!ares.a0)
		return ares.a1;
	else
		return 0xDEADDEAD;
}

int mtkips_getvmin(struct ips_drv_data *ips_data)
{
	int vmin = 0;

	if (ips_data->enable) {
		vmin = __mtkips_getvmin(ips_data);
		if (vmin == 0xDEADDEAD)
			return vmin;
		vmin = 375000 + ((vmin & 0xFF) * 750000) / 255;
		return vmin;
	} else
		return vmin;
}

static int __mtkips_getvmin_clear(struct ips_drv_data *ips_data)
{
	struct arm_smccc_res ares;
	u32 val;

	if (ips_data->dvd->id == 0) {
		val = ips_read(ips_data, IPS_06);
		ips_write(ips_data, IPS_05, 0x4);
		ips_write(ips_data, IPS_05, 0x0);
		return val;
	}

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_IPS_GET_VMIN_CLEAR,
	ips_data->dvd->id, 0, 0, 0, 0, 0,
	&ares);
	if (!ares.a0)
		return ares.a1;
	else
		return 0xDEADDEAD;
}

int mtkips_getvmin_clear(struct ips_drv_data *ips_data)
{
	int vmin = 0;

	if (ips_data->enable) {
		vmin = __mtkips_getvmin_clear(ips_data);
		if (vmin == 0xDEADDEAD)
			return vmin;
		vmin = 375000 + ((vmin & 0xFF) * 750000) / 255;
		return vmin;
	} else
		return vmin;
}

int mtkips_worst_vmin(struct ips_drv_data *ips_data)
{
	int vmin = 0;

	vmin = dvfsrc_read_rsrv5(ips_data);
	vmin = 375000 + ((vmin & 0xFF) * 750000) / 255;

	return vmin;
}

int __mtkips_set_clk(struct ips_drv_data *ips_data, struct clk *set_clk)
{
	int ret;

	ret = clk_set_parent(ips_data->ips_clk, set_clk);
	if (ret < 0)
		pr_info("failed to set clk (%d).\n", ret);
	return ret;
}

int mtkips_set_clk(struct ips_drv_data *ips_data, int clk)
{
	switch (clk) {
	case clk_src_0 :
		__mtkips_set_clk(ips_data, ips_data->clk_src_0);
		break;
	case clk_src_1 :
		__mtkips_set_clk(ips_data, ips_data->clk_src_1);
		break;
	case clk_src_2 :
			__mtkips_set_clk(ips_data, ips_data->clk_src_2);
			break;
	case clk_src_3 :
			__mtkips_set_clk(ips_data, ips_data->clk_src_3);
			break;
	default:
			return -EINVAL;
	}

	return 0;
}

static int mtkips_helper_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ips_drv_data *ips_drv;
	struct resource *res;
	int ret = 0;

	ips_drv = devm_kzalloc(dev, sizeof(*ips_drv), GFP_KERNEL);
	if (!ips_drv)
		return -ENOMEM;

	ips_drv->dvd = of_device_get_match_data(&pdev->dev);
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ips");
	if (!res)
		return -ENODEV;

	ips_drv->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(ips_drv->regs))
		return PTR_ERR(ips_drv->regs);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dvfsrc");
	if (res) {
		ips_drv->dvfsrc_regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(ips_drv->dvfsrc_regs))
			ips_drv->dvfsrc_regs = NULL;
	}

	if (!ips_drv->dvd->clk_bypass) {
		ips_drv->clk_src_0= devm_clk_get(&pdev->dev, "clk_src_0");
		if (IS_ERR(ips_drv->clk_src_0)) {
			dev_info(&pdev->dev, "cannot get clock clk_src_0\n");
			return PTR_ERR(ips_drv->clk_src_0);
		}

		ips_drv->clk_src_1= devm_clk_get(&pdev->dev, "clk_src_1");
		if (IS_ERR(ips_drv->clk_src_1)) {
			dev_info(&pdev->dev, "cannot get clock clk_src_1\n");
			return PTR_ERR(ips_drv->clk_src_1);
		}

		ips_drv->clk_src_2= devm_clk_get(&pdev->dev, "clk_src_2");
		if (IS_ERR(ips_drv->clk_src_2)) {
			dev_info(&pdev->dev, "cannot get clock clk_src_2\n");
			return PTR_ERR(ips_drv->clk_src_2);
		}

		ips_drv->clk_src_3= devm_clk_get(&pdev->dev, "clk_src_3");
		if (IS_ERR(ips_drv->clk_src_3)) {
			dev_info(&pdev->dev, "cannot get clock clk_src_3\n");
			return PTR_ERR(ips_drv->clk_src_3);
		}

		ips_drv->ips_clk = devm_clk_get(&pdev->dev, "ips-clk");
		if (IS_ERR(ips_drv->ips_clk)) {
			dev_info(&pdev->dev, "cannot get ips clock\n");
			return PTR_ERR(ips_drv->ips_clk);
		}

		ret = clk_set_parent(ips_drv->ips_clk, ips_drv->clk_src_3);
		if (ret < 0) {
			dev_info(&pdev->dev, "failed to clk_set_parent (%d)\n", ret);
			return ret;
		}
	}

	mutex_init(&ips_drv->pw_clk_mutex);
	platform_set_drvdata(pdev, ips_drv);
	ips_register_sysfs(dev);

	return ret;
}


static int mtkips_helper_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	ips_unregister_sysfs(dev);
	return 0;
}

static int mtkips_pm_suspend(struct device *dev)
{
	return 0;
}

static int mtkips_pm_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops mtkips_pm_ops = {
	.suspend = mtkips_pm_suspend,
	.resume  = mtkips_pm_resume,
};

static const struct ips_soc_data mt6897_soc_east_data = {
	.regs = mt6985_regs,
	.clk_bypass = true,
	.id = 1,
};
static const struct ips_soc_data mt6897_soc_west_data = {
	.regs = mt6985_regs,
	.clk_bypass = true,
	.id = 2,
};

static const struct ips_soc_data mt6985_soc_east_data = {
	.regs = mt6985_regs,
	.id = 1,
};

static const struct ips_soc_data mt6985_soc_west_data = {
	.regs = mt6985_regs,
	.id = 2,
};

static const struct ips_soc_data mt6989_soc_east_data = {
	.regs = mt6985_regs,
	.id = 0,
};
static const struct ips_soc_data mt6989_soc_west_data = {
	.regs = mt6985_regs,
	.id = 0,
};

static const struct of_device_id of_mtkips_match_tbl[] = {
	{
		.compatible = "mediatek,mt6985-soc-ips-east",
		.data = &mt6985_soc_east_data,
	},
	{
		.compatible = "mediatek,mt6985-soc-ips-west",
		.data = &mt6985_soc_west_data,
	},
	{
		.compatible = "mediatek,mt6897-soc-ips-east",
		.data = &mt6897_soc_east_data,
	},
	{
		.compatible = "mediatek,mt6897-soc-ips-west",
		.data = &mt6897_soc_west_data,
	},
	{
		.compatible = "mediatek,mt6989-soc-ips-east",
		.data = &mt6989_soc_east_data,
	},
	{
		.compatible = "mediatek,mt6989-soc-ips-west",
		.data = &mt6989_soc_west_data,
	},
	{}
};

static struct platform_driver mtkips_drv = {
	.probe = mtkips_helper_probe,
	.remove	= mtkips_helper_remove,
	.driver = {
		.name = "mtk-mtkips",
		.of_match_table = of_mtkips_match_tbl,
		.pm = &mtkips_pm_ops,
	},
};

static int __init mtk_ips_init(void)
{
	s32 status;

	status = platform_driver_register(&mtkips_drv);
	if (status) {
		pr_notice("Failed to register IPS driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_ips_exit(void)
{
	platform_driver_unregister(&mtkips_drv);
}

module_init(mtk_ips_init);
module_exit(mtk_ips_exit);
MODULE_DESCRIPTION("MTK IPS driver");
MODULE_AUTHOR("Arvin Wang<arvin.wang@mediatek.com>");
MODULE_LICENSE("GPL");
