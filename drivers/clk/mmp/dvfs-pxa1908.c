/*
 *  linux/drivers/clk/mmp/dvfs-pxa1908.c
 *
 *  based on drivers/clk/mmp/dvfs-pxa1u88.c
 *  Copyright (C) 2013 Mrvl, Inc. by Zhoujie Wu <zjwu@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/clk/mmp_sdh_tuning.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/clk/dvfs-dvc.h>
#include <linux/clk/mmpcpdvc.h>

#include <linux/cputype.h>
#include "clk-plat.h"
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

#define VL_MAX	8

#define ACTIVE_RAIL_FLAG	(AFFECT_RAIL_ACTIVE)
#define ACTIVE_M2_RAIL_FLAG	(AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2)
#define ACTIVE_M2_D1P_RAIL_FLAG \
	(AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P)

#define APMU_BASE		0xd4282800
#define GEU_BASE		0xd4201000
#define HWDVC_BASE		0xd4050000

/* Fuse information related register definition */
#define APMU_GEU		0x068
#define MANU_PARA_31_00		0x410
#define MANU_PARA_63_32		0x414
#define MANU_PARA_95_64		0x418
#define BLOCK0_224_255		0x420
/* For chip unique ID */
#define UID_H_32			0x48C
#define UID_L_32			0x4A8
/* For chip DRO and profile */
#define NUM_PROFILES	16
/* used to save fuse parameter */
static unsigned int uimanupara_31_00;
static unsigned int uimanupara_63_32;
static unsigned int uimanupara_95_64;
static unsigned int uiblock0_rsv1;
static unsigned int uiblock7_rsv2;
static unsigned int uiblock7_rsv5;

static unsigned int uiprofilefuses;
static unsigned int uisvcver;
static unsigned int uiprofile;
static unsigned int uisvtdro;
static unsigned int uilvtdro;
static unsigned int uisidd1p05;
static unsigned int uisidd1p30;

static struct comm_fuse_info fuseinfo;

#define CHIP_PROFILE_15   (15)

struct svtrng {
	unsigned int min;
	unsigned int max;
	unsigned int profile;
};

static struct svtrng svtrngtb_z0[] = {
	{0, 300, 15},
	{301, 327, 13},
	{328, 354, 11},
	{355, 367, 9},
	{368, 380, 8},
	{381, 394, 7},
	{395, 407, 6},
	{408, 421, 5},
	{422, 434, 4},
	{435, 448, 3},
	{449, 0xffffffff, 2},
	/* NOTE: rsved profile 1/10/12/14, by default use profile 0 */
};

static u32 convert_svtdro2profile(unsigned int uisvtdro)
{
	unsigned int uiprofile = 0, idx;

	for (idx = 0; idx < ARRAY_SIZE(svtrngtb_z0); idx++) {
		if (uisvtdro >= svtrngtb_z0[idx].min &&
			uisvtdro <= svtrngtb_z0[idx].max) {
			uiprofile = svtrngtb_z0[idx].profile;
			break;
		}
	}

	pr_info("%s uisvtdro[%d]->profile[%d]\n", __func__, uisvtdro, uiprofile);
	return uiprofile;
}

static u32 convert_fuse2profile(unsigned int uiprofilefuses)
{
	unsigned int uitemp = 1, uitemp2 = 1, uiprofile = 0;
	int i;

	for (i = 1; i < NUM_PROFILES; i++) {
		if (uitemp == uiprofilefuses)
			uiprofile = i;
		uitemp |= uitemp2 << (i);
	}

	pr_info("%s fuse[%d]->profile[%d]\n", __func__, uiprofilefuses, uiprofile);
	return uiprofile;
}

static int __init __init_read_droinfo(void)
{
	struct fuse_info arg;
	unsigned int uiallocrev = 0;
	unsigned int uifab = 0;
	unsigned int uirun = 0;
	unsigned int uiwafer = 0;
	unsigned int uix = 0;
	unsigned int uiy = 0;
	unsigned int uiparity = 0;
	unsigned int uifusever = 0;
	unsigned int uiskusetting = 0;

#ifdef CONFIG_ARM64
	smc_get_fuse_info(0xD0002000, (void *)&arg);
#else
	smc_get_fuse_info(0x90002000, (void *)&arg);
#endif
	uimanupara_31_00 = arg.arg0;
	uimanupara_63_32 = arg.arg1;
	uimanupara_95_64 = arg.arg2;
	uiblock0_rsv1 = arg.arg3;
	uiblock7_rsv2 = arg.arg4;
	uiblock7_rsv5 = arg.arg5;
	pr_info("FUSE %x %x %x %x %x %x\n",
		uimanupara_31_00, uimanupara_63_32, uimanupara_95_64,
		uiblock0_rsv1, uiblock7_rsv2, uiblock7_rsv5);

	uiallocrev	= uimanupara_31_00 & 0x7;
	uifab		= (uimanupara_31_00 >>  3) & 0x1f;
	uirun		= ((uimanupara_63_32 & 0x3) << 24)
			| ((uimanupara_31_00 >> 8) & 0xffffff);
	uiwafer		= (uimanupara_63_32 >>  2) & 0x1f;
	uix		= (uimanupara_63_32 >>  7) & 0xff;
	uiy		= (uimanupara_63_32 >> 15) & 0xff;
	uiparity	= (uimanupara_63_32 >> 23) & 0x1;

	/*bit 174 ~ 179 for core speed and UDR voltage*/
	uiskusetting	= (uimanupara_95_64 >> 14) & 0x3f;

	uifusever = (uiblock0_rsv1 >> 6) & 0x3;
	if (1 == uifusever) {
		uisvtdro = ((uimanupara_95_64 & 0x3) << 8) | ((uimanupara_63_32 >> 24) & 0xff);
		uisidd1p05 = (uiblock7_rsv5 >> 9) & 0x3ff;
	} else {
		uisvtdro = (uiblock7_rsv2 >> 22) & 0x3ff;
		uisidd1p05 =  (uiblock7_rsv2 >>  8) & 0x3ff;
		uisidd1p30 =  (uiblock7_rsv5 >>  9) & 0x3ff;
	}
	uilvtdro = (uimanupara_95_64 >>  4) & 0x3ff;
	/* bit 240 ~ 255 for Profile information */

	uisvcver =  (uiblock0_rsv1 >> 13) & 0x7;
	uiprofilefuses = (uiblock0_rsv1 >> 16) & 0x0000FFFF;
	if (uiprofilefuses)
		uiprofile = convert_fuse2profile(uiprofilefuses);
	else
		uiprofile = convert_svtdro2profile(uisvtdro);

	fuseinfo.fab = uifab;
	fuseinfo.lvtdro = uilvtdro;
	fuseinfo.svtdro = uisvtdro;

	fuseinfo.profile = uiprofile;
	fuseinfo.iddq_1050 = uisidd1p05;
	fuseinfo.iddq_1030 = uisidd1p30;
	fuseinfo.skusetting = uiskusetting;
	plat_fill_fuseinfo(&fuseinfo);

	pr_info(" ");
	pr_info("	*********************************\n");
	pr_info("	*	ULT: %08X%08X	*\n",
	       uimanupara_63_32, uimanupara_31_00);
	pr_info("	*********************************\n");
	pr_info("	 ULT decoded below\n");
	pr_info("		alloc_rev[2:0]	= %d\n", uiallocrev);
	pr_info("		fab [ 7: 3]	= %d\n", uifab);
	pr_info("		run [33: 8]	= %d (0x%X)\n", uirun, uirun);
	pr_info("		wafer [38:34]	= %d\n", uiwafer);
	pr_info("		x [46:39]	= %d\n", uix);
	pr_info("		y [54:47]	= %d\n", uiy);
	pr_info("		parity [55:55]	= %d\n", uiparity);
	pr_info("		skusetting [174:179]	= %d\n", uiskusetting);
	pr_info("	*********************************\n");
	pr_info("	*********************************\n");
	pr_info("		LVTDRO [77:68]	= %d\n", uilvtdro);
	pr_info("		SVTDRO [9:0]	= %d\n", uisvtdro);
	pr_info("		Profile	= %d\n", uiprofile);
	pr_info("		SVCver	= %d\n", uisvcver);
	pr_info("		SIDD1P05 = %d\n", uisidd1p05);
	pr_info("	*********************************\n");
	pr_info("\n");

	if (CHIP_PROFILE_15 == uiprofile)
		panic("!!!!!!profile 15 chip not support on ULC1!!!!!!\n");

	return 0;
}

/* components frequency combination */
/* FIXME: adjust according to SVC */
static unsigned long freqs_cmb_1908[VM_RAIL_MAX][VL_MAX] = {
	/* CORE */
	{ 0, 624000, 624000, 832000, 832000, 1057000, 1248000, 1526000 },
	/* DDR */
	{ 0, 312000, 312000, 416000, 528000, 528000, 624000, 667000},
	/* AXI */
	{ 208000, 208000, 208000, 312000, 312000, 312000, 312000, 312000 },
	/* GC3D */
	{ 312000, 312000, 312000, 416000, 624000, 705000, 705000, 832000 },
	/* GC2D */
	{ 156000, 156000, 156000, 312000, 312000, 312000, 416000, 624000 },
	/* GCSHADER */
	{ 312000, 312000, 416000, 416000, 624000, 624000, 705000, 832000 },
	/* GC ACLK */
	{ 0, 312000, 416000, 416000, 416000, 416000, 624000, 624000 },
	/* VPU */
	{ 208000, 312000, 312000, 416000, 416000, 416000, 533000, 533000 },
	/* ISP */
	{ 0, 208000, 208000, 312000, 312000, 312000, 312000, 312000 },
	/* SDH0 dummy dvfs clk*/
	{ DUMMY_VL_TO_KHZ(0), DUMMY_VL_TO_KHZ(1), DUMMY_VL_TO_KHZ(2), DUMMY_VL_TO_KHZ(3),
	  DUMMY_VL_TO_KHZ(4), DUMMY_VL_TO_KHZ(5), DUMMY_VL_TO_KHZ(6), DUMMY_VL_TO_KHZ(7)
	},
	/* SDH1 dummy dvfs clk*/
	{ DUMMY_VL_TO_KHZ(0), DUMMY_VL_TO_KHZ(1), DUMMY_VL_TO_KHZ(2), DUMMY_VL_TO_KHZ(3),
	  DUMMY_VL_TO_KHZ(4), DUMMY_VL_TO_KHZ(5), DUMMY_VL_TO_KHZ(6), DUMMY_VL_TO_KHZ(7)
	},
	/* SDH2 dummy dvfs clk*/
	{ DUMMY_VL_TO_KHZ(0), DUMMY_VL_TO_KHZ(1), DUMMY_VL_TO_KHZ(2), DUMMY_VL_TO_KHZ(3),
	  DUMMY_VL_TO_KHZ(4), DUMMY_VL_TO_KHZ(5), DUMMY_VL_TO_KHZ(6), DUMMY_VL_TO_KHZ(7)
	},
};

/* 8 VLs PMIC setting */
/* FIXME: adjust according to SVC */
static int vm_millivolts_1908_svcsec[][VL_MAX] = {
	{1025, 1075, 1075, 1150, 1150, 1175, 1225, 1300},
	{1025, 1075, 1075, 1150, 1150, 1175, 1225, 1300},
	{963,  975,  988,  988,  988,  1013, 1075, 1125},/* Profile2 */
	{963,  975,  988,  988,  1000, 1025, 1088, 1138},
	{963,  975,  988,  988,  1000, 1038, 1088, 1163},

	{963,  975,  988,  988,  1013, 1050, 1100, 1175},
	{963,  975,  988,  1000, 1013, 1050, 1100, 1200}, /* Profile6 */
	{975,  975,  988,  1013, 1025, 1063, 1113, 1213},
	{975,  975,  988,  1025, 1038, 1075, 1125, 1238},
	{975,  975,  988,  1038, 1050, 1088, 1125, 1250},

	{975,  975,  988,  1038, 1050, 1088, 1125, 1250},
	{988,  1025, 1025, 1075, 1075, 1125, 1163, 1300},/* Profile11 */
	{988,  1025, 1025, 1075, 1075, 1125, 1163, 1300},
	{1000, 1050, 1050, 1100, 1100, 1150, 1200, 1300},/* Profile13 */
	{1000, 1050, 1050, 1100, 1100, 1150, 1200, 1300},
	{1025, 1075, 1075, 1150, 1150, 1175, 1225, 1300},/* Profile15 */
};

#if 0 /* FIX ME (unused) */
static struct cpmsa_dvc_info cpmsa_dvc_info_1908sec = {
	.cpdvcinfo[0] = {416, VL2},
	.cpdvcinfo[1] = {624, VL3},
	.cpdvcinfo[2] = {832, VL6},
	.msadvcvl = VL2,
};
#endif

/*
 * dvfs_rail_component.freqs is inited dynamicly, due to different stepping
 * may have different VL combination
 */
static struct dvfs_rail_component vm_rail_comp_tbl_dvc[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, ACTIVE_M2_D1P_RAIL_FLAG, NULL),
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
	return pm88x_dvc_set_volt(lvl, mv * mV2uV);
}

static int get_pmic_volt(unsigned int lvl)
{
	int uv = 0;

	uv = pm88x_dvc_get_volt(lvl);
	if (uv < 0)
		return uv;
	return DIV_ROUND_UP(uv, mV2uV);
}

static struct dvc_plat_info dvc_pxa1908_info = {
	.comps = vm_rail_comp_tbl_dvc,
	.num_comps = ARRAY_SIZE(vm_rail_comp_tbl_dvc),
	.num_volts = VL_MAX,
	.cp_pmudvc_lvl = VL2,
	.dp_pmudvc_lvl = VL2,
	.set_vccmain_volt = set_pmic_volt,
	.get_vccmain_volt = get_pmic_volt,
	.pmic_maxvl = 8,
	.pmic_rampup_step = 12500,
	/* by default print the debug msg into logbuf */
	.dbglvl = 1,
	.regname = "vccmain",
	/* real measured 8us + 4us, PMIC suggestes 16us for 12.5mV/us */
	.extra_timer_dlyus = 16,
};

int __init setup_pxa1908_dvfs_platinfo(void)
{
	void __iomem *hwdvc_base;
	enum dvfs_comp idx;
	struct dvc_plat_info *plat_info = &dvc_pxa1908_info;
	unsigned long (*freqs_cmb)[VL_MAX];

	__init_read_droinfo();

	/* FIXME: Here may need to identify chip stepping and profile */
	dvc_pxa1908_info.millivolts =
		vm_millivolts_1908_svcsec[uiprofile];
	freqs_cmb = freqs_cmb_1908;
	plat_set_vl_min(0);
	plat_set_vl_max(dvc_pxa1908_info.num_volts);

	//fillcpdvcinfo(&cpmsa_dvc_info_1908sec);

	/* register the platform info into dvfs-dvc.c(hwdvc driver) */
	hwdvc_base = ioremap(HWDVC_BASE, SZ_16K);
	if (hwdvc_base == NULL) {
		pr_err("error to ioremap hwdvc base\n");
		return -ENOMEM;
	}
	plat_info->dvc_reg_base = hwdvc_base;
	for (idx = CORE; idx < VM_RAIL_MAX; idx++)
		plat_info->comps[idx].freqs = freqs_cmb[idx];
	return dvfs_setup_dvcplatinfo(plat_info);
}
