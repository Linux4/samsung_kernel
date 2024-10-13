/*
 * include/sound/audio-sipc.h
 *
 * SPRD SoC VBC -- SpreadTrum SOC SIPC for audio Common function.
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
#ifndef __AUDIO_SIPC_H
#define __AUDIO_SIPC_H

#include <sound/audio-smsg.h>
#include <sound/vbc-utils.h>


#define AUDIO_SMSG_TXBUF_ADDR		(0)
#define AUDIO_SMSG_TXBUF_SIZE		(SZ_1K)
#define AUDIO_SMSG_RXBUF_ADDR		(AUDIO_SMSG_TXBUF_SIZE)
#define AUDIO_SMSG_RXBUF_SIZE		(SZ_1K)
#define AUDIO_SMSG_RINGHDR		(AUDIO_SMSG_TXBUF_SIZE + AUDIO_SMSG_RXBUF_SIZE)
#define AUDIO_SMSG_TXBUF_RDPTR	(AUDIO_SMSG_RINGHDR + 0)
#define AUDIO_SMSG_TXBUF_WRPTR	(AUDIO_SMSG_RINGHDR + 4)
#define AUDIO_SMSG_RXBUF_RDPTR	(AUDIO_SMSG_RINGHDR + 8)
#define AUDIO_SMSG_RXBUF_WRPTR	(AUDIO_SMSG_RINGHDR + 12)
/*add by ninglei*/
#define AUDIO_SMSG_RINGHDR_EXT_SIZE    (16)
#define AUDIO_SMSG_RINGHDR_EXT_TXRDPTR (0)
#define AUDIO_SMSG_RINGHDR_EXT_TXWRPTR (4)
#define AUDIO_SMSG_RINGHDR_EXT_RXRDPTR (8)
#define AUDIO_SMSG_RINGHDR_EXT_RXWRPTR (12)

#define AUDIO_SIPC_WAIT_FOREVER	(-1)

struct audio_sipc_of_data {
	char * name;
	uint32_t cmdaddr_base;	/* address of cmd-para in iram */
	uint32_t cmdaddr_size;	/* size of the buffer for storing cmd-para in iram */
	uint32_t smsg_txbase;	/* address of smsg tx buffer */
	uint32_t smsg_txsize;	/* size of smsg tx buffer */
	uint32_t smsg_rxsize;	/* size of smsg rx buffer */
	uint32_t target_id;		/* AGCP id:5 */
	uint32_t shm_base;		/* address of shm in iram */
	uint32_t shm_size[SND_VBC_SHM_MAX];	/* size of each type in shm buf */
};

struct audio_cmd_buf {
	size_t		txbuf_addr_v;	/* start virtual address in iram of command para txbuf */
	size_t		txbuf_addr_p;	/* start physical address in iram of command para txbuf */
	uint32_t	txbuf_size;		/* byte size of txbuf */
	spinlock_t	txpinlock;		/* lock for send-buffer */
};

struct audio_shm_buf {
	size_t		audio_shm_addr_v[SND_VBC_SHM_MAX];
	size_t		audio_shm_addr_p[SND_VBC_SHM_MAX];
	uint32_t	audio_shm_size[SND_VBC_SHM_MAX];
	spinlock_t	txpinlock;		/* lock for send-buffer */
};

struct sipc_iram_buf {
	struct audio_cmd_buf cmd_iram_buf;	/* this area is for cmd para in iram */
	struct audio_shm_buf shm_iram_buf;	/* this area is for share memory in iram */
};

int snd_vbc_sipc_init(struct audio_sipc_of_data *node_info);
void snd_vbc_sipc_deinit(void);
int snd_audio_sipc_ch_open(uint8_t dst, uint16_t channel, int timeout);
int snd_audio_sipc_ch_close(uint8_t dst, uint16_t channel, int timeout);
int snd_audio_send_cmd(uint16_t channel, int id, int stream,
			uint32_t cmd, void *para, size_t n, int32_t timeout);
int snd_audio_send_cmd_norecev(uint16_t channel, int id, int stream,
			uint32_t cmd, void *para, size_t n, int32_t timeout);
int snd_audio_recv_cmd(uint16_t channel, uint32_t cmd, uint32_t *buf, int32_t timeout);
int snd_audio_send_buffer(uint16_t channel, int id, int stream,
			int type, void *buf, size_t n, int32_t timeout);
int snd_audio_recv_buffer(uint16_t channel, int id, int stream,
			int type, struct sipc_iram_buf **buf_out, int32_t timeout);

#endif /* __AUDIO_SIPC_H */


