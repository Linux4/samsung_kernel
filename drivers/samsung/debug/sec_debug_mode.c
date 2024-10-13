// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/panic_notifier.h>
#include <linux/sec_debug.h>
#include <soc/samsung/exynos/debug-snapshot.h>

/* upload mode en/disable */
int __read_mostly force_upload;
module_param(force_upload, int, 0440);

int secdbg_mode_enter_upload(void)
{
	return force_upload;
}
EXPORT_SYMBOL(secdbg_mode_enter_upload);

static int __init secdbg_mode_init(void)
{
	pr_info("%s: force_upload is %d\n", __func__, force_upload);

	if (!secdbg_mode_enter_upload()) {
		dbg_snapshot_scratch_clear();
		pr_info("%s: dbg_snapshot_scratch_clear done.. (force_upload: %d)\n", __func__, force_upload);
	}

	return 0;
}
module_init(secdbg_mode_init);

static void __exit secdbg_mode_exit(void)
{
	return;
}
module_exit(secdbg_mode_exit);

MODULE_DESCRIPTION("Samsung Debug mode driver");
MODULE_LICENSE("GPL v2");
