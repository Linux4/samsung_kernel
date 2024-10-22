// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/types.h>

#include <swpm_dbg_fs_common.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_mml_v6989.h>

int __init swpm_mml_v6989_dbg_init(void)
{

	swpm_mml_v6989_init();

	pr_notice("swpm mml install success\n");

	return 0;
}

void __exit swpm_mml_v6989_dbg_exit(void)
{
	swpm_mml_v6989_exit();
}

module_init(swpm_mml_v6989_dbg_init);
module_exit(swpm_mml_v6989_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6989 SWPM mml debug module");
MODULE_AUTHOR("MediaTek Inc.");
