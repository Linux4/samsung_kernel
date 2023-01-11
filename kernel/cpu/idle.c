/*
 * Generic entry point for the idle threads
 */
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/tick.h>
#include <linux/mm.h>
#include <linux/stackprotector.h>
#include <linux/debugfs.h>

#include <asm/tlb.h>

#include <trace/events/power.h>

static int __read_mostly cpu_idle_force_poll;

void cpu_idle_poll_ctrl(bool enable)
{
	if (enable) {
		cpu_idle_force_poll++;
	} else {
		cpu_idle_force_poll--;
		WARN_ON_ONCE(cpu_idle_force_poll < 0);
	}
}

#ifdef CONFIG_GENERIC_IDLE_POLL_SETUP
static int __init cpu_idle_poll_setup(char *__unused)
{
	cpu_idle_force_poll = 1;
	return 1;
}
__setup("nohlt", cpu_idle_poll_setup);

static int __init cpu_idle_nopoll_setup(char *__unused)
{
	cpu_idle_force_poll = 0;
	return 1;
}
__setup("hlt", cpu_idle_nopoll_setup);
#endif

#ifdef CONFIG_DEBUG_FS
ssize_t nohlt_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	pr_info("cpu_idle_force_poll value is %d.\n",
		cpu_idle_force_poll);
	return 0;
}

ssize_t nohlt_write(struct file *file, const char __user *buff,
			 size_t len, loff_t *ppos)
{

	char info[20];
	int ret;
	memset(info, '\0', 20);

	if (copy_from_user(info, buff, len))
		return -EFAULT;
	ret = sscanf(info, "%d", &cpu_idle_force_poll);
	if (ret < 0)
		return -EINVAL;
	if (cpu_idle_force_poll > 1 || cpu_idle_force_poll < 0) {
		pr_err("wrong parameter: nohlt flag should be 1(enable) or 0(disable)!\n");
		cpu_idle_force_poll = 0;
		}
	return len;
}

const struct file_operations nohlt_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= nohlt_read,
	.write		= nohlt_write,
};

static	struct dentry *nohlt_flag;
int nohlt_debugfs_init(void)
{
	nohlt_flag = debugfs_create_file("nohlt", S_IRUGO | S_IWUSR | S_IWGRP,
					     NULL, NULL, &nohlt_ops);

	if (!nohlt_flag) {
		pr_err("faile to create nohlt debugfs!\n");
		return -ENOENT;
	} else if (nohlt_flag == ERR_PTR(-ENODEV)) {
		pr_err("nohlt error!\n");
		return -ENOENT;
	}

	return 0;
}

void nohlt_debugfs_remove(void)
{
	debugfs_remove(nohlt_flag);
}
#endif

static inline int cpu_idle_poll(void)
{
	rcu_idle_enter();
	trace_cpu_idle_rcuidle(0, smp_processor_id());
	local_irq_enable();
	while (!tif_need_resched())
		cpu_relax();
	trace_cpu_idle_rcuidle(PWR_EVENT_EXIT, smp_processor_id());
	rcu_idle_exit();
	return 1;
}

/* Weak implementations for optional arch specific functions */
void __weak arch_cpu_idle_prepare(void) { }
void __weak arch_cpu_idle_enter(void) { }
void __weak arch_cpu_idle_exit(void) { }
void __weak arch_cpu_idle_dead(void) { }
void __weak arch_cpu_idle(void)
{
	cpu_idle_force_poll = 1;
	local_irq_enable();
}

/*
 * Generic idle loop implementation
 */
static void cpu_idle_loop(void)
{
	while (1) {
		tick_nohz_idle_enter();

		while (!need_resched()) {
			check_pgt_cache();
			rmb();

			if (cpu_is_offline(smp_processor_id()))
				arch_cpu_idle_dead();

			local_irq_disable();
			arch_cpu_idle_enter();

			/*
			 * In poll mode we reenable interrupts and spin.
			 *
			 * Also if we detected in the wakeup from idle
			 * path that the tick broadcast device expired
			 * for us, we don't want to go deep idle as we
			 * know that the IPI is going to arrive right
			 * away
			 */
			if (cpu_idle_force_poll || tick_check_broadcast_expired()) {
				cpu_idle_poll();
			} else {
				if (!current_clr_polling_and_test()) {
					stop_critical_timings();
					rcu_idle_enter();
					arch_cpu_idle();
					WARN_ON_ONCE(irqs_disabled());
					rcu_idle_exit();
					start_critical_timings();
				} else {
					local_irq_enable();
				}
				__current_set_polling();
			}
			arch_cpu_idle_exit();
		}

		/*
		 * Since we fell out of the loop above, we know
		 * TIF_NEED_RESCHED must be set, propagate it into
		 * PREEMPT_NEED_RESCHED.
		 *
		 * This is required because for polling idle loops we will
		 * not have had an IPI to fold the state for us.
		 */
		preempt_set_need_resched();
		tick_nohz_idle_exit();
		schedule_preempt_disabled();
	}
}

void cpu_startup_entry(enum cpuhp_state state)
{
	/*
	 * This #ifdef needs to die, but it's too late in the cycle to
	 * make this generic (arm and sh have never invoked the canary
	 * init for the non boot cpus!). Will be fixed in 3.11
	 */
#ifdef CONFIG_X86
	/*
	 * If we're the non-boot CPU, nothing set the stack canary up
	 * for us. The boot CPU already has it initialized but no harm
	 * in doing it again. This is a good place for updating it, as
	 * we wont ever return from this function (so the invalid
	 * canaries already on the stack wont ever trigger).
	 */
	boot_init_stack_canary();
#endif
	__current_set_polling();
	arch_cpu_idle_prepare();
	cpu_idle_loop();
}
#ifdef CONFIG_DEBUG_FS
postcore_initcall(nohlt_debugfs_init);
#endif
