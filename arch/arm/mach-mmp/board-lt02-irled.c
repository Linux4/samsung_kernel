/* arch/arm/mach-mmp/board-lt02-irled.c
 *
 * Copyright (C) 2013 Samsung Electronics Co, Ltd.
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

#ifdef CONFIG_SEC_IRLED

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_irled.h>

#include <mach/mfp-pxa986-lt02.h>

enum {
	GPIO_IRDA_EN = 0,
	GPIO_IRDA_WAKE,
	GPIO_IRDA_IRQ,
};

static struct gpio irled_gpios[] = {
	[GPIO_IRDA_EN] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "IRDA_EN",
		.gpio = mfp_to_gpio(GPIO002_GPIO_2),
	},
	[GPIO_IRDA_WAKE] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "IRDA_WAKE",
		.gpio = mfp_to_gpio(GPIO004_GPIO_4),
	},
	[GPIO_IRDA_IRQ] = {
		.flags = GPIOF_IN,
		.label = "IRDA_IRQ",
		.gpio = mfp_to_gpio(GPIO091_GPIO_91),
	},
};

static void irda_wake_en(bool onoff)
{
	gpio_direction_output(irled_gpios[GPIO_IRDA_WAKE].gpio, onoff);
}

static void irda_vdd_onoff(bool onoff)
{
	int ret = 0;
	static struct regulator *irled_vdd;

	if (unlikely(!irled_vdd)) {
		irled_vdd = regulator_get(NULL, "vled_ic");
		if (IS_ERR(irled_vdd)) {
			pr_err("irled: could not get irled_vdd regulator\n");
			return;
		}
		regulator_set_voltage(irled_vdd, 1900000, 1900000);
	}

	if (onoff) {
		gpio_direction_output(irled_gpios[GPIO_IRDA_EN].gpio, onoff);
		ret = regulator_enable(irled_vdd);
		if (ret) {
			pr_err("irled: failed to enable irled_vdd regulator\n");
			return;
		}
		mdelay(1);
	} else {
		gpio_direction_output(irled_gpios[GPIO_IRDA_EN].gpio, onoff);
		if (regulator_is_enabled(irled_vdd))
			ret = regulator_disable(irled_vdd);

		if (ret) {
			pr_err("irled: failed to disable irled_vdd regulator\n");
			return;
		}
	}
}

static bool irda_irq_state(void)
{
	return !!gpio_get_value(irled_gpios[GPIO_IRDA_IRQ].gpio);
}

static struct sec_irled_platform_data mc96_pdata = {
	.ir_wake_en = irda_wake_en,
	.ir_vdd_onoff = irda_vdd_onoff,
	.ir_irq_state = irda_irq_state,
};

static struct i2c_board_info __initdata lt02_irled_boardinfo[] = {
	{
		I2C_BOARD_INFO("mc96", 0x50),
		.platform_data = &mc96_pdata,
	},
};

static void __init lt02_irled_gpio_init(void)
{
	gpio_request_array(irled_gpios, ARRAY_SIZE(irled_gpios));
}

void __init pxa986_lt02_irled_init(void)
{
	lt02_irled_gpio_init();

	i2c_register_board_info(11, lt02_irled_boardinfo,
				ARRAY_SIZE(lt02_irled_boardinfo));
}

#endif
