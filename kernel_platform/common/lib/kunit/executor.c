// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

/*
 * These symbols point to the .kunit_test_suites section and are defined in
 * include/asm-generic/vmlinux.lds.h, and consequently must be extern.
 */
extern struct kunit_suite * const * const __kunit_suites_start[];
extern struct kunit_suite * const * const __kunit_suites_end[];

#if IS_BUILTIN(CONFIG_KUNIT)

static void kunit_print_tap_header(void)
{
	struct kunit_suite * const * const *suites, * const *subsuite;
	int num_of_suites = 0;

	for (suites = __kunit_suites_start;
	     suites < __kunit_suites_end;
	     suites++)
		for (subsuite = *suites; *subsuite != NULL; subsuite++)
			num_of_suites++;

	pr_info("TAP version 14\n");
	pr_info("1..%d\n", num_of_suites);
}

int kunit_run_all_tests(void)
{
	struct kunit_suite * const * const *suites;

	kunit_print_tap_header();

	for (suites = __kunit_suites_start;
	     suites < __kunit_suites_end;
	     suites++)
			__kunit_test_suites_init(*suites);

	return 0;
}

BLOCKING_NOTIFIER_HEAD(kunit_notify_chain);

int test_executor_init(void)
{
#ifndef CONFIG_UML
	int noti = 0;
#endif
	/* Trigger the built-in kunit tests */
	if (!kunit_run_all_tests())
		pr_warn("Running built-in kunit tests are unsuccessful.\n");
#ifndef CONFIG_UML
	/* Trigger the module kunit tests */
	noti = blocking_notifier_call_chain(&kunit_notify_chain, 0, NULL);
	if (noti == NOTIFY_OK || noti == NOTIFY_DONE)
		return 0;
	pr_warn("Running kunit_notifier_calls are unsuccessful. errno: 0x%x", noti);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(test_executor_init);

int register_kunit_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&kunit_notify_chain, nb);
}
EXPORT_SYMBOL(register_kunit_notifier);

int unregister_kunit_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&kunit_notify_chain, nb);
}
EXPORT_SYMBOL(unregister_kunit_notifier);
#endif /* IS_BUILTIN(CONFIG_KUNIT) */
