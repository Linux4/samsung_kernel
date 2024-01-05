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

#include <linux/io.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/clock.h>
#include <plat/adc.h>
#include <plat/s5p-clock.h>
#include <linux/exynos_audio.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_jack.h>
#include <mach/regs-pmu.h>
#include <mach/regs-audss.h>

#include <asm/system_info.h>

#include <mach/pmu.h>
#include <mach/exynos4-audio.h>

#include "board-universal4415.h"

#define CODEC_LDO_EN		EXYNOS4_GPK3(1)
#define GPIO_MAIN_MIC_BIAS_EN		EXYNOS4_GPM0(6)

static struct sec_jack_zone universal4415_jack_zones[] = {
	{
		/* adc == 0, unstable zone, default to 3pole if it stays
		 * in this range for 100ms (10ms delays, 10 samples)
		 */
		.adc_high = 0,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 0 < adc <= 1200, unstable zone, default to 3pole if it stays
		 * in this range for 100ms (10ms delays, 10 samples)
		 */
		.adc_high = 1200,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 1129 < adc <= 2200, unstable zone, default to 4pole if it
		 * stays in this range for 100ms (10ms delays, 10 samples)
		 */
		.adc_high = 2200,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_4POLE,
	},
	{
		/* 2200 < adc <= 3000, 4 pole zone, default to 4pole if it
		 * stays in this range for 100ms (10ms delays, 10 samples)
		 */
		.adc_high = 3000,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_4POLE,
	},
	{
		/* adc > 2740, unstable zone, default to 3pole if it stays
		 * in this range for two seconds (10ms delays, 200 samples)
		 */
		.adc_high = 0x7fffffff,
		.delay_ms = 10,
		.check_count = 200,
		.jack_type = SEC_HEADSET_3POLE,
	},
};

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

static struct wm8994_drc_cfg drc_value[] = {
	{
		.name = "voice call DRC",
		.regs[0] = 0x009B,
		.regs[1] = 0x0844,
		.regs[2] = 0x00E8,
		.regs[3] = 0x0210,
		.regs[4] = 0x0000,
	},
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

	.irq_base = IRQ_BOARD_AUDIO_START,

	/* Apply DRC Value */
	.drc_cfgs = drc_value,
	.num_drc_cfgs = ARRAY_SIZE(drc_value),

	/* Support external capacitors*/
	.jd_ext_cap = 1,

	/* Regulated mode at highest output voltage */
	.micbias = {0x2f, 0x2b},

	.micd_lvl_sel = 0xFF,

	.ldo_ena_always_driven = true,

	.lineout1fb = 1,
	.lineout2fb = 0,

	.micdet_delay = 200,
};

static void universal4415_gpio_init(void)
{
	int err;

	/* gpio power-down config */
	s5p_gpio_set_pd_cfg(CODEC_LDO_EN, S5P_GPIO_PD_PREV_STATE);

	err = gpio_request(GPIO_MAIN_MIC_BIAS_EN, "MAIN MIC");
	if (err) {
		pr_err(KERN_ERR "MAIN_MIC_BIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_MAIN_MIC_BIAS_EN, 0);
}

static void universal4415_set_ext_main_mic(int on)
{
	gpio_direction_output(GPIO_MAIN_MIC_BIAS_EN, on);

	pr_info("%s: main_mic bias on = %d\n", __func__, on);
}

struct exynos_sound_platform_data universal4415_sound_pdata __initdata = {
	.set_ext_main_mic = universal4415_set_ext_main_mic,
	.set_mclk = exynos4_audio_set_mclk,
	.dcs_offset_l = -11,
	.dcs_offset_r = -6,

	.zones = universal4415_jack_zones,
	.num_zones = ARRAY_SIZE(universal4415_jack_zones),

	.use_jackdet_type = 1,
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{
		I2C_BOARD_INFO("wm1811", 0x1a),
		.platform_data	= &wm1811_platform_data,
		.irq = IRQ_EINT(14),
	},
};

struct s3c2410_platform_i2c universal4415_i2c_pdata __initdata = {
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 100*1000,
	.sda_delay	= 100,
	.bus_num	= 1,
};

static struct platform_device universal4415_i2s_device = {
	.name   = "universal4415-i2s",
	.id     = -1,
};

static struct platform_device *universal4415_audio_devices[] __initdata = {
	&s3c_device_i2c1,
	&universal4415_i2s_device,
	&wm1811_fixed_voltage0,
	&wm1811_fixed_voltage1,
	&wm1811_fixed_voltage2,
};

void __init exynos4_universal4415_audio_init(void)
{
	exynos4_audio_init();
	universal4415_gpio_init();

	s3c_i2c1_set_platdata(&universal4415_i2c_pdata);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	platform_add_devices(universal4415_audio_devices,
			ARRAY_SIZE(universal4415_audio_devices));

	if (exynos_sound_set_platform_data(&universal4415_sound_pdata))
		pr_err("%s: failed to register sound pdata\n", __func__);
}
