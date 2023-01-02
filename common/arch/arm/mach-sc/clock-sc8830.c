/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Fixes:
 *		0.2
 *		ARM: sc: add parent pll clock alias
 *		and enable mpll fedback divider config
 *		Change-Id: Ic2e5d78a058d3b017ea17b82e3a920c3efefcf49
 *		0.1
 *		shark dcam: update dcam and mm clocks
 *		Change-Id: Id85d58178aca40fdf13b996853711e92e1171801
 *
 * To Fix:
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>

#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/hardware.h>

#include "clock.h"
#include "mach/__clock_tree.h"

int sci_mm_enable(struct clk *c, int enable, unsigned long *pflags);

const u32 __clkinit0 __clkinit_begin = 0xeeeebbbb;
const u32 __clkinit2 __clkinit_end = 0xddddeeee;

DEFINE_SPINLOCK(clocks_lock);

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	if (IS_ERR_OR_NULL(clk))
		return -EINVAL;

	clk_enable(clk->parent);

	spin_lock_irqsave(&clocks_lock, flags);
	if ((clk->usage++) == 0 && clk->enable)
		(clk->enable) (clk, 1, &flags);
	spin_unlock_irqrestore(&clocks_lock, flags);
	debug0("clk %p, usage %d\n", clk, clk->usage);
	return 0;
}

EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;
	if (IS_ERR_OR_NULL(clk))
		return;

	spin_lock_irqsave(&clocks_lock, flags);
	if ((--clk->usage) == 0 && clk->enable)
		(clk->enable) (clk, 0, &flags);
	if (WARN(clk->usage < 0, "warning: clock (%s) usage (%d)\n",
		 clk->regs->name, clk->usage)) {
		clk->usage = 0;	/* force reset clock refcnt */
		spin_unlock_irqrestore(&clocks_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&clocks_lock, flags);
	debug0("clk %p, usage %d\n", clk, clk->usage);
	clk_disable(clk->parent);
}

EXPORT_SYMBOL(clk_disable);

/**
 * clk_force_disable - force disable clock output
 * @clk: clock source
 *
 * Forcibly disable the clock output.
 * NOTE: this *will* disable the clock output even if other consumer
 * devices have it enabled. This should be used for situations when device
 * suspend or damage will likely occur if the devices is not disabled.
 */
void clk_force_disable(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return;

	debug2("clk %p, usage %d\n", clk, clk->usage);
	while (clk->usage > 0) {
		clk_disable(clk);
	}
}

EXPORT_SYMBOL(clk_force_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	debug0("clk %p, rate %lu\n", clk, IS_ERR_OR_NULL(clk) ? -1 : clk->rate);
	if (IS_ERR_OR_NULL(clk))
		return 0;

/* FIXME:
 * auto refill clk->rate after new parent or division
 * auto update all child clock rate
	if (clk->rate != 0)
		return clk->rate;
*/

	if (clk->ops != NULL && clk->ops->get_rate != NULL)
		return (clk->ops->get_rate) (clk);

	if (clk->parent != NULL)
		return clk_get_rate(clk->parent);

	return clk->rate;
}

EXPORT_SYMBOL(clk_get_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (!IS_ERR_OR_NULL(clk) && clk->ops && clk->ops->round_rate)
		return (clk->ops->round_rate) (clk, rate);

	return rate;
}

EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;
	unsigned long flags;
	debug0("clk %p, rate %lu\n", clk, rate);
	if (IS_ERR_OR_NULL(clk) || rate == 0)
		return -EINVAL;

	/* We do not default just do a clk->rate = rate as
	 * the clock may have been made this way by choice.
	 */

	//WARN_ON(clk->ops == NULL);
	//WARN_ON(clk->ops && clk->ops->set_rate == NULL);

	if (clk->ops == NULL || clk->ops->set_rate == NULL)
		return -EINVAL;

	spin_lock_irqsave(&clocks_lock, flags);
	ret = (clk->ops->set_rate) (clk, rate);
	spin_unlock_irqrestore(&clocks_lock, flags);
	return ret;
}

EXPORT_SYMBOL(clk_set_rate);

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}

EXPORT_SYMBOL(clk_get_parent);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = -EACCES;
	unsigned long flags;
#if defined(CONFIG_DEBUG_FS)
	struct clk *old_parent = clk_get_parent(clk);
#endif
	debug0("clk %p, parent %p <<< %p\n", clk, parent, clk_get_parent(clk));
	if (IS_ERR_OR_NULL(clk) || IS_ERR(parent))
		return -EINVAL;

	spin_lock_irqsave(&clocks_lock, flags);
	if (clk->ops && clk->ops->set_parent)
		ret = (clk->ops->set_parent) (clk, parent);
	spin_unlock_irqrestore(&clocks_lock, flags);

#if defined(CONFIG_DEBUG_FS)
	/* FIXME: call debugfs_rename() out of spin lock,
	 * maybe not match with the real parent-child relationship
	 * in some extreme scenes.
	 */
	if (0 == ret && old_parent && old_parent->dent && clk->dent
	    && parent && parent->dent) {
		debug0("directory dentry move %s to %s\n",
		       old_parent->regs->name, parent->regs->name);
		debugfs_rename(old_parent->dent, clk->dent,
			       parent->dent, clk->regs->name);
	}
#endif
	return ret;
}

EXPORT_SYMBOL(clk_set_parent);

static int sci_clk_enable(struct clk *c, int enable, unsigned long *pflags)
{
	debug("clk %p (%s) %s %08x[%d]\n", c, c->regs->name,
	      enable ? "enb" : "dis", c->regs->enb.reg,
	      __ffs(c->regs->enb.mask));

	BUG_ON(!c->regs->enb.reg);
	if (c->regs->enb.reg & 1)
		enable = !enable;

	if (!c->regs->enb.mask) {	/* enable matrix clock */
		if (pflags)
			spin_unlock_irqrestore(&clocks_lock, *pflags);
		if (enable)
			clk_enable((struct clk *)c->regs->enb.reg);
		else
			clk_disable((struct clk *)c->regs->enb.reg);
		if (pflags)
			spin_lock_irqsave(&clocks_lock, *pflags);
	} else {
		if (enable)
			sci_glb_set(c->regs->enb.reg & ~1, c->regs->enb.mask);
		else
			sci_glb_clr(c->regs->enb.reg & ~1, c->regs->enb.mask);
	}
	return 0;
}

static int sci_clk_is_enable(struct clk *c)
{
	int enable;

	debug0("clk %p (%s) enb %08x\n", c, c->regs->name, c->regs->enb.reg);

	BUG_ON(!c->regs->enb.reg);
	if (!c->regs->enb.mask) {	/* check matrix clock */
		enable = ! !sci_clk_is_enable((struct clk *)c->regs->enb.reg);
	} else {
		enable =
		    ! !sci_glb_read(c->regs->enb.reg & ~1, c->regs->enb.mask);
	}

	if (c->regs->enb.reg & 1)
		enable = !enable;
	return enable;
}

static int sci_clk_set_rate(struct clk *c, unsigned long rate)
{
	int div;
	u32 div_shift;
	debug2("clk %p (%s) set rate %lu\n", c, c->regs->name, rate);
	rate = clk_round_rate(c, rate);
	div = clk_get_rate(c->parent) / rate - 1;	//FIXME:
	if (div < 0)
		return -EINVAL;
	div_shift = __ffs(c->regs->div.mask);
	debug("clk %p (%s) pll div reg %08x, val %08x mask %08x\n", c,
	      c->regs->name, c->regs->div.reg, div << div_shift,
	      c->regs->div.mask);

	if (c->regs->div.reg)
		sci_glb_write(c->regs->div.reg, div << div_shift,
			      c->regs->div.mask);

	return 0;
}

static unsigned long sci_clk_get_rate(struct clk *c)
{
	u32 div = 0, div_shift;
	unsigned long rate;
	div_shift = __ffs(c->regs->div.mask);
	debug0("clk %p (%s) div reg %08x, shift %u msk %08x\n", c,
	       c->regs->name, c->regs->div.reg, div_shift, c->regs->div.mask);
	rate = clk_get_rate(c->parent);

	if (c->regs->div.reg)
		div = sci_glb_read(c->regs->div.reg,
				   c->regs->div.mask) >> div_shift;
	debug0("clk %p (%s) parent rate %lu, div %u\n", c, c->regs->name, rate,
	       div + 1);
	rate = rate / (div + 1);	//FIXME:
	debug0("clk %p (%s) get real rate %lu\n", c, c->regs->name, rate);
	return rate;
}

static unsigned long sci_pll_get_refin_rate(struct clk *c)
{
	const unsigned long refin[4] = { 2000000, 4000000, 13000000, 26000000 };
	u32 i, msk = BITS_MPLL_REFIN(-1);
	i = sci_glb_read(c->regs->div.reg, msk) >> __ffs(msk);
	debug0("pll %p (%s) refin %d\n", c, c->regs->name, i);
	BUG_ON(i >= ARRAY_SIZE(refin));
	return refin[i];
}

static unsigned long sci_pll_get_rate(struct clk *c)
{
	u32 mn = 1, mn_shift;
	unsigned long rate;
	mn_shift = __ffs(c->regs->div.mask);
	debug0("pll %p (%s) mn reg %08x, shift %u msk %08x\n", c, c->regs->name,
	       c->regs->div.reg, mn_shift, c->regs->div.mask);

	/* get parent rate */
	rate = clk_get_rate(c->parent);

	if (0 == c->regs->div.reg) {
		if (c->rate)
			rate = c->rate;	/* fixed rate */
	} else if (c->regs->div.reg < MAX_DIV) {
		mn = c->regs->div.reg;
		if (mn)
			rate = rate / mn;
	} else {
#ifdef CONFIG_ARCH_SCX30G
		u32 k;
		rate = sci_pll_get_refin_rate(c);
		mn = sci_glb_read(c->regs->div.reg, 0xfffff03f);
		k = (mn >> 12);
		mn = mn & 0x3f;
		rate = 26 * (mn) * 1000000 + DIV_ROUND_CLOSEST(26 * k * 100, 1048576) * 10000;
#else
		rate = sci_pll_get_refin_rate(c);
		mn = sci_glb_read(c->regs->div.reg,
				  c->regs->div.mask) >> mn_shift;
		if (mn)
			rate = rate * mn;
#endif
	}
	debug0("pll %p (%s) get real rate %lu\n", c, c->regs->name, rate);
	return rate;
}

static int __pll_enable_time(struct clk *c, unsigned long old_rate)
{
	/* FIXME: for mpll, each step (100MHz) takes 50us */
	u32 rate = c->ops->get_rate(c) / 1000000;
	int dly = abs(rate - old_rate) * 50 / 100;
	WARN_ON(dly > 1000);
	udelay(dly);
	return 0;
}

static int sci_pll_set_rate(struct clk *c, unsigned long rate)
{
	u32 mn = 1, mn_shift;
	mn_shift = __ffs(c->regs->div.mask);
	debug("pll %p (%s) rate %lu, mn reg %08x, shift %u msk %08x\n", c,
	      c->regs->name, rate, c->regs->div.reg, mn_shift,
	      c->regs->div.mask);

	if (0 == c->regs->div.reg || c->regs->div.reg < MAX_DIV) {
/*
		WARN(1, "warning: clock (%s) not support set\n", c->regs->name);
 */
	} else {
		u32 old_rate = c->ops->get_rate(c) / 1000000;
#ifdef CONFIG_ARCH_SCX30G
		u32 k;
		mn = (rate / 1000000) / 26;
		k = DIV_ROUND_CLOSEST(((rate / 10000) - 26 * mn * 100) * 1048576, 26 * 100);
		sci_glb_write(c->regs->div.reg, (k << 12)|(mn), 0xfffff03f);
#else
		mn = rate / sci_pll_get_refin_rate(c);
		sci_glb_write(c->regs->div.reg, mn << mn_shift,
			      c->regs->div.mask);
#endif
		__pll_enable_time(c, old_rate);
	}

	debug2("pll %p (%s) set rate %lu\n", c, c->regs->name, rate);
	return 0;
}

static unsigned long sci_clk_round_rate(struct clk *c, unsigned long rate)
{
	debug0("clk %p (%s) round rate %lu\n", c, c->regs->name, rate);
	return rate;
}

static int sci_clk_set_parent(struct clk *c, struct clk *parent)
{
	int i;
	debug0("clk %p (%s) parent %p (%s)\n", c, c->regs->name,
	       parent, parent ? parent->regs->name : 0);

	for (i = 0; i < c->regs->nr_sources; i++) {
		if (c->regs->sources[i] == parent) {
			u32 sel_shift = __ffs(c->regs->sel.mask);
			debug0("clk sel reg %08x, val %08x, msk %08x\n",
			       c->regs->sel.reg, i << sel_shift,
			       c->regs->sel.mask);
			if (c->regs->sel.reg)
				sci_glb_write(c->regs->sel.reg, i << sel_shift,
					      c->regs->sel.mask);
			c->parent = parent;
			return 0;
		}
	}

	WARN(1, "warning: clock (%s) not support parent (%s)\n",
	     c->regs->name, parent ? parent->regs->name : 0);
	return -EINVAL;
}

static int sci_clk_get_parent(struct clk *c)
{
	int i = 0;
	u32 sel_shift = __ffs(c->regs->sel.mask);
	debug0("pll sel reg %08x, val %08x, msk %08x\n",
	       c->regs->sel.reg, i << sel_shift, c->regs->sel.mask);
	if (c->regs->sel.reg) {
		i = sci_glb_read(c->regs->sel.reg,
				 c->regs->sel.mask) >> sel_shift;
	}
	return i;
}

static struct clk_ops generic_clk_ops = {
	.set_rate = sci_clk_set_rate,
	.get_rate = sci_clk_get_rate,
	.round_rate = sci_clk_round_rate,
	.set_parent = sci_clk_set_parent,
};

static struct clk_ops generic_pll_ops = {
	.set_rate = sci_pll_set_rate,
	.get_rate = sci_pll_get_rate,
	.round_rate = 0,
	.set_parent = sci_clk_set_parent,
};

/* debugfs support to trace clock tree hierarchy and attributes */
#if defined(CONFIG_DEBUG_FS)
static int debugfs_usecount_get(void *data, u64 * val)
{
	struct clk *c = data;
	int is_enable = ! !(c->enable == NULL || sci_clk_is_enable(c));
	*val = (is_enable) ? c->usage : -c->usage;
	return 0;
}

static int debugfs_usecount_set(void *data, u64 val)
{
	struct clk *c = data;
	(val) ? clk_enable(c) : clk_disable(c);
	return 0;
}

static int debugfs_rate_get(void *data, u64 * val)
{
	struct clk *c = data;
	*val = clk_get_rate(c);
	return 0;
}

static int debugfs_rate_set(void *data, u64 val)
{
	struct clk *c = data;
	clk_set_rate(c, val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_usecount,
			debugfs_usecount_get, debugfs_usecount_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(fops_rate,
			debugfs_rate_get, debugfs_rate_set, "%llu\n");

static struct dentry *clk_debugfs_root;
static int __init clk_debugfs_register(struct clk *c)
{
	char name[NAME_MAX], *p = name;
	p += sprintf(p, "%s", c->regs->name);

	if (IS_ERR_OR_NULL((c->dent =
			    debugfs_create_dir(name,
					       c->parent ? c->parent->dent :
					       clk_debugfs_root))))
		goto err_exit;
	if (IS_ERR_OR_NULL(debugfs_create_file
			   ("usecount", S_IRUGO | S_IWUSR, c->dent, (u32 *) c,
			    &fops_usecount)))
		goto err_exit;
	if (IS_ERR_OR_NULL(debugfs_create_file
			   ("rate", S_IRUGO | S_IWUSR, c->dent, (u32 *) c,
			    &fops_rate)))
		goto err_exit;
	return 0;
err_exit:
	if (c->dent)
		debugfs_remove_recursive(c->dent);
	return -ENOMEM;
}
#endif

static __init int __clk_is_dummy_pll(struct clk *c)
{
	return (c->regs->enb.reg & 1) || strstr(c->regs->name, "pll");
}

/*
static __init int __clk_is_dummy_internal(struct clk *c)
{
	int i = strlen(c->regs->name);
	return c->regs->name[i - 2] == '_' && c->regs->name[i - 1] == 'i';
}
*/
static __init int __clk_add_alias(struct clk *c)
{
	int i = strlen(c->regs->name);
	const char *p = &c->regs->name[i - 3];
	if (isdigit(p[0]) && p[1] == 'm' && isdigit(p[2])) {
		char alias[16];
		struct clk_lookup *l;
		strcpy(alias, c->regs->name);
		strcat(alias, "00k");
		l = clkdev_alloc(c, alias, 0);
		BUG_ON(!l);
		clkdev_add(l);
		debug("%s <--- %s\n", c->regs->name, alias);
	}
	return 0;
}

int __init sci_clk_register(struct clk_lookup *cl)
{
	struct clk *c = cl->clk;

	if (c->ops == NULL) {
		c->ops = &generic_clk_ops;
		if (c->rate && !c->regs->nr_sources)	/* fixed OSC */
			c->ops = NULL;
		else if ((c->regs->div.reg >= 0 && c->regs->div.reg < MAX_DIV)
			 || c->rate || strstr(c->regs->name, "pll")) {
			c->ops = &generic_pll_ops;
		}
	}

	debug0
	    ("clk %p (%s) rate %lu ops %p enb %08x sel %08x div %08x nr_sources %u\n",
	     c, c->regs->name, c->rate, c->ops, c->regs->enb.reg,
	     c->regs->sel.reg, c->regs->div.reg, c->regs->nr_sources);

	if (c->regs->nr_sources) {	/* FIXME: dummy update clock parent and rate */
		clk_set_parent(c, c->regs->sources[sci_clk_get_parent(c)]);
#if defined(CONFIG_DEBUG_FS)
		clk_set_rate(c, clk_get_rate(c));
#endif
	}
#if defined(CONFIG_ARCH_SCX35)
	if (strcmp(c->regs->name, "clk_mm_i") == 0) {
		c->enable = sci_mm_enable;
	}
#endif

	if (c->enable == NULL && c->regs->enb.reg) {
		c->enable = sci_clk_enable;
		/* FIXME: dummy update some pll clocks usage */
		if (__clk_is_dummy_pll(c) && sci_clk_is_enable(c)) {
			clk_enable(c);
		}
	}

	clkdev_add(cl);
	__clk_add_alias(c);

#if defined(CONFIG_DEBUG_FS)
	clk_debugfs_register(c);
#endif
	return 0;
}

static int __init sci_clock_dump(void)
{
	clk_enable(&clk_mm_i);
#if 0
	struct clk_lookup *cl = (struct clk_lookup *)(&__clkinit_begin + 1);
	clk_enable(&clk_gpu_i);
	while (cl < (struct clk_lookup *)&__clkinit_end) {
		struct clk *c = cl->clk;
		struct clk *p = clk_get_parent(c);
		if (!__clk_is_dummy_internal(c))
			printk
			    ("@@@clock[%s] is %sactive, usage %d, rate %lu, parent[%s]\n",
			     c->regs->name,
			     (c->enable == NULL
			      || sci_clk_is_enable(c)) ? "" : "in", c->usage,
			     clk_get_rate(c), p ? p->regs->name : "none");
		cl++;
	}
	debug("okay\n");
	clk_disable(&clk_gpu_i);
#endif
	clk_disable(&clk_mm_i);
	return 0;
}

static int
__clk_cpufreq_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
#if 0				/*!defined(CONFIG_ARCH_SCX35) */
	struct cpufreq_freqs *freq = data;
	printk("%s (%u) dump cpu freq (%u %u %u %u)\n",
	       __func__, (unsigned int)val,
	       freq->cpu, freq->old, freq->new, (unsigned int)freq->flags);
#endif
	return 0;
}

static struct notifier_block __clk_cpufreq_notifier_block = {
	.notifier_call = __clk_cpufreq_notifier
};

int __init sci_clock_init(void)
{
	__raw_writel(__raw_readl((void *)REG_PMU_APB_PD_MM_TOP_CFG)
		     & ~(BIT_PD_MM_TOP_FORCE_SHUTDOWN),
		     (void *)REG_PMU_APB_PD_MM_TOP_CFG);

	__raw_writel(__raw_readl((void *)REG_PMU_APB_PD_GPU_TOP_CFG)
		     & ~(BIT_PD_GPU_TOP_FORCE_SHUTDOWN),
		     (void *)REG_PMU_APB_PD_GPU_TOP_CFG);

	__raw_writel(__raw_readl((void *)REG_AON_APB_APB_EB0) | BIT_MM_EB |
		     BIT_GPU_EB, (void *)REG_AON_APB_APB_EB0);

	__raw_writel(__raw_readl((void *)REG_MM_AHB_AHB_EB) | BIT_MM_CKG_EB,
		     (void *)REG_MM_AHB_AHB_EB);

	__raw_writel(__raw_readl((void *)REG_MM_AHB_GEN_CKG_CFG)
		     | BIT_MM_MTX_AXI_CKG_EN | BIT_MM_AXI_CKG_EN,
		     (void *)REG_MM_AHB_GEN_CKG_CFG);

	__raw_writel(__raw_readl((void *)REG_MM_CLK_MM_AHB_CFG) | 0x3,
		     (void *)REG_MM_CLK_MM_AHB_CFG);
#ifndef CONFIG_MACH_SPX15FPGA
#if defined(CONFIG_DEBUG_FS)
	clk_debugfs_root = debugfs_create_dir("sprd-clock", NULL);
	if (IS_ERR_OR_NULL(clk_debugfs_root))
		return -ENOMEM;
#endif

	/* register all clock sources */
	{
		struct clk_lookup *cl =
		    (struct clk_lookup *)(&__clkinit_begin + 1);
		struct clk_lookup *cl_end = (struct clk_lookup *)&__clkinit_end;
		int seq = (0 == strcmp(cl->con_id, "ext_26m"));

		debug0("%p (%x) -- %p -- %p (%x)\n",
		       &__clkinit_begin, __clkinit_begin, cl,
		       &__clkinit_end, __clkinit_end);

		if (seq) {
			while (cl < cl_end)
				sci_clk_register(cl++);
		} else {
			while (--cl_end >= cl)
				sci_clk_register(cl_end);
		}
	}

	/* keep track of cpu frequency transitions */
	cpufreq_register_notifier(&__clk_cpufreq_notifier_block,
				  CPUFREQ_TRANSITION_NOTIFIER);
#endif
	return 0;
}

arch_initcall(sci_clock_init);

/* FIXME: clock dump fail when gpu/mm domain power off
*/
late_initcall_sync(sci_clock_dump);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum Clock Driver");
MODULE_AUTHOR("robot <zhulin.lian@spreadtrum.com>");
MODULE_VERSION("0.2");
