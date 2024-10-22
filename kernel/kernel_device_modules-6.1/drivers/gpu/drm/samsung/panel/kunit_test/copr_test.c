// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "copr.h"

extern struct class *lcd_class;

struct copr_test {
	struct panel_device *panel;
	struct copr_info *copr;
};

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
static void copr_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void copr_test_copy_reg_to_byte_array_approval_test(struct kunit *test)
{
	/* copy from s6e3hae_rainbow_b0_panel_copr.h */
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_EN = (1);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_PWR = (1);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_MASK = (0);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI_CTRL = (0x1F);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_GAMMA = (0);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_FRAME_COUNT = (0); /* deprecated */

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI1_ER = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI1_EG = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI1_EB = (256);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI2_ER = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI2_EG = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI2_EB = (256);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI3_ER = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI3_EG = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI3_EB = (256);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI4_ER = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI4_EG = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI4_EB = (256);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI5_ER = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI5_EG = (256);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI5_EB = (256);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI1_X_S = (739);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI1_Y_S = (243);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI1_X_E = (771);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI1_Y_E = (279);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI2_X_S = (0);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI2_Y_S = (0);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI2_X_E = (1439);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI2_Y_E = (3087);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI3_X_S = (723);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI3_Y_S = (223);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI3_X_E = (787);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI3_Y_E = (299);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI4_X_S = (707);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI4_Y_S = (211);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI4_X_E = (803);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI4_Y_E = (311);

	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI5_X_S = (0);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI5_Y_S = (0);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI5_X_E = (1439);
	const unsigned int S6E3HAE_RAINBOW_B0_COPR_ROI5_Y_E = (3087);
	struct copr_reg_v6 test_reg_v6 = {
		.copr_mask = S6E3HAE_RAINBOW_B0_COPR_MASK,
		.copr_pwr = S6E3HAE_RAINBOW_B0_COPR_PWR,
		.copr_en = S6E3HAE_RAINBOW_B0_COPR_EN,
		.copr_gamma = S6E3HAE_RAINBOW_B0_COPR_GAMMA,
		.copr_frame_count = S6E3HAE_RAINBOW_B0_COPR_FRAME_COUNT,
		.roi_on = S6E3HAE_RAINBOW_B0_COPR_ROI_CTRL,
		.roi = {
			[0] = {
				.roi_er = S6E3HAE_RAINBOW_B0_COPR_ROI1_ER,
				.roi_eg = S6E3HAE_RAINBOW_B0_COPR_ROI1_EG,
				.roi_eb = S6E3HAE_RAINBOW_B0_COPR_ROI1_EB,
				.roi_xs = S6E3HAE_RAINBOW_B0_COPR_ROI1_X_S,
				.roi_ys = S6E3HAE_RAINBOW_B0_COPR_ROI1_Y_S,
				.roi_xe = S6E3HAE_RAINBOW_B0_COPR_ROI1_X_E,
				.roi_ye = S6E3HAE_RAINBOW_B0_COPR_ROI1_Y_E,
			},
			[1] = {
				.roi_er = S6E3HAE_RAINBOW_B0_COPR_ROI2_ER,
				.roi_eg = S6E3HAE_RAINBOW_B0_COPR_ROI2_EG,
				.roi_eb = S6E3HAE_RAINBOW_B0_COPR_ROI2_EB,
				.roi_xs = S6E3HAE_RAINBOW_B0_COPR_ROI2_X_S,
				.roi_ys = S6E3HAE_RAINBOW_B0_COPR_ROI2_Y_S,
				.roi_xe = S6E3HAE_RAINBOW_B0_COPR_ROI2_X_E,
				.roi_ye = S6E3HAE_RAINBOW_B0_COPR_ROI2_Y_E,
			},
			[2] = {
				.roi_er = S6E3HAE_RAINBOW_B0_COPR_ROI3_ER,
				.roi_eg = S6E3HAE_RAINBOW_B0_COPR_ROI3_EG,
				.roi_eb = S6E3HAE_RAINBOW_B0_COPR_ROI3_EB,
				.roi_xs = S6E3HAE_RAINBOW_B0_COPR_ROI3_X_S,
				.roi_ys = S6E3HAE_RAINBOW_B0_COPR_ROI3_Y_S,
				.roi_xe = S6E3HAE_RAINBOW_B0_COPR_ROI3_X_E,
				.roi_ye = S6E3HAE_RAINBOW_B0_COPR_ROI3_Y_E,
			},
			[3] = {
				.roi_er = S6E3HAE_RAINBOW_B0_COPR_ROI4_ER,
				.roi_eg = S6E3HAE_RAINBOW_B0_COPR_ROI4_EG,
				.roi_eb = S6E3HAE_RAINBOW_B0_COPR_ROI4_EB,
				.roi_xs = S6E3HAE_RAINBOW_B0_COPR_ROI4_X_S,
				.roi_ys = S6E3HAE_RAINBOW_B0_COPR_ROI4_Y_S,
				.roi_xe = S6E3HAE_RAINBOW_B0_COPR_ROI4_X_E,
				.roi_ye = S6E3HAE_RAINBOW_B0_COPR_ROI4_Y_E,
			},
			[4] = {
				.roi_er = S6E3HAE_RAINBOW_B0_COPR_ROI5_ER,
				.roi_eg = S6E3HAE_RAINBOW_B0_COPR_ROI5_EG,
				.roi_eb = S6E3HAE_RAINBOW_B0_COPR_ROI5_EB,
				.roi_xs = S6E3HAE_RAINBOW_B0_COPR_ROI5_X_S,
				.roi_ys = S6E3HAE_RAINBOW_B0_COPR_ROI5_Y_S,
				.roi_xe = S6E3HAE_RAINBOW_B0_COPR_ROI5_X_E,
				.roi_ye = S6E3HAE_RAINBOW_B0_COPR_ROI5_Y_E,
			},
		},
	};
	u8 approval_dst[COPR_V6_CTRL_REG_SIZE] = {
		0x03, 0x1F, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
		0x00, 0x01, 0x00, 0x02, 0xE3, 0x00, 0xF3, 0x03, 0x03, 0x01, 0x17, 0x00, 0x00, 0x00, 0x00, 0x05,
		0x9F, 0x0C, 0x0F, 0x02, 0xD3, 0x00, 0xDF, 0x03, 0x13, 0x01, 0x2B, 0x02, 0xC3, 0x00, 0xD3, 0x03,
		0x23, 0x01, 0x37, 0x00, 0x00, 0x00, 0x00, 0x05, 0x9F, 0x0C, 0x0F
	};
	u8 received_dst[COPR_V6_CTRL_REG_SIZE] = { 0, };
	unsigned int i;
	char message[256];

	copr_reg_to_byte_array((struct copr_reg *)&test_reg_v6,
			COPR_VER_6, received_dst);
	for (i = 0; i < ARRAY_SIZE(approval_dst); i++) {
		/*
		 * read 0xE1 register from rainbow b0 panel to approval test
		 * COPR_MASK register is automatically set by 1 when COPR_MASK=0.
		 * so when comare approval and received buffer, masking is needed.
		 */
		u32 mask = (i == 0) ? 0x03 : 0xFF;
		u32 len = snprintf(message, sizeof(message),
				"Expected (approval_dst[%d] & 0x%02X) == (received_dst[%d] & 0x%02X), but\n", i, mask, i, mask);
		len += snprintf(message + len, sizeof(message) - len,
				"(approval_dst[%d] & 0x%02X) == 0x%02X\n", i, mask, approval_dst[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"(received_dst[%d] & 0x%02X) == 0x%02X\n", i, mask, received_dst[i]);
		KUNIT_EXPECT_TRUE_MSG(test, ((approval_dst[i] & mask) == (received_dst[i] & mask)), message);
	}
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int copr_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct copr_test *ctx =
		kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel->of_node_name = "test_panel_name";
	panel->dev = device_create(lcd_class,
			NULL, 0, panel, PANEL_DRV_NAME);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->dev);

	panel_mutex_init(panel, &panel->io_lock);
	panel_mutex_init(panel, &panel->op_lock);
	panel_mutex_init(panel, &panel->cmdq.lock);
	panel_mutex_init(panel, &panel->copr.lock);

	INIT_LIST_HEAD(&panel->command_initdata_list);

	INIT_LIST_HEAD(&panel->maptbl_list);
	INIT_LIST_HEAD(&panel->seq_list);
	INIT_LIST_HEAD(&panel->rdi_list);
	INIT_LIST_HEAD(&panel->res_list);
	INIT_LIST_HEAD(&panel->dump_list);
	INIT_LIST_HEAD(&panel->key_list);
	INIT_LIST_HEAD(&panel->pkt_list);
	INIT_LIST_HEAD(&panel->dly_list);
	INIT_LIST_HEAD(&panel->cond_list);
	INIT_LIST_HEAD(&panel->pwrctrl_list);
	INIT_LIST_HEAD(&panel->cfg_list);
	INIT_LIST_HEAD(&panel->func_list);

	INIT_LIST_HEAD(&panel->gpio_list);
	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->regulator_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);
	INIT_LIST_HEAD(&panel->prop_list);

	ctx->panel = panel;
	ctx->copr = &panel->copr;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void copr_test_exit(struct kunit *test)
{
	struct copr_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	copr_remove(panel);

	device_unregister(panel->dev);
	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case copr_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(copr_test_test),
	KUNIT_CASE(copr_test_copy_reg_to_byte_array_approval_test),
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
static struct kunit_suite copr_test_module = {
	.name = "copr_test",
	.init = copr_test_init,
	.exit = copr_test_exit,
	.test_cases = copr_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&copr_test_module);

MODULE_LICENSE("GPL v2");
