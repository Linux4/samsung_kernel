// SPDX-License-Identifier: GPL-2.0
/*
 * Panel Samsung Driver KUnit test.
 *
 * Copyright (C) 2021, Samsung Electronics.
 * Author: Gwanghui Lee <gwanghui.lee@samsung.com>
 */

#include <kunit/test.h>
#include <linux/of.h>
#include "panel/panel-samsung-drv.h"
#include "exynos_drm_dsim.h"

extern int mcd_drm_mipi_read(void *_ctx, u8 addr, u32 offset, u8 *buf, int size, u32 option);
extern int mcd_drm_mipi_write(void *_ctx, u8 cmd_id, const u8 *cmd, u32 offset, int size, u32 option);
extern int mcd_drm_mipi_get_state(void *_ctx);
extern int mcd_drm_parse_dt(void *_ctx, struct device_node *np);
extern int mcd_drm_wait_for_vsync(void *_ctx, u32 timeout);

struct mcd_panel_samsung_drv_test {
	struct exynos_panel *exynos_panel;
	struct device_node *np;
	struct mipi_dsi_device *dsi;
	struct mipi_dsi_host *dsi_host;
};

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct test *)`. `struct test` is a context object that stores
 * information about the current test.
 */
static void mcd_panel_samsung_drv_simple_test(struct test *test)
{
	/*
	 * This is an EXPECTATION; it is how KUnit tests things. When you want
	 * to test a piece of code, you set some expectations about what the
	 * code should do. KUnit then runs the test and verifies that the code's
	 * behavior matched what was expected.
	 */
	EXPECT_EQ(test, 1 + 1, 2);
}

static void mcd_panel_samsung_drv_test_mcd_drm_mipi_write_fail_with_invalid_args(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx = test->priv;
	struct exynos_panel *exynos_panel = ctx->exynos_panel;
	unsigned char buffer[3] = { 0xF0, 0x5A, 0x5A };

	EXPECT_ERROR(test, mcd_drm_mipi_write(NULL, 0, NULL, 0, 0, 0), -EINVAL);

	/* test error if target buffer is null */
	EXPECT_ERROR(test, mcd_drm_mipi_write(exynos_panel, 0, NULL, 0, 0, 0), -ENODATA);

	/* test error if exynos_panel->dev is null */
	exynos_panel->dev = NULL;
	EXPECT_ERROR(test, mcd_drm_mipi_write(exynos_panel, 0, buffer, 3, 0, 0), -EINVAL);
}

static void mcd_panel_samsung_drv_test_mcd_drm_mipi_write_fail_if_dsi_host_is_null(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx = test->priv;
	struct exynos_panel *exynos_panel = ctx->exynos_panel;
	struct mipi_dsi_device *dsi = ctx->dsi;
	unsigned char buffer[3] = { 0xF0, 0x5A, 0x5A };

	dsi->host = NULL;
	EXPECT_ERROR(test, mcd_drm_mipi_write(exynos_panel, 0, buffer, 3, 0, 0), -EINVAL);
}

static void mcd_panel_samsung_drv_test_mcd_drm_mipi_read_fail_with_invalid_args(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx = test->priv;
	struct exynos_panel *exynos_panel = ctx->exynos_panel;
	unsigned char buffer[3];

	EXPECT_ERROR(test, mcd_drm_mipi_read(NULL, 0, 0, NULL, 0, 0), -EINVAL);

	/* test error if target buffer is null */
	EXPECT_ERROR(test, mcd_drm_mipi_read(exynos_panel, 0, 0, NULL, 0, 0), -ENODATA);

	/* test error if exynos_panel->dev is null */
	exynos_panel->dev = NULL;
	EXPECT_ERROR(test, mcd_drm_mipi_read(exynos_panel, 0x04, 0, buffer, 3, 0), -EINVAL);
}

static void mcd_panel_samsung_drv_test_mcd_drm_mipi_read_fail_if_dsi_host_is_null(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx = test->priv;
	struct exynos_panel *exynos_panel = ctx->exynos_panel;
	struct mipi_dsi_device *dsi = ctx->dsi;
	unsigned char buffer[3];

	dsi->host = NULL;
	EXPECT_ERROR(test, mcd_drm_mipi_read(exynos_panel, 0x04, 0, buffer, 3, 0), -EINVAL);
}

static void mcd_panel_samsung_drv_test_mcd_drm_mipi_get_state_fail_with_invalid_args(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx = test->priv;
	struct exynos_panel *exynos_panel = ctx->exynos_panel;

	EXPECT_ERROR(test, mcd_drm_mipi_get_state(NULL), -EINVAL);

	/* test error if exynos_panel->dev is null */
	exynos_panel->dev = NULL;
	EXPECT_ERROR(test, mcd_drm_mipi_get_state(exynos_panel), -EINVAL);
}

static void mcd_panel_samsung_drv_test_mcd_drm_mipi_get_state_fail_if_dsi_host_is_null(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx = test->priv;
	struct exynos_panel *exynos_panel = ctx->exynos_panel;
	struct mipi_dsi_device *dsi = ctx->dsi;

	dsi->host = NULL;
	EXPECT_ERROR(test, mcd_drm_mipi_get_state(exynos_panel), -EINVAL);
}

static void mcd_panel_samsung_drv_test_mcd_drm_parse_dt_fail_with_invalid_args(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx = test->priv;
	struct exynos_panel *exynos_panel = ctx->exynos_panel;
	struct device_node *np = ctx->np;

	EXPECT_ERROR(test, mcd_drm_parse_dt(NULL, np), -EINVAL);
	EXPECT_ERROR(test, mcd_drm_parse_dt(exynos_panel, NULL), -EINVAL);

	/* test error if exynos_panel->dev is null */
	exynos_panel->dev = NULL;
	EXPECT_ERROR(test, mcd_drm_parse_dt(exynos_panel, np), -EINVAL);
}

static void mcd_panel_samsung_drv_test_mcd_drm_wait_for_vsync_fail_with_invalid_args(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx = test->priv;
	struct exynos_panel *exynos_panel = ctx->exynos_panel;
	struct mipi_dsi_host *orig_host;
	struct mipi_dsi_device *mipi_device;
	struct dsim_device *dsim;
	struct drm_crtc *crtc;

	EXPECT_ERROR(test, mcd_drm_wait_for_vsync(NULL, 0), -ENODEV);

	exynos_panel->panel_drm_state = PANEL_DRM_STATE_DISABLED;
	EXPECT_ERROR(test, mcd_drm_wait_for_vsync(exynos_panel, 0), -EINVAL);
	exynos_panel->panel_drm_state = PANEL_DRM_STATE_ENABLED;

	mipi_device = to_mipi_dsi_device(exynos_panel->dev);
	orig_host = mipi_device->host;
	mipi_device->host = NULL;
	EXPECT_ERROR(test, mcd_drm_wait_for_vsync(exynos_panel, 0), -EINVAL);
	mipi_device->host = orig_host;

	dsim = container_of(mipi_device->host, struct dsim_device, dsi_host);
	crtc = dsim->encoder.crtc;
	dsim->encoder.crtc = NULL;
	EXPECT_ERROR(test, mcd_drm_wait_for_vsync(exynos_panel, 0), -EINVAL);
	dsim->encoder.crtc = crtc;
}

/*
 * This is run once before each test case, see the comment on
 * mcd_panel_samsung_drv_test_suite for more information.
 */
static int mcd_panel_samsung_drv_test_init(struct test *test)
{
	struct mcd_panel_samsung_drv_test *ctx;
	struct mipi_dsi_device *dsi_device;

	ctx = test_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	ASSERT_NOT_NULL(test, ctx);

	ctx->exynos_panel = test_kzalloc(test, sizeof(*ctx->exynos_panel), GFP_KERNEL);
	ASSERT_NOT_NULL(test, ctx->exynos_panel);

	ctx->np = test_kzalloc(test, sizeof(*ctx->np), GFP_KERNEL);
	ASSERT_NOT_NULL(test, ctx->np);

	ctx->dsi = test_kzalloc(test, sizeof(*ctx->dsi), GFP_KERNEL);
	ASSERT_NOT_NULL(test, ctx->dsi);

	ctx->dsi_host = test_kzalloc(test, sizeof(*ctx->dsi_host), GFP_KERNEL);
	ASSERT_NOT_NULL(test, ctx->dsi_host);

	ctx->exynos_panel->dev = &ctx->dsi->dev;
	ctx->dsi->host = ctx->dsi_host;

	mutex_init(&ctx->exynos_panel->panel_drm_state_lock);
	test->priv = ctx;

	return 0;
}

/*
 * Here we make a list of all the test cases we want to add to the test suite
 * below.
 */
static struct test_case mcd_panel_samsung_drv_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test suite.
	 */
	TEST_CASE(mcd_panel_samsung_drv_simple_test),
	TEST_CASE(mcd_panel_samsung_drv_test_mcd_drm_mipi_write_fail_with_invalid_args),
	TEST_CASE(mcd_panel_samsung_drv_test_mcd_drm_mipi_write_fail_if_dsi_host_is_null),
	TEST_CASE(mcd_panel_samsung_drv_test_mcd_drm_mipi_read_fail_with_invalid_args),
	TEST_CASE(mcd_panel_samsung_drv_test_mcd_drm_mipi_read_fail_if_dsi_host_is_null),
	TEST_CASE(mcd_panel_samsung_drv_test_mcd_drm_mipi_get_state_fail_with_invalid_args),
	TEST_CASE(mcd_panel_samsung_drv_test_mcd_drm_mipi_get_state_fail_if_dsi_host_is_null),
	TEST_CASE(mcd_panel_samsung_drv_test_mcd_drm_parse_dt_fail_with_invalid_args),
	TEST_CASE(mcd_panel_samsung_drv_test_mcd_drm_wait_for_vsync_fail_with_invalid_args),
	{}
};

/*
 * This defines a suite or grouping of tests.
 *
 * Test cases are defined as belonging to the suite by adding them to
 * `kunit_cases`.
 *
 * Often it is desirable to run some function which will set up things which
 * will be used by every test; this is accomplished with an `init` function
 * which runs before each test case is invoked. Similarly, an `exit` function
 * may be specified which runs after every test case and can be used to for
 * cleanup. For clarity, running tests in a test suite would behave as follows:
 *
 * suite.init(test);
 * suite.test_case[0](test);
 * suite.exit(test);
 * suite.init(test);
 * suite.test_case[1](test);
 * suite.exit(test);
 * ...;
 */
static struct test_module mcd_panel_samsung_drv_test_module = {
	.name = "mcd_panel_samsung_drv",
	.init = mcd_panel_samsung_drv_test_init,
	.test_cases = mcd_panel_samsung_drv_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
module_test(mcd_panel_samsung_drv_test_module);
