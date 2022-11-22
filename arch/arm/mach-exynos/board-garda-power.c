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

static struct regulator_consumer_supply s2m_buck8_consumer =
	REGULATOR_SUPPLY("main_cam_core_1v2", NULL);

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

static struct regulator_consumer_supply s2m_ldo26_consumer[] = {
	REGULATOR_SUPPLY("main_cam_io_1v8", NULL),
	REGULATOR_SUPPLY("vt_cam_io_1v8", NULL),
};

static struct regulator_consumer_supply s2m_ldo27_consumer[] = {
	REGULATOR_SUPPLY("main_cam_sensor_a2v8", NULL),
	REGULATOR_SUPPLY("vt_cam_sensor_a2v8", NULL),
};

static struct regulator_consumer_supply s2m_ldo28_consumer =
	REGULATOR_SUPPLY("main_cam_af_2v8", NULL);

static struct regulator_consumer_supply s2m_ldo29_consumer =
	REGULATOR_SUPPLY("vlcd_3v0", NULL);

static struct regulator_consumer_supply s2m_ldo30_consumer =
	REGULATOR_SUPPLY("vtsp_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo31_consumer =
	REGULATOR_SUPPLY("vt_cam_core_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo32_consumer =
	REGULATOR_SUPPLY("vtsp_a3v0", NULL);

static struct regulator_consumer_supply s2m_ldo33_consumer =
	REGULATOR_SUPPLY("key_led_3v3", NULL);

static struct regulator_consumer_supply s2m_ldo34_consumer =
	REGULATOR_SUPPLY("vdd_motor_3v0", NULL);

static struct regulator_consumer_supply s2m_ldo35_consumer =
	REGULATOR_SUPPLY("sensor_2v8", NULL);

static struct regulator_consumer_supply s2m_ldo37_consumer =
	REGULATOR_SUPPLY("vlcd_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo38_consumer =
	REGULATOR_SUPPLY("led_a_3v0", NULL);

static struct regulator_consumer_supply s2m_ldo39_consumer =
	REGULATOR_SUPPLY("sub_mic_bias_2v8", NULL);

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

static struct regulator_init_data s2m_buck8_data = {
	.constraints	= {
		.name		= "Main Camera Core (1.2V)",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_buck8_consumer,
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
		.name		= "Camera I/O (1.8V)",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s2m_ldo26_consumer,
};

static struct regulator_init_data s2m_ldo27_data = {
	.constraints	= {
		.name		= "Camera Sensor (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s2m_ldo27_consumer,
};

static struct regulator_init_data s2m_ldo28_data = {
	.constraints	= {
		.name		= "Main Camera AF (2.8V)",
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

static struct regulator_init_data s2m_ldo29_data = {
	.constraints	= {
		.name		= "VLCD_3V0",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.boot_on 	= 1,		
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo29_consumer,
};

static struct regulator_init_data s2m_ldo30_data = {
	.constraints	= {
		.name		= "VTSP_1V8",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo30_consumer,
};

static struct regulator_init_data s2m_ldo31_data = {
	.constraints	= {
		.name		= "VT Camera Core (1.8V)",
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

static struct regulator_init_data s2m_ldo32_data = {
	.constraints	= {
		.name		= "VTSP_A3V0",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo32_consumer,
};

static struct regulator_init_data s2m_ldo33_data = {
	.constraints	= {
		.name		= "KEY_LED_3V3",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo33_consumer,
};

static struct regulator_init_data s2m_ldo34_data = {
	.constraints	= {
		.name		= "VDD_MOTOR_3V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 0,
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
		.name		= "SENSOR_2V8",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo35_consumer,
};

static struct regulator_init_data s2m_ldo37_data = {
	.constraints	= {
		.name		= "VLCD_1V8",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.boot_on 	= 1,	
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo37_consumer,
};

static struct regulator_init_data s2m_ldo38_data = {
	.constraints	= {
		.name		= "LED_A_3V0",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo38_consumer,
};

static struct regulator_init_data s2m_ldo39_data = {
	.constraints	= {
		.name		= "SUB_MIC_BIAS_2V8",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo39_consumer,
};

static struct sec_regulator_data exynos_regulators[] = {
	{S2MPU01A_BUCK1, &s2m_buck1_data},
	{S2MPU01A_BUCK2, &s2m_buck2_data},
	{S2MPU01A_BUCK3, &s2m_buck3_data},
	{S2MPU01A_BUCK4, &s2m_buck4_data},
	{S2MPU01A_BUCK8, &s2m_buck8_data},
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
};

static struct sec_regulator_data exynos_regulators_rev06[] = {
	{S2MPU01A_BUCK1, &s2m_buck1_data},
	{S2MPU01A_BUCK2, &s2m_buck2_data},
	{S2MPU01A_BUCK3, &s2m_buck3_data},
	{S2MPU01A_BUCK4, &s2m_buck4_data},
	{S2MPU01A_BUCK8, &s2m_buck8_data},
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
	{S2MPU01A_LDO31, &s2m_ldo31_data},
	{S2MPU01A_LDO32, &s2m_ldo32_data},
	{S2MPU01A_LDO33, &s2m_ldo33_data},
	{S2MPU01A_LDO34, &s2m_ldo34_data},
	{S2MPU01A_LDO35, &s2m_ldo35_data},
	{S2MPU01A_LDO37, &s2m_ldo37_data},
	{S2MPU01A_LDO38, &s2m_ldo38_data},
	{S2MPU01A_LDO39, &s2m_ldo39_data},
};

struct sec_opmode_data s2mpu01a_opmode_data[S2MPU01A_REG_MAX] = {
	[S2MPU01A_BUCK1] = {S2MPU01A_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK2] = {S2MPU01A_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK3] = {S2MPU01A_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK4] = {S2MPU01A_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK8] = {S2MPU01A_BUCK8, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO2] = {S2MPU01A_LDO2, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO4] = {S2MPU01A_LDO4, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO5] = {S2MPU01A_LDO5, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO6] = {S2MPU01A_LDO6, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO7] = {S2MPU01A_LDO7, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO8] = {S2MPU01A_LDO8, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO9] = {S2MPU01A_LDO9, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO26] = {S2MPU01A_LDO26, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO27] = {S2MPU01A_LDO27, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO28] = {S2MPU01A_LDO28, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO29] = {S2MPU01A_LDO29, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO30] = {S2MPU01A_LDO30, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO31] = {S2MPU01A_LDO31, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO32] = {S2MPU01A_LDO32, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO33] = {S2MPU01A_LDO33, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO34] = {S2MPU01A_LDO34, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO35] = {S2MPU01A_LDO35, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO37] = {S2MPU01A_LDO37, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO38] = {S2MPU01A_LDO38, SEC_OPMODE_NORMAL},
};

struct sec_opmode_data s2mpu01a_opmode_data_rev06[S2MPU01A_REG_MAX] = {
	[S2MPU01A_BUCK1] = {S2MPU01A_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK2] = {S2MPU01A_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK3] = {S2MPU01A_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK4] = {S2MPU01A_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPU01A_BUCK8] = {S2MPU01A_BUCK8, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO2] = {S2MPU01A_LDO2, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO4] = {S2MPU01A_LDO4, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO5] = {S2MPU01A_LDO5, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO6] = {S2MPU01A_LDO6, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO7] = {S2MPU01A_LDO7, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO8] = {S2MPU01A_LDO8, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO9] = {S2MPU01A_LDO9, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO26] = {S2MPU01A_LDO26, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO27] = {S2MPU01A_LDO27, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO28] = {S2MPU01A_LDO28, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO29] = {S2MPU01A_LDO29, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO31] = {S2MPU01A_LDO31, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO32] = {S2MPU01A_LDO32, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO33] = {S2MPU01A_LDO33, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO34] = {S2MPU01A_LDO34, SEC_OPMODE_STANDBY},
	[S2MPU01A_LDO35] = {S2MPU01A_LDO35, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO37] = {S2MPU01A_LDO37, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO38] = {S2MPU01A_LDO38, SEC_OPMODE_NORMAL},
	[S2MPU01A_LDO39] = {S2MPU01A_LDO39, SEC_OPMODE_NORMAL},
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

	.buck1_init             = 1200000,
	.buck2_init             = 1100000,
	.buck3_init             = 1100000,
	.buck4_init             = 1200000,
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
	.threshold = 70,
	.trigger_levels[0] = 25,
	.trigger_levels[1] = 30,
	.trigger_levels[2] = 45,
	.trigger_levels[3] = 45,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 0,
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
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 2,
	.freq_tab_count = 3,
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

	if (system_rev < 7) {
		exynos4_s2m_pdata.regulators = exynos_regulators_rev06;
		exynos4_s2m_pdata.num_regulators = ARRAY_SIZE(exynos_regulators_rev06);
		exynos4_s2m_pdata.opmode_data = s2mpu01a_opmode_data_rev06;
	}
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

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
