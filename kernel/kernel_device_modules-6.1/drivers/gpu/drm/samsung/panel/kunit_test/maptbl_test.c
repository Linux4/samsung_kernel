// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "maptbl.h"
#include "panel_function.h"

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

static int fake_callback_return_error(struct maptbl *tbl)
{
	return -EINVAL;
}

static int fake_callback_return_zero(struct maptbl *tbl)
{
	return 0;
}

int fake_return_value;
static int fake_callback_return_value(struct maptbl *tbl)
{
	return fake_return_value;
}

static void fake_copy_callback_void_return(struct maptbl *tbl, unsigned char *dst) {}

static DEFINE_PNOBJ_FUNC(fake_callback_return_error, fake_callback_return_error);
static DEFINE_PNOBJ_FUNC(fake_callback_return_zero, fake_callback_return_zero);
static DEFINE_PNOBJ_FUNC(fake_callback_return_value, fake_callback_return_value);
static DEFINE_PNOBJ_FUNC(fake_copy_callback_void_return, fake_copy_callback_void_return);

/* NOTE: UML TC */
static void maptbl_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

/* maptbl_alloc_buffer() test */
static void maptbl_test_maptbl_alloc_buffer_should_not_allocate_with_passing_null_pointer_or_zero(struct kunit *test)
{
	struct maptbl *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	KUNIT_EXPECT_EQ(test, maptbl_alloc_buffer(NULL, 256), -EINVAL);
	KUNIT_EXPECT_TRUE(test, !m->arr);

	KUNIT_EXPECT_EQ(test, maptbl_alloc_buffer(m, 0), -EINVAL);
	KUNIT_EXPECT_TRUE(test, !m->arr);
}

static void maptbl_test_maptbl_alloc_buffer_success(struct kunit *test)
{
	struct maptbl *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	KUNIT_EXPECT_EQ(test, maptbl_alloc_buffer(m, 256), 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m->arr);

	maptbl_free_buffer(m);
}

/* maptbl_free_buffer() test */
static void maptbl_test_maptbl_free_buffer_should_not_occur_exception_with_passing_null_pointer(struct kunit *test)
{
	struct maptbl *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	maptbl_free_buffer(NULL);

	m->arr = NULL;
	maptbl_free_buffer(m);
}

static void maptbl_test_maptbl_free_buffer_success(struct kunit *test)
{
	struct maptbl *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	KUNIT_ASSERT_EQ(test, maptbl_alloc_buffer(m, 256), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m->arr);

	maptbl_free_buffer(m);
	KUNIT_EXPECT_TRUE(test, !m->arr);
}

/* maptbl_set_shape() test */
static void maptbl_test_maptbl_set_shape_fail_with_invalid_argument(struct kunit *test)
{
	struct maptbl *m;
	DEFINE_MAPTBL_SHAPE(shape_4d_5x4x3x2, 5, 4, 3, 2);

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	maptbl_set_shape(NULL, &shape_4d_5x4x3x2);
	maptbl_set_shape(m, NULL);
}

static void maptbl_test_maptbl_set_shape_success(struct kunit *test)
{
	struct maptbl *m;
	DEFINE_MAPTBL_SHAPE(shape_4d_5x4x3x2, 5, 4, 3, 2);

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	maptbl_set_shape(m, &shape_4d_5x4x3x2);

	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, (unsigned int)4);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_1D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_2D], (unsigned int)3);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_3D], (unsigned int)4);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_4D], (unsigned int)5);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_5D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_6D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_7D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_8D], (unsigned int)0);
}

/* maptbl_set_ops() test */
static void maptbl_test_maptbl_set_ops_fail_with_invalid_argument(struct kunit *test)
{
	struct maptbl *m;
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(fake_callback_return_error),
		.getidx = &PNOBJ_FUNC(fake_callback_return_zero),
		.copy = &PNOBJ_FUNC(fake_copy_callback_void_return),
	};

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	maptbl_set_ops(NULL, &ops);
	maptbl_set_ops(m, NULL);
}

static void maptbl_test_maptbl_set_ops_copy_operation_success(struct kunit *test)
{
	struct maptbl *m;
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(fake_callback_return_error),
		.getidx = &PNOBJ_FUNC(fake_callback_return_zero),
		.copy = &PNOBJ_FUNC(fake_copy_callback_void_return),
	};

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	maptbl_set_ops(m, &ops);
	KUNIT_EXPECT_PTR_EQ(test, m->ops.init, ops.init);
	KUNIT_EXPECT_PTR_EQ(test, m->ops.getidx, ops.getidx);
	KUNIT_EXPECT_PTR_EQ(test, m->ops.copy, ops.copy);
}

/* maptbl_set_initialized() test */
static void maptbl_test_maptbl_set_initialized_fail_with_invalid_argument(struct kunit *test)
{
	struct maptbl *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	maptbl_set_initialized(NULL, true);
}

static void maptbl_test_maptbl_set_initialized_sholud_update_initlaized_flag(struct kunit *test)
{
	struct maptbl *m;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	maptbl_set_initialized(m, true);
	KUNIT_EXPECT_TRUE(test, m->initialized);
	KUNIT_EXPECT_TRUE(test, maptbl_is_initialized(m));

	maptbl_set_initialized(m, false);
	KUNIT_EXPECT_FALSE(test, m->initialized);
	KUNIT_EXPECT_FALSE(test, maptbl_is_initialized(m));
}

/* maptbl_set_private_data(), maptbl_get_private_data() test */
static void maptbl_test_maptbl_private_data_setter_and_getter_fail_with_invalid_argument(struct kunit *test)
{
	struct maptbl *m;
	void *priv;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	priv = kunit_kzalloc(test, sizeof(int), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, priv);

	maptbl_set_private_data(NULL, priv);
	maptbl_set_private_data(m, NULL);
	KUNIT_EXPECT_TRUE(test, !m->pdata);

	KUNIT_EXPECT_TRUE(test, !maptbl_get_private_data(NULL));
	KUNIT_EXPECT_TRUE(test, !maptbl_get_private_data(m));
}

static void maptbl_test_maptbl_private_data_setter_and_getter_success(struct kunit *test)
{
	struct maptbl *m;
	void *priv;

	m = kunit_kzalloc(test, sizeof(*m), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);

	priv = kunit_kzalloc(test, sizeof(int), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, priv);

	maptbl_set_private_data(m, priv);
	KUNIT_EXPECT_PTR_EQ(test, m->pdata, priv);
	KUNIT_EXPECT_PTR_EQ(test, maptbl_get_private_data(m), priv);
}

/* maptbl_create() and maptbl_destroy() test */
static void maptbl_test_maptbl_create_should_create_empty_maptbl_with_no_name_and_shape(struct kunit *test)
{
	struct maptbl *m = maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_TRUE(test, !maptbl_get_name(m));
	KUNIT_EXPECT_TRUE(test, !m->shape.nr_dimen);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_1D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_2D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_3D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_4D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_5D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_6D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_7D], (unsigned int)0);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_8D], (unsigned int)0);

	kfree(m);
}

static void maptbl_test_maptbl_destroy_should_not_occur_exception_with_null_pointer_or_empty_maptbl(struct kunit *test)
{
	struct maptbl *m = maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL);

	maptbl_destroy(NULL);
	maptbl_destroy(m);
}

static void maptbl_test_maptbl_create_should_allocate_as_shape_and_copy_init_data(struct kunit *test)
{
	struct maptbl *m;
	DEFINE_MAPTBL_SHAPE(shape_8d_2x2x2x2x2x2x2x2, 2, 2, 2, 2, 2, 2, 2, 2);
	char *name = "test-maptbl";
	void *init_data;
	void *priv_data;

	init_data = kunit_kzalloc(test, sizeof(u8) * 256, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, init_data);

	priv_data = kunit_kzalloc(test, sizeof(int), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, priv_data);

	memset(init_data, 0xEF, sizeof(u8) * 256);

	m = maptbl_create(name, &shape_8d_2x2x2x2x2x2x2x2, NULL, NULL, init_data, priv_data);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);

	KUNIT_EXPECT_PTR_NE(test, maptbl_get_name(m), name);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), name);
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, (unsigned int)8);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_1D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_2D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_3D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_4D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_5D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_6D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_7D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, m->shape.sz_dimen[NDARR_8D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), 256);

	/* arr should not be same with init_data */
	KUNIT_EXPECT_PTR_NE(test, m->arr, (u8 *)init_data);

	/* memory data m->arr should be same with memroy data of init_data */
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, init_data, sizeof(u8) * 256));

	maptbl_destroy(m);
}

static void maptbl_test_dynamic_allocated_maptbl_should_be_same_with_maptbl_is_defined_by_macro(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(fake_callback_return_zero),
		.getidx = &PNOBJ_FUNC(fake_callback_return_zero),
		.copy = &PNOBJ_FUNC(fake_copy_callback_void_return),
	};
	DEFINE_MAPTBL_SHAPE(shape_1d_2, 2);
	DEFINE_MAPTBL_SHAPE(shape_2d_3x2, 3, 2);
	DEFINE_MAPTBL_SHAPE(shape_3d_4x3x2, 4, 3, 2);
	DEFINE_MAPTBL_SHAPE(shape_4d_5x4x3x2, 5, 4, 3, 2);
	char *name_1d_table = "test_1d_table";
	char *name_2d_table = "test_2d_table";
	char *name_3d_table = "test_3d_table";
	char *name_4d_table = "test_4d_table";
	void *init_data;
	struct maptbl *m;
	char message[256];
	int i;

	init_data = kunit_kzalloc(test, sizeof(u8) * 256, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, init_data);

	memset(test_1d_table, 0xF1, sizeof(test_1d_table));
	memset(init_data, 0xF1, sizeof(u8) * 256);
	m = maptbl_create(name_1d_table, &shape_1d_2, &ops, NULL, init_data, NULL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_1d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_1d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_1d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_1d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_1d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_1d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_1d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_2d_table, 0xF2, sizeof(test_2d_table));
	memset(init_data, 0xF2, sizeof(u8) * 256);
	m = maptbl_create(name_2d_table, &shape_2d_3x2, &ops, NULL, init_data, NULL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_2d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_2d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_2d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_2d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_2d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_2d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_2d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_3d_table, 0xF3, sizeof(test_3d_table));
	memset(init_data, 0xF3, sizeof(u8) * 256);
	m = maptbl_create(name_3d_table, &shape_3d_4x3x2, &ops, NULL, init_data, NULL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_3d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_3d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_3d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_3d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_3d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_3d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_3d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_4d_table, 0xF4, sizeof(test_4d_table));
	memset(init_data, 0xF4, sizeof(u8) * 256);
	m = maptbl_create(name_4d_table, &shape_4d_5x4x3x2, &ops, NULL, init_data, NULL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_4d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_4d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_4d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_4d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_4d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_4d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_4d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);
}

/* maptbl_clone() */
static void maptbl_test_maptbl_clone_fail_with_invalid_args(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, !maptbl_clone(NULL));
}

static void maptbl_test_maptbl_clone_success_cloning_empty_maptbl(struct kunit *test)
{
	struct maptbl *m1, *m2;

	m1 = maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m1);

	m2 = maptbl_clone(m1);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m2);
	KUNIT_EXPECT_PTR_NE(test, m1, m2);
	KUNIT_EXPECT_TRUE(test, !m2->arr);

	maptbl_destroy(m1);
	maptbl_destroy(m2);
}

static void maptbl_test_cloned_maptbl_should_be_same_with_original_maptbl(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl *m;
	char message[256];
	int i;

	memset(test_1d_table, 0xF1, sizeof(test_1d_table));
	m = maptbl_clone(&test_1d_maptbl);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_1d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_1d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_1d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_1d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_1d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_1d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_1d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_2d_table, 0xF2, sizeof(test_2d_table));
	m = maptbl_clone(&test_2d_maptbl);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_2d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_2d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_2d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_2d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_2d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_2d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_2d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_3d_table, 0xF3, sizeof(test_3d_table));
	m = maptbl_clone(&test_3d_maptbl);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_3d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_3d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_3d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_3d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_3d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_3d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_3d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_4d_table, 0xF4, sizeof(test_4d_table));
	m = maptbl_clone(&test_4d_maptbl);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_4d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_4d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_4d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_4d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_4d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_4d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_4d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);
}

/* maptbl_deepcopy() test */
static void maptbl_test_maptbl_deepcopy_fail_with_invalid_args(struct kunit *test)
{
	struct maptbl *m1, *m2;

	m1 = maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL);
	m2 = maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL);

	KUNIT_EXPECT_TRUE(test, !maptbl_deepcopy(NULL, NULL));
	KUNIT_EXPECT_TRUE(test, !maptbl_deepcopy(m1, NULL));
	KUNIT_EXPECT_TRUE(test, !maptbl_deepcopy(NULL, m2));

	maptbl_destroy(m1);
	maptbl_destroy(m2);
}

static void maptbl_test_maptbl_deepcopy_should_free_array_buffer_if_source_maptbl_array_is_null(struct kunit *test)
{
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(fake_callback_return_zero),
		.getidx = &PNOBJ_FUNC(fake_callback_return_zero),
		.copy = &PNOBJ_FUNC(fake_copy_callback_void_return),
	};
	DEFINE_MAPTBL_SHAPE(shape, 5, 4, 3, 2);
	unsigned char init_data[5 * 4 * 3 * 2];
	struct maptbl *src, *dst, *m;

	memset(init_data, 0xF1, sizeof(init_data));
	src = maptbl_create("maptbl", NULL, &ops, NULL, init_data, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, src);
	KUNIT_ASSERT_TRUE(test, !src->arr);

	dst = maptbl_create("maptbl", &shape, &ops, NULL, init_data, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dst);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dst->arr);

	m = maptbl_deepcopy(dst, src);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_PTR_EQ(test, m, dst);
	KUNIT_EXPECT_TRUE(test, !m->arr);

	maptbl_destroy(src);
	KUNIT_ASSERT_TRUE(test, !src->arr);

	maptbl_destroy(dst);
	KUNIT_ASSERT_TRUE(test, !dst->arr);
}

static void maptbl_test_maptbl_deepcopy_success_if_passing_same_maptbl_pointer(struct kunit *test)
{
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(fake_callback_return_zero),
		.getidx = &PNOBJ_FUNC(fake_callback_return_zero),
		.copy = &PNOBJ_FUNC(fake_copy_callback_void_return),
	};
	DEFINE_MAPTBL_SHAPE(shape, 5, 4, 3, 2);
	unsigned char init_data[5 * 4 * 3 * 2], *arr;
	struct maptbl *m1, *m2;

	memset(init_data, 0xF1, sizeof(init_data));
	m1 = maptbl_create("maptbl", &shape, &ops, NULL, init_data, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m1->arr);
	arr = m1->arr;

	m2 = maptbl_deepcopy(m1, m1);
	KUNIT_EXPECT_PTR_EQ(test, m1, m2);
	KUNIT_EXPECT_PTR_EQ(test, m1->arr, arr);

	maptbl_destroy(m1);
	KUNIT_ASSERT_TRUE(test, !m1->arr);
}

static void maptbl_test_dst_maptbl_array_buffer_should_be_free_and_allocated(struct kunit *test)
{
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(fake_callback_return_zero),
		.getidx = &PNOBJ_FUNC(fake_callback_return_zero),
		.copy = &PNOBJ_FUNC(fake_copy_callback_void_return),
	};
	DEFINE_MAPTBL_SHAPE(shape, 5, 4, 3, 2);
	unsigned char init_data[5 * 4 * 3 * 2], *arr1;
	struct maptbl *m1, *m2, *m3;

	memset(init_data, 0xF1, sizeof(init_data));
	m1 = maptbl_create("maptbl", &shape, &ops, NULL, init_data, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m1->arr);
	arr1 = m1->arr;

	memset(init_data, 0xF2, sizeof(init_data));
	m2 = maptbl_create("maptbl", &shape, &ops, NULL, init_data, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m2);

	/*
	 * 1) free m1->arr
	 * 2) assign m1->arr array pointer is allocated
	 * 3) copy from m2->arr to m1->arr.
	 */
	m3 = maptbl_deepcopy(m1, m2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m3);
	KUNIT_EXPECT_PTR_EQ(test, m1, m3);
	KUNIT_EXPECT_PTR_NE(test, m1->arr, m2->arr);

	maptbl_destroy(m1);
	KUNIT_ASSERT_TRUE(test, !m1->arr);

	maptbl_destroy(m2);
	KUNIT_ASSERT_TRUE(test, !m2->arr);
}

static void maptbl_test_deep_copied_maptbl_should_be_same_with_original_maptbl(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_callback_return_zero), &PNOBJ_FUNC(fake_copy_callback_void_return));
	void *init_data;
	struct maptbl *m;
	char message[256];
	int i;

	init_data = kunit_kzalloc(test, sizeof(u8) * 256, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, init_data);

	memset(test_1d_table, 0xF1, sizeof(test_1d_table));
	memset(init_data, 0xF1, sizeof(u8) * 256);
	m = maptbl_deepcopy(maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL), &test_1d_maptbl);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_1d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_1d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_1d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_1d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_1d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_1d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_1d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_2d_table, 0xF2, sizeof(test_2d_table));
	memset(init_data, 0xF2, sizeof(u8) * 256);
	m = maptbl_deepcopy(maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL), &test_2d_maptbl);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_2d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_2d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_2d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_2d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_2d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_2d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_2d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_3d_table, 0xF3, sizeof(test_3d_table));
	memset(init_data, 0xF3, sizeof(u8) * 256);
	m = maptbl_deepcopy(maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL), &test_3d_maptbl);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_3d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_3d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_3d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_3d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_3d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_3d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_3d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);

	memset(test_4d_table, 0xF4, sizeof(test_4d_table));
	memset(init_data, 0xF4, sizeof(u8) * 256);
	m = maptbl_deepcopy(maptbl_create(NULL, NULL, NULL, NULL, NULL, NULL), &test_4d_maptbl);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, m);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m), maptbl_get_name(&test_4d_maptbl));
	KUNIT_EXPECT_EQ(test, m->shape.nr_dimen, test_4d_maptbl.shape.nr_dimen);
	for (i = 0; i < m->shape.nr_dimen; i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected expected (allocated) sz_dimen[%d] == (defined) sz_dimen[%d], but\n", i, i);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (allocated) sz_dimen[%d] == 0x%02X\n", i, m->shape.sz_dimen[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"expected (defined) sz_dimen[%d] == 0x%02X\n", i, test_4d_maptbl.shape.sz_dimen[i]);
		KUNIT_EXPECT_TRUE_MSG(test, (m->shape.sz_dimen[i] == test_4d_maptbl.shape.sz_dimen[i]), message);
	}
	KUNIT_EXPECT_TRUE(test, !memcmp(&m->ops, &test_4d_maptbl.ops, sizeof(m->ops)));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(m), maptbl_get_sizeof_maptbl(&test_4d_maptbl));
	KUNIT_EXPECT_TRUE(test, !memcmp(m->arr, test_4d_maptbl.arr, maptbl_get_sizeof_maptbl(m)));
	maptbl_destroy(m);
}

/* maptbl_for_each() test */
static void maptbl_test_maptbl_for_each_iteration_count_should_be_same_with_size_of_maptbl(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	int index, iteration_count, begin, end;

	iteration_count = 0;
	maptbl_for_each(&test_1d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, 0);
	KUNIT_EXPECT_EQ(test, end, (int)sizeof(test_1d_table));
	KUNIT_EXPECT_EQ(test, iteration_count, (int)sizeof(test_1d_table));

	iteration_count = 0;
	maptbl_for_each(&test_2d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, 0);
	KUNIT_EXPECT_EQ(test, end, (int)sizeof(test_2d_table));
	KUNIT_EXPECT_EQ(test, iteration_count, (int)sizeof(test_2d_table));

	iteration_count = 0;
	maptbl_for_each(&test_3d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, 0);
	KUNIT_EXPECT_EQ(test, end, (int)sizeof(test_3d_table));
	KUNIT_EXPECT_EQ(test, iteration_count, (int)sizeof(test_3d_table));

	iteration_count = 0;
	maptbl_for_each(&test_4d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, 0);
	KUNIT_EXPECT_EQ(test, end, (int)sizeof(test_4d_table));
	KUNIT_EXPECT_EQ(test, iteration_count, (int)sizeof(test_4d_table));
}

/* maptbl_for_each_reverse() test */
static void maptbl_test_maptbl_for_each_reverse_iteration_count_should_be_same_with_size_of_maptbl(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	int index, iteration_count, begin, end;

	iteration_count = 0;
	maptbl_for_each_reverse(&test_1d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, (int)(sizeof(test_1d_table) - 1));
	KUNIT_EXPECT_EQ(test, end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, (int)sizeof(test_1d_table));

	iteration_count = 0;
	maptbl_for_each_reverse(&test_2d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, (int)(sizeof(test_2d_table) - 1));
	KUNIT_EXPECT_EQ(test, end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, (int)sizeof(test_2d_table));

	iteration_count = 0;
	maptbl_for_each_reverse(&test_3d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, (int)(sizeof(test_3d_table) - 1));
	KUNIT_EXPECT_EQ(test, end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, (int)sizeof(test_3d_table));

	iteration_count = 0;
	maptbl_for_each_reverse(&test_4d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, (int)(sizeof(test_4d_table) - 1));
	KUNIT_EXPECT_EQ(test, end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, (int)sizeof(test_4d_table));
}

static void maptbl_test_maptbl_for_each_reverse_with_unsigned_int_variable(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	unsigned int index, iteration_count, begin, end;

	iteration_count = 0;
	maptbl_for_each_reverse(&test_4d_maptbl, index) {
		if (iteration_count == 0)
			begin = index;
		iteration_count++;
		if (iteration_count > maptbl_get_sizeof_maptbl(&test_4d_maptbl))
			break;
	}
	end = index;
	KUNIT_EXPECT_EQ(test, begin, (unsigned int)(sizeof(test_4d_table) - 1));
	KUNIT_EXPECT_EQ(test, (int)end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, (unsigned int)sizeof(test_4d_table));
}

/* maptbl_for_each_dimen() test */
static void maptbl_test_maptbl_for_each_dimen_iteration_count_should_be_same_with_number_of_dimensions(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	int dimen, iteration_count, begin, end;

	iteration_count = 0;
	maptbl_for_each_dimen(&test_1d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, begin, NDARR_1D);
	KUNIT_EXPECT_EQ(test, end, maptbl_get_countof_dimen(&test_1d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_dimen(&test_1d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, 1);

	iteration_count = 0;
	maptbl_for_each_dimen(&test_2d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, begin, NDARR_1D);
	KUNIT_EXPECT_EQ(test, end, maptbl_get_countof_dimen(&test_2d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_dimen(&test_2d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, 2);

	iteration_count = 0;
	maptbl_for_each_dimen(&test_3d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, begin, NDARR_1D);
	KUNIT_EXPECT_EQ(test, end, maptbl_get_countof_dimen(&test_3d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_dimen(&test_3d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, 3);

	iteration_count = 0;
	maptbl_for_each_dimen(&test_4d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, begin, NDARR_1D);
	KUNIT_EXPECT_EQ(test, end, maptbl_get_countof_dimen(&test_4d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_dimen(&test_4d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, 4);
}

/* maptbl_for_each_dimen_reverse() test */
static void maptbl_test_maptbl_for_each_dimen_reverse_iteration_count_should_be_same_with_number_of_dimensions(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	int dimen, iteration_count, begin, end;

	iteration_count = 0;
	maptbl_for_each_dimen_reverse(&test_1d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, begin, maptbl_get_countof_dimen(&test_1d_maptbl) - 1);
	KUNIT_EXPECT_EQ(test, end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_dimen(&test_1d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, 1);

	iteration_count = 0;
	maptbl_for_each_dimen_reverse(&test_2d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, begin, maptbl_get_countof_dimen(&test_2d_maptbl) - 1);
	KUNIT_EXPECT_EQ(test, end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_dimen(&test_2d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, 2);

	iteration_count = 0;
	maptbl_for_each_dimen_reverse(&test_3d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, begin, maptbl_get_countof_dimen(&test_3d_maptbl) - 1);
	KUNIT_EXPECT_EQ(test, end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_dimen(&test_3d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, 3);

	iteration_count = 0;
	maptbl_for_each_dimen_reverse(&test_4d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, begin, maptbl_get_countof_dimen(&test_4d_maptbl) - 1);
	KUNIT_EXPECT_EQ(test, end, -1);
	KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_dimen(&test_4d_maptbl));
	KUNIT_EXPECT_EQ(test, iteration_count, 4);
}

static void maptbl_test_maptbl_for_each_dimen_reverse_with_unsigned_int_variable(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	unsigned int dimen, iteration_count, begin, end;

	iteration_count = 0;
	maptbl_for_each_dimen_reverse(&test_4d_maptbl, dimen) {
		if (iteration_count == 0)
			begin = dimen;
		iteration_count++;
		if (iteration_count > maptbl_get_countof_dimen(&test_4d_maptbl))
			break;
	}
	end = dimen;
	KUNIT_EXPECT_EQ(test, (int)begin, maptbl_get_countof_dimen(&test_4d_maptbl) - 1);
	KUNIT_EXPECT_EQ(test, (int)end, -1);
	KUNIT_EXPECT_EQ(test, (int)iteration_count, maptbl_get_countof_dimen(&test_4d_maptbl));
	KUNIT_EXPECT_EQ(test, (int)iteration_count, 4);
}

/* maptbl_for_each_n_dimen_element() test */
static void maptbl_test_maptbl_for_each_n_dimen_element_iteration_count_should_be_same_with_number_of_elements(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	int indexof_element, dimen, iteration_count, begin, end;

	maptbl_for_each_dimen(&test_1d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element(&test_1d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, begin, 0);
		KUNIT_EXPECT_EQ(test, end, maptbl_get_countof_n_dimen_element(&test_1d_maptbl, dimen));
		KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_n_dimen_element(&test_1d_maptbl, dimen));
	}

	maptbl_for_each_dimen(&test_2d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element(&test_2d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, begin, 0);
		KUNIT_EXPECT_EQ(test, end, maptbl_get_countof_n_dimen_element(&test_2d_maptbl, dimen));
		KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_n_dimen_element(&test_2d_maptbl, dimen));
	}

	maptbl_for_each_dimen(&test_3d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element(&test_3d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, begin, 0);
		KUNIT_EXPECT_EQ(test, end, maptbl_get_countof_n_dimen_element(&test_3d_maptbl, dimen));
		KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_n_dimen_element(&test_3d_maptbl, dimen));
	}

	maptbl_for_each_dimen(&test_4d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element(&test_4d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, begin, 0);
		KUNIT_EXPECT_EQ(test, end, maptbl_get_countof_n_dimen_element(&test_4d_maptbl, dimen));
		KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_n_dimen_element(&test_4d_maptbl, dimen));
	}
}

/* maptbl_for_each_n_dimen_element_reverse() test */
static void maptbl_test_maptbl_for_each_n_dimen_element_reverse_iteration_count_should_be_same_with_number_of_elements(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	int indexof_element, dimen, iteration_count, begin, end;

	maptbl_for_each_dimen(&test_1d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element_reverse(&test_1d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, begin, maptbl_get_countof_n_dimen_element(&test_1d_maptbl, dimen) - 1);
		KUNIT_EXPECT_EQ(test, end, -1);
		KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_n_dimen_element(&test_1d_maptbl, dimen));
	}

	maptbl_for_each_dimen(&test_2d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element_reverse(&test_2d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, begin, maptbl_get_countof_n_dimen_element(&test_2d_maptbl, dimen) - 1);
		KUNIT_EXPECT_EQ(test, end, -1);
		KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_n_dimen_element(&test_2d_maptbl, dimen));
	}

	maptbl_for_each_dimen(&test_3d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element_reverse(&test_3d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, begin, maptbl_get_countof_n_dimen_element(&test_3d_maptbl, dimen) - 1);
		KUNIT_EXPECT_EQ(test, end, -1);
		KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_n_dimen_element(&test_3d_maptbl, dimen));
	}

	maptbl_for_each_dimen(&test_4d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element_reverse(&test_4d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, begin, maptbl_get_countof_n_dimen_element(&test_4d_maptbl, dimen) - 1);
		KUNIT_EXPECT_EQ(test, end, -1);
		KUNIT_EXPECT_EQ(test, iteration_count, maptbl_get_countof_n_dimen_element(&test_4d_maptbl, dimen));
	}
}

static void maptbl_test_maptbl_for_each_n_dimen_element_reverse_with_unsigned_int_variable(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	unsigned int indexof_element, dimen, iteration_count, begin, end;

	maptbl_for_each_dimen(&test_4d_maptbl, dimen) {
		iteration_count = 0;
		maptbl_for_each_n_dimen_element_reverse(&test_4d_maptbl, dimen, indexof_element) {
			if (iteration_count == 0)
				begin = indexof_element;
			iteration_count++;
			if (iteration_count > maptbl_get_countof_n_dimen_element(&test_4d_maptbl, dimen))
				break;
		}
		end = indexof_element;
		KUNIT_EXPECT_EQ(test, (int)begin, maptbl_get_countof_n_dimen_element(&test_4d_maptbl, dimen) - 1);
		KUNIT_EXPECT_EQ(test, (int)end, -1);
		KUNIT_EXPECT_EQ(test, (int)iteration_count, maptbl_get_countof_n_dimen_element(&test_4d_maptbl, dimen));
	}
}

/* maptbl_get_indexof_xxx() function test */
static void maptbl_test_maptbl_get_indexof_fail_with_null_maptbl(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(NULL, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(NULL, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(NULL, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(NULL, 0), -EINVAL);
}

static void maptbl_test_maptbl_get_indexof_fail_if_index_exceed_countof_each_dimension(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	/* indexof_col for maptbl of each dimension */
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(&test_1d_maptbl, ARRAY_SIZE(test_1d_table)), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(&test_2d_maptbl, ARRAY_SIZE(test_2d_table[0])), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(&test_3d_maptbl, ARRAY_SIZE(test_3d_table[0][0])), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(&test_4d_maptbl, ARRAY_SIZE(test_4d_table[0][0][0])), -EINVAL);

	/* indexof_row for maptbl of each dimension */
	/* TODO: 1D MAPTBL must be removed */
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(&test_1d_maptbl, ARRAY_SIZE(test_1d_table)), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(&test_2d_maptbl, ARRAY_SIZE(test_2d_table)), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(&test_3d_maptbl, ARRAY_SIZE(test_3d_table[0])), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(&test_4d_maptbl, ARRAY_SIZE(test_4d_table[0][0])), -EINVAL);

	/* indexof_layer for maptbl of each dimension */
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(&test_1d_maptbl, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(&test_2d_maptbl, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(&test_3d_maptbl, ARRAY_SIZE(test_3d_table)), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(&test_4d_maptbl, ARRAY_SIZE(test_4d_table[0])), -EINVAL);

	/* indexof_box for maptbl of each dimension */
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(&test_1d_maptbl, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(&test_2d_maptbl, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(&test_3d_maptbl, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(&test_4d_maptbl, ARRAY_SIZE(test_4d_table)), -EINVAL);
}

static void maptbl_test_maptbl_get_indexof_success_if_index_less_than_countof_each_dimension(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	/* indexof_col for maptbl of each dimension */
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(&test_1d_maptbl, ARRAY_SIZE(test_1d_table) - 1),
			(int)ARRAY_SIZE(test_1d_table) - 1);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(&test_2d_maptbl, ARRAY_SIZE(test_2d_table[0]) - 1),
			(int)ARRAY_SIZE(test_2d_table[0]) - 1);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(&test_3d_maptbl, ARRAY_SIZE(test_3d_table[0][0]) - 1),
			(int)ARRAY_SIZE(test_3d_table[0][0]) - 1);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_col(&test_4d_maptbl, ARRAY_SIZE(test_4d_table[0][0][0]) - 1),
			(int)ARRAY_SIZE(test_4d_table[0][0][0]) - 1);

	/* indexof_row for maptbl of each dimension */
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(&test_1d_maptbl, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(&test_2d_maptbl, ARRAY_SIZE(test_2d_table) - 1),
			(int)(maptbl_get_sizeof_row(&test_2d_maptbl) * (ARRAY_SIZE(test_2d_table) - 1)));
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(&test_3d_maptbl, ARRAY_SIZE(test_3d_table[0]) - 1),
			(int)(maptbl_get_sizeof_row(&test_3d_maptbl) * (ARRAY_SIZE(test_3d_table[0]) - 1)));
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_row(&test_4d_maptbl, ARRAY_SIZE(test_4d_table[0][0]) - 1),
			(int)(maptbl_get_sizeof_row(&test_4d_maptbl) * (ARRAY_SIZE(test_4d_table[0][0]) - 1)));

	/* indexof_layer for maptbl of each dimension */
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(&test_1d_maptbl, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(&test_2d_maptbl, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(&test_3d_maptbl, ARRAY_SIZE(test_3d_table) - 1),
			(int)(maptbl_get_sizeof_layer(&test_3d_maptbl) * (ARRAY_SIZE(test_3d_table) - 1)));
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_layer(&test_4d_maptbl, ARRAY_SIZE(test_4d_table[0]) - 1),
			(int)(maptbl_get_sizeof_layer(&test_4d_maptbl) * (ARRAY_SIZE(test_4d_table[0]) - 1)));

	/* indexof_box for maptbl of each dimension */
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(&test_1d_maptbl, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(&test_2d_maptbl, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(&test_3d_maptbl, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_get_indexof_box(&test_4d_maptbl, ARRAY_SIZE(test_4d_table) - 1),
			(int)(maptbl_get_sizeof_box(&test_4d_maptbl) * (ARRAY_SIZE(test_4d_table) - 1)));
}

/* maptbl_get_sizeof_box() function test */
static void maptbl_test_sizeof_box_should_be_same_with_size_of_3d_table_inside_of_n_dimension_table(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_box(&test_1d_maptbl), (int)sizeof(test_1d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_box(&test_2d_maptbl), (int)sizeof(test_2d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_box(&test_3d_maptbl), (int)sizeof(test_3d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_box(&test_4d_maptbl), (int)sizeof(test_4d_table[0]));
}

/* maptbl_get_sizeof_layer() function test */
static void maptbl_test_sizeof_layer_should_be_same_with_size_of_2d_table_inside_of_n_dimension_table(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_layer(&test_1d_maptbl), (int)sizeof(test_1d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_layer(&test_2d_maptbl), (int)sizeof(test_2d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_layer(&test_3d_maptbl), (int)sizeof(test_3d_table[0]));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_layer(&test_4d_maptbl), (int)sizeof(test_4d_table[0][0]));
}

/* maptbl_get_sizeof_row() function test */
static void maptbl_test_sizeof_row_should_be_same_with_size_of_1d_table_inside_of_n_dimension_table(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_row(&test_1d_maptbl), (int)sizeof(test_1d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_row(&test_2d_maptbl), (int)sizeof(test_2d_table[0]));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_row(&test_3d_maptbl), (int)sizeof(test_3d_table[0][0]));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_row(&test_4d_maptbl), (int)sizeof(test_4d_table[0][0][0]));
}

/* sizeof_maptbl function test */
static void maptbl_test_sizeof_maptbl_should_be_same_with_size_of_table(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(&test_1d_maptbl), (int)sizeof(test_1d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(&test_2d_maptbl), (int)sizeof(test_2d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(&test_3d_maptbl), (int)sizeof(test_3d_table));
	KUNIT_EXPECT_EQ(test, maptbl_get_sizeof_maptbl(&test_4d_maptbl), (int)sizeof(test_4d_table));
}

/* maptbl_index function test */
static void maptbl_test_maptbl_index_fail_with_null_maptbl(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, maptbl_index(NULL, 0, 0, 0), -EINVAL);
}

static void maptbl_test_maptbl_index_should_be_sum_of_index_of_each_dimensions(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_index(&test_1d_maptbl, 0, 0, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_index(&test_2d_maptbl, 0, 0, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_index(&test_3d_maptbl, 0, 0, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_index(&test_4d_maptbl, 0, 0, 0), 0);

	KUNIT_EXPECT_EQ(test, maptbl_index(&test_1d_maptbl, 0, 0, 1),
			(maptbl_get_sizeof_box(&test_1d_maptbl) * 0 +
			maptbl_get_sizeof_layer(&test_1d_maptbl) * 0 +
			maptbl_get_sizeof_row(&test_1d_maptbl) * 0 + 1));
	KUNIT_EXPECT_EQ(test, maptbl_index(&test_2d_maptbl, 0, 1, 1),
			(maptbl_get_sizeof_box(&test_2d_maptbl) * 0 +
			maptbl_get_sizeof_layer(&test_2d_maptbl) * 0 +
			maptbl_get_sizeof_row(&test_2d_maptbl) * 1 + 1));
	KUNIT_EXPECT_EQ(test, maptbl_index(&test_3d_maptbl, 1, 1, 1),
			(maptbl_get_sizeof_box(&test_3d_maptbl) * 0 +
			maptbl_get_sizeof_layer(&test_3d_maptbl) * 1 +
			maptbl_get_sizeof_row(&test_3d_maptbl) * 1 + 1));
	KUNIT_EXPECT_EQ(test, maptbl_index(&test_4d_maptbl, 1, 1, 1),
			(maptbl_get_sizeof_box(&test_4d_maptbl) * 0 +
			maptbl_get_sizeof_layer(&test_4d_maptbl) * 1 +
			maptbl_get_sizeof_row(&test_4d_maptbl) * 1 + 1));
}

static void maptbl_test_maptbl_4d_index_fail_with_null_maptbl(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, maptbl_4d_index(NULL, 0, 0, 0, 0), -EINVAL);
}

static void maptbl_test_maptbl_4d_index_should_be_sum_of_index_of_each_dimensions(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_4d_index(&test_1d_maptbl, 0, 0, 0, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_4d_index(&test_2d_maptbl, 0, 0, 0, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_4d_index(&test_3d_maptbl, 0, 0, 0, 0), 0);
	KUNIT_EXPECT_EQ(test, maptbl_4d_index(&test_4d_maptbl, 0, 0, 0, 0), 0);

	KUNIT_EXPECT_EQ(test, maptbl_4d_index(&test_1d_maptbl, 0, 0, 0, 1),
			(maptbl_get_sizeof_box(&test_1d_maptbl) * 0 +
			maptbl_get_sizeof_layer(&test_1d_maptbl) * 0 +
			maptbl_get_sizeof_row(&test_1d_maptbl) * 0 + 1));
	KUNIT_EXPECT_EQ(test, maptbl_4d_index(&test_2d_maptbl, 0, 0, 1, 1),
			(maptbl_get_sizeof_box(&test_2d_maptbl) * 0 +
			maptbl_get_sizeof_layer(&test_2d_maptbl) * 0 +
			maptbl_get_sizeof_row(&test_2d_maptbl) * 1 + 1));
	KUNIT_EXPECT_EQ(test, maptbl_4d_index(&test_3d_maptbl, 0, 1, 1, 1),
			(maptbl_get_sizeof_box(&test_3d_maptbl) * 0 +
			maptbl_get_sizeof_layer(&test_3d_maptbl) * 1 +
			maptbl_get_sizeof_row(&test_3d_maptbl) * 1 + 1));
	KUNIT_EXPECT_EQ(test, maptbl_4d_index(&test_4d_maptbl, 1, 1, 1, 1),
			(maptbl_get_sizeof_box(&test_4d_maptbl) * 1 +
			maptbl_get_sizeof_layer(&test_4d_maptbl) * 1 +
			maptbl_get_sizeof_row(&test_4d_maptbl) * 1 + 1));
}

/* maptbl_pos_to_index function test */
static void maptbl_test_maptbl_pos_to_index_fail_with_invalid_args(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl_pos target_pos;

	KUNIT_EXPECT_EQ(test, maptbl_pos_to_index(NULL, &target_pos), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_pos_to_index(&test_4d_maptbl, NULL), -EINVAL);
}

static void maptbl_test_maptbl_pos_to_index_success(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl_pos target_pos = {
		.index = {
			[NDARR_1D] = 0,
			[NDARR_2D] = 2,
			[NDARR_3D] = 3,
			[NDARR_4D] = 4,
		},
	};

	KUNIT_EXPECT_EQ(test, maptbl_pos_to_index(&test_4d_maptbl, &target_pos),
			(int)(sizeof(test_4d_table[0]) * 4 +
			sizeof(test_4d_table[0][0]) * 3 +
			sizeof(test_4d_table[0][0][0]) * 2));
	KUNIT_EXPECT_EQ(test, maptbl_pos_to_index(&test_4d_maptbl, &target_pos),
			maptbl_4d_index(&test_4d_maptbl, 4, 3, 2, 0));
}

/* maptbl_index_to_pos function test */
static void maptbl_test_maptbl_index_to_pos_fail_with_invalid_args(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl_pos target_pos;

	KUNIT_EXPECT_EQ(test, maptbl_index_to_pos(NULL, 0, &target_pos), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_index_to_pos(&test_4d_maptbl, 0, NULL), -EINVAL);
}

static void maptbl_test_maptbl_index_to_pos_fail_if_index_is_out_of_maptbl_array_bound(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl_pos target_pos;

	KUNIT_EXPECT_EQ(test, maptbl_index_to_pos(&test_4d_maptbl,
				sizeof(test_4d_table), &target_pos), -EINVAL);
}

static void maptbl_test_maptbl_index_to_pos_success(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl_pos target_pos;

	KUNIT_EXPECT_EQ(test, maptbl_index_to_pos(&test_4d_maptbl,
				(sizeof(test_4d_table[0]) * 4 +
				 sizeof(test_4d_table[0][0]) * 3 +
				 sizeof(test_4d_table[0][0][0]) * 2 +
				 sizeof(test_4d_table[0][0][0][0]) * 1),
				&target_pos), 0);
	KUNIT_EXPECT_EQ(test, target_pos.index[NDARR_4D], (unsigned int)4);
	KUNIT_EXPECT_EQ(test, target_pos.index[NDARR_3D], (unsigned int)3);
	KUNIT_EXPECT_EQ(test, target_pos.index[NDARR_2D], (unsigned int)2);
	KUNIT_EXPECT_EQ(test, target_pos.index[NDARR_1D], (unsigned int)1);
}

/* maptbl_is_initialized function test */
static void maptbl_test_maptbl_is_initialized_false_with_null_maptbl(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, maptbl_is_initialized(NULL));
}

static void maptbl_test_maptbl_is_initialized_true_if_initialized(struct kunit *test)
{
	struct maptbl initialized_maptbl = { .initialized = true };

	KUNIT_EXPECT_TRUE(test, maptbl_is_initialized(&initialized_maptbl));
}

/* maptbl_is_index_in_bound function test */
static void maptbl_test_maptbl_is_index_in_bound_false_with_null_maptbl(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, maptbl_is_index_in_bound(NULL, 0));
}

static void maptbl_test_maptbl_is_index_in_bound_false_if_index_exceed_sizeof_maptbl(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_FALSE(test, maptbl_is_index_in_bound(&test_1d_maptbl,
				sizeof(test_1d_table) - maptbl_get_sizeof_copy(&test_1d_maptbl) + 1));
	KUNIT_EXPECT_FALSE(test, maptbl_is_index_in_bound(&test_2d_maptbl,
				sizeof(test_2d_table) - maptbl_get_sizeof_copy(&test_2d_maptbl) + 1));
	KUNIT_EXPECT_FALSE(test, maptbl_is_index_in_bound(&test_3d_maptbl,
				sizeof(test_3d_table) - maptbl_get_sizeof_copy(&test_3d_maptbl) + 1));
	KUNIT_EXPECT_FALSE(test, maptbl_is_index_in_bound(&test_4d_maptbl,
				sizeof(test_4d_table) - maptbl_get_sizeof_copy(&test_4d_maptbl) + 1));
}

static void maptbl_test_maptbl_is_index_in_bound_true_if_index_less_than_sizeof_maptbl(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_TRUE(test, maptbl_is_index_in_bound(&test_1d_maptbl,
				sizeof(test_1d_table) - maptbl_get_sizeof_copy(&test_1d_maptbl)));
	KUNIT_EXPECT_TRUE(test, maptbl_is_index_in_bound(&test_2d_maptbl,
				sizeof(test_2d_table) - maptbl_get_sizeof_copy(&test_2d_maptbl)));
	KUNIT_EXPECT_TRUE(test, maptbl_is_index_in_bound(&test_3d_maptbl,
				sizeof(test_3d_table) - maptbl_get_sizeof_copy(&test_3d_maptbl)));
	KUNIT_EXPECT_TRUE(test, maptbl_is_index_in_bound(&test_4d_maptbl,
				sizeof(test_4d_table) - maptbl_get_sizeof_copy(&test_4d_maptbl)));
}

/* maptbl init callback test */
static void maptbl_test_maptbl_init_fail_with_null_maptbl(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, maptbl_init(NULL), -EINVAL);
}

static void maptbl_test_maptbl_init_fail_if_init_callback_is_null(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl no_ops_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, NULL, NULL);

	no_ops_2d_maptbl.initialized = false;

	KUNIT_EXPECT_EQ(test, maptbl_init(&no_ops_2d_maptbl), -EINVAL);
	KUNIT_EXPECT_FALSE(test, maptbl_is_initialized(&no_ops_2d_maptbl));
}

static void maptbl_test_maptbl_init_fail_if_init_callback_return_error(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			&PNOBJ_FUNC(fake_callback_return_error), NULL, NULL);

	test_2d_maptbl.initialized = false;

	KUNIT_EXPECT_EQ(test, maptbl_init(&test_2d_maptbl), -EINVAL);
	KUNIT_EXPECT_FALSE(test, maptbl_is_initialized(&test_2d_maptbl));
}

static void maptbl_test_maptbl_init_success_if_init_callback_return_zero(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			&PNOBJ_FUNC(fake_callback_return_zero), NULL, NULL);

	test_2d_maptbl.initialized = false;

	KUNIT_EXPECT_EQ(test, maptbl_init(&test_2d_maptbl), 0);
	KUNIT_EXPECT_TRUE(test, maptbl_is_initialized(&test_2d_maptbl));
}

/* maptbl getidx callback test */
static void maptbl_test_maptbl_getidx_fail_with_null_maptbl(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, maptbl_getidx(NULL), -EINVAL);
}

static void maptbl_test_maptbl_getidx_fail_if_maptbl_is_not_initialized(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl uninit_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	uninit_2d_maptbl.initialized = false;

	KUNIT_EXPECT_EQ(test, maptbl_getidx(&uninit_2d_maptbl), -EINVAL);
}

static void maptbl_test_maptbl_getidx_fail_if_getidx_callback_is_null(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl no_ops_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, NULL, NULL);

	no_ops_2d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_getidx(&no_ops_2d_maptbl), -EINVAL);
}

static void maptbl_test_maptbl_getidx_fail_if_getidx_callback_return_error(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_error), NULL);

	test_2d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_getidx(&test_2d_maptbl), -EINVAL);
}

static void maptbl_test_maptbl_getidx_fail_if_getidx_callback_return_out_of_maptbl_range_index(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);

	test_1d_maptbl.initialized = true;
	test_2d_maptbl.initialized = true;
	test_3d_maptbl.initialized = true;
	test_4d_maptbl.initialized = true;

	fake_return_value = sizeof(test_1d_table) + 1;
	KUNIT_EXPECT_EQ(test, maptbl_getidx(&test_1d_maptbl), -EINVAL);

	fake_return_value = sizeof(test_2d_table) + 1;
	KUNIT_EXPECT_EQ(test, maptbl_getidx(&test_2d_maptbl), -EINVAL);

	fake_return_value = sizeof(test_3d_table) + 1;
	KUNIT_EXPECT_EQ(test, maptbl_getidx(&test_3d_maptbl), -EINVAL);

	fake_return_value = sizeof(test_4d_table) + 1;
	KUNIT_EXPECT_EQ(test, maptbl_getidx(&test_4d_maptbl), -EINVAL);
}

static void maptbl_test_maptbl_getidx_success_if_getidx_callback_return_zero(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	test_2d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_getidx(&test_2d_maptbl), 0);
}

/* maptbl copy callback test */
static void maptbl_test_maptbl_copy_fail_with_null_maptbl(struct kunit *test)
{
	unsigned char dst[1];

	KUNIT_EXPECT_EQ(test, maptbl_copy(NULL, dst), -EINVAL);
}

static void maptbl_test_maptbl_copy_fail_with_null_dst(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, NULL, &PNOBJ_FUNC(fake_copy_callback_void_return));

	test_2d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_copy(&test_2d_maptbl, NULL), -EINVAL);
}

static void maptbl_test_maptbl_copy_fail_if_maptbl_is_not_initialized(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl uninit_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, NULL, &PNOBJ_FUNC(fake_copy_callback_void_return));
	unsigned char dst[1];

	uninit_2d_maptbl.initialized = false;

	KUNIT_EXPECT_EQ(test, maptbl_copy(&uninit_2d_maptbl, dst), -EINVAL);
}

static void maptbl_test_maptbl_copy_fail_if_copy_callback_is_null(struct kunit *test)
{
	unsigned char test_2d_table[1][1];
	struct maptbl no_ops_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, NULL, NULL);
	unsigned char dst[1];

	no_ops_2d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_copy(&no_ops_2d_maptbl, dst), -EINVAL);
}

/* maptbl_fill() function test */
static void maptbl_test_maptbl_fill_fail_with_invalid_args(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl_pos target_pos = {
		.index = {
			[NDARR_1D] = 0,
			[NDARR_2D] = 2,
			[NDARR_3D] = 3,
			[NDARR_4D] = 4,
		},
	};
	unsigned char src[2] = { 0x11, 0x22 };

	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_fill(NULL, &target_pos, src, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_fill(&test_4d_maptbl, NULL, src, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_fill(&test_4d_maptbl, &target_pos, NULL, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, maptbl_fill(&test_4d_maptbl, &target_pos, src, 0), -EINVAL);
}

static void maptbl_test_maptbl_fill_fail_if_copy_size_is_greater_than_count_of_columns(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl_pos target_pos = {
		.index = {
			[NDARR_1D] = 0,
			[NDARR_2D] = 2,
			[NDARR_3D] = 3,
			[NDARR_4D] = 4,
		},
	};
	unsigned char src[2] = { 0x11, 0x22 };

	test_4d_maptbl.initialized = true;

	KUNIT_EXPECT_EQ(test, maptbl_fill(&test_4d_maptbl, &target_pos, src, 3), -EINVAL);
}

static void maptbl_test_maptbl_fill_success(struct kunit *test)
{
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl_pos target_pos = {
		.index = {
			[NDARR_1D] = 0,
			[NDARR_2D] = 2,
			[NDARR_3D] = 3,
			[NDARR_4D] = 4,
		},
	};
	unsigned char src[2] = { 0x11, 0x22 };

	test_4d_maptbl.initialized = true;

	memset(test_4d_table, 0, sizeof(test_4d_table));

	KUNIT_EXPECT_EQ(test, maptbl_fill(&test_4d_maptbl, &target_pos, src, sizeof(src)), 0);
	KUNIT_EXPECT_EQ(test, test_4d_table[4][3][2][0], (unsigned char)0x11);
	KUNIT_EXPECT_EQ(test, test_4d_table[4][3][2][1], (unsigned char)0x22);
}

/* maptbl_cmp_shape() function test */
static void maptbl_test_maptbl_cmp_shape_return_zero_if_both_maptbl_is_null(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, maptbl_cmp_shape(NULL, NULL), 0);
}

static void maptbl_test_maptbl_cmp_shape_return_plus_if_second_maptbl_is_null(struct kunit *test)
{
	struct maptbl test_maptbl;

	KUNIT_EXPECT_GT(test, maptbl_cmp_shape(&test_maptbl, NULL), 0);
}

static void maptbl_test_maptbl_cmp_shape_return_minus_if_first_maptbl_is_null(struct kunit *test)
{
	struct maptbl test_maptbl;

	KUNIT_EXPECT_LT(test, maptbl_cmp_shape(NULL, &test_maptbl), 0);
}

static void maptbl_test_maptbl_cmp_shape_return_zero_with_same_maptbl(struct kunit *test)
{
	struct maptbl test_maptbl;

	KUNIT_EXPECT_EQ(test, maptbl_cmp_shape(&test_maptbl, &test_maptbl), 0);
}

static void maptbl_test_maptbl_cmp_shape_return_plus_if_count_of_elements_of_high_dimension_is_bigger(struct kunit *test)
{
	unsigned char test_1x9x9x9_table[1][9][9][9];
	unsigned char test_9x1x1x1_table[9][1][1][1];
	struct maptbl test_1x9x9x9_maptbl = DEFINE_4D_MAPTBL(test_1x9x9x9_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_9x1x1x1_maptbl = DEFINE_4D_MAPTBL(test_9x1x1x1_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	unsigned char test_1x1x9x9_table[1][1][9][9];
	unsigned char test_1x9x1x1_table[1][9][1][1];
	struct maptbl test_1x1x9x9_maptbl = DEFINE_4D_MAPTBL(test_1x1x9x9_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_1x9x1x1_maptbl = DEFINE_4D_MAPTBL(test_1x9x1x1_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	unsigned char test_1x1x1x9_table[1][1][1][9];
	unsigned char test_1x1x9x1_table[1][1][9][1];
	struct maptbl test_1x1x1x9_maptbl = DEFINE_4D_MAPTBL(test_1x1x1x9_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_1x1x9x1_maptbl = DEFINE_4D_MAPTBL(test_1x1x9x1_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	KUNIT_EXPECT_GT(test, maptbl_cmp_shape(&test_9x1x1x1_maptbl, &test_1x9x9x9_maptbl), 0);
	KUNIT_EXPECT_EQ(test, maptbl_cmp_shape(&test_9x1x1x1_maptbl, &test_1x9x9x9_maptbl), (9 - 1));

	KUNIT_EXPECT_GT(test, maptbl_cmp_shape(&test_1x9x1x1_maptbl, &test_1x1x9x9_maptbl), 0);
	KUNIT_EXPECT_EQ(test, maptbl_cmp_shape(&test_1x9x1x1_maptbl, &test_1x1x9x9_maptbl), (9 - 1));

	KUNIT_EXPECT_GT(test, maptbl_cmp_shape(&test_1x1x9x1_maptbl, &test_1x1x1x9_maptbl), 0);
	KUNIT_EXPECT_EQ(test, maptbl_cmp_shape(&test_1x1x9x1_maptbl, &test_1x1x1x9_maptbl), (9 - 1));
}

static void maptbl_test_maptbl_cmp_shape_return_minus_if_count_of_elements_of_high_dimension_is_bigger(struct kunit *test)
{
	unsigned char test_1x9x9x9_table[1][9][9][9];
	unsigned char test_9x1x1x1_table[9][1][1][1];
	struct maptbl test_1x9x9x9_maptbl = DEFINE_4D_MAPTBL(test_1x9x9x9_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_9x1x1x1_maptbl = DEFINE_4D_MAPTBL(test_9x1x1x1_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	unsigned char test_1x1x9x9_table[1][1][9][9];
	unsigned char test_1x9x1x1_table[1][9][1][1];
	struct maptbl test_1x1x9x9_maptbl = DEFINE_4D_MAPTBL(test_1x1x9x9_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_1x9x1x1_maptbl = DEFINE_4D_MAPTBL(test_1x9x1x1_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	unsigned char test_1x1x1x9_table[1][1][1][9];
	unsigned char test_1x1x9x1_table[1][1][9][1];
	struct maptbl test_1x1x1x9_maptbl = DEFINE_4D_MAPTBL(test_1x1x1x9_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);
	struct maptbl test_1x1x9x1_maptbl = DEFINE_4D_MAPTBL(test_1x1x9x1_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_zero), NULL);

	KUNIT_EXPECT_LT(test, maptbl_cmp_shape(&test_1x9x9x9_maptbl, &test_9x1x1x1_maptbl), 0);
	KUNIT_EXPECT_EQ(test, maptbl_cmp_shape(&test_1x9x9x9_maptbl, &test_9x1x1x1_maptbl), (1 - 9));

	KUNIT_EXPECT_LT(test, maptbl_cmp_shape(&test_1x1x9x9_maptbl, &test_1x9x1x1_maptbl), 0);
	KUNIT_EXPECT_EQ(test, maptbl_cmp_shape(&test_1x1x9x9_maptbl, &test_1x9x1x1_maptbl), (1 - 9));

	KUNIT_EXPECT_LT(test, maptbl_cmp_shape(&test_1x1x1x9_maptbl, &test_1x1x9x1_maptbl), 0);
	KUNIT_EXPECT_EQ(test, maptbl_cmp_shape(&test_1x1x1x9_maptbl, &test_1x1x9x1_maptbl), (1 - 9));
}

/* maptbl_memcpy() function test */
static void maptbl_test_maptbl_memcpy_fail_with_null_maptbl(struct kunit *test)
{
	unsigned char src_2d_table[3][2];
	struct maptbl src_2d_maptbl = DEFINE_2D_MAPTBL(src_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	unsigned char dst_2d_table[3][2];
	struct maptbl dst_2d_maptbl = DEFINE_2D_MAPTBL(dst_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);

	memset(src_2d_table, 0, sizeof(src_2d_table));
	memset(dst_2d_table, -1, sizeof(dst_2d_table));

	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_2d_maptbl, NULL), (struct maptbl *)NULL);
	KUNIT_EXPECT_FALSE(test, !memcmp(src_2d_table, dst_2d_table, sizeof(dst_2d_table)));

	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(NULL, &src_2d_maptbl), (struct maptbl *)NULL);
	KUNIT_EXPECT_FALSE(test, !memcmp(src_2d_table, dst_2d_table, sizeof(dst_2d_table)));
}

static void maptbl_test_maptbl_memcpy_fail_with_empy_maptbl(struct kunit *test)
{
	unsigned char src_2d_table[3][2];
	struct maptbl src_2d_maptbl = DEFINE_2D_MAPTBL(src_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl src_0d_maptbl = DEFINE_0D_MAPTBL(src_0d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	unsigned char dst_2d_table[3][2];
	struct maptbl dst_2d_maptbl = DEFINE_2D_MAPTBL(dst_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl dst_0d_maptbl = DEFINE_0D_MAPTBL(dst_0d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);

	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_0d_maptbl, &src_2d_maptbl), (struct maptbl *)NULL);
	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_2d_maptbl, &src_0d_maptbl), (struct maptbl *)NULL);
	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_0d_maptbl, &src_0d_maptbl), (struct maptbl *)NULL);
}

static void maptbl_test_maptbl_memcpy_fail_with_different_shape_of_maptbl(struct kunit *test)
{
	unsigned char src_1d_table[1];
	unsigned char src_2d_table[3][1];
	unsigned char src_3d_table[4][3][1];
	unsigned char src_4d_table[5][4][3][1];
	struct maptbl src_1d_maptbl = DEFINE_1D_MAPTBL(src_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl src_2d_maptbl = DEFINE_2D_MAPTBL(src_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl src_3d_maptbl = DEFINE_3D_MAPTBL(src_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl src_4d_maptbl = DEFINE_4D_MAPTBL(src_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);

	unsigned char dst_1d_table[2];
	unsigned char dst_2d_table[3][2];
	unsigned char dst_3d_table[4][3][2];
	unsigned char dst_4d_table[5][4][3][2];
	struct maptbl dst_1d_maptbl = DEFINE_1D_MAPTBL(dst_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl dst_2d_maptbl = DEFINE_2D_MAPTBL(dst_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl dst_3d_maptbl = DEFINE_3D_MAPTBL(dst_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl dst_4d_maptbl = DEFINE_4D_MAPTBL(dst_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);

	memset(src_1d_table, 0, sizeof(src_1d_table));
	memset(src_2d_table, 0, sizeof(src_2d_table));
	memset(src_3d_table, 0, sizeof(src_3d_table));
	memset(src_4d_table, 0, sizeof(src_4d_table));

	memset(dst_1d_table, -1, sizeof(dst_1d_table));
	memset(dst_2d_table, -1, sizeof(dst_2d_table));
	memset(dst_3d_table, -1, sizeof(dst_3d_table));
	memset(dst_4d_table, -1, sizeof(dst_4d_table));

	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_1d_maptbl, &src_1d_maptbl), (struct maptbl *)NULL);
	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_2d_maptbl, &src_2d_maptbl), (struct maptbl *)NULL);
	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_3d_maptbl, &src_3d_maptbl), (struct maptbl *)NULL);
	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_4d_maptbl, &src_4d_maptbl), (struct maptbl *)NULL);
}

static void maptbl_test_maptbl_memcpy_success_with_same_shape_of_maptbl(struct kunit *test)
{
	unsigned char src_1d_table[2];
	unsigned char src_2d_table[3][2];
	unsigned char src_3d_table[4][3][2];
	unsigned char src_4d_table[5][4][3][2];
	struct maptbl src_1d_maptbl = DEFINE_1D_MAPTBL(src_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl src_2d_maptbl = DEFINE_2D_MAPTBL(src_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl src_3d_maptbl = DEFINE_3D_MAPTBL(src_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl src_4d_maptbl = DEFINE_4D_MAPTBL(src_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);

	unsigned char dst_1d_table[2];
	unsigned char dst_2d_table[3][2];
	unsigned char dst_3d_table[4][3][2];
	unsigned char dst_4d_table[5][4][3][2];
	struct maptbl dst_1d_maptbl = DEFINE_1D_MAPTBL(dst_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl dst_2d_maptbl = DEFINE_2D_MAPTBL(dst_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl dst_3d_maptbl = DEFINE_3D_MAPTBL(dst_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl dst_4d_maptbl = DEFINE_4D_MAPTBL(dst_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);

	memset(src_1d_table, 0, sizeof(src_1d_table));
	memset(src_2d_table, 0, sizeof(src_2d_table));
	memset(src_3d_table, 0, sizeof(src_3d_table));
	memset(src_4d_table, 0, sizeof(src_4d_table));

	memset(dst_1d_table, -1, sizeof(dst_1d_table));
	memset(dst_2d_table, -1, sizeof(dst_2d_table));
	memset(dst_3d_table, -1, sizeof(dst_3d_table));
	memset(dst_4d_table, -1, sizeof(dst_4d_table));

	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_1d_maptbl, &src_1d_maptbl), &dst_1d_maptbl);
	KUNIT_EXPECT_TRUE(test, !memcmp(src_1d_table, dst_1d_table, sizeof(dst_1d_table)));

	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_2d_maptbl, &src_2d_maptbl), &dst_2d_maptbl);
	KUNIT_EXPECT_TRUE(test, !memcmp(src_2d_table, dst_2d_table, sizeof(dst_2d_table)));

	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_3d_maptbl, &src_3d_maptbl), &dst_3d_maptbl);
	KUNIT_EXPECT_TRUE(test, !memcmp(src_3d_table, dst_3d_table, sizeof(dst_3d_table)));

	KUNIT_EXPECT_PTR_EQ(test, maptbl_memcpy(&dst_4d_maptbl, &src_4d_maptbl), &dst_4d_maptbl);
	KUNIT_EXPECT_TRUE(test, !memcmp(src_4d_table, dst_4d_table, sizeof(dst_4d_table)));
}

/* maptbl_snprintf_head() function test */
static void maptbl_test_maptbl_snprintf_head(struct kunit *test)
{
	unsigned char test_1d_table[2];
	unsigned char test_2d_table[3][2];
	unsigned char test_3d_table[4][3][2];
	unsigned char test_4d_table[5][4][3][2];
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	char *buf1, *buf2;
	int len1, len2;

	buf1 = kunit_kzalloc(test, 256, GFP_KERNEL);
	buf2 = kunit_kzalloc(test, 256, GFP_KERNEL);

	len1 = maptbl_snprintf_head(&test_1d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "%dD-MAPTBL:%s[%ld]\n",
			1, maptbl_get_name(&test_1d_maptbl), ARRAY_SIZE(test_1d_table));
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf_head(&test_2d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "%dD-MAPTBL:%s[%ld][%ld]\n",
			2, maptbl_get_name(&test_2d_maptbl), ARRAY_SIZE(test_2d_table),
			ARRAY_SIZE(test_2d_table[0]));
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf_head(&test_3d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "%dD-MAPTBL:%s[%ld][%ld][%ld]\n",
			3, maptbl_get_name(&test_3d_maptbl), ARRAY_SIZE(test_3d_table),
			ARRAY_SIZE(test_3d_table[0]), ARRAY_SIZE(test_3d_table[0][0]));
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf_head(&test_4d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "%dD-MAPTBL:%s[%ld][%ld][%ld][%ld]\n",
			4, maptbl_get_name(&test_4d_maptbl), ARRAY_SIZE(test_4d_table),
			ARRAY_SIZE(test_4d_table[0]), ARRAY_SIZE(test_4d_table[0][0]),
			ARRAY_SIZE(test_4d_table[0][0][0]));
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);
}

static void maptbl_test_maptbl_snprintf_body(struct kunit *test)
{
	unsigned char test_1d_table[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
	unsigned char test_2d_table[2][1] = { { 0xAA }, { 0xBB } };
	unsigned char test_3d_table[2][1][1] = { { { 0xAA } }, { { 0xBB } } };
	unsigned char test_4d_table[2][1][1][1] = { { { { 0xAA } } }, { { { 0xBB } } } };
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	char *buf1, *buf2;
	int len1, len2;

	buf1 = kunit_kzalloc(test, 256, GFP_KERNEL);
	buf2 = kunit_kzalloc(test, 256, GFP_KERNEL);

	len1 = maptbl_snprintf_body(&test_1d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "\n"
			"01 02 03 04 05 06 07 08\n"
			"10 20 30 40 50 60 70 80\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf_body(&test_2d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256,
			"\n"
			"[0] = {\n"
				"\tAA\n"
			"},\n"
			"[1] = {\n"
				"\tBB\n"
			"},\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf_body(&test_3d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256,
			"\n"
			"[0] = {\n"
				"\t[0] = {\n"
					"\t\tAA\n"
				"\t},\n"
			"},\n"
			"[1] = {\n"
				"\t[0] = {\n"
					"\t\tBB\n"
				"\t},\n"
			"},\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf_body(&test_4d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256,
			"\n"
			"[0] = {\n"
				"\t[0] = {\n"
					"\t\t[0] = {\n"
						"\t\t\tAA\n"
					"\t\t},\n"
				"\t},\n"
			"},\n"
			"[1] = {\n"
				"\t[0] = {\n"
					"\t\t[0] = {\n"
						"\t\t\tBB\n"
					"\t\t},\n"
				"\t},\n"
			"},\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);
}

static void maptbl_test_maptbl_snprintf_tail(struct kunit *test)
{
	unsigned char test_4d_table[2][1][1][1] = { { { { 0xAA } } }, { { { 0xBB } } } };
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	char *buf1, *buf2;
	int len1, len2;

	buf1 = kunit_kzalloc(test, 256, GFP_KERNEL);
	buf2 = kunit_kzalloc(test, 256, GFP_KERNEL);

	len1 = maptbl_snprintf_tail(&test_4d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "=====================================\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);
}

static void maptbl_test_maptbl_snprintf(struct kunit *test)
{
	unsigned char test_1d_table[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
	unsigned char test_2d_table[2][1] = { { 0xAA }, { 0xBB } };
	unsigned char test_3d_table[2][1][1] = { { { 0xAA } }, { { 0xBB } } };
	unsigned char test_4d_table[2][1][1][1] = { { { { 0xAA } } }, { { { 0xBB } } } };
	struct maptbl test_1d_maptbl = DEFINE_1D_MAPTBL(test_1d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_2d_maptbl = DEFINE_2D_MAPTBL(test_2d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_3d_maptbl = DEFINE_3D_MAPTBL(test_3d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	struct maptbl test_4d_maptbl = DEFINE_4D_MAPTBL(test_4d_table,
			NULL, &PNOBJ_FUNC(fake_callback_return_value), NULL);
	char *buf1, *buf2;
	int len1, len2;

	buf1 = kunit_kzalloc(test, 256, GFP_KERNEL);
	buf2 = kunit_kzalloc(test, 256, GFP_KERNEL);

	len1 = maptbl_snprintf(&test_1d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "%dD-MAPTBL:%s[%ld]\n",
			1, maptbl_get_name(&test_1d_maptbl), ARRAY_SIZE(test_1d_table));
	len2 += snprintf(buf2 + len2, 256 - len2, "\n"
			"01 02 03 04 05 06 07 08\n"
			"10 20 30 40 50 60 70 80\n");
	len2 += snprintf(buf2 + len2, 256 - len2, "=====================================\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf(&test_2d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "%dD-MAPTBL:%s[%ld][%ld]\n",
			2, maptbl_get_name(&test_2d_maptbl), ARRAY_SIZE(test_2d_table),
			ARRAY_SIZE(test_2d_table[0]));
	len2 += snprintf(buf2 + len2, 256 - len2,
			"\n"
			"[0] = {\n"
				"\tAA\n"
			"},\n"
			"[1] = {\n"
				"\tBB\n"
			"},\n");
	len2 += snprintf(buf2 + len2, 256 - len2, "=====================================\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf(&test_3d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "%dD-MAPTBL:%s[%ld][%ld][%ld]\n",
			3, maptbl_get_name(&test_3d_maptbl), ARRAY_SIZE(test_3d_table),
			ARRAY_SIZE(test_3d_table[0]), ARRAY_SIZE(test_3d_table[0][0]));
	len2 += snprintf(buf2 + len2, 256 - len2,
			"\n"
			"[0] = {\n"
				"\t[0] = {\n"
					"\t\tAA\n"
				"\t},\n"
			"},\n"
			"[1] = {\n"
				"\t[0] = {\n"
					"\t\tBB\n"
				"\t},\n"
			"},\n");
	len2 += snprintf(buf2 + len2, 256 - len2, "=====================================\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);

	len1 = maptbl_snprintf(&test_4d_maptbl, buf1, 256);
	len2 = snprintf(buf2, 256, "%dD-MAPTBL:%s[%ld][%ld][%ld][%ld]\n",
			4, maptbl_get_name(&test_4d_maptbl), ARRAY_SIZE(test_4d_table),
			ARRAY_SIZE(test_4d_table[0]), ARRAY_SIZE(test_4d_table[0][0]),
			ARRAY_SIZE(test_4d_table[0][0][0]));
	len2 += snprintf(buf2 + len2, 256 - len2,
			"\n"
			"[0] = {\n"
				"\t[0] = {\n"
					"\t\t[0] = {\n"
						"\t\t\tAA\n"
					"\t\t},\n"
				"\t},\n"
			"},\n"
			"[1] = {\n"
				"\t[0] = {\n"
					"\t\t[0] = {\n"
						"\t\t\tBB\n"
					"\t\t},\n"
				"\t},\n"
			"},\n");
	len2 += snprintf(buf2 + len2, 256 - len2, "=====================================\n");
	KUNIT_EXPECT_EQ(test, len1, len2);
	KUNIT_EXPECT_STREQ(test, buf1, buf2);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int maptbl_test_init(struct kunit *test)
{
	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void maptbl_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case maptbl_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(maptbl_test_test),

	KUNIT_CASE(maptbl_test_maptbl_alloc_buffer_should_not_allocate_with_passing_null_pointer_or_zero),
	KUNIT_CASE(maptbl_test_maptbl_alloc_buffer_success),

	KUNIT_CASE(maptbl_test_maptbl_free_buffer_should_not_occur_exception_with_passing_null_pointer),
	KUNIT_CASE(maptbl_test_maptbl_free_buffer_success),

	KUNIT_CASE(maptbl_test_maptbl_set_shape_fail_with_invalid_argument),
	KUNIT_CASE(maptbl_test_maptbl_set_shape_success),

	KUNIT_CASE(maptbl_test_maptbl_set_ops_fail_with_invalid_argument),
	KUNIT_CASE(maptbl_test_maptbl_set_ops_copy_operation_success),

	KUNIT_CASE(maptbl_test_maptbl_set_initialized_fail_with_invalid_argument),
	KUNIT_CASE(maptbl_test_maptbl_set_initialized_sholud_update_initlaized_flag),

	KUNIT_CASE(maptbl_test_maptbl_private_data_setter_and_getter_fail_with_invalid_argument),
	KUNIT_CASE(maptbl_test_maptbl_private_data_setter_and_getter_success),

	KUNIT_CASE(maptbl_test_maptbl_create_should_create_empty_maptbl_with_no_name_and_shape),
	KUNIT_CASE(maptbl_test_maptbl_destroy_should_not_occur_exception_with_null_pointer_or_empty_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_create_should_allocate_as_shape_and_copy_init_data),
	KUNIT_CASE(maptbl_test_dynamic_allocated_maptbl_should_be_same_with_maptbl_is_defined_by_macro),

	KUNIT_CASE(maptbl_test_maptbl_clone_fail_with_invalid_args),
	KUNIT_CASE(maptbl_test_maptbl_clone_success_cloning_empty_maptbl),
	KUNIT_CASE(maptbl_test_cloned_maptbl_should_be_same_with_original_maptbl),

	KUNIT_CASE(maptbl_test_maptbl_deepcopy_fail_with_invalid_args),
	KUNIT_CASE(maptbl_test_maptbl_deepcopy_should_free_array_buffer_if_source_maptbl_array_is_null),
	KUNIT_CASE(maptbl_test_maptbl_deepcopy_success_if_passing_same_maptbl_pointer),
	KUNIT_CASE(maptbl_test_dst_maptbl_array_buffer_should_be_free_and_allocated),
	KUNIT_CASE(maptbl_test_deep_copied_maptbl_should_be_same_with_original_maptbl),

	KUNIT_CASE(maptbl_test_maptbl_for_each_iteration_count_should_be_same_with_size_of_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_for_each_reverse_iteration_count_should_be_same_with_size_of_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_for_each_reverse_with_unsigned_int_variable),

	KUNIT_CASE(maptbl_test_maptbl_for_each_dimen_iteration_count_should_be_same_with_number_of_dimensions),
	KUNIT_CASE(maptbl_test_maptbl_for_each_dimen_reverse_iteration_count_should_be_same_with_number_of_dimensions),
	KUNIT_CASE(maptbl_test_maptbl_for_each_dimen_reverse_with_unsigned_int_variable),

	KUNIT_CASE(maptbl_test_maptbl_for_each_n_dimen_element_iteration_count_should_be_same_with_number_of_elements),
	KUNIT_CASE(maptbl_test_maptbl_for_each_n_dimen_element_reverse_iteration_count_should_be_same_with_number_of_elements),
	KUNIT_CASE(maptbl_test_maptbl_for_each_n_dimen_element_reverse_with_unsigned_int_variable),

	KUNIT_CASE(maptbl_test_maptbl_get_indexof_fail_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_get_indexof_fail_if_index_exceed_countof_each_dimension),
	KUNIT_CASE(maptbl_test_maptbl_get_indexof_success_if_index_less_than_countof_each_dimension),

	KUNIT_CASE(maptbl_test_maptbl_index_fail_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_index_should_be_sum_of_index_of_each_dimensions),

	KUNIT_CASE(maptbl_test_maptbl_4d_index_fail_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_4d_index_should_be_sum_of_index_of_each_dimensions),

	KUNIT_CASE(maptbl_test_sizeof_box_should_be_same_with_size_of_3d_table_inside_of_n_dimension_table),
	KUNIT_CASE(maptbl_test_sizeof_layer_should_be_same_with_size_of_2d_table_inside_of_n_dimension_table),
	KUNIT_CASE(maptbl_test_sizeof_row_should_be_same_with_size_of_1d_table_inside_of_n_dimension_table),
	KUNIT_CASE(maptbl_test_sizeof_maptbl_should_be_same_with_size_of_table),

	KUNIT_CASE(maptbl_test_maptbl_pos_to_index_fail_with_invalid_args),
	KUNIT_CASE(maptbl_test_maptbl_pos_to_index_success),

	KUNIT_CASE(maptbl_test_maptbl_index_to_pos_fail_with_invalid_args),
	KUNIT_CASE(maptbl_test_maptbl_index_to_pos_fail_if_index_is_out_of_maptbl_array_bound),
	KUNIT_CASE(maptbl_test_maptbl_index_to_pos_success),

	KUNIT_CASE(maptbl_test_maptbl_is_initialized_false_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_is_initialized_true_if_initialized),

	KUNIT_CASE(maptbl_test_maptbl_is_index_in_bound_false_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_is_index_in_bound_false_if_index_exceed_sizeof_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_is_index_in_bound_true_if_index_less_than_sizeof_maptbl),

	KUNIT_CASE(maptbl_test_maptbl_init_fail_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_init_fail_if_init_callback_is_null),
	KUNIT_CASE(maptbl_test_maptbl_init_fail_if_init_callback_return_error),
	KUNIT_CASE(maptbl_test_maptbl_init_success_if_init_callback_return_zero),

	KUNIT_CASE(maptbl_test_maptbl_getidx_fail_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_getidx_fail_if_maptbl_is_not_initialized),
	KUNIT_CASE(maptbl_test_maptbl_getidx_fail_if_getidx_callback_is_null),
	KUNIT_CASE(maptbl_test_maptbl_getidx_fail_if_getidx_callback_return_error),
	KUNIT_CASE(maptbl_test_maptbl_getidx_fail_if_getidx_callback_return_out_of_maptbl_range_index),
	KUNIT_CASE(maptbl_test_maptbl_getidx_success_if_getidx_callback_return_zero),

	KUNIT_CASE(maptbl_test_maptbl_copy_fail_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_copy_fail_with_null_dst),
	KUNIT_CASE(maptbl_test_maptbl_copy_fail_if_maptbl_is_not_initialized),
	KUNIT_CASE(maptbl_test_maptbl_copy_fail_if_copy_callback_is_null),

	KUNIT_CASE(maptbl_test_maptbl_fill_fail_with_invalid_args),
	KUNIT_CASE(maptbl_test_maptbl_fill_fail_if_copy_size_is_greater_than_count_of_columns),
	KUNIT_CASE(maptbl_test_maptbl_fill_success),

	KUNIT_CASE(maptbl_test_maptbl_cmp_shape_return_zero_if_both_maptbl_is_null),
	KUNIT_CASE(maptbl_test_maptbl_cmp_shape_return_plus_if_second_maptbl_is_null),
	KUNIT_CASE(maptbl_test_maptbl_cmp_shape_return_minus_if_first_maptbl_is_null),
	KUNIT_CASE(maptbl_test_maptbl_cmp_shape_return_zero_with_same_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_cmp_shape_return_plus_if_count_of_elements_of_high_dimension_is_bigger),
	KUNIT_CASE(maptbl_test_maptbl_cmp_shape_return_minus_if_count_of_elements_of_high_dimension_is_bigger),

	KUNIT_CASE(maptbl_test_maptbl_memcpy_fail_with_null_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_memcpy_fail_with_empy_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_memcpy_fail_with_different_shape_of_maptbl),
	KUNIT_CASE(maptbl_test_maptbl_memcpy_success_with_same_shape_of_maptbl),

	KUNIT_CASE(maptbl_test_maptbl_snprintf_head),
	KUNIT_CASE(maptbl_test_maptbl_snprintf_body),
	KUNIT_CASE(maptbl_test_maptbl_snprintf_tail),
	KUNIT_CASE(maptbl_test_maptbl_snprintf),
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
static struct kunit_suite maptbl_test_module = {
	.name = "maptbl_test",
	.init = maptbl_test_init,
	.exit = maptbl_test_exit,
	.test_cases = maptbl_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&maptbl_test_module);

MODULE_LICENSE("GPL v2");
