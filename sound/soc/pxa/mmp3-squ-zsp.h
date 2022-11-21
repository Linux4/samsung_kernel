/*
 * linux/sound/soc/pxa/mmp3-squ-zsp.h
 *
 * Base on linux/sound/soc/pxa/mmp-squ-zsp.h
 *
 * Copyright (C) 2007 Marvell International Ltd.
 * Author: Libin Yang <lbyang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _MMP3_SQU_ZSP_H
#define _MMP3_SQU_ZSP_H

#include "mmp-zsp-audio.h"
struct zsp_buffer {
	void *buf;
	dma_addr_t buf_phys;
	u32 zsp_offset;
	u32 app_offset;
	int buf_len;
	int zsp_period_bytes;
};

struct mmp_zsp_runtime_data {
	struct snd_pcm_substream *substream;
	void *zmq_deliver;
	int render_id;
	int stream_id;
	int fmbuf_len;
	int elapsed_bytes;
	struct zsp_buffer zsp_buf;
	struct workqueue_struct *zmq_workqueue;
	struct work_struct zsp_queue;
	struct completion zmq_cmd_completion;
	adma_config_t zsp_adma_conf;
#define ZMQ_OPENED 1
#define	ZMQ_CLOSED 0
#define	ZMQ_SUSPENDED 2
	int zmq_state;
	atomic_t zmq_sync;
	struct mutex zmq_cmd_mutex;
	struct mutex zsp_call_mutex;
};


#endif
