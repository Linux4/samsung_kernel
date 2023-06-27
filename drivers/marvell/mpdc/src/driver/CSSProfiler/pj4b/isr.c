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
 * PJ4B CSS ISR implementation
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

#include "PXD_css.h"
#include "CSSProfilerSettings.h"
#include "pmu_pj4b.h"
#include "css_drv.h"
#include "common.h"

extern struct CSSEventSettings g_ebs_settings;
extern struct CSSTimerSettings g_tbs_settings;
extern bool g_calibration_mode;

extern int start_tbs(bool is_start_paused);
extern int stop_tbs(void);
extern int pause_tbs(void);
extern int resume_tbs(void);

struct event_settings
{
	bool   enabled;
	bool   calibration;
	u32    reset_value;   /* reset value for the register */
	u32    overflow;      /* overflow number */
};

extern unsigned int get_timer_irq(void);

static struct event_settings es[COUNTER_PJ4B_MAX_ID];
static u32 reg_value[COUNTER_PJ4B_MAX_ID];

unsigned long get_timestamp_freq(void)
{
	return USEC_PER_SEC;
}

static irqreturn_t px_pmu_isr(struct pt_regs * const regs,
                              unsigned int pid,
                              unsigned int tid,
                              unsigned int cpu,
                              unsigned long long ts)
{
	u32 pmnc_value;
	u32 flag_value;
	unsigned int i;
	unsigned int reg_id;
	bool buffer_full = false;
	char ** bt_buffer = &per_cpu(g_bt_buffer, cpu);
	
	PXD32_CSS_Call_Stack_V2 *css_data = (PXD32_CSS_Call_Stack_V2 *)*bt_buffer;

	/* disable the counters */
	pmnc_value = PJ4B_Read_PMNC();
	pmnc_value &= ~0x1;
	PJ4B_Write_PMNC(pmnc_value);

	/* clear the overflow flag */
	flag_value = PJ4B_Read_OVSR();
	PJ4B_Write_OVSR(0xffffffff);
	
	backtrace(regs, cpu, pid, tid, css_data);

	if ((flag_value & 0x80000000) && es[COUNTER_PJ4B_PMU_CCNT].enabled)
	{
		reg_id = COUNTER_PJ4B_PMU_CCNT;

		/* ccnt overflow */
		if (es[reg_id].calibration == false)
		{
			/* write css data in non-calibration mode */
			if (!buffer_full)
			{
				fill_css_data_head(css_data, pid, tid, reg_id, ts);

				buffer_full |= write_css_data(cpu, css_data);
			}
		}
		else
		{
			/* calculate the overflow count in calibration mode */
			es[reg_id].overflow++;
		}

		PJ4B_WritePMUCounter(reg_id, es[reg_id].reset_value);
	}

	for (i=0; i<PJ4B_PMN_NUM; i++)
	{
		if (flag_value & (0x1 << i))
		{
			switch (i)
			{
			case 0:	reg_id = COUNTER_PJ4B_PMU_PMN0; break;
			case 1:	reg_id = COUNTER_PJ4B_PMU_PMN1; break;
			case 2:	reg_id = COUNTER_PJ4B_PMU_PMN2; break;
			case 3:	reg_id = COUNTER_PJ4B_PMU_PMN3; break;
			case 4:	reg_id = COUNTER_PJ4B_PMU_PMN4; break;
			case 5:	reg_id = COUNTER_PJ4B_PMU_PMN5; break;
			default: break;
			}

			if (es[reg_id].calibration == false)
			{
				/* write css data in non-calibration mode */
				if (!buffer_full)
				{
					fill_css_data_head(css_data, pid, tid, reg_id, ts);

					buffer_full |= write_css_data(cpu, css_data);
				}
			}
			else
			{
				/* calculate the overflow count in calibration mode */
				es[reg_id].overflow++;
			}

			PJ4B_WritePMUCounter(reg_id, es[reg_id].reset_value);
		}

	}

	if (!buffer_full)
	{
		/* enable the counters */
		pmnc_value |= 0x1;
		PJ4B_Write_PMNC(pmnc_value);
	}

	return IRQ_HANDLED;
}

static irqreturn_t px_css_isr(int irq, void * dev)
{
	struct pt_regs *regs;

	unsigned int pid;
	unsigned int tid;
	unsigned int cpu;
	unsigned long flags;
	unsigned long long ts;
	irqreturn_t ret;

	local_irq_save(flags);

	ret = IRQ_NONE;

	regs = get_irq_regs();

	pid = current->tgid;
	tid = current->pid;
	cpu = smp_processor_id();
	
	ts = get_timestamp();

	ret = px_pmu_isr(regs, pid, tid, cpu, ts);

	local_irq_restore(flags);

	return ret;
}

static void set_ccnt_events(struct CSSEventConfig *ec, struct pmu_registers_pj4b *pmu_regs)
{
	if (ec->eventId == PJ4B_PMU_CCNT_CORE_CLOCK_TICK_64)
	{
		/* 64 clock cycle */
		pmu_regs->pmnc |= 0x8;
	}
	else
	{
		pmu_regs->pmnc &= ~0x8;
	}

	/* reset CCNT */
	//pmu_regs->pmnc |= 0x4;

	pmu_regs->cntenset |= 0x1 << 31;
	pmu_regs->cntenclr |= 0x1 << 31;
	pmu_regs->ovsr     |= 0x1 << 31;
	pmu_regs->intenset |= 0x1 << 31;
	pmu_regs->intenclr |= 0x1 << 31;

	pmu_regs->ccnt = es[COUNTER_PJ4B_PMU_CCNT].reset_value;
}

static void set_pmn_events(int pmn_index,
                           struct CSSEventConfig *ec,
                           struct pmu_registers_pj4b *pmu_regs)
{
	int register_id;
	int neon_group_id;

	switch (pmn_index)
	{
	case 0: register_id = COUNTER_PJ4B_PMU_PMN0; break;
	case 1: register_id = COUNTER_PJ4B_PMU_PMN1; break;
	case 2: register_id = COUNTER_PJ4B_PMU_PMN2; break;
	case 3: register_id = COUNTER_PJ4B_PMU_PMN3; break;
	case 4: register_id = COUNTER_PJ4B_PMU_PMN4; break;
	case 5: register_id = COUNTER_PJ4B_PMU_PMN5; break;
	default: return;
	}

	/* reset PMN counter */
	//pmu_regs->pmnc |= 0x2;

	pmu_regs->intenset |= (0x1 << pmn_index);
	pmu_regs->intenclr |= (0x1 << pmn_index);
	pmu_regs->ovsr     |= (0x1 << pmn_index);

	pmu_regs->cntenclr |= (0x1 << pmn_index);
	pmu_regs->cntenset |= (0x1 << pmn_index);

	pmu_regs->evtsel[pmn_index] = get_pmu_event_id(ec->eventId);

	pmu_regs->pmncnt[pmn_index] = es[register_id].reset_value;

	/* check if it is a neon event */
	if (is_neon_event(ec->eventId, &neon_group_id))
	{
		enable_neon_event_group(neon_group_id);
	}
}

static unsigned long long get_event_count_for_calibration_pj4b(int register_id)
{
	unsigned long long value;

	switch (register_id)
	{
	case COUNTER_PJ4B_PMU_CCNT:
	case COUNTER_PJ4B_PMU_PMN0:
	case COUNTER_PJ4B_PMU_PMN1:
	case COUNTER_PJ4B_PMU_PMN2:
	case COUNTER_PJ4B_PMU_PMN3:
	case COUNTER_PJ4B_PMU_PMN4:
	case COUNTER_PJ4B_PMU_PMN5:
		value = reg_value[register_id];
		break;

	default:
		return -1;
	}

	return ((unsigned long long)es[register_id].overflow << 32) + value;
}

static void get_settings(void)
{
	int i;

	/* get event settings */
	for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
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

static void start_pmu(void * data)
{
	int i;

	bool is_start_paused;
	struct pmu_registers_pj4b pmu_regs;

	is_start_paused = *(bool *)data;

	pmu_regs.pmnc     = 0x0;
	pmu_regs.ccnt     = 0x0;
	pmu_regs.cntenset = 0x0;
	pmu_regs.cntenclr = 0x0;
	pmu_regs.intenset = 0x0;
	pmu_regs.intenclr = 0x0;
	pmu_regs.ovsr     = 0x0;

	for (i=0; i<PJ4B_PMN_NUM; i++)
	{
		pmu_regs.evtsel[i] = 0;
		pmu_regs.pmncnt[i] = 0;
	}

	/* disable PMU and clear CCNT & PMNx */
	pmu_regs.pmnc = PJ4B_Read_PMNC();
	pmu_regs.pmnc &= ~0x1;
	//pmu_regs.pmnc |= 0x6;
	PJ4B_Write_PMNC(pmu_regs.pmnc);

	for (i=0; i<g_ebs_settings.eventNumber; i++)
	{
		switch (g_ebs_settings.event[i].registerId)
		{
		case COUNTER_PJ4B_PMU_CCNT:
			set_ccnt_events(&g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ4B_PMU_PMN0:
			set_pmn_events(0, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ4B_PMU_PMN1:
			set_pmn_events(1, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ4B_PMU_PMN2:
			set_pmn_events(2, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ4B_PMU_PMN3:
			set_pmn_events(3, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ4B_PMU_PMN4:
			set_pmn_events(4, &g_ebs_settings.event[i], &pmu_regs);
			break;

		case COUNTER_PJ4B_PMU_PMN5:
			set_pmn_events(5, &g_ebs_settings.event[i], &pmu_regs);
			break;

		default:
			break;
		}

	}

	/* disable all counters */
	PJ4B_Write_CNTENCLR(0xffffffff);

	/* disable all interrupts */
	PJ4B_Write_INTENCLR(0xffffffff);

	/* clear overflow flags */
	PJ4B_Write_OVSR(0xffffffff);

	/* write the counter values */
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
		PJ4B_Write_PMNCNT(pmu_regs.pmncnt[i]);
		PJ4B_Write_EVTSEL(pmu_regs.evtsel[i]);
	}

	if (is_start_paused)
	{
		pmu_regs.pmnc &= ~0x1;
	}
	else
	{
		pmu_regs.pmnc |= 0x1;
	}


	/* enable the interrupts */
	PJ4B_Write_INTENSET(pmu_regs.intenset);

	/* enable the counters */
	PJ4B_Write_CNTENSET(pmu_regs.cntenset);

	/* Enable PMU */
	PJ4B_Write_PMNC(pmu_regs.pmnc);
}

static void stop_pmu(void * data)
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

	/* We need to save the PMU counter value for calibration result before calling free_pmu()
 	 * because free_pmu() may cause these registers be modified by IPM */
 	reg_value[COUNTER_PJ4B_PMU_CCNT] += PJ4B_ReadPMUCounter(COUNTER_PJ4B_PMU_CCNT);
	reg_value[COUNTER_PJ4B_PMU_PMN0] += PJ4B_ReadPMUCounter(COUNTER_PJ4B_PMU_PMN0);
	reg_value[COUNTER_PJ4B_PMU_PMN1] += PJ4B_ReadPMUCounter(COUNTER_PJ4B_PMU_PMN1);
	reg_value[COUNTER_PJ4B_PMU_PMN2] += PJ4B_ReadPMUCounter(COUNTER_PJ4B_PMU_PMN2);
	reg_value[COUNTER_PJ4B_PMU_PMN3] += PJ4B_ReadPMUCounter(COUNTER_PJ4B_PMU_PMN3);
	reg_value[COUNTER_PJ4B_PMU_PMN4] += PJ4B_ReadPMUCounter(COUNTER_PJ4B_PMU_PMN4);
	reg_value[COUNTER_PJ4B_PMU_PMN5] += PJ4B_ReadPMUCounter(COUNTER_PJ4B_PMU_PMN5);
}

static void pause_pmu(void * data)
{
	u32 pmnc_value;

	/* disable all counters */
	pmnc_value = PJ4B_Read_PMNC();
	pmnc_value &= ~0x1;
	PJ4B_Write_PMNC(pmnc_value);
}

static void resume_pmu(void * data)
{
	u32 pmnc_value;

	/* enable all counters */
	pmnc_value = PJ4B_Read_PMNC();
	pmnc_value |= 0x1;
	PJ4B_Write_PMNC(pmnc_value);
}

static bool is_pmu_enabled(void)
{
	if (es[COUNTER_PJ4B_PMU_CCNT].enabled)
		return true;

	if (es[COUNTER_PJ4B_PMU_PMN0].enabled)
		return true;

	if (es[COUNTER_PJ4B_PMU_PMN1].enabled)
		return true;

	if (es[COUNTER_PJ4B_PMU_PMN2].enabled)
		return true;

	if (es[COUNTER_PJ4B_PMU_PMN3].enabled)
		return true;

	if (es[COUNTER_PJ4B_PMU_PMN4].enabled)
		return true;

	if (es[COUNTER_PJ4B_PMU_PMN5].enabled)
		return true;

	return false;
}

static int start_ebs(bool is_start_paused)
{
	int i;
	int ret;

	get_settings();

	if (is_pmu_enabled())
	{
		ret = allocate_pmu();

		if (ret != 0)
		{
			return ret;
		}

		ret = register_pmu_isr(px_css_isr);

		if (ret != 0)
		{
			return ret;
		}

		for (i=0; i<COUNTER_PJ4B_MAX_ID; i++)
			reg_value[i] = 0;

		on_each_cpu(start_pmu, &is_start_paused, 1);
	}

	return 0;
}

static int stop_ebs(void)
{
	int ret;

	if (is_pmu_enabled())
	{
		on_each_cpu(stop_pmu, NULL, 1);

		ret = unregister_pmu_isr();
		if (ret != 0)
		{
			return ret;
		}

		ret = free_pmu();
		if (ret != 0)
		{
			return ret;
		}
	}

	return 0;
}

static int pause_ebs(void)
{
	if (is_pmu_enabled())
	{
		on_each_cpu(pause_pmu, NULL, 1);
	}

	return 0;
}

static int resume_ebs(void)
{
	if (is_pmu_enabled())
	{
		on_each_cpu(resume_pmu, NULL, 1);
	}

	return 0;
}

static int start_sampling_pj4b(bool is_start_paused)
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

static int stop_sampling_pj4b(void)
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

static int pause_sampling_pj4b(void)
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

static int resume_sampling_pj4b(void)
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

struct css_sampling_op_mach css_sampling_op_pj4b =
{
	.start  = start_sampling_pj4b,
	.stop   = stop_sampling_pj4b,
	.pause  = pause_sampling_pj4b,
	.resume = resume_sampling_pj4b,
	.get_count_for_cal = get_event_count_for_calibration_pj4b,
};
