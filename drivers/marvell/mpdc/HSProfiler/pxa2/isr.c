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

/*
 * PXA Hotspot ISR implementation
 */

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30))
#include <mach/regs-ost.h>
#elif defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <mach/hardware.h>
#include <mach/pxa-regs.h>
#else
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#endif

#include "PXD_hs.h"
#include "HSProfilerSettings.h"
#include "pmu_pxa2.h"
#include "timer_pxa2.h"
#include "hs_drv.h"

struct event_settings
{
	bool         enabled;
	bool         calibration;
	unsigned int reset_value;
	unsigned int overflow;
};

static struct event_settings es[COUNTER_PXA2_MAX_ID];

static u32 reg_value[COUNTER_PXA2_MAX_ID];

/* Additional definitions for OSCR5 related reg */
#define OSCR0 __REG(0x40a00010)
#define OMCR5 __REG(0x40a000c4)
#define OSCR5 __REG(0x40a00044)
#define OSMR5 __REG(0x40a00084)
#define OSSR_PX_INTERRUPT   (0x1 << 5)

static int dev_oscr;
static int result = 0;

static irqreturn_t px_pmu_isr(unsigned int pid,
                              unsigned int tid,
                              unsigned int pc,
                              unsigned long long ts)
{
	int i;
	int reg = 0;
	unsigned long flag_value;
	unsigned long pmnc_value;
	PXD32_Hotspot_Sample_V2 sample_rec;
	bool buffer_full = false;

	/* disable the timer interrupt */
	//disable_irq(PX_IRQ_PXA2_TIMER);

	/* disable the counters */
	pmnc_value = ReadPMUReg(PXA2_PMU_PMNC);
	WritePMUReg(PXA2_PMU_PMNC, pmnc_value & ~PMNC_ENABLE_BIT);

	/* clear the overflow flag */
	flag_value = ReadPMUReg(PXA2_PMU_FLAG);
	WritePMUReg(PXA2_PMU_FLAG, FLAG_OVERFLOW_BITS);

	/* write sample record to sample buffer */
	sample_rec.pc	   = pc;
	sample_rec.pid	   = pid;
	sample_rec.tid	   = tid;
	memcpy(sample_rec.timestamp, &ts, sizeof(sample_rec.timestamp));

	if (flag_value & CCNT_OVERFLAG_BIT && es[COUNTER_PXA2_PMU_CCNT].enabled)
	{
		sample_rec.registerId = COUNTER_PXA2_PMU_CCNT;
		if (es[COUNTER_PXA2_PMU_CCNT].calibration == false)
		{
			/* write sample record in non-calibration mode */
			buffer_full |= write_sample(0, &sample_rec);
		}
		else
		{
			/* calculate the overflow count in calibration mode */
			es[sample_rec.registerId].overflow++;
		}

		/* reset the counter value */
		WritePMUReg(PXA2_PMU_CCNT, es[COUNTER_PXA2_PMU_CCNT].reset_value);
	}

	for (i=0; i<PXA2_PMN_NUM; i++)
	{
		if (flag_value & (PMN0_OVERFLAG_BIT << i))
		{
			switch (i)
			{
			case 0: sample_rec.registerId = COUNTER_PXA2_PMU_PMN0; reg = PXA2_PMU_PMN0; break;
			case 1: sample_rec.registerId = COUNTER_PXA2_PMU_PMN1; reg = PXA2_PMU_PMN1; break;
			case 2: sample_rec.registerId = COUNTER_PXA2_PMU_PMN2; reg = PXA2_PMU_PMN2; break;
			case 3: sample_rec.registerId = COUNTER_PXA2_PMU_PMN3; reg = PXA2_PMU_PMN3; break;
			default: break;
			}

			if (es[sample_rec.registerId].calibration == false)
			{
				/* write sample record in non-calibration mode */
				buffer_full |= write_sample(0, &sample_rec);
			}
			else
			{
				/* calculate the overflow count in calibration mode */
				es[sample_rec.registerId].overflow++;
			}

			/* reset the counter value */
			WritePMUReg(reg, es[sample_rec.registerId].reset_value);
		}
	}

	if (!buffer_full)
	{
		/* enable the counters if buffer is not full */
		WritePMUReg(PXA2_PMU_PMNC, pmnc_value | PMNC_ENABLE_BIT);
	}

	/* enable the timer interrupt */
	//enable_irq(PX_IRQ_PXA2_TIMER);
	return IRQ_HANDLED;
}

static void set_ccnt_events(struct HSEventConfig *ec, struct pmu_registers_pxa2 *pmu_regs)
{
	pmu_regs->inten |= 0x1;
	pmu_regs->flag  |= 0x1;

	if (ec->eventId == PXA2_PMU_CCNT_CORE_CLOCK_TICK_64)
	{
		/* set PMNC.D bit */
		pmu_regs->pmnc |= 0x8;
	}
	else
	{
		/* clear PMNC.D bit */
		pmu_regs->pmnc &= ~0x8;
	}

	pmu_regs->ccnt = es[COUNTER_PXA2_PMU_CCNT].reset_value;
}

static void set_pmn_events(int pmn_index,
                           struct HSEventConfig *ec,
                           struct pmu_registers_pxa2 *pmu_regs)
{
	int register_id;

	switch (pmn_index)
	{
	case 0: register_id = COUNTER_PXA2_PMU_PMN0; break;
	case 1: register_id = COUNTER_PXA2_PMU_PMN1; break;
	case 2: register_id = COUNTER_PXA2_PMU_PMN2; break;
	case 3: register_id = COUNTER_PXA2_PMU_PMN3; break;
	default: return;
	}

	pmu_regs->inten |= (0x2 << pmn_index);
	pmu_regs->flag  |= (0x2 << pmn_index);

	pmu_regs->evtsel &= ~(0x000000ff << (pmn_index * 8));
	pmu_regs->evtsel |=  ec->eventId << (pmn_index * 8);

	pmu_regs->pmn[pmn_index] = es[register_id].reset_value;
}

static unsigned long long get_event_count_for_calibration_pxa2(int register_id)
{
	unsigned int value;
	unsigned long long count;

	switch (register_id)
	{
	case COUNTER_PXA2_PMU_CCNT:
	case COUNTER_PXA2_PMU_PMN0:
	case COUNTER_PXA2_PMU_PMN1:
	case COUNTER_PXA2_PMU_PMN2:
	case COUNTER_PXA2_PMU_PMN3:
		value = reg_value[register_id];
		break;
	default: return -1;
	}

	count = ((unsigned long long)es[register_id].overflow << 32) + value;
	return count;
}

static void get_settings(void)
{
	int i;

	/* get event settings */
	for (i=0; i<COUNTER_PXA2_MAX_ID; i++)
	{
		es[i].enabled     = false;
		es[i].calibration = false;
		es[i].reset_value = 0;
		es[i].overflow    = 0;
	}

	for (i=0; i<g_ebs_settings.eventNumber; i++)
	{
		int register_id;

		register_id = g_ebs_settings.event[i].registerId;
		es[register_id].enabled = true;

		if (g_calibration_mode)
		{
			es[register_id].reset_value = 0;
			es[register_id].calibration = true;
		}
		else
		{
			es[register_id].reset_value = (u32)(-1) - g_ebs_settings.event[i].eventsPerSample + 1;
			es[register_id].calibration = false;
		}
	}
}

#define OMCR_NOT_OSCR4      (0x1 << 7)
#define OMCR_MATCH_CONTINUE (0x1 << 6)
#define OMCR_NO_EXT_SYNC    (0x0)
#define OMCR_MATCH_RESET    (0x1 << 3)
#define OMCR_FREQ_32K       (0x1)           /* 32768 Hz */
#define OMCR_DISABLED       (0x7)

#define PX_TIMER_INIT_VALUE     (OMCR_NOT_OSCR4 | OMCR_MATCH_CONTINUE | OMCR_NO_EXT_SYNC | OMCR_MATCH_RESET)

static irqreturn_t px_timer_isr(unsigned int pid,
                                unsigned int tid,
                                unsigned int pc,
                                unsigned long long ts)
{
	bool buffer_full = false;

	if (OSSR & OSSR_PX_INTERRUPT)
	{
		PXD32_Hotspot_Sample_V2 sample_rec;

		/* disable the counter */
		OMCR5 &= ~0x7;

		/* clear the interrupt flag */
		OSSR = OSSR_PX_INTERRUPT;

		/* write sample record to sample buffer */
		sample_rec.pc  = pc;
		sample_rec.pid = pid;
		sample_rec.tid = tid;
		sample_rec.registerId = COUNTER_PXA2_OS_TIMER;
		memcpy(sample_rec.timestamp, &ts, sizeof(sample_rec.timestamp));

		buffer_full = write_sample(0, &sample_rec);

		if (buffer_full)
		{
			/* if sample buffer is full, pause sampling */
			OMCR5 &= ~0x7;
		}
		else
		{
			/* enable the counter */
			OMCR5 |= OMCR_FREQ_32K;
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

#if 0
static unsigned long long get_timestamp(void)
{
	/*
	 * call do_gettimeofday() in ISR will call kernel crash somtime.
	 * root cause is unknown, maybe a bug in IPM in PXA3xx
	 * so we read OSCR0 for the timestamp instead
	 */
#if 0
	unsigned long long ts;
	struct timeval tv;
	struct timespec t;

	do_gettimeofday(&tv);

	ts = (u64)tv.tv_sec * USEC_PER_SEC + tv.tv_usec;

	return ts;
#else
	/* we make the timestamp freq as 1MHz, so we need to get OSCR0 multiplied 3.25 */
	return OSCR0 / 13 * 4;
#endif
}
#endif

unsigned long get_timestamp_freq(void)
{
	return USEC_PER_SEC;
}

irqreturn_t px_hotspot_isr(int irq, void * dev)
{
	struct pt_regs *regs;

	unsigned int pid;
	unsigned int tid;
	unsigned int pc;
	unsigned long long ts;
	unsigned long flags;
	irqreturn_t ret;

	local_irq_save(flags);

	ret = IRQ_NONE;

	regs = get_irq_regs();

	pid = current->tgid;
	tid = current->pid;
	pc  = regs->ARM_pc;
	ts  = get_timestamp();

	if (irq == PX_IRQ_PXA2_TIMER)
	{
		ret = px_timer_isr(pid, tid, pc, ts);
	}
	else
	{
		ret = px_pmu_isr(pid, tid, pc, ts);
	}

	local_irq_restore(flags);

	return ret;
}

static int start_pmu(bool is_start_paused)
{
	int i;
	int ret;

	struct pmu_registers_pxa2 pmu_regs;

	pmu_regs.pmnc  = 0x0;
	pmu_regs.inten = 0x0;
	pmu_regs.flag  = 0x0;
	pmu_regs.ccnt  = 0x0;

	for (i=0; i<PXA2_PMN_NUM; i++)
		pmu_regs.pmn[i] = 0;

	pmu_regs.evtsel = 0x0;

	ret = allocate_pmu();

	if (ret != 0)
	{
		return ret;
	}

	ret = register_pmu_isr(px_hotspot_isr);

	if (ret != 0)
	{
		return ret;
	}

	pmu_regs.pmnc = 0x10;
	WritePMUReg(PXA2_PMU_PMNC, pmu_regs.pmnc);

	for (i=0; i<g_ebs_settings.eventNumber; i++)
	{
		switch (g_ebs_settings.event[i].registerId)
		{
		case COUNTER_PXA2_PMU_CCNT:
			set_ccnt_events(&g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PXA2_PMU_PMN0:
			set_pmn_events(0, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PXA2_PMU_PMN1:
			set_pmn_events(1, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PXA2_PMU_PMN2:
			set_pmn_events(2, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PXA2_PMU_PMN3:
			set_pmn_events(3, &g_ebs_settings.event[i], &pmu_regs);
			break;

		default:
			break;
		}
	}

	if (!is_start_paused)
	{
		pmu_regs.pmnc &= 0x0f;
		pmu_regs.pmnc |= 0x1;
	}

	WritePMUReg(PXA2_PMU_CCNT, pmu_regs.ccnt);
	WritePMUReg(PXA2_PMU_PMN0, pmu_regs.pmn[0]);
	WritePMUReg(PXA2_PMU_PMN1, pmu_regs.pmn[1]);
	WritePMUReg(PXA2_PMU_PMN2, pmu_regs.pmn[2]);
	WritePMUReg(PXA2_PMU_PMN3, pmu_regs.pmn[3]);

	WritePMUReg(PXA2_PMU_EVTSEL, pmu_regs.evtsel);
	WritePMUReg(PXA2_PMU_FLAG, pmu_regs.flag);
	WritePMUReg(PXA2_PMU_INTEN, pmu_regs.inten);
	WritePMUReg(PXA2_PMU_PMNC, pmu_regs.pmnc);

	return 0;
}

static int stop_pmu(void)
{
	unsigned int inten_value;
	unsigned int pmnc_value;
	unsigned int flag_value;

	inten_value  = 0x0;
	flag_value = 0x1f;

	pmnc_value = ReadPMUReg(PXA2_PMU_PMNC);

	/* disable the counters */
	pmnc_value |= 0x10;
	pmnc_value &= ~0x1;

	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);
	WritePMUReg(PXA2_PMU_INTEN, inten_value);
	WritePMUReg(PXA2_PMU_FLAG, flag_value);

	/* We need to save the PMU counter value for calibration result before calling free_pmu()
 	 * because free_pmu() may cause these registers be modified by IPM */
 	reg_value[COUNTER_PXA2_PMU_CCNT] = ReadPMUReg(PXA2_PMU_CCNT);
	reg_value[COUNTER_PXA2_PMU_PMN0] = ReadPMUReg(PXA2_PMU_PMN0);
	reg_value[COUNTER_PXA2_PMU_PMN1] = ReadPMUReg(PXA2_PMU_PMN1);
	reg_value[COUNTER_PXA2_PMU_PMN2] = ReadPMUReg(PXA2_PMU_PMN2);
	reg_value[COUNTER_PXA2_PMU_PMN3] = ReadPMUReg(PXA2_PMU_PMN3);

	unregister_pmu_isr();

	free_pmu();

	return 0;
}

static int pause_pmu(void)
{
	unsigned int flag_value = 0;
	unsigned int pmnc_value;

	pmnc_value = ReadPMUReg(PXA2_PMU_PMNC);

	/* disable the counters */
	pmnc_value |= 0x10;
	pmnc_value &= ~0x1;

	/* clear the overflow flags */
	flag_value = 0x1f;

	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);
	WritePMUReg(PXA2_PMU_FLAG, flag_value);

	return 0;
}

static int resume_pmu(void)
{
	unsigned int pmnc_value;

	pmnc_value = ReadPMUReg(PXA2_PMU_PMNC);

	pmnc_value |= 0x1;
	pmnc_value &= ~0x10;

	WritePMUReg(PXA2_PMU_PMNC, pmnc_value);

	return 0;
}

static int start_tbs(bool is_start_paused)
{
	u32 offset = g_tbs_settings.interval / 100 * 32768 / 10000;

	OMCR5 = PX_TIMER_INIT_VALUE;
	OIER &= ~OSSR_PX_INTERRUPT;

	/* set match offset */
	OSMR5 = offset;
	result = request_irq(PX_IRQ_PXA2_TIMER, px_hotspot_isr, IRQF_TIMER|IRQF_SHARED, "CPA Timer", &dev_oscr);

	if (result != 0)
	{
		printk(KERN_ALERT "[CPA] Request IRQ returns 0x%x!\n", result);
	}

	/* a write operation starts the counter */
	OSCR5 = 0;

	/* enable interrupt for channel 5, OSCR5 */
	OIER |= OSSR_PX_INTERRUPT;

	/* select frequency and enable the counter */
	if (!is_start_paused)
		OMCR5 |= OMCR_FREQ_32K;
	else
		OMCR5 &= ~0x7;

	return 0;
}

static int stop_tbs(void)
{
	OMCR5 &= ~0x7;

	free_irq(PX_IRQ_PXA2_TIMER, &dev_oscr);

	OIER &= ~OSSR_PX_INTERRUPT;

	/* clear the interrupt flag */
	OSSR = OSSR_PX_INTERRUPT;
	return 0;
}

static int pause_tbs(void)
{
	OMCR5 &= ~0x7;

	return 0;
}

static int resume_tbs(void)
{
	OMCR5 |= OMCR_FREQ_32K;

	return 0;

}

static bool is_pmu_enabled(void)
{
	if (es[COUNTER_PXA2_PMU_CCNT].enabled)
		return true;

	if (es[COUNTER_PXA2_PMU_PMN0].enabled)
		return true;

	if (es[COUNTER_PXA2_PMU_PMN1].enabled)
		return true;

	if (es[COUNTER_PXA2_PMU_PMN2].enabled)
		return true;

	if (es[COUNTER_PXA2_PMU_PMN3].enabled)
		return true;

	return false;
}

static int start_ebs(bool is_start_paused)
{
	int ret;

	get_settings();

	if (is_pmu_enabled())
	{
		if ((ret = start_pmu(is_start_paused)) != 0)
			return ret;
	}

	return 0;
}

static int stop_ebs(void)
{
	int ret;

	if (is_pmu_enabled())
	{
		if ((ret = stop_pmu()) != 0)
			return ret;
	}

	return 0;
}

static int pause_ebs(void)
{
	int ret;

	if (is_pmu_enabled())
	{
		if ((ret = pause_pmu()) != 0)
			return ret;
	}

	return 0;
}

static int resume_ebs(void)
{
	int ret;

	if (is_pmu_enabled())
	{
		if ((ret = resume_pmu()) != 0)
			return ret;
	}

	return 0;
}

static int start_sampling_pxa2(bool is_start_paused)
{
	int ret;

	/*
	 * config tbs sampling with OSCR5
	 * don't start TBS in calibration mode
	 */
	if ((g_tbs_settings.interval != 0) && !g_calibration_mode)
	{
		if ((ret = start_tbs(is_start_paused)) != 0)
			return ret;
	}

	/*
	 * config ebs sampling
	 */
	if (g_ebs_settings.eventNumber != 0)
	{
		if ((ret = start_ebs(is_start_paused)) != 0)
			return ret;
	}

	return 0;
}

static int stop_sampling_pxa2(void)
{
	/* config tbs sampling with OSCR5 */
	if ((g_tbs_settings.interval != 0) && !g_calibration_mode)
	{
		int reterr = stop_tbs();
		if (reterr)
			return reterr;
	}

	/* config ebs sampling */
	if (g_ebs_settings.eventNumber != 0)
	{
		int reterr = stop_ebs();
		if (reterr)
			return reterr;
	}

	return 0;
}

static int pause_sampling_pxa2(void)
{
	if ((g_tbs_settings.interval != 0) && !g_calibration_mode)
	{
		// Disable timer
		int reterr = pause_tbs();
		if (reterr)
			return reterr;
	}

	// config ebs sampling
	if (g_ebs_settings.eventNumber != 0)
	{
		int reterr = pause_ebs();
		if (reterr)
			return reterr;
	}

	return 0;
}

static int resume_sampling_pxa2(void)
{
	if ((g_tbs_settings.interval != 0) && !g_calibration_mode)
	{
		// Enable timer
		int reterr = resume_tbs();
		if (reterr)
			return reterr;
	}

	// config ebs sampling
	if (g_ebs_settings.eventNumber != 0)
	{
		int reterr = resume_ebs();
		if (reterr)
			return reterr;
	}

	return 0;
}

struct hs_sampling_op_mach hs_sampling_op_pxa2 =
{
	.start  = start_sampling_pxa2,
	.stop   = stop_sampling_pxa2,
	.pause  = pause_sampling_pxa2,
	.resume = resume_sampling_pxa2,
	.get_count_for_cal = get_event_count_for_calibration_pxa2,
};
