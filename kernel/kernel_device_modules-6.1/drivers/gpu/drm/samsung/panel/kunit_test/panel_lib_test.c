// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>

#include "panel_lib.h"

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct kunit *)`. `struct kunit` is a context object that stores
 * information about the current test.
 */

#ifdef CONFIG_UML

/* NOTE: UML TC */
static void panel_lib_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_lib_test_rdinfo_create_should_allocate_with_data(struct kunit *test)
{
	struct rdinfo *m;
	u8 buf[3] = { 0, };

	m = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&m->base), (u32)CMD_TYPE_RX_PACKET);
	KUNIT_EXPECT_EQ(test, get_rdinfo_type(m), (u32)DSI_PKT_TYPE_RD);
	KUNIT_EXPECT_STREQ(test, get_rdinfo_name(m), "test_rd_info");
	KUNIT_EXPECT_EQ(test, m->addr, (u32)0x04);
	KUNIT_EXPECT_EQ(test, m->offset, (u32)0);
	KUNIT_EXPECT_EQ(test, m->len, (u32)3);
	KUNIT_EXPECT_NE(test, (long long)m->data, (long long)buf);
	KUNIT_EXPECT_TRUE(test, !memcmp(m->data, buf, m->len));
	panel_lib_rdinfo_destroy(m);
}

static void panel_lib_test_rdinfo_create_should_allocate_with_no_data(struct kunit *test)
{
	struct rdinfo *m;

	m = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, NULL);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&m->base), (u32)CMD_TYPE_RX_PACKET);
	KUNIT_EXPECT_EQ(test, get_rdinfo_type(m), (u32)DSI_PKT_TYPE_RD);
	KUNIT_EXPECT_STREQ(test, get_rdinfo_name(m), "test_rd_info");
	KUNIT_EXPECT_EQ(test, m->addr, (u32)0x04);
	KUNIT_EXPECT_EQ(test, m->offset, (u32)0);
	KUNIT_EXPECT_EQ(test, m->len, (u32)3);
	KUNIT_EXPECT_TRUE(test, !m->data);

	panel_lib_rdinfo_destroy(m);
}

static void panel_lib_test_rdinfo_create_should_return_null_with_zero_len(struct kunit *test)
{
	struct rdinfo *m;
	u8 buf[3] = { 0, };

	m = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 3, 0, buf);
	KUNIT_EXPECT_TRUE(test, !m);
}

static void panel_lib_test_rdinfo_create_should_return_null_with_no_name(struct kunit *test)
{
	struct rdinfo *m;
	u8 buf[3] = { 0, };

	m = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, NULL, 0x04, 3, 3, buf);
	KUNIT_EXPECT_TRUE(test, !m);
}

static void panel_lib_test_rdinfo_create_should_return_null_with_zero_addr(struct kunit *test)
{
	struct rdinfo *m;
	u8 buf[3] = { 0, };

	m = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0, 3, 3, buf);
	KUNIT_EXPECT_TRUE(test, !m);
}

static void panel_lib_test_rdinfo_create_should_return_null_with_invalid_type(struct kunit *test)
{
	struct rdinfo *m;
	u8 buf[3] = { 0, };

	m = panel_lib_rdinfo_create(DSI_PKT_TYPE_WR_MEM, "test_rd_info", 0x04, 3, 3, buf);
	KUNIT_EXPECT_TRUE(test, !m);

	m = panel_lib_rdinfo_create(SPI_PKT_TYPE_WR, "test_rd_info", 0x04, 3, 3, buf);
	KUNIT_EXPECT_TRUE(test, !m);
}

static void panel_lib_test_rdinfo_destroy_should_not_occur_exception_with_null_pointer(struct kunit *test)
{
	panel_lib_rdinfo_destroy(NULL);
}

static void panel_lib_test_rdinfo_alloc_buffer_success(struct kunit *test)
{
	struct rdinfo *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	m->len = 3;
	KUNIT_EXPECT_TRUE(test, panel_lib_rdinfo_alloc_buffer(m) == 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m->data);
	panel_lib_rdinfo_free_buffer(m);
}

static void panel_lib_test_rdinfo_alloc_buffer_should_return_err_already_have_buffer(struct kunit *test)
{
	struct rdinfo *m;
	u8 buf[3] = { 0, };

	m = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);

	KUNIT_EXPECT_EQ(test, panel_lib_rdinfo_alloc_buffer(m), -EINVAL);
	panel_lib_rdinfo_destroy(m);
}

static void panel_lib_test_rdinfo_alloc_buffer_should_return_err_with_null_pointer_or_len_zero(struct kunit *test)
{
	struct rdinfo *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	KUNIT_EXPECT_EQ(test, panel_lib_rdinfo_alloc_buffer(NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_lib_rdinfo_alloc_buffer(m), -EINVAL);
}

static void panel_lib_test_rdinfo_free_buffer_success(struct kunit *test)
{
	struct rdinfo *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	m->len = 3;
	panel_lib_rdinfo_alloc_buffer(m);
	panel_lib_rdinfo_free_buffer(m);

	KUNIT_EXPECT_TRUE(test, !m->data);
}

static void panel_lib_test_rdinfo_copy_success(struct kunit *test)
{
	struct rdinfo *src_rdinfo, *dst_rdinfo;
	u8 buf[3] = { 0, };

	src_rdinfo = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);

	dst_rdinfo = kunit_kzalloc(test, sizeof(*dst_rdinfo), GFP_KERNEL);

	KUNIT_EXPECT_TRUE(test, panel_lib_rdinfo_copy(dst_rdinfo, src_rdinfo) == 0);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&src_rdinfo->base), get_pnobj_cmd_type(&dst_rdinfo->base));
	KUNIT_EXPECT_EQ(test, get_rdinfo_type(src_rdinfo), get_rdinfo_type(dst_rdinfo));
	KUNIT_EXPECT_STREQ(test, get_rdinfo_name(src_rdinfo), get_rdinfo_name(dst_rdinfo));
	KUNIT_EXPECT_EQ(test, src_rdinfo->addr, dst_rdinfo->addr);
	KUNIT_EXPECT_EQ(test, src_rdinfo->offset, dst_rdinfo->offset);
	KUNIT_EXPECT_EQ(test, src_rdinfo->len, dst_rdinfo->len);
	KUNIT_EXPECT_PTR_NE(test, src_rdinfo->data, dst_rdinfo->data);
	KUNIT_EXPECT_TRUE(test, !memcmp(src_rdinfo->data, dst_rdinfo->data, src_rdinfo->len));

	panel_lib_rdinfo_destroy(src_rdinfo);
}

static void panel_lib_test_rdinfo_copy_should_return_err_with_zero_len(struct kunit *test)
{
	struct rdinfo *src_rdinfo, *dst_rdinfo;
	u8 buf[3] = { 0, };

	src_rdinfo = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	src_rdinfo->len = 0;

	dst_rdinfo = kunit_kzalloc(test, sizeof(*dst_rdinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_rdinfo_copy(dst_rdinfo, src_rdinfo), -EINVAL);

	panel_lib_rdinfo_destroy(src_rdinfo);
}

static void panel_lib_test_rdinfo_copy_should_return_err_with_no_name(struct kunit *test)
{
	struct rdinfo *src_rdinfo, *dst_rdinfo;
	u8 buf[3] = { 0, };

	src_rdinfo = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	set_pnobj_name(&src_rdinfo->base, NULL);

	dst_rdinfo = kunit_kzalloc(test, sizeof(*dst_rdinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_rdinfo_copy(dst_rdinfo, src_rdinfo), -EINVAL);

	panel_lib_rdinfo_destroy(src_rdinfo);
}

static void panel_lib_test_rdinfo_copy_should_return_err_with_zero_addr(struct kunit *test)
{
	struct rdinfo *src_rdinfo, *dst_rdinfo;
	u8 buf[3] = { 0, };

	src_rdinfo = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	src_rdinfo->addr = 0;

	dst_rdinfo = kunit_kzalloc(test, sizeof(*dst_rdinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_rdinfo_copy(dst_rdinfo, src_rdinfo), -EINVAL);

	panel_lib_rdinfo_destroy(src_rdinfo);
}

static void panel_lib_test_rdinfo_copy_should_return_err_with_invalid_type(struct kunit *test)
{
	struct rdinfo *src_rdinfo, *dst_rdinfo;
	u8 buf[3] = { 0, };

	src_rdinfo = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	set_pnobj_cmd_type(&src_rdinfo->base, CMD_TYPE_NONE);

	dst_rdinfo = kunit_kzalloc(test, sizeof(*dst_rdinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_rdinfo_copy(dst_rdinfo, src_rdinfo), -EINVAL);

	panel_lib_rdinfo_destroy(src_rdinfo);
}

static void panel_lib_test_rdinfo_free_buffer_should_not_occur_exception_with_null_pointer(struct kunit *test)
{
	panel_lib_rdinfo_free_buffer(NULL);
}

static void panel_lib_test_res_update_info_create_should_allocate_with_offset_zero_and_rdinfo(struct kunit *test)
{
	struct rdinfo *rditbl;
	struct res_update_info *resui;
	u8 buf[3] = { 0, };

	rditbl = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rditbl);

	resui = panel_lib_res_update_info_create(0, rditbl);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, resui);
	KUNIT_EXPECT_PTR_EQ(test, resui->rditbl, rditbl);

	panel_lib_res_update_info_destroy(resui);
	panel_lib_rdinfo_destroy(rditbl);
}

static void panel_lib_test_res_update_info_create_should_return_null_with_no_rdinfo(struct kunit *test)
{
	struct res_update_info *resui;

	resui = panel_lib_res_update_info_create(0, NULL);
	KUNIT_EXPECT_TRUE(test, !resui);
}

static void panel_lib_test_res_update_info_destroy_should_not_occur_exception_with_null_pointer(struct kunit *test)
{
	panel_lib_res_update_info_destroy(NULL);
}

static void panel_lib_test_res_update_info_copy_success(struct kunit *test)
{
	struct res_update_info *src_resui, *dst_resui;
	struct rdinfo *rdinfo;
	u8 buf[3] = { 0, };

	rdinfo = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	src_resui = panel_lib_res_update_info_create(0x1, rdinfo);
	dst_resui = kunit_kzalloc(test, sizeof(*dst_resui), GFP_KERNEL);

	panel_lib_res_update_info_copy(dst_resui, src_resui);

	KUNIT_EXPECT_EQ(test, dst_resui->offset, src_resui->offset);
	KUNIT_EXPECT_PTR_EQ(test, dst_resui->rditbl, src_resui->rditbl);
	KUNIT_EXPECT_PTR_NE(test, dst_resui, src_resui);

	panel_lib_res_update_info_destroy(src_resui);
	panel_lib_rdinfo_destroy(rdinfo);
}

static void panel_lib_test_res_update_info_copy_should_return_err_with_same_dst_src(struct kunit *test)
{
	struct res_update_info *resui;
	struct rdinfo *rdinfo;
	u8 buf[3] = { 0, };

	rdinfo = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	resui = panel_lib_res_update_info_create(0x1, rdinfo);

	KUNIT_EXPECT_EQ(test, panel_lib_res_update_info_copy(resui, resui), -EINVAL);

	panel_lib_res_update_info_destroy(resui);
	panel_lib_rdinfo_destroy(rdinfo);
}

static void panel_lib_test_res_update_info_copy_should_return_err_with_null(struct kunit *test)
{
	struct res_update_info *resui;
	struct rdinfo *rdinfo;
	u8 buf[3] = { 0, };

	rdinfo = panel_lib_rdinfo_create(DSI_PKT_TYPE_RD, "test_rd_info", 0x04, 0, 3, buf);
	resui = panel_lib_res_update_info_create(0x1, rdinfo);

	KUNIT_EXPECT_EQ(test, panel_lib_res_update_info_copy(resui, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_lib_res_update_info_copy(NULL, resui), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_lib_res_update_info_copy(NULL, NULL), -EINVAL);

	panel_lib_res_update_info_destroy(resui);
	panel_lib_rdinfo_destroy(rdinfo);
}

static void panel_lib_test_resinfo_create_success_with_arguments(struct kunit *test)
{
	struct resinfo *res;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	res = panel_lib_resinfo_create("test_res", buf, 3, &resui, 1);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&res->base), (u32)CMD_TYPE_RES);
	KUNIT_EXPECT_STREQ(test, get_resource_name(res), "test_res");
	KUNIT_EXPECT_EQ(test, res->state, RES_UNINITIALIZED);
	KUNIT_EXPECT_EQ(test, res->dlen, (u32)3);
	KUNIT_EXPECT_PTR_EQ(test, res->resui, &resui);
	KUNIT_EXPECT_EQ(test, res->nr_resui, (u32)1);
	KUNIT_EXPECT_NE(test, (long long)res->data, (long long)buf);
	KUNIT_EXPECT_TRUE(test, !memcmp(res->data, buf, res->dlen));

	panel_lib_resinfo_destroy(res);
}

static void panel_lib_test_resinfo_create_should_allocate_with_no_data(struct kunit *test)
{
	struct resinfo *res;
	struct res_update_info resui;

	res = panel_lib_resinfo_create("test_res", NULL, 3, &resui, 1);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, res);
	panel_lib_resinfo_destroy(res);
}

static void panel_lib_test_resinfo_create_should_return_null_with_no_resui(struct kunit *test)
{
	struct resinfo *res;
	u8 buf[3] = { 0, };

	res = panel_lib_resinfo_create("test_res", buf, 3, NULL, 1);
	KUNIT_EXPECT_TRUE(test, !res);
}

static void panel_lib_test_resinfo_create_should_return_null_with_ng_arguments(struct kunit *test)
{
	struct resinfo *res;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	res = panel_lib_resinfo_create("test_res", buf, 0, &resui, 1);
	KUNIT_EXPECT_TRUE(test, !res);
}

static void panel_lib_test_resinfo_destroy_should_not_occur_exception_with_null_pointer(struct kunit *test)
{
	panel_lib_resinfo_destroy(NULL);
}

static void panel_lib_test_resinfo_copy_success(struct kunit *test)
{
	struct resinfo *src_resinfo, *dst_resinfo;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	src_resinfo = panel_lib_resinfo_create("test_res", buf, 3, &resui, 1);
	dst_resinfo = kunit_kzalloc(test, sizeof(*dst_resinfo), GFP_KERNEL);

	panel_lib_resinfo_copy(dst_resinfo, src_resinfo);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&src_resinfo->base), get_pnobj_cmd_type(&dst_resinfo->base));
	KUNIT_EXPECT_STREQ(test, get_resource_name(src_resinfo), get_resource_name(dst_resinfo));
	KUNIT_EXPECT_EQ(test, src_resinfo->state, dst_resinfo->state);
	KUNIT_EXPECT_EQ(test, src_resinfo->dlen, dst_resinfo->dlen);
	KUNIT_EXPECT_PTR_EQ(test, src_resinfo->resui, dst_resinfo->resui);
	KUNIT_EXPECT_EQ(test, src_resinfo->nr_resui, dst_resinfo->nr_resui);
	KUNIT_EXPECT_PTR_NE(test, src_resinfo->data, dst_resinfo->data);
	KUNIT_EXPECT_TRUE(test, !memcmp(src_resinfo->data, dst_resinfo->data, src_resinfo->dlen));

	panel_lib_resinfo_destroy(src_resinfo);
}

static void panel_lib_test_resinfo_copy_success_with_no_data(struct kunit *test)
{
	struct resinfo *src_resinfo, *dst_resinfo;
	struct res_update_info resui;

	src_resinfo = panel_lib_resinfo_create("test_res", NULL, 3, &resui, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, src_resinfo);
	dst_resinfo = kunit_kzalloc(test, sizeof(*dst_resinfo), GFP_KERNEL);

	KUNIT_EXPECT_TRUE(test, panel_lib_resinfo_copy(dst_resinfo, src_resinfo) == 0);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&src_resinfo->base), get_pnobj_cmd_type(&dst_resinfo->base));
	KUNIT_EXPECT_STREQ(test, get_resource_name(src_resinfo), get_resource_name(dst_resinfo));
	KUNIT_EXPECT_EQ(test, src_resinfo->state, dst_resinfo->state);
	KUNIT_EXPECT_EQ(test, src_resinfo->dlen, dst_resinfo->dlen);
	KUNIT_EXPECT_PTR_EQ(test, src_resinfo->resui, dst_resinfo->resui);
	KUNIT_EXPECT_EQ(test, src_resinfo->nr_resui, dst_resinfo->nr_resui);
	KUNIT_EXPECT_PTR_EQ(test, src_resinfo->data, dst_resinfo->data);

	panel_lib_resinfo_destroy(src_resinfo);
}

static void panel_lib_test_resinfo_copy_should_return_err_with_null(struct kunit *test)
{
	struct resinfo *src_resinfo, *dst_resinfo;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	src_resinfo = panel_lib_resinfo_create("test_res", buf, 3, &resui, 1);
	dst_resinfo = kunit_kzalloc(test, sizeof(*dst_resinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_copy(NULL, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_copy(dst_resinfo, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_copy(NULL, src_resinfo), -EINVAL);

	panel_lib_resinfo_destroy(src_resinfo);
}

static void panel_lib_test_resinfo_copy_should_return_err_with_zero_len(struct kunit *test)
{
	struct resinfo *src_resinfo, *dst_resinfo;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	src_resinfo = panel_lib_resinfo_create("test_res", buf, 3, &resui, 1);
	src_resinfo->dlen = 0;

	dst_resinfo = kunit_kzalloc(test, sizeof(*dst_resinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_copy(dst_resinfo, src_resinfo), -EINVAL);

	panel_lib_resinfo_destroy(src_resinfo);
}

static void panel_lib_test_resinfo_copy_should_return_err_with_ng_type(struct kunit *test)
{
	struct resinfo *src_resinfo, *dst_resinfo;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	src_resinfo = panel_lib_resinfo_create("test_res", buf, 3, &resui, 1);
	set_pnobj_cmd_type(&src_resinfo->base, CMD_TYPE_NONE);

	dst_resinfo = kunit_kzalloc(test, sizeof(*dst_resinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_copy(dst_resinfo, src_resinfo), -EINVAL);

	panel_lib_resinfo_destroy(src_resinfo);
}

static void panel_lib_test_resinfo_copy_should_return_err_with_no_name(struct kunit *test)
{
	struct resinfo *src_resinfo, *dst_resinfo;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	src_resinfo = panel_lib_resinfo_create("test_res", buf, 3, &resui, 1);
	set_pnobj_name(&src_resinfo->base, NULL);

	dst_resinfo = kunit_kzalloc(test, sizeof(*dst_resinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_copy(dst_resinfo, src_resinfo), -EINVAL);

	panel_lib_resinfo_destroy(src_resinfo);
}

static void panel_lib_test_resinfo_copy_should_return_err_with_ng_state(struct kunit *test)
{
	struct resinfo *src_resinfo, *dst_resinfo;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	src_resinfo = panel_lib_resinfo_create("test_res", buf, 3, &resui, 1);
	src_resinfo->state = 9999;

	dst_resinfo = kunit_kzalloc(test, sizeof(*dst_resinfo), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_copy(dst_resinfo, src_resinfo), -EINVAL);

	panel_lib_resinfo_destroy(src_resinfo);
}

static void panel_lib_test_resinfo_alloc_buffer_success(struct kunit *test)
{
	struct resinfo *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	m->dlen = 3;
	KUNIT_EXPECT_TRUE(test, panel_lib_resinfo_alloc_buffer(m) == 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m->data);
	panel_lib_resinfo_free_buffer(m);
}

static void panel_lib_test_resinfo_alloc_buffer_should_return_err_already_have_buffer(struct kunit *test)
{
	struct resinfo *m;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	m = panel_lib_resinfo_create("test_res", buf, 3, &resui, 1);

	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_alloc_buffer(m), -EINVAL);
	panel_lib_resinfo_destroy(m);
}

static void panel_lib_test_resinfo_alloc_buffer_should_return_err_with_null_pointer_or_len_zero(struct kunit *test)
{
	struct resinfo *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_alloc_buffer(NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_lib_resinfo_alloc_buffer(m), -EINVAL);
}

static void panel_lib_test_resinfo_free_buffer_success(struct kunit *test)
{
	struct resinfo *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	m->dlen = 3;
	panel_lib_resinfo_alloc_buffer(m);
	panel_lib_resinfo_free_buffer(m);

	KUNIT_EXPECT_TRUE(test, !m->data);
}

static void panel_lib_test_resinfo_free_buffer_should_not_occur_exception_with_null_pointer(struct kunit *test)
{
	panel_lib_resinfo_free_buffer(NULL);
}
#else
/* NOTE: On device TC */
static void panel_lib_test_foo(struct kunit *test)
{
	/*
	 * This is an EXPECTATION; it is how KUnit tests things. When you want
	 * to test a piece of code, you set some expectations about what the
	 * code should do. KUnit then runs the test and verifies that the code's
	 * behavior matched what was expected.
	 */
	KUNIT_EXPECT_EQ(test, 1, 2); // Obvious failure.
}
#endif


/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_lib_test_init(struct kunit *test)
{
	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_lib_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_lib_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#ifdef CONFIG_UML
	/* NOTE: UML TC */
	KUNIT_CASE(panel_lib_test_test),

	/* panel_lib_rdinfo_copy */
	KUNIT_CASE(panel_lib_test_rdinfo_copy_success),
	KUNIT_CASE(panel_lib_test_rdinfo_copy_should_return_err_with_zero_len),
	KUNIT_CASE(panel_lib_test_rdinfo_copy_should_return_err_with_no_name),
	KUNIT_CASE(panel_lib_test_rdinfo_copy_should_return_err_with_zero_addr),
	KUNIT_CASE(panel_lib_test_rdinfo_copy_should_return_err_with_invalid_type),
	/* panel_lib_rdinfo_create */
	KUNIT_CASE(panel_lib_test_rdinfo_create_should_allocate_with_data),
	KUNIT_CASE(panel_lib_test_rdinfo_create_should_allocate_with_no_data),
	KUNIT_CASE(panel_lib_test_rdinfo_create_should_return_null_with_zero_len),
	KUNIT_CASE(panel_lib_test_rdinfo_create_should_return_null_with_no_name),
	KUNIT_CASE(panel_lib_test_rdinfo_create_should_return_null_with_zero_addr),
	KUNIT_CASE(panel_lib_test_rdinfo_create_should_return_null_with_invalid_type),
	/* panel_lib_rdinfo_destroy */
	KUNIT_CASE(panel_lib_test_rdinfo_destroy_should_not_occur_exception_with_null_pointer),
	/* panel_lib_rdinfo_alloc_buffer */
	KUNIT_CASE(panel_lib_test_rdinfo_alloc_buffer_success),
	KUNIT_CASE(panel_lib_test_rdinfo_alloc_buffer_should_return_err_already_have_buffer),
	KUNIT_CASE(panel_lib_test_rdinfo_alloc_buffer_should_return_err_with_null_pointer_or_len_zero),
	/* panel_lib_rdinfo_free_buffer */
	KUNIT_CASE(panel_lib_test_rdinfo_free_buffer_success),
	KUNIT_CASE(panel_lib_test_rdinfo_free_buffer_should_not_occur_exception_with_null_pointer),

	/* panel_lib_res_update_info_create */
	KUNIT_CASE(panel_lib_test_res_update_info_create_should_allocate_with_offset_zero_and_rdinfo),
	KUNIT_CASE(panel_lib_test_res_update_info_create_should_return_null_with_no_rdinfo),
	/* panel_lib_res_update_info_destroy */
	KUNIT_CASE(panel_lib_test_res_update_info_destroy_should_not_occur_exception_with_null_pointer),
	/* panel_lib_res_update_info_copy */
	KUNIT_CASE(panel_lib_test_res_update_info_copy_success),
	KUNIT_CASE(panel_lib_test_res_update_info_copy_should_return_err_with_same_dst_src),
	KUNIT_CASE(panel_lib_test_res_update_info_copy_should_return_err_with_null),

	/* panel_lib_resinfo_create */
	KUNIT_CASE(panel_lib_test_resinfo_create_success_with_arguments),
	KUNIT_CASE(panel_lib_test_resinfo_create_should_allocate_with_no_data),
	KUNIT_CASE(panel_lib_test_resinfo_create_should_return_null_with_no_resui),

	KUNIT_CASE(panel_lib_test_resinfo_create_should_return_null_with_ng_arguments),
	/* panel_lib_resinfo_destroy */
	KUNIT_CASE(panel_lib_test_resinfo_destroy_should_not_occur_exception_with_null_pointer),
	/* panel_lib_resinfo_copy */
	KUNIT_CASE(panel_lib_test_resinfo_copy_success),
	KUNIT_CASE(panel_lib_test_resinfo_copy_success_with_no_data),
	KUNIT_CASE(panel_lib_test_resinfo_copy_should_return_err_with_null),
	KUNIT_CASE(panel_lib_test_resinfo_copy_should_return_err_with_zero_len),
	KUNIT_CASE(panel_lib_test_resinfo_copy_should_return_err_with_ng_type),
	KUNIT_CASE(panel_lib_test_resinfo_copy_should_return_err_with_no_name),
	KUNIT_CASE(panel_lib_test_resinfo_copy_should_return_err_with_ng_state),
	/* panel_lib_resinfo_alloc_buffer */
	KUNIT_CASE(panel_lib_test_resinfo_alloc_buffer_success),
	KUNIT_CASE(panel_lib_test_resinfo_alloc_buffer_should_return_err_already_have_buffer),
	KUNIT_CASE(panel_lib_test_resinfo_alloc_buffer_should_return_err_with_null_pointer_or_len_zero),
	/* panel_lib_resinfo_free_buffer */
	KUNIT_CASE(panel_lib_test_resinfo_free_buffer_success),
	KUNIT_CASE(panel_lib_test_resinfo_free_buffer_should_not_occur_exception_with_null_pointer),
#else
	/* NOTE: On device TC */
	KUNIT_CASE(panel_lib_test_foo),
#endif
	{},
};

/*
 * This defines a suite or grouping of tests.
 *
 * Test cases are defined as belonging to the suite by adding them to
 * `test_cases`.
 *
 * Often it is desirable to run some function which will set up things which
 * will be used by every test; this is accomplished with an `init` function
 * which runs before each test case is invoked. Similarly, an `exit` function
 * may be specified which runs after every test case and can be used to for
 * cleanup. For clarity, running tests in a test module would behave as follows:
 *
 * module.init(test);
 * module.test_case[0](test);
 * module.exit(test);
 * module.init(test);
 * module.test_case[1](test);
 * module.exit(test);
 * ...;
 */
static struct kunit_suite panel_lib_test_module = {
	.name = "lib_test_test",
	.init = panel_lib_test_init,
	.exit = panel_lib_test_exit,
	.test_cases = panel_lib_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_lib_test_module);

MODULE_LICENSE("GPL v2");
