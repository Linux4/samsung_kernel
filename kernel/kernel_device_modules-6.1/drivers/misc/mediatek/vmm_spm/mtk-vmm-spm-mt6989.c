// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Nick Wen <Nick.Wen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/irqflags.h>


#define NEED_SWITCH_MUX 0

#define ISPDVFS_DBG
#ifdef ISPDVFS_DBG
#define ISP_LOGD(fmt, args...) \
	do { \
		if (mtk_ispdvfs_dbg_level) \
			pr_notice("[ISPDVFS] %s(): " fmt "\n",\
				__func__, ##args); \
	} while (0)
#else
#define ISPDVFS_DBG(fmt, args...)
#endif
#define ISP_LOGI(fmt, args...) \
	pr_notice("[ISPDVFS] %s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("[ISPDVFS] error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("[ISPDVFS] fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

#define SPM_CHECK_IDX(idx, max) ((idx) >= (max) ? 1 : 0)

static unsigned int spm_ver;

enum {
	VMM_SPM_DEFAULT = 0,
	VMM_SPM_MT6989_BRINGUP,
	VMM_SPM_MT6989,

	VMM_SPM_MAX,
};

struct vmm_spm_reg_info {
	/* reg */
	unsigned int spm_iso_con_status;
	unsigned int spm_iso_con_set;
	unsigned int spm_iso_con_clr;
	unsigned int spm_mux_hfrp_ctrl; // switch iso mux between hfrp/spm.
	unsigned int hfrp_mm_ctrl;

	/* bit */
	unsigned int spm_ext_iso_bit;
	unsigned int spm_iso_din_bit;
	unsigned int spm_latch_en_bit;
	unsigned int spm_to_hfrp_ctrl_bit;	// switch iso mux. (1: mux to hfrp; 0: mux to spm)
	unsigned int hfrp_ext_iso_bit;
	unsigned int hfrp_iso_din_bit;
	unsigned int hfrp_latch_en_bit;
};

struct vmm_spm_drv_data {
	void __iomem *spm_reg;
	void __iomem *hfrp_reg;
	struct notifier_block nb;
};

static struct vmm_spm_drv_data g_drv_data;

static const struct vmm_spm_reg_info spm_reg_tbl[VMM_SPM_MAX] = {
	[VMM_SPM_MT6989_BRINGUP] = {
		/* reg */
		.spm_iso_con_status = 0xF64,
		.spm_iso_con_set = 0xF68,
		.spm_iso_con_clr = 0xF6C,
		.spm_mux_hfrp_ctrl = 0x420,	// switch iso mux between hfrp/spm.
		.hfrp_mm_ctrl = 0x37098,

		/* bit */
		.spm_ext_iso_bit = 20,
		.spm_iso_din_bit = 21,
		.spm_latch_en_bit = 22,
		.spm_to_hfrp_ctrl_bit = 0,	// mux(1: mux to hfrp, 0: mux to spm)
		.hfrp_ext_iso_bit = 0,
		.hfrp_iso_din_bit = 1,
		.hfrp_latch_en_bit = 2,
	},
	[VMM_SPM_MT6989] = {
		/* reg */
		.spm_iso_con_status = 0xF64,
		.spm_iso_con_set = 0xF68,
		.spm_iso_con_clr = 0xF6C,
		.spm_mux_hfrp_ctrl = 0x420,	// switch iso mux between hfrp/spm.
		.hfrp_mm_ctrl = 0x37098,

		/* bit */
		.spm_ext_iso_bit = 20,
		.spm_iso_din_bit = 21,
		.spm_latch_en_bit = 22,
		.spm_to_hfrp_ctrl_bit = 0,	// mux(1: mux to hfrp, 0: mux to spm)
		.hfrp_ext_iso_bit = 0,
		.hfrp_iso_din_bit = 1,
		.hfrp_latch_en_bit = 2,
	},
};

#if NEED_SWITCH_MUX == 1
static void vmm_iso_mux_ctrl_mt6989(void __iomem *base, bool mux_to_hfrp)
{
	void __iomem *addr;
	u32 reg_iso_mux_val;

	if (SPM_CHECK_IDX(spm_ver, VMM_SPM_MAX) || !base) {
		ISP_LOGE("[fatal]VMM spm param error! ver[cur=%u/max=%u]\n",
				spm_ver, VMM_SPM_MAX);
		return;
	}

	addr = base + spm_reg_tbl[spm_ver].spm_mux_hfrp_ctrl;
	if (mux_to_hfrp == true) {
		reg_iso_mux_val = readl_relaxed(addr);
		reg_iso_mux_val |= (1UL << spm_reg_tbl[spm_ver].spm_to_hfrp_ctrl_bit);
		writel_relaxed(reg_iso_mux_val, addr);
	} else {
		reg_iso_mux_val = readl_relaxed(addr);
		reg_iso_mux_val &= ~(1UL << spm_reg_tbl[spm_ver].spm_to_hfrp_ctrl_bit);
		writel_relaxed(reg_iso_mux_val, addr);
	}
}
#endif

static void vmm_spm_iso_ctrl_mt6989(void __iomem *base, bool iso_on)
{
	void __iomem *addr;

	if (SPM_CHECK_IDX(spm_ver, VMM_SPM_MAX) || !base) {
		ISP_LOGE("[fatal]VMM spm param error! ver[cur=%u/max=%u]\n",
				spm_ver, VMM_SPM_MAX);
		return;
	}

	if (iso_on == true) {
		addr = base + spm_reg_tbl[spm_ver].spm_iso_con_set;
		writel_relaxed((1 << spm_reg_tbl[spm_ver].spm_latch_en_bit), addr);
		writel_relaxed((1 << spm_reg_tbl[spm_ver].spm_iso_din_bit), addr);
		writel_relaxed((1 << spm_reg_tbl[spm_ver].spm_ext_iso_bit), addr);
	} else {
		addr = base + spm_reg_tbl[spm_ver].spm_iso_con_clr;
		writel_relaxed((1 << spm_reg_tbl[spm_ver].spm_ext_iso_bit), addr);
		writel_relaxed((1 << spm_reg_tbl[spm_ver].spm_iso_din_bit), addr);
		writel_relaxed((1 << spm_reg_tbl[spm_ver].spm_latch_en_bit), addr);
	}
}

#ifdef HFRP_REG_SET_ISSUE_FIXED
static void vmm_hfrp_iso_ctrl_mt6989(void __iomem *base, bool iso_on)
{
	void __iomem *addr;
	u32 reg_buck_iso_val;

	if (SPM_CHECK_IDX(spm_ver, VMM_SPM_MAX) || !base) {
		ISP_LOGE("[fatal]VMM spm param error! ver[cur=%u/max=%u]\n",
				spm_ver, VMM_SPM_MAX);
		return;
	}
	addr = base + spm_reg_tbl[spm_ver].hfrp_mm_ctrl;

	if (iso_on == true) {
		reg_buck_iso_val = readl_relaxed(addr);
		reg_buck_iso_val |= (1UL << spm_reg_tbl[spm_ver].hfrp_latch_en_bit);
		writel_relaxed(reg_buck_iso_val, addr);

		reg_buck_iso_val = readl_relaxed(addr);
		reg_buck_iso_val |= (1UL << spm_reg_tbl[spm_ver].hfrp_iso_din_bit);
		writel_relaxed(reg_buck_iso_val, addr);

		reg_buck_iso_val = readl_relaxed(addr);
		reg_buck_iso_val |= (1UL << spm_reg_tbl[spm_ver].hfrp_ext_iso_bit);
		writel_relaxed(reg_buck_iso_val, addr);
	} else {
		reg_buck_iso_val = readl_relaxed(addr);
		reg_buck_iso_val &= ~(1UL << spm_reg_tbl[spm_ver].hfrp_ext_iso_bit);
		writel_relaxed(reg_buck_iso_val, addr);

		reg_buck_iso_val = readl_relaxed(addr);
		reg_buck_iso_val &= ~(1UL << spm_reg_tbl[spm_ver].hfrp_iso_din_bit);
		writel_relaxed(reg_buck_iso_val, addr);

		reg_buck_iso_val = readl_relaxed(addr);
		reg_buck_iso_val &= ~(1UL << spm_reg_tbl[spm_ver].hfrp_latch_en_bit);
		writel_relaxed(reg_buck_iso_val, addr);
	}
}

static void vmm_hfrp_iso_reg_check(void __iomem *base, bool iso_on)
{
	void __iomem *addr;
	u32 read_reg_val;

	if (SPM_CHECK_IDX(spm_ver, VMM_SPM_MAX) || !base) {
		ISP_LOGE("[fatal]VMM spm param error! ver[cur=%u/max=%u]\n",
				spm_ver, VMM_SPM_MAX);
		return;
	}

	addr = base + spm_reg_tbl[spm_ver].hfrp_mm_ctrl;
	read_reg_val = readl_relaxed(addr);
	if (iso_on) {
		if (((read_reg_val & BIT(spm_reg_tbl[spm_ver].hfrp_latch_en_bit))
			!= BIT(spm_reg_tbl[spm_ver].hfrp_latch_en_bit)) ||
			((read_reg_val & BIT(spm_reg_tbl[spm_ver].hfrp_iso_din_bit))
			!= BIT(spm_reg_tbl[spm_ver].hfrp_iso_din_bit)) ||
			((read_reg_val & BIT(spm_reg_tbl[spm_ver].hfrp_ext_iso_bit))
			!= BIT(spm_reg_tbl[spm_ver].hfrp_ext_iso_bit))) {
			ISP_LOGE("HFRP ISO on error! read_reg_val[0x%x] = %u, spm_ver=%d\n",
				spm_reg_tbl[spm_ver].hfrp_mm_ctrl, read_reg_val, spm_ver);
			panic("HFRP VMM ISO set on wrong!");
		}
	} else {
		if (((read_reg_val & BIT(spm_reg_tbl[spm_ver].hfrp_latch_en_bit)) != 0) ||
			((read_reg_val & BIT(spm_reg_tbl[spm_ver].hfrp_iso_din_bit)) != 0) ||
			((read_reg_val & BIT(spm_reg_tbl[spm_ver].hfrp_ext_iso_bit)) != 0)) {
			ISP_LOGE("HFRP ISO off_error! read_reg_val[0x%x] = %u, spm_ver=%d\n",
				spm_reg_tbl[spm_ver].hfrp_mm_ctrl, read_reg_val, spm_ver);
			panic("HFRP VMM ISO set off wrong!");
		}
	}
	ISP_LOGI("VMM buck iso hfrp setting success! spm_ver=%d, iso_on=%d\n",
			spm_ver,
			iso_on);
}
#endif

static void vmm_spm_iso_reg_check(void __iomem *base, bool iso_on)
{
	void __iomem *addr;
	u32 read_reg_val;

	if (SPM_CHECK_IDX(spm_ver, VMM_SPM_MAX) || !base) {
		ISP_LOGE("[fatal]VMM spm param error! ver[cur=%u/max=%u]\n",
				spm_ver, VMM_SPM_MAX);
		return;
	}

	addr = base + spm_reg_tbl[spm_ver].spm_iso_con_status;
	read_reg_val = readl_relaxed(addr);
	if (iso_on) {
		if (((read_reg_val & BIT(spm_reg_tbl[spm_ver].spm_latch_en_bit))
			!= BIT(spm_reg_tbl[spm_ver].spm_latch_en_bit)) ||
			((read_reg_val & BIT(spm_reg_tbl[spm_ver].spm_iso_din_bit))
			!= BIT(spm_reg_tbl[spm_ver].spm_iso_din_bit)) ||
			((read_reg_val & BIT(spm_reg_tbl[spm_ver].spm_ext_iso_bit))
			!= BIT(spm_reg_tbl[spm_ver].spm_ext_iso_bit))) {
			ISP_LOGE("SPM ISO on error! read_reg_val[0x%x] = %u, spm_ver=%d\n",
				spm_reg_tbl[spm_ver].spm_iso_con_status, read_reg_val, spm_ver);
			return;
		}
	} else {
		if (((read_reg_val & BIT(spm_reg_tbl[spm_ver].spm_latch_en_bit)) != 0) ||
			((read_reg_val & BIT(spm_reg_tbl[spm_ver].spm_iso_din_bit)) != 0) ||
			((read_reg_val & BIT(spm_reg_tbl[spm_ver].spm_ext_iso_bit)) != 0)) {
			ISP_LOGE("SPM ISO off_error! read_reg_val[0x%x] = %u, spm_ver=%d\n",
				spm_reg_tbl[spm_ver].spm_iso_con_status, read_reg_val, spm_ver);
			return;
		}
	}
	ISP_LOGI("VMM buck iso spm setting success! spm_ver=%d, iso_on=%d\n",
			spm_ver,
			iso_on);
}

static int regulator_event_notify_mt6989(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct vmm_spm_drv_data *drv_data = &g_drv_data;

	if (!drv_data->spm_reg || !drv_data->hfrp_reg) {
		ISP_LOGE("SPM_BASE or HFRP_BASE is NULL");
		return -EINVAL;
	}

	if (event == REGULATOR_EVENT_ENABLE) {
		vmm_spm_iso_ctrl_mt6989(drv_data->spm_reg, false);
		vmm_spm_iso_reg_check(drv_data->spm_reg, false);
		ISP_LOGI("VMM regulator enable done, ver = %u\n", spm_ver);
	} else if (event == REGULATOR_EVENT_PRE_DISABLE) {
		vmm_spm_iso_ctrl_mt6989(drv_data->spm_reg, true);
		vmm_spm_iso_reg_check(drv_data->spm_reg, true);
		ISP_LOGI("VMM regulator before disable, ver = %u\n", spm_ver);
	}

	return 0;
}

static int vmm_spm_probe_mt6989(struct platform_device *pdev)
{
	struct vmm_spm_drv_data *drv_data = &g_drv_data;
	struct device *dev = &pdev->dev;
	struct resource *spm_res;
#ifdef HFRP_REG_SET_ISSUE_FIXED
	struct resource *hfrp_res;
#endif
	struct regulator *reg;
	s32 ret = 0;

	/* Get related registers in dts */
	spm_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!spm_res) {
		ISP_LOGE("fail to get resource SPM_BASE");
		return -EINVAL;
	}
#ifdef HFRP_REG_SET_ISSUE_FIXED
	hfrp_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!hfrp_res) {
		ISP_LOGE("fail to get resource HFRP_BASE");
		return -EINVAL;
	}
#endif

	/* remap regs */
	drv_data->spm_reg = devm_ioremap(dev, spm_res->start, resource_size(spm_res));
	if (!(drv_data->spm_reg)) {
		ISP_LOGE("fail to ioremap SPM_BASE: 0x%llx", spm_res->start);
		return -EINVAL;
	}
#ifdef HFRP_REG_SET_ISSUE_FIXED
	drv_data->hfrp_reg = devm_ioremap(dev, hfrp_res->start, resource_size(hfrp_res));
	if (!(drv_data->hfrp_reg)) {
		ISP_LOGE("fail to ioremap HFRP_BASE: 0x%llx", hfrp_res->start);
		return -EINVAL;
	}
#endif

	/* get spm ver */
	if (of_device_is_compatible(dev->of_node, "mediatek,vmm_spm_mt6989"))
		spm_ver = VMM_SPM_MT6989;
	else if (of_device_is_compatible(dev->of_node, "mediatek,vmm_spm_mt6989_bringup"))
		spm_ver = VMM_SPM_MT6989_BRINGUP;
	else
		spm_ver = VMM_SPM_DEFAULT;

	if (spm_ver == VMM_SPM_MT6989) {
		/* formal flow: This flow had been replaced by preloader, we just check iso status here. */
#ifdef HFRP_REG_SET_ISSUE_FIXED
		vmm_hfrp_iso_ctrl_mt6989(drv_data->hfrp_reg, false);
		vmm_hfrp_iso_reg_check(drv_data->hfrp_reg, false);
#endif
		ISP_LOGI("VMMSPM boot success, ver = %u\n", spm_ver);
	} else if (VMM_SPM_MT6989_BRINGUP) {
		/* bringup flow */
		vmm_spm_iso_ctrl_mt6989(drv_data->spm_reg, false);
		vmm_spm_iso_reg_check(drv_data->spm_reg, false);
		ISP_LOGI("VMM regulator enable done, ver = %u\n", spm_ver);

		reg = devm_regulator_get(dev, "vmm-pmic");
		if (IS_ERR(reg)) {
			ISP_LOGE("devm_regulator_get vmm-pmic fail");
			return PTR_ERR(reg);
		}
		drv_data->nb.notifier_call = regulator_event_notify_mt6989;
		ret = devm_regulator_register_notifier(reg, &drv_data->nb);
		if (ret)
			ISP_LOGE("Failed to register notifier: %d\n", ret);
	} else {
		ISP_LOGE("dts ver is wrong!\n");
		panic("VMM dts compatible is wrong!\n");
	}

	return ret;
}

static const struct of_device_id of_vmm_spm_match_tbl[] = {
	{
		.compatible = "mediatek,vmm_spm_mt6989",
	},
	{
		.compatible = "mediatek,vmm_spm_mt6989_bringup",
	},
	{}
};

int mtk_vmm_spm_get_data_mt6989(const char *val, const struct kernel_param *kp)
{
	struct vmm_spm_drv_data *drv_data = &g_drv_data;
	void __iomem *addr;
	void __iomem *base;

	if (!drv_data->spm_reg || !drv_data->hfrp_reg || spm_ver >= VMM_SPM_MAX) {
		ISP_LOGE("SPM_BASE is NULL or spm_ver[%u] is wrong. Max=%u",
			spm_ver, VMM_SPM_MAX);
		return -EINVAL;
	}

	/* SPM reg */
	base = drv_data->spm_reg;
	addr = base + spm_reg_tbl[spm_ver].spm_iso_con_status;
	ISP_LOGI("%s spm_iso_con_status[0x%x] = 0x%x\n",
			__func__,
			spm_reg_tbl[spm_ver].spm_iso_con_status,
			readl_relaxed(addr));
	addr = base + spm_reg_tbl[spm_ver].spm_mux_hfrp_ctrl;
	ISP_LOGI("%s spm_mux_hfrp_ctrl[0x%x] = 0x%x\n",
			__func__,
			spm_reg_tbl[spm_ver].spm_mux_hfrp_ctrl,
			readl_relaxed(addr));

	/* HFRP reg */
	base = drv_data->hfrp_reg;
	addr = base + spm_reg_tbl[spm_ver].hfrp_mm_ctrl;
	ISP_LOGI("%s hfrp_mm_ctrl[0x%x] = 0x%x\n",
			__func__,
			spm_reg_tbl[spm_ver].hfrp_mm_ctrl,
			readl_relaxed(addr));
	return 0;
}

static const struct kernel_param_ops vmm_spm_get_data = {
	.set = mtk_vmm_spm_get_data_mt6989,
};

module_param_cb(vmm_spm_info, &vmm_spm_get_data, NULL, 0644);
MODULE_PARM_DESC(vmm_spm_info,
	"get vmm spm info");

static struct platform_driver drv_vmm_spm_mt6989 = {
	.probe = vmm_spm_probe_mt6989,
	.driver = {
		.name = "mtk-vmm-spm-mt6989",
		.of_match_table = of_vmm_spm_match_tbl,
	},
};

static int __init mtk_vmm_spm_init_mt6989(void)
{
	s32 status;

	status = platform_driver_register(&drv_vmm_spm_mt6989);
	if (status) {
		pr_notice("Failed to register VMM SPM  mt6989 driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_vmm_spm_exit_mt6989(void)
{
	platform_driver_unregister(&drv_vmm_spm_mt6989);
}

module_init(mtk_vmm_spm_init_mt6989);
module_exit(mtk_vmm_spm_exit_mt6989);
MODULE_DESCRIPTION("MTK VMM SPM mt6989 driver");
MODULE_AUTHOR("Nick.wen <nick.wen@mediatek.com>");
MODULE_SOFTDEP("pre:vcp");
MODULE_SOFTDEP("post:mtk-scpsys-mt6989");
MODULE_LICENSE("GPL");
