// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>

#include <lpm_dbg_fs_common.h>

void lpm_dbg_fs_exit(void)
{
	lpm_cpuidle_fs_deinit();
	lpm_rc_fs_deinit();
	lpm_trace_fs_deinit();
	lpm_hwreq_fs_deinit();
}
EXPORT_SYMBOL(lpm_dbg_fs_exit);

int lpm_dbg_fs_init(void)
{
	lpm_trace_fs_init();
	lpm_rc_fs_init();
	lpm_cpuidle_fs_init();
	lpm_hwreq_fs_init();
	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}
EXPORT_SYMBOL(lpm_dbg_fs_init);
