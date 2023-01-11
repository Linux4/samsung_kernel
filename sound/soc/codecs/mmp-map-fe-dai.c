/*
 * mmp-map-fe-dai.c  --  MAP(MARVELL AUDIO PROCESSOR) FE DAI driver
 *
 * Copyright (C) 2014 Marvell International Ltd.
 * Author: Nenghua Cao <nhcao@marvell.com>
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
#include <linux/clk.h>
#include <linux/pm.h>
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
#include <linux/mfd/mmp-map.h>
#include <linux/mfd/88pm80x.h>

#include "../pxa/mmp-tdm.h"

struct map_fe_dai_private {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	enum	snd_soc_control_type control_type;
	void	*control_data;
	struct	proc_dir_entry *proc_file;
	/* point to mmp-map */
	struct map_private *map_priv;
	/* Indication if i2s is configured */
	bool i2s_config[5];
	/* Indication if i2s need to be active/inactive */
};

static unsigned int map_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;

	map_fe_dai_priv = snd_soc_codec_get_drvdata(codec);
	map_priv = map_fe_dai_priv->map_priv;

	return map_raw_read(map_priv, reg);
}

static int map_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	int ret = 0;

	map_fe_dai_priv = snd_soc_codec_get_drvdata(codec);
	map_priv = map_fe_dai_priv->map_priv;

	ret = map_raw_write(map_priv, reg, value);

	return ret;
}

static int map_bytes_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	int ret;

	map_fe_dai_priv = snd_soc_codec_get_drvdata(codec);
	map_priv = map_fe_dai_priv->map_priv;

	ret = map_raw_bulk_read(map_priv, params->base,
			ucontrol->value.bytes.data,
			params->num_regs);

	if (params->base == MAP_DSP1_FW_REG)
		*(u32 *)(&ucontrol->value.bytes.data) = map_priv->dsp1_sw_id;
	else if (params->base == MAP_DSP2_FW_REG)
		*(u32 *)(&ucontrol->value.bytes.data) = map_priv->dsp2_sw_id;
	else if (params->base == MAP_DSP1A_FW_REG)
		*(u32 *)(&ucontrol->value.bytes.data) = map_priv->dsp1a_sw_id;
	else if (params->base == MAP_BT_WORK_MODE)
		*(u32 *)(&ucontrol->value.bytes.data) = map_priv->bt_wb_sel;
	/* Hide any masked bytes to ensure consistent data reporting */
	else if (ret == 0 && params->mask) {
		((u32 *)(&ucontrol->value.bytes.data))[0]
			&= ~params->mask;
	}

	return ret;
}

static int map_bytes_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	int ret = 0, len;
	unsigned int val, mask;
	void *data;

	map_fe_dai_priv = snd_soc_codec_get_drvdata(codec);
	map_priv = map_fe_dai_priv->map_priv;

	len = params->num_regs * codec->val_bytes;

	data = kmemdup(ucontrol->value.bytes.data, len, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	/* firmware virtual register can't be wrotten */
	if ((params->base == MAP_DSP1_FW_REG) ||
		(params->base == MAP_DSP2_FW_REG) ||
		(params->base == MAP_DSP1A_FW_REG))
		goto out;

	val = ((u32 *)data)[0];
	if ((params->base == MAP_BT_WORK_MODE) && (!!val)) {
		map_priv->bt_wb_sel = true;
		goto out;
	} else if ((params->base == MAP_BT_WORK_MODE) && (!val)) {
		map_priv->bt_wb_sel = false;
		goto out;
	}

	/* will finally remove below WA */
	if (!map_priv->b0_fix) {
		if ((params->base == MAP_DSP1_DAC_CTRL_REG) && (val & 0x10))
			map_priv->dsp1_mono_en = true;
		else if ((params->base == MAP_DSP1_DAC_CTRL_REG) &&
							((val & 0x10) == 0))
			map_priv->dsp1_mono_en = false;

		if ((params->base == MAP_DSP2_DAC_CTRL_REG) && (val & 0x10))
			map_priv->dsp2_mono_en = true;
		else if ((params->base == MAP_DSP2_DAC_CTRL_REG) &&
							((val & 0x10) == 0))
			map_priv->dsp2_mono_en = false;

		if ((params->base == MAP_ADC_CTRL_REG) && (val & 0x10))
			map_priv->dsp1a_mono_en = true;
		else if ((params->base == MAP_ADC_CTRL_REG) &&
							((val & 0x10) == 0))
			map_priv->dsp1a_mono_en = false;
	}

	/*
	 * If we've got a mask then we need to preserve the register
	 * bits.  We shouldn't modify the incoming data so take a
	 * copy.
	 */
	if (params->mask) {
		val = map_raw_read(map_priv, params->base);

		/*
		 * WA for map fix: no need to mask bit 4, so that mono mode
		 * can be set freely for dsp1/2/1a
		 */
		if (map_priv->b0_fix && (params->mask == 0x13))
			if ((params->base == MAP_DSP1_DAC_CTRL_REG) ||
				(params->base == MAP_DSP2_DAC_CTRL_REG) ||
				(params->base == MAP_ADC_CTRL_REG))
				params->mask = 0x3;

		val &= params->mask;

		mask = ~params->mask;

		((u32 *)data)[0] &= mask;

		((u32 *)data)[0] |= val;
	}

	if ((params->base == MAP_TOP_CTRL_REG_1) &&
			(!map_priv->path_enabled) && (val & 0x3)) {
		map_be_active(map_priv);
		map_priv->path_enabled = true;
		goto out;
	} else if ((params->base == MAP_TOP_CTRL_REG_1) &&
			map_priv->path_enabled && ((val & 0x3) == 0)) {
		map_be_reset(map_priv);
		map_priv->path_enabled = false;
		goto out;
	} else if (params->base == MAP_TOP_CTRL_REG_1)
		goto out;

	ret = map_raw_bulk_write(map_priv, params->base,
			       data, params->num_regs);
out:
	kfree(data);

	return ret;
}

static int map_snd_soc_bytes_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	int ret;

	map_fe_dai_priv = snd_soc_codec_get_drvdata(codec);
	map_priv = map_fe_dai_priv->map_priv;

	ret = map_raw_bulk_read(map_priv, params->base,
			ucontrol->value.bytes.data,
			params->num_regs);

	/* Hide any masked bytes to ensure consistent data reporting */
	if (ret == 0 && params->mask) {
		switch (codec->val_bytes) {
		case 1:
			ucontrol->value.bytes.data[0] &= ~params->mask;
			break;
		case 2:
			((u16 *)(&ucontrol->value.bytes.data))[0]
				&= cpu_to_be16(~params->mask);
			break;
		case 4:
			((u32 *)(&ucontrol->value.bytes.data))[0]
				&= cpu_to_be32(~params->mask);
			break;
		default:
			return -EINVAL;
		}
	}

#ifdef	CONFIG_SND_TDM_STATIC_ALLOC
	if (params->base == TDM_CLK_ENABLE)
		*(u32 *)(&ucontrol->value.bytes.data) = map_priv->tdm_clk_enabled;
#endif
	return ret;
}

static int map_snd_soc_bytes_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int ret = 0, len;
	unsigned int val, mask;
	void *data;
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;

	map_fe_dai_priv = snd_soc_codec_get_drvdata(codec);
	map_priv = map_fe_dai_priv->map_priv;

	len = params->num_regs * codec->val_bytes;

	data = kmemdup(ucontrol->value.bytes.data, len, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	val = ((u32 *)data)[0];
	if ((params->base == TDM_CLK_ENABLE) && val) {
		/* enable TDM clock */
		if (!map_priv->user_count)
			pr_info("MAP is reset, will not enable TDM clk.\n");
		else
			tdm_clk_enable(map_priv, 1);
		goto out;

	} else if ((params->base == TDM_CLK_ENABLE) && (!val)) {
		/* disable TDM clock */
		if (!map_priv->user_count)
			pr_info("MAP is reset, will not disable TDM clk.\n");
		else
			tdm_clk_enable(map_priv, 0);
		goto out;
	}
#endif

	/*
	 * If we've got a mask then we need to preserve the register
	 * bits.  We shouldn't modify the incoming data so take a
	 * copy.
	 */
	if (params->mask) {
		val = map_raw_read(map_priv, params->base);

		val &= params->mask;

		switch (codec->val_bytes) {
		case 1:
			((u8 *)data)[0] &= ~params->mask;
			((u8 *)data)[0] |= val;
			break;
		case 2:
			mask = ~params->mask;

			((u16 *)data)[0] &= mask;

			((u16 *)data)[0] |= val;
			break;
		case 4:
			mask = ~params->mask;

			((u32 *)data)[0] &= mask;

			((u32 *)data)[0] |= val;
			break;
		default:
			ret = -EINVAL;
			goto out;
		}
	}

	ret = map_raw_bulk_write(map_priv, params->base,
			       data, params->num_regs);

out:
	kfree(data);

	return ret;
}


#define SND_SOC_BYTES_INFO(xname, xbase, xregs, \
	 xhandler_get, xhandler_put) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_bytes_info, .get = xhandler_get, \
	.put = xhandler_put, .private_value =	      \
		((unsigned long)&(struct soc_bytes)           \
		{.base = xbase, .num_regs = xregs }) }

#define SND_SOC_BYTES_INFO_MASK(xname, xbase, xregs, xmask,	\
	 xhandler_get, xhandler_put) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,   \
	.info = snd_soc_bytes_info, .get = xhandler_get, \
	.put = xhandler_put, .private_value =	      \
		((unsigned long)&(struct soc_bytes)           \
		{.base = xbase, .num_regs = xregs,	      \
		 .mask = xmask }) }

#define SND_SOC_BYTES_MAP(xname, xbase, xregs)		      \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,   \
	.info = snd_soc_bytes_info, .get = map_snd_soc_bytes_get, \
	.put = map_snd_soc_bytes_put, .private_value =	      \
		((unsigned long)&(struct soc_bytes)           \
		{.base = xbase, .num_regs = xregs }) }

#define SND_SOC_BYTES_MAP_MASK(xname, xbase, xregs, xmask)	      \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,   \
	.info = snd_soc_bytes_info, .get = map_snd_soc_bytes_get, \
	.put = map_snd_soc_bytes_put, .private_value =	      \
		((unsigned long)&(struct soc_bytes)           \
		{.base = xbase, .num_regs = xregs,	      \
		 .mask = xmask }) }


static const struct snd_kcontrol_new map_snd_controls[] = {
	/*
	 * below dummy controls are between map_widgets and map_controls.
	 * adjust the dummy control number to keep valid widget & control
	 * numid unchanged when adding/reducing new items.
	 */
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG8", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG9", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG10", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG11", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG12", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG13", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG14", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG15", 0, 0),

	SND_SOC_BYTES_MAP("MAP_REVISION", MAP_REV, 1),
	SND_SOC_BYTES_MAP("MAP_LRCLK_RATE_REG", MAP_LRCLK_RATE_REG, 1),
	SND_SOC_BYTES_MAP("MAP_I2S1_CTRL_REG", MAP_I2S1_CTRL_REG, 1),
	SND_SOC_BYTES_MAP("MAP_I2S2_CTRL_REG", MAP_I2S2_CTRL_REG, 1),
	SND_SOC_BYTES_MAP("MAP_I2S3_CTRL_REG", MAP_I2S3_CTRL_REG, 1),
	SND_SOC_BYTES_MAP("MAP_I2S4_CTRL_REG", MAP_I2S4_CTRL_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DEI2S_CTRL_REG", MAP_DEI2S_CTRL_REG, 1),
	SND_SOC_BYTES_MAP("MAP_STATUS_REG_1", MAP_STATUS_REG_1, 1),
	SND_SOC_BYTES_MAP("MAP_STATUS_REG_2", MAP_STATUS_REG_2, 1),
	SND_SOC_BYTES_INFO("MAP_TOP_CTRL_REG_1", MAP_TOP_CTRL_REG_1, 1,
				map_bytes_get, map_bytes_put),
	SND_SOC_BYTES_MAP("MAP_TOP_CTRL_REG_2", MAP_TOP_CTRL_REG_2, 1),
	SND_SOC_BYTES_MAP("MAP_DATAPATH_FLOW_CTRL_REG_1",
			MAP_DATAPATH_FLOW_CTRL_REG_1, 1),
	SND_SOC_BYTES_MAP("MAP_DATAPATH_FLOW_CTRL_REG_2",
			MAP_DATAPATH_FLOW_CTRL_REG_2, 1),
	SND_SOC_BYTES_MAP("MAP_DATAPATH_FLOW_CTRL_REG_3",
			MAP_DATAPATH_FLOW_CTRL_REG_3, 1),
	SND_SOC_BYTES_MAP("MAP_ASRC_CTRL_REG", MAP_ASRC_CTRL_REG, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_1", MAP_TDM_CTRL_REG_1, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_2", MAP_TDM_CTRL_REG_2, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_3", MAP_TDM_CTRL_REG_3, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_4", MAP_TDM_CTRL_REG_4, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_5", MAP_TDM_CTRL_REG_5, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_6", MAP_TDM_CTRL_REG_6, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_7", MAP_TDM_CTRL_REG_7, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_8", MAP_TDM_CTRL_REG_8, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_9", MAP_TDM_CTRL_REG_9, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_10", MAP_TDM_CTRL_REG_10, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_11", MAP_TDM_CTRL_REG_11, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_12", MAP_TDM_CTRL_REG_12, 1),
	SND_SOC_BYTES_MAP("MAP_TDM_CTRL_REG_13", MAP_TDM_CTRL_REG_13, 1),
	SND_SOC_BYTES_MAP("MAP_INTERRUPT_CTRL_REG", MAP_INTERRUPT_CTRL_REG, 1),
	SND_SOC_BYTES_MAP("MAP_I2S1_BCLK_DIV", MAP_I2S1_BCLK_DIV, 1),
	SND_SOC_BYTES_MAP("MAP_I2S2_BCLK_DIV", MAP_I2S2_BCLK_DIV, 1),
	SND_SOC_BYTES_MAP("MAP_I2S3_BCLK_DIV", MAP_I2S3_BCLK_DIV, 1),
	SND_SOC_BYTES_MAP("MAP_I2S4_BCLK_DIV", MAP_I2S4_BCLK_DIV, 1),
	SND_SOC_BYTES_MAP("MAP_I2S_OUT_BCLK_DIV", MAP_I2S_OUT_BCLK_DIV, 1),
	SND_SOC_BYTES_MAP_MASK("MAP_DSP1_DAC_PROCESSING_REG",
			MAP_DSP1_DAC_PROCESSING_REG, 1, 0x3e20),
	SND_SOC_BYTES_INFO_MASK("MAP_DSP1_DAC_CTRL_REG", MAP_DSP1_DAC_CTRL_REG,
				1, 0x13, map_bytes_get, map_bytes_put),
	SND_SOC_BYTES_MAP("MAP_DSP1_DAC_VOLUME",
			MAP_DSP1_DAC_VOLUME, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_ANC_PARAM_U_REG",
			MAP_DSP1_ANC_PARAM_U_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_ANC_PARAM_LAMBA_REG",
			MAP_DSP1_ANC_PARAM_LAMBA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_ANC_PARAM_BETA_REG",
			MAP_DSP1_ANC_PARAM_BETA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_ANC_PARAM_ERRTH_REG",
			MAP_DSP1_ANC_PARAM_ERRTH_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND1_GAIN",
			MAP_DSP1_EQ_BAND1_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND1_CENTER_FREQ",
			MAP_DSP1_EQ_BAND1_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND2_GAIN",
			MAP_DSP1_EQ_BAND2_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND2_CENTER_FREQ",
			MAP_DSP1_EQ_BAND2_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND2_BANDWIDTH",
			MAP_DSP1_EQ_BAND2_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND3_GAIN",
			MAP_DSP1_EQ_BAND3_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND3_CENTER_FREQ",
			MAP_DSP1_EQ_BAND3_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND3_BANDWIDTH",
			MAP_DSP1_EQ_BAND3_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND4_GAIN",
			MAP_DSP1_EQ_BAND4_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND4_CENTER_FREQ",
			MAP_DSP1_EQ_BAND4_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND4_BANDWIDTH",
			MAP_DSP1_EQ_BAND4_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND5_GAIN",
			MAP_DSP1_EQ_BAND5_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND5_CENTER_FREQ",
			MAP_DSP1_EQ_BAND5_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND5_BANDWIDTH",
			MAP_DSP1_EQ_BAND5_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND6_GAIN",
			MAP_DSP1_EQ_BAND6_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND6_CENTER_FREQ",
			MAP_DSP1_EQ_BAND6_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND6_BANDWIDTH",
			MAP_DSP1_EQ_BAND6_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND7_GAIN",
			MAP_DSP1_EQ_BAND7_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND7_CENTER_FREQ",
			MAP_DSP1_EQ_BAND7_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND7_BANDWIDTH",
			MAP_DSP1_EQ_BAND7_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND8_GAIN",
			MAP_DSP1_EQ_BAND8_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_EQ_BAND8_CENTER_FREQ",
			MAP_DSP1_EQ_BAND8_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DAC_DRC_THRESHOLD",
			MAP_DSP1_DAC_DRC_THRESHOLD, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DAC_DRC_OFFSET",
			MAP_DSP1_DAC_DRC_OFFSET, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DAC_DRC_COMPRESSION_RATIO",
			MAP_DSP1_DAC_DRC_COMPRESSION_RATIO, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DAC_DRC_ENERGY_ALPHA_REG",
			MAP_DSP1_DAC_DRC_ENERGY_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DAC_DRC_ATTACK_ALPHA_REG",
			MAP_DSP1_DAC_DRC_ATTACK_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DAC_DRC_DECAY_ALPHA_REG",
			MAP_DSP1_DAC_DRC_DECAY_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DAC_OUTPUT_MIX",
			MAP_DSP1_DAC_OUTPUT_MIX, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_TXRX_MIX_COEF_REG",
			MAP_DSP1_TXRX_MIX_COEF_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_INMIX_COEF_REG",
			MAP_DSP1_INMIX_COEF_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_3D_REG1",
			MAP_DSP1_3D_REG1, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_3D_REG2",
			MAP_DSP1_3D_REG2, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DUMMY_1",
			MAP_DSP1_DUMMY_1, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DUMMY_2",
			MAP_DSP1_DUMMY_2, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DUMMY_3",
			MAP_DSP1_DUMMY_3, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DUMMY_4",
			MAP_DSP1_DUMMY_4, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DUMMY_5",
			MAP_DSP1_DUMMY_5, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1_DSM_SCALING_REG",
			MAP_DSP1_DSM_SCALING_REG, 1),
	SND_SOC_BYTES_MAP_MASK("MAP_DSP2_DAC_PROCESSING_REG",
			MAP_DSP2_DAC_PROCESSING_REG, 1, 0x3e20),
	SND_SOC_BYTES_INFO_MASK("MAP_DSP2_DAC_CTRL_REG", MAP_DSP2_DAC_CTRL_REG,
				1, 0x13, map_bytes_get, map_bytes_put),
	SND_SOC_BYTES_MAP("MAP_DSP2_DAC_VOLUME",
			MAP_DSP2_DAC_VOLUME, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_ANC_PARAM_U_REG",
			MAP_DSP2_ANC_PARAM_U_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_ANC_PARAM_LAMBA_REG",
			MAP_DSP2_ANC_PARAM_LAMBA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_ANC_PARAM_BETA_REG",
			MAP_DSP2_ANC_PARAM_BETA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_ANC_PARAM_ERRTH_REG",
			MAP_DSP2_ANC_PARAM_ERRTH_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND1_GAIN",
			MAP_DSP2_EQ_BAND1_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND1_CENTER_FREQ",
			MAP_DSP2_EQ_BAND1_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND2_GAIN",
			MAP_DSP2_EQ_BAND2_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND2_CENTER_FREQ",
			MAP_DSP2_EQ_BAND2_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND2_BANDWIDTH",
			MAP_DSP2_EQ_BAND2_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND3_GAIN",
			MAP_DSP2_EQ_BAND3_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND3_CENTER_FREQ",
			MAP_DSP2_EQ_BAND3_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND3_BANDWIDTH",
			MAP_DSP2_EQ_BAND3_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND4_GAIN",
			MAP_DSP2_EQ_BAND4_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND4_CENTER_FREQ",
			MAP_DSP2_EQ_BAND4_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND4_BANDWIDTH",
			MAP_DSP2_EQ_BAND4_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND5_GAIN",
			MAP_DSP2_EQ_BAND5_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND5_CENTER_FREQ",
			MAP_DSP2_EQ_BAND5_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND5_BANDWIDTH",
			MAP_DSP2_EQ_BAND5_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND6_GAIN",
			MAP_DSP2_EQ_BAND6_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND6_CENTER_FREQ",
			MAP_DSP2_EQ_BAND6_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND6_BANDWIDTH",
			MAP_DSP2_EQ_BAND6_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND7_GAIN",
			MAP_DSP2_EQ_BAND7_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND7_CENTER_FREQ",
			MAP_DSP2_EQ_BAND7_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND7_BANDWIDTH",
			MAP_DSP2_EQ_BAND7_BANDWIDTH, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND8_GAIN",
			MAP_DSP2_EQ_BAND8_GAIN, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_EQ_BAND8_CENTER_FREQ",
			MAP_DSP2_EQ_BAND8_CENTER_FREQ, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DAC_DRC_THRESHOLD",
			MAP_DSP2_DAC_DRC_THRESHOLD, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DAC_DRC_OFFSET",
			MAP_DSP2_DAC_DRC_OFFSET, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DAC_DRC_COMPRESSION_RATIO",
			MAP_DSP2_DAC_DRC_COMPRESSION_RATIO, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DAC_DRC_ENERGY_ALPHA_REG",
			MAP_DSP2_DAC_DRC_ENERGY_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DAC_DRC_ATTACK_ALPHA_REG",
			MAP_DSP2_DAC_DRC_ATTACK_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DAC_DRC_DECAY_ALPHA_REG",
			MAP_DSP2_DAC_DRC_DECAY_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DAC_OUTPUT_MIX",
			MAP_DSP2_DAC_OUTPUT_MIX, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_TXRX_MIX_COEF_REG",
			MAP_DSP2_TXRX_MIX_COEF_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_INMIX_COEF_REG",
			MAP_DSP2_INMIX_COEF_REG, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_3D_REG1",
			MAP_DSP2_3D_REG1, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_3D_REG2",
			MAP_DSP2_3D_REG2, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DUMMY_1",
			MAP_DSP2_DUMMY_1, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DUMMY_2",
			MAP_DSP2_DUMMY_2, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DUMMY_3",
			MAP_DSP2_DUMMY_3, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DUMMY_4",
			MAP_DSP2_DUMMY_4, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DUMMY_5",
			MAP_DSP2_DUMMY_5, 1),
	SND_SOC_BYTES_MAP("MAP_DSP2_DSM_SCALING_REG",
			MAP_DSP2_DSM_SCALING_REG, 1),
	SND_SOC_BYTES_MAP_MASK("MAP_ADC_PROCESSING_REG",
			MAP_ADC_PROCESSING_REG, 1, 0x2420),
	SND_SOC_BYTES_INFO_MASK("MAP_ADC_CTRL_REG", MAP_ADC_CTRL_REG,
				1, 0x13, map_bytes_get, map_bytes_put),
	SND_SOC_BYTES_MAP("MAP_ADC_VOLUME",
			MAP_ADC_VOLUME, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_ALC_UPPER_THRESHOLD",
			MAP_ADC_ALC_UPPER_THRESHOLD, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_ALC_LOWER_THRESHOLD",
			MAP_ADC_ALC_LOWER_THRESHOLD, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_ALC_OFFSET",
			MAP_ADC_ALC_OFFSET, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_ALC_COMPRESSION_RATIO",
			MAP_ADC_ALC_COMPRESSION_RATIO, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_ALC_ENERGY_ALPHA_REG",
			MAP_ADC_ALC_ENERGY_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_ALC_ATTACK_ALPHA_REG",
			MAP_ADC_ALC_ATTACK_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_ALC_DECAY_ALPHA_REG",
			MAP_ADC_ALC_DECAY_ALPHA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_NOISE_GATE_THRESHOD",
			MAP_ADC_NOISE_GATE_THRESHOD, 1),
	SND_SOC_BYTES_MAP("MAP_ADC_OUTPUT_MIX", MAP_ADC_OUTPUT_MIX, 1),
	SND_SOC_BYTES_MAP("MAP_AEC_PARAM_U_REG", MAP_AEC_PARAM_U_REG, 1),
	SND_SOC_BYTES_MAP("MAP_AEC_PARAM_LAMBA_REG", MAP_AEC_PARAM_LAMBA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_AEC_PARAM_BETA_REG", MAP_AEC_PARAM_BETA_REG, 1),
	SND_SOC_BYTES_MAP("MAP_AEC_PARAM_ERR_TH_REG", MAP_AEC_PARAM_ERR_TH_REG, 1),
	SND_SOC_BYTES_MAP("MAP_WNR_FILTER_COEF", MAP_WNR_FILTER_COEF, 1),
	SND_SOC_BYTES_MAP("MAP_SSL_PARAM_MU", MAP_SSL_PARAM_MU, 1),
	SND_SOC_BYTES_MAP("MAP_INPUT_MIX_REG", MAP_INPUT_MIX_REG, 1),
	SND_SOC_BYTES_MAP("MAP_BF_PARAM_REG1", MAP_BF_PARAM_REG1, 1),
	SND_SOC_BYTES_MAP("MAP_BF_PARAM_REG2", MAP_BF_PARAM_REG2, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1A_DUMMY_1", MAP_DSP1A_DUMMY_1, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1A_DUMMY_2", MAP_DSP1A_DUMMY_2, 1),
	SND_SOC_BYTES_MAP("MAP_DSP1A_DUMMY_3", MAP_DSP1A_DUMMY_3, 1),
	SND_SOC_BYTES_MAP("MAP_DIG_TEST_MUX_CTRL_REG",
			MAP_DIG_TEST_MUX_CTRL_REG, 1),
	SND_SOC_BYTES_MAP("MAP_LOOPBACK_MODES", MAP_LOOPBACK_MODES, 1),
	SND_SOC_BYTES_MAP("MAP_DELAY_BUF_CTRL", MAP_DELAY_BUF_CTRL, 1),
	SND_SOC_BYTES_MAP("MAP_DAC_ANA_MISC", MAP_DAC_ANA_MISC, 1),

	SND_SOC_BYTES_INFO("MAP_DSP1_FW_REG", MAP_DSP1_FW_REG, 1,
				map_bytes_get, map_bytes_put),
	SND_SOC_BYTES_INFO("MAP_DSP2_FW_REG", MAP_DSP2_FW_REG, 1,
				map_bytes_get, map_bytes_put),
	SND_SOC_BYTES_INFO("MAP_DSP1A_FW_REG", MAP_DSP1A_FW_REG, 1,
				map_bytes_get, map_bytes_put),

	SND_SOC_BYTES_INFO("MAP_BT_WORK_MODE", MAP_BT_WORK_MODE, 1,
				map_bytes_get, map_bytes_put),
	/*
	 * The following dummy reg is only for adding spcae with
	 * other component.
	 */
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	SND_SOC_BYTES_MAP("TDM_CLK_ENABLE", TDM_CLK_ENABLE, 1),
#else
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG2", 0, 0),
#endif
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG3", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG4", 0, 0),
	SND_SOC_BYTES_MAP("MAP_DUMMY_REG5", 0, 0),
};

/* dac1 input mux selection */
/* d1in1 */
static const char * const map_dac_input_d1in1_mux_txt[] = {
	"AOUT1", "D1AIN1", "D1AIN2", "D2OUT"
};
static const struct soc_enum dac_input_d1in1_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			6,
			4,
			map_dac_input_d1in1_mux_txt);

static const struct snd_kcontrol_new dac_input_d1in1_mux =
	SOC_DAPM_ENUM("dac input d1in1 mux",
			dac_input_d1in1_mux_enum);
/* d1in2 */
static const char * const map_dac_input_d1in2_mux_txt[] = {
	"AOUT2", "D1AIN1", "D1AIN2", "D2OUT"
};
static const struct soc_enum dac_input_d1in2_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			4,
			4,
			map_dac_input_d1in2_mux_txt);

static const struct snd_kcontrol_new dac_input_d1in2_mux =
	SOC_DAPM_ENUM("dac input d1in2 mux",
			dac_input_d1in2_mux_enum);

/* d1in3 */
static const char * const map_dac_input_d1in3_mux_txt[] = {
	"AOUT3", "D1AIN1", "D1AIN2", "D2OUT"
};
static const struct soc_enum dac_input_d1in3_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			2,
			4,
			map_dac_input_d1in3_mux_txt);

static const struct snd_kcontrol_new dac_input_d1in3_mux =
	SOC_DAPM_ENUM("dac input d1in3 mux",
			dac_input_d1in3_mux_enum);

/* d1in4 */
static const char * const map_dac_input_d1in4_mux_txt[] = {
	"AOUT4", "D1AIN1", "D1AIN2", "D2OUT"
};
static const struct soc_enum dac_input_d1in4_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			0,
			4,
			map_dac_input_d1in4_mux_txt);

static const struct snd_kcontrol_new dac_input_d1in4_mux =
	SOC_DAPM_ENUM("dac input d1in4 mux",
			dac_input_d1in4_mux_enum);

/* dac2 input mux selection */
/* d2in1 */
static const char * const map_dac_input_d2in1_mux_txt[] = {
	"AOUT1", "D1AIN1", "D1AIN2", "D1OUT"
};
static const struct soc_enum dac_input_d2in1_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			14,
			4,
			map_dac_input_d2in1_mux_txt);

static const struct snd_kcontrol_new dac_input_d2in1_mux =
	SOC_DAPM_ENUM("dac input d2in1 mux",
			dac_input_d2in1_mux_enum);
/* d2in2 */
static const char * const map_dac_input_d2in2_mux_txt[] = {
	"AOUT2", "D1AIN1", "D1AIN2", "D1OUT"
};
static const struct soc_enum dac_input_d2in2_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			12,
			4,
			map_dac_input_d2in2_mux_txt);

static const struct snd_kcontrol_new dac_input_d2in2_mux =
	SOC_DAPM_ENUM("dac input d2in2 mux",
			dac_input_d2in2_mux_enum);

/* d2in3 */
static const char * const map_dac_input_d2in3_mux_txt[] = {
	"AOUT3", "D1AIN1", "D1AIN2", "D1OUT"
};
static const struct soc_enum dac_input_d2in3_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			10,
			4,
			map_dac_input_d2in3_mux_txt);

static const struct snd_kcontrol_new dac_input_d2in3_mux =
	SOC_DAPM_ENUM("dac input d2in3 mux",
			dac_input_d2in3_mux_enum);

/* d2in4 */
static const char * const map_dac_input_d2in4_mux_txt[] = {
	"AOUT4", "D1AIN1", "D1AIN2", "D1OUT"
};
static const struct soc_enum dac_input_d2in4_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			8,
			4,
			map_dac_input_d2in4_mux_txt);

static const struct snd_kcontrol_new dac_input_d2in4_mux =
	SOC_DAPM_ENUM("dac input d2in4 mux",
			dac_input_d2in4_mux_enum);

/* ADC output mux selection */
/* AIN1 */
static const char * const map_adc_output_ain_mux_txt[] = {
	"D1AOUT", "AOUT1", "AOUT2", "AOUT3", "AOUT4",
	"MIC1", "MIC2", "D1OUT", "D2OUT"
};
static const struct soc_enum adc_output_ain1_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_1,
			12,
			9,
			map_adc_output_ain_mux_txt);

static const struct snd_kcontrol_new adc_output_ain1_mux =
	SOC_DAPM_ENUM("adc output ain1 mux",
			adc_output_ain1_mux_enum);

/* AIN2 */
static const struct soc_enum adc_output_ain2_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_1,
			8,
			9,
			map_adc_output_ain_mux_txt);

static const struct snd_kcontrol_new adc_output_ain2_mux =
	SOC_DAPM_ENUM("adc output ain2 mux",
			adc_output_ain2_mux_enum);

/* AIN3 */
static const struct soc_enum adc_output_ain3_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_1,
			4,
			9,
			map_adc_output_ain_mux_txt);

static const struct snd_kcontrol_new adc_output_ain3_mux =
	SOC_DAPM_ENUM("adc output ain3 mux",
			adc_output_ain3_mux_enum);

/* AIN4 */
static const struct soc_enum adc_output_ain4_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_1,
			0,
			9,
			map_adc_output_ain_mux_txt);

static const struct snd_kcontrol_new adc_output_ain4_mux =
	SOC_DAPM_ENUM("adc output ain4 mux",
			adc_output_ain4_mux_enum);

/* DAC output mux */
/* OUT1 */
static const char * const map_dac_output_out1_mux_txt[] = {
	"D1OUT", "D1IN1", "D1IN2", "D1IN3",
	"D1IN4", "D2OUT", "DSPMIX", "Reserved"
};
static const struct soc_enum dac_output_out1_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_1,
			19,
			8,
			map_dac_output_out1_mux_txt);

static const struct snd_kcontrol_new dac_output_out1_mux =
	SOC_DAPM_ENUM("dac output out1 mux",
			dac_output_out1_mux_enum);

/* OUT2 */
static const char * const map_dac_output_out2_mux_txt[] = {
	"D2OUT", "D2IN1", "D2IN2", "D2IN3",
	"D2IN4", "D1OUT", "DSPMIX", "Reserved"
};
static const struct soc_enum dac_output_out2_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_1,
			16,
			8,
			map_dac_output_out2_mux_txt);

static const struct snd_kcontrol_new dac_output_out2_mux =
	SOC_DAPM_ENUM("dac output out2 mux",
			dac_output_out2_mux_enum);

/* dsp1 enable */
static const struct snd_kcontrol_new dsp1_en_control =
	SOC_DAPM_SINGLE_VIRTUAL("Switch", 0, 0, 1, 0);

static const struct snd_kcontrol_new dac1_d1in1_inmix_control =
	SOC_DAPM_SINGLE("Switch", MAP_DSP1_DAC_PROCESSING_REG, 9, 1, 0);

static const struct snd_kcontrol_new dac1_d1in2_inmix_control =
	SOC_DAPM_SINGLE("Switch", MAP_DSP1_DAC_PROCESSING_REG, 10, 1, 0);

static const struct snd_kcontrol_new dac1_d1in3_inmix_control =
	SOC_DAPM_SINGLE("Switch", MAP_DSP1_DAC_PROCESSING_REG, 11, 1, 0);

static const struct snd_kcontrol_new dac1_d1in4_inmix_control =
	SOC_DAPM_SINGLE("Switch", MAP_DSP1_DAC_PROCESSING_REG, 12, 1, 0);

/* DSP2 enable */
static const struct snd_kcontrol_new dsp2_en_control =
	SOC_DAPM_SINGLE_VIRTUAL("Switch", 0, 0, 1, 0);

static const struct snd_kcontrol_new dac2_d1in1_inmix_control =
	SOC_DAPM_SINGLE("Switch", MAP_DSP2_DAC_PROCESSING_REG, 9, 1, 0);

static const struct snd_kcontrol_new dac2_d1in2_inmix_control =
	SOC_DAPM_SINGLE("Switch", MAP_DSP2_DAC_PROCESSING_REG, 10, 1, 0);

static const struct snd_kcontrol_new dac2_d1in3_inmix_control =
	SOC_DAPM_SINGLE("Switch", MAP_DSP2_DAC_PROCESSING_REG, 11, 1, 0);

static const struct snd_kcontrol_new dac2_d1in4_inmix_control =
	SOC_DAPM_SINGLE("Switch", MAP_DSP2_DAC_PROCESSING_REG, 12, 1, 0);

/* virtual mux for inmix due to there is no control register */
static const char * const dac1_in_mux_text[] = {
	"in mix",
	"zero input",
};

static const struct soc_enum dac1_in_mux_enum =
	SOC_ENUM_SINGLE(0, 0, 2, dac1_in_mux_text);

static const struct snd_kcontrol_new dac1_in_mux =
	SOC_DAPM_ENUM_VIRT("DAC1 in mux", dac1_in_mux_enum);

static const char * const dac2_in_mux_text[] = {
	"in mix",
	"zero input",
};

static const struct soc_enum dac2_in_mux_enum =
	SOC_ENUM_SINGLE(0, 0, 2, dac2_in_mux_text);

static const struct snd_kcontrol_new dac2_in_mux =
	SOC_DAPM_ENUM_VIRT("DAC2 in mux", dac2_in_mux_enum);

/* DSP1 TXRX mux: 0 for not enable txrx mixer, 1 for enable it */
static const char * const map_dac1_txrx_mux_txt[] = {
	"inmix", "txrx"
};
static const struct soc_enum dac1_txrx_mux_enum =
	SOC_ENUM_SINGLE(MAP_DSP1_DAC_PROCESSING_REG,
			13,
			2,
			map_dac1_txrx_mux_txt);

static const struct snd_kcontrol_new dac1_txrx_mux =
	SOC_DAPM_ENUM("dac1 txrx mux",
			dac1_txrx_mux_enum);

/* DSP2 TXRX mux: 0 for not enable txrx mixer, 1 for enable it */
static const char * const map_dac2_txrx_mux_txt[] = {
	"inmix", "txrx"
};
static const struct soc_enum dac2_txrx_mux_enum =
	SOC_ENUM_SINGLE(MAP_DSP2_DAC_PROCESSING_REG,
			13,
			2,
			map_dac2_txrx_mux_txt);

static const struct snd_kcontrol_new dac2_txrx_mux =
	SOC_DAPM_ENUM("dac2 txrx mux",
			dac2_txrx_mux_enum);

/* ADC input mux */
/* D1AIN1 */
static const char * const map_adc_input_d1ain1_mux_txt[] = {
	"MIC1", "AOUT1", "AOUT2", "AOUT3", "AOUT4"
};
static const struct soc_enum adc_input_d1ain1_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			19,
			5,
			map_adc_input_d1ain1_mux_txt);

static const struct snd_kcontrol_new adc_input_d1ain1_mux =
	SOC_DAPM_ENUM("adc input d1ain1 mux",
			adc_input_d1ain1_mux_enum);

/* D1AIN2 */
static const char * const map_adc_input_d1ain2_mux_txt[] = {
	"MIC2", "AOUT1", "AOUT2", "AOUT3", "AOUT4"
};
static const struct soc_enum adc_input_d1ain2_mux_enum =
	SOC_ENUM_SINGLE(MAP_DATAPATH_FLOW_CTRL_REG_2,
			16,
			5,
			map_adc_input_d1ain2_mux_txt);

static const struct snd_kcontrol_new adc_input_d1ain2_mux =
	SOC_DAPM_ENUM("adc input d1ain2 mux",
			adc_input_d1ain2_mux_enum);

/* ADC DSP1A enable */
static const struct snd_kcontrol_new dsp1a_en_control =
	SOC_DAPM_SINGLE_VIRTUAL("Switch", 0, 0, 1, 0);

/* ADC_in1_mix enable */
static const struct snd_kcontrol_new adc_in1_mix_en_control =
	SOC_DAPM_SINGLE("Switch", MAP_ADC_PROCESSING_REG, 10, 1, 0);

/* ADC_in2_mix enable */
static const struct snd_kcontrol_new adc_in2_mix_en_control =
	SOC_DAPM_SINGLE("Switch", MAP_ADC_PROCESSING_REG, 13, 1, 0);

/* mux for ADC inmix */
static const char * const adc_in_mux_text[] = {
	"adc in mix",
	"zero input",
};

static const struct soc_enum adc_in_mux_enum =
	SOC_ENUM_SINGLE(0, 0, 2, adc_in_mux_text);

static const struct snd_kcontrol_new adc_in_mux =
	SOC_DAPM_ENUM_VIRT("ADC in mux", adc_in_mux_enum);

/* out1->hs enable */
static const struct snd_kcontrol_new out1_hs_en_control =
	SOC_DAPM_SINGLE_VIRTUAL("Switch", 0, 0, 1, 0);

/* out1->spkr enable */
static const struct snd_kcontrol_new out1_spkr_en_control =
	SOC_DAPM_SINGLE_VIRTUAL("Switch", 0, 0, 1, 0);

/* ADC output out3 -> i2s3_bt_vc -> BT_VC_DL enable */
static const struct snd_kcontrol_new i2s3_bt_vc_control =
	SOC_DAPM_SINGLE_VIRTUAL("Switch", 0, 0, 1, 0);

/* mux for I2S3 function select */
static const char * const i2s3_func_sel_mux_text[] = {
	"i2s3_fm",
	"i2s3_bt",
};

static const struct soc_enum i2s3_func_sel_mux_enum =
	SOC_ENUM_SINGLE(0, 0, 2, i2s3_func_sel_mux_text);

static const struct snd_kcontrol_new i2s3_func_sel_mux =
	SOC_DAPM_ENUM_VIRT("i2s3_func_sel", i2s3_func_sel_mux_enum);

static const struct snd_soc_dapm_widget map_dapm_widgets[] = {
	SND_SOC_DAPM_VIRT_MUX("i2s3_func_sel", SND_SOC_NOPM, 0, 0, &i2s3_func_sel_mux),

	SND_SOC_DAPM_VIRT_SWITCH("i2s3_bt_vc", SND_SOC_NOPM, 0, 0,
				&i2s3_bt_vc_control),

	SND_SOC_DAPM_ADC("AOUT3 record", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_VIRT_SWITCH("out1_hs_en", SND_SOC_NOPM, 0, 0,
				&out1_hs_en_control),

	SND_SOC_DAPM_VIRT_SWITCH("out1_spkr_en", SND_SOC_NOPM, 0, 0,
				&out1_spkr_en_control),

	/* DAC1 input mux */
	SND_SOC_DAPM_MUX("DAC input d1in1", SND_SOC_NOPM,
			0, 0, &dac_input_d1in1_mux),
	SND_SOC_DAPM_MUX("DAC input d1in2", SND_SOC_NOPM,
			0, 0, &dac_input_d1in2_mux),
	SND_SOC_DAPM_MUX("DAC input d1in3", SND_SOC_NOPM,
			0, 0, &dac_input_d1in3_mux),
	SND_SOC_DAPM_MUX("DAC input d1in4", SND_SOC_NOPM,
			0, 0, &dac_input_d1in4_mux),

	/* DAC2 input mux */
	SND_SOC_DAPM_MUX("DAC input d2in1", SND_SOC_NOPM,
			0, 0, &dac_input_d2in1_mux),
	SND_SOC_DAPM_MUX("DAC input d2in2", SND_SOC_NOPM,
			0, 0, &dac_input_d2in2_mux),
	SND_SOC_DAPM_MUX("DAC input d2in3", SND_SOC_NOPM,
			0, 0, &dac_input_d2in3_mux),
	SND_SOC_DAPM_MUX("DAC input d2in4", SND_SOC_NOPM,
			0, 0, &dac_input_d2in4_mux),

	/* full zero input for dsp1 and dsp2 */
	SND_SOC_DAPM_DAC("zero input1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("zero input2", NULL, SND_SOC_NOPM, 0, 0),

	/* DAC is transparent for dapm */
	SND_SOC_DAPM_DAC("DAC1 out", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC2 out", NULL, SND_SOC_NOPM, 0, 0),

	/* DAC output mux */
	SND_SOC_DAPM_MUX("DAC1 output out1", SND_SOC_NOPM,
			0, 0, &dac_output_out1_mux),
	SND_SOC_DAPM_MUX("DAC2 output out2", SND_SOC_NOPM,
			0, 0, &dac_output_out2_mux),

	/* ADC input mux */
	SND_SOC_DAPM_MUX("ADC input d1ain1", SND_SOC_NOPM,
			0, 0, &adc_input_d1ain1_mux),
	SND_SOC_DAPM_MUX("ADC input d1ain2", SND_SOC_NOPM,
			0, 0, &adc_input_d1ain2_mux),

	SND_SOC_DAPM_VIRT_SWITCH("dsp1a_enable", SND_SOC_NOPM, 0, 0,
				&dsp1a_en_control),

	SND_SOC_DAPM_MIXER("ADC in mix", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_VIRT_MUX("ADC in mux", SND_SOC_NOPM, 0, 0, &adc_in_mux),

	/* ADC is transparent for dapm */
	SND_SOC_DAPM_ADC("ADC out", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC input1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC input2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC input3", NULL, SND_SOC_NOPM, 0, 0),

	/* ADC output mux */
	SND_SOC_DAPM_MUX("ADC output ain1", SND_SOC_NOPM,
			0, 0, &adc_output_ain1_mux),
	SND_SOC_DAPM_MUX("ADC output ain2", SND_SOC_NOPM,
			0, 0, &adc_output_ain2_mux),
	SND_SOC_DAPM_MUX("ADC output ain3", SND_SOC_NOPM,
			0, 0, &adc_output_ain3_mux),
	SND_SOC_DAPM_MUX("ADC output ain4", SND_SOC_NOPM,
			0, 0, &adc_output_ain4_mux),

	SND_SOC_DAPM_VIRT_SWITCH("dsp1_enable", SND_SOC_NOPM, 0, 0,
				&dsp1_en_control),

	SND_SOC_DAPM_SWITCH("d1in1_mix_enable", SND_SOC_NOPM, 0, 0,
				&dac1_d1in1_inmix_control),
	SND_SOC_DAPM_SWITCH("d1in2_mix_enable", SND_SOC_NOPM, 0, 0,
				&dac1_d1in2_inmix_control),
	SND_SOC_DAPM_SWITCH("d1in3_mix_enable", SND_SOC_NOPM, 0, 0,
				&dac1_d1in3_inmix_control),
	SND_SOC_DAPM_SWITCH("d1in4_mix_enable", SND_SOC_NOPM, 0, 0,
				&dac1_d1in4_inmix_control),

	SND_SOC_DAPM_MIXER("DAC1 in mix", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_VIRT_MUX("DAC1 in mux", SND_SOC_NOPM, 0, 0, &dac1_in_mux),

	/* txrx */
	SND_SOC_DAPM_MIXER("DAC1 txrx mix", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("DAC1 txrx mux", SND_SOC_NOPM,
			0, 0, &dac1_txrx_mux),

	SND_SOC_DAPM_MIXER("DAC dspmix",
				SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_VIRT_SWITCH("dsp2_enable", SND_SOC_NOPM, 0, 0,
				&dsp2_en_control),

	SND_SOC_DAPM_SWITCH("d2in1_mix_enable", SND_SOC_NOPM, 0, 0,
				&dac2_d1in1_inmix_control),
	SND_SOC_DAPM_SWITCH("d2in2_mix_enable", SND_SOC_NOPM, 0, 0,
				&dac2_d1in2_inmix_control),
	SND_SOC_DAPM_SWITCH("d2in3_mix_enable", SND_SOC_NOPM, 0, 0,
				&dac2_d1in3_inmix_control),
	SND_SOC_DAPM_SWITCH("d2in4_mix_enable", SND_SOC_NOPM, 0, 0,
				&dac2_d1in4_inmix_control),

	SND_SOC_DAPM_MIXER("DAC2 in mix", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_VIRT_MUX("DAC2 in mux", SND_SOC_NOPM, 0, 0, &dac2_in_mux),

	/* txrx */
	SND_SOC_DAPM_MIXER("DAC2 txrx mix", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("DAC2 txrx mux", SND_SOC_NOPM,
			0, 0, &dac2_txrx_mux),

	/* adc in mix */
	SND_SOC_DAPM_SWITCH("d1ain1_mix_enable", SND_SOC_NOPM, 0, 0,
				&adc_in1_mix_en_control),
	SND_SOC_DAPM_SWITCH("d1ain2_mix_enable", SND_SOC_NOPM, 0, 0,
				&adc_in2_mix_en_control),
};

static const struct snd_soc_dapm_route map_intercon[] = {
	/* ADC input */
	{"ADC input d1ain1", "MIC1", "ADC input1"},
	{"ADC input d1ain1", "AOUT1", "MM_DL1"},
	{"ADC input d1ain1", "AOUT4", "MM_DL2"},
	{"ADC input d1ain1", "AOUT2", "VC_DL"},
	{"ADC input d1ain1", "AOUT3", "ADC input3"},

	{"ADC input d1ain2", "MIC2", "ADC input2"},
	{"ADC input d1ain2", "AOUT1", "MM_DL1"},
	{"ADC input d1ain2", "AOUT4", "MM_DL2"},
	{"ADC input d1ain2", "AOUT2", "VC_DL"},
	{"ADC input d1ain2", "AOUT3", "ADC input3"},

	{"d1ain1_mix_enable", "Switch", "ADC input d1ain1"},
	{"d1ain2_mix_enable", "Switch", "ADC input d1ain2"},

	{"ADC in mix", NULL, "d1ain1_mix_enable"},
	{"ADC in mix", NULL, "d1ain2_mix_enable"},

	{"ADC in mux", "adc in mix", "ADC in mix"},
	{"ADC in mux", "zero input", "zero input1"},

	{"dsp1a_enable", "Switch", "ADC in mux"},

	{"ADC out", NULL, "dsp1a_enable"},

	/* AIN1 mux */
	{"ADC output ain1", "D1AOUT", "ADC out"},
	{"ADC output ain1", "AOUT1", "MM_DL1"},
	{"ADC output ain1", "AOUT3", "AOUT3 record"},
	{"ADC output ain1", "AOUT2", "VC_DL"},
	{"ADC output ain1", "AOUT4", "MM_DL2"},
	{"ADC output ain1", "MIC1", "ADC input1"},
	{"ADC output ain1", "MIC2", "ADC input2"},
	{"ADC output ain1", "D1OUT", "DAC1 out"},
	{"ADC output ain1", "D2OUT", "DAC2 out"},

	/* AIN2 mux */
	{"ADC output ain2", "D1AOUT", "ADC out"},
	{"ADC output ain2", "AOUT1", "MM_DL1"},
	{"ADC output ain2", "AOUT3", "AOUT3 record"},
	{"ADC output ain2", "AOUT2", "VC_DL"},
	{"ADC output ain2", "AOUT4", "MM_DL2"},
	{"ADC output ain2", "MIC1", "ADC input1"},
	{"ADC output ain2", "MIC2", "ADC input2"},
	{"ADC output ain2", "D1OUT", "DAC1 out"},
	{"ADC output ain2", "D2OUT", "DAC2 out"},

	/* AIN3 mux */
	{"ADC output ain3", "D1AOUT", "ADC out"},
	{"ADC output ain3", "AOUT1", "MM_DL1"},
	{"ADC output ain3", "AOUT3", "AOUT3 record"},
	{"ADC output ain3", "AOUT2", "VC_DL"},
	{"ADC output ain3", "AOUT4", "MM_DL2"},
	{"ADC output ain3", "MIC1", "ADC input1"},
	{"ADC output ain3", "MIC2", "ADC input2"},
	{"ADC output ain3", "D1OUT", "DAC1 out"},
	{"ADC output ain3", "D2OUT", "DAC2 out"},

	/* AIN4 mux */
	{"ADC output ain4", "D1AOUT", "ADC out"},
	{"ADC output ain4", "AOUT1", "MM_DL1"},
	{"ADC output ain4", "AOUT3", "AOUT3 record"},
	{"ADC output ain4", "AOUT2", "VC_DL"},
	{"ADC output ain4", "AOUT4", "MM_DL2"},
	{"ADC output ain4", "MIC1", "ADC input1"},
	{"ADC output ain4", "MIC2", "ADC input2"},
	{"ADC output ain4", "D1OUT", "DAC1 out"},
	{"ADC output ain4", "D2OUT", "DAC2 out"},

	{"MM_UL1", NULL, "ADC output ain1"},
	{"MM_UL2", NULL, "ADC output ain4"},
	{"VC_UL", NULL, "ADC output ain2"},

	{"BT_UL", NULL, "ADC output ain3"},
	{"i2s3_bt_vc", "Switch", "ADC output ain3"},

	{"i2s3_func_sel", "i2s3_fm", "FM_DL"},
	{"i2s3_func_sel", "i2s3_bt", "BT_DL"},

	/* DAC1 input */
	{"DAC input d1in1", "AOUT1", "MM_DL1"},
	{"DAC input d1in1", "D1AIN1", "ADC input d1ain1"},
	{"DAC input d1in1", "D1AIN2", "ADC input d1ain2"},
	{"DAC input d1in1", "D2OUT", "DAC2 out"},

	{"DAC input d1in2", "AOUT2", "VC_DL"},
	{"DAC input d1in2", "D1AIN1", "ADC input d1ain1"},
	{"DAC input d1in2", "D1AIN2", "ADC input d1ain2"},
	{"DAC input d1in2", "D2OUT", "DAC2 out"},

	{"DAC input d1in3", "AOUT3", "i2s3_func_sel"},
	{"DAC input d1in3", "D1AIN1", "ADC input d1ain1"},
	{"DAC input d1in3", "D1AIN2", "ADC input d1ain2"},
	{"DAC input d1in3", "D2OUT", "DAC2 out"},

	{"DAC input d1in4", "AOUT4", "MM_DL2"},
	{"DAC input d1in4", "D1AIN1", "ADC input d1ain1"},
	{"DAC input d1in4", "D1AIN2", "ADC input d1ain2"},
	{"DAC input d1in4", "D2OUT", "DAC2 out"},

	{"d1in1_mix_enable", "Switch", "DAC input d1in1"},
	{"d1in2_mix_enable", "Switch", "DAC input d1in2"},
	{"d1in3_mix_enable", "Switch", "DAC input d1in3"},
	{"d1in4_mix_enable", "Switch", "DAC input d1in4"},

	{"DAC1 in mix", NULL, "d1in1_mix_enable"},
	{"DAC1 in mix", NULL, "d1in2_mix_enable"},
	{"DAC1 in mix", NULL, "d1in3_mix_enable"},
	{"DAC1 in mix", NULL, "d1in4_mix_enable"},

	{"DAC1 in mux", "zero input", "zero input1"},
	{"DAC1 in mux", "in mix", "DAC1 in mix"},

	/* txrx */
	{"DAC1 txrx mix", NULL, "DAC1 in mux"},
	{"DAC1 txrx mix", NULL, "ADC out"},
	{"DAC1 txrx mux", "inmix", "DAC1 in mux"},
	{"DAC1 txrx mux", "txrx", "DAC1 txrx mix"},

	/* DAC1 is transparent for dapm */
	{"dsp1_enable", "Switch", "DAC1 txrx mux"},
	{"DAC1 out", NULL, "dsp1_enable"},

	/* DAC2 input */
	{"DAC input d2in1", "AOUT1", "MM_DL1"},
	{"DAC input d2in1", "D1AIN1", "ADC input d1ain1"},
	{"DAC input d2in1", "D1AIN2", "ADC input d1ain2"},
	{"DAC input d2in1", "D1OUT", "DAC1 out"},

	{"DAC input d2in2", "AOUT2", "VC_DL"},
	{"DAC input d2in2", "D1AIN1", "ADC input d1ain1"},
	{"DAC input d2in2", "D1AIN2", "ADC input d1ain2"},
	{"DAC input d2in2", "D1OUT", "DAC1 out"},

	{"DAC input d2in3", "AOUT3", "i2s3_func_sel"},
	{"DAC input d2in3", "D1AIN1", "ADC input d1ain1"},
	{"DAC input d2in3", "D1AIN2", "ADC input d1ain2"},
	{"DAC input d2in3", "D1OUT", "DAC1 out"},

	{"DAC input d2in4", "AOUT4", "MM_DL2"},
	{"DAC input d2in4", "D1AIN1", "ADC input d1ain1"},
	{"DAC input d2in4", "D1AIN2", "ADC input d1ain2"},
	{"DAC input d2in4", "D1OUT", "DAC1 out"},

	{"d2in1_mix_enable", "Switch", "DAC input d2in1"},
	{"d2in2_mix_enable", "Switch", "DAC input d2in2"},
	{"d2in3_mix_enable", "Switch", "DAC input d2in3"},
	{"d2in4_mix_enable", "Switch", "DAC input d2in4"},

	{"DAC2 in mix", NULL, "d2in1_mix_enable"},
	{"DAC2 in mix", NULL, "d2in2_mix_enable"},
	{"DAC2 in mix", NULL, "d2in3_mix_enable"},
	{"DAC2 in mix", NULL, "d2in4_mix_enable"},

	{"DAC2 in mux", "zero input", "zero input2"},
	{"DAC2 in mux", "in mix", "DAC2 in mix"},

	/* txrx */
	{"DAC2 txrx mix", NULL, "DAC2 in mux"},
	{"DAC2 txrx mix", NULL, "ADC out"},
	{"DAC2 txrx mux", "inmix", "DAC2 in mux"},
	{"DAC2 txrx mux", "txrx", "DAC2 txrx mix"},

	/* DAC1 is transparent for dapm */
	{"dsp2_enable", "Switch", "DAC2 txrx mux"},
	{"DAC2 out", NULL, "dsp2_enable"},

	/* dsp mixer */
	{"DAC dspmix", NULL, "DAC1 out"},
	{"DAC dspmix", NULL, "DAC2 out"},

	/* DAC1 output part */
	{"DAC1 output out1", "D1OUT", "DAC1 out"},
	{"DAC1 output out1", "D1IN1", "DAC input d1in1"},
	{"DAC1 output out1", "D1IN2", "DAC input d1in2"},
	{"DAC1 output out1", "D1IN3", "DAC input d1in3"},
	{"DAC1 output out1", "D1IN4", "DAC input d1in4"},
	{"DAC1 output out1", "D2OUT", "DAC2 out"},
	{"DAC1 output out1", "DSPMIX", "DAC dspmix"},

	{"out1_hs_en", "Switch", "DAC1 output out1"},
	{"out1_spkr_en", "Switch", "DAC1 output out1"},

	/* DAC2 output part */
	{"DAC2 output out2", "D2OUT", "DAC2 out"},
	{"DAC2 output out2", "D2IN1", "DAC input d2in1"},
	{"DAC2 output out2", "D2IN2", "DAC input d2in2"},
	{"DAC2 output out2", "D2IN3", "DAC input d2in3"},
	{"DAC2 output out2", "D2IN4", "DAC input d2in4"},
	{"DAC2 output out2", "D1OUT", "DAC1 out"},
	{"DAC2 output out2", "DSPMIX", "DAC dspmix"},
};

static int map_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = codec->card;

	snd_soc_dapm_new_controls(dapm, map_dapm_widgets,
				ARRAY_SIZE(map_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, map_intercon, ARRAY_SIZE(map_intercon));

	snd_soc_dapm_new_widgets(card);
	pr_info("MAP: add widgets and routes\n");

	return 0;
}

/* set map interface source clock */
static int map_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	/*
	 * we need to set bit28 together with mn div currently
	 * in future with optimized clock framework, we'll switch
	 * to use clock framework API again. like below:
	 * return map_i2s_sysclk(codec_dai->id - 1, clk_id);
	 */
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	u32 reg, val;

	map_fe_dai_priv = snd_soc_dai_get_drvdata(codec_dai);
	map_priv = map_fe_dai_priv->map_priv;
	if (map_fe_dai_priv->i2s_config[codec_dai->id - 1])
		return 0;

	/*
	 * Per DE: need to set I2S_OUT bit clock for 48K sample rate
	 * before configuring other I2S bit clock
	 */
	map_raw_write(map_priv, MAP_I2S_OUT_BCLK_DIV, 0x10010030);

	switch (codec_dai->id) {
	case 1:
		reg =	MAP_I2S1_BCLK_DIV;
		break;
	case 2:
		reg =	MAP_I2S2_BCLK_DIV;
		break;
	case 3:
	case 5:
		reg =	MAP_I2S3_BCLK_DIV;
		break;
	case 4:
		reg =	MAP_I2S4_BCLK_DIV;
		break;
	default:
		return -EINVAL;
	}

	/* this value will be changed if apll1 fclk changes */
	if (clk_id == APLL_32K) {
		switch (freq) {
		case 8000:
			val = 0x10010120;
			break;
		case 16000:
			val = 0x10010090;
			break;
		case 32000:
			val = 0x10010048;
			break;
		case 44100:
			val = 0x11b95a00;
			break;
		case 48000:
			val = 0x10010030;
			break;
		case 96000:
			val = 0x10010018;
			break;
		default:
			return -EINVAL;
		}
	} else if (clk_id == APLL_26M) {
		switch (freq) {
		case 8000:
			val = 0x10080659;
			break;
		case 16000:
			val = 0x10100659;
			break;
		case 32000:
			val = 0x10200659;
			break;
		case 44100:
			val = 0x13721fbd;
			break;
		case 48000:
			val = 0x10300659;
		case 96000:
			val = 0x10600659;
		default:
			return -EINVAL;
		}
	} else
		return -EINVAL;

	map_raw_write(map_priv, reg, val);

	return 0;
}

/*
 * need to consider use clkdiv or add clock source for these interfaces
 */
static int map_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	unsigned int addr;

	map_fe_dai_priv = snd_soc_dai_get_drvdata(codec_dai);
	map_priv = map_fe_dai_priv->map_priv;
	if (map_fe_dai_priv->i2s_config[codec_dai->id - 1])
		return 0;

	switch (codec_dai->id) {
	case 1:
		addr =	MAP_I2S1_BCLK_DIV;
		break;
	case 2:
		addr =	MAP_I2S2_BCLK_DIV;
		break;
	case 3:
	case 5:
		addr =	MAP_I2S3_BCLK_DIV;
		break;
	case 4:
		addr =	MAP_I2S4_BCLK_DIV;
		break;
	default:
		return -EINVAL;
	}
	/* set dai divider <0:27> */
	map_raw_write(map_priv, addr, div);

	return 0;
}

static int map_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	unsigned int inf = 0, addr, port;

	map_fe_dai_priv = snd_soc_dai_get_drvdata(dai);
	map_priv = map_fe_dai_priv->map_priv;
	if (map_fe_dai_priv->i2s_config[dai->id - 1])
		return 0;

	port = dai->id;
	switch (dai->id) {
	case 1:
		addr =	MAP_I2S1_CTRL_REG;
		break;
	case 2:
		addr =	MAP_I2S2_CTRL_REG;
		/* i2s3's pcm width bit will affect i2s2 */
		inf = map_raw_read(map_priv, MAP_I2S3_CTRL_REG);
		inf |= MAP_PCM_WIDTH_SEL;
		map_raw_write(map_priv, MAP_I2S3_CTRL_REG, inf);
		break;
	case 3:
	case 5:
		addr =	MAP_I2S3_CTRL_REG;
		port = 3;
		break;
	case 4:
		addr =	MAP_I2S4_CTRL_REG;
		break;
	default:
		return -EINVAL;
	}

	/* bit size */
	inf = map_raw_read(map_priv, addr);
	inf &= ~MAP_WLEN_MASK;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		inf |= MAP_WLEN_16_BIT;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		inf |= MAP_WLEN_20_BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		inf |= MAP_WLEN_24_BIT;
		break;
	default:
		return -EINVAL;
	}
	map_raw_write(map_priv, addr, inf);

	/* sample rate */
	map_set_port_freq(map_priv, port, params_rate(params));
	map_fe_dai_priv->i2s_config[dai->id - 1] = true;

	switch (dai->id) {
	case 1:
		/* reset i2s1 interface(audio) */
		map_reset_port(map_priv, I2S1);
		break;
	case 2:
		/* reset i2s2 interface(audio) */
		map_reset_port(map_priv, I2S2);
		break;
	case 3:
	case 5:
		/* reset i2s3 interface(audio) */
		map_reset_port(map_priv, I2S3);
		break;
	case 4:
		/* reset i2s4 interface (hifi) */
		map_reset_port(map_priv, I2S4);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int map_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	unsigned int inf = 0, addr;

	map_fe_dai_priv = snd_soc_dai_get_drvdata(codec_dai);
	map_priv = map_fe_dai_priv->map_priv;
	if (map_fe_dai_priv->i2s_config[codec_dai->id - 1])
		return 0;

	switch (codec_dai->id) {
	case 1:
		addr =	MAP_I2S1_CTRL_REG;
		break;
	case 2:
		addr =	MAP_I2S2_CTRL_REG;
		break;
	case 3:
	case 5:
		addr =	MAP_I2S3_CTRL_REG;
		break;
	case 4:
		addr =	MAP_I2S4_CTRL_REG;
		break;
	default:
		return -EINVAL;
	}

	/* set master/slave audio interface */
	inf = map_raw_read(map_priv, addr);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		inf |= MAP_I2S_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		inf &= ~MAP_I2S_MASTER;
		break;
	default:
		return -EINVAL;
	}

	inf &= ~MAP_LRCLK_POL;
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		inf |= MAP_LRCLK_POL;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return -EINVAL;
	}

	inf &= ~MAP_I2S_MODE_MASK;
	inf &= ~MAP_BCLK_POL;
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		inf |= MAP_I2S_MODE_I2S_FORMAT;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		inf |= MAP_I2S_MODE_RIGHT_JUST;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		inf |= MAP_I2S_MODE_LEFT_JUST;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		inf |= MAP_I2S_MODE_PCM_FORMAT;
		inf &= ~MAP_PCM_MODE_B;
		inf |= MAP_BCLK_POL;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		inf |= MAP_I2S_MODE_PCM_FORMAT;
		inf |= MAP_PCM_MODE_B;
		break;
	default:
		break;
	}
	map_raw_write(map_priv, addr, inf);

	return 0;
}

static int mmp_map_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *codec_dai)
{
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;

	if (codec_dai->active)
		return 0;

	map_fe_dai_priv = snd_soc_dai_get_drvdata(codec_dai);
	map_priv = map_fe_dai_priv->map_priv;

	map_be_active(map_priv);

	/* means FM opened */
	if ((codec_dai->id == 3) || (codec_dai->id == 5))
		map_priv->bt_fm_sel = true;

	return 0;
}

static void mmp_map_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *codec_dai)
{
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	unsigned int inf = 0, addr;

	if (codec_dai->active)
		return;

	map_fe_dai_priv = snd_soc_dai_get_drvdata(codec_dai);
	map_priv = map_fe_dai_priv->map_priv;

	/* Fixme: for I2S2, disable it here */
	if (codec_dai->id == 2) {
		addr =	MAP_I2S2_CTRL_REG;
		inf = map_raw_read(map_priv, addr);
		inf &= ~I2S_GEN_EN;
		inf &= ~I2S_REC_EN;
		map_raw_write(map_priv, addr, inf);

		/* apply the change */
		map_apply_change(map_priv);
	}
	map_fe_dai_priv->i2s_config[codec_dai->id - 1] = false;

	/*
	 * comment out: will finally use it after optimize it.
	 * map_disable_i2s_bclk(codec_dai->id - 1);
	 */

	map_be_reset(map_priv);

	/* means FM opened */
	if ((codec_dai->id == 3) || (codec_dai->id == 5))
		map_priv->bt_fm_sel = false;

	return;
}

static int mmp_map_trigger(struct snd_pcm_substream *substream, int cmd,
			     struct snd_soc_dai *codec_dai)
{
	struct map_fe_dai_private *map_fe_dai_priv;
	struct map_private *map_priv;
	unsigned int inf = 0, addr, stream;
	int ret = 0;

	map_fe_dai_priv = snd_soc_dai_get_drvdata(codec_dai);
	map_priv = map_fe_dai_priv->map_priv;
	stream = substream->stream;

	switch (codec_dai->id) {
	case 1:
		addr =	MAP_I2S1_CTRL_REG;
		break;
	case 2:
		addr =	MAP_I2S2_CTRL_REG;
		break;
	case 3:
	case 5:
		addr =	MAP_I2S3_CTRL_REG;
		break;
	case 4:
		addr =	MAP_I2S4_CTRL_REG;
		break;
	default:
		return -EINVAL;
	}

	inf = map_raw_read(map_priv, addr);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			inf |= I2S_REC_EN;
		else
			inf |= I2S_GEN_EN;

		map_raw_write(map_priv, addr, inf);
		/* apply the change */
		map_apply_change(map_priv);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			inf &= ~I2S_REC_EN;
		else
			inf &= ~I2S_GEN_EN;

		map_raw_write(map_priv, addr, inf);
		/* apply the change */
		map_apply_change(map_priv);
		break;

	default:
		ret = -EINVAL;
	}
	return ret;
}

static int map_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

/*
 * MAP can support sample rate 12000 and 24000, if let ASOC support
 * these two rates, you need to change include/sound/pcm.h and
 * sound/core/pcm_native.c
 */
#define MAP_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 | \
	SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_11025 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 | \
	SNDRV_PCM_RATE_176400)

#define MAP_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops map_dai_ops = {
	.startup	= mmp_map_startup,
	.shutdown	= mmp_map_shutdown,
	.trigger	= mmp_map_trigger,
	.hw_params	= map_hw_params,
	.digital_mute	= map_mute,
	.set_fmt	= map_set_dai_fmt,
	.set_sysclk	= map_set_dai_sysclk,
	.set_clkdiv	= map_set_dai_clkdiv,
};

struct snd_soc_dai_driver map_dai[] = {
	/* map codec dai */
	{
		.name = "map-i2s1-dai",
		.id = 1,
		.playback = {
			.stream_name  = "MM_DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates	      = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.capture = {
			.stream_name  = "MM_UL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.ops = &map_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "map-i2s4-dai",
		.id = 4,
		.playback = {
			.stream_name  = "MM_DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.capture = {
			.stream_name  = "MM_UL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.ops = &map_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "map-i2s2-dai",
		.id = 2,
		.playback = {
			.stream_name  = "VC_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.capture = {
			.stream_name  = "VC_UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.ops = &map_dai_ops,
		.symmetric_rates = 1,
	},
	/* For FM playback */
	{
		.name = "map-i2s3-dai",
		.id = 3,
		.playback = {
			.stream_name  = "FM_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.ops = &map_dai_ops,
		.symmetric_rates = 1,
	},
	/* For BT playback & capture */
	{
		.name = "map-i2s5-dai",
		.id = 5,
		.playback = {
			.stream_name  = "BT_DL",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.capture = {
			.stream_name  = "BT_UL",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = MAP_RATES,
			.formats      = MAP_FORMATS,
		},
		.ops = &map_dai_ops,
		.symmetric_rates = 1,
	}
};

static int map_probe(struct snd_soc_codec *codec)
{
	struct map_fe_dai_private *map_fe_dai_priv;

	map_fe_dai_priv = snd_soc_codec_get_drvdata(codec);
	map_fe_dai_priv->codec = codec;
	codec->control_data = map_fe_dai_priv->regmap;
	/* map's register is 4 bytes */
	codec->val_bytes = 4;

	map_add_widgets(codec);
	return 0;
}

static int map_remove(struct snd_soc_codec *codec)
{
	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_map = {
	.probe   = map_probe,
	.remove  = map_remove,
	.read = map_read,
	.write = map_write,
	.reg_cache_size = MAP_CACHE_SIZE,
	.reg_cache_step = 4,
	.reg_word_size = sizeof(u32),
	.controls = map_snd_controls,
	.num_controls = ARRAY_SIZE(map_snd_controls),
	.ignore_pmdown_time = true,
};

static int mmp_map_fe_dai_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct map_private *map_priv = dev_get_drvdata(pdev->dev.parent);
	struct map_fe_dai_private *map_fe_dai_priv;
	int ret = 0;

	if (!np) {
		dev_err(&pdev->dev, "no device node for map-fe\n");
		return -EINVAL;
	}

	map_fe_dai_priv = devm_kzalloc(&pdev->dev,
		sizeof(struct map_fe_dai_private), GFP_KERNEL);
	if (map_fe_dai_priv == NULL)
		return -ENOMEM;

	map_fe_dai_priv->map_priv = map_priv;
	map_fe_dai_priv->regmap = map_priv->regmap;
	platform_set_drvdata(pdev, map_fe_dai_priv);

	ret = snd_soc_register_codec(&pdev->dev,
		&soc_codec_dev_map, map_dai, ARRAY_SIZE(map_dai));
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register MAP codec\n");
		return ret;
	}

	return ret;
}

static int mmp_map_fe_dai_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mmp_map_fe_dai_driver = {
	.probe		= mmp_map_fe_dai_probe,
	.remove		= mmp_map_fe_dai_remove,
	.driver		= {
		.name	= "mmp-map-codec",
		.owner	= THIS_MODULE,
	},
};

static int __init mmp_map_fe_dai_init(void)
{
	return platform_driver_register(&mmp_map_fe_dai_driver);
}

static void __exit mmp_map_fe_dai_exit(void)
{
	platform_driver_unregister(&mmp_map_fe_dai_driver);
}

module_init(mmp_map_fe_dai_init);
module_exit(mmp_map_fe_dai_exit);

MODULE_DESCRIPTION("ASoc Marvell MAP FE driver");
MODULE_AUTHOR("Nenghua Cao <nhcao@marvell.com>");
MODULE_LICENSE("GPL");
