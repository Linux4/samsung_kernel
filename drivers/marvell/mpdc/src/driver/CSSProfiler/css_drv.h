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

#ifndef __PX_CSS_DRV_H__
#define __PX_CSS_DRV_H__

#include <linux/string.h>
#include <linux/limits.h>
#include <linux/wait.h>

#include "PXD_css.h"
#include "constants.h"
#include "ring_buffer.h"
#include "CSSProfilerSettings.h"

#define MAX_CSS_DEPTH 100

struct css_sampling_op_mach
{
	int (*start)(bool is_start_paused);
	int (*stop)(void);
	int (*pause)(void);
	int (*resume)(void);
	unsigned long long (*get_count_for_cal)(int reg_id);
};

extern struct css_sampling_op_mach css_sampling_op_pxa2;
extern struct css_sampling_op_mach css_sampling_op_pj1;
extern struct css_sampling_op_mach css_sampling_op_pj4;
extern struct css_sampling_op_mach css_sampling_op_pj4b;
extern struct css_sampling_op_mach css_sampling_op_a9;

extern bool write_sample(PXD32_CSS_Call_Stack_V2 * sample_rec);

extern bool g_launched_app_exit;
extern bool g_is_image_load_set;
extern bool g_is_app_exit_set;

/* the pid of auto launched application */
extern pid_t g_launch_app_pid;
/* the full path of the wait image */
extern char g_image_load_path[PATH_MAX];

extern struct css_sampling_op_mach * css_samp_op;

DECLARE_PER_CPU(struct RingBufferInfo, g_sample_buffer);
extern struct RingBufferInfo g_module_buffer;
DECLARE_PER_CPU(char *, g_bt_buffer);

extern struct CSSTimerSettings g_tbs_settings;
extern struct CSSEventSettings g_ebs_settings;
extern bool g_calibration_mode;
extern bool g_is_image_load_set;
extern bool g_is_app_exit_set;

extern char g_image_load_path[PATH_MAX];
extern pid_t g_launch_app_pid;

extern bool g_launched_app_exit;
extern void** system_call_table;

extern wait_queue_head_t pxcss_kd_wait;

extern unsigned long long g_sample_count;

extern unsigned long get_timestamp_freq(void);

extern int handle_image_loaded(void);

extern void backtrace(struct pt_regs * const orig_regs, unsigned int cpu, pid_t pid, pid_t tid, PXD32_CSS_Call_Stack_V2 * css_data);
extern bool write_css_data(unsigned cpu, PXD32_CSS_Call_Stack_V2 *css_data);
extern void fill_css_data_head(PXD32_CSS_Call_Stack_V2 *css_data, pid_t pid, pid_t tid, unsigned int reg_id, unsigned long long ts);



#endif /* __PX_HOTSPOT_DRV_H__ */
