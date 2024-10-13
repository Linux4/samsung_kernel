/*
 * sound/soc/sprd/dai/sprd-compr.h
 *
 * SPRD-COMPR -- SpreadTrum Compress Sound Plateform.
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY ork FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRD_COMPR_H
#define __SPRD_COMPR_H

enum {
	COMPR_CMD_OPEN = 100,
	COMPR_CMD_CLOSE,
	COMPR_CMD_START,
	COMPR_CMD_STOP,
	COMPR_CMD_PAUSE_PUSH,
	COMPR_CMD_PAUSE_RELEASE,
	COMPR_CMD_TRIGGER,
	COMPR_CMD_RESET,
	COMPR_CMD_PARAMS,
	COMPR_CMD_POINTER,
	COMPR_CMD_FLUSH,
	COMPR_CMD_DRAIN,
};


#define COMPR_DATA_PCM						0x00000020
#define COMPR_DATA_PCM_RET					0x00000040
#define COMPR_DATA_SILENCE					0x00000080

#define PLAYBACK_DATA_ABORT					0x00000001
#define PLAYBACK_DATA_BREAK					0x00000002

#define COMPR_DEV_CTRL_ABORT				0x00000001
#define COMPR_DEV_CTRL_BREAK				0x00000002

#define COMPR_SUBCMD_PLAYBACK				0x0
#define COMPR_SUBCMD_CAPTURE				0x1
#define COMPR_SUBCMD_HANDSHAKE				0x2

#define FORMAT_MP3 							0x1
#define FORMAT_AAC 							0x2

#define SPRD_COMPR_CARD_NAME_LEN_MAX		32

struct cmd_common {
	uint32_t command;
	uint32_t sub_cmd;
	uint32_t reserved1;
	uint32_t reserved2;
};

struct cmd_pointer {
	struct cmd_common common;
	uint32_t low;	/* rate in Hz */
};

struct cmd_prepare {
	struct cmd_common common;
	uint32_t rate;	/* rate in Hz */
	uint32_t samplerate;
	uint32_t mcdt_chn;
	uint32_t channels;	/* channels */
	uint32_t format;
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t period;	/* period size */
	uint32_t periods;	/* periods */
	uint32_t info_paddr;
	uint32_t info_size;
};

enum snd_status {
	COMPR_IDLE = 0,
	COMPR_OPENNED,
	COMPR_CLOSED,
	COMPR_STOPPED,
	COMPR_PARAMSED,
	COMPR_TRIGGERED,
	COMPR_PARAMIZED,
	COMPR_ABORT,
};

struct sprd_compr_dev_ctrl {
	uint32_t dev_state;
	struct mutex mutex;
	uint32_t dst;
	uint32_t ctrl_channel;
	uint32_t monitor_channel;
	uint8_t name[SPRD_COMPR_CARD_NAME_LEN_MAX];
};

struct sprd_compr_playinfo {
	uint32_t uiTotalTime;
	uint32_t uiCurrentTime;
	uint32_t uiTotalDataLength;
	uint32_t uiCurrentDataOffset;
};

#endif /* __SPRD_COMPR_H */
