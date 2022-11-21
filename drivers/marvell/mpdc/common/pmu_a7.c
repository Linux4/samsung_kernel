/*
** (C) Copyright 2011 Marvell International Ltd.
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

Module Name:  $Workfile: pmu_a7.c $

Abstract:
 This file is for a7 PMU access function

Notes:

--*/
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/device.h>

#include "constants.h"
#include "pmu_a7.h"
#include "RegisterId_a7.h"

#ifdef PX_SOC_PXA1088
struct resource pmu_resource_pxa1088[] = {
        {
                .start  = IRQ_PXA988_CORESIGHT,
                .end    = IRQ_PXA988_CORESIGHT,
                .flags  = IORESOURCE_IRQ,
        },

        {
                .start  = IRQ_PXA988_CORESIGHT2,
                .end    = IRQ_PXA988_CORESIGHT2,
                .flags  = IORESOURCE_IRQ,
        },

#if defined(CONFIG_CPU_PXA1088)
        /* core2 */
        {
                .start  = IRQ_PXA1088_CORESIGHT3,
                .end    = IRQ_PXA1088_CORESIGHT3,
                .flags  = IORESOURCE_IRQ,
        },
        /* core3 */
        {
                .start  = IRQ_PXA1088_CORESIGHT4,
                .end    = IRQ_PXA1088_CORESIGHT4,
                .flags  = IORESOURCE_IRQ,
        },
#endif
};
#endif


#ifdef PX_SOC_PXAEDEN
struct resource pmu_resource_pxaeden[] = {
        /* multicore_npmuirq[0] */
        {
                .start  = IRQ_EDEN_MULTICORE_NPMUIRQ0,
                .end    = IRQ_EDEN_MULTICORE_NPMUIRQ0,
                .flags  = IORESOURCE_IRQ,
        },
        /* multicore_npmuirq[1] */
        {
                .start  = IRQ_EDEN_MULTICORE_NPMUIRQ1,
                .end    = IRQ_EDEN_MULTICORE_NPMUIRQ1,
                .flags  = IORESOURCE_IRQ,
        },
        /* multicore_npmuirq[2] */
        {
                .start  = IRQ_EDEN_MULTICORE_NPMUIRQ2,
                .end    = IRQ_EDEN_MULTICORE_NPMUIRQ2,
                .flags  = IORESOURCE_IRQ,
        },
        /* multicore_npmuirq[3] */
        {
                .start  = IRQ_EDEN_MULTICORE_NPMUIRQ2,
                .end    = IRQ_EDEN_MULTICORE_NPMUIRQ2,
                .flags  = IORESOURCE_IRQ,
        },
};
#endif


u32 A7_Read_PMCR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 0" : "=r"(v));
	return v;
}

void A7_Write_PMCR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 0" : :"r"(value));
}

u32 A7_Read_CNTENSET(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 1" : "=r"(v));
	return v;
}

void A7_Write_CNTENSET(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 1" : :"r"(value));
}


u32 A7_Read_CNTENCLR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 2" : "=r"(v));
	return v;
}

void A7_Write_CNTENCLR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 2" : :"r"(value));
}

u32 A7_Read_OVSR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 3" : "=r"(v));
	return v;
}

void A7_Write_OVSR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 3" : :"r"(value));
}

u32 A7_Read_SWINC(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 4" : "=r"(v));
	return v;
}

void A7_Write_SWINC(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 4" : :"r"(value));
}

u32 A7_Read_PMNXSEL(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 5" : "=r"(v));
	return v;
}

void A7_Write_PMNXSEL(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 5" : :"r"(value));
}

u32 A7_Read_CCNT(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 0" : "=r"(v));
	return v;
}

void A7_Write_CCNT(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 0" : :"r"(value));
}

u32 A7_Read_EVTSEL(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 1" : "=r"(v));
	return v;
}

void A7_Write_EVTSEL(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 1" : :"r"(value));
}

u32 A7_Read_PMNCNT(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 2" : "=r"(v));
	return v;
}

void A7_Write_PMNCNT(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 2" : :"r"(value));
}

u32 A7_Read_USEREN(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 0" : "=r"(v));
	return v;
}

void A7_Write_USEREN(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 0" : :"r"(value));
}

u32 A7_Read_INTENSET(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 1" : "=r"(v));
	return v;
}

void A7_Write_INTENSET(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 1" : :"r"(value));
}

u32 A7_Read_INTENCLR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 2" : "=r"(v));
	return v;
}

void A7_Write_INTENCLR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 2" : :"r"(value));
}

u32 A7_ReadPMUCounter(int counterId)
{
	u32 value;

	value = 0;
	switch(counterId)
	{
		case COUNTER_A7_PMU_CCNT:
			value = A7_Read_CCNT();
			break;

		case COUNTER_A7_PMU_PMN0:
			A7_Write_PMNXSEL(0x0);
			value = A7_Read_PMNCNT();
			break;

		case COUNTER_A7_PMU_PMN1:
			A7_Write_PMNXSEL(0x1);
			value = A7_Read_PMNCNT();
			break;

		case COUNTER_A7_PMU_PMN2:
			A7_Write_PMNXSEL(0x2);
			value = A7_Read_PMNCNT();
			break;

		case COUNTER_A7_PMU_PMN3:
			A7_Write_PMNXSEL(0x3);
			value = A7_Read_PMNCNT();
			break;

		default:
			break;
	}

	return value;
}

bool A7_WritePMUCounter(int counterId, u32 value)
{
	switch(counterId)
	{
		case COUNTER_A7_PMU_CCNT:
			A7_Write_CCNT(value);
			return true;

		case COUNTER_A7_PMU_PMN0:
			A7_Write_PMNXSEL(0x0);
			A7_Write_PMNCNT(value);
			return true;

		case COUNTER_A7_PMU_PMN1:
			A7_Write_PMNXSEL(0x1);
			A7_Write_PMNCNT(value);
			return true;

		case COUNTER_A7_PMU_PMN2:
			A7_Write_PMNXSEL(0x2);
			A7_Write_PMNCNT(value);
			return true;

		case COUNTER_A7_PMU_PMN3:
			A7_Write_PMNXSEL(0x3);
			A7_Write_PMNCNT(value);
			return true;

		default:
			return false;
	}
}

unsigned int get_pmu_event_id(unsigned int user_event_id)
{
	return user_event_id & EVENT_ID_MASK;
}

int get_pmu_resource(struct resource ** pmu_resource, unsigned int * pmu_resource_num)
{

#ifdef PX_SOC_PXA1088
	*pmu_resource      = pmu_resource_pxa1088;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_pxa1088);

	return 0;
#endif

#ifdef PX_SOC_PXAEDEN
	*pmu_resource      = pmu_resource_pxaeden;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_pxaeden);

	return 0;
#endif

	return 0;
}

int set_pmu_resource(int core_num, unsigned int * pmu_irq)
{
        int i;
        struct resource * pmu_resource;

#ifdef PX_SOC_PXA1088
        pmu_resource = pmu_resource_pxa1088;
#endif

#ifdef PX_SOC_PXAEDEN
        pmu_resource = pmu_resource_pxaeden;
#endif

        //if (core_num != ARRAY_SIZE(pmu_resource))
        //      return -EINVAL;

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
	int cpu;

	cpu = smp_processor_id();

	printk("[CPA] CPU%d: pmcr = 0x%x\n", cpu, A7_Read_PMCR());
	printk("[CPA] CPU%d: ccnt = 0x%x\n", cpu, A7_Read_CCNT());

	for (i=0; i<6; i++)
	{
		A7_Write_PMNXSEL(i);
		printk("[CPA] CPU%d: pmncnt[%d] = 0x%x\n", cpu, i, A7_Read_PMNCNT());
		printk("[CPA] CPU%d: evtsel[%d] = 0x%x\n", cpu, i, A7_Read_EVTSEL());
	}

	printk("[CPA] CPU%d: cntenset = 0x%x\n", cpu, A7_Read_CNTENSET());
	printk("[CPA] CPU%d: cntenclr = 0x%x\n", cpu, A7_Read_CNTENCLR());
	printk("[CPA] CPU%d: intenset = 0x%x\n", cpu, A7_Read_INTENSET());
	printk("[CPA] CPU%d: intenclr = 0x%x\n", cpu, A7_Read_INTENCLR());
	printk("[CPA] CPU%d: ovsr = 0x%x\n", cpu, A7_Read_OVSR());
	printk("[CPA] CPU%d: useren = 0x%x\n", cpu, A7_Read_USEREN());
}


