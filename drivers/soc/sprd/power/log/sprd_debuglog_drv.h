/*
 * sprd_debuglog.h -- Sprd Debug Power data type description support.
 *
 * Copyright (C) 2020, 2021 unisoc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Debug Power data type description.
 */

#ifndef _SPRD_DEBUGLOG_DRV_H
#define _SPRD_DEBUGLOG_DRV_H

#include <linux/device.h>

/* bit */
struct reg_bit {
	char *name;
	u32 mask;
	u32 offset;
	u32 expect;
};

/* register */
struct reg_info {
	char *name;
	u32 offset;
	struct reg_bit *bit;
	u32 num;
};

/* table */
struct reg_table {
	char *name, *dts;
	struct reg_info *reg;
	u32 num;
};

/* Register init macor */
#define REG_BIT_INIT(bit_name, bit_mask, bit_offset, bit_expect)	\
{									\
	.name = bit_name,						\
	.mask = bit_mask,						\
	.offset = bit_offset,						\
	.expect = bit_expect,						\
}

#define REG_INFO_INIT(reg_name, reg_offset, bit_set)			\
{									\
	.name = reg_name,						\
	.offset = reg_offset,						\
	.num = ARRAY_SIZE(bit_set),					\
	.bit = bit_set,							\
}

#define REG_TABLE_INIT(table_name, dts_name, reg_set)			\
{									\
	.name = table_name,						\
	.dts = dts_name,						\
	.num = ARRAY_SIZE(reg_set),					\
	.reg = reg_set,							\
}

struct plat_data {
	struct reg_table *sleep_condition_table;
	u32 sleep_condition_table_num;
	struct reg_table *subsys_state_table;
	u32 subsys_state_table_num;
	char **wakeup_source_info;
	u32 wakeup_source_info_num;
	int (*wakeup_source_match)(u32 m, u32 s, u32 t, void *data, int num);
};

#endif /* _SPRD_DEBUGLOG_DRV_H */
