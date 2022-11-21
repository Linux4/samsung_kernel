/*
 * sound/soc/sprd/dai/sprd-dmaengine-pcm.h
 *
 * SpreadTrum DMA for the pcm stream.
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SPRD_DMA_ENGINE_PCM_H
#define __SPRD_DMA_ENGINE_PCM_H

//#include <mach/dma.h>
//#include <sprd_dma.h>
#include <../../../drivers/dma/sprd_dma.h>
#include <linux/dmaengine.h>


#define VBC_PCM_FORMATS			SNDRV_PCM_FMTBIT_S16_LE
#define VBC_FIFO_FRAME_NUM	  	(160)
#define VBC_BUFFER_BYTES_MAX 		(128 * 1024)

#define I2S_BUFFER_BYTES_MAX 		(64 * 1024)

#define AUDIO_BUFFER_BYTES_MAX 		(VBC_BUFFER_BYTES_MAX + I2S_BUFFER_BYTES_MAX)

struct sprd_pcm_dma_params {
	char *name;		/* stream identifier */
	int channels[2];	/* channel id */
	int irq_type;		/* dma interrupt type */
	struct sprd_dma_cfg desc;	/* dma description struct */
	u32 dev_paddr[2];	/* device physical address for DMA */
};

static inline u32 sprd_pcm_dma_get_addr(struct dma_chan *dma_chn,
					dma_cookie_t cookie, struct snd_pcm_substream *substream)
{
	struct dma_tx_state dma_state;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_state.residue = SPRD_SRC_ADDR;
	} else {
		dma_state.residue = SPRD_DST_ADDR;
	}
	dmaengine_tx_status(dma_chn, cookie, &dma_state);
	//printk(KERN_ERR "%s, residue:0x%x \n",__func__,dma_state.residue);
	return dma_state.residue;
}

#endif /* __SPRD_DMA_ENGINE_PCM_H */
