/* sound/soc/samsung/eax-dma.c
 *
 * Exynos Audio Mixer DMA driver
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/iommu.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/exynos.h>

#include <mach/hardware.h>
#include <mach/dma.h>

#include "lpass.h"
#include "dma.h"
#include "eax.h"


#define FRAME_BYTE		4
#define MIXBUF_SIZE		120
#define MIXBUF_BYTE		(MIXBUF_SIZE * FRAME_BYTE)
#define DMA_PERIOD		120
#define DMA_PERIOD_CNT		4
#define DMA_PERIOD_BYTE		(DMA_PERIOD * FRAME_BYTE)
#define DMA_START_THRESHOLD	(DMA_PERIOD_CNT - 1)

static const struct snd_pcm_hardware dma_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= 128 * 1024,
	.period_bytes_min	= 128,
	.period_bytes_max	= 64 * 1024,
	.periods_min		= 2,
	.periods_max		= 128,
	.fifo_size		= 32,
};

struct runtime_data {
	spinlock_t		lock;
	bool			running;
	struct snd_pcm_substream *substream;
	struct snd_soc_dai	*cpu_dai;
	unsigned int		dma_period;
	dma_addr_t		dma_start;
	dma_addr_t		dma_pos;
	dma_addr_t		dma_end;
	u32			*dma_buf;
	short			*dma_mono;
};

struct mixer_info {
	spinlock_t		lock;
	struct task_struct	*thread_id;
	struct snd_soc_dai	*cpu_dai;
	short			mix_buf[MIXBUF_BYTE >> 1];
	bool			buf_fill;
	bool			running;
} mi;

struct buf_info {
	struct runtime_data	*prtd;
	struct list_head	node;
};

static LIST_HEAD(buf_list);
static DECLARE_WAIT_QUEUE_HEAD(mixer_run_wq);
static DECLARE_WAIT_QUEUE_HEAD(mixer_buf_wq);

static struct dma_info {
	spinlock_t		lock;
	struct mutex		mutex;
	struct snd_soc_dai	*cpu_dai;
	struct s3c_dma_params	*params;
	bool			params_init;
	bool			params_done;
	bool			prepare_done;
	bool			running;
	bool			buf_done;
	bool			buf_fill[DMA_PERIOD_CNT];
	unsigned char		*buf_wr_p[DMA_PERIOD_CNT];
	int			buf_wr_idx;
	int			buf_rd_idx;
	u32			*dma_buf;
	unsigned int		dma_period;
	dma_addr_t		dma_start;
	dma_addr_t		dma_pos;
	dma_addr_t		dma_end;
} di;

static int eax_mixer_add(struct runtime_data *prtd);
static int eax_mixer_remove(struct runtime_data *prtd);
static void eax_mixer_trigger(bool on);
static int eax_mixer_kthread(void *arg);

static void eax_adma_alloc_buf(void)
{
	int n;
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	size_t size = 128 * 1024;
	struct iommu_domain *domain = lpass_get_iommu_domain();

	di.dma_buf = dma_alloc_writecombine(di.cpu_dai->dev,
			size, &di.dma_start, GFP_KERNEL);

	iommu_map(domain, 0x48000000, di.dma_start, size, 0);
	di.dma_start = 0x48000000;
#else
	size_t size = DMA_PERIOD_BYTE * DMA_PERIOD_CNT;

	di.dma_buf = dma_alloc_writecombine(di.cpu_dai->dev,
			size, &di.dma_start, GFP_KERNEL);
#endif

	for (n = 0; n < DMA_PERIOD_CNT; n++) {
		di.buf_wr_p[n] = (unsigned char *)di.dma_buf;
		di.buf_wr_p[n] += DMA_PERIOD_BYTE * n;
	}
}

static void eax_adma_free_buf(void)
{
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	size_t size = 128 * 1024;
	struct iommu_domain *domain = lpass_get_iommu_domain();

	dma_free_coherent(di.cpu_dai->dev, size, (void *)di.dma_buf, di.dma_start);
	iommu_unmap(domain, 0x48000000, size);
	di.dma_start = 0;
#else
	size_t size = DMA_PERIOD_BYTE * DMA_PERIOD_CNT;
	dma_free_coherent(di.cpu_dai->dev, size, (void *)di.dma_buf, di.dma_start);
#endif
	memset(di.buf_wr_p, 0, sizeof(unsigned char *) * DMA_PERIOD_CNT);
}

int eax_dma_dai_register(struct snd_soc_dai *dai)
{
	spin_lock_init(&di.lock);
	mutex_init(&di.mutex);

	di.cpu_dai = dai;
	di.running = false;
	di.params_init = false;
	di.params_done = false;
	di.prepare_done = false;

	spin_lock_init(&mi.lock);
	mi.cpu_dai = dai;
	mi.running = false;
	mi.thread_id = (struct task_struct *)
			kthread_run(eax_mixer_kthread, NULL, "eax-mixer");

	eax_adma_alloc_buf();

	return 0;
}

int eax_dma_dai_unregister(void)
{
	mutex_destroy(&di.mutex);

	di.cpu_dai = NULL;
	di.running = false;
	di.params_init = false;
	di.params_done = false;
	di.prepare_done = false;

	mi.cpu_dai = NULL;
	mi.running = false;
	mi.thread_id = NULL;

	eax_adma_free_buf();

	return 0;
}

int eax_dma_params_register(struct s3c_dma_params *dma)
{
	di.params = dma;

	return 0;
}

static void eax_dma_elapsed(int buf_idx)
{
	int n;

	di.buf_rd_idx = buf_idx;

	for (n = 0; n < DMA_PERIOD_CNT; n++) {
		if (--buf_idx < 0)
			buf_idx += DMA_PERIOD_CNT;

		di.buf_fill[buf_idx] = false;
		if (buf_idx == di.buf_wr_idx)
			break;
	}

	di.buf_done = true;
	if (waitqueue_active(&mixer_buf_wq))
		wake_up_interruptible(&mixer_buf_wq);
}

static void eax_adma_buffdone(void *data)
{
	dma_addr_t src, dst, pos;
	int buf_idx;

	if (!di.running)
		return;

	di.params->ops->getposition(di.params->ch, &src, &dst);
	pos = src - di.dma_start;
	pos /= di.dma_period;
	buf_idx = pos;
	pos = di.dma_start + (pos * di.dma_period);
	if (pos >= di.dma_end)
		pos = di.dma_start;

	di.dma_pos = pos;

	eax_dma_elapsed(buf_idx);
}

static void eax_adma_hw_params(void)
{
	struct samsung_dma_req req;
	struct samsung_dma_config config;

	mutex_lock(&di.mutex);

	if (di.params_done)
		goto out;

	di.params_done = true;

	if (!di.params_init) {
		di.params_init = true;
		di.params->ops = samsung_dma_get_ops();

		req.cap = DMA_CYCLIC;
		req.client = di.params->client;
		config.direction = DMA_MEM_TO_DEV;
		config.width = di.params->dma_size;
		config.fifo = di.params->dma_addr;
		di.params->ch = di.params->ops->request(di.params->channel,
				&req, di.cpu_dai->dev, di.params->ch_name);
		di.params->ops->config(di.params->ch, &config);
	}

	di.dma_period = DMA_PERIOD_BYTE;
	di.dma_pos = di.dma_start;
	di.dma_end = di.dma_start + di.dma_period * DMA_PERIOD_CNT;

	pr_info("EAXDMA:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d dma_area=0x%x\n",
		di.dma_start, di.dma_end - di.dma_start, di.dma_period,
		DMA_PERIOD_CNT, (unsigned int)di.dma_buf);
out:
	mutex_unlock(&di.mutex);
}

static void eax_adma_hw_free(void)
{
	mutex_lock(&di.mutex);

	if (di.running)
		goto out;

	if (di.params_init) {
		di.params_init = false;
		di.params->ops->flush(di.params->ch);
		di.params->ops->release(di.params->ch, di.params->client);
	}

	di.params_done = false;
	di.prepare_done = false;
out:
	mutex_unlock(&di.mutex);
}

static void eax_adma_prepare(void)
{
	struct samsung_dma_prep dma_info;
	int n;

	mutex_lock(&di.mutex);

	if (di.prepare_done)
		goto out;

	di.prepare_done = true;

	/* zero fill */
	mi.buf_fill = false;
	di.buf_wr_idx = 0;
	di.buf_rd_idx = DMA_PERIOD_CNT;
	memset(di.dma_buf, 0, DMA_PERIOD_BYTE * DMA_PERIOD_CNT);
	for (n = 0; n < DMA_PERIOD_CNT; n++)
		di.buf_fill[n] = true;

	/* prepare */
	di.params->ops->flush(di.params->ch);
	di.dma_pos = di.dma_start;

	/* enqueue */
	dma_info.cap = DMA_CYCLIC;
	dma_info.direction = DMA_MEM_TO_DEV;
	dma_info.fp = eax_adma_buffdone;
	dma_info.fp_param = NULL;
	dma_info.period = di.dma_period;
	dma_info.len = di.dma_period * DMA_PERIOD_CNT;

	dma_info.buf = di.dma_pos;
	dma_info.infiniteloop = DMA_PERIOD_CNT;
	di.params->ops->prepare(di.params->ch, &dma_info);
out:
	mutex_unlock(&di.mutex);
}

static void eax_adma_trigger(bool on)
{
	spin_lock(&di.lock);

	if (on) {
		di.running = on;
		di.params->ops->trigger(di.params->ch);
	} else {
		di.params->ops->stop(di.params->ch);
		di.prepare_done = false;
		di.running = on;
	}

	spin_unlock(&di.lock);
}

static inline void eax_dma_xfer(struct runtime_data *prtd,
			short *pcm_l, short *pcm_r)
{
	dma_addr_t dma_pos;

	*pcm_l = *prtd->dma_mono++;
	*pcm_r = *prtd->dma_mono++;
	dma_pos = prtd->dma_pos + 4;

	if (dma_pos == prtd->dma_end) {
		prtd->dma_pos = prtd->dma_start;
		prtd->dma_mono = (short *)prtd->dma_buf;
	} else {
		prtd->dma_pos = dma_pos;
	}

	if (prtd->running && (prtd->dma_pos % prtd->dma_period) == 0)
		snd_pcm_period_elapsed(prtd->substream);
}

static int eax_dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long totbytes = params_buffer_bytes(params);

	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totbytes;

	spin_lock_irq(&prtd->lock);
	prtd->dma_period = params_period_bytes(params);
	prtd->dma_start = runtime->dma_addr;
	prtd->dma_pos = prtd->dma_start;
	prtd->dma_end = prtd->dma_start + totbytes;
	prtd->dma_buf = (u32 *)(runtime->dma_area);
	prtd->dma_mono = (short *)prtd->dma_buf;
	spin_unlock_irq(&prtd->lock);

	pr_info("EAX:%s:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d area=0x%x\n",
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
		prtd->dma_start, runtime->dma_bytes,
		params_period_bytes(params), params_periods(params),
		(unsigned int)runtime->dma_area);

	return 0;
}

static int eax_dma_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, NULL);

	eax_adma_hw_free();

	return 0;
}

static int eax_dma_prepare(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	prtd->dma_pos = prtd->dma_start;
	prtd->dma_mono = (short *)prtd->dma_buf;

	eax_adma_hw_params();
	eax_adma_prepare();

	return ret;
}

static int eax_dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->running = true;
		eax_mixer_trigger(true);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		prtd->running = false;
		eax_mixer_trigger(false);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&prtd->lock);

	return ret;
}

static snd_pcm_uframes_t eax_dma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long res;

	pr_debug("Entered %s\n", __func__);

	res = prtd->dma_pos - prtd->dma_start;

	pr_debug("Pointer offset: %lu\n", res);

	if (res >= snd_pcm_lib_buffer_bytes(substream)) {
		if (res == snd_pcm_lib_buffer_bytes(substream))
			res = 0;
	}

	return bytes_to_frames(substream->runtime, res);
}

static int eax_dma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd;

	pr_debug("Entered %s\n", __func__);

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_soc_set_runtime_hwparams(substream, &dma_hardware);

	prtd = kzalloc(sizeof(struct runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	runtime->private_data = prtd;
	prtd->substream = substream;

	eax_mixer_add(prtd);

	return 0;
}

static int eax_dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	pr_debug("Entered %s\n", __func__);

	eax_mixer_remove(prtd);

	if (!prtd)
		pr_debug("dma_close called with prtd == NULL\n");

	kfree(prtd);

	return 0;
}

static int eax_dma_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("Entered %s\n", __func__);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

static struct snd_pcm_ops eax_dma_ops = {
	.open		= eax_dma_open,
	.close		= eax_dma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= eax_dma_hw_params,
	.hw_free	= eax_dma_hw_free,
	.prepare	= eax_dma_prepare,
	.trigger	= eax_dma_trigger,
	.pointer	= eax_dma_pointer,
	.mmap		= eax_dma_mmap,
};

static int eax_prealloc_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = dma_hardware.buffer_bytes_max;

	pr_debug("Entered %s\n", __func__);

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;

	return 0;
}

static u64 eax_dma_mask = DMA_BIT_MASK(32);
static int eax_dma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &eax_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = eax_prealloc_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = eax_prealloc_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
out:
	return ret;
}

static void eax_dma_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	pr_debug("Entered %s\n", __func__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		kfree(buf->area);
		buf->area = NULL;
	}
}

static struct snd_soc_platform_driver eax_asoc_platform = {
	.ops		= &eax_dma_ops,
	.pcm_new	= eax_dma_new,
	.pcm_free	= eax_dma_free,
};

int eax_asoc_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &eax_asoc_platform);
}
EXPORT_SYMBOL_GPL(eax_asoc_platform_register);

void eax_asoc_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(eax_asoc_platform_unregister);

static inline bool eax_mixer_any_buf_running(void)
{
	struct buf_info *bi;

	list_for_each_entry(bi, &buf_list, node) {
		if (bi->prtd && bi->prtd->running)
			return true;
	}

	return false;
}

static void eax_mixer_prepare(void)
{
	struct buf_info *bi;
	short mix_l, mix_r;
	short *mix_buf;
	short pcm_l, pcm_r;
	int n;

	if (mi.buf_fill)
		return;

	spin_lock(&mi.lock);
	mix_buf = mi.mix_buf;

	for (n = 0; n < MIXBUF_SIZE; n++) {
		mix_l = 0;
		mix_r = 0;

		list_for_each_entry(bi, &buf_list, node) {
			if (bi->prtd && bi->prtd->running) {
				eax_dma_xfer(bi->prtd, &pcm_l, &pcm_r);
				mix_l += pcm_l;
				mix_r += pcm_r;
			}
		}

		*mix_buf++ = mix_l;
		*mix_buf++ = mix_r;
	}

	mi.buf_fill = true;
	spin_unlock(&mi.lock);
}

static void eax_mixer_write(void)
{
	int ret;

	spin_lock(&mi.lock);
	if (!eax_mixer_any_buf_running()) {
		spin_unlock(&mi.lock);
		return;
	}
	spin_unlock(&mi.lock);

	if (!di.running && di.buf_fill[DMA_START_THRESHOLD])
		eax_adma_trigger(true);

	if (di.buf_fill[di.buf_wr_idx]) {
		if (!di.running)
			return;

		di.buf_done = false;
		ret = wait_event_interruptible_timeout(mixer_buf_wq,
						di.buf_done, HZ / 50);
		if (!ret)
			return;
	}

	memcpy(di.buf_wr_p[di.buf_wr_idx], mi.mix_buf, MIXBUF_BYTE);

	mi.buf_fill = false;
	di.buf_fill[di.buf_wr_idx] = true;
	di.buf_wr_idx++;
	if (di.buf_wr_idx == DMA_PERIOD_CNT)
		di.buf_wr_idx = 0;
}

static int eax_mixer_kthread(void *arg)
{
	struct sched_param param_fifo = {.sched_priority = MAX_RT_PRIO >> 1};

	sched_setscheduler_nocheck(current, SCHED_FIFO, &param_fifo);

	while (!kthread_should_stop()) {
		wait_event_interruptible(mixer_run_wq, mi.running);
		eax_mixer_prepare();
		eax_mixer_write();
	}

	return 0;
}

static int eax_mixer_add(struct runtime_data *prtd)
{
	struct buf_info *bi;
	unsigned long flags;

	bi = kzalloc(sizeof(struct buf_info), GFP_KERNEL);
	if (!bi) {
		pr_err("%s: Memory alloc fails!\n", __func__);
		return -ENOMEM;
	}

	bi->prtd = prtd;

	spin_lock_irqsave(&mi.lock, flags);
	list_add(&bi->node, &buf_list);
	spin_unlock_irqrestore(&mi.lock, flags);

	pr_debug("%s: prtd %p added\n", __func__, prtd);

	return 0;
}

static int eax_mixer_remove(struct runtime_data *prtd)
{
	struct buf_info *bi;
	unsigned long flags;
	bool node_found = false;

	spin_lock_irqsave(&mi.lock, flags);
	list_for_each_entry(bi, &buf_list, node) {
		if (bi->prtd == prtd) {
			node_found = true;
			break;
		}
	}

	if (!node_found) {
		spin_unlock_irqrestore(&mi.lock, flags);
		pr_err("%s: prtd %p not found\n", __func__, prtd);
		return -EINVAL;
	}

	list_del(&bi->node);
	spin_unlock_irqrestore(&mi.lock, flags);
	pr_debug("%s: prtd %p removed\n", __func__, prtd);

	return 0;
}

static void eax_mixer_trigger(bool on)
{
	if (on) {
		mi.running = true;
		if (waitqueue_active(&mixer_run_wq))
			wake_up_interruptible(&mixer_run_wq);
	} else {
		if (!eax_mixer_any_buf_running()) {
			if (di.running)
				eax_adma_trigger(false);

			mi.running = false;
		}
	}
}

MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC EAX-DMA Driver");
MODULE_LICENSE("GPL");
