// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_CSTAT_SELF_TEST_H
#define PABLO_HW_CSTAT_SELF_TEST_H

#define NUM_OF_CSTAT_PARAM (PARAM_CSTAT_MAX - PARAM_CSTAT_CONTROL + 1)

size_t pst_get_preset_test_size_cstat(void);
unsigned long *pst_get_preset_test_result_buf_cstat(void);
const enum pst_blob_node *pst_get_blob_node_cstat(void);
void pst_set_buf_cstat(struct is_frame *frame, u32 param[][PARAMETER_MAX_MEMBER], u32 param_idx,
		       struct is_priv_buf *pb[NUM_OF_CSTAT_PARAM]);
void pst_init_param_cstat(u32 param[][PARAMETER_MAX_MEMBER], unsigned int index);

#endif /* PABLO_HW_CSTAT_SELF_TEST_H */
