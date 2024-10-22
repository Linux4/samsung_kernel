// SPDX-License-Identifier: GPL-2.0
//
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author: Light Hsieh <light.hsieh@mediatek.com>
 *
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "pinctrl-mtk-common-v2.h"
#include "pinctrl-paris.h"

#define MTK_SIMPLE_PIN(_number, _name, ...) {		\
		.number = _number,			\
		.name = _name,				\
		.funcs = (struct mtk_func_desc[]){	\
			__VA_ARGS__, { } },		\
	}

static const struct mtk_pin_desc mtk_pins_mt6363[] = {
	MTK_SIMPLE_PIN(1, "GPIO1", MTK_FUNCTION(0, "GPIO1")),
	MTK_SIMPLE_PIN(2, "GPIO2", MTK_FUNCTION(0, "GPIO2")),
	MTK_SIMPLE_PIN(3, "GPIO3", MTK_FUNCTION(0, "GPIO3")),
	MTK_SIMPLE_PIN(4, "GPIO4", MTK_FUNCTION(0, "GPIO4")),
	MTK_SIMPLE_PIN(5, "GPIO5", MTK_FUNCTION(0, "GPIO5")),
	MTK_SIMPLE_PIN(6, "GPIO6", MTK_FUNCTION(0, "GPIO6")),
	MTK_SIMPLE_PIN(7, "GPIO7", MTK_FUNCTION(0, "GPIO7")),
	MTK_SIMPLE_PIN(8, "GPIO8", MTK_FUNCTION(0, "GPIO8")),
	MTK_SIMPLE_PIN(9, "GPIO9", MTK_FUNCTION(0, "GPIO9")),
	MTK_SIMPLE_PIN(0xFFFFFFFF, "DUMMY", MTK_FUNCTION(0, NULL)),
};

#define PIN_SIMPLE_FIELD_BASE(pin, addr, bit, width) {		\
		.s_pin = pin,					\
		.s_addr = addr,					\
		.s_bit = bit,					\
		.x_bits = width					\
	}

static const struct mtk_pin_field_calc mt6363_pin_smt_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x22, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x23, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x23, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x23, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x23, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x23, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x23, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x23, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x23, 7, 1),
};

static const struct mtk_pin_field_calc mt6363_pin_drv_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x28, 5, 2),
	PIN_SIMPLE_FIELD_BASE(2, 0x29, 1, 2),
	PIN_SIMPLE_FIELD_BASE(3, 0x29, 5, 2),
	PIN_SIMPLE_FIELD_BASE(4, 0x2a, 1, 2),
	PIN_SIMPLE_FIELD_BASE(5, 0x2a, 5, 2),
	PIN_SIMPLE_FIELD_BASE(6, 0x2b, 1, 2),
	PIN_SIMPLE_FIELD_BASE(7, 0x2b, 5, 2),
	PIN_SIMPLE_FIELD_BASE(8, 0x2c, 1, 2),
	PIN_SIMPLE_FIELD_BASE(9, 0x2c, 5, 2),
};

static const struct mtk_pin_field_calc mt6363_pin_sr_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x28, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x29, 3, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x29, 7, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x2a, 3, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x2a, 7, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x2b, 3, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x2b, 7, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x2c, 3, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x2c, 7, 1),
};

static const struct mtk_pin_field_calc mt6363_pin_dir_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x88, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x8b, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x8b, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x8b, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x8b, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x8b, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x8b, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x8b, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x8b, 7, 1),
};

static const struct mtk_pin_field_calc mt6363_pin_pullen_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x8e, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x91, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x91, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x91, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x91, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x91, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x91, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x91, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x91, 7, 1),
};

static const struct mtk_pin_field_calc mt6363_pin_pullsel_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x94, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x97, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x97, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x97, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x97, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x97, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x97, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x97, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x97, 7, 1),
};

static const struct mtk_pin_field_calc mt6363_pin_do_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0xa0, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0xa3, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0xa3, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0xa3, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0xa3, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0xa3, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0xa3, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0xa3, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xa3, 7, 1),
};

static const struct mtk_pin_field_calc mt6363_pin_di_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0xa6, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0xa7, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0xa7, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0xa7, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0xa7, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0xa7, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0xa7, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0xa7, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xa7, 7, 1),
};

static const struct mtk_pin_field_calc mt6363_pin_mode_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0xb3, 3, 3),
	PIN_SIMPLE_FIELD_BASE(2, 0xb6, 0, 3),
	PIN_SIMPLE_FIELD_BASE(3, 0xb6, 3, 3),
	PIN_SIMPLE_FIELD_BASE(4, 0xb9, 0, 3),
	PIN_SIMPLE_FIELD_BASE(5, 0xb9, 3, 3),
	PIN_SIMPLE_FIELD_BASE(6, 0xbc, 0, 3),
	PIN_SIMPLE_FIELD_BASE(7, 0xbc, 3, 3),
	PIN_SIMPLE_FIELD_BASE(8, 0xbf, 0, 3),
	PIN_SIMPLE_FIELD_BASE(9, 0xbf, 3, 3),
};

static const struct mtk_pin_field_calc mt6363_pin_ad_switch_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x148, 0, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x148, 1, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x148, 2, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x148, 3, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x148, 4, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x148, 5, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x148, 6, 1),
};

#define MTK_RANGE(_a)		{ .range = (_a), .nranges = ARRAY_SIZE(_a), }

static const struct mtk_pin_reg_calc mt6363_reg_cals[PINCTRL_PIN_REG_MAX] = {
	[PINCTRL_PIN_REG_MODE] = MTK_RANGE(mt6363_pin_mode_range),
	[PINCTRL_PIN_REG_DIR] = MTK_RANGE(mt6363_pin_dir_range),
	[PINCTRL_PIN_REG_DI] = MTK_RANGE(mt6363_pin_di_range),
	[PINCTRL_PIN_REG_DO] = MTK_RANGE(mt6363_pin_do_range),
	[PINCTRL_PIN_REG_SMT] = MTK_RANGE(mt6363_pin_smt_range),
	[PINCTRL_PIN_REG_PULLEN] = MTK_RANGE(mt6363_pin_pullen_range),
	[PINCTRL_PIN_REG_PULLSEL] = MTK_RANGE(mt6363_pin_pullsel_range),
	[PINCTRL_PIN_REG_DRV] = MTK_RANGE(mt6363_pin_drv_range),
	[PINCTRL_PIN_REG_SR] = MTK_RANGE(mt6363_pin_sr_range),
	[PINCTRL_PIN_REG_AD_SWITCH] = MTK_RANGE(mt6363_pin_ad_switch_range),
};

#include <linux/regmap.h>
static void mt6363_reg_lock(struct mtk_pinctrl *hw, int enable)
{
	struct regmap *pinctrl_regmap;

	pinctrl_regmap = (struct regmap *)hw->base[0];
	if (enable) {
		(void)regmap_write(pinctrl_regmap, 0x39e, 0x0);
		(void)regmap_write(pinctrl_regmap, 0x39f, 0x0);
	} else {
		(void)regmap_write(pinctrl_regmap, 0x39e, 0x9c);
		(void)regmap_write(pinctrl_regmap, 0x39f, 0x9c);
	}
}

static const struct mtk_pin_soc mt6363_data = {
	.reg_cal = mt6363_reg_cals,
	.pins = mtk_pins_mt6363,
	.npins = 10,
	.ngrps = 10,
	.nfuncs = 2,
	.gpio_m = 0,
	.capability_flags = FLAG_GPIO_START_IDX_1,
	.reg_lock = mt6363_reg_lock,
};

static int mt6363_pinctrl_probe(struct platform_device *pdev)
{
	return mt63xx_pinctrl_probe(pdev, &mt6363_data);
}

static const struct of_device_id mt6363_pinctrl_of_match[] = {
	{ .compatible = "mediatek,mt6363-pinctrl", },
	{ }
};

static struct platform_driver mt6363_pinctrl_driver = {
	.driver = {
		.name = "mt6363-pinctrl",
		.of_match_table = mt6363_pinctrl_of_match,
	},
	.probe = mt6363_pinctrl_probe,
};
module_platform_driver(mt6363_pinctrl_driver);

MODULE_AUTHOR("Light Hsieh <light.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MT6363 Pinctrl driver");
MODULE_LICENSE("GPL v2");
