/*
 *  linux/arch/arm/mach-mmp/include/mach/dvfs.h
 *
 *  Author: Xiaoguang Chen chenxg@marvell.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _MACH_MMP_DVFS_DVC_H_
#define _MACH_MMP_DVFS_DVC_H_
#include <linux/clk/dvfs.h>

#define mV2uV	1000
#define INIT_DVFS(_clk_name, _auto, _flag, _freqs)	\
	{								\
		.clk_name	= _clk_name,				\
		.auto_dvfs	= _auto,				\
		.affectrail_flag	= _flag,			\
		.freqs		= _freqs,				\
	}

/*
 * NOTES: DVC is used to change voltage, currently use max
 * voltage lvl 8 due to SOC limitation
 */
enum {
	VL0 = 0,
	VL1,
	VL2,
	VL3,
	VL4,
	VL5,
	VL6,
	VL7,
	MAX_PMIC_LEVEL,
};


enum hwdvc_rails {
	AP_ACTIVE = 0,
	AP_LPM,
	APSUB_IDLE,
	APSUB_SLEEP,
	AP_COMP_MAX,
};

/* flag used for hwdvc feature to represent component affect rails */
enum comp_affected_rail {
	AFFECT_RAIL_ACTIVE = (1 << AP_ACTIVE),
	AFFECT_RAIL_M2 = (1 << AP_LPM),
	AFFECT_RAIL_D1P = (1 << APSUB_IDLE),
};

struct dvfs_rail_component {
	const char *clk_name;
	bool auto_dvfs;

	/*
	 * freqs represent the max freq support in specific voltage lvl,
	 * unit Khz
	 */
	unsigned long *freqs;
	int *millivolts;

	/* HWDVC: fill the clock affected rail flag */
	unsigned int affectrail_flag;

	/* used to save related clk_node and active rails dvfs ptr */
	struct clk *clk_node;
	struct dvfs *dvfs;
};

struct dvc_plat_info {
	/* passed component related table information */
	struct dvfs_rail_component *comps;
	unsigned int num_comps;

	/* platform supported voltage lvl, voltage tbl is sort ascending */
	int *millivolts;
	int num_volts;

	/* register base for hw dvc register */
	void __iomem *dvc_reg_base;
	/* cp and mas init voltage lvl requirement */
	unsigned int cp_pmudvc_lvl;
	unsigned int dp_pmudvc_lvl;

	/* delay for extra timer, unit us */
	unsigned int extra_timer_dlyus;
	/* voltage setting callback for i2c adjustment, unit mV */
	int (*set_vccmain_volt)(unsigned int lvl, unsigned int volt);
	int (*get_vccmain_volt)(unsigned int lvl);
	/* how many voltage lvls pmic supported */
	int pmic_maxvl;
	/* pmic ramp up time, determine the stable time, unit uV/us */
	unsigned int pmic_rampup_step;
	/* debug level : 1-to console, 0-to logbuffer */
	bool dbglvl;

	/* regulator name for SWdvc */
	const char *regname;
};

extern int dvfs_setup_dvcplatinfo(struct dvc_plat_info *platinfo);
extern int dvfs_get_dvcplatinfo(struct dvc_plat_info *platinfo);

extern int dvfs_get_svc_freq_table(unsigned long const **freq, const char *name);
extern int get_nodvfs(void);

/* hwdvc notifier */
struct hwdvc_notifier_data {
	unsigned int oldlv;
	unsigned int newlv;

};
extern int hwdvc_notifier_register(struct notifier_block *n);
extern int hwdvc_notifier_unregister(struct notifier_block *n);

#endif
