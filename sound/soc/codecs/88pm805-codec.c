/*
 * 88pm805-codec.c -- 88PM805 ALSA SoC Audio Driver
 *
 * Copyright 2014 Marvell International Ltd.
 * Author: Xiaofan Tian <tianxf@marvell.com>
 * Update: Zhao Ye<zhaoy@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm80x.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <sound/jack.h>
#include <trace/events/asoc.h>

#include "88pm805-codec.h"

#ifdef CONFIG_DUAL_SPEAKER
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <plat/mfp.h>

static int gpio_speaker_switch_port;
#endif


struct pm805_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	struct pm80x_chip *chip;
	int irq_micin;
	int irq_audio_short1;
	int irq_audio_short2;
};

#ifdef CONFIG_DUAL_SPEAKER
static int gpio_speaker_switch_select;

static ssize_t gpio_speaker_switch_select_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gpio_speaker_switch_select);
}

static ssize_t gpio_speaker_switch_select_set(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint((const char *)buf, count, &gpio_speaker_switch_select);
	if (ret)
		return ret;

	if (gpio_speaker_switch_port > 0) {
		if (gpio_speaker_switch_select == 1) {
			gpio_direction_output(gpio_speaker_switch_port, 1);
		} else if (gpio_speaker_switch_select == 0) {
			/* pull low  to un-mute */
			gpio_direction_output(gpio_speaker_switch_port, 0);
		}
	}

	return count;
}

static DEVICE_ATTR(gpio_speaker_switch_select, 0666,
	gpio_speaker_switch_select_show, gpio_speaker_switch_select_set);

static void create_gpio_file(struct platform_device *pdev)
{
	int ret;

	/* add gpio_speaker_switch_select sysfs entries */
	ret = device_create_file(&pdev->dev,
		&dev_attr_gpio_speaker_switch_select);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s: failed to add gpio_speaker_switch_select sysfs files: %d\n",
			__func__, ret);
		return;
	}

	gpio_speaker_switch_port = mfp_to_gpio(MFP_PIN_GPIO19);

	if (gpio_speaker_switch_port) {
		if (gpio_request(gpio_speaker_switch_port, "mute or unmute")) {
			gpio_speaker_switch_port = 0;
		} else /* pull low to un-mute */
			gpio_direction_output(gpio_speaker_switch_port, 0);
	}
}

#endif

static struct pm80x_chip *pm80x_get_companion(struct pm80x_chip *chip)
{
	struct pm80x_chip *chip_comp;

	if (!chip->companion) {
		dev_err(chip->dev, "%s: no companion chip\n", __func__);
		return NULL;
	}

	chip_comp = i2c_get_clientdata(chip->companion);

	return chip_comp;
}

static unsigned int pm805_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct pm80x_chip *chip =
		(struct pm80x_chip *)codec->control_data, *companion;
	struct regmap *map;
	unsigned int val;
	int ret;

	map = chip->regmap;

	if (reg >= PMIC_INDEX) {
		companion = pm80x_get_companion(chip);
		if (!companion)
			return -EINVAL;

		map = companion->regmap;

		if (companion->type == CHIP_PM822) {
			if (reg == PM822_CLASS_D_1)
				reg = PM822_CLASS_D_REG_BASE;
			else if (reg == PM822_MIS_CLASS_D_1)
				reg = PM822_MIS_CLASS_D_REG_1;
			else if (reg == PM822_MIS_CLASS_D_2)
				reg = PM822_MIS_CLASS_D_REG_2;
		} else
			reg = reg - PMIC_INDEX + PM800_CLASS_D_REG_BASE;
	}

	ret = regmap_read(map, reg, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read reg 0x%x: %d\n", reg, ret);
		return ret;
	}

	return val;
}

static int pm805_write(struct snd_soc_codec *codec,
		       unsigned int reg, unsigned int value)
{
	struct pm80x_chip *chip =
		(struct pm80x_chip *)codec->control_data, *companion;
	struct regmap *map, *map_comp;
	int ret;

	value &= 0xff;

	dev_dbg(chip->dev, "write reg 0x%x, value 0x%x\n", reg, value);

	map = chip->regmap;

	/* Enable pm800 audio mode */
	companion = pm80x_get_companion(chip);
	map_comp = companion->regmap;

	if (map_comp) {
		if (companion->type == CHIP_PM822) {
			if (reg >= PMIC_INDEX) {
				if (reg == PM822_CLASS_D_1)
					reg = PM822_CLASS_D_REG_BASE;
				else if (reg == PM822_MIS_CLASS_D_1)
					reg = PM822_MIS_CLASS_D_REG_1;
				else if (reg == PM822_MIS_CLASS_D_2)
					reg = PM822_MIS_CLASS_D_REG_2;

				map = map_comp;
			}
		} else {
			if (reg == PM805_CODEC_MAIN_POWERUP) {
				if (value & PM805_STBY_B)
					regmap_update_bits(map_comp,
							   PM800_LOW_POWER2,
							   PM800_AUDIO_MODE_EN,
							   PM800_AUDIO_MODE_EN);
				else
					regmap_update_bits(map_comp,
							   PM800_LOW_POWER2,
							   PM800_AUDIO_MODE_EN,
							   0);
				usleep_range(1000, 1200);
			} else if (reg >= PMIC_INDEX) {
				reg =
				    reg - PMIC_INDEX + PM800_CLASS_D_REG_BASE;
				map = map_comp;

			}
		}

	}

	ret = regmap_write(map, reg, value);
	if (ret < 0)
		dev_err(chip->dev, "Failed to write reg 0x%x: %d\n", reg, ret);

	return ret;
}

static int pm805_bulk_read_reg(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned char buf[PM805_MIXER_COEFFICIENT_MAX_NUM];
	int i, count = 0;
	struct pm80x_chip *chip = (struct pm80x_chip *)codec->control_data;

	count = (ucontrol->value.integer.value[0] & 0xff);

	if (count < 1 || count >= PM805_MIXER_COEFFICIENT_MAX_NUM)
		return -EINVAL;

	dev_dbg(chip->dev, "%s: 0x%x, count %d\n", __func__, reg, count);

	regmap_write(chip->regmap, reg, ucontrol->value.integer.value[1]);
	i2c_master_recv(chip->client, buf, count);
	for (i = 0; i < count; i++) {
		(ucontrol->value.integer.value[i]) = buf[i];
		dev_dbg(chip->dev, "read value 0x%x\n", buf[i]);
	}

	return 0;
}

static int pm805_bulk_write_reg(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned char buf[PM805_MIXER_COEFFICIENT_MAX_NUM];
	int i, count = 0;
	struct pm80x_chip *chip = (struct pm80x_chip *)codec->control_data;

	count = (ucontrol->value.integer.value[0] & 0xff);

	if (count < 1 || count > PM805_MIXER_COEFFICIENT_MAX_NUM) {
		dev_err(chip->dev, "error count %d, must between 1~32\n", count);
		return -EINVAL;
	}

	dev_dbg(chip->dev, "%s: 0x%x, count %d\n", __func__, reg, count);

	for (i = 0; i < count; i++) {
		buf[i] = (ucontrol->value.integer.value[i + 1]);
		dev_dbg(chip->dev, "value 0x%x\n", buf[i]);
	}

	return regmap_raw_write(chip->regmap, reg, buf, count);
}

static int pm805_info_volsw(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int platform_max;

	if (!mc->platform_max)
		mc->platform_max = mc->max;
	platform_max = mc->platform_max;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = PM805_MIXER_COEFFICIENT_MAX_NUM + 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = platform_max;
	return 0;
}

#define SOC_SINGLE_INFO(xname, xreg, xshift, xmax, xinvert,\
	 xhandler_get, xhandler_put, xhandler_info) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = xhandler_info, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = SOC_SINGLE_VALUE(xreg, xshift, xmax, xinvert, 0) }

static const struct snd_kcontrol_new pm805_audio_controls[] = {
	/* Main Section */
	SOC_SINGLE("PM805_CODEC_ID", PM805_CODEC_ID, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_MAIN_POWERUP", PM805_CODEC_MAIN_POWERUP, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_INT_MANAGEMENT", PM805_CODEC_INT_MANAGEMENT, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_INT_1", PM805_CODEC_INT_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_INT_2", PM805_CODEC_INT_2, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_INT_MASK_1", PM805_CODEC_INT_MASK_1, 0, 0xff,
		   0),
	SOC_SINGLE("PM805_CODEC_INT_MASK_2", PM805_CODEC_INT_MASK_2, 0, 0xff,
		   0),
	SOC_SINGLE("PM805_CODEC_MIC_DETECT_1", PM805_CODEC_MIC_DETECT_1, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_MIC_DETECT_2", PM805_CODEC_MIC_DETECT_2, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_MIC_DETECT_STS", PM805_CODEC_MIC_DETECT_STS, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_MIC_DETECT_3", PM805_CODEC_MIC_DETECT_3, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_AUTO_SEQUENCE_STS_1",
		   PM805_CODEC_AUTO_SEQUENCE_STS_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_AUTO_SEQUENCE_STS_2",
		   PM805_CODEC_AUTO_SEQUENCE_STS_2, 0, 0xff, 0),

	/* ADC/DMIC Section */
	SOC_SINGLE("PM805_CODEC_ADCS_SETTING_1", PM805_CODEC_ADCS_SETTING_1, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_ADCS_SETTING_2", PM805_CODEC_ADCS_SETTING_2, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_ADCS_SETTING_3", PM805_CODEC_ADCS_SETTING_3, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_ADC_GAIN_1", PM805_CODEC_ADC_GAIN_1, 0, 0xff,
		   0),
	SOC_SINGLE("PM805_CODEC_ADC_GAIN_2", PM805_CODEC_ADC_GAIN_2, 0, 0xff,
		   0),
	SOC_SINGLE("PM805_CODEC_DMIC_SETTING", PM805_CODEC_DMIC_SETTING, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_DWS_SETTING", PM805_CODEC_DWS_SETTING, 0, 0xff,
		   0),
	SOC_SINGLE("PM805_CODEC_MIC_CONFLICT_STS", PM805_CODEC_MIC_CONFLICT_STS,
		   0, 0xff, 0),

	/* DAC/PDM Section */
	SOC_SINGLE("PM805_CODEC_PDM_SETTING_1", PM805_CODEC_PDM_SETTING_1, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_PDM_SETTING_2", PM805_CODEC_PDM_SETTING_2, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_PDM_SETTING_3", PM805_CODEC_PDM_SETTING_3, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_PDM_CONTROL_1", PM805_CODEC_PDM_CONTROL_1, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_PDM_CONTROL_2", PM805_CODEC_PDM_CONTROL_2, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_PDM_CONTROL_3", PM805_CODEC_PDM_CONTROL_3, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_HEADPHONE_SETTING",
		   PM805_CODEC_HEADPHONE_SETTING, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_HEADPHONE_GAIN_A2A",
		   PM805_CODEC_HEADPHONE_GAIN_A2A, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_HEADPHONE_SHORT_STS",
		   PM805_CODEC_HEADPHONE_SHORT_STS, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_EARPHONE_SETTING", PM805_CODEC_EARPHONE_SETTING,
		   0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_AUTO_SEQUENCE_SETTING",
		   PM805_CODEC_AUTO_SEQUENCE_SETTING, 0, 0xff, 0),

	/* SAI/SRC Section */
	SOC_SINGLE("PM805_CODEC_SAI1_SETTING_1", PM805_CODEC_SAI1_SETTING_1, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI1_SETTING_2", PM805_CODEC_SAI1_SETTING_2, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI1_SETTING_3", PM805_CODEC_SAI1_SETTING_3, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI1_SETTING_4", PM805_CODEC_SAI1_SETTING_4, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI1_SETTING_5", PM805_CODEC_SAI1_SETTING_5, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI2_SETTING_1", PM805_CODEC_SAI2_SETTING_1, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI2_SETTING_2", PM805_CODEC_SAI2_SETTING_2, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI2_SETTING_3", PM805_CODEC_SAI2_SETTING_3, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI2_SETTING_4", PM805_CODEC_SAI2_SETTING_4, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SAI2_SETTING_5", PM805_CODEC_SAI2_SETTING_5, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SRC_DPLL_LOCK", PM805_CODEC_SRC_DPLL_LOCK, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SRC_SETTING_1", PM805_CODEC_SRC_SETTING_1, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SRC_SETTING_2", PM805_CODEC_SRC_SETTING_2, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SRC_SETTING_3", PM805_CODEC_SRC_SETTING_3, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_SIDETONE_SETTING", PM805_CODEC_SIDETONE_SETTING,
		   0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_SIDETONE_COEFFICIENT_1",
		   PM805_CODEC_SIDETONE_COEFFICIENT_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_SIDETONE_COEFFICIENT_2",
		   PM805_CODEC_SIDETONE_COEFFICIENT_2, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_SIDETONE_COEFFICIENT_3",
		   PM805_CODEC_SIDETONE_COEFFICIENT_3, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_SIDETONE_COEFFICIENT_4",
		   PM805_CODEC_SIDETONE_COEFFICIENT_4, 0, 0xff, 0),

	/* DIG/PROC Section */
	SOC_SINGLE("PM805_CODEC_DIGITAL_BLOCK_EN_1",
		   PM805_CODEC_DIGITAL_BLOCK_EN_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_DIGITAL_BLOCK_EN_2",
		   PM805_CODEC_DIGITAL_BLOCK_EN_2, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_VOL_CHANNEL_1_2_SEL",
		   PM805_CODEC_VOL_CHANNEL_1_2_SEL, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_VOL_CHANNLE_3_4_SEL",
		   PM805_CODEC_VOL_CHANNLE_3_4_SEL, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_ZERO_CROSS_AUTOMUTE",
		   PM805_CODEC_ZERO_CROSS_AUTOMUTE, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_VOL_CTRL_PARAM_SEL",
		   PM805_CODEC_VOL_CTRL_PARAM_SEL, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_VOL_SEL_CHANNEL_1",
		   PM805_CODEC_VOL_SEL_CHANNEL_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_VOL_SEL_CHANNEL_2",
		   PM805_CODEC_VOL_SEL_CHANNEL_2, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_VOL_SEL_CHANNEL_3",
		   PM805_CODEC_VOL_SEL_CHANNEL_3, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_VOL_SEL_CHANNEL_4",
		   PM805_CODEC_VOL_SEL_CHANNEL_4, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_MIX_EQ_COEFFICIENT_1",
		   PM805_CODEC_MIX_EQ_COEFFICIENT_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_MIX_EQ_COEFFICIENT_2",
		   PM805_CODEC_MIX_EQ_COEFFICIENT_2, 0, 0xff, 0),
	SOC_SINGLE_INFO("PM805_CODEC_MIX_EQ_COEFFICIENT_3",
			PM805_CODEC_MIX_EQ_COEFFICIENT_3, 0, 0xff, 0,
			pm805_bulk_read_reg, pm805_bulk_write_reg,
			pm805_info_volsw),
	SOC_SINGLE("PM805_CODEC_MIX_EQ_COEFFICIENT_4",
		   PM805_CODEC_MIX_EQ_COEFFICIENT_4, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_CLIP_BITS_1", PM805_CODEC_CLIP_BITS_1, 0, 0xff,
		   0),
	SOC_SINGLE("PM805_CODEC_CLIP_BITS_2", PM805_CODEC_CLIP_BITS_2, 0, 0xff,
		   0),
	SOC_SINGLE("PM805_CODEC_CLIP_BITS_3", PM805_CODEC_CLIP_BITS_3, 0, 0xff,
		   0),

	/* Advanced Settings Section */
	SOC_SINGLE("PM805_CODEC_ANALOG_BLOCK_EN", PM805_CODEC_ANALOG_BLOCK_EN,
		   0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_ANALOG_BLOCK_STS_1",
		   PM805_CODEC_ANALOG_BLOCK_STS_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_PAD_ANALOG_SETTING",
		   PM805_CODEC_PAD_ANALOG_SETTING, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_ANALOG_BLOCK_STS_2",
		   PM805_CODEC_ANALOG_BLOCK_STS_2, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_CHARGE_PUMP_SETTING_1",
		   PM805_CODEC_CHARGE_PUMP_SETTING_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_CHARGE_PUMP_SETTING_2",
		   PM805_CODEC_CHARGE_PUMP_SETTING_2, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_CHARGE_PUMP_SETTING_3",
		   PM805_CODEC_CHARGE_PUMP_SETTING_3, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_CLOCK_SETTING", PM805_CODEC_CLOCK_SETTING, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_HEADPHONE_AMP_SETTING",
		   PM805_CODEC_HEADPHONE_AMP_SETTING, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_POWER_AMP_ENABLE", PM805_CODEC_POWER_AMP_ENABLE,
		   0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_HEAD_EAR_PHONE_SETTING",
		   PM805_CODEC_HEAD_EAR_PHONE_SETTING, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_RECONSTRUCTION_FILTER_1",
		   PM805_CODEC_RECONSTRUCTION_FILTER_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_RECONSTRUCTION_FILTER_2",
		   PM805_CODEC_RECONSTRUCTION_FILTER_2, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_RECONSTRUCTION_FILTER_3",
		   PM805_CODEC_RECONSTRUCTION_FILTER_3, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_DWA_SETTING", PM805_CODEC_DWA_SETTING, 0, 0xff,
		   0),
	SOC_SINGLE("PM805_CODEC_SDM_VOL_DELAY", PM805_CODEC_SDM_VOL_DELAY, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_REF_GROUP_SETTING_1",
		   PM805_CODEC_REF_GROUP_SETTING_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_ADC_SETTING_1", PM805_CODEC_ADC_SETTING_1, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_ADC_SETTING_2", PM805_CODEC_ADC_SETTING_2, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_ADC_SETTING_3", PM805_CODEC_ADC_SETTING_3, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_ADC_SETTING_4", PM805_CODEC_ADC_SETTING_4, 0,
		   0xff, 0),
	SOC_SINGLE("PM805_CODEC_FLL_SPREAD_SPECTRUM_1",
		   PM805_CODEC_FLL_SPREAD_SPECTRUM_1, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_FLL_SPREAD_SPECTRUM_2",
		   PM805_CODEC_FLL_SPREAD_SPECTRUM_2, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_FLL_SPREAD_SPECTRUM_3",
		   PM805_CODEC_FLL_SPREAD_SPECTRUM_3, 0, 0xff, 0),
	SOC_SINGLE("PM805_CODEC_FLL_STS", PM805_CODEC_FLL_STS, 0, 0xff, 0),
};

static const struct snd_kcontrol_new pm822_audio_controls[] = {
	SOC_SINGLE("PM822_CLASS_D_1", PM822_CLASS_D_1, 0, 0xff, 0),
	SOC_SINGLE("PM822_MIS_CLASS_D_1", PM822_MIS_CLASS_D_1, 0, 0xff, 0),
	SOC_SINGLE("PM822_MIS_CLASS_D_2", PM822_MIS_CLASS_D_2, 0, 0xff, 0),
};

static const struct snd_kcontrol_new pm800_audio_controls[] = {
	SOC_SINGLE("PM800_CLASS_D_1", PM800_CLASS_D_1, 0, 0xff, 0),
	SOC_SINGLE("PM800_CLASS_D_2", PM800_CLASS_D_2, 0, 0xff, 0),
	SOC_SINGLE("PM800_CLASS_D_3", PM800_CLASS_D_3, 0, 0xff, 0),
	SOC_SINGLE("PM800_CLASS_D_4", PM800_CLASS_D_4, 0, 0xff, 0),
	SOC_SINGLE("PM800_CLASS_D_5", PM800_CLASS_D_5, 0, 0xff, 0),
};

/*
 * Use MUTE_LEFT & MUTE_RIGHT to implement digital mute.
 * These bits can also be used to mute.
 */
static int pm805_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	return 0;
}

static irqreturn_t pm805_short_handler(int irq, void *data)
{
	struct pm805_priv *pm805 = data;
	struct pm80x_chip *chip = pm805->chip;
	struct regmap *map = pm805->regmap;
	unsigned int val, short_count = 20;

	if ((irq == pm805->irq_audio_short1)
	    || (irq == pm805->irq_audio_short2)) {
		regmap_read(map, PM805_CODEC_HEADPHONE_SHORT_STS, &val);
		val &= 0x3;

		while (short_count-- && val) {
			/* clear short status */
			regmap_write(map,
				PM805_CODEC_HEADPHONE_SHORT_STS, 0);

			msleep(500);
			regmap_read(map,
				PM805_CODEC_HEADPHONE_SHORT_STS, &val);
			val &= 0x3;
		}

		if (val) {
			regmap_read(map,
				PM805_CODEC_HEADPHONE_AMP_SETTING, &val);
			val &= (~0x24);
			regmap_write(map,
				PM805_CODEC_HEADPHONE_AMP_SETTING, val);
			dev_err(chip->dev,
				"permanent short!!! disable headset amp\n");
		}
	}

	return IRQ_HANDLED;
}

static int pm805_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned char inf, addr;

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		inf = PM805_WLEN_8_BIT;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		inf = PM805_WLEN_16_BIT;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		inf = PM805_WLEN_20_BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		inf = PM805_WLEN_24_BIT;
		break;
	default:
		return -EINVAL;
	}

	addr = PM805_CODEC_SAI1_SETTING_1 + (dai->id - 1) * 0x5;
	snd_soc_update_bits_locked(codec, addr, PM805_WLEN_24_BIT, inf);

	/* sample rate */
	switch (params_rate(params)) {
	case 8000:
		inf = PM805_FSYN_RATE_8000;
		break;
	case 11025:
		inf = PM805_FSYN_RATE_11025;
		break;
	case 16000:
		inf = PM805_FSYN_RATE_16000;
		break;
	case 22050:
		inf = PM805_FSYN_RATE_22050;
		break;
	case 32000:
		inf = PM805_FSYN_RATE_32000;
		break;
	case 44100:
		inf = PM805_FSYN_RATE_44100;
		break;
	case 48000:
		inf = PM805_FSYN_RATE_48000;
		break;
	default:
		return -EINVAL;
	}
	addr = PM805_CODEC_SAI1_SETTING_2 + (dai->id - 1) * 0x5;
	snd_soc_update_bits_locked(codec, addr, PM805_FSYN_RATE_128000, inf);

	return 0;
}

static int pm805_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	unsigned char inf = 0, mask = 0, addr;

	addr = PM805_CODEC_SAI1_SETTING_1 + (codec_dai->id - 1) * 0x5;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		inf |= PM805_SAI_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		inf &= ~PM805_SAI_MASTER;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		inf |= PM805_SAI_I2S_MODE;
		break;
	default:
		inf &= ~PM805_SAI_I2S_MODE;
		break;
	}
	mask |= PM805_SAI_MASTER | PM805_SAI_I2S_MODE;
	snd_soc_update_bits_locked(codec, addr, mask, inf);
	return 0;
}

static int pm805_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	codec->dapm.bias_level = level;
	return 0;
}

static int pm805_set_dai_sysclk(struct snd_soc_dai *dai,
				int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static struct snd_soc_dai_ops pm805_dai_ops = {
	.digital_mute = pm805_digital_mute,
	.hw_params = pm805_hw_params,
	.set_fmt = pm805_set_dai_fmt,
	.set_sysclk = pm805_set_dai_sysclk,
};

static struct snd_soc_dai_driver pm805_dai[] = {
	{
		/* DAI I2S(SAI1) */
		.name	= "88pm805-i2s",
		.id	= 1,
		.playback = {
			.stream_name	= "I2S Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S16_LE |
					  SNDRV_PCM_FORMAT_S18_3LE,
		},
		.capture = {
			.stream_name	= "I2S Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S16_LE |
					  SNDRV_PCM_FORMAT_S18_3LE,
		},
		.ops	= &pm805_dai_ops,
	}, {
		/* DAI PCM(SAI2) */
		.name	= "88pm805-pcm",
		.id	= 2,
		.playback = {
			.stream_name	= "PCM Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S8 |
					  SNDRV_PCM_FORMAT_S16_LE |
					  SNDRV_PCM_FORMAT_S20_3LE |
					  SNDRV_PCM_FORMAT_S24,
		},
		.capture = {
			.stream_name	= "PCM Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S8 |
					  SNDRV_PCM_FORMAT_S16_LE |
					  SNDRV_PCM_FORMAT_S20_3LE |
					  SNDRV_PCM_FORMAT_S24,
		},
		.ops	= &pm805_dai_ops,
	}, {
		/* dummy (SAI3) */
		.name	= "88pm805-dummy",
		.id	= 3,
		.playback = {
			.stream_name	= "PCM dummy Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S8 |
					  SNDRV_PCM_FORMAT_S16_LE |
					  SNDRV_PCM_FORMAT_S20_3LE |
					  SNDRV_PCM_FORMAT_S24 |
					  SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name	= "PCM dummy Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S8 |
					  SNDRV_PCM_FORMAT_S16_LE |
					  SNDRV_PCM_FORMAT_S20_3LE |
					  SNDRV_PCM_FORMAT_S24 |
					  SNDRV_PCM_FMTBIT_S32_LE,
		},
	},
};

static int pm805_probe(struct snd_soc_codec *codec)
{
	struct pm805_priv *pm805 = snd_soc_codec_get_drvdata(codec);
	struct pm80x_chip *chip = pm805->chip, *companion;

	pm805->codec = codec;
	codec->control_data = chip;

	companion = pm80x_get_companion(chip);

	/* add below snd ctls to keep align with audio server */
	snd_soc_add_codec_controls(codec, pm805_audio_controls,
			     ARRAY_SIZE(pm805_audio_controls));

	if (companion->type == CHIP_PM822)
		snd_soc_add_codec_controls(codec, pm822_audio_controls,
			     ARRAY_SIZE(pm822_audio_controls));
	else
		snd_soc_add_codec_controls(codec, pm800_audio_controls,
			     ARRAY_SIZE(pm800_audio_controls));
	return 0;
}

static int pm805_remove(struct snd_soc_codec *codec)
{
	pm805_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_pm805 = {
	.probe =		pm805_probe,
	.remove =		pm805_remove,
	.read =			pm805_read,
	.write =		pm805_write,
	.reg_cache_size =	CODEC_TOTAL_REG_SIZE,
	.reg_word_size =	sizeof(u8),
	.set_bias_level =	pm805_set_bias_level,
};

static int pm805_codec_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm805_priv *pm805;
	int ret, irq_short1, irq_short2;

	pm805 = devm_kzalloc(&pdev->dev, sizeof(struct pm805_priv), GFP_KERNEL);
	if (pm805 == NULL)
		return -ENOMEM;

	pm805->chip = chip;
	pm805->regmap = chip->regmap;

#ifdef CONFIG_DUAL_SPEAKER
	create_gpio_file(pdev);
#endif

	/* refer to drivers/mfd/88pm805.c for irq resource */
	irq_short1 = platform_get_irq(pdev, 1);
	if (irq_short1 < 0)
		/* if do not get irq num, we do not return to stop the probe.
		the short irq is use to error detect in codec, if this
		function do not work, it will not affect the other function */
		dev_dbg(&pdev->dev, "No IRQ resource for audio short 1!\n");
	else {
		pm805->irq_audio_short1 = irq_short1;
		ret = devm_request_threaded_irq(&pdev->dev,
				pm805->irq_audio_short1, NULL,
				pm805_short_handler, IRQF_ONESHOT,
				"audio_short1", pm805);
		if (ret < 0)
			dev_dbg(&pdev->dev, "Failed to request IRQ: #%d: %d\n",
				pm805->irq_audio_short1, ret);
	}

	irq_short2 = platform_get_irq(pdev, 2);
	if (irq_short2 < 0)
		/* the same as irq_short1, we do not return, if we do not get
		irq_short2 */
		dev_err(&pdev->dev, "No IRQ resource for audio short 2!\n");
	else {
		pm805->irq_audio_short2 = irq_short2;
		ret = devm_request_threaded_irq(&pdev->dev,
				pm805->irq_audio_short2, NULL,
				pm805_short_handler, IRQF_ONESHOT,
				"audio_short2", pm805);
		if (ret < 0)
			dev_dbg(&pdev->dev, "Failed to request IRQ: #%d: %d\n",
				pm805->irq_audio_short2, ret);
	}

	platform_set_drvdata(pdev, pm805);

	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_pm805,
				     pm805_dai, ARRAY_SIZE(pm805_dai));
}

static int pm805_codec_remove(struct platform_device *pdev)
{
#ifdef CONFIG_DUAL_SPEAKER
	device_remove_file(&pdev->dev, &dev_attr_gpio_speaker_switch_select);
#endif
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver pm805_codec_driver = {
	.driver = {
		.name = "88pm80x-codec",
		.owner = THIS_MODULE,
	},
	.probe = pm805_codec_probe,
	.remove = pm805_codec_remove,
};

module_platform_driver(pm805_codec_driver);

MODULE_DESCRIPTION("ASoC 88PM805 driver");
MODULE_AUTHOR("Xiaofan Tian <tianxf@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:88pm805-codec");
