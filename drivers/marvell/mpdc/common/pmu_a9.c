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

Module Name:  $Workfile: pmu_a9.c $

Abstract:
 This file is for a9 PMU access function

Notes:

--*/
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/device.h>

#include "constants.h"
#include "pmu_a9.h"
#include "RegisterId_a9.h"

#ifdef PX_SOC_TEGRA2
struct resource pmu_resource_tegra2[] = {
        {
                .start  = INT_CPU0_PMU_INTR,
                .end    = INT_CPU0_PMU_INTR,
                .flags  = IORESOURCE_IRQ,
        },

        {
                .start  = INT_CPU2_PMU_INTR,
                .end    = INT_CPU2_PMU_INTR,
                .flags  = IORESOURCE_IRQ,
        },
};
#endif

#ifdef PX_SOC_PXA988
struct resource pmu_resource_pxa988[] = {
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
};
#endif

#ifdef PX_SOC_OMAP
struct resource pmu_resource_omap44xx[] = {
        {
                .start  = INT_34XX_BENCH_MPU_EMUL,
                .end    = INT_34XX_BENCH_MPU_EMUL,
                .flags  = IORESOURCE_IRQ,
        },

        {
                .start  = INT_34XX_BENCH_MPU_EMUL,
                .end    = INT_34XX_BENCH_MPU_EMUL,
                .flags  = IORESOURCE_IRQ,
        },
};
#endif

#ifdef PX_SOC_NEVO
struct resource pmu_resource_nevo[] = {
        {
                .start  = IRQ_PMU,
                .end    = IRQ_PMU,
                .flags  = IORESOURCE_IRQ,
        },
};
#endif


u32 A9_Read_PMCR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 0" : "=r"(v));
	return v;
}

void A9_Write_PMCR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 0" : :"r"(value));
}

u32 A9_Read_CNTENSET(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 1" : "=r"(v));
	return v;
}

void A9_Write_CNTENSET(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 1" : :"r"(value));
}


u32 A9_Read_CNTENCLR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 2" : "=r"(v));
	return v;
}

void A9_Write_CNTENCLR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 2" : :"r"(value));
}

u32 A9_Read_OVSR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 3" : "=r"(v));
	return v;
}

void A9_Write_OVSR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 3" : :"r"(value));
}

u32 A9_Read_SWINC(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 4" : "=r"(v));
	return v;
}

void A9_Write_SWINC(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 4" : :"r"(value));
}

u32 A9_Read_PMNXSEL(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 5" : "=r"(v));
	return v;
}

void A9_Write_PMNXSEL(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 5" : :"r"(value));
}

u32 A9_Read_CCNT(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 0" : "=r"(v));
	return v;
}

void A9_Write_CCNT(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 0" : :"r"(value));
}

u32 A9_Read_EVTSEL(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 1" : "=r"(v));
	return v;
}

void A9_Write_EVTSEL(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 1" : :"r"(value));
}

u32 A9_Read_PMNCNT(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 2" : "=r"(v));
	return v;
}

void A9_Write_PMNCNT(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 2" : :"r"(value));
}

u32 A9_Read_USEREN(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 0" : "=r"(v));
	return v;
}

void A9_Write_USEREN(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 0" : :"r"(value));
}

u32 A9_Read_INTENSET(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 1" : "=r"(v));
	return v;
}

void A9_Write_INTENSET(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 1" : :"r"(value));
}

u32 A9_Read_INTENCLR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 2" : "=r"(v));
	return v;
}

void A9_Write_INTENCLR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 2" : :"r"(value));
}

u32 A9_ReadPMUCounter(int counterId)
{
	u32 value;

	value = 0;
	switch(counterId)
	{
		case COUNTER_A9_PMU_CCNT:
			value = A9_Read_CCNT();
			break;

		case COUNTER_A9_PMU_PMN0:
			A9_Write_PMNXSEL(0x0);
			value = A9_Read_PMNCNT();
			break;

		case COUNTER_A9_PMU_PMN1:
			A9_Write_PMNXSEL(0x1);
			value = A9_Read_PMNCNT();
			break;

		case COUNTER_A9_PMU_PMN2:
			A9_Write_PMNXSEL(0x2);
			value = A9_Read_PMNCNT();
			break;

		case COUNTER_A9_PMU_PMN3:
			A9_Write_PMNXSEL(0x3);
			value = A9_Read_PMNCNT();
			break;

		case COUNTER_A9_PMU_PMN4:
			A9_Write_PMNXSEL(0x4);
			value = A9_Read_PMNCNT();
			break;

		case COUNTER_A9_PMU_PMN5:
			A9_Write_PMNXSEL(0x5);
			value = A9_Read_PMNCNT();
			break;

		default:
			break;
	}

	return value;
}

bool A9_WritePMUCounter(int counterId, u32 value)
{
	switch(counterId)
	{
		case COUNTER_A9_PMU_CCNT:
			A9_Write_CCNT(value);
			return true;

		case COUNTER_A9_PMU_PMN0:
			A9_Write_PMNXSEL(0x0);
			A9_Write_PMNCNT(value);
			return true;

		case COUNTER_A9_PMU_PMN1:
			A9_Write_PMNXSEL(0x1);
			A9_Write_PMNCNT(value);
			return true;

		case COUNTER_A9_PMU_PMN2:
			A9_Write_PMNXSEL(0x2);
			A9_Write_PMNCNT(value);
			return true;

		case COUNTER_A9_PMU_PMN3:
			A9_Write_PMNXSEL(0x3);
			A9_Write_PMNCNT(value);
			return true;

		case COUNTER_A9_PMU_PMN4:
			A9_Write_PMNXSEL(0x4);
			A9_Write_PMNCNT(value);
			return true;

		case COUNTER_A9_PMU_PMN5:
			A9_Write_PMNXSEL(0x5);
			A9_Write_PMNCNT(value);
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

#ifdef PX_SOC_PXA988
	*pmu_resource      = pmu_resource_pxa988;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_pxa988);

	return 0;
#endif

#ifdef PX_SOC_OMAP
	*pmu_resource      = pmu_resource_omap44xx;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_omap44xx);
	
	return 0;
#endif

#ifdef PX_SOC_TEGRA2
	*pmu_resource      = pmu_resource_tegra2;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_tegra2);
	
	return 0;
#endif

#ifdef PX_SOC_NEVO
	*pmu_resource      = pmu_resource_nevo;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_nevo);

	return 0;
#endif

	return 0;
}

int set_pmu_resource(int core_num, unsigned int * pmu_irq)
{
        int i;
        struct resource * pmu_resource;

#ifdef PX_SOC_PXA988
        pmu_resource = pmu_resource_pxa988;
#endif

#ifdef PX_SOC_OMAP
        pmu_resource = pmu_resource_omap44xx;
#endif

#ifdef PX_SOC_TEGRA2
        pmu_resource = pmu_resource_tegra2;
#endif

#ifdef PX_SOC_NEVO
        pmu_resource = pmu_resource_nevo;
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

	printk("[CPA] CPU%d: pmcr = 0x%x\n", cpu, A9_Read_PMCR());
	printk("[CPA] CPU%d: ccnt = 0x%x\n", cpu, A9_Read_CCNT());

	for (i=0; i<6; i++)
	{
		A9_Write_PMNXSEL(i);
		printk("[CPA] CPU%d: pmncnt[%d] = 0x%x\n", cpu, i, A9_Read_PMNCNT());
		printk("[CPA] CPU%d: evtsel[%d] = 0x%x\n", cpu, i, A9_Read_EVTSEL());
	}

	printk("[CPA] CPU%d: cntenset = 0x%x\n", cpu, A9_Read_CNTENSET());
	printk("[CPA] CPU%d: cntenclr = 0x%x\n", cpu, A9_Read_CNTENCLR());
	printk("[CPA] CPU%d: intenset = 0x%x\n", cpu, A9_Read_INTENSET());
	printk("[CPA] CPU%d: intenclr = 0x%x\n", cpu, A9_Read_INTENCLR());
	printk("[CPA] CPU%d: ovsr = 0x%x\n", cpu, A9_Read_OVSR());
	printk("[CPA] CPU%d: useren = 0x%x\n", cpu, A9_Read_USEREN());
}


