/*
 * aw881xx.c   aw881xx codec module
 *
 * Version: v0.1.2
 *
 * Copyright (c) 2019 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/syscalls.h>
#include <sound/tlv.h>
#include "aw881xx.h"
#include "aw88194.h"
#include "aw88195.h"
#include "aw88194_reg.h"

#include <linux/hardware_info.h>
extern char smart_pa_name[HARDWARE_MAX_ITEM_LONGTH];
extern int aw88194a_state;

/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW881XX_I2C_NAME "aw881xx_smartpa"

#define AW881XX_VERSION "v0.1.2"

#define AW881XX_RATES SNDRV_PCM_RATE_8000_48000
#define AW881XX_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)

#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 5	/* 5ms */
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 5

/******************************************************
 *
 * Value
 *
 ******************************************************/
static int aw881xx_spk_control;
static int aw881xx_rcv_control;
static int aw881xx_dsp_control;

static char aw881xx_cfg_name[][AW881XX_CFG_NAME_MAX] = {
	{"aw881xx_pid_01_spk_reg.bin"},
	{"aw881xx_pid_01_spk_fw.bin"},
	{"aw881xx_pid_01_spk_cfg.bin"},
	{"aw881xx_pid_01_rcv_reg.bin"},
	{"aw881xx_pid_01_rcv_fw.bin"},
	{"aw881xx_pid_01_rcv_cfg.bin"},
};

static unsigned int aw881xx_mode_cfg_shift[AW881XX_MODE_SHIFT_MAX] = {
	AW881XX_MODE_SPK_SHIFT,
	AW881XX_MODE_RCV_SHIFT,
};

/******************************************************
 *
 * aw881xx i2c write/read
 *
 ******************************************************/
int aw881xx_i2c_writes(struct aw881xx *aw881xx,
	uint8_t reg_addr, uint8_t *buf, uint16_t len)
{
	int ret = -1;
	uint8_t *data = NULL;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s: can not allocate memory\n", __func__);
		return -ENOMEM;
	}

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw881xx->i2c, data, len + 1);
	if (ret < 0)
		pr_err("%s: i2c master send error\n", __func__);

	kfree(data);

	return ret;
}

int aw881xx_i2c_reads(struct aw881xx *aw881xx,
	uint8_t reg_addr, uint8_t *buf, uint16_t len)
{
	int ret = -1;

	ret = i2c_smbus_write_byte(aw881xx->i2c, reg_addr);
	if (ret < 0) {
		pr_err("%s: i2c master send error, ret=%d\n", __func__, ret);
		return ret;
	}

	ret = i2c_master_recv(aw881xx->i2c, buf, len);
	if (ret != len) {
		pr_err("%s: couldn't read registers, return %d bytes\n",
			__func__, ret);
		return ret;
	}

	return ret;
}

int aw881xx_i2c_write(struct aw881xx *aw881xx,
	uint8_t reg_addr, uint16_t reg_data)
{
	int ret = -1;
	uint8_t cnt = 0;
	uint8_t buf[2];

	buf[0] = (reg_data & 0xff00) >> 8;
	buf[1] = (reg_data & 0x00ff) >> 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = aw881xx_i2c_writes(aw881xx, reg_addr, buf, 2);
		if (ret < 0)
			pr_err("%s: i2c_write cnt=%d error=%d\n",
				__func__, cnt, ret);
		else
			break;
		cnt++;
	}

	return ret;
}

int aw881xx_i2c_read(struct aw881xx *aw881xx,
	uint8_t reg_addr, uint16_t *reg_data)
{
	int ret = -1;
	uint8_t cnt = 0;
	uint8_t buf[2];

	while (cnt < AW_I2C_RETRIES) {
		ret = aw881xx_i2c_reads(aw881xx, reg_addr, buf, 2);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			*reg_data = (buf[0] << 8) | (buf[1] << 0);
			break;
		}
		cnt++;
	}

	return ret;
}

int aw881xx_i2c_write_bits(struct aw881xx *aw881xx,
	uint8_t reg_addr, uint16_t mask, uint16_t reg_data)
{
	int ret = -1;
	uint16_t reg_val = 0;

	ret = aw881xx_i2c_read(aw881xx, reg_addr, &reg_val);
	if (ret < 0) {
		pr_err("%s: i2c read error, ret=%d\n", __func__, ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data;
	ret = aw881xx_i2c_write(aw881xx, reg_addr, reg_val);
	if (ret < 0) {
		pr_err("%s: i2c read error, ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

int aw881xx_dsp_write(struct aw881xx *aw881xx,
	uint16_t dsp_addr, uint16_t dsp_data)
{
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	int ret = -1;
	uint8_t dsp_mem_addr_reg = 0;
	uint8_t dsp_mem_data_reg = 0;

	ret = dev_ops->get_dsp_mem_reg(aw881xx,
		&dsp_mem_addr_reg, &dsp_mem_data_reg);
	if (ret < 0) {
		pr_err("%s: get_dsp_mem_info fail\n", __func__);
		return ret;
	}

	ret = aw881xx_i2c_write(aw881xx, dsp_mem_addr_reg, dsp_addr);
	if (ret < 0) {
		pr_err("%s: i2c write error, ret=%d\n", __func__, ret);
		return ret;
	}

	ret = aw881xx_i2c_write(aw881xx, dsp_mem_data_reg, dsp_data);
	if (ret < 0) {
		pr_err("%s: i2c write error, ret=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

int aw881xx_dsp_read(struct aw881xx *aw881xx,
	uint16_t dsp_addr, uint16_t *dsp_data)
{
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	int ret = -1;
	uint8_t dsp_mem_addr_reg = 0;
	uint8_t dsp_mem_data_reg = 0;

	ret = dev_ops->get_dsp_mem_reg(aw881xx,
		&dsp_mem_addr_reg, &dsp_mem_data_reg);
	if (ret < 0) {
		pr_err("%s: get_dsp_mem_info fail\n", __func__);
		return ret;
	}
	pr_info("%s: dsp_mem_addr_reg=0x%02x, dsp_mem_data_reg=0x%02x\n",
		__func__, dsp_mem_addr_reg, dsp_mem_data_reg);

	ret = aw881xx_i2c_write(aw881xx, dsp_mem_addr_reg, dsp_addr);
	if (ret < 0) {
		pr_err("%s: i2c write error, ret=%d\n", __func__, ret);
		return ret;
	}

	ret = aw881xx_i2c_read(aw881xx, dsp_mem_data_reg, dsp_data);
	if (ret < 0) {
		pr_err("%s: i2c read error, ret=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

/******************************************************
 *
 * aw881xx cfg select
 *
 ******************************************************/
void aw881xx_get_cfg_shift(struct aw881xx *aw881xx)
{
	aw881xx->cfg_num = aw881xx_mode_cfg_shift[aw881xx->spk_rcv_mode];
	pr_debug("%s: cfg_num=%d\n", __func__, aw881xx->cfg_num);
}

/******************************************************
 *
 * aw881xx cali store
 *
 ******************************************************/
static int aw881xx_get_cali_re(struct aw881xx *aw881xx)
{
	/* get cali re from phone */
	pr_debug("%s aw881xx->cali_re=%d\n",__func__,aw881xx->cali_re);

	/*imped not in 5.8~11.2 user the default cali re */
	if ((aw881xx->cali_re > 45875 )||(aw881xx->cali_re < 23756)) {
		aw881xx->cali_re = AW881XX_DFT_CALI_RE;
		pr_debug("%s: re invalid, use default re=0x%x\n",
			__func__, aw881xx->cali_re);
	}

	return 0;
}

static int aw881xx_set_cali_re(struct aw881xx *aw881xx)
{
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	/* set cali re to phone */

	pr_debug("%s aw881xx->cali_re=%d\n",__func__,aw881xx->cali_re);
	/* set cali re to aw881xx */
	dev_ops->set_cali_re(aw881xx);

	return 0;
}

int aw881xx_dsp_update_cali_re(struct aw881xx *aw881xx)
{
	aw881xx_get_cali_re(aw881xx);
	aw881xx_set_cali_re(aw881xx);

	return 0;
}


/******************************************************
 *
 * kcontrol
 *
 ******************************************************/
static const char *const spk_function[] = { "Off", "On" };
static const char *const rcv_function[] = { "Off", "On" };
static const char *const dsp_function[] = { "Off", "On" };

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 50, 0);

struct soc_mixer_control aw881xx_mixer = {
	.reg = AW881XX_REG_HAGCCFG7,
	.shift = AW881XX_VOL_REG_SHIFT,
	.min = AW881XX_VOLUME_MIN,
	.max = AW881XX_VOLUME_MAX,
};

static int aw881xx_volume_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	/* set kcontrol info */
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = mc->max - mc->min;

	return 0;
}

static int aw881xx_volume_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	unsigned int value = 0;

	dev_ops->get_volume(aw881xx, &value);
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int aw881xx_volume_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	unsigned int value = 0;

	/* value is right */
	value = ucontrol->value.integer.value[0];
	if (value > (mc->max - mc->min) || value < 0) {
		pr_err("%s:value over range\n", __func__);
		return -EINVAL;
	}

	dev_ops->set_volume(aw881xx, value);

	return 0;
}

static struct snd_kcontrol_new aw881xx_volume = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "aw881xx_rx_volume",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
		SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.tlv.p = (digital_gain),
	.info = aw881xx_volume_info,
	.get = aw881xx_volume_get,
	.put = aw881xx_volume_put,
	.private_value = (unsigned long)&aw881xx_mixer,
};

static int aw881xx_spk_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: aw881xx_spk_control=%d\n",
		__func__, aw881xx_spk_control);

	ucontrol->value.integer.value[0] = aw881xx_spk_control;

	return 0;
}

static int aw881xx_spk_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: ucontrol->value.integer.value[0]=%ld\n",
		__func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0] == aw881xx_spk_control)
		return 1;

	aw881xx_spk_control = ucontrol->value.integer.value[0];

	if (aw881xx->spk_rcv_mode != AW881XX_SPEAKER_MODE) {
		aw881xx->spk_rcv_mode = AW881XX_SPEAKER_MODE;
		aw881xx->init = AW881XX_INIT_ST;
	}

	return 0;
}

static int aw881xx_rcv_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: aw881xx_rcv_control=%d\n",
		__func__, aw881xx_rcv_control);

	ucontrol->value.integer.value[0] = aw881xx_rcv_control;

	return 0;
}

static int aw881xx_rcv_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: ucontrol->value.integer.value[0]=%ld\n",
		__func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0] == aw881xx_rcv_control)
		return 1;

	aw881xx_rcv_control = ucontrol->value.integer.value[0];

	if (aw881xx->spk_rcv_mode != AW881XX_RECEIVER_MODE) {
		aw881xx->spk_rcv_mode = AW881XX_RECEIVER_MODE;
		aw881xx->init = AW881XX_INIT_ST;
	}

	return 0;
}

static int aw881xx_dsp_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: aw881xx_dsp_control=%d\n",
		__func__, aw881xx_dsp_control);

	ucontrol->value.integer.value[0] = aw881xx_dsp_control;

	return 0;
}

#define DSP_MASK	4
static int aw881xx_dsp_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	uint16_t reg_val = 0;

	pr_debug("%s: ucontrol->value.integer.value[0]=%ld\n",
		__func__, ucontrol->value.integer.value[0]);

	aw881xx_dsp_control = ucontrol->value.integer.value[0];
	if (aw881xx_dsp_control == 0) {
		pr_debug("%s: bypass dsp\n",__func__);
		ret = aw881xx_i2c_read(aw881xx, AW88194_REG_SYSCTRL, &reg_val);
		if (ret < 0) {
			pr_err("%s: i2c read error, ret=%d\n", __func__, ret);
			return ret;
		}
		reg_val |= (DSP_MASK);
		ret = aw881xx_i2c_write(aw881xx, AW88194_REG_SYSCTRL, reg_val);
		if (ret < 0) {
			pr_err("%s: i2c read error, ret=%d\n", __func__, ret);
			return ret;
		}
	} else {
		pr_debug("%s: open dsp\n",__func__);
		ret = aw881xx_i2c_read(aw881xx, AW88194_REG_SYSCTRL, &reg_val);
		if (ret < 0) {
			pr_err("%s: i2c read error, ret=%d\n", __func__, ret);
			return ret;
		}
		reg_val &= (~DSP_MASK);
		ret = aw881xx_i2c_write(aw881xx, AW88194_REG_SYSCTRL, reg_val);
		if (ret < 0) {
			pr_err("%s: i2c read error, ret=%d\n", __func__, ret);
			return ret;
		}
	}
	return ret;
}

static const struct soc_enum aw881xx_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_function), spk_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rcv_function), rcv_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dsp_function), dsp_function),
};

static struct snd_kcontrol_new aw881xx_controls[] = {
	SOC_ENUM_EXT("aw881xx_speaker_switch", aw881xx_snd_enum[0],
		aw881xx_spk_get, aw881xx_spk_set),
	SOC_ENUM_EXT("aw881xx_receiver_switch", aw881xx_snd_enum[1],
		aw881xx_rcv_get, aw881xx_rcv_set),
	SOC_ENUM_EXT("aw881xx_dsp_switch", aw881xx_snd_enum[2],
		aw881xx_dsp_get, aw881xx_dsp_set),
};

static void aw881xx_add_codec_controls(struct aw881xx *aw881xx)
{
	pr_info("%s: enter\n", __func__);

	snd_soc_add_codec_controls(aw881xx->codec, aw881xx_controls,
		ARRAY_SIZE(aw881xx_controls));

	snd_soc_add_codec_controls(aw881xx->codec, &aw881xx_volume, 1);
}

/******************************************************
 *
 * Digital Audio Interface
 *
 ******************************************************/
static int aw881xx_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	pr_info("%s: enter\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mutex_lock(&aw881xx->lock);
		dev_ops->run_pwd(aw881xx, false);
		mutex_unlock(&aw881xx->lock);
	}

	return 0;
}

static int aw881xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	/*struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(dai->codec);*/
	struct snd_soc_codec *codec = dai->codec;

	pr_info("%s: fmt=0x%x\n", __func__, fmt);

	/* supported mode: regular I2S, slave, or PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) !=
			SND_SOC_DAIFMT_CBS_CFS) {
			dev_err(codec->dev,
				"%s: invalid codec master mode\n", __func__);
			return -EINVAL;
		}
		break;
	default:
		dev_err(codec->dev, "%s: unsupported DAI format %d\n",
			__func__, fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}
	return 0;
}

static int aw881xx_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec_dai->codec);

	pr_info("%s: freq=%d\n", __func__, freq);

	aw881xx->sysclk = freq;
	return 0;
}

static int aw881xx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_debug("%s: requested rate: %d, sample size: %d\n",
			__func__, params_rate(params),
			snd_pcm_format_width(params_format(params)));
		return 0;
	}
	/* get rate param */
	aw881xx->rate = params_rate(params);
	pr_debug("%s: requested rate: %d, sample size: %d\n",
		__func__, aw881xx->rate,
		 snd_pcm_format_width(params_format(params)));

	/* get bit width */
	aw881xx->width = params_width(params);
	pr_debug("%s: width = %d\n", __func__, aw881xx->width);

	mutex_lock(&aw881xx->lock);
	dev_ops->update_hw_params(aw881xx);
	mutex_unlock(&aw881xx->lock);

	return 0;
}

static int aw881xx_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	pr_info("%s: mute state=%d\n", __func__, mute);

	if (!(aw881xx->flags & AW881XX_FLAG_START_ON_MUTE))
		return 0;

	if (mute) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mutex_lock(&aw881xx->lock);
			dev_ops->smartpa_cfg(aw881xx, false);
			mutex_unlock(&aw881xx->lock);
		}
	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mutex_lock(&aw881xx->lock);
			dev_ops->smartpa_cfg(aw881xx, true);
			mutex_unlock(&aw881xx->lock);
		}
	}

	return 0;
}

static void aw881xx_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aw881xx->rate = 0;
		mutex_lock(&aw881xx->lock);
		dev_ops->run_pwd(aw881xx, true);
		mutex_unlock(&aw881xx->lock);
	}
}

static const struct snd_soc_dai_ops aw881xx_dai_ops = {
	.startup = aw881xx_startup,
	.set_fmt = aw881xx_set_fmt,
	.set_sysclk = aw881xx_set_dai_sysclk,
	.hw_params = aw881xx_hw_params,
	.mute_stream = aw881xx_mute,
	.shutdown = aw881xx_shutdown,
};

static struct snd_soc_dai_driver aw881xx_dai[] = {
	{
	.name = "aw881xx-aif",
	.id = 1,
	.playback = {
		.stream_name = "Speaker_Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AW881XX_RATES,
		.formats = AW881XX_FORMATS,
		},
	.capture = {
		.stream_name = "Speaker_Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AW881XX_RATES,
		.formats = AW881XX_FORMATS,
		},
	.ops = &aw881xx_dai_ops,
	.symmetric_rates = 1,
	.symmetric_channels = 1,
	.symmetric_samplebits = 1,
	},
};

/*****************************************************
 *
 * codec driver
 *
 *****************************************************/
static int aw881xx_probe(struct snd_soc_codec *codec)
{
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	int ret = -1;

	pr_info("%s: enter\n", __func__);

	aw881xx->codec = codec;

	aw881xx_add_codec_controls(aw881xx);

	if (codec->dev->of_node)
		dev_set_name(codec->dev, "%s", "aw881xx_smartpa");

	pr_info("%s: exit\n", __func__);

	ret = 0;
	return ret;
}

static int aw881xx_remove(struct snd_soc_codec *codec)
{
	/*struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);*/
	pr_info("%s: enter\n", __func__);

	return 0;
}

static unsigned int aw881xx_codec_read(struct snd_soc_codec *codec,
	unsigned int reg)
{
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	uint16_t value = 0;
	int ret = -1;

	pr_debug("%s: enter\n", __func__);

	if (dev_ops->get_reg_access(reg) & REG_RD_ACCESS) {
		ret = aw881xx_i2c_read(aw881xx, reg, &value);
		if (ret < 0) {
			pr_debug("%s: read register failed\n", __func__);
			return ret;
		}
	} else {
		pr_debug("%s: register 0x%x no read access\n",
			__func__, reg);
		return ret;
	}
	return ret;
}

static int aw881xx_codec_write(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int value)
{
	int ret = -1;
	struct aw881xx *aw881xx = snd_soc_codec_get_drvdata(codec);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	pr_debug("%s: enter ,reg is 0x%x value is 0x%x\n",
		__func__, reg, value);

	if (dev_ops->get_reg_access(reg) & REG_WR_ACCESS)
		ret = aw881xx_i2c_write(aw881xx, (uint8_t)reg, (uint16_t)value);
	else
		pr_debug("%s: register 0x%x no write access\n",
			__func__, reg);

	return ret;
}

static struct snd_soc_codec_driver soc_codec_dev_aw881xx = {
	.probe = aw881xx_probe,
	.remove = aw881xx_remove,
	.read = aw881xx_codec_read,
	.write = aw881xx_codec_write,
	.reg_cache_size = AW881XX_REG_MAX,
	.reg_word_size = 2,
};

/******************************************************
 *
 * irq
 *
 ******************************************************/
static irqreturn_t aw881xx_irq(int irq, void *data)
{
	struct aw881xx *aw881xx = data;
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	pr_info("%s: enter\n", __func__);

	dev_ops->interrupt_handle(aw881xx);

	pr_info("%s: exit\n", __func__);

	return IRQ_HANDLED;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw881xx_parse_dt(struct device *dev, struct aw881xx *aw881xx,
				struct device_node *np)
{
	int ret = -1;

	/* gpio */
	aw881xx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw881xx->reset_gpio < 0) {
		dev_err(dev, "%s: no reset gpio provided, will not hw reset\n",
			__func__);
		return -EIO;
	} else {
		dev_info(dev, "%s: reset gpio provided ok\n", __func__);
	}

	aw881xx->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw881xx->irq_gpio < 0)
		dev_info(dev, "%s: no irq gpio provided.\n", __func__);
	else
		dev_info(dev, "%s: irq gpio provided ok.\n", __func__);

	/* vbat monitor */
	ret = of_property_read_u32(np, "monitor-flag", &aw881xx->monitor_flag);
	if (ret) {
		dev_err(dev, "%s: find no monitor-flag, use default config\n",
			__func__);
		aw881xx->monitor_flag = AW881XX_MONITOR_DFT_FLAG;
	} else {
		dev_info(dev, "%s: monitor-flag = %d\n",
			__func__, aw881xx->monitor_flag);
	}
	ret = of_property_read_u32(np, "monitor-timer-val",
		&aw881xx->monitor_timer_val);
	if (ret) {
		dev_err(dev,
			"%s: find no monitor-timer-val, use default config\n",
			__func__);
		aw881xx->monitor_timer_val = AW881XX_MONITOR_TIMER_DFT_VAL;
	} else {
		dev_info(dev, "%s: monitor-timer-val = %d\n",
			__func__, aw881xx->monitor_timer_val);
	}

	return 0;
}

int aw881xx_hw_reset(struct aw881xx *aw881xx)
{
	pr_info("%s: enter\n", __func__);

	if (aw881xx && gpio_is_valid(aw881xx->reset_gpio)) {
		gpio_set_value_cansleep(aw881xx->reset_gpio, 0);
		msleep(1);
		gpio_set_value_cansleep(aw881xx->reset_gpio, 1);
		msleep(1);
	} else {
		dev_err(aw881xx->dev, "%s:  failed\n", __func__);
	}
	return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
int aw881xx_update_cfg_name(struct aw881xx *aw881xx)
{
	char aw881xx_head[] = {"aw881xx_pid_"};
	char buf[2];
	uint8_t i = 0;
	uint8_t head_index = 0;

	head_index = sizeof(aw881xx_head) - 1;

	snprintf(buf, sizeof(buf)+1, "%02x", aw881xx->pid);

	for (i = 0; i < sizeof(aw881xx_cfg_name)/AW881XX_CFG_NAME_MAX; i++)
		memcpy(aw881xx_cfg_name[i] + head_index, buf, sizeof(buf));

	return 0;
}

int aw881xx_read_product_id(struct aw881xx *aw881xx)
{
	int ret = -1;
	uint16_t reg_val = 0;
	uint16_t product_id = 0;
	uint16_t cfg_num = 0;
	int i = 0;

	ret = aw881xx_i2c_read(aw881xx, AW881XX_REG_PRODUCT_ID, &reg_val);
	if (ret < 0) {
		dev_err(aw881xx->dev,
			"%s: failed to read REG_EFRL: %d\n",
			__func__, ret);
		return -EIO;
	}

	product_id = reg_val & (~AW881XX_BIT_PRODUCT_ID_MASK);
	product_id = AW881XX_PID_03;
	switch (product_id) {
	case AW881XX_PID_01:
		aw881xx->pid = AW881XX_PID_01;
		aw88194_ops(aw881xx);
		break;
	case AW881XX_PID_03:
		aw881xx->pid = AW881XX_PID_03;
		aw88195_ops(aw881xx);
		break;
	default:
		pr_info("%s: unsupported product_id=0x%02x\n",
			__func__, product_id);
		return -EIO;
	}

	aw881xx_update_cfg_name(aw881xx);

	memcpy(aw881xx->cfg_name, aw881xx_cfg_name, sizeof(aw881xx_cfg_name));

	cfg_num = sizeof(aw881xx_cfg_name)/AW881XX_CFG_NAME_MAX;
	for (i = 0; i < cfg_num; i++)
		pr_debug("%s: id[%d], [%s]\n", __func__,
			i, aw881xx->cfg_name[i]);

	return 0;
}

int aw881xx_read_chipid(struct aw881xx *aw881xx)
{
	int ret = -1;
	uint8_t cnt = 0;
	uint16_t reg_val = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw881xx_i2c_read(aw881xx, AW881XX_REG_ID, &reg_val);
		if (ret < 0) {
			dev_err(aw881xx->dev,
				"%s: failed to read chip id, ret=%d\n",
				__func__, ret);
			return -EIO;
		}
		switch (reg_val) {
		case AW881XX_CHIPID:
			aw881xx->chipid = AW881XX_CHIPID;
			aw881xx->flags |= AW881XX_FLAG_START_ON_MUTE;
			aw881xx->flags |= AW881XX_FLAG_SKIP_INTERRUPTS;

			ret = aw881xx_read_product_id(aw881xx);
			if (ret < 0) {
				dev_err(aw881xx->dev,
					"%s: fail to read product id, ret=%d\n",
					__func__, ret);
				return -EIO;
			}

			pr_info("%s: chipid=0x%04x, product_id=0x%02x\n",
				__func__, aw881xx->chipid, aw881xx->pid);
			return 0;
		default:
			pr_info("%s: unsupported device revision (0x%x)\n",
				__func__, reg_val);
			break;
		}
		cnt++;

		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

/*****************************************************
 *
 * monitor
 *
 *****************************************************/
int aw881xx_monitor_stop(struct aw881xx *aw881xx)
{
	pr_info("%s: enter\n", __func__);

	if (hrtimer_active(&aw881xx->monitor_timer)) {
		pr_info("%s: cancel monitor\n", __func__);
		hrtimer_cancel(&aw881xx->monitor_timer);
	}
	return 0;
}

int aw881xx_monitor_start(struct aw881xx *aw881xx)
{
	int timer_val = aw881xx->monitor_timer_val;

	pr_info("%s enter\n", __func__);

	if (!hrtimer_active(&aw881xx->monitor_timer)) {
		pr_info("%s: start monitor\n", __func__);
		hrtimer_start(&aw881xx->monitor_timer,
			ktime_set(timer_val / 1000,
			(timer_val % 1000) * 1000000),
			HRTIMER_MODE_REL);
	}
	return 0;
}

static enum hrtimer_restart aw881xx_monitor_timer_func(struct hrtimer *timer)
{
	struct aw881xx *aw881xx =
		container_of(timer, struct aw881xx, monitor_timer);

	pr_debug("%s: enter\n", __func__);

	if (aw881xx->monitor_flag)
		schedule_work(&aw881xx->monitor_work);

	return HRTIMER_NORESTART;
}

static void aw881xx_monitor_work_routine(struct work_struct *work)
{
	struct aw881xx *aw881xx =
		container_of(work, struct aw881xx, monitor_work);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	pr_debug("%s: enter\n", __func__);

	mutex_lock(&aw881xx->lock);
	if (!dev_ops->get_hmute(aw881xx)) {
		dev_ops->monitor(aw881xx);
		aw881xx_monitor_start(aw881xx);
	}
	mutex_unlock(&aw881xx->lock);
}

static int aw881xx_monitor_init(struct aw881xx *aw881xx)
{
	pr_info("%s: enter\n", __func__);

	hrtimer_init(&aw881xx->monitor_timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	aw881xx->monitor_timer.function = aw881xx_monitor_timer_func;
	INIT_WORK(&aw881xx->monitor_work, aw881xx_monitor_work_routine);

	return 0;
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw881xx_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);

	unsigned int databuf[2] = { 0 };

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1]))
		aw881xx_i2c_write(aw881xx, databuf[0], databuf[1]);

	return count;
}

static ssize_t aw881xx_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);

	ssize_t len = 0;
	uint8_t i = 0;
	uint16_t reg_val = 0;

	for (i = 0; i < AW881XX_REG_MAX; i++) {
		if (dev_ops->get_reg_access(i) & REG_RD_ACCESS) {
			aw881xx_i2c_read(aw881xx, i, &reg_val);
			len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02x=0x%04x\n", i, reg_val);
		}
	}
	return len;
}

static ssize_t aw881xx_rw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);

	unsigned int databuf[2] = { 0 };

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		aw881xx->reg_addr = (uint8_t)databuf[0];
		aw881xx_i2c_write(aw881xx, databuf[0], databuf[1]);
	} else if (1 == sscanf(buf, "%x", &databuf[0])) {
		aw881xx->reg_addr = (uint8_t)databuf[0];
	}

	return count;
}

static ssize_t aw881xx_rw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	ssize_t len = 0;
	uint16_t reg_val = 0;

	if (dev_ops->get_reg_access(aw881xx->reg_addr) & REG_RD_ACCESS) {
		aw881xx_i2c_read(aw881xx, aw881xx->reg_addr, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"reg:0x%02x=0x%04x\n", aw881xx->reg_addr, reg_val);
	}
	return len;
}

static ssize_t aw881xx_dsp_rw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	ssize_t len = 0;
	uint16_t reg_val = 0;
	uint8_t dsp_mem_addr_reg = 0;
	uint8_t dsp_mem_data_reg = 0;
	int ret = -1;

	ret = dev_ops->get_dsp_mem_reg(aw881xx,
		&dsp_mem_addr_reg, &dsp_mem_data_reg);
	if (ret < 0) {
		pr_err("%s: get_dsp_mem_info fail\n", __func__);
		return ret;
	}

	aw881xx_i2c_read(aw881xx, dsp_mem_data_reg, &reg_val);
	len += snprintf(buf + len, PAGE_SIZE - len,
		"dsp:0x%04x=0x%04x\n", aw881xx->dsp_addr, reg_val);
	aw881xx_i2c_read(aw881xx, dsp_mem_data_reg, &reg_val);
	len += snprintf(buf + len, PAGE_SIZE - len,
		"dsp:0x%04x=0x%04x\n", aw881xx->dsp_addr + 1, reg_val);

	return len;
}

static ssize_t aw881xx_dsp_rw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	unsigned int databuf[2] = { 0 };
	uint8_t dsp_mem_addr_reg = 0;
	uint8_t dsp_mem_data_reg = 0;
	int ret = -1;

	ret = dev_ops->get_dsp_mem_reg(aw881xx,
		&dsp_mem_addr_reg, &dsp_mem_data_reg);
	if (ret < 0) {
		pr_err("%s: get_dsp_mem_info fail\n", __func__);
		return ret;
	}

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		aw881xx->dsp_addr = (unsigned int)databuf[0];
		aw881xx_i2c_write(aw881xx, dsp_mem_addr_reg, databuf[0]);
		aw881xx_i2c_write(aw881xx, dsp_mem_data_reg, databuf[1]);
		pr_debug("%s: get param: %x %x\n", __func__, databuf[0],
			databuf[1]);
	} else if (1 == sscanf(buf, "%x", &databuf[0])) {
		aw881xx->dsp_addr = (unsigned int)databuf[0];
		aw881xx_i2c_write(aw881xx, dsp_mem_addr_reg, databuf[0]);
		pr_debug("%s: get param: %x\n", __func__, databuf[0]);
	}

	return count;
}

static ssize_t aw881xx_spk_rcv_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	int ret = -1;
	unsigned int databuf[2] = {0};

	ret = kstrtouint(buf, 0, &databuf[0]);
	if (ret < 0)
		return ret;

	aw881xx->spk_rcv_mode = databuf[0];
	return count;
}

static ssize_t aw881xx_spk_rcv_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (aw881xx->spk_rcv_mode == AW881XX_SPEAKER_MODE)
		len += snprintf(buf + len, PAGE_SIZE - len,
			"aw881xx spk_rcv: %d, speaker mode\n",
			aw881xx->spk_rcv_mode);
	else if (aw881xx->spk_rcv_mode == AW881XX_RECEIVER_MODE)
		len += snprintf(buf + len, PAGE_SIZE - len,
			"aw881xx spk_rcv: %d, receiver mode\n",
			aw881xx->spk_rcv_mode);
	else
		len += snprintf(buf + len, PAGE_SIZE - len,
			"aw881xx spk_rcv: %d, unknown mode\n",
			aw881xx->spk_rcv_mode);

	return len;
}

static ssize_t aw881xx_update_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	pr_debug("%s: value=%d\n", __func__, val);

	if (val) {
		aw881xx->init = AW881XX_INIT_ST;
		dev_ops->smartpa_cfg(aw881xx, false);
		dev_ops->smartpa_cfg(aw881xx, true);
	}
	return count;
}

static ssize_t aw881xx_update_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/*struct aw881xx *aw881xx = dev_get_drvdata(dev);*/
	ssize_t len = 0;

	return len;
}


static ssize_t aw881xx_dsp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	pr_debug("%s: value=%d\n", __func__, val);

	if (val) {
		aw881xx->init = AW881XX_INIT_ST;
		dev_ops->update_dsp(aw881xx);
	}
	return count;
}

static ssize_t aw881xx_dsp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	struct device_ops *dev_ops = &(aw881xx->dev_ops);
	ssize_t len = 0;
	unsigned int i = 0;
	uint16_t reg_val = 0;
	uint8_t dsp_mem_addr_reg = 0;
	uint8_t dsp_mem_data_reg = 0;
	int ret = -1;

	ret = dev_ops->get_dsp_mem_reg(aw881xx,
		&dsp_mem_addr_reg, &dsp_mem_data_reg);
	if (ret < 0) {
		pr_err("%s: get_dsp_mem_info fail\n", __func__);
		return ret;
	}

	dev_ops->get_dsp_config(aw881xx);
	if (aw881xx->dsp_cfg == AW881XX_DSP_BYPASS) {
		len += snprintf((char *)(buf + len), PAGE_SIZE - len,
			"%s: aw881xx dsp bypass\n", __func__);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"aw881xx dsp working\n");
		ret = dev_ops->get_iis_status(aw881xx);
		if (ret < 0) {
			len += snprintf((char *)(buf + len),
				PAGE_SIZE - len,
				"%s: aw881xx no iis signal\n", __func__);
		} else {
			pr_debug("%s: aw881xx_dsp_firmware:\n", __func__);
			aw881xx_i2c_write(aw881xx, dsp_mem_addr_reg,
				AW881XX_SPK_DSP_FW_ADDR);
			for (i = 0; i < aw881xx->dsp_fw_len; i += 2) {
				aw881xx_i2c_read(aw881xx,
					dsp_mem_data_reg, &reg_val);
				pr_debug("%s: dsp_fw[0x%04x]:0x%02x,0x%02x\n",
					__func__, i, (reg_val >> 0) & 0xff,
					(reg_val >> 8) & 0xff);
			}
			pr_debug("\n");

			pr_debug("%s: aw881xx_dsp_cfg:\n", __func__);
			aw881xx_i2c_write(aw881xx, dsp_mem_addr_reg,
				AW881XX_SPK_DSP_FW_ADDR);
			len += snprintf(buf + len, PAGE_SIZE - len,
				"aw881xx dsp config:\n");
			aw881xx_i2c_write(aw881xx, dsp_mem_addr_reg,
				AW881XX_SPK_DSP_CFG_ADDR);
			for (i = 0; i < aw881xx->dsp_cfg_len; i += 2) {
				aw881xx_i2c_read(aw881xx,
					dsp_mem_data_reg, &reg_val);
				len += snprintf(buf + len, PAGE_SIZE - len,
					"%02x,%02x,",
					(reg_val >> 0) & 0xff,
					(reg_val >> 8) & 0xff);
				pr_debug("%s: dsp_cfg[0x%04x]:0x%02x,0x%02x\n",
					__func__, i, (reg_val >> 0) & 0xff,
					(reg_val >> 8) & 0xff);
				if ((i / 2 + 1) % 8 == 0) {
					len += snprintf(buf + len,
						PAGE_SIZE - len, "\n");
				}
			}
			len += snprintf(buf + len, PAGE_SIZE - len, "\n");
		}
	}

	return len;
}

static ssize_t aw881xx_cali_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	int ret = -1;
	unsigned int databuf[2] = {0};

	ret = kstrtouint(buf, 0, &databuf[0]);
	if (ret < 0)
		return ret;

	aw881xx->cali_re = databuf[0];
	aw881xx_set_cali_re(aw881xx);

	return count;
}

static ssize_t aw881xx_cali_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"aw881xx cali re: 0x%04x\n", aw881xx->cali_re);

	return len;
}

static ssize_t aw881xx_re_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	int ret = -1;
	unsigned int databuf[2] = {0};

	ret = kstrtouint(buf, 0, &databuf[0]);
	if (ret < 0)
		return ret;

	aw881xx->re = databuf[0];

	return count;
}

static ssize_t aw881xx_re_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	ssize_t len = 0;
	int temp;
	int imped_integer=0;
	int imped_decimals=0;
	int result=0;

	if((aw881xx->re>=27852)&&(aw881xx->re<=41779))
		result=1;
	else
		result=0;

	temp=(aw881xx->re*100/4096);
	imped_integer=temp/100;
	imped_decimals=temp%100;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"re=%d,impedance=%d.%d,result=%d\n", aw881xx->re,imped_integer,imped_decimals,result);

	return len;
}

static ssize_t aw881xx_f0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	int ret = -1;
	unsigned int databuf[2] = {0};

	ret = kstrtouint(buf, 0, &databuf[0]);
	if (ret < 0)
		return ret;

	aw881xx->f0 = databuf[0];

	return count;
}

static ssize_t aw881xx_f0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%d\n", aw881xx->f0);

	return len;
}

static ssize_t aw881xx_monitor_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	int ret = -1;
	unsigned int databuf[2] = {0};

	ret = kstrtouint(buf, 0, &databuf[0]);
	if (ret < 0)
		return ret;

	mutex_lock(&aw881xx->lock);
	aw881xx->monitor_flag = databuf[0];
	mutex_unlock(&aw881xx->lock);

	return count;
}

static ssize_t aw881xx_monitor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw881xx *aw881xx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"aw881xx monitor_flag=%d\n", aw881xx->monitor_flag);

	return len;
}

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO,
	aw881xx_reg_show, aw881xx_reg_store);
static DEVICE_ATTR(rw, S_IWUSR | S_IRUGO,
	aw881xx_rw_show, aw881xx_rw_store);
static DEVICE_ATTR(dsp_rw, S_IWUSR | S_IRUGO,
	aw881xx_dsp_rw_show, aw881xx_dsp_rw_store);
static DEVICE_ATTR(spk_rcv, S_IWUSR | S_IRUGO,
	aw881xx_spk_rcv_show, aw881xx_spk_rcv_store);
static DEVICE_ATTR(update, S_IWUSR | S_IRUGO,
	aw881xx_update_show, aw881xx_update_store);
static DEVICE_ATTR(dsp, S_IWUSR | S_IRUGO,
	aw881xx_dsp_show, aw881xx_dsp_store);
static DEVICE_ATTR(cali, S_IWUSR | S_IRUGO,
	aw881xx_cali_show, aw881xx_cali_store);
static DEVICE_ATTR(re, S_IWUSR | S_IRUGO,
	aw881xx_re_show, aw881xx_re_store);
static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO,
	aw881xx_f0_show, aw881xx_f0_store);
static DEVICE_ATTR(monitor, S_IWUSR | S_IRUGO,
	aw881xx_monitor_show, aw881xx_monitor_store);

static struct attribute *aw881xx_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_rw.attr,
	&dev_attr_dsp_rw.attr,
	&dev_attr_spk_rcv.attr,
	&dev_attr_update.attr,
	&dev_attr_dsp.attr,
	&dev_attr_cali.attr,
	&dev_attr_re.attr,
	&dev_attr_f0.attr,
	&dev_attr_monitor.attr,
	NULL
};

static struct attribute_group aw881xx_attribute_group = {
	.attrs = aw881xx_attributes
};

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw881xx_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct snd_soc_dai_driver *dai;
	struct aw881xx *aw881xx;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags = 0;
	int ret = -1;

	pr_info("%s: enter\n", __func__);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aw881xx = devm_kzalloc(&i2c->dev,
		sizeof(struct aw881xx), GFP_KERNEL);
	if (aw881xx == NULL)
		return -ENOMEM;

	aw881xx->dev = &i2c->dev;
	aw881xx->i2c = i2c;

	i2c_set_clientdata(i2c, aw881xx);
	mutex_init(&aw881xx->lock);

	/* aw881xx rst & int */
	if (np) {
		ret = aw881xx_parse_dt(&i2c->dev, aw881xx, np);
		if (ret) {
			dev_err(&i2c->dev,
				"%s: failed to parse device tree node\n",
				__func__);
			goto err_parse_dt;
		}
	} else {
		aw881xx->reset_gpio = -1;
		aw881xx->irq_gpio = -1;
	}

	if (gpio_is_valid(aw881xx->reset_gpio)) {
		ret = devm_gpio_request_one(
			&i2c->dev, aw881xx->reset_gpio,
			GPIOF_OUT_INIT_LOW, "aw881xx_rst");
		if (ret) {
			dev_err(&i2c->dev, "%s: rst request failed\n",
				__func__);
			goto err_reset_gpio_request;
		}
	}

	if (gpio_is_valid(aw881xx->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw881xx->irq_gpio,
			GPIOF_DIR_IN, "aw881xx_int");
		if (ret) {
			dev_err(&i2c->dev, "%s: int request failed\n",
				__func__);
			goto err_irq_gpio_request;
		}
	}

	/* hardware reset */
	aw881xx_hw_reset(aw881xx);

	/* aw881xx chip id */
	ret = aw881xx_read_chipid(aw881xx);
	if (ret < 0) {
		aw88194a_state=0;
		dev_err(&i2c->dev, "%s: aw881xx_read_chipid failed ret=%d\n",
			__func__, ret);
		goto err_id;
	}else{
		strcpy(smart_pa_name, "AW88194ACSR");
	}

	/* aw881xx device name */
	if (i2c->dev.of_node)
		dev_set_name(&i2c->dev, "%s", "aw881xx_smartpa");
	else
		dev_err(&i2c->dev, "%s failed to set device name: %d\n",
			__func__, ret);

	/* register codec */
	dai = devm_kzalloc(&i2c->dev, sizeof(aw881xx_dai), GFP_KERNEL);
	if (!dai)
		goto err_dai_kzalloc;

	memcpy(dai, aw881xx_dai, sizeof(aw881xx_dai));
	pr_info("%s: dai->name(%s)\n", __func__, dai->name);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_aw881xx,
		dai, ARRAY_SIZE(aw881xx_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "%s failed to register aw881xx: %d\n",
			__func__, ret);
		goto err_register_codec;
	}

	/* aw881xx irq */
	if (gpio_is_valid(aw881xx->irq_gpio) &&
		!(aw881xx->flags & AW881XX_FLAG_SKIP_INTERRUPTS)) {
		aw881xx->dev_ops.interrupt_setup(aw881xx);
		/* register irq handler */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
			gpio_to_irq(aw881xx->irq_gpio),
			NULL, aw881xx_irq, irq_flags,
			"aw881xx", aw881xx);
		if (ret != 0) {
			dev_err(&i2c->dev,
				"failed to request IRQ %d: %d\n",
				gpio_to_irq(aw881xx->irq_gpio), ret);
			goto err_irq;
		}
	} else {
		dev_info(&i2c->dev,
			"%s skipping IRQ registration\n", __func__);
		/* disable feature support if gpio was invalid */
		aw881xx->flags |= AW881XX_FLAG_SKIP_INTERRUPTS;
	}

	dev_set_drvdata(&i2c->dev, aw881xx);
	ret = sysfs_create_group(&i2c->dev.kobj,
		&aw881xx_attribute_group);
	if (ret < 0) {
		dev_info(&i2c->dev,
			"%s error creating sysfs attr files\n",
			 __func__);
		goto err_sysfs;
	}

	aw881xx_monitor_init(aw881xx);

	pr_info("%s: probe completed successfully!\n", __func__);

	return 0;

err_sysfs:
	devm_free_irq(&i2c->dev, gpio_to_irq(aw881xx->irq_gpio), aw881xx);
err_irq:
	snd_soc_unregister_codec(&i2c->dev);
err_register_codec:
	devm_kfree(&i2c->dev, dai);
	dai = NULL;
err_dai_kzalloc:
err_id:
	if (gpio_is_valid(aw881xx->irq_gpio))
		devm_gpio_free(&i2c->dev, aw881xx->irq_gpio);
err_irq_gpio_request:
	if (gpio_is_valid(aw881xx->reset_gpio))
		devm_gpio_free(&i2c->dev, aw881xx->reset_gpio);
err_reset_gpio_request:
err_parse_dt:
	devm_kfree(&i2c->dev, aw881xx);
	aw881xx = NULL;
	return ret;
}

static int aw881xx_i2c_remove(struct i2c_client *i2c)
{
	struct aw881xx *aw881xx = i2c_get_clientdata(i2c);

	pr_info("%s: enter\n", __func__);

	if (gpio_to_irq(aw881xx->irq_gpio))
		devm_free_irq(&i2c->dev, gpio_to_irq(aw881xx->irq_gpio),
			aw881xx);

	snd_soc_unregister_codec(&i2c->dev);

	if (gpio_is_valid(aw881xx->irq_gpio))
		devm_gpio_free(&i2c->dev, aw881xx->irq_gpio);
	if (gpio_is_valid(aw881xx->reset_gpio))
		devm_gpio_free(&i2c->dev, aw881xx->reset_gpio);

	devm_kfree(&i2c->dev, aw881xx);
	aw881xx = NULL;

	return 0;
}

static const struct i2c_device_id aw881xx_i2c_id[] = {
	{AW881XX_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw881xx_i2c_id);

static struct of_device_id aw881xx_dt_match[] = {
	{.compatible = "awinic,aw881xx_smartpa"},
	{},
};

static struct i2c_driver aw881xx_i2c_driver = {
	.driver = {
		.name = AW881XX_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw881xx_dt_match),
		},
	.probe = aw881xx_i2c_probe,
	.remove = aw881xx_i2c_remove,
	.id_table = aw881xx_i2c_id,
};

static int __init aw881xx_i2c_init(void)
{
	int ret = -1;

	pr_info("%s: aw881xx driver version %s\n", __func__, AW881XX_VERSION);

	ret = i2c_add_driver(&aw881xx_i2c_driver);
	if (ret)
		pr_err("%s: fail to add aw881xx device into i2c\n", __func__);

	return ret;
}

module_init(aw881xx_i2c_init);

static void __exit aw881xx_i2c_exit(void)
{
	i2c_del_driver(&aw881xx_i2c_driver);
}

module_exit(aw881xx_i2c_exit);

MODULE_DESCRIPTION("ASoC AW881XX Smart PA Driver");
MODULE_LICENSE("GPL v2");
