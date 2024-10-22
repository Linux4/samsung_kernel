// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <mtk_cpupm_dbg.h>
#include <lpm_dbg_common_v2.h>
#include <lpm_module.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_logger.h>
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
#include <lpm_sys_res.h>
#endif

static int __init dbg_early_initcall(void)
{
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
subsys_initcall(dbg_early_initcall);
#endif

static int __init dbg_device_initcall(void)
{
	lpm_dbg_common_fs_init();
	lpm_dbg_fs_init();
	mtk_cpupm_dbg_init();
	return 0;
}

static int __init dbg_late_initcall(void)
{
	lpm_logger_init();
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
late_initcall_sync(dbg_late_initcall);
#endif

int __init lpm_dbg_init(void)
{
	int ret = 0;
#ifdef MTK_LPM_MODE_MODULE
	ret = dbg_early_initcall();
#endif
	if (ret)
		goto dbg_init_fail;

	ret = dbg_device_initcall();

	if (ret)
		goto dbg_init_fail;

#ifdef MTK_LPM_MODE_MODULE
	ret = dbg_late_initcall();
#endif

	if (ret)
		goto dbg_init_fail;

	ret = lpm_dbg_pm_init();

	if (ret)
		goto dbg_init_fail;
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
	lpm_sys_res_init();
#endif
	return 0;

dbg_init_fail:
	return -EAGAIN;
}

void __exit lpm_dbg_exit(void)
{
	lpm_dbg_pm_exit();
	lpm_dbg_common_fs_exit();
	lpm_dbg_fs_exit();
	mtk_cpupm_dbg_exit();
	lpm_logger_deinit();
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
	lpm_sys_res_exit();
#endif
}

module_init(lpm_dbg_init);
module_exit(lpm_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("low power debug module");
MODULE_AUTHOR("MediaTek Inc.");
