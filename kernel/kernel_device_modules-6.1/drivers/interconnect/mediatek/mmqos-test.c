// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Wendy-ST Lin <wendy-st.lin@mediatek.com>
 */

#include <dt-bindings/interconnect/mtk,mmqos.h>
#include "mmqos-mtk.h"
#include "mmqos-test.h"

struct icc_node *create_fake_icc_node(int id)
{
	struct icc_node *node;

	node = mtk_icc_node_create(id);
	return node;
}

void set_fake_v2_bw(struct icc_node *node, int srt_r, int srt_w, int hrt_r, int hrt_w)
{
	node->v2_avg_r_bw = srt_r;
	node->v2_avg_w_bw = srt_w;
	node->v2_peak_r_bw = hrt_r;
	node->v2_peak_w_bw = hrt_w;
}

static void test_update_channel_bw(bool is_disp_by_larb)
{
	struct icc_node *node1, *node2, *node3;
	struct common_port_node *comm_port_node1, *comm_port_node2, *comm_port_node3;
	int node_id1, node_id2, node_id3;
	u32 orig_mmqos_state;
	int disp_ans0[4] = {0, 0, 0, 0};
	int disp_ans_by_larb[4] = {1000, 2000, 3000, 4000};
	int disp_ans2[4] = {10, 20, 15, 40};
	int *disp_ans;

	clear_chnn_bw();
	orig_mmqos_state = mmqos_state;
	mmqos_state |= DPC_ENABLE;
	if (is_disp_by_larb)
		mmqos_state |= DISP_BY_LARB_ENABLE;
	else
		mmqos_state &= ~DISP_BY_LARB_ENABLE;
	MMQOS_DBG("orig mmqos_state:%#x, set DPC_ENABLE & %s DISP_BY_LARB, new state:%#x",
		orig_mmqos_state,
		is_disp_by_larb ? "set" : "unset", mmqos_state);

	node_id1 = MASTER_COMMON_PORT(0, 0);
	node1 = create_fake_icc_node(node_id1);
	comm_port_node1 = create_fake_comm_port_node(HRT_DISP, 0, 0, 0, 0);
	node1->data = (void *)comm_port_node1;
	set_fake_v2_bw(node1, 10, 20, 30, 40);
	update_channel_bw(0, 0, node1);

	if (mmqos_state & DISP_BY_LARB_ENABLE) {
		disp_ans = disp_ans0;
		check_chnn_bw(0, 0, 0, 0, 0, 0);
	} else {
		disp_ans = disp_ans2;
		check_chnn_bw(0, 0, 10, 20, 15, 40);
	}
	check_disp_chnn_bw(0, 0, disp_ans);

	node_id2 = MASTER_COMMON_PORT(0, 1);
	node2 = create_fake_icc_node(node_id2);
	comm_port_node2 = create_fake_comm_port_node(HRT_NONE, 0, 0, 0, 0);
	node2->data = (void *)comm_port_node2;
	set_fake_v2_bw(node2, 100, 200, 300, 400);
	update_channel_bw(0, 0, node2);
	check_disp_chnn_bw(0, 0, disp_ans);
	if (mmqos_state & DISP_BY_LARB_ENABLE)
		check_chnn_bw(0, 0, 100, 200, 300, 400);
	else
		check_chnn_bw(0, 0, 110, 220, 315, 440);


	set_fake_v2_bw(node1, 10, 20, 30, 40);
	update_channel_bw(0, 0, node1);
	check_disp_chnn_bw(0, 0, disp_ans);
	if (mmqos_state & DISP_BY_LARB_ENABLE)
		check_chnn_bw(0, 0, 100, 200, 300, 400);
	else
		check_chnn_bw(0, 0, 110, 220, 315, 440);

	node_id3 = MASTER_COMMON_PORT(0, 3);
	node3 = create_fake_icc_node(node_id3);
	comm_port_node3 = create_fake_comm_port_node(HRT_DISP_BY_LARB, 0, 0, 0, 0);
	node3->data = (void *)comm_port_node3;
	set_fake_v2_bw(node3, 1000, 2000, 3000, 4000);
	update_channel_bw(0, 0, node3);
	if (mmqos_state & DISP_BY_LARB_ENABLE) {
		disp_ans = disp_ans_by_larb;
		check_chnn_bw(0, 0, 1100, 2200, 3300, 4400);
	} else {
		check_chnn_bw(0, 0, 110, 220, 315, 440);
	}
	check_disp_chnn_bw(0, 0, disp_ans);

	clear_chnn_bw();

	// clear DPC_ENABLE, check disp_bw all 0
	mmqos_state &= ~DPC_ENABLE;
	MMQOS_DBG("clear DPC_ENABLE, mmqos_state:%#x", mmqos_state);

	node_id1 = MASTER_COMMON_PORT(0, 0);
	node1 = create_fake_icc_node(node_id1);
	comm_port_node1 = create_fake_comm_port_node(HRT_DISP, 0, 0, 0, 0);
	node1->data = (void *)comm_port_node1;
	set_fake_v2_bw(node1, 10, 20, 30, 40);
	update_channel_bw(0, 0, node1);

	disp_ans = disp_ans0;
	check_disp_chnn_bw(0, 0, disp_ans);
	if (mmqos_state & DISP_BY_LARB_ENABLE)
		check_chnn_bw(0, 0, 0, 0, 0, 0);
	else
		check_chnn_bw(0, 0, 10, 20, 15, 40);


	node_id2 = MASTER_COMMON_PORT(0, 1);
	node2 = create_fake_icc_node(node_id2);
	comm_port_node2 = create_fake_comm_port_node(HRT_NONE, 0, 0, 0, 0);
	node2->data = (void *)comm_port_node2;
	set_fake_v2_bw(node2, 100, 200, 300, 400);
	update_channel_bw(0, 0, node2);
	check_disp_chnn_bw(0, 0, disp_ans);
	if (mmqos_state & DISP_BY_LARB_ENABLE)
		check_chnn_bw(0, 0, 100, 200, 300, 400);
	else
		check_chnn_bw(0, 0, 110, 220, 315, 440);

	set_fake_v2_bw(node1, 10, 20, 30, 40);
	update_channel_bw(0, 0, node1);
	check_disp_chnn_bw(0, 0, disp_ans);
	if (mmqos_state & DISP_BY_LARB_ENABLE)
		check_chnn_bw(0, 0, 100, 200, 300, 400);
	else
		check_chnn_bw(0, 0, 110, 220, 315, 440);

	node_id3 = MASTER_COMMON_PORT(0, 3);
	node3 = create_fake_icc_node(node_id3);
	comm_port_node3 = create_fake_comm_port_node(HRT_DISP_BY_LARB, 0, 0, 0, 0);
	node3->data = (void *)comm_port_node3;
	set_fake_v2_bw(node3, 1000, 2000, 3000, 4000);
	update_channel_bw(0, 0, node3);
	check_disp_chnn_bw(0, 0, disp_ans);
	if (mmqos_state & DISP_BY_LARB_ENABLE)
		check_chnn_bw(0, 0, 1100, 2200, 3300, 4400);
	else
		check_chnn_bw(0, 0, 110, 220, 315, 440);

	mmqos_state = orig_mmqos_state;
	MMQOS_DBG("restore mmqos_state:%#x", mmqos_state);
}

void mmqos_kernel_test(u32 test_id)
{
	switch (test_id) {
	case TEST_CHNN_BW_DISP_BY_LARB:
		test_update_channel_bw(true);
		break;
	case TEST_CHNN_BW_NO_DISP_BY_LARB:
		test_update_channel_bw(false);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(mmqos_kernel_test);

MODULE_LICENSE("GPL");
