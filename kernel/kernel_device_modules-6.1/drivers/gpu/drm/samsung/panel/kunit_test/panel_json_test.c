// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>

#include "panel.h"
#include "panel_obj.h"
#include "panel_debug.h"
#include "panel_drv.h"
#include "panel_function.h"
#include "panel_property.h"
#include "ezop/panel_json.h"
#include "ezop/json_writer.h"
#include "ezop/json_reader.h"
#include "ezop/json.h"

struct panel_json_test {
	/* for reader */
	json_reader_t *r;
	struct list_head *pnobj_list;
	/* for writer */
	json_writer_t *w;
	char *buf;
	struct list_head prop_list;
};

extern void jsonw_pnobj(json_writer_t *w, struct pnobj *pnobj);
extern int jsonr_byte_array_field(json_reader_t *r, const char *prop, u8 **out);
extern void jsonw_byte_array_field(json_writer_t *w, const char *prop, u8 *arr, size_t size);

static int init_common_table(struct maptbl *tbl) { return -EINVAL; }
static int getidx_dsc_table(struct maptbl *tbl) { return 0; }
static int getidx_common_table(struct maptbl *tbl) { return 0; }
static void copy_common_maptbl(struct maptbl *tbl, unsigned char *dst) {}
static bool condition_true(struct panel_device *panel) { return true; }
static bool condition_false(struct panel_device *panel) { return false; }
static int show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res;
	u8 rddpm[PANEL_RDDPM_LEN] = { 0, };

	if (!info)
		return -EINVAL;

	res = info->res;
	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(rddpm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddpm[0], ((rddpm[0] & 0x9C) == 0x9C) ? "GOOD" : "NG");
	panel_info("* Bootster Mode : %s\n", rddpm[0] & 0x80 ? "ON (GD)" : "OFF (NG)");
	panel_info("* Idle Mode     : %s\n", rddpm[0] & 0x40 ? "ON (NG)" : "OFF (GD)");
	panel_info("* Partial Mode  : %s\n", rddpm[0] & 0x20 ? "ON" : "OFF");
	panel_info("* Sleep Mode    : %s\n", rddpm[0] & 0x10 ? "OUT (GD)" : "IN (NG)");
	panel_info("* Normal Mode   : %s\n", rddpm[0] & 0x08 ? "OK (GD)" : "SLEEP (NG)");
	panel_info("* Display ON    : %s\n", rddpm[0] & 0x04 ? "ON (GD)" : "OFF (NG)");
	panel_info("=================================================\n");

	return 0;
}

static DEFINE_PNOBJ_FUNC(init_common_table, init_common_table);
static DEFINE_PNOBJ_FUNC(getidx_dsc_table, getidx_dsc_table);
static DEFINE_PNOBJ_FUNC(getidx_common_table, getidx_common_table);
static DEFINE_PNOBJ_FUNC(copy_common_maptbl, copy_common_maptbl);
static DEFINE_PNOBJ_FUNC(condition_true, condition_true);
static DEFINE_PNOBJ_FUNC(condition_false, condition_false);
static DEFINE_PNOBJ_FUNC(show_rddpm, show_rddpm);

struct panel_prop_enum_item vrr_mode_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_NORMAL_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_HS_MODE),
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

/* NOTE: UML TC */
static void panel_json_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_json_test_panel_json_parse_pnobj(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"pnobj\":{\"type\":\"MAPTBL\",\"name\":\"rainbow_g0_dsc_table\"}";
	struct pnobj *pnobj = kzalloc(sizeof(*pnobj), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_pnobj(ctx->r, pnobj), 0);

	jsonw_pnobj(ctx->w, pnobj);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(pnobj);
}

static void panel_json_test_jsonr_byte_array(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"arr\":[0,1]";
	u8 *arr = NULL;

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_byte_array_field(ctx->r, "arr", &arr), 2);
	jsonw_byte_array_field(ctx->w, "arr", arr, 2);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kvfree(arr);
}

static void panel_json_test_jsonr_byte_array_rle(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js1 = "\"arr1\":[[255],[5]]";
	char *js1_exp = "\"arr1\":[255,255,255,255,255]";
	char *js2 = "\"arr2\":[[255],[128]]";
	char *js2_exp = "\"arr1\":[255,255,255,255,255],\"arr2\":[[255],[128]]";
	u8 *arr = NULL;

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js1), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_byte_array_field(ctx->r, "arr1", &arr), 5);
	jsonw_byte_array_field(ctx->w, "arr1", arr, 5);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js1_exp);
	kvfree(arr);

	/* len > 128BYTE */
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js2), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_byte_array_field(ctx->r, "arr2", &arr), 128);
	jsonw_byte_array_field(ctx->w, "arr2", arr, 128);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js2_exp);
	kvfree(arr);
}

static void panel_json_test_jsonr_maptbl(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"rainbow_g0_dsc_table\":{\"pnobj\":{\"type\":\"MAPTBL\",\"name\":\"rainbow_g0_dsc_table\"},\"shape\":{\"nr_dimen\":2,\"sz_dimen\":[1,2]},\"arr\":[0,1],\"ops\":{\"init\":{\"$ref\":\"#/FUNCTION/init_common_table\"},\"getidx\":{\"$ref\":\"#/FUNCTION/getidx_dsc_table\"},\"copy\":{\"$ref\":\"#/FUNCTION/copy_common_maptbl\"}},\"props\":[]}";
	struct maptbl *m = kzalloc(sizeof(*m), GFP_KERNEL);

	pnobj_function_list_add(&PNOBJ_FUNC(init_common_table), ctx->pnobj_list);
	pnobj_function_list_add(&PNOBJ_FUNC(getidx_common_table), ctx->pnobj_list);
	pnobj_function_list_add(&PNOBJ_FUNC(getidx_dsc_table), ctx->pnobj_list);
	pnobj_function_list_add(&PNOBJ_FUNC(copy_common_maptbl), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_maptbl(ctx->r, m), 0);
	KUNIT_ASSERT_EQ(test, jsonw_maptbl(ctx->w, m), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	maptbl_destroy(m);
}

static void panel_json_test_jsonr_static_packet(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"rainbow_g0_sleep_out\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_sleep_out\"},\"type\":\"DSI_WR\",\"data\":[17],\"offset\":0,\"pktui\":[],\"option\":0}";
	struct pktinfo *pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_tx_packet(ctx->r, pkt), 0);
	KUNIT_EXPECT_PTR_EQ(test, get_pktinfo_initdata(pkt), get_pktinfo_txbuf(pkt));
	KUNIT_ASSERT_EQ(test, jsonw_tx_packet(ctx->w, pkt), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	destroy_tx_packet(pkt);
}

static void panel_json_test_jsonr_packet_update_info(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "{\"offset\":3,\"maptbl\":{\"$ref\":\"#/MAPTBL/rainbow_g0_te_frame_sel_table\"},\"getidx\":{}}";
	DEFINE_MAPTBL_SHAPE(shape_2d_2x1, 2, 1);
	struct maptbl_ops ops = { .init = &PNOBJ_FUNC(init_common_table), .getidx = &PNOBJ_FUNC(getidx_dsc_table), .copy = &PNOBJ_FUNC(copy_common_maptbl), };
	static u8 rainbow_g0_dsc_table_init_data[][1] = { { 0x00 }, { 0x01 }, };
	struct maptbl *m = maptbl_create("rainbow_g0_te_frame_sel_table", &shape_2d_2x1, &ops, NULL, rainbow_g0_dsc_table_init_data, NULL);
	struct pkt_update_info *pktui = kzalloc(sizeof(*pktui), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&m->base), ctx->pnobj_list);
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_packet_update_info(ctx->r, pktui), 0);

	maptbl_destroy(m);
	kfree(pktui);
}

static void panel_json_test_jsonr_variable_packet(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"rainbow_g0_te_frame_sel\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_te_frame_sel\"},\"type\":\"DSI_WR\",\"data\":[185,113,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,5,250,0,48],\"offset\":2,\"pktui\":[{\"offset\":3,\"maptbl\":{\"$ref\":\"#/MAPTBL/rainbow_g0_te_frame_sel_table\"},\"getidx\":{}},{\"offset\":17,\"maptbl\":{\"$ref\":\"#/MAPTBL/rainbow_g0_te_timing_table\"},\"getidx\":{}}],\"option\":0}";
	DEFINE_MAPTBL_SHAPE(shape_2d_2x1, 2, 1);
	struct maptbl_ops ops = { .init = &PNOBJ_FUNC(init_common_table), .getidx = &PNOBJ_FUNC(getidx_dsc_table), .copy = &PNOBJ_FUNC(copy_common_maptbl), };
	u8 rainbow_g0_te_frame_sel_init_data[][1] = { { 0xCC }, { 0xDD }, };
	struct maptbl *te_frame_sel_maptbl = maptbl_create("rainbow_g0_te_frame_sel_table",
			&shape_2d_2x1, &ops, NULL, rainbow_g0_te_frame_sel_init_data, NULL);
	u8 rainbow_g0_te_timing_init_data[][1] = { { 0xEE }, { 0xFF }, };
	struct maptbl *te_timing_maptbl = maptbl_create("rainbow_g0_te_timing_table",
			&shape_2d_2x1, &ops, NULL, rainbow_g0_te_timing_init_data, NULL);
	struct pktinfo *pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&te_frame_sel_maptbl->base), ctx->pnobj_list);
	list_add_tail(get_pnobj_list(&te_timing_maptbl->base), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_tx_packet(ctx->r, pkt), 0);
	KUNIT_EXPECT_PTR_NE(test, get_pktinfo_initdata(pkt), get_pktinfo_txbuf(pkt));
	KUNIT_EXPECT_TRUE(test, !memcmp(get_pktinfo_initdata(pkt), get_pktinfo_txbuf(pkt), pkt->dlen));
	KUNIT_ASSERT_EQ(test, jsonw_tx_packet(ctx->w, pkt), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	destroy_tx_packet(pkt);
}

static void panel_json_test_jsonr_keyinfo(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 RAINBOW_G0_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
	DEFINE_STATIC_PACKET(rainbow_g0_level1_key_enable, DSI_PKT_TYPE_WR, RAINBOW_G0_KEY1_ENABLE, 0);
	char *js = "\"rainbow_g0_level1_key_enable\":{\"pnobj\":{\"type\":\"KEY\",\"name\":\"rainbow_g0_level1_key_enable\"},\"level\":1,\"en\":2,\"packet\":{\"$ref\":\"#/TX_PACKET/rainbow_g0_level1_key_enable\"}}";
	struct keyinfo *key = kzalloc(sizeof(*key), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&PKTINFO(rainbow_g0_level1_key_enable).base), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_keyinfo(ctx->r, key), 0);
	KUNIT_ASSERT_EQ(test, jsonw_keyinfo(ctx->w, key), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(get_pnobj_name(&key->base));
	kfree(key);
}

static void panel_json_test_jsonr_condition_if(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	struct condinfo *cond = kzalloc(sizeof(*cond), GFP_KERNEL);
	char *js = "\"test_cond_is_true\":{"
		"\"pnobj\":{\"type\":\"COND_IF\",\"name\":\"test_cond_is_true\"},"
		"\"rule\":{"
			"\"item\":["
				"{\"type\":\"FUNC\",\"op\":{\"$ref\":\"#/FUNCTION/condition_true\"}}"
			"]"
		"}}";

	pnobj_function_list_add(&PNOBJ_FUNC(condition_true), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_condition(ctx->r, cond), 0);
	KUNIT_ASSERT_EQ(test, jsonw_condition(ctx->w, cond), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(cond->rule.item);
	kfree(get_pnobj_name(&cond->base));
	kfree(cond);
}

static void panel_json_test_jsonr_condition_el(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"panel_cond_else\":{"
		"\"pnobj\":{\"type\":\"COND_EL\",\"name\":\"panel_cond_else\"},"
		"\"rule\":{"
			"\"item\":[]"
		"}}";
	struct condinfo *cond = kzalloc(sizeof(*cond), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_condition(ctx->r, cond), 0);
	KUNIT_ASSERT_EQ(test, jsonw_condition(ctx->w, cond), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(cond->rule.item);
	kfree(get_pnobj_name(&cond->base));
	kfree(cond);
}

static void panel_json_test_jsonr_condition_fi(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"panel_cond_endif\":{"
		"\"pnobj\":{\"type\":\"COND_FI\",\"name\":\"panel_cond_endif\"},"
		"\"rule\":{"
			"\"item\":[]"
		"}}";
	struct condinfo *cond = kzalloc(sizeof(*cond), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_condition(ctx->r, cond), 0);
	KUNIT_ASSERT_EQ(test, jsonw_condition(ctx->w, cond), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(cond->rule.item);
	kfree(get_pnobj_name(&cond->base));
	kfree(cond);
}

static void panel_json_test_jsonr_rx_packet(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"date\":{\"pnobj\":{\"type\":\"RX_PACKET\",\"name\":\"date\"},\"type\":\"DSI_RD\",\"addr\":161,\"offset\":4,\"len\":7}";
	struct rdinfo *rdi = kzalloc(sizeof(*rdi), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_rx_packet(ctx->r, rdi), 0);
	KUNIT_ASSERT_EQ(test, jsonw_rx_packet(ctx->w, rdi), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	destroy_rx_packet(rdi);
}

static void panel_json_test_jsonr_resource_with_mutable_resource(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"date\":{\"pnobj\":{\"type\":\"RES\",\"name\":\"date\"},\"data\":[1,2,3,4,5,6,7],\"resui\":[{\"offset\":0,\"rdi\":{\"$ref\":\"#/RX_PACKET/date\"}}]}";
	/* mutable resource data will be set by 0 */
	char *expected_js = "\"date\":{\"pnobj\":{\"type\":\"RES\",\"name\":\"date\"},\"data\":[0,0,0,0,0,0,0],\"resui\":[{\"offset\":0,\"rdi\":{\"$ref\":\"#/RX_PACKET/date\"}}]}";
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	struct resinfo *res = kzalloc(sizeof(*res), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&RDINFO(date).base), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_resource(ctx->r, res), 0);
	KUNIT_ASSERT_EQ(test, jsonw_resource(ctx->w, res), 0);
	KUNIT_EXPECT_TRUE(test, is_resource_mutable(res));
	KUNIT_EXPECT_STREQ(test, ctx->buf, expected_js);

	destroy_resource(res);
}

static void panel_json_test_jsonr_resource_with_immutable_resource(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"date\":{\"pnobj\":{\"type\":\"RES\",\"name\":\"date\"},\"data\":[1,2,3,4,5,6,7],\"resui\":[]}";
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	struct resinfo *res = kzalloc(sizeof(*res), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&RDINFO(date).base), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_resource(ctx->r, res), 0);
	KUNIT_ASSERT_EQ(test, jsonw_resource(ctx->w, res), 0);
	KUNIT_EXPECT_FALSE(test, is_resource_mutable(res));
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	destroy_resource(res);
}

static void panel_json_test_jsonr_dumpinfo(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"rddpm\":{\"pnobj\":{\"type\":\"DUMP\",\"name\":\"rddpm\"},\"res\":{\"$ref\":\"#/RES/rddpm\"},\"callback\":{\"$ref\":\"#/FUNCTION/show_rddpm\"},\"expects\":[{\"offset\":0,\"mask\":128,\"value\":128,\"msg\":\"Booster Mode : OFF(NG)\"},{\"offset\":0,\"mask\":64,\"value\":0,\"msg\":\"Idle Mode : ON(NG)\"},{\"offset\":0,\"mask\":16,\"value\":16,\"msg\":\"Sleep Mode : IN(NG)\"},{\"offset\":0,\"mask\":8,\"value\":8,\"msg\":\"Normal Mode : SLEEP(NG)\"},{\"offset\":0,\"mask\":4,\"value\":4,\"msg\":\"Display Mode : OFF(NG)\"}]}";
	u8 S6E3HAE_RDDPM[PANEL_RDDPM_LEN];
	DEFINE_RDINFO(rddpm, DSI_PKT_TYPE_RD, 0x0A, 0, PANEL_RDDPM_LEN);
	DEFINE_RESUI(rddpm, &RDINFO(rddpm), 0);
	DEFINE_RESOURCE(rddpm, S6E3HAE_RDDPM, RESUI(rddpm));
	struct dumpinfo *dump = kzalloc(sizeof(*dump), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&RESINFO(rddpm).base), ctx->pnobj_list);
	pnobj_function_list_add(&PNOBJ_FUNC(show_rddpm), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_dumpinfo(ctx->r, dump), 0);
	KUNIT_ASSERT_EQ(test, jsonw_dumpinfo(ctx->w, dump), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	destroy_dumpinfo(dump);
}

static void panel_json_test_jsonr_delayinfo(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"rainbow_g0_wait_sleep_out_104msec\":{\"pnobj\":{\"type\":\"TIMER_DELAY\",\"name\":\"rainbow_g0_wait_sleep_out_104msec\"},\"usec\":104000,\"nframe\":0,\"nvsync\":0}";
	struct delayinfo *delay = kzalloc(sizeof(*delay), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_delayinfo(ctx->r, delay), 0);
	KUNIT_ASSERT_EQ(test, jsonw_delayinfo(ctx->w, delay), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(get_pnobj_name(&delay->base));
	kfree(delay);
}

static void panel_json_test_jsonr_timer_delay_begin(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"rainbow_g0_wait_sleep_out_104msec\":{\"pnobj\":{\"type\":\"TIMER_DELAY_BEGIN\",\"name\":\"rainbow_g0_wait_sleep_out_104msec\"},\"delay\":{\"$ref\":\"#/TIMER_DELAY/rainbow_g0_wait_sleep_out_104msec\"}}";
	DEFINE_PANEL_TIMER_MDELAY(rainbow_g0_wait_sleep_out_104msec, 104);
	struct timer_delay_begin_info *tdbi = kzalloc(sizeof(*tdbi), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&DLYINFO(rainbow_g0_wait_sleep_out_104msec).base), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_timer_delay_begin_info(ctx->r, tdbi), 0);
	KUNIT_ASSERT_EQ(test, jsonw_timer_delay_begin_info(ctx->w, tdbi), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(get_pnobj_name(&tdbi->base));
	kfree(tdbi);
}

static void panel_json_test_jsonr_power_ctrl(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"fast_discharge_enable\":{\"pnobj\":{\"type\":\"PWRCTRL\",\"name\":\"fast_discharge_enable\"},\"key\":\"panel_fd_enable\"}";
	struct pwrctrl *pwrctrl = kzalloc(sizeof(*pwrctrl), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_power_ctrl(ctx->r, pwrctrl), 0);
	KUNIT_ASSERT_EQ(test, jsonw_power_ctrl(ctx->w, pwrctrl), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(get_pnobj_name(&pwrctrl->base));
	kfree(pwrctrl->key);
	kfree(pwrctrl);
}

static void panel_json_test_jsonr_property_range_type(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"panel_refresh_rate\":{\"pnobj\":{\"type\":\"PROPERTY\",\"name\":\"panel_refresh_rate\"},\"type\":\"RANGE\",\"min\":0,\"max\":120}";
	struct panel_property *prop = kzalloc(sizeof(*prop), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_property(ctx->r, prop), 0);
	KUNIT_ASSERT_EQ(test, jsonw_property(ctx->w, prop), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	panel_property_destroy(prop);
}

static void panel_json_test_jsonr_property_enum_type(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"panel_refresh_mode\":{\"pnobj\":{\"type\":\"PROPERTY\",\"name\":\"panel_refresh_mode\"},\"type\":\"ENUM\",\"enums\":{\"VRR_NORMAL_MODE\":0,\"VRR_HS_MODE\":1}}";
	struct panel_property *prop = kzalloc(sizeof(*prop), GFP_KERNEL);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_property(ctx->r, prop), 0);
	KUNIT_ASSERT_EQ(test, jsonw_property(ctx->w, prop), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	panel_property_destroy(prop);
}

static void panel_json_test_jsonr_config(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	struct panel_prop_enum_item wait_tx_done_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_AUTO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_ON),
	};
	struct panel_property *panel_wait_tx_done_property =
		panel_property_create_enum(PANEL_PROPERTY_WAIT_TX_DONE,
				WAIT_TX_DONE_AUTO, wait_tx_done_enum_items,
				ARRAY_SIZE(wait_tx_done_enum_items));
	char *js = "\"set_wait_tx_done_property_off\":{\"pnobj\":{\"type\":\"CONFIG\",\"name\":\"set_wait_tx_done_property_off\"},\"prop\":{\"$ref\":\"#/PROPERTY/wait_tx_done\"},\"value\":1}";
	struct pnobj_config *config = kzalloc(sizeof(*config), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&panel_wait_tx_done_property->base), ctx->pnobj_list);
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_config(ctx->r, config), 0);
	KUNIT_ASSERT_EQ(test, jsonw_config(ctx->w, config), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	kfree(get_pnobj_name(&config->base));
	kfree(config);
	panel_property_destroy(panel_wait_tx_done_property);
}

static void panel_json_test_jsonr_sequence(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_FUNC_BASED_COND(test_cond_is_true, &PNOBJ_FUNC(condition_true));
	u8 RAINBOW_G0_SLEEP_OUT[] = { 0x11 };
	DEFINE_STATIC_PACKET(rainbow_g0_sleep_out, DSI_PKT_TYPE_WR, RAINBOW_G0_SLEEP_OUT, 0);
	u8 RAINBOW_G0_DISPLAY_ON[] = { 0x29 };
	DEFINE_STATIC_PACKET(rainbow_g0_display_on, DSI_PKT_TYPE_WR, RAINBOW_G0_DISPLAY_ON, 0);
	char *js = "\"rainbow_g0_test_seq\":{\"pnobj\":{\"type\":\"SEQ\",\"name\":\"rainbow_g0_test_seq\"},\"cmdtbl\":"
			"[{\"$ref\":\"#/COND_IF/test_cond_is_true\"},"
			"{\"$ref\":\"#/TX_PACKET/rainbow_g0_sleep_out\"},"
			"{\"$ref\":\"#/TX_PACKET/rainbow_g0_display_on\"},"
			"{\"$ref\":\"#/COND_FI/panel_cond_endif\"}]}";
	struct seqinfo *seq = kzalloc(sizeof(*seq), GFP_KERNEL);

	list_add_tail(get_pnobj_list(&CONDINFO_IF(test_cond_is_true).base), ctx->pnobj_list);
	list_add_tail(get_pnobj_list(&CONDINFO_EL(_).base), ctx->pnobj_list);
	list_add_tail(get_pnobj_list(&CONDINFO_FI(_).base), ctx->pnobj_list);
	list_add_tail(get_pnobj_list(&PKTINFO(rainbow_g0_sleep_out).base), ctx->pnobj_list);
	list_add_tail(get_pnobj_list(&PKTINFO(rainbow_g0_display_on).base), ctx->pnobj_list);

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_sequence(ctx->r, seq), 0);
	KUNIT_ASSERT_EQ(test, jsonw_sequence(ctx->w, seq), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	destroy_sequence(seq);
}

static void panel_json_test_jsonr_function(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"condition_true\":{\"pnobj\":{\"type\":\"FUNCTION\",\"name\":\"condition_true\"}}";
	struct pnobj_func *pnobj_func = kzalloc(sizeof(*pnobj_func), GFP_KERNEL);

	panel_function_insert(&PNOBJ_FUNC(condition_true));

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_function(ctx->r, pnobj_func), 0);
	KUNIT_ASSERT_EQ(test, jsonw_function(ctx->w, pnobj_func), 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);

	destroy_pnobj_function(pnobj_func);
}

static void panel_json_test_jsonr_all(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	struct pnobj *pos, *next;
	char *js = "{"
		"\"PROPERTY\":{},"
		"\"FUNCTION\":{},"
		"\"MAPTBL\":{},"
		"\"DELAY\":{},"
		"\"COND_IF\":{},"
		"\"COND_EL\":{},"
		"\"COND_FI\":{},"
		"\"PWRCTRL\":{},"
		"\"RX_PACKET\":{},"
		"\"CONFIG\":{},"
		"\"TX_PACKET\":{},"
		"\"KEY\":{},"
		"\"RES\":{},"
		"\"DUMP\":{},"
		"\"SEQ\":{}}";

	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_ASSERT_EQ(test, jsonr_all(ctx->r), 0);

	list_for_each_entry_safe(pos, next, get_jsonr_pnobj_list(ctx->r), list)
		destroy_panel_object(pos);
}

static void panel_json_test_jsonw_byte_array_field(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 *arr1 = kunit_kmalloc(test, 5, GFP_KERNEL);
	u8 *arr2 = kunit_kmalloc(test, 128, GFP_KERNEL);

	memset(arr1, 10, 5);
	memset(arr2, 255, 128);

	jsonw_byte_array_field(ctx->w, "arr1", arr1, 5);
	jsonw_byte_array_field(ctx->w, "arr2", arr2, 128);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"arr1\":[10,10,10,10,10],\"arr2\":[[255],[128]]");
}

static void panel_json_test_jsonw_byte_array_field_with_null_array(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 *arr1 = kunit_kmalloc(test, 5, GFP_KERNEL);
	u8 *arr2 = kunit_kmalloc(test, 128, GFP_KERNEL);

	memset(arr1, 10, 5);
	memset(arr2, 255, 128);

	jsonw_byte_array_field(ctx->w, "arr1", NULL, 5);
	jsonw_byte_array_field(ctx->w, "arr2", NULL, 128);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"arr1\":[0,0,0,0,0],\"arr2\":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]");
}

static void panel_json_test_jsonw_maptbl(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_MAPTBL_SHAPE(shape_2d_2x1, 2, 1);
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(init_common_table),
		.getidx = &PNOBJ_FUNC(getidx_dsc_table),
		.copy = &PNOBJ_FUNC(copy_common_maptbl),
	};
	static u8 rainbow_g0_dsc_table_init_data[][1] = {
		{ 0x00 },
		{ 0x01 },
	};
	struct maptbl *m = maptbl_create("rainbow_g0_dsc_table", &shape_2d_2x1, &ops, NULL, rainbow_g0_dsc_table_init_data, NULL);

	KUNIT_EXPECT_TRUE(test, jsonw_maptbl(ctx->w, m) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"rainbow_g0_dsc_table\":{\"pnobj\":{\"type\":\"MAPTBL\",\"name\":\"rainbow_g0_dsc_table\"},\"shape\":{\"nr_dimen\":2,\"sz_dimen\":[1,2]},\"arr\":[0,1],\"ops\":{\"init\":{\"$ref\":\"#/FUNCTION/init_common_table\"},\"getidx\":{\"$ref\":\"#/FUNCTION/getidx_dsc_table\"},\"copy\":{\"$ref\":\"#/FUNCTION/copy_common_maptbl\"}},\"props\":[]}");

	maptbl_destroy(m);
}

static void panel_json_test_jsonw_static_packet(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 RAINBOW_G0_SLEEP_OUT[] = { 0x11 };
	DEFINE_STATIC_PACKET(rainbow_g0_sleep_out, DSI_PKT_TYPE_WR, RAINBOW_G0_SLEEP_OUT, 0);

	KUNIT_EXPECT_TRUE(test, jsonw_tx_packet(ctx->w, &PKTINFO(rainbow_g0_sleep_out)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"rainbow_g0_sleep_out\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_sleep_out\"},\"type\":\"DSI_WR\",\"data\":[17],\"offset\":0,\"pktui\":[],\"option\":0}");
}

static void panel_json_test_jsonw_variable_packet(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_MAPTBL_SHAPE(shape_2d_2x1, 2, 1);
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(init_common_table),
		.getidx = &PNOBJ_FUNC(getidx_common_table),
		.copy = &PNOBJ_FUNC(copy_common_maptbl),
	};
	u8 rainbow_g0_te_frame_sel_init_data[][1] = { { 0xCC }, { 0xDD }, };
	struct maptbl *te_frame_sel_maptbl = maptbl_create("rainbow_g0_te_frame_sel_table",
			&shape_2d_2x1, &ops, NULL, rainbow_g0_te_frame_sel_init_data, NULL);
	u8 rainbow_g0_te_timing_init_data[][1] = { { 0xEE }, { 0xFF }, };
	struct maptbl *te_timing_maptbl = maptbl_create("rainbow_g0_te_timing_table",
			&shape_2d_2x1, &ops, NULL, rainbow_g0_te_timing_init_data, NULL);
	u8 RAINBOW_G0_TE_FRAME_SEL[] = {
		0xB9,
		0x71, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x05, 0xFA, 0x00, 0x30
	};
	DECLARE_PKTUI(rainbow_g0_te_frame_sel) = {
		{ .offset = 3, .maptbl = te_frame_sel_maptbl },
		{ .offset = 17, .maptbl = te_timing_maptbl },
	};
	DEFINE_VARIABLE_PACKET(rainbow_g0_te_frame_sel, DSI_PKT_TYPE_WR, RAINBOW_G0_TE_FRAME_SEL, 2);

	KUNIT_EXPECT_TRUE(test, jsonw_tx_packet(ctx->w, &PKTINFO(rainbow_g0_te_frame_sel)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"rainbow_g0_te_frame_sel\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_te_frame_sel\"},\"type\":\"DSI_WR\",\"data\":[185,113,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,5,250,0,48],\"offset\":2,\"pktui\":[{\"offset\":3,\"maptbl\":{\"$ref\":\"#/MAPTBL/rainbow_g0_te_frame_sel_table\"},\"getidx\":{}},{\"offset\":17,\"maptbl\":{\"$ref\":\"#/MAPTBL/rainbow_g0_te_timing_table\"},\"getidx\":{}}],\"option\":0}");
}

static void panel_json_test_jsonw_keyinfo(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 RAINBOW_G0_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
	DEFINE_STATIC_PACKET(rainbow_g0_level1_key_enable, DSI_PKT_TYPE_WR, RAINBOW_G0_KEY1_ENABLE, 0);
	DEFINE_PANEL_KEY(rainbow_g0_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(rainbow_g0_level1_key_enable));

	KUNIT_EXPECT_TRUE(test, jsonw_keyinfo(ctx->w, &KEYINFO(rainbow_g0_level1_key_enable)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"rainbow_g0_level1_key_enable\":{\"pnobj\":{\"type\":\"KEY\",\"name\":\"rainbow_g0_level1_key_enable\"},\"level\":1,\"en\":2,\"packet\":{\"$ref\":\"#/TX_PACKET/rainbow_g0_level1_key_enable\"}}");
}

static void panel_json_test_jsonw_function_based_condition_if(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_FUNC_BASED_COND(test_cond_is_true, &PNOBJ_FUNC(condition_true));
	char *js = "\"test_cond_is_true\":{"
		"\"pnobj\":{\"type\":\"COND_IF\",\"name\":\"test_cond_is_true\"},"
		"\"rule\":{"
			"\"item\":["
				"{\"type\":\"FUNC\",\"op\":{\"$ref\":\"#/FUNCTION/condition_true\"}}"
			"]"
		"}}";

	KUNIT_EXPECT_TRUE(test, jsonw_condition(ctx->w, &CONDINFO_IF(test_cond_is_true)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);
};

static void panel_json_test_jsonw_rule_based_condition_if(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_RULE_BASED_COND(test_cond_is_120hz, PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 120);
	char *js = "\"test_cond_is_120hz\":{"
		"\"pnobj\":{\"type\":\"COND_IF\",\"name\":\"test_cond_is_120hz\"},"
		"\"rule\":{"
			"\"item\":["
				"{\"type\":\"FROM\"},"
				"{\"type\":\"PROP\",\"op\":\"panel_refresh_rate\"},"
				"{\"type\":\"BIT_AND\"},"
				"{\"type\":\"U32\",\"op\":4294967295},"
				"{\"type\":\"TO\"},"
				"{\"type\":\"EQ\"},"
				"{\"type\":\"U32\",\"op\":120}"
			"]"
		"}}";

	KUNIT_EXPECT_TRUE(test, jsonw_condition(ctx->w, &CONDINFO_IF(test_cond_is_120hz)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);
};

static void panel_json_test_jsonw_condition_el(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"panel_cond_else\":{"
		"\"pnobj\":{\"type\":\"COND_EL\",\"name\":\"panel_cond_else\"},"
		"\"rule\":{"
			"\"item\":[]"
		"}}";

	KUNIT_EXPECT_TRUE(test, jsonw_condition(ctx->w, &CONDINFO_EL(_)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);
}

static void panel_json_test_jsonw_condition_fi(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	char *js = "\"panel_cond_endif\":{"
		"\"pnobj\":{\"type\":\"COND_FI\",\"name\":\"panel_cond_endif\"},"
		"\"rule\":{"
			"\"item\":[]"
		"}}";

	KUNIT_EXPECT_TRUE(test, jsonw_condition(ctx->w, &CONDINFO_FI(_)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, js);
}

static void panel_json_test_jsonw_rx_packet(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);

	KUNIT_EXPECT_TRUE(test, jsonw_rx_packet(ctx->w, &RDINFO(date)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"date\":{\"pnobj\":{\"type\":\"RX_PACKET\",\"name\":\"date\"},\"type\":\"DSI_RD\",\"addr\":161,\"offset\":4,\"len\":7}");
}

static void panel_json_test_jsonw_resource(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 S6E3HAE_DATE[7] = {1, 2, 3, 4, 5, 6, 7};
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	DEFINE_RESUI(date, &RDINFO(date), 0);
	DEFINE_RESOURCE(date, S6E3HAE_DATE, RESUI(date));

	KUNIT_EXPECT_TRUE(test, jsonw_resource(ctx->w, &RESINFO(date)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"date\":{\"pnobj\":{\"type\":\"RES\",\"name\":\"date\"},\"data\":[0,0,0,0,0,0,0],\"resui\":[{\"offset\":0,\"rdi\":{\"$ref\":\"#/RX_PACKET/date\"}}]}");
}

static void panel_json_test_jsonw_dumpinfo(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 S6E3HAE_RDDPM[PANEL_RDDPM_LEN];
	DEFINE_RDINFO(rddpm, DSI_PKT_TYPE_RD, 0x0A, 0, PANEL_RDDPM_LEN);
	DEFINE_RESUI(rddpm, &RDINFO(rddpm), 0);
	DEFINE_RESOURCE(rddpm, S6E3HAE_RDDPM, RESUI(rddpm));
	struct dump_ops ops = { .show = &PNOBJ_FUNC(show_rddpm) };
	struct dump_expect rddpm_after_display_on_expects[] = {
		{ .offset = 0, .mask = 0x80, .value = 0x80, .msg = "Booster Mode : OFF(NG)" },
		{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Idle Mode : ON(NG)" },
		{ .offset = 0, .mask = 0x10, .value = 0x10, .msg = "Sleep Mode : IN(NG)" },
		{ .offset = 0, .mask = 0x08, .value = 0x08, .msg = "Normal Mode : SLEEP(NG)" },
		{ .offset = 0, .mask = 0x04, .value = 0x04, .msg = "Display Mode : OFF(NG)" },
	};
	struct dumpinfo *dump_rddpm = create_dumpinfo("rddpm", &RESINFO(rddpm), &ops,
			rddpm_after_display_on_expects, ARRAY_SIZE(rddpm_after_display_on_expects));

	KUNIT_EXPECT_TRUE(test, jsonw_dumpinfo(ctx->w, dump_rddpm) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"rddpm\":{\"pnobj\":{\"type\":\"DUMP\",\"name\":\"rddpm\"},\"res\":{\"$ref\":\"#/RES/rddpm\"},\"callback\":{\"$ref\":\"#/FUNCTION/show_rddpm\"},\"expects\":[{\"offset\":0,\"mask\":128,\"value\":128,\"msg\":\"Booster Mode : OFF(NG)\"},{\"offset\":0,\"mask\":64,\"value\":0,\"msg\":\"Idle Mode : ON(NG)\"},{\"offset\":0,\"mask\":16,\"value\":16,\"msg\":\"Sleep Mode : IN(NG)\"},{\"offset\":0,\"mask\":8,\"value\":8,\"msg\":\"Normal Mode : SLEEP(NG)\"},{\"offset\":0,\"mask\":4,\"value\":4,\"msg\":\"Display Mode : OFF(NG)\"}]}");

	destroy_dumpinfo(dump_rddpm);
}

static void panel_json_test_jsonw_delayinfo(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_PANEL_TIMER_MDELAY(rainbow_g0_wait_sleep_out_104msec, 104);

	KUNIT_EXPECT_TRUE(test, jsonw_delayinfo(ctx->w, &DLYINFO(rainbow_g0_wait_sleep_out_104msec)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"rainbow_g0_wait_sleep_out_104msec\":{\"pnobj\":{\"type\":\"TIMER_DELAY\",\"name\":\"rainbow_g0_wait_sleep_out_104msec\"},\"usec\":104000,\"nframe\":0,\"nvsync\":0}");
}

static void panel_json_test_jsonw_timer_delay_begin(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_PANEL_TIMER_MDELAY(rainbow_g0_wait_sleep_out_104msec, 104);
	DEFINE_PANEL_TIMER_BEGIN(rainbow_g0_wait_sleep_out_104msec,
			TIMER_DLYINFO(&rainbow_g0_wait_sleep_out_104msec));

	KUNIT_EXPECT_TRUE(test, jsonw_timer_delay_begin_info(ctx->w, &TIMER_DLYINFO_BEGIN(rainbow_g0_wait_sleep_out_104msec)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"rainbow_g0_wait_sleep_out_104msec\":{\"pnobj\":{\"type\":\"TIMER_DELAY_BEGIN\",\"name\":\"rainbow_g0_wait_sleep_out_104msec\"},\"delay\":{\"$ref\":\"#/TIMER_DELAY/rainbow_g0_wait_sleep_out_104msec\"}}");
}

static void panel_json_test_jsonw_power_ctrl(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_PCTRL(fast_discharge_enable, "panel_fd_enable");

	KUNIT_EXPECT_TRUE(test, jsonw_power_ctrl(ctx->w, &PWRCTRL(fast_discharge_enable)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"fast_discharge_enable\":{\"pnobj\":{\"type\":\"PWRCTRL\",\"name\":\"fast_discharge_enable\"},\"key\":\"panel_fd_enable\"}");
}

static void panel_json_test_jsonw_property_range_type(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	struct panel_property *panel_refresh_rate_property =
		panel_property_create_range(PANEL_PROPERTY_PANEL_REFRESH_RATE, 60, 0, 120);

	KUNIT_EXPECT_TRUE(test, jsonw_property(ctx->w, panel_refresh_rate_property) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"panel_refresh_rate\":{\"pnobj\":{\"type\":\"PROPERTY\",\"name\":\"panel_refresh_rate\"},\"type\":\"RANGE\",\"min\":0,\"max\":120}");
	panel_property_destroy(panel_refresh_rate_property);
}

static void panel_json_test_jsonw_property_enum_type(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	struct panel_property *panel_refresh_mode_property =
		panel_property_create_enum(PANEL_PROPERTY_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items, ARRAY_SIZE(vrr_mode_enum_items));

	KUNIT_EXPECT_TRUE(test, jsonw_property(ctx->w, panel_refresh_mode_property) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"panel_refresh_mode\":{\"pnobj\":{\"type\":\"PROPERTY\",\"name\":\"panel_refresh_mode\"},\"type\":\"ENUM\",\"enums\":{\"VRR_NORMAL_MODE\":0,\"VRR_HS_MODE\":1}}");
	panel_property_destroy(panel_refresh_mode_property);
}

static void panel_json_test_jsonw_config(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_PNOBJ_CONFIG(set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);

	KUNIT_EXPECT_TRUE(test, jsonw_config(ctx->w, &PNOBJ_CONFIG(set_wait_tx_done_property_off)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"set_wait_tx_done_property_off\":{\"pnobj\":{\"type\":\"CONFIG\",\"name\":\"set_wait_tx_done_property_off\"},\"prop\":{\"$ref\":\"#/PROPERTY/wait_tx_done\"},\"value\":1}");
}

static void panel_json_test_jsonw_sequence(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_FUNC_BASED_COND(test_cond_is_true, &PNOBJ_FUNC(condition_true));
	u8 RAINBOW_G0_SLEEP_OUT[] = { 0x11 };
	DEFINE_STATIC_PACKET(rainbow_g0_sleep_out, DSI_PKT_TYPE_WR, RAINBOW_G0_SLEEP_OUT, 0);
	u8 RAINBOW_G0_DISPLAY_ON[] = { 0x29 };
	DEFINE_STATIC_PACKET(rainbow_g0_display_on, DSI_PKT_TYPE_WR, RAINBOW_G0_DISPLAY_ON, 0);
	void *rainbow_g0_test_cmdtbl[] = {
		&CONDINFO_IF(test_cond_is_true),
		&PKTINFO(rainbow_g0_sleep_out),
		&PKTINFO(rainbow_g0_display_on),
		&CONDINFO_FI(_),
	};
	DEFINE_SEQINFO(rainbow_g0_test_seq, rainbow_g0_test_cmdtbl);

	KUNIT_EXPECT_TRUE(test, jsonw_sequence(ctx->w, &SEQINFO(rainbow_g0_test_seq)) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"rainbow_g0_test_seq\":{\"pnobj\":{\"type\":\"SEQ\",\"name\":\"rainbow_g0_test_seq\"},\"cmdtbl\":"
			"[{\"$ref\":\"#/COND_IF/test_cond_is_true\"},"
			"{\"$ref\":\"#/TX_PACKET/rainbow_g0_sleep_out\"},"
			"{\"$ref\":\"#/TX_PACKET/rainbow_g0_display_on\"},"
			"{\"$ref\":\"#/COND_FI/panel_cond_endif\"}]}");
}

static void panel_json_test_jsonw_maptbl_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_MAPTBL_SHAPE(shape_2d_2x1, 2, 1);
	struct maptbl_ops ops = {
		.init = &PNOBJ_FUNC(init_common_table),
		.getidx = &PNOBJ_FUNC(getidx_dsc_table),
		.copy = &PNOBJ_FUNC(copy_common_maptbl),
	};
	static u8 test_table1_init_data[][1] = { { 0x00 }, { 0x01 }, };
	static u8 test_table2_init_data[][1] = { { 0x02 }, { 0x03 }, };
	struct maptbl *m1 = maptbl_create("test_table1", &shape_2d_2x1, &ops, NULL, test_table1_init_data, NULL);
	struct maptbl *m2 = maptbl_create("test_table2", &shape_2d_2x1, &ops, NULL, test_table2_init_data, NULL);
	struct list_head maptbl_list;

	INIT_LIST_HEAD(&maptbl_list);
	list_add_tail(maptbl_get_list(m1), &maptbl_list);
	list_add_tail(maptbl_get_list(m2), &maptbl_list);
	KUNIT_EXPECT_TRUE(test, jsonw_maptbl_list(ctx->w, &maptbl_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"MAPTBL\":{\"test_table1\":{\"pnobj\":{\"type\":\"MAPTBL\",\"name\":\"test_table1\"},\"shape\":{\"nr_dimen\":2,\"sz_dimen\":[1,2]},\"arr\":[0,1],\"ops\":{\"init\":{\"$ref\":\"#/FUNCTION/init_common_table\"},\"getidx\":{\"$ref\":\"#/FUNCTION/getidx_dsc_table\"},\"copy\":{\"$ref\":\"#/FUNCTION/copy_common_maptbl\"}},\"props\":[]},\"test_table2\":{\"pnobj\":{\"type\":\"MAPTBL\",\"name\":\"test_table2\"},\"shape\":{\"nr_dimen\":2,\"sz_dimen\":[1,2]},\"arr\":[2,3],\"ops\":{\"init\":{\"$ref\":\"#/FUNCTION/init_common_table\"},\"getidx\":{\"$ref\":\"#/FUNCTION/getidx_dsc_table\"},\"copy\":{\"$ref\":\"#/FUNCTION/copy_common_maptbl\"}},\"props\":[]}}");
}

static void panel_json_test_jsonw_rx_packet_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	DEFINE_RDINFO(id, DSI_PKT_TYPE_RD, 0x04, 0, 3);
	struct list_head rdi_list;

	INIT_LIST_HEAD(&rdi_list);
	list_add_tail(&RDINFO(date).base.list, &rdi_list);
	list_add_tail(&RDINFO(id).base.list, &rdi_list);

	KUNIT_EXPECT_TRUE(test, jsonw_rx_packet_list(ctx->w, &rdi_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf,
			"\"RX_PACKET\":{"
			"\"date\":{\"pnobj\":{\"type\":\"RX_PACKET\",\"name\":\"date\"},\"type\":\"DSI_RD\",\"addr\":161,\"offset\":4,\"len\":7},"
			"\"id\":{\"pnobj\":{\"type\":\"RX_PACKET\",\"name\":\"id\"},\"type\":\"DSI_RD\",\"addr\":4,\"offset\":0,\"len\":3}}");
}

static void panel_json_test_jsonw_resource_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 S6E3HAE_DATE[7] = {1, 2, 3, 4, 5, 6, 7};
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	DEFINE_RESUI(date, &RDINFO(date), 0);
	DEFINE_RESOURCE(date1, S6E3HAE_DATE, RESUI(date));
	DEFINE_IMMUTABLE_RESOURCE(date2, S6E3HAE_DATE);
	struct list_head resource_list;

	INIT_LIST_HEAD(&resource_list);
	list_add_tail(&RESINFO(date1).base.list, &resource_list);
	list_add_tail(&RESINFO(date2).base.list, &resource_list);

	KUNIT_EXPECT_TRUE(test, jsonw_resource_list(ctx->w, &resource_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"RES\":{\"date1\":{\"pnobj\":{\"type\":\"RES\",\"name\":\"date1\"},\"data\":[0,0,0,0,0,0,0],\"resui\":[{\"offset\":0,\"rdi\":{\"$ref\":\"#/RX_PACKET/date\"}}]},\"date2\":{\"pnobj\":{\"type\":\"RES\",\"name\":\"date2\"},\"data\":[1,2,3,4,5,6,7],\"resui\":[]}}");
}

static void panel_json_test_jsonw_dumpinfo_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 S6E3HAE_RDDPM[PANEL_RDDPM_LEN];
	DEFINE_RDINFO(rddpm, DSI_PKT_TYPE_RD, 0x0A, 0, PANEL_RDDPM_LEN);
	DEFINE_RESUI(rddpm, &RDINFO(rddpm), 0);
	DEFINE_RESOURCE(rddpm, S6E3HAE_RDDPM, RESUI(rddpm));
	struct dump_ops ops = { .show = &PNOBJ_FUNC(show_rddpm) };
	struct dump_expect rddpm_after_display_on_expects[] = {
		{ .offset = 0, .mask = 0x80, .value = 0x80, .msg = "Booster Mode : OFF(NG)" },
		{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Idle Mode : ON(NG)" },
		{ .offset = 0, .mask = 0x10, .value = 0x10, .msg = "Sleep Mode : IN(NG)" },
		{ .offset = 0, .mask = 0x08, .value = 0x08, .msg = "Normal Mode : SLEEP(NG)" },
		{ .offset = 0, .mask = 0x04, .value = 0x04, .msg = "Display Mode : OFF(NG)" },
	};
	struct dumpinfo *dump_rddpm1 = create_dumpinfo("rddpm1", &RESINFO(rddpm), &ops,
			rddpm_after_display_on_expects, ARRAY_SIZE(rddpm_after_display_on_expects));
	struct dumpinfo *dump_rddpm2 = create_dumpinfo("rddpm2", &RESINFO(rddpm), &ops, NULL, 0);
	struct list_head dump_list;

	INIT_LIST_HEAD(&dump_list);
	list_add_tail(get_pnobj_list(&dump_rddpm1->base), &dump_list);
	list_add_tail(get_pnobj_list(&dump_rddpm2->base), &dump_list);
	KUNIT_EXPECT_TRUE(test, jsonw_dumpinfo_list(ctx->w, &dump_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf,
			"\"DUMP\":{"
			"\"rddpm1\":{\"pnobj\":{\"type\":\"DUMP\",\"name\":\"rddpm1\"},\"res\":{\"$ref\":\"#/RES/rddpm\"},\"callback\":{\"$ref\":\"#/FUNCTION/show_rddpm\"},\"expects\":[{\"offset\":0,\"mask\":128,\"value\":128,\"msg\":\"Booster Mode : OFF(NG)\"},{\"offset\":0,\"mask\":64,\"value\":0,\"msg\":\"Idle Mode : ON(NG)\"},{\"offset\":0,\"mask\":16,\"value\":16,\"msg\":\"Sleep Mode : IN(NG)\"},{\"offset\":0,\"mask\":8,\"value\":8,\"msg\":\"Normal Mode : SLEEP(NG)\"},{\"offset\":0,\"mask\":4,\"value\":4,\"msg\":\"Display Mode : OFF(NG)\"}]},"
			"\"rddpm2\":{\"pnobj\":{\"type\":\"DUMP\",\"name\":\"rddpm2\"},\"res\":{\"$ref\":\"#/RES/rddpm\"},\"callback\":{\"$ref\":\"#/FUNCTION/show_rddpm\"},\"expects\":[]}}");


	destroy_dumpinfo(dump_rddpm1);
	destroy_dumpinfo(dump_rddpm2);
}

static void panel_json_test_jsonw_power_ctrl_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_PCTRL(fast_discharge_enable, "panel_fd_enable");
	DEFINE_PCTRL(fast_discharge_disable, "panel_fd_disable");
	struct list_head pwr_ctrl_list;

	INIT_LIST_HEAD(&pwr_ctrl_list);
	list_add_tail(&PWRCTRL(fast_discharge_enable).base.list, &pwr_ctrl_list);
	list_add_tail(&PWRCTRL(fast_discharge_disable).base.list, &pwr_ctrl_list);
	KUNIT_EXPECT_TRUE(test, jsonw_power_ctrl_list(ctx->w, &pwr_ctrl_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"PWRCTRL\":{\"fast_discharge_enable\":{\"pnobj\":{\"type\":\"PWRCTRL\",\"name\":\"fast_discharge_enable\"},\"key\":\"panel_fd_enable\"},\"fast_discharge_disable\":{\"pnobj\":{\"type\":\"PWRCTRL\",\"name\":\"fast_discharge_disable\"},\"key\":\"panel_fd_disable\"}}");
}

static void panel_json_test_jsonw_property_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	struct panel_property *panel_refresh_rate_property =
		panel_property_create_range(PANEL_PROPERTY_PANEL_REFRESH_RATE, 60, 0, 120);
	struct panel_property *panel_refresh_mode_property =
		panel_property_create_enum(PANEL_PROPERTY_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items, ARRAY_SIZE(vrr_mode_enum_items));
	struct list_head prop_list;

	INIT_LIST_HEAD(&prop_list);
	list_add_tail(get_pnobj_list(&panel_refresh_rate_property->base), &prop_list);
	list_add_tail(get_pnobj_list(&panel_refresh_mode_property->base), &prop_list);
	KUNIT_EXPECT_TRUE(test, jsonw_property_list(ctx->w, &prop_list) == 0);

	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"PROPERTY\":{"
		"\"panel_refresh_rate\":{\"pnobj\":{\"type\":\"PROPERTY\",\"name\":\"panel_refresh_rate\"},\"type\":\"RANGE\",\"min\":0,\"max\":120},"
		"\"panel_refresh_mode\":{\"pnobj\":{\"type\":\"PROPERTY\",\"name\":\"panel_refresh_mode\"},\"type\":\"ENUM\",\"enums\":{\"VRR_NORMAL_MODE\":0,\"VRR_HS_MODE\":1}}}");

	panel_property_destroy(panel_refresh_rate_property);
	panel_property_destroy(panel_refresh_mode_property);
}

static void panel_json_test_jsonw_config_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_PNOBJ_CONFIG(set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
	DEFINE_PNOBJ_CONFIG(set_wait_tx_done_property_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);
	struct list_head cfg_list;

	INIT_LIST_HEAD(&cfg_list);
	list_add_tail(&PNOBJ_CONFIG(set_wait_tx_done_property_off).base.list, &cfg_list);
	list_add_tail(&PNOBJ_CONFIG(set_wait_tx_done_property_auto).base.list, &cfg_list);
	KUNIT_EXPECT_TRUE(test, jsonw_config_list(ctx->w, &cfg_list) == 0);

	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"CONFIG\":{"
			"\"set_wait_tx_done_property_off\":{\"pnobj\":{\"type\":\"CONFIG\",\"name\":\"set_wait_tx_done_property_off\"},\"prop\":{\"$ref\":\"#/PROPERTY/wait_tx_done\"},\"value\":1},"
			"\"set_wait_tx_done_property_auto\":{\"pnobj\":{\"type\":\"CONFIG\",\"name\":\"set_wait_tx_done_property_auto\"},\"prop\":{\"$ref\":\"#/PROPERTY/wait_tx_done\"},\"value\":0}}");
}

static void panel_json_test_jsonw_sequence_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_FUNC_BASED_COND(test_cond_is_true, &PNOBJ_FUNC(condition_true));
	u8 RAINBOW_G0_SLEEP_OUT[] = { 0x11 };
	DEFINE_STATIC_PACKET(rainbow_g0_sleep_out, DSI_PKT_TYPE_WR, RAINBOW_G0_SLEEP_OUT, 0);
	u8 RAINBOW_G0_DISPLAY_ON[] = { 0x29 };
	DEFINE_STATIC_PACKET(rainbow_g0_display_on, DSI_PKT_TYPE_WR, RAINBOW_G0_DISPLAY_ON, 0);
	void *rainbow_g0_test_cmdtbl1[] = {
		&CONDINFO_IF(test_cond_is_true),
		&PKTINFO(rainbow_g0_sleep_out),
		&PKTINFO(rainbow_g0_display_on),
		&CONDINFO_FI(_),
	};
	void *rainbow_g0_test_cmdtbl2[] = {
		&CONDINFO_IF(test_cond_is_true),
		&PKTINFO(rainbow_g0_sleep_out),
		&PKTINFO(rainbow_g0_display_on),
		&CONDINFO_FI(_),
	};
	DEFINE_SEQINFO(rainbow_g0_test_seq1, rainbow_g0_test_cmdtbl1);
	DEFINE_SEQINFO(rainbow_g0_test_seq2, rainbow_g0_test_cmdtbl2);
	struct list_head seq_list;

	INIT_LIST_HEAD(&seq_list);
	list_add_tail(&SEQINFO(rainbow_g0_test_seq1).base.list, &seq_list);
	list_add_tail(&SEQINFO(rainbow_g0_test_seq2).base.list, &seq_list);

	KUNIT_EXPECT_TRUE(test, jsonw_sequence_list(ctx->w, &seq_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"SEQ\":{"
			"\"rainbow_g0_test_seq1\":{\"pnobj\":{\"type\":\"SEQ\",\"name\":\"rainbow_g0_test_seq1\"},\"cmdtbl\":"
			"[{\"$ref\":\"#/COND_IF/test_cond_is_true\"},"
			"{\"$ref\":\"#/TX_PACKET/rainbow_g0_sleep_out\"},"
			"{\"$ref\":\"#/TX_PACKET/rainbow_g0_display_on\"},"
			"{\"$ref\":\"#/COND_FI/panel_cond_endif\"}]},"
			"\"rainbow_g0_test_seq2\":{\"pnobj\":{\"type\":\"SEQ\",\"name\":\"rainbow_g0_test_seq2\"},\"cmdtbl\":"
			"[{\"$ref\":\"#/COND_IF/test_cond_is_true\"},"
			"{\"$ref\":\"#/TX_PACKET/rainbow_g0_sleep_out\"},"
			"{\"$ref\":\"#/TX_PACKET/rainbow_g0_display_on\"},"
			"{\"$ref\":\"#/COND_FI/panel_cond_endif\"}]}}");
}

static void panel_json_test_jsonw_key_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 RAINBOW_G0_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
	u8 RAINBOW_G0_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
	DEFINE_STATIC_PACKET(rainbow_g0_level1_key_enable, DSI_PKT_TYPE_WR, RAINBOW_G0_KEY1_ENABLE, 0);
	DEFINE_STATIC_PACKET(rainbow_g0_level2_key_enable, DSI_PKT_TYPE_WR, RAINBOW_G0_KEY2_ENABLE, 0);
	DEFINE_PANEL_KEY(rainbow_g0_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(rainbow_g0_level1_key_enable));
	DEFINE_PANEL_KEY(rainbow_g0_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(rainbow_g0_level2_key_enable));
	struct list_head key_list;

	INIT_LIST_HEAD(&key_list);
	list_add_tail(&KEYINFO(rainbow_g0_level1_key_enable).base.list, &key_list);
	list_add_tail(&KEYINFO(rainbow_g0_level2_key_enable).base.list, &key_list);

	KUNIT_EXPECT_TRUE(test, jsonw_key_list(ctx->w, &key_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf, "\"KEY\":{"
			"\"rainbow_g0_level1_key_enable\":{\"pnobj\":{\"type\":\"KEY\",\"name\":\"rainbow_g0_level1_key_enable\"},\"level\":1,\"en\":2,\"packet\":{\"$ref\":\"#/TX_PACKET/rainbow_g0_level1_key_enable\"}},"
			"\"rainbow_g0_level2_key_enable\":{\"pnobj\":{\"type\":\"KEY\",\"name\":\"rainbow_g0_level2_key_enable\"},\"level\":2,\"en\":2,\"packet\":{\"$ref\":\"#/TX_PACKET/rainbow_g0_level2_key_enable\"}}}");
}

static void panel_json_test_jsonw_tx_packet_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	u8 RAINBOW_G0_SLEEP_OUT[] = { 0x11 };
	DEFINE_STATIC_PACKET(rainbow_g0_sleep_out, DSI_PKT_TYPE_WR, RAINBOW_G0_SLEEP_OUT, 0);
	u8 RAINBOW_G0_DISPLAY_ON[] = { 0x29 };
	DEFINE_STATIC_PACKET(rainbow_g0_display_on, DSI_PKT_TYPE_WR, RAINBOW_G0_DISPLAY_ON, 0);
	u8 RAINBOW_G0_DSC[] = { 0x01 };
	DEFINE_STATIC_PACKET(rainbow_g0_dsc, DSI_PKT_TYPE_WR_COMP, RAINBOW_G0_DSC, 0);
	u8 RAINBOW_G0_PPS[] = { 0x11, 0x00 };
	DEFINE_STATIC_PACKET(rainbow_g0_pps, DSI_PKT_TYPE_WR_PPS, RAINBOW_G0_PPS, 0);
	u8 RAINBOW_G0_ANALOG_GAMMA_DATA[] = { 0x01, 0x02 };
	DEFINE_STATIC_PACKET_WITH_OPTION(rainbow_g0_analog_gamma_data, DSI_PKT_TYPE_WR_SR,
			RAINBOW_G0_ANALOG_GAMMA_DATA, 0, PKT_OPTION_SR_ALIGN_4 | PKT_OPTION_SR_MAX_1024 | PKT_OPTION_SR_REG_6C);
	u8 S6E3FAC_MAFPC_DEFAULT_IMG[] = { 0x01, 0x02 };
	DEFINE_STATIC_PACKET_WITH_OPTION(rainbow_g0_mafpc_default_img, DSI_PKT_TYPE_WR_SR_FAST,
			S6E3FAC_MAFPC_DEFAULT_IMG, 0, PKT_OPTION_SR_ALIGN_12);
	struct list_head pkt_list;

	INIT_LIST_HEAD(&pkt_list);
	list_add_tail(&PKTINFO(rainbow_g0_sleep_out).base.list, &pkt_list);
	list_add_tail(&PKTINFO(rainbow_g0_display_on).base.list, &pkt_list);
	list_add_tail(&PKTINFO(rainbow_g0_dsc).base.list, &pkt_list);
	list_add_tail(&PKTINFO(rainbow_g0_pps).base.list, &pkt_list);
	list_add_tail(&PKTINFO(rainbow_g0_analog_gamma_data).base.list, &pkt_list);
	list_add_tail(&PKTINFO(rainbow_g0_mafpc_default_img).base.list, &pkt_list);

	KUNIT_EXPECT_TRUE(test, jsonw_tx_packet_list(ctx->w, &pkt_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf,
			"\"TX_PACKET\":{"
			"\"rainbow_g0_sleep_out\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_sleep_out\"},"
			"\"type\":\"DSI_WR\",\"data\":[17],\"offset\":0,\"pktui\":[],\"option\":0},"
			"\"rainbow_g0_display_on\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_display_on\"},"
			"\"type\":\"DSI_WR\",\"data\":[41],\"offset\":0,\"pktui\":[],\"option\":0},"
			"\"rainbow_g0_dsc\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_dsc\"},"
			"\"type\":\"DSI_WR_DSC\",\"data\":[1],\"offset\":0,\"pktui\":[],\"option\":0},"
			"\"rainbow_g0_pps\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_pps\"},"
			"\"type\":\"DSI_WR_PPS\",\"data\":[17,0],\"offset\":0,\"pktui\":[],\"option\":0},"
			"\"rainbow_g0_analog_gamma_data\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_analog_gamma_data\"},"
			"\"type\":\"DSI_WR_SR\",\"data\":[1,2],\"offset\":0,\"pktui\":[],\"option\":82},"
			"\"rainbow_g0_mafpc_default_img\":{\"pnobj\":{\"type\":\"TX_PACKET\",\"name\":\"rainbow_g0_mafpc_default_img\"},"
			"\"type\":\"DSI_WR_SR_FAST\",\"data\":[1,2],\"offset\":0,\"pktui\":[],\"option\":4}}");
}

static void panel_json_test_jsonw_delay_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_PANEL_MDELAY(rainbow_g0_wait_sleep_out_120msec, 120);
	DEFINE_PANEL_VSYNC_DELAY(rainbow_g0_wait_1_vsync, 1);
	DEFINE_PANEL_TIMER_MDELAY(rainbow_g0_wait_sleep_out_104msec, 104);
	DEFINE_PANEL_TIMER_BEGIN(rainbow_g0_wait_sleep_out_104msec,
			TIMER_DLYINFO(&rainbow_g0_wait_sleep_out_104msec));
	struct list_head dly_list;

	INIT_LIST_HEAD(&dly_list);
	list_add_tail(&DLYINFO(rainbow_g0_wait_sleep_out_120msec).base.list, &dly_list);
	list_add_tail(&DLYINFO(rainbow_g0_wait_1_vsync).base.list, &dly_list);
	list_add_tail(&TIMER_DLYINFO(rainbow_g0_wait_sleep_out_104msec).base.list, &dly_list);
	list_add_tail(&TIMER_DLYINFO_BEGIN(rainbow_g0_wait_sleep_out_104msec).base.list, &dly_list);

	KUNIT_EXPECT_TRUE(test, jsonw_delay_list(ctx->w, &dly_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf,
			"\"DELAY\":{"
			"\"rainbow_g0_wait_sleep_out_120msec\":{\"pnobj\":{\"type\":\"DELAY\",\"name\":\"rainbow_g0_wait_sleep_out_120msec\"},"
			"\"usec\":120000,\"nframe\":0,\"nvsync\":0},"
			"\"rainbow_g0_wait_1_vsync\":{\"pnobj\":{\"type\":\"DELAY\",\"name\":\"rainbow_g0_wait_1_vsync\"},"
			"\"usec\":0,\"nframe\":0,\"nvsync\":1}},"
			"\"TIMER_DELAY\":{"
			"\"rainbow_g0_wait_sleep_out_104msec\":{\"pnobj\":{\"type\":\"TIMER_DELAY\",\"name\":\"rainbow_g0_wait_sleep_out_104msec\"},"
			"\"usec\":104000,\"nframe\":0,\"nvsync\":0}},"
			"\"TIMER_DELAY_BEGIN\":{"
			"\"rainbow_g0_wait_sleep_out_104msec\":{\"pnobj\":{\"type\":\"TIMER_DELAY_BEGIN\",\"name\":\"rainbow_g0_wait_sleep_out_104msec\"},"
			"\"delay\":{\"$ref\":\"#/TIMER_DELAY/rainbow_g0_wait_sleep_out_104msec\"}}}");
}

static void panel_json_test_jsonw_condition_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	DEFINE_FUNC_BASED_COND(test_cond_is_true, &PNOBJ_FUNC(condition_true));
	DEFINE_RULE_BASED_COND(test_cond_is_120hz, PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 120);
	struct list_head cond_list;

	INIT_LIST_HEAD(&cond_list);
	list_add_tail(&CONDINFO_IF(test_cond_is_true).base.list, &cond_list);
	list_add_tail(&CONDINFO_IF(test_cond_is_120hz).base.list, &cond_list);
	list_add_tail(&CONDINFO_EL(_).base.list, &cond_list);
	list_add_tail(&CONDINFO_FI(_).base.list, &cond_list);

	KUNIT_EXPECT_TRUE(test, jsonw_condition_list(ctx->w, &cond_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf,
			"\"COND_IF\":{"
			"\"test_cond_is_true\":{\"pnobj\":{\"type\":\"COND_IF\",\"name\":\"test_cond_is_true\"},"
			"\"rule\":{\"item\":[{\"type\":\"FUNC\",\"op\":{\"$ref\":\"#/FUNCTION/condition_true\"}}]}},"
			"\"test_cond_is_120hz\":{\"pnobj\":{\"type\":\"COND_IF\",\"name\":\"test_cond_is_120hz\"},"
			"\"rule\":{\"item\":["
				"{\"type\":\"FROM\"},"
				"{\"type\":\"PROP\",\"op\":\"panel_refresh_rate\"},"
				"{\"type\":\"BIT_AND\"},"
				"{\"type\":\"U32\",\"op\":4294967295},"
				"{\"type\":\"TO\"},"
				"{\"type\":\"EQ\"},"
				"{\"type\":\"U32\",\"op\":120}"
			"]}}},"
			"\"COND_EL\":{"
			"\"panel_cond_else\":{\"pnobj\":{\"type\":\"COND_EL\",\"name\":\"panel_cond_else\"},"
			"\"rule\":{\"item\":[]}}},"
			"\"COND_FI\":{"
			"\"panel_cond_endif\":{\"pnobj\":{\"type\":\"COND_FI\",\"name\":\"panel_cond_endif\"},"
			"\"rule\":{\"item\":[]}}}");
}

static void panel_json_test_jsonw_function_list(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;
	struct list_head func_list;

	INIT_LIST_HEAD(&func_list);

	list_add_tail(get_pnobj_list(&(PNOBJ_FUNC(condition_true).base)), &func_list);
	list_add_tail(get_pnobj_list(&(PNOBJ_FUNC(condition_false).base)), &func_list);

	KUNIT_EXPECT_TRUE(test, jsonw_function_list(ctx->w, &func_list) == 0);
	KUNIT_EXPECT_STREQ(test, ctx->buf,
			"\"FUNCTION\":{"
			"\"condition_true\":{\"pnobj\":{\"type\":\"FUNCTION\",\"name\":\"condition_true\"}},"
			"\"condition_false\":{\"pnobj\":{\"type\":\"FUNCTION\",\"name\":\"condition_false\"}}}");
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_json_test_init(struct kunit *test)
{
	struct panel_json_test *ctx =
		kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	//panel_function_init();
	ctx->pnobj_list =
		kunit_kzalloc(test, sizeof(*ctx->pnobj_list), GFP_KERNEL);
	INIT_LIST_HEAD(ctx->pnobj_list);
	INIT_LIST_HEAD(&ctx->prop_list);
	ctx->r = jsonr_new(SZ_32K, ctx->pnobj_list);

	ctx->buf = kunit_kzalloc(test, SZ_32K, GFP_KERNEL);
	ctx->w = jsonw_new(ctx->buf, SZ_32K);

	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_json_test_exit(struct kunit *test)
{
	struct panel_json_test *ctx = test->priv;

	jsonw_destroy(&ctx->w);
	jsonr_destroy(&ctx->r);
	//panel_function_deinit();
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_json_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_json_test_test),
	KUNIT_CASE(panel_json_test_panel_json_parse_pnobj),
	KUNIT_CASE(panel_json_test_jsonr_byte_array),
	KUNIT_CASE(panel_json_test_jsonr_byte_array_rle),
	KUNIT_CASE(panel_json_test_jsonr_maptbl),
	KUNIT_CASE(panel_json_test_jsonr_static_packet),
	KUNIT_CASE(panel_json_test_jsonr_packet_update_info),
	KUNIT_CASE(panel_json_test_jsonr_variable_packet),
	KUNIT_CASE(panel_json_test_jsonr_keyinfo),
	KUNIT_CASE(panel_json_test_jsonr_condition_if),
	KUNIT_CASE(panel_json_test_jsonr_condition_el),
	KUNIT_CASE(panel_json_test_jsonr_condition_fi),
	KUNIT_CASE(panel_json_test_jsonr_rx_packet),
	KUNIT_CASE(panel_json_test_jsonr_resource_with_mutable_resource),
	KUNIT_CASE(panel_json_test_jsonr_resource_with_immutable_resource),
	KUNIT_CASE(panel_json_test_jsonr_dumpinfo),
	KUNIT_CASE(panel_json_test_jsonr_delayinfo),
	KUNIT_CASE(panel_json_test_jsonr_timer_delay_begin),
	KUNIT_CASE(panel_json_test_jsonr_power_ctrl),
	KUNIT_CASE(panel_json_test_jsonr_property_range_type),
	KUNIT_CASE(panel_json_test_jsonr_property_enum_type),
	KUNIT_CASE(panel_json_test_jsonr_config),
	KUNIT_CASE(panel_json_test_jsonr_sequence),
	KUNIT_CASE(panel_json_test_jsonr_function),
	KUNIT_CASE(panel_json_test_jsonr_all),
	KUNIT_CASE(panel_json_test_jsonw_byte_array_field),
	KUNIT_CASE(panel_json_test_jsonw_byte_array_field_with_null_array),
	KUNIT_CASE(panel_json_test_jsonw_maptbl),
	KUNIT_CASE(panel_json_test_jsonw_static_packet),
	KUNIT_CASE(panel_json_test_jsonw_variable_packet),
	KUNIT_CASE(panel_json_test_jsonw_keyinfo),
	KUNIT_CASE(panel_json_test_jsonw_function_based_condition_if),
	KUNIT_CASE(panel_json_test_jsonw_rule_based_condition_if),
	KUNIT_CASE(panel_json_test_jsonw_condition_el),
	KUNIT_CASE(panel_json_test_jsonw_condition_fi),
	KUNIT_CASE(panel_json_test_jsonw_rx_packet),
	KUNIT_CASE(panel_json_test_jsonw_resource),
	KUNIT_CASE(panel_json_test_jsonw_dumpinfo),
	KUNIT_CASE(panel_json_test_jsonw_delayinfo),
	KUNIT_CASE(panel_json_test_jsonw_timer_delay_begin),
	KUNIT_CASE(panel_json_test_jsonw_power_ctrl),
	KUNIT_CASE(panel_json_test_jsonw_property_range_type),
	KUNIT_CASE(panel_json_test_jsonw_property_enum_type),
	KUNIT_CASE(panel_json_test_jsonw_config),
	KUNIT_CASE(panel_json_test_jsonw_sequence),
	KUNIT_CASE(panel_json_test_jsonw_maptbl_list),
	KUNIT_CASE(panel_json_test_jsonw_rx_packet_list),
	KUNIT_CASE(panel_json_test_jsonw_resource_list),
	KUNIT_CASE(panel_json_test_jsonw_dumpinfo_list),
	KUNIT_CASE(panel_json_test_jsonw_power_ctrl_list),
	KUNIT_CASE(panel_json_test_jsonw_property_list),
	KUNIT_CASE(panel_json_test_jsonw_config_list),
	KUNIT_CASE(panel_json_test_jsonw_sequence_list),
	KUNIT_CASE(panel_json_test_jsonw_key_list),
	KUNIT_CASE(panel_json_test_jsonw_tx_packet_list),
	KUNIT_CASE(panel_json_test_jsonw_delay_list),
	KUNIT_CASE(panel_json_test_jsonw_condition_list),
	KUNIT_CASE(panel_json_test_jsonw_function_list),
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
static struct kunit_suite panel_json_test_module = {
	.name = "panel_json_test",
	.init = panel_json_test_init,
	.exit = panel_json_test_exit,
	.test_cases = panel_json_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_json_test_module);

MODULE_LICENSE("GPL v2");
