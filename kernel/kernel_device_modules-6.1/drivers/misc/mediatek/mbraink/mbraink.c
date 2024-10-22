// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include <linux/skbuff.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <linux/suspend.h>


#include "mbraink_power.h"
#include "mbraink_video.h"
#include "mbraink.h"
#include "mbraink_process.h"
#include "mbraink_memory.h"
#include "mbraink_gpu.h"
#include "mbraink_audio.h"
#include "mbraink_cpufreq.h"
#include "mbraink_battery.h"
#include "mbraink_pmu.h"


#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
	IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)

#include <lpm_dbg_logger.h>

#endif
static DEFINE_MUTEX(power_lock);
static DEFINE_MUTEX(pmu_lock);
struct mbraink_data mbraink_priv;

static int mbraink_genetlink_recv_msg(struct sk_buff *skb, struct genl_info *info);

static int mbraink_open(struct inode *inode, struct file *filp)
{
	/*pr_info("[MBK_INFO] %s\n", __func__);*/

	return 0;
}

static int mbraink_release(struct inode *inode, struct file *filp)
{
	/*pr_info("[MBK_INFO] %s\n", __func__);*/

	return 0;
}

static long handleMemoryDdrInfo(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_memory_ddrInfo *pMemoryDdrInfo =
		(struct mbraink_memory_ddrInfo *)(mbraink_data);

	memset(pMemoryDdrInfo,
			0,
			sizeof(struct mbraink_memory_ddrInfo));
	ret = mbraink_memory_getDdrInfo(pMemoryDdrInfo);
	if (ret == 0) {
		if (copy_to_user((struct mbraink_memory_ddrInfo *)arg,
				pMemoryDdrInfo,
				sizeof(struct mbraink_memory_ddrInfo))) {
			pr_notice("Copy memory ddr Info to UserSpace error!\n");
			ret = -EPERM;
		}
	}

	return ret;
}

static long handleIdleRatioInfo(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_audio_idleRatioInfo *audioIdleRatioInfo =
		(struct mbraink_audio_idleRatioInfo *)(mbraink_data);

	memset(audioIdleRatioInfo,
		0,
		sizeof(struct mbraink_audio_idleRatioInfo));
	ret = mbraink_audio_getIdleRatioInfo(audioIdleRatioInfo);
	if (ret == 0) {
		if (copy_to_user((struct mbraink_audio_idleRatioInfo *)arg,
				audioIdleRatioInfo,
				sizeof(struct mbraink_audio_idleRatioInfo))) {
			pr_notice("Copy audio idle ratio info from UserSpace Err!\n");
			ret = -EPERM;
		}
	}
	return ret;
}

static long handleVcoreInfo(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_power_vcoreInfo *pPowerVcoreInfo =
		(struct mbraink_power_vcoreInfo *)(mbraink_data);

	memset(pPowerVcoreInfo,
		0,
		sizeof(struct mbraink_power_vcoreInfo));
	ret = mbraink_power_getVcoreInfo(pPowerVcoreInfo);
	if (ret == 0) {
		if (copy_to_user((struct mbraink_power_vcoreInfo *)arg,
				pPowerVcoreInfo,
				sizeof(struct mbraink_power_vcoreInfo))) {
			pr_notice("Copy vcore info from UserSpace Err!\n");
			ret = -EPERM;
		}
	}

	return ret;
}

static long handleFeatureEn(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_feature_en *featureEnInfo =
		(struct mbraink_feature_en *)(mbraink_data);

	memset(featureEnInfo,
		0,
		sizeof(struct mbraink_feature_en));

	if (copy_from_user(featureEnInfo,
			 (char *)arg,
			 sizeof(struct mbraink_feature_en))) {
		pr_notice("Data get feature en from UserSpace Err!\n");
		return -EPERM;
	}

	if (mbraink_priv.feature_en != featureEnInfo->feature_en) {
		pr_notice("mbraink feature enable.\n");
		if ((featureEnInfo->feature_en &
				MBRAINK_FEATURE_GPU_EN)
					== MBRAINK_FEATURE_GPU_EN) {
			pr_notice("mbraink feature enable gpu.\n");
			ret =  mbraink_gpu_init();
			if (ret)
				pr_notice("mbraink gpu init failed.\n");
			else
				mbraink_priv.feature_en |=
					MBRAINK_FEATURE_GPU_EN;
		}

		if ((featureEnInfo->feature_en &
				MBRAINK_FEATURE_AUDIO_EN)
					== MBRAINK_FEATURE_AUDIO_EN) {
			pr_notice("mbraink feature enable audio.\n");
			ret = mbraink_audio_init();
			if (ret)
				pr_notice("mbraink audio init failed.\n");
			else
				mbraink_priv.feature_en |=
					MBRAINK_FEATURE_AUDIO_EN;
		}
		pr_notice("mbraink en set (%d) to (%d)\n",
					featureEnInfo->feature_en,
					mbraink_priv.feature_en);
	} else {
		pr_notice("mbraink feature enabled before.\n");
	}

	return ret;
}

static long handlePmuEn(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_pmu_en *pmuEnInfo =
		(struct mbraink_pmu_en *)(mbraink_data);

	memset(pmuEnInfo,
		0,
		sizeof(struct mbraink_pmu_en));

	if (copy_from_user(pmuEnInfo,
			 (char *)arg,
			 sizeof(struct mbraink_pmu_en))) {
		pr_notice("Data get pmu en from UserSpace Err!\n");
		return -EPERM;
	}

	mutex_lock(&pmu_lock);
	if (mbraink_priv.pmu_en != pmuEnInfo->pmu_en) {
		pr_notice("mbraink pmu_en enable.\n");
		if ((pmuEnInfo->pmu_en & MBRAINK_PMU_INST_SPEC_EN) == MBRAINK_PMU_INST_SPEC_EN) {
			pr_notice("mbraink feature enable pmu inst spec.\n");
			ret = mbraink_enable_pmu_inst_spec(true);
			if (ret)
				pr_notice("mbraink pmu inst spec enabled failed.\n");
			else
				mbraink_priv.pmu_en |= MBRAINK_PMU_INST_SPEC_EN;
		} else {
			pr_notice("mbraink feature disable pmu inst spec.\n");
			ret = mbraink_enable_pmu_inst_spec(false);
			if (ret)
				pr_notice("mbraink pmu inst spec enabled failed.\n");
			else
				mbraink_priv.pmu_en ^= MBRAINK_PMU_INST_SPEC_EN;
		}

		pr_notice("mbraink en set (%d) to (%d)\n",
					pmuEnInfo->pmu_en,
					mbraink_priv.pmu_en);
	} else {
		pr_notice("mbraink pmu_en enabled before.\n");
	}
	mutex_unlock(&pmu_lock);

	return ret;
}

static long handlePmuInfo(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_pmu_info *pmuInfo =
		(struct mbraink_pmu_info *)(mbraink_data);

	pr_notice("mbraink %s\n", __func__);
	memset(pmuInfo,
		0,
		sizeof(struct mbraink_pmu_info));

	mutex_lock(&pmu_lock);
	ret = mbraink_get_pmu_inst_spec(pmuInfo);
	if (ret == 0) {
		if (copy_to_user((struct mbraink_pmu_info *)arg,
				pmuInfo,
				sizeof(struct mbraink_pmu_info))) {
			pr_notice("Copy pmu Info to UserSpace error!\n");
			ret = -EPERM;
		}
	}
	mutex_unlock(&pmu_lock);

	return ret;
}

static long handleMdvInfo(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_memory_mdvInfo *memory_mdv_info =
		(struct mbraink_memory_mdvInfo *)(mbraink_data);

	memset(memory_mdv_info,
		0,
		sizeof(struct mbraink_memory_mdvInfo));

	if (copy_from_user(memory_mdv_info,
			(struct mbraink_memory_mdvInfo *) arg,
			sizeof(struct mbraink_memory_mdvInfo))) {
		pr_notice("Data write memory mdv info from UserSpace Err!\n");
		return -EPERM;
	}
	ret = mbraink_memory_getMdvInfo(memory_mdv_info);
	if (ret == 0) {
		if (copy_to_user((struct mbraink_memory_mdvInfo *) arg,
				memory_mdv_info,
				sizeof(struct mbraink_memory_mdvInfo))) {
			pr_notice("Copy memory_mdv_info to UserSpace error!\n");
			return -EPERM;
		}
	}
	return ret;
}

static long handle_spmpower_info(unsigned long arg)
{
	int n = 0;
	long ret = 0;

	n = mbraink_get_power_info(mbraink_priv.power_buffer, MAX_BUF_SZ, CURRENT_DATA);
	if (n <= 0) {
		pr_notice("mbraink_get_power_info return failed, err %d\n", n);
	} else {
		mutex_lock(&power_lock);

		if (mbraink_priv.suspend_power_info_en[0] == '1') {
			if (mbraink_priv.suspend_power_buffer[0] != '\0'
				&& mbraink_priv.suspend_power_data_size != 0) {
				memcpy(mbraink_priv.power_buffer+n,
					mbraink_priv.suspend_power_buffer,
					mbraink_priv.suspend_power_data_size+1);
				n = n + mbraink_priv.suspend_power_data_size;
			}
			mbraink_priv.suspend_power_buffer[0] = '\0';
			mbraink_priv.suspend_power_data_size = 0;

			if (mbraink_priv.resume_power_buffer[0] != '\0'
				 && mbraink_priv.resume_power_data_size != 0) {
				memcpy(mbraink_priv.power_buffer+n,
					mbraink_priv.resume_power_buffer,
					mbraink_priv.resume_power_data_size+1);
				n = n + mbraink_priv.resume_power_data_size;
			}
			mbraink_priv.resume_power_buffer[0] = '\0';
			mbraink_priv.resume_power_data_size = 0;
		}

		if (copy_to_user((char *)arg, mbraink_priv.power_buffer, n+1)) {
			pr_notice("Copy Power_info to UserSpace error!\n");
			mutex_unlock(&power_lock);
			return -EPERM;
		}
		mutex_unlock(&power_lock);
	}
	return ret;
}

static long handle_video_info(unsigned long arg, void *mbraink_data)
{
	char *buffer = (char *)(mbraink_data);
	int n = 0;
	long ret = 0;

	n = mbraink_get_video_info(buffer);
	if (n <= 0) {
		pr_notice("mbraink_get_video_info return failed, err %d\n", n);
	} else if (copy_to_user((char *)arg, buffer, n+1)) {
		pr_notice("Copy Video_info to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_supend_power_en(unsigned long arg)
{
	long ret = 0;

	mutex_lock(&power_lock);
	if (copy_from_user(mbraink_priv.suspend_power_info_en,
			(char *)arg,
			sizeof(mbraink_priv.suspend_power_info_en))) {
		pr_notice("Data write suspend_power_en from UserSpace Err!\n");
		mutex_unlock(&power_lock);
		return -EPERM;
	}
	pr_info("[MBK_INFO] mbraink_priv.suspend_power_info_en = %c\n",
		mbraink_priv.suspend_power_info_en[0]);
	mutex_unlock(&power_lock);

	return ret;
}

static long handle_process_stat(unsigned long arg, void *mbraink_data)
{
	struct mbraink_process_stat_data *process_stat_buffer =
		(struct mbraink_process_stat_data *)(mbraink_data);
	long ret = 0;

	pid_t pid = 1;

	if (copy_from_user(process_stat_buffer,
			(struct mbraink_process_stat_data *)arg,
			sizeof(struct mbraink_process_stat_data))) {
		pr_notice("copy process info from user Err!\n");
		return -EPERM;
	}

	if (process_stat_buffer->pid > PID_MAX_DEFAULT) {
		pr_notice("process state: Invalid pid %u\n",
			process_stat_buffer->pid);
		return -EINVAL;
	}
	pid = process_stat_buffer->pid;

	mbraink_get_process_stat_info(pid, process_stat_buffer);

	if (copy_to_user((struct mbraink_process_stat_data *)arg,
			process_stat_buffer,
			sizeof(struct mbraink_process_stat_data))) {
		pr_notice("Copy process_info to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_process_memory(unsigned long arg, void *mbraink_data)
{
	struct mbraink_process_memory_data *process_memory_buffer =
		(struct mbraink_process_memory_data *)(mbraink_data);
	long ret = 0;
	pid_t pid = 1;
	unsigned int current_cnt = 0;

	if (copy_from_user(process_memory_buffer,
			(struct mbraink_process_memory_data *)arg,
			sizeof(struct mbraink_process_memory_data))) {
		pr_notice("copy process memory info from user Err!\n");
		return -EPERM;
	}

	if (process_memory_buffer->pid > PID_MAX_DEFAULT ||
		process_memory_buffer->pid_count > PID_MAX_DEFAULT) {
		pr_notice("process memory: Invalid pid_idx %u or pid_count %u\n",
			process_memory_buffer->pid, process_memory_buffer->pid_count);
		return -EINVAL;
	}
	pid = process_memory_buffer->pid;
	current_cnt = process_memory_buffer->current_cnt;

	mbraink_get_process_memory_info(pid, current_cnt, process_memory_buffer);

	if (copy_to_user((struct mbraink_process_memory_data *)arg,
			process_memory_buffer,
			sizeof(struct mbraink_process_memory_data))) {
		pr_notice("Copy process_memory_info to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_process_monitor_list(unsigned long arg, void *mbraink_data)
{
	struct mbraink_monitor_processlist *monitor_processlist_buffer =
		(struct mbraink_monitor_processlist *)(mbraink_data);
	unsigned short monitor_process_count = 0;
	long ret = 0;

	if (copy_from_user(monitor_processlist_buffer,
			(struct mbraink_monitor_processlist *)arg,
			sizeof(struct mbraink_monitor_processlist))) {
		pr_notice("copy mbraink_monitor_processlist from user Err!\n");
		return -EPERM;
	}

	if (monitor_processlist_buffer->monitor_process_count > MAX_MONITOR_PROCESS_NUM) {
		pr_notice("Invalid monitor_process_count!\n");
		monitor_processlist_buffer->monitor_process_count =
								MAX_MONITOR_PROCESS_NUM;
	}

	monitor_process_count =
		monitor_processlist_buffer->monitor_process_count;

	mbraink_processname_to_pid(monitor_process_count,
				monitor_processlist_buffer, 0);
	return ret;
}

static long handle_thread_stat(unsigned long arg, void *mbraink_data)
{
	struct mbraink_thread_stat_data *thread_stat_buffer =
		(struct mbraink_thread_stat_data *)(mbraink_data);
	long ret = 0;
	pid_t pid_idx = 0, tid = 0;

	if (copy_from_user(thread_stat_buffer,
			(struct mbraink_thread_stat_data *)arg,
			sizeof(struct mbraink_thread_stat_data))) {
		pr_notice("copy thread_stat_info data from user Err!\n");
		return -EPERM;
	}

	if (thread_stat_buffer->pid_idx > PID_MAX_DEFAULT ||
			thread_stat_buffer->tid > PID_MAX_DEFAULT) {
		pr_notice("Invalid pid_idx %u or tid %u!\n",
			thread_stat_buffer->pid_idx, thread_stat_buffer->tid);
		return -EINVAL;
	}
	pid_idx = thread_stat_buffer->pid_idx;
	tid = thread_stat_buffer->tid;

	mbraink_get_thread_stat_info(pid_idx, tid, thread_stat_buffer);

	if (copy_to_user((struct mbraink_thread_stat_data *)arg,
			thread_stat_buffer,
			sizeof(struct mbraink_thread_stat_data))) {
		pr_notice("Copy thread_stat_info to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_trace_process(unsigned long arg, void *mbraink_data)
{
	struct mbraink_tracing_pid_data *tracing_pid_buffer =
		(struct mbraink_tracing_pid_data *)(mbraink_data);
	long ret = 0;
	unsigned short tracing_idx = 0;

	if (copy_from_user(tracing_pid_buffer,
			(struct mbraink_tracing_pid_data *)arg,
			sizeof(struct mbraink_tracing_pid_data))) {
		pr_notice("copy tracing_pid_buffer data from user Err!\n");
		return -EPERM;
	}

	if (tracing_pid_buffer->tracing_idx > MAX_TRACE_NUM) {
		pr_notice("invalid tracing_idx %u !\n", tracing_pid_buffer->tracing_idx);
		return -EINVAL;
	}
	tracing_idx = tracing_pid_buffer->tracing_idx;

	mbraink_get_tracing_pid_info(tracing_idx, tracing_pid_buffer);

	if (copy_to_user((struct mbraink_tracing_pid_data *)arg,
			tracing_pid_buffer,
			sizeof(struct mbraink_tracing_pid_data))) {
		pr_notice("Copy tracing_pid_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_cpufreq_notify(unsigned long arg, void *mbraink_data)
{
	struct mbraink_cpufreq_notify_struct_data *pcpufreq_notify_buffer =
		(struct mbraink_cpufreq_notify_struct_data *)(mbraink_data);
	unsigned short notify_cluster_idx = 0;
	unsigned short notify_idx = 0;
	long ret = 0;

	if (copy_from_user(pcpufreq_notify_buffer,
			(struct mbraink_cpufreq_notify_struct_data *)arg,
			sizeof(struct mbraink_cpufreq_notify_struct_data))) {
		pr_notice("Copy cpufreq_notify_buffer from user Err!\n");
		return -EPERM;
	}

	if (pcpufreq_notify_buffer->notify_cluster_idx > CPU_CLUSTER_SZ ||
		pcpufreq_notify_buffer->notify_idx > CPUFREQ_NOTIFY_SZ) {
		pr_notice("invalid notify_cluster_idx %u or notify_idx %u !\n",
			pcpufreq_notify_buffer->notify_cluster_idx,
			pcpufreq_notify_buffer->notify_idx);
		return -EINVAL;
	}
	notify_cluster_idx = pcpufreq_notify_buffer->notify_cluster_idx;
	notify_idx = pcpufreq_notify_buffer->notify_idx;
	mbraink_get_cpufreq_notifier_info(notify_cluster_idx,
					notify_idx,
					pcpufreq_notify_buffer);
	if (copy_to_user((struct mbraink_cpufreq_notify_struct_data *)arg,
			pcpufreq_notify_buffer,
			sizeof(struct mbraink_cpufreq_notify_struct_data))) {
		pr_notice("Copy cpufreq_notify_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

unsigned long handle_battery_info(unsigned long arg, void *mbraink_data)
{
	struct mbraink_battery_data *battery_buffer =
		(struct mbraink_battery_data *)(mbraink_data);
	long ret = 0;

	memset(battery_buffer,
		0,
		sizeof(struct mbraink_battery_data));
	mbraink_get_battery_info(battery_buffer, 0);
	if (copy_to_user((struct mbraink_battery_data *) arg,
			battery_buffer,
			sizeof(struct mbraink_battery_data))) {
		pr_notice("Copy battery_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_wakeup_info(unsigned long arg, void *mbraink_data)
{
	struct mbraink_power_wakeup_data *power_wakeup_data =
		(struct mbraink_power_wakeup_data *)(mbraink_data);
	long ret = 0;

	if (copy_from_user(power_wakeup_data,
			(struct mbraink_power_wakeup_data *) arg,
			sizeof(struct mbraink_power_wakeup_data))) {
		pr_notice("Data write power_wakeup_data from UserSpace Err!\n");
		return -EPERM;
	}
	mbraink_get_power_wakeup_info(power_wakeup_data);
	if (copy_to_user((struct mbraink_power_wakeup_data *) arg,
			power_wakeup_data,
			sizeof(struct mbraink_power_wakeup_data))) {
		pr_notice("Copy power_wakeup_data to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_power_spm_raw(unsigned long arg, void *mbraink_data)
{
	struct mbraink_power_spm_raw *power_spm_buffer =
		(struct mbraink_power_spm_raw *)(mbraink_data);
	long ret = 0;

	if (copy_from_user(power_spm_buffer,
			(struct mbraink_power_spm_raw *) arg,
			sizeof(struct mbraink_power_spm_raw))) {
		pr_notice("Data write power_spm_buffer from UserSpace Err!\n");
		return -EPERM;
	}
	mbraink_power_get_spm_info(power_spm_buffer);
	if (copy_to_user((struct mbraink_power_spm_raw *) arg,
			power_spm_buffer,
			sizeof(struct mbraink_power_spm_raw))) {
		pr_notice("Copy power_spm_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_modem_info(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_modem_raw *modem_buffer =
		(struct mbraink_modem_raw *)(mbraink_data);

	if (copy_from_user(modem_buffer,
			(struct mbraink_modem_raw *) arg,
			sizeof(struct mbraink_modem_raw))) {
		pr_notice("Data write modem_buffer from UserSpace Err!\n");
		return -EPERM;
	}
	mbraink_power_get_modem_info(modem_buffer);

	if (copy_to_user((struct mbraink_modem_raw *) arg,
			modem_buffer,
			sizeof(struct mbraink_modem_raw))) {
		pr_notice("Copy modem_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_monitor_binder_process(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	struct mbraink_monitor_processlist *monitor_binder_processlist_buffer =
		(struct mbraink_monitor_processlist *)(mbraink_data);
	unsigned short monitor_binder_process_count = 0;

	if (copy_from_user(monitor_binder_processlist_buffer,
			(struct mbraink_monitor_processlist *) arg,
			sizeof(struct mbraink_monitor_processlist))) {
		pr_notice("copy monitor_binder_processlist from user Err!\n");
		return -EPERM;
	}

	monitor_binder_process_count =
		monitor_binder_processlist_buffer->monitor_process_count;
	if (monitor_binder_process_count > MAX_MONITOR_PROCESS_NUM) {
		pr_notice("Invalid monitor_binder_process_count!\n");
		monitor_binder_process_count = MAX_MONITOR_PROCESS_NUM;
		monitor_binder_processlist_buffer->monitor_process_count =
							MAX_MONITOR_PROCESS_NUM;
	}

	mbraink_processname_to_pid(monitor_binder_process_count,
				monitor_binder_processlist_buffer, 1);
	return ret;
}

static long handle_trace_binder(unsigned long arg, void *mbraink_data)
{
	struct mbraink_binder_trace_data *binder_trace_buffer =
		(struct mbraink_binder_trace_data *)(mbraink_data);
	unsigned short tracing_idx = 0;
	long ret = 0;

	if (copy_from_user(binder_trace_buffer,
			(struct mbraink_binder_trace_data *) arg,
			sizeof(struct mbraink_binder_trace_data))) {
		pr_notice("copy binder_trace_buffer data from user Err!\n");
		return -EPERM;
	}

	if (binder_trace_buffer->tracing_idx > MAX_BINDER_TRACE_NUM) {
		pr_notice("invalid binder tracing_idx %u !\n",
			binder_trace_buffer->tracing_idx);
		return -EINVAL;
	}

	tracing_idx = binder_trace_buffer->tracing_idx;

	mbraink_get_binder_trace_info(tracing_idx, binder_trace_buffer);

	if (copy_to_user((struct mbraink_binder_trace_data *) arg,
			binder_trace_buffer, sizeof(struct mbraink_binder_trace_data))) {
		pr_notice("%s: Copy binder_trace_buffer to UserSpace error!\n",
			__func__);
		return -EPERM;
	}
	return ret;
}

static long handle_vcore_voting_info(unsigned long arg, void *mbraink_data)
{
	struct mbraink_voting_struct_data *mbraink_vcorefs_src =
		(struct mbraink_voting_struct_data *)(mbraink_data);
	long ret = 0;

	mbraink_power_get_voting_info(mbraink_vcorefs_src);

	if (copy_to_user((struct mbraink_voting_struct_data *) arg,
			mbraink_vcorefs_src, sizeof(struct mbraink_voting_struct_data))) {
		pr_notice("%s: Copy mbraink_vcorefs_src to UserSpace error!\n",
			__func__);
		return -EPERM;
	}
	return ret;
}

static long handle_power_spm_l2_info(unsigned long arg, void *mbraink_data)
{
	struct mbraink_power_spm_l2_info *power_spm_l2_buffer =
		(struct mbraink_power_spm_l2_info *)(mbraink_data);
	long ret = 0;

	if (copy_from_user(power_spm_l2_buffer,
			(struct mbraink_power_spm_l2_info *) arg,
			sizeof(struct mbraink_power_spm_l2_info))) {
		pr_notice("Data write power_spm_l2_buffer from UserSpace Err!\n");
		return -EPERM;
	}
	mbraink_power_get_spm_l2_info(power_spm_l2_buffer);
	if (copy_to_user((struct mbraink_power_spm_l2_info *) arg,
			power_spm_l2_buffer,
			sizeof(struct mbraink_power_spm_l2_info))) {
		pr_notice("Copy power_spm_l2_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_power_scp_info(unsigned long arg, void *mbraink_data)
{
	struct mbraink_power_scp_info *power_scp_buffer =
		(struct mbraink_power_scp_info *)(mbraink_data);
	long ret = 0;

	memset(power_scp_buffer,
		0,
		sizeof(struct mbraink_power_scp_info));
	mbraink_power_get_scp_info(power_scp_buffer);
	if (copy_to_user((struct mbraink_power_scp_info *) arg,
			power_scp_buffer,
			sizeof(struct mbraink_power_scp_info))) {
		pr_notice("Copy power_scp_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_gpu_opp_info(unsigned long arg, void *mbraink_data)
{
	struct mbraink_gpu_opp_info *gpu_opp_info_buffer =
		(struct mbraink_gpu_opp_info *)(mbraink_data);
	long ret = 0;

	memset(gpu_opp_info_buffer,
		0x00,
		sizeof(struct mbraink_gpu_opp_info));
	mbraink_gpu_getOppInfo(gpu_opp_info_buffer);
	if (copy_to_user((struct mbraink_gpu_opp_info *) arg,
			gpu_opp_info_buffer,
			sizeof(struct mbraink_gpu_opp_info))) {
		pr_notice("Copy gpu_opp_info_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_gpu_state_info(unsigned long arg, void *mbraink_data)
{
	struct mbraink_gpu_state_info *gpu_state_info_buffer =
		(struct mbraink_gpu_state_info *)(mbraink_data);
	long ret = 0;

	memset(gpu_state_info_buffer,
		0x00,
		sizeof(struct mbraink_gpu_state_info));
	mbraink_gpu_getStateInfo(gpu_state_info_buffer);
	if (copy_to_user((struct mbraink_gpu_state_info *) arg,
			gpu_state_info_buffer,
			sizeof(struct mbraink_gpu_state_info))) {
		pr_notice("Copy gpu_state_info_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long handle_gpu_loading_info(unsigned long arg, void *mbraink_data)
{
	struct mbraink_gpu_loading_info *gpu_loading_info_buffer =
		(struct mbraink_gpu_loading_info *)(mbraink_data);
	long ret = 0;

	memset(gpu_loading_info_buffer,
		0x00,
		sizeof(struct mbraink_gpu_loading_info));
	mbraink_gpu_getLoadingInfo(gpu_loading_info_buffer);
	if (copy_to_user((struct mbraink_gpu_loading_info *) arg,
			gpu_loading_info_buffer,
			sizeof(struct mbraink_gpu_loading_info))) {
		pr_notice("Copy gpu_loading_info_buffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

static long mbraink_ioctl(struct file *filp,
							unsigned int cmd,
							unsigned long arg)
{
	long ret = 0;
	void *mbraink_data = NULL;

	switch (cmd) {
	case RO_POWER:
	{
		ret = handle_spmpower_info(arg);
		break;
	}
	case RO_VIDEO:
	{
		mbraink_data = kmalloc(128 * sizeof(char), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_video_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case WO_SUSPEND_POWER_EN:
	{
		ret = handle_supend_power_en(arg);
		break;
	}
	case RO_PROCESS_STAT:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_process_stat_data), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_process_stat(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_PROCESS_MEMORY:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_process_memory_data), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_process_memory(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case WO_MONITOR_PROCESS:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_monitor_processlist), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_process_monitor_list(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_THREAD_STAT:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_thread_stat_data), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_thread_stat(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_TRACE_PROCESS:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_tracing_pid_data), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_trace_process(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_CPUFREQ_NOTIFY:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_cpufreq_notify_struct_data),
					GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_cpufreq_notify(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}

	case RO_MEMORY_DDR_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_memory_ddrInfo), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handleMemoryDdrInfo(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}

	case RO_IDLE_RATIO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_audio_idleRatioInfo), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handleIdleRatioInfo(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}

	case RO_VCORE_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_power_vcoreInfo), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handleVcoreInfo(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}

	case RO_BATTERY_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_battery_data), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_battery_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case WO_FEATURE_EN:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_feature_en), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handleFeatureEn(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_WAKEUP_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_power_wakeup_data), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_wakeup_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case WO_PMU_EN:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_pmu_en), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handlePmuEn(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_PMU_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_pmu_info), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handlePmuInfo(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_POWER_SPM_RAW:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_power_spm_raw), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_power_spm_raw(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_MODEM_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_modem_raw), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_modem_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_MEMORY_MDV_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_memory_mdvInfo), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handleMdvInfo(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case WO_MONITOR_BINDER_PROCESS:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_monitor_processlist), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_monitor_binder_process(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_TRACE_BINDER:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_binder_trace_data), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_trace_binder(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_VCORE_VOTE:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_voting_struct_data), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_vcore_voting_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_POWER_SPM_L2_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_power_spm_l2_info), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_power_spm_l2_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_POWER_SCP_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_power_scp_info), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_power_scp_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_GPU_OPP_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_gpu_opp_info), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_gpu_opp_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_GPU_STATE_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_gpu_state_info), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_gpu_state_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	case RO_GPU_LOADING_INFO:
	{
		mbraink_data = kmalloc(sizeof(struct mbraink_gpu_loading_info), GFP_KERNEL);
		if (!mbraink_data)
			goto End;
		ret = handle_gpu_loading_info(arg, mbraink_data);
		kfree(mbraink_data);
		break;
	}
	default:
		pr_notice("illegal ioctl number %u.\n", cmd);
		return -EINVAL;
	}

	return ret;
End:
	pr_info("%s: kmalloc failed\n", __func__);
	return -ENOMEM;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mbraink_compat_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	return mbraink_ioctl(filp, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct file_operations mbraink_fops = {
	.owner		= THIS_MODULE,
	.open		= mbraink_open,
	.release        = mbraink_release,
	.unlocked_ioctl = mbraink_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = mbraink_compat_ioctl,
#endif
};

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int mbraink_prepare(struct device *dev)
{
	struct timespec64 tv = { 0 };
	ktime_t suspend_ktime;

	ktime_get_real_ts64(&tv);
	suspend_ktime = ktime_get();

	mbraink_priv.last_suspend_timestamp =
		(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	mbraink_priv.last_suspend_ktime =
		ktime_to_ms(suspend_ktime);

	mbraink_get_battery_info(&mbraink_priv.suspend_battery_buffer,
				 mbraink_priv.last_suspend_timestamp);

	return 0;
}
static int mbraink_suspend(struct device *dev)
{
	int ret;

	mutex_lock(&power_lock);
	if (mbraink_priv.suspend_power_info_en[0] == '1') {
		mbraink_priv.suspend_power_data_size =
			mbraink_get_power_info(mbraink_priv.suspend_power_buffer,
						MAX_BUF_SZ, SUSPEND_DATA);
		if (mbraink_priv.suspend_power_data_size <= 0) {
			pr_notice("mbraink_get_power_info return failed, err %d\n",
						mbraink_priv.suspend_power_data_size);
		}
	}
	mutex_unlock(&power_lock);

	pr_info("[MBK_INFO] %s\n", __func__);
	ret = pm_generic_suspend(dev);

	return ret;
}

static int mbraink_resume(struct device *dev)
{
	int ret;

	ret = pm_generic_resume(dev);

	pr_info("[MBK_INFO] %s\n", __func__);

	mutex_lock(&power_lock);
	if (mbraink_priv.suspend_power_info_en[0] == '1') {
		mbraink_priv.resume_power_data_size =
			mbraink_get_power_info(mbraink_priv.resume_power_buffer,
						MAX_BUF_SZ, RESUME_DATA);
		if (mbraink_priv.resume_power_data_size <= 0) {
			pr_notice("mbraink_get_power_info return failed, err %d\n",
				mbraink_priv.resume_power_data_size);
		}
	}
	mutex_unlock(&power_lock);

	return ret;
}

static void mbraink_complete(struct device *dev)
{
	struct timespec64 tv = { 0 };
	ktime_t resume_ktime;
	char netlink_buf[MAX_BUF_SZ] = {'\0'};
	int n = 0;
	long long last_resume_ktime = 0;
	struct mbraink_battery_data resume_battery_buffer;

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
	IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)

	struct lpm_logger_mbrain_dbg_ops *logger_mbrain_ops = NULL;
	long long wakeup_event = 0;
#else
	long long wakeup_event = 0;
#endif

	memset(&resume_battery_buffer, 0,
		sizeof(struct mbraink_battery_data));

	ktime_get_real_ts64(&tv);
	resume_ktime = ktime_get();
	mbraink_priv.last_resume_timestamp =
		(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	last_resume_ktime =
		ktime_to_ms(resume_ktime);

	mbraink_get_battery_info(&resume_battery_buffer, mbraink_priv.last_resume_timestamp);

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
		IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)

	logger_mbrain_ops = get_lpm_logger_mbrain_dbg_ops();
	if (logger_mbrain_ops && logger_mbrain_ops->get_last_suspend_wakesrc)
		wakeup_event = (long long)(logger_mbrain_ops->get_last_suspend_wakesrc());
#else
	wakeup_event = 0;
#endif

	n += snprintf(netlink_buf, MAX_BUF_SZ,
			"%s %lld:%lld:%lld:%lld:%lld %d:%d:%d:%d %d:%d:%d:%d",
			NETLINK_EVENT_SYSRESUME,
			mbraink_priv.last_suspend_timestamp,
			mbraink_priv.last_resume_timestamp,
			mbraink_priv.last_suspend_ktime,
			last_resume_ktime,
			wakeup_event,
			mbraink_priv.suspend_battery_buffer.quse,
			mbraink_priv.suspend_battery_buffer.qmaxt,
			mbraink_priv.suspend_battery_buffer.precise_soc,
			mbraink_priv.suspend_battery_buffer.precise_uisoc,
			resume_battery_buffer.quse,
			resume_battery_buffer.qmaxt,
			resume_battery_buffer.precise_soc,
			resume_battery_buffer.precise_uisoc
	);

	mbraink_netlink_send_msg(netlink_buf);

#if !IS_ENABLED(CONFIG_PM)
	mbraink_priv.last_resume_timestamp = 0;
#endif
	mbraink_priv.last_suspend_timestamp = 0;
	mbraink_priv.last_suspend_ktime = 0;
	memset(&mbraink_priv.suspend_battery_buffer, 0,
		sizeof(struct mbraink_battery_data));
}

#if IS_ENABLED(CONFIG_PM)
static void mbraink_notifier_post_suspend(void)
{
	int ret;
	char netlink_buf[MAX_BUF_SZ] = {'\0'};
	long long spm_l1_info[SPM_L1_DATA_NUM];
	int n = 0;

	if (mbraink_priv.last_resume_timestamp == 0)
		return;

	memset(spm_l1_info, 0, sizeof(spm_l1_info));
	ret = mbraink_power_get_spm_l1_info(spm_l1_info, SPM_L1_DATA_NUM);
	if (ret) {
		mbraink_priv.last_resume_timestamp = 0;
		return;
	}

	n += snprintf(netlink_buf, MAX_BUF_SZ,
			"%s %lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld",
			NETLINK_EVENT_SYSNOTIFIER_PS,
			mbraink_priv.last_resume_timestamp,
			spm_l1_info[0],
			spm_l1_info[1],
			spm_l1_info[2],
			spm_l1_info[3],
			spm_l1_info[4],
			spm_l1_info[5],
			spm_l1_info[6],
			spm_l1_info[7],
			spm_l1_info[8],
			spm_l1_info[9],
			spm_l1_info[10],
			spm_l1_info[11],
			spm_l1_info[12],
			spm_l1_info[13]
	);

	mbraink_priv.last_resume_timestamp = 0;
	mbraink_netlink_send_msg(netlink_buf);
}

static int mbraink_sys_res_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		mbraink_notifier_post_suspend();
		return NOTIFY_DONE;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block mbraink_sys_res_pm_notifier_func = {
	.notifier_call = mbraink_sys_res_pm_event,
	.priority = 0,
};

#endif

static const struct dev_pm_ops mbraink_class_dev_pm_ops = {
	.prepare	= mbraink_prepare,
	.suspend	= mbraink_suspend,
	.resume		= mbraink_resume,
	.complete	= mbraink_complete,
};


#define MBRAINK_CLASS_DEV_PM_OPS (&mbraink_class_dev_pm_ops)
#else
#define MBRAINK_CLASS_DEV_PM_OPS NULL
#endif /*end of CONFIG_PM_SLEEP*/

static void class_create_release(struct class *cls)
{
	/*do nothing because the mbraink class is not from malloc*/
}

static struct class mbraink_class = {
	.name		= "mbraink_host",
	.owner		= THIS_MODULE,
	.class_release	= class_create_release,
	.pm		= MBRAINK_CLASS_DEV_PM_OPS,
};

static void device_create_release(struct device *dev)
{
	/*do nothing because the mbraink device is not from malloc*/
}

static struct device mbraink_device = {
	.init_name	= "mbraink",
	.release		= device_create_release,
	.parent		= NULL,
	.driver_data	= NULL,
	.class		= NULL,
	.devt		= 0,
};

static ssize_t mbraink_info_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	mbraink_show_process_info();

	return sprintf(buf, "show the process information...\n");
}

static ssize_t mbraink_info_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf,
								size_t count)
{
	mbraink_netlink_send_msg(buf);

	return count;
}
static DEVICE_ATTR_RW(mbraink_info);

static ssize_t mbraink_gpu_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	ssize_t size = 0;

	size = getTimeoutCouterReport(buf);
	return size;
}

static ssize_t mbraink_gpu_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf,
								size_t count)
{
	unsigned int command;
	unsigned long long value;
	int retSize = 0;

	retSize = sscanf(buf, "%d %llu", &command, &value);
	if (retSize == -1)
		return 0;

	pr_info("%s: Get Command (%d), Value (%llu) size(%d)\n",
			__func__,
			command,
			value,
			retSize);

	if (command == 1)
		mbraink_gpu_setQ2QTimeoutInNS(value);
	if (command == 2)
		mbraink_gpu_setPerfIdxTimeoutInNS(value);
	if (command == 3)
		mbraink_gpu_setPerfIdxLimit(value);
	if (command == 4)
		mbraink_gpu_dumpPerfIdxList();
	return count;
}

static DEVICE_ATTR_RW(mbraink_gpu);


static int mbraink_dev_init(void)
{
	dev_t mbraink_dev_no = 0;
	int attr_ret = 0;

	mbraink_priv.power_buffer[0] = '\0';
	mbraink_priv.suspend_power_buffer[0] = '\0';
	mbraink_priv.suspend_power_data_size = 0;
	mbraink_priv.resume_power_buffer[0] = '\0';
	mbraink_priv.resume_power_data_size = 0;
	mbraink_priv.suspend_power_info_en[0] = '0';
	mbraink_priv.last_suspend_timestamp = 0;
	mbraink_priv.last_suspend_ktime = 0;
	mbraink_priv.feature_en = 0;
	mbraink_priv.pmu_en = 0;
	memset(&mbraink_priv.suspend_battery_buffer, 0,
		sizeof(struct mbraink_battery_data));

	/*Allocating Major number*/
	if ((alloc_chrdev_region(&mbraink_dev_no, 0, 1, CHRDEV_NAME)) < 0) {
		pr_notice("Cannot allocate major number %u\n",
				mbraink_dev_no);
		return -EBADF;
	}
	pr_info("[MBK_INFO] %s: Major = %u Minor = %u\n",
			__func__, MAJOR(mbraink_dev_no),
			MINOR(mbraink_dev_no));

	/*Initialize cdev structure*/
	cdev_init(&mbraink_priv.mbraink_cdev, &mbraink_fops);

	/*Adding character device to the system*/
	if ((cdev_add(&mbraink_priv.mbraink_cdev, mbraink_dev_no, 1)) < 0) {
		pr_notice("Cannot add the device to the system\n");
		goto r_class;
	}

	/*Register mbraink class*/
	if (class_register(&mbraink_class)) {
		pr_notice("Cannot register the mbraink class %s\n",
			mbraink_class.name);
		goto r_class;
	}

	/*add mbraink device into mbraink_class host,
	 *and assign the character device id to mbraink device
	 */

	mbraink_device.devt = mbraink_dev_no;
	mbraink_device.class = &mbraink_class;

	/*Register mbraink device*/
	if (device_register(&mbraink_device)) {
		pr_notice("Cannot register the Device %s\n",
			mbraink_device.init_name);
		goto r_device;
	}
	pr_info("[MBK_INFO] %s: Mbraink device init done.\n", __func__);

	attr_ret = device_create_file(&mbraink_device, &dev_attr_mbraink_info);
	pr_info("[MBK_INFO] %s: device create file mbraink info ret = %d\n", __func__, attr_ret);

	attr_ret = device_create_file(&mbraink_device, &dev_attr_mbraink_gpu);
	pr_info("[MBK_INFO] %s: device create file mbraink gpu ret = %d\n", __func__, attr_ret);

    #if IS_ENABLED(CONFIG_PM)
	attr_ret = register_pm_notifier(&mbraink_sys_res_pm_notifier_func);
	if (attr_ret)
		pr_info("[MBK_INFO] %s: register_pm_notifier fail ret = %d\n", __func__, attr_ret);

    #endif

	return 0;

r_device:
	class_unregister(&mbraink_class);
r_class:
	unregister_chrdev_region(mbraink_dev_no, 1);

	return -EPERM;
}

static struct nla_policy mbraink_genl_policy[MBRAINK_A_MAX + 1] = {
	[MBRAINK_A_MSG] = { .type = NLA_NUL_STRING },
};

static struct genl_ops mbraink_genl_ops[] = {
	{
		.cmd = MBRAINK_C_PID_CTRL,
		.flags = 0,
		.policy = mbraink_genl_policy,
		.doit = mbraink_genetlink_recv_msg,
		.dumpit = NULL,
	},
};

static const struct genl_multicast_group mbraink_genl_mcgr[] = {
	{ .name = "MBRAINK_MCGRP", },
};

static struct genl_family mbraink_genl_family = {
	.id = GENL_ID_GENERATE,
	.hdrsize = 0,
	.name = "MBRAINK_LINK",
	.version = 1,
	.maxattr = MBRAINK_A_MAX,
	.ops = mbraink_genl_ops,
	.n_ops = ARRAY_SIZE(mbraink_genl_ops),
	.mcgrps =  mbraink_genl_mcgr,
};

int mbraink_netlink_send_msg(const char *msg)
{
	struct sk_buff *skb = NULL;
	void *msg_head = NULL;
	int ret = -1, size = 0;

	if (mbraink_priv.client_pid != -1) {
		size = nla_total_size(strlen(msg) + 1);
		skb = genlmsg_new(size, GFP_ATOMIC);
		if (!skb) {
			pr_notice("[%s]: mbraink Failed to allocate new skb\n", __func__);
			return -ENOMEM;
		}
		msg_head = genlmsg_put(skb, mbraink_priv.client_pid, 0, &mbraink_genl_family,
					0, MBRAINK_C_PID_CTRL);
		if (msg_head == NULL) {
			pr_notice("[%s] genlmsg_put fail\n", __func__);
			nlmsg_free(skb);
			return -EMSGSIZE;
		}
		ret = nla_put(skb, MBRAINK_A_MSG, strlen(msg) + 1, msg);
		if (ret != 0) {
			pr_notice("[%s] nla_put fail, ret=[%d]\n", __func__, ret);
			genlmsg_cancel(skb, msg_head);
			nlmsg_free(skb);
			return ret;
		}
		genlmsg_end(skb, msg_head);
		ret = genlmsg_unicast(&init_net, skb, mbraink_priv.client_pid);
		if (ret < 0)
			pr_notice("[%s] nla_put fail, ret=[%d]\n", __func__, ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mbraink_netlink_send_msg);

static int mbraink_genetlink_recv_msg(struct sk_buff *skb, struct genl_info *info)
{
	struct nlmsghdr *nlhdr = NULL;

	nlhdr = nlmsg_hdr(skb);

	mbraink_priv.client_pid = nlhdr->nlmsg_pid;

	pr_info("[%s]: mbraink receive the connected client pid %d\n",
		__func__,
		mbraink_priv.client_pid);
	return 0;
}

static int mbraink_genetlink_init(void)
{
	int ret = 0;

	mbraink_priv.client_pid = -1;

	ret = genl_register_family(&mbraink_genl_family);
	if (ret != 0)
		pr_notice("mbraink Failed to register genetlink family, ret %d\n", ret);

	return ret;
}

static int mbraink_init(void)
{
	int ret = 0;

	ret = mbraink_dev_init();
	if (ret)
		pr_notice("mbraink device init failed.\n");

	ret = mbraink_genetlink_init();
	if (ret)
		pr_notice("mbraink genetlink init failed.\n");

	ret = mbraink_process_tracer_init();
	if (ret)
		pr_notice("mbraink tracer init failed.\n");

	ret =  mbraink_cpufreq_notify_init();
	if (ret)
		pr_notice("mbraink cpufreq tracer init failed.\n");

	return ret;
}

static void mbraink_dev_exit(void)
{
#if IS_ENABLED(CONFIG_PM)
	int ret = 0;
#endif
	device_remove_file(&mbraink_device, &dev_attr_mbraink_info);

	device_unregister(&mbraink_device);
	mbraink_device.class = NULL;

	class_unregister(&mbraink_class);
	cdev_del(&mbraink_priv.mbraink_cdev);
	unregister_chrdev_region(mbraink_device.devt, 1);

    #if IS_ENABLED(CONFIG_PM)
    /* register pm notifier */
	ret = unregister_pm_notifier(&mbraink_sys_res_pm_notifier_func);
	if (ret != 0) {
		/* Failed to unregister_pm_notifier */
		pr_notice("Failed to unregister_pm_notifier(%d)\n", ret);
	}
    #endif

	pr_info("[MBK_INFO] %s: MBraink device exit done, major:minor %u:%u\n",
			__func__,
			MAJOR(mbraink_device.devt),
			MINOR(mbraink_device.devt));
}

static void mbraink_genetlink_exit(void)
{
	genl_unregister_family(&mbraink_genl_family);
	pr_info("[%s] mbraink_genetlink exit done.\n", __func__);
}

static void mbraink_exit(void)
{
	mbraink_dev_exit();
	mbraink_genetlink_exit();
	mbraink_process_tracer_exit();
	mbraink_gpu_deinit();
	mbraink_audio_deinit();
	mbraink_cpufreq_notify_exit();
}

module_init(mbraink_init);
module_exit(mbraink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK Linux Device Driver");
MODULE_VERSION("1.0");
