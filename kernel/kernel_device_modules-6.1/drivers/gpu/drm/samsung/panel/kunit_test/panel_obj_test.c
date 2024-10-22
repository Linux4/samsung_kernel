// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/list_sort.h>

#include "panel_drv.h"
#include "panel_obj.h"

struct panel_obj_test {
	struct panel_device *panel;
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
static void panel_obj_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_obj_test_set_get_pnobj_success(struct kunit *test)
{
	struct pnobj *base = kunit_kzalloc(test,
			sizeof(struct pnobj), GFP_KERNEL);
	char name[32] = "test panel object";

	set_pnobj_cmd_type(base, CMD_TYPE_MAP);
	KUNIT_EXPECT_EQ(test, (u32)get_pnobj_cmd_type(base), (u32)CMD_TYPE_MAP);

	set_pnobj_name(base, name);
	KUNIT_EXPECT_PTR_NE(test, get_pnobj_name(base), (char *)name);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(base), (char *)name);
}

static void panel_obj_test_pnobj_init_success(struct kunit *test)
{
	struct pnobj *base = kunit_kzalloc(test,
			sizeof(struct pnobj), GFP_KERNEL);
	char name[32] = "test panel object";

	pnobj_init(base, CMD_TYPE_MAP, name);
	KUNIT_EXPECT_EQ(test, (u32)get_pnobj_cmd_type(base), (u32)CMD_TYPE_MAP);
	KUNIT_EXPECT_PTR_NE(test, get_pnobj_name(base), (char *)name);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(base), (char *)name);
}

static void panel_obj_test_is_valid_panel_obj(struct kunit *test)
{
	struct pnobj *test_pnobj = kunit_kzalloc(test,
			sizeof(struct pnobj), GFP_KERNEL);

	KUNIT_EXPECT_TRUE(test, !is_valid_panel_obj(NULL));
	KUNIT_EXPECT_TRUE(test, !is_valid_panel_obj(test_pnobj));

	pnobj_init(test_pnobj, CMD_TYPE_MAP, NULL);
	KUNIT_EXPECT_TRUE(test, !is_valid_panel_obj(test_pnobj));

	pnobj_init(test_pnobj, CMD_TYPE_NONE, "test_pnobj");
	KUNIT_EXPECT_TRUE(test, !is_valid_panel_obj(test_pnobj));

	pnobj_init(test_pnobj, MAX_CMD_TYPE + 1, "test_pnobj");
	KUNIT_EXPECT_TRUE(test, !is_valid_panel_obj(test_pnobj));

	pnobj_init(test_pnobj, CMD_TYPE_MAP, "test_pnobj");
	KUNIT_EXPECT_TRUE(test, is_valid_panel_obj(test_pnobj));
}

static void panel_obj_test_pnobj_find_by_name_success(struct kunit *test)
{
	struct pnobj pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj1, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj2, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj3, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj4, CMD_TYPE_SEQ),
		{ .name = NULL, .cmd_type = CMD_TYPE_NONE },
	};
	struct list_head pnobj_list;
	int i;

	INIT_LIST_HEAD(&pnobj_list);
	for (i = 0; i < ARRAY_SIZE(pnobj_array); i++)
		list_add_tail(&pnobj_array[i].list, &pnobj_list);

	KUNIT_EXPECT_PTR_EQ(test, pnobj_find_by_name(&pnobj_list, "test_pnobj2"),
			&pnobj_array[1]);
	KUNIT_EXPECT_TRUE(test, !pnobj_find_by_name(&pnobj_list, "test_pnobj"));
	KUNIT_EXPECT_TRUE(test, !pnobj_find_by_name(&pnobj_list, "test_pnobj22"));
	KUNIT_EXPECT_TRUE(test, !pnobj_find_by_name(&pnobj_list, NULL));
}

static void panel_obj_test_pnobj_find_by_substr_success(struct kunit *test)
{
	struct pnobj pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj1, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj2, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj3, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj4, CMD_TYPE_SEQ),
		{ .name = NULL, .cmd_type = CMD_TYPE_NONE },
	};
	struct list_head pnobj_list;
	int i;

	INIT_LIST_HEAD(&pnobj_list);
	for (i = 0; i < ARRAY_SIZE(pnobj_array); i++)
		list_add_tail(&pnobj_array[i].list, &pnobj_list);

	KUNIT_EXPECT_PTR_EQ(test, pnobj_find_by_substr(&pnobj_list, "test_pnobj2"),
			&pnobj_array[1]);
	KUNIT_EXPECT_PTR_EQ(test, pnobj_find_by_substr(&pnobj_list, "test_pnobj"),
			&pnobj_array[0]);
	KUNIT_EXPECT_TRUE(test, !pnobj_find_by_substr(&pnobj_list, "test_pnobj22"));
	KUNIT_EXPECT_TRUE(test, !pnobj_find_by_substr(&pnobj_list, NULL));
}

static void panel_obj_test_pnobj_find_by_pnobj_success(struct kunit *test)
{
	struct pnobj pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj_property, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_map, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_delay, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_cond_el, CMD_TYPE_COND_EL),
		__PNOBJ_INITIALIZER(test_pnobj_cond_fi, CMD_TYPE_COND_FI),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_config, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet1, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_packet, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_packet, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_key, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_resource, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_dump, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_seq, CMD_TYPE_SEQ),
	};
	struct list_head pnobj_list;
	struct pnobj *test_pnobj = kunit_kzalloc(test,
			sizeof(struct pnobj), GFP_KERNEL);
	struct pnobj *pnobj;
	int i;

	INIT_LIST_HEAD(&pnobj_list);
	for (i = 0; i < ARRAY_SIZE(pnobj_array); i++)
		list_add_tail(&pnobj_array[i].list, &pnobj_list);

	pnobj_init(test_pnobj, CMD_TYPE_MAP, "test_pnobj_map");
	pnobj = pnobj_find_by_pnobj(&pnobj_list, test_pnobj);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pnobj);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(pnobj), (u32)CMD_TYPE_MAP);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(pnobj), "test_pnobj_map");

	pnobj_init(test_pnobj, CMD_TYPE_TX_PACKET, "test_pnobj_tx_packet1");
	pnobj = pnobj_find_by_pnobj(&pnobj_list, test_pnobj);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pnobj);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(pnobj), (u32)CMD_TYPE_TX_PACKET);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(pnobj), "test_pnobj_tx_packet1");

	pnobj_init(test_pnobj, CMD_TYPE_TX_PACKET, "test_pnobj_packet");
	pnobj = pnobj_find_by_pnobj(&pnobj_list, test_pnobj);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pnobj);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(pnobj), (u32)CMD_TYPE_TX_PACKET);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(pnobj), "test_pnobj_packet");

	pnobj_init(test_pnobj, CMD_TYPE_RX_PACKET, "test_pnobj_packet");
	pnobj = pnobj_find_by_pnobj(&pnobj_list, test_pnobj);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pnobj);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(pnobj), (u32)CMD_TYPE_RX_PACKET);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(pnobj), "test_pnobj_packet");

	pnobj_init(test_pnobj, CMD_TYPE_RX_PACKET, "test_pnobj_tx_packet1");
	pnobj = pnobj_find_by_pnobj(&pnobj_list, test_pnobj);
	KUNIT_EXPECT_TRUE(test, !pnobj);
}

static void panel_obj_test_pnobj_list_sort_by_pnobj_compare_success(struct kunit *test)
{
	struct pnobj pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj_property_abc, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_config_bbb, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl_bbb, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet_aaa, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_cond_el, CMD_TYPE_COND_EL),
		__PNOBJ_INITIALIZER(test_pnobj_key_bbb, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_resource_aaa, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet_bbb, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_resource_bbb, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_dump_bbb, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_seq_aaa, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj_dump_aaa, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_property_bcd, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin_b, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_key_aaa, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_map_aaa, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_property_ab, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet_bbb, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_config_aaa, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_seq_bbb, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj_map_bbb, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_delay_bbb, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_a, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_delay_aaa, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_property_abd, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if_aaa, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_b, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_cond_fi, CMD_TYPE_COND_FI),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin_a, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if_bbb, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl_aaa, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet_aaa, CMD_TYPE_RX_PACKET),
	};
	struct pnobj expected_pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj_property_ab, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_property_abc, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_property_abd, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_property_bcd, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_map_aaa, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_map_bbb, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_delay_aaa, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_delay_bbb, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_a, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_b, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin_a, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin_b, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if_aaa, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if_bbb, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_cond_el, CMD_TYPE_COND_EL),
		__PNOBJ_INITIALIZER(test_pnobj_cond_fi, CMD_TYPE_COND_FI),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl_aaa, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl_bbb, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet_aaa, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet_bbb, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_config_aaa, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_config_bbb, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet_aaa, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet_bbb, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_key_aaa, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_key_bbb, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_resource_aaa, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_resource_bbb, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_dump_aaa, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_dump_bbb, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_seq_aaa, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj_seq_bbb, CMD_TYPE_SEQ),
	};
	struct list_head pnobj_list;
	struct pnobj *pnobj;
	int i;

	INIT_LIST_HEAD(&pnobj_list);
	for (i = 0; i < ARRAY_SIZE(pnobj_array); i++)
		list_add_tail(&pnobj_array[i].list, &pnobj_list);

	list_sort(NULL, &pnobj_list, pnobj_compare);

	i = 0;
	list_for_each_entry(pnobj, &pnobj_list, list) {
		KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(pnobj),
				get_pnobj_cmd_type(&expected_pnobj_array[i]));
		KUNIT_EXPECT_STREQ(test, get_pnobj_name(pnobj),
				get_pnobj_name(&expected_pnobj_array[i]));
		i++;
	}
}

static void panel_obj_test_pnobj_list_sort_by_pnobj_ref_compare_success(struct kunit *test)
{
	struct pnobj pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj_property_abc, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_config_bbb, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl_bbb, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet_aaa, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_cond_el, CMD_TYPE_COND_EL),
		__PNOBJ_INITIALIZER(test_pnobj_key_bbb, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_resource_aaa, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet_bbb, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_resource_bbb, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_dump_bbb, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_seq_aaa, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj_dump_aaa, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_property_bcd, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin_b, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_key_aaa, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_map_aaa, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_property_ab, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet_bbb, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_config_aaa, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_seq_bbb, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj_map_bbb, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_delay_bbb, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_a, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_delay_aaa, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_property_abd, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if_aaa, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_b, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_cond_fi, CMD_TYPE_COND_FI),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin_a, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if_bbb, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl_aaa, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet_aaa, CMD_TYPE_RX_PACKET),
	};
	struct pnobj expected_pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj_property_ab, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_property_abc, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_property_abd, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_property_bcd, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_map_aaa, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_map_bbb, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_delay_aaa, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_delay_bbb, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_a, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_b, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin_a, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin_b, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if_aaa, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if_bbb, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_cond_el, CMD_TYPE_COND_EL),
		__PNOBJ_INITIALIZER(test_pnobj_cond_fi, CMD_TYPE_COND_FI),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl_aaa, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl_bbb, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet_aaa, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet_bbb, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_config_aaa, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_config_bbb, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet_aaa, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet_bbb, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_key_aaa, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_key_bbb, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_resource_aaa, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_resource_bbb, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_dump_aaa, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_dump_bbb, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_seq_aaa, CMD_TYPE_SEQ),
		__PNOBJ_INITIALIZER(test_pnobj_seq_bbb, CMD_TYPE_SEQ),
	};
	struct list_head pnobj_list;
	struct pnobj_refs *refs;
	struct pnobj_ref *ref;
	struct pnobj *pnobj;
	int i;

	INIT_LIST_HEAD(&pnobj_list);
	for (i = 0; i < ARRAY_SIZE(pnobj_array); i++)
		list_add_tail(&pnobj_array[i].list, &pnobj_list);

	refs = pnobj_list_to_pnobj_refs(&pnobj_list);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, refs);

	list_sort(NULL, &refs->list, pnobj_ref_compare);

	i = 0;
	list_for_each_entry(ref, &refs->list, list) {
		pnobj = ref->pnobj;
		KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(pnobj),
				get_pnobj_cmd_type(&expected_pnobj_array[i]));
		KUNIT_EXPECT_STREQ(test, get_pnobj_name(pnobj),
				get_pnobj_name(&expected_pnobj_array[i]));
		i++;
	}
}

static void panel_obj_test_add_pnobj_ref_success(struct kunit *test)
{
	struct pnobj pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj_property, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_map, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_delay, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_delay, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_delay, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_cond_el, CMD_TYPE_COND_EL),
		__PNOBJ_INITIALIZER(test_pnobj_cond_fi, CMD_TYPE_COND_FI),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_config, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_key, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_resource, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_dump, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_seq, CMD_TYPE_SEQ),
	};
	struct pnobj_refs test_pnobj_refs;
	struct pnobj_ref *pnobj_ref;
	int i;

	INIT_PNOBJ_REFS(&test_pnobj_refs);
	for (i = 0; i < ARRAY_SIZE(pnobj_array); i++)
		add_pnobj_ref(&test_pnobj_refs, &pnobj_array[i]);

	i = 0;
	list_for_each_entry(pnobj_ref, &test_pnobj_refs.list, list)
		KUNIT_EXPECT_PTR_EQ(test, pnobj_ref->pnobj, &pnobj_array[i++]);

	remove_all_pnobj_ref(&test_pnobj_refs);
	KUNIT_ASSERT_TRUE(test, list_empty(&test_pnobj_refs.list));
}

static void panel_obj_test_get_count_of_pnobj_ref_success(struct kunit *test)
{
	struct pnobj_refs pkt_refs;
	u8 TEST_PACKET_DATA[] = { 0x11, 0x00 };
	DEFINE_STATIC_PACKET(test_static_packet, DSI_PKT_TYPE_WR, TEST_PACKET_DATA, 0);
	int i;

	INIT_PNOBJ_REFS(&pkt_refs);

	KUNIT_ASSERT_EQ(test, get_count_of_pnobj_ref(&pkt_refs), 0);

	add_tx_packet_on_pnobj_refs(&PKTINFO(test_static_packet), &pkt_refs);
	KUNIT_EXPECT_EQ(test, get_count_of_pnobj_ref(&pkt_refs), 1);

	for (i = 0; i < 10; i++)
		add_tx_packet_on_pnobj_refs(&PKTINFO(test_static_packet), &pkt_refs);

	KUNIT_EXPECT_EQ(test, get_count_of_pnobj_ref(&pkt_refs), 11);
	remove_all_pnobj_ref(&pkt_refs);

	KUNIT_ASSERT_TRUE(test, list_empty(get_pnobj_refs_list(&pkt_refs)));
}

static void panel_obj_test_pnobj_list_to_pnobj_refs_success(struct kunit *test)
{
	struct pnobj pnobj_array[] = {
		__PNOBJ_INITIALIZER(test_pnobj_property, CMD_TYPE_PROP),
		__PNOBJ_INITIALIZER(test_pnobj_map, CMD_TYPE_MAP),
		__PNOBJ_INITIALIZER(test_pnobj_delay, CMD_TYPE_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay, CMD_TYPE_TIMER_DELAY),
		__PNOBJ_INITIALIZER(test_pnobj_timer_delay_begin, CMD_TYPE_TIMER_DELAY_BEGIN),
		__PNOBJ_INITIALIZER(test_pnobj_cond_if, CMD_TYPE_COND_IF),
		__PNOBJ_INITIALIZER(test_pnobj_cond_el, CMD_TYPE_COND_EL),
		__PNOBJ_INITIALIZER(test_pnobj_cond_fi, CMD_TYPE_COND_FI),
		__PNOBJ_INITIALIZER(test_pnobj_pwrctrl, CMD_TYPE_PCTRL),
		__PNOBJ_INITIALIZER(test_pnobj_rx_packet, CMD_TYPE_RX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_config, CMD_TYPE_CFG),
		__PNOBJ_INITIALIZER(test_pnobj_tx_packet, CMD_TYPE_TX_PACKET),
		__PNOBJ_INITIALIZER(test_pnobj_key, CMD_TYPE_KEY),
		__PNOBJ_INITIALIZER(test_pnobj_resource, CMD_TYPE_RES),
		__PNOBJ_INITIALIZER(test_pnobj_dump, CMD_TYPE_DMP),
		__PNOBJ_INITIALIZER(test_pnobj_seq, CMD_TYPE_SEQ),
	};
	struct list_head pnobj_list;
	struct pnobj_refs *refs;
	struct pnobj_ref *ref;
	struct pnobj *pnobj;
	int i;

	INIT_LIST_HEAD(&pnobj_list);
	refs = pnobj_list_to_pnobj_refs(&pnobj_list);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, refs);
	KUNIT_EXPECT_TRUE(test, list_empty(&refs->list));
	remove_pnobj_refs(refs);

	for (i = 0; i < ARRAY_SIZE(pnobj_array); i++)
		list_add_tail(&pnobj_array[i].list, &pnobj_list);
	refs = pnobj_list_to_pnobj_refs(&pnobj_list);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, refs);
	KUNIT_EXPECT_TRUE(test, !list_empty(&refs->list));

	i = 0;
	list_for_each_entry(ref, &refs->list, list) {
		pnobj = ref->pnobj;
		KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(pnobj),
				get_pnobj_cmd_type(&pnobj_array[i]));
		KUNIT_EXPECT_STREQ(test, get_pnobj_name(pnobj),
				get_pnobj_name(&pnobj_array[i]));
		i++;
	}
	remove_pnobj_refs(refs);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_obj_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_obj_test *ctx = kunit_kzalloc(test,
		sizeof(struct panel_obj_test), GFP_KERNEL);

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	INIT_LIST_HEAD(&panel->prop_list);

	ctx->panel = panel;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_obj_test_exit(struct kunit *test)
{
	struct panel_obj_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_obj_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_obj_test_test),
	KUNIT_CASE(panel_obj_test_set_get_pnobj_success),
	KUNIT_CASE(panel_obj_test_pnobj_init_success),

	KUNIT_CASE(panel_obj_test_is_valid_panel_obj),

	KUNIT_CASE(panel_obj_test_pnobj_find_by_name_success),
	KUNIT_CASE(panel_obj_test_pnobj_find_by_substr_success),
	KUNIT_CASE(panel_obj_test_pnobj_find_by_pnobj_success),
	KUNIT_CASE(panel_obj_test_pnobj_list_sort_by_pnobj_compare_success),
	KUNIT_CASE(panel_obj_test_pnobj_list_sort_by_pnobj_ref_compare_success),

	KUNIT_CASE(panel_obj_test_add_pnobj_ref_success),
	KUNIT_CASE(panel_obj_test_get_count_of_pnobj_ref_success),
	KUNIT_CASE(panel_obj_test_pnobj_list_to_pnobj_refs_success),
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
static struct kunit_suite panel_obj_test_module = {
	.name = "panel_obj_test",
	.init = panel_obj_test_init,
	.exit = panel_obj_test_exit,
	.test_cases = panel_obj_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_obj_test_module);

MODULE_LICENSE("GPL v2");
