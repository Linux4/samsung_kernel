/*
 * display clock framework source file
 *
 * Copyright (C) 2014 Marvell
 * huang yonghai <huangyh@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/devfreq.h>
#include <linux/clk-private.h>

#include "clk.h"
#include "mmp_ctrl.h"

static int src_sel_offset = 29;
static int src_sel_mask = 0x7;
static const char *dsi_parent[] = {"dsi_sclk"};
static u32 dsi_mux_tbl[] = {0, 3};
static const char *dsisclk_parent[8];
static struct clk *parent_clks[8];
static int parent_count;

struct mmp_clk_disp {
	struct clk_hw	hw;
	struct clk_mux mux;
	struct clk_divider divider;
	struct mmp_clk_gate2 gate2;
	struct mmp_clk_gate gate;
	spinlock_t *lock;
	struct clk *disp1_clk;
	struct clk *dsipll_clk;
	const struct clk_ops *mux_ops;
	const struct clk_ops *div_ops;
	const struct clk_ops *gate_ops;
};

static DEFINE_SPINLOCK(disp_lock);

static struct mmp_clk_disp pnpath = {
	.div_ops = &clk_divider_ops,
	.divider.width = 8,
	.divider.shift = 0,
	.divider.reg = 0,
	.divider.flags = CLK_DIVIDER_ONE_BASED,
	.divider.lock = &disp_lock,
	.gate.mask = 0x10000000,
	.gate.lock = &disp_lock,
	.gate.mask = 0x10000000,
	.gate.val_enable = 0x0,
	.gate.val_disable = 0x10000000,
	.gate.flags = 0x0,
	.gate.lock = &disp_lock,
	.gate_ops = &mmp_clk_gate_ops,
};

static struct mmp_clk_disp dsipath = {
	.div_ops = &clk_divider_ops,
	.divider.width = 4,
	.divider.shift = 8,
	.divider.reg = 0,
	.divider.flags = CLK_DIVIDER_ONE_BASED,
	.divider.lock = &disp_lock,
	.gate2.mask = 0xf00,
	.gate2.lock = &disp_lock,
	.gate2.val_disable = 0x0,
	.gate2.val_enable = 0x100,
	.gate2.val_shadow = 0x100,
	.gate2.flags = 0x0,
	.gate2.lock = &disp_lock,
	.gate_ops = &mmp_clk_gate2_ops,
};

long mmp_disp_clk_round_rate(struct mmphw_ctrl *ctrl, unsigned long rate)
{
	struct clk *clk, *parent;
	int i, j, num_parents, div;
	long new_rate, best_new_rate, parent_rate;
	int diff, min_diff = rate;

	best_new_rate = rate;

	for (i = 0; i < parent_count; i++) {
		clk = parent_clks[i];
		if (IS_ERR(clk)) {
			pr_err("%s, can't get parent clk, round rate failure\n", __func__);
			return best_new_rate;
		}

		num_parents = clk->num_parents;
		for (j = 0; j < num_parents; j++) {
			parent = clk_get_parent_by_index(clk, j);
			if (!parent)
				continue;
			parent_rate = __clk_get_rate(parent);
			for (div = 1; div < 16; div++) {
				new_rate = parent_rate / div;
				diff = (new_rate > rate) ? (new_rate - rate) : (rate - new_rate);
				if (diff < min_diff) {
					min_diff = diff;
					best_new_rate = new_rate;
				}
			}
		}
	}

	return best_new_rate;
}

int mmp_display_clk_init(struct mmphw_ctrl *ctrl)
{
	struct clk *clk;
	void __iomem *pn_sclk_reg = ctrl->reg_base + LCD_SCLK_DIV;
	static const char *dsipll_parent;

	if (!DISP_GEN4(ctrl->version)) {
		src_sel_offset = 30;
		src_sel_mask = 0x3;
	}

	if (DISP_GEN4_LITE(ctrl->version))
		src_sel_mask = 0x3;

	pnpath.disp1_clk = devm_clk_get(ctrl->dev, "disp1_clk");
	if (IS_ERR(pnpath.disp1_clk)) {
		pr_err("%s, can't get parent disp1 clk\n", __func__);
		pnpath.disp1_clk = NULL;
		return -EINVAL;
	} else {
		parent_clks[parent_count] = pnpath.disp1_clk;
		dsisclk_parent[parent_count] = __clk_get_name(pnpath.disp1_clk);
		parent_count++;
	}

	pnpath.dsipll_clk = devm_clk_get(ctrl->dev, "dsipll_clk");
	if (IS_ERR(pnpath.dsipll_clk))
		pnpath.dsipll_clk = NULL;
	else {
		/* pxa1L88 dsi clk have more parent clks, so the "dsi_pll" are register in clk driver  */
		if (!strcmp(__clk_get_name(pnpath.dsipll_clk), "dsi_pll")) {
			parent_clks[parent_count] = pnpath.dsipll_clk;
			dsisclk_parent[parent_count] = __clk_get_name(pnpath.dsipll_clk);
		} else {
			dsipll_parent = __clk_get_name(pnpath.dsipll_clk);
			clk = clk_register_mux_table(NULL, "dsi_pll", &dsipll_parent,
				1, 0, pn_sclk_reg, src_sel_offset, src_sel_mask,
				0, dsi_mux_tbl, &disp_lock);
			if (IS_ERR(clk))
				pr_err("%s, mmp register dsipll clk error\n", __func__);
			clk_register_clkdev(clk, "dsi_pll", NULL);
			dsisclk_parent[parent_count] = "dsi_pll";
			parent_clks[parent_count] = clk;
		}
		parent_count++;
	}

	clk = clk_register_mux_table(NULL, "dsi_sclk", dsisclk_parent,
		parent_count, CLK_SET_RATE_PARENT,
		pn_sclk_reg, src_sel_offset, src_sel_mask, 0, dsi_mux_tbl, &disp_lock);
	if (IS_ERR(clk))
		pr_err("%s, mmp register dsisclk clk error\n", __func__);
	clk_register_clkdev(clk, "dsi_sclk", NULL);

	pnpath.divider.reg = pn_sclk_reg;
	pnpath.gate.reg = pn_sclk_reg;
	clk = clk_register_composite(NULL, "mmp_pnpath", dsi_parent,
				ARRAY_SIZE(dsi_parent),
				NULL, NULL,
				&pnpath.divider.hw, pnpath.div_ops,
				&pnpath.gate.hw, pnpath.gate_ops,
				0);
	if (IS_ERR(clk))
		pr_err("%s, mmp register mmp_pnpath clk error\n", __func__);
	clk_register_clkdev(clk, "mmp_pnpath", NULL);

	dsipath.divider.reg = pn_sclk_reg;
	dsipath.gate2.reg = pn_sclk_reg;
	clk = clk_register_composite(NULL, "mmp_dsi1", dsi_parent,
				ARRAY_SIZE(dsi_parent),
				NULL, NULL,
				&dsipath.divider.hw, dsipath.div_ops,
				&dsipath.gate2.hw, dsipath.gate_ops,
				CLK_SET_RATE_PARENT);
	if (IS_ERR(clk))
		pr_err("%s, mmp register mmp_dsi1 clk error\n", __func__);
	clk_register_clkdev(clk, "mmp_dsi1", NULL);

	return 0;
}
