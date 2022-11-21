/*
 * Copyright (C) 2018 Samsung Electronics. All rights reserved.
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
#include "qbt2000_common.h"
#ifdef CONFIG_CPU_FREQ_LIMIT
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
#define LIMIT_RELEASE	-1
extern int set_freq_limit(unsigned int id, unsigned int freq);
#endif

int qbt2000_set_clk(struct qbt2000_drvdata *drvdata, bool onoff)
{
	int rc = 0;

	if (drvdata->enabled_clk == onoff) {
		pr_err("already %s\n", onoff ? "enabled" : "disabled");
		return rc;
	}

	if (onoff) {
		__pm_stay_awake(drvdata->fp_spi_lock);
		drvdata->enabled_clk = true;
	} else {
		__pm_relax(drvdata->fp_spi_lock);
		drvdata->enabled_clk = false;
	}
	return rc;
}

int qbt2000_register_platform_variable(struct qbt2000_drvdata *drvdata)
{
	return 0;
}

int qbt2000_unregister_platform_variable(struct qbt2000_drvdata *drvdata)
{
	return 0;
}

int qbt2000_set_cpu_speedup(struct qbt2000_drvdata *drvdata, int onoff)
{
#ifdef CONFIG_CPU_FREQ_LIMIT
	if (drvdata->min_cpufreq_limit) {
		if (onoff) {
			pr_info("FP_CPU_SPEEDUP ON:%d\n", onoff);
			pm_qos_add_request(&drvdata->pm_qos, PM_QOS_CPU_DMA_LATENCY, 0);
			set_freq_limit(CFLM_FINGER, drvdata->min_cpufreq_limit);
		} else {
			pr_info("FP_CPU_SPEEDUP OFF\n");
			set_freq_limit(CFLM_FINGER, LIMIT_RELEASE);
			pm_qos_remove_request(&drvdata->pm_qos);
		}
	}
#else
	pr_info("FP_CPU_SPEEDUP does not set\n");
#endif
	return 0;
}

int fps_resume_set(void) {
	return 0;
}

int qbt2000_pinctrl_register(struct qbt2000_drvdata *drvdata)
{
	drvdata->p = NULL;
	drvdata->pins_poweroff = NULL;
	drvdata->pins_poweron = NULL;

	return 0;
}

