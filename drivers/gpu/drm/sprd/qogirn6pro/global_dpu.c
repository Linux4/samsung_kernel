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
#include <linux/io.h>
#include <linux/of_address.h>

#include "global_dpu_qos.h"
#include "sprd_dpu.h"

static void __iomem *dpu_qos_base;
static struct clk *clk_dpuvsp_eb;
static struct clk *clk_dpuvsp_disp_eb;
static struct clk *clk_master_div6_eb;

static struct dpu_clk_context {
	struct clk *clk_src_256m;
	struct clk *clk_src_307m2;
	struct clk *clk_src_312m5;
	struct clk *clk_src_384m;
	struct clk *clk_src_409m6;
	struct clk *clk_src_416m7;
	struct clk *clk_src_512m;
	struct clk *clk_src_614m4;
	struct clk *clk_dpu_core;
	struct clk *clk_dpu_dpi;
} dpu_clk_ctx;

static struct qos_thres {
	u8 awqos_thres;
	u8 arqos_thres;
} qos_cfg;

enum {
	CLK_DPI_DIV6 = 6,
	CLK_DPI_DIV8 = 8
};


static const u32 dpu_core_clk[] = {
	256000000,
	307200000,
	384000000,
	409600000,
	512000000,
	614400000
};

static const u32 dpi_clk_src[] = {
	256000000,
	307200000,
	312500000,
	384000000,
	416700000
};

static struct dpu_glb_context {
	unsigned int enable_reg;
	unsigned int mask_bit;
	struct regmap *regmap;
} ctx_reset, vau_reset, ctx_qos;

static struct clk *val_to_clk(struct dpu_clk_context *ctx, u32 val)
{
	switch (val) {
	case 256000000:
		return ctx->clk_src_256m;
	case 307200000:
		return ctx->clk_src_307m2;
	case 384000000:
		return ctx->clk_src_384m;
	case 409600000:
		return ctx->clk_src_409m6;
	case 512000000:
		return ctx->clk_src_512m;
	case 614400000:
		return ctx->clk_src_614m4;
	default:
		pr_err("invalid clock value %u\n", val);
		return NULL;
	}
}

static int dpu_clk_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	clk_ctx->clk_src_256m =
		of_clk_get_by_name(np, "clk_src_256m");
	clk_ctx->clk_src_307m2 =
		of_clk_get_by_name(np, "clk_src_307m2");
	clk_ctx->clk_src_312m5 =
		of_clk_get_by_name(np, "clk_src_312m5");
	clk_ctx->clk_src_384m =
		of_clk_get_by_name(np, "clk_src_384m");
	clk_ctx->clk_src_409m6 =
		of_clk_get_by_name(np, "clk_src_409m6");
	clk_ctx->clk_src_416m7 =
		of_clk_get_by_name(np, "clk_src_416m7");
	clk_ctx->clk_src_512m =
		of_clk_get_by_name(np, "clk_src_512m");
	clk_ctx->clk_src_614m4 =
		of_clk_get_by_name(np, "clk_src_614m4");
	clk_ctx->clk_dpu_core =
		of_clk_get_by_name(np, "clk_dpu_core");
	clk_ctx->clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");

	if (IS_ERR(clk_ctx->clk_src_256m)) {
		pr_warn("read clk_src_256m failed\n");
		clk_ctx->clk_src_256m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_307m2)) {
		pr_warn("read clk_src_307m2 failed\n");
		clk_ctx->clk_src_307m2 = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_312m5)) {
		pr_warn("read clk_src_312m5 failed\n");
		clk_ctx->clk_src_384m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_384m)) {
		pr_warn("read clk_src_384m failed\n");
		clk_ctx->clk_src_384m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_409m6)) {
		pr_warn("read clk_src_409m6 failed\n");
		clk_ctx->clk_src_409m6 = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_416m7)) {
		pr_warn("read clk_src_416m7 failed\n");
		clk_ctx->clk_src_384m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_512m)) {
		pr_warn("read clk_src_512m failed\n");
		clk_ctx->clk_src_512m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_614m4)) {
		pr_warn("read clk_src_614m4 failed\n");
		clk_ctx->clk_src_614m4 = NULL;
	}

	if (IS_ERR(clk_ctx->clk_dpu_core)) {
		pr_warn("read clk_dpu_core failed\n");
		clk_ctx->clk_dpu_core = NULL;
	}

	if (IS_ERR(clk_ctx->clk_dpu_dpi)) {
		pr_warn("read clk_dpu_dpi failed\n");
		clk_ctx->clk_dpu_dpi = NULL;
	}

	return 0;
}

static u32 calc_dpu_core_clk(void)
{
	return dpu_core_clk[ARRAY_SIZE(dpu_core_clk) - 1];
}

static u32 calc_dpi_clk_src(u32 pclk)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dpi_clk_src); i++) {
		if ((dpi_clk_src[i] % pclk) == 0)
			return dpi_clk_src[i];
	}

	pr_err("calc DPI_CLK_SRC failed, use default\n");
	return 96000000;
}

static struct clk *div_to_clk(struct dpu_clk_context *clk_ctx, u32 clk_div)
{
	switch (clk_div) {
	case CLK_DPI_DIV6:
		return clk_ctx->clk_src_416m7;
	case CLK_DPI_DIV8:
		return clk_ctx->clk_src_312m5;
	default:
		pr_err("invalid clock value %u\n", clk_div);
		return NULL;
	}
}


static int dpu_clk_init(struct dpu_context *ctx)
{
	int ret;
	u32 dpu_core_val;
	u32 dpi_src_val;
	struct clk *clk_src;
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx,
				struct sprd_dpu, ctx);

	dpu_core_val = calc_dpu_core_clk();

	if (dpu->dsi->ctx.dpi_clk_div) {
		pr_info("DPU_CORE_CLK = %u, DPI_CLK_DIV = %d\n",
				dpu_core_val, dpu->dsi->ctx.dpi_clk_div);
	} else {
		dpi_src_val = calc_dpi_clk_src(ctx->vm.pixelclock);
		pr_info("DPU_CORE_CLK = %u, DPI_CLK_SRC = %u\n",
				dpu_core_val, dpi_src_val);
		pr_info("dpi clock is %lu\n", ctx->vm.pixelclock);
	}

	clk_src = val_to_clk(clk_ctx, dpu_core_val);
	ret = clk_set_parent(clk_ctx->clk_dpu_core, clk_src);
	if (ret)
		pr_warn("set dpu core clk source failed\n");

	if (dpu->dsi->ctx.dpi_clk_div) {
		clk_src = div_to_clk(clk_ctx, dpu->dsi->ctx.dpi_clk_div);
		ret = clk_set_parent(clk_ctx->clk_dpu_dpi, clk_src);
		if (ret)
			pr_warn("set dpi clk source failed\n");
	} else {
		clk_src = val_to_clk(clk_ctx, dpi_src_val);
		ret = clk_set_parent(clk_ctx->clk_dpu_dpi, clk_src);
		if (ret)
			pr_warn("set dpi clk source failed\n");

		ret = clk_set_rate(clk_ctx->clk_dpu_dpi, ctx->vm.pixelclock);
		if (ret)
			pr_err("dpu update dpi clk rate failed\n");
	}

	return ret;
}

static int dpu_clk_enable(struct dpu_context *ctx)
{
	int ret;
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;
	static bool div6_uboot_enable = true;
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx,
				struct sprd_dpu, ctx);

	ret = clk_prepare_enable(clk_ctx->clk_dpu_core);
	if (ret) {
		pr_err("enable clk_dpu_core error\n");
		return ret;
	}

	ret = clk_prepare_enable(clk_ctx->clk_dpu_dpi);
	if (ret) {
		pr_err("enable clk_dpu_dpi error\n");
		clk_disable_unprepare(clk_ctx->clk_dpu_core);
		return ret;
	}

	if (dpu->dsi->ctx.dpi_clk_div) {
		if (div6_uboot_enable) {
			div6_uboot_enable = false;
			return 0;
		}

		ret = clk_prepare_enable(clk_master_div6_eb);
		if (ret) {
			pr_err("enable clk_master_div6_eb error\n");
			return ret;
		}
		clk_disable_unprepare(clk_master_div6_eb);

	}

	return 0;
}

int dpu_r6p0_enable_div6_clk(struct dpu_context *ctx)
{
	int ret;
	struct clk *clk_src;
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx,
				struct sprd_dpu, ctx);

	clk_src = div_to_clk(clk_ctx, dpu->dsi->ctx.dpi_clk_div);
	ret = clk_set_parent(clk_ctx->clk_dpu_dpi, clk_src);
	if (ret)
		pr_warn("set dpi clk source failed\n");

	return ret;
}

static int dpu_clk_disable(struct dpu_context *ctx)
{
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	clk_disable_unprepare(clk_ctx->clk_dpu_dpi);
	clk_disable_unprepare(clk_ctx->clk_dpu_core);

	clk_set_parent(clk_ctx->clk_dpu_dpi, clk_ctx->clk_src_256m);
	clk_set_parent(clk_ctx->clk_dpu_core, clk_ctx->clk_src_307m2);

	return 0;
}

static int dpu_glb_parse_dt(struct dpu_context *ctx,
		struct device_node *np)
{
	unsigned int syscon_args[2];
	struct device_node *qos_np = NULL;
	int ret;
	struct resource r;

	ctx_reset.regmap = syscon_regmap_lookup_by_name(np, "reset");
	if (IS_ERR(ctx_reset.regmap)) {
		pr_warn("failed to map dpu glb reg: reset\n");
		return PTR_ERR(ctx_reset.regmap);
	}

	ret = syscon_get_args_by_name(np, "reset", 2, syscon_args);
	if (ret == 2) {
		ctx_reset.enable_reg = syscon_args[0];
		ctx_reset.mask_bit = syscon_args[1];
	} else {
		pr_warn("failed to parse dpu glb reg: reset\n");
	}

	vau_reset.regmap = syscon_regmap_lookup_by_name(np, "vau_reset");
	if (IS_ERR(vau_reset.regmap)) {
		pr_warn("failed to map dpu glb reg: vau_reset\n");
		return PTR_ERR(vau_reset.regmap);
	}

	ret = syscon_get_args_by_name(np, "vau_reset", 2, syscon_args);
	if (ret == 2) {
		vau_reset.enable_reg = syscon_args[0];
		vau_reset.mask_bit = syscon_args[1];
	} else {
		pr_warn("failed to parse dpu glb reg: vau_reset\n");
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

	clk_master_div6_eb =
		of_clk_get_by_name(np, "clk_master_div6_eb");
	if (IS_ERR(clk_master_div6_eb)) {
		pr_warn("read clk_master_div6_eb failed\n");
		clk_master_div6_eb = NULL;
	}

	if (of_address_to_resource(np, 1, &r))
		DRM_ERROR("parse dt soc qos base address failed\n");
	else {

		dpu_qos_base = ioremap_nocache(r.start,
				resource_size(&r));
		if (!dpu_qos_base)
			pr_warn("dpu_qos_base ioremap fail\n");
	}

	ctx_qos.regmap = syscon_regmap_lookup_by_name(np, "qos");
	if (IS_ERR(ctx_qos.regmap)) {
		pr_warn("failed to map dpu glb reg: qos\n");
		return PTR_ERR(ctx_qos.regmap);
	}

	ret = syscon_get_args_by_name(np, "qos", 2, syscon_args);
	if (ret == 2) {
		ctx_qos.enable_reg = syscon_args[0];
		ctx_qos.mask_bit = syscon_args[1];
	} else {
		pr_warn("failed to parse dpu glb reg: qos\n");
	}

	qos_np = of_parse_phandle(np, "sprd,qos", 0);
	if (!qos_np)
		pr_warn("can't find dpu qos cfg node\n");

	ret = of_property_read_u8(qos_np, "awqos-threshold",
					&qos_cfg.awqos_thres);
	if (ret)
		pr_warn("read awqos-threshold failed, use default\n");

	ret = of_property_read_u8(qos_np, "arqos-threshold",
					&qos_cfg.arqos_thres);
	if (ret)
		pr_warn("read arqos-threshold failed, use default\n");

	return 0;
}

static void dpu_glb_enable(struct dpu_context *ctx)
{
}

static void dpu_glb_disable(struct dpu_context *ctx)
{
	clk_disable_unprepare(clk_dpuvsp_disp_eb);
	clk_disable_unprepare(clk_dpuvsp_eb);
}

static void dpu_soc_qos_config(void)
{
	unsigned int i;

	if (!dpu_qos_base) {
		pr_warn("dpu_qos_base is NULL\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(dpu_mtx_qos); i++)
		writel_bits_relaxed(dpu_mtx_qos[i].mask,
			dpu_mtx_qos[i].value,
			dpu_qos_base + dpu_mtx_qos[i].offset);
}

static void dpu_reset(struct dpu_context *ctx)
{
	regmap_update_bits(ctx_reset.regmap,
			ctx_reset.enable_reg,
			ctx_reset.mask_bit,
			ctx_reset.mask_bit);
	udelay(10);
	regmap_update_bits(ctx_reset.regmap,
			ctx_reset.enable_reg,
			ctx_reset.mask_bit,
			(unsigned int)(~ctx_reset.mask_bit));
	regmap_update_bits(vau_reset.regmap,
			vau_reset.enable_reg,
			vau_reset.mask_bit,
			vau_reset.mask_bit);
	udelay(10);
	regmap_update_bits(vau_reset.regmap,
			vau_reset.enable_reg,
			vau_reset.mask_bit,
			(unsigned int)(~vau_reset.mask_bit));

	dpu_soc_qos_config();
}

static void dpu_power_domain(struct dpu_context *ctx, int enable)
{
#if 0
	if (enable)
		regmap_update_bits(ctx_qos.regmap,
			    ctx_qos.enable_reg,
			    ctx_qos.mask_bit,
			    qos_cfg.awqos_thres |
			    qos_cfg.arqos_thres << 4);
#endif
}

static struct dpu_clk_ops dpu_clk_ops = {
	.parse_dt = dpu_clk_parse_dt,
	.init = dpu_clk_init,
	.enable = dpu_clk_enable,
	.disable = dpu_clk_disable,
};

static struct dpu_glb_ops dpu_glb_ops = {
	.parse_dt = dpu_glb_parse_dt,
	.reset = dpu_reset,
	.enable = dpu_glb_enable,
	.disable = dpu_glb_disable,
	.power = dpu_power_domain,
};

static struct ops_entry clk_entry = {
	.ver = "qogirn6pro",
	.ops = &dpu_clk_ops,
};

static struct ops_entry glb_entry = {
	.ver = "qogirn6pro",
	.ops = &dpu_glb_ops,
};

static int __init dpu_glb_register(void)
{
	dpu_clk_ops_register(&clk_entry);
	dpu_glb_ops_register(&glb_entry);
	return 0;
}

subsys_initcall(dpu_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Junxiao.feng@unisoc.com");
MODULE_DESCRIPTION("sprd qogirn6pro dpu global and clk regs config");
