// SPDX-License-Identifier: GPL-2.0
/*
 * Sysdump sched stats for Spreadtrum SoCs
 *
 * Copyright (C) 2021 Spreadtrum corporation. http://www.unisoc.com
 */

#define pr_fmt(fmt)  "sprd-sysdump: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kmsg_dump.h>
#include <linux/seq_buf.h>
#include <linux/mm.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>
#include <linux/kallsyms.h>
#include <linux/soc/sprd/sprd_sysdump.h>
#include "../../../../../kernel/sched/sched.h"
#ifdef CONFIG_ARM64
#include "sysdump64.h"
#endif

#define SPRD_DUMP_RQ_SIZE	(2000 * NR_CPUS)
#define SPRD_DUMP_MAX_TASK	2500
#define SPRD_DUMP_TASK_SIZE	(141 * (SPRD_DUMP_MAX_TASK + 2))
#ifdef CONFIG_ARM64
#define SPRD_DUMP_STACK_SIZE	92160
#else
#define SPRD_DUMP_STACK_SIZE	16000
#endif
#define MAX_CALLBACK_LEVEL  16

#define SEQ_printf(m, x...)			\
do {						\
	if (m)					\
		seq_buf_printf(m, x);		\
	else					\
		pr_debug(x);			\
} while (0)

#ifdef CONFIG_ARM64
#define sprd_virt_addr_valid(kaddr) ((((void *)(kaddr) >= (void *)PAGE_OFFSET && \
					(void *)(kaddr) < (void *)high_memory) || \
					((void *)(kaddr) >= (void *)KIMAGE_VADDR && \
					(void *)(kaddr) < (void *)_end)) && \
					pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#else
#define sprd_virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET && \
					(void *)(kaddr) < (void *)high_memory && \
					pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#endif

static char *sprd_task_buf;
static struct seq_buf *sprd_task_seq_buf;
static char *sprd_runq_buf;
static struct seq_buf *sprd_rq_seq_buf;
static char *sprd_stack_reg_buf;
static struct seq_buf *sprd_sr_seq_buf;

static DEFINE_RAW_SPINLOCK(dump_lock);

static int minidump_add_section(const char *name, long vaddr, int size)
{
	int ret;

	pr_info("%s in. vaddr : 0x%lx  len :0x%x\n", __func__, vaddr, size);
	ret = minidump_save_extend_information(name, __pa(vaddr),
						__pa(vaddr + size));
	if (!ret)
		pr_info("%s added to minidump section ok!!\n", name);

	return ret;
}

/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000000);
		return -nsec;
	}
	do_div(nsec, 1000000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000000);
}

static align_offset;

static void dump_align(void)
{
	int tab_offset = align_offset;

	while (tab_offset--)
		SEQ_printf(sprd_rq_seq_buf, " | ");
	SEQ_printf(sprd_rq_seq_buf, " |--");
}

static void dump_task_info(struct task_struct *task, char *status,
			      struct task_struct *curr)
{
	struct sched_entity *se;

	dump_align();
	if (!task) {
		SEQ_printf(sprd_rq_seq_buf, "%s : None(0)\n", status);
		return;
	}

	se = &task->se;
	if (task == curr) {
		SEQ_printf(sprd_rq_seq_buf, "[status: curr] pid: %d comm: %s preempt: %#x\n",
			task_pid_nr(task), task->comm,
			task_thread_info(task)->preempt_count);
		return;
	}

	SEQ_printf(sprd_rq_seq_buf, "[status: %s] pid: %d tsk: %#lx comm: %s stack: %#lx",
		status, task_pid_nr(task),
		(unsigned long)task,
		task->comm,
		(unsigned long)task->stack);
	SEQ_printf(sprd_rq_seq_buf, " prio: %d aff: %*pb",
		       task->prio, cpumask_pr_args(&task->cpus_allowed));
#ifdef CONFIG_SCHED_WALT
	SEQ_printf(sprd_rq_seq_buf, " sleep: %lu", task->last_sleep_ts);
#endif
	SEQ_printf(sprd_rq_seq_buf, " vrun: %lu arr: %lu sum_ex: %lu\n",
		(unsigned long)se->vruntime,
		(unsigned long)se->exec_start,
		(unsigned long)se->sum_exec_runtime);
}

static void dump_cfs_rq(struct cfs_rq *cfs, struct task_struct *curr);

static void dump_cgroup_state(char *status, struct sched_entity *se_p,
				   struct task_struct *curr)
{
	struct task_struct *task;
	struct cfs_rq *my_q = NULL;
	unsigned int nr_running;

	if (!se_p) {
		dump_task_info(NULL, status, NULL);
		return;
	}
#ifdef CONFIG_FAIR_GROUP_SCHED
	my_q = se_p->my_q;
#endif
	if (!my_q) {
		task = container_of(se_p, struct task_struct, se);
		dump_task_info(task, status, curr);
		return;
	}
	nr_running = my_q->nr_running;
	dump_align();
	SEQ_printf(sprd_rq_seq_buf, "%s: %d process is grouping\n", status, nr_running);
	align_offset++;
	dump_cfs_rq(my_q, curr);
	align_offset--;
}

static void dump_cfs_node_func(struct rb_node *node,
				    struct task_struct *curr)
{
	struct sched_entity *se_p = container_of(node, struct sched_entity,
						 run_node);

	dump_cgroup_state("pend", se_p, curr);
}

static void rb_walk_cfs(struct rb_root_cached *rb_root_cached_p,
			     struct task_struct *curr)
{
	int max_walk = 100;	/* Bail out, in case of loop */
	struct rb_node *leftmost = rb_root_cached_p->rb_leftmost;
	struct rb_root *root = &rb_root_cached_p->rb_root;
	struct rb_node *rb_node = rb_first(root);

	if (!leftmost)
		return;
	while (rb_node && max_walk--) {
		dump_cfs_node_func(rb_node, curr);
		rb_node = rb_next(rb_node);
	}
}

static void dump_cfs_rq(struct cfs_rq *cfs, struct task_struct *curr)
{
	struct rb_root_cached *rb_root_cached_p = &cfs->tasks_timeline;

	dump_cgroup_state("curr", cfs->curr, curr);
	dump_cgroup_state("next", cfs->next, curr);
	dump_cgroup_state("last", cfs->last, curr);
	dump_cgroup_state("skip", cfs->skip, curr);
	rb_walk_cfs(rb_root_cached_p, curr);
}

static void dump_rt_rq(struct rt_rq  *rt_rq, struct task_struct *curr)
{
	struct rt_prio_array *array = &rt_rq->active;
	struct sched_rt_entity *rt_se;
	int idx;

	/* Lifted most of the below code from dump_throttled_rt_tasks() */
	if (bitmap_empty(array->bitmap, MAX_RT_PRIO))
		return;

	idx = sched_find_first_bit(array->bitmap);
	while (idx < MAX_RT_PRIO) {
		list_for_each_entry(rt_se, array->queue + idx, run_list) {
			struct task_struct *p;

#ifdef CONFIG_RT_GROUP_SCHED
			if (rt_se->my_q)
				continue;
#endif

			p = container_of(rt_se, struct task_struct, rt);
			dump_task_info(p, "pend", curr);
		}
		idx = find_next_bit(array->bitmap, MAX_RT_PRIO, idx + 1);
	}
}

static void sprd_dump_runqueues(void)
{
	int cpu;
	struct rq *rq;
	struct rt_rq  *rt;
	struct cfs_rq *cfs;

	if (!sprd_runq_buf || !sprd_rq_seq_buf)
		return;

	for_each_possible_cpu(cpu) {
		rq = cpu_rq(cpu);
		rt = &rq->rt;
		cfs = &rq->cfs;
		SEQ_printf(sprd_rq_seq_buf, "CPU%d %d process is running\n",
					cpu, rq->nr_running);
		dump_task_info(cpu_curr(cpu), "curr", NULL);
		SEQ_printf(sprd_rq_seq_buf, " CFS %d process is pending\n", cfs->nr_running);
		dump_cfs_rq(cfs, cpu_curr(cpu));
		SEQ_printf(sprd_rq_seq_buf, " RT %d process is pending\n", rt->rt_nr_running);
		dump_rt_rq(rt, cpu_curr(cpu));
		SEQ_printf(sprd_rq_seq_buf, "\n");
	}
}

static void sprd_print_task_stats(int cpu, struct rq *rq, struct task_struct *p)
{
	struct seq_buf *task_seq_buf;

	task_seq_buf = sprd_task_seq_buf;

	SEQ_printf(task_seq_buf, "  %d ", cpu);
	if (rq->curr == p)
		SEQ_printf(task_seq_buf, ">R");
	else
		SEQ_printf(task_seq_buf, " %c", task_state_to_char(p));

	SEQ_printf(task_seq_buf, " %15s %5d %5d %13lld  ",
				p->comm, task_pid_nr(p), p->prio,
				(long long)(p->nvcsw + p->nivcsw));

#ifdef CONFIG_SCHED_INFO
	SEQ_printf(task_seq_buf, "%6lld.%09ld  %6lld.%09ld  %6lld.%09ld",
				nsec_high(p->sched_info.last_arrival),
				nsec_low(p->sched_info.last_arrival),
				nsec_high(p->sched_info.last_queued),
				nsec_low(p->sched_info.last_queued),
				nsec_high(p->sched_info.run_delay),
				nsec_low(p->sched_info.run_delay));
#endif
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(p->se.sum_exec_runtime),
				nsec_low(p->se.sum_exec_runtime));

#ifdef CONFIG_SCHED_WALT
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(p->last_sleep_ts),
				nsec_low(p->last_sleep_ts));
#endif
	SEQ_printf(task_seq_buf, "\n");
}

static void sprd_dump_task_stats(void)
{
	struct task_struct *g, *p;
	int cpu;
	struct rq *rq;

	if (!sprd_task_buf || !sprd_task_seq_buf)
		return;

	SEQ_printf(sprd_task_seq_buf, "cpu  S       task_comm   PID  prio   num_of_exec");
#ifdef CONFIG_SCHED_INFO
	SEQ_printf(sprd_task_seq_buf, "   exec_started_ts    last_queued_ts   total_wait_time ");
#endif
	SEQ_printf(sprd_task_seq_buf, "   total_exec_time");
#ifdef CONFIG_SCHED_WALT
	SEQ_printf(sprd_task_seq_buf, "      last_sleep_ts");
#endif
	SEQ_printf(sprd_task_seq_buf, "\n-----------------------------------------------------------"
		"---------------------------------------------------------------------------------\n");

	rcu_read_lock();
	for_each_process_thread(g, p) {
		cpu = task_cpu(p);
		rq = cpu_rq(cpu);

		sprd_print_task_stats(cpu, rq, p);
	}
	rcu_read_unlock();
}

#ifdef CONFIG_ARM64
/*
 * dump a block of kernel memory from around the given address
 */
static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;
	struct vm_struct *vaddr;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < KIMAGE_VADDR || addr > -256UL)
		return;

	if (addr > VMALLOC_START && addr < VMALLOC_END) {
		vaddr = find_vm_area_no_wait((const void *)addr);
		if (!vaddr || ((vaddr->flags & VM_IOREMAP) == VM_IOREMAP))
			return;
	}

	SEQ_printf(sprd_sr_seq_buf, "\n%s: %#lx:\n", name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;


	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		SEQ_printf(sprd_sr_seq_buf, "%04lx ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32 data;

			if (probe_kernel_address(p, data))
				SEQ_printf(sprd_sr_seq_buf, " ********");
			else
				SEQ_printf(sprd_sr_seq_buf, " %08x", data);

			++p;
		}
		SEQ_printf(sprd_sr_seq_buf, "\n");
	}
	flush_cache_all();
}

static void dump_extra_register_data(struct pt_regs *regs, int nbytes)
{
	mm_segment_t fs;
	unsigned int i;

	fs = get_fs();
	set_fs(KERNEL_DS);
	show_data(regs->pc - nbytes, nbytes * 2, "PC");
	show_data(regs->regs[30] - nbytes, nbytes * 2, "LR");
	show_data(regs->sp - nbytes, nbytes * 2, "SP");
	for (i = 0; i < 30; i++) {
		char name[4];

		snprintf(name, sizeof(name), "X%u", i);
		show_data(regs->regs[i] - nbytes, nbytes * 2, name);
	}
	set_fs(fs);
}

static void sprd_dump_regs(struct pt_regs *regs)
{
	int i, top_reg;
	u64 lr, sp;

	if (compat_user_mode(regs)) {
		lr = regs->compat_lr;
		sp = regs->compat_sp;
		top_reg = 12;
	} else {
		lr = regs->regs[30];
		sp = regs->sp;
		top_reg = 29;
	}
	SEQ_printf(sprd_sr_seq_buf, "pc : %016llx\n", regs->pc);
	SEQ_printf(sprd_sr_seq_buf, "lr : %016llx\n", lr);
	SEQ_printf(sprd_sr_seq_buf, "sp : %016llx pstate : %08llx\n", sp, regs->pstate);

	i = top_reg;
	while (i >= 0) {
		SEQ_printf(sprd_sr_seq_buf, "x%-2d: %016llx ", i, regs->regs[i]);
		i--;

		if (i % 2 == 0) {
			SEQ_printf(sprd_sr_seq_buf, "x%-2d: %016llx ", i, regs->regs[i]);
			i--;
		}

		SEQ_printf(sprd_sr_seq_buf, "\n");
	}
	if (!user_mode(regs))
		dump_extra_register_data(regs, 128);
	SEQ_printf(sprd_sr_seq_buf, "\n");
}
#else
static const char *processor_modes[] __maybe_unused = {
"USER_26", "FIQ_26", "IRQ_26",  "SVC_26",  "UK4_26",  "UK5_26",  "UK6_26",  "UK7_26",
"UK8_26",  "UK9_26", "UK10_26", "UK11_26", "UK12_26", "UK13_26", "UK14_26", "UK15_26",
"USER_32", "FIQ_32", "IRQ_32",  "SVC_32",  "UK4_32",  "UK5_32",  "MON_32",  "ABT_32",
"UK8_32",  "UK9_32", "HYP_32",  "UND_32",  "UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static const char *isa_modes[] __maybe_unused = {
"ARM", "Thumb", "Jazelle", "ThumbEE"
};

static void sprd_dump_regs(struct pt_regs *regs)
{
	unsigned long flags;
	char buf[64];
#ifndef CONFIG_CPU_V7M
	unsigned int domain, fs;
#ifdef CONFIG_CPU_SW_DOMAIN_PAN
	/*
	 * Get the domain register for the parent context. In user
	 * mode, we don't save the DACR, so lets use what it should
	 * be. For other modes, we place it after the pt_regs struct.
	 */
	if (user_mode(regs)) {
		domain = DACR_UACCESS_ENABLE;
		fs = get_fs();
	} else {
		domain = to_svc_pt_regs(regs)->dacr;
		fs = to_svc_pt_regs(regs)->addr_limit;
	}
#else
	domain = get_domain();
	fs = get_fs();
#endif
#endif
	SEQ_printf(sprd_sr_seq_buf, "pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n",
	       regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr);
	SEQ_printf(sprd_sr_seq_buf, "sp : %08lx  ip : %08lx  fp : %08lx\n",
	       regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	SEQ_printf(sprd_sr_seq_buf, "r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	SEQ_printf(sprd_sr_seq_buf, "r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);
	SEQ_printf(sprd_sr_seq_buf, "r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2,
		regs->ARM_r1, regs->ARM_r0);

	flags = regs->ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

#ifndef CONFIG_CPU_V7M
	{
		const char *segment;

		if ((domain & domain_mask(DOMAIN_USER)) ==
		    domain_val(DOMAIN_USER, DOMAIN_NOACCESS))
			segment = "none";
		else if (fs == get_ds())
			segment = "kernel";
		else
			segment = "user";

		SEQ_printf(sprd_sr_seq_buf, "Flags: %s  IRQs o%s  FIQs o%s  Mode %s  ISA %s  Segment %s\n",
			buf, interrupts_enabled(regs) ? "n" : "ff",
			fast_interrupts_enabled(regs) ? "n" : "ff",
			processor_modes[processor_mode(regs)],
			isa_modes[isa_mode(regs)], segment);
	}
#else
	SEQ_printf(sprd_sr_seq_buf, "xPSR: %08lx\n", regs->ARM_cpsr);
#endif

#ifdef CONFIG_CPU_CP15
	{
		unsigned int ctrl;

		buf[0] = '\0';
#ifdef CONFIG_CPU_CP15_MMU
		{
			unsigned int transbase;

			asm("mrc p15, 0, %0, c2, c0\n\t"
			    : "=r" (transbase));
			snprintf(buf, sizeof(buf), "  Table: %08x  DAC: %08x",
				transbase, domain);
		}
#endif
		asm("mrc p15, 0, %0, c1, c0\n" : "=r" (ctrl));

		SEQ_printf(sprd_sr_seq_buf, "Control: %08x%s\n", ctrl, buf);
	}
#endif
	SEQ_printf(sprd_sr_seq_buf, "\n");
}
#endif
void sprd_dump_stack_reg(int cpu, struct pt_regs *pregs)
{
	int i;
	struct stackframe frame;
	unsigned long sp;

	raw_spin_lock(&dump_lock);
	if (!sprd_stack_reg_buf || !sprd_sr_seq_buf)
		goto unlock;

	SEQ_printf(sprd_sr_seq_buf, "-----cpu%d stack info-----\n", cpu);

#ifdef CONFIG_ARM64
	frame.fp = pregs->regs[29];
	frame.pc = pregs->pc;
	sp = pregs->sp;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = 0;
#endif
#else
	frame.fp = pregs->ARM_fp;
	frame.sp = pregs->ARM_sp;
	frame.lr = pregs->ARM_lr;
	frame.pc = pregs->ARM_pc;
	sp = pregs->ARM_sp;
#endif


#ifdef CONFIG_VMAP_STACK
	if (!((sp >= VMALLOC_START) && (sp < VMALLOC_END))) {
		SEQ_printf(sprd_sr_seq_buf, "%s sp out of kernel addr space %08lx\n", __func__, sp);
		goto unlock;
	}
#else
	if (!sprd_virt_addr_valid(sp)) {
		SEQ_printf(sprd_sr_seq_buf, "invalid sp[%lx]\n", sp);
		goto unlock;
	}
#endif

	SEQ_printf(sprd_sr_seq_buf, "callstack:\n");
	SEQ_printf(sprd_sr_seq_buf, "[<%08lx>] (%ps)\n", frame.pc, (void *)frame.pc);

	for (i = 0; i < MAX_CALLBACK_LEVEL; i++) {
		int urc;
#ifdef CONFIG_ARM64
		urc = unwind_frame(NULL, &frame);
#else
		urc = unwind_frame(&frame);
#endif
		if (urc < 0)
			break;


		if (!sprd_virt_addr_valid(frame.pc)) {
			SEQ_printf(sprd_sr_seq_buf, "i=%d, sprd_virt_addr_valid fail\n", i);
			break;
		}

		SEQ_printf(sprd_sr_seq_buf, "[<%08lx>] (%ps)\n", frame.pc, (void *)frame.pc);
	}

	SEQ_printf(sprd_sr_seq_buf, "\n-----cpu%d regs info-----\n", cpu);
	sprd_dump_regs(pregs);
unlock:
	raw_spin_unlock(&dump_lock);
}

static int sprd_sched_panic_event(struct notifier_block *self,
				  unsigned long val, void *reason)
{
	pr_crit("Dump runqueue/task stats...\n");
	sprd_dump_runqueues();
	sprd_dump_task_stats();

	return NOTIFY_DONE;
}

static struct notifier_block sprd_sched_panic_event_nb = {
	.notifier_call	= sprd_sched_panic_event,
	.priority	= INT_MAX - 1,
};

static int __init sprd_dump_sched_init(void)
{
	int ret;

	sprd_task_buf = kzalloc(SPRD_DUMP_TASK_SIZE, GFP_KERNEL);
	if (!sprd_task_buf)
		return -EINVAL;

	sprd_task_seq_buf = kzalloc(sizeof(*sprd_task_seq_buf), GFP_KERNEL);
	if (!sprd_task_seq_buf) {
		ret = -EINVAL;
		goto err_task_seq;
	}

	sprd_runq_buf = kzalloc(SPRD_DUMP_RQ_SIZE, GFP_KERNEL);
	if (!sprd_runq_buf) {
		ret = -EINVAL;
		goto err_runq;
	}

	sprd_rq_seq_buf = kzalloc(sizeof(*sprd_rq_seq_buf), GFP_KERNEL);
	if (!sprd_rq_seq_buf) {
		ret = -EINVAL;
		goto err_rq_seq;
	}

	sprd_stack_reg_buf = kzalloc(SPRD_DUMP_STACK_SIZE, GFP_KERNEL);
	if (!sprd_stack_reg_buf) {
		ret = -EINVAL;
		goto err_sr_buf;
	}

	sprd_sr_seq_buf = kzalloc(sizeof(*sprd_sr_seq_buf), GFP_KERNEL);
	if (!sprd_sr_seq_buf) {
		ret = -EINVAL;
		goto err_sr_seq;
	}

	if (minidump_add_section("task_stats", (unsigned long)(sprd_task_buf), SPRD_DUMP_TASK_SIZE) ||
	    minidump_add_section("runqueue", (unsigned long)(sprd_runq_buf), SPRD_DUMP_RQ_SIZE) ||
	    minidump_add_section("stack_regs", (unsigned long)(sprd_stack_reg_buf), SPRD_DUMP_STACK_SIZE)) {
		ret = -EINVAL;
		goto err_save;
	}

	seq_buf_init(sprd_task_seq_buf, sprd_task_buf, SPRD_DUMP_TASK_SIZE);
	seq_buf_init(sprd_rq_seq_buf, sprd_runq_buf, SPRD_DUMP_RQ_SIZE);
	seq_buf_init(sprd_sr_seq_buf, sprd_stack_reg_buf, SPRD_DUMP_STACK_SIZE);

	/* register sched panic notifier */
	atomic_notifier_chain_register(&panic_notifier_list,
					&sprd_sched_panic_event_nb);

	pr_info("sprd: sched_panic_nofifier_register success\n");

	return 0;

err_save:
	kfree(sprd_sr_seq_buf);
err_sr_seq:
	kfree(sprd_stack_reg_buf);
err_sr_buf:
	kfree(sprd_rq_seq_buf);
err_rq_seq:
	kfree(sprd_runq_buf);
err_runq:
	kfree(sprd_task_seq_buf);
err_task_seq:
	kfree(sprd_task_buf);

	return ret;
}

static void __exit sprd_dump_sched_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &sprd_sched_panic_event_nb);
	kfree(sprd_task_buf);
	kfree(sprd_task_seq_buf);
	kfree(sprd_runq_buf);
	kfree(sprd_rq_seq_buf);
	kfree(sprd_stack_reg_buf);
	kfree(sprd_sr_seq_buf);
}

module_init(sprd_dump_sched_init);
module_exit(sprd_dump_sched_exit);

MODULE_DESCRIPTION("kernel sched stats for Unisoc");
MODULE_LICENSE("GPL");
