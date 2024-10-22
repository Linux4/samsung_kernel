// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include "scp_mbrain_dbg.h"

static struct scp_res_mbrain_dbg_ops _scp_res_mbrain_dbg_ops;

struct scp_res_mbrain_dbg_ops *get_scp_mbrain_dbg_ops(void)
{
	return &_scp_res_mbrain_dbg_ops;
}
EXPORT_SYMBOL(get_scp_mbrain_dbg_ops);

int register_scp_mbrain_dbg_ops(struct scp_res_mbrain_dbg_ops *ops)
{
	if (!ops)
		return -1;

	_scp_res_mbrain_dbg_ops.get_length = ops->get_length;
	_scp_res_mbrain_dbg_ops.get_data = ops->get_data;

	return 0;
}
EXPORT_SYMBOL(register_scp_mbrain_dbg_ops);

void unregister_scp_mbrain_dbg_ops(void)
{
	_scp_res_mbrain_dbg_ops.get_length = NULL;
	_scp_res_mbrain_dbg_ops.get_data = NULL;
}
EXPORT_SYMBOL(unregister_scp_mbrain_dbg_ops);
