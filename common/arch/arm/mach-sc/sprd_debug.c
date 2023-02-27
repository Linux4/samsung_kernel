/*
 *  sprd_debug.c
 *
 */

#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/sysrq.h>
//#include <mach/regs-pmu.h>

/* FIXME: This is temporary solution come from Midas */
#include <asm/outercache.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>

#include <mach/hardware.h>
#include <mach/system.h>
#include <mach/sprd_debug.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <mach/pm_debug.h>
#include <linux/regs_debug.h>

#define MY_DEBUG 0

struct sprd_debug_regs_access *sprd_debug_last_regs_access;
EXPORT_SYMBOL(sprd_debug_last_regs_access);

/* enable/disable sprd_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union sprd_debug_level_t sprd_debug_level = { .en.kernel_fault = 1, };

module_param_named(enable, sprd_debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, sprd_debug_level.en.user_fault, ushort, 0644);
module_param_named(level, sprd_debug_level.uint_val, uint, 0644);


static const char *gkernel_sprd_build_info_date_time[] = {
	__DATE__,
	__TIME__
};

static char gkernel_sprd_build_info[100];

/* klaatu - schedule log */
#ifdef CONFIG_SPRD_DEBUG_SCHED_LOG
static struct sched_log sprd_debug_log __cacheline_aligned;
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t hrtimer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
struct sched_log (*psprd_debug_log) = (&sprd_debug_log);
#endif



void sprd_debug_save_pte(void *pte, int task_addr)
{

//	memcpy(&per_cpu(sprd_debug_fault_status,smp_processor_id()), pte,sizeof(struct sprd_debug_fault_status_t));
//	per_cpu(sprd_debug_fault_status,smp_processor_id()).cur_process_magic =task_addr;
}
void sprd_debug_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	if (!sprd_debug_level.en.kernel_fault)
		return;

#if 0
	/* Must be deleted later */
	pr_info("Test %s:key code(%d) value(%d),(up:%d,down:%d)\n", __func__, code, value, volup_p, voldown_p);
#endif

	/* Enter Force Upload
	 *  Hold volume down key first
	 *  and then press power key twice
	 *  and volume up key should not be pressed
	 */
	if (value) {
		if (code == KEY_VOLUMEUP)
			volup_p = true;
		if (code == KEY_VOLUMEDOWN)
			voldown_p = true;
		if (volup_p && voldown_p) {
			static unsigned long stack_dump_jiffies = 0;

			if (!stack_dump_jiffies || jiffies > stack_dump_jiffies + HZ * 10) {
				pr_info("trigger sysrq w & m\n");

				/* show blocked thread */
				handle_sysrq('w');

				/* show memory */
				handle_sysrq('m');
				stack_dump_jiffies = jiffies;
			}
			if (code == KEY_POWER) {
				pr_info("%s: Crash key count : %d\n", __func__, ++loopcount);
				if (loopcount == 2)
					panic("Crash Key");
			}
		}
	} else {
		if (code == KEY_VOLUMEUP)
			volup_p = false;
		if (code == KEY_VOLUMEDOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}

static void sprd_debug_set_build_info(void)
{
	char *p = gkernel_sprd_build_info;
	sprintf(p, "Kernel Build Info : ");
	strcat(p, " Date:");
	strncat(p, gkernel_sprd_build_info_date_time[0], 12);
	strcat(p, " Time:");
	strncat(p, gkernel_sprd_build_info_date_time[1], 9);
}

__init int sprd_debug_init(void)
{
	u32 addr;

	if (!sprd_debug_level.en.kernel_fault)
		return -1;

	sprd_debug_set_build_info();
	sprd_debug_last_regs_access = (struct sprd_debug_regs_access*)dma_alloc_coherent(
				NULL, sizeof(struct sprd_debug_regs_access)*NR_CPUS, &addr, GFP_KERNEL);
	printk("*** %s, size:%u, sprd_debug_last_regs_access:%p *** \n",
		__func__, sizeof(struct sprd_debug_regs_access)*NR_CPUS, sprd_debug_last_regs_access);

	return 0;
}

int get_sprd_debug_level(void)
{
	return sprd_debug_level.uint_val;
}

/* klaatu - schedule log */
#ifdef CONFIG_SPRD_DEBUG_SCHED_LOG
void __sprd_debug_task_log(int cpu, struct task_struct *task)
{
	unsigned i;

	i = atomic_inc_return(&task_log_idx[cpu]) &
	    (ARRAY_SIZE(psprd_debug_log->task[0]) - 1);
	psprd_debug_log->task[cpu][i].time = cpu_clock(cpu);
#ifdef CP_DEBUG
	psprd_debug_log->task[cpu][i].sys_cnt = get_sys_cnt();
#endif
	strcpy(psprd_debug_log->task[cpu][i].comm, task->comm);
	psprd_debug_log->task[cpu][i].pid = task->pid;
}

void __sprd_debug_irq_log(unsigned int irq, void *fn, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	i = atomic_inc_return(&irq_log_idx[cpu]) &
	    (ARRAY_SIZE(psprd_debug_log->irq[0]) - 1);
	psprd_debug_log->irq[cpu][i].time = cpu_clock(cpu);
#ifdef CP_DEBUG
	psprd_debug_log->irq[cpu][i].sys_cnt = get_sys_cnt();
#endif
	psprd_debug_log->irq[cpu][i].irq = irq;
	psprd_debug_log->irq[cpu][i].fn = (void *)fn;
	psprd_debug_log->irq[cpu][i].en = en;
}

void __sprd_debug_work_log(struct worker *worker,
			  struct work_struct *work, work_func_t f)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	i = atomic_inc_return(&work_log_idx[cpu]) &
	    (ARRAY_SIZE(psprd_debug_log->work[0]) - 1);
	psprd_debug_log->work[cpu][i].time = cpu_clock(cpu);
#ifdef CP_DEBUG
	psprd_debug_log->work[cpu][i].sys_cnt = get_sys_cnt();
#endif
	psprd_debug_log->work[cpu][i].worker = worker;
	psprd_debug_log->work[cpu][i].work = work;
	psprd_debug_log->work[cpu][i].f = f;
}

void __sprd_debug_hrtimer_log(struct hrtimer *timer,
		     enum hrtimer_restart (*fn) (struct hrtimer *), int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	i = atomic_inc_return(&hrtimer_log_idx[cpu]) &
	    (ARRAY_SIZE(psprd_debug_log->hrtimers[0]) - 1);
	psprd_debug_log->hrtimers[cpu][i].time = cpu_clock(cpu);
#ifdef CP_DEBUG
	psprd_debug_log->hrtimers[cpu][i].sys_cnt = get_sys_cnt();
#endif
	psprd_debug_log->hrtimers[cpu][i].timer = timer;
	psprd_debug_log->hrtimers[cpu][i].fn = fn;
	psprd_debug_log->hrtimers[cpu][i].en = en;
}

int get_task_log_idx(int cpu)
{
	return atomic_inc_return(&task_log_idx[cpu]) & (ARRAY_SIZE(psprd_debug_log->task[0]) - 1);
}

int get_irq_log_idx(int cpu)
{
	return atomic_inc_return(&irq_log_idx[cpu]) & (ARRAY_SIZE(psprd_debug_log->irq[0]) - 1);
}

#endif /* CONFIG_SPRD_DEBUG_SCHED_LOG */

