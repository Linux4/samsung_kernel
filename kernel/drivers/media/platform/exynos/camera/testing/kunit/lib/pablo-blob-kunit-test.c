// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "pablo-blob.h"
#include "is-video-config.h"

static struct pablo_kunit_blob_func *func;

static void pablo_blob_set_dump_lvn_name_kunit_test(struct kunit *test)
{
	int ret;
	char *argv[2] = { "1", "2" };

	/* Sub TC: argc < 2 */
	ret = func->set_dump_lvn_name(0, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: argc >= 2 */
	ret = func->set_dump_lvn_name(ARRAY_SIZE(argv), argv);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Restore dump_lvn_name */
	argv[1] = "\0";
	ret = func->set_dump_lvn_name(ARRAY_SIZE(argv), argv);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_blob_set_clear_pd_kunit_test(struct kunit *test)
{
	struct pablo_blob_pd *blob_pd =
		kunit_kzalloc(test, sizeof(struct pablo_blob_pd) * PD_BLOB_MAX, 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, blob_pd);

	blob_pd[0].blob.size = 1;
	func->set_clear_pd(blob_pd);
	KUNIT_EXPECT_EQ(test, blob_pd[0].blob.size, (unsigned long)0);

	/* Restore */
	kunit_kfree(test, blob_pd);
}

static void pablo_blob_set_dump_pd_kunit_test(struct kunit *test)
{
	int ret;
	char *argv[3] = { "1", "CSI", "30" };

	/* Sub TC: argc < 3 */
	ret = func->set_dump_pd(0, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: argc >= 3 */
	ret = func->set_dump_pd(ARRAY_SIZE(argv), argv);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: Invalid dump_pd_count */
	argv[2] = "A";
	ret = func->set_dump_pd(ARRAY_SIZE(argv), argv);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Restore dump_pd */
	argv[1] = "\0";
	argv[2] = "0";
	ret = func->set_dump_pd(ARRAY_SIZE(argv), argv);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_blob_set_kunit_test(struct kunit *test)
{
	int ret, i;
	char val[10];

	/* Sub TC: !argv */
	ret = func->set(NULL, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: argc < 1 */
	snprintf(val, sizeof(val), "%s", "");
	ret = func->set(val, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: Invalid act */
	snprintf(val, sizeof(val), "%s", "A");
	ret = func->set(val, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: switch/case statement */
	for (i = PABLO_BLOB_SET_RESERVED; i <= PABLO_BLOB_BUF_ACT_MAX; i++) {
		snprintf(val, sizeof(val), "%d", i);
		ret = func->set(val, NULL);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
}

static void pablo_blob_get_kunit_test(struct kunit *test)
{
	int ret;

	ret = func->get(NULL, NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_blob_dump_kunit_test(struct kunit *test)
{
	int ret;
	struct debugfs_blob_wrapper blob;
	u32 i, num_plane = 2;
	size_t size[2];
	ulong kva[2];

	/* Sub TC: !size_sum */
	ret = func->dump(&blob, NULL, NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: num_plane = 2 */
	for (i = 0; i < num_plane; i++) {
		size[i] = SZ_2M;
		kva[i] = (ulong)kunit_kzalloc(test, size[i], 0);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, (void *)kva[i]);
		memset((void *)kva[i], 0xCD, size[i]);
	}
	ret = func->dump(&blob, kva, size, num_plane);
	KUNIT_EXPECT_EQ(test, ret, 0);
	for (i = 0; i < num_plane; i++) {
		KUNIT_EXPECT_EQ(test, memcmp((void *)kva[i], blob.data, size[i]), 0);
		kunit_kfree(test, (void *)kva[i]);
	}
}

static void pablo_blob_lvn_name_to_blob_id_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_blob_lvn blob;

	/* Sub TC: -EINVAL */
	ret = func->lvn_name_to_blob_id(&blob, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: lvn_name == blob[blob_id].lvn_name */
	blob.lvn_name = "1";
	ret = func->lvn_name_to_blob_id(&blob, "1");
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_blob_lvn_dump_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_blob_lvn blob;
	struct is_sub_dma_buf *buf = kunit_kzalloc(test, sizeof(struct is_sub_dma_buf), 0);
	char *argv[2];

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	/* Sub TC: FIMC_BUG_VOID(!blob) */
	ret = func->lvn_dump(NULL, buf);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = func->lvn_dump(&blob, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: mismatch lvn_name */
	ret = func->lvn_dump(&blob, buf);
	KUNIT_EXPECT_EQ(test, ret, -ENOENT);

	/* Sub TC: blob_id < 0 */
	argv[1] = (char *)vn_name[buf->vid];
	func->set_dump_lvn_name(ARRAY_SIZE(argv), argv);
	ret = func->lvn_dump(&blob, buf);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: failed pablo_blob_dump */
	argv[1] = (char *)vn_name[buf->vid];
	func->set_dump_lvn_name(ARRAY_SIZE(argv), argv);
	buf->vid = 0;
	blob.lvn_name = vn_name[buf->vid];
	ret = func->lvn_dump(&blob, buf);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: success */
	/* TODO: It needs kva */

	/* Restore */
	kunit_kfree(test, buf);
}

static void pablo_blob_lvn_probe_kunit_test(struct kunit *test)
{
	int ret;
	struct dentry *root;
	struct pablo_blob_lvn *blob = NULL;
	const char *blob_lvn_name[] = {
		"1",
		"2",
	};

	root = debugfs_create_dir("KUNIT-blob-lvn", NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, root);

	/* Sub TC: !root */
	ret = func->lvn_probe(NULL, NULL, blob_lvn_name, ARRAY_SIZE(blob_lvn_name));
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: ZERO_OR_NULL_PTR(blob_tmp) */
	ret = func->lvn_probe(root, &blob, NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, -ENOMEM);

	/* Sub TC: success */
	ret = func->lvn_probe(root, &blob, blob_lvn_name, ARRAY_SIZE(blob_lvn_name));
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_NE(test, (void *)root, NULL);
	KUNIT_EXPECT_PTR_NE(test, (void *)blob, NULL);
	KUNIT_EXPECT_STREQ(test, blob[0].lvn_name, "1");
	KUNIT_EXPECT_STREQ(test, blob[1].lvn_name, "2");
	KUNIT_EXPECT_PTR_NE(test, (void *)blob[0].dentry, NULL);
	KUNIT_EXPECT_PTR_NE(test, (void *)blob[1].dentry, NULL);
	func->lvn_remove(&blob, ARRAY_SIZE(blob_lvn_name));
	KUNIT_EXPECT_NULL(test, (void *)blob);

	/* Restore */
	debugfs_remove(root);
}

static void pablo_blob_debugfs_create_dir_kunit_test(struct kunit *test)
{
	int ret;
	struct dentry *root;

	ret = func->debugfs_create_dir("", &root);
	KUNIT_EXPECT_EQ(test, ret, -ENOMEM);

	ret = func->debugfs_create_dir("KUNIT", &root);
	KUNIT_EXPECT_EQ(test, ret, 0);

	func->debugfs_remove(root);
}

static void pablo_blob_wq_func_dump_kunit_test(struct kunit *test)
{
	struct pablo_blob_pd *blob_pd = kunit_kzalloc(test, sizeof(struct pablo_blob_pd), 0);
	struct is_frame *frame = kunit_kzalloc(test, sizeof(struct is_frame), 0);
	size_t t1_size = SZ_4, t2_size = SZ_8;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, blob_pd);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

	/* Sub TC: failed pablo_blob_dump() */
	frame->planes = 0;
	frame->size[0] = 0;
	blob_pd->frame = frame;
	blob_pd->blob.size = t1_size;
	func->wq_func_pd_dump(&blob_pd->work);
	KUNIT_EXPECT_EQ(test, blob_pd->blob.size, t1_size);

	/* Sub TC: success pablo_blob_dump() */
	frame->planes = 1;
	frame->size[0] = t2_size;
	frame->kvaddr_buffer[0] = (ulong)kunit_kzalloc(test, frame->size[0], 0);
	blob_pd->frame = frame;
	func->wq_func_pd_dump(&blob_pd->work);
	KUNIT_EXPECT_EQ(test, blob_pd->blob.size, (unsigned long)frame->size[0]);

	/* Restore */
	kunit_kfree(test, (void *)frame->kvaddr_buffer[0]);
	kunit_kfree(test, frame);
	kunit_kfree(test, blob_pd);
}

static void wq_func_pd_dump_kunit_test(struct work_struct *data)
{
	/* dummy work queue */
	pr_info("%s\n", __func__);
}

static void pablo_blob_pd_dump_kunit_test(struct kunit *test)
{
	int ret, i;
	struct pablo_blob_pd *blob_pd =
		kunit_kzalloc(test, sizeof(struct pablo_blob_pd) * PD_BLOB_MAX, 0);
	struct is_frame *frame = kunit_kzalloc(test, sizeof(struct is_frame), 0);
	char *argv[3] = { "1", "CSI", "1" };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, blob_pd);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

	for (i = 0; i < PD_BLOB_MAX; i++)
		INIT_WORK(&blob_pd[i].work, wq_func_pd_dump_kunit_test);

	/* Sub TC: -ENOENT */
	ret = func->pd_dump(blob_pd, frame, "CSI%d_F%d", 0, frame->fcount);
	KUNIT_EXPECT_EQ(test, ret, -ENOENT);

	ret = func->set_dump_pd(ARRAY_SIZE(argv), argv);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: success dump */
	ret = func->pd_dump(blob_pd, frame, "CSI%d_F%d", 0, frame->fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: blob_id >= dump_pd_count || blob_id >= PD_BLOB_MAX */
	ret = func->pd_dump(blob_pd, frame, "CSI%d_F%d", 0, frame->fcount);
	KUNIT_EXPECT_EQ(test, ret, -ENFILE);

	/* Restore */
	func->set_clear_pd(blob_pd);
	kunit_kfree(test, frame);
	kunit_kfree(test, blob_pd);
}

static void pablo_blob_pd_probe_kunit_test(struct kunit *test)
{
	int ret;
	struct dentry *root;
	struct pablo_blob_pd *blob = NULL;

	root = debugfs_create_dir("KUNIT-blob-pd", NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, root);

	/* Sub TC: IS_ERR(root_tmp) */
	ret = func->pd_probe(NULL, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: success */
	ret = func->pd_probe(root, &blob);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_NE(test, (void *)blob, NULL);
	KUNIT_EXPECT_PTR_NE(test, (void *)blob[0].dentry, NULL);
	func->pd_remove(&blob);
	KUNIT_EXPECT_NULL(test, (void *)blob);

	debugfs_remove(root);
}

static int pablo_blob_kunit_test_init(struct kunit *test)
{
	func = pablo_kunit_get_blob_func();

	return 0;
}

static void pablo_blob_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_blob_kunit_test_cases[] = {
	KUNIT_CASE(pablo_blob_set_dump_lvn_name_kunit_test),
	KUNIT_CASE(pablo_blob_set_clear_pd_kunit_test),
	KUNIT_CASE(pablo_blob_set_dump_pd_kunit_test),
	KUNIT_CASE(pablo_blob_set_kunit_test),
	KUNIT_CASE(pablo_blob_get_kunit_test),
	KUNIT_CASE(pablo_blob_dump_kunit_test),
	KUNIT_CASE(pablo_blob_lvn_name_to_blob_id_kunit_test),
	KUNIT_CASE(pablo_blob_lvn_dump_kunit_test),
	KUNIT_CASE(pablo_blob_lvn_probe_kunit_test),
	KUNIT_CASE(pablo_blob_wq_func_dump_kunit_test),
	KUNIT_CASE(pablo_blob_pd_dump_kunit_test),
	KUNIT_CASE(pablo_blob_pd_probe_kunit_test),
	KUNIT_CASE(pablo_blob_debugfs_create_dir_kunit_test),
	{},
};

struct kunit_suite pablo_blob_kunit_test_suite = {
	.name = "pablo-blob-kunit-test",
	.init = pablo_blob_kunit_test_init,
	.exit = pablo_blob_kunit_test_exit,
	.test_cases = pablo_blob_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_blob_kunit_test_suite);

MODULE_LICENSE("GPL v2");
