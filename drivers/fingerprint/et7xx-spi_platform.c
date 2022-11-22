/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "fingerprint.h"
#include "et7xx.h"
#include <linux/cpufreq.h>

int fps_resume_set(void) {
	return 0;
}

int fps_suspend_set(struct et7xx_data *etspi) {
	return 0;
}

int et7xx_register_platform_variable(struct et7xx_data *etspi)
{
	return 0;
}

int et7xx_unregister_platform_variable(struct et7xx_data *etspi)
{
	return 0;
}

int et7xx_spi_clk_enable(struct et7xx_data *etspi)
{
	int retval = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (!etspi->enabled_clk) {
        wake_lock(&etspi->fp_spi_lock);
		etspi->enabled_clk = true;
	}
#endif
	return retval;
}

int et7xx_spi_clk_disable(struct et7xx_data *etspi)
{
	int retval = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (etspi->enabled_clk) {
		wake_unlock(&etspi->fp_spi_lock);
		etspi->enabled_clk = false;
	}
#endif
	return retval;
}

int et7xx_set_cpu_speedup(struct et7xx_data *etspi, int onoff)
{
	int retval = 0;
#ifdef CONFIG_CPU_FREQ_LIMIT_USERSPACE
	int retry_cnt = 0;

	if (etspi->min_cpufreq_limit) {
		if (onoff) {
			pr_info("FP_CPU_SPEEDUP ON\n");
			pm_qos_add_request(&etspi->pm_qos, PM_QOS_CPU_DMA_LATENCY, 0);
			do {
				retval = set_freq_limit(DVFS_FINGER_ID, etspi->min_cpufreq_limit);
				retry_cnt++;
				if (retval) {
					pr_err("booster start failed. (%d) retry: %d\n",
						retval, retry_cnt);
					usleep_range(500, 510);
				}
			} while (retval && retry_cnt < 7);
		} else {
			pr_info("FP_CPU_SPEEDUP OFF\n");
			retval = set_freq_limit(DVFS_FINGER_ID, -1);
			if (retval)
				pr_err("booster stop failed. (%d)\n", retval);
			pm_qos_remove_request(&etspi->pm_qos);
		}
	}
#else
	pr_info("FP_CPU_SPEEDUP does not set\n");
#endif
	return retval;
}
