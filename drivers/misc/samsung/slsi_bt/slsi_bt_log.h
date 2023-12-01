/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth Uart Driver                                                      *
 *                                                                            *
 ******************************************************************************/
#ifndef __SLSI_BT_LOG_H__
#define __SLSI_BT_LOG_H__

#ifdef CONFIG_KUNIT_SLSI_BT_TEST
#include "mock/mock_slsi_bt_log.h"
#endif
#ifndef BT_LOG
#include <scsc/scsc_logring.h>
#endif

#define SLSI_BT_LOG_TRACE_TRANSPORT
#define SLSI_BT_LOG_TRACE_DATA_HEX

/* Logging porting */
#ifndef BT_LOG
#define BT_LOG
#define BT_INFO(...)            SCSC_TAG_INFO(BT_COMMON, __VA_ARGS__)
#define BT_DBG(...)             SCSC_TAG_INFO(BT_COMMON, __VA_ARGS__)
#define BT_WARNING(...)	        SCSC_TAG_WARNING(BT_COMMON, __VA_ARGS__)
#define BT_ERR(...)	        SCSC_TAG_ERR(BT_COMMON, __VA_ARGS__)

#define TR_INFO(...)            SCSC_TAG_INFO(BT_H4, __VA_ARGS__)
#define TR_DBG(...)             SCSC_TAG_INFO(BT_H4, __VA_ARGS__)
#define TR_WARNING(...)	        SCSC_TAG_WARNING(BT_H4, __VA_ARGS__)
#define TR_ERR(...)	        SCSC_TAG_ERR(BT_H4, __VA_ARGS__)

/* Tracing Transport elements */
#ifdef SLSI_BT_LOG_TRACE_TRANSPORT
#define BTTR_SEND(...)          SCSC_TAG_DEBUG(BT_TX, __VA_ARGS__)
#define BTTR_RECV(...)          SCSC_TAG_DEBUG(BT_RX, __VA_ARGS__)
#else
#define BTTR_SEND(...)
#define BTTR_RECV(...)
#endif
#endif /* BT_LOG */

#define BTTR_TAG_USR_TX         0x10
#define BTTR_TAG_USR_RX         0x11
#define BTTR_TAG_H4_TX          0x20
#define BTTR_TAG_H4_RX          0x21
#define BTTR_TAG_BCSP_TX        0x40
#define BTTR_TAG_BCSP_RX        0x41
#define BTTR_TAG_SLIP_TX        0x80
#define BTTR_TAG_SLIP_RX        0x81

#define BTTR_LOG_FILTER_DEFAULT 0xE0

/* Print data hex */
#ifdef SLSI_BT_LOG_TRACE_DATA_HEX
#define SLSI_BT_LOG_DATA_BUF_MAX	90
void slsi_bt_log_data_hex(const char *func, unsigned int tag,
		const unsigned char *data, size_t len);
#else
#define slsi_bt_log_data_hex(...)
#endif

struct firm_log_filter {
	union {
		unsigned long long             uint64[2];
		unsigned int                   uint32[4];
		struct {
			unsigned int           en0_l;
			unsigned int           en0_h;
			unsigned int           en1_l;
			unsigned int           en1_h;
		};
	};
};

struct firm_log_filter slsi_bt_log_filter_get(void);
void slsi_bt_log_init(void *htr);
void slsi_bt_log_deinit(void);

/* MXLOG_LOG_EVENT_IND HANDLER */
void slsi_bt_mxlog_log_event(const unsigned char len, const unsigned char *);

#endif /* __SLSI_BT_LOG_H__ */
