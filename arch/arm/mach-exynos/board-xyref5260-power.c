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
#include <linux/bq24160_charger.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/tmu.h>
#include <mach/cpufreq.h>

#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mpa01.h>
#include <linux/platform_data/exynos_thermal.h>

#include "board-xyref5260.h"

#define XYREF5260_PMIC_EINT	IRQ_EINT(2)
#define XYREF5260_GPIO_WRST	EXYNOS5260_GPA2(0)

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
struct cpumask mp_cluster_cpus[CA_END];

static void __init init_mp_cpumask_set(void)
{
	unsigned int i;

	for_each_cpu(i, cpu_possible_mask) {
		if (exynos_boot_cluster == CA7) {
			if (i >= NR_CA7)
				cpumask_set_cpu(i, &mp_cluster_cpus[CA15]);
			else
				cpumask_set_cpu(i, &mp_cluster_cpus[CA7]);
		} else {
			if (i >= NR_CA15)
				cpumask_set_cpu(i, &mp_cluster_cpus[CA7]);
			else
				cpumask_set_cpu(i, &mp_cluster_cpus[CA15]);
		}
	}
}
#endif

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
	REGULATOR_SUPPLY("vcc_1.0v_MIPI", NULL);

static struct regulator_consumer_supply s2m_ldo7_consumer =
	REGULATOR_SUPPLY("vcc_1.8v_mipi", NULL);

static struct regulator_consumer_supply s2m_ldo14_consumer =
	REGULATOR_SUPPLY("main_cam_io_1v8", NULL);

static struct regulator_consumer_supply s2m_ldo15_consumer[] = {
	REGULATOR_SUPPLY("main_cam_sensor_a2v8", NULL),
	REGULATOR_SUPPLY("vt_cam_sensor_a2v8", NULL),
};

static struct regulator_consumer_supply s2m_ldo16_consumer =
	REGULATOR_SUPPLY("main_cam_af_2v8", NULL);

static struct regulator_consumer_supply s2m_ldo17_consumer =
	REGULATOR_SUPPLY("vt_cam_core_1v8", NULL);

#ifdef CONFIG_EXYNOS5260_XYREF_REV0
static struct regulator_consumer_supply s2m_ldo18_consumer =
	REGULATOR_SUPPLY("vcc_1.8v_LCD", NULL);
#else
static struct regulator_consumer_supply s2m_ldo18_consumer =
	REGULATOR_SUPPLY("vcc_3.3v_tsp", NULL);
#endif

static struct regulator_consumer_supply s2m_ldo22_consumer =
	REGULATOR_SUPPLY("vcc_1.8v_tsp", NULL);

static struct regulator_consumer_supply s2m_ldo19_consumer =
	REGULATOR_SUPPLY("vcc_3.0_evt1", NULL);

static struct regulator_consumer_supply s2m_ldo20_consumer =
	REGULATOR_SUPPLY("vcc_1.8_evt1", NULL);

static struct regulator_consumer_supply s2m_ldo24_consumer =
	REGULATOR_SUPPLY("tsp_io", NULL);

static struct regulator_consumer_supply s2m_ldo25_consumer =
#ifdef CONFIG_EXYNOS5260_XYREF_REV0
	REGULATOR_SUPPLY("vcc_3.3v", NULL);
#else
	REGULATOR_SUPPLY("vcc_1.8v_peri_evt1", NULL);
#endif

static struct regulator_consumer_supply s2m_ldo26_consumer =
	REGULATOR_SUPPLY("vcc_1.1v_codec", NULL);

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
	.constraints	= {
		.name		= "vcc_1.0v_MIPI",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo6_consumer,
};

static struct regulator_init_data s2m_ldo7_data = {
	.constraints	= {
		.name		= "vcc_1.8v_MIPI",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo7_consumer,
};

static struct regulator_init_data s2m_ldo14_data = {
	.constraints	= {
		.name		= "Main Camera IO (1.8V)",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo14_consumer,
};

static struct regulator_init_data s2m_ldo15_data = {
	.constraints	= {
		.name		= "Camera Sensor (2.8V)",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= s2m_ldo15_consumer,
};

static struct regulator_init_data s2m_ldo16_data = {
	.constraints	= {
		.name		= "Main Camera AF (2.8V)",
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
	.consumer_supplies	= &s2m_ldo17_consumer,
};


#ifdef CONFIG_EXYNOS5260_XYREF_REV0
static struct regulator_init_data s2m_ldo18_data = {
	.constraints	= {
		.name		= "vcc_1.8v_LCD",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo18_consumer,
};
#else
static struct regulator_init_data s2m_ldo18_data = {
	.constraints	= {
		.name		= "tsp_avdd_3.3v",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo18_consumer,
};
#endif

static struct regulator_init_data s2m_ldo22_data = {
	.constraints	= {
		.name		= "tsp_avdd_1.8",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo22_consumer,
};

static struct regulator_init_data s2m_ldo19_data = {
	.constraints	= {
		.name		= "vcc_3.0_evt1",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo19_consumer,
};

static struct regulator_init_data s2m_ldo20_data = {
	.constraints	= {
		.name		= "vcc_1.8_evt1",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo20_consumer,
};

static struct regulator_init_data s2m_ldo24_data = {
	.constraints	= {
		.name		= "tsp_io range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo24_consumer,
};

static struct regulator_init_data s2m_ldo25_data = {
	.constraints	= {
#ifdef CONFIG_EXYNOS5260_XYREF_REV0
		.name		= "vcc_3.3v",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},

#else
		.name		= "vcc_1.8v",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.boot_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
#endif
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo25_consumer,
};

static struct regulator_init_data s2m_ldo26_data = {
	.constraints	= {
		.name		= "vcc_1.1v_codec range",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.enabled        = 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s2m_ldo26_consumer
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
	{S2MPA01_LDO14, &s2m_ldo14_data},
	{S2MPA01_LDO15, &s2m_ldo15_data},
	{S2MPA01_LDO16, &s2m_ldo16_data},
	{S2MPA01_LDO17, &s2m_ldo17_data},
	{S2MPA01_LDO18, &s2m_ldo18_data},
	{S2MPA01_LDO19, &s2m_ldo19_data},
	{S2MPA01_LDO20, &s2m_ldo20_data},
	{S2MPA01_LDO22, &s2m_ldo22_data},
	{S2MPA01_LDO24, &s2m_ldo24_data},
	{S2MPA01_LDO25, &s2m_ldo25_data},
	{S2MPA01_LDO26, &s2m_ldo26_data},
};

struct sec_opmode_data s2mpa01_opmode_data[S2MPA01_REG_MAX] = {
	[S2MPA01_BUCK1] = {S2MPA01_BUCK1, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK2] = {S2MPA01_BUCK2, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK3] = {S2MPA01_BUCK3, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK4] = {S2MPA01_BUCK4, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK6] = {S2MPA01_BUCK6, SEC_OPMODE_STANDBY},
	[S2MPA01_BUCK7] = {S2MPA01_BUCK7, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO6] = {S2MPA01_LDO6, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO7] = {S2MPA01_LDO7, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO14] = {S2MPA01_LDO14, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO15] = {S2MPA01_LDO15, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO16] = {S2MPA01_LDO16, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO17] = {S2MPA01_LDO17, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO18] = {S2MPA01_LDO18, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO19] = {S2MPA01_LDO19, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO20] = {S2MPA01_LDO20, SEC_OPMODE_STANDBY},
	[S2MPA01_LDO22] = {S2MPA01_LDO22, SEC_OPMODE_STANDBY},
#ifdef CONFIG_EXYNOS5260_XYREF_REV0
	[S2MPA01_LDO25] = {S2MPA01_LDO25, SEC_OPMODE_STANDBY},
#else
	[S2MPA01_LDO25] = {S2MPA01_LDO25, SEC_OPMODE_NORMAL},
#endif
	[S2MPA01_LDO26] = {S2MPA01_LDO26, SEC_OPMODE_NORMAL},
};

static int sec_cfg_pm_wrst(void)
{
	int gpio = XYREF5260_GPIO_WRST;

	if (gpio_request(gpio, "PM_WRST")) {
		pr_err("%s : PM_WRST request port erron", __func__);
	}

	gpio_direction_output(gpio, 1);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	return 0;
}

static int sec_cfg_irq(void)
{
	unsigned int pin = irq_to_gpio(XYREF5260_PMIC_EINT);

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
	.buck3_ramp_delay	= 12,
	.buck5_ramp_delay	= 1,
	.buck7_ramp_delay	= 1,
	.buck8910_ramp_delay	= 1,
	.buck1_ramp_enable	= 1,
	.buck2_ramp_enable	= 1,
	.buck3_ramp_enable	= 1,
	.buck4_ramp_enable	= 1,
};

#ifdef CONFIG_EXYNOS_THERMAL
static struct exynos_tmu_platform_data exynos5_tmu_data = {
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
		.freq_clip_max = 1500 * 1000,
		.temp_level = 80,
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		.mask_val = &mp_cluster_cpus[CA15],
#endif
	},
	.freq_tab[1] = {
		.freq_clip_max = 1300 * 1000,
		.temp_level = 85,
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		.mask_val = &mp_cluster_cpus[CA15],
#endif
	},
	.freq_tab[2] = {
		.freq_clip_max = 1000 * 1000,
		.temp_level = 90,
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		.mask_val = &mp_cluster_cpus[CA15],
#endif
	},
	.freq_tab[3] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 95,
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		.mask_val = &mp_cluster_cpus[CA15],
#endif
	},
	.freq_tab[4] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 100,
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		.mask_val = &mp_cluster_cpus[CA15],
#endif
	},
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	.freq_tab_kfc[0] = {
		.freq_clip_max = 1300 * 1000,
		.temp_level = 80,
		.mask_val = &mp_cluster_cpus[CA7],
	},
	.freq_tab_kfc[1] = {
		.freq_clip_max = 1300 * 1000,
		.temp_level = 85,
		.mask_val = &mp_cluster_cpus[CA7],
	},
	.freq_tab_kfc[2] = {
		.freq_clip_max = 1300 * 1000,
		.temp_level = 90,
		.mask_val = &mp_cluster_cpus[CA7],
	},
	.freq_tab_kfc[3] = {
		.freq_clip_max = 1300 * 1000,
		.temp_level = 95,
		.mask_val = &mp_cluster_cpus[CA7],
	},
	.freq_tab_kfc[4] = {
		.freq_clip_max = 1200 * 1000,
		.temp_level = 100,
		.mask_val = &mp_cluster_cpus[CA7],
	},
#endif
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 4,
	.freq_tab_count = 5,
	.type = SOC_ARCH_EXYNOS5,
	.clk_name = "xxti",
};
#endif

#define GPIO_CHG_INT		EXYNOS5260_GPX0(0)
static void xyref5260_chg_init(void)
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


static struct i2c_board_info hs_i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO("sec-pmic", 0xCC >> 1),
		.platform_data	= &exynos5_s2m_pdata,
		.irq		= XYREF5260_PMIC_EINT,
	},
};

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

struct exynos5_platform_i2c hs_i2c0_data __initdata = {
	.bus_number = 0,
	.operation_mode = HSI2C_POLLING,
	.speed_mode = HSI2C_HIGH_SPD,
	.fast_speed = 400000,
	.high_speed = 2500000,
	.cfg_gpio = NULL,
};

struct s3c2410_platform_i2c i2c_data_chg __initdata = {
	.bus_num	= 5,
	.flags		= 0,
	.slave_addr	= 0xD6,
	.frequency	= 400*1000,
	.sda_delay	= 100,
};

static struct platform_device *xyref5260_power_devices[] __initdata = {
	&exynos5_device_hs_i2c0,
	&s3c_device_i2c1,
#ifdef CONFIG_EXYNOS_THERMAL
	&exynos5260_device_tmu,
#endif
};

void __init exynos5_xyref5260_power_init(void)
{
	exynos5_hs_i2c0_set_platdata(&hs_i2c0_data);
	i2c_register_board_info(0, hs_i2c_devs0, ARRAY_SIZE(hs_i2c_devs0));
	s3c_i2c1_set_platdata(&i2c_data_chg);
	i2c_register_board_info(5, i2c_devs_chg, ARRAY_SIZE(i2c_devs_chg));
	xyref5260_chg_init();
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_set_platdata(&exynos5_tmu_data);
#endif
	platform_add_devices(xyref5260_power_devices,
			ARRAY_SIZE(xyref5260_power_devices));
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	init_mp_cpumask_set();
#endif
}
