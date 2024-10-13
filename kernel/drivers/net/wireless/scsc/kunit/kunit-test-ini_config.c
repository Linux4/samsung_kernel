// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-mib.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-misc.h"
#include "../ini_config.c"


/* unit test function definition */
static void test_log_failed_psids(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 mib_buffer[10] = {0, };

	log_failed_psids(sdev, mib_buffer, 10);

	mib_buffer[0] = 1;
	log_failed_psids(sdev, mib_buffer, 10);

	log_failed_psids(sdev, NULL, 0);
}

static void test_set_mibs(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	sdev->sig_wait.cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	((struct slsi_skb_cb *)sdev->sig_wait.cfm->cb)->data_length = 1;
	((struct slsi_skb_cb *)sdev->sig_wait.cfm->cb)->sig_length = 1;
	sdev->ini_conf_struct.ini_conf_buff_pos = 1;
	set_mibs(sdev);

	sdev->sig_wait.cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	((struct slsi_skb_cb *)sdev->sig_wait.cfm->cb)->data_length = 1;
	((struct slsi_skb_cb *)sdev->sig_wait.cfm->cb)->sig_length = 0;
	sdev->ini_conf_struct.ini_conf_buff_pos = 1;
	set_mibs(sdev);
}

#define SLSI_INI_CONFIG_BUFF_SIZE	1024
static void test_append_mibdata_to_buffer(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[SLSI_INI_CONFIG_BUFF_SIZE] = {0,};
	u16 pos = 1;
	u8 mib = 1;
	int mib_len = SLSI_INI_CONFIG_BUFF_SIZE -1;
	sdev->ini_conf_struct.ini_conf_buff_pos = 0;

	append_mibdata_to_buffer(sdev, buf, &pos, &mib, mib_len);
}

static void test_encode_int_mib(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 2;
	u8 value[2] = {'0', '\0'};
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	lookup_entry->index = 1;
	lookup_entry->psid = SLSI_PSID_UNIFI_HT_CAPABILITIES;
	lookup_entry->unit_con_factor = 1;

	KUNIT_EXPECT_EQ(test, 0, encode_int_mib(sdev, buf, &pos, value, lookup_entry, true));
	KUNIT_EXPECT_EQ(test, 0, encode_int_mib(sdev, buf, &pos, value, lookup_entry, false));
}

static void test_encode_uint_mib(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 value[3] = {'1', '0', '\0'};
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_uint_mib(sdev, buf, &pos, value, lookup_entry));
}

static void test_encode_sint_mib(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 value[3] = {'2', '0', '\0'};
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_sint_mib(sdev, buf, &pos, value, lookup_entry));
}

static void test_encode_mib_gen_with_two_psids(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	s8 index1 = 0;
	s8 index2 = 0;
	u16 psid1 = SLSI_PSID_UNIFI_TDLS_IN_P2P_ACTIVATED;
	u16 psid2 = SLSI_PSID_UNIFI_TDLS_RSSI_THRESHOLD;
	u32 value_int = 1;

	KUNIT_EXPECT_EQ(test, 0,
			encode_mib_gen_with_two_psids(sdev, buf, &pos, index1, index2, psid1, psid2, value_int));
}

static void test_encode_mib_roamcu_trig(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 value[12] = {0,};
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_roamcu_trig(sdev, buf, &pos, NULL, lookup_entry));

	lookup_entry->unit_con_factor = 1;
	snprintf(value, sizeof(value), "%d", 1234);
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, encode_mib_roamcu_trig(sdev, buf, &pos, value, lookup_entry));
}

static void test_encode_mib_octet_with_2indices(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 value[5] = {'1', '2', '3', '4', '\0'};
	s8 index1 = 0;
	s8 index2 = 8;
	s8 octet_index = -1;
	u16 psid = SLSI_PSID_UNIFI_ROAM_IDLE_VARIATION_RSSI;
	int unit_con_factor = 1;		//slsi_ini_config_lookup_table[]

	KUNIT_EXPECT_EQ(test, 0, encode_mib_octet_with_2indices(sdev, buf, &pos, value,
					index1, index2, octet_index, psid, unit_con_factor));
	KUNIT_EXPECT_EQ(test, 0, encode_mib_octet_with_2indices(sdev, buf, &pos, NULL,
					index1, index2, octet_index, psid, unit_con_factor));
}

static void test_encode_mib_band1_rssi_factor_value1(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-100";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_value1(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_value1(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_value2(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-52";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_value2(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_value2(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_value3(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-53";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_value3(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_value3(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_value4(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-54";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_value4(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_value4(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_value5(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-55";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_value5(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_value5(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_score1(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 value[4] = {'1', '0', '0', '\0'};
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_score1(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_score1(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_score2(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "92";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_score2(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_score2(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_score3(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "93";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_score3(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_score3(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_score4(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "94";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_score4(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_score4(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band1_rssi_factor_score5(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "95";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band1_rssi_factor_score5(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band1_rssi_factor_score5(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_value1(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-91";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_value1(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_value1(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_value2(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-92";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_value2(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_value2(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_value3(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-93";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_value3(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_value3(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_value4(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "-94";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_value4(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_value4(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_value5(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 value[4] = {'-', '9', '5', '\0'};
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_value5(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_value5(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_score1(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "10";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_score1(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_score1(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_score2(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "20";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_score2(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_score2(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_score3(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "30";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_score3(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_score3(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_score4(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 value[3] = {'4', '0', '\0'};
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_score4(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_score4(sdev, buf, &pos, "", lookup_entry));
}

static void test_encode_mib_band2_rssi_factor_score5(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 buf[20] = {0,};
	u16 pos = 0;
	u8 *value = "50";
	struct ini_lookup *lookup_entry = kunit_kzalloc(test, sizeof(struct ini_lookup), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, encode_mib_band2_rssi_factor_score5(sdev, buf, &pos, value, lookup_entry));
	KUNIT_EXPECT_EQ(test, -EINVAL, encode_mib_band2_rssi_factor_score5(sdev, buf, &pos, "", lookup_entry));
}

static void test_slsi_ini_validate_parameter_range(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	int key_val = 0;
	int lookup_table_index = 0;

	KUNIT_EXPECT_EQ(test, 0, slsi_ini_validate_parameter_range(sdev, key_val, lookup_table_index));

	lookup_table_index = 3;		//key = RoamScan_SecondTimer, min_value = 60
	KUNIT_EXPECT_EQ(test, 0, slsi_ini_validate_parameter_range(sdev, key_val, lookup_table_index));
}

static void test_slsi_ini_config_fn_lookup(struct kunit *test)
{
	u8 *key = "RoamCommonTest";
	int len = 14;

	KUNIT_EXPECT_EQ(test, -1, slsi_ini_config_fn_lookup(key, len));
}

static void test_slsi_ini_trim_white_space(struct kunit *test)
{
	u8 *line = " ";
	int len = 1;

	KUNIT_EXPECT_STREQ(test, "", slsi_ini_trim_white_space(line, line, len));
}

static void test_slsi_parse_ini_config(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 *line = "RoamRSSI_Trigger=y";
	int len = 18;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_parse_ini_config(sdev, line, len, true));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_parse_ini_config(sdev, line, len, false));

	line = "RoamRSSI_Triggor=y";
	len = 18;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_parse_ini_config(sdev, line, len, true));

	line = "   \t";
	len = 4;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_parse_ini_config(sdev, line, len, true));
}

static void test_slsi_read_ini_config_file(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct firmware *e = kunit_kzalloc(test, sizeof(struct firmware), GFP_KERNEL);
	e->data = "RoamScan_StepRSSI=5";
	e->size = 19;

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_req, MLME_SET_REQ, SLSI_NET_INDEX_WLAN, 10);

	KUNIT_EXPECT_NE(test, -EINVAL, slsi_read_ini_config_file(sdev, e, true));
	if (sdev->ini_conf_struct.ini_conf_buff) {
		kfree(sdev->ini_conf_struct.ini_conf_buff);
		sdev->ini_conf_struct.ini_conf_buff = NULL;
	}

	e->data = "\n  ";
	e->size = 4;
	sdev->sig_wait.cfm = fapi_alloc(mlme_set_req, MLME_SET_REQ, SLSI_NET_INDEX_WLAN, 10);
	KUNIT_EXPECT_NE(test, 0, slsi_read_ini_config_file(sdev, e, true));
	if (sdev->ini_conf_struct.ini_conf_buff) {
		kfree(sdev->ini_conf_struct.ini_conf_buff);
		sdev->ini_conf_struct.ini_conf_buff = NULL;
	}
}

#define SLSI_INI_TRY_FALLBACK	1
static void test_slsi_load_ini_config_file(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct firmware *e = kunit_kzalloc(test, sizeof(struct firmware), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, SLSI_INI_TRY_FALLBACK, slsi_load_ini_config_file(sdev, INI_CONFIG_FILE_PATH, &e));

	sdev->maxwell_core = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_load_ini_config_file(sdev, INI_CONFIG_FILE_PATH, &e));

	KUNIT_EXPECT_EQ(test, SLSI_INI_TRY_FALLBACK, slsi_load_ini_config_file(sdev, INI_CONFIG_FILE_PATH, NULL));
}

static void test_slsi_process_ini_config_file(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	slsi_process_ini_config_file(sdev);
	if (sdev->ini_conf_struct.ini_conf_buff) {
		kfree(sdev->ini_conf_struct.ini_conf_buff);
		sdev->ini_conf_struct.ini_conf_buff = NULL;
	}
}


/* Test fictures */
static int ini_config_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void ini_config_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case ini_config_test_cases[] = {
	KUNIT_CASE(test_log_failed_psids),
	KUNIT_CASE(test_set_mibs),
	KUNIT_CASE(test_append_mibdata_to_buffer),
	KUNIT_CASE(test_encode_int_mib),
	KUNIT_CASE(test_encode_uint_mib),
	KUNIT_CASE(test_encode_sint_mib),
	KUNIT_CASE(test_encode_mib_gen_with_two_psids),
	KUNIT_CASE(test_encode_mib_roamcu_trig),
	KUNIT_CASE(test_encode_mib_octet_with_2indices),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_value1),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_value2),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_value3),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_value4),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_value5),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_score1),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_score2),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_score3),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_score4),
	KUNIT_CASE(test_encode_mib_band1_rssi_factor_score5),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_value1),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_value2),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_value3),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_value4),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_value5),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_score1),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_score2),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_score3),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_score4),
	KUNIT_CASE(test_encode_mib_band2_rssi_factor_score5),
	KUNIT_CASE(test_slsi_ini_validate_parameter_range),
	KUNIT_CASE(test_slsi_ini_config_fn_lookup),
	KUNIT_CASE(test_slsi_ini_trim_white_space),
	KUNIT_CASE(test_slsi_parse_ini_config),
	KUNIT_CASE(test_slsi_read_ini_config_file),
	KUNIT_CASE(test_slsi_load_ini_config_file),
	KUNIT_CASE(test_slsi_process_ini_config_file),
	{}
};

static struct kunit_suite ini_config_test_suite[] = {
	{
		.name = "ini_config-test",
		.test_cases = ini_config_test_cases,
		.init = ini_config_test_init,
		.exit = ini_config_test_exit,
	}
};

kunit_test_suites(ini_config_test_suite);
