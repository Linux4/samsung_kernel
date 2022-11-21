/*
 *  linux/arch/arm/plat-pxa/clock.c
 *
 *  based on arch/arm/mach-tegra/clock.c
 *	 Copyright (C) 2010 Google, Inc. by Colin Cross <ccross@google.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <plat/clock.h>
#include <plat/dvfs.h>

static DEFINE_MUTEX(clock_list_lock);
static LIST_HEAD(clocks);

int enable_voltage_based_dvfm;
EXPORT_SYMBOL(enable_voltage_based_dvfm);

static inline bool clk_cansleep(struct clk *c)
{
	return c->cansleep;
}

#define clk_lock_save(c, flags)						\
	do {								\
		if (clk_cansleep(c)) {					\
			flags = 0;					\
			mutex_lock(&c->mutex);				\
		} else {						\
			spin_lock_irqsave(&c->spinlock, flags);		\
		}							\
	} while (0)

#define clk_unlock_restore(c, flags)					\
	do {								\
		if (clk_cansleep(c))					\
			mutex_unlock(&c->mutex);			\
		else							\
			spin_unlock_irqrestore(&c->spinlock, flags);	\
	} while (0)

#ifdef CONFIG_LOCKDEP

#define clock_set_lockdep_class(c, lock)				\
	do {								\
		lockdep_set_class(lock, &c->lockdep_key);		\
	} while (0)

#else

#define clock_set_lockdep_class(c, lock)	do {} while (0)

#endif

static inline void clk_lock_init(struct clk *c)
{
	mutex_init(&c->mutex);
	clock_set_lockdep_class(c, &c->mutex);
	spin_lock_init(&c->spinlock);
	clock_set_lockdep_class(c, &c->spinlock);
}

static void __clk_set_cansleep(struct clk *c)
{
	struct clk *child;
	BUG_ON(mutex_is_locked(&c->mutex));
	BUG_ON(spin_is_locked(&c->spinlock));

	list_for_each_entry(child, &clocks, node) {
		if (child->parent != c)
			continue;

		WARN(child->ops && child->ops->set_parent,
			"can't make child clock %s of %s "
			"sleepable if it's parent could change",
			child->name, c->name);

		__clk_set_cansleep(child);
	}

	c->cansleep = true;
}

/* Must be called before any clk_get calls */
void clk_set_cansleep(struct clk *c)
{

	mutex_lock(&clock_list_lock);
	__clk_set_cansleep(c);
	mutex_unlock(&clock_list_lock);
}

int clk_reparent(struct clk *c, struct clk *parent)
{
	c->parent = parent;
	return 0;
}

void clk_init(struct clk *c)
{
	clk_lock_init(c);

	if (c->ops && c->ops->init)
		c->ops->init(c);

	if (!c->ops || !c->ops->enable)
		c->refcnt++;

	mutex_lock(&clock_list_lock);
	list_add(&c->node, &clocks);
	mutex_unlock(&clock_list_lock);
}

/* Must be called with clk_lock(c) held */
static unsigned long clk_predict_rate_from_parent(struct clk *c, struct clk *p)
{
	u64 rate;

	rate = clk_get_rate(p);

	if (c->mul != 0 && c->div != 0) {
		rate *= c->mul;
		do_div(rate, c->div);
	}

	return rate;
}

/* Must be called with clk_lock(c) held */
unsigned long clk_get_rate_locked(struct clk *c)
{
	unsigned long rate;

	if (c->ops && c->ops->getrate)
		rate = c->ops->getrate(c);
	else if (c->parent)
		rate = clk_predict_rate_from_parent(c, c->parent);
	else
		rate = c->rate;

	return rate;
}

int clk_enable(struct clk *c)
{
	int ret = 0;
	unsigned long flags;
	int i = 0;
	u32 dependence_count = c->dependence_count;

	clk_lock_save(c, flags);

	if (clk_is_dvfs(c)) {
		ret = dvfs_set_rate(c, clk_get_rate_locked(c));
		if (ret)
			goto out;
	}

	if (c->refcnt == 0) {
		while (dependence_count != 0) {
			dependence_count--;
			ret = clk_enable(c->dependence[i]);
			if (ret) {
				while (i > 0)
					clk_disable(c->dependence[--i]);
				goto out;
			}
			i++;
		}

		if (c->parent) {
			ret = clk_enable(c->parent);
			if (ret)
				goto disable_depend;
		}

		if (c->ops && c->ops->enable) {
			ret = c->ops->enable(c);
			if (ret) {
				if (c->parent)
					clk_disable(c->parent);
				goto disable_depend;
			}
		}
	}
	c->refcnt++;

	clk_unlock_restore(c, flags);
	return 0;

disable_depend:
	dependence_count = c->dependence_count;
	while (dependence_count != 0)
		clk_disable(c->dependence[--dependence_count]);
out:
	clk_unlock_restore(c, flags);
	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *c)
{
	unsigned long flags;
	u32 dependence_count = c->dependence_count;

	clk_lock_save(c, flags);

	if (c->refcnt == 0) {
		clk_unlock_restore(c, flags);
		return;
	}
	if (c->refcnt == 1) {
		if (c->ops && c->ops->disable)
			c->ops->disable(c);

		if (c->parent)
			clk_disable(c->parent);

		while (dependence_count != 0)
			clk_disable(c->dependence[--dependence_count]);
	}
	c->refcnt--;

	if (clk_is_dvfs(c) && c->refcnt == 0)
		dvfs_set_rate(c, 0);

	clk_unlock_restore(c, flags);
}
EXPORT_SYMBOL(clk_disable);

int clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	unsigned long flags, new_rate, old_rate;
	int sr_flag = 0;

	clk_lock_save(c, flags);

	if ((c->refcnt != 0) && !c->dynamic_change) {
		ret = -EBUSY;
		goto out;
	}

	if (!c->ops || !c->ops->setrate) {
		ret = -ENOSYS;
		goto out;
	}

	old_rate = clk_get_rate_locked(c);

	if (c->ops && c->ops->round_rate) {
		new_rate = c->ops->round_rate(c, rate);

		if (new_rate < 0) {
			ret = new_rate;
			goto out;
		}

		rate = new_rate;
	}

	if (clk_is_dvfs(c) && c->refcnt > 0 && rate > old_rate) {
		ret = dvfs_set_rate(c, rate);
		if (ret)
			goto out;
	}

	new_rate = rate;

	if (!enable_voltage_based_dvfm  || !c->is_combined_fc) {
		ret = c->ops->setrate(c, rate);
		if (ret)
			goto out;

		if (c->ops->getrate)
			new_rate = c->ops->getrate(c);
		if (rate != new_rate) {
			rate = new_rate;
			sr_flag = 1;
		}

		c->rate = rate;
	}
	if (clk_is_dvfs(c) && c->refcnt > 0 && (rate < old_rate || sr_flag)) {
		ret = dvfs_set_rate(c, rate);
		if (ret)
			goto out;
	}

out:
	clk_unlock_restore(c, flags);
	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

unsigned long clk_get_rate(struct clk *c)
{
	unsigned long flags;
	unsigned long rate;

	clk_lock_save(c, flags);

	rate = clk_get_rate_locked(c);

	clk_unlock_restore(c, flags);

	return rate;
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_parent(struct clk *c, struct clk *parent)
{
	int ret = 0;
	unsigned long flags, new_rate, old_rate;

	clk_lock_save(c, flags);

	if (!c->ops || !c->ops->set_parent) {
		ret = -ENOSYS;
		goto out;
	}

	new_rate = clk_predict_rate_from_parent(c, parent);
	old_rate = clk_get_rate_locked(c);

	if (c->ops && c->ops->getrate)
		new_rate = c->ops->getrate(c);

	if (clk_is_dvfs(c) && c->refcnt > 0 && new_rate > old_rate) {
		ret = dvfs_set_rate(c, new_rate);
		if (ret)
			goto out;
	}

	ret = c->ops->set_parent(c, parent);
	if (ret)
		goto out;

	if (clk_is_dvfs(c) && c->refcnt > 0 && new_rate < old_rate)
		ret = dvfs_set_rate(c, new_rate);

out:
	clk_unlock_restore(c, flags);
	return ret;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *c)
{
	return c->parent;
}
EXPORT_SYMBOL(clk_get_parent);

long clk_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long flags;
	long ret;

	clk_lock_save(c, flags);

	if (!c->ops || !c->ops->round_rate) {
		ret = -ENOSYS;
		goto out;
	}

	ret = c->ops->round_rate(c, rate);

out:
	clk_unlock_restore(c, flags);
	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

struct clk *get_clock_by_name(const char *name)
{
	struct clk *c;
	struct clk *ret = NULL;
	mutex_lock(&clock_list_lock);
	list_for_each_entry(c, &clocks, node) {
		if (strcmp(c->name, name) == 0) {
			ret = c;
			break;
		}
	}
	mutex_unlock(&clock_list_lock);
	return ret;
}

#if defined(CONFIG_DEBUG_FS)
DEFINE_MUTEX(clk_mutex);
static int clk_enable_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t
clk_enable_read(struct file *filp, char __user *ubuf, size_t cnt,
		loff_t *ppos)
{
	struct clk *clk = filp->private_data;
	char buf[5];
	int ret;

	mutex_lock(&clk_mutex);
	sprintf(buf, "%4x", clk->refcnt);
	mutex_unlock(&clk_mutex);
	buf[4] = '\n';

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, 5);

	return ret;
}

static ssize_t
clk_enable_write(struct file *filp, const char __user *ubuf, size_t cnt,
		 loff_t *ppos)
{
	struct clk *clk = filp->private_data;
	char buf[64];
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val != 0 && val != 1)
		return -EINVAL;

	*ppos += cnt;
	mutex_lock(&clk_mutex);
	if (val)
		clk_enable(clk);
	else
		clk_disable(clk);
	mutex_unlock(&clk_mutex);

	return cnt;
}

static const struct file_operations clk_enable_fops = {
	.open = clk_enable_open,
	.read = clk_enable_read,
	.write = clk_enable_write,
};

static int clk_rate_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t
clk_getrate_read(struct file *filp, char __user *ubuf, size_t cnt,
		loff_t *ppos)
{
	struct clk *clk = filp->private_data;
	char buf[12];
	int ret, len = 0;

	mutex_lock(&clk_mutex);
	len = snprintf(buf, sizeof(buf), "%10lu\n", clk_get_rate(clk));
	mutex_unlock(&clk_mutex);

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, len);
	return ret;
}

static ssize_t
clk_setrate_write(struct file *filp, const char __user *ubuf, size_t cnt,
		 loff_t *ppos)
{
	struct clk *clk = filp->private_data;
	char buf[64];
	unsigned long rate;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, &rate);
	if (ret < 0)
		return ret;

	*ppos += cnt;
	mutex_lock(&clk_mutex);
	clk_set_rate(clk, rate);
	mutex_unlock(&clk_mutex);

	return cnt;
}


static const struct file_operations clk_rate_fops = {
	.open = clk_rate_open,
	.read = clk_getrate_read,
	.write = clk_setrate_write,
};


/*
 *	debugfs support to trace clock tree hierarchy and attributes
 */
static struct dentry *clk_debugfs_root;

static int clk_debugfs_register_one(struct clk *c)
{
	int err;
	struct dentry *d, *child, *child_tmp;
	struct clk *pa = c->parent;
	char s[255];
	char *p = s;

	if (!c->name)
		return -EINVAL;

	p += sprintf(p, "%s", c->name);
	d = debugfs_create_dir(s, pa ? pa->dent : clk_debugfs_root);
	if (!d)
		return -ENOMEM;
	c->dent = d;

	d = debugfs_create_u32("usecount", S_IRUGO, c->dent, (u32 *)&c->refcnt);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	d = debugfs_create_file("rate", 0644, c->dent, c, &clk_rate_fops);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	d = debugfs_create_file("enable", 0644, c->dent, c, &clk_enable_fops);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	return 0;

err_out:
	d = c->dent;
	list_for_each_entry_safe(child, child_tmp, &d->d_subdirs, d_u.d_child)
		debugfs_remove(child);
	debugfs_remove(c->dent);
	return err;
}

static int clk_debugfs_register(struct clk *c)
{
	int err;
	struct clk *pa = c->parent;

	if (pa && !pa->dent) {
		err = clk_debugfs_register(pa);
		if (err)
			return err;
	}

	if (!c->dent) {
		err = clk_debugfs_register_one(c);
		if (err)
			return err;
	}
	return 0;
}

static int __init clk_debugfs_init(void)
{
	struct clk *c;
	struct dentry *d;
	int err;

	d = debugfs_create_dir("clock", NULL);
	if (!d)
		return -ENOMEM;
	clk_debugfs_root = d;

	list_for_each_entry(c, &clocks, node) {
		err = clk_debugfs_register(c);
		if (err)
			goto err_out;
	}
	return 0;
err_out:
	debugfs_remove_recursive(clk_debugfs_root);
	return err;
}
late_initcall(clk_debugfs_init);

#endif /* CONFIG_DEBUG_FS */
