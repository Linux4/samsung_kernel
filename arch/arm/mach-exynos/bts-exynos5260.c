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

void __iomem *drex0_va_base;
void __iomem *drex1_va_base;

enum bts_index {
	BTS_IDX_FIMDM0 = 0,
	BTS_IDX_FIMDM1,
	BTS_IDX_TVM0,
	BTS_IDX_TVM1,
	BTS_IDX_FIMCLITE_A,
	BTS_IDX_FIMCLITE_B,
	BTS_IDX_MDMA0,
	BTS_IDX_G2D,
	BTS_IDX_JPEG,
	BTS_IDX_EAGLE,
	BTS_IDX_KFC,
	BTS_IDX_MFC0,
	BTS_IDX_MFC1,
	BTS_IDX_G3D,
	BTS_IDX_FIMC_ISP,
	BTS_IDX_FIMC_DRC,
	BTS_IDX_FIMC_SCALER_C,
	BTS_IDX_FIMC_SCALER_P,
	BTS_IDX_FIMC_FD,
	BTS_IDX_FIMC_ISPCX,
	BTS_IDX_GSCL0,
	BTS_IDX_GSCL1,
	BTS_IDX_MSCL0,
	BTS_IDX_MSCL1,
};

enum bts_id {
	BTS_FIMDM0 = (1 << BTS_IDX_FIMDM0),
	BTS_FIMDM1 = (1 << BTS_IDX_FIMDM1),
	BTS_TVM0 = (1 << BTS_IDX_TVM0),
	BTS_TVM1 = (1 << BTS_IDX_TVM1),
	BTS_FIMCLITE_A = (1 << BTS_IDX_FIMCLITE_A),
	BTS_FIMCLITE_B = (1 << BTS_IDX_FIMCLITE_B),
	BTS_MDMA0 = (1 << BTS_IDX_MDMA0),
	BTS_G2D = (1 << BTS_IDX_G2D),
	BTS_JPEG = (1 << BTS_IDX_JPEG),
	BTS_EAGLE = (1 << BTS_IDX_EAGLE),
	BTS_KFC = (1 << BTS_IDX_KFC),
	BTS_MFC0 = (1 << BTS_IDX_MFC0),
	BTS_MFC1 = (1 << BTS_IDX_MFC1),
	BTS_G3D = (1 << BTS_IDX_G3D),
	BTS_FIMC_ISP = (1 << BTS_IDX_FIMC_ISP),
	BTS_FIMC_DRC = (1 << BTS_IDX_FIMC_DRC),
	BTS_FIMC_SCALER_C = (1 << BTS_IDX_FIMC_SCALER_C),
	BTS_FIMC_SCALER_P = (1 << BTS_IDX_FIMC_SCALER_P),
	BTS_FIMC_FD = (1 << BTS_IDX_FIMC_FD),
	BTS_FIMC_ISPCX = (1 << BTS_IDX_FIMC_ISPCX),
	BTS_GSCL0 = (1 << BTS_IDX_GSCL0),
	BTS_GSCL1 = (1 << BTS_IDX_GSCL1),
	BTS_MSCL0 = (1 << BTS_IDX_MSCL0),
	BTS_MSCL1 = (1 << BTS_IDX_MSCL1),
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

#define BTS_TABLE(num)	\
static struct bts_set_table axiqos_##num##_table[] = {	\
	{READ_QOS_CONTROL, 0x0},			\
	{WRITE_QOS_CONTROL, 0x0},			\
	{READ_CHANNEL_PRIORITY, num},			\
	{READ_TOKEN_MAX_VALUE, 0xffdf},			\
	{READ_BW_UPPER_BOUNDARY, 0x18},			\
	{READ_BW_LOWER_BOUNDARY, 0x1},			\
	{READ_INITIAL_TOKEN_VALUE, 0x8},		\
	{WRITE_CHANNEL_PRIORITY, num},			\
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},		\
	{WRITE_BW_UPPER_BOUNDARY, 0x18},		\
	{WRITE_BW_LOWER_BOUNDARY, 0x1},			\
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},		\
	{READ_QOS_CONTROL, 0x1},			\
	{WRITE_QOS_CONTROL, 0x1}			\
}

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
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
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
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{WRITE_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{READ_QOS_MODE, 0x1},
	{WRITE_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
	{WRITE_QOS_CONTROL, 0x7}
};

BTS_TABLE(0x8888);
BTS_TABLE(0xCCCC);

#define BTS_CPU (BTS_KFC | BTS_EAGLE)
#define BTS_FIMD (BTS_FIMDM0 | BTS_FIMDM1)
#define BTS_TV (BTS_TVM0 | BTS_TVM1)
#define BTS_MFC (BTS_MFC0 | BTS_MFC1)
#define BTS_MSCL (BTS_MSCL0 | BTS_MSCL1)
#define BTS_GSCL (BTS_GSCL0 | BTS_GSCL1)

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

static struct bts_info exynos5_bts[] = {
	[BTS_IDX_FIMDM0] = {
		.id = BTS_FIMDM0,
		.name = "fimdm0",
		.pa_base = EXYNOS5260_PA_BTS_FIMDM0,
		.pd_name = "spd-fimd",
		.clk_name = "lcd",
		.devname = "exynos5-fb.1",
		.table.table_list = axiqos_0x8888_table,
		.table.table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = true,
	},
	[BTS_IDX_FIMDM1] = {
		.id = BTS_FIMDM1,
		.name = "fimdm1",
		.pa_base = EXYNOS5260_PA_BTS_FIMDM1,
		.pd_name = "spd-fimd",
		.clk_name = "lcd",
		.devname = "exynos5-fb.1",
		.table.table_list = axiqos_0x8888_table,
		.table.table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = true,
	},
	[BTS_IDX_TVM0] = {
		.id = BTS_TVM0,
		.name = "tvm0",
		.pa_base = EXYNOS5260_PA_BTS_TVM0,
		.pd_name = "spd-tvmixer",
		.clk_name = "mixer",
		.devname = "s5p-mixer",
		.table.table_list = axiqos_0x8888_table,
		.table.table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = true,
	},
	[BTS_IDX_TVM1] = {
		.id = BTS_TVM1,
		.name = "tvm1",
		.pa_base = EXYNOS5260_PA_BTS_TVM1,
		.pd_name = "spd-tvmixer",
		.clk_name = "mixer",
		.devname = "s5p-mixer",
		.table.table_list = axiqos_0x8888_table,
		.table.table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = true,
	},
	[BTS_IDX_FIMCLITE_A] = {
		.id = BTS_FIMCLITE_A,
		.name = "fimclite_a",
		.pa_base = EXYNOS5260_PA_BTS_FIMCLITE_A,
		.pd_name = "spd-flite-a",
		.clk_name = "gscl_flite.0",
		.table.table_list = axiqos_0xCCCC_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xCCCC_table),
		.on = true,
	},
	[BTS_IDX_FIMCLITE_B] = {
		.id = BTS_FIMCLITE_B,
		.name = "fimclite_b",
		.pa_base = EXYNOS5260_PA_BTS_FIMCLITE_B,
		.pd_name = "spd-flite-b",
		.clk_name = "gscl_flite.1",
		.table.table_list = axiqos_0xCCCC_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xCCCC_table),
		.on = true,
	},
	[BTS_IDX_MDMA0] = {
		.id = BTS_MDMA0,
		.name = "mdma0",
		.pa_base = EXYNOS5260_PA_BTS_MDMA0,
		.pd_name = "spd-mdma",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_G2D] = {
		.id = BTS_G2D,
		.name = "g2d",
		.pa_base = EXYNOS5260_PA_BTS_G2D,
		.pd_name = "spd-g2d",
		.clk_name = "fimg2d",
		.devname = "s5p-fimg2d",
		.table.table_list = fbm_l_r_high_table,
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_JPEG] = {
		.id = BTS_JPEG,
		.name = "jpeg",
		.pa_base = EXYNOS5260_PA_BTS_JPEG,
		.pd_name = "spd-jpeg",
		.clk_name = "jpeg-hx",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_EAGLE] = {
		.id = BTS_EAGLE,
		.name = "eagle",
		.pa_base = EXYNOS5260_PA_BTS_EAGLE,
		.pd_name = "pd-eagle",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_KFC] = {
		.id = BTS_KFC,
		.name = "kfc",
		.pa_base = EXYNOS5260_PA_BTS_KFC,
		.pd_name = "pd-kfc",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MFC0] = {
		.id = BTS_MFC0,
		.name = "mfc0",
		.pa_base = EXYNOS5260_PA_BTS_MFC0,
		.pd_name = "pd-mfc",
		.clk_name = "mfc",
		.devname = "s3c-mfc",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MFC1] = {
		.id = BTS_MFC1,
		.name = "mfc1",
		.pa_base = EXYNOS5260_PA_BTS_MFC1,
		.pd_name = "pd-mfc",
		.clk_name = "mfc",
		.devname = "s3c-mfc",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_G3D] = {
		.id = BTS_G3D,
		.name = "g3d",
		.pa_base = EXYNOS5260_PA_BTS_G3D,
		.pd_name = "pd-g3d",
		.clk_name = "g3d",
		.devname = "mali.0",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_FIMC_ISP] = {
		.id = BTS_FIMC_ISP,
		.name = "fimc_isp",
		.pa_base = EXYNOS5260_PA_BTS_FIMC_ISP,
		.pd_name = "pd-isp",
		.clk_name = "isp1",
		.devname = "exynos-fimc-is",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_FIMC_DRC] = {
		.id = BTS_FIMC_DRC,
		.name = "fimc_drc",
		.pa_base = EXYNOS5260_PA_BTS_FIMC_DRC,
		.pd_name = "pd-isp",
		.clk_name = "isp1",
		.devname = "exynos-fimc-is",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_FIMC_SCALER_C] = {
		.id = BTS_FIMC_SCALER_C,
		.name = "fimc_scaler_c",
		.pa_base = EXYNOS5260_PA_BTS_FIMC_SCALER_C,
		.pd_name = "pd-isp",
		.clk_name = "isp1",
		.devname = "exynos-fimc-is",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_FIMC_SCALER_P] = {
		.id = BTS_FIMC_SCALER_P,
		.name = "fimc_scaler_p",
		.pa_base = EXYNOS5260_PA_BTS_FIMC_SCALER_P,
		.pd_name = "pd-isp",
		.clk_name = "isp1",
		.devname = "exynos-fimc-is",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_FIMC_FD] = {
		.id = BTS_FIMC_FD,
		.name = "fimc_fd",
		.pa_base = EXYNOS5260_PA_BTS_FIMC_FD,
		.pd_name = "pd-isp",
		.clk_name = "isp1",
		.devname = "exynos-fimc-is",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_FIMC_ISPCX] = {
		.id = BTS_FIMC_ISPCX,
		.name = "fimc_ispcx",
		.pa_base = EXYNOS5260_PA_BTS_FIMC_ISPCX,
		.pd_name = "pd-isp",
		.clk_name = "isp1",
		.devname = "exynos-fimc-is",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_GSCL0] = {
		.id = BTS_GSCL0,
		.name = "gscl0",
		.pa_base = EXYNOS5260_PA_BTS_GSCL0,
		.pd_name = "spd-gscl0",
		.clk_name = "gscl",
		.devname = "exynos-gsc.0",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_GSCL1] = {
		.id = BTS_GSCL1,
		.name = "gscl1",
		.pa_base = EXYNOS5260_PA_BTS_GSCL1,
		.pd_name = "spd-gscl1",
		.clk_name = "gscl",
		.devname = "exynos-gsc.1",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MSCL0] = {
		.id = BTS_MSCL0,
		.name = "mscl0",
		.pa_base = EXYNOS5260_PA_BTS_MSCL0,
		.pd_name = "spd-mscl0",
		.clk_name = "mscl",
		.devname = "exynos5-scaler.0",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MSCL1] = {
		.id = BTS_MSCL1,
		.name = "mscl0",
		.pa_base = EXYNOS5260_PA_BTS_MSCL1,
		.pd_name = "spd-mscl1",
		.clk_name = "mscl",
		.devname = "exynos5-scaler.1",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
};

struct bts_scenario {
	const char *name;
	unsigned int ip;
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);
static void bts_drex_init(void);

static int exynos_bts_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	switch (event) {
	case PM_POST_SUSPEND:
		bts_drex_init();
		bts_initialize("pd-kfc", true);
		bts_initialize("pd-eagle", true);
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

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;

	spin_lock(&bts_lock);

	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);
	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			bts->on = on;
			BTS_DBG("[BTS] %s on/off:%d\n", bts->name, bts->on);

			if (on)
				set_bts_ip_table(bts);
		}

	spin_unlock(&bts_lock);
}

static void bts_drex_init(void)
{

	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	__raw_writel(0x00000000, drex0_va_base + 0x00D8);
	__raw_writel(0x00800080, drex0_va_base + 0x00C0);
	__raw_writel(0x0fff0fff, drex0_va_base + 0x00A0);
	__raw_writel(0x00000000, drex0_va_base + 0x0100);
	__raw_writel(0x88588858, drex0_va_base + 0x0104);
	__raw_writel(0x20180705, drex0_va_base + 0x0214);
	__raw_writel(0x20180705, drex0_va_base + 0x0224);
	__raw_writel(0x20180705, drex0_va_base + 0x0234);
	__raw_writel(0x20180705, drex0_va_base + 0x0244);
	__raw_writel(0x20180705, drex0_va_base + 0x0218);
	__raw_writel(0x20180705, drex0_va_base + 0x0228);
	__raw_writel(0x20180705, drex0_va_base + 0x0238);
	__raw_writel(0x20180705, drex0_va_base + 0x0248);
	__raw_writel(0x00000000, drex0_va_base + 0x0210);
	__raw_writel(0x00000000, drex0_va_base + 0x0220);
	__raw_writel(0x00000000, drex0_va_base + 0x0230);
	__raw_writel(0x00000000, drex0_va_base + 0x0240);

	__raw_writel(0x00000000, drex1_va_base + 0x00D8);
	__raw_writel(0x00800080, drex1_va_base + 0x00C0);
	__raw_writel(0x0fff0fff, drex1_va_base + 0x00A0);
	__raw_writel(0x00000000, drex1_va_base + 0x0100);
	__raw_writel(0x88588858, drex1_va_base + 0x0104);
	__raw_writel(0x20180705, drex1_va_base + 0x0214);
	__raw_writel(0x20180705, drex1_va_base + 0x0224);
	__raw_writel(0x20180705, drex1_va_base + 0x0234);
	__raw_writel(0x20180705, drex1_va_base + 0x0244);
	__raw_writel(0x20180705, drex1_va_base + 0x0218);
	__raw_writel(0x20180705, drex1_va_base + 0x0228);
	__raw_writel(0x20180705, drex1_va_base + 0x0238);
	__raw_writel(0x20180705, drex1_va_base + 0x0248);
	__raw_writel(0x00000000, drex1_va_base + 0x0210);
	__raw_writel(0x00000000, drex1_va_base + 0x0220);
	__raw_writel(0x00000000, drex1_va_base + 0x0230);
	__raw_writel(0x00000000, drex1_va_base + 0x0240);
}

static int __init exynos5_bts_init(void)
{
	int i;
	struct clk *clk;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(exynos5_bts); i++) {
		exynos5_bts[i].va_base
			= ioremap(exynos5_bts[i].pa_base, SZ_4K);

		if (exynos5_bts[i].clk_name) {
			clk = clk_get_sys(exynos5_bts[i].devname,
				exynos5_bts[i].clk_name);
			if (IS_ERR(clk))
				pr_err("failed to get bts clk %s\n",
					exynos5_bts[i].clk_name);
			else
				exynos5_bts[i].clk = clk;
		}

		list_add(&exynos5_bts[i].list, &bts_list);
	}

	drex0_va_base = ioremap(EXYNOS5_PA_DREXI_0, SZ_4K);
	drex1_va_base = ioremap(EXYNOS5_PA_DREXI_1, SZ_4K);

	bts_drex_init();

	bts_initialize("pd-eagle", true);
	bts_initialize("pd-kfc", true);

	register_pm_notifier(&exynos_bts_notifier);

	return 0;
}
arch_initcall(exynos5_bts_init);
