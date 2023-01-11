/*
 *  linux/drivers/clk/mmp/dvfs.c
 *  clock dvfs framwork src file for mmp platforms
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/seq_file.h>
#include <linux/list_sort.h>
#include <linux/debugfs.h>
#include <linux/clk/dvfs.h>
#include <trace/events/pxa.h>

static LIST_HEAD(dvfs_rail_list);
static DEFINE_MUTEX(dvfs_lock);
static DEFINE_SPINLOCK(dvfs_spinlock);

static int dvfs_rail_update(struct dvfs_rail *rail);

static unsigned long dvfs_rail_getlock(struct dvfs_rail *r)
{
	unsigned long flags = 0;
	if (r->update_inatomic)
		spin_lock_irqsave(&dvfs_spinlock, flags);
	else
		mutex_lock(&dvfs_lock);
	return flags;
}

static void dvfs_rail_putlock(struct dvfs_rail *r, unsigned long flags)
{
	if (r->update_inatomic)
		spin_unlock_irqrestore(&dvfs_spinlock, flags);
	else
		mutex_unlock(&dvfs_lock);
}

void dvfs_add_relationships(struct dvfs_relationship *rels, int n)
{
	int i;
	struct dvfs_relationship *rel;

	mutex_lock(&dvfs_lock);

	for (i = 0; i < n; i++) {
		rel = &rels[i];
		list_add_tail(&rel->from_node, &rel->to->relationships_from);
		list_add_tail(&rel->to_node, &rel->from->relationships_to);
	}

	mutex_unlock(&dvfs_lock);
}

void dvfs_remove_relationship(struct dvfs_relationship *rel)
{
	mutex_lock(&dvfs_lock);

	list_del(&rel->from_node);
	list_del(&rel->to_node);

	mutex_unlock(&dvfs_lock);
}

int dvfs_init_rails(struct dvfs_rail *rails[], int n)
{
	int i;

	mutex_lock(&dvfs_lock);

	for (i = 0; i < n; i++) {
		INIT_LIST_HEAD(&rails[i]->dvfs);
		INIT_LIST_HEAD(&rails[i]->relationships_from);
		INIT_LIST_HEAD(&rails[i]->relationships_to);
		rails[i]->millivolts = rails[i]->nominal_millivolts;
		rails[i]->new_millivolts = rails[i]->nominal_millivolts;
		if (!rails[i]->step)
			rails[i]->step = rails[i]->max_millivolts;

		list_add_tail(&rails[i]->node, &dvfs_rail_list);
	}

	mutex_unlock(&dvfs_lock);

	return 0;
};

static int dvfs_solve_relationship(struct dvfs_relationship *rel)
{
	return rel->solve(rel->from, rel->to);
}

/*
 * Sets the voltage on a dvfs rail to a specific value, and updates any
 * rails that depend on this rail.
 */
static int dvfs_rail_set_voltage(struct dvfs_rail *rail, int millivolts)
{
	int ret = 0;
	struct dvfs_relationship *rel;
	int step = (millivolts > rail->millivolts) ? rail->step : -rail->step;
	int i;
	int steps;

	if (!rail->reg && !rail->set_volt) {
		if (millivolts == rail->millivolts)
			return 0;
		else
			return -EINVAL;
	}
	/* if there is regulator call back, DVFS should call regulator
	 * interface to change voltage. in this case, 0 as milliolts value
	 * should be meanliness.
	 * if other call back such as set_volt involved, DVFS should pass
	 * millivolts value to it, whatever its value is.
	 */
	if (rail->reg && !millivolts) {
		pr_warn("dvfs: voltage setting ignored because of 0V value.\n");
		return 0;
	}

	steps = DIV_ROUND_UP(abs(millivolts - rail->millivolts), rail->step);

	for (i = 0; i < steps; i++) {
		if (abs(millivolts - rail->millivolts) > rail->step)
			rail->new_millivolts = rail->millivolts + step;
		else
			rail->new_millivolts = millivolts;

		/*
		 * Before changing the voltage, tell each rail that depends
		 * on this rail that the voltage will change.
		 * This rail will be the "from" rail in the relationship,
		 * the rail that depends on this rail will be the "to" rail.
		 * from->millivolts will be the old voltage
		 * from->new_millivolts will be the new voltage
		 */
		list_for_each_entry(rel, &rail->relationships_to, to_node) {
			ret = dvfs_rail_update(rel->to);
			if (ret)
				return ret;
		}

		if (rail->reg)
			ret = regulator_set_voltage(rail->reg,
				rail->new_millivolts * 1000,
				rail->max_millivolts * 1000);
		else
			ret = rail->set_volt(rail, rail->new_millivolts);
		if (ret) {
			pr_err("Failed to set dvfs regulator %s\n",
				rail->reg_id);
			return ret;
		}

		trace_pxa_set_voltage(rail->reg_id,
			rail->millivolts, rail->new_millivolts);

		rail->millivolts = rail->new_millivolts;

		/*
		 * After changing the voltage, tell each rail that depends
		 * on this rail that the voltage has changed.
		 * from->millivolts and from->new_millivolts will be the
		 * new voltage
		 */
		list_for_each_entry(rel, &rail->relationships_to, to_node) {
			ret = dvfs_rail_update(rel->to);
			if (ret)
				return ret;
		}
	}

	if (unlikely(rail->millivolts != millivolts)) {
		pr_err("%s: rail didn't reach target %d in %d steps (%d)\n",
			__func__, millivolts, steps, rail->millivolts);
		return -EINVAL;
	}

	return ret;
}

/*
 * Determine the minimum valid voltage for a rail, taking into account
 * the dvfs clocks and any rails that this rail depends on.  Calls
 * dvfs_rail_set_voltage with the new voltage, which will call
 * dvfs_rail_update on any rails that depend on this rail.
 */
static int dvfs_rail_update(struct dvfs_rail *rail)
{
	int millivolts = 0;
	struct dvfs *d;
	struct dvfs_relationship *rel;
	int ret = 0;

	/* if regulators are not connected yet, return and handle it later */
	if (!rail->reg && !rail->set_volt) {
		pr_err("%s lack of regulator info or set_volt callback!\n",
					__func__);
		return 0;
	}

	/* Find the maximum voltage requested by any clock */
	list_for_each_entry(d, &rail->dvfs, dvfs_node)
		millivolts = max(d->millivolts, millivolts);
	rail->new_millivolts = millivolts;

	/* Check any rails that this rail depends on */
	list_for_each_entry(rel, &rail->relationships_from, from_node)
		rail->new_millivolts = dvfs_solve_relationship(rel);

	if (rail->new_millivolts != rail->millivolts)
		ret = dvfs_rail_set_voltage(rail, rail->new_millivolts);

	return ret;
}

static int dvfs_rail_connect_to_regulator(struct dvfs_rail *rail)
{
	struct regulator *reg = NULL;

	/* allow rail to use its private set_volt func instead of regulator */
	if (rail->set_volt)
		return 0;

	if (rail->reg) {
		pr_warn("dvfs: rail has already been connnected to reg\n");
		return -EINVAL;
	}

	reg = regulator_get(NULL, rail->reg_id);
	if (IS_ERR(reg))
		return -EINVAL;

	rail->reg = reg;
	return 0;
}

static int dvfs_set_rate(struct dvfs *d, unsigned long rate)
{
	int i = 0;
	int ret;

	if (d->vol_freq_table == NULL)
		return -ENODEV;

	if (rate > d->vol_freq_table[d->num_freqs - 1].freq) {
		pr_warn("dvfs: rate %lu too high for dvfs on %s\n", rate,
			d->clk_name);
		return -EINVAL;
	}

	if (rate == 0) {
		d->millivolts = 0;
	} else {
		while (i < d->num_freqs && rate > d->vol_freq_table[i].freq)
			i++;

		d->millivolts = d->vol_freq_table[i].millivolts;
	}

	d->cur_rate = rate;
	ret = dvfs_rail_update(d->dvfs_rail);
	if (ret)
		pr_err("Failed to set regulator %s for clock %s to %d mV\n",
			d->dvfs_rail->reg_id, d->clk_name, d->millivolts);

	return ret;
}

#define to_dvfs(_nb) container_of(_nb, struct dvfs, nb)

static int dvfs_clk_notifier_handler(struct notifier_block *nb,
		unsigned long msg, void *data)
{
	struct clk_notifier_data *cnd = data;
	struct dvfs *d = to_dvfs(nb);
	int ret = 0;
	unsigned long lockflags;

	/*
	 * clock rate change/enable/disable notifier used to adjust voltage
	 * 1) for clock rate change(rate A->B), if components is not enabled,
	 *  that is: old > 0 && new > 0 && enable_count == 0, ignore dvfs req
	 * 2) after clock disable(rate A->0), dvfs req has to be handled
	 */
	if ((cnd->clk->enable_count == 0) && cnd->old_rate && cnd->new_rate)
		return NOTIFY_OK;

	/* scaling up?  scale voltage before frequency */
	if ((msg & PRE_RATE_CHANGE) && (cnd->new_rate > cnd->old_rate)) {
		lockflags = dvfs_rail_getlock(d->dvfs_rail);
		ret = dvfs_set_rate(d, cnd->new_rate);
		dvfs_rail_putlock(d->dvfs_rail, lockflags);
	} else if ((msg & POST_RATE_CHANGE) &&
				(cnd->new_rate < cnd->old_rate)) {
		/* scaling down?  scale voltage after frequency */
		lockflags = dvfs_rail_getlock(d->dvfs_rail);
		ret = dvfs_set_rate(d, cnd->new_rate);
		dvfs_rail_putlock(d->dvfs_rail, lockflags);
	}

	if (ret)
		return notifier_from_errno(ret);

	return NOTIFY_OK;
}

/* May only be called during clock init, does not take any locks on clock c. */
int __init enable_dvfs_on_clk(struct clk *c, struct dvfs *d)
{
	int ret = 0;

	d->nb.notifier_call = dvfs_clk_notifier_handler;
	ret = clk_notifier_register(c, &d->nb);
	if (ret)
		goto out;

	mutex_lock(&dvfs_lock);
	list_add_tail(&d->dvfs_node, &d->dvfs_rail->dvfs);
	mutex_unlock(&dvfs_lock);
out:
	return ret;
}

/*
 * Iterate through all the dvfs regulators, finding the regulator exported
 * by the regulator api for each one.
 * Must be called after all the regulator api initialized (subsys_initcall).
 * Must be called before all the cpufreq/devfreq framework initialized
 * (module_initcall).
 */
int __init dvfs_late_init(void)
{
	struct dvfs_rail *rail;

	mutex_lock(&dvfs_lock);

	list_for_each_entry(rail, &dvfs_rail_list, node)
		dvfs_rail_connect_to_regulator(rail);

	list_for_each_entry(rail, &dvfs_rail_list, node)
		dvfs_rail_update(rail);

	mutex_unlock(&dvfs_lock);

	return 0;
}
fs_initcall(dvfs_late_init);

#ifdef CONFIG_DEBUG_FS
static int dvfs_tree_sort_cmp(void *p, struct list_head *a, struct list_head *b)
{
	struct dvfs *da = list_entry(a, struct dvfs, dvfs_node);
	struct dvfs *db = list_entry(b, struct dvfs, dvfs_node);
	int ret;

	ret = strcmp(da->dvfs_rail->reg_id, db->dvfs_rail->reg_id);
	if (ret != 0)
		return ret;

	if (da->millivolts < db->millivolts)
		return 1;
	if (da->millivolts > db->millivolts)
		return -1;

	return strcmp(da->clk_name, db->clk_name);
}

static int dvfs_tree_show(struct seq_file *s, void *data)
{
	struct dvfs *d;
	struct dvfs_rail *rail;
	struct dvfs_relationship *rel;

	seq_puts(s, "   clock      rate       mV\n");
	seq_puts(s, "--------------------------------\n");

	mutex_lock(&dvfs_lock);

	list_for_each_entry(rail, &dvfs_rail_list, node) {
		seq_printf(s, "%s %d mV:\n", rail->reg_id, rail->millivolts);
		list_for_each_entry(rel, &rail->relationships_from, from_node) {
			seq_printf(s, "   %-10s %-7d mV %-4d mV\n",
				rel->from->reg_id,
				rel->from->millivolts,
				dvfs_solve_relationship(rel));
		}

		list_sort(NULL, &rail->dvfs, dvfs_tree_sort_cmp);

		list_for_each_entry(d, &rail->dvfs, dvfs_node) {
			seq_printf(s, "   %-10s %-10lu %-4d mV\n", d->clk_name,
				d->cur_rate, d->millivolts);
		}
	}

	mutex_unlock(&dvfs_lock);

	return 0;
}

static int dvfs_tree_open(struct inode *inode, struct file *file)
{
	return single_open(file, dvfs_tree_show, inode->i_private);
}

static const struct file_operations dvfs_tree_fops = {
	.open		= dvfs_tree_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int __init dvfs_debugfs_init(struct dentry *clk_debugfs_root)
{
	struct dentry *d;

	d = debugfs_create_file("dvfs", S_IRUGO, clk_debugfs_root, NULL,
		&dvfs_tree_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}
#endif
