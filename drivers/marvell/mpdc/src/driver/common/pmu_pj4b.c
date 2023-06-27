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

Module Name:  $Workfile: pmu_pj4b.c $

Abstract:
 This file is for pj4b PMU access function

Notes:

--*/
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/device.h>

#include "constants.h"
#include "pmu_pj4b.h"
#include "RegisterId_pj4b.h"

#ifdef PX_SOC_MMP3
#ifdef CONFIG_ANDROID
struct resource pmu_resource_mmp3[] = {
        {
                .start  = IRQ_MMP3_PMU_INT0,
                .end    = IRQ_MMP3_PMU_INT0,
                .flags  = IORESOURCE_IRQ,
        },

        {
                .start  = IRQ_MMP3_PMU_INT1,
                .end    = IRQ_MMP3_PMU_INT1,
                .flags  = IORESOURCE_IRQ,
        },

        {
                .start  = IRQ_MMP3_PMU_INT2,
                .end    = IRQ_MMP3_PMU_INT2,
                .flags  = IORESOURCE_IRQ,
        },
};

#else /* CONFIG_ANDROID */

static struct resource pmu_resource_mmp3[] = {
#ifdef CONFIG_ARM_GIC
        [0] = {
                .start          = IRQ_AP_PMU_CPU0,
                .end            = IRQ_AP_PMU_CPU0,
                .flags          = IORESOURCE_IRQ,
        },
        [1] = {
                .start          = IRQ_AP_PMU_CPU1,
                .end            = IRQ_AP_PMU_CPU1,
                .flags          = IORESOURCE_IRQ,
        },
        [2] = {
                .start          = IRQ_AP_PMU_CPU2,
                .end            = IRQ_AP_PMU_CPU2,
                .flags          = IORESOURCE_IRQ,
        },
        [3] = {
                .start          = IRQ_AP_PMU_CPU3,
                .end            = IRQ_AP_PMU_CPU3,
                .flags          = IORESOURCE_IRQ,
        },
#else
        [0] = {
                .start          = IRQ_AP_PMU,
                .end            = IRQ_AP_PMU,
                .flags          = IORESOURCE_IRQ,
        },
#endif
};
#endif /* CONFIG_ANDROID */

#endif /* PX_SOC_MMP3 */

#ifdef PX_SOC_DISCOSMP
struct resource pmu_resource_bg2[] = {
        [0] = {
                .start  = IRQ_AURORA_MP,
                .end    = IRQ_AURORA_MP,
                .flags  = IORESOURCE_IRQ,
        },
};
#endif /* PX_SOC_DISCOSMP */

#ifdef PX_SOC_ARMADAXP
struct resource pmu_resource_armadaxp[] = {
        [0] = {
                .start  = IRQ_AURORA_MP,
                .end    = IRQ_AURORA_MP,
                .flags  = IORESOURCE_IRQ,
        },
};
#endif /* PX_SOC_ARMADAXP */

#ifdef PX_SOC_BG2
struct resource pmu_resource_bg2[] = {
        [0] = {
                .start  = 30+32,              /* copy from BSP code */
                .end    = 30+32,              /* copy from BSP code */
                .flags  = IORESOURCE_IRQ,
        },
        [1] = {
                .start  = 31+32,              /* copy from BSP code */
                .end    = 31+32,              /* copy from BSP code */
                .flags  = IORESOURCE_IRQ,
        },
};
#endif /* PX_SOC_BG2 */


u32 PJ4B_Read_PMNC(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 0" : "=r"(v));
	return v;
}

void PJ4B_Write_PMNC(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 0" : :"r"(value));
}

u32 PJ4B_Read_CNTENSET(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 1" : "=r"(v));
	return v;
}

void PJ4B_Write_CNTENSET(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 1" : :"r"(value));
}


u32 PJ4B_Read_CNTENCLR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 2" : "=r"(v));
	return v;
}

void PJ4B_Write_CNTENCLR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 2" : :"r"(value));
}

u32 PJ4B_Read_OVSR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 3" : "=r"(v));
	return v;
}

void PJ4B_Write_OVSR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 3" : :"r"(value));
}

u32 PJ4B_Read_SWINC(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 4" : "=r"(v));
	return v;
}

void PJ4B_Write_SWINC(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 4" : :"r"(value));
}

u32 PJ4B_Read_PMNXSEL(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c12, 5" : "=r"(v));
	return v;
}

void PJ4B_Write_PMNXSEL(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c12, 5" : :"r"(value));
}

u32 PJ4B_Read_CCNT(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 0" : "=r"(v));
	return v;
}

void PJ4B_Write_CCNT(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 0" : :"r"(value));
}

u32 PJ4B_Read_EVTSEL(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 1" : "=r"(v));
	return v;
}

void PJ4B_Write_EVTSEL(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 1" : :"r"(value));
}

u32 PJ4B_Read_PMNCNT(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c13, 2" : "=r"(v));
	return v;
}

void PJ4B_Write_PMNCNT(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c13, 2" : :"r"(value));
}

u32 PJ4B_Read_USEREN(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 0" : "=r"(v));
	return v;
}

void PJ4B_Write_USEREN(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 0" : :"r"(value));
}

u32 PJ4B_Read_INTENSET(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 1" : "=r"(v));
	return v;
}

void PJ4B_Write_INTENSET(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 1" : :"r"(value));
}

u32 PJ4B_Read_INTENCLR(void)
{
	u32 v = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c9, c14, 2" : "=r"(v));
	return v;
}

void PJ4B_Write_INTENCLR(u32 value)
{
	__asm__ __volatile__ ("mcr  p15, 0, %0, c9, c14, 2" : :"r"(value));
}

u32 PJ4B_ReadPMUCounter(int counterId)
{
	u32 value;

	value = 0;
	switch(counterId)
	{
		case COUNTER_PJ4B_PMU_CCNT:
			value = PJ4B_Read_CCNT();
			break;

		case COUNTER_PJ4B_PMU_PMN0:
			PJ4B_Write_PMNXSEL(0x0);
			value = PJ4B_Read_PMNCNT();
			break;

		case COUNTER_PJ4B_PMU_PMN1:
			PJ4B_Write_PMNXSEL(0x1);
			value = PJ4B_Read_PMNCNT();
			break;

		case COUNTER_PJ4B_PMU_PMN2:
			PJ4B_Write_PMNXSEL(0x2);
			value = PJ4B_Read_PMNCNT();
			break;

		case COUNTER_PJ4B_PMU_PMN3:
			PJ4B_Write_PMNXSEL(0x3);
			value = PJ4B_Read_PMNCNT();
			break;

		case COUNTER_PJ4B_PMU_PMN4:
			PJ4B_Write_PMNXSEL(0x4);
			value = PJ4B_Read_PMNCNT();
			break;

		case COUNTER_PJ4B_PMU_PMN5:
			PJ4B_Write_PMNXSEL(0x5);
			value = PJ4B_Read_PMNCNT();
			break;

		default:
			break;
	}

	return value;
}

bool PJ4B_WritePMUCounter(int counterId, u32 value)
{
	switch(counterId)
	{
		case COUNTER_PJ4B_PMU_CCNT:
			PJ4B_Write_CCNT(value);
			return true;

		case COUNTER_PJ4B_PMU_PMN0:
			PJ4B_Write_PMNXSEL(0x0);
			PJ4B_Write_PMNCNT(value);
			return true;

		case COUNTER_PJ4B_PMU_PMN1:
			PJ4B_Write_PMNXSEL(0x1);
			PJ4B_Write_PMNCNT(value);
			return true;

		case COUNTER_PJ4B_PMU_PMN2:
			PJ4B_Write_PMNXSEL(0x2);
			PJ4B_Write_PMNCNT(value);
			return true;

		case COUNTER_PJ4B_PMU_PMN3:
			PJ4B_Write_PMNXSEL(0x3);
			PJ4B_Write_PMNCNT(value);
			return true;

		case COUNTER_PJ4B_PMU_PMN4:
			PJ4B_Write_PMNXSEL(0x4);
			PJ4B_Write_PMNCNT(value);
			return true;

		case COUNTER_PJ4B_PMU_PMN5:
			PJ4B_Write_PMNXSEL(0x5);
			PJ4B_Write_PMNCNT(value);
			return true;

		default:
			return false;
	}
}

bool is_neon_event(unsigned int user_event_id, int * neon_group_id)
{
	int zone;
	int group_id;

	zone = (user_event_id & ZONE_MASK);

	switch (zone)
	{
	case PJ4B_EVENT_ZONE_PMU_NEON_BP_CP_INTERFACE:
		group_id = 0;
		break;

	case PJ4B_EVENT_ZONE_PMU_NEON_ISSUE_STAGE:
		group_id = 1;
		break;

	case PJ4B_EVENT_ZONE_PMU_NEON_ISSUE_STALL:
		group_id = 2;
		break;

	case PJ4B_EVENT_ZONE_PMU_NEON_EU_CONFLICT_STALL:
		group_id = 3;
		break;

	case PJ4B_EVENT_ZONE_PMU_NEON_DATA_HAZARD_STALL:
		group_id = 4;
		break;

	case PJ4B_EVENT_ZONE_PMU_NEON_ROB:
		group_id = 5;
		break;

	default:
		return false;
	}

	if (neon_group_id != NULL)
		*neon_group_id = group_id;

	return true;
}

unsigned int get_pmu_event_id(unsigned int user_event_id)
{
	return user_event_id & EVENT_ID_MASK;
}

void enable_neon_event_group(int neon_group_id)
{
	/*
	 * The following instruction is equal to
	 *           VMSR    r0, fpinst2
	 * We use MCR instead of VMSR because some compilers do not support VMSR instruction
	 */
	__asm__ __volatile__ ("mcr  p10, 7, %0, cr10, cr0, 0" : :"r"(neon_group_id));
}

int get_pmu_resource(struct resource ** pmu_resource, unsigned int * pmu_resource_num)
{
#ifdef PX_SOC_MMP3
	*pmu_resource      = pmu_resource_mmp3;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_mmp3);
	
	return 0;
#endif

#ifdef PX_SOC_DISCOSMP
	return 0;
#endif

/*
#ifdef PX_SOC_ARMADAXP
	return 0;
#endif
*/

#ifdef PX_SOC_ARMADAXP
	*pmu_resource      = pmu_resource_armadaxp;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_armadaxp);
	return 0;
#endif

#ifdef PX_SOC_BG2
	*pmu_resource      = pmu_resource_bg2;
	*pmu_resource_num  = ARRAY_SIZE(pmu_resource_bg2);
	
	return 0;
#endif

	return 0;
}

int set_pmu_resource(int core_num, unsigned int * pmu_irq)
{
	int i;
	struct resource * pmu_resource;
	
#ifdef PX_SOC_MMP3
	pmu_resource = pmu_resource_mmp3;
#endif

#ifdef PX_SOC_DISCOSMP
	return -EINVAL;
#endif

/*
#ifdef PX_SOC_ARMADAXP
	return -EINVAL;
#endif
*/

#ifdef PX_SOC_ARMADAXP
	pmu_resource = pmu_resource_armadaxp;
#endif

#ifdef PX_SOC_BG2
	pmu_resource = pmu_resource_bg2;
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

void display_pmu_registers_for_cpu(void)
{
	int i;
	int cpu;

	cpu = smp_processor_id();

	printk("[CPA] CPU%d: pmcr = 0x%x\n", cpu, PJ4B_Read_PMNC());
	printk("[CPA] CPU%d: ccnt = 0x%x\n", cpu, PJ4B_Read_CCNT());

	for (i=0; i<6; i++)
	{
		PJ4B_Write_PMNXSEL(i);
		printk("[CPA] CPU%d: pmncnt[%d] = 0x%x\n", cpu, i, PJ4B_Read_PMNCNT());
		printk("[CPA] CPU%d: evtsel[%d] = 0x%x\n", cpu, i, PJ4B_Read_EVTSEL());
	}

	printk("[CPA] CPU%d: cntenset = 0x%x\n", cpu, PJ4B_Read_CNTENSET());
	printk("[CPA] CPU%d: cntenclr = 0x%x\n", cpu, PJ4B_Read_CNTENCLR());
	printk("[CPA] CPU%d: intenset = 0x%x\n", cpu, PJ4B_Read_INTENSET());
	printk("[CPA] CPU%d: intenclr = 0x%x\n", cpu, PJ4B_Read_INTENCLR());
	printk("[CPA] CPU%d: ovsr = 0x%x\n", cpu, PJ4B_Read_OVSR());
	printk("[CPA] CPU%d: useren = 0x%x\n", cpu, PJ4B_Read_USEREN());
}

void display_pmu_registers(void)
{
	display_pmu_registers_for_cpu();
}



