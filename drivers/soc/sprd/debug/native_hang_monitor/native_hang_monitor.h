/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __RT_MINITOR__H__
#define __RT_MINITOR__H__
/*		rt_monitor		*/
#define SS_WDT_CTRL_SET_PARA	1  /*	systemserver watchdog control cmd	*/
#define SF_WDT_CTRL_SET_PARA	11 /*    surfacefilinger watchdog control cmd  */
#define SF_WDT_CTRL_GET_PARA	12 /*    surfacefilinger watchdog control cmd  */

#define HANG_INFO_MAX 	(1 * 1024 * 1024)
#define MAX_STRING_SIZE 256
#define WAIT_BOOT_COMPLETE 120 		/*wait boot 120 s */
#define MAX_KERNEL_BT 16    /* MAX_NR_FRAME for max unwind layer */
#define NR_FRAME 32
#define SYMBOL_SIZE_L 140
#define SYMBOL_SIZE_S 80
#define CORE_TASK_NAME_SIZE 20
#define CORE_TASK_NUM_MAX 20
#define TASK_STATE_TO_CHAR_STR "RSDTtZXxKWP"

struct thread_backtrace_info {
	__u32 pid;
	__u32 nr_entries;
	struct backtrace_frame *entries;
};

struct backtrace_frame {
	 __u64 pc;
	__u64 lr;
	__u32 pad[5];
	char pc_symbol[SYMBOL_SIZE_S];
	char lr_symbol[SYMBOL_SIZE_L];
};

struct core_task_info {
	int pid;
	char name[CORE_TASK_NAME_SIZE];
};
#endif /*	__RT_MINITOR__H__	*/
