/*
** (C) Copyright 2007 Marvell International Ltd.
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

/*++

Module Name:  $Workfile: pmu_pj1.c $

Abstract:
 This file is for pj1 PMU access function

Notes:

--*/
#include <linux/kernel.h>
#include <linux/device.h>

#include "pmu_pj1.h"
#include "constants.h"

#ifdef PX_SOC_ARMADA168
#include <mach/irqs.h>
struct resource pmu_resource_armada168[] =
{
	{
		.start  = IRQ_PXA168_PMU,
		.end    = IRQ_PXA168_PMU,
		.flags  = IORESOURCE_IRQ,
	},
};
#endif

#ifdef PX_SOC_PXA920
struct resource pmu_resource_pxa910[] =
{
	{
		.start  = 58,       /* this is not defined as a macro in the kernel header file */
		.end    = 58,
		.flags  = IORESOURCE_IRQ,
	},
};
#endif

/* This routine reads interrupt enable regsiter from co-processor 15 */
u32 PJ1_ReadINTEN(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c15, c14, 0" : "=r"(v));
	return v;
}

/* This routine writes interrupt enable regsiter */
void PJ1_WriteINTEN(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c15, c14, 0" : :"r"(value));
}

/* This routine reads overflow flag status regsiter */
u32 PJ1_ReadFLAG(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c15, c14, 1" : "=r"(v));
	return v;
}

/* This routine writes overflow flag status regsiter */
void PJ1_WriteFLAG(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c15, c14, 1" : :"r"(value));
}

/* This routine reads the Counter Operations Registers (COR) */
u32 PJ1_ReadCOR(int cor)
{
	u32 v = 0;
	switch (cor)
	{
	case PJ1_PMU_COR0:
		__asm__ __volatile__ ("mrc  p15, 0, %0, c15, c12, 0" : "=r"(v));
		break;
	case PJ1_PMU_COR1:
		__asm__ __volatile__ ("mrc  p15, 0, %0, c15, c12, 1" : "=r"(v));
		break;
	case PJ1_PMU_COR2:
		__asm__ __volatile__ ("mrc  p15, 0, %0, c15, c12, 2" : "=r"(v));
		break;
	case PJ1_PMU_COR3:
		__asm__ __volatile__ ("mrc  p15, 0, %0, c15, c12, 3" : "=r"(v));
		break;
	}
	return v;
}

/* Writes to the Counter Operations Registers */
void PJ1_WriteCOR(int cor, u32 value)
{
	switch (cor)
	{
	case PJ1_PMU_COR0:
		__asm__ __volatile__ ("mcr  p15, 0, %0, c15, c12, 0" : :"r"(value));
		break;
	case PJ1_PMU_COR1:
		__asm__ __volatile__ ("mcr  p15, 0, %0, c15, c12, 1" : :"r"(value));
		break;
	case PJ1_PMU_COR2:
		__asm__ __volatile__ ("mcr  p15, 0, %0, c15, c12, 2" : :"r"(value));
		break;
	case PJ1_PMU_COR3:
		__asm__ __volatile__ ("mcr  p15, 0, %0, c15, c12, 3" : :"r"(value));
		break;
	}
}

/* Read the Counter Register */
u64 PJ1_ReadCounter(int counter)
{
	u64 v = 0;
	u32 vl = 0;
	u32 vh = 0;

	switch (counter)
	{
	case PJ1_PMU_PMN0:
		__asm__ __volatile__ (
			"mrc     p15, 0, %0, c15, c13, 0\n\t"
			"mrc     p15, 0, %1, c15, c13, 1\n\t"
			: "=r"(vl), "=r"(vh));
		break;
	case PJ1_PMU_PMN1:
		__asm__ __volatile__ (
			"mrc     p15, 0, %0, c15, c13, 2\n\t"
			"mrc     p15, 0, %1, c15, c13, 3\n\t"
			: "=r"(vl), "=r"(vh));
		break;
	case PJ1_PMU_PMN2:
		__asm__ __volatile__ (
			"mrc     p15, 0, %0, c15, c13, 4\n\t"
			"mrc     p15, 0, %1, c15, c13, 5\n\t"
			: "=r"(vl), "=r"(vh));
		break;
	case PJ1_PMU_PMN3:
		__asm__ __volatile__ (
			"mrc     p15, 0, %0, c15, c13, 6\n\t"
			"mrc     p15, 0, %1, c15, c13, 7\n\t"
			: "=r"(vl), "=r"(vh));
		break;
	}
	v = vh;
	v <<= 32;
	v += vl;
	return v;
}

/* Writes to the Counter Register */
void PJ1_WriteCounter(int counter, u64 value)
{
	u32 vl = (u32)(value & 0xffffffff);
	u32 vh = (u32)(value >> 32);

	switch (counter)
	{
	case PJ1_PMU_PMN0:
		__asm__ __volatile__ (
			"mcr     p15, 0, %0, c15, c13, 0\n\t"
			"mcr     p15, 0, %1, c15, c13, 1\n\t"
			: : "r"(vl), "r"(vh));
		break;
	case PJ1_PMU_PMN1:
		__asm__ __volatile__ (
			"mcr     p15, 0, %0, c15, c13, 2\n\t"
			"mcr     p15, 0, %1, c15, c13, 3\n\t"
			: : "r"(vl), "r"(vh));
		break;
	case PJ1_PMU_PMN2:
		__asm__ __volatile__ (
			"mcr     p15, 0, %0, c15, c13, 4\n\t"
			"mcr     p15, 0, %1, c15, c13, 5\n\t"
			: : "r"(vl), "r"(vh));
		break;
	case PJ1_PMU_PMN3:
		__asm__ __volatile__ (
			"mcr     p15, 0, %0, c15, c13, 6\n\t"
			"mcr     p15, 0, %1, c15, c13, 7\n\t"
			: : "r"(vl), "r"(vh));
		break;
	}
}

unsigned int get_pmu_event_id(unsigned int user_event_id)
{
	return (1 << (user_event_id & EVENT_ID_MASK)) + 1;
}

int get_pmu_resource(struct resource ** pmu_resource, unsigned int * pmu_resource_num)
{
#ifdef PX_SOC_ARMADA168
	*pmu_resource      = pmu_resource_armada168;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_armada168);
	
	return 0;
#endif

#ifdef PX_SOC_PXA920
	*pmu_resource      = pmu_resource_pxa910;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_pxa910);
	
	return 0;
#endif

	return 0;
}

int set_pmu_resource(int core_num, unsigned int * pmu_irq)
{
	int i;
	struct resource * pmu_resource;
	
#ifdef PX_SOC_ARMADA168
	pmu_resource = pmu_resource_armada168;
#endif

#ifdef PX_SOC_PXA920
	pmu_resource = pmu_resource_pxa910;
#endif

	//if (core_num != ARRAY_SIZE(pmu_resource))
	//	return -EINVAL;

	for (i=0; i<core_num; i++)
	{
		pmu_resource[i].start = pmu_irq[i];
		pmu_resource[i].end   = pmu_irq[i];
	}
	
	return 0;
}

void display_pmu_registers(void)
{
	printk("[CPA] pmn0 = 0x%llx\n", PJ1_ReadCounter(PJ1_PMU_PMN0));
	printk("[CPA] pmn1 = 0x%llx\n", PJ1_ReadCounter(PJ1_PMU_PMN1));
	printk("[CPA] pmn2 = 0x%llx\n", PJ1_ReadCounter(PJ1_PMU_PMN2));
	printk("[CPA] pmn3 = 0x%llx\n", PJ1_ReadCounter(PJ1_PMU_PMN3));

	printk("[CPA] cor0 = 0x%x\n", PJ1_ReadCOR(PJ1_PMU_COR0));
	printk("[CPA] cor1 = 0x%x\n", PJ1_ReadCOR(PJ1_PMU_COR1));
	printk("[CPA] cor2 = 0x%x\n", PJ1_ReadCOR(PJ1_PMU_COR2));
	printk("[CPA] cor3 = 0x%x\n", PJ1_ReadCOR(PJ1_PMU_COR3));

	printk("[CPA] flag = 0x%x\n", PJ1_ReadFLAG());
	printk("[CPA] inten = 0x%x\n", PJ1_ReadINTEN());
}
