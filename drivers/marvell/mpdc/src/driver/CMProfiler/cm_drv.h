/*
** (C) Copyright 2009 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#ifndef __PX_COUNTER_MONITOR_DRV_H__
#define __PX_COUNTER_MONITOR_DRV_H__

#include <linux/types.h>
#include <linux/wait.h>

#include "ring_buffer.h"
#include "CMProfilerDef.h"
#include "common.h"

struct cm_ctr_op_mach
{
	int (*start)(void);
	int (*stop)(void);
	int (*pause)(void);
	int (*resume)(void);
	//bool (*read_counter_on_all_cpus)(int counter_id, unsigned long long * value);
	bool (*read_counter_on_this_cpu)(int counter_id, unsigned long long * value);
	void (*pmu_isr)(void);
};

extern struct cm_ctr_op_mach *cm_ctr_op;
extern struct cm_ctr_op_mach cm_op_pxa2;
extern struct cm_ctr_op_mach cm_op_pj1;
extern struct cm_ctr_op_mach cm_op_pj4;
extern struct cm_ctr_op_mach cm_op_pj4b;
extern struct cm_ctr_op_mach cm_op_a9;

extern struct RingBufferInfo g_process_create_buf_info;
extern struct RingBufferInfo g_thread_create_buf_info;
DECLARE_PER_CPU(struct RingBufferInfo, g_thread_switch_buf_info);

extern void** system_call_table;
extern pid_t g_data_collector_pid;
extern wait_queue_head_t pxcm_kd_wait;
extern unsigned long g_mode;
extern struct CMCounterConfigs g_counter_config;
extern pid_t g_specific_pid;
extern pid_t g_specific_tid;
extern bool g_is_app_exit_set;
extern pid_t g_launch_app_pid;
extern bool g_launched_app_exit;
extern unsigned long long **spt_cv;

extern void write_thread_create_info(unsigned int pid, unsigned int tid, unsigned long long ts);
extern bool is_specific_object_mode(void);
extern int read_counter_on_all_cpus(unsigned int counter_id, unsigned long long * value);

#endif /* __PX_COUNTER_MONITOR_DRV_H__ */
