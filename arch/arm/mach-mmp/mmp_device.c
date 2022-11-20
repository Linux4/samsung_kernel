
/*
 * mmp_device implementation
 *
 * Copyright (C) 2012 Marvell Inc
 * Lei Wen <leiwen@marvell.com>
 *
 * Developed in collaboration with (alphabetical order): Benoit
 * Cousson, Thara Gopinath, Tony Lindgren, Rajendra Nayak, Vikram
 * Pandita, Sakari Poussa, Anand Sawant, Santosh Shilimkar, Richard
 * Woodruff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/pm_runtime.h>
#include <linux/notifier.h>
#include <plat/pm.h>
#include <mach/mmp_device.h>

static LIST_HEAD(mmp_device_hw_list);

static int _mmp_device_enable(struct mmp_device *md)
{
	struct mmp_device_hw *hw = md->hw;
	int i;

	/* avoid system enter low power modes */
	if (hw->constraint)
		pm_qos_update_request(&hw->qos_idle,
				hw->constraint);

	if (hw->activate_func)
		return hw->activate_func(md);

	clk_enable(hw->fn_clk.clk);
	for (i = 0; i < MAX_OPT_CLKS && hw->opt_clks[i].clk; i ++)
		clk_enable(hw->opt_clks[i].clk);

	return 0;
}

static int _mmp_device_disable(struct mmp_device *md)
{
	struct mmp_device_hw *hw = md->hw;
	int i;

	if (hw->deactivate_func)
		return hw->deactivate_func(md);

	clk_disable(hw->fn_clk.clk);
	for (i = 0; i < MAX_OPT_CLKS && hw->opt_clks[i].clk; i ++)
		clk_disable(hw->opt_clks[i].clk);

	/* allow system enter low power modes */
	if (hw->constraint)
		pm_qos_update_request(&hw->qos_idle,
				PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	return 0;
}

int mmp_device_enable(struct platform_device *pdev)
{
	int ret;
	struct mmp_device *md;

	md = to_mmp_device(pdev);

	if (md->state == MMP_DEVICE_STATE_ACTIVE) {
		dev_warn(&pdev->dev,
			 "mmp_device: %s() called from invalid state %d\n",
			 __func__, md->state);
		return -EINVAL;
	}

	ret = _mmp_device_enable(md);

	md->state = MMP_DEVICE_STATE_ACTIVE;

	return ret;
}

int mmp_device_disable(struct platform_device *pdev)
{
	int ret;
	struct mmp_device *md;

	md = to_mmp_device(pdev);

	if (md->state != MMP_DEVICE_STATE_ACTIVE) {
		dev_warn(&pdev->dev,
			 "mmp_device: %s() called from invalid state %d\n",
			 __func__, md->state);
		return -EINVAL;
	}

	ret = _mmp_device_disable(md);

	md->state = MMP_DEVICE_STATE_IDLE;

	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int md_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	ret = pm_generic_runtime_suspend(dev);

	if (!ret)
		mmp_device_disable(pdev);

	return ret;
}

static int md_runtime_idle(struct device *dev)
{
	return pm_generic_runtime_idle(dev);
}

static int md_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	mmp_device_enable(pdev);

	return pm_generic_runtime_resume(dev);
}
#endif

#ifdef CONFIG_SUSPEND
static int md_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmp_device *md = to_mmp_device(pdev);
	int ret;

	ret = pm_generic_suspend_noirq(dev);

	if (!ret && !pm_runtime_status_suspended(dev)) {
		if (pm_generic_runtime_suspend(dev) == 0) {
			if (!(md->flags & MMP_DEVICE_NO_IDLE_ON_SUSPEND))
				mmp_device_disable(pdev);
			md->flags |= MMP_DEVICE_SUSPENDED;
		}
	}

	return ret;
}

static int md_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmp_device *md = to_mmp_device(pdev);

	if ((md->flags & MMP_DEVICE_SUSPENDED) &&
	    !pm_runtime_status_suspended(dev)) {
		md->flags &= ~MMP_DEVICE_SUSPENDED;
		if (!(md->flags & MMP_DEVICE_NO_IDLE_ON_SUSPEND))
			mmp_device_enable(pdev);
		pm_generic_runtime_resume(dev);
	}

	return pm_generic_resume_noirq(dev);
}
#else
#define md_suspend_noirq NULL
#define md_resume_noirq NULL
#endif

struct dev_pm_domain mmp_device_pm_domain = {
	.ops = {
		SET_RUNTIME_PM_OPS(md_runtime_suspend, md_runtime_resume,
				   md_runtime_idle)
		USE_PLATFORM_PM_SLEEP_OPS
		.suspend_noirq = md_suspend_noirq,
		.resume_noirq = md_resume_noirq,
	}
};

static int init_mmp_clk(struct mmp_clk *mmp_clk, const char *clk_name)
{
	mmp_clk->name = clk_name;
	mmp_clk->clk = clk_get(NULL, clk_name);
	if (IS_ERR(mmp_clk->clk)) {
		printk(KERN_WARNING "cannot get clk(%s)\n", clk_name);
		return PTR_ERR(mmp_clk->clk);
	}

	return 0;
}

int __init mmp_device_hw_register(struct mmp_hw_desc *desc)
{
	struct mmp_device_hw * hw;
	int i, ret;

	hw = kzalloc(sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	hw->name = desc->name;
	hw->id = desc->id;
	hw->constraint = desc->constraint;

	ret = init_mmp_clk(&hw->fn_clk, desc->fn_clk);
	if (ret)
		goto error_exit;

	for (i = 0; i < MAX_OPT_CLKS && desc->opt_clks[i]; i ++) {
		ret = init_mmp_clk(&hw->opt_clks[i], desc->opt_clks[i]);
		if (ret)
			goto error_exit;
	}

	list_add_tail(&hw->node, &mmp_device_hw_list);

	/* init qos with constraint */
	if (hw->constraint) {
		hw->qos_idle.name = hw->name;
		pm_qos_add_request(&hw->qos_idle, PM_QOS_CPUIDLE_BLOCK,
				PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	}
	return 0;

error_exit:
	kfree(hw);
	return ret;
}

static struct mmp_device_hw *hw_lookup(const char *name)
{
	struct mmp_device_hw *hw, *temp;

	hw = NULL;
	list_for_each_entry(temp, &mmp_device_hw_list, node) {
		if (!strcmp(name, temp->name)) {
			hw = temp;
			break;
		}
	}

	return hw;
}

static void mmp_device_build(struct platform_device *pdev)
{
	struct mmp_device_hw *hw;
	struct mmp_device *md;

	hw = hw_lookup(pdev->name);
	if (!hw)
		return;

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return;

	md->pdev = pdev;
	md->hw = hw;
	md->state = MMP_DEVICE_STATE_IDLE;
	pdev->archdata.md = md;
	pdev->dev.pm_domain = &mmp_device_pm_domain;
}

static void mmp_device_delete(struct platform_device *pdev)
{
	struct mmp_device *md = pdev->archdata.md;

	if (!md)
		return;

	pdev->archdata.md = NULL;
	kfree(md);
}

static int mmp_device_notifier_call(struct notifier_block *nb,
				      unsigned long event, void *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	/* only do the lookup the pdev name */
	if (!pdev->name)
		return NOTIFY_DONE;

	switch (event) {
	case BUS_NOTIFY_ADD_DEVICE:
		mmp_device_build(pdev);
		break;

	case BUS_NOTIFY_DEL_DEVICE:
		mmp_device_delete(pdev);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block platform_nb = {
	.notifier_call = mmp_device_notifier_call,
};


static int __init mmp_device_init(void)
{
	bus_register_notifier(&platform_bus_type, &platform_nb);
	return 0;
}
core_initcall(mmp_device_init);
