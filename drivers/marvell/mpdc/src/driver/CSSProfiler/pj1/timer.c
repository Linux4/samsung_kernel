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
 * PJ1 CSS Timer implementation
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/profile.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <asm/io.h>

#include "PXD_css.h"
#include "CSSProfilerSettings.h"
#include "css_drv.h"
#include "timer_pj1.h"
#include "common.h"

extern struct CSSTimerSettings g_tbs_settings;
extern irqreturn_t px_css_isr(int irq, void * dev);

static bool tbs_running;

#ifdef HW_TIMER
static void * dev_id = 0;

/* default timer group and timer# */
static int timer_group = 1;
static int timer_no = 2;

static void select_timer(void)
{
/* select timer group */
#ifdef PJ1_TIMER_GROUP
#if PJ1_TIMER_GROUP == 1
	timer_group = 1;
#elif PJ1_TIMER_GROUP == 2
	timer_group = 2;
#endif
#endif


/* select timer number */
#ifdef PJ1_TIMER_NO
#if PJ1_TIMER_NO == 0
	timer_no = 0;
#elif PJ1_TIMER_NO == 1
	timer_no = 1;
#elif PJ1_TIMER_NO == 2
	timer_no = 2;
#endif
#endif

}

static void init_ap_timer(void)
{
	static bool init = false;

	/* Disable counters explicitly */
	TMR_CER(timer_group) &= ~(0x1 << timer_no);

	/* Setup clock source */
	switch (timer_no)
	{
	case 0:
		TMR_CCR(timer_group) &= ~0x1;
		break;
	case 1:
		TMR_CCR(timer_group) &= ~(0x1 << 2);
		break;
	case 2:
		TMR_CCR(timer_group) &= ~(0x1 << 5);
		break;
	default:
		return;
	}

	/* Setup counter to free running mode - UNDOCUMENTED */
	TMR_CMR(timer_group) &= ~(0x1 << timer_no);
	//TMR_CMR(timer_group) |= (0x1 << timer_no);

	/* Setup preload value */
	TMR_PLVRn(timer_group, timer_no) = 0;

	/* Set to free running mode for preload control */
	TMR_PLCRn(timer_group, timer_no) = 1;
	//TMR_PLCRn(timer_group, timer_no) = 0;

	/* Clear any pending match status {0-2} */
	TMR_ICRn(timer_group, timer_no) = 0x7;

	/* Cause the latch */
	TMR_CVWRn(timer_group, timer_no) = 1;

	init = true;
}

/*
 * return the timer frequency in KHz)
 */
static unsigned int get_timer_freq(void)
{
	unsigned value;

	switch (timer_group)
	{
	case 1:
		value = APBC_TIMERS0_CLK_RST;
		break;
	case 2:
		value = APBC_TIMERS1_CLK_RST;
		break;
	default:
		return -1;
	}

	value = (value & 0x70) >> 4;

	switch (value)
	{
	case 0:		return 13000;       // 13 MHz
	case 1:		return 32;          // 32 KHz
	case 2:		return 6500;        // 6.5 MHz
	case 3:		return 3250;        // 3.25 MHz
	case 4:		return 1000;        // 1 MHz
	default:	return -1;
	}
}

irqreturn_t px_timer_isr(struct pt_regs * const regs,
                         unsigned int pid,
                         unsigned int tid,
                         unsigned int cpu,
                         unsigned long long ts)
{
	bool buffer_full = false;

	// disable the interrupt
	TMR_IERn(timer_group, timer_no) = 0;

	// clear the interrupt flag
	TMR_ICRn(timer_group, timer_no) = 1;

	// set next match
	TMR_Tn_Mm(timer_group, timer_no, 0) = g_tbs_settings.interval * get_timer_freq() / 1000;
	//TMR_Tn_Mm(timer_group, timer_no, 0) += g_tbs_settings.interval * get_timer_freq() / 1000;

	if (tbs_running)
	{
		char ** bt_buffer = &per_cpu(g_bt_buffer, cpu);
		PXD32_CSS_Call_Stack_V2 *css_data = (PXD32_CSS_Call_Stack_V2 *)*bt_buffer;

		/* get css data */
		//buffer_full = backtrace(regs, pid, tid, COUNTER_PJ1_OS_TIMER, cpu, ts);
		backtrace(regs, cpu, pid, tid, css_data);

		fill_css_data_head(css_data, pid, tid, COUNTER_PJ1_OS_TIMER, ts);

		buffer_full = write_css_data(cpu, css_data);

		if (buffer_full)
		{
			tbs_running = false;
		}
		else
		{
			tbs_running = true;
		}
	}

	// enable the interrupt
	TMR_IERn(timer_group, timer_no) = 1;
	return IRQ_HANDLED;
}

unsigned int get_timer_irq(void)
{
	switch (timer_group)
	{
	case 1:
		switch (timer_no)
		{
		case 0:  return PX_IRQ_PJ1_TIMER1_1;
		case 1:  return PX_IRQ_PJ1_TIMER1_2;
		case 2:  return PX_IRQ_PJ1_TIMER1_3;
		default: break;
		}
	case 2:
		switch (timer_no)
		{
		case 0:  return PX_IRQ_PJ1_TIMER2_1;
		case 1:  return PX_IRQ_PJ1_TIMER2_2;
		case 2:  return PX_IRQ_PJ1_TIMER2_3;
		default: break;
		}
	default:
		break;
	}

	return -1;
}

#else
static struct hrtimer hrtimer_for_tbs;

irqreturn_t px_timer_isr(struct pt_regs * const regs,
                         unsigned int pid,
                         unsigned int tid,
                         unsigned int cpu,
                         unsigned long long ts)
{
	bool buffer_full = false;

	if (tbs_running)
	{
		char ** bt_buffer = &per_cpu(g_bt_buffer, cpu);
		PXD32_CSS_Call_Stack_V2 *css_data = (PXD32_CSS_Call_Stack_V2 *)*bt_buffer;

		//buffer_full = backtrace(regs, pid, tid, COUNTER_PJ1_OS_TIMER, cpu, ts);
		backtrace(regs, cpu, pid, tid, css_data);

		fill_css_data_head(css_data, pid, tid, COUNTER_PJ1_OS_TIMER, ts);

		buffer_full = write_css_data(cpu, css_data);

		if (buffer_full)
		{
			tbs_running = false;
		}
		else
		{
			tbs_running = true;
		}
	}

	return 0;
}

static enum hrtimer_restart hrtimer_func(struct hrtimer *hrtimer)
{
	unsigned int pid;
	unsigned int tid;
	unsigned int pc;
	unsigned int cpu;
	unsigned long flags;
	unsigned long long ts;
	ktime_t ktime;
	struct pt_regs *regs;

	local_irq_save(flags);

	regs = get_irq_regs();

	if (regs != NULL)
	{
		pid = current->tgid;
		tid = current->pid;
		pc  = regs->ARM_pc;
		ts  = get_timestamp();
		cpu = smp_processor_id();

		px_timer_isr(regs, pid, tid, cpu, ts);
	}

	ktime = ktime_set(0, g_tbs_settings.interval * NSEC_PER_USEC);
	hrtimer_forward_now(hrtimer, ktime);

	local_irq_restore(flags);

	return HRTIMER_RESTART;
}
#endif /* HW_TIMER */

int start_tbs(bool is_start_paused)
{
#ifdef HW_TBS
	int ret;

	select_timer();
	init_ap_timer();

	ret = request_irq(get_timer_irq(), px_css_isr, IRQF_TIMER/*IRQF_TIMER|IRQF_DISABLED*/, "CPA Timer", dev_id);

	if (ret != 0)
	{
		printk(KERN_ALERT "[CPA] Failed to request IRQ: %d\n", get_timer_irq());
		return ret;
	}

 	/* Enable interrupt on match0 */
	TMR_IERn(timer_group, timer_no) = 1;

	/* Prime the match register */
	TMR_Tn_Mm(timer_group, timer_no, 0) = g_tbs_settings.interval * get_timer_freq() / 1000;

	if (!is_start_paused)
	{
		tbs_running = true;

		/* Enable the timer */;
		TMR_CER(timer_group) |= 0x1 << timer_no;
	}
	else
	{
		tbs_running = false;
		/* Disable the timer */
		//TMR_CER(timer_group) &= ~(0x1 << timer_no);

		/* Enable the timer */;
		TMR_CER(timer_group) |= 0x1 << timer_no;
	}

	return 0;
#else
	int ret;
	ktime_t ktime;
	unsigned long delay_in_ns = g_tbs_settings.interval * NSEC_PER_USEC;
	
	if (!is_start_paused)
	{
		tbs_running = true;
	}
	else
	{
		tbs_running = false;
	}

	ktime = ktime_set(0, delay_in_ns);

	hrtimer_init(&hrtimer_for_tbs, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	hrtimer_for_tbs.function = hrtimer_func;

	ret = hrtimer_start(&hrtimer_for_tbs, ktime, HRTIMER_MODE_REL);

	return 0;
#endif
}


int stop_tbs(void)
{
#ifdef HW_TBS
	free_irq(get_timer_irq(), dev_id);
	tbs_running = false;

	/* Disable the timer */
	TMR_CER(timer_group) &= ~(0x1 << timer_no);

	/* Disable interrupt on match0 */;
	TMR_IERn(timer_group, timer_no) = 0;

	return 0;
#else
	tbs_running = false;

	hrtimer_cancel(&hrtimer_for_tbs);
	
	return 0;
#endif
}

/*
 * if we use free-running mode, we can't disable the timer when calling pause_tbs()
 * Otherwise when resuming, we need to enable the timer, and we need to set the preload
 * value to the value when paused. But on PJ1, it is difficult to get the counter value correctly.
 *
 * So we just set a flag (tbs_running) to implement pause/resume in free-running mode.
 */
int pause_tbs(void)
{
	/* Disable timer */
	//TMR_CER(timer_group) &= ~(0x1 << timer_no);

	tbs_running = false;
	return 0;
}

int resume_tbs(void)
{
	tbs_running = true;

	/* Enable the timer */
	//TMR_CER(timer_group) |= 0x1 << timer_no;
	return 0;
}
