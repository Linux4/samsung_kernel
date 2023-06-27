/*
 *  linux/arch/arm/mach-mmp/dvfs-pxa988.c
 *
 *  based on arch/arm/mach-tegra/tegra2_dvfs.c
 *	 Copyright (C) 2010 Google, Inc. by Colin Cross <ccross@google.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#if defined(CONFIG_MFD_88PM822)
#include <linux/mfd/88pm822.h>
#elif defined(CONFIG_MFD_88PM800)
#include <linux/mfd/88pm80x.h>
#endif
#include <asm/io.h>
#include <asm/delay.h>
#include <mach/regs-mpmu.h>
#include <mach/cputype.h>
#include <mach/clock-pxa988.h>
#include <mach/dvfs.h>
#include <mach/features.h>
#include <plat/clock.h>
#include <plat/debugfs.h>
#include "common.h"

#if defined(CONFIG_MFD_88PM822)
#define BUCK1_AP_ACTIVE		PM822_ID_BUCK1_AP_ACTIVE
#define BUCK1_AP_LPM		PM822_ID_BUCK1_AP_LPM
#define BUCK1_APSUB_IDLE	PM822_ID_BUCK1_APSUB_IDLE
#define BUCK1_APSUB_SLEEP	PM822_ID_BUCK1_APSUB_SLEEP
#elif defined(CONFIG_MFD_88PM800)
#define BUCK1_AP_ACTIVE		PM800_ID_BUCK1_AP_ACTIVE
#define BUCK1_AP_LPM		PM800_ID_BUCK1_AP_LPM
#define BUCK1_APSUB_IDLE	PM800_ID_BUCK1_APSUB_IDLE
#define BUCK1_APSUB_SLEEP	PM800_ID_BUCK1_APSUB_SLEEP
#else /* CONFIG_MFD_D2199 */
#define BUCK1_AP_ACTIVE		D2199_ID_BUCK1_AP_ACTIVE
#define BUCK1_AP_LPM		D2199_ID_BUCK1_AP_LPM
#define BUCK1_APSUB_IDLE	D2199_ID_BUCK1_APSUB_IDLE
#define BUCK1_APSUB_SLEEP	D2199_ID_BUCK1_APSUB_SLEEP
#endif

#if defined(CONFIG_D2199_DVC)
#include <linux/d2199/core.h>
#include <linux/d2199/pmic.h>
#endif
#ifdef CONFIG_MACH_WILCOX_CMCC
#define BOARD_ID_CMCC_REV05 (0x7)
#endif
/*
 * for PMU DVC, the GPIO pins are switched or not
 * 1 - switched
 * 0 - not switched
 * set it as 1 by default
 */
#if defined(CONFIG_MFD_D2199)
static int dvc_pin_switch = 0;
#else
static int dvc_pin_switch = 1;
#endif

int dvc_flag = 1;
EXPORT_SYMBOL(dvc_flag);

#if defined(CONFIG_MFD_D2199)	// DLG
#define D2199_DVFS
#define dlg_trace(fmt, ...) printk(KERN_INFO "[WS][DVFS][%s]-"fmt, __func__, ##__VA_ARGS__)

extern int d2199_dvfs_get_voltage(int reg_addr);
extern int d2199_dvfs_set_voltage(int reg_addr, unsigned int reg_val);

#else
#define dlg_trace(fmt, ...) 	do { } while(0);
#endif
static int __init dvc_flag_setup(char *str)
{
	int n;

	dlg_trace("str : %s\n", str);

	if (!get_option(&str, &n))
		return 0;
	dvc_flag = n;
	return 1;
}
__setup("dvc_flag=", dvc_flag_setup);

enum {
	CORE = 0,
	DDR_AXI,
	GC,
#ifdef CONFIG_CPU_PXA1088
	GC2D,
	ISP,
#endif
	VPU,
	VM_RAIL_MAX,
};

/*
 * NOTES: DVC is used to change voltage, currently use max
 * voltage lvl 4, could add if has requirement
 */
enum {
	VL0 = 0,
	VL1,
	VL2,
	VL3,
	VL_MAX,
};

#define KHZ_TO_HZ	1000
#define PROFILE_NUM	9

#define MAX_RAIL_NUM	4
struct dvfs_rail_component {
	const char *clk_name;
	bool auto_dvfs;
	const int *millivolts;
	struct dvfs_rail *dvfs_rail;
	unsigned int freqs_mult;
	unsigned long *freqs;
	/* used to save related clk_node and dvfs ptr */
	struct clk *clk_node;
	struct dvfs *dvfs;
};

enum {
	DVC_AP,
	DVC_CP,
	DVC_DP,
	DVC_APSUB,
	DVC_CHIP,
	DVC_END,
};

enum {
	LEVEL_START,
	LEVEL0,
	LEVEL1,
	LEVEL2,
	LEVEL3,
	LEVEL4,
	LEVEL5,
	LEVEL6,
	LEVEL7,
	LEVEL0_1, /* LEVEL0_x is used to replace level 0 */
	LEVEL0_2,
	LEVEL_END,
};

enum {
	AP_ACTIVE,
	AP_LPM,
	APSUB_IDLE,
	APSUB_SLEEP,
};

struct dvc_reg {
	void __iomem *reg;
	int offset_H;	/* offset of the high bits voltage */
	int mask_H;	/* mask of the high bits voltage */
	int offset_L;	/* offset of the low bits voltage */
	int mask_L;	/* mask of the low bits voltage */
	int offset_trig;/* The offset of triggering voltage change */
	int allow_ap;	/* Whether allow AP to set this register */
};

struct voltage_item {
	int volt_level_H; /* Active voltage or High bits voltage level */
	int volt_level_L; /* Low power mode voltage or low bits voltage level */
};

#define INIT_DVFS(_clk_name, _auto, _rail, _millivolts,  _freqs)	\
	{								\
		.clk_name	= _clk_name,				\
		.auto_dvfs	= _auto,				\
		.millivolts	= _millivolts,				\
		.freqs_mult	= KHZ_TO_HZ,				\
		.dvfs_rail	= _rail,				\
		.freqs		= _freqs,				\
	}

int *vm_millivolts;
/* voltage tbl is sort ascending, it's default setting */
#ifdef D2199_DVFS	// DLG DVFS
static int vm_millivolts_default[VL_MAX] = {
	1300, 1300, 1350, 1400,

};
#else
static int vm_millivolts_default[VL_MAX] = {
	1300, 1300, 1350, 1400,
};
#endif

static int vm_millivolts_z1z2[VL_MAX] = {
	1300, 1300, 1300
};

/* 988 Z3 SVC table */
static int vm_millivolts_988z3_svc[PROFILE_NUM][VL_MAX] = {
	/* PP <= 312, PP<=624, PP<=1066, PP<=1205 */
	{1150, 1150, 1350, 1400},	/* profile 0 */
	{1150, 1150, 1275, 1275},	/* profile 1 */
	{1150, 1150, 1300, 1300},	/* profile 2 */
	{1150, 1150, 1325, 1325},	/* profile 3 */
	{1150, 1150, 1325, 1350},	/* profile 4 */  // --> TEST for 4 leves
	{1150, 1150, 1325, 1375},	/* profile 5 */
	{1150, 1150, 1325, 1400},	/* profile 6 */
	{1150, 1150, 1325, 1400},	/* profile 7 */
	{1150, 1150, 1350, 1400},	/* profile 8 */
};

/* 986 Z3 SVC table, as CP runs at 624M, higher than 988 416M */
static int vm_millivolts_986z3_svc[PROFILE_NUM][VL_MAX] = {
	/* PP <= 312, PP<=624, PP<=1066, PP<=1205 */
	{1300, 1300, 1350, 1400},	/* profile 0 */
	{1300, 1300, 1300, 1300},	/* profile 1 */
	{1300, 1300, 1300, 1300},	/* profile 2 */
	{1300, 1300, 1325, 1325},	/* profile 3 */
	{1300, 1300, 1325, 1350},	/* profile 4 */
	{1300, 1300, 1325, 1375},	/* profile 5 */
	{1300, 1300, 1325, 1400},	/* profile 6 */
	{1300, 1300, 1325, 1400},	/* profile 7 */
	{1300, 1300, 1350, 1400},	/* profile 8 */
};

/* 988 Ax SVC table, CP 416M vote VL1 */
static int vm_millivolts_988ax_svc[PROFILE_NUM][VL_MAX] = {
	{1025, 1138, 1275, 1350},	/* profile 0 */
	{1025, 1100, 1100, 1188},	/* profile 1 */
	{1025, 1100, 1113, 1200},	/* profile 2 */
	{1025, 1100, 1138, 1213},	/* profile 3 */
	{1025, 1100, 1150, 1238},	/* profile 4 */
	{1025, 1100, 1175, 1263},	/* profile 5 */
	{1025, 1100, 1200, 1288},	/* profile 6 */
	{1025, 1138, 1225, 1313},	/* profile 7 */
	{1025, 1138, 1275, 1350},	/* profile 8 */
};

/* 986 Ax SVC table, CP 624M vote VL2 */
static int vm_millivolts_986ax_svc[PROFILE_NUM][VL_MAX] = {
	{1025, 1138, 1275, 1350},	/* profile 0 */
	{1025, 1100, 1100, 1188},	/* profile 1 */
	{1025, 1100, 1113, 1200},	/* profile 2 */
	{1025, 1100, 1138, 1213},	/* profile 3 */
	{1025, 1100, 1150, 1238},	/* profile 4 */
	{1025, 1100, 1175, 1263},	/* profile 5 */
	{1025, 1100, 1200, 1288},	/* profile 6 */
	{1025, 1138, 1225, 1313},	/* profile 7 */
	{1025, 1138, 1275, 1350},	/* profile 8 */
};

/* 988 Ax 1p5G SVC table, CP 416M vote VL1 */
static int vm_millivolts_988ax_svc_1p5G[PROFILE_NUM][VL_MAX] = {
	{1025, 1138, 1275, 1375},	/* profile 0 */
	{1025, 1100, 1100, 1288},	/* profile 1 */
	{1025, 1100, 1113, 1325},	/* profile 2 */
	{1025, 1100, 1138, 1363},	/* profile 3 */
	{1025, 1100, 1150, 1375},	/* profile 4 */
	{1025, 1100, 1175, 1375},	/* profile 5 */
	{1025, 1100, 1200, 1375},	/* profile 6 */
	{1025, 1138, 1225, 1375},	/* profile 7 */
	{1025, 1138, 1275, 1375},	/* profile 8 */
};

/* FIXME: hard coding svc of 986ax for harrison bringup*/
static int vm_millivolts_986ax_1v4_svc[PROFILE_NUM][VL_MAX] = {
    {1400, 1400, 1400, 1400},   /* profile 0 */
    {1400, 1400, 1400, 1400},   /* profile 1 */
    {1400, 1400, 1400, 1400},   /* profile 2 */
    {1400, 1400, 1400, 1400},   /* profile 3 */
    {1400, 1400, 1400, 1400},   /* profile 4 */
    {1400, 1400, 1400, 1400},   /* profile 5 */
    {1400, 1400, 1400, 1400},   /* profile 6 */
    {1400, 1400, 1400, 1400},   /* profile 7 */
    {1400, 1400, 1400, 1400},   /* profile 8 */
};

#if !defined(CONFIG_CORE_1248)
/* 986 Ax 1p5G table, CP 624M vote VL2 */
static int vm_millivolts_986ax_svc_1p5G[PROFILE_NUM][VL_MAX] = {
	{1025, 1138, 1275, 1375},	/* profile 0 */
	{1025, 1100, 1100, 1288},	/* profile 1 */
	{1025, 1100, 1113, 1325},	/* profile 2 */
	{1025, 1100, 1138, 1363},	/* profile 3 */
	{1025, 1100, 1150, 1375},	/* profile 4 */
	{1025, 1100, 1175, 1375},	/* profile 5 */
	{1025, 1100, 1200, 1375},	/* profile 6 */
	{1025, 1138, 1225, 1375},	/* profile 7 */
	{1025, 1138, 1275, 1375},	/* profile 8 */
};
#else
/* 986 Ax 1p5G table, CP 624M vote VL2 */
static int vm_millivolts_986ax_svc_1p5G[PROFILE_NUM][VL_MAX] = {
	{1025, 1138, 1275, 1400},	/* profile 0 */
	{1025, 1100, 1100, 1188},	/* profile 1 */
	{1025, 1100, 1113, 1213},	/* profile 2 */
	{1025, 1100, 1138, 1238},	/* profile 3 */
	{1025, 1100, 1150, 1263},	/* profile 4 */
	{1025, 1100, 1175, 1300},	/* profile 5 */
	{1025, 1100, 1200, 1338},	/* profile 6 */
	{1025, 1138, 1225, 1363},	/* profile 7 */
	{1025, 1138, 1275, 1400},	/* profile 8 */
};
#endif
/*
 * 1088 SVC table
 */
static int vm_mv_1088a0_svc_1p1G[PROFILE_NUM][VL_MAX] = {
	{1000, 1075, 1300, 1375},	/* profile 0 */
	{1000, 1075, 1100, 1138},	/* profile 1 */
	{1000, 1075, 1100, 1175},	/* profile 2 */
	{1000, 1075, 1125, 1225},	/* profile 3 */
	{1000, 1075, 1163, 1238},	/* profile 4 */
	{1000, 1075, 1200, 1275},	/* profile 5 */
	{1000, 1075, 1238, 1325},	/* profile 6 */
	{1000, 1075, 1275, 1350},	/* profile 7 */
	{1000, 1075, 1300, 1375},	/* profile 8 */
};

static int vm_mv_1088a0_svc_1p2G[PROFILE_NUM][VL_MAX] = {
	{1000, 1075, 1300, 1375},	/* profile 0 */
	{1000, 1075, 1100, 1200},	/* profile 1 */
	{1000, 1075, 1100, 1200},	/* profile 2 */
	{1000, 1075, 1125, 1238},	/* profile 3 */
	{1000, 1075, 1163, 1288},	/* profile 4 */
	{1000, 1075, 1200, 1325},	/* profile 5 */
	{1000, 1075, 1238, 1363},	/* profile 6 */
	{1000, 1075, 1275, 1375},	/* profile 7 */
	{1000, 1075, 1300, 1375},	/* profile 8 */
};

static int vm_mv_1088a0_svc_1p3G[PROFILE_NUM][VL_MAX] = {
	{1000, 1075, 1300, 1375},	/* profile 0 */
	{1000, 1075, 1100, 1238},	/* profile 1 */
	{1000, 1075, 1100, 1288},	/* profile 2 */
	{1000, 1075, 1125, 1338},	/* profile 3 */
	{1000, 1075, 1163, 1375},	/* profile 4 */
	{1000, 1075, 1200, 1375},	/* profile 5 */
	{1000, 1075, 1238, 1375},	/* profile 6 */
	{1000, 1075, 1275, 1375},	/* profile 7 */
	{1000, 1075, 1300, 1375},	/* profile 8 */
};

static int vm_mv_1088a1_svc_1p1G[PROFILE_NUM][VL_MAX] = {
	{1025, 1075, 1250, 1375},	/* profile 0 */
	{1025, 1075, 1150, 1138},	/* profile 1 */
	{1025, 1075, 1150, 1175},	/* profile 2 */
	{1025, 1075, 1150, 1213},	/* profile 3 */
	{1025, 1075, 1150, 1263},	/* profile 4 */
	{1025, 1075, 1150, 1300},	/* profile 5 */
	{1025, 1075, 1188, 1338},	/* profile 6 */
	{1025, 1075, 1225, 1375},	/* profile 7 */
	{1025, 1075, 1250, 1375},	/* profile 8 */
};

static int vm_mv_1088a1_svc_1p2G[PROFILE_NUM][VL_MAX] = {
	{1075, 1075, 1250, 1375},	/* profile 0 */
	{1075, 1075, 1100, 1225},	/* profile 1 */
	{1075, 1075, 1100, 1225},	/* profile 2 */
	{1075, 1075, 1100, 1238},	/* profile 3 */
	{1075, 1075, 1125, 1288},	/* profile 4 */
	{1075, 1075, 1150, 1325},	/* profile 5 */
	{1075, 1075, 1188, 1363},	/* profile 6 */
	{1075, 1075, 1225, 1375},	/* profile 7 */
	{1075, 1075, 1250, 1375},	/* profile 8 */
};
static int vm_mv_1088a1_svc_1p3G[PROFILE_NUM][VL_MAX] = {
	{1025, 1075, 1250, 1375},	/* profile 0 */
	{1025, 1075, 1100, 1238},	/* profile 1 */
	{1025, 1075, 1100, 1288},	/* profile 2 */
	{1025, 1075, 1100, 1338},	/* profile 3 */
	{1025, 1075, 1125, 1375},	/* profile 4 */
	{1025, 1075, 1150, 1375},	/* profile 5 */
	{1025, 1075, 1188, 1375},	/* profile 6 */
	{1025, 1075, 1225, 1375},	/* profile 7 */
	{1025, 1075, 1250, 1375},	/* profile 8 */
};

/* ptr used for components' freqs combination */
static unsigned long (*component_freqs)[VL_MAX];

/************************* GPIO DVC **************************/
/*
 * NOTES: we set step to 500mV here as we don't want
 * voltage change step by step, as GPIO based DVC is
 * used. This can avoid tmp voltage which is not in saved
 * in 4 level regulator table.
 */
static struct dvfs_rail pxa988_dvfs_rail_vm = {
	.reg_id = "vcc_main",
	.max_millivolts = 1400,
	.min_millivolts = 1000,
	.nominal_millivolts = 1200,
	.step = 500,
};

static unsigned long freqs_cmb_z1z2[VM_RAIL_MAX][VL_MAX] = {
	{ 600000, 800000, 1200000 },	/* CORE */
	{ 300000, 400000, 400000 },	/* DDR/AXI */
	{ 600000, 600000, 600000 },	/* GC */
	{ 400000, 400000, 400000 }	/* VPU */
};

static struct dvfs_rail_component *vm_rail_comp_tbl;
static struct dvfs_rail_component vm_rail_comp_tbl_z1z2[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_vm, NULL,
		  freqs_cmb_z1z2[CORE]),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_vm, NULL,
		  freqs_cmb_z1z2[DDR_AXI]),
	INIT_DVFS("GCCLK", false, &pxa988_dvfs_rail_vm, NULL,
		  freqs_cmb_z1z2[GC]),
	INIT_DVFS("VPUCLK", false, &pxa988_dvfs_rail_vm, NULL,
		  freqs_cmb_z1z2[VPU]),
};

static unsigned long freqs_cmb_z3[VM_RAIL_MAX][VL_MAX] = {
	{ 312000, 624000, 1066000, 1205000 },	/* CORE */
	{ 312000, 312000, 533000, 533000 },	/* DDR/AXI */
	{ 0	, 416000, 624000, 624000 },	/* GC */
	{ 208000, 312000, 416000, 416000 }	/* VPU */
};

static unsigned long freqs_cmb_ax[VM_RAIL_MAX][VL_MAX] = {
	{ 624000, 624000, 1066000, 1482000 },	/* CORE */
	{ 312000, 312000, 312000, 533000 },	/* DDR/AXI */
	{ 0, 416000, 624000, 624000 },	/* GC */
	{ 0, 312000, 416000, 416000 }	/* VPU */
};

static unsigned long freqs_cmb_1088a0[VM_RAIL_MAX][VL_MAX] = {
	{ 312000, 312000, 1066000, 1300000 },	/* CORE */
	{ 156000, 156000, 312000, 533000 },	/* DDR/AXI */
	{ 0, 416000, 624000, 624000 },		/* GC */
#ifdef	CONFIG_CPU_PXA1088
	{ 0, 312000, 416000, 624000 },		/* GC2D */
	{ 312000, 416000, 416000, 416000 },	/* ISP */
#endif
	{ 0, 312000, 416000, 624000 }		/* VPU */
};

static unsigned long freqs_cmb_1088a1[VM_RAIL_MAX][VL_MAX] = {
	{ 312000, 312000, 800000, 1300000 },	/* CORE */
	{ 156000, 312000, 400000, 533000 },	/* DDR/AXI */
	{ 0, 416000, 416000, 624000 },		/* GC */
#ifdef	CONFIG_CPU_PXA1088
	{ 0, 312000, 416000, 416000 },		/* GC2D */
	{ 312000, 416000, 416000, 416000 },	/* ISP */
#endif
	{ 0, 312000, 416000, 416000 }		/* VPU */
};

static struct dvfs_rail_component vm_rail_comp_tbl_gpiodvc[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_vm, NULL, NULL),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_vm, NULL, NULL),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_vm, NULL, NULL),
#ifdef CONFIG_CPU_PXA1088
	INIT_DVFS("GC2DCLK", true,  &pxa988_dvfs_rail_vm, NULL, NULL),
	INIT_DVFS("ISP-CLK", true,  &pxa988_dvfs_rail_vm, NULL, NULL),
#endif
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_vm, NULL, NULL),
};

/************************* PMU DVC **************************/
/* default CP/MSA required PMU DVC VL */
static unsigned int cp_pmudvc_lvl = VL1;
static unsigned int msa_pmudvc_lvl = VL1;

/* Rails for pmu dvc */
static struct dvfs_rail pxa988_dvfs_rail_ap_active = {
	.reg_id = "vcc_main_ap_active",
	.max_millivolts = LEVEL3,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL_START,
	.step = 0xFF,
};

static struct dvfs_rail pxa988_dvfs_rail_ap_lpm = {
	.reg_id = "vcc_main_ap_lpm",
	.max_millivolts = LEVEL3,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL_START,
	.step = 0xFF,
};

static struct dvfs_rail pxa988_dvfs_rail_apsub_idle = {
	.reg_id = "vcc_main_apsub_idle",
	.max_millivolts = LEVEL3,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL_START,
	.step = 0xFF,
};

static struct dvfs_rail pxa988_dvfs_rail_apsub_sleep = {
	.reg_id = "vcc_main_apsub_sleep",
	.max_millivolts = LEVEL3,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL_START,
	.step = 0xFF,
};

static struct dvfs_rail *pxa988_dvfs_rails_pmudvc[] = {
	&pxa988_dvfs_rail_ap_active,
	&pxa988_dvfs_rail_ap_lpm,
	&pxa988_dvfs_rail_apsub_idle,
	&pxa988_dvfs_rail_apsub_sleep,
};

/* component_voltage table and component_freqs combination is dynamic inited */
static int component_voltage[][MAX_RAIL_NUM][LEVEL_END] = {
	[CORE] = {
	},
	[DDR_AXI] = {
	},
	[GC] = {
	},
#ifdef	CONFIG_CPU_PXA1088
	[GC2D] = {
	},
	[ISP] = {
	},
#endif
	[VPU] = {
	},
};

/* PMU DVC freq-combination to dynamic filled to support different platform */
static struct dvfs_rail_component vm_rail_ap_active_tbl[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_ap_active,
		component_voltage[CORE][AP_ACTIVE], NULL),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_ap_active,
		component_voltage[DDR_AXI][AP_ACTIVE], NULL),
	INIT_DVFS("GCCLK", true,  &pxa988_dvfs_rail_ap_active,
		component_voltage[GC][AP_ACTIVE], NULL),
#ifdef CONFIG_CPU_PXA1088
	INIT_DVFS("GC2DCLK", true,  &pxa988_dvfs_rail_ap_active,
		component_voltage[GC2D][AP_ACTIVE], NULL),
	INIT_DVFS("ISP-CLK", true,  &pxa988_dvfs_rail_ap_active,
		component_voltage[ISP][AP_ACTIVE], NULL),
#endif
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_ap_active,
		component_voltage[VPU][AP_ACTIVE], NULL),
};

static struct dvfs_rail_component vm_rail_ap_lpm_tbl[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_ap_lpm,
		component_voltage[CORE][AP_LPM], NULL),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_ap_lpm,
		component_voltage[DDR_AXI][AP_LPM], NULL),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_ap_lpm,
		component_voltage[GC][AP_LPM], NULL),
#ifdef CONFIG_CPU_PXA1088
	INIT_DVFS("GC2DCLK", true,  &pxa988_dvfs_rail_ap_lpm,
		component_voltage[GC2D][AP_LPM], NULL),
	INIT_DVFS("ISP-CLK", true,  &pxa988_dvfs_rail_ap_lpm,
		component_voltage[ISP][AP_LPM], NULL),
#endif
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_ap_lpm,
		component_voltage[VPU][AP_LPM], NULL),
};

static struct dvfs_rail_component vm_rail_apsub_idle_tbl[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_apsub_idle,
		component_voltage[CORE][APSUB_IDLE], NULL),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_apsub_idle,
		component_voltage[DDR_AXI][APSUB_IDLE], NULL),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_apsub_idle,
		component_voltage[GC][APSUB_IDLE], NULL),
#ifdef CONFIG_CPU_PXA1088
	INIT_DVFS("GC2DCLK", true,  &pxa988_dvfs_rail_apsub_idle,
		component_voltage[GC2D][APSUB_IDLE], NULL),
	INIT_DVFS("ISP-CLK", true,  &pxa988_dvfs_rail_apsub_idle,
		component_voltage[ISP][APSUB_IDLE], NULL),
#endif
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_apsub_idle,
		component_voltage[VPU][APSUB_IDLE], NULL),
};

static struct dvfs_rail_component vm_rail_apsub_sleep_tbl[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_apsub_sleep,
		component_voltage[CORE][APSUB_SLEEP], NULL),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_apsub_sleep,
		component_voltage[DDR_AXI][APSUB_SLEEP], NULL),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_apsub_sleep,
		component_voltage[GC][APSUB_SLEEP], NULL),
#ifdef CONFIG_CPU_PXA1088
	INIT_DVFS("GC2DCLK", true,  &pxa988_dvfs_rail_apsub_sleep,
		component_voltage[GC2D][APSUB_SLEEP], NULL),
	INIT_DVFS("ISP-CLK", true,  &pxa988_dvfs_rail_apsub_sleep,
		component_voltage[ISP][APSUB_SLEEP], NULL),
#endif
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_apsub_sleep,
		component_voltage[VPU][APSUB_SLEEP], NULL),
};

static struct dvfs_rail_component *dvfs_rail_component_list[] = {
	vm_rail_ap_active_tbl,
	vm_rail_ap_lpm_tbl,
	vm_rail_apsub_idle_tbl,
	vm_rail_apsub_sleep_tbl,
};

static void update_all_pmudvc_rails_info(void);

static struct dvc_reg dvc_reg_table[DVC_END] = {
	{
		.reg = PMUM_DVC_AP,
		.offset_H = 4,
		.mask_H = 0x70,
		.offset_L = 0,
		.mask_L = 0x7,
		.offset_trig = 7,
		.allow_ap = 1,
	},
	{
		.reg = PMUM_DVC_CP,
		.offset_H = 4,
		.mask_H = 0x70,
		.offset_L = 0,
		.mask_L = 0x7,
		.offset_trig = 7,
		.allow_ap = 0,
	},
	{
		.reg = PMUM_DVC_DP,
		.offset_H = 4,
		.mask_H = 0x70,
		.offset_L = 0,
		.mask_L = 0x7,
		.offset_trig = 7,
		.allow_ap = 0,
	},
	{
		.reg = PMUM_DVC_APSUB,
		.offset_H = 8,
		.mask_H = 0x700,
		.offset_L = 0,
		.mask_L = 0x7,
		.allow_ap = 1,
	},
	{
		.reg = PMUM_DVC_CHIP,
		.offset_H = 4,
		.mask_H = 0x70,
		.offset_L = 0,
		.mask_L = 0x7,
		.allow_ap = 1,
	},
};

#define PM800_BUCK1	(0x3C)
static struct voltage_item current_volt_table[DVC_END];

static inline int volt_to_reg(int millivolts)
{
	//dlg_trace("millivolts : %d, [%d]\n", millivolts, (millivolts - 600) * 100 / 625);
#ifdef D2199_DVFS	// DLG DVFS
	return (millivolts - 600) * 100 / 625;
#else
	return (millivolts - 600) * 10 / 125;
#endif
}

static inline int reg_to_volt(int value)
{
	/* Round up to int value, eg, 1287.5 ==> 1288 */
	//dlg_trace("value : %d, [%d]\n", value, (value * 625 + 60000 + 90) / 100);
#ifdef D2199_DVFS	// DLG DVFS
	return (value * 625 + 60000 + 90) / 100;
#else
	return (value * 125 + 6000 + 9) / 10;
#endif
}

static inline int get_buck1_volt(int level)
{
#if defined(CONFIG_MFD_88PM822)
	return pm822_extern_read(PM822_POWER_PAGE, PM822_BUCK1 + level);
#elif defined(CONFIG_MFD_88PM800)
	return pm800_extern_read(PM80X_POWER_PAGE, PM800_BUCK1 + level);
#else /* CONFIG_MFD_D2199 */
#ifdef D2199_DVFS
#if defined(CONFIG_D2199_DVC)
	return d2199_extern_dvc_read(level);
#else
	return d2199_dvfs_get_voltage(level);
#endif /* CONFIG_D2199_DVC */
#endif /* D2199_DVFS */
#endif /* CONFIG_MFD_D2199 */
}

static inline int set_buck1_volt(int level, int val)
{
#if defined(CONFIG_MFD_88PM822)
	return pm822_extern_write(PM822_POWER_PAGE, PM822_BUCK1 + level, val);
#elif defined(CONFIG_MFD_88PM800)
	return pm800_extern_write(PM80X_POWER_PAGE, PM800_BUCK1 + level, val);
#else /* CONFIG_MFD_D2199 */
#ifdef D2199_DVFS
#if defined(CONFIG_D2199_DVC)
	return d2199_extern_dvc_write(level, val);
#else
	return d2199_dvfs_set_voltage(level, val);
#endif /* CONFIG_D2199_DVC */
#endif /* D2199_DVFS */
#endif /* CONFIG_MFD_D2199 */
}

static int get_stable_ticks(int millivolts1, int millivolts2)
{
	int max, min, ticks;
	//dlg_trace("millivolts1[%d], millivolts2[%d]\n", millivolts1, millivolts2);

	max = max(millivolts1, millivolts2);
	min = min(millivolts1, millivolts2);
	/*
	 * clock is VCTCXO(26Mhz), 1us is 26 ticks
	 * PMIC voltage change is 12.5mV/us
	 * PMIC launch time is 8us(include 2us dvc pin sync time)
	 * For safe consideration, add 2us in ramp up time
	 * so the total latency is 10us
	 */
	ticks = ((max - min) * 10 / 125 + 10) * 26;
	if (ticks > VLXX_ST_MASK)
		ticks = VLXX_ST_MASK;

	//dlg_trace("ticks[%d]\n", ticks);
	return ticks;
}

static int stable_time_inited;

/* Set PMIC voltage value of a specific level */
static int set_voltage_value(int level, int value)
{
	unsigned int regval;
	int ret = 0;
	if (level < 0 || level > 3) {
		printk(KERN_ERR "Wrong level! level should be between 0~3\n");
		return -EINVAL;
	}
	regval = volt_to_reg(value);
	//dlg_trace("level[%d], value[%d], regval[%d]\n", level, value, regval);

	ret = set_buck1_volt(level, regval);
	if (ret)
		printk(KERN_ERR "PMIC voltage replacement failed!\n");
	return ret;
}

/* Read PMIC to get voltage value according to level */
static int get_voltage_value(int level)
{
	int reg, value;
	if (level < 0 || level > 3) {
		printk(KERN_ERR "Wrong level! level should be between 0~3\n");
		return -EINVAL;
	}
	reg = get_buck1_volt(level);
	if (reg < 0) {
		printk(KERN_ERR "PMIC voltage reading failed !\n");
		return -1;
	}
	value = reg_to_volt(reg);
	dlg_trace("level[%d], value[%d]\n", level, value);
	return value;
}

/* FIXME: add the real voltages here
 * Note: the voltage should be indexed by LEVEL
 * first value is useless, it is indexed by LEVEL_START
 * Second value is LEVEL0, then LEVEL1, LEVEL2 ...
 * Different level may have same voltage value
*/
static int voltage_millivolts[MAX_RAIL_NUM][LEVEL_END] = {
	{ }, /* AP active voltages (in millivolts)*/
	{ }, /* AP lpm voltages (in millivolts)*/
	{ }, /* APSUB idle voltages (in millivolts)*/
	{ }, /* APSUB sleep voltages (in millivolts)*/
};



/* pmu level to pmic level mapping
 * Current setting:
 * level 1 is mapped to level 1
 * level 2 is mapped to level 2
 * level 3~7 is mapped to level 3
 * level 0 and level 0_x is mapped to level 0
 */
static int level_mapping[LEVEL_END];
static void init_level_mapping(void)
{
	dlg_trace("\n");
	level_mapping[LEVEL0] = 0;
	level_mapping[LEVEL1] = 1;
	level_mapping[LEVEL2] = 2;
	level_mapping[LEVEL3] = 3;
	level_mapping[LEVEL4] = 3;
	level_mapping[LEVEL5] = 3;
	level_mapping[LEVEL6] = 3;
	level_mapping[LEVEL7] = 3;
	level_mapping[LEVEL0_1] = 0;
	level_mapping[LEVEL0_2] = 0;
}

#define PMIC_LEVEL_NUM	4
static int cur_level_volt[PMIC_LEVEL_NUM];
static int cur_volt_inited;
/* Set a different level voltage according to the level value
 * @level the actual level value that dvc wants to set.
 * for example, if level == 4, and pmic only has level 0~3,
 * it needs to replace one level with level4's voltage.
*/
static int replace_level_voltage(int buck_id, int *level)
{
	int value, ticks;
	int pmic_level = level_mapping[*level];
	int volt = voltage_millivolts[buck_id - BUCK1_AP_ACTIVE][*level];

	//dlg_trace("buck_id[%d], level[%d], pmic_level[%d], volt[%d]\n", buck_id, *level, pmic_level, volt);
	//dlg_trace("cur_volt_inited[%d], cur_level_volt[pmic_level][%d], volt[%d]\n",
	//			cur_volt_inited, cur_level_volt[pmic_level], volt);

#if defined(CONFIG_D2199_DVC)
	if (cur_volt_inited) {
#else
	if (cur_volt_inited && (cur_level_volt[pmic_level] != volt)) {
#endif
		//dlg_trace("pmic_level[%d]\n", pmic_level);

		set_voltage_value(pmic_level, volt);
		pr_debug("Replace pmic level %d from %d -> %d!\n",
			pmic_level, cur_level_volt[pmic_level], volt);
		cur_level_volt[pmic_level] = volt;
		/* Update voltage level change stable time */
		if ((pmic_level == 0) || (pmic_level == 1)) {
			value = __raw_readl(PMUM_VL01STR);
			value &= ~VLXX_ST_MASK;
			ticks = get_stable_ticks(cur_level_volt[0], cur_level_volt[1]);
			value |= ticks;
			__raw_writel(value, PMUM_VL01STR);
		}

		if ((pmic_level == 1) || (pmic_level == 2)) {
			value = __raw_readl(PMUM_VL12STR);
			value &= ~VLXX_ST_MASK;
			ticks = get_stable_ticks(cur_level_volt[1], cur_level_volt[2]);
			value |= ticks;
			__raw_writel(value, PMUM_VL12STR);
		}

		if ((pmic_level == 2) || (pmic_level == 3)) {
			value = __raw_readl(PMUM_VL23STR);
			value &= ~VLXX_ST_MASK;
			ticks = get_stable_ticks(cur_level_volt[2], cur_level_volt[3]);
			value |= ticks;
			__raw_writel(value, PMUM_VL23STR);
		}
	}

	*level = pmic_level;
	//dlg_trace("level[%d]\n", *level);
	return 0;
}

#if defined(CONFIG_D2199_DVC)
static int ramp_up_vol = 2500; /* unit : 100uV */
int d2199_dvc_set_voltage(int buck_id, int level)
{
	unsigned int reg_val;
	int reg_index = 0, high_bits; /* If this is high bits voltage */
	static unsigned int pmic_vl_inited = 0;
	static unsigned int lvl = 0;
	/*
	 * Max delay time, unit is us. : (ramp_up_vol / 12.5mv)
	 * mode transition time = 18
	 * add some buffer here = 4
	 * default delay should be 0xFFFF * 1 tick(L2-->L3)
	 */
	int max_delay;
	//dlg_trace("stable_time_inited[%d], buck_id[%d], level[%d]\n", stable_time_inited, buck_id, level);

	/* updated lvl1 ~ lv3 during init stage */
	if (unlikely(!pmic_vl_inited)) {
		/* max vl to pmic leve 3 */
		lvl = pxa988_get_vl_num() - 1;
#if 1	// dlg
		set_voltage_value(level_mapping[LEVEL3], pxa988_get_vl(lvl));
		set_voltage_value(level_mapping[LEVEL1], pxa988_get_vl(1));
		set_voltage_value(level_mapping[LEVEL2], pxa988_get_vl(2));
#endif
		pmic_vl_inited = 1;
	}

	if (stable_time_inited)
		max_delay = ((ramp_up_vol / 125) + 18 + 4) * 1; /* pmic sync time may be accumulated */
	else
		max_delay = DIV_ROUND_UP(0xFFFF * 1, 26);

	if ((buck_id < BUCK1_AP_ACTIVE) || (buck_id > BUCK1_APSUB_SLEEP)) {
		printk(KERN_ERR "Not new dvc regulator, Can't use %s\n", __func__);
		return 0;
	}

	replace_level_voltage(buck_id, &level);
	//dlg_trace("buck_id[%d], level[%d]\n", buck_id, level);

	if (buck_id == BUCK1_AP_ACTIVE) {
		reg_index = DVC_AP;
		high_bits = 1;
	} else if (buck_id == BUCK1_AP_LPM) {
		reg_index = DVC_AP;
		high_bits = 0;
	} else if (buck_id == BUCK1_APSUB_IDLE) {
		reg_index = DVC_APSUB;
		high_bits = 0;
	} else if (buck_id == BUCK1_APSUB_SLEEP) {
		reg_index = DVC_APSUB;
		high_bits = 1;
	}

	if (!dvc_reg_table[reg_index].allow_ap) {
		printk(KERN_ERR "AP can't set this register !\n");
		return 0;
	}

	if (high_bits) {
		//dlg_trace(" if high_bits\n");
		/* Set high bits voltage */
		if (current_volt_table[reg_index].volt_level_H == level)
			return 0;

		//////////////
		d2199_dvfs_set_voltage(0,level);

		//////////////
		reg_val = __raw_readl(dvc_reg_table[reg_index].reg);
		reg_val &= ~dvc_reg_table[reg_index].mask_H;
		reg_val |= (level << dvc_reg_table[reg_index].offset_H);
		if (dvc_reg_table[reg_index].offset_trig)
			reg_val |= (1 << dvc_reg_table[reg_index].offset_trig);
		pr_debug("%s Active VL[%x] = [%x]\n", __func__,
			(unsigned int)dvc_reg_table[reg_index].reg, reg_val);
		__raw_writel(reg_val, dvc_reg_table[reg_index].reg);

		/* Only AP Active voltage change needs polling */
		if (buck_id == BUCK1_AP_ACTIVE) {
			if (stable_time_inited) {
				if ( (level == lvl) &&
					(level > current_volt_table[reg_index].volt_level_H) )
					udelay(max_delay);
			} else {
				while (max_delay && !(__raw_readl(PMUM_DVC_ISR)
				       & AP_VC_DONE_INTR_ISR)) {
					udelay(1);
					max_delay--;
				}
				if (!max_delay) {
					printk(KERN_ERR "AP active voltage change can't finish!\n");
					BUG_ON(1);
				}
				/*
				 * Clear AP interrupt status
				 * write 0 to clear, write 1 has no effect
				 */
				__raw_writel(0x6, PMUM_DVC_ISR);
			}
		}
		current_volt_table[reg_index].volt_level_H = level;
	} else {
		//dlg_trace(" else high_bits\n");
		/* Set low bits voltage, no need to poll status */
		if (current_volt_table[reg_index].volt_level_L == level)
			return 0;
		///////////
		d2199_dvfs_set_voltage(0,level);

		/////////////
		reg_val = __raw_readl(dvc_reg_table[reg_index].reg);
		reg_val &= ~dvc_reg_table[reg_index].mask_L;
		reg_val |= (level << dvc_reg_table[reg_index].offset_L);
		pr_debug("%s LPM VL[%x] = [%x]\n", __func__,
			(unsigned int)dvc_reg_table[reg_index].reg, reg_val);
		__raw_writel(reg_val, dvc_reg_table[reg_index].reg);
		current_volt_table[reg_index].volt_level_L = level;
	}
	return 0;
}
EXPORT_SYMBOL(d2199_dvc_set_voltage);
#endif	// DLG_DVFS

int dvc_set_voltage(int buck_id, int level)
{
	unsigned int reg_val;
	int reg_index = 0, high_bits; /* If this is high bits voltage */
	static unsigned int pmic_vl_inited = 0;
	unsigned int lvl = 0;
	/*
	 * Max delay time, unit is us. (1.5v - 1v) / 0.125v = 40
	 * Also PMIC needs 10us to launch and sync dvc pins
	 * default delay should be 0xFFFF * 3 ticks(L0-->L3 or L3-->L0)
	 */
	int max_delay;
	dlg_trace("stable_time_inited[%d], buck_id[%d], level[%d]\n", stable_time_inited, buck_id, level);

	/* updated lvl1 ~ lv3 during init stage */
	if (unlikely(!pmic_vl_inited)) {
		/* max vl to pmic leve 3 */
		lvl = pxa988_get_vl_num() - 1;
		set_voltage_value(level_mapping[LEVEL3], pxa988_get_vl(lvl));
		set_voltage_value(level_mapping[LEVEL1], pxa988_get_vl(1));
		set_voltage_value(level_mapping[LEVEL2], pxa988_get_vl(2));
		pmic_vl_inited = 1;
	}

	if (stable_time_inited)
		max_delay = 40 + 10 * 3; /* pmic sync time may be accumulated */
	else
		max_delay = DIV_ROUND_UP(0xFFFF * 3, 26);

	if ((buck_id < BUCK1_AP_ACTIVE) || (buck_id > BUCK1_APSUB_SLEEP)) {
		printk(KERN_ERR "Not new dvc regulator, Can't use %s\n", __func__);
		return 0;
	}
	replace_level_voltage(buck_id, &level);
	dlg_trace("buck_id[%d], level[%d]\n", buck_id, level);
	if (buck_id == BUCK1_AP_ACTIVE) {
		reg_index = DVC_AP;
		high_bits = 1;
	} else if (buck_id == BUCK1_AP_LPM) {
		reg_index = DVC_AP;
		high_bits = 0;
	} else if (buck_id == BUCK1_APSUB_IDLE) {
		reg_index = DVC_APSUB;
		high_bits = 0;
	} else if (buck_id == BUCK1_APSUB_SLEEP) {
		reg_index = DVC_APSUB;
		high_bits = 1;
	}

	if (!dvc_reg_table[reg_index].allow_ap) {
		printk(KERN_ERR "AP can't set this register !\n");
		return 0;
	}

	if (high_bits) {
		dlg_trace(" if high_bits\n");
		/* Set high bits voltage */
		if (current_volt_table[reg_index].volt_level_H == level)
			return 0;
		/*
		 * AP SW is the only client to trigger AP DVC.
		 * Clear AP interrupt status to make sure no wrong signal is set
		 * write 0 to clear, write 1 has no effect
		 */
		__raw_writel(0x6, PMUM_DVC_ISR);

		reg_val = __raw_readl(dvc_reg_table[reg_index].reg);
		reg_val &= ~dvc_reg_table[reg_index].mask_H;
		reg_val |= (level << dvc_reg_table[reg_index].offset_H);
		if (dvc_reg_table[reg_index].offset_trig)
			reg_val |= (1 << dvc_reg_table[reg_index].offset_trig);
		pr_debug("%s Active VL[%x] = [%x]\n", __func__,
			(unsigned int)dvc_reg_table[reg_index].reg, reg_val);
		__raw_writel(reg_val, dvc_reg_table[reg_index].reg);
		/* Only AP Active voltage change needs polling */
		if (buck_id == BUCK1_AP_ACTIVE) {
			while (max_delay && !(__raw_readl(PMUM_DVC_ISR)
			       & AP_VC_DONE_INTR_ISR)) {
				udelay(1);
				max_delay--;
			}
			if (!max_delay) {
				printk(KERN_ERR "AP active voltage change can't finish!\n");
				BUG_ON(1);
			}
			/*
			 * Clear interrupt status
			 * write 0 to clear, write 1 has no effect
			 */
			__raw_writel(0x6, PMUM_DVC_ISR);
		}
		current_volt_table[reg_index].volt_level_H = level;
	} else {
		dlg_trace(" else high_bits\n");
		/* Set low bits voltage, no need to poll status */
		if (current_volt_table[reg_index].volt_level_L == level)
			return 0;
		reg_val = __raw_readl(dvc_reg_table[reg_index].reg);
		reg_val &= ~dvc_reg_table[reg_index].mask_L;
		reg_val |= (level << dvc_reg_table[reg_index].offset_L);
		pr_debug("%s LPM VL[%x] = [%x]\n", __func__,
			(unsigned int)dvc_reg_table[reg_index].reg, reg_val);
		__raw_writel(reg_val, dvc_reg_table[reg_index].reg);
		current_volt_table[reg_index].volt_level_L = level;
	}

	return 0;
}
EXPORT_SYMBOL(dvc_set_voltage);

/* init the voltage and rail/frequency tbl according to platform info */
static int __init setup_dvfs_platinfo(void)
{
	unsigned int uiprofile = get_profile();
	int i, j;
	int min_cp_millivolts = 0;

	dlg_trace(" cpu_is_pxa1088 [%d], uiprofile[%d]\n", cpu_is_pxa1088(), uiprofile);

	if (get_board_id() == VER_T7)
		dvc_pin_switch = 0;

	vm_millivolts = vm_millivolts_default;

	/* z1/z2/z3/Ax will use different voltage */
	if (cpu_is_z1z2()) {
		vm_millivolts = vm_millivolts_z1z2;
		dvc_flag = 0;
	} else if (cpu_is_pxa988_z3()) {
		vm_millivolts = vm_millivolts_988z3_svc[uiprofile];
		dvc_flag = 0;
		component_freqs = freqs_cmb_z3;
	} else if (cpu_is_pxa986_z3()) {
		vm_millivolts = vm_millivolts_986z3_svc[uiprofile];
		dvc_flag = 0;
		component_freqs = freqs_cmb_z3;
	} else if (cpu_is_pxa988_a0()) {
		if (is_pxa988a0svc) {
			vm_millivolts = vm_millivolts_988ax_svc[uiprofile];
			if (max_freq > 1205)
				vm_millivolts =
					vm_millivolts_988ax_svc_1p5G[uiprofile];
		} else
			vm_millivolts = vm_millivolts_988z3_svc[uiprofile];
		/*
		 * 988 A0 CP/MSA/TD modem requires at least VL1 1.1V
		 */
		cp_pmudvc_lvl = msa_pmudvc_lvl = VL1;
		component_freqs = freqs_cmb_ax;
	} else if (cpu_is_pxa986_a0()) {
		if (is_pxa988a0svc) {
			vm_millivolts = vm_millivolts_986ax_svc[uiprofile];
			if (max_freq > 1205)
				vm_millivolts =
					vm_millivolts_986ax_svc_1p5G[uiprofile];
		} else
			vm_millivolts = vm_millivolts_986z3_svc[uiprofile];
		/*
		 * 986 A0 CP/MSA/TD modem requires at least VL2
		 */
		cp_pmudvc_lvl = msa_pmudvc_lvl = VL2;
		component_freqs = freqs_cmb_ax;
	} else if (cpu_is_pxa1088()) {
		if (has_feat_higher_ddr_vmin()) {
			if (ddr_mode) {
				pr_err("DDR 533M is not supported on this board!!!\n");
				BUG_ON(1);
			}

			if (max_freq > CORE_1p18G)
				vm_millivolts =
					vm_mv_1088a0_svc_1p3G[uiprofile];
			else if (max_freq > CORE_1p1G)
				vm_millivolts =
					vm_mv_1088a0_svc_1p2G[uiprofile];
			else
				vm_millivolts =
					vm_mv_1088a0_svc_1p1G[uiprofile];

			component_freqs = freqs_cmb_1088a0;
		} else {
			if (max_freq > CORE_1p18G)
				vm_millivolts =
					vm_mv_1088a1_svc_1p3G[uiprofile];
			else if (max_freq > CORE_1p1G)
				vm_millivolts =
					vm_mv_1088a1_svc_1p2G[uiprofile];
			else
				vm_millivolts =
					vm_mv_1088a1_svc_1p1G[uiprofile];
			component_freqs = freqs_cmb_1088a1;
		}
		 /* CP 416M/624M are both using level 2 */
		cp_pmudvc_lvl = msa_pmudvc_lvl = VL2;
	}

	/* z1/z2 and z3/ax use different PPs */
	if (cpu_is_z1z2())
		vm_rail_comp_tbl = vm_rail_comp_tbl_z1z2;
	else {
		vm_rail_comp_tbl = vm_rail_comp_tbl_gpiodvc;
		/*
		 * For GPIO dvc, make sure all voltages can meet
		 * CP requirement.
		 */
		if (!dvc_flag) {
			min_cp_millivolts =
				max(vm_millivolts[cp_pmudvc_lvl],
					vm_millivolts[msa_pmudvc_lvl]);
			for (i = 0; i < VL_MAX; i++)
				if (vm_millivolts[i] < min_cp_millivolts)
					vm_millivolts[i] = min_cp_millivolts;
		}
	}
	dlg_trace("dvc_flag[%d]\n", dvc_flag);
	if (dvc_flag) {
		/* update pmu dvc voltage table by max freq table */
		update_all_pmudvc_rails_info();
		/*
		 * init voltage value for voltage_millivolts
		 * Currently set it the same as svc table
		 */
		for (i = 0; i < MAX_RAIL_NUM; i++)
			for (j = 1; j <= VL_MAX; j++)
				voltage_millivolts[i][j] = vm_millivolts[j - 1];

		if (dvc_pin_switch) {
			/* Need to swap level 1 and level 2's voltage */
			i = vm_millivolts[1];
			vm_millivolts[1] = vm_millivolts[2];
			vm_millivolts[2] = i;
		}
	}
#if defined(CONFIG_D2199_DVC)
	ramp_up_vol = (vm_millivolts[VL3] - vm_millivolts[VL2]) * 10;
	dlg_trace("ramp_up_vol[%d]\n", ramp_up_vol);
#endif
	return 0;
}
core_initcall_sync(setup_dvfs_platinfo);

unsigned int pxa988_get_vl_num(void)
{
	unsigned int i = 0;
	while (i < VL_MAX && vm_millivolts[i])
		i++;
	return i;
};

unsigned int pxa988_get_vl(unsigned int vl)
{
	return vm_millivolts[vl];
};

static void update_component_voltage(unsigned int comp,
	unsigned int num_volt)
{
	int i, j = 0;

	for (i = 0; i < num_volt; i++) {
		component_voltage[comp][AP_ACTIVE][i] = LEVEL0 + i;
	}

	if (has_feat_dvc_M2D1Pignorecore() && (comp == CORE)) {
		for (i = AP_LPM; i <= APSUB_IDLE; i++)
			for (j = 0; j < num_volt; j++)
				component_voltage[comp][i][j] = LEVEL0;
	} else {
		for (i = AP_LPM; i <= APSUB_IDLE; i++)
			for (j = 0; j < num_volt; j++)
				component_voltage[comp][i][j] =
					component_voltage[comp][AP_ACTIVE][j];
	}

	/* all Components have no request for APSUB_SLEEP */
	for (j = 0; j < num_volt; j++)
		component_voltage[comp][APSUB_SLEEP][j] = LEVEL0;
}

static void update_all_pmudvc_rails_info(void)
{
	unsigned int num_volt, idx;

	/* Update all rails corresponding voltage */
	num_volt = pxa988_get_vl_num();
	for (idx = 0; idx < VM_RAIL_MAX; idx++)
		update_component_voltage(idx, num_volt);
}

static struct dvfs *vcc_main_dvfs_init
	(struct dvfs_rail_component *dvfs_component, int factor)
{
	struct dvfs *vm_dvfs = NULL;
	struct vol_table *vt = NULL;
	int i;
	unsigned int vl_num = 0;
	const char *clk_name;

	dlg_trace("factor[%d], dvc_flag[%d]\n", factor, dvc_flag);
	/* dvfs is not enabled for this factor in vcc_main_threshold */
	if (!dvfs_component[factor].auto_dvfs)
		goto err;

	clk_name = dvfs_component[factor].clk_name;

	vm_dvfs = kzalloc(sizeof(struct dvfs), GFP_KERNEL);
	if (!vm_dvfs) {
		pr_err("failed to request mem for vcc_main dvfs\n");
		goto err;
	}

	vl_num = pxa988_get_vl_num();

	vt = kzalloc(sizeof(struct vol_table) * vl_num, GFP_KERNEL);
	if (!vt) {
		pr_err("failed to request mem for vcc_main vol table\n");
		goto err_vt;
	}

	for (i = 0; i < vl_num; i++) {
		vt[i].freq = dvfs_component[factor].freqs[i] * \
			dvfs_component[factor].freqs_mult;
		vt[i].millivolts = dvfs_component[factor].millivolts[i];
		dlg_trace("clk[%s] rate[%lu] volt[%d]\n", clk_name, vt[i].freq,
					vt[i].millivolts);
	}
	vm_dvfs->vol_freq_table = vt;
	vm_dvfs->clk_name = clk_name;
	vm_dvfs->num_freqs = vl_num;
	vm_dvfs->dvfs_rail = dvfs_component[factor].dvfs_rail;

	dvfs_component[factor].clk_node =
		clk_get_sys(NULL, clk_name);
	dvfs_component[factor].dvfs = vm_dvfs;

	dlg_trace("end of function\n");
	return vm_dvfs;
err_vt:
	kzfree(vm_dvfs);
	vm_dvfs = NULL;
err:
	return vm_dvfs;
}

static int get_frequency_from_dvfs(struct dvfs *dvfs, int millivolts)
{
	unsigned long max_freq = 0;
	int i;
	dlg_trace("millivolts[%d]\n", millivolts);
	for (i = 0; i < dvfs->num_freqs; i++) {
		if (dvfs->vol_freq_table[i].millivolts == millivolts) {
			if (dvfs->vol_freq_table[i].freq > max_freq)
				max_freq = dvfs->vol_freq_table[i].freq;
		}
	}

	dlg_trace("max_freq[%d]\n", max_freq);
	return max_freq;
}

static int global_notifier(struct dvfs *dvfs, int state,
			   int old_rate, int new_rate)
{
	struct dvfs_freqs freqs;
	dlg_trace("state[%d], old_rate[%d], new_rate[%d]\n", state, old_rate, new_rate);
	freqs.old = old_rate / KHZ_TO_HZ;
	freqs.new = new_rate / KHZ_TO_HZ;
	freqs.dvfs = dvfs;
	dvfs_notifier_frequency(&freqs, state);
	return 0;
}

static DEFINE_MUTEX(solve_lock);
/*
 * dvfs_update_rail will call dvfs_solve_relationship when the rail's "from"
 * list is not null. So the rail must be the fake rail since fake rail's "from"
 * list is not null(real rail's "from" list is null).
 *
 * Return value is tricky here. it returns 0 always, this return value
 * will be used as millivolts for the fake rail which keeps the millivolts
 * and new_millivolts are both 0, which consequently prevent fake rail from
 * calling its own dvfs_rail_set_voltage in dvfs_update_rail. so the fake
 * rail's state is constant.
 *
*/
static int vcc_main_solve(struct dvfs_rail *from, struct dvfs_rail *to)
{
	int old_millivolts, new_millivolts;
	struct dvfs *temp;
	/* whether frequency change is behind voltage change */
	static int is_after_change;
	if (from == NULL) {
		printk(KERN_ERR "The \"from\" part of the relation is NULL\n");
		return 0;
	}

	dlg_trace("millivolts[%d], new_millivolts[%d]\n", from->millivolts, from->new_millivolts);
	mutex_lock(&solve_lock);

	old_millivolts = from->millivolts;
	new_millivolts = from->new_millivolts;

	/*
	 * this function is called before and after the real rail's
	 * set voltage function.
	 * a: when it is called before voltage change:
	 * 1) if old < new,
	 *    then we can't increase the frequencies of each component
	 *    since voltage is not raised yet. return directly here.
	 * 2) if old > new, then we lower the components's frequency
	 * 3) if old == new, return directly
	 * b: when it is called after frequency change, then there is only
	 * one condition, that is old == new.
	 * 1) previous voltage change is raise voltage, then raise
	 *    frequencies of each component.
	 * 2) previous voltage is not changed or lowered down, return directly
	*/
	if (old_millivolts < new_millivolts) {
		is_after_change = 1;
		mutex_unlock(&solve_lock);
		return 0;
	} else if (old_millivolts == new_millivolts) {
		if (!is_after_change) {
			mutex_unlock(&solve_lock);
			return 0;
		}
	}
	pr_debug("Voltage from %d to %d\n", old_millivolts, new_millivolts);
	list_for_each_entry(temp, &from->dvfs, dvfs_node) {
		struct clk *cur_clk = clk_get(NULL, temp->clk_name);
		if ((cur_clk->refcnt > 0) && (cur_clk->is_combined_fc)) {
			unsigned long frequency =
				get_frequency_from_dvfs(temp, new_millivolts);
			int cur_freq = cur_clk->ops->getrate(cur_clk);
			pr_debug("clock: %s 's rate is set from %ld to %ld Hz,"
				 " millivolts: %d\n", temp->clk_name,
				 cur_clk->rate, frequency, temp->millivolts);
			global_notifier(temp, DVFS_FREQ_PRECHANGE,
					cur_freq, frequency);
			cur_clk->ops->setrate(cur_clk, frequency);
			global_notifier(temp, DVFS_FREQ_POSTCHANGE,
					cur_freq, frequency);
			cur_clk->rate = cur_clk->ops->getrate(cur_clk);
		}
	}
	is_after_change = 0;
	mutex_unlock(&solve_lock);
	return 0;
}

/*
 * Fake rail, use relationship to implement dvfs based dvfm
 * only define reg_id to make rail->reg as non-null after
 * rail is connected to regulator, all other components are 0
 * and should be kept as 0 in further operations.
 */
static struct dvfs_rail pxa988_dvfs_rail_vm_dup = {
	.reg_id = "vcc_main",
};

/*
 * Real rail is the "from" in the relationship, its "to" list
 * contains fake rail, but it has no "from" list.
 * Fake rail is the "to" in the relationship. its "from" list
 * contains real rail, but it has no "to" list.
*/
static struct dvfs_relationship pxa988_dvfs_relationships[] = {
	{
		.from = &pxa988_dvfs_rail_vm,
		.to = &pxa988_dvfs_rail_vm_dup,
		.solve = vcc_main_solve,
	},
};

static struct dvfs_rail *pxa988_dvfs_rails[] = {
	&pxa988_dvfs_rail_vm,
	&pxa988_dvfs_rail_vm_dup,
};

/*
 * "from" is the lpm rails, "to" is active rail
*/
static int vcc_main_solve_lpm(struct dvfs_rail *from, struct dvfs_rail *to)
{
	int millivolts = 0;
	struct dvfs *d_to, *d_from, *d;
	struct list_head *list_from, *list_to;
	struct regulator *ori_reg = to->reg;

	to->reg = NULL; /* hack here to avoid recursion of dvfs_rail_update */
	//dlg_trace("\n");

	for (list_from = from->dvfs.next, list_to = to->dvfs.next;
	     (list_from != &from->dvfs) && (list_to != &to->dvfs);
	     list_from = list_from->next, list_to = list_to->next) {
		d_to = list_entry(list_to, struct dvfs, dvfs_node);
		d_from = list_entry(list_from, struct dvfs, dvfs_node);
		if (has_feat_dvc_M2D1Pignorecore()) {
			/*
			 * CPU won't request voltage on lpm rails
			 * skip it from comparison
			*/
			if (!strcmp(d_to->clk_name, "cpu") &&
			    !strcmp(d_from->clk_name, "cpu"))
				continue;
		}
		if (d_to->millivolts != d_from->millivolts) {
			d_from->millivolts = d_to->millivolts;
			dvfs_rail_update(from);
			break;
		}
	}
	/* Solve function's return value will be used as the new_millivolts
	 * for active rail, as our solve function will not affect active rail's
	 * voltage, so only get the max value of each dvfs under active rail
	*/
	list_for_each_entry(d, &to->dvfs, dvfs_node)
		millivolts = max(d->millivolts, millivolts);

	to->reg = ori_reg;
	//dlg_trace("millivolts[%d]\n", millivolts);

	return millivolts;
}

static struct dvfs_relationship pxa988_dvfs_relationships_pmudvc[] = {
	{
		.from = &pxa988_dvfs_rail_ap_lpm,
		.to = &pxa988_dvfs_rail_ap_active,
		.solve = vcc_main_solve_lpm,
	},
	{
		.from = &pxa988_dvfs_rail_apsub_idle,
		.to = &pxa988_dvfs_rail_ap_active,
		.solve = vcc_main_solve_lpm,
	},
};

static void __init enable_ap_dvc(void)
{
	int value;
	dlg_trace("\n");
	value = __raw_readl(PMUM_DVCR);
#ifdef CONFIG_MFD_D2199
	value |= (DVCR_VC_EN);
#else
	value |= (DVCR_VC_EN | DVCR_LPM_AVC_EN);
#endif
	__raw_writel(value, PMUM_DVCR);

	value = __raw_readl(PMUM_DVC_AP);
	value |= DVC_AP_LPM_AVC_EN;
	__raw_writel(value, PMUM_DVC_AP);

	value = __raw_readl(PMUM_DVC_IMR);
	value |= AP_VC_DONE_INTR_MASK;
	__raw_writel(value, PMUM_DVC_IMR);

	value = __raw_readl(PMUM_DVC_APSUB);
	value |= (nUDR_AP_SLP_AVC_EN | AP_IDLE_DDRON_AVC_EN);
	__raw_writel(value, PMUM_DVC_APSUB);

	value = __raw_readl(PMUM_DVC_CHIP);
	value |= (UDR_SLP_AVC_EN | nUDR_SLP_AVC_EN);
	__raw_writel(value, PMUM_DVC_CHIP);
}

/* vote active and LPM voltage level request for CP */
static void __init enable_cp_dvc(void)
{
	unsigned int value, mask, lvl;

	dlg_trace("\n");
	/*
	 * cp_pmudvc_lvl and msa_pmudvc_lvl is set up during init,
	 * default as VL1
	 */

	/*
	 * Vote CP active cp_pmudvc_lvl and LPM VL0
	 * and trigger CP frequency request
	 */
	value = __raw_readl(dvc_reg_table[DVC_CP].reg);
	mask = (dvc_reg_table[DVC_CP].mask_H) | \
		(dvc_reg_table[DVC_CP].mask_L);
	value &= ~mask;
	lvl = (cp_pmudvc_lvl << dvc_reg_table[DVC_CP].offset_H) |\
		((LEVEL0 - LEVEL0) << dvc_reg_table[DVC_CP].offset_L);
	value |= lvl | DVC_CP_LPM_AVC_EN | \
		(1 << dvc_reg_table[DVC_CP].offset_trig);
	__raw_writel(value, dvc_reg_table[DVC_CP].reg);

	/*
	 * Vote MSA active msa_pmudvc_lvl and LPM VL0
	 * and trigger MSA frequency request
	 */
	value = __raw_readl(dvc_reg_table[DVC_DP].reg);
	mask = (dvc_reg_table[DVC_DP].mask_H) | \
		(dvc_reg_table[DVC_DP].mask_L);
	value &= ~mask;
	lvl = (msa_pmudvc_lvl << dvc_reg_table[DVC_DP].offset_H) |\
		((LEVEL0 - LEVEL0) << dvc_reg_table[DVC_DP].offset_L);
	value |= lvl | DVC_DP_LPM_AVC_EN | \
		(1 << dvc_reg_table[DVC_DP].offset_trig);
	__raw_writel(value, dvc_reg_table[DVC_DP].reg);

	/* unmask cp/msa DVC done int */
	value = __raw_readl(PMUM_DVC_IMR);
	value |= (CP_VC_DONE_INTR_MASK | DP_VC_DONE_INTR_MASK);
	__raw_writel(value, PMUM_DVC_IMR);
}

static int __init pxa988_init_dvfs(void)
{
	int i, ret, j, r;
	struct dvfs *d;
	struct clk *c;
	unsigned long rate;

	dlg_trace("dvc_flag[%d]\n", dvc_flag);
	if (!dvc_flag) {
		dvfs_init_rails(pxa988_dvfs_rails, ARRAY_SIZE(pxa988_dvfs_rails));
		for (i = 0; i < VM_RAIL_MAX; i++) {
			vm_rail_comp_tbl[i].freqs = component_freqs[i];
			vm_rail_comp_tbl[i].millivolts = vm_millivolts;
			d = vcc_main_dvfs_init(vm_rail_comp_tbl, i);
			if (!d)
				continue;
			c = vm_rail_comp_tbl[i].clk_node;
			if (!c) {
				pr_err("pxa988_dvfs: no clock found for %s\n",
					d->clk_name);
				kzfree(d->vol_freq_table);
				kzfree(d);
				continue;
			}
			ret = enable_dvfs_on_clk(c, d);
			if (ret) {
				pr_err("pxa988_dvfs: failed to enable dvfs on %s\n",
					c->name);
				kzfree(d->vol_freq_table);
				kzfree(d);
			}
			/*
			* adjust the voltage request according to its current rate
			* for those clk is always on
			*/
			if (c->refcnt) {
				rate = clk_get_rate(c);
				j = 0;
				while (j < d->num_freqs && rate > d->vol_freq_table[j].freq)
					j++;
				d->millivolts = d->vol_freq_table[j].millivolts;
			}
		}
	} else {
		struct dvfs_rail_component *rail_component;
		dvfs_init_rails(pxa988_dvfs_rails_pmudvc,
			ARRAY_SIZE(pxa988_dvfs_rails_pmudvc));
		dvfs_add_relationships(pxa988_dvfs_relationships_pmudvc,
			ARRAY_SIZE(pxa988_dvfs_relationships_pmudvc));
		init_level_mapping();
		enable_ap_dvc();

		for (r = 0; r < ARRAY_SIZE(pxa988_dvfs_rails_pmudvc); r++) {
			rail_component = dvfs_rail_component_list[r];
			for (i = 0; i < VM_RAIL_MAX; i++) {
				rail_component[i].freqs = component_freqs[i];
				vm_rail_comp_tbl[i].millivolts = vm_millivolts;
				d = vcc_main_dvfs_init(rail_component, i);
				if (!d)
					continue;
				c = rail_component[i].clk_node;
				if (!c) {
					pr_err("pxa988_dvfs: no clock found for %s\n",
						d->clk_name);
					kzfree(d->vol_freq_table);
					kzfree(d);
					continue;
				}
				if (r == 0) { /* only allow active rail's dvfs connect to clock */
					ret = enable_dvfs_on_clk(c, d);
					if (ret) {
						pr_err("pxa988_dvfs: failed to enable dvfs on %s\n",
							c->name);
						kzfree(d->vol_freq_table);
						kzfree(d);
					}
				} else
					list_add_tail(&d->dvfs_node, &d->dvfs_rail->dvfs);
				/*
				* adjust the voltage request according to its current rate
				* for those clk is always on
				*/
				if (c->refcnt) {
					rate = clk_get_rate(c);
					j = 0;
					while (j < d->num_freqs && rate > d->vol_freq_table[j].freq)
						j++;
					d->millivolts = d->vol_freq_table[j].millivolts;
				}
			}
		}
	}

	return 0;
}
subsys_initcall(pxa988_init_dvfs);

static int __init pxa988_init_level_volt(void)
{
	int i, value, ticks;
	u8 data;
	dlg_trace("dvc_flag[%d]\n", dvc_flag);
	if (!dvc_flag)
		return 0;

	/* Write level 0 svc values, level 1~3 are written after pm800 init */
	value = set_buck1_volt(0, volt_to_reg(vm_millivolts[0]));
	if (value < 0) {
		printk(KERN_ERR "SVC table writting failed !\n");
		return -1;
	}
#ifdef CONFIG_MACH_WILCOX_CMCC
#if defined(CONFIG_MFD_D2199)	
     if (get_board_id() <BOARD_ID_CMCC_REV05) {  
	/* set ldo6 to be 1.7v only for rB1 Mynah Tranceiver */
	d2199_extern_reg_read(D2199_LDO6_REG, &data);
	data &= ~(0x3f);
	data |= 0xa;
	d2199_extern_reg_write(D2199_LDO6_REG,data);
    }
#endif
#endif
	for (i = 0; i < PMIC_LEVEL_NUM; i++) {
		cur_level_volt[i] = get_voltage_value(i);
		pr_info("PMIC level %d: %d mV\n", i, cur_level_volt[i]);
	}

	enable_cp_dvc();

	/* Get the current level information */
	for (i = DVC_AP; i < DVC_END; i++) {
		value = __raw_readl(dvc_reg_table[i].reg);
		current_volt_table[i].volt_level_H =
			(value & dvc_reg_table[i].mask_H)
			>> (dvc_reg_table[i].offset_H);
		current_volt_table[i].volt_level_L =
			(value & dvc_reg_table[i].mask_L)
			>> (dvc_reg_table[i].offset_L);
		pr_info("DVC Reg %d, volt high: %d, volt low: %d\n", i,
			current_volt_table[i].volt_level_H,
			current_volt_table[i].volt_level_L);
	}
	cur_volt_inited = 1;

	if (dvc_pin_switch) {
		value = cur_level_volt[1];
		cur_level_volt[1] = cur_level_volt[2];
		cur_level_volt[2] = value;
	}

	/* Fill the stable time for level transition
	 * As pmic only supports 4 levels, so only init
	 * 0->1, 1->2, 2->3 's stable time.
	 */
	value = __raw_readl(PMUM_VL01STR);
	value &= ~VLXX_ST_MASK;
	ticks = get_stable_ticks(cur_level_volt[0], cur_level_volt[1]);
	value |= ticks;
	__raw_writel(value, PMUM_VL01STR);

	value = __raw_readl(PMUM_VL12STR);
	value &= ~VLXX_ST_MASK;
	ticks = get_stable_ticks(cur_level_volt[1], cur_level_volt[2]);
	value |= ticks;
	__raw_writel(value, PMUM_VL12STR);

	value = __raw_readl(PMUM_VL23STR);
	value &= ~VLXX_ST_MASK;
	ticks = get_stable_ticks(cur_level_volt[2], cur_level_volt[3]);
	value |= ticks;
	__raw_writel(value, PMUM_VL23STR);
	stable_time_inited = 1;

	return 0;
}
/* the init must before cpufreq init(module_init)
 * must before dvfs framework init(fs_initcall)
 * must after 88pm800 init(subsys_initcall)
 * must after dvfs init (fs_initcall)
 */
device_initcall(pxa988_init_level_volt);

#ifdef CONFIG_DEBUG_FS
static void attach_clk_auto_dvfs(const char *name, unsigned int endis)
{
	unsigned int i;

	dlg_trace("endis[%d]\n", endis);

	for (i = 0; i < VM_RAIL_MAX; i++) {
		if ((vm_rail_comp_tbl[i].auto_dvfs) && \
			(!strcmp(vm_rail_comp_tbl[i].clk_name, name)))
			break;
	}
	if (i >= VM_RAIL_MAX) {
		pr_err("clk %s doesn't support dvfs\n", name);
		return;
	}

	if (!endis)
		vm_rail_comp_tbl[i].clk_node->dvfs = NULL;
	else
		vm_rail_comp_tbl[i].clk_node->dvfs =
			vm_rail_comp_tbl[i].dvfs;
	pr_info("%s clk %s auto dvfs!\n",
		endis ? "Enable" : "Disable", name);
}

static ssize_t dc_clk_dvfs_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[32] = {0};
	char name[10] = {0};
	unsigned int enable_dvfs = 0;

	dlg_trace("\n");
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (0x2 != sscanf(buf, "%9s %10u", name, &enable_dvfs)) {
		pr_info("[cmd guide]echo clkname(cpu/ddr/GCCLK/VPUCLK) "\
			"enable(0/1) > file node\n");
		return count;
	}
	attach_clk_auto_dvfs(name, enable_dvfs);
	return count;
}

static ssize_t dc_clk_dvfs_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[156];
	int len = 0;
	size_t size = sizeof(buf) - 1;
	struct clk *clk_node;
	unsigned int i;
	const char *clk_name;
	dlg_trace("\n");

	len = snprintf(buf, size, "| name\t| auto_dvfs |\n");
	for (i = 0; i < VM_RAIL_MAX; i++) {
		if (vm_rail_comp_tbl[i].auto_dvfs) {
			clk_name = vm_rail_comp_tbl[i].clk_name;
			clk_node = vm_rail_comp_tbl[i].clk_node;
			len += snprintf(buf + len, size - len,
				"| %s\t| %d |\n", clk_name,
				clk_is_dvfs(clk_node));
		}
	}
	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

/*
 * Disable clk auto dvfs function, only avaiable when
 * has no corresponding clk FC
 */
const struct file_operations dc_clk_dvfs_fops = {
	.write = dc_clk_dvfs_write,
	.read = dc_clk_dvfs_read,
};

static ssize_t voltage_based__dvfm_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[32] = {0};
	int prevalue = enable_voltage_based_dvfm;
	dlg_trace("\n");
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	sscanf(buf, "%10d", &enable_voltage_based_dvfm);

	if (!prevalue && enable_voltage_based_dvfm)
		dvfs_add_relationships(pxa988_dvfs_relationships,
			ARRAY_SIZE(pxa988_dvfs_relationships));
	else if (prevalue && !enable_voltage_based_dvfm)
		dvfs_remove_relationship(&pxa988_dvfs_relationships[0]);

	return count;
}

static ssize_t voltage_based_dvfm_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[20];
	int len = 0;
	size_t size = sizeof(buf) - 1;
	dlg_trace("\n");
	len = snprintf(buf, size, "%d\n", enable_voltage_based_dvfm);
	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

/*
 * Enable/disable voltage based dvfm solution
 */
const struct file_operations voltage_based_dvfm_fops = {
	.write = voltage_based__dvfm_write,
	.read = voltage_based_dvfm_read,
};

static ssize_t voltage_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[1000];
	int len = 0, value, volt1, volt2;
	unsigned int i;
	struct dvfs *d;
	unsigned long rate;

	dlg_trace("\n");

	size_t size = sizeof(buf) - 1;
	value = __raw_readl(PMUM_DVC_AP);
	volt1 = (value >> 4) & 0x7;
	volt2 = value & 0x7;
	len += snprintf(buf + len, size, "|DVC_AP:\t|Active: %d,\t"
			"Lpm:   %d |\n",	volt1, volt2);

	value = __raw_readl(PMUM_DVC_CP);
	volt1 = (value >> 4) & 0x7;
	volt2 = value & 0x7;
	len += snprintf(buf + len, size, "|DVC_CP:\t|Active: %d,\t"
			"Lpm:   %d |\n", volt1, volt2);

	value = __raw_readl(PMUM_DVC_DP);
	volt1 = (value >> 4) & 0x7;
	volt2 = value & 0x7;
	len += snprintf(buf + len, size, "|DVC_DP:\t|Active: %d,\t"
			"Lpm:   %d |\n",	volt1, volt2);

	value = __raw_readl(PMUM_DVC_APSUB);
	volt1 = (value >> 8) & 0x7;
	volt2 = value & 0x7;
	len += snprintf(buf + len, size, "|DVC_APSUB:\t|IDLE:   %d,\t"
			"SLEEP: %d |\n",	volt2, volt1);

	value = __raw_readl(PMUM_DVC_CHIP);
	volt1 = (value >> 4) & 0x7;
	volt2 = value & 0x7;
	len += snprintf(buf + len, size, "|DVC_CHIP:\t|UDR:    %d,\t"
			"nUDR:  %d |\n",	volt1, volt2);

	value = __raw_readl(PMUM_DVC_STATUS);
	volt1 = (value >> 1) & 0x7;

	len += snprintf(buf + len, size, "|DVC Voltage:    Level %d ",
			volt1);

	if (dvc_pin_switch) {
		if (volt1 == 1)
			volt1 = 2;
		else if (volt1 == 2)
			volt1 = 1;
	}
//	volt1 = 1250;
	dlg_trace("volt1[%d]\n", volt1);
	volt1 = get_buck1_volt(volt1);
	volt1 = reg_to_volt(volt1);
	len += snprintf(buf + len, size, "(%d mV)\t\t  |\n", volt1);

	for (i = 0; i < VM_RAIL_MAX; i++) {
		if (vm_rail_ap_active_tbl[i].auto_dvfs) {
			d = vm_rail_ap_active_tbl[i].clk_node->dvfs;
			rate = clk_get_rate(vm_rail_ap_active_tbl[i].clk_node);
			if (d->millivolts > 0) {
				len += snprintf(buf + len, size,
				"|%-15s| freq %luMhz,\t voltage: Level %d |\n",
				vm_rail_ap_active_tbl[i].clk_name,
				rate / 1000000, level_mapping[d->millivolts]);
			} else {
				len += snprintf(buf + len, size,
				"|%-15s| freq %luMhz,\t voltage: %d |\n",
				vm_rail_ap_active_tbl[i].clk_name,
				rate / 1000000, d->millivolts);
			}
		}
	}

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}


/* Get current voltage for each power mode */
const struct file_operations voltage_fops = {
	.read = voltage_read,
};


static int __init pxa988_dvfs_create_debug_node(void)
{
	struct dentry *dvfs_node;
	struct dentry *dc_dvfs;
	struct dentry *volt_dvfs;
	struct dentry *volt_status;

	dlg_trace("\n");
	dvfs_node = debugfs_create_dir("dvfs", pxa);
	if (!dvfs_node)
		return -ENOENT;

	dc_dvfs = debugfs_create_file("dc_clk_dvfs", 0664,
		dvfs_node, NULL, &dc_clk_dvfs_fops);
	if (!dc_dvfs)
		goto err_dc_dvfs;
	volt_dvfs = debugfs_create_file("enable_volt_based_dvfm", 0664,
		dvfs_node, NULL, &voltage_based_dvfm_fops);
	if (!volt_dvfs)
		goto err_volt_dvfs;
	if (dvc_flag) {
		volt_status = debugfs_create_file("voltage", 0444,
			dvfs_node, NULL, &voltage_fops);
		if (!volt_status)
			goto err_voltage;
	}

	return 0;

err_voltage:
	debugfs_remove(volt_dvfs);
err_volt_dvfs:
	debugfs_remove(dc_dvfs);
err_dc_dvfs:
	debugfs_remove(dvfs_node);
	return -ENOENT;
}
late_initcall(pxa988_dvfs_create_debug_node);
#endif
