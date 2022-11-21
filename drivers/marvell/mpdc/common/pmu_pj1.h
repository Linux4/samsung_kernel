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

#ifndef __PX_PJ1_PMU_DEF_H__
#define __PX_PJ1_PMU_DEF_H__

#include <linux/types.h>
#include <linux/device.h>

#ifndef PX_SOC_PXA920
#include <mach/irqs.h>
#endif

#include "pxpmu.h"

#define PJ1_PMN_NUM 4

struct pmu_registers_pj1
{
	u32 inten;
	u32 flag;
	u32 cor[PJ1_PMN_NUM];
	u64 pmn[PJ1_PMN_NUM];
};

#define PJ1_PMU_COR0    0
#define PJ1_PMU_COR1    1
#define PJ1_PMU_COR2    2
#define PJ1_PMU_COR3    3

#define PJ1_PMU_PMN0    0
#define PJ1_PMU_PMN1    1
#define PJ1_PMU_PMN2    2
#define PJ1_PMU_PMN3    3

extern u32  PJ1_ReadINTEN(void);
extern void PJ1_WriteINTEN(u32 value);
extern u32  PJ1_ReadFLAG(void);
extern void PJ1_WriteFLAG(u32 value);
extern u32  PJ1_ReadCOR(int cor);
extern void PJ1_WriteCOR(int cor, u32 value);
extern u64  PJ1_ReadCounter(int counter);
extern void PJ1_WriteCounter(int counter, u64 value);
extern unsigned int get_pmu_event_id(unsigned int user_event_id);
extern int get_pmu_resource(struct resource ** pmu_resource, unsigned int * pmu_resource_num);
extern int set_pmu_resource(int core_num, unsigned int * pmu_irq);

extern void display_pmu_registers(void);

#endif /* __PX_PJ1_PMU_DEF_H__ */
