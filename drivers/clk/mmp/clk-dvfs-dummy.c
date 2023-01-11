/*
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Fixed rate clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>

/*
 * DOC: basic fixed-rate clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_(un)prepare only ensures parents are prepared
 * enable - clk_enable only ensures parents are enabled
 * rate - rate is always a fixed value.  No clk_set_rate support
 * parent - fixed parent.  No clk_set_parent support
 */
struct clk_dvfs_dummy {
	struct		clk_hw hw;
	unsigned long	dummy_rate;
	u32		dummy_enable;
};

#define to_clk_dummy_rate(_hw) container_of(_hw, struct clk_dvfs_dummy, hw)

static unsigned long clk_dvfs_dummy_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return to_clk_dummy_rate(hw)->dummy_rate;
}

static long clk_dvfs_dummy_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	return rate;
}

static int clk_dvfs_dummy_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	to_clk_dummy_rate(hw)->dummy_rate = rate;

	return 0;
}

static void clk_dvfs_dummy_endis(struct clk_hw *hw, int enable)
{
	to_clk_dummy_rate(hw)->dummy_enable = enable;
}

static int clk_dvfs_dummy_enable(struct clk_hw *hw)
{
	clk_dvfs_dummy_endis(hw, 1);
	return 0;
}

static void clk_dvfs_dummy_disable(struct clk_hw *hw)
{
	clk_dvfs_dummy_endis(hw, 0);
}

static const struct clk_ops clk_dvfs_dummy_ops = {
	.recalc_rate = clk_dvfs_dummy_recalc_rate,
	.round_rate = clk_dvfs_dummy_round_rate,
	.set_rate = clk_dvfs_dummy_set_rate,
	.enable = clk_dvfs_dummy_enable,
	.disable = clk_dvfs_dummy_disable,
};

/**
 * clk_register_fixed_rate - register fixed-rate clock with the clock framework
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @fixed_rate: non-adjustable clock rate
 */
struct clk *mmp_clk_register_dvfs_dummy(const char *name,
			const char *parent_name, unsigned long flags,
			unsigned long init_rate)
{
	struct clk_dvfs_dummy *clkdummy;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate fixed-rate clock */
	clkdummy = kzalloc(sizeof(struct clk_dvfs_dummy), GFP_KERNEL);
	if (!clkdummy) {
		pr_err("%s: could not allocate dummy clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &clk_dvfs_dummy_ops;
	init.flags = flags | CLK_IS_ROOT;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_fixed_rate assignments */
	clkdummy->dummy_rate = init_rate;
	clkdummy->hw.init = &init;

	/* register the clock */
	clk = clk_register(NULL, &clkdummy->hw);
	if (IS_ERR(clk))
		kfree(clkdummy);

	return clk;
}
