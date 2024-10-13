// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/random.h>

#include <linux/samsung/sec_of_kunit.h>

#include "sec_reloc_gpio_test.h"

static void test__reloc_gpio_parse_dt_reloc_base(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct reloc_gpio_drvdata *drvdata;
	int err;

	err = __reloc_gpio_parse_dt_reloc_base(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct reloc_gpio_drvdata, bd);

	KUNIT_EXPECT_EQ(test, 2, drvdata->nr_chip);
	KUNIT_EXPECT_EQ(test, 0, drvdata->chip[0].base);
	KUNIT_EXPECT_EQ(test, 256, drvdata->chip[1].base);
}

static void test__reloc_gpio_parse_dt_gpio_label(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct reloc_gpio_drvdata *drvdata;
	int err;

	err = __reloc_gpio_parse_dt_gpio_label(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct reloc_gpio_drvdata, bd);

	KUNIT_EXPECT_STREQ(test, "sec,mock-gpio-0", drvdata->chip[0].label);
	KUNIT_EXPECT_STREQ(test, "sec,mock-gpio-1", drvdata->chip[1].label);
}

static void __test__reloc_gpio_is_valid_gpio_num_n(struct kunit *test, int chip_idx)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct reloc_gpio_drvdata *drvdata = container_of(testdata->bd,
			struct reloc_gpio_drvdata, bd);
	struct reloc_gpio_chip *chip = &drvdata->chip[chip_idx];
	const int nr_gpio = 128;
	bool is_valid;

	is_valid = __reloc_gpio_is_valid_gpio_num(chip, nr_gpio, chip->base - 1);
	KUNIT_EXPECT_FALSE(test, is_valid);

	is_valid = __reloc_gpio_is_valid_gpio_num(chip, nr_gpio, chip->base);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __reloc_gpio_is_valid_gpio_num(chip, nr_gpio, chip->base + 1);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __reloc_gpio_is_valid_gpio_num(chip, nr_gpio, chip->base + nr_gpio - 1);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __reloc_gpio_is_valid_gpio_num(chip, nr_gpio, chip->base + nr_gpio);
	KUNIT_EXPECT_FALSE(test, is_valid);
}

static void test__reloc_gpio_is_valid_gpio_num_0(struct kunit *test)
{
	__test__reloc_gpio_is_valid_gpio_num_n(test, 0);
}

static void test__reloc_gpio_is_valid_gpio_num_1(struct kunit *test)
{
	__test__reloc_gpio_is_valid_gpio_num_n(test, 1);
}

static struct reloc_gpio_chip *__test__reloc_gpio_is_matched_n(
		struct kunit *test, int chip_idx, struct gpio_chip *gc)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct reloc_gpio_drvdata *drvdata = container_of(testdata->bd,
			struct reloc_gpio_drvdata, bd);
	struct reloc_gpio_chip *chip = &drvdata->chip[chip_idx];
	const int nr_gpio = 128;
	bool is_valid;

	gc->label = kstrdup(chip->label, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, gc->label);
	gc->ngpio = nr_gpio;

	is_valid = __reloc_gpio_is_matched(gc, chip, chip->base);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __reloc_gpio_is_matched(gc, chip, chip->base + 1);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __reloc_gpio_is_matched(gc, chip, chip->base + nr_gpio - 1);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __reloc_gpio_is_matched(gc, chip, chip->base + nr_gpio);
	KUNIT_EXPECT_FALSE(test, is_valid);

	kfree(gc->label);
	gc->label = NULL;

	return chip;
}

static void test__reloc_gpio_is_matched_0(struct kunit *test)
{
	struct reloc_gpio_chip *chip;
	const int any_number = __INT_MAX__;
	struct gpio_chip *gc;
	bool is_valid;

	gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);
	chip = __test__reloc_gpio_is_matched_n(test, 0, gc);

	gc->label = "sec,mock-gpio-";
	is_valid = __reloc_gpio_is_matched(gc, chip, any_number);
	KUNIT_EXPECT_FALSE(test, is_valid);

	gc->label = "sec,mock-gpio-01";
	is_valid = __reloc_gpio_is_matched(gc, chip, any_number);
	KUNIT_EXPECT_FALSE(test, is_valid);
}

static void test__reloc_gpio_is_matched_1(struct kunit *test)
{
	struct reloc_gpio_chip *chip;
	const int any_number = __INT_MAX__;
	struct gpio_chip *gc;
	bool is_valid;

	gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);
	chip = __test__reloc_gpio_is_matched_n(test, 1, gc);

	gc->label = "sec,mock-gpio-";
	is_valid = __reloc_gpio_is_matched(gc, chip, any_number);
	KUNIT_EXPECT_FALSE(test, is_valid);

	gc->label = "sec,mock-gpio-02";
	is_valid = __reloc_gpio_is_matched(gc, chip, any_number);
	KUNIT_EXPECT_FALSE(test, is_valid);
}

static void __test_sec_reloc_gpio_is_matched_gpio_chip_n(struct kunit *test,
		int chip_idx, struct gpio_chip *gc)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct reloc_gpio_drvdata *drvdata = container_of(testdata->bd,
			struct reloc_gpio_drvdata, bd);
	struct reloc_gpio_chip *chip = &drvdata->chip[chip_idx];
	int is_valid;

	drvdata->gpio_num = chip->base - 1;
	is_valid = sec_reloc_gpio_is_matched_gpio_chip(gc, drvdata);
	KUNIT_EXPECT_EQ(test, 0, is_valid);

	drvdata->gpio_num = chip->base;
	is_valid = sec_reloc_gpio_is_matched_gpio_chip(gc, drvdata);
	KUNIT_EXPECT_EQ(test, 1, is_valid);

	drvdata->gpio_num = chip->base + 1;
	is_valid = sec_reloc_gpio_is_matched_gpio_chip(gc, drvdata);
	KUNIT_EXPECT_EQ(test, 1, is_valid);

	drvdata->gpio_num = chip->base + gc->ngpio - 1;
	is_valid = sec_reloc_gpio_is_matched_gpio_chip(gc, drvdata);
	KUNIT_EXPECT_EQ(test, 1, is_valid);

	drvdata->gpio_num = chip->base + gc->ngpio;
	is_valid = sec_reloc_gpio_is_matched_gpio_chip(gc, drvdata);
	KUNIT_EXPECT_EQ(test, 0, is_valid);
}

static void test_sec_reloc_gpio_is_matched_gpio_chip_0(struct kunit *test)
{
	const int nr_gpio = 128;
	struct gpio_chip *gc;

	gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);

	gc->ngpio = nr_gpio;
	gc->label = "sec,mock-gpio-0";

	__test_sec_reloc_gpio_is_matched_gpio_chip_n(test, 0, gc);
}

static void test_sec_reloc_gpio_is_matched_gpio_chip_1(struct kunit *test)
{
	const int nr_gpio = 64;
	struct gpio_chip *gc;

	gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);

	gc->ngpio = nr_gpio;
	gc->label = "sec,mock-gpio-1";

	__test_sec_reloc_gpio_is_matched_gpio_chip_n(test, 1, gc);
}

static void test__reloc_gpio_from_legacy_number_n(struct kunit *test,
		int chip_idx, struct gpio_chip *gc)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct reloc_gpio_drvdata *drvdata = container_of(testdata->bd,
			struct reloc_gpio_drvdata, bd);
	struct reloc_gpio_chip *chip = &drvdata->chip[chip_idx];
	int n;

	drvdata->chip_idx_found = -1;
	n = __reloc_gpio_from_legacy_number(drvdata, gc);
	KUNIT_EXPECT_EQ(test, -EINVAL, n);

	drvdata->chip_idx_found = chip_idx;

	drvdata->gpio_num = chip->base - 1;
	n = __reloc_gpio_from_legacy_number(drvdata, gc);
	KUNIT_EXPECT_EQ(test, -ERANGE, n);

	drvdata->gpio_num = chip->base;
	n = __reloc_gpio_from_legacy_number(drvdata, gc);
	KUNIT_EXPECT_EQ(test, gc->base, n);

	drvdata->gpio_num = chip->base + gc->ngpio - 1;
	n = __reloc_gpio_from_legacy_number(drvdata, gc);
	KUNIT_EXPECT_EQ(test, gc->base + gc->ngpio - 1, n);

	drvdata->gpio_num = chip->base + gc->ngpio;
	n = __reloc_gpio_from_legacy_number(drvdata, gc);
	KUNIT_EXPECT_EQ(test, -ERANGE, n);
}

static void test__reloc_gpio_from_legacy_number_0(struct kunit *test)
{
	const int nr_gpio = 128;
	struct gpio_chip *gc;

	gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);

	gc->base = get_random_u32() % 512;
	gc->ngpio = nr_gpio;

	test__reloc_gpio_from_legacy_number_n(test, 0, gc);
}

static void test__reloc_gpio_from_legacy_number_1(struct kunit *test)
{
	const int nr_gpio = 64;
	struct gpio_chip *gc;

	gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);

	gc->base = get_random_u32() % 512;
	gc->ngpio = nr_gpio;

	test__reloc_gpio_from_legacy_number_n(test, 1, gc);
}

static struct sec_of_kunit_data *sec_reloc_gpio_testdata;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_reloc_gpio_test);
static SEC_OF_KUNIT_DTB_INFO(sec_reloc_gpio_test);

static int __reloc_gpio_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct reloc_gpio_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,reloc_gpio_test", &drvdata->bd,
			"samsung,reloc_gpio", &sec_reloc_gpio_test_info);
}

static void __reloc_gpio_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct reloc_gpio_drvdata *drvdata = container_of(testdata->bd,
			struct reloc_gpio_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int sec_reloc_gpio_test_case_init(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_reloc_gpio_testdata;

	if (!testdata) {
		int err;

		testdata = kzalloc(sizeof(*testdata), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testdata);

		err = __reloc_gpio_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		sec_reloc_gpio_testdata = testdata;
	}

	test->priv = testdata;

	return 0;
}

static void tear_down_sec_reloc_gpio_test(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_reloc_gpio_testdata;

	__reloc_gpio_test_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_reloc_gpio_testdata = NULL;
}

static struct kunit_case sec_reloc_gpio_test_cases[] = {
	KUNIT_CASE(test__reloc_gpio_parse_dt_reloc_base),
	KUNIT_CASE(test__reloc_gpio_parse_dt_gpio_label),
	KUNIT_CASE(test__reloc_gpio_is_valid_gpio_num_0),
	KUNIT_CASE(test__reloc_gpio_is_valid_gpio_num_1),
	KUNIT_CASE(test__reloc_gpio_is_matched_0),
	KUNIT_CASE(test__reloc_gpio_is_matched_1),
	KUNIT_CASE(test_sec_reloc_gpio_is_matched_gpio_chip_0),
	KUNIT_CASE(test_sec_reloc_gpio_is_matched_gpio_chip_1),
	KUNIT_CASE(test__reloc_gpio_from_legacy_number_0),
	KUNIT_CASE(test__reloc_gpio_from_legacy_number_1),
	KUNIT_CASE(tear_down_sec_reloc_gpio_test),
	{},
};

struct kunit_suite sec_reloc_gpio_test_suite = {
	.name = "sec_reloc_gpio_test",
	.init = sec_reloc_gpio_test_case_init,
	.test_cases = sec_reloc_gpio_test_cases,
};

kunit_test_suites(&sec_reloc_gpio_test_suite);
