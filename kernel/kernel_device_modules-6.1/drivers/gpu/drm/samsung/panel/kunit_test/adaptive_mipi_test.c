// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_freq_hop.h"
#include "dev_ril_header.h"

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


extern struct class *lcd_class;
extern struct rf_band_element
*search_rf_band_element(struct panel_adaptive_mipi *adap_mipi, struct band_info *band);
extern int
get_optimal_frequency(struct panel_adaptive_mipi *adap_mipi, struct cp_info *cp_msg, struct freq_hop_param *param);


#define TEST_CONN_STATUS_PRIMARY		0x01
#define TEST_CONN_STATUS_SECONDARY		0x02

#define TEST_BANDWIDTH_WIDE			20000
#define TEST_BANDWIDTH_NARROW			10000

#define TEST_SINR_STRONG			20
#define TEST_SINR_WEAK				10

struct panel_adaptive_mipi_test {
	struct panel_device *panel;
};

struct rf_band_element band_info1[] = {
	[0] = DEFINE_AD_TABLE_RATING3(LB07, 3040, 3179, 0, 0, 100, 0),
	[1] = DEFINE_AD_TABLE_RATING3(LB26, 8690, 8719, 0, 30, 0, 0),
	[2] = DEFINE_AD_TABLE_RATING3(LB03, 1808, 1889, 20, 0, 0, 0),
	[3] = DEFINE_AD_TABLE_RATING3(LB40, 39555, 39649, 0, 10, 0, 0),
};

struct rf_band_element band_info2[] = {
	[0] = DEFINE_AD_TABLE_RATING3(WB01, 10562, 10754, 0, 0, 0, 0),
};

unsigned int test_mipi_lists[MAX_MIPI_FREQ_CNT] = {
	1362, 1368, 1328
};

static void panel_adaptive_mipi_search_test(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct panel_adaptive_mipi *adap_mipi = &panel->adap_mipi;
	struct band_info search_test_band[] = {
		[0] = DEFINE_TEST_BAND(LB07, 3040, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
		[1] = DEFINE_TEST_BAND(LB07, 3179, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_NARROW, TEST_SINR_STRONG),
		[2] = DEFINE_TEST_BAND(LB07, 3050, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
		[3] = DEFINE_TEST_BAND(LB07, 3050, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_WIDE, TEST_SINR_STRONG),
		[4] = DEFINE_TEST_BAND(WB01, 10562, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
		[5] = DEFINE_TEST_BAND(WB01, 10600, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_WIDE, TEST_SINR_WEAK),
	};

	/* test narrow band */
	KUNIT_EXPECT_PTR_EQ(test, search_rf_band_element(adap_mipi, &search_test_band[0]), &band_info1[0]);
	KUNIT_EXPECT_PTR_EQ(test, search_rf_band_element(adap_mipi, &search_test_band[1]), &band_info1[0]);
	KUNIT_EXPECT_PTR_EQ(test, search_rf_band_element(adap_mipi, &search_test_band[2]), &band_info1[0]);
	KUNIT_EXPECT_TRUE(test, search_rf_band_element(adap_mipi, &search_test_band[3]) == NULL);

	/*test wide bandwidth*/
	KUNIT_EXPECT_TRUE(test, search_rf_band_element(adap_mipi, &search_test_band[4]) == NULL);
	KUNIT_EXPECT_PTR_EQ(test, search_rf_band_element(adap_mipi, &search_test_band[5]), &band_info2[0]);
}


struct cp_info test_cp_msg1 = {
	.infos = {
		[0] = DEFINE_TEST_BAND(LB07, 3100, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
		[1] = DEFINE_TEST_BAND(LB26, 8700, TEST_CONN_STATUS_SECONDARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
	},
	.cell_count = 2,
};

struct cp_info test_cp_msg2 = {
	.infos = {
		[0] = DEFINE_TEST_BAND(LB07, 3100, TEST_CONN_STATUS_SECONDARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
		[1] = DEFINE_TEST_BAND(LB26, 8700, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
	},
	.cell_count = 2,
};

struct cp_info test_cp_msg3 = {
	.infos = {
		[0] = DEFINE_TEST_BAND(LB03, 1840, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
		[1] = DEFINE_TEST_BAND(LB07, 3100, TEST_CONN_STATUS_SECONDARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
	},
	.cell_count = 2,
};

struct cp_info test_cp_msg4 = {
	.infos = {
		[0] = DEFINE_TEST_BAND(LB03, 1850, TEST_CONN_STATUS_PRIMARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
		[1] = DEFINE_TEST_BAND(LB26, 8700, TEST_CONN_STATUS_SECONDARY, TEST_BANDWIDTH_NARROW, TEST_SINR_WEAK),
	},
	.cell_count = 2,
};


static void panel_adaptive_mipi_get_freq_test(struct kunit *test)
{
	struct freq_hop_param param;
	struct panel_device *panel = test->priv;
	struct panel_adaptive_mipi *adap_mipi = &panel->adap_mipi;

	get_optimal_frequency(adap_mipi, &test_cp_msg1, &param);
	KUNIT_EXPECT_EQ(test, param.dsi_freq, test_mipi_lists[0] * 1000);

	get_optimal_frequency(adap_mipi, &test_cp_msg2, &param);
	KUNIT_EXPECT_EQ(test, param.dsi_freq, test_mipi_lists[0] * 1000);

	get_optimal_frequency(adap_mipi, &test_cp_msg3, &param);
	KUNIT_EXPECT_EQ(test, param.dsi_freq, test_mipi_lists[1] * 1000);

	get_optimal_frequency(adap_mipi, &test_cp_msg4, &param);
	KUNIT_EXPECT_EQ(test, param.dsi_freq, test_mipi_lists[2] * 1000);
}

static void register_band_info(struct panel_adaptive_mipi *adap_mipi)
{
	int i;
	struct adaptive_mipi_freq *freq_info = &adap_mipi->freq_info;

	for (i = 0; i < ARRAY_SIZE(band_info1); i++)
		list_add_tail(&band_info1[i].list, &adap_mipi->rf_info_head[0]);

	for (i = 0; i < ARRAY_SIZE(band_info2); i++)
		list_add_tail(&band_info2[i].list, &adap_mipi->rf_info_head[1]);

	for (i = 0; i < MAX_MIPI_FREQ_CNT; i++)
		freq_info->mipi_lists[i] = test_mipi_lists[i];

	freq_info->mipi_cnt = MAX_MIPI_FREQ_CNT;
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_adaptive_mipi_test_init(struct kunit *test)
{
	int i;
	struct panel_device *panel;
	struct panel_adaptive_mipi *adap_mipi;

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel->of_node_name = "test_panel_name";
	panel->dev = device_create(lcd_class,
			NULL, 0, panel, PANEL_DRV_NAME);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->dev);

	adap_mipi = &panel->adap_mipi;

	for (i = 0; i < MAX_RF_BW_CNT; i++)
		INIT_LIST_HEAD(&adap_mipi->rf_info_head[i]);

	register_band_info(adap_mipi);

	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_adaptive_mipi_test_exit(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	device_unregister(panel->dev);

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_adaptive_mipi_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_adaptive_mipi_search_test),
	KUNIT_CASE(panel_adaptive_mipi_get_freq_test),
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
static struct kunit_suite panel_adaptive_mipi_test_module = {
	.name = "panel_adaptive_mipi_test",
	.init = panel_adaptive_mipi_test_init,
	.exit = panel_adaptive_mipi_test_exit,
	.test_cases = panel_adaptive_mipi_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_adaptive_mipi_test_module);

MODULE_LICENSE("GPL v2");
