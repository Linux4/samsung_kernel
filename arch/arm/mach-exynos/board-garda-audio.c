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
#include <linux/regulator/consumer.h>
#include <linux/mfd/wm8994/pdata.h>

#include <linux/platform_data/es305.h>
#include <linux/exynos_audio.h>
#include <linux/sec_jack.h>

#include <asm/system_info.h>

#include <mach/pmu.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/adc.h>

#include "board-universal222ap.h"


#ifdef CONFIG_SND_SOC_WM8994
#define CODEC_LDO_EN		EXYNOS4_GPM0(7)

#define GPIO_SUB_MIC_BIAS_EN	EXYNOS4_GPM4(6)
#define GPIO_MAIN_MIC_BIAS_EN	EXYNOS4_GPM4(7)
#define GPIO_DOC_SWITCH		EXYNOS4_GPX1(1)
#endif

#ifdef CONFIG_AUDIENCE_ES305
#define GPIO_ES305_WAKEUP	EXYNOS4_GPX0(2)
#define GPIO_ES305_RESET	EXYNOS4_GPC0(1)
#endif


static struct sec_jack_zone universal222ap_jack_zones[] = {
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

static unsigned int mclk_usecnt;
static void universal222ap_mclk_enable(bool enable, bool forced)
{
	struct clk *clkout;

	clkout = clk_get(NULL, "clkout");
	if (!clkout) {
		pr_err("%s: Failed to get clkout\n", __func__);
		return;
	}

	if (!enable && forced) {
		/* forced disable */
		mclk_usecnt = 0;

		clk_disable(clkout);
		exynos_xxti_sys_powerdown(0);
		goto exit;
	}

	if (enable) {
		if (mclk_usecnt == 0) {
			exynos_xxti_sys_powerdown(1);
			clk_enable(clkout);
		}

		mclk_usecnt++;
	} else {
		if (mclk_usecnt == 0)
			goto exit;

		if (--mclk_usecnt > 0)
			goto exit;

		clk_disable(clkout);
		exynos_xxti_sys_powerdown(0);
	}

	pr_info("%s: mclk is %d(count: %d)\n", __func__, enable, mclk_usecnt);

exit:
	clk_put(clkout);
}

#ifdef CONFIG_AUDIENCE_ES305
static struct es305_platform_data es305_pdata = {
	.gpio_wakeup = GPIO_ES305_WAKEUP,
	.gpio_reset = GPIO_ES305_RESET,
	.clk_enable = universal222ap_mclk_enable,
	.passthrough_cmd = {0x00000060, 0x00000074}, /* A to C, B to D */
	.passthrough_num = 2,
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("audience_es305", 0x3e),
		.platform_data = &es305_pdata,
	},
};
#endif

#ifdef CONFIG_SND_SOC_WM8994
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
	.micbias = {0x2f, 0x2f},

	.micd_lvl_sel = 0x87,

	.ldo_ena_always_driven = true,

	.lineout1fb = 1,
	.lineout2fb = 0,

	.micdet_delay = 200,
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{
		I2C_BOARD_INFO("wm1811", 0x1a),
		.platform_data	= &wm1811_platform_data,
		.irq = IRQ_EINT(14),
	},
};

static struct platform_device universal222ap_i2s_device = {
	.name   = "universal222ap-i2s",
	.id     = -1,
};
#endif

static void universal222ap_gpio_init(void)
{
	int err;

	s5p_gpio_set_pd_cfg(CODEC_LDO_EN, S5P_GPIO_PD_PREV_STATE);

	if (system_rev >= 7) {
		/* Main Microphone BIAS */
		err = gpio_request(GPIO_MAIN_MIC_BIAS_EN, "MAIN MIC");
		if (err) {
			pr_err(KERN_ERR "MAIN_MIC_BIAS_EN GPIO set error!\n");
			return;
		}
		gpio_direction_output(GPIO_MAIN_MIC_BIAS_EN, 0);

		/* Sub Microphone BIAS */
		err = gpio_request(GPIO_SUB_MIC_BIAS_EN, "SUB MIC");
		if (err) {
			pr_err(KERN_ERR "SUB_MIC_BIAS_EN GPIO set error!\n");
			return;
		}
		gpio_direction_output(GPIO_SUB_MIC_BIAS_EN, 0);
	}

	err = gpio_request(GPIO_DOC_SWITCH, "DOC SWITCH");
	if (err) {
		pr_err(KERN_ERR "DOC_SWITCH GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_DOC_SWITCH, 0);
}

static void universal222ap_set_lineout_switch(int on)
{
	gpio_direction_output(GPIO_DOC_SWITCH, on);

	pr_info("%s: set lineout switch = %d\n", __func__, on);
}

static void universal222ap_set_ext_main_mic(int on)
{
	gpio_direction_output(GPIO_MAIN_MIC_BIAS_EN, on);

	pr_info("%s: main_mic bias on = %d\n", __func__, on);
}

static void universal222ap_set_ext_sub_mic(int on)
{
	if (system_rev < 7) {
		struct regulator *regulator =
					regulator_get(NULL, "sub_mic_bias_2v8");
		if (IS_ERR(regulator)) {
			pr_err("%s: failed to get %s\n",
						__func__, "sub_mic_bias_2v8");
			return;
		}

		if (on)
			regulator_enable(regulator);
		else
			regulator_disable(regulator);

		regulator_put(regulator);
	} else {
		gpio_direction_output(GPIO_SUB_MIC_BIAS_EN, on);
	}

	pr_info("%s: sub_mic bias on = %d\n", __func__, on);
}

struct exynos_sound_platform_data universal222ap_sound_pdata __initdata = {
	.set_mclk = universal222ap_mclk_enable,
	.set_ext_sub_mic = universal222ap_set_ext_sub_mic,
	.set_lineout_switch = universal222ap_set_lineout_switch,
	.dcs_offset_l = -9,
	.dcs_offset_r = -7,
	.zones = universal222ap_jack_zones,
	.num_zones = ARRAY_SIZE(universal222ap_jack_zones),
};

static struct platform_device *universal222ap_audio_devices[] __initdata = {
	&exynos_device_audss,
#ifdef CONFIG_S3C_DEV_I2C1
	&s3c_device_i2c1,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos4_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&samsung_asoc_dma,
	&samsung_asoc_idma,
#endif
	&exynos4270_device_srp,
#ifdef CONFIG_SND_SOC_WM8994
	&universal222ap_i2s_device,
	&wm1811_fixed_voltage0,
	&wm1811_fixed_voltage1,
	&wm1811_fixed_voltage2,
#endif
};

static void universal222ap_audio_setup_clocks(void)
{
	struct clk *xusbxti;
	struct clk *clkout;

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

void __init exynos4_smdk4270_audio_init(void)
{
	universal222ap_audio_setup_clocks();
	universal222ap_gpio_init();

#ifdef CONFIG_SND_SOC_WM8994
	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
#endif

#ifdef CONFIG_AUDIENCE_ES305
	if (system_rev < 8) {
		s3c_i2c4_set_platdata(NULL);
		i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
	}
#endif

	platform_add_devices(universal222ap_audio_devices,
				ARRAY_SIZE(universal222ap_audio_devices));

	if (system_rev >= 9) {
		universal222ap_sound_pdata.use_jackdet_type = 1;
		universal222ap_sound_pdata.set_ext_main_mic =
						universal222ap_set_ext_main_mic;
	} else
		universal222ap_sound_pdata.use_jackdet_type = 0;

	if (exynos_sound_set_platform_data(&universal222ap_sound_pdata))
		pr_err("%s: failed to register sound pdata\n", __func__);

}
