// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
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
#include <swpm_disp_v6989.h>

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


int __init swpm_disp_v6989_dbg_init(void)
{
	swpm_disp_v6989_init();

	pr_notice("swpm disp install success\n");

	return 0;
}

void __exit swpm_disp_v6989_dbg_exit(void)
{
	swpm_disp_v6989_exit();
}

module_init(swpm_disp_v6989_dbg_init);
module_exit(swpm_disp_v6989_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6989 SWPM disp debug module");
MODULE_AUTHOR("MediaTek Inc.");
