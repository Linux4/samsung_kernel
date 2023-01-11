/*
 * GPADC driver for Marvell 88PM886 PMIC
 *
 * Copyright (c) 2014 Marvell International Ltd.
 * Author:	Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/driver.h>
#include <linux/iio/machine.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/delay.h>

#define PM88X_GAPDC_CONFIG11		(0x0b) /* perbias and bias for gpadc0 */
#define PM88X_GAPDC_CONFIG12		(0x0c) /* perbias and bias for gpadc1 */
#define PM88X_GAPDC_CONFIG13		(0x0d) /* perbias and bias for gpadc2 */
#define PM88X_GAPDC_CONFIG14		(0x0e) /* perbias and bias for gpadc3 */
#define PM88X_GPADC_CONFIG20		(0x14) /* gp_bias_out and gp_bias_en */

/* for nonexistent measurement */
#define PM88X_GPADC_NO_REG		(0x0)
#define PM88X_GPADC_NO_EN_MASK		(0x0)
#define PM88X_GPADC_NO_MIN_MEA		(0x0)
#define PM88X_GPADC_NO_MAX_MEA		(0x0)
#define PM88X_GPADC_NO_AVG_MEA		(0x0)
#define PM88X_GPADC_NO_SLP_AVG_MEA	(0x0)

/* GPADC enable mask */
#define PM88X_GPADC_VSC_EN_MASK		(0x01)
#define PM88X_GPADC_VBAT_EN_MASK	(0x02)
#define PM88X_GPADC_GND_DET1_EN_MASK	(0x08)
#define PM88X_GPADC_VBUS_EN_MASK	(0x10)
#define PM88X_GPADC_VCHG_PWR_EN_MASK	(0x20)
#define PM88X_GPADC_VCF_OUT_EN_MASK	(0x40)
#define PM88X_GPADC_TINT_EN_MASK	(0x01)
#define PM88X_GPADC_PMODE_EN_MASK	(0x02)
#define PM88X_GPADC_GPADC0_EN_MASK	(0x04)
#define PM88X_GPADC_GPADC1_EN_MASK	(0x08)
#define PM88X_GPADC_GPADC2_EN_MASK	(0x10)
#define PM88X_GPADC_GPADC3_EN_MASK	(0x20)
#define PM88X_GPADC_MIC_DET_EN_MASK	(0x40)
#define PM88X_GPADC_GND_DET2_EN_MASK	(0x01)


/* GPADC minimum measurement  */
#define PM88X_GPADC_VBAT_MIN_MEA	(0x80)
#define PM88X_GPADC_VBUS_MIN_MEA	(0x84)
#define PM88X_GPADC_GPADC3_MIN_MEA	(0x86)
#define PM88X_GPADC_MIC_DET_MIN_MEA	(0x88)

/* GPADC maximum measurement  */
#define PM88X_GPADC_VBAT_MAX_MEA	(0x90)
#define PM88X_GPADC_VBUS_MAX_MEA	(0x94)
#define PM88X_GPADC_GPADC3_MAX_MEA	(0x96)
#define PM88X_GPADC_MIC_DET_MAX_MEA	(0x98)

/* GPADC average measurement  */
#define PM88X_GPADC_VBAT_AVG_MEA	(0xa0)
#define PM88X_GPADC_GND_DET1_AVG_MEA	(0xa4)
#define PM88X_GPADC_GND_DET2_AVG_MEA	(0xa6)
#define PM88X_GPADC_VBUS_AVG_MEA	(0xa8)
#define PM88X_GPADC_GPADC3_AVG_MEA	(0xaa)
#define PM88X_GPADC_MIC_DET_AVG_MEA	(0xac)
#define PM88X_GPADC_VBAT_SLP_AVG_MEA	(0xb0)

#define PM88X_GPADC_CHECK_EN(val) (val ? "enable" : "disable")

enum {
	GPADC_0_RES,
	GPADC_1_RES,
	GPADC_2_RES,
	GPADC_3_RES,
	GPADC_MAX,
};

enum {
	VSC_VOLT_CHAN,
	VCHG_PWR_VOLT_CHAN,
	VCF_OUT_CHAN,
	TINT_TEMP_CHAN,

	GPADC0_VOLT_CHAN,
	GPADC1_VOLT_CHAN,
	GPADC2_VOLT_CHAN,

	VBAT_VOLT_CHAN = 7,
	GNDDET1_VOLT_CHAN,
	GNDDET2_VOLT_CHAN,
	VBUS_VOLT_CHAN,
	GPADC3_VOLT_CHAN,
	MIC_DET_VOLT_CHAN,
	VBAT_SLP_VOLT_CHAN = 13,

	GPADC0_RES_CHAN = 14,
	GPADC1_RES_CHAN = 15,
	GPADC2_RES_CHAN = 16,
	GPADC3_RES_CHAN = 17,
};

struct pm88x_gpadc_info {
	struct pm88x_chip *chip;
	struct mutex	lock;
	u8 (*channel_to_reg)(int channel);
	u8 (*channel_to_gpadc_num)(int channel);
	struct iio_map *map;
};

struct pm88x_gpadc_extra_info {
	const char *name;
	const char *unit;
	u8 enable_reg;
	u8 enable_mask;
	u8 avg_reg;
	u8 min_reg;
	u8 max_reg;
	u8 slp_reg;
};

#define PM88X_GPADC_EXTRA(_name, _unit, _en_reg, _en_mask, _avg_reg, _min_reg, _max_reg, _slp_reg)\
{								\
	.name = _name,						\
	.unit = _unit,						\
	.enable_reg = PM88X_GPADC_##_en_reg,			\
	.enable_mask = PM88X_GPADC_##_en_mask##_EN_MASK,	\
	.avg_reg = PM88X_GPADC_##_avg_reg##_AVG_MEA,		\
	.min_reg = PM88X_GPADC_##_min_reg##_MIN_MEA,		\
	.max_reg = PM88X_GPADC_##_max_reg##_MAX_MEA,		\
	.slp_reg = PM88X_GPADC_##_slp_reg##_SLP_AVG_MEA,	\
}

static struct pm88x_gpadc_extra_info pm88x_extra_gpadc[] = {
	/* PM88X_GPADC_EXTRA(name, unit, en_reg, en_mask, avg_reg, min_reg, max_reg, slp_reg) */
	PM88X_GPADC_EXTRA("gpadc0", "mV", CONFIG2, GPADC0, NO, NO, NO, NO),
	PM88X_GPADC_EXTRA("gpadc1", "mV", CONFIG2, GPADC1, NO, NO, NO, NO),
	PM88X_GPADC_EXTRA("gpadc2", "mV", CONFIG2, GPADC2, NO, NO, NO, NO),
	PM88X_GPADC_EXTRA("gpadc3", "mV", CONFIG2, GPADC3, GPADC3, GPADC3, GPADC3, NO),
};
static struct pm88x_gpadc_extra_info pm88x_extra_other[] = {
	/* PM88X_GPADC_EXTRA(name, unit, en_reg, en_mask, avg_reg, min_reg, max_reg, slp_reg) */
	PM88X_GPADC_EXTRA("vsc", "mV", CONFIG1, VSC, NO, NO, NO, NO),
	PM88X_GPADC_EXTRA("vbat", "mV", CONFIG1, VBAT, VBAT, VBAT, VBAT, VBAT),
	PM88X_GPADC_EXTRA("gnddet1", "mV", CONFIG1, GND_DET1, GND_DET1, NO, NO, NO),
	PM88X_GPADC_EXTRA("vbus", "mV", CONFIG1, VBUS, VBUS, VBUS, VBUS, NO),
	PM88X_GPADC_EXTRA("vchg_pwr", "mV", CONFIG1, VCHG_PWR, NO, NO, NO, NO),
	PM88X_GPADC_EXTRA("vcf_out", "mV", CONFIG1, VCF_OUT, NO, NO, NO, NO),
	PM88X_GPADC_EXTRA("tint", "C", CONFIG2, TINT, NO, NO, NO, NO),
	PM88X_GPADC_EXTRA("pmode", "mV", CONFIG2, PMODE, NO, NO, NO, NO),
	PM88X_GPADC_EXTRA("mic_det", "mV", CONFIG2, MIC_DET, MIC_DET, MIC_DET, MIC_DET, NO),
	PM88X_GPADC_EXTRA("gnddet2", "mV", CONFIG3, GND_DET2, GND_DET2, NO, NO, NO),
};

struct pm88x_gpadc_bias_info {
	const char *name;
	u8 bias_reg;
	u8 bias_mask;
	u8 bias_en_reg;
	u8 bias_en_mask;
};

#define PM88X_GPADC_BIAS_INFO(_name, _reg, _mask, _en_reg, _en_mask)	\
{					\
	.name = _name,			\
	.bias_reg = _reg,		\
	.bias_mask = _mask,		\
	.bias_en_reg = _en_reg,		\
	.bias_en_mask = _en_mask,	\
}

static struct pm88x_gpadc_bias_info pm88x_bias_info[] = {
	PM88X_GPADC_BIAS_INFO("gpadc0", 0x0b, 0x0f, 0x14, 0x1),
	PM88X_GPADC_BIAS_INFO("gpadc1", 0x0c, 0x0f, 0x14, 0x2),
	PM88X_GPADC_BIAS_INFO("gpadc2", 0x0d, 0x0f, 0x14, 0x4),
	PM88X_GPADC_BIAS_INFO("gpadc3", 0x0e, 0x0f, 0x14, 0x8),
};

/* used by external access */
static struct pm88x_gpadc_info *g_gpadc;
static struct iio_dev *g_iio;

static u8 pm88x_channel_to_reg(int channel)
{
	u8 reg;

	switch (channel) {
	case VSC_VOLT_CHAN:
		reg = 0x40;
		break;
	case VCHG_PWR_VOLT_CHAN:
	case VCF_OUT_CHAN:
	case TINT_TEMP_CHAN:
		reg = 0x4c + (channel - VCHG_PWR_VOLT_CHAN) * 2;
		break;
	case GPADC0_VOLT_CHAN:
	case GPADC1_VOLT_CHAN:
	case GPADC2_VOLT_CHAN:
		/* gapdc 0/1/2 */
		reg = 0x54 + (channel - GPADC0_VOLT_CHAN) * 2;
		break;

	case VBAT_VOLT_CHAN:
		reg = 0xa0;
		break;

	case GNDDET1_VOLT_CHAN:
	case GNDDET2_VOLT_CHAN:
	case VBUS_VOLT_CHAN:
	case GPADC3_VOLT_CHAN:
	case MIC_DET_VOLT_CHAN:
		reg = 0xa4 + (channel - GNDDET1_VOLT_CHAN) * 2;
		break;
	case VBAT_SLP_VOLT_CHAN:
		reg = 0xb0;
		break;
	case GPADC0_RES_CHAN:
	case GPADC1_RES_CHAN:
	case GPADC2_RES_CHAN:
		/* gapdc 0/1/2 -- resistor */
		reg = 0x54 + (channel - GPADC0_RES_CHAN) * 2;
		break;

	case GPADC3_RES_CHAN:
		/* gapdc 3 -- resistor */
		reg = 0xaa;
		break;

	default:
		reg = 0xb0;
		break;
	}
	pr_debug("%s: reg = 0x%x\n", __func__, reg);

	return reg;
}

static u8 pm88x_channel_to_gpadc_num(int channel)
{
	return channel - GPADC0_RES_CHAN;
}

/*
 * enable/disable bias current
 * - there are 4 GPADC channels
 * - the workable range for the GPADC is [0, 1400mV],
 */
static int pm88x_gpadc_set_current_generator(struct pm88x_gpadc_info *info,
					     int gpadc_number, int on)
{
	int ret;
	u8 gp_bias_out, gp_bias_en, mask1, mask2;

	if (!info || !info->chip)
		return -EINVAL;

	if (gpadc_number > GPADC_3_RES || gpadc_number < GPADC_0_RES)
		return -EINVAL;

	mask1 = BIT(gpadc_number) << 4;
	mask2 = BIT(gpadc_number);
	if (on) {
		gp_bias_out = mask1;
		gp_bias_en = mask2;
	} else {
		gp_bias_out = 0;
		gp_bias_en = 0;
	}

	ret = regmap_update_bits(info->chip->gpadc_regmap,
				 PM88X_GPADC_CONFIG20, mask1 | mask2,
				 gp_bias_out | gp_bias_en);

	return ret;
}

static int pm88x_gpadc_get_raw(struct pm88x_gpadc_info *gpadc, int channel, int *res)
{
	u8 buf[2];
	int raw, ret;
	u8 reg = gpadc->channel_to_reg(channel);

	ret = regmap_bulk_read(gpadc->chip->gpadc_regmap, reg, buf, 2);
	if (ret) {
		dev_err(gpadc->chip->dev, "unable to read reg 0x%x\n", reg);
		return ret;
	}

	raw = ((buf[0] & 0xff)  << 4) | (buf[1] & 0xf);
	raw &= 0xfff;
	*res = raw;

	dev_dbg(gpadc->chip->dev,
		"%s: reg = 0x%x, buf[0] = 0x%x, buf[1] = 0x%x, raw_val = %d\n",
		 __func__, reg, buf[0], buf[1], *res);

	return ret;
}

static int pm88x_gpadc_get_processed(struct pm88x_gpadc_info *gpadc,
				     int channel, int *res)
{
	int ret, val;
	struct iio_dev *iio = iio_priv_to_dev(gpadc);

	ret = pm88x_gpadc_get_raw(gpadc, channel, &val);
	if (ret) {
		dev_err(gpadc->chip->dev, "get raw value fails: 0x%x\n", ret);
		return ret;
	}
	*res = val * ((iio->channels[channel]).address);

	dev_dbg(gpadc->chip->dev,
		"%s: raw_val = 0x%x, step_val = %ld, processed_val = %d\n",
		 __func__, val, (iio->channels[channel]).address, *res);

	return ret;
}

static int pm88x_gpadc_set_bias_current(struct pm88x_gpadc_info *info,
					   int gpadc_number, int bias_current)
{
	int reg_val;
	u8 reg;
	switch (gpadc_number) {
	case GPADC_0_RES:
		reg = PM88X_GAPDC_CONFIG11;
		break;
	case GPADC_1_RES:
		reg = PM88X_GAPDC_CONFIG12;
		break;
	case GPADC_2_RES:
		reg = PM88X_GAPDC_CONFIG13;
		break;
	case GPADC_3_RES:
		reg = PM88X_GAPDC_CONFIG14;
		break;
	default:
		dev_err(info->chip->dev, "unsupported gpadc!\n");
		return -EINVAL;
	}

	reg_val = (bias_current - 1) / 5;
	return regmap_update_bits(info->chip->gpadc_regmap, reg, 0xf, reg_val);
}

static int pm88x_gpadc_choose_bias_current(struct pm88x_gpadc_info *info,
					   int gpadc_number,
					   int *bias_current, int *bias_voltage)
{
	int ret, channel, index;
	u8 reg;

	switch (gpadc_number) {
	case GPADC_0_RES:
		reg = PM88X_GAPDC_CONFIG11;
		channel = GPADC0_RES_CHAN;
		break;
	case GPADC_1_RES:
		reg = PM88X_GAPDC_CONFIG12;
		channel = GPADC1_RES_CHAN;
		break;
	case GPADC_2_RES:
		reg = PM88X_GAPDC_CONFIG13;
		channel = GPADC2_RES_CHAN;
		break;
	case GPADC_3_RES:
		reg = PM88X_GAPDC_CONFIG14;
		channel = GPADC3_RES_CHAN;
		break;
	default:
		dev_err(info->chip->dev, "unsupported gpadc!\n");
		return -EINVAL;
	}

	/*
	 * set an inital bias 31uA (index=6) which should cover most cases
	 */
	index = 6;
	do {
		ret = regmap_update_bits(info->chip->gpadc_regmap, reg, 0xf, index);
		if (ret < 0)
			return ret;
		usleep_range(20000, 30000);
		/* uA */
		*bias_current = 1 + index * 5;
		ret = pm88x_gpadc_get_processed(info, channel, bias_voltage);
		if (ret < 0)
			return ret;

		/* voltage too low, need to increase bias */
		if (*bias_voltage < 300000) {
			dev_dbg(info->chip->dev,
				"not a good bias! current = %duA, voltage = %duV\n",
				*bias_current, *bias_voltage);
			index++;
		/* voltage too high, need to decrease bias */
		} else if (*bias_voltage > 1250000) {
			dev_dbg(info->chip->dev,
				"not a good bias! current = %duA, voltage = %duV\n",
				*bias_current, *bias_voltage);
			index--;
		/* voltage is inside the valid range */
		} else {
			dev_dbg(info->chip->dev,
				"hit: current = %duA, voltage = %duV\n",
				*bias_current, *bias_voltage);
			break;
		}
	} while (index >= 0 && index <= 15);

	if (index < 0 || index > 15) {
		dev_err(info->chip->dev,
			"reached the boundery, no more biases left! (%duA, %duV)\n",
			*bias_current, *bias_voltage);
	}

	return 0;
}

static int pm88x_gpadc_get_resistor(struct pm88x_gpadc_info *gpadc,
				    int channel, int *res)
{
	int ret, bias_current, bias_voltage;

	u8 gpadc_number = gpadc->channel_to_gpadc_num(channel);

	/* enable bias current */
	ret = pm88x_gpadc_set_current_generator(gpadc, gpadc_number, 1);
	if (ret < 0)
		return ret;

	ret = pm88x_gpadc_choose_bias_current(gpadc, gpadc_number,
					      &bias_current, &bias_voltage);
	if (ret < 0)
		return ret;

	/* uv / uA == ohm */
	*res = bias_voltage / bias_current;

	return 0;
}

static int pm88x_gpadc_read_raw(struct iio_dev *iio,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	struct pm88x_gpadc_info *gpadc = iio_priv(iio);
	int err;
	mutex_lock(&gpadc->lock);

	dev_dbg(gpadc->chip->dev, "%s: channel name = %s\n",
		__func__, chan->datasheet_name);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		err = pm88x_gpadc_get_raw(gpadc, chan->channel, val);
		err = err ? -EIO : IIO_VAL_INT;
		break;

	case IIO_CHAN_INFO_PROCESSED:
		err = pm88x_gpadc_get_processed(gpadc, chan->channel, val);
		err = err ? -EIO : IIO_VAL_INT;
		break;

	case IIO_CHAN_INFO_SCALE:
		err = pm88x_gpadc_get_resistor(gpadc, chan->channel, val);
		err = err ? -EIO : IIO_VAL_INT;
		break;

	default:
		err = 0;
		break;
	}

	mutex_unlock(&gpadc->lock);

	return err;
}

#define ADC_CHANNEL(_index, _lsb, _type, _id, chan_info) {	\
	.type = _type,						\
	.indexed = 1,						\
	.channel = _index,					\
	.address = _lsb,					\
	.info_mask_separate = BIT(chan_info),			\
	.datasheet_name = _id,					\
}

/* according to value register sequence */
static const struct iio_chan_spec pm88x_gpadc_channels[] = {

	ADC_CHANNEL(VSC_VOLT_CHAN, 1367, IIO_VOLTAGE, "vsc", IIO_CHAN_INFO_PROCESSED),
	ADC_CHANNEL(VCHG_PWR_VOLT_CHAN, 1709, IIO_VOLTAGE, "vchg_pwr", IIO_CHAN_INFO_PROCESSED),
	ADC_CHANNEL(VCF_OUT_CHAN, 1367, IIO_VOLTAGE, "vcf_out", IIO_CHAN_INFO_PROCESSED),
	ADC_CHANNEL(TINT_TEMP_CHAN, 104, IIO_TEMP, "tint", IIO_CHAN_INFO_PROCESSED),

	ADC_CHANNEL(GPADC0_VOLT_CHAN, 342, IIO_VOLTAGE, "gpadc0", IIO_CHAN_INFO_PROCESSED),
	ADC_CHANNEL(GPADC1_VOLT_CHAN, 342, IIO_VOLTAGE, "gpadc1", IIO_CHAN_INFO_PROCESSED),
	ADC_CHANNEL(GPADC2_VOLT_CHAN, 342, IIO_VOLTAGE, "gpadc2", IIO_CHAN_INFO_PROCESSED),

	/* use AVG register */
	ADC_CHANNEL(VBAT_VOLT_CHAN, 1367, IIO_VOLTAGE, "vbat", IIO_CHAN_INFO_PROCESSED),
	/* FIXME */
	ADC_CHANNEL(GNDDET1_VOLT_CHAN, 342, IIO_VOLTAGE, "gnddet1", IIO_CHAN_INFO_RAW),
	/* FIXME */
	ADC_CHANNEL(GNDDET2_VOLT_CHAN, 342, IIO_VOLTAGE, "gnddet2", IIO_CHAN_INFO_RAW),
	ADC_CHANNEL(VBUS_VOLT_CHAN, 1709, IIO_VOLTAGE, "vbus", IIO_CHAN_INFO_PROCESSED),
	ADC_CHANNEL(GPADC3_VOLT_CHAN, 342, IIO_VOLTAGE, "gpadc3", IIO_CHAN_INFO_PROCESSED),
	/* FIXME */
	ADC_CHANNEL(MIC_DET_VOLT_CHAN, 1367, IIO_VOLTAGE, "mic_det", IIO_CHAN_INFO_RAW),
	ADC_CHANNEL(VBAT_SLP_VOLT_CHAN, 1367, IIO_VOLTAGE, "vbat_slp", IIO_CHAN_INFO_PROCESSED),

	/* the following are virtual channels used to measure the resistor */
	ADC_CHANNEL(GPADC0_RES_CHAN, 342, IIO_TEMP, "gpadc0_res", IIO_CHAN_INFO_SCALE),
	ADC_CHANNEL(GPADC1_RES_CHAN, 342, IIO_TEMP, "gpadc1_res", IIO_CHAN_INFO_SCALE),
	ADC_CHANNEL(GPADC2_RES_CHAN, 342, IIO_TEMP, "gpadc2_res", IIO_CHAN_INFO_SCALE),
	ADC_CHANNEL(GPADC3_RES_CHAN, 342, IIO_TEMP, "gpadc3_res", IIO_CHAN_INFO_SCALE),
};

static const struct iio_info pm88x_gpadc_iio_info = {
	.read_raw = pm88x_gpadc_read_raw,
	.driver_module = THIS_MODULE,
};

static int pm88x_gpadc_setup(struct pm88x_gpadc_info *gpadc)
{
	int ret;
	if (!gpadc || !gpadc->chip || !gpadc->chip->gpadc_regmap) {
		pr_err("%s: gpadc info is empty.\n", __func__);
		return -ENODEV;
	}
	/* gpadc enable */
	ret = regmap_update_bits(gpadc->chip->gpadc_regmap, PM88X_GPADC_CONFIG6,
				 1 << 0, 1 << 0);
	if (ret < 0)
		return ret;

	/* enable all of the gpadc */
	regmap_write(gpadc->chip->gpadc_regmap, PM88X_GPADC_CONFIG1, 0xff);
	regmap_write(gpadc->chip->gpadc_regmap, PM88X_GPADC_CONFIG2, 0xfd);
	regmap_write(gpadc->chip->gpadc_regmap, PM88X_GPADC_CONFIG3, 0x01);

	return 0;
}

static const struct of_device_id of_pm88x_gpadc_match[] = {
	{ .compatible = "marvell,88pm88x-gpadc" },
	{ }
};
MODULE_DEVICE_TABLE(of, of_pm88x_gpadc_match);

/* default maps used by iio consumer */
static struct iio_map pm88x_default_iio_maps[] = {
	{
		.consumer_dev_name = "88pm88x-battery",
		.consumer_channel = "vbat",
		.adc_channel_label = "vbat",
	},
	{
		.consumer_dev_name = "88pm88x-battery",
		.consumer_channel = "vbat_slp",
		.adc_channel_label = "vbat_slp",
	},
	{
		.consumer_dev_name = "88pm88x-battery",
		.consumer_channel = "gpadc3_res",
		.adc_channel_label = "gpadc3_res",
	},
	{ }
};

static int pm88x_iio_map_register(struct iio_dev *iio,
				  struct pm88x_gpadc_info *gpadc)
{
	struct iio_map *map = pm88x_default_iio_maps;
	int ret;

	ret = iio_map_array_register(iio, map);
	if (ret) {
		dev_err(&iio->dev, "iio map err: %d\n", ret);
		return ret;
	}

	gpadc->map = map;

	return 0;
}

static int pm800_gpadc_check_en(struct regmap *map, unsigned int reg, unsigned int mask)
{
	int ret;
	unsigned int enable;

	ret = regmap_read(map, reg, &enable);
	if (ret < 0)
		return ret;

	enable = (enable & mask);

	return enable;
}

static int pm88x_gpadc_get_meas(struct regmap *map, unsigned int reg, unsigned int *value)
{
	int ret;
	unsigned char val[2];

	ret = regmap_bulk_read(map, reg, val, 2);
	if (ret < 0) {
		*value = reg;
		return ret;
	}

	*value = (val[0] << 4) | (val[1] & 0x0f);
	return 0;
}

static int pm88x_gpadc_print_bias_res(struct regmap *map,
				      struct pm88x_gpadc_extra_info *extra_info,
				      char *buf, int *len, int volt)
{
	struct pm88x_gpadc_bias_info *bias_info = pm88x_bias_info;
	unsigned int en_mask, out_mask, mask, val = 0;
	int i, ret, flag = 0;

	mask = bias_info->bias_mask;
	en_mask = bias_info->bias_en_mask;
	out_mask = bias_info->bias_en_mask << 4;
	for (i = 0; i < ARRAY_SIZE(pm88x_bias_info); i++) {
		if (!(strcmp(extra_info->name, bias_info[i].name)) && bias_info[i].bias_en_reg) {
			ret = regmap_read(map, bias_info[i].bias_en_reg, &val);
			if (ret < 0)
				return ret;
			else {
				/*
				 * the bias is disabled only if:
				 * GP_BIAS_EN = 1 && GP_BIAS_OUT = 0
				 * thus, !GP_BIAS_EN || GP_BIAS_OUT
				 */
				val = (!(val & en_mask) || (val & out_mask));
			}
			if (val) {
				ret = regmap_read(map, bias_info[i].bias_reg, &val);
				if (ret < 0)
					return ret;
				else {
					val = ((val & mask) >> (ffs(mask) - 1)) * 5 + 1;
					*len += sprintf(buf + *len, "   %-7d |", val);
					*len += sprintf(buf + *len, "   %-7d |", volt / val);
					flag = 1;
				}
			} else {
				*len += sprintf(buf + *len, "%-7s |", "Disable");
				*len += sprintf(buf + *len, "%-7s |", "-");
				flag = 1;
			}
		}
	}
	if (!flag) {
		*len += sprintf(buf + *len, "   %-7s |", "-");
		*len += sprintf(buf + *len, "   %-7s |", "-");
		flag = 0;
	}
	return val;
}

static int pm88x_gpadc_print(struct regmap *map, struct iio_dev *iio,
			     const struct iio_chan_spec *channel,
			     struct pm88x_gpadc_extra_info *info,
			     char *buf, int *len, int is_gpadc)
{
	int i, volt, mea_num = 0;
	int ret[5];
	unsigned int val[5] = {0};

	if (is_gpadc)
		mea_num = 4;
	else
		mea_num = 5;

	ret[0] = pm88x_gpadc_read_raw(iio, channel, &val[0], NULL,
				      ffs(channel->info_mask_separate) - 1);
	if (ret[0] > 0)
		ret[0] = 0;
	volt = val[0];
	if (channel->info_mask_separate == BIT(IIO_CHAN_INFO_PROCESSED))
		val[0] = val[0] / 1000;

	if (info->avg_reg)
		ret[1]  = pm88x_gpadc_get_meas(map, info->avg_reg, &val[1]);
	else
		ret[1] = 1;
	if (info->min_reg)
		ret[2]  = pm88x_gpadc_get_meas(map, info->min_reg, &val[2]);
	else
		ret[2] = 1;
	if (info->max_reg)
		ret[3]  = pm88x_gpadc_get_meas(map, info->max_reg, &val[3]);
	else
		ret[3] = 1;
	if (!is_gpadc) {
		if (info->slp_reg)
			ret[4]  = pm88x_gpadc_get_meas(map, info->slp_reg, &val[4]);
		else
			ret[4] = 1;
	}

	for (i = 0; i < mea_num; i++) {
		if (ret[i] < 0)
			*len += sprintf(buf + *len, "ERR 0x%-5x|", val[i]);
		else if (ret[i] > 0)
			*len += sprintf(buf + *len, "   %-7s |", "-");
		else {
			if (i > 0 && channel->info_mask_separate == BIT(IIO_CHAN_INFO_PROCESSED))
				val[i] = val[i] * channel->address / 1000;
			*len += sprintf(buf + *len, "  %-7d  |", val[i]);
		}
	}

	if (is_gpadc)
		pm88x_gpadc_print_bias_res(map, info, buf, len, volt);
	*len += sprintf(buf + *len, "\n");
	return *len;
}

int pm88x_display_gpadc(struct pm88x_chip *chip, char *buf)
{
	int gpadc_num, other_num, channels_num, i, j, len = 0;
	ssize_t ret = 0;
	struct regmap *map = chip->gpadc_regmap;
	struct pm88x_gpadc_extra_info *extra_info_gpadc = pm88x_extra_gpadc;
	struct pm88x_gpadc_extra_info *extra_info_other = pm88x_extra_other;
	const struct iio_chan_spec *channels = pm88x_gpadc_channels;

	gpadc_num = ARRAY_SIZE(pm88x_extra_gpadc);
	other_num = ARRAY_SIZE(pm88x_extra_other);
	channels_num = ARRAY_SIZE(pm88x_gpadc_channels);

	len += sprintf(buf + len, "\nGPADC");

	len += sprintf(buf + len, "\n-------------------------------------------");
	len += sprintf(buf + len, "--------------------------------------------\n");
	len += sprintf(buf + len, "|      name     | status  |   value   |    avg    ");
	len += sprintf(buf + len, "|    min    |    max    |    slp    |");
	len += sprintf(buf + len, "\n-------------------------------------------");
	len += sprintf(buf + len, "--------------------------------------------\n");

	for (i = 0; i < other_num; i++) {
		len += sprintf(buf + len, "|%-10s %-4s|", extra_info_other[i].name,
			       extra_info_other[i].unit);
		if (extra_info_other[i].enable_reg) {
			ret = pm800_gpadc_check_en(map, extra_info_other[i].enable_reg,
						   extra_info_other[i].enable_mask);
			if (ret < 0)
				len += sprintf(buf + len, "ERR 0x%-2x |",
					       extra_info_other[i].enable_reg);
			else
				len += sprintf(buf + len, " %-7s |", PM88X_GPADC_CHECK_EN(ret));
		} else
				len += sprintf(buf + len, " %-7s |", "-");
		if (!ret && extra_info_other[i].enable_reg) {
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |\n", "-");
		} else {
			for (j = 0; j < channels_num; j++) {
				if (!strcmp(extra_info_other[i].name, channels[j].datasheet_name))
					pm88x_gpadc_print(map, g_iio, &channels[j],
							  &extra_info_other[i], buf, &len, 0);
			}
		}
	}

	len += sprintf(buf + len, "--------------------------------------------");
	len += sprintf(buf + len, "-------------------------------------------\n");
	len += sprintf(buf + len, "\n-----------------------------------------------");
	len += sprintf(buf + len, "-------------------------------------------------\n");
	len += sprintf(buf + len, "|    name    | status  | value(mV) |  avg(mV)  |  min(mV)  ");
	len += sprintf(buf + len, "|  max(mV)  |  bias(uA) | res(Ohm)  |");
	len += sprintf(buf + len, "\n-----------------------------------------------");
	len += sprintf(buf + len, "-------------------------------------------------\n");

	for (i = 0; i < gpadc_num; i++) {
		len += sprintf(buf + len, "| %-10s |", extra_info_gpadc[i].name);
		if (extra_info_gpadc[i].enable_reg) {
			ret = pm800_gpadc_check_en(map, extra_info_gpadc[i].enable_reg,
						   extra_info_gpadc[i].enable_mask);
			if (ret < 0)
				len += sprintf(buf + len, "ERR 0x%-2x |",
					       extra_info_gpadc[i].enable_reg);
			else
				len += sprintf(buf + len, " %-7s |", PM88X_GPADC_CHECK_EN(ret));
		} else
				len += sprintf(buf + len, " %-7s |", "-");
		if (!ret && extra_info_gpadc[i].enable_reg) {
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |", "-");
			len += sprintf(buf + len, "   %-7s |\n", "-");
		} else {
			for (j = 0; j < channels_num; j++) {
				if (!strcmp(extra_info_gpadc[i].name, channels[j].datasheet_name))
					pm88x_gpadc_print(map, g_iio, &channels[j],
							  &extra_info_gpadc[i], buf, &len, 1);
			}
		}
	}

	len += sprintf(buf + len, "------------------------------------------------");
	len += sprintf(buf + len, "------------------------------------------------\n");

	return len;
}

static int pm88x_gpadc_probe(struct platform_device *pdev)
{
	struct iio_dev *iio;
	struct device *dev = &pdev->dev;
	struct pm88x_gpadc_info *gpadc;
	const struct of_device_id *match;
	int err;

	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);

	match = of_match_device(of_pm88x_gpadc_match, dev);
	if (!match)
		return -EINVAL;

	iio = devm_iio_device_alloc(dev, sizeof(*gpadc));
	if (!iio)
		return -ENOMEM;

	gpadc = iio_priv(iio);
	gpadc->chip = chip;
	gpadc->channel_to_reg = pm88x_channel_to_reg;
	gpadc->channel_to_gpadc_num = pm88x_channel_to_gpadc_num;

	mutex_init(&gpadc->lock);

	iio->dev.of_node = pdev->dev.of_node;
	err = pm88x_iio_map_register(iio, gpadc);
	if (err)
		return err;

	iio->name = "88pm88x-gpadc";
	iio->dev.parent = dev;
	iio->info = &pm88x_gpadc_iio_info;
	iio->modes = INDIO_DIRECT_MODE;
	iio->channels = pm88x_gpadc_channels;
	iio->num_channels = ARRAY_SIZE(pm88x_gpadc_channels);

	err = pm88x_gpadc_setup(gpadc);
	if (err < 0)
		return err;

	err = iio_device_register(iio);
	if (err < 0) {
		dev_err(&pdev->dev, "iio dev register err: %d\n", err);
		return err;
	}
	dev_info(&iio->dev, "%s is successful to be probed.\n", __func__);

	g_gpadc = gpadc;
	g_iio = iio;
	return 0;
}

static int pm88x_gpadc_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	iio_device_unregister(iio);

	return 0;
}

int extern_pm88x_gpadc_set_current_generator(int gpadc_number, int on)
{
	return pm88x_gpadc_set_current_generator(g_gpadc, gpadc_number, on);
}
EXPORT_SYMBOL(extern_pm88x_gpadc_set_current_generator);

int extern_pm88x_gpadc_get_volt(int gpadc_number, int *volt)
{
	int channel;
	switch (gpadc_number) {
	case GPADC_0_RES:
		channel = GPADC0_RES_CHAN;
		break;
	case GPADC_1_RES:
		channel = GPADC1_RES_CHAN;
		break;
	case GPADC_2_RES:
		channel = GPADC2_RES_CHAN;
		break;
	case GPADC_3_RES:
		channel = GPADC3_RES_CHAN;
		break;
	default:
		pr_err("%s: unsupported gpadc!\n", __func__);
		return -EINVAL;
	}

	return pm88x_gpadc_get_processed(g_gpadc, channel, volt);
}
EXPORT_SYMBOL(extern_pm88x_gpadc_get_volt);

int extern_pm88x_gpadc_set_bias_current(int gpadc_number, int bias)
{
	return pm88x_gpadc_set_bias_current(g_gpadc, gpadc_number, bias);
}
EXPORT_SYMBOL(extern_pm88x_gpadc_set_bias_current);

static struct platform_driver pm88x_gpadc_driver = {
	.driver = {
		.name = "88pm88x-gpadc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_pm88x_gpadc_match),
	},
	.probe = pm88x_gpadc_probe,
	.remove = pm88x_gpadc_remove,
};

static int pm88x_gpadc_init(void)
{
	return platform_driver_register(&pm88x_gpadc_driver);
}
module_init(pm88x_gpadc_init);

static void pm88x_gpadc_exit(void)
{
	platform_driver_unregister(&pm88x_gpadc_driver);
}
module_exit(pm88x_gpadc_exit);

MODULE_AUTHOR("Yi Zhang <yizhang@marvell.com>");
MODULE_DESCRIPTION("Marvell 88PM88x GPADC driver");
MODULE_LICENSE("GPL v2");
