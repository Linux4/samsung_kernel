/*
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
#include <linux/gpio.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>
#include <mach/irqs.h>
#include <mach/hs-iic.h>

#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mpa01.h>

#define UNIVERSAL5260_PMIC_EINT	IRQ_EINT(7)
#define UNIVERSAL5260_GPIO_WRST	EXYNOS5260_GPF1(6)

static struct regulator_consumer_supply s2m_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s2m_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s2m_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s2m_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply s2m_buck6_consumer =
	REGULATOR_SUPPLY("vdd_kfc", NULL);

static struct regulator_consumer_supply s2m_buck7_consumer =
	REGULATOR_SUPPLY("vdd_disp", NULL);

static struct regulator_consumer_supply s2m_ldo2_consumer =
	REGULATOR_SUPPLY("vqmmc", "dw_mmc.2");

static struct regulator_consumer_supply s2m_ldo3_consumer =
	REGULATOR_SUPPLY("vcc_1.8v_AP", NULL);

static struct regulator_consumer_supply s2m_ldo6_consumer =
	REGULATOR_SUPPLY("vdd10_mipi", NULL);

static struct regulator_consumer_supply s2m_ldo7_consumer =
	REGULATOR_SUPPLY("vdd18_mipi", NULL);

static struct regulator_consumer_supply s2m_ldo11_consumer =
	REGULATOR_SUPPLY("tsp_avdd_3.3v", NULL);

static struct regulator_consumer_supply s2m_ldo13_consumer =
	REGULATOR_SUPPLY("vmmc", NULL);

static struct regulator_consumer_supply s2m_ldo15_consumer[] = {
	REGULATOR_SUPPLY("main_cam_sensor_a2v8", NULL),
	REGULATOR_SUPPLY("vt_cam_sensor_a2v8", NULL),
};

static struct regulator_consumer_supply s2m_ldo16_consumer =
	REGULATOR_SUPPLY("vcc_2.8v_AP", NULL);

static struct regulator_consumer_supply s2m_ldo17_consumer =
	REGULATOR_SUPPLY("vcc_lcd_1.8v", NULL);

static struct regulator_consumer_supply s2m_ldo18_consumer =
	REGULATOR_SUPPLY("vcc_lcd_3.0v", NULL);

static struct regulator_consumer_supply s2m_ldo19_consumer =
	REGULATOR_SUPPLY("vcc_3.0v_motor", NULL);

static struct regulator_consumer_supply s2m_ldo20_consumer =
	REGULATOR_SUPPLY("vt_cam_core_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo21_consumer =
	REGULATOR_SUPPLY("wacom_3.0v", NULL);

static struct regulator_consumer_supply s2m_ldo22_consumer =
	REGULATOR_SUPPLY("main_cam_af_2v8", NULL);

static struct regulator_consumer_supply s2m_ldo23_consumer[] = {
	REGULATOR_SUPPLY("ges_led_3.3v", NULL), /*for evt0*/
	REGULATOR_SUPPLY("prox_vdd_2.8v", NULL), /*for evt1*/
};

static struct regulator_consumer_supply s2m_ldo24_consumer =
	REGULATOR_SUPPLY("vtouch_3.3v", NULL);

static struct regulator_consumer_supply s2m_ldo25_consumer[] = {
	REGULATOR_SUPPLY("main_cam_io_1v8", NULL), /*for evt0*/
        REGULATOR_SUPPLY("main_cam_core_1v2", NULL), /*for evt1*/
};

static struct regulator_consumer_supply s2m_ldo26_consumer =
	REGULATOR_SUPPLY("main_cam_core_1v1", NULL); /*for evt0*/

static struct regulator_init_data s2m_buck1_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		=  600000,
		.max_uV		= 1400000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.uV		= 1000000,
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
		.min_uV		=  600000,
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
		.min_uV		=  600000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.uV		= 1000000,
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
		.min_uV		=  600000,
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

static struct regulator_init_data s2m_buck6_data = {
	.constraints	= {
		.name		= "vdd_kfc range",
		.min_uV		=  600000,
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
	.consumer_supplies	= &s2m_buck6_consumer,
};

static struct regulator_init_data s2m_buck7_data = {
	.constraints	= {
		.name		= "vdd_disp range",
		.min_uV		=  600000,
		.max_uV		= 1200000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_buck7_consumer,
};

static struct regulator_init_data s2m_ldo2_data = {
	.constraints	= {
		.name		= "vdd_ldo2 range",
		.min_uV		= 1800000,
		.max_uV		= 3300000,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo2_consumer,
};

static struct regulator_init_data s2m_ldo3_data = {
	.constraints    = {
		.name           = "vcc_1.8v_AP",
		.min_uV         = 1800000,
		.max_uV         = 1800000,
		.apply_uV       = 1,
		.always_on      = 1,
		.state_mem      = {
			.enabled        = 1,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2m_ldo3_consumer,
};

static struct regulator_init_data s2m_ldo6_data = {
	.constraints    = {
		.name           = "vdd10_mipi",
		.min_uV         = 1000000,
		.max_uV         = 1000000,
		.apply_uV       = 1,
		.always_on      = 1,
		.state_mem      = {
			.enabled        = 1,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2m_ldo6_consumer,
};

static struct regulator_init_data s2m_ldo7_data = {
	.constraints    = {
		.name           = "vdd18_mipi",
		.min_uV         = 1800000,
		.max_uV         = 1800000,
		.apply_uV       = 1,
		.always_on      = 1,
		.state_mem      = {
			.enabled        = 1,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2m_ldo7_consumer,
};

static struct regulator_init_data s2m_ldo11_data = {
	.constraints	= {
		.name		= "tsp_avdd 3.3v",
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
	.consumer_supplies	= &s2m_ldo11_consumer,
};

static struct regulator_init_data s2m_ldo13_data = {
	.constraints	= {
		.name		= "VTF_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo13_consumer,
};

static struct regulator_init_data s2m_ldo15_data = {
	.constraints	= {
		.name		= "Camera Sensor (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.boot_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s2m_ldo15_consumer,
};

static struct regulator_init_data s2m_ldo16_data = {
	.constraints	= {
		.name           = "vcc_2.8v_AP",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo16_consumer,
};

static struct regulator_init_data s2m_ldo17_data = {
	.constraints    = {
		.name           = "vcc_lcd_1.8v",
		.min_uV         = 1800000,
		.max_uV         = 1800000,
		.apply_uV       = 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem      = {
			.enabled        = 1,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2m_ldo17_consumer,
};

static struct regulator_init_data s2m_ldo18_data = {
	.constraints    = {
		.name           = "vcc_lcd_3.0v",
		.min_uV         = 3000000,
		.max_uV         = 3000000,
		.apply_uV       = 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem      = {
			.enabled        = 1,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2m_ldo18_consumer,
};

static struct regulator_init_data s2m_ldo19_data = {
	.constraints    = {
		.name           = "vcc_3.0v_motor",
		.min_uV         = 3000000,
		.max_uV         = 3000000,
		.apply_uV       = 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem      = {
			.enabled        = 1,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &s2m_ldo19_consumer,
};

static struct regulator_init_data s2m_ldo20_data = {
	.constraints	= {
		.name		= "VT Camera Core (1.8V)",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.boot_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo20_consumer,
};

static struct regulator_init_data s2m_ldo21_data = {
	.constraints	= {
		.name		= "WACOM (3.0V)",
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
	.consumer_supplies	= &s2m_ldo21_consumer,
};

static struct regulator_init_data s2m_ldo22_data = {
	.constraints	= {
		.name		= "Main Camera AF (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.boot_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo22_consumer,
};

static struct regulator_init_data s2m_ldo23_data_evt0 = {
	.constraints	= {
		.name		= "GES_LED (3.3V)",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s2m_ldo23_consumer,
};

static struct regulator_init_data s2m_ldo23_data = {
	.constraints	= {
		.name		= "PROX VDD (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s2m_ldo23_consumer,
};

static struct regulator_init_data s2m_ldo24_data = {
	.constraints	= {
		.name		= "vtouch range",
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
	.consumer_supplies	= &s2m_ldo24_consumer,
};

static struct regulator_init_data s2m_ldo25_data_evt0 = {
	.constraints	= {
		.name		= "Main Camera IO (1.8V)",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.boot_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s2m_ldo25_consumer,
};

static struct regulator_init_data s2m_ldo25_data = {
	.constraints	= {
		.name		= "CAM CORE (1.2V)",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s2m_ldo25_consumer,
};

static struct regulator_init_data s2m_ldo26_data_evt0 = {
	.constraints	= {
		.name		= "Main Camera Core (1.1V)",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo26_consumer,
};

static struct sec_regulator_data exynos_regulators_evt0[] = {
	{S2MPA01_BUCK1, &s2m_buck1_data},
	{S2MPA01_BUCK2, &s2m_buck2_data},
	{S2MPA01_BUCK3, &s2m_buck3_data},
	{S2MPA01_BUCK4, &s2m_buck4_data},
	{S2MPA01_BUCK6, &s2m_buck6_data},
	{S2MPA01_BUCK7, &s2m_buck7_data},
	{S2MPA01_LDO2, &s2m_ldo2_data},
	{S2MPA01_LDO3, &s2m_ldo3_data},
	{S2MPA01_LDO6, &s2m_ldo6_data},
	{S2MPA01_LDO7, &s2m_ldo7_data},
	{S2MPA01_LDO11, &s2m_ldo11_data},
	{S2MPA01_LDO13, &s2m_ldo13_data},
	{S2MPA01_LDO15, &s2m_ldo15_data},
	{S2MPA01_LDO16, &s2m_ldo16_data},
	{S2MPA01_LDO17, &s2m_ldo17_data},
	{S2MPA01_LDO18, &s2m_ldo18_data},
	{S2MPA01_LDO19, &s2m_ldo19_data},
	{S2MPA01_LDO20, &s2m_ldo20_data},
	{S2MPA01_LDO21, &s2m_ldo21_data},
	{S2MPA01_LDO22, &s2m_ldo22_data},
	{S2MPA01_LDO23, &s2m_ldo23_data_evt0},
	{S2MPA01_LDO24, &s2m_ldo24_data},
	{S2MPA01_LDO25, &s2m_ldo25_data_evt0},
	{S2MPA01_LDO26, &s2m_ldo26_data_evt0},
};

static struct sec_regulator_data exynos_regulators[] = {
	{S2MPA01_BUCK1, &s2m_buck1_data},
	{S2MPA01_BUCK2, &s2m_buck2_data},
	{S2MPA01_BUCK3, &s2m_buck3_data},
	{S2MPA01_BUCK4, &s2m_buck4_data},
	{S2MPA01_BUCK6, &s2m_buck6_data},
	{S2MPA01_BUCK7, &s2m_buck7_data},
	{S2MPA01_LDO2, &s2m_ldo2_data},
	{S2MPA01_LDO3, &s2m_ldo3_data},
	{S2MPA01_LDO6, &s2m_ldo6_data},
	{S2MPA01_LDO7, &s2m_ldo7_data},
	{S2MPA01_LDO11, &s2m_ldo11_data},
	{S2MPA01_LDO13, &s2m_ldo13_data},
	{S2MPA01_LDO15, &s2m_ldo15_data},
	{S2MPA01_LDO16, &s2m_ldo16_data},
	{S2MPA01_LDO17, &s2m_ldo17_data},
	{S2MPA01_LDO18, &s2m_ldo18_data},
	{S2MPA01_LDO19, &s2m_ldo19_data},
	{S2MPA01_LDO20, &s2m_ldo20_data},
	{S2MPA01_LDO21, &s2m_ldo21_data},
	{S2MPA01_LDO22, &s2m_ldo22_data},
	{S2MPA01_LDO23, &s2m_ldo23_data},
	{S2MPA01_LDO24, &s2m_ldo24_data},
	{S2MPA01_LDO25, &s2m_ldo25_data},
};

struct sec_opmode_data s2mpa01_opmode_data[S2MPA01_REG_MAX] = {
	[S2MPA01_BUCK1] = {S2MPA01_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK2] = {S2MPA01_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK3] = {S2MPA01_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK4] = {S2MPA01_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK6] = {S2MPA01_BUCK6, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK7] = {S2MPA01_BUCK7, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO2] = {S2MPA01_LDO2, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO6] = {S2MPA01_LDO6, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO7] = {S2MPA01_LDO7, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO11] = {S2MPA01_LDO11, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO13] = {S2MPA01_LDO13, SEC_OPMODE_NORMAL},
	[S2MPA01_LDO15] = {S2MPA01_LDO15, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO16] = {S2MPA01_LDO16, SEC_OPMODE_NORMAL},
	[S2MPA01_LDO17] = {S2MPA01_LDO17, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO18] = {S2MPA01_LDO18, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO19] = {S2MPA01_LDO19, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO20] = {S2MPA01_LDO20, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO21] = {S2MPA01_LDO21, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO22] = {S2MPA01_LDO22, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO23] = {S2MPA01_LDO23, SEC_OPMODE_NORMAL},
	[S2MPA01_LDO24] = {S2MPA01_LDO24, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO25] = {S2MPA01_LDO25, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO26] = {S2MPA01_LDO26, SEC_OPMODE_STANDBY},
};

static int sec_cfg_pm_wrst(void)
{
	int gpio = UNIVERSAL5260_GPIO_WRST;

	if (gpio_request(gpio, "PM_WRST")) {
		pr_err("%s : PM_WRST request port erron", __func__);
	}

	gpio_direction_output(gpio, 1);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	return 0;
}

static int sec_cfg_irq(void)
{
	unsigned int pin = irq_to_gpio(UNIVERSAL5260_PMIC_EINT);

	s3c_gpio_cfgpin(pin, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);
	return 0;
}

static struct sec_pmic_platform_data exynos5_s2m_pdata = {
	.device_type		= S2MPA01,
	.irq_base		= IRQ_BOARD_START,
	.num_regulators		= ARRAY_SIZE(exynos_regulators),
	.regulators		= exynos_regulators,
	.cfg_pmic_irq		= sec_cfg_irq,
	.cfg_pmic_wrst		= sec_cfg_pm_wrst,
	.wakeup			= 1,
	.wtsr_smpl		= 1,
	.opmode_data		= s2mpa01_opmode_data,
	.buck16_ramp_delay	= 12,
	.buck24_ramp_delay	= 12,
        .buck3_ramp_delay       = 12,
        .buck5_ramp_delay       = 12,
        .buck7_ramp_delay       = 12,
        .buck8910_ramp_delay    = 12,

	.buck1_ramp_enable	= 1,
	.buck2_ramp_enable	= 1,
	.buck3_ramp_enable	= 1,
	.buck4_ramp_enable	= 1,
};

static struct i2c_board_info s3c_i2c_devs1[] __initdata = {
	{
		I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
		.platform_data	= &exynos5_s2m_pdata,
		.irq		= UNIVERSAL5260_PMIC_EINT,
	},
};

struct s3c2410_platform_i2c s3c_i2c_data1 __initdata = {
	.bus_num	= 5,
	.flags		= 0,
	.frequency	= 400*1000,
	.sda_delay	= 100,
};

static struct i2c_board_info hs_i2c_devs0[] __initdata = {
    {
        I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
        .platform_data  = &exynos5_s2m_pdata,
        .irq            = UNIVERSAL5260_PMIC_EINT,
    },
};

static struct exynos5_platform_i2c hs_i2c0_data __initdata = {
    .bus_number = 0,
    .operation_mode = HSI2C_POLLING,
    .speed_mode = HSI2C_HIGH_SPD,
    .fast_speed = 400000,
    .high_speed = 2500000,
    .cfg_gpio = NULL,
};

static struct platform_device *universal5260_power_devices[] __initdata = {
    &exynos5_device_hs_i2c0,
};

extern unsigned int system_rev;

void __init board_hl_pmic_init(void)
{
    printk( "system rev A %d", system_rev);

	if (system_rev >= 8) {
		s2m_ldo22_data.constraints.min_uV = 1800000;
		s2m_ldo22_data.constraints.max_uV = 1800000;
	}

	if( system_rev <= 3 ){
		exynos5_s2m_pdata.num_regulators = ARRAY_SIZE(exynos_regulators_evt0),
		exynos5_s2m_pdata.regulators = exynos_regulators_evt0;
	}

	if( system_rev == 0 ){
		universal5260_power_devices[0]=&s3c_device_i2c1;
		s3c_i2c1_set_platdata(&s3c_i2c_data1);
		i2c_register_board_info(5, s3c_i2c_devs1, ARRAY_SIZE(s3c_i2c_devs1));
	}else{
		exynos5_hs_i2c0_set_platdata(&hs_i2c0_data);
		i2c_register_board_info(0, hs_i2c_devs0, ARRAY_SIZE(hs_i2c_devs0));
	}

	platform_add_devices(universal5260_power_devices,
			ARRAY_SIZE(universal5260_power_devices));
}
