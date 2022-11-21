/*
 * 88pm805-codec.c -- 88PM805 ALSA SoC Audio Driver
 *
 * Copyright 2011 Marvell International Ltd.
 * Author: Xiaofan Tian <tianxf@marvell.com>
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
#include <mach/gpio-edge.h>
#include <mach/mfp-pxa988-aruba.h>
#include <linux/gpio.h>

struct pm805_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	struct pm80x_chip *chip;
	struct work_struct work_short;
	int irq_micin;
	int irq_audio_short1;
	int irq_audio_short2;
};

#ifdef CONFIG_MACH_HENDRIX
#define GPIO_STATUS_LEN	1
static char gpio_status[GPIO_STATUS_LEN] = "0";
static int mute_gpio;

static ssize_t gpio_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	copy_to_user(page,gpio_status,1);
	printk("gpio_status=%c\n",gpio_status[0]);
	return 1;
}

static ssize_t gpio_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char messages[256];
	int  ret;

	if (len > 255)
		len = 255;

	memset(messages, 0, sizeof(messages));

	if (!buff || copy_from_user(messages, buff, len))
		return -EFAULT;
   gpio_status[0]=messages[0];

	printk("write gpio5 value 0x%x\n", messages[0]);
	if(messages[0]==1)
		gpio_direction_output(mute_gpio, 1);	/*pull high to mute */
	else if(messages[0]==0)
		gpio_direction_output(mute_gpio, 0);	/* pull low  to un-mute */
		
	return len;
}

static void create_gpio_proc_file(void)
{
	struct proc_dir_entry *gpio_proc_file = NULL;

	gpio_proc_file = create_proc_entry("driver/gpio_mute", 0666, NULL);
	if (!gpio_proc_file) {
		pr_err("gpio proc file create failed!\n");
		return;
	}

	gpio_proc_file->read_proc = gpio_read_proc;
	gpio_proc_file->write_proc = (write_proc_t  *)gpio_write_proc;
	
	mute_gpio = mfp_to_gpio(MFP_PIN_GPIO31);
	if (!mute_gpio)  {
		printk(KERN_ERR "mfp_to_gpio  failed,"
			"gpio: mute_gpio :%d\n", mute_gpio);	
		//return;
	}
	gpio_direction_output(mute_gpio, 0);	/* pull low to un-mute */

}
#endif

static struct regmap *pm80x_get_companion(struct pm80x_chip *chip)
{
	struct pm80x_chip *chip_comp;

	if (!chip->companion) {
		pr_err("%s: no companion chip\n", __func__);
		return NULL;
	}

	chip_comp = i2c_get_clientdata(chip->companion);

	return chip_comp->regmap;
}

static unsigned int pm805_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct pm80x_chip *chip = (struct pm80x_chip *)codec->control_data;
	struct regmap *map;
	unsigned int val;
	int ret;

	BUG_ON(reg >= codec->reg_size);

	map = chip->regmap;

	if (reg >= PM800_CLASS_D_INDEX) {
		reg = reg - PM800_CLASS_D_INDEX + PM800_CLASS_D_REG_BASE;
		map = pm80x_get_companion(chip);
		if (!map)
			return -EINVAL;
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
	struct pm80x_chip *chip = (struct pm80x_chip *)codec->control_data;
	struct regmap *map, *map_comp;
	int ret;

	BUG_ON(reg >= codec->reg_size);

	value &= 0xff;

	pr_debug("write reg 0x%x, value 0x%x\n", reg, value);

	map = chip->regmap;

	/* Enable pm800 audio mode */
	map_comp = pm80x_get_companion(chip);
	if (map_comp) {
		if (reg == PM805_CODEC_MAIN_POWERUP) {
			if (value & PM805_STBY_B)
				regmap_update_bits(map_comp,
						   PM800_LOW_POWER2,
						   PM800_AUDIO_MODE_EN,
						   PM800_AUDIO_MODE_EN);
			else
				regmap_update_bits(map_comp,
						   PM800_LOW_POWER2,
						   PM800_AUDIO_MODE_EN, 0);
			msleep(1);
		} else if (reg >= PM800_CLASS_D_INDEX) {
			reg =
			    reg - PM800_CLASS_D_INDEX + PM800_CLASS_D_REG_BASE;
			map = map_comp;
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
		pr_info("error count %d, must between 1~32\n", count);
		return -EINVAL;
	}

	pr_debug("%s: 0x%x, count %d\n", __func__, reg, count);

	for (i = 0; i < count; i++) {
		buf[i] = (ucontrol->value.integer.value[i + 1]);
		pr_debug("value 0x%x\n", buf[i]);
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
	.private_value = SOC_SINGLE_VALUE(xreg, xshift, xmax, xinvert) }

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

static void pm805_short_work(struct work_struct *work)
{
	struct pm805_priv *pm805 =
	    container_of(work, struct pm805_priv, work_short);
	struct regmap *map = pm805->regmap;
	unsigned int val, short_count = 20;

	regmap_read(map, PM805_CODEC_HEADPHONE_SHORT_STS, &val);
	val &= 0x3;

	while(short_count-- && val) {
		/* clear short status */
		regmap_write(map, PM805_CODEC_HEADPHONE_SHORT_STS, 0);

		msleep(500);
		regmap_read(map, PM805_CODEC_HEADPHONE_SHORT_STS, &val);
		val &= 0x3;
	}

	if (val) {
		regmap_read(map, PM805_CODEC_HEADPHONE_AMP_SETTING, &val);
		val &= (~0x24);
		regmap_write(map, PM805_CODEC_HEADPHONE_AMP_SETTING, val);
		pr_err("permanent short!!! disable headset amp\n");
	}
}

static irqreturn_t pm805_short_handler(int irq, void *data)
{
	struct pm805_priv *pm805 = data;

	if ((irq == pm805->irq_audio_short1) || (irq == pm805->irq_audio_short2))
		queue_work(system_wq, &pm805->work_short);

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
#if 0
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
			/* Enable Audio PLL & Audio section */
			snd_soc_update_bits_locked(codec,
						PM805_CODEC_MAIN_POWERUP,
						PM805_STBY_B, PM805_STBY_B);
		break;

	case SND_SOC_BIAS_OFF:
		if (codec->dapm.bias_level != SND_SOC_BIAS_OFF)
			snd_soc_update_bits_locked(codec,
						PM805_CODEC_MAIN_POWERUP,
						PM805_STBY_B, 0);
		break;
	}
#endif
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
			.formats	= SNDRV_PCM_FORMAT_S16_LE | \
					  SNDRV_PCM_FORMAT_S18_3LE,
		},
		.capture = {
			.stream_name	= "I2S Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S16_LE | \
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
			.formats	= SNDRV_PCM_FORMAT_S8|		\
					  SNDRV_PCM_FORMAT_S16_LE |	\
					  SNDRV_PCM_FORMAT_S20_3LE |	\
					  SNDRV_PCM_FORMAT_S24,
		},
		.capture = {
			.stream_name	= "PCM Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S8|		\
					  SNDRV_PCM_FORMAT_S16_LE |	\
					  SNDRV_PCM_FORMAT_S20_3LE |	\
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
			.formats	= SNDRV_PCM_FORMAT_S8|		\
					  SNDRV_PCM_FORMAT_S16_LE |	\
					  SNDRV_PCM_FORMAT_S20_3LE |	\
					  SNDRV_PCM_FORMAT_S24 |	\
					  SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name	= "PCM dummy Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S8|		\
					  SNDRV_PCM_FORMAT_S16_LE |	\
					  SNDRV_PCM_FORMAT_S20_3LE |	\
					  SNDRV_PCM_FORMAT_S24 |	\
					  SNDRV_PCM_FMTBIT_S32_LE,
		},
	},
};

static int pm805_audio_suspend(struct snd_soc_codec *codec)
{
	return 0;
}


static int pm805_audio_resume(struct snd_soc_codec *codec)
{
	return 0;
}

static int pm805_probe(struct snd_soc_codec *codec)
{
	struct pm805_priv *pm805 = snd_soc_codec_get_drvdata(codec);
	struct pm80x_chip *chip = pm805->chip;

	pm805->codec = codec;
	codec->control_data = chip;

	/* add below snd ctls to keep align with audio server */
	snd_soc_add_codec_controls(codec, pm805_audio_controls,
			     ARRAY_SIZE(pm805_audio_controls));
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
	.suspend =		pm805_audio_suspend,
	.resume =		pm805_audio_resume,
	.reg_cache_size =	CODEC_TOTAL_REG_SIZE,
	.reg_word_size =	sizeof(u8),
	.set_bias_level =	pm805_set_bias_level,
};

static int __devinit pm805_codec_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm805_priv *pm805;
	int ret, irq_short1, irq_short2;

	pm805 = devm_kzalloc(&pdev->dev, sizeof(struct pm805_priv), GFP_KERNEL);
	if (pm805 == NULL)
		return -ENOMEM;

	pm805->chip = chip;
	pm805->regmap = chip->regmap;

#ifdef CONFIG_MACH_HENDRIX
	create_gpio_proc_file();
#endif

	/* refer to drivers/mfd/88pm805.c for irq resource */
	irq_short1 = platform_get_irq(pdev, 1);
	if (irq_short1 < 0)
		dev_err(&pdev->dev, "No IRQ resource for audio short 1!\n");

	irq_short2 = platform_get_irq(pdev, 2);
	if (irq_short2 < 0)
		dev_err(&pdev->dev, "No IRQ resource for audio short 2!\n");

	pm805->irq_audio_short1 = irq_short1 + chip->irq_base;
	pm805->irq_audio_short2 = irq_short2 + chip->irq_base;
//KSND Marvell patch
	INIT_WORK(&pm805->work_short, pm805_short_work);

	ret = devm_request_threaded_irq(&pdev->dev, pm805->irq_audio_short1, NULL, pm805_short_handler, IRQF_ONESHOT, "audio_short1", pm805);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to request IRQ: #%d: %d\n", pm805->irq_audio_short1, ret);

	ret = devm_request_threaded_irq(&pdev->dev, pm805->irq_audio_short2, NULL, pm805_short_handler, IRQF_ONESHOT, "audio_short2", pm805);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to request IRQ: #%d: %d\n", pm805->irq_audio_short2, ret);

	platform_set_drvdata(pdev, pm805);

	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_pm805,
				     pm805_dai, ARRAY_SIZE(pm805_dai));
}

static int __devexit pm805_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int pm805_codec_suspend(struct device *dev)
{
	return 0;
}

static int pm805_codec_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops pm805_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm805_codec_suspend, pm805_codec_resume)
};

static struct platform_driver pm805_codec_driver = {
	.driver = {
		.name = "88pm80x-codec",
		.owner = THIS_MODULE,
		.pm = &pm805_pm_ops,
	},
	.probe = pm805_codec_probe,
	.remove = __devexit_p(pm805_codec_remove),
};

module_platform_driver(pm805_codec_driver);

MODULE_DESCRIPTION("ASoC 88PM805 driver");
MODULE_AUTHOR("Xiaofan Tian <tianxf@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:88pm805-codec");
