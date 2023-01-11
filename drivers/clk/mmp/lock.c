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

struct mmp_clk_spinlock_node {
	struct device_node *share;
	struct list_head node;
};

struct mmp_clk_spinlock {
	spinlock_t lock;
	struct device_node *owner;
	unsigned int reg_base;
	struct list_head share_list;
	struct list_head node;
};

static LIST_HEAD(lock_list);

static DEFINE_MUTEX(lock_mutex);

static struct mmp_clk_spinlock_node *create_lock_node(struct device_node *np)
{
	struct mmp_clk_spinlock_node *node;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		pr_err("%s:%s failed to allocate spinlock node.\n",
			__func__, np->name);
		return NULL;
	}

	node->share = np;

	return node;
}

static struct mmp_clk_spinlock *create_lock(struct device_node *np,
					    unsigned int reg_base)
{
	struct mmp_clk_spinlock *lock;

	lock = kzalloc(sizeof(*lock), GFP_KERNEL);
	if (!lock) {
		pr_err("%s:%s failed to allocate spinlock.\n",
			__func__, np->name);
		return NULL;
	}

	lock->owner = np;
	lock->reg_base = reg_base;
	INIT_LIST_HEAD(&lock->node);
	INIT_LIST_HEAD(&lock->share_list);
	spin_lock_init(&lock->lock);

	return lock;
}

static struct mmp_clk_spinlock *find_lock_by_np(struct device_node *np)
{
	struct mmp_clk_spinlock *lock;

	list_for_each_entry(lock, &lock_list, node) {
		if (lock->owner == np)
			return lock;
	}

	return NULL;
}

static struct mmp_clk_spinlock *find_lock_by_reg_base(unsigned int reg_base)
{
	struct mmp_clk_spinlock *lock;

	list_for_each_entry(lock, &lock_list, node) {
		if (lock->reg_base == reg_base)
			return lock;
	}

	return NULL;
}

spinlock_t *of_mmp_clk_get_spinlock(struct device_node *np,
					unsigned int reg_base)
{
	struct mmp_clk_spinlock *lock;
	struct mmp_clk_spinlock_node *node;
	struct device_node *owner;

	if (of_property_read_bool(np, "marvell,mmp-clk-spinlock-new")) {

		mutex_lock(&lock_mutex);

		lock = find_lock_by_np(np);
		if (!lock) {
			lock = create_lock(np, reg_base);
			if (lock)
				list_add(&lock->node, &lock_list);
		}

		mutex_unlock(&lock_mutex);

		return &lock->lock;
	}

	if (of_find_property(np, "marvell,mmp-clk-spinlock", NULL)) {

		mutex_lock(&lock_mutex);

		owner = of_parse_phandle(np, "marvell,mmp-clk-spinlock", 0);
		lock = find_lock_by_np(owner);
	} else {

		mutex_lock(&lock_mutex);

		lock = find_lock_by_reg_base(reg_base);
	}

	if (!lock) {
		lock = create_lock(np, reg_base);
		if (lock)
			list_add(&lock->node, &lock_list);
	}

	if (!lock) {
		mutex_unlock(&lock_mutex);
		pr_err("%s:%s failed to get spinlock\n", __func__, np->name);
		return NULL;
	}

	node = create_lock_node(np);
	if (!node) {
		mutex_unlock(&lock_mutex);
		pr_err("%s:%s failed to create spinlock node\n",
			__func__, np->name);
		return NULL;
	}
	node->share = np;
	list_add(&node->node, &lock->share_list);

	mutex_unlock(&lock_mutex);

	return &lock->lock;
}
