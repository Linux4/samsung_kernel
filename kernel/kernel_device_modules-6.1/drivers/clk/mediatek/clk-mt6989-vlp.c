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

static const struct mtk_gate_regs scp_cg_regs = {
	.set_ofs = 0x154,
	.clr_ofs = 0x158,
	.sta_ofs = 0x154,
};

#define GATE_SCP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate scp_clks[] = {
	GATE_SCP(CLK_SCP_SET_SPI0, "scp_set_spi0",
			"ulposc_ck"/* parent */, 0),
	GATE_SCP(CLK_SCP_SET_SPI1, "scp_set_spi1",
			"ulposc_ck"/* parent */, 1),
	GATE_SCP(CLK_SCP_SET_SPI2, "scp_set_spi2",
			"ulposc_ck"/* parent */, 2),
	GATE_SCP(CLK_SCP_SET_SPI3, "scp_set_spi3",
			"ulposc_ck"/* parent */, 3),
};

static const struct mtk_clk_desc scp_mcd = {
	.clks = scp_clks,
	.num_clks = CLK_SCP_NR_CLK,
};

static const struct mtk_gate_regs scp_fast_iic_cg_regs = {
	.set_ofs = 0xE18,
	.clr_ofs = 0xE14,
	.sta_ofs = 0xE10,
};

#define GATE_SCP_FAST_IIC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_fast_iic_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate scp_fast_iic_clks[] = {
	GATE_SCP_FAST_IIC(CLK_SCP_FAST_IIC_I2C0, "scp_fast_iic_i2c0",
			"ulposc_ck"/* parent */, 0),
};

static const struct mtk_clk_desc scp_fast_iic_mcd = {
	.clks = scp_fast_iic_clks,
	.num_clks = CLK_SCP_FAST_IIC_NR_CLK,
};

static const struct mtk_gate_regs scp_iic_cg_regs = {
	.set_ofs = 0xE18,
	.clr_ofs = 0xE14,
	.sta_ofs = 0xE10,
};

static const struct mtk_gate_regs scp_iic_hwv_regs = {
	.set_ofs = 0x0028,
	.clr_ofs = 0x002C,
	.sta_ofs = 0x1C14,
};

#define GATE_SCP_IIC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_iic_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_SCP_IIC(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "hw-voter-regmap",				\
		.regs = &scp_iic_cg_regs,			\
		.hwv_regs = &scp_iic_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate scp_iic_clks[] = {
	GATE_SCP_IIC(CLK_SCP_IIC_I2C0, "scp_iic_i2c0",
			"ulposc_ck"/* parent */, 0),
	GATE_HWV_SCP_IIC(CLK_SCP_IIC_I2C1, "scp_iic_i2c1",
			"ulposc_ck"/* parent */, 1),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C2, "scp_iic_i2c2",
			"ulposc_ck"/* parent */, 2),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C3, "scp_iic_i2c3",
			"ulposc_ck"/* parent */, 3),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C4, "scp_iic_i2c4",
			"ulposc_ck"/* parent */, 4),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C5, "scp_iic_i2c5",
			"ulposc_ck"/* parent */, 5),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C6, "scp_iic_i2c6",
			"ulposc_ck"/* parent */, 6),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C7, "scp_iic_i2c7",
			"ulposc_ck"/* parent */, 7),
};

static const struct mtk_clk_desc scp_iic_mcd = {
	.clks = scp_iic_clks,
	.num_clks = CLK_SCP_IIC_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6989_vlp[] = {
	{
		.compatible = "mediatek,mt6989-scp",
		.data = &scp_mcd,
	}, {
		.compatible = "mediatek,mt6989-scp_fast_iic",
		.data = &scp_fast_iic_mcd,
	}, {
		.compatible = "mediatek,mt6989-scp_iic",
		.data = &scp_iic_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6989_vlp_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6989_vlp_drv = {
	.probe = clk_mt6989_vlp_grp_probe,
	.driver = {
		.name = "clk-mt6989-vlp",
		.of_match_table = of_match_clk_mt6989_vlp,
	},
};

module_platform_driver(clk_mt6989_vlp_drv);
MODULE_LICENSE("GPL");
