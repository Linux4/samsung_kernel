/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MBRAINK_H
#define MBRAINK_H

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <net/sock.h>
#include <linux/pid.h>

#include "mbraink_ioctl_struct_def.h"

#define IOC_MAGIC	'k'

#define MAX_BUF_SZ			1024

/*Mbrain Delegate Info List*/
#define POWER_INFO				'1'
#define VIDEO_INFO				'2'
#define POWER_SUSPEND_EN		'3'
#define PROCESS_MEMORY_INFO		'4'
#define PROCESS_STAT_INFO		'5'
#define THREAD_STAT_INFO		'6'
#define SET_MINITOR_PROCESS		'7'
#define MEMORY_DDR_INFO			'8'
#define IDLE_RATIO_INFO         '9'
#define TRACE_PROCESS_INFO      'a'
#define VCORE_INFO              'b'
#define CPUFREQ_NOTIFY_INFO		'c'
#define BATTERY_INFO			'e'
#define FEATURE_EN				'f'
#define WAKEUP_INFO				'g'
#define PMU_EN					'h'
#define PMU_INFO				'i'
#define POWER_SPM_RAW			'j'
#define MODEM_INFO				'k'
#define MEMORY_MDV_INFO			'l'
#define MONITOR_BINDER_PROCESS         'm'
#define TRACE_BINDER_INFO              'o'
#define VCORE_VOTE_INFO			'p'
#define POWER_SPM_L2_INFO		'q'
#define POWER_SCP_INFO			'r'
#define GPU_OPP_INFO			's'
#define GPU_STATE_INFO			't'
#define GPU_LOADING_INFO		'u'

/*Mbrain Delegate IOCTL List*/
#define RO_POWER				_IOR(IOC_MAGIC, POWER_INFO, char*)
#define RO_VIDEO				_IOR(IOC_MAGIC, VIDEO_INFO, char*)
#define WO_SUSPEND_POWER_EN		_IOW(IOC_MAGIC, POWER_SUSPEND_EN, char*)
#define RO_PROCESS_MEMORY		_IOR(IOC_MAGIC, PROCESS_MEMORY_INFO, \
							struct mbraink_process_memory_data*)
#define RO_PROCESS_STAT			_IOR(IOC_MAGIC, PROCESS_STAT_INFO, \
							struct mbraink_process_stat_data*)
#define RO_THREAD_STAT			_IOR(IOC_MAGIC, THREAD_STAT_INFO, \
							struct mbraink_thread_stat_data*)
#define WO_MONITOR_PROCESS		_IOW(IOC_MAGIC, SET_MINITOR_PROCESS, \
							struct mbraink_monitor_processlist*)
#define RO_MEMORY_DDR_INFO		_IOR(IOC_MAGIC, MEMORY_DDR_INFO, \
							struct mbraink_memory_ddrInfo*)
#define RO_IDLE_RATIO                  _IOR(IOC_MAGIC, IDLE_RATIO_INFO, \
							struct mbraink_audio_idleRatioInfo*)
#define RO_TRACE_PROCESS            _IOR(IOC_MAGIC, TRACE_PROCESS_INFO, \
							struct mbraink_tracing_pid_data*)
#define RO_VCORE_INFO                 _IOR(IOC_MAGIC, VCORE_INFO, \
							struct mbraink_power_vcoreInfo*)
#define RO_CPUFREQ_NOTIFY		_IOR(IOC_MAGIC, CPUFREQ_NOTIFY_INFO, \
							struct mbraink_cpufreq_notify_struct_data*)
#define RO_BATTERY_INFO			_IOR(IOC_MAGIC, BATTERY_INFO, \
							struct mbraink_battery_data*)
#define WO_FEATURE_EN		_IOW(IOC_MAGIC, FEATURE_EN, \
							struct mbraink_feature_en*)
#define RO_WAKEUP_INFO			_IOR(IOC_MAGIC, WAKEUP_INFO, \
							struct mbraink_power_wakeup_data*)
#define WO_PMU_EN				_IOW(IOC_MAGIC, PMU_EN, \
							struct mbraink_pmu_en*)
#define RO_PMU_INFO				_IOR(IOC_MAGIC, PMU_INFO, \
							struct mbraink_pmu_info*)

#define RO_POWER_SPM_RAW			_IOR(IOC_MAGIC, POWER_SPM_RAW, \
								struct mbraink_power_spm_raw*)

#define RO_MODEM_INFO			_IOR(IOC_MAGIC, MODEM_INFO, \
							struct mbraink_modem_raw*)
#define WO_MONITOR_BINDER_PROCESS	_IOW(IOC_MAGIC,	MONITOR_BINDER_PROCESS,	\
						struct mbraink_monitor_processlist*)
#define RO_TRACE_BINDER			_IOR(IOC_MAGIC, TRACE_BINDER_INFO,	\
						struct mbraink_binder_trace_data*)

#define RO_MEMORY_MDV_INFO			_IOR(IOC_MAGIC, MEMORY_MDV_INFO, \
							struct mbraink_memory_mdvInfo*)

#define RO_VCORE_VOTE			_IOR(IOC_MAGIC, VCORE_VOTE_INFO, \
						struct mbraink_voting_struct_data*)

#define RO_POWER_SPM_L2_INFO	_IOR(IOC_MAGIC, POWER_SPM_L2_INFO, \
						struct mbraink_power_spm_raw*)

#define RO_POWER_SCP_INFO	_IOR(IOC_MAGIC, POWER_SCP_INFO, \
						struct mbraink_power_scp_raw*)

#define RO_GPU_OPP_INFO	_IOR(IOC_MAGIC, GPU_OPP_INFO, \
						struct mbraink_gpu_opp_info*)

#define RO_GPU_STATE_INFO	_IOR(IOC_MAGIC, GPU_STATE_INFO, \
						struct mbraink_gpu_state_info*)

#define RO_GPU_LOADING_INFO	_IOR(IOC_MAGIC, GPU_LOADING_INFO, \
						struct mbraink_gpu_loading_info*)

#define SUSPEND_DATA	0
#define RESUME_DATA		1
#define CURRENT_DATA	2

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE    0
#endif

enum {
	MBRAINK_A_UNSPEC,
	MBRAINK_A_MSG,
	__MBRAINK_A_MAX,
};
#define MBRAINK_A_MAX (__MBRAINK_A_MAX - 1)

enum {
	MBRAINK_C_UNSPEC,
	MBRAINK_C_PID_CTRL,
	__MBRAINK_C_MAX,
};
#define MBRAINK_C_MAX (__MBRAINK_C_MAX - 1)

struct mbraink_data {
#define CHRDEV_NAME     "mbraink_chrdev"
	struct cdev mbraink_cdev;
	char power_buffer[MAX_BUF_SZ * 3];
	char suspend_power_buffer[MAX_BUF_SZ];
	char resume_power_buffer[MAX_BUF_SZ];
	char suspend_power_info_en[2];
	int suspend_power_data_size;
	int resume_power_data_size;
	long long last_suspend_timestamp;
	long long last_resume_timestamp;
	long long last_suspend_ktime;
	struct mbraink_battery_data suspend_battery_buffer;
	int client_pid;
	unsigned int feature_en;
	unsigned int pmu_en;
};

int mbraink_netlink_send_msg(const char *msg);

#endif /*end of MBRAINK_H*/
