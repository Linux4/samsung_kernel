// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <media/v4l2-subdev.h>
#include <dt-bindings/display/panel-display.h>
#include "test_helper.h"
#include "test_panel.h"
#include "panel_obj.h"
#include "panel_drv.h"
#include "panel_drv_ioctl.h"
#include "panel_property.h"
#include "mock_panel_drv_funcs.h"

#define DEF_VOLTAGE 3000000
#define LPM_VOLTAGE 1800000
#define DEF_CURRENT 4000
#define LPM_CURRENT 0

struct panel_drv_test {
	struct panel_device *panel;
	struct panel_gpio gpio_list[PANEL_GPIO_MAX];
};

struct prop_test_param {
	char *propname;
	unsigned int param;
	unsigned int expected_param;
};

struct panel_drv_prop_test_param {
	char *prop_name;
	unsigned int *prop_ptr;
	unsigned int value;
};

extern struct list_head panel_device_list;
extern struct class *lcd_class;
extern struct panel_gpio *panel_gpio_list_add(struct panel_device *panel,
	struct panel_gpio *_gpio);
extern void set_panel_id(struct panel_device *panel, unsigned int panel_id);
extern int get_panel_id(struct panel_device *panel);
extern int get_panel_id1(struct panel_device *panel);
extern int get_panel_id2(struct panel_device *panel);
extern int get_panel_id3(struct panel_device *panel);
extern bool panel_is_gpio_valid(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern struct panel_gpio *panel_get_gpio(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern int panel_get_gpio_value(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern int panel_set_gpio_value(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id, int value);
extern int panel_get_gpio_state(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern const char *panel_get_gpio_name(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern bool panel_is_gpio_irq_valid(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern bool panel_is_gpio_irq_enabled(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern int panel_enable_gpio_irq(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern int panel_disable_gpio_irq(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id);
extern int panel_display_on(struct panel_device *panel);
extern int panel_display_off(struct panel_device *panel);
extern int panel_power_on(struct panel_device *panel);
extern int panel_power_off(struct panel_device *panel);
extern int panel_sleep_in(struct panel_device *panel);
extern int panel_sleep_out(struct panel_device *panel);
extern int panel_doze(struct panel_device *panel);
extern int panel_register_isr(struct panel_device *panel);
extern int panel_snprintf_bypass(struct panel_device *panel, char *buf, size_t size);
extern int panel_get_bypass_reason(struct panel_device *panel);
extern const char * const panel_get_bypass_reason_str(struct panel_device *panel);
extern int panel_snprintf_bypass_reason(struct panel_device *panel, char *buf, size_t size);
extern void panel_print_bypass_reason(struct panel_device *panel);
extern int panel_get_gpio_state(struct panel_device *panel, enum panel_gpio_lists panel_gpio_id);
extern int panel_disp_det_state(struct panel_device *panel);
#ifdef CONFIG_USDM_PANEL_ERRFG_RECOVERY
extern int panel_err_fg_state(struct panel_device *panel);
#endif
extern int panel_conn_det_state(struct panel_device *panel);

extern int panel_initialize_regulator(struct panel_device *panel);
extern int panel_enable_regulators(struct panel_device *panel);
extern int panel_disable_regulators(struct panel_device *panel);
extern int panel_set_regulators_voltage(struct panel_device *panel, int state);
extern int panel_set_regulators_current_limit(struct panel_device *panel, int state);
extern int panel_prepare_prop_list(struct panel_device *panel,
		struct common_panel_info *cpi);
extern int panel_unprepare_prop_list(struct panel_device *panel);
extern int panel_prepare(struct panel_device *panel, struct common_panel_info *info);
extern int panel_unprepare(struct panel_device *panel);
extern int panel_setup_command_initdata_list(struct panel_device *panel,
		struct common_panel_info *info);

extern int panel_enable_irq(struct panel_device *panel, u32 pins);
extern int panel_disable_irq(struct panel_device *panel, u32 pins);
extern int panel_poll_irq(struct panel_device *panel, enum panel_gpio_lists pin);
extern int panel_duplicate_packet(struct panel_device *panel, struct pktinfo *src);
extern int panel_duplicate_sequence(struct panel_device *panel, struct seqinfo *src);

/* ===== Setup() Functions ===== */
static int panel_drv_test_panel_gpio_init(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	int panel_gpio_default_active_low[PANEL_GPIO_MAX] = {
		[PANEL_GPIO_RESET] = true,
		[PANEL_GPIO_DISP_DET] = true,
		[PANEL_GPIO_PCD] = true,
		[PANEL_GPIO_ERR_FG] = false,
		[PANEL_GPIO_CONN_DET] = false,
		[PANEL_GPIO_DISP_TE] = false,
	};
	char *panel_gpio_names[PANEL_GPIO_MAX] = {
		[PANEL_GPIO_RESET] = PANEL_GPIO_NAME_RESET,
		[PANEL_GPIO_DISP_DET] = PANEL_GPIO_NAME_DISP_DET,
		[PANEL_GPIO_PCD] = PANEL_GPIO_NAME_PCD,
		[PANEL_GPIO_ERR_FG] = PANEL_GPIO_NAME_ERR_FG,
		[PANEL_GPIO_CONN_DET] = PANEL_GPIO_NAME_CONN_DET,
		[PANEL_GPIO_DISP_TE] = PANEL_GPIO_NAME_DISP_TE,
	};
	int panel_gpio_irq_registered[PANEL_GPIO_MAX] = {
		[PANEL_GPIO_RESET] = false,
		[PANEL_GPIO_DISP_DET] = true,
		[PANEL_GPIO_PCD] = true,
		[PANEL_GPIO_ERR_FG] = true,
		[PANEL_GPIO_CONN_DET] = true,
		[PANEL_GPIO_DISP_TE] = false,
	};

	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	for (i = 0; i < PANEL_GPIO_MAX; i++) {
		struct panel_gpio_funcs *temp_gpio_funcs =
			kunit_kzalloc(test, sizeof(*temp_gpio_funcs), GFP_KERNEL);
		struct panel_gpio *gpio = &ctx->gpio_list[i];

		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, temp_gpio_funcs);

		memcpy(temp_gpio_funcs, &panel_gpio_funcs, sizeof(*temp_gpio_funcs));

		gpio->name = panel_gpio_names[i];
		gpio->num = 100 + i;
		gpio->active_low = panel_gpio_default_active_low[i];
		gpio->funcs = temp_gpio_funcs;
		gpio->irq_pend_reg = NULL;
		gpio->irq_registered = panel_gpio_irq_registered[i];

		list_add(&gpio->head, &panel->gpio_list);
	}

	return 0;
}

static int panel_drv_test_panel_regulator_init(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	static const char *panel_regulator_names[] = {
		"test_vci",
		"test_vdd3",
		"test_vddr",
		"test_ssd",
	};
	int panel_regulator_types[] = {
		PANEL_REGULATOR_TYPE_PWR,
		PANEL_REGULATOR_TYPE_PWR,
		PANEL_REGULATOR_TYPE_PWR,
		PANEL_REGULATOR_TYPE_SSD,
	};
	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	for (i = 0; i < ARRAY_SIZE(panel_regulator_types); i++) {
		struct panel_regulator *regulator = kunit_kzalloc(test, sizeof(*regulator), GFP_KERNEL);

		regulator->funcs = kunit_kzalloc(test, sizeof(*regulator->funcs), GFP_KERNEL);
		regulator->name = panel_regulator_names[i];
		/* unable to create 'struct regulator', so create dummy buffer */
		regulator->reg = kunit_kzalloc(test, 16, GFP_KERNEL);
		regulator->type = panel_regulator_types[i];
		list_add(&regulator->head, &panel->regulator_list);
	}

	return 0;
}

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
static void panel_drv_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_drv_test_get_lcd_info_before_panel_drv_init(struct kunit *test)
{
	extern int boot_panel_id;
	extern int get_lcd_info(char *arg);

	INIT_LIST_HEAD(&panel_device_list);

	/* panel is detected in LK */
	boot_panel_id = 8459585;
	KUNIT_EXPECT_EQ(test, get_lcd_info("id"), 8459585);

	/* panel is not detected in LK */
	boot_panel_id = -1;
	KUNIT_EXPECT_EQ(test, get_lcd_info("id"), 0);
}

static void panel_drv_test_get_lcd_info_after_panel_drv_init(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	extern int boot_panel_id;
	extern int get_lcd_info(char *arg);

	/* panel is detected in LK */
	boot_panel_id = 0;
	set_panel_id(panel, 8459585);
	KUNIT_EXPECT_EQ(test, get_lcd_info("id"), 8459585);

	/* panel is not detected in LK */
	boot_panel_id = -1;
	panel->boot_panel_id = -1;
	set_panel_id(panel, 8459585);
	KUNIT_EXPECT_EQ(test, get_lcd_info("id"), 0);
}

static void panel_drv_test_panel_set_panel_id_test(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	memset(panel->panel_data.id, 0, sizeof(panel->panel_data.id));
	set_panel_id(NULL, 0x123456);
	KUNIT_EXPECT_EQ(test, get_panel_id(panel), (int)0);

	memset(panel->panel_data.id, 0, sizeof(panel->panel_data.id));
	set_panel_id(panel, 0x123456);
	KUNIT_EXPECT_EQ(test, get_panel_id(panel), (int)0x123456);

	memset(panel->panel_data.id, 0, sizeof(panel->panel_data.id));
	set_panel_id(panel, 0x12345678);
	KUNIT_EXPECT_EQ(test, get_panel_id(panel), (int)0x345678);
}

static void panel_drv_test_panel_get_panel_id_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_EQ(test, get_panel_id(NULL), -EINVAL);

	set_panel_id(panel, 0x123456);
	KUNIT_EXPECT_EQ(test, get_panel_id(panel), 0x123456);
	KUNIT_EXPECT_EQ(test, get_panel_id1(panel), 0x12);
	KUNIT_EXPECT_EQ(test, get_panel_id2(panel), 0x34);
	KUNIT_EXPECT_EQ(test, get_panel_id3(panel), 0x56);
}

static void panel_drv_test_panel_get_gpio_return_null_with_invalid_arg(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_TRUE(test, !panel_get_gpio(NULL, PANEL_GPIO_RESET));
	KUNIT_EXPECT_TRUE(test, !panel_get_gpio(panel, PANEL_GPIO_MAX));
}

static void panel_drv_test_panel_get_gpio_return_null_with_invalid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_TRUE(test, !panel_get_gpio(panel, PANEL_GPIO_RESET));
}

static void panel_drv_test_panel_get_gpio_return_panel_gpio_with_valid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, true);

	KUNIT_EXPECT_PTR_EQ(test, panel_get_gpio(panel, PANEL_GPIO_RESET), gp);
}

static void panel_drv_test_panel_set_gpio_value_fail_with_invalid_arg_or_invalid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	KUNIT_EXPECT_EQ(test, panel_set_gpio_value(NULL, PANEL_GPIO_RESET, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_set_gpio_value(panel, PANEL_GPIO_MAX, 1), -EINVAL);

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_set_gpio_value(panel, PANEL_GPIO_RESET, 1), -EINVAL);
}

static void panel_drv_test_panel_set_gpio_value_success_with_valid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->set_value = mock_panel_gpio_set_value;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_set_value(ptr_eq(test, gp), any(test))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_set_gpio_value(panel, PANEL_GPIO_RESET, 1), 0);
}

static void panel_drv_test_panel_is_gpio_valid_return_false_with_invalid_arg(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_FALSE(test, panel_is_gpio_valid(NULL, PANEL_GPIO_RESET));
	KUNIT_EXPECT_FALSE(test, panel_is_gpio_valid(panel, PANEL_GPIO_MAX));
}

static void panel_drv_test_panel_is_gpio_valid_return_false_with_invalid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_FALSE(test, panel_is_gpio_valid(panel, PANEL_GPIO_RESET));
}

static void panel_drv_test_panel_is_gpio_valid_return_true_with_valid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, true);

	KUNIT_EXPECT_TRUE(test, panel_is_gpio_valid(panel, PANEL_GPIO_RESET));
}

static void panel_drv_test_panel_get_gpio_value_fail_with_invalid_arg_or_invalid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	KUNIT_EXPECT_EQ(test, panel_get_gpio_value(NULL, PANEL_GPIO_RESET), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_get_gpio_value(panel, PANEL_GPIO_MAX), -EINVAL);

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_get_gpio_value(panel, PANEL_GPIO_RESET), -EINVAL);
}

static void panel_drv_test_panel_get_gpio_value_success_with_valid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_get_gpio_value(panel, PANEL_GPIO_RESET), 0);
}

static void panel_drv_test_panel_get_gpio_state_fail_with_invalid_arg_or_invalid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	KUNIT_EXPECT_EQ(test, panel_get_gpio_state(NULL, PANEL_GPIO_RESET), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_get_gpio_state(panel, PANEL_GPIO_MAX), -EINVAL);

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_get_gpio_state(panel, PANEL_GPIO_RESET), -EINVAL);
}

static void panel_drv_test_panel_get_gpio_state_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 1);

	gp->active_low = true;
	KUNIT_EXPECT_EQ(test, panel_get_gpio_state(panel, PANEL_GPIO_RESET), PANEL_GPIO_NORMAL_STATE);

	gp->active_low = false;
	KUNIT_EXPECT_EQ(test, panel_get_gpio_state(panel, PANEL_GPIO_RESET), PANEL_GPIO_ABNORMAL_STATE);
}

static void panel_drv_test_panel_get_gpio_name_return_null_with_invalid_arg(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_TRUE(test, !panel_get_gpio_name(NULL, PANEL_GPIO_RESET));
	KUNIT_EXPECT_TRUE(test, !panel_get_gpio_name(panel, PANEL_GPIO_MAX));
}

static void panel_drv_test_panel_get_gpio_name_return_null_with_invalid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_TRUE(test, !panel_get_gpio_name(panel, PANEL_GPIO_RESET));
}

static void panel_drv_test_panel_get_gpio_name_return_gpio_name_with_valid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	const char *name;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_RESET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, true);

	name = panel_get_gpio_name(panel, PANEL_GPIO_RESET);
	KUNIT_EXPECT_PTR_EQ(test, name, gp->name);
	KUNIT_EXPECT_STREQ(test, name, PANEL_GPIO_NAME_RESET);
}

static void panel_drv_test_panel_gpio_irq_wrapper_functions_fail_with_invalid_gpio(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_FALSE(test, panel_is_gpio_irq_valid(panel, PANEL_GPIO_DISP_DET));
	KUNIT_EXPECT_FALSE(test, panel_is_gpio_irq_enabled(panel, PANEL_GPIO_DISP_DET));
	KUNIT_EXPECT_EQ(test, panel_enable_gpio_irq(panel, PANEL_GPIO_DISP_DET), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_disable_gpio_irq(panel, PANEL_GPIO_DISP_DET), -EINVAL);
}

static void panel_drv_test_panel_gpio_irq_wrapper_functions_fail_with_invalid_gpio_irq(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = AtLeast(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation2 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gp))));
	expectation2->action = bool_return(test, false);

	KUNIT_EXPECT_FALSE(test, panel_is_gpio_irq_valid(panel, PANEL_GPIO_DISP_DET));
	KUNIT_EXPECT_FALSE(test, panel_is_gpio_irq_enabled(panel, PANEL_GPIO_DISP_DET));
	KUNIT_EXPECT_EQ(test, panel_enable_gpio_irq(panel, PANEL_GPIO_DISP_DET), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_disable_gpio_irq(panel, PANEL_GPIO_DISP_DET), -EINVAL);
}

static void panel_drv_test_panel_gpio_irq_wrapper_functions_success_with_valid_gpio_irq(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct mock_expectation *expectation3, *expectation4;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = AtLeast(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation2 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gp))));
	expectation2->action = bool_return(test, true);

	gp->funcs->enable_irq = mock_panel_gpio_enable_irq;
	expectation3 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_enable_irq(ptr_eq(test, gp))));
	expectation3->action = int_return(test, 0);

	gp->funcs->disable_irq = mock_panel_gpio_disable_irq;
	expectation4 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_disable_irq(ptr_eq(test, gp))));
	expectation4->action = int_return(test, 0);

	KUNIT_EXPECT_TRUE(test, panel_is_gpio_irq_valid(panel, PANEL_GPIO_DISP_DET));

	gp->irq_enable = true;
	KUNIT_EXPECT_TRUE(test, panel_is_gpio_irq_enabled(panel, PANEL_GPIO_DISP_DET));

	gp->irq_enable = false;
	KUNIT_EXPECT_FALSE(test, panel_is_gpio_irq_enabled(panel, PANEL_GPIO_DISP_DET));

	KUNIT_EXPECT_EQ(test, panel_enable_gpio_irq(panel, PANEL_GPIO_DISP_DET), 0);
	KUNIT_EXPECT_EQ(test, panel_disable_gpio_irq(panel, PANEL_GPIO_DISP_DET), 0);
}

static void panel_drv_test_panel_poll_gpio_should_return_error_if_gpio_is_not_exist(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_TE];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = AtLeast(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, false);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Never(KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_poll_gpio(panel, PANEL_GPIO_DISP_TE, 1, 1, 1), -EINVAL);
}

static void panel_drv_test_panel_poll_gpio_should_return_timeout_error_if_different_with_expected_level(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_TE];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = AtLeast(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = AtLeast(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_poll_gpio(panel, PANEL_GPIO_DISP_TE, 1, 1, 1), -ETIMEDOUT);
}

static void panel_drv_test_panel_poll_gpio_should_return_zero_if_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_TE];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = AtLeast(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = AtLeast(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 1);

	KUNIT_EXPECT_EQ(test, panel_poll_gpio(panel, PANEL_GPIO_DISP_TE, 1, 1, 1), 0);
}

static void panel_drv_test_panel_disp_det_state_should_return_nok_if_avdd_swire_is_low(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_disp_det_state(panel), PANEL_STATE_NOK);
}

static void panel_drv_test_panel_disp_det_state_should_return_ok_if_avdd_swire_is_high(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 1);

	KUNIT_EXPECT_EQ(test, panel_disp_det_state(panel), PANEL_STATE_OK);
}

static void panel_drv_test_panel_irq_enable_functions_success_with_valid_gpio_irq(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2, *expectation3;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_ERR_FG];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->enable_irq = mock_panel_gpio_enable_irq;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_enable_irq(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	gp->funcs->clear_irq_pending_bit = mock_panel_gpio_clear_irq_pending_bit;
	expectation3 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_clear_irq_pending_bit(ptr_eq(test, gp))));
	expectation3->action = int_return(test, 1);

	KUNIT_EXPECT_EQ(test, panel_enable_irq(panel, PANEL_IRQ_ERR_FG), 0);
}

static void panel_drv_test_panel_irq_enable_functions_success_with_valid_multi_gpio_irq(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2, *expectation3;
	struct mock_expectation *expectation4, *expectation5, *expectation6;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_ERR_FG];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->enable_irq = mock_panel_gpio_enable_irq;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_enable_irq(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	gp->funcs->clear_irq_pending_bit = mock_panel_gpio_clear_irq_pending_bit;
	expectation3 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_clear_irq_pending_bit(ptr_eq(test, gp))));
	expectation3->action = int_return(test, 1);

	gp = &ctx->gpio_list[PANEL_GPIO_PCD];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation4 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation4->action = bool_return(test, true);

	gp->funcs->enable_irq = mock_panel_gpio_enable_irq;
	expectation5 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_enable_irq(ptr_eq(test, gp))));
	expectation5->action = int_return(test, 0);

	gp->funcs->clear_irq_pending_bit = mock_panel_gpio_clear_irq_pending_bit;
	expectation6 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_clear_irq_pending_bit(ptr_eq(test, gp))));
	expectation6->action = int_return(test, 1);

	KUNIT_EXPECT_EQ(test, panel_enable_irq(panel, PANEL_IRQ_ERR_FG|PANEL_IRQ_PCD), 0);
}

static void panel_drv_test_panel_irq_disable_functions_success_with_valid_gpio_irq(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_ERR_FG];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->disable_irq = mock_panel_gpio_disable_irq;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_disable_irq(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_disable_irq(panel, PANEL_IRQ_ERR_FG), 0);
}

static void panel_drv_test_panel_irq_disable_functions_success_with_valid_multi_gpio_irq(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct mock_expectation *expectation3, *expectation4;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_ERR_FG];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->disable_irq = mock_panel_gpio_disable_irq;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_disable_irq(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	gp = &ctx->gpio_list[PANEL_GPIO_PCD];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation3 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation3->action = bool_return(test, true);

	gp->funcs->disable_irq = mock_panel_gpio_disable_irq;
	expectation4 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_disable_irq(ptr_eq(test, gp))));
	expectation4->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_disable_irq(panel, PANEL_IRQ_ERR_FG|PANEL_IRQ_PCD), 0);
}

static void panel_drv_test_panel_irq_poll_functions_success_with_valid_gpio_value(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 1);

	KUNIT_EXPECT_EQ(test, panel_poll_irq(panel, PANEL_GPIO_DISP_DET), 0);
}

#ifdef CONFIG_USDM_PANEL_ERRFG_RECOVERY
static void panel_drv_test_panel_err_fg_state_should_return_nok_if_gpio_is_high(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_ERR_FG];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 1);

	KUNIT_EXPECT_EQ(test, panel_err_fg_state(panel), PANEL_STATE_NOK);
}

static void panel_drv_test_panel_err_fg_state_should_return_ok_if_gpio_is_low(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_ERR_FG];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_err_fg_state(panel), PANEL_STATE_OK);
}
#endif

static void panel_drv_test_panel_conn_det_state_should_return_nok_if_gpio_is_high(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_CONN_DET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 1);

	KUNIT_EXPECT_EQ(test, panel_conn_det_state(panel), PANEL_STATE_NOK);
	KUNIT_EXPECT_TRUE(test, panel_disconnected(panel));
}

static void panel_drv_test_panel_conn_det_state_should_return_ok_if_gpio_is_low(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_CONN_DET];

	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_conn_det_state(panel), PANEL_STATE_OK);
	KUNIT_EXPECT_FALSE(test, panel_disconnected(panel));
}

static void panel_drv_test_panel_snprintf_bypass_return_zero_with_invalid_arg(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	char *buf;

	buf = kunit_kzalloc(test, 256, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass(NULL, buf, 256), 0);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass(panel, NULL, 256), 0);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass(panel, buf, 0), 0);
}

static void panel_drv_test_panel_snprintf_bypass_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	char *buf;
	char *str_bypass_on = "bypass:on";
	char *str_bypass_off = "bypass:off";

	buf = kunit_kzalloc(test, 256, GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_ON), 0);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass(panel, buf, 256), (int)strlen(str_bypass_on));
	KUNIT_EXPECT_STREQ(test, buf, str_bypass_on);

	KUNIT_ASSERT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_OFF), 0);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass(panel, buf, 256), (int)strlen(str_bypass_off));
	KUNIT_EXPECT_STREQ(test, buf, str_bypass_off);
}

static void panel_drv_test_panel_get_bypass_reason_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_get_bypass_reason(NULL), -EINVAL);
}

static void panel_drv_test_panel_snprintf_bypass_reason_return_zero_with_invalid_arg(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	char *buf;

	buf = kunit_kzalloc(test, 256, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass_reason(NULL, buf, 256), 0);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass_reason(panel, NULL, 256), 0);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass_reason(panel, buf, 0), 0);
}

static void panel_drv_test_check_bypass_reason_when_bypass_is_off(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	char *buf, *str_bypass_reason = "bypass:off(reason:none)";

	buf = kunit_kzalloc(test, 256, GFP_KERNEL);

	/* return BYPASS_REASON_NONE if bypass:off state */
	KUNIT_ASSERT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_OFF), 0);
	KUNIT_ASSERT_FALSE(test, panel_bypass_is_on(panel));

	/* test panel_get_bypass_reason() */
	KUNIT_EXPECT_EQ(test, panel_get_bypass_reason(panel), BYPASS_REASON_NONE);

	/* test panel_snprintf_bypass() */
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass_reason(panel, buf, 256), (int)strlen(str_bypass_reason));
	KUNIT_EXPECT_STREQ(test, buf, str_bypass_reason);
}

static void panel_drv_test_check_bypass_reason_when_display_connector_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	char *buf, *reason = "bypass:on(reason:display connector is disconnected)";
	struct panel_gpio *gp;

	gp = &ctx->gpio_list[PANEL_GPIO_CONN_DET];

	/* precondition bypass:on state */
	KUNIT_ASSERT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_ON), 0);
	KUNIT_ASSERT_TRUE(test, panel_bypass_is_on(panel));

	/* precondition conn-det 'HIGH'(NOK) */
	gp->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp))));
	expectation1->action = bool_return(test, true);

	gp->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp))));
	expectation2->action = int_return(test, 1);

	/* test panel_get_bypass_reason */
	KUNIT_EXPECT_EQ(test, panel_get_bypass_reason(panel), BYPASS_REASON_DISPLAY_CONNECTOR_IS_DISCONNECTED);

	/* test panel_snprintf_bypass_reason */
	buf = kunit_kzalloc(test, 256, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass_reason(panel, buf, 256), (int)strlen(reason));
	KUNIT_EXPECT_STREQ(test, buf, reason);
}

static void panel_drv_test_check_bypass_reason_when_disp_det_is_low_after_slpout(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct mock_expectation *expectation3, *expectation4;
	char *buf, *reason = "bypass:on(reason:AVDD_SWIRE is off after SLPOUT)";
	struct panel_gpio *gp1, *gp2;

	gp1 = &ctx->gpio_list[PANEL_GPIO_CONN_DET];
	gp2 = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	/* precondition bypass:on state */
	KUNIT_ASSERT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_ON), 0);
	KUNIT_ASSERT_TRUE(test, panel_bypass_is_on(panel));

	/* precondition conn-det 'LOW'(OK) */
	gp1->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp1))));
	expectation1->action = bool_return(test, true);

	gp1->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp1))));
	expectation2->action = int_return(test, 0);

	/* precondition disp-det 'LOW'(NOK) */
	gp2->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation3 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp2))));
	expectation3->action = bool_return(test, true);

	gp2->funcs->get_value = mock_panel_gpio_get_value;
	expectation4 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp2))));
	expectation4->action = int_return(test, 0);

	/* test panel_get_bypass_reason */
	KUNIT_EXPECT_EQ(test, panel_get_bypass_reason(panel), BYPASS_REASON_AVDD_SWIRE_IS_LOW_AFTER_SLPOUT);

	/* test panel_snprintf_bypass_reason */
	buf = kunit_kzalloc(test, 256, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass_reason(panel, buf, 256), (int)strlen(reason));
	KUNIT_EXPECT_STREQ(test, buf, reason);
}

static void panel_drv_test_check_bypass_reason_when_panel_id_read_failure(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct mock_expectation *expectation3, *expectation4;
	char *buf, *reason = "bypass:on(reason:panel id read failure)";
	struct panel_gpio *gp1, *gp2;

	gp1 = &ctx->gpio_list[PANEL_GPIO_CONN_DET];
	gp2 = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	/* precondition bypass:on state */
	KUNIT_ASSERT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_ON), 0);
	KUNIT_ASSERT_TRUE(test, panel_bypass_is_on(panel));

	/* precondition conn-det 'LOW'(OK) */
	gp1->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp1))));
	expectation1->action = bool_return(test, true);

	gp1->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp1))));
	expectation2->action = int_return(test, 0);

	/* precondition disp-det 'HIGH'(OK) */
	gp2->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation3 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp2))));
	expectation3->action = bool_return(test, true);

	gp2->funcs->get_value = mock_panel_gpio_get_value;
	expectation4 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp2))));
	expectation4->action = int_return(test, 1);

	/* precondition panel id is empty */
	memset(panel->panel_data.id, 0, sizeof(panel->panel_data.id));

	/* test panel_get_bypass_reason */
	KUNIT_EXPECT_EQ(test, panel_get_bypass_reason(panel), (int)BYPASS_REASON_PANEL_ID_READ_FAILURE);

	/* test panel_snprintf_bypass_reason */
	buf = kunit_kzalloc(test, 256, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass_reason(panel, buf, 256), (int)strlen(reason));
	KUNIT_EXPECT_STREQ(test, buf, reason);
}

static void panel_drv_test_check_bypass_reason_when_there_is_no_reason_to_bypass(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation1, *expectation2;
	struct mock_expectation *expectation3, *expectation4;
	char *buf, *reason = "bypass:on(reason:none)";
	struct panel_gpio *gp1, *gp2;

	gp1 = &ctx->gpio_list[PANEL_GPIO_CONN_DET];
	gp2 = &ctx->gpio_list[PANEL_GPIO_DISP_DET];

	/* precondition bypass:on state */
	KUNIT_ASSERT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_ON), 0);
	KUNIT_ASSERT_TRUE(test, panel_bypass_is_on(panel));

	/* precondition conn-det 'LOW'(OK) */
	gp1->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp1))));
	expectation1->action = bool_return(test, true);

	gp1->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp1))));
	expectation2->action = int_return(test, 0);

	/* precondition disp-det 'HIGH'(OK) */
	gp2->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation3 = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gp2))));
	expectation3->action = bool_return(test, true);

	gp2->funcs->get_value = mock_panel_gpio_get_value;
	expectation4 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gp2))));
	expectation4->action = int_return(test, 1);

	/* precondition panel id is not empty */
	panel->panel_data.id[0] = 0xF1;
	panel->panel_data.id[1] = 0x11;
	panel->panel_data.id[2] = 0x00;

	/* test panel_get_bypass_reason */
	KUNIT_EXPECT_EQ(test, panel_get_bypass_reason(panel), BYPASS_REASON_NONE);

	/* test panel_snprintf_bypass_reason */
	buf = kunit_kzalloc(test, 256, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, panel_snprintf_bypass_reason(panel, buf, 256), (int)strlen(reason));
	KUNIT_EXPECT_STREQ(test, buf, reason);
}

static void panel_drv_test_panel_bypass_is_on_should_return_false_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, panel_bypass_is_on(NULL));
}

static void panel_drv_test_panel_bypass_is_on_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel->state.bypass = PANEL_BYPASS_ON;
	KUNIT_EXPECT_TRUE(test, panel_bypass_is_on(panel));

	panel->state.bypass = PANEL_BYPASS_OFF;
	KUNIT_EXPECT_FALSE(test, panel_bypass_is_on(panel));
}

static void panel_drv_test_panel_set_bypass_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_EQ(test, panel_set_bypass(NULL, PANEL_BYPASS_ON), -EINVAL);
	KUNIT_EXPECT_FALSE(test, panel_bypass_is_on(panel));
}

static void panel_drv_test_panel_set_bypass_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_ON), 0);
	KUNIT_EXPECT_TRUE(test, panel_bypass_is_on(panel));

	KUNIT_EXPECT_EQ(test, panel_set_bypass(panel, PANEL_BYPASS_OFF), 0);
	KUNIT_EXPECT_FALSE(test, panel_bypass_is_on(panel));
}

static void panel_drv_test_panel_display_on_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_display_on(NULL), -EINVAL);
}

static void panel_drv_test_panel_display_on_should_return_not_error_if_panel_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_set_bypass(panel, PANEL_BYPASS_ON);
	KUNIT_EXPECT_EQ(test, panel_display_on(panel), 0);
}

static void panel_drv_test_panel_display_off_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_display_off(NULL), -EINVAL);
}

static void panel_drv_test_panel_display_off_should_return_not_error_if_panel_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_set_bypass(panel, PANEL_BYPASS_ON);
	KUNIT_EXPECT_EQ(test, panel_display_off(panel), 0);
}

static void panel_drv_test_panel_power_on_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_power_on(NULL), -EINVAL);
}

static void panel_drv_test_panel_power_on_should_fail_if_panel_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_set_bypass(panel, PANEL_BYPASS_ON);
	KUNIT_EXPECT_EQ(test, panel_power_on(panel), -ENODEV);
}

static void panel_drv_test_panel_power_off_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_power_off(NULL), -EINVAL);
}

static void panel_drv_test_panel_power_off_should_fail_if_panel_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_set_bypass(panel, PANEL_BYPASS_ON);
	KUNIT_EXPECT_EQ(test, panel_power_off(panel), -ENODEV);
}

static void panel_drv_test_panel_sleep_in_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_sleep_in(NULL), -EINVAL);
}

static void panel_drv_test_panel_sleep_in_should_return_not_error_if_panel_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_set_bypass(panel, PANEL_BYPASS_ON);
	KUNIT_EXPECT_EQ(test, panel_sleep_in(panel), 0);
}

static void panel_drv_test_panel_sleep_out_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_sleep_out(NULL), -EINVAL);
}

static void panel_drv_test_panel_sleep_out_should_fail_if_panel_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_set_bypass(panel, PANEL_BYPASS_ON);
	KUNIT_EXPECT_EQ(test, panel_sleep_out(panel), -ENODEV);
}

#ifdef CONFIG_USDM_PANEL_LPM
static void panel_drv_test_panel_doze_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_doze(NULL), -EINVAL);
}

static void panel_drv_test_panel_doze_should_return_not_error_if_panel_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_set_bypass(panel, PANEL_BYPASS_ON);
	KUNIT_EXPECT_EQ(test, panel_doze(panel), 0);
}
#endif

static void panel_drv_test_panel_register_isr_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_register_isr(NULL), -EINVAL);
}

static void panel_drv_test_panel_register_isr_fail_if_panel_device_is_null(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct device *orig_dev;

	orig_dev = panel->dev;
	panel->dev = NULL;

	KUNIT_EXPECT_EQ(test, panel_register_isr(panel), -ENODEV);

	panel->dev = orig_dev;
}

static void panel_drv_test_panel_register_isr_should_return_not_error_if_panel_is_disconnected(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_set_bypass(panel, PANEL_BYPASS_ON);
	KUNIT_EXPECT_EQ(test, panel_register_isr(panel), 0);
}

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static void panel_drv_test_panel_get_display_mode_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_EQ(test, call_panel_drv_func(panel, get_display_mode, NULL), -EINVAL);
}

static void panel_drv_test_panel_get_display_mode_fail_if_panel_modes_is_not_prepared(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_display_modes *pdms;

	panel->panel_modes = NULL;
	KUNIT_EXPECT_EQ(test, call_panel_drv_func(panel, get_display_mode, &pdms), -EINVAL);
}

static void panel_drv_test_panel_get_display_mode_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_display_modes *pdms;

	panel->panel_modes = kunit_kzalloc(test, sizeof(*pdms), GFP_KERNEL);
	KUNIT_EXPECT_TRUE(test, call_panel_drv_func(panel, get_display_mode, &pdms) == 0);
	KUNIT_EXPECT_PTR_EQ(test, panel->panel_modes, pdms);
}

static void panel_drv_test_panel_set_display_mode_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_EQ(test, call_panel_drv_func(panel, set_display_mode, NULL), -EINVAL);
}

static void panel_drv_test_panel_set_display_mode_fail_if_panel_modes_is_not_prepared(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_display_mode test_pdm_120hs = {
		PANEL_MODE(PANEL_DISPLAY_MODE_1080x2400_120HS, 1080, 2400,
				120, REFRESH_MODE_HS, 120, PANEL_REFRESH_MODE_HS, 0, 0, 0, 0,
				true, 2, 2, 120, 0, 0, 0, 0, 0, 0, 0, 3337, false, false)
	};

	panel->panel_modes = NULL;
	KUNIT_EXPECT_EQ(test, call_panel_drv_func(panel, set_display_mode, &test_pdm_120hs), -EINVAL);
}

static void panel_drv_test_panel_set_display_mode_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_properties *props =
		&panel->panel_data.props;
	struct panel_display_mode test_pdm_120hs = {
		PANEL_MODE(PANEL_DISPLAY_MODE_1080x2400_120HS, 1080, 2400,
				120, REFRESH_MODE_HS, 120, PANEL_REFRESH_MODE_HS, 0, 0, 0, 0,
				true, 2, 2, 120, 0, 0, 0, 0, 0, 0, 0, 3337, false, false)
	};
	struct panel_display_mode test_pdm_60hs = {
		PANEL_MODE(PANEL_DISPLAY_MODE_1080x2400_60HS, 1080, 2400,
				60, REFRESH_MODE_HS, 60, PANEL_REFRESH_MODE_HS, 0, 0, 0, 0,
				true, 2, 2, 120, 0, 0, 0, 0, 0, 0, 0, 10361, false, false)
	};
	struct panel_display_mode test_pdm_60ns = {
		PANEL_MODE(PANEL_DISPLAY_MODE_1080x2400_60NS, 1080, 2400,
				60, REFRESH_MODE_NS, 60, PANEL_REFRESH_MODE_NS, 0, 0, 0, 0,
				true, 2, 2, 120, 0, 0, 0, 0, 0, 0, 0, 10361, false, false)
	};
	struct panel_display_mode *test_modes[] = {
		&test_pdm_120hs,
		&test_pdm_60hs,
		&test_pdm_60ns,
	};
	struct panel_display_modes test_pdms = {
		.num_modes = ARRAY_SIZE(test_modes),
		.native_mode = 0,
		.modes = test_modes,
	};

	struct panel_vrr panel_vrr_120hs = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	};
	struct panel_vrr panel_vrr_60hs = {
		.fps = 60,
		.base_fps = 120,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	};
	struct panel_vrr panel_vrr_60ns = {
		.fps = 60,
		.base_fps = 60,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	};
	struct panel_vrr *panel_vrrtbl[] = {
		&panel_vrr_120hs,
		&panel_vrr_60hs,
		&panel_vrr_60ns,
	};
	struct panel_resol panel_fhd_resolution = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 120,
			},
		},
		.available_vrr = panel_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(panel_vrrtbl),
	};
	struct common_panel_display_mode cpdm_fhd_120hs = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS,
		.resol = &panel_fhd_resolution,
		.vrr = &panel_vrr_120hs,
	};
	struct common_panel_display_mode cpdm_fhd_60hs = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS,
		.resol = &panel_fhd_resolution,
		.vrr = &panel_vrr_60hs,
	};
	struct common_panel_display_mode cpdm_fhd_60ns = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60NS,
		.resol = &panel_fhd_resolution,
		.vrr = &panel_vrr_60ns,
	};
	struct common_panel_display_mode *cpdm_array[] = {
		&cpdm_fhd_120hs,
		&cpdm_fhd_60hs,
		&cpdm_fhd_60ns,
	};
	struct common_panel_display_modes cpdms = {
		.num_modes = ARRAY_SIZE(cpdm_array),
		.modes = (struct common_panel_display_mode **)&cpdm_array,
	};
	void *test_display_mode_cmdtbl[] = {
		NULL,
	};
	struct seqinfo test_seqtbl[] = {
		SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, test_display_mode_cmdtbl),
	};
	struct pnobj *pnobj;
	int i;

	panel->panel_modes = &test_pdms;
	props->panel_mode = 0;
	panel->panel_data.common_panel_modes = &cpdms;
	for (i = 0; i < ARRAY_SIZE(test_seqtbl); i++) {
		pnobj = &test_seqtbl[i].base;
		if (get_pnobj_cmd_type(pnobj) != CMD_TYPE_SEQ)
			continue;
		list_add_tail(&pnobj->list, &panel->seq_list);
	}
	panel->panel_data.mres.resol = &panel_fhd_resolution;
	panel->panel_data.mres.nr_resol = 1;
	panel->panel_data.vrrtbl = panel_vrrtbl;
	panel->panel_data.nr_vrrtbl = ARRAY_SIZE(panel_vrrtbl);

	/* skip if same panel_mode */
	KUNIT_EXPECT_TRUE(test, call_panel_drv_func(panel, set_display_mode, &test_pdm_120hs) == 0);
	KUNIT_EXPECT_EQ(test, props->panel_mode, (u32)0);

	/* change panel_mode successfully */
	KUNIT_EXPECT_TRUE(test, call_panel_drv_func(panel, set_display_mode, &test_pdm_60ns) == 0);
	KUNIT_EXPECT_EQ(test, props->panel_mode, (u32)2);
}
#endif

static void panel_drv_test_panel_initialize_regulator_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_initialize_regulator(NULL), -EINVAL);
}

static void panel_drv_test_panel_initialize_regulator_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_regulator *regulator;
	struct mock_expectation *expectation;
	int cnt = 0;

	list_for_each_entry(regulator, &panel->regulator_list, head) {
		regulator->funcs->init = mock_panel_regulator_init;
		cnt++;
	}

	expectation = Times(cnt, KUNIT_EXPECT_CALL(mock_panel_regulator_init(any(test))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_initialize_regulator(panel), 0);
}

static void panel_drv_test_panel_duplicate_sequence_with_invalid_pnobj(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct pnobj *empty_pnobj =
		kunit_kzalloc(test, sizeof(*empty_pnobj), GFP_KERNEL);
	void *test_display_mode_cmdtbl[] = { empty_pnobj, };
	struct seqinfo test_seq =
		SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, test_display_mode_cmdtbl);
	struct pnobj *pos, *next;
	int ret;

	ret = panel_duplicate_sequence(panel, &test_seq);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, !list_empty(&panel->seq_list));

	list_for_each_entry_safe(pos, next, &panel->seq_list, list)
		destroy_sequence(pnobj_container_of(pos, struct seqinfo));
}

static void panel_drv_test_panel_duplicate_sequence_with_empty_sequence(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct pnobj *empty_pnobj =
		kunit_kzalloc(test, sizeof(*empty_pnobj), GFP_KERNEL);
	void *test_display_mode_cmdtbl[] = { };
	struct seqinfo test_seq =
		SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, test_display_mode_cmdtbl);
	int ret;

	ret = panel_duplicate_sequence(panel, &test_seq);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->seq_list));
}

static void panel_drv_test_panel_duplicate_sequence_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 TEST_PANEL_DISPLAY_ON[] = { 0x29 };
	DEFINE_STATIC_PACKET(test_panel_display_on, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_ON, 0);
	void *test_panel_display_on_cmdtbl[] = {
		&PKTINFO(test_panel_display_on),
	};
	struct seqinfo test_seq =
		SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, test_panel_display_on_cmdtbl);
	struct pnobj *pos, *next;
	int ret;

	ret = panel_duplicate_packet(panel, &PKTINFO(test_panel_display_on));
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, !list_empty(&panel->pkt_list));

	ret = panel_duplicate_sequence(panel, &test_seq);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, !list_empty(&panel->seq_list));

	list_for_each_entry_safe(pos, next, &panel->pkt_list, list)
		destroy_tx_packet(pnobj_container_of(pos, struct pktinfo));

	list_for_each_entry_safe(pos, next, &panel->seq_list, list)
		destroy_sequence(pnobj_container_of(pos, struct seqinfo));
}

static void panel_drv_test_pnobj_find_by_pnobj_with_invalid_argument(struct kunit *test)
{
	struct pnobj *temp_pnobj =
		kunit_kzalloc(test, sizeof(*temp_pnobj), GFP_KERNEL);
	struct pnobj *temp_pnobj1 =
		kunit_kzalloc(test, sizeof(*temp_pnobj), GFP_KERNEL);
	LIST_HEAD(command_list);

	/* empty list */
	KUNIT_EXPECT_TRUE(test, !pnobj_find_by_pnobj(&command_list, temp_pnobj));

	pnobj_init(temp_pnobj1, CMD_TYPE_TX_PACKET, NULL);
	pnobj_init(temp_pnobj, CMD_TYPE_TX_PACKET, NULL);
	list_add_tail(&temp_pnobj1->list, &command_list);
	KUNIT_EXPECT_TRUE(test, !pnobj_find_by_pnobj(&command_list, temp_pnobj));
}

static void panel_drv_test_panel_setup_command_initdata_list_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct common_panel_info *info = &test_panel_info;
	struct pnobj *temp_pnobj = kzalloc(sizeof(*temp_pnobj), GFP_KERNEL);
	struct pnobj *pnobj;

	KUNIT_ASSERT_TRUE(test, panel_setup_command_initdata_list(panel, info) == 0);

	pnobj_init(temp_pnobj, CMD_TYPE_TX_PACKET, "test_panel_level1_key_enable");
	pnobj = pnobj_find_by_pnobj(&panel->command_initdata_list, temp_pnobj);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pnobj);

	pnobj_init(temp_pnobj, CMD_TYPE_KEY, "test_panel_level1_key_enable");
	pnobj = pnobj_find_by_pnobj(&panel->command_initdata_list, temp_pnobj);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pnobj);

	pnobj_init(temp_pnobj, CMD_TYPE_TX_PACKET, "test_panel_acl_opr");
	pnobj = pnobj_find_by_pnobj(&panel->command_initdata_list, temp_pnobj);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pnobj);

	pnobj_init(temp_pnobj, CMD_TYPE_MAP, "test_panel_acl_opr_table");
	pnobj = pnobj_find_by_pnobj(&panel->command_initdata_list, temp_pnobj);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pnobj);

	pnobj = pnobj_find_by_pnobj(&panel->command_initdata_list,
			&test_panel_info.rditbl[READ_ID].base);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pnobj);

	pnobj = pnobj_find_by_pnobj(&panel->command_initdata_list,
			&test_panel_info.restbl[RES_DATE].base);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pnobj);

	pnobj = pnobj_find_by_pnobj(&panel->command_initdata_list,
			&test_panel_info.dumpinfo[DUMP_RDDPM].base);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pnobj);

	pnobj_init(temp_pnobj, CMD_TYPE_SEQ, PANEL_INIT_SEQ);
	pnobj = pnobj_find_by_pnobj(&panel->command_initdata_list, temp_pnobj);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pnobj);
}

static void panel_drv_test_panel_prepare_prop_list_test(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct common_panel_info *cpi =
		kunit_kzalloc(test, sizeof(*cpi), GFP_KERNEL);
	struct panel_prop_list oled_property_array[] = {
		__PANEL_PROPERTY_RANGE_INITIALIZER("oled_property", 0, 0, 1000000000),
	};
	struct panel_prop_list ddi_property_array[] = {
		__PANEL_PROPERTY_RANGE_INITIALIZER("ddi_property", 0, 0, 1000000000),
	};
	struct panel_prop_list model_property_array[] = {
		__PANEL_PROPERTY_RANGE_INITIALIZER("model_property", 0, 0, 1000000000),
	};
	struct common_panel_info *cpi_backup;

	cpi->prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_COMMON] = ARRAY_SIZE(oled_property_array);
	cpi->prop_lists[USDM_DRV_LEVEL_DDI] = ddi_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_DDI] = ARRAY_SIZE(ddi_property_array);
	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = model_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(model_property_array);

	cpi_backup = panel->cpi;
	panel->cpi = cpi;
	KUNIT_ASSERT_TRUE(test, !list_empty(&panel->prop_list));
	KUNIT_EXPECT_EQ(test, panel_prepare_prop_list(panel, cpi), 0);
	KUNIT_EXPECT_EQ(test, panel_unprepare_prop_list(panel), 0);
	KUNIT_ASSERT_TRUE(test, !list_empty(&panel->prop_list));
	panel->cpi = cpi_backup;
}

static void panel_drv_test_panel_prepare_and_unprepare_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct common_panel_info *info = &test_panel_info;

	info->vendor = NULL;
	KUNIT_EXPECT_TRUE(test, panel_prepare(panel, info) == 0);
	KUNIT_EXPECT_STREQ(test, (char *)panel->panel_data.vendor, "");
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->maptbl_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->seq_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->rdi_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->res_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->dump_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->key_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->pkt_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->dly_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->cond_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->pwrctrl_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->prop_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->cfg_list));
#if defined(CONFIG_USDM_PANEL_JSON)
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->func_list));
#endif

	KUNIT_EXPECT_TRUE(test, panel_unprepare(panel) == 0);
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->maptbl_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->seq_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->rdi_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->res_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->dump_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->key_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->pkt_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->dly_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->cond_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->pwrctrl_list));
	KUNIT_EXPECT_FALSE(test, list_empty(&panel->prop_list));
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->cfg_list));
#if defined(CONFIG_USDM_PANEL_JSON)
	KUNIT_EXPECT_TRUE(test, list_empty(&panel->func_list));
#endif
}

static void panel_drv_test_madatory_panel_property_set_and_get_test(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct prop_test_param prop_test_param[] = {
		/* panel_state */
		{ .propname = PANEL_PROPERTY_PANEL_STATE, .param = PANEL_STATE_OFF, .expected_param = PANEL_STATE_OFF, },
		{ .propname = PANEL_PROPERTY_PANEL_STATE, .param = PANEL_STATE_ON, .expected_param = PANEL_STATE_ON, },
		{ .propname = PANEL_PROPERTY_PANEL_STATE, .param = PANEL_STATE_NORMAL, .expected_param = PANEL_STATE_NORMAL, },
		{ .propname = PANEL_PROPERTY_PANEL_STATE, .param = PANEL_STATE_ALPM, .expected_param = PANEL_STATE_ALPM, },
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
		/* brightdot_test_enable */
		{ .propname = PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE, .param = 0, .expected_param = 0, },
		{ .propname = PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE, .param = 1, .expected_param = 1, },
#endif
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(prop_test_param); i++) {
		panel_set_property_value(panel,
				prop_test_param[i].propname,
				prop_test_param[i].param);
		KUNIT_EXPECT_EQ(test,
				panel_get_property_value(panel,
					prop_test_param[i].propname),
				(int)prop_test_param[i].expected_param);
	}
}

static void panel_drv_test_panel_set_property_success(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_drv_prop_test_param panel_drv_prop_test_success_params[] = {
		/* PANEL_PROPERTY_PANEL_STATE */
		{ PANEL_PROPERTY_PANEL_STATE, &panel->state.cur_state, PANEL_STATE_OFF },
		{ PANEL_PROPERTY_PANEL_STATE, &panel->state.cur_state, PANEL_STATE_ON },
		{ PANEL_PROPERTY_PANEL_STATE, &panel->state.cur_state, PANEL_STATE_NORMAL },
		{ PANEL_PROPERTY_PANEL_STATE, &panel->state.cur_state, PANEL_STATE_ALPM },
		/* PANEL_PROPERTY_PANEL_ID_1 */
		{ PANEL_PROPERTY_PANEL_ID_1, &panel->panel_data.id[0], 0x00 },
		{ PANEL_PROPERTY_PANEL_ID_1, &panel->panel_data.id[0], 0xFF },
		/* PANEL_PROPERTY_PANEL_ID_2 */
		{ PANEL_PROPERTY_PANEL_ID_2, &panel->panel_data.id[1], 0x00 },
		{ PANEL_PROPERTY_PANEL_ID_2, &panel->panel_data.id[1], 0xFF },
		/* PANEL_PROPERTY_PANEL_ID_3 */
		{ PANEL_PROPERTY_PANEL_ID_3, &panel->panel_data.id[2], 0x00 },
		{ PANEL_PROPERTY_PANEL_ID_3, &panel->panel_data.id[2], 0xFF },
		/* PANEL_PROPERTY_PANEL_REFRESH_RATE */
		{ PANEL_PROPERTY_PANEL_REFRESH_RATE, &panel->panel_data.props.vrr_fps, 0 },
		{ PANEL_PROPERTY_PANEL_REFRESH_RATE, &panel->panel_data.props.vrr_fps, 120 },
		/* PANEL_PROPERTY_PANEL_REFRESH_MODE */
		{ PANEL_PROPERTY_PANEL_REFRESH_MODE, &panel->panel_data.props.vrr_mode, VRR_NORMAL_MODE },
		{ PANEL_PROPERTY_PANEL_REFRESH_MODE, &panel->panel_data.props.vrr_mode, VRR_HS_MODE },
		/* PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE */
		{ PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE, &panel->panel_data.props.vrr_origin_fps, 0 },
		{ PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE, &panel->panel_data.props.vrr_origin_fps, 120 },
		/* PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE */
		{ PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE, &panel->panel_data.props.vrr_origin_mode, VRR_NORMAL_MODE },
		{ PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE, &panel->panel_data.props.vrr_origin_mode, VRR_HS_MODE },
		/* PANEL_PROPERTY_DSI_FREQ */
		{ PANEL_PROPERTY_DSI_FREQ, &panel->panel_data.props.dsi_freq, 0 },
		{ PANEL_PROPERTY_DSI_FREQ, &panel->panel_data.props.dsi_freq, 1000000 },
		/* PANEL_PROPERTY_OSC_FREQ */
		{ PANEL_PROPERTY_OSC_FREQ, &panel->panel_data.props.osc_freq, 0 },
		{ PANEL_PROPERTY_OSC_FREQ, &panel->panel_data.props.osc_freq, 1000000 },
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
		/* PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE */
		{ PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE, &panel->panel_data.props.brightdot_test_enable, 0 },
		{ PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE, &panel->panel_data.props.brightdot_test_enable, 1 },
#endif
#ifdef CONFIG_USDM_FACTORY_VGLHIGHDOT_TEST
		/* PANEL_PROPERTY_VGLHIGHDOT */
		{ PANEL_PROPERTY_VGLHIGHDOT, &panel->panel_data.props.vglhighdot, 0 },
		{ PANEL_PROPERTY_VGLHIGHDOT, &panel->panel_data.props.vglhighdot, 1 },
		{ PANEL_PROPERTY_VGLHIGHDOT, &panel->panel_data.props.vglhighdot, 2 },
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
		/* PANEL_PROPERTY_XTALK_MODE */
		{ PANEL_PROPERTY_XTALK_MODE, &panel->panel_data.props.xtalk_mode, XTALK_OFF },
		{ PANEL_PROPERTY_XTALK_MODE, &panel->panel_data.props.xtalk_mode, XTALK_ON },
#endif
		/* PANEL_PROPERTY_IRC_MODE */
		{ PANEL_PROPERTY_IRC_MODE, &panel->panel_data.props.irc_mode, IRC_MODE_MODERATO },
		{ PANEL_PROPERTY_IRC_MODE, &panel->panel_data.props.irc_mode, IRC_MODE_FLAT_GAMMA },
		/* PANEL_PROPERTY_DIA_MODE */
		{ PANEL_PROPERTY_DIA_MODE, &panel->panel_data.props.dia_mode, 0 },
		{ PANEL_PROPERTY_DIA_MODE, &panel->panel_data.props.dia_mode, 1 },
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
		/* PANEL_PROPERTY_GCT_VDDM */
		{ PANEL_PROPERTY_GCT_VDDM, &panel->panel_data.props.gct_vddm, VDDM_ORIG },
		{ PANEL_PROPERTY_GCT_VDDM, &panel->panel_data.props.gct_vddm, VDDM_LV },
		{ PANEL_PROPERTY_GCT_VDDM, &panel->panel_data.props.gct_vddm, VDDM_HV },
		/* PANEL_PROPERTY_GCT_PATTERN */
		{ PANEL_PROPERTY_GCT_PATTERN, &panel->panel_data.props.gct_pattern, GCT_PATTERN_NONE },
		{ PANEL_PROPERTY_GCT_PATTERN, &panel->panel_data.props.gct_pattern, GCT_PATTERN_1 },
		{ PANEL_PROPERTY_GCT_PATTERN, &panel->panel_data.props.gct_pattern, GCT_PATTERN_2 },
#endif
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
		/* PANEL_PROPERTY_FD_ENABLED */
		{ PANEL_PROPERTY_FD_ENABLED, &panel->panel_data.props.enable_fd, 0 },
		{ PANEL_PROPERTY_FD_ENABLED, &panel->panel_data.props.enable_fd, 1 },
#endif
	};
	int i;
	char msg[SZ_256];

	for (i = 0; i < ARRAY_SIZE(panel_drv_prop_test_success_params); i++) {
		KUNIT_EXPECT_EQ(test, panel_set_property(panel,
				panel_drv_prop_test_success_params[i].prop_ptr,
				panel_drv_prop_test_success_params[i].value), 0);

		snprintf(msg, SZ_256, "panel_get_property_value(panel, "
			"panel_drv_prop_test_success_params[%d].prop_name) != "
			"panel_drv_prop_test_success_params[%d].value)", i, i);
		KUNIT_EXPECT_EQ_MSG(test, panel_get_property_value(panel,
				panel_drv_prop_test_success_params[i].prop_name),
				panel_drv_prop_test_success_params[i].value, msg);
	}
}

static void panel_drv_test_panel_set_property_failure(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_drv_prop_test_param panel_drv_prop_test_failure_params[] = {
		/* PANEL_PROPERTY_PANEL_STATE */
		{ PANEL_PROPERTY_PANEL_STATE, &panel->state.cur_state, MAX_PANEL_STATE },
		/* PANEL_PROPERTY_PANEL_ID_1 */
		{ PANEL_PROPERTY_PANEL_ID_1, &panel->panel_data.id[0], 0x100 },
		/* PANEL_PROPERTY_PANEL_ID_2 */
		{ PANEL_PROPERTY_PANEL_ID_2, &panel->panel_data.id[1], 0x100 },
		/* PANEL_PROPERTY_PANEL_ID_3 */
		{ PANEL_PROPERTY_PANEL_ID_3, &panel->panel_data.id[2], 0x100 },
		/* PANEL_PROPERTY_PANEL_REFRESH_RATE */
		{ PANEL_PROPERTY_PANEL_REFRESH_RATE, &panel->panel_data.props.vrr_fps, 121 },
		/* PANEL_PROPERTY_PANEL_REFRESH_MODE */
		{ PANEL_PROPERTY_PANEL_REFRESH_MODE, &panel->panel_data.props.vrr_mode, MAX_VRR_MODE },
		/* PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE */
		{ PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE, &panel->panel_data.props.vrr_origin_fps, 121 },
		/* PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE */
		{ PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE, &panel->panel_data.props.vrr_origin_mode, MAX_VRR_MODE },
		/* PANEL_PROPERTY_DSI_FREQ */
		{ PANEL_PROPERTY_DSI_FREQ, &panel->panel_data.props.dsi_freq, 1000000001 },
		/* PANEL_PROPERTY_OSC_FREQ */
		{ PANEL_PROPERTY_OSC_FREQ, &panel->panel_data.props.osc_freq, 1000000001 },
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
		/* PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE */
		{ PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE, &panel->panel_data.props.brightdot_test_enable, 2 },
#endif
#ifdef CONFIG_USDM_FACTORY_VGLHIGHDOT_TEST
		/* PANEL_PROPERTY_VGLHIGHDOT_TEST_ENABLE */
		{ PANEL_PROPERTY_VGLHIGHDOT, &panel->panel_data.props.vglhighdot, 3 },
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
		/* PANEL_PROPERTY_XTALK_MODE */
		{ PANEL_PROPERTY_XTALK_MODE, &panel->panel_data.props.xtalk_mode, 2 },
#endif
		/* PANEL_PROPERTY_IRC_MODE */
		{ PANEL_PROPERTY_IRC_MODE, &panel->panel_data.props.irc_mode, 2 },
		/* PANEL_PROPERTY_DIA_MODE */
		{ PANEL_PROPERTY_DIA_MODE, &panel->panel_data.props.irc_mode, 2 },
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
		/* PANEL_PROPERTY_GCT_VDDM */
		{ PANEL_PROPERTY_GCT_VDDM, &panel->panel_data.props.gct_vddm, VDDM_HV + 1},
		/* PANEL_PROPERTY_GCT_PATTERN */
		{ PANEL_PROPERTY_GCT_PATTERN, &panel->panel_data.props.gct_pattern, GCT_PATTERN_2 + 1 },
#endif
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
		/* PANEL_PROPERTY_FD_ENABLED */
		{ PANEL_PROPERTY_FD_ENABLED, &panel->panel_data.props.enable_fd, 2 },
#endif
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(panel_drv_prop_test_failure_params); i++) {
		KUNIT_EXPECT_EQ(test, panel_set_property(panel,
				panel_drv_prop_test_failure_params[i].prop_ptr,
				panel_drv_prop_test_failure_params[i].value), -EINVAL);
	}
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_drv_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_drv_test *ctx = kunit_kzalloc(test,
		sizeof(struct panel_drv_test), GFP_KERNEL);

	static struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};

	static struct panel_prop_enum_item wait_tx_done_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_AUTO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_ON),
	};

	static struct panel_prop_enum_item separate_tx_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_ON),
	};

	static struct panel_prop_enum_item vrr_mode_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_NORMAL_MODE),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_HS_MODE),
	};

#ifdef CONFIG_SUPPORT_XTALK_MODE
	static struct panel_prop_enum_item xtalk_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(XTALK_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(XTALK_ON),
	};
#endif

	static struct panel_prop_enum_item irc_mode_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(IRC_MODE_MODERATO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(IRC_MODE_FLAT_GAMMA),
	};

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	static struct panel_prop_enum_item gct_vddm_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VDDM_ORIG),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VDDM_LV),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VDDM_HV),
	};

	static struct panel_prop_enum_item gct_pattern_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GCT_PATTERN_NONE),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GCT_PATTERN_1),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GCT_PATTERN_2),
	};
#endif

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
#ifdef CONFIG_SUPPORT_XTALK_MODE
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_XTALK_MODE,
				XTALK_OFF, xtalk_enum_items),
#endif
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_IRC_MODE,
				IRC_MODE_MODERATO, irc_mode_enum_items),
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_GCT_VDDM,
				VDDM_ORIG, gct_vddm_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_GCT_PATTERN,
				GCT_PATTERN_NONE, gct_pattern_enum_items),
#endif
		/* range property */
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_NUMBER_0,
				0, 0, 0),
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
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_LFD_FIX,
				0, 0, 4),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_HMD_LFD_FIX,
				0, 0, 4),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_LPM_LFD_FIX,
				0, 0, 4),
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
#ifdef CONFIG_USDM_FACTORY_VGLHIGHDOT_TEST
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_VGLHIGHDOT,
				0, 0, 2),
#endif
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_DIA_MODE,
				1, 0, 1),
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_FD_ENABLED,
				0, 0, 1),
#endif
	};

	INIT_LIST_HEAD(&panel_device_list);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel->of_node_name = "test_panel_name";
	panel->dev = device_create(lcd_class,
			NULL, 0, panel, PANEL_DRV_NAME);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->dev);

#ifdef CONFIG_USDM_PANEL_COPR
	panel_mutex_init(panel, &panel->copr.lock);
#endif
	panel_mutex_init(panel, &panel->io_lock);
	panel_mutex_init(panel, &panel->op_lock);
	panel_mutex_init(panel, &panel->cmdq.lock);
#ifdef CONFIG_USDM_MDNIE
	panel_mutex_init(panel, &panel->mdnie.lock);
#endif

	panel->cmdbuf = kunit_kzalloc(test, SZ_512, GFP_KERNEL);
	panel->adapter.fifo_size = SZ_512;

	panel->funcs = &panel_drv_funcs;
	ctx->panel = panel;
	test->priv = ctx;

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

	KUNIT_ASSERT_TRUE(test,
			!panel_add_property_from_array(panel,
			prop_array, ARRAY_SIZE(prop_array)));

	panel_drv_test_panel_gpio_init(test);
	panel_drv_test_panel_regulator_init(test);

	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(panel_cmdq_flush);

	list_add_tail(&panel->list, &panel_device_list);

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_drv_test_exit(struct kunit *test)
{
	struct panel_drv_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	int i;

	panel_delete_property_all(panel);
	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));

	for (i = 0; i < panel_cmdq_get_size(panel); i++) {
		kfree(panel->cmdq.cmd[i].buf);
		panel->cmdq.cmd[i].buf = NULL;
	}

	device_unregister(panel->dev);
	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_drv_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_drv_test_test),

	KUNIT_CASE(panel_drv_test_get_lcd_info_before_panel_drv_init),
	KUNIT_CASE(panel_drv_test_get_lcd_info_after_panel_drv_init),

	KUNIT_CASE(panel_drv_test_panel_is_gpio_valid_return_false_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_is_gpio_valid_return_false_with_invalid_gpio),
	KUNIT_CASE(panel_drv_test_panel_is_gpio_valid_return_true_with_valid_gpio),

	KUNIT_CASE(panel_drv_test_panel_set_panel_id_test),
	KUNIT_CASE(panel_drv_test_panel_get_panel_id_success),

	KUNIT_CASE(panel_drv_test_panel_get_gpio_return_null_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_get_gpio_return_null_with_invalid_gpio),
	KUNIT_CASE(panel_drv_test_panel_get_gpio_return_panel_gpio_with_valid_gpio),

	KUNIT_CASE(panel_drv_test_panel_set_gpio_value_fail_with_invalid_arg_or_invalid_gpio),
	KUNIT_CASE(panel_drv_test_panel_set_gpio_value_success_with_valid_gpio),

	KUNIT_CASE(panel_drv_test_panel_get_gpio_value_fail_with_invalid_arg_or_invalid_gpio),
	KUNIT_CASE(panel_drv_test_panel_get_gpio_value_success_with_valid_gpio),

	KUNIT_CASE(panel_drv_test_panel_get_gpio_state_fail_with_invalid_arg_or_invalid_gpio),
	KUNIT_CASE(panel_drv_test_panel_get_gpio_state_success),

	KUNIT_CASE(panel_drv_test_panel_get_gpio_name_return_null_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_get_gpio_name_return_null_with_invalid_gpio),
	KUNIT_CASE(panel_drv_test_panel_get_gpio_name_return_gpio_name_with_valid_gpio),

	KUNIT_CASE(panel_drv_test_panel_gpio_irq_wrapper_functions_fail_with_invalid_gpio),
	KUNIT_CASE(panel_drv_test_panel_gpio_irq_wrapper_functions_fail_with_invalid_gpio_irq),
	KUNIT_CASE(panel_drv_test_panel_gpio_irq_wrapper_functions_success_with_valid_gpio_irq),

	KUNIT_CASE(panel_drv_test_panel_poll_gpio_should_return_error_if_gpio_is_not_exist),
	KUNIT_CASE(panel_drv_test_panel_poll_gpio_should_return_timeout_error_if_different_with_expected_level),
	KUNIT_CASE(panel_drv_test_panel_poll_gpio_should_return_zero_if_success),

	KUNIT_CASE(panel_drv_test_panel_disp_det_state_should_return_nok_if_avdd_swire_is_low),
	KUNIT_CASE(panel_drv_test_panel_disp_det_state_should_return_ok_if_avdd_swire_is_high),

	KUNIT_CASE(panel_drv_test_panel_irq_enable_functions_success_with_valid_gpio_irq),
	KUNIT_CASE(panel_drv_test_panel_irq_enable_functions_success_with_valid_multi_gpio_irq),
	KUNIT_CASE(panel_drv_test_panel_irq_disable_functions_success_with_valid_gpio_irq),
	KUNIT_CASE(panel_drv_test_panel_irq_disable_functions_success_with_valid_multi_gpio_irq),
	KUNIT_CASE(panel_drv_test_panel_irq_poll_functions_success_with_valid_gpio_value),

#if IS_ENABLED(CONFIG_USDM_PANEL_ERRFG_RECOVERY)
	KUNIT_CASE(panel_drv_test_panel_err_fg_state_should_return_nok_if_gpio_is_high),
	KUNIT_CASE(panel_drv_test_panel_err_fg_state_should_return_ok_if_gpio_is_low),
#endif

	KUNIT_CASE(panel_drv_test_panel_conn_det_state_should_return_nok_if_gpio_is_high),
	KUNIT_CASE(panel_drv_test_panel_conn_det_state_should_return_ok_if_gpio_is_low),

	KUNIT_CASE(panel_drv_test_panel_snprintf_bypass_return_zero_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_snprintf_bypass_success),

	KUNIT_CASE(panel_drv_test_panel_get_bypass_reason_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_snprintf_bypass_reason_return_zero_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_check_bypass_reason_when_bypass_is_off),
	KUNIT_CASE(panel_drv_test_check_bypass_reason_when_display_connector_is_disconnected),
	KUNIT_CASE(panel_drv_test_check_bypass_reason_when_disp_det_is_low_after_slpout),
	KUNIT_CASE(panel_drv_test_check_bypass_reason_when_panel_id_read_failure),
	KUNIT_CASE(panel_drv_test_check_bypass_reason_when_there_is_no_reason_to_bypass),

	KUNIT_CASE(panel_drv_test_panel_bypass_is_on_should_return_false_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_bypass_is_on_success),

	KUNIT_CASE(panel_drv_test_panel_set_bypass_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_set_bypass_success),

	KUNIT_CASE(panel_drv_test_panel_display_on_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_display_on_should_return_not_error_if_panel_is_disconnected),

	KUNIT_CASE(panel_drv_test_panel_display_off_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_display_off_should_return_not_error_if_panel_is_disconnected),

	KUNIT_CASE(panel_drv_test_panel_power_on_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_power_on_should_fail_if_panel_is_disconnected),

	KUNIT_CASE(panel_drv_test_panel_power_off_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_power_off_should_fail_if_panel_is_disconnected),

	KUNIT_CASE(panel_drv_test_panel_sleep_in_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_sleep_in_should_return_not_error_if_panel_is_disconnected),

	KUNIT_CASE(panel_drv_test_panel_sleep_out_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_sleep_out_should_fail_if_panel_is_disconnected),

#ifdef CONFIG_USDM_PANEL_LPM
	KUNIT_CASE(panel_drv_test_panel_doze_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_doze_should_return_not_error_if_panel_is_disconnected),
#endif
	KUNIT_CASE(panel_drv_test_panel_register_isr_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_register_isr_fail_if_panel_device_is_null),
	KUNIT_CASE(panel_drv_test_panel_register_isr_should_return_not_error_if_panel_is_disconnected),

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	/* panel_get_display_mode */
	KUNIT_CASE(panel_drv_test_panel_get_display_mode_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_get_display_mode_fail_if_panel_modes_is_not_prepared),
	KUNIT_CASE(panel_drv_test_panel_get_display_mode_success),

	/* panel_set_display_mode */
	KUNIT_CASE(panel_drv_test_panel_set_display_mode_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_set_display_mode_fail_if_panel_modes_is_not_prepared),
	KUNIT_CASE(panel_drv_test_panel_set_display_mode_success),
#endif
	KUNIT_CASE(panel_drv_test_panel_initialize_regulator_fail_with_invalid_arg),
	KUNIT_CASE(panel_drv_test_panel_initialize_regulator_success),
	KUNIT_CASE(panel_drv_test_panel_duplicate_sequence_with_invalid_pnobj),
	KUNIT_CASE(panel_drv_test_panel_duplicate_sequence_with_empty_sequence),
	KUNIT_CASE(panel_drv_test_panel_duplicate_sequence_success),
	KUNIT_CASE(panel_drv_test_pnobj_find_by_pnobj_with_invalid_argument),
	KUNIT_CASE(panel_drv_test_panel_setup_command_initdata_list_success),
	KUNIT_CASE(panel_drv_test_panel_prepare_prop_list_test),
	KUNIT_CASE(panel_drv_test_panel_prepare_and_unprepare_success),

	KUNIT_CASE(panel_drv_test_madatory_panel_property_set_and_get_test),
	KUNIT_CASE(panel_drv_test_panel_set_property_success),
	KUNIT_CASE(panel_drv_test_panel_set_property_failure),
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
static struct kunit_suite panel_drv_test_module = {
	.name = "panel_drv_test",
	.init = panel_drv_test_init,
	.exit = panel_drv_test_exit,
	.test_cases = panel_drv_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_drv_test_module);

MODULE_LICENSE("GPL v2");
