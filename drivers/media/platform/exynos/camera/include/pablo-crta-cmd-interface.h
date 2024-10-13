/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CRTA_CMD_INTERFACE_H
#define IS_CRTA_CMD_INTERFACE_H

#define PABLO_CRTA_MAGIC_NUMBER		0xCA220114

/* STAT_REG_MAX		(400) */
#define PDP_CR_NUM			400
/* CSTAT_REG_CNT + (CSTAT_LUT_REG_CNT * CSTAT_LUT_NUM) */
#define CSTAT_CR_NUM			4972

#define SENSOR_CONTROL_MAX		100
#define EXPOSURE_NUM_MAX		3
#define SEAMLESS_MODE_MAX		10

/* message */
#define MSG_INSTANCE_ID_MASK		0xF0000000
#define MSG_INSTANCE_ID_SHIFT			28
#define MSG_TYPE_MASK			0x0000F000
#define MSG_TYPE_SHIFT				12
#define MSG_COMMAND_ID_MASK		0x00000FF0
#define MSG_COMMAND_ID_SHIFT			 4
#define MSG_RESPONSE_ID_MASK		0x0000000F
#define MSG_RESPONSE_ID_SHIFT			 0

#define DVA_HIGH_MASK		0x0000000FFFFFFFF0
#define DVA_HIGH_SHIFT				 4
#define DVA_LOW_MASK		0x000000000000000F
#define DVA_LOW_SHIFT				 0

#define BUF_IDX_MASK			0xFFFF0000
#define BUF_IDX_SHIFT				16
#define BUF_TYPE_MASK			0x0000FFFF
#define BUF_TYPE_SHIFT				 0

enum pablo_message_type {
	PABLO_MESSAGE_COMMAND,
	PABLO_MESSAGE_RESPONSE,
	PABLO_MESSAGE_MAX,
};

enum pablo_hic_cmd_id {
	PABLO_HIC_OPEN,
	PABLO_HIC_PUT_BUF,
	PABLO_HIC_SET_SHARED_BUF_IDX,
	PABLO_HIC_START,
	PABLO_HIC_SHOT,
	PABLO_HIC_CSTAT_FRAME_START,
	PABLO_HIC_CSTAT_CDAF_END,
	PABLO_HIC_PDP_STAT0_END,
	PABLO_HIC_PDP_STAT1_END,
	PABLO_HIC_CSTAT_FRAME_END,
	PABLO_HIC_STOP,
	PABLO_HIC_CLOSE,
	PABLO_HIC_MAX
};

enum pablo_ihc_cmd_id {
	PABLO_IHC_CONTROL_SENSOR,
	PABLO_IHC_MAX
};

enum pablo_response_id {
	PABLO_RSP_SUCCESS,
	PABLO_RSP_FAIL,
	PABLO_RSP_MAX,
};

/* OPEN CMD */
enum pablo_frame_type {
	/* Sensor HDR mode frame types */
	PABLO_FRAME_HDR_SINGLE,
	PABLO_FRAME_HDR_LONG,
	PABLO_FRAME_HDR_SHORT,
};

/* PUT_BUFFER CMD */
enum pablo_buffer_type {
	PABLO_BUFFER_RTA,
	PABLO_BUFFER_SENSOR_CONTROL,
	PABLO_BUFFER_STATIC,
	PABLO_BUFFER_MAX,
};

/* STOP CMD */
enum pablo_stop_type {
	PABLO_STOP_IMMEDIATE,
	PABLO_STOP_SUSPEND,
};

#endif /* IS_CRTA_CMD_INTERFACE_H */
