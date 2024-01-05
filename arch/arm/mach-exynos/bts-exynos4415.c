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

enum bts_index {
	BTS_IDX_G2D = 0,
	BTS_IDX_G3D,
	BTS_IDX_CPU,
	BTS_IDX_LCD,
	BTS_IDX_TIDE_GDR,
	BTS_IDX_TIDE_GDL
};

enum bts_id {
	BTS_G2D = (1 << BTS_IDX_G2D),
	BTS_G3D = (1 << BTS_IDX_G3D),
	BTS_CPU = (1 << BTS_IDX_CPU),
	BTS_LCD = (1 << BTS_IDX_LCD),
	BTS_GDR = (1 << BTS_IDX_TIDE_GDR),
	BTS_GDL = (1 << BTS_IDX_TIDE_GDL),
};

enum bts_clock_index {
	BTS_CLOCK_G3D = 0,
	BTS_CLOCK_MMC,
	BTS_CLOCK_USB,
	BTS_CLOCK_DIS1,
	BTS_CLOCK_MAX,
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
	struct list_head scen_list;
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

static struct bts_set_table fbm_l_r_high_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x0},
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
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x8},
	{WRITE_CHANNEL_PRIORITY, 0x0000},
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
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x8},
	{READ_QOS_MODE, 0x1},
	{WRITE_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
	{WRITE_QOS_CONTROL, 0x7}
};

static struct bts_set_table fbm_disable_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
};

static struct bts_set_table fbm_lcd_table[] = {
	{0x0000, 0x6},
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
	{0x00f0, 0x0},
};

static struct bts_set_table tidemark_gdr_table[] = {
	{0x0400, 0xA},
	{0x0404, 0x6},
};

static struct bts_set_table tidemark_gdl_table[] = {
	{0x0400, 0x6},
	{0x0404, 0x2},
};

#define BTS_TV (BTS_TVM0 | BTS_TVM1)
#define BTS_FIMD (BTS_FIMDM0 | BTS_FIMDM1)

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
	[BTS_IDX_G2D] = {
		.id = BTS_G2D,
		.name = "g2d",
		.pa_base = EXYNOS5_PA_BTS_G2D,
		.pd_name = "pd-g2d",
		.clk_name = "fimg2d",
		.devname = "s5p-fimg2d",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_G3D] = {
		.id = BTS_G3D,
		.name = "g3d",
		.pa_base = EXYNOS4_PA_BTS_G3D,
		.pd_name = "pd-g3d",
		.clk_name = "sclk_g3d",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_CPU] = {
		.id = BTS_CPU,
		.name = "cpu",
		.pa_base = EXYNOS4_PA_BTS_CPU,
		.pd_name = "pd-cpu",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_LCD] = {
		.id = BTS_LCD,
		.name = "lcd",
		.pa_base = EXYNOS4_FBM_LCD_BASE,
		.pd_name = "pd-lcd",
		.clk_name = "fimd",
		.devname = "exynos4-fb.1",
		.table.table_list = fbm_lcd_table,
		.table.table_num = ARRAY_SIZE(fbm_lcd_table),
		.on = true,
	},
	[BTS_IDX_TIDE_GDR] = {
		.id = BTS_GDR,
		.name = "gdr",
		.pa_base = EXYNOS4_TIDEMARK_GDR,
		.pd_name = "pd-gdr",
		.table.table_list = tidemark_gdr_table,
		.table.table_num = ARRAY_SIZE(tidemark_gdr_table),
		.on = true,
	},
	[BTS_IDX_TIDE_GDL] = {
		.id = BTS_GDL,
		.name = "gdl",
		.pa_base = EXYNOS4_TIDEMARK_GDL,
		.pd_name = "pd-gdl",
		.table.table_list = tidemark_gdl_table,
		.table.table_num = ARRAY_SIZE(tidemark_gdl_table),
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
		bts_initialize("pd-gdr", true);
		bts_initialize("pd-gdl", true);
		return NOTIFY_OK;
		break;
	}

	return NOTIFY_DONE;
}


static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};


static void set_bts_ip_table(struct bts_info *bts)
{
	int i;
	unsigned int table_size = bts->table.table_num;
	struct bts_set_table *table = bts->table.table_list;

	if (!strcmp(bts->name,"g3d") && (bts->on == false)) {
		table = fbm_disable_table;
		table_size = ARRAY_SIZE(fbm_disable_table);
	}

	if (!strcmp(bts->name,"cpu") && (bts->on == false)) {
		table = fbm_disable_table;
		table_size = ARRAY_SIZE(fbm_disable_table);
	}

	if (!strcmp(bts->name,"g2d") && (bts->on == false)) {
		table = fbm_disable_table;
		table_size = ARRAY_SIZE(fbm_disable_table);
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

	spin_lock(&bts_lock);

	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);
	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			bts->on = on;
			BTS_DBG("[BTS] %s on/off:%d\n", bts->name, bts->on);

			if(!strcmp(pd_name,"pd-g3d")) {
				if (exynos4_bts[BTS_IDX_LCD].on == true)
					set_bts_ip_table(bts);
			} else if (!strcmp(pd_name,"pd-lcd") && bts->on == false) {
				exynos4_bts[BTS_IDX_CPU].on = false;
				exynos4_bts[BTS_IDX_G3D].on = false;
				exynos4_bts[BTS_IDX_G2D].on = false;
				set_bts_ip_table(&exynos4_bts[BTS_IDX_CPU]);
				set_bts_ip_table(&exynos4_bts[BTS_IDX_G3D]);
				set_bts_ip_table(&exynos4_bts[BTS_IDX_G2D]);
			} else if (!strcmp(pd_name,"pd-lcd") && bts->on == true) {
				set_bts_ip_table(bts);
				exynos4_bts[BTS_IDX_CPU].on = true;
				set_bts_ip_table(&exynos4_bts[BTS_IDX_CPU]);
				exynos4_bts[BTS_IDX_G2D].on = true;
				set_bts_ip_table(&exynos4_bts[BTS_IDX_G2D]);
			} else if (on)
				set_bts_ip_table(bts);
		}

	spin_unlock(&bts_lock);
}

void set_mfc_scen(bool enable)
{
	unsigned int val;

	spin_lock(&bts_lock);

	val = __raw_readl(exynos4_bts[BTS_IDX_TIDE_GDL].va_base + 0x404);
	if (enable)
		val |= (1 << 3);
	else
		val &= ~(1 << 3);
	__raw_writel(val, exynos4_bts[BTS_IDX_TIDE_GDL].va_base + 0x404);

	val = __raw_readl(exynos4_bts[BTS_IDX_TIDE_GDR].va_base + 0x404);
	if (enable)
		val |= (1 << 4);
	else
		val &= ~(1 << 4);
	__raw_writel(val, exynos4_bts[BTS_IDX_TIDE_GDR].va_base + 0x404);

	spin_unlock(&bts_lock);
}

static int __init exynos4_bts_init(void)
{
	int i;
	struct clk *clk;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(exynos4_bts); i++) {
		exynos4_bts[i].va_base
			= ioremap(exynos4_bts[i].pa_base, SZ_4K);

		if (exynos4_bts[i].clk_name) {
			clk = clk_get_sys(exynos4_bts[i].devname,
				exynos4_bts[i].clk_name);
			if (IS_ERR(clk))
				pr_err("failed to get bts clk %s\n",
					exynos4_bts[i].clk_name);
			else
				exynos4_bts[i].clk = clk;
		}

		list_add(&exynos4_bts[i].list, &bts_list);
	}

	register_pm_notifier(&exynos_bts_notifier);

	return 0;
}
arch_initcall(exynos4_bts_init);
