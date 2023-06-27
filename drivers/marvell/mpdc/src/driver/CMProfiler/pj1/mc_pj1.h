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

#ifndef __PX_MC_PJ1_H__
#define __PX_MC_PJ1_H__

#define IRQ_DDR_MC      26
#define DDR_MCU_BASE    0xB0000000

#define PERF_COUNT_CNTRL_0  (0x0F00 | DDR_MCU_BASE)
#define PERF_COUNT_CNTRL_1  (0x0F10 | DDR_MCU_BASE)
#define PERF_COUNT_STAT     (0x0F20 | DDR_MCU_BASE)
#define PERF_COUNT_SEL      (0x0F40 | DDR_MCU_BASE)
#define PERF_COUNT          (0x0F50 | DDR_MCU_BASE)

/* PERF_COUNT_CNTRL_0 bits */
#define ENABLE_COUNT_0_INT      0x100
#define ENABLE_COUNT_1_INT      0x200
#define ENABLE_COUNT_2_INT      0x400
#define ENABLE_COUNT_3_INT      0x800
#define ENABLE_COUNT_0          0x1
#define ENABLE_COUNT_1          0x2
#define ENABLE_COUNT_2          0x4
#define ENABLE_COUNT_3          0x8
#define DISABLE_COUNT_0_INT     0x1000
#define DISABLE_COUNT_1_INT     0x2000
#define DISABLE_COUNT_2_INT     0x4000
#define DISABLE_COUNT_3_INT     0x8000
#define DISABLE_COUNT_ALL_INT   0xF000
#define DISABLE_COUNT_0         0x10
#define DISABLE_COUNT_1         0x20
#define DISABLE_COUNT_2         0x40
#define DISABLE_COUNT_3         0x80
#define DISABLE_COUNT_ALL       0xF0

/* PERF_COUNT_CNTRL_1 bits */
#define CONTINUE_WHEN_OVERFLOW    0x10
#define START_WHEN_ENABLED        0x0

/* PERF_COUNT_STAT bits */
#define COUNT_0_OVERFLOW    0x10000
#define COUNT_1_OVERFLOW    0x20000
#define COUNT_2_OVERFLOW    0x40000
#define COUNT_3_OVERFLOW    0x80000


#endif
