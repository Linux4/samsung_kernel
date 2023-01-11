/*
 *  linux/drivers/clk/mmp/dvfs-dvc.c
 *
 *  based on arch/arm/mach-tegra/tegra2_dvfs.c
 * Copyright (C) 2010 Google, Inc. by Colin Cross <ccross@google.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/clk-private.h>
#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk/dvfs-dvc.h>
#include <linux/clk/mmpdcstat.h>
#include <linux/debugfs-pxa.h>

#define KHZ_TO_HZ		1000
#define MV_TO_UV		1000
#define EXTRA_RAMUPUS		10

/* Start from logic lvl 0 */
enum hwdvc_lvl {
	LEVEL0 = 0,
	LEVEL1,
	LEVEL2,
	LEVEL3,
	LEVEL4,
	LEVEL5,
	LEVEL6,
	LEVEL7,
	LEVEL_END,
};

enum hwdvc_reg_off {
	DVC_DVCR = 0x2000,
	DVC_VL01STR = 0x2004,
	DVC_AP = 0x2020,
	DVC_CP = 0x2024,
	DVC_DP = 0x2028,
	DVC_APSUB = 0x202c,
	DVC_APCHIP = 0x2030,
	DVC_STATUS = 0x2040,
	DVC_IMR = 0x2050,
	DVC_ISR = 0x2054,
	DVC_DEBUG = 0x2058,
	DVC_EXRA_STR = 0x205c,
};

static int dvc_flag = 1;
static int __init dvc_flag_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	dvc_flag = n;
	return 1;
}
__setup("dvc_flag=", dvc_flag_setup);

/* For debug purpose,
 * totally disable dvfs, and use fixed voltage in uboot.
 * Please use it carefully, as freq chg won't trigger vol chg any more
 * after setting nodvfs = 1.
 * Make sure you pass the voltage is enough for all components to
 * run at highest freq.
 */
static int __read_mostly nodvfs;
static int __init nodvfs_setup(char *str)
{
	nodvfs = 1;
	return 1;
}
__setup("nodvfs", nodvfs_setup);

int get_nodvfs(void)
{
	return nodvfs;
}

/* sw triggered active&lpm dvc register */
union pmudvc_cr {
	struct {
		unsigned int lpm_avc_en:1;
		unsigned int act_avc_en:1;
		unsigned int rsv:30;
	} b;
	unsigned int v;
};

/* sw triggered active&lpm dvc register */
union pmudvc_xp {
	struct {
		unsigned int lpm_vl:3;
		unsigned int lpm_avc_en:1;
		unsigned int act_vl:3;
		unsigned int act_vcreq:1;
		unsigned int rsv:24;
	} b;
	unsigned int v;
};

/* interrupts mask/status register */
/* IMR: 0 - mask, 1 - unmask, ISR: 0 - clear, 1 - ignore */
union pmudvc_imsr {
	struct {
		unsigned int ap_vc_done:1;
		unsigned int cp_vc_done:1;
		unsigned int dp_vc_done:1;
		unsigned int rsv:29;
	} b;
	unsigned int v;
};

/* stable timer setting of VLi ->VLi+1 */
union pmudvc_stbtimer {
	struct {
		unsigned int stbtimer:16;
		unsigned int rsv:16;
	} b;
	unsigned int v;
};

/* hwdvc apsub apchip register */
union pmudvc_apsubchip {
	struct {
		unsigned int mode0_vl:3; /*apsub_idle/nudr_apchip_sleep */
		unsigned int mode0_en:1;
		unsigned int mode1_vl:3; /*udr_apchip_sleep */
		unsigned int mode1_en:1;
		unsigned int mode2_vl:3; /*nudr_apsub_sleep */
		unsigned int mode2_en:1;
		/* add used field here */
		unsigned int rsv:20;
	} b;
	unsigned int v;
};

struct dvc_private_info {
	struct dvc_plat_info *dvcplatinfo;

	/* record current pmic setting of lvls */
	int cur_level_volt[MAX_PMIC_LEVEL];
	int cur_level_num;

	int level_mapping[LEVEL_END];
	bool pmic_lvl_inited;

	/*
	 * dvfs million volts,
	 * for hwdvc, it is logic vl,
	 * for swdvc, it is volts considered cp/dp req
	 */
	int *dvfs_millivolts;
	bool stb_timer_inited;

	/* cached current voltage lvl for each hwdvc rails */
	int cached_cur_lvl[AP_COMP_MAX];
};

/* record dvc register base address, pass from platform */
static void __iomem *dvc_reg_base;
#define DVC_REG_WRITE(val, reg)	\
	(__raw_writel(val, (dvc_reg_base + reg)))
#define DVC_REG_READ(reg)	\
	(__raw_readl((dvc_reg_base + reg)))

/* static ptr used to save the dvc platform related information */
static struct dvc_private_info *dvc_info;

#define DVC_PRINT(fmt, ...)				\
do {							\
	if (unlikely(dvc_info->dvcplatinfo->dbglvl))	\
		pr_info(fmt, ##__VA_ARGS__);		\
	else						\
		printk(KERN_DEBUG fmt, ##__VA_ARGS__);	\
} while (0)

/* notifier for hwdvc */
static ATOMIC_NOTIFIER_HEAD(dvc_notifier);
static void hwdvc_notifier_call_chain(enum hwdvc_rails rails,
	struct hwdvc_notifier_data *data);

/************************* SW DVC **************************/
/*
 * NOTES: we set step to 500mV here as we don't want
 * voltage change step by step, as GPIO based DVC is
 * used. This can avoid tmp voltage which is not in saved
 * in 4 level regulator table.
 * It is mainly used for other vendors pmic which doesn't
 * support hw dvc. i2c cmd is used to adjust the voltage lvl,
 * reg_id must align with the vcc_main regulator name
 */
static struct dvfs_rail swdvc_dvfs_rail = {
	/* mainly for pmic */
	.reg_id = "vcc_main",
	.max_millivolts = 1400,
	.min_millivolts = 900,
	.nominal_millivolts = 1200,
	.step = 500,
};

static struct dvfs_rail *swdvc_dvfs_rails[] = {
	&swdvc_dvfs_rail,
};


/************************* PMU DVC **************************/
static int hwdvc_set_active_vl(struct dvfs_rail *rail, int lvl);
static int hwdvc_set_m2_vl(struct dvfs_rail *rail, int lvl);
static int hwdvc_set_d1p_vl(struct dvfs_rail *rail, int lvl);

/*
 * Rails for pmu dvc,
 * totally have three rails for active/m2(lpm)/d1p(apsub_idle)
 */
static struct dvfs_rail hwdvc_dvfs_rail_ap_active = {
	.reg_id = "vcc_main_ap_active",
	.max_millivolts = LEVEL7,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL0,
	.step = 0xFF,
	.set_volt = hwdvc_set_active_vl,
};

static struct dvfs_rail hwdvc_dvfs_rail_ap_lpm = {
	.reg_id = "vcc_main_ap_lpm",
	.max_millivolts = LEVEL7,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL0,
	.step = 0xFF,
	.set_volt = hwdvc_set_m2_vl,
};

static struct dvfs_rail hwdvc_dvfs_rail_apsub_idle = {
	.reg_id = "vcc_main_apsub_idle",
	.max_millivolts = LEVEL7,
	.min_millivolts = LEVEL0,
	.nominal_millivolts = LEVEL0,
	.step = 0xFF,
	.set_volt = hwdvc_set_d1p_vl,
};

static struct dvfs_rail *hwdvc_dvfs_rails[] = {
	&hwdvc_dvfs_rail_ap_active,
	&hwdvc_dvfs_rail_ap_lpm,
	&hwdvc_dvfs_rail_apsub_idle,
};

static int get_stable_ticks(int millivolts1, int millivolts2)
{
	int timeus, ticks;
	unsigned int rampupstep;
	/*
	 * clock is VCTCXO(26Mhz), 1us is 26 ticks
	 * PMIC voltage change is 12.5mV/us
	 * PMIC launch time is 8us(include 2us dvc pin sync time)
	 * For safe consideration, add 2us in ramp up time
	 * so the total latency is 10us
	 * Helan LTE adds one extra counter to avoid such latency
	 */
	rampupstep = dvc_info->dvcplatinfo->pmic_rampup_step;
	timeus = DIV_ROUND_UP(abs(millivolts2 - millivolts1) *
		MV_TO_UV, rampupstep);
	/* there is extra timer for delay */
	if (dvc_info->dvcplatinfo->extra_timer_dlyus)
		ticks = timeus * 26;
	else
		ticks = (timeus + EXTRA_RAMUPUS) * 26;

	return ticks;
}

/* Set PMIC voltage value of a specific level */
static int set_voltage_value(int level, int value)
{
	BUG_ON(!dvc_info->dvcplatinfo->set_vccmain_volt);
	return dvc_info->dvcplatinfo->set_vccmain_volt(level, value);
}

/* Read PMIC to get voltage value according to level */
static int get_voltage_value(int level)
{
	BUG_ON(!dvc_info->dvcplatinfo->get_vccmain_volt);
	return dvc_info->dvcplatinfo->get_vccmain_volt(level);
}

/* enable active and LPM dvc for system and AP */
static void __init hwdvc_enable_ap_dvc(void)
{
	struct hwdvc_notifier_data data;

	union pmudvc_cr pmudvc_cr;
	union pmudvc_xp pmudvc_xp;
	union pmudvc_imsr pmudvc_imsr;
	union pmudvc_apsubchip pmudvc_apsub, pmudvc_apchip;

	/* global enable control */
	pmudvc_cr.v = DVC_REG_READ(DVC_DVCR);
	pmudvc_cr.b.lpm_avc_en = 1;
	pmudvc_cr.b.act_avc_en = 1;
	DVC_REG_WRITE(pmudvc_cr.v, DVC_DVCR);

	/* unmask AP DVC done int */
	pmudvc_imsr.v = DVC_REG_READ(DVC_IMR);
	pmudvc_imsr.b.ap_vc_done = 1;
	DVC_REG_WRITE(pmudvc_imsr.v, DVC_IMR);

	/* enable ap active and LPM(M2) */
	pmudvc_xp.v = DVC_REG_READ(DVC_AP);
	pmudvc_xp.b.lpm_avc_en = 1;
	DVC_REG_WRITE(pmudvc_xp.v, DVC_AP);

	/* enable ap sub idle */
	pmudvc_apsub.v = DVC_REG_READ(DVC_APSUB);
	pmudvc_apsub.b.mode0_en = 1;
	DVC_REG_WRITE(pmudvc_apsub.v, DVC_APSUB);

	/* enable nudr and udr ap chip sleep with VL0 */
	pmudvc_apchip.v = DVC_REG_READ(DVC_APCHIP);
	pmudvc_apchip.b.mode0_vl = VL0;
	pmudvc_apchip.b.mode0_en = 1;
	pmudvc_apchip.b.mode1_vl = VL0;
	pmudvc_apchip.b.mode1_en = 1;
	DVC_REG_WRITE(pmudvc_apchip.v, DVC_APCHIP);

	data.oldlv = data.newlv = VL0;
	hwdvc_notifier_call_chain(APSUB_SLEEP, &data);
}

/* vote active and LPM voltage level request for CP */
static int __init hwdvc_enable_cpdp_dvc(void)
{
	unsigned int cp_pmudvc_lvl = 0;
	unsigned int dp_pmudvc_lvl = 0;
	int max_delay = DIV_ROUND_UP(0xFFFF * 8, 26);
	union pmudvc_xp pmudvc_xp;
	union pmudvc_imsr pmudvc_imsr;

	if (dvc_info == NULL) {
		pr_err("dvc_info is NULL, nodvfs!\n");
		return -EINVAL;
	} else {
		cp_pmudvc_lvl =
			dvc_info->dvcplatinfo->cp_pmudvc_lvl;
		dp_pmudvc_lvl =
			dvc_info->dvcplatinfo->dp_pmudvc_lvl;
	}

	/* unmask cp/msa DVC done int */
	pmudvc_imsr.v = DVC_REG_READ(DVC_IMR);
	pmudvc_imsr.b.cp_vc_done = 1;
	pmudvc_imsr.b.dp_vc_done = 1;
	DVC_REG_WRITE(pmudvc_imsr.v, DVC_IMR);

	/*
	 * Vote CP active cp_pmudvc_lvl and LPM VL0
	 * and trigger CP VC request
	 */
	pmudvc_xp.v = DVC_REG_READ(DVC_CP);
	pmudvc_xp.b.lpm_vl = VL0;
	pmudvc_xp.b.lpm_avc_en = 0;
	pmudvc_xp.b.act_vl = cp_pmudvc_lvl;
	pmudvc_xp.b.act_vcreq = 1;
	DVC_REG_WRITE(pmudvc_xp.v, DVC_CP);

	/*
	 * Vote DP active dp_pmudvc_lvl and LPM VL0
	 * and trigger DP VC request
	 */
	pmudvc_xp.v = DVC_REG_READ(DVC_DP);
	pmudvc_xp.b.lpm_vl = VL0;
	pmudvc_xp.b.lpm_avc_en = 1;
	pmudvc_xp.b.act_vl = dp_pmudvc_lvl;
	pmudvc_xp.b.act_vcreq = 1;
	DVC_REG_WRITE(pmudvc_xp.v, DVC_DP);

	/* wait dvc done and clear the request triggered by AP init */
	pmudvc_imsr.v = DVC_REG_READ(DVC_ISR);
	while (max_delay && !pmudvc_imsr.b.cp_vc_done
		&& !pmudvc_imsr.b.dp_vc_done) {
		pmudvc_imsr.v = DVC_REG_READ(DVC_ISR);
		max_delay--;
	}
	if (!max_delay) {
		pr_err("%s cp/dp dvc failed! DVC_ISR:%x\n", __func__,
			DVC_REG_READ(DVC_ISR));
		return -EINVAL;
	}
	pmudvc_imsr.v = DVC_REG_READ(DVC_ISR);
	pmudvc_imsr.b.ap_vc_done = 1;
	pmudvc_imsr.b.cp_vc_done = 0;
	pmudvc_imsr.b.dp_vc_done = 0;
	DVC_REG_WRITE(pmudvc_imsr.v, DVC_ISR);

	pmudvc_xp.v = DVC_REG_READ(DVC_CP);
	DVC_PRINT("Default cp active: %d, lpm: %d\n",
		pmudvc_xp.b.act_vl, pmudvc_xp.b.lpm_vl);
	pmudvc_xp.v = DVC_REG_READ(DVC_DP);
	DVC_PRINT("Default dp active: %d, lpm: %d\n",
		pmudvc_xp.b.act_vl, pmudvc_xp.b.lpm_vl);
	return 0;
}
/* enable cp dvc at late stage for sdh tunning */
late_initcall(hwdvc_enable_cpdp_dvc);

/* set stable timer of VLi and VLi+1 */
static void hwdvc_set_stable_timer(unsigned int lvl)
{
	union pmudvc_stbtimer stbtimer;
	unsigned int ticks;
	int volt_lvl, volt_lvladd1;

	volt_lvl = dvc_info->cur_level_volt[lvl];
	volt_lvladd1 = dvc_info->cur_level_volt[lvl+1];
	stbtimer.v = DVC_REG_READ(DVC_VL01STR + lvl * 0x4);
	ticks = get_stable_ticks(volt_lvl, volt_lvladd1);
	stbtimer.b.stbtimer = ticks;
	DVC_REG_WRITE(stbtimer.v, DVC_VL01STR + lvl * 0x4);
}

static inline void hwdvc_clr_apvcdone_isr(void)
{
	union pmudvc_imsr pmudvc_isr;

	/* write 0 to clear, write 1 has no effect */
	pmudvc_isr.v = DVC_REG_READ(DVC_ISR);
	pmudvc_isr.b.ap_vc_done = 0;
	pmudvc_isr.b.cp_vc_done = 1;
	pmudvc_isr.b.dp_vc_done = 1;
	DVC_REG_WRITE(pmudvc_isr.v, DVC_ISR);
}

static inline void hwdvc_fill_pmic_volt(void)
{
	int *millvolts = dvc_info->cur_level_volt;
	int idx, pmicmaxvl = dvc_info->dvcplatinfo->pmic_maxvl;

	/*
	 * updated lvl1 ~ lv3 during the first voltage change
	 * due to we always boot from 00 setting
	 */
	if (unlikely(!dvc_info->pmic_lvl_inited)) {
		for (idx = pmicmaxvl - 1; idx > 0; idx--) {
			set_voltage_value(dvc_info->level_mapping[idx],
				millvolts[idx]);
		}
		dvc_info->pmic_lvl_inited = true;
	}
}

/*
 * pmu level to pmic level mapping
 * At least support 4 VLs, Current setting:
 * level 1 is mapped to level 1
 * level 2 is mapped to level 2
 * level 3~7 is mapped to level 3
 */
static int __init hwdvc_init_lvl_mapping(void)
{
	int idx, pmicmaxvl = dvc_info->dvcplatinfo->pmic_maxvl;

	if (dvc_info->cur_level_num > pmicmaxvl) {
		dvc_info->level_mapping[LEVEL0] = LEVEL0;
		dvc_info->level_mapping[LEVEL1] = LEVEL1;
		dvc_info->level_mapping[LEVEL2] = LEVEL2;
		dvc_info->level_mapping[LEVEL3] = LEVEL3;
		dvc_info->level_mapping[LEVEL4] = LEVEL3;
		dvc_info->level_mapping[LEVEL5] = LEVEL3;
		dvc_info->level_mapping[LEVEL6] = LEVEL3;
		dvc_info->level_mapping[LEVEL7] = LEVEL3;
	} else {
		for (idx = 0; idx < LEVEL_END; idx++) {
			dvc_info->level_mapping[LEVEL0 + idx] =
				LEVEL0 + idx;
		}
	}
	return 0;
}

/*
 * Set a different level voltage according to the level value
 * @level the actual level value that dvc wants to set.
 * for example, if level == 4, and pmic only has level 0~3,
 * it needs to replace one level with level4's voltage.
 */
static int hwdvc_replace_lvl_voltage(int *level)
{
	int pmic_level = dvc_info->level_mapping[*level];
	int volt = dvc_info->dvcplatinfo->millivolts[*level - LEVEL0];
	int *pmic_volts = dvc_info->cur_level_volt;
	int maxvl = dvc_info->cur_level_num;
	bool pmic_volts_inited = dvc_info->pmic_lvl_inited;
	int idx, timeus, rampupstep;

	if (likely(pmic_volts_inited) && (pmic_volts[pmic_level] != volt)) {
		pr_debug("Replace pmic level %d from %d -> %d!\n",
			pmic_level, pmic_volts[pmic_level], volt);
		set_voltage_value(pmic_level, volt);
		pmic_volts[pmic_level] = volt;
		/* Update voltage level change stable time */
		for (idx = 0; idx < maxvl - 1; idx++)
			hwdvc_set_stable_timer(idx);
		/* If voltage increases, delay until voltage rampup ready */
		if (pmic_volts[pmic_level] < volt) {
			rampupstep = dvc_info->dvcplatinfo->pmic_rampup_step;
			timeus = DIV_ROUND_UP((volt - pmic_volts[pmic_level]) *
				MV_TO_UV, rampupstep) +
				dvc_info->dvcplatinfo->extra_timer_dlyus;
			udelay(timeus);
		}
	}
	*level = pmic_level;
	return 0;
}

static int hwdvc_set_active_vl(struct dvfs_rail *rail, int lvl)
{
	/*
	 * Max delay time, unit is us. (1.5v - 1v) / 0.125v = 40
	 * Also PMIC needs 10us to launch and sync dvc pins
	 * default delay should be 0xFFFF * 8 ticks
	 * (LV0-->LV7 or LV7-->L0) + extra timer
	 */
	int max_delay, hwlvl = lvl;
	union pmudvc_xp pmudvc_ap;
	union pmudvc_imsr pmudvc_isr;
	struct hwdvc_notifier_data data;

	if (dvc_info->stb_timer_inited)
		max_delay = 40 + dvc_info->dvcplatinfo->extra_timer_dlyus;
	else
		max_delay = DIV_ROUND_UP(0xFFFF * 8, 26);

	hwdvc_fill_pmic_volt();
	hwdvc_replace_lvl_voltage(&hwlvl);
	if (dvc_info->cached_cur_lvl[AP_ACTIVE] == hwlvl)
		return 0;
	/*
	 * AP SW is the only client to trigger AP DVC.
	 * Clear AP interrupt status to make sure no wrong signal is set
	 */
	hwdvc_clr_apvcdone_isr();

	/* trigger vc and wait for done */
	pmudvc_ap.v = DVC_REG_READ(DVC_AP);
	pmudvc_ap.b.act_vl = hwlvl;
	pmudvc_ap.b.act_vcreq = 1;
	pr_debug("%s Active VL %d [%x] = [%x]\n", __func__,
		hwlvl, (unsigned int)DVC_AP, pmudvc_ap.v);
	DVC_REG_WRITE(pmudvc_ap.v, DVC_AP);

	pmudvc_isr.v = DVC_REG_READ(DVC_ISR);
	while (max_delay && !pmudvc_isr.b.ap_vc_done) {
		pmudvc_isr.v = DVC_REG_READ(DVC_ISR);
		udelay(1);
		max_delay--;
	}
	if (!max_delay) {
		pr_err("AP active voltage change can't finish!\n");
		BUG_ON(1);
	}
	hwdvc_clr_apvcdone_isr();

	data.oldlv = dvc_info->cached_cur_lvl[AP_ACTIVE];
	data.newlv = hwlvl;
	hwdvc_notifier_call_chain(AP_ACTIVE, &data);

	dvc_info->cached_cur_lvl[AP_ACTIVE] = hwlvl;

#ifdef CONFIG_VOLDC_STAT
	vol_dcstat_event(VLSTAT_VOL_CHG, 0, hwlvl);
#endif
	return 0;
}

static int hwdvc_set_m2_vl(struct dvfs_rail *rail, int lvl)
{
	union pmudvc_xp pmudvc_ap;
	int hwlvl = lvl;
	struct hwdvc_notifier_data data;

	hwdvc_fill_pmic_volt();
	hwdvc_replace_lvl_voltage(&hwlvl);
	if (dvc_info->cached_cur_lvl[AP_LPM] == hwlvl)
		return 0;

	pmudvc_ap.v = DVC_REG_READ(DVC_AP);
	pmudvc_ap.b.lpm_vl = hwlvl;
	pr_debug("%s LPM VL %d [%x] = [%x]\n", __func__,
		hwlvl, (unsigned int)DVC_AP, pmudvc_ap.v);
	DVC_REG_WRITE(pmudvc_ap.v, DVC_AP);

	data.oldlv = dvc_info->cached_cur_lvl[AP_LPM];
	data.newlv = hwlvl;
	hwdvc_notifier_call_chain(AP_LPM, &data);

	dvc_info->cached_cur_lvl[AP_LPM] = hwlvl;

#ifdef CONFIG_VOLDC_STAT
	vol_dcstat_event(VLSTAT_VOL_CHG, 2, hwlvl);
#endif
	return 0;
}

static int hwdvc_set_d1p_vl(struct dvfs_rail *rail, int lvl)
{
	union pmudvc_apsubchip pmudvc_apsub;
	int hwlvl = lvl;
	struct hwdvc_notifier_data data;

	hwdvc_fill_pmic_volt();
	hwdvc_replace_lvl_voltage(&hwlvl);
	if (dvc_info->cached_cur_lvl[APSUB_IDLE] == hwlvl)
		return 0;

	pmudvc_apsub.v = DVC_REG_READ(DVC_APSUB);
	pmudvc_apsub.b.mode0_vl = hwlvl;
	pr_debug("%s LPM VL %d [%x] = [%x]\n", __func__,
		hwlvl, (unsigned int)DVC_APSUB, pmudvc_apsub.v);
	DVC_REG_WRITE(pmudvc_apsub.v, DVC_APSUB);

	data.oldlv = dvc_info->cached_cur_lvl[APSUB_IDLE];
	data.newlv = hwlvl;
	hwdvc_notifier_call_chain(APSUB_IDLE, &data);

	dvc_info->cached_cur_lvl[APSUB_IDLE] = hwlvl;

#ifdef CONFIG_VOLDC_STAT
	vol_dcstat_event(VLSTAT_VOL_CHG, 3, hwlvl);
#endif
	return 0;
}

/*
 * Platform should call this function as early as possible to
 * init the voltage and rail/frequency tbl according to platform info
 * svc and profile should be handled by platform, only pass this chips
 * voltage value here, suggest to use core_initcall_sync etc
 */
int dvfs_setup_dvcplatinfo(struct dvc_plat_info *platinfo)
{
	int idx = 0, maxvl, min_cp_reqlvl, min_dp_reqlvl, min_reqvolt;

	if (nodvfs) {
		WARN(1, "nodvfs featue is selected for debug!\n");
		return 0;
	}

	if (dvc_info) {
		pr_warn("dvc_info is already inited!\n");
		return 0;
	}

	dvc_info = kzalloc(sizeof(struct dvc_private_info), GFP_KERNEL);
	if (!dvc_info) {
		pr_err("%s dvc_info info malloc failed!\n", __func__);
		return -ENOMEM;
	}
	dvc_info->dvcplatinfo = platinfo;
	dvc_info->dvfs_millivolts =
			kzalloc(platinfo->num_volts * sizeof(int), GFP_KERNEL);
	if (!dvc_info->dvfs_millivolts) {
		pr_err("%s hwdvc_logic_vl malloc failed!\n", __func__);
		kfree(dvc_info);
		return -ENOMEM;
	}

	/* init pmic avaible voltage setting */
	maxvl = min(platinfo->pmic_maxvl, platinfo->num_volts);
	dvc_info->cur_level_num = maxvl;
	for (idx = 0; idx < maxvl; idx++)
		dvc_info->cur_level_volt[idx] =
			platinfo->millivolts[idx];

	dvc_reg_base = platinfo->dvc_reg_base;
	/* For hw dvc feature support */
	if (dvc_flag) {
		/* hw dvc is using logic voltage lvl */
		for (idx = 0; idx < platinfo->num_volts; idx++)
			dvc_info->dvfs_millivolts[idx] = LEVEL0 + idx;

#ifdef CONFIG_VOLDC_STAT
		register_vldcstatinfo(platinfo->millivolts, maxvl);
#endif
	} else {
		/*
		 * For sw dvc feature support,
		 * the lowest voltage requirement should meet CP&DP's req,
		 * go through the table to ensure the voltage req
		 */
		min_cp_reqlvl = platinfo->cp_pmudvc_lvl;
		min_dp_reqlvl = platinfo->dp_pmudvc_lvl;
		min_cp_reqlvl = max(min_cp_reqlvl, min_dp_reqlvl);
		min_reqvolt = dvc_info->cur_level_volt[min_cp_reqlvl];
		for (idx = 0; idx < maxvl; idx++)
			dvc_info->cur_level_volt[idx] =
				max(min_reqvolt,
					dvc_info->cur_level_volt[idx]);
		for (idx = 0; idx < platinfo->num_volts; idx++)
			dvc_info->dvfs_millivolts[idx] =
				max(min_reqvolt,
					platinfo->millivolts[idx]);
	}
	return 0;
}
EXPORT_SYMBOL(dvfs_setup_dvcplatinfo);

int dvfs_get_dvcplatinfo(struct dvc_plat_info *platinfo)
{
	if (likely(platinfo && dvc_info && dvc_info->dvcplatinfo)) {
		/* read only for dvc_info->dvcplatinfo */
		memcpy(platinfo, dvc_info->dvcplatinfo,
			sizeof(struct dvc_plat_info));
		return 0;
	} else
		return -EINVAL;
}
EXPORT_SYMBOL(dvfs_get_dvcplatinfo);

/* return value is dvc_info->dvcplatinfo->num_volts */
int dvfs_get_svc_freq_table(unsigned long const **freq, const char *name)
{
	struct dvfs_rail_component *comps_tbl;
	int idx, num_comps;

	if (likely(dvc_info && dvc_info->dvcplatinfo)) {
		comps_tbl = dvc_info->dvcplatinfo->comps;
		num_comps = dvc_info->dvcplatinfo->num_comps;
		for (idx = 0; idx < num_comps; idx++)
			if (!strcmp(name, comps_tbl[idx].clk_name)) {
				*freq = comps_tbl[idx].freqs;
				return dvc_info->dvcplatinfo->num_volts;
			}
	}

	pr_err("dvfs_get_svc_freq_table failed to get %s freq table\n", name);
	return -EINVAL;
}
EXPORT_SYMBOL(dvfs_get_svc_freq_table);


/* notifier for hwdvc */
int hwdvc_notifier_register(struct notifier_block *n)
{
	if (!dvc_flag || !dvc_info)
		return 0;
	return atomic_notifier_chain_register(&dvc_notifier, n);
}
EXPORT_SYMBOL(hwdvc_notifier_register);

int hwdvc_notifier_unregister(struct notifier_block *n)
{
	if (!dvc_flag || !dvc_info)
		return 0;
	return atomic_notifier_chain_unregister(&dvc_notifier, n);
}
EXPORT_SYMBOL(hwdvc_notifier_unregister);

static void hwdvc_notifier_call_chain(enum hwdvc_rails rails,
	struct hwdvc_notifier_data *data)
{
	if (!dvc_flag)
		return;
	atomic_notifier_call_chain(&dvc_notifier, rails, data);
}

static struct dvfs *vcc_main_dvfs_init
	(struct dvfs_rail_component *dvfs_component, int factor,
		struct dvfs_rail *rail)
{
	struct dvfs *vm_dvfs = NULL;
	struct vol_table *vt = NULL;
	int i;
	unsigned int vl_num = 0;
	const char *clk_name;
	struct clk *clk_node;

	/* dvfs is not enabled for this factor in vcc_main_threshold */
	if (!dvfs_component[factor].auto_dvfs)
		goto err;

	clk_name = dvfs_component[factor].clk_name;
	clk_node = __clk_lookup(clk_name);
	if (!clk_node) {
		pr_err("failed to get clk node %s\n", clk_name);
		goto err;
	}

	vm_dvfs = kzalloc(sizeof(struct dvfs), GFP_KERNEL);
	if (!vm_dvfs) {
		pr_err("failed to request mem for vcc_main dvfs\n");
		goto err;
	}

	vl_num = dvc_info->dvcplatinfo->num_volts;

	vt = kzalloc(sizeof(struct vol_table) * vl_num, GFP_KERNEL);
	if (!vt) {
		pr_err("failed to request mem for vcc_main vol table\n");
		goto err_vt;
	}

	for (i = 0; i < vl_num; i++) {
		vt[i].freq = dvfs_component[factor].freqs[i] * KHZ_TO_HZ;
		vt[i].millivolts = dvfs_component[factor].millivolts[i];
		DVC_PRINT("clk[%s] rate[%lu] volt[%d]\n", clk_name, vt[i].freq,
					vt[i].millivolts);
	}
	vm_dvfs->vol_freq_table = vt;
	vm_dvfs->clk_name = clk_name;
	vm_dvfs->num_freqs = vl_num;
	vm_dvfs->dvfs_rail = rail;

	dvfs_component[factor].clk_node = clk_node;
	dvfs_component[factor].dvfs = vm_dvfs;

	return vm_dvfs;
err_vt:
	kzfree(vm_dvfs);
	vm_dvfs = NULL;
err:
	return vm_dvfs;
}

static void __init swdvc_init_dvfs(void)
{
	int idx, j, num_comps, ret;
	struct dvfs *d;
	struct clk *c;
	unsigned long rate;
	struct dvfs_rail_component *comps_tbl;
	char *regname = NULL;

	num_comps = dvc_info->dvcplatinfo->num_comps;
	comps_tbl = dvc_info->dvcplatinfo->comps;

	/* try to use regulator name passed from platform info */
	if (dvc_info->dvcplatinfo->regname) {
		regname = kstrdup(dvc_info->dvcplatinfo->regname,
			GFP_KERNEL);
		if (regname) {
			swdvc_dvfs_rail.reg_id = regname;
			pr_info("%s: use reg name %s!\n", __func__,
				swdvc_dvfs_rail.reg_id);
		}
	}

	dvfs_init_rails(swdvc_dvfs_rails, ARRAY_SIZE(swdvc_dvfs_rails));
	for (idx = 0; idx < num_comps; idx++) {
		comps_tbl[idx].millivolts = dvc_info->dvfs_millivolts;
		d = vcc_main_dvfs_init(comps_tbl, idx,
			swdvc_dvfs_rails[AP_ACTIVE]);
		if (!d)
			continue;
		c = comps_tbl[idx].clk_node;
		if (!c) {
			pr_err("swdvc_init_dvfs: no clock found for %s\n",
				d->clk_name);
			kzfree(d->vol_freq_table);
			kzfree(d);
			continue;
		}
		ret = enable_dvfs_on_clk(c, d);
		if (ret) {
			pr_err("swdvc_init_dvfs: failed to enable dvfs on %s\n",
				c->name);
			kzfree(d->vol_freq_table);
			kzfree(d);
		}
		/*
		 * adjust the voltage request according to its current rate
		 * for those clk is always ond
		 */
		if (c->enable_count) {
			rate = clk_get_rate(c);
			j = 0;
			while (j < d->num_freqs &&
				rate > d->vol_freq_table[j].freq)
				j++;
			d->millivolts = d->vol_freq_table[j].millivolts;
			d->cur_rate = rate;
		}
	}
}

static void __init hwdvc_init_dvfs(void)
{
	int idx, j, r, num_comps, ret;
	struct dvfs *d;
	struct clk *c;
	unsigned long rate;
	struct dvfs_rail_component *comps_tbl;
	unsigned int affectrail_flag;

	comps_tbl = dvc_info->dvcplatinfo->comps;
	num_comps = dvc_info->dvcplatinfo->num_comps;

	dvfs_init_rails(hwdvc_dvfs_rails, ARRAY_SIZE(hwdvc_dvfs_rails));
	hwdvc_init_lvl_mapping();
	hwdvc_enable_ap_dvc();

	for (r = 0; r < ARRAY_SIZE(hwdvc_dvfs_rails); r++) {
		for (idx = 0; idx < num_comps; idx++) {
			affectrail_flag = comps_tbl[idx].affectrail_flag;
			/*
			 * check if the component affect this rail or not,
			 * we only add it to the affect rail
			 */
			if (!(affectrail_flag & (1 << r)))
				continue;
			comps_tbl[idx].millivolts = dvc_info->dvfs_millivolts;
			DVC_PRINT("Rail %s\n", hwdvc_dvfs_rails[r]->reg_id);
			d = vcc_main_dvfs_init(comps_tbl, idx,
				hwdvc_dvfs_rails[r]);
			if (!d)
				continue;
			c = comps_tbl[idx].clk_node;
			if (!c) {
				pr_err("hwdvc_init_dvfs: no clock found for %s\n",
							d->clk_name);
				kzfree(d->vol_freq_table);
				kzfree(d);
				continue;
			}
			ret = enable_dvfs_on_clk(c, d);
			if (ret) {
				pr_err("hwdvc_init_dvfs: failed to enable dvfs on %s\n",
					c->name);
				kzfree(d->vol_freq_table);
				kzfree(d);
				continue;
			}
			/*
			 * adjust the voltage request according to its
			 * current rate for those clk is always on
			 */
			if (c->enable_count) {
				rate = clk_get_rate(c);
				j = 0;
				while (j < d->num_freqs &&
					rate > d->vol_freq_table[j].freq)
					j++;
				d->cur_rate = rate;
				d->millivolts = d->vol_freq_table[j].millivolts;
			}
		}
	}
}

static int __init dvc_init_dvfs(void)
{
	if (!dvc_info)
		return 0;

	if (dvc_flag)
		hwdvc_init_dvfs();
	else
		swdvc_init_dvfs();
	return 0;
}
subsys_initcall(dvc_init_dvfs);

static int __init hwdvc_init_level_volt(void)
{
	int idx, val, ret = 0;
	union pmudvc_xp pmudvc_ap;
	union pmudvc_apsubchip pmudvc_apsub;
	union pmudvc_stbtimer extrastbtimer;

	if (!dvc_info || !dvc_flag)
		return 0;

	/* Write level 0 svc values, level 1~3 are written after pm800 init */
	ret = set_voltage_value(0, dvc_info->cur_level_volt[0]);
	if (ret < 0) {
		pr_err("VL0 writting failed !\n");
		return ret;
	}

	/* set up stabler timer */
	/* Helan LTE adds one extra counter for PMIC rampup time */
	if (dvc_info->dvcplatinfo->extra_timer_dlyus) {
		extrastbtimer.v = DVC_REG_READ(DVC_EXRA_STR);
		extrastbtimer.b.stbtimer =
			dvc_info->dvcplatinfo->extra_timer_dlyus * 26;
		DVC_REG_WRITE(extrastbtimer.v, DVC_EXRA_STR);
	}

	for (idx = 0; idx < dvc_info->cur_level_num - 1; idx++)
		hwdvc_set_stable_timer(idx);
	dvc_info->stb_timer_inited = true;

	/* Get current PMIC setting */
	for (idx = 0; idx < dvc_info->cur_level_num; idx++) {
		val = get_voltage_value(idx);
		DVC_PRINT("PMIC level %d: %d mV, cur %d mV\n", idx, val,
			dvc_info->cur_level_volt[idx]);
		WARN_ON(dvc_info->cur_level_volt[idx] != val);
	}

	/* Get current level information */
	pmudvc_ap.v = DVC_REG_READ(DVC_AP);
	pmudvc_apsub.v = DVC_REG_READ(DVC_APSUB);
	dvc_info->cached_cur_lvl[AP_ACTIVE] = pmudvc_ap.b.act_vl;
	dvc_info->cached_cur_lvl[AP_LPM] = pmudvc_ap.b.lpm_vl;
	dvc_info->cached_cur_lvl[APSUB_IDLE] = pmudvc_apsub.b.mode0_vl;
	DVC_PRINT("AP active %d, M2 %d, D1P: %d\n",
		dvc_info->cached_cur_lvl[AP_ACTIVE],
		dvc_info->cached_cur_lvl[AP_LPM],
		dvc_info->cached_cur_lvl[APSUB_IDLE]);

	return 0;
}
/*
 * the init must before cpufreq init(module_init)
 * must after pmic_regulator init(subsys_initcall)
 * must after dvfs init (fs_initcall)
 */
device_initcall(hwdvc_init_level_volt);


#ifdef CONFIG_DEBUG_FS
static ssize_t voltage_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *buf;
	int len = 0, cur_vl, num_comps;
	unsigned int i;
	struct dvfs *d;
	unsigned long rate;
	union pmudvc_xp pmudvc_xp;
	union pmudvc_apsubchip pmudvc_apsub, pmudvc_chip;
	union pmudvc_cr pmudvc_cr;
	struct dvfs_rail_component *comps;
	struct clk *c;
	size_t size = PAGE_SIZE, ret;

	buf = (char *)__get_free_pages(GFP_NOIO, get_order(size));
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, size,
		"\n|dvc_flag:\t| %d (Use %s)\t\t\t  |\n",
		dvc_flag, dvc_flag ? "HWDVC" : "SWDVC");

	pmudvc_cr.v = DVC_REG_READ(DVC_DVCR);
	len += snprintf(buf + len, size,
		"|HW_DVC:\t| %-10s%d,%5s%-10s%d%4s|\n", "Active_E:",
		pmudvc_cr.b.act_avc_en,  " ", "Lpm_E:",
		pmudvc_cr.b.lpm_avc_en, " ");

	pmudvc_xp.v = DVC_REG_READ(DVC_AP);
	len += snprintf(buf + len, size,
		"|DVC_AP:\t| %-10s%d,%5s%-10s%d(%d) |\n", "Active:",
		pmudvc_xp.b.act_vl,  " ", "Lpm(E):",
		pmudvc_xp.b.lpm_vl, pmudvc_xp.b.lpm_avc_en);

	pmudvc_xp.v = DVC_REG_READ(DVC_CP);
	len += snprintf(buf + len, size,
		"|DVC_CP:\t| %-10s%d,%5s%-10s%d(%d) |\n", "Active:",
		pmudvc_xp.b.act_vl,  " ", "Lpm(E):",
		pmudvc_xp.b.lpm_vl, pmudvc_xp.b.lpm_avc_en);

	pmudvc_xp.v = DVC_REG_READ(DVC_DP);
	len += snprintf(buf + len, size,
		"|DVC_DP:\t| %-10s%d,%5s%-10s%d(%d) |\n", "Active:",
		pmudvc_xp.b.act_vl,  " ", "Lpm(E):",
		pmudvc_xp.b.lpm_vl, pmudvc_xp.b.lpm_avc_en);

	pmudvc_apsub.v = DVC_REG_READ(DVC_APSUB);
	len += snprintf(buf + len, size,
		"|DVC_APSUB:\t| %-10s%d(%d),%2s%-10s%d(%d) |\n", "IDLE(E):",
		pmudvc_apsub.b.mode0_vl, pmudvc_apsub.b.mode0_en,
		" ", "SLEEP(E):",
		pmudvc_apsub.b.mode2_vl, pmudvc_apsub.b.mode2_en);

	pmudvc_chip.v = DVC_REG_READ(DVC_APCHIP);
	len += snprintf(buf + len, size,
		"|DVC_CHIP:\t| %-10s%d(%d),%2s%-10s%d(%d) |\n\n", "nUDR(E):",
		pmudvc_chip.b.mode0_vl, pmudvc_chip.b.mode0_en,
		" ", "UDR(E):",
		pmudvc_chip.b.mode1_vl, pmudvc_chip.b.mode1_en);

	cur_vl = (DVC_REG_READ(DVC_STATUS) >> 1) & 0x7;
	len += snprintf(buf + len, size, "|DVC Voltage:\t| Level %d ",
			cur_vl);

	len += snprintf(buf + len, size, "(%d mV)\t\t  |\n",
		(dvc_flag ? get_voltage_value(cur_vl) :
		(!swdvc_dvfs_rail.reg ? 0 :
		regulator_get_voltage(swdvc_dvfs_rail.reg) / 1000)));

	num_comps = dvc_info->dvcplatinfo->num_comps;
	comps = dvc_info->dvcplatinfo->comps;
	for (i = 0; i < num_comps; i++) {
		if (comps[i].auto_dvfs) {
			d = comps[i].dvfs;
			if (!d)
				continue;
			c = comps[i].clk_node;
			rate = clk_get_rate(c);
			len += snprintf(buf + len, size,
				"|%-15s| freq %luMhz,\t voltage:%6s %d |\n",
				comps[i].clk_name, rate / 1000000,
				(c->enable_count && dvc_flag) ? "Level" : "",
				(c->enable_count && dvc_flag) ?
				dvc_info->level_mapping[d->millivolts] :
				d->millivolts);
		}
	}

	ret = simple_read_from_buffer(buffer, count, ppos, buf, len);
	free_pages((unsigned long)buf, get_order(size));
	return ret;
}


/* Get current voltage for each power mode */
const struct file_operations voltage_fops = {
	.read = voltage_read,
};

static int __init dvfs_dvc_create_debug_node(void)
{
	struct dentry *dvfs_node;
	struct dentry *volt_status;

	if (!dvc_info)
		return 0;

	dvfs_node = debugfs_create_dir("dvfs", pxa);
	if (!dvfs_node)
		return -ENOENT;

	volt_status = debugfs_create_file("voltage", 0444,
		dvfs_node, NULL, &voltage_fops);
	if (!volt_status)
		goto err_voltage;

	dvfs_debugfs_init(dvfs_node);
	return 0;

err_voltage:
	debugfs_remove(dvfs_node);
	return -ENOENT;
}
late_initcall(dvfs_dvc_create_debug_node);
#endif
