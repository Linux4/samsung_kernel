/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SMCDSD_DSI_MSG_H__
#define __SMCDSD_DSI_MSG_H__

#include <drm/drm_mipi_dsi.h>
#include <drm/drmP.h>
#include "../../mediatek/mtk_panel_ext.h"

enum {
	MIPI_DCS_WRITE_SIDE_RAM_START = 0x4C,
	MIPI_DCS_WRITE_SIDE_RAM_CONTINUE = 0x5C,

	MTK_DSI_MAX_TX_FIFO = 510,
	SRAM_BYTE_ALIGN = 16,

	MAX_SEGMENT_NUM = (MAX_TX_CMD_NUM * 2),

	MAX_RX_READ_NUM = RT_MAX_NUM,
};

enum {
	/* bit number for flow control modes */
	MSG_MODE_BLOCKING,
	MSG_MODE_SINGLE_TRANSFER,
};

#define MSG_MODE_SEND_MASK	(BIT(MSG_MODE_BLOCKING) | BIT(MSG_MODE_SINGLE_TRANSFER))

struct msg_ndarray {
	unsigned char *data;
	int ndim;
	int shape[5];
	int jump[5+1];
};

struct msg_segment {
	struct mipi_dsi_msg dsi_msg;
	char *msg_name;	/* tx_buf */
	int delay;

	unsigned int modes;

	union {
		struct msg_ndarray *ndarray;
		void (*cb)(int value);
	};
};

struct msg_package {
	void *buf;
	int len;
};

#define RANGE_LENGTH(_val)	(((0?_val) - (1?_val) + 1) + (BUILD_BUG_ON_ZERO((1?_val) > (0?_val))))

#define GET_PACKAGE_5(_base, _1, _2, _3, _4)	get_patched_package(_base, _1, _2, _3, _4)
#define GET_PACKAGE_4(_base, _1, _2, _3)	get_patched_package(_base, _1, _2, _3, 0)
#define GET_PACKAGE_3(_base, _1, _2)		get_patched_package(_base, _1, _2, 0, 0)
#define GET_PACKAGE_2(_base, _1)		get_patched_package(_base, _1, 0, 0, 0)
#define GET_PACKAGE_1(_base)			_base
#define GET_PACKAGE(...)			CONCATENATE(GET_PACKAGE_, COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define DEFINE_NDARRAY_5(_name, _1, _2, _3, _4)	static unsigned char _name##_NDARRAY_DATA[_1][_2][_3][_4];	static struct msg_ndarray _name##_NDARRAY = { .ndim = 4, .data = (char *)_name##_NDARRAY_DATA, .jump = { 0, (_4)*(_3)*(_2), (_4)*(_3), (_4) },	};	static unsigned char _name##_NDARRAY_DATA[_1][_2][_3][_4]
#define DEFINE_NDARRAY_4(_name, _1, _2, _3)	static unsigned char _name##_NDARRAY_DATA[_1][_2][_3];		static struct msg_ndarray _name##_NDARRAY = { .ndim = 3, .data = (char *)_name##_NDARRAY_DATA, .jump = { 0, (_3)*(_2), (_3), },		 	};	static unsigned char _name##_NDARRAY_DATA[_1][_2][_3]
#define DEFINE_NDARRAY_3(_name, _1, _2)		static unsigned char _name##_NDARRAY_DATA[_1][_2];		static struct msg_ndarray _name##_NDARRAY = { .ndim = 2, .data = (char *)_name##_NDARRAY_DATA, .jump = { 0, (_2), },				};	static unsigned char _name##_NDARRAY_DATA[_1][_2]
#define DEFINE_NDARRAY(...)			CONCATENATE(DEFINE_NDARRAY_, COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define DEFINE_POINT_GPARA(_reg, _th)	static unsigned char RX_GPARA_##_reg##_##_th[] = { 0xB0 + BUILD_BUG_ON_ZERO(_th <= 0), ((_th - 1) & 0xff00) >> 8, (_th - 1) & 0xff, 0x##_reg }
#define DEFINE_RXBUF(_reg, _len)	static unsigned char RX_BUF_##_reg[_len + 1] = { 0x##_reg, };	/* DEFINE_RXBUF(1A, 16) = static unsigned char RX_BUF_1A[16 + 1] = {0x1A, }; */

#define MSG_RX_2(_name, _val)		.dsi_msg.type = MIPI_DSI_DCS_READ, .dsi_msg.tx_len = 1, .dsi_msg.tx_buf = &_name[0], .dsi_msg.rx_len = RANGE_LENGTH(_val) + (BUILD_BUG_ON_ZERO(((0?_val) - (1?_val) + 1) > MAX_RX_READ_NUM)), .dsi_msg.rx_buf = &_name[(1?_val)]
#define MSG_RX_1(_name)			.dsi_msg.type = MIPI_DSI_DCS_READ, .dsi_msg.tx_len = 1, .dsi_msg.tx_buf = &_name[0], .dsi_msg.rx_buf = &_name[1]
#define MSG_RX(...)			CONCATENATE(MSG_RX_, COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define MSG_TX_2(_name, _size)		.dsi_msg.tx_buf = (void *)_name, .dsi_msg.tx_len = _size, .msg_name = #_name
#define MSG_TX_1(_name)			.dsi_msg.tx_buf = (void *)_name, .dsi_msg.tx_len = ARRAY_SIZE(_name), .msg_name = #_name
#define MSG_TX(...)			CONCATENATE(MSG_TX_, COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define MSG_MSLEEP(_msec)		.delay = ((_msec) * USEC_PER_MSEC)
#define MSG_USLEEP(_usec)		.delay = (_usec)

#define ENUM_START(_name)		ENUM_##_name##_START,	ENUM_##_name##_START_DUMMY = ENUM_##_name##_START - 1,
#define ENUM_END(_name)			ENUM_##_name##_END,	ENUM_##_name##_SIZE = ENUM_##_name##_END - ENUM_##_name##_START, ENUM_##_name##_END_DUMMY = ENUM_##_name##_END - 1,

#define ENUM_MULTI(_name, _list)	ENUM_START(_name)	ENUM_##_name = ENUM_##_name##_START, ENUM_##_name##_DUMMY = ENUM_##_name##_START - 1, _list(__XX_ENUM)	ENUM_END(_name)
#define ENUM_ALONE(_name)		ENUM_START(_name)	ENUM_##_name = ENUM_##_name##_START, ENUM_END(_name)
#define ENUM_MACRO(_1, _2, _N, ...)	_N
#define ENUM_APPEND(...)		ENUM_MACRO(__VA_ARGS__, ENUM_MULTI, ENUM_ALONE)(__VA_ARGS__)

#define ADDRESS_2(_name, _size)		.buf = (void *)&_name, .len = _size
#define ADDRESS_1(_name)		.buf = (void *)&_name, .len = ARRAY_SIZE(_name)
#define ADDRESS(...)			CONCATENATE(ADDRESS_, COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define GET_ENUM_WITH_NAME(_name)	ENUM_##_name

#define for_each_msg_in_segment(__idx, __segment, __end, __dsi_msg)	\
	for ((__idx) = 0; (__idx) < (__end) && ((__dsi_msg) = &((__segment)[__idx].dsi_msg)); (__idx)++)	\

#define for_each_rx_msg_in_segment(__idx, __segment, __end, __dsi_msg)	\
	for_each_msg_in_segment(__idx, __segment, __end, __dsi_msg)	\
		for_each_if((__dsi_msg)->rx_buf)

#define for_each_dsi_msg_in_package(__package, __segment, __idx, __dsi_msg)	\
	for ((__idx) = 0; __idx < (__package)->len && \
		((__segment) = &((struct msg_segment *)(__package)->buf)[__idx]); (__idx)++)	\
		for_each_if (__segment && (__dsi_msg = __segment))	\

extern void dump_dsi_msg_tx(unsigned long data0);
extern int send_msg_segment(void *msg, int len);
extern int send_msg_package(void *src, int len, void *dst);
extern int send_cmd(unsigned char *cmd, int len);
extern struct msg_package *get_patched_package(struct msg_package *base, int n1, int n2, int n3, int n4);

extern int MAX_TRANSFER_NUM;

#endif

