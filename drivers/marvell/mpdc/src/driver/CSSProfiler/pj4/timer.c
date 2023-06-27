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
 * PJ4 CSS Timer implementation
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/profile.h>
#include <linux/sched.h>
#include <asm/io.h>

#include "PXD_css.h"
#include "CSSProfilerSettings.h"
#include "css_drv.h"
#include "common.h"

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
#include "timer_pj4.h"
#endif

#if defined(PX_SOC_PXA955) && defined(HW_TBS)
#include <mach/hardware.h>
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30))
#include <mach/regs-ost.h>
#else
#include <mach/pxa-regs.h>
#endif
#endif

extern struct CSSTimerSettings g_tbs_settings;
extern irqreturn_t px_css_isr(int irq, void * dev);

#if defined(PX_SOC_ARMADA510) || !defined(HW_TBS)
static bool tbs_running;
#endif

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
static bool tbs_running;
static void * dev_id = 0;

/* default timer group and timer# */
static int timer_group = 1;
static int timer_no = 2;
#endif

#if defined(PX_SOC_PXA955) && defined(HW_TBS)
/* Additional definitions for OSCR5 related reg */
#define OSCR0 __REG(0x40a00010)
#define OMCR5 __REG(0x40a000c4)
#define OSCR5 __REG(0x40a00044)
#define OSMR5 __REG(0x40a00084)
#define OSSR_PX_INTERRUPT   (0x1 << 5)
#define OMCR_NOT_OSCR4      (0x1 << 7)
#define OMCR_MATCH_CONTINUE (0x1 << 6)
#define OMCR_NO_EXT_SYNC    (0x0)
#define OMCR_MATCH_RESET    (0x1 << 3)
#define OMCR_FREQ_32K       (0x1)           /* 32768 Hz */
#define OMCR_DISABLED       (0x7)
#define PX_TIMER_INIT_VALUE     (OMCR_NOT_OSCR4 | OMCR_MATCH_CONTINUE | OMCR_NO_EXT_SYNC | OMCR_MATCH_RESET)

static int dev_oscr;
#endif


#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
static void select_timer(void)
{
/* select timer group */
#ifdef PJ4_TIMER_GROUP
#if PJ4_TIMER_GROUP == 1
	timer_group = 1;
#elif PJ4_TIMER_GROUP == 2
	timer_group = 2;
#endif
#endif


/* select timer number */
#ifdef PJ4_TIMER_NO
#if PJ4_TIMER_NO == 0
	timer_no = 0;
#elif PJ4_TIMER_NO == 1
	timer_no = 1;
#elif PJ4_TIMER_NO == 2
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
#endif

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
/*
 * return the timer frequency in KHz
 */
static unsigned int get_timer_freq(void)
{
	unsigned value;

	switch (timer_group)
	{
	case 1:
		value = APBC_TIMERS0_CLK_RST;
		break;
#if 0
	case 2:
		value = APBC_TIMERS1_CLK_RST;
		break;
#endif
	default:
		return -1;
	}

	value = (value & 0x70) >> 4;

	/* need to modify the frequency for different vctcxo clock frequency */
	switch (value)
	{
	case 0:		return 32;           // 32 KHz
	case 1:		return 6500;         // 6.5 MHz
	case 2:		return 13000;        // 13 MHz
	case 3:		return 26000;        // 26 MHz
	default:	return -1;
	}
}
#endif

irqreturn_t px_timer_isr(struct pt_regs * const regs,
                         unsigned int pid,
                         unsigned int tid,
                         unsigned int cpu,
                         unsigned long long ts)
{
#if defined(PX_SOC_ARMADA510) || !defined(HW_TBS)
	bool buffer_full = false;
	char ** bt_buffer = &per_cpu(g_bt_buffer, cpu);
	PXD32_CSS_Call_Stack_V2 *css_data = (PXD32_CSS_Call_Stack_V2 *)*bt_buffer;

	if (tbs_running)
	{
		backtrace(regs, cpu, pid, tid, css_data);

		fill_css_data_head(css_data, pid, tid, COUNTER_PJ4_OS_TIMER, ts);

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
#endif

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
	bool buffer_full = false;
	char ** bt_buffer = &per_cpu(g_bt_buffer, cpu);
	PXD32_CSS_Call_Stack_V2 *css_data = (PXD32_CSS_Call_Stack_V2 *)*bt_buffer;

	// disable the interrupt
	TMR_IERn(timer_group, timer_no) = 0;

	// clear the interrupt flag
	TMR_ICRn(timer_group, timer_no) = 1;

	// set next match
	TMR_Tn_Mm(timer_group, timer_no, 0) = g_tbs_settings.interval * get_timer_freq() / 1000;
	//TMR_Tn_Mm(timer_group, timer_no, 0) += g_tbs_settings.interval * get_timer_freq() / 1000;

	if (tbs_running)
	{
		/* write css data to sample buffer */
		backtrace(regs, cpu, pid, tid, css_data);

		fill_css_data_head(css_data, pid, tid, COUNTER_PJ4_OS_TIMER, ts);

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
#endif
#if defined(PX_SOC_PXA955) && defined(HW_TBS)
	bool buffer_full = false;
	char ** bt_buffer = &per_cpu(g_bt_buffer, cpu);
	PXD32_CSS_Call_Stack_V2 *css_data = (PXD32_CSS_Call_Stack_V2 *)*bt_buffer;

	if (OSSR & OSSR_PX_INTERRUPT)
	{
		/* disable the counter */
		OMCR5 &= ~0x7;

		/* clear the interrupt flag */
		OSSR = OSSR_PX_INTERRUPT;

		/* write css data to sample buffer */
		backtrace(regs, cpu, pid, tid, css_data);

		fill_css_data_head(css_data, pid, tid, COUNTER_PJ4_OS_TIMER, ts);

		buffer_full = write_css_data(cpu, css_data);

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

#endif
	return IRQ_HANDLED;
}

unsigned int get_timer_irq(void)
{
#if defined(PX_SOC_ARMADA510) || !defined(HW_TBS)
	return -1;
#endif

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
	switch (timer_group)
	{
	case 1:
		switch (timer_no)
		{
		case 0:  return PX_IRQ_MMP2_TIMER1_1;
		case 1:  return PX_IRQ_MMP2_TIMER1_2;
		case 2:  return PX_IRQ_MMP2_TIMER1_3;
		default: break;
		}

	case 2:
		switch (timer_no)
		{
		case 0:  return PX_IRQ_MMP2_TIMER2_1;
		case 1:  return PX_IRQ_MMP2_TIMER2_2;
		case 2:  return PX_IRQ_MMP2_TIMER2_3;
		default: break;
		}

	default:
		break;
	}
#endif

#if defined(PX_SOC_PXA955) && defined(HW_TBS)
	return IRQ_OST_4_11;
#endif

	return -1;
}

#if defined(PX_SOC_ARMADA510) || !defined(HW_TBS)
static struct hrtimer hrtimer_for_tbs;

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

	ktime = ktime_set(0, g_tbs_settings.interval * 1000);

	//hrtimer_forward_now(hrtimer, ktime);
	if (regs != NULL)
	{
		pid = current->tgid;
		tid = current->pid;
		pc  = regs->ARM_pc;
		ts  = get_timestamp();
		cpu = smp_processor_id();

		px_timer_isr(regs, pid, tid, cpu, ts);
	}

	hrtimer_forward_now(hrtimer, ktime);

	local_irq_restore(flags);

	return HRTIMER_RESTART;
}
#endif

int start_tbs(bool is_start_paused)
{
#if defined(PX_SOC_ARMADA510) || !defined(HW_TBS)
	int ret;
	ktime_t ktime;
	unsigned long delay_in_ns = g_tbs_settings.interval * 1000;

	ktime = ktime_set(0, delay_in_ns);

	hrtimer_init(&hrtimer_for_tbs, CLOCK_REALTIME, HRTIMER_MODE_REL);

	hrtimer_for_tbs.function = hrtimer_func;

	ret = hrtimer_start(&hrtimer_for_tbs, ktime, HRTIMER_MODE_REL);

	if (!is_start_paused)
	{
		tbs_running = true;
	}
	else
	{
		tbs_running = false;
	}

	return 0;
#endif

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
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
#endif

#if defined(PX_SOC_PXA955) && defined(HW_TBS)
	int ret;
	u32 offset = g_tbs_settings.interval / 100 * 32768 / 10000;

	OMCR5 = PX_TIMER_INIT_VALUE;
	OIER &= ~OSSR_PX_INTERRUPT;

	/* set match offset */
	OSMR5 = offset;
	ret = request_irq(IRQ_OST_4_11, px_css_isr, IRQF_TIMER|IRQF_SHARED, "CPA Timer", &dev_oscr);
	if (ret != 0)
	{
		printk(KERN_ALERT "[CPA] Request IRQ returns %d!\n", ret);
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

#endif
	return 0;
}


int stop_tbs(void)
{
#if defined(PX_SOC_ARMADA510) || !defined(HW_TBS)
	tbs_running = false;

	hrtimer_cancel(&hrtimer_for_tbs);
	
	return 0;
#endif

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
	free_irq(get_timer_irq(), dev_id);
	tbs_running = false;

	/* Disable the timer */
	TMR_CER(timer_group) &= ~(0x1 << timer_no);

	/* Disable interrupt on match0 */;
	TMR_IERn(timer_group, timer_no) = 0;

	return 0;
#endif

#if defined(PX_SOC_PXA955) && defined(HW_TBS)
	OMCR5 &= ~0x7;

	free_irq(IRQ_OST_4_11, &dev_oscr);

	OIER &= ~OSSR_PX_INTERRUPT;

	/* clear the interrupt flag */
	OSSR = OSSR_PX_INTERRUPT;

	return 0;
#endif
	return 0;
}

/*
 * if we use free-running mode, we can't disable the timer when calling pause_tbs()
 * Otherwise when resuming, we need to enable the timer, and we need to set the preload
 * value to the value when paused. But on PJ4, it is difficult to get the counter value correctly.
 *
 * So we just set a flag (tbs_running) to implement pause/resume in free-running mode.
 */
int pause_tbs(void)
{
#if defined(PX_SOC_ARMADA510) || !defined(HW_TBS)
	tbs_running = false;
#endif

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
	/* Disable timer */
	//TMR_CER(timer_group) &= ~(0x1 << timer_no);

	tbs_running = false;
#endif

#if defined(PX_SOC_PXA955) && defined(HW_TBS)
	OMCR5 &= ~0x7;
#endif

	return 0;
}

int resume_tbs(void)
{
#if defined(PX_SOC_ARMADA510) || !defined(HW_TBS)
	tbs_running = true;
#endif

#if defined(PX_SOC_ARMADA610) && defined(HW_TBS)
	tbs_running = true;

	/* Enable the timer */
	//TMR_CER(timer_group) |= 0x1 << timer_no;
#endif

#if defined(PX_SOC_PXA955) && defined(HW_TBS)
	OMCR5 |= OMCR_FREQ_32K;
#endif

	return 0;
}
