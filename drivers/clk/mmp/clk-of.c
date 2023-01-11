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
#include <linux/list.h>
#include <linux/of.h>

#include "clk.h"

static int of_mmp_clk_get_flags(struct device_node *np,
				unsigned long *flags)
{
	*flags = 0;

	return 0;
}

static int of_mmp_clk_get_bits(struct device_node *np, const char *name,
				u8 *width, u8 *shift)
{
	int ret;
	u32 tmp[2];

	ret = of_property_read_u32_array(np, name, tmp, 2);
	if (ret) {
		pr_err("%s:%s failed to read bits %s\n",
			__func__, np->name, name);
		return -EINVAL;
	}

	*width = tmp[0];
	*shift = tmp[1];

	return 0;
}

static int of_mmp_clk_div_dt_parse(struct device_node *np, u8 *shift,
					u8 *width,
					struct clk_div_table **ptable,
					u8 *div_flags)
{
	int i, ret;
	const __be32 *prop;
	unsigned int proplen;
	struct clk_div_table *table;
	unsigned int size;

	ret = of_mmp_clk_get_bits(np, "marvell,mmp-clk-bits-div",
				width, shift);
	if (ret)
		return ret;

	*div_flags = 0;
	if (of_property_read_bool(np, "marvell,mmp-clk-div-power-of-two"))
		*div_flags |= CLK_DIVIDER_POWER_OF_TWO;
	else if (of_property_read_bool(np, "marvell,mmp-clk-div-one-based"))
		*div_flags |= CLK_DIVIDER_ONE_BASED;

	if (ptable)
		*ptable = NULL;

	prop = of_get_property(np, "marvell,mmp-clk-div-table", &proplen);
	if (prop) {
		if (!ptable)
			return -EINVAL;

		size = proplen / sizeof(u32);
		if ((proplen % sizeof(u32)) || size % 2) {
			pr_err("%s:%s marvell,mmp-clk-div-table wrong value\n",
				__func__, np->name);
			return -EINVAL;
		}
		table = kzalloc(sizeof(*table) * (size / 2 + 1), GFP_KERNEL);
		if (!table) {
			pr_err("%s:%s failed to allocate table\n",
				__func__, np->name);
			return -EINVAL;
		}

		for (i = 0; i < size; i += 2) {
			table[i / 2].val = be32_to_cpup(prop + i);
			table[i / 2].div = be32_to_cpup(prop + i + 1);
		}
		/* For safe. */
		table[i / 2].val = 0;
		table[i / 2].div = 0;

		*ptable = table;
	}

	return 0;
}

static int of_mmp_clk_mux_dt_parse(struct device_node *np, u8 *shift,
					u8 *width,
					u32 **ptable, u8 *mux_flags)
{
	int ret;
	const __be32 *prop;
	unsigned int proplen;
	u32 *table;
	unsigned int size;
	int i;

	ret = of_mmp_clk_get_bits(np, "marvell,mmp-clk-bits-mux",
				width, shift);
	if (ret)
		return ret;

	*mux_flags = 0;
	if (of_property_read_bool(np, "marvell,mmp-clk-mux-index-bits"))
		*mux_flags |= CLK_MUX_INDEX_BIT;
	else if (of_property_read_bool(np, "marvell,mmp-clk-mux_index-one"))
		*mux_flags |= CLK_MUX_INDEX_ONE;

	if (ptable)
		*ptable = NULL;

	prop = of_get_property(np, "marvell,mmp-clk-mux-table", &proplen);
	if (prop) {
		if (!ptable)
			return -EINVAL;

		size = proplen / sizeof(u32);
		table = kzalloc(sizeof(*table) * size, GFP_KERNEL);
		if (!table) {
			pr_err("%s:%s failed to allocate table\n",
				__func__, np->name);
			return -EINVAL;
		}

		for (i = 0; i < size; i++)
			table[i] = be32_to_cpup(prop + i);

		*ptable = table;
	}

	return 0;
}

static int of_mmp_clk_general_gate_dt_parse(struct device_node *np,
					u8 *bit_idx, u8 *gate_flags)
{
	int ret;
	u32 tmp;

	ret = of_property_read_u32(np, "marvell,mmp-clk-bit-gate", &tmp);
	if (ret) {
		pr_err("%s:%s can not find marvell,mmp-clk-bit-gate\n",
			__func__, np->name);
		return -EINVAL;
	}
	*bit_idx = tmp;

	*gate_flags = 0;

	return 0;
}

static int of_mmp_clk_gate_dt_parse(struct device_node *np,
				u32 *mask, u32 *val_enable, u32 *val_disable,
				unsigned int *gate_flags)
{
	int ret;
	u32 tmp[3];

	ret = of_property_read_u32_array(np, "marvell,mmp-clk-mask", tmp, 3);
	if (ret) {
		pr_err("%s:%s can not find marvell,mmp-clk-mask\n",
			__func__, np->name);
		return -EINVAL;
	}
	*mask = tmp[0];
	*val_enable = tmp[1];
	*val_disable = tmp[2];

	*gate_flags = 0;
	if (of_property_read_bool(np, "marvell,mmp-clk-gate-need-delay"))
		*gate_flags |= MMP_CLK_GATE_NEED_DELAY;

	return 0;
}


static int of_mmp_clk_mix_dt_parse(struct device_node *np,
					struct mmp_clk_mix_config *config,
					spinlock_t **plock)
{
	struct mmp_clk_mix_reg_info *ri;
	struct mmp_clk_mix_clk_table *table;
	int i, ret, size;
	u32 tmp;
	spinlock_t *lock;
	void __iomem *reg;
	unsigned int reg_phys;
	const __be32 *prop;
	unsigned int proplen;

	ri = &config->reg_info;
	ret = of_mmp_clk_div_dt_parse(np, &ri->shift_div, &ri->width_div,
					&config->div_table, &config->div_flags);
	if (ret)
		return ret;

	ret = of_mmp_clk_mux_dt_parse(np, &ri->shift_mux, &ri->width_mux,
					&config->mux_table,
					&config->mux_flags);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "marvell,mmp-clk-bit-fc", &tmp);
	if (ret)
		ri->bit_fc = (u8)-1;
	else
		ri->bit_fc = tmp;

	reg = of_mmp_clk_get_reg(np, 0, &reg_phys);
	if (!reg)
		return -EINVAL;
	ri->reg_clk_ctrl = reg;

	lock = of_mmp_clk_get_spinlock(np, reg_phys);
	if (!lock)
		return -EINVAL;

	*plock = lock;
	reg = of_mmp_clk_get_reg(np, 1, &reg_phys);
	if (reg)
		ri->reg_clk_sel = reg;

	prop = of_get_property(np, "marvell,mmp-clk-mix-table", &proplen);
	if (prop) {
		size = proplen / sizeof(u32);
		if ((proplen % sizeof(u32)) || size % 2) {
			pr_err("%s:%s marvell,mmp-clk-mix-table wrong value\n",
				__func__, np->name);
			return -EINVAL;
		}
		table = kzalloc(sizeof(*table) * (size / 2), GFP_KERNEL);
		if (!table) {
			pr_err("%s:%s failed to allocate table\n",
				__func__, np->name);
			return -EINVAL;
		}

		for (i = 0; i < size; i += 2) {
			table[i / 2].rate = be32_to_cpup(prop + i);
			table[i / 2].parent_index = be32_to_cpup(prop + i + 1);
		}
		config->table = table;
		config->table_size = size / 2;
	} else {
		config->table = NULL;
		config->table_size = 0;
	}

	return 0;
}

static int of_mmp_clk_factor_dt_parse(struct device_node *np,
					struct mmp_clk_factor_masks **pmasks,
					struct mmp_clk_factor_tbl **pftbl,
					unsigned int *pftbl_cnt)
{
	struct mmp_clk_factor_masks *masks;
	struct mmp_clk_factor_tbl *table;
	u8 width, shift;
	int i, ret, size;
	u32 tmp;
	const __be32 *prop;
	unsigned int proplen;

	masks = kzalloc(sizeof(*masks), GFP_KERNEL);
	if (!masks) {
		pr_err("%s:%s failed to allocate factor masks\n",
			__func__, np->name);
		return -ENOMEM;
	}

	ret = of_property_read_u32(np, "marvell,mmp-clk-factor-factor", &tmp);
	if (ret) {
		pr_err("%s:%s can not find marvell,mmp-clk-factor-num\n",
			__func__, np->name);
		return -EINVAL;
	}
	masks->factor = tmp;

	ret = of_mmp_clk_get_bits(np, "marvell,mmp-clk-factor-bits-num",
				&width, &shift);
	if (ret)
		return ret;
	masks->num_mask = BIT(width) - 1;
	masks->num_shift = shift;

	ret = of_mmp_clk_get_bits(np, "marvell,mmp-clk-factor-bits-den",
				&width, &shift);
	if (ret)
		return ret;
	masks->den_mask = BIT(width) - 1;
	masks->den_shift = shift;
	*pmasks = masks;

	prop = of_get_property(np, "marvell,mmp-clk-factor-table", &proplen);
	if (!prop) {
		pr_err("%s:%s failed to get marvell,mmp-clk-factor-table\n",
			__func__, np->name);
		return -EINVAL;
	}

	size = proplen / sizeof(u32);
	if ((proplen % sizeof(u32)) || size % 2) {
		pr_err("%s:%s marvell,mmp-clk-factor-table wrong value\n",
			__func__, np->name);
		return -EINVAL;
	}
	table = kzalloc(sizeof(*table) * (size / 2), GFP_KERNEL);
	if (!table) {
		pr_err("%s:%s failed to allocate table\n",
			__func__, np->name);
		return -EINVAL;
	}

	for (i = 0; i < size; i += 2) {
		table[i / 2].num = be32_to_cpup(prop + i);
		table[i / 2].den = be32_to_cpup(prop + i + 1);
	}
	*pftbl = table;
	*pftbl_cnt = size / 2;

	return 0;
}

static void of_mmp_clk_mix_setup(struct device_node *np)
{
	struct mmp_clk_mix *mix;
	struct mmp_clk_mix_config config;
	struct clk *clk;
	spinlock_t *lock;
	unsigned int num_parents;
	const char **parent_names;
	int i, ret;

	ret = of_mmp_clk_mix_dt_parse(np, &config, &lock);
	if (ret)
		return;

	if (of_mmp_clk_is_composite(np)) {
		mix = kzalloc(sizeof(*mix), GFP_KERNEL);
		if (!mix) {
			pr_err("%s:%s failed to allocate clk\n",
				__func__, np->name);
			return;
		}
		memcpy(&mix->reg_info, &config.reg_info,
			sizeof(config.reg_info));
		mix->div_flags = config.div_flags;
		mix->mux_flags = config.mux_flags;
		mix->lock = lock;
		if (config.table) {
			mix->table = config.table;
			mix->table_size = config.table_size;
		}

		of_mmp_clk_composite_add_member(np, &mix->hw, &mmp_clk_mix_ops,
					MMP_CLK_COMPOSITE_TYPE_MUXMIX);
	} else {
		num_parents = of_clk_get_parent_count(np);
		parent_names = kcalloc(num_parents, sizeof(char *),
					GFP_KERNEL);
		if (!parent_names) {
			pr_err("%s:%s failed to allocate parent_names\n",
				__func__, np->name);
			return;
		}
		for (i = 0; i < num_parents; i++)
			parent_names[i] = of_clk_get_parent_name(np, i);

		clk = mmp_clk_register_mix(NULL, np->name, parent_names,
					num_parents, 0, &config, lock);

		if (IS_ERR(clk)) {
			kfree(parent_names);

			pr_err("%s:%s failed to register clk\n",
				__func__, np->name);
			return;
		}

		of_clk_add_provider(np, of_clk_src_simple_get, clk);
	}
}
CLK_OF_DECLARE(mmp_clk_mix, "marvell,mmp-clk-mix",
		of_mmp_clk_mix_setup);

static void of_mmp_clk_div_setup(struct device_node *np)
{
	struct clk_divider *div;
	void __iomem *reg;
	u8 width, shift, div_flags;
	struct clk_div_table *table;
	unsigned long flags;
	const char *parent_name;
	struct clk *clk;
	unsigned int reg_phys;
	spinlock_t *lock;
	int ret;

	reg = of_mmp_clk_get_reg(np, 0, &reg_phys);
	if (!reg)
		return;

	ret = of_mmp_clk_div_dt_parse(np, &shift, &width, &table, &div_flags);
	if (ret)
		return;

	ret = of_mmp_clk_get_flags(np, &flags);
	if (ret)
		return;

	lock = of_mmp_clk_get_spinlock(np, reg_phys);
	if (!lock)
		return;

	if (of_mmp_clk_is_composite(np)) {
		div = kzalloc(sizeof(*div), GFP_KERNEL);
		if (!div) {
			pr_err("%s:%s failed to allocate clk\n",
				__func__, np->name);
			return;
		}
		div->shift = shift;
		div->width = width;
		div->table = table;
		div->flags = div_flags;
		div->lock = lock;
		div->reg = reg;

		of_mmp_clk_composite_add_member(np, &div->hw, &clk_divider_ops,
					MMP_CLK_COMPOSITE_TYPE_DIV);
	} else {
		parent_name = of_clk_get_parent_name(np, 0);

		if (!table)
			clk = clk_register_divider(NULL, np->name, parent_name,
					flags, reg, shift, width, div_flags,
					lock);
		else
			clk = clk_register_divider_table(NULL, np->name,
					parent_name, flags, reg, shift, width,
					div_flags, table, lock);
		if (IS_ERR(clk)) {
			pr_err("%s:%s failed to register clk\n",
				__func__, np->name);
			return;
		}

		of_clk_add_provider(np, of_clk_src_simple_get, clk);
	}
}
CLK_OF_DECLARE(mmp_clk_div, "marvell,mmp-clk-div",
		of_mmp_clk_div_setup);

static void of_mmp_clk_mux_setup(struct device_node *np)
{
	struct clk_mux *mux;
	void __iomem *reg;
	u8 width, shift, mux_flags;
	unsigned long flags;
	spinlock_t *lock;
	unsigned int num_parents;
	const char **parent_names;
	struct clk *clk;
	unsigned int reg_phys;
	u32 *table;
	int i, ret;

	reg = of_mmp_clk_get_reg(np, 0, &reg_phys);
	if (!reg)
		return;

	ret = of_mmp_clk_mux_dt_parse(np, &shift, &width, &table, &mux_flags);
	if (ret)
		return;

	ret = of_mmp_clk_get_flags(np, &flags);
	if (ret)
		return;

	lock = of_mmp_clk_get_spinlock(np, reg_phys);
	if (!lock)
		return;

	if (of_mmp_clk_is_composite(np)) {
		mux = kzalloc(sizeof(*mux), GFP_KERNEL);
		if (!mux) {
			pr_err("%s:%s failed to allocate clk\n",
				__func__, np->name);
			return;
		}

		mux->reg = reg;
		mux->mask = BIT(width) - 1;
		mux->shift = shift;
		mux->lock = lock;
		mux->flags = mux_flags;
		mux->table = table;
		of_mmp_clk_composite_add_member(np, &mux->hw, &clk_mux_ops,
					MMP_CLK_COMPOSITE_TYPE_MUXMIX);
	} else {
		num_parents = of_clk_get_parent_count(np);
		parent_names = kcalloc(num_parents, sizeof(char *),
					GFP_KERNEL);
		if (!parent_names) {
			pr_err("%s:%s failed to allocate parent_names\n",
				__func__, np->name);
			return;
		}
		for (i = 0; i < num_parents; i++)
			parent_names[i] = of_clk_get_parent_name(np, i);

		if (!table)
			clk = clk_register_mux(NULL, np->name, parent_names,
					num_parents, flags,
					reg, shift, width, mux_flags, lock);
		else
			clk = clk_register_mux_table(NULL, np->name,
					parent_names, num_parents, flags,
					reg, shift, width, mux_flags,
					table, lock);
		if (IS_ERR(clk)) {
			pr_err("%s:%s failed to register clk\n",
				__func__, np->name);
			return;
		}

		of_clk_add_provider(np, of_clk_src_simple_get, clk);
	}
}
CLK_OF_DECLARE(mmp_clk_mux, "marvell,mmp-clk-mux",
		of_mmp_clk_mux_setup);

static void of_mmp_clk_general_gate_setup(struct device_node *np)
{
	struct clk_gate *gate;
	void __iomem *reg;
	u8 bit_idx, gate_flags;
	unsigned long flags;
	spinlock_t *lock;
	const char *parent_name;
	struct clk *clk;
	unsigned int reg_phys;
	int ret;

	reg = of_mmp_clk_get_reg(np, 0, &reg_phys);
	if (!reg)
		return;

	ret = of_mmp_clk_general_gate_dt_parse(np, &bit_idx, &gate_flags);
	if (ret)
		return;

	ret = of_mmp_clk_get_flags(np, &flags);
	if (ret)
		return;

	lock = of_mmp_clk_get_spinlock(np, reg_phys);
	if (!lock)
		return;

	if (of_mmp_clk_is_composite(np)) {
		gate = kzalloc(sizeof(*gate), GFP_KERNEL);
		if (!gate) {
			pr_err("%s:%s failed to allocate clk\n",
				__func__, np->name);
			return;
		}
		gate->bit_idx = bit_idx;
		gate->flags = gate_flags;
		gate->reg = reg;
		gate->lock = lock;
		of_mmp_clk_composite_add_member(np, &gate->hw, &clk_gate_ops,
					MMP_CLK_COMPOSITE_TYPE_GATE);
	} else {
		parent_name = of_clk_get_parent_name(np, 0);

		clk = clk_register_gate(NULL, np->name, parent_name, flags,
					reg, bit_idx, gate_flags, lock);
		if (IS_ERR(clk)) {
			pr_err("%s:%s failed to register clk\n",
				__func__, np->name);
			return;
		}

		of_clk_add_provider(np, of_clk_src_simple_get, clk);
	}
}
CLK_OF_DECLARE(mmp_clk_general_gate, "marvell,mmp-clk-general-gate",
		of_mmp_clk_general_gate_setup);

static void of_mmp_clk_gate_setup(struct device_node *np)
{
	struct mmp_clk_gate *gate;
	void __iomem *reg;
	u32 mask, val_enable, val_disable;
	unsigned int gate_flags;
	unsigned long flags;
	spinlock_t *lock;
	const char *parent_name;
	struct clk *clk;
	unsigned int reg_phys;
	int ret;

	reg = of_mmp_clk_get_reg(np, 0, &reg_phys);
	if (!reg)
		return;

	ret = of_mmp_clk_gate_dt_parse(np, &mask, &val_enable, &val_disable,
					&gate_flags);
	if (ret)
		return;

	ret = of_mmp_clk_get_flags(np, &flags);
	if (ret)
		return;

	lock = of_mmp_clk_get_spinlock(np, reg_phys);
	if (!lock)
		return;

	if (of_mmp_clk_is_composite(np)) {
		gate = kzalloc(sizeof(*gate), GFP_KERNEL);
		if (!gate) {
			pr_err("%s:%s failed to allocate clk\n",
				__func__, np->name);
			return;
		}

		gate->flags = gate_flags;
		gate->mask = mask;
		gate->val_enable = val_enable;
		gate->val_disable = val_disable;
		gate->reg = reg;
		gate->lock = lock;
		of_mmp_clk_composite_add_member(np, &gate->hw,
				&mmp_clk_gate_ops,
				MMP_CLK_COMPOSITE_TYPE_GATE);
	} else {
		parent_name = of_clk_get_parent_name(np, 0);

		clk = mmp_clk_register_gate(NULL, np->name, parent_name, flags,
					reg, mask, val_enable, val_disable,
					gate_flags, lock);
		if (IS_ERR(clk)) {
			pr_err("%s:%s failed to register clk\n",
				__func__, np->name);
			return;
		}

		of_clk_add_provider(np, of_clk_src_simple_get, clk);
	}
}
CLK_OF_DECLARE(mmp_clk_gate, "marvell,mmp-clk-gate",
		of_mmp_clk_gate_setup);

static void of_mmp_clk_factor_setup(struct device_node *np)
{
	void __iomem *reg;
	struct mmp_clk_factor_masks *masks;
	struct mmp_clk_factor_tbl *table;
	unsigned int table_size;
	unsigned long flags;
	spinlock_t *lock;
	const char *parent_name;
	struct clk *clk;
	unsigned int reg_phys;
	int ret;

	reg = of_mmp_clk_get_reg(np, 0, &reg_phys);
	if (!reg)
		return;

	ret = of_mmp_clk_factor_dt_parse(np, &masks, &table, &table_size);
	if (ret)
		return;

	ret = of_mmp_clk_get_flags(np, &flags);
	if (ret)
		return;

	lock = of_mmp_clk_get_spinlock(np, reg_phys);
	if (!lock)
		return;

	parent_name = of_clk_get_parent_name(np, 0);

	clk = mmp_clk_register_factor(np->name, parent_name, flags,
					reg, masks, table, table_size,
					lock);
	if (IS_ERR(clk)) {
		pr_err("%s:%s failed to register clk\n", __func__, np->name);
		return;
	}

	of_clk_add_provider(np, of_clk_src_simple_get, clk);

}
CLK_OF_DECLARE(mmp_clk_factor,
		"marvell,mmp-clk-factor",
		of_mmp_clk_factor_setup);

void mmp_clk_of_init(void)
{
	struct device_node *next;

	next = NULL;
	do {
		next = of_mmp_clk_master_init(next);
	} while (next);
}
