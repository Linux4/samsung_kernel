// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#include <kunit/test.h>

#include <linux/samsung/builder_pattern.h>

struct test_builder_pattern {
	struct builder bd;
	unsigned long result_parse_dt;
	unsigned long result_construct_dev;
	unsigned long result_destruct_dev;
};

#define DEFINE_TEST_PARSE_DT(__bit) \
static int __test_bdp_parse_dt_bit_##__bit(struct builder *bd, \
		struct device_node *np) \
{ \
	struct test_builder_pattern *drvdata = \
		container_of(bd, struct test_builder_pattern, bd); \
	\
	drvdata->result_parse_dt |= BIT(__bit); \
	\
	return 0; \
}

DEFINE_TEST_PARSE_DT(0);
DEFINE_TEST_PARSE_DT(1);
DEFINE_TEST_PARSE_DT(2);
DEFINE_TEST_PARSE_DT(3);
DEFINE_TEST_PARSE_DT(4);

static int __test_bdp_parse_dt_always_fail(struct builder *bd,
		struct device_node *np)
{
	return -ENODEV;
}

static struct dt_builder __test_bdp_dt_builder_case_0[] = {
	DT_BUILDER(__test_bdp_parse_dt_bit_0),
	DT_BUILDER(__test_bdp_parse_dt_bit_1),
	DT_BUILDER(__test_bdp_parse_dt_bit_2),
	DT_BUILDER(__test_bdp_parse_dt_bit_3),
	DT_BUILDER(__test_bdp_parse_dt_bit_4),
};

static void test_case_0_sec_director_parse_dt(struct kunit *test)
{
	struct test_builder_pattern *drvdata;
	int err;

	drvdata = kunit_kzalloc(test, sizeof(*drvdata), GFP_KERNEL);

	err = sec_director_parse_dt(&drvdata->bd,
			__test_bdp_dt_builder_case_0,
			ARRAY_SIZE(__test_bdp_dt_builder_case_0));

	KUNIT_EXPECT_EQ(test, err, 0);
	KUNIT_EXPECT_EQ(test, drvdata->result_parse_dt, 0x1FUL);

	kunit_kfree(test, drvdata);
}

static struct dt_builder __test_bdp_dt_builder_case_1[] = {
	DT_BUILDER(__test_bdp_parse_dt_bit_0),
	DT_BUILDER(__test_bdp_parse_dt_bit_1),
	DT_BUILDER(__test_bdp_parse_dt_always_fail),
	DT_BUILDER(__test_bdp_parse_dt_bit_3),
	DT_BUILDER(__test_bdp_parse_dt_bit_4),
};

static void test_case_1_sec_director_parse_dt(struct kunit *test)
{
	struct test_builder_pattern *drvdata;
	int err;

	drvdata = kunit_kzalloc(test, sizeof(*drvdata), GFP_KERNEL);

	err = sec_director_parse_dt(&drvdata->bd,
			__test_bdp_dt_builder_case_1,
			ARRAY_SIZE(__test_bdp_dt_builder_case_1));

	KUNIT_EXPECT_EQ(test, err, -ENODEV);
	KUNIT_EXPECT_EQ(test, drvdata->result_parse_dt, 0x03UL);

	kunit_kfree(test, drvdata);
}

#define DEFINE_TEST_CONSTRUCT_DEV(__bit) \
static int __test_bdp_construct_dev_bit_##__bit(struct builder *bd) \
{ \
	struct test_builder_pattern *drvdata = \
		container_of(bd, struct test_builder_pattern, bd); \
	\
	drvdata->result_construct_dev |= BIT(__bit); \
	\
	return 0; \
}

DEFINE_TEST_CONSTRUCT_DEV(0);
DEFINE_TEST_CONSTRUCT_DEV(1);
DEFINE_TEST_CONSTRUCT_DEV(2);
DEFINE_TEST_CONSTRUCT_DEV(3);
DEFINE_TEST_CONSTRUCT_DEV(4);

#define DEFINE_TEST_DESTRUCT_DEV(__bit) \
static void __test_bdp_destruct_dev_bit_##__bit(struct builder *bd) \
{ \
	struct test_builder_pattern *drvdata = \
		container_of(bd, struct test_builder_pattern, bd); \
	\
	drvdata->result_destruct_dev |= BIT(__bit); \
}

// DEFINE_TEST_DESTRUCT_DEV(0);
DEFINE_TEST_DESTRUCT_DEV(1);
// DEFINE_TEST_DESTRUCT_DEV(2);
DEFINE_TEST_DESTRUCT_DEV(3);
DEFINE_TEST_DESTRUCT_DEV(4);

static struct dev_builder __test_bdp_dev_builder_test_case_0[] = {
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_0, NULL),
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_1,
		       __test_bdp_destruct_dev_bit_1),
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_2, NULL),
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_3,
		       __test_bdp_destruct_dev_bit_3),
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_4,
		       __test_bdp_destruct_dev_bit_4),
};

static void test_case_0_sec_director_construct_dev(struct kunit *test)
{
	struct test_builder_pattern *drvdata;
	ssize_t last_failed;
	int err;

	drvdata = kunit_kzalloc(test, sizeof(*drvdata), GFP_KERNEL);

	err = sec_director_construct_dev(&drvdata->bd,
			__test_bdp_dev_builder_test_case_0,
			ARRAY_SIZE(__test_bdp_dev_builder_test_case_0),
			&last_failed);

	KUNIT_EXPECT_EQ(test, last_failed,
			(ssize_t)ARRAY_SIZE(__test_bdp_dev_builder_test_case_0));
	KUNIT_EXPECT_EQ(test, drvdata->result_construct_dev, 0x1FUL);

	kunit_kfree(test, drvdata);
}

static void test_case_0_sec_director_destruct_dev(struct kunit *test)
{
	struct test_builder_pattern *drvdata;

	drvdata = kunit_kzalloc(test, sizeof(*drvdata), GFP_KERNEL);

	sec_director_destruct_dev(&drvdata->bd,
			__test_bdp_dev_builder_test_case_0,
			ARRAY_SIZE(__test_bdp_dev_builder_test_case_0),
			ARRAY_SIZE(__test_bdp_dev_builder_test_case_0));

	KUNIT_EXPECT_EQ(test, drvdata->result_destruct_dev, 0x1AUL);

	kunit_kfree(test, drvdata);
}

static int __test_bdp_construct_dev_always_fail(struct builder *bd)
{
	return -EBUSY;
}

static struct dev_builder __test_bdp_dev_builder_test_case_1[] = {
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_0, NULL),
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_1, NULL),
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_2, NULL),
	DEVICE_BUILDER(__test_bdp_construct_dev_always_fail, NULL),
	DEVICE_BUILDER(__test_bdp_construct_dev_bit_4, NULL),
};

static void test_case_1_sec_director_construct_dev(struct kunit *test)
{
	struct test_builder_pattern *drvdata;
	ssize_t last_failed;
	int err;

	drvdata = kunit_kzalloc(test, sizeof(*drvdata), GFP_KERNEL);

	err = sec_director_construct_dev(&drvdata->bd,
			__test_bdp_dev_builder_test_case_1,
			ARRAY_SIZE(__test_bdp_dev_builder_test_case_1),
			&last_failed);

	KUNIT_EXPECT_EQ(test, last_failed, (ssize_t)-3);
	KUNIT_EXPECT_EQ(test, drvdata->result_construct_dev, 0x07UL);

	kunit_kfree(test, drvdata);
}

static struct kunit_case builder_pattern_test_cases[] = {
	KUNIT_CASE(test_case_0_sec_director_parse_dt),
	KUNIT_CASE(test_case_1_sec_director_parse_dt),
	KUNIT_CASE(test_case_0_sec_director_construct_dev),
	KUNIT_CASE(test_case_0_sec_director_destruct_dev),
	KUNIT_CASE(test_case_1_sec_director_construct_dev),
	{}
};

static struct kunit_suite builder_pattern_test_suite = {
	.name = "SEC Builder Pattern",
	.test_cases = builder_pattern_test_cases,
};

kunit_test_suites(&builder_pattern_test_suite);
