/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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

#ifndef _VPP_DITHER_TYPES_H_
#define _VPP_DITHER_TYPES_H_

#include <linux/types.h>
#include <linux/earlysuspend.h>

#define VPP_DITHER_IRQ_NUM		40
#define VPP_DITHER_BASE_ADDR	0X61000000


struct fb_to_vpp_info		//FB Interface
{
	unsigned int input_format;		//0:ARGB888		1:RGBA888
	unsigned int output_format;		//0:RGB565		1:RGB666
	unsigned int pixel_type;		//0:one pixel half word		1:one pixel one word
	unsigned int img_width;
	unsigned int img_height;
	unsigned int buf_addr;			//buf address
};

struct vpp_dither_reg_info
{
	unsigned int input_format;		//ARGB888/RGBA888
	unsigned int output_format;		//RGB565/RGB666
	unsigned int pixel_type;		//half_word / one_word
	unsigned int block_mod;
	unsigned int img_width;
	unsigned int img_height;
	unsigned int src_addr;
	unsigned int des_addr;
};


typedef struct vpp_dither_device
{
	unsigned int is_device_free;
	unsigned int irq_num;
	unsigned int s_suspend_resume_flag;
	unsigned int reg_base_addr;
	struct clk *clk_vpp;
	struct clk *clk_mm;
	struct clk *clk_vpp_parent;
	struct semaphore hw_resource_sem;
	struct semaphore wait_interrupt_sem;
	struct vpp_dither_reg_info reg_info;
	struct miscdevice misc_dev;
#ifdef CONFIG_OF
	struct device *of_dev;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
}vpp_dither_device;

#define TB_REG_GET(reg_addr)		(*(volatile unsigned int *)(reg_addr))
#define TB_REG_SET(reg_addr, value)	*(volatile unsigned int *)(reg_addr) = (value)

#endif


