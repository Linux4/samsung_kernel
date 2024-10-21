// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/time.h>
#include <media/v4l2-subdev.h>
#include "panel_drv.h"
#include "panel_drv_ioctl.h"
#include "mock_panel_drv_funcs.h"

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

extern struct panel_drv_funcs panel_drv_funcs;

/* NOTE: UML TC */
static void panel_drv_ioctl_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_drv_test_panel_drv_ioctl_fail_with_invalid_ioctl_command(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	int ret;

	/* ioctl command is zero */
	ret = panel_ioctl(panel, 0, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* ioctl command type is not same with PANEL_IOC_BASE */
	ret = panel_ioctl(panel, _IOR('C', 41, int *), NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* ioctl command number is out of range */
#if defined(CONFIG_USDM_PANEL_MASK_LAYER)
	ret = panel_ioctl(panel, _IO(PANEL_IOC_BASE, _IOC_NR(PANEL_IOC_SET_MASK_LAYER) + 1), NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
#else
	ret = panel_ioctl(panel, _IO(PANEL_IOC_BASE, _IOC_NR(PANEL_IOC_REG_VRR_CB) + 1), NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
#endif
}

static void panel_drv_ioctl_test_panel_drv_ioctl_scnprintf_cmd_name_success(struct kunit *test)
{
	size_t size = 128;
	char *buf = kunit_kmalloc(test, size, GFP_KERNEL);

	memset(buf, 0, size);
	KUNIT_EXPECT_EQ(test, panel_drv_ioctl_scnprintf_cmd_name(buf, size, PANEL_IOC_ATTACH_ADAPTER), strlen("ATTACH_ADAPTER"));
	KUNIT_EXPECT_STREQ(test, buf, "ATTACH_ADAPTER");

	memset(buf, 0, size);
	KUNIT_EXPECT_EQ(test, panel_drv_ioctl_scnprintf_cmd_name(buf, size, PANEL_IOC_GET_PANEL_STATE), strlen("GET_PANEL_STATE"));
	KUNIT_EXPECT_STREQ(test, buf, "GET_PANEL_STATE");
}

static void panel_drv_ioctl_test_panel_drv_ioctl_scnprintf_cmd_name_should_print_ioctl_cmd_detail_information_if_not_exist(struct kunit *test)
{
	size_t size = 128;
	char *buf = kunit_kmalloc(test, size, GFP_KERNEL);
	char *cmp_buf = kunit_kmalloc(test, size, GFP_KERNEL);
	unsigned int cmd;

	memset(buf, 0, size);
	cmd = _IOW('T', 10, int);
	scnprintf(cmp_buf, size, "cmd=0x%x (%c nr=%d len=%ld dir=%d)",
			cmd, 'T', 10, sizeof(int), _IOC_WRITE);
	KUNIT_EXPECT_EQ(test, panel_drv_ioctl_scnprintf_cmd_name(buf, size, cmd), strlen(cmp_buf));
	KUNIT_EXPECT_STREQ(test, buf, cmp_buf);

	memset(buf, 0, size);
	cmd = _IOW('T', 11, struct timespec64 *);
	scnprintf(cmp_buf, size, "cmd=0x%x (%c nr=%d len=%ld dir=%d)",
			cmd, 'T', 11, sizeof(struct timespec64 *), _IOC_WRITE);
	KUNIT_EXPECT_EQ(test, panel_drv_ioctl_scnprintf_cmd_name(buf, size, cmd), strlen(cmp_buf));
	KUNIT_EXPECT_STREQ(test, buf, cmp_buf);

	/* ioctl command number is out of range */
#if defined(CONFIG_USDM_PANEL_MASK_LAYER)
	memset(buf, 0, size);
	cmd = _IO(PANEL_IOC_BASE, _IOC_NR(PANEL_IOC_SET_MASK_LAYER) + 1);
	scnprintf(cmp_buf, size, "cmd=0x%x (%c nr=%d len=%d dir=%d)",
			cmd, PANEL_IOC_BASE, (int)_IOC_NR(PANEL_IOC_SET_MASK_LAYER) + 1, 0, _IOC_NONE);
	KUNIT_EXPECT_EQ(test, panel_drv_ioctl_scnprintf_cmd_name(buf, size, cmd), strlen(cmp_buf));
	KUNIT_EXPECT_STREQ(test, buf, cmp_buf);
#else
	memset(buf, 0, size);
	cmd = _IO(PANEL_IOC_BASE, _IOC_NR(PANEL_IOC_REG_VRR_CB) + 1);
	scnprintf(cmp_buf, size, "cmd=0x%x (%c nr=%d len=%d dir=%d)",
			cmd, PANEL_IOC_BASE, (int)_IOC_NR(PANEL_IOC_REG_VRR_CB) + 1, 0, _IOC_NONE);
	KUNIT_EXPECT_EQ(test, panel_drv_ioctl_scnprintf_cmd_name(buf, size, cmd), strlen(cmp_buf));
	KUNIT_EXPECT_STREQ(test, buf, cmp_buf);
#endif
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_attach_adapter_by_PANEL_IOC_ATTACH_ADAPTER(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	void *ctx = kunit_kzalloc(test, 128, GFP_KERNEL);

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_attach_adapter(ptr_eq(test, panel), ptr_eq(test, ctx))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_ATTACH_ADAPTER, ctx), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_register_error_cb_by_PANEL_IOC_REG_RESET_CB(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	struct disp_error_cb_info cb_info;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_register_error_cb(ptr_eq(test, panel), ptr_eq(test, &cb_info))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_REG_RESET_CB, &cb_info), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_register_cb_by_PANEL_IOC_REG_VRR_CB(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	struct disp_cb_info cb_info;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_register_cb(ptr_eq(test, panel), int_eq(test, PANEL_CB_VRR), ptr_eq(test, &cb_info))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_REG_VRR_CB, &cb_info), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_get_panel_state_by_PANEL_IOC_GET_PANEL_STATE(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	struct panel_state *state;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_get_panel_state(ptr_eq(test, panel), ptr_eq(test, &state))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_GET_PANEL_STATE, &state), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_probe_by_PANEL_IOC_PANEL_PROBE(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_probe(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_PANEL_PROBE, NULL), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_sleep_in_by_PANEL_IOC_SLEEP_IN(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_sleep_in(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_SLEEP_IN, NULL), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_sleep_out_by_PANEL_IOC_SLEEP_OUT(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_sleep_out(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_SLEEP_OUT, NULL), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_display_on_by_PANEL_IOC_DISP_ON(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	int onoff = 1;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_display_on(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_DISP_ON, &onoff), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_display_off_by_PANEL_IOC_DISP_ON(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	int onoff = 0;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_display_off(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_DISP_ON, &onoff), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_power_on_by_PANEL_IOC_SET_POWER(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	int onoff = 1;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_power_on(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_SET_POWER, &onoff), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_power_off_by_PANEL_IOC_SET_POWER(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	int onoff = 0;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_power_off(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_SET_POWER, &onoff), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_debug_dump_by_PANEL_IOC_PANEL_DUMP(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_debug_dump(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_PANEL_DUMP, NULL), (long)0);
}

#ifdef CONFIG_USDM_PANEL_LPM
static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_doze_by_PANEL_IOC_DOZE(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_doze(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_DOZE, NULL), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_doze_by_PANEL_IOC_DOZE_SUSPEND(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_doze(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_DOZE_SUSPEND, NULL), (long)0);
}
#endif

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_register_cb_by_PANEL_IOC_REG_DISPLAY_MODE_CB(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	struct disp_cb_info cb_info;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_register_cb(ptr_eq(test, panel), int_eq(test, PANEL_CB_DISPLAY_MODE), ptr_eq(test, &cb_info))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_REG_DISPLAY_MODE_CB, &cb_info), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_get_display_mode_by_PANEL_IOC_GET_DISPLAY_MODE(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	struct panel_display_modes *panel_modes;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_get_display_mode(ptr_eq(test, panel), ptr_eq(test, &panel_modes))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_GET_DISPLAY_MODE, &panel_modes), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_set_display_mode_by_PANEL_IOC_SET_DISPLAY_MODE(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	int display_mode_index = 0;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_set_display_mode(ptr_eq(test, panel), ptr_eq(test, &display_mode_index))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_SET_DISPLAY_MODE, &display_mode_index), (long)0);
}
#endif

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_event_frame_done_by_PANEL_IOC_EVT_FRAME_DONE(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	struct timespec64 ts;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_frame_done(ptr_eq(test, panel), ptr_eq(test, &ts))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_EVT_FRAME_DONE, &ts), (long)0);
}

static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_event_vsync_by_PANEL_IOC_EVT_VSYNC(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	struct timespec64 ts;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_vsync(ptr_eq(test, panel), ptr_eq(test, &ts))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_EVT_VSYNC, &ts), (long)0);
}

#ifdef CONFIG_USDM_PANEL_MASK_LAYER
static void panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_set_mask_layer_by_PANEL_IOC_SET_MASK_LAYER(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct mock_expectation *expectation;
	struct mask_layer_data req_mask_data;

	panel->funcs = &mock_panel_drv_funcs;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_set_mask_layer(ptr_eq(test, panel), ptr_eq(test, &req_mask_data))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_ioctl(panel, PANEL_IOC_SET_MASK_LAYER, &req_mask_data), (long)0);
}
#endif

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_drv_ioctl_test_init(struct kunit *test)
{
	struct panel_device *panel;

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel_mutex_init(panel, &panel->io_lock);

	panel->funcs = &panel_drv_funcs;
	INIT_LIST_HEAD(&panel->gpio_list);
	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->regulator_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);
	INIT_LIST_HEAD(&panel->prop_list);

	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_drv_ioctl_test_exit(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_drv_ioctl_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_drv_ioctl_test_test),

	KUNIT_CASE(panel_drv_test_panel_drv_ioctl_fail_with_invalid_ioctl_command),

	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_scnprintf_cmd_name_success),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_scnprintf_cmd_name_should_print_ioctl_cmd_detail_information_if_not_exist),

	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_attach_adapter_by_PANEL_IOC_ATTACH_ADAPTER),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_register_error_cb_by_PANEL_IOC_REG_RESET_CB),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_register_cb_by_PANEL_IOC_REG_VRR_CB),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_get_panel_state_by_PANEL_IOC_GET_PANEL_STATE),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_probe_by_PANEL_IOC_PANEL_PROBE),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_sleep_in_by_PANEL_IOC_SLEEP_IN),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_sleep_out_by_PANEL_IOC_SLEEP_OUT),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_display_on_by_PANEL_IOC_DISP_ON),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_display_off_by_PANEL_IOC_DISP_ON),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_power_on_by_PANEL_IOC_SET_POWER),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_power_off_by_PANEL_IOC_SET_POWER),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_debug_dump_by_PANEL_IOC_PANEL_DUMP),
#ifdef CONFIG_USDM_PANEL_LPM
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_doze_by_PANEL_IOC_DOZE),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_doze_by_PANEL_IOC_DOZE_SUSPEND),
#endif
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_register_cb_by_PANEL_IOC_REG_DISPLAY_MODE_CB),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_get_display_mode_by_PANEL_IOC_GET_DISPLAY_MODE),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_set_display_mode_by_PANEL_IOC_SET_DISPLAY_MODE),
#endif
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_event_frame_done_by_PANEL_IOC_EVT_FRAME_DONE),
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_ioctl_event_vsync_by_PANEL_IOC_EVT_VSYNC),
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	KUNIT_CASE(panel_drv_ioctl_test_panel_drv_ioctl_should_call_panel_set_mask_layer_by_PANEL_IOC_SET_MASK_LAYER),
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
static struct kunit_suite panel_drv_ioctl_test_module = {
	.name = "panel_drv_ioctl_test",
	.init = panel_drv_ioctl_test_init,
	.exit = panel_drv_ioctl_test_exit,
	.test_cases = panel_drv_ioctl_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_drv_ioctl_test_module);

MODULE_LICENSE("GPL v2");
