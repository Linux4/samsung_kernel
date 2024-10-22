// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/sdp/adaptive_mipi_v2.h>
#include <linux/sdp/adaptive_mipi_v2_cp_info.h>

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set KUNIT_EXPECTATIONs and KUNIT_ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct kunit *)`. `struct kunit` is a context object that stores
 * information about the current test.
 */

struct adaptive_mipi_v2_table_element mipi_table_10m[] = {
	{ .rat = 3, .band = 91, .from_ch = 0,    .end_ch = 599,  .mipi_clocks_rating = {0, 10, 20} },
	{ .rat = 3, .band = 92, .from_ch = 600,  .end_ch = 1199, .mipi_clocks_rating = {10, 10, 10} },
	{ .rat = 3, .band = 93, .from_ch = 1200, .end_ch = 1807, .mipi_clocks_rating = {1, 1, 10} },
	{ .rat = 3, .band = 93, .from_ch = 1808, .end_ch = 1889, .mipi_clocks_rating = {10, 10, 10} },
	{ .rat = 3, .band = 93, .from_ch = 1890, .end_ch = 1947, .mipi_clocks_rating = {10, 10, 10} },
	{ .rat = 3, .band = 93, .from_ch = 1948, .end_ch = 1949, .mipi_clocks_rating = {20, 20, 20} },
	{ .rat = 3, .band = 104, .from_ch = 7000, .end_ch = 8000, .mipi_clocks_rating = {0, 10, 10} },
	{ .rat = 3, .band = 105, .from_ch = 7000, .end_ch = 8000, .mipi_clocks_rating = {10, 0, 10} },
};

struct adaptive_mipi_v2_table_element mipi_table_20m[] = {
	{ .rat = 3, .band = 94,  .from_ch = 1950, .end_ch = 2399, .mipi_clocks_rating = {10, 10, 10} },
	{ .rat = 3, .band = 95,  .from_ch = 2400, .end_ch = 2649, .mipi_clocks_rating = {20, 20, 20} },
	{ .rat = 3, .band = 97,  .from_ch = 2750, .end_ch = 3449, .mipi_clocks_rating = {8, 8, 0} },
	{ .rat = 3, .band = 98,  .from_ch = 3450, .end_ch = 3799, .mipi_clocks_rating = {20, 20, 20} },
	{ .rat = 3, .band = 102, .from_ch = 5010, .end_ch = 5179, .mipi_clocks_rating = {20, 20, 20} },
	{ .rat = 3, .band = 103, .from_ch = 5180, .end_ch = 5279, .mipi_clocks_rating = {20, 20, 20} },
	{ .rat = 3, .band = 105, .from_ch = 7000, .end_ch = 8000, .mipi_clocks_rating = {10, 10, 0} },
};

struct adaptive_mipi_v2_table_element osc_table[] = {
	/* rat	band	from_ch	end_ch	osc_idx */
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

struct adaptive_mipi_v2_info gInfo = {
	.mipi_clocks_kbps = {951000, 954000, 997000},
	.mipi_clocks_size = 3,
	.osc_clocks_khz = {96500, 94500},
	.osc_clocks_size = 2,

	.mipi_table = { mipi_table_10m, mipi_table_20m },
	.mipi_table_size = { ARRAY_SIZE(mipi_table_10m), ARRAY_SIZE(mipi_table_20m) },
	.osc_table = osc_table,
	.osc_table_size = ARRAY_SIZE(osc_table),

	.funcs = &test_adaptive_mipi_v2_adapter_funcs,
};

#ifdef CONFIG_UML
/* NOTE: UML TC */
static void adaptive_mipi_v2_test_bar(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void adaptive_mipi_v2_test_init_func(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = test->priv;
	struct adaptive_mipi_v2_info emptyInfo;

	KUNIT_ASSERT_EQ(test, sdp_cleanup_adaptive_mipi_v2(info), 0);

	memset(&emptyInfo, 0, sizeof(emptyInfo));
	KUNIT_EXPECT_EQ(test, sdp_init_adaptive_mipi_v2(NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, sdp_init_adaptive_mipi_v2(&emptyInfo), -EINVAL);

	KUNIT_EXPECT_EQ(test, sdp_init_adaptive_mipi_v2(info), 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, info->ril_nb.notifier_call);
}

static void adaptive_mipi_v2_test_invalid_notifier(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = test->priv;
	struct notifier_block *nb = &info->ril_nb;

	struct cp_info cp_msg_count_zero = { .cell_count = 0, };
	struct cp_info cp_msg_count_over_than_max = { .cell_count = MAX_BAND + 1, };
	struct cp_info cp_msg_ok = {
		.cell_count = 1,
		.infos[0] = {
			.rat = 3,
			.band = 92,
			.channel = 599,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL + 1,
		},
	};

	struct dev_ril_bridge_msg ril_msg_cell_count_zero = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(struct cp_info),
		.data = (void *)&cp_msg_count_zero,
	};

	struct dev_ril_bridge_msg ril_msg_cell_count_over_than_max = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(struct cp_info),
		.data = (void *)&cp_msg_count_over_than_max,
	};

	struct dev_ril_bridge_msg ril_msg_invalid_dev_id = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO + 1,
		.data_len = sizeof(struct cp_info),
		.data = (void *)&cp_msg_ok,
	};

	struct dev_ril_bridge_msg ril_msg_ok = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg_ok),
		.data = (void *)&cp_msg_ok,
	};

	ril_msg_cell_count_over_than_max.data = (void *)&cp_msg_count_over_than_max;

	KUNIT_EXPECT_EQ(test, nb->notifier_call(NULL, sizeof(struct dev_ril_bridge_msg),
				(void *)&ril_msg_ok), NOTIFY_BAD);
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(struct dev_ril_bridge_msg),
				NULL), NOTIFY_BAD);

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(struct dev_ril_bridge_msg),
				(void *)&ril_msg_cell_count_zero), NOTIFY_BAD);
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(struct dev_ril_bridge_msg),
				(void *)&ril_msg_cell_count_over_than_max), NOTIFY_BAD);

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(struct dev_ril_bridge_msg),
				(void *)&ril_msg_invalid_dev_id), NOTIFY_DONE);
}

static void adaptive_mipi_v2_test_no_matched_info_at_all(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = test->priv;
	struct notifier_block *nb = &info->ril_nb;

	struct cp_info cp_msg = {
		.cell_count = 3,
		.infos[0] = {
			.rat = 3,
			.band = 92,
			.channel = 599,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL + 1,
		},
		.infos[1] = {
			.rat = 2,
			.band = 92,
			.channel = 600,
			.connection_status = STATUS_SECONDARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL,
		},
		.infos[2] = {
			.rat = 3,
			.band = 92,
			.channel = 600,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 20000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
	};

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg),
		.data = (void *)&cp_msg,
	};

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, 951000);
	KUNIT_EXPECT_EQ(test, ret_osc_clk_khz, 96500);
}

static void adaptive_mipi_v2_test_no_matched_info_partially(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = test->priv;
	struct notifier_block *nb = &info->ril_nb;

	struct cp_info cp_msg = {
		.cell_count = 3,
		.infos[0] = {
			.rat = 3,
			.band = 92,
			.channel = 599,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL + 1,
		},
		.infos[1] = {
			.rat = 7,
			.band = 91,
			.channel = 175000,
			.connection_status = STATUS_SECONDARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL,
		},
		.infos[2] = {
			.rat = 3,
			.band = 97,
			.channel = 3000,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 20000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
	};

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg),
		.data = (void *)&cp_msg,
	};

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, 997000);
	KUNIT_EXPECT_EQ(test, ret_osc_clk_khz, 94500);
}

static void adaptive_mipi_v2_test_all_matched_info(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = test->priv;
	struct notifier_block *nb = &info->ril_nb;

	struct cp_info cp_msg = {
		.cell_count = 3,
		.infos[0] = {
			.rat = 3,
			.band = 91,
			.channel = 300,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 5000,
			.sinr = 3,
		},
		.infos[1] = {
			.rat = 3,
			.band = 93,
			.channel = 1500,
			.connection_status = STATUS_SECONDARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL,
		},
		.infos[2] = {
			.rat = 3,
			.band = 97,
			.channel = 2800,
			.connection_status = STATUS_SECONDARY_SERVING,
			.bandwidth = 20000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},

	};

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg),
		.data = (void *)&cp_msg,
	};

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, 951000);
	KUNIT_EXPECT_EQ(test, ret_osc_clk_khz, 94500);
}

static void adaptive_mipi_v2_test_same_ratings(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = test->priv;
	struct notifier_block *nb = &info->ril_nb;

	struct cp_info cp_msg = {
		.cell_count = 4,
		.infos[0] = {
			.rat = 3,
			.band = 93,
			.channel = 1850,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
		.infos[1] = {
			.rat = 3,
			.band = 93,
			.channel = 1900,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
		.infos[2] = {
			.rat = 3,
			.band = 94,
			.channel = 2000,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 20000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
		.infos[3] = {
			.rat = 3,
			.band = 97,
			.channel = 3000,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 20000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
	};

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg),
		.data = (void *)&cp_msg,
	};

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, 997000);

	cp_msg.infos[3].sinr = JUDGE_STRONG_SIGNAL;
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, 951000);
}

static void adaptive_mipi_v2_test_sinr(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = test->priv;
	struct notifier_block *nb = &info->ril_nb;

	struct cp_info cp_msg = {
		.cell_count = 3,
		.infos[0] = {
			.rat = 3,
			.band = 104,
			.channel = 7500,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 5000,
			.sinr = JUDGE_STRONG_SIGNAL + 1,
		},
		.infos[1] = {
			.rat = 3,
			.band = 105,
			.channel = 7500,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 10000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},
		.infos[2] = {
			.rat = 3,
			.band = 105,
			.channel = 7500,
			.connection_status = STATUS_PRIMARY_SERVING,
			.bandwidth = 20000,
			.sinr = JUDGE_STRONG_SIGNAL - 1,
		},

	};

	struct dev_ril_bridge_msg ril_msg = {
		.dev_id = IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO,
		.data_len = sizeof(cp_msg),
		.data = (void *)&cp_msg,
	};

	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, 954000);

	cp_msg.infos[1].sinr = JUDGE_STRONG_SIGNAL + 1;
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, 997000);

	cp_msg.infos[0].sinr = DEFAULT_WEAK_SIGNAL;
	KUNIT_EXPECT_EQ(test, nb->notifier_call(nb, sizeof(ril_msg), (void *)&ril_msg), NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test, ret_mipi_clk_kbps, 951000);
}

#else
/* NOTE: On device TC */
static void adaptive_mipi_v2_test_foo(struct kunit *test)
{
	/*
	 * This is an KUNIT_EXPECTATION; it is how KUnit tests things. When you want
	 * to test a piece of code, you set some expectations about what the
	 * code should do. KUnit then runs the test and verifies that the code's
	 * behavior matched what was expected.
	 */
	KUNIT_EXPECT_EQ(test, 1, 2); // Obvious failure.
}
#endif


/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int adaptive_mipi_v2_test_init(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = &gInfo;

	KUNIT_ASSERT_EQ(test, sdp_init_adaptive_mipi_v2(info), 0);
	test->priv = info;

	ret_mipi_clk_kbps = -1;
	ret_osc_clk_khz = -1;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void adaptive_mipi_v2_test_exit(struct kunit *test)
{
	struct adaptive_mipi_v2_info *info = test->priv;

	KUNIT_ASSERT_EQ(test, sdp_cleanup_adaptive_mipi_v2(info), 0);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case adaptive_mipi_v2_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#ifdef CONFIG_UML
	/* NOTE: UML TC */
	KUNIT_CASE(adaptive_mipi_v2_test_bar),
	KUNIT_CASE(adaptive_mipi_v2_test_init_func),

	KUNIT_CASE(adaptive_mipi_v2_test_invalid_notifier),
	KUNIT_CASE(adaptive_mipi_v2_test_no_matched_info_at_all),
	KUNIT_CASE(adaptive_mipi_v2_test_no_matched_info_partially),
	KUNIT_CASE(adaptive_mipi_v2_test_all_matched_info),
	KUNIT_CASE(adaptive_mipi_v2_test_same_ratings),
	KUNIT_CASE(adaptive_mipi_v2_test_sinr),

	//KUNIT_CASE(adaptive_mipi_v2_test_no_osc_table),
#else
	/* NOTE: On device TC */
	KUNIT_CASE(adaptive_mipi_v2_test_foo),
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
struct kunit_suite adaptive_mipi_v2_test_module = {
	.name = "adaptive_mipi_v2_test",
	.init = adaptive_mipi_v2_test_init,
	.exit = adaptive_mipi_v2_test_exit,
	.test_cases = adaptive_mipi_v2_test_cases,
};
EXPORT_SYMBOL_KUNIT(adaptive_mipi_v2_test_module);

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&adaptive_mipi_v2_test_module);

MODULE_LICENSE("GPL v2");
