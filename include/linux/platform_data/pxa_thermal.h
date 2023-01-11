/*
 * pxa_thermal.h - Marvell PXA TMU (Thermal Management Unit)
 *
 * Author:      Liang Chen <chl@marvell.com>
 * Copyright:   (C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_PXA_THERMAL_H
#define _LINUX_PXA_THERMAL_H
#include <linux/cpu_cooling.h>

enum calibration_type {
	TYPE_ONE_POINT_TRIMMING,
	TYPE_TWO_POINT_TRIMMING,
	TYPE_NONE,
};

enum soc_type {
	SOC_ARCH_PXA1088 = 1,
	SOC_ARCH_PXA1L88,
};
/**
 * struct freq_clip_table
 * @freq_clip_max: maximum frequency allowed for this cooling state.
 * @temp_level: Temperature level at which the temperature clipping will
 *	happen.
 * @mask_val: cpumask of the allowed cpu's where the clipping will take place.
 *
 * This structure is required to be filled and passed to the
 * cpufreq_cooling_unregister function.
 */
struct freq_clip_table {
	unsigned int freq_clip_max;
	unsigned int temp_level;
	const struct cpumask *mask_val;
};

/**
 * struct pxa_tmu_platform_data
 * @threshold: basic temperature for generating interrupt
 *	       25 <= threshold <= 125 s[unit: degree Celsius]
 * @threshold_falling: differntial value for setting threshold
 *		       of temperature falling interrupt.
 * @trigger_levels: array for each interrupt levels
 *	[unit: degree Celsius]
 * @type: determines the type of SOC
 * @efuse_value: platform defined fuse value
 * @cal_type: calibration type for temperature
 * @freq_clip_table: Table representing frequency reduction percentage.
 * @freq_tab_count: Count of the above table as frequency reduction may
 *	applicable to only some of the trigger levels.
 *
 * This structure is required for configuration of pxa_tmu driver.
 */
struct pxa_tmu_platform_data {
	u8 threshold;
	u8 trigger_levels[4];
	u8 trigger_levels_count;

	enum soc_type type;
	struct freq_clip_table freq_tab[4];
	unsigned int freq_tab_count;
};
#endif /* _LINUX_PXA_THERMAL_H */
