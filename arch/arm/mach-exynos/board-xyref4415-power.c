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
#include <linux/bq24160_charger.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s5m8767.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_data/exynos_thermal.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>
#include <mach/tmu.h>
#include <mach/devfreq.h>

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

static struct regulator_consumer_supply s5m8767_ldo18_consumer =
	REGULATOR_SUPPLY("vqmmc", "dw_mmc.2");

static struct regulator_consumer_supply s5m8767_ldo19_consumer =
	REGULATOR_SUPPLY("main_cam_af_2v8", NULL);

static struct regulator_consumer_supply s5m8767_ldo20_consumer =
	REGULATOR_SUPPLY("main_cam_sensor_a2v8", NULL);

static struct regulator_consumer_supply s5m8767_ldo21_consumer =
	REGULATOR_SUPPLY("vt_cam_sensor_a2v8", NULL);

static struct regulator_consumer_supply s5m8767_ldo22_consumer =
	REGULATOR_SUPPLY("tsp_avdd_3.3v", NULL);

static struct regulator_consumer_supply s5m8767_ldo23_consumer =
	REGULATOR_SUPPLY("vmmc", "dw_mmc.2");

static struct regulator_consumer_supply s5m8767_ldo24_consumer =
	REGULATOR_SUPPLY("vcc_lcd_3.0", NULL);

static struct regulator_consumer_supply s5m8767_ldo25_consumer =
	REGULATOR_SUPPLY("main_cam_core_1v2", NULL);

static struct regulator_consumer_supply s5m8767_ldo26_consumer =
	REGULATOR_SUPPLY("main_cam_io_1v8", NULL);

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

static struct regulator_init_data s5m8767_ldo18_data = {
	.constraints	= {
		.name		= "vmmc2_2.8v_ap range",
		.min_uV		= 1800000,
		.max_uV		= 3300000,
		.always_on = 0,
		.boot_on = 0,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
			.enabled	= 0,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo18_consumer,
};

static struct regulator_init_data s5m8767_ldo19_data = {
	.constraints	= {
		.name		= "Main Camera AF (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo19_consumer,
};

static struct regulator_init_data s5m8767_ldo20_data = {
	.constraints	= {
		.name		= "Main Camera Sensor (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo20_consumer,
};

static struct regulator_init_data s5m8767_ldo21_data = {
	.constraints	= {
		.name		= "VT Camera Sensor (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo21_consumer,
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

static struct regulator_init_data s5m8767_ldo23_data = {
	.constraints	= {
		.name		= "vdd_ldo23 range",
		.min_uV		= 1800000,
		.max_uV		= 2850000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on	= 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_ldo23_consumer,
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

static struct regulator_init_data s5m8767_ldo25_data = {
	.constraints	= {
		.name		= "Main Camera Core (1.2V)",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo25_consumer,
};

static struct regulator_init_data s5m8767_ldo26_data = {
	.constraints	= {
		.name		= "Camera I/O (1.8V)",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.always_on = 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo26_consumer,
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
	{S5M8767_LDO18, &s5m8767_ldo18_data},
	{S5M8767_LDO19, &s5m8767_ldo19_data},
	{S5M8767_LDO20, &s5m8767_ldo20_data},
	{S5M8767_LDO21, &s5m8767_ldo21_data},
	{S5M8767_LDO22, &s5m8767_ldo22_data},
	{S5M8767_LDO23, &s5m8767_ldo23_data},
	{S5M8767_LDO24, &s5m8767_ldo24_data},
	{S5M8767_LDO25, &s5m8767_ldo25_data},
	{S5M8767_LDO26, &s5m8767_ldo26_data},
	{S5M8767_LDO27, &s5m8767_ldo27_data},
	{S5M8767_LDO28, &s5m8767_ldo28_data},
};

struct sec_opmode_data s5m8767_opmode_data[S5M8767_REG_MAX] = {
	[S5M8767_BUCK1] = {S5M8767_BUCK1, SEC_OPMODE_STANDBY},
	[S5M8767_BUCK2] = {S5M8767_BUCK2, SEC_OPMODE_STANDBY},
	[S5M8767_BUCK3] = {S5M8767_BUCK3, SEC_OPMODE_STANDBY},
	[S5M8767_BUCK4] = {S5M8767_BUCK4, SEC_OPMODE_STANDBY},
	[S5M8767_LDO18] = {S5M8767_LDO18, SEC_OPMODE_STANDBY},
	[S5M8767_LDO19] = {S5M8767_LDO19, SEC_OPMODE_STANDBY},
	[S5M8767_LDO20] = {S5M8767_LDO20, SEC_OPMODE_STANDBY},
	[S5M8767_LDO21] = {S5M8767_LDO21, SEC_OPMODE_STANDBY},
	[S5M8767_LDO22] = {S5M8767_LDO22, SEC_OPMODE_STANDBY},
	[S5M8767_LDO23] = {S5M8767_LDO23, SEC_OPMODE_STANDBY},
	[S5M8767_LDO24] = {S5M8767_LDO24, SEC_OPMODE_STANDBY},
	[S5M8767_LDO25] = {S5M8767_LDO25, SEC_OPMODE_STANDBY},
	[S5M8767_LDO26] = {S5M8767_LDO26, SEC_OPMODE_STANDBY},
	[S5M8767_LDO27] = {S5M8767_LDO27, SEC_OPMODE_STANDBY},
	[S5M8767_LDO28] = {S5M8767_LDO28, SEC_OPMODE_STANDBY},
};

#define GPIO_CHG_INT		EXYNOS4_GPX0(0)
static void xyref4415_chg_init(void)
{
	int gpio;

	gpio = GPIO_CHG_INT;
	if (gpio_request(gpio, "CHG_INT")) {
		pr_err("%s : CHG_INT request port erron", __func__);
	} else {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	}
}

struct bq24160_platform_data bq24160_platform_data = {
	.name = BQ24160_NAME,
	.support_boot_charging = 1,
};

static struct i2c_board_info i2c_devs_chg[] __initdata = {
	{
		I2C_BOARD_INFO(BQ24160_NAME, 0xD6 >> 1),
		.irq		= IRQ_EINT(0),
		.platform_data	= &bq24160_platform_data,
	},
};

static struct i2c_gpio_platform_data gpio_i2c_chg = {
	.sda_pin = EXYNOS4_GPF0(4),
	.scl_pin = EXYNOS4_GPF0(5),
};

struct platform_device s3c_device_i2c_chg = {
	.name = "i2c-gpio",
	.id = 17,
	.dev.platform_data = &gpio_i2c_chg,
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

#ifdef CONFIG_EXYNOS_THERMAL
static struct exynos_tmu_platform_data exynos4415_tmu_data = {
	.trigger_levels[0] = 80,
	.trigger_levels[1] = 85,
	.trigger_levels[2] = 100,
	.trigger_levels[3] = 110,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 1,
	.gain = 8,
	.reference_voltage = 16,
	.noise_cancel_mode = 4,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.efuse_value = 55,
	.freq_tab[0] = {
		.freq_clip_max = 1400 * 1000,
		.temp_level = 80,
	},
	.freq_tab[1] = {
		.freq_clip_max = 1300 * 1000,
		.temp_level = 85,
	},
	.freq_tab[2] = {
		.freq_clip_max = 1200 * 1000,
		.temp_level = 90,
	},
	.freq_tab[3] = {
		.freq_clip_max = 1100 * 1000,
		.temp_level = 95,
	},
	.freq_tab[4] = {
		.freq_clip_max = 1000 * 1000,
		.temp_level = 100,
	},
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 4,
	.freq_tab_count = 5,
	.type = SOC_ARCH_EXYNOS4,
	.clk_name = "tmu_apbif",
};
#endif

#ifdef CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ
static struct platform_device exynos4415_int_devfreq = {
	.name	= "exynos4415-busfreq-int",
	.id	= -1,
};

static struct exynos_devfreq_platdata smdk5420_qos_int_pd __initdata = {
	/* locking the INT min level to L2 : 133000MHz */
	.default_qos = 100000,
};

static struct platform_device exynos4415_mif_devfreq = {
	.name	= "exynos4415-busfreq-mif",
	.id	= -1,
};

static struct exynos_devfreq_platdata smdk4415_qos_mif_pd __initdata = {
	/* locking the MIF min level to L3 : 206000MHz */
	.default_qos = 103000,
};
#endif

static struct platform_device *xyref4415_power_devices[] __initdata = {
	&s3c_device_i2c0,
#ifdef CONFIG_BATTERY_SAMSUNG
	&samsung_device_battery,
#endif
#ifdef CONFIG_EXYNOS_THERMAL
	&exynos4415_device_tmu,
#endif
#ifdef CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ
	&exynos4415_mif_devfreq,
	&exynos4415_int_devfreq,
#endif
	&s3c_device_i2c_chg,
};

void __init exynos4_xyref4415_power_init(void)
{
	s3c_i2c0_set_platdata(NULL);

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(17, i2c_devs_chg, ARRAY_SIZE(i2c_devs_chg));
	xyref4415_chg_init();

#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_set_platdata(&exynos4415_tmu_data);
#endif
#ifdef CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ
	s3c_set_platdata(&smdk5420_qos_int_pd,
			sizeof(struct exynos_devfreq_platdata),
			&exynos4415_int_devfreq);
	s3c_set_platdata(&smdk4415_qos_mif_pd,
			sizeof(struct exynos_devfreq_platdata),
			&exynos4415_mif_devfreq);

#endif
	platform_add_devices(xyref4415_power_devices,
			ARRAY_SIZE(xyref4415_power_devices));
}
