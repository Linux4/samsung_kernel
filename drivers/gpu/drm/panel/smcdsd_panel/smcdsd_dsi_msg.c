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

#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)

int MAX_TRANSFER_NUM = MAX_TX_CMD_NUM;

void dump_dsi_msg_tx(unsigned long data0)
{
	struct mtk_ddic_dsi_msg *mtk_msg = (struct mtk_ddic_dsi_msg *)data0;
	unsigned int i;

	for (i = 0; i < (u16)mtk_msg->tx_cmd_num; i++)
		dsi_write_data_dump(mtk_msg->type[i], (unsigned long)mtk_msg->tx_buf[i], mtk_msg->tx_len[i]);
}

int send_msg_segment(void *msg, int len)
{
	struct msg_segment **segment = (struct msg_segment **)msg;
	struct mtk_ddic_dsi_msg mtk_buf = {0, };
	struct mtk_ddic_dsi_msg *mtk_msg = &mtk_buf;
	int loop, pos, type;
	int last;
	int send = 0, null = 0;
	int ret = 0;

	for (loop = 0; loop < len; loop++) {
		null = send = 0;

		if (!segment[loop]->dsi_msg.tx_len && !segment[loop]->dsi_msg.rx_len)
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
		}

		last = (loop + 1 >= len) ? 1 : 0;

		if (last)
			send |= 1;
		else if (mtk_msg->tx_cmd_num >= MAX_TRANSFER_NUM)
			send |= 1;
		else if (segment[loop]->modes & MSG_MODE_SEND_MASK)
			send |= 1;
		else if (segment[loop]->delay)
			send |= 1;
		else if (segment[loop]->dsi_msg.flags != segment[loop + 1]->dsi_msg.flags) /* take care last and then loop + 1 */
			send |= 1;

		if (!send)
			continue;

		if (mtk_msg->tx_cmd_num) {
			ret = smcdsd_dsi_msg_tx(NULL, (unsigned long)mtk_msg, segment[loop]->modes & BIT(MSG_MODE_BLOCKING));
			memset(mtk_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
		}

		if (segment[loop]->delay)
			usleep_range(segment[loop]->delay, segment[loop]->delay);
	}

	if (mtk_msg->tx_cmd_num)
		BUG();

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

	return send_msg_segment(segment, total);
}

int append_msg_segment(struct mipi_dsi_msg *src, struct mtk_ddic_dsi_msg *dst)
{
	struct msg_segment segment = {0, };
	struct mtk_ddic_dsi_msg *mtk_msg = dst;
	int pos, type;
	int tx_cmd_num = mtk_msg->tx_cmd_num;

	if (tx_cmd_num >= MAX_TRANSFER_NUM) {
		dbg_info("%s: NUM(%d) MAX(%d)\n", __func__, tx_cmd_num, MAX_TRANSFER_NUM);
		return tx_cmd_num;
	}

	segment.dsi_msg = *src;

	pos = mtk_msg->tx_cmd_num;

	type = segment.dsi_msg.type;
	if (!type) {
		if (segment.dsi_msg.tx_len == 1)
			type = MIPI_DSI_DCS_SHORT_WRITE;
		else if (segment.dsi_msg.tx_len == 2)
			type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		else
			type = MIPI_DSI_DCS_LONG_WRITE;
	}

	segment.dsi_msg.type = type;

	mtk_msg->tx_buf[pos] = segment.dsi_msg.tx_buf;
	mtk_msg->tx_len[pos] = segment.dsi_msg.tx_len;
	mtk_msg->type[pos] = segment.dsi_msg.type;
	mtk_msg->flags = segment.dsi_msg.flags;

	mtk_msg->tx_cmd_num++;

	return mtk_msg->tx_cmd_num;
}

int append_msg(struct mipi_dsi_msg *src, struct mtk_ddic_dsi_msg *dst)
{
	return append_msg_segment(src, dst);
}

int append_cmd(unsigned char *cmd, int len, struct mtk_ddic_dsi_msg *dst)
{
	struct mipi_dsi_msg src = {0, };

	src.tx_buf = (void *)cmd;
	src.tx_len = len;

	return append_msg(&src, dst);
}

int send_cmd(unsigned char *cmd, int len)
{
	struct mtk_ddic_dsi_msg mtk_buf = {0, };
	struct mtk_ddic_dsi_msg *mtk_msg = &mtk_buf;
	int tx_cmd_num = mtk_msg->tx_cmd_num;
	int ret;

	ret = append_cmd(cmd, len, mtk_msg);
	if (tx_cmd_num == ret) {
		return -EINVAL;
	}

	return smcdsd_dsi_msg_tx(NULL, (unsigned long)mtk_msg, 0);
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
				segment->dsi_msg.tx_len, segment->dsi_msg.type, segment->dsi_msg.flags, segment->delay, segment->modes, segment->msg_name ? segment->msg_name : "null", segment->dsi_msg.tx_len, (char *)segment->dsi_msg.tx_buf);
		}
	}
}

