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

#ifndef __PX_HOTSPOT_DRV_H__
#define __PX_HOTSPOT_DRV_H__

#include <linux/string.h>
#include <linux/limits.h>
#include <linux/wait.h>

#include "PXD_hs.h"
#include "constants.h"
#include "ring_buffer.h"
#include "HSProfilerSettings.h"
#include "common.h"

struct hs_sampling_op_mach
{
	int (*start)(bool is_start_paused);
	int (*stop)(void);
	int (*pause)(void);
	int (*resume)(void);
	unsigned long long (*get_count_for_cal)(int reg_id);
};

extern struct hs_sampling_op_mach hs_sampling_op_pxa2;
extern struct hs_sampling_op_mach hs_sampling_op_pj1;
extern struct hs_sampling_op_mach hs_sampling_op_pj4;
extern struct hs_sampling_op_mach hs_sampling_op_a9;
extern struct hs_sampling_op_mach hs_sampling_op_pj4b;

extern bool write_sample(unsigned int cpu, PXD32_Hotspot_Sample_V2 * sample_rec);

extern struct RingBufferInfo g_module_buffer;
DECLARE_PER_CPU(struct RingBufferInfo, g_sample_buffer);

extern unsigned long long g_sample_count;

extern void** system_call_table;
extern wait_queue_head_t pxhs_kd_wait;

extern struct HSTimerSettings g_tbs_settings;
extern struct HSEventSettings g_ebs_settings;
extern bool g_calibration_mode;

extern bool g_is_image_load_set;
extern bool g_is_app_exit_set;
extern bool g_launched_app_exit;

/* the full path of the wait image */
extern char g_image_load_path[PATH_MAX];

/* the pid of auto launched application */
extern pid_t g_launch_app_pid;

extern int handle_image_loaded(void);

#endif /* __PX_HOTSPOT_DRV_H__ */
