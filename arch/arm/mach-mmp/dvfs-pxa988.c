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
#include <linux/mfd/88pm80x.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <mach/regs-mpmu.h>
#include <mach/cputype.h>
#include <mach/clock-pxa988.h>
#include <plat/clock.h>
#include <mach/dvfs.h>
#include <plat/debugfs.h>

int dvc_flag = 1;
EXPORT_SYMBOL(dvc_flag);

static int __init dvc_flag_setup(char *str)
{
	int n;
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

/* voltage tbl is sort ascending, it's default setting for Z1 */
static int vm_millivolts[VL_MAX] = {
	1150, 1238, 1250
};

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
	{1150, 1150, 1325, 1350},	/* profile 4 */
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
	/* PP <= 312, PP<=624, PP<=1066, PP<=1205 */
	{1050, 1100, 1275, 1350},       /* profile 0 */
	{1050, 1100, 1125, 1188},       /* profile 1 */
	{1050, 1100, 1125, 1200},       /* profile 2 */
	{1050, 1100, 1138, 1213},       /* profile 3 */
	{1050, 1100, 1150, 1238},       /* profile 4 */
	{1050, 1100, 1175, 1263},       /* profile 5 */
	{1050, 1100, 1200, 1288},       /* profile 6 */
	{1050, 1138, 1225, 1313},       /* profile 7 */
	{1050, 1138, 1275, 1350},       /* profile 8 */

};

/* 986 Ax SVC table, CP 624M vote VL2 */
static int vm_millivolts_986ax_svc[PROFILE_NUM][VL_MAX] = {
	/* PP <= 312, PP<=624, PP<=1066, PP<=1205 */
	{1050, 1138, 1275, 1350},	/* profile 0 */
	{1050, 1100, 1100, 1200},	/* profile 1 */
	{1050, 1100, 1113, 1200},	/* profile 2 */
	{1050, 1100, 1138, 1213},	/* profile 3 */
	{1050, 1100, 1150, 1238},	/* profile 4 */
	{1050, 1100, 1175, 1263},	/* profile 5 */
	{1050, 1100, 1200, 1288},	/* profile 6 */
	{1050, 1138, 1225, 1313},	/* profile 7 */
	{1050, 1138, 1275, 1350},	/* profile 8 */
};
/* default CP/MSA required PMU DVC VL */
static unsigned int cp_pmudvc_lvl = VL1;
static unsigned int msa_pmudvc_lvl = VL1;

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
	return (millivolts - 600) * 10 / 125;
}

static inline int reg_to_volt(int value)
{
	/* Round up to int value, eg, 1287.5 ==> 1288 */
	return (value * 125 + 6000 + 9) / 10;

}

static int get_stable_ticks(int millivolts1, int millivolts2)
{
	int max, min, ticks;
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
	ret = pm800_extern_write(PM80X_POWER_PAGE,
			PM800_BUCK1 + level, regval);
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
	reg = pm800_extern_read(PM80X_POWER_PAGE, PM800_BUCK1 + level);
	if (reg < 0) {
		printk(KERN_ERR "PMIC voltage reading failed !\n");
		return -1;
	}
	value = reg_to_volt(reg);
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
	int volt = voltage_millivolts[buck_id - PM800_ID_BUCK1_AP_ACTIVE][*level];

	if (cur_volt_inited && (cur_level_volt[pmic_level] != volt)) {
		set_voltage_value(pmic_level, volt);
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
	return 0;
}

int dvc_set_voltage(int buck_id, int level)
{
	unsigned int reg_val;
	int reg_index = 0, high_bits; /* If this is high bits voltage */
	/*
	 * Max delay time, unit is us. (1.5v - 1v) / 0.125v = 40
	 * Also PMIC needs 10us to launch and sync dvc pins
	 * default delay should be 0xFFFF * 3 ticks(L0-->L3 or L3-->L0)
	 */
	int max_delay;
	if (stable_time_inited)
		max_delay = 40 + 10 * 3; /* pmic sync time may be accumulated */
	else
		max_delay = DIV_ROUND_UP(0xFFFF * 3, 26);

	if ((buck_id < PM800_ID_BUCK1_AP_ACTIVE) || (buck_id > PM800_ID_BUCK1_APSUB_SLEEP)) {
		printk(KERN_ERR "Not new dvc regulator, Can't use %s\n", __func__);
		return 0;
	}
	replace_level_voltage(buck_id, &level);
	if (buck_id == PM800_ID_BUCK1_AP_ACTIVE) {
		reg_index = DVC_AP;
		high_bits = 1;
	} else if (buck_id == PM800_ID_BUCK1_AP_LPM) {
		reg_index = DVC_AP;
		high_bits = 0;
	} else if (buck_id == PM800_ID_BUCK1_APSUB_IDLE) {
		reg_index = DVC_APSUB;
		high_bits = 0;
	} else if (buck_id == PM800_ID_BUCK1_APSUB_SLEEP) {
		reg_index = DVC_APSUB;
		high_bits = 1;
	}

	if (!dvc_reg_table[reg_index].allow_ap) {
		printk(KERN_ERR "AP can't set this register !\n");
		return 0;
	}

	if (high_bits) {
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
		__raw_writel(reg_val, dvc_reg_table[reg_index].reg);
		/* Only AP Active voltage change needs polling */
		if (buck_id == PM800_ID_BUCK1_AP_ACTIVE) {
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
		current_volt_table[reg_index].volt_level_H = level;
	} else {
		/* Set low bits voltage, no need to poll status */
		if (current_volt_table[reg_index].volt_level_L == level)
			return 0;
		reg_val = __raw_readl(dvc_reg_table[reg_index].reg);
		reg_val &= ~dvc_reg_table[reg_index].mask_L;
		reg_val |= (level << dvc_reg_table[reg_index].offset_L);
		__raw_writel(reg_val, dvc_reg_table[reg_index].reg);
		current_volt_table[reg_index].volt_level_L = level;
	}
	return 0;
}
EXPORT_SYMBOL(dvc_set_voltage);

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

#define INIT_DVFS(_clk_name, _auto, _rail, _millivolts,  _freqs)	\
	{								\
		.clk_name	= _clk_name,				\
		.auto_dvfs	= _auto,				\
		.millivolts	= _millivolts,				\
		.freqs_mult	= KHZ_TO_HZ,				\
		.dvfs_rail	= _rail,				\
		.freqs		= _freqs,				\
	}

static unsigned long freqs_cmb_z1z2[VM_RAIL_MAX][VL_MAX] = {
	{ 600000, 800000, 1200000 },	/* CORE */
	{ 300000, 400000, 400000 },	/* DDR/AXI */
	{ 600000, 600000, 600000 },	/* GC */
	{ 400000, 400000, 400000 }	/* VPU */
};

static struct dvfs_rail_component *vm_rail_comp_tbl;
static struct dvfs_rail_component vm_rail_comp_tbl_z1z2[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_z1z2[CORE]),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_z1z2[DDR_AXI]),
	INIT_DVFS("GCCLK", false, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_z1z2[GC]),
	INIT_DVFS("VPUCLK", false, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_z1z2[VPU]),
};

static unsigned long freqs_cmb_z3[VM_RAIL_MAX][VL_MAX] = {
	{ 312000, 624000, 1066000, 1205000 },	/* CORE */
	{ 312000, 312000, 533000, 533000 },	/* DDR/AXI */
	{ 0	, 416000, 624000, 624000 },	/* GC */
	{ 208000, 312000, 416000, 416000 }	/* VPU */
};

static struct dvfs_rail_component vm_rail_comp_tbl_z3[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_z3[CORE]),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_z3[DDR_AXI]),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_z3[GC]),
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_z3[VPU]),
};

static unsigned long freqs_cmb_ax[VM_RAIL_MAX][VL_MAX] = {
	{ 624000, 624000, 1066000, 1205000 },	/* CORE */
	{ 312000, 312000, 312000, 533000 },	/* DDR/AXI */
	{ 0, 416000, 624000, 624000 },	/* GC */
	{ 0, 312000, 416000, 416000 }	/* VPU */
};

static struct dvfs_rail_component vm_rail_comp_tbl_ax[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_ax[CORE]),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_ax[DDR_AXI]),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_ax[GC]),
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_vm, vm_millivolts,
		  freqs_cmb_ax[VPU]),
};

static void update_all_component_voltage(void);

/* init the voltage and rail/frequency tbl according to platform info */
static int __init setup_dvfs_platinfo(void)
{
	unsigned int uiprofile = get_profile();
	int i, j;
	int min_cp_millivolts = 0;

	/* z1/z2/z3/Ax will use different voltage */
	if (cpu_is_z1z2()) {
		memcpy(&vm_millivolts, &vm_millivolts_z1z2,\
			sizeof(int) * ARRAY_SIZE(vm_millivolts));
		dvc_flag = 0;
	} else if (cpu_is_pxa988_z3()) {
		memcpy(&vm_millivolts, &vm_millivolts_988z3_svc[uiprofile],\
			sizeof(int) * ARRAY_SIZE(vm_millivolts));
		dvc_flag = 0;
	} else if (cpu_is_pxa986_z3()) {
		memcpy(&vm_millivolts, &vm_millivolts_986z3_svc[uiprofile],\
			sizeof(int) * ARRAY_SIZE(vm_millivolts));
		dvc_flag = 0;
	} else if (cpu_is_pxa988_a0()) {
		if (is_pxa988a0svc) {
			memcpy(&vm_millivolts,\
				&vm_millivolts_988ax_svc[uiprofile],\
				sizeof(int) * ARRAY_SIZE(vm_millivolts));
		} else {
			memcpy(&vm_millivolts,\
				&vm_millivolts_988z3_svc[uiprofile],\
				sizeof(int) * ARRAY_SIZE(vm_millivolts));
		}
		/*
		 * 988 A0 CP/MSA/TD modem requires at least VL1 1.1V
		 */
		cp_pmudvc_lvl = msa_pmudvc_lvl = VL1;
	} else if (cpu_is_pxa986_a0()) {
		if (is_pxa988a0svc) {
			memcpy(&vm_millivolts,\
				&vm_millivolts_986ax_svc[uiprofile],\
			sizeof(int) * ARRAY_SIZE(vm_millivolts));
		} else {
			memcpy(&vm_millivolts,\
				&vm_millivolts_986z3_svc[uiprofile],\
				sizeof(int) * ARRAY_SIZE(vm_millivolts));
		}
		/*
		 * 986 A0 CP/MSA/TD modem requires at least VL2
		 */
		cp_pmudvc_lvl = msa_pmudvc_lvl = VL2;
	}

	/* z1/z2 and z3/ax use different PPs */
	if (cpu_is_z1z2())
		vm_rail_comp_tbl = vm_rail_comp_tbl_z1z2;
	else if (cpu_is_pxa988_z3() || cpu_is_pxa986_z3())
		vm_rail_comp_tbl = vm_rail_comp_tbl_z3;
	else {
		vm_rail_comp_tbl = vm_rail_comp_tbl_ax;
		/*
		 * For GPIO dvc, make sure all voltages can meet
		 * CP requirement.
		 */
		if (!dvc_flag) {
			min_cp_millivolts =
				max(vm_millivolts[cp_pmudvc_lvl],
					vm_millivolts[msa_pmudvc_lvl]);
			for (i = 0; i < ARRAY_SIZE(vm_millivolts); i++)
				if (vm_millivolts[i] < min_cp_millivolts)
					vm_millivolts[i] = min_cp_millivolts;
		}
	}
	if (dvc_flag) {
		/* update pmu dvc voltage table by max freq table */
		update_all_component_voltage();
		/*
		 * init voltage value for voltage_millivolts
		 * Currently set it the same as svc table
		 */
		for (i = 0; i < MAX_RAIL_NUM; i++)
			for (j = 1; j <= ARRAY_SIZE(vm_millivolts); j++)
				voltage_millivolts[i][j] = vm_millivolts[j - 1];
#ifdef	DVC_WR
		/* Need to swap level 1 and level 2's voltage */
		i = vm_millivolts[1];
		vm_millivolts[1] = vm_millivolts[2];
		vm_millivolts[2] = i;
#endif
	}
	return 0;
}
core_initcall_sync(setup_dvfs_platinfo);

static struct dvfs_rail pxa988_dvfs_rail_ap_active = {
	.reg_id = "vcc_main_ap_active",
	.max_millivolts = LEVEL3,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL0,
	.step = 0xFF,
};

static struct dvfs_rail pxa988_dvfs_rail_ap_lpm = {
	.reg_id = "vcc_main_ap_lpm",
	.max_millivolts = LEVEL3,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL0,
	.step = 0xFF,
};

static struct dvfs_rail pxa988_dvfs_rail_apsub_idle = {
	.reg_id = "vcc_main_apsub_idle",
	.max_millivolts = LEVEL3,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL0,
	.step = 0xFF,
};

static struct dvfs_rail pxa988_dvfs_rail_apsub_sleep = {
	.reg_id = "vcc_main_apsub_sleep",
	.max_millivolts = LEVEL3,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL0,
	.step = 0xFF,
};

#define MAX_FREQ_NUM	20
/* Each components' freq number should be the same with voltage level number */
static unsigned long component_freqs[VM_RAIL_MAX][MAX_FREQ_NUM] = {
	{ 312000, 624000, 1066000, 1205000 },		/* CORE */
	{ 156000, 312000, 533000 },			/* DDR/AXI */
	{ 156000, 312000, 416000, 624000 },		/* GC */
	{ 156000, 208000, 312000, 416000 }		/* VPU */
};

static unsigned int pxa988_get_freq_num(const char *name)
{
	unsigned int i = 0, index = 0;
	if (!strcmp(name, "cpu"))
		index = CORE;
	else if (!strcmp(name, "ddr"))
		index = DDR_AXI;
	else if (!strcmp(name, "GCCLK"))
		index = GC;
	else if (!strcmp(name, "VPUCLK"))
		index = VPU;
	else
		printk(KERN_ERR "%s: Wrong component name %s\n", __func__, name);

	while (component_freqs[index][i] && i < MAX_FREQ_NUM)
		i++;
	return i;
};

/*
 * one frequency is connected to one voltage level.
 * but one voltage can related to several frequencies
 * for example, cpu frequency list is:
 * 312, 624, 1066, 1205
 * and related voltage is:
 * L0,  L1,  L2,   L3
 * table should be defined as below
 */

static int component_voltage[][MAX_RAIL_NUM][LEVEL_END] = {
		/* 312,  624,    1066,   1205 */
	[CORE] = {
		{LEVEL0, LEVEL0, LEVEL2, LEVEL2,}, /* AP Active voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL0,}, /* AP LPM voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL0,}, /* APSUB idle voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL0,}, /* APSUB sleep voltage */
	},
		/* 156,  312,    533 */
	[DDR_AXI] = {
		{LEVEL0, LEVEL0, LEVEL3,}, /* AP Active voltage */
		{LEVEL0, LEVEL0, LEVEL3,}, /* AP LPM voltage */
		{LEVEL0, LEVEL0, LEVEL3,}, /* APSUB idle voltage */
		{LEVEL0, LEVEL0, LEVEL0,}, /* APSUB sleep voltage */
	},
		/* 156,  312,    416,    624 */
	[GC] = {
		{LEVEL0, LEVEL0, LEVEL0, LEVEL1 }, /* AP Active voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL1 }, /* AP LPM voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL1 }, /* APSUB idle voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL0 }, /* APSUB sleep voltage */
	},
		/* 156,  208,    312,    416 */
	[VPU] = {
		{LEVEL0, LEVEL0, LEVEL0, LEVEL1,}, /* AP Active voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL1,}, /* AP LPM voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL1,}, /* APSUB idle voltage */
		{LEVEL0, LEVEL0, LEVEL0, LEVEL0,}, /* APSUB sleep voltage */
	},
};

static int update_component_voltage(int comp, unsigned long *max_freq_table,
				    int num_volt, unsigned long *freq_table,
				    int num_freq)
{
	int i, j = 0;

	for (i = 0; i < num_volt; i++) {
		if (j == num_freq)
			break;
		for (; j < num_freq; j++) {
			if (freq_table[j] <= max_freq_table[i]) {
				component_voltage[comp][AP_ACTIVE][j] =
					LEVEL0 + i;
				continue;
			} else
				break;
		}
	}
	for (i = AP_LPM; i <= APSUB_IDLE; i++)
		for (j = 0; j < num_freq; j++)
			component_voltage[comp][i][j] =
				component_voltage[comp][AP_ACTIVE][j];
	for (j = 0; j < num_freq; j++)
		component_voltage[comp][APSUB_SLEEP][j] = LEVEL0;
	return 0;
}

static void update_all_component_voltage(void)
{
	int num_volt, num_freq;
	num_volt = pxa988_get_vl_num();
	num_freq = pxa988_get_freq_num("cpu");
	update_component_voltage(CORE, freqs_cmb_ax[CORE], num_volt,
				 component_freqs[CORE], num_freq);
	num_freq = pxa988_get_freq_num("ddr");
	update_component_voltage(DDR_AXI, freqs_cmb_ax[DDR_AXI], num_volt,
				 component_freqs[DDR_AXI], num_freq);
	num_freq = pxa988_get_freq_num("GCCLK");
	update_component_voltage(GC, freqs_cmb_ax[GC], num_volt,
				 component_freqs[GC], num_freq);
	num_freq = pxa988_get_freq_num("VPUCLK");
	update_component_voltage(VPU, freqs_cmb_ax[VPU], num_volt,
				 component_freqs[VPU], num_freq);
}

static struct dvfs_rail_component vm_rail_ap_active_tbl[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_ap_active,
	component_voltage[CORE][AP_ACTIVE], component_freqs[CORE]),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_ap_active,
	component_voltage[DDR_AXI][AP_ACTIVE], component_freqs[DDR_AXI]),
	INIT_DVFS("GCCLK", true,  &pxa988_dvfs_rail_ap_active,
	component_voltage[GC][AP_ACTIVE], component_freqs[GC]),
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_ap_active,
	component_voltage[VPU][AP_ACTIVE], component_freqs[VPU]),
};

static struct dvfs_rail_component vm_rail_ap_lpm_tbl[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_ap_lpm,
	component_voltage[CORE][AP_LPM], component_freqs[CORE]),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_ap_lpm,
	component_voltage[DDR_AXI][AP_LPM], component_freqs[DDR_AXI]),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_ap_lpm,
	component_voltage[GC][AP_LPM], component_freqs[GC]),
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_ap_lpm,
	component_voltage[VPU][AP_LPM], component_freqs[VPU]),
};

static struct dvfs_rail_component vm_rail_apsub_idle_tbl[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_apsub_idle,
	component_voltage[CORE][APSUB_IDLE], component_freqs[CORE]),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_apsub_idle,
	component_voltage[DDR_AXI][APSUB_IDLE], component_freqs[DDR_AXI]),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_apsub_idle,
	component_voltage[GC][APSUB_IDLE], component_freqs[GC]),
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_apsub_idle,
	component_voltage[VPU][APSUB_IDLE], component_freqs[VPU]),
};

static struct dvfs_rail_component vm_rail_apsub_sleep_tbl[VM_RAIL_MAX] = {
	INIT_DVFS("cpu", true, &pxa988_dvfs_rail_apsub_sleep,
	component_voltage[CORE][APSUB_SLEEP], component_freqs[CORE]),
	INIT_DVFS("ddr", true, &pxa988_dvfs_rail_apsub_sleep,
	component_voltage[DDR_AXI][APSUB_SLEEP], component_freqs[DDR_AXI]),
	INIT_DVFS("GCCLK", true, &pxa988_dvfs_rail_apsub_sleep,
	component_voltage[GC][APSUB_SLEEP], component_freqs[GC]),
	INIT_DVFS("VPUCLK", true, &pxa988_dvfs_rail_apsub_sleep,
	component_voltage[VPU][APSUB_SLEEP], component_freqs[VPU]),
};

static struct dvfs_rail_component *dvfs_rail_component_list[] = {
	vm_rail_ap_active_tbl,
	vm_rail_ap_lpm_tbl,
	vm_rail_apsub_idle_tbl,
	vm_rail_apsub_sleep_tbl,
};

unsigned int pxa988_get_vl_num(void)
{
	unsigned int i = 0;
	while ((i < VL_MAX) && vm_millivolts[i])
		i++;
	return i;
};

unsigned int pxa988_get_vl(unsigned int vl)
{
	return vm_millivolts[vl];
};

static struct dvfs *vcc_main_dvfs_init(struct dvfs_rail_component *dvfs_component, int factor)
{
	struct dvfs *vm_dvfs = NULL;
	struct vol_table *vt = NULL;
	int i;
	unsigned int vl_num = 0;
	const char *clk_name;

	/* dvfs is not enabled for this factor in vcc_main_threshold */
	if (!dvfs_component[factor].auto_dvfs)
		goto err;

	clk_name = dvfs_component[factor].clk_name;

	vm_dvfs = kzalloc(sizeof(struct dvfs), GFP_KERNEL);
	if (!vm_dvfs) {
		pr_err("failed to request mem for vcc_main dvfs\n");
		goto err;
	}

	if (!dvc_flag)
		vl_num = pxa988_get_vl_num();
	else
		vl_num = pxa988_get_freq_num(clk_name);

	vt = kzalloc(sizeof(struct vol_table) * vl_num, GFP_KERNEL);
	if (!vt) {
		pr_err("failed to request mem for vcc_main vol table\n");
		goto err_vt;
	}

	for (i = 0; i < vl_num; i++) {
		vt[i].freq = dvfs_component[factor].freqs[i] * \
			dvfs_component[factor].freqs_mult;
		vt[i].millivolts = dvfs_component[factor].millivolts[i];
		pr_info("clk[%s] rate[%lu] volt[%d]\n", clk_name, vt[i].freq,
					vt[i].millivolts);
	}
	vm_dvfs->vol_freq_table = vt;
	vm_dvfs->clk_name = clk_name;
	vm_dvfs->num_freqs = vl_num;
	vm_dvfs->dvfs_rail = dvfs_component[factor].dvfs_rail;

	dvfs_component[factor].clk_node =
		clk_get_sys(NULL, clk_name);
	dvfs_component[factor].dvfs = vm_dvfs;

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
	for (i = 0; i < dvfs->num_freqs; i++) {
		if (dvfs->vol_freq_table[i].millivolts == millivolts) {
			if (dvfs->vol_freq_table[i].freq > max_freq)
				max_freq = dvfs->vol_freq_table[i].freq;
		}
	}
	return max_freq;
}

static ATOMIC_NOTIFIER_HEAD(dvfs_freq_notifier_list);

int dvfs_notifier_frequency(struct dvfs_freqs *freqs, unsigned int state)
{
	int ret;

	switch (state) {
	case DVFS_FREQ_PRECHANGE:
		ret = atomic_notifier_call_chain(&dvfs_freq_notifier_list,
						 DVFS_FREQ_PRECHANGE, freqs);
		if (ret != NOTIFY_DONE)
			pr_debug("Failure in device driver before "
				 "switching frequency\n");
		break;
	case DVFS_FREQ_POSTCHANGE:
		ret = atomic_notifier_call_chain(&dvfs_freq_notifier_list,
						 DVFS_FREQ_POSTCHANGE, freqs);
		if (ret != NOTIFY_DONE)
			pr_debug("Failure in device driver after "
				 "switching frequency\n");
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(dvfs_notifier_frequency);

int dvfs_register_notifier(struct notifier_block *nb, unsigned int list)
{
	int ret;

	switch (list) {
	case DVFS_FREQUENCY_NOTIFIER:
		ret = atomic_notifier_chain_register
		    (&dvfs_freq_notifier_list, nb);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(dvfs_register_notifier);

int dvfs_unregister_notifier(struct notifier_block *nb, unsigned int list)
{
	int ret;

	switch (list) {
	case DVFS_FREQUENCY_NOTIFIER:
		ret = atomic_notifier_chain_unregister
		    (&dvfs_freq_notifier_list, nb);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(dvfs_unregister_notifier);

static int global_notifier(struct dvfs *dvfs, int state,
			   int old_rate, int new_rate)
{
	struct dvfs_freqs freqs;
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

static struct dvfs_rail *pxa988_dvfs_rails_ax[] = {
	&pxa988_dvfs_rail_ap_active,
	&pxa988_dvfs_rail_ap_lpm,
	&pxa988_dvfs_rail_apsub_idle,
	&pxa988_dvfs_rail_apsub_sleep,
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

	for (list_from = from->dvfs.next, list_to = to->dvfs.next;
	     (list_from != &from->dvfs) && (list_to != &to->dvfs);
	     list_from = list_from->next, list_to = list_to->next) {
		d_to = list_entry(list_to, struct dvfs, dvfs_node);
		d_from = list_entry(list_from, struct dvfs, dvfs_node);
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

	return millivolts;
}

static struct dvfs_relationship pxa988_dvfs_relationships_ax[] = {
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
	value = __raw_readl(PMUM_DVCR);
	value |= (DVCR_VC_EN | DVCR_LPM_AVC_EN);
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

	if (!dvc_flag) {
		dvfs_init_rails(pxa988_dvfs_rails, ARRAY_SIZE(pxa988_dvfs_rails));
		for (i = 0; i < VM_RAIL_MAX; i++) {
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
		dvfs_init_rails(pxa988_dvfs_rails_ax, ARRAY_SIZE(pxa988_dvfs_rails_ax));
		dvfs_add_relationships(pxa988_dvfs_relationships_ax,
					ARRAY_SIZE(pxa988_dvfs_relationships_ax));
		init_level_mapping();
		enable_ap_dvc();

		for (r = 0; r < ARRAY_SIZE(pxa988_dvfs_rails_ax); r++) {
			rail_component = dvfs_rail_component_list[r];
			for (i = 0; i < VM_RAIL_MAX; i++) {
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
	if (!dvc_flag)
		return 0;

	/* Write level 0 svc values, level 1~3 are written after pm800 init */
	value = pm800_extern_write(PM80X_POWER_PAGE,
			PM800_BUCK1, volt_to_reg(vm_millivolts[0]));
	if (value < 0) {
		printk(KERN_ERR "SVC table writting failed !\n");
		return -1;
	}

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

#ifdef	DVC_WR
	value = cur_level_volt[1];
	cur_level_volt[1] = cur_level_volt[2];
	cur_level_volt[2] = value;
#endif

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

	len += snprintf(buf + len, size, "|DVC Voltage: Level %d ",
			volt1);
#ifdef	DVC_WR
	if (volt1 == 1)
		volt1 = 2;
	else if (volt1 == 2)
		volt1 = 1;
#endif
	volt1 = pm800_extern_read(PM80X_POWER_PAGE, PM800_BUCK1 + volt1);
	volt1 = reg_to_volt(volt1);
	len += snprintf(buf + len, size, "(%d mV)\t\t |\n", volt1);

	for (i = 0; i < VM_RAIL_MAX; i++) {
		if (vm_rail_ap_active_tbl[i].auto_dvfs) {
			d = vm_rail_ap_active_tbl[i].clk_node->dvfs;
			rate = clk_get_rate(vm_rail_ap_active_tbl[i].clk_node);
			if (d->millivolts > 0) {
				len += snprintf(buf + len, size,
				"| %s:\t\t| freq %luMhz,\t voltage: Level %d |\n",
				vm_rail_ap_active_tbl[i].clk_name,
				rate / 1000000, level_mapping[d->millivolts]);
			} else {
				len += snprintf(buf + len, size,
				"| %s:\t| freq %luMhz,\t voltage: %d |\n",
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
	volt_status = debugfs_create_file("voltage", 0444,
		dvfs_node, NULL, &voltage_fops);
	if (!volt_status)
		goto err_voltage;

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
