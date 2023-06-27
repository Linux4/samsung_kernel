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
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq_regs.h>

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <mach/hardware.h>
#else
#include <asm/hardware.h>
#endif

#include "PXD_hs.h"
#include "HSProfilerSettings.h"
#include "pxpmu.h"
#include "pmu_pj1.h"
#include "hs_drv.h"
#include "common.h"

extern int start_tbs(bool is_start_paused);
extern int stop_tbs(void);
extern int pause_tbs(void);
extern int resume_tbs(void);

struct event_settings
{
	bool   enabled;
	bool   calibration;
	u64    reset_value;   /* reset value for the register */
	u32    overflow;      /* overflow number */
};

#ifdef HW_TIMER
extern irqreturn_t px_timer_isr(unsigned int pid, unsigned int tid, unsigned int pc, unsigned long long ts);
extern unsigned int get_timer_irq(void);
#endif

static struct event_settings es[COUNTER_PJ1_MAX_ID];
static u64 reg_value[COUNTER_PJ1_MAX_ID];

unsigned long get_timestamp_freq(void)
{
	return USEC_PER_SEC;
}

static irqreturn_t px_pmu_isr(unsigned int pid,
                              unsigned int tid,
                              unsigned int pc,
                              unsigned long long timestamp)
{
	unsigned int cor_value[PJ1_PMN_NUM];
	unsigned int flag_value;
	unsigned int i;
	unsigned int reg = 0;
	bool buffer_full = false;

	PXD32_Hotspot_Sample_V2 sample_rec;

	/* disable the counters */
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

	/* clear the overflow flag */
	flag_value = PJ1_ReadFLAG();
	PJ1_WriteFLAG(0xf);

	sample_rec.pc = pc;
	sample_rec.pid = pid;
	sample_rec.tid = tid;
	memcpy(sample_rec.timestamp, &timestamp, sizeof(sample_rec.timestamp));

	for (i=0; i<PJ1_PMN_NUM; i++)
	{
		if (flag_value & (0x1 << i))
		{
			switch (i)
			{
			case 0:	sample_rec.registerId = COUNTER_PJ1_PMU_PMN0; reg = PJ1_PMU_PMN0; break;
			case 1:	sample_rec.registerId = COUNTER_PJ1_PMU_PMN1; reg = PJ1_PMU_PMN1; break;
			case 2:	sample_rec.registerId = COUNTER_PJ1_PMU_PMN2; reg = PJ1_PMU_PMN2; break;
			case 3:	sample_rec.registerId = COUNTER_PJ1_PMU_PMN3; reg = PJ1_PMU_PMN3; break;
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

			PJ1_WriteCounter(reg, es[sample_rec.registerId].reset_value);
		}

	}

	if (!buffer_full)
	{
		/* enable the counters if sample buffer is not full */
		cor_value[0] = PJ1_ReadCOR(PJ1_PMU_COR0);
		cor_value[1] = PJ1_ReadCOR(PJ1_PMU_COR1);
		cor_value[2] = PJ1_ReadCOR(PJ1_PMU_COR2);
		cor_value[3] = PJ1_ReadCOR(PJ1_PMU_COR3);

		for (i=0; i<PJ1_PMN_NUM; i++)
		{
			cor_value[i] |= 0x1;
		}

		PJ1_WriteCOR(PJ1_PMU_COR0, cor_value[0]);
		PJ1_WriteCOR(PJ1_PMU_COR1, cor_value[1]);
		PJ1_WriteCOR(PJ1_PMU_COR2, cor_value[2]);
		PJ1_WriteCOR(PJ1_PMU_COR3, cor_value[3]);
	}

	return IRQ_HANDLED;
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

#ifdef HW_TIMER
	if (irq == get_timer_irq())
	{
		ret = px_timer_isr(pid, tid, pc, ts);
	}
	else
#endif
	{
		ret = px_pmu_isr(pid, tid, pc, ts);
	}

	local_irq_restore(flags);

	return ret;
}

static void set_pmn_events(int pmn_index,
                           struct HSEventConfig *ec,
                           struct pmu_registers_pj1 *pmu_regs)
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

	pmu_regs->inten |= (0x1 << pmn_index);
	pmu_regs->flag  |= (0x1 << pmn_index);

	pmu_regs->cor[pmn_index] = get_pmu_event_id(ec->eventId);

	pmu_regs->pmn[pmn_index] = es[register_id].reset_value;
}

static unsigned long long get_event_count_for_calibration_pj1(int register_id)
{
	unsigned long long value;

	switch (register_id)
	{
	case COUNTER_PJ1_PMU_PMN0:
	case COUNTER_PJ1_PMU_PMN1:
	case COUNTER_PJ1_PMU_PMN2:
	case COUNTER_PJ1_PMU_PMN3:
		value = reg_value[register_id];
		break;
	default: return -1;
	}

	return value;
}

static void get_settings(void)
{
	int i;

	/* get event settings */
	for (i=0; i<COUNTER_PJ1_MAX_ID; i++)
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
			es[register_id].reset_value = (u64)(-1) - g_ebs_settings.event[i].eventsPerSample + 1;
			es[register_id].calibration = false;
		}
	}
}

#define AIRQ_BASE 0xf4031000
#define airq_writel(val, reg) (*(unsigned int*)(AIRQ_BASE + (reg)) = (val))
#define airq_readl(reg)       (*(unsigned int*)(AIRQ_BASE + (reg)))
#define airq_writew(val, reg) (*(unsigned short*)(AIRQ_BASE + (reg)) = (val))
#define airq_readw(reg)       (*(unsigned short*)(AIRQ_BASE + (reg)))

static int start_pmu(bool is_start_paused)
{
	int i;
	int ret;

	struct pmu_registers_pj1 pmu_regs;

	pmu_regs.inten = 0x0;
	pmu_regs.flag = 0x0;
	
	for (i=0; i<PJ1_PMN_NUM; i++)
	{
		pmu_regs.cor[i] = 0;
		pmu_regs.pmn[i] = 0;
	}

	/* There is no PMU interrupt in Avanta */
#if !defined(PX_SOC_AVANTA)
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
#endif

	for (i=0; i<g_ebs_settings.eventNumber; i++)
	{
		switch (g_ebs_settings.event[i].registerId)
		{
		case COUNTER_PJ1_PMU_PMN0:
			set_pmn_events(0, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ1_PMU_PMN1:
			set_pmn_events(1, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ1_PMU_PMN2:
			set_pmn_events(2, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ1_PMU_PMN3:
			set_pmn_events(3, &g_ebs_settings.event[i], &pmu_regs);
			break;

		default:
			break;
		}

	}

	/* disable all counters */
	PJ1_WriteCOR(PJ1_PMU_COR0, 0);
	PJ1_WriteCOR(PJ1_PMU_COR1, 0);
	PJ1_WriteCOR(PJ1_PMU_COR2, 0);
	PJ1_WriteCOR(PJ1_PMU_COR3, 0);

	/* disable the interrupts */
	PJ1_WriteINTEN(0x0);

	/* clear the overflow flags */
	PJ1_WriteFLAG(0xf);

	/* write the counter values */
	PJ1_WriteCounter(PJ1_PMU_PMN0, pmu_regs.pmn[0]);
	PJ1_WriteCounter(PJ1_PMU_PMN1, pmu_regs.pmn[1]);
	PJ1_WriteCounter(PJ1_PMU_PMN2, pmu_regs.pmn[2]);
	PJ1_WriteCounter(PJ1_PMU_PMN3, pmu_regs.pmn[3]);

	/* enable the interrupts */
	PJ1_WriteINTEN(pmu_regs.inten);

	/* set events and enable the counters */
	if (is_start_paused)
	{
		for (i=0; i<PJ1_PMN_NUM; i++)
		{
			pmu_regs.cor[i] &= ~0x1;
		}
	}
	else
	{
		if (es[COUNTER_PJ1_PMU_PMN0].enabled)
			pmu_regs.cor[0] |= 0x1;

		if (es[COUNTER_PJ1_PMU_PMN1].enabled)
			pmu_regs.cor[1] |= 0x1;

		if (es[COUNTER_PJ1_PMU_PMN2].enabled)
			pmu_regs.cor[2] |= 0x1;

		if (es[COUNTER_PJ1_PMU_PMN3].enabled)
			pmu_regs.cor[3] |= 0x1;
/*
		for (i=0; i<PJ1_PMN_NUM; i++)
		{
			pmu_regs.cor[i] |= 0x1;
		}
*/
	}

	PJ1_WriteCOR(PJ1_PMU_COR0, pmu_regs.cor[0]);
	PJ1_WriteCOR(PJ1_PMU_COR1, pmu_regs.cor[1]);
	PJ1_WriteCOR(PJ1_PMU_COR2, pmu_regs.cor[2]);
	PJ1_WriteCOR(PJ1_PMU_COR3, pmu_regs.cor[3]);


	return 0;
}

static int stop_pmu(void)
{
	int i;
	u32 cor_value[PJ1_PMN_NUM];
	u32 inten_value;
	u32 flag_value;


	inten_value  = 0x0;
	flag_value = 0xf;

	/* disable all counters */
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



	/* clear overflow flags */
	PJ1_WriteFLAG(flag_value);

	/* disable all interrupts */
	PJ1_WriteINTEN(inten_value);

	/* We need to save the PMU counter value for calibration result before calling free_pmu()
 	 * because free_pmu() may cause these registers be modified by IPM */
	reg_value[COUNTER_PJ1_PMU_PMN0] = PJ1_ReadCounter(PJ1_PMU_PMN0);
	reg_value[COUNTER_PJ1_PMU_PMN1] = PJ1_ReadCounter(PJ1_PMU_PMN1);
	reg_value[COUNTER_PJ1_PMU_PMN2] = PJ1_ReadCounter(PJ1_PMU_PMN2);
	reg_value[COUNTER_PJ1_PMU_PMN3] = PJ1_ReadCounter(PJ1_PMU_PMN3);

	/* There is no PMU interrupt in Avanta */
#if !defined(PX_SOC_AVANTA)
	unregister_pmu_isr();
#endif

	free_pmu();
	
	return 0;
}

static int pause_pmu(void)
{
	unsigned int i;
	unsigned int cor_value[PJ1_PMN_NUM];

	/* disable all counters */
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

	return 0;
}

static int resume_pmu(void)
{
	unsigned int cor_value[PJ1_PMN_NUM];

	/* enable the counters */
	cor_value[0] = PJ1_ReadCOR(PJ1_PMU_COR0);
	cor_value[1] = PJ1_ReadCOR(PJ1_PMU_COR1);
	cor_value[2] = PJ1_ReadCOR(PJ1_PMU_COR2);
	cor_value[3] = PJ1_ReadCOR(PJ1_PMU_COR3);

	if (es[COUNTER_PJ1_PMU_PMN0].enabled)
		cor_value[0] |= 0x1;

	if (es[COUNTER_PJ1_PMU_PMN1].enabled)
		cor_value[1] |= 0x1;

	if (es[COUNTER_PJ1_PMU_PMN2].enabled)
		cor_value[2] |= 0x1;

	if (es[COUNTER_PJ1_PMU_PMN3].enabled)
		cor_value[3] |= 0x1;

/*
	for (i=0; i<PJ1_PMN_NUM; i++)
	{
		cor_value[i] |= 0x1;
	}
*/
	PJ1_WriteCOR(PJ1_PMU_COR0, cor_value[0]);
	PJ1_WriteCOR(PJ1_PMU_COR1, cor_value[1]);
	PJ1_WriteCOR(PJ1_PMU_COR2, cor_value[2]);
	PJ1_WriteCOR(PJ1_PMU_COR3, cor_value[3]);

	return 0;
}

static bool is_pmu_enabled(void)
{
	if (es[COUNTER_PJ1_PMU_PMN0].enabled)
		return true;

	if (es[COUNTER_PJ1_PMU_PMN1].enabled)
		return true;

	if (es[COUNTER_PJ1_PMU_PMN2].enabled)
		return true;

	if (es[COUNTER_PJ1_PMU_PMN3].enabled)
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

static int start_sampling_pj1(bool is_start_paused)
{
	// config tbs sampling with ap timer 2
	if ((g_tbs_settings.interval != 0) && !g_calibration_mode)
	{
		int reterr = start_tbs(is_start_paused);
		if (reterr)
			return reterr;
	}

	// config ebs sampling
	if (g_ebs_settings.eventNumber != 0)
	{
		int reterr = start_ebs(is_start_paused);
		if (reterr)
			return reterr;
	}

	return 0;
}

static int stop_sampling_pj1(void)
{
	// config tbs sampling with ap timer 2
	if ((g_tbs_settings.interval != 0) && !g_calibration_mode)
	{
		int reterr = stop_tbs();
		if (reterr)
			return reterr;
	}

	// config ebs sampling
	if (g_ebs_settings.eventNumber != 0)
	{
		int reterr = stop_ebs();
		if (reterr)
			return reterr;
	}

	return 0;
}

static int pause_sampling_pj1(void)
{
	if ((g_tbs_settings.interval != 0) && !g_calibration_mode)
	{
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

static int resume_sampling_pj1(void)
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

struct hs_sampling_op_mach hs_sampling_op_pj1 =
{
	.start  = start_sampling_pj1,
	.stop   = stop_sampling_pj1,
	.pause  = pause_sampling_pj1,
	.resume = resume_sampling_pj1,
	.get_count_for_cal = get_event_count_for_calibration_pj1,
};
