/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>

#include <plat/devs.h>

#include <mach/map.h>
#include <mach/bts.h>
#include <mach/regs-bts.h>

enum bts_index {
	BTS_IDX_CPU = 0,
	BTS_IDX_G2D = 1,
	BTS_IDX_G3D = 2,
	BTS_IDX_FIMC_L0 = 3,
	BTS_IDX_FIMC_L1 = 4,
};

enum bts_id {
	BTS_CPU = (1 << BTS_IDX_CPU),
	BTS_G2D = (1 << BTS_IDX_G2D),
	BTS_G3D = (1 << BTS_IDX_G3D),
	BTS_FIMC_L0 = (1 << BTS_IDX_FIMC_L0),
	BTS_FIMC_L1 = (1 << BTS_IDX_FIMC_L1),
};

struct bts_table {
	struct bts_set_table *table_list;
	unsigned int table_num;
};

struct bts_info {
	enum bts_id id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table;
	const char *pd_name;
	const char *devname;
	const char *clk_name;
	struct clk *clk;
	struct list_head list;
};

struct bts_set_table {
	unsigned int reg;
	unsigned int val;
};

void __iomem *fbm_right;
void __iomem *tidemark_gdr;

static struct bts_set_table axiqos_highest_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0xffff},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},

	{WRITE_CHANNEL_PRIORITY, 0xffff},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{READ_QOS_CONTROL, 0x1},
	{WRITE_QOS_CONTROL, 0x1}
};

static struct bts_set_table fbm_r_high_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x2},

	{WRITE_CHANNEL_PRIORITY, 0x4444},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{WRITE_DEMOTION_WINDOW, 0x7fff},
	{WRITE_DEMOTION_TOKEN, 0x1},
	{WRITE_DEFAULT_WINDOW, 0x7fff},
	{WRITE_DEFAULT_TOKEN, 0x1},
	{WRITE_PROMOTION_WINDOW, 0x7fff},
	{WRITE_PROMOTION_TOKEN, 0x1},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x2},
	{READ_QOS_MODE, 0x1},
	{WRITE_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
	{WRITE_QOS_CONTROL, 0x7}
};

#define BTS_CAM (BTS_FIMC_L0 | BTS_FIMC_L1)

#ifdef BTS_DBGGEN
#define BTS_DBG(x...) pr_err(x)
#else
#define BTS_DBG(x...) do {} while (0)
#endif

#ifdef BTS_DBGGEN1
#define BTS_DBG1(x...) pr_err(x)
#else
#define BTS_DBG1(x...) do {} while (0)
#endif

static struct bts_info exynos4_bts[] = {
	[BTS_IDX_CPU] = {
		.id = BTS_CPU,
		.name = "cpu",
		.pa_base = EXYNOS4_PA_BTS_CPU,
		.pd_name = "pd-cpu",
		.table.table_list = fbm_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_r_high_table),
	},
	[BTS_IDX_G2D] = {
		.id = BTS_G2D,
		.name = "fimg2d",
		.pa_base = EXYNOS4_PA_BTS_G2D,
		.pd_name = "pd-g2d",
		.clk_name = "fimg2d",
		.devname = "s5p-fimg2d",
		.table.table_list = fbm_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_r_high_table),
	},
	[BTS_IDX_G3D] = {
		.id = BTS_G3D,
		.name = "g3d",
		.pa_base = EXYNOS4_PA_BTS_G3D,
		.pd_name = "pd-g3d",
		.clk_name = "sclk_g3d",
		.table.table_list = fbm_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_r_high_table),
	},
	[BTS_IDX_FIMC_L0] = {
		.id = BTS_FIMC_L0,
		.name = "fimc-l0",
		.pa_base = EXYNOS4_PA_BTS_FIMC_L0,
		.pd_name = "pd-cam",
		.table.table_list = axiqos_highest_table,
		.table.table_num = ARRAY_SIZE(axiqos_highest_table),
	},
	[BTS_IDX_FIMC_L1] = {
		.id = BTS_FIMC_L1,
		.name = "fimc-l1",
		.pa_base = EXYNOS4_PA_BTS_FIMC_L1,
		.pd_name = "pd-cam",
		.table.table_list = axiqos_highest_table,
		.table.table_num = ARRAY_SIZE(axiqos_highest_table),
	},
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);

static int exynos_bts_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	switch (event) {
	case PM_POST_SUSPEND:
		bts_initialize("pd-cpu", true);
		bts_initialize("pd-g2d", true);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

static void set_bts_ip_table(struct bts_info *bts)
{
	int i;
	struct bts_set_table *table = bts->table.table_list;

	BTS_DBG("[BTS] bts set: %s\n", bts->name);

	if (bts->clk)
		clk_enable(bts->clk);

	for (i = 0; i < bts->table.table_num; i++) {
		__raw_writel(table->val, bts->va_base + table->reg);
		BTS_DBG1("[BTS] %x-%x\n", table->reg, table->val);
		table++;
	}

	if (bts->clk)
		clk_disable(bts->clk);
}

static void set_cam_scenario(bool on)
{
	BTS_DBG("[BTS] cam scenario on:%x\n", on);

	if (on){
		__raw_writel(0x6, tidemark_gdr + 0x400);
		__raw_writel(0x6, tidemark_gdr + 0x404);

		__raw_writel(0x3, fbm_right + FBM_MODESEL0);
		__raw_writel(0x4, fbm_right + FBM_THRESHOLD0);
		__raw_writel(0x0, fbm_right + FBM_OUTSEL0);

		__raw_writel(0x3, fbm_right + FBM_MODESEL1);
		__raw_writel(0x3, fbm_right + FBM_THRESHOLD1);
		__raw_writel(0x1, fbm_right + FBM_OUTSEL2);

		__raw_writel(0x3, fbm_right + FBM_MODESEL2);
		__raw_writel(0x3, fbm_right + FBM_THRESHOLD2);
		__raw_writel(0x2, fbm_right + FBM_OUTSEL20);
	} else {
		__raw_writel(0x4, fbm_right + FBM_THRESHOLD0);
		__raw_writel(0x8, fbm_right + FBM_THRESHOLD1);
		__raw_writel(0x8, fbm_right + FBM_THRESHOLD2);

		__raw_writel(0x0, tidemark_gdr + 0x400);
		__raw_writel(0x0, tidemark_gdr + 0x404);
	}
}

static void set_fbm(void)
{
	BTS_DBG("[BTS] FBM SETUP\n");

	__raw_writel(0x3, fbm_right + FBM_MODESEL0);
	__raw_writel(0x4, fbm_right + FBM_THRESHOLD0);
	__raw_writel(0x0, fbm_right + FBM_OUTSEL0);

	__raw_writel(0x3, fbm_right + FBM_MODESEL1);
	__raw_writel(0x8, fbm_right + FBM_THRESHOLD1);
	__raw_writel(0x1, fbm_right + FBM_OUTSEL2);

	__raw_writel(0x3, fbm_right + FBM_MODESEL2);
	__raw_writel(0x8, fbm_right + FBM_THRESHOLD2);
	__raw_writel(0x2, fbm_right + FBM_OUTSEL20);

	__raw_writel(0x0, tidemark_gdr + 0x400);
	__raw_writel(0x0, tidemark_gdr + 0x404);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;
	bool cam_flag = false;

	spin_lock(&bts_lock);

	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);

	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			if (on && (bts->id & BTS_CPU))
				set_fbm();

			if (on)
				set_bts_ip_table(bts);

			if (bts->id & BTS_CAM)
				cam_flag = true;
		}

	if (cam_flag)
		set_cam_scenario(on);

	spin_unlock(&bts_lock);
}

static int __init exynos4_bts_init(void)
{
	int i;
	struct clk *clk;

	BTS_DBG("[BTS] bts init]\n");

	fbm_right = ioremap(EXYNOS4_FBM_RIGHT, SZ_4K);
	tidemark_gdr = ioremap(EXYNOS4_TIDEMARK_GDR, SZ_4K);

	for (i = 0; i < ARRAY_SIZE(exynos4_bts); i++) {
		if (exynos4_bts[i].clk_name) {
			clk = clk_get_sys(exynos4_bts[i].devname,
				exynos4_bts[i].clk_name);
			if (IS_ERR(clk))
				pr_err("failed to get bts clk %s\n",
					exynos4_bts[i].clk_name);
			else
				exynos4_bts[i].clk = clk;
		}

		exynos4_bts[i].va_base
			= ioremap(exynos4_bts[i].pa_base, SZ_4K);

		list_add(&exynos4_bts[i].list, &bts_list);
	}

	bts_initialize("pd-cpu", true);

	register_pm_notifier(&exynos_bts_notifier);

	return 0;
}
arch_initcall(exynos4_bts_init);
