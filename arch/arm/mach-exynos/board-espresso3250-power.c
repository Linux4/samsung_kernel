/* linux/arch/arm/mach-exynos/board-espresso3250-power.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mps14.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>
#include <mach/devfreq.h>
#include <mach/tmu.h>

#define ESPRESSO3250_PMIC_EINT	IRQ_EINT(2)

static struct regulator_consumer_supply s2mps14_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s2mps14_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s2mps14_buck3_consumer[] = {
	REGULATOR_SUPPLY("vdd_int", NULL),
	REGULATOR_SUPPLY("vdd_g3d", NULL),
};

static struct regulator_consumer_supply s2mps14_ldo13_consumer =
	REGULATOR_SUPPLY("vt_cam_sensor_a2v8", NULL);

static struct regulator_consumer_supply s2mps14_ldo15_consumer =
	REGULATOR_SUPPLY("tsp_vdd_3.3v", NULL);

static struct regulator_consumer_supply s2mps14_ldo16_consumer =
	REGULATOR_SUPPLY("vcc_lcd_3.0", NULL);

static struct regulator_consumer_supply s2mps14_ldo17_consumer =
	REGULATOR_SUPPLY("vcc_3.3v_peri", NULL);

static struct regulator_consumer_supply s2mps14_ldo19_consumer =
	REGULATOR_SUPPLY("tsp_vdd_1.8v", NULL);

static struct regulator_consumer_supply s2mps14_ldo20_consumer =
	REGULATOR_SUPPLY("vcc_lcd_1.8", NULL);

static struct regulator_consumer_supply s2mps14_ldo21_consumer =
	REGULATOR_SUPPLY("vt_cam_core_1v8", NULL);

static struct regulator_consumer_supply s2mps14_ldo23_consumer =
	REGULATOR_SUPPLY("vcc_1.8v_peri", NULL);

static struct regulator_init_data s2m_buck1_data = {
	.constraints    = {
		.name           = "vdd_mif range",
		.min_uV         =  600000,
		.max_uV         = 1600000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_buck1_consumer,
};

static struct regulator_init_data s2m_buck2_data = {
	.constraints    = {
		.name           = "vdd_arm range",
		.min_uV         =  600000,
		.max_uV         = 1600000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_buck2_consumer,
};

static struct regulator_init_data s2m_buck3_data = {
	.constraints    = {
		.name           = "vdd_int_g3d range",
		.min_uV         =  600000,
		.max_uV         = 1600000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
	.num_consumer_supplies  = 2,
	.consumer_supplies      = s2mps14_buck3_consumer,
};


static struct regulator_init_data s2m_ldo13_data = {
	.constraints    = {
		.name           = "VT Camera Sensor (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.always_on = 1,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_ldo13_consumer,
};


static struct regulator_init_data s2m_ldo15_data = {
	.constraints    = {
		.name           = "tsp_vdd_3.3v",
		.min_uV         = 3300000,
		.max_uV         = 3300000,
		.apply_uV       = 1,
		.always_on      = 0,
		.boot_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_ldo15_consumer,
};

static struct regulator_init_data s2m_ldo16_data = {
	.constraints    = {
		.name           = "vcc_lcd_3.0",
		.min_uV         = 1800000,
		.max_uV         = 3300000,
		.apply_uV       = 1,
		.always_on      = 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_ldo16_consumer,
};

static struct regulator_init_data s2m_ldo17_data = {
	.constraints    = {
		.name           = "vcc_3.3v_peri",
		.min_uV         = 3300000,
		.max_uV         = 3300000,
		.apply_uV       = 1,
		.always_on      = 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_ldo17_consumer,
};

static struct regulator_init_data s2m_ldo19_data = {
	.constraints    = {
		.name           = "tsp_vdd_1.8v",
		.min_uV         = 1800000,
		.max_uV         = 1800000,
		.apply_uV       = 1,
		.always_on      = 0,
		.boot_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_ldo19_consumer,
};

static struct regulator_init_data s2m_ldo20_data = {
	.constraints    = {
		.name           = "vcc_lcd_1.8",
		.min_uV         = 800000,
		.max_uV         = 2375000,
		.apply_uV       = 1,
		.always_on      = 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_ldo20_consumer,
};

static struct regulator_init_data s2m_ldo21_data = {
	.constraints	= {
		.name		= "VT Camera Core (1.8V)",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2mps14_ldo21_consumer,
};

static struct regulator_init_data s2m_ldo23_data = {
	.constraints    = {
		.name           = "vcc_1.8v_peri",
		.min_uV         = 1800000,
		.max_uV         = 1800000,
		.apply_uV       = 1,
		.always_on      = 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mps14_ldo23_consumer,
};

static struct sec_regulator_data exynos_regulators[] = {
	{S2MPS14_BUCK1, &s2m_buck1_data},
	{S2MPS14_BUCK2, &s2m_buck2_data},
	{S2MPS14_BUCK3, &s2m_buck3_data},
	{S2MPS14_LDO13, &s2m_ldo13_data},
	{S2MPS14_LDO15, &s2m_ldo15_data},
	{S2MPS14_LDO16, &s2m_ldo16_data},
	{S2MPS14_LDO17, &s2m_ldo17_data},
	{S2MPS14_LDO19, &s2m_ldo19_data},
	{S2MPS14_LDO20, &s2m_ldo20_data},
	{S2MPS14_LDO21, &s2m_ldo21_data},
	{S2MPS14_LDO23, &s2m_ldo23_data},
};

struct sec_opmode_data s2mps14_opmode_data[S2MPS14_REG_MAX] = {
	[S2MPS14_BUCK1] = {S2MPS14_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPS14_BUCK2] = {S2MPS14_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPS14_BUCK3] = {S2MPS14_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPS14_LDO13] = {S2MPS14_LDO13, SEC_OPMODE_STANDBY},
	[S2MPS14_LDO15] = {S2MPS14_LDO15, SEC_OPMODE_STANDBY},
	[S2MPS14_LDO16] = {S2MPS14_LDO16, SEC_OPMODE_STANDBY},
	[S2MPS14_LDO17] = {S2MPS14_LDO17, SEC_OPMODE_STANDBY},
	[S2MPS14_LDO19] = {S2MPS14_LDO19, SEC_OPMODE_STANDBY},
	[S2MPS14_LDO20] = {S2MPS14_LDO20, SEC_OPMODE_STANDBY},
	[S2MPS14_LDO21] = {S2MPS14_LDO21, SEC_OPMODE_STANDBY},
	[S2MPS14_LDO23] = {S2MPS14_LDO23, SEC_OPMODE_STANDBY},
};

static int sec_cfg_irq(void)
{
	unsigned int pin = irq_to_gpio(ESPRESSO3250_PMIC_EINT);

	s3c_gpio_cfgpin(pin, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);

	return 0;
}

static struct sec_pmic_platform_data exynos3_s2m_pdata = {
	.device_type		= S2MPS14,
	.irq_base		= IRQ_BOARD_START,
	.num_regulators		= ARRAY_SIZE(exynos_regulators),
	.regulators		= exynos_regulators,
	.cfg_pmic_irq		= sec_cfg_irq,
	.wakeup		= 1,
	.wtsr_smpl		= 1,
	.opmode_data		= s2mps14_opmode_data,
	.buck12345_ramp_delay	= 12,
};

#ifdef CONFIG_EXYNOS_THERMAL
static struct exynos_tmu_platform_data exynos3_tmu_data = {
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
	.noise_cancel_mode = 0,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.efuse_value = 55,
	.freq_tab[0] = {
		.freq_clip_max = 900 * 1000,
		.temp_level = 80,
	},
	.freq_tab[1] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 85,
	},
	.freq_tab[2] = {
		.freq_clip_max = 700 * 1000,
		.temp_level = 90,
	},
	.freq_tab[3] = {
		.freq_clip_max = 600 * 1000,
		.temp_level = 95,
	},
	.freq_tab[4] = {
		.freq_clip_max = 500 * 1000,
		.temp_level = 100,
	},
	.size[THERMAL_TRIP_ACTIVE] = 4,
	.size[THERMAL_TRIP_PASSIVE] = 1,
	.freq_tab_count = 5,
	.type = SOC_ARCH_EXYNOS3,
	.clk_name = "tmu_apbif",
};
#endif
struct s3c2410_platform_i2c i2c_data_s2mps14 __initdata = {
	.flags          = 0,
	.slave_addr     = 0xCC,
	.frequency      = 400*1000,
	.sda_delay      = 100,
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
		.platform_data	= &exynos3_s2m_pdata,
		.irq		= ESPRESSO3250_PMIC_EINT,
	},
};
#ifdef CONFIG_BATTERY_SAMSUNG
static struct platform_device samsung_device_battery = {
	.name	= "samsung-fake-battery",
	.id	= -1,
};
#endif

#ifdef CONFIG_PM_DEVFREQ
static struct platform_device exynos3250_mif_devfreq = {
	.name	= "exynos3250-busfreq-mif",
	.id	= -1,
};

static struct exynos_devfreq_platdata espresso3250_qos_int_pd __initdata = {
	/* locking the INT min level to L2 : 133000MHz */
	.default_qos = 133000,
};
static struct platform_device exynos3250_int_devfreq = {
	.name	= "exynos3250-busfreq-int",
	.id	= -1,
};

static struct exynos_devfreq_platdata espresso3250_qos_mif_pd __initdata = {
	/* locking the MIF min level to L0 : 400000MHz */
	.default_qos = 400000,
};
#endif

static struct platform_device *espresso3250_power_devices[] __initdata = {
#ifdef CONFIG_PM_DEVFREQ
	&exynos3250_mif_devfreq,
	&exynos3250_int_devfreq,
#endif
	&s3c_device_i2c0,
#ifdef CONFIG_BATTERY_SAMSUNG
	&samsung_device_battery,
#endif
#ifdef CONFIG_EXYNOS_THERMAL
	&exynos3250_device_tmu,
#endif
};

void __init exynos3_espresso3250_power_init(void)
{
	s3c_i2c0_set_platdata(&i2c_data_s2mps14);

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

#ifdef CONFIG_PM_DEVFREQ
	s3c_set_platdata(&espresso3250_qos_int_pd,
			sizeof(struct exynos_devfreq_platdata),
			&exynos3250_int_devfreq);
	s3c_set_platdata(&espresso3250_qos_mif_pd,
			sizeof(struct exynos_devfreq_platdata),
			&exynos3250_mif_devfreq);
#endif

#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_set_platdata(&exynos3_tmu_data);
#endif
	platform_add_devices(espresso3250_power_devices,
			ARRAY_SIZE(espresso3250_power_devices));
}
