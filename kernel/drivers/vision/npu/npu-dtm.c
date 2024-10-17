/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/printk.h>
#include <linux/cpuidle.h>
#include <soc/samsung/bts.h>
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos-devfreq.h>
#endif

#include "npu-config.h"
#include "npu-scheduler-governor.h"
#include "npu-device.h"
#include "npu-llc.h"
#include "npu-util-regs.h"
#include "npu-hw-device.h"

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
bool npu_dtm_get_flag(void)
{
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
	return (info->curr_thermal >= info->dtm_nm_lut[0]) ? TRUE : FALSE;
}

void npu_dtm_set(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;
	int npu_thermal = 0;
	s64 pid_val;
	s64 thermal_err;
	int idx_prev;
	int idx_curr;
	s64 err_sum = 0;
	int *th_err;
	int period = 1;
	int i;
	int qt_freq = 0;

	if (list_empty(&info->ip_list)) {
		npu_err("[PID]no device for scheduler\n");
		return;
	}
	th_err = &info->th_err_db[0];

	period = info->pid_period;
	thermal_zone_get_temp(info->npu_tzd, &npu_thermal);
	info->curr_thermal= npu_thermal;

	//NORMAL MODE Loop
	if ((info->mode == NPU_PERF_MODE_NORMAL ||
			info->mode == NPU_PERF_MODE_NPU_BOOST_ONEXE) &&
			info->pid_en == 0) {

		if (npu_thermal >= info->dtm_nm_lut[0]) {
			qt_freq = npu_scheduler_get_clk(info, npu_scheduler_get_lut_idx(info, info->dtm_nm_lut[1], DIT_CORE), DIT_CORE);
			npu_dbg("NORMAL mode DTM set freq : %d\n", qt_freq);
		}
		else {
			qt_freq = npu_scheduler_get_clk(info, 0, DIT_CORE);
		}
		if (info->dtm_prev_freq != qt_freq) {
			info->dtm_prev_freq = qt_freq;
			mutex_lock(&info->exec_lock);
			list_for_each_entry(d, &info->ip_list, ip_list) {
				if (!strcmp("NPU0", d->name) || !strcmp("NPU1", d->name) || !strcmp("DSP", d->name)) {
					npu_dvfs_set_freq(d, &d->qos_req_max, qt_freq);
				}
				if (!strcmp("DNC", d->name)) {
					npu_dvfs_set_freq(d, &d->qos_req_max,
						npu_scheduler_get_clk(info, npu_scheduler_get_lut_idx(info, qt_freq, DIT_CORE), DIT_DNC));
				}
			}
			mutex_unlock(&info->exec_lock);
		}
#ifdef CONFIG_NPU_USE_PI_DTM_DEBUG
		info->debug_log[info->debug_cnt][0] = info->idx_cnt;
		info->debug_log[info->debug_cnt][1] = npu_thermal/1000;
		info->debug_log[info->debug_cnt][2] = info->dtm_prev_freq/1000;
		if (info->debug_cnt < PID_DEBUG_CNT - 1)
			info->debug_cnt += 1;
#endif
	}
	//PID MODE Loop
	else if (info->idx_cnt % period == 0) {
		if (info->pid_target_thermal == NPU_SCH_DEFAULT_VALUE)
			return;

		idx_curr = info->idx_cnt / period;
		idx_prev = (idx_curr - 1) % PID_I_BUF_SIZE;
		idx_curr = (idx_curr) % PID_I_BUF_SIZE;
		thermal_err = (int)info->pid_target_thermal - npu_thermal;

		if (thermal_err < 0)
			thermal_err = (thermal_err * info->pid_inv_gain) / 100;

		th_err[idx_curr] = thermal_err;

		for (i = 0 ; i < PID_I_BUF_SIZE ; i++)
			err_sum += th_err[i];

		pid_val = (s64)(info->pid_p_gain * thermal_err) + (s64)(info->pid_i_gain * err_sum);

		info->dtm_curr_freq += pid_val / 100;	//for int calculation

		if (info->dtm_curr_freq > PID_MAX_FREQ_MARGIN + info->pid_max_clk)
			info->dtm_curr_freq = PID_MAX_FREQ_MARGIN + info->pid_max_clk;

		qt_freq = npu_scheduler_get_clk(info, npu_scheduler_get_lut_idx(info, info->dtm_curr_freq, DIT_CORE), DIT_CORE);

		if (info->dtm_prev_freq != qt_freq) {
			info->dtm_prev_freq = qt_freq;
			mutex_lock(&info->exec_lock);
			list_for_each_entry(d, &info->ip_list, ip_list) {
				if (!strcmp("NPU0", d->name) || !strcmp("NPU1", d->name) || !strcmp("DSP", d->name)) {
					//npu_dvfs_set_freq(d, &d->qos_req_min, qt_freq);
					npu_dvfs_set_freq(d, &d->qos_req_max, qt_freq);
				}
				if (!strcmp("DNC", d->name)) {
					npu_dvfs_set_freq(d, &d->qos_req_max,
						npu_scheduler_get_clk(info, npu_scheduler_get_lut_idx(info, qt_freq, DIT_CORE), DIT_DNC));
				}
			}
			mutex_unlock(&info->exec_lock);
			npu_dbg("BOOST mode DTM set freq : %d->%d\n", info->dtm_prev_freq, qt_freq);
		}

#ifdef CONFIG_NPU_USE_PI_DTM_DEBUG
		info->debug_log[info->debug_cnt][0] = info->idx_cnt;
		info->debug_log[info->debug_cnt][1] = npu_thermal/1000;
		info->debug_log[info->debug_cnt][2] = info->dtm_curr_freq/1000;
		if (info->debug_cnt < PID_DEBUG_CNT - 1)
			info->debug_cnt += 1;
#endif

	}
	info->idx_cnt = info->idx_cnt + 1;
}
#endif

