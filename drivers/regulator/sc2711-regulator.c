/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Fixes:
 *	0.2 unprotect dcdc/ldo before turn on and off
 *		remove dcdc/ldo calibration
 * To Fix:
 *
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/arch_misc.h>

#define REGULATOR_ROOT_DIR	"sprd-regulator"

#undef debug
#define debug(format, arg...) pr_info("regu: " "@@@%s: " format, __func__, ## arg)
#define debug0(format, arg...)	//pr_debug("regu: " "@@@%s: " format, __func__, ## arg)
#define debug2(format, arg...)	pr_debug("regu: " "@@@%s: " format, __func__, ## arg)

#ifndef	ANA_REG_OR
#define	ANA_REG_OR(_r, _b)	sci_adi_write(_r, _b, 0)
#endif

#ifndef	ANA_REG_BIC
#define	ANA_REG_BIC(_r, _b)	sci_adi_write(_r, 0, _b)
#endif

#ifndef	ANA_REG_GET
#define	ANA_REG_GET(_r)		sci_adi_read(_r)
#endif

#ifndef	ANA_REG_SET
#define	ANA_REG_SET(_r, _v, _m)	sci_adi_write((_r), ((_v) & (_m)), (_m))
#endif

struct sci_regulator_regs {
	int typ;
	u32 pd_set, pd_set_bit;
	/**
	 * at new feature, some LDOs had only set, no rst bits.
	 * and DCDCs voltage and trimming controller is the same register
	 */
	u32 pd_rst, pd_rst_bit;
	u32 slp_ctl, slp_ctl_bit;
	u32 vol_trm, vol_trm_bits;
	u32 cal_ctl, cal_ctl_bits;
	u32 vol_def;
	u32 vol_ctl, vol_ctl_bits;
	u32 vol_sel_cnt, vol_sel[];
};

struct sci_regulator_data {
	struct regulator_dev *rdev;
};

struct sci_regulator_desc {
	struct regulator_desc desc;
	struct sci_regulator_ops *ops;
	struct sci_regulator_regs *regs;
	struct sci_regulator_data data;	/* FIXME: dynamic */
#if defined(CONFIG_DEBUG_FS)
	struct dentry *debugfs;
#endif
};

enum {
	VDD_TYP_LDO = 0,
	VDD_TYP_LDO_D = 1,
	VDD_TYP_DCDC = 2,
	VDD_TYP_LPREF = 3,
	VDD_TYP_BOOST = 4,
};

#define REGU_VERIFY_DLY	(1000)	/*ms */

static u16 ana_status_bondopt4;
static u32 ana_chip_id;

static DEFINE_MUTEX(adc_chan_mutex);
extern int sci_efuse_calibration_get(u32 * p_cal_data);

#define SCI_REGU_REG(VDD, TYP, PD_SET, SET_BIT, PD_RST, RST_BIT, SLP_CTL, SLP_CTL_BIT, \
                     VOL_TRM, VOL_TRM_BITS, CAL_CTL, CAL_CTL_BITS, VOL_DEF, \
                     VOL_CTL, VOL_CTL_BITS, VOL_SEL_CNT, ...)   \
do { 														\
	static struct sci_regulator_regs REGS_##VDD = {	\
		.typ		= TYP,									\
		.pd_set = PD_SET,                           		\
		.pd_set_bit = SET_BIT,                      		\
		.pd_rst = PD_RST,                           		\
		.pd_rst_bit = RST_BIT,                      		\
		.slp_ctl = SLP_CTL,                         		\
		.slp_ctl_bit = SLP_CTL_BIT,                 		\
		.vol_trm = VOL_TRM,                                 \
		.vol_trm_bits = VOL_TRM_BITS,                       \
		.cal_ctl = CAL_CTL, 								\
		.cal_ctl_bits = CAL_CTL_BITS,						\
		.vol_def = VOL_DEF,									\
		.vol_ctl = VOL_CTL,                         		\
		.vol_ctl_bits = VOL_CTL_BITS,               		\
		.vol_sel_cnt = VOL_SEL_CNT,                 		\
		.vol_sel = {__VA_ARGS__},                   		\
	};														\
	static struct sci_regulator_desc DESC_##VDD = {			\
		.desc.name = #VDD,									\
		.desc.id = 0,										\
		.desc.ops = 0,										\
		.desc.type = REGULATOR_VOLTAGE,						\
		.desc.owner = THIS_MODULE,							\
		.regs = &REGS_##VDD,								\
	};														\
	sci_regulator_register(pdev, &DESC_##VDD);				\
} while (0)

static struct sci_regulator_desc *__get_desc(struct regulator_dev *rdev)
{
	return (struct sci_regulator_desc *)rdev->desc;
}

/* standard ldo ops*/
static int ldo_turn_on(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;

	debug0("regu %p (%s), set %08x[%d], rst %08x[%d]\n", regs,
	       desc->desc.name, regs->pd_set, __ffs(regs->pd_set_bit),
	       regs->pd_rst, __ffs(regs->pd_rst_bit));

	if (regs->pd_set == ANA_REG_GLB_LDO_DCDC_PD)
		sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,
				  BITS_PWR_WR_PROT_VALUE(0x6e7f));

	if (regs->pd_rst)
		ANA_REG_OR(regs->pd_rst, regs->pd_rst_bit);

	if (regs->pd_set)
		ANA_REG_BIC(regs->pd_set, regs->pd_set_bit);

	if (regs->pd_set == ANA_REG_GLB_LDO_DCDC_PD)
		sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE, 0);

	debug2("regu %p (%s), turn on\n", regs, desc->desc.name);
	return 0;
}

static int ldo_turn_off(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;

	debug0("regu %p (%s), set %08x[%d], rst %08x[%d]\n", regs,
	       desc->desc.name, regs->pd_set, __ffs(regs->pd_set_bit),
	       regs->pd_rst, __ffs(regs->pd_rst_bit));

#if !defined(CONFIG_REGULATOR_CAL_DUMMY)
	if (regs->pd_set == ANA_REG_GLB_LDO_DCDC_PD)
		sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,
				  BITS_PWR_WR_PROT_VALUE(0x6e7f));

	if (regs->pd_set)
		ANA_REG_OR(regs->pd_set, regs->pd_set_bit);

	if (regs->pd_rst)
		ANA_REG_BIC(regs->pd_rst, regs->pd_rst_bit);

	if (regs->pd_set == ANA_REG_GLB_LDO_DCDC_PD)
		sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE, 0);

#endif

	debug2("regu %p (%s), turn off\n", regs, desc->desc.name);
	return 0;
}

static int ldo_is_on(struct regulator_dev *rdev)
{
	int ret = -EINVAL;
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;

	debug0("regu %p (%s), set %08x[%d], rst %08x[%d]\n", regs,
	       desc->desc.name, regs->pd_set, __ffs(regs->pd_set_bit),
	       regs->pd_rst, __ffs(regs->pd_rst_bit));

	if (regs->pd_rst && regs->pd_set) {
		/*for pd_rst has higher prioty than pd_set, what's more, their reset values are the same, 0 */
		ret = ! !(ANA_REG_GET(regs->pd_rst) & regs->pd_rst_bit);
		/* FIXME: when reset, pd_set & pd_rst are all zero, always get here */
		if (ret == ! !(ANA_REG_GET(regs->pd_set) & regs->pd_set_bit))
			ret = -EINVAL;
	} else if (regs->pd_rst) {
		ret = ! !(ANA_REG_GET(regs->pd_rst) & regs->pd_rst_bit);
	} else if (regs->pd_set) {	/* new feature */
		ret = !(ANA_REG_GET(regs->pd_set) & regs->pd_set_bit);
	}

	debug2("regu %p (%s) return %d\n", regs, desc->desc.name, ret);
	return ret;
}

#if 0				/* FIXME: todo later */
static int ldo_enable_time(struct regulator_dev *rdev)
{
	return 1000 * 1;	/*Microseconds */
}
#endif

static int ldo_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;

	debug("regu %p (%s), slp %08x[%d] mode %x\n", regs, desc->desc.name,
	      regs->slp_ctl, regs->slp_ctl_bit, mode);

	if (!regs->slp_ctl)
		return -EINVAL;

	if (mode == REGULATOR_MODE_STANDBY) {	/* disable auto slp */
		ANA_REG_BIC(regs->slp_ctl, regs->slp_ctl_bit);
	} else {
		ANA_REG_OR(regs->slp_ctl, regs->slp_ctl_bit);
	}
	return 0;
}

static int ldo_set_voltage(struct regulator_dev *rdev, int min_uV,
			   int max_uV, unsigned *selector)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int mv = min_uV / 1000;
	int ret = -EINVAL;

	debug("regu %p (%s) %d %d\n", regs, desc->desc.name, min_uV, max_uV);

	if (!regs->vol_ctl && regs->vol_trm && regs->vol_sel_cnt == 2) {
		int shft = __ffs(regs->vol_trm_bits);
		u32 trim =	/* vol_sel[0]: output (mV), [1]:step (uV) */
		    DIV_ROUND_UP((mv - regs->vol_sel[0]) * 1000,
				 regs->vol_sel[1]);

		ret = trim > (regs->vol_trm_bits >> shft);
		WARN(0 != ret,
		     "warning: regulator (%s) not support %dmV\n",
		     desc->desc.name, mv);

		if (0 == ret) {
			ANA_REG_SET(regs->vol_trm,
				    trim << __ffs(regs->vol_trm_bits),
				    regs->vol_trm_bits);
		}
		return ret;
	}

	return ret;
}

static int ldo_get_voltage(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	u32 vol;

	debug0("regu %p (%s), vol ctl %08x, shft %d, mask %08x\n",
	       regs, desc->desc.name, regs->vol_ctl, 0, regs->vol_ctl_bits);

	if (!regs->vol_ctl && regs->vol_trm && regs->vol_sel_cnt == 2) {
		int shft = __ffs(regs->vol_trm_bits);
		u32 trim =
		    (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits) >> shft;
		vol = regs->vol_sel[0] * 1000 + trim * regs->vol_sel[1];
		debug2("regu %p (%s), voltage %d\n", regs, desc->desc.name,
		       vol);
		return vol;
	}

	return -EFAULT;
}

/* FIXME: get dcdc cal offset config from uboot */
#define DCDC_CAL_CONF_BASE	(SPRD_IRAM0_BASE + 0x1f00)
#define DCDC_MAX_CNT		(4)
struct dcdc_cal_t {
	char name[32];
	int cal_vol;
};

typedef struct {
	u16 ideal_vol;
	const char name[14];
}vol_para_t;

#define PP_VOL_PARA		( 0x50005c20 )	/* assert in iram2 */
#define TO_IRAM2(_p_)	( SPRD_IRAM2_BASE + (u32)(_p_) - SPRD_IRAM2_PHYS )
#define IN_IRAM2(_p_)	( (u32)(_p_) >= SPRD_IRAM2_PHYS && (u32)(_p_) < SPRD_IRAM2_PHYS + SPRD_IRAM2_SIZE )

int regulator_default_get(const char con_id[])
{
	int i = 0, res = 0;
	vol_para_t *pvol_para = (vol_para_t *)__raw_readl((void *)TO_IRAM2(PP_VOL_PARA));

	if (!(IN_IRAM2(pvol_para)))
		return 0;

	pvol_para = (vol_para_t *)TO_IRAM2(pvol_para);

	if(strcmp((pvol_para)[0].name, "volpara_begin") || (0xfaed != (pvol_para)[0].ideal_vol))
		return 0;

	while(0 != strcmp((pvol_para)[i++].name, "volpara_end")) {
		if (0 == strcmp((pvol_para)[i].name, con_id)) {
			debug("%s name %s, ideal_vol %d\n", __func__, (pvol_para)[i].name, (pvol_para)[i].ideal_vol);
			return res = (pvol_para)[i].ideal_vol;
		}
	}
	return res;
}

static int __init_trimming(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int ctl_vol, to_vol;

	if (!regs->vol_trm)
		goto exit;

	to_vol = regulator_default_get(desc->desc.name);
	if (!to_vol) {
		to_vol = regs->vol_def;
	}

	if (to_vol && rdev->desc->ops->get_voltage) {
		ctl_vol = rdev->desc->ops->get_voltage(rdev);
		rdev->constraints->uV_offset = ctl_vol - to_vol * 1000;//uV
		debug("regu %p (%s), uV offset %d\n", regs, desc->desc.name, rdev->constraints->uV_offset);
	}
exit:
	return 0;
}

static int __match_dcdc_vol(const struct sci_regulator_regs *regs, u32 vol)
{
	int i, j = -1;
	int ds, min_ds = 100;	/* mV, the max range of small voltage */
	for (i = 0; i < regs->vol_sel_cnt; i++) {
		ds = vol - regs->vol_sel[i];
		if (ds >= 0 && ds < min_ds) {
			min_ds = ds;
			j = i;
		}
	}
	return j;
}

static int dcdc_get_trimming_step(struct regulator_dev *rdev, int to_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);

	/* FIXME: vddmem step 200/32mV */
	return (0 == strcmp(desc->desc.name, "vddmem") ) ? (1000 * 200 / 32) : (1000 * 100 / 32) /*uV */;
}

static int __dcdc_enable_time(struct regulator_dev *rdev, int old_vol)
{
	int vol = rdev->desc->ops->get_voltage(rdev) / 1000;
	if (vol > old_vol) {
		/* FIXME: for dcdc, each step (50mV) takes 10us */
		int dly = (vol - old_vol) * 10 / 50;
		WARN_ON(dly > 1000);
		udelay(dly);
	}
	return 0;
}

static int dcdc_set_voltage(struct regulator_dev *rdev, int min_uV,
			    int max_uV, unsigned *selector)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int i, mv = min_uV / 1000;
	int old_vol = rdev->desc->ops->get_voltage(rdev) / 1000;

	debug0("regu %p (%s) %d %d\n", regs, desc->desc.name, min_uV, max_uV);

	BUG_ON(0 != __ffs(regs->vol_trm_bits));
	BUG_ON(regs->vol_sel_cnt > 8);

	if (!regs->vol_ctl)
		return -EACCES;

	/* found the closely vol ctrl bits */
	i = __match_dcdc_vol(regs, mv);
	if (i < 0)
		return WARN(-EINVAL,
			    "not found %s closely ctrl bits for %dmV\n",
			    desc->desc.name, mv);

	debug2("regu %p (%s) %d = %d %+dmv\n", regs, desc->desc.name,
	       mv, regs->vol_sel[i], mv - regs->vol_sel[i]);

#if !defined(CONFIG_REGULATOR_CAL_DUMMY)
	/* dcdc calibration control bits (default 00000),
	 * small adjust voltage: 100/32mv ~= 3.125mv
	 */
	{
		int shft = __ffs(regs->vol_ctl_bits);
		int j = (int)(mv - regs->vol_sel[i]) * 1000 / dcdc_get_trimming_step(rdev, 0);

		if (0 == strcmp(desc->desc.name, "vddmem") )
			j += 0x10;

		BUG_ON(j > (regs->vol_trm_bits >> __ffs(regs->vol_trm_bits)));

		if (regs->vol_trm == regs->vol_ctl) {	/* new feature */
			ANA_REG_SET(regs->vol_ctl, j | (i << shft),
				    regs->vol_trm_bits | regs->vol_ctl_bits);
		} else {
			BUG_ON(1);
		}
	}

	__dcdc_enable_time(rdev, old_vol);
#endif

	return 0;
}

/** CONFIG_ARCH_SCX35
	bonding option 5
	bonding option 4	dcdc_wrf_ctl[2]
	bonding option 3
	bonding option 2	dcdc_mem_ctl[2]
	bonding option 1	dcdc_mem_ctl[1]
	bonding option 0
 */
static int dcdc_get_voltage(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;
	u32 mv;
	int cal = 0;		/* uV */
	int i, shft = __ffs(regs->vol_ctl_bits);

	debug0("regu %p (%s), vol ctl %08x, shft %d, mask %08x, sel %d\n",
	       regs, desc->desc.name, regs->vol_ctl,
	       shft, regs->vol_ctl_bits, regs->vol_sel_cnt);

	if (!regs->vol_ctl)
		return -EINVAL;

	BUG_ON(0 != __ffs(regs->vol_trm_bits));
	BUG_ON(regs->vol_sel_cnt > 8);

	i = (ANA_REG_GET(regs->vol_ctl) & regs->vol_ctl_bits) >> shft;

	mv = regs->vol_sel[i];

	if (regs->vol_trm) {
		/*check the reset relative bit of vol ctl */
		if (regs->vol_trm != regs->vol_ctl) {
			u32 vol_bits =
			    (~ANA_REG_GET(regs->vol_ctl) &
			     (regs->vol_ctl_bits << 4)) >> 4;

			if (i != vol_bits) {
				return -EFAULT;
			}
		}

		cal = (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits);
		if (0 == strcmp(desc->desc.name, "vddmem") )
			cal -= 0x10;

		cal *= dcdc_get_trimming_step(rdev, 0);	/*uV */
	}

	debug2("regu %p (%s) %d +%dmv\n", regs, desc->desc.name, mv, cal / 1000);

	return (mv * 1000 + cal) /*uV */;
}

/* standard boost ops*/
#define MAX_CURRENT_SINK	(500)	/*FIXME: max current sink */
static int boost_set_current_limit(struct regulator_dev *rdev, int min_uA,
				   int max_uA)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int ma = min_uA / 1000;
	int ret = -EACCES;
	int i, shft = __ffs(regs->vol_ctl_bits);
	int trim = (int)regs->vol_def / 1000;
	int steps = (regs->vol_ctl_bits >> shft) + 1;

	debug("regu %p (%s) %d %d\n", regs, desc->desc.name, min_uA, max_uA);

	if (!regs->vol_ctl)
		goto exit;

	if (trim > 0) {
		trim <<= __ffs(regs->vol_trm_bits);
	}

	i = ma * steps / MAX_CURRENT_SINK;
	if (i >= 0 && i < steps) {
		ANA_REG_SET(regs->vol_ctl, (i << shft) | trim,
			    regs->vol_ctl_bits | regs->vol_trm_bits);

		ret = 0;
	}

	WARN(0 != ret,
	     "warning: regulator (%s) not support %dmA\n", desc->desc.name, ma);

exit:
	return ret;
}

static int boost_get_current_limit(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	u32 cur;
	int i, shft = __ffs(regs->vol_ctl_bits);
	int steps = (regs->vol_ctl_bits >> shft) + 1;

	debug0("regu %p (%s), vol ctl %08x, shft %d, mask %08x\n",
	       regs, desc->desc.name, regs->vol_ctl, shft, regs->vol_ctl_bits);

	if (!regs->vol_ctl)
		return -EACCES;

	i = ((ANA_REG_GET(regs->vol_ctl) & regs->vol_ctl_bits) >> shft);
	cur = i * MAX_CURRENT_SINK / steps;
	debug2("regu %p (%s), current %d\n", regs, desc->desc.name, cur);
	return cur * 1000;
}

static int adc_sample_bit = 1;	/*12bits mode */
static short adc_data[3][2]
#if defined(CONFIG_REGULATOR_ADC_DEBUG)
    = {
	{4200, 3320},		/* same as nv adc_t */
	{3600, 2844},
	{400, 316},		/* 0.4@VBAT, Reserved IdealC Value */
}
#endif
;

static int __is_valid_adc_cal(void)
{
	return 0 != adc_data[0][0];
}

static int __init __adc_cal_setup(char *str)
{
	u32 *p = (u32 *) adc_data;
	*p = simple_strtoul(str, &str, 0);
	if (*p++ && *++str) {
		*p = simple_strtoul(str, &str, 0);
		if (*p) {
			debug("%d : %d -- %d : %d\n",
			      (int)adc_data[0][0], (int)adc_data[0][1],
			      (int)adc_data[1][0], (int)adc_data[1][1]);
			if (adc_data[0][1] < BIT(10)
			    && adc_data[1][1] < BIT(10))
				adc_sample_bit = 0;	/*10bits mode */
		}
	}
	return 0;
}
early_param("adc_cal", __adc_cal_setup);

static int __init __adc_cal_fuse_setup(void)
{
	if (!__is_valid_adc_cal() &&
	    sci_efuse_calibration_get((u32 *) adc_data)) {
		debug("%d : %d -- %d : %d\n",
		      (int)adc_data[0][0], (int)adc_data[0][1],
		      (int)adc_data[1][0], (int)adc_data[1][1]);
	}

	return 0;
}

static int __adc2vbat(int adc_res)
{
	int t = adc_data[0][0] - adc_data[1][0];
	t *= (adc_res - adc_data[0][1]);
	t /= (adc_data[0][1] - adc_data[1][1]);
	t += adc_data[0][0];
	return t;
}

#define MEASURE_TIMES	(15)

static void __dump_adc_result(u32 adc_val[])
{
#if defined(CONFIG_REGULATOR_ADC_DEBUG)
	int i;
	for (i = 0; i < MEASURE_TIMES; i++) {
		printk("%d ", adc_val[i]);
	}
	printk("\n");
#endif
}

static int cmp_val(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

/**
 * __adc_voltage - get regulator output voltage through auxadc
 * @regulator: regulator source
 *
 * This returns the current regulator voltage in mV.
 *
 * NOTE: If the regulator is disabled it will return the voltage value. This
 * function should not be used to determine regulator state.
 */
static int regu_adc_voltage(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;

	int ret, adc_chan = regs->cal_ctl_bits >> 16;
	u16 ldo_cal_sel = regs->cal_ctl_bits & 0xFFFF;
	u32 adc_res, adc_val[MEASURE_TIMES];
	u32 chan_numerators = 1, chan_denominators = 1;
	u32 bat_numerators, bat_denominators;

	struct adc_sample_data data = {
		.channel_id = adc_chan,
		.channel_type = 0,	/*sw */
		.hw_channel_delay = 0,	/*reserved */
		.scale = 1,	/*big scale */
		.pbuf = &adc_val[0],
		.sample_num = MEASURE_TIMES,
		.sample_bits = adc_sample_bit,
		.sample_speed = 0,	/*quick mode */
		.signal_mode = 0,	/*resistance path */
	};

	if (!__is_valid_adc_cal())
		return -EACCES;

	if (!regs->cal_ctl)
		return -EINVAL;

	/* enable ldo cal before adc sampling and ldo calibration */
	if (0 == regs->typ) {
		mutex_lock(&adc_chan_mutex);
		ANA_REG_OR(regs->cal_ctl, ldo_cal_sel);
		debug0("%s adc channel %d : %04x\n",
		       desc->desc.name, data.channel_id, ldo_cal_sel);
	}

	ret = sci_adc_get_values(&data);
	BUG_ON(0 != ret);

	/* close ldo cal and release multiplexed aux adc channel */
	if (0 == regs->typ) {
		ANA_REG_BIC(regs->cal_ctl, ldo_cal_sel);
		mutex_unlock(&adc_chan_mutex);
	}

	__dump_adc_result(adc_val);
	sort(adc_val, MEASURE_TIMES, sizeof(u32), cmp_val, 0);
	/*__dump_adc_result(adc_val);*/

	sci_adc_get_vol_ratio(data.channel_id, data.scale,
			      &chan_numerators, &chan_denominators);

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);

	adc_res = adc_val[MEASURE_TIMES / 2];
	debug("%s adc channel %d : 0x%04x, ratio (%d/%d), result value %d\n",
	      desc->desc.name, data.channel_id, ldo_cal_sel,
	      chan_numerators, chan_denominators, adc_res);

	if (adc_res == 0)
		return -EAGAIN;
	else
		return __adc2vbat(adc_res)
		    * (bat_numerators * chan_denominators)
		    / (bat_denominators * chan_numerators);
}

/**
 * regulator_strongly_disable - strongly disable regulator output
 * @regulator: regulator source
 *
 * Strongly try disable the regulator output voltage or current.
 * NOTE: this *will* disable the regulator output even if other consumer
 * devices have it enabled. This should be used for situations when device
 * had unbalanced with calls to regulator_enable().
 * *Not* recommended to call this function before try to balance the use_count.
 */
int regulator_strongly_disable(struct regulator *regulator)
{
	struct regulator_dev *rdev = regulator_get_drvdata(regulator);
	int ret = 0;

	if (rdev)
		while (rdev->use_count--)
			regulator_disable(regulator);

	return ret;
}

EXPORT_SYMBOL_GPL(regulator_strongly_disable);

static struct regulator_ops ldo_ops = {
	.enable = ldo_turn_on,
	.disable = ldo_turn_off,
	.is_enabled = ldo_is_on,
	.set_voltage = ldo_set_voltage,
	.get_voltage = ldo_get_voltage,
	.set_mode = ldo_set_mode,
/*	.enable_time = ldo_enable_time, */
};

static struct regulator_ops dcdc_ops = {
	.enable = ldo_turn_on,
	.disable = ldo_turn_off,
	.is_enabled = ldo_is_on,
	.set_voltage = dcdc_set_voltage,
	.get_voltage = dcdc_get_voltage,
};

static struct regulator_ops boost_ops = {
	.enable = ldo_turn_on,
	.disable = ldo_turn_off,
	.is_enabled = ldo_is_on,
	.set_current_limit = boost_set_current_limit,
	.get_current_limit = boost_get_current_limit,
	.set_mode = ldo_set_mode,
};

/*
 * Consider the following machine :-
 *
 *   Regulator-1 -+-> [Consumer A @ 1.8V]
 *                |
 *                +-> [Consumer B @ 1.8V]
 *
 *   Regulator-2 ---> [Consumer C @ 3.3V]
 *
 * The drivers for consumers A & B must be mapped to the correct regulator in
 * order to control their power supply. This mapping can be achieved in board/machine
 * initialisation code by creating a struct regulator_consumer_supply for each regulator.
 * Alternatively, we built a regulator supply-consumers map, the format is as follow:
 *
 *      supply source-1, consumer A, consumer B, ..., NULL
 *      supply source-2, consumer C, ..., NULL
 *      ...
 *      NULL
 *
 */
static struct regulator_consumer_supply *set_supply_map(struct device *dev,
							const char *supply_name,
							int *num)
{
	char **map = (char **)dev_get_platdata(dev);
	int i, n;
	struct regulator_consumer_supply *consumer_supplies = NULL;

	if (!supply_name || !(map && map[0]))
		return NULL;

	for (i = 0; map[i] || map[i + 1]; i++) {
		if (map[i] && 0 == strcmp(map[i], supply_name))
			break;
	}

	/* i++; *//* Do not skip supply name */

	for (n = 0; map[i + n]; n++) ;

	if (n) {
		debug0("supply %s consumers %d - %d\n", supply_name, i, n);
		consumer_supplies =
		    kzalloc(n * sizeof(*consumer_supplies), GFP_KERNEL);
		BUG_ON(!consumer_supplies);
		for (n = 0; map[i]; i++, n++) {
			consumer_supplies[n].supply = map[i];
		}
		if (num)
			*num = n;
	}
	return consumer_supplies;
}

#if defined(CONFIG_DEBUG_FS)
static struct dentry *debugfs_root = NULL;

static u32 ana_addr = 0;
static int debugfs_ana_addr_get(void *data, u64 * val)
{
	if (ana_addr < PAGE_SIZE) {
		*val = ANA_REG_GET(ana_addr + (ANA_REGS_GLB_BASE & PAGE_MASK));
	} else {
		void *addr = ioremap(ana_addr, PAGE_SIZE);
		*val = __raw_readl(addr);
		iounmap(addr);
	}
	return 0;
}

static int debugfs_ana_addr_set(void *data, u64 val)
{
	if (ana_addr < PAGE_SIZE) {
		ANA_REG_SET(ana_addr + (ANA_REGS_GLB_BASE & PAGE_MASK), val,
			    -1);
	} else {
		void *addr = ioremap(ana_addr, PAGE_SIZE);
		__raw_writel(val, addr);
		iounmap(addr);
	}
	return 0;
}

static int adc_chan = 5 /*VBAT*/;
static int debugfs_adc_chan_get(void *pdata, u64 * val)
{
	int i, ret;
	u32 adc_res, adc_val[MEASURE_TIMES];
	struct adc_sample_data data = {
		.channel_id = adc_chan,
		.channel_type = 0,	/*sw */
		.hw_channel_delay = 0,	/*reserved */
		.scale = 1,	/*big scale */
		.pbuf = &adc_val[0],
		.sample_num = MEASURE_TIMES,
		.sample_bits = adc_sample_bit,
		.sample_speed = 0,	/*quick mode */
		.signal_mode = 0,	/*resistance path */
	};

	ret = sci_adc_get_values(&data);
	BUG_ON(0 != ret);

	for (i = 0; i < MEASURE_TIMES; i++) {
		printk("%d ", adc_val[i]);
	}
	printk("\n");

	sort(adc_val, MEASURE_TIMES, sizeof(u32), cmp_val, 0);
	adc_res = adc_val[MEASURE_TIMES / 2];
	pr_info("adc chan %d, result value %d, vbat %d\n",
		data.channel_id, adc_res, __adc2vbat(adc_res));
	*val = adc_res;
	return 0;
}

static int debugfs_adc_chan_set(void *data, u64 val)
{
	adc_chan = val;
	return 0;
}

static int debugfs_enable_get(void *data, u64 * val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->is_enabled)
		*val = rdev->desc->ops->is_enabled(rdev);
	else
		*val = -1;
	return 0;
}

static int debugfs_enable_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->enable)
		(val) ? rdev->desc->ops->enable(rdev)
		    : rdev->desc->ops->disable(rdev);
	return 0;
}

static int debugfs_voltage_get(void *data, u64 * val)
{
	struct regulator_dev *rdev = data;
	if (rdev)
		*val = regu_adc_voltage(rdev);
	else
		*val = -1;
	return 0;
}

static int debugfs_voltage_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->set_voltage) {
		u32 min_uV = (u32)val * 1000;
		min_uV += rdev->constraints->uV_offset;
		rdev->desc->ops->set_voltage(rdev, min_uV, min_uV, 0);
	}
	return 0;
}

static int debugfs_boost_get(void *data, u64 * val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->get_current_limit)
		*val = rdev->desc->ops->get_current_limit(rdev) / 1000;
	else
		*val = -1;
	return 0;
}

static int debugfs_boost_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->set_current_limit)
		rdev->desc->ops->set_current_limit(rdev, val * 1000,
						   val * 1000);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_ana_addr,
			debugfs_ana_addr_get, debugfs_ana_addr_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_adc_chan,
			debugfs_adc_chan_get, debugfs_adc_chan_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_enable,
			debugfs_enable_get, debugfs_enable_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_ldo,
			debugfs_voltage_get, debugfs_voltage_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_boost,
			debugfs_boost_get, debugfs_boost_set, "%llu\n");

static void rdev_init_debugfs(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	desc->debugfs = debugfs_create_dir(rdev->desc->name, debugfs_root);
	if (IS_ERR(rdev->debugfs) || !rdev->debugfs) {
		pr_warn("Failed to create debugfs directory\n");
		rdev->debugfs = NULL;
		return;
	}

	debugfs_create_file("enable", S_IRUGO | S_IWUSR,
			    desc->debugfs, rdev, &fops_enable);

	if (desc->desc.type == REGULATOR_CURRENT)
		debugfs_create_file("current", S_IRUGO | S_IWUSR,
				    desc->debugfs, rdev, &fops_boost);
	else
		debugfs_create_file("voltage", S_IRUGO | S_IWUSR,
				    desc->debugfs, rdev, &fops_ldo);
}
#else
static void rdev_init_debugfs(struct regulator_dev *rdev)
{
}
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#define reg_info(format, arg...) pr_info("reg: " "@@@%s: " format, __func__, ## arg)
#endif

static inline int __strcmp(const char *cs, const char *ct)
{
	if (!cs || !ct) return -1;
	return strcmp(cs, ct);
}

void *sci_regulator_register(struct platform_device *pdev,
				       struct sci_regulator_desc *desc)
{
	static atomic_t idx = ATOMIC_INIT(1);	/* 0: dummy */
	struct regulator_dev *rdev;
	struct regulator_ops *__regs_ops[] = {
		&ldo_ops, 0, &dcdc_ops, 0 /*lpref_ops */ , &boost_ops,
		0,
	};
	struct regulator_consumer_supply consumer_supplies_default[] = {
		[0] = {
		       .supply = desc->desc.name,
		       }
	};
#ifndef CONFIG_OF
	struct regulator_init_data init_data = {
		.supply_regulator = 0,
		.constraints = {
				.min_uV = 0,
				.max_uV = 4200 * 1000,
				.valid_modes_mask =
				REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
				.valid_ops_mask =
				REGULATOR_CHANGE_STATUS |
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_MODE,
				},
		.num_consumer_supplies = 1,
		.consumer_supplies = consumer_supplies_default,
		.regulator_init = 0,
		.driver_data = 0,
	};

	struct regulator_config config = { };

	if (sci_ana_chip_ver_is_compatible(SC2711AA)) {
		if (0 == strcmp(desc->desc.name, "vdd18") ||
			0 == strcmp(desc->desc.name, "vddemmcio") ||
			0 == strcmp(desc->desc.name, "vddcamd") ||
			0 == strcmp(desc->desc.name, "vddcamio")) {

			desc->regs->vol_sel[0] = 1400;
			debug("%s vol base %d\n", desc->desc.name, desc->regs->vol_sel[0]);
		}
	}

#else
	struct regulator_config config = { };
	struct regulator_init_data *init_data;
	struct device_node *dev_np;
	struct device_node *node_np;
	dev_np = pdev->dev.of_node;
	node_np = of_get_child_by_name(dev_np, desc->desc.name);
	init_data = of_get_regulator_init_data(&pdev->dev, node_np);
	if(!init_data || 0 != __strcmp(init_data->constraints.name, desc->desc.name)){
		dev_err(&pdev->dev, "out of memory or %s not found\n", desc->desc.name);
		return NULL;
	}
	reg_info("[%d]%s range %d - %d\n", idx.counter, init_data->constraints.name,
		init_data->constraints.min_uV, init_data->constraints.max_uV);

	init_data->supply_regulator = 0;
	init_data->constraints.min_uV = 0,
	init_data->constraints.max_uV = 4200 * 1000;
	init_data->constraints.valid_modes_mask = 
		REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY;
	init_data->constraints.valid_ops_mask = REGULATOR_CHANGE_MODE |
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE;
	init_data->num_consumer_supplies = 1;
	init_data->consumer_supplies = consumer_supplies_default;
#endif

	desc->desc.id = atomic_inc_return(&idx) - 1;

	if (0 == strcmp(desc->desc.name, "vddrf0")) {
		if (ana_status_bondopt4) { /* BONDOPT4 */
			desc->regs->vol_def = 2800;
		} else {
			desc->regs->vol_def = 1800;
		}
	}

	BUG_ON(desc->regs->pd_set
	       && desc->regs->pd_set == desc->regs->pd_rst
	       && desc->regs->pd_set_bit == desc->regs->pd_rst_bit);

	BUG_ON(desc->regs->typ >= ARRAY_SIZE(__regs_ops));
	if (!desc->desc.ops)
		desc->desc.ops = __regs_ops[desc->regs->typ];

#ifndef CONFIG_OF
	init_data.consumer_supplies =
	    set_supply_map(&pdev->dev, desc->desc.name,
			   &init_data.num_consumer_supplies);

	if (!init_data.consumer_supplies)
		init_data.consumer_supplies = consumer_supplies_default;

	debug0("regu %p (%s)\n", desc->regs, desc->desc.name);
        config.dev = &pdev->dev;
        config.init_data = &init_data;
        config.driver_data = NULL;
        config.of_node = NULL;
        rdev = regulator_register(&desc->desc, &config);
	if (init_data.consumer_supplies != consumer_supplies_default)
		kfree(init_data.consumer_supplies);
#else
	init_data->consumer_supplies =
	    set_supply_map(&pdev->dev, desc->desc.name,
			   &init_data->num_consumer_supplies);

	if (!init_data->consumer_supplies)
		init_data->consumer_supplies = consumer_supplies_default;

	debug0("regu %p (%s)\n", desc->regs, desc->desc.name);
        config.dev = &pdev->dev;
        config.init_data = init_data;
        config.driver_data = NULL;
        config.of_node = node_np;
        rdev = regulator_register(&desc->desc, &config);
	if (init_data->consumer_supplies != consumer_supplies_default)
		kfree(init_data->consumer_supplies);
#endif

	if (!IS_ERR(rdev)) {
		rdev->reg_data = rdev;
		desc->data.rdev = rdev;
		__init_trimming(rdev);
		rdev_init_debugfs(rdev);
	}
	return rdev;
}

/**
 * IMPORTANT!!!
 * spreadtrum power regulators is intergrated on the chip, include LDOs and DCDCs.
 * so i autogen all regulators non-variable description in plat or mach directory,
 * which named __xxxx_regulator_map.h, BUT register all in regulator driver probe func,
 * just like other regulator vendor drivers.
 */
static int sci_regulator_probe(struct platform_device *pdev)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_root =
		debugfs_create_dir(REGULATOR_ROOT_DIR, NULL);
	if (IS_ERR(debugfs_root) || !debugfs_root) {
		WARN(!debugfs_root,
			 "%s: Failed to create debugfs directory\n", REGULATOR_ROOT_DIR);
		debugfs_root = NULL;
	}

	/* compatible with 8810 adc test */
	debugfs_create_u32("ana_addr", S_IRUGO | S_IWUSR,
			   debugfs_root, (u32 *) & ana_addr);
	debugfs_create_file("ana_valu", S_IRUGO | S_IWUSR,
				debugfs_root, &ana_addr, &fops_ana_addr);
	debugfs_create_file("adc_chan", S_IRUGO | S_IWUSR,
				debugfs_root, &adc_chan, &fops_adc_chan);
	debugfs_create_u64("adc_data", S_IRUGO | S_IWUSR,
			   debugfs_root, (u64 *) & adc_data);

	{ /* vddarm/vddcore/vddmem common debugfs interface */
		char str[NAME_MAX];
		struct dentry *vol_root = debugfs_create_dir("vol", NULL);
		sprintf(str, "../%s/vddarm/voltage", REGULATOR_ROOT_DIR);
		debugfs_create_symlink("dcdcarm", vol_root, str);
		sprintf(str, "../%s/vddcore/voltage", REGULATOR_ROOT_DIR);
		debugfs_create_symlink("dcdccore", vol_root, str);
		sprintf(str, "../%s/vddmem/voltage", REGULATOR_ROOT_DIR);
		debugfs_create_symlink("dcdcmem", vol_root, str);
	}
#endif

	ana_chip_id = (sci_get_ana_chip_id() | sci_get_ana_chip_ver());
	ana_status_bondopt4 = (ANA_REG_GET(ANA_REG_GLB_LDO_V_CTRL9) >> 15) & 0x1;

	pr_info("sc271x ana chip id: (0x%08x), bond opt (0x%08x)\n", ana_chip_id, ana_status_bondopt4);

#include CONFIG_REGULATOR_SPRD_MAP

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id sprd_regulator_of_match[] = {
	{ .compatible = "sprd,sc2711-regulator", },
	{ }
};
#endif

static struct platform_driver sci_regulator_driver = {
	.driver = {
		   .name = "sc2711-regulator",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(sprd_regulator_of_match),
		   },
	.probe = sci_regulator_probe,
};

static int __init regu_driver_init(void)
{
	__adc_cal_fuse_setup();

	return platform_driver_register(&sci_regulator_driver);
}

int __init sci_regulator_init(void)
{
#ifndef CONFIG_OF
	static struct platform_device regulator_device = {
		.name = "sc2711-regulator",
		.id = -1,
	};

	sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,
			  BITS_PWR_WR_PROT_VALUE(0x6e7f));
	WARN_ON(!
		(sci_adi_read(ANA_REG_GLB_PWR_WR_PROT_VALUE) &
		 BIT_PWR_WR_PROT));
	sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE, 0);

	__adc_cal_fuse_setup();
	return platform_device_register(&regulator_device);
#else
	return of_platform_populate(of_find_node_by_path("/sprd-regulators"),
			sprd_regulator_of_match, NULL, NULL);
#endif
}

subsys_initcall(regu_driver_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum voltage regulator driver");
MODULE_AUTHOR("robot <zhulin.lian@spreadtrum.com>");
MODULE_VERSION("0.2");
