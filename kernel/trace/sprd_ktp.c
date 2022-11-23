// SPDX-License-Identifier: GPL-2.0
/*
 * irq/switch info for Spreadtrum SoCs
 *
 * Copyright (C) 2021 Spreadtrum corporation. http://www.unisoc.com
 */

#define pr_fmt(fmt)  "sprd-ktp: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/kmsg_dump.h>
#include <linux/seq_buf.h>
#include <linux/mm.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_sysdump.h>
#include <linux/sprd_ktp.h>

#define SPRD_TRACE_SIZE		524288 //512k
#define PER_ITEM_SIZE		64
struct sprd_ktp_state {
	long	vaddr;
	char	*buffer;
	int	items;
	int	buf_pos;
	int	enabled;
	int	initialized;
	uint32_t filter;
};

static atomic_t sprd_ktp_idx;

static struct sprd_ktp_state sprd_ktp = {
	.filter = (1 << KTP_CTXID) | (1 << KTP_IRQ),
	.enabled = 1,
};

module_param_named(enable, sprd_ktp.enabled, int, 0644);
module_param_named(filter, sprd_ktp.filter, uint, 0644);

static int notrace sprd_ktp_event_should_log(enum ktp_event_type ktp_type)
{
	return sprd_ktp.enabled && sprd_ktp.initialized &&
		((1 << (ktp_type & ~KTYPE_NOPC)) & sprd_ktp.filter);
}

static void kevent_tp_idx(enum ktp_event_type ktp_type, void *data, int idx)
{
	char *start;
	int cpu = smp_processor_id();
	u64 timestamp = sched_clock();
	u64 sec, usec;

	start = &sprd_ktp.buffer[(idx & (sprd_ktp.items - 1)) * PER_ITEM_SIZE];
	memset(start, ' ', PER_ITEM_SIZE);

	do_div(timestamp, NSEC_PER_USEC);

	sec = timestamp;
	usec = do_div(sec, USEC_PER_SEC);

	/* add timestamp header */
	snprintf(start, 24, "[%9d.%06d] c%1d: ", (int)sec, (int)usec, cpu);

	switch (ktp_type) {
	case KTP_NONE:
		break;
	case KTP_CTXID:
		snprintf(start + 23, PER_ITEM_SIZE - 23,
			 "switch:prev_pid=%d,next_pid=%lld",
			 current->pid, (u64)data);
		break;
	case KTP_IRQ:
		snprintf(start + 23, PER_ITEM_SIZE - 23,
			 "irq: interrupt %d handler(gic-v3)", (int)data);
		break;
	default:
		break;
	}
	start[PER_ITEM_SIZE - 1] = '\n';
	mb();
}

noinline int notrace kevent_tp(enum ktp_event_type ktp_type, void *data)
{
	int idx;

	if (!sprd_ktp_event_should_log(ktp_type))
		return 0;

	idx = atomic_inc_return(&sprd_ktp_idx);
	idx--;

	kevent_tp_idx(ktp_type, data, idx);

	return 1;
}

static int sprd_ktp_panic_event(struct notifier_block *self,
				unsigned long val, void *reason)
{
	sprd_ktp.enabled = 0;
	return NOTIFY_DONE;
}

static struct notifier_block sprd_ktp_panic_event_nb = {
	.notifier_call	= sprd_ktp_panic_event,
	.priority	= INT_MAX,
};

static int __init sprd_ktp_init(void)
{
	int ret;

	sprd_ktp.buffer = kzalloc(SPRD_TRACE_SIZE, GFP_KERNEL);
	if (!sprd_ktp.buffer)
		return -EINVAL;

	sprd_ktp.vaddr = (long)(sprd_ktp.buffer);
	pr_info("%s in. vaddr : 0x%lx  len :0x%x\n",
		 __func__, sprd_ktp.vaddr, SPRD_TRACE_SIZE);
	ret = minidump_save_extend_information("sprd_ktp", __pa(sprd_ktp.vaddr),
						__pa(sprd_ktp.vaddr + SPRD_TRACE_SIZE));
	if (ret) {
		kfree(sprd_ktp.buffer);
		goto out;
	}

	sprd_ktp.items = SPRD_TRACE_SIZE / PER_ITEM_SIZE;
	/* Round this down to a power of 2 */
	sprd_ktp.items = __rounddown_pow_of_two(sprd_ktp.items);

	atomic_set(&sprd_ktp_idx, 0);

	atomic_notifier_chain_register(&panic_notifier_list,
					&sprd_ktp_panic_event_nb);
	sprd_ktp.initialized = 1;
	pr_info("kernel trace point added to minidump section ok!!\n");
out:
	return ret;
}

static void __exit sprd_ktp_exit(void)
{
	if (sprd_ktp.initialized) {
		sprd_ktp.initialized = 0;
		atomic_notifier_chain_unregister(&panic_notifier_list,
						&sprd_ktp_panic_event_nb);
	}
	kfree(sprd_ktp.buffer);
}

module_init(sprd_ktp_init);
module_exit(sprd_ktp_exit);

MODULE_DESCRIPTION("kernel_trace_point for Unisoc");
MODULE_LICENSE("GPL");
