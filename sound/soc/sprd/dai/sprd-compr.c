/*
 *  sprd-compr.c - ASoC Spreadtrum Compress Platform driver
 *
 *  Copyright (C) 2010-2020 Spreadtrum Communications Inc.
 *  Author: yintang.ren
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("COMPR") fmt
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <sound/compress_driver.h>
#include <sound/compress_offload.h>
#include <linux/of.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

#include "sprd-compr.h"
#include "sprd-asoc-common.h"

#include "sprd-compr-util.c"
#include <soc/sprd/sprd_dma.h>
#include <../../../drivers/mcdt/mcdt_hw.h>
#include <sound/sprd_memcpy_ops.h>

#define ADEBUG() sp_asoc_pr_info("sprd-compr.c: %s, line: %d\n", __FUNCTION__, __LINE__)

#define DMA_LINKLIST_CFG_NODE_SIZE (sizeof(struct sprd_dma_cfg))

/* Default values used if user space does not set */
#define COMPR_PLAYBACK_MIN_FRAGMENT_SIZE	(8 * 1024)
#define COMPR_PLAYBACK_MAX_FRAGMENT_SIZE	(128 * 1024)
#define COMPR_PLAYBACK_MIN_NUM_FRAGMENTS	(4)
#define COMPR_PLAYBACK_MAX_NUM_FRAGMENTS	(16 * 4)

#define CMD_TIMEOUT							msecs_to_jiffies(5000)
#define DATA_TIMEOUT						msecs_to_jiffies(50)
#define CMD_MODEM_RESET_TIMEOUT				msecs_to_jiffies(10000)

/***********************************************************************************************
        |<-------------------------------------54K----------------------------------------->|
        |<---- For VBC---->|<-----------------SPRD_COMPR_DMA_TOTAL_SIZE--------------------|
                                       |<-------SPRD_COMPR_DMA_DATA_SIZE------->|<--------1K--------->|
                                                                                                                |<---DESC--->|<-info->|
************************************************************************************************/
#define SPRD_COMPR_INFO_SIZE 				(sizeof(struct sprd_compr_playinfo))
#define SPRD_COMPR_DMA_DESC_SIZE 			(1024 - SPRD_COMPR_INFO_SIZE)
#define SPRD_COMPR_DMA_DATA_SIZE 			(iram_size_dma)
#define SPRD_COMPR_TOTAL_SIZE 				(SPRD_COMPR_DMA_DATA_SIZE + SPRD_COMPR_DMA_DESC_SIZE + SPRD_COMPR_INFO_SIZE)

#define SPRD_COMPR_DMA_PADDR_TRAN(x)		(x - iram_ap_base + iram_agcp_base)

#define MCDT_CHN_COMPR						(0)
#define MCDT_EMPTY_WMK						(0)
#define MCDT_FULL_WMK						(512 - 1) /* only 9bits in register, could not set 512 */
#define MCDT_FIFO_SIZE						(512)

struct sprd_compr_dma_cb_data {
	struct snd_compr_stream *substream;
	struct dma_chan *dma_chn;
};

struct sprd_compr_dma_params {
	char *name;			/* stream identifier */
	int channels[4];	/* channel id */
	int irq_type;		/* dma interrupt type */
	int stage;			/* stage num of dma */
	uint32_t frag_len;
	uint32_t block_len;
	uint32_t tran_len;
	struct sprd_dma_cfg desc;	/* dma description struct, 2 stages dma */
};

struct sprd_compr_rtd {
	struct sprd_compr_dma_params params;
	struct dma_chan *dma_chn[2];
	struct sprd_compr_dma_cb_data *dma_cb_data;

	dma_addr_t dma_cfg_phy[2];
	void *dma_cfg_virt[2];
	struct dma_async_tx_descriptor *dma_tx_des[2];
	dma_cookie_t cookie[2];
	int int_pos_update[2];
	int hw_chan;
	int cb_called;
	int dma_stage;

	struct snd_compr_stream *cstream;
	struct snd_compr_caps compr_cap;
	struct snd_compr_codec_caps codec_caps;
	struct snd_compr_params codec_param;

	uint32_t info_paddr;
	uint64_t info_vaddr;
	uint32_t info_size;


	int stream_id;
	uint32_t stream_state;

	uint32_t codec;
	uint32_t buffer_paddr; /* physical address */
	char *buffer;
	uint32_t app_pointer;
	uint32_t buffer_size;

	uint32_t copied_total;
	uint32_t bytes_received;

	int sample_rate;
	uint32_t num_channels;

	uint32_t drain_ready;

	atomic_t start;
	atomic_t eos;
	atomic_t drain;
	atomic_t xrun;
	atomic_t pause;

	wait_queue_head_t eos_wait;
	wait_queue_head_t drain_wait;
	wait_queue_head_t flush_wait;

	int wake_locked;
	struct wake_lock wake_lock;
	spinlock_t lock;
	struct mutex mutex;

#ifdef CONFIG_SND_VERBOSE_PROCFS
	struct snd_info_entry *proc_info_entry;
#endif
};

struct sprd_compr_pdata {
	bool suspend;
	wait_queue_head_t point_wait;
	struct task_struct *thread;
	struct sprd_compr_dev_ctrl dev_ctrl;
};

static unsigned long iram_paddr;
static unsigned long iram_ap_base;
static unsigned long iram_agcp_base;
static unsigned long iram_size_total;
static unsigned long iram_size_dma;
static char *s_sprd_compr_dma_paddr;

static struct sprd_compr_dma_params sprd_compr_dma_param_out = {
	.name = "COMPR out",
	.irq_type = BLK_DONE,
	.frag_len = 512 * 4,
	.block_len = 512 * 4,
	.desc = {
		 .datawidth = SHORT_WIDTH,
		 .fragmens_len = 240,
		 .src_step = 4,
		 .des_step = 4,
		 },
};

static int sprd_compress_new(struct snd_soc_pcm_runtime *rtd);
static int sprd_platform_compr_trigger(struct snd_compr_stream *cstream, int cmd);

static DEFINE_MUTEX(sprd_compr_lock);

#define SPRD_IRAM_INFO_ALL_PHYS  0xffffff



/* start -- sprd-compr-dai functions */
static int sprd_compr_dai_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	sp_asoc_pr_dbg("%s\n", __func__);
	return 0;
}

static void sprd_compr_dai_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	sp_asoc_pr_dbg("%s\n", __func__);
}

static struct snd_soc_dai_ops sprd_compr_dai_ops = {
	.startup = sprd_compr_dai_startup,
	.shutdown = sprd_compr_dai_shutdown,
};

struct snd_soc_dai_driver sprd_compr_dai[] = {
	{
	 .id = 0,
	 .name = "compress-dai",
	 .compress_dai = 1,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,},
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,},
	 .ops = &sprd_compr_dai_ops,
	 },
	{
	 .id = 1,
	 .name = "compress-ad23-dai",
	 .compress_dai = 1,
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &sprd_compr_dai_ops,
	 },
};

static const struct snd_soc_component_driver sprd_compr_component = {
	.name = "sprd-compr-component",
};

/* end -- sprd-compr-dai functions */
int sprd_compr_mornitor_thread(void *data)
{
	struct sprd_compr_rtd *srtd = data;
	int ret = 0;
	while (1) {
		ret = wait_event_interruptible(srtd->drain_wait, srtd->drain_ready);
		msleep(500);
		sp_asoc_pr_info("%s: drain finished, ret=%d\n", __func__, ret);
		srtd->drain_ready = 0;
		mutex_lock(&srtd->cstream->device->lock);
		snd_compr_drain_notify(srtd->cstream);
		atomic_set(&srtd->drain, 0);
		mutex_unlock(&srtd->cstream->device->lock);
	}
}

/* yintang: to be confirmed */
int sprd_compr_configure_dsp(struct sprd_compr_rtd *srtd)
{
#if 0
	struct snd_compr_stream *cstream = srtd->cstream;
	struct snd_compr_runtime *runtime = cstream->runtime;

	int result = 0;
	const int stream_id = cstream->direction;
	const int dev = cstream->device->device;

	ADEBUG();

	runtime->fragments = srtd->codec_param.buffer.fragments;
	runtime->fragment_size = srtd->codec_param.buffer.fragment_size;
	sp_asoc_pr_dbg("%s : allocate %d buffers each of size %d\n", __func__,
			runtime->fragments, runtime->fragment_size);

	return 0;
out:
	sp_asoc_pr_dbg("%s failed\n", __func__);
	return result;
#endif
	return 0;
}

static void sprd_compr_populate_codec_list(struct sprd_compr_rtd *srtd)
{
	ADEBUG();

	srtd->compr_cap.direction = srtd->cstream->direction;
	srtd->compr_cap.min_fragment_size = COMPR_PLAYBACK_MIN_FRAGMENT_SIZE;
	srtd->compr_cap.max_fragment_size = COMPR_PLAYBACK_MAX_FRAGMENT_SIZE;
	srtd->compr_cap.min_fragments = COMPR_PLAYBACK_MIN_NUM_FRAGMENTS;
	srtd->compr_cap.max_fragments = COMPR_PLAYBACK_MAX_NUM_FRAGMENTS;
	srtd->compr_cap.num_codecs = 2;
	srtd->compr_cap.codecs[0] = SND_AUDIOCODEC_MP3;
	srtd->compr_cap.codecs[1] = SND_AUDIOCODEC_AAC;

	ADEBUG();
}

/*
 * proc interface
 */

/* #ifdef CONFIG_SND_VERBOSE_PROCFS */
#if 0
static void sprd_pcm_proc_dump_reg(int id, struct snd_info_buffer *buffer)
{
	u32 reg, base_reg;
	u32 offset = sci_dma_dump_reg(id, &base_reg);
	for (reg = base_reg; reg < base_reg + offset; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%08x 0x%08x 0x%08x 0x%08x\n",
			    (unsigned int)(reg - base_reg),
			    __raw_readl((void __iomem *)(reg + 0x00)),
			    __raw_readl((void __iomem *)(reg + 0x04)),
			    __raw_readl((void __iomem *)(reg + 0x08)),
			    __raw_readl((void __iomem *)(reg + 0x0C))
		    );
	}
}

static void sprd_pcm_proc_read(struct snd_info_entry *entry,
			       struct snd_info_buffer *buffer)
{
	struct snd_pcm_substream *substream = entry->private_data;
	struct sprd_compr_rtd *rtd = substream->runtime->private_data;
	int i;

	for (i = 0; i < rtd->hw_chan; i++) {
		if (rtd->uid_cid_map[i] >= 0) {
			snd_iprintf(buffer, "Channel%d Config\n",
				    rtd->uid_cid_map[i]);
			sprd_pcm_proc_dump_reg(rtd->uid_cid_map[i], buffer);
		}
	}
}

static void sprd_compr_proc_init(struct snd_pcm_substream *substream)
{
	struct snd_info_entry *entry;
	struct snd_pcm_str *pstr = substream->pstr;
	struct snd_pcm *pcm = pstr->pcm;
	struct sprd_compr_rtd *rtd = substream->runtime->private_data;

	entry = snd_info_create_card_entry(pcm->card, "DMA", pstr->proc_root);
	if (entry != NULL) {
		snd_info_set_text_ops(entry, substream, sprd_pcm_proc_read);
		if (snd_info_register(entry) < 0) {
			snd_info_free_entry(entry);
			entry = NULL;
		}
	}
	rtd->proc_info_entry = entry;
}

static void sprd_compr_proc_done(struct snd_pcm_substream *substream)
{
	struct sprd_compr_rtd *rtd = substream->runtime->private_data;
	snd_info_free_entry(rtd->proc_info_entry);
	rtd->proc_info_entry = NULL;
}
#else /* !CONFIG_SND_VERBOSE_PROCFS */
static inline void sprd_compr_proc_init(struct snd_compr_stream *substream)
{
}

static void sprd_compr_proc_done(struct snd_compr_stream *substream)
{
}
#endif

static int sprd_compr_config_sprd_rtd(struct sprd_compr_rtd *srtd,
						struct snd_compr_stream *substream)
{
	int ret = 0;
	int i, count;

	ADEBUG();

	if (!srtd) {
		printk(KERN_ERR "%s, rtd hasn't been allocated yet \n", __func__);
		goto err1;
	}

	srtd->dma_cfg_phy[0] = s_sprd_compr_dma_paddr + SPRD_COMPR_DMA_DATA_SIZE;
	srtd->buffer = ioremap_nocache(s_sprd_compr_dma_paddr, SPRD_COMPR_TOTAL_SIZE);
	srtd->dma_cfg_virt[0] = srtd->buffer + SPRD_COMPR_DMA_DATA_SIZE;
	srtd->buffer_size = SPRD_COMPR_DMA_DATA_SIZE;
	srtd->info_size = SPRD_COMPR_INFO_SIZE;
	/*srtd->info_paddr = srtd->dma_cfg_phy[0] + SPRD_COMPR_DMA_DESC_SIZE;*/
	srtd->info_paddr = iram_size_total - SPRD_COMPR_INFO_SIZE;
	srtd->info_vaddr = srtd->dma_cfg_virt[0] + SPRD_COMPR_DMA_DESC_SIZE;
	srtd->dma_cb_data =
		(struct sprd_compr_dma_cb_data *)kzalloc(sizeof(struct sprd_compr_dma_cb_data), GFP_KERNEL);
	if (!srtd->dma_cb_data)
		goto err2;
	srtd->dma_stage = 1;
	srtd->hw_chan = 1;
	srtd->dma_chn[0] = NULL;
	srtd->dma_tx_des[0] = srtd->dma_tx_des[1] = NULL;
	srtd->cookie[0] = srtd->cookie[1] = 0;
	srtd->params = sprd_compr_dma_param_out;
	goto out;

err2:
	pr_err("ERR:dma_cb_data alloc failed!\n");
err1:
	ret = -1;
out:
	return ret;
}

int s_buf_done_count;
static void sprd_compr_dma_buf_done(void *data)
{
	struct sprd_compr_dma_cb_data *dma_cb_data = (struct sprd_compr_dma_cb_data *)data;
	struct snd_compr_runtime *runtime = dma_cb_data->substream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;

	if (!srtd->wake_locked) {
		wake_lock(&srtd->wake_lock);
		sp_asoc_pr_info("buff done wake_lock\n");
		srtd->wake_locked = 1;
	}

	s_buf_done_count++;
	srtd->copied_total += srtd->params.block_len;
	sp_asoc_pr_info("%s, DMA copied totol=%d, buf_done_count=%d\n", __func__, srtd->copied_total, s_buf_done_count);
	snd_compr_fragment_elapsed(dma_cb_data->substream);

}

struct dma_async_tx_descriptor *sprd_compr_config_dma_channel(struct dma_chan *dma_chn, struct sprd_dma_cfg *dma_config,
		struct sprd_compr_dma_cb_data *dma_cb_data, unsigned int flag)
{
	int ret = 0;
	struct dma_async_tx_descriptor *dma_tx_des = NULL;
	/* config dma channel */
	ret = dmaengine_device_control(dma_chn,
							DMA_SLAVE_CONFIG,
							(unsigned long)(dma_config));
	if (ret < 0) {
		sp_asoc_pr_dbg("%s, DMA chan ID %d config is failed!\n", __func__,
			dma_chn->chan_id);
		return NULL;
	}
	/* get dma desc from dma config */
	dma_tx_des =
			dma_chn->device->device_prep_dma_memcpy(dma_chn, 0, 0, 0,
					DMA_CFG_FLAG|DMA_HARDWARE_FLAG);
	if (!dma_tx_des) {
		sp_asoc_pr_dbg("%s, DMA chan ID %d memcpy is failed!\n", __func__,
			dma_chn->chan_id);
		return NULL;
	}

	if (!(flag & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP)) {
		sp_asoc_pr_info("%s, Register Callback func for DMA chan ID %d\n",
				__func__, dma_chn->chan_id);
		dma_tx_des->callback = sprd_compr_dma_buf_done;
		dma_tx_des->callback_param = (void *)(dma_cb_data);
	}
	return dma_tx_des;
}

int compr_stream_hw_params(struct snd_compr_stream *substream,
			      struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = substream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;

	struct sprd_compr_dma_cb_data *dma_cb_data_ptr;

	struct dma_chan *dma_chn_request;
	struct sprd_dma_cfg *dma_config_ptr;
	dma_cap_mask_t mask;

	size_t totsize = 0;
	size_t period = 0;

	struct sprd_dma_cfg *cfg_ptr = NULL;
	enum dma_filter_param chn_type;
	dma_addr_t dma_buff_phys;

	int hw_req_id = 0;

	int ret = 0;
	int j = 0, s = 0;



	ret = sprd_compr_config_sprd_rtd(srtd, substream);
	if (ret < 0) {
		goto hw_param_err;
	}

	/* request dma channel */
	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY|DMA_SG, mask);
	chn_type = AGCP_FULL_DMA;
	dma_chn_request = dma_request_channel(mask, sprd_dma_filter_fn, (void *)&chn_type);
	if (!dma_chn_request) {
		pr_err("ERR:PCM Request DMA Error %d\n", dma_chn_request);
		goto hw_param_err;
	}
	srtd->dma_chn[s] = dma_chn_request;
	sp_asoc_pr_dbg("dma_chn_request=%d, chn_type:%d \n",
				dma_chn_request, chn_type);

	dma_config_ptr = (struct sprd_dma_cfg *)kzalloc(
		sizeof(struct sprd_dma_cfg) * (SPRD_COMPR_DMA_DESC_SIZE / DMA_LINKLIST_CFG_NODE_SIZE),
		GFP_KERNEL);

	if (!dma_config_ptr)
		goto hw_param_err;
	memset(dma_config_ptr, 0,
		sizeof(struct sprd_dma_cfg) * (SPRD_COMPR_DMA_DESC_SIZE / DMA_LINKLIST_CFG_NODE_SIZE));

	dma_cb_data_ptr = srtd->dma_cb_data;
	memset(dma_cb_data_ptr, 0, sizeof(struct sprd_compr_dma_cb_data));
	dma_cb_data_ptr->dma_chn = srtd->dma_chn[(s)];
	dma_cb_data_ptr->substream = substream;
	dma_buff_phys = (dma_addr_t)s_sprd_compr_dma_paddr;

	if (substream->direction == SND_COMPRESS_PLAYBACK)
		hw_req_id = mcdt_dac_dma_enable(MCDT_CHN_COMPR, MCDT_EMPTY_WMK);
	else
		hw_req_id = mcdt_adc_dma_enable(MCDT_CHN_COMPR, MCDT_FULL_WMK);

	totsize = params->buffer.fragment_size * params->buffer.fragments;
	period = params->buffer.fragment_size;
	srtd->params.frag_len = (MCDT_FIFO_SIZE - MCDT_EMPTY_WMK) * 4;
	srtd->params.block_len = period;

	do {
		cfg_ptr = dma_config_ptr + j;
		cfg_ptr->datawidth = 2;
		cfg_ptr->fragmens_len = srtd->params.frag_len;
		cfg_ptr->block_len = period;
		cfg_ptr->transcation_len = cfg_ptr->block_len;
		cfg_ptr->req_mode = FRAG_REQ_MODE;
		cfg_ptr->irq_mode = BLK_DONE;

		if (substream->direction == SND_COMPRESS_PLAYBACK) {
			cfg_ptr->src_step = 4;
			cfg_ptr->des_step = 0;
			cfg_ptr->src_addr = SPRD_COMPR_DMA_PADDR_TRAN(dma_buff_phys);
			cfg_ptr->des_addr = mcdt_dac_dma_phy_addr(MCDT_CHN_COMPR);
		} else {
			cfg_ptr->src_step = 0;
			cfg_ptr->des_step = 4;
			cfg_ptr->src_addr = mcdt_dac_dma_phy_addr(MCDT_CHN_COMPR);
			cfg_ptr->des_addr = SPRD_COMPR_DMA_PADDR_TRAN(dma_buff_phys);
		}

		cfg_ptr->link_cfg_v = (unsigned long)(srtd->dma_cfg_virt[s]);
		cfg_ptr->link_cfg_p = SPRD_COMPR_DMA_PADDR_TRAN((unsigned long)(srtd->dma_cfg_phy[s]));
		cfg_ptr->dev_id = hw_req_id;

		dma_buff_phys += cfg_ptr->block_len;

		if (period > totsize)
			period = totsize;
		j++;
	} while (totsize -= period);

	(dma_config_ptr + j - 1)->is_end = 2;

	srtd->dma_tx_des[0] = sprd_compr_config_dma_channel(srtd->dma_chn[0],
		dma_config_ptr, dma_cb_data_ptr, 0);
	srtd->params.tran_len = srtd->params.block_len * (j - 1);
	kfree(dma_config_ptr);

	sp_asoc_pr_dbg("srtd frag_len=%d, block_len=%d, buffer.fragments=%d, fragment_size=%d\n",
			srtd->params.frag_len, srtd->params.block_len, params->buffer.fragments, params->buffer.fragment_size);

	sp_asoc_pr_info("totsize=%d, period=%d, j=%d, hw_req_id=%d, srtd->dma_tx_des[0]=0x%lx\n",
			totsize, period, j, hw_req_id, srtd->dma_tx_des[0]);

	if (!srtd->dma_tx_des[0]) {
		goto hw_param_err;
	}

	goto ok_go_out;

hw_param_err:
	sp_asoc_pr_dbg("hw_param_err\n");
ok_go_out:
	sp_asoc_pr_dbg("return %i\n", ret);
	return ret;
}

static int compr_stream_hw_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	int s = 0;

	ADEBUG();

	if (cstream->direction == SND_COMPRESS_PLAYBACK)
		mcdt_dac_dma_disable(MCDT_CHN_COMPR);
	else
		mcdt_adc_dma_disable(MCDT_CHN_COMPR);

	sprd_compr_proc_done(cstream);

	for (s = 0; s < srtd->dma_stage; s++)
		if (srtd->dma_chn[s])
			dma_release_channel(srtd->dma_chn[s]);

	if (srtd->buffer) {
		iounmap(srtd->buffer);
	}

	if (srtd->dma_cb_data)
		kfree(srtd->dma_cb_data);

	if (srtd)
		kfree(srtd);

	return 0;
}

static int compr_stream_trigger(struct snd_compr_stream *substream, int cmd)
{
	struct sprd_compr_rtd *srtd = substream->runtime->private_data;
	struct sprd_compr_dma_params *dma = &srtd->params;
	int ret = 0;
	int i, s;
	if (!dma) {
		sp_asoc_pr_dbg("no trigger");
		return 0;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
/*
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
*/
		for (s = 0; s < srtd->dma_stage; s++) {
			if (srtd->dma_tx_des[s]) {
				srtd->cookie[s] = dmaengine_submit(srtd->dma_tx_des[s]);
			}
		}
		dma_issue_pending_all();
		sp_asoc_pr_info("S\n");
		break;
	case SNDRV_PCM_TRIGGER_STOP:
/*
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
*/
		for (s = 0; s < srtd->dma_stage; s++) {
			if (srtd->dma_chn[s]) {
				dmaengine_pause(srtd->dma_chn[s]);
			}
		}
		srtd->cb_called = 0;
		sp_asoc_pr_info("E\n");
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int sprd_platform_send_cmd(int cmd, int stream_id, void *buff, int buff_size)
{
	struct cmd_common command = {0};
	int ret = 0;

	command.command = cmd;
	command.sub_cmd = stream_id;
	sp_asoc_pr_dbg("%s:cmd=%d,stream_id=%d\n",
		__func__, cmd, stream_id);

	ret = compr_send_cmd(cmd, (void *)&command, sizeof(command));
	if (ret < 0) {
		sp_asoc_pr_dbg(KERN_ERR "%s: failed to send command(%d), ret=%d\n",
			__func__, cmd, ret);
		return -EIO;
	}
#if 0
	ret = compr_recv_cmd(SIPC_ID_AGDSP, cmd, COMPR_WAIT_FOREVER);
	if (ret < 0) {
		sp_asoc_pr_dbg(KERN_ERR "%s: failed to recv command(%d), ret=%d\n",
			__func__, cmd, ret);
		return -EIO;
	}
#endif
	return 0;
}

static int sprd_platform_send_param(int cmd, int stream_id, void *buff, int buff_size)
{
	int ret = 0;

	sp_asoc_pr_dbg("%s:cmd=%d,stream_id=%d,buff=0x%x,buff_size=%d\n",
		__func__, cmd, stream_id, buff, buff_size);

	ret = compr_send_cmd(cmd, buff, buff_size);
	if (ret < 0) {
		sp_asoc_pr_dbg(KERN_ERR "%s: failed to send command(%d), ret=%d\n", __func__, cmd, ret);
		return -EIO;
	}
#if 0
	ret = compr_recv_cmd(SIPC_ID_AGDSP, cmd, COMPR_WAIT_FOREVER);
	if (ret < 0) {
		sp_asoc_pr_dbg(KERN_ERR "%s: failed to recv command(%d), ret=%d\n", __func__, cmd, ret);
		return -EIO;
	}
#endif
	return 0;
}

static int sprd_platform_compr_open(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata = snd_soc_platform_get_drvdata(rtd->platform);

	int result = 0;
	const int dev = cstream->device->device;
	const int stream_id = cstream->direction;
	struct sprd_compr_rtd *srtd = NULL;
	struct sprd_compr_dev_ctrl *dev_ctrl = NULL;

	ADEBUG();

	sprd_compress_new(rtd);

	sp_asoc_pr_info("sprd_platform_compr_open E,pdata=0x%x, platform name=%s\n",
		pdata, rtd->platform->name);

	srtd = (struct sprd_compr_rtd *)kzalloc(sizeof(struct sprd_compr_rtd), GFP_KERNEL);
	memset(srtd, 0, sizeof(struct sprd_compr_rtd));
	if (!srtd) {
		sp_asoc_pr_dbg("srtd is NULL!\n");
		result = -1;
		goto err;
	}

	sp_asoc_pr_info("sprd_platform_compr_open,srtd=0x%x,cstream->direction=%d\n", srtd, cstream->direction);

	srtd->cstream = cstream;

	srtd->stream_id = stream_id;
	srtd->stream_state = COMPR_IDLE;
	srtd->codec = FORMAT_MP3;
	srtd->sample_rate = 44100;
	srtd->num_channels = 2;
	srtd->hw_chan = 1;

	runtime->private_data = srtd;

	sprd_compr_populate_codec_list(srtd);

	spin_lock_init(&srtd->lock);

	atomic_set(&srtd->eos, 0);
	atomic_set(&srtd->start, 0);
	atomic_set(&srtd->drain, 0);
	atomic_set(&srtd->xrun, 0);
	atomic_set(&srtd->pause, 0);
	wake_lock_init(&srtd->wake_lock, WAKE_LOCK_SUSPEND, "compr write");

	init_waitqueue_head(&srtd->eos_wait);
	init_waitqueue_head(&srtd->drain_wait);
	init_waitqueue_head(&srtd->flush_wait);

	dev_ctrl = &pdata->dev_ctrl;
	mutex_lock(&dev_ctrl->mutex);

	sp_asoc_pr_dbg("%s: before send open cmd\n", __func__);
	result = compr_ch_open(SIPC_ID_AGDSP, COMPR_WAIT_FOREVER);
	if (result) {
		sp_asoc_pr_dbg(KERN_ERR "%s: failed to open sipc channle, ret=%d\n", __func__, result);
		goto out_ops;
	}

	result = sprd_platform_send_cmd(COMPR_CMD_OPEN, stream_id, 0, 0);

	if (result < 0)
		goto out_ops;


	pdata->thread = kthread_run(sprd_compr_mornitor_thread, srtd, "sprd_compr_monitor");

	mutex_unlock(&dev_ctrl->mutex);
	ADEBUG();
	return 0;

out_ops:
	sp_asoc_pr_dbg("%s failed\n", __func__);
	mutex_unlock(&dev_ctrl->mutex);
err:
	return result;
}

static int sprd_platform_compr_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata = snd_soc_platform_get_drvdata(rtd->platform);

	int ret = 0;
	const int stream_id = cstream->direction;
	struct sprd_compr_dev_ctrl *dev_ctrl = &pdata->dev_ctrl;

	ADEBUG();

	if (srtd->stream_state == COMPR_TRIGGERED) {
		sprd_platform_compr_trigger(cstream, SNDRV_PCM_TRIGGER_STOP);
	}
	mutex_lock(&dev_ctrl->mutex);

	ret = sprd_platform_send_cmd(COMPR_CMD_CLOSE, stream_id, 0, 0);
	if (ret < 0) {
		goto out_ops;
	}

	ret = compr_ch_close(SIPC_ID_AGDSP, COMPR_WAIT_FOREVER);
	if (ret) {
		sp_asoc_pr_info(KERN_ERR "%s: failed to open sipc channle, ret=%d\n", __func__, ret);
		goto out_ops;
	}
	mutex_unlock(&dev_ctrl->mutex);

	compr_stream_hw_free(cstream);
	wake_lock_destroy(&srtd->wake_lock);

	return 0;

out_ops:
	sp_asoc_pr_dbg("%s failed\n", __func__);
	mutex_unlock(&dev_ctrl->mutex);
	return ret;
}

static int sprd_platform_compr_set_params(struct snd_compr_stream *cstream,
					struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata = snd_soc_platform_get_drvdata(rtd->platform);
	struct sprd_compr_dev_ctrl *dev_ctrl = NULL;
	const int stream_id = cstream->direction;
	int result = 0;

	ADEBUG();
	/*prtd->cstream = cstream;*/

	memcpy(&srtd->codec_param, params, sizeof(struct snd_compr_params));

	/* ToDo: remove duplicates */
	srtd->num_channels = srtd->codec_param.codec.ch_in;
	sp_asoc_pr_info("%s: channels: %d, sample_rate=%d\n",
		__func__, srtd->num_channels, srtd->codec_param.codec.sample_rate);

	switch (srtd->codec_param.codec.sample_rate) {
	case SNDRV_PCM_RATE_8000:
		srtd->sample_rate = 8000;
		break;
	case SNDRV_PCM_RATE_11025:
		srtd->sample_rate = 11025;
		break;
	case SNDRV_PCM_RATE_16000:
		srtd->sample_rate = 16000;
		break;
	case SNDRV_PCM_RATE_22050:
		srtd->sample_rate = 22050;
		break;
	case SNDRV_PCM_RATE_32000:
		srtd->sample_rate = 32000;
		break;
	case SNDRV_PCM_RATE_44100:
		srtd->sample_rate = 44100;
		break;
	case SNDRV_PCM_RATE_48000:
		srtd->sample_rate = 48000;
		break;
	default:
		srtd->sample_rate = srtd->codec_param.codec.sample_rate;
		break;
    }

	sp_asoc_pr_info("%s: sample_rate %d, code.id=%d\n", __func__, srtd->sample_rate, params->codec.id);

	switch (params->codec.id) {
	case SND_AUDIOCODEC_MP3:
		srtd->codec = FORMAT_MP3;
		break;
	case SND_AUDIOCODEC_AAC:
		srtd->codec = FORMAT_AAC;
		break;
	default:
		return -EINVAL;
	}

	compr_stream_hw_params(cstream, params);

	result = sprd_compr_configure_dsp(srtd);
	if (result) {
		sp_asoc_pr_info("sprd_compr_configure_dsp error %d\n", result);
		return result;
	}

	dev_ctrl = &pdata->dev_ctrl;

	struct cmd_prepare prepare = {0};
	prepare.common.command = COMPR_CMD_PARAMS;
	prepare.common.sub_cmd = stream_id;
	prepare.samplerate = srtd->sample_rate;
	prepare.channels = srtd->num_channels;
	prepare.format = srtd->codec;
	prepare.info_paddr = srtd->info_paddr;
	prepare.info_size = srtd->info_size;
	prepare.mcdt_chn = MCDT_CHN_COMPR;
	prepare.rate = srtd->codec_param.codec.bit_rate;

	mutex_lock(&dev_ctrl->mutex);

	result = sprd_platform_send_param(COMPR_CMD_PARAMS, stream_id, &prepare, sizeof(prepare));

	if (result < 0)
		goto out_ops;

	mutex_unlock(&dev_ctrl->mutex);

	srtd->stream_state = COMPR_PARAMSED;

	ADEBUG();
	return 0;

out_ops:
	sp_asoc_pr_info("%s failed\n", __func__);
	mutex_unlock(&dev_ctrl->mutex);
	return result;
}

static int sprd_platform_compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata = snd_soc_platform_get_drvdata(rtd->platform);

	int result = 0;
	const int stream_id = cstream->direction;
	const int dev = cstream->device->device;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct sprd_compr_dev_ctrl *dev_ctrl = &pdata->dev_ctrl;
	struct cmd_common cmd_drain = {0};

	ADEBUG();

	if (cstream->direction != SND_COMPRESS_PLAYBACK) {
		sp_asoc_pr_dbg("%s: Unsupported stream type\n", __func__);
		return -EINVAL;
	}

	compr_stream_trigger(cstream, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		sp_asoc_pr_dbg("%s: SNDRV_PCM_TRIGGER_START\n", __func__);
		spin_lock_irq(&srtd->lock);
		atomic_set(&srtd->start, 1);
		srtd->stream_state = COMPR_TRIGGERED;
		spin_unlock_irq(&srtd->lock);

		mutex_lock(&dev_ctrl->mutex);
		result = sprd_platform_send_cmd(COMPR_CMD_START, stream_id, 0, 0);
		if (result < 0)
			goto out_ops;
		mutex_unlock(&dev_ctrl->mutex);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
		sp_asoc_pr_dbg("%s: SNDRV_PCM_TRIGGER_STOP\n", __func__);

		if (atomic_read(&srtd->drain)) {
			sp_asoc_pr_dbg("wake up on drain\n");
			srtd->drain_ready = 1;
			wake_up(&srtd->drain_wait);
			atomic_set(&srtd->drain, 0);
		}

		spin_lock_irq(&srtd->lock);
		atomic_set(&srtd->start, 0);
		atomic_set(&srtd->pause, 0);
		srtd->stream_state = COMPR_STOPPED;
		srtd->copied_total = 0;
		srtd->app_pointer  = 0;
		srtd->bytes_received = 0;
		spin_unlock_irq(&srtd->lock);

		mutex_lock(&dev_ctrl->mutex);
		result = sprd_platform_send_cmd(COMPR_CMD_STOP, stream_id, 0, 0);
		if (result < 0)
			goto out_ops;

		if (cstream->direction == SND_COMPRESS_PLAYBACK)
			mcdt_da_fifo_clr(MCDT_CHN_COMPR);
		else
			mcdt_ad_fifo_clr(MCDT_CHN_COMPR);

		mutex_unlock(&dev_ctrl->mutex);

		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sp_asoc_pr_dbg("%s: SNDRV_PCM_TRIGGER_PAUSE_PUSH\n", __func__);

		mutex_lock(&dev_ctrl->mutex);
		atomic_set(&srtd->pause, 1);

		result = sprd_platform_send_cmd(COMPR_CMD_PAUSE_PUSH, stream_id, 0, 0);
		if (result < 0)
			goto out_ops;

		mutex_unlock(&dev_ctrl->mutex);

		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		sp_asoc_pr_dbg("%s: SNDRV_PCM_TRIGGER_PAUSE_RELEASE\n", __func__);

		mutex_lock(&dev_ctrl->mutex);
		if (atomic_read(&srtd->pause)) {
			result = sprd_platform_send_cmd(COMPR_CMD_PAUSE_RELEASE, stream_id, 0, 0);
			if (result < 0)
				goto out_ops;
		}

		atomic_set(&srtd->pause, 0);
		mutex_unlock(&dev_ctrl->mutex);

		break;
#if 0
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
		sp_asoc_pr_dbg("%s: SND_COMPR_TRIGGER_PARTIAL_DRAIN\n", __func__);
		/* Make sure all the data is sent to DSP before sending EOS */
		atomic_set(&srtd->drain, 1);

		mutex_lock(&dev_ctrl->mutex);
		result = sprd_platform_send_cmd(COMPR_CMD_DRAIN, stream_id, 0, 0);
		if (result < 0)
			goto out_ops;
		mutex_unlock(&dev_ctrl->mutex);
		break;
#endif
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
	case SND_COMPR_TRIGGER_DRAIN:
		sp_asoc_pr_info("%s: SNDRV_COMPRESS_DRAIN, total=%d\n",
			__func__, srtd->bytes_received);
		/* Make sure all the data is sent to DSP before sending EOS */
		atomic_set(&srtd->drain, 1);

		mutex_lock(&dev_ctrl->mutex);

		cmd_drain.command = COMPR_CMD_DRAIN;
		cmd_drain.sub_cmd = stream_id;
		cmd_drain.reserved1 = srtd->bytes_received;
		result = sprd_platform_send_param(COMPR_CMD_DRAIN, stream_id, &cmd_drain, sizeof(cmd_drain));
		if (result < 0) {
			sp_asoc_pr_info("drain out err!");
			goto out_ops;
		}
		mutex_unlock(&dev_ctrl->mutex);

		/* if (prtd->bytes_received > prtd->copied_total) { */
		sp_asoc_pr_dbg("%s: before compress drain\n", __func__);
#if 0
		srtd->drain_ready = 0;
		do {
		    result = wait_event_interruptible(srtd->drain_wait, srtd->drain_ready);
		    sp_asoc_pr_dbg("%s: after compress drain\n", __func__);
		} while (result == -ERESTARTSYS);
#endif

#if 0
		srtd->drain_ready = 0;
		snd_compr_drain_notify(srtd->cstream);

#endif

#if 1
		/* yintang: for testing */
		{
			int ret = 0;

			ret = dmaengine_pause(srtd->dma_chn[0]);
			sp_asoc_pr_info("%s: drain out, dma stopped, ret=%d------------------!\n", __func__, ret);

			srtd->drain_ready = 1;
			wake_up(&srtd->drain_wait);
		}
#endif
		sp_asoc_pr_info("%s: out of drain\n", __func__);
		break;
	case SND_COMPR_TRIGGER_NEXT_TRACK:
		sp_asoc_pr_dbg("%s: SND_COMPR_TRIGGER_NEXT_TRACK\n", __func__);
		break;
	default:
		break;
	}

	ADEBUG();
	return 0;

out_ops:
	sp_asoc_pr_info("%s failed\n", __func__);
	mutex_unlock(&dev_ctrl->mutex);
	return result;
}

static int sprd_platform_compr_pointer(struct snd_compr_stream *cstream,
					struct snd_compr_tstamp *arg)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata = snd_soc_platform_get_drvdata(rtd->platform);

	int result = 0;
	const int stream_id = cstream->direction;
	const int dev = cstream->device->device;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct sprd_compr_dev_ctrl *dev_ctrl = &pdata->dev_ctrl;
	struct sprd_compr_playinfo *info = (struct sprd_compr_playinfo *)srtd->info_vaddr;

	ADEBUG();

	struct snd_compr_tstamp tstamp;
	uint32_t pcm_frames = 0;
	/*uint64_t timestamp = 0;*/

	sp_asoc_pr_dbg("%s E, info=0x%x\n", __func__, info);
	if (pdata->suspend == true) {
		sp_asoc_pr_dbg("%s, wait_event\n", __func__);
		wait_event(pdata->point_wait, !pdata->suspend);
	}
	memset(&tstamp, 0x0, sizeof(struct snd_compr_tstamp));

	tstamp.sampling_rate = srtd->sample_rate;
	tstamp.copied_total = srtd->copied_total;
#if 0
	/* DSP returns timestamp in usec */
	timestamp = (uint64_t)info->uiCurrentTime;
	timestamp *= srtd->sample_rate;
	tstamp.pcm_io_frames = (snd_pcm_uframes_t)div64_u64(timestamp, 1000);
#else
	tstamp.pcm_io_frames = (uint64_t)info->uiCurrentDataOffset;
	/*
	timestamp = (uint64_t)info->uiCurrentDataOffset * 1000;
	timestamp = div64_u64(timestamp, srtd->sample_rate);
	tstamp.timestamp = timestamp;
	*/
#endif
	memcpy(arg, &tstamp, sizeof(struct snd_compr_tstamp));
	sp_asoc_pr_info("%s: copied_total: %d    bit_rate: %d    sample_rate: %d    pcm_io_frames: %d\n", __func__,
			tstamp.copied_total, srtd->codec_param.codec.bit_rate, srtd->sample_rate, tstamp.pcm_io_frames);

	return 0;
}

static int sprd_platform_compr_copy(struct snd_compr_stream *cstream, char __user *buf,
					size_t count)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata = snd_soc_platform_get_drvdata(rtd->platform);

	int result = 0;
	void *dstn;
	size_t copy;
	size_t bytes_available = 0;
	const int stream_id = cstream->direction;
	const int dev = cstream->device->device;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct sprd_compr_dev_ctrl *dev_ctrl = &pdata->dev_ctrl;

	if (srtd->wake_locked) {
		sp_asoc_pr_info("wake_lock released\n");
		wake_unlock(&srtd->wake_lock);
		srtd->wake_locked = 0;
	}

	sp_asoc_pr_info("%s: count = %d,app_pointer=%d,buff_size=%d,bytes_received=%d\n",
		__func__, count, srtd->app_pointer, srtd->buffer_size, srtd->bytes_received);

	dstn = srtd->buffer + srtd->app_pointer;
	if (count < srtd->buffer_size - srtd->app_pointer) {
		if (unalign_copy_from_user(dstn, buf, count))
			return -EFAULT;
		srtd->app_pointer += count;
	} else {
		copy = srtd->buffer_size - srtd->app_pointer;
		if (unalign_copy_from_user(dstn, buf, copy))
			return -EFAULT;
		if (unalign_copy_from_user(srtd->buffer, buf + copy, count - copy))
			return -EFAULT;
		srtd->app_pointer = count - copy;
	}

	/*
	 * If stream is started and there has been an xrun,
	 * since the available bytes fits fragment_size, copy the data right away
	 */

	srtd->bytes_received += count;

	return count;
}

static int sprd_platform_compr_ack(struct snd_compr_stream *cstream,
					size_t bytes)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata = snd_soc_platform_get_drvdata(rtd->platform);

	int result = 0;
	const int stream_id = cstream->direction;
	const int dev = cstream->device->device;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct sprd_compr_dev_ctrl *dev_ctrl = &pdata->dev_ctrl;

	ADEBUG();

	ADEBUG();
	return 0;
}

static int sprd_platform_compr_get_caps(struct snd_compr_stream *cstream,
					struct snd_compr_caps *caps)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;

	ADEBUG();
	memcpy(caps, &srtd->compr_cap, sizeof(struct snd_compr_caps));

	ADEBUG();
	return 0;
}

static int sprd_platform_compr_get_codec_caps(struct snd_compr_stream *cstream,
					struct snd_compr_codec_caps *codec)
{
	ADEBUG();

	/* yintang:how to get codec caps, to be confirmed */

	switch (codec->codec) {
	case SND_AUDIOCODEC_MP3:
		codec->num_descriptors = 2;
		codec->descriptor[0].max_ch = 2;
		/*codec->descriptor[0].sample_rates = 0;*/
		codec->descriptor[0].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[0].bit_rate[1] = 128;
		codec->descriptor[0].num_bitrates = 2;
		codec->descriptor[0].profiles = 0;
		codec->descriptor[0].modes = SND_AUDIOCHANMODE_MP3_STEREO;
		codec->descriptor[0].formats = 0;
		break;
	case SND_AUDIOCODEC_AAC:
		codec->num_descriptors = 2;
		codec->descriptor[1].max_ch = 2;
		/*codec->descriptor[1].sample_rates = 1;*/
		codec->descriptor[1].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[1].bit_rate[1] = 128;
		codec->descriptor[1].num_bitrates = 2;
		codec->descriptor[1].profiles = 0;
		codec->descriptor[1].modes = 0;
		codec->descriptor[1].formats = 0;
		break;
	default:
		sp_asoc_pr_dbg("%s: Unsupported audio codec %d\n", __func__, codec->codec);
		return -EINVAL;
	}

	ADEBUG();
	return 0;
}

static int sprd_platform_compr_set_metadata(struct snd_compr_stream *cstream,
					struct snd_compr_metadata *metadata)
{
	ADEBUG();

	if (!metadata || !cstream)
		return -EINVAL;

	if (metadata->key == SNDRV_COMPRESS_ENCODER_PADDING) {
		sp_asoc_pr_dbg("%s, got encoder padding %u\n", __func__, metadata->value[0]);
	} else if (metadata->key == SNDRV_COMPRESS_ENCODER_DELAY) {
		sp_asoc_pr_dbg("%s, got encoder delay %u\n", __func__, metadata->value[0]);
	}

	ADEBUG();
	return 0;
}
#ifdef CONFIG_PROC_FS
static void sprd_compress_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct snd_soc_platform *platform = entry->private_data;
	int reg;

	snd_iprintf(buffer, "%s\n", platform->name);
}

static void sprd_compress_proc_init(struct snd_soc_platform *platform)
{
	struct snd_info_entry *entry;

	sp_asoc_pr_dbg("%s E", __func__);
	if (!snd_card_proc_new(platform->card->snd_card, "sprd-compress", &entry))
		snd_info_set_text_ops(entry, platform, sprd_compress_proc_read);
}
#else /* !CONFIG_PROC_FS */
static inline void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
}
#endif

static int sprd_compr_probe(struct snd_soc_platform *platform)
{
	int result = 0;
	struct sprd_compr_pdata *pdata = NULL;

	ADEBUG();
	sprd_compress_proc_init(platform);

	pdata = (struct sprd_compr_pdata *)kzalloc(sizeof(struct sprd_compr_pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	init_waitqueue_head(&pdata->point_wait);
	mutex_init(&pdata->dev_ctrl.mutex);

	pdata->suspend = false;
	snd_soc_platform_set_drvdata(platform, pdata);

	ADEBUG();
	return 0;
}

static u64 sprd_compr_dmamask = DMA_BIT_MASK(32);
static int sprd_compress_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sprd_compr_pdata *pdata = snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sprd_compr_dmamask; /*yintang: to be confirmed */
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	sp_asoc_pr_dbg("Playback alloc memery\n");

out:
	sp_asoc_pr_dbg("return %i\n", ret);
	return ret;
}

static struct snd_compr_ops sprd_platform_compr_ops = {
    .open = sprd_platform_compr_open,
    .free = sprd_platform_compr_free,
    .set_params = sprd_platform_compr_set_params,
    .set_metadata = sprd_platform_compr_set_metadata,
    .trigger = sprd_platform_compr_trigger,
    .pointer = sprd_platform_compr_pointer,
    .copy = sprd_platform_compr_copy,
    .ack = sprd_platform_compr_ack,
    .get_caps = sprd_platform_compr_get_caps,
    .get_codec_caps = sprd_platform_compr_get_codec_caps,
};

static struct snd_soc_platform_driver sprd_platform_compr_drv = {
	.probe		= sprd_compr_probe,
	.compr_ops	= &sprd_platform_compr_ops,
	.pcm_new    = sprd_compress_new,
};

static int sprd_platform_compr_probe(struct platform_device *pdev)
{
	int ret = 0;

	uint32_t array[2];
	struct device_node *node = pdev->dev.of_node;

	ADEBUG();
	sp_asoc_pr_info("%s E,devname:%s, drivername:%s\n",
		__func__, dev_name(&pdev->dev), (pdev->dev).driver->name);
	if (node) {
		ret = of_property_read_u32_array(node, "iram_base", &array[0], 2);
		if (!ret) {
			iram_agcp_base = array[0];
			iram_ap_base = array[1];
		} else {
			pr_err("compr iram base parse error!\n");
			return -EINVAL;
		}
		if (of_property_read_u32(node, "iram_paddr", &iram_paddr)) {
			pr_err("compr iram_paddr parse error!\n");
			return -EINVAL;
		}
		if (of_property_read_u32(node, "iram_size_total", &iram_size_total)) {
			pr_err("compr iram_size_total parse error!\n");
			return -EINVAL;
		}
		if (of_property_read_u32(node, "iram_size_dma", &iram_size_dma)) {
			pr_err("compr iram_size_dma parse error!\n");
			return -EINVAL;
		}
		s_sprd_compr_dma_paddr = (char *)(iram_paddr + iram_size_total - SPRD_COMPR_TOTAL_SIZE);
	} else {
		pr_err("compr iram configure is not exist!\n");
		return -EINVAL;
	}

	ret = snd_soc_register_platform(&pdev->dev, &sprd_platform_compr_drv);
	if (ret) {
		sp_asoc_pr_info("registering soc platform failed\n");
		return ret;
	}

	ret = snd_soc_register_component(&pdev->dev, &sprd_compr_component,
			sprd_compr_dai, ARRAY_SIZE(sprd_compr_dai));
	if (ret) {
		sp_asoc_pr_info("registering cpu dais failed\n");
		snd_soc_unregister_platform(&pdev->dev);
	}
	ADEBUG();
	return ret;
}

static int sprd_platform_compr_remove(struct platform_device *pdev)
{
	struct sprd_compr_pdata *pdata = NULL;

	ADEBUG();

	pdata = (struct sprd_compr_pdata *)dev_get_drvdata(&pdev->dev);
	if (pdata) {
			kfree(pdata);
	}

	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);
	ADEBUG();
	return 0;
}

static int sprd_platform_compr_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i = 0;
	struct sprd_compr_pdata *pdata = NULL;

	ADEBUG();

	/*dump_stack();*/

	pdata = (struct sprd_compr_pdata *)dev_get_drvdata(&pdev->dev);
	if (pdata) {
		pdata->suspend = true;
	}

	ADEBUG();
	return 0;
}

static int sprd_platform_compr_resume(struct platform_device *pdev)
{
	struct sprd_compr_pdata *pdata = NULL;

	ADEBUG();

	pdata = (struct sprd_compr_pdata *)dev_get_drvdata(&pdev->dev);
	if (pdata) {
		sp_asoc_pr_dbg("sprd compr platform wake_up\n");
		pdata->suspend = false;
		wake_up(&pdata->point_wait);
	}

	ADEBUG();
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sprd_compress_of_match[] = {
    {.compatible = "sprd,sprd-compr-platform",},
    {},
};

MODULE_DEVICE_TABLE(of, sprd_compress_of_match);
#endif


static struct platform_driver sprd_platform_compr_driver = {
	.driver = {
		.name = "sprd-compr-platform",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sprd_compress_of_match),
	},
	.probe = sprd_platform_compr_probe,
	.remove = sprd_platform_compr_remove,
	.suspend = sprd_platform_compr_suspend,
	.resume = sprd_platform_compr_resume,
};

module_platform_driver(sprd_platform_compr_driver);

MODULE_DESCRIPTION("ASoC Spreadtrum Compress Platform driver");
MODULE_AUTHOR("Yintang Ren<yintang.ren@spreadtrum.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:compress-platform");
