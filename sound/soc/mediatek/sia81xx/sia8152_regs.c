/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
#define LOG_FLAG	"sia8152_regs"


#include <linux/regmap.h>
#include <linux/device.h>
#include "sia81xx_common.h"
#include "sia81xx_regmap.h"
#include "sia8152_regs.h"

#define SIA8152_WRITEABLE_REG_NUM			(5)

static const char sia8152_palyback_defaults[][SIA8152_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x90,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0xA0,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x90,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0xA0,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	}
};

static const char sia8152_voice_defaults[][SIA8152_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x90,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0xA0,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x90,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0xA0,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	}
};

static const char sia8152_receiver_defaults[][SIA8152_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x40,		//SIA8152_REG_MOD_CFG
				0xD4,		//SIA8152_REG_SYS_EN
				0xA0,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x40,		//SIA8152_REG_MOD_CFG
				0xD4,		//SIA8152_REG_SYS_EN
				0xA0,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	}
};

static const char sia8152_factory_defaults[][SIA8152_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x90,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0xA0,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x90,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0xA0,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	}
};

const struct sia81xx_reg_default_val sia8152_reg_default_val = {
	.chip_type = CHIP_TYPE_SIA8152, 
	.offset = SIA8152_REG_MOD_CFG,
	.reg_defaults[AUDIO_SCENE_PLAYBACK] = {
		.vals = (char *)sia8152_palyback_defaults,
		.num = ARRAY_SIZE(sia8152_palyback_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_VOICE] = {
		.vals = (char *)sia8152_voice_defaults,
		.num = ARRAY_SIZE(sia8152_voice_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_RECEIVER] = {
		.vals = (char *)sia8152_receiver_defaults,
		.num = ARRAY_SIZE(sia8152_receiver_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_FACTORY] = {
		.vals = (char *)sia8152_factory_defaults,
		.num = ARRAY_SIZE(sia8152_factory_defaults[0])
	}
};

static bool sia8152_writeable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8152_REG_MOD_CFG ... 0x11:	// SIA8152_REG_TEST_CFG :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8152_readable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8152_REG_CHIP_ID ... 0x11:	// SIA8152_REG_STATE_FLAG2 :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8152_volatile_register(
	struct device *dev, 
	unsigned int reg)
{
	return true;
}

const struct regmap_config sia8152_regmap_config = {
	.name = "sia8152",
	.reg_bits = 8,
	.val_bits = 8,
	.reg_stride = 0,
	.pad_bits = 0,
	.cache_type = REGCACHE_NONE,
	.reg_defaults = NULL,
	.num_reg_defaults = 0,
	.writeable_reg = sia8152_writeable_register,
	.readable_reg = sia8152_readable_register,
	.volatile_reg = sia8152_volatile_register,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static int sia8152_check_chip_id(
	struct regmap *regmap) 
{
	char val = 0;

	if(0 != sia81xx_regmap_read(regmap, SIA8152_REG_CHIP_ID, 1, &val))
		return -1;

	if((SIA8152_CHIP_ID_VAL & SIA8152_CHIP_ID_MASK) != 
		(val & SIA8152_CHIP_ID_MASK))
		return -1;

	return 0;
}

static void sia8152_chip_on(
	struct regmap *regmap, unsigned int scene)
{
	char val = 0;

	if(0 != sia81xx_regmap_read(regmap, SIA8152_REG_OPC_HCFG, 1, &val))
		return;

	val |= 0x01;
	if(0 != sia81xx_regmap_write(regmap, SIA8152_REG_OPC_HCFG, 1, &val))
		return;
}

static void sia8152_chip_off(
	struct regmap *regmap)
{
	char val = 0;

	if(0 != sia81xx_regmap_write(regmap, SIA8152_REG_OPC_HCFG, 1, &val))
		return;

	val = 0x10;
	if(0 != sia81xx_regmap_write(regmap, SIA8152_REG_MOD_CFG, 1, &val))
		return;

	val = 0x80;
	if(0 != sia81xx_regmap_write(regmap, SIA8152_REG_SYS_EN, 1, &val))
		return;
}

static bool sia8152_get_chip_en(
	struct regmap *regmap)
{
	char val = 0;

	if(0 != sia81xx_regmap_read(regmap, SIA8152_REG_OPC_HCFG, 1, &val))
		return false;

	if (val & 0x01)
		return true;

	return false;
}

const struct sia81xx_opt_if sia8152_opt_if = {
	.check_chip_id = sia8152_check_chip_id,
	.set_xfilter = NULL,
	.chip_on = sia8152_chip_on,
	.chip_off = sia8152_chip_off,
	.get_chip_en = sia8152_get_chip_en,
};

