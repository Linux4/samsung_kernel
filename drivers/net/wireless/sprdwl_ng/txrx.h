/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRDWL_TXRX_H__
#define __SPRDWL_TXRX_H__

struct sprdwl_vif;
struct sprdwl_priv;
struct sprdwl_msg_buf;

int sprdwl_send_data(struct sprdwl_vif *vif,
		     struct sprdwl_msg_buf *msg,
		     struct sk_buff *skb, u8 type, u8 offset);
int sprdwl_send_cmd(struct sprdwl_priv *priv, struct sprdwl_msg_buf *msg);

unsigned short sprdwl_rx_data_process(struct sprdwl_priv *priv,
				      unsigned char *msg);
unsigned short sprdwl_rx_event_process(struct sprdwl_priv *priv, u8 *msg);
unsigned short sprdwl_rx_rsp_process(struct sprdwl_priv *priv, u8 *msg);

#endif
