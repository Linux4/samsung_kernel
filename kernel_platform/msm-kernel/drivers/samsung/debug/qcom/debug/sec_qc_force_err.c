// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/qcom_scm.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_force_err.h>

#include <soc/qcom/watchdog.h>

static void __qc_simulate_secure_wdog_bite(struct force_err_handle *h)
{
	int err;

	if (!qcom_scm_is_secure_wdog_trigger_available()) {
		pr_emerg("secure wdog trigger is not available\n");
		goto __fallback;
	}

	err = qcom_scm_sec_wdog_trigger();
	if (err) {
		pr_emerg("secure wdog trigger is failed (%d)\n", err);
		goto __fallback;
	}

	return;

__fallback:
	qcom_wdt_trigger_bite();
}

static struct force_err_handle __qc_force_err_default[] = {
	FORCE_ERR_HANDLE("secdogbite", "simulating secure watch dog bite!",
			__qc_simulate_secure_wdog_bite),
};

static ssize_t __qc_force_err_add_handlers(ssize_t begin)
{
	struct force_err_handle *h;
	int err = 0;
	ssize_t n = ARRAY_SIZE(__qc_force_err_default);
	ssize_t i;

	for (i = begin; i < n; i++) {
		h = &__qc_force_err_default[i];

		INIT_HLIST_NODE(&h->node);

		err = sec_force_err_add_custom_handle(h);
		if (err) {
			pr_err("failed to add a handler - %ps (%d)\n",
					h->func, err);
			return -i;
		}
	}

	return n;
}

static void __qc_force_err_del_handlers(ssize_t last_failed)
{
	struct force_err_handle *h;
	int err = 0;
	ssize_t n = ARRAY_SIZE(__qc_force_err_default);
	ssize_t i;

	BUG_ON((last_failed < 0) || (last_failed > n));

	for (i = last_failed - 1; i >= 0; i--) {
		h = &__qc_force_err_default[i];

		err = sec_force_err_del_custom_handle(h);
		if (err)
			pr_warn("failed to add a handler - %ps (%d)\n",
					h->func, err);
	}
}

static int __qc_force_err_init_prolog(struct builder *bd)
{
	ssize_t last_failed;
	int err = 0;

	last_failed = __qc_force_err_add_handlers(0);
	if (last_failed <= 0) {
		err = -EINVAL;
		goto err_add_handlers;
	}

	return 0;

err_add_handlers:
	__qc_force_err_del_handlers(-last_failed);
	return err;
}

static void __qc_force_err_exit_epilog(struct builder *bd)
{
	__qc_force_err_del_handlers(ARRAY_SIZE(__qc_force_err_default));
}

static struct dev_builder __qc_force_err_dev_builder[] = {
	DEVICE_BUILDER(__qc_force_err_init_prolog, __qc_force_err_exit_epilog),
};

int sec_qc_force_err_init(struct builder *bd)
{
	struct builder bd_dummy = { .dev = NULL, };

	if (!IS_ENABLED(CONFIG_SEC_FORCE_ERR))
		return 0;

	return sec_director_probe_dev(&bd_dummy, __qc_force_err_dev_builder,
			ARRAY_SIZE(__qc_force_err_dev_builder));
}

void sec_qc_force_err_exit(struct builder *bd)
{
	struct builder bd_dummy = { .dev = NULL, };

	if (!IS_ENABLED(CONFIG_SEC_FORCE_ERR))
		return;

	sec_director_destruct_dev(&bd_dummy, __qc_force_err_dev_builder,
			ARRAY_SIZE(__qc_force_err_dev_builder),
			ARRAY_SIZE(__qc_force_err_dev_builder));
}
