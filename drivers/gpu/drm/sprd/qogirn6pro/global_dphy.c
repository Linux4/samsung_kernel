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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dphy.h"

static struct dphy_glb_context {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	struct regmap *regmap;
} ctx_enable, ctx_power, s_ctx_enable, s_ctx_power, ctx_aod_mode, ctx_aod_pd;

static int dphy_glb_parse_dt(struct dphy_context *ctx,
				struct device_node *np)
{
	unsigned int syscon_args[2];
	int ret;

	ctx_enable.regmap = syscon_regmap_lookup_by_name(np, "enable");
	if (IS_ERR(ctx_enable.regmap)) {
		pr_warn("failed to map dphy glb reg: enable\n");
		return PTR_ERR(ctx_enable.regmap);
	}

	ret = syscon_get_args_by_name(np, "enable", 2, syscon_args);
	if (ret == 2) {
		ctx_enable.ctrl_reg = syscon_args[0];
		ctx_enable.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: enable\n");

	ctx_power.regmap = syscon_regmap_lookup_by_name(np, "power");
	if (IS_ERR(ctx_power.regmap)) {
		pr_warn("failed to map dphy glb reg: power\n");
		return PTR_ERR(ctx_power.regmap);
	}

	ret = syscon_get_args_by_name(np, "power", 2, syscon_args);
	if (ret == 2) {
		ctx_power.ctrl_reg = syscon_args[0];
		ctx_power.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: power");

	ctx_aod_mode.regmap = syscon_regmap_lookup_by_name(np, "aod_mode");
	if (IS_ERR(ctx_aod_mode.regmap)) {
		pr_warn("failed to map dphy glb reg: aod_mode\n");
		return PTR_ERR(ctx_aod_mode.regmap);
	}

	ret = syscon_get_args_by_name(np, "aod_mode", 2, syscon_args);
	if (ret == 2) {
		ctx_aod_mode.ctrl_reg = syscon_args[0];
		ctx_aod_mode.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: aod_mode");

	ctx_aod_pd.regmap = syscon_regmap_lookup_by_name(np, "aod_pd");
	if (IS_ERR(ctx_aod_pd.regmap)) {
		pr_warn("failed to map dphy glb reg: aod_mode\n");
		return PTR_ERR(ctx_aod_pd.regmap);
	}

	ret = syscon_get_args_by_name(np, "aod_pd", 2, syscon_args);
	if (ret == 2) {
		ctx_aod_pd.ctrl_reg = syscon_args[0];
		ctx_aod_pd.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: aod_pd");

	return 0;
}

static int dphy_s_glb_parse_dt(struct dphy_context *ctx,
				struct device_node *np)
{
	unsigned int syscon_args[2];
	int ret;

	pr_info("%s enter\n", __func__);

	s_ctx_enable.regmap = syscon_regmap_lookup_by_name(np, "enable");
	if (IS_ERR(s_ctx_enable.regmap)) {
		pr_warn("failed to map dphy glb reg: enable\n");
		return PTR_ERR(s_ctx_enable.regmap);
	}

	ret = syscon_get_args_by_name(np, "enable", 2, syscon_args);
	if (ret == 2) {
		s_ctx_enable.ctrl_reg = syscon_args[0];
		s_ctx_enable.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: enable\n");

	s_ctx_power.regmap = syscon_regmap_lookup_by_name(np, "power");
	if (IS_ERR(s_ctx_power.regmap)) {
		pr_warn("failed to map dphy glb reg: power\n");
		return PTR_ERR(s_ctx_power.regmap);
	}

	ret = syscon_get_args_by_name(np, "power", 2, syscon_args);
	if (ret == 2) {
		s_ctx_power.ctrl_reg = syscon_args[0];
		s_ctx_power.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: power");

	return 0;
}
static void dphy_glb_enable(struct dphy_context *ctx)
{
	if (ctx->aod_mode)
		regmap_update_bits(ctx_aod_mode.regmap,
				ctx_aod_mode.ctrl_reg,
				ctx_aod_mode.ctrl_mask,
				ctx_enable.ctrl_mask & (ctx->aod_mode << 9));
	if (ctx->aod_mode == 5)
		regmap_update_bits(ctx_aod_pd.regmap,
				ctx_aod_pd.ctrl_reg,
				ctx_aod_pd.ctrl_mask,
				(unsigned int)(~ctx_aod_pd.ctrl_mask));

	regmap_update_bits(ctx_enable.regmap,
		ctx_enable.ctrl_reg,
		ctx_enable.ctrl_mask,
		ctx_enable.ctrl_mask);
}

static void dphy_s_glb_enable(struct dphy_context *ctx)
{
	regmap_update_bits(s_ctx_enable.regmap,
		s_ctx_enable.ctrl_reg,
		s_ctx_enable.ctrl_mask,
		s_ctx_enable.ctrl_mask);
}

static void dphy_glb_disable(struct dphy_context *ctx)
{

	if (ctx->aod_mode == 5)
		regmap_update_bits(ctx_aod_pd.regmap,
				ctx_aod_pd.ctrl_reg,
				ctx_aod_pd.ctrl_mask,
				ctx_aod_pd.ctrl_mask);

	regmap_update_bits(ctx_enable.regmap,
		ctx_enable.ctrl_reg,
		ctx_enable.ctrl_mask,
		(unsigned int)(~ctx_enable.ctrl_mask));
}

static void dphy_s_glb_disable(struct dphy_context *ctx)
{
	regmap_update_bits(s_ctx_enable.regmap,
		s_ctx_enable.ctrl_reg,
		s_ctx_enable.ctrl_mask,
		(unsigned int)(~s_ctx_enable.ctrl_mask));
}

static void dphy_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(ctx_power.regmap,
			ctx_power.ctrl_reg,
			ctx_power.ctrl_mask,
			(unsigned int)(~ctx_power.ctrl_mask));

		/* Dphy has a random wakeup failed after poweron,
		 * this will caused testclr reset failed and
		 * writing pll configuration parameter failed.
		 * Delay 100us after dphy poweron, waiting for pll is stable.
		 */
		udelay(100);
	} else {
		regmap_update_bits(ctx_power.regmap,
			ctx_power.ctrl_reg,
			ctx_power.ctrl_mask,
			ctx_power.ctrl_mask);
	}
}

static void dphy_s_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(s_ctx_power.regmap,
			s_ctx_power.ctrl_reg,
			s_ctx_power.ctrl_mask,
			(unsigned int)(~s_ctx_power.ctrl_mask));

		/* Dphy has a random wakeup failed after poweron,
		 * this will caused testclr reset failed and
		 * writing pll configuration parameter failed.
		 * Delay 100us after dphy poweron, waiting for pll is stable.
		 */
		udelay(100);
	} else {
		regmap_update_bits(s_ctx_power.regmap,
			s_ctx_power.ctrl_reg,
			s_ctx_power.ctrl_mask,
			s_ctx_power.ctrl_mask);
	}
}

static struct dphy_glb_ops dphy_glb_ops = {
	.parse_dt = dphy_glb_parse_dt,
	.enable = dphy_glb_enable,
	.disable = dphy_glb_disable,
	.power = dphy_power_domain,
};

static struct dphy_glb_ops dphy_s_glb_ops = {
	.parse_dt = dphy_s_glb_parse_dt,
	.enable = dphy_s_glb_enable,
	.disable = dphy_s_glb_disable,
	.power = dphy_s_power_domain,
};

static struct ops_entry entry = {
	.ver = "qogirn6pro",
	.ops = &dphy_glb_ops,
};

static struct ops_entry entry_slave = {
	.ver = "qogirn6pro_s",
	.ops = &dphy_s_glb_ops,
};

static int __init dphy_glb_register(void)
{
	return dphy_glb_ops_register(&entry);
}

subsys_initcall(dphy_glb_register);

static int __init dphy_s_glb_register(void)
{
	return dphy_glb_ops_register(&entry_slave);
}

subsys_initcall(dphy_s_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Junxiao.feng@unisoc.com");
MODULE_DESCRIPTION("sprd qogirn6pro dphy global AHB&APB regs low-level config");
