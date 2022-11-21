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

#include "pmu_pj4.h"
#include "mc_pj4.h"
#include "CMProfilerDef.h"
#include "CMProfilerSettings.h"
#include "EventTypes_pj4.h"
#include "RegisterId_pj4.h"
#include "cm_drv.h"

extern struct CMCounterConfigs g_counter_config;

static unsigned int interrupt_count[MAX_CM_COUNTER_NUM];
static u64  register_value[COUNTER_PJ4_MAX_ID];
static void * mc_addr = NULL;

#ifdef PX_SOC_ARMADA610
static void * dev_id = NULL;
static u32  mc_ctrl_0_value = 0;
#endif

struct counter_settings
{
	bool enabled;   /* if the counter is enabled */
	int  event_id;  /* the event id set to this counter */
};

static struct counter_settings cs[COUNTER_PJ4_MAX_ID];

static unsigned long long read_ccnt_counter(void)
{
	unsigned long long value;

	value = ((unsigned long long)interrupt_count[COUNTER_PJ4_PMU_CCNT] << 32) +  PJ4_Read_CCNT();

	return value;
}

static unsigned long long read_pmn0_counter(void)
{
	unsigned long long value;

	PJ4_Write_PMNXSEL(0x0);

	value = ((unsigned long long)interrupt_count[COUNTER_PJ4_PMU_PMN0] << 32) +  PJ4_Read_PMNCNT();

	return value;
}

static unsigned long long read_pmn1_counter(void)
{
	unsigned long long value;

	PJ4_Write_PMNXSEL(0x1);

	value = ((unsigned long long)interrupt_count[COUNTER_PJ4_PMU_PMN1] << 32) +  PJ4_Read_PMNCNT();

	return value;
}

static unsigned long long read_pmn2_counter(void)
{
	unsigned long long value;

	PJ4_Write_PMNXSEL(0x2);

	value = ((unsigned long long)interrupt_count[COUNTER_PJ4_PMU_PMN2] << 32) +  PJ4_Read_PMNCNT();

	return value;
}

static unsigned long long read_pmn3_counter(void)
{
	unsigned long long value;

	PJ4_Write_PMNXSEL(0x3);

	value = ((unsigned long long)interrupt_count[COUNTER_PJ4_PMU_PMN3] << 32) +  PJ4_Read_PMNCNT();

	return value;
}

static u32 mc_read_reg(int reg)
{
	return readl(mc_addr + (reg - PERF_COUNT_CNTRL_0));
}

static void mc_write_reg(int reg, u32 value)
{
	writel(value, mc_addr + (reg - PERF_COUNT_CNTRL_0));
}

static unsigned long long mc_read_counter(int counter)
{
	unsigned long long value;

	mc_write_reg(PERF_COUNT_SEL, counter - COUNTER_PJ4_MC0);
	value = mc_read_reg(PERF_COUNT);

	return value;
}

static unsigned long long read_os_timer_counter(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	return (tv.tv_usec + (USEC_PER_SEC * (unsigned long long)tv.tv_sec));
}

static bool read_counter_pj4(int counter_id, unsigned long long * value)
{
	unsigned long long  count64;

	count64 = 0;

	switch(counter_id)
	{
		case COUNTER_PJ4_OS_TIMER:
			count64  = read_os_timer_counter();
			break;

		case COUNTER_PJ4_PMU_CCNT:
			count64 = read_ccnt_counter();
			break;

		case COUNTER_PJ4_PMU_PMN0:
			count64 = read_pmn0_counter();
			break;

		case COUNTER_PJ4_PMU_PMN1:
			count64 = read_pmn1_counter();
			break;

		case COUNTER_PJ4_PMU_PMN2:
			count64 = read_pmn2_counter();
			break;

		case COUNTER_PJ4_PMU_PMN3:
			count64 = read_pmn3_counter();
			break;

		case COUNTER_PJ4_MC0:
		case COUNTER_PJ4_MC1:
		case COUNTER_PJ4_MC2:
		case COUNTER_PJ4_MC3:
			if (mc_addr == NULL)
				count64 = ((u64)interrupt_count[counter_id] << 32) + register_value[counter_id];
			else
				count64 = ((u64)interrupt_count[counter_id] << 32) + mc_read_counter(counter_id);
			break;

		default:
			return false;
	}

	*value = count64;

	return true;
}

/* read counter value on current CPU */
static bool read_counter_on_this_cpu_pj4(int counter_id, unsigned long long * value)
{
	return read_counter_pj4(counter_id, value);
}

#if 0
/*
 * read counter value on each CPU
 * parameter:
 *            counter_id: which counter to read
 *            value:      array which contains counter values for each CPU
 */
static bool read_counter_on_all_cpus_pj4(int counter_id, unsigned long long * value)
{
	bool ret;
	unsigned long long cv;

	if (value == NULL)
		return false;

	ret = read_counter_on_this_cpu_pj4(counter_id, &cv);

	value[0] = cv;

	return ret;
}
#endif

static irqreturn_t pmu_isr_pj4(int irq, void *dev_id)
{
	u32 pmnc_value;
	u32 flag_value;

	/* Disable counters */
	pmnc_value = PJ4_Read_PMNC();
	pmnc_value &= ~0x1;
	PJ4_Write_PMNC(pmnc_value);

	/* Save current FLAG value */
	flag_value = PJ4_Read_FLAG();

	/* Clear overflow flags */
	PJ4_Write_FLAG(0x8000000f);

	if (flag_value & 0x80000000)
	{
		/* CCNT is overflow */
		interrupt_count[COUNTER_PJ4_PMU_CCNT]++;
	}

	if (flag_value & 0x00000001)
	{
		/* PMN0 is overflow */
		interrupt_count[COUNTER_PJ4_PMU_PMN0]++;
	}

	if (flag_value & 0x00000002)
	{
		/* PMN1 is overflow */
		interrupt_count[COUNTER_PJ4_PMU_PMN1]++;
	}

	if (flag_value & 0x00000004)
	{
		/* PMN2 is overflow */
		interrupt_count[COUNTER_PJ4_PMU_PMN2]++;
	}

	if (flag_value & 0x00000008)
	{
		/* PMN3 is overflow */
		interrupt_count[COUNTER_PJ4_PMU_PMN3]++;
	}

	/* re-enable all counters */
	pmnc_value |= 0x1;
	PJ4_Write_PMNC(pmnc_value);

	return IRQ_HANDLED;
}

static void reset_all_counters(void)
{
	unsigned int i;

	for (i=0; i<COUNTER_PJ4_MAX_ID; i++)
	{
		interrupt_count[i] = 0;
	}

	memset(register_value, 0, sizeof(register_value));
}

static void setup_pmu_regs(struct pmu_registers_pj4 *pmu_regs)
{
	int i;

	if (pmu_regs == NULL)
		return;

	memset(pmu_regs, 0, sizeof(struct pmu_registers_pj4));
	pmu_regs->pmnc = PJ4_Read_PMNC();

	for (i=0; i<COUNTER_PJ4_MAX_ID; i++)
	{
		unsigned int pmn_index;

		bool is_ccnt = false;
		unsigned int event_id;

		switch (i)
		{
			case COUNTER_PJ4_PMU_CCNT:
				pmn_index = -1;
				is_ccnt = true;
				break;
			case COUNTER_PJ4_PMU_PMN0:
				pmn_index = 0;
				break;
			case COUNTER_PJ4_PMU_PMN1:
				pmn_index = 1;
				break;
			case COUNTER_PJ4_PMU_PMN2:
				pmn_index = 2;
				break;
			case COUNTER_PJ4_PMU_PMN3:
				pmn_index = 3;
				break;
			default:
				continue;
		}

		if (!cs[i].enabled)
			continue;

		event_id = cs[i].event_id;

		if (is_ccnt)
		{
			/* it is the CCNT counter */
			if (event_id == PJ4_PMU_CCNT_CORE_CLOCK_TICK_64)
			{
				/* 64 clock cycle */
				pmu_regs->pmnc |= 0x8;
			}
			else if (event_id == PJ4_PMU_CCNT_CORE_CLOCK_TICK)
			{
				pmu_regs->pmnc &= ~0x8;
			}

			pmu_regs->intens |= 0x1 << 31;
			pmu_regs->cntens |= 0x1 << 31;
			pmu_regs->flag   |= 0x1 << 31;

			pmu_regs->ccnt = 0;
		}
		else
		{
			/* it is the PMN counter */
			pmu_regs->intens |= (0x1 << pmn_index);
			pmu_regs->cntens |= (0x1 << pmn_index);
			pmu_regs->flag   |= (0x1 << pmn_index);

			pmu_regs->evtsel[pmn_index] = event_id;

			pmu_regs->pmncnt[pmn_index] = 0;
		}
	}
}

static int start_pmu_counters(void)
{
	int i;
	int ret;

	struct pmu_registers_pj4 pmu_regs;

	ret = allocate_pmu();
	if (ret != 0)
		return ret;

	/* register pmu isr */
	ret = register_pmu_isr(pmu_isr_pj4);
	if (ret != 0)
		return ret;

	/* disable all PMU counters */
	pmu_regs.pmnc = PJ4_Read_PMNC();
	pmu_regs.pmnc &= ~0x1;
	PJ4_Write_PMNC(pmu_regs.pmnc);

	PJ4_Write_CNTENC(0x8000000f);

	/* disable interrupts and clear the overflow flags */
	PJ4_Write_INTENC(0x8000000f);
	PJ4_Write_FLAG(0x8000000f);

	/* calculate the register values to be set */
	setup_pmu_regs(&pmu_regs);

	/* set value to pmu counters */
	PJ4_WritePMUCounter(COUNTER_PJ4_PMU_CCNT, pmu_regs.ccnt);
	PJ4_WritePMUCounter(COUNTER_PJ4_PMU_PMN0, pmu_regs.pmncnt[0]);
	PJ4_WritePMUCounter(COUNTER_PJ4_PMU_PMN1, pmu_regs.pmncnt[1]);
	PJ4_WritePMUCounter(COUNTER_PJ4_PMU_PMN2, pmu_regs.pmncnt[2]);
	PJ4_WritePMUCounter(COUNTER_PJ4_PMU_PMN3, pmu_regs.pmncnt[3]);

	/* set events */
	for (i=0; i<4; i++)
	{
		PJ4_Write_PMNXSEL(i);
		PJ4_Write_EVTSEL(pmu_regs.evtsel[i]);
	}

	/* enable the interrupts */
	PJ4_Write_INTENS(pmu_regs.intens);

	/* enable the counters */
	PJ4_Write_CNTENS(pmu_regs.cntens);

	/* Enable PMU */
	pmu_regs.pmnc |= 0x1;
	PJ4_Write_PMNC(pmu_regs.pmnc);

	return 0;
}

static int stop_pmu_counters(void)
{
	u32 intenc_value;
	u32 flag_value;
	u32 pmnc_value;

	intenc_value = 0x8000000f;
	flag_value   = 0x8000000f;

	/* disable all counters */
	pmnc_value = PJ4_Read_PMNC();
	pmnc_value &= ~0x1;
	PJ4_Write_PMNC(pmnc_value);

	/* clear overflow flags */
	PJ4_Write_FLAG(flag_value);

	/* disable all interrupts */
	PJ4_Write_INTENC(intenc_value);

	/* unregiste the pmu isr */
	unregister_pmu_isr();

	free_pmu();

	return 0;
}

static int pause_pmu_counters(void)
{
	u32 pmnc_value;

	/* disable all counters */
	pmnc_value = PJ4_Read_PMNC();
	pmnc_value &= ~0x1;
	PJ4_Write_PMNC(pmnc_value);

	return 0;
}

static int resume_pmu_counters(void)
{
	u32 pmnc_value;

	/* enable all counters */
	pmnc_value = PJ4_Read_PMNC();
	pmnc_value |= 0x1;
	PJ4_Write_PMNC(pmnc_value);

	return 0;
}

static void config_all_counters(void)
{
	unsigned int i;
	unsigned int cid;
	unsigned int cevent;

	for (i=0; i<COUNTER_PJ4_MAX_ID; i++)
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
	if (cs[COUNTER_PJ4_PMU_CCNT].enabled)
		return true;

	if (cs[COUNTER_PJ4_PMU_PMN0].enabled)
		return true;

	if (cs[COUNTER_PJ4_PMU_PMN1].enabled)
		return true;

	if (cs[COUNTER_PJ4_PMU_PMN2].enabled)
		return true;

	if (cs[COUNTER_PJ4_PMU_PMN3].enabled)
		return true;

	return false;
}

static bool is_mc_enabled(void)
{
	if (cs[COUNTER_PJ4_MC0].enabled)
		return true;

	if (cs[COUNTER_PJ4_MC1].enabled)
		return true;

	if (cs[COUNTER_PJ4_MC2].enabled)
		return true;

	if (cs[COUNTER_PJ4_MC3].enabled)
		return true;

	return false;
}

#ifdef PX_SOC_ARMADA610
irqreturn_t px_mc_irq(int irq, void * dev)
{
	u32 overflow;

	mc_write_reg(PERF_COUNT_CNTRL_0, DISABLE_COUNT_ALL_INT | DISABLE_COUNT_ALL);
	overflow = mc_read_reg(PERF_COUNT_STAT);

	if (overflow & COUNT_0_OVERFLOW)
		interrupt_count[COUNTER_PJ4_MC0]++;
	if (overflow & COUNT_1_OVERFLOW)
		interrupt_count[COUNTER_PJ4_MC1]++;
	if (overflow & COUNT_2_OVERFLOW)
		interrupt_count[COUNTER_PJ4_MC2]++;
	if (overflow & COUNT_3_OVERFLOW)
		interrupt_count[COUNTER_PJ4_MC3]++;

	// clear the overflow flags
	mc_write_reg(PERF_COUNT_STAT, 0);

	// enable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, mc_ctrl_0_value);
	return IRQ_HANDLED;
}
#endif

static int start_mc_counters(void)
{
#ifdef PX_SOC_ARMADA610
	int result;

	// setup
	mc_addr = ioremap(PERF_COUNT_CNTRL_0, 0x100);

	mc_write_reg(PERF_COUNT_CNTRL_0, DISABLE_COUNT_ALL_INT | DISABLE_COUNT_ALL);
	mc_write_reg(PERF_COUNT_CNTRL_1, CONTINUE_WHEN_OVERFLOW | START_WHEN_ENABLED);

	// clear the overflow flags
	mc_write_reg(PERF_COUNT_STAT, 0);

	// set events and enable flags
	if (cs[COUNTER_PJ4_MC0].enabled) {
		mc_write_reg(PERF_COUNT_SEL, 0x80000000 | (cs[COUNTER_PJ4_MC0].event_id << 4) | 0x0);
		mc_ctrl_0_value |= ENABLE_COUNT_0 | ENABLE_COUNT_0_INT;
	}
	if (cs[COUNTER_PJ4_MC1].enabled) {
		mc_write_reg(PERF_COUNT_SEL, 0x80000000 | (cs[COUNTER_PJ4_MC1].event_id << 4) | 0x1);
		mc_ctrl_0_value |= ENABLE_COUNT_1 | ENABLE_COUNT_1_INT;
	}
	if (cs[COUNTER_PJ4_MC2].enabled) {
		mc_write_reg(PERF_COUNT_SEL, 0x80000000 | (cs[COUNTER_PJ4_MC2].event_id << 4) | 0x2);
		mc_ctrl_0_value |= ENABLE_COUNT_2 | ENABLE_COUNT_2_INT;
	}
	if (cs[COUNTER_PJ4_MC3].enabled) {
		mc_write_reg(PERF_COUNT_SEL, 0x80000000 | (cs[COUNTER_PJ4_MC3].event_id << 4) | 0x3);
		mc_ctrl_0_value |= ENABLE_COUNT_3 | ENABLE_COUNT_3_INT;
	}

	result = request_irq(IRQ_DDR_MC, px_mc_irq, 0/*IRQF_DISABLED*/, "CPA MC", dev_id);
	if (result)
		return result;

	// enable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, mc_ctrl_0_value);
	return 0;
#else
	return 0;
#endif

}

static int mc_stop_counters(void)
{
#ifdef PX_SOC_ARMADA610
	// disable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, DISABLE_COUNT_ALL_INT | DISABLE_COUNT_ALL);

	// save the register values for future reading
	register_value[COUNTER_PJ4_MC0] = mc_read_counter(COUNTER_PJ4_MC0);
	register_value[COUNTER_PJ4_MC1] = mc_read_counter(COUNTER_PJ4_MC1);
	register_value[COUNTER_PJ4_MC2] = mc_read_counter(COUNTER_PJ4_MC2);
	register_value[COUNTER_PJ4_MC3] = mc_read_counter(COUNTER_PJ4_MC3);

	// cleanup
	free_irq(IRQ_DDR_MC, dev_id);
	if (mc_addr != NULL)
		iounmap(mc_addr);
	mc_addr = NULL;

	return 0;
#else
	return 0;
#endif
}

static int pause_mc_counters(void)
{
#ifdef PX_SOC_ARMADA610
	// disable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, DISABLE_COUNT_ALL_INT | DISABLE_COUNT_ALL);
	return 0;
#else
	return 0;
#endif
}

static int mc_resume_counters(void)
{
#ifdef PX_SOC_ARMADA610
	// enable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, mc_ctrl_0_value);
	return 0;
#else
	return 0;
#endif
}

static int start_counters_pj4(void)
{
	int ret;

	reset_all_counters();

	config_all_counters();

	if (is_pmu_enabled() && (ret = start_pmu_counters()) != 0)
	{
		return ret;
	}

	if (is_mc_enabled() && (ret = start_mc_counters()) != 0)
	{
		return ret;
	}

	return 0;
}

static int stop_counters_pj4(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = stop_pmu_counters()) != 0)
		return ret;

	if (is_mc_enabled() && (ret = mc_stop_counters()) != 0)
		return ret;

	return 0;
}

static int pause_counters_pj4(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = pause_pmu_counters()) != 0)
		return ret;

	if (is_mc_enabled() && (ret = pause_mc_counters()) != 0)
		return ret;

	return 0;
}

static int resume_counters_pj4(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = resume_pmu_counters()) != 0)
		return ret;

	if (is_mc_enabled() && (ret = mc_resume_counters()) != 0)
		return ret;

	return 0;
}

struct cm_ctr_op_mach cm_op_pj4 = {
	.start        = start_counters_pj4,
	.stop         = stop_counters_pj4,
	.pause        = pause_counters_pj4,
	.resume       = resume_counters_pj4,
	//.read_counter = read_counter_pj4,
	//.read_counter_on_all_cpus = read_counter_on_all_cpus_pj4,
	.read_counter_on_this_cpu = read_counter_on_this_cpu_pj4,
//	.pmu_isr = pmu_isr_pj4,
};
