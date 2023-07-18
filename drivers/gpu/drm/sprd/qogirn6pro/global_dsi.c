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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dsi.h"

static struct clk *clk_dsi0_eb;
static struct clk *clk_dsi1_eb;
static struct clk *clk_dpu_dpi;
static struct clk *clk_src_384m;
static struct clk *clk_dpuvsp_eb;
static struct clk *clk_dpuvsp_disp_eb;

static struct dsi_glb_context {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	struct regmap *regmap;
} ctx_reset, s_ctx_reset;

static int dsi_glb_parse_dt(struct dsi_context *ctx,
		struct device_node *np)
{
	unsigned int syscon_args[2];
	int ret;


	clk_dsi0_eb =
		of_clk_get_by_name(np, "clk_dsi0_eb");
	if (IS_ERR(clk_dsi0_eb)) {
		pr_warn("read clk_dsi0_eb failed\n");
		clk_dsi0_eb = NULL;
	}

	clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");
	if (IS_ERR(clk_dpu_dpi)) {
		pr_warn("read clk_dpu_dpi failed\n");
		clk_dpu_dpi = NULL;
	}

	clk_src_384m =
		of_clk_get_by_name(np, "clk_src_384m");
	if (IS_ERR(clk_src_384m)) {
		pr_warn("read clk_src_384m failed\n");
		clk_src_384m = NULL;
	}

	clk_dpuvsp_eb =
		of_clk_get_by_name(np, "clk_dpuvsp_eb");
	if (IS_ERR(clk_dpuvsp_eb)) {
		pr_warn("read clk_dpuvsp_eb failed\n");
		clk_dpuvsp_eb = NULL;
	}

	clk_dpuvsp_disp_eb =
		of_clk_get_by_name(np, "clk_dpuvsp_disp_eb");
	if (IS_ERR(clk_dpuvsp_disp_eb)) {
		pr_warn("read clk_dpuvsp_disp_eb failed\n");
		clk_dpuvsp_disp_eb = NULL;
	}

	ctx_reset.regmap = syscon_regmap_lookup_by_name(np, "reset");
	if (IS_ERR(ctx_reset.regmap)) {
		pr_warn("failed to map dsi glb reg\n");
		return PTR_ERR(ctx_reset.regmap);
	}

	ret = syscon_get_args_by_name(np, "reset", 2, syscon_args);
	if (ret == 2) {
		ctx_reset.ctrl_reg = syscon_args[0];
		ctx_reset.ctrl_mask = syscon_args[1];
	} else {
		pr_warn("failed to parse dsi glb reg\n");
	}

	return 0;
}

static int dsi_s_glb_parse_dt(struct dsi_context *ctx,
				struct device_node *np)
{
	unsigned int syscon_args[2];
	int ret;

	pr_info("%s enter\n", __func__);

	clk_dsi1_eb =
		of_clk_get_by_name(np, "clk_dsi1_eb");
	if (IS_ERR(clk_dsi1_eb)) {
		pr_warn("read clk_dsi1_eb failed\n");
		clk_dsi1_eb = NULL;
	}

	s_ctx_reset.regmap = syscon_regmap_lookup_by_name(np, "reset");
	if (IS_ERR(s_ctx_reset.regmap)) {
		pr_warn("failed to map dsi glb reg\n");
		return PTR_ERR(s_ctx_reset.regmap);
	}

	ret = syscon_get_args_by_name(np, "reset", 2, syscon_args);
	if (ret == 2) {
		s_ctx_reset.ctrl_reg = syscon_args[0];
		s_ctx_reset.ctrl_mask = syscon_args[1];
	} else {
		pr_warn("failed to parse dsi glb reg\n");
	}
	return 0;
}

static int dsi_core_clk_switch(struct dsi_context *ctx)
{
	int ret;

	ret = clk_set_parent(clk_dpu_dpi, clk_src_384m);
	if (ret) {
		pr_err("clk_dpu_dpi set 384m error\n");
		return ret;
	}
	ret = clk_set_rate(clk_dpu_dpi, 384000000);
	if (ret)
		pr_err("dpi clk rate failed\n");

	return ret;
}

static void dsi_glb_enable(struct dsi_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_dpuvsp_eb);
	if (ret) {
		pr_err("enable clk_dpuvsp_eb failed!\n");
		return;
	}

	ret = clk_prepare_enable(clk_dpuvsp_disp_eb);
	if (ret) {
		pr_err("enable clk_dpuvsp_disp_eb failed!\n");
		return;
	}

	ret = clk_prepare_enable(clk_dsi0_eb);
	if (ret)
		pr_err("enable clk_dsi0_eb failed!\n");
}

static void dsi_s_glb_enable(struct dsi_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_dsi1_eb);
	if (ret)
		pr_err("enable clk_dsi1_eb failed!\n");
}

static void dsi_glb_disable(struct dsi_context *ctx)
{
	if (ctx->clk_dpi_384m) {
		dsi_core_clk_switch(ctx);
		ctx->clk_dpi_384m = false;
	} else {
		clk_disable_unprepare(clk_dsi0_eb);
	}
}

static void dsi_s_glb_disable(struct dsi_context *ctx)
{
	clk_disable_unprepare(clk_dsi1_eb);
}

static void dsi_reset(struct dsi_context *ctx)
{
	regmap_update_bits(ctx_reset.regmap,
			ctx_reset.ctrl_reg,
			ctx_reset.ctrl_mask,
			ctx_reset.ctrl_mask);
	udelay(10);
	regmap_update_bits(ctx_reset.regmap,
			ctx_reset.ctrl_reg,
			ctx_reset.ctrl_mask,
			(unsigned int)(~ctx_reset.ctrl_mask));
}

static void dsi_s_reset(struct dsi_context *ctx)
{
	regmap_update_bits(s_ctx_reset.regmap,
			s_ctx_reset.ctrl_reg,
			s_ctx_reset.ctrl_mask,
			s_ctx_reset.ctrl_mask);
	udelay(10);
	regmap_update_bits(s_ctx_reset.regmap,
			s_ctx_reset.ctrl_reg,
			s_ctx_reset.ctrl_mask,
			(unsigned int)(~s_ctx_reset.ctrl_mask));
}

static struct dsi_glb_ops dsi_glb_ops = {
	.parse_dt = dsi_glb_parse_dt,
	.reset = dsi_reset,
	.enable = dsi_glb_enable,
	.disable = dsi_glb_disable,
};

static struct dsi_glb_ops dsi_s_glb_ops = {
	.parse_dt = dsi_s_glb_parse_dt,
	.reset = dsi_s_reset,
	.enable = dsi_s_glb_enable,
	.disable = dsi_s_glb_disable,
};

static struct ops_entry entry = {
	.ver = "qogirn6pro",
	.ops = &dsi_glb_ops,
};

static struct ops_entry entry_slave = {
	.ver = "qogirn6pro_s",
	.ops = &dsi_s_glb_ops,
};

static int __init dsi_glb_register(void)
{
	return dsi_glb_ops_register(&entry);
}

subsys_initcall(dsi_glb_register);

static int __init dsi_s_glb_register(void)
{
	return dsi_glb_ops_register(&entry_slave);
}

subsys_initcall(dsi_s_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Junxiao.feng@unisoc.com");
MODULE_DESCRIPTION("sprd qogirn6pro dsi global APB regs low-level config");
