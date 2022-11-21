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

#ifndef	_SPRD_VPP_H
#define _SPRD_VPP_H

#include <linux/ioctl.h>

/*----------------------------------------------------------------------------*
**                            Macro Definitions                               *
**---------------------------------------------------------------------------*/
/*
	Bit define, for video
*/
#define V_BIT_0               0x00000001
#define V_BIT_1               0x00000002
#define V_BIT_2               0x00000004
#define V_BIT_3               0x00000008
#define V_BIT_4               0x00000010
#define V_BIT_5               0x00000020
#define V_BIT_6               0x00000040
#define V_BIT_7               0x00000080
#define V_BIT_8               0x00000100
#define V_BIT_9               0x00000200
#define V_BIT_10              0x00000400
#define V_BIT_11              0x00000800
#define V_BIT_12              0x00001000
#define V_BIT_13              0x00002000
#define V_BIT_14              0x00004000
#define V_BIT_15              0x00008000
#define V_BIT_16              0x00010000
#define V_BIT_17              0x00020000
#define V_BIT_18              0x00040000
#define V_BIT_19              0x00080000
#define V_BIT_20              0x00100000
#define V_BIT_21              0x00200000
#define V_BIT_22              0x00400000
#define V_BIT_23              0x00800000
#define V_BIT_24              0x01000000
#define V_BIT_25              0x02000000
#define V_BIT_26              0x04000000
#define V_BIT_27              0x08000000
#define V_BIT_28              0x10000000
#define V_BIT_29              0x20000000
#define V_BIT_30              0x40000000
#define V_BIT_31              0x80000000

#define USE_INTERRUPT_DEINT
//#define SEQUENCE_RUNNING
#define SET_CLK_FOR_VPP_TWICE
#define SPRD_VPP_MAP_SIZE 0x4000

#define SPRD_VPP_IOCTL_MAGIC 'd'
#define SPRD_VPP_ALLOCATE_PHYSICAL_MEMORY _IO(SPRD_VPP_IOCTL_MAGIC, 1)
#define SPRD_VPP_FREE_PHYSICAL_MEMORY         _IO(SPRD_VPP_IOCTL_MAGIC, 2)
#define SPRD_VPP_DEINT_COMPLETE                     _IO(SPRD_VPP_IOCTL_MAGIC, 3)
#define SPRD_VPP_DEINT_ACQUIRE                     _IO(SPRD_VPP_IOCTL_MAGIC, 4)
#define SPRD_VPP_DEINT_RELEASE                     _IO(SPRD_VPP_IOCTL_MAGIC, 5)
#define VPP_RESET                     _IO(SPRD_VPP_IOCTL_MAGIC, 6)


#define VPP_CFG     	        (0x0000)
#define VPP_INT_STS 	(0x0010)
#define VPP_INT_MSK 	(0x0014)
#define VPP_INT_CLR 	(0x0018)
#define VPP_INT_RAW 	(0x001C)
#define DEINT_PATH_CFG   	(0x2000)
#define DEINT_PATH_START   	(0x2004)
#define DEINT_IMG_SIZE   	(0x2008)

typedef struct vpp_drv_buffer_t {
    unsigned int size;
    unsigned int phys_addr;
    unsigned long base;	     /*kernel logical address in use kernel*/
    unsigned long virt_addr; /* virtual user space address */
} VPP_DRV_BUFFER_T;

enum sprd_vpp_frequency_e {
    VPP_FREQENCY_LEVEL_0 = 0,
    VPP_FREQENCY_LEVEL_1 = 1,
    VPP_FREQENCY_LEVEL_2 = 2,
    VPP_FREQENCY_LEVEL_3 = 3
};

#endif
