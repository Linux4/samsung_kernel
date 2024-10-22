// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/fs.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_sys_res.h>
#include <lpm_sys_res_fs.h>

static struct lpm_sys_res_ops _lpm_sys_res_ops;

int lpm_sys_res_init(void)
{
	lpm_sys_res_fs_init();
	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}
EXPORT_SYMBOL(lpm_sys_res_init);

void lpm_sys_res_exit(void)
{
	lpm_sys_res_fs_deinit();
	pr_info("%s %d: finish", __func__, __LINE__);
}
EXPORT_SYMBOL(lpm_sys_res_exit);

struct lpm_sys_res_ops *get_lpm_sys_res_ops(void)
{
	return &_lpm_sys_res_ops;
}
EXPORT_SYMBOL(get_lpm_sys_res_ops);

int register_lpm_sys_res_ops(struct lpm_sys_res_ops *ops)
{
	if (!ops)
		return -1;

	_lpm_sys_res_ops.get = ops->get;
	_lpm_sys_res_ops.update = ops->update;
	_lpm_sys_res_ops.get_detail = ops->get_detail;
	_lpm_sys_res_ops.get_threshold = ops->get_threshold;
	_lpm_sys_res_ops.set_threshold = ops->set_threshold;
	_lpm_sys_res_ops.enable_common_log = ops->enable_common_log;
	_lpm_sys_res_ops.get_log_enable = ops->get_log_enable;
	_lpm_sys_res_ops.log = ops->log;
	_lpm_sys_res_ops.lock = ops->lock;
	_lpm_sys_res_ops.get_id_name = ops->get_id_name;

	return 0;
}
EXPORT_SYMBOL(register_lpm_sys_res_ops);

void unregister_lpm_sys_res_ops(void)
{
	_lpm_sys_res_ops.get = NULL;
	_lpm_sys_res_ops.update = NULL;
	_lpm_sys_res_ops.get_detail = NULL;
	_lpm_sys_res_ops.get_threshold = NULL;
	_lpm_sys_res_ops.set_threshold = NULL;
	_lpm_sys_res_ops.enable_common_log = NULL;
	_lpm_sys_res_ops.get_log_enable = NULL;
	_lpm_sys_res_ops.log = NULL;
	_lpm_sys_res_ops.lock = NULL;
	_lpm_sys_res_ops.get_id_name = NULL;
}

EXPORT_SYMBOL(unregister_lpm_sys_res_ops);
