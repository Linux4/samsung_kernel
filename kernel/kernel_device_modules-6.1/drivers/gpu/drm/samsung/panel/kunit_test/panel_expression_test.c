// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/list_sort.h>

#include "panel_drv.h"
#include "panel_property.h"
#include "panel_expression.h"
#include "panel_function.h"

extern bool valid_panel_expr_infix(struct panel_expr_data *data, size_t num_data);
extern int rule_operator_precedence(struct panel_expr_data *data);
extern int snprintf_panel_expr_data(char *buf, size_t size, struct panel_expr_data *data);
extern void exprtree_inorder(struct panel_expr_node *node, struct list_head *inorder_head);
extern void exprtree_postorder(struct panel_expr_node *node, struct list_head *postorder_head);
extern void exprtree_delete(struct panel_expr_node *node);
extern struct panel_expr_data *infix_to_postfix(struct panel_expr_data *infix, size_t num_infix);
extern struct panel_expr_node *panel_expr_from_infix(struct panel_expr_data *data, size_t num_data);
extern struct panel_expr_node *panel_expr_from_postfix(struct panel_expr_data *data, size_t num_data);

static bool condition_true(struct panel_device *panel) { return true; }
static bool condition_false(struct panel_device *panel) { return false; }

static DEFINE_PNOBJ_FUNC(condition_true, condition_true);
static DEFINE_PNOBJ_FUNC(condition_false, condition_false);

struct panel_expression_test {
	struct panel_device *panel;
};

/*
 * This is the most fundamental dataent of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct kunit *)`. `struct kunit` is a context object that stores
 * information about the current test.
 */

/* NOTE: UML TC */
static void panel_expression_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_expression_test_panel_valid_panel_expr_infix_should_return_false_with_invalid_infix_expression(struct kunit *test)
{
	struct panel_expr_data error_case1[] = {
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
	};
	struct panel_expr_data error_case2[] = { PANEL_EXPR_OPERATOR(OR) };
	struct panel_expr_data error_case3[] = { PANEL_EXPR_OPERATOR(FROM) };
	struct panel_expr_data error_case4[] = { PANEL_EXPR_OPERATOR(TO) };
	struct panel_expr_data error_case5[] = {
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
		PANEL_EXPR_OPERATOR(NOT),
	};
	struct panel_expr_data error_case6[] = {
		PANEL_EXPR_OPERATOR(AND),
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
	};
	struct panel_expr_data error_case7[] = {
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
		PANEL_EXPR_OPERATOR(AND),
	};
	struct panel_expr_data error_case8[] = {
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
		PANEL_EXPR_OPERATOR(AND),
	};
	struct panel_expr_data error_case9[] = {
		PANEL_EXPR_OPERATOR(FROM),
		PANEL_EXPR_OPERATOR(TO),
	};
	struct panel_expr_data error_case10[] = {
		PANEL_EXPR_OPERATOR(FROM),
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
		PANEL_EXPR_OPERATOR(TO),
		PANEL_EXPR_OPERATOR(FROM),
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
		PANEL_EXPR_OPERATOR(TO),
	};

	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case1, ARRAY_SIZE(error_case1)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case2, ARRAY_SIZE(error_case2)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case3, ARRAY_SIZE(error_case3)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case4, ARRAY_SIZE(error_case4)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case5, ARRAY_SIZE(error_case5)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case6, ARRAY_SIZE(error_case6)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case7, ARRAY_SIZE(error_case7)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case8, ARRAY_SIZE(error_case8)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case9, ARRAY_SIZE(error_case9)));
	KUNIT_EXPECT_FALSE(test, valid_panel_expr_infix(error_case10, ARRAY_SIZE(error_case10)));
}

static void panel_expression_test_infix_to_postfix(struct kunit *test)
{
	char *buf = kunit_kzalloc(test, SZ_128, GFP_KERNEL);
	int len = 0;
	/*
	 * !( [brightness] > 260 ) &&
	 * ([panel_refresh_rate] == 60 || [panel_refresh_rate] == 120)
	 */
	struct panel_expr_data infix[] = {
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERAND_FUNC(&PNOBJ_FUNC(condition_true)),
		PANEL_EXPR_OPERATOR(AND),
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
			PANEL_EXPR_OPERATOR(GT),
			PANEL_EXPR_OPERAND_U32(260),
		PANEL_EXPR_OPERATOR(TO),

		PANEL_EXPR_OPERATOR(AND),

		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(60),
			PANEL_EXPR_OPERATOR(OR),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(120),
		PANEL_EXPR_OPERATOR(TO),
	};
	struct panel_expr_data *postfix = infix_to_postfix(infix, ARRAY_SIZE(infix));
	int i, j;

	KUNIT_ASSERT_TRUE(test, postfix != NULL);

	for (i = 0, j = 0; i < ARRAY_SIZE(infix); i++) {
		if (IS_PANEL_EXPR_OPERATOR_FROM(infix[i].type) ||
			IS_PANEL_EXPR_OPERATOR_TO(infix[i].type))
			continue;
		len += snprintf_panel_expr_data(buf + len, SZ_128 - len, &postfix[j++]);
	}

	KUNIT_ASSERT_STREQ(test, buf, "[condition_true()]!!!![brightness]260>&&[panel_refresh_rate]60==[panel_refresh_rate]120==||&&");

	kfree(postfix);
}

static void panel_expression_test_panel_expr_from_infix(struct kunit *test)
{
	/*
	 * !([brightness] > 260) &&
	 * ([panel_refresh_rate] == 60 || [panel_refresh_rate] == 120)
	 */
	struct panel_expr_data infix[] = {
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
			PANEL_EXPR_OPERATOR(GT),
			PANEL_EXPR_OPERAND_U32(260),
		PANEL_EXPR_OPERATOR(TO),

		PANEL_EXPR_OPERATOR(AND),

		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(60),
			PANEL_EXPR_OPERATOR(OR),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(120),
		PANEL_EXPR_OPERATOR(TO),
	};
	struct list_head exprtree_head;
	struct list_head postorder_head;
	struct panel_expr_node *pos, *exprtree_root;
	char *buf = kunit_kzalloc(test, SZ_128, GFP_KERNEL);
	int len = 0;

	INIT_LIST_HEAD(&exprtree_head);
	INIT_LIST_HEAD(&postorder_head);

	exprtree_root = panel_expr_from_infix(infix, ARRAY_SIZE(infix));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);

	exprtree_postorder(exprtree_root, &postorder_head);

	list_for_each_entry(pos, &postorder_head, list)
		len += snprintf_panel_expr_data(buf + len, SZ_128 - len, &pos->data);
	KUNIT_ASSERT_STREQ(test, buf, "[brightness]260>![panel_refresh_rate]60==[panel_refresh_rate]120==||&&");

	exprtree_delete(exprtree_root);
}

static void panel_expression_test_panel_expr_from_postfix(struct kunit *test)
{
	/* [brightness] 260 > */
	struct panel_expr_data postfix[] = {
		PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
		PANEL_EXPR_OPERAND_U32(260),
		PANEL_EXPR_OPERATOR(GT),
	};
	struct list_head exprtree_head;
	struct list_head inorder_head;
	struct panel_expr_node *pos, *exprtree_root;
	char *buf = kunit_kzalloc(test, SZ_128, GFP_KERNEL);
	int len = 0;

	INIT_LIST_HEAD(&exprtree_head);
	INIT_LIST_HEAD(&inorder_head);

	KUNIT_ASSERT_TRUE(test, postfix != NULL);
	KUNIT_ASSERT_TRUE(test, !memcmp(postfix,
				postfix, sizeof(postfix)));

	exprtree_root = panel_expr_from_postfix(postfix,
			ARRAY_SIZE(postfix));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);

	exprtree_inorder(exprtree_root, &inorder_head);

	list_for_each_entry(pos, &inorder_head, list)
		len += snprintf_panel_expr_data(buf + len, SZ_128 - len, &pos->data);
	KUNIT_ASSERT_STREQ(test, buf, "[brightness]>260");

	exprtree_delete(exprtree_root);
}

static void panel_expression_test_panel_expr_eval_unary_operation(struct kunit *test)
{
	struct panel_expression_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_expr_data expr_not[] = {
		/* !128 */
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERAND_U32(128),
	};
	struct panel_expr_data expr_not_not[] = {
		/* !!128 */
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERAND_U32(128),
	};
	struct panel_expr_data expr_test1[] = {
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERATOR(NOT),
			PANEL_EXPR_OPERATOR(FROM),
				PANEL_EXPR_OPERATOR(NOT),
				PANEL_EXPR_OPERATOR(FROM),
					PANEL_EXPR_OPERATOR(FROM),
					PANEL_EXPR_OPERAND_U32(260),
					PANEL_EXPR_OPERATOR(LT),
					PANEL_EXPR_OPERAND_U32(500),
					PANEL_EXPR_OPERATOR(TO),
					PANEL_EXPR_OPERATOR(GT),
					PANEL_EXPR_OPERATOR(NOT),
					PANEL_EXPR_OPERATOR(NOT),
					PANEL_EXPR_OPERAND_U32(0),
				PANEL_EXPR_OPERATOR(TO),
			PANEL_EXPR_OPERATOR(TO),
		PANEL_EXPR_OPERATOR(TO),
	};
	struct panel_expr_data expr_test2[] = {
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_FUNC(&PNOBJ_FUNC(condition_true)),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(true),
		PANEL_EXPR_OPERATOR(TO),
		PANEL_EXPR_OPERATOR(AND),
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERAND_FUNC(&PNOBJ_FUNC(condition_false)),
	};
	struct panel_expr_node *exprtree_root;

	exprtree_root = panel_expr_from_infix(expr_not, ARRAY_SIZE(expr_not));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_not_not, ARRAY_SIZE(expr_not_not));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_test1, ARRAY_SIZE(expr_test1));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_test2, ARRAY_SIZE(expr_test2));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	exprtree_delete(exprtree_root);
}

static void panel_expression_test_panel_expr_arithmetic_operation(struct kunit *test)
{
	struct panel_expression_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_expr_data expr_modulo[] = {
		/* !([panel_refresh_rate] % 48) */
		PANEL_EXPR_OPERATOR(NOT),
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(MOD),
			PANEL_EXPR_OPERAND_U32(48),
		PANEL_EXPR_OPERATOR(TO),
	};
	struct panel_expr_node *exprtree_root;

	exprtree_root = panel_expr_from_infix(expr_modulo, ARRAY_SIZE(expr_modulo));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 48);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 96);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	exprtree_delete(exprtree_root);
}

static void panel_expression_test_panel_expr_eval_bit_operation(struct kunit *test)
{
	struct panel_expression_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_expr_data expr_bit_and[] = {
		/* ([panel_id_3] & 0xF) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_ID_3),
		PANEL_EXPR_OPERATOR(BIT_AND),
		PANEL_EXPR_OPERAND_U32(0xF),
	};
	struct panel_expr_data expr_bit_or[] = {
		/* ([panel_id_3] | 0xF) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_ID_3),
		PANEL_EXPR_OPERATOR(BIT_AND),
		PANEL_EXPR_OPERAND_U32(0xF),
	};
	struct panel_expr_node *exprtree_root;

	exprtree_root = panel_expr_from_infix(expr_bit_and, ARRAY_SIZE(expr_bit_and));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_ID_3, 0x14);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), 0x04);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_bit_or, ARRAY_SIZE(expr_bit_or));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_ID_3, 0x14);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), 0x04);
	exprtree_delete(exprtree_root);
}

static void panel_expression_test_panel_expr_eval_relational_expression(struct kunit *test)
{
	struct panel_expression_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_expr_data expr_less_than[] = {
		/* ([panel_refresh_rate] < 60) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(LT),
		PANEL_EXPR_OPERAND_U32(60),
	};
	struct panel_expr_data expr_less_than_or_equal[] = {
		/* ([panel_refresh_rate] <= 60) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(LE),
		PANEL_EXPR_OPERAND_U32(60),
	};
	struct panel_expr_data expr_greater_than[] = {
		/* ([panel_refresh_rate] > 60) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(GT),
		PANEL_EXPR_OPERAND_U32(60),
	};
	struct panel_expr_data expr_greater_than_or_equal[] = {
		/* ([panel_refresh_rate] >= 60) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(GE),
		PANEL_EXPR_OPERAND_U32(60),
	};
	struct panel_expr_data expr_equal[] = {
		/* ([panel_refresh_rate] == 60) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(EQ),
		PANEL_EXPR_OPERAND_U32(60),
	};
	struct panel_expr_data expr_not_equal[] = {
		/* ([panel_refresh_rate] == 60) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(NE),
		PANEL_EXPR_OPERAND_U32(60),
	};
	struct panel_expr_node *exprtree_root;

	exprtree_root = panel_expr_from_infix(expr_less_than, ARRAY_SIZE(expr_less_than));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 30);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_less_than_or_equal, ARRAY_SIZE(expr_less_than_or_equal));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_greater_than, ARRAY_SIZE(expr_greater_than));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_greater_than_or_equal, ARRAY_SIZE(expr_greater_than_or_equal));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 30);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_equal, ARRAY_SIZE(expr_equal));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 30);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_not_equal, ARRAY_SIZE(expr_not_equal));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 30);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	exprtree_delete(exprtree_root);
}

static void panel_expression_test_panel_expr_eval_logical_expression(struct kunit *test)
{
	struct panel_expression_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_expr_data expr_and[] = {
		/* ([panel_refresh_rate] == 60) && ([panel_refresh_mode] == VRR_HS_MODE) */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(EQ),
		PANEL_EXPR_OPERAND_U32(60),

		PANEL_EXPR_OPERATOR(AND),

		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_MODE),
		PANEL_EXPR_OPERATOR(EQ),
		PANEL_EXPR_OPERAND_U32(VRR_HS_MODE),
	};
	struct panel_expr_data expr_or[] = {
		/*
		 * ([panel_refresh_rate] == 48 ||
		 *  [panel_refresh_rate] == 60 ||
		 *  [panel_refresh_rate] == 96)
		 */
		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(EQ),
		PANEL_EXPR_OPERAND_U32(48),

		PANEL_EXPR_OPERATOR(OR),

		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(EQ),
		PANEL_EXPR_OPERAND_U32(60),

		PANEL_EXPR_OPERATOR(OR),

		PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
		PANEL_EXPR_OPERATOR(EQ),
		PANEL_EXPR_OPERAND_U32(96),
	};
	struct panel_expr_data expr_and_or_mix[] = {
		/*
		 * ([panel_refresh_rate] == 48 ||
		 *  [panel_refresh_rate] == 60 ||
		 *  [panel_refresh_rate] == 96) &&
		 * ([panel_refresh_mode] == VRR_HS_MODE)
		 */
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(48),

			PANEL_EXPR_OPERATOR(OR),

			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(60),

			PANEL_EXPR_OPERATOR(OR),

			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(96),
		PANEL_EXPR_OPERATOR(TO),

		PANEL_EXPR_OPERATOR(AND),

		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_MODE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(VRR_HS_MODE),
		PANEL_EXPR_OPERATOR(TO),
	};
	struct panel_expr_node *exprtree_root;

	exprtree_root = panel_expr_from_infix(expr_and, ARRAY_SIZE(expr_and));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_or, ARRAY_SIZE(expr_or));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 48);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 96);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	exprtree_delete(exprtree_root);

	exprtree_root = panel_expr_from_infix(expr_and_or_mix, ARRAY_SIZE(expr_and_or_mix));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 48);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 96);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_NORMAL_MODE);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);
	exprtree_delete(exprtree_root);
}

static void panel_expression_test_panel_expr_eval(struct kunit *test)
{
	struct panel_expression_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_expr_data infix[] = {
		/* ([panel_refresh_mode] == VRR_HS_MODE) */
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_MODE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(VRR_HS_MODE),
		PANEL_EXPR_OPERATOR(TO),

		/* && */
		PANEL_EXPR_OPERATOR(AND),

		/*
		 * ([panel_refresh_rate] == 48 ||
		 *  [panel_refresh_rate] == 60 ||
		 *  [panel_refresh_rate] == 96)
		 */
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(48),

			PANEL_EXPR_OPERATOR(OR),

			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(60),

			PANEL_EXPR_OPERATOR(OR),

			PANEL_EXPR_OPERAND_PROP(PANEL_PROPERTY_PANEL_REFRESH_RATE),
			PANEL_EXPR_OPERATOR(EQ),
			PANEL_EXPR_OPERAND_U32(96),
		PANEL_EXPR_OPERATOR(TO),

		/* && */
		PANEL_EXPR_OPERATOR(AND),

		/* ([brightness] > 260) */
		PANEL_EXPR_OPERATOR(FROM),
			PANEL_EXPR_OPERAND_PROP(PANEL_BL_PROPERTY_BRIGHTNESS),
			PANEL_EXPR_OPERATOR(GT),
			PANEL_EXPR_OPERAND_U32(260),
		PANEL_EXPR_OPERATOR(TO),
	};
	struct panel_expr_node *exprtree_root;

	exprtree_root = panel_expr_from_infix(infix, ARRAY_SIZE(infix));
	KUNIT_ASSERT_TRUE(test, exprtree_root != NULL);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_NORMAL_MODE);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_BL_PROPERTY_BRIGHTNESS, 322);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	panel_set_property_value(panel, PANEL_BL_PROPERTY_BRIGHTNESS, 322);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_BL_PROPERTY_BRIGHTNESS, 260);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)false);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_BL_PROPERTY_BRIGHTNESS, 322);
	KUNIT_EXPECT_EQ(test, panel_expr_eval(panel, exprtree_root), (int)true);

	exprtree_delete(exprtree_root);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_expression_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_expression_test *ctx = kunit_kzalloc(test,
			sizeof(struct panel_expression_test), GFP_KERNEL);
	struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};
	struct panel_prop_enum_item wait_tx_done_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_AUTO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_ON),
	};
	struct panel_prop_enum_item separate_tx_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_ON),
	};
	struct panel_prop_enum_item vrr_mode_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_NORMAL_MODE),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_HS_MODE),
	};
	struct panel_prop_list prop_array[] = {
		/* enum property */
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_STATE,
				PANEL_STATE_OFF, panel_state_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_WAIT_TX_DONE,
				WAIT_TX_DONE_AUTO, wait_tx_done_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_SEPARATE_TX,
				SEPARATE_TX_OFF, separate_tx_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items),
		/* range property */
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_1,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_2,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_3,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE,
				60, 0, 120),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE,
				60, 0, 120),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_RESOLUTION_CHANGED,
				false, false, true),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_DSI_FREQ,
				0, 0, 1000000000),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_OSC_FREQ,
				0, 0, 1000000000),
#ifdef CONFIG_USDM_FACTORY
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_IS_FACTORY_MODE,
				1, 0, 1),
#else
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_IS_FACTORY_MODE,
				0, 0, 1),
#endif
		/* DISPLAY TEST */
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE,
				0, 0, 1),
#endif
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_BL_PROPERTY_BRIGHTNESS,
				128, 0, 1000000000),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_BL_PROPERTY_PREV_BRIGHTNESS,
				128, 0, 1000000000),
	};

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	INIT_LIST_HEAD(&panel->prop_list);

	KUNIT_ASSERT_TRUE(test, !panel_add_property_from_array(panel,
			prop_array, ARRAY_SIZE(prop_array)));

	ctx->panel = panel;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_expression_test_exit(struct kunit *test)
{
	struct panel_expression_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_delete_property_all(panel);
	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_expression_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_expression_test_test),
	KUNIT_CASE(panel_expression_test_panel_valid_panel_expr_infix_should_return_false_with_invalid_infix_expression),
	KUNIT_CASE(panel_expression_test_infix_to_postfix),
	KUNIT_CASE(panel_expression_test_panel_expr_from_infix),
	KUNIT_CASE(panel_expression_test_panel_expr_from_postfix),
	KUNIT_CASE(panel_expression_test_panel_expr_eval_unary_operation),
	KUNIT_CASE(panel_expression_test_panel_expr_arithmetic_operation),
	KUNIT_CASE(panel_expression_test_panel_expr_eval_bit_operation),
	KUNIT_CASE(panel_expression_test_panel_expr_eval_relational_expression),
	KUNIT_CASE(panel_expression_test_panel_expr_eval_logical_expression),
	KUNIT_CASE(panel_expression_test_panel_expr_eval),
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
static struct kunit_suite panel_expression_test_module = {
	.name = "panel_expression_test",
	.init = panel_expression_test_init,
	.exit = panel_expression_test_exit,
	.test_cases = panel_expression_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_expression_test_module);

MODULE_LICENSE("GPL v2");
