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
#include <linux/platform_data/exynos_thermal.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>
#include <mach/tmu.h>
#include <mach/devfreq.h>

#include "board-universal4415.h"

#ifdef CONFIG_MFD_RT5033
#ifdef CONFIG_REGULATOR_RT5033

#include <linux/mfd/rt5033.h>
#if 0
static struct regulator_consumer_supply rt5033_safe_ldo_consumers[] = {
	REGULATOR_SUPPLY("rt5033_safe_ldo",NULL),
};
#endif
static struct regulator_consumer_supply rt5033_ldo_consumers[] = {
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
		.name		= "CAM_SENSOR_A2.8V",
		.min_uV = 2800000,
		.max_uV = 2800000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_ldo_consumers),
	.consumer_supplies = rt5033_ldo_consumers,
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
#endif
#endif

#define UNIVERSAL4415_PMIC_EINT	IRQ_EINT(7)
#define UNIVERSAL4415_GPIO_WRST	EXYNOS4_GPF1(0)

static struct regulator_consumer_supply s5m8767_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s5m8767_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s5m8767_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s5m8767_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply s5m8767_ldo8_consumer =
	REGULATOR_SUPPLY("vhsif_1.0v_ap", NULL);

static struct regulator_consumer_supply s5m8767_ldo9_consumer =
	REGULATOR_SUPPLY("vt_cam_1.8v", NULL);

static struct regulator_consumer_supply s5m8767_ldo10_consumer =
	REGULATOR_SUPPLY("vhsif_1.8v_ap", NULL);

static struct regulator_consumer_supply s5m8767_ldo18_consumer[] = {
	REGULATOR_SUPPLY("vqmmc", "dw_mmc.2"),
	REGULATOR_SUPPLY("vddq_mmc2_ap", NULL),
};

static struct regulator_consumer_supply s5m8767_ldo19_consumer =
	REGULATOR_SUPPLY("cam_af_2.8v", NULL);

static struct regulator_consumer_supply s5m8767_ldo21_consumer =
	REGULATOR_SUPPLY("vdd_sensor_2.8v", NULL);

static struct regulator_consumer_supply s5m8767_ldo22_consumer =
	REGULATOR_SUPPLY("tsp_avdd_3.3v", NULL);

static struct regulator_consumer_supply s5m8767_ldo23_consumer[] ={
	REGULATOR_SUPPLY("vmmc", "dw_mmc.2"),
        REGULATOR_SUPPLY("vtf_2.8v", NULL),
};

static struct regulator_consumer_supply s5m8767_ldo24_consumer =
	REGULATOR_SUPPLY("led_a_3.0v", NULL);

static struct regulator_consumer_supply s5m8767_ldo26_consumer =
	REGULATOR_SUPPLY("cam_io_1.8v", NULL);

static struct regulator_consumer_supply s5m8767_ldo27_consumer =
	REGULATOR_SUPPLY("lcd_vdd_1.8v", NULL);

static struct regulator_consumer_supply s5m8767_ldo28_consumer[] ={
	REGULATOR_SUPPLY("tsp_avdd_1.8", NULL),
        REGULATOR_SUPPLY("tsp_vdd_1.8v", NULL),
};

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

static struct regulator_init_data s5m8767_ldo8_data = {
	.constraints	= {
		.name		= "ldo8",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo8_consumer,
};

static struct regulator_init_data s5m8767_ldo9_data = {
	.constraints	= {
		.name		= "ldo9",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo9_consumer,
};

static struct regulator_init_data s5m8767_ldo10_data = {
	.constraints	= {
		.name		= "vhsif_1.8v_ap",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on = 1,
		.boot_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo10_consumer,
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
	.num_consumer_supplies = 2,
	.consumer_supplies = s5m8767_ldo18_consumer,
};

static struct regulator_init_data s5m8767_ldo19_data = {
	.constraints	= {
		.name		= "ldo19",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo19_consumer,
};

static struct regulator_init_data s5m8767_ldo21_data = {
	.constraints	= {
		.name		= "ldo21",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo21_consumer,
};

static struct regulator_init_data s5m8767_ldo22_data = {
	.constraints	= {
		.name		= "ldo22",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
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
		.always_on	= 0,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s5m8767_ldo23_consumer,
};

static struct regulator_init_data s5m8767_ldo24_data = {
	.constraints	= {
		.name		= "ldo24",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo24_consumer,
};

static struct regulator_init_data s5m8767_ldo26_data = {
	.constraints	= {
		.name		= "ldo26",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo26_consumer,
};

static struct regulator_init_data s5m8767_ldo27_data = {
	.constraints	= {
		.name		= "vcc_lcd_1.8v range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_ldo27_consumer,
};


static struct regulator_init_data s5m8767_ldo28_data = {
	.constraints	= {
		.name		= "ldo28",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on = 0,
		.boot_on = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 2,
	.consumer_supplies = s5m8767_ldo28_consumer,
};

static struct sec_regulator_data hudson_regulators[] = {
        {S5M8767_BUCK1, &s5m8767_buck1_data},
        {S5M8767_BUCK2, &s5m8767_buck2_data},
        {S5M8767_BUCK3, &s5m8767_buck3_data},
        {S5M8767_BUCK4, &s5m8767_buck4_data},
        {S5M8767_LDO8, &s5m8767_ldo8_data},
        {S5M8767_LDO9, &s5m8767_ldo9_data},
        {S5M8767_LDO10, &s5m8767_ldo10_data},
        {S5M8767_LDO18, &s5m8767_ldo18_data},
        {S5M8767_LDO19, &s5m8767_ldo19_data},
        {S5M8767_LDO21, &s5m8767_ldo21_data},
        {S5M8767_LDO22, &s5m8767_ldo22_data},
        {S5M8767_LDO23, &s5m8767_ldo23_data},
        {S5M8767_LDO24, &s5m8767_ldo24_data},
        {S5M8767_LDO26, &s5m8767_ldo26_data},
        {S5M8767_LDO27, &s5m8767_ldo27_data},
        {S5M8767_LDO28, &s5m8767_ldo28_data},
};

struct sec_opmode_data s5m8767_opmode_data[S5M8767_REG_MAX] = {
        [S5M8767_BUCK1] = {S5M8767_BUCK1, SEC_OPMODE_STANDBY},
        [S5M8767_BUCK2] = {S5M8767_BUCK2, SEC_OPMODE_STANDBY},
        [S5M8767_BUCK3] = {S5M8767_BUCK3, SEC_OPMODE_STANDBY},
        [S5M8767_BUCK4] = {S5M8767_BUCK4, SEC_OPMODE_STANDBY},
        [S5M8767_LDO8]  = {S5M8767_LDO8,  SEC_OPMODE_STANDBY},
        [S5M8767_LDO9]  = {S5M8767_LDO9,  SEC_OPMODE_STANDBY},
        [S5M8767_LDO10] = {S5M8767_LDO10, SEC_OPMODE_STANDBY},
        [S5M8767_LDO18] = {S5M8767_LDO18, SEC_OPMODE_STANDBY},
        [S5M8767_LDO19] = {S5M8767_LDO19, SEC_OPMODE_STANDBY},
        [S5M8767_LDO21] = {S5M8767_LDO21, SEC_OPMODE_NORMAL},
        [S5M8767_LDO22] = {S5M8767_LDO22, SEC_OPMODE_STANDBY},
        [S5M8767_LDO23] = {S5M8767_LDO23, SEC_OPMODE_NORMAL},
        [S5M8767_LDO24] = {S5M8767_LDO24, SEC_OPMODE_NORMAL},
        [S5M8767_LDO26] = {S5M8767_LDO26, SEC_OPMODE_STANDBY},
        [S5M8767_LDO27] = {S5M8767_LDO27, SEC_OPMODE_STANDBY},
        [S5M8767_LDO28] = {S5M8767_LDO28, SEC_OPMODE_STANDBY},
};

static int sec_cfg_irq(void)
{
	unsigned int pin = irq_to_gpio(UNIVERSAL4415_PMIC_EINT);

	s3c_gpio_cfgpin(pin, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pin, S3C_GPIO_PULL_NONE);

	return 0;
}

static int sec_cfg_pmic_wrst(void)
{
	unsigned int gpio = UNIVERSAL4415_GPIO_WRST;
	unsigned int i;

	for (i = 0; i <= 5; i++) {
		s5p_gpio_set_pd_cfg(gpio + i, S5P_GPIO_PD_PREV_STATE);
	}

	return 0;
}

static struct sec_pmic_platform_data universal4415_s5m8767_pdata = {
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
		.platform_data = &universal4415_s5m8767_pdata,
		.irq		= UNIVERSAL4415_PMIC_EINT,
	},
};

static struct platform_device *universal4415_pmic_devices[] __initdata = {
	&s3c_device_i2c0,
};

extern int system_rev;
void __init exynos4_universal4415_pmic_init(void)
{
	s3c_i2c0_set_platdata(NULL);

/************************
REV00 1G : 0x0
REV00 2G : 0x1
REV01 2G : 0x2
REV01 1.5G :  0x3
*************************/
        if( system_rev <= 0x1) // REV00
                s5m8767_ldo10_data.constraints.always_on = 0;

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	platform_add_devices(universal4415_pmic_devices,
			ARRAY_SIZE(universal4415_pmic_devices));

}
