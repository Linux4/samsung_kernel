// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fb.h>
#include <mt-plat/mtk_gpu_utility.h>
#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#include <gpufreq_v2.h>
#endif
#include "ged_base.h"
#include "ged_hal.h"
#include "ged_sysfs.h"

#include "ged_dvfs.h"

#include "ged_notify_sw_vsync.h"
#include "ged_kpi.h"
#include "ged_global.h"
#include "ged_dcs.h"
#include "ged_eb.h"
#if defined(MTK_GPU_SLC_POLICY)
#include "ged_gpu_slc.h"
#endif /* MTK_GPU_SLC_POLICY */

#include "ged_debugFS.h"
static struct dentry *gpsHALDir;
static struct dentry *gpsOppCostsEntry;
#define GED_HAL_DEBUGFS_SIZE 64
uint64_t reset_base_us;

static struct kobject *hal_kobj;

int tokenizer(char *pcSrc, int i32len, int *pi32IndexArray, int i32NumToken)
{
	int i = 0;
	int j = 0;
	int head = -1;

	for ( ; i < i32len; i++) {
		if (pcSrc[i] != ' ') {
			if (head == -1)
				head = i;
		} else {
			if (head != -1) {
				pi32IndexArray[j] = head;
				j++;
				if (j == i32NumToken)
					return j;
				head = -1;
			}
			pcSrc[i] = 0;
		}
	}

	if (head != -1) {
		pi32IndexArray[j] = head;
		j++;
		return j;
	}

	return -1;
}

static ssize_t ged_dvfs_opp_cost_write_entry(const char __user *pszBuffer,
		size_t uiCount, loff_t uiPosition, void *pvData)
{
	char acBuffer[GED_HAL_DEBUGFS_SIZE] = {0};
	int i32Value = 0;

	if ((uiCount > 0) && (uiCount < GED_HAL_DEBUGFS_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			acBuffer[uiCount] = '\0';
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				ged_dvfs_reset_opp_cost(i32Value);
				reset_base_us = ged_get_time();
				reset_base_us = reset_base_us >> 10;
			}
		}
	}

	return uiCount;
}

static void *ged_dvfs_opp_cost_seq_start(struct seq_file *psSeqFile,
		loff_t *puiPosition)
{
	if (*puiPosition == 0)
		return SEQ_START_TOKEN;

	return NULL;
}

static void ged_dvfs_opp_cost_seq_stop(struct seq_file *psSeqFile,
		void *pvData)
{

}

static void *ged_dvfs_opp_cost_seq_next(struct seq_file *psSeqFile,
		void *pvData, loff_t *puiPosition)
{
	return NULL;
}

static int ged_dvfs_opp_cost_seq_show(struct seq_file *psSeqFile,
		void *pvData)
{
	int len = 0;
	char acBuffer[MAX_OPP_LOGS_COLUMN_DIGITS] = {0};

	if (pvData != NULL) {
		int i = 0, j = 0, cur_idx = 0;
		unsigned int ui32FqCount = g_real_oppfreq_num;
		unsigned int ui32TotalTrans = 0;
		struct GED_DVFS_OPP_STAT *report = NULL;
		u64 last_ts = 0;

		if (ui32FqCount)
			report = vmalloc(sizeof(struct GED_DVFS_OPP_STAT) * ui32FqCount);

		if ((report != NULL) &&
			!ged_dvfs_query_opp_cost(report, ui32FqCount, false, &last_ts)) {

			cur_idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
			cur_idx = (cur_idx > g_real_minfreq_idx)? g_real_minfreq_idx: cur_idx;
			ui32TotalTrans = 0;

			seq_puts(psSeqFile, "From(MHz): To(MHz)\n");
			seq_puts(psSeqFile, "        ");

			for (i = 0; i < ui32FqCount; i++) {
				len = scnprintf(acBuffer, MAX_OPP_LOGS_COLUMN_DIGITS,
						"%10u", gpufreq_get_freq_by_idx(TARGET_DEFAULT, i)/1000);
				acBuffer[len] = 0;
				seq_puts(psSeqFile, acBuffer);
			}

			seq_puts(psSeqFile, "   time(ms)\n");

			for (i = 0; i < ui32FqCount; i++) {
				if (i == cur_idx)
					seq_puts(psSeqFile, "*");
				else
					seq_puts(psSeqFile, " ");

				len = scnprintf(acBuffer, MAX_OPP_LOGS_COLUMN_DIGITS,
						"%4u   ", gpufreq_get_freq_by_idx(TARGET_DEFAULT, i)/1000);
				acBuffer[len] = 0;
				seq_puts(psSeqFile, acBuffer);
				for (j = 0; j < ui32FqCount; j++) {
					len = scnprintf(acBuffer, MAX_OPP_LOGS_COLUMN_DIGITS,
							"%10u", report[i].uMem.aTrans[j]);
					acBuffer[len] = 0;
					seq_puts(psSeqFile, acBuffer);
					ui32TotalTrans += report[i].uMem.aTrans[j];
				}
				/* truncate to ms */
				len = scnprintf(acBuffer, MAX_OPP_LOGS_COLUMN_DIGITS,
						"%10u\n", (unsigned int)(report[i].ui64Active) >> 10);
				acBuffer[len] = 0;
				seq_puts(psSeqFile, acBuffer);
			}

			len = scnprintf(acBuffer, MAX_OPP_LOGS_COLUMN_DIGITS,
					"Total transition : %u\n", ui32TotalTrans);
			acBuffer[len] = 0;
			seq_puts(psSeqFile, acBuffer);
		} else {
			seq_puts(psSeqFile, "Not Supported.\n");
		}

		if (report != NULL)
			vfree(report);
	}

	return 0;
}
const struct seq_operations gsDvfsOppCostsReadOps = {
	.start = ged_dvfs_opp_cost_seq_start,
	.stop = ged_dvfs_opp_cost_seq_stop,
	.next = ged_dvfs_opp_cost_seq_next,
	.show = ged_dvfs_opp_cost_seq_show,
};

static ssize_t opp_logs_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int len = 0, i = 0, j = 0, cur_idx = 0;
	unsigned int ui32FqCount = g_real_oppfreq_num;
	struct GED_DVFS_OPP_STAT *report = NULL;
	u64 last_ts = 0;

	if (ui32FqCount)
		report = vmalloc(sizeof(struct GED_DVFS_OPP_STAT) * ui32FqCount);

	if ((report != NULL) &&
		ged_dvfs_query_opp_cost(report, ui32FqCount, false, &last_ts) == 0) {
		cur_idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
		cur_idx = (cur_idx > g_real_minfreq_idx)? g_real_minfreq_idx: cur_idx;
		len = sprintf(buf, "Last TS: %llu\n", last_ts);
		len += sprintf(buf + len, "GPU Freq(MHz)  Time(ms)\n");

		for (i = 0; i < ui32FqCount; i++) {
			if (i == cur_idx)
				len += sprintf(buf + len, "*");
			else
				len += sprintf(buf + len, " ");

			len += sprintf(buf + len, "%4u    ",
					gpufreq_get_freq_by_idx(TARGET_DEFAULT, i) / 1000);

			/* truncate to ms */
			len += sprintf(buf + len, "%llu\n", report[i].ui64Active);
		}
	} else
		len = sprintf(buf, "Not Supported.\n");

	if (report != NULL)
		vfree(report);

	return len;
}
static KOBJ_ATTR_RO(opp_logs);

static ssize_t gpu_sum_loading_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u64 sum_loading = 0, sum_delta_time = 0;

	ged_dvfs_query_loading(&sum_loading, &sum_delta_time);

	return scnprintf(buf, PAGE_SIZE, "%llu %llu\n", sum_loading, sum_delta_time);
}
static KOBJ_ATTR_RO(gpu_sum_loading);

static ssize_t gpu_power_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u64 off_time = 0, idle_time = 0, on_time = 0;
	u64 last_ts = 0;

	ged_dvfs_query_power_state_time(&off_time, &idle_time, &on_time, &last_ts);

	return scnprintf(buf, PAGE_SIZE, "last_ts:%llu off: %llu idle: %llu on: %llu\n",
			last_ts, off_time, idle_time, on_time);
}
static KOBJ_ATTR_RO(gpu_power_state);
//-----------------------------------------------------------------------------
static ssize_t total_gpu_freq_level_count_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32FreqLevelCount;

	if (false == mtk_custom_get_gpu_freq_level_count(&ui32FreqLevelCount))
		ui32FreqLevelCount = 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32FreqLevelCount);
}

static KOBJ_ATTR_RO(total_gpu_freq_level_count);
//-----------------------------------------------------------------------------
static ssize_t custom_boost_gpu_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32BoostGpuFreqLevel = 0;

	ui32BoostGpuFreqLevel = ged_dvfs_get_custom_boost_gpu_freq();

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32BoostGpuFreqLevel);
}

static ssize_t custom_boost_gpu_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				mtk_custom_boost_gpu_freq(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(custom_boost_gpu_freq);
//-----------------------------------------------------------------------------
static ssize_t custom_upbound_gpu_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32UpboundGpuFreqLevel = 0;

	ui32UpboundGpuFreqLevel = ged_dvfs_get_custom_ceiling_gpu_freq();

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32UpboundGpuFreqLevel);
}

static ssize_t custom_upbound_gpu_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				mtk_custom_upbound_gpu_freq(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(custom_upbound_gpu_freq);
//-----------------------------------------------------------------------------
static ssize_t current_freqency_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_DVFS_FREQ_DATA sFreqInfo;

	ged_dvfs_get_gpu_cur_freq(&sFreqInfo);

	return scnprintf(buf, PAGE_SIZE, "%u %lu\n",
		sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
}

static KOBJ_ATTR_RO(current_freqency);
//-----------------------------------------------------------------------------
static ssize_t previous_freqency_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_DVFS_FREQ_DATA sFreqInfo;

	ged_dvfs_get_gpu_pre_freq(&sFreqInfo);

	return scnprintf(buf, PAGE_SIZE, "%u %lu\n",
		sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
}

static KOBJ_ATTR_RO(previous_freqency);
//-----------------------------------------------------------------------------
static ssize_t gpu_utilization_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int loading = 0;
	unsigned int block = 0;
	unsigned int idle = 0;

	mtk_get_gpu_loading(&loading);
	mtk_get_gpu_block(&block);
	mtk_get_gpu_idle(&idle);

	return scnprintf(buf, PAGE_SIZE, "%u %u %u\n", loading, block, idle);
}

static KOBJ_ATTR_RO(gpu_utilization);
//-----------------------------------------------------------------------------
static int32_t _boost_level = -1;
#define MAX_BOOST_DIGITS 10
static ssize_t gpu_boost_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", _boost_level);
}

static ssize_t gpu_boost_level_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char str_num[MAX_BOOST_DIGITS];
	long val;

	if (count > 0 && count < MAX_BOOST_DIGITS) {
		if (scnprintf(str_num, MAX_BOOST_DIGITS, "%s", buf)) {
			if (kstrtol(str_num, 10, &val) == 0)
				_boost_level = (int32_t)val;
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gpu_boost_level);
//-----------------------------------------------------------------------------
int ged_dvfs_boost_value(void)
{
	return _boost_level;
}

//-----------------------------------------------------------------------------
#ifdef MTK_GED_KPI
static ssize_t ged_kpi_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	unsigned int fps;
	unsigned int cpu_time;
	unsigned int gpu_time;
	unsigned int response_time;
	unsigned int gpu_remained_time;
	unsigned int cpu_remained_time;
	unsigned int gpu_freq;

	fps = ged_kpi_get_cur_fps();
	cpu_time = ged_kpi_get_cur_avg_cpu_time();
	gpu_time = ged_kpi_get_cur_avg_gpu_time();
	response_time = ged_kpi_get_cur_avg_response_time();
	cpu_remained_time = ged_kpi_get_cur_avg_cpu_remained_time();
	gpu_remained_time = ged_kpi_get_cur_avg_gpu_remained_time();
	gpu_freq = ged_kpi_get_cur_avg_gpu_freq();

	return scnprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u,%u\n",
			fps, cpu_time, gpu_time, response_time,
			cpu_remained_time, gpu_remained_time, gpu_freq);
}

static KOBJ_ATTR_RO(ged_kpi);
#endif /* MTK_GED_KPI */
//-------------------------------------------------------------------------
extern unsigned int early_force_fallback_enable;
static ssize_t early_force_fallback_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{

	int pos = 0;

	if (early_force_fallback_enable)
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
			"early_force_fallback_policy is enabled\n");
	else
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
			"early_force_fallback_policy is disabled\n");

	return pos;
}
static ssize_t early_force_fallback_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 256)
					early_force_fallback_enable = 1;
				else
					early_force_fallback_enable = 0;
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(early_force_fallback_policy);

//-----------------------------------------------------------------------------
static ssize_t dvfs_margin_value_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32DvfsMarginValue;
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_margin_value(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_dvfs_margin_value false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", i32DvfsMarginValue);
	pos += length;

	return pos;
}

static ssize_t dvfs_margin_value_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_margin_value(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_margin_value);
//-----------------------------------------------------------------------------
static ssize_t loading_base_dvfs_step_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32StepValue;
	int pos = 0;
	int length;

	if (false == mtk_get_loading_base_dvfs_step(&i32StepValue)) {
		i32StepValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_loading_base_dvfs_step false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%x\n", i32StepValue);
	pos += length;

	return pos;
}

static ssize_t loading_base_dvfs_step_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_loading_base_dvfs_step(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(loading_base_dvfs_step);
//-----------------------------------------------------------------------------
static ssize_t timer_base_dvfs_margin_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32DvfsMarginValue;
	int pos = 0;
	int length;

	if (false == mtk_get_timer_base_dvfs_margin(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_timer_base_dvfs_margin false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", i32DvfsMarginValue);
	pos += length;

	return pos;
}

static ssize_t timer_base_dvfs_margin_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_timer_base_dvfs_margin(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(timer_base_dvfs_margin);


static ssize_t dvfs_loading_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32DvfsLoadingMode;
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_loading_mode(&ui32DvfsLoadingMode)) {
		ui32DvfsLoadingMode = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_dvfs_loading_mode false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", ui32DvfsLoadingMode);
	pos += length;

	return pos;
}

static ssize_t dvfs_loading_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_loading_mode(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_loading_mode);

static ssize_t dvfs_workload_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32DvfsWorkloadMode;
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_workload_mode(&ui32DvfsWorkloadMode)) {
		ui32DvfsWorkloadMode = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_dvfs_workload_mode false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", ui32DvfsWorkloadMode);
	pos += length;

	return pos;
}

static ssize_t dvfs_workload_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_workload_mode(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_workload_mode);

//-----------------------------------------------------------------------------

static ssize_t reduce_mips_dvfs_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int reduce_mips_support;
	unsigned int eb_policy_mode;
	int pos = 0;
	int length;

	reduce_mips_support = is_fdvfs_enable();
	eb_policy_mode = ged_dvfs_get_recude_mips_policy_state();

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"dvfs recude-mips, eb_policy(%d), dts support(%d)\n",
			eb_policy_mode, reduce_mips_support);

	pos += length;

	return pos;
}

static ssize_t reduce_mips_dvfs_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
				if (mips_support_flag)
					mtk_set_fastdvfs_mode(u32Value);
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(reduce_mips_dvfs);

//-----------------------------------------------------------------------------

static ssize_t frame_base_optimize_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32FastDVFSMode;
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"%d\n", g_ged_frame_base_optimize);

	pos += length;

	return pos;
}

static ssize_t frame_base_optimize_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				g_ged_frame_base_optimize = i32Value;
		}
	}

	return count;

}
static KOBJ_ATTR_RW(frame_base_optimize);

//-----------------------------------------------------------------------------

static ssize_t pre_fence_chk_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32FastDVFSMode;
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"%d\n", g_ged_pre_fence_chk);

	pos += length;

	return pos;
}

static ssize_t pre_fence_chk_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				g_ged_pre_fence_chk = i32Value;
		}
	}

	return count;

}
static KOBJ_ATTR_RW(pre_fence_chk);

//-----------------------------------------------------------------------------

static struct notifier_block ged_fb_notifier;

static int ged_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		g_ui32EventStatus |= GED_EVENT_LCD;
		ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	case FB_BLANK_POWERDOWN:
		g_ui32EventStatus &= ~GED_EVENT_LCD;
		ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	default:
		break;
	}

	return 0;
}

struct ged_event_change_entry_t {
	ged_event_change_fp callback;
	void *private_data;
	char name[128];
	struct list_head sList;
};

static struct {
	struct mutex lock;
	struct list_head listen;
} g_ged_event_change = {
	.lock     = __MUTEX_INITIALIZER(g_ged_event_change.lock),
	.listen   = LIST_HEAD_INIT(g_ged_event_change.listen),
};

bool mtk_register_ged_event_change(const char *name,
	ged_event_change_fp callback, void *private_data)
{
	struct ged_event_change_entry_t *entry = NULL;

	entry = kmalloc(sizeof(struct ged_event_change_entry_t), GFP_KERNEL);
	if (entry == NULL)
		return false;

	entry->callback = callback;
	entry->private_data = private_data;
	strncpy(entry->name, name, sizeof(entry->name) - 1);
	entry->name[sizeof(entry->name) - 1] = 0;
	INIT_LIST_HEAD(&entry->sList);

	mutex_lock(&g_ged_event_change.lock);

	list_add(&entry->sList, &g_ged_event_change.listen);

	mutex_unlock(&g_ged_event_change.lock);

	return true;
}

bool mtk_unregister_ged_event_change(const char *name)
{
	struct list_head *pos, *head;
	struct ged_event_change_entry_t *entry = NULL;

	mutex_lock(&g_ged_event_change.lock);

	head = &g_ged_event_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct ged_event_change_entry_t, sList);
		if (strncmp(entry->name, name, sizeof(entry->name) - 1) == 0)
			break;
		entry = NULL;
	}

	if (entry) {
		list_del(&entry->sList);
		kfree(entry);
	}

	mutex_unlock(&g_ged_event_change.lock);

	return true;
}

void mtk_ged_event_notify(int events)
{
	struct list_head *pos, *head;
	struct ged_event_change_entry_t *entry = NULL;

	mutex_lock(&g_ged_event_change.lock);

	head = &g_ged_event_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct ged_event_change_entry_t, sList);
		entry->callback(entry->private_data, events);
	}

	mutex_unlock(&g_ged_event_change.lock);
}
//------------------------------------------------------------------------------
extern unsigned int force_loading_based_enable;
static ssize_t force_loading_base_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{

	int pos = 0;

	if (force_loading_based_enable)
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,"force_loading_base is enabled\n");
	else
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,"force_loading_base is disabled\n");

	return pos;
}
static ssize_t force_loading_base_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 256)
					force_loading_based_enable = 1;
				else
					force_loading_based_enable = 0;
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(force_loading_base);

//-----------------------------------------------------------------------------
#ifdef GED_DCS_POLICY
static ssize_t dcs_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct gpufreq_core_mask_info *avail_mask_table;
	int avail_mask_num = 0;
	int dcs_enable = 0;
	int mode = 0;
	int pos = 0;
	int i = 0;

	dcs_enable = is_dcs_enable();

	avail_mask_table = dcs_get_avail_mask_table();
	avail_mask_num = dcs_get_avail_mask_num();

	if (dcs_enable) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"DCS Policy is enable\n");
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Current in use core num: %d\n", dcs_get_cur_core_num());
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Available max core num: %d\n",	dcs_get_max_core_num());
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"DCS stress enable: %d (0:disable 1:enable 2:enable with log)\n",
					dcs_get_dcs_stress());
	} else {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"DCS Policy is disabled\n");
	}
	/* User Defined DCS Core num */
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"====================================\n");
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"Enable DCS with user-defined mode with min Available Freq Code:\n");
	for (i = 1; i < avail_mask_num; i++) {
		mode += (1 << (avail_mask_table[i].num));
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[%d] Achieve %d/%d Freq --> %d\n", i, avail_mask_table[i].num,
				avail_mask_table[0].num, mode);
	}
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"====================================\n");
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"Please echo [the min Code you want] for enable\n");
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"(Echo 0 for disable)\n");

	return pos;
}

static ssize_t dcs_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;
	int mode = 0, i = 0;
	unsigned int ud_mask_bit = 0;
	struct gpufreq_core_mask_info *avail_mask_table;
	int avail_mask_num = 0;

	avail_mask_table = dcs_get_avail_mask_table();
	avail_mask_num = dcs_get_avail_mask_num();

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value <= 0) {
					dcs_enable(0);
					return count;
				}
				ud_mask_bit = (i32Value >> 1);
				for (i = 1; i < avail_mask_num; i++) {
					mode += (1 << (avail_mask_table[i].num));
					if (i32Value == mode) {
						ged_set_ud_mask_bit(ud_mask_bit);
						break;
					}
				}
				dcs_enable(1);
			}
		}
	}
	return count;
}

static KOBJ_ATTR_RW(dcs_mode);

static ssize_t dcs_stress_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", dcs_get_dcs_stress());
}

static ssize_t dcs_stress_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				if (i32Value <= 0)
					dcs_set_dcs_stress(0);
				else
					dcs_set_dcs_stress(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dcs_stress);

static ssize_t dcs_adjust_support_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", dcs_get_adjust_support());
}

static ssize_t dcs_adjust_support_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				if (i32Value <= 0)
					dcs_set_adjust_support(0);
				else
					dcs_set_adjust_support(i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(dcs_adjust_support);

static ssize_t dcs_adjust_ratio_th_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", dcs_get_adjust_ratio_th());
}

static ssize_t dcs_adjust_ratio_th_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				if (i32Value <= 0)
					dcs_set_adjust_ratio_th(0);
				else
					dcs_set_adjust_ratio_th(i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(dcs_adjust_ratio_th);

static ssize_t dcs_adjust_fr_cnt_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", dcs_get_adjust_fr_cnt());
}

static ssize_t dcs_adjust_fr_cnt_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				if (i32Value <= 0)
					dcs_set_adjust_fr_cnt(0);
				else
					dcs_set_adjust_fr_cnt(i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(dcs_adjust_fr_cnt);

static ssize_t dcs_adjust_non_dcs_th_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", dcs_get_adjust_non_dcs_th());
}

static ssize_t dcs_adjust_non_dcs_th_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				if (i32Value <= 0)
					dcs_set_adjust_non_dcs_th(0);
				else
					dcs_set_adjust_non_dcs_th(i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(dcs_adjust_non_dcs_th);



#endif /* GED_DCS_POLICY */
//-----------------------------------------------------------------------------

#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
static ssize_t fw_idle_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int ui32FwIdle = 0;
	int fw_idle_enable = 0;
	int pos = 0;

	ui32FwIdle = ged_kpi_get_fw_idle_mode();
	fw_idle_enable = ged_kpi_is_fw_idle_policy_enable();

	if (fw_idle_enable > 0) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"FW idle policy is enable\n");
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Policy mode : %d\n", ui32FwIdle);
	} else {
		pos = scnprintf(buf + pos, PAGE_SIZE - pos,
					"FW idle policy is disabled\n");
	}

	return pos;
}

static ssize_t fw_idle_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value;
	int mode = 0;
	int enable = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				switch (i32Value) {
				case 0xF0:
					ged_kpi_enable_fw_idle_policy(1);
					break;
				case 0xFF:
					ged_kpi_enable_fw_idle_policy(0);
					break;
				default:
					if (ged_kpi_is_fw_idle_policy_enable())
						ged_kpi_set_fw_idle_mode(i32Value);
				}
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(fw_idle);
#endif /* MTK_GPU_FW_IDLE */

#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
static ssize_t apo_threshold_us_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned long long apo_threshold_us = 0;
	int pos = 0;

	apo_threshold_us = ged_get_apo_threshold_us();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO-Threshold]: %llu us\n", apo_threshold_us);

	return pos;
}

static ssize_t apo_threshold_us_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_threshold_us((unsigned long long)i32Value);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_threshold_us);

static ssize_t apo_wakeup_us_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned long long apo_wakeup_us = 0;
	int pos = 0;

	apo_wakeup_us = ged_get_apo_wakeup_us();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO-Wakeup]: %llu us\n", apo_wakeup_us);

	return pos;
}

static ssize_t apo_wakeup_us_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_wakeup_us((unsigned long long)i32Value);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_wakeup_us);

static ssize_t apo_lp_threshold_us_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned long long apo_lp_threshold_us = 0;
	int pos = 0;

	apo_lp_threshold_us = ged_get_apo_lp_threshold_us();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO-LP-Threshold]: %llu us\n", apo_lp_threshold_us);

	return pos;
}

static ssize_t apo_lp_threshold_us_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_lp_threshold_us((unsigned long long)i32Value);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_lp_threshold_us);

static ssize_t apo_force_hint_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int apo_force_hint = -1;
	int pos = 0;

	apo_force_hint = ged_get_apo_force_hint();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO-Force-Hint]: %d\n", apo_force_hint);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[Help]:\n0: APO_NORMAL_HINT, 1: APO_LP_HINT, Others: No Operation (APO_INVALID_HINT)\n");

	return pos;
}

static ssize_t apo_force_hint_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_force_hint((int)i32Value);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_force_hint);

static ssize_t apo_status_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	bool bGPUAPO;
	bool bGPUPredictAPO;
	long long ns_gpu_off_duration;
	long long ns_gpu_predict_off_duration;
	int apo_hint;
	int pos = 0;

	bGPUAPO = ged_gpu_apo_notify();
	bGPUPredictAPO = ged_gpu_predict_apo_notify();
	ns_gpu_off_duration = ged_get_power_duration();
	ns_gpu_predict_off_duration = ged_get_predict_power_duration();
	apo_hint = ged_get_apo_hint();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO]: %d, [Predict-APO]: %d\n", bGPUAPO, bGPUPredictAPO);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[Dur]: %lld, [Predict-Dur]: %lld\n",
				ns_gpu_off_duration, ns_gpu_predict_off_duration);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO-Hint]: %d\n",
				apo_hint);

	return pos;
}
static KOBJ_ATTR_RO(apo_status);
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */

//-----------------------------------------------------------------------------

unsigned int g_loading_stride_size = GED_DEFAULT_SLIDE_STRIDE_SIZE;
unsigned int g_loading_target_mode;

static ssize_t loading_stride_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
		g_loading_target_mode * 100 + g_loading_stride_size);
}

static ssize_t loading_stride_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 200 && i32Value > 0) {
					if (i32Value < 100) {
						g_loading_target_mode = 0;
						g_loading_stride_size = i32Value;
					} else if (i32Value < 200 && i32Value > 100) {
						g_loading_target_mode = 1;
						g_loading_stride_size = i32Value % 100;
					}
				} else {
					g_loading_target_mode = 0;
					g_loading_stride_size = GED_DEFAULT_SLIDE_STRIDE_SIZE;
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(loading_stride_size);

//-----------------------------------------------------------------------------

unsigned int g_frame_target_mode = GED_DEFAULT_FRAME_TARGET_MODE;
unsigned int g_frame_target_time = GED_DEFAULT_FRAME_TARGET_TIME;

static ssize_t fallback_timing_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", g_frame_target_mode * 100 + g_frame_target_time);
}

static ssize_t fallback_timing_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 300 && i32Value >= 0) {
					if (i32Value < 100) {
						g_frame_target_mode = 0;
						g_frame_target_time = i32Value;
					} else if (i32Value < 200 && i32Value > 100) {
						g_frame_target_mode = 1;
						g_frame_target_time =  i32Value % 100;
					} else if (i32Value < 300 && i32Value > 200) {
						g_frame_target_mode = 2;
						g_frame_target_time =  i32Value % 100;
					}
				} else {
					g_frame_target_mode = GED_DEFAULT_FRAME_TARGET_MODE;
					g_frame_target_time = GED_DEFAULT_FRAME_TARGET_TIME;
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fallback_timing);

//-----------------------------------------------------------------------------

unsigned int g_fallback_mode = GED_DEFAULT_FALLBACK_MODE;
unsigned int g_fallback_time = GED_DEFAULT_FALLBACK_TIME;

static ssize_t fallback_interval_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", g_fallback_mode * 100 + g_fallback_time);
}

static ssize_t fallback_interval_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value >= 0 && i32Value < 400) {
					if (i32Value < 100) {
						g_fallback_mode = 0;
						g_fallback_time = i32Value;
					}
					if (i32Value > 100 && i32Value < 200) {
						g_fallback_mode = 1;
						g_fallback_time = i32Value%100;
					}
					if (i32Value > 200 && i32Value < 300) {
						g_fallback_mode = 2;
						g_fallback_time = i32Value%100;
					}
				} else {
					g_fallback_mode = GED_DEFAULT_FALLBACK_MODE;
					g_fallback_time = GED_DEFAULT_FALLBACK_TIME;
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fallback_interval);

//-----------------------------------------------------------------------------
unsigned int g_fallback_window_size = GED_DEFAULT_FALLBACK_WINDOW_SIZE;

static ssize_t fallback_window_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", g_fallback_window_size);
}

static ssize_t fallback_window_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 65)
					g_fallback_window_size = i32Value;
				else
					g_fallback_window_size = GED_DEFAULT_FALLBACK_WINDOW_SIZE;
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fallback_window_size);

//-----------------------------------------------------------------------------
unsigned int g_fallback_frequency_adjust = 1;

static ssize_t fallback_frequency_adjust_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", g_fallback_frequency_adjust);
}

static ssize_t fallback_frequency_adjust_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
				if (u32Value <= 1)
					g_fallback_frequency_adjust = u32Value;

				/* Temporary solution: if fallback_frequency_adjust enabled,
				 * use previous dvfs_loading_mode policy(LOADING_MAX_ITERMCU)
				 */
				if (g_fallback_frequency_adjust == 0) {
					mtk_dvfs_loading_mode(LOADING_MAX_ITERMCU);
					mtk_dvfs_workload_mode(WORKLOAD_MAX_ITERMCU);
				} else {
					mtk_dvfs_loading_mode(LOADING_ACTIVE);
					mtk_dvfs_workload_mode(WORKLOAD_ACTIVE);
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fallback_frequency_adjust);
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static ssize_t dvfs_async_ratio_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int async_ratio_support, force_stack_opp, force_top_opp;
	unsigned int log_level;
	int pos = 0;
	int length;

	async_ratio_support = ged_dvfs_get_async_ratio_support();
	force_stack_opp = ged_dvfs_get_stack_oppidx();
	force_top_opp = ged_dvfs_get_top_oppidx();
	log_level = ged_dvfs_get_async_log_level();

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"dvfs_async test, force stack opp(%d), force top opp(%d), enable log(%d), enable async(%d)\n",
			force_stack_opp, force_top_opp, log_level, async_ratio_support);

	pos += length;

	return pos;
}

static ssize_t dvfs_async_ratio_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value, FORCE_TOP_OPP;
	unsigned int log_level;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value & (0x1 << 9)) {
					log_level = i32Value & 0xF;
					ged_dvfs_set_async_log_level(log_level);
				} else if (i32Value & (0x1 << 7)) {
					if (i32Value & 0x1)
						ged_dvfs_enable_async_ratio(1);
					else
						ged_dvfs_enable_async_ratio(0);
				} else if (i32Value & (0x1 << 8)) {
					FORCE_TOP_OPP = i32Value & ((0x1 << 6) - 1);
					ged_dvfs_force_top_oppidx(FORCE_TOP_OPP);
				} else {
					if (i32Value & (0x1 << 6)) {
						ged_dvfs_force_top_oppidx(0);
						ged_dvfs_force_stack_oppidx(0);
					} else {
						ged_dvfs_force_stack_oppidx(i32Value);
					}
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_async_ratio);
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static ssize_t ged_log_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"GED log level: %d\n", g_default_log_level);

	pos += length;

	return pos;
}

static ssize_t ged_log_level_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value >= 0)
					g_default_log_level = i32Value;
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(ged_log_level);
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static ssize_t ged_fallback_tuning_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"ged_fallback_tuning: %d\n", ged_dvfs_get_fallback_tuning());

	pos += length;

	return pos;
}

static ssize_t ged_fallback_tuning_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value >= 0)
					ged_dvfs_set_fallback_tuning(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(ged_fallback_tuning);
//-----------------------------------------------------------------------------
unsigned int g_loading_slide_window_size = GED_DEFAULT_SLIDE_WINDOW_SIZE;

static ssize_t loading_window_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", g_loading_slide_window_size);
}

static ssize_t loading_window_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 256)
					ged_dvfs_set_slide_window_size(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(loading_window_size);

//-----------------------------------------------------------------------------
#if defined(MTK_GPU_SLC_POLICY)
static ssize_t gpu_slc_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	struct gpu_slc_stat *slc_stat = NULL;

	slc_stat = get_gpu_slc_stat();

	if (slc_stat != NULL) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"mode:			%d\n", slc_stat->mode);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(1)Common Hitrate:	%d\n", slc_stat->policy_0_hit_rate_r);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(2)RT Hitrate:		%d\n", slc_stat->policy_1_hit_rate_r);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(4)Common_LSC Hitrate:	%d\n", slc_stat->policy_2_hit_rate_r);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(8)Common_TEX Hitrate:	%d\n", slc_stat->policy_3_hit_rate_r);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Final Policy:		%d\n", slc_stat->policy);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Final HitRate:		%d\n", slc_stat->hit_rate_r);
		if(slc_stat->isoverflow == 1)
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(bw overflow)\n", slc_stat->hit_rate_r);
	} else {
		pos = scnprintf(buf + pos, PAGE_SIZE - pos,
					"GPU SLC sysFS not supports\n");
	}

	return pos;
}

static ssize_t gpu_slc_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_gpu_slc_dynamic_mode(i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(gpu_slc_policy);
#endif /* MTK_GPU_SLC_POLICY */

//-----------------------------------------------------------------------------

GED_ERROR ged_hal_init(void)
{
	GED_ERROR err = GED_OK;

	err = ged_debugFS_create_entry_dir("hal", NULL, &gpsHALDir);

	if (unlikely(err != GED_OK)) {
		err = GED_ERROR_FAIL;
		GED_LOGE("ged: failed to create hal dir!\n");
		goto ERROR;
	}

	/* Report Opp Cost */
	err = ged_debugFS_create_entry("opp_logs", gpsHALDir,
			&gsDvfsOppCostsReadOps, ged_dvfs_opp_cost_write_entry, NULL,
			&gpsOppCostsEntry);

	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to create opp_logs entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_dir(NULL, "hal", &hal_kobj);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create hal dir!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_total_gpu_freq_level_count);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create total_gpu_freq_level_count entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_custom_boost_gpu_freq);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create custom_boost_gpu_freq entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_custom_upbound_gpu_freq);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create custom_upbound_gpu_freq entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_current_freqency);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create current_freqency entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_previous_freqency);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create previous_freqency entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_utilization);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_utilization entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_boost_level);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_boost_level entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_opp_logs);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to create opp_logs entry!\n");
		goto ERROR;
	}

#ifdef MTK_GED_KPI
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_kpi);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create ged_kpi entry!\n");
		goto ERROR;
	}
#endif

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_margin_value);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_margin_value entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_loading_base_dvfs_step);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create loading_base_dvfs_step entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_timer_base_dvfs_margin);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create timer_base_dvfs_margin entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_loading_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_loading_mode entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_workload_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_workload_mode entry!\n");
		goto ERROR;
	}

	ged_fb_notifier.notifier_call = ged_fb_notifier_callback;
	if (fb_register_client(&ged_fb_notifier))
		GED_LOGE("Register fb_notifier fail!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_reduce_mips_dvfs);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create reduce_mips_dvfs entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_force_loading_base);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create force_loading_base entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_early_force_fallback_policy);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create early_force_fallback_policy entry!\n");

#ifdef GED_DCS_POLICY
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dcs_mode entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_stress);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create dcs_stress entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_adjust_support);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create dcs_adjust_support entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_adjust_ratio_th);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create dcs_adjust_ratio_th entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_adjust_fr_cnt);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create dcs_adjust_fr_cnt entry!\n");
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_adjust_non_dcs_th);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create dcs_adjust_non_dcs_th entry!\n");

#endif /* GED_DCS_POLICY */

#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fw_idle);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create fw_idle entry!\n");
		goto ERROR;
	}
#endif /* MTK_GPU_FW_IDLE */

#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_threshold_us);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_threshold_us entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_wakeup_us);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_wakeup_us entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_lp_threshold_us);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_lp_threshold_us entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_force_hint);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_force_hint entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_status);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_status entry!\n");
		goto ERROR;
	}
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_loading_window_size);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create loading_window_size entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_loading_stride_size);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create loading_stride_size entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fallback_timing);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create fallback_timing entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fallback_interval);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create fallback_interval entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fallback_window_size);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create fallback_window_size entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fallback_frequency_adjust);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create fallback_frequency_adjust entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_async_ratio);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create dvfs_async_ratio entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_log_level);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create ged_log_level entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_fallback_tuning);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create ged_fallback_tuning entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_frame_base_optimize);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create frame_base_optimize entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_pre_fence_chk);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create pre_fence_chk entry!\n");
		goto ERROR;
	}

#if defined(MTK_GPU_SLC_POLICY)
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_slc_policy);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_slc_policy entry!\n");
		goto ERROR;
	}
#endif /* MTK_GPU_SLC_POLICY */

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_sum_loading);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_sum_loading entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_power_state);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_power_state entry!\n");
		goto ERROR;
	}

	return err;

ERROR:

	ged_hal_exit();

	return err;
}
//-----------------------------------------------------------------------------
void ged_hal_exit(void)
{
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_reduce_mips_dvfs);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_loading_mode);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_workload_mode);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_timer_base_dvfs_margin);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_loading_base_dvfs_step);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_margin_value);

#ifdef MTK_GED_KPI
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_kpi);
#endif
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_opp_logs);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_loading_window_size);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_loading_stride_size);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_boost_level);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_utilization);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_previous_freqency);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_current_freqency);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_custom_upbound_gpu_freq);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_custom_boost_gpu_freq);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_total_gpu_freq_level_count);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fallback_timing);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fallback_interval);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fallback_window_size);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fallback_frequency_adjust);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_async_ratio);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_log_level);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_force_loading_base);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_early_force_fallback_policy);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_fallback_tuning);
#ifdef GED_DCS_POLICY
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_mode);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_stress);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_adjust_support);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_adjust_ratio_th);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_adjust_fr_cnt);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_adjust_non_dcs_th);
#endif
#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fw_idle);
#endif /* MTK_GPU_FW_IDLE */
#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_threshold_us);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_wakeup_us);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_lp_threshold_us);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_force_hint);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_status);
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */
#if defined(MTK_GPU_SLC_POLICY)
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_slc_policy);
#endif /* MTK_GPU_SLC_POLICY */

	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_sum_loading);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_power_state);
	ged_sysfs_remove_dir(&hal_kobj);
}
//-----------------------------------------------------------------------------
