/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "panel.h"
#include "panel_obj.h"
#include "panel_packet.h"

unsigned int get_pktinfo_type(struct pktinfo *pkt)
{
	return get_pnobj_cmd_type(&pkt->base);
}

char *get_pktinfo_name(struct pktinfo *pkt)
{
	return get_pnobj_name(&pkt->base);
}

unsigned int get_rdinfo_type(struct rdinfo *rdi)
{
	return get_pnobj_cmd_type(&rdi->base);
}

char *get_rdinfo_name(struct rdinfo *rdi)
{
	return get_pnobj_name(&rdi->base);
}

bool is_valid_tx_packet(struct pktinfo *tx_packet)
{
	unsigned int type;

	if (!tx_packet)
		return false;

	type = get_pnobj_cmd_type(&tx_packet->base);
	if (!IS_CMD_TYPE_TX_PKT(type))
		return false;

	return true;
}

bool is_valid_rdinfo(struct rdinfo *rdi)
{
	unsigned int type;

	if (!rdi)
		return false;

	type = get_pnobj_cmd_type(&rdi->base);
	if (!IS_CMD_TYPE_RX_PKT(type))
		return false;

	return true;
}

/**
 * create_tx_packet - create a struct pktinfo structure
 * @name: pointer to a string for the name of this packet.
 * @type: type of packet.
 * @msg: message to tx.
 * @pktui: pointer to a packet update information array.
 * @nr_pktui: number of packet update information.
 * @option: property of tx packet.
 *
 * This is used to create a struct pktinfo pointer.
 *
 * Returns &struct pktinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_tx_packet().
 */
struct pktinfo *create_tx_packet(char *name, u32 type,
		struct panel_tx_msg *msg, struct pkt_update_info *pktui, u32 nr_pktui, u32 option)
{
	struct pktinfo *tx_packet;

	if (!name)
		return NULL;

	if (!msg)
		return NULL;

	if (!IS_CMD_TYPE_TX_PKT(type))
		return NULL;

	tx_packet = kzalloc(sizeof(*tx_packet), GFP_KERNEL);
	if (!tx_packet)
		return NULL;

	tx_packet->data =
		kvmalloc(sizeof(u8) * msg->len, GFP_KERNEL);
	if (!tx_packet->data)
		goto err_alloc_data;

	tx_packet->offset = msg->gpara_offset;
	memcpy(tx_packet->data, msg->buf, msg->len);
	tx_packet->dlen = msg->len;

	if (pktui && nr_pktui > 0) {
		tx_packet->pktui =
			kzalloc(sizeof(*pktui) * nr_pktui, GFP_KERNEL);
		if (!tx_packet->pktui)
			goto err_alloc_pktui;

		memcpy(tx_packet->pktui, pktui, sizeof(*pktui) * nr_pktui);
		tx_packet->nr_pktui = nr_pktui;
	}
	tx_packet->option = option;
	pnobj_init(&tx_packet->base, type, name);

	return tx_packet;

err_alloc_pktui:
	kvfree(tx_packet->data);

err_alloc_data:
	kfree(tx_packet);
	return NULL;
}
EXPORT_SYMBOL(create_tx_packet);

/**
 * destroy_tx_packet - destroys a struct pktinfo structure
 * @tx_packet: pointer to the struct pktinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_tx_packet().
 */
void destroy_tx_packet(struct pktinfo *tx_packet)
{
	if (!tx_packet)
		return;

	free_pnobj_name(&tx_packet->base);
	kfree(tx_packet->pktui);
	kvfree(tx_packet->data);
	kfree(tx_packet);
}
EXPORT_SYMBOL(destroy_tx_packet);

/**
 * create_rx_packet - create a struct rdinfo structure
 * @name: pointer to a string for the name of this packet.
 * @type: type of packet.
 *
 * This is used to create a struct rdinfo pointer.
 *
 * Returns &struct rdinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_rx_packet().
 */
struct rdinfo *create_rx_packet(char *name, u32 type, struct panel_rx_msg *msg)
{
	struct rdinfo *rx_packet;

	if (!name)
		return NULL;

	if (!msg)
		return NULL;

	if (!IS_CMD_TYPE_RX_PKT(type))
		return NULL;

	rx_packet = kzalloc(sizeof(*rx_packet), GFP_KERNEL);
	if (!rx_packet)
		return NULL;

	rx_packet->data =
		kvmalloc(sizeof(u8) * msg->len, GFP_KERNEL);
	if (!rx_packet->data)
		goto err;

	rx_packet->addr = msg->addr;
	rx_packet->offset = msg->gpara_offset;
	rx_packet->len = msg->len;
	pnobj_init(&rx_packet->base, type, name);

	return rx_packet;

err:
	kfree(rx_packet);
	return NULL;
}
EXPORT_SYMBOL(create_rx_packet);

/**
 * destroy_rx_packet - destroys a struct pktinfo structure
 * @rx_packet: pointer to the struct pktinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_rx_packet().
 */
void destroy_rx_packet(struct rdinfo *rx_packet)
{
	if (!rx_packet)
		return;

	free_pnobj_name(&rx_packet->base);
	kvfree(rx_packet->data);
	kfree(rx_packet);
}
EXPORT_SYMBOL(destroy_rx_packet);
