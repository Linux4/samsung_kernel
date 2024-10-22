/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __MTK_ISE_MAILBOX_H__
#define __MTK_ISE_MAILBOX_H__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/soc/mediatek/mtk-ise-mbox.h>

#define PFX	"[ISE MBOX]: "

#define MB_HOST_VERSION_OFFSET			(0x0000)
#define MB_HOST_IN_FIFO_OFFSET			(0x0010)
#define MB_HOST_IN_STATUS_OFFSET		(0x0020)
#define MB_HOST_IN_CONTROL_OFFSET		(0x0030)
#define MB_HOST_OUT_FIFO_OFFSET			(0x0040)
#define MB_HOST_OUT_STATUS_OFFSET		(0x0050)
#define MB_HOST_OUT_CONTROL_OFFSET		(0x0060)
#define MB_HOST_SAFETY_FIFO_OFFSET		(0x0070)
#define MB_HOST_SAFETY_STATUS_OFFSET		(0x0080)
#define MB_HOST_SAFETY_CONTROL_OFFSET		(0x0090)
#define MB_HOST_SECTION_BOOT_STATUS_OFFSET	(0x00A0)
#define MB_HOST_BOOT_IP_INTEGRITY_STATUS_OFFSET	(0x00B0)
#define MB_HOST_IRQ_CONFIG_0_OFFSET		(0x00C0)
#define MB_HOST_IRQ_CONFIG_1_OFFSET		(0x00D0)
#define MB_HOST_IRQ_CLEAR_0_OFFSET		(0x00E0)
#define MB_HOST_IRQ_CLEAR_1_OFFSET		(0x00F0)
#define MB_HOST_IRQ_STATUS_0_OFFSET		(0x0100)
#define MB_HOST_IRQ_STATUS_1_OFFSET		(0x0110)

#define MAILBOX_RETRY	1000000000

static inline uint32_t mailbox_pack_request_header(const mailbox_request_t *request)
{
	uint32_t packed_request_header = 0;

	packed_request_header |= request->service_id;
	packed_request_header |= (uint32_t)(request->payload.size << 8);
	packed_request_header |= (uint32_t)(request->service_version << 12);
	packed_request_header |= (uint32_t)(request->request_type << 20);

	return packed_request_header;
}

static inline void mailbox_unpack_request_header(const uint32_t packed_header,
	mailbox_request_t *request)
{
	request->service_id = packed_header & 0xFF;
	request->payload.size = (packed_header >> 8) & MAILBOX_PAYLOAD_FIELDS_COUNT;
	request->service_version = (packed_header >> 12) & 0xFF;
	request->request_type = (packed_header >> 20) & 0xFF;
}

static inline uint32_t mailbox_pack_reply_header(const mailbox_reply_t *reply)
{
	uint32_t packed_reply_header = 0;

	packed_reply_header |= reply->service_id;
	packed_reply_header |= (uint32_t)(reply->payload.size << 8);
	packed_reply_header |= (uint32_t)(reply->status.service_error << 12);
	packed_reply_header |= (uint32_t)(reply->status.driver_error << 24);

	return packed_reply_header;
}

static inline void mailbox_unpack_reply_header(const uint32_t packed_header,
	mailbox_reply_t *reply)
{
	reply->service_id = packed_header & 0xFF;
	reply->payload.size = (packed_header >> 8) & MAILBOX_PAYLOAD_FIELDS_COUNT;
	reply->status.service_error = (packed_header >> 12) & 0xFFF;
	reply->status.driver_error = (packed_header >> 24) & 0xFF;
}

#endif /* __MTK_ISE_MAILBOX_H__ */
