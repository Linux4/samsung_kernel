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
 *		0.4
 *		Bug#183980 add dcdc and pll enable time
 *		Change-Id: I6e6e06ee0beb306cd846964d0ba24aef449e5beb
 *		0.3
 *		Bug#164001 add dcdc mem/gen/wpa/wrf map
 *		Change-Id: I07dac5700c0907aca99f6112bd4b5799358a9a88
 *		0.2
 *		Bug#164001 shark dcam: add camera ldo calibration
 * 		Change-Id: Icaee2706b8b0985ae6f3122b236d8e278dcc0db2
 *		0.1
 *		sc8830: fix adc cal data from cmdline fail
 *		Change-Id: Id85d58178aca40fdf13b996853711e92e1171801
 *
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

/**
 * struct sci_regulator_ops - sci regulator operations.
 *
 * @trimming:
 *
 * This struct describes regulator operations which can be implemented by
 * regulator chip drivers.
 */
struct sci_regulator_ops {
	int trimming_def_val;	/* trimming controller default value in A-Die */
	int (*get_trimming_step) (struct regulator_dev * rdev, int);
	int (*set_trimming) (struct regulator_dev * rdev, int, int, int);
	int (*calibrate) (struct regulator_dev * rdev, int, int);
};

struct sci_regulator_data {
	struct delayed_work dwork;
	struct regulator_dev *rdev;
};

struct sci_regulator_desc {
	struct regulator_desc desc;
	struct sci_regulator_ops *ops;
	const struct sci_regulator_regs *regs;
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

static DEFINE_MUTEX(adc_chan_mutex);
static int __is_trimming(struct regulator_dev *);
static int __regu_calibrate(struct regulator_dev *, int, int);
extern int sci_efuse_calibration_get(u32 * p_cal_data);

#define SCI_REGU_REG(VDD, TYP, PD_SET, SET_BIT, PD_RST, RST_BIT, SLP_CTL, SLP_CTL_BIT, \
                     VOL_TRM, VOL_TRM_BITS, CAL_CTL, CAL_CTL_BITS, VOL_DEF, \
                     VOL_CTL, VOL_CTL_BITS, VOL_SEL_CNT, ...)   \
do { 														\
	static const struct sci_regulator_regs REGS_##VDD = {	\
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

	if (regs->pd_rst)
		ANA_REG_OR(regs->pd_rst, regs->pd_rst_bit);

	if (regs->pd_set)
		ANA_REG_BIC(regs->pd_set, regs->pd_set_bit);

	debug2("regu %p (%s), turn on\n", regs, desc->desc.name);
	/* ldo trimming when first turn on */
	if (desc->ops && !__is_trimming(rdev))
		__regu_calibrate(rdev, 0, 0);
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
	if (regs->pd_set)
		ANA_REG_OR(regs->pd_set, regs->pd_set_bit);

	if (regs->pd_rst)
		ANA_REG_BIC(regs->pd_rst, regs->pd_rst_bit);
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
	static const int vol_bits[4] = { 0xa, 0x9, 0x6, 0x5 };
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int mv = min_uV / 1000;
	int ret = -EINVAL;
	int i, shft = __ffs(regs->vol_ctl_bits);
	int has_rst_bit = !(0x3 == (regs->vol_ctl_bits >> shft));	/* new feature */

	BUG_ON(regs->vol_sel_cnt > 4);
	debug("regu %p (%s) %d %d (%d)\n", regs, desc->desc.name, min_uV,
	      max_uV, has_rst_bit);

	if (!regs->vol_ctl)
		return -EACCES;
	for (i = 0; i < regs->vol_sel_cnt; i++) {
		if (regs->vol_sel[i] == mv) {
			ANA_REG_SET(regs->vol_ctl,
				    ((!has_rst_bit) ? i : vol_bits[i]) << shft,
				    regs->vol_ctl_bits);
			/*clear_bit(desc->desc.id, trimming_state); */
			ret = 0;
			break;
		}
	}

	WARN(0 != ret,
	     "warning: regulator (%s) not support %dmV\n", desc->desc.name, mv);
	return ret;
}

static int ldo_get_voltage(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	u32 vol, vol_bits;
	int i, shft = __ffs(regs->vol_ctl_bits);
	int has_rst_bit = !(0x3 == (regs->vol_ctl_bits >> shft));	/* new feature */

	debug0("regu %p (%s), vol ctl %08x, shft %d, mask %08x\n",
	       regs, desc->desc.name, regs->vol_ctl, shft, regs->vol_ctl_bits);

	if (!regs->vol_ctl)
		return -EACCES;

	BUG_ON(regs->vol_sel_cnt != 4);
	vol_bits = ((ANA_REG_GET(regs->vol_ctl) & regs->vol_ctl_bits) >> shft);

	if (!has_rst_bit) {
		i = vol_bits;
	} else if (((vol_bits & BIT(0)) ^ (vol_bits & BIT(1))
		    && (vol_bits & BIT(2)) ^ (vol_bits & BIT(3)))) {
		i = (vol_bits & BIT(0)) | ((vol_bits >> 1) & BIT(1));
	} else
		return -EFAULT;

	vol = regs->vol_sel[i];
	debug2("regu %p (%s), voltage %d\n", regs, desc->desc.name, vol);
	return vol * 1000;
}

static unsigned long trimming_state[2] = { 0, 0 };	/* max 64 bits */

/* FIXME: get dcdc cal offset config from uboot */
#define DCDC_CAL_CONF_BASE	(SPRD_IRAM0_BASE + 0x1f00)
#define DCDC_MAX_CNT		(4)
struct dcdc_cal_t {
	char name[32];
	int cal_vol;
};

static int __dcdc_get_offset(struct regulator_dev *rdev)
{
#ifdef CONFIG_ARCH_SCX35
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct dcdc_cal_t *dcdc = (struct dcdc_cal_t *)DCDC_CAL_CONF_BASE;
	int i;
	for (i = 0; i < DCDC_MAX_CNT; i++) {
		if (0 == strcmp(dcdc[i].name, desc->desc.name)) {
			debug("regu %p (%s) offset %+dmV\n", desc->regs,
			      desc->desc.name, dcdc[i].cal_vol);
			return dcdc[i].cal_vol * 1000;	/*uV */
		}
	}
#endif
	return 0;
}

static int __is_trimming(struct regulator_dev *rdev)
{
	int id;
	BUG_ON(!rdev);
	id = rdev->desc->id;
	BUG_ON(!(id > 0 && id < sizeof(trimming_state) * 8));
	return test_bit(id, trimming_state);
}

static int __init_trimming(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int ret = -EINVAL;
	u32 trim = 0;

	if (!regs->vol_trm || !desc->ops)
		goto exit;

	trim = (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits)
	    >> __ffs(regs->vol_trm_bits);
	if (0 == regs->vol_def && desc->regs->typ == 2 /*DCDC*/) {
		rdev->constraints->uV_offset = __dcdc_get_offset(rdev);
	} else if (trim != desc->ops->trimming_def_val && !(regs->vol_def & 1)) {
		/* some DCDC/LDOs had been calibrated in uboot-spl */
		debug("regu %p (%s) trimming ok before startup\n", regs,
		      desc->desc.name);
		set_bit(desc->desc.id, trimming_state);
		ret = trim;
	} else
		ret = __regu_calibrate(rdev, 0, 0);

exit:
	return ret;
}

/**
 * ldo trimming step about 0.625%, range 90% ~ 109.375%. that all maps as follow.

	0x1F :  +9.375 : 109.375
	0x1E :  +8.750 : 108.750
	0x1D :  +8.125 : 108.125
	0x1C :  +7.500 : 107.500
	0x1B :  +6.875 : 106.875
	0x1A :  +6.250 : 106.250
	0x19 :  +5.625 : 105.625
	0x18 :  +5.000 : 105.000
	0x17 :  +4.375 : 104.375
	0x16 :  +3.750 : 103.750
	0x15 :  +3.125 : 103.125
	0x14 :  +2.500 : 102.500
	0x13 :  +1.875 : 101.875
	0x12 :  +1.250 : 101.250
	0x11 :  +0.625 : 100.625
	0x10 :  +0.000 : 100.000
	0x0F :  -0.625 : 99.375
	0x0E :  -1.250 : 98.750
	0x0D :  -1.875 : 98.125
	0x0C :  -2.500 : 97.500
	0x0B :  -3.125 : 96.875
	0x0A :  -3.750 : 96.250
	0x09 :  -4.375 : 95.625
	0x08 :  -5.000 : 95.000
	0x07 :  -5.625 : 94.375
	0x06 :  -6.250 : 93.750
	0x05 :  -6.875 : 93.125
	0x04 :  -7.500 : 92.500
	0x03 :  -8.125 : 91.875
	0x02 :  -8.750 : 91.250
	0x01 :  -9.375 : 90.625
	0x00 : -10.000 : 90.000

 */
static int ldo_set_trimming(struct regulator_dev *rdev, int def_vol, int to_vol,
			    int adc_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int ret = -EINVAL;

	/* FIXME: always update voltage ctrl bits */
/*
	ret =
	    rdev->desc->ops->set_voltage(rdev, to_vol * 1000, to_vol * 1000, 0);
	if (IS_ERR_VALUE(ret) && regs->vol_ctl)
		goto exit;
*/
	if (regs->vol_trm) {
		u32 trim =	/* assert 5 valid trim bits, R = V_IDEAL / V_ADCIN - 1 */
		    DIV_ROUND_UP((to_vol * 100 - adc_vol * 90) * 32,
				 (adc_vol * 20));
		if (trim > BIT(5) - 1)
			goto exit;
		debug("regu %p (%s) trimming %d = %d %+d%%, got [%02X]\n",
		      regs, desc->desc.name, to_vol, adc_vol,
		      (trim * 20 / 32 - 10), trim);

#if !defined(CONFIG_REGULATOR_CAL_DUMMY)
		ANA_REG_SET(regs->vol_trm,
			    trim << __ffs(regs->vol_trm_bits),
			    regs->vol_trm_bits);
		ret = 0;
#endif
	}

exit:
	return ret;
}

static int dcdcldo_set_trimming(struct regulator_dev *rdev, int def_vol,
				int to_vol, int adc_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int ret = -EINVAL;

	if (regs->vol_trm) {
		u32 trim = desc->ops->trimming_def_val;
		if (adc_vol > to_vol) {
			trim -=
			    ((adc_vol - to_vol) * 100 * 32) / (adc_vol * 25);
		} else {
			trim +=
			    DIV_ROUND_UP((to_vol - adc_vol) * 100 * 32,
					 (adc_vol * 25));
		}
		if (trim > BIT(5) - 1)
			goto exit;
		debug("regu %p (%s) trimming %d = %d %+d%%, got [%02X]\n",
		      regs, desc->desc.name, to_vol, adc_vol,
		      ((int)trim - 0x10) * 25 / 32, trim);

#if !defined(CONFIG_REGULATOR_CAL_DUMMY)
		ANA_REG_SET(regs->vol_trm,
			    trim << __ffs(regs->vol_trm_bits),
			    regs->vol_trm_bits);
		ret = 0;
#endif
	}

exit:
	return ret;
}

static int ldo_get_trimming_step(struct regulator_dev *rdev, int to_vol)
{
	return 1000 * to_vol * 20 / 32;	/*uV */
}

static int dcdcldo_get_trimming_step(struct regulator_dev *rdev, int to_vol)
{
	return 1000 * to_vol * 25 / 32;	/*uV */
}

/* FIXME: patch for sc7710 BA version */
#ifdef CONFIG_ARCH_SC7710
/**
 * lpref trimming step about 1.39%, range -22.22% ~ +20.83%. that all maps as follow.
	0x0F : +20.833 : 120.833
	0x0E : +19.444 : 119.444
	0x0D : +18.056 : 118.056
	0x0C : +16.667 : 116.667
	0x0B : +15.278 : 115.278
	0x0A : +13.889 : 113.889
	0x09 : +12.500 : 112.500
	0x08 : +11.111 : 111.111
	0x07 :  +9.722 : 109.722
	0x06 :  +8.333 : 108.333
	0x05 :  +6.944 : 106.944
	0x04 :  +5.556 : 105.556
	0x03 :  +4.167 : 104.167
	0x02 :  +2.778 : 102.778
	0x01 :  +1.389 : 101.389
	0x00 :  +0.000 : 100.000
	0x1F :  -1.389 : 98.611
	0x1E :  -2.778 : 97.222
	0x1D :  -4.167 : 95.833
	0x1C :  -5.556 : 94.444
	0x1B :  -6.944 : 93.056
	0x1A :  -8.333 : 91.667
	0x19 :  -9.722 : 90.278
	0x18 : -11.111 : 88.889
	0x17 : -12.500 : 87.500
	0x16 : -13.889 : 86.111
	0x15 : -15.278 : 84.722
	0x14 : -16.667 : 83.333
	0x13 : -18.056 : 81.944
	0x12 : -19.444 : 80.556
	0x11 : -20.833 : 79.167
	0x10 : -22.222 : 77.778
*/
static int lpref_set_trimming(struct regulator_dev *rdev, int def_vol,
			      int to_vol, int adc_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int ret = -EINVAL;
	u32 set_bits, rst_bits;

	u32 trim =		/* assert 5 valid trim bits, R = V_IDEAL / V_ADCIN - 1 */
	    (abs(to_vol - adc_vol) * 72 / adc_vol);

	if (to_vol < adc_vol)
		trim = ~trim & 0x1f;
	if (trim < BIT(5) - 1)
		trim++;		/* FIXME: a little higher, according asic */
	else if (trim > BIT(5) - 1)
		goto exit;

	debug("regu %p (%s) trimming %d = %d %+d%%, got [%02X]\n",
	      regs, desc->desc.name, to_vol, adc_vol,
	      (100 - adc_vol * 100 / to_vol), trim);

#if !defined(CONFIG_REGULATOR_CAL_DUMMY)
	mutex_lock(&adc_chan_mutex);
	ANA_REG_BIC(ANA_REG_GLB_LDO_SW, BIT_WPA_DCDC_SEL);
	switch (regs->cal_ctl_bits >> 16) {
	case 0x1a:		//abb/rf/amp/emmcore/emmio/vddwif0/vddwif1
		set_bits = (trim >> 0) & (BIT(0) | BIT(1));
		ANA_REG_SET(ANA_REG_GLB_CHGR_CTRL0, set_bits << 8,
			    BIT(9) | BIT(8));
		set_bits = (trim >> 2) & (BIT(0) | BIT(1));
		rst_bits = ~set_bits & (BIT(0) | BIT(1));
		ANA_REG_SET(ANA_REG_GLB_DCDC_MEM_CTRL2,
			    set_bits << 10 | rst_bits << 14,
			    BIT(10) | BIT(11) | BIT(14) | BIT(15));
		set_bits = (trim >> 4) & BIT(0);
		rst_bits = ~set_bits & BIT(0);
		ANA_REG_SET(ANA_REG_GLB_WPA_DCDC_CTRL1,
			    set_bits << 10 | rst_bits << 14, BIT(10) | BIT(14));
		break;
	case 0x19:		//vdd18/vdd28/vdd25/vddcama/vddcamd/vddmem/vddusb
		trim ^= BIT(4);
		ANA_REG_SET(ANA_REG_GLB_LDO_TRIM6, trim << 8,
			    BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(12));
		break;
	case 0x18:		//sdio/sim0/sim1/sim2
		set_bits = (trim >> 0) & (BIT(0) | BIT(1));
		rst_bits = ~set_bits & (BIT(0) | BIT(1));
		ANA_REG_SET(ANA_REG_GLB_DCDC_ARM_CTRL2,
			    set_bits << 10 | rst_bits << 14,
			    BIT(10) | BIT(11) | BIT(14) | BIT(15));
		set_bits = (trim >> 2) & (BIT(0) | BIT(1));
		rst_bits = ~set_bits & (BIT(0) | BIT(1));
		ANA_REG_SET(ANA_REG_GLB_DCDC_CORE_CTRL2,
			    set_bits << 10 | rst_bits << 14,
			    BIT(10) | BIT(11) | BIT(14) | BIT(15));
		set_bits = (trim >> 4) & BIT(0);
		rst_bits = ~set_bits & BIT(0);
		ANA_REG_SET(ANA_REG_GLB_WPA_DCDC_CTRL1,
			    set_bits << 11 | rst_bits << 15, BIT(11) | BIT(15));
		break;
	default:
		break;
	}

	msleep(1);		/* FIXME: wait for lpref voltage is okay */
	ANA_REG_OR(ANA_REG_GLB_LDO_SW, BIT_WPA_DCDC_SEL);
	mutex_unlock(&adc_chan_mutex);
	ret = 0;
#endif

exit:
	return ret;
}

static int lpref_get_trimming_step(struct regulator_dev *rdev, int to_vol)
{
	return 1000 * to_vol * 100 / 72;	/*uV */
}
#endif

/* standard dcdc ops*/
#define BITS_DCDC_CAL_RST(_x_)     ( (_x_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_CAL(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )
static int dcdc_get_trimming_step(struct regulator_dev *rdev, int to_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	if (0 == strcmp(desc->desc.name, "vddmem")) {	/* FIXME: vddmem step 200/32mV */
		return 1000 * 200 / 32;	/*uV */
	}
	return 1000 * 100 / 32;	/*uV */
}

static int dcdc_set_trimming(struct regulator_dev *rdev,
			     int def_vol, int to_vol, int adc_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	int acc_vol = desc->ops->get_trimming_step(rdev, to_vol);
/**
 * FIXME: no need division?
	int ctl_vol = DIV_ROUND_UP(def_vol * to_vol * 1000, adc_vol) + acc_vol;
*/
	int ctl_vol = 1000 * (to_vol - (adc_vol - def_vol)) + acc_vol;	/*uV */

	/* FIXME: dcdc core ctrl should be keeped after trimming.
	 * but now, uV_offset is used for dcdc set/get correct voltage API.
	 */
	rdev->constraints->uV_offset = ctl_vol - to_vol * 1000;
	debug("regu (%s) ctl %d to %d, offset %dmv\n", desc->desc.name,
	      ctl_vol / 1000, to_vol, rdev->constraints->uV_offset / 1000);
	return rdev->desc->ops->set_voltage(rdev, ctl_vol, ctl_vol, 0);
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
		int max = regs->vol_ctl_bits >> shft;
		int j = (mv - regs->vol_sel[i]) * 1000 /
		    desc->ops->get_trimming_step(rdev, mv) % 32;

		if (regs->vol_trm == regs->vol_ctl) {	/* new feature */
			ANA_REG_SET(regs->vol_ctl, j | (i << shft),
				    regs->vol_trm_bits | regs->vol_ctl_bits);
		} else {
			if (regs->vol_trm) {	/* small adjust first */
				ANA_REG_SET(regs->vol_trm,
					    BITS_DCDC_CAL(j) |
					    BITS_DCDC_CAL_RST(BITS_DCDC_CAL(-1)
							      - j), -1);
			}

			ANA_REG_SET(regs->vol_ctl, i | (max - i) << 4, -1);
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
#if defined(CONFIG_ARCH_SC7710)
				BUG_ON(0 != __ffs(regs->vol_ctl_bits));
				WARN(!(0 == i
				       && regs->vol_ctl_bits == vol_bits),
				     "the reset relative ctrl bits of %s is invalid, %x",
				     desc->desc.name,
				     ANA_REG_GET(regs->vol_ctl));

				vol_bits = ANA_REG_GET(regs->vol_trm);
				i = vol_bits & regs->vol_trm_bits;
				vol_bits = (~vol_bits
					    & (regs->vol_trm_bits << 5)) >> 5;
				WARN(i != vol_bits
				     && !(0 == i
					  && regs->vol_trm_bits == vol_bits),
				     "the reset relative cal ctrl bits of %s is invalid, %x",
				     desc->desc.name,
				     ANA_REG_GET(regs->vol_trm));

				/* FIXME: correct default value */
				if (0 == strcmp(desc->desc.name, "vddarm"))
					mv = 1200;
#else
				return -EFAULT;
#endif
			}
		}

		cal = (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits)
		    * desc->ops->get_trimming_step(rdev, mv);	/*uV */
	}

	debug2("regu %p (%s) %d +%dmv\n", regs, desc->desc.name, mv,
	       cal / 1000);
	return mv * 1000 + cal;
}

#if defined(CONFIG_ARCH_SC7710)
/* vddmem trimming: -100mv ~ +100 mV, step 6.25mV */
static int vmem_get_voltage(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;
	u32 vol_bits;
	int cal = 0;		/* uV */
	int i, j, shft = __ffs(regs->vol_ctl_bits);

	debug0("regu %p (%s), vol ctl %08x, shft %d, mask %08x, sel %d\n",
	       regs, desc->desc.name, regs->vol_ctl,
	       shft, regs->vol_ctl_bits, regs->vol_sel_cnt);

	BUG_ON(0 != __ffs(regs->vol_trm_bits));
	BUG_ON(regs->vol_sel_cnt > 8);

	i = (ANA_REG_GET(regs->vol_ctl) & regs->vol_ctl_bits) >> shft;
	j = (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits);

	vol_bits =
	    (~ANA_REG_GET(regs->vol_ctl) & (regs->vol_ctl_bits << 4)) >> 4;

	if (i != vol_bits)
		j = 0x10;

	cal = (j - 0x10) * desc->ops->get_trimming_step(rdev, regs->vol_sel[i]);	/*uV */

	debug2("regu %p (%s) %d +%dmv\n", regs, desc->desc.name,
	       regs->vol_sel[i], cal / 1000);
	return regs->vol_sel[i] * 1000 + cal;
}

static int vmem_set_voltage(struct regulator_dev *rdev, int min_uV,
			    int max_uV, unsigned *selector)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int i = 0, j, ctl_vol = min_uV, def_vol, acc_vol;
	int shft = __ffs(regs->vol_ctl_bits);
	int max = regs->vol_ctl_bits >> shft;

	debug0("regu %p (%s) %d %d\n", regs, desc->desc.name, min_uV, max_uV);

	def_vol = regs->vol_sel[i] * 1000;	/*FIXME: fixed vmem@1.8v */
	acc_vol = desc->ops->get_trimming_step(rdev, 0);	/*uV */

	j = (ctl_vol - def_vol + acc_vol * 0x10) / acc_vol;
	if (j >= 0 && j < 32) {
		debug("regu %p (%s) %d = %d %+dmv\n", regs,
		      desc->desc.name, ctl_vol / 1000, def_vol / 1000,
		      (j - 0x10) * acc_vol / 1000);

#if !defined(CONFIG_REGULATOR_CAL_DUMMY)
		ANA_REG_SET(regs->vol_trm,
			    BITS_DCDC_CAL(j) |
			    BITS_DCDC_CAL_RST(BITS_DCDC_CAL(-1) - j), -1);

		ANA_REG_SET(regs->vol_ctl, i | (max - i) << 4, -1);
#endif
		return 0;
	}
	return WARN(-EINVAL,
		    "not found %s closely ctrl bits for %dmV\n",
		    desc->desc.name, ctl_vol / 1000);
}
#endif

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
#if 0
			/* FIXME:
			 * update adc data from kernel parameter,
			 * and compensate 6~12mV if need.
			 */
			adc_data[0][0] -= 6;
			adc_data[1][0] -= 6;
#endif
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

#ifdef CONFIG_ARCH_SCX35
	if (0 == strcmp(desc->desc.name, "vddcamio")) {	/* FIXME: others is 1/2 */
		chan_numerators = 1;
		chan_denominators = 3;
	} else if (0 == strcmp(desc->desc.name, "vddwrf")) {	/* FIXME: bonding options? */
		chan_numerators = 1;
		chan_denominators = 3;
	}
#endif

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

static void do_regu_work(struct work_struct *w)
{
	struct sci_regulator_data *data =
	    container_of(w, struct sci_regulator_data, dwork.work);
	struct sci_regulator_desc *desc = __get_desc(data->rdev);
	debug0("%s\n", desc->desc.name);
	if (!__is_trimming(data->rdev)) {
		mutex_lock(&data->rdev->mutex);
		desc->ops->calibrate(data->rdev, 0, 0);
		mutex_unlock(&data->rdev->mutex);
	}
}

int __regu_calibrate(struct regulator_dev *rdev, int def_vol, int to_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int in_calibration(void);
	if (in_calibration() || !__is_valid_adc_cal()
	    || !regs->vol_def || !regs->cal_ctl || !regs->vol_trm) {
		/* FIXME: BYPASS if in CFT or not adc cal or no cal ctl
		 * or no def vol.
		 */
		return -EACCES;
	}

	schedule_delayed_work(&desc->data.dwork, msecs_to_jiffies(10));
	return 0;
}

/*
 * FIXME: ASSERT dcdc/ldo is enabled
 */
static int regu_calibrate(struct regulator_dev *rdev, int def_vol, int to_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;
	int ret = 0, retry_count = 1;
	int adc_vol = 0, ctl_vol, cal_vol = 0;

retry:
	ctl_vol = rdev->desc->ops->get_voltage(rdev);
	if (IS_ERR_VALUE(ctl_vol)) {
		debug0("no valid %s vol ctrl bits\n", desc->desc.name);
	} else			/* dcdc/ldo maybe had been adjusted or opened in uboot-spl */
		ctl_vol /= 1000;

	if (!def_vol)
		def_vol = (IS_ERR_VALUE(ctl_vol)) ? regs->vol_def : ctl_vol;

	if (!to_vol) {
		to_vol = (IS_ERR_VALUE(ctl_vol)) ? regs->vol_def : ctl_vol;

		/* FIXME: Ideal voltage maybe not chip default which in the choice */
		if (to_vol != regs->vol_def) {
			int i = __match_dcdc_vol(regs, regs->vol_def);
			if (i >= 0 && regs->vol_sel[i] != regs->vol_def)
				to_vol = regs->vol_def;
		}
	}

	adc_vol = regu_adc_voltage(rdev);
	if (adc_vol <= 0) {
		debug("%s default %dmv, maybe not enable\n",
		      desc->desc.name, def_vol);
		goto exit;
	}

	cal_vol = abs(adc_vol - to_vol);
	debug("%s default %dmv, from %dmv to %dmv, bias %c%d.%03d%%\n",
	      desc->desc.name, def_vol, adc_vol, to_vol,
	      (adc_vol > to_vol) ? '+' : '-',
	      cal_vol * 100 / adc_vol, cal_vol * 100 * 1000 / adc_vol % 1000);

	if (!def_vol || !to_vol || adc_vol <= 0)
		goto exit;

	if (abs(adc_vol - def_vol) >= def_vol / 9)	/* adjust limit 10% */
		goto exit;
	else if (cal_vol < to_vol / 100) {	/* bias 1% */
		/**
		 * FIXME: cal_vol * 100 / adc_vol <= 1 is okay for ldo lpref
		 */
		set_bit(desc->desc.id, trimming_state);
		debug("%s is okay\n", desc->desc.name);
		return 0;
	} else if (0 == retry_count--) {
		/* FIXME: unfortunately, dcdc/ldo need calibrate again */
		WARN(1, "%s try again\n", desc->desc.name);
		return def_vol;
	}

	ret = desc->ops->set_trimming(rdev, def_vol, to_vol, adc_vol);
	if (IS_ERR_VALUE(ret))
		goto exit;

	def_vol = 0;		/*force reacquire */
	set_bit(desc->desc.id, trimming_state);	/*force set before verify */
	msleep(REGU_VERIFY_DLY);	/* wait a moment before cal verify */
	goto retry;

exit:
	debug("%s failure\n", desc->desc.name);
	return -1;
}

static int regu_force_trimming(struct regulator_dev *rdev, int trim)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	const struct sci_regulator_regs *regs = desc->regs;

	if (regs->vol_trm)
		ANA_REG_SET(regs->vol_trm,
			    trim << __ffs(regs->vol_trm_bits),
			    regs->vol_trm_bits);
	return 0;
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

/**
 * regulator_calibrate - force calibrate the regulator to the ideal value
 * @regulator: regulator source
 *
 */
int regulator_calibrate(struct regulator *regulator, int to_vol)
{
	struct regulator_dev *rdev = regulator_get_drvdata(regulator);
	int ret = -1;

	if (rdev) {
		struct sci_regulator_desc *desc = __get_desc(rdev);
		if (desc && desc->ops) {
			mutex_lock(&rdev->mutex);
			ret = desc->ops->calibrate(rdev, 0, to_vol);
			mutex_unlock(&rdev->mutex);
		}
	}

	return ret;
}

EXPORT_SYMBOL_GPL(regulator_calibrate);

static struct regulator_ops ldo_ops = {
	.enable = ldo_turn_on,
	.disable = ldo_turn_off,
	.is_enabled = ldo_is_on,
	.set_voltage = ldo_set_voltage,
	.get_voltage = ldo_get_voltage,
	.set_mode = ldo_set_mode,
/*	.enable_time = ldo_enable_time, */
};

static struct regulator_ops usbd_ops = {
	.enable = 0,		/* reserved d-die ldo */
	.disable = 0,		/* reserved d-die ldo */
	.is_enabled = 0,	/* reserved d-die ldo */
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

static struct sci_regulator_ops sci_ldo_ops = {
	.trimming_def_val = 0x10,	/* 100% */
	.get_trimming_step = ldo_get_trimming_step,
	.set_trimming = ldo_set_trimming,
	.calibrate = regu_calibrate,
};

static struct sci_regulator_ops sci_dcdcldo_ops = {
	.trimming_def_val = 0x10,	/* 100% */
	.get_trimming_step = dcdcldo_get_trimming_step,
	.set_trimming = dcdcldo_set_trimming,
	.calibrate = regu_calibrate,
};

static struct sci_regulator_ops sci_dcdc_ops = {
	.get_trimming_step = dcdc_get_trimming_step,
	.set_trimming = dcdc_set_trimming,
	.calibrate = regu_calibrate,
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

static int debugfs_ldo_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->set_voltage) {
		if (val < 200)	/* FIXME: debug force trimming */
			regu_force_trimming(rdev, val);
		else
			rdev->desc->ops->set_voltage(rdev, val * 1000,
						     val * 1000, 0);
	}
	return 0;
}

static int debugfs_dcdc_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;
	struct sci_regulator_desc *desc;

	if (rdev) {
		desc = __get_desc(rdev);
		if (val < 200)	/* FIXME: debug force trimming */
			regu_force_trimming(rdev, val);
		else if (desc && desc->ops) {
			mutex_lock(&rdev->mutex);
			desc->ops->calibrate(rdev, 0, val);
			mutex_unlock(&rdev->mutex);
		}
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
			debugfs_voltage_get, debugfs_ldo_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_dcdc,
			debugfs_voltage_get, debugfs_dcdc_set, "%llu\n");
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
				    desc->debugfs, rdev,
				    (0 ==
				     desc->regs->typ) ? &fops_ldo : &fops_dcdc);
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

void * sci_regulator_register(struct platform_device *pdev,
				       struct sci_regulator_desc *desc)
{
	static atomic_t idx = ATOMIC_INIT(1);	/* 0: dummy */
	struct regulator_dev *rdev;
	struct regulator_config config = {};
	struct regulator_ops *__regs_ops[] = {
		&ldo_ops, &usbd_ops, &dcdc_ops, 0 /*lpref_ops */ , &boost_ops,
		0,
	};
	struct sci_regulator_ops *__sci_regs_ops[] = {
		&sci_ldo_ops, 0, &sci_dcdc_ops, 0, 0,
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
#else
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

	BUG_ON(desc->regs->pd_set
	       && desc->regs->pd_set == desc->regs->pd_rst
	       && desc->regs->pd_set_bit == desc->regs->pd_rst_bit);

	if (!desc->ops)
		desc->ops = __sci_regs_ops[desc->regs->typ];

	BUG_ON(desc->regs->typ >= ARRAY_SIZE(__regs_ops));
	if (!desc->desc.ops)
		desc->desc.ops = __regs_ops[desc->regs->typ];

#ifdef CONFIG_ARCH_SCX35
	if (desc->regs->typ == VDD_TYP_BOOST) {	/*FIXME: reconfig current sink */
#ifndef CONFIG_OF
		init_data.constraints.min_uA = 0;
		init_data.constraints.max_uA = MAX_CURRENT_SINK * 1000;
		init_data.constraints.valid_ops_mask |=
		    REGULATOR_CHANGE_CURRENT;
#else
		init_data->constraints.min_uA = 0;
		init_data->constraints.max_uA = MAX_CURRENT_SINK * 1000;
		init_data->constraints.valid_ops_mask |=
		    REGULATOR_CHANGE_CURRENT;
#endif
		desc->desc.type = REGULATOR_CURRENT;
	}

	if (desc->regs->typ == VDD_TYP_LDO) {	/*FIXME: reconfig dcdcldo ops */
		if ((desc->regs->cal_ctl_bits & 0xFFFF0000) ==
		    (BIT(17) | BIT(18) | BIT(20))) {
			desc->ops = &sci_dcdcldo_ops;
		}
	}
#endif

/* FIXME: patch for sc7710 BA version */
#ifdef CONFIG_ARCH_SC7710
	if (sci_get_ana_chip_id() == ANA_CHIP_ID_BA &&
	    desc->regs->typ == VDD_TYP_LPREF) {
		static struct sci_regulator_ops sci_lpref_ops = {
			.get_trimming_step = lpref_get_trimming_step,
			.set_trimming = lpref_set_trimming,
			.calibrate = regu_calibrate,
		};

		BUG_ON(VDD_TYP_LPREF != 3);
		sci_lpref_ops.trimming_def_val = sci_ldo_ops.trimming_def_val;
		desc->ops = &sci_lpref_ops;
		desc->desc.ops = &ldo_ops;
	}

	if (0 == strcmp(desc->desc.name, "vddmem")) {
		static struct sci_regulator_ops sci_vmem_ops = {
			.get_trimming_step = dcdc_get_trimming_step,
			.set_trimming = dcdc_set_trimming,
			.calibrate = regu_calibrate,
		};
		static struct regulator_ops vmem_ops = {
			.enable = ldo_turn_on,
			.disable = ldo_turn_off,
			.is_enabled = ldo_is_on,
			.set_voltage = vmem_set_voltage,
			.get_voltage = vmem_get_voltage,
		};

		if (sci_get_ana_chip_id() == ANA_CHIP_ID_BA) {
			desc->ops = &sci_vmem_ops;
			desc->desc.ops = &vmem_ops;
		} else
			desc->desc.ops = 0;	/*FIXME: reserved for board v1.1.0 */
	}
#endif

#ifndef CONFIG_OF
	init_data.consumer_supplies =
	    set_supply_map(&pdev->dev, desc->desc.name,
			   &init_data.num_consumer_supplies);

	if (!init_data.consumer_supplies)
		init_data.consumer_supplies = consumer_supplies_default;

	debug0("regu %p (%s)\n", desc->regs, desc->desc.name);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
	config.dev = &pdev->dev;
	config.init_data = &init_data;
	config.driver_data = 0;
	rdev = regulator_register(&desc->desc, &config);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0))
	rdev = regulator_register(&desc->desc, &pdev->dev, &init_data, 0, 0);
#else
	rdev = regulator_register(&desc->desc, &pdev->dev, &init_data, 0);
#endif
	if (init_data.consumer_supplies != consumer_supplies_default)
		kfree(init_data.consumer_supplies);
#else /* CONFIG_OF */
	init_data->consumer_supplies =
	    set_supply_map(&pdev->dev, desc->desc.name,
			   &init_data->num_consumer_supplies);

	if (!init_data->consumer_supplies)
		init_data->consumer_supplies = consumer_supplies_default;

	debug0("regu %p (%s)\n", desc->regs, desc->desc.name);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
	config.dev = &pdev->dev;
	config.init_data = init_data;
	config.of_node = node_np;
	config.driver_data = 0;
	rdev = regulator_register(&desc->desc, &config);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0))
	rdev = regulator_register(&desc->desc, &pdev->dev, init_data, 0, 0);
#else
	rdev = regulator_register(&desc->desc, &pdev->dev, init_data, 0);
#endif
	if (init_data->consumer_supplies != consumer_supplies_default)
		kfree(init_data->consumer_supplies);
#endif /* end of CONFIG_OF */

	if (!IS_ERR(rdev)) {
		rdev->reg_data = rdev;
		INIT_DELAYED_WORK(&desc->data.dwork, do_regu_work);
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

		debugfs_create_u32("ana_addr", S_IRUGO | S_IWUSR,
				   debugfs_root, (u32 *) & ana_addr);
		debugfs_create_file("ana_valu", S_IRUGO | S_IWUSR,
					debugfs_root, &ana_addr, &fops_ana_addr);
		debugfs_create_file("adc_chan", S_IRUGO | S_IWUSR,
					debugfs_root, &adc_chan, &fops_adc_chan);
		debugfs_create_u64("adc_data", S_IRUGO | S_IWUSR,
				   debugfs_root, (u64 *) & adc_data);

		{/* vddarm/vddcore/vddmem common debugfs interface */
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

	pr_info("sc271x ana chip id: (0x%08x), bond opt (0x%08x)\n",
		(sci_get_ana_chip_id() | sci_get_ana_chip_ver()),
		ANA_REG_GET(ANA_REG_GLB_ANA_STATUS));

#include CONFIG_REGULATOR_SPRD_MAP
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id sprd_regulator_of_match[] = {
	{ .compatible = "sprd,sc2713-regulator", },
	{ }
};
#endif

static struct platform_driver sci_regulator_driver = {
	.driver = {
		   .name = "sc2713-regulator",
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
		.name = "sc2713-regulator",
		.id = -1,
	};
	static struct platform_device sc2713s_regulator_device = {
		.name = "sc2713s-regulator",
		.id = -1,
	};

	if (sci_get_ana_chip_id() == 0x2713C000) /* SC2713S */
		return platform_device_register(&sc2713s_regulator_device);
	else
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
MODULE_VERSION("0.4");
