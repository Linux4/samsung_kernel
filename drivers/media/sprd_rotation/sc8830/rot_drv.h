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
#ifndef _ROT_DRV_H_
#define _ROT_DRV_H_

#include <linux/of.h>
#include <linux/of_device.h>

#include "rot_reg.h"

/*#define ROTATE_DEBUG*/

#ifdef ROTATE_DEBUG
	#define ROTATE_TRACE printk
#else
	#define ROTATE_TRACE pr_debug
#endif


typedef void (*rot_isr_func)(void *rot_k_private);

typedef struct _rot_param_tag {
	ROT_SIZE_T img_size;
	ROT_DATA_FORMAT_E format;
	ROT_ANGLE_E angle;
	ROT_ADDR_T src_addr;
	ROT_ADDR_T dst_addr;
	uint32_t s_addr;
	uint32_t d_addr;
	ROT_PIXEL_FORMAT_E pixel_format;
	ROT_UV_MODE_E uv_mode;
	int is_end;
	ROT_ENDIAN_E src_endian;
	ROT_ENDIAN_E dst_endian;
} ROT_PARAM_CFG_T;

struct rot_drv_private{
	ROT_PARAM_CFG_T cfg;
	rot_isr_func user_isr_func;
	spinlock_t rot_drv_lock;
	void *rot_fd;/*rot file*/
};

int rot_k_module_en(struct device_node *dn);
int rot_k_module_dis(struct device_node *dn);
int rot_k_isr_reg(rot_isr_func user_func,struct rot_drv_private *drv_private);
int rot_k_is_end(ROT_PARAM_CFG_T *s);
int rot_k_set_UV_param(ROT_PARAM_CFG_T *s);
void rot_k_register_cfg(ROT_PARAM_CFG_T *s);
void rot_k_close(void);
int rot_k_io_cfg(ROT_CFG_T * param_ptr, ROT_PARAM_CFG_T *s);
#endif
