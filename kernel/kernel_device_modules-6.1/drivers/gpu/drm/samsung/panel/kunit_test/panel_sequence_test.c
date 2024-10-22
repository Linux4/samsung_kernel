// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel.h"
#include "panel_drv.h"
#include "panel_obj.h"
#include "panel_sequence.h"
#include "panel_property.h"

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

extern int add_edge(struct list_head *head, int vertex);
extern void topo_sort(struct list_head head[],
		int vertex, bool visited[], int stack[], int *top);
extern bool check_cycle(struct list_head adj_list[],
		int n, int stack[], int *top);
extern bool is_cyclic(struct list_head *adj_list, int n);

struct panel_sequence_test {
	struct panel_device *panel;
};

static void panel_sequence_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_sequence_test_is_valid_sequence_return_false_with_invalid_sequence(struct kunit *test)
{
	struct seqinfo *seq =
		kunit_kzalloc(test, sizeof(*seq), GFP_KERNEL);

	pnobj_init(&seq->base, CMD_TYPE_SEQ, "test_sequence");
	KUNIT_EXPECT_FALSE(test, is_valid_sequence(seq));

	seq->cmdtbl = kunit_kzalloc(test, sizeof(void *), GFP_KERNEL);
	seq->size = 1;

	pnobj_init(&seq->base, CMD_TYPE_NONE, "test_sequence");
	KUNIT_EXPECT_FALSE(test, is_valid_sequence(seq));
}

static void panel_sequence_test_is_valid_sequence_return_true_with_valid_sequence(struct kunit *test)
{
	struct seqinfo *seq =
		kunit_kzalloc(test, sizeof(*seq), GFP_KERNEL);

	seq->cmdtbl = kunit_kzalloc(test, sizeof(void *), GFP_KERNEL);
	seq->size = 1;

	pnobj_init(&seq->base, CMD_TYPE_SEQ, "test_sequence");
	KUNIT_EXPECT_TRUE(test, is_valid_sequence(seq));
}

static void panel_sequence_test_snprintf_sequence(struct kunit *test)
{
	u8 TEST_PANEL_SLEEP_OUT[] = { 0x11 };
	u8 TEST_PANEL_WRDISBV[] = { 0x51, 0x03, 0xFF };
	u8 TEST_PANEL_DISPLAY_ON[] = { 0x29 };
	DEFINE_STATIC_PACKET(test_panel_sleep_out, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_OUT, 0);
	DEFINE_PKTUI(test_panel_wrdisbv, NULL, 1);
	DEFINE_VARIABLE_PACKET(test_panel_wrdisbv, DSI_PKT_TYPE_WR, TEST_PANEL_WRDISBV, 0);
	DEFINE_STATIC_PACKET(test_panel_display_on, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_ON, 0);
	void *test_panel_init_cmdtbl[] = {
		&PKTINFO(test_panel_sleep_out),
		&PKTINFO(test_panel_wrdisbv),
		&PKTINFO(test_panel_display_on),
	};
	DEFINE_SEQINFO(test_panel_init_seq, test_panel_init_cmdtbl);
	char *buf = kunit_kzalloc(test, SZ_1K, GFP_KERNEL);

	snprintf_sequence(buf, SZ_1K, SEQINFO(&test_panel_init_seq));
	KUNIT_EXPECT_STREQ(test, buf, "test_panel_init_seq\n"
			"commands: 3\n"
			"[0]: type: TX_PACKET, name: test_panel_sleep_out\n"
			"[1]: type: TX_PACKET, name: test_panel_wrdisbv\n"
			"[2]: type: TX_PACKET, name: test_panel_display_on");
}

static void panel_sequence_test_create_sequence_success(struct kunit *test)
{
	struct seqinfo *seq =
		create_sequence("test_sequence", 1);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, seq);
	destroy_sequence(seq);
}

static void panel_sequence_test_add_edge_with_invalid_arguments(struct kunit *test)
{
	LIST_HEAD(head);

	KUNIT_EXPECT_EQ(test, add_edge(NULL, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, add_edge(&head, -1), -EINVAL);

	KUNIT_EXPECT_TRUE(test, list_empty(&head));
}

#define MAX_COMMAND_NODE (6)

static void panel_sequence_test_toposort_success(struct kunit *test)
{
	struct list_head adj_list[MAX_COMMAND_NODE];
	bool visited[MAX_COMMAND_NODE] = {0};
	int stack[MAX_COMMAND_NODE];
	int top = 0;
	int i;

	for (i = 0; i < MAX_COMMAND_NODE; i++)
		INIT_LIST_HEAD(&adj_list[i]);

	add_edge(&adj_list[5], 0);
	add_edge(&adj_list[5], 2);
	add_edge(&adj_list[2], 3);
	add_edge(&adj_list[3], 1);
	add_edge(&adj_list[4], 0);
	add_edge(&adj_list[4], 1);
	add_edge(&adj_list[0], 1);

	for (i = 0; i < MAX_COMMAND_NODE; i++) {
		if (!visited[i])
			topo_sort(adj_list, i, visited, stack, &top);
	}

	top--;
	KUNIT_EXPECT_EQ(test, stack[top], 5);
	top--;
	KUNIT_EXPECT_EQ(test, stack[top], 4);
	top--;
	KUNIT_EXPECT_EQ(test, stack[top], 2);
	top--;
	KUNIT_EXPECT_EQ(test, stack[top], 3);
	top--;
	KUNIT_EXPECT_EQ(test, stack[top], 0);
	top--;
	KUNIT_EXPECT_EQ(test, stack[top], 1);

	for (i = 0; i < MAX_COMMAND_NODE; i++) {
		struct command_node *pos, *next;

		list_for_each_entry_safe(pos, next, &adj_list[i], list)
			kfree(pos);
	};
}

static void panel_sequence_test_check_cycle_success(struct kunit *test)
{
	struct list_head adj_list[MAX_COMMAND_NODE];
	bool visited[MAX_COMMAND_NODE] = {0};
	int stack[MAX_COMMAND_NODE];
	bool cycle = false;
	int top = 0;
	int i;

	for (i = 0; i < MAX_COMMAND_NODE; i++)
		INIT_LIST_HEAD(&adj_list[i]);

	add_edge(&adj_list[0], 1);
	add_edge(&adj_list[1], 2);
	add_edge(&adj_list[2], 3);
	add_edge(&adj_list[3], 0);

	for (i = 0; i < MAX_COMMAND_NODE; i++) {
		if (!visited[i])
			topo_sort(adj_list, i, visited, stack, &top);
	}

	cycle = check_cycle(adj_list, MAX_COMMAND_NODE, stack, &top);

	KUNIT_EXPECT_TRUE(test, cycle);

	for (i = 0; i < MAX_COMMAND_NODE; i++) {
		struct command_node *pos, *next;

		list_for_each_entry_safe(pos, next, &adj_list[i], list)
			kfree(pos);
	};
}

static void panel_sequence_test_is_cyclic_success(struct kunit *test)
{
	struct list_head adj_list[MAX_COMMAND_NODE];
	int i;

	for (i = 0; i < MAX_COMMAND_NODE; i++)
		INIT_LIST_HEAD(&adj_list[i]);

	add_edge(&adj_list[0], 1);
	add_edge(&adj_list[1], 2);
	add_edge(&adj_list[2], 3);
	KUNIT_EXPECT_FALSE(test, is_cyclic(adj_list, MAX_COMMAND_NODE));

	add_edge(&adj_list[1], 3);
	KUNIT_EXPECT_FALSE(test, is_cyclic(adj_list, MAX_COMMAND_NODE));

	add_edge(&adj_list[3], 4);
	add_edge(&adj_list[1], 4);
	add_edge(&adj_list[5], 0);
	KUNIT_EXPECT_FALSE(test, is_cyclic(adj_list, MAX_COMMAND_NODE));

	add_edge(&adj_list[4], 5);
	KUNIT_EXPECT_TRUE(test, is_cyclic(adj_list, MAX_COMMAND_NODE));

	for (i = 0; i < MAX_COMMAND_NODE; i++) {
		struct command_node *pos, *next;

		list_for_each_entry_safe(pos, next, &adj_list[i], list)
			kfree(pos);
	};
}

static void panel_sequence_test_get_index_of_sequence_success(struct kunit *test)
{
	struct list_head seq_list;
	void *test_panel_command_table1[1];
	void *test_panel_command_table2[1];
	DEFINE_SEQINFO(test_panel_sequence1, test_panel_command_table1);
	DEFINE_SEQINFO(test_panel_sequence2, test_panel_command_table2);

	INIT_LIST_HEAD(&seq_list);
	list_add_tail(get_pnobj_list(&SEQINFO(test_panel_sequence1).base), &seq_list);
	list_add_tail(get_pnobj_list(&SEQINFO(test_panel_sequence2).base), &seq_list);
	KUNIT_EXPECT_EQ(test, (u32)get_index_of_sequence(&seq_list,
				&SEQINFO(test_panel_sequence1)), 0U);
	KUNIT_EXPECT_EQ(test, (u32)get_index_of_sequence(&seq_list,
				&SEQINFO(test_panel_sequence2)), 1U);
}

static void panel_sequence_test_get_count_of_sequence_success(struct kunit *test)
{
	struct list_head seq_list;
	struct list_head list1;
	struct list_head list2;
	void *test_panel_command_table1[1];
	void *test_panel_command_table2[1];
	DEFINE_SEQINFO(test_panel_sequence1, test_panel_command_table1);
	DEFINE_SEQINFO(test_panel_sequence2, test_panel_command_table2);

	INIT_LIST_HEAD(&seq_list);
	list_add_tail(&list1, &seq_list);
	list_add_tail(&list2, &seq_list);

	KUNIT_EXPECT_EQ(test, (u32)get_count_of_sequence(&seq_list), 0U);

	list_add_tail(get_pnobj_list(&SEQINFO(test_panel_sequence1).base), &seq_list);
	list_add_tail(get_pnobj_list(&SEQINFO(test_panel_sequence2).base), &seq_list);
	KUNIT_EXPECT_EQ(test, (u32)get_count_of_sequence(&seq_list), 2U);
}

static void panel_sequence_test_sequence_sort_fail_with_invalid_sequence(struct kunit *test)
{
	struct list_head seq_list;
	u8 TEST_PANEL_SLEEP_OUT[] = { 0x11 };
	u8 TEST_PANEL_SLEEP_IN[] = { 0x10 };
	u8 TEST_PANEL_DISPLAY_OFF[] = { 0x28 };
	u8 TEST_PANEL_DISPLAY_ON[] = { 0x29 };
	u8 TEST_PANEL_WRDISBV[] = { 0x51, 0x03, 0xFF };
	DEFINE_STATIC_PACKET(test_panel_sleep_out, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_OUT, 0);
	DEFINE_STATIC_PACKET(test_panel_sleep_in, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_IN, 0);
	DEFINE_STATIC_PACKET(test_panel_display_on, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_ON, 0);
	DEFINE_STATIC_PACKET(test_panel_display_off, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_OFF, 0);
	DEFINE_PKTUI(test_panel_wrdisbv, NULL, 1);
	DEFINE_VARIABLE_PACKET(test_panel_wrdisbv, DSI_PKT_TYPE_WR, TEST_PANEL_WRDISBV, 0);

	void *test_panel_set_brightness_command_table[] = {
		&PKTINFO(test_panel_wrdisbv),
	};
	DEFINE_SEQINFO(test_panel_set_brightness_sequence, test_panel_set_brightness_command_table);

	void *test_panel_set_display_mode_command_table[] = {
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_set_display_mode_sequence, test_panel_set_display_mode_command_table);

	void *test_panel_init_command_table[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&PKTINFO(test_panel_sleep_out),
	};
	DEFINE_SEQINFO(test_panel_init_sequence, test_panel_init_command_table);

	void *test_panel_display_on_command_table[] = {
		&PKTINFO(test_panel_display_on),
	};
	DEFINE_SEQINFO(test_panel_display_on_sequence, test_panel_display_on_command_table);

	void *test_panel_display_off_command_table[] = {
		&PKTINFO(test_panel_display_off),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_display_off_sequence, test_panel_display_off_command_table);

	void *test_panel_exit_command_table[3] = {
		&PKTINFO(test_panel_sleep_in),
		&SEQINFO(test_panel_set_display_mode_sequence),
		NULL,
	};
	DEFINE_SEQINFO(test_panel_exit_sequence, test_panel_exit_command_table);

	void *test_panel_all_command_table[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&SEQINFO(test_panel_init_sequence),
		&SEQINFO(test_panel_display_on_sequence),
		&SEQINFO(test_panel_display_off_sequence),
		&SEQINFO(test_panel_exit_sequence),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_all_sequence, test_panel_all_command_table);

	void *test_panel_dummy_command_table[] = {
		NULL,
	};
	DEFINE_SEQINFO(test_panel_dummy_sequence, test_panel_dummy_command_table);

	/*
	 * test adjacent list
	 *
	 * [set_brightness] -> ()
	 * [set_display_mode] -> (set_brightness)
	 * [init] -> (set_display_mode)
	 * [display_on] -> ()
	 * [display_off] -> (set_brightness)
	 * [exit] -> (set_display_mode)
	 * [all] -> (set_brightness, se_display_mode, init, display_on, display_off, exit)
	 */
	struct seqinfo *unsorted_seq_array[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&SEQINFO(test_panel_init_sequence),
		&SEQINFO(test_panel_display_on_sequence),
		&SEQINFO(test_panel_display_off_sequence),
		&SEQINFO(test_panel_dummy_sequence),
		&SEQINFO(test_panel_all_sequence),
		&SEQINFO(test_panel_exit_sequence),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	struct pnobj *pos;
	int i;

	INIT_LIST_HEAD(&seq_list);

	/* make invalid sequence */
	set_pnobj_cmd_type(&SEQINFO(test_panel_dummy_sequence).base, PNOBJ_TYPE_NONE);

	for (i = 0; i < ARRAY_SIZE(unsorted_seq_array); i++)
		list_add_tail(get_pnobj_list(&unsorted_seq_array[i]->base), &seq_list);

	KUNIT_EXPECT_EQ(test, sequence_sort(&seq_list), -EINVAL);

	i = 0;
	list_for_each_entry(pos, &seq_list, list) {
		KUNIT_EXPECT_STREQ(test, get_pnobj_name(pos),
				get_sequence_name(unsorted_seq_array[i]));
		i++;
	}
}

static void panel_sequence_test_sequence_sort_success_without_cycle(struct kunit *test)
{
	struct list_head seq_list;
	u8 TEST_PANEL_SLEEP_OUT[] = { 0x11 };
	u8 TEST_PANEL_SLEEP_IN[] = { 0x10 };
	u8 TEST_PANEL_DISPLAY_OFF[] = { 0x28 };
	u8 TEST_PANEL_DISPLAY_ON[] = { 0x29 };
	u8 TEST_PANEL_WRDISBV[] = { 0x51, 0x03, 0xFF };
	DEFINE_STATIC_PACKET(test_panel_sleep_out, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_OUT, 0);
	DEFINE_STATIC_PACKET(test_panel_sleep_in, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_IN, 0);
	DEFINE_STATIC_PACKET(test_panel_display_on, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_ON, 0);
	DEFINE_STATIC_PACKET(test_panel_display_off, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_OFF, 0);
	DEFINE_PKTUI(test_panel_wrdisbv, NULL, 1);
	DEFINE_VARIABLE_PACKET(test_panel_wrdisbv, DSI_PKT_TYPE_WR, TEST_PANEL_WRDISBV, 0);

	void *test_panel_set_brightness_command_table[] = {
		&PKTINFO(test_panel_wrdisbv),
	};
	DEFINE_SEQINFO(test_panel_set_brightness_sequence, test_panel_set_brightness_command_table);

	void *test_panel_set_display_mode_command_table[] = {
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_set_display_mode_sequence, test_panel_set_display_mode_command_table);

	void *test_panel_init_command_table[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&PKTINFO(test_panel_sleep_out),
	};
	DEFINE_SEQINFO(test_panel_init_sequence, test_panel_init_command_table);

	void *test_panel_display_on_command_table[] = {
		&PKTINFO(test_panel_display_on),
	};
	DEFINE_SEQINFO(test_panel_display_on_sequence, test_panel_display_on_command_table);

	void *test_panel_display_off_command_table[] = {
		&PKTINFO(test_panel_display_off),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_display_off_sequence, test_panel_display_off_command_table);

	void *test_panel_exit_command_table[3] = {
		&PKTINFO(test_panel_sleep_in),
		&SEQINFO(test_panel_set_display_mode_sequence),
		NULL,
	};
	DEFINE_SEQINFO(test_panel_exit_sequence, test_panel_exit_command_table);

	void *test_panel_all_command_table[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&SEQINFO(test_panel_init_sequence),
		&SEQINFO(test_panel_display_on_sequence),
		&SEQINFO(test_panel_display_off_sequence),
		&SEQINFO(test_panel_exit_sequence),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_all_sequence, test_panel_all_command_table);

	void *test_panel_dummy_command_table[] = {
		NULL,
	};
	DEFINE_SEQINFO(test_panel_dummy_sequence, test_panel_dummy_command_table);

	/*
	 * test adjacent list
	 *
	 * [set_brightness] -> ()
	 * [set_display_mode] -> (set_brightness)
	 * [init] -> (set_display_mode)
	 * [display_on] -> ()
	 * [display_off] -> (set_brightness)
	 * [exit] -> (set_display_mode)
	 * [all] -> (set_brightness, se_display_mode, init, display_on, display_off, exit)
	 */
	struct seqinfo *unsorted_seq_array[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&SEQINFO(test_panel_init_sequence),
		&SEQINFO(test_panel_display_on_sequence),
		&SEQINFO(test_panel_display_off_sequence),
		&SEQINFO(test_panel_dummy_sequence),
		&SEQINFO(test_panel_all_sequence),
		&SEQINFO(test_panel_exit_sequence),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	struct seqinfo *sorted_seq_array[] = {
		&SEQINFO(test_panel_set_brightness_sequence),
		&SEQINFO(test_panel_set_display_mode_sequence),
		&SEQINFO(test_panel_init_sequence),
		&SEQINFO(test_panel_display_on_sequence),
		&SEQINFO(test_panel_display_off_sequence),
		&SEQINFO(test_panel_dummy_sequence),
		&SEQINFO(test_panel_exit_sequence),
		&SEQINFO(test_panel_all_sequence),
	};
	struct pnobj *pos;
	int i;

	INIT_LIST_HEAD(&seq_list);

	for (i = 0; i < ARRAY_SIZE(unsorted_seq_array); i++)
		list_add_tail(get_pnobj_list(&unsorted_seq_array[i]->base), &seq_list);

	KUNIT_EXPECT_EQ(test, sequence_sort(&seq_list), 0);

	i = 0;
	list_for_each_entry(pos, &seq_list, list) {
		KUNIT_EXPECT_STREQ(test, get_pnobj_name(pos),
				get_sequence_name(sorted_seq_array[i]));
		i++;
	}
}

static void panel_sequence_test_sequence_sort_fail_with_cycle(struct kunit *test)
{
	struct list_head seq_list;
	u8 TEST_PANEL_SLEEP_OUT[] = { 0x11 };
	u8 TEST_PANEL_SLEEP_IN[] = { 0x10 };
	u8 TEST_PANEL_DISPLAY_OFF[] = { 0x28 };
	u8 TEST_PANEL_DISPLAY_ON[] = { 0x29 };
	u8 TEST_PANEL_WRDISBV[] = { 0x51, 0x03, 0xFF };
	DEFINE_STATIC_PACKET(test_panel_sleep_out, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_OUT, 0);
	DEFINE_STATIC_PACKET(test_panel_sleep_in, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_IN, 0);
	DEFINE_STATIC_PACKET(test_panel_display_on, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_ON, 0);
	DEFINE_STATIC_PACKET(test_panel_display_off, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_OFF, 0);
	DEFINE_PKTUI(test_panel_wrdisbv, NULL, 1);
	DEFINE_VARIABLE_PACKET(test_panel_wrdisbv, DSI_PKT_TYPE_WR, TEST_PANEL_WRDISBV, 0);

	void *test_panel_set_brightness_command_table[] = {
		&PKTINFO(test_panel_wrdisbv),
	};
	DEFINE_SEQINFO(test_panel_set_brightness_sequence, test_panel_set_brightness_command_table);

	void *test_panel_set_display_mode_command_table[] = {
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_set_display_mode_sequence, test_panel_set_display_mode_command_table);

	void *test_panel_init_command_table[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&PKTINFO(test_panel_sleep_out),
	};
	DEFINE_SEQINFO(test_panel_init_sequence, test_panel_init_command_table);

	void *test_panel_display_on_command_table[] = {
		&PKTINFO(test_panel_display_on),
	};
	DEFINE_SEQINFO(test_panel_display_on_sequence, test_panel_display_on_command_table);

	void *test_panel_display_off_command_table[] = {
		&PKTINFO(test_panel_display_off),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_display_off_sequence, test_panel_display_off_command_table);

	void *test_panel_exit_command_table[3] = {
		&PKTINFO(test_panel_sleep_in),
		&SEQINFO(test_panel_set_display_mode_sequence),
		NULL,
	};
	DEFINE_SEQINFO(test_panel_exit_sequence, test_panel_exit_command_table);

	void *test_panel_all_command_table[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&SEQINFO(test_panel_init_sequence),
		&SEQINFO(test_panel_display_on_sequence),
		&SEQINFO(test_panel_display_off_sequence),
		&SEQINFO(test_panel_exit_sequence),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	DEFINE_SEQINFO(test_panel_all_sequence, test_panel_all_command_table);

	/*
	 * test adjacent list
	 *
	 * [set_brightness] -> ()
	 * [set_display_mode] -> (set_brightness)
	 * [init] -> (set_display_mode)
	 * [display_on] -> ()
	 * [display_off] -> (set_brightness)
	 * [exit] -> (set_display_mode)
	 * [all] -> (set_brightness, se_display_mode, init, display_on, display_off, exit)
	 */
	struct seqinfo *unsorted_seq_array[] = {
		&SEQINFO(test_panel_set_display_mode_sequence),
		&SEQINFO(test_panel_init_sequence),
		&SEQINFO(test_panel_display_on_sequence),
		&SEQINFO(test_panel_display_off_sequence),
		&SEQINFO(test_panel_all_sequence),
		&SEQINFO(test_panel_exit_sequence),
		&SEQINFO(test_panel_set_brightness_sequence),
	};
	struct pnobj *pos;
	int i;

	/* make cycle */
	test_panel_exit_command_table[2] =
		&SEQINFO(test_panel_all_sequence);

	INIT_LIST_HEAD(&seq_list);

	for (i = 0; i < ARRAY_SIZE(unsorted_seq_array); i++)
		list_add_tail(get_pnobj_list(&unsorted_seq_array[i]->base), &seq_list);

	KUNIT_ASSERT_EQ(test, sequence_sort(&seq_list), -EINVAL);

	i = 0;
	list_for_each_entry(pos, &seq_list, list) {
		KUNIT_EXPECT_STREQ(test, get_pnobj_name(pos),
				get_sequence_name(unsorted_seq_array[i]));
		i++;
	}
}

static void panel_sequence_test_sequence_to_pnobj_refs_success(struct kunit *test)
{
	struct pnobj_refs *cmd_refs;
	struct panel_expr_data expr_true[] = { PANEL_EXPR_OPERAND_U32(1) };
	DEFINE_CONDITION(condition_true, expr_true);
	u8 TEST_PANEL_SLEEP_OUT[] = { 0x11 };
	u8 TEST_PANEL_SLEEP_IN[] = { 0x10 };
	u8 TEST_PANEL_DISPLAY_OFF[] = { 0x28 };
	u8 TEST_PANEL_DISPLAY_ON[] = { 0x29 };
	u8 TEST_PANEL_WRDISBV[] = { 0x51, 0x03, 0xFF };
	DEFINE_STATIC_PACKET(test_panel_sleep_out, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_OUT, 0);
	DEFINE_STATIC_PACKET(test_panel_sleep_in, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_IN, 0);
	DEFINE_STATIC_PACKET(test_panel_display_on, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_ON, 0);
	DEFINE_STATIC_PACKET(test_panel_display_off, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_OFF, 0);
	DEFINE_PKTUI(test_panel_wrdisbv, NULL, 1);
	DEFINE_VARIABLE_PACKET(test_panel_wrdisbv, DSI_PKT_TYPE_WR, TEST_PANEL_WRDISBV, 0);
	void *test_panel_all_command_table[] = {
		&CONDINFO_IF(condition_true),
			&PKTINFO(test_panel_sleep_out),
			&PKTINFO(test_panel_display_on),
			&PKTINFO(test_panel_wrdisbv),
		&CONDINFO_EL(condition_true),
			&PKTINFO(test_panel_display_off),
			&PKTINFO(test_panel_sleep_in),
		&CONDINFO_FI(condition_true),
	};
	DEFINE_SEQINFO(test_panel_all_sequence, test_panel_all_command_table);
	struct pnobj_ref *ref;
	int i = 0;

	cmd_refs = sequence_to_pnobj_refs(&SEQINFO(test_panel_all_sequence));
	KUNIT_ASSERT_TRUE(test, !list_empty(get_pnobj_refs_list(cmd_refs)));

	list_for_each_entry(ref, get_pnobj_refs_list(cmd_refs), list)
		KUNIT_EXPECT_STREQ(test, get_pnobj_name(ref->pnobj),
				get_pnobj_name(test_panel_all_command_table[i++]));

	remove_pnobj_refs(cmd_refs);
}

static void panel_sequence_test_sequence_condition_filter_success(struct kunit *test)
{
	struct pnobj_refs *cmd_refs, *pkt_refs;
	struct panel_sequence_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_expr_data expr_true[] = { PANEL_EXPR_OPERAND_U32(1) };
	DEFINE_CONDITION(condition_true, expr_true);
	u8 TEST_PANEL_SLEEP_OUT[] = { 0x11 };
	u8 TEST_PANEL_SLEEP_IN[] = { 0x10 };
	u8 TEST_PANEL_DISPLAY_OFF[] = { 0x28 };
	u8 TEST_PANEL_DISPLAY_ON[] = { 0x29 };
	u8 TEST_PANEL_WRDISBV[] = { 0x51, 0x03, 0xFF };
	DEFINE_STATIC_PACKET(test_panel_sleep_out, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_OUT, 0);
	DEFINE_STATIC_PACKET(test_panel_sleep_in, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_IN, 0);
	DEFINE_STATIC_PACKET(test_panel_display_on, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_ON, 0);
	DEFINE_STATIC_PACKET(test_panel_display_off, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_OFF, 0);
	DEFINE_PKTUI(test_panel_wrdisbv, NULL, 1);
	DEFINE_VARIABLE_PACKET(test_panel_wrdisbv, DSI_PKT_TYPE_WR, TEST_PANEL_WRDISBV, 0);
	void *test_panel_all_command_table[] = {
		&CONDINFO_IF(condition_true),
			&PKTINFO(test_panel_sleep_out),
			&PKTINFO(test_panel_display_on),
			&PKTINFO(test_panel_wrdisbv),
		&CONDINFO_EL(condition_true),
			&PKTINFO(test_panel_display_off),
			&PKTINFO(test_panel_sleep_in),
		&CONDINFO_FI(condition_true),
	};
	DEFINE_SEQINFO(test_panel_all_sequence, test_panel_all_command_table);
	struct pnobj_refs test_pnobj_refs;
	struct pnobj_ref *ref;
	int i = 1;

	cmd_refs = sequence_condition_filter(panel, &SEQINFO(test_panel_all_sequence));
	list_for_each_entry(ref, get_pnobj_refs_list(cmd_refs), list)
		KUNIT_ASSERT_STREQ(test, get_pnobj_name(ref->pnobj),
				get_pnobj_name(test_panel_all_command_table[i++]));

	i = 1;
	pkt_refs = pnobj_refs_filter(is_tx_packet, cmd_refs);
	list_for_each_entry(ref, get_pnobj_refs_list(cmd_refs), list)
		KUNIT_ASSERT_STREQ(test, get_pnobj_name(ref->pnobj),
				get_pnobj_name(test_panel_all_command_table[i++]));

	INIT_PNOBJ_REFS(&test_pnobj_refs);
	list_replace_init(get_pnobj_refs_list(pkt_refs),
			get_pnobj_refs_list(&test_pnobj_refs));

	i = 1;
	list_for_each_entry(ref, &test_pnobj_refs.list, list)
		KUNIT_ASSERT_STREQ(test, get_pnobj_name(ref->pnobj),
				get_pnobj_name(test_panel_all_command_table[i++]));

	KUNIT_ASSERT_EQ(test, get_count_of_pnobj_ref(&test_pnobj_refs), 3);
	remove_all_pnobj_ref(&test_pnobj_refs);

	remove_pnobj_refs(cmd_refs);
	remove_pnobj_refs(pkt_refs);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_sequence_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_sequence_test *ctx = kunit_kzalloc(test,
			sizeof(struct panel_sequence_test), GFP_KERNEL);

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	ctx->panel = panel;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_sequence_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_sequence_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	KUNIT_CASE(panel_sequence_test_test),
	KUNIT_CASE(panel_sequence_test_is_valid_sequence_return_false_with_invalid_sequence),
	KUNIT_CASE(panel_sequence_test_is_valid_sequence_return_true_with_valid_sequence),
	KUNIT_CASE(panel_sequence_test_snprintf_sequence),
	KUNIT_CASE(panel_sequence_test_create_sequence_success),
	KUNIT_CASE(panel_sequence_test_add_edge_with_invalid_arguments),
	KUNIT_CASE(panel_sequence_test_toposort_success),
	KUNIT_CASE(panel_sequence_test_check_cycle_success),
	KUNIT_CASE(panel_sequence_test_is_cyclic_success),
	KUNIT_CASE(panel_sequence_test_get_index_of_sequence_success),
	KUNIT_CASE(panel_sequence_test_get_count_of_sequence_success),
	KUNIT_CASE(panel_sequence_test_sequence_sort_fail_with_invalid_sequence),
	KUNIT_CASE(panel_sequence_test_sequence_sort_success_without_cycle),
	KUNIT_CASE(panel_sequence_test_sequence_sort_fail_with_cycle),
	KUNIT_CASE(panel_sequence_test_sequence_to_pnobj_refs_success),
	KUNIT_CASE(panel_sequence_test_sequence_condition_filter_success),
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
static struct kunit_suite panel_sequence_test_module = {
	.name = "panel_sequence_test",
	.init = panel_sequence_test_init,
	.exit = panel_sequence_test_exit,
	.test_cases = panel_sequence_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_sequence_test_module);

MODULE_LICENSE("GPL v2");
