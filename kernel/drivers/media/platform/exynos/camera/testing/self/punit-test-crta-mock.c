// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>

#include "pablo-icpu-adapter.h"
#include "pablo-crta-bufmgr.h"
#include "is-hw.h"
#include "is-core.h"

static struct pablo_icpu_adt *icpu_adt_save;
static struct pablo_icpu_adt *icpu_adt_mock;
static atomic_t refcnt;

struct cb_handler {
	pablo_response_msg_cb cb[IS_STREAM_COUNT];
};

struct icpu_adt_v2 {
	struct cb_handler cb_handler[PABLO_HIC_MAX];
};

static int pst_icpu_adt_send_msg_cstat_frame_end(struct pablo_icpu_adt *icpu_adt, u32 instance,
						 void *cb_caller, void *cb_ctx, u32 fcount,
						 struct pablo_crta_buf_info *shot_buf,
						 struct pablo_crta_buf_info *stat_buf,
						 u32 edge_score)
{
	struct icpu_adt_v2 *adt = (struct icpu_adt_v2 *)icpu_adt->priv;
	u32 cmd = PABLO_HIC_CSTAT_FRAME_END;
	struct pablo_icpu_adt_rsp_msg rsp_msg;

	mdbg_adt(1, "%s\n", instance, __func__);

	rsp_msg.instance = instance;
	rsp_msg.fcount = fcount;

	adt->cb_handler[cmd].cb[instance](cb_caller, cb_ctx, &rsp_msg);

	return 0;
}

static int pst_icpu_adt_send_msg_shot(struct pablo_icpu_adt *icpu_adt, u32 instance,
				      void *cb_caller, void *cb_ctx, u32 fcount,
				      struct pablo_crta_buf_info *shot_buf,
				      struct pablo_crta_buf_info *pcfi_buf)
{
	struct icpu_adt_v2 *adt = (struct icpu_adt_v2 *)icpu_adt->priv;
	u32 cmd = PABLO_HIC_SHOT;
	struct pablo_icpu_adt_rsp_msg rsp_msg;

	mdbg_adt(1, "%s: [F%d]shot(0x%llx) pcfi(0x%llx)", instance, __func__, fcount, shot_buf->dva,
		 pcfi_buf->dva);

	rsp_msg.instance = instance;
	rsp_msg.fcount = fcount;

	adt->cb_handler[cmd].cb[instance](cb_caller, cb_ctx, &rsp_msg);

	return 0;
}

static int pst_icpu_adt_register_response_msg_cb(struct pablo_icpu_adt *icpu_adt, u32 instance,
						 enum pablo_hic_cmd_id msg,
						 pablo_response_msg_cb cb)
{
	struct icpu_adt_v2 *adt = (struct icpu_adt_v2 *)icpu_adt->priv;

	mdbg_adt(1, "%s\n", instance, __func__);

	adt->cb_handler[msg].cb[instance] = cb;

	return 0;
}

const static struct pablo_icpu_adt_msg_ops pst_icpu_adt_msg_ops_mock = {
	.register_response_msg_cb = pst_icpu_adt_register_response_msg_cb,
	.send_msg_open = NULL,
	.send_msg_put_buf = NULL,
	.send_msg_set_shared_buf_idx = NULL,
	.send_msg_start = NULL,
	.send_msg_shot = pst_icpu_adt_send_msg_shot,
	.send_msg_cstat_frame_start = NULL,
	.send_msg_cstat_cdaf_end = NULL,
	.send_msg_pdp_stat0_end = NULL,
	.send_msg_pdp_stat1_end = NULL,
	.send_msg_cstat_frame_end = pst_icpu_adt_send_msg_cstat_frame_end,
	.send_msg_stop = NULL,
	.send_msg_close = NULL,

};

const static struct pablo_icpu_adt_ops pst_icpu_adt_ops_mock = {
};

void pst_deinit_crta_mock(void)
{
	struct is_hardware *hw = &is_get_is_core()->hardware;

	if (atomic_dec_return(&refcnt) > 0)
		return;

	kvfree(icpu_adt_mock->priv);
	icpu_adt_mock->priv = NULL;

	kvfree(icpu_adt_mock);
	icpu_adt_mock = NULL;

	hw->icpu_adt = icpu_adt_save;
	pablo_set_icpu_adt(icpu_adt_save);
	icpu_adt_save = NULL;

	pr_info("%s(refcnt:%d)\n", __func__, atomic_read(&refcnt));
}

int pst_init_crta_mock(void)
{
	int ret;
	struct is_hardware *hw = &is_get_is_core()->hardware;
	struct icpu_adt_v2 *adt;

	if (atomic_inc_return(&refcnt) > 1)
		return 0;

	icpu_adt_save = pablo_get_icpu_adt();

	icpu_adt_mock = kvzalloc(sizeof(struct pablo_icpu_adt), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(icpu_adt_mock)) {
		pr_err("failed to allocate icpu_adt_mock");
		ret = -ENOMEM;
		goto err_alloc_icpu_adt_mock;
	}

	adt = kvzalloc(sizeof(struct icpu_adt_v2), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(adt)) {
		merr_adt("failed to alloc icpu_adt_mock", 0);
		ret = -ENOMEM;
		goto err_alloc_adt;
	}

	icpu_adt_mock->priv = (void *)adt;
	icpu_adt_mock->ops = &pst_icpu_adt_ops_mock;
	icpu_adt_mock->msg_ops = &pst_icpu_adt_msg_ops_mock;

	hw->icpu_adt = icpu_adt_mock;
	pablo_set_icpu_adt(icpu_adt_mock);

	pr_info("%s(refcnt:%d)\n", __func__, atomic_read(&refcnt));

	return 0;

err_alloc_adt:
	kvfree(icpu_adt_mock);

err_alloc_icpu_adt_mock:
	atomic_dec(&refcnt);

	return ret;
}
