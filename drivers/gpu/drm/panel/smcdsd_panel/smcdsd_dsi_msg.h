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

enum {
	/* bit number for flow control modes */
	MSG_MODE_BLOCKING,
	MSG_MODE_SINGLE_TRANSFER,
};

#define MSG_MODE_SEND_MASK	(BIT(MSG_MODE_BLOCKING) | BIT(MSG_MODE_SINGLE_TRANSFER))

struct msg_segment {
	struct mipi_dsi_msg dsi_msg;
	char *msg_name;	/* tx_buf */
	int delay;

	unsigned int modes;
};

struct msg_package {
	void *buf;
	int len;
};

//#define PACK(...) (__VA_ARGS__)
#define UNPACK(...) __VA_ARGS__

#define __MSG_TX(_name, ...)	.dsi_msg.tx_buf = (void *)_name, .dsi_msg.tx_len = ARRAY_SIZE(_name), .msg_name = #_name
#define __MSLEEP(_msec)		.delay = ((_msec) * USEC_PER_MSEC)
#define __USLEEP(_usec)		.delay = (_usec)
#define __ADDRESS(_name)	.buf = (void *)_name, .len = ARRAY_SIZE(_name)

#define ENUM_START(_name)		ENUM_##_name##_START,	ENUM_##_name##_START_DUMMY = ENUM_##_name##_START - 1,
#define ENUM_END(_name)			ENUM_##_name##_END,	ENUM_##_name##_SIZE = ENUM_##_name##_END - ENUM_##_name##_START, ENUM_##_name##_END_DUMMY = ENUM_##_name##_END - 1,

#define ENUM_MULTI(_name, _list)	ENUM_START(_name)	ENUM_##_name = ENUM_##_name##_START, ENUM_##_name##_DUMMY = ENUM_##_name##_START - 1, _list(__XX_ENUM)	ENUM_END(_name)
#define ENUM_ALONE(_name)		ENUM_START(_name)	ENUM_##_name = ENUM_##_name##_START, ENUM_END(_name)
#define ENUM_MACRO(_1, _2, _N, ...)	_N
#define ENUM_APPEND(...)		ENUM_MACRO(__VA_ARGS__, ENUM_MULTI, ENUM_ALONE)(__VA_ARGS__)

#define GET_ENUM_WITH_NAME(_name)	ENUM_##_name

#define ___XX_TO_CMDX(_name, _cmdx...)		static unsigned char _name[] = { UNPACK _cmdx };
#define __XX_TO_CMDX(_name1, _name2, _cmdx, ...)	___XX_TO_CMDX(_name1##_##_name2, _cmdx)
#define __XX_MAKE_CMDX(__XX_INFO)		__XX_INFO(__XX_TO_CMDX)

#define ___XX_ENUM(_name, ...)			ENUM_ALONE(_name)
#define __XX_ENUM(_name1, _name2, ...)		___XX_ENUM(_name1##_##_name2, ...)

#define ___XX_MSGX(_name, _msgx...)		{ _msgx },
#define __XX_MSGX(_name1, _name2, _msgx...)	___XX_MSGX(_name1##_##_name2, _msgx)

#define ___XX_CMDX(_name, _cmdx, _msgx...)	{ __MSG_TX(_name), _msgx },
#define __XX_CMDX(_name1, _name2, _cmdx, _msgx...)	___XX_CMDX(_name1##_##_name2, _cmdx, _msgx)

#define ADDRESS(_base, _name)			[ENUM_##_name##_START] = { .buf = (void *)&_base[ENUM_##_name##_START], .len = ENUM_##_name##_SIZE },
#define __XX_ADDRESS(_name1, _name2, ...)	ADDRESS(_name1, _name1##_##_name2)

extern void dump_dsi_msg_tx(unsigned long data0);
extern int send_msg_segment(void *msg, int len);
extern int send_msg_package(void *src, int len, void *dst);
extern int send_cmd(unsigned char *cmd, int len);

extern int MAX_TRANSFER_NUM;

#endif

