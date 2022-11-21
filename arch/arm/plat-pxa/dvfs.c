/*
 *  linux/arch/arm/plat-pxa/dvfs.c
 *
 *  based on arch/arm/mach-tegra/dvfs.c
 *	 Copyright (C) 2010 Google, Inc. by Colin Cross <ccross@google.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/list.h>

#include <plat/clock.h>
#include <plat/dvfs.h>
#include <plat/pxa_trace.h>

static LIST_HEAD(dvfs_rail_list);
static DEFINE_MUTEX(dvfs_lock);

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

	if (!rail->reg) {
		if (millivolts == rail->millivolts)
			return 0;
		else
			return -EINVAL;
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

		ret = regulator_set_voltage(rail->reg,
				rail->new_millivolts * 1000,
				rail->max_millivolts * 1000);
		if (ret) {
			pr_err("Failed to set dvfs regulator %s\n",
				rail->reg_id);
			return ret;
		}

		trace_pxa_set_voltage((u32)rail,\
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
int dvfs_rail_update(struct dvfs_rail *rail)
{
	int millivolts = 0;
	struct dvfs *d;
	struct dvfs_relationship *rel;
	int ret = 0;

	/* if regulators are not connected yet, return and handle it later */
	if (!rail->reg)
		return 0;

	/* Find the maximum voltage requested by any clock */
	list_for_each_entry(d, &rail->dvfs, dvfs_node)
		millivolts = max(d->millivolts, millivolts);

	if (millivolts != 0)
		rail->new_millivolts = millivolts;

	/* Check any rails that this rail depends on */
	list_for_each_entry(rel, &rail->relationships_from, from_node)
		rail->new_millivolts = dvfs_solve_relationship(rel);

	if (rail->new_millivolts != rail->millivolts)
		ret = dvfs_rail_set_voltage(rail, rail->new_millivolts);

	return ret;
}
EXPORT_SYMBOL(dvfs_rail_update);

static int dvfs_rail_connect_to_regulator(struct dvfs_rail *rail)
{
	struct regulator *reg = NULL;

	if (rail->reg) {
		pr_warn("dvfs: rail has already been connnected to reg\n");
		return -EINVAL;
	}

	reg = regulator_get(NULL, rail->reg_id);
	if (IS_ERR(reg))
		return -EINVAL;

	rail->reg = reg;

	if (regulator_is_enabled(rail->reg))
		rail->nominal_millivolts = regulator_get_voltage(rail->reg) / 1000;

	return 0;
}

static int
__dvfs_set_rate(struct dvfs *d, unsigned long rate)
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

	ret = dvfs_rail_update(d->dvfs_rail);
	if (ret)
		pr_err("Failed to set regulator %s for clock %s to %d mV\n",
			d->dvfs_rail->reg_id, d->clk_name, d->millivolts);

	return ret;
}

int dvfs_set_rate(struct clk *c, unsigned long rate)
{
	int ret;

	if (!c->dvfs)
		return -EINVAL;

	mutex_lock(&dvfs_lock);
	ret = __dvfs_set_rate(c->dvfs, rate);
	mutex_unlock(&dvfs_lock);

	return ret;
}
EXPORT_SYMBOL(dvfs_set_rate);

/* May only be called during clock init, does not take any locks on clock c. */
int __init enable_dvfs_on_clk(struct clk *c, struct dvfs *d)
{
	if (c->dvfs) {
		pr_err("Error when enabling dvfs on %s for clock %s:\n",
			d->dvfs_rail->reg_id, c->name);
		pr_err("DVFS already enabled for %s\n",
			c->dvfs->dvfs_rail->reg_id);
		return -EINVAL;
	}

	clk_set_cansleep(c);

	c->dvfs = d;

	mutex_lock(&dvfs_lock);
	list_add_tail(&d->dvfs_node, &d->dvfs_rail->dvfs);
	mutex_unlock(&dvfs_lock);

	return 0;
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

