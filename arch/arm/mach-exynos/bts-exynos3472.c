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
#include <mach/regs-pmu.h>

#define EXYNOS3472_PA_DREX0		0x105f0000

void __iomem *drex0_va_base;

enum bts_index {
	BTS_IDX_FIMD0 = 0,
	BTS_IDX_FIMD1,
	BTS_IDX_FIMCLITE0,
	BTS_IDX_FIMCLITE1,
	BTS_IDX_G3D,
	BTS_IDX_CPU,
	BTS_IDX_FBM_R,
	BTS_IDX_TIDE_GDR,
};

enum bts_id {
	BTS_FIMD0 = (1 << BTS_IDX_FIMD0),
	BTS_FIMD1 = (1 << BTS_IDX_FIMD1),
	BTS_FIMCLITE0 = (1 << BTS_IDX_FIMCLITE0),
	BTS_FIMCLITE1 = (1 << BTS_IDX_FIMCLITE1),
	BTS_G3D = (1 << BTS_IDX_G3D),
	BTS_CPU = (1 << BTS_IDX_CPU),
	BTS_FBM_R = (1 << BTS_IDX_FBM_R),
	BTS_TIDEMARK = (1 << BTS_IDX_TIDE_GDR),
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
	const char *devname;
	const char *pd_name;
	const char *clk_name;
	struct clk *clk;
	bool on;
	struct list_head list;
};

struct bts_set_table {
	unsigned int reg;
	unsigned int val;
};

struct bts_ip_clk {
	const char *clkname;
	const char *devname;
	struct clk *clk;
};

struct bts_scen_status {
	bool fbm_r;
	bool tidemark;
	bool cpu;
	bool g3d;
};

struct bts_scen_status pr_state = {
	.fbm_r = false,
	.tidemark = false,
	.cpu = false,
	.g3d = false,
};

#define update_fbm_r(a) (pr_state.fbm_r = a)
#define update_tidemark(a) (pr_state.tidemark = a)
#define update_cpu(a) (pr_state.cpu = a)
#define update_g3d(a) (pr_state.g3d = a)

#define BTS_TABLE(num)					\
static struct bts_set_table axiqos_##num##_table[] = {	\
	{READ_QOS_CONTROL, 0x0},			\
	{WRITE_QOS_CONTROL, 0x0},			\
	{READ_CHANNEL_PRIORITY, num},			\
	{WRITE_CHANNEL_PRIORITY, num},			\
	{READ_QOS_CONTROL, 0x1},			\
	{WRITE_QOS_CONTROL, 0x1}			\
}

#define BTS_IP (BTS_FIMD0 | BTS_FIMD1 | BTS_FIMCLITE0 | BTS_FIMCLITE1 | BTS_G3D | BTS_CPU)

BTS_TABLE(0xdddd);
BTS_TABLE(0xeeee);

static struct bts_set_table fbm_l_r_high_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
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

static struct bts_set_table disable_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
};

static struct bts_set_table fbm_r_table[] = {
	{0x0000, 0x2},
	{0x0040, 0x8},
	{0x0080, 0x0},
	{0x0084, 0x0},
	{0x0088, 0x0},
	{0x008c, 0x0},
	{0x0090, 0x0},
	{0x0094, 0x0},
	{0x0098, 0x0},
	{0x009c, 0x0},
	{0x00a0, 0x0},
	{0x00a4, 0x0},
	{0x00a8, 0x0},
	{0x00ac, 0x0},
	{0x00b0, 0x0},
	{0x00b4, 0x0},
	{0x00b8, 0x0},
	{0x00bc, 0x0},
	{0x00c0, 0x0},
	{0x00c4, 0x0},
	{0x00c8, 0x0},
	{0x00cc, 0x0},
	{0x00d0, 0x0},
	{0x00d4, 0x0},
	{0x00d8, 0x0},
	{0x00dc, 0x0},
	{0x00e0, 0x0},
	{0x00e4, 0x0},
	{0x00e8, 0x0},
	{0x00ec, 0x0},
	{0x00f0, 0x0},
	{0x00f4, 0x0},
	{0x00f8, 0x0},
	{0x00fc, 0x0},
};

static struct bts_set_table tidemark_gdr_table[] = {
	{0x0400, 0xA},
	{0x0404, 0x6},
};

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

static void bts_drex_init(void)
{
	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	__raw_writel(0x00000000, drex0_va_base + 0xD8);
	__raw_writel(0x00800080, drex0_va_base + 0xD0);
	__raw_writel(0x00800080, drex0_va_base + 0xC8);
	__raw_writel(0x0fff0fff, drex0_va_base + 0xC0);
}

static struct bts_info exynos3_bts[] = {
	[BTS_IDX_FIMD0] = {
		.id = BTS_FIMD0,
		.name = "fimd0",
		.pa_base = EXYNOS3_PA_BTS_FIMD0,
		.pd_name = "pd-fimd0",
		.clk_name = "lcd",
		.devname = "exynos3-fb.0",
		.table.table_list = axiqos_0xdddd_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.on = true,
	},
	[BTS_IDX_FIMD1] = {
		.id = BTS_FIMD1,
		.name = "fimd1",
		.pa_base = EXYNOS3_PA_BTS_FIMD1,
		.pd_name = "pd-fimd0",
		.clk_name = "lcd",
		.devname = "exynos3-fb.0",
		.table.table_list = axiqos_0xdddd_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.on = true,
	},
	[BTS_IDX_FIMCLITE0] = {
		.id = BTS_FIMCLITE0,
		.name = "fimc0",
		.pa_base = EXYNOS3_PA_BTS_FIMCLITE0,
		.pd_name = "pd-isp",
		.clk_name = "isp0_ctrl",
		.devname = "exynos-fimc-is",
		.table.table_list = axiqos_0xeeee_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.on = true,
	},
	[BTS_IDX_FIMCLITE1] = {
		.id = BTS_FIMCLITE1,
		.name = "fimc1",
		.pa_base = EXYNOS3_PA_BTS_FIMCLITE1,
		.pd_name = "pd-isp",
		.clk_name = "isp0_ctrl",
		.devname = "exynos-fimc-is",
		.table.table_list = axiqos_0xeeee_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.on = true,
	},
	[BTS_IDX_G3D] = {
		.id = BTS_G3D,
		.name = "g3d",
		.pa_base = EXYNOS3_PA_BTS_G3D,
		.pd_name = "pd-g3d",
		.clk_name = "qe.g3d",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_CPU] = {
		.id = BTS_CPU,
		.name = "cpu",
		.pa_base = EXYNOS3_PA_BTS_CPU,
		.pd_name = "pd-cpu",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_FBM_R] = {
		.id = BTS_FBM_R,
		.name = "fbm_r",
		.pa_base = EXYNOS3_PA_FBM_LCD_BASE,
		.pd_name = "pd-fbm_r",
		.clk_name = "lcd",
		.devname = "exynos3-fb.0",
		.table.table_list = fbm_r_table,
		.table.table_num = ARRAY_SIZE(fbm_r_table),
		.on = true,
	},
	[BTS_IDX_TIDE_GDR] = {
		.id = BTS_TIDEMARK,
		.name = "tidemark",
		.pa_base = EXYNOS3_PA_BTS_AXI_GDR,
		.pd_name = "pd-tidemark",
		.table.table_list = tidemark_gdr_table,
		.table.table_num = ARRAY_SIZE(tidemark_gdr_table),
		.on = true,
	},
};

struct bts_scenario {
	const char *name;
	unsigned int ip;
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);


static int exynos_bts_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	switch (event) {
	case PM_POST_SUSPEND:
		bts_initialize("pd-fbm_r", true);
		bts_initialize("pd-tidemark", true);
		bts_initialize("pd-cpu", true);
		return NOTIFY_OK;
		break;
	case PM_SUSPEND_PREPARE:
		bts_initialize("pd-cpu", false);
		if (pr_state.g3d)
			bts_initialize("pd-g3d", false);
		bts_initialize("pd-tidemark", false);
		bts_initialize("pd-fbm_r", false);
		return NOTIFY_OK;
		break;
	}

	return NOTIFY_DONE;
}


static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};


static void set_bts_ip_table(struct bts_info *bts, bool on)
{
	int i;
	unsigned int table_size = bts->table.table_num;
	struct bts_set_table *table = bts->table.table_list;

	if (!on) {
		if (bts->id & BTS_IP) {
			table = disable_table;
			table_size = ARRAY_SIZE(disable_table);
		} else {
			return;
		}
	}

	BTS_DBG("[BTS] bts set: %s\n", bts->name);

	if (bts->clk)
		clk_enable(bts->clk);

	for (i = 0; i < table_size; i++) {
		__raw_writel(table->val, bts->va_base + table->reg);
		BTS_DBG1("[BTS] %x-%x\n", table->reg, table->val);
		table++;
	}

	if (bts->clk)
		clk_disable(bts->clk);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;
	bool fbm_r_flag = false;

	spin_lock(&bts_lock);

	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);

	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			bts->on = on;
			BTS_DBG("[BTS] %s on/off:%d\n", bts->name, bts->on);

			if (bts->id & BTS_TIDEMARK) {
				update_tidemark(on);
			} else if (bts->id & BTS_FBM_R) {
				update_fbm_r(on);
				fbm_r_flag = true;
			} else if (bts->id & BTS_CPU) {
				update_cpu(on);
			} else if (bts->id & BTS_G3D) {
				update_g3d(on);
			}

			if (!(pr_state.tidemark && pr_state.fbm_r) &&
				(bts->id & (BTS_CPU | BTS_G3D)))
				break;

			set_bts_ip_table(bts, on);
		}

	if (fbm_r_flag && pr_state.fbm_r) {
		if (pr_state.cpu)
			set_bts_ip_table(&exynos3_bts[BTS_IDX_CPU], true);

		if (pr_state.g3d)
			set_bts_ip_table(&exynos3_bts[BTS_IDX_G3D], true);
	}


	spin_unlock(&bts_lock);
}

static int __init exynos3_bts_init(void)
{
	int i;
	struct clk *clk;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(exynos3_bts); i++) {
		exynos3_bts[i].va_base
			= ioremap(exynos3_bts[i].pa_base, SZ_4K);

		if (exynos3_bts[i].clk_name) {
			clk = clk_get_sys(exynos3_bts[i].devname,
				exynos3_bts[i].clk_name);
			if (IS_ERR(clk))
				pr_err("failed to get bts clk %s\n",
					exynos3_bts[i].clk_name);
			else
				exynos3_bts[i].clk = clk;
		}

		list_add(&exynos3_bts[i].list, &bts_list);
	}

	drex0_va_base = ioremap(EXYNOS3472_PA_DREX0, SZ_4K);

	bts_drex_init();

	bts_initialize("pd-tidemark", true);
	bts_initialize("pd-fbm_r", true);
	bts_initialize("pd-cpu", true);

	register_pm_notifier(&exynos_bts_notifier);

	return 0;
}
arch_initcall(exynos3_bts_init);
