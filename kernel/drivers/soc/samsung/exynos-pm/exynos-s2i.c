/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>

#include <trace/hooks/cpuidle_psci.h>
#include <soc/samsung/exynos-s2i.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos-pm.h>
#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
#include <soc/samsung/esca.h>
#endif

struct exynos_s2i_ops *exynos_s2i_ops;
static DEFINE_MUTEX(exynos_s2i_ops_lock);

struct exynos_s2i{
	struct device *dev;
	u32 suspend_cnt;

	void __iomem *notify_base;
	u32 notify_offset;
	u32 sr_offset;
	u32 intgr_offset;
};
static struct exynos_s2i *s2i;

/**
 * register_exynos_s2i_ops - Register a set of system core operations.
 * @ops: System core operations to register.
 */
void register_exynos_s2i_ops(struct exynos_s2i_ops *ops)
{
	pr_info("[%s] %pS\n", __func__, ops->suspend);
	mutex_lock(&exynos_s2i_ops_lock);
	list_add_tail(&ops->node, &exynos_s2i_ops->node);
	mutex_unlock(&exynos_s2i_ops_lock);
}
EXPORT_SYMBOL_GPL(register_exynos_s2i_ops);

/**
 * unregister_exynos_s2i_ops - Unregister a set of system core operations.
 * @ops: System core operations to unregister.
 */
void unregister_exynos_s2i_ops(struct exynos_s2i_ops *ops)
{
	pr_info("[%s] %pS\n", __func__, ops->suspend);

	mutex_lock(&exynos_s2i_ops_lock);
	list_del(&ops->node);
	mutex_unlock(&exynos_s2i_ops_lock);
}
EXPORT_SYMBOL_GPL(unregister_exynos_s2i_ops);

static int exynos_s2idle_core_suspend(void)
{
	struct exynos_s2i_ops *ops;

	pr_info("%s start\n", __func__);

	list_for_each_entry_reverse(ops, &exynos_s2i_ops->node, node) {
		if (ops->suspend)
			ops->suspend();
	}
	return 0;
}

static void exynos_s2idle_core_resume(void)
{
	struct exynos_s2i_ops *ops;

	pr_info("%s start\n", __func__);

	list_for_each_entry(ops, &exynos_s2i_ops->node, node) {
		if (ops->resume)
			ops->resume();
	}
}

static irqreturn_t exynos_s2i_wakeup_interrupt(int irq, void *dev)
{
	pr_info("%s  NOTIFY : 0x%x \n", __func__,
			readl(s2i->notify_base + s2i->notify_offset));
	writel(0x0, s2i->notify_base + s2i->notify_offset);

	return IRQ_HANDLED;
}

static void exynos_s2i_ipi_fn(void *data)
{
	pr_info("Wakeup cpu(%d)\n", smp_processor_id());
	return;
}

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern u32 esca_get_apsleep_count(void);
#else
extern u32 acpm_get_apsleep_count(void);
#endif
static void android_vh_cpuidle_psci_enter(void *data,
		struct cpuidle_device *dev, bool s2idle)
{
	int cpu = smp_processor_id();

	if (!s2idle)
		return;

	/* Set C2 */
	if (cpu) {
		cal_cpu_disable(cpu);
#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
		s2i->suspend_cnt = esca_get_apsleep_count();
#else
		s2i->suspend_cnt = acpm_get_apsleep_count();
#endif
		return;
	}
	pr_info("%s (cpu : %d)\n", __func__, cpu);

	/* SET SLEEP ONLY CPU0 */
	exynos_s2idle_core_suspend();
}

static void release_last_core_det(void)
{
	exynos_pmu_update(s2i->intgr_offset , (0 << 7), (1 << 7));
	exynos_pmu_write(s2i->sr_offset, 1 << 30);
	/* SPARE_CTRL__DATA7*/
	exynos_pmu_update(s2i->intgr_offset , (1 << 7), (1 << 7));
}

static void android_vh_cpuidle_psci_exit(void *data,
		struct cpuidle_device *dev, bool s2idle)
{
	int cpu = smp_processor_id();
	u32 post_cnt;

	if (!s2idle)
		return;


	if (!cpu) {
		pr_info("%s (cpu : %d)\n", __func__, cpu);
		exynos_s2idle_core_resume();

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
		post_cnt = esca_get_apsleep_count();
#else
		post_cnt = acpm_get_apsleep_count();
#endif
		if (s2i->suspend_cnt != post_cnt) /* enter suspend */
			release_last_core_det();
	}

	/* EXIT C2 */
	if (cpu)
		cal_cpu_enable(cpu);
}

static void register_vendor_hooks(void)
{
	register_trace_android_vh_cpuidle_psci_enter(
			android_vh_cpuidle_psci_enter, NULL);
	register_trace_android_vh_cpuidle_psci_exit(
			android_vh_cpuidle_psci_exit, NULL);
}

static int exynos_s2i_suspend(struct device *dev)
{
	/* Suspend callback function might be registered if necessary */
	return 0;
}

static int exynos_s2i_resume(struct device *dev)
{
	/* Wakeup all secondary core */
	smp_call_function_many(cpu_online_mask, exynos_s2i_ipi_fn, NULL, 1);
	return 0;
}

static int exynos_s2i_probe(struct platform_device *pdev)
{
	int irq_wakeup;
	int ret = 0;

	s2i = devm_kzalloc(&pdev->dev, sizeof(*s2i), GFP_KERNEL);
	if (!s2i)
		return -ENOMEM;

	s2i->dev = &pdev->dev;
	mutex_init(&exynos_s2i_ops_lock);

	exynos_s2i_ops = kzalloc(sizeof(struct exynos_s2i_ops), GFP_KERNEL);
	INIT_LIST_HEAD(&exynos_s2i_ops->node);

	/* gic wakeup delivery */
	ret = of_property_read_u32(pdev->dev.of_node,
				"sr-offset",&s2i->sr_offset);
	if (ret) {
		pr_err("%s %s: unabled to get PMU SR offset\n",
				EXYNOS_PM_PREFIX, __func__);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				"intgr-offset", &s2i->intgr_offset);
	if (ret) {
		pr_err("%s %s: unabled to get PMU INTGR offset\n",
				EXYNOS_PM_PREFIX, __func__);
		return ret;
	}

	/* Wakeup notify */
	s2i->notify_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(s2i->notify_base)) {
		pr_err("%s : unabled to ioremap notify base address\n",
					 __func__);
		return PTR_ERR(s2i->notify_base);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				"notify-offset", &s2i->notify_offset);
	if (ret) {
		pr_err("%s %s: unabled to get notify offset\n",
				EXYNOS_PM_PREFIX, __func__);
		return ret;
	}

	irq_wakeup = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (irq_wakeup < 0)
		return irq_wakeup;

	ret = devm_request_irq(&pdev->dev, irq_wakeup,
			       exynos_s2i_wakeup_interrupt,
			       IRQF_TRIGGER_HIGH,
			       "s2idle_wakeup", s2i);
	if (ret) {
		dev_err(&pdev->dev, "failed to request wakeup interrupt\n");
		return ret;
	}

	ret = irq_set_irq_wake(irq_wakeup, 1);
	if (ret)
		dev_warn(&pdev->dev, "failed to mark wakeup irq as wakeup\n");

	register_vendor_hooks();

	platform_set_drvdata(pdev, s2i);
	pr_info("%s success !\n", __func__);

	return 0;
}

static int exynos_s2i_remove(struct platform_device *pdev)
{
	struct exynos_s2i *s2i = dev_get_drvdata(&pdev->dev);

	mutex_destroy(&exynos_s2i_ops_lock);
	kfree(s2i);

	return 0;
}

static const struct of_device_id exynos_s2i_match[] = {
	{
		.compatible	= "samsung,exynos-s2idle",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_s2i_match);

static const struct dev_pm_ops exynos_s2i_pm_ops = {
	.suspend	= exynos_s2i_suspend,
	.resume		= exynos_s2i_resume,
};

static struct platform_driver exynos_s2i_driver = {
	.probe = exynos_s2i_probe,
	.remove = exynos_s2i_remove,
	.driver  = {
		.name  = "exynos_s2idle",
		.of_match_table = exynos_s2i_match,
		.pm	= &exynos_s2i_pm_ops,
	},
};

static int __init exynos_s2i_init(void)
{
	return platform_driver_register(&exynos_s2i_driver);
}
arch_initcall(exynos_s2i_init);

static void __exit exynos_s2i_exit(void)
{
	platform_driver_unregister(&exynos_s2i_driver);
}
module_exit(exynos_s2i_exit)

MODULE_AUTHOR("Naeun Yoo <ne.yoo@samsung.com>");
MODULE_LICENSE("GPL");
