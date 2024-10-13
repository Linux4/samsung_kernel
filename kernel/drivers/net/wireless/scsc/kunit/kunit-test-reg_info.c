// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-misc.h"
#include "../reg_info.c"

#define CHAN2G(_freq, _idx) { \
		.band = NL80211_BAND_2GHZ, \
		.center_freq = (_freq), \
		.hw_value = (_idx), \
		.max_power = 17, \
}

#define CHAN5G(_freq, _idx) { \
		.band = NL80211_BAND_5GHZ, \
		.center_freq = (_freq), \
		.hw_value = (_idx), \
		.max_power = 17, \
}

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
#define CHAN6G(_freq, _idx) { \
		.band = NL80211_BAND_6GHZ, \
		.center_freq = (_freq), \
		.hw_value = (_idx), \
		.max_power = 17, \
}
#endif

static struct ieee80211_channel slsi_2ghz_channels[] = {
	CHAN2G(2412, 1),
	CHAN2G(2417, 2),
	CHAN2G(2422, 3),
	CHAN2G(2427, 4),
	CHAN2G(2432, 5),
	CHAN2G(2437, 6),
	CHAN2G(2442, 7),
	CHAN2G(2447, 8),
	CHAN2G(2452, 9),
	CHAN2G(2457, 10),
	CHAN2G(2462, 11),
	CHAN2G(2467, 12),
	CHAN2G(2472, 13),
	CHAN2G(2484, 14),
};

static struct ieee80211_channel slsi_5ghz_channels[] = {
	/* UNII 1 */
	CHAN5G(5180, 36),
	CHAN5G(5200, 40),
	CHAN5G(5220, 44),
	CHAN5G(5240, 48),
	/* UNII 2a */
	CHAN5G(5260, 52),
	CHAN5G(5280, 56),
	CHAN5G(5300, 60),
	CHAN5G(5320, 64),
	/* UNII 2c */
	CHAN5G(5500, 100),
	CHAN5G(5520, 104),
	CHAN5G(5540, 108),
	CHAN5G(5560, 112),
	CHAN5G(5580, 116),
	CHAN5G(5600, 120),
	CHAN5G(5620, 124),
	CHAN5G(5640, 128),
	CHAN5G(5660, 132),
	CHAN5G(5680, 136),
	CHAN5G(5700, 140),
	CHAN5G(5720, 144),
	/* UNII 3 */
	CHAN5G(5745, 149),
	CHAN5G(5765, 153),
	CHAN5G(5785, 157),
	CHAN5G(5805, 161),
	CHAN5G(5825, 165),
#ifdef CONFIG_SCSC_UNII4
	/* UNII 4 */
	CHAN5G(5845, 169),
	CHAN5G(5865, 173),
	CHAN5G(5885, 177),
#endif
};

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
static struct ieee80211_channel slsi_6ghz_channels[] = {
	/* U-NII-5 */
	CHAN6G(5955, 1),
	CHAN6G(5975, 5),
	CHAN6G(5995, 9),
	CHAN6G(6015, 13),
	CHAN6G(6035, 17),
	CHAN6G(6055, 21),
	CHAN6G(6075, 25),
	CHAN6G(6095, 29),
	CHAN6G(6115, 33),
	CHAN6G(6135, 37),
	CHAN6G(6155, 41),
	CHAN6G(6175, 45),
	CHAN6G(6195, 49),
	CHAN6G(6215, 53),
	CHAN6G(6235, 57),
	CHAN6G(6255, 61),
	CHAN6G(6275, 65),
	CHAN6G(6295, 69),
	CHAN6G(6315, 73),
	CHAN6G(6335, 77),
	CHAN6G(6355, 81),
	CHAN6G(6375, 85),
	CHAN6G(6395, 89),
	CHAN6G(6415, 93),
	/* U-NII-6 */
	CHAN6G(6435, 97),
	CHAN6G(6455, 101),
	CHAN6G(6475, 105),
	CHAN6G(6495, 109),
	CHAN6G(6515, 113),
	/* U-NII-7 */
	CHAN6G(6535, 117),
	CHAN6G(6555, 121),
	CHAN6G(6575, 125),
	CHAN6G(6595, 129),
	CHAN6G(6615, 133),
	CHAN6G(6635, 137),
	CHAN6G(6655, 141),
	CHAN6G(6675, 145),
	CHAN6G(6695, 149),
	CHAN6G(6715, 153),
	CHAN6G(6735, 157),
	CHAN6G(6755, 161),
	CHAN6G(6775, 165),
	CHAN6G(6795, 169),
	CHAN6G(6815, 173),
	CHAN6G(6835, 177),
	CHAN6G(6855, 181),
	/* U-NII-8 */
	CHAN6G(6875, 185),
	CHAN6G(6895, 189),
	CHAN6G(6915, 193),
	CHAN6G(6935, 197),
	CHAN6G(6955, 201),
	CHAN6G(6975, 205),
	CHAN6G(6995, 209),
	CHAN6G(7015, 213),
	CHAN6G(7035, 217),
	CHAN6G(7055, 221),
	CHAN6G(7075, 225),
	CHAN6G(7095, 229),
	CHAN6G(7115, 233),
};
#endif

struct slsi_test_bh_work {
	bool available;
	struct slsi_test_dev *uftestdev;
	struct workqueue_struct *workqueue;
	struct work_struct work;
	struct slsi_spinlock spinlock;
};

struct slsi_test_data_route {
	bool configured;
	u16 test_device_minor_number; /* index into slsi_test_devices[] */
	u8 mac[ETH_ALEN];
	u16 vif;
	u8 ipsubnet;
	u16 sequence_number;
};

struct slsi_test_dev {
	/* This is used for:
	 * 1) The uf6kunittesthip<n> chardevice number
	 * 2) The uf6kunittest<n> chardevice number
	 * 3) The /procf/devices/unifi<n> number
	 */
	int device_minor_number;

	void *uf_cdev;
	struct device *dev;
	struct slsi_dev *sdev;

	struct workqueue_struct *attach_detach_work_queue;
	/* a std mutex */
	struct mutex attach_detach_mutex;
	struct work_struct attach_work;
	struct work_struct detach_work;
	bool attached;

	u8 hw_addr[ETH_ALEN];
	struct slsi_test_bh_work bh_work;

	/* a std spinlock */
	spinlock_t route_spinlock;
	struct slsi_test_data_route route[SLSI_AP_PEER_CONNECTIONS_MAX];
};



/* unit test function definition */
static void test_slsi_regd_init(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
								  sizeof(struct ieee80211_reg_rule) * 10, GFP_KERNEL);

	slsi_regd_init(sdev);
}

static void test_slsi_regd_init_wiphy_not_registered(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	int i, j, k;

	sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
								  sizeof(struct ieee80211_reg_rule) * 10, GFP_KERNEL);
	for (i = 0; i < NUM_NL80211_BANDS; i++) {
		if (i == NL80211_BAND_2GHZ) {
			sdev->wiphy->bands[i] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band) +
							      sizeof(struct ieee80211_channel) *
							      ARRAY_SIZE(slsi_2ghz_channels),
							      GFP_KERNEL);
			sdev->wiphy->bands[i]->channels = slsi_2ghz_channels;
			sdev->wiphy->bands[i]->n_channels = ARRAY_SIZE(slsi_2ghz_channels);

		} else if (i == NL80211_BAND_5GHZ) {
			sdev->wiphy->bands[i] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band) +
							      sizeof(struct ieee80211_channel) *
							      ARRAY_SIZE(slsi_5ghz_channels),
							      GFP_KERNEL);
			sdev->wiphy->bands[i]->channels = slsi_5ghz_channels;
			sdev->wiphy->bands[i]->n_channels = ARRAY_SIZE(slsi_5ghz_channels);

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
		} else if (i == NL80211_BAND_6GHZ) {
			sdev->wiphy->bands[i] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
			sdev->wiphy->bands[i]->channels = slsi_6ghz_channels;
			sdev->wiphy->bands[i]->n_channels = ARRAY_SIZE(slsi_6ghz_channels);
#endif
		} else {
			sdev->wiphy->bands[i] = NULL;
		}
	}

	slsi_regd_init_wiphy_not_registered(sdev);

	if (sdev && sdev->wiphy && sdev->wiphy->regd)
		kfree(sdev->wiphy->regd);
}

static void test_slsi_regd_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_regd_deinit(sdev);
}

static void test_slsi_read_regulatory(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	sdev->maxwell_core = 0;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = -1;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 1;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 3;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 5;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 7;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 9;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 11;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 13;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 15;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 17;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 19;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 21;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 23;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 25;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 27;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 29;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 31;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_read_regulatory(sdev));

	sdev->maxwell_core = 33;
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	KUNIT_EXPECT_EQ(test, 0, slsi_read_regulatory(sdev));

	if (sdev->regdb.country)
		kfree(sdev->regdb.country);

	if (sdev->regdb.rules_collection)
		kfree(sdev->regdb.rules_collection);

	if (sdev->regdb.reg_rules)
		kfree(sdev->regdb.reg_rules);

	if (sdev->regdb.freq_ranges)
		kfree(sdev->regdb.freq_ranges);

	sdev->regdb.regdb_state = SLSI_REG_DB_SET;
	KUNIT_EXPECT_EQ(test, 0, slsi_read_regulatory(sdev));
}

/* Test fictures */
static int reg_info_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void reg_info_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case reg_info_test_cases[] = {
	KUNIT_CASE(test_slsi_regd_init),
	KUNIT_CASE(test_slsi_regd_init_wiphy_not_registered),
	KUNIT_CASE(test_slsi_regd_deinit),
	KUNIT_CASE(test_slsi_read_regulatory),
	{}
};

static struct kunit_suite reg_info_test_suite[] = {
	{
		.name = "reg_info-test",
		.test_cases = reg_info_test_cases,
		.init = reg_info_test_init,
		.exit = reg_info_test_exit,
	}
};

kunit_test_suites(reg_info_test_suite);
