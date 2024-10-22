// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_freq_hop.h"
#include "sdp_adaptive_mipi.h"
#include "dev_ril_header.h"
#include <linux/sdp/adaptive_mipi_v2.h>
#include <linux/sdp/adaptive_mipi_v2_cp_info.h>

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

extern int u32_array_to_mipi_rating_elem(
		struct adaptive_mipi_v2_table_element *elem,
		u32 *array, u32 num_values);

extern int u32_array_to_osc_sel_elem(
		struct adaptive_mipi_v2_table_element *elem,
		u32 *array, u32 num_values);

struct sdp_adaptive_mipi_test {
	struct panel_device *panel;
};

static int ret_mipi_clk_kbps;
static int ret_osc_clk_khz;

static int test_apply_freq(int mipi_clk_kbps, int osc_clk_khz, void *ctx)
{
	ret_mipi_clk_kbps = mipi_clk_kbps;
	ret_osc_clk_khz = osc_clk_khz;

	return 0;
}

static struct adaptive_mipi_v2_adapter_funcs test_adaptive_mipi_v2_adapter_funcs = {
	.apply_freq = test_apply_freq,
};

static struct adaptive_mipi_v2_table_element mipi_table_10m[] = {
	{ .rat = 3, .band = 91, .from_ch = 0,    .end_ch = 599,  .mipi_clocks_rating = {0, 10, 20} },
	{ .rat = 3, .band = 92, .from_ch = 600,  .end_ch = 1199, .mipi_clocks_rating = {10, 10, 10} },
	{ .rat = 3, .band = 93, .from_ch = 1200, .end_ch = 1807, .mipi_clocks_rating = {1, 1, 10} },
	{ .rat = 3, .band = 93, .from_ch = 1808, .end_ch = 1889, .mipi_clocks_rating = {10, 10, 10} },
	{ .rat = 3, .band = 93, .from_ch = 1890, .end_ch = 1947, .mipi_clocks_rating = {10, 10, 10} },
	{ .rat = 3, .band = 93, .from_ch = 1948, .end_ch = 1949, .mipi_clocks_rating = {20, 20, 20} },
};

static struct adaptive_mipi_v2_table_element mipi_table_20m[] = {
	{ .rat = 3, .band = 94,  .from_ch = 1950, .end_ch = 2399, .mipi_clocks_rating = {10, 10, 10} },
	{ .rat = 3, .band = 95,  .from_ch = 2400, .end_ch = 2649, .mipi_clocks_rating = {20, 20, 20} },
	{ .rat = 3, .band = 97,  .from_ch = 2750, .end_ch = 3449, .mipi_clocks_rating = {8, 8, 0} },
	{ .rat = 3, .band = 98,  .from_ch = 3450, .end_ch = 3799, .mipi_clocks_rating = {20, 20, 20} },
	{ .rat = 3, .band = 102, .from_ch = 5010, .end_ch = 5179, .mipi_clocks_rating = {20, 20, 20} },
	{ .rat = 3, .band = 103, .from_ch = 5180, .end_ch = 5279, .mipi_clocks_rating = {20, 20, 20} },
};

static struct adaptive_mipi_v2_table_element osc_table[] = {
	/* rat  band    from_ch end_ch  osc_idx */
	{ .rat = 3, .band = 91, .from_ch = 0,    .end_ch = 599,  .osc_idx = 1 },
	{ .rat = 3, .band = 91, .from_ch = 2400, .end_ch = 2534, .osc_idx = 1 },
	{ .rat = 3, .band = 91, .from_ch = 3710, .end_ch = 3799, .osc_idx = 1 },
	{ .rat = 3, .band = 91, .from_ch = 5280, .end_ch = 5379, .osc_idx = 1 },
	{ .rat = 3, .band = 91, .from_ch = 5850, .end_ch = 5999, .osc_idx = 1 },
	{ .rat = 3, .band = 91, .from_ch = 6000, .end_ch = 6074, .osc_idx = 1 },
	{ .rat = 3, .band = 91, .from_ch = 8690, .end_ch = 8924, .osc_idx = 1 },
	{ .rat = 3, .band = 91, .from_ch = 9210, .end_ch = 9489, .osc_idx = 1 },

	{ .rat = 7, .band = 91, .from_ch = 173800, .end_ch = 176480, .osc_idx = 1 },
	{ .rat = 7, .band = 91, .from_ch = 190181, .end_ch = 191980, .osc_idx = 1 },
	{ .rat = 7, .band = 91, .from_ch = 151600, .end_ch = 157180, .osc_idx = 1 },
};

#define TEST_MIPI_CLOCK_1 (1362000)
#define TEST_MIPI_CLOCK_2 (1368000)
#define TEST_MIPI_CLOCK_3 (1328000)

struct adaptive_mipi_v2_info sdp_adaptive_mipi_v2_info = {
	.mipi_clocks_kbps = { TEST_MIPI_CLOCK_1, TEST_MIPI_CLOCK_2, TEST_MIPI_CLOCK_3 },
	.mipi_clocks_size = 3,
	.osc_clocks_khz = { 96500, 94500 },
	.osc_clocks_size = 2,
	.mipi_table = { mipi_table_10m, mipi_table_20m },
	.mipi_table_size = { ARRAY_SIZE(mipi_table_10m), ARRAY_SIZE(mipi_table_20m) },
	.osc_table = osc_table,
	.osc_table_size = ARRAY_SIZE(osc_table),

	.funcs = &test_adaptive_mipi_v2_adapter_funcs,
};

#define TEST_CONN_STATUS_PRIMARY		0x01
#define TEST_CONN_STATUS_SECONDARY		0x02

#define TEST_BANDWIDTH_NARROW (10000)
#define TEST_BANDWIDTH_WIDE	(20000)

#define TEST_SINR_STRONG			20
#define TEST_SINR_WEAK				10

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
	TEST_MIPI_CLOCK_1, TEST_MIPI_CLOCK_2, TEST_MIPI_CLOCK_3,
};

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

#if 0
static void panel_adaptive_mipi_search_test(struct kunit *test)
{
	struct sdp_adaptive_mipi_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct adaptive_mipi_v2_info *sdp_adap_mipi = &panel->sdp_adap_mipi;
	struct notifier_block *nb = &sdp_adap_mipi->ril_nb;
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
#endif

static void adaptive_mipi_v2_test_no_matched_info_at_all(struct kunit *test)
{
	struct sdp_adaptive_mipi_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct adaptive_mipi_v2_info *sdp_adap_mipi = &panel->sdp_adap_mipi;
	struct notifier_block *nb = &sdp_adap_mipi->ril_nb;

	struct cp_info cp_msg = {
		.cell_count = 3,
		.infos[0] = {
			.rat = 3,
			.band = 92,
			.channel = 599,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_NARROW,
			.sinr = JUDGE_STRONG_SIGNAL + 1,
		},
		.infos[1] = {
			.rat = 2,
			.band = 92,
			.channel = 600,
			.connection_status = STATUS_SECONDARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_NARROW,
			.sinr = JUDGE_STRONG_SIGNAL,
		},
		.infos[2] = {
			.rat = 3,
			.band = 92,
			.channel = 600,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_WIDE,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
	};

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg),
		.data = (void *)&cp_msg,
	};

	memcpy(sdp_adap_mipi, &sdp_adaptive_mipi_v2_info, sizeof(*sdp_adap_mipi));
	KUNIT_ASSERT_EQ(test, sdp_init_adaptive_mipi_v2(sdp_adap_mipi), 0);

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, TEST_MIPI_CLOCK_2);
	KUNIT_EXPECT_EQ(test, ret_osc_clk_khz, 96500);
}

static void adaptive_mipi_v2_test_no_matched_info_partially(struct kunit *test)
{
	struct sdp_adaptive_mipi_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct adaptive_mipi_v2_info *sdp_adap_mipi = &panel->sdp_adap_mipi;
	struct notifier_block *nb = &sdp_adap_mipi->ril_nb;

	struct cp_info cp_msg = {
		.cell_count = 3,
		.infos[0] = {
			.rat = 3,
			.band = 92,
			.channel = 599,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_NARROW,
			.sinr = JUDGE_STRONG_SIGNAL + 1,
		},
		.infos[1] = {
			.rat = 7,
			.band = 91,
			.channel = 175000,
			.connection_status = STATUS_SECONDARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_NARROW,
			.sinr = JUDGE_STRONG_SIGNAL,
		},
		.infos[2] = {
			.rat = 3,
			.band = 97,
			.channel = 3000,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_WIDE,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
	};

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg),
		.data = (void *)&cp_msg,
	};

	memcpy(sdp_adap_mipi, &sdp_adaptive_mipi_v2_info, sizeof(*sdp_adap_mipi));
	KUNIT_ASSERT_EQ(test, sdp_init_adaptive_mipi_v2(sdp_adap_mipi), 0);

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, TEST_MIPI_CLOCK_3);
	KUNIT_EXPECT_EQ(test, ret_osc_clk_khz, 94500);
}

static void adaptive_mipi_v2_test_all_matched_info(struct kunit *test)
{
	struct sdp_adaptive_mipi_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct adaptive_mipi_v2_info *sdp_adap_mipi = &panel->sdp_adap_mipi;
	struct notifier_block *nb = &sdp_adap_mipi->ril_nb;

	struct cp_info cp_msg = {
		.cell_count = 3,
		.infos[0] = {
			.rat = 3,
			.band = 91,
			.channel = 300,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_NARROW,
			.sinr = 3,
		},
		.infos[1] = {
			.rat = 3,
			.band = 93,
			.channel = 1500,
			.connection_status = STATUS_SECONDARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_NARROW,
			.sinr = JUDGE_STRONG_SIGNAL,
		},
		.infos[2] = {
			.rat = 3,
			.band = 97,
			.channel = 2800,
			.connection_status = STATUS_SECONDARY_SERVING,
			.bandwidth = TEST_BANDWIDTH_WIDE,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},

	};

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg),
		.data = (void *)&cp_msg,
	};

	memcpy(sdp_adap_mipi, &sdp_adaptive_mipi_v2_info, sizeof(*sdp_adap_mipi));
	KUNIT_ASSERT_EQ(test, sdp_init_adaptive_mipi_v2(sdp_adap_mipi), 0);

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, TEST_MIPI_CLOCK_1);
	KUNIT_EXPECT_EQ(test, ret_osc_clk_khz, 94500);
}

static void sdp_adaptive_mipi_test_u32_array_to_mipi_rating_elem(struct kunit *test)
{
	const u32 TEST_RAT = 2;
	const u32 TEST_BAND = 11;
	const u32 TEST_FROM_CH = 10562;
	const u32 TEST_END_CH = 10628;
	const u32 TEST_RATING[3] = { 0, 0, 80 };
	u32 array[10] = { 0, };
	struct adaptive_mipi_v2_table_element *elem =
		kunit_kzalloc(test, sizeof(*elem), GFP_KERNEL);

	/* initialize u32 array */
	array[0] = TEST_RAT;
	array[1] = TEST_BAND;
	array[2] = TEST_FROM_CH;
	array[3] = TEST_END_CH;
	array[4] = TEST_RATING[0];
	array[5] = TEST_RATING[1];
	array[6] = TEST_RATING[2];

	KUNIT_EXPECT_EQ(test, u32_array_to_mipi_rating_elem(elem, array, 3), 0);
	/* boundary check */
	KUNIT_EXPECT_EQ(test, array[7], 0);

	KUNIT_EXPECT_EQ(test, elem->rat, TEST_RAT);
	KUNIT_EXPECT_EQ(test, elem->band, TEST_BAND);
	KUNIT_EXPECT_EQ(test, elem->from_ch, TEST_FROM_CH);
	KUNIT_EXPECT_EQ(test, elem->end_ch, TEST_END_CH);
	KUNIT_EXPECT_EQ(test, elem->mipi_clocks_rating[0], TEST_RATING[0]);
	KUNIT_EXPECT_EQ(test, elem->mipi_clocks_rating[1], TEST_RATING[1]);
	KUNIT_EXPECT_EQ(test, elem->mipi_clocks_rating[2], TEST_RATING[2]);
}

static void sdp_adaptive_mipi_test_u32_array_to_osc_sel_elem(struct kunit *test)
{
	const u32 TEST_RAT = 2;
	const u32 TEST_BAND = 11;
	const u32 TEST_FROM_CH = 10562;
	const u32 TEST_END_CH = 10628;
	const u32 TEST_OSC_IDX = 1;
	u32 array[10] = { 0, };
	struct adaptive_mipi_v2_table_element *elem =
		kunit_kzalloc(test, sizeof(*elem), GFP_KERNEL);

	/* initialize u32 array */
	array[0] = TEST_RAT;
	array[1] = TEST_BAND;
	array[2] = TEST_FROM_CH;
	array[3] = TEST_END_CH;
	array[4] = TEST_OSC_IDX;

	KUNIT_EXPECT_EQ(test, u32_array_to_osc_sel_elem(elem, array, 1), 0);
	/* boundary check */
	KUNIT_EXPECT_EQ(test, array[5], 0);

	/* element value check */
	KUNIT_EXPECT_EQ(test, elem->rat, TEST_RAT);
	KUNIT_EXPECT_EQ(test, elem->band, TEST_BAND);
	KUNIT_EXPECT_EQ(test, elem->from_ch, TEST_FROM_CH);
	KUNIT_EXPECT_EQ(test, elem->end_ch, TEST_END_CH);
	KUNIT_EXPECT_EQ(test, elem->osc_idx, TEST_OSC_IDX);
}

static void sdp_adaptive_mipi_test_get_freq_test(struct kunit *test)
{
	struct sdp_adaptive_mipi_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct adaptive_mipi_v2_info *sdp_adap_mipi = &panel->sdp_adap_mipi;
	struct notifier_block *nb = &sdp_adap_mipi->ril_nb;
	struct panel_adaptive_mipi *adap_mipi = &panel->adap_mipi;

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

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
	};

	int i;

	for (i = 0; i < MAX_BANDWIDTH_IDX; i++)
		INIT_LIST_HEAD(&adap_mipi->rf_info_head[i]);

	register_band_info(adap_mipi);
	adaptive_mipi_v2_info_initialize(sdp_adap_mipi, adap_mipi);
	sdp_adap_mipi->ctx = panel;
	sdp_adap_mipi->funcs = &test_adaptive_mipi_v2_adapter_funcs;

	KUNIT_ASSERT_EQ(test, sdp_init_adaptive_mipi_v2(sdp_adap_mipi), 0);

	ret_mipi_clk_kbps = -1;
	ret_osc_clk_khz = -1;
	ril_msg.data_len = sizeof(test_cp_msg1);
	ril_msg.data = (void *)&test_cp_msg1;
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, TEST_MIPI_CLOCK_1);

	ret_mipi_clk_kbps = -1;
	ret_osc_clk_khz = -1;
	ril_msg.data_len = sizeof(test_cp_msg2);
	ril_msg.data = (void *)&test_cp_msg2;
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, TEST_MIPI_CLOCK_1);

	ret_mipi_clk_kbps = -1;
	ret_osc_clk_khz = -1;
	ril_msg.data_len = sizeof(test_cp_msg3);
	ril_msg.data = (void *)&test_cp_msg3;
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, TEST_MIPI_CLOCK_2);

	ret_mipi_clk_kbps = -1;
	ret_osc_clk_khz = -1;
	ril_msg.data_len = sizeof(test_cp_msg4);
	ril_msg.data = (void *)&test_cp_msg4;
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, TEST_MIPI_CLOCK_3);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int sdp_adaptive_mipi_test_init(struct kunit *test)
{
	struct sdp_adaptive_mipi_test *ctx;
	struct panel_device *panel;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel->of_node_name = "test_panel_name";
	panel->dev = device_create(lcd_class,
			NULL, 0, panel, PANEL_DRV_NAME);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->dev);

	ctx->panel = panel;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void sdp_adaptive_mipi_test_exit(struct kunit *test)
{
	struct sdp_adaptive_mipi_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct adaptive_mipi_v2_info *sdp_adap_mipi = &panel->sdp_adap_mipi;

	KUNIT_ASSERT_EQ(test, sdp_cleanup_adaptive_mipi_v2(sdp_adap_mipi), 0);
	device_unregister(panel->dev);
	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case sdp_adaptive_mipi_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(adaptive_mipi_v2_test_no_matched_info_at_all),
	KUNIT_CASE(adaptive_mipi_v2_test_no_matched_info_partially),
	KUNIT_CASE(adaptive_mipi_v2_test_all_matched_info),
	//KUNIT_CASE(panel_adaptive_mipi_search_test),
	KUNIT_CASE(sdp_adaptive_mipi_test_u32_array_to_mipi_rating_elem),
	KUNIT_CASE(sdp_adaptive_mipi_test_u32_array_to_osc_sel_elem),
	KUNIT_CASE(sdp_adaptive_mipi_test_get_freq_test),
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
static struct kunit_suite sdp_adaptive_mipi_test_module = {
	.name = "sdp_adaptive_mipi_test",
	.init = sdp_adaptive_mipi_test_init,
	.exit = sdp_adaptive_mipi_test_exit,
	.test_cases = sdp_adaptive_mipi_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&sdp_adaptive_mipi_test_module);

MODULE_LICENSE("GPL v2");
