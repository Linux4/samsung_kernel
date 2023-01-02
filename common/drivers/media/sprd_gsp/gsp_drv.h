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
#ifndef __GSP_DRV_H__
#define __GSP_DRV_H__

#include "gsp_config_if.h"



#ifdef GSP_WORK_AROUND1
#define TRANSLATION_CALC_OPT //cut down the times of multiplication in the translation-calculation.

#define CMDQ_DPT_DEPTH  5
#define PARTS_CNT_MAX   5 //AQ has the max count 5

#define MAX(a,b)                   (((a)>(b))?(a):(b))
#define MIN(a,b)                   (((a)<(b))?(a):(b))

#define GSP_GET_RECT_FROM_PART_POINTS(pp,rect)\
{\
    rect.st_x = (MIN(pp.st_x,pp.end_x)&(~0x1));\
    rect.st_y = (MIN(pp.st_y,pp.end_y)&(~0x1));\
    rect.rect_w = ((MAX(pp.st_x,pp.end_x) - rect.st_x + 1)&(~0x1));\
    rect.rect_h = ((MAX(pp.st_y,pp.end_y) - rect.st_y + 1)&(~0x1));\
}

//a rectangle's left-top point and right-bottom point
typedef struct _part_points
{
    int32_t st_x;
    int32_t st_y;
    int32_t end_x;
    int32_t end_y;
}
PART_POINTS;

typedef int32_t Matrix33 [3][3];
typedef int32_t Matrix31 [3];


//the relationship is in "shark_gsp_test_case.xls"
typedef enum _L1_L0_RELATIONSHIP_TAG_
{
    L1_L0_RELATIONSHIP_I = 0x00,
    L1_L0_RELATIONSHIP_AD,// 1, L1(st_x,st_y) at left and top of L0,and L1(end_x,end_y) in L0 but not on right or bottom pixel line
    L1_L0_RELATIONSHIP_CF,
    L1_L0_RELATIONSHIP_NQ,
    L1_L0_RELATIONSHIP_LO,
    L1_L0_RELATIONSHIP_G,// 5
    L1_L0_RELATIONSHIP_GH,
    L1_L0_RELATIONSHIP_GK,
    L1_L0_RELATIONSHIP_JK,
    L1_L0_RELATIONSHIP_K,
    L1_L0_RELATIONSHIP_B,// 10
    L1_L0_RELATIONSHIP_BE,
    L1_L0_RELATIONSHIP_BP,
    L1_L0_RELATIONSHIP_MP,
    L1_L0_RELATIONSHIP_P,
    L1_L0_RELATIONSHIP_AC,// 15
    L1_L0_RELATIONSHIP_CQ,
    L1_L0_RELATIONSHIP_OQ,
    L1_L0_RELATIONSHIP_AO,
    L1_L0_RELATIONSHIP_AQ,// 19
    L1_L0_RELATIONSHIP_MAX,
} GSP_L1_L0_RELATIONSHIP_E;

#endif


#ifdef GSP_DEBUG
#define GSP_TRACE	printk
#else
#define GSP_TRACE	pr_debug
#endif

#define INVALID_USER_ID 0xFFFFFFFF
#define GSP_MAX_USER    8

typedef struct _gsp_user
{
    pid_t pid;
    uint32_t is_exit_force;// if the client process hungup on IOCTL-wait-finish, this variable mark it should wakeup and return anyway
    struct semaphore sem_open;// one thread can open device once per time
} gsp_user;

#endif
