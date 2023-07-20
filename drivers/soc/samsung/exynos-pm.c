/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/wakeup_reason.h>
#include <linux/gpio.h>
#include <linux/syscore_ops.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/psci.h>
#include <linux/debugfs.h>
#if defined(CONFIG_SEC_FACTORY)
#include <linux/sec_class.h>
#endif
#include <asm/cpuidle.h>
#include <asm/smp_plat.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>
#include <soc/samsung/exynos-flexpmu-dbg.h>
#include <linux/soc/samsung/exynos-soc.h>

#define WAKEUP_STAT_EINT                (1 << 15)
#define WAKEUP_STAT_RTC_ALARM           (1 << 0)
/*
 * PMU register offset
 */
#define EXYNOS_PMU_EINT_WAKEUP_MASK	0x060C
#define EXYNOS_PMU_EINT_WAKEUP_MASK2	0x061C

extern u32 exynos_eint_to_pin_num(int eint);
#define EXYNOS_EINT_PEND(b, x)      ((b) + 0xA00 + (((x) >> 3) * 4))

#ifdef CONFIG_PM_WAKELOCKS
extern int pm_wake_lock(const char *buf);
#else
inline int pm_wake_lock(const char *buf)
{
	return 0;
}
#endif

#define WAKEUP_STAT_SYSINT_MASK		~ (1 << 12)

struct wakeup_stat_name {
	const char *name[32];
};

struct exynos_pm_info {
	void __iomem *eint_base;		/* GPIO_ALIVE base to check wkup reason */
	void __iomem *gic_base;			/* GICD_ISPENDRn base to check wkup reason */
	unsigned int num_eint;			/* Total number of EINT sources */
	unsigned int num_gic;			/* Total number of GIC sources */
	bool is_early_wakeup;
	bool is_usbl2_suspend;
	unsigned int suspend_mode_idx;		/* power mode to be used in suspend scenario */
	unsigned int suspend_psci_idx;		/* psci index to be used in suspend scenario */
	unsigned int *wakeup_stat;		/* wakeup stat SFRs offset */
	unsigned int apdn_cnt_prev;		/* sleep apsoc down sequence prev count */
	unsigned int apdn_cnt;			/* sleep apsoc down sequence count */

	unsigned int usbl2_suspend_available;
	unsigned int usbl2_suspend_mode_idx;	/* power mode to be used in suspend scenario */
	u8 num_wakeup_stat;			/* Total number of wakeup_stat */
	u32 (*usb_is_connect)(void);
	struct wakeup_stat_name *ws_names;	/* Names of each bits of wakeup_stat */

	u8 num_dbg_subsystem;
	const char **dbg_subsystem_name;
	u32 *dbg_subsystem_offset;
	void __iomem *i3c_base;
	u32 vgpio_tx_monitor;
};
static struct exynos_pm_info *pm_info;

struct exynos_pm_dbg {
	u32 test_early_wakeup;
	u32 test_usbl2_suspend;

	unsigned int mifdn_early_wakeup_prev;
	unsigned int mifdn_early_wakeup_cnt;
	unsigned int mifdn_cnt_prev;
	unsigned int mifdn_cnt;
	unsigned int mif_req;
};
static struct exynos_pm_dbg *pm_dbg;

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int i, size;
	long unsigned int ext_int_pend;
	u64 eint_wakeup_mask;
	bool found = 0;
	unsigned int val0 = 0, val1 = 0;

	exynos_pmu_read(EXYNOS_PMU_EINT_WAKEUP_MASK, &val0);
	exynos_pmu_read(EXYNOS_PMU_EINT_WAKEUP_MASK2, &val1);
	eint_wakeup_mask = val1;
	eint_wakeup_mask = ((eint_wakeup_mask << 32) | val0);

	for (i = 0, size = 8; i < pm_info->num_eint; i += size) {
		ext_int_pend =
			__raw_readl(EXYNOS_EINT_PEND(pm_info->eint_base, i));

		for_each_set_bit(bit, &ext_int_pend, size) {
			u32 gpio;
			int irq;

			if (eint_wakeup_mask & (1 << (i + bit)))
				continue;

			gpio = exynos_eint_to_pin_num(i + bit);
			irq = gpio_to_irq(gpio);

#ifdef CONFIG_SUSPEND
			log_wakeup_reason(irq);
		//	update_wakeup_reason_stats(irq, i + bit);
#endif
			found = 1;
		}
	}

	if (!found)
		pr_info("%s Resume caused by unknown EINT\n", EXYNOS_PM_PREFIX);
}

static void exynos_show_wakeup_registers(unsigned int wakeup_stat)
{
	int i, size;

	pr_info("WAKEUP_STAT:\n");
	for (i = 0; i < pm_info->num_wakeup_stat; i++) {
		exynos_pmu_read(pm_info->wakeup_stat[i], &wakeup_stat);
		pr_info("0x%08x\n", wakeup_stat);
	}

	pr_info("EINT_PEND: ");
	for (i = 0, size = 8; i < pm_info->num_eint; i += size)
		pr_info("0x%02x ", __raw_readl(EXYNOS_EINT_PEND(pm_info->eint_base, i)));
}

static void exynos_show_wakeup_reason_sysint(unsigned int stat,
					struct wakeup_stat_name *ws_names)
{
	int bit;
	unsigned long int lstat = stat;
	const char *name;

	for_each_set_bit(bit, &lstat, 32) {
		name = ws_names->name[bit];

		if (!name)
			continue;
#ifdef CONFIG_SUSPEND
		log_wakeup_reason_name(name);
#endif
	}
}

static void exynos_show_wakeup_reason_detail(unsigned int wakeup_stat)
{
	int i;
	unsigned int wss;

	if (wakeup_stat & WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();

	if (unlikely(!pm_info->ws_names))
		return;

	for (i = 0; i < pm_info->num_wakeup_stat; i++) {
		if (i == 0)
			wss = wakeup_stat & WAKEUP_STAT_SYSINT_MASK;
		else
			exynos_pmu_read(pm_info->wakeup_stat[i], &wss);

		if (!wss)
			continue;

		exynos_show_wakeup_reason_sysint(wss, &pm_info->ws_names[i]);
	}
}

static void exynos_show_wakeup_reason(bool sleep_abort)
{
	unsigned int wakeup_stat;
	int i, size;

	if (sleep_abort) {
		pr_info("%s early wakeup! Dumping pending registers...\n", EXYNOS_PM_PREFIX);

		pr_info("EINT_PEND:\n");
		for (i = 0, size = 8; i < pm_info->num_eint; i += size)
			pr_info("0x%x\n", __raw_readl(EXYNOS_EINT_PEND(pm_info->eint_base, i)));

		pr_info("GIC_PEND:\n");
		for (i = 0; i < pm_info->num_gic; i++)
			pr_info("GICD_ISPENDR[%d] = 0x%x\n", i, __raw_readl(pm_info->gic_base + i*4));

		pr_info("%s done.\n", EXYNOS_PM_PREFIX);
		return ;
	}

	if (!pm_info->num_wakeup_stat)
		return;

	exynos_pmu_read(pm_info->wakeup_stat[0], &wakeup_stat);
	exynos_show_wakeup_registers(wakeup_stat);

	exynos_show_wakeup_reason_detail(wakeup_stat);

	if (wakeup_stat & WAKEUP_STAT_RTC_ALARM)
		pr_info("%s Resume caused by RTC alarm\n", EXYNOS_PM_PREFIX);
	else {
		for (i = 0; i < pm_info->num_wakeup_stat; i++) {
			exynos_pmu_read(pm_info->wakeup_stat[i], &wakeup_stat);
			pr_info("%s Resume caused by wakeup%d_stat 0x%08x\n",
					EXYNOS_PM_PREFIX, i + 1, wakeup_stat);
		}
	}
}

#ifdef CONFIG_CPU_IDLE
static DEFINE_RWLOCK(exynos_pm_notifier_lock);
static RAW_NOTIFIER_HEAD(exynos_pm_notifier_chain);

int exynos_pm_register_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_register(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_register_notifier);

int exynos_pm_unregister_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_unregister(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_unregister_notifier);

static int __exynos_pm_notify(enum exynos_pm_event event, int nr_to_call, int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&exynos_pm_notifier_chain, event, NULL,
		nr_to_call, nr_calls);

	return notifier_to_errno(ret);
}

int exynos_pm_notify(enum exynos_pm_event event)
{
	int nr_calls;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = __exynos_pm_notify(event, -1, &nr_calls);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_notify);
#endif /* CONFIG_CPU_IDLE */

#ifdef CONFIG_SEC_GPIO_DVS
extern void gpio_dvs_check_sleepgpio(void);
#endif

static void print_dbg_subsystem(void)
{
	int i = 0;
	u32 reg;

	if (!pm_info->num_dbg_subsystem)
		return;

	pr_info("%s Debug Subsystem:\n", EXYNOS_PM_PREFIX);

	for (i = 0; i < pm_info->num_dbg_subsystem; i++) {
		exynos_pmu_read(pm_info->dbg_subsystem_offset[i], &reg);

		pr_info("%s:\t0x%08x\n", pm_info->dbg_subsystem_name[i], reg);
	}

	if (pm_info->i3c_base) {
		reg = __raw_readl(pm_info->i3c_base + pm_info->vgpio_tx_monitor);
		pr_info("VGPIO_TX_MONITOR: 0x%08x\n", reg);
	}
}

static int exynos_pm_syscore_suspend(void)
{
	if (!exynos_check_cp_status()) {
		pr_info("%s %s: sleep canceled by CP reset \n",
					EXYNOS_PM_PREFIX, __func__);
		return -EINVAL;
	}

	pm_info->is_usbl2_suspend = false;
	if (pm_info->usbl2_suspend_available) {
		if (!IS_ERR_OR_NULL(pm_info->usb_is_connect))
			pm_info->is_usbl2_suspend = !!pm_info->usb_is_connect();
	}

	if (pm_info->is_usbl2_suspend || pm_dbg->test_usbl2_suspend) {
		exynos_prepare_sys_powerdown(pm_info->usbl2_suspend_mode_idx);
		pr_info("%s %s: Enter Suspend scenario. usbl2_mode_idx = %d)\n",
				EXYNOS_PM_PREFIX,__func__, pm_info->usbl2_suspend_mode_idx);
	} else {
		exynos_prepare_sys_powerdown(pm_info->suspend_mode_idx);
		pr_info("%s %s: Enter Suspend scenario. suspend_mode_idx = %d)\n",
				EXYNOS_PM_PREFIX,__func__, pm_info->suspend_mode_idx);
	}
	
#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************/
	/* This function must be located in appropriate SLEEP position
	 * in accordance with the specification of each BB vendor.
	 */
	/************************ Caution !!! ****************************/
	gpio_dvs_check_sleepgpio();
#endif /* CONFIG_SEC_GPIO_DVS */

#define CODEC_IRQ_INT_EN	(1 << 1)
#define PMIC_IRQ_INT_EN		(1 << 2)
	if (exynos_soc_info.revision > 0) {
		pr_info("%s %s: Set Usrdef_int_en for pmic wakeup\n",
				EXYNOS_PM_PREFIX, __func__);
		exynos_pmu_update(0x3A94, CODEC_IRQ_INT_EN | PMIC_IRQ_INT_EN,
						CODEC_IRQ_INT_EN | PMIC_IRQ_INT_EN);
	}

	/* Send an IPI if test_early_wakeup flag is set */
	if (pm_dbg->test_early_wakeup)
		arch_send_call_function_single_ipi(0);

	pm_dbg->mifdn_cnt_prev = acpm_get_mifdn_count();
	pm_info->apdn_cnt_prev = acpm_get_apsocdn_count();
	pm_dbg->mif_req = acpm_get_mif_request();

	pm_dbg->mifdn_early_wakeup_prev = acpm_get_early_wakeup_count();

	print_dbg_subsystem();

	pr_info("%s: prev mif_count:%d, apsoc_count:%d, seq_early_wakeup_count:%d\n",
			EXYNOS_PM_PREFIX, pm_dbg->mifdn_cnt_prev,
			pm_info->apdn_cnt_prev, pm_dbg->mifdn_early_wakeup_prev);

	exynos_flexpmu_dbg_suspend_mif_req();

	return 0;
}

static void exynos_pm_syscore_resume(void)
{
	pm_dbg->mifdn_cnt = acpm_get_mifdn_count();
	pm_info->apdn_cnt = acpm_get_apsocdn_count();
	pm_dbg->mifdn_early_wakeup_cnt = acpm_get_early_wakeup_count();

	pr_info("%s: post mif_count:%d, apsoc_count:%d, seq_early_wakeup_count:%d\n",
			EXYNOS_PM_PREFIX, pm_dbg->mifdn_cnt,
			pm_info->apdn_cnt, pm_dbg->mifdn_early_wakeup_cnt);

	if (pm_info->apdn_cnt == pm_info->apdn_cnt_prev)
		pm_info->is_early_wakeup = true;
	else
		pm_info->is_early_wakeup = false;

	if (pm_info->is_early_wakeup)
		pr_info("%s %s: return to originator\n",
				EXYNOS_PM_PREFIX, __func__);

	if (pm_dbg->mifdn_early_wakeup_cnt != pm_dbg->mifdn_early_wakeup_prev)
		pr_debug("%s: Sequence early wakeup\n", EXYNOS_PM_PREFIX);

	if (pm_dbg->mifdn_cnt == pm_dbg->mifdn_cnt_prev)
		pr_info("%s: MIF blocked. MIF request Mster was  0x%x\n",
				EXYNOS_PM_PREFIX, pm_dbg->mif_req);
	else
		pr_info("%s: MIF down. cur_count: %d, acc_count: %d\n",
				EXYNOS_PM_PREFIX, pm_dbg->mifdn_cnt - pm_dbg->mifdn_cnt_prev, pm_dbg->mifdn_cnt);

	exynos_flexpmu_dbg_resume_mif_req();

	print_dbg_subsystem();

	if (pm_info->is_usbl2_suspend || pm_dbg->test_usbl2_suspend)
		exynos_wakeup_sys_powerdown(pm_info->usbl2_suspend_mode_idx, pm_info->is_early_wakeup);
	else
		exynos_wakeup_sys_powerdown(pm_info->suspend_mode_idx, pm_info->is_early_wakeup);

	exynos_show_wakeup_reason(pm_info->is_early_wakeup);

	if (!pm_info->is_early_wakeup)
		pr_debug("%s %s: post sleep, preparing to return\n",
						EXYNOS_PM_PREFIX, __func__);
}

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_syscore_suspend,
	.resume		= exynos_pm_syscore_resume,
};

int register_usb_is_connect(u32 (*func)(void))
{
	if(func) {
		pm_info->usb_is_connect = func;
		pr_info("Registered usb_is_connect func\n");
		return 0;
	} else {
		pr_err("%s	:function pointer is NULL \n", __func__);
		return -ENXIO;
	}
}
EXPORT_SYMBOL_GPL(register_usb_is_connect);

bool is_test_usbl2_suspend_set(void)
{
	if (!pm_dbg)
		return false;

	return pm_dbg->test_usbl2_suspend;
}
EXPORT_SYMBOL_GPL(is_test_usbl2_suspend_set);

#ifdef CONFIG_DEBUG_FS
static void __init exynos_pm_debugfs_init(void)
{
	struct dentry *root, *d;

	root = debugfs_create_dir("exynos-pm", NULL);
	if (!root) {
		pr_err("%s %s: could't create debugfs dir\n", EXYNOS_PM_PREFIX, __func__);
		return;
	}

	d = debugfs_create_u32("test_early_wakeup", 0644, root, &pm_dbg->test_early_wakeup);
	if (!d) {
		pr_err("%s %s: could't create debugfs test_early_wakeup\n",
					EXYNOS_PM_PREFIX, __func__);
		return;
	}

	d = debugfs_create_u32("test_usbl2_suspend", 0644, root, &pm_dbg->test_usbl2_suspend);
	if (!d) {
		pr_err("%s %s: could't create debugfs test_usbl2_suspend\n",
					EXYNOS_PM_PREFIX, __func__);
		return;
	}
}
#endif

static void parse_dt_wakeup_stat_names(struct device_node *np)
{
	struct device_node *root, *child;
	int ret;
	int size, n, idx = 0;

	root = of_find_node_by_name(np, "wakeup_stats");
	n = of_get_child_count(root);

	if (pm_info->num_wakeup_stat != n || !n) {
		pr_err("%s: failed to get wakeup_stats(%d)\n", __func__, n);
		return;
	}

	pm_info->ws_names = kzalloc(sizeof(*pm_info->ws_names)* n, GFP_KERNEL);
	if (!pm_info->ws_names)
		return;

	for_each_child_of_node(root, child) {
		size = of_property_count_strings(child, "ws-name");
		if (size <= 0 || size > 32) {
			pr_err("%s: failed to get wakeup_stat name cnt(%d)\n",
					__func__, size);
			return;
		}

		ret = of_property_read_string_array(child, "ws-name",
				pm_info->ws_names[idx].name, size);
		if (ret < 0) {
			pr_err("%s: failed to read wakeup_stat name(%d)\n",
					__func__, ret);
			return;
		}

		idx++;
	}
}

static void parse_dt_debug_subsystem(struct device_node *np)
{
	struct device_node *child;
	int ret;
	int size_name, size_offset = 0;

	child = of_find_node_by_name(np, "debug_subsystem");

	if (!child) {
		pr_err("%s %s: unable to get debug_subsystem value from DT\n",
				EXYNOS_PM_PREFIX, __func__);
		return;
	}

	size_name = of_property_count_strings(child, "sfr-name");
	size_offset = of_property_count_u32_elems(child, "sfr-offset");

	if (size_name != size_offset) {
		pr_err("%s %s: size mismatch(%d, %d)\n",
				EXYNOS_PM_PREFIX, __func__, size_name, size_offset);
		return;
	}

	pm_info->dbg_subsystem_name = kzalloc(sizeof(const char *) * size_name, GFP_KERNEL);
	if (!pm_info->dbg_subsystem_name)
		return;

	pm_info->dbg_subsystem_offset = kzalloc(sizeof(unsigned int) * size_offset, GFP_KERNEL);
	if (!pm_info->dbg_subsystem_offset)
		goto free_name;

	ret = of_property_read_string_array(child, "sfr-name",
			pm_info->dbg_subsystem_name, size_name);
	if (ret < 0) {
		pr_err("%s %s: failed to get debug_subsystem name from DT\n",
				EXYNOS_PM_PREFIX, __func__);
		goto free_offset;
	}

	ret = of_property_read_u32_array(child, "sfr-offset",
			pm_info->dbg_subsystem_offset, size_offset);
	if (ret < 0) {
		pr_err("%s %s: failed to get debug_subsystem offset from DT\n",
				EXYNOS_PM_PREFIX, __func__);
		goto free_offset;
	}

	pm_info->num_dbg_subsystem = size_name;
	return;

free_offset:
	kfree(pm_info->dbg_subsystem_offset);
free_name:
	kfree(pm_info->dbg_subsystem_name);
	pm_info->num_dbg_subsystem = 0;
	return;
}

#if defined(CONFIG_SEC_FACTORY)
static ssize_t asv_info_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int count = 0;

	/* Set asv group info to buf */
	count += sprintf(&buf[count], "%d ", asv_ids_information(tg));
	count += sprintf(&buf[count], "%03x ", asv_ids_information(bg));
	count += sprintf(&buf[count], "%03x ", asv_ids_information(g3dg));
	count += sprintf(&buf[count], "%u ", asv_ids_information(bids));
	count += sprintf(&buf[count], "%u ", asv_ids_information(gids));
	count += sprintf(&buf[count], "\n");

	return count;
}

static DEVICE_ATTR_RO(asv_info);
#endif /* CONFIG_SEC_FACTORY */

static __init int exynos_pm_drvinit(void)
{
	int ret;
#if defined(CONFIG_SEC_FACTORY)
	struct device *dev;
#endif

	pm_info = kzalloc(sizeof(struct exynos_pm_info), GFP_KERNEL);
	if (pm_info == NULL) {
		pr_err("%s %s: failed to allocate memory for exynos_pm_info\n",
					EXYNOS_PM_PREFIX, __func__);
		BUG();
	}

	pm_dbg = kzalloc(sizeof(struct exynos_pm_dbg), GFP_KERNEL);
	if (pm_dbg == NULL) {
		pr_err("%s %s: failed to allocate memory for exynos_pm_dbg\n",
					EXYNOS_PM_PREFIX, __func__);
		BUG();
	}

	if (of_have_populated_dt()) {
		struct device_node *np;
		unsigned int wake_lock = 0;
		np = of_find_compatible_node(NULL, NULL, "samsung,exynos-pm");
		if (!np) {
			pr_err("%s %s: unabled to find compatible node (%s)\n",
					EXYNOS_PM_PREFIX, __func__, "samsung,exynos-pm");
			BUG();
		}

		pm_info->eint_base = of_iomap(np, 0);
		if (!pm_info->eint_base) {
			pr_err("%s %s: unabled to ioremap EINT base address\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		pm_info->gic_base = of_iomap(np, 1);
		if (!pm_info->gic_base) {
			pr_err("%s %s: unbaled to ioremap GIC base address\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "num-eint", &pm_info->num_eint);
		if (ret) {
			pr_err("%s %s: unabled to get the number of eint from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "num-gic", &pm_info->num_gic);
		if (ret) {
			pr_err("%s %s: unabled to get the number of gic from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "suspend_mode_idx", &pm_info->suspend_mode_idx);
		if (ret) {
			pr_err("%s %s: unabled to get suspend_mode_idx from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "suspend_psci_idx", &pm_info->suspend_psci_idx);
		if (ret) {
			pr_err("%s %s: unabled to get suspend_psci_idx from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "usbl2_suspend_available", &pm_info->usbl2_suspend_available);
		if (ret) {
			pr_info("%s %s: Not support usbl2_suspend mode\n",
					EXYNOS_PM_PREFIX, __func__);
		} else {
			ret = of_property_read_u32(np, "usbl2_suspend_mode_idx", &pm_info->usbl2_suspend_mode_idx);
			if (ret) {
				pr_err("%s %s: unabled to get usbl2_suspend_mode_idx from DT\n",
						EXYNOS_PM_PREFIX, __func__);
				BUG();
			}
		}

		ret = of_property_count_u32_elems(np, "wakeup_stat");
		if (!ret) {
			pr_err("%s %s: unabled to get wakeup_stat value from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		} else if (ret > 0) {
			pm_info->num_wakeup_stat = ret;
			pm_info->wakeup_stat = kzalloc(sizeof(unsigned int) * ret, GFP_KERNEL);
			of_property_read_u32_array(np, "wakeup_stat", pm_info->wakeup_stat, ret);
		}

		ret = of_property_read_u32(np, "wake_lock", &wake_lock);
		if (ret)
			pr_info("%s %s: unabled to get wake_lock from DT\n",
					EXYNOS_PM_PREFIX, __func__);

		if (wake_lock)
			pm_wake_lock("exynos-pm_wake_lock");
		parse_dt_wakeup_stat_names(np);

		parse_dt_debug_subsystem(np);

		pm_info->i3c_base = of_iomap(np, 2);
		if (!pm_info->i3c_base) {
			pr_err("%s %s: unbaled to ioremap I3C base address\n",
					EXYNOS_PM_PREFIX, __func__);
		}
		ret = of_property_read_u32(np, "vgpio_tx_monitor", &pm_info->vgpio_tx_monitor);
		if (ret) {
			pr_err("%s %s: unabled to get vgpio_tx_monitor from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			goto err_vgpio_tx_monitor;
		}

	} else {
		pr_err("%s %s: failed to have populated device tree\n",
					EXYNOS_PM_PREFIX, __func__);
		BUG();
	}

	register_syscore_ops(&exynos_pm_syscore_ops);
#ifdef CONFIG_DEBUG_FS
	exynos_pm_debugfs_init();
#endif

#if defined(CONFIG_SEC_FACTORY)
	dev = sec_device_create(NULL, "asv");
	BUG_ON(!dev);
	if (IS_ERR(dev))
		pr_err("%s %s: failed to create sec device\n",
				EXYNOS_PM_PREFIX, __func__);

	if (device_create_file(dev, &dev_attr_asv_info) < 0)
		pr_err("%s %s: failed to create sysfs file\n",
				EXYNOS_PM_PREFIX, __func__);
#endif

	return 0;

err_vgpio_tx_monitor:
	iounmap(pm_info->i3c_base);
	pm_info->i3c_base = NULL;
	return 0;
}
arch_initcall(exynos_pm_drvinit);
