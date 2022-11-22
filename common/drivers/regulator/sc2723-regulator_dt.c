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
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/regulator/of_regulator.h>
#endif
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
	int typ; /* BIT4: default on/off(0: off, 1: on); BIT0~BIT3: dcdc/ldo type(0: ldo; 2: dcdc) */
	unsigned long pd_set;
	u32 pd_set_bit;
	unsigned long pwr_sel; /* otp pwr select reg */
	u32 pwr_sel_bit; /* 0: otp enable(from emmc), 1: otp disable(from sw register) */
	unsigned long vol_trm;
	u32 vol_trm_bits;
	unsigned long cal_ctl;
	u32 cal_ctl_bits, cal_chan;
	u32 min_uV, step_uV;
	u32 vol_def;
	unsigned long vol_ctl;
	u32 vol_ctl_bits;
	u32 vol_sel_cnt;
	u32 *vol_sel;
};

struct sci_regulator_data {
	struct regulator_dev *rdev;
};

struct sci_regulator_desc {
	struct regulator_desc desc;
	struct regulator_init_data *init_data;
	struct sci_regulator_regs regs;
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

static u32 dcdc_vol_select[] = {1100000, 700000, 800000, 900000, 1000000, 650000, 1200000, 1300000}; /* uV */

static struct sci_regulator_desc *sci_desc_list = NULL;

static atomic_t idx = ATOMIC_INIT(1);	/* 0: dummy */

static u32 ana_chip_id;
static u16 ana_mixed_ctl, otp_pwr_sel;
static int regu_cnt;
static DEFINE_MUTEX(adc_chan_mutex);
static int regulator_get_trimming_step(struct regulator_dev *rdev, int to_vol);
static int __is_valid_adc_cal(void);
extern int sci_efuse_get_cal(unsigned int * pdata, int num);


static struct sci_regulator_desc *__get_desc(struct regulator_dev *rdev)
{
	return (struct sci_regulator_desc *)rdev->desc;
}

/* standard ldo ops*/
static int ldo_turn_on(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;

	debug0("regu 0x%p (%s), power down 0x%08x[%d]\n", regs,
	       desc->desc.name, regs->pd_set, __ffs(regs->pd_set_bit));

	if (regs->pd_set == ANA_REG_GLB_LDO_DCDC_PD)
		sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,
				  BITS_PWR_WR_PROT_VALUE(0x6e7f));

	if (regs->pd_set)
		ANA_REG_BIC(regs->pd_set, regs->pd_set_bit);

	if (regs->pd_set == ANA_REG_GLB_LDO_DCDC_PD)
		sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE, 0);

	debug("regu 0x%p (%s), turn on\n", regs, desc->desc.name);
	return 0;
}

static int ldo_turn_off(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;

	debug0("regu 0x%p (%s), power down 0x%08x[%d]\n", regs,
	       desc->desc.name, regs->pd_set, __ffs(regs->pd_set_bit));

#if !defined(CONFIG_REGULATOR_CAL_DUMMY)
	if (regs->pd_set == ANA_REG_GLB_LDO_DCDC_PD)
		sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,
				  BITS_PWR_WR_PROT_VALUE(0x6e7f));

	if (regs->pd_set)
		ANA_REG_OR(regs->pd_set, regs->pd_set_bit);

	if (regs->pd_set == ANA_REG_GLB_LDO_DCDC_PD)
		sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE, 0);

#endif

	debug("regu 0x%p (%s), turn off\n", regs, desc->desc.name);
	return 0;
}

static int ldo_is_on(struct regulator_dev *rdev)
{
	int ret = -EINVAL;
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;

	debug0("regu 0x%p (%s), power down 0x%08x[%d]\n", regs,
	       desc->desc.name, regs->pd_set, __ffs(regs->pd_set_bit));

	if (regs->pd_set) {
		ret = !(ANA_REG_GET(regs->pd_set) & regs->pd_set_bit);
	}

	debug2("regu 0x%p (%s) turn on, return %d\n", regs, desc->desc.name, ret);
	return ret;
}

static int ldo_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
#if 0
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;

	debug("regu 0x%p (%s), slp 0x%08x[%d] mode %x\n", regs, desc->desc.name,
	      regs->slp_ctl, regs->slp_ctl_bit, mode);

	if (!regs->slp_ctl)
		return -EINVAL;

	if (mode == REGULATOR_MODE_STANDBY) {	/* disable auto slp */
		ANA_REG_BIC(regs->slp_ctl, regs->slp_ctl_bit);
	} else {
		ANA_REG_OR(regs->slp_ctl, regs->slp_ctl_bit);
	}
#endif

	return 0;
}

static int ldo_set_voltage(struct regulator_dev *rdev, int min_uV,
			   int max_uV, unsigned *selector)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;
	//int mv = min_uV / 1000;
	int ret = -EINVAL;

	debug("regu 0x%p (%s) set voltage, %d(uV) %d(uV)\n", regs, desc->desc.name, min_uV, max_uV);

	if (regs->vol_trm) {
		int shft = __ffs(regs->vol_trm_bits);
		u32 trim = DIV_ROUND_UP((int)(min_uV - regs->min_uV), regs->step_uV);

		ret = trim > (regs->vol_trm_bits >> shft);
		WARN(0 != ret,
		     "warning: regulator (%s) not support %d(uV)\n",
		     desc->desc.name, min_uV);

		if (0 == ret) {
			ANA_REG_SET(regs->vol_trm,
				    trim << shft,
				    regs->vol_trm_bits);
		}
	}

	return ret;
}

static int ldo_get_voltage(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;
	u32 vol;

	debug0("regu 0x%p (%s), vol trm 0x%08x, mask 0x%08x\n",
	       regs, desc->desc.name, regs->vol_trm, regs->vol_trm_bits);

	if (regs->vol_trm) {
		int shft = __ffs(regs->vol_trm_bits);
		u32 trim =
		    (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits) >> shft;
		vol = regs->min_uV + trim * regs->step_uV;
		debug2("regu 0x%p (%s), voltage %d\n", regs, desc->desc.name, vol);
		return vol;
	}

	return -EFAULT;
}

extern u32 __adie_efuse_read(int blk_index);
extern int sci_otp_get_offset(const char *name);

static int set_regu_offset(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;
	const char *regu_name = desc->desc.name;
	int efuse_data = 0;

	if (NULL == regu_name)
		return -1;

	efuse_data = sci_otp_get_offset(regu_name);
	if (!(desc->regs.typ & BIT(4)))
		rdev->constraints->uV_offset = efuse_data * regulator_get_trimming_step(rdev, 0);
	else
		rdev->constraints->uV_offset = 0;

	debug("%s otp delta: %d, voltage offset: %d(uV)\n", desc->desc.name, efuse_data, rdev->constraints->uV_offset);

	/* switch sw register control from otp emmc only for vddmem/vdd25 */
	if ((0 == strcmp(regu_name, "vddmem"))
		 || (0 == strcmp(regu_name, "vdd25"))) {
		if (regs->pwr_sel) {
			int shft = __ffs(regs->vol_trm_bits);
			u32 trim = 0;

			if (regs->vol_trm) {
				trim = (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits) >> shft;
				trim += efuse_data;
				ANA_REG_SET(regs->vol_trm,
					    trim << shft,
					    regs->vol_trm_bits);
			}

			/* set pwr sel bit for sw control */
			ANA_REG_OR(regs->pwr_sel, regs->pwr_sel_bit);
		}
	}

	return 0;
}


/* FIXME: get dcdc cal offset config from uboot */
typedef struct {
	u16 ideal_vol;
	const char name[14];
}vol_para_t;

static void __iomem * spl_start_base = NULL;

#if defined(CONFIG_ARCH_SCX35L)
#define SPRD_SPL_PHYS	( 0x50003000 )
#else
#define SPRD_SPL_PHYS	( 0x50005000 )
#endif
#define SPRD_SPL_SIZE	( SZ_32K )
#define PP_VOL_PARA		( SPRD_SPL_PHYS + 0xC20 ) /* assert in iram2 */
#define TO_IRAM2(_p_)	( (unsigned long)spl_start_base + (unsigned long)(_p_) - SPRD_SPL_PHYS )
#define IN_IRAM2(_p_)	( (unsigned long)(_p_) >= SPRD_SPL_PHYS && (unsigned long)(_p_) < SPRD_SPL_PHYS + SPRD_SPL_SIZE )

int regulator_default_get(const char con_id[])
{
	int i = 0, res = 0;
	vol_para_t *pvol_para = (vol_para_t *)__raw_readl((void *)TO_IRAM2(PP_VOL_PARA));
	debug0("pvol_para phy_addr 0x%08x\n", pvol_para);

	if (!(IN_IRAM2(pvol_para)))
		return 0;

	pvol_para = (vol_para_t *)TO_IRAM2(pvol_para);
	debug0("pvol_para vir_addr 0x%08x\n", pvol_para);
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
	struct sci_regulator_regs *regs = &desc->regs;
	int ctl_vol, to_vol;
	uint otp_ana_flag = 0;

	if (!regs->vol_trm)
		return -1;

	if(!__is_valid_adc_cal())
		return -2;

	otp_ana_flag = (u8)__adie_efuse_read(0);
	debug("emmeory block(0) data %#x\n", otp_ana_flag);;
	if(!(otp_ana_flag & BIT(7))) {
		set_regu_offset(rdev);
	} else {

		to_vol = regulator_default_get(desc->desc.name);
		to_vol *= 1000; //uV
		if (!to_vol)
			to_vol = regs->vol_def;

		if (to_vol && rdev->desc->ops->get_voltage) {
			ctl_vol = rdev->desc->ops->get_voltage(rdev);
			rdev->constraints->uV_offset = ctl_vol - to_vol;//uV
			debug("regu 0x%p (%s), uV offset %d\n", regs, desc->desc.name, rdev->constraints->uV_offset);
		}
	}

	return 0;
}


static int regulator_get_trimming_step(struct regulator_dev *rdev, int to_vol)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;

	BUG_ON(!regs->step_uV);

	return regs->step_uV;
}

static int __match_dcdc_vol(struct sci_regulator_regs *regs, u32 vol)
{
	int i, j = -1;
	int ds, min_ds = 100 * 1000;	/* uV, the max range of small voltage */

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
	int vol = rdev->desc->ops->get_voltage(rdev);
	if (vol > old_vol) {
		/* FIXME: for dcdc, each step (50mV) takes 10us */
		int dly = (vol - old_vol) * 10 / (50 * 1000);
		WARN_ON(dly > 1000);
		udelay(dly);
	}
	return 0;
}

static int dcdc_set_voltage(struct regulator_dev *rdev, int min_uV,
			    int max_uV, unsigned *selector)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;
	int i = 0;
	//int mV = min_uV / 1000;
	int old_vol = rdev->desc->ops->get_voltage(rdev);

	debug0("regu 0x%p (%s) %d %d\n", regs, desc->desc.name, min_uV, max_uV);

	if (regs->vol_ctl) {
		/* found the closely vol ctrl bits */
		i = __match_dcdc_vol(regs, min_uV);
		if (i < 0)
			return WARN(-EINVAL, "not found %s closely ctrl bits for %d(uV)\n",
				    desc->desc.name, min_uV);
	}

#if !defined(CONFIG_REGULATOR_CAL_DUMMY)
	/* dcdc calibration control bits (default 00000),
	 * small adjust voltage: 100/32mv ~= 3.125mv
	 */
	{
		int shft_trm = __ffs(regs->vol_trm_bits);
		int shft_ctl = 0;
		int step = 0;
		int j = 0;

		if (regs->vol_ctl) {
			shft_ctl = __ffs(regs->vol_ctl_bits);
			step = regulator_get_trimming_step(rdev, 0);

			j = DIV_ROUND_UP((int)(min_uV - (int)regs->vol_sel[i]), step);

			debug("regu 0x%p (%s) %d = %d %+duV(trim %#x)\n", regs, desc->desc.name,
				   min_uV, regs->vol_sel[i], min_uV - regs->vol_sel[i], j);
		} else {
			j = DIV_ROUND_UP((int)(min_uV - regs->min_uV), regs->step_uV);

			debug("regu 0x%p (%s) %d = %d %+duV(trim %#x)\n", regs, desc->desc.name,
				   min_uV, regs->min_uV, min_uV - regs->min_uV, j);
		}

		BUG_ON(j > (regs->vol_trm_bits >> shft_trm));

		if (regs->vol_trm == regs->vol_ctl) {
			ANA_REG_SET(regs->vol_ctl, (j << shft_trm) | (i << shft_ctl),
				    regs->vol_trm_bits | regs->vol_ctl_bits);
		} else {
			if (regs->vol_trm) {	/* small adjust first */
				ANA_REG_SET(regs->vol_trm, j << shft_trm,
					regs->vol_trm_bits);
			}

			if (regs->vol_ctl) {
				ANA_REG_SET(regs->vol_ctl, i << shft_ctl,
					regs->vol_ctl_bits);
			}
		}
	}

	__dcdc_enable_time(rdev, old_vol);
#endif

	return 0;
}

static int dcdc_set_voltage_step(struct regulator_dev *rdev, int min_uV,
			int max_uV, unsigned *selector)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	int to_vol = min_uV;
	int step = 25 * 1000;/*uV*/
	int vol = rdev->desc->ops->get_voltage(rdev);

	if (vol < to_vol) {
		do {		/*FIXME: dcdc sw step up for eliminate overshoot (+65mV) */
			vol += step;
			if (vol > to_vol)
				vol = to_vol;
			dcdc_set_voltage(rdev, vol, vol, selector);
		}while (vol < to_vol);
	} else {
		do {
			vol -= step;
			if(vol < to_vol)
				vol = to_vol;
			dcdc_set_voltage(rdev, vol, vol, selector);
		}while (vol > to_vol);
	}
	return 0;
}

static int dcdc_get_voltage(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;
	u32 uV = 0;
	int i, cal = 0 /* uV */;

	if (regs->vol_ctl) {
		int shft_ctl = __ffs(regs->vol_ctl_bits);
		int shft_trm = __ffs(regs->vol_trm_bits);

		debug0("regu 0x%p (%s), vol ctl 0x%08x, shft %d, mask 0x%08x, sel %d\n",
			   regs, desc->desc.name, regs->vol_ctl,
			   shft_ctl, regs->vol_ctl_bits, regs->vol_sel_cnt);

		i = (ANA_REG_GET(regs->vol_ctl) & regs->vol_ctl_bits) >> shft_ctl;
		uV = regs->vol_sel[i];

		if (regs->vol_trm) {
			cal = (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits) >> shft_trm;
			cal *= regulator_get_trimming_step(rdev, 0);	/* uV */
		}

	} else if (regs->vol_trm) {
		int shft_trm = __ffs(regs->vol_trm_bits);
		u32 trim =
		    (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits) >> shft_trm;
		uV = regs->min_uV + trim * regs->step_uV;
	}

	debug2("%s get voltage, %d +%duv\n", desc->desc.name, uV, cal);

	return (uV + cal) /*uV */;
}


/* standard boost ops*/
#define MAX_CURRENT_SINK	(500)	/*FIXME: max current sink */
static int boost_set_current_limit(struct regulator_dev *rdev, int min_uA,
				   int max_uA)
{
	struct sci_regulator_desc *desc = __get_desc(rdev);
	struct sci_regulator_regs *regs = &desc->regs;
	int ma = min_uA / 1000;
	int ret = -EACCES;
	int i, shft = __ffs(regs->vol_ctl_bits);
	int trim = (int)regs->vol_def / (1000 * 1000);
	int steps = (regs->vol_ctl_bits >> shft) + 1;

	debug("regu 0x%p (%s) %d %d\n", regs, desc->desc.name, min_uA, max_uA);

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
	struct sci_regulator_regs *regs = &desc->regs;
	u32 cur;
	int i, shft = __ffs(regs->vol_ctl_bits);
	int steps = (regs->vol_ctl_bits >> shft) + 1;

	debug0("regu 0x%p (%s), vol ctl 0x%08x, shft %d, mask 0x%08x\n",
	       regs, desc->desc.name, regs->vol_ctl, shft, regs->vol_ctl_bits);

	if (!regs->vol_ctl)
		return -EACCES;

	i = ((ANA_REG_GET(regs->vol_ctl) & regs->vol_ctl_bits) >> shft);
	cur = i * MAX_CURRENT_SINK / steps;
	debug("regu 0x%p (%s) get current %d\n", regs, desc->desc.name, cur);
	return cur * 1000;
}

static int adc_sample_bit = 1;	/*12bits mode */
static short adc_data[3][2]
#if defined(CONFIG_REGULATOR_ADC_DEBUG)
    = {
	{4200, 3387},  /* same as nv adc_t */
	{3600, 2905},
	{400, 316},  /* 0.4@VBAT, Reserved IdealC Value */
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
	    (0 == sci_efuse_get_cal((u32 *) adc_data, 2))) {
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
	struct sci_regulator_regs *regs = &desc->regs;

	int ret, adc_chan = regs->cal_chan;
	u16 ldo_cal_sel = regs->cal_ctl_bits & 0xFFFF;
	u32 adc_res, adc_val[MEASURE_TIMES];
	u32 ratio = 1, chan_numerators = 1, chan_denominators = 1;
	u32 bat_numerators, bat_denominators;

	struct adc_sample_data data = {
		.channel_id = adc_chan,
		.channel_type = 0,	/*sw */
		.hw_channel_delay = 0,	/*reserved */
		.scale = (((adc_chan != 13) && (adc_chan != 14)) ? 1 : 0),	/* chanel = 13/14: small scale, others: big scale */
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
	if (ldo_cal_sel) {
		mutex_lock(&adc_chan_mutex);
		ANA_REG_OR(regs->cal_ctl, ldo_cal_sel);
		debug0("%s adc channel %d : %04x\n",
		       desc->desc.name, data.channel_id, ldo_cal_sel);
	}

	ret = sci_adc_get_values(&data);
	BUG_ON(0 != ret);

	/* close ldo cal and release multiplexed aux adc channel */
	if (ldo_cal_sel) {
		ANA_REG_BIC(regs->cal_ctl, ldo_cal_sel);
		mutex_unlock(&adc_chan_mutex);
	}

	__dump_adc_result(adc_val);
	sort(adc_val, MEASURE_TIMES, sizeof(u32), cmp_val, 0);
	/*__dump_adc_result(adc_val);*/

	ratio = (u32)sci_adc_get_ratio(data.channel_id, data.scale, ldo_cal_sel);
	chan_numerators = ratio >> 16;
	chan_denominators = ratio & 0xFFFF;

	ratio = (u32)sci_adc_get_ratio(ADC_CHANNEL_VBAT, 1, 0);
	bat_numerators = ratio >> 16;
	bat_denominators = ratio & 0xFFFF;

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
};

static struct regulator_ops dcdc_ops = {
	.enable = ldo_turn_on,
	.disable = ldo_turn_off,
	.is_enabled = ldo_is_on,
	.set_voltage = dcdc_set_voltage_step,
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

void print_regulator_state(struct seq_file *s)
{
	int i = 0, state = 0;
	struct sci_regulator_desc *sci_desc = NULL;
	sci_desc = sci_desc_list;
	for(i=0; i<regu_cnt; i++) {
		state = ldo_is_on(sci_desc->data.rdev);
		seq_printf(s, "|%5s\t%15s\t %25d\t|\n", sci_desc->desc.name, \
			state?"on":"off", state?regu_adc_voltage(sci_desc->data.rdev):0);
		sci_desc++;
	}

}
EXPORT_SYMBOL_GPL(print_regulator_state);

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
		ANA_REG_SET(ana_addr + (ANA_REGS_GLB_BASE & PAGE_MASK), val, -1);
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
	if (IS_ERR_OR_NULL(rdev->debugfs)) {
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


static inline int __strcmp(const char *cs, const char *ct)
{
	if (!cs || !ct)
		return -1;

	return strcmp(cs, ct);
}

static struct of_device_id sprd_regulator_of_match[] = {
	{ .compatible = "sprd,sc2723-regulator", },
	{ }
};


#undef REGS_ANA_GLB_BASE
#ifndef REGS_ANA_GLB_BASE
#define REGS_ANA_GLB_PHYS			(SPRD_ADI_PHYS + 0x8800)
#define REGS_ANA_GLB_BASE			(SPRD_ADI_BASE + 0x8800)
#define REGS_ANA_GLB_SIZE			(SZ_2K) //0x40038800 ~ 0x40039000
#endif

#define IS_VALID_ANA_ADDR(pa, pa_base, size)	( (pa) >= (pa_base) && (pa) < (pa_base) + (size) )
#define PHY_TO_VIR(pa, pa_base, va_base) 		( (pa) - (pa_base) + (va_base) )

static unsigned long phy2vir(const u32 reg_phy)
{
	unsigned long reg_vir = 0;

	if(!reg_phy)
		return 0;

	if (IS_VALID_ANA_ADDR(reg_phy, REGS_ANA_GLB_PHYS, REGS_ANA_GLB_SIZE)) {
		reg_vir = (unsigned long)(PHY_TO_VIR(reg_phy, REGS_ANA_GLB_PHYS, REGS_ANA_GLB_BASE));
	} else {
		WARN(1, "reg(0x%08x) physical address is invalid!\n", reg_phy);
	}

	return reg_vir;
}

static int reconfig_regulator(struct sci_regulator_desc *desc)
{
	struct sci_regulator_regs *regs = &desc->regs;

	/* Fixme: Config DCDC  linear/no linear control
	 *   accoring to BIT14 of Reg(0x40038800 + 0x0118)
	 */
	if (ana_mixed_ctl & BIT_DCDC_V_CTRL_MODE) {
		/* dcdc linear control */
		if ((0 == strcmp(desc->desc.name, "vddcore"))
			 || (0 == strcmp(desc->desc.name, "vddarm"))) {
			regs->vol_ctl = 0;
			regs->vol_ctl_bits = 0;
		}
	} else {
		/* dcdc Non-linear control */
		if ((0 == strcmp(desc->desc.name, "vddcore"))
			 || (0 == strcmp(desc->desc.name, "vddarm"))) {
			regs->vol_ctl = regs->vol_trm;
			regs->vol_trm_bits = (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4));
			regs->vol_ctl_bits = (BIT(5)|BIT(6)|BIT(7));
			regs->vol_sel_cnt = ARRAY_SIZE(dcdc_vol_select);
			regs->vol_sel = dcdc_vol_select;
		}
	}

	return 0;
}

static int of_regu_read_reg(struct device_node *np, int idx,
		struct sci_regulator_regs *regs)
{
	const __be32 *cell;
	u32 reg_phy;
	int count;

	if(!np || !regs)
		return -EINVAL;

	cell = of_get_property(np, "reg", &count);
	count /= 4;
	if(cell)
		WARN_ON(idx >= count);

	cell = of_get_address(np, idx, NULL, NULL);

	switch(idx) {
	case 0: /* ldo/dcdc power down */
		if(cell) {
			reg_phy = (u32)be32_to_cpu(*(cell++));
			regs->pd_set = phy2vir(reg_phy);
			regs->pd_set_bit = (u32)be32_to_cpu(*(cell++));
			debug0("reg(%s) pd_set phy addr: 0x%08x, vir addr: 0x%08x, msk: 0x%08x\n",
				np->name, reg_phy, regs->pd_set, regs->pd_set_bit);
		}
		break;
	case 1: /* ldo/dcdc voltage trim */
		if(cell) {
			reg_phy = (u32)be32_to_cpu(*(cell++));
			regs->vol_trm = phy2vir(reg_phy);
			regs->vol_trm_bits= (u32)be32_to_cpu(*(cell++));
			debug0("reg(%s) vol_trm phy addr: 0x%08x, vir addr: 0x%08x, msk: 0x%08x\n",
				np->name, reg_phy, regs->vol_trm, regs->vol_trm_bits);
		}
		break;
	case 2: /* otp pwr select */
		if(cell) {
			reg_phy = (u32)be32_to_cpu(*(cell++));
			regs->pwr_sel = phy2vir(reg_phy);
			regs->pwr_sel_bit= (u32)be32_to_cpu(*(cell++));
			debug0("reg(%s) otp_pwr_sel phy addr: 0x%08x, vir addr: 0x%08x, msk: 0x%08x\n",
				np->name, reg_phy, regs->pwr_sel, regs->pwr_sel_bit);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int sci_regulator_parse_dt(struct platform_device *pdev,
			struct device_node *np, struct sci_regulator_desc *desc,
			struct regulator_consumer_supply *supply, int sz)
{
	struct sci_regulator_regs *regs = &desc->regs;
	const __be32 *tmp;
	u32 data[8] = {0};
	u32 tmp_val_u32;
	int type = 0, cnt = 0, ret = 0;

	if (!pdev || !np || !desc || !supply) {
		return -EINVAL;
	}

	desc->desc.name = np->name;
	desc->desc.id = (atomic_inc_return(&idx) - 1);
	desc->desc.type = REGULATOR_VOLTAGE;
	desc->desc.owner = THIS_MODULE;

	supply[0].dev_name = NULL;
	supply[0].supply = np->name;
	desc->init_data = of_get_regulator_init_data(&pdev->dev, np);
	if (!desc->init_data || 0 != __strcmp(desc->init_data->constraints.name, np->name)) {
		dev_err(&pdev->dev, "failed to parse regulator(%s) init data! \n", np->name);
		return -EINVAL;
	}

	desc->init_data->supply_regulator = 0;
	desc->init_data->constraints.valid_modes_mask =
		REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY;
	desc->init_data->constraints.valid_ops_mask =
		REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE;
	desc->init_data->num_consumer_supplies = sz;

	desc->init_data->consumer_supplies =
		set_supply_map(&pdev->dev, desc->desc.name,
			   &desc->init_data->num_consumer_supplies);

	if (!desc->init_data->consumer_supplies)
		desc->init_data->consumer_supplies = supply;

	/* Fill struct sci_regulator_regs variable desc->regs */
	type = (of_property_read_bool(np, "dcdc") ? 2 : 0);
	regs->typ = type;

	if (of_property_read_bool(np, "default-on"))
		regs->typ |= BIT(4);

	of_regu_read_reg(np, 0, regs);
	of_regu_read_reg(np, 1, regs);
	of_regu_read_reg(np, 2, regs);

	regs->min_uV = desc->init_data->constraints.min_uV;

	ret = of_property_read_u32(np, "regulator-step-microvolt", &tmp_val_u32);
	if (!ret)
		regs->step_uV = tmp_val_u32;

	ret = of_property_read_u32(np, "regulator-default-microvolt", &tmp_val_u32);
	if (!ret)
		regs->vol_def = tmp_val_u32;

	debug("[%d] %s type %d, range %d(uV) - %d(uV), step %d(uV), default %d(uV) - (%s)\n", (idx.counter - 1),
		np->name, type, desc->init_data->constraints.min_uV, desc->init_data->constraints.max_uV,
		regs->step_uV, regs->vol_def, (regs->typ & BIT(4)) ? "on" : "off");


	tmp = of_get_property(np, "regulator-cal-channel", &cnt);
	if (tmp) {
		cnt /= 4;
		ret = of_property_read_u32_array(np, "regulator-cal-channel", data, cnt);
		if (!ret) {
			regs->cal_ctl = phy2vir(data[0]);
			regs->cal_ctl_bits = data[1];
			regs->cal_chan = data[2];
		}

		debug0("cal_ctl (phyaddr: 0x%08x, vir addr: 0x%08x, msk: 0x%08x, chan: %d)\n",
			data[0], regs->cal_ctl, regs->cal_ctl_bits, regs->cal_chan);
	}

#if 0
	tmp = of_get_property(np, "regulator-selects", &cnt);
	if (tmp) {
		debug0("prop regulator-selects count(%d)\n", cnt);
		cnt /= 4;
		ret = of_property_read_u32_array(np, "regulator-selects", data, cnt);
		if (!ret) {
			int i = 0;

			/* Dynamically allocate memory for voltage select array */
			regs->vol_sel = kzalloc(cnt * sizeof(*regs->vol_sel), GFP_KERNEL);
			if (!regs->vol_sel) {
				pr_err("%s() failed to allocate memory for voltage select\n", __func__);
				return -ENOMEM;
			}

			regs->vol_sel_cnt = cnt;
			for(i = 0; i < cnt; i++) {
				regs->vol_sel[i] = data[i];
			}
		}
	}
#endif

	return 0;
}

static int sci_regulator_register_dt(struct platform_device *pdev)
{
	struct sci_regulator_desc *sci_desc = NULL;

	struct regulator_dev *rdev;
	struct regulator_ops *__regs_ops[] = {
		&ldo_ops, 0, &dcdc_ops, 0 /*lpref_ops */ , &boost_ops, 0,
	};
	struct regulator_consumer_supply consumer_supplies_default[1] = { };
	struct regulator_config config = { };

	struct device_node *dev_np = pdev->dev.of_node;
	struct device_node *child_np;
	int ret = 0;

	regu_cnt = of_get_child_count(dev_np);
	if (!regu_cnt)
		return -EINVAL;

	regu_cnt -= 1; /* exclude dummy node */

	sci_desc_list = devm_kzalloc(&pdev->dev, regu_cnt * sizeof(struct sci_regulator_desc), GFP_KERNEL);
	if (!sci_desc_list) {
		dev_err(&pdev->dev, "failed allocate memory for sci_regulator_desc list\n");
		return -ENOMEM;
	}
	sci_desc = sci_desc_list;

	debug("regulators desc list 0x%p, count %d, sci_regulator_desc size %d\n", sci_desc, regu_cnt, sizeof(struct sci_regulator_desc));

	for_each_child_of_node(dev_np, child_np) {
		if(0 == strcmp(child_np->name, "dummy")) /* skip dummy node */
			continue;

		ret = sci_regulator_parse_dt(pdev, child_np, sci_desc, consumer_supplies_default, ARRAY_SIZE(consumer_supplies_default));
		if(ret) {
			dev_err(&pdev->dev, "failed to parse regulator(%s) dts\n", child_np->name);
			continue;
		}

		reconfig_regulator(sci_desc);

		BUG_ON((sci_desc->regs.typ & (BIT(4) - 1)) >= ARRAY_SIZE(__regs_ops));
		if (!sci_desc->desc.ops)
			sci_desc->desc.ops = __regs_ops[sci_desc->regs.typ & (BIT(4) - 1)];

        config.dev = &pdev->dev;
        config.init_data = sci_desc->init_data;
        config.driver_data = NULL;
        config.of_node = child_np;
        rdev = regulator_register(&sci_desc->desc, &config);

		debug0("regulator_desc 0x%p, rdev 0x%p\n", &sci_desc->desc, rdev);

		if (sci_desc->init_data->consumer_supplies != consumer_supplies_default)
			kfree(sci_desc->init_data->consumer_supplies);

		if (!IS_ERR_OR_NULL(rdev)) {
			rdev->reg_data = rdev;
			sci_desc->data.rdev = rdev;
			__init_trimming(rdev);
			rdev_init_debugfs(rdev);
		}

		sci_desc++;
	}

	return 0;
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
	if (IS_ERR_OR_NULL(debugfs_root)) {
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

	ana_chip_id = ((u32)ANA_REG_GET(ANA_REG_GLB_CHIP_ID_HIGH) << 16) |
				((u32)ANA_REG_GET(ANA_REG_GLB_CHIP_ID_LOW) & 0xFFFF);
	ana_mixed_ctl = ANA_REG_GET(ANA_REG_GLB_MIXED_CTRL0);
	otp_pwr_sel = ANA_REG_GET(ANA_REG_GLB_PWR_SEL);

	pr_info("sc272x ana chipid:(0x%08x), ana_mixed_ctl:(0x%08x), otp_sel:(0x%08x)\n", ana_chip_id, ana_mixed_ctl, otp_pwr_sel);

	spl_start_base = ioremap(SPRD_SPL_PHYS, 0x8000);
	if(spl_start_base) {
		pr_info("%s remap iram phy addr(%#x) to vir addr (0x%p) ok!\n", __func__, SPRD_SPL_PHYS, spl_start_base);
	} else {
		pr_info("%s remap iram phy addr(%#x) error!\n", __func__, SPRD_SPL_PHYS);
	}

	sci_regulator_register_dt(pdev);

	if(spl_start_base) {
		iounmap(spl_start_base);
		spl_start_base = NULL;
	}

	return 0;
}

static int sci_regulator_remove(struct platform_device *pdev)
{
	if(sci_desc_list) {
		if(sci_desc_list->init_data) {
			kfree(sci_desc_list->init_data);
		}
		kfree(sci_desc_list);
	}

	return 0;
}

static struct platform_driver sci_regulator_driver = {
	.driver = {
		   .name = "sc2723-regulator",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(sprd_regulator_of_match),
	},
	.probe = sci_regulator_probe,
	.remove = sci_regulator_remove
};

static int __init regu_driver_init(void)
{
	__adc_cal_fuse_setup();

	return platform_driver_register(&sci_regulator_driver);
}

int __init sci_regulator_init(void)
{
#if 0
	return of_platform_populate(of_find_node_by_path("/sprd-regulators"),
			sprd_regulator_of_match, NULL, NULL);
#else
	return 0;
#endif
}

subsys_initcall(regu_driver_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum voltage regulator driver");
MODULE_AUTHOR("kevin <ronghua.yu@spreadtrum.com>");
MODULE_VERSION("0.6");
