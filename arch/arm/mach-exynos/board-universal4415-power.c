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
	.type = SOC_ARCH_EXYNOS5,
	.clk_name = "tmu_apbif",
};
#endif

#ifdef CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ
static struct platform_device exynos4415_int_devfreq = {
	.name	= "exynos4415-busfreq-int",
	.id	= -1,
};

static struct exynos_devfreq_platdata exynos4415_qos_int_pd __initdata = {
	/* locking the INT min level to L2 : 133000MHz */
	.default_qos = 100000,
};

static struct platform_device exynos4415_mif_devfreq = {
	.name	= "exynos4415-busfreq-mif",
	.id	= -1,
};

static struct exynos_devfreq_platdata exynos4415_qos_mif_pd __initdata = {
	/* locking the MIF min level to L3 : 206000MHz */
	.default_qos = 103000,
};
#endif

static struct platform_device *universal4415_power_devices[] __initdata = {
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
};

void __init exynos4_universal4415_power_init(void)
{
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_set_platdata(&exynos4415_tmu_data);
#endif

#ifdef CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ
	s3c_set_platdata(&exynos4415_qos_int_pd,
			sizeof(struct exynos_devfreq_platdata),
			&exynos4415_int_devfreq);
	s3c_set_platdata(&exynos4415_qos_mif_pd,
			sizeof(struct exynos_devfreq_platdata),
			&exynos4415_mif_devfreq);
#endif

	platform_add_devices(universal4415_power_devices,
			ARRAY_SIZE(universal4415_power_devices));

}
