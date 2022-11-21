/*
 * arch/arm/mach-mmp/include/mach/mmp_device.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MMP_DEVICE__
#define __MMP_DEVICE__

#include <linux/pm_qos.h>

#define MAX_OPT_CLKS	3
struct mmp_hw_desc {
	const char		*name;
	int			id;
	unsigned int		constraint;
	const char		*fn_clk;
	const char		*opt_clks[MAX_OPT_CLKS];
};

struct mmp_clk {
	const char	*name;
	struct clk	*clk;
};

struct mmp_device;
struct mmp_device_hw {
	const char		*name;
	int			id;
	struct mmp_clk		fn_clk;
	struct mmp_clk		opt_clks[MAX_OPT_CLKS];
	unsigned int		constraint;
	int (*activate_func)(struct mmp_device *);
	int (*deactivate_func)(struct mmp_device *);

	struct pm_qos_request   qos_idle;
	struct list_head	node;
};

struct mmp_device {
	struct platform_device	*pdev;
	struct mmp_device_hw	*hw;
#define MMP_DEVICE_STATE_ACTIVE		1
#define MMP_DEVICE_STATE_IDLE		2
	unsigned int		state;

#define MMP_DEVICE_NO_IDLE_ON_SUSPEND 	(1 << 0)
#define MMP_DEVICE_SUSPENDED		(1 << 1)
	unsigned int		flags;
};

#define MMP_HW_DESC(_name, _desc, _id, _constraint, _fn_clk, _opt_clk...) \
struct mmp_hw_desc mmp_device_hw_##_name __initdata = {			\
	.name		= _desc,					\
	.id		= _id,						\
	.constraint	= _constraint,					\
	.fn_clk		= _fn_clk,					\
	.opt_clks	= { _opt_clk },					\
}

static inline struct mmp_device *to_mmp_device(struct platform_device *pdev)
{
	return pdev ? pdev->archdata.md : NULL;
}

int __init mmp_device_hw_register(struct mmp_hw_desc *desc);
#endif /* __MMP_DEVICE__ */
