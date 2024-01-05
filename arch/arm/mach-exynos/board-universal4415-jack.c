/* arch/arm/mach-exynos/universal4415-jack.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <mach/gpio-exynos.h>

#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/sec_jack.h>


static void sec_set_jack_micbias(bool on)
{
#ifdef GPIO_EAR_MICBIAS_EN
	gpio_set_value(GPIO_EAR_MICBIAS_EN, on);
#endif
}

static struct sec_jack_zone sec_jack_zones[] = {
	{
		/* 0 <= adc <= 750, unstable zone, default to 3pole if it stays
		 * in this range for 100ms (10ms delays, 10 samples)
		 */
		.adc_high = 750,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 750 < adc, unstable zone, default to 4pole if it
		 * stays in this range for 100ms (10ms delays, 10 samples)
		 */
		.adc_high = 0x7fffffff,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_4POLE,
	},
};

/* To support 3-buttons earjack */
static struct sec_jack_buttons_zone sec_jack_buttons_zones[] = {
	{
		/* 0 <= adc <= 225, stable zone */
		.code = KEY_MEDIA,
		.adc_low = 0,
		.adc_high = 225,
	},
	{
		/* 226 <= adc <= 315, stable zone */
		.code = KEY_VOLUMEUP,
		.adc_low = 226,
		.adc_high = 315,
	},
	{
		/* 316 <= adc <= 540, stable zone */
		.code = KEY_VOLUMEDOWN,
		.adc_low = 316,
		.adc_high = 540,
	},
};

static struct sec_jack_platform_data sec_jack_data = {
	.set_micbias_state = sec_set_jack_micbias,
	.zones = sec_jack_zones,
	.num_zones = ARRAY_SIZE(sec_jack_zones),
	.buttons_zones = sec_jack_buttons_zones,
	.num_buttons_zones = ARRAY_SIZE(sec_jack_buttons_zones),
#ifdef GPIO_EAR_DET_35
	.det_gpio = GPIO_EAR_DET_35,
#endif
#ifdef GPIO_EAR_SEND_END
	.send_end_gpio = GPIO_EAR_SEND_END,
#endif
	.send_end_active_high	= false,
};

static struct platform_device sec_device_jack = {
	.name = "sec_jack",
	.id = 1,		/* will be used also for gpio_event id */
	.dev.platform_data = &sec_jack_data,
};

static void universal4415_jack_gpio_init(void)
{
#ifdef GPIO_EAR_MICBIAS_EN
	/* Ear Microphone BIAS */
	if (gpio_request(GPIO_EAR_MICBIAS_EN, "EAR MIC")) {
		pr_err(KERN_ERR "GPIO_EAR_MIC_BIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_EAR_MICBIAS_EN, 1);
	gpio_set_value(GPIO_EAR_MICBIAS_EN, 0);
#endif
}

void __init exynos4_universal4415_jack_init(void)
{
	universal4415_jack_gpio_init();
	platform_device_register(&sec_device_jack);
}
