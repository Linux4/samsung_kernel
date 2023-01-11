/*
 * mmp master clock source file
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
#include <linux/of_address.h>
#include <linux/ioport.h>

#include "clk.h"

#define MAX_REG		8

struct mmp_clk_master_node {
	unsigned int reg_base[MAX_REG];
	void __iomem *reg[MAX_REG];
	struct device_node *np;
	struct list_head node;
};

static LIST_HEAD(master_list);
static DEFINE_MUTEX(master_mutex);

static void mmp_clk_master_setup(struct device_node *np)
{
	struct mmp_clk_master_node *node;
	struct resource res;
	int i, ret;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		pr_err("%s:%s failed to allocate master node.\n",
			__func__, np->name);
		return;
	}

	for (i = 0; i < MAX_REG; i++) {
		ret = of_address_to_resource(np, i, &res);
		if (ret)
			break;
		node->reg_base[i] = res.start;
		node->reg[i] = ioremap(res.start, resource_size(&res));
		if (!node->reg[i]) {
			pr_err("%s:%s failed to map register.\n",
				__func__, np->name);
			goto error;
		}
	}

	node->np = np;
	INIT_LIST_HEAD(&node->node);

	mutex_lock(&master_mutex);

	list_add(&node->node, &master_list);

	mutex_unlock(&master_mutex);

	return;
error:
	for (i--; i >= 0; i--)
		iounmap(node->reg[i]);

	kfree(node);
}

struct of_device_id mmp_clk_master_of_id[] = {
	{
		.compatible = "marvell,mmp-clk-master",
		.data = mmp_clk_master_setup,
	},
	{ },
};

static struct mmp_clk_master_node *get_master_node(struct device_node *child)
{
	struct device_node *master;
	struct mmp_clk_master_node *node;

	/* Find the master device node */
	master = child;
	do {
		master = of_get_next_parent(master);
	} while (!of_match_node(mmp_clk_master_of_id, master));

	mutex_lock(&master_mutex);

	list_for_each_entry(node, &master_list, node) {
		if (node->np == master) {
			mutex_unlock(&master_mutex);
			return node;
		}
	}

	mutex_unlock(&master_mutex);

	return NULL;
}

static void __iomem *get_child_reg(struct device_node *child,
				unsigned int reg_index,
				unsigned int *reg_base)
{
	struct mmp_clk_master_node *node;

	if (reg_index >= MAX_REG) {
		pr_err("%s:%s reg_index too big.\n", __func__, child->name);
		return NULL;
	}

	node = get_master_node(child);
	if (!node) {
		pr_err("%s:%s failed to get master node\n",
			__func__, child->name);
		return NULL;
	}

	*reg_base = node->reg_base[reg_index];

	return node->reg[reg_index];
}

void __iomem *of_mmp_clk_get_reg(struct device_node *np,
					unsigned int index,
					unsigned int *reg_phys)
{
	const __be32 *prop;
	unsigned int proplen, size;
	u32 reg_index, reg_offset;
	unsigned int reg_base;
	void __iomem *reg;

	prop = of_get_property(np, "marvell,reg-offset", &proplen);
	if (!prop) {
		pr_err("%s:%s can not find marvell,reg-offset\n",
			__func__, np->name);
		return NULL;
	}

	size = proplen / sizeof(u32);

	if ((proplen % sizeof(u32)) || (size <= (index * 2))) {
		pr_debug("%s:%s no index %d register defined\n",
			__func__, np->name, index);
		return NULL;
	}

	reg_index = be32_to_cpup(prop + index * 2);
	reg_offset = be32_to_cpup(prop + index * 2 + 1);
	reg = get_child_reg(np, reg_index, &reg_base);
	if (!reg) {
		pr_err("%s:%s failed to get reg\n",
			__func__, np->name);
		return NULL;
	}

	*reg_phys = reg_base + reg_offset;

	return reg + reg_offset;
}

struct device_node *of_mmp_clk_master_init(struct device_node *from)
{
	struct device_node *parent, *child;
	const struct of_device_id *match;
	of_clk_init_cb_t clk_init_cb;

	parent = from;
	parent = of_find_matching_node_and_match(from, mmp_clk_master_of_id,
						&match);
	if (parent) {
		clk_init_cb = (of_clk_init_cb_t)match->data;
		clk_init_cb(parent);
		for_each_child_of_node(parent, child) {
			match = of_match_node(&__clk_of_table, child);
			if (!match)
				continue;
			clk_init_cb = (of_clk_init_cb_t)match->data;
			clk_init_cb(child);
		}
	}

	return parent;
}
