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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <asm/pmu.h>

#include "pmu_pj4b.h"
#include "CMProfilerDef.h"
#include "CMProfilerSettings.h"
#include "EventTypes_pj4b.h"
#include "RegisterId_pj4b.h"
#include "cm_drv.h"
#include "common.h"

//static struct platform_device * pmu_dev;

extern struct CMCounterConfigs g_counter_config;

static unsigned int ** interrupt_count = NULL;
static u64  ** register_value = NULL;

struct counter_settings
{
	bool enabled;   /* if the counter is enabled */
	int  event_id;  /* the event id set to this counter */
};

static struct counter_settings cs[COUNTER_PJ4B_MAX_ID];

static int get_pmn_index_from_counter_id(int counter_id)
{
	switch (counter_id)
	{
	case COUNTER_PJ4B_PMU_PMN0:
		return 0;

	case COUNTER_PJ4B_PMU_PMN1:
		return 1;

	case COUNTER_PJ4B_PMU_PMN2:
		return 2;

	case COUNTER_PJ4B_PMU_PMN3:
		return 3;

	case COUNTER_PJ4B_PMU_PMN4:
		return 4;

	case COUNTER_PJ4B_PMU_PMN5:
		return 5;

	default:
		return -1;
	}
}

static int get_counter_id_from_pmn_index(int index)
{
	switch (index)
	{
	case 0:
		return COUNTER_PJ4B_PMU_PMN0;
	case 1:
		return COUNTER_PJ4B_PMU_PMN1;
	case 2:
		return COUNTER_PJ4B_PMU_PMN2;
	case 3:
		return COUNTER_PJ4B_PMU_PMN3;
	case 4:
		return COUNTER_PJ4B_PMU_PMN4;
	case 5:
		return COUNTER_PJ4B_PMU_PMN5;
	}

	return COUNTER_PJ4B_UNKNOWN;
}

/* read CCNT value for current CPU */
static unsigned long long read_ccnt_counter(void)
{
	unsigned int cpu;
	unsigned long long value;

	cpu = smp_processor_id();

	value = ((unsigned long long)interrupt_count[COUNTER_PJ4B_PMU_CCNT][cpu] << 32) +  PJ4B_Read_CCNT();

	return value;
}

#if 0
static void read_ccnt_counter_per_cpu(void * data)
{
	unsigned long long value;
	unsigned long long * p;
	unsigned int cpu;

	cpu = smp_processor_id();

	p = (unsigned long long *)data;

	value = read_ccnt_counter();

	p[cpu] = value;
}

/*
 * read CCNT value for all CPUs
 * parameter:
 *             value: the array which contains counter value for each CPU
 */
static void read_ccnt_counter_on_all_cpus(unsigned long long * value)
{
	on_each_cpu(read_ccnt_counter_per_cpu, value, 1);
}
#endif

/* read PMN[i] value for current CPU */
static unsigned long long read_pmn_counter(unsigned int counter_id)
{
	unsigned int cpu;
	unsigned int pmn_index;
	unsigned long long value;

	cpu = smp_processor_id();

	pmn_index = get_pmn_index_from_counter_id(counter_id);

	if (pmn_index == -1)
	{
		PX_BUG();
		return -1;
	}

	PJ4B_Write_PMNXSEL(pmn_index);

	value = ((unsigned long long)interrupt_count[counter_id][cpu] << 32) +  PJ4B_Read_PMNCNT();

	return value;
}

/*
static void read_pmn_counter_per_cpu(void * data)
{
	unsigned long long value;
	struct rcop * rp = (struct rcop *)data;
	unsigned int cpu;

	cpu = smp_processor_id();

	value = read_pmn_counter(rp->cid);

	rp->value[cpu] = value;
}

static void read_pmn_counter_on_all_cpus(unsigned int counter_id, unsigned long long * value)
{
	int i;
	struct rcop rc;

	rc.value = value;
	rc.cid = counter_id;

	on_each_cpu(read_pmn_counter_per_cpu, &rc, 1);

	for (i=0; i<num_possible_cpus(); i++)
	{
		value[i] = rc.value[i];
	}
}
*/

static unsigned long long read_os_timer_counter(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	return (tv.tv_usec + (USEC_PER_SEC * (unsigned long long)tv.tv_sec));
}

#if 0
static void read_os_timer_counter_on_all_cpus(unsigned long long * value)
{
	int i;
	unsigned long long count64;

	count64  = read_os_timer_counter();

	for (i=0; i<num_possible_cpus(); i++)
	{
		value[i] = count64;
	}
}
#endif

/*
 * read counter value on current CPU
 * parameter:
 *            counter_id: which counter to read
 *            value:      address of the variable to receive the counter value
 */
static bool read_counter_on_this_cpu_pj4b(int counter_id, unsigned long long * value)
{
	unsigned long long count64;

	switch(counter_id)
	{
		case COUNTER_PJ4B_OS_TIMER:
			count64 = read_os_timer_counter();
			break;

		case COUNTER_PJ4B_PMU_CCNT:
			count64 = read_ccnt_counter();
			break;

		case COUNTER_PJ4B_PMU_PMN0:
		case COUNTER_PJ4B_PMU_PMN1:
		case COUNTER_PJ4B_PMU_PMN2:
		case COUNTER_PJ4B_PMU_PMN3:
		case COUNTER_PJ4B_PMU_PMN4:
		case COUNTER_PJ4B_PMU_PMN5:
			count64 = read_pmn_counter(counter_id);
			break;

		default:
			return false;
	}

	if (value != NULL)
		*value = count64;

	return true;
}

#if 0
/*
 * read counter value on each CPU
 * parameter:
 *            counter_id: which counter to read
 *            value:      array which contains counter values for each CPU
 */
static bool read_counter_on_all_cpus_pj4b(int counter_id, unsigned long long * value)
{
	switch(counter_id)
	{
		case COUNTER_PJ4B_OS_TIMER:
			read_os_timer_counter_on_all_cpus(value);
			break;

		case COUNTER_PJ4B_PMU_CCNT:
			read_ccnt_counter_on_all_cpus(value);
			break;

		case COUNTER_PJ4B_PMU_PMN0:
		case COUNTER_PJ4B_PMU_PMN1:
		case COUNTER_PJ4B_PMU_PMN2:
		case COUNTER_PJ4B_PMU_PMN3:
		case COUNTER_PJ4B_PMU_PMN4:
		case COUNTER_PJ4B_PMU_PMN5:
			read_pmn_counter_on_all_cpus(counter_id, value);
			break;

		default:
			return false;
	}

	return true;
}
#endif

static irqreturn_t cm_pmu_isr_pj4b(int irq, void * dev)
{
	u32 pmnc_value;
	u32 flag_value;
	int cpu;

	cpu = smp_processor_id();

	/* Disable counters */
	pmnc_value = PJ4B_Read_PMNC();
	pmnc_value &= ~0x1;
	PJ4B_Write_PMNC(pmnc_value);

	/* Save current OVERFLOW FLAG value */
	flag_value = PJ4B_Read_OVSR();

	/* Clear overflow flags */
	PJ4B_Write_OVSR(0xffffffff);

	if (flag_value & 0x80000000)
	{
		/* CCNT is overflow */
		interrupt_count[COUNTER_PJ4B_PMU_CCNT][cpu]++;
	}

	if (flag_value & 0x00000001)
	{
		/* PMN0 is overflow */
		interrupt_count[COUNTER_PJ4B_PMU_PMN0][cpu]++;
	}

	if (flag_value & 0x00000002)
	{
		/* PMN1 is overflow */
		interrupt_count[COUNTER_PJ4B_PMU_PMN1][cpu]++;
	}

	if (flag_value & 0x00000004)
	{
		/* PMN2 is overflow */
		interrupt_count[COUNTER_PJ4B_PMU_PMN2][cpu]++;
	}

	if (flag_value & 0x00000008)
	{
		/* PMN3 is overflow */
		interrupt_count[COUNTER_PJ4B_PMU_PMN3][cpu]++;
	}

	if (flag_value & 0x00000010)
	{
		/* PMN4 is overflow */
		interrupt_count[COUNTER_PJ4B_PMU_PMN4][cpu]++;
	}

	if (flag_value & 0x00000020)
	{
		/* PMN5 is overflow */
		interrupt_count[COUNTER_PJ4B_PMU_PMN5][cpu]++;
	}

	/* re-enable all counters */
	pmnc_value |= 0x1;
	PJ4B_Write_PMNC(pmnc_value);

	return IRQ_HANDLED;
}

static int free_internal_data(void)
{
	int i;

	if (interrupt_count != NULL)
	{
		for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
		{
			kfree(interrupt_count[i]);
			interrupt_count[i] = NULL;
		}

		kfree(interrupt_count);
		interrupt_count = NULL;
	}

	if (register_value != NULL)
	{
		for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
		{
			kfree(register_value[i]);
			register_value[i] = NULL;
		}

		kfree(register_value);
		register_value = NULL;
	}


	return 0;
}

static int allocate_internal_data(void)
{
	unsigned int i;
	unsigned int cpu_num;

	cpu_num = num_possible_cpus();

	interrupt_count = kzalloc(COUNTER_PJ4B_MAX_ID * sizeof(unsigned int *), GFP_ATOMIC);

	if (interrupt_count == NULL)
		goto error;

	for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
	{
		interrupt_count[i] = kzalloc(cpu_num * sizeof(unsigned int), GFP_ATOMIC);

		if (interrupt_count[i] == NULL)
			goto error;
	}

	register_value = kzalloc(COUNTER_PJ4B_MAX_ID * sizeof(u64 *), GFP_ATOMIC);

	if (register_value == NULL)
		goto error;

	for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
	{
		register_value[i] = kzalloc(COUNTER_PJ4B_MAX_ID * sizeof(u64), GFP_ATOMIC);

		if (register_value[i] == NULL)
			goto error;
	}

	return 0;
error:
	free_internal_data();

	return -ENOMEM;
}

static void reset_all_counters(void)
{
	unsigned int i;
	unsigned int cpu;
	unsigned int cpu_num;

	cpu_num = num_possible_cpus();

	for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
	{
		for (cpu=0; cpu<cpu_num; cpu++)
		{
			interrupt_count[i][cpu] = 0;
			register_value[i][cpu] = 0;
		}
	}
}

static void setup_pmu_regs(struct pmu_registers_pj4b *pmu_regs)
{
	int i;

	if (pmu_regs == NULL)
		return;

	memset(pmu_regs, 0, sizeof(struct pmu_registers_pj4b));
	pmu_regs->pmnc = PJ4B_Read_PMNC();

	for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
	{
		unsigned int pmn_index = -1;

		unsigned int event_id;

		if (!cs[i].enabled)
			continue;

		event_id = cs[i].event_id;

		if (i == COUNTER_PJ4B_PMU_CCNT)
		{
			/* it is the CCNT counter */
			if (event_id == PJ4B_PMU_CCNT_CORE_CLOCK_TICK_64)
			{
				/* 64 clock cycle */
				pmu_regs->pmnc |= 0x8;
			}
			else if (event_id == PJ4B_PMU_CCNT_CORE_CLOCK_TICK)
			{
				pmu_regs->pmnc &= ~0x8;
			}

			pmu_regs->intenset |= 0x1 << 31;
			pmu_regs->cntenset |= 0x1 << 31;
			pmu_regs->ovsr   |= 0x1 << 31;

			pmu_regs->ccnt = 0;
		}
		else
		{
			int neon_group_id;

			/* it is the PMN counter */
			pmn_index = get_pmn_index_from_counter_id(i);

			if (pmn_index == -1)
				continue;

			pmu_regs->intenset |= (0x1 << pmn_index);
			pmu_regs->cntenset |= (0x1 << pmn_index);
			pmu_regs->ovsr     |= (0x1 << pmn_index);

			pmu_regs->evtsel[pmn_index] = get_pmu_event_id(event_id);

			pmu_regs->pmncnt[pmn_index] = 0;

			/* check if it is a neon event */
			if (is_neon_event(event_id, &neon_group_id))
			{
				enable_neon_event_group(neon_group_id);
			}

		}
	}
}

static void start_pmu_counters(void *data)
{
	int i;

	struct pmu_registers_pj4b pmu_regs;

	/* disable all PMU counters */
	pmu_regs.pmnc = PJ4B_Read_PMNC();
	pmu_regs.pmnc &= ~0x1;
	PJ4B_Write_PMNC(pmu_regs.pmnc);

	PJ4B_Write_CNTENCLR(0xffffffff);

	/* disable interrupts and clear the overflow flags */
	PJ4B_Write_INTENCLR(0xffffffff);
	PJ4B_Write_OVSR(0xffffffff);

	/* calculate the register values to be set */
	setup_pmu_regs(&pmu_regs);

	/* set value to pmu counters */
	PJ4B_WritePMUCounter(COUNTER_PJ4B_PMU_CCNT, pmu_regs.ccnt);
	PJ4B_WritePMUCounter(COUNTER_PJ4B_PMU_PMN0, pmu_regs.pmncnt[0]);
	PJ4B_WritePMUCounter(COUNTER_PJ4B_PMU_PMN1, pmu_regs.pmncnt[1]);
	PJ4B_WritePMUCounter(COUNTER_PJ4B_PMU_PMN2, pmu_regs.pmncnt[2]);
	PJ4B_WritePMUCounter(COUNTER_PJ4B_PMU_PMN3, pmu_regs.pmncnt[3]);
	PJ4B_WritePMUCounter(COUNTER_PJ4B_PMU_PMN4, pmu_regs.pmncnt[4]);
	PJ4B_WritePMUCounter(COUNTER_PJ4B_PMU_PMN5, pmu_regs.pmncnt[5]);

	/* set events */
	for (i=0; i<PJ4B_PMN_NUM; i++)
	{
		PJ4B_Write_PMNXSEL(i);
		PJ4B_Write_EVTSEL(pmu_regs.evtsel[i]);
	}

	/* enable the interrupts */
	PJ4B_Write_INTENSET(pmu_regs.intenset);

	/* enable the counters */
	PJ4B_Write_CNTENSET(pmu_regs.cntenset);

	/* Enable PMU */
	pmu_regs.pmnc |= 0x1;
	PJ4B_Write_PMNC(pmu_regs.pmnc);

//	return 0;
}

static void stop_pmu_counters(void *data)
{
	u32 intenc_value;
	u32 flag_value;
	u32 pmnc_value;

	intenc_value = 0xffffffff;
	flag_value   = 0xffffffff;

	/* disable all counters */
	pmnc_value = PJ4B_Read_PMNC();
	pmnc_value &= ~0x1;
	PJ4B_Write_PMNC(pmnc_value);

	/* clear overflow flags */
	PJ4B_Write_OVSR(flag_value);

	/* disable all interrupts */
	PJ4B_Write_INTENCLR(intenc_value);

	//return 0;
}

static void pause_pmu_counters(void *data)
{
	u32 pmnc_value;

	/* disable all counters */
	pmnc_value = PJ4B_Read_PMNC();
	pmnc_value &= ~0x1;
	PJ4B_Write_PMNC(pmnc_value);

	//return 0;
}

static void resume_pmu_counters(void *data)
{
	u32 pmnc_value;

	/* enable all counters */
	pmnc_value = PJ4B_Read_PMNC();
	pmnc_value |= 0x1;
	PJ4B_Write_PMNC(pmnc_value);

	//return 0;
}

static void config_all_counters(void)
{
	unsigned int i;
	unsigned int cid;
	unsigned int cevent;

	for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
	{
		cs[i].enabled  = false;
		cs[i].event_id = 0;
	}

	for (i=0; i<g_counter_config.number; i++)
	{
		cid    = g_counter_config.settings[i].cid;
		cevent = g_counter_config.settings[i].cevent;

		cs[cid].enabled  = true;
		cs[cid].event_id = cevent;
	}

}

static bool is_pmu_enabled(void)
{
	if (cs[COUNTER_PJ4B_PMU_CCNT].enabled)
		return true;

	if (cs[COUNTER_PJ4B_PMU_PMN0].enabled)
		return true;

	if (cs[COUNTER_PJ4B_PMU_PMN1].enabled)
		return true;

	if (cs[COUNTER_PJ4B_PMU_PMN2].enabled)
		return true;

	if (cs[COUNTER_PJ4B_PMU_PMN3].enabled)
		return true;

	if (cs[COUNTER_PJ4B_PMU_PMN4].enabled)
		return true;

	if (cs[COUNTER_PJ4B_PMU_PMN5].enabled)
		return true;

	return false;
}

static int start_counters_pj4b(void)
{
	int ret;

	free_internal_data();

	ret = allocate_internal_data();

	if (ret != 0)
	{
		return ret;
	}

	reset_all_counters();

	config_all_counters();

	if (is_pmu_enabled())
	{
		ret = allocate_pmu();
		if (ret != 0)
			return ret;

		ret = register_pmu_isr(cm_pmu_isr_pj4b);
		if (ret != 0)
			return ret;

		on_each_cpu(start_pmu_counters, NULL, 1);
	}

	return 0;
}

static int stop_counters_pj4b(void)
{
	int ret;

	if (is_pmu_enabled())
	{
		on_each_cpu(stop_pmu_counters, NULL, 1);

		unregister_pmu_isr();

		ret = free_pmu();

		if (ret != 0)
			return ret;
	}

	return 0;
}

static int pause_counters_pj4b(void)
{
	if (is_pmu_enabled())
	{
		on_each_cpu(pause_pmu_counters, NULL, 1);
	}

	return 0;
}

static int resume_counters_pj4b(void)
{
	if (is_pmu_enabled())
	{
		on_each_cpu(resume_pmu_counters, NULL, 1);
	}

	return 0;
}

struct cm_ctr_op_mach cm_op_pj4b = {
	.start        = start_counters_pj4b,
	.stop         = stop_counters_pj4b,
	.pause        = pause_counters_pj4b,
	.resume       = resume_counters_pj4b,
	//.read_counter_on_all_cpus = read_counter_on_all_cpus_pj4b,
	.read_counter_on_this_cpu = read_counter_on_this_cpu_pj4b,
	//.pmu_isr = pmu_isr_pj4b,
};
