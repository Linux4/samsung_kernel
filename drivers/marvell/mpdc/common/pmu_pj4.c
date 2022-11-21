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

Module Name:  $Workfile: pmu_pj4.c $

Abstract:
 This file is for pj4 PMU access function

Notes:

--*/
#include <linux/kernel.h>
#include <linux/device.h>

#include "pmu_pj4.h"
#include "RegisterId_pj4.h"

#ifdef PX_SOC_ARMADA610
struct resource pmu_resource_mmp2[] =
{
	{
		.start  = IRQ_MMP2_PERF,
		.end    = IRQ_MMP2_PERF,
		.flags  = IORESOURCE_IRQ,
	},
};
#endif

#ifdef PX_SOC_ARMADA510
struct resource pmu_resource_dove[] =
{
	{
		.start  = IRQ_DOVE_PERFORM_MNTR,
		.end    = IRQ_DOVE_PERFORM_MNTR,
		.flags  = IORESOURCE_IRQ,
	},
};
#endif

#ifdef PX_SOC_PXA955
struct resource pmu_resource_mg1[] =
{
	{
		.start  = IRQ_PMU,
		.end    = IRQ_PMU,
		.flags  = IORESOURCE_IRQ,
	},
};
#endif

u32 PJ4_Read_PMNC(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 0" : "=r"(v));
	return v;
}

void PJ4_Write_PMNC(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 0" : :"r"(value));
}

u32 PJ4_Read_CNTENS(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 1" : "=r"(v));
	return v;
}

void PJ4_Write_CNTENS(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 1" : :"r"(value));
}


u32 PJ4_Read_CNTENC(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 2" : "=r"(v));
	return v;
}

void PJ4_Write_CNTENC(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 2" : :"r"(value));
}

u32 PJ4_Read_FLAG(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 3" : "=r"(v));
	return v;
}

void PJ4_Write_FLAG(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 3" : :"r"(value));
}

u32 PJ4_Read_SWINCR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 4" : "=r"(v));
	return v;
}

void PJ4_Write_SWINCR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 4" : :"r"(value));
}

u32 PJ4_Read_PMNXSEL(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 5" : "=r"(v));
	return v;
}

void PJ4_Write_PMNXSEL(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 5" : :"r"(value));
}

u32 PJ4_Read_CCNT(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 0" : "=r"(v));
	return v;
}

void PJ4_Write_CCNT(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 0" : :"r"(value));
}

u32 PJ4_Read_EVTSEL(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 1" : "=r"(v));
	return v;
}

void PJ4_Write_EVTSEL(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 1" : :"r"(value));
}

u32 PJ4_Read_PMNCNT(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 2" : "=r"(v));
	return v;
}

void PJ4_Write_PMNCNT(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 2" : :"r"(value));
}

u32 PJ4_Read_USEREN(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 0" : "=r"(v));
	return v;
}

void PJ4_Write_USEREN(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 0" : :"r"(value));
}

u32 PJ4_Read_INTENS(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 1" : "=r"(v));
	return v;
}

void PJ4_Write_INTENS(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 1" : :"r"(value));
}

u32 PJ4_Read_INTENC(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 2" : "=r"(v));
	return v;
}

void PJ4_Write_INTENC(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 2" : :"r"(value));
}

u32 PJ4_ReadPMUCounter(int counterId)
{
	u32 value;

	value = 0;
	switch(counterId)
	{
		case COUNTER_PJ4_PMU_CCNT:
			value = PJ4_Read_CCNT();
			break;

		case COUNTER_PJ4_PMU_PMN0:
			PJ4_Write_PMNXSEL(0x0);
			value = PJ4_Read_PMNCNT();
			break;

		case COUNTER_PJ4_PMU_PMN1:
			PJ4_Write_PMNXSEL(0x1);
			value = PJ4_Read_PMNCNT();
			break;

		case COUNTER_PJ4_PMU_PMN2:
			PJ4_Write_PMNXSEL(0x2);
			value = PJ4_Read_PMNCNT();
			break;

		case COUNTER_PJ4_PMU_PMN3:
			PJ4_Write_PMNXSEL(0x3);
			value = PJ4_Read_PMNCNT();
			break;

		default:
			break;
	}

	return value;
}

bool PJ4_WritePMUCounter(int counterId, u32 value)
{
	switch(counterId)
	{
		case COUNTER_PJ4_PMU_CCNT:
			PJ4_Write_CCNT(value);
			return true;

		case COUNTER_PJ4_PMU_PMN0:
			PJ4_Write_PMNXSEL(0x0);
			PJ4_Write_PMNCNT(value);
			return true;

		case COUNTER_PJ4_PMU_PMN1:
			PJ4_Write_PMNXSEL(0x1);
			PJ4_Write_PMNCNT(value);
			return true;

		case COUNTER_PJ4_PMU_PMN2:
			PJ4_Write_PMNXSEL(0x2);
			PJ4_Write_PMNCNT(value);
			return true;

		case COUNTER_PJ4_PMU_PMN3:
			PJ4_Write_PMNXSEL(0x3);
			PJ4_Write_PMNCNT(value);
			return true;

		default:
			return false;
	}
}

int get_pmu_resource(struct resource ** pmu_resource, unsigned int * pmu_resource_num)
{
#ifdef PX_SOC_ARMADA610
	*pmu_resource      = pmu_resource_mmp2;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_mmp2);
	
	return 0;
#endif

#ifdef PX_SOC_ARMADA510
	*pmu_resource      = pmu_resource_dove;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_dove);
	
	return 0;
#endif

#ifdef PX_SOC_PXA955
	*pmu_resource      = pmu_resource_mg1;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_mg1);
	
	return 0;
#endif

	return 0;
}

int set_pmu_resource(int core_num, unsigned int * pmu_irq)
{
	int i;
	struct resource * pmu_resource;
	
#ifdef PX_SOC_ARMADA610
	pmu_resource = pmu_resource_mmp2;
#endif

#ifdef PX_SOC_ARMADA510
	pmu_resource = pmu_resource_dove;
#endif

#ifdef PX_SOC_PXA955
	pmu_resource = pmu_resource_mg1;
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
	int i;

	printk("[CPA] pmnc = 0x%x\n", PJ4_Read_PMNC());
	printk("[CPA] ccnt = 0x%x\n", PJ4_Read_CCNT());

	for (i=0; i<PJ4_PMN_NUM; i++)
	{
		PJ4_Write_PMNXSEL(i);
		printk("[CPA] pmncnt[%d] = 0x%x\n", i, PJ4_Read_PMNCNT());
		printk("[CPA] evtsel[%d] = 0x%x\n", i, PJ4_Read_EVTSEL());
	}

	printk("[CPA] cntens = 0x%x\n", PJ4_Read_CNTENS());
	printk("[CPA] cntenc = 0x%x\n", PJ4_Read_CNTENC());
	printk("[CPA] intens = 0x%x\n", PJ4_Read_INTENS());
	printk("[CPA] intenc = 0x%x\n", PJ4_Read_INTENC());
	printk("[CPA] flag = 0x%x\n", PJ4_Read_FLAG());
}


