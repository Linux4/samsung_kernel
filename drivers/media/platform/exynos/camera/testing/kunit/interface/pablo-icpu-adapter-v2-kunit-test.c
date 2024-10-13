// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-debug.h"
#include "pablo-kunit-test.h"
#include "icpu/pablo-icpu-core.h"
#include "pablo-icpu-itf.h"
#include "icpu/hardware/pablo-icpu-hw.h"
#include "icpu/mbox/pablo-icpu-mbox.h"
#include "pablo-icpu-adapter.h"
#include "pablo-crta-bufmgr.h"
#include "pablo-icpu-cmd-v1.h"
#include "is-interface-sensor.h"
#include "is-config.h"
#include "linux/random.h"
#include "pablo-crta-interface.h"

#define MBOX_IHC_PARAM			8
#define SET_MSG_VALUE(val, mask, shift)	((val << shift) & mask)
#define GET_DVA_HIGH(dva)		((dva & DVA_HIGH_MASK) >> DVA_HIGH_SHIFT)
#define GET_DVA_LOW(dva)		((dva & DVA_LOW_MASK) >> DVA_LOW_SHIFT)

static struct icpu_platform_data *__pdata;
static struct icpu_platform_data __test_pdata;
static struct pablo_icpu_adt *__adt;
static struct pablo_icpu_itf_api *__itf;
static struct pablo_crta_bufmgr __bufmgr;
static struct is_sensor_interface __sensor_itf;

/* Define the test cases. */
static struct test_ctx {
	struct pablo_icpu_adt *adt;
	struct icpu_mbox_controller tx_mbox;
	struct icpu_mbox_controller rx_mbox;
	struct pablo_icpu_mbox_chan tx_chan;
	struct pablo_icpu_mbox_chan rx_chan;
	struct icpu_mbox_client cl;
	bool rsp_msg;	/* should send rsp msg */
	u32 rx_data[MBOX_IHC_PARAM];
	struct semaphore smp_rsp;
	const struct pablo_memlog_operations *pml_ops_b;
	const struct pablo_dss_operations *pdss_ops_b;

	struct kunit *test;
} __ctx;

static void pablo_icpu_adt_v2_open_kunit_test(struct kunit *test)
{
	int ret;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);

	ret = __adt->ops->open(__adt, 0, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_open_negative_kunit_test(struct kunit *test)
{
	int ret;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);

	ret = __adt->ops->open(__adt, IS_STREAM_COUNT, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, -EINVAL);

	ret = __adt->ops->close(__adt, 0);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);

	/* TODO: check open same instance again */
}

static int __test_rsp_cb_func(void *user, void *ctx, void *rsp_msg)
{
	return 0;
}

static void pablo_icpu_adt_v2_register_response_cb_kunit_test(struct kunit *test)
{
	int ret;
	enum pablo_hic_cmd_id msg = PABLO_HIC_OPEN;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->register_response_msg_cb);

	ret = __adt->ops->open(__adt, 0, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->msg_ops->register_response_msg_cb(__adt, 0, msg, __test_rsp_cb_func);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_register_response_cb_negative_kunit_test(struct kunit *test)
{
	int ret;
	enum pablo_hic_cmd_id msg = PABLO_HIC_OPEN;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->register_response_msg_cb);

	ret = __adt->msg_ops->register_response_msg_cb(__adt, 0, msg, __test_rsp_cb_func);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);

	ret = __adt->ops->open(__adt, 0, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	msg = PABLO_HIC_MAX;
	ret = __adt->msg_ops->register_response_msg_cb(__adt, 0, msg, __test_rsp_cb_func);
	KUNIT_ASSERT_EQ(test, ret, -EINVAL);

	ret = __adt->ops->close(__adt, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_open_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	u32 hw_id = 0;
	u32 rep_flag = 0;
	u32 position = 0;
	u32 f_type = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_open);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_open(__adt, instance, hw_id, rep_flag, position, f_type);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_open_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	u32 hw_id = 0;
	u32 rep_flag = 0;
	u32 position = 0;
	u32 f_type = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_open);

	/* icpu adt sends msg */
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_open(__adt, instance, hw_id, rep_flag, position, f_type);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_put_buf_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	enum pablo_buffer_type type = PABLO_BUFFER_RTA;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_put_buf);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info.id, sizeof(u32));
	get_random_bytes(&buf_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_put_buf(__adt, instance, type, &buf_info);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_put_buf_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	enum pablo_buffer_type type = PABLO_BUFFER_RTA;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_put_buf);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info.id, sizeof(u32));
	get_random_bytes(&buf_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_put_buf(__adt, instance, type, &buf_info);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_set_shared_buf_idx_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	enum pablo_buffer_type type = PABLO_BUFFER_RTA;
	u32 fcnt = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_set_shared_buf_idx);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_set_shared_buf_idx(__adt, instance, type, fcnt);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_set_shared_buf_idx_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	enum pablo_buffer_type type = PABLO_BUFFER_RTA;
	u32 fcnt = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_set_shared_buf_idx);

	/* icpu adt sends msg */
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_set_shared_buf_idx(__adt, instance, type, fcnt);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_start_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_start);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info.kva, sizeof(void *));
	get_random_bytes(&buf_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_start(__adt, instance, &buf_info);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_shot_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	struct pablo_crta_buf_info shot_info = { 0, }, prfi_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_shot);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	get_random_bytes(&shot_info.dva, sizeof(dma_addr_t));
	get_random_bytes(&prfi_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_shot(__adt, instance, cb_user, cb_ctx,
			fcount, &shot_info, &prfi_info);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_shot_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	struct pablo_crta_buf_info shot_info = { 0, }, prfi_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_shot);

	/* icpu adt sends msg */
	get_random_bytes(&shot_info.dva, sizeof(dma_addr_t));
	get_random_bytes(&prfi_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_shot(__adt, instance, cb_user, cb_ctx,
			fcount, &shot_info, &prfi_info);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_cstat_frame_start_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_cstat_frame_start);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_cstat_frame_start(__adt, instance, cb_user, cb_ctx,
			fcount, &buf_info);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_cstat_frame_start_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_cstat_frame_start);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_cstat_frame_start(__adt, instance, cb_user, cb_ctx,
			fcount, &buf_info);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_cstat_cdaf_end_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	u32 dva_num = 1;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_cstat_cdaf_end);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_cstat_cdaf_end(__adt, instance, cb_user, cb_ctx,
			fcount, dva_num, &buf_info);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_cstat_cdaf_end_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	u32 dva_num = 1;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_cstat_cdaf_end);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info.dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_cstat_cdaf_end(__adt, instance, cb_user, cb_ctx,
			fcount, dva_num, &buf_info);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_pdp_stat0_end_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	u32 dva_num = 2;
	struct pablo_crta_buf_info buf_info[2] = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_pdp_stat0_end);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info[0].dva, sizeof(dma_addr_t));
	get_random_bytes(&buf_info[1].dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_pdp_stat0_end(__adt, instance, cb_user, cb_ctx,
			fcount, dva_num, buf_info);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_pdp_stat0_end_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	u32 dva_num = 2;
	struct pablo_crta_buf_info buf_info[2] = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_pdp_stat0_end);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info[0].dva, sizeof(dma_addr_t));
	get_random_bytes(&buf_info[1].dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_pdp_stat0_end(__adt, instance, cb_user, cb_ctx,
			fcount, dva_num, buf_info);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_pdp_stat1_end_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	u32 dva_num = 2;
	struct pablo_crta_buf_info buf_info[2] = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_pdp_stat1_end);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info[0].dva, sizeof(dma_addr_t));
	get_random_bytes(&buf_info[1].dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_pdp_stat1_end(__adt, instance, cb_user, cb_ctx,
			fcount, dva_num, buf_info);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_pdp_stat1_end_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	u32 dva_num = 2;
	struct pablo_crta_buf_info buf_info[2] = { 0, };

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_pdp_stat1_end);

	/* icpu adt sends msg */
	get_random_bytes(&buf_info[0].dva, sizeof(dma_addr_t));
	get_random_bytes(&buf_info[1].dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_pdp_stat1_end(__adt, instance, cb_user, cb_ctx,
			fcount, dva_num, buf_info);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_cstat_frame_end_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	struct pablo_crta_buf_info shot_info = { 0, };
	u32 dva_num = 2;
	struct pablo_crta_buf_info buf_info[2] = { 0, };
	u32 edge_score = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_cstat_frame_end);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	get_random_bytes(&shot_info.dva, sizeof(dma_addr_t));
	get_random_bytes(&buf_info[0].dva, sizeof(dma_addr_t));
	get_random_bytes(&buf_info[1].dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_cstat_frame_end(__adt, instance, cb_user, cb_ctx,
			fcount, &shot_info, dva_num, buf_info, edge_score);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_cstat_frame_end_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	void *cb_user = NULL;
	void *cb_ctx = NULL;
	u32 fcount = 0;
	struct pablo_crta_buf_info shot_info = { 0, };
	u32 dva_num = 2;
	struct pablo_crta_buf_info buf_info[2] = { 0, };
	u32 edge_score = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_cstat_frame_end);

	/* icpu adt sends msg */
	get_random_bytes(&shot_info.dva, sizeof(dma_addr_t));
	get_random_bytes(&buf_info[0].dva, sizeof(dma_addr_t));
	get_random_bytes(&buf_info[1].dva, sizeof(dma_addr_t));
	__ctx.rsp_msg = false;
	ret = __adt->msg_ops->send_msg_cstat_frame_end(__adt, instance, cb_user, cb_ctx,
			fcount, &shot_info, dva_num, buf_info, edge_score);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_stop_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_stop);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_stop(__adt, instance, PABLO_STOP_IMMEDIATE);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_stop_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_stop);

	/* icpu adt sends msg */
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_stop(__adt, instance, PABLO_STOP_IMMEDIATE);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_send_msg_close_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_close);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* icpu adt sends msg */
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_send_msg_close_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->msg_ops->send_msg_close);

	/* icpu adt sends msg */
	__ctx.rsp_msg = true;
	ret = __adt->msg_ops->send_msg_close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, -ENODEV);
}

static void pablo_icpu_adt_v2_receive_msg_control_sensor_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	struct pablo_crta_bufmgr *bufmgr = &__bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };
	struct pablo_sensor_control_info *psci;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);

	/* open icpu adt */
	ret = __adt->ops->open(__adt, instance, &__sensor_itf, bufmgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* prepare msg */
	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, instance);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, PABLO_CRTA_BUF_SS_CTRL, 0, false, &buf_info);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_NE(test, 0, buf_info.dva);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf_info.kva);

	psci = (struct pablo_sensor_control_info *)buf_info.kva;
	psci->sensor_control_size = 0;
	psci->sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;

	__ctx.rx_data[0] = SET_MSG_VALUE(PABLO_IHC_CONTROL_SENSOR, MSG_COMMAND_ID_MASK, MSG_COMMAND_ID_SHIFT);
	__ctx.rx_data[1] = 0;
	__ctx.rx_data[2] = GET_DVA_HIGH(buf_info.dva);
	__ctx.rx_data[3] = GET_DVA_LOW(buf_info.dva);

	/* init sema */
	sema_init(&__ctx.smp_rsp, 0);

	/* kunit sends msg so that icpu adt could receive it. */
	__ctx.rsp_msg = false;
	__ctx.cl.rx_callback(&__ctx.cl, __ctx.rx_data, 4);

	/* kunit waits rsp msg until icpu adt sends it */
	ret = down_interruptible(&__ctx.smp_rsp);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* close bufmgr */
	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* close icpu adt */
	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_icpu_adt_v2_fw_err_handler_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	if (!__adt)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __adt->ops->close);

	ret = __adt->ops->open(__adt, instance, NULL, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	__ctx.rx_data[0] = SET_MSG_VALUE(PABLO_IHC_ERROR, MSG_COMMAND_ID_MASK, MSG_COMMAND_ID_SHIFT);
	__ctx.cl.rx_callback(&__ctx.cl, __ctx.rx_data, 1);

	ret = __adt->ops->close(__adt, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static struct kunit_case pablo_icpu_adt_v2_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_adt_v2_open_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_open_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_register_response_cb_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_register_response_cb_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_open_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_open_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_put_buf_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_put_buf_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_set_shared_buf_idx_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_set_shared_buf_idx_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_start_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_shot_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_shot_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_cstat_frame_start_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_cstat_frame_start_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_cstat_cdaf_end_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_cstat_cdaf_end_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_pdp_stat0_end_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_pdp_stat0_end_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_pdp_stat1_end_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_pdp_stat1_end_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_cstat_frame_end_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_cstat_frame_end_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_stop_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_stop_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_close_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_send_msg_close_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_receive_msg_control_sensor_kunit_test),
	KUNIT_CASE(pablo_icpu_adt_v2_fw_err_handler_kunit_test),
	{},
};

static int __debug_memlog_do_dump(struct memlog_obj *obj, int log_level)
{
	return 0;
}

const static struct pablo_memlog_operations pml_ops_mock = {
	.do_dump = __debug_memlog_do_dump,
};

static int __dss_expire_watchdog(void)
{
	return 0;
}

const static struct pablo_dss_operations pdss_ops_mock = {
	.expire_watchdog = __dss_expire_watchdog,
};

static int __send_rsp_message_for_adt_v2(void *data)
{
	__ctx.rx_data[0] |= SET_MSG_VALUE(PABLO_MESSAGE_RESPONSE, MSG_TYPE_MASK, MSG_TYPE_SHIFT);
	__ctx.cl.rx_callback(&__ctx.cl, __ctx.rx_data, 16);

	return 0;
}

static int __send_message_for_adt_v2(struct pablo_icpu_mbox_chan *chan, u32 *tx_data, u32 len)
{
	u32 i;
	struct kunit *test = __ctx.test;
	struct task_struct *task;

	if (chan->cl->tx_done)
		chan->cl->tx_done(chan->cl, tx_data, len);

	if (__ctx.rsp_msg) {
		for (i = 0; i < len; i++)
			__ctx.rx_data[i] = tx_data[i];
		task = kthread_run(__send_rsp_message_for_adt_v2, (void *)tx_data, "send_rsp_message");
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, task);
	}

	up(&__ctx.smp_rsp);

	return 0;
}

static struct icpu_mbox_chan_ops __test_tx_ops = {
	.send_data = __send_message_for_adt_v2,
	.startup = NULL,
	.shutdown = NULL,
};

static struct icpu_mbox_chan_ops __test_rx_ops = {
	.send_data = NULL,
	.startup = NULL,
	.shutdown = NULL,
};

static int pablo_icpu_adt_v2_kunit_test_init(struct kunit *test)
{
	int ret;
	struct is_debug *debug;

	/* Backup the original pablo_icpu_adt */
	__ctx.adt = pablo_get_icpu_adt();

	/* probe icpu_adt */
	pablo_icpu_adt_probe();
	__adt = pablo_get_icpu_adt();

	if (!__adt)
		return 0;

	/* init sema */
	sema_init(&__ctx.smp_rsp, 0);

	/* probe crta_bufmgr */
	pablo_crta_bufmgr_probe(&__bufmgr);

	/* prepare icpu */

	__pdata = pablo_icpu_test_set_platform_data(NULL);

	__itf = pablo_icpu_itf_api_get();

	__ctx.cl = pablo_icpu_test_itf_get_client();

	__ctx.tx_mbox.ops = &__test_tx_ops;
	__ctx.tx_mbox.debug_timeout = 500;
	__ctx.rx_mbox.ops = &__test_rx_ops;
	__ctx.rx_mbox.debug_timeout = 500;

	__ctx.tx_chan.cl = &__ctx.cl;
	__ctx.tx_chan.mbox = &__ctx.tx_mbox;

	__ctx.rx_chan.cl = &__ctx.cl;
	__ctx.rx_chan.mbox = &__ctx.rx_mbox;

	test->priv = &__ctx;
	__ctx.test = test;

	memcpy(&__test_pdata, __pdata, sizeof(struct icpu_platform_data));

	__test_pdata.mcuctl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __test_pdata.mcuctl_reg_base);

	__test_pdata.sysctrl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __test_pdata.sysctrl_reg_base);

	pablo_icpu_test_set_platform_data(&__test_pdata);

	pablo_itf_api_mode_change(ITF_API_MODE_TEST);

	ret = pablo_icpu_test_itf_open(__ctx.cl, &__ctx.tx_chan, &__ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* save debug */
	debug = is_debug_get();
	__ctx.pml_ops_b = debug->memlog.ops;
	debug->memlog.ops = &pml_ops_mock;
	__ctx.pdss_ops_b = debug->dss_ops;
	debug->dss_ops = &pdss_ops_mock;

	return 0;
}

static void pablo_icpu_adt_v2_kunit_test_exit(struct kunit *test)
{
	struct is_debug *debug;

	if (!__adt)
		return;

	pablo_icpu_test_itf_close();
	pablo_icpu_adt_remove();
	pablo_itf_api_mode_change(ITF_API_MODE_NORMAL);
	pablo_icpu_test_set_platform_data(__pdata);

	kunit_kfree(test, __test_pdata.mcuctl_reg_base);
	kunit_kfree(test, __test_pdata.sysctrl_reg_base);

	/* restore the pablo_icpu_adt */
	pablo_set_icpu_adt(__ctx.adt);

	/* restore debug */
	debug = is_debug_get();
	debug->memlog.ops = __ctx.pml_ops_b;
	debug->dss_ops = __ctx.pdss_ops_b;
}

struct kunit_suite pablo_icpu_adt_v2_kunit_test_suite = {
	.name = "pablo-icpu-adt-v2-kunit-test",
	.init = pablo_icpu_adt_v2_kunit_test_init,
	.exit = pablo_icpu_adt_v2_kunit_test_exit,
	.test_cases = pablo_icpu_adt_v2_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_adt_v2_kunit_test_suite);

MODULE_LICENSE("GPL");
