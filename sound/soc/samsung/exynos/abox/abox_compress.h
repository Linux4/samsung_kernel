/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ALSA SoC - Samsung Abox Compress Header
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_COMPRESS_H
#define __SND_SOC_ABOX_COMPRESS_H

/* Mailbox between driver and firmware for offload */
#define COMPR_CMD_CODE		(0x0004)
#define COMPR_HANDLE_ID		(0x0008)
#define COMPR_IP_TYPE		(0x000C)
#define COMPR_SIZE_OF_FRAGMENT	(0x0010)
#define COMPR_PHY_ADDR_INBUF	(0x0014)
#define COMPR_SIZE_OF_INBUF	(0x0018)
#define COMPR_LEFT_VOL		(0x001C)
#define COMPR_RIGHT_VOL		(0x0020)
#define EFFECT_EXT_ON		(0x0024)
#define COMPR_ALPA_NOTI		(0x0028)
#define COMPR_STREAM_FORMAT	(0x002C)
#define COMPR_ENCODER_PADDING	(0x0030)
#define COMPR_ENCODER_DELAY	(0x0034)
#define COMPR_PARAM_SAMPLE	(0x0038)
#define COMPR_PARAM_CH		(0x003C)
#define COMPR_RENDERED_PCM_SIZE	(0x004C)
#define COMPR_RETURN_CMD	(0x0040)
#define COMPR_IP_ID		(0x0044)
#define COMPR_SIZE_OUT_DATA	(0x0048)
#define COMPR_UPSCALE		(0x0050)
#define COMPR_CPU_LOCK_LV	(0x0054)
#define COMPR_CHECK_CMD		(0x0058)
#define COMPR_CHECK_RUNNING	(0x005C)
#define COMPR_ACK		(0x0060)
#define COMPR_INTR_ACK		(0x0064)
#define COMPR_INTR_DMA_ACK	(0x0068)
#define COMPR_MAX		COMPR_INTR_DMA_ACK

/* Interrupt type */
#define INTR_WAKEUP		(0x0)
#define INTR_READY		(0x1000)
#define INTR_DMA		(0x2000)
#define INTR_CREATED		(0x3000)
#define INTR_DECODED		(0x4000)
#define INTR_RENDERED		(0x5000)
#define INTR_FLUSH		(0x6000)
#define INTR_PAUSED		(0x6001)
#define INTR_EOS		(0x7000)
#define INTR_DESTROY		(0x8000)
#define INTR_FX_EXT		(0x9000)
#define INTR_EFF_REQUEST	(0xA000)
#define INTR_SET_CPU_LOCK	(0xC000)
#define INTR_FW_LOG		(0xFFFF)

#define COMPRESSED_LR_VOL_MAX_STEPS     0x2000

#define COMPR_CAPTURE_BASE_ID 120

enum OFFLOAD_CMDTYPE {
	/* OFFLOAD */
	CMD_COMPR_CREATE = 0x50,
	CMD_COMPR_DESTROY,
	CMD_COMPR_SET_PARAM,
	CMD_COMPR_WRITE,
	CMD_COMPR_READ,
	CMD_COMPR_START,
	CMD_COMPR_STOP,
	CMD_COMPR_PAUSE,
	CMD_COMPR_EOS,
	CMD_COMPR_GET_VOLUME,
	CMD_COMPR_SET_VOLUME,
	CMD_COMPR_CA5_WAKEUP,
	CMD_COMPR_HPDET_NOTIFY,
	CMD_COMPR_SET_METADATA,
};

enum OFFLOAD_IPTYPE {
	COMPR_MP3 = 0x0,
	COMPR_AAC = 0x1,
	COMPR_FLAC = 0x2,
};

enum OFFLOAD_STREAM_FORMAT {
	STREAM_FORMAT_DEFAULT,
	STREAM_FORMAT_ADTS,
};

struct abox_compr_data {
	/* compress offload */
	struct snd_compr_stream *cstream;

	void *dma_area;
	size_t dma_size;
	dma_addr_t dma_addr;
	unsigned long iova;

	unsigned int handle_id;
	unsigned int codec_id;
	unsigned int stream_format;
	unsigned int channels;
	unsigned int sample_rate;
	unsigned int bit_depth;

	unsigned int byte_offset;
	u64 copied_total;
	u64 received_total;
	u32 pcm_size;

	bool start;
	bool bespoke_start;
	bool dirty;

	atomic_t draining;

	struct completion flushed;
	struct completion destroyed;
	struct completion created;

	spinlock_t lock;
	struct mutex cmd_lock;

	int (*isr_handler)(void *data);

	struct snd_compr_params codec_param;
};

#endif /* __SND_SOC_ABOX_COMPRESS_H */
