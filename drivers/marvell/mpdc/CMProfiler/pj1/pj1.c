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

#include "pxpmu.h"
#include "pmu_pj1.h"
#include "mc_pj1.h"
#include "CMProfilerDef.h"
#include "CMProfilerSettings.h"
#include "EventTypes_pj1.h"
#include "RegisterId_pj1.h"
#include "cm_drv.h"

extern u32  PJ1_ReadINTEN(void);
extern void PJ1_WriteINTEN(u32 value);
extern u32  PJ1_ReadFLAG(void);
extern void PJ1_WriteFLAG(u32 value);
extern u32  PJ1_ReadCOR(int cor);
extern void PJ1_WriteCOR(int cor, u32 value);
extern u64  PJ1_ReadCounter(int counter);
extern void PJ1_WriteCounter(int counter, u64 value);

extern struct CMCounterConfigs g_counter_config;

struct counter_settings
{
	bool enabled;
	int  event_id;
};

static unsigned int interrupt_count[COUNTER_PJ1_MAX_ID];
static struct counter_settings cs[COUNTER_PJ1_MAX_ID];
static u64  register_value[COUNTER_PJ1_MAX_ID];
#if 0
static void * mc_addr = NULL;
static void * dev_id = NULL;
static u32  mc_ctrl_0_value = 0;
#endif
void __iomem *pml_base = NULL;
#define PML_ESEL_REG(i) (void __iomem *)(pml_base + i * 4)

static unsigned long long pmu_read_counter(int counter)
{
	unsigned long long value;

	value = PJ1_ReadCounter(counter);

	return value;
}

#if 0
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

	mc_write_reg(PERF_COUNT_SEL, counter - COUNTER_PJ1_MC0);
	value = mc_read_reg(PERF_COUNT);

	return value;
}
#endif

static unsigned long long ost_read_counter(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	return (tv.tv_usec + (USEC_PER_SEC * (unsigned long long)tv.tv_sec));
}

static bool read_counter_pj1(int counter_id, unsigned long long * value)
{
	unsigned long long  count64 = 0;

	switch(counter_id)
	{
	case COUNTER_PJ1_OS_TIMER:
		count64  = ost_read_counter();
		break;

	case COUNTER_PJ1_PMU_PMN0:
	case COUNTER_PJ1_PMU_PMN1:
	case COUNTER_PJ1_PMU_PMN2:
	case COUNTER_PJ1_PMU_PMN3:
		count64 = pmu_read_counter(PJ1_PMU_PMN0 + counter_id - COUNTER_PJ1_PMU_PMN0);
		break;
#if 0
	case COUNTER_PJ1_MC0:
	case COUNTER_PJ1_MC1:
	case COUNTER_PJ1_MC2:
	case COUNTER_PJ1_MC3:
		if (mc_addr == NULL)
			count64 = ((u64)interrupt_count[counter_id] << 32) + register_value[counter_id];
		else
			count64 = ((u64)interrupt_count[counter_id] << 32) + mc_read_counter(counter_id);
		break;
#endif
	default:
		return false;
	}

	*value = count64;

	return true;
}

/* read counter value on current CPU */
static bool read_counter_on_this_cpu_pj1(int counter_id, unsigned long long * value)
{
	return read_counter_pj1(counter_id, value);
}

#if 0
/*
 * read counter value on each CPU
 * parameter:
 *            counter_id: which counter to read
 *            value:      array which contains counter values for each CPU
 */
static bool read_counter_on_all_cpus_pj1(int counter_id, unsigned long long * value)
{
	bool ret;
	unsigned long long cv;

	if (value == NULL)
		return false;

	ret = read_counter_on_this_cpu_pj1(counter_id, &cv);

	value[0] = cv;

	return ret;
}
#endif

irqreturn_t pmu_isr_pj1(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static void reset_all_counters(void)
{
	memset(interrupt_count, 0, sizeof(interrupt_count));
	memset(cs, 0, sizeof(cs));
	memset(register_value, 0, sizeof(register_value));
}

static void set_pmn_events(int                        pmn_index,
                           struct counter_settings  * cs,
                           struct pmu_registers_pj1 * pmu_regs)
{
	int register_id;

	switch (pmn_index)
	{
	case 0: register_id = COUNTER_PJ1_PMU_PMN0; break;
	case 1: register_id = COUNTER_PJ1_PMU_PMN1; break;
	case 2: register_id = COUNTER_PJ1_PMU_PMN2; break;
	case 3: register_id = COUNTER_PJ1_PMU_PMN3; break;
	default: return;
	}

	pmu_regs->cor[pmn_index] = get_pmu_event_id(cs->event_id);
	pmu_regs->pmn[pmn_index] = 0;

	return;
}

static int start_pmu_counters(void)
{
	int i;
	int ret;

	struct pmu_registers_pj1 pmu_regs;

	pmu_regs.inten = 0;
	pmu_regs.flag  = 0xf;

	for (i=0; i<PJ1_PMN_NUM; i++)
	{
		pmu_regs.cor[i] = 0;
	}

	for (i=0; i<PJ1_PMN_NUM; i++)
	{
		pmu_regs.pmn[i] = 0;
	}

	/* There is no PMU interrupt in Avanta */
	#if !defined(PX_SOC_AVANTA)
	ret = allocate_pmu();
	if (ret != 0)
		return ret;

	/* register pmu isr */
	ret = register_pmu_isr(pmu_isr_pj1);
	if (ret != 0)
		return ret;

	#endif

	/* disable all PMU counters */
	PJ1_WriteCounter(PJ1_PMU_COR0, 0);
	PJ1_WriteCounter(PJ1_PMU_COR1, 0);
	PJ1_WriteCounter(PJ1_PMU_COR2, 0);
	PJ1_WriteCounter(PJ1_PMU_COR3, 0);

	/* disable interrupts and clear the overflow flags */
	PJ1_WriteINTEN(pmu_regs.inten);
	PJ1_WriteFLAG(pmu_regs.flag);

	/* calculate the register values to be set */
	if (cs[COUNTER_PJ1_PMU_PMN0].enabled)
	{
		set_pmn_events(0, &cs[COUNTER_PJ1_PMU_PMN0], &pmu_regs);
	}

	if (cs[COUNTER_PJ1_PMU_PMN1].enabled)
	{
		set_pmn_events(1, &cs[COUNTER_PJ1_PMU_PMN1], &pmu_regs);
	}

	if (cs[COUNTER_PJ1_PMU_PMN2].enabled)
	{
		set_pmn_events(2, &cs[COUNTER_PJ1_PMU_PMN2], &pmu_regs);
	}

	if (cs[COUNTER_PJ1_PMU_PMN3].enabled)
	{
		set_pmn_events(3, &cs[COUNTER_PJ1_PMU_PMN3], &pmu_regs);
	}

	/* initialize the counter values: zero */
	PJ1_WriteCounter(PJ1_PMU_PMN0, pmu_regs.pmn[0]);
	PJ1_WriteCounter(PJ1_PMU_PMN1, pmu_regs.pmn[1]);
	PJ1_WriteCounter(PJ1_PMU_PMN2, pmu_regs.pmn[2]);
	PJ1_WriteCounter(PJ1_PMU_PMN3, pmu_regs.pmn[3]);

	/* set events and enable the counters */
	PJ1_WriteCOR(PJ1_PMU_COR0, pmu_regs.cor[0]);
	PJ1_WriteCOR(PJ1_PMU_COR1, pmu_regs.cor[1]);
	PJ1_WriteCOR(PJ1_PMU_COR2, pmu_regs.cor[2]);
	PJ1_WriteCOR(PJ1_PMU_COR3, pmu_regs.cor[3]);

	return 0;
}

static int stop_pmu_counters(void)
{
	int i;

	u32 cor_value[PJ1_PMN_NUM];
	u32 inten_value;
	u32 flag_value;

	inten_value = 0x0;
	flag_value  = 0xf;

	/* disable all the PMU counters */
	cor_value[0] = PJ1_ReadCOR(PJ1_PMU_COR0);
	cor_value[1] = PJ1_ReadCOR(PJ1_PMU_COR1);
	cor_value[2] = PJ1_ReadCOR(PJ1_PMU_COR2);
	cor_value[3] = PJ1_ReadCOR(PJ1_PMU_COR3);

	for (i=0; i<PJ1_PMN_NUM; i++)
	{
		cor_value[i] &= ~0x1;
	}

	PJ1_WriteCOR(PJ1_PMU_COR0, cor_value[0]);
	PJ1_WriteCOR(PJ1_PMU_COR1, cor_value[1]);
	PJ1_WriteCOR(PJ1_PMU_COR2, cor_value[2]);
	PJ1_WriteCOR(PJ1_PMU_COR3, cor_value[3]);

	/* disable all interrupts */
	PJ1_WriteINTEN(inten_value);

	/* clear overflow flags */
	PJ1_WriteFLAG(flag_value);

	/* There is no PMU interrupt in AVANTA */
#if !defined(PX_SOC_AVANTA)
	unregister_pmu_isr();
#endif

	free_pmu();

	return 0;
}

static int pause_pmu_counters(void)
{
	int i;
	int cor_value[PJ1_PMN_NUM];

	cor_value[0] = PJ1_ReadCOR(PJ1_PMU_COR0);
	cor_value[1] = PJ1_ReadCOR(PJ1_PMU_COR1);
	cor_value[2] = PJ1_ReadCOR(PJ1_PMU_COR2);
	cor_value[3] = PJ1_ReadCOR(PJ1_PMU_COR3);

	/* disable all the PMU counters */
	for (i=0; i<PJ1_PMN_NUM; i++)
	{
		cor_value[i] &= ~0x1;
	}

	PJ1_WriteCOR(PJ1_PMU_COR0, cor_value[0]);
	PJ1_WriteCOR(PJ1_PMU_COR1, cor_value[1]);
	PJ1_WriteCOR(PJ1_PMU_COR2, cor_value[2]);
	PJ1_WriteCOR(PJ1_PMU_COR3, cor_value[3]);

	return 0;
}

static int resume_pmu_counters(void)
{
	int i;
	int cor_value[PJ1_PMN_NUM];

	cor_value[0] = PJ1_ReadCOR(PJ1_PMU_COR0);
	cor_value[1] = PJ1_ReadCOR(PJ1_PMU_COR1);
	cor_value[2] = PJ1_ReadCOR(PJ1_PMU_COR2);
	cor_value[3] = PJ1_ReadCOR(PJ1_PMU_COR3);

	/* enable all the PMU counters */
	for (i=0; i<PJ1_PMN_NUM; i++)
	{
		cor_value[i] |= 0x1;
	}

	PJ1_WriteCOR(PJ1_PMU_COR0, cor_value[0]);
	PJ1_WriteCOR(PJ1_PMU_COR1, cor_value[1]);
	PJ1_WriteCOR(PJ1_PMU_COR2, cor_value[2]);
	PJ1_WriteCOR(PJ1_PMU_COR3, cor_value[3]);

	return 0;
}

static void config_all_counters(void)
{
	unsigned int i;
	unsigned int cid;
	unsigned int cevent;

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
	if (cs[COUNTER_PJ1_PMU_PMN0].enabled)
		return true;

	if (cs[COUNTER_PJ1_PMU_PMN1].enabled)
		return true;

	if (cs[COUNTER_PJ1_PMU_PMN2].enabled)
		return true;

	if (cs[COUNTER_PJ1_PMU_PMN3].enabled)
		return true;

	return false;
}

#if 0
static bool is_mc_enabled(void)
{
	if (cs[COUNTER_PJ1_MC0].enabled)
		return true;

	if (cs[COUNTER_PJ1_MC1].enabled)
		return true;

	if (cs[COUNTER_PJ1_MC2].enabled)
		return true;

	if (cs[COUNTER_PJ1_MC3].enabled)
		return true;

	return false;
}

irqreturn_t px_mc_irq(int irq, void * dev)
{
	u32 overflow;

	mc_write_reg(PERF_COUNT_CNTRL_0, DISABLE_COUNT_ALL_INT | DISABLE_COUNT_ALL);
	overflow = mc_read_reg(PERF_COUNT_STAT);

	if (overflow & COUNT_0_OVERFLOW)
		interrupt_count[COUNTER_PJ1_MC0]++;
	if (overflow & COUNT_1_OVERFLOW)
		interrupt_count[COUNTER_PJ1_MC1]++;
	if (overflow & COUNT_2_OVERFLOW)
		interrupt_count[COUNTER_PJ1_MC2]++;
	if (overflow & COUNT_3_OVERFLOW)
		interrupt_count[COUNTER_PJ1_MC3]++;

	// clear the overflow flags
	mc_write_reg(PERF_COUNT_STAT, 0);

	// enable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, mc_ctrl_0_value);
	return IRQ_HANDLED;
}

static int start_mc_counters(void)
{
	int result;

	// setup
	mc_addr = ioremap(PERF_COUNT_CNTRL_0, 0x100);
	mc_write_reg(PERF_COUNT_CNTRL_0, DISABLE_COUNT_ALL_INT | DISABLE_COUNT_ALL);
	mc_write_reg(PERF_COUNT_CNTRL_1, CONTINUE_WHEN_OVERFLOW | START_WHEN_ENABLED);

	// clear the overflow flags
	mc_write_reg(PERF_COUNT_STAT, 0);

	// set events and enable flags
	if (cs[COUNTER_PJ1_MC0].enabled) {
		mc_write_reg(PERF_COUNT_SEL, 0x80000000 | (cs[COUNTER_PJ1_MC0].event_id << 4) | 0x0);
		mc_ctrl_0_value |= ENABLE_COUNT_0 | ENABLE_COUNT_0_INT;
	}
	if (cs[COUNTER_PJ1_MC1].enabled) {
		mc_write_reg(PERF_COUNT_SEL, 0x80000000 | (cs[COUNTER_PJ1_MC1].event_id << 4) | 0x1);
		mc_ctrl_0_value |= ENABLE_COUNT_1 | ENABLE_COUNT_1_INT;
	}
	if (cs[COUNTER_PJ1_MC2].enabled) {
		mc_write_reg(PERF_COUNT_SEL, 0x80000000 | (cs[COUNTER_PJ1_MC2].event_id << 4) | 0x2);
		mc_ctrl_0_value |= ENABLE_COUNT_2 | ENABLE_COUNT_2_INT;
	}
	if (cs[COUNTER_PJ1_MC3].enabled) {
		mc_write_reg(PERF_COUNT_SEL, 0x80000000 | (cs[COUNTER_PJ1_MC3].event_id << 4) | 0x3);
		mc_ctrl_0_value |= ENABLE_COUNT_3 | ENABLE_COUNT_3_INT;
	}

	result = request_irq(IRQ_DDR_MC, px_mc_irq, 0/*IRQF_DISABLED*/, "CPA MC", dev_id);
	if (result)
		return result;

	// enable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, mc_ctrl_0_value);
	return 0;
}

static int stop_mc_counters(void)
{
	// disable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, DISABLE_COUNT_ALL_INT | DISABLE_COUNT_ALL);

	// save the register values for future reading
	register_value[COUNTER_PJ1_MC0] = mc_read_counter(COUNTER_PJ1_MC0);
	register_value[COUNTER_PJ1_MC1] = mc_read_counter(COUNTER_PJ1_MC1);
	register_value[COUNTER_PJ1_MC2] = mc_read_counter(COUNTER_PJ1_MC2);
	register_value[COUNTER_PJ1_MC3] = mc_read_counter(COUNTER_PJ1_MC3);

	// cleanup
	free_irq(IRQ_DDR_MC, dev_id);
	if (mc_addr != NULL)
		iounmap(mc_addr);
	mc_addr = NULL;

	return 0;
}

static int pause_mc_counters(void)
{
	// disable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, DISABLE_COUNT_ALL_INT | DISABLE_COUNT_ALL);

	return 0;
}

static int resume_mc_counters(void)
{
	// enable counters
	mc_write_reg(PERF_COUNT_CNTRL_0, mc_ctrl_0_value);

	return 0;
}

#endif

static int start_counters_pj1(void)
{
	int ret;

	reset_all_counters();

	config_all_counters();

	if (is_pmu_enabled() && (ret = start_pmu_counters()) != 0)
		return ret;
#if 0
	if (is_mc_enabled() && (ret = start_mc_counters()) != 0)
		return ret;
#endif
	return 0;
}

static int stop_counters_pj1(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = stop_pmu_counters()) != 0)
		return ret;
#if 0
	if (is_mc_enabled() && (ret = stop_mc_counters()) != 0)
		return ret;
#endif
	return 0;
}

static int pause_counters_pj1(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = pause_pmu_counters()) != 0)
		return ret;
#if 0
	if (is_mc_enabled() && (ret = pause_mc_counters()) != 0)
		return ret;
#endif
	return 0;
}

static int resume_counters_pj1(void)
{
	int ret;

	if (is_pmu_enabled() && (ret = resume_pmu_counters()) != 0)
		return ret;
#if 0
	if (is_mc_enabled() && (ret = resume_mc_counters()) != 0)
		return ret;
#endif
	return 0;
}
/*
void init_pj1(void)
{
	__REG_PXA910(0xd4282c08) |= 0x100;
}
*/

struct cm_ctr_op_mach cm_op_pj1 =
{
	.start  = start_counters_pj1,
	.stop   = stop_counters_pj1,
	.pause  = pause_counters_pj1,
	.resume = resume_counters_pj1,
	//.read_counter = read_counter_pj1,
	//.read_counter_on_all_cpus = read_counter_on_all_cpus_pj1,
	.read_counter_on_this_cpu = read_counter_on_this_cpu_pj1,
//	.pmu_isr = pmu_isr_pj1,
};
