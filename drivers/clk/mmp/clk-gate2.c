/*
 * mmp gate clock operation source file
 *
 * Copyright (C) 2014 Marvell
 * Chao Xie <chao.xie@marvell.com>
 * Yonghai Huang <huangyh@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk-provider.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>

#include "clk.h"

/*
 * Some clocks will have mutiple bits to enable the clocks, and
 * the bits to disable the clock is not same as enabling bits.
 */

#define to_clk_mmp_gate2(hw)	container_of(hw, struct mmp_clk_gate2, hw)

static int mmp_clk_gate2_enable(struct clk_hw *hw)
{
	struct mmp_clk_gate2 *gate = to_clk_mmp_gate2(hw);
	struct clk *clk = hw->clk;
	unsigned long flags = 0;
	unsigned long rate;
	u32 tmp;
	u32 divider;

	if (gate->lock)
		spin_lock_irqsave(gate->lock, flags);

	tmp = readl(gate->reg);
	divider = tmp & gate->mask;

	if (divider != gate->val_disable)
		gate->val_enable = divider;
	else
		gate->val_enable = gate->val_shadow;

	tmp &= ~gate->mask;
	tmp |= gate->val_enable;
	writel(tmp, gate->reg);

	if (gate->lock)
		spin_unlock_irqrestore(gate->lock, flags);

	if (gate->flags & MMP_CLK_GATE_NEED_DELAY) {
		rate = __clk_get_rate(clk);
		/* Need delay 2 cycles. */
		udelay(2000000/rate);
	}

	return 0;
}

static void mmp_clk_gate2_disable(struct clk_hw *hw)
{
	struct mmp_clk_gate2 *gate = to_clk_mmp_gate2(hw);
	unsigned long flags = 0;
	u32 tmp;

	if (gate->lock)
		spin_lock_irqsave(gate->lock, flags);

	tmp = readl(gate->reg);
	gate->val_shadow = tmp & gate->mask;
	tmp &= ~gate->mask;
	tmp |= gate->val_disable;
	writel(tmp, gate->reg);

	if (gate->lock)
		spin_unlock_irqrestore(gate->lock, flags);
}

static int mmp_clk_gate2_is_enabled(struct clk_hw *hw)
{
	struct mmp_clk_gate2 *gate = to_clk_mmp_gate2(hw);
	unsigned long flags = 0;
	u32 tmp;

	if (gate->lock)
		spin_lock_irqsave(gate->lock, flags);

	tmp = readl(gate->reg);

	if (gate->lock)
		spin_unlock_irqrestore(gate->lock, flags);

	return (tmp & gate->mask) != 0;
}

const struct clk_ops mmp_clk_gate2_ops = {
	.enable = mmp_clk_gate2_enable,
	.disable = mmp_clk_gate2_disable,
	.is_enabled = mmp_clk_gate2_is_enabled,
};

struct clk *mmp_clk_register_gate2(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u32 mask, u32 val_enable, u32 val_disable,
		unsigned int gate_flags, spinlock_t *lock)
{
	struct mmp_clk_gate2 *gate;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the gate */
	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate) {
		pr_err("%s:%s could not allocate gate clk\n", __func__, name);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &mmp_clk_gate2_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_gate assignments */
	gate->reg = reg;
	gate->mask = mask;
	gate->val_enable = val_enable;
	gate->val_disable = val_disable;
	gate->val_shadow = val_enable;
	gate->flags = gate_flags;
	gate->lock = lock;
	gate->hw.init = &init;

	clk = clk_register(dev, &gate->hw);

	if (IS_ERR(clk))
		kfree(gate);

	return clk;
}
