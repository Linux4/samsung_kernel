/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/adi.h>

#undef debug
#define debug(format, arg...) pr_debug("regu: " "@@@%s: " format, __func__, ## arg)
#define debug0(format, arg...)

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
#define	ANA_REG_SET(_r, _v, _m)	sci_adi_write(_r, _v, _m)
#endif

struct sci_regulator_regs {
	int typ;
	u32 pd_set, pd_set_bit;
	u32 pd_rst, pd_rst_bit;
	u32 slp_ctl, slp_ctl_bit;
	u32 vol_trm, vol_trm_bits;
	u32 vol_ctl, vol_ctl_bits;
	u32 vol_sel_cnt, vol_sel[];
};

struct sci_regulator_desc {
	struct regulator_desc desc;
	const struct sci_regulator_regs *regs;
#if defined(CONFIG_DEBUG_FS)
	struct dentry *debugfs;
#endif
};

enum {
	VDD_TYP_LDO = 0,
	VDD_TYP_LDO_D = 1,
	VDD_TYP_DCDC = 2,
};

enum {
	VDD_IS_ON = 0,
	VDD_ON,
	VDD_OFF,
	VOL_SET,
	VOL_GET,
};

#define SCI_REGU_REG(VDD, TYP, PD_SET, SET_BIT, PD_RST, RST_BIT, SLP_CTL, SLP_CTL_BIT, \
                     VOL_TRM, VOL_TRM_BITS, VOL_CTL, VOL_CTL_BITS, VOL_SEL_CNT, ...)   \
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

int reguator_is_trimming(struct regulator_dev *rdev);
int ldo_trimming_callback(void *data);
int __def_callback(void *data)
{
	return WARN_ON(1);
}

int ldo_trimming_callback(void *)
    __attribute__ ((weak, alias("__def_callback")));

/* standard ldo ops*/
int sci_ldo_op(const struct sci_regulator_regs *regs, int op)
{
	int ret = 0;

	debug0("regu %p op(%d), set %08x[%d], rst %08x[%d]\n", regs, op,
	       regs->pd_set, __ffs(regs->pd_set_bit), regs->pd_rst,
	       __ffs(regs->pd_rst_bit));

	if (!regs->pd_rst || !regs->pd_set)
		return -EACCES;

	switch (op) {
	case VDD_ON:
		ANA_REG_OR(regs->pd_rst, regs->pd_rst_bit);
		ANA_REG_BIC(regs->pd_set, regs->pd_set_bit);
		break;
	case VDD_OFF:
		ANA_REG_OR(regs->pd_set, regs->pd_set_bit);
		ANA_REG_BIC(regs->pd_rst, regs->pd_rst_bit);
		break;
	case VDD_IS_ON:
		ret = ! !(ANA_REG_GET(regs->pd_rst) & regs->pd_rst_bit);
		if (ret == ! !(ANA_REG_GET(regs->pd_set) & regs->pd_set_bit))
			return -EINVAL;
		break;
	default:
		break;
	}
	return ret;
}

static int ldo_turn_on(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;

	int ret = sci_ldo_op(regs, VDD_ON);

	debug0("regu %p (%s), turn on\n", regs, desc->desc.name);
	/* notify ldo trimming when first turn on */
	if (0 == ret && regs->vol_trm && !reguator_is_trimming(rdev)) {
		ldo_trimming_callback((void *)desc->desc.name);
	}
	return ret;
}

static int ldo_turn_off(struct regulator_dev *rdev)
{
	return sci_ldo_op(((struct sci_regulator_desc *)(rdev->desc))->regs,
			  VDD_OFF);
}

static int ldo_is_on(struct regulator_dev *rdev)
{
	return sci_ldo_op(((struct sci_regulator_desc *)(rdev->desc))->regs,
			  VDD_IS_ON);
}

static int ldo_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
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
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;
	int mv = min_uV / 1000;
	int ret = -EINVAL;
	int i, shft = __ffs(regs->vol_ctl_bits);
	BUG_ON(regs->vol_sel_cnt > 4);
	debug("regu %p (%s) %d %d\n", regs, desc->desc.name, min_uV, max_uV);

	if (!regs->vol_ctl)
		return -EACCES;
	for (i = 0; i < regs->vol_sel_cnt; i++) {
		if (regs->vol_sel[i] == mv) {
			ANA_REG_SET(regs->vol_ctl, vol_bits[i] << shft,
				    regs->vol_ctl_bits);
			//clear_bit(desc->desc.id, trimming_state);
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
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;
	u32 vol, vol_bits;
	int i, shft = __ffs(regs->vol_ctl_bits);

	debug0("regu %p (%s), vol ctl %08x, shft %d, mask %08x\n",
	       regs, desc->desc.name, regs->vol_ctl, shft, regs->vol_ctl_bits);

	if (!regs->vol_ctl)
		return -EACCES;

	BUG_ON(regs->vol_sel_cnt != 4);
	vol_bits = ((ANA_REG_GET(regs->vol_ctl) & regs->vol_ctl_bits) >> shft);

	if ((vol_bits & BIT(0)) ^ (vol_bits & BIT(1))
	    && (vol_bits & BIT(2)) ^ (vol_bits & BIT(3))) {
		i = (vol_bits & BIT(0)) | ((vol_bits >> 1) & BIT(1));
		vol = regs->vol_sel[i];
		debug("regu %p (%s), voltage %d\n", regs, desc->desc.name, vol);
		return vol * 1000;
	}
	return -EFAULT;
}

static unsigned long trimming_state[2] = { 0, 0 };	/* 64 bits */

int reguator_is_trimming(struct regulator_dev *rdev)
{
	int id;
	BUG_ON(!rdev);
	id = rdev->desc->id;
	BUG_ON(!(id > 0 && id < sizeof(trimming_state) * 8));

	return test_bit(id, trimming_state);
}

static int ldo_init_trimming(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;
	int ret = -EINVAL;
	int shft = __ffs(regs->vol_trm_bits);
	u32 trim;

	if (!regs->vol_trm)
		goto exit;

	trim = (ANA_REG_GET(regs->vol_trm) & regs->vol_trm_bits) >> shft;
	if (trim != 0x10 /* 100 % */ ) {
		debug("regu %p (%s) trimming ok\n", regs, desc->desc.name);
		set_bit(desc->desc.id, trimming_state);
		ret = trim;
	} else if (1 == ldo_is_on(rdev)) {	/* some LDOs had been turned in uboot-spl */
		ret = ldo_turn_on(rdev);
	}

exit:
	return ret;
}

/**
 * ldo trimming step about 0.7%, range 90% ~ 110%. that all maps as follow.
	0x00 : 90.000
	0x01 : 90.625
	0x02 : 91.250
	0x03 : 91.875
	0x04 : 92.500
	0x05 : 93.125
	0x06 : 93.750
	0x07 : 94.375
	0x08 : 95.000
	0x09 : 95.625
	0x0A : 96.250
	0x0B : 96.875
	0x0C : 97.500
	0x0D : 98.125
	0x0E : 98.750
	0x0F : 99.375
	0x10 : 100.000
	0x11 : 100.625
	0x12 : 101.250
	0x13 : 101.875
	0x14 : 102.500
	0x15 : 103.125
	0x16 : 103.750
	0x17 : 104.375
	0x18 : 105.000
	0x19 : 105.625
	0x1A : 106.250
	0x1B : 106.875
	0x1C : 107.500
	0x1D : 108.125
	0x1E : 108.750
	0x1F : 109.375

	0x20 : 110.000
 */
static int ldo_set_trimming(struct regulator_dev *rdev, int ctl_vol, int to_vol)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;
	int ret = -EINVAL, cal_vol;

	ctl_vol /= 1000;
	to_vol /= 1000;

	cal_vol = ctl_vol - to_vol * 90 / 100;	/* cal range 90% ~ 110% */
	if (!regs->vol_trm || cal_vol < 0 || cal_vol >= to_vol * 20 / 100)
		goto exit;

	/* always update voltage ctrl bits */
	ret = ldo_set_voltage(rdev, to_vol * 1000, to_vol * 1000, 0);
	if (IS_ERR_VALUE(ret) && regs->vol_ctl)
		goto exit;

	else {
		u32 trim =	/* assert 5 valid trim bits */
		    (cal_vol * 100 * 32) / (to_vol * 20) & 0x1f;
		debug
		    ("regu %p (%s) trimming %u = %u %+dmv, got [%02X] %u.%03u%%\n",
		     regs, desc->desc.name, ctl_vol, to_vol,
		     (cal_vol - to_vol / 10), trim, ctl_vol * 100 / to_vol,
		     (ctl_vol * 100 * 1000 / to_vol) % 1000);

		ANA_REG_SET(regs->vol_trm, trim << __ffs(regs->vol_trm_bits),
			    regs->vol_trm_bits);
		set_bit(desc->desc.id, trimming_state);
		ret = 0;
	}

exit:
	return ret;
}

int regulator_set_trimming(struct regulator *regulator, int ctl_vol, int to_vol)
{
	struct regulator_dev *rdev = regulator_get_drvdata(regulator);
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;

	return (2 /*DCDC*/ == regs->typ)
	    ? regulator_set_voltage(regulator, ctl_vol, ctl_vol)
	    : ldo_set_trimming(rdev, ctl_vol, to_vol);
}

int regulator_get_trimming_step(struct regulator *regulator, int def_vol)
{
	struct regulator_dev *rdev = regulator_get_drvdata(regulator);
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;

	return (2 /*DCDC*/ == regs->typ)
	    ? 100 / 32
	    : def_vol * 7 / 1000;
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

/* standard dcdc ops*/
static int dcdc_set_voltage(struct regulator_dev *rdev, int min_uV,
			    int max_uV, unsigned *selector)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;
	int mv = min_uV / 1000;
	int i, shft = __ffs(regs->vol_ctl_bits);
	int max = regs->vol_ctl_bits >> shft;

	debug0("regu %p (%s) %d %d\n", regs, desc->desc.name, min_uV, max_uV);

	BUG_ON(shft != 0);
	BUG_ON(regs->vol_sel_cnt > 8);

	if (!regs->vol_ctl)
		return -EACCES;

	/* found the closely vol ctrl bits */
	i = __match_dcdc_vol(regs, mv);
	if (i < 0)
		return -EINVAL;

	debug("regu %p (%s) %d = %d %+dmv\n", regs, desc->desc.name,
	      mv, regs->vol_sel[i], mv - regs->vol_sel[i]);

	if (regs->vol_trm) {	/* small adjust first */
		/* dcdc calibration control bits (default 00000),
		 * small adjust voltage: 100/32mv ~= 3.125mv
		 */
		int j = ((mv - regs->vol_sel[i]) * 32) / (100) % 32;
		ANA_REG_SET(regs->vol_trm,
			    BITS_DCDC_CAL(j) |
			    BITS_DCDC_CAL_RST(BITS_DCDC_CAL(-1) - j), -1);
	}

	ANA_REG_SET(regs->vol_ctl, i | (max - i) << 4, -1);
	return 0;
}

static int dcdc_get_voltage(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;
	const struct sci_regulator_regs *regs = desc->regs;
	u32 mv, vol_bits;
	int cal = 0;		/* mV */
	int i, shft = __ffs(regs->vol_ctl_bits);

	debug0("regu %p (%s), vol ctl %08x, shft %d, mask %08x, sel %d\n",
	       regs, desc->desc.name, regs->vol_ctl,
	       shft, regs->vol_ctl_bits, regs->vol_sel_cnt);

	BUG_ON(shft != 0);
	BUG_ON(regs->vol_sel_cnt > 8);

	if (!regs->vol_ctl)
		return -EINVAL;

	i = (ANA_REG_GET(regs->vol_ctl) & regs->vol_ctl_bits);

	/*check the reset relative bit of vol ctl */
	vol_bits =
	    (~ANA_REG_GET(regs->vol_ctl) & (regs->vol_ctl_bits << 4)) >> 4;

	if (i != vol_bits)
		return -EFAULT;

	mv = regs->vol_sel[i];

	if (regs->vol_trm) {
		int j = ANA_REG_GET(regs->vol_trm) & BITS_DCDC_CAL(-1);
		cal = DIV_ROUND_CLOSEST(j * 100, 32);
	}

	debug("regu %p (%s) %d +%dmv\n", regs, desc->desc.name, mv, cal);
	return (mv + cal) * 1000;
}

/* standard ldo-D-Die ops*/
static int usbd_turn_on(struct regulator_dev *rdev)
{
	const struct sci_regulator_regs *regs =
	    ((struct sci_regulator_desc *)(rdev->desc))->regs;
	sci_glb_clr(regs->pd_set, regs->pd_set_bit);
	return 0;
}

static int usbd_turn_off(struct regulator_dev *rdev)
{
	const struct sci_regulator_regs *regs =
	    ((struct sci_regulator_desc *)(rdev->desc))->regs;
	sci_glb_set(regs->pd_set, regs->pd_set_bit);
	return 0;
}

static int usbd_is_on(struct regulator_dev *rdev)
{
	const struct sci_regulator_regs *regs =
	    ((struct sci_regulator_desc *)(rdev->desc))->regs;
	return !sci_glb_read(regs->pd_set, regs->pd_set_bit);
}

static struct regulator_ops ldo_ops = {
	.enable = ldo_turn_on,
	.disable = ldo_turn_off,
	.is_enabled = ldo_is_on,
	.set_voltage = ldo_set_voltage,
	.get_voltage = ldo_get_voltage,
	.set_mode = ldo_set_mode,
};

static struct regulator_ops usbd_ops = {
	.enable = usbd_turn_on,
	.disable = usbd_turn_off,
	.is_enabled = usbd_is_on,
};

static struct regulator_ops dcdc_ops = {
	.enable = ldo_turn_on,
	.disable = ldo_turn_off,
	.is_enabled = ldo_is_on,
	.set_voltage = dcdc_set_voltage,
	.get_voltage = dcdc_get_voltage,
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
		pr_info("supply %s consumers %d - %d\n", supply_name, i, n);
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
	if (rdev && rdev->desc->ops->get_voltage)
		*val = rdev->desc->ops->get_voltage(rdev) / 1000;
	else
		*val = -1;
	return 0;
}

static int debugfs_voltage_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->set_voltage)
		rdev->desc->ops->set_voltage(rdev, val * 1000, val * 1000, 0);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_enable,
			debugfs_enable_get, debugfs_enable_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_voltage,
			debugfs_voltage_get, debugfs_voltage_set, "%llu\n");

static void rdev_init_debugfs(struct regulator_dev *rdev)
{
	struct sci_regulator_desc *desc =
	    (struct sci_regulator_desc *)rdev->desc;

	desc->debugfs = debugfs_create_dir(rdev->desc->name, debugfs_root);
	if (IS_ERR(rdev->debugfs) || !rdev->debugfs) {
		pr_warn("Failed to create debugfs directory\n");
		rdev->debugfs = NULL;
		return;
	}

	debugfs_create_file("enable", S_IRUGO | S_IWUSR,
			    desc->debugfs, rdev, &fops_enable);

	debugfs_create_file("voltage", S_IRUGO | S_IWUSR,
			    desc->debugfs, rdev, &fops_voltage);
}
#else
static void rdev_init_debugfs(struct regulator_dev *rdev)
{
}
#endif

void *__devinit sci_regulator_register(struct platform_device *pdev,
				       struct sci_regulator_desc *desc)
{
	static atomic_t __devinitdata idx = ATOMIC_INIT(1);	/* 0: dummy */
	struct regulator_dev *rdev;
	struct regulator_ops *__regs_ops[] = {
		&ldo_ops, &usbd_ops, &dcdc_ops, 0,
	};
	struct regulator_consumer_supply consumer_supplies_default[] = {
		[0] = {
//		       .dev = 0,
		       .dev_name = 0,
		       .supply = desc->desc.name,
		       }
	};
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

	BUG_ON(desc->regs->typ > 3);
	if (!desc->desc.ops)
		desc->desc.ops = __regs_ops[desc->regs->typ];

	desc->desc.id = atomic_inc_return(&idx) - 1;

	init_data.consumer_supplies =
	    set_supply_map(&pdev->dev, desc->desc.name,
			   &init_data.num_consumer_supplies);

	if (!init_data.consumer_supplies)
		init_data.consumer_supplies = consumer_supplies_default;

	debug0("regu %p (%s)\n", desc->regs, desc->desc.name);
	rdev = regulator_register(&desc->desc, &pdev->dev, &init_data, 0, 0);
	if (init_data.consumer_supplies != consumer_supplies_default)
		kfree(init_data.consumer_supplies);

	if (!IS_ERR(rdev)) {
		rdev->reg_data = rdev;
		if (desc->desc.ops == &ldo_ops)
			ldo_init_trimming(rdev);
		rdev_init_debugfs(rdev);
	}
	return rdev;
}

/**
 * IMPORTANT!!!
 * spreadtrum power regulators is intergrated on the chip, include LDOs and DCDCs.
 * so i autogen all regulators non-variable description in plat or mach directory,
 * which named __regulator_map.h, BUT register all in regulator driver probe func,
 * just like other regulator vendor drivers.
 */
static int __devinit sci_regulator_probe(struct platform_device *pdev)
{
	debug0("platform device %p\n", pdev);
#include CONFIG_REGULATOR_SPRD_MAP
	return 0;
}

static struct platform_driver sci_regulator_driver = {
	.driver = {
		   .name = "sprd-regulator",
		   .owner = THIS_MODULE,
		   },
	.probe = sci_regulator_probe,
};

static int __init sci_regulator_init(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_root =
	    debugfs_create_dir(sci_regulator_driver.driver.name, NULL);
	if (IS_ERR(debugfs_root) || !debugfs_root) {
		pr_warn("%s: Failed to create debugfs directory\n",
			sci_regulator_driver.driver.name);
		debugfs_root = NULL;
	}
#endif
	return platform_driver_register(&sci_regulator_driver);
}

subsys_initcall(sci_regulator_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum voltage regulator driver");
MODULE_AUTHOR("robot <zhulin.lian@spreadtrum.com>");
