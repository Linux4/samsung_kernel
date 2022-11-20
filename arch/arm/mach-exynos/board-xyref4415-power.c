/* linux/arch/arm/mach-exynos/board-smdk4x12-power.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s5m8767.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>

#include "board-xyref4415.h"

#define XYREF4415_PMIC_EINT	IRQ_EINT(2)
#define XYREF4415_GPIO_WRST	EXYNOS4_GPF1(0)

static struct regulator_consumer_supply s5m8767_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s5m8767_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s5m8767_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s5m8767_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply s5m8767_ldo22_consumer =
	REGULATOR_SUPPLY("tsp_avdd_3.3v", NULL);

static struct regulator_consumer_supply s5m8767_ldo24_consumer =
	REGULATOR_SUPPLY("vcc_lcd_3.0", NULL);

static struct regulator_consumer_supply s5m8767_ldo27_consumer =
	REGULATOR_SUPPLY("vcc_lcd_1.8", NULL);

static struct regulator_consumer_supply s5m8767_ldo28_consumer =
	REGULATOR_SUPPLY("tsp_avdd_1.8", NULL);

static struct regulator_init_data s5m8767_buck1_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		=  650000,
		.max_uV		= 2200000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck1_consumer,
};

static struct regulator_init_data s5m8767_buck2_data = {
	.constraints	= {
		.name		= "vdd_arm range",
		.min_uV		=  600000,
		.max_uV		= 1600000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck2_consumer,
};

static struct regulator_init_data s5m8767_buck3_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		=  600000,
		.max_uV		= 1600000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.always_on = 1,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck3_consumer,
};

static struct regulator_init_data s5m8767_buck4_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		=  600000,
		.max_uV		= 1600000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.boot_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_buck4_consumer,
};

static struct regulator_init_data s5m8767_ldo22_data = {
	.constraints	= {
		.name		= "tsp_avdd_3.3v range",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo22_consumer,
};

static struct regulator_init_data s5m8767_ldo24_data = {
	.constraints	= {
		.name		= "vcc_lcd_3.0v range",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo24_consumer,
};

static struct regulator_init_data s5m8767_ldo27_data = {
	.constraints	= {
		.name		= "vcc_lcd_1.8v range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo27_consumer,
};

static struct regulator_init_data s5m8767_ldo28_data = {
	.constraints	= {
		.name		= "tsp_avdd_1.8v range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo28_consumer,
};

static struct sec_regulator_data hudson_regulators[] = {
	{S5M8767_BUCK1, &s5m8767_buck1_data},
	{S5M8767_BUCK2, &s5m8767_buck2_data},
	{S5M8767_BUCK3, &s5m8767_buck3_data},
	{S5M8767_BUCK4, &s5m8767_buck4_data},
	{S5M8767_LDO22, &s5m8767_ldo22_data},
	{S5M8767_LDO24, &s5m8767_ldo24_data},
	{S5M8767_LDO27, &s5m8767_ldo27_data},
	{S5M8767_LDO28, &s5m8767_ldo28_data},
};

struct sec_opmode_data s5m8767_opmode_data[S5M8767_REG_MAX] = {
	[S5M8767_BUCK1] = {S5M8767_BUCK1, SEC_OPMODE_STANDBY},
	[S5M8767_BUCK2] = {S5M8767_BUCK2, SEC_OPMODE_STANDBY},
	[S5M8767_BUCK3] = {S5M8767_BUCK3, SEC_OPMODE_STANDBY},
	[S5M8767_BUCK4] = {S5M8767_BUCK4, SEC_OPMODE_STANDBY},
	[S5M8767_LDO22] = {S5M8767_LDO22, SEC_OPMODE_STANDBY},
	[S5M8767_LDO24] = {S5M8767_LDO22, SEC_OPMODE_STANDBY},
	[S5M8767_LDO27] = {S5M8767_LDO22, SEC_OPMODE_STANDBY},
	[S5M8767_LDO28] = {S5M8767_LDO22, SEC_OPMODE_STANDBY},
};

static int sec_cfg_irq(void)
{
	unsigned int pin = irq_to_gpio(XYREF4415_PMIC_EINT);

	s3c_gpio_cfgpin(pin, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);

	return 0;
}

static int sec_cfg_pmic_wrst(void)
{
	unsigned int gpio = XYREF4415_GPIO_WRST;
	unsigned int i;

	for (i = 0; i <= 5; i++) {
		s5p_gpio_set_pd_cfg(gpio + i, S5P_GPIO_PD_PREV_STATE);
	}

	return 0;
}

static struct sec_pmic_platform_data xyref4415_s5m8767_pdata = {
	.device_type		= S5M8767X,
	.irq_base		= IRQ_BOARD_START,
	.num_regulators		= ARRAY_SIZE(hudson_regulators),
	.regulators		= hudson_regulators,
	.cfg_pmic_irq		= sec_cfg_irq,
	.cfg_pmic_wrst		= sec_cfg_pmic_wrst,
	.wakeup			= 1,
	.opmode_data		= s5m8767_opmode_data,
	.wtsr_smpl		= 1,
	.buck_gpios[0]		= EXYNOS4_GPF1(3), /* DVS 1 */
	.buck_gpios[1]		= EXYNOS4_GPF1(4), /* DVS 2 */
	.buck_gpios[2]		= EXYNOS4_GPF1(5), /* DVS 3 */
	.buck_ds[0]		= EXYNOS4_GPF1(0),  /* DS 2 */
	.buck_ds[1]		= EXYNOS4_GPF1(1),  /* DS 3 */
	.buck_ds[2]		= EXYNOS4_GPF1(2),  /* DS 4 */
	.buck_ramp_delay        = 25,
	.buck2_ramp_enable      = true,
	.buck3_ramp_enable      = true,
	.buck4_ramp_enable      = true,
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
		.platform_data = &xyref4415_s5m8767_pdata,
		.irq		= XYREF4415_PMIC_EINT,
	},
};

#ifdef CONFIG_BATTERY_SAMSUNG
static struct platform_device samsung_device_battery = {
	.name	= "samsung-fake-battery",
	.id	= -1,
};
#endif

static struct platform_device *xyref4415_power_devices[] __initdata = {
	&s3c_device_i2c0,
#ifdef CONFIG_BATTERY_SAMSUNG
	&samsung_device_battery,
#endif
};

void __init exynos4_xyref4415_power_init(void)
{
	s3c_i2c0_set_platdata(NULL);

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	platform_add_devices(xyref4415_power_devices,
			ARRAY_SIZE(xyref4415_power_devices));
}
