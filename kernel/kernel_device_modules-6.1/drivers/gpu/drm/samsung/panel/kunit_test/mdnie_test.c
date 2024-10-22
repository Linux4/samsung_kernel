// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel.h"
#include "mdnie.h"

struct mdnie_test {
	struct mdnie_info *mdnie;
	struct mdnie_tune *mdnie_tune;
};

struct mdnie_prop_test_param {
	char *prop_name;
	unsigned int *prop_ptr;
	unsigned int value;
};

#define TEST_MDNIE_TUNE_LINE_0_NUM	(-56)
#define TEST_MDNIE_TUNE_LINE_0_DEN	(55)
#define TEST_MDNIE_TUNE_LINE_0_CON	(-102)

#define TEST_MDNIE_TUNE_LINE_1_NUM	(5)
#define TEST_MDNIE_TUNE_LINE_1_DEN	(1)
#define TEST_MDNIE_TUNE_LINE_1_CON	(-18483)

#define TEST_MDNIE_TUNE_COEFF_Q1_A	(-52615)
#define TEST_MDNIE_TUNE_COEFF_Q1_B	(-61905)
#define TEST_MDNIE_TUNE_COEFF_Q1_C	(21249)
#define TEST_MDNIE_TUNE_COEFF_Q1_D	(15603)
#define TEST_MDNIE_TUNE_COEFF_Q1_E	(40775)
#define TEST_MDNIE_TUNE_COEFF_Q1_F	(80902)
#define TEST_MDNIE_TUNE_COEFF_Q1_G	(-19651)
#define TEST_MDNIE_TUNE_COEFF_Q1_H	(-19618)

#define TEST_MDNIE_TUNE_COEFF_Q2_A	(-212096)
#define TEST_MDNIE_TUNE_COEFF_Q2_B	(-186041)
#define TEST_MDNIE_TUNE_COEFF_Q2_C	(61987)
#define TEST_MDNIE_TUNE_COEFF_Q2_D	(65143)
#define TEST_MDNIE_TUNE_COEFF_Q2_E	(-75083)
#define TEST_MDNIE_TUNE_COEFF_Q2_F	(-27237)
#define TEST_MDNIE_TUNE_COEFF_Q2_G	(16637)
#define TEST_MDNIE_TUNE_COEFF_Q2_H	(15737)

#define TEST_MDNIE_TUNE_COEFF_Q3_A	(69454)
#define TEST_MDNIE_TUNE_COEFF_Q3_B	(77493)
#define TEST_MDNIE_TUNE_COEFF_Q3_C	(-27852)
#define TEST_MDNIE_TUNE_COEFF_Q3_D	(-19429)
#define TEST_MDNIE_TUNE_COEFF_Q3_E	(-93856)
#define TEST_MDNIE_TUNE_COEFF_Q3_F	(-133061)
#define TEST_MDNIE_TUNE_COEFF_Q3_G	(37638)
#define TEST_MDNIE_TUNE_COEFF_Q3_H	(35353)

#define TEST_MDNIE_TUNE_COEFF_Q4_A	(192949)
#define TEST_MDNIE_TUNE_COEFF_Q4_B	(174780)
#define TEST_MDNIE_TUNE_COEFF_Q4_C	(-56853)
#define TEST_MDNIE_TUNE_COEFF_Q4_D	(-60597)
#define TEST_MDNIE_TUNE_COEFF_Q4_E	(57592)
#define TEST_MDNIE_TUNE_COEFF_Q4_F	(13018)
#define TEST_MDNIE_TUNE_COEFF_Q4_G	(-11491)
#define TEST_MDNIE_TUNE_COEFF_Q4_H	(-10757)

#define TEST_MDNIE_MAX_NIGHT_MODE		(2)
#define TEST_MDNIE_MAX_NIGHT_LEVEL		(102)

extern int panel_create_lcd_device(struct panel_device *panel, unsigned int id);
extern int panel_destroy_lcd_device(struct panel_device *panel);

extern int mdnie_init(struct mdnie_info *mdnie);
extern int mdnie_exit(struct mdnie_info *mdnie);
extern int mdnie_set_name(struct mdnie_info *mdnie, unsigned int id);
extern const char *mdnie_get_name(struct mdnie_info *mdnie);
extern int mdnie_create_class(struct mdnie_info *mdnie);
extern int mdnie_destroy_class(struct mdnie_info *mdnie);
extern int mdnie_create_device(struct mdnie_info *mdnie);
extern int mdnie_destroy_device(struct mdnie_info *mdnie);
extern int mdnie_create_device_files(struct mdnie_info *mdnie);
extern int mdnie_remove_device_files(struct mdnie_info *mdnie);
extern int mdnie_create_class_and_device(struct mdnie_info *mdnie);
extern int mdnie_remove_class_and_device(struct mdnie_info *mdnie);

extern int mdnie_init_property(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune);
extern int mdnie_deinit_property(struct mdnie_info *mdnie);
extern int mdnie_init_tables(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune);
extern int mdnie_get_coordinate(struct mdnie_info *mdnie, int *x, int *y);
extern int mdnie_init_coordinate_tune(struct mdnie_info *mdnie);
extern int mdnie_set_property(struct mdnie_info *mdnie, u32 *property, unsigned int value);

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

/* NOTE: UML TC */
static void mdnie_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void mdnie_test_macro_test(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, IS_HDR(HDR_OFF), false);
	KUNIT_EXPECT_EQ(test, IS_HDR(HDR_1), true);
	KUNIT_EXPECT_EQ(test, IS_HDR(HDR_2), true);
	KUNIT_EXPECT_EQ(test, IS_HDR(HDR_3), true);

	mdnie->props.hdr = HDR_OFF;
	KUNIT_EXPECT_EQ(test, IS_HDR_MODE(mdnie), false);
	mdnie->props.hdr = HDR_1;
	KUNIT_EXPECT_EQ(test, IS_HDR_MODE(mdnie), true);
}

static void mdnie_test_mdnie_set_def_wrgb_fail_with_invalid_argument(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char r = 255, g = 255, b = 255;

	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));

	KUNIT_EXPECT_EQ(test, mdnie_set_def_wrgb(NULL, r, g, b), -EINVAL);

	/* expect to keep initial value */
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[RED], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[GREEN], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[BLUE], (u8)0);
}

static void mdnie_test_mdnie_set_def_wrgb_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char r = 255, g = 255, b = 255;

	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));

	KUNIT_EXPECT_EQ(test, mdnie_set_def_wrgb(mdnie, r, g, b), 0);

	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[RED], (u8)r);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[GREEN], (u8)g);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[BLUE], (u8)b);
}

static void mdnie_test_mdnie_set_cur_wrgb_fail_with_invalid_argument(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char r = 255, g = 255, b = 255;

	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));

	KUNIT_EXPECT_EQ(test, mdnie_set_cur_wrgb(NULL, r, g, b), -EINVAL);

	/* expect to keep initial value */
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[RED], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[GREEN], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[BLUE], (u8)0);
}

static void mdnie_test_mdnie_set_cur_wrgb_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char r = 255, g = 255, b = 255;

	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));

	KUNIT_EXPECT_EQ(test, mdnie_set_cur_wrgb(mdnie, r, g, b), 0);

	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[RED], (u8)r);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[GREEN], (u8)g);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[BLUE], (u8)b);
}

static void mdnie_test_mdnie_cur_wrgb_to_byte_array_fail_with_invalid_argument(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char dst[6] = { 0, };
	unsigned char r = 255, g = 255, b = 255;

	mdnie->props.cur_wrgb[RED] = r;
	mdnie->props.cur_wrgb[GREEN] = g;
	mdnie->props.cur_wrgb[BLUE] = b;

	KUNIT_EXPECT_EQ(test, mdnie_cur_wrgb_to_byte_array(NULL, dst, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, mdnie_cur_wrgb_to_byte_array(mdnie, NULL, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, mdnie_cur_wrgb_to_byte_array(mdnie, NULL, 0), -EINVAL);

	/* expect to keep initial value */
	KUNIT_EXPECT_EQ(test, dst[0], (unsigned char)0);
	KUNIT_EXPECT_EQ(test, dst[1], (unsigned char)0);
	KUNIT_EXPECT_EQ(test, dst[2], (unsigned char)0);
	KUNIT_EXPECT_EQ(test, dst[3], (unsigned char)0);
	KUNIT_EXPECT_EQ(test, dst[4], (unsigned char)0);
	KUNIT_EXPECT_EQ(test, dst[5], (unsigned char)0);
}

static void mdnie_test_mdnie_cur_wrgb_to_byte_array_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char dst[6] = { 0, };
	unsigned char r = 255, g = 255, b = 255;

	mdnie->props.cur_wrgb[RED] = r;
	mdnie->props.cur_wrgb[GREEN] = g;
	mdnie->props.cur_wrgb[BLUE] = b;

	KUNIT_EXPECT_EQ(test, mdnie_cur_wrgb_to_byte_array(mdnie, dst, 2), 0);

	/* expect to copy from n-th position to 2n-th position */
	KUNIT_EXPECT_EQ(test, dst[0], r);
	KUNIT_EXPECT_EQ(test, dst[1], (unsigned char)0);
	KUNIT_EXPECT_EQ(test, dst[2], g);
	KUNIT_EXPECT_EQ(test, dst[3], (unsigned char)0);
	KUNIT_EXPECT_EQ(test, dst[4], b);
	KUNIT_EXPECT_EQ(test, dst[5], (unsigned char)0);
}

static void mdnie_test_mdnie_update_wrgb_fail_with_invalid_argument(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_update_wrgb(NULL, 0, 0, 0), -EINVAL);
}

static void mdnie_test_mdnie_update_wrgb_fail_with_invalid_scr_white_mode(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char r = 128, g = 245, b = 20;

	mdnie->props.scr_white_mode = MAX_SCR_WHITE_MODE;

	KUNIT_EXPECT_EQ(test, mdnie_update_wrgb(mdnie, r, g, b), -EINVAL);
}

static void mdnie_test_mdnie_update_wrgb_should_update_def_wrgb_and_cur_wrgb_in_color_coordinate_mode(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char r = 128, g = 245, b = 20;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_COLOR_COORDINATE;

	/* SCENARIO_MODE:AUTO */
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));
	mdnie->props.scenario_mode = AUTO;
	mdnie->props.def_wrgb_ofs[RED] = -20;
	mdnie->props.def_wrgb_ofs[GREEN] = 30;
	mdnie->props.def_wrgb_ofs[BLUE] = -40;

	KUNIT_EXPECT_EQ(test, mdnie_update_wrgb(mdnie, r, g, b), 0);

	/* expect to update def_wrgb with (r, g, b) */
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[RED], r);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[GREEN], g);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[BLUE], b);

	/*
	 * expect to update cur_wrgb as below
	 * cur_wrgb[RED] = min(max((r + wrgb_ofs[RED]), 0), 255)
	 * cur_wrgb[GREEN] = min(max((g + wrgb_ofs[GREEN]), 0), 255)
	 * cur_wrgb[BLUE] = min(max((b + wrgb_ofs[BLUE]), 0), 255)
	 */
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[RED], (u8)(r - 20));
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[GREEN], (u8)255);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[BLUE], (u8)0);

	/* SCENARIO_MODE:NATURAL */
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));
	mdnie->props.scenario_mode = NATURAL;
	mdnie->props.def_wrgb_ofs[RED] = -20;
	mdnie->props.def_wrgb_ofs[GREEN] = 30;
	mdnie->props.def_wrgb_ofs[BLUE] = -40;

	KUNIT_EXPECT_EQ(test, mdnie_update_wrgb(mdnie, r, g, b), 0);

	/* expect to update def_wrgb with (r, g, b) */
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[RED], r);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[GREEN], g);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[BLUE], b);

	/*
	 * expect to update cur_wrgb as below
	 * cur_wrgb[RED] = r
	 * cur_wrgb[GREEN] = g
	 * cur_wrgb[BLUE] = b
	 */
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[RED], r);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[GREEN], g);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[BLUE], b);
}

static void mdnie_test_mdnie_update_wrgb_should_update_cur_wrgb_and_keep_def_wrgb_in_adjust_ldu_mode(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char r = 128, g = 245, b = 20;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_ADJUST_LDU;
	mdnie->props.def_wrgb_ofs[RED] = -20;
	mdnie->props.def_wrgb_ofs[GREEN] = 30;
	mdnie->props.def_wrgb_ofs[BLUE] = -40;

	/* SCENARIO:UI_MODE,AUTO */
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));
	mdnie->props.scenario_mode = AUTO;
	mdnie->props.scenario = UI_MODE;

	KUNIT_EXPECT_EQ(test, mdnie_update_wrgb(mdnie, r, g, b), 0);

	/* expect to keep initial def_wrgb */
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[RED], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[GREEN], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[BLUE], (u8)0);

	/*
	 * expect to update cur_wrgb as below
	 * cur_wrgb[RED] = min(max((r + wrgb_ofs[RED]), 0), 255)
	 * cur_wrgb[GREEN] = min(max((g + wrgb_ofs[GREEN]), 0), 255)
	 * cur_wrgb[BLUE] = min(max((b + wrgb_ofs[BLUE]), 0), 255)
	 */
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[RED], (u8)(r - 20));
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[GREEN], (u8)255);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[BLUE], (u8)0);

	/* SCENARIO:EBOOK_MODE,AUTO */
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));
	mdnie->props.scenario_mode = AUTO;
	mdnie->props.scenario = EBOOK_MODE;

	KUNIT_EXPECT_EQ(test, mdnie_update_wrgb(mdnie, r, g, b), 0);

	/* expect to keep initial def_wrgb */
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[RED], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[GREEN], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[BLUE], (u8)0);

	/* expect to update cur_wrgb with (r, g, b) */
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[RED], r);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[GREEN], g);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[BLUE], b);

	/* SCENARIO_MODE:UI_MODE,NATURAL */
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));
	mdnie->props.scenario_mode = NATURAL;
	mdnie->props.scenario = UI_MODE;

	KUNIT_EXPECT_EQ(test, mdnie_update_wrgb(mdnie, r, g, b), 0);

	/* expect to keep initial def_wrgb */
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[RED], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[GREEN], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[BLUE], (u8)0);

	/* expect to update cur_wrgb with (r, g, b) */
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[RED], r);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[GREEN], g);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[BLUE], b);
}

static void mdnie_test_mdnie_update_wrgb_should_cur_wrgb_is_updated_in_sensor_rgb_mode(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	unsigned char r = 128, g = 245, b = 20;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_SENSOR_RGB;
	mdnie->props.def_wrgb_ofs[RED] = -20;
	mdnie->props.def_wrgb_ofs[GREEN] = 30;
	mdnie->props.def_wrgb_ofs[BLUE] = -40;

	/* SCENARIO:UI_MODE,AUTO */
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));
	mdnie->props.scenario_mode = AUTO;
	mdnie->props.scenario = UI_MODE;

	KUNIT_EXPECT_EQ(test, mdnie_update_wrgb(mdnie, r, g, b), 0);

	/* expect to keep initial def_wrgb */
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[RED], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[GREEN], (u8)0);
	KUNIT_EXPECT_EQ(test, mdnie->props.def_wrgb[BLUE], (u8)0);

	/* expect to update cur_wrgb with (r, g, b) */
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[RED], r);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[GREEN], g);
	KUNIT_EXPECT_EQ(test, mdnie->props.cur_wrgb[BLUE], b);
}

static void mdnie_test_mdnie_set_name_fail_with_invalid_args(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_set_name(NULL, 0), -EINVAL);
}

static void mdnie_test_mdnie_get_name_return_null_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, !mdnie_get_name(NULL));
}

static void mdnie_test_mdnie_set_name_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, mdnie_set_name(mdnie, 0), 0);
	KUNIT_EXPECT_STREQ(test, mdnie_get_name(mdnie), "mdnie");

	KUNIT_EXPECT_EQ(test, mdnie_set_name(mdnie, 1), 0);
	KUNIT_EXPECT_STREQ(test, mdnie_get_name(mdnie), "mdnie-1");
}

static void mdnie_test_mdnie_destroy_class_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_destroy_class(NULL), -EINVAL);
}

static void mdnie_test_mdnie_destroy_class_fail_if_mdnie_class_is_not_exist(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, mdnie_destroy_class(mdnie), -EINVAL);
}

static void mdnie_test_mdnie_create_class_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_create_class(NULL), -EINVAL);
}

static void mdnie_test_mdnie_create_and_destroy_class_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	/* create 'mdnie' class */
	KUNIT_ASSERT_EQ(test, mdnie_set_name(mdnie, 0), 0);
	KUNIT_ASSERT_STREQ(test, mdnie_get_name(mdnie), "mdnie");

	KUNIT_EXPECT_EQ(test, mdnie_create_class(mdnie), 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, mdnie->class);

	KUNIT_EXPECT_EQ(test, mdnie_destroy_class(mdnie), 0);
	KUNIT_EXPECT_TRUE(test, !mdnie->class);

	/* create 'mdnie-1' class */
	KUNIT_ASSERT_EQ(test, mdnie_set_name(mdnie, 1), 0);
	KUNIT_ASSERT_STREQ(test, mdnie_get_name(mdnie), "mdnie-1");

	KUNIT_EXPECT_EQ(test, mdnie_create_class(mdnie), 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, mdnie->class);

	KUNIT_EXPECT_EQ(test, mdnie_destroy_class(mdnie), 0);
	KUNIT_EXPECT_TRUE(test, !mdnie->class);
}

static void mdnie_test_mdnie_destroy_device_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_destroy_device(NULL), -EINVAL);
}

static void mdnie_test_mdnie_destroy_device_fail_if_mdnie_device_is_not_exist(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, mdnie_create_class(mdnie), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mdnie->class);

	KUNIT_EXPECT_EQ(test, mdnie_destroy_device(mdnie), -ENODEV);

	KUNIT_EXPECT_EQ(test, mdnie_destroy_class(mdnie), 0);
	KUNIT_EXPECT_TRUE(test, !mdnie->class);
}

static void mdnie_test_mdnie_create_device_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_create_device(NULL), -EINVAL);
}

static void mdnie_test_mdnie_create_device_fail_if_mdnie_class_is_not_exist(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, mdnie_create_device(mdnie), -EINVAL);
}

static void mdnie_test_mdnie_create_device_fail_if_panel_lcd_device_is_not_exist(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	struct panel_device *panel = to_panel_device(mdnie);
	struct device *lcd_dev = panel->lcd_dev;

	KUNIT_EXPECT_EQ(test, mdnie_create_class(mdnie), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mdnie->class);

	/* backup lcd_dev */
	lcd_dev = panel->lcd_dev;
	panel->lcd_dev = NULL;

	KUNIT_ASSERT_EQ(test, mdnie_create_device(mdnie), -EINVAL);

	/* restore lcd_dev */
	panel->lcd_dev = lcd_dev;

	KUNIT_EXPECT_EQ(test, mdnie_destroy_class(mdnie), 0);
	KUNIT_EXPECT_TRUE(test, !mdnie->class);
}

static void mdnie_test_mdnie_create_and_destroy_device_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_ASSERT_EQ(test, mdnie_create_class(mdnie), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mdnie->class);

	KUNIT_EXPECT_EQ(test, mdnie_create_device(mdnie), 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, mdnie->dev);

	KUNIT_EXPECT_EQ(test, mdnie_destroy_device(mdnie), 0);
	KUNIT_EXPECT_TRUE(test, !mdnie->dev);

	KUNIT_EXPECT_EQ(test, mdnie_destroy_class(mdnie), 0);
	KUNIT_EXPECT_TRUE(test, !mdnie->class);
}

static void mdnie_test_mdnie_remove_device_files_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_remove_device_files(NULL), -EINVAL);
}

static void mdnie_test_mdnie_remove_device_files_fail_if_mdnie_device_is_not_exist(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, mdnie_remove_device_files(mdnie), -EINVAL);
}

static void mdnie_test_mdnie_create_device_files_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_create_device_files(NULL), -EINVAL);
}

static void mdnie_test_mdnie_create_device_files_fail_if_mdnie_device_is_not_exist(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, mdnie_create_device_files(mdnie), -EINVAL);
}

static void mdnie_test_mdnie_create_and_remove_device_files_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	mdnie_create_class(mdnie);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mdnie->class);

	mdnie_create_device(mdnie);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mdnie->dev);

	/* check sysfs existence */
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "mode"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "scenario"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "accessibility"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "bypass"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "lux"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "mdnie"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "sensorRGB"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "whiteRGB"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "night_mode"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "color_lens"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "hdr"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "light_notification"));
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "mdnie_ldu"));
#ifdef CONFIG_USDM_PANEL_HMD
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "hmt_color_temperature"));
#endif
#ifdef CONFIG_USDM_MDNIE_AFC
	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(mdnie->class, "afc"));
#endif

	KUNIT_EXPECT_EQ(test, mdnie_create_device_files(mdnie), 0);

	/* check sysfs existence */
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "mode"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "scenario"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "accessibility"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "bypass"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "lux"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "mdnie"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "sensorRGB"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "whiteRGB"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "night_mode"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "color_lens"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "hdr"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "light_notification"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "mdnie_ldu"));
#ifdef CONFIG_USDM_PANEL_HMD
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "hmt_color_temperature"));
#endif
#ifdef CONFIG_USDM_MDNIE_AFC
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(mdnie->class, "afc"));
#endif

	KUNIT_EXPECT_EQ(test, mdnie_remove_device_files(mdnie), 0);

	mdnie_destroy_device(mdnie);
	mdnie_destroy_class(mdnie);
}

static void mdnie_test_mdnie_init_property_fail_with_invalid_args(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	struct mdnie_tune *mdnie_tune = ctx->mdnie_tune;

	KUNIT_EXPECT_EQ(test, mdnie_init_property(NULL, mdnie_tune), -EINVAL);
	KUNIT_EXPECT_EQ(test, mdnie_init_property(mdnie, NULL), -EINVAL);
}

static void mdnie_test_mdnie_init_property_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	struct mdnie_tune *mdnie_tune = ctx->mdnie_tune;

	KUNIT_EXPECT_EQ(test, mdnie_init_property(mdnie, mdnie_tune), 0);
	KUNIT_EXPECT_EQ(test, mdnie->props.num_ldu_mode, mdnie_tune->num_ldu_mode);
	KUNIT_EXPECT_EQ(test, mdnie->props.num_night_level, mdnie_tune->num_night_level);
	KUNIT_EXPECT_EQ(test, mdnie->props.num_color_lens_color, mdnie_tune->num_color_lens_color);
	KUNIT_EXPECT_EQ(test, mdnie->props.num_color_lens_level, mdnie_tune->num_color_lens_level);
	KUNIT_EXPECT_TRUE(test, !memcmp(mdnie->props.line, mdnie_tune->line, sizeof(mdnie->props.line)));
	KUNIT_EXPECT_TRUE(test, !memcmp(mdnie->props.coef, mdnie_tune->coef, sizeof(mdnie->props.coef)));
	KUNIT_EXPECT_TRUE(test, !memcmp(mdnie->props.vtx, mdnie_tune->vtx, sizeof(mdnie->props.vtx)));
	KUNIT_EXPECT_EQ(test, mdnie->props.cal_x_center, mdnie_tune->cal_x_center);
	KUNIT_EXPECT_EQ(test, mdnie->props.cal_y_center, mdnie_tune->cal_y_center);
	KUNIT_EXPECT_EQ(test, mdnie->props.cal_boundary_center, mdnie_tune->cal_boundary_center);
	KUNIT_EXPECT_EQ(test, mdnie_deinit_property(mdnie), 0);
}

static void mdnie_test_mdnie_set_property_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	struct panel_device *panel = to_panel_device(mdnie);
	struct mdnie_tune *mdnie_tune = ctx->mdnie_tune;
	struct mdnie_prop_test_param mdnie_prop_test_success_params[] = {
		/* MDNIE_MODE_PROPERTY */
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_OFF_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_BYPASS_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_ACCESSIBILITY_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_LIGHT_NOTIFICATION_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_COLOR_LENS_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_HDR_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_HMD_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_NIGHT_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_HBM_CE_MODE },
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MDNIE_SCENARIO_MODE },
		/* MDNIE_SCR_WHITE_MODE_PROPERTY */
		{ MDNIE_SCR_WHITE_MODE_PROPERTY, &mdnie->props.scr_white_mode, SCR_WHITE_MODE_NONE },
		{ MDNIE_SCR_WHITE_MODE_PROPERTY, &mdnie->props.scr_white_mode, SCR_WHITE_MODE_COLOR_COORDINATE },
		{ MDNIE_SCR_WHITE_MODE_PROPERTY, &mdnie->props.scr_white_mode, SCR_WHITE_MODE_ADJUST_LDU },
		{ MDNIE_SCR_WHITE_MODE_PROPERTY, &mdnie->props.scr_white_mode, SCR_WHITE_MODE_SENSOR_RGB },
		/* MDNIE_SCENARIO_MODE_PROPERTY */
		{ MDNIE_SCENARIO_MODE_PROPERTY, &mdnie->props.scenario_mode, DYNAMIC },
		{ MDNIE_SCENARIO_MODE_PROPERTY, &mdnie->props.scenario_mode, STANDARD },
		{ MDNIE_SCENARIO_MODE_PROPERTY, &mdnie->props.scenario_mode, NATURAL },
		{ MDNIE_SCENARIO_MODE_PROPERTY, &mdnie->props.scenario_mode, MOVIE },
		{ MDNIE_SCENARIO_MODE_PROPERTY, &mdnie->props.scenario_mode, AUTO },
		/* MDNIE_SCREEN_MODE_PROPERTY */
		{ MDNIE_SCREEN_MODE_PROPERTY, &mdnie->props.screen_mode, MDNIE_SCREEN_MODE_VIVID },
		{ MDNIE_SCREEN_MODE_PROPERTY, &mdnie->props.screen_mode, MDNIE_SCREEN_MODE_NATURAL },
		/* MDNIE_SCENARIO_PROPERTY */
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, UI_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, VIDEO_NORMAL_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, CAMERA_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, NAVI_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, GALLERY_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, VT_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, BROWSER_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, EBOOK_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, EMAIL_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, GAME_LOW_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, GAME_MID_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, GAME_HIGH_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, VIDEO_ENHANCER },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, VIDEO_ENHANCER_THIRD },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, HMD_8_MODE },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, HMD_16_MODE },
		/* MDNIE_ACCESSIBILITY_PROPERTY */
		{ MDNIE_ACCESSIBILITY_PROPERTY, &mdnie->props.accessibility, ACCESSIBILITY_OFF },
		{ MDNIE_ACCESSIBILITY_PROPERTY, &mdnie->props.accessibility, NEGATIVE },
		{ MDNIE_ACCESSIBILITY_PROPERTY, &mdnie->props.accessibility, COLOR_BLIND },
		{ MDNIE_ACCESSIBILITY_PROPERTY, &mdnie->props.accessibility, SCREEN_CURTAIN },
		{ MDNIE_ACCESSIBILITY_PROPERTY, &mdnie->props.accessibility, GRAYSCALE },
		{ MDNIE_ACCESSIBILITY_PROPERTY, &mdnie->props.accessibility, GRAYSCALE_NEGATIVE },
		{ MDNIE_ACCESSIBILITY_PROPERTY, &mdnie->props.accessibility, COLOR_BLIND_HBM },
		/* MDNIE_BYPASS_PROPERTY */
		{ MDNIE_BYPASS_PROPERTY, &mdnie->props.bypass, BYPASS_OFF },
		{ MDNIE_BYPASS_PROPERTY, &mdnie->props.bypass, BYPASS_ON },
		/* MDNIE_HBM_CE_LEVEL_PROPERTY */
		{ MDNIE_HBM_CE_LEVEL_PROPERTY, &mdnie->props.hbm_ce_level, 0 },
		{ MDNIE_HBM_CE_LEVEL_PROPERTY, &mdnie->props.hbm_ce_level, MAX_HBM_CE_LEVEL },
		/* MDNIE_NIGHT_MODE_PROPERTY */
		{ MDNIE_NIGHT_MODE_PROPERTY, &mdnie->props.night, NIGHT_MODE_OFF },
		{ MDNIE_NIGHT_MODE_PROPERTY, &mdnie->props.night, NIGHT_MODE_ON },
		/* MDNIE_NIGHT_LEVEL_PROPERTY */
		{ MDNIE_NIGHT_LEVEL_PROPERTY, &mdnie->props.night_level, 0 },
		{ MDNIE_NIGHT_LEVEL_PROPERTY, &mdnie->props.night_level, 1 },
		{ MDNIE_NIGHT_LEVEL_PROPERTY, &mdnie->props.night_level, 101 },
		/* MDNIE_COLOR_LENS_PROPERTY */
		{ MDNIE_COLOR_LENS_PROPERTY, &mdnie->props.color_lens, COLOR_LENS_OFF },
		{ MDNIE_COLOR_LENS_PROPERTY, &mdnie->props.color_lens, COLOR_LENS_ON },
		/* MDNIE_COLOR_LENS_COLOR_PROPERTY */
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_BLUE },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_AZURE },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_CYAN },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_SPRING_GREEN },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_GREEN },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_CHARTREUSE_GREEN },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_YELLOW },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_ORANGE },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_RED },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_ROSE },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_MAGENTA },
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_VIOLET },
		/* MDNIE_COLOR_LENS_LEVEL_PROPERTY */
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_20P },
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_25P },
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_30P },
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_35P },
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_40P },
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_45P },
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_50P },
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_55P },
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_60P },
		/* MDNIE_HDR_PROPERTY */
		{ MDNIE_HDR_PROPERTY, &mdnie->props.hdr, HDR_OFF },
		{ MDNIE_HDR_PROPERTY, &mdnie->props.hdr, HDR_1 },
		{ MDNIE_HDR_PROPERTY, &mdnie->props.hdr, HDR_2 },
		{ MDNIE_HDR_PROPERTY, &mdnie->props.hdr, HDR_3 },
		/* MDNIE_LIGHT_NOTIFICATION_PROPERTY */
		{ MDNIE_LIGHT_NOTIFICATION_PROPERTY, &mdnie->props.light_notification, LIGHT_NOTIFICATION_OFF },
		{ MDNIE_LIGHT_NOTIFICATION_PROPERTY, &mdnie->props.light_notification, LIGHT_NOTIFICATION_ON },
		/* MDNIE_LDU_MODE_PROPERTY */
		{ MDNIE_LDU_MODE_PROPERTY, &mdnie->props.ldu, LDU_MODE_OFF },
		{ MDNIE_LDU_MODE_PROPERTY, &mdnie->props.ldu, LDU_MODE_1 },
		{ MDNIE_LDU_MODE_PROPERTY, &mdnie->props.ldu, LDU_MODE_2 },
		{ MDNIE_LDU_MODE_PROPERTY, &mdnie->props.ldu, LDU_MODE_3 },
		{ MDNIE_LDU_MODE_PROPERTY, &mdnie->props.ldu, LDU_MODE_4 },
		{ MDNIE_LDU_MODE_PROPERTY, &mdnie->props.ldu, LDU_MODE_5 },
		/* MDNIE_HMD_PROPERTY */
		{ MDNIE_HMD_PROPERTY, &mdnie->props.hmd, HMD_MDNIE_OFF },
		{ MDNIE_HMD_PROPERTY, &mdnie->props.hmd, HMD_3000K },
		{ MDNIE_HMD_PROPERTY, &mdnie->props.hmd, HMD_4000K },
		{ MDNIE_HMD_PROPERTY, &mdnie->props.hmd, HMD_5000K },
		{ MDNIE_HMD_PROPERTY, &mdnie->props.hmd, HMD_6500K },
		{ MDNIE_HMD_PROPERTY, &mdnie->props.hmd, HMD_7500K },
		/* MDNIE_ENABLE_PROPERTY */
		{ MDNIE_ENABLE_PROPERTY, &mdnie->props.enable, 0 },
		{ MDNIE_ENABLE_PROPERTY, &mdnie->props.enable, 1 },
		/* MDNIE_TRANS_MODE_PROPERTY */
		{ MDNIE_TRANS_MODE_PROPERTY, &mdnie->props.trans_mode, TRANS_OFF },
		{ MDNIE_TRANS_MODE_PROPERTY, &mdnie->props.trans_mode, TRANS_ON },
	};
	int i;

	KUNIT_ASSERT_EQ(test, mdnie_init_property(mdnie, mdnie_tune), 0);
	for (i = 0; i < ARRAY_SIZE(mdnie_prop_test_success_params); i++) {
		KUNIT_EXPECT_EQ(test, mdnie_set_property(mdnie,
				mdnie_prop_test_success_params[i].prop_ptr,
				mdnie_prop_test_success_params[i].value), 0);
		KUNIT_EXPECT_EQ(test, panel_get_property_value(panel,
				mdnie_prop_test_success_params[i].prop_name),
				mdnie_prop_test_success_params[i].value);
	}
	KUNIT_ASSERT_EQ(test, mdnie_deinit_property(mdnie), 0);
}

static void mdnie_test_mdnie_set_property_failure(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	struct mdnie_tune *mdnie_tune = ctx->mdnie_tune;
	struct mdnie_prop_test_param mdnie_prop_test_failure_params[] = {
		/* MDNIE_MODE_PROPERTY */
		{ MDNIE_MODE_PROPERTY, &mdnie->props.mdnie_mode, MAX_MDNIE_MODE },
		/* MDNIE_SCR_WHITE_MODE_PROPERTY */
		{ MDNIE_SCR_WHITE_MODE_PROPERTY, &mdnie->props.scr_white_mode, MAX_SCR_WHITE_MODE },
		/* MDNIE_SCENARIO_MODE_PROPERTY */
		{ MDNIE_SCENARIO_MODE_PROPERTY, &mdnie->props.scenario_mode, MODE_MAX },
		/* MDNIE_SCREEN_MODE_PROPERTY */
		{ MDNIE_SCREEN_MODE_PROPERTY, &mdnie->props.screen_mode, MAX_MDNIE_SCREEN_MODE },
		/* MDNIE_SCENARIO_PROPERTY */
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, VIDEO_NORMAL_MODE+1 },
		{ MDNIE_SCENARIO_PROPERTY, &mdnie->props.scenario, SCENARIO_MAX },
		/* MDNIE_ACCESSIBILITY_PROPERTY */
		{ MDNIE_ACCESSIBILITY_PROPERTY, &mdnie->props.accessibility, ACCESSIBILITY_MAX },
		/* MDNIE_BYPASS_PROPERTY */
		{ MDNIE_BYPASS_PROPERTY, &mdnie->props.bypass, BYPASS_MAX },
		/* MDNIE_HBM_CE_LEVEL_PROPERTY */
		{ MDNIE_HBM_CE_LEVEL_PROPERTY, &mdnie->props.hbm_ce_level, MAX_HBM_CE_LEVEL+1 },
		/* MDNIE_NIGHT_MODE_PROPERTY */
		{ MDNIE_NIGHT_MODE_PROPERTY, &mdnie->props.night, NIGHT_MODE_MAX },
		/* MDNIE_NIGHT_LEVEL_PROPERTY */
		{ MDNIE_NIGHT_LEVEL_PROPERTY, &mdnie->props.night_level, 306 },
		/* MDNIE_COLOR_LENS_PROPERTY */
		{ MDNIE_COLOR_LENS_PROPERTY, &mdnie->props.color_lens, COLOR_LENS_MAX },
		/* MDNIE_COLOR_LENS_COLOR_PROPERTY */
		{ MDNIE_COLOR_LENS_COLOR_PROPERTY, &mdnie->props.color_lens_color, COLOR_LENS_COLOR_MAX },
		/* MDNIE_COLOR_LENS_LEVEL_PROPERTY */
		{ MDNIE_COLOR_LENS_LEVEL_PROPERTY, &mdnie->props.color_lens_level, COLOR_LENS_LEVEL_MAX },
		/* MDNIE_HDR_PROPERTY */
		{ MDNIE_HDR_PROPERTY, &mdnie->props.hdr, HDR_MAX },
		/* MDNIE_LIGHT_NOTIFICATION_PROPERTY */
		{ MDNIE_LIGHT_NOTIFICATION_PROPERTY, &mdnie->props.light_notification, LIGHT_NOTIFICATION_MAX },
		/* MDNIE_LDU_MODE_PROPERTY */
		{ MDNIE_LDU_MODE_PROPERTY, &mdnie->props.ldu, MAX_LDU_MODE },
		/* MDNIE_HMD_PROPERTY */
		{ MDNIE_HMD_PROPERTY, &mdnie->props.hmd, HMD_MDNIE_MAX },
		/* MDNIE_ENABLE_PROPERTY */
		{ MDNIE_ENABLE_PROPERTY, &mdnie->props.enable, 2 },
		/* MDNIE_TRANS_MODE_PROPERTY */
		{ MDNIE_TRANS_MODE_PROPERTY, &mdnie->props.trans_mode, MAX_TRANS_MODE },
		/* not allowed to set property */
		{ NULL, &mdnie->props.sz_scr, 0 },
#ifdef CONFIG_USDM_MDNIE_AFC
		{ NULL, &mdnie->props.afc_on, 0 },
#endif
		{ NULL, &mdnie->props.num_ldu_mode, 0 },
		{ NULL, &mdnie->props.num_night_level, 0 },
		{ NULL, &mdnie->props.num_color_lens_color, 0 },
		{ NULL, &mdnie->props.num_color_lens_level, 0 },
		{ NULL, &mdnie->props.cal_x_center, 0 },
		{ NULL, &mdnie->props.cal_y_center, 0 },
		{ NULL, &mdnie->props.cal_boundary_center, 0 },
		//{ NULL, &mdnie->props.hbm_ce_lux, 0 },
		{ NULL, &mdnie->props.scr_white_len, 0 },
		{ NULL, &mdnie->props.scr_cr_ofs, 0 },
		{ NULL, &mdnie->props.night_mode_ofs, 0 },
		{ NULL, &mdnie->props.color_lens_ofs, 0 },
	};
	int i;

	KUNIT_ASSERT_EQ(test, mdnie_init_property(mdnie, mdnie_tune), 0);
	for (i = 0; i < ARRAY_SIZE(mdnie_prop_test_failure_params); i++) {
		KUNIT_EXPECT_EQ(test, mdnie_set_property(mdnie,
				mdnie_prop_test_failure_params[i].prop_ptr,
				mdnie_prop_test_failure_params[i].value), -EINVAL);
	}
	KUNIT_ASSERT_EQ(test, mdnie_deinit_property(mdnie), 0);
}

static void mdnie_test_mdnie_get_coordinate_fail_with_invalid_args(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	int x, y;

	KUNIT_EXPECT_EQ(test, mdnie_get_coordinate(NULL, &x, &y), -EINVAL);
	KUNIT_EXPECT_EQ(test, mdnie_get_coordinate(mdnie, NULL, &y), -EINVAL);
	KUNIT_EXPECT_EQ(test, mdnie_get_coordinate(mdnie, &x, NULL), -EINVAL);
}

static void mdnie_test_mdnie_get_coordinate_fail_if_coordinate_resource_is_not_initialized(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	int x, y;

	KUNIT_EXPECT_EQ(test, mdnie_get_coordinate(mdnie, &x, &y), -EINVAL);
}

static void mdnie_test_mdnie_init_coordinate_tune_fail_with_invalid_args(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_init_coordinate_tune(NULL), -EINVAL);
}

static void mdnie_test_mdnie_create_class_and_device_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_create_class_and_device(NULL), -EINVAL);
}

static void mdnie_test_mdnie_remove_class_and_device_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_remove_class_and_device(NULL), -EINVAL);
}

static void mdnie_test_mdnie_create_and_remove_class_and_device_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, mdnie_create_class_and_device(mdnie), 0);
	KUNIT_EXPECT_EQ(test, mdnie_remove_class_and_device(mdnie), 0);
}

static void mdnie_test_mdnie_init_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_init(NULL), -EINVAL);
}

static void mdnie_test_mdnie_exit_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mdnie_exit(NULL), -EINVAL);
}

static void mdnie_test_mdnie_init_and_exit_success(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;

	KUNIT_EXPECT_EQ(test, mdnie_init(mdnie), 0);
	KUNIT_EXPECT_EQ(test, mdnie_exit(mdnie), 0);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int mdnie_test_init(struct kunit *test)
{
	struct mdnie_test *ctx;
	struct panel_device *panel;
	struct mdnie_tune *mdnie_tune;
	struct mdnie_tune default_mdnie_tune = {
		.line = {
			{ .num = TEST_MDNIE_TUNE_LINE_0_NUM, .den = TEST_MDNIE_TUNE_LINE_0_DEN, .con = TEST_MDNIE_TUNE_LINE_0_CON },
			{ .num = TEST_MDNIE_TUNE_LINE_1_NUM, .den = TEST_MDNIE_TUNE_LINE_1_DEN, .con = TEST_MDNIE_TUNE_LINE_1_CON },
		},
		.coef = {
			{
				.a = TEST_MDNIE_TUNE_COEFF_Q1_A, .b = TEST_MDNIE_TUNE_COEFF_Q1_B, .c = TEST_MDNIE_TUNE_COEFF_Q1_C, .d = TEST_MDNIE_TUNE_COEFF_Q1_D,
				.e = TEST_MDNIE_TUNE_COEFF_Q1_E, .f = TEST_MDNIE_TUNE_COEFF_Q1_F, .g = TEST_MDNIE_TUNE_COEFF_Q1_G, .h = TEST_MDNIE_TUNE_COEFF_Q1_H,
			},
			{
				.a = TEST_MDNIE_TUNE_COEFF_Q2_A, .b = TEST_MDNIE_TUNE_COEFF_Q2_B, .c = TEST_MDNIE_TUNE_COEFF_Q2_C, .d = TEST_MDNIE_TUNE_COEFF_Q2_D,
				.e = TEST_MDNIE_TUNE_COEFF_Q2_E, .f = TEST_MDNIE_TUNE_COEFF_Q2_F, .g = TEST_MDNIE_TUNE_COEFF_Q2_G, .h = TEST_MDNIE_TUNE_COEFF_Q2_H,
			},
			{
				.a = TEST_MDNIE_TUNE_COEFF_Q3_A, .b = TEST_MDNIE_TUNE_COEFF_Q3_B, .c = TEST_MDNIE_TUNE_COEFF_Q3_C, .d = TEST_MDNIE_TUNE_COEFF_Q3_D,
				.e = TEST_MDNIE_TUNE_COEFF_Q3_E, .f = TEST_MDNIE_TUNE_COEFF_Q3_F, .g = TEST_MDNIE_TUNE_COEFF_Q3_G, .h = TEST_MDNIE_TUNE_COEFF_Q3_H,
			},
			{
				.a = TEST_MDNIE_TUNE_COEFF_Q4_A, .b = TEST_MDNIE_TUNE_COEFF_Q4_B, .c = TEST_MDNIE_TUNE_COEFF_Q4_C, .d = TEST_MDNIE_TUNE_COEFF_Q4_D,
				.e = TEST_MDNIE_TUNE_COEFF_Q4_E, .f = TEST_MDNIE_TUNE_COEFF_Q4_F, .g = TEST_MDNIE_TUNE_COEFF_Q4_G, .h = TEST_MDNIE_TUNE_COEFF_Q4_H,
			},
		},
		.cal_x_center = 3050,
		.cal_y_center = 3210,
		.cal_boundary_center = 225,
		.vtx = {
			[WCRD_TYPE_ADAPTIVE] = {
				[CCRD_PT_NONE] = { 0xff, 0xff, 0xff }, /* dummy */
				[CCRD_PT_1] = { 0xff, 0xfa, 0xf9 }, /* Tune_1 */
				[CCRD_PT_2] = { 0xff, 0xfc, 0xff }, /* Tune_2 */
				[CCRD_PT_3] = { 0xf8, 0xf7, 0xff }, /* Tune_3 */
				[CCRD_PT_4] = { 0xff, 0xfd, 0xf9 }, /* Tune_4 */
				[CCRD_PT_5] = { 0xff, 0xff, 0xff }, /* Tune_5 */
				[CCRD_PT_6] = { 0xf8, 0xfa, 0xff }, /* Tune_6 */
				[CCRD_PT_7] = { 0xfd, 0xff, 0xf8 }, /* Tune_7 */
				[CCRD_PT_8] = { 0xfb, 0xff, 0xfc }, /* Tune_8 */
				[CCRD_PT_9] = { 0xf8, 0xfd, 0xff }, /* Tune_9 */
			},
			[WCRD_TYPE_D65] = {
				[CCRD_PT_NONE] = { 0xff, 0xfc, 0xf6 }, /* dummy */
				[CCRD_PT_1] = { 0xff, 0xf8, 0xf0 }, /* Tune_1 */
				[CCRD_PT_2] = { 0xff, 0xf9, 0xf6 }, /* Tune_2 */
				[CCRD_PT_3] = { 0xff, 0xfb, 0xfd }, /* Tune_3 */
				[CCRD_PT_4] = { 0xff, 0xfb, 0xf0 }, /* Tune_4 */
				[CCRD_PT_5] = { 0xff, 0xfc, 0xf6 }, /* Tune_5 */
				[CCRD_PT_6] = { 0xff, 0xff, 0xfd }, /* Tune_6 */
				[CCRD_PT_7] = { 0xff, 0xfe, 0xf0 }, /* Tune_7 */
				[CCRD_PT_8] = { 0xff, 0xff, 0xf6 }, /* Tune_8 */
				[CCRD_PT_9] = { 0xfc, 0xff, 0xfa }, /* Tune_9 */
			},
		},
		.num_ldu_mode = MAX_LDU_MODE,
		.num_night_level = TEST_MDNIE_MAX_NIGHT_LEVEL,
		.num_color_lens_color = COLOR_LENS_COLOR_MAX,
		.num_color_lens_level = COLOR_LENS_LEVEL_MAX,
	};

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel_create_lcd_device(panel, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->lcd_dev);

	INIT_LIST_HEAD(&panel->maptbl_list);
	INIT_LIST_HEAD(&panel->seq_list);
	INIT_LIST_HEAD(&panel->rdi_list);
	INIT_LIST_HEAD(&panel->res_list);
	INIT_LIST_HEAD(&panel->key_list);
	INIT_LIST_HEAD(&panel->dump_list);
	INIT_LIST_HEAD(&panel->pkt_list);
	INIT_LIST_HEAD(&panel->dly_list);
	INIT_LIST_HEAD(&panel->cond_list);
	INIT_LIST_HEAD(&panel->pwrctrl_list);
	INIT_LIST_HEAD(&panel->cfg_list);
	INIT_LIST_HEAD(&panel->func_list);

	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);
	INIT_LIST_HEAD(&panel->prop_list);

	KUNIT_ASSERT_EQ(test, mdnie_set_name(&panel->mdnie, 0), 0);
	KUNIT_ASSERT_STREQ(test, mdnie_get_name(&panel->mdnie), "mdnie");

	ctx->mdnie = &panel->mdnie;

	mdnie_tune = kunit_kzalloc(test, sizeof(*mdnie_tune), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mdnie_tune);
	memcpy(mdnie_tune, &default_mdnie_tune, sizeof(*mdnie_tune));
	ctx->mdnie_tune = mdnie_tune;

	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void mdnie_test_exit(struct kunit *test)
{
	struct mdnie_test *ctx = test->priv;
	struct mdnie_info *mdnie = ctx->mdnie;
	struct panel_device *panel;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mdnie);
	panel = to_panel_device(mdnie);

	panel_destroy_lcd_device(panel);
	KUNIT_ASSERT_TRUE(test, !panel->lcd_dev);

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case mdnie_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(mdnie_test_test),
	KUNIT_CASE(mdnie_test_macro_test),

	KUNIT_CASE(mdnie_test_mdnie_set_def_wrgb_fail_with_invalid_argument),
	KUNIT_CASE(mdnie_test_mdnie_set_def_wrgb_success),

	KUNIT_CASE(mdnie_test_mdnie_set_cur_wrgb_fail_with_invalid_argument),
	KUNIT_CASE(mdnie_test_mdnie_set_cur_wrgb_success),

	KUNIT_CASE(mdnie_test_mdnie_cur_wrgb_to_byte_array_fail_with_invalid_argument),
	KUNIT_CASE(mdnie_test_mdnie_cur_wrgb_to_byte_array_success),

	KUNIT_CASE(mdnie_test_mdnie_update_wrgb_fail_with_invalid_argument),
	KUNIT_CASE(mdnie_test_mdnie_update_wrgb_fail_with_invalid_scr_white_mode),
	KUNIT_CASE(mdnie_test_mdnie_update_wrgb_should_update_def_wrgb_and_cur_wrgb_in_color_coordinate_mode),
	KUNIT_CASE(mdnie_test_mdnie_update_wrgb_should_update_cur_wrgb_and_keep_def_wrgb_in_adjust_ldu_mode),
	KUNIT_CASE(mdnie_test_mdnie_update_wrgb_should_cur_wrgb_is_updated_in_sensor_rgb_mode),

	KUNIT_CASE(mdnie_test_mdnie_set_name_fail_with_invalid_args),
	KUNIT_CASE(mdnie_test_mdnie_get_name_return_null_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_set_name_success),

	KUNIT_CASE(mdnie_test_mdnie_destroy_class_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_destroy_class_fail_if_mdnie_class_is_not_exist),

	KUNIT_CASE(mdnie_test_mdnie_create_class_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_create_and_destroy_class_success),

	KUNIT_CASE(mdnie_test_mdnie_destroy_device_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_destroy_device_fail_if_mdnie_device_is_not_exist),

	KUNIT_CASE(mdnie_test_mdnie_create_device_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_create_device_fail_if_mdnie_class_is_not_exist),
	KUNIT_CASE(mdnie_test_mdnie_create_device_fail_if_panel_lcd_device_is_not_exist),
	KUNIT_CASE(mdnie_test_mdnie_create_and_destroy_device_success),

	KUNIT_CASE(mdnie_test_mdnie_remove_device_files_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_remove_device_files_fail_if_mdnie_device_is_not_exist),

	KUNIT_CASE(mdnie_test_mdnie_create_device_files_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_create_device_files_fail_if_mdnie_device_is_not_exist),
	KUNIT_CASE(mdnie_test_mdnie_create_and_remove_device_files_success),

	KUNIT_CASE(mdnie_test_mdnie_init_property_fail_with_invalid_args),
	KUNIT_CASE(mdnie_test_mdnie_init_property_success),
	KUNIT_CASE(mdnie_test_mdnie_set_property_success),
	KUNIT_CASE(mdnie_test_mdnie_set_property_failure),

	KUNIT_CASE(mdnie_test_mdnie_get_coordinate_fail_with_invalid_args),
	KUNIT_CASE(mdnie_test_mdnie_get_coordinate_fail_if_coordinate_resource_is_not_initialized),

	KUNIT_CASE(mdnie_test_mdnie_init_coordinate_tune_fail_with_invalid_args),

	KUNIT_CASE(mdnie_test_mdnie_create_class_and_device_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_remove_class_and_device_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_create_and_remove_class_and_device_success),

	KUNIT_CASE(mdnie_test_mdnie_init_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_exit_fail_with_invalid_arg),
	KUNIT_CASE(mdnie_test_mdnie_init_and_exit_success),
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
static struct kunit_suite mdnie_test_module = {
	.name = "mdnie_test",
	.init = mdnie_test_init,
	.exit = mdnie_test_exit,
	.test_cases = mdnie_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&mdnie_test_module);

MODULE_LICENSE("GPL v2");
