// SPDX-License-Identifier: GPL-2.0+
/* fs1818.c -- fs1818 ALSA SoC Audio driver
 *
 * Copyright (C) Fourier Semiconductor Inc. 2016-2021.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

// #define DEBUG
#define pr_fmt(fmt) "%s: " fmt "\n", __func__
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_REGULATOR)
#include <linux/regulator/consumer.h>
#endif
#include "fs1818.h"

#define log_debug(dev, fmt, args...) \
	dev_dbg(dev, "%s: " fmt "\n", __func__, ##args)
#define log_info(dev, fmt, args...) \
	dev_info(dev, "%s: " fmt "\n", __func__, ##args)
#define log_err(dev, fmt, args...) \
	dev_err(dev, "%s: " fmt "\n", __func__, ##args)

/* Supported rates and data formats */
#define FSM_RATES SNDRV_PCM_RATE_8000_48000
#define FSM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE \
		| SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static DEFINE_MUTEX(g_fsm_mutex);

static int fsm_dev_count;
static const unsigned int fsm_rates[] = { 8000, 16000, 32000, 44100, 48000 };
static const struct snd_pcm_hw_constraint_list fsm_constraints = {
	.list = fsm_rates,
	.count = ARRAY_SIZE(fsm_rates),
};

const static struct fsm_pll_config g_fs1818_pll_tbl[] = {
	/* bclk,    0xC1,   0xC2,   0xC3 */
	{  256000, 0x01A0, 0x0180, 0x0001 }, //  8000*16*2
	{  384000, 0x0260, 0x0100, 0x0001 }, //  8000*24*2
	{  512000, 0x01A0, 0x0180, 0x0002 }, // 16000*16*2 &  8000*32*2
	{  768000, 0x0260, 0x0100, 0x0002 }, // 16000*24*2
	{ 1024000, 0x0260, 0x0120, 0x0003 }, //            & 16000*32*2
	{ 1024032, 0x0260, 0x0120, 0x0003 }, // 32000*16*2+32
	{ 1411200, 0x01A0, 0x0100, 0x0004 }, // 44100*16*2
	{ 1536000, 0x0260, 0x0100, 0x0004 }, // 48000*16*2
	{ 1536032, 0x0260, 0x0100, 0x0004 }, // 32000*24*2+32
	{ 2048032, 0x0260, 0x0120, 0x0006 }, //            & 32000*32*2+32
	{ 2116800, 0x01A0, 0x0100, 0x0006 }, // 44100*24*2
	{ 2304000, 0x0260, 0x0100, 0x0006 }, // 48000*24*2
	{ 2822400, 0x01A0, 0x0100, 0x0008 }, //            & 44100*32*2
	{ 3072000, 0x0260, 0x0100, 0x0008 }, //            & 48000*32*2
};


static void fsm_mutex_lock(void)
{
	mutex_lock(&g_fsm_mutex);
}

static void fsm_mutex_unlock(void)
{
	mutex_unlock(&g_fsm_mutex);
}

static void fsm_delay_ms(uint32_t delay_ms)
{
	if (delay_ms > 0)
		usleep_range(delay_ms * 1000, delay_ms * 1000 + 1);
}

static uint16_t set_bf_val(
	uint16_t *pval, const uint16_t bf, const uint16_t bf_val)
{
	uint8_t len = (bf >> 12) & 0x0F;
	uint8_t pos = (bf >> 8) & 0x0F;
	uint16_t new_val;
	uint16_t old_val;
	uint16_t msk;

	if (pval == NULL)
		return 0xFFFF;

	old_val = new_val = *pval;
	msk = ((1 << (len + 1)) - 1) << pos;
	new_val &= ~msk;
	new_val |= bf_val << pos;
	*pval = new_val;

	return old_val;
}

static uint16_t get_bf_val(const uint16_t bf, const uint16_t val)
{
	uint8_t len = (bf >> 12) & 0x0F;
	uint8_t pos = (bf >> 8) & 0x0F;
	uint16_t msk, value;

	msk = ((1 << (len + 1)) - 1) << pos;
	value = (val & msk) >> pos;

	return value;
}

static int fsm_i2c_reg_read(
	struct fsm_dev *fsm_dev, uint8_t reg, uint16_t *pVal)
{
	struct i2c_msg msgs[2];
	uint8_t retries = 0;
	uint8_t buffer[2];
	int ret;

	if (!fsm_dev || !fsm_dev->i2c || !pVal) {
		pr_err("i2c read invalid");
		return -EINVAL;
	}

	// write register address.
	msgs[0].addr = fsm_dev->i2c->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;
	// read register buffer.
	msgs[1].addr = fsm_dev->i2c->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = &buffer[0];

	do {
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_transfer(fsm_dev->i2c->adapter,
				&msgs[0], ARRAY_SIZE(msgs));
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret != ARRAY_SIZE(msgs)) {
			fsm_delay_ms(5);
			retries++;
		}
	} while (ret != ARRAY_SIZE(msgs) && retries < FSM_I2C_RETRY);

	if (ret != ARRAY_SIZE(msgs)) {
		log_err(fsm_dev->dev, "read %02x transfer error: %d", reg, ret);
		return -EIO;
	}

	*pVal = ((buffer[0] << 8) | buffer[1]);

	return 0;
}

#if (!KERNEL_VERSION_HIGHER(4, 19, 0))
static int fsm_i2c_reg_write(struct fsm_dev *fsm_dev, uint8_t reg, uint16_t val)
{
	struct i2c_msg msgs[1];
	uint8_t retries = 0;
	uint8_t buffer[3];
	int ret;

	if (!fsm_dev || !fsm_dev->i2c) {
		pr_err("i2c write invalid");
		return -EINVAL;
	}

	buffer[0] = reg;
	buffer[1] = (val >> 8) & 0x00ff;
	buffer[2] = val & 0x00ff;
	msgs[0].addr = fsm_dev->i2c->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(buffer);
	msgs[0].buf = &buffer[0];

	do {
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_transfer(fsm_dev->i2c->adapter,
				&msgs[0], ARRAY_SIZE(msgs));
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret != ARRAY_SIZE(msgs)) {
			fsm_delay_ms(5);
			retries++;
		}
	} while (ret != ARRAY_SIZE(msgs) && retries < FSM_I2C_RETRY);

	if (ret != ARRAY_SIZE(msgs)) {
		log_err(fsm_dev->dev,
			"write %02x transfer error: %d", reg, ret);
		return -EIO;
	}

	return 0;
}
#endif

static int fsm_snd_soc_read(struct fsm_dev *fsm_dev, uint8_t reg, uint16_t *val)
{
	unsigned int value;
	int ret;

	if (fsm_dev == NULL || fsm_dev->codec == NULL || val == NULL) {
		pr_err("invalid parameters");
		return -EINVAL;
	}
#if KERNEL_VERSION_HIGHER(4, 19, 0)
	mutex_lock(&fsm_dev->i2c_lock);
	ret = snd_soc_component_read(fsm_dev->codec, reg, &value);
	mutex_unlock(&fsm_dev->i2c_lock);
#else
	ret = fsm_i2c_reg_read(fsm_dev, reg, (uint16_t *)&value);
//	value = snd_soc_read(fsm_dev->codec, reg);
//	if (value == -1) {
//		pr_err("read fail: %02X:%02X", fsm_dev->addr, reg);
//		ret = -EINVAL;
//	} else {
//		ret = 0;
//	}
#endif
	*val = (ret ? 0 : (uint16_t)value);
	dev_dbg(fsm_dev->dev, "R: %02X %04X\n", reg, *val);

	return ret;
}

static int fsm_snd_soc_write(struct fsm_dev *fsm_dev, uint8_t reg, uint16_t val)
{
	int ret;

	if (fsm_dev == NULL || fsm_dev->codec == NULL) {
		pr_err("fsm soc write invalid");
		return -EINVAL;
	}
#if KERNEL_VERSION_HIGHER(4, 19, 0)
	mutex_lock(&fsm_dev->i2c_lock);
	ret = snd_soc_component_write(fsm_dev->codec, reg, val);
	mutex_unlock(&fsm_dev->i2c_lock);
#else
	ret = fsm_i2c_reg_write(fsm_dev, reg, val);
	// ret = snd_soc_write(fsm_dev->codec, reg, val);
#endif
	dev_dbg(fsm_dev->dev, "W: %02X %04X\n", reg, val);

	return ret;
}

static int fsm_set_bf(
	struct fsm_dev *fsm_dev, const uint16_t bf, const uint16_t val)
{
	struct reg_unit reg;
	uint16_t oldval;
	uint16_t msk;
	int ret;

	if (fsm_dev == NULL)
		return -EINVAL;

	reg.len = (bf >> 12) & 0x0F;
	reg.pos = (bf >> 8) & 0x0F;
	reg.addr = bf & 0xFF;
	if (reg.len == 15)
		return fsm_snd_soc_write(fsm_dev, reg.addr, val);

	ret = fsm_snd_soc_read(fsm_dev, reg.addr, &oldval);
	if (ret) {
		log_err(fsm_dev->dev, "get bf:%04X failed", bf);
		return ret;
	}

	msk = ((1 << (reg.len + 1)) - 1) << reg.pos;
	reg.value = oldval & (~msk);
	reg.value |= val << reg.pos;

	if (oldval == reg.value)
		return 0;

	ret = fsm_snd_soc_write(fsm_dev, reg.addr, reg.value);
	if (ret) {
		log_err(fsm_dev->dev, "set bf:%04X failed", bf);
		return ret;
	}

	return ret;
}

static int fsm_get_bf(
	struct fsm_dev *fsm_dev, const uint16_t bf, uint16_t *pval)
{
	struct reg_unit reg;
	uint16_t msk;
	int ret;

	if (fsm_dev == NULL)
		return -EINVAL;
	reg.len = (bf >> 12) & 0x0F;
	reg.pos = (bf >> 8) & 0x0F;
	reg.addr = bf & 0xFF;
	ret = fsm_snd_soc_read(fsm_dev, reg.addr, &reg.value);
	if (ret) {
		log_err(fsm_dev->dev, "get bf:%04X failed", bf);
		return ret;
	}
	msk = ((1 << (reg.len + 1)) - 1) << reg.pos;
	reg.value &= msk;
	if (pval)
		*pval = reg.value >> reg.pos;

	return ret;
}

int fsm_read_status(struct fsm_dev *fsm_dev, uint8_t reg, uint16_t *pval)
{
	uint16_t value = 0;
	uint16_t old;
	int count = 0;
	int ret;

	if (fsm_dev == NULL && pval == NULL)
		return -EINVAL;

	while (count++ < FSM_I2C_RETRY) {
		ret = fsm_snd_soc_read(fsm_dev, reg, &value);
		if (ret)
			continue;
		if (count > 1 && old == value)
			break;
		old = value;
	}
	*pval = value;
	if (ret)
		log_err(fsm_dev->dev, "read status:%02X fail:%d", reg, ret);

	return ret;
}

static int fsm_reg_dump(struct fsm_dev *fsm_dev)
{
	uint16_t val[8];
	uint8_t idx;
	int ret;

	if (fsm_dev == NULL)
		return -EINVAL;

	for (idx = 0; idx < 0xD0; idx += 8) {
		ret  = fsm_snd_soc_read(fsm_dev, idx, &val[0]);
		ret |= fsm_snd_soc_read(fsm_dev, idx + 1, &val[1]);
		ret |= fsm_snd_soc_read(fsm_dev, idx + 2, &val[2]);
		ret |= fsm_snd_soc_read(fsm_dev, idx + 3, &val[3]);
		ret |= fsm_snd_soc_read(fsm_dev, idx + 4, &val[4]);
		ret |= fsm_snd_soc_read(fsm_dev, idx + 5, &val[5]);
		ret |= fsm_snd_soc_read(fsm_dev, idx + 6, &val[6]);
		ret |= fsm_snd_soc_read(fsm_dev, idx + 7, &val[7]);
		if (ret) {
			log_err(fsm_dev->dev, "dump fail:%d", ret);
			break;
		}
		log_info(fsm_dev->dev,
			"0x%02X: %04X %04X %04X %04X %04X %04X %04X %04X",
			idx, val[0], val[1], val[2], val[3],
			val[4], val[5], val[6], val[7]
		);
	}

	return ret;
}

static int fsm_enable_vddd(struct device *dev, bool enable)
{
	int ret = 0;
#if defined(CONFIG_REGULATOR)
	struct regulator *fsm_vddd;

	fsm_vddd = regulator_get(dev, "fsm_vddd");
	if (IS_ERR(fsm_vddd) != 0) {
		log_err(dev, "error getting fsm_vddd regulator");
		ret = PTR_ERR(fsm_vddd);
		fsm_vddd = NULL;
		return ret;
	}
	if (!enable) {
		log_info(dev, "disable regulator");
		regulator_disable(fsm_vddd);
		regulator_put(fsm_vddd);
		return 0;
	}
	log_info(dev, "enable regulator");
	regulator_set_voltage(fsm_vddd, 1800000, 1800000);
	ret = regulator_enable(fsm_vddd);
	if (ret < 0)
		log_err(dev, "enabling fsm_vddd failed: %d", ret);

	fsm_delay_ms(10);
#endif
	return ret;
}

static void *fsm_devm_kstrdup(struct device *dev, void *buf, size_t size)
{
	char *devm_buf = devm_kzalloc(dev, size + 1, GFP_KERNEL);

	if (devm_buf == NULL)
		return devm_buf;

	memcpy(devm_buf, buf, size);

	return devm_buf;
}

static void fsm_append_suffix(struct fsm_dev *fsm_dev, const char **str_name)
{
	char buf[FSM_STR_MAX];

	if (fsm_dev == NULL || *str_name == NULL) {
		pr_err("invalid parameters");
		return;
	}
	if (fsm_dev->chn_name == NULL)
		return;

	snprintf(buf, FSM_STR_MAX, "%s_%s", *str_name, fsm_dev->chn_name);
	*str_name = fsm_devm_kstrdup(fsm_dev->dev, buf, strlen(buf));
	log_info(fsm_dev->dev, "device name updated:%s", *str_name);
}

#ifdef CONFIG_REGMAP
static bool fsm_writeable_register(struct device *dev, uint32_t reg)
{
	return 1;
}

static bool fsm_readable_register(struct device *dev, uint32_t reg)
{
	return 1;
}

static bool fsm_volatile_register(struct device *dev, uint32_t reg)
{
	return 1;
}

static const struct regmap_config fsm_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = 0xFF,
	.val_format_endian = REGMAP_ENDIAN_BIG, //REGMAP_ENDIAN_NATIVE,
	.writeable_reg = fsm_writeable_register,
	.readable_reg = fsm_readable_register,
	.volatile_reg = fsm_volatile_register,
	.cache_type = REGCACHE_NONE,
};

static struct regmap *fsm_regmap_i2c_init(struct i2c_client *i2c)
{
	struct regmap *regmap;
	int ret;

	regmap = devm_regmap_init_i2c(i2c, &fsm_i2c_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		pr_err("alloc register map fail:%d", ret);
		return NULL;
	}

	return regmap;
}
#endif

static struct fsm_dev *fsm_get_drvdata_from_dai(struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec;

	if (dai == NULL) {
		pr_err("dai is null");
		return NULL;
	}
	codec = snd_soc_get_codec(dai);
	if (codec == NULL) {
		pr_err("codec is null");
		return NULL;
	}

	return snd_soc_codec_get_drvdata(codec);
}

static struct fsm_dev *fsm_get_drvdata_from_kctrl(struct snd_kcontrol *kcontrol)
{
	struct snd_soc_codec *codec;

	if (kcontrol == NULL) {
		pr_err("dai is null");
		return NULL;
	}
	codec = snd_soc_kcontrol_codec(kcontrol);
	if (codec == NULL) {
		pr_err("codec is null");
		return NULL;
	}

	return snd_soc_codec_get_drvdata(codec);
}

static struct fsm_dev *fsm_get_drvdata_from_widget(
		struct snd_soc_dapm_widget *widget)
{
	struct snd_soc_codec *codec;

	if (widget == NULL) {
		pr_err("dai is null");
		return NULL;
	}
	codec = snd_soc_dapm_to_codec(widget->dapm);
	if (codec == NULL) {
		pr_err("codec is null");
		return NULL;
	}

	return snd_soc_codec_get_drvdata(codec);
}

static struct snd_soc_dapm_context *fsm_get_dapm_from_codec(
		struct snd_soc_codec *codec)
{
#if (KERNEL_VERSION_HIGHER(4, 4, 0))
	return snd_soc_codec_get_dapm(codec);
#else
	return &codec->dapm;
#endif
}

static int fs1818_compat_32k_srate(struct fsm_dev *fsm_dev, int srate)
{
	uint16_t anactrl;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	anactrl = ((srate == 32000) ? 0x0101 : 0x0100);
	ret  = fsm_snd_soc_write(fsm_dev, REG(FS1818_OTPACC), 0xCA91);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_ANACTRL), anactrl);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_OTPACC), 0x0000);

	return ret;
}

static int fs1818_wait_boost(struct fsm_dev *fsm_dev, bool on)
{
	uint16_t val;
	int i = 0;
	int ret;

	fsm_delay_ms(5);
	while (i++ < 20) {
		fsm_delay_ms(2);
		ret = fsm_read_status(fsm_dev, REG(FS1818_BSTCTRL), &val);
		if (on && get_bf_val(FS1818_SSEND, val))
			return ret;
		else if (!on && !get_bf_val(FS1818_SSEND, val))
			return ret;
	}
	log_info(fsm_dev->dev, "wait boost %s timeout!", on ? "on" : "off");

	return ret;
}

static int fs1818_config_pll(struct fsm_dev *fsm_dev, int bclk)
{
	int idx;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	if (!fsm_dev->dev_init)
		return 0;

	for (idx = 0; idx < ARRAY_SIZE(g_fs1818_pll_tbl); idx++) {
		if (g_fs1818_pll_tbl[idx].bclk == bclk)
			break;
	}
	if (idx >= ARRAY_SIZE(g_fs1818_pll_tbl)) {
		log_err(fsm_dev->dev, "Not found bclk: %d", bclk);
		return -EINVAL;
	}
	ret = fsm_snd_soc_write(fsm_dev, REG(FS1818_PLLCTRL1),
			g_fs1818_pll_tbl[idx].c1);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_PLLCTRL2),
			g_fs1818_pll_tbl[idx].c2);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_PLLCTRL3),
			g_fs1818_pll_tbl[idx].c3);

	return ret;
}

static int fs1818_power_on(struct fsm_dev *fsm_dev)
{
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	ret = fsm_snd_soc_write(fsm_dev, REG(FS1818_SYSCTRL), 0x0008);
	fs1818_wait_boost(fsm_dev, true);

	return ret;
}

static int fs1818_power_off(struct fsm_dev *fsm_dev)
{
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	ret = fsm_snd_soc_write(fsm_dev, REG(FS1818_SYSCTRL), 0x0001);
	fs1818_wait_boost(fsm_dev, false);

	return ret;
}

static int fs1818_set_boost_mode(struct fsm_dev *fsm_dev, int mode)
{
	uint16_t bstctrl;
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	ret = fsm_read_status(fsm_dev, REG(FS1818_BSTCTRL), &bstctrl);
	/* Boost disable */
	set_bf_val(&bstctrl, FS1818_BSTEN, 0);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_BSTCTRL), bstctrl);
	if (get_bf_val(FS1818_SSEND, bstctrl))
		fs1818_wait_boost(fsm_dev, false);
	if (mode > 0) {
		set_bf_val(&bstctrl, FS1818_BST_MODE, mode);
		ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_BSTCTRL), bstctrl);
		set_bf_val(&bstctrl, FS1818_BSTEN, 1);
		ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_BSTCTRL), bstctrl);
	}
	fsm_dev->hw_params.boost_mode = mode;

	return ret;
}

static int fs1818_set_dac_gain(struct fsm_dev *fsm_dev, int dac_gain)
{
	uint16_t bstctrl;
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	ret = fsm_read_status(fsm_dev, REG(FS1818_BSTCTRL), &bstctrl);
	set_bf_val(&bstctrl, FS1818_BSTEN, 0);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_BSTCTRL), bstctrl);
	if (get_bf_val(FS1818_SSEND, bstctrl))
		fs1818_wait_boost(fsm_dev, false);
	set_bf_val(&bstctrl, FS1818_DAC_GAIN, dac_gain);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_BSTCTRL), bstctrl);
	set_bf_val(&bstctrl, FS1818_BSTEN, 1);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_BSTCTRL), bstctrl);

	fsm_dev->hw_params.dac_gain = dac_gain;

	return ret;
}


static int fs1818_set_do_output(struct fsm_dev *fsm_dev, int mode)
{
	uint16_t i2sctrl;
	uint16_t i2sset;
	uint16_t temp;
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	/* Notice:
	 * both left and right channel is enabled for mono use case.
	 */
	ret = fsm_snd_soc_read(fsm_dev, REG(FS1818_I2SCTRL), &i2sctrl);
	temp = i2sctrl;
	if (get_bf_val(FS1818_I2SDOE, i2sctrl)) {
		set_bf_val(&i2sctrl, FS1818_I2SDOE, 0);
		ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_I2SCTRL), i2sctrl);
		fsm_delay_ms(20);
	}
	ret |= fsm_read_status(fsm_dev, REG(FS1818_I2SSET), &i2sset);
	set_bf_val(&i2sset, FS1818_AECSELL, mode);
	set_bf_val(&i2sset, FS1818_AECSELR, mode);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_I2SSET), i2sset);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_I2SCTRL), temp);
	fsm_dev->hw_params.do_type = mode;

	return ret;
}

static int fs1818_set_mute(struct fsm_dev *fsm_dev, int mute)
{
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	ret = fsm_set_bf(fsm_dev, FS1818_DACMUTE, mute);
	if (mute)
		fsm_delay_ms(20);

	return ret;
}

static int fs1818_set_fmt(struct fsm_dev *fsm_dev, unsigned int fmt)
{
	uint16_t fmt_val;
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	ret = fsm_read_status(fsm_dev, REG(FS1818_I2SSET), &fmt_val);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		set_bf_val(&fmt_val, FS1818_LRCLKP, 0);
		set_bf_val(&fmt_val, FS1818_BCLKP, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		set_bf_val(&fmt_val, FS1818_LRCLKP, 1);
		set_bf_val(&fmt_val, FS1818_BCLKP, 0);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		set_bf_val(&fmt_val, FS1818_LRCLKP, 0);
		set_bf_val(&fmt_val, FS1818_BCLKP, 1);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		set_bf_val(&fmt_val, FS1818_LRCLKP, 1);
		set_bf_val(&fmt_val, FS1818_BCLKP, 1);
		break;
	default:
		set_bf_val(&fmt_val, FS1818_LRCLKP, 0);
		set_bf_val(&fmt_val, FS1818_BCLKP, 0);
		log_err(fsm_dev->dev, "invalid dai inverse, set to DAIFMT_NB_NF");
		break;
	}

	if (fsm_dev->amp_on) {
		ret |= fs1818_power_off(fsm_dev);
		ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_I2SSET), fmt_val);
		ret |= fs1818_power_on(fsm_dev);
	} else
		ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_I2SSET), fmt_val);

	return ret;
}

static int fs1818_hw_params(struct fsm_dev *fsm_dev)
{
	struct fsm_hw_params *fsm_params;
	uint16_t i2sctrl;
	int i2ssr;
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	fsm_params = &fsm_dev->hw_params;
	switch (fsm_params->sample_rate) {
	case 8000:
		i2ssr = 0;
		break;
	case 16000:
		i2ssr = 3;
		break;
	case 44100:
		i2ssr = 7;
		break;
	case 32000:
	case 48000:
		i2ssr = 8;
		break;
	default:
		log_err(fsm_dev->dev, "unsupport rate:%d",
			fsm_params->sample_rate);
		return -EINVAL;
	}
	ret = fsm_snd_soc_read(fsm_dev, REG(FS1818_I2SCTRL), &i2sctrl);
	set_bf_val(&i2sctrl, FS1818_I2SSR, i2ssr);
	set_bf_val(&i2sctrl, FS1818_I2SF, fsm_params->i2s_fmt);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_I2SCTRL), i2sctrl);
	ret |= fs1818_compat_32k_srate(fsm_dev, fsm_params->sample_rate);
	ret |= fs1818_config_pll(fsm_dev, fsm_params->freq_bclk);

	return ret;
}

static int fs1818_feedback_event(struct fsm_dev *fsm_dev, bool enable)
{
	int ret;

	ret = fsm_set_bf(fsm_dev, FS1818_I2SDOE, enable);
	fsm_dev->hw_params.do_enable = enable;

	return ret;
}

static int fs1818_update_otctrl(struct fsm_dev *fsm_dev)
{
	uint16_t otppg1w0;
	uint16_t otctrl;
	int new_val;
	int offset;
	int ret;

	ret = fsm_snd_soc_write(fsm_dev, REG(FS1818_OTPACC), 0xCA91);
	ret |= fsm_snd_soc_read(fsm_dev, REG(FS1818_OTPPG1W0), &otppg1w0);
	ret |= fsm_snd_soc_read(fsm_dev, REG(FS1818_OTCTRL), &otctrl);
	offset = (otppg1w0 & 0x007F);
	if ((otppg1w0 & 0x0080) != 0)
		offset = (-1 * offset);

	do {
		new_val = offset + LOW8(otctrl);
		if (new_val < 0 || new_val > 0xFF)
			break;
		otctrl = ((otctrl & 0xFF00) | new_val);
		new_val = offset + HIGH8(otctrl);
		if (new_val < 0 || new_val > 0xFF)
			break;
		otctrl = ((otctrl & 0x00FF) | (new_val << 8));
		ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_OTCTRL), otctrl);
	} while (0);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_OTPACC), 0x0000);

	return ret;
}

static int fs1818_i2c_reset(struct fsm_dev *fsm_dev)
{
	uint16_t val;
	int i, ret;

	for (i = 0; i < FSM_I2C_RETRY; i++) {
		fsm_snd_soc_write(fsm_dev, REG(FS1818_SYSCTRL), 0x0002);
		fsm_snd_soc_read(fsm_dev, REG(FS1818_SYSCTRL), &val);
		fsm_delay_ms(15); // 15ms
		ret = fsm_snd_soc_write(fsm_dev, REG(FS1818_SYSCTRL), 0x0001);
		// check init finish flag
		ret |= fsm_read_status(fsm_dev, REG(FS1818_CHIPINI), &val);
		if ((val == 0x0003) || (val == 0x0300))
			break;
	}
	if (i == FSM_I2C_RETRY) {
		log_err(fsm_dev->dev, "reset timeout!");
		return -ETIMEDOUT;
	}

	return ret;
}

static int fs1818_dev_init(struct fsm_dev *fsm_dev)
{
	uint16_t val;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	ret = fs1818_i2c_reset(fsm_dev);
	if (ret) {
		log_err(fsm_dev->dev, "i2c reset fail:%d", ret);
		return ret;
	}
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_SYSCTRL), 0x0000);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_PLLCTRL4), 0x000B);
	fsm_delay_ms(10);
	ret |= fs1818_update_otctrl(fsm_dev);
	val  = ((uint16_t)fsm_dev->digi_vol << 8);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_AUDIOCTRL), val);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_DSPCTRL), 0x0802);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_DACCTRL), 0x0310);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_TSCTRL), 0x6623);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_MODCTRL), 0x800A);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_BSTCTRL), 0x19EE);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_OTPACC), 0xCA91);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_CLDCTRL), 0x0000);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_AUXCFG), 0x1020);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_OTPACC), 0x0000);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_SYSCTRL), 0x0001);
	fsm_delay_ms(10);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_PLLCTRL4), 0x0000);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_I2SCTRL), 0x801B);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_TEMPSEL), 0x0188);
	fsm_dev->dev_init = true;
	if (ret) {
		fsm_dev->dev_init = false;
		log_err(fsm_dev->dev, "init fail:%d", ret);
	}

	return ret;
}

static int fs1818_start_up(struct fsm_dev *fsm_dev, int unmute)
{
	struct fsm_hw_params *fsm_params;
	uint16_t value;
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	fsm_params = &fsm_dev->hw_params;
	ret = fsm_read_status(fsm_dev, REG(FS1818_TEMPSEL), &value);
	if (value == 0x0010) { // default value
		ret |= fs1818_dev_init(fsm_dev);
		ret |= fs1818_set_fmt(fsm_dev, fsm_params->dai_fmt);
		ret |= fs1818_hw_params(fsm_dev);
		ret |= fs1818_set_boost_mode(fsm_dev, fsm_params->boost_mode);
		fsm_dev->start_up = false;
	}
	if (fsm_dev->start_up)
		return ret;

	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_PLLCTRL4), 0x000B);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_SYSCTRL), 0x0008);
	fsm_dev->start_up = true;
	if (unmute) {
		fs1818_wait_boost(fsm_dev, true);
		ret |= fs1818_set_mute(fsm_dev, false);
	}

	return ret;
}

static int fs1818_shut_down(struct fsm_dev *fsm_dev, int mute)
{
	int ret;

	if (!fsm_dev->dev_init)
		return 0;

	if (mute)
		ret = fs1818_set_mute(fsm_dev, true);

	ret = fsm_snd_soc_write(fsm_dev, REG(FS1818_SYSCTRL), 0x0001);
	ret |= fsm_snd_soc_write(fsm_dev, REG(FS1818_PLLCTRL4), 0x0000);
	fsm_dev->start_up = false;
	fs1818_wait_boost(fsm_dev, false);

	return ret;
}

static int fsm_set_monitor(struct fsm_dev *fsm_dev, bool enable)
{
	if (fsm_dev == NULL || fsm_dev->fsm_wq == NULL)
		return -EINVAL;

	if (!fsm_dev->dev_init || !fsm_dev->montr_en)
		return 0;

	if (enable && fsm_dev->amp_on) {
		queue_delayed_work(fsm_dev->fsm_wq,
				&fsm_dev->monitor_work, 1*HZ);
	} else if (!enable) {
		cancel_delayed_work_sync(&fsm_dev->monitor_work);
	}

	return 0;
}

static void fsm_work_monitor(struct work_struct *work)
{
	struct fsm_dev *fsm_dev;
	uint16_t val;
	int ret;
	int flag = 0;

	fsm_dev = container_of(work, struct fsm_dev, monitor_work.work);
	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return;
	}
	log_debug(fsm_dev->dev, "status monitoring");

	fsm_mutex_lock();
	ret = fsm_read_status(fsm_dev, REG(FS1818_STATUS), &val);
	fsm_mutex_unlock();
	if (ret) {
		log_err(fsm_dev->dev, "get status fail:%d", ret);
		return;
	}
	if (!get_bf_val(FS1818_BOVDS, val)) {
		log_err(fsm_dev->dev, "boost overvoltage detected");
		flag = 1;
	}
	if (!get_bf_val(FS1818_PLLS, val)) {
		log_err(fsm_dev->dev, "PLL is not in lock");
		flag = 1;
	}
	if (!get_bf_val(FS1818_OTDS, val)) {
		log_err(fsm_dev->dev, "overtemperature detected");
		flag = 1;
	}
	if (!get_bf_val(FS1818_OVDS, val)) {
		log_err(fsm_dev->dev, "overvoltage detected on VBAT");
		flag = 1;
	}
	if (!get_bf_val(FS1818_UVDS, val)) {
		log_err(fsm_dev->dev, "undervoltage detected on VBAT");
		flag = 1;
	}
	if (get_bf_val(FS1818_OCDS, val)) {
		log_err(fsm_dev->dev, "overcurrent detected in amplifier");
		flag = 1;
	}
	if (!get_bf_val(FS1818_CLKS, val)) {
		log_err(fsm_dev->dev, "the ration bclk/ws is not stable");
		flag = 1;
	}

	fsm_mutex_lock();
	if (flag)
		fsm_reg_dump(fsm_dev);
	if (fsm_dev->amp_on)
		ret |= fs1818_start_up(fsm_dev, 1);
	fsm_mutex_unlock();

	if (fsm_dev->montr_en && fsm_dev->amp_on) {
		/* reschedule */
		queue_delayed_work(fsm_dev->fsm_wq, &fsm_dev->monitor_work,
				fsm_dev->montr_pd*HZ);
	}
}

static int fsm_dac_event(struct snd_soc_dapm_widget *widget,
			struct snd_kcontrol *kcontrol, int event)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_widget(widget);
	int ret = 0;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	log_debug(fsm_dev->dev, "event:%d", event);

	if (!fsm_dev->dev_init)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		log_info(fsm_dev->dev, "DAC PRE_PMU");
		fsm_mutex_lock();
		ret = fs1818_start_up(fsm_dev, 0);
		fsm_mutex_unlock();
		break;

	case SND_SOC_DAPM_PRE_PMD:
		log_info(fsm_dev->dev, "DAC PRE_PMD");
		fsm_mutex_lock();
		ret = fs1818_shut_down(fsm_dev, 0);
		fsm_mutex_unlock();
		break;

	default:
		break;
	}

	return ret;
}

static int fsm_dac_feedback_event(struct snd_soc_dapm_widget *widget,
			struct snd_kcontrol *kcontrol, int event)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_widget(widget);
	int ret = 0;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	log_debug(fsm_dev->dev, "event:%d", event);

	if (!fsm_dev->dev_init)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		log_info(fsm_dev->dev, "FEEDBACK PRE_PMU");
		fsm_mutex_lock();
		ret = fs1818_feedback_event(fsm_dev, true);
		fsm_mutex_unlock();
		break;

	case SND_SOC_DAPM_PRE_PMD:
		log_info(fsm_dev->dev, "FEEDBACK PRE_PMD");
		fsm_mutex_lock();
		ret = fs1818_feedback_event(fsm_dev, false);
		fsm_mutex_unlock();
		break;
	default:
		break;
	}

	return ret;
}

static int fsm_amp_switch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = (fsm_dev->amp_on ? 1 : 0);

	return 0;
}

static int fsm_amp_switch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	int enable;
	int ret = 0;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	enable = (int)ucontrol->value.integer.value[0];
	if (enable && !fsm_dev->amp_on) {
		fsm_mutex_lock();
		ret = fs1818_start_up(fsm_dev, 1);
		fsm_dev->amp_on = true;
		fsm_mutex_unlock();
		fsm_set_monitor(fsm_dev, true);
	} else if (!enable && fsm_dev->amp_on) {
		fsm_set_monitor(fsm_dev, false);
		fsm_mutex_lock();
		fsm_dev->amp_on = false;
		ret = fs1818_shut_down(fsm_dev, 1);
		fsm_mutex_unlock();
	}

	return ret;
}

static int fsm_boost_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = fsm_dev->hw_params.boost_mode;

	return 0;
}

static int fsm_boost_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	int mode;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	mode = (int)ucontrol->value.integer.value[0];
	if (mode < 0 || mode > 3) {
		log_err(fsm_dev->dev, "invalid mode:%d", mode);
		return -EINVAL;
	}
	fsm_mutex_lock();
	ret = fs1818_set_boost_mode(fsm_dev, mode);
	fsm_mutex_unlock();
	if (ret)
		log_err(fsm_dev->dev, "set mode:%d fail:%d", mode, ret);

	return ret;
}

static int fsm_dac_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	uint16_t val;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}

	fsm_mutex_lock();
	ret = fsm_read_status(fsm_dev, REG(FS1818_BSTCTRL), &val);
	fsm_mutex_unlock();

	val = get_bf_val(FS1818_DAC_GAIN, val);
	if (val < 0 || val > 3) {
		log_err(fsm_dev->dev, "invalid dac gain:%d", val);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int fsm_dac_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	int val;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}

	val = (int)ucontrol->value.integer.value[0];
	if (val < 0 || val > 3) {
		log_err(fsm_dev->dev, "invalid dac gain:%d", val);
		return -EINVAL;
	}

	fsm_mutex_lock();
	ret = fs1818_set_dac_gain(fsm_dev, val);
	fsm_mutex_unlock();
	if (ret)
		log_err(fsm_dev->dev, "set dac_gain:%d fail:%d", val, ret);

	return ret;
}

static int fsm_i2s_format_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	uint16_t val = 0;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	fsm_mutex_lock();
	ret = fsm_get_bf(fsm_dev, FS1818_I2SF, &val);
	fsm_mutex_unlock();
	switch (val) {
	case 2: // MSB
		val = 0;
		break;
	case 3: // I2S
		val = 1;
		break;
	case 4: // LSB_16B
		val = 2;
		break;
	case 6: // LSB_20B
		val = 3;
		break;
	case 7: // LSB_24B
		val = 4;
		break;
	default: // RSVD
		val = 5;
		break;
	}
	ucontrol->value.integer.value[0] = val;

	return ret;
}

static int fsm_i2s_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	int val;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	val = (int)ucontrol->value.integer.value[0];
	switch (val) {
	case 0: // MSB
		val = 2;
		break;
	case 1: // I2S
		val = 3;
		break;
	case 2: // LSB_16B
		val = 4;
		break;
	case 3: // LSB_20B
		val = 6;
		break;
	case 4: // LSB_24B
		val = 7;
		break;
	default:
		val = 3;
		break;
	}
	fsm_mutex_lock();
	ret = fsm_set_bf(fsm_dev, FS1818_I2SF, val);
	fsm_mutex_unlock();

	return ret;
}

static int fsm_i2s_channel_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	uint16_t val = 0;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	fsm_mutex_lock();
	ret = fsm_get_bf(fsm_dev, FS1818_CHS12, &val);
	fsm_mutex_unlock();
	if (val <= 1) {
		/* left: 0 or 1 */
		ucontrol->value.integer.value[0] = 0;
	} else {
		ucontrol->value.integer.value[0] = val - 1;
	}

	return ret;
}

static int fsm_i2s_channel_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	int val;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	val = (int)ucontrol->value.integer.value[0] + 1;
	fsm_mutex_lock();
	ret = fsm_set_bf(fsm_dev, FS1818_CHS12, val);
	fsm_mutex_unlock();

	return ret;
}

static int fsm_i2s_out_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = fsm_dev->hw_params.do_type;

	return 0;
}

static int fsm_i2s_out_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fsm_dev *fsm_dev = fsm_get_drvdata_from_kctrl(kcontrol);
	int mode;
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	/* Notice:
	 * both left and right channel is enabled for mono use case.
	 */
	mode = (int)ucontrol->value.integer.value[0];
	fsm_mutex_lock();
	ret = fs1818_set_do_output(fsm_dev, mode);
	fsm_mutex_unlock();
	if (ret) {
		log_err(fsm_dev->dev, "set mode:%d fail:%d", mode, ret);
		return ret;
	}

	return ret;
}

static const char * const fsm_i2s_format_txt[] = {
	"MSB", "I2S", "LSB_16B", "LSB_20B", "LSB_24B", "RSVD"
};

static const struct soc_enum fsm_i2s_format_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsm_i2s_format_txt),
		fsm_i2s_format_txt);

static const char * const fsm_i2s_channel_txt[] = {
	"Left", "Right", "Mono"
};

static const struct soc_enum fsm_i2s_channel_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsm_i2s_channel_txt),
		fsm_i2s_channel_txt);

static const char * const fsm_bst_mode_txt[] = {
	"Disable", "Boost", "Adaptive", "Follow"
};

static const char * const fsm_dac_gain_txt[] = {
	"0dB", "7.1dB", "10.4dB", "16.5dB"
};

static const struct soc_enum fsm_bst_mode_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsm_bst_mode_txt),
		fsm_bst_mode_txt);

static const struct soc_enum fsm_dac_gain_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsm_dac_gain_txt),
		fsm_dac_gain_txt);

static const char * const fsm_i2s_out_txt[] = {
	"I2SIN", "AGC_OUT", "AEC_REF"
};

static const char * const fsm_switch_state_txt[] = {
	"Off", "On"
};

static const struct soc_enum fsm_switch_state_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsm_switch_state_txt),
		fsm_switch_state_txt);

static const struct soc_enum fsm_i2s_out_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsm_i2s_out_txt),
		fsm_i2s_out_txt);

static const DECLARE_TLV_DB_SCALE(fsm_volume_tlv, -9690, 38, 0);

static const struct snd_kcontrol_new fsm_snd_controls[] = {
	SOC_ENUM_EXT("fs18xx_amp_switch", fsm_switch_state_enum,
			fsm_amp_switch_get, fsm_amp_switch_put),
	SOC_ENUM_EXT("fs18xx_boost_mode", fsm_bst_mode_enum,
			fsm_boost_mode_get, fsm_boost_mode_put),
	SOC_ENUM_EXT("fs18xx_dac_gain", fsm_dac_gain_enum,
			fsm_dac_gain_get, fsm_dac_gain_put),
	SOC_ENUM_EXT("fs18xx_i2s_format", fsm_i2s_format_enum,
			fsm_i2s_format_get, fsm_i2s_format_put),
	SOC_ENUM_EXT("fs18xx_i2s_channel", fsm_i2s_channel_enum,
			fsm_i2s_channel_get, fsm_i2s_channel_put),
	SOC_SINGLE("fs18xx_i2s_out_onoff", REG(FS1818_I2SCTRL), 11, 1, 0),
	SOC_ENUM_EXT("fs18xx_i2s_out", fsm_i2s_out_enum,
			fsm_i2s_out_get, fsm_i2s_out_put),
	SOC_SINGLE("fs18xx_bclk_invert", REG(FS1818_I2SSET), 6, 1, 0),
	SOC_SINGLE("fs18xx_ws_invert", REG(FS1818_I2SSET), 5, 1, 0),
	SOC_SINGLE_TLV("fs18xx_volume", REG(FS1818_AUDIOCTRL), 8, 0xFF, 0,
			fsm_volume_tlv),
	SOC_SINGLE("fs18xx_mute", REG(FS1818_DACCTRL), 8, 1, 0),
	SOC_SINGLE("fs18xx_fade", REG(FS1818_DACCTRL), 9, 1, 0),
};

static struct snd_soc_dapm_widget fsm_dapm_widgets_common[] = {
	/* Stream widgets */
	SND_SOC_DAPM_DAC_E("DAC", "Playback", SND_SOC_NOPM, 0, 0,
			fsm_dac_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_ADC_E("DAC_FEEDBACK", "Capture", SND_SOC_NOPM, 0, 0,
			fsm_dac_feedback_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT"),
	SND_SOC_DAPM_INPUT("SDO"),
};

static const struct snd_soc_dapm_route fsm_dapm_routes_common[] = {
	/* sink, control, source */
	{ "OUT", NULL, "DAC" },
	{ "DAC_FEEDBACK", NULL, "SDO" },
};

static int fsm_add_widgets(struct snd_soc_codec *codec, struct fsm_dev *fsm_dev)
{
	struct snd_kcontrol_new *kctrl_new;
	struct snd_soc_dapm_context *dapm;
	int count;
	int i;

	if (codec == NULL || fsm_dev == NULL) {
		pr_err("invalid parameters");
		return -EINVAL;
	}
	dapm = fsm_get_dapm_from_codec(codec);
	if (dapm == NULL) {
		log_err(fsm_dev->dev, "get dapm failed");
		return -EINVAL;
	}

	count = ARRAY_SIZE(fsm_snd_controls);
	kctrl_new = devm_kzalloc(fsm_dev->dev,
		count * sizeof(struct snd_kcontrol_new), GFP_KERNEL);
	if (kctrl_new == NULL)
		return -EINVAL;

	memcpy(kctrl_new, fsm_snd_controls,
			count * sizeof(struct snd_kcontrol_new));
	if (fsm_dev->chn_name != NULL) {
		for (i = 0; i < count; i++) {
			fsm_append_suffix(fsm_dev,
					(const char **)&kctrl_new[i].name);
		}
	}
	snd_soc_add_codec_controls(codec, kctrl_new, count);

	snd_soc_dapm_new_controls(dapm, fsm_dapm_widgets_common,
				ARRAY_SIZE(fsm_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, fsm_dapm_routes_common,
				ARRAY_SIZE(fsm_dapm_routes_common));

	snd_soc_dapm_ignore_suspend(dapm, "Playback");
	snd_soc_dapm_ignore_suspend(dapm, "OUT");
	snd_soc_dapm_sync(dapm);

	return 0;
}

static int fsm_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct fsm_dev *fsm_dev;
	int ret;

	fsm_dev = fsm_get_drvdata_from_dai(dai);

	if (!fsm_dev) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}

	if (!substream->runtime) {
		log_info(fsm_dev->dev, "runtime is null");
		return 0;
	}

	ret = snd_pcm_hw_constraint_mask64(substream->runtime,
			SNDRV_PCM_HW_PARAM_FORMAT, FSM_FORMATS);
	if (ret < 0) {
		log_err(fsm_dev->dev, "set pcm param format fail:%d", ret);
		return ret;
	}

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&fsm_constraints);
	if (ret < 0) {
		log_err(fsm_dev->dev, "set pcm param sample rate fail:%d", ret);
		return ret;
	}

	log_info(fsm_dev->dev, "done");
	return 0;
}

static void fsm_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct fsm_dev *fsm_dev;

	fsm_dev = fsm_get_drvdata_from_dai(dai);
	if (fsm_dev)
		log_info(fsm_dev->dev, "done");
}

static int fsm_set_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct fsm_dev *fsm_dev;

	fsm_dev = fsm_get_drvdata_from_dai(codec_dai);
	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}
	log_info(fsm_dev->dev, "sysclk freq:%d", freq);
	fsm_dev->hw_params.freq_bclk = freq;

	return 0;
}

static int fsm_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct fsm_dev *fsm_dev;
	int ret;

	fsm_dev = fsm_get_drvdata_from_dai(dai);

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}

	log_info(fsm_dev->dev, "dai format:0x%X", fmt);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	default:
		// only supports Slave mode
		log_err(fsm_dev->dev, "invalid DAI master/slave interface");
		return -EINVAL;
	}

	fsm_mutex_lock();
	fsm_dev->hw_params.dai_fmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
	ret = fs1818_set_fmt(fsm_dev, fmt);
	fsm_mutex_unlock();

	return ret;
}

static int fsm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct fsm_hw_params *fsm_params;
	struct fsm_dev *fsm_dev;
	unsigned int format;
	int ret;

	fsm_dev = fsm_get_drvdata_from_dai(dai);
	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}

	fsm_params = &fsm_dev->hw_params;
	format = params_format(params);
	switch (format) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		fsm_params->bit_width = 32;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		fsm_params->bit_width = 24;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		fsm_params->bit_width = 16;
		break;
	default:
		log_err(fsm_dev->dev, "invalid pcm format:%X", format);
		return -EINVAL;
	}
	fsm_params->sample_rate = params_rate(params);
	log_debug(fsm_dev->dev,
		"pcm format:%d, rate:%d, width:%d & %d", format,
		fsm_params->sample_rate,
		snd_pcm_format_width(format),
		snd_pcm_format_physical_width(format));

	if (params_channels(params) <= 2)
		fsm_params->channel = 2;
	else {
		log_err(fsm_dev->dev, "invalid channel:%d", params_channels(params));
		return -EINVAL;
	}

	fsm_params->freq_bclk = fsm_params->sample_rate \
		* fsm_params->bit_width \
		* fsm_params->channel;
	if (fsm_params->sample_rate == 32000)
		fsm_params->freq_bclk += 32;

	switch (fsm_params->dai_fmt) {
	case SND_SOC_DAIFMT_I2S:
		fsm_params->i2s_fmt = 3;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		if (fsm_params->bit_width == 16)
			fsm_params->i2s_fmt = 4;
		else
			fsm_params->i2s_fmt = 7;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		fsm_params->i2s_fmt = 2;
		break;
	default:
		log_err(fsm_dev->dev, "invalid dai format:%X", fsm_params->dai_fmt);
		return -EINVAL;
	}

	log_info(fsm_dev->dev, "rate:%d, chn:%d, width:%d, bclk:%d",
		fsm_params->sample_rate,
		fsm_params->channel,
		fsm_params->bit_width,
		fsm_params->freq_bclk);

	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
		log_info(fsm_dev->dev, "capture stream");
		return 0;
	}

	fsm_mutex_lock();
	if (fsm_dev->start_up) {
		ret = fs1818_shut_down(fsm_dev, fsm_dev->amp_on);
		ret |= fs1818_hw_params(fsm_dev);
		ret |= fs1818_start_up(fsm_dev, fsm_dev->amp_on);
	} else
		ret = fs1818_hw_params(fsm_dev);
	fsm_mutex_unlock();

	return ret;
}

static int fsm_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	struct fsm_dev *fsm_dev;
	int ret;

	fsm_dev = fsm_get_drvdata_from_dai(dai);
	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}

	if (stream != SNDRV_PCM_STREAM_PLAYBACK) {
		log_info(fsm_dev->dev, "capture stream");
		return 0;
	}

	if (mute) {
		fsm_set_monitor(fsm_dev, false);
		fsm_mutex_lock();
		fsm_dev->amp_on = !mute;
		ret = fs1818_set_mute(fsm_dev, mute);
		fsm_mutex_unlock();
		log_info(fsm_dev->dev, "playback mute");
	} else {
		fsm_mutex_lock();
		fsm_dev->amp_on = !mute;
		ret = fs1818_set_mute(fsm_dev, mute);
		fsm_mutex_unlock();
		log_info(fsm_dev->dev, "playback unmute");
		fsm_set_monitor(fsm_dev, true);
	}

	return ret;
}

static ssize_t fsm_reg_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct fsm_dev *fsm_dev = container_of(class,
			struct fsm_dev, fsm_class);
	int ret;

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}

	fsm_mutex_lock();
	ret = fsm_reg_dump(fsm_dev);
	fsm_mutex_unlock();

	return scnprintf(buf, PAGE_SIZE, "Log DUMP\n");
}

static ssize_t fsm_reg_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t len)
{
	return len;
}

static ssize_t fsm_montr_en_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct fsm_dev *fsm_dev = container_of(class,
			struct fsm_dev, fsm_class);

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null");
		return -EINVAL;
	}

	return scnprintf(buf, PAGE_SIZE, "montr_en:%d\n",
			fsm_dev->montr_en);
}

static ssize_t fsm_montr_en_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t len)
{
	struct fsm_dev *fsm_dev = container_of(class,
			struct fsm_dev, fsm_class);
	int enable;
	int ret;

	ret = kstrtoint(buf, 10, &enable);
	if (ret)
		return -EINVAL;

	if (enable && !fsm_dev->montr_en) {
		fsm_mutex_lock();
		fsm_dev->montr_en = true;
		fsm_mutex_unlock();
		fsm_set_monitor(fsm_dev, true);
	} else if (!enable && fsm_dev->montr_en) {
		fsm_set_monitor(fsm_dev, false);
		fsm_mutex_lock();
		fsm_dev->montr_en = false;
		fsm_mutex_unlock();
	}
	log_info(fsm_dev->dev, "enable:%d, montr_en:%d",
		enable, fsm_dev->montr_en);

	return (ssize_t)len;
}

#if (KERNEL_VERSION_HIGHER(4, 14, 0))
static CLASS_ATTR_RW(fsm_reg);
static CLASS_ATTR_RW(fsm_montr_en);

static struct attribute *fsm_class_attrs[] = {
	&class_attr_fsm_reg.attr,
	&class_attr_fsm_montr_en.attr,
	NULL,
};
ATTRIBUTE_GROUPS(fsm_class);

/** Device model classes */
static struct class g_fsm_class = {
	.name = FS18XX_DRV_NAME,
	.class_groups = fsm_class_groups,
};
#else /* LINUX_VERSION_CODE < kernel-4.14 */
static struct class_attribute g_fsm_class_attrs[] = {
	__ATTR(fsm_reg, 0644, fsm_reg_show, fsm_reg_store),
	__ATTR(fsm_montr_en, 0644, fsm_montr_en_show, fsm_montr_en_store),
	__ATTR_NULL
};

static struct class g_fsm_class = {
	.name = FS18XX_DRV_NAME,
	.class_attrs = g_fsm_class_attrs,
};
#endif

int fsm_sysfs_init(struct fsm_dev *fsm_dev)
{
	int ret;

	// path: sys/class/$(FS18XX_DRV_NAME)
	fsm_dev->fsm_class = g_fsm_class;
	fsm_append_suffix(fsm_dev, &fsm_dev->fsm_class.name);
	ret = class_register(&fsm_dev->fsm_class);
	if (ret)
		log_err(fsm_dev->dev, "add class device fail:%d", ret);

	return ret;
}

static void fsm_sysfs_deinit(struct fsm_dev *fsm_dev)
{
	class_unregister(&fsm_dev->fsm_class);
}

static const struct snd_soc_dai_ops fsm_aif_dai_ops = {
	.startup      = fsm_startup,
	.set_fmt      = fsm_set_fmt,
	.set_sysclk   = fsm_set_sysclk,
	.hw_params    = fsm_hw_params,
	.mute_stream  = fsm_mute_stream,
	.shutdown     = fsm_shutdown,
};

static struct snd_soc_dai_driver fsm_aif_dai[] = {
	{
		.name = "fs18xx-aif",
		.id = 1,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = FSM_RATES,
			.formats = FSM_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = FSM_RATES,
			.formats = FSM_FORMATS,
		},
		.ops = &fsm_aif_dai_ops,
		.symmetric_rates = 1,
#if (KERNEL_VERSION_HIGHER(3, 14, 0))
		.symmetric_channels = 1,
		//.symmetric_samplebits = 1,
#endif
	},
};

static int fsm_codec_probe(struct snd_soc_codec *codec)
{
	struct fsm_dev *fsm_dev;
	int ret;

	if (codec == NULL) {
		pr_err("codec is NULL");
		return -EINVAL;
	}

	fsm_dev = snd_soc_codec_get_drvdata(codec);
	if (fsm_dev == NULL) {
		pr_err("fsm_dev is NULL");
		return -EINVAL;
	}

	log_debug(fsm_dev->dev, "dev_name: %s", dev_name(codec->dev));
	fsm_dev->codec = codec;
	fsm_mutex_lock();
	ret = fs1818_dev_init(fsm_dev);
	fsm_mutex_unlock();
	if (ret) {
		log_err(fsm_dev->dev, "init fail:%d", ret);
		return ret;
	}
	ret = fsm_add_widgets(codec, fsm_dev);
	log_info(fsm_dev->dev, "%s!", ret ? "fail" : "done");

	return ret;
}

static remove_ret_type fsm_codec_remove(struct snd_soc_codec *codec)
{
	struct fsm_dev *fsm_dev;

	if (codec == NULL)
		return remove_ret_val;

	fsm_dev = snd_soc_codec_get_drvdata(codec);
	if (fsm_dev == NULL)
		return remove_ret_val;

	log_info(fsm_dev->dev, "codec remove done");

	return remove_ret_val;
}

static struct snd_soc_codec_driver soc_codec_dev_fsm = {
	.probe  = fsm_codec_probe,
	.remove = fsm_codec_remove,
};

static int fsm_codec_register(struct fsm_dev *fsm_dev, int id)
{
	struct snd_soc_dai_driver *dai;
	int i, ret;

	log_info(fsm_dev->dev, "id:%d", id);
	dai = devm_kzalloc(fsm_dev->dev, sizeof(fsm_aif_dai), GFP_KERNEL);
	if (dai == NULL) {
		log_err(fsm_dev->dev, "alloc codec dai fail");
		return -EINVAL;
	}
	memcpy(dai, fsm_aif_dai, sizeof(fsm_aif_dai));
	for (i = 0; i < ARRAY_SIZE(fsm_aif_dai); i++)
		fsm_append_suffix(fsm_dev, &dai[i].name);

	ret = snd_soc_register_codec(fsm_dev->dev, &soc_codec_dev_fsm,
			dai, ARRAY_SIZE(fsm_aif_dai));
	if (ret < 0)
		log_err(fsm_dev->dev, "failed to register CODEC DAI: %d", ret);

	return ret;
}

static void fsm_codec_unregister(struct fsm_dev *fsm_dev)
{
	log_info(fsm_dev->dev, "codec unregister");
	snd_soc_unregister_codec(fsm_dev->dev);
}

#ifdef CONFIG_OF
static int fsm_parse_dts(struct i2c_client *i2c, struct fsm_dev *fsm_dev)
{
	struct device_node *np = i2c->dev.of_node;
	int ret;

	if (fsm_dev == NULL || np == NULL) {
		pr_err("invalid parameter");
		return -EINVAL;
	}

	ret = of_property_read_string(np, "fsm,channel_name",
		&fsm_dev->chn_name);

	ret = of_property_read_u8(np, "fsm,digital_volume",
		&fsm_dev->digi_vol);

	ret = of_property_read_bool(np, "fsm,monitor_enable");
	if (!ret)
		fsm_dev->montr_en = false;

	ret = of_property_read_u8(np, "fsm,monitor_period",
		&fsm_dev->montr_pd);

	if (fsm_dev->montr_pd == 0)
		fsm_dev->montr_pd = 1;

	return 0;
}

const static struct of_device_id fsm_match_tbl[] = {
	{ .compatible = "foursemi,fs18xx" },
	{},
};
MODULE_DEVICE_TABLE(of, fsm_match_tbl);
#endif

static int fsm_detect_device(struct fsm_dev *fsm_dev, uint8_t addr)
{
	uint16_t id = 0;
	int ret;

	if (fsm_dev == NULL)
		return -EINVAL;

	fsm_dev->addr = addr;
	ret = fsm_i2c_reg_read(fsm_dev, REG(FS1818_DEVID), &id);

	switch (HIGH8(id)) {
	case FS18XM_DEV_ID:
	case FS18HF_DEV_ID:
		fsm_dev->use_irq = false;
		break;
	default:
		log_err(fsm_dev->dev, "Invalid ID:%04X", id);
		return -EINVAL;
	}
	fsm_dev->version = id;
	log_info(fsm_dev->dev, "Found DEVICE:%04X", id);

	return 0;
}

static int fsm_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct fsm_hw_params *fsm_params;
	struct fsm_dev *fsm_dev;
	int ret;

	log_info(&i2c->dev, "version: %s", FSM_CODE_VERSION);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		log_err(&i2c->dev, "check I2C_FUNC_I2C failed");
		return -EIO;
	}

	fsm_dev = devm_kzalloc(&i2c->dev, sizeof(struct fsm_dev), GFP_KERNEL);
	if (fsm_dev == NULL)
		return -ENOMEM;

	memset(fsm_dev, 0, sizeof(struct fsm_dev));
	mutex_init(&fsm_dev->i2c_lock);
	fsm_dev->i2c = i2c;
	fsm_dev->dev = &i2c->dev;
	fsm_dev->amp_on = false;
	fsm_dev->montr_en = true;
	fsm_dev->montr_pd = 1; // 1 sec
	fsm_dev->digi_vol = 0xF4; // -4.125dB
	fsm_dev->chn_name = NULL;

	fsm_params = &fsm_dev->hw_params;
	fsm_params->dai_fmt = SND_SOC_DAIFMT_I2S;
	fsm_params->i2s_fmt = 3; // I2S
	fsm_params->boost_mode = 2; // ADP mode
	fsm_params->dac_gain = 3; // 16.5dB
	fsm_params->do_type = 2; // AEC
	fsm_params->do_enable = 0; // Disable
	fsm_params->sample_rate = 48000;
	fsm_params->bit_width = 16;
	fsm_params->channel = 2;
	fsm_params->freq_bclk = 1536000;

#ifdef CONFIG_OF
	ret = fsm_parse_dts(i2c, fsm_dev);
	if (ret)
		log_err(&i2c->dev, "parse DTS node fail:%d", ret);

	log_info(fsm_dev->dev, "chn name: %s, volume: 0x%X",
		fsm_dev->chn_name, fsm_dev->digi_vol);
	log_info(fsm_dev->dev, "monitor enable: %d, period: %d",
		fsm_dev->montr_en, fsm_dev->montr_pd);
#endif
#ifdef CONFIG_REGMAP
	fsm_dev->regmap = fsm_regmap_i2c_init(i2c);
	if (fsm_dev->regmap == NULL) {
		log_err(&i2c->dev, "regmap init failed");
		devm_kfree(&i2c->dev, fsm_dev);
		return -EINVAL;
	}
#endif
	fsm_enable_vddd(&i2c->dev, true);
	ret = fsm_detect_device(fsm_dev, i2c->addr);
	if (ret) {
		log_err(&i2c->dev, "%02X: detect device failed", i2c->addr);
		fsm_enable_vddd(&i2c->dev, false);
		devm_kfree(&i2c->dev, fsm_dev);
		return ret;
	}
	fsm_dev->id = fsm_dev_count;
	fsm_dev_count++;
	i2c_set_clientdata(i2c, fsm_dev);
	log_info(&i2c->dev, "fsm_dev->id:%d", fsm_dev->id);
	fsm_dev->fsm_wq = create_singlethread_workqueue("fs18xx");
	INIT_DELAYED_WORK(&fsm_dev->monitor_work, fsm_work_monitor);
	ret = fsm_codec_register(fsm_dev, fsm_dev->id);
	if (ret) {
		log_err(&i2c->dev, "register codec fail:%d", ret);
		return ret;
	}
	fsm_sysfs_init(fsm_dev);
	log_info(&i2c->dev, "done");

	return 0;
}

static int fsm_i2c_remove(struct i2c_client *i2c)
{
	struct fsm_dev *fsm_dev = i2c_get_clientdata(i2c);

	if (fsm_dev == NULL) {
		pr_err("fsm_dev is NULL");
		return -EINVAL;
	}
	fsm_codec_unregister(fsm_dev);
	fsm_sysfs_deinit(fsm_dev);

	if (fsm_dev->fsm_wq) {
		cancel_delayed_work_sync(&fsm_dev->monitor_work);
		destroy_workqueue(fsm_dev->fsm_wq);
	}
	fsm_enable_vddd(&i2c->dev, false);
	devm_kfree(&i2c->dev, fsm_dev);
	log_info(&i2c->dev, "done");

	return 0;
}

static void fsm_i2c_shutdown(struct i2c_client *i2c)
{
	log_debug(&i2c->dev, "enter");
}

static const struct i2c_device_id fsm_i2c_id[] = {
	{ FS18XX_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fsm_i2c_id);

static struct i2c_driver fsm_i2c_driver = {
	.driver = {
		.name  = FS18XX_DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(fsm_match_tbl),
#endif
	},
	.probe    = fsm_i2c_probe,
	.remove   = fsm_i2c_remove,
	.shutdown = fsm_i2c_shutdown,
	.id_table = fsm_i2c_id,
};

#ifdef CONFIG_MEDIATEK_SOLUTION
/* For mtk speaker amp driver */
int exfsm_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	return fsm_i2c_probe(i2c, id);
}
EXPORT_SYMBOL(exfsm_i2c_probe);

int exfsm_i2c_remove(struct i2c_client *i2c)
{
	return fsm_i2c_remove(i2c);
}
EXPORT_SYMBOL(exfsm_i2c_remove);

void exfsm_i2c_shutdown(struct i2c_client *i2c)
{
	fsm_i2c_shutdown(i2c);
}
EXPORT_SYMBOL(exfsm_i2c_shutdown);
#endif

static int __init fsm_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&fsm_i2c_driver);
	if (ret) {
		pr_err("init fail: %d", ret);
		return ret;
	}

	return 0;
}

static void __exit fsm_i2c_exit(void)
{
	i2c_del_driver(&fsm_i2c_driver);
}

//module_i2c_driver(fsm_i2c_driver);
module_init(fsm_i2c_init);
module_exit(fsm_i2c_exit);

MODULE_AUTHOR("FourSemi SW <support@foursemi.com>");
MODULE_DESCRIPTION("FourSemi Smart PA Driver");
MODULE_VERSION(FSM_CODE_VERSION);
MODULE_ALIAS("foursemi:" FS18XX_DRV_NAME);
MODULE_LICENSE("GPL");

