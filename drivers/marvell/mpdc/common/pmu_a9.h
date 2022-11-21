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

#ifndef __PX_CTX_A9_PMU_DEF_H__
#define __PX_CTX_A9_PMU_DEF_H__

#include <linux/types.h>
#include <linux/version.h>
#include <linux/device.h>
#include <mach/irqs.h>

#include "pxpmu.h"

#define A9_PMN_NUM 6

struct pmu_registers_a9
{
	u32 pmcr;
	u32 cntenset;
	u32 cntenclr;
	u32 ovsr;
	u32 swinc;
	u32 selr;
	u32 ccnt;
	u32 evtsel[A9_PMN_NUM];
	u32 pmncnt[A9_PMN_NUM];
	u32 useren;
	u32 intenset;
	u32 intenclr;
};

/* Read/Write Performance Monitor Control Register */
extern u32  A9_Read_PMCR(void);
extern void A9_Write_PMCR(u32 value);

/* Read/Write Count Enable Set Register */
extern u32  A9_Read_CNTENSET(void);
extern void A9_Write_CNTENSET(u32 value);

/* Read/Write Count Enable Clear Register */
extern u32  A9_Read_CNTENCLR(void);
extern void A9_Write_CNTENCLR(u32 value);

/* Read/Write Overflow Flag Status Register */
extern u32  A9_Read_OVSR(void);
extern void A9_Write_OVSR(u32 value);

/* Read/Write Software Increment Register */
extern u32  A9_Read_SWINC(void);
extern void A9_Write_SWINC(u32 value);

/* Read/Write Event Counter Selection Register */
extern u32  A9_Read_PMNXSEL(void);
extern void A9_Write_PMNXSEL(u32 value);

/* Read/Write Cycle Count Register */
extern u32  A9_Read_CCNT(void);
extern void A9_Write_CCNT(u32 value);

/* Read/Write Event Selection Register */
extern u32  A9_Read_EVTSEL(void);
extern void A9_Write_EVTSEL(u32 value);

/* Read/Write Performance Monitor Count Registers */
extern u32  A9_Read_PMNCNT(void);
extern void A9_Write_PMNCNT(u32 value);

/* Read/Write User Enable Register */
extern u32  A9_Read_USEREN(void);
extern void  A9_Write_USEREN(u32 value);

/* Read/Write Interrupt Enable Set Register */
extern u32  A9_Read_INTENSET(void);
extern void  A9_Write_INTENSET(u32 value);

/* Read/Write Interrupt Enable Clear Register */
extern u32  A9_Read_INTENCLR(void);
extern void  A9_Write_INTENCLR(u32 value);

extern bool A9_WritePMUCounter(int counterId, u32 value);
extern u32 A9_ReadPMUCounter(int counterId);

extern unsigned int get_pmu_event_id(unsigned int user_event_id);

extern int get_pmu_resource(struct resource ** pmu_resource, unsigned int * pmu_resource_num);
extern int set_pmu_resource(int core_num, unsigned int * pmu_irq);

extern void display_pmu_registers(void);

#endif /* __PX_CTX_A9_PMU_DEF_H__ */
