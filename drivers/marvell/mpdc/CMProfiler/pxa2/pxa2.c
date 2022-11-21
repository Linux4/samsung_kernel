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

#include "pmu_pxa2.h"
#include "CMProfilerDef.h"
#include "CMProfilerSettings.h"
#include "EventTypes_pxa2.h"
#include "cm_drv.h"

extern unsigned int ReadPMUReg(int reg_id);
extern void WritePMUReg(int reg_id, unsigned int value);

extern struct CMCounterConfigs g_counter_config;

static unsigned int interrupt_count[MAX_CM_COUNTER_NUM];

struct counter_settings
{
	bool enabled;
	int  event_id;
};

static struct counter_settings cs[COUNTER_PXA2_MAX_ID];

void __iomem *pml_base = NULL;
#define PML_ESEL_REG(i) (void __iomem *)(pml_base + i * 4)

static unsigned long long read_ccnt_counter(void)
{
	unsigned long long value;

	value = ((unsigned long long)interrupt_count[COUNTER_PXA2_PMU_CCNT] << 32) +  ReadPMUReg(PXA2_PMU_CCNT);

	return value;
}

static unsigned long long read_pmn0_counter(void)
{
	unsigned long long value;

	value = ((unsigned long long)interrupt_count[COUNTER_PXA2_PMU_PMN0] << 32) +  ReadPMUReg(PXA2_PMU_PMN0);

	return value;
}

static unsigned long long read_pmn1_counter(void)
{
	unsigned long long value;

	value = ((unsigned long long)interrupt_count[COUNTER_PXA2_PMU_PMN1] << 32) +  ReadPMUReg(PXA2_PMU_PMN1);

	return value;
}

static unsigned long long read_pmn2_counter(void)
{
	unsigned long long value;

	value = ((unsigned long long)interrupt_count[COUNTER_PXA2_PMU_PMN2] << 32) +  ReadPMUReg(PXA2_PMU_PMN2);

	return value;
}

static unsigned long long read_pmn3_counter(void)
{
	unsigned long long value;

	value = ((unsigned long long)interrupt_count[COUNTER_PXA2_PMU_PMN3] << 32) +  ReadPMUReg(PXA2_PMU_PMN3);

	return value;
}

static unsigned long long read_os_timer_counter(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	return (tv.tv_usec + (USEC_PER_SEC * (unsigned long long)tv.tv_sec));
}

bool read_counter_pxa2(int counter_id, unsigned long long * value)
{
	unsigned long long  count64;

	count64 = 0;

	switch(counter_id)
	{
		case COUNTER_PXA2_OS_TIMER:
			count64  = read_os_timer_counter();
			break;

		case COUNTER_PXA2_PMU_CCNT:
			count64 = read_ccnt_counter();
			break;

		case COUNTER_PXA2_PMU_PMN0:
			count64 = read_pmn0_counter();
			break;

		case COUNTER_PXA2_PMU_PMN1:
			count64 = read_pmn1_counter();
			break;

		case COUNTER_PXA2_PMU_PMN2:
			count64 = read_pmn2_counter();
			break;

		case COUNTER_PXA2_PMU_PMN3:
			count64 = read_pmn3_counter();
			break;

		default:
			return false;
	}

	*value = count64;

	return true;
}

/* read counter value on current CPU */
static bool read_counter_on_this_cpu_pxa2(int counter_id, unsigned long long * value)
{
	return read_counter_pxa2(counter_id, value);
}

#if 0
/*
 * read counter value on each CPU
 * parameter:
 *            counter_id: which counter to read
 *            value:      array which contains counter values for each CPU
 */
static bool read_counter_on_all_cpus_pxa2(int counter_id, unsigned long long * value)
{
	bool ret;
	unsigned long long cv;

	if (value == NULL)
		return false;

	ret = read_counter_on_this_cpu_pxa2(counter_id, &cv);

	value[0] = cv;

	return ret;
}
#endif

static irqreturn_t pmu_isr_pxa2(int irq, void *dev_id)
{
	unsigned long flag_value;
	unsigned long pmnc_value;

	/* Disable counters */
	pmnc_value = ReadPMUReg(PXA2_PMU_PMNC);
	pmnc_value &= ~0x1;
	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	/* Save current FLAG value */
	flag_value = ReadPMUReg(PXA2_PMU_FLAG);

	/* Clear overflow flags */
	WritePMUReg(PXA2_PMU_FLAG, 0x1F);

	if (flag_value & CCNT_OVERFLAG_BIT)
	{
		/* CCNT is overflow */
		interrupt_count[COUNTER_PXA2_PMU_CCNT]++;
	}

	if (flag_value & PMN0_OVERFLAG_BIT)
	{
		/* PMN0 is overflow */
		interrupt_count[COUNTER_PXA2_PMU_PMN0]++;
	}

	if (flag_value & PMN1_OVERFLAG_BIT)
	{
		/* PMN1 is overflow */
		interrupt_count[COUNTER_PXA2_PMU_PMN1]++;
	}

	if (flag_value & PMN2_OVERFLAG_BIT)
	{
		/* PMN2 is overflow */
		interrupt_count[COUNTER_PXA2_PMU_PMN2]++;
	}

	if (flag_value & PMN3_OVERFLAG_BIT)
	{
		/* PMN3 is overflow */
		interrupt_count[COUNTER_PXA2_PMU_PMN3]++;
	}

	/* re-enable all counters */
	pmnc_value |= PMNC_ENABLE_BIT;
	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	return IRQ_HANDLED;
}

static void reset_all_counters(void)
{
	unsigned int i;

	for (i=0; i<COUNTER_PXA2_MAX_ID; i++)
	{
		interrupt_count[i] = 0;
	}
}

static void WritePML(int pmn_index, unsigned int value)
{
	__raw_writel(value, PML_ESEL_REG(pmn_index));
}

static int start_pmu_counters(void)
{
	int ret;
	int i;

	unsigned int flag_value;
	unsigned int pmnc_value;
	unsigned int inten_value;
	unsigned int evtsel_value;

	pml_base = ioremap(0x4600ff00, 32);

	if (pml_base == NULL)
	{
		return -ENOMEM;
	}

	/* disable all counters */
	pmnc_value = 0x10;
	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	/* reset overflow flag */
	flag_value = 0x1f;
	WritePMUReg(PXA2_PMU_FLAG, flag_value);

	ret = allocate_pmu();
	if (ret != 0)
		return ret;

	/* register pmu isr */
	ret = register_pmu_isr(pmu_isr_pxa2);
	if (ret != 0)
		return ret;

	pmnc_value   = 0x0;
	inten_value  = 0x0;
	evtsel_value = 0xffffffff;

	/* handle CCNT counter */
	if (cs[COUNTER_PXA2_PMU_CCNT].enabled)
	{
		/* reset CCNT counters to zero */
		pmnc_value |= 0x4;

		/* enable CCNT interrupt */
		inten_value |= 0x1;

		if (cs[COUNTER_PXA2_PMU_CCNT].event_id == PXA2_PMU_CCNT_CORE_CLOCK_TICK_64)
		{
			/* enable 64 clockticks CCNT */
			pmnc_value |= 0x8;
		}
	}

	/* handle PMN[i] counter */
	for (i=COUNTER_PXA2_PMU_PMN0; i<=COUNTER_PXA2_PMU_PMN3; i++)
	{
		if (cs[i].enabled)
		{
			int pmn_index = i - COUNTER_PXA2_PMU_PMN0;

			/* reset PMN[i] counters to zero */
			pmnc_value |= 0x2;

			/* enable PMN[i] interrupt */
			inten_value |= 0x2 << pmn_index;

			/* set PMN[i] event */
			if (cs[i].event_id & PXA2_PMU_MONAHANS_EVENT_MASK)
			{
				WritePML(pmn_index, cs[i].event_id & PXA2_PMU_MONAHANS_EVENT_MASK);
				evtsel_value |= (0xff - (PXA2_PMU_EVENT_ASSP_0 + pmn_index)) << (pmn_index * 8);
			}
			else
			{
				evtsel_value ^= (0xff - cs[i].event_id ) << (pmn_index * 8);
			}
		}
	}

	WritePMUReg(PXA2_PMU_EVTSEL, evtsel_value);
	WritePMUReg(PXA2_PMU_INTEN, inten_value);
	//WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	/* enable all the PMU counters */
	//pmnc_value &= ~0x60;
	pmnc_value &= ~0x10;
	pmnc_value |= 0x1;
	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	return 0;
}

static int stop_pmu_counters(void)
{
	unsigned int pmnc_value;

	pmnc_value = ReadPMUReg(PXA2_PMU_PMNC);

	/* disable all the PMU counters */
	pmnc_value &= ~0x1;

	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	unregister_pmu_isr();

	free_pmu();

	return 0;
}

static int pause_pmu_counters(void)
{
	int pmnc_value;

	pmnc_value = ReadPMUReg(PXA2_PMU_PMNC);

	/* disable all the PMU counters */
	pmnc_value &= ~0x1;

	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	return 0;
}

static int resume_pmu_counters(void)
{
	unsigned int pmnc_value;

	pmnc_value = ReadPMUReg(PXA2_PMU_PMNC);

	/* enable all the PMU counters */
	pmnc_value |= 0x1;

	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	return 0;
}

static void config_all_counters(void)
{
	unsigned int i;
	unsigned int cid;
	unsigned int cevent;

	for (i=0; i<COUNTER_PXA2_MAX_ID; i++)
	{
		cs[i].enabled  = false;
		cs[i].event_id = 0;
	}

	for (i=0; i<g_counter_config.number; i++)
	{
		cid = g_counter_config.settings[i].cid;
		cevent = g_counter_config.settings[i].cevent;

		cs[cid].enabled = true;
		cs[cid].event_id = cevent;
	}

}

static bool is_pmu_enabled(void)
{
	if (cs[COUNTER_PXA2_PMU_CCNT].enabled)
		return true;

	if (cs[COUNTER_PXA2_PMU_PMN0].enabled)
		return true;

	if (cs[COUNTER_PXA2_PMU_PMN1].enabled)
		return true;

	if (cs[COUNTER_PXA2_PMU_PMN2].enabled)
		return true;

	if (cs[COUNTER_PXA2_PMU_PMN3].enabled)
		return true;

	return false;
}

static int start_counters_pxa2(void)
{
	int ret;

	reset_all_counters();

	config_all_counters();

	if (is_pmu_enabled() && (ret = start_pmu_counters()) != 0)
	{
		return ret;
	}

	return 0;
}

static int stop_counters_pxa2(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = stop_pmu_counters()) != 0)
		return ret;

	return 0;
}

static int pause_counters_pxa2(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = pause_pmu_counters()) != 0)
		return ret;

	return 0;
}

static int resume_counters_pxa2(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = resume_pmu_counters()) != 0)
		return ret;

	return 0;
}

struct cm_ctr_op_mach cm_op_pxa2 = {
	.start        = start_counters_pxa2,
	.stop         = stop_counters_pxa2,
	.pause        = pause_counters_pxa2,
	.resume       = resume_counters_pxa2,
	//.read_counter = read_counter_pxa2,
	//.read_counter_on_all_cpus = read_counter_on_all_cpus_pxa2,
	.read_counter_on_this_cpu = read_counter_on_this_cpu_pxa2,
	//.pmu_isr      = pmu_isr_pxa2,
};
