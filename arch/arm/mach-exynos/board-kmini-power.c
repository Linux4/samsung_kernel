/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/platform_data/exynos_thermal.h>

#include <asm/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/devfreq.h>
#include <mach/tmu.h>
#include <mach/map.h>

#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mpu01.h>
#include <linux/mfd/samsung/s2mpu01a.h>

#include "board-universal222ap.h"

#ifdef CONFIG_MFD_RT5033
#ifdef CONFIG_REGULATOR_RT5033

#include <linux/mfd/rt5033.h>
#if 0
static struct regulator_consumer_supply rt5033_safe_ldo_consumers[] = {
	REGULATOR_SUPPLY("rt5033_safe_ldo",NULL),
};
#endif
static struct regulator_consumer_supply rt5033_ldo_consumers[] = {
	REGULATOR_SUPPLY("cam_af_2v8",NULL),
};

static struct regulator_consumer_supply rt5033_ldo_consumers_rev00[] = {
	REGULATOR_SUPPLY("cam_sensor_a2v8",NULL),
};

static struct regulator_consumer_supply rt5033_buck_consumers[] = {
	REGULATOR_SUPPLY("cam_sensor_core_1v2",NULL),
};
#if 0
static struct regulator_init_data rt5033_safe_ldo_data = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 4950000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
		.always_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_safe_ldo_consumers),
	.consumer_supplies = rt5033_safe_ldo_consumers,
};
#endif
static struct regulator_init_data rt5033_ldo_data = {
	.constraints	= {
		.name		= "CAM_AF_2.8V",
		.min_uV = 2800000,
		.max_uV = 2800000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_ldo_consumers),
	.consumer_supplies = rt5033_ldo_consumers,
};

static struct regulator_init_data rt5033_ldo_data_rev00 = {
	.constraints	= {
		.name		= "CAM_SENSOR_A2.8V",
		.min_uV = 2800000,
		.max_uV = 2800000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_ldo_consumers_rev00),
	.consumer_supplies = rt5033_ldo_consumers_rev00,
};

static struct regulator_init_data rt5033_buck_data = {
	.constraints	= {
		.name		= "CAM_SENSOR_CORE_1.2V",
		.min_uV = 1200000,
		.max_uV = 1200000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_buck_consumers),
	.consumer_supplies = rt5033_buck_consumers,
};

struct rt5033_regulator_platform_data rv_pdata = {
	.regulator = {
#if 0
		[RT5033_ID_LDO_SAFE] = &rt5033_safe_ldo_data,
#endif
		[RT5033_ID_LDO1] = &rt5033_ldo_data,
		[RT5033_ID_DCDC1] = &rt5033_buck_data,
	},
};

struct rt5033_regulator_platform_data rv_pdata_rev00 = {
	.regulator = {
#if 0
		[RT5033_ID_LDO_SAFE] = &rt5033_safe_ldo_data,
#endif
		[RT5033_ID_LDO1] = &rt5033_ldo_data_rev00,
		[RT5033_ID_DCDC1] = &rt5033_buck_data,
	},
};

#endif
#endif

#define SMDK4270_PMIC_EINT	IRQ_EINT(6)

#ifdef CONFIG_REGULATOR_S2MPU01A
static struct regulator_consumer_supply s2m_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s2m_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s2m_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s2m_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);
#if 0 /*main cam core -> if pmic */
static struct regulator_consumer_supply s2m_buck8_consumer =
	REGULATOR_SUPPLY("main_cam_core_1v2", NULL);
#endif
static struct regulator_consumer_supply s2m_ldo2_consumer =
	REGULATOR_SUPPLY("vdd_ldo2", NULL);

static struct regulator_consumer_supply s2m_ldo4_consumer =
	REGULATOR_SUPPLY("vqmmc", "dw_mmc.2");

static struct regulator_consumer_supply s2m_ldo5_consumer =
	REGULATOR_SUPPLY("vdd_pll_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo6_consumer =
	REGULATOR_SUPPLY("vdd_pll_1v0", NULL);

static struct regulator_consumer_supply s2m_ldo7_consumer =
	REGULATOR_SUPPLY("vdd_ldo7", NULL);

static struct regulator_consumer_supply s2m_ldo8_consumer =
	REGULATOR_SUPPLY("vdd_ldo8", NULL);

static struct regulator_consumer_supply s2m_ldo9_consumer =
	REGULATOR_SUPPLY("vdd_ldo9", NULL);

static struct regulator_consumer_supply s2m_ldo26_consumer =
	REGULATOR_SUPPLY("cam_sensor_io_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo27_consumer =
	REGULATOR_SUPPLY("vreg_fem/gps_2v85", NULL);

static struct regulator_consumer_supply s2m_ldo28_consumer =
	REGULATOR_SUPPLY("cam_sensor_a2v8", NULL);

static struct regulator_consumer_supply s2m_ldo28_consumer_rev00 =
	REGULATOR_SUPPLY("cam_af_2v8", NULL);

static struct regulator_consumer_supply s2m_ldo29_consumer =
	REGULATOR_SUPPLY("vlcd_3v0", NULL);

static struct regulator_consumer_supply s2m_ldo30_consumer =
	REGULATOR_SUPPLY("vlcd_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo31_consumer =
	REGULATOR_SUPPLY("vt_sensor_io_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo31_consumer_rev01 =
	REGULATOR_SUPPLY("vdd_mot_3v0", NULL);

static struct regulator_consumer_supply s2m_ldo31_consumer_rev00 =
	REGULATOR_SUPPLY("vt_sensor_io_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo32_consumer =
	REGULATOR_SUPPLY("vtsp_a3v3", NULL);

static struct regulator_consumer_supply s2m_ldo33_consumer =
	REGULATOR_SUPPLY("key_led_3v3", NULL);

static struct regulator_consumer_supply s2m_ldo34_consumer =
	REGULATOR_SUPPLY("led_a_3v0", NULL);

static struct regulator_consumer_supply s2m_ldo35_consumer[] =
{
        REGULATOR_SUPPLY("sensor_2v8", NULL),
	REGULATOR_SUPPLY("light_sensor_2v8", NULL),
	REGULATOR_SUPPLY("proxi_sensor_2v8", NULL),
	REGULATOR_SUPPLY("alps_sensor_2v8", NULL),
};

static struct regulator_consumer_supply s2m_ldo36_consumer =
	REGULATOR_SUPPLY("vt_sensor_io_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo36_consumer_rev00 =
	REGULATOR_SUPPLY("vddi_spi_1v2", NULL);

static struct regulator_consumer_supply s2m_ldo37_consumer =
	REGULATOR_SUPPLY("vdd_tsp_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo38_consumer =
	REGULATOR_SUPPLY("vled_3v3", NULL);

static struct regulator_consumer_supply s2m_ldo39_consumer =
        REGULATOR_SUPPLY("vgps_2.8v", NULL);

static struct regulator_consumer_supply s2m_ldo39_consumer_rev00 =
        REGULATOR_SUPPLY("fm_radio_2.8v", NULL);

static struct regulator_init_data s2m_buck1_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		=  800000,
		.max_uV		= 1300000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_buck1_consumer,
};

static struct regulator_init_data s2m_buck2_data = {
	.constraints	= {
		.name		= "vdd_arm range",
		.min_uV		=  800000,
		.max_uV		= 1400000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_buck2_consumer,
};

static struct regulator_init_data s2m_buck3_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		=  800000,
		.max_uV		= 1400000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_buck3_consumer,
};

static struct regulator_init_data s2m_buck4_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		=  800000,
		.max_uV		= 1400000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_buck4_consumer,
};

static struct regulator_init_data s2m_ldo2_data = {
	.constraints	= {
		.name		= "vdd_ldo2 range",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo2_consumer,
};

static struct regulator_init_data s2m_ldo4_data = {
	.constraints	= {
		.name		= "vddq_mmc range",
		.min_uV		= 1800000,
		.max_uV		= 3300000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo4_consumer,
};

static struct regulator_init_data s2m_ldo5_data = {
	.constraints	= {
		.name		= "vdd_pll_1v8",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo5_consumer,
};

static struct regulator_init_data s2m_ldo6_data = {
	.constraints	= {
		.name		= "vdd_pll_1v0",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo6_consumer,
};

static struct regulator_init_data s2m_ldo7_data = {
	.constraints	= {
		.name		= "vdd_ldo7 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo7_consumer,
};

static struct regulator_init_data s2m_ldo8_data = {
	.constraints	= {
		.name		= "vdd_ldo8 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo8_consumer,
};

static struct regulator_init_data s2m_ldo9_data = {
	.constraints	= {
		.name		= "vdd_ldo9",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo9_consumer,
};

static struct regulator_init_data s2m_ldo26_data = {
	.constraints	= {
		.name		= "CAM_SENSOR_IO_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo26_consumer,
};

static struct regulator_init_data s2m_ldo27_data = {
	.constraints	= {
		.name		= "GPS_2.85V",
		.min_uV		= 2850000,
		.max_uV		= 2850000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo27_consumer,
};

static struct regulator_init_data s2m_ldo28_data = {
	.constraints	= {
		.name		= "CAM_SENSOR_A2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo28_consumer,
};

static struct regulator_init_data s2m_ldo28_data_rev00 = {
	.constraints	= {
		.name		= "CAM_AF_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo28_consumer_rev00,
};


static struct regulator_init_data s2m_ldo29_data = {
	.constraints	= {
		.name		= "VLCD_3.0V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.boot_on 	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo29_consumer,
};

static struct regulator_init_data s2m_ldo30_data = {
	.constraints	= {
		.name		= "VLCD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.boot_on 	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo30_consumer,
};

static struct regulator_init_data s2m_ldo31_data = {
	.constraints	= {
		.name		= "VT_SENSOR_IO_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo31_consumer,
};

static struct regulator_init_data s2m_ldo31_data_rev01 = {
	.constraints	= {
		.name		= "VDD_MOT_3.0V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo31_consumer_rev01,
};

static struct regulator_init_data s2m_ldo31_data_rev00 = {
	.constraints	= {
		.name		= "VT_SENSOR_IO_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo31_consumer_rev00,
};


static struct regulator_init_data s2m_ldo32_data = {
	.constraints	= {
		.name		= "VTSP_A3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo32_consumer,
};

static struct regulator_init_data s2m_ldo33_data = {
	.constraints	= {
		.name		= "KEY_LED_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo33_consumer,
};

static struct regulator_init_data s2m_ldo34_data = {
	.constraints	= {
		.name		= "LEDA_3.0V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
                .valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo34_consumer,
};

static struct regulator_init_data s2m_ldo35_data = {
	.constraints	= {
		.name		= "SENSOR_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.boot_on	= 1,
                .valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(s2m_ldo35_consumer),
	.consumer_supplies	= s2m_ldo35_consumer,
};

static struct regulator_init_data s2m_ldo36_data = {
	.constraints	= {
		.name		= "VT_SENSOR_IO_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo36_consumer,
};

static struct regulator_init_data s2m_ldo36_data_rev00 = {
	.constraints	= {
		.name		= "VDDI_SPI_1.2V",
		.min_uV		= 800000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on      = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo36_consumer_rev00,
};


static struct regulator_init_data s2m_ldo37_data = {
	.constraints	= {
		.name		= "VDD_TSP_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo37_consumer,
};

static struct regulator_init_data s2m_ldo38_data = {
	.constraints    = {
		.name           = "VLED_3.3V",
		.min_uV         = 3300000,
		.max_uV         = 3300000,
		.apply_uV       = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2m_ldo38_consumer,
};

static struct regulator_init_data s2m_ldo39_data = {
	.constraints	= {
		.name		= "VGPS_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on      = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo39_consumer,
};

static struct regulator_init_data s2m_ldo39_data_rev00 = {
	.constraints	= {
		.name		= "FM_RADIO_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on      = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo39_consumer_rev00,
};


static struct sec_regulator_data exynos_regulators[] = {
	{S2MPU01A_BUCK1, &s2m_buck1_data},
	{S2MPU01A_BUCK2, &s2m_buck2_data},
	{S2MPU01A_BUCK3, &s2m_buck3_data},
	{S2MPU01A_BUCK4, &s2m_buck4_data},
	{S2MPU01A_LDO2, &s2m_ldo2_data},
	{S2MPU01A_LDO4, &s2m_ldo4_data},
	{S2MPU01A_LDO5, &s2m_ldo5_data},
	{S2MPU01A_LDO6, &s2m_ldo6_data},
	{S2MPU01A_LDO7, &s2m_ldo7_data},
	{S2MPU01A_LDO8, &s2m_ldo8_data},
	{S2MPU01A_LDO9, &s2m_ldo9_data},
	{S2MPU01A_LDO26, &s2m_ldo26_data},
	{S2MPU01A_LDO27, &s2m_ldo27_data},
	{S2MPU01A_LDO28, &s2m_ldo28_data},
	{S2MPU01A_LDO29, &s2m_ldo29_data},
	{S2MPU01A_LDO30, &s2m_ldo30_data},
	{S2MPU01A_LDO31, &s2m_ldo31_data},
	{S2MPU01A_LDO32, &s2m_ldo32_data},
	{S2MPU01A_LDO33, &s2m_ldo33_data},
	{S2MPU01A_LDO34, &s2m_ldo34_data},
	{S2MPU01A_LDO35, &s2m_ldo35_data},
	{S2MPU01A_LDO37, &s2m_ldo37_data},
	{S2MPU01A_LDO38, &s2m_ldo38_data},
	{S2MPU01A_LDO39, &s2m_ldo39_data},
};

static struct sec_regulator_data exynos_regulators_rev01[] = {
	{S2MPU01A_BUCK1, &s2m_buck1_data},
	{S2MPU01A_BUCK2, &s2m_buck2_data},
	{S2MPU01A_BUCK3, &s2m_buck3_data},
	{S2MPU01A_BUCK4, &s2m_buck4_data},
	{S2MPU01A_LDO2, &s2m_ldo2_data},
	{S2MPU01A_LDO4, &s2m_ldo4_data},
	{S2MPU01A_LDO5, &s2m_ldo5_data},
	{S2MPU01A_LDO6, &s2m_ldo6_data},
	{S2MPU01A_LDO7, &s2m_ldo7_data},
	{S2MPU01A_LDO8, &s2m_ldo8_data},
	{S2MPU01A_LDO9, &s2m_ldo9_data},
	{S2MPU01A_LDO26, &s2m_ldo26_data},
	{S2MPU01A_LDO27, &s2m_ldo27_data},
	{S2MPU01A_LDO28, &s2m_ldo28_data},
	{S2MPU01A_LDO29, &s2m_ldo29_data},
	{S2MPU01A_LDO30, &s2m_ldo30_data},
	{S2MPU01A_LDO31, &s2m_ldo31_data_rev01},
	{S2MPU01A_LDO32, &s2m_ldo32_data},
	{S2MPU01A_LDO33, &s2m_ldo33_data},
	{S2MPU01A_LDO34, &s2m_ldo34_data},
	{S2MPU01A_LDO35, &s2m_ldo35_data},
	{S2MPU01A_LDO36, &s2m_ldo36_data},
	{S2MPU01A_LDO37, &s2m_ldo37_data},
	{S2MPU01A_LDO38, &s2m_ldo38_data},
	{S2MPU01A_LDO39, &s2m_ldo39_data},
};

static struct sec_regulator_data exynos_regulators_rev00[] = {
	{S2MPU01A_BUCK1, &s2m_buck1_data},
	{S2MPU01A_BUCK2, &s2m_buck2_data},
	{S2MPU01A_BUCK3, &s2m_buck3_data},
	{S2MPU01A_BUCK4, &s2m_buck4_data},
	{S2MPU01A_LDO2, &s2m_ldo2_data},
	{S2MPU01A_LDO4, &s2m_ldo4_data},
	{S2MPU01A_LDO5, &s2m_ldo5_data},
	{S2MPU01A_LDO6, &s2m_ldo6_data},
	{S2MPU01A_LDO7, &s2m_ldo7_data},
	{S2MPU01A_LDO8, &s2m_ldo8_data},
	{S2MPU01A_LDO9, &s2m_ldo9_data},
	{S2MPU01A_LDO26, &s2m_ldo26_data},
	{S2MPU01A_LDO27, &s2m_ldo27_data},
	{S2MPU01A_LDO28, &s2m_ldo28_data_rev00},
	{S2MPU01A_LDO29, &s2m_ldo29_data},
	{S2MPU01A_LDO30, &s2m_ldo30_data},
	{S2MPU01A_LDO31, &s2m_ldo31_data_rev00},
	{S2MPU01A_LDO32, &s2m_ldo32_data},
	{S2MPU01A_LDO33, &s2m_ldo33_data},
	{S2MPU01A_LDO34, &s2m_ldo34_data},
	{S2MPU01A_LDO35, &s2m_ldo35_data},
	{S2MPU01A_LDO36, &s2m_ldo36_data_rev00},
	{S2MPU01A_LDO37, &s2m_ldo37_data},
	{S2MPU01A_LDO38, &s2m_ldo38_data},
	{S2MPU01A_LDO39, &s2m_ldo39_data_rev00},
};

struct sec_opmode_data s2mpu01a_opmode_data[S2MPU01A_REG_MAX] = {
	[S2MPU01A_BUCK1] = {S2MPU01A_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK2] = {S2MPU01A_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK3] = {S2MPU01A_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK4] = {S2MPU01A_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO2] = {S2MPU01A_LDO2, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO4] = {S2MPU01A_LDO4, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO5] = {S2MPU01A_LDO5, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO6] = {S2MPU01A_LDO6, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO7] = {S2MPU01A_LDO7, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO8] = {S2MPU01A_LDO8, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO9] = {S2MPU01A_LDO9, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO26] = {S2MPU01A_LDO26, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO27] = {S2MPU01A_LDO27, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO28] = {S2MPU01A_LDO28, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO29] = {S2MPU01A_LDO29, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO30] = {S2MPU01A_LDO30, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO31] = {S2MPU01A_LDO31, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO32] = {S2MPU01A_LDO32, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO33] = {S2MPU01A_LDO33, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO34] = {S2MPU01A_LDO34, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO35] = {S2MPU01A_LDO35, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO37] = {S2MPU01A_LDO37, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO38] = {S2MPU01A_LDO38, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO39] = {S2MPU01A_LDO39, SEC_OPMODE_NORMAL},
};

struct sec_opmode_data s2mpu01a_opmode_data_rev01[S2MPU01A_REG_MAX] = {
	[S2MPU01A_BUCK1] = {S2MPU01A_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK2] = {S2MPU01A_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK3] = {S2MPU01A_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK4] = {S2MPU01A_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO2] = {S2MPU01A_LDO2, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO4] = {S2MPU01A_LDO4, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO5] = {S2MPU01A_LDO5, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO6] = {S2MPU01A_LDO6, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO7] = {S2MPU01A_LDO7, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO8] = {S2MPU01A_LDO8, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO9] = {S2MPU01A_LDO9, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO26] = {S2MPU01A_LDO26, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO27] = {S2MPU01A_LDO27, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO28] = {S2MPU01A_LDO28, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO29] = {S2MPU01A_LDO29, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO30] = {S2MPU01A_LDO30, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO31] = {S2MPU01A_LDO31, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO32] = {S2MPU01A_LDO32, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO33] = {S2MPU01A_LDO33, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO34] = {S2MPU01A_LDO34, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO35] = {S2MPU01A_LDO35, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO36] = {S2MPU01A_LDO36, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO37] = {S2MPU01A_LDO37, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO38] = {S2MPU01A_LDO38, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO39] = {S2MPU01A_LDO39, SEC_OPMODE_NORMAL},
};

struct sec_opmode_data s2mpu01a_opmode_data_rev00[S2MPU01A_REG_MAX] = {
	[S2MPU01A_BUCK1] = {S2MPU01A_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK2] = {S2MPU01A_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK3] = {S2MPU01A_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK4] = {S2MPU01A_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO2] = {S2MPU01A_LDO2, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO4] = {S2MPU01A_LDO4, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO5] = {S2MPU01A_LDO5, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO6] = {S2MPU01A_LDO6, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO7] = {S2MPU01A_LDO7, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO8] = {S2MPU01A_LDO8, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO9] = {S2MPU01A_LDO9, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO26] = {S2MPU01A_LDO26, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO27] = {S2MPU01A_LDO27, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO28] = {S2MPU01A_LDO28, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO29] = {S2MPU01A_LDO29, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO30] = {S2MPU01A_LDO30, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO31] = {S2MPU01A_LDO31, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO32] = {S2MPU01A_LDO32, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO33] = {S2MPU01A_LDO33, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO34] = {S2MPU01A_LDO34, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO35] = {S2MPU01A_LDO35, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO36] = {S2MPU01A_LDO36, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO37] = {S2MPU01A_LDO37, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO38] = {S2MPU01A_LDO38, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO39] = {S2MPU01A_LDO39, SEC_OPMODE_STANDBY},
};

static int sec_cfg_irq(void)
{
	unsigned int pin = irq_to_gpio(SMDK4270_PMIC_EINT);

	s3c_gpio_cfgpin(pin, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);

	return 0;
}

static struct sec_pmic_platform_data exynos4_s2m_pdata = {
	.device_type		= S2MPU01A,
	.irq_base		= IRQ_BOARD_START,
	.num_regulators		= ARRAY_SIZE(exynos_regulators),
	.regulators		= exynos_regulators,
	.cfg_pmic_irq		= sec_cfg_irq,
	.wakeup			= 1,
	.wtsr_smpl		= 1,
	.opmode_data		= s2mpu01a_opmode_data,
	.buck16_ramp_delay	= 12,
	.buck2_ramp_delay	= 12,
	.buck34_ramp_delay	= 12,
	.buck1_ramp_enable	= 1,
	.buck2_ramp_enable	= 1,
	.buck3_ramp_enable	= 1,
	.buck4_ramp_enable	= 1,

	.buck1_init             = 1100000,
	.buck2_init             = 1100000,
	.buck3_init             = 1100000,
	.buck4_init             = 1100000,
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
		.platform_data	= &exynos4_s2m_pdata,
		.irq		= SMDK4270_PMIC_EINT,
	},
};

#endif /* CONFIG_REGULATOR_S2MPU01A */
#ifdef CONFIG_BATTERY_EXYNOS
static struct platform_device samsung_device_battery = {
	.name	= "samsung-fake-battery",
};
#endif
#ifdef CONFIG_ARM_EXYNOS3470_BUS_DEVFREQ
static struct platform_device exynos4270_mif_devfreq = {
	.name	= "exynos4270-busfreq-mif",
	.id	= -1,
};

static struct platform_device exynos4270_int_devfreq = {
	.name	= "exynos4270-busfreq-int",
	.id	= -1,
};

static struct exynos_devfreq_platdata universial222_qos_mif_pd __initdata = {
	.default_qos = 200000,
};

static struct exynos_devfreq_platdata universial222_qos_int_pd __initdata = {
	.default_qos = 100000,
};
#endif

#ifdef CONFIG_EXYNOS_THERMAL
static struct exynos_tmu_platform_data exynos4270_tmu_data = {
	.threshold = 0,
	.trigger_levels[0] = 95,
	.trigger_levels[1] = 100,
	.trigger_levels[2] = 110,
	.trigger_levels[3] = 115,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 1,
	.gain = 5,
	.reference_voltage = 16,
	.noise_cancel_mode = 4,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.efuse_value = 55,
	.freq_tab[0] = {
		.freq_clip_max = 1300 * 1000,
		.temp_level = 95,
	},
	.freq_tab[1] = {
		.freq_clip_max = 1200 * 1000,
		.temp_level = 100,
	},
	.freq_tab[2] = {
		.freq_clip_max = 1100 * 1000,
		.temp_level = 105,
	},
	.freq_tab[3] = {
		.freq_clip_max = 1000 * 1000,
		.temp_level = 110,
	},
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 3,
	.freq_tab_count = 4,
	.type = SOC_ARCH_EXYNOS4,
};
#endif

static struct platform_device *smdk4270_power_devices[] __initdata = {
	&s3c_device_i2c0,
#ifdef CONFIG_BATTERY_EXYNOS
	&samsung_device_battery,
#endif
#ifdef CONFIG_ARM_EXYNOS3470_BUS_DEVFREQ
	&exynos4270_mif_devfreq,
	&exynos4270_int_devfreq,
#endif
#ifdef CONFIG_EXYNOS_THERMAL
	&exynos4270_device_tmu,
#endif
};

extern unsigned int system_rev;

void __init exynos4_smdk4270_power_init(void)
{
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	if( system_rev == 0 ){
		exynos4_s2m_pdata.num_regulators = ARRAY_SIZE(exynos_regulators_rev00),
		exynos4_s2m_pdata.regulators = exynos_regulators_rev00;
                exynos4_s2m_pdata.opmode_data = s2mpu01a_opmode_data_rev00;
	}else if( system_rev == 1 ){
		exynos4_s2m_pdata.num_regulators = ARRAY_SIZE(exynos_regulators_rev01),
		exynos4_s2m_pdata.regulators = exynos_regulators_rev01;
                exynos4_s2m_pdata.opmode_data = s2mpu01a_opmode_data_rev01;
        }

#ifdef CONFIG_EXYNOS_THERMAL
	s3c_set_platdata(&exynos4270_tmu_data,
			sizeof(struct exynos_tmu_platform_data),
			&exynos4270_device_tmu);
#endif

#ifdef CONFIG_ARM_EXYNOS3470_BUS_DEVFREQ
	s3c_set_platdata(&universial222_qos_mif_pd, sizeof(struct exynos_devfreq_platdata),
				&exynos4270_mif_devfreq);
	s3c_set_platdata(&universial222_qos_int_pd, sizeof(struct exynos_devfreq_platdata),
				&exynos4270_int_devfreq);
#endif
	platform_add_devices(smdk4270_power_devices,
			ARRAY_SIZE(smdk4270_power_devices));
}
