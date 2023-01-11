/*
 *  linux/drivers/clk/mmp/dvfs-pxa1L88.c
 *
 *
 *  Xiaoguang Chen <chenxg@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mfd/88pm80x.h>
#include <linux/clk/dvfs-dvc.h>

#include <linux/cputype.h>

#include "clk-plat.h"

#ifdef CONFIG_TZ_HYPERVISOR
#include <linux/pxa_tzlc.h>
#endif

enum {
	CORE = 0,
	DDR,
	AXI,
	GC,
	GC_SHADER,
	GC2D,
	ISP,
	VPU,
	VM_RAIL_MAX,
};

#define VL_MAX	5

#ifndef CONFIG_TZ_HYPERVISOR
#define APMU_BASE		0xd4282800
#define GEU_BASE		0xd4201000
#define HWDVC_BASE		0xd4050000

#define APMU_GEU	0x068
#define UIMAINFUSE_31_00	0x410
#define UIMAINFUSE_63_32	0x414
#define UIMAINFUSE_95_64	0x418
#define BLOCK0_224_255		0x420
/* For chip UID */
#define UID_H_32	0x48C
#define UID_L_32	0x4A8
#endif

int is_1p5G_chip;

/* components frequency combination */
static unsigned long freqs_cmb_1L88a0[][VL_MAX] = {
	{312000, 624000, 800000, 1248000},	/* CORE */
	{312000, 312000, 312000, 533000},	/* DDR */
	{200000, 200000, 266000, 312000},	/* AXI */
	{0, 0, 624000, 624000},			/* GC */
	{0, 0, 710000, 710000},			/* GC Shader */
	{0, 416000, 624000, 624000},		/* GC2D */
	{312000, 416000, 416000, 416000},	/* ISP */
	{312000, 416000, 533000, 533000},	/* VPU */
};

static unsigned long freqs_cmb_1L88a0c[VM_RAIL_MAX][VL_MAX] = {
	{312000, 624000, 800000, 1248000, 1482000},	/* CORE */
	{312000, 312000, 312000, 533000, 533000},	/* DDR/AXI */
	{156000, 200000, 266000, 312000, 312000},	/* AXI */
	{0, 0, 624000, 624000, 624000},			/* GC */
	{0, 0, 624000, 710000, 710000},			/* GC Shader */
	{0, 416000, 624000, 624000, 624000},		/* GC2D */
	{312000, 416000, 416000, 416000, 416000},	/* ISP */
	{208000, 416000, 416000, 533000, 533000}	/* VPU */
};

static unsigned long freqs_cmb_1L88a0c_umc[VM_RAIL_MAX][VL_MAX] = {
	{312000, 624000, 800000, 1248000, 1482000},	/* CORE */
	{312000, 312000, 312000, 533000, 533000},	/* DDR */
	{156000, 200000, 266000, 312000, 312000},	/* AXI */
	{0, 0, 624000, 624000, 624000},			/* GC */
	{0, 0, 624000, 710000, 710000},			/* GC Shader */
	{0, 416000, 624000, 624000, 624000},		/* GC2D */
	{312000, 416000, 416000, 416000, 416000},	/* ISP */
	{0, 416000, 416000, 533000, 533000},		/* VPU */
};

/* 1L88 SVC table, CP 416M/624M vote VL2 */
static int vm_mv_1L88a0_svc_1p2G[][VL_MAX] = {
	{1113, 1225, 1300, 1375},	/* profile 0 */
	{1050, 1100, 1175, 1175},	/* profile 1 */
	{1050, 1100, 1175, 1175},	/* profile 2 */
	{1050, 1100, 1175, 1175},	/* profile 3 */
	{1050, 1100, 1175, 1175},	/* profile 4 */
	{1050, 1100, 1175, 1213},	/* profile 5 */
	{1050, 1100, 1175, 1250},	/* profile 6 */
	{1100, 1125, 1200, 1288},	/* profile 7 */
	{1100, 1150, 1225, 1325},	/* profile 8 */
	{1100, 1175, 1250, 1350},	/* profile 9 */
	{1100, 1200, 1275, 1375},	/* profile 10 */
	{1113, 1225, 1300, 1375},	/* profile 11 */
};

static int vm_mv_1L88a0_svc_1p25G[][VL_MAX] = {
	{1113, 1225, 1300, 1375},	/* profile 0 */
	{1050, 1100, 1175, 1200},	/* profile 1 */
	{1050, 1100, 1175, 1200},	/* profile 2 */
	{1050, 1100, 1175, 1200},	/* profile 3 */
	{1050, 1100, 1175, 1200},	/* profile 4 */
	{1050, 1100, 1175, 1238},	/* profile 5 */
	{1050, 1100, 1175, 1275},	/* profile 6 */
	{1100, 1125, 1200, 1313},	/* profile 7 */
	{1100, 1150, 1225, 1350},	/* profile 8 */
	{1100, 1175, 1250, 1375},	/* profile 9 */
	{1100, 1200, 1275, 1375},	/* profile 10 */
	{1113, 1225, 1300, 1375},	/* profile 11 */
};

static int vm_mv_1L88a0c_svc_1p2G[][VL_MAX] = {
	{1125, 1225, 1275, 1375},	/* profile 0 */
	{1050, 1100, 1175, 1175},	/* profile 1 */
	{1050, 1100, 1175, 1175},	/* profile 2 */
	{1050, 1100, 1175, 1175},	/* profile 3 */
	{1050, 1100, 1175, 1188},	/* profile 4 */
	{1050, 1100, 1175, 1200},	/* profile 5 */
	{1100, 1113, 1175, 1238},	/* profile 6 */
	{1100, 1138, 1188, 1275},	/* profile 7 */
	{1100, 1150, 1213, 1313},	/* profile 8 */
	{1125, 1175, 1225, 1350},	/* profile 9 */
	{1125, 1200, 1250, 1375},	/* profile 10 */
	{1125, 1225, 1275, 1375},	/* profile 11 */
};

static int vm_mv_1L88a0c_umc_svc_1p2G[][VL_MAX] = {
	{1050, 1225, 1275, 1375},	/* profile 0 */
	{1025, 1100, 1175, 1175},	/* profile 1 */
	{1025, 1100, 1175, 1175},	/* profile 2 */
	{1025, 1100, 1175, 1175},	/* profile 3 */
	{1025, 1100, 1175, 1188},	/* profile 4 */
	{1025, 1100, 1175, 1213},	/* profile 5 */
	{1025, 1100, 1175, 1250},	/* profile 6 */
	{1025, 1138, 1188, 1275},	/* profile 7 */
	{1025, 1150, 1213, 1313},	/* profile 8 */
	{1050, 1175, 1225, 1350},	/* profile 9 */
	{1050, 1200, 1250, 1375},	/* profile 10 */
	{1050, 1225, 1275, 1375},	/* profile 11 */
};

static int vm_mv_1L88a0c_svc_1p25G[][VL_MAX] = {
	{1125, 1225, 1275, 1375},	/* profile 0 */
	{1050, 1100, 1175, 1188},	/* profile 1 */
	{1050, 1100, 1175, 1188},	/* profile 2 */
	{1050, 1100, 1175, 1188},	/* profile 3 */
	{1050, 1100, 1175, 1213},	/* profile 4 */
	{1050, 1100, 1175, 1225},	/* profile 5 */
	{1100, 1113, 1175, 1263},	/* profile 6 */
	{1100, 1138, 1188, 1300},	/* profile 7 */
	{1100, 1150, 1213, 1338},	/* profile 8 */
	{1125, 1175, 1225, 1363},	/* profile 9 */
	{1125, 1200, 1250, 1375},	/* profile 10 */
	{1125, 1225, 1275, 1375},	/* profile 11 */
};

static int vm_mv_1L88a0c_umc_svc_1p25G[][VL_MAX] = {
	{1050, 1225, 1275, 1375},	/* profile 0 */
	{1025, 1100, 1175, 1188},	/* profile 1 */
	{1025, 1100, 1175, 1188},	/* profile 2 */
	{1025, 1100, 1175, 1188},	/* profile 3 */
	{1025, 1100, 1175, 1213},	/* profile 4 */
	{1025, 1100, 1175, 1225},	/* profile 5 */
	{1025, 1100, 1175, 1263},	/* profile 6 */
	{1025, 1138, 1188, 1300},	/* profile 7 */
	{1025, 1150, 1213, 1338},	/* profile 8 */
	{1050, 1175, 1225, 1363},	/* profile 9 */
	{1050, 1200, 1250, 1375},	/* profile 10 */
	{1050, 1225, 1275, 1375},	/* profile 11 */
};

static int vm_mv_1L88a0c_svc_1p5G[][VL_MAX] = {
	{1125, 1225, 1275, 1375, 1375},	/* profile 0 */
	{1050, 1100, 1175, 1238, 1275},	/* profile 1 */
	{1050, 1100, 1175, 1238, 1275},	/* profile 2 */
	{1050, 1100, 1175, 1238, 1300},	/* profile 3 */
	{1050, 1100, 1175, 1263, 1325},	/* profile 4 */
	{1050, 1100, 1175, 1275, 1350},	/* profile 5 */
	{1100, 1113, 1175, 1313, 1375},	/* profile 6 */
	{1100, 1138, 1188, 1350, 1375},	/* profile 7 */
	{1100, 1150, 1213, 1375, 1375},	/* profile 8 */
	{1125, 1175, 1225, 1375, 1375},	/* profile 9 */
	{1125, 1200, 1250, 1375, 1375},	/* profile 10 */
	{1125, 1225, 1275, 1375, 1375},	/* profile 11 */
};

static int vm_mv_1L88a0c_umc_svc_1p5G[][VL_MAX] = {
	{1050, 1225, 1275, 1375, 1375},	/* profile 0 */
	{1025, 1100, 1175, 1238, 1275},	/* profile 1 */
	{1025, 1100, 1175, 1238, 1300},	/* profile 2 */
	{1025, 1100, 1175, 1238, 1325},	/* profile 3 */
	{1025, 1100, 1175, 1263, 1350},	/* profile 4 */
	{1025, 1100, 1175, 1275, 1375},	/* profile 5 */
	{1025, 1100, 1175, 1313, 1375},	/* profile 6 */
	{1025, 1138, 1188, 1350, 1375},	/* profile 7 */
	{1025, 1150, 1213, 1375, 1375},	/* profile 8 */
	{1050, 1175, 1225, 1375, 1375},	/* profile 9 */
	{1050, 1200, 1250, 1375, 1375},	/* profile 10 */
	{1050, 1225, 1275, 1375, 1375},	/* profile 11 */
};

static struct dvfs_rail_component vm_rail_comp_tbl_dvc[] = {
	INIT_DVFS("cpu", true,
		  (AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P),
		  NULL),
#if 0
	INIT_DVFS("cpu", true, (AFFECT_RAIL_ACTIVE), NULL),
#endif
	INIT_DVFS("ddr", true,
		  (AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P),
		  NULL),
	INIT_DVFS("axi", true,
		  (AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2), NULL),
	INIT_DVFS("gc", true,
		  (AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P),
		  NULL),
	INIT_DVFS("gc_shader", true,
		  (AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P),
		  NULL),
	INIT_DVFS("gc2d", true,
		  (AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P),
		  NULL),
	INIT_DVFS("isp-clk", true,
		  (AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P),
		  NULL),
	INIT_DVFS("vpu", true,
		  (AFFECT_RAIL_ACTIVE | AFFECT_RAIL_M2 | AFFECT_RAIL_D1P),
		  NULL),
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

static struct dvc_plat_info dvc_pxa1L88_info = {
	.comps = vm_rail_comp_tbl_dvc,
	.num_comps = ARRAY_SIZE(vm_rail_comp_tbl_dvc),
	.cp_pmudvc_lvl = VL2,
	.dp_pmudvc_lvl = VL2,
	.set_vccmain_volt = set_pmic_volt,
	.get_vccmain_volt = get_pmic_volt,
	.pmic_maxvl = 4,
	.pmic_rampup_step = 12500,
	.dbglvl = 1,
	.regname = "BUCK1",
	.extra_timer_dlyus = 16,
};

#define NUM_PROFILES	11
static unsigned int ui_chip_profile;
static unsigned int ui_chip_foundry;
static u64 ul_id;
static unsigned int pm_dro_status;

static unsigned int convert_fuses_to_profile(unsigned int ui_fuses)
{
	unsigned int ui_profile = 0;
	unsigned int ui_temp = 3, ui_temp2 = 1;
	unsigned int i;

	for (i = 0; i < NUM_PROFILES; i++) {
		ui_temp |= ui_temp2 << (i + 1);
		if (ui_temp == ui_fuses)
			ui_profile = i + 1;
	}

	return ui_profile;
}

static unsigned int get_profile(void)
{
	return ui_chip_profile;
}

unsigned int get_profile_pxa1L88(void)
{
	return ui_chip_profile;
}

unsigned int get_foundry(void)
{
	return ui_chip_foundry;
}

static int __init __init_read_droinfo(void)
{
	void __iomem *apmu_base, *geu_base;

	unsigned int uiMainFuse_63_32 = 0;
	unsigned int uiMainFuse_95_64 = 0;
	unsigned int uiMainFuse_31_00 = 0;
	unsigned int ui_alloc_rev;
	unsigned int ui_fab;
	unsigned int ui_run;
	unsigned int ui_wafer;
	unsigned int uix;
	unsigned int uiy;
	unsigned int ui_parity;
	unsigned int ui_dro_avg;
	unsigned int ui_geu_status;
	unsigned int ui_fuses;
#ifdef CONFIG_TZ_HYPERVISOR
	int ret = -1;
	tzlc_cmd_desc cmd_desc;
	tzlc_handle tzlc_hdl;
#endif

#ifndef CONFIG_TZ_HYPERVISOR
	apmu_base = ioremap(APMU_BASE, SZ_4K);
	if (apmu_base == NULL) {
		pr_err("error to ioremap APMU base\n");
		return -1;
	}

	geu_base = ioremap(GEU_BASE, SZ_4K);
	if (geu_base == NULL) {
		pr_err("error to ioremap GEU base\n");
		return -1;
	}

	/*
	 * Read out DRO value, need enable GEU clock, if already disable,
	 * need enable it firstly
	 */
	ui_geu_status = __raw_readl(apmu_base + APMU_GEU);
	if (!(ui_geu_status & 0x08)) {
		__raw_writel((ui_geu_status | 0x09), apmu_base + APMU_GEU);
		udelay(10);
	}

	uiMainFuse_31_00 = __raw_readl(geu_base + UIMAINFUSE_31_00);
	uiMainFuse_63_32 = __raw_readl(geu_base + UIMAINFUSE_63_32);
	uiMainFuse_95_64 = __raw_readl(geu_base + UIMAINFUSE_95_64);
	ui_fuses = __raw_readl(geu_base + BLOCK0_224_255);
	ul_id = __raw_readl(geu_base + UID_H_32);
	ul_id = (ul_id << 32) | __raw_readl(geu_base + UID_L_32);

	__raw_writel(ui_geu_status, apmu_base + APMU_GEU);
#else
	/*
	 * with TrustZone enabled, fuse related info should be read in
	 * secure world.
	 */
	tzlc_hdl = pxa_tzlc_create_handle();

	memset(&cmd_desc, 0, sizeof(tzlc_cmd_desc));
	cmd_desc.op = TZLC_CMD_READ_MANUFACTURING_BITS;
	ret = pxa_tzlc_cmd_op(tzlc_hdl, &cmd_desc);

	if (ret == 0) {
		ui_fuses = cmd_desc.args[0];
		uiMainFuse_95_64 = cmd_desc.args[1];
		uiMainFuse_63_32 = cmd_desc.args[2];
		uiMainFuse_31_00 = cmd_desc.args[3];
	}

	memset(&cmd_desc, 0, sizeof(tzlc_cmd_desc));
	cmd_desc.op = TZLC_CMD_READ_OEM_UNIQUE_ID;
	ret = pxa_tzlc_cmd_op(tzlc_hdl, &cmd_desc);

	if (ret == 0) {
		ul_id = cmd_desc.args[0];
		ul_id = (ul_id << 32) | cmd_desc.args[1];
	}

	pxa_tzlc_destroy_handle(tzlc_hdl);
#endif

	pr_info("  0x%x   0x%x   0x%x\n",
		uiMainFuse_31_00, uiMainFuse_63_32, uiMainFuse_95_64);

	ui_alloc_rev = uiMainFuse_31_00 & 0x7;
	ui_fab = (uiMainFuse_31_00 >> 3) & 0x1f;
	ui_run = ((uiMainFuse_63_32 & 0x3) << 24)
	    | ((uiMainFuse_31_00 >> 8) & 0xffffff);
	ui_wafer = (uiMainFuse_63_32 >> 2) & 0x1f;
	uix = (uiMainFuse_63_32 >> 7) & 0xff;
	uiy = (uiMainFuse_63_32 >> 15) & 0xff;
	ui_parity = (uiMainFuse_63_32 >> 23) & 0x1;
	ui_dro_avg = (uiMainFuse_95_64 >> 4) & 0x3ff;

	is_1p5G_chip = (uiMainFuse_95_64 >> 26) & 0x3;
	pr_info("1L88 chip max supported freq %s\n",
		(is_1p5G_chip == 1) ? "1.5G" : "1.2G");

	/* bit 237 ~ 239 for foundry information */
	ui_chip_foundry = (ui_fuses >> (237 - 224)) & 0x7;
	/* bit 240 ~ 255 for Profile information */
	ui_fuses = (ui_fuses >> 16) & 0x0000FFFF;
	ui_chip_profile = convert_fuses_to_profile(ui_fuses);

	pm_dro_status = ui_dro_avg;

	pr_info(" ");
	pr_info("	*********************************\n");
	pr_info("	*	ULT: %08X%08X	*\n",
		uiMainFuse_63_32, uiMainFuse_31_00);
	pr_info("	*********************************\n");
	pr_info("	 ULT decoded below\n");
	pr_info("		alloc_rev[2:0]	= %d\n", ui_alloc_rev);
	pr_info("		fab [ 7: 3]	= %d\n", ui_fab);
	pr_info("		run [33: 8]	= %d (0x%X)\n", ui_run, ui_run);
	pr_info("		wafer [38:34]	= %d\n", ui_wafer);
	pr_info("		x [46:39]	= %d\n", uix);
	pr_info("		y [54:47]	= %d\n", uiy);
	pr_info("		parity [55:55]	= %d\n", ui_parity);
	pr_info("	*********************************\n");
	pr_info("	*********************************\n");
	pr_info("		DRO [77:68]	= %d\n", ui_dro_avg);
	pr_info("		Profile	= %d\n", ui_chip_profile);
	pr_info("		Foundry	= %u\n", ui_chip_foundry);
	pr_info("		UID	= %llx\n", ul_id);
	pr_info("	*********************************\n");
	pr_info("\n");

	return ui_dro_avg;
}

int __init setup_pxa1L88_dvfs_platinfo(void)
{
	unsigned int ui_profile = get_profile();
	void __iomem *hwdvc_base;
	unsigned long (*freqs_cmb)[VL_MAX];
	int i;

	__init_read_droinfo();

	hwdvc_base = ioremap(HWDVC_BASE, SZ_16K);
	if (hwdvc_base == NULL) {
		pr_err("error to ioremap hwdvc base\n");
		return -EINVAL;
	}

	dvc_pxa1L88_info.num_volts = 4;

	if (cpu_is_pxa1L88_a0()) {
		if (max_freq > CORE_1p2G)
			dvc_pxa1L88_info.millivolts =
			    vm_mv_1L88a0_svc_1p25G[ui_profile];
		else
			dvc_pxa1L88_info.millivolts =
			    vm_mv_1L88a0_svc_1p2G[ui_profile];
		freqs_cmb = freqs_cmb_1L88a0;
	} else {
		if (!is_1p5G_chip && (max_freq > CORE_1p25G)) {
			WARN_ON("1.2G/1.25G chip: max freq > 1.25G!\n");
			max_freq = CORE_1p2G;
		}
		freqs_cmb = freqs_cmb_1L88a0c;
		if (is_1p5G_chip && (max_freq > CORE_1p25G)) {
			if (get_foundry() == 2) {
				dvc_pxa1L88_info.millivolts =
				    vm_mv_1L88a0c_umc_svc_1p5G[ui_profile];
				freqs_cmb = freqs_cmb_1L88a0c_umc;
			} else {
				dvc_pxa1L88_info.millivolts =
				    vm_mv_1L88a0c_svc_1p5G[ui_profile];
			}
			dvc_pxa1L88_info.num_volts = 5;
		} else if (max_freq > CORE_1p2G) {
			if (get_foundry() == 2) {
				dvc_pxa1L88_info.millivolts =
				    vm_mv_1L88a0c_umc_svc_1p25G[ui_profile];
				freqs_cmb = freqs_cmb_1L88a0c_umc;
			} else {
				dvc_pxa1L88_info.millivolts =
				    vm_mv_1L88a0c_svc_1p25G[ui_profile];
			}
		} else {
			if (get_foundry() == 2) {
				dvc_pxa1L88_info.millivolts =
				    vm_mv_1L88a0c_umc_svc_1p2G[ui_profile];
				freqs_cmb = freqs_cmb_1L88a0c_umc;
			} else {
				dvc_pxa1L88_info.millivolts =
				    vm_mv_1L88a0c_svc_1p2G[ui_profile];
			}
		}
	}
	for (i = 0; i < VM_RAIL_MAX; i++)
		dvc_pxa1L88_info.comps[i].freqs = freqs_cmb[i];
	dvc_pxa1L88_info.dvc_reg_base = hwdvc_base;
	return dvfs_setup_dvcplatinfo(&dvc_pxa1L88_info);
}
