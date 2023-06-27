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

#ifndef __PX_PXA2_PMU_DEF_H__
#define __PX_PXA2_PMU_DEF_H__

#include <linux/types.h>

#include "pxpmu.h"

#define PXA2_PMN_NUM 4

#define PXA2_PMU_PMNC    0
#define PXA2_PMU_CCNT    1
#define PXA2_PMU_PMN0    2
#define PXA2_PMU_PMN1    3
#define PXA2_PMU_PMN2    4
#define PXA2_PMU_PMN3    5
#define PXA2_PMU_INTEN   6
#define PXA2_PMU_FLAG    7
#define PXA2_PMU_EVTSEL  8


#define EVENT_CNT_NUM 		4
#define PMNC_ENABLE_BIT 	0x00000001
#define FLAG_OVERFLOW_BITS 	0x0000001F
#define CCNT_OVERFLAG_BIT 	0x00000001
#define PMN0_OVERFLAG_BIT 	0x00000002
#define PMN1_OVERFLAG_BIT 	0x00000004
#define PMN2_OVERFLAG_BIT 	0x00000008
#define PMN3_OVERFLAG_BIT 	0x00000010

#define DISABLE_PXA2_PMU_AND_CLR_OVR(pmnc, flag) \
	pmnc = ReadPMUReg(PXA2_PMU_PMNC);\
	WritePMUReg(PXA2_PMU_PMNC, pmnc & ~PMNC_ENABLE_BIT);\
	flag = ReadPMUReg(PXA2_PMU_FLAG);\
	WritePMUReg(PXA2_PMU_FLAG, FLAG_OVERFLOW_BITS)

struct pmu_registers_pxa2
{
	u32 pmnc;
	u32 ccnt;
	u32 pmn[PXA2_PMN_NUM];
	u32 inten;
	u32 flag;
	u32 evtsel;
};

extern u32 ReadPMUReg(int reg);
extern void WritePMUReg(int reg, u32 value);

extern int get_pmu_resource(struct resource ** pmu_resource, unsigned int * pmu_resource_num);
extern int set_pmu_resource(int core_num, unsigned int * pmu_irq);

#endif /* __PX_PXA2_PMU_DEF_H__ */
