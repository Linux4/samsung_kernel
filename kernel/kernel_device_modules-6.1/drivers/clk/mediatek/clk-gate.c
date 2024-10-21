// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>
#include <linux/module.h>
#include <linux/sched/clock.h>

#include "clk-mtk.h"
#include "clk-gate.h"

static bool is_registered;

static int mtk_cg_bit_is_cleared(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val = 0;

	if (!is_registered)
		return 0;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val == 0;
}

static int mtk_cg_bit_is_set(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val = 0;

	if (!is_registered)
		return 0;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static void mtk_cg_set_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_write(cg->regmap, cg->set_ofs, BIT(cg->bit));
}

static void mtk_cg_clr_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_write(cg->regmap, cg->clr_ofs, BIT(cg->bit));
}

static void mtk_cg_set_bit_no_setclr(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 cgbit = BIT(cg->bit);

	regmap_update_bits(cg->regmap, cg->sta_ofs, cgbit, cgbit);
}

static void mtk_cg_clr_bit_no_setclr(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 cgbit = BIT(cg->bit);

	regmap_update_bits(cg->regmap, cg->sta_ofs, cgbit, 0);
}

static int mtk_cg_enable(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	mtk_clk_notify(NULL, NULL, clk_hw_get_name(hw), 0, 1, 0, CLK_EVT_CLK_TRACE);

	return 0;
}

static void mtk_cg_disable(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	mtk_clk_notify(NULL, NULL, clk_hw_get_name(hw), 0, 0, 0, CLK_EVT_CLK_TRACE);
}

static void mtk_cg_disable_unused(struct clk_hw *hw)
{
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	mtk_cg_set_bit(hw);
}

static int mtk_cg_enable_inv(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	mtk_clk_notify(NULL, NULL, clk_hw_get_name(hw), 0, 1, 0, CLK_EVT_CLK_TRACE);

	return 0;
}

static void mtk_cg_disable_inv(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	mtk_clk_notify(NULL, NULL, clk_hw_get_name(hw), 0, 0, 0, CLK_EVT_CLK_TRACE);
}

static void mtk_cg_disable_unused_inv(struct clk_hw *hw)
{
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	mtk_cg_clr_bit(hw);
}

static int mtk_cg_is_set_hwv(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val = 0;

	if (!is_registered)
		return 0;

	regmap_read(cg->hwv_regmap, cg->hwv_set_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static int mtk_cg_is_done_hwv(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val = 0;

	regmap_read(cg->hwv_regmap, cg->hwv_sta_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static int __cg_enable_hwv(struct clk_hw *hw, bool inv)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val = 0, val2 = 0, val3 = 0;
	bool is_done = false;
	int i = 0;

	if (cg->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(true);

	regmap_write(cg->hwv_regmap, cg->hwv_set_ofs,
			BIT(cg->bit));

	while (!mtk_cg_is_set_hwv(hw)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (1) {
		if (!is_done)
			regmap_read(cg->hwv_regmap, cg->hwv_sta_ofs, &val);

		if ((val & BIT(cg->bit)) != 0)
			is_done = true;

		if (is_done) {
			regmap_read(cg->regmap, cg->sta_ofs, &val2);
			if ((inv && (val2 & BIT(cg->bit)) != 0) ||
					(!inv && (val2 & BIT(cg->bit)) == 0))
				break;
		}

		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;

		i++;
	}

	mtk_clk_notify(cg->regmap, cg->hwv_regmap, clk_hw_get_name(hw),
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_HWV_CG_CHK_PWR);

	if (cg->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return 0;

hwv_done_fail:
	regmap_read(cg->regmap, cg->sta_ofs, &val);
	regmap_read(cg->hwv_regmap, cg->hwv_sta_ofs, &val2);

	if (inv)
		regmap_write(cg->regmap, cg->set_ofs, BIT(cg->bit));
	else
		regmap_write(cg->regmap, cg->clr_ofs, BIT(cg->bit));
	regmap_read(cg->regmap, cg->sta_ofs, &val3);

	pr_err("%s cg enable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
	pr_err("%s cg rewrite(%x)\n", clk_hw_get_name(hw), val3);
hwv_prepare_fail:
	regmap_read(cg->regmap, cg->hwv_sta_ofs, &val);
	pr_err("%s cg prepare timeout(%x)\n", clk_hw_get_name(hw), val);

	mtk_clk_notify(cg->regmap, cg->hwv_regmap, clk_hw_get_name(hw),
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_HWV_CG_TIMEOUT);
	if (cg->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return -EBUSY;
}

static int mtk_cg_enable_hwv(struct clk_hw *hw)
{
	mtk_clk_notify(NULL, NULL, clk_hw_get_name(hw), 0, 1, 0, CLK_EVT_CLK_TRACE);

	return __cg_enable_hwv(hw, false);
}

static int mtk_cg_enable_hwv_inv(struct clk_hw *hw)
{
	mtk_clk_notify(NULL, NULL, clk_hw_get_name(hw), 0, 1, 0, CLK_EVT_CLK_TRACE);

	return __cg_enable_hwv(hw, true);
}

static void mtk_cg_disable_hwv(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;
	int i = 0;

	if (cg->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(true);

	/* dummy read to clr idle signal of hw voter bus */
	regmap_read(cg->hwv_regmap, cg->hwv_clr_ofs, &val);

	regmap_write(cg->hwv_regmap, cg->hwv_clr_ofs, BIT(cg->bit));

	while (mtk_cg_is_set_hwv(hw)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (!mtk_cg_is_done_hwv(hw)) {
		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;
		i++;
	}

	mtk_clk_notify(cg->regmap, cg->hwv_regmap, clk_hw_get_name(hw),
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_HWV_CG_CHK_PWR);

	mtk_clk_notify(NULL, NULL, clk_hw_get_name(hw), 0, 0, 0, CLK_EVT_CLK_TRACE);

	if (cg->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return;

hwv_done_fail:
	pr_err("%s cg disable timeout(%dus)\n", clk_hw_get_name(hw),
			i * MTK_WAIT_HWV_DONE_US);
hwv_prepare_fail:
	regmap_read(cg->regmap, cg->sta_ofs, &val);
	pr_err("%s cg unprepare timeout(%dus)(0x%x)\n", clk_hw_get_name(hw),
			i * MTK_WAIT_HWV_PREPARE_US, val);

	mtk_clk_notify(cg->regmap, cg->hwv_regmap, clk_hw_get_name(hw),
			cg->sta_ofs, (cg->hwv_set_ofs / MTK_HWV_ID_OFS),
			cg->bit, CLK_EVT_HWV_CG_TIMEOUT);
	if (cg->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);
}

static void mtk_cg_disable_unused_hwv(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	regmap_write(cg->hwv_regmap, cg->hwv_clr_ofs, BIT(cg->bit));
}

static int mtk_cg_enable_no_setclr(struct clk_hw *hw)
{
	mtk_cg_clr_bit_no_setclr(hw);

	return 0;
}

static void mtk_cg_disable_no_setclr(struct clk_hw *hw)
{
	mtk_cg_set_bit_no_setclr(hw);
}

static void mtk_cg_disable_unused_no_setclr(struct clk_hw *hw)
{
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	mtk_cg_set_bit_no_setclr(hw);
}


static int mtk_cg_enable_inv_no_setclr(struct clk_hw *hw)
{
	mtk_cg_set_bit_no_setclr(hw);

	return 0;
}

static void mtk_cg_disable_inv_no_setclr(struct clk_hw *hw)
{
	mtk_cg_clr_bit_no_setclr(hw);
}

static void mtk_cg_disable_unused_inv_no_setclr(struct clk_hw *hw)
{
	const char *c_n = clk_hw_get_name(hw);

	pr_notice("disable_unused - %s\n", c_n);
	mtk_cg_clr_bit_no_setclr(hw);
}

const struct clk_ops mtk_clk_gate_ops_setclr_dummy = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_setclr_dummy);

const struct clk_ops mtk_clk_gate_ops_setclr_dummys = {
	.is_enabled	= mtk_cg_bit_is_cleared,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_setclr_dummys);

const struct clk_ops mtk_clk_gate_ops_hwv_dummy = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable_hwv,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_hwv_dummy);

const struct clk_ops mtk_clk_gate_ops_setclr_inv_dummy = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
};
EXPORT_SYMBOL(mtk_clk_gate_ops_setclr_inv_dummy);

const struct clk_ops mtk_clk_gate_ops_setclr = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable,
	.disable_unused = mtk_cg_disable_unused,
};
EXPORT_SYMBOL_GPL(mtk_clk_gate_ops_setclr);

const struct clk_ops mtk_clk_gate_ops_setclr_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv,
	.disable_unused = mtk_cg_disable_unused_inv,
};
EXPORT_SYMBOL_GPL(mtk_clk_gate_ops_setclr_inv);

const struct clk_ops mtk_clk_gate_ops_hwv = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable_hwv,
	.disable	= mtk_cg_disable_hwv,
	.disable_unused = mtk_cg_disable_unused_hwv,
};
EXPORT_SYMBOL_GPL(mtk_clk_gate_ops_hwv);

const struct clk_ops mtk_clk_gate_ops_hwv_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_hwv_inv,
	.disable	= mtk_cg_disable_hwv,
	.disable_unused = mtk_cg_disable_unused_hwv,
};
EXPORT_SYMBOL_GPL(mtk_clk_gate_ops_hwv_inv);

const struct clk_ops mtk_clk_gate_ops_no_setclr = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable_no_setclr,
	.disable	= mtk_cg_disable_no_setclr,
	.disable_unused = mtk_cg_disable_unused_no_setclr,
};
EXPORT_SYMBOL_GPL(mtk_clk_gate_ops_no_setclr);

const struct clk_ops mtk_clk_gate_ops_no_setclr_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv_no_setclr,
	.disable	= mtk_cg_disable_inv_no_setclr,
	.disable_unused = mtk_cg_disable_unused_inv_no_setclr,
};
EXPORT_SYMBOL_GPL(mtk_clk_gate_ops_no_setclr_inv);

struct clk *mtk_clk_register_gate_hwv(
		const struct mtk_gate *gate,
		struct regmap *regmap,
		struct regmap *hwv_regmap,
		struct device *dev)
{
	struct mtk_clk_gate *cg;
	struct clk *clk;
	struct clk_init_data init = {};

	is_registered = false;

	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return ERR_PTR(-ENOMEM);

	init.name = gate->name;
	init.flags = gate->flags | CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE;
	init.parent_names = gate->parent_name ? &gate->parent_name : NULL;
	init.num_parents = gate->parent_name ? 1 : 0;
	if (gate->flags & CLK_USE_HW_VOTER) {
		if (hwv_regmap)
			init.ops = gate->ops;
		else
			init.ops = gate->dma_ops;
	} else
		init.ops = gate->ops;

	cg->regmap = regmap;
	cg->hwv_regmap = hwv_regmap;
	cg->set_ofs = gate->regs->set_ofs;
	cg->clr_ofs = gate->regs->clr_ofs;
	cg->sta_ofs = gate->regs->sta_ofs;
	cg->hwv_set_ofs = gate->hwv_regs->set_ofs;
	cg->hwv_clr_ofs = gate->hwv_regs->clr_ofs;
	cg->hwv_sta_ofs = gate->hwv_regs->sta_ofs;
	cg->bit = gate->shift;
	cg->flags = gate->flags;

	cg->hw.init = &init;

	clk = clk_register(dev, &cg->hw);
	if (IS_ERR(clk))
		kfree(cg);

	is_registered = true;

	return clk;
}
EXPORT_SYMBOL_GPL(mtk_clk_register_gate_hwv);

struct clk *mtk_clk_register_gate(
		const struct mtk_gate *gate,
		struct regmap *regmap,
		struct device *dev)
{
	struct mtk_clk_gate *cg;
	struct clk *clk;
	struct clk_init_data init = {};

	is_registered = false;

	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return ERR_PTR(-ENOMEM);

	init.name = gate->name;
	init.flags = gate->flags | CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE;
	init.parent_names = gate->parent_name ? &gate->parent_name : NULL;
	init.num_parents = gate->parent_name ? 1 : 0;
	if (gate->flags & CLK_USE_HW_VOTER)
		init.ops = gate->dma_ops;
	else
		init.ops = gate->ops;

	cg->regmap = regmap;
	cg->set_ofs = gate->regs->set_ofs;
	cg->clr_ofs = gate->regs->clr_ofs;
	cg->sta_ofs = gate->regs->sta_ofs;
	cg->bit = gate->shift;
	cg->flags = gate->flags;

	cg->hw.init = &init;

	clk = clk_register(dev, &cg->hw);
	if (IS_ERR(clk))
		kfree(cg);

	is_registered = true;

	return clk;
}
EXPORT_SYMBOL_GPL(mtk_clk_register_gate);

MODULE_LICENSE("GPL");
