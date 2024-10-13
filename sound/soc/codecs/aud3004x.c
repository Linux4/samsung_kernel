/*
 * sound/soc/codec/aud3004x.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2022 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/samsung/abox.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/completion.h>
#include <sound/aud3004x.h>
#include <uapi/linux/input-event-codes.h>
#include <soc/samsung/acpm_mfd.h>
#include "../samsung/abox/abox.h"

#include "aud3004x.h"

#if IS_ENABLED(CONFIG_SND_SOC_AUD3004X_5PIN)
#include "aud3004x-5pin.h"
#endif

#if IS_ENABLED(CONFIG_SND_SOC_AUD3004X_6PIN)
#include "aud3004x-6pin.h"
#endif

#define AUD3004X_CODEC_VER 2001

static struct device_node *codec_np;

void aud3004x_usleep(unsigned int u_sec)
{
	usleep_range(u_sec, u_sec + 10);
}

int aud3004x_acpm_read_reg(unsigned int slave, unsigned int reg,
		unsigned int *val)
{
	int ret;
	u8 reg_val;

	ret = exynos_acpm_read_reg(codec_np, 0, slave, reg, &reg_val);
	*val = reg_val;

	if (ret)
		pr_err("[%s] acpm ipc read failed! err: %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(aud3004x_acpm_read_reg);

int aud3004x_acpm_write_reg(unsigned int slave, unsigned int reg,
		unsigned int val)
{
	int ret;

	ret = exynos_acpm_write_reg(codec_np, 0, slave, reg, val);

	if (ret)
		pr_err("[%s] acpm ipc write failed! err: %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(aud3004x_acpm_write_reg);

int aud3004x_acpm_update_reg(unsigned int slave, unsigned int reg,
		unsigned int val, unsigned int mask)
{
	int ret;

	ret = exynos_acpm_update_reg(codec_np, 0, slave, reg, val, mask);

	if (ret)
		pr_err("[%s] acpm ipc update failed! err: %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(aud3004x_acpm_update_reg);

void aud3004x_regcache_switch(struct aud3004x_priv *aud3004x, bool on)
{
	int p_map;

	if (!on)
		mutex_lock(&aud3004x->regcache_lock);
	else
		mutex_unlock(&aud3004x->regcache_lock);

	if (aud3004x->is_suspend)
		for (p_map = AUD3004D; p_map <= AUD3004O; p_map++)
			regcache_cache_only(aud3004x->regmap[p_map], on);
}

static int i2c_client_return(unsigned int reg)
{
	switch (reg) {
	case 0x00 ... 0xFF:
		return AUD3004D;
	case 0x100 ... 0x1FF:
		return AUD3004A;
	case 0x200 ... 0x2FF:
		return AUD3004O;
	default:
		return CODEC_CLOSE;
	}
}

int aud3004x_read(struct aud3004x_priv *aud3004x, unsigned int slave,
		  unsigned int addr, unsigned int *value)
{
	int ret;

	switch (slave) {
	case AUD3004X_DIGITAL_ADDR:
		ret = regmap_read(aud3004x->regmap[AUD3004D], addr, value);
		break;
	case AUD3004X_ANALOG_ADDR:
		ret = regmap_read(aud3004x->regmap[AUD3004A], addr, value);
		break;
	case AUD3004X_OTP_ADDR:
		ret = regmap_read(aud3004x->regmap[AUD3004O], addr, value);
		break;
	default:
		dev_err(aud3004x->dev, "%s: Cannot find slave address. addr: 0x%02X%02X\n",
				__func__, slave, addr);
		ret = -1;
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(aud3004x_read);

int aud3004x_write(struct aud3004x_priv *aud3004x, unsigned int slave,
		   unsigned int addr, unsigned int value)
{
	int ret;

	switch (slave) {
	case AUD3004X_DIGITAL_ADDR:
		ret = regmap_write(aud3004x->regmap[AUD3004D], addr, value);
		break;
	case AUD3004X_ANALOG_ADDR:
		ret = regmap_write(aud3004x->regmap[AUD3004A], addr, value);
		break;
	case AUD3004X_OTP_ADDR:
		ret = regmap_write(aud3004x->regmap[AUD3004O], addr, value);
		break;
	default:
		dev_err(aud3004x->dev, "%s: Cannot find slave address. addr: 0x%02X%02X\n",
				__func__, slave, addr);
		ret = -1;
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(aud3004x_write);

int aud3004x_update_bits(struct aud3004x_priv *aud3004x, unsigned int slave,
			 unsigned int addr, unsigned int mask, unsigned int value)
{
	int ret;

	switch (slave) {
	case AUD3004X_DIGITAL_ADDR:
		ret = regmap_update_bits(aud3004x->regmap[AUD3004D], addr, mask, value);
		break;
	case AUD3004X_ANALOG_ADDR:
		ret = regmap_update_bits(aud3004x->regmap[AUD3004A], addr, mask, value);
		break;
	case AUD3004X_OTP_ADDR:
		ret = regmap_update_bits(aud3004x->regmap[AUD3004O], addr, mask, value);
		break;
	default:
		dev_err(aud3004x->dev, "%s: Cannot find slave address. addr: 0x%02X%02X\n",
				__func__, slave, addr);
		ret = -1;
		break;
	}
	return ret;

}
EXPORT_SYMBOL_GPL(aud3004x_update_bits);

/*
 * Return Value
 * True: If the register value cannot be cached, hence we have to read from the
 * hardware directly.
 * False: If the register value can be read from cache.
 */
static bool aud3004x_volatile_register(struct device *dev, unsigned int reg)
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
	case 0x01 ... 0x06:
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
static bool aud3004x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x08 ... 0x0F:
	case 0x10 ... 0x29:
	case 0x2F ... 0x4E:
	case 0x50 ... 0x86:
	case 0x88 ... 0x8E:
	case 0x90 ... 0x9E:
	case 0xA0 ... 0xFC:
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
static bool aud3004x_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x08 ... 0x0D:
	case 0x10 ... 0x25:
	case 0x2F ... 0x4E:
	case 0x50 ... 0x56:
	case 0x58 ... 0x74:
	case 0x77 ... 0x7F:
	case 0x88 ... 0x8E:
	case 0x90 ... 0x97:
	case 0x9B ... 0x9E:
	case 0xA0 ... 0xEF:
		return true;
	default:
		return false;
	}
}

static int aud3004x_dreg_hw_write(void *context, unsigned int reg,
				  unsigned int value)
{
	return aud3004x_acpm_write_reg(AUD3004X_DIGITAL_ADDR, reg, value);
}

static int aud3004x_dreg_hw_read(void *context, unsigned int reg,
				 unsigned int *value)
{
	return aud3004x_acpm_read_reg(AUD3004X_DIGITAL_ADDR, reg, value);
}

static const struct regmap_bus aud3004x_dbus = {
	.reg_write = aud3004x_dreg_hw_write,
	.reg_read = aud3004x_dreg_hw_read,
};

const struct regmap_config aud3004x_dregmap = {
	.reg_bits = 8,
	.val_bits = 8,

	/*
	 * "i3c" string should be described in the name field
	 * this will be used for the i3c inteface,
	 * when read/write operations are used in the regmap driver.
	 * APM functions will be called instead of the I2C
	 * refer to the "drivers/base/regmap/regmap-i2c.c
	 */
	.name = "i3c, AUD3004X",
	.max_register = AUD3004X_REGCACHE_SYNC_END,
	.readable_reg = aud3004x_readable_register,
	.writeable_reg = aud3004x_writeable_register,
	.volatile_reg = aud3004x_volatile_register,
	.use_single_read = true,
	.use_single_write = true,
	.cache_type = REGCACHE_RBTREE,
};

static bool aud3004x_volatile_analog_register(struct device *dev, unsigned int reg)
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
	default:
		return false;
	}
}

static bool aud3004x_readable_analog_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00 ... 0x1F:
	case 0x30 ... 0x38:
	case 0x50 ... 0x62:
	case 0x70 ... 0x8C:
	case 0x90 ... 0xA3:
		return true;
	default:
		return false;
	}
}

static bool aud3004x_writeable_analog_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00 ... 0x1F:
	case 0x30 ... 0x38:
	case 0x50 ... 0x62:
	case 0x70 ... 0x79:
	case 0x90 ... 0xA3:
		return true;
	default:
		return false;
	}
}

static int aud3004x_areg_hw_write(void *context, unsigned int reg,
				  unsigned int value)
{
	return aud3004x_acpm_write_reg(AUD3004X_ANALOG_ADDR, reg, value);
}

static int aud3004x_areg_hw_read(void *context, unsigned int reg,
				 unsigned int *value)
{
	return aud3004x_acpm_read_reg(AUD3004X_ANALOG_ADDR, reg, value);
}

static const struct regmap_bus aud3004x_abus = {
	.reg_write = aud3004x_areg_hw_write,
	.reg_read = aud3004x_areg_hw_read,
};

const struct regmap_config aud3004x_aregmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.name = "i3c, AUD3004X_ANALOG",
	.max_register = AUD3004X_REGCACHE_SYNC_END,
	.readable_reg = aud3004x_readable_analog_register,
	.writeable_reg = aud3004x_writeable_analog_register,
	.volatile_reg = aud3004x_volatile_analog_register,
	.use_single_read = true,
	.use_single_write = true,
	.cache_type = REGCACHE_RBTREE,
};

static bool aud3004x_volatile_otp_register(struct device *dev, unsigned int reg)
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
	default:
		return false;
	}
}

static bool aud3004x_readable_otp_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00 ... 0xB1:
		return true;
	default:
		return false;
	}
}

static bool aud3004x_writeable_otp_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00 ... 0xB1:
		return true;
	default:
		return false;
	}
}

static int aud3004x_oreg_hw_write(void *context, unsigned int reg,
				  unsigned int value)
{
	return aud3004x_acpm_write_reg(AUD3004X_OTP_ADDR, reg, value);
}

static int aud3004x_oreg_hw_read(void *context, unsigned int reg,
				 unsigned int *value)
{
	return aud3004x_acpm_read_reg(AUD3004X_OTP_ADDR, reg, value);
}

static const struct regmap_bus aud3004x_obus = {
	.reg_write = aud3004x_oreg_hw_write,
	.reg_read = aud3004x_oreg_hw_read,
};

const struct regmap_config aud3004x_oregmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.name = "i3c, AUD3004X_OTP",
	.max_register = AUD3004X_REGCACHE_SYNC_END,
	.readable_reg = aud3004x_readable_otp_register,
	.writeable_reg = aud3004x_writeable_otp_register,
	.volatile_reg = aud3004x_volatile_otp_register,
	.use_single_read = true,
	.use_single_write = true,
	.cache_type = REGCACHE_RBTREE,
};

bool read_from_cache(struct device *dev, unsigned int reg, int map_type)
{
	bool result = false;

	switch (map_type) {
	case AUD3004D:
		result = aud3004x_readable_register(dev, reg) &&
			(!aud3004x_volatile_register(dev, reg));
		break;
	case AUD3004A:
		result = aud3004x_readable_analog_register(dev, reg) &&
			(!aud3004x_volatile_analog_register(dev, reg));
		break;
	case AUD3004O:
		result = aud3004x_readable_otp_register(dev, reg) &&
			(!aud3004x_volatile_otp_register(dev, reg));
		break;
	}

	return result;
}
EXPORT_SYMBOL_GPL(read_from_cache);

bool write_to_hw(struct device *dev, unsigned int reg, int map_type)
{
	bool result = false;

	switch (map_type) {
	case AUD3004D:
		result = aud3004x_writeable_register(dev, reg) &&
			(!aud3004x_volatile_register(dev, reg));
		break;
	case AUD3004A:
		result = aud3004x_writeable_analog_register(dev, reg) &&
			(!aud3004x_volatile_analog_register(dev, reg));
		break;
	case AUD3004O:
		result = aud3004x_writeable_otp_register(dev, reg) &&
			(!aud3004x_volatile_otp_register(dev, reg));
		break;
	}

	return result;
}

/*
 * TLV_DB_SCALE_ITEM (TLV: Threshold Limit Value)
 *
 * For various properties, the dB values don't change linearly with respect to
 * the digital value of related bit-field. At most, they are quasi-linear,
 * that means they are linear for various ranges of digital values. Following
 * table define such ranges of various properties.
 *
 * TLV_DB_RANGE_HEAD(num)
 * num defines the number of linear ranges of dB values.
 *
 * s0, e0, TLV_DB_SCALE_ITEM(min, step, mute),
 * s0: digital start value of this range (inclusive)
 * e0: digital end valeu of this range (inclusive)
 * min: dB value corresponding to s0
 * step: the delta of dB value in this range
 * mute: ?
 *
 * Example:
 *	TLV_DB_RANGE_HEAD(3),
 *	0, 1, TLV_DB_SCALE_ITEM(-2000, 2000, 0),
 *	2, 4, TLV_DB_SCALE_ITEM(1000, 1000, 0),
 *	5, 6, TLV_DB_SCALE_ITEM(3800, 8000, 0),
 *
 * The above code has 3 linear ranges with following digital-dB mapping.
 * (0...6) -> (-2000dB, 0dB, 1000dB, 2000dB, 3000dB, 3800dB, 4600dB),
 *
 * DECLARE_TLV_DB_SCALE
 *
 * This macro is used in case where there is a linear mapping between
 * the digital value and dB value.
 *
 * DECLARE_TLV_DB_SCALE(name, min, step, mute)
 *
 * name: name of this dB scale
 * min: minimum dB value corresponding to digital 0
 * step: the delta of dB value
 * mute: ?
 *
 * NOTE: The information is mostly for user-space consumption, to be viewed
 * alongwith amixer.
 */

/*
 * aud3004x_dvol_adc_tlv - Digital volume for ADC
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
static const unsigned int aud3004x_dvol_adc_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x00, 0x05, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x06, 0xE5, TLV_DB_SCALE_ITEM(-6950, 50, 0),
};

/*
 * aud3004x_dvol_dmic_tlv - Digital MIC Gain Control
 *
 * 0x00 ~ 0x07 : 1 to 15, step 2 Level
 *
 */
static const unsigned int aud3004x_dvol_dmic_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x00, 0x07, TLV_DB_SCALE_ITEM(1, 2, 0),
};
static int dmic_bias_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int bias_flag;

	if (aud3004x->dmic_bias_gpio < 0)
		return 0;
	bias_flag = gpio_get_value(aud3004x->dmic_bias_gpio);
	dev_info(aud3004x->dev, "aud3004x %s, dmic bias gpio: %d\n", __func__, bias_flag);

	if (bias_flag < 0)
		dev_err(aud3004x->dev, "%s, dmic bias gpio read error!\n", __func__);
	else
		ucontrol->value.integer.value[0] = bias_flag;

	return 0;
}

static int dmic_bias_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (gpio_is_valid(aud3004x->dmic_bias_gpio)) {
		gpio_direction_output(aud3004x->dmic_bias_gpio, value);

		dev_info(aud3004x->dev, "aud3004x %s: dmic bias : %s\n", __func__, (value) ? "On" : "Off");
	} else {
		dev_err(aud3004x->dev, "aud3004x %s, dmic bias gpio not valid!\n", __func__);
	}

	return 0;
}

static int amic_bias_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int amic_bias_flag;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_110_PD_REF, &amic_bias_flag);
	amic_bias_flag &= PDB_MCB_LDO_CODEC_MASK;

	ucontrol->value.integer.value[0] = amic_bias_flag;

	return 0;
}

static int amic_bias_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	/* PDB_VMID is moved vmid_ev, cause EP Multi sequence */
	if (value) {
		/* T_PDB_VMID ON */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B5_ODSEL0,
				T_PDB_VMID_MASK, T_PDB_VMID_MASK);

		/* PDB_VMID ON */
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_110_PD_REF,
				PDB_VMID_MASK, PDB_VMID_MASK);
		/* MICBIAS1 ON */
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_110_PD_REF,
				PDB_MCB_LDO_CODEC_MASK | PDB_MCB1_MASK,
				PDB_MCB_LDO_CODEC_MASK | PDB_MCB1_MASK);
	} else {
		/* MICBIAS1 OFF */
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_110_PD_REF,
				PDB_MCB_LDO_CODEC_MASK | PDB_MCB1_MASK, 0);
	}
	return 0;
}

static int amic_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int amic_mute;

	amic_mute = aud3004x->amic_mute;

	ucontrol->value.integer.value[0] = amic_mute;
	return 0;
}

static int amic_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int amic_mute = ucontrol->value.integer.value[0];

	aud3004x->amic_mute = amic_mute;
	return 0;
}

static int dmic_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int dmic_mute;

	dmic_mute = aud3004x->dmic_mute;

	ucontrol->value.integer.value[0] = dmic_mute;
	return 0;
}

static int dmic_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int dmic_mute = ucontrol->value.integer.value[0];

	aud3004x->dmic_mute = dmic_mute;
	return 0;
}

static int ovp_on_setting_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_on_setting;

	ovp_on_setting = aud3004x->ovp_on_setting;

	ucontrol->value.integer.value[0] = ovp_on_setting;
	return 0;
}

static int ovp_on_setting_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_on_setting = ucontrol->value.integer.value[0];

	aud3004x->ovp_on_setting = ovp_on_setting;
	return 0;
}

static int ovp_refp_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_refp;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_15F_OVP1, &ovp_refp);
	ovp_refp &= CTRV_SURGE_REFP_MASK;

	ucontrol->value.integer.value[0] = ovp_refp >> CTRV_SURGE_REFP_SHIFT;
	return 0;
}

static int ovp_refp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_refp = ucontrol->value.integer.value[0];

	if (ovp_refp < SURGE_REFP_MINNUM)
		ovp_refp = SURGE_REFP_MINNUM;
	else if (ovp_refp > SURGE_REFP_MAXNUM)
		ovp_refp = SURGE_REFP_MAXNUM;

	aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_15F_OVP1,
			CTRV_SURGE_REFP_MASK, ovp_refp << CTRV_SURGE_REFP_SHIFT);
	return 0;
}

static int ovp_refn_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_refn;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_15F_OVP1, &ovp_refn);
	ovp_refn &= CTRV_SURGE_REFN_MASK;

	ucontrol->value.integer.value[0] = ovp_refn >> CTRV_SURGE_REFN_SHIFT;
	return 0;
}

static int ovp_refn_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_refn = ucontrol->value.integer.value[0];

	if (ovp_refn < SURGE_REFN_MINNUM)
		ovp_refn = SURGE_REFN_MINNUM;
	else if (ovp_refn > SURGE_REFN_MAXNUM)
		ovp_refn = SURGE_REFN_MAXNUM;

	aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_15F_OVP1,
			CTRV_SURGE_REFN_MASK, ovp_refn << CTRV_SURGE_REFN_SHIFT);
	return 0;
}

static int ovp_time_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_time;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2, &ovp_time);
	ovp_time &= OVP_TIME_SLOT_MASK;

	ucontrol->value.integer.value[0] = ovp_time >> OVP_TIME_SLOT_SHIFT;
	return 0;
}

static int ovp_time_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_time = ucontrol->value.integer.value[0];

	if (ovp_time < SURGE_TIME_MINNUM)
		ovp_time = SURGE_TIME_MINNUM;
	else if (ovp_time > SURGE_TIME_MAXNUM)
		ovp_time = SURGE_TIME_MAXNUM;

	aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2,
			OVP_TIME_SLOT_MASK, ovp_time << OVP_TIME_SLOT_SHIFT);

	switch (ovp_time) {
	case 0:
		aud3004x->ovp_det_delay = 2000;
		break;
	case 1:
		aud3004x->ovp_det_delay = 1000;
		break;
	case 2:
		aud3004x->ovp_det_delay = 1500;
		break;
	case 3:
		aud3004x->ovp_det_delay = 3000;
		break;
	default:
		aud3004x->ovp_det_delay = 2000;
		break;
	}
	return 0;
}

static int ovp_mask_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_mask_flag;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2, &ovp_mask_flag);
	ovp_mask_flag &= OVP_MASK_MASK;

	ucontrol->value.integer.value[0] = ovp_mask_flag >> OVP_MASK_SHIFT;
	return 0;
}

static int ovp_mask_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_mask_flag = ucontrol->value.integer.value[0];

	if (ovp_mask_flag)
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2,
				OVP_MASK_MASK, OVP_MASK_MASK);
	else
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2,
				OVP_MASK_MASK, 0);
	return 0;
}

static int ovp_apon_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_apon_flag;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2, &ovp_apon_flag);
	ovp_apon_flag &= OVP_APON_MASK;

	ucontrol->value.integer.value[0] = ovp_apon_flag;
	return 0;
}

static int ovp_apon_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_apon_flag = ucontrol->value.integer.value[0];

	if (ovp_apon_flag)
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2,
				OVP_APON_MASK, OVP_APON_MASK);
	else
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2,
				OVP_APON_MASK, 0);
	return 0;
}

static int ovp_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_mute_flag;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2, &ovp_mute_flag);
	ovp_mute_flag &= OVP_UNMUTE_MASK;

	/* To Reverse due to naming unmute */
	ucontrol->value.integer.value[0] = !ovp_mute_flag;
	return 0;
}

static int ovp_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ovp_mute_flag = ucontrol->value.integer.value[0];

	/* To Reverse due to naming unmute */
	if (ovp_mute_flag)
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2,
				OVP_UNMUTE_MASK, 0);
	else
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2,
				OVP_UNMUTE_MASK, OVP_UNMUTE_MASK);
	return 0;
}

static int amic1_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int val;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_134_VOL_AD1, &val);
	val &= CTVOL_BST1_MASK;

	ucontrol->value.integer.value[0] = val >> CTVOL_BST1_SHIFT;
	return 0;
}

static int amic1_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	switch (value) {
	case CTVOL_ALOG_GAIN_0:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_134_VOL_AD1,
				CTVOL_BST1_MASK, CTVOL_ALOG_GAIN_0 << CTVOL_BST1_SHIFT);
		break;
	case CTVOL_ALOG_GAIN_12:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_134_VOL_AD1,
				CTVOL_BST1_MASK, CTVOL_ALOG_GAIN_12 << CTVOL_BST1_SHIFT);
		break;
	case CTVOL_ALOG_GAIN_20:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_134_VOL_AD1,
				CTVOL_BST1_MASK, CTVOL_ALOG_GAIN_20 << CTVOL_BST1_SHIFT);
		break;
	default:
		dev_info(codec->dev, "aud3004x %s called, Not supported value: %d\n", __func__, value);
		break;
	}
	return 0;
}

static int amic2_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int value;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_135_VOL_AD2, &value);
	value &= CTVOL_BST2_MASK;

	ucontrol->value.integer.value[0] = value >> CTVOL_BST2_SHIFT;
	return 0;
}

static int amic2_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	switch (value) {
	case CTVOL_ALOG_GAIN_0:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_135_VOL_AD2,
				CTVOL_BST2_MASK, CTVOL_ALOG_GAIN_0 << CTVOL_BST2_SHIFT);
		break;
	case CTVOL_ALOG_GAIN_12:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_135_VOL_AD2,
				CTVOL_BST2_MASK, CTVOL_ALOG_GAIN_12 << CTVOL_BST2_SHIFT);
		break;
	case CTVOL_ALOG_GAIN_20:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_135_VOL_AD2,
				CTVOL_BST2_MASK, CTVOL_ALOG_GAIN_20 << CTVOL_BST2_SHIFT);
		break;
	default:
		dev_info(codec->dev, "aud3004x %s called, Not supported value: %d\n", __func__, value);
		break;
	}
	return 0;
}

static int amic3_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int value;

	aud3004x_read(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_136_VOL_AD3, &value);
	value &= CTVOL_BST3_MASK;

	ucontrol->value.integer.value[0] = value >> CTVOL_BST3_SHIFT;
	return 0;
}

static int amic3_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	switch (value) {
	case CTVOL_ALOG_GAIN_0:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_136_VOL_AD3,
				CTVOL_BST3_MASK, CTVOL_ALOG_GAIN_0 << CTVOL_BST3_SHIFT);
		break;
	case CTVOL_ALOG_GAIN_12:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_136_VOL_AD3,
				CTVOL_BST3_MASK, CTVOL_ALOG_GAIN_12 << CTVOL_BST3_SHIFT);
		break;
	case CTVOL_ALOG_GAIN_20:
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_136_VOL_AD3,
				CTVOL_BST3_MASK, CTVOL_ALOG_GAIN_20 << CTVOL_BST3_SHIFT);
		break;
	default:
		dev_info(codec->dev, "aud3004x %s called, Not supported value: %d\n", __func__, value);
		break;
	}
	return 0;
}
/*
 * aud3004x_adc_dat_src - I2S channel input data selection
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
static const char * const aud3004x_adc_dat_src[] = {
		"ADCL", "ADCR", "Zero1", "Zero2"
};

static SOC_ENUM_SINGLE_DECL(aud3004x_adc_dat_enum0, AUD3004X_23_IF_FORM4,
		SEL_ADC0_SHIFT, aud3004x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(aud3004x_adc_dat_enum1,	AUD3004X_23_IF_FORM4,
		SEL_ADC1_SHIFT, aud3004x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(aud3004x_adc_dat_enum2,	AUD3004X_23_IF_FORM4,
		SEL_ADC2_SHIFT, aud3004x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(aud3004x_adc_dat_enum3, AUD3004X_23_IF_FORM4,
		SEL_ADC3_SHIFT, aud3004x_adc_dat_src);

/*
 * aud3004x_dvol_dac_tlv - Maximum headphone gain for EAR/RCV path
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
static const unsigned int aud3004x_dvol_dac_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x01, 0x03, TLV_DB_SCALE_ITEM(-9630, 600, 0),
	0x04, 0x04, TLV_DB_SCALE_ITEM(-8240, 0, 0),
	0x05, 0x09, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x0A, 0xE9, TLV_DB_SCALE_ITEM(-7000, 50, 0),
};

/*
 * aud3004x_dac_mixl_mode_text - DACL Mixer Selection
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
static const char * const aud3004x_dac_center_mixer_text[] = {
		"Data L", "LR/2 PolCh", "Reserv1", "LR/2",
		"Reserv2", "Data R", "Zero1", "Zero2"
};

static SOC_ENUM_SINGLE_DECL(aud3004x_dac_center_mixer_enum,
		AUD3004X_45_PLAY_MIX1, DAC_MIXC_SHIFT, aud3004x_dac_center_mixer_text);

static int codec_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = !(aud3004x->is_suspend);

	return 0;
}

static int codec_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	int value = ucontrol->value.integer.value[0];

	if (value)
		aud3004x_enable(codec->dev);
	else
		aud3004x_disable(codec->dev);

	dev_info(codec->dev, "aud3004x %s: codec enable : %s\n", __func__, (value) ? "On" : "Off");

	return 0;
}

static int avc_mute_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int avc13;
	bool avc_mute_flag;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_5C_AVC13, &avc13);
	avc_mute_flag = avc13 & AMUTE_MASKB_MASK;

	ucontrol->value.integer.value[0] = !avc_mute_flag;

	return 0;
}

static int avc_mute_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (!!value)
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_5C_AVC13,
				AMUTE_MASKB_MASK, 0);
	else
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_5C_AVC13,
				AMUTE_MASKB_MASK, AMUTE_MASKB_MASK);

	return 0;
}

static const char * const aud3004x_hpf_sel_text[] = {
	"15Hz", "33Hz", "60Hz", "113Hz"
};

static const char * const aud3004x_hpf_order_text[] = {
	"2nd", "1st"
};

static const char * const aud3004x_hpf_ch_text[] = {
	"Off", "On"
};

static const char * const aud3004x_gdet_vth_text[] = {
	"0.675V", "0.8V", "0.9V", "0.982V", "1.05V",
	"1.11V", "1.16V", "1.2V"
};

static const char * const aud3004x_ldet_vth_text[] = {
	"1.63V", "1.57V", "1.24V", "0.90V"
};

static SOC_ENUM_SINGLE_DECL(aud3004x_hpf_sel_enum, AUD3004X_37_AD_HPF,
		HPF_SEL_SHIFT, aud3004x_hpf_sel_text);

static SOC_ENUM_SINGLE_DECL(aud3004x_hpf_order_enum, AUD3004X_37_AD_HPF,
		HPF_ORDER_SHIFT, aud3004x_hpf_order_text);

static SOC_ENUM_SINGLE_DECL(aud3004x_hpf_ch_enum_l, AUD3004X_37_AD_HPF,
		HPF_ENL_SHIFT, aud3004x_hpf_ch_text);

static SOC_ENUM_SINGLE_DECL(aud3004x_hpf_ch_enum_r, AUD3004X_37_AD_HPF,
		HPF_ENR_SHIFT, aud3004x_hpf_ch_text);

static SOC_ENUM_SINGLE_DECL(aud3004x_hpf_ch_enum_c, AUD3004X_37_AD_HPF,
		HPF_ENC_SHIFT, aud3004x_hpf_ch_text);

static SOC_ENUM_SINGLE_DECL(aud3004x_gdet_vth_enum, AUD3004X_C0_ACTR_JD1,
		CTRV_GDET_VTG_SHIFT, aud3004x_gdet_vth_text);

static SOC_ENUM_SINGLE_DECL(aud3004x_ldet_vth_enum, AUD3004X_C1_ACTR_JD2,
		CTRV_LDET_VTH_SHIFT, aud3004x_ldet_vth_text);

/*
 * struct snd_kcontrol_new aud3004x_snd_control
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
 * All the data goes into aud3004x_snd_controls.
 * All path inter-connections goes into aud3004x_dapm_routes
 */
static const struct snd_kcontrol_new aud3004x_snd_controls[] = {
	/*
	 * ADC(Tx) path control
	 */
	SOC_SINGLE_TLV("ADC Left Gain", AUD3004X_34_AD_VOLL,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, aud3004x_dvol_adc_tlv),

	SOC_SINGLE_TLV("ADC Right Gain", AUD3004X_35_AD_VOLR,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, aud3004x_dvol_adc_tlv),

	SOC_SINGLE_TLV("DMIC Gain PRE", AUD3004X_32_ADC3,
			DMIC_GAIN_PRE_SHIFT,
			DMIC_GAIN_15, 0, aud3004x_dvol_dmic_tlv),

	SOC_SINGLE_EXT("MAIN MIC Gain", SND_SOC_NOPM, 0, 3, 0,
			amic1_gain_get, amic1_gain_put),

	SOC_SINGLE_EXT("SUB MIC Gain", SND_SOC_NOPM, 0, 3, 0,
			amic2_gain_get, amic2_gain_put),

	SOC_SINGLE_EXT("EAR MIC Gain", SND_SOC_NOPM, 0, 3, 0,
			amic3_gain_get, amic3_gain_put),

	SOC_SINGLE_EXT("DMIC Bias", SND_SOC_NOPM, 0, 1, 0,
			dmic_bias_get, dmic_bias_put),

	SOC_SINGLE_EXT("AMIC Bias", SND_SOC_NOPM, 0, 1, 0,
			amic_bias_get, amic_bias_put),

	SOC_SINGLE_EXT("DMIC Mute", SND_SOC_NOPM, 0, 1000, 0,
			dmic_mute_get, dmic_mute_put),

	SOC_SINGLE_EXT("AMIC Mute", SND_SOC_NOPM, 0, 1000, 0,
			amic_mute_get, amic_mute_put),

	SOC_ENUM("ADC DAT Mux0", aud3004x_adc_dat_enum0),

	SOC_ENUM("ADC DAT Mux1", aud3004x_adc_dat_enum1),

	SOC_ENUM("ADC DAT Mux2", aud3004x_adc_dat_enum2),

	SOC_ENUM("ADC DAT Mux3", aud3004x_adc_dat_enum3),

	/*
	 * HPF Control
	 */
	SOC_ENUM("HPF Tuning", aud3004x_hpf_sel_enum),

	SOC_ENUM("HPF Order", aud3004x_hpf_order_enum),

	SOC_ENUM("HPF Left", aud3004x_hpf_ch_enum_l),

	SOC_ENUM("HPF Right", aud3004x_hpf_ch_enum_r),

	SOC_ENUM("HPF Center", aud3004x_hpf_ch_enum_c),

	/*
	 * OVP Control
	 */
	SOC_SINGLE_EXT("OVP ON Setting", SND_SOC_NOPM, 0, 1000, 0,
			ovp_on_setting_get, ovp_on_setting_put),

	SOC_SINGLE_EXT("OVP REFP", SND_SOC_NOPM, 0, SURGE_REFP_MAXNUM, 0,
			ovp_refp_get, ovp_refp_put),

	SOC_SINGLE_EXT("OVP REFN", SND_SOC_NOPM, 0, SURGE_REFN_MAXNUM, 0,
			ovp_refn_get, ovp_refn_put),

	SOC_SINGLE_EXT("OVP Time", SND_SOC_NOPM, 0, 3, 0,
			ovp_time_get, ovp_time_put),

	SOC_SINGLE_EXT("OVP Mask", SND_SOC_NOPM, 0, 1, 0,
			ovp_mask_get, ovp_mask_put),

	SOC_SINGLE_EXT("OVP APON", SND_SOC_NOPM, 0, 1, 0,
			ovp_apon_get, ovp_apon_put),

	SOC_SINGLE_EXT("OVP Mute", SND_SOC_NOPM, 0, 1, 0,
			ovp_mute_get, ovp_mute_put),

	/*
	 * VTH control
	 */
	SOC_ENUM("GDET VTH", aud3004x_gdet_vth_enum),

	SOC_ENUM("LDET VTH", aud3004x_ldet_vth_enum),

	/*
	 * DAC(Rx) path control
	 */
	SOC_DOUBLE_R_TLV("DAC Gain", AUD3004X_41_PLAY_VOLL, AUD3004X_42_PLAY_VOLR,
			DVOL_DA_SHIFT,
			DAC_DVOL_MINNUM, 1, aud3004x_dvol_dac_tlv),

	SOC_SINGLE_TLV("DAC Center Gain", AUD3004X_43_PLAY_VOLC,
			DVOL_DA_SHIFT,
			DAC_DVOL_MINNUM, 1, aud3004x_dvol_dac_tlv),

	SOC_ENUM("DAC Center Mixer", aud3004x_dac_center_mixer_enum),

	SOC_SINGLE("AVC Gain", AUD3004X_53_AVC4,
			CTVOL_AVC_PO_GAIN_SHIFT, AVC_PO_GAIN_7, 0),
	/*
	 * Jack control
	 */
	SOC_SINGLE("Jack DBNC In", AUD3004X_D8_DCTR_DBNC1,
			A2D_JACK_DBNC_IN_SHIFT, A2D_JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack DBNC Out", AUD3004X_D8_DCTR_DBNC1,
			A2D_JACK_DBNC_OUT_SHIFT, A2D_JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack ANT DBNC In", AUD3004X_DA_DCTR_DBNC3,
			A2D_JACK_DBNC_IN_SHIFT, A2D_JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack ANT DBNC Out", AUD3004X_DA_DCTR_DBNC3,
			A2D_JACK_DBNC_OUT_SHIFT, A2D_JACK_DBNC_MAX, 0),

	/*
	 * Codec control
	 */
	SOC_SINGLE_EXT("Codec Enable", SND_SOC_NOPM, 0, 1, 0,
			codec_enable_get, codec_enable_put),

	/*
	 * AVC Mute enable/disable control
	 */
	SOC_SINGLE_EXT("AVC Mute Enable", SND_SOC_NOPM, 0, 1, 0,
			avc_mute_enable_get, avc_mute_enable_put),
};

/*
 * ADC(Tx) functions
 */

/*
 * aud3004x_adc_digital_mute() - Set ADC digital Mute
 *
 * @codec: SoC audio codec device
 * @channel: Digital mute control for ADC channel
 * @on: mic mute is true, mic unmute is false
 *
 * Desc: When ADC path turn on, analog block noise can be recorded.
 * For remove this, ADC path was muted always except that it was used.
 */
void aud3004x_adc_digital_mute(struct snd_soc_component *codec,
		unsigned int channel, bool on)
{
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	mutex_lock(&aud3004x->adc_mute_lock);
	dev_info(codec->dev, "%s called, %s\n", __func__, on ? "Mute" : "Unmute");

	if (on)
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_30_ADC1, channel, channel);
	else
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_30_ADC1, channel, 0);

	aud3004x_usleep(AUD3004X_ADC_MUTE_USEC);
	mutex_unlock(&aud3004x->adc_mute_lock);
	dev_info(codec->dev, "%s: channel: %d work done.\n", __func__, channel);
}

static void aud3004x_adc_mute_work(struct work_struct *work)
{
	struct aud3004x_priv *aud3004x =
		container_of(work, struct aud3004x_priv, adc_mute_work.work);
	struct snd_soc_component *codec = aud3004x->codec;
	unsigned int chop_val1, chop_val2, dmic_on, amic_on;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1E_CHOP1, &chop_val1);
	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val2);

	dmic_on = chop_val1 & DMIC_ON_MASK;
	amic_on = chop_val2 & AMIC_ON_MASK;

	dev_info(codec->dev, "%s called, dmic_on = 0x%02x, amic_on = 0x%02x\n",
			__func__, dmic_on, amic_on);

	if (aud3004x->capture_on)
		aud3004x_adc_digital_mute(codec, ADC_MUTE_ALL, false);
}

static void aud3004x_ovp_on_setting_work(struct work_struct *work)
{
	struct aud3004x_priv *aud3004x =
		container_of(work, struct aud3004x_priv, ovp_on_setting_work.work);
	struct snd_soc_component *codec = aud3004x->codec;

	aud3004x_write(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2, 0x12);

	dev_info(codec->dev, "%s called, ovp setting done.\n", __func__);
}

static int vmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val;
	bool dac_on;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val);
	dac_on = chop_val & (SPK_ON_MASK |
			EP_ON_MASK | HP_ON_MASK | LINEOUT_ON_MASK);

	dev_info(codec->dev, "aud3004x %s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		aud3004x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
		/* Clock Gate */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
				SEQ_CLK_GATE_MASK | ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				SEQ_CLK_GATE_MASK | ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK);

		/* HPF ON */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_37_AD_HPF,
				HPF_ORDER_MASK | HPF_ENL_MASK | HPF_ENR_MASK | HPF_ENC_MASK,
				HPF_ORDER_2ND << HPF_ORDER_SHIFT |
				HPF_ENL_MASK | HPF_ENR_MASK | HPF_ENC_MASK);

		/* Gain Setting */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_38_AD_TRIM1, 0x05);
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_39_AD_TRIM2, 0x03);

		/* DWA Enable Control */
		aud3004x_write(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_106_DSM_AD, 0x07);

		/* LPF Cut-OFF frequency Control */
		aud3004x_write(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_107_LPF_AD, 0x00);

		/* ADC Current Control */
		aud3004x_write(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_2A2_CTRL_IREF1, 0x22);
		aud3004x_write(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_2A3_CTRL_IREF2, 0x02);
		aud3004x_write(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_2A4_CTRL_IREF3, 0x20);
		break;
	case SND_SOC_DAPM_POST_PMU:
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4, 0x00);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		aud3004x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* HPF OFF */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_37_AD_HPF,
				HPF_ORDER_MASK | HPF_ENL_MASK | HPF_ENR_MASK | HPF_ENC_MASK,
				HPF_ORDER_1ST << HPF_ORDER_SHIFT |
				0 << HPF_ENL_SHIFT | 0 << HPF_ENR_SHIFT | 0 << HPF_ENC_SHIFT);

		/* Clock Gate */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK, 0);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
				ADC_CLK_GATE_MASK, 0);

		if (!dac_on) {
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
					SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK, 0);
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B5_ODSEL0,
					T_PDB_VMID_MASK, 0);
			aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_110_PD_REF,
					PDB_VMID_MASK, 0);
		}
		break;
	}
	return 0;
}

static int dvmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val;
	bool dac_on;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val);
	dac_on = chop_val & (SPK_ON_MASK |
			EP_ON_MASK | HP_ON_MASK | LINEOUT_ON_MASK);

	dev_info(codec->dev, "aud3004x %s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		aud3004x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
		/* DMIC Clock Gate Enable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
				DMIC_CLK_GATE_MASK | ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				DMIC_CLK_GATE_MASK | ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK);

		/* DMIC PRE Gain */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_32_ADC3,
				DMIC_CLK_ZTIE_MASK, 0);

		/* 2ND HPF ON */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_37_AD_HPF,
				HPF_ORDER_MASK | HPF_ENL_MASK | HPF_ENR_MASK | HPF_ENC_MASK,
				HPF_ORDER_2ND << HPF_ORDER_SHIFT |
				HPF_ENL_MASK | HPF_ENR_MASK | HPF_ENC_MASK);

		/* Gain Setting */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_38_AD_TRIM1, 0xBA);
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_39_AD_TRIM2, 0x24);

		/* DMIC Path Selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_31_ADC2,
				EN_DMIC_MASK, EN_DMIC_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Compensation Filter/OSR */
		switch (aud3004x->capture_aifrate) {
		case AUD3004X_SAMPLE_RATE_48KHZ:
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_NORMAL_DMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_64 << DMIC_OSR_SHIFT);
			break;
		case AUD3004X_SAMPLE_RATE_192KHZ:
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_UHQA_WO_LP_DMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_64 << DMIC_OSR_SHIFT);
		default:
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_NORMAL_DMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_64 << DMIC_OSR_SHIFT);
			break;
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		aud3004x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
		/* Compensation Filter/OSR off */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_NORMAL_AMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_NO << DMIC_OSR_SHIFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Gain Clear */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_38_AD_TRIM1, 0x05);
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_39_AD_TRIM2, 0x03);

		/* 2ND HPF OFF */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_37_AD_HPF,
				HPF_ORDER_MASK | HPF_ENL_MASK | HPF_ENR_MASK | HPF_ENC_MASK,
				HPF_ORDER_1ST << HPF_ORDER_SHIFT |
				0 << HPF_ENL_SHIFT | 0 << HPF_ENR_SHIFT | 0 << HPF_ENC_SHIFT);

		/* DMIC PRE Gain */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_32_ADC3,
				DMIC_CLK_ZTIE_MASK, DMIC_CLK_ZTIE_MASK);

		/* DMIC Path Selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_31_ADC2,
				EN_DMIC_MASK, 0);

		/* DMIC Clock Gate Disble */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK, 0);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
				DMIC_CLK_GATE_MASK | ADC_CLK_GATE_MASK, 0);

		if (!dac_on) {
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
					COM_CLK_GATE_MASK, 0);
		}
		break;
	}
	return 0;
}

static int mic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val);

	dev_info(codec->dev, "aud3004x %s called, event = %d\n", __func__, event);

	if (!(chop_val & MIC1_ON_MASK)) {
		dev_info(codec->dev, "aud3004x %s: MIC1 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reset ADC */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK);
		aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC1 Auto Power ON */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_18_PWAUTO_AD,
				APW_MIC1L_MASK, APW_MIC1L_MASK);
		msleep(AUD3004X_APW_TIME_MSEC);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* MIC1 Auto Power OFF */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_18_PWAUTO_AD,
				APW_MIC1L_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

static int mic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	if (!(chop_val & MIC2_ON_MASK)) {
		dev_info(codec->dev, "%s: MIC2 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reset ADC */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK);
		aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC2 Auto Power ON */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_18_PWAUTO_AD,
				APW_MIC2R_MASK, APW_MIC2R_MASK);
		msleep(AUD3004X_APW_TIME_MSEC);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* MIC2 Auto Power OFF */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_18_PWAUTO_AD,
				APW_MIC2R_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

static int mic3_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val);

	dev_info(codec->dev, "aud3004x %s called, event = %d\n", __func__, event);

	if (!(chop_val & MIC3_ON_MASK)) {
		dev_info(codec->dev, "aud3004x %s: MIC3 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Prevent Call Drop */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_D2_DCTR_TEST2,
				T_PDB_ANT_MDET_MASK, 0);

		/* T_PDB_VMID ON */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B5_ODSEL0,
				T_PDB_VMID_MASK, T_PDB_VMID_MASK);

		/* PDB_VMID ON */
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_110_PD_REF,
				PDB_VMID_MASK, PDB_VMID_MASK);

		/* Reset ADC */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK);
		aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC3 Auto Power ON */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_18_PWAUTO_AD,
				APW_MIC3L_MASK, APW_MIC3L_MASK);
		msleep(AUD3004X_APW_TIME_MSEC);
		/* HQ Mode */
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_117_CTRL_REF,
				CTRM_MCB2_MASK, CTRM_MCB2_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* HQ Mode */
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_117_CTRL_REF,
				CTRM_MCB2_MASK, 0);
		/* MIC3 Auto Power OFF */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_18_PWAUTO_AD,
				APW_MIC3L_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (aud3004x->p_jackdet->ant_mode_bypass) {
			/* Prevent Call Drop */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_D2_DCTR_TEST2,
					T_PDB_ANT_MDET_MASK, T_PDB_ANT_MDET_MASK);
		}
		break;
	}

	return 0;
}

static int dmic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int dmic_on;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1E_CHOP1, &dmic_on);

	dev_info(codec->dev, "aud3004x %s called, event = %d\n", __func__, event);

	if (!(dmic_on & DMIC1_ON_MASK)) {
		dev_info(codec->dev, "aud3004x %s: DMIC1 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reset ADC */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK);
		aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* DMIC1 Enable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4,
				DMIC_EN1_MASK, DMIC_EN1_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* DMIC1 Disable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4,
				DMIC_EN1_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

static int dmic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int dmic_on;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1E_CHOP1, &dmic_on);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	if (!(dmic_on & DMIC2_ON_MASK)) {
		dev_info(codec->dev, "%s: DMIC2 is not enabled, returning.\n", __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reset ADC */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK);
		aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* DMIC2 Enable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4,
				DMIC_EN2_MASK, DMIC_EN2_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* DMIC2 Disable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_33_ADC4,
				DMIC_EN2_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

static int adc_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val1, chop_val2;
	bool amic_on, dmic_on;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1E_CHOP1, &chop_val1);
	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val2);
	dmic_on = chop_val1 & (DMIC1_ON_MASK | DMIC2_ON_MASK);
	amic_on = chop_val2 & (MIC1_ON_MASK | MIC2_ON_MASK | MIC3_ON_MASK);

	dev_info(codec->dev, "aud3004x %s called, event = %d,"
			" chop_val1: 0x%02x, chop_val2: 0x%02x\n",
			__func__, event, chop_val1, chop_val2);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Disable ADC digital mute after configuring ADC */
		cancel_delayed_work_sync(&aud3004x->adc_mute_work);
		if (amic_on)
			queue_delayed_work(aud3004x->adc_mute_wq, &aud3004x->adc_mute_work,
					msecs_to_jiffies(aud3004x->amic_mute));
		else if (dmic_on)
			queue_delayed_work(aud3004x->adc_mute_wq, &aud3004x->adc_mute_work,
					msecs_to_jiffies(aud3004x->dmic_mute));
		else
			queue_delayed_work(aud3004x->adc_mute_wq, &aud3004x->adc_mute_work,
					msecs_to_jiffies(220));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		cancel_delayed_work_sync(&aud3004x->adc_mute_work);
		/* Enable ADC digital mute before configuring ADC */
		aud3004x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Reset ADC Data */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);
		break;
	}
	return 0;
}

/*
 * DAC(Rx) functions
 */
/*
 * aud3004x_dac_soft_mute() - Set DAC soft mute
 *
 * @codec: SoC audio codec device
 * @channel: Soft mute control for DAC channel
 * @on: dac mute is true, dac unmute is false
 *
 * Desc: When DAC path turn on, analog block noise can be played.
 * For remove this, DAC path was muted always except that it was used.
 */
static void aud3004x_dac_soft_mute(struct snd_soc_component *codec,
		unsigned int channel, bool on)
{
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, %s\n", __func__, on ? "Mute" : "Unmute");

	if (on)
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1, channel, channel);
	else
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1, channel, 0);

	aud3004x_usleep(AUD3004X_DAC_MUTE_USEC);
	dev_info(codec->dev, "%s: channel: %d work done.\n", __func__, channel);
}

static int dac_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val1, chop_val2;
	bool mic_on;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val1);
	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1E_CHOP1, &chop_val2);
	mic_on = (chop_val1 & (MIC1_ON_MASK | MIC2_ON_MASK | MIC3_ON_MASK)) |
		(chop_val2 & (DMIC1_ON_MASK | DMIC2_ON_MASK));

	dev_info(codec->dev, "aud3004x %s called, event = %d,"
			" chop_val1: 0x%02x, chop_val2: 0x%02x\n",
			__func__, event, chop_val1, chop_val2);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Clock Gate */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
				SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK |
				DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK |
				DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		/* DAC Trim */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_46_PLAY_MIX2, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMU:
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* Reset off DAC Data */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Compensation Mode selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
				DAC_CH_MODE_MASK | LCH_COM_SEL_MASK
				| RCH_COM_SEL_MASK | CCH_COM_SEL_MASK, 0);

		/* Clock Mode selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
				DAC_FSEL_MASK, 0);

		/* Play Mode Selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
				PLAY_MODE_SEL_MASK, 0);

		/* Clock Gate clear */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_11_CLKGATE1,
				DSML_CLK_GATE_MASK | DSMR_CLK_GATE_MASK | DSMC_CLK_GATE_MASK |
				DAC_CIC_CGL_MASK | DAC_CIC_CGR_MASK | DAC_CIC_CGC_MASK, 0);

		/* Clock Gate disable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
				AVC_CLK_GATE_MASK | DAC_CLK_GATE_MASK | OVP_CLK_GATE_MASK, 0);

		if (!mic_on) {
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
					SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK, 0);
		}
		break;
	}
	return 0;
}

static int spkdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int val = 0;

	dev_info(codec->dev, "aud3004x %s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Clock Gate */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_11_CLKGATE1,
				DSMC_CLK_GATE_MASK | DAC_CIC_CGC_MASK,
				DSMC_CLK_GATE_MASK | DAC_CIC_CGC_MASK);

		/* Compensation Mode selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
				DAC_CH_MODE_MASK | CCH_COM_SEL_MASK,
				DAC_CH_MODE_TRIPLE << DAC_CH_MODE_SHIFT |
				COM_SEL_48K_SPK << CCH_COM_SEL_SHIFT);

		/* Clock Mode selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
				DAC_FSEL_MASK,
				CLK_DAC_512_UHQA << DAC_FSEL_SHIFT);

		/* Play Mode Selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
				PLAY_MODE_SEL_MASK,
				DAC_FREQ_TRIPLE_48K << PLAY_MODE_SEL_SHIFT);

		/* Set DAC L/R Gain Default */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_41_PLAY_VOLL, 0x54);
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_42_PLAY_VOLR, 0x54);

		/* Speaker Boost enable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_92_BOOST_CTRL0,
				DBFS_ADJ_MASK | BOOST_CTRL_EN_MASK,
				DBFS_ADJ_1 | BOOST_CTRL_EN_MASK);

		/* Speaker EXT_BST enable */
		aud3004x_acpm_update_reg(AUD3004X_PMIC_ADDR, 0x26, BIT(7), BIT(7));
		aud3004x_acpm_update_reg(AUD3004X_PMIC_ADDR, 0x26, BIT(4), BIT(4));
		aud3004x_acpm_update_reg(AUD3004X_PMIC_ADDR, 0x14, BIT(7), BIT(7));

		/* RCV Mode Dump */
		aud3004x_read(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_2A8_RCV_MODE, &val);
		aud3004x_write(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_28B_SPKOFF_S_0, val);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Reset DAC path */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK);

		/* Analog PGA Unmute */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE,
				MUTE_SPK_MASK, 0);

		/* Auto Power On */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_19_PWAUTO_DA,
				APW_SPK_MASK, APW_SPK_MASK);
		msleep(AUD3004X_APW_TIME_MSEC);

		/* DAC mute disable */
		aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* DAC mute enable */
		aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, true);

		/* Auto Power Off */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_19_PWAUTO_DA, APW_SPK_MASK, 0);
		msleep(AUD3004X_APW_TIME_MSEC);

		/* Analog PGA Mute */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE,
				MUTE_SPK_MASK, MUTE_SPK_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Speaker EXT_BST disable */
		aud3004x_acpm_update_reg(AUD3004X_PMIC_ADDR, 0x14, 0, BIT(7));
		aud3004x_acpm_update_reg(AUD3004X_PMIC_ADDR, 0x26, 0, BIT(7));
		aud3004x_acpm_update_reg(AUD3004X_PMIC_ADDR, 0x26, 0, BIT(4));
		/* Speaker Boost disable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_92_BOOST_CTRL0,
				DBFS_ADJ_MASK | BOOST_CTRL_EN_MASK, 0);
		break;
	}
	return 0;
}

static int epdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int val = 0;

	dev_info(codec->dev, "aud3004x %s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Clock Gate */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_11_CLKGATE1,
				DSMC_CLK_GATE_MASK | DAC_CIC_CGC_MASK,
				DSMC_CLK_GATE_MASK | DAC_CIC_CGC_MASK);

		/* Compensation Mode selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
				DAC_CH_MODE_MASK,
				DAC_CH_MODE_TRIPLE << DAC_CH_MODE_SHIFT);

		/* Clock Mode selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
				DAC_FSEL_MASK,
				CLK_DAC_512_UHQA << DAC_FSEL_SHIFT);

		/* Play Mode Selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
				PLAY_MODE_SEL_MASK,
				DAC_FREQ_TRIPLE_48K << PLAY_MODE_SEL_SHIFT);

		/* Set DAC L/R Gain Default */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_41_PLAY_VOLL, 0x54);
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_42_PLAY_VOLR, 0x54);

		/* DRC Range Selection */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_7A_AVC43, 0x38);

		/* Testmode Enable */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B9_ODSEL4,
				T_EN_EP_OCP_MASK, T_EN_EP_OCP_MASK);

		/* SPK Mode Dump */
		aud3004x_read(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_2A7_SPK_MODE, &val);
		aud3004x_write(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_28B_SPKOFF_S_0, val);

		/* EN_EP_OCP Disable */
		aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_150_CTRL_EP,
				EN_EP_OCP_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Reset DAC path */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK);

		/* Analog PGA Unmute */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE,
				MUTE_EP_MASK, 0);

		/* Auto Power On */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_19_PWAUTO_DA,
				APW_EP_MASK, APW_EP_MASK);
		msleep(AUD3004X_APW_TIME_MSEC);

		/* DAC mute disable */
		aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* DAC mute enable */
		aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, true);

		/* Auto Power Off */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_19_PWAUTO_DA, APW_EP_MASK, 0);
		msleep(AUD3004X_APW_TIME_MSEC);

		/* Analog PGA Mute */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE,
				MUTE_EP_MASK, MUTE_EP_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B9_ODSEL4,
				T_EN_EP_OCP_MASK, 0);

		/* DRC Range Selection */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_7A_AVC43, 0xA0);
		break;
	}
	return 0;
}

static int hpdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val;
	bool spk_on, ep_on, lineout_on;

	aud3004x_read(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1F_CHOP2, &chop_val);
	spk_on = chop_val & SPK_ON_MASK;
	ep_on = chop_val & EP_ON_MASK;
	lineout_on = chop_val & LINEOUT_ON_MASK;

	dev_info(codec->dev, "aud3004x %s called, chop: 0x%02x, event: %d, cur_aif: %d\n",
			__func__, chop_val, event, aud3004x->playback_aifrate);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Clock Gate */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_10_CLKGATE0,
				OVP_CLK_GATE_MASK, OVP_CLK_GATE_MASK);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_11_CLKGATE1,
				DSML_CLK_GATE_MASK | DSMR_CLK_GATE_MASK |
				DAC_CIC_CGL_MASK | DAC_CIC_CGR_MASK,
				DSML_CLK_GATE_MASK | DSMR_CLK_GATE_MASK |
				DAC_CIC_CGL_MASK | DAC_CIC_CGR_MASK);

		/* AVC Slope Change */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_53_AVC4,
				DRC_SLOPE_EN_MASK, 0);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_53_AVC4,
				DRC_SLOPE_SEL_MASK, 7 << DRC_SLOPE_SEL_SHIFT);

		/* DRC Range Selection */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_7A_AVC43, 0x70);

		switch (aud3004x->playback_aifrate) {
		case AUD3004X_SAMPLE_RATE_48KHZ:
			/* CP Frequency Control */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B0_AUTO_COM1, 0x0C);
			aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_150_CTRL_EP,
					EN_CP_AUTOSEL_MASK, 0);

			/* HP Driver Current */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_A6_AUTO_HP7, 0x1B);

			/* DAC Trim */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_47_TRIM_DAC0, 0xE9);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004x_48_TRIM_DAC1, 0x79);
			if (ep_on | lineout_on) {
				/* Compensation Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
						DAC_CH_MODE_MASK,
						DAC_CH_MODE_TRIPLE << DAC_CH_MODE_SHIFT);

				/* Clock Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
						DAC_FSEL_MASK,
						CLK_DAC_512_UHQA << DAC_FSEL_SHIFT);

				/* Play Mode Selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
						PLAY_MODE_SEL_MASK,
						DAC_FREQ_TRIPLE_48K << PLAY_MODE_SEL_SHIFT);
			} else if (spk_on) {
				/* Compensation Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
						DAC_CH_MODE_MASK | CCH_COM_SEL_MASK,
						DAC_CH_MODE_TRIPLE << DAC_CH_MODE_SHIFT |
						COM_SEL_48K_SPK << CCH_COM_SEL_SHIFT);

				/* Clock Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
						DAC_FSEL_MASK,
						CLK_DAC_512_UHQA << DAC_FSEL_SHIFT);

				/* Play Mode Selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
						PLAY_MODE_SEL_MASK,
						DAC_FREQ_TRIPLE_48K << PLAY_MODE_SEL_SHIFT);
			} else {
				/* Compensation Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
						DAC_CH_MODE_MASK,
						DAC_CH_MODE_STEREO << DAC_CH_MODE_SHIFT);

				/* Clock Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
						DAC_FSEL_MASK,
						CLK_DAC_256_STEREO << DAC_FSEL_SHIFT);

				/* Play Mode Selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
						PLAY_MODE_SEL_MASK,
						DAC_FREQ_STEREO_48K << PLAY_MODE_SEL_SHIFT);
			}
			break;
		case AUD3004X_SAMPLE_RATE_192KHZ:
			/* CP Frequency Control */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B0_AUTO_COM1, 0x1C);
			aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_150_CTRL_EP,
					EN_CP_AUTOSEL_MASK, 0);

			/* HP Driver Current */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_A6_AUTO_HP7, 0x36);

			/* DAC Trim */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_47_TRIM_DAC0, 0xFF);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004x_48_TRIM_DAC1, 0x29);

			/* Compensation Mode selection */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
					DAC_CH_MODE_MASK | LCH_COM_SEL_MASK
					| RCH_COM_SEL_MASK | CCH_COM_SEL_MASK,
					DAC_CH_MODE_STEREO << DAC_CH_MODE_SHIFT |
					COM_SEL_192K << LCH_COM_SEL_SHIFT |
					COM_SEL_192K << RCH_COM_SEL_SHIFT |
					COM_SEL_192K << CCH_COM_SEL_SHIFT);

			/* Clock Mode selection */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
					DAC_FSEL_MASK,
					CLK_DAC_512_UHQA << DAC_FSEL_SHIFT);

			/* Play Mode Selection */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
					PLAY_MODE_SEL_MASK, DAC_FREQ_STEREO_192K << PLAY_MODE_SEL_SHIFT);
			break;
		default:
			/* CP Frequency Control */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B0_AUTO_COM1, 0x0C);
			aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_150_CTRL_EP,
					EN_CP_AUTOSEL_MASK, 0);

			/* HP Driver Current */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_A6_AUTO_HP7, 0x1B);

			/* DAC Trim */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_47_TRIM_DAC0, 0xE9);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004x_48_TRIM_DAC1, 0x79);
			if (ep_on | lineout_on) {
				/* Compensation Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
						DAC_CH_MODE_MASK,
						DAC_CH_MODE_TRIPLE << DAC_CH_MODE_SHIFT);

				/* Clock Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
						DAC_FSEL_MASK,
						CLK_DAC_512_UHQA << DAC_FSEL_SHIFT);

				/* Play Mode Selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
						PLAY_MODE_SEL_MASK,
						DAC_FREQ_TRIPLE_48K << PLAY_MODE_SEL_SHIFT);
			} else if (spk_on) {
				/* Compensation Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
						DAC_CH_MODE_MASK | CCH_COM_SEL_MASK,
						DAC_CH_MODE_TRIPLE << DAC_CH_MODE_SHIFT |
						COM_SEL_48K_SPK << CCH_COM_SEL_SHIFT);

				/* Clock Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
						DAC_FSEL_MASK,
						CLK_DAC_512_UHQA << DAC_FSEL_SHIFT);

				/* Play Mode Selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
						PLAY_MODE_SEL_MASK,
						DAC_FREQ_TRIPLE_48K << PLAY_MODE_SEL_SHIFT);
			} else {
				/* Compensation Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
						DAC_CH_MODE_MASK,
						DAC_CH_MODE_STEREO << DAC_CH_MODE_SHIFT);

				/* Clock Mode selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
						DAC_FSEL_MASK,
						CLK_DAC_256_STEREO << DAC_FSEL_SHIFT);

				/* Play Mode Selection */
				aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
						PLAY_MODE_SEL_MASK,
						DAC_FREQ_STEREO_48K << PLAY_MODE_SEL_SHIFT);
			}
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Reset DAC path */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK);

		/* Analog PGA Unmute */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE,
				MUTE_HP_MASK, 0);

		/* Auto Power On */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_19_PWAUTO_DA,
				APW_HP_MASK, APW_HP_MASK);
		msleep(AUD3004X_APW_TIME_MSEC);

		/* enable HP OVP */
		cancel_delayed_work_sync(&aud3004x->ovp_on_setting_work);
		queue_delayed_work(aud3004x->ovp_on_setting_wq, &aud3004x->ovp_on_setting_work,
				msecs_to_jiffies(aud3004x->ovp_on_setting));

		/* DAC mute disable */
		aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* DAC mute enable */
		aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, true);

		/* Auto Power Off */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_19_PWAUTO_DA, APW_HP_MASK, 0);
		msleep(AUD3004X_APW_TIME_MSEC);

		/* Cancel enable HP OVP */
		cancel_delayed_work_sync(&aud3004x->ovp_on_setting_work);
		msleep(AUD3004X_OVP_OFF_MSEC);
		aud3004x_write(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_160_OVP2, 0x02);

		/* Analog PGA Mute */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE,
				MUTE_HP_MASK, MUTE_HP_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Default Setting */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B0_AUTO_COM1, 0x0D);

		/* DAC Trim clear */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_47_TRIM_DAC0, 0xC9);
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004x_48_TRIM_DAC1, 0x65);

		/* HP Driver Current Clear */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_A6_AUTO_HP7, 0x01);

		/* DRC Range Setting Default */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_7A_AVC43, 0xA0);

		/* AVC Slope Setting Default */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_53_AVC4,
				DRC_SLOPE_SEL_MASK, 0 << DRC_SLOPE_SEL_SHIFT);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_53_AVC4,
				DRC_SLOPE_EN_MASK, DRC_SLOPE_EN_MASK);
		break;
	}
	return 0;
}

static int linedrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "aud3004x %s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Clock Gate */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_11_CLKGATE1,
				DSMC_CLK_GATE_MASK | DAC_CIC_CGC_MASK,
				DSMC_CLK_GATE_MASK | DAC_CIC_CGC_MASK);

		/* Compensation Mode selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_44_PLAY_MODE2,
				DAC_CH_MODE_MASK,
				DAC_CH_MODE_TRIPLE << DAC_CH_MODE_SHIFT);

		/* Clock Mode selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_16_CLK_MODE_SEL,
				DAC_FSEL_MASK,
				CLK_DAC_512_UHQA << DAC_FSEL_SHIFT);

		/* Play Mode Selection */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_40_PLAY_MODE1,
				PLAY_MODE_SEL_MASK,
				DAC_FREQ_TRIPLE_48K << PLAY_MODE_SEL_SHIFT);

		/* Set DAC L/R Gain Default */
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_41_PLAY_VOLL, 0x54);
		aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_42_PLAY_VOLR, 0x54);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Reset DAC path */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK,
				AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK);

		/* Analog PGA Unmute */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE,
				MUTE_LINE_MASK, 0);

		/* Auto Power On */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_19_PWAUTO_DA,
				APW_LINE_MASK, APW_LINE_MASK);
		msleep(AUD3004X_APW_TIME_MSEC);

		/* DAC mute disable */
		aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* DAC mute enable */
		aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, true);

		/* Auto Power Off */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_19_PWAUTO_DA, APW_LINE_MASK, 0);
		msleep(AUD3004X_APW_TIME_MSEC);

		/* Analog PGA Mute */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE,
				MUTE_LINE_MASK, MUTE_LINE_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

/*
 * dapm widget controls set
 */

/* INP SEL */
static const char * const aud3004x_inp_sel_src_l[] = {
	"AMIC_L ADC_L", "AMIC_R ADC_L", "Not Used", "Zero ADC_L",
	"DMIC1L ADC_L", "DMIC1R ADC_L", "DMIC2L ADC_L", "DMIC2R ADC_L"
};
static const char * const aud3004x_inp_sel_src_r[] = {
	"AMIC_R ADC_R", "AMIC_L ADC_R", "Not Used", "Zero ADC_R",
	"DMIC1L ADC_R", "DMIC1R ADC_R", "DMIC2L ADC_R", "DMIC2R ADC_R"
};

static SOC_ENUM_SINGLE_DECL(aud3004x_inp_sel_enum_l, AUD3004X_31_ADC2,
		INP_SEL_L_SHIFT, aud3004x_inp_sel_src_l);
static SOC_ENUM_SINGLE_DECL(aud3004x_inp_sel_enum_r, AUD3004X_31_ADC2,
		INP_SEL_R_SHIFT, aud3004x_inp_sel_src_r);

static const struct snd_kcontrol_new aud3004x_inp_sel_l =
		SOC_DAPM_ENUM("INP_SEL_L", aud3004x_inp_sel_enum_l);
static const struct snd_kcontrol_new aud3004x_inp_sel_r =
		SOC_DAPM_ENUM("INP_SEL_R", aud3004x_inp_sel_enum_r);

/* MIC On */
static const struct snd_kcontrol_new mic1_on[] = {
	SOC_DAPM_SINGLE("MIC1 On", AUD3004X_1F_CHOP2, MIC1_ON_SHIFT, 1, 0),
};
static const struct snd_kcontrol_new mic2_on[] = {
	SOC_DAPM_SINGLE("MIC2 On", AUD3004X_1F_CHOP2, MIC2_ON_SHIFT, 1, 0),
};
static const struct snd_kcontrol_new mic3_on[] = {
	SOC_DAPM_SINGLE("MIC3 On", AUD3004X_1F_CHOP2, MIC3_ON_SHIFT, 1, 0),
};

/* ADC Mixer */
static const struct snd_kcontrol_new adcl_mix[] = {
	SOC_DAPM_SINGLE("MIC1L Switch", AUD3004X_1E_CHOP1,
			MIX_MIC1L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2L Switch", AUD3004X_1E_CHOP1,
			MIX_MIC2L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC3L Switch", AUD3004X_1E_CHOP1,
			MIX_MIC3L_SHIFT, 1, 0),
};
static const struct snd_kcontrol_new adcr_mix[] = {
	SOC_DAPM_SINGLE("MIC1R Switch", AUD3004X_1E_CHOP1,
			MIX_MIC1R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2R Switch", AUD3004X_1E_CHOP1,
			MIX_MIC2R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC3R Switch", AUD3004X_1E_CHOP1,
			MIX_MIC3R_SHIFT, 1, 0),
};

/* DMIC On */
static const struct snd_kcontrol_new dmic1_on[] = {
	SOC_DAPM_SINGLE("DMIC1 On", AUD3004X_1E_CHOP1, DMIC1_ON_SHIFT, 1, 0),
};
static const struct snd_kcontrol_new dmic2_on[] = {
	SOC_DAPM_SINGLE("DMIC2 On", AUD3004X_1E_CHOP1, DMIC2_ON_SHIFT, 1, 0),
};

/* Rx Devices */
static const struct snd_kcontrol_new spk_on[] = {
	SOC_DAPM_SINGLE("SPK On", AUD3004X_1F_CHOP2, SPK_ON_SHIFT, 1, 0),
};
static const struct snd_kcontrol_new ep_on[] = {
	SOC_DAPM_SINGLE("EP On", AUD3004X_1F_CHOP2, EP_ON_SHIFT, 1, 0),
};
static const struct snd_kcontrol_new hp_on[] = {
	SOC_DAPM_SINGLE("HP On", AUD3004X_1F_CHOP2, HP_ON_SHIFT, 1, 0),
};
static const struct snd_kcontrol_new lineout_on[] = {
	SOC_DAPM_SINGLE("LINEOUT On", AUD3004X_1F_CHOP2, LINEOUT_ON_SHIFT, 1, 0),
};

static const struct snd_soc_dapm_widget aud3004x_dapm_widgets[] = {
	/*
	 * ADC(Tx) dapm widget
	 */
	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),
	SND_SOC_DAPM_INPUT("IN3L"),

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

	SND_SOC_DAPM_PGA_E("DMIC1_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, dmic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DMIC2_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, dmic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("MIC1", SND_SOC_NOPM, 0, 0, mic1_on),
	SND_SOC_DAPM_SWITCH("MIC2", SND_SOC_NOPM, 0, 0, mic2_on),
	SND_SOC_DAPM_SWITCH("MIC3", SND_SOC_NOPM, 0, 0, mic3_on),

	SND_SOC_DAPM_MIXER("ADCL Mixer", SND_SOC_NOPM, 0, 0, adcl_mix,
			ARRAY_SIZE(adcl_mix)),
	SND_SOC_DAPM_MIXER("ADCR Mixer", SND_SOC_NOPM, 0, 0, adcr_mix,
			ARRAY_SIZE(adcr_mix)),

	SND_SOC_DAPM_SWITCH("DMIC1", SND_SOC_NOPM, 0, 0, dmic1_on),
	SND_SOC_DAPM_SWITCH("DMIC2", SND_SOC_NOPM, 0, 0, dmic2_on),

	SND_SOC_DAPM_MUX("INP_SEL_L", SND_SOC_NOPM, 0, 0, &aud3004x_inp_sel_l),
	SND_SOC_DAPM_MUX("INP_SEL_R", SND_SOC_NOPM, 0, 0, &aud3004x_inp_sel_r),

	SND_SOC_DAPM_ADC_E("ADC", "AIF Capture", SND_SOC_NOPM, 0, 0, adc_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC", "AIF2 Capture", SND_SOC_NOPM, 0, 0, adc_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	/*
	 * DAC(Rx) dapm widget
	 */
	SND_SOC_DAPM_SWITCH("SPK", SND_SOC_NOPM, 0, 0, spk_on),
	SND_SOC_DAPM_SWITCH("EP", SND_SOC_NOPM, 0, 0, ep_on),
	SND_SOC_DAPM_SWITCH("HP", SND_SOC_NOPM, 0, 0, hp_on),
	SND_SOC_DAPM_SWITCH("LINEOUT", SND_SOC_NOPM, 0, 0, lineout_on),

	SND_SOC_DAPM_OUT_DRV_E("SPKDRV", SND_SOC_NOPM, 0, 0, NULL, 0, spkdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("EPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, epdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("HPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, hpdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("LINEDRV", SND_SOC_NOPM, 0, 0, NULL, 0, linedrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("DAC", "AIF Playback", SND_SOC_NOPM, 0, 0, dac_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC", "AIF2 Playback", SND_SOC_NOPM, 0, 0, dac_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("SPKOUTLN"),
	SND_SOC_DAPM_OUTPUT("EPOUTLN"),
	SND_SOC_DAPM_OUTPUT("HPOUTLN"),
	SND_SOC_DAPM_OUTPUT("LINEOUTLN"),
};

static const struct snd_soc_dapm_route aud3004x_dapm_routes[] = {
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

	{"ADCL Mixer", "MIC1L Switch", "MIC1"},
	{"ADCR Mixer", "MIC1R Switch", "MIC1"},
	{"ADCL Mixer", "MIC2L Switch", "MIC2"},
	{"ADCR Mixer", "MIC2R Switch", "MIC2"},
	{"ADCL Mixer", "MIC3L Switch", "MIC3"},
	{"ADCR Mixer", "MIC3R Switch", "MIC3"},

	{"DMIC1_PGA", NULL, "IN1L"},
	{"DMIC1_PGA", NULL, "DVMID"},
	{"DMIC1", "DMIC1 On", "DMIC1_PGA"},

	{"DMIC2_PGA", NULL, "IN2L"},
	{"DMIC2_PGA", NULL, "DVMID"},
	{"DMIC2", "DMIC2 On", "DMIC2_PGA"},

	{"INP_SEL_L", "AMIC_L ADC_L", "ADCL Mixer"},
	{"INP_SEL_L", "AMIC_R ADC_L", "ADCR Mixer"},
	{"INP_SEL_L", "DMIC1L ADC_L", "DMIC1"},
	{"INP_SEL_L", "DMIC1R ADC_L", "DMIC1"},
	{"INP_SEL_L", "DMIC2L ADC_L", "DMIC2"},
	{"INP_SEL_L", "DMIC2R ADC_L", "DMIC2"},

	{"INP_SEL_R", "AMIC_L ADC_R", "ADCL Mixer"},
	{"INP_SEL_R", "AMIC_R ADC_R", "ADCR Mixer"},
	{"INP_SEL_R", "DMIC1L ADC_R", "DMIC1"},
	{"INP_SEL_R", "DMIC1R ADC_R", "DMIC1"},
	{"INP_SEL_R", "DMIC2L ADC_R", "DMIC2"},
	{"INP_SEL_R", "DMIC2R ADC_R", "DMIC2"},

	{"ADC", NULL, "INP_SEL_L"},
	{"ADC", NULL, "INP_SEL_R"},

	{"AIF Capture", NULL, "ADC"},
	{"AIF2 Capture", NULL, "ADC"},

	/*
	 * DAC(Rx) dapm route
	 */
	{"DAC", NULL, "AIF Playback"},
	{"DAC", NULL, "AIF2 Playback"},

	{"SPKDRV", NULL, "DAC"},
	{"SPK", "SPK On", "SPKDRV"},
	{"SPKOUTLN", NULL, "SPK"},

	{"EPDRV", NULL, "DAC"},
	{"EP", "EP On", "EPDRV"},
	{"EPOUTLN", NULL, "EP"},

	{"HPDRV", NULL, "DAC"},
	{"HP", "HP On", "HPDRV"},
	{"HPOUTLN", NULL, "HP"},

	{"LINEDRV", NULL, "DAC"},
	{"LINEOUT", "LINEOUT On", "LINEDRV"},
	{"LINEOUTLN", NULL, "LINEOUT"},
};

static int aud3004x_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *codec = dai->component;

	dev_info(codec->dev, "%s called. fmt: %d\n", __func__, fmt);

	return 0;
}

static int aud3004x_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
//	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "(%s) %s completed\n", substream->stream ? "C" : "P", __func__);

	return 0;
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
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n", __func__,
		 aud3004x->capture_aifrate, cur_aifrate);

	aud3004x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
	aud3004x->capture_on = true;

	if (aud3004x->capture_aifrate != cur_aifrate) {
		switch (cur_aifrate) {
		case AUD3004X_SAMPLE_RATE_48KHZ:
			/* RESET Core */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
					CORE_RESETB_MASK, 0);
			aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
					CORE_RESETB_MASK, CORE_RESETB_MASK);

			/* 48K output format selection */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_29_IF_FORM6,
					ADC_OUT_FORMAT_SEL_MASK, ADC_FM_48K_AT_48K);

			/* UHQA I/O strength selection */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_2F_DMIC_ST,
					DMIC_ST_MASK, DMIC_IO_ST_4);
			break;
		case AUD3004X_SAMPLE_RATE_192KHZ:
			/* RESET Core */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
					CORE_RESETB_MASK, 0);
			aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
					CORE_RESETB_MASK, CORE_RESETB_MASK);

			/* 192K output format selection */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_29_IF_FORM6,
					ADC_OUT_FORMAT_SEL_MASK, ADC_FM_192K_AT_192K);
			/* UHQA I/O strength selection */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_2F_DMIC_ST,
					DMIC_ST_MASK, DMIC_IO_ST_8);
			break;
		default:
			dev_err(codec->dev, "%s: sample rate error!\n", __func__);
			break;
		}
		aud3004x->capture_aifrate = cur_aifrate;
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
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called. priv_aif: %d, cur_aif: %d\n", __func__,
		 aud3004x->playback_aifrate, cur_aifrate);
	aud3004x->playback_on = true;

	if (aud3004x->playback_aifrate != cur_aifrate) {
		switch (cur_aifrate) {
		case AUD3004X_SAMPLE_RATE_48KHZ:
			/* RESET Core */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
					CORE_RESETB_MASK, 0);
			aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
					CORE_RESETB_MASK, CORE_RESETB_MASK);

			/* AVC delay default setting */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_72_AVC35, 0x38);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_71_AVC34, 0x18);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_74_AVC37, 0xC1);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_73_AVC36, 0x17);

			/* OTP DCT Current */
			aud3004x_write(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_2A6_CTRL_IREF5, 0x59);
			break;
		case AUD3004X_SAMPLE_RATE_192KHZ:
			/* RESET Core */
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
					CORE_RESETB_MASK, 0);
			aud3004x_usleep(AUD3004X_RESETB_TIME_USEC);
			aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
					CORE_RESETB_MASK, CORE_RESETB_MASK);

			/* AVC delay default setting */
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_72_AVC35, 0x19);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_71_AVC34, 0x1A);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_74_AVC37, 0x21);
			aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_73_AVC36, 0x18);

			/* OTP DCT Current */
			aud3004x_write(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_2A6_CTRL_IREF5, 0x51);
			break;
		default:
			dev_err(codec->dev, "%s: sample rate error!\n", __func__);
			break;
		}
		aud3004x->playback_aifrate = cur_aifrate;
	}
}

static int aud3004x_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	unsigned int cur_aifrate, width, channels;

	/* Get params */
	cur_aifrate = params_rate(params);
	width = params_width(params);
	channels = params_channels(params);

	dev_info(codec->dev, "(%s) %s called. aifrate: %d, width: %d, channels: %d\n",
		substream->stream ? "C" : "P", __func__, cur_aifrate, width, channels);

	switch (width) {
	case BIT_RATE_16:
		/* I2S 16bit/32fs Set */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_16);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_22_IF_FORM3,
				I2S_XFS_IF3_MASK, I2S_XFS_32);
		break;
	case BIT_RATE_24:
		/* I2S 24bit/48fs Set */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_24);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_22_IF_FORM3,
				I2S_XFS_IF3_MASK, I2S_XFS_48);
		break;
	case BIT_RATE_32:
		/* I2S 32bit/64fs Set */
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_32);
		aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_22_IF_FORM3,
				I2S_XFS_IF3_MASK, I2S_XFS_64);
		break;
	default:
		dev_err(codec->dev, "%s: bit rate error!\n", __func__);
		break;
	}

	if (substream->stream)
		capture_hw_params(codec, cur_aifrate);
	else
		playback_hw_params(codec, cur_aifrate);

	return 0;
}

static void aud3004x_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	if (substream->stream)
		aud3004x->capture_on = false;
	else
		aud3004x->playback_on = false;

	dev_info(codec->dev, "(%s) %s completed\n",
		 substream->stream ? "C" : "P", __func__);
}

static int aud3004x_dai_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;

	dev_info(codec->dev, "(%s) %s completed\n",
		 substream->stream ? "C" : "P", __func__);

	return 0;
}

static const struct snd_soc_dai_ops aud3004x_dai_ops = {
	.set_fmt = aud3004x_dai_set_fmt,
	.startup = aud3004x_dai_startup,
	.hw_params = aud3004x_dai_hw_params,
	.shutdown = aud3004x_dai_shutdown,
	.hw_free = aud3004x_dai_hw_free,
};

#define AUD3004X_RATES		SNDRV_PCM_RATE_8000_192000
#define AUD3004X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S20_3LE | \
				SNDRV_PCM_FMTBIT_S24_LE  | \
				SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver aud3004x_dai[] = {
	{
		.name = "aud3004x-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = AUD3004X_RATES,
			.formats = AUD3004X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = AUD3004X_RATES,
			.formats = AUD3004X_FORMATS,
		},
		.ops = &aud3004x_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "aud3004x-aif2",
		.id = 2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AUD3004X_RATES,
			.formats = AUD3004X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AUD3004X_RATES,
			.formats = AUD3004X_FORMATS,
		},
		.ops = &aud3004x_dai_ops,
		.symmetric_rates = 1,
	},
};

static void aud3004x_regmap_sync(struct device *dev)
{
	struct aud3004x_priv *aud3004x = dev_get_drvdata(dev);
	unsigned int reg[AUD3004X_REGCACHE_SYNC_END] = {0,};
	int p_reg, p_map;

	for (p_map = AUD3004D; p_map <= AUD3004O; p_map++) {
		/* Read from Cache */
		for (p_reg = 0; p_reg < AUD3004X_REGCACHE_SYNC_END; p_reg++) {
			if (read_from_cache(dev, p_reg, p_map))
				regmap_read(aud3004x->regmap[p_map], p_reg, &reg[p_reg]);
		}

		regcache_cache_bypass(aud3004x->regmap[p_map], true);
		/* Update HW */
		for (p_reg = 0; p_reg < AUD3004X_REGCACHE_SYNC_END; p_reg++)
			if (write_to_hw(dev, p_reg, p_map))
				regmap_write(aud3004x->regmap[p_map], p_reg, reg[p_reg]);
		regcache_cache_bypass(aud3004x->regmap[p_map], false);

	}
}

static void aud3004x_reg_restore(struct snd_soc_component *codec)
{
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_D0_DCTR_CM,
			PDB_JD_CLK_EN_MASK, PDB_JD_CLK_EN_MASK);

	msleep(AUD3004X_PDB_CLK_MSEC);
	aud3004x_regmap_sync(codec->dev);
}

int aud3004x_regulators_enable(struct snd_soc_component *codec)
{
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);
	int ret;

	ret = regulator_enable(aud3004x->vdd);
	ret = regulator_enable(aud3004x->vdd2);

	return ret;
}

void aud3004x_regulators_disable(struct snd_soc_component *codec)
{
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	if (aud3004x->regulator_count == 0) {
		regulator_disable(aud3004x->vdd);
		regulator_disable(aud3004x->vdd2);
	}
}

int aud3004x_enable(struct device *dev)
{
	struct aud3004x_priv *aud3004x = dev_get_drvdata(dev);
	int p_map;

	dev_info(dev, "(*) %s\n", __func__);
	aud3004x->is_suspend = false;
	aud3004x->regulator_count++;

	abox_enable_mclk(true);
	aud3004x_regulators_enable(aud3004x->codec);

	/* Disable cache_only feature and sync the cache with h/w */
	for (p_map = AUD3004D; p_map <= AUD3004O; p_map++)
		regcache_cache_only(aud3004x->regmap[p_map], false);

	aud3004x_reg_restore(aud3004x->codec);

	return 0;
}

int aud3004x_disable(struct device *dev)
{
	struct aud3004x_priv *aud3004x = dev_get_drvdata(dev);
	int p_map;

	dev_info(dev, "(*) %s\n", __func__);
	aud3004x->is_suspend = true;
	aud3004x->regulator_count--;

	/* As device is going to suspend-state, limit the writes to cache */
	for (p_map = AUD3004D; p_map <= AUD3004O; p_map++)
		regcache_cache_only(aud3004x->regmap[p_map], true);

	aud3004x_regulators_disable(aud3004x->codec);
	abox_enable_mclk(false);

	return 0;
}

static int aud3004x_sys_resume(struct device *dev)
{
#if !IS_ENABLED(CONFIG_PM)
	struct aud3004x_priv *aud3004x = dev_get_drvdata(dev);

	if (!aud3004x->is_suspend) {
		dev_info(dev, "(*)aud3004x not resuming, cp functioning\n");
		return 0;
	}
	dev_info(dev, "(*) %s\n", __func__);
	aud3004x->pm_suspend = false;
	aud3004x_enable(dev);
#endif

	return 0;
}

static int aud3004x_sys_suspend(struct device *dev)
{
#if !IS_ENABLED(CONFIG_PM)
	struct aud3004x_priv *aud3004x = dev_get_drvdata(dev);

	if (abox_is_on()) {
		dev_info(dev, "(*)Don't suspend aud3004x, cp functioning\n");
		return 0;
	}
	dev_info(dev, "(*) %s\n", __func__);
	aud3004x->pm_suspend = true;
	aud3004x_disable(dev);
#endif

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int aud3004x_runtime_resume(struct device *dev)
{
	dev_info(dev, "(*) %s\n", __func__);
	aud3004x_enable(dev);

	return 0;
}

static int aud3004x_runtime_suspend(struct device *dev)
{
	dev_info(dev, "(*) %s\n", __func__);
	aud3004x_disable(dev);

	return 0;
}
#endif

static const struct dev_pm_ops aud3004x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(
			aud3004x_sys_suspend,
			aud3004x_sys_resume)
#if IS_ENABLED(CONFIG_PM)
	SET_RUNTIME_PM_OPS(
			aud3004x_runtime_suspend,
			aud3004x_runtime_resume,
			NULL)
#endif
};


static void aud3004x_i2c_parse_dt(struct aud3004x_priv *aud3004x)
{
	struct device *dev = aud3004x->dev;
	struct device_node *np = dev->of_node;
	unsigned int feature_flag;
	int ret;

	codec_np = np;

	/* model feature flag */
	ret = of_property_read_u32(dev->of_node, "use-feature-flag", &feature_flag);
	if (!ret)
		aud3004x->model_feature_flag = feature_flag;
	else
		aud3004x->model_feature_flag = 0;

	dev_info(dev, "%s, Codec Feature Flag: %#x\n", __func__, feature_flag);

	aud3004x->dmic_bias_gpio = of_get_named_gpio(np, "dmic-bias-gpio", 0);
	if (aud3004x->dmic_bias_gpio < 0)
		dev_err(dev, "%s: cannot find dmic bias gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: dmic bias gpio = %d\n", __func__,
			 aud3004x->dmic_bias_gpio);
}


/*
 * aud3004x_register_initialize() - Codec reigster initialize
 *
 * When system boot, codec should successful probe with specific parameters.
 * These specific values change the chip default register value
 * to arbitrary value in booting in order to modify an issue
 * that cannot be predicted during the design.
 *
 * The values provided in this function are hard-coded register values, and we
 * need not update these values as per bit-fields.
 */
static void aud3004x_register_initialize(void *context)
{
	struct snd_soc_component *codec = (struct snd_soc_component *)context;
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

#if IS_ENABLED(CONFIG_PM)
	pm_runtime_get_sync(codec->dev);
#else
	aud3004x_enable(codec->dev);
#endif

	dev_info(codec->dev, "%s, setting defaults\n", __func__);

	/* PDB_JD_CLK_EN */
	aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_D0_DCTR_CM,
			PDB_JD_CLK_EN_MASK, 0);
	msleep(AUD3004X_PDB_CLK_MSEC);
	aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_D0_DCTR_CM,
			PDB_JD_CLK_EN_MASK, PDB_JD_CLK_EN_MASK);

	/* AVC RESETB initialize */
	aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_14_RESETB0,
			AVC_RESETB_MASK, 0);

	/* HP Protection Off */
	aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_BB_ODSEL6, 0x60);

	/* OVP Setting */
	aud3004x_write(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_15F_OVP1, 0xAE);

	/* OTP HP Current */
	aud3004x_write(aud3004x, AUD3004X_OTP_ADDR, AUD3004X_2B0_CTRL_HPS, 0x59);

	/* C-Channel Mixing */
	aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_45_PLAY_MIX1,
			DAC_MIXC_MASK, MIXC_LR_BY_2);

	/* AVC Mute Disable */
	aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_5C_AVC13, 0x88);

	/* DLDO Set */
	aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_CC_ACTR_DLDO,
			CTRV_DIG_LDO_MASK, DLDO_1_5V << CTRV_DIG_LDO_SHIFT);

	/* Speaker Skip */
	aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_B1_AUTO_SPK1, 0x62);

	/* Speaker Boost Setting */
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x37, 0x5A);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x38, 0xC5);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x39, 0x6A);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x3A, 0x68);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x3B, 0x1D);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x3C, 0x30);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x3D, 0x55);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x3E, 0x20);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x3F, 0x95);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x40, 0x75);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x41, 0x97);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x42, 0x83);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x43, 0x88);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x44, 0x86);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x45, 0x87);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x46, 0x84);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x47, 0x90);
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x48, 0x8A);

	/* AMIC Boost Volume Control */
	aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_134_VOL_AD1,
			CTVOL_BST_PGA1_MASK, 0);
	aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_135_VOL_AD2,
			CTVOL_BST_PGA2_MASK, 0);
	aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_136_VOL_AD3,
			CTVOL_BST_PGA3_MASK, 0);

	/* DMIC Gain */
	aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_32_ADC3,
			DMIC_GAIN_PRE_MASK, DMIC_GAIN_3 << DMIC_GAIN_PRE_SHIFT);

	/* MDET Comp */
	aud3004x_acpm_write_reg(AUD3004X_CLOSE_ADDR, 0x97, 0x55);

	/* Offset Range control */
	aud3004x_update_bits(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_4E_OFFSET_OPT,
			OFFSET_RNGL_MASK | OFFSET_RNGR_MASK | OFFSET_RNGC_MASK,
			3 << OFFSET_RNGL_SHIFT |
			3 << OFFSET_RNGR_SHIFT |
			3 << OFFSET_RNGC_SHIFT);

	/* SEL Check DIGMUTE HP */
	aud3004x_update_bits(aud3004x, AUD3004X_ANALOG_ADDR, AUD3004X_102_CLK3_COD,
			SEL_DIGMUTE_HP_MASK, 2 << SEL_DIGMUTE_HP_SHIFT);

	/* Tuning AVC Parameters */
	aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_68_AVC25, 0x08);

	/* ADC/DAC Mute */
	aud3004x_write(aud3004x, AUD3004X_DIGITAL_ADDR, AUD3004X_1A_DRIVER_MUTE, 0x0F);
	aud3004x_adc_digital_mute(codec, ADC_MUTE_ALL, true);
	aud3004x_dac_soft_mute(codec, DAC_MUTE_ALL, true);

	/* All boot time hardware access is done. Put the device to sleep. */
#if IS_ENABLED(CONFIG_PM)
	pm_runtime_put_sync(codec->dev);
#else
	aud3004x_disable(codec->dev);
#endif
}

static int aud3004x_codec_probe(struct snd_soc_component *codec)
{
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s codec name: %s, id: %d\n", __func__, codec->name, codec->id);

	aud3004x->codec = codec;

	/* register codec power */
	aud3004x->vdd = devm_regulator_get(codec->dev, "vdd_aldo1");
	if (IS_ERR(aud3004x->vdd)) {
		dev_err(codec->dev, "%s, failed to get regulator vdd\n", __func__);
		return PTR_ERR(aud3004x->vdd);
	}

	aud3004x->vdd2 = devm_regulator_get(codec->dev, "vdd_aldo2");
	if (IS_ERR(aud3004x->vdd2)) {
		dev_err(codec->dev, "%s, failed to get regulator vdd2\n", __func__);
		return PTR_ERR(aud3004x->vdd2);
	}

	/* initialize codec_priv variable */
	aud3004x->codec_ver = AUD3004X_CODEC_VER;
	aud3004x->playback_aifrate = 0;
	aud3004x->capture_aifrate = 0;
	aud3004x->mic_status = 0;
	aud3004x->playback_on = false;
	aud3004x->capture_on = false;
	aud3004x->amic_mute = AUD3004X_AMIC_MUTE_DEF_MSEC;
	aud3004x->dmic_mute = AUD3004X_DMIC_MUTE_DEF_MSEC;
	aud3004x->ovp_on_setting = AUD3004X_OVP_ON_SETTING_DEF_MSEC;
	aud3004x->ovp_det_delay = AUD3004X_OVP_DET_DEF_MSEC;


	/* initialize workqueue */
	/* initialize workqueue for adc mute handling */
	INIT_DELAYED_WORK(&aud3004x->adc_mute_work, aud3004x_adc_mute_work);
	aud3004x->adc_mute_wq = create_singlethread_workqueue("adc_mute_wq");
	if (aud3004x->adc_mute_wq == NULL) {
		dev_err(codec->dev, "Failed to create adc_mute_wq\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&aud3004x->ovp_on_setting_work, aud3004x_ovp_on_setting_work);
	aud3004x->ovp_on_setting_wq = create_singlethread_workqueue("ovp_on_setting_wq");
	if (aud3004x->ovp_on_setting_wq == NULL) {
		dev_err(codec->dev, "Failed to create ovp_on_setting_wq\n");
		return -ENOMEM;
	}
	
	/* initialize mutex lock */
	mutex_init(&aud3004x->regcache_lock);
	mutex_init(&aud3004x->adc_mute_lock);

	msleep(AUD3004X_PDB_CLK_MSEC);

	/* dt parse for codec */
	aud3004x_i2c_parse_dt(aud3004x);
	/* register value init for codec */
	aud3004x_register_initialize(codec);

	/* dmic bias gpio initialize */
	if (aud3004x->dmic_bias_gpio > 0) {
		if (gpio_request(aud3004x->dmic_bias_gpio, "aud3004x_dmic_bias") < 0)
			dev_err(aud3004x->dev, "%s: Request for %d GPIO failed\n",
					__func__, (int)aud3004x->dmic_bias_gpio);
		if (gpio_direction_output(aud3004x->dmic_bias_gpio, 0) < 0)
			dev_err(aud3004x->dev, "%s: GPIO direction to output failed!\n",
					__func__);
	}

	/* Ignore suspend status for DAPM endpoint */
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "SPKOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "EPOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "HPOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "LINEOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN1L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN2L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN3L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF2 Capture");
	snd_soc_dapm_sync(snd_soc_component_get_dapm(codec));

	dev_info(codec->dev, "%s done. SW Ver: %d\n", __func__, aud3004x->codec_ver);
	aud3004x->is_probe_done = true;

#if IS_ENABLED(CONFIG_SND_SOC_AUD3004X_EXT_ANT)
	aud3004x->is_ext_ant = true;
#else
	aud3004x->is_ext_ant = false;
#endif


#if IS_ENABLED(CONFIG_SND_SOC_AUD3004X_5PIN) || IS_ENABLED(CONFIG_SND_SOC_AUD3004X_6PIN)
	/* Jack probe */
	aud3004x_jack_probe(codec);
#endif
	return 0;
}

static void aud3004x_codec_remove(struct snd_soc_component *codec)
{
	struct aud3004x_priv *aud3004x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "(*) %s called\n", __func__);

	destroy_workqueue(aud3004x->adc_mute_wq);
	destroy_workqueue(aud3004x->ovp_on_setting_wq);

#if IS_ENABLED(CONFIG_SND_SOC_AUD3004X_5PIN) || IS_ENABLED(CONFIG_SND_SOC_AUD3004X_6PIN)
	aud3004x_jack_remove(codec);
#endif
	aud3004x_regulators_disable(codec);
}

static const struct snd_soc_component_driver soc_codec_dev_aud3004x = {
	.probe = aud3004x_codec_probe,
	.remove = aud3004x_codec_remove,
	.controls = aud3004x_snd_controls,
	.num_controls = ARRAY_SIZE(aud3004x_snd_controls),
	.dapm_widgets = aud3004x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(aud3004x_dapm_widgets),
	.dapm_routes = aud3004x_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(aud3004x_dapm_routes),
	.use_pmdown_time = false,
	.idle_bias_on = false,
};

static int aud3004x_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct aud3004x_priv *aud3004x;
	struct device *dev;
	int ret;
	
	pr_info("%s, name: %s, i2c addr: %#x\n", __func__, id->name, (int)i2c->addr);
	
	aud3004x = kzalloc(sizeof(struct aud3004x_priv), GFP_KERNEL);
	if (aud3004x == NULL)
		return -ENOMEM;

	aud3004x->dev = &i2c->dev;
	aud3004x->is_probe_done = false;
	aud3004x->regulator_flag = false;
	aud3004x->regulator_count = 0;

	aud3004x->i2c_priv[AUD3004D] = i2c;
	aud3004x->i2c_priv[AUD3004A] = i2c_new_dummy_device(i2c->adapter, AUD3004X_ANALOG_ADDR);
	aud3004x->i2c_priv[AUD3004O] = i2c_new_dummy_device(i2c->adapter, AUD3004X_OTP_ADDR);

	aud3004x->regmap[AUD3004D] =
		devm_regmap_init(&aud3004x->i2c_priv[AUD3004D]->dev,
				 &aud3004x_dbus, aud3004x, &aud3004x_dregmap);
	if (IS_ERR(aud3004x->regmap[AUD3004D])) {
		dev_err(&i2c->dev, "Failed to allocate digital regmap: %li\n",
				PTR_ERR(aud3004x->regmap[AUD3004D]));
		ret = -ENOMEM;
		goto err;
	}

	aud3004x->regmap[AUD3004A] =
		devm_regmap_init(&aud3004x->i2c_priv[AUD3004A]->dev,
				 &aud3004x_abus, aud3004x, &aud3004x_aregmap);
	if (IS_ERR(aud3004x->regmap[AUD3004A])) {
		dev_err(&i2c->dev, "Failed to allocate analog regmap: %li\n",
				PTR_ERR(aud3004x->regmap[AUD3004A]));
		ret = -ENOMEM;
		goto err;
	}

	aud3004x->regmap[AUD3004O] =
		devm_regmap_init(&aud3004x->i2c_priv[AUD3004O]->dev,
				 &aud3004x_obus, aud3004x, &aud3004x_oregmap);
	if (IS_ERR(aud3004x->regmap[AUD3004O])) {
		dev_err(&i2c->dev, "Failed to allocate otp regmap: %li\n",
				PTR_ERR(aud3004x->regmap[AUD3004O]));
		ret = -ENOMEM;
		goto err;
	}


	i2c_set_clientdata(aud3004x->i2c_priv[AUD3004D], aud3004x);
	i2c_set_clientdata(aud3004x->i2c_priv[AUD3004A], aud3004x);
	i2c_set_clientdata(aud3004x->i2c_priv[AUD3004O], aud3004x);
	dev = &aud3004x->i2c_priv[AUD3004D]->dev;
	ret = snd_soc_register_component(dev, &soc_codec_dev_aud3004x,
			aud3004x_dai, ARRAY_SIZE(aud3004x_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register digital codec: %d\n", ret);
		goto err;
	}

#if IS_ENABLED(CONFIG_PM)
	pm_runtime_enable(aud3004x->dev);
#endif

	return ret;
err:
	kfree(aud3004x);
	return ret;
}

static int aud3004x_i2c_remove(struct i2c_client *i2c)
{
	struct aud3004x_priv *aud3004x = dev_get_drvdata(&i2c->dev);

	dev_info(aud3004x->dev, "(*) %s called\n", __func__);

	snd_soc_unregister_component(&i2c->dev);
	kfree(aud3004x);

	return 0;
}
static const struct i2c_device_id aud3004x_i2c_id[] = {
	{ "aud3004x", 3004 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aud3004x_i2c_id);

const struct of_device_id aud3004x_of_match[] = {
	{ .compatible = "codec,aud3004x", },
	{ },
};

static struct i2c_driver aud3004x_i2c_driver = {
	.driver = {
		.name = "aud3004x",
		.owner = THIS_MODULE,
		.pm = &aud3004x_pm,
		.of_match_table = of_match_ptr(aud3004x_of_match),
	},
	.probe = aud3004x_i2c_probe,
	.remove = aud3004x_i2c_remove,
	.id_table = aud3004x_i2c_id,
};

module_i2c_driver(aud3004x_i2c_driver);

MODULE_DESCRIPTION("ASoC AUD3004X driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:AUD3004X-codec");
