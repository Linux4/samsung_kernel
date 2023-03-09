/*
 * sound/soc/codec/s5m3700x.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#include "s5m3700x.h"
#include "s5m3700x-jack.h"
#include "s5m3700x-register.h"
#include "s5m3700x-regmap.h"

#define S5M3700X_SLAVE_ADDR		0x14

/* Codec Function */
int s5m3700x_codec_enable(struct device *dev);
int s5m3700x_codec_disable(struct device *dev);
static void s5m3700x_codec_power_enable(struct s5m3700x_priv *s5m3700x, bool enable);
static void s5m3700x_register_initialize(struct s5m3700x_priv *s5m3700x);
void s5m3700x_adc_digital_mute(struct s5m3700x_priv *s5m3700x, unsigned int channel, bool on);
static void s5m3700x_dac_soft_mute(struct s5m3700x_priv *s5m3700x, unsigned int channel, bool on);
static void s5m3700x_adc_mute_work(struct work_struct *work);

/* DebugFS for register read / write */
#ifdef CONFIG_DEBUG_FS
static void s5m3700x_debug_init(struct s5m3700x_priv *s5m3700x);
static void s5m3700x_debug_remove(struct s5m3700x_priv *s5m3700x);
#endif

void s5m3700x_usleep(unsigned int u_sec)
{
	usleep_range(u_sec, u_sec + 10);
}
EXPORT_SYMBOL_GPL(s5m3700x_usleep);

bool is_cache_bypass_enabled(struct s5m3700x_priv *s5m3700x)
{
	return (s5m3700x->cache_bypass > 0) ? true : false;
}

bool is_cache_only_enabled(struct s5m3700x_priv *s5m3700x)
{
	return (s5m3700x->cache_only > 0) ? true : false;
}

void s5m3700x_add_device_status(struct s5m3700x_priv *s5m3700x, unsigned int status)
{
	s5m3700x->status |= status;
}

void s5m3700x_remove_device_status(struct s5m3700x_priv *s5m3700x, unsigned int status)
{
	s5m3700x->status &= ~status;
}

bool s5m3700x_check_device_status(struct s5m3700x_priv *s5m3700x, unsigned int status)
{
	unsigned int compare_device = s5m3700x->status & status;

	if (compare_device)
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(s5m3700x_check_device_status);
/*
 * Return Value
 * True: If the register value cannot be cached, hence we have to read from the
 * hardware directly.
 * False: If the register value can be read from cache.
 */
static bool s5m3700x_volatile_register(struct device *dev, unsigned int reg)
{
	/*
	 * For all the registers for which we want to restore the value during
	 * regcache_sync operation, we need to return true here. For registers
	 * whose value need not be cached and restored should return false here.
	 *
	 * For the time being, let us cache the value of all registers other
	 * than the IRQ pending and IRQ status registers.
	 */
	switch (reg) {
	/* Digital Block */
	case 0x0001 ... 0x0006:
	case 0x0080 ... 0x0088:
	case 0x008F:
		return true;
	default:
		return false;
	}
}

/*
 * Return Value
 * True: If the register value can be read
 * False: If the register cannot be read
 */
static bool s5m3700x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	/* Digital Block */
	case 0x0001 ... 0x0006:
	case 0x0008 ... 0x000F:
	case 0x0010 ... 0x0029:
	case 0x002F ... 0x008F:
	case 0x0093 ... 0x009E:
	case 0x00A0 ... 0x00EE:
	case 0x00F0 ... 0x00FF:
		return true;
	/* Analog Block */
	case 0x0102 ... 0x010D:
	case 0x0110 ... 0x0123:
	case 0x0125:
	case 0x0130 ... 0x0132:
	case 0x0135:
	case 0x0138 ... 0x013B:
	case 0x013F ... 0x0140:
	case 0x0142 ... 0x0145:
	case 0x0149 ... 0x014A:
	case 0x014C ... 0x014D:
	case 0x0150 ... 0x0156:
	case 0x0180 ... 0x0195:
		return true;
	/* OTP Block */
	case 0x0200 ... 0x0277:
	case 0x0288 ... 0x028A:
	case 0x02A6 ... 0x02CB:
	case 0x02D0 ... 0x02F0:
		return true;
	/* MV Block */
	case 0x0300 ... 0x0301:
	case 0x0303 ... 0x0304:
	case 0x0307:
		return true;
	default:
		return false;
	}
}

/*
 * Return Value
 * True: If the register value can be write
 * False: If the register cannot be write
 */
static bool s5m3700x_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	/* Digital Block */
	case 0x08 ... 0x0D:
	case 0x10 ... 0x25:
	case 0x29:
	case 0x2F ... 0x56:
	case 0x58 ... 0x74:
	case 0x77 ... 0x7F:
	case 0x89 ... 0x8E:
	case 0x93 ... 0x9A:
	case 0x9D ... 0x9E:
	case 0xA0 ... 0xBE:
	case 0xC0 ... 0xCD:
	case 0xD0 ... 0xEE:
	case 0xFD ... 0xFF:
		return true;
	/* Analog Block */
	case 0x0102 ... 0x010D:
	case 0x0110 ... 0x0123:
	case 0x0125:
	case 0x0130 ... 0x0132:
	case 0x0135:
	case 0x0138 ... 0x013B:
	case 0x013F ... 0x0140:
	case 0x0142 ... 0x0145:
	case 0x0149 ... 0x014A:
	case 0x014C ... 0x014D:
	case 0x0150 ... 0x0156:
		return true;
	/* OTP Block */
	case 0x0200 ... 0x0277:
	case 0x0288 ... 0x028A:
	case 0x02A6 ... 0x02CB:
	case 0x02D0 ... 0x02F0:
		return true;
	/* MV Block */
	case 0x0300 ... 0x0301:
	case 0x0304:
	case 0x0307:
		return true;
	default:
		return false;
	}
}

/*
 * Put a register map into cache only mode, not cause any effect HW device.
 */
void s5m3700x_regcache_cache_only_switch(struct s5m3700x_priv *s5m3700x, bool on)
{
	mutex_lock(&s5m3700x->regcache_lock);
	if (on)
		s5m3700x->cache_only++;
	else
		s5m3700x->cache_only--;
	mutex_unlock(&s5m3700x->regcache_lock);

	dev_dbg(s5m3700x->dev, "%s count %d enable %d\n", __func__, s5m3700x->cache_only, on);

	if (s5m3700x->cache_only < 0) {
		s5m3700x->cache_only = 0;
		return;
	}

	if (on) {
		if (s5m3700x->cache_only == 1)
			regcache_cache_only(s5m3700x->regmap, true);
	} else {
		if (s5m3700x->cache_only == 0)
			regcache_cache_only(s5m3700x->regmap, false);
	}
}
EXPORT_SYMBOL_GPL(s5m3700x_regcache_cache_only_switch);

/*
 * Put a register map into cache bypass mode, not cause any effect Cache.
 * priority of cache_bypass is higher than cache_only
 */
void s5m3700x_regcache_cache_bypass_switch(struct s5m3700x_priv *s5m3700x, bool on)
{
	mutex_lock(&s5m3700x->regcache_lock);
	if (on)
		s5m3700x->cache_bypass++;
	else
		s5m3700x->cache_bypass--;
	mutex_unlock(&s5m3700x->regcache_lock);

	dev_dbg(s5m3700x->dev, "%s count %d enable %d\n", __func__, s5m3700x->cache_bypass, on);

	if (s5m3700x->cache_bypass < 0) {
		s5m3700x->cache_bypass = 0;
		return;
	}

	if (on) {
		if (s5m3700x->cache_bypass == 1)
			regcache_cache_bypass(s5m3700x->regmap, true);
	} else {
		if (s5m3700x->cache_bypass == 0)
			regcache_cache_bypass(s5m3700x->regmap, false);
	}
}
EXPORT_SYMBOL_GPL(s5m3700x_regcache_cache_bypass_switch);

int s5m3700x_read(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int *value)
{
	int ret = 0;

	ret = regmap_read(s5m3700x->regmap, addr, value);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_read);

int s5m3700x_write(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int value)
{
	int ret = 0;

	ret = regmap_write(s5m3700x->regmap, addr, value);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_write);

int s5m3700x_update_bits(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int mask, unsigned int value)
{
	int ret = 0;

	ret = regmap_update_bits(s5m3700x->regmap, addr, mask, value);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_update_bits);

int s5m3700x_read_only_cache(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int *value)
{
	int ret = 0;
	struct device *dev = s5m3700x->dev;

	if (!is_cache_bypass_enabled(s5m3700x)) {
		s5m3700x_regcache_cache_only_switch(s5m3700x, true);
		ret = regmap_read(s5m3700x->regmap, addr, value);
		s5m3700x_regcache_cache_only_switch(s5m3700x, false);
	} else {
		dev_err(dev, "%s: cannot read cache register.\n", __func__);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_read_only_cache);

int s5m3700x_write_only_cache(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int value)
{
	int ret = 0;
	struct device *dev = s5m3700x->dev;

	if (!is_cache_bypass_enabled(s5m3700x)) {
		s5m3700x_regcache_cache_only_switch(s5m3700x, true);
		ret = regmap_write(s5m3700x->regmap, addr, value);
		s5m3700x_regcache_cache_only_switch(s5m3700x, false);
	} else {
		dev_err(dev, "%s: cannot write cache register.\n", __func__);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_write_only_cache);

int s5m3700x_update_bits_only_cache(struct s5m3700x_priv *s5m3700x,
	 int addr, unsigned int mask, unsigned int value)
{
	int ret = 0;
	struct device *dev = s5m3700x->dev;

	if (!is_cache_bypass_enabled(s5m3700x)) {
		s5m3700x_regcache_cache_only_switch(s5m3700x, true);
		ret = regmap_update_bits(s5m3700x->regmap, addr, mask, value);
		s5m3700x_regcache_cache_only_switch(s5m3700x, false);
	} else {
		dev_err(dev, "%s: cannot update cache register.\n", __func__);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_update_bits_only_cache);

int s5m3700x_read_only_hardware(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int *value)
{
	int ret = 0;
	struct device *dev = s5m3700x->dev;

	if (!is_cache_only_enabled(s5m3700x)) {
		s5m3700x_regcache_cache_bypass_switch(s5m3700x, true);
		ret = regmap_read(s5m3700x->regmap, addr, value);
		s5m3700x_regcache_cache_bypass_switch(s5m3700x, false);
	} else {
		dev_err(dev, "%s: cannot read HW register.\n", __func__);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_read_only_hardware);

int s5m3700x_write_only_hardware(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int value)
{
	int ret = 0;
	struct device *dev = s5m3700x->dev;

	if (!is_cache_only_enabled(s5m3700x)) {
		s5m3700x_regcache_cache_bypass_switch(s5m3700x, true);
		ret = regmap_write(s5m3700x->regmap, addr, value);
		s5m3700x_regcache_cache_bypass_switch(s5m3700x, false);
	} else {
		dev_err(dev, "%s: cannot write HW register.\n", __func__);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_write_only_hardware);

int s5m3700x_update_bits_only_hardware(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int mask, unsigned int value)
{
	int ret = 0;
	struct device *dev = s5m3700x->dev;

	if (!is_cache_only_enabled(s5m3700x)) {
		s5m3700x_regcache_cache_bypass_switch(s5m3700x, true);
		ret = regmap_update_bits(s5m3700x->regmap, addr, mask, value);
		s5m3700x_regcache_cache_bypass_switch(s5m3700x, false);
	} else {
		dev_err(dev, "%s: cannot update HW register.\n", __func__);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_update_bits_only_hardware);

static void s5m3700x_print_dump(struct s5m3700x_priv *s5m3700x, int reg[], int register_block)
{
	u16 line, p_line;

	if(register_block == S5M3700X_DIGITAL_BLOCK) {
		dev_info(s5m3700x->dev, "========== Codec Digital Block(0x0) Dump ==========\n");
	} else if(register_block == S5M3700X_ANALOG_BLOCK) {
		dev_info(s5m3700x->dev, "========== Codec Analog Block(0x1) Dump ==========\n");
	} else if(register_block == S5M3700X_OTP_BLOCK) {
		dev_info(s5m3700x->dev, "========== Codec OTP Block(0x2) Dump ==========\n");
	} else if(register_block == S5M3700X_MV_BLOCK) {
		dev_info(s5m3700x->dev, "========== Codec MV Block(0x3) Dump ==========\n");
	}

	dev_info(s5m3700x->dev, "      00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");

	for (line = 0; line <= 0xf; line++) {
		p_line = line << 4;
		dev_info(s5m3700x->dev,
				"%04x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				p_line,	reg[p_line + 0x0], reg[p_line + 0x1], reg[p_line + 0x2],
				reg[p_line + 0x3], reg[p_line + 0x4], reg[p_line + 0x5],
				reg[p_line + 0x6], reg[p_line + 0x7], reg[p_line + 0x8],
				reg[p_line + 0x9], reg[p_line + 0xa], reg[p_line + 0xb],
				reg[p_line + 0xc], reg[p_line + 0xd], reg[p_line + 0xe],
				reg[p_line + 0xf]);
	}
}

static void s5m3700x_show_regmap_cache_registers(struct s5m3700x_priv *s5m3700x, int register_block)
{
	int reg[S5M3700X_BLOCK_SIZE] = {0,};
	int i = S5M3700X_REGISTER_START, end = (register_block + 1) * S5M3700X_BLOCK_SIZE;

	dev_info(s5m3700x->dev, "%s enter\n", __func__);

	/* read from Cache */
	for (i = register_block * S5M3700X_BLOCK_SIZE; i < end ; i++)
		s5m3700x_read_only_cache(s5m3700x, i, &reg[i % S5M3700X_BLOCK_SIZE]);

	s5m3700x_print_dump(s5m3700x, reg, register_block);

	dev_info(s5m3700x->dev, "%s done\n", __func__);
}

static void s5m3700x_show_regmap_hardware_registers(struct s5m3700x_priv *s5m3700x, int register_block)
{
	int reg[S5M3700X_BLOCK_SIZE] = {0,};
	int i = S5M3700X_REGISTER_START, end = (register_block + 1) * S5M3700X_BLOCK_SIZE;

	dev_info(s5m3700x->dev, "%s enter\n", __func__);

	/* read from HW */
	for (i = register_block * S5M3700X_BLOCK_SIZE; i < end ; i++)
		s5m3700x_read_only_hardware(s5m3700x, i, &reg[i % S5M3700X_BLOCK_SIZE]);

	s5m3700x_print_dump(s5m3700x, reg, register_block);

	dev_info(s5m3700x->dev, "%s done\n", __func__);
}

/*
 * Sync the register cache with the hardware
 * if cache register value is same as reg_defaults value, write is not occurred.
 */
int s5m3700x_regmap_register_sync(struct s5m3700x_priv *s5m3700x)
{
	int ret = 0;

	dev_dbg(s5m3700x->dev, "%s enter\n", __func__);

	mutex_lock(&s5m3700x->regsync_lock);

	if (!s5m3700x->cache_only) {
		regcache_mark_dirty(s5m3700x->regmap);
		ret = regcache_sync(s5m3700x->regmap);
		if (ret) {
			dev_err(s5m3700x->dev, "%s: failed to sync regmap : %d\n",
				__func__, ret);
		}
	} else {
		dev_err(s5m3700x->dev, "%s: regcache_cache_only is already occupied.(%d)\n",
			__func__, s5m3700x->cache_only);
		ret = -1;
	}
	mutex_unlock(&s5m3700x->regsync_lock);

	dev_dbg(s5m3700x->dev, "%s exit\n", __func__);

	return ret;
}

/*
 * Sync the hardware with the cache
 * need to start and end address for register sync
 */
int s5m3700x_hardware_register_sync(struct s5m3700x_priv *s5m3700x, int start, int end)
{
	int reg = 0, i = S5M3700X_REGISTER_START;
	int ret = 0;

	dev_dbg(s5m3700x->dev, "%s enter\n", __func__);

	mutex_lock(&s5m3700x->regsync_lock);

	for (i = start; i <= end ; i++) {
		/* read from HW */
		s5m3700x_read_only_hardware(s5m3700x, i, &reg);
		
		/* write to Cache */
		s5m3700x_write_only_cache(s5m3700x, i, reg);
	}

	mutex_unlock(&s5m3700x->regsync_lock);

	/* call s5m3700x_regmap_register_sync for sync from Cache to HW */
	s5m3700x_regmap_register_sync(s5m3700x);

	dev_dbg(s5m3700x->dev, "%s exit\n", __func__);
	return ret;
}

/*
 * Sync Cache with reg_defaults when power is off
 */
int s5m3700x_cache_register_sync_default(struct s5m3700x_priv *s5m3700x)
{
	int ret = 0, i = 0, array_size = ARRAY_SIZE(s5m3700x_reg_defaults), value = 0;

	dev_dbg(s5m3700x->dev, "%s enter\n", __func__);

	mutex_lock(&s5m3700x->regsync_lock);

	for (i = 0 ; i < array_size ; i++) {
		ret = s5m3700x_read_only_cache(s5m3700x, s5m3700x_reg_defaults[i].reg, &value);
		if (!ret) {
			if (value != s5m3700x_reg_defaults[i].def)
				s5m3700x_write_only_cache(s5m3700x, s5m3700x_reg_defaults[i].reg, s5m3700x_reg_defaults[i].def);
		}
	}

	mutex_unlock(&s5m3700x->regsync_lock);

	dev_dbg(s5m3700x->dev, "%s exit\n", __func__);

	return ret;
}

const struct regmap_config s5m3700x_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_LITTLE,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,

	.name = "i2c,S5M3700X",
	.max_register = S5M3700X_REGISTER_END,
	.readable_reg = s5m3700x_readable_register,
	.writeable_reg = s5m3700x_writeable_register,
	.volatile_reg = s5m3700x_volatile_register,
	.use_single_read = true,
	.use_single_write = true,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = s5m3700x_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(s5m3700x_reg_defaults),
};

/* initialize regmap again */
int s5m3700x_regmap_reinit_cache(struct s5m3700x_priv *s5m3700x)
{
	return regmap_reinit_cache(s5m3700x->regmap, &s5m3700x_regmap);
}

/*
 * patches are only updated HW registers, not cache.
 * Update reg_defaults value so that update hw and cache both
 * when regcache_sync is called.
 */
void s5m3700x_update_reg_defaults(const struct reg_sequence *patch, const int array_size)
{
	int i = 0, j = 0;

	for (i = 0; i < array_size; i++) {
		for (j = 0; j < ARRAY_SIZE(s5m3700x_reg_defaults); j++) {
			if (s5m3700x_reg_defaults[j].reg == patch[i].reg) {
				s5m3700x_reg_defaults[j].def = patch[i].def;
				break;
			}
		}
	}
}

/*
 * Digital Audio Interface - struct snd_kcontrol_new
 */

/*
 * s5m3700x_i2s_df_format - I2S Data Format Selection
 *
 * Map as per data-sheet:
 * 000 : I2S, PCM Long format
 * 001 : Left Justified format
 * 010 : Right Justified format
 * 011 : Not used
 * 100 : I2S, PCM Short format
 * 101 ~ 111 : Not used
 * I2S_DF : reg(0x20), shift(4), width(3)
 */
static const char * const s5m3700x_i2s_df_format[] = {
	"PCM-L", "LJ", "RJ", "Zero0", "PCM-S", "Zero1", "Zero2", "Zero3"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_i2s_df_enum, S5M3700X_020_IF_FORM1,
		I2S_DF_SHIFT, s5m3700x_i2s_df_format);

/*
 * s5m3700x_i2s_bck_format - I2S BCLK Data Selection
 *
 * Map as per data-sheet:
 * 0 : Normal
 * 1 : Invert
 * I2S_DF : reg(0x20), shift(1)
 */

static const char * const s5m3700x_i2s_bck_format[] = {
	"Normal", "Invert"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_i2s_bck_enum, S5M3700X_020_IF_FORM1,
		BCK_POL_SHIFT, s5m3700x_i2s_bck_format);

/*
 * s5m3700x_i2s_lrck_format - I2S LRCLK Data Selection
 *
 * Map as per data-sheet:
 * 0 : 0&2 Low, 1&3 High
 * 1 : 1&3 Low, 0&2 High
 * I2S_DF : reg(0x20), shift(0)
 */

static const char * const s5m3700x_i2s_lrck_format[] = {
	"Normal", "Invert"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_i2s_lrck_enum, S5M3700X_020_IF_FORM1,
		LRCK_POL_SHIFT, s5m3700x_i2s_lrck_format);


/*
 * s5m3700x_dvol_adc_tlv - Digital volume for ADC
 *
 * Map as per data-sheet:
 * 0x00 ~ 0xE0 : +42dB to -70dB, step 0.5dB
 * 0xE0 ~ 0xE5 : -70dB to -80dB, step 2.0dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_ADCL : reg(0x34), shift(0), width(8), invert(1), max(0xE5)
 * DVOL_ADCR : reg(0x35), shift(0), width(8), invert(1), max(0xE5)
 * DVOL_ADCC : reg(0x36), shift(0), width(8), invert(1), max(0xE5)
 */
static const unsigned int s5m3700x_dvol_adc_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x00, 0x05, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x06, 0xE5, TLV_DB_SCALE_ITEM(-6950, 50, 0),
};

static int mic1_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3700x_read(s5m3700x, S5M3700X_111_BST1, &mic_boost);
	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;

	return 0;
}

static int mic1_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_111_BST1, CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);

	return 0;
}

static int mic2_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3700x_read(s5m3700x, S5M3700X_112_BST2, &mic_boost);
	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;

	return 0;
}

static int mic2_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_112_BST2, CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);

	return 0;
}

static int mic3_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3700x_read(s5m3700x, S5M3700X_113_BST3, &mic_boost);
	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;

	return 0;
}

static int mic3_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3, CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);

	return 0;
}

static int mic4_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3700x_read(s5m3700x, S5M3700X_114_BST4, &mic_boost);
	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;

	return 0;
}

static int mic4_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_114_BST4, CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);

	return 0;
}

/*
 * s5m3700x_dmic_gain_tlv - Digital MIC gain
 *
 * Map as per data-sheet:
 * 000 ~ 111 : 1 to 15, step 2 code
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DMIC_GAIN_PRE : reg(0x32), shift(4), width(3), invert(0), max(7)
 */
static const unsigned int s5m3700x_dmic_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x7, TLV_DB_SCALE_ITEM(100, 200, 0),
};

/*
 * s5m3700x_dmic_st_tlv - DMIC IO strength selection
 *
 * Map as per data-sheet:
 * 0x0 ~ 0x3 : 0 to 3, step 1
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DMIC_ST : reg(0x2F), shift(2), width(2), invert(0), max(0x3)
 */
static const unsigned int s5m3700x_dmic_st_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x3, TLV_DB_SCALE_ITEM(0, 300, 0),
};

/*
 * s5m3700x_sdout_st_tlv - SDOUT IO strength selection
 *
 * Map as per data-sheet:
 * 0x0 ~ 0x3 : 0 to 3, step 1
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * SDOUT_ST : reg(0x2F), shift(0), width(2), invert(0), max(0x3)
 */
static const unsigned int s5m3700x_sdout_st_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x3, TLV_DB_SCALE_ITEM(0, 300, 0),
};

static int micbias1_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;
	bool micbias_enable;

	s5m3700x_read(s5m3700x, S5M3700X_0D0_DCTR_CM, &value);
	micbias_enable = value & APW_MCB1_MASK;

	ucontrol->value.integer.value[0] = micbias_enable;

	return 0;
}

static int micbias1_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_DMIC1)) {
		dev_info(codec->dev, "%s skip to control micbias1\n", __func__);
		return 0;
	}

	if (value)
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB1_MASK, APW_MCB1_MASK);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB1_MASK, 0);

	dev_info(codec->dev, "%s called, micbias1 turn %s\n", __func__, value ? "On" : "Off");
	return 0;
}

static int micbias2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;
	bool micbias_enable;

	s5m3700x_read(s5m3700x, S5M3700X_0D0_DCTR_CM, &value);
	micbias_enable = value & APW_MCB2_MASK;

	ucontrol->value.integer.value[0] = micbias_enable;

	return 0;
}

static int micbias2_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	mutex_lock(&s5m3700x->hp_mbias_lock);

	if (value) {
		s5m3700x->hp_bias_cnt++;
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB2_MASK, APW_MCB2_MASK);
	} else {
		s5m3700x->hp_bias_cnt--;
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB2_MASK, 0);
	}

	if (s5m3700x->hp_bias_cnt < 0)
		s5m3700x->hp_bias_cnt = 0;

	dev_info(codec->dev, "%s called, micbias2 turn %s\n", __func__, value ? "On" : "Off");

	mutex_unlock(&s5m3700x->hp_mbias_lock);

	return 0;
}

static int micbias3_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;
	bool micbias_enable;

	s5m3700x_read(s5m3700x, S5M3700X_0D0_DCTR_CM, &value);
	micbias_enable = value & APW_MCB3_MASK;

	ucontrol->value.integer.value[0] = micbias_enable;

	return 0;
}

static int micbias3_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_DMIC2)) {
		dev_info(codec->dev, "%s skip to control micbias3\n", __func__);
		return 0;
	}

	if (value)
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB3_MASK, APW_MCB3_MASK);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB3_MASK, 0);

	dev_info(codec->dev, "%s called, micbias3 turn %s\n", __func__, value ? "On" : "Off");
	return 0;
}

/*
 * s5m3700x_adc_dat_src - I2S channel input data selection
 *
 * Map as per data-sheet:
 * 00 : ADC Left Channel Data
 * 01 : ADC Right Channel Data
 * 10 : ADC Center Channel Data
 * 11 : Zero data
 *
 * SEL_ADC0 : reg(0x23), shift(0), width(2)
 * SEL_ADC1 : reg(0x23), shift(2), width(2)
 * SEL_ADC2 : reg(0x23), shift(4), width(2)
 * SEL_ADC3 : reg(0x23), shift(6), width(2)
 */
static const char * const s5m3700x_adc_dat_src[] = {
		"ADCL", "ADCR", "ADCC", "Zero"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_adc_dat_enum0, S5M3700X_023_IF_FORM4,
		SEL_ADC0_SHIFT, s5m3700x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3700x_adc_dat_enum1,	S5M3700X_023_IF_FORM4,
		SEL_ADC1_SHIFT, s5m3700x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3700x_adc_dat_enum2,	S5M3700X_023_IF_FORM4,
		SEL_ADC2_SHIFT, s5m3700x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3700x_adc_dat_enum3, S5M3700X_023_IF_FORM4,
		SEL_ADC3_SHIFT, s5m3700x_adc_dat_src);

static int dmic_delay_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->dmic_delay;
	return 0;
}

static int dmic_delay_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->dmic_delay = value;
	return 0;
}

static int amic_delay_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->amic_delay;
	return 0;
}

static int amic_delay_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->amic_delay = value;
	return 0;
}

static const char * const s5m3700x_hpf_sel_text[] = {
	"HPF-15Hz", "HPF-33Hz", "HPF-60Hz", "HPF-113Hz"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_sel_enum, S5M3700X_037_AD_HPF,
		HPF_SEL_SHIFT, s5m3700x_hpf_sel_text);

static const char * const s5m3700x_hpf_order_text[] = {
	"HPF-2nd", "HPF-1st"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_order_enum, S5M3700X_037_AD_HPF,
		HPF_ORDER_SHIFT, s5m3700x_hpf_order_text);

static const char * const s5m3700x_hpf_channel_text[] = {
	"Off", "On"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_channel_enum_l, S5M3700X_037_AD_HPF,
		HPF_ENL_SHIFT, s5m3700x_hpf_channel_text);

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_channel_enum_r, S5M3700X_037_AD_HPF,
		HPF_ENR_SHIFT, s5m3700x_hpf_channel_text);

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_channel_enum_c, S5M3700X_037_AD_HPF,
		HPF_ENC_SHIFT, s5m3700x_hpf_channel_text);

/*
 * DAC(Rx) path control - struct snd_kcontrol_new
 */
/*
 * s5m3700x_dvol_dac_tlv - Maximum headphone gain for EAR/RCV path
 *
 * Map as per data-sheet:
 * 0x00 ~ 0xE0 : +42dB to -70dB, step 0.5dB
 * 0xE1 ~ 0xE5 : -72dB to -80dB, step 2.0dB
 * 0xE6 : -82.4dB
 * 0xE7 ~ 0xE9 : -84.3dB to -96.3dB, step 6dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_DAL : reg(0x41), shift(0), width(8), invert(1), max(0xE9)
 * DVOL_DAR : reg(0x42), shift(0), width(8), invert(1), max(0xE9)
 */
static const unsigned int s5m3700x_dvol_dac_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x01, 0x03, TLV_DB_SCALE_ITEM(-9630, 600, 0),
	0x04, 0x04, TLV_DB_SCALE_ITEM(-8240, 0, 0),
	0x05, 0x09, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x0A, 0xE9, TLV_DB_SCALE_ITEM(-7000, 50, 0),
};

static int hp_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = 0;

	s5m3700x_read(s5m3700x, S5M3700X_05F_AVC16, &value);
	value = value & 0x1f;
	ucontrol->value.integer.value[0] = value;

	dev_info(s5m3700x->dev, "%s, hp_avc_gain: %d\n", __func__, value);

	return 0;
}

static int hp_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_write(s5m3700x, S5M3700X_07A_AVC43, 0x00);

	s5m3700x_update_bits(s5m3700x, S5M3700X_05F_AVC16, SIGN_MASK, 1 << SIGN_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_061_AVC18, SIGN_MASK, 1 << SIGN_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_063_AVC20, SIGN_MASK, 1 << SIGN_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_05F_AVC16, AVC_AVOL_MASK, value);
	s5m3700x_update_bits(s5m3700x, S5M3700X_061_AVC18, AVC_AVOL_MASK, value);
	s5m3700x_update_bits(s5m3700x, S5M3700X_063_AVC20, AVC_AVOL_MASK, value);
	return 0;
}

static int rcv_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = 0;

	s5m3700x_read(s5m3700x, S5M3700X_05F_AVC16, &value);
	value = value & 0x1f;
	ucontrol->value.integer.value[0] = value;

	dev_info(s5m3700x->dev, "%s, rcv_avc_gain: %d\n", __func__, value);

	return 0;
}

static int rcv_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_write(s5m3700x, S5M3700X_07A_AVC43, 0x00);

	s5m3700x_update_bits(s5m3700x, S5M3700X_05F_AVC16, SIGN_MASK, 1 << SIGN_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_061_AVC18, SIGN_MASK, 1 << SIGN_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_063_AVC20, SIGN_MASK, 1 << SIGN_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_05F_AVC16, AVC_AVOL_MASK, value);
	s5m3700x_update_bits(s5m3700x, S5M3700X_061_AVC18, AVC_AVOL_MASK, value);
	s5m3700x_update_bits(s5m3700x, S5M3700X_063_AVC20, AVC_AVOL_MASK, value);
	return 0;
}

/*
 * s5m3700x_dac_dat_src - I2S channel input data selection
 *
 * Map as per data-sheet:
 * 00 : DAC I2S Channel 0
 * 01 : DAC I2S Channel 1
 * 10 : DAC I2S Channel 2
 * 11 : DAC I2S Channel 3
 *
 * SEL_DAC0 : reg(0x24), shift(0), width(2)
 * SEL_DAC1 : reg(0x23), shift(4), width(2)
 */
static const char * const s5m3700x_dac_dat_src[] = {
		"CH0", "CH1", "CH2", "CH3"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_dac_dat_enum0, S5M3700X_024_IF_FORM5,
		SEL_DAC0_SHIFT, s5m3700x_dac_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3700x_dac_dat_enum1, S5M3700X_024_IF_FORM5,
		SEL_DAC1_SHIFT, s5m3700x_dac_dat_src);

/*
 * s5m3700x_dac_mixl_mode_text - DACL Mixer Selection
 *
 * Map as per data-sheet:
 * 000 : Data L
 * 001 : (L+R)/2 Mono
 * 010 : (L+R) Mono
 * 011 : (L+R)/2 Polarity Changed
 * 100 : (L+R) Polarity Changed
 * 101 : Zero Padding
 * 110 : Data L Polarity Changed
 * 111 : Data R Polarity Changed
 *
 * DAC_MIXL : reg(0x44), shift(4), width(3)
 */
static const char * const s5m3700x_dac_mixl_mode_text[] = {
		"Data L", "LR/2 Mono", "LR Mono", "LR/2 PolCh",
		"LR PolCh", "Zero", "Data L PolCh", "Data R PolCh"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_dac_mixl_mode_enum, S5M3700X_044_PLAY_MIX0,
		DAC_MIXL_SHIFT, s5m3700x_dac_mixl_mode_text);

/*
 * s5m3700x_dac_mixr_mode_text - DACR Mixer Selection
 *
 * Map as per data-sheet:
 * 000 : Data R
 * 001 : (L+R)/2 Mono
 * 010 : (L+R) Mono
 * 011 : (L+R)/2 Polarity Changed
 * 100 : (L+R) Polarity Changed
 * 101 : Zero Padding
 * 110 : Data R Polarity Changed
 * 111 : Data L Polarity Changed
 *
 * DAC_MIXR : reg(0x44), shift(0), width(3)
 */
static const char * const s5m3700x_dac_mixr_mode_text[] = {
		"Data R", "LR/2 Mono", "LR Mono", "LR/2 PolCh",
		"LR PolCh", "Zero", "Data R PolCh", "Data L PolCh"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_dac_mixr_mode_enum, S5M3700X_044_PLAY_MIX0,
		DAC_MIXR_SHIFT, s5m3700x_dac_mixr_mode_text);

/*
 * s5m3700x_lnrcv_mix_mode_text - LINE/RCV Mixer Selection
 *
 * Map as per data-sheet:
 * 00 : Data L
 * 01 : Data R
 * 10 : L/2 + R/2
 * 11 : Zero
 *
 * LINE/RCV MIX : reg(0x45), shift(3), width(2)
 */
static const char * const s5m3700x_lnrcv_mix_mode_text[] = {
		"Data L", "Data R", "LR/2", "Zero",
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_lnrcv_mix_mode_enum, S5M3700X_045_PLAY_MIX1,
		LN_RCV_MIX_SHIFT, s5m3700x_lnrcv_mix_mode_text);

/*
 * Codec control - struct snd_kcontrol_new
 */
static int codec_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int val = gpio_get_value_cansleep(s5m3700x->resetb_gpio);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int codec_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = codec->dev;
	int value = ucontrol->value.integer.value[0];

	dev_info(dev, "%s: codec enable : %s\n",
			__func__, (value) ? "On" : "Off");

	if (value == 1)
		s5m3700x_codec_enable(dev);
	else
		s5m3700x_codec_disable(dev);

	return 0;
}

static int codec_regdump_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int codec_regdump_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];
	int i = 0;

	if (value) {
		for ( i = 0; i < S5M3700X_REGISTER_BLOCKS; i++) {
			s5m3700x_show_regmap_cache_registers(s5m3700x,i);
			s5m3700x_show_regmap_hardware_registers(s5m3700x,i);
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_SND_SOC_S5M3700X_VTS)
void s5m3700x_vts_amic_off(struct s5m3700x_priv *s5m3700x)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	bool dac_on, adc_on, mic1_on, mic2_on, mic3_on, mic4_on;

	dac_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DAC_ON);
	adc_on = s5m3700x_check_device_status(s5m3700x, DEVICE_ADC_ON);
	mic1_on = s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC1);
	mic2_on = s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC2);
	mic3_on = s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC3);
	mic4_on = s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC4);

	dev_info(codec->dev, "%s called. VTS_AMIC_OFF (0x%x).\n", __func__, s5m3700x->status);

	/* VTS1 & VTS2 APW OFF */
	s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_VTS1_MASK | APW_VTS2_MASK, 0);

	/* VTS High-Z Setting */
	s5m3700x_update_bits(s5m3700x, S5M3700X_02F_DMIC_ST, VTS_DATA_ENB_MASK, VTS_DATA_ENB_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1, VTS_RESETB_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2, VTS_CLK_GATE_MASK, 0);

	if ((!dac_on) && (!adc_on))
		s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0, SEQ_CLK_GATE_MASK, 0);

	s5m3700x_update_bits(s5m3700x, S5M3700X_109_CTRL_VTS1,
			CTMI_VTS_INT1_MASK | CTMI_VTS_INT2_MASK,
			CTMI_VTS_2 << CTMI_VTS_INT1_SHIFT |
			CTMI_VTS_2 << CTMI_VTS_INT2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_10A_CTRL_VTS2,
			CTMI_VTS_INT3_MASK | CTMI_VTS_INT4_MASK,
			CTMI_VTS_2 << CTMI_VTS_INT3_SHIFT |
			CTMI_VTS_2 << CTMI_VTS_INT4_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_11E_CTRL_MIC_I1,
			CTMI_MIC_BST_MASK | CTMI_MIC_INT1_MASK,
			MIC_VAL_0 << CTMI_MIC_BST_SHIFT |
			MIC_VAL_2 << CTMI_MIC_INT1_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_10B_CTRL_VTS3,
			CTMF_VTS_LPF_CH1_MASK | CTMF_VTS_LPF_CH2_MASK,
			VTS_LPF_72K << CTMF_VTS_LPF_CH1_SHIFT |
			VTS_LPF_72K << CTMF_VTS_LPF_CH2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_111_BST1, EN_BST1_LPM_MASK | EN_BST2_LPM_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_111_BST1, CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3, CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2, EN_ISIBLKC_MASK, EN_ISIBLKC_MASK);

	if ((!mic1_on) && (!mic2_on)) {
		s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);
	}

	if ((!mic2_on) && (!mic3_on)) {
		s5m3700x_write(s5m3700x, S5M3700X_0BA_ODSEL11, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_122_BST4PN, 0x00);
	}
	s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD, SEL_BST3_MASK, 0);

	if (!adc_on)
		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD, SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK, 0);

	/* MCB3 OFF */
	if (!mic3_on) {
		/* MCB3 OFF */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM,	APW_MCB3_MASK, 0);

		/* Setting Mic3 Bias Voltage Default Value */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0C9_ACTR_MCB5,
				CTRV_MCB3_MASK, s5m3700x->p_jackdet->mic_bias3_voltage << CTRV_MCB3_SHIFT);
	}

	if ((!mic1_on) && (!mic2_on) && (!mic3_on))
		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD, SEL_BST2_MASK, 0);
	if (!mic4_on)
		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD, SEL_BST1_MASK, 0);

	s5m3700x_remove_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);
}

void s5m3700x_vts_amic1_on(struct s5m3700x_priv *s5m3700x)
{
	struct snd_soc_component *codec = s5m3700x->codec;

	dev_info(codec->dev, "%s called. VTS_AMIC1_ON.\n", __func__);
	s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
			SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK,
			SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK);

	s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2, VTS_CLK_GATE_MASK, VTS_CLK_GATE_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1, VTS_RESETB_MASK, VTS_RESETB_MASK);

	/* VTS Output Setting */
	s5m3700x_update_bits(s5m3700x, S5M3700X_02F_DMIC_ST, VTS_DATA_ENB_MASK, 0);

	s5m3700x_update_bits(s5m3700x, S5M3700X_111_BST1, EN_BST1_LPM_MASK, EN_BST1_LPM_MASK);

	s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
			CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
			CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
			CTMR_BSTR_0 << CTMR_BSTR2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3, CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2,
			CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
			CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
			CTMR_BSTR_0 << CTMR_BSTR2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2, EN_ISIBLKC_MASK, EN_ISIBLKC_MASK);

	s5m3700x_update_bits(s5m3700x, S5M3700X_109_CTRL_VTS1,
			CTMI_VTS_INT1_MASK | CTMI_VTS_INT2_MASK,
			CTMI_VTS_2 << CTMI_VTS_INT1_SHIFT |
			CTMI_VTS_0 << CTMI_VTS_INT2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_10A_CTRL_VTS2,
			CTMI_VTS_INT3_MASK | CTMI_VTS_INT4_MASK,
			CTMI_VTS_0 << CTMI_VTS_INT3_SHIFT |
			CTMI_VTS_0 << CTMI_VTS_INT4_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_11E_CTRL_MIC_I1,
			CTMI_MIC_BST_MASK | CTMI_MIC_INT1_MASK,
			MIC_VAL_0 << CTMI_MIC_BST_SHIFT |
			MIC_VAL_2 << CTMI_MIC_INT1_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_10B_CTRL_VTS3,
			CTMF_VTS_LPF_CH1_MASK | CTMF_VTS_LPF_CH2_MASK,
			VTS_LPF_12K << CTMF_VTS_LPF_CH1_SHIFT |
			VTS_LPF_12K << CTMF_VTS_LPF_CH2_SHIFT);

	/* Setting Mic3 Bias Voltage Default Value */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0C9_ACTR_MCB5,
			CTRV_MCB3_MASK, MICBIAS_VO_1_8V << CTRV_MCB3_SHIFT);

	/* MCB3 ON */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB3_MASK, APW_MCB3_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
			SEL_CLK_LCP_MASK | SEL_BST2_MASK |
			SEL_BST3_MASK | SEL_BST_IN_VCM_MASK,
			SEL_CLK_LCP_MASK | SEL_BST2_MASK |
			SEL_BST3_MASK | SEL_BST_IN_VCM_MASK);

	/* VTS1 APW ON */
	s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_VTS1_MASK, APW_VTS1_MASK);
	msleep(60);

	s5m3700x_add_device_status(s5m3700x, DEVICE_VTS_AMIC1);
}

void s5m3700x_vts_amic2_on(struct s5m3700x_priv *s5m3700x)
{
	struct snd_soc_component *codec = s5m3700x->codec;

	dev_info(codec->dev, "%s called. VTS_AMIC2_ON.\n", __func__);
	s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
			SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK,
			SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK);

	s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2, VTS_CLK_GATE_MASK, VTS_CLK_GATE_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1, VTS_RESETB_MASK, VTS_RESETB_MASK);

	/* VTS Output Setting */
	s5m3700x_update_bits(s5m3700x, S5M3700X_02F_DMIC_ST, VTS_DATA_ENB_MASK, 0);
	s5m3700x_update_bits(s5m3700x, S5M3700X_111_BST1, EN_BST2_LPM_MASK, EN_BST2_LPM_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_111_BST1, CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
			CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
			CTMR_BSTR_0 << CTMR_BSTR1_SHIFT |
			CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
			CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2,
			CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
			CTMR_BSTR_0 << CTMR_BSTR1_SHIFT |
			CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2, EN_ISIBLKC_MASK, EN_ISIBLKC_MASK);

	s5m3700x_update_bits(s5m3700x, S5M3700X_109_CTRL_VTS1,
			CTMI_VTS_INT1_MASK | CTMI_VTS_INT2_MASK,
			CTMI_VTS_2 << CTMI_VTS_INT1_SHIFT |
			CTMI_VTS_0 << CTMI_VTS_INT2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_10A_CTRL_VTS2,
			CTMI_VTS_INT3_MASK | CTMI_VTS_INT4_MASK,
			CTMI_VTS_0 << CTMI_VTS_INT3_SHIFT |
			CTMI_VTS_0 << CTMI_VTS_INT4_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_11E_CTRL_MIC_I1,
			CTMI_MIC_BST_MASK | CTMI_MIC_INT1_MASK,
			MIC_VAL_0 << CTMI_MIC_BST_SHIFT |
			MIC_VAL_2 << CTMI_MIC_INT1_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_10B_CTRL_VTS3,
			CTMF_VTS_LPF_CH1_MASK | CTMF_VTS_LPF_CH2_MASK,
			VTS_LPF_12K << CTMF_VTS_LPF_CH1_SHIFT |
			VTS_LPF_12K << CTMF_VTS_LPF_CH2_SHIFT);

	s5m3700x_write(s5m3700x, S5M3700X_0BA_ODSEL11, 0x02);
	s5m3700x_write(s5m3700x, S5M3700X_122_BST4PN, 0x80);

	/* Setting Mic3 Bias Voltage Default Value */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0C9_ACTR_MCB5,
			CTRV_MCB3_MASK, MICBIAS_VO_1_8V << CTRV_MCB3_SHIFT);
	/* MCB3 ON */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB3_MASK, APW_MCB3_MASK);

	s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
			SEL_CLK_LCP_MASK | SEL_BST2_MASK |
			SEL_BST3_MASK | SEL_BST_IN_VCM_MASK,
			SEL_CLK_LCP_MASK | SEL_BST2_MASK |
			SEL_BST3_MASK | SEL_BST_IN_VCM_MASK);

	/* VTS2 APW ON */
	s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_VTS2_MASK, APW_VTS2_MASK);
	msleep(60);
	s5m3700x_add_device_status(s5m3700x, DEVICE_VTS_AMIC2);
}

void s5m3700x_vts_amic_dual_on(struct s5m3700x_priv *s5m3700x)
{
	struct snd_soc_component *codec = s5m3700x->codec;

	dev_info(codec->dev, "%s called. VTS_AMIC_DUAL_ON.\n", __func__);
	s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
			SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK,
			SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK);

	s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2, VTS_CLK_GATE_MASK, VTS_CLK_GATE_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1, VTS_RESETB_MASK, VTS_RESETB_MASK);

	/* VTS Output Setting */
	s5m3700x_update_bits(s5m3700x, S5M3700X_02F_DMIC_ST, VTS_DATA_ENB_MASK, 0);

	s5m3700x_update_bits(s5m3700x, S5M3700X_111_BST1,
			EN_BST1_LPM_MASK | EN_BST2_LPM_MASK,
			EN_BST1_LPM_MASK | EN_BST2_LPM_MASK);

	s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
			CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
			CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
			CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3, CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2,
			CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
			CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
			CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2, EN_ISIBLKC_MASK, EN_ISIBLKC_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_109_CTRL_VTS1,
			CTMI_VTS_INT1_MASK | CTMI_VTS_INT2_MASK,
			CTMI_VTS_2 << CTMI_VTS_INT1_SHIFT |
			CTMI_VTS_0 << CTMI_VTS_INT2_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_10A_CTRL_VTS2,
			CTMI_VTS_INT3_MASK | CTMI_VTS_INT4_MASK,
			CTMI_VTS_0 << CTMI_VTS_INT3_SHIFT |
			CTMI_VTS_0 << CTMI_VTS_INT4_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_11E_CTRL_MIC_I1,
			CTMI_MIC_BST_MASK | CTMI_MIC_INT1_MASK,
			MIC_VAL_0 << CTMI_MIC_BST_SHIFT |
			MIC_VAL_2 << CTMI_MIC_INT1_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_10B_CTRL_VTS3,
			CTMF_VTS_LPF_CH1_MASK | CTMF_VTS_LPF_CH2_MASK,
			VTS_LPF_12K << CTMF_VTS_LPF_CH1_SHIFT |
			VTS_LPF_12K << CTMF_VTS_LPF_CH2_SHIFT);

	/* Setting Mic3 Bias Voltage Default Value */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0C9_ACTR_MCB5,
			CTRV_MCB3_MASK, MICBIAS_VO_1_8V << CTRV_MCB3_SHIFT);

	/* MCB3 ON */
	s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB3_MASK, APW_MCB3_MASK);
	s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
			SEL_CLK_LCP_MASK | SEL_BST2_MASK |
			SEL_BST3_MASK | SEL_BST_IN_VCM_MASK,
			SEL_CLK_LCP_MASK | SEL_BST2_MASK |
			SEL_BST3_MASK | SEL_BST_IN_VCM_MASK);

	/* VTS1 & VTS2 APW ON */
	s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD,
			APW_VTS1_MASK | APW_VTS2_MASK,
			APW_VTS1_MASK | APW_VTS2_MASK);
	msleep(60);
	s5m3700x_add_device_status(s5m3700x, DEVICE_VTS_AMIC1|DEVICE_VTS_AMIC2);
}

void s5m3700x_vts_amic(struct s5m3700x_priv *s5m3700x, unsigned int type)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	bool vts_amic_on;

	vts_amic_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);

	switch (type) {
	case VTS_AMIC_OFF:
		if (vts_amic_on)
			s5m3700x_vts_amic_off(s5m3700x);
		else
			dev_info(codec->dev, "%s called. State is already VTS_AMIC_OFF (%d).\n", __func__, type);
		break;
	case VTS_AMIC1_ON:
		if (vts_amic_on)
			dev_info(codec->dev, "%s called. prev state should be VTS_AMIC_OFF (%d).\n", __func__, type);
		else
			s5m3700x_vts_amic1_on(s5m3700x);
		break;
	case VTS_AMIC2_ON:
		if (vts_amic_on)
			dev_info(codec->dev, "%s called. prev state should be VTS_AMIC_OFF (%d).\n", __func__, type);
		else
			s5m3700x_vts_amic2_on(s5m3700x);
		break;
	case VTS_AMIC_DUAL_ON:
		if (vts_amic_on)
			dev_info(codec->dev, "%s called. prev state should be VTS_AMIC_OFF (%d).\n", __func__, type);
		else
			s5m3700x_vts_amic_dual_on(s5m3700x);
		break;
	}
}

void s5m3700x_vts_dmic1_on(struct s5m3700x_priv *s5m3700x)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	bool dmic1_on;

	dmic1_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DMIC1);

	dev_info(codec->dev, "%s called VTS_DMIC1_ON.\n", __func__);
	if (!dmic1_on)
		/* Enable MicBias1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB1_MASK, APW_MCB1_MASK);
	s5m3700x_add_device_status(s5m3700x, DEVICE_VTS_DMIC1);
}

void s5m3700x_vts_dmic2_on(struct s5m3700x_priv *s5m3700x)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	bool dmic2_on;

	dmic2_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DMIC2);

	dev_info(codec->dev, "%s called VTS_DMIC2_ON.\n", __func__);
	if (!dmic2_on)
		/* Enable MicBias3 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB3_MASK, APW_MCB3_MASK);
	s5m3700x_add_device_status(s5m3700x, DEVICE_VTS_DMIC2);
}

void s5m3700x_vts_dmic_off(struct s5m3700x_priv *s5m3700x)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	bool dmic1_on, dmic2_on, vts_dmic1_on, vts_dmic2_on;

	dmic1_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DMIC1);
	dmic2_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DMIC2);
	vts_dmic1_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_DMIC1);
	vts_dmic2_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_DMIC2);

	dev_info(codec->dev, "%s called. VTS_DMIC_OFF (0x%x).\n", __func__, s5m3700x->status);

	if (vts_dmic1_on && (!dmic1_on))
		/* Disable MicBias1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB1_MASK, 0);

	if (vts_dmic2_on && (!dmic2_on))
		/* Disable MicBias3 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_0D0_DCTR_CM, APW_MCB3_MASK, 0);
	s5m3700x_remove_device_status(s5m3700x, DEVICE_VTS_DMIC_ON);
}

void s5m3700x_vts_dmic(struct s5m3700x_priv *s5m3700x, unsigned int type)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	bool vts_dmic_on;

	vts_dmic_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_DMIC_ON);

	switch (type) {
	case VTS_DMIC1_ON:
		if (vts_dmic_on)
			dev_info(codec->dev, "%s called. prev state should be VTS_DMIC_OFF (%d).\n", __func__, type);
		else
			s5m3700x_vts_dmic1_on(s5m3700x);
		break;
	case VTS_DMIC2_ON:
		if (vts_dmic_on)
			dev_info(codec->dev, "%s called. prev state should be VTS_DMIC_OFF (%d).\n", __func__, type);
		else
			s5m3700x_vts_dmic2_on(s5m3700x);
		break;
	case VTS_DMIC_OFF:
		if (vts_dmic_on)
			s5m3700x_vts_dmic_off(s5m3700x);
		else
			dev_info(codec->dev, "%s called. State is already VTS_DMIC_OFF (%d).\n", __func__, type);
		break;
	}
}

static int s5m3700x_vts_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x->vts_mic == VTS_UNDEFINED) {
		ucontrol->value.integer.value[0] = UNDEFINED;
	} else if (s5m3700x->vts_mic == VTS_AMIC) {
		if (s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC1|DEVICE_VTS_AMIC2))
			ucontrol->value.integer.value[0] = VTS_AMIC_DUAL_ON;
		else if (s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC1))
			ucontrol->value.integer.value[0] = VTS_AMIC1_ON;
		else if (s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC2))
			ucontrol->value.integer.value[0] = VTS_AMIC2_ON;
		else
			ucontrol->value.integer.value[0] = VTS_AMIC_OFF;
	} else if (s5m3700x->vts_mic == VTS_DMIC) {
		if (s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_DMIC1))
			ucontrol->value.integer.value[0] = VTS_DMIC1_ON;
		else if (s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_DMIC2))
			ucontrol->value.integer.value[0] = VTS_DMIC2_ON;
		else
			ucontrol->value.integer.value[0] = VTS_DMIC_OFF;
	}

	return 0;
}

/*
 * VTS DMIC Control is depended on HW schemetic.
 */
static int s5m3700x_vts_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value == UNDEFINED) {
		dev_info(codec->dev, "%s: force reset vts_mic to Undefined status\n", __func__);
		s5m3700x_vts_amic(s5m3700x, VTS_AMIC_OFF);
		s5m3700x_vts_dmic(s5m3700x, VTS_DMIC_OFF);
		s5m3700x->vts_mic = VTS_UNDEFINED;
		return 0;
	}

	if (s5m3700x->vts_mic == VTS_UNDEFINED) {
		if ((value >= VTS_AMIC_OFF) && (value <= VTS_AMIC_DUAL_ON))
			s5m3700x->vts_mic = VTS_AMIC;
		else if ((value >= VTS_DMIC_OFF) && (value <= VTS_DMIC2_ON))
			s5m3700x->vts_mic = VTS_DMIC;
	}

	dev_info(codec->dev, "%s: VTS %d, VTS MIC %d\n", __func__, value, s5m3700x->vts_mic);

	if (s5m3700x->vts_mic == VTS_AMIC) {
		if ((value >= VTS_AMIC_OFF) && (value <= VTS_AMIC_DUAL_ON))
			s5m3700x_vts_amic(s5m3700x, value);
		else
			dev_err(codec->dev, "%s: Unsupported value\n", __func__);
	} else if (s5m3700x->vts_mic == VTS_DMIC) {
		if ((value >= VTS_DMIC_OFF) && (value <= VTS_DMIC2_ON))
			s5m3700x_vts_dmic(s5m3700x, value);
		else
			dev_err(codec->dev, "%s: Unsupported value\n", __func__);
	}

	return 0;
}

static const char * const s5m3700x_vts_mode_text[] = {
	"UNDEFINED", "AMIC_OFF", "AMIC1_ON", "AMIC2_ON", "AMIC_DUAL",
	"DMIC_OFF", "DMIC1_ON", "DMIC2_ON",
};

static SOC_ENUM_SINGLE_EXT_DECL(s5m3700x_vts_mode, s5m3700x_vts_mode_text);

static const struct snd_kcontrol_new s5m3700x_vts_controls[] = {
	/*
	 * VTS Control
	 */
	SOC_ENUM_EXT("VTS Mode", s5m3700x_vts_mode,
			s5m3700x_vts_get, s5m3700x_vts_put),
};
#endif

/*
 * struct snd_kcontrol_new s5m3700x_snd_control
 *
 * Every distinct bit-fields within the CODEC SFR range may be considered
 * as a control elements. Such control elements are defined here.
 *
 * Depending on the access mode of these registers, different macros are
 * used to define these control elements.
 *
 * SOC_ENUM: 1-to-1 mapping between bit-field value and provided text
 * SOC_SINGLE: Single register, value is a number
 * SOC_SINGLE_TLV: Single register, value corresponds to a TLV scale
 * SOC_SINGLE_TLV_EXT: Above + custom get/set operation for this value
 * SOC_SINGLE_RANGE_TLV: Register value is an offset from minimum value
 * SOC_DOUBLE: Two bit-fields are updated in a single register
 * SOC_DOUBLE_R: Two bit-fields in 2 different registers are updated
 */

/*
 * All the data goes into s5m3700x_snd_controls.
 * All path inter-connections goes into s5m3700x_dapm_routes
 */
static const struct snd_kcontrol_new s5m3700x_snd_controls[] = {
	/*
	 * Digital Audio Interface
	 */
	SOC_ENUM("I2S DF", s5m3700x_i2s_df_enum),
	SOC_ENUM("I2S BCLK POL", s5m3700x_i2s_bck_enum),
	SOC_ENUM("I2S LRCK POL", s5m3700x_i2s_lrck_enum),

	/*
	 * ADC(Tx) Volume control
	 */
	SOC_SINGLE_TLV("ADC Left Gain", S5M3700X_034_AD_VOLL,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, s5m3700x_dvol_adc_tlv),
	SOC_SINGLE_TLV("ADC Right Gain", S5M3700X_035_AD_VOLR,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, s5m3700x_dvol_adc_tlv),
	SOC_SINGLE_TLV("ADC Center Gain", S5M3700X_036_AD_VOLC,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, s5m3700x_dvol_adc_tlv),

	SOC_SINGLE_EXT("MIC1 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic1_boost_get, mic1_boost_put),
	SOC_SINGLE_EXT("MIC2 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic2_boost_get, mic2_boost_put),
	SOC_SINGLE_EXT("MIC3 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic3_boost_get, mic3_boost_put),
	SOC_SINGLE_EXT("MIC4 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic4_boost_get, mic4_boost_put),

	SOC_SINGLE_TLV("DMIC Gain", S5M3700X_032_ADC3,
			DMIC_GAIN_PRE_SHIFT,
			DMIC_GAIN_15, 0, s5m3700x_dmic_gain_tlv),

	/*
	 * ADC(Tx) IO Strength control
	 */
	SOC_SINGLE_TLV("DMIC ST", S5M3700X_02F_DMIC_ST,
			DMIC_ST_SHIFT,
			DMIC_IO_16mA, 0, s5m3700x_dmic_st_tlv),

	SOC_SINGLE_TLV("SDOUT ST", S5M3700X_02F_DMIC_ST,
			SDOUT_ST_SHIFT,
			SDOUT_IO_16mA, 0, s5m3700x_sdout_st_tlv),

	/*
	 * ADC(Tx) Mic Bias control
	 */

	SOC_SINGLE_EXT("MIC BIAS1", SND_SOC_NOPM, 0, 1, 0,
			micbias1_get, micbias1_put),
	SOC_SINGLE_EXT("MIC BIAS2", SND_SOC_NOPM, 0, 1, 0,
			micbias2_get, micbias2_put),
	SOC_SINGLE_EXT("MIC BIAS3", SND_SOC_NOPM, 0, 1, 0,
			micbias3_get, micbias3_put),

	/*
	 * ADC(Tx) path control
	 */

	SOC_ENUM("ADC DAT Mux0", s5m3700x_adc_dat_enum0),
	SOC_ENUM("ADC DAT Mux1", s5m3700x_adc_dat_enum1),
	SOC_ENUM("ADC DAT Mux2", s5m3700x_adc_dat_enum2),
	SOC_ENUM("ADC DAT Mux3", s5m3700x_adc_dat_enum3),

	/*
	 * ADC(Tx) Mic Enable Delay
	 */

	SOC_SINGLE_EXT("DMIC delay", SND_SOC_NOPM, 0, 1000, 0,
			dmic_delay_get, dmic_delay_put),
	SOC_SINGLE_EXT("AMIC delay", SND_SOC_NOPM, 0, 1000, 0,
			amic_delay_get, amic_delay_put),

	/*
	 * ADC(Tx) HPF Tuning Control
	 */
	SOC_ENUM("HPF Tuning", s5m3700x_hpf_sel_enum),
	SOC_ENUM("HPF Order", s5m3700x_hpf_order_enum),
	SOC_ENUM("HPF Left", s5m3700x_hpf_channel_enum_l),
	SOC_ENUM("HPF Right", s5m3700x_hpf_channel_enum_r),
	SOC_ENUM("HPF Center", s5m3700x_hpf_channel_enum_c),

	/*
	 * DAC(Rx) volume control
	 */
	SOC_DOUBLE_R_TLV("DAC Gain", S5M3700X_041_PLAY_VOLL, S5M3700X_042_PLAY_VOLR,
			DVOL_DA_SHIFT,
			DAC_DVOL_MINNUM, 1, s5m3700x_dvol_dac_tlv),
	SOC_SINGLE_EXT("HP AVC Gain", SND_SOC_NOPM, 0, 24, 0,
			hp_gain_get, hp_gain_put),
	SOC_SINGLE_EXT("RCV AVC Gain", SND_SOC_NOPM, 0, 20, 0,
			rcv_gain_get, rcv_gain_put),
	/*
	 * DAC(Rx) path control
	 */
	SOC_ENUM("DAC DAT Mux0", s5m3700x_dac_dat_enum0),
	SOC_ENUM("DAC DAT Mux1", s5m3700x_dac_dat_enum1),
	SOC_ENUM("DAC MIXL Mode", s5m3700x_dac_mixl_mode_enum),
	SOC_ENUM("DAC MIXR Mode", s5m3700x_dac_mixr_mode_enum),
	SOC_ENUM("LNRCV MIX Mode", s5m3700x_lnrcv_mix_mode_enum),

	/*
	 * Codec control
	 */
	SOC_SINGLE_EXT("Codec Enable", SND_SOC_NOPM, 0, 1, 0,
			codec_enable_get, codec_enable_put),
	SOC_SINGLE_EXT("Codec Regdump", SND_SOC_NOPM, 0, 1, 0,
			codec_regdump_get, codec_regdump_put),
};

/*
 * snd_soc_dapm_widget controls set
 */
static const char * const s5m3700x_device_enable_enum_src[] = {
	"Off", "On",
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_device_enable_enum, SND_SOC_NOPM, 0,
								s5m3700x_device_enable_enum_src);
/* Tx Devices */
/*Common Configuration for Tx*/
int check_adc_mode(struct s5m3700x_priv *s5m3700x)
{
	unsigned int mode = 0;
	unsigned int val1 = 0, val2 = 0;
	unsigned int adc_l_status, adc_r_status, adc_c_status;

	s5m3700x_read(s5m3700x, S5M3700X_031_ADC2, &val1);
	s5m3700x_read(s5m3700x, S5M3700X_032_ADC3, &val2);

	adc_l_status = (val1 & INP_SEL_L_MASK) >> INP_SEL_L_SHIFT;
	adc_r_status = (val1 & INP_SEL_R_MASK) >> INP_SEL_R_SHIFT;
	adc_c_status = (val2 & INP_SEL_C_MASK) >> INP_SEL_C_SHIFT;

	if (adc_l_status != ADC_INP_SEL_ZERO)
		mode++;
	if (adc_r_status != ADC_INP_SEL_ZERO)
		mode++;
	if (adc_c_status != ADC_INP_SEL_ZERO)
		mode++;

	return mode;
}

static int vmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool dac_on, vts_on;
	int adc_status = 0;

	dac_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DAC_ON);
	vts_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);

	adc_status = check_adc_mode(s5m3700x);
	dev_info(codec->dev, "%s called, event = %d, adc_status: %d\n",
			__func__, event, adc_status);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Clock Gate */
		s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
				SEQ_CLK_GATE_MASK | ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				SEQ_CLK_GATE_MASK | ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		switch (adc_status) {
		case AMIC_MONO:
			/* Setting Mono Mode */
			s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2, ADC_CIC_CGL_MASK, ADC_CIC_CGL_MASK);

			s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1,
					ADC_FSEL_MASK, ADC_MODE_MONO << ADC_FSEL_SHIFT);

			s5m3700x_update_bits(s5m3700x, S5M3700X_030_ADC1,
					CH_MODE_MASK, ADC_CH_MONO << CH_MODE_SHIFT);
			break;
		case AMIC_STEREO:
			/* Setting Stereo Mode */
			s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2,
					ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
					ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK);

			s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1,
					ADC_FSEL_MASK, ADC_MODE_STEREO << ADC_FSEL_SHIFT);

			s5m3700x_update_bits(s5m3700x, S5M3700X_030_ADC1,
					CH_MODE_MASK, ADC_CH_STEREO << CH_MODE_SHIFT);
			break;
		case AMIC_TRIPLE:
			/* Setting Triple Mode */
			s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2,
					ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK | ADC_CIC_CGC_MASK,
					ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK | ADC_CIC_CGC_MASK);

			s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1,
					ADC_FSEL_MASK, ADC_MODE_TRIPLE << ADC_FSEL_SHIFT);

			s5m3700x_update_bits(s5m3700x, S5M3700X_030_ADC1,
					CH_MODE_MASK, ADC_CH_TRIPLE << CH_MODE_SHIFT);
			break;
		default:
			break;
		}
		/* DMIC OSR Clear */
		s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4, DMIC_OSR_MASK, 0);

		switch (s5m3700x->capture_params.aifrate) {
		case S5M3700X_SAMPLE_RATE_48KHZ:
			s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4,
					CP_TYPE_SEL_MASK, FILTER_TYPE_NORMAL_AMIC << CP_TYPE_SEL_SHIFT);
			break;
		case S5M3700X_SAMPLE_RATE_192KHZ:
		case S5M3700X_SAMPLE_RATE_384KHZ:
			s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4,
					CP_TYPE_SEL_MASK, FILTER_TYPE_UHQA_W_LP_AMIC << CP_TYPE_SEL_SHIFT);
			break;
		}

		/* DSML offset Setting */
		s5m3700x_write(s5m3700x, S5M3700X_11A_DSMC3, 0x00);

		/* DWA schema change */
		s5m3700x_write(s5m3700x, S5M3700X_116_DSMC_1, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);
		if (!dac_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,	AVC_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,	RSTB_ADC_MASK | ADC_RESETB_MASK, 0);
		if (!dac_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0, AVC_RESETB_MASK | CORE_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* DSML offset Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_11A_DSMC3, 0xE0);

		/* DWA schema Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_116_DSMC_1, 0x80);

		s5m3700x_write(s5m3700x, S5M3700X_033_ADC4, 0x00);
		s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2,
				ADC_CIC_CGC_MASK | ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
				ADC_CLK_GATE_MASK, 0);

		if ((!dac_on) && (!vts_on)) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0, SEQ_CLK_GATE_MASK, 0);
			s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0, COM_CLK_GATE_MASK, 0);
		}
		break;
	}
	return 0;
}

static int dvmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool dac_on, dmic2_on, vts_on;

	dac_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DAC_ON);
	vts_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);
	dmic2_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DMIC2);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
				ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		if (!dmic2_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
					DMIC_CLK1_GATE_MASK, DMIC_CLK1_GATE_MASK);
		else
			s5m3700x_update_bits(s5m3700x, S5M3700X_011_CLKGATE1,
					DMIC_CLK2_GATE_MASK, DMIC_CLK2_GATE_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* DMIC CLK ZTIE 0 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_032_ADC3,
				DMIC_CLK_ZTIE_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_NORMAL_DMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_64 << DMIC_OSR_SHIFT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK, 0);

		/* DMIC CLK ZTIE 1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_032_ADC3,
				DMIC_CLK_ZTIE_MASK, DMIC_CLK_ZTIE_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_011_CLKGATE1,
				DMIC_CLK2_GATE_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
				ADC_CLK_GATE_MASK | DMIC_CLK1_GATE_MASK, 0);

		if ((!dac_on) && (!vts_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
					COM_CLK_GATE_MASK, 0);
		break;
	}
	return 0;
}

static int adc_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool dmic_on, amic_on;

	dmic_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DMIC_ON);
	amic_on = s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC_ON);

	dev_info(codec->dev, "%s called, event = %d, status: 0x%x\n",
			__func__, event, s5m3700x->status);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMU:
		cancel_delayed_work(&s5m3700x->adc_mute_work);
		if (amic_on)
			queue_delayed_work(s5m3700x->adc_mute_wq, &s5m3700x->adc_mute_work,
					msecs_to_jiffies(s5m3700x->amic_delay));
		else if (dmic_on)
			queue_delayed_work(s5m3700x->adc_mute_wq, &s5m3700x->adc_mute_work,
					msecs_to_jiffies(s5m3700x->dmic_delay));
		else
			queue_delayed_work(s5m3700x->adc_mute_wq, &s5m3700x->adc_mute_work,
					msecs_to_jiffies(220));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

/* INP SEL */
static const char * const s5m3700x_inp_sel_src_l[] = {
	"AMIC_L ADC_L", "AMIC_R ADC_L", "AMIC_C ADC_L", "Zero ADC_L",
	"DMIC1L ADC_L", "DMIC1R ADC_L", "DMIC2L ADC_L", "DMIC2R ADC_L"
};
static const char * const s5m3700x_inp_sel_src_r[] = {
	"AMIC_R ADC_R", "AMIC_L ADC_R", "AMIC_C ADC_R", "Zero ADC_R",
	"DMIC1L ADC_R", "DMIC1R ADC_R", "DMIC2L ADC_R", "DMIC2R ADC_R"
};
static const char * const s5m3700x_inp_sel_src_c[] = {
	"AMIC_C ADC_C", "AMIC_L ADC_C", "AMIC_R ADC_C", "Zero ADC_C",
	"DMIC1L ADC_C", "DMIC1R ADC_C", "DMIC2L ADC_C", "DMIC2R ADC_C"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_inp_sel_enum_l, S5M3700X_031_ADC2,
		INP_SEL_L_SHIFT, s5m3700x_inp_sel_src_l);
static SOC_ENUM_SINGLE_DECL(s5m3700x_inp_sel_enum_r, S5M3700X_031_ADC2,
		INP_SEL_R_SHIFT, s5m3700x_inp_sel_src_r);
static SOC_ENUM_SINGLE_DECL(s5m3700x_inp_sel_enum_c, S5M3700X_032_ADC3,
		INP_SEL_C_SHIFT, s5m3700x_inp_sel_src_c);

static const struct snd_kcontrol_new s5m3700x_inp_sel_l =
		SOC_DAPM_ENUM("INP_SEL_L", s5m3700x_inp_sel_enum_l);
static const struct snd_kcontrol_new s5m3700x_inp_sel_r =
		SOC_DAPM_ENUM("INP_SEL_R", s5m3700x_inp_sel_enum_r);
static const struct snd_kcontrol_new s5m3700x_inp_sel_c =
		SOC_DAPM_ENUM("INP_SEL_C", s5m3700x_inp_sel_enum_c);

/*Specific Configuration for Tx*/
static int mic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool vts_on;

	vts_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* DSML Blocking & Chopping On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_115_DSM1,
				EN_ISIBLKL_MASK | EN_CHOPL_MASK | CAL_SKIP_MASK,
				EN_ISIBLKL_MASK | EN_CHOPL_MASK | CAL_SKIP_MASK);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC1 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_MIC1L_MASK, APW_MIC1L_MASK);
		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD, SEL_BST2_MASK, SEL_BST2_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* MIC1 APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_MIC1L_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!vts_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK |
					SEL_BST1_MASK | SEL_BST2_MASK, 0);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* DSML Blocking & Chopping Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_115_DSM1, EN_CHOPL_MASK | CAL_SKIP_MASK, 0);
		break;
	}
	return 0;
}

static int s5m3700x_dapm_mic1_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC1))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_mic1_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_AMIC1);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_AMIC1);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : AMIC1 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}


static const struct snd_kcontrol_new mic1_on[] = {
	SOC_DAPM_ENUM_EXT("MIC1 On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_mic1_on_get, s5m3700x_dapm_mic1_on_put),
};

static int mic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool vts_on;

	vts_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Boost1 LCP On */
		s5m3700x_write(s5m3700x, S5M3700X_0BA_ODSEL11, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_122_BST4PN, 0x80);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* DSML Blocking & Chopping On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_115_DSM1,
				EN_CHOPC_MASK | CAL_SKIP_MASK,
				EN_CHOPC_MASK | CAL_SKIP_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC2 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_MIC2C_MASK, APW_MIC2C_MASK);
		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD, SEL_BST2_MASK, SEL_BST2_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* MIC2 APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_MIC2C_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!vts_on) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK | SEL_BST2_MASK, 0);

			/* Boost1 LCP Off */
			s5m3700x_write(s5m3700x, S5M3700X_0BA_ODSEL11, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_122_BST4PN, 0x00);
		}
		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* DSML Blocking & Chopping Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_115_DSM1, EN_CHOPC_MASK | CAL_SKIP_MASK, 0);
		break;
	}
	return 0;
}

static int s5m3700x_dapm_mic2_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC2))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_mic2_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_AMIC2);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_AMIC2);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : AMIC2 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}

static const struct snd_kcontrol_new mic2_on[] = {
	SOC_DAPM_ENUM_EXT("MIC2 On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_mic2_on_get, s5m3700x_dapm_mic2_on_put),
};

static int mic3_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool vts_on;

	vts_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* MIC3/4 Selection */
		s5m3700x_write(s5m3700x, S5M3700X_11D_BST_OP, 0x00);

		/* Boost1 LCP On */
		s5m3700x_write(s5m3700x, S5M3700X_0BA_ODSEL11, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_122_BST4PN, 0x80);

		/* DSML Blocking & Chopping On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_115_DSM1,
				EN_CHOPR_MASK | CAL_SKIP_MASK,
				EN_CHOPR_MASK | CAL_SKIP_MASK);

		/* DSMR ISI Blocking */
		s5m3700x_write(s5m3700x, S5M3700X_119_DSM3, 0x10);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_118_DSMC_2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_114_BST4,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC3 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_MIC3R_MASK, APW_MIC3R_MASK);
		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD, SEL_BST2_MASK, SEL_BST2_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_MIC3R_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!vts_on) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK |
					SEL_BST3_MASK | SEL_BST2_MASK, 0);

			/* Boost1 LCP Off */
			s5m3700x_write(s5m3700x, S5M3700X_0BA_ODSEL11, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_122_BST4PN, 0x00);
		}

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_118_DSMC_2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_114_BST4,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* DSMR ISI Blocking */
		s5m3700x_write(s5m3700x, S5M3700X_119_DSM3, 0x10);

		/* DSML Blocking & Chopping Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_115_DSM1,
				EN_CHOPR_MASK | CAL_SKIP_MASK, 0);

		/* MIC3/4 Selection */
		s5m3700x_write(s5m3700x, S5M3700X_11D_BST_OP, 0x00);
		break;
	}
	return 0;
}

static int s5m3700x_dapm_mic3_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC3))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_mic3_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_AMIC3);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_AMIC3);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : AMIC3 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}

static const struct snd_kcontrol_new mic3_on[] = {
	SOC_DAPM_ENUM_EXT("MIC3 On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_mic3_on_get, s5m3700x_dapm_mic3_on_put),
};

static int mic4_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool vts_on;

	vts_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* MIC3/4 Selection */
		s5m3700x_write(s5m3700x, S5M3700X_11D_BST_OP, 0x04);

		/* DSML Blocking & Chopping On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_115_DSM1,
				EN_CHOPR_MASK | CAL_SKIP_MASK,
				EN_CHOPR_MASK | CAL_SKIP_MASK);

		/* DSMR ISI Blocking */
		s5m3700x_write(s5m3700x, S5M3700X_119_DSM3, 0x10);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_118_DSMC_2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_114_BST4,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC4 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD,
				APW_MIC4R_MASK, APW_MIC4R_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
				SEL_BST1_MASK, SEL_BST1_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* MIC4 APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_018_PWAUTO_AD, APW_MIC4R_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!vts_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK |
					SEL_BST4_MASK | SEL_BST1_MASK, 0);

		/* DSML Blocking & Chopping Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_115_DSM1,
				EN_CHOPR_MASK | CAL_SKIP_MASK, 0);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_118_DSMC_2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_114_BST4,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* DSMR ISI Blocking */
		s5m3700x_write(s5m3700x, S5M3700X_119_DSM3, 0x10);

		/* MIC3/4 Selection */
		s5m3700x_write(s5m3700x, S5M3700X_11D_BST_OP, 0x00);
		break;
	}
	return 0;
}

static int s5m3700x_dapm_mic4_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC4))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_mic4_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_AMIC4);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_AMIC4);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : AMIC4 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}


static const struct snd_kcontrol_new mic4_on[] = {
	SOC_DAPM_ENUM_EXT("MIC4 On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_mic4_on_get, s5m3700x_dapm_mic4_on_put),
};

static int dmic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool dac_on;

	dac_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DAC_ON);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4, DMIC_EN1_MASK, DMIC_EN1_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		if (!dac_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0, AVC_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);

		if (!dac_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
					AVC_RESETB_MASK | CORE_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4, DMIC_EN1_MASK, 0);
		break;
	}
	return 0;
}

static int s5m3700x_dapm_dmic1_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_DMIC1))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_dmic1_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_DMIC1);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_DMIC1);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : DMIC1 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}


static const struct snd_kcontrol_new dmic1_on[] = {
	SOC_DAPM_ENUM_EXT("DMIC1 On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_dmic1_on_get, s5m3700x_dapm_dmic1_on_put),
};

static int dmic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool dac_on;

	dac_on = s5m3700x_check_device_status(s5m3700x, DEVICE_DAC_ON);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4, DMIC_EN2_MASK, DMIC_EN2_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		if (!dac_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0, AVC_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0, RSTB_ADC_MASK | ADC_RESETB_MASK, 0);

		if (!dac_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
					AVC_RESETB_MASK | CORE_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_033_ADC4, DMIC_EN2_MASK, 0);
		break;
	}
	return 0;
}

static int s5m3700x_dapm_dmic2_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_DMIC2))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_dmic2_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_DMIC2);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_DMIC2);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : DMIC2 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}


static const struct snd_kcontrol_new dmic2_on[] = {
	SOC_DAPM_ENUM_EXT("DMIC2 On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_dmic2_on_get, s5m3700x_dapm_dmic2_on_put),
};

/* Rx Devices */
/*Common Configuration for Rx*/
/*Control Clock & Reset, CP */
static int dac_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	bool hp_on, ep_on, mic_on, vts_on;

	hp_on = s5m3700x_check_device_status(s5m3700x, DEVICE_HP);
	ep_on = s5m3700x_check_device_status(s5m3700x, DEVICE_EP);
	mic_on = s5m3700x_check_device_status(s5m3700x, DEVICE_AMIC_ON | DEVICE_DMIC_ON);
	vts_on = s5m3700x_check_device_status(s5m3700x, DEVICE_VTS_AMIC_ON);

	dev_info(codec->dev, "%s called, event = %d, status: 0x%x\n",
			__func__, event, s5m3700x->status);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (hp_on) {
			/* CLKGATE 0 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
					OVP_CLK_GATE_MASK | SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK
					| DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
					OVP_CLK_GATE_MASK | SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK
					| DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

			/* CLKGATE 1 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_011_CLKGATE1,
					DAC_CIC_TEMP_MASK | DAC_CIC_CGLR_MASK,
					DAC_CIC_TEMP_MASK | DAC_CIC_CGLR_MASK);

			/* CLKGATE 2 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2,
					CED_CLK_GATE_MASK, CED_CLK_GATE_MASK);

			/* RESETB */
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
					AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK,
					AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK);
		} else {
			/* CLKGATE 0 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
					SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK
					| DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
					SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK
					| DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

			/* CLKGATE 1 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_011_CLKGATE1,
					DAC_CIC_TEMP_MASK | DAC_CIC_CGC_MASK,
					DAC_CIC_TEMP_MASK | DAC_CIC_CGC_MASK);

			/* CLKGATE 2 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2,
					CED_CLK_GATE_MASK, CED_CLK_GATE_MASK);

			/* RESETB */
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
					AVC_RESETB_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK,
					AVC_RESETB_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_write(s5m3700x, S5M3700X_104_CTRL_CP, 0x88);
		s5m3700x_write(s5m3700x, S5M3700X_104_CTRL_CP, 0xAA);
		s5m3700x_write(s5m3700x, S5M3700X_106_CTRL_CP2_2, 0xFF);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_write(s5m3700x, S5M3700X_106_CTRL_CP2_2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_104_CTRL_CP, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CLKGATE 2 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_012_CLKGATE2,
				CED_CLK_GATE_MASK, 0);

		/* CLKGATE 1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_011_CLKGATE1,
				DAC_CIC_TEMP_MASK | DAC_CIC_CGLR_MASK | DAC_CIC_CGC_MASK, 0);

		/* CLKGATE 0 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
				OVP_CLK_GATE_MASK, 0);

		if ((!mic_on) && (!vts_on)) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_010_CLKGATE0,
					SEQ_CLK_GATE_MASK, 0);
		}

		/* RESETB */
		s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
				RSTB_DAC_DSM_MASK, 0);

		if (!mic_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_014_RESETB0,
					CORE_RESETB_MASK, 0);

		s5m3700x_usleep(5000);
		break;
	}
	return 0;
}

/*Specific Configuration for Rx*/
/* EP Device */
static void ep_32ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1,
				RSTB_DAC_DSM_C_MASK, RSTB_DAC_DSM_C_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_04F_HSYS_CP2_SIGN, 0x82);

		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xBD);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xBD);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x22);

		/* Class-H */
		s5m3700x_write(s5m3700x, S5M3700X_2C4_CP2_TH2_32, 0x7C);
		s5m3700x_write(s5m3700x, S5M3700X_2C5_CP2_TH3_32, 0x80);

		/* IDAC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_288_IDAC3_OTP, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_289_IDAC4_OTP, 0x15);
		s5m3700x_write(s5m3700x, S5M3700X_28A_IDAC5_OTP, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_149_PD_EP, 0x80);

		/* EP Control Setting */
		s5m3700x_write(s5m3700x, S5M3700X_2B7_CTRL_EP0, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_2B8_CTRL_EP1, 0x13);
		s5m3700x_write(s5m3700x, S5M3700X_2B9_CTRL_EP2, 0x63);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA,
				APW_RCV_MASK, APW_RCV_MASK);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA,
				APW_RCV_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_04F_HSYS_CP2_SIGN, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x33);

		/* IDAC & EP Control Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_149_PD_EP, 0xD0);

		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1,
				RSTB_DAC_DSM_C_MASK, 0);
		break;
	}
}

static void ep_12ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1,
				RSTB_DAC_DSM_C_MASK, RSTB_DAC_DSM_C_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_04F_HSYS_CP2_SIGN, 0x82);

		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xCC);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xCC);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x22);

		/* Class-H */
		s5m3700x_write(s5m3700x, S5M3700X_2C4_CP2_TH2_32, 0x7B);
		s5m3700x_write(s5m3700x, S5M3700X_2C5_CP2_TH3_32, 0x7F);

		/* IDAC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_288_IDAC3_OTP, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_289_IDAC4_OTP, 0x15);
		s5m3700x_write(s5m3700x, S5M3700X_28A_IDAC5_OTP, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_149_PD_EP, 0x80);

		/* EP Control Setting */
		s5m3700x_write(s5m3700x, S5M3700X_2B7_CTRL_EP0, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_2B8_CTRL_EP1, 0x24);
		s5m3700x_write(s5m3700x, S5M3700X_2B9_CTRL_EP2, 0x63);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA,
				APW_RCV_MASK, APW_RCV_MASK);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA,
				APW_RCV_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_04F_HSYS_CP2_SIGN, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x33);

		/* IDAC & EP Control Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_149_PD_EP, 0xD0);

		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1,
				RSTB_DAC_DSM_C_MASK, 0);
		break;
	}
}

static void ep_6ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1,
				RSTB_DAC_DSM_C_MASK, RSTB_DAC_DSM_C_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_04F_HSYS_CP2_SIGN, 0x82);

		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xCC);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xCC);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x22);

		/* Class-H */
		s5m3700x_write(s5m3700x, S5M3700X_2C4_CP2_TH2_32, 0x75);
		s5m3700x_write(s5m3700x, S5M3700X_2C5_CP2_TH3_32, 0x79);

		/* IDAC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_288_IDAC3_OTP, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_289_IDAC4_OTP, 0x15);
		s5m3700x_write(s5m3700x, S5M3700X_28A_IDAC5_OTP, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_149_PD_EP, 0x80);

		/* EP Control Setting */
		s5m3700x_write(s5m3700x, S5M3700X_2B7_CTRL_EP0, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_2B8_CTRL_EP1, 0x24);
		s5m3700x_write(s5m3700x, S5M3700X_2B9_CTRL_EP2, 0x63);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA,
				APW_RCV_MASK, APW_RCV_MASK);
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA,
				APW_RCV_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_04F_HSYS_CP2_SIGN, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x33);

		/* IDAC & EP Control Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_149_PD_EP, 0xD0);

		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1,
				RSTB_DAC_DSM_C_MASK, 0);
		break;
	}
}

static int epdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	switch (s5m3700x->rcv_imp) {
	case RCV_IMP_32:
		dev_info(codec->dev, "%s called, ep: 32ohm, event = %d\n",
				__func__, event);
		ep_32ohm(codec, s5m3700x, event);
		break;
	case RCV_IMP_12:
		dev_info(codec->dev, "%s called, ep: 12ohm, event = %d\n",
				__func__, event);
		ep_12ohm(codec, s5m3700x, event);
		break;
	case RCV_IMP_6:
		dev_info(codec->dev, "%s called, ep: 6ohm, event = %d\n",
				__func__, event);
		ep_6ohm(codec, s5m3700x, event);
		break;
	default:
		dev_info(codec->dev, "%s called, default ep: 32ohm, event = %d\n",
				__func__, event);
		ep_32ohm(codec, s5m3700x, event);
		break;
	}

	return 0;
}

static int s5m3700x_dapm_ep_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_EP))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_ep_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_EP);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_EP);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : EP %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}

static const struct snd_kcontrol_new ep_on[] = {
	SOC_DAPM_ENUM_EXT("EP On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_ep_on_get, s5m3700x_dapm_ep_on_put),
};

/* LINE Device */
static int linedrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1,
				RSTB_DAC_DSM_C_MASK, RSTB_DAC_DSM_C_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_04F_HSYS_CP2_SIGN, 0x05);

		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xBB);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xBB);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x22);

		/* Class-H */
		s5m3700x_write(s5m3700x, S5M3700X_2C4_CP2_TH2_32, 0x77);
		s5m3700x_write(s5m3700x, S5M3700X_2C5_CP2_TH3_32, 0x7D);

		/* IDAC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_288_IDAC3_OTP, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_289_IDAC4_OTP, 0x15);
		s5m3700x_write(s5m3700x, S5M3700X_28A_IDAC5_OTP, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_14A_PD_LO, 0x80);

		s5m3700x_write(s5m3700x, S5M3700X_2BA_CTRL_LN0, 0x22);
		s5m3700x_write(s5m3700x, S5M3700X_2BB_CTRL_LN1, 0x63);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA,
				APW_LINE_MASK, APW_LINE_MASK);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA,
				APW_LINE_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_04F_HSYS_CP2_SIGN, 0x85);
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xBB);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xBB);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x22);

		/* IDAC & EP Control Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_14A_PD_LO, 0xD0);

		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_015_RESETB1,
				RSTB_DAC_DSM_C_MASK, 0);
		break;
	}
	return 0;
}

static int s5m3700x_dapm_ln_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_LINE))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_ln_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_LINE);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_LINE);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : LINE %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}

static const struct snd_kcontrol_new line_on[] = {
	SOC_DAPM_ENUM_EXT("LN On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_ln_on_get, s5m3700x_dapm_ln_on_put),
};

/* Headphone Device */
static void hp_32ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xEE);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x35);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xEE);

		/* Class-H CP2 Setting */
		s5m3700x_write(s5m3700x, S5M3700X_2C4_CP2_TH2_32, 0x7E);
		s5m3700x_write(s5m3700x, S5M3700X_2C5_CP2_TH3_32, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_2C6_CP2_TH1_16, 0x7C);
		s5m3700x_write(s5m3700x, S5M3700X_2C7_CP2_TH2_16, 0x81);

		/* IDAC Chop Enable */
		s5m3700x_write(s5m3700x, S5M3700X_131_PD_IDAC2, 0xE0);
		s5m3700x_write(s5m3700x, S5M3700X_017_CLK_MODE_SEL2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_2BE_IDAC0_OTP, 0X81);

		/* BIAS & HPZ & CC Setting */
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_HP_BODY_MASK | SEL_RES_SUB_MASK, 0);
		s5m3700x_write(s5m3700x, S5M3700X_13B_OCP_HP, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_2B3_CTRL_IHP, 0x13);
		s5m3700x_write(s5m3700x, S5M3700X_2B5_CTRL_HP1, 0x13);

		switch (s5m3700x->playback_params.aifrate) {
		case S5M3700X_SAMPLE_RATE_192KHZ:
		case S5M3700X_SAMPLE_RATE_384KHZ:
			s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x44);
			s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_UHQA_MASK, EN_UHQA_MASK);
			s5m3700x_write(s5m3700x, S5M3700X_2B4_CTRL_HP0, 0x13);
			break;
		case S5M3700X_SAMPLE_RATE_48KHZ:
		default:
			s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x22);
			s5m3700x_write(s5m3700x, S5M3700X_2B4_CTRL_HP0, 0x63);
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_write(s5m3700x, S5M3700X_07A_AVC43, 0x3A);
		/* HP APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA, APW_HP_MASK, APW_HP_MASK);
		s5m3700x_write(s5m3700x, S5M3700X_07A_AVC43, 0x9A);

		/* OVP Setting */
		s5m3700x_write(s5m3700x, S5M3700X_142_REF_SURGE, 0x29);
		s5m3700x_write(s5m3700x, S5M3700X_143_CTRL_OVP1, 0x12);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);
		/* HP APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA, APW_HP_MASK, 0);

		/* OVP Off */
		s5m3700x_write(s5m3700x, S5M3700X_143_CTRL_OVP1, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_142_REF_SURGE, 0x67);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xBE);

		/* IDAC Chop Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_131_PD_IDAC2, 0xC0);
		s5m3700x_write(s5m3700x, S5M3700X_017_CLK_MODE_SEL2, 0x08);

		/* BIAS & HPZ & CC Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_13B_OCP_HP, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x33);
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK);
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_UHQA_MASK, 0);

		/* AVC & Playmode Setting Default */
		s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1, DAC_FSEL_MASK | AVC_CLK_SEL_MASK, 0);
		s5m3700x_write(s5m3700x, S5M3700X_040_PLAY_MODE, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_047_TRIM_DAC0, 0xF7);
		s5m3700x_write(s5m3700x, S5M3700X_048_TRIM_DAC1, 0x4F);
		s5m3700x_write(s5m3700x, S5M3700X_072_AVC35, 0x2F);
		s5m3700x_write(s5m3700x, S5M3700X_074_AVC37, 0xE9);
		s5m3700x_write(s5m3700x, S5M3700X_07A_AVC43, 0xC0);

		/* AVC Default Setting */
		s5m3700x_write(s5m3700x, S5M3700X_050_AVC1, 0x05);
		s5m3700x_write(s5m3700x, S5M3700X_051_AVC2, 0x6A);
		break;
	}
}

static void hp_6ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xEE);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x35);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xEE);

		/* Class-H CP2 Setting */
		s5m3700x_write(s5m3700x, S5M3700X_2C4_CP2_TH2_32, 0x7E);
		s5m3700x_write(s5m3700x, S5M3700X_2C5_CP2_TH3_32, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_2C6_CP2_TH1_16, 0x79);
		s5m3700x_write(s5m3700x, S5M3700X_2C7_CP2_TH2_16, 0x80);
		s5m3700x_write(s5m3700x, S5M3700X_2CA_CP2_TH2_06, 0x75);
		s5m3700x_write(s5m3700x, S5M3700X_2CB_CP2_TH3_06, 0x7B);

		/* IDAC Chop Setting */
		s5m3700x_write(s5m3700x, S5M3700X_131_PD_IDAC2, 0xE0);
		s5m3700x_write(s5m3700x, S5M3700X_017_CLK_MODE_SEL2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_2BE_IDAC0_OTP, 0x81);

		/* BIAS & HPZ & CC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_13B_OCP_HP, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_2B3_CTRL_IHP, 0x13);
		s5m3700x_write(s5m3700x, S5M3700X_2B5_CTRL_HP1, 0x13);
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_HP_BODY_MASK | SEL_RES_SUB_MASK, 0);
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_UHQA_MASK, EN_UHQA_MASK);

		switch (s5m3700x->playback_params.aifrate) {
		case S5M3700X_SAMPLE_RATE_192KHZ:
		case S5M3700X_SAMPLE_RATE_384KHZ:
			s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x44);
			s5m3700x_write(s5m3700x, S5M3700X_2B4_CTRL_HP0, 0x12);
			break;
		case S5M3700X_SAMPLE_RATE_48KHZ:
		default:
			s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x22);
			s5m3700x_write(s5m3700x, S5M3700X_2B4_CTRL_HP0, 0x13);
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* HP APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA, APW_HP_MASK, APW_HP_MASK);
		s5m3700x_write(s5m3700x, S5M3700X_07A_AVC43, 0x9A);

		/* OVP Setting */
		s5m3700x_write(s5m3700x, S5M3700X_142_REF_SURGE, 0x29);
		s5m3700x_write(s5m3700x, S5M3700X_143_CTRL_OVP1, 0x12);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);
		/* HP APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_019_PWAUTO_DA, APW_HP_MASK, 0);

		/* OVP Off */
		s5m3700x_write(s5m3700x, S5M3700X_143_CTRL_OVP1, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_142_REF_SURGE, 0x67);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_155_DA_CP2, 0xBE);

		/* IDAC Chop Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_131_PD_IDAC2, 0xC0);
		s5m3700x_write(s5m3700x, S5M3700X_017_CLK_MODE_SEL2, 0x08);

		/* BIAS & HPZ & CC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_13B_OCP_HP, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_156_DA_CP3, 0x33);
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK);
		s5m3700x_update_bits(s5m3700x, S5M3700X_13A_POP_HP, EN_UHQA_MASK, 0);

		/* AVC & Playmode Setting Default */
		s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1, DAC_FSEL_MASK | AVC_CLK_SEL_MASK, 0);
		s5m3700x_write(s5m3700x, S5M3700X_040_PLAY_MODE, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_047_TRIM_DAC0, 0xF7);
		s5m3700x_write(s5m3700x, S5M3700X_048_TRIM_DAC1, 0x4F);
		s5m3700x_write(s5m3700x, S5M3700X_072_AVC35, 0x2F);
		s5m3700x_write(s5m3700x, S5M3700X_074_AVC37, 0xE9);
		s5m3700x_write(s5m3700x, S5M3700X_07A_AVC43, 0xC0);

		/* AVC Default Setting */
		s5m3700x_write(s5m3700x, S5M3700X_050_AVC1, 0x05);
		s5m3700x_write(s5m3700x, S5M3700X_051_AVC2, 0x6A);
		break;
	}
}

static int hpdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	switch (s5m3700x->hp_imp) {
	case HP_IMP_6:
		dev_info(codec->dev, "%s called, hp: 16ohm (%d) , event = %d\n",
				__func__, s5m3700x->hp_imp, event);
		hp_6ohm(codec, s5m3700x, event);
		break;
	case HP_IMP_32:
	default:
		dev_info(codec->dev, "%s called, hp: 32ohm (%d) , event = %d\n",
				__func__, s5m3700x->hp_imp, event);
		hp_32ohm(codec, s5m3700x, event);
		break;
	}
	return 0;
}

static int s5m3700x_dapm_hp_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_HP))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3700x_dapm_hp_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3700x_add_device_status(s5m3700x, DEVICE_HP);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3700x_remove_device_status(s5m3700x, DEVICE_HP);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : Headphone %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3700x->status);

	return 0;
}

static const struct snd_kcontrol_new hp_on[] = {
	SOC_DAPM_ENUM_EXT("HP On", s5m3700x_device_enable_enum,
					s5m3700x_dapm_hp_on_get, s5m3700x_dapm_hp_on_put),
};

static const struct snd_soc_dapm_widget s5m3700x_dapm_widgets[] = {
	/*
	 * ADC(Tx) dapm widget
	 */
	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),
	SND_SOC_DAPM_INPUT("IN3L"),
	SND_SOC_DAPM_INPUT("IN4L"),

	SND_SOC_DAPM_SUPPLY("VMID", SND_SOC_NOPM, 0, 0, vmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("DVMID", SND_SOC_NOPM, 0, 0, dvmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("MIC1_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC2_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC3_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic3_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC4_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic4_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("DMIC1_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, dmic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DMIC2_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, dmic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("MIC1", SND_SOC_NOPM, 0, 0, mic1_on),
	SND_SOC_DAPM_SWITCH("MIC2", SND_SOC_NOPM, 0, 0, mic2_on),
	SND_SOC_DAPM_SWITCH("MIC3", SND_SOC_NOPM, 0, 0, mic3_on),
	SND_SOC_DAPM_SWITCH("MIC4", SND_SOC_NOPM, 0, 0, mic4_on),

	SND_SOC_DAPM_SWITCH("DMIC1", SND_SOC_NOPM, 0, 0, dmic1_on),
	SND_SOC_DAPM_SWITCH("DMIC2", SND_SOC_NOPM, 0, 0, dmic2_on),

	SND_SOC_DAPM_MUX("INP_SEL_L", SND_SOC_NOPM, 0, 0, &s5m3700x_inp_sel_l),
	SND_SOC_DAPM_MUX("INP_SEL_R", SND_SOC_NOPM, 0, 0, &s5m3700x_inp_sel_r),
	SND_SOC_DAPM_MUX("INP_SEL_C", SND_SOC_NOPM, 0, 0, &s5m3700x_inp_sel_c),

	SND_SOC_DAPM_ADC_E("ADC", "AIF Capture", SND_SOC_NOPM, 0, 0, adc_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	/*
	 * DAC(Rx) dapm widget
	 */
	SND_SOC_DAPM_DAC_E("DAC", "AIF Playback", SND_SOC_NOPM, 0, 0, dac_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUT_DRV_E("EPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, epdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUT_DRV_E("LNDRV", SND_SOC_NOPM, 0, 0, NULL, 0, linedrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUT_DRV_E("HPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, hpdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("EP", SND_SOC_NOPM, 0, 0, ep_on),
	SND_SOC_DAPM_SWITCH("LINE", SND_SOC_NOPM, 0, 0, line_on),
	SND_SOC_DAPM_SWITCH("HP", SND_SOC_NOPM, 0, 0, hp_on),

	SND_SOC_DAPM_OUTPUT("EPOUTN"),
	SND_SOC_DAPM_OUTPUT("LNOUTN"),
	SND_SOC_DAPM_OUTPUT("HPOUTLN"),
};

/*
 * snd_soc_dapm_route set
 */
static const struct snd_soc_dapm_route s5m3700x_dapm_routes[] = {
	/*
	 * ADC(Tx) dapm route
	 */
	{"MIC1_PGA", NULL, "IN1L"},
	{"MIC1_PGA", NULL, "VMID"},
	{"MIC1", "MIC1 On", "MIC1_PGA"},

	{"MIC2_PGA", NULL, "IN2L"},
	{"MIC2_PGA", NULL, "VMID"},
	{"MIC2", "MIC2 On", "MIC2_PGA"},

	{"MIC3_PGA", NULL, "IN3L"},
	{"MIC3_PGA", NULL, "VMID"},
	{"MIC3", "MIC3 On", "MIC3_PGA"},

	{"MIC4_PGA", NULL, "IN4L"},
	{"MIC4_PGA", NULL, "VMID"},
	{"MIC4", "MIC4 On", "MIC4_PGA"},

	{"DMIC1_PGA", NULL, "IN1L"},
	{"DMIC1_PGA", NULL, "DVMID"},
	{"DMIC1", "DMIC1 On", "DMIC1_PGA"},

	{"DMIC2_PGA", NULL, "IN2L"},
	{"DMIC2_PGA", NULL, "DVMID"},
	{"DMIC2", "DMIC2 On", "DMIC2_PGA"},

	{"INP_SEL_L", "AMIC_L ADC_L", "MIC1"},
	{"INP_SEL_L", "AMIC_C ADC_L", "MIC2"},
	{"INP_SEL_L", "AMIC_R ADC_L", "MIC3"},
	{"INP_SEL_L", "AMIC_R ADC_L", "MIC4"},
	{"INP_SEL_L", "DMIC1L ADC_L", "DMIC1"},
	{"INP_SEL_L", "DMIC1R ADC_L", "DMIC1"},
	{"INP_SEL_L", "DMIC2L ADC_L", "DMIC2"},
	{"INP_SEL_L", "DMIC2R ADC_L", "DMIC2"},

	{"INP_SEL_R", "AMIC_L ADC_R", "MIC1"},
	{"INP_SEL_R", "AMIC_C ADC_R", "MIC2"},
	{"INP_SEL_R", "AMIC_R ADC_R", "MIC3"},
	{"INP_SEL_R", "AMIC_R ADC_R", "MIC4"},
	{"INP_SEL_R", "DMIC1L ADC_R", "DMIC1"},
	{"INP_SEL_R", "DMIC1R ADC_R", "DMIC1"},
	{"INP_SEL_R", "DMIC2L ADC_R", "DMIC2"},
	{"INP_SEL_R", "DMIC2R ADC_R", "DMIC2"},

	{"INP_SEL_C", "AMIC_L ADC_C", "MIC1"},
	{"INP_SEL_C", "AMIC_C ADC_C", "MIC2"},
	{"INP_SEL_C", "AMIC_R ADC_C", "MIC3"},
	{"INP_SEL_C", "AMIC_R ADC_C", "MIC4"},
	{"INP_SEL_C", "DMIC1L ADC_C", "DMIC1"},
	{"INP_SEL_C", "DMIC1R ADC_C", "DMIC1"},
	{"INP_SEL_C", "DMIC2L ADC_C", "DMIC2"},
	{"INP_SEL_C", "DMIC2R ADC_C", "DMIC2"},

	{"ADC", NULL, "INP_SEL_L"},
	{"ADC", NULL, "INP_SEL_R"},
	{"ADC", NULL, "INP_SEL_C"},

	{"AIF Capture", NULL, "ADC"},

	/*
	 * DAC(Rx) dapm route
	 */
	{"DAC", NULL, "AIF Playback"},

	{"EPDRV", NULL, "DAC"},
	{"EP", "EP On", "EPDRV"},
	{"EPOUTN", NULL, "EP"},

	{"LNDRV", NULL, "DAC"},
	{"LINE", "LN On", "LNDRV"},
	{"LNOUTN", NULL, "LINE"},

	{"HPDRV", NULL, "DAC"},
	{"HP", "HP On", "HPDRV"},
	{"HPOUTLN", NULL, "HP"},
};

/*
 * snd_soc_dai_driver set
 */
static int s5m3700x_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called. fmt: %d\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		s5m3700x_update_bits(s5m3700x, S5M3700X_020_IF_FORM1,
				I2S_DF_MASK, 0x00);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		s5m3700x_update_bits(s5m3700x, S5M3700X_020_IF_FORM1,
				I2S_DF_MASK, 0x10);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		s5m3700x_update_bits(s5m3700x, S5M3700X_020_IF_FORM1,
				I2S_DF_MASK, 0x20);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		s5m3700x_update_bits(s5m3700x, S5M3700X_020_IF_FORM1,
				I2S_DF_MASK, 0x40);
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		s5m3700x_update_bits(s5m3700x, S5M3700X_020_IF_FORM1,
				LRCK_POL_MASK|BCK_POL_MASK, 0x00);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		s5m3700x_update_bits(s5m3700x, S5M3700X_020_IF_FORM1,
				LRCK_POL_MASK|BCK_POL_MASK, LRCK_POL_MASK);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		s5m3700x_update_bits(s5m3700x, S5M3700X_020_IF_FORM1,
				LRCK_POL_MASK|BCK_POL_MASK, BCK_POL_MASK);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		s5m3700x_update_bits(s5m3700x, S5M3700X_020_IF_FORM1,
				LRCK_POL_MASK|BCK_POL_MASK, LRCK_POL_MASK|BCK_POL_MASK);
		break;
	}

	return 0;
}

static const unsigned int s5m3700x_src_rates[] = {
	48000, 192000, 384000
};

static const struct snd_pcm_hw_constraint_list s5m3700x_constraints = {
	.count = ARRAY_SIZE(s5m3700x_src_rates),
	.list = s5m3700x_src_rates,
};

static int s5m3700x_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int ret = 0;

	dev_info(s5m3700x->dev, "%s : startup for %s\n", __func__, substream->stream ? "Capture" : "Playback");

	if (substream->runtime)
		ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
						SNDRV_PCM_HW_PARAM_RATE, &s5m3700x_constraints);

	if (ret < 0)
		dev_err(s5m3700x->dev, "%s : Unsupported sample rates", __func__);

	return ret;
}

/*
 * capture_hw_params() - Register setting for capture
 *
 * @codec: SoC audio codec device
 * @cur_aifrate: current sample rate
 *
 * Desc: Set codec register related sample rate format before capture.
 */
static void capture_hw_params(struct snd_soc_component *codec,
		unsigned int cur_aifrate)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n",
			__func__, s5m3700x->capture_params.aifrate, cur_aifrate);
	s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);

	switch (cur_aifrate) {
	case S5M3700X_SAMPLE_RATE_48KHZ:
		/* 48K output format selection */
		s5m3700x_update_bits(s5m3700x, S5M3700X_029_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_FM_48K_AT_48K);
		break;
	case S5M3700X_SAMPLE_RATE_192KHZ:
		/* 192K output format selection */
		s5m3700x_update_bits(s5m3700x, S5M3700X_029_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_FM_192K_AT_192K);
		break;
	case S5M3700X_SAMPLE_RATE_384KHZ:
		/* 192K output format selection */
		s5m3700x_update_bits(s5m3700x, S5M3700X_029_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_FM_192K_AT_192K);
		break;
	default:
		dev_err(codec->dev, "%s: sample rate error!\n", __func__);
		/* 48K output format selection */
		s5m3700x_update_bits(s5m3700x, S5M3700X_029_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_FM_48K_AT_48K);
		break;
	}
}

/*
 * playback_hw_params() - Register setting for playback
 *
 * @codec: SoC audio codec device
 * @cur_aifrate: current sample rate
 *
 * Desc: Set codec register related sample rate format before playback.
 */
static void playback_hw_params(struct snd_soc_component *codec,
		unsigned int cur_aifrate)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int i = 0;

	dev_info(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n",
			__func__, s5m3700x->playback_params.aifrate, cur_aifrate);

	s5m3700x_write(s5m3700x, S5M3700X_07A_AVC43, 0x9A);

	switch (cur_aifrate) {
	case S5M3700X_SAMPLE_RATE_48KHZ:
		/* AVC Var Mode on */
		s5m3700x_write(s5m3700x, S5M3700X_050_AVC1, 0x05);

		/* AVC Range Default Setting */
		s5m3700x_write(s5m3700x, S5M3700X_051_AVC2, 0x6A);

		/* CLK Mode Selection */
		s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1,
				DAC_FSEL_MASK, 0);

		/* PlayMode Selection */
		s5m3700x_write(s5m3700x, S5M3700X_040_PLAY_MODE, 0x04);

		/* DAC Gain trim */
		s5m3700x_write(s5m3700x, S5M3700X_046_PLAY_MIX2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_047_TRIM_DAC0, 0xF7);
		s5m3700x_write(s5m3700x, S5M3700X_048_TRIM_DAC1, 0x4F);
		/* AVC Delay Setting */
		s5m3700x_write(s5m3700x, S5M3700X_071_AVC34, 0x18);

		/* Setting the HP Normal Trim */
		for (i = 0; i < 50; i++)
			s5m3700x_write(s5m3700x, S5M3700X_200_HP_OFFSET_L0 + i, s5m3700x->hp_normal_trim[i]);

		s5m3700x_write(s5m3700x, S5M3700X_072_AVC35, 0x2F);

		s5m3700x_write(s5m3700x, S5M3700X_073_AVC36, 0x17);
		s5m3700x_write(s5m3700x, S5M3700X_074_AVC37, 0xE9);
		break;
	case S5M3700X_SAMPLE_RATE_192KHZ:
		/* AVC Var Mode off */
		s5m3700x_write(s5m3700x, S5M3700X_050_AVC1, 0x01);

		/* AVC Range Default Setting */
		s5m3700x_write(s5m3700x, S5M3700X_051_AVC2, 0x50);

		/* CLK Mode Selection */
		s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1,
				DAC_FSEL_MASK, DAC_FSEL_MASK);

		/* PlayMode Selection */
		s5m3700x_write(s5m3700x, S5M3700X_040_PLAY_MODE, 0x44);

		/* DAC Gain trim */
		s5m3700x_write(s5m3700x, S5M3700X_046_PLAY_MIX2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_047_TRIM_DAC0, 0xFF);
		s5m3700x_write(s5m3700x, S5M3700X_048_TRIM_DAC1, 0x1E);

		/* Setting the HP UHQA Trim */
		for (i = 0; i < 50; i++)
			s5m3700x_write(s5m3700x, S5M3700X_200_HP_OFFSET_L0 + i, s5m3700x->hp_uhqa_trim[i]);

		/* AVC Delay Setting */
		s5m3700x_write(s5m3700x, S5M3700X_071_AVC34, 0xC6);
		s5m3700x_write(s5m3700x, S5M3700X_072_AVC35, 0x01);

		s5m3700x_write(s5m3700x, S5M3700X_073_AVC36, 0xC4);
		s5m3700x_write(s5m3700x, S5M3700X_074_AVC37, 0x39);
		break;
	case S5M3700X_SAMPLE_RATE_384KHZ:
		/* AVC Var Mode off */
		s5m3700x_write(s5m3700x, S5M3700X_050_AVC1, 0x01);

		/* AVC Range Default Setting */
		s5m3700x_write(s5m3700x, S5M3700X_051_AVC2, 0x50);

		/* CLK Mode Selection */
		s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1,
				DAC_FSEL_MASK | AVC_CLK_SEL_MASK,
				DAC_FSEL_MASK | AVC_CLK_SEL_MASK);

		/* PlayMode Selection */
		s5m3700x_write(s5m3700x, S5M3700X_040_PLAY_MODE, 0x54);

		/* DAC Gain trim */
		s5m3700x_write(s5m3700x, S5M3700X_046_PLAY_MIX2, 0x54);
		s5m3700x_write(s5m3700x, S5M3700X_047_TRIM_DAC0, 0xFD);
		s5m3700x_write(s5m3700x, S5M3700X_048_TRIM_DAC1, 0x12);

		/* AVC Delay Setting */
		s5m3700x_write(s5m3700x, S5M3700X_071_AVC34, 0xC6);

		/* Setting the HP UHQA Trim */
		for (i = 0; i < 50; i++)
			s5m3700x_write(s5m3700x, S5M3700X_200_HP_OFFSET_L0 + i, s5m3700x->hp_uhqa_trim[i]);

		s5m3700x_write(s5m3700x, S5M3700X_072_AVC35, 0x00);

		s5m3700x_write(s5m3700x, S5M3700X_073_AVC36, 0xC4);
		s5m3700x_write(s5m3700x, S5M3700X_074_AVC37, 0x3A);
		break;
	default:
		/* CLK Mode Selection */
		s5m3700x_update_bits(s5m3700x, S5M3700X_016_CLK_MODE_SEL1,
				DAC_FSEL_MASK, 0);

		/* PlayMode Selection */
		s5m3700x_write(s5m3700x, S5M3700X_040_PLAY_MODE, 0x04);

		/* DAC Gain trim */
		s5m3700x_write(s5m3700x, S5M3700X_046_PLAY_MIX2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_047_TRIM_DAC0, 0xF7);
		s5m3700x_write(s5m3700x, S5M3700X_048_TRIM_DAC1, 0x4F);
		/* AVC Delay Setting */
		s5m3700x_write(s5m3700x, S5M3700X_071_AVC34, 0x18);

		/* Setting the HP Normal Trim */
		for (i = 0; i < 50; i++)
			s5m3700x_write(s5m3700x, S5M3700X_200_HP_OFFSET_L0 + i, s5m3700x->hp_normal_trim[i]);

		s5m3700x_write(s5m3700x, S5M3700X_072_AVC35, 0x2F);

		s5m3700x_write(s5m3700x, S5M3700X_073_AVC36, 0x17);
		s5m3700x_write(s5m3700x, S5M3700X_074_AVC37, 0xE9);

		dev_err(codec->dev, "%s: sample rate error!\n", __func__);
		break;
	}
}

/*
 *set i2s data length and Sampling frequency
 * XFS = Data Length * Channels
 */
static void s5m3700x_set_i2s_configuration(struct s5m3700x_priv *s5m3700x, unsigned int width, unsigned int channels)
{
	switch (width) {
	case BIT_RATE_16:
		/* I2S 16bit Set */
		s5m3700x_update_bits(s5m3700x, S5M3700X_021_IF_FORM2, I2S_DL_MASK, I2S_DL_16);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_32);
			break;
		case CHANNEL_4:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_64);
			break;
		default:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_32);
			break;
		}
		break;
	case BIT_RATE_24:
		/* I2S 24bit Set */
		s5m3700x_update_bits(s5m3700x, S5M3700X_021_IF_FORM2, I2S_DL_MASK, I2S_DL_24);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_48);
			break;
		case CHANNEL_4:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_96);
			break;
		default:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_48);
			break;
		}
		break;
	case BIT_RATE_32:
		/* I2S 32bit Set */
		s5m3700x_update_bits(s5m3700x, S5M3700X_021_IF_FORM2, I2S_DL_MASK, I2S_DL_32);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_64);
			break;
		case CHANNEL_4:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_128);
			break;
		default:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_64);
			break;
		}
		break;
	default:
		dev_err(s5m3700x->dev, "%s: bit rate error!\n", __func__);
		/* I2S 16bit Set */
		s5m3700x_update_bits(s5m3700x, S5M3700X_021_IF_FORM2, I2S_DL_MASK, I2S_DL_16);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_32);
			break;
		case CHANNEL_4:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_64);
			break;
		default:
			s5m3700x_update_bits(s5m3700x, S5M3700X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_32);
			break;
		}
		break;
	}
}

static int s5m3700x_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int cur_aifrate, width, channels;

	/* Get params */
	cur_aifrate = params_rate(params);
	width = params_width(params);
	channels = params_channels(params);

	dev_info(codec->dev, "(%s) %s called. aifrate: %d, width: %d, channels: %d\n",
			substream->stream ? "C" : "P", __func__, cur_aifrate, width, channels);

	s5m3700x_set_i2s_configuration(s5m3700x, width, channels);

	if (substream->stream) {
		capture_hw_params(codec, cur_aifrate);
		s5m3700x->capture_params.aifrate = cur_aifrate;
		s5m3700x->capture_params.width = width;
		s5m3700x->capture_params.channels = channels;
	} else {
		playback_hw_params(codec, cur_aifrate);
		s5m3700x->playback_params.aifrate = cur_aifrate;
		s5m3700x->playback_params.width = width;
		s5m3700x->playback_params.channels = channels;
	}

	return 0;
}

static void s5m3700x_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;

	dev_info(codec->dev, "(%s) %s completed.\n", __func__,
			substream->stream ? "C" : "P");
}

static const struct snd_soc_dai_ops s5m3700x_dai_ops = {
	.set_fmt = s5m3700x_dai_set_fmt,
	.startup = s5m3700x_dai_startup,
	.hw_params = s5m3700x_dai_hw_params,
	.shutdown = s5m3700x_dai_shutdown,
};

#define S5M3700X_RATES		SNDRV_PCM_RATE_8000_192000
#define S5M3700X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
							SNDRV_PCM_FMTBIT_S20_3LE | \
							SNDRV_PCM_FMTBIT_S24_LE  | \
							SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver s5m3700x_dai[] = {
	{
		.name = "s5m3700x-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = S5M3700X_RATES,
			.formats = S5M3700X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = S5M3700X_RATES,
			.formats = S5M3700X_FORMATS,
		},
		.ops = &s5m3700x_dai_ops,
		.symmetric_rate = 1,
	},
};

static void s5m3700x_i2c_parse_dt(struct s5m3700x_priv *s5m3700x)
{
	struct device *dev = s5m3700x->dev;
	struct device_node *np = dev->of_node;
	unsigned int jack_type;

	/* RESETB gpio */
	s5m3700x->resetb_gpio = of_get_named_gpio(np, "s5m3700x-resetb", 0);
	if (s5m3700x->resetb_gpio < 0)
		dev_err(dev, "%s: cannot find resetb gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: resetb gpio = %d\n",
				__func__, s5m3700x->resetb_gpio);

	/* 1.8V external_ldo gpio */
	s5m3700x->external_ldo_1_8v = of_get_named_gpio(np, "s5m3700x-external-ldo-1-8v", 0);
	if (s5m3700x->external_ldo_1_8v < 0)
		dev_err(dev, "%s: cannot find 1.8V external_ldo gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: 1,8V external_ldo gpio = %d\n",
				__func__, s5m3700x->external_ldo_1_8v);

	 /* 3.4V external_ldo gpio */
        s5m3700x->external_ldo_3_4v = of_get_named_gpio(np, "s5m3700x-external-ldo-3-4v", 0);
        if (s5m3700x->external_ldo_3_4v < 0)
                dev_err(dev, "%s: cannot find 3.4V external_ldo gpio in the dt\n", __func__);
        else
                dev_info(dev, "%s: 3.4V external_ldo gpio = %d\n",
                                __func__, s5m3700x->external_ldo_3_4v);

        /* Always Power On*/
        s5m3700x->power_always_on = of_property_read_bool(np, "s5m3700x-power-always-on");
        dev_info(dev, "%s: power_always_on = %d\n", __func__, s5m3700x->power_always_on);


	/* receiver impedance */
	if (!of_property_read_u32(np, "s5m3700x-rcv_imp", &s5m3700x->rcv_imp)) {
		if ((s5m3700x->rcv_imp > 2) || (s5m3700x->rcv_imp < 0)) {
			dev_err(dev, "%s: rcv_imp is out of range, set default impedance\n", __func__);
			s5m3700x->rcv_imp = RCV_IMP_DEFAULT;
		} else {
			dev_info(dev, "%s: rcv_imp = %d\n", __func__, s5m3700x->rcv_imp);
		}
	} else {
		dev_err(dev, "%s: cannot find rcv_imp in the dt, set default impedance\n", __func__);
		s5m3700x->rcv_imp = RCV_IMP_DEFAULT;
	}

	/*
	 * Setting Jack type
	 * 0: 3.5 Pi, 1: USB Type-C
	 */
	if (!of_property_read_u32(dev->of_node, "jack-type", &jack_type))
		s5m3700x->jack_type = jack_type;
	else
		s5m3700x->jack_type = JACK;

	dev_info(dev, "%s: Jack type: %s\n", __func__, s5m3700x->jack_type ? "USB Jack" : "3.5 PI");
}

static void s5m3700x_power_configuration(struct s5m3700x_priv *s5m3700x)
{
	/* Setting RESETB gpio */
	if (s5m3700x->resetb_gpio > 0) {
		if (gpio_request(s5m3700x->resetb_gpio, "s5m3700x_resetb_gpio") < 0)
			dev_err(s5m3700x->dev, "%s: Request for %d GPIO failed\n", __func__, (int)s5m3700x->resetb_gpio);
		/* turn off for default */
		if (gpio_direction_output(s5m3700x->resetb_gpio, 0) < 0)
			dev_err(s5m3700x->dev, "%s: GPIO direction to output failed!\n", __func__);
	}

	/* Setting 1.8V External LDO gpio */
	if (s5m3700x->external_ldo_1_8v > 0) {
		if (gpio_request(s5m3700x->external_ldo_1_8v, "s5m3700x_external_ldo_1_8v") < 0)
			dev_err(s5m3700x->dev, "%s: Request for %d GPIO failed\n", __func__, (int)s5m3700x->external_ldo_1_8v);
		/* turn off for default */
		if (gpio_direction_output(s5m3700x->external_ldo_1_8v, 0) < 0)
			dev_err(s5m3700x->dev, "%s: GPIO direction to output failed!\n", __func__);
	}

	/* Setting 3.4V External LDO gpio */
	if (s5m3700x->external_ldo_3_4v > 0) {
                if (gpio_request(s5m3700x->external_ldo_3_4v, "s5m3700x_external_ldo_3_4v") < 0)
                        dev_err(s5m3700x->dev, "%s: Request for %d GPIO failed\n", __func__, (int)s5m3700x->external_ldo_3_4v);
                /* turn off for default */
                if (gpio_direction_output(s5m3700x->external_ldo_3_4v, 0) < 0)
                        dev_err(s5m3700x->dev, "%s: GPIO direction to output failed!\n", __func__);
        }


	/* register codec power */

	s5m3700x->vdd = devm_regulator_get(s5m3700x->dev, "vdd_ldo31");
	if (IS_ERR(s5m3700x->vdd))
		dev_warn(s5m3700x->dev, "failed to get regulator vdd %d\n", PTR_ERR(s5m3700x->vdd));
}

/* Control external Regulator and GPIO for power supplier */
static void s5m3700x_power_supplier_enable(struct s5m3700x_priv *s5m3700x, bool enable)
{
	unsigned int ret;

	/* Enable External LDOs */
	if (enable) {
                if (s5m3700x->external_ldo_3_4v >= 0)
                        if (gpio_direction_output(s5m3700x->external_ldo_3_4v, 1) < 0)
                                dev_err(s5m3700x->dev, "%s: 3.4V external_ldo GPIO direction to output failed!\n", __func__);

		if (s5m3700x->external_ldo_1_8v >= 0)
			if (gpio_direction_output(s5m3700x->external_ldo_1_8v, 1) < 0)
				dev_err(s5m3700x->dev, "%s: 1.8V external_ldo GPIO direction to output failed!\n", __func__);

		 /* 1 msec HW delay */
        	 s5m3700x_usleep(1000);


		if (!IS_ERR(s5m3700x->vdd))
			if (!regulator_is_enabled(s5m3700x->vdd))
				ret = regulator_enable(s5m3700x->vdd);

		/* 2 msec HW delay */
		s5m3700x_usleep(2000);

		/* set resetb gpio high */
		if (s5m3700x->resetb_gpio >= 0)
			if (gpio_direction_output(s5m3700x->resetb_gpio, 1) < 0)
				dev_err(s5m3700x->dev, "%s: resetb_gpio GPIO direction to output failed!\n", __func__);
	} else {
		/* set resetb gpio low */
		if (s5m3700x->resetb_gpio >= 0)
			if (gpio_direction_output(s5m3700x->resetb_gpio, 0) < 0)
				dev_err(s5m3700x->dev, "%s: GPIO direction to output failed!\n", __func__);

		if (!IS_ERR(s5m3700x->vdd))
			if (regulator_is_enabled(s5m3700x->vdd))
				ret = regulator_disable(s5m3700x->vdd);

		if (s5m3700x->external_ldo_1_8v >= 0)
			if (gpio_direction_output(s5m3700x->external_ldo_1_8v, 0) < 0)
				dev_err(s5m3700x->dev, "%s: 1.8V external_ldo GPIO direction to output failed!\n", __func__);

		if (s5m3700x->external_ldo_3_4v >= 0)
                        if (gpio_direction_output(s5m3700x->external_ldo_3_4v, 0) < 0)
                                dev_err(s5m3700x->dev, "%s: 3.4V external_ldo GPIO direction to output failed!\n", __func__);

	}
}

/* Register is set on only HW Register */
/* It doesn't need to sync up with cache register */
static void s5m3700x_codec_power_enable(struct s5m3700x_priv *s5m3700x, bool enable)
{
	dev_info(s5m3700x->dev, "%s : enable %d\n", __func__, enable);
	if (enable) {
		/* power supplier */
		s5m3700x_power_supplier_enable(s5m3700x, true);

		/* BGR & IBIAS On */
		s5m3700x_update_bits_only_hardware(s5m3700x, S5M3700X_304_MV_ACTR1,
				PDB_IBIAS_MASK|PDB_BGR_MASK, PDB_IBIAS_MASK|PDB_BGR_MASK);
		s5m3700x_usleep(5000);

		/* DLDO On */
		s5m3700x_update_bits_only_hardware(s5m3700x, S5M3700X_304_MV_ACTR1,
				PDB_DIG_LDO_MASK, PDB_DIG_LDO_MASK);

		/* Audio Codec LV RESETB High */
		s5m3700x_update_bits_only_hardware(s5m3700x, S5M3700X_307_LV_DCTR,
			EN_LS_AUD_MASK|COD_RESETB_MASK, EN_LS_AUD_MASK|COD_RESETB_MASK);
		msleep(20);
	} else {
		/* Audio Codec LV RESETB low */
		s5m3700x_update_bits_only_hardware(s5m3700x, S5M3700X_307_LV_DCTR,
			EN_LS_AUD_MASK|COD_RESETB_MASK, 0x0);
		msleep(20);

		/* DLDO Off */
		s5m3700x_update_bits_only_hardware(s5m3700x, S5M3700X_304_MV_ACTR1,
				PDB_DIG_LDO_MASK, 0x0);
		s5m3700x_usleep(5000);

		/* BGR & IBIAS Off */
		s5m3700x_update_bits_only_hardware(s5m3700x, S5M3700X_304_MV_ACTR1,
				PDB_IBIAS_MASK|PDB_BGR_MASK, 0x0);

		/* power supplier */
		s5m3700x_power_supplier_enable(s5m3700x, false);
	}
}

void s5m3700x_update_otp_register(struct s5m3700x_priv *s5m3700x)
{
	int i = 0, array_size = ARRAY_SIZE(s5m3700x_reg_defaults), value = 0;
	bool is_find = false;

	pr_info("%s: enter\n", __func__);

	/* find start index for OTP Register */
	for (i = 0; i < array_size; i++) {
		if (s5m3700x_reg_defaults[i].reg == S5M3700X_200_HP_OFFSET_L0) {
			is_find = true;
			break;
		}
	}

	if (is_find) {
		/* Read OTP Register from HW and update reg_defaults. */
		while (s5m3700x_reg_defaults[i].reg != S5M3700X_304_MV_ACTR1) {
			if (!s5m3700x_read_only_hardware(s5m3700x, s5m3700x_reg_defaults[i].reg, &value))
				s5m3700x_reg_defaults[i].def = value;
			i++;
		}
	}
}

/*
 * apply register initialize by patch
 * patch is only updated hw register, so cache need to be updated.
 */
int s5m3700x_regmap_register_patch(struct s5m3700x_priv *s5m3700x)
{
	struct device *dev = s5m3700x->dev;
	int ret = 0;

	/* register patch for playback / capture initialze */
	ret = regmap_register_patch(s5m3700x->regmap, s5m3700x_init_patch,
				ARRAY_SIZE(s5m3700x_init_patch));
	if (ret < 0) {
		dev_err(dev, "Failed to apply s5m3700x_init_patch %d\n", ret);
		return ret;
	}

	/* update reg_defaults with registered patch */
	s5m3700x_update_reg_defaults(s5m3700x_init_patch, ARRAY_SIZE(s5m3700x_init_patch));
	return ret;
}

static void s5m3700x_register_initialize(struct s5m3700x_priv *s5m3700x)
{
	/* update otp initial registers on cache */
	s5m3700x_update_otp_register(s5m3700x);

	/* update initial registers on cache and hw registers */
	s5m3700x_regmap_register_patch(s5m3700x);

	/* reinit regmap cache because reg_defaults are updated*/
	s5m3700x_regmap_reinit_cache(s5m3700x);
}

static void s5m3700x_adc_mute_work(struct work_struct *work)
{
	struct s5m3700x_priv *s5m3700x =
		container_of(work, struct s5m3700x_priv, adc_mute_work.work);

	dev_info(s5m3700x->dev, "%s called, status: 0x%x\n", __func__, s5m3700x->status);

	if (s5m3700x_check_device_status(s5m3700x, DEVICE_CAPTURE_ON))
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, false);
}

/*
 * s5m3700x_adc_digital_mute() - Set ADC digital Mute
 *
 * @codec: SoC audio codec device
 * @channel: Digital mute control for ADC channel
 * @on: mic mute is true, mic unmute is false
 *
 * Desc: When ADC path turn on, analog block noise can be recorded.
 * For remove this, ADC path was muted always except that it was used.
 */
void s5m3700x_adc_digital_mute(struct s5m3700x_priv *s5m3700x,
		unsigned int channel, bool on)
{
	struct device *dev = s5m3700x->dev;

	mutex_lock(&s5m3700x->mute_lock);

	if (on)
		s5m3700x_update_bits(s5m3700x, S5M3700X_030_ADC1, channel, channel);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_030_ADC1, channel, 0);

	mutex_unlock(&s5m3700x->mute_lock);

	dev_info(dev, "%s called, %s work done.\n", __func__, on ? "Mute" : "Unmute");
}
EXPORT_SYMBOL_GPL(s5m3700x_adc_digital_mute);

/*
 * DAC(Rx) functions
 */
/*
 * s5m3700x_dac_soft_mute() - Set DAC soft mute
 *
 * @codec: SoC audio codec device
 * @channel: Soft mute control for DAC channel
 * @on: dac mute is true, dac unmute is false
 *
 * Desc: When DAC path turn on, analog block noise can be played.
 * For remove this, DAC path was muted always except that it was used.
 */
static void s5m3700x_dac_soft_mute(struct s5m3700x_priv *s5m3700x,
		unsigned int channel, bool on)
{
	struct device *dev = s5m3700x->dev;

	mutex_lock(&s5m3700x->mute_lock);

	if (on)
		s5m3700x_update_bits(s5m3700x, S5M3700X_040_PLAY_MODE, channel, channel);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_040_PLAY_MODE, channel, 0);

	mutex_unlock(&s5m3700x->mute_lock);

	dev_info(dev, "%s called, %s work done.\n", __func__, on ? "Mute" : "Unmute");
}

static int s5m3700x_codec_probe(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	pr_info("Codec Driver Probe: (%s).\n", __func__);

	s5m3700x->codec = codec;

	/* initialize Codec Power Supply */
	s5m3700x_power_configuration(s5m3700x);

	/* Codec Power Up */
	if (s5m3700x->power_always_on)
		s5m3700x_codec_power_enable(s5m3700x, true);

#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3700x->dev))
		pm_runtime_get_sync(s5m3700x->dev);
#endif

	/* register additional initialize */
	s5m3700x_register_initialize(s5m3700x);

	/* ADC/DAC Mute */
	s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);
	s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3700x->dev))
		pm_runtime_put_sync(s5m3700x->dev);
#endif

	/* Ignore suspend status for DAPM endpoint */
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "EPOUTN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "LNOUTN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "HPOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN1L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN2L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN3L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN4L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Capture");

#if IS_ENABLED(CONFIG_SND_SOC_S5M3700X_VTS)
	snd_soc_add_component_controls(codec, s5m3700x_vts_controls, ARRAY_SIZE(s5m3700x_vts_controls));
#endif

	/* Jack probe */
	if (s5m3700x->jack_type  == USB_JACK)
		s5m3700x_usbjack_probe(codec, s5m3700x);
	else
		s5m3700x_jack_probe(codec, s5m3700x);

	dev_info(codec->dev, "Codec Probe Done.\n");

	return 0;
}

static void s5m3700x_codec_remove(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct device *dev = s5m3700x->dev;

	dev_info(s5m3700x->dev, "(*) %s called\n", __func__);

	if (s5m3700x->jack_type  == USB_JACK)
		s5m3700x_usbjack_remove(codec);
	else
		s5m3700x_jack_remove(codec);

	s5m3700x_codec_disable(dev);
}

static const struct snd_soc_component_driver soc_codec_dev_s5m3700x = {
	.probe = s5m3700x_codec_probe,
	.remove = s5m3700x_codec_remove,
	.controls = s5m3700x_snd_controls,
	.num_controls = ARRAY_SIZE(s5m3700x_snd_controls),
	.dapm_widgets = s5m3700x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(s5m3700x_dapm_widgets),
	.dapm_routes = s5m3700x_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(s5m3700x_dapm_routes),
	.use_pmdown_time = false,
	.idle_bias_on = false,
};

static int s5m3700x_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct s5m3700x_priv *s5m3700x;
	int ret = 0;

	dev_info(&i2c->dev, "Codec I2C Probe: (%s) name: %s, size: %d, i2c addr: 0x%02x\n",
	__func__, id->name, sizeof(*s5m3700x), (int)i2c->addr);

	s5m3700x = kzalloc(sizeof(*s5m3700x), GFP_KERNEL);
	if (s5m3700x == NULL)
		return -ENOMEM;

	i2c->addr = S5M3700X_SLAVE_ADDR;
	s5m3700x->dev = &i2c->dev;

	/* initialize codec_priv variable */
	s5m3700x->codec_power_ref_cnt = 0;
	s5m3700x->cache_only = 0;
	s5m3700x->cache_bypass = 0;
	s5m3700x->power_always_on = false;
	s5m3700x->status = DEVICE_NONE;
	s5m3700x->vts_mic = VTS_UNDEFINED;
	s5m3700x->rcv_imp = RCV_IMP_DEFAULT;
	s5m3700x->hp_imp = HP_IMP_DEFAULT;
	s5m3700x->dmic_delay = 120;
	s5m3700x->amic_delay = 100;
	s5m3700x->playback_params.aifrate = S5M3700X_SAMPLE_RATE_48KHZ;
	s5m3700x->playback_params.width = BIT_RATE_16;
	s5m3700x->playback_params.channels = CHANNEL_2;
	s5m3700x->capture_params.aifrate = S5M3700X_SAMPLE_RATE_48KHZ;
	s5m3700x->capture_params.width = BIT_RATE_16;
	s5m3700x->capture_params.channels = CHANNEL_2;
	s5m3700x->jack_type = JACK;
	s5m3700x->hp_bias_cnt = 0;

	/* Parsing to dt for initialize codec variable*/
	s5m3700x_i2c_parse_dt(s5m3700x);

	s5m3700x->i2c_priv = i2c;
	i2c_set_clientdata(s5m3700x->i2c_priv, s5m3700x);

	s5m3700x->regmap = devm_regmap_init_i2c(i2c, &s5m3700x_regmap);
	if (IS_ERR(s5m3700x->regmap)) {
		dev_err(&i2c->dev, "Failed to allocate regmap: %li\n",
				PTR_ERR(s5m3700x->regmap));
		ret = -ENOMEM;
		goto err;
	}

	/* initialize workqueue */
	INIT_DELAYED_WORK(&s5m3700x->adc_mute_work, s5m3700x_adc_mute_work);
	s5m3700x->adc_mute_wq = create_singlethread_workqueue("adc_mute_wq");
	if (s5m3700x->adc_mute_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create adc_mute_wq\n");
		return -ENOMEM;
	}

	/* initialize mutex lock */
	mutex_init(&s5m3700x->regcache_lock);
	mutex_init(&s5m3700x->regsync_lock);
	mutex_init(&s5m3700x->mute_lock);
	mutex_init(&s5m3700x->hp_mbias_lock);

	/* register alsa component */
	ret = snd_soc_register_component(&i2c->dev, &soc_codec_dev_s5m3700x,
			s5m3700x_dai, 1);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register digital codec: %d\n", ret);
		goto err;
	}

#if IS_ENABLED(CONFIG_PM)
	if (s5m3700x->power_always_on)
		pm_runtime_disable(s5m3700x->dev);
	else
		pm_runtime_enable(s5m3700x->dev);
#endif
#ifdef CONFIG_DEBUG_FS
	s5m3700x_debug_init(s5m3700x);
#endif
	dev_info(&i2c->dev, "Codec I2C Probe: done\n");

	return ret;
err:
	kfree(s5m3700x);
	return ret;
}

static int s5m3700x_i2c_remove(struct i2c_client *i2c)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(&i2c->dev);

	dev_info(s5m3700x->dev, "(*) %s called\n", __func__);

	destroy_workqueue(s5m3700x->adc_mute_wq);
#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3700x->dev))
		pm_runtime_disable(s5m3700x->dev);
#endif
#ifdef CONFIG_DEBUG_FS
	s5m3700x_debug_remove(s5m3700x);
#endif
	snd_soc_unregister_component(&i2c->dev);
	kfree(s5m3700x);

	return 0;
}

int s5m3700x_codec_enable(struct device *dev)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);

	dev_info(dev, "%s : enter\n", __func__);
	mutex_lock(&s5m3700x->mute_lock);

	if (s5m3700x->codec_power_ref_cnt == 0) {
		/* Disable Cache Only Mode */
		s5m3700x_regcache_cache_only_switch(s5m3700x, false);
		/* Codec Power On */
		s5m3700x_codec_power_enable(s5m3700x, true);
		/* HW Register sync with cahce */
		s5m3700x_regmap_register_sync(s5m3700x);
	}

	s5m3700x->codec_power_ref_cnt++;

	mutex_unlock(&s5m3700x->mute_lock);
	dev_info(dev, "%s : exit\n", __func__);

	return 0;
}

int s5m3700x_codec_disable(struct device *dev)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);

	dev_info(dev, "%s : enter\n", __func__);
	
	mutex_lock(&s5m3700x->mute_lock);
	s5m3700x->codec_power_ref_cnt--;

	if (s5m3700x->codec_power_ref_cnt == 0) {
		if (s5m3700x->cache_only >= 1) {
			s5m3700x->cache_only = 1;
			s5m3700x_regcache_cache_only_switch(s5m3700x, false);
			dev_info(dev, "%s : force s5m3700x_regcache_cache_only_switch set false by codec disable\n", __func__);
		}
		/* Codec Power Off */
		s5m3700x_codec_power_enable(s5m3700x, false);
		/* Cache fall back to reg_defaults */
		s5m3700x_cache_register_sync_default(s5m3700x);
		/* Enable Cache Only Mode */
		s5m3700x_regcache_cache_only_switch(s5m3700x, true);
		/* set device status none */
		s5m3700x->status = DEVICE_NONE;
	}

	mutex_unlock(&s5m3700x->mute_lock);
	dev_info(dev, "%s : exit\n", __func__);

	return 0;
}

#if IS_ENABLED(CONFIG_PM) || IS_ENABLED(CONFIG_PM_SLEEP)
static int s5m3700x_resume(struct device *dev)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);
	if (!s5m3700x->power_always_on)
		s5m3700x_codec_enable(dev);

	return 0;
}

static int s5m3700x_suspend(struct device *dev)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);
	if (!s5m3700x->power_always_on)
		s5m3700x_codec_disable(dev);

	return 0;
}
#endif

static const struct dev_pm_ops s5m3700x_pm = {
#if IS_ENABLED(CONFIG_PM_SLEEP)
	SET_SYSTEM_SLEEP_PM_OPS(
			s5m3700x_suspend,
			s5m3700x_resume)
#endif
#if IS_ENABLED(CONFIG_PM)
	SET_RUNTIME_PM_OPS(
			s5m3700x_suspend,
			s5m3700x_resume,
			NULL)
#endif
};

static const struct i2c_device_id s5m3700x_i2c_id[] = {
	{ "s5m3700x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s5m3700x_i2c_id);

const struct of_device_id s5m3700x_of_match[] = {
	{ .compatible = "samsung,s5m3700x", },
	{ },
};
MODULE_DEVICE_TABLE(of, s5m3700x_of_match);

static struct i2c_driver s5m3700x_i2c_driver = {
	.driver = {
		.name = "s5m3700x",
		.owner = THIS_MODULE,
		.pm = &s5m3700x_pm,
		.of_match_table = of_match_ptr(s5m3700x_of_match),
	},
	.probe = s5m3700x_i2c_probe,
	.remove = s5m3700x_i2c_remove,
	.id_table = s5m3700x_i2c_id,
};

module_i2c_driver(s5m3700x_i2c_driver);

#ifdef CONFIG_DEBUG_FS

#define ADDR_SEPERATOR_STRING	"========================================================\n"
#define ADDR_INDEX_STRING		"      00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n"

static char *s5m3700x_get_register_block(unsigned int j)
{
	switch (j) {
	case 0:
		return "Digital Register Block\n";
	case 1:
		return "Analog Register Block\n";
	case 2:
		return "OTP Register Block\n";
	case 3:
		return "MV Register Block\n";
	}

	return "";
}

static ssize_t s5m3700x_debugfs_cache_write_file(struct file *file,
									const char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3700x_priv *s5m3700x = file->private_data;
	struct device *dev = s5m3700x->dev;

	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long reg, value;

	dev_info(dev, "%s : enter\n", __func__);

	buf_size = min(count, (sizeof(buf)-1));

	if (simple_write_to_buffer(buf, buf_size, ppos, user_buf, count) < 0)
		return -EFAULT;

	/* add zero at the end of data buf from user */
	buf[buf_size] = 0;

	/* extract register address */
	while (*start == ' ')
		start++;
	reg = simple_strtoul(start, &start, 16);

	/* extract register value */
	while (*start == ' ')
		start++;
	if (kstrtoul(start, 16, &value))
		return -EINVAL;

	ret = s5m3700x_write_only_cache(s5m3700x, reg, value);
	if (ret < 0)
		return ret;
	return buf_size;
}

static ssize_t s5m3700x_debugfs_cache_read_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3700x_priv *s5m3700x = file->private_data;
	struct device *dev = s5m3700x->dev;
	char *reg_dump;
	unsigned int value = 0, i = 0, j = 0, reg_dump_pos = 0, len = 0;

	dev_info(dev, "%s : enter\n", __func__);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
		count = PAGE_SIZE << (MAX_ORDER - 1);

	reg_dump = kmalloc(count, GFP_KERNEL);
	if (!reg_dump)
		return -ENOMEM;

	memset(reg_dump, 0, count);

	/* Register Block */
	for (j = 0; j < 4; j++) {
		/* copy address index at the top of stdout */
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "               %s", s5m3700x_get_register_block(j));
		if (len >= 0)
			reg_dump_pos += len;

		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, ADDR_SEPERATOR_STRING);
		if (len >= 0)
			reg_dump_pos += len;

		/* copy address index at the top of stdout */
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, ADDR_INDEX_STRING);
		if (len >= 0)
			reg_dump_pos += len;

		/* register address 0x00 to 0xFF of each block */
		for (i = 0; i <= 255 ; i++) {
			/* line numbers like 0x10, 0x20, 0x30 ... 0xff
			 * in the first colume
			 */
			if (i % 16 == 0) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "%04x:", j<<8|(i/16*16));
				if (len >= 0)
					reg_dump_pos += len;
			}

			/* read cache register value */
			ret = s5m3700x_read_only_cache(s5m3700x, j<<8|i, &value);
			if (ret == 0) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, " %02x", value);
				if (len >= 0)
					reg_dump_pos += len;
			} else {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, " 00");
				if (len >= 0)
					reg_dump_pos += len;
			}

			/* change line */
			if (i % 16 == 15) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\n");
				if (len >= 0)
					reg_dump_pos += len;
			}
		}
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\n");
		if (len >= 0)
			reg_dump_pos += len;
	}

	/*get string length */
	ret = reg_dump_pos;

	/*show stdout */
	ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, reg_dump_pos);

	kfree(reg_dump);
	return ret;
}

static const struct file_operations s5m3700x_debugfs_cache_register_fops = {
	.open = simple_open,
	.read = s5m3700x_debugfs_cache_read_file,
	.write = s5m3700x_debugfs_cache_write_file,
	.llseek = default_llseek,
};

static ssize_t s5m3700x_debugfs_hw_write_file(struct file *file,
									const char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3700x_priv *s5m3700x = file->private_data;
	struct device *dev = s5m3700x->dev;

	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long reg, value;

	dev_info(dev, "%s : enter\n", __func__);

	buf_size = min(count, (sizeof(buf)-1));

	if (simple_write_to_buffer(buf, buf_size, ppos, user_buf, count) < 0)
		return -EFAULT;

	/* add zero at the end of data buf from user */
	buf[buf_size] = 0;

	/* extract register address */
	while (*start == ' ')
		start++;
	reg = simple_strtoul(start, &start, 16);

	/* extract register value */
	while (*start == ' ')
		start++;
	if (kstrtoul(start, 16, &value))
		return -EINVAL;

	ret = s5m3700x_write_only_hardware(s5m3700x, reg, value);
	if (ret < 0)
		return ret;
	return buf_size;
}

static ssize_t s5m3700x_debugfs_hw_read_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3700x_priv *s5m3700x = file->private_data;
	struct device *dev = s5m3700x->dev;
	char *reg_dump;
	unsigned int value = 0, i = 0, j = 0, reg_dump_pos = 0, len = 0;

	dev_info(dev, "%s : enter\n", __func__);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
		count = PAGE_SIZE << (MAX_ORDER - 1);

	reg_dump = kmalloc(count, GFP_KERNEL);
	if (!reg_dump)
		return -ENOMEM;

	memset(reg_dump, 0, count);

	/* Register Block */
	for (j = 0; j < 4; j++) {
		/* copy address index at the top of stdout */
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "                  %s", s5m3700x_get_register_block(j));
		if (len >= 0)
			reg_dump_pos += len;

		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, ADDR_SEPERATOR_STRING);
		if (len >= 0)
			reg_dump_pos += len;

		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, ADDR_INDEX_STRING);
		if (len >= 0)
			reg_dump_pos += len;

		/* register address 0x00 to 0xFF of each block */
		for (i = 0; i <= 255 ; i++) {
			/* line numbers like 0x10, 0x20, 0x30 ... 0xff
			 * in the first colume
			 */
			if (i % 16 == 0) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "%04x:", j<<8|(i/16*16));
				if (len >= 0)
					reg_dump_pos += len;
			}

			/* read HW register value */
			ret = s5m3700x_read_only_hardware(s5m3700x, j<<8|i, &value);
			if (ret == 0) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, " %02x", value);
				if (len >= 0)
					reg_dump_pos += len;
			} else {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, " 00");
				if (len >= 0)
					reg_dump_pos += len;
			}

			/* change line */
			if (i % 16 == 15) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\n");
				if (len >= 0)
					reg_dump_pos += len;
			}
		}
		/* change line */
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\n");
		if (len >= 0)
			reg_dump_pos += len;
	}

	/*get string length */
	ret = reg_dump_pos;

	/*show stdout */
	ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, reg_dump_pos);

	kfree(reg_dump);
	return ret;
}

static const struct file_operations s5m3700x_debugfs_hw_register_fops = {
	.open = simple_open,
	.read = s5m3700x_debugfs_hw_read_file,
	.write = s5m3700x_debugfs_hw_write_file,
	.llseek = default_llseek,
};

static ssize_t s5m3700x_debugfs_cache_only_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3700x_priv *s5m3700x = file->private_data;
	struct device *dev = s5m3700x->dev;
	char *reg_dump;
	unsigned int reg_dump_pos = 0, len = 0;

	dev_info(dev, "%s : enter\n", __func__);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
		count = PAGE_SIZE << (MAX_ORDER - 1);

	reg_dump = kmalloc(count, GFP_KERNEL);

	if (!reg_dump)
		return -ENOMEM;

	memset(reg_dump, 0, count);

	len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\nCache only is %s.\n\n",
					is_cache_only_enabled(s5m3700x) > 0 ? "Enabled" : "Disabled");
	if (len >= 0)
		reg_dump_pos += len;

	/*get string length */
	ret = reg_dump_pos;

	/*show stdout */
	ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, reg_dump_pos);

	kfree(reg_dump);
	return ret;
}

static const struct file_operations s5m3700x_debugfs_cache_only_fops = {
	.open = simple_open,
	.read = s5m3700x_debugfs_cache_only_file,
	.llseek = default_llseek,
};

static ssize_t s5m3700x_debugfs_cache_bypass_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3700x_priv *s5m3700x = file->private_data;
	struct device *dev = s5m3700x->dev;
	char *reg_dump;
	unsigned int reg_dump_pos = 0, len = 0;

	dev_info(dev, "%s : enter\n", __func__);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
		count = PAGE_SIZE << (MAX_ORDER - 1);

	reg_dump = kmalloc(count, GFP_KERNEL);

	if (!reg_dump)
		return -ENOMEM;

	memset(reg_dump, 0, count);

	len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\nCache bypass is %s.\n\n",
					is_cache_bypass_enabled(s5m3700x) > 0 ? "Enabled" : "Disabled");
	if (len >= 0)
		reg_dump_pos += len;

	/*get string length */
	ret = reg_dump_pos;

	/*show stdout */
	ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, reg_dump_pos);

	kfree(reg_dump);
	return ret;
}

static const struct file_operations s5m3700x_debugfs_cache_bypass_fops = {
	.open = simple_open,
	.read = s5m3700x_debugfs_cache_bypass_file,
	.llseek = default_llseek,
};

static void s5m3700x_debug_init(struct s5m3700x_priv *s5m3700x)
{
	struct device *dev = s5m3700x->dev;

	if (s5m3700x->dbg_root) {
		dev_err(dev, "%s : debugfs is already exist\n", __func__);
		return;
	}

	s5m3700x->dbg_root = debugfs_create_dir("s5m3700x", NULL);
	if (!s5m3700x->dbg_root) {
		dev_err(dev, "%s : cannot creat debugfs dir\n", __func__);
		return;
	}
	debugfs_create_file("cache_registers", S_IRUGO|S_IWUGO, s5m3700x->dbg_root,
			s5m3700x, &s5m3700x_debugfs_cache_register_fops);
	debugfs_create_file("hw_registers", S_IRUGO|S_IWUGO, s5m3700x->dbg_root,
			s5m3700x, &s5m3700x_debugfs_hw_register_fops);
	debugfs_create_file("cache_only", S_IRUGO|S_IWUGO, s5m3700x->dbg_root,
			s5m3700x, &s5m3700x_debugfs_cache_only_fops);
	debugfs_create_file("cache_bypass", S_IRUGO|S_IWUGO, s5m3700x->dbg_root,
			s5m3700x, &s5m3700x_debugfs_cache_bypass_fops);
}

static void s5m3700x_debug_remove(struct s5m3700x_priv *s5m3700x)
{
	if (s5m3700x->dbg_root)
		debugfs_remove_recursive(s5m3700x->dbg_root);
}
#endif //CONFIG_DEBUG_FS

MODULE_SOFTDEP("pre: snd-soc-samsung-vts");
MODULE_DESCRIPTION("ASoC S5M3700X driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:S5M3700X-codec");

