// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <drm/drm_mipi_dsi.h>
#include <video/mipi_display.h>

#include "smcdsd_dsi_msg.h"
#include "smcdsd_panel.h"
#include "../panels/dd.h"

#undef pr_fmt
#define pr_fmt(fmt) "smcdsd: %s: " fmt, __func__

#define dbg_info(fmt, ...)		pr_info(fmt, ##__VA_ARGS__)

#define BUG_ON_MSG(_condition, ...)	do { if (_condition) { pr_info(__VA_ARGS__); BUG(); }; } while (0)

int MAX_TRANSFER_NUM = MAX_TX_CMD_NUM;

void dump_dsi_msg_tx(unsigned long data0)
{
	struct mtk_ddic_dsi_msg *mtk_msg = (struct mtk_ddic_dsi_msg *)data0;
	unsigned int i;

	for (i = 0; i < (u16)mtk_msg->tx_cmd_num; i++)
		dsi_write_data_dump(mtk_msg->type[i], (unsigned long)mtk_msg->tx_buf[i], mtk_msg->tx_len[i]);
}

static int __send_msg_segment(void *msg, int len)
{
	struct msg_segment **segment = (struct msg_segment **)msg;
	struct mtk_ddic_dsi_msg mtk_buf = {0, };
	struct mtk_ddic_dsi_msg *mtk_msg = &mtk_buf;
	int loop, pos, type;
	int last;
	int send = 0, null = 0;
	int ret = 0;
	int modes = 0;

	for (loop = 0; loop < len; loop++) {
		null = send = 0;

		if (!segment[loop]->dsi_msg.tx_len && !segment[loop]->dsi_msg.rx_len)
			null = 1;

		if (!segment[loop]->dsi_msg.tx_buf && !segment[loop]->dsi_msg.rx_buf)
			null = 1;

		if (segment[loop]->dsi_msg.rx_len && !segment[loop]->dsi_msg.tx_len)
			null = 1;

		if (!!(segment[loop]->dsi_msg.rx_buf) ^ !!(segment[loop]->dsi_msg.rx_len))
			null = 1;

		if (!null) {
			pos = mtk_msg->tx_cmd_num;

			type = segment[loop]->dsi_msg.type;
			if (!type) {
				if (segment[loop]->dsi_msg.tx_len == 1)
					type = MIPI_DSI_DCS_SHORT_WRITE;
				else if (segment[loop]->dsi_msg.tx_len == 2)
					type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
				else
					type = MIPI_DSI_DCS_LONG_WRITE;
			}

			segment[loop]->dsi_msg.type = type;

			mtk_msg->tx_buf[pos] = segment[loop]->dsi_msg.tx_buf;
			mtk_msg->tx_len[pos] = segment[loop]->dsi_msg.tx_len;
			mtk_msg->type[pos] = segment[loop]->dsi_msg.type;
			mtk_msg->flags = segment[loop]->dsi_msg.flags;
			mtk_msg->tx_cmd_num++;

			if (segment[loop]->dsi_msg.rx_buf) {
				mtk_msg->rx_buf[pos] = segment[loop]->dsi_msg.rx_buf;
				mtk_msg->rx_len[pos] = segment[loop]->dsi_msg.rx_len;
				mtk_msg->rx_cmd_num++;
			}

			modes = segment[loop]->modes;
		}

		last = (loop + 1 >= len) ? 1 : 0;

		if (last)
			send |= 1;
		else if (null)
			send |= 1;
		else if (mtk_msg->tx_cmd_num >= MAX_TRANSFER_NUM)
			send |= 1;
		else if (modes & MSG_MODE_SEND_MASK)
			send |= 1;
		else if (segment[loop]->delay)
			send |= 1;
		else if (segment[loop]->dsi_msg.flags != segment[loop + 1]->dsi_msg.flags) /* take care last and then loop + 1 */
			send |= 1;
		else if (segment[loop]->dsi_msg.rx_len || segment[loop + 1]->dsi_msg.rx_len) {
			send |= 1;
			modes |= BIT(MSG_MODE_BLOCKING);
		}

		if (!send)
			continue;

		BUG_ON_MSG(mtk_msg->tx_cmd_num > MAX_TRANSFER_NUM, "tx_cmd_num(%d)\n", mtk_msg->tx_cmd_num);
		BUG_ON_MSG(mtk_msg->rx_cmd_num && mtk_msg->rx_cmd_num != mtk_msg->tx_cmd_num,
			"rx_cmd_num(%d) != tx_cmd_num(%d)\n", mtk_msg->rx_cmd_num, mtk_msg->tx_cmd_num);

		if (mtk_msg->rx_cmd_num) {
			memset(mtk_msg->rx_buf[0], 0, mtk_msg->rx_len[0]);
			ret = smcdsd_dsi_msg_rx(mtk_msg);
			dsi_rx_data_dump(mtk_msg->type[0], *((u8 *)mtk_msg->tx_buf[0]), mtk_msg->rx_len[0], ret, mtk_msg->rx_buf[0]);
			memset(mtk_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
			modes = 0;

			if (segment[loop]->cb)
				segment[loop]->cb(0);	/* todo: read fail */
		}

		if (mtk_msg->tx_cmd_num) {
			ret = smcdsd_dsi_msg_tx(NULL, (unsigned long)mtk_msg, modes & BIT(MSG_MODE_BLOCKING));
			memset(mtk_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
			modes = 0;
		}

		if (segment[loop]->delay)
			usleep_range(segment[loop]->delay, segment[loop]->delay);
	}

	BUG_ON_MSG(mtk_msg->tx_cmd_num, "tx_cmd_num(%d)\n", mtk_msg->tx_cmd_num);

	return ret;
}

int send_msg_package(void *src, int len, void *dst)
{
	struct msg_package **package = (struct msg_package **)src;
	struct msg_segment **segment = dst;
	int i, k, total = 0;

	for (i = 0; i < len; i++) {
		for (k = 0; k < package[i]->len; k++)
			segment[total++] = &((struct msg_segment *)package[i]->buf)[k];
	}

	BUG_ON(total > MAX_SEGMENT_NUM);

	return __send_msg_segment(segment, total);
}

int send_cmd(unsigned char *cmd, int len)
{
	struct msg_segment segment = {0, };
	struct msg_segment *segments[1] = { &segment };

	segment.dsi_msg.tx_buf = (void *)cmd;
	segment.dsi_msg.tx_len = len;

	segment.msg_name = "send_cmd";
	segment.modes = MSG_MODE_SEND_MASK;

	return __send_msg_segment(&segments, 1);
}

int send_msg_segment(void *msg, int len)
{
	struct msg_package package = { msg, len };

	struct msg_package *src = &package;

	struct msg_segment *dst[MAX_SEGMENT_NUM] = {0, };

	return send_msg_package(&src, 1, &dst);
}

void dump_msg_package(struct msg_package *package_list_for_dump, int max)
{
	int i, j;

	dbg_info("%s: max(%d)\n", __func__, max);

	for (i = 0; i < max; i++) {
		struct msg_package *package;
		struct msg_segment *segment;

		dbg_info("[%3d] length is %d\n", i, package_list_for_dump[i].len);

		if (!package_list_for_dump[i].len)
			continue;

		package = (struct msg_package *)package_list_for_dump[i].buf;
		for (j = 0; j < package_list_for_dump[i].len; j++) {
			dbg_info("[%3d] package len is %d\n", j, package[j].len);

			if (!package[j].len)
				continue;

			segment = (struct msg_segment *)package[j].buf;

			dbg_info("[%3d] segment tx_len(%3d)\n", j, segment->dsi_msg.tx_len);

			if (!segment->dsi_msg.tx_len)
				continue;

			dbg_info("[%3d] tx_len(%3d) type(%2x) flags(%d) delay(%d) modes(%d) name(%s) tx_buf(%*ph)\n", j,
				segment->dsi_msg.tx_len, segment->dsi_msg.type, segment->dsi_msg.flags, segment->delay, segment->modes,
				segment->msg_name ? segment->msg_name : "null", segment->dsi_msg.tx_len, (char *)segment->dsi_msg.tx_buf);
		}
	}
}

struct msg_package *get_patched_package(struct msg_package *base, int n1, int n2, int n3, int n4)
{
	struct msg_package *package = base;
	struct msg_segment *segment;
	int *jump;
	int idx;
	int offset = 0;

	for (idx = 0; idx < package->len; idx++) {
		segment = &((struct msg_segment *)package->buf)[idx];

		if (!segment->ndarray)
			continue;

		if (!segment->ndarray->data)
			continue;

		jump = segment->ndarray->jump;
		offset += !n1 ? n1 : n1 * jump[1];
		offset += !n2 ? n2 : n2 * jump[2];
		offset += !n3 ? n3 : n3 * jump[3];
		offset += !n4 ? n4 : n4 * jump[4];

		segment->dsi_msg.tx_len = segment->ndarray->jump[segment->ndarray->ndim - 1];
		segment->dsi_msg.tx_buf = (void *)&segment->ndarray->data[offset];

		//pr_info("n[%d %d %d %d] j[%d %d %d %d] offset(%d) ndim(%d) tx_len(%d) data(%*ph)\n",
			//n1, n2, n3, n4, jump[1], jump[2], jump[3], jump[4], offset, segment->ndarray->ndim,
			//segment->dsi_msg.tx_len, segment->dsi_msg.tx_len, segment->ndarray->data);
	}

	return package;
}


