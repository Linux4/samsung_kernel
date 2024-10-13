// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC - Samsung Abox Virtual DMA driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/memblock.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "abox_util.h"
#include "abox.h"
#include "abox_ion.h"
#include "abox_shm.h"
#include "abox_vdma.h"
#include "abox_memlog.h"
#include "abox_compress.h"

#define VDMA_COUNT_MAX SZ_32
#define NAME_LENGTH SZ_32
#define DEVICE_NAME "abox-vdma"

struct abox_vdma_rtd {
	struct snd_dma_buffer buffer;
	struct snd_pcm_hardware hardware;
	struct snd_pcm_substream *substream;
	struct abox_ion_buf *ion_buf;
	struct snd_hwdep *hwdep;
	unsigned long iova;
	size_t pointer;
	bool iommu_mapped;
	bool dma_alloc;
};

struct abox_vdma_info {
	struct device *dev;
	int id;
	bool aaudio;
	char name[NAME_LENGTH];
	struct abox_vdma_rtd rtd[SNDRV_PCM_STREAM_LAST + 1];
	struct abox_compr_data compr_data;
};

static struct device *abox_vdma_dev_abox;
static struct abox_vdma_info abox_vdma_list[VDMA_COUNT_MAX];
static const struct snd_compr_caps abox_vdma_compr_caps = {
	.direction		= SND_COMPRESS_CAPTURE,
	.min_fragment_size	= SZ_2K,
	.max_fragment_size	= SZ_64K,
	.min_fragments		= 1,
	.max_fragments		= 4,
	.num_codecs		= 1,
	.codecs			= {
		SND_AUDIOCODEC_AAC,
	},
};

static int abox_vdma_get_idx(int id)
{
	return id - PCMTASK_VDMA_ID_BASE;
}

static unsigned long abox_vdma_get_iova(int id, int stream)
{
	int idx = abox_vdma_get_idx(id);
	long ret;

	switch (stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
	case SNDRV_PCM_STREAM_CAPTURE:
		ret = IOVA_VDMA_BUFFER(idx) + (SZ_512K * stream);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct abox_vdma_info *abox_vdma_get_info(int id)
{
	int idx = abox_vdma_get_idx(id);

	if (idx < 0 || idx >= ARRAY_SIZE(abox_vdma_list))
		return NULL;

	return &abox_vdma_list[idx];
}

static struct abox_vdma_rtd *abox_vdma_get_rtd(struct abox_vdma_info *info,
		int stream)
{
	if (!info || stream < 0 || stream >= ARRAY_SIZE(info->rtd))
		return NULL;

	return &info->rtd[stream];
}

static int abox_vdma_request_ipc(ABOX_IPC_MSG *msg, int atomic, int sync)
{
	return abox_request_ipc(abox_vdma_dev_abox, msg->ipcid, msg,
			sizeof(*msg), atomic, sync);
}

int abox_vdma_period_elapsed(struct abox_vdma_info *info,
		struct abox_vdma_rtd *rtd, size_t pointer)
{
	abox_dbg(info->dev, "%s[%c](%zx)\n", __func__,
			substream_to_char(rtd->substream), pointer);

	rtd->pointer = pointer;
	snd_pcm_period_elapsed(rtd->substream);

	return 0;
}

static int abox_vdma_compr_ipc_handler(struct abox_vdma_info *info,
		ABOX_IPC_MSG *msg)
{
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg->msg.pcmtask;
	struct abox_compr_data *data = &info->compr_data;
	struct snd_compr_stream *cstream;
	struct snd_compr_runtime *runtime;
	u32 encoded_size;

	switch (pcmtask_msg->msgtype) {
	case PCM_PLTDAI_POINTER:
		if (!data->cstream || !data->cstream->runtime) {
			abox_err(info->dev, "INTR_ENCODED: no runtime\n");
			break;
		}

		cstream = data->cstream;
		runtime = cstream->runtime;

		data->pcm_size = pcmtask_msg->param.compr_pointer.pcm_size;
		encoded_size = pcmtask_msg->param.compr_pointer.pointer;
		abox_dbg(info->dev, "ENCODED SIZE: %u, PCM SIZE: %u\n",
			encoded_size, data->pcm_size);

		/* update copied total bytes */
		data->copied_total += encoded_size;
		data->byte_offset += encoded_size;
		if (data->byte_offset >= runtime->buffer_size)
			data->byte_offset -= runtime->buffer_size;

		snd_compr_fragment_elapsed(cstream);

		if (!data->start && runtime->state != SNDRV_PCM_STATE_PAUSED) {
			/* writes must be restarted */
			abox_err(info->dev, "INTR_ENCODED: invalid state: %d\n",
					runtime->state);
			break;
		}
		break;
	case PCM_PLTDAI_CLOSED:
		abox_info(info->dev, "%s: INTR_DESTROY\n", __func__);
		complete(&data->destroyed);
		break;
	default:
		abox_warn(info->dev, "unknown message: %d\n", pcmtask_msg->msgtype);
		break;
	}
	return IRQ_HANDLED;
}

static int abox_vdma_compr_set_param(struct device *dev,
	struct snd_compr_stream *stream, struct snd_compr_runtime *runtime)
{
	int id = to_platform_device(dev)->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_compr_data *data = &info->compr_data;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;
	int ret;

	abox_info(dev, "%s buffer size: %llubytes\n", __func__, runtime->buffer_size);

	/* free memory allocated by ALSA */
	if (runtime->buffer != data->dma_area)
		kfree(runtime->buffer);

	runtime->buffer = data->dma_area;
	if (runtime->buffer_size > data->dma_size) {
		abox_err(dev, "allocated buffer size is smaller than requested(%llu > %zu)\n",
				runtime->buffer_size, data->dma_size);
		ret = -ENOMEM;
		goto error;
	}
	msg.ipcid = abox_stream_to_ipcid(stream->direction);
	msg.task_id = pcmtask_msg->channel_id = id;

	pcmtask_msg->msgtype = PCM_SET_BUFFER;
	pcmtask_msg->param.setbuff.addr = data->iova;
	pcmtask_msg->param.setbuff.size = runtime->fragment_size;
	pcmtask_msg->param.setbuff.count = runtime->fragments;
	ret = abox_vdma_request_ipc(&msg, 0, 0);
	if (ret < 0)
		goto error;

	pcmtask_msg->msgtype = PCM_PLTDAI_HW_PARAMS;
	pcmtask_msg->param.hw_params.sample_rate = data->sample_rate;
	pcmtask_msg->param.hw_params.bit_depth = data->bit_depth;
	pcmtask_msg->param.hw_params.channels = data->channels;
	pcmtask_msg->param.hw_params.ip_type = data->codec_id;
	ret = abox_vdma_request_ipc(&msg, 0, 0);
	if (ret < 0)
		goto error;

error:
	return ret;
}

static int abox_vdma_compr_open(struct snd_soc_component *component,
		struct snd_compr_stream *stream)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_compr_data *data = &info->compr_data;
	struct abox_data *abox_data = dev_get_drvdata(abox_vdma_dev_abox);
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_info(dev, "%s[%c]\n", __func__,
	(stream->direction == SNDRV_PCM_STREAM_PLAYBACK) ? 'p' : 'c');

	abox_wait_restored(abox_data);

	/* init runtime data */
	data->cstream = stream;
	data->byte_offset = 0;
	data->copied_total = 0;
	data->bit_depth = 16;
	data->start = false;
	reinit_completion(&data->destroyed);

	abox_set_system_state(abox_data, SYSTEM_OFFLOAD, true);

	msg.ipcid = abox_stream_to_ipcid(stream->direction);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_OPEN;
	return abox_vdma_request_ipc(&msg, 0, 0);

}

static int abox_vdma_compr_free(struct snd_soc_component *component,
		struct snd_compr_stream *stream)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_compr_data *data = &info->compr_data;
	struct abox_data *abox_data = dev_get_drvdata(abox_vdma_dev_abox);
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;
	int ret = 0;

	abox_info(dev, "%s[%c]\n", __func__,
	(stream->direction == SNDRV_PCM_STREAM_PLAYBACK) ? 'p' : 'c');

	msg.ipcid = abox_stream_to_ipcid(stream->direction);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_HW_FREE;
	abox_vdma_request_ipc(&msg, 0, 0);

	if (ret >= 0) {
		ret = wait_for_completion_timeout(&data->destroyed,
				abox_get_waiting_jiffies(true));
		if (!ret) {
			abox_err(dev, "CMD_COMPR_DESTROY time out\n");
			ret = -EBUSY;
		} else {
			ret = 0;
		}
	}

	/* prevent kfree in ALSA */
	stream->runtime->buffer = NULL;

	abox_set_system_state(abox_data, SYSTEM_OFFLOAD, false);

	return ret;
}

static int abox_vdma_compr_set_params(struct snd_soc_component *component,
		struct snd_compr_stream *stream,
		struct snd_compr_params *params)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_compr_data *data = &info->compr_data;
	struct snd_compr_runtime *runtime = stream->runtime;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	/* COMPR set_params */
	memcpy(&data->codec_param, params, sizeof(data->codec_param));

	data->byte_offset = 0;
	data->copied_total = 0;
	data->stream_format = STREAM_FORMAT_DEFAULT;
	data->channels = data->codec_param.codec.ch_in;
	data->sample_rate = data->codec_param.codec.sample_rate;

	if (data->sample_rate == 0 ||
		data->channels == 0) {
		abox_err(dev, "%s: invalid parameters: sample(%u), ch(%u)\n",
				__func__, data->sample_rate, data->channels);
		return -EINVAL;
	}

	switch (params->codec.id) {
	case SND_AUDIOCODEC_MP3:
		data->codec_id = COMPR_MP3;
		break;
	case SND_AUDIOCODEC_AAC:
		data->codec_id = COMPR_AAC;
		if (params->codec.format == SND_AUDIOSTREAMFORMAT_MP4ADTS)
			data->stream_format = STREAM_FORMAT_ADTS;
		break;
	case SND_AUDIOCODEC_FLAC:
		data->codec_id = COMPR_FLAC;
		break;
	default:
		abox_err(dev, "%s: unknown codec id %d\n", __func__,
				params->codec.id);
		break;
	}

	ret = abox_vdma_compr_set_param(dev, stream, runtime);
	if (ret) {
		abox_err(dev, "%s: compr_set_param fail(%d)\n", __func__,
				ret);
		return ret;
	}

	abox_info(dev, "%s: sample rate:%u, channels:%u, codec id:%d\n", __func__,
			data->sample_rate, data->channels, data->codec_id);
	return ret;
}

static int abox_vdma_compr_trigger(struct snd_soc_component *component,
		struct snd_compr_stream *stream, int cmd)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_compr_data *data = &info->compr_data;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	int ret = 0;

	abox_info(dev, "%s(%d)\n", __func__, cmd);

	msg.ipcid = abox_stream_to_ipcid(stream->direction);
	pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_TRIGGER;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		abox_dbg(dev, "SNDRV_PCM_TRIGGER_PAUSE_PUSH\n");
		pcmtask_msg->param.trigger = SNDRV_PCM_TRIGGER_PAUSE_PUSH;
		ret = abox_vdma_request_ipc(&msg, 0, 0);
		if (ret < 0)
			abox_err(dev, "%s: pause cmd failed(%d)\n", __func__,
					ret);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		abox_dbg(dev, "SNDRV_PCM_TRIGGER_STOP\n");
		pcmtask_msg->param.trigger = SNDRV_PCM_TRIGGER_STOP;
		ret = abox_vdma_request_ipc(&msg, 0, 0);
		if (ret < 0)
			abox_err(dev, "%s: pause cmd failed(%d)\n", __func__,
					ret);

		data->start = false;

		/* reset */
		data->byte_offset = 0;
		data->copied_total = 0;
		data->pcm_size = 0;
		break;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		abox_dbg(dev, "%s: %s", __func__,
				(cmd == SNDRV_PCM_TRIGGER_START) ?
				"SNDRV_PCM_TRIGGER_START" :
				"SNDRV_PCM_TRIGGER_PAUSE_RELEASE");

		data->start = true;
		pcmtask_msg->param.trigger = SNDRV_PCM_TRIGGER_START;
		ret = abox_vdma_request_ipc(&msg, 0, 0);
		if (ret < 0)
			abox_err(dev, "%s: start cmd failed\n", __func__);

		break;
	default:
		abox_warn(dev, "Invalid command: %d\n", cmd);
		break;
	}

	return 0;
}

static int abox_vdma_compr_pointer(struct snd_soc_component *component,
		struct snd_compr_stream *stream,
		struct snd_compr_tstamp *tstamp)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_compr_data *data = &info->compr_data;
	unsigned int num_channel;
	u32 pcm_size;
	struct snd_compr_runtime *runtime = stream->runtime;

	abox_dbg(dev, "%s\n", __func__);

	tstamp->sampling_rate = data->sample_rate;
	tstamp->byte_offset = data->byte_offset;
	tstamp->copied_total = data->copied_total;

	pcm_size = data->pcm_size;

	/* set the number of channels */
	num_channel = hweight32(data->channels);

	if (pcm_size) {
		tstamp->pcm_io_frames = pcm_size / (2 * num_channel);
		abox_dbg(dev, "%s: pcm(%u), frame(%u), total_avail(%u), total_trans(%u)\n",
				__func__, pcm_size, tstamp->pcm_io_frames,
				 runtime->total_bytes_available, runtime->total_bytes_transferred);
	}

	return 0;
}

static int abox_vdma_compr_read_data(struct snd_soc_component *component,
		struct snd_compr_stream *stream,
	       char __user *buf, size_t count)
{
	void *src;
	size_t copy;
	struct snd_compr_runtime *runtime = stream->runtime;
	/* 64-bit Modulus */
	u64 hw_pointer = div64_u64(runtime->total_bytes_transferred,
				    runtime->buffer_size);
	hw_pointer = runtime->total_bytes_transferred -
		      (hw_pointer * runtime->buffer_size);
	src = runtime->buffer + hw_pointer;

	pr_debug("%s, copying %ld at %lld\n", __func__, (unsigned long)count, hw_pointer);

	if (count < runtime->buffer_size - hw_pointer) {
		if (copy_to_user(buf, src, count))
			return -EFAULT;
	} else {
		copy = runtime->buffer_size - hw_pointer;
		count = copy;
		if (copy_to_user(buf, src, copy))
			return -EFAULT;
		if (copy_to_user(buf + copy, runtime->buffer, count - copy))
			return -EFAULT;
	}

	return count;
}

static int abox_vdma_compr_copy(struct snd_soc_component *component,
		struct snd_compr_stream *stream,
		char __user *buf, size_t count)
{
	struct device *dev = component->dev;

	abox_dbg(dev, "%s\n", __func__);

	return abox_vdma_compr_read_data(component, stream, buf, count);
}

static int abox_vdma_compr_get_caps(struct snd_soc_component *component,
		struct snd_compr_stream *stream,
		struct snd_compr_caps *caps)
{
	struct device *dev = component->dev;

	abox_info(dev, "%s\n", __func__);

	memcpy(caps, &abox_vdma_compr_caps, sizeof(*caps));

	return 0;
}

static int abox_vdma_compr_get_codec_caps(struct snd_soc_component *component,
		struct snd_compr_stream *stream,
		struct snd_compr_codec_caps *codec)
{
	/*Nothing to do*/

	return 0;
}

static int abox_vdma_compr_set_metadata(struct snd_soc_component *component,
		struct snd_compr_stream *stream,
		struct snd_compr_metadata *metadata)
{
	/*Nothing to do*/

	return 0;
}

static int abox_vdma_compr_probe(struct device *dev, int dir)
{
	int id = to_platform_device(dev)->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_compr_data *data = &info->compr_data;
	int ret;

	abox_info(dev, "%s\n", __func__);

	data->dma_size = abox_vdma_compr_caps.max_fragments *
			abox_vdma_compr_caps.max_fragment_size;
	data->dma_area = dmam_alloc_coherent(dev,
			data->dma_size, &data->dma_addr,
			GFP_KERNEL);
	data->iova = abox_vdma_get_iova(id, dir);

	init_completion(&data->destroyed);

	if (data->dma_area == NULL) {
		abox_err(dev, "dma memory allocation failed: %lu\n",
				PTR_ERR(data->dma_area));
		return -ENOMEM;
	}
	ret = abox_iommu_map(abox_vdma_dev_abox, data->iova,
			data->dma_addr,
			PAGE_ALIGN(data->dma_size),
			data->dma_area);
	if (ret < 0) {
		abox_err(dev, "dma memory iommu map failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static void abox_vdma_compr_remove(struct snd_soc_component *component)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_compr_data *data = &info->compr_data;

	abox_iommu_unmap(abox_vdma_dev_abox, data->iova);
}

static int abox_vdma_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_info(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	abox_wait_restored(dev_get_drvdata(abox_vdma_dev_abox));

	rtd->substream = substream;
	snd_soc_set_runtime_hwparams(substream, &rtd->hardware);

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_OPEN;
	return abox_vdma_request_ipc(&msg, 0, 0);
}

static int abox_vdma_close(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_info(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	rtd->substream = NULL;

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_CLOSE;
	return abox_vdma_request_ipc(&msg, 0, 1);
}

static int abox_vdma_hw_params(struct snd_soc_component *component,
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	struct snd_dma_buffer *dmab = &substream->dma_buffer;
	size_t buffer_bytes = params_buffer_bytes(params);
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;
	int ret;

	abox_dbg(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	if (!dmab->area && dmab->bytes >= buffer_bytes) {
		snd_pcm_set_runtime_buffer(substream, dmab);
		substream->runtime->dma_bytes = buffer_bytes;
	} else {
		ret = snd_pcm_lib_malloc_pages(substream, buffer_bytes);
		if (ret < 0)
			return ret;
	}

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;

	pcmtask_msg->msgtype = PCM_SET_BUFFER;
	pcmtask_msg->param.setbuff.addr = rtd->iova;
	pcmtask_msg->param.setbuff.size = params_period_bytes(params);
	pcmtask_msg->param.setbuff.count = params_periods(params);
	ret = abox_vdma_request_ipc(&msg, 0, 0);
	if (ret < 0)
		return ret;

	pcmtask_msg->msgtype = PCM_PLTDAI_HW_PARAMS;
	pcmtask_msg->param.hw_params.sample_rate = params_rate(params);
	pcmtask_msg->param.hw_params.bit_depth = params_width(params);
	pcmtask_msg->param.hw_params.channels = params_channels(params);
	ret = abox_vdma_request_ipc(&msg, 0, 0);
	if (ret < 0)
		return ret;

	abox_info(dev, "%s:Total=%u PrdSz=%u(%u) #Prds=%u rate=%u, width=%d, channels=%u\n",
			snd_pcm_stream_str(substream),
			params_buffer_bytes(params), params_period_size(params),
			params_period_bytes(params), params_periods(params),
			params_rate(params), params_width(params),
			params_channels(params));

	return ret;
}

static int abox_vdma_hw_free(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_HW_FREE;
	abox_vdma_request_ipc(&msg, 0, 0);

	return snd_pcm_lib_free_pages(substream);
}

static int abox_vdma_prepare(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	rtd->pointer = 0;
	abox_shm_write_vdma_appl_ptr(abox_vdma_get_idx(id), 0);

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_PREPARE;
	return abox_vdma_request_ipc(&msg, 0, 0);
}

static int abox_vdma_trigger(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;
	int ret;

	abox_info(dev, "%s[%c](%d)\n", __func__,
			substream_to_char(substream), cmd);

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_TRIGGER;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pcmtask_msg->param.trigger = 1;
		ret = abox_vdma_request_ipc(&msg, 1, 0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pcmtask_msg->param.trigger = 0;
		ret = abox_vdma_request_ipc(&msg, 1, 0);
		break;
	default:
		abox_err(dev, "invalid command: %d\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t abox_vdma_pointer(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	size_t pointer;

	abox_dbg(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	pointer = abox_shm_read_vdma_hw_ptr(abox_vdma_get_idx(id));

	if (!pointer)
		pointer = rtd->pointer;

	if (pointer >= rtd->iova)
		pointer -= rtd->iova;

	return bytes_to_frames(substream->runtime, pointer);
}

static int abox_vdma_mmap(struct snd_soc_component *component,
		struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;

	abox_info(dev, "%s\n", __func__);

	if (!IS_ERR_OR_NULL(rtd->ion_buf))
		return dma_buf_mmap(rtd->ion_buf->dma_buf, vma, 0);
	else
		return dma_mmap_wc(dev, vma, runtime->dma_area,
				runtime->dma_addr, runtime->dma_bytes);
}

static int abox_vdma_copy_user_dma_area(struct snd_pcm_substream *substream,
		int channel, unsigned long pos, void __user *buf,
		unsigned long bytes)
{
	struct snd_pcm_runtime *pcm_rtd = substream->runtime;
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	void *ptr;

	abox_dbg(info->dev, "%s[%c](%ld, %ld)\n", __func__,
			substream_to_char(substream), pos, bytes);

	ptr = pcm_rtd->dma_area + pos + channel *
			(pcm_rtd->dma_bytes / pcm_rtd->channels);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (copy_from_user(ptr, buf, bytes))
			return -EFAULT;
	} else {
		if (copy_to_user(buf, ptr, bytes))
			return -EFAULT;
	}

	return 0;
}

static int abox_vdma_copy_user(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int channel,
		unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct snd_pcm_runtime *pcm_rtd = substream->runtime;
	struct snd_soc_pcm_runtime *soc_rtd = asoc_substream_to_rtd(substream);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	unsigned long appl_bytes = (pos + bytes) % pcm_rtd->dma_bytes;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_vdma_copy_user_dma_area(substream, channel, pos, buf, bytes);

	/* Firmware doesn't need ack of capture stream. */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return 0;

	abox_dbg(dev, "%s[%c]: %ld\n", __func__,
			substream_to_char(substream), appl_bytes);

	abox_shm_write_vdma_appl_ptr(abox_vdma_get_idx(id), (u32)appl_bytes);

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_ACK;
	pcmtask_msg->param.pointer = (unsigned int)appl_bytes;

	return abox_vdma_request_ipc(&msg, 1, 0);
}

static int abox_vdma_probe(struct snd_soc_component *component)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;

	abox_dbg(dev, "%s\n", __func__);

	snd_soc_component_set_drvdata(component, abox_vdma_get_info(id));
	return 0;
}

static int abox_vdma_alloc_ion_buf(struct snd_soc_pcm_runtime *soc_rtd,
		struct abox_vdma_info *info, int stream)
{
	struct device *dev = info->dev;
	struct abox_vdma_rtd *rtd = &info->rtd[stream];
	struct abox_data *abox_data = dev_get_drvdata(abox_vdma_dev_abox);
	bool playback = (stream == SNDRV_PCM_STREAM_PLAYBACK);
	int ret;

	rtd->ion_buf = abox_ion_alloc(dev, abox_data, rtd->iova,
			rtd->buffer.bytes, playback);
	if (IS_ERR(rtd->ion_buf)) {
		ret = PTR_ERR(rtd->ion_buf);
		rtd->ion_buf = NULL;
		return ret;
	}

	ret = abox_ion_new_hwdep(soc_rtd, rtd->ion_buf, &rtd->hwdep);
	if (ret < 0) {
		abox_err(dev, "adding hwdep failed: %d\n", ret);
		abox_ion_free(dev, abox_data, rtd->ion_buf);
		return ret;
	}

	/* update buffer using ion allocated buffer */
	rtd->buffer.area = rtd->ion_buf->kva;
	rtd->buffer.addr = rtd->ion_buf->iova;

	return 0;
}

static int abox_vdma_alloc_dma_buf(struct snd_soc_pcm_runtime *soc_rtd,
		struct abox_vdma_info *info, int stream)
{
	struct device *dev = info->dev;
	struct abox_vdma_rtd *rtd = &info->rtd[stream];

	rtd->dma_alloc = true;
	return snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, dev, rtd->buffer.bytes,
			&rtd->buffer);
}

static int abox_vdma_pcm_new(struct snd_soc_component *component,
		struct snd_soc_pcm_runtime *soc_rtd)
{
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	struct device *dev_abox = abox_vdma_dev_abox;
	struct snd_pcm *pcm = soc_rtd->pcm;
	int i, ret;

	abox_dbg(dev, "%s\n", __func__);

	for (i = 0; i <= SNDRV_PCM_STREAM_LAST; i++) {
		struct snd_pcm_substream *substream = pcm->streams[i].substream;
		struct abox_vdma_rtd *rtd = &info->rtd[i];

		if (!substream)
			continue;

		if (rtd->iova == 0)
			rtd->iova = abox_vdma_get_iova(id, i);

		if (rtd->buffer.bytes == 0)
			rtd->buffer.bytes = BUFFER_BYTES_MIN;

		if (!rtd->buffer.addr) {
			if (info->aaudio)
				ret = abox_vdma_alloc_ion_buf(soc_rtd, info, i);
			else
				ret = abox_vdma_alloc_dma_buf(soc_rtd, info, i);
			if (ret < 0)
				return ret;
		}
		substream->dma_buffer = rtd->buffer;

		if (abox_iova_to_phys(dev_abox, rtd->iova) == 0) {
			ret = abox_iommu_map(dev_abox, rtd->iova,
					substream->dma_buffer.addr,
					substream->dma_buffer.bytes,
					substream->dma_buffer.area);
			if (ret < 0)
				return ret;

			rtd->iommu_mapped = true;
		}

		rtd->substream = substream;
	}

	return 0;
}

static void abox_vdma_pcm_free(struct snd_soc_component *component,
		struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *soc_rtd = snd_pcm_chip(pcm);
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	struct device *dev_abox = abox_vdma_dev_abox;
	struct abox_data *abox_data = dev_get_drvdata(dev_abox);
	int i;

	abox_dbg(dev, "%s\n", __func__);

	for (i = 0; i <= SNDRV_PCM_STREAM_LAST; i++) {
		struct snd_pcm_substream *substream = pcm->streams[i].substream;

		if (!substream)
			continue;

		info->rtd[i].substream = NULL;

		if (info->rtd[i].iommu_mapped) {
			abox_iommu_unmap(dev_abox, info->rtd[i].iova);
			info->rtd[i].iommu_mapped = false;
		}

		if (info->rtd[i].ion_buf)
			abox_ion_free(dev, abox_data, info->rtd[i].ion_buf);
		else if (info->rtd[i].dma_alloc)
			snd_dma_free_pages(&info->rtd[i].buffer);

		substream->dma_buffer.area = NULL;
	}
}

static struct snd_compress_ops abox_vdma_compr_ops = {
	.open		= abox_vdma_compr_open,
	.free		= abox_vdma_compr_free,
	.set_params	= abox_vdma_compr_set_params,
	.set_metadata	= abox_vdma_compr_set_metadata,
	.trigger	= abox_vdma_compr_trigger,
	.pointer	= abox_vdma_compr_pointer,
	.copy		= abox_vdma_compr_copy,
	.get_caps	= abox_vdma_compr_get_caps,
	.get_codec_caps	= abox_vdma_compr_get_codec_caps,
};

static const struct snd_soc_component_driver abox_vdma_compr_component = {
	.probe		= abox_vdma_probe,
	.remove		= abox_vdma_compr_remove,
	.compress_ops	= &abox_vdma_compr_ops,
};

static const struct snd_soc_component_driver abox_vdma_component = {
	.probe		= abox_vdma_probe,
	.pcm_construct	= abox_vdma_pcm_new,
	.pcm_destruct	= abox_vdma_pcm_free,
	.open		= abox_vdma_open,
	.close		= abox_vdma_close,
	.hw_params	= abox_vdma_hw_params,
	.hw_free	= abox_vdma_hw_free,
	.prepare	= abox_vdma_prepare,
	.trigger	= abox_vdma_trigger,
	.pointer	= abox_vdma_pointer,
	.copy_user	= abox_vdma_copy_user,
	.mmap		= abox_vdma_mmap,
};

static irqreturn_t abox_vdma_ipc_handler(int ipc_id, void *dev_id,
		ABOX_IPC_MSG *msg)
{
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg->msg.pcmtask;
	int id = pcmtask_msg->channel_id;
	int stream = abox_ipcid_to_stream(ipc_id);
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, stream);

	if (!info || !rtd)
		return IRQ_NONE;

	if (id == COMPR_CAPTURE_BASE_ID)
		return abox_vdma_compr_ipc_handler(info, msg);

	switch (pcmtask_msg->msgtype) {
	case PCM_PLTDAI_POINTER:
		abox_vdma_period_elapsed(info, rtd, pcmtask_msg->param.pointer);
		break;
	case PCM_PLTDAI_ACK:
		/* Ignore ack request and always send ack IPC to firmware. */
		break;
	default:
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static struct snd_soc_dai_link abox_vdma_dai_links[VDMA_COUNT_MAX];

static struct snd_soc_card abox_vdma_card = {
	.name = "abox_vdma",
	.owner = THIS_MODULE,
	.dai_link = abox_vdma_dai_links,
	.num_links = 0,
};

SND_SOC_DAILINK_DEF(dailink_comp_dummy, DAILINK_COMP_ARRAY(COMP_DUMMY()));

static void abox_vdma_register_card_work_func(struct work_struct *work)
{
	int i;

	abox_dbg(abox_vdma_dev_abox, "%s\n", __func__);

	for (i = 0; i < abox_vdma_card.num_links; i++) {
		struct snd_soc_dai_link *link = &abox_vdma_card.dai_link[i];

		if (link->name)
			continue;

		link->name = link->stream_name =
				kasprintf(GFP_KERNEL, "dummy%d", i);
		link->cpus = dailink_comp_dummy;
		link->num_cpus = ARRAY_SIZE(dailink_comp_dummy);
		link->codecs = dailink_comp_dummy;
		link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
		link->no_pcm = 1;
	}
	abox_register_extra_sound_card(abox_vdma_card.dev, &abox_vdma_card, 1);
}

static DECLARE_DELAYED_WORK(abox_vdma_register_card_work,
		abox_vdma_register_card_work_func);

struct device *abox_vdma_register_component(struct device *dev,
		int id, const char *name, bool aaudio,
		struct snd_pcm_hardware *playback,
		struct snd_pcm_hardware *capture)
{
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd;

	if (!info)
		return ERR_PTR(-EINVAL);

	abox_dbg(dev, "%s(%d, %s, %d, %d)\n", __func__, id, name,
			!!playback, !!capture);

	info->id = id;
	strlcpy(info->name, name, sizeof(info->name));
	info->aaudio = aaudio;

	if (playback) {
		rtd = abox_vdma_get_rtd(info, SNDRV_PCM_STREAM_PLAYBACK);
		rtd->hardware = *playback;
		rtd->hardware.info = SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID;
		rtd->buffer.dev.type = SNDRV_DMA_TYPE_DEV;
		rtd->buffer.dev.dev = dev;
		rtd->buffer.bytes = playback->buffer_bytes_max;
	}

	if (capture) {
		rtd = abox_vdma_get_rtd(info, SNDRV_PCM_STREAM_CAPTURE);
		rtd->hardware = *capture;
		rtd->hardware.info = SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID;
		rtd->buffer.dev.type = SNDRV_DMA_TYPE_DEV;
		rtd->buffer.dev.dev = dev;
		rtd->buffer.bytes = capture->buffer_bytes_max;
	}

	return info->dev;
}

static int samsung_abox_vdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int id = pdev->id;
	struct abox_vdma_info *info;
	int ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	if (id < 0) {
		abox_vdma_card.dev = dev;
		schedule_delayed_work(&abox_vdma_register_card_work, 0);
	} else {
		info = abox_vdma_get_info(id);
		if (!info) {
			abox_err(dev, "invalid id: %d\n", id);
			return -EINVAL;
		}

		info->dev = dev;
		dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));
		pm_runtime_no_callbacks(dev);
		pm_runtime_enable(dev);

		if (id == COMPR_CAPTURE_BASE_ID) {
			ret = devm_snd_soc_register_component(dev, &abox_vdma_compr_component,
					NULL, 0);
			abox_vdma_compr_probe(dev, SNDRV_PCM_STREAM_CAPTURE);
		} else
			ret = devm_snd_soc_register_component(dev, &abox_vdma_component,
					NULL, 0);
		if (ret < 0)
			abox_err(dev, "register component failed: %d\n", ret);
	}

	return ret;
}

static int samsung_abox_vdma_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);

	return 0;
}

static void samsung_abox_vdma_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);
	pm_runtime_disable(dev);
}

static const struct platform_device_id samsung_abox_vdma_driver_ids[] = {
	{
		.name = DEVICE_NAME,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, samsung_abox_vdma_driver_ids);

struct platform_driver samsung_abox_vdma_driver = {
	.probe  = samsung_abox_vdma_probe,
	.remove = samsung_abox_vdma_remove,
	.shutdown = samsung_abox_vdma_shutdown,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = samsung_abox_vdma_driver_ids,
};

void abox_vdma_register_card(struct device *dev_abox)
{
	if (!dev_abox)
		return;

	platform_device_register_data(dev_abox, DEVICE_NAME, -1, NULL, 0);
}

void abox_vdma_init(struct device *dev_abox)
{
	struct platform_device *pdev;
	int i;

	abox_info(dev_abox, "%s\n", __func__);

	abox_vdma_dev_abox = dev_abox;
	abox_register_ipc_handler(dev_abox, IPC_PCMPLAYBACK,
			abox_vdma_ipc_handler, dev_abox);
	abox_register_ipc_handler(dev_abox, IPC_PCMCAPTURE,
			abox_vdma_ipc_handler, dev_abox);

	for (i = 0; i < VDMA_COUNT_MAX; i++) {
		pdev = platform_device_register_data(dev_abox, DEVICE_NAME,
				PCMTASK_VDMA_ID_BASE + i, NULL, 0);
		if (IS_ERR(pdev))
			abox_err(dev_abox, "vdma(%d) register failed: %ld\n",
					i, PTR_ERR(pdev));
	}
}
