/*
 *  linux/drivers/clk/mmp/dvfs-pxa1u88.c
 *
 *  based on drivers/clk/mmp/dvfs-pxa1L88.c
 *  Copyright (C) 2013 Mrvl, Inc. by Zhoujie Wu <zjwu@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/clk/mmp_sdh_tuning.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/88pm80x.h>
#include <linux/clk/dvfs-dvc.h>

#include <linux/cputype.h>

#include "clk.h"

/* components that affect the vmin */
enum dvfs_comp {
	CORE = 0,
	DDR,
	AXI,
	GC3D,
	GC2D,
	GC_SHADER,
	GCACLK,
	VPU,
	ISP,
	SDH0,
	SDH1,
	SDH2,
	VM_RAIL_MAX,
};

#define VL_MAX	4

#define ACTIVE_RAIL_FLAG	(AFFECT_RAIL_ACTIVE)
#define ACTIVE_M2_RAIL_FLAG	(AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2)
#define ACTIVE_M2_D1P_RAIL_FLAG \
	(AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P)

#define APMU_BASE		0xd4282800
#define GEU_BASE		0xd4292800
#define HWDVC_BASE		0xd4050000

/* Fuse information related register definition */
#define APMU_GEU		0x068
#define MANU_PARA_31_00		0x110
#define MANU_PARA_63_32		0x114
#define MANU_PARA_95_64		0x118
#define BLOCK0_224_255		0x11c
#define BLOCK4_PARA_31_00	0x2B4
#define BLOCK4_PARA_63_32	0x2A4
/* For chip unique ID */
#define UID_H_32		0x18c
#define UID_L_32		0x1a8
/* For chip DRO and profile */
#define NUM_PROFILES	11
/* used to save fuse parameter */
static unsigned int uimanupara_31_00;
static unsigned int uimanupara_63_32;
static unsigned int uimanupara_95_64;
static unsigned int uifuses;
static unsigned int uiprofile;
static unsigned int uidro;
unsigned int uisvtdro;
static unsigned int ui_Block4_TrKeyEcc_63_32;
static unsigned int uiSIDD1_05, uiSIDD1_30;

struct svtrng {
	unsigned int min;
	unsigned int max;
	unsigned int profile;
};

static struct svtrng svtrngtb_z0[] = {
	{0, 295, 15},
	{296, 308, 14},
	{309, 320, 13},
	{321, 332, 12},
	{333, 344, 11},
	{345, 357, 10},
	{358, 370, 9},
	{371, 382, 8},
	{383, 394, 7},
	{395, 407, 6},
	{408, 420, 5},
	{421, 432, 4},
	{433, 444, 3},
	{445, 0xffffffff, 2},
	/* NOTE: rsved profile 1, by default use profile 0 */
};

static unsigned int convert_svtdro2profile(unsigned int uisvtdro)
{
	unsigned int uiprofile = 0, idx;

	for (idx = 0; idx < ARRAY_SIZE(svtrngtb_z0); idx++) {
		if (uisvtdro >= svtrngtb_z0[idx].min &&
			uisvtdro <= svtrngtb_z0[idx].max) {
			uiprofile = svtrngtb_z0[idx].profile;
			break;
		}
	}

	return uiprofile;
}

static int __maybe_unused __init __init_read_droinfo(void)
{
	void __iomem *apmu_base, *geu_base;
	unsigned int uigeustatus, uiblk431_00;

#ifndef CONFIG_TZ_HYPERVISOR
	apmu_base = ioremap(APMU_BASE, SZ_4K);
	if (apmu_base == NULL) {
		pr_err("error to ioremap APMU base\n");
		return -EINVAL;
	}

	geu_base = ioremap(GEU_BASE, SZ_4K);
	if (geu_base == NULL) {
		pr_err("error to ioremap GEU base\n");
		return -EINVAL;
	}

	/*
	 * Read out DRO value, need enable GEU clock, if already disable,
	 * need enable it firstly
	 */
	uigeustatus = __raw_readl(apmu_base + APMU_GEU);
	if (!(uigeustatus & 0x30)) {
		__raw_writel((uigeustatus | 0x30), apmu_base + APMU_GEU);
		udelay(10);
	}

	uimanupara_31_00 = __raw_readl(geu_base + MANU_PARA_31_00);
	uimanupara_63_32 = __raw_readl(geu_base + MANU_PARA_63_32);
	uimanupara_95_64 = __raw_readl(geu_base + MANU_PARA_95_64);
	uiblk431_00 = __raw_readl(geu_base + BLOCK4_PARA_31_00);
	uifuses = __raw_readl(geu_base + BLOCK0_224_255);
	ui_Block4_TrKeyEcc_63_32 = __raw_readl(geu_base + BLOCK4_PARA_63_32);

	__raw_writel(uigeustatus, apmu_base + APMU_GEU);
#else
	/* FIXME: add TZ support here */
#endif

	uidro		= (uimanupara_95_64 >>  4) & 0x3ff;
	/* bit 240 ~ 255 for Profile information */
	uifuses = (uifuses >> 16) & 0x0000FFFF;
	uiSIDD1_05 = (ui_Block4_TrKeyEcc_63_32 >>  8) & 0x3ff;
	uiSIDD1_30 = (ui_Block4_TrKeyEcc_63_32 >> 18) & 0x3ff;

	uisvtdro = uiblk431_00 & 0x3ff;
	uiprofile  = convert_svtdro2profile(uisvtdro);
	pr_info("uiprofile = %d\n", uiprofile);

	return 0;
}

/* components frequency combination */
/* FIXME: DDR800M uses VL1 after verified */
static unsigned long freqs_cmb_1u88z1[VM_RAIL_MAX][VL_MAX] = {
	{ 624000, 1248000, 1526000, 1803000 },   /* CORE */
	{ 533000, 533000, 800000, 800000 },     /* DDR */
	{ 208000, 312000, 312000, 312000 },     /* AXI */
	{ 0, 624000, 705000, 705000 },		/* GC3D */
	{ 0, 312000, 416000, 416000 },		/* GC2D */
	{ 0, 624000, 705000, 705000 },		/* GC Shader */
	{ 0, 416000, 416000, 416000 },		/* GC ACLK */
	{ 416000, 533000, 533000, 533000 },	/* VPU */
	{ 0, 312000, 416000, 416000 },		/* ISP */
	{ DUMMY_VL_TO_KHZ(0), DUMMY_VL_TO_KHZ(1),
	  DUMMY_VL_TO_KHZ(2), DUMMY_VL_TO_KHZ(3)
	}, /* SDH0 */
	{ DUMMY_VL_TO_KHZ(0), DUMMY_VL_TO_KHZ(1),
	  DUMMY_VL_TO_KHZ(2), DUMMY_VL_TO_KHZ(3)
	}, /* SDH1 */
	{ DUMMY_VL_TO_KHZ(0), DUMMY_VL_TO_KHZ(1),
	  DUMMY_VL_TO_KHZ(2), DUMMY_VL_TO_KHZ(3)
	}, /* SDH2 */
};

/* 1u88 z1 SVC table, CP/MSA votes VL0 by default */
/* FIXME: VL0 > 1V due to 533M DDR freq-chg vmin concern */
static int vm_millivolts_1u88z1_svc[][VL_MAX] = {
	{1063, 1250, 1288, 1300},	/* profile 0, no fuse info */

	{1063, 1250, 1288, 1300},	/* profile 1, no svc, use worst */
	{988, 1100, 1125, 1200},	/* profile 2 */
	{988, 1100, 1125, 1200},	/* profile 3 */
	{988, 1100, 1125, 1200},	/* profile 4 */
	{988, 1100, 1138, 1213},	/* profile 5 */

	{988, 1100, 1150, 1225},	/* profile 6 */
	{988, 1100, 1163, 1250},	/* profile 7 */
	{1000, 1100, 1175, 1263},	/* profile 8 */
	{1013, 1100, 1200, 1275},	/* profile 9 */

	{1013, 1150, 1225, 1300},	/* profile 10, same as profile 11 */
	{1013, 1150, 1225, 1300},	/* profile 11 */

	{1013, 1200, 1250, 1300},	/* profile 12, same as profile 13 */
	{1013, 1200, 1250, 1300},	/* profile 13 */

	{1038, 1250, 1288, 1300},	/* profile 14, same as profile 15 */
	{1038, 1250, 1288, 1300},	/* profile 15 */
};

/*
 * dvfs_rail_component.freqs is inited dynamicly, due to different stepping
 * may have different VL combination
 */
static struct dvfs_rail_component vm_rail_comp_tbl_dvc[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, ACTIVE_RAIL_FLAG, NULL),
	INIT_DVFS("ddr", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("axi", true, ACTIVE_M2_RAIL_FLAG, NULL),
	INIT_DVFS("gc3d_clk", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("gc2d_clk", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("gcsh_clk", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("gcbus_clk", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("vpufunc_clk", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("isp_pipe_clk", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("sdh0_dummy", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("sdh1_dummy", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
	INIT_DVFS("sdh2_dummy", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
};

static int set_pmic_volt(unsigned int lvl, unsigned int mv)
{
	return pm8xx_dvc_setvolt(PM800_ID_BUCK1, lvl, mv * mV2uV);
}

static int get_pmic_volt(unsigned int lvl)
{
	int uv = 0, ret = 0;

	ret = pm8xx_dvc_getvolt(PM800_ID_BUCK1, lvl, &uv);
	if (ret < 0)
		return ret;
	return DIV_ROUND_UP(uv, mV2uV);
}

static struct dvc_plat_info dvc_pxa1u88_info = {
	.comps = vm_rail_comp_tbl_dvc,
	.num_comps = ARRAY_SIZE(vm_rail_comp_tbl_dvc),
	.num_volts = VL_MAX,
	/* FIXME: CP/MSA VL may need to be adjusted according to SVC */
	.cp_pmudvc_lvl = VL0,
	.dp_pmudvc_lvl = VL0,
	.set_vccmain_volt = set_pmic_volt,
	.get_vccmain_volt = get_pmic_volt,
	.pmic_maxvl = 4,
	.pmic_rampup_step = 12500,
	/* by default print the debug msg into logbuf */
	.dbglvl = 0,
	.regname = "BUCK1",
	/* real measured 8us + 4us, PMIC suggestes 16us for 12.5mV/us */
	.extra_timer_dlyus = 16,
};

int __init setup_pxa1u88_dvfs_platinfo(void)
{
	void __iomem *hwdvc_base;
	enum dvfs_comp idx;
	struct dvc_plat_info *plat_info = &dvc_pxa1u88_info;
	unsigned long (*freqs_cmb)[VL_MAX];

	__init_read_droinfo();

	/* FIXME: Here may need to identify chip stepping and profile */
	dvc_pxa1u88_info.millivolts =
		vm_millivolts_1u88z1_svc[uiprofile];
	freqs_cmb = freqs_cmb_1u88z1;
	plat_set_vl_min(0);
	plat_set_vl_max(dvc_pxa1u88_info.num_volts);

	/* register the platform info into dvfs-dvc.c(hwdvc driver) */
	hwdvc_base = ioremap(HWDVC_BASE, SZ_16K);
	if (hwdvc_base == NULL) {
		pr_err("error to ioremap hwdvc base\n");
		return -EINVAL;
	}
	plat_info->dvc_reg_base = hwdvc_base;
	for (idx = CORE; idx < VM_RAIL_MAX; idx++)
		plat_info->comps[idx].freqs = freqs_cmb[idx];
	return dvfs_setup_dvcplatinfo(&dvc_pxa1u88_info);
}
