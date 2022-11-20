/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/clk-provider.h>

#include <mach/bts.h>
#include <mach/regs-bts.h>
#include "cal_bts_3475.h"

#define MAX_MIF_IDX		6

#define HD_BW			(960 * 720 * 4 * 60)

#ifdef BTS_DBGGEN
#define BTS_DBG(x...)		pr_err(x)
#else
#define BTS_DBG(x...)		do {} while (0)
#endif

#define DREX_BASE	0x10400000

#define MODEM_QOS_ENABLE_OFFSET	0x0600
#define MODEM_QOS_ENABLE	0x1

#define BTS_SYSREG_DOMAIN (BTS_AUD_QOS | 	/* BUS_P_AUD */\
		BTS_DISP_QOS | 			/* DECON_INT2,DECON_INT1, DECON_INT0 */\
		BTS_FSYS0_QOS | 		/* FSYS_PDMA1, PDMA0, FSYS_USBOTG, FSYS_USBHS */\
		BTS_FSYS1_QOS | 		/* FSYS_MMC2, FSYS_MMC1, FSYS_MMC0 */\
		BTS_IMEM_QOS | 			/* IMEM_RTIC, MEM_SSS1, IMEM_SSS0 */\
		BTS_ISP0_QOS | 			/* FIMC_SCL, FIMC_FD */\
		BTS_ISP1_QOS | 			/* FIMC_ISP, FIMC_BNS_L */\
		BTS_MIF_MODAPIF_QOS | \
		BTS_MIF_CPU_QOS | \
		BTS_MFC_QOS | \
		BTS_JPEG_QOS | \
		BTS_MSCL_BI_QOS | \
		BTS_MSCL_POLY_QOS | \
		BTS_ISP1_M0_MO | \
		BTS_ISP1_S1_MO | \
		BTS_ISP1_S0_MO | \
		BTS_ISP0_M0_MO | \
		BTS_ISP0_S2_MO | \
		BTS_ISP0_S1_MO | \
		BTS_ISP0_S0_MO | \
		BTS_MSCL_M0_MO | \
		BTS_MSCL_S0_MO | \
		BTS_MSCL_S1_MO | \
		BTS_MSCL_S2_MO | \
		BTS_MFC_S0_MO | \
		BTS_MFC_S1_MO | \
		BTS_MFC_M0_MO |\
		BTS_G3D)


enum bts_index {
		BTS_IDX_AUD_QOS,		/* BUS_P_AUD */
		BTS_IDX_DISP_QOS, 		/* DECON_INT2,DECON_INT1, DECON_INT0 */
		BTS_IDX_FSYS0_QOS, 		/* FSYS_PDMA1, PDMA0, FSYS_USBOTG, FSYS_USBHS */
		BTS_IDX_FSYS1_QOS, 		/* FSYS_MMC2, FSYS_MMC1, FSYS_MMC0 */
		BTS_IDX_IMEM_QOS, 		/* IMEM_RTIC, MEM_SSS1, IMEM_SSS0 */
		BTS_IDX_ISP0_QOS, 		/* FIMC_SCL, FIMC_FD */
		BTS_IDX_ISP1_QOS, 		/* FIMC_ISP, FIMC_BNS_L */
		BTS_IDX_MIF_MODAPIF_QOS,
		BTS_IDX_MIF_CPU_QOS,
		BTS_IDX_MFC_QOS,
		BTS_IDX_JPEG_QOS,
		BTS_IDX_MSCL_BI_QOS,
		BTS_IDX_MSCL_POLY_QOS,
		BTS_IDX_ISP1_M0_MO,
		BTS_IDX_ISP1_S1_MO,
		BTS_IDX_ISP1_S0_MO,
		BTS_IDX_ISP0_M0_MO,
		BTS_IDX_ISP0_S2_MO,
		BTS_IDX_ISP0_S1_MO,
		BTS_IDX_ISP0_S0_MO,
		BTS_IDX_MSCL_M0_MO,
		BTS_IDX_MSCL_S0_MO,
		BTS_IDX_MSCL_S1_MO,
		BTS_IDX_MSCL_S2_MO,
		BTS_IDX_MFC_S0_MO,
		BTS_IDX_MFC_S1_MO,
		BTS_IDX_MFC_M0_MO,
		BTS_IDX_G3D,
		BTS_MAX,
};

enum bts_id {
	BTS_AUD_QOS = (1 << BTS_IDX_AUD_QOS),
	BTS_DISP_QOS = (1 << BTS_IDX_DISP_QOS),
	BTS_FSYS0_QOS = (1 << BTS_IDX_FSYS0_QOS),
	BTS_FSYS1_QOS = (1 << BTS_IDX_FSYS1_QOS),
	BTS_IMEM_QOS = (1 << BTS_IDX_IMEM_QOS),
	BTS_ISP0_QOS = (1 << BTS_IDX_ISP0_QOS),
	BTS_ISP1_QOS = (1 << BTS_IDX_ISP1_QOS),
	BTS_MIF_MODAPIF_QOS = (1 << BTS_IDX_MIF_MODAPIF_QOS),
	BTS_MIF_CPU_QOS = (1 << BTS_IDX_MIF_CPU_QOS),
	BTS_MFC_QOS = (1 << BTS_IDX_MFC_QOS),
	BTS_JPEG_QOS = (1 << BTS_IDX_JPEG_QOS),
	BTS_MSCL_BI_QOS = (1 << BTS_IDX_MSCL_BI_QOS),
	BTS_MSCL_POLY_QOS = (1 << BTS_IDX_MSCL_POLY_QOS),
	BTS_ISP1_M0_MO = (1 << BTS_IDX_ISP1_M0_MO),
	BTS_ISP1_S1_MO = (1 << BTS_IDX_ISP1_S1_MO),
	BTS_ISP1_S0_MO = (1 << BTS_IDX_ISP1_S0_MO),
	BTS_ISP0_M0_MO = (1 << BTS_IDX_ISP0_M0_MO),
	BTS_ISP0_S2_MO = (1 << BTS_IDX_ISP0_S2_MO),
	BTS_ISP0_S1_MO = (1 << BTS_IDX_ISP0_S1_MO),
	BTS_ISP0_S0_MO = (1 << BTS_IDX_ISP0_S0_MO),
	BTS_MSCL_M0_MO = (1 << BTS_IDX_MSCL_M0_MO),
	BTS_MSCL_S0_MO = (1 << BTS_IDX_MSCL_S0_MO),
	BTS_MSCL_S1_MO = (1 << BTS_IDX_MSCL_S1_MO),
	BTS_MSCL_S2_MO = (1 << BTS_IDX_MSCL_S2_MO),
	BTS_MFC_S0_MO = (1 << BTS_IDX_MFC_S0_MO),
	BTS_MFC_S1_MO = (1 << BTS_IDX_MFC_S1_MO),
	BTS_MFC_M0_MO = (1 << BTS_IDX_MFC_M0_MO),
	BTS_G3D = (1 << BTS_IDX_G3D),
};

enum exynos_bts_scenario {
	BS_DISABLE,
	BS_DEFAULT,
	BS_DEBUG,
	BS_MAX,
};

enum exynos_bts_function {
	BF_SETQOS,
	BF_SETQOS_BW,
	BF_SETQOS_MO,
	BF_DISABLE,
	BF_NOP,
};

struct bts_table {
	enum exynos_bts_function fn;
	unsigned int priority;
	unsigned int window;
	unsigned int token;
	unsigned int mo;
	struct bts_info *next_bts;
	int prev_scen;
	int next_scen;
};

struct bts_info {
	enum bts_id id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table[BS_MAX];
	const char *pd_name;
	bool on;
	struct list_head list;
	bool enable;
	struct clk_info *ct_ptr;
	enum exynos_bts_scenario cur_scen;
	enum exynos_bts_scenario top_scen;
};

struct bts_scenario {
	const char *name;
	unsigned int ip;
	enum exynos_bts_scenario id;
	struct bts_info *head;
};

struct clk_info {
	const char *clk_name;
	struct clk *clk;
	enum bts_index index;
};

static struct pm_qos_request exynos3475_mif_bts_qos;
static struct pm_qos_request exynos3475_int_bts_qos;
static struct srcu_notifier_head exynos_media_notifier;

static DEFINE_MUTEX(media_mutex);
static DEFINE_SPINLOCK(timeout_mutex);
static unsigned int decon_bw, cam_bw, total_bw;
static void __iomem *base_drex;
static int cur_target_idx;
static unsigned int mif_freq;
static unsigned int num_of_win;
static void __iomem *modem_qos_base;

static int exynos3475_bts_param_table[MAX_MIF_IDX][8] = {
	/* DISP(0)     DISP(1)     DISP(2)     DISP(3)       CP */
	{0x01000200, 0x01000200, 0x01000200, 0x01000200, 0x00800100,},/*MIF_LV0 */
	{0x01000200, 0x01000200, 0x01000200, 0x01000200, 0x00800100,},/*MIF_LV1 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x00800100,},/*MIF_LV2 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x00800100,},/*MIF_LV3 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x00100020,},/*MIF_LV4 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x00100020,},/*MIF_LV5 */
};

static int exynos3475_bts_param_table_isp[MAX_MIF_IDX][8] = {
	/* DISP(0)   DISP(1)     DISP(2)     DISP(3)	 DECON */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x01000200},/*MIF_LV0 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x01000200},/*MIF_LV1 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x00800100},/*MIF_LV2 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x00800100},/*MIF_LV3 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x00800100},/*MIF_LV4 */
	{0x00800100, 0x00800100, 0x00800100, 0x00800100, 0x00800100},/*MIF_LV5 */
};

static struct clk_info clk_table[] = {
	{"aclk_g3d_667_aclk_qe_g3d_aclk", NULL, BTS_IDX_G3D},
	{"pclk_g3d_167_pclk_qe_g3d_pclk", NULL, BTS_IDX_G3D},
};

static struct bts_info exynos3475_bts[] = {
	[BTS_IDX_AUD_QOS] = {
		.id = BTS_AUD_QOS,
		.name = "aud",
		.pa_base = EXYNOS3475_PA_SYSREG_AUD,
		.pd_name = "pd-dispaud",
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_DISP_QOS] = {
		.id = BTS_DISP_QOS,
		.name = "disp",
		.pa_base = EXYNOS3475_PA_SYSREG_DISP,
		.pd_name = "pd-dispaud",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xA,
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},

	[BTS_IDX_FSYS0_QOS] = {
		.id = BTS_FSYS0_QOS,
		.name = "fsys0",
		.pa_base = EXYNOS3475_PA_SYSREG_FSYS0,
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.pd_name = "pd-none",
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_FSYS1_QOS] = {
		.id = BTS_FSYS1_QOS,
		.name = "fsys1",
		.pa_base = EXYNOS3475_PA_SYSREG_FSYS1,
		.pd_name = " pd-none ",
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_IMEM_QOS] = {
		.id = BTS_IMEM_QOS,
		.name = "imem",
		.pa_base = EXYNOS3475_PA_SYSREG_IMEM,
		.pd_name = "pd-none",
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_ISP0_QOS] = {
		.id = BTS_ISP0_QOS,
		.name = "isp0",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP,
		.pd_name = "pd-isp",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xB,
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},

	[BTS_IDX_ISP1_QOS] = {
		.id = BTS_ISP1_QOS,
		.name = "isp1",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP,
		.pd_name = "pd-isp",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xC,
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},

	[BTS_IDX_MIF_MODAPIF_QOS] = {
		.id = BTS_MIF_MODAPIF_QOS,
		.name = "mif_modapif",
		.pa_base = EXYNOS3475_PA_SYSREG_MIF_MODAPIF,
		.pd_name = "pd-none",
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MIF_CPU_QOS] = {
		.id = BTS_MIF_CPU_QOS,
		.name = "mif_cpu",
		.pa_base = EXYNOS3475_PA_SYSREG_MIF_CPU,
		.pd_name = "pd-none",
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MFC_QOS] = {
		.id = BTS_MFC_QOS,
		.name = "mfc",
		.pa_base = EXYNOS3475_PA_SYSREG_MFCMSCL,
		.pd_name = "pd-mfcmscl",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0x8,
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_JPEG_QOS] = {
		.id = BTS_JPEG_QOS,
		.name = "jpeg",
		.pa_base = EXYNOS3475_PA_SYSREG_MFCMSCL,
		.pd_name = "pd-none",
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MSCL_BI_QOS] = {
		.id = BTS_MSCL_BI_QOS,
		.name = "mscl_bi",
		.pa_base = EXYNOS3475_PA_SYSREG_MFCMSCL,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MSCL_POLY_QOS] = {
		.id = BTS_MSCL_POLY_QOS,
		.name = "mscl_poly",
		.pa_base = EXYNOS3475_PA_SYSREG_MFCMSCL,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_SETQOS,
		.table[BS_DISABLE].priority = 0x4,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_ISP1_M0_MO] = {
		.id = BTS_ISP1_M0_MO,
		.name = "isp1_m0",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP1,
		.pd_name = "pd-isp",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_ISP1_S1_MO] = {
		.id = BTS_ISP1_S1_MO,
		.name = "isp1_s1",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP1,
		.pd_name = "pd-isp",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_ISP1_S0_MO] = {
		.id = BTS_ISP1_S0_MO,
		.name = "isp1_s0",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP1,
		.pd_name = "pd-isp",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_ISP0_M0_MO] = {
		.id = BTS_ISP0_M0_MO,
		.name = "isp0_m0",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP0,
		.pd_name = "pd-isp",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_ISP0_S2_MO] = {
		.id = BTS_ISP0_S2_MO,
		.name = "isp0_s2",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP0,
		.pd_name = "pd-isp",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_ISP0_S1_MO] = {
		.id = BTS_ISP0_S1_MO,
		.name = "isp0_s1",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP0,
		.pd_name = "pd-isp",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_ISP0_S0_MO] = {
		.id = BTS_ISP0_S0_MO,
		.name = "isp0_s0",
		.pa_base = EXYNOS3475_PA_SYSREG_ISP0,
		.pd_name = "pd-isp",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MSCL_M0_MO] = {
		.id = BTS_MSCL_M0_MO,
		.name = "mscl_m0",
		.pa_base = EXYNOS3475_PA_SYSREG_MSCL,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MSCL_S0_MO] = {
		.id = BTS_MSCL_S0_MO,
		.name = "mscl_s0",
		.pa_base = EXYNOS3475_PA_SYSREG_MSCL,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MSCL_S1_MO] = {
		.id = BTS_MSCL_S1_MO,
		.name = "mscl_s1",
		.pa_base = EXYNOS3475_PA_SYSREG_MSCL,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MSCL_S2_MO] = {
		.id = BTS_MSCL_S2_MO,
		.name = "mscl_s2",
		.pa_base = EXYNOS3475_PA_SYSREG_MSCL,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MFC_S0_MO] = {
		.id = BTS_MFC_S0_MO,
		.name = "mfc_s0",
		.pa_base = EXYNOS3475_PA_SYSREG_MFC,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MFC_S1_MO] = {
		.id = BTS_MFC_S1_MO,
		.name = "mfc_s1",
		.pa_base = EXYNOS3475_PA_SYSREG_MFC,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_MFC_M0_MO] = {
		.id = BTS_MFC_M0_MO,
		.name = "mfc_m0",
		.pa_base = EXYNOS3475_PA_SYSREG_MFC,
		.pd_name = "pd-mfcmscl",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},

	[BTS_IDX_G3D] = {
		.id = BTS_G3D,
		.name = "g3d",
		.pa_base = EXYNOS3475_PA_SYSREG_G3D,
		.pd_name = "pd-g3d",
		.table[BS_DISABLE].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
};

static struct bts_scenario bts_scen[] = {
	[BS_DISABLE] = {
		.name = "bts_disable",
		.id = BS_DISABLE,
	},
	[BS_DEFAULT] = {
		.name = "bts_default",
		.id = BS_DEFAULT,
	},
	[BS_DEBUG] = {
		.name = "bts_dubugging_ip",
		.id = BS_DEBUG,
	},
	[BS_MAX] = {
		.name = "undefined"
	}
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);

static void is_bts_clk_enabled(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;

	if (ptr) {
		btstable_index = ptr->index;
		do {
			if (!__clk_is_enabled(ptr->clk))
				pr_err("[BTS] CLK is not enabled : %s in %s\n",
						ptr->clk_name,
						bts->name);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_clk_on(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;

	if (ptr) {
		btstable_index = ptr->index;
		do {
			clk_enable(ptr->clk);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_clk_off(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;
	if (ptr) {
		btstable_index = ptr->index;
		do {
			clk_disable(ptr->clk);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_setqos_ip_sysreg(enum bts_id id, void __iomem *va_base,
		unsigned int ar, unsigned int aw)
{
	switch (id) {
	case BTS_AUD_QOS:
		bts_setqos_sysreg(BTS_SYSREG_AUD, va_base, ar, aw);
		break;
	case BTS_DISP_QOS:
		bts_setqos_sysreg(BTS_SYSREG_DISP, va_base, ar, aw);
		break;
	case BTS_FSYS0_QOS:
		bts_setqos_sysreg(BTS_SYSREG_FSYS0, va_base, ar, aw);
		break;
	case BTS_FSYS1_QOS:
		bts_setqos_sysreg(BTS_SYSREG_FSYS1, va_base, ar, aw);
		break;
	case BTS_IMEM_QOS:
		bts_setqos_sysreg(BTS_SYSREG_IMEM, va_base, ar, aw);
		break;
	case BTS_ISP0_QOS:
		bts_setqos_sysreg(BTS_SYSREG_FIMC_SCL, va_base, ar, aw);
		bts_setqos_sysreg(BTS_SYSREG_FIMC_FD, va_base, ar, aw);
		break;
	case BTS_ISP1_QOS:
		bts_setqos_sysreg(BTS_SYSREG_FIMC_ISP, va_base, ar, aw);
		bts_setqos_sysreg(BTS_SYSREG_FIMC_BNS_L, va_base, ar, aw);
		break;
	case BTS_MIF_MODAPIF_QOS:
		bts_setqos_sysreg(BTS_SYSREG_MIF_MODAPIF, va_base, ar, aw);
		break;
	case BTS_MIF_CPU_QOS:
		bts_setqos_sysreg(BTS_SYSREG_MIF_CPU, va_base, ar, aw);
		break;
	case BTS_MFC_QOS:
		bts_setqos_sysreg(BTS_SYSREG_MFC, va_base, ar, aw);
		break;
	case BTS_JPEG_QOS:
		bts_setqos_sysreg(BTS_SYSREG_JPEG, va_base, ar, aw);
		break;
	case BTS_MSCL_BI_QOS:
	case BTS_MSCL_POLY_QOS:
		bts_setqos_sysreg(BTS_SYSREG_MSCL, va_base, ar, aw);
		break;
	default:
		break;
	}
}

static void bts_setmo_ip_sysreg(enum bts_id id, void __iomem *va_base,
					unsigned int ar, unsigned int aw)
{
	switch (id) {
	case BTS_ISP1_M0_MO:
		bts_setmo_sysreg(BTS_SYSREG_ISP1_M0, va_base, ar, aw);
		break;
	case BTS_ISP1_S1_MO:
		bts_setmo_sysreg(BTS_SYSREG_ISP1_S1, va_base, ar, aw);
		break;
	case BTS_ISP1_S0_MO:
		bts_setmo_sysreg(BTS_SYSREG_ISP1_S0, va_base, ar, aw);
		break;
	case BTS_ISP0_M0_MO:
		bts_setmo_sysreg(BTS_SYSREG_ISP0_M0, va_base, ar, aw);
		break;
	case BTS_ISP0_S0_MO:
		bts_setmo_sysreg(BTS_SYSREG_ISP0_S0, va_base, ar, aw);
		break;
	case BTS_ISP0_S1_MO:
		bts_setmo_sysreg(BTS_SYSREG_ISP0_S1, va_base, ar, aw);
		break;
	case BTS_ISP0_S2_MO:
		bts_setmo_sysreg(BTS_SYSREG_ISP0_S2, va_base, ar, aw);
		break;
	case	BTS_MSCL_M0_MO:
	case	BTS_MSCL_S0_MO:
	case	BTS_MSCL_S1_MO:
	case	BTS_MSCL_S2_MO:
	case	BTS_MFC_S0_MO:
	case	BTS_MFC_S1_MO:
	case	BTS_MFC_M0_MO:
		break;
	default:
		break;
	}
}

static void bts_set_ip_table(enum exynos_bts_scenario scen,
		struct bts_info *bts)
{
	enum exynos_bts_function fn = bts->table[scen].fn;

	is_bts_clk_enabled(bts);
	BTS_DBG("[BTS] %s on:%d bts scen: [%s]->[%s]\n", bts->name, bts->on,
			bts_scen[bts->cur_scen].name, bts_scen[scen].name);

	switch (fn) {
	case BF_SETQOS:
		if (bts->id & BTS_SYSREG_DOMAIN)
			bts_setqos_ip_sysreg(bts->id, bts->va_base, \
						bts->table[scen].priority, \
						bts->table[scen].priority);
		else if (bts->id == BTS_G3D)
			bts_setqos(bts->va_base, bts->table[scen].priority);
		break;
	case BF_SETQOS_BW:
		if (bts->id == BTS_G3D)
			bts_setqos_bw(bts->va_base, bts->table[scen].priority, \
				bts->table[scen].window, bts->table[scen].token);
		break;
	case BF_SETQOS_MO:
		if (bts->id & BTS_SYSREG_DOMAIN)
			bts_setmo_ip_sysreg(bts->id, bts->va_base, \
						bts->table[scen].mo, \
						bts->table[scen].mo);
		else if (bts->id == BTS_G3D)
			bts_setqos_mo(bts->va_base, bts->table[scen].priority, \
					bts->table[scen].mo);
		break;
	case BF_DISABLE:
		if (bts->id == BTS_G3D)
			bts_disable(bts->va_base);
		break;
	case BF_NOP:
		break;
	}

	bts->cur_scen = scen;
}

static enum exynos_bts_scenario bts_get_scen(struct bts_info *bts)
{
	enum exynos_bts_scenario scen;

	scen = BS_DEFAULT;

	return scen;
}


static void bts_add_scen(enum exynos_bts_scenario scen, struct bts_info *bts)
{
	struct bts_info *first = bts;
	int next = 0;
	int prev = 0;

	if (!bts)
		return;

	BTS_DBG("[bts %s] scen %s off\n",
			bts->name, bts_scen[scen].name);

	do {
		if (bts->enable) {
			if (bts->table[scen].next_scen == 0) {
				if (scen >= bts->top_scen) {
					bts->table[scen].prev_scen = bts->top_scen;
					bts->table[bts->top_scen].next_scen = scen;
					bts->top_scen = scen;
					bts->table[scen].next_scen = -1;

					if (bts->on)
						bts_set_ip_table(bts->top_scen, bts);

				} else {
					for (prev = bts->top_scen; prev > scen; prev = bts->table[prev].prev_scen)
						next = prev;

					bts->table[scen].prev_scen = bts->table[next].prev_scen;
					bts->table[scen].next_scen = bts->table[prev].next_scen;
					bts->table[next].prev_scen = scen;
					bts->table[prev].next_scen = scen;
				}
			}
		}

		bts = bts->table[scen].next_bts;

	} while (bts && bts != first);
}

static void bts_del_scen(enum exynos_bts_scenario scen, struct bts_info *bts)
{
	struct bts_info *first = bts;
	int next = 0;
	int prev = 0;

	if (!bts)
		return;

	BTS_DBG("[bts %s] scen %s off\n",
			bts->name, bts_scen[scen].name);

	do {
		if (bts->enable) {
			if (bts->table[scen].next_scen != 0) {
				if (scen == bts->top_scen) {
					prev = bts->table[scen].prev_scen;
					bts->top_scen = prev;
					bts->table[prev].next_scen = -1;
					bts->table[scen].next_scen = 0;
					bts->table[scen].prev_scen = 0;

					if (bts->on)
						bts_set_ip_table(prev, bts);
				} else if (scen < bts->top_scen) {
					prev = bts->table[scen].prev_scen;
					next = bts->table[scen].next_scen;

					bts->table[next].prev_scen = bts->table[scen].prev_scen;
					bts->table[prev].next_scen = bts->table[scen].next_scen;

					bts->table[scen].prev_scen = 0;
					bts->table[scen].next_scen = 0;

				} else {
					BTS_DBG("%s scenario couldn't exist above top_scen\n", bts_scen[scen].name);
				}
			}

		}

		bts = bts->table[scen].next_bts;

	} while (bts && bts != first);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;
	enum exynos_bts_scenario scen = BS_DISABLE;

	spin_lock(&bts_lock);

	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strncmp(bts->pd_name, pd_name, strlen(pd_name))) {
			BTS_DBG("[BTS] %s on/off:%d->%d\n", bts->name, bts->on, on);

			if (!bts->on && on)
				bts_clk_on(bts);
			else if (bts->on && !on)
				bts_clk_off(bts);

			if (!bts->enable) {
				bts->on = on;
				continue;
			}

			scen = bts_get_scen(bts);
			if (on) {
				bts_add_scen(scen, bts);
				if (!bts->on) {
					bts->on = true;
					bts_set_ip_table(bts->top_scen, bts);
				}
			} else {
				if (bts->on)
					bts->on = false;

				bts_del_scen(scen, bts);
			}
		}

	spin_unlock(&bts_lock);
}

static void scen_chaining(enum exynos_bts_scenario scen)
{
	struct bts_info *prev = NULL;
	struct bts_info *first = NULL;
	struct bts_info *bts;

	if (bts_scen[scen].ip) {
		list_for_each_entry(bts, &bts_list, list) {
			if (bts_scen[scen].ip & bts->id) {
				if (!first)
					first = bts;
				if (prev)
					prev->table[scen].next_bts = bts;

				prev = bts;
			}
		}

		if (prev)
			prev->table[scen].next_bts = first;

		bts_scen[scen].head = first;
	}
}

static int exynos3475_qos_status_open_show(struct seq_file *buf, void *d)
{
	unsigned int i;
	unsigned int val_r, val_w;

	spin_lock(&bts_lock);

	for (i = 0; i < ARRAY_SIZE(exynos3475_bts); i++) {
		seq_printf(buf, "bts[%d] %s : ", i, exynos3475_bts[i].name);
		if (exynos3475_bts[i].on && exynos3475_bts[i].enable) {
			if (exynos3475_bts[i].ct_ptr)
				bts_clk_on(exynos3475_bts + i);
			/* axi qoscontrol */
			switch (exynos3475_bts[i].id) {
			case BTS_AUD_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + DISPAUD_XIU_DISPAUD_QOS_CON);
				seq_printf(buf, "%s: qos_aw: 0x%x, qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 16) & 0xF, (val_r >> 12) & 0xF);
				break;
			case BTS_DISP_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + DISPAUD_XIU_DISPAUD_QOS_CON);
				seq_printf(buf, "%s(%x): qos_ar0: 0x%x qos_ar1: 0x%x qos_ar2: 0x%x\n",
						exynos3475_bts[i].name,
						__raw_readl(exynos3475_bts[i].va_base + DISPAUD_QOS_SEL) & 0x1,
						val_r & 0xF, (val_r >> 4) & 0xF, (val_r >> 8) & 0xF);
				break;
			case BTS_FSYS0_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + FSYS_QOS_CON0);
				seq_printf(buf, "%s: qos_aw0: 0x%x qos_aw1: 0x%x qos_aw2: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 4) & 0xF, (val_r >> 12) & 0xF, (val_r >> 28) & 0xF);
				seq_printf(buf, "%s: qos_ar0: 0x%x qos_ar1: 0x%x qos_ar2: 0x%x\n",
						exynos3475_bts[i].name,
						val_r & 0xF, (val_r >> 8) & 0xF, (val_r >> 24) & 0xF);
				break;
			case BTS_FSYS1_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + FSYS_QOS_CON1);
				seq_printf(buf, "%s: qos_aw0: 0x%x qos_aw1: 0x%x qos_aw2: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 4) & 0xF, (val_r >> 12) & 0xF, (val_r >> 20) & 0xF);
				seq_printf(buf, "%s: qos_ar0: 0x%x qos_ar1: 0x%x qos_ar2: 0x%x\n",
						exynos3475_bts[i].name,
						val_r & 0xF, (val_r >> 8) & 0xF, (val_r >> 16) & 0xF);
				break;
			case BTS_IMEM_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + IMEM_QOS_CON);
				seq_printf(buf, "%s: qos_aw0: 0x%x qos_aw1: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 4) & 0xF, (val_r >> 12) & 0xF);
				seq_printf(buf, "%s: qos_ar0: 0x%x qos_ar1: 0x%x qos_ar2: 0x%x\n",
						exynos3475_bts[i].name,
						val_r & 0xF, (val_r >> 8) & 0xF, (val_r >> 16) & 0xF);
				break;
			case BTS_ISP0_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_QOS_CON0);
				seq_printf(buf, "%s-%s: qos_aw: 0x%x\n",
						exynos3475_bts[i].name,
						"SCL" , (val_r >> 28) & 0xF);
				seq_printf(buf, "%s-%s: qos_aw: 0x%x qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						"FD", (val_r >> 20) & 0xF, (val_r >> 16) & 0xF);
				break;
			case BTS_ISP1_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_QOS_CON0);
				seq_printf(buf, "%s-%s: qos_aw:0x%x qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						"ISP", (val_r >> 12) & 0xF, (val_r >> 8) & 0xF);
				seq_printf(buf, "%s-%s: qos_aw: 0x%x\n",
						exynos3475_bts[i].name,
						"BNS_L", (val_r >> 4) & 0xF);
				break;
			case BTS_MIF_MODAPIF_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + MIF_MODAPIF_QOS_CON);
				seq_printf(buf, "%s(%x): qos_aw: 0x%x qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						val_r & 0x1, (val_r >> 8) & 0xF, (val_r >> 4) & 0xF);
				break;
			case BTS_MIF_CPU_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + MIF_CPU_QOS_CON);
				seq_printf(buf, "%s: qos_aw: 0x%x qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 8) & 0xF, (val_r >> 4) & 0xF);
				break;
			case BTS_MFC_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + MFCMSCL_QOS_CON);
				seq_printf(buf, "%s: qos_aw: 0x%x qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 28) & 0xF, (val_r >> 24) & 0xF);
				break;
			case BTS_JPEG_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + MFCMSCL_QOS_CON);
				seq_printf(buf, "%s: qos_aw: 0x%x qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 20) & 0xF, (val_r >> 16) & 0xF);
				break;
			case BTS_MSCL_BI_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + MFCMSCL_QOS_CON);
				seq_printf(buf, "%s: qos_aw: 0x%x qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 12) & 0xF, (val_r >> 8) & 0xF);
				break;
			case BTS_MSCL_POLY_QOS:
				val_r = __raw_readl(exynos3475_bts[i].va_base + MFCMSCL_QOS_CON);
				seq_printf(buf, "%s: qos_aw: 0x%x qos_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_r >> 4) & 0xF, (val_r) & 0xF);
				break;
			case BTS_ISP1_M0_MO:
				val_w = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AW_AC_TARGET_CON);
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AR_AC_TARGET_CON);
				seq_printf(buf, "%s: mo_aw: 0x%x mo_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_w >> 16) & 0x3F, (val_r >> 16) & 0x3F);
				break;
			case BTS_ISP1_S1_MO:
				val_w = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AW_AC_TARGET_CON);
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AR_AC_TARGET_CON);
				seq_printf(buf, "%s: mo_aw: 0x%x mo_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_w >> 8) & 0x3F, (val_r >> 8) & 0x3F);
				break;
			case BTS_ISP1_S0_MO:
				val_w = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AW_AC_TARGET_CON);
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AR_AC_TARGET_CON);
				seq_printf(buf, "%s: mo_aw: 0x%x mo_ar: 0x%x\n",
						exynos3475_bts[i].name,
						val_w & 0x3F, val_r & 0x3F);
				break;
			case BTS_ISP0_M0_MO:
				val_w = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AW_AC_TARGET_CON);
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AR_AC_TARGET_CON);
				seq_printf(buf, "%s: mo_aw: 0x%x mo_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_w >> 24) & 0x3F, (val_r >> 24) & 0x3F);
				break;
			case BTS_ISP0_S2_MO:
				val_w = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AW_AC_TARGET_CON);
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AR_AC_TARGET_CON);
				seq_printf(buf, "%s: mo_aw: 0x%x mo_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_w >> 16) & 0x3F, (val_r >> 16) & 0x3F);
				break;
			case BTS_ISP0_S1_MO:
				val_w = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AW_AC_TARGET_CON);
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AR_AC_TARGET_CON);
				seq_printf(buf, "%s: mo_aw: 0x%x mo_ar: 0x%x\n",
						exynos3475_bts[i].name,
						(val_w >> 8) & 0x3F, (val_r >> 8) & 0x3F);
				break;
			case BTS_ISP0_S0_MO:
				val_w = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AW_AC_TARGET_CON);
				val_r = __raw_readl(exynos3475_bts[i].va_base + ISP_XIU_ISP1_AR_AC_TARGET_CON);
				seq_printf(buf, "%s: mo_aw: 0x%x mo_ar: 0x%x\n",
						exynos3475_bts[i].name,
						val_w & 0x3F, val_r & 0x3F);
				break;
			default:
				break;
			}

			if (exynos3475_bts[i].ct_ptr)
				bts_clk_off(exynos3475_bts + i);
		} else {
			seq_puts(buf, "off\n");
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static int exynos3475_qos_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos3475_qos_status_open_show, inode->i_private);
}

static const struct file_operations debug_qos_status_fops = {
	.open		= exynos3475_qos_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int exynos3475_bw_status_open_show(struct seq_file *buf, void *d)
{
	mutex_lock(&media_mutex);

	seq_printf(buf, "bts bandwidth (total %u) : decon %u, cam %u, mif_freq %u\n",
			total_bw, decon_bw, cam_bw, mif_freq);

	mutex_unlock(&media_mutex);

	return 0;
}

static int exynos3475_bw_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos3475_bw_status_open_show, inode->i_private);
}

static const struct file_operations debug_bw_status_fops = {
	.open		= exynos3475_bw_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int debug_enable_get(void *data, unsigned long long *val)
{
	struct bts_info *first = bts_scen[BS_DEBUG].head;
	struct bts_info *bts = bts_scen[BS_DEBUG].head;
	int cnt = 0;

	if (first) {
		do {
			pr_info("%s, ", bts->name);
			cnt++;
			bts = bts->table[BS_DEBUG].next_bts;
		} while (bts && bts != first);
	}
	if (first && first->top_scen == BS_DEBUG)
		pr_info("is on\n");
	else
		pr_info("is off\n");
	*val = cnt;

	return 0;
}

static int debug_enable_set(void *data, unsigned long long val)
{
	struct bts_info *first = bts_scen[BS_DEBUG].head;
	struct bts_info *bts = bts_scen[BS_DEBUG].head;

	if (first) {
		do {
			pr_info("%s, ", bts->name);

			bts = bts->table[BS_DEBUG].next_bts;
		} while (bts && bts != first);
	}

	spin_lock(&bts_lock);

	if (val) {
		bts_add_scen(BS_DEBUG, bts_scen[BS_DEBUG].head);
		pr_info("is on\n");
	} else {
		bts_del_scen(BS_DEBUG, bts_scen[BS_DEBUG].head);
		pr_info("is off\n");
	}

	spin_unlock(&bts_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_enable_fops, debug_enable_get, debug_enable_set, "%llx\n");

static void bts_status_print(void)
{
	pr_info("0 : disable debug ip\n");
	pr_info("1 : BF_SETQOS\n");
	pr_info("2 : BF_SETQOS_BW\n");
	pr_info("3 : BF_SETQOS_MO\n");
}

static int debug_ip_enable_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	if (bts->table[BS_DEBUG].next_scen) {
		switch (bts->table[BS_DEBUG].fn) {
		case BF_SETQOS:
			*val = 1;
			break;
		case BF_SETQOS_BW:
			*val = 2;
			break;
		case BF_SETQOS_MO:
			*val = 3;
			break;
		default:
			*val = 4;
			break;
		}
	} else {
		*val = 0;
	}

	bts_status_print();

	return 0;
}

static int debug_ip_enable_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	if (val) {
		bts_scen[BS_DEBUG].ip |= bts->id;

		scen_chaining(BS_DEBUG);

		switch (val) {
		case 1:
			bts->table[BS_DEBUG].fn = BF_SETQOS;
			break;
		case 2:
			bts->table[BS_DEBUG].fn = BF_SETQOS_BW;
			break;
		case 3:
			bts->table[BS_DEBUG].fn = BF_SETQOS_MO;
			break;
		default:
			break;
		}

		bts_add_scen(BS_DEBUG, bts);

		pr_info("%s on 0x%x\n", bts->name, bts_scen[BS_DEBUG].ip);
	} else {
		bts->table[BS_DEBUG].next_bts = NULL;
		bts_del_scen(BS_DEBUG, bts);

		bts_scen[BS_DEBUG].ip &= ~bts->id;
		scen_chaining(BS_DEBUG);

		pr_info("%s off 0x%x\n", bts->name, bts_scen[BS_DEBUG].ip);
	}

	spin_unlock(&bts_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_enable_fops, debug_ip_enable_get, debug_ip_enable_set, "%llx\n");

static int debug_ip_mo_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	*val = bts->table[BS_DEBUG].mo;

	spin_unlock(&bts_lock);

	return 0;
}

static int debug_ip_mo_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	bts->table[BS_DEBUG].mo = val;
	if (bts->top_scen == BS_DEBUG) {
		if (bts->on)
			bts_set_ip_table(BS_DEBUG, bts);
	}
	pr_info("Debug mo set %s : mo 0x%x\n", bts->name, bts->table[BS_DEBUG].mo);

	spin_unlock(&bts_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_mo_fops, debug_ip_mo_get, debug_ip_mo_set, "%llx\n");

static int debug_ip_token_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	if (bts->id == BTS_G3D)
		*val = bts->table[BS_DEBUG].token;
	else
		*val = 0;

	spin_unlock(&bts_lock);

	return 0;
}

static int debug_ip_token_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	if (bts->id == BTS_G3D) {
		bts->table[BS_DEBUG].token = val;
		if (bts->top_scen == BS_DEBUG) {
			if (bts->on && bts->table[BS_DEBUG].window)
				bts_set_ip_table(BS_DEBUG, bts);
		}
		pr_info("Debug bw set %s : priority 0x%x window 0x%x token 0x%x\n",
			bts->name, bts->table[BS_DEBUG].priority,
			bts->table[BS_DEBUG].window,
			bts->table[BS_DEBUG].token);
	}

	spin_unlock(&bts_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_token_fops, debug_ip_token_get, debug_ip_token_set, "%llx\n");

static int debug_ip_window_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	if (bts->id == BTS_G3D)
		*val = bts->table[BS_DEBUG].window;
	else
		*val = 0;

	spin_unlock(&bts_lock);

	return 0;
}

static int debug_ip_window_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	if (bts->id == BTS_G3D) {
		bts->table[BS_DEBUG].window = val;
		if (bts->top_scen == BS_DEBUG) {
			if (bts->on && bts->table[BS_DEBUG].token)
				bts_set_ip_table(BS_DEBUG, bts);
		}
		pr_info("Debug bw set %s : priority 0x%x window 0x%x token 0x%x\n",
			bts->name, bts->table[BS_DEBUG].priority,
			bts->table[BS_DEBUG].window,
			bts->table[BS_DEBUG].token);
	}

	spin_unlock(&bts_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_window_fops, debug_ip_window_get, debug_ip_window_set, "%llx\n");

static int debug_ip_qos_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	*val = bts->table[BS_DEBUG].priority;

	spin_unlock(&bts_lock);

	return 0;
}

static int debug_ip_qos_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	bts->table[BS_DEBUG].priority = val;
	if (bts->top_scen == BS_DEBUG) {
		if (bts->on)
			bts_setqos(bts->va_base, bts->table[BS_DEBUG].priority);
	}
	pr_info("Debug qos set %s : 0x%x\n", bts->name, bts->table[BS_DEBUG].priority);

	spin_unlock(&bts_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_qos_fops, debug_ip_qos_get, debug_ip_qos_set, "%llx\n");

void bts_debugfs(void)
{
	struct bts_info *bts;
	struct dentry *den;
	struct dentry *subden;

	den = debugfs_create_dir("bts_dbg", NULL);
	debugfs_create_file("qos_status", 0440,	den, NULL, &debug_qos_status_fops);
	debugfs_create_file("bw_status", 0440,	den, NULL, &debug_bw_status_fops);
	debugfs_create_file("enable", 0440,	den, NULL, &debug_enable_fops);

	den = debugfs_create_dir("bts", den);
	list_for_each_entry(bts, &bts_list, list) {
		subden = debugfs_create_dir(bts->name, den);
		debugfs_create_file("qos", 0644, subden, bts, &debug_ip_qos_fops);
		debugfs_create_file("token", 0644, subden, bts, &debug_ip_token_fops);
		debugfs_create_file("window", 0644, subden, bts, &debug_ip_window_fops);
		debugfs_create_file("mo", 0644, subden, bts, &debug_ip_mo_fops);
		debugfs_create_file("enable", 0644, subden, bts, &debug_ip_enable_fops);
	}
}

static void bts_drex_init(void __iomem *base)
{

	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	__raw_writel(0x00000000, base + QOS_TIMEOUT_0xF);
	__raw_writel(0x00200020, base + QOS_TIMEOUT_0xE);
	__raw_writel(0x00400080, base + QOS_TIMEOUT_0xD);
	__raw_writel(0x00400080, base + QOS_TIMEOUT_0xC);
	__raw_writel(0x00800100, base + QOS_TIMEOUT_0xB);
	__raw_writel(0x01000200, base + QOS_TIMEOUT_0xA);
	__raw_writel(0x07FF0FFF, base + QOS_TIMEOUT_0x8);
	__raw_writel(0x00000000, base + BRB_CON);
	__raw_writel(0x88888888, base + BRB_THRESHOLD);
}

static void exynos_bts_modem_qos_enable(void)
{
	unsigned int temp;

	temp = __raw_readl(modem_qos_base + MODEM_QOS_ENABLE_OFFSET);
	temp = temp | MODEM_QOS_ENABLE;
	__raw_writel(temp, modem_qos_base + MODEM_QOS_ENABLE_OFFSET);
}

static int exynos_bts_notifier_event(struct notifier_block *this,
		unsigned long event,
		void *ptr)
{
	switch (event) {
	case PM_POST_SUSPEND:

		bts_drex_init(base_drex);
		bts_initialize("pd-none", true);
		exynos_bts_modem_qos_enable();

		return NOTIFY_OK;
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

#define NUM_OF_CP	4
#define NUM_OF_DECON	4

int exynos_update_bts_param(int target_idx, int work)
{
	if (!work)
		return 0;

	spin_lock(&timeout_mutex);

	cur_target_idx = target_idx;

	if (decon_bw && !cam_bw) {
		__raw_writel(exynos3475_bts_param_table[target_idx][num_of_win], base_drex + QOS_TIMEOUT_0xA);
		__raw_writel(exynos3475_bts_param_table[target_idx][NUM_OF_CP], base_drex + QOS_TIMEOUT_0xD);

		__raw_writel(0x00400080, base_drex + QOS_TIMEOUT_0xC);
		__raw_writel(0x00800100, base_drex + QOS_TIMEOUT_0xB);
	} else if (decon_bw && cam_bw) {
		__raw_writel(exynos3475_bts_param_table_isp[target_idx][num_of_win], base_drex + QOS_TIMEOUT_0xB);
		__raw_writel(exynos3475_bts_param_table_isp[target_idx][NUM_OF_DECON], base_drex + QOS_TIMEOUT_0xA);
		__raw_writel(0x00400080, base_drex + QOS_TIMEOUT_0xD);
		__raw_writel(0x00400080, base_drex + QOS_TIMEOUT_0xC);
	}

	spin_unlock(&timeout_mutex);

	return 0;
}

static int exynos3475_bts_notify(unsigned long freq)
{
	BUG_ON(irqs_disabled());

	return srcu_notifier_call_chain(&exynos_media_notifier, freq, NULL);
}

int exynos_bts_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&exynos_media_notifier, nb);
}

int exynos_bts_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&exynos_media_notifier, nb);
}

void exynos3475_init_bts_ioremap(void)
{
	base_drex = ioremap(DREX_BASE, SZ_4K);
	modem_qos_base = ioremap(EXYNOS3475_PA_SYSREG_MIF_MODAPIF, SZ_4K);

	pm_qos_add_request(&exynos3475_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&exynos3475_int_bts_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
}

void exynos_update_media_scenario(enum bts_media_type media_type,
		unsigned int bw, int bw_type)
{
	mutex_lock(&media_mutex);

	switch (media_type) {
	case TYPE_DECON_INT:
		decon_bw = bw;
		num_of_win = decon_bw / HD_BW;
		break;
	case TYPE_CAM:
		cam_bw = bw;
		break;
	default:
		pr_err("DEVFREQ(MIF) : unsupportd media_type - %u", media_type);
		break;
	}

	total_bw = decon_bw + cam_bw;

	/* MIF minimum frequency calculation as per BTS guide */
	if (cam_bw) {
		if (decon_bw <=  2 * HD_BW)
			mif_freq = 413000;
		else
			mif_freq = 559000;
	} else {
		if (decon_bw <= 2 * HD_BW)
			mif_freq = 200000;
		else
			mif_freq = 338000;
	}

	exynos3475_bts_notify(mif_freq);

	BTS_DBG("[BTS BW] total: %u, decon %u, cam %u\n",
			total_bw, decon_bw, cam_bw);
	BTS_DBG("[BTS FREQ] mif_freq: %u\n", mif_freq);

	if (pm_qos_request_active(&exynos3475_mif_bts_qos))
		pm_qos_update_request(&exynos3475_mif_bts_qos, mif_freq);

	mutex_unlock(&media_mutex);
}

static int __init exynos3475_bts_init(void)
{
	int i;
	int ret;
	enum bts_index btstable_index = BTS_MAX - 1;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(clk_table); i++) {
		if (btstable_index != clk_table[i].index) {
			btstable_index = clk_table[i].index;
			exynos3475_bts[btstable_index].ct_ptr = clk_table + i;
		}
		clk_table[i].clk = clk_get(NULL, clk_table[i].clk_name);

		if (IS_ERR(clk_table[i].clk)) {
			BTS_DBG("failed to get bts clk %s\n",
					clk_table[i].clk_name);
			exynos3475_bts[btstable_index].ct_ptr = NULL;
		} else {
			ret = clk_prepare(clk_table[i].clk);
			if (ret) {
				pr_err("[BTS] failed to prepare bts clk %s\n",
						clk_table[i].clk_name);
				for (; i >= 0; i--)
					clk_put(clk_table[i].clk);
				return ret;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(exynos3475_bts); i++) {
		exynos3475_bts[i].va_base = ioremap(exynos3475_bts[i].pa_base, SZ_8K);

		list_add(&exynos3475_bts[i].list, &bts_list);
	}

	for (i = BS_DEFAULT + 1; i < BS_MAX; i++)
		scen_chaining(i);

	exynos3475_init_bts_ioremap();
	bts_drex_init(base_drex);

	register_pm_notifier(&exynos_bts_notifier);

	bts_debugfs();

	srcu_init_notifier_head(&exynos_media_notifier);

	bts_initialize("pd-none", true);
	exynos_bts_modem_qos_enable();

	return 0;
}
arch_initcall(exynos3475_bts_init);
