/* linux/arch/arm/mach-exynos/board-universal_5410-led.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/init.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <mach/irqs.h>
#include <mach/hs-iic.h>

#ifdef CONFIG_MFD_MAX77804K
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77804k-private.h>
#else
#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#endif
#endif
#include <linux/regulator/machine.h>
#ifdef CONFIG_LEDS_MAX77803
#include <linux/leds-max77803.h>
#endif

#include "board-universal5260.h"

#define GPIO_IF_PMIC_SDA	EXYNOS5260_GPB5(0)
#define GPIO_IF_PMIC_SCL	EXYNOS5260_GPB5(1)
#define GPIO_IF_PMIC_IRQ	EXYNOS5260_GPX1(4)

#define GPIO_WPC_INT		EXYNOS5420_GPX2(3)

#ifdef CONFIG_MFD_MAX77803

static struct regulator_consumer_supply safeout1_supply[] = {
	REGULATOR_SUPPLY("safeout1", NULL),
};

static struct regulator_consumer_supply safeout2_supply[] = {
	REGULATOR_SUPPLY("safeout2", NULL),
};

static struct regulator_consumer_supply charger_supply[] = {
	REGULATOR_SUPPLY("vinchg1", "charger-manager.0"),
};

static struct regulator_init_data safeout1_init_data = {
	.constraints	= {
		.name		= "safeout1 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout1_supply),
	.consumer_supplies	= safeout1_supply,
};

static struct regulator_init_data safeout2_init_data = {
	.constraints	= {
		.name		= "safeout2 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 0,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout2_supply),
	.consumer_supplies	= safeout2_supply,
};

static struct regulator_init_data charger_init_data = {
	.constraints	= {
		.name		= "CHARGER",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
		REGULATOR_CHANGE_CURRENT,
		.boot_on	= 1,
		.min_uA		= 60000,
		.max_uA		= 2580000,
	},
	.num_consumer_supplies	= ARRAY_SIZE(charger_supply),
	.consumer_supplies	= charger_supply,
};

struct max77803_regulator_data max77803_regulators[] = {
	{MAX77803_ESAFEOUT1, &safeout1_init_data,},
	{MAX77803_ESAFEOUT2, &safeout2_init_data,},
	{MAX77803_CHARGER, &charger_init_data,},
};

static bool is_muic_default_uart_path_cp(void)
{
	return false;
}

#ifdef CONFIG_LEDS_MAX77803
static struct max77803_led_platform_data max77803_led_pdata = {

	.num_leds = 2,

	.leds[0].name = "leds-sec1",
	.leds[0].id = MAX77803_FLASH_LED_1,
	.leds[0].timer = MAX77803_FLASH_TIME_187P5MS,
	.leds[0].timer_mode = MAX77803_TIMER_MODE_MAX_TIMER,
	.leds[0].cntrl_mode = MAX77803_LED_CTRL_BY_FLASHSTB,
	.leds[0].brightness = 0x3D,

	.leds[1].name = "torch-sec1",
	.leds[1].id = MAX77803_TORCH_LED_1,
	.leds[1].cntrl_mode = MAX77803_LED_CTRL_BY_FLASHSTB,
	.leds[1].brightness = 0x06,
};

#endif

#ifdef CONFIG_VIBETONZ
static struct max77803_haptic_platform_data max77803_haptic_pdata = {
	.max_timeout = 10000,
#ifdef CONFIG_MFD_MAX77804K
	.duty = 37920,
	.period = 38485,	// M2A
#else
	.duty = 31317,
	.period = 33674,	// Fresco
#endif
	.reg2 = MOTOR_LRA | EXT_PWM | DIVIDER_128,
	.init_hw = NULL,
	.motor_en = NULL,
	.pwm_id = 0,
	.regulator_name = "vcc_3.0v_motor",
};
#endif

struct max77803_platform_data exynos4_max77803_info = {
	.irq_base	= IRQ_BOARD_IFIC_START,
	.irq_gpio	= GPIO_IF_PMIC_IRQ,
	.wc_irq_gpio	= GPIO_WPC_INT,
	.wakeup		= 1,
	.muic = &max77803_muic,
	.is_default_uart_path_cp =  is_muic_default_uart_path_cp,
	.regulators = max77803_regulators,
	.num_regulators = MAX77803_REG_MAX,
#ifdef CONFIG_VIBETONZ
	.haptic_data = &max77803_haptic_pdata,
#endif

#ifdef CONFIG_LEDS_MAX77803
	.led_data = &max77803_led_pdata,
#endif
#ifdef CONFIG_CHARGER_MAX77803
	.charger_data	= &sec_battery_pdata,
#endif
};

static struct i2c_board_info i2c_devs8_emul[] __initdata = {
	{
		I2C_BOARD_INFO("max77803", (0xCC >> 1)),
		.platform_data	= &exynos4_max77803_info,
	}
};

#if 0
static struct i2c_gpio_platform_data gpio_i2c_data8 = {
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
};

struct platform_device s3c_device_i2c8 = {
	.name = "i2c-gpio",
	.id = 8,
	.dev.platform_data = &gpio_i2c_data8,
};
#endif

static struct platform_device *universal5260_mfd_device[] __initdata = {
	&s3c_device_i2c4,
};

#endif

void __init exynos5_universal5260_mfd_init(void)
{
#ifdef CONFIG_MFD_MAX77803
	i2c_register_board_info(8, i2c_devs8_emul,
				ARRAY_SIZE(i2c_devs8_emul));
        s3c_i2c4_set_platdata(NULL);
	platform_add_devices(universal5260_mfd_device,
			ARRAY_SIZE(universal5260_mfd_device));
#endif
}
