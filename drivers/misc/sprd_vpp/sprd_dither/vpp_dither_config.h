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

#ifndef _VPP_DITHER_CONFIG_H_
#define _VPP_DITHER_CONFIG_H_

#include <video/gsp_types_shark.h>
#include <mach/sci_glb_regs.h>
#include <mach/globalregs.h> //define IRQ_GSP_INT
#include <linux/delay.h>
#include <linux/clk.h>

#define DITHER_PATH_SIZE	1024

#define VPP_CLOCK_PARENT	("clk_vpp_parent")
#define VPP_CLOCK_NAME		("clk_vpp")


void VPP_Dither_Early_Init();

int VPP_Dither_Clock_Init(struct vpp_dither_device *dev);

void VPP_Dither_Clock_Enable(struct vpp_dither_device *dev);

void VPP_Dither_Clock_Disable(struct vpp_dither_device *dev);

void VPP_Dither_Module_Enable();

void VPP_Dither_Module_Disable();

void VPP_Dither_IRQSTATUS_Clear();

void VPP_Dither_IRQENABLE_SET(unsigned char irq_flag);

void VPP_Dither_Init(struct vpp_dither_device *dev);

void VPP_Dither_Deinit(struct vpp_dither_device *dev);

void VPP_Dither_Module_Reset();

void VPP_Dither_Info_Config(struct fb_to_vpp_info *fb_info, struct vpp_dither_device *dev);

void VPP_Dither_CFG_Config(struct vpp_dither_device *dev);

int VPP_Dither_Map(struct vpp_dither_device *dev);

int VPP_Dither_Unmap(struct vpp_dither_device *dev);

int irq_status_get();

irqreturn_t VPP_Dither_IRQ_Handler(int irq, void *dev_id);

void VPP_Dither_Trigger();

unsigned int Dither_WORKSTATUS_Get();

void VPP_Dither_Wait_Finish();

int Do_Dither_Work(struct fb_to_vpp_info *fb_info);



#endif


