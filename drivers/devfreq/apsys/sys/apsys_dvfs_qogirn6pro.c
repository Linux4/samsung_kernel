/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include "sprd_dvfs_apsys.h"
#include "apsys_dvfs_qogirn6pro.h"

char *qogirn6pro_apsys_val_to_volt(u32 val)
{
	switch (val) {
	case 0:
		return "0.7v";
	case 1:
		return "0.75v";
	case 2:
		return "0.8v";
	default:
		pr_err("invalid voltage value %u\n", val);
		return "N/A";
	}
}

char *qogirn6pro_dpu_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "307.2M";
	case 2:
		return "384M";
	case 3:
		return "409.6M";
	case 4:
		return "512M";
	case 5:
		return "614.4M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

char *qogirn6pro_gsp_val_to_volt(u32 val)
{
	switch (val) {
	case 0:
		return "0.55v";
	case 1:
		return "0.6v";
	case 2:
		return "0.65v";
	case 3:
		return "0.7v";
	case 4:
		return "0.75v";
	default:
		pr_err("invalid voltage value %u\n", val);
		return "N/A";
	}
}

char *qogirn6pro_gsp_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "307.2M";
	case 2:
		return "384M";
	case 3:
		return "512M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

char *qogirn6pro_vpu_val_to_volt(u32 val)
{
	switch (val) {
	case 0:
		return "0.55v";
	case 1:
		return "0.6v";
	case 2:
		return "0.65v";
	case 3:
		return "0.7v";
	case 4:
		return "0.75v";
	default:
		pr_err("invalid voltage value %u\n", val);
		return "N/A";
	}
}

char *qogirn6pro_vpuenc_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "307.2M";
	case 2:
		return "384M";
	case 3:
		return "512M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

char *qogirn6pro_vpudec_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "307.2M";
	case 2:
		return "384M";
	case 3:
		return "512M";
	case 4:
		return "680M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

char *qogirn6pro_vdsp_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "384M";
	case 2:
		return "512M";
	case 3:
		return "614.4M";
	case 4:
		return "768M";
	case 5:
		return "936M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

static void apsys_dvfs_force_en(u32 force_en)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)regmap_ctx.apsys_base;

	if (force_en)
		reg->cgm_dpu_vsp_dvfs_clk_gate_ctrl |= BIT(1);
	else
		reg->cgm_dpu_vsp_dvfs_clk_gate_ctrl &= ~BIT(1);
}

static void apsys_dvfs_auto_gate(u32 gate_sel)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)regmap_ctx.apsys_base;

	if (gate_sel)
		reg->cgm_dpu_vsp_dvfs_clk_gate_ctrl |= BIT(0);
	else
		reg->cgm_dpu_vsp_dvfs_clk_gate_ctrl &= ~BIT(0);
}

static void apsys_dvfs_hold_en(u32 hold_en)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->dpu_vsp_dvfs_hold_ctrl = hold_en;
}

static void apsys_dvfs_wait_window(u32 wait_window)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->dpu_vsp_dvfs_wait_window_cfg = wait_window;
}

static void apsys_dvfs_min_volt(u32 min_volt)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->dpu_vsp_min_voltage_cfg = min_volt;
}

static const struct of_device_id sprd_dvfs_of_match[] = {
	{
		.compatible = "sprd,ump962x-syscon",
	},
};

int dpu_vsp_dvfs_check_clkeb(void)
{
	u32  dpu_vsp_dvfs_eb_reg = 0;

	regmap_read(regmap_ctx.aon_base, 0x0, &dpu_vsp_dvfs_eb_reg);

	if (dpu_vsp_dvfs_eb_reg&BIT(21))
		return 0;
	else
		return -1;
}

static void apsys_top_dvfs_init(void)
{
	void __iomem *base;
	u32 temp;
	struct platform_device *pdev_regmap;
	struct device_node *regmap_np;
	const struct of_device_id *dev_id;
	struct regmap *regmap;

	base = ioremap_nocache(0x64940000, 0x600);
	if (IS_ERR(base))
		pr_err("ioremap top address failed\n");

	regmap_ctx.top_base = (unsigned long)base;

	temp = readl_relaxed(base + 0xd84);
	temp &= 0xfffffffe;
	writel_relaxed(temp, base + 0xd84);

	regmap_np = of_find_matching_node_and_match(NULL, sprd_dvfs_of_match,
			&dev_id);
	if (!regmap_np) {
		pr_err("regmap get device node fail\n");
		return;
	}

	pdev_regmap = of_find_device_by_node(regmap_np);
	if (!pdev_regmap) {
		pr_err("parent device get device node fail\n");
		return;
	}

	regmap = dev_get_regmap(pdev_regmap->dev.parent, NULL);

	/*0xa158 pmic mm dvfs enable register*/
	regmap_write(regmap, 0xa158, 0x4);
}

static int dcdc_modem_cur_volt(void)
{
	struct dpu_vspsys_dvfs_reg *reg;

	reg = (struct dpu_vspsys_dvfs_reg *)(regmap_ctx.apsys_base);

	return (reg->dpu_vsp_dvfs_voltage_dbg >> 8) & 0x7;
}

static int apsys_dvfs_parse_dt(struct apsys_dev *apsys,
				struct device_node *np)
{
	int ret;

	pr_info("%s()\n", __func__);

	regmap_ctx.aon_base = syscon_regmap_lookup_by_phandle(np, "sprd,aon_apb_regs_syscon");
	if (IS_ERR(regmap_ctx.aon_base))
		pr_err("ioremap aon address failed\n");

	ret = of_property_read_u32(np, "sprd,ap-dvfs-hold",
			&apsys->dvfs_coffe.dvfs_hold_en);
	if (ret)
		apsys->dvfs_coffe.dvfs_hold_en = 0;

	ret = of_property_read_u32(np, "sprd,ap-dvfs-force-en",
			&apsys->dvfs_coffe.dvfs_force_en);
	if (ret)
		apsys->dvfs_coffe.dvfs_force_en = 1;

	ret = of_property_read_u32(np, "sprd,ap-dvfs-auto-gate",
			&apsys->dvfs_coffe.dvfs_auto_gate);
	if (ret)
		apsys->dvfs_coffe.dvfs_auto_gate = 0;

	ret = of_property_read_u32(np, "sprd,ap-dvfs-wait-window",
			&apsys->dvfs_coffe.dvfs_wait_window);
	if (ret)
		apsys->dvfs_coffe.dvfs_wait_window = 0x10080;

	ret = of_property_read_u32(np, "sprd,ap-dvfs-min-volt",
			&apsys->dvfs_coffe.dvfs_min_volt);
	if (ret)
		apsys->dvfs_coffe.dvfs_min_volt = 0;

	return ret;
}

static void apsys_dvfs_init(struct apsys_dev *apsys)
{
	if (dpu_vsp_dvfs_check_clkeb()) {
		pr_info("%s(), dpu_vsp eb is not on\n", __func__);
		return;
	}
	apsys_dvfs_hold_en(apsys->dvfs_coffe.dvfs_hold_en);
	apsys_dvfs_force_en(apsys->dvfs_coffe.dvfs_force_en);
	apsys_dvfs_auto_gate(apsys->dvfs_coffe.dvfs_auto_gate);
	apsys_dvfs_wait_window(apsys->dvfs_coffe.dvfs_wait_window);
	apsys_dvfs_min_volt(apsys->dvfs_coffe.dvfs_min_volt);
}

static struct apsys_dvfs_ops apsys_dvfs_ops = {
	.parse_dt = apsys_dvfs_parse_dt,
	.dvfs_init = apsys_dvfs_init,
	.apsys_auto_gate = apsys_dvfs_auto_gate,
	.apsys_hold_en = apsys_dvfs_hold_en,
	.apsys_wait_window = apsys_dvfs_wait_window,
	.apsys_min_volt = apsys_dvfs_min_volt,
	.top_dvfs_init = apsys_top_dvfs_init,
	.top_cur_volt = dcdc_modem_cur_volt,
};

static struct dvfs_ops_entry apsys_dvfs_entry = {
	.ver = "qogirn6pro",
	.ops = &apsys_dvfs_ops,
};

static int __init apsys_dvfs_register(void)
{
	return apsys_dvfs_ops_register(&apsys_dvfs_entry);
}

subsys_initcall(apsys_dvfs_register);
