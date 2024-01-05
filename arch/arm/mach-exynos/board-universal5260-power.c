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
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <asm/io.h>

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

#include "board-universal5260.h"

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
		.freq_clip_max = 1600 * 1000,
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

static struct platform_device *universal5260_power_devices[] __initdata = {
#ifdef CONFIG_EXYNOS_THERMAL
	&exynos5260_device_tmu,
#endif
};

void __init exynos5_universal5260_power_init(void)
{
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_set_platdata(&exynos5_tmu_data);
#endif

	platform_add_devices(universal5260_power_devices,
			ARRAY_SIZE(universal5260_power_devices));
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	init_mp_cpumask_set();
#endif
}
