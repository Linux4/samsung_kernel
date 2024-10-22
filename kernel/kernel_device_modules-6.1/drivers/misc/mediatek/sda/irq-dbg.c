// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/sched.h>
#include <linux/preempt.h>
#include <linux/sched/cputime.h>
#include <linux/sched/debug.h>
#include <sched/sched.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/stacktrace.h>

#define MAX_CORE_NUM	(8)

enum gic_smc_index {
	GIC_CPU_DUMP = 0,
	FIND_ACTIVE_IRQ,
};

enum smc_ret_info_offset {
	GIC_MASK = 0x0,
	GIC_GROUP = 0x1,
	GIC_GROUPM = 0x2,
	GIC_PRIORITY = 0x3,
	GIC_SENS = 0xb,
	GIC_PENDING = 0xc,
	GIC_ACTIVE = 0xd,
	GIC_TARGET_CPU = 0xe,
	CPU_I_BIT = 0x1e,
	CPU_POWER_STATE = 0x1f,
	GIC_WAKEUP_SIG = 0x20,
	ADV_IRQ_DBG_SUPPORT = 0x3f,
};

static unsigned int gic_irq(struct irq_data *d)
{
	return d->hwirq;
}

static int virq_to_hwirq(unsigned int virq)
{
	struct irq_desc *desc;
	unsigned int hwirq = 0;

	desc = irq_to_desc(virq);
	if (desc)
		hwirq = gic_irq(&desc->irq_data);
	else
		WARN_ON(!desc);

	return hwirq;
}

static bool is_cpu_in_interrupt_context(unsigned int cpu)
{
	return ((task_thread_info(cpu_curr(cpu))->preempt.count)&HARDIRQ_MASK);
}

static int get_cpu_num(unsigned int router)
{
	int cpu;

	for (cpu = 0; cpu < MAX_CORE_NUM; cpu++) {
		if (router & (0x1 << cpu))
			return cpu;
	}

	return -1;
}

static const char *get_suspected_irq_name(unsigned int irq, unsigned int hwirq)
{
	struct irq_desc *desc;
	struct irq_domain *domain;

	desc = irq_to_desc(irq);

	if (!desc) {
		pr_info("[%s] first irq desc is null\n", __func__);
		return NULL;
	}

	domain = desc->irq_data.domain;

	if (!domain) {
		pr_info("[%s] irq domain is null\n", __func__);
		return NULL;
	}

	desc = irq_resolve_mapping(domain, hwirq);

	if (!desc) {
		pr_info("[%s] second irq desc is null\n", __func__);
		return NULL;
	}

	return desc->action->name;
}

static int mt_irq_dump_status_buf(unsigned int irq, char *buf)
{
	unsigned int result, pending, enable, wakeup_sig_asserted;
	unsigned int hwirq, intid;
	unsigned int i_bit, cpu, is_cpu_on;
	int num = 0;
	char *ptr = buf, tsk_name[TASK_COMM_LEN];
	struct arm_smccc_res res;
	unsigned long stacks[32];
	int nr_entries;
	int i;
	struct task_struct *task;

	if (!ptr)
		return 0;

	hwirq = virq_to_hwirq(irq);
	if (!hwirq)
		return 0;

	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] hwirq = %d, virq: %d\n", hwirq, irq);
	if (num == PAGE_SIZE)
		goto OUT;

	arm_smccc_smc(MTK_SIP_KERNEL_GIC_DUMP,
			GIC_CPU_DUMP, hwirq, 0, 0, 0, 0, 0, &res);
	if (res.a0 == 0) {
		num += snprintf(ptr + num, PAGE_SIZE - num,
				"[mt gic dump] not allowed to dump!\n");
		if (num == PAGE_SIZE)
			goto OUT;

		return num;
	}

	/* get mask */
	result = (res.a0 >> GIC_MASK) & 0x1;
	enable = result;
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] enable = %d\n", result);
	if (num == PAGE_SIZE)
		goto OUT;

	/* get group */
	result = (res.a0 >> GIC_GROUP) & 0x1;
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] group = %d\n", result);
	if (num == PAGE_SIZE)
		goto OUT;

	/* get group modifier */
	result = (res.a0 >> GIC_GROUPM) & 0x1;
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] group modifier = %d\n", result);
	if (num == PAGE_SIZE)
		goto OUT;

	/* get priority */
	result = (res.a0 >> GIC_PRIORITY) & 0xff;
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] priority = %d\n", result);
	if (num == PAGE_SIZE)
		goto OUT;

	/* get sensitivity */
	result = (res.a0 >> GIC_SENS) & 0x1;
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] sensitivity = %x ", result);
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"(edge:0x1, level:0x0)\n");
	if (num == PAGE_SIZE)
		goto OUT;

	/* get pending status */
	result = (res.a0 >> GIC_PENDING) & 0x1;
	pending = result;
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] pending = %x\n", result);
	if (num == PAGE_SIZE)
		goto OUT;

	/* get active status */
	result = (res.a0 >> GIC_ACTIVE) & 0x1;
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] active status = %x\n", result);
	if (num == PAGE_SIZE)
		goto OUT;

	/* get target cpu */
	result = (res.a0 >> GIC_TARGET_CPU) & 0xffff;
	if (result != 0xffff)
		cpu = get_cpu_num(result);
	else
		cpu = result;
	num += snprintf(ptr + num, PAGE_SIZE - num,
			"[mt gic dump] target cpu = %x\n", cpu);
	if (num == PAGE_SIZE)
		goto OUT;

	/* check if support advanced irq debug api */
	if (!((res.a0 >> ADV_IRQ_DBG_SUPPORT) & 0x1))
		return num;

	/* for enable & pending=1 scenario to dump more info */
	if (pending==1 && enable==1) {
		if (cpu==0xffff) {
			num += snprintf(ptr + num, PAGE_SIZE - num,
					"[mt gic dump] No support for advanced irq debug dump for 1 of N irq\n");
			if (num == PAGE_SIZE)
				goto OUT;
			return num;
		}

		/* get CPSR.I bit of target cpu from dbg mon reg */
		i_bit = (res.a0 >> CPU_I_BIT) & 0x1;

		if (i_bit) {
			if (is_cpu_in_interrupt_context(cpu)) {
				/* SMC to EL3 to find active SPI GIC ID in CPUn */
				arm_smccc_smc(MTK_SIP_KERNEL_GIC_DUMP,
						FIND_ACTIVE_IRQ, cpu, 0, 0, 0, 0, 0, &res);

				if (res.a0 == 0) {
					num += snprintf(ptr + num, PAGE_SIZE - num,
							"[mt gic dump] not allowed to dump!\n");
					if (num == PAGE_SIZE)
						goto OUT;

					return num;
				}

				if (res.a0 == 1) {
					num += snprintf(ptr + num, PAGE_SIZE - num,
							"[mt gic dump] active SPI GIC ID not found!\n");
					if (num == PAGE_SIZE)
						goto OUT;

					return num;
				}

				/* get active SPI GIC ID */
				intid = res.a0 & 0x3ff;
				num += snprintf(ptr + num, PAGE_SIZE - num,
						"[mt gic dump] hwirq %d is blocked by hwirq %d (irq name:%s)\n",
						hwirq, intid, get_suspected_irq_name(irq, intid));
				if (num == PAGE_SIZE)
					goto OUT;
			} else {
				num += snprintf(ptr + num, PAGE_SIZE - num,
						"[mt gic dump] hwirq %d is blocked by a task with the following info\n",
						hwirq);
				if (num == PAGE_SIZE)
					goto OUT;

				task = cpu_curr(cpu);
				/* get task name and target cpu */
				get_task_comm(tsk_name, task);
				num += snprintf(ptr + num, PAGE_SIZE - num,
						"[mt gic dump] task name:%s     target cpu:%d\n",
						tsk_name, task_cpu(task));
				if (num == PAGE_SIZE)
					goto OUT;

				/* dump the backtrace of the task */
				num += snprintf(ptr + num, PAGE_SIZE - num,
						"[mt gic dump] Call trace:\n");
				nr_entries = stack_trace_save_tsk(cpu_curr(cpu), stacks, ARRAY_SIZE(stacks), 0);
				for (i = 0; i < nr_entries; i++) {
					num += snprintf(ptr + num, PAGE_SIZE - num,
							"[mt gic dump] %pS\n", (void *)stacks[i]);
					if (num == PAGE_SIZE)
						goto OUT;
				}
			}
		} else {
			/* get the power status of cpu */
			is_cpu_on = (res.a0 >> CPU_POWER_STATE) & 0x1;
			if (is_cpu_on) {
				num += snprintf(ptr + num, PAGE_SIZE - num,
						"[mt gic dump] CPU is on and CPSR.I = 0x%x\n", i_bit);
				if (num == PAGE_SIZE)
					goto OUT;
			} else {
				/* get gic wakeup signal status from cpc reg */
				wakeup_sig_asserted = (res.a0 >> GIC_WAKEUP_SIG) & 0x1;
				num += snprintf(ptr + num, PAGE_SIZE - num,
						"[mt gic dump] CPU is off, wakeup_sig_asserted = 0x%x\n",
						wakeup_sig_asserted);
				if (num == PAGE_SIZE)
					goto OUT;
			}
		}
	}

	return num;

OUT:
	pr_info("[mt gic dump] buffer is full\n");

	return num;

}

void mt_irq_dump_status(unsigned int irq)
{
	char *buf = kmalloc(PAGE_SIZE, GFP_ATOMIC);

	if (!buf)
		return;

	/* support GIC PPI/SPI only */
	if (mt_irq_dump_status_buf(irq, buf) > 0)
		pr_info("%s", buf);

	kfree(buf);

}
EXPORT_SYMBOL(mt_irq_dump_status);

static int __init irq_dbg_init(void)
{
	return 0;
}

static __exit void irq_dbg_exit(void)
{
}

module_init(irq_dbg_init);
module_exit(irq_dbg_exit);


MODULE_DESCRIPTION("MediaTek IRQ dbg Driver");
MODULE_LICENSE("GPL v2");
