// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6989-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ifrao_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

#define GATE_IFRAO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ifrao_clks[] = {
	GATE_IFRAO(CLK_IFRAO_DPMAIF_MAIN, "ifrao_dpmaif_main",
			"dpmaif_main_ck"/* parent */, 22),
	GATE_IFRAO(CLK_IFRAO_RG_MMW_DPMAIF26M, "ifrao_dpmaif_26m",
			"f26m_ck"/* parent */, 25),
	GATE_IFRAO(CLK_IFRAO_RG_SOCSYS_FATB, "ifrao_socsys_fatb",
			"atb_ck"/* parent */, 28),
	GATE_IFRAO(CLK_IFRAO_RG_SOCSYS_SDF, "ifrao_socsys_sdf",
			"sdf_ck"/* parent */, 30),
};

static const struct mtk_clk_desc ifrao_mcd = {
	.clks = ifrao_clks,
	.num_clks = CLK_IFRAO_NR_CLK,
};



static const struct of_device_id of_match_clk_mt6989_bus[] = {
	{
		.compatible = "mediatek,mt6989-infracfg_ao",
		.data = &ifrao_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6989_bus_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6989_bus_drv = {
	.probe = clk_mt6989_bus_grp_probe,
	.driver = {
		.name = "clk-mt6989-bus",
		.of_match_table = of_match_clk_mt6989_bus,
	},
};

module_platform_driver(clk_mt6989_bus_drv);
MODULE_LICENSE("GPL");
