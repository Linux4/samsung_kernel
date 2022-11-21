/*
 *  linux/arch/arm/plat-pxa/include/pxa/clock.h
 *
 *  based on arch/arm/mach-tegra/clock.h
 *	 Copyright (C) 2010 Google, Inc. by Colin Cross <ccross@google.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __PLAT_PXA_CLOCK_H
#define __PLAT_PXA_CLOCK_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/clkdev.h>

struct clk_mux_sel {
	struct clk *input;
	u32 value;
};

enum reg_type {
	SOURCE = 0,
	DIV,
	MUL,
	REG_TYPE_NUM,
};

enum reg_control {
	STATUS = 0,
	CONTROL,
	REG_CONTROL_NUM,
};

struct clkops {
	void (*init) (struct clk *);
	int (*enable) (struct clk *);
	void (*disable) (struct clk *);
	unsigned long (*getrate) (struct clk *);
	int (*setrate) (struct clk *, unsigned long);
	int (*set_parent) (struct clk *, struct clk *);
	long (*round_rate) (struct clk *, unsigned long);
};

struct clk {
	const struct clkops *ops;

	/* node for master clocks list */
	struct list_head node;
	struct clk_lookup lookup;
	struct dvfs *dvfs;

	bool cansleep;
	bool dynamic_change;
	const char *name;

	u32 refcnt;
	struct clk *parent;
	struct clk **dependence;
	u32 dependence_count;
	u32 div;
	u32 mul;

	const struct clk_mux_sel *inputs;

	struct {
		void __iomem *reg;
		u32 reg_shift;
		u32 reg_mask;
	} reg_data[REG_TYPE_NUM][REG_CONTROL_NUM];

	struct list_head shared_bus_list;

	struct mutex mutex;
	spinlock_t spinlock;

	unsigned long rate;

#ifdef CONFIG_LOCKDEP
	struct lock_class_key lockdep_key;
#endif
	/*
	 * This is for the old MMP clock implementation
	 * will remove them later
	 */
	/* For APBC clocks */
	void __iomem *clk_rst;
	int fnclksel;
	/* value for clock enable (APMU) */
	uint32_t enable_val;
#ifdef CONFIG_DEBUG_FS
	struct dentry		*dent;	/* For visible tree hierarchy */
#endif
	bool is_combined_fc;	/* whether combine fc with other clocks */
};

void clk_init(struct clk *clk);
int clk_reparent(struct clk *c, struct clk *parent);
void clk_set_cansleep(struct clk *c);
unsigned long clk_get_rate_locked(struct clk *c);
struct clk *get_clock_by_name(const char *name);
extern int enable_voltage_based_dvfm;

static inline bool clk_is_dvfs(struct clk *c)
{
	return (c->dvfs != NULL);
}

#endif
