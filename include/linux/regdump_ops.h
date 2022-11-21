/*
 *  regdump.h - framework for dump the system periperal registers.
 *
 *  Copyright (C) 2012 Lei Wen <leiwen@marvell.com>, Marvell Inc.
 *
 *  This file is released under the GPLv2.
 */

#ifndef _LINUX_REGDUMP_OPS_H
#define _LINUX_REGDUMP_OPS_H

#include <linux/list.h>
#include <linux/slab.h>

#ifdef CONFIG_REGDUMP
struct regdump_ops;
struct regdump_region {
	const char		*name;
	unsigned long		start;
	unsigned long		size;
	int (*cond) (struct regdump_ops *ops);
};

struct regdump_ops {
	struct list_head 	node;
	const char		*dev_name;
	int			enable;
	unsigned long		base;
	void			*buffer;

	struct regdump_region	*regions;
	unsigned long		reg_nums;
};

extern int dump_reg_to_mem(void);
extern int register_regdump_ops(struct regdump_ops *ops);
extern int unregister_regdump_ops(struct regdump_ops *ops);
static int inline regdump_cond_true(struct regdump_ops *ops)
{
	return 1;
}
#else
#define register_regdump_ops()		do {} while(0)
#define unregister_regdump_ops()	do {} while(0)
#define dump_reg_to_mem()		do {} while(0)
#define regdump_cond_true()		do {} while(0)
#endif
#endif
