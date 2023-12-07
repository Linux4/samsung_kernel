/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/pm_opp.h>
#include <soc/qcom/msm_cpu_voltage.h>

#define MAX_FREQ_GAP_KHZ 200000

int msm_match_cpu_voltage(unsigned long input_freq)
{
	struct dev_pm_opp *opp = NULL;
	struct device *cls0_dev = NULL;
	struct device *cls1_dev = NULL;
	unsigned long freq = 0;
	unsigned long input_volt = 0;
	unsigned long input_freq_actual = 0;
	unsigned long target_freq = 0;
	unsigned long temp_freq = 0;
	unsigned long temp_volt = 0;
	int i, max_opps_cls0 = 0;

	cls0_dev = get_cpu_device(0);
	if (!cls0_dev) {
		pr_err("Error getting CPU0 device\n");
		return 0;
	}
	cls1_dev = get_cpu_device(2);
	if (!cls1_dev) {
		pr_err("Error getting CPU2 device\n");
		return 0;
	}

	/*
	 * Parse Cluster 1 OPP table for closest freq to input_freq
	 * and save its voltage
	 */
	freq = input_freq * 1000;
	rcu_read_lock();
	opp = dev_pm_opp_find_freq_ceil(cls1_dev, &freq);
	if (IS_ERR(opp)) {
		pr_err("Error: could not find freq close to input_freq: %lu\n", input_freq);
		goto exit;
	}
	input_freq_actual = freq / 1000;
	input_volt = dev_pm_opp_get_voltage(opp);
	if (input_volt == 0) {
		pr_err("Error getting OPP voltage on cluster 1\n");
		goto exit;
	}

	/*
	 * Parse Cluster 0 OPP table for closest voltage to input_volt
	 * and iterate to end to save max freq of Cluster 0
	 */
	max_opps_cls0 = dev_pm_opp_get_opp_count(cls0_dev);
	if (max_opps_cls0 <= 0)
		pr_err("Error getting OPP count for CPU0\n");
	for (i = 0, freq = 0; i < max_opps_cls0; i++, freq++) {
		opp = dev_pm_opp_find_freq_ceil(cls0_dev, &freq);
		if (IS_ERR(opp)) {
			pr_err("Error getting OPP freq on cluster 0\n");
			goto exit;
		}
		temp_freq = freq / 1000;
		temp_volt = dev_pm_opp_get_voltage(opp);
		/* Compare voltage and continue to next frequency if gap is too large */
		if ((target_freq == 0) && (temp_volt > input_volt)
				&& ((temp_freq + MAX_FREQ_GAP_KHZ) >= input_freq_actual)) {
			/* block freq higher than input_freq */
			if (temp_freq > input_freq)
				target_freq = input_freq;
			else
				target_freq = temp_freq;
		}
	}

	/*
	 * If we didn't find a frequency, either input voltage is too high
	 * or gap was too large (input frequency was too high)
	 * In either case, return Cluster 0 Fmax as this is the best possible freq
	 * Also, if input_freq is > Cluster 0 Fmax, return Cluster 0 Fmax
	 */
	if (target_freq == 0 || input_freq > temp_freq)
		target_freq = temp_freq;
exit:
	rcu_read_unlock();
	return target_freq;
}
EXPORT_SYMBOL(msm_match_cpu_voltage);
