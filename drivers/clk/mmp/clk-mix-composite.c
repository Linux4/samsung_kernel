/*
 * mmp mix(div and mux) clock operation source file
 *
 * Copyright (C) 2014 Marvell
 * Chao Xie <chao.xie@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "clk.h"

#define to_clk_composite(_hw) container_of(_hw, struct mmp_clk_composite, hw)

static u8 mmp_clk_composite_get_parent(struct clk_hw *hw)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *mix_ops = composite->mix_ops;
	struct clk_hw *mix_hw = composite->mix_hw;

	mix_hw->clk = hw->clk;

	return mix_ops->get_parent(mix_hw);
}

static int mmp_clk_composite_set_parent(struct clk_hw *hw, u8 index)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *mix_ops = composite->mix_ops;
	struct clk_hw *mix_hw = composite->mix_hw;

	mix_hw->clk = hw->clk;

	return mix_ops->set_parent(mix_hw, index);
}

static int mmp_clk_composite_set_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long parent_rate)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *mix_ops = composite->mix_ops;
	struct clk_hw *mix_hw = composite->mix_hw;

	mix_hw->clk = hw->clk;

	return mix_ops->set_rate(mix_hw, rate, parent_rate);
}

static unsigned long mmp_clk_composite_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *mix_ops = composite->mix_ops;
	struct clk_hw *mix_hw = composite->mix_hw;

	mix_hw->clk = hw->clk;

	return mix_ops->recalc_rate(mix_hw, parent_rate);
}

static long mmp_clk_composite_determine_rate(struct clk_hw *hw,
					unsigned long rate,
					unsigned long *best_parent_rate,
					struct clk **best_parent_p)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *mix_ops = composite->mix_ops;
	struct clk_hw *mix_hw = composite->mix_hw;

	mix_hw->clk = hw->clk;

	return mix_ops->determine_rate(mix_hw, rate, best_parent_rate,
					best_parent_p);
}

static int mmp_clk_composite_set_rate_and_parent(struct clk_hw *hw,
					unsigned long rate,
					unsigned long parent_rate, u8 index)

{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *mix_ops = composite->mix_ops;
	struct clk_hw *mix_hw = composite->mix_hw;

	mix_hw->clk = hw->clk;

	return mix_ops->set_rate_and_parent(mix_hw, rate, parent_rate, index);
}

static int mmp_clk_composite_is_enabled(struct clk_hw *hw)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *gate_ops = composite->gate_ops;
	struct clk_hw *gate_hw = composite->gate_hw;

	gate_hw->clk = hw->clk;

	return gate_ops->is_enabled(gate_hw);
}

static int mmp_clk_composite_enable(struct clk_hw *hw)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *gate_ops = composite->gate_ops;
	struct clk_hw *gate_hw = composite->gate_hw;

	gate_hw->clk = hw->clk;

	return gate_ops->enable(gate_hw);
}

static void mmp_clk_composite_disable(struct clk_hw *hw)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *gate_ops = composite->gate_ops;
	struct clk_hw *gate_hw = composite->gate_hw;

	gate_hw->clk = hw->clk;

	gate_ops->disable(gate_hw);
}

static void mmp_clk_composite_init(struct clk_hw *hw)
{
	struct mmp_clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *gate_ops = composite->gate_ops;
	struct clk_hw *gate_hw = composite->gate_hw;
	const struct clk_ops *mix_ops = composite->mix_ops;
	struct clk_hw *mix_hw = composite->mix_hw;

	mix_hw->clk = hw->clk;
	gate_hw->clk = hw->clk;

	if (mix_ops->init)
		mix_ops->init(mix_hw);
	if (gate_ops->init)
		gate_ops->init(gate_hw);
}

static struct clk_ops mmp_clk_composite_ops = {
	.enable = mmp_clk_composite_enable,
	.disable = mmp_clk_composite_disable,
	.is_enabled = mmp_clk_composite_is_enabled,
	.determine_rate = mmp_clk_composite_determine_rate,
	.set_rate_and_parent = mmp_clk_composite_set_rate_and_parent,
	.set_rate = mmp_clk_composite_set_rate,
	.recalc_rate = mmp_clk_composite_recalc_rate,
	.get_parent = mmp_clk_composite_get_parent,
	.set_parent = mmp_clk_composite_set_parent,
	.init = mmp_clk_composite_init,
};

struct clk *mmp_clk_register_composite(struct device *dev, const char *name,
			const char **parent_names, int num_parents,
			struct clk_hw *mix_hw, const struct clk_ops *mix_ops,
			struct clk_hw *gate_hw, const struct clk_ops *gate_ops,
			unsigned long flags)
{
	struct clk *clk;
	struct clk_init_data init;
	struct mmp_clk_composite *composite;

	if (!mix_hw || !gate_hw)
		return ERR_PTR(-EINVAL);

	composite = kzalloc(sizeof(*composite), GFP_KERNEL);
	if (!composite) {
		pr_err("%s: could not allocate mmp composite clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.flags = flags;
	init.parent_names = parent_names;
	init.num_parents = num_parents;
	init.ops = &mmp_clk_composite_ops;

	composite->mix_hw = mix_hw;
	composite->mix_ops = mix_ops;
	composite->gate_hw = gate_hw;
	composite->gate_ops = gate_ops;
	composite->hw.init = &init;

	clk = clk_register(dev, &composite->hw);
	if (IS_ERR(clk))
		kfree(composite);

	return clk;
}
