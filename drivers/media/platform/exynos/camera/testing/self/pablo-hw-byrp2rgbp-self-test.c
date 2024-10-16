// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include "pablo-self-test-hw-ip.h"
#include "pablo-self-test-crta-mock.h"
#include "is-core.h"

static int pst_set_hw_byrp2rgbp(const char *val, const struct kernel_param *kp);
static int pst_get_hw_byrp2rgbp(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_byrp2rgbp = {
	.set = pst_set_hw_byrp2rgbp,
	.get = pst_get_hw_byrp2rgbp,
};
module_param_cb(test_hw_byrp2rgbp, &pablo_param_ops_hw_byrp2rgbp, NULL, 0644);

#define NUM_OF_B2M_PARAM (sizeof(struct b2m_param)/PARAMETER_MAX_SIZE)

static struct is_frame *frame_b2r;
static u32 b2r_param[NUM_OF_B2M_PARAM][PARAMETER_MAX_MEMBER];

static const struct b2m_param b2r_param_preset[] = {
	/* 12 MP full size */
	[0].sz_byrp2yuvp.x_start = 0,
	[0].sz_byrp2yuvp.y_start = 0,
	[0].sz_byrp2yuvp.w_start = 4032,
	[0].sz_byrp2yuvp.h_start = 3024,
	[0].sz_byrp2yuvp.x_byrp = 0,
	[0].sz_byrp2yuvp.y_byrp = 0,
	[0].sz_byrp2yuvp.w_byrp = 4032,
	[0].sz_byrp2yuvp.h_byrp = 3024,
	[0].sz_byrp2yuvp.x_rgbp = 0,
	[0].sz_byrp2yuvp.y_rgbp = 0,
	[0].sz_byrp2yuvp.w_rgbp = 4032,
	[0].sz_byrp2yuvp.h_rgbp = 3024,
};

static DECLARE_BITMAP(result, ARRAY_SIZE(b2r_param_preset));

static void pst_init_param_byrp2rgbp(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct b2m_param *preset = b2r_param_preset;
	const struct pst_callback_ops *pst_cb;

	memcpy(b2r_param[i++], (u32 *)&preset[index].sz_byrp2yuvp, PARAMETER_MAX_SIZE);

	pst_cb = pst_get_hw_byrp_cb();
	CALL_PST_CB(pst_cb, init_param, 0, PST_HW_IP_GROUP);

	pst_cb = pst_get_hw_rgbp_cb();
	CALL_PST_CB(pst_cb, init_param, 0, PST_HW_IP_GROUP);
}

static void pst_set_param_byrp2rgbp(struct is_frame *frame)
{
	int i = 0;
	struct b2m_param *p = (struct b2m_param *)b2r_param;
	const struct pst_callback_ops *pst_cb;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;

	if (pst_init_crta_mock(hw))
		pr_err("failed to pst_init_crta_mock\n");

	pst_cb = pst_get_hw_byrp_cb();
	CALL_PST_CB(pst_cb, set_size, &p->sz_byrp2yuvp, &p->sz_byrp2yuvp);
	CALL_PST_CB(pst_cb, set_param, frame);

	pst_cb = pst_get_hw_rgbp_cb();
	CALL_PST_CB(pst_cb, set_size, &p->sz_byrp2yuvp, &p->sz_byrp2yuvp);
	CALL_PST_CB(pst_cb, set_param, frame);

	memset(frame->hw_slot_id, HW_SLOT_MAX, sizeof(frame->hw_slot_id));
	frame->hw_slot_id[i++] = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_BYRP);
	frame->hw_slot_id[i++] = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_RGBP);
}

static void pst_clr_param_byrp2rgbp(struct is_frame *frame)
{
	const struct pst_callback_ops *pst_cb;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;

	pst_cb = pst_get_hw_byrp_cb();
	CALL_PST_CB(pst_cb, clr_param, frame);

	pst_cb = pst_get_hw_rgbp_cb();
	CALL_PST_CB(pst_cb, clr_param, frame);

	pst_deinit_crta_mock(hw);
}

static void pst_set_rta_info_byrp2rgbp(struct is_frame *frame, struct size_cr_set *cr_set)
{
}

static const struct pst_callback_ops pst_cb_b2r = {
	.init_param = pst_init_param_byrp2rgbp,
	.set_param = pst_set_param_byrp2rgbp,
	.clr_param = pst_clr_param_byrp2rgbp,
	.set_rta_info = pst_set_rta_info_byrp2rgbp,
};

static int pst_set_hw_byrp2rgbp(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_END,
			frame_b2r,
			b2r_param,
			NULL,
			ARRAY_SIZE(b2r_param_preset),
			result,
			&pst_cb_b2r);
}

static int pst_get_hw_byrp2rgbp(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "BYRP2RGBP", ARRAY_SIZE(b2r_param_preset), result);
}
