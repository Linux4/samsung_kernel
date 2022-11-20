/*
 * linux/arch/arm/mach-exynos/exynos-coresight.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/debug-snapshot.h>

#include <asm/core_regs.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-coresight.h>

#define CS_READ(base, offset)		__raw_readl(base + offset)
#define CS_READQ(base, offset)		__raw_readq(base + offset)
#define CS_WRITE(val, base, offset)	__raw_writel(val, base + offset)

#define SYS_READ(reg, val)	asm volatile("mrs %0, " #reg : "=r" (val))
#define SYS_WRITE(reg, val)	asm volatile("msr " #reg ", %0" :: "r" (val))

#define DBG_UNLOCK(base)	\
	do { isb(); __raw_writel(OSLOCK_MAGIC, base + DBGLAR); }while(0)
#define DBG_LOCK(base)		\
	do { __raw_writel(0x1, base + DBGLAR); isb(); }while(0)

#define DBG_REG_MAX_SIZE	(8)
#define DBG_BW_REG_MAX_SIZE	(30)
#define OS_LOCK_FLAG		(DBG_REG_MAX_SIZE - 1)
#define ITERATION		CONFIG_PC_ITERATION
#define CORE_CNT		CONFIG_NR_CPUS
#define MAX_CPU			(8)
#define MSB_PADDING		(0xFFFFFF0000000000)
#define MSB_MASKING		(0x0000FF0000000000)
#define MIDR_ARCH_MASK		(0xfffff)

struct cs_dbg_cpu {
	void __iomem		*base;
};

struct cs_dbg {
	struct device *dev;
	u32			arch;
	bool			enabled;
	struct cs_dbg_cpu	cpu[MAX_CPU];
};

struct cs_core_state {
	bool			is_hp_out;
	bool			is_c2;
	raw_spinlock_t		lock;
};

static DEFINE_PER_CPU(struct cs_core_state, cs_core_state) =
{
	.lock = __RAW_SPIN_LOCK_UNLOCKED(cs_core_state.lock),
};

static struct cs_dbg dbg;

#ifdef CONFIG_LOCKUP_DETECTOR
extern struct atomic_notifier_head hardlockup_notifier_list;
#endif

static inline void get_arm_arch_version(int cpu)
{
	dbg.arch = CS_READ(dbg.cpu[0].base, MIDR);
	dbg.arch = dbg.arch & MIDR_ARCH_MASK;
}

static inline void dbg_os_lock(void __iomem *base)
{
	switch (dbg.arch) {
	case ARMV8_PROCESSOR:
		CS_WRITE(0x1, base, DBGOSLAR);
		break;
	default:
		break;
	}
	isb();
}

static inline void dbg_os_unlock(void __iomem *base)
{
	isb();
	switch (dbg.arch) {
	case ARMV8_PROCESSOR:
		CS_WRITE(0x0, base, DBGOSLAR);
		break;
	default:
		break;
	}
}

static inline bool is_power_up(int cpu)
{
	return CS_READ(dbg.cpu[cpu].base, DBGPRSR) & POWER_UP;
}

static inline bool is_reset_state(int cpu)
{
	return CS_READ(dbg.cpu[cpu].base, DBGPRSR) & RESET_STATE;
}

struct exynos_cs_pcsr {
	unsigned long pc;
	int ns;
	int el;
};
static struct exynos_cs_pcsr exynos_cs_pc[CORE_CNT][ITERATION];

static inline int exynos_cs_get_cpu_part_num(int cpu)
{
	u32 midr = CS_READ(dbg.cpu[cpu].base, MIDR);

	return MIDR_PARTNUM(midr);
}

static inline bool have_pc_offset(void __iomem *base)
{
	 return !(CS_READ(base, DBGDEVID1) & 0xf);
}

static int exynos_cs_get_pc(int cpu, int iter)
{
	void __iomem *base = dbg.cpu[cpu].base;
	unsigned long val = 0, valHi;
	int ns = -1, el = -1;

	if (!base)
		return -ENOMEM;

	if (!is_power_up(cpu)) {
		dev_err(dbg.dev, "Power down!\n");
		return -EACCES;
	}

	if (is_reset_state(cpu)) {
		dev_err(dbg.dev, "Power on but reset state!\n");
		return -EACCES;
	}

	switch (exynos_cs_get_cpu_part_num(cpu)) {
	case ARM_CPU_PART_MONGOOSE:
	case ARM_CPU_PART_MEERKAT:
	case ARM_CPU_PART_CORTEX_A53:
	case ARM_CPU_PART_CORTEX_A57:
		DBG_UNLOCK(base);
		dbg_os_unlock(base);

		val = CS_READ(base, DBGPCSRlo);
		valHi = CS_READ(base, DBGPCSRhi);

		val |= (valHi << 32L);
		if (have_pc_offset(base))
			val -= 0x8;
		if (MSB_MASKING == (MSB_MASKING & val))
			val |= MSB_PADDING;

		dbg_os_lock(base);
		DBG_LOCK(base);
		break;
	case ARM_CPU_PART_CORTEX_A55:
	case ARM_CPU_PART_CORTEX_A75:
	case ARM_CPU_PART_CORTEX_A76:
	case ARM_CPU_PART_CORTEX_A77:
		DBG_UNLOCK(base);
		dbg_os_unlock(base);
		DBG_UNLOCK(base + PMU_OFFSET);

		val = CS_READQ(base + PMU_OFFSET, PMUPCSR);
		ns = (val >> 63L) & 0x1;
		el = (val >> 61L) & 0x3;
		if (MSB_MASKING == (MSB_MASKING & val))
			val |= MSB_PADDING;

		DBG_LOCK(base + PMU_OFFSET);
		break;
	default:
		break;
	}

	exynos_cs_pc[cpu][iter].pc = val;
	exynos_cs_pc[cpu][iter].ns = ns;
	exynos_cs_pc[cpu][iter].el = el;

	return 0;
}

#ifdef CONFIG_LOCKUP_DETECTOR
static int exynos_cs_lockup_handler(struct notifier_block *nb,
					unsigned long l, void *core)
{
	unsigned long val = 0, iter;
	char buf[KSYM_SYMBOL_LEN];
	unsigned int *cpu = (unsigned int *)core;
	int ret = 0;

	pr_auto(ASL5, "CPU[%d] saved pc value\n", *cpu);
	for (iter = 0; iter < ITERATION; iter++) {
#ifdef CONFIG_EXYNOS_PMU
		if (!exynos_cpu.power_state(*cpu))
			continue;
#endif
		ret = exynos_cs_get_pc(*cpu, iter);
		if (ret < 0)
			continue;

		val = exynos_cs_pc[*cpu][iter].pc;
		sprint_symbol(buf, val);
		pr_auto(ASL5, "      0x%016zx : %s\n", val, buf);
	}

	pr_auto(ASL5, "\n");

	return 0;
}

static struct notifier_block exynos_cs_lockup_nb = {
	.notifier_call = exynos_cs_lockup_handler,
};
#endif

static int exynos_cs_panic_handler(struct notifier_block *np,
				unsigned long l, void *msg)
{
	unsigned long flags, val;
	unsigned int cpu, iter, curr_cpu;
	char buf[KSYM_SYMBOL_LEN];

	if (num_online_cpus() <= 1)
		return 0;

	local_irq_save(flags);
	curr_cpu = raw_smp_processor_id();
	for (iter = 0; iter < ITERATION; iter++) {
		for (cpu = 0; cpu < CORE_CNT; cpu++) {
			exynos_cs_pc[cpu][iter].pc = 0;
#ifdef CONFIG_EXYNOS_PMU
			if (cpu == curr_cpu || !exynos_cpu.power_state(cpu))
#else
			if (cpu == curr_cpu)
#endif
				continue;

			if (exynos_cs_get_pc(cpu, iter) < 0)
				continue;
		}
	}

	local_irq_restore(flags);
	for (cpu = 0; cpu < CORE_CNT; cpu++) {
		dev_err(dbg.dev, "CPU[%d] saved pc value\n", cpu);
		for (iter = 0; iter < ITERATION; iter++) {
			val = exynos_cs_pc[cpu][iter].pc;
			if (!val)
				continue;

			sprint_symbol(buf, val);
			dev_err(dbg.dev, "      0x%016zx : %s\n", val, buf);
		}
	}
	return 0;
}

static struct notifier_block exynos_cs_panic_nb = {
	.notifier_call = exynos_cs_panic_handler,
};

unsigned long exynos_cs_read_pc(int cpu)
{
	struct cs_core_state *state;
	unsigned long flags;
	unsigned long target_cpu_pc;

	if (!dbg.enabled)
		return 0;

	state = per_cpu_ptr(&cs_core_state, cpu);

	raw_spin_lock_irqsave(&state->lock, flags);
	if (cpu_is_offline(cpu) || !exynos_cpu.power_state(cpu) ||
					state->is_hp_out || state->is_c2) {
		dev_emerg(dbg.dev, "%s: cpu%d is turned off : "
			"c2:[%x], hot-plug out:[%x], power:[%x] ,offline:[%ld]\n",
			__func__, cpu, state->is_c2, state->is_hp_out,
			exynos_cpu.power_state(cpu), cpu_is_offline(cpu));
		target_cpu_pc = 0;
	} else {
		dev_emerg(dbg.dev, "%s: cpu%d is power on\n", __func__, cpu);
		exynos_cs_get_pc(cpu, 1);
		target_cpu_pc = exynos_cs_pc[cpu][1].pc;
		dev_emerg(dbg.dev, "%s: cpu%d PC = [0x%lx]\n", __func__, cpu, target_cpu_pc);
	}
	raw_spin_unlock_irqrestore(&state->lock, flags);

	return target_cpu_pc;
}

static void exynos_cs_c2_enter(int cpu)
{
	struct cs_core_state *state;
	unsigned long flags;

	state = per_cpu_ptr(&cs_core_state, cpu);

	raw_spin_lock_irqsave(&state->lock, flags);
	state->is_c2 = 1;
	raw_spin_unlock_irqrestore(&state->lock, flags);
}

static void exynos_cs_c2_exit(int cpu)
{
	struct cs_core_state *state;
	unsigned long flags;

	state = per_cpu_ptr(&cs_core_state, cpu);

	raw_spin_lock_irqsave(&state->lock, flags);
	state->is_c2 = 0;
	raw_spin_unlock_irqrestore(&state->lock, flags);
}

static int exynos_cs_c2_pm_notifier(struct notifier_block *self,
					unsigned long action, void *v)
{
	int cpu = raw_smp_processor_id();

	switch (action) {
	case CPU_PM_ENTER:
		exynos_cs_c2_enter(cpu);
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		exynos_cs_c2_exit(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER:
		exynos_cs_c2_enter(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		exynos_cs_c2_exit(cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_cs_c2_pm_nb = {
	.notifier_call = exynos_cs_c2_pm_notifier,
};

static int exynos_cs_start_cpu(unsigned int cpu)
{
	struct cs_core_state *state;
	unsigned long flags;

	state = per_cpu_ptr(&cs_core_state, cpu);

	raw_spin_lock_irqsave(&state->lock, flags);
	state->is_hp_out = 0;
	raw_spin_unlock_irqrestore(&state->lock, flags);

	return 0;
}

static int exynos_cs_stop_cpu(unsigned int cpu)
{
	struct cs_core_state *state;
	unsigned long flags;

	state = per_cpu_ptr(&cs_core_state, cpu);

	raw_spin_lock_irqsave(&state->lock, flags);
	state->is_hp_out = 1;
	raw_spin_unlock_irqrestore(&state->lock, flags);

	return 0;
}

static const struct of_device_id of_exynos_cs_matches[] __initconst= {
	{.compatible = "exynos,coresight"},
	{},
};

static int exynos_cs_init_dt(void)
{
	struct device_node *np = NULL;
	unsigned int offset, sj_offset, val, cs_reg_base;
	int ret = 0, i = 0;
	void __iomem *sj_base;

	np = of_find_matching_node(NULL, of_exynos_cs_matches);

	if (of_property_read_u32(np, "base", &cs_reg_base))
		return -EINVAL;

	if (of_property_read_u32(np, "sj-offset", &sj_offset))
		sj_offset = CS_SJTAG_OFFSET;

	sj_base = ioremap(cs_reg_base + sj_offset, SZ_8);
	if (!sj_base) {
		dev_err(dbg.dev, "%s: Failed ioremap sj-offset.\n", __func__);
		return -ENOMEM;
	}

	val = __raw_readl(sj_base + SJTAG_STATUS);
	iounmap(sj_base);

	if (val & SJTAG_SOFT_LOCK)
		return -EIO;

	if (dbg_snapshot_get_sjtag_status() == true)
		return -EIO;

	while ((np = of_find_node_by_type(np, "cs"))) {
		ret = of_property_read_u32(np, "dbg-offset", &offset);
		if (ret)
			return -EINVAL;
		dbg.cpu[i].base = ioremap(cs_reg_base + offset, SZ_256K);
		if (!dbg.cpu[i].base) {
			dev_err(dbg.dev, "%s: Failed ioremap (%d).\n", __func__, i);
			return -ENOMEM;
		}

		i++;
	}
	return 0;
}

static int __init exynos_cs_init(void)
{
	int ret = 0;

	dbg.dev = create_empty_device();
	if (!dbg.dev) {
		pr_err("Exynos: create empty device fail\n");
		goto err;
	}
	dev_set_socdata(dbg.dev, "Exynos", "CS");

	ret = exynos_cs_init_dt();
	if (ret < 0) {
		dev_info(dbg.dev, "Failed get DT(%d).\n", ret);
		goto err;
	}

	get_arm_arch_version(0);

#ifdef CONFIG_LOCKUP_DETECTOR
	atomic_notifier_chain_register(&hardlockup_notifier_list,
			&exynos_cs_lockup_nb);
#endif
	atomic_notifier_chain_register(&panic_notifier_list,
			&exynos_cs_panic_nb);
	dev_info(dbg.dev, "Success Init.\n");

	/* register cpu pm notifier for C2 */
	cpu_pm_register_notifier(&exynos_cs_c2_pm_nb);

	/* register cpuhp state for hot plug(non-boot cores) */
	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "exynos-cs:online",
				exynos_cs_start_cpu, exynos_cs_stop_cpu);

	if (ret >= 0) {
		dbg.enabled = 1;
		dev_info(dbg.dev, "cpuhp setup success\n");
	}

err:
	return ret;
}
subsys_initcall(exynos_cs_init);
