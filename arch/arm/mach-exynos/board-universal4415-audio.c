/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <linux/exynos_audio.h>
#include <linux/regulator/consumer.h>

#include "board-universal4415.h"

#define CODEC_LDO_EN		EXYNOS4_GPK3(1)
#define GPIO_SUB_MIC_BIAS_EN	EXYNOS4_GPF2(0)

static struct regulator *sub_mic_bias_2v8_regulator = NULL;

static struct regulator_consumer_supply wm1811_fixed_voltage0_supplies[] = {
	REGULATOR_SUPPLY("AVDD2", "1-001a"),
	REGULATOR_SUPPLY("CPVDD", "1-001a"),
	REGULATOR_SUPPLY("DBVDD1", "1-001a"),
	REGULATOR_SUPPLY("DBVDD2", "1-001a"),
};

static struct regulator_consumer_supply wm1811_fixed_voltage1_supplies[] = {
	REGULATOR_SUPPLY("SPKVDD1", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "1-001a"),
};

static struct regulator_consumer_supply wm1811_fixed_voltage2_supplies =
	REGULATOR_SUPPLY("DBVDD3", "1-001a");

static struct regulator_init_data wm1811_fixed_voltage0_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm1811_fixed_voltage0_supplies),
	.consumer_supplies	= wm1811_fixed_voltage0_supplies,
};

static struct regulator_init_data wm1811_fixed_voltage1_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm1811_fixed_voltage1_supplies),
	.consumer_supplies	= wm1811_fixed_voltage1_supplies,
};

static struct regulator_init_data wm1811_fixed_voltage2_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm1811_fixed_voltage2_supplies,
};

static struct fixed_voltage_config wm1811_fixed_voltage0_config = {
	.supply_name	= "VDD_1.8V",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &wm1811_fixed_voltage0_init_data,
};

static struct fixed_voltage_config wm1811_fixed_voltage1_config = {
	.supply_name	= "DC_5V",
	.microvolts	= 5000000,
	.gpio		= -EINVAL,
	.init_data	= &wm1811_fixed_voltage1_init_data,
};

static struct fixed_voltage_config wm1811_fixed_voltage2_config = {
	.supply_name	= "VDD_3.3V",
	.microvolts	= 3300000,
	.gpio		= -EINVAL,
	.init_data	= &wm1811_fixed_voltage2_init_data,
};

static struct platform_device wm1811_fixed_voltage0 = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data	= &wm1811_fixed_voltage0_config,
	},
};

static struct platform_device wm1811_fixed_voltage1 = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev		= {
		.platform_data	= &wm1811_fixed_voltage1_config,
	},
};

static struct platform_device wm1811_fixed_voltage2 = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev		= {
		.platform_data	= &wm1811_fixed_voltage2_config,
	},
};

static struct regulator_consumer_supply wm1811_avdd1_supply =
	REGULATOR_SUPPLY("AVDD1", "1-001a");

static struct regulator_consumer_supply wm1811_dcvdd_supply =
	REGULATOR_SUPPLY("DCVDD", "1-001a");

static struct regulator_init_data wm1811_ldo1_data = {
	.constraints	= {
		.name		= "AVDD1",
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm1811_avdd1_supply,
};

static struct regulator_init_data wm1811_ldo2_data = {
	.constraints	= {
		.name		= "DCVDD",
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm1811_dcvdd_supply,
};

static struct wm8994_pdata wm1811_platform_data = {
	/* configure gpio1 function: 0x0001(Logic level input/output) */
	.gpio_defaults[0] = 0x0003,
	/* If the i2s0 and i2s2 is enabled simultaneously */
	.gpio_defaults[7] = 0x8100, /* GPIO8  DACDAT3 in */
	.gpio_defaults[8] = 0x0100, /* GPIO9  ADCDAT3 out */
	.gpio_defaults[9] = 0x0100, /* GPIO10 LRCLK3  out */
	.gpio_defaults[10] = 0x0100,/* GPIO11 BCLK3   out */
	.ldo[0] = { CODEC_LDO_EN, &wm1811_ldo1_data },
	.ldo[1] = { 0, &wm1811_ldo2_data },

	/* .irq_base = IRQ_BOARD_AUDIO_START, */

	/* Support external capacitors*/
	.jd_ext_cap = 1,

	/* Regulated mode at highest output voltage */
	.micbias = {0x2f, 0x2b},

	.micd_lvl_sel = 0xFF,
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{
		I2C_BOARD_INFO("wm1811", 0x1a),
		.platform_data	= &wm1811_platform_data,
		.irq = IRQ_EINT(14),
	},
};

static struct platform_device universal4415_i2s_device = {
        .name   = "universal4415-i2s",
        .id     = -1,
};

static void universal4415_gpio_init(void)
{
	int err;

	/* gpio power-down config */
	s5p_gpio_set_pd_cfg(CODEC_LDO_EN, S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(GPIO_SUB_MIC_BIAS_EN, S5P_GPIO_PD_PREV_STATE);

	/* Sub Microphone BIAS */
	err = gpio_request(GPIO_SUB_MIC_BIAS_EN, "SUB MIC");
	if (err) {
		pr_err(KERN_ERR "SUB_MIC_BIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_SUB_MIC_BIAS_EN, 0);
	gpio_free(GPIO_SUB_MIC_BIAS_EN);
}

static void universal4415_set_ext_sub_mic(int on)
{
	/* Sub Microphone BIAS */
	gpio_set_value(GPIO_SUB_MIC_BIAS_EN, on);

	sub_mic_bias_2v8_regulator = regulator_get(NULL, "sub_mic_bias_2v8");
	if (IS_ERR(sub_mic_bias_2v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "sub_mic_bias_2v8");
	}

	if (on)
		regulator_enable(sub_mic_bias_2v8_regulator);
	else
		regulator_disable(sub_mic_bias_2v8_regulator);

	pr_info("%s: sub_mic bias on = %d\n", __func__, on);
}

struct exynos_sound_platform_data universal4415_sound_pdata __initdata = {
	.set_ext_sub_mic	= universal4415_set_ext_sub_mic,

	.dcs_offset_l = -9,
	.dcs_offset_r = -7,
};

static struct platform_device *universal4415_audio_devices[] __initdata = {
	&s3c_device_i2c1,
#ifdef CONFIG_SND_SAMSUNG_ALP
	&exynos4_device_srp,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos4_device_i2s0,
#endif
	&universal4415_i2s_device,
	&wm1811_fixed_voltage0,
	&wm1811_fixed_voltage1,
	&wm1811_fixed_voltage2,
	&samsung_asoc_dma,
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	&samsung_asoc_idma,
#endif
};

static void universal4415_audio_setup_clocks(void)
{
	struct clk *xusbxti = NULL;
	struct clk *clkout = NULL;

	clk_set_rate(&clk_fout_epll, 192000000);
	clk_enable(&clk_fout_epll);

	xusbxti = clk_get(NULL, "xusbxti");
	if (!xusbxti) {
		pr_err("%s: cannot get xusbxti clock\n", __func__);
		return;
	}

	clkout = clk_get(NULL, "clkout");
	if (!clkout) {
		pr_err("%s: cannot get clkout\n", __func__);
		clk_put(xusbxti);
		return;
	}

	clk_set_parent(clkout, xusbxti);
	clk_put(clkout);
	clk_put(xusbxti);
}

void __init exynos4_universal4415_audio_init(void)
{
	universal4415_audio_setup_clocks();
	universal4415_gpio_init();

	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	platform_add_devices(universal4415_audio_devices,
			ARRAY_SIZE(universal4415_audio_devices));

	pr_info("%s: set sound platform data for universal4415 device\n", __func__);
	if (exynos_sound_set_platform_data(&universal4415_sound_pdata))
		pr_err("%s: failed to register sound pdata\n", __func__);
}
