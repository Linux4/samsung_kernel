/*
    Marvell Audio Stub driver for Linux
    Copyright (C) 2012 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef AUDIO_STUB_H_
#define AUDIO_STUB_H_

#include <linux/types.h>

struct atc_header {
	u8 cmd_code;
	u8 sub_cmd;
	u8 cmd_type;
	u8 data_len;
} __packed;

struct handshake_msg {
	u8 pcm_master; /* 0:codec is master, 1:cp is master */
	u8 is_wb;	/* 0:nb, 1:wb */
	u16 reserved;
	u32 ver;
	u32 msg_id;
} __packed;

struct volume_ctlmsg {
	u8 direction; /* 0:input, 1:output */
	u8 reserved[3];
	u32 gain;
	u32 misc_volume;
	u32 msg_id;
} __packed;

struct mute_ctlmsg {
	u8 direction; /* 0:input, 1:output */
	u8 mute;	/* 0:off, 1:on */
	u16 reserved;
	u32 msg_id;
} __packed;

struct path_ctlmsg {
	u32 path;
	u32 msg_id;
} __packed;

struct eq_ctlmsg {
	u16 reserved;
	u16 dha_mode;
	u32 dha_ch_flag;
	u8  dha_hearing_level[24];
	u32 msg_id;
} __packed;

struct loop_ctlmsg {
	u8 test_mode;	/* 0:off, 1:loopback pcm, 2:loopback packet */
	u8 reserved[3];
	u32 path;
	u32 msg_id;
} __packed;

struct pcm_record_ctlmsg {
	u8 on_off;	/* 0:off, 1:on */
	u8 near_far_end;	/*1:near, 2:far, 3:both */
	u8 near_codec_vocoder;	/* 1:near codec, 2:near vocoder */
	u8 reserved;
	/* callback field not used by AP, however it must be
	 * different for playback and capture(record) stream
	 * since CP uses this field as stream identifier.
	 */
	u32 callback;
	u32 msg_id;
} __packed;

struct pcm_playback_ctlmsg {
	u8 on_off;	/* 0:off, 1:on */
	u8 near_far_end;	/* 1:near, 2:far, 3:both */
	u8 near_codec_vocoder;	/* 1:near codec, 2:near vocoder */
	u8 comb_with_call;	/* 0:not combined, 1:combined */
	/* callback field not used by AP, however it must be
	 * different for playback and capture(record) stream
	 * since CP uses this field as stream identifier.
	 */
	u32 callback;
	u32 msg_id;
} __packed;

struct response_msg {
	u32 status;
	u32 msg_id;
} __packed;

struct pcm_stream_ind {
	u32 callback;
	u32 msg_id;
} __packed;

struct pcm_stream_data {
	u32 callback;
	u32 msg_id;
	u32 len;
	u8 data[0];
} __packed;

#define ATC_HANDSHAKE 0x0
#define ATC_VOLUMECTL 0x1
#define ATC_MUTECTL 0x2
#define ATC_PATHCTL 0x3
#define ATC_EQCTL 0x4
#define ATC_LOOPBACKCTL 0x5
#define ATC_PCMRECCTL 0x6
#define ATC_PCMPLAYBACKCTL 0x7
#define ATC_PCMRECSTREAM 0x8
#define ATC_PCMPLAYSTREAM 0x9
#define ATC_MSOCKET_LINKDOWN 0xfd
#define ATC_MSOCKET_LINKUP 0xfe
#define ATC_INVALIDMSG 0xff

#define AUDIODRV_MAGIC 'e'
#define AUDIOSTUB_GET_STATUS _IOR(AUDIODRV_MAGIC, 0x1, int)
#define AUDIOSTUB_GET_WRITECNT _IOR(AUDIODRV_MAGIC, 0x2, u32)
#define AUDIOSTUB_GET_READCNT _IOR(AUDIODRV_MAGIC, 0x3, u32)
#define AUDIOSTUB_SET_PKTSIZE _IOW(AUDIODRV_MAGIC, 0x4, u32)
#define AUDIOSTUB_SET_CALLSTART _IOW(AUDIODRV_MAGIC, 0x5, int)
#define AUDIOSTUB_PCMPLAYBACK_DRAIN _IOW(AUDIODRV_MAGIC, 0x6, u16)
#define AUDIOSTUB_VOLUMECTL _IOW(AUDIODRV_MAGIC, 0x10, struct volume_ctlmsg)
#define AUDIOSTUB_MUTECTL _IOW(AUDIODRV_MAGIC, 0x11, struct mute_ctlmsg)
#define AUDIOSTUB_PATHCTL _IOW(AUDIODRV_MAGIC, 0x12, struct path_ctlmsg)
#define AUDIOSTUB_EQCTL _IOW(AUDIODRV_MAGIC, 0x13, struct eq_ctlmsg)
#define AUDIOSTUB_LOOPBACKCTL _IOW(AUDIODRV_MAGIC, 0x14, struct loop_ctlmsg)
#define AUDIOSTUB_PCMRECCTL _IOW(AUDIODRV_MAGIC, 0x15, struct pcm_record_ctlmsg)
#define AUDIOSTUB_PCMPLAYBACKCTL _IOW(AUDIODRV_MAGIC, 0x16,\
					struct pcm_playback_ctlmsg)

/* AP to CP */
#define CMD_TYPE_EXECUTE 0x1
#define CMD_TYPE_RESPONSE 0x2

/* CP to AP */
#define CMD_TYPE_CONFIRM 0x1
#define CMD_TYPE_INDICATION 0x2

#define AUDIO_CMD_CODE 0x9

#define IS_WB(x) ((x) & 0x1)
#define IS_PCM_MASTER (((x) & 0x2) != 0)

#endif /* AUDIO_STUB_H_ */
