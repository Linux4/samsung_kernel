/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __MTK_ISE_MBOX_H__
#define __MTK_ISE_MBOX_H__

#define MAILBOX_SUCCESS			0
#define MAILBOX_INVALID_PARAMETER	1
#define MAILBOX_INVALID_SIZE		2
#define MAILBOX_INVALID_SAFETY		3
#define MAILBOX_TIMEOUT			4

#define MAILBOX_PAYLOAD_SIZE_BIT_LENGTH	4u
#define MAILBOX_PAYLOAD_FIELDS_COUNT	0xFu

typedef enum {
	REQUEST_ISE,
	REQUEST_EVITA,
	REQUEST_PKCS11,
	REQUEST_CUSTOM,
	REQUEST_TRUSTY,
	REQUEST_LOWPOWER,
	REQUEST_RPMB
} request_type_enum;

typedef enum {
	TRUSTY_SR_ID_UT = 1,
	TRUSTY_SR_ID_SMC
} trusty_protocol_id_enum;

#define TRUSTY_SR_VER_UT	1
#define TRUSTY_SR_VER_SMC	1

typedef union {
	struct {
		unsigned int service_error:12;
		unsigned int driver_error:8;
		int _reserved:12;
	};

	uint32_t error;
} scz_error_t;

typedef struct {
	unsigned int size:MAILBOX_PAYLOAD_SIZE_BIT_LENGTH;
	uint32_t fields[MAILBOX_PAYLOAD_FIELDS_COUNT];
} mailbox_payload_t;

typedef struct {
	request_type_enum request_type;
	uint8_t service_id;
	uint8_t service_version;
	mailbox_payload_t payload;
} mailbox_request_t;

typedef struct {
	scz_error_t status;
	uint8_t service_id;
	mailbox_payload_t payload;
} mailbox_reply_t;

mailbox_reply_t ise_mailbox_request(mailbox_request_t *request,
	mailbox_payload_t *payload, request_type_enum request_type,
	uint8_t service_id, uint8_t service_version);

#endif /* __MTK_ISE_MBOX_H__ */
