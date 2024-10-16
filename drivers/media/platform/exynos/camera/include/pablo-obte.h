// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_OBTE_H
#define PABLO_OBTE_H

/*
 * ENUM defined by OBTE
 * V4L2_CID_IS_OBTE_CONFIG: bit field of value
 * {0} VC_STAT_TYPE, 0=BSP (live), 1=INVALID (emulation)
 * {1} use of custom bayer order, 0=disable, 1=enable
 * {2:3} bayer order, 0=GRGB, 1=RGGB, 2=BGGR, 3=GBRG
 * {4} use of custom sensor binning, 0=disable, 1=enable
 * {5:9} sensor binning, e.g. 00001(b) = 1x binning, 00010(b) = 2x binning, 01100(b) = 12x binning
 * {14} INPUT BITWIDTH (EX_12BIT), 0=BSP, 1=14BIT (backward compatibility)
 * {15:31} reserved
 */
enum v4l2_cid_pablo_obte_config_bit {
	OBTE_CONFIG_BIT_VC_STAT		=	0,
	OBTE_CONFIG_BIT_BAYER_ORDER_EN	=	1,
	OBTE_CONFIG_BIT_BAYER_ORDER_LSB	=	2,
	OBTE_CONFIG_BIT_BAYER_ORDER_MSB	=	3,
	OBTE_CONFIG_BIT_SENSOR_BINNING_EN	=	4,
	OBTE_CONFIG_BIT_SENSOR_BINNING_RATIO	=	5,

	OBTE_CONFIG_BIT_EX_12BIT	=	14, /* to use EX_12BIT 14 in exynos_is_dt.h */

	OBTE_CONFIG_BIT_MAX		=	31,
};

#define OBTE_CONFIG_BITMASK_SENSOR_BINNING_RATIO 0x3E0
#define OBTE_CONFIG_GET_SENSOR_BINNING_RATIO(binning) \
	((((binning) & OBTE_CONFIG_BITMASK_SENSOR_BINNING_RATIO) >> \
	OBTE_CONFIG_BIT_SENSOR_BINNING_RATIO) * 1000)

#if IS_ENABLED(CONFIG_PABLO_OBTE_SUPPORT)
bool pablo_obte_is_running(void);

int __nocfi pablo_obte_init_3aa(u32 instance, bool flag);
void pablo_obte_deinit_3aa(u32 instance);

void pablo_obte_setcount_ssx_open(u32 id, s32 status);
void pablo_obte_setcount_ssx_close(u32 id);

void pablo_obte_regdump(u32 instance_id, u32 hw_id, u32 stripe_idx, u32 stripe_num);

int pablo_obte_init(void);
void pablo_obte_exit(void);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
void pablo_kunit_obte_set_interface(void *itf);
#endif

#else
#define pablo_obte_is_running()			({0;})
#define pablo_obte_init_3aa(i, f)		({0;})
#define pablo_obte_deinit_3aa(i)		do { } while(0)
#define pablo_obte_setcount_ssx_open(i, s)	do { } while(0)
#define pablo_obte_setcount_ssx_close(i)	do { } while(0)
#define pablo_obte_regdump(i, h, si, sn)	do { } while(0)
#define pablo_obte_init()			({0;})
#define pablo_obte_exit()			do { } while(0)
#endif

#define IS_RUNNING_TUNING_SYSTEM() (pablo_obte_is_running())

#define trace_obte() \
	pr_info("[@][OBTE][TRACE]%s:%d\n", __func__, __LINE__)
#define dbg_obte(fmt, args...) \
	pr_info("[@][OBTE][INF]%s:%d:" fmt, __func__, __LINE__, ##args)
#define warn_obte(fmt, args...) \
	pr_warn("[@][OBTE][WARN]%s:%d:" fmt, __func__, __LINE__, ##args)
#define err_obte(fmt, args...) \
	pr_err("[@][OBTE][ERR]%s:%d:" fmt, __func__, __LINE__, ##args)

#endif
