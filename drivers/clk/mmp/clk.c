#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk.h"

void mmp_clk_init(struct device_node *np, struct mmp_clk_unit *unit,
		int nr_clks)
{
	static struct clk **clk_table;

	clk_table = kzalloc(sizeof(struct clk *) * nr_clks, GFP_KERNEL);
	if (!clk_table) {
		pr_err("%s: could not allocate clock lookup table\n", __func__);
		return;
	}
	unit->clk_table = clk_table;
	unit->nr_clks = nr_clks;
	unit->clk_data.clks = clk_table;
	unit->clk_data.clk_num = nr_clks;
	of_clk_add_provider(np, of_clk_src_onecell_get, &unit->clk_data);
}

void mmp_register_fixed_rate_clks(struct mmp_clk_unit *unit,
				struct mmp_param_fixed_rate_clk *clks,
				int size)
{
	int i;
	struct clk *clk;

	for (i = 0; i < size; i++) {
		clk = clk_register_fixed_rate(NULL, clks[i].name,
					clks[i].parent_name,
					clks[i].flags,
					clks[i].fixed_rate);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
			       __func__, clks[i].name);
			continue;
		}
		unit->clk_table[clks[i].id] = clk;
	}
}

void mmp_register_fixed_factor_clks(struct mmp_clk_unit *unit,
				struct mmp_param_fixed_factor_clk *clks,
				int size)
{
	struct clk *clk;
	int i;

	for (i = 0; i < size; i++) {
		clk = clk_register_fixed_factor(NULL, clks[i].name,
						clks[i].parent_name,
						clks[i].flags, clks[i].mult,
						clks[i].div);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
			       __func__, clks[i].name);
			continue;
		}
		unit->clk_table[clks[i].id] = clk;
	}
}

void mmp_register_general_gate_clks(struct mmp_clk_unit *unit,
				struct mmp_param_general_gate_clk *clks,
				void __iomem *base, int size)
{
	struct clk *clk;
	int i;

	for (i = 0; i < size; i++) {
		clk = clk_register_gate(NULL, clks[i].name,
					clks[i].parent_name,
					clks[i].flags,
					base + clks[i].offset,
					clks[i].bit_idx,
					clks[i].gate_flags,
					clks[i].lock);

		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
			       __func__, clks[i].name);
			continue;
		}
		unit->clk_table[clks[i].id] = clk;
	}
}

void mmp_register_gate_clks(struct mmp_clk_unit *unit,
			struct mmp_param_gate_clk *clks,
			void __iomem *base, int size)
{
	struct clk *clk;
	int i;

	for (i = 0; i < size; i++) {
		clk = mmp_clk_register_gate(NULL, clks[i].name,
					clks[i].parent_name,
					clks[i].flags,
					base + clks[i].offset,
					clks[i].mask,
					clks[i].val_enable,
					clks[i].val_disable,
					clks[i].gate_flags,
					clks[i].lock);

		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
			       __func__, clks[i].name);
			continue;
		}
		unit->clk_table[clks[i].id] = clk;
	}
}

void mmp_clk_add(struct mmp_clk_unit *unit, unsigned int id,
			struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk)) {
		pr_err("CLK %d has invalid pointer %p\n", id, clk);
		return;
	}
	if (id > unit->nr_clks) {
		pr_err("CLK %d is invalid\n", id);
		return;
	}

	unit->clk_table[id] = clk;
}
