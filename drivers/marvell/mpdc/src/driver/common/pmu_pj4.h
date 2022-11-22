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

#ifndef __PX_PJ4_PMU_DEF_H__
#define __PX_PJ4_PMU_DEF_H__

#include <linux/types.h>
#include <linux/version.h>
#include <linux/device.h>
#include <mach/irqs.h>

#include "pxpmu.h"

#define PJ4_PMN_NUM 4

struct pmu_registers_pj4
{
	u32 pmnc;
	u32 cntens;
	u32 cntenc;
	u32 flag;
	u32 swincr;
	u32 pmnxsel;
	u32 ccnt;
	u32 evtsel[PJ4_PMN_NUM];
	u32 pmncnt[PJ4_PMN_NUM];
	u32 useren;
	u32 intens;
	u32 intenc;
};

extern u32  PJ4_Read_PMNC(void);
extern void PJ4_Write_PMNC(u32 value);

extern u32  PJ4_Read_CNTENS(void);
extern void PJ4_Write_CNTENS(u32 value);

extern u32  PJ4_Read_CNTENC(void);
extern void PJ4_Write_CNTENC(u32 value);

extern u32  PJ4_Read_FLAG(void);
extern void PJ4_Write_FLAG(u32 value);

extern u32  PJ4_Read_SWINCR(void);
extern void PJ4_Write_SWINCR(u32 value);

extern u32  PJ4_Read_PMNXSEL(void);
extern void PJ4_Write_PMNXSEL(u32 value);

extern u32  PJ4_Read_CCNT(void);
extern void PJ4_Write_CCNT(u32 value);

extern u32  PJ4_Read_EVTSEL(void);
extern void PJ4_Write_EVTSEL(u32 value);

extern u32  PJ4_Read_PMNCNT(void);
extern void PJ4_Write_PMNCNT(u32 value);

extern u32  PJ4_Read_USEREN(void);
extern void  PJ4_Write_USEREN(u32 value);

extern u32  PJ4_Read_INTENS(void);
extern void  PJ4_Write_INTENS(u32 value);

extern u32  PJ4_Read_INTENC(void);
extern void  PJ4_Write_INTENC(u32 value);

extern bool PJ4_WritePMUCounter(int counterId, u32 value);
extern u32 PJ4_ReadPMUCounter(int counterId);

extern int get_pmu_resource(struct resource ** pmu_resource, unsigned int * pmu_resource_num);
extern int set_pmu_resource(int core_num, unsigned int * pmu_irq);

extern void display_pmu_registers(void);

#endif /* __PX_PJ4_PMU_DEF_H__ */
