/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth HCI Packet                                                       *
 *                                                                            *
 ******************************************************************************/
#include <linux/skbuff.h>

#include "slsi_bt_log.h"
#include "hci_pkt.h"

struct hci_pkt_type {
	char         *name;
	char         type;
	char         hdr_size;
	char         len_offset;
	char         len_size;
	unsigned int len_mask;
};

static struct hci_pkt_type hci_command_type = {
	.type = HCI_COMMAND_PKT,
	.name = "HCI Command Packet",
	.hdr_size = 3,
	.len_offset = 2,
	.len_size = 1,
	.len_mask = 0xFF,
};

static struct hci_pkt_type hci_event_type = {
	.type = HCI_EVENT_PKT,
	.name = "HCI Event Packet",
	.hdr_size = 2,
	.len_offset = 1,
	.len_size = 1,
	.len_mask = 0xFF,
};

static struct hci_pkt_type hci_acl_type = {
	.type = HCI_ACL_DATA_PKT,
	.name = "HCI ACL Data Packet",
	.hdr_size = 4,
	.len_offset = 2,
	.len_size = 2,
	.len_mask = 0xFFFF,
};

static struct hci_pkt_type hci_iso_type = {
	.type = HCI_ISO_DATA_PKT,
	.name = "HCI ISO Data Packet",
	.hdr_size = 4,
	.len_offset = 2,
	.len_size = 2,
	.len_mask = 0x3FFF,
};

static struct hci_pkt_type *get_hci_pkt_type(int type)
{
	if (type == HCI_COMMAND_PKT)
		return &hci_command_type;
	else if (type == HCI_EVENT_PKT)
		return &hci_event_type;
	else if (type == HCI_ACL_DATA_PKT)
		return &hci_acl_type;
	else if (type == HCI_ISO_DATA_PKT)
		return &hci_iso_type;
	return NULL;
}

static size_t hci_pkt_get_payload_size(struct hci_pkt_type *htype, char *hdr,
					size_t hdr_len)
{
	int oct;
	size_t ret = 0;
	if (!htype || !hdr || htype->hdr_size > hdr_len)
		return 0;

	for (oct = 0; oct < htype->len_size; oct++)
		ret |= hdr[htype->len_offset + oct] << (oct * 8);
	return ret & htype->len_mask;
}

static size_t hci_pkt_get_size(struct hci_pkt_type *htype, char *hdr,
				size_t hdr_len)
{
	if (!htype || !hdr || htype->hdr_size > hdr_len)
		return 0;

	return htype->hdr_size + hci_pkt_get_payload_size(htype, hdr, hdr_len);
}

bool hci_pkt_check_complete(char type, char *data, size_t len)
{
	struct hci_pkt_type *htype;
	size_t pkt_size;

	if (!data || len == 0) {
		BT_DBG("missing packet data\n");
		return false;
	}

	htype = get_hci_pkt_type(type);
	if (!htype) {
		BT_DBG("Unknown HCI packet type(%d)\n", type);
		return false;
	}

	if (htype->hdr_size > len) {
		BT_DBG("Not enough header. type:%d, expected:%d, len:%zu\n",
			 type, htype->hdr_size, len);
		return false;
	}

	pkt_size = hci_pkt_get_size(htype, data, htype->hdr_size);
	if (pkt_size > len) {
		BT_DBG("Not enough packet length. type:%d, expected:%zu, len:%zu\n",
			 type, pkt_size, len);
		return false;
	} else if (pkt_size < len) {
		BT_WARNING("Overrun packet length. type:%d, expected:%zu, len:%zu\n",
				type, pkt_size, len);
		return false;
	}

	BT_INFO("HCI packet type(%s), packet length:%zu", htype->name, pkt_size);
	return true;
}
