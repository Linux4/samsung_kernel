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

struct mmp_clk_composite_member {
	struct device_node *np;
	struct clk_hw *hw;
	const struct clk_ops *ops;
	int num_parents;
	const char **parent_names;
};

struct mmp_clk_composite_node {
	struct device_node *np;
	struct mmp_clk_composite_member *members[MMP_CLK_COMPOSITE_TYPE_MAX];
	struct list_head node;
};

static LIST_HEAD(composite_list);
static DEFINE_MUTEX(composite_mutex);

int of_mmp_clk_composite_add_member(struct device_node *np, struct clk_hw *hw,
				const struct clk_ops *ops, int type)
{
	struct device_node *parent_np;
	struct mmp_clk_composite_node *node;
	struct mmp_clk_composite_member *member;
	unsigned int found = 0;
	size_t size;
	int i, ret;

	mutex_lock(&composite_mutex);

	parent_np = of_get_next_parent(np);

	list_for_each_entry(node, &composite_list, node) {
		if (node->np == parent_np) {
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_err("%s:%s can not find member %s\n",
			__func__, parent_np->name, np->name);
		ret = -ENOENT;
		goto out;
	}

	if (node->members[type]) {
		pr_err("%s:%s already has type %d,when add member %s\n",
			__func__, parent_np->name, type, np->name);
		ret = -EBUSY;
		goto out;
	}

	member = kzalloc(sizeof(*member), GFP_KERNEL);
	if (!member) {
		pr_err("%s:%s failed to allocate member.\n",
			__func__, parent_np->name);
		ret = -ENOMEM;
		goto out;
	}
	member->np = np;
	member->hw = hw;
	member->ops = ops;

	ret = of_clk_get_parent_count(np);
	if (ret >= 1) {
		member->num_parents = ret;
		size = sizeof(*member->parent_names) * member->num_parents;
		member->parent_names = kzalloc(size, GFP_KERNEL);
		if (!member->parent_names) {
			pr_err("%s:%s failed to allocate parent_names.\n",
				__func__, parent_np->name);
			ret = -ENOMEM;
			goto member_free;
		}
		for (i = 0; i < member->num_parents; i++)
			member->parent_names[i] =
					of_clk_get_parent_name(np, i);
	} else {
		member->num_parents = 0;
		member->parent_names = NULL;
	}

	node->members[type] = member;

	ret = 0;
	goto out;

member_free:
	kfree(member);
out:
	mutex_unlock(&composite_mutex);

	return ret;
}

static void of_mmp_clk_composite_setup(struct device_node *np)
{
	struct mmp_clk_composite_node *node;
	struct mmp_clk_composite_member *mix, *gate;
	struct device_node *child;
	struct clk *clk;
	const struct of_device_id *match;
	of_clk_init_cb_t clk_init_cb;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		pr_err("%s:%s failed to allocate node\n", __func__, np->name);
		return;
	}

	INIT_LIST_HEAD(&node->node);
	node->np = np;

	mutex_lock(&composite_mutex);
	list_add(&node->node, &composite_list);
	mutex_unlock(&composite_mutex);

	for_each_child_of_node(np, child) {
		match = of_match_node(&__clk_of_table, child);
		clk_init_cb = (of_clk_init_cb_t)match->data;
		clk_init_cb(child);
	}

	mix = node->members[MMP_CLK_COMPOSITE_TYPE_MUXMIX];
	gate = node->members[MMP_CLK_COMPOSITE_TYPE_GATE];
	if (!mix || !gate) {
		pr_err("%s:%s failed to parse members.\n", __func__, np->name);
		goto error;
	}

	clk = mmp_clk_register_composite(NULL, np->name,
				mix->parent_names, mix->num_parents,
				mix->hw, mix->ops,
				gate->hw, gate->ops, 0);
	if (!IS_ERR(clk))
		of_clk_add_provider(np, of_clk_src_simple_get, clk);

	return;

error:
	mutex_lock(&composite_mutex);
	list_del(&node->node);
	mutex_unlock(&composite_mutex);
	kfree(node);
}

static void of_mmp_clk_general_composite_setup(struct device_node *np)
{
	struct mmp_clk_composite_node *node;
	struct mmp_clk_composite_member *mux, *gate, *div;
	struct device_node *child;
	struct clk *clk;
	const char **parent_names;
	int num_parents;
	const struct of_device_id *match;
	of_clk_init_cb_t clk_init_cb;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		pr_err("%s:%s failed to allocate node\n", __func__, np->name);
		return;
	}

	INIT_LIST_HEAD(&node->node);
	node->np = np;

	mutex_lock(&composite_mutex);
	list_add(&node->node, &composite_list);
	mutex_unlock(&composite_mutex);

	for_each_child_of_node(np, child) {
		match = of_match_node(&__clk_of_table, child);
		clk_init_cb = (of_clk_init_cb_t)match->data;
		clk_init_cb(child);
	}

	mux = node->members[MMP_CLK_COMPOSITE_TYPE_MUXMIX];
	gate = node->members[MMP_CLK_COMPOSITE_TYPE_GATE];
	div = node->members[MMP_CLK_COMPOSITE_TYPE_DIV];

	if ((!mux && !div) || (!mux && !gate) || (!gate && !div)) {
		pr_err("%s:%s failed to parse members.\n", __func__, np->name);
		goto error;
	}

	if (mux) {
		parent_names = mux->parent_names;
		num_parents = mux->num_parents;
	} else {
		parent_names = div->parent_names;
		num_parents = div->num_parents;
	}

	clk = clk_register_composite(NULL, np->name,
				parent_names, num_parents,
				mux ? mux->hw : NULL, mux ? mux->ops : NULL,
				div ? div->hw : NULL, div ? div->ops : NULL,
				gate ? gate->hw : NULL,
				gate ? gate->ops : NULL, 0);
	if (!IS_ERR(clk))
		of_clk_add_provider(np, of_clk_src_simple_get, clk);

	return;

error:
	mutex_lock(&composite_mutex);
	list_del(&node->node);
	mutex_unlock(&composite_mutex);
	kfree(node);
}

CLK_OF_DECLARE(mmp_clk_composite, "marvell,mmp-clk-composite",
		of_mmp_clk_composite_setup);
CLK_OF_DECLARE(mmp_clk_general_composite, "marvell,mmp-clk-general-composite",
		of_mmp_clk_general_composite_setup);

int of_mmp_clk_is_composite(struct device_node *np)
{
	struct device_node *parent_np;
	struct mmp_clk_composite_node *node;

	parent_np = of_get_next_parent(np);
	mutex_lock(&composite_mutex);
	list_for_each_entry(node, &composite_list, node) {
		if (node->np == parent_np) {
			mutex_unlock(&composite_mutex);
			return 1;
		}
	}
	mutex_unlock(&composite_mutex);

	return 0;
}
