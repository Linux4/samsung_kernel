/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/tick.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/stat.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/device.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include <mach/exynos-pm.h>
#include <mach/exynos-powermode-smp.h>
#include <mach/exynos-pm.h>
#include <mach/pmu.h>
#include <mach/pmu_cal_sys.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>

#include <sound/exynos.h>

static int boot_core_id __read_mostly;
static int boot_cluster_id __read_mostly;

#if defined(CONFIG_GPS_BCMxxxxx)
extern int check_gps_op(void);
#endif

#ifdef CONFIG_ARM_EXYNOS_SMP_CPUFREQ
void (*disable_c3_idle)(bool disable);

static bool disabled_cluster_power_down = false;

static void __maybe_unused exynos_disable_cluster_power_down(bool disable)
{
	disabled_cluster_power_down = disable;
}
#endif

static void store_boot_cpu_info(void)
{
	unsigned int mpidr = read_cpuid_mpidr();

	boot_core_id = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	boot_cluster_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_info("A booting CPU: core %d cluster %d\n", boot_core_id,
						       boot_cluster_id);
}

static bool is_in_boot_cluster(unsigned int cpu)
{
	return boot_cluster_id == MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
}

static void exynos_idle_clock_down(bool enable)
{
	unsigned int val;
	void __iomem *boot_cluster_reg, *nonboot_cluster_reg;

	boot_cluster_reg = EXYNOS7580_CPU_PWR_CTRL3;
	nonboot_cluster_reg = EXYNOS7580_APL_PWR_CTRL3;

	val = enable ? USE_L2QACTIVE : 0;

	__raw_writel(val, boot_cluster_reg);
	__raw_writel(0x0, nonboot_cluster_reg);
}

static DEFINE_SPINLOCK(cpd_lock);
static struct cpumask cpd_state_mask;

static void init_cpd_state_mask(void)
{
	cpumask_clear(&cpd_state_mask);
}

static void update_cpd_state(bool down, unsigned int cpu)
{
	if (down)
		cpumask_set_cpu(cpu, &cpd_state_mask);
	else
		cpumask_clear_cpu(cpu, &cpd_state_mask);
}

static unsigned int calc_expected_residency(unsigned int cpu)
{
	struct clock_event_device *dev = per_cpu(tick_cpu_device, cpu).evtdev;

	ktime_t now = ktime_get(), next = dev->next_event;

	return (unsigned int)ktime_to_us(ktime_sub(next, now));
}

static int is_busy(unsigned int target_residency, const struct cpumask *mask)
{
	int cpu;

	for_each_cpu_and(cpu, cpu_possible_mask, mask) {
		if (!cpumask_test_cpu(cpu, &cpd_state_mask))
			return -EBUSY;

		if (calc_expected_residency(cpu) < target_residency)
			return -EBUSY;
	}

	return 0;
}

/*
 * If AP put into LPC, console cannot work normally. For development,
 * support sysfs to enable or disable LPC. Refer below :
 *
 * echo 0/1 > /sys/power/lpc (0:disable, 1:enable)
 */
static int lpc_enabled = 1;

static ssize_t show_lpc_enabled(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 3, "%d\n", lpc_enabled);
}

static ssize_t store_lpc_enabled(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	lpc_enabled = !!input;

	return count;
}

static struct kobj_attribute lpc_attribute =
	__ATTR(lpc, S_IRUGO | S_IWUSR, show_lpc_enabled, store_lpc_enabled);

static int check_cp_status(void)
{
	unsigned int cp_stat;
	cp_stat = (__raw_readl(CP_STAT) & (0x11));
	if (cp_stat == CP_SLEEP)
		return 0;

	return -EBUSY;
}

static unsigned int lpc_reg_num __read_mostly;
static struct hw_info lpc_regs[MAX_NUM_REGS] __read_mostly;

static void set_lpc_flag(void)
{
	__raw_writel(0x1, LPC_FLAG_ADDR);
}

#if defined(CONFIG_MUIC_NOTIFIER)
static bool jig_is_attached;

static inline bool is_jig_attached(void)
{
	return jig_is_attached;
}
#else
static inline bool is_jig_attached(void)
{
	return false;
}
#endif

bool is_lpc_available(unsigned int target_residency)
{
	if (!lpc_enabled)
		return false;

	if (is_jig_attached())
		return false;

	if (is_busy(target_residency, cpu_online_mask))
		return false;

	if (check_hw_status(lpc_regs, lpc_reg_num))
		return false;

	if (exynos_lpc_prepare())
		return false;

	if (exynos_check_aud_pwr() > AUD_PWR_LPC)
		return false;

	if (pwm_check_enable_cnt())
		return false;

#if defined(CONFIG_GPS_BCMxxxxx)
	if (check_gps_op())
		return false;
#endif

	if (check_cp_status())
		return false;

	return true;
}

int determine_cpd(int index, int c2_index, unsigned int cpu,
		  unsigned int target_residency)
{
	exynos_cpu.power_down(cpu);

	if (index == c2_index)
		return c2_index;

#ifdef CONFIG_ARM_EXYNOS_SMP_CPUFREQ
	if (disabled_cluster_power_down)
		return c2_index;
#endif

	spin_lock(&cpd_lock);

	update_cpd_state(true, cpu);

	if (is_lpc_available(target_residency)) {
		set_lpc_flag();
		s3c24xx_serial_fifo_wait();
	}

	if (is_in_boot_cluster(cpu)) {
		index = c2_index;
		goto unlock;
	}

	if (is_busy(target_residency, cpu_coregroup_mask(cpu)))
		index = c2_index;
	else
		exynos_cpu_sequencer_ctrl(true);

unlock:
	spin_unlock(&cpd_lock);

	return index;
}

void wakeup_from_c2(unsigned int cpu)
{
	exynos_cpu.power_up(cpu);

	spin_lock(&cpd_lock);

	update_cpd_state(false, cpu);

	if (is_in_boot_cluster(cpu))
		goto unlock;

	exynos_cpu_sequencer_ctrl(false);

unlock:
	spin_unlock(&cpd_lock);
}

static unsigned int lpm_reg_num __read_mostly;
static struct hw_info lpm_regs[MAX_NUM_REGS] __read_mostly;

int determine_lpm(void)
{
	if (exynos_check_aud_pwr() != AUD_PWR_LPA)
		return SYS_AFTR;

	if (check_hw_status(lpm_regs, lpm_reg_num)) {
		lpa_blocking_counter(1);
		return SYS_AFTR;
	}

	if (exynos_lpa_prepare()) {
		lpa_blocking_counter(0);
		return SYS_AFTR;
	}

#if defined(CONFIG_GPS_BCMxxxxx)
	if (check_gps_op())
		return SYS_AFTR;
#endif

	if (check_cp_status()) {
		lpa_blocking_counter(2);
		return SYS_AFTR;
	}
	return SYS_LPA;
}

static void parse_dt_reg_list(struct device_node *np, const char *reg,
			      const char *val, struct hw_info *regs,
			      unsigned int *reg_num)
{
	unsigned int i, reg_len, val_len;
	const __be32 *reg_list, *val_list;

	reg_list = of_get_property(np, reg, &reg_len);
	val_list = of_get_property(np, val, &val_len);

	if (!reg_list) {
		pr_warn("%s property does not exist\n", reg);
		return;
	}

	if (!val_list) {
		pr_warn("%s property does not exist\n", val);
		return;
	}

	BUG_ON(reg_len != val_len);

	*reg_num = reg_len / sizeof(unsigned int);

	for (i = 0; i < *reg_num; i++) {
		regs[i].addr = ioremap(be32_to_cpup(reg_list++), SZ_32);
		BUG_ON(!regs[i].addr);
		regs[i].mask = be32_to_cpup(val_list++);
	}
}

static void exynos_lpm_dt_init(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "low-power-mode");

	parse_dt_reg_list(np, "lpc-reg", "lpc-val", lpc_regs, &lpc_reg_num);
	parse_dt_reg_list(np, "lpm-reg", "lpm-val", lpm_regs, &lpm_reg_num);
}

static struct sfr_save save_clk_regs[] = {
	SFR_SAVE(EXYNOS7580_DIV_BUS0),
	SFR_SAVE(EXYNOS7580_DIV_BUS1),
	SFR_SAVE(EXYNOS7580_DIV_BUS2),

	SFR_SAVE(EXYNOS7580_DIV_MIF0),
	SFR_SAVE(EXYNOS7580_DIV_MIF1),
	SFR_SAVE(EXYNOS7580_DIV_TOP0),
	SFR_SAVE(EXYNOS7580_DIV_TOP1),

	SFR_SAVE(EXYNOS7580_DIV_TOP_FSYS0),
	SFR_SAVE(EXYNOS7580_DIV_TOP_FSYS1),
	SFR_SAVE(EXYNOS7580_DIV_TOP_FSYS2),

	SFR_SAVE(EXYNOS7580_DIV_TOP_PERI0),
	SFR_SAVE(EXYNOS7580_DIV_TOP_PERI1),
	SFR_SAVE(EXYNOS7580_DIV_TOP_PERI2),
	SFR_SAVE(EXYNOS7580_DIV_TOP_PERI3),

	SFR_SAVE(EXYNOS7580_DIV_TOP_ISP0),
	SFR_SAVE(EXYNOS7580_DIV_TOP_ISP1),

	SFR_SAVE(EXYNOS7580_MUX_SEL_MIF0),
	SFR_SAVE(EXYNOS7580_MUX_SEL_MIF1),
	SFR_SAVE(EXYNOS7580_MUX_SEL_MIF2),
	SFR_SAVE(EXYNOS7580_MUX_SEL_MIF3),
	SFR_SAVE(EXYNOS7580_MUX_SEL_MIF4),
	SFR_SAVE(EXYNOS7580_MUX_SEL_MIF5),

	SFR_SAVE(EXYNOS7580_MUX_SEL_TOP0),
	SFR_SAVE(EXYNOS7580_MUX_SEL_TOP1),
	SFR_SAVE(EXYNOS7580_MUX_SEL_TOP2),
};

static void exynos_sys_powerdown_set_clk(enum sys_powerdown mode)
{
	switch (mode) {
	case SYS_AFTR:
		break;
	case SYS_LPA:
		exynos_save_sfr(save_clk_regs, ARRAY_SIZE(save_clk_regs));
		break;
	case SYS_SLEEP:
		break;
	default:
		break;
	}
}

static void exynos_sys_powerdown_restore_clk(enum sys_powerdown mode)
{
	switch (mode) {
	case SYS_AFTR:
		break;
	case SYS_LPA:
		exynos_restore_sfr(save_clk_regs, ARRAY_SIZE(save_clk_regs));
		break;
	case SYS_SLEEP:
		break;
	default:
		break;
	}
}

void exynos7580_set_wakeupmask(enum sys_powerdown mode)
{
	u64 eintmask = exynos_get_eint_wake_mask();
	u32 intmask = 0, intmask2, intmask3;

	/* Set external interrupt mask */
	__raw_writel((u32)eintmask, EXYNOS_PMU_EINT_WAKEUP_MASK);

	intmask2 = 0xFFFF0000;
	intmask3 = 0xFFFF0000;

	switch (mode) {
	case SYS_AFTR:
		intmask2 = 0xFFFE0000;
		/* fall thru */
	case SYS_LPA:
	case SYS_ALPA:
		intmask = 0x40000000;
		break;
	case SYS_SLEEP:
		/* BIT(31): deactivate wakeup event monitoring circuit */
		intmask = 0x7CEFFFFF;
		break;
	default:
		break;
	}
	__raw_writel(intmask, EXYNOS_PMU_WAKEUP_MASK);
	__raw_writel(intmask2, EXYNOS_PMU_WAKEUP_MASK2);
	__raw_writel(intmask3, EXYNOS_PMU_WAKEUP_MASK3);
}

void exynos_prepare_sys_powerdown(enum sys_powerdown mode)
{
	exynos7580_set_wakeupmask(mode);

	exynos_pmu_cal_sys_prepare(mode);

	exynos_idle_clock_down(false);

	switch (mode) {
	case SYS_LPA:
		exynos_lpa_enter();
		break;
	default:
		break;
	}

	exynos_sys_powerdown_set_clk(mode);
}

void exynos_wakeup_sys_powerdown(enum sys_powerdown mode, bool early_wakeup)
{
	if (early_wakeup)
		exynos_pmu_cal_sys_earlywake(mode);
	else
		exynos_pmu_cal_sys_post(mode);

	exynos_idle_clock_down(true);

	exynos_sys_powerdown_restore_clk(mode);

	switch (mode) {
	case SYS_LPA:
		exynos_lpa_exit();
		break;
	default:
		break;
	}
}

bool exynos_sys_powerdown_enabled(void)
{
	/*
	 * When system enters low power mode,
	 * this bit changes automatically to high
	 */
	return !((__raw_readl(EXYNOS_PMU_CENTRAL_SEQ_CONFIGURATION) &
			CENTRALSEQ_PWR_CFG));
}

int __init exynos_powermode_init(void)
{
	store_boot_cpu_info();

	if (IS_ENABLED(CONFIG_CPU_IDLE_EXYNOS))
		exynos_idle_clock_down(true);

	init_cpd_state_mask();

	exynos_lpm_dt_init();

	exynos_pmu_cal_sys_init();

	if (sysfs_create_file(power_kobj, &lpc_attribute.attr))
		pr_err("%s: failed to create sysfs to control LPC\n", __func__);

#ifdef CONFIG_ARM_EXYNOS_SMP_CPUFREQ
	disable_c3_idle = exynos_disable_cluster_power_down;
#endif
	return 0;
}
arch_initcall(exynos_powermode_init);

#if defined(CONFIG_MUIC_NOTIFIER)
struct notifier_block cpuidle_muic_nb;

static int exynos_cpuidle_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:	// 523k
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
#if defined(CONFIG_MUIC_S2MU005) && defined(CONFIG_SEC_FACTORY)
	case ATTACHED_DEV_JIG_UART_ON_MUIC:	// 619k
#endif
		if (action == MUIC_NOTIFY_CMD_DETACH)
			jig_is_attached = false;
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			jig_is_attached = true;
		else
			pr_err("%s: ACTION Error!\n", __func__);
		break;
	default:
		break;
	}

	pr_info("%s: dev=%d, action=%lu\n", __func__, attached_dev, action);

	return NOTIFY_DONE;
}

static int __init exynos_powermode_muic_notifier_init(void)
{
	return muic_notifier_register(&cpuidle_muic_nb,
			exynos_cpuidle_muic_notifier, MUIC_NOTIFY_DEV_CPUIDLE);
}
late_initcall(exynos_powermode_muic_notifier_init);
#endif
