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
 * ARM A7 Hotspot ISR implementation
 */

#include <linux/types.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/cpu.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq_regs.h>
#include <asm/pmu.h>
#include <linux/cpu_pm.h>

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <mach/hardware.h>
#else
#include <asm/hardware.h>
#endif

#include "PXD_hs.h"
#include "HSProfilerSettings.h"
#include "pmu_a7.h"
#include "hs_drv.h"
#include "common.h"

extern int start_tbs_hs(bool is_start_paused);
extern int stop_tbs_hs(void);
extern int pause_tbs_hs(void);
extern int resume_tbs_hs(void);

/* add for PXA1088 platform, Need to init CTI irq line */
#ifdef PX_SOC_PXA1088
typedef void (* pxa988_ack_ctiint_t)(void);
static pxa988_ack_ctiint_t pxa988_ack_ctiint_func;
#endif

struct event_settings
{
	bool   enabled;
	bool   calibration;
	u32    reset_value;   /* reset value for the register */
	u32    overflow;      /* overflow number */
};

static struct event_settings es[COUNTER_A7_MAX_ID];
static u32 reg_value[COUNTER_A7_MAX_ID];
static bool pmu_paused;

/* structure to mark whether a cpu is profiling */
struct brun_struct
{
	spinlock_t brun_lock; /* hotplug and lpm may access brun at the same time, this lock to protect critical section  */
	bool brun;
};

static struct brun_struct brun_flags[NR_CPUS];

unsigned long get_timestamp_freq_hs(void)
{
	return USEC_PER_SEC;
}

static irqreturn_t px_pmu_isr(unsigned int pid,
                              unsigned int tid,
                              struct pt_regs * const regs,
                              unsigned int cpu,
                              unsigned long long ts)
{
	u32 pmcr_value;
	u32 flag_value;
	unsigned int i;
	bool buffer_full = false;
	bool found = false;

	PXD32_Hotspot_Sample_V2 sample_rec;

	/* disable the counters */
	pmcr_value = A7_Read_PMCR();
	pmcr_value &= ~0x1;
	//pmcr_value |= 0x6;
	A7_Write_PMCR(pmcr_value);

	/* clear the overflow flag */
	flag_value = A7_Read_OVSR();
	A7_Write_OVSR(0xffffffff);

	/* add for PXA1088 platform, Need to init CTI irq line */
	#ifdef PX_SOC_PXA1088
	pxa988_ack_ctiint_func = (pxa988_ack_ctiint_t)kallsyms_lookup_name_func("pxa988_ack_ctiint");
	pxa988_ack_ctiint_func();
	#endif

	if (flag_value == 0)
	{
		return IRQ_NONE;
	}

	sample_rec.pc = regs->ARM_pc;
	sample_rec.pid = pid;
	sample_rec.tid = tid;
	memcpy(sample_rec.timestamp, &ts, sizeof(sample_rec.timestamp));

	if ((flag_value & 0x80000000) && es[COUNTER_A7_PMU_CCNT].enabled)
	{
		found = true;
		sample_rec.registerId = COUNTER_A7_PMU_CCNT;

		/* ccnt overflow */
		if (es[sample_rec.registerId].calibration == false)
		{
			/* write sample record in non-calibration mode */
			buffer_full |= write_sample(cpu, &sample_rec);
		}
		else
		{
			/* calculate the overflow count in calibration mode */
			es[sample_rec.registerId].overflow++;
		}

		A7_WritePMUCounter(sample_rec.registerId, es[sample_rec.registerId].reset_value);
	}

	for (i=0; i<A7_PMN_NUM; i++)
	{
		if (flag_value & (0x1 << i))
		{
			found = true;

			switch (i)
			{
			case 0:	sample_rec.registerId = COUNTER_A7_PMU_PMN0; break;
			case 1:	sample_rec.registerId = COUNTER_A7_PMU_PMN1; break;
			case 2:	sample_rec.registerId = COUNTER_A7_PMU_PMN2; break;
			case 3:	sample_rec.registerId = COUNTER_A7_PMU_PMN3; break;
			default: break;
			}

			if (es[sample_rec.registerId].calibration == false)
			{
				/* write sample record in non-calibration mode */
				buffer_full |= write_sample(cpu, &sample_rec);
			}
			else
			{
				/* calculate the overflow count in calibration mode */
				es[sample_rec.registerId].overflow++;
			}

			A7_WritePMUCounter(sample_rec.registerId, es[sample_rec.registerId].reset_value);
		}

	}

	if (!buffer_full)
	{
		/* enable the counters */
		pmcr_value |= 0x1;
		pmcr_value &= ~0x6;
		A7_Write_PMCR(pmcr_value);
	}

	return IRQ_HANDLED;
}

static irqreturn_t px_hotspot_isr(int irq, void * dev)
{
	struct pt_regs *regs;

	unsigned int pid;
	unsigned int tid;
	unsigned int cpu;
	unsigned long long ts;
	unsigned long flags;
	irqreturn_t ret;

	local_irq_save(flags);

	ret = IRQ_NONE;

	regs = get_irq_regs();

	pid = current->tgid;
	tid = current->pid;
	ts  = get_timestamp();

	cpu = smp_processor_id();

	ret = px_pmu_isr(pid, tid, regs, cpu, ts);

	local_irq_restore(flags);

	return ret;
}

static void set_ccnt_events(struct HSEventConfig *ec, struct pmu_registers_a7 *pmu_regs)
{
	if (ec->eventId == A7_PMU_CCNT_CORE_CLOCK_TICK_64)
	{
		/* 64 clock cycle */
		pmu_regs->pmcr |= 0x8;
	}
	else
	{
		pmu_regs->pmcr &= ~0x8;
	}

	/* reset CCNT */
	pmu_regs->pmcr |= 0x4;

	pmu_regs->cntenset |= 0x1 << 31;
	pmu_regs->cntenclr |= 0x1 << 31;
	pmu_regs->ovsr     |= 0x1 << 31;
	pmu_regs->intenset |= 0x1 << 31;
	pmu_regs->intenclr |= 0x1 << 31;

	pmu_regs->ccnt = es[COUNTER_A7_PMU_CCNT].reset_value;
}

static void set_pmn_events(int pmn_index,
                           struct HSEventConfig *ec,
                           struct pmu_registers_a7 *pmu_regs)
{
	int register_id;

	switch (pmn_index)
	{
	case 0: register_id = COUNTER_A7_PMU_PMN0; break;
	case 1: register_id = COUNTER_A7_PMU_PMN1; break;
	case 2: register_id = COUNTER_A7_PMU_PMN2; break;
	case 3: register_id = COUNTER_A7_PMU_PMN3; break;

	default: return;
	}

	/* reset PMN counter */
	pmu_regs->pmcr |= 0x2;

	pmu_regs->intenset |= (0x1 << pmn_index);
	pmu_regs->intenclr |= (0x1 << pmn_index);
	pmu_regs->ovsr     |= (0x1 << pmn_index);

	pmu_regs->cntenclr |= (0x1 << pmn_index);
	pmu_regs->cntenset |= (0x1 << pmn_index);

	pmu_regs->evtsel[pmn_index] = get_pmu_event_id(ec->eventId);

	pmu_regs->pmncnt[pmn_index] = es[register_id].reset_value;
}

static unsigned long long get_event_count_for_calibration_a7(int register_id)
{
	unsigned long long value;

	switch (register_id)
	{
	case COUNTER_A7_PMU_CCNT:
	case COUNTER_A7_PMU_PMN0:
	case COUNTER_A7_PMU_PMN1:
	case COUNTER_A7_PMU_PMN2:
	case COUNTER_A7_PMU_PMN3:

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
	for (i=0; i<COUNTER_A7_MAX_ID; i++)
	{
		es[i].enabled     = false;
		es[i].calibration = false;
		es[i].reset_value = 0;
		es[i].overflow    = 0;
	}

	for (i=0; i<g_ebs_settings_hs.eventNumber; i++)
	{
		int register_id;

		register_id = g_ebs_settings_hs.event[i].registerId;
		es[register_id].enabled = true;

		if (g_calibration_mode_hs)
		{
			es[register_id].reset_value = 0;
			es[register_id].calibration = true;
		}
		else
		{
			es[register_id].reset_value = (u32)(-1) - g_ebs_settings_hs.event[i].eventsPerSample + 1;
			es[register_id].calibration = false;
		}
	}

}

static void start_pmu(void *data)
{
	int i;

	struct pmu_registers_a7 pmu_regs;

	bool is_start_paused;

	is_start_paused = *(bool *)data;

	pmu_regs.pmcr   = 0x0;
	pmu_regs.ccnt   = 0x0;
	pmu_regs.cntenset = 0x0;
	pmu_regs.cntenclr = 0x0;
	pmu_regs.intenset = 0x0;
	pmu_regs.intenclr = 0x0;
	pmu_regs.ovsr   = 0x0;

	for (i=0; i<A7_PMN_NUM; i++)
	{
		pmu_regs.evtsel[i] = 0;
		pmu_regs.pmncnt[i] = 0;
	}


	/* disable PMU and clear CCNT & PMNx */
	pmu_regs.pmcr = A7_Read_PMCR();
	pmu_regs.pmcr &= ~0x1;
	pmu_regs.pmcr |= 0x6;
	A7_Write_PMCR(pmu_regs.pmcr);

	for (i=0; i<g_ebs_settings_hs.eventNumber; i++)
	{
		switch (g_ebs_settings_hs.event[i].registerId)
		{
		case COUNTER_A7_PMU_CCNT:
			set_ccnt_events(&g_ebs_settings_hs.event[i], &pmu_regs);
			break;

		case COUNTER_A7_PMU_PMN0:
			set_pmn_events(0, &g_ebs_settings_hs.event[i], &pmu_regs);
			break;

		case COUNTER_A7_PMU_PMN1:
			set_pmn_events(1, &g_ebs_settings_hs.event[i], &pmu_regs);
			break;

		case COUNTER_A7_PMU_PMN2:
			set_pmn_events(2, &g_ebs_settings_hs.event[i], &pmu_regs);
			break;

		case COUNTER_A7_PMU_PMN3:
			set_pmn_events(3, &g_ebs_settings_hs.event[i], &pmu_regs);
			break;

		default:
			break;
		}

	}

	/* disable all counters */
	A7_Write_CNTENCLR(0xffffffff);

	/* disable all interrupts */
	A7_Write_INTENCLR(0xffffffff);

	/* clear overflow flags */
	A7_Write_OVSR(0xffffffff);

	A7_Write_USEREN(0);

	/* write the counter values */
	A7_WritePMUCounter(COUNTER_A7_PMU_CCNT, pmu_regs.ccnt);
	A7_WritePMUCounter(COUNTER_A7_PMU_PMN0, pmu_regs.pmncnt[0]);
	A7_WritePMUCounter(COUNTER_A7_PMU_PMN1, pmu_regs.pmncnt[1]);
	A7_WritePMUCounter(COUNTER_A7_PMU_PMN2, pmu_regs.pmncnt[2]);
	A7_WritePMUCounter(COUNTER_A7_PMU_PMN3, pmu_regs.pmncnt[3]);

	/* set events */
	for (i=0; i<A7_PMN_NUM; i++)
	{
		A7_Write_PMNXSEL(i);
		A7_Write_PMNCNT(pmu_regs.pmncnt[i]);
		A7_Write_EVTSEL(pmu_regs.evtsel[i]);
	}

	pmu_regs.pmcr &= ~0x6;

	if (is_start_paused)
	{
		pmu_regs.pmcr &= ~0x1;
	}
	else
	{
		pmu_regs.pmcr |= 0x1;
	}

	/* enable the interrupts */
	A7_Write_INTENSET(pmu_regs.intenset);

	/* enable the counters */
	A7_Write_CNTENSET(pmu_regs.cntenset);

	/* Enable PMU */
	A7_Write_PMCR(pmu_regs.pmcr);
}

static void stop_pmu(void * param)
{
	u32 pmcr_value;

	/* disable all counters */
	pmcr_value = A7_Read_PMCR();
	pmcr_value &= ~0x1;
	A7_Write_PMCR(pmcr_value);

	A7_Write_CNTENCLR(0xffffffff);

	/* clear overflow flags */
	A7_Write_OVSR(0xffffffff);

	/* disable all interrupts */
	A7_Write_INTENCLR(0xffffffff);

	/* We need to save the PMU counter value for calibration result before calling free_pmu()
 	 * because free_pmu() may cause these registers be modified by IPM */
	reg_value[COUNTER_A7_PMU_CCNT] += A7_ReadPMUCounter(COUNTER_A7_PMU_CCNT);
	reg_value[COUNTER_A7_PMU_PMN0] += A7_ReadPMUCounter(COUNTER_A7_PMU_PMN0);
	reg_value[COUNTER_A7_PMU_PMN1] += A7_ReadPMUCounter(COUNTER_A7_PMU_PMN1);
	reg_value[COUNTER_A7_PMU_PMN2] += A7_ReadPMUCounter(COUNTER_A7_PMU_PMN2);
	reg_value[COUNTER_A7_PMU_PMN3] += A7_ReadPMUCounter(COUNTER_A7_PMU_PMN3);
}

static void pause_pmu(void * param)
{
	u32 pmcr_value;

	/* disable all counters */
	pmcr_value = A7_Read_PMCR();
	pmcr_value &= ~0x1;
	A7_Write_PMCR(pmcr_value);
}

static void resume_pmu(void * param)
{
	u32 pmcr_value;

	/* enable all counters */
	pmcr_value = A7_Read_PMCR();
	pmcr_value |= 0x1;
	A7_Write_PMCR(pmcr_value);
}

static bool is_pmu_enabled(void)
{
	if (es[COUNTER_A7_PMU_CCNT].enabled)
		return true;

	if (es[COUNTER_A7_PMU_PMN0].enabled)
		return true;

	if (es[COUNTER_A7_PMU_PMN1].enabled)
		return true;

	if (es[COUNTER_A7_PMU_PMN2].enabled)
		return true;

	if (es[COUNTER_A7_PMU_PMN3].enabled)
		return true;

	return false;
}

static int __cpuinit cpu_notify(struct notifier_block *self, unsigned long action, void *hcpu)
{
	long cpu = (long) hcpu;

	switch (action)
	{
		case CPU_ONLINE: 
		case CPU_ONLINE_FROZEN:
			spin_lock(&brun_flags[cpu].brun_lock);
			if(!brun_flags[cpu].brun) {
				brun_flags[cpu].brun = true;
				set_cpu_irq_affinity(cpu);
				smp_call_function_single(cpu, start_pmu, &pmu_paused, 1);
			}
			spin_unlock(&brun_flags[cpu].brun_lock);
			break;

		case CPU_DYING:
			spin_lock(&brun_flags[cpu].brun_lock);
			if(brun_flags[cpu].brun) {
				brun_flags[cpu].brun = false;
				smp_call_function_single(cpu, stop_pmu, NULL, 1);
			}
			spin_unlock(&brun_flags[cpu].brun_lock);
			break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __refdata cpu_notifier_for_pmu = {
	.notifier_call = cpu_notify,
	.priority = CPU_PRI_PERF,
};

#if 0
static int coresight_notifier(struct notifier_block *self,
				unsigned long cmd, void *v)
{
	int cpu;

	cpu = smp_processor_id();

	switch (cmd) {
	case CPU_PM_ENTER:
		spin_lock(&brun_flags[cpu].brun_lock);
		if(brun_flags[cpu].brun) {
			brun_flags[cpu].brun = false;
			stop_pmu(NULL);
		}
		spin_unlock(&brun_flags[cpu].brun_lock);
		break;

	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		spin_lock(&brun_flags[cpu].brun_lock);
		if(!brun_flags[cpu].brun) {
			brun_flags[cpu].brun = true;
			start_pmu(&pmu_paused);
		}
		spin_unlock(&brun_flags[cpu].brun_lock);
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block coresight_notifier_block = {
	.notifier_call = coresight_notifier,
};


static int register_low_power_mode_events(void)
{
	cpu_pm_register_notifier(&coresight_notifier_block);

	return 0;
}

static int unregister_low_power_mode_events(void)
{
	cpu_pm_unregister_notifier(&coresight_notifier_block);

	return 0;
}
#endif

static int start_ebs(bool is_start_paused)
{
	int i;
	int ret;
	int cpu;

	get_settings();

	if (is_pmu_enabled())
	{
		pmu_paused = is_start_paused;

		ret = allocate_pmu();
		if (ret != 0)
			return ret;

		ret = register_pmu_isr(px_hotspot_isr);
		if (ret != 0)
			return ret;

		for (i=0; i<COUNTER_A7_MAX_ID; i++)
			reg_value[i] = 0;

//		get_online_cpus();

		ret = register_hotcpu_notifier(&cpu_notifier_for_pmu);
		if (ret != 0) {
//			put_online_cpus();
			return ret;
		}

		on_each_cpu(start_pmu, &is_start_paused, 1);
		for_each_online_cpu(cpu) {
			brun_flags[cpu].brun = true;
		}

//		put_online_cpus();

		/* register low power enter and exit events */
//		register_low_power_mode_events();
	}

	return 0;
}

static int stop_ebs(void)
{
	int ret = 0;
//	int cpu = 0;

	if (is_pmu_enabled())
	{
//		get_online_cpus();

		on_each_cpu(stop_pmu, NULL, 1);
		unregister_hotcpu_notifier(&cpu_notifier_for_pmu);

//		put_online_cpus();

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

//		unregister_low_power_mode_events();
	}
	
	return 0;
}


static int pause_ebs(void)
{
	if (is_pmu_enabled())
	{
		pmu_paused = true;

		on_each_cpu(pause_pmu, NULL, 1);
	}

	return 0;
}

static int resume_ebs(void)
{
	if (is_pmu_enabled())
	{
		pmu_paused = false;

		on_each_cpu(resume_pmu, NULL, 1);
	}

	return 0;
}

static void init_for_lpm_hotplug(void) {
	int cpu;

	for_each_possible_cpu(cpu) {
		brun_flags[cpu].brun = false;
		spin_lock_init(&brun_flags[cpu].brun_lock);
	}
}

static int start_sampling_a7(bool is_start_paused)
{

	init_for_lpm_hotplug();
	
	// config tbs sampling with ap timer 2
	if ((g_tbs_settings_hs.interval != 0) && !g_calibration_mode_hs)
	{
		int reterr = start_tbs_hs(is_start_paused);
		if (reterr)
			return reterr;
	}

	// config ebs sampling
	if (g_ebs_settings_hs.eventNumber != 0)
	{
		int reterr = start_ebs(is_start_paused);
		if (reterr)
			return reterr;
	}

	return 0;
}

static int stop_sampling_a7(void)
{
	// config tbs sampling with ap timer 2
	if ((g_tbs_settings_hs.interval != 0) && !g_calibration_mode_hs)
	{
		int reterr = stop_tbs_hs();
		if (reterr)
			return reterr;
	}

	// config ebs sampling
	if (g_ebs_settings_hs.eventNumber != 0)
	{
		int reterr = stop_ebs();
		if (reterr)
			return reterr;
	}

	return 0;
}

static int pause_sampling_a7(void)
{
	if ((g_tbs_settings_hs.interval != 0) && !g_calibration_mode_hs)
	{
		int reterr = pause_tbs_hs();
		if (reterr)
			return reterr;
	}

	// config ebs sampling
	if (g_ebs_settings_hs.eventNumber != 0)
	{
		int reterr = pause_ebs();
		if (reterr)
			return reterr;
	}

	return 0;
}

static int resume_sampling_a7(void)
{
	if ((g_tbs_settings_hs.interval != 0) && !g_calibration_mode_hs)
	{
		// Enable timer
		int reterr = resume_tbs_hs();
		if (reterr)
			return reterr;
	}

	// config ebs sampling
	if (g_ebs_settings_hs.eventNumber != 0)
	{
		int reterr = resume_ebs();
		if (reterr)
			return reterr;
	}

	return 0;
}

struct hs_sampling_op_mach hs_sampling_op_a7 =
{
	.start  = start_sampling_a7,
	.stop   = stop_sampling_a7,
	.pause  = pause_sampling_a7,
	.resume = resume_sampling_a7,
	.get_count_for_cal = get_event_count_for_calibration_a7,
};
