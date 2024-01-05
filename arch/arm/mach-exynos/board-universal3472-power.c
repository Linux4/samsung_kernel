
/* linux/arch/arm/mach-exynos/board-universal3472-power.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
#include <linux/mfd/samsung/s2mpu02x.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>
#include <mach/tmu.h>
#include <mach/devfreq.h>

#define UNIVERSAL3472_PMIC_EINT	IRQ_EINT(7)

static struct regulator_consumer_supply s2mpu02_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s2mpu02_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s2mpu02_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s2mpu02_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply s2mpu02_ldo4_consumer =
	REGULATOR_SUPPLY("vqmmc", "dw_mmc.2");

static struct regulator_consumer_supply s2mpu02_ldo7_consumer =
	REGULATOR_SUPPLY("mipi_vdd_1.0", NULL);

static struct regulator_consumer_supply s2mpu02_ldo13_consumer =
	REGULATOR_SUPPLY("tsp_vdd_3.3", NULL);

static struct regulator_consumer_supply s2mpu02_ldo18_consumer =
	REGULATOR_SUPPLY("vmmc", "dw_mmc.2");

static struct regulator_consumer_supply s2mpu02_ldo19_consumer =
	REGULATOR_SUPPLY("3m_core_1v2", NULL);

static struct regulator_consumer_supply s2mpu02_ldo20_consumer =
	REGULATOR_SUPPLY("vt_cam_1v8", NULL);

static struct regulator_consumer_supply s2mpu02_ldo21_consumer =
	REGULATOR_SUPPLY("cam_avdd_2v8", NULL);

static struct regulator_consumer_supply s2mpu02_ldo22_consumer =
	REGULATOR_SUPPLY("cam_io_1v8", NULL);

static struct regulator_consumer_supply s2mpu02_ldo26_consumer =
	REGULATOR_SUPPLY("vdd_lcd_1.8", NULL);

static struct regulator_consumer_supply s2mpu02_ldo27_consumer =
	REGULATOR_SUPPLY("tsp_vdd_1.8", NULL);

static struct regulator_init_data s2m_buck1_data = {
	.constraints    = {
		.name           = "vdd_mif range",
		.min_uV         =  600000,
		.max_uV         = 1400000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.always_on 	= 1,
		.boot_on	= 1,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_buck1_consumer,
};

static struct regulator_init_data s2m_buck2_data = {
	.constraints    = {
		.name           = "vdd_arm range",
		.min_uV         =  600000,
		.max_uV         = 1600000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.always_on 	= 1,
		.boot_on	= 1,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_buck2_consumer,
};

static struct regulator_init_data s2m_buck3_data = {
	.constraints    = {
		.name           = "vdd_int range",
		.min_uV         =  600000,
		.max_uV         = 1200000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.always_on 	= 1,
		.boot_on	= 1,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_buck3_consumer,
};

static struct regulator_init_data s2m_buck4_data = {
	.constraints    = {
		.name           = "vdd_g3d range",
		.min_uV         =  600000,
		.max_uV         = 1400000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.always_on 	= 1,
		.boot_on	= 1,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_buck4_consumer,
};

static struct regulator_init_data s2m_ldo4_data = {
	.constraints	= {
		.name		= "vqmmc2_2.8v_ap range",
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
	.consumer_supplies = &s2mpu02_ldo4_consumer,
};

static struct regulator_init_data s2m_ldo7_data = {
	.constraints    = {
		.name           = "mipi_vdd_1.0",
		.min_uV         = 900000,
		.max_uV         = 1250000,
		.apply_uV       = 1,
		.always_on      = 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_ldo7_consumer,
};

static struct regulator_init_data s2m_ldo13_data = {
	.constraints    = {
		.name           = "tsp_vdd_3.3",
		.min_uV         = 1600000,
		.max_uV         = 3300000,
		.apply_uV       = 1,
		.always_on      = 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_ldo13_consumer,
};

static struct regulator_init_data s2m_ldo18_data = {
	.constraints	= {
		.name		= "vmmc2_2.8v_ap range",
		.min_uV		= 1800000,
		.max_uV		= 3300000,
		.always_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s2mpu02_ldo18_consumer,
};

static struct regulator_init_data s2m_ldo19_data = {
	.constraints    = {
		.name       = "3m_core_1v2",
		.min_uV     = 1200000,
		.max_uV     = 1200000,
		.apply_uV   = 1,
		.always_on  = 0,
		.boot_on    = 0,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.state_mem  = {
			.enabled    = 1,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies  = &s2mpu02_ldo19_consumer,
};

static struct regulator_init_data s2m_ldo20_data = {
	.constraints    = {
		.name       = "vt_cam_1v8",
		.min_uV     = 1800000,
		.max_uV     = 1800000,
		.apply_uV   = 1,
		.always_on  = 0,
		.boot_on    = 0,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.state_mem  = {
			.enabled    = 1,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies  = &s2mpu02_ldo20_consumer,
};

static struct regulator_init_data s2m_ldo21_data = {
	.constraints    = {
		.name       = "cam_avdd_2v8",
		.min_uV     = 2800000,
		.max_uV     = 2800000,
		.apply_uV   = 1,
		.always_on  = 0,
		.boot_on    = 0,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_ldo21_consumer,
};

static struct regulator_init_data s2m_ldo22_data = {
	.constraints    = {
		.name       = "cam_io_2v8",
		.min_uV     = 1800000,
		.max_uV     = 1800000,
		.apply_uV   = 1,
		.always_on  = 0,
		.boot_on    = 0,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_ldo22_consumer,
};

static struct regulator_init_data s2m_ldo26_data = {
	.constraints    = {
		.name           = "vdd_lcd_1.8",
		.min_uV         = 1600000,
		.max_uV         = 2000000,
		.apply_uV       = 1,
		.always_on      = 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_ldo26_consumer,
};

static struct regulator_init_data s2m_ldo27_data = {
	.constraints    = {
		.name           = "tsp_vdd_1.8",
		.min_uV         = 1600000,
		.max_uV         = 3600000,
		.apply_uV       = 1,
		.always_on      = 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2mpu02_ldo27_consumer,
};

static struct sec_regulator_data exynos_regulators[] = {
	{ S2MPU02X_BUCK1, &s2m_buck1_data},
	{ S2MPU02X_BUCK2, &s2m_buck2_data},
	{ S2MPU02X_BUCK3, &s2m_buck3_data},
	{ S2MPU02X_BUCK4, &s2m_buck4_data},
	{ S2MPU02X_LDO4,  &s2m_ldo4_data},
	{ S2MPU02X_LDO7,  &s2m_ldo7_data},
	{ S2MPU02X_LDO13, &s2m_ldo13_data},
	{ S2MPU02X_LDO18, &s2m_ldo18_data},
	{ S2MPU02X_LDO19, &s2m_ldo19_data},
	{ S2MPU02X_LDO20, &s2m_ldo20_data},
	{ S2MPU02X_LDO21, &s2m_ldo21_data},
	{ S2MPU02X_LDO22, &s2m_ldo22_data},
	{ S2MPU02X_LDO26, &s2m_ldo26_data},
	{ S2MPU02X_LDO27, &s2m_ldo27_data},
};

struct sec_opmode_data s2mpu02_opmode_data[S2MPU02X_REG_MAX] = {
	[S2MPU02X_BUCK1] = {S2MPU02X_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPU02X_BUCK2] = {S2MPU02X_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPU02X_BUCK3] = {S2MPU02X_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPU02X_BUCK4] = {S2MPU02X_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO4 ] = {S2MPU02X_LDO4 , SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO7 ] = {S2MPU02X_LDO7 , SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO13] = {S2MPU02X_LDO13, SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO18] = {S2MPU02X_LDO18, SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO19] = {S2MPU02X_LDO19, SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO20] = {S2MPU02X_LDO20, SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO21] = {S2MPU02X_LDO21, SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO22] = {S2MPU02X_LDO22, SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO26] = {S2MPU02X_LDO26, SEC_OPMODE_STANDBY},
	[S2MPU02X_LDO27] = {S2MPU02X_LDO27, SEC_OPMODE_STANDBY},
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
		.freq_clip_max = 1300 * 1000,
		.temp_level = 80,
	},
	.freq_tab[1] = {
		.freq_clip_max = 1200 * 1000,
		.temp_level = 85,
	},
	.freq_tab[2] = {
		.freq_clip_max = 1000 * 1000,
		.temp_level = 90,
	},
	.freq_tab[3] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 95,
	},
	.freq_tab[4] = {
		.freq_clip_max = 500 * 1000,
		.temp_level = 100,
	},
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 4,
	.freq_tab_count = 5,
	.type = SOC_ARCH_EXYNOS3,
	.clk_name = "tmu_apbif",
};
#endif

static int sec_cfg_irq(void)
{
	unsigned int pin = irq_to_gpio(UNIVERSAL3472_PMIC_EINT);

	s3c_gpio_cfgpin(pin, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);

	return 0;
}

static struct sec_pmic_platform_data exynos3_s2m_pdata = {
	.device_type		= S2MPU02X,
	.irq_base		= IRQ_BOARD_START,
	.num_regulators		= ARRAY_SIZE(exynos_regulators),
	.regulators		= exynos_regulators,
	.cfg_pmic_irq		= sec_cfg_irq,
	.wakeup			= 1,
	.wtsr_smpl		= 1,
	.opmode_data		= s2mpu02_opmode_data,
	.buck1_ramp_delay	= 12,
	.buck2_ramp_delay	= 12,
	.buck3_ramp_delay	= 12,
	.buck4_ramp_delay	= 12,
};

struct s3c2410_platform_i2c i2c_data_s2mpu02 __initdata = {
	.flags          = 0,
	.slave_addr     = 0xCC,
	.frequency      = 400*1000,
	.sda_delay      = 100,
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
		.platform_data	= &exynos3_s2m_pdata,
		.irq		= UNIVERSAL3472_PMIC_EINT,
	},
};

#ifdef CONFIG_PM_DEVFREQ
static struct platform_device exynos3472_mif_devfreq = {
	.name   = "exynos3472-busfreq-mif",
	.id     = -1,
};

static struct platform_device exynos3472_int_devfreq = {
	.name   = "exynos3472-busfreq-int",
	.id     = -1,
};

static struct exynos_devfreq_platdata exynos3472_qos_int_pd __initdata = {
	        .default_qos = 100000,
};

static struct exynos_devfreq_platdata exynos3472_qos_mif_pd __initdata = {
	        .default_qos = 200000,
};
#endif


static struct platform_device *universal3472_power_devices[] __initdata = {
	&s3c_device_i2c0,
#ifdef CONFIG_EXYNOS_THERMAL
	&exynos3472_device_tmu,
#endif
#ifdef CONFIG_PM_DEVFREQ
	&exynos3472_mif_devfreq,
	&exynos3472_int_devfreq,
#endif
};

void __init exynos3_universal3472_power_init(void)
{
	s3c_i2c0_set_platdata(&i2c_data_s2mpu02);

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
#ifdef CONFIG_PM_DEVFREQ
	s3c_set_platdata(&exynos3472_qos_int_pd,
			sizeof(struct exynos_devfreq_platdata),
			&exynos3472_int_devfreq);
	s3c_set_platdata(&exynos3472_qos_mif_pd,
			sizeof(struct exynos_devfreq_platdata),
			&exynos3472_mif_devfreq);
#endif
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_set_platdata(&exynos3_tmu_data);
#endif
	platform_add_devices(universal3472_power_devices,
			ARRAY_SIZE(universal3472_power_devices));
}
