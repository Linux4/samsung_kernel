/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#ifndef _SCALE_DRV_H_
#define _SCALE_DRV_H_

#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/types.h>
#include <video/sprd_scale_k.h>
#include "scale_reg.h"

//#define SCALE_DEBUG

#ifdef SCALE_DEBUG
	#define SCALE_TRACE             printk
#else
	#define SCALE_TRACE             pr_debug
#endif

typedef void (*scale_isr_func)(void *scale_k_private);

struct scale_path_info{
	struct scale_size_t input_size;
	struct scale_rect_t  input_rect;
	struct scale_size_t sc_input_size;
	struct scale_addr_t input_addr;
	uint32_t input_format;
	struct scale_size_t output_size;
	struct scale_addr_t output_addr;
	uint32_t output_format;
	uint32_t scale_mode;
	uint32_t slice_height;
	uint32_t slice_out_height;
	uint32_t slice_in_height;
	uint32_t is_last_slice;
	void *user_data;
	uint32_t sc_deci_val;
	void * coeff_addr;
	uint32_t is_wait_stop;
	struct semaphore done_sem;
};

struct scale_drv_private{
	struct scale_path_info path_info;
	struct scale_frame_info_t frm_info;
	spinlock_t scale_drv_lock;
	scale_isr_func user_isr_func;
	void *scale_fd;/*scale file*/
};

int32_t scale_k_module_en(struct device_node *dn);
int32_t scale_k_module_dis(struct device_node *dn);
int scale_k_start(struct scale_frame_param_t *cfg_ptr, struct scale_path_info *path_info_ptr);
int scale_k_stop(void);
int scale_k_continue(struct scale_slice_param_t *cfg_ptr, struct scale_path_info *path_info_ptr);
int scale_k_isr_reg(scale_isr_func user_func, struct scale_drv_private *drv_private);

#endif
