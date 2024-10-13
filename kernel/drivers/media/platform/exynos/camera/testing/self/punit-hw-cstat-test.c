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

#include "punit-test-hw-ip.h"
#include "punit-test-file-io.h"
#include "punit-hw-cstat-test.h"

static int pst_set_hw_cstat(const char *val, const struct kernel_param *kp);
static int pst_get_hw_cstat(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_cstat = {
	.set = pst_set_hw_cstat,
	.get = pst_get_hw_cstat,
};
module_param_cb(test_hw_cstat, &pablo_param_ops_hw_cstat, NULL, 0644);

static struct is_frame *frame_cstat;
static u32 cstat_param[NUM_OF_CSTAT_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_CSTAT_PARAM];
static struct size_cr_set cstat_cr_set;

static void pst_init_param_cstat_wrap(unsigned int index, enum pst_hw_ip_type type)
{
	pst_init_param_cstat(cstat_param, index);
}

static void pst_set_param_cstat(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_CSTAT_PARAM; i++) {
		pst_set_param(frame, cstat_param[i], PARAM_CSTAT_CONTROL + i);
		pst_set_buf_cstat(frame, cstat_param, i, pb);
	}
}

static void pst_clr_param_cstat(struct is_frame *frame)
{
	int i;
	const enum pst_blob_node *blob_node = pst_get_blob_node_cstat();

	for (i = 0; i < NUM_OF_CSTAT_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(blob_node[i], pb[i]);
		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_cstat(struct is_frame *frame, struct size_cr_set *cr_set)
{

}

static const struct pst_callback_ops pst_cb_cstat = {
	.init_param = pst_init_param_cstat_wrap,
	.set_param = pst_set_param_cstat,
	.clr_param = pst_clr_param_cstat,
	.set_rta_info = pst_set_rta_info_cstat,
};

static int pst_set_hw_cstat(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val, DEV_HW_3AA0, frame_cstat, cstat_param, &cstat_cr_set,
			     pst_get_preset_test_size_cstat(),
			     pst_get_preset_test_result_buf_cstat(), &pst_cb_cstat);
}

static int pst_get_hw_cstat(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "CSTAT", pst_get_preset_test_size_cstat(),
			     pst_get_preset_test_result_buf_cstat());
}
