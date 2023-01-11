/*
 * 88pm860-codec.c  --  88PM860 ALSA Soc Audio driver
 *
 * Copyright (C) 2014 Marvell International Ltd.
 * Author: Zhao Ye <zhaoy@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <asm/div64.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/mfd/88pm80x.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/delay.h>

#include "88pm860-codec.h"
#include "../pxa/mmp-tdm.h"

#define AUDIO_NAME      "88PM860"
#define DRIVER_VERSION  "1.00"
#define DRIVER_RELEASE_DATE     "Jan.07 2013"

#define USE_DAPM_CTRL_88PM860

static atomic_t tdm_count = ATOMIC_INIT(0);

struct tdm_manage_private {
	/*tdm mode select*/
	bool use_4_wires;

	/* for read/write reg */
	struct device *dev;

	struct mutex mutex;
	/* slot maintaining */
	unsigned int slot_tol;
	/*for 4 wires*/
	unsigned int tx_unused_slot_tol;
	unsigned int rx_unused_slot_tol;
	/*for 3 wires*/
	unsigned int unused_slot_tol;

	struct list_head substream_list;
	unsigned int substream_num;

	/*tdm clock source*/
	unsigned int clk_src;

	/* tdm_cfg_param */
	unsigned int slot_size;
	unsigned int slot_space;
	unsigned int start_slot;
	unsigned int fsyn_rate;
	unsigned int bclk_rate;
	unsigned int fsyn_pulse_width;
};

/* TDM dai private data */
struct slot_info {
	int slot;
	int stream_id;
	enum tdm_out_reg cntrl_reg_id;
};

struct tdm_dai_private {
	struct device *dev;
	struct regmap *regmap;

	/* point to tdm management*/
	struct tdm_manage_private *tdm_manage_priv;
	/*
	 * tdm bt slot configuration
	 * bt tx/rx only support mono stream
	 * but for supporting increase/reduce
	 * slot dynamically, add additional one
	 */
	struct slot_info tdm_bt_tx[2];
	unsigned int tdm_bt_rx;
	int tdm_bt_tx_num;
	int tdm_bt_rx_num;

	/* tdm codec slot configuration */
	struct slot_info tdm_out1_tx[3];
	int tdm_out1_tx_num;
	struct slot_info tdm_out2_tx[3];
	int tdm_out2_tx_num;
	unsigned int tdm_codec_in1_rx[2];
	int tdm_codec_in1_rx_num;
	unsigned int tdm_codec_in2_rx[2];
	int tdm_codec_in2_rx_num;

	int tdm_fm_rx[2];
	int tdm_fm_rx_num;
};

struct tdm_used_entity {
	struct list_head list;
	struct snd_pcm_substream *substream;
	/* suppose each entity only supports mono/stereo */
	int tx[2];
	int tx_num;
	int rx[2];
	int rx_num;
};


struct pm860_private {
	struct snd_soc_codec *codec;

	enum snd_soc_control_type control_type;
	void *control_data;

#ifdef CONFIG_GPIOLIB
	struct work_struct work;
	unsigned gpio;
	unsigned det_hs;
	unsigned hs_det_status;
	unsigned det_mic;
	unsigned mic_det_status;
#endif

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	struct input_dev *input;
#endif

	struct proc_dir_entry *proc_file;
	struct regmap *regmap;
	struct pm80x_chip *chip;
	struct tdm_dai_private *tdm_dai;
	int irq_audio_short1;
	int irq_audio_short2;
};

struct pm860_init_reg {
	char name[30];
	u8	reg_value;
	u8	reg_index;
};

static int caps_charge = 2000;
module_param(caps_charge, int, 0);
MODULE_PARM_DESC(caps_charge, "88PM860 caps charge time (msecs)");

static struct regmap *pm80x_get_companion(struct pm80x_chip *chip)
{
	struct pm80x_chip *chip_comp;

	if (chip->pmic_type == PMIC_PM886) {
		return get_companion();
	} else {
		if (!chip->companion) {
			dev_err(chip->dev,
				"%s: no companion chip\n", __func__);
			return NULL;
		}

		chip_comp = i2c_get_clientdata(chip->companion);

		return chip_comp->regmap;
	}
}

static unsigned int pm860_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	struct pm860_private *pm860_priv = snd_soc_codec_get_drvdata(codec);
	struct pm80x_chip *chip = pm860_priv->chip;
	unsigned int value, ret;
	struct regmap *map;

	map = chip->regmap;

	if (reg >= PMIC_INDEX) {
		map = pm80x_get_companion(chip);
		if (!map)
			return -EINVAL;

		if (reg == PM822_CLASS_D_1)
			reg = PM822_CLASS_D_REG_BASE;
		else if (reg == PM822_MIS_CLASS_D_1)
			reg = PM822_MIS_CLASS_D_REG_1;
		else if (reg == PM822_MIS_CLASS_D_2)
			reg = PM822_MIS_CLASS_D_REG_2;
	}

	ret = regmap_read(map, reg, &value);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read reg 0x%x: %d\n", reg, ret);
		return ret;
	}

	return value;
}

static int pm860_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct pm860_private *pm860_priv = snd_soc_codec_get_drvdata(codec);
	struct pm80x_chip *chip = pm860_priv->chip;
	struct regmap *map, *map_comp;
	int ret = 0;

	/* when tdm dai is open, we can't let codec power off */
	if (reg == PM860_MAIN_POWER_REG	&& value == 0
		&& atomic_read(&tdm_count) > 0)
		return ret;

	value &= 0xff;

	dev_dbg(chip->dev, "write reg 0x%x, value 0x%x\n", reg, value);

	map = chip->regmap;

	/* Enable pm800 audio mode */
	map_comp = pm80x_get_companion(chip);

	if (map_comp) {
		if (reg >= PMIC_INDEX) {
			if (reg == PM822_CLASS_D_1)
				reg = PM822_CLASS_D_REG_BASE;
			else if (reg == PM822_MIS_CLASS_D_1)
				reg = PM822_MIS_CLASS_D_REG_1;
			else if (reg == PM822_MIS_CLASS_D_2)
				reg = PM822_MIS_CLASS_D_REG_2;

			map = map_comp;
		}
	}

	ret = regmap_write(map, reg, value);
	if (ret < 0)
		dev_err(chip->dev, "Failed to write reg 0x%x: %d\n", reg, ret);

	return ret;

}

int pm860_hw_init(struct snd_soc_codec *codec)
{
	return 0;
}
EXPORT_SYMBOL_GPL(pm860_hw_init);

static int pm860_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	return 0;
}

int pm860_set_sample_rate(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	return 0;
}
EXPORT_SYMBOL_GPL(pm860_set_sample_rate);

static const struct snd_kcontrol_new pm860_snd_controls[] = {
	SOC_SINGLE("PM860_REVISION_CTRL", 0x00, 0, 0xff, 0),
	SOC_SINGLE("PM860_MAIN_POWER_REG", 0x01, 0, 0xff, 0),
	SOC_SINGLE("PM860_INT_MANAGEMENT", 0x02, 0, 0xff, 0),
	SOC_SINGLE("PM860_INT_REG1", 0x03, 0, 0xff, 0),
	SOC_SINGLE("PM860_INT_REG2", 0x04, 0, 0xff, 0),
	SOC_SINGLE("PM860_INT_MASK_REG1", 0x05, 0, 0xff, 0),
	SOC_SINGLE("PM860_INT_MASK_REG2", 0x06, 0, 0xff, 0),
	SOC_SINGLE("PM860_SEQ_STATUS_REG1", 0x07, 0, 0xff, 0),
	SOC_SINGLE("PM860_SEQ_STATUS_REG2", 0x08, 0, 0xff, 0),
	SOC_SINGLE("PM860_SEQ_STATUS_REG3", 0x09, 0, 0xff, 0),
	SOC_SINGLE("PM860_SHORT_PROTECT", 0x0a, 0, 0xff, 0),
	SOC_SINGLE("PM860_LOAD_GROUND_DET1", 0x0b, 0, 0xff, 0),
	SOC_SINGLE("PM860_LOAD_GROUND_DET2", 0x0c, 0, 0xff, 0),
	SOC_SINGLE("PM860_LOAD_GROUND_DET3", 0x0d, 0, 0xff, 0),
	SOC_SINGLE("PM860_LOAD_GROUND_DET4", 0x0e, 0, 0xff, 0),
	SOC_SINGLE("PM860_LOAD_GROUND_DET5", 0x0f, 0, 0xff, 0),
	SOC_SINGLE("PM860_LOAD_GROUND_DET6", 0x10, 0, 0xff, 0),
	SOC_SINGLE("PM860_LOAD_GROUND_DET7", 0x11, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_SETTINGS", 0x20, 0, 0xff, 0),
	SOC_SINGLE("PM860_ANALOG_MIC_GAIN1", 0x21, 0, 0xff, 0),
	SOC_SINGLE("PM860_ANALOG_MIC_GAIN2", 0x22, 0, 0xff, 0),
	SOC_SINGLE("PM860_DMIC_SETTINGS", 0x23, 0, 0xff, 0),
	SOC_SINGLE("PM860_DWS_SETTINGS1", 0x24, 0, 0xff, 0),
	SOC_SINGLE("PM860_DWS_SETTINGS2", 0x25, 0, 0xff, 0),
	SOC_SINGLE("PM860_PDM_SETTINGS1", 0x30, 0, 0xff, 0),
	SOC_SINGLE("PM860_PDM_SETTINGS2",   0x31, 0, 0xff, 0),
	SOC_SINGLE("PM860_PDM_SETTINGS3", 0x32, 0, 0xff, 0),
	SOC_SINGLE("PM860_PDM_CONTROL1", 0x33, 0, 0xff, 0),
	SOC_SINGLE("PM860_PDM_CONTROL2",  0x34, 0, 0xff, 0),
	SOC_SINGLE("PM860_PDM_CONTROL3",  0x35, 0, 0xff, 0),
	SOC_SINGLE("PM860_HP_EP_SETTING",   0x36, 0, 0xff, 0),
	SOC_SINGLE("PM860_HP_SHRT_STATE", 0x37, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING1",   0x40, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING2", 0x41, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING3", 0x42, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING5",  0x43, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING6",  0x44, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING7",   0x45, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING8", 0x46, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING9", 0x47, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING10",  0x48, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING11",  0x49, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING12",   0x4a, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING13", 0x4b, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING14", 0x4c, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING15",  0x4d, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING17",  0x4e, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING16",   0x4f, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_SETTING17_1", 0x50, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_INTERRUPT", 0x51, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_FIFO_SIZE",   0x52, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_PLL_DIV",   0x53, 0, 0xff, 0),
	SOC_SINGLE("PM860_FIFO_NUM_SAMPLE",   0x54, 0, 0xff, 0),
	SOC_SINGLE("PM860_FIFO_STATUS",   0x55, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_APPLY_CONF", 0x56, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_TEST", 0x57, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_CHO_TEST1", 0x58, 0, 0xff, 0),
	SOC_SINGLE("PM860_TDM_CHO_TEST2", 0x59, 0, 0xff, 0),
	SOC_SINGLE("PM860_DIG_BLOCK_EN_REG1", 0x60, 0, 0xff, 0),
	SOC_SINGLE("PM860_DIG_BLOCK_EN_REG2", 0x61, 0, 0xff, 0),
	SOC_SINGLE("PM860_VOLUME_CHANNEL", 0x62, 0, 0xff, 0),
	SOC_SINGLE("PM860_AUTOMUTE_PARAM", 0x63, 0, 0xff, 0),
	SOC_SINGLE("PM860_VOL_SEL_1", 0x64, 0, 0xff, 0),
	SOC_SINGLE("PM860_VOL_SEL_2", 0x65, 0, 0xff, 0),
	SOC_SINGLE("PM860_VOL_SEL_3",   0x66, 0, 0xff, 0),
	SOC_SINGLE("PM860_VOL_SEL_4", 0x67, 0, 0xff, 0),
	SOC_SINGLE("PM860_CLIP_BITS_REG1", 0x68, 0, 0xff, 0),
	SOC_SINGLE("PM860_CLIP_BITS_REG2", 0x69, 0, 0xff, 0),
	SOC_SINGLE("PM860_DRE_REG1", 0x6a, 0, 0xff, 0),
	SOC_SINGLE("PM860_DRE_REG2", 0x6b, 0, 0xff, 0),
	SOC_SINGLE("PM860_DRE_REG3", 0x6c, 0, 0xff, 0),
	SOC_SINGLE("PM860_DRE_REG4", 0x6d, 0, 0xff, 0),
	SOC_SINGLE("PM860_ANALOG_BLOCK_EN", 0x70, 0, 0xff, 0),
	SOC_SINGLE("PM860_ANALOG_BLOCK_STATUS", 0x71, 0, 0xff, 0),
	SOC_SINGLE("PM860_ANALOG_BLOCK_SETTING", 0x72, 0, 0xff, 0),
	SOC_SINGLE("PM860_PAD_SETTING1", 0x73, 0, 0xff, 0),
	SOC_SINGLE("PM860_PAD_SETTING2", 0x74, 0, 0xff, 0),
	SOC_SINGLE("PM860_SDM_BLOCK_EN1", 0x75, 0, 0xff, 0),
	SOC_SINGLE("PM860_SDM_BLOCK_EN2", 0x76, 0, 0xff, 0),
	SOC_SINGLE("PM860_POWER_APMLIFIER", 0x77, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_BLOCK_EN", 0x78, 0, 0xff, 0),
	SOC_SINGLE("PM860_CLOCK_SETTINGS", 0x79, 0, 0xff, 0),
	SOC_SINGLE("PM860_REFGEN_SETTING", 0x7a, 0, 0xff, 0),
	SOC_SINGLE("PM860_CHARGE_PUMP_REG1", 0x7b, 0, 0xff, 0),
	SOC_SINGLE("PM860_CHARGE_PUMP_REG2", 0x7c, 0, 0xff, 0),
	SOC_SINGLE("PM860_CHARGE_PUMP_REG3", 0x7d, 0, 0xff, 0),
	SOC_SINGLE("PM860_SPECTRUM_SETTINGS1", 0x7e, 0, 0xff, 0),
	SOC_SINGLE("PM860_SPECTRUM_SETTINGS2", 0x7f, 0, 0xff, 0),
	SOC_SINGLE("PM860_SPECTRUM_SETTINGS3", 0x80, 0, 0xff, 0),
	SOC_SINGLE("PM860_FLL_STATUS", 0x81, 0, 0xff, 0),
	SOC_SINGLE("PM860_AUTO_SEQUENCER_1", 0x82, 0, 0xff, 0),
	SOC_SINGLE("PM860_AUTO_SEQUENCER_2", 0x83, 0, 0xff, 0),
	SOC_SINGLE("PM860_RECONSTRUCTION_FILTER", 0x84, 0, 0xff, 0),
	SOC_SINGLE("PM860_DWA_SETTINGS", 0x85, 0, 0xff, 0),
	SOC_SINGLE("PM860_VOLUME_OUT_SETTING", 0x86, 0, 0xff, 0),
	SOC_SINGLE("PM860_AUTOMUTE_SETTING", 0x87, 0, 0xff, 0),
	SOC_SINGLE("PM860_POWER_AMPLIFIER", 0x88, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_SETTING1", 0x89, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_SETTING2", 0x8a, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_SETTING3", 0x8b, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_SETTING4", 0x8c, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_SETTING5", 0x8d, 0, 0xff, 0),
	SOC_SINGLE("PM860_SPARE_BITS", 0x8e, 0, 0xff, 0),
	SOC_SINGLE("PM860_HP_GAIN", 0x8f, 0, 0xff, 0),
	SOC_SINGLE("PM860_TEST_ID_REG", 0xa0, 0, 0xff, 0),
	SOC_SINGLE("PM860_CHARGE_PUMP_1", 0xa1, 0, 0xff, 0),
	SOC_SINGLE("PM860_CHARGE_PUMP_PLL", 0xa2, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_SETTING_6", 0xa3, 0, 0xff, 0),
	SOC_SINGLE("PM860_ADC_SPARE_BIT", 0xa4, 0, 0xff, 0),
	SOC_SINGLE("PM860_PAD_SETTING", 0xa5, 0, 0xff, 0),
	SOC_SINGLE("PM860_MIC_BIAS_SETTING", 0xa6, 0, 0xff, 0),
	SOC_SINGLE("PM860_REFERENCE_GROUP", 0xa7, 0, 0xff, 0),
	SOC_SINGLE("PM860_PLL_REG", 0xa8, 0, 0xff, 0),
	SOC_SINGLE("PM860_DIG_TEST_MODE1", 0xa9, 0, 0xff, 0),
	SOC_SINGLE("PM860_DAC_DWA_TEST", 0xaa, 0, 0xff, 0),
	SOC_SINGLE("PM860_DWA_TEST_SETTING", 0xab, 0, 0xff, 0),
	SOC_SINGLE("PM860_PDM_OVS_DWS", 0xac, 0, 0xff, 0),
	SOC_SINGLE("PM860_DIGITAL_TEST", 0xad, 0, 0xff, 0),
	SOC_SINGLE("PM860_AUTOMUTE_TO_ANALOG", 0xae, 0, 0xff, 0),
	SOC_SINGLE("PM860_DIG_TEST_MODE2", 0xaf, 0, 0xff, 0),
	SOC_SINGLE("PM860_DIG_TEST_MODE3", 0xb0, 0, 0xff, 0),
	SOC_SINGLE("PM860_TEST_SETTING", 0xb1, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG1", 0xb2, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG2", 0xb3, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG3", 0xb4, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG4", 0xb5, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG5", 0xb6, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG6", 0xb7, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG7", 0xb8, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG8", 0xb9, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG9", 0xba, 0, 0xff, 0),
	SOC_SINGLE("PM860_SINUSOIDAL_WAVE_REG10", 0xbb, 0, 0xff, 0),
	SOC_SINGLE("PM860_IO_TEST_SETTING_REG1", 0xbc, 0, 0xff, 0),
	SOC_SINGLE("PM860_IO_TEST_SETTING_REG2", 0xbd, 0, 0xff, 0),
	SOC_SINGLE("PM860_IO_TEST_SETTING_REG5", 0xbe, 0, 0xff, 0),
	SOC_SINGLE("PM860_IO_TEST_SETTING_REG6", 0xbf, 0, 0xff, 0),
	SOC_SINGLE("PM860_FLL_SETTINGS_REG1", 0xc0, 0, 0xff, 0),
	SOC_SINGLE("PM860_FLL_SETTINGS_REG2", 0xc1, 0, 0xff, 0),
	SOC_SINGLE("PM860_DIGITAL_TEST_OUT", 0xc2, 0, 0xff, 0),
	SOC_SINGLE("PM860_MEMORY_CONTROL1", 0xc3, 0, 0xff, 0),
	SOC_SINGLE("PM860_MEMORY_CONTROL2", 0xc4, 0, 0xff, 0),
	SOC_SINGLE("PM860_MEMORY_CONTROL3", 0xc5, 0, 0xff, 0),
	SOC_SINGLE("PM822_CLASS_D_1", PM822_CLASS_D_1, 0, 0xff, 0),
	SOC_SINGLE("PM822_MIS_CLASS_D_1", PM822_MIS_CLASS_D_1, 0, 0xff, 0),
	SOC_SINGLE("PM822_MIS_CLASS_D_2", PM822_MIS_CLASS_D_2, 0, 0xff, 0),
};

static int pm860_set_dai_pll(struct snd_soc_dai *codec_dai,
			      int pll_id, int source,
			      unsigned int freq_in,
			      unsigned int freq_out)
{
	return 0;
}

static int pm860_set_dai_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int tmp;
	unsigned int count = 100;

	if (atomic_read(&tdm_count) == 0) {
		/* we need codec power up, before set tdm register */
		if (!snd_soc_read(codec, PM860_MAIN_POWER_REG)) {
			usleep_range(1000, 1200);
			snd_soc_write(codec, PM860_MAIN_POWER_REG, 0x1);
			while (count--) {
				tmp = snd_soc_read(codec,
					PM860_SEQ_STATUS_REG1);
				if (tmp & 1)
					break;

			}
		}

#ifdef USE_3_WIRES_MODE
		/* set 3 wires mode for codec */
		tmp = snd_soc_read(codec, PM860_TDM_SETTING1);
		tmp |= (1 << 7);
		snd_soc_write(codec, PM860_TDM_SETTING1, tmp);
#endif
		/* enable pll_compat_mode */
		tmp = snd_soc_read(codec, PM860_TDM_PLL_DIV);
		tmp |= (1 << 5);
		snd_soc_write(codec, PM860_TDM_PLL_DIV, tmp);
		/* align the tdm slot space with MAP */
		snd_soc_write(codec, PM860_TDM_SETTING15, 0x10);
		/* align fsyn pulse width with MAP */
		snd_soc_write(codec, PM860_TDM_SETTING5, 0x02);
		snd_soc_write(codec, PM860_TDM_SETTING6, 0xa0);

		/* enable the serial audio interface */
		tmp = snd_soc_read(codec, PM860_DIG_BLOCK_EN_REG2);
		tmp |= 1;
		snd_soc_write(codec, PM860_DIG_BLOCK_EN_REG2, tmp);
		/* apply tdm config */
		tmp = (APPLY_TDM_CONF | APPLY_CKG_CONF);
		snd_soc_write(codec, PM860_TDM_APPLY_CONF, tmp);
	}

	atomic_inc(&tdm_count);

	return 0;
}

static void pm860_set_dai_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int tmp;

	/* disable the serial audio interface */
	if (atomic_read(&tdm_count) == 1) {
		tmp = snd_soc_read(codec, PM860_DIG_BLOCK_EN_REG2);
		tmp &= 0xfe;
		snd_soc_write(codec, PM860_DIG_BLOCK_EN_REG2, tmp);
		/* apply tdm config */
		tmp = (APPLY_TDM_CONF | APPLY_CKG_CONF);
		snd_soc_write(codec, PM860_TDM_APPLY_CONF, tmp);
	}

	atomic_dec(&tdm_count);

	/* if all TDM dai are disabled, codec can be disabled */
	if (atomic_read(&tdm_count) == 0)
		snd_soc_write(codec, PM860_MAIN_POWER_REG, 0x0);

	return;
}

static int pm860_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	int ret = 0;

	return ret;
}

static int pm860_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int tdm_ctrl_reg1, tdm_ctrl_reg6, tdm_ctrl_reg7;
	struct tdm_used_entity *entity;
	int i;
	unsigned int offset;

	entity = get_tdm_entity(substream);
	if (unlikely(entity == NULL))
		return -EINVAL;

	tdm_ctrl_reg6 = snd_soc_read(codec, PM860_TDM_SETTING16);
	tdm_ctrl_reg7 = snd_soc_read(codec, PM860_TDM_SETTING17_1);
	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		for (i = 0; i < entity->tx_num; i++) {
			offset = (entity->tx[i] - 1) * 2;
			tdm_ctrl_reg6 &= ~(0x3 << offset);
			tdm_ctrl_reg6 |= TDM_CHO_DL(1) << offset;
		}
		for (i = 0; i < entity->rx_num; i++) {
			offset = (entity->rx[i] - 1) * 2;
			tdm_ctrl_reg7 &= ~(0x3 << offset);
			tdm_ctrl_reg7 |= TDM_CHI_DL(1) << offset;

		}
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		for (i = 0; i < entity->tx_num; i++) {
			tdm_ctrl_reg6 &= ~(0x3 << entity->tx[i]);
			tdm_ctrl_reg6 |= TDM_CHO_DL(0) << entity->tx[i];
		}
		for (i = 0; i < entity->rx_num; i++) {
			tdm_ctrl_reg7 &= ~(0x3 << entity->rx[i]);
			tdm_ctrl_reg7 |= TDM_CHI_DL(0) << entity->rx[i];
		}
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		for (i = 0; i < entity->tx_num; i++) {
			tdm_ctrl_reg6 &= ~(0x3 << entity->tx[i]);
			tdm_ctrl_reg6 |= TDM_CHO_DL(2) << entity->tx[i];
		}
		for (i = 0; i < entity->rx_num; i++) {
			tdm_ctrl_reg7 &= ~(0x3 << entity->rx[i]);
			tdm_ctrl_reg7 |= TDM_CHI_DL(2) << entity->rx[i];
		}
		break;
	default:
		return -EINVAL;
	}
	snd_soc_write(codec, PM860_TDM_SETTING16, tdm_ctrl_reg6);
	snd_soc_write(codec, PM860_TDM_SETTING17_1, tdm_ctrl_reg7);

	/* apply tdm config */
	tdm_ctrl_reg1 = (APPLY_TDM_CONF | APPLY_CKG_CONF);
	snd_soc_write(codec, PM860_TDM_APPLY_CONF, tdm_ctrl_reg1);


	return 0;
}

static int pm860_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	unsigned int tdm_ctrl1 = 0, addr;
	unsigned int ret = 0;

	addr =	PM860_TDM_SETTING1;
	tdm_ctrl1 = snd_soc_read(codec, addr);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		tdm_ctrl1 |= PM860_TDM_MST;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		tdm_ctrl1 &= ~PM860_TDM_MST;
		break;
	default:
		return -EINVAL;
	}
	ret = snd_soc_write(codec, addr, tdm_ctrl1);
	if (ret < 0)
		return ret;

	/* apply tdm config */
	tdm_ctrl1 = (APPLY_TDM_CONF | APPLY_CKG_CONF);
	snd_soc_write(codec, PM860_TDM_APPLY_CONF, tdm_ctrl1);

	return ret;
}

static int pm860_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static void tdm_mic_channel_update(struct snd_soc_codec *codec, int channel,
	int stream, int slot)
{
	unsigned int reg = 0, val;

	switch (channel) {
	case 1:
		reg = PM860_TDM_SETTING12;
		break;
	case 2:
		reg = PM860_TDM_SETTING11;
		break;
	case 3:
		reg = PM860_TDM_SETTING10;
		break;
	case 4:
		reg = PM860_TDM_SETTING9;
		break;
	default:
		pr_debug("the channel num is out of range\n");
		return;
	}

	if (slot == 1) {
		val = snd_soc_read(codec, reg);
		val &= ~0xf;
		val |= stream;
		snd_soc_write(codec, reg, val);
	} else if (slot == 2) {
		val = snd_soc_read(codec, reg);
		val &= ~(0xf << 4);
		val |= (stream << 4);
		snd_soc_write(codec, reg, val);
	}
}

/* mic1 */
static int pm860_set_dai_mic1_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pm860_private *pm860_priv = snd_soc_codec_get_drvdata(codec);
	struct tdm_dai_private *tdm_dai_priv = pm860_priv->tdm_dai;
	int stream_id;
	int tdm_out1_tx_num;
	struct slot_info *tdm_out1_tx;
	bool found;
	enum tdm_out_reg cntrl_reg_id;
	int i, j;
	unsigned int tdm_apply_ctrl;

	tdm_out1_tx_num = tdm_dai_priv->tdm_out1_tx_num;
	tdm_out1_tx = tdm_dai_priv->tdm_out1_tx;
	cntrl_reg_id = NO_OUT_CONTROL_REG;
	if (tx_num > tdm_out1_tx_num) {
		for (i = 0; i < tx_num; i++) {
			if (tdm_dai_priv->tdm_out1_tx_num == 0)
				stream_id = OUT1_CH1;
			else if (tdm_dai_priv->tdm_out1_tx_num == 1)
				stream_id = OUT1_CH2;
			else if (tdm_dai_priv->tdm_out1_tx_num == 2)
				stream_id = (tdm_out1_tx[0].slot
					 > tdm_out1_tx[1].slot) ?
					tdm_out1_tx[0].stream_id :
					tdm_out1_tx[1].stream_id;

			found = false;
			for (j = 0; j < tdm_out1_tx_num; j++) {
				if (tx_slot[i] == tdm_out1_tx[j].slot) {
					found = true;
					break;
				}
				if (stream_id == tdm_out1_tx[j].stream_id) {
					cntrl_reg_id =
						tdm_out1_tx[j].cntrl_reg_id;
				}
			}
			if (found)
				continue;

			if (cntrl_reg_id != TDM_CONTRL_REG2) {
				tdm_mic_channel_update(codec, stream_id,
				tx_slot[i], 1);
				tdm_out1_tx[tdm_dai_priv->
					tdm_out1_tx_num].slot =
					tx_slot[i];
				tdm_out1_tx[tdm_dai_priv->
					tdm_out1_tx_num].stream_id =
					stream_id;
				tdm_out1_tx[tdm_dai_priv->
					tdm_out1_tx_num].cntrl_reg_id =
					TDM_CONTRL_REG2;
				tdm_dai_priv->tdm_out1_tx_num++;
			} else {
				tdm_mic_channel_update(codec, stream_id,
				tx_slot[i], 2);
				tdm_out1_tx[tdm_dai_priv->
					tdm_out1_tx_num].slot =
					tx_slot[i];
				tdm_out1_tx[tdm_dai_priv->
					tdm_out1_tx_num].stream_id =
					stream_id;
				tdm_out1_tx[tdm_dai_priv->
					tdm_out1_tx_num].cntrl_reg_id =
					TDM_CONTRL_REG3;
				tdm_dai_priv->tdm_out1_tx_num++;
			}
		}
	} else if (tx_num < tdm_out1_tx_num) {
		for (i = 0; i < tdm_out1_tx_num; i++) {
			found = false;
			for (j = 0; j < tx_num; j++) {
				if (tx_slot[j] == tdm_out1_tx[i].slot) {
					found = true;
					break;
				}
			}
			if (found)
				continue;

			/* reduce the slot */
			if (tdm_out1_tx[i].cntrl_reg_id == TDM_CONTRL_REG2)
				tdm_mic_channel_update(codec,
					tdm_out1_tx[i].stream_id, 0, 1);
			else
				tdm_mic_channel_update(codec,
					tdm_out1_tx[i].stream_id, 0, 2);

			/* rearrage the tdm_out1_tx array */
			for (j = i; j < tdm_out1_tx_num; j++) {
				tdm_out1_tx[j].slot =
					tdm_out1_tx[j+1].slot;
				tdm_out1_tx[j].stream_id =
					tdm_out1_tx[j+1].stream_id;
				tdm_out1_tx[j].cntrl_reg_id =
					tdm_out1_tx[j+1].cntrl_reg_id;
			}
			tdm_out1_tx[j - 1].slot = 0;
			tdm_out1_tx[j - 1].stream_id = 0;
			tdm_out1_tx[j - 1].cntrl_reg_id = 0;
			tdm_dai_priv->tdm_out1_tx_num--;
			break;
		}
	} else {
		/* replace for disabling slot purpose */
		for (i = 0; i < tdm_out1_tx_num; i++) {
			if ((tx_slot[i] == 0) &&
				(tdm_out1_tx[i].cntrl_reg_id ==
					TDM_CONTRL_REG2)) {
				tdm_mic_channel_update(codec,
					tdm_out1_tx[i].stream_id, 0, 1);
				tdm_out1_tx[i].slot = 0;
				tdm_out1_tx[i].stream_id = 0;
				tdm_out1_tx[i].cntrl_reg_id = 0;
			} else if ((tx_slot[i] == 0) &&
				(tdm_out1_tx[i].cntrl_reg_id ==
					PM860_TDM_SETTING13)) {
				tdm_mic_channel_update(codec,
					tdm_out1_tx[i].stream_id, 0, 2);
				tdm_out1_tx[i].slot = 0;
				tdm_out1_tx[i].stream_id = 0;
				tdm_out1_tx[i].cntrl_reg_id = 0;
			}
		}
		tdm_dai_priv->tdm_out1_tx_num = 0;
	}
	tdm_apply_ctrl = (APPLY_TDM_CONF | APPLY_CKG_CONF);
	snd_soc_write(codec, PM860_TDM_APPLY_CONF, tdm_apply_ctrl);

	return 0;
}

/* mic2 */
static int pm860_set_dai_mic2_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pm860_private *pm860_priv = snd_soc_codec_get_drvdata(codec);
	struct tdm_dai_private *tdm_dai_priv = pm860_priv->tdm_dai;
	int stream_id;
	int tdm_out2_tx_num;
	struct slot_info *tdm_out2_tx;
	bool found;
	enum tdm_out_reg cntrl_reg_id;
	int i, j;
	unsigned int tdm_apply_ctrl;

	tdm_out2_tx_num = tdm_dai_priv->tdm_out2_tx_num;
	tdm_out2_tx = tdm_dai_priv->tdm_out2_tx;
	cntrl_reg_id = NO_OUT_CONTROL_REG;
	if (tx_num > tdm_out2_tx_num) {
		for (i = 0; i < tx_num; i++) {
			if (tdm_dai_priv->tdm_out2_tx_num == 0)
				stream_id = OUT2_CH1;
			else if (tdm_dai_priv->tdm_out2_tx_num == 1)
				stream_id = OUT2_CH2;
			else if (tdm_dai_priv->tdm_out2_tx_num == 2)
				stream_id =
					(tdm_out2_tx[0].slot
					 > tdm_out2_tx[1].slot) ?
					tdm_out2_tx[0].stream_id :
					tdm_out2_tx[1].stream_id;

			found = false;
			for (j = 0; j < tdm_out2_tx_num; j++) {
				if (tx_slot[i] == tdm_out2_tx[j].slot) {
					found = true;
					break;
				}
				if (stream_id == tdm_out2_tx[j].stream_id) {
					cntrl_reg_id =
						tdm_out2_tx[j].cntrl_reg_id;
				}
			}
			if (found)
				continue;

			if (cntrl_reg_id != TDM_CONTRL_REG2) {
				tdm_mic_channel_update(codec, stream_id,
						tx_slot[i], 1);
				tdm_out2_tx[tdm_dai_priv->
					tdm_out2_tx_num].slot =
					tx_slot[i];
				tdm_out2_tx[tdm_dai_priv->
					tdm_out2_tx_num].stream_id =
					stream_id;
				tdm_out2_tx[tdm_dai_priv->
					tdm_out2_tx_num].cntrl_reg_id =
					TDM_CONTRL_REG2;
				tdm_dai_priv->tdm_out2_tx_num++;
			} else {
				tdm_mic_channel_update(codec, stream_id,
						tx_slot[i], 2);
				tdm_out2_tx[tdm_dai_priv->
					tdm_out2_tx_num].slot =
					tx_slot[i];
				tdm_out2_tx[tdm_dai_priv->
					tdm_out2_tx_num].stream_id =
					stream_id;
				tdm_out2_tx[tdm_dai_priv->
					tdm_out2_tx_num].cntrl_reg_id =
					TDM_CONTRL_REG3;
				tdm_dai_priv->tdm_out2_tx_num++;
			}
		}
	} else if (tx_num < tdm_out2_tx_num) {
		for (i = 0; i < tdm_out2_tx_num; i++) {
			found = false;
			for (j = 0; j < tx_num; j++) {
				if (tx_slot[j] == tdm_out2_tx[i].slot) {
					found = true;
					break;
				}
			}
			if (found)
				continue;
			/* reduce the slot */
			if (tdm_out2_tx[i].cntrl_reg_id == TDM_CONTRL_REG2)
				tdm_mic_channel_update(codec,
					tdm_out2_tx[i].stream_id, 0, 1);
			else
				tdm_mic_channel_update(codec,
					tdm_out2_tx[i].stream_id, 0, 2);

			/* re-arrange the tdm_out2_tx array */
			for (j = i; j < tdm_out2_tx_num; j++) {
				tdm_out2_tx[j].slot =
					tdm_out2_tx[j+1].slot;
				tdm_out2_tx[j].stream_id =
					tdm_out2_tx[j+1].stream_id;
				tdm_out2_tx[j].cntrl_reg_id =
					tdm_out2_tx[j+1].cntrl_reg_id;
			}
			tdm_out2_tx[j - 1].slot = 0;
			tdm_out2_tx[j - 1].stream_id = 0;
			tdm_out2_tx[j - 1].cntrl_reg_id = 0;
			tdm_dai_priv->tdm_out2_tx_num--;
			break;
		}
	} else {
		/* replace for disabling slot purpose */
		for (i = 0; i < tdm_out2_tx_num; i++) {
			if ((tx_slot[i] == 0) &&
				(tdm_out2_tx[i].cntrl_reg_id ==
					TDM_CONTRL_REG2)) {
				tdm_mic_channel_update(codec,
					tdm_out2_tx[i].stream_id, 0, 1);
				tdm_out2_tx[i].slot = 0;
				tdm_out2_tx[i].stream_id = 0;
				tdm_out2_tx[i].cntrl_reg_id = 0;
			} else if ((tx_slot[i] == 0) &&
					(tdm_out2_tx[i].cntrl_reg_id ==
						TDM_CONTRL_REG3)) {
				tdm_mic_channel_update(codec,
					tdm_out2_tx[i].stream_id, 0, 2);
				tdm_out2_tx[i].slot = 0;
				tdm_out2_tx[i].stream_id = 0;
				tdm_out2_tx[i].cntrl_reg_id = 0;
			}
		}
		tdm_dai_priv->tdm_out2_tx_num = 0;
	}
	tdm_apply_ctrl = (APPLY_TDM_CONF | APPLY_CKG_CONF);
	snd_soc_write(codec, PM860_TDM_APPLY_CONF, tdm_apply_ctrl);

	return 0;
}

/* out1 */
static int pm860_set_dai_out1_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pm860_private *pm860_priv = snd_soc_codec_get_drvdata(codec);
	struct tdm_dai_private *tdm_dai_priv = pm860_priv->tdm_dai;
	unsigned int rx_slot_1, tdm_apply_ctrl;
	int i;

	rx_slot_1 = snd_soc_read(codec, PM860_TDM_SETTING14);

	/* FixME: For out1, we assume it use the [4-7] bits */
	rx_slot_1 &= ~0xff;
	rx_slot_1 |= rx_slot[0];
	rx_slot_1 |= (rx_slot[1] << 4);
	snd_soc_write(codec, PM860_TDM_SETTING14, rx_slot_1);

	tdm_dai_priv->tdm_codec_in1_rx_num = rx_num;
	for (i = 0; i < rx_num; i++)
		tdm_dai_priv->tdm_codec_in1_rx[i] = rx_slot[i];

	tdm_apply_ctrl = (APPLY_TDM_CONF | APPLY_CKG_CONF);
	snd_soc_write(codec, PM860_TDM_APPLY_CONF, tdm_apply_ctrl);

	return 0;
}

/* out2 */
static int pm860_set_dai_out2_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pm860_private *pm860_priv = snd_soc_codec_get_drvdata(codec);
	struct tdm_dai_private *tdm_dai_priv = pm860_priv->tdm_dai;
	unsigned int rx_slot_1, tdm_apply_ctrl;
	int i;

	rx_slot_1 = snd_soc_read(codec, PM860_TDM_SETTING13);

	rx_slot_1 &= ~0xff;
	rx_slot_1 |= rx_slot[0];
	rx_slot_1 |= rx_slot[1] << 4;

	snd_soc_write(codec, PM860_TDM_SETTING13, rx_slot_1);

	tdm_dai_priv->tdm_codec_in2_rx_num = rx_num;
	for (i = 0; i < rx_num; i++)
		tdm_dai_priv->tdm_codec_in2_rx[i] = rx_slot[i];

	tdm_apply_ctrl = (APPLY_TDM_CONF | APPLY_CKG_CONF);
	snd_soc_write(codec, PM860_TDM_APPLY_CONF, tdm_apply_ctrl);


	return 0;
}

/*
 * 88PM860 can support sample rate 12000 and 24000, if let ASOC support
 * these two rates, you need to change include/sound/pcm.h and
 * sound/core/pcm_native.c
 */
#define PM860_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 | \
	SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_11025 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 | \
	SNDRV_PCM_RATE_176400)

#define PM860_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops pm860_mic1_dai_ops = {
	.startup	= pm860_set_dai_startup,
	.shutdown	= pm860_set_dai_shutdown,
	.hw_params	= pm860_hw_params,
	.digital_mute	= pm860_mute,
	.set_fmt	= pm860_set_dai_fmt,
	.set_sysclk	= pm860_set_dai_sysclk,
	.set_pll	= pm860_set_dai_pll,
	.set_channel_map = pm860_set_dai_mic1_channel_map,
};

static struct snd_soc_dai_ops pm860_mic2_dai_ops = {
	.startup	= pm860_set_dai_startup,
	.shutdown	= pm860_set_dai_shutdown,
	.hw_params	= pm860_hw_params,
	.digital_mute	= pm860_mute,
	.set_fmt	= pm860_set_dai_fmt,
	.set_sysclk	= pm860_set_dai_sysclk,
	.set_pll	= pm860_set_dai_pll,
	.set_channel_map = pm860_set_dai_mic2_channel_map,
};

static struct snd_soc_dai_ops pm860_out1_dai_ops = {
	.startup	= pm860_set_dai_startup,
	.shutdown	= pm860_set_dai_shutdown,
	.hw_params	= pm860_hw_params,
	.digital_mute	= pm860_mute,
	.set_fmt	= pm860_set_dai_fmt,
	.set_sysclk	= pm860_set_dai_sysclk,
	.set_pll	= pm860_set_dai_pll,
	.set_channel_map = pm860_set_dai_out1_channel_map,
};

static struct snd_soc_dai_ops pm860_out2_dai_ops = {
	.startup	= pm860_set_dai_startup,
	.shutdown	= pm860_set_dai_shutdown,
	.hw_params	= pm860_hw_params,
	.digital_mute	= pm860_mute,
	.set_fmt	= pm860_set_dai_fmt,
	.set_sysclk	= pm860_set_dai_sysclk,
	.set_pll	= pm860_set_dai_pll,
	.set_channel_map = pm860_set_dai_out2_channel_map,
};

struct snd_soc_dai_driver pm860_dai[4] = {
	/* hifi codec dai */
	{
		.name = "88pm860-tdm-mic1",
		.id = 1,
		.capture = {
			.stream_name  = "Hifi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = PM860_RATES,
			.formats      = PM860_FORMATS,
		},
		.ops = &pm860_mic1_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "88pm860-tdm-mic2",
		.id = 2,
		.capture = {
			.stream_name  = "Voice Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = PM860_RATES,
			.formats      = PM860_FORMATS,
		},
		.ops = &pm860_mic2_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "88pm860-tdm-out1",
		.id = 3,
		.playback = {
			.stream_name  = "Hifi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = PM860_RATES,
			.formats      = PM860_FORMATS,
		},
		.ops = &pm860_out1_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "88pm860-tdm-out2",
		.id = 4,
		.playback = {
			.stream_name  = "Voice Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = PM860_RATES,
			.formats      = PM860_FORMATS,
		},
		.ops = &pm860_out2_dai_ops,
		.symmetric_rates = 1,
	}
};

static void pm860_work(struct work_struct *work)
{
	struct snd_soc_dapm_context *dapm =
		container_of(work, struct snd_soc_dapm_context,
			delayed_work.work);
	struct snd_soc_codec *codec = dapm->codec;
	pm860_set_bias_level(codec, dapm->bias_level);
}

static int pm860_remove(struct snd_soc_codec *codec)
{
	pm860_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

#ifdef CONFIG_PM
static int pm860_suspend(struct snd_soc_codec *codec)
{

	pm860_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int pm860_resume(struct snd_soc_codec *codec)
{

	pm860_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
static int pm860_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int pm860_resume(struct snd_soc_codec *codec)
{
	return 0;
}
#endif

static int pm860_probe(struct snd_soc_codec *codec)
{
	struct pm860_private *pm860_priv = snd_soc_codec_get_drvdata(codec);
	struct pm80x_chip *chip = pm860_priv->chip;
	int ret = 0;

	INIT_DELAYED_WORK(&codec->dapm.delayed_work, pm860_work);

	codec->control_data = chip;

	pm860_priv->codec = codec;

	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	pm860_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
	codec->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	schedule_delayed_work(&codec->dapm.delayed_work,
				msecs_to_jiffies(caps_charge));

	return ret;
}

struct snd_soc_codec_driver soc_codec_dev_pm860 = {
	.probe   = pm860_probe,
	.remove  = pm860_remove,
	.suspend = pm860_suspend,
	.resume  = pm860_resume,
	.read = pm860_read,
	.write = pm860_write,
	.set_bias_level = pm860_set_bias_level,
	.reg_cache_size = CODEC_TOTAL_REG_SIZE,
	.reg_word_size = sizeof(u8),
	.controls = pm860_snd_controls,
	.num_controls = ARRAY_SIZE(pm860_snd_controls),
};

static int pm860_codec_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm860_private *pm860_priv;
	struct tdm_dai_private *tdm_dai_priv;
	struct tdm_manage_private *tdm_priv;
	int ret = 0;

	pm860_priv = devm_kzalloc(&pdev->dev,
		sizeof(struct pm860_private), GFP_KERNEL);
	if (pm860_priv == NULL)
		return -ENOMEM;

	tdm_dai_priv = devm_kzalloc(&pdev->dev,
		sizeof(struct tdm_dai_private), GFP_KERNEL);
	if (tdm_dai_priv == NULL)
		return -ENOMEM;

	tdm_priv = devm_kzalloc(&pdev->dev,
		sizeof(struct tdm_manage_private), GFP_KERNEL);
	if (tdm_priv == NULL)
		return -ENOMEM;

	tdm_dai_priv->regmap = chip->regmap;
	tdm_dai_priv->tdm_manage_priv = tdm_priv;

	pm860_priv->chip = chip;
	pm860_priv->regmap = chip->regmap;
	pm860_priv->tdm_dai = tdm_dai_priv;

	platform_set_drvdata(pdev, pm860_priv);

	ret = snd_soc_register_codec(&pdev->dev,
		&soc_codec_dev_pm860, pm860_dai, ARRAY_SIZE(pm860_dai));
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register codec 88pm860\n");
		goto err_out1;
	}

	return ret;

err_out1:
	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int pm860_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void pm860_codec_shutdown(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct regmap *map = chip->regmap;

	/* mute all the volume */
	regmap_write(map, PM860_VOL_SEL_1, 0);
	regmap_write(map, PM860_VOL_SEL_2, 0);
	regmap_write(map, PM860_VOL_SEL_3, 0);
	regmap_write(map, PM860_VOL_SEL_4, 0);

	/* disable the codec */
	regmap_write(map, PM860_MAIN_POWER_REG, 0);

	return;
}

static struct platform_driver pm860_codec_driver = {
	.probe		= pm860_codec_probe,
	.remove		= pm860_codec_remove,
	.shutdown	= pm860_codec_shutdown,
	.driver		= {
		.name	= "88pm860-codec",
		.owner	= THIS_MODULE,
	},
};

static int __init pm860_init(void)
{
	return platform_driver_register(&pm860_codec_driver);
}

static void __exit pm860_exit(void)
{
	platform_driver_unregister(&pm860_codec_driver);
}

module_init(pm860_init);
module_exit(pm860_exit);

MODULE_DESCRIPTION("ASoc Marvell 88PM860 driver");
MODULE_AUTHOR("Zhao Ye <zhaoy@marvell.com>");
MODULE_LICENSE("GPL");
