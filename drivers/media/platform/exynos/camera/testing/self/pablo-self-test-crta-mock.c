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
#include "is-hw.h"

static struct pablo_icpu_adt *saved_icpu_adt;
static struct pablo_crta_bufmgr *saved_crta_bufmgr[IS_STREAM_COUNT];
static struct pablo_crta_bufmgr *mock_crta_bufmgr;

const static struct pablo_icpu_adt_msg_ops pst_icpu_adt_msg_ops_mock = {
};

static const struct pablo_crta_bufmgr_ops pst_crta_bufmgr_ops_mock = {
};

void pst_deinit_crta_mock(struct is_hardware *hw)
{
	int i;

	vfree(mock_crta_bufmgr);
	mock_crta_bufmgr = NULL;
	for (i = 0; i < IS_STREAM_COUNT; i++) {
		if (saved_crta_bufmgr[i]) {
			hw->crta_bufmgr[i] = saved_crta_bufmgr[i];
			saved_crta_bufmgr[i] = NULL;
		}
	}

	vfree(hw->icpu_adt);
	hw->icpu_adt = NULL;
	if (saved_icpu_adt) {
		hw->icpu_adt = saved_icpu_adt;
		saved_icpu_adt = NULL;
	}
}

int pst_init_crta_mock(struct is_hardware *hw)
{
	int ret, i;

	/* icpu_adt mock */
	if (hw->icpu_adt)
		saved_icpu_adt = hw->icpu_adt;

	hw->icpu_adt = vmalloc(sizeof(struct pablo_icpu_adt));
	if (!hw->icpu_adt) {
		pr_err("failed to allocate icpu_adt");
		return -ENOMEM;
	}

	hw->icpu_adt->msg_ops = &pst_icpu_adt_msg_ops_mock;

	/* crta_bufmgr mock */
	mock_crta_bufmgr = vmalloc(sizeof(struct pablo_crta_bufmgr));
	if (!mock_crta_bufmgr) {
		pr_err("failed to allocate mock_crta_bufmgr");
		ret = -ENOMEM;
		goto err_vmalloc_crta_bufmgr;
	}

	mock_crta_bufmgr->ops = &pst_crta_bufmgr_ops_mock;

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		if (hw->crta_bufmgr[i])
			saved_crta_bufmgr[i] = hw->crta_bufmgr[i];

		hw->crta_bufmgr[i] = mock_crta_bufmgr;
	}

	return 0;

err_vmalloc_crta_bufmgr:
	pst_deinit_crta_mock(hw);

	return ret;
}
