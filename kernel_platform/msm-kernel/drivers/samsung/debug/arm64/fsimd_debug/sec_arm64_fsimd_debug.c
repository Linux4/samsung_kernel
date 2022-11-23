// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2019-2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include <asm/fpsimd.h>

#include <trace/hooks/fpsimd.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/debug/sec_force_err.h>

static int __fsimd_debug_parse_dt_check_debug_level(struct builder *bd,
		struct device_node *np)
{
	struct device *dev = bd->dev;
	int nr_dbg_level;
	unsigned int sec_dbg_level;
	u32 dbg_level;
	int i;

	nr_dbg_level = of_property_count_u32_elems(np, "sec,debug_level");
	if (nr_dbg_level <= 0) {
		dev_warn(dev, "this crashkey_dev (%s) will be enabled all sec debug levels!\n",
				dev_name(dev));
		return 0;
	}

	sec_dbg_level = sec_debug_level();
	for (i = 0; i < nr_dbg_level; i++) {
		of_property_read_u32_index(np, "sec,debug_level", i,
				&dbg_level);
		if (sec_dbg_level == (unsigned int)dbg_level)
			return 0;
	}

	return -ENODEV;
}

static struct dt_builder __fsimd_debug_dt_builder[] = {
	DT_BUILDER(__fsimd_debug_parse_dt_check_debug_level),
};

static int __fsimd_debug_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __fsimd_debug_dt_builder,
			ARRAY_SIZE(__fsimd_debug_dt_builder));
}

#if IS_BUILTIN(CONFIG_SEC_ARM64_FSIMD_DEBUG)
static __always_inline void __fpsimd_save_state(struct user_fpsimd_state *state)
{
	fpsimd_save_state(state);
}
#else
/* NOTE: copied from arch/arm64/kernel/entry-fpsimd.S */
static void __naked __fpsimd_save_state(struct user_fpsimd_state *state)
{
	asm volatile (
		"stp	q0, q1, [x0] \n\t"
		"stp	q2, q3, [x0, #32] \n\t"
		"stp	q4, q5, [x0, #64] \n\t"
		"stp	q6, q7, [x0, #96] \n\t"
		"stp	q8, q9, [x0, #128] \n\t"
		"stp	q10, q11, [x0, #160] \n\t"
		"stp	q12, q13, [x0, #192] \n\t"
		"stp	q14, q15, [x0, #224] \n\t"
		"stp	q16, q17, [x0, #256] \n\t"
		"stp	q18, q19, [x0, #288] \n\t"
		"stp	q20, q21, [x0, #320] \n\t"
		"stp	q22, q23, [x0, #352] \n\t"
		"stp	q24, q25, [x0, #384] \n\t"
		"stp	q26, q27, [x0, #416] \n\t"
		"stp	q28, q29, [x0, #448] \n\t"
		"stp	q30, q31, [x0, #480]! \n\t"
		"mrs	x8, fpsr \n\t"
		"str	w8, [x0, #32] \n\t"
		"mrs	x8, fpcr \n\t"
		"str	w8, [x0, #36] \n\t"
		"ret \n\t"
	);
}
#endif

static void __trace_android_vh_is_fpsimd_save(void *unused,
		struct task_struct *prev, struct task_struct *next)
{
	struct user_fpsimd_state current_st;
	struct user_fpsimd_state *saved_st = &next->thread.uw.fpsimd_state;
	size_t i;

	if (test_tsk_thread_flag(next, TIF_FOREIGN_FPSTATE))
		return;

	__fpsimd_save_state(&current_st);

	for (i = 0; i < ARRAY_SIZE(current_st.vregs); i++)
		BUG_ON(current_st.vregs[i] != saved_st->vregs[i]);

	BUG_ON((current_st.fpsr != saved_st->fpsr) ||
	       (current_st.fpcr != saved_st->fpcr));
}

static int __fsimd_debug_install_vendor_hook(struct builder *bd)
{
	return register_trace_android_vh_is_fpsimd_save(
			__trace_android_vh_is_fpsimd_save,
			NULL);
}

static void __fsimd_debug_uninstall_vendor_hook(struct builder *bd)
{
	unregister_trace_android_vh_is_fpsimd_save(
			__trace_android_vh_is_fpsimd_save,
			NULL);
}

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

static noinline unsigned long __fsimd_random_pi(unsigned long seed)
{
	unsigned long pi_int;

	pi_int = (unsigned long)(M_PI * seed);

	return pi_int;
}

static void sec_arm64_fsimd_random_pi(struct work_struct *work)
{
	unsigned long pi_int, seed;
	size_t i;

	for (i = 0; i < 80; i++) {
		seed = get_random_long() % 100UL;
		pi_int = __fsimd_random_pi(seed);
		pr_info("int(M_PI * %lu) = %lu\n",
				seed, pi_int);
		msleep(10);
	}
}

static struct work_struct random_pi_work[10];

static int __fsimd_debug_init_random_pi_work(struct builder *bd)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(random_pi_work); i++)
		INIT_WORK(&random_pi_work[i], sec_arm64_fsimd_random_pi);

	return 0;
}

static void __simulate_fsimd_error(struct force_err_handle *h)
{
	size_t i;

	pr_emerg("Simulating fsimd error in kernel space\n");

	for (i = 0; i < ARRAY_SIZE(random_pi_work); i++)
		queue_work(system_long_wq, &random_pi_work[i]);

	ssleep(1);

	/* if we reach here, simulation failed */
	pr_emerg("Simulation of fsimd error failed\n");
}

static struct force_err_handle __fsimd_force_error =
	FORCE_ERR_HANDLE("fsimd_err", "Generating an fsimd error!",
			__simulate_fsimd_error);

static int __fsimd_debug_add_force_err(struct builder *bd)
{
	return sec_force_err_add_custom_handle(&__fsimd_force_error);
}

static void __fsimd_debug_del_force_err(struct builder *bd)
{
	sec_force_err_del_custom_handle(&__fsimd_force_error);
}

static struct dev_builder __fsimd_debug_dev_builder[] = {
	DEVICE_BUILDER(__fsimd_debug_parse_dt, NULL),
	DEVICE_BUILDER(__fsimd_debug_init_random_pi_work, NULL),
	DEVICE_BUILDER(__fsimd_debug_install_vendor_hook,
		       __fsimd_debug_uninstall_vendor_hook),
	DEVICE_BUILDER(__fsimd_debug_add_force_err,
		       __fsimd_debug_del_force_err),
};

static int __fsimd_debug_probe(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct builder bd_dummy = { .dev = dev };
	int err;

	err = sec_director_probe_dev(&bd_dummy, builder, n);
	if (err)
		return err;

	return 0;
}

static int __fsimd_debug_remove(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct builder bd_dummy = { .dev = dev };

	sec_director_destruct_dev(&bd_dummy, builder, n, n);

	return 0;
}

static int sec_fsimd_debug_probe(struct platform_device *pdev)
{
	return __fsimd_debug_probe(pdev, __fsimd_debug_dev_builder,
			ARRAY_SIZE(__fsimd_debug_dev_builder));
}

static int sec_fsimd_debug_remove(struct platform_device *pdev)
{
	return __fsimd_debug_remove(pdev, __fsimd_debug_dev_builder,
			ARRAY_SIZE(__fsimd_debug_dev_builder));
}

static const struct of_device_id sec_fsimd_debug_match_table[] = {
	{ .compatible = "samsung,fsimd_debug" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_fsimd_debug_match_table);

static struct platform_driver sec_fsimd_debug_driver = {
	.driver = {
		.name = "samsung,fsimd_debug",
		.of_match_table = of_match_ptr(sec_fsimd_debug_match_table),
	},
	.probe = sec_fsimd_debug_probe,
	.remove = sec_fsimd_debug_remove,
};

static int __init sec_fsimd_debug_init(void)
{
	return platform_driver_register(&sec_fsimd_debug_driver);
}
#if IS_BUILTIN(CONFIG_SEC_ARM64_FSIMD_DEBUG)
arch_initcall_sync(sec_fsimd_debug_init);
#else
module_init(sec_fsimd_debug_init);
#endif

static void __exit sec_fsimd_debug_exit(void)
{
	return platform_driver_unregister(&sec_fsimd_debug_driver);
}
module_exit(sec_fsimd_debug_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Detecting fsimd register corruption");
MODULE_LICENSE("GPL v2");
