/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos mDNIE driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_MDNIE_H__
#define __SAMSUNG_MDNIE_H__

#define MDNIE_FSM_VFP		50
#define MDNIE_FSM_VSW		3
#define MDNIE_FSM_VBP		2
#define MDNIE_FSM_HFP		1
#define MDNIE_FSM_HSW		1
#define MDNIE_FSM_HBP		1

/* #define FW_TEST */

#ifdef FW_TEST
#include "decon_fw.h"
#define MDNIE_BASE 0xd000

static inline u32 mdnie_read(u32 reg_id)
{
	u32 uReg;
	uReg= get_decon_drvdata(0);
	//return readl(uReg + MDNIE_BASE + reg_id * 4);
	return readl(uReg + MDNIE_BASE + reg_id);
}

static inline u32 mdnie_read_mask(u32 reg_id, u32 mask)
{
	u32 val = mdnie_read(reg_id);
	val &= (~mask);
	return val;
}

static inline void mdnie_write(u32 reg_id, u32 val)
{
	u32 uReg;
	uReg = get_decon_drvdata(0);
	//writel(val,uReg + MDNIE_BASE +reg_id * 4);
	writel(val,uReg + MDNIE_BASE +reg_id);
}

static inline void mdnie_write_mask(u32 reg_id, u32 val, u32 mask)
{
	u32 uReg;
	uReg = get_decon_drvdata(0);
	u32 old = mdnie_read(reg_id);

	val = (val & mask) | (old & ~mask);
	//writel(val, uReg + MDNIE_BASE +reg_id * 4);
	writel(val, uReg + MDNIE_BASE +reg_id);
}
#else
#include "decon.h"
#define MDNIE_BASE 0xd000

static inline u32 mdnie_read(u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(0);
	return readl(decon->regs + MDNIE_BASE + reg_id * 4);
}

static inline u32 mdnie_read_mask(u32 reg_id, u32 mask)
{
	u32 val = mdnie_read(reg_id);
	val &= (~mask);
	return val;
}

static inline void mdnie_write(u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(0);
	writel(val, decon->regs + MDNIE_BASE + reg_id * 4);
}

static inline void mdnie_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u32 old = mdnie_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, decon->regs + MDNIE_BASE + reg_id * 4);
}
#endif

void mdnie_reg_set_img_size(u32 w, u32 h);
void mdnie_reg_set_hsync_period(u32 hync);
void mdnie_reg_enable_input_data(u32 en);
void mdnie_reg_unmask_all(void);

void mdnie_reg_start(u32 w, u32 h);
void mdnie_reg_update_frame(u32 w, u32 h);
void mdnie_reg_stop(void);

#endif /* __SAMSUNG_MDNIE_H__ */
