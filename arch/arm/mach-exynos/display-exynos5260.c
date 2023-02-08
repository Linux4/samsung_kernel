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

#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/clock-clksrc.h>

#include "display-exynos5260.h"

void __init exynos5_keep_disp_clock(struct device *dev)
{
	struct clk *clk = clk_get(dev, "lcd");

	if (IS_ERR(clk)) {
		pr_err("failed to get lcd clock\n");
		return;
	}

	clk_enable(clk);
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
		clkval_f = gcd(div, div_limit);
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

	clk_put(clk);
	clk_put(clk_parent);

	return 0;
}

