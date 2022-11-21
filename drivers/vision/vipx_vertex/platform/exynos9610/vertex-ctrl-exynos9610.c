/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>

#include "vertex-log.h"
#include "vertex-system.h"
#include "platform/vertex-ctrl.h"

enum vertex_reg_ss1_id {
	REG_SS1_VERSION_ID,
	REG_SS1_QCHANNEL,
	REG_SS1_IRQ_VIPX_TO_HOST,
	REG_SS1_IRQ0_HOST_TO_VIPX,
	REG_SS1_IRQ1_HOST_TO_VIPX,
	REG_SS1_GLOBAL_CTRL,
	REG_SS1_CORTEX_CONTROL,
	REG_SS1_CPU_CTRL,
	REG_SS1_PROGRAM_COUNTER,
	REG_SS1_DEBUG0,
	REG_SS1_DEBUG1,
	REG_SS1_DEBUG2,
	REG_SS1_DEBUG3,
	REG_SS1_MAX
};

enum vertex_reg_ss2_id {
	REG_SS2_QCHANNEL,
	REG_SS2_GLOBAL_CTRL,
	REG_SS2_MAX
};

static const struct vertex_reg regs_ss1[] = {
	{0x00000, "SS1_VERSION_ID"},
	{0x00004, "SS1_QCHANNEL"},
	{0x00008, "SS1_IRQ_VIPX_TO_HOST"},
	{0x0000C, "SS1_IRQ0_HOST_TO_VIPX"},
	{0x00010, "SS1_IRQ1_HOST_TO_VIPX"},
	{0x00014, "SS1_GLOBAL_CTRL"},
	{0x00018, "SS1_CORTEX_CONTROL"},
	{0x0001C, "SS1_CPU_CTRL"},
	{0x00044, "SS1_PROGRAM_COUNTER"},
	{0x00048, "SS1_DEBUG0"},
	{0x0004C, "SS1_DEBUG1"},
	{0x00050, "SS1_DEBUG2"},
	{0x00054, "SS1_DEBUG3"},
};

static const struct vertex_reg regs_ss2[] = {
	{0x00000, "SS2_QCHANNEL"},
	{0x00004, "SS2_GLOBAL_CTRL"},
};

static int vertex_exynos9610_ctrl_reset(struct vertex_system *sys)
{
	void __iomem *ss1, *ss2;
	unsigned int val;

	vertex_enter();
	ss1 = sys->reg_ss[VERTEX_REG_SS1];
	ss2 = sys->reg_ss[VERTEX_REG_SS2];

	/* TODO: check delay */
	val = readl(ss1 + regs_ss1[REG_SS1_GLOBAL_CTRL].offset);
	writel(val | 0xF1, ss1 + regs_ss1[REG_SS1_GLOBAL_CTRL].offset);
	udelay(10);

	val = readl(ss2 + regs_ss2[REG_SS2_GLOBAL_CTRL].offset);
	writel(val | 0xF1, ss2 + regs_ss2[REG_SS2_GLOBAL_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	udelay(10);

	writel(0x0, ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_QCHANNEL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_QCHANNEL].offset);
	udelay(10);

	val = readl(ss2 + regs_ss2[REG_SS2_QCHANNEL].offset);
	writel(val | 0x1, ss2 + regs_ss2[REG_SS2_QCHANNEL].offset);
	udelay(10);

	vertex_leave();
	return 0;
}

static int vertex_exynos9610_ctrl_start(struct vertex_system *sys)
{
	vertex_enter();
	writel(0x0, sys->reg_ss[VERTEX_REG_SS1] +
			regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	vertex_leave();
	return 0;
}

static int vertex_exynos9610_ctrl_get_interrupt(struct vertex_system *sys,
		int direction)
{
	vertex_enter();
	writel(0x0, sys->reg_ss[VERTEX_REG_SS1] +
			regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	vertex_leave();
	return 0;
}

static int vertex_exynos9610_ctrl_set_interrupt(struct vertex_system *sys,
		int direction)
{
	vertex_enter();
	vertex_leave();
	return 0;
}

static int vertex_exynos9610_ctrl_debug_dump(struct vertex_system *sys)
{
	void __iomem *ss1;
	unsigned int val;
	const char *name;

	vertex_enter();
	ss1 = sys->reg_ss[VERTEX_REG_SS1];

	val = readl(ss1 + regs_ss1[REG_SS1_PROGRAM_COUNTER].offset);
	name = regs_ss1[REG_SS1_PROGRAM_COUNTER].name;
	vertex_info("[%20s] 0x%08x\n", name, val);

	val = readl(ss1 + regs_ss1[REG_SS1_DEBUG0].offset);
	name = regs_ss1[REG_SS1_DEBUG0].name;
	vertex_info("[%20s] 0x%08x\n", name, val);

	val = readl(ss1 + regs_ss1[REG_SS1_DEBUG1].offset);
	name = regs_ss1[REG_SS1_DEBUG1].name;
	vertex_info("[%20s] 0x%08x\n", name, val);

	val = readl(ss1 + regs_ss1[REG_SS1_DEBUG2].offset);
	name = regs_ss1[REG_SS1_DEBUG2].name;
	vertex_info("[%20s] 0x%08x\n", name, val);

	val = readl(ss1 + regs_ss1[REG_SS1_DEBUG3].offset);
	name = regs_ss1[REG_SS1_DEBUG3].name;
	vertex_info("[%20s] 0x%08x\n", name, val);

	vertex_leave();
	return 0;
}

const struct vertex_ctrl_ops vertex_ctrl_ops = {
	.reset		= vertex_exynos9610_ctrl_reset,
	.start		= vertex_exynos9610_ctrl_start,
	.get_interrupt	= vertex_exynos9610_ctrl_get_interrupt,
	.set_interrupt	= vertex_exynos9610_ctrl_set_interrupt,
	.debug_dump	= vertex_exynos9610_ctrl_debug_dump,
};
