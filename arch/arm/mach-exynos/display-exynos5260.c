/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/gcd.h>
#include <linux/clk-provider.h>

#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/clock-clksrc.h>

#include "display-exynos5260.h"

struct disp_clk {
	struct list_head	list;
	char			*name;
	unsigned long		rate;
};

static LIST_HEAD(disp_clk_list);

void __init exynos5_keep_disp_clock(struct device *dev)
{
	struct clk *clk = clk_get(dev, "lcd");

	if (IS_ERR(clk)) {
		pr_err("failed to get lcd clock\n");
		return;
	}

	clk_enable(clk);
}

int exynos_clk_recover_rate(struct clk *clk)
{
	struct disp_clk *pos;

	list_for_each_entry(pos, &disp_clk_list, list) {
		if (!strcmp(pos->name, clk->name)) {
			clk_set_rate(clk, pos->rate);
			return 0;
		}
	}

	/* if you can't search name, it means initial state, so add it */
	pos = kzalloc(sizeof(struct disp_clk), GFP_KERNEL);
	if (!pos) {
		pr_err("fail to allocate %s\n", __func__);
		return -ENOMEM;
	}

	pos->rate = clk_get_rate(clk);
	pos->name = kzalloc(strlen(clk->name) + 1, GFP_KERNEL);
	strcpy(pos->name, clk->name);

	list_add(&pos->list, &disp_clk_list);

	pr_info("backup %s clk rate as %ld\n", clk->name, clk_get_rate(clk));

	return 0;
}

static unsigned long __init get_clk_rate(struct clk *clk, struct clk *clk_parent, struct panel_info *info)
{
	unsigned long rate, rate_parent;
	unsigned int div, div_limit, div_max, clkval_f;
	struct clksrc_clk *clksrc = to_clksrc(clk);

	rate = (info->hbp + info->hfp + info->hsw + info->xres) *
		(info->vbp + info->vfp + info->vsw + info->yres);

	rate_parent = clk_get_rate(clk_parent);

	div_max = 1 << clksrc->reg_div.size;

	div = DIV_ROUND_CLOSEST(rate_parent, rate * info->refresh);

	if (info->limit) {
		div_limit = DIV_ROUND_CLOSEST(rate_parent, rate * info->limit);
		clkval_f = (max(div, div_limit) > div_max) ? gcd(div, div_limit) : 1;
		div /= clkval_f;
	}

	if (div > div_max) {
		for (clkval_f = 2; clkval_f <= div; clkval_f++) {
			if (!(div%clkval_f) && (div/clkval_f <= div_max))
				break;
		}
		div /= clkval_f;
	}

	div = (div > div_max) ? div_max : div;
	rate = rate_parent / div;

	if ((rate_parent % rate) && (div != 1)) {
		div--;
		rate = rate_parent / div;
		if (!(rate_parent % rate))
			rate--;
	}

	return rate;
}

int __init exynos_fimd_set_rate(struct device *dev, const char *clk_name,
		const char *parent_clk_name, struct panel_info *info)
{
	struct clk *clk_parent;
	struct clk *clk;
	unsigned long rate;
	struct clksrc_clk *clksrc;

	clk = clk_get(dev, clk_name);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	clk_parent = clk_get(NULL, parent_clk_name);
	if (IS_ERR(clk_parent)) {
		clk_put(clk);
		return PTR_ERR(clk_parent);
	}

	clksrc = to_clksrc(clk);

	rate = get_clk_rate(clk, clk_parent, info);

	exynos5_fimd1_setup_clock(dev, clk_name, parent_clk_name, rate);

	pr_info("%s: %ld, %s: %ld, clkdiv: 0x%08x\n", clk_parent->name, clk_get_rate(clk_parent),
		clk->name, clk_get_rate(clk), __raw_readl(clksrc->reg_div.reg));

	exynos_clk_recover_rate(clk);

	clk_put(clk);
	clk_put(clk_parent);

	return 0;
}

