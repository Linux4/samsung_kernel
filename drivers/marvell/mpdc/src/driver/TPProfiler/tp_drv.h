/*
** (C) Copyright 2011 Marvell International Ltd.
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

#ifndef __PX_TP_DRV_H__
#define __PX_TP_DRV_H__

#include <linux/string.h>
#include <linux/wait.h>
#include <asm/traps.h>

#include "PXD_tp.h"
#include "common.h"
#include "vdkiocode.h"

struct tp_sampling_op_mach
{
	int (*start)(bool is_start_paused);
	int (*stop)(void);
	int (*pause)(void);
	int (*resume)(void);
	unsigned long long (*get_count_for_cal)(int reg_id);
};

extern wait_queue_head_t pxtp_kd_wait;
extern struct RingBufferInfo g_event_buffer;
extern PXD32_DWord convert_to_PXD32_DWord(unsigned long long n);

extern pid_t g_mpdc_tgid;
extern struct RingBufferInfo g_event_buffer;
extern struct RingBufferInfo g_module_buffer;

extern struct miscdevice px_tp_d;
extern struct miscdevice pxtp_event_d;
extern struct miscdevice pxtp_module_d;
extern struct miscdevice pxtp_dsa_d;

extern char g_image_load_path[PATH_MAX];
/* the pid of auto launched application */
extern pid_t g_launch_app_pid;
extern pid_t g_tgid;

extern bool g_launched_app_exit;
extern bool g_is_image_load_set;
extern bool g_is_app_exit_set;

//extern struct px_tp_dsa *g_dsa;
extern void** system_call_table;

extern int start_module_tracking(void);
extern int stop_module_tracking(void);
extern int start_functions_hooking(void);
extern int stop_functions_hooking(void);
extern int add_module_record(struct add_module_data * data);
extern int handle_image_loaded(void);

#endif /* __PX_TP_DRV_H__ */
