/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#define pr_fmt(fmt) "[saudio] " fmt

#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/moduleparam.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#endif

#include <linux/sipc.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/memory.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <sound/saudio.h>

#define ETRACE(x...)           printk(KERN_ERR "Error: " x)
#define WTRACE(x...)		pr_debug(KERN_ERR x)

#define ADEBUG()			pr_debug(KERN_ERR "saudio.c: function: %s,line %d\n",__FUNCTION__,__LINE__)

#define CMD_BLOCK_SIZE			80
#define TX_DATA_BLOCK_SIZE		80
#define RX_DATA_BLOCK_SIZE		0
#define MAX_BUFFER_SIZE			(10*1024)

#define MAX_PERIOD_SIZE			MAX_BUFFER_SIZE

#define USE_FORMATS			(SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE)

#define USE_RATE			SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000
#define USE_RATE_MIN			5500
#define USE_RATE_MAX			48000

#define USE_CHANNELS_MIN		1

#define USE_CHANNELS_MAX		2

#define USE_PERIODS_MIN			1

#define USE_PERIODS_MAX			1024

#define CMD_TIMEOUT			msecs_to_jiffies(5000)
#define CMD_MODEM_RESET_TIMEOUT		msecs_to_jiffies(10000)

#define SAUDIO_CMD_NONE			0x00000000
#define SAUDIO_CMD_OPEN			0x00000001
#define SAUDIO_CMD_CLOSE		0x00000002
#define SAUDIO_CMD_START		0x00000004
#define SAUDIO_CMD_STOP			0x00000008
#define SAUDIO_CMD_PREPARE		0x00000010
#define SAUDIO_CMD_TRIGGER              0x00000020
#define SAUDIO_CMD_HANDSHAKE            0x00000040
#define SAUDIO_CMD_RESET		0x00000080

#define SAUDIO_CMD_OPEN_RET		0x00010000
#define SAUDIO_CMD_CLOSE_RET		0x00020000
#define SAUDIO_CMD_START_RET		0x00040000
#define SAUDIO_CMD_STOP_RET             0x00080000
#define SAUDIO_CMD_PREPARE_RET		0x00100000
#define SAUDIO_CMD_TRIGGER_RET		0x00200000
#define SAUDIO_CMD_HANDSHAKE_RET	0x00400000
#define SAUDIO_CMD_RESET_RET		0x00800000

#define SAUDIO_DATA_PCM			0x00000040
#define SAUDIO_DATA_SILENCE		0x00000080

#define PLAYBACK_DATA_ABORT		0x00000001
#define PLAYBACK_DATA_BREAK		0x00000002

#define  SAUDIO_DEV_CTRL_ABORT		0x00000001
#define SAUDIO_DEV_CTRL_BREAK		0x00000002

#define SAUDIO_SUBCMD_CAPTURE		0x0
#define SAUDIO_SUBCMD_PLAYBACK		0x1

#define SAUDIO_DEV_MAX			1
#define SAUDIO_STREAM_MAX		2
#define SAUDIO_CARD_NAME_LEN_MAX	16

#define  SAUDIO_STREAM_BLOCK_COUNT	4
#define  SAUDIO_CMD_BLOCK_COUNT		8

#define  SAUDIO_MONITOR_BLOCK_COUNT	2
#define  SAUDIO_MONITOR_BLOCK_COUNT	2

struct cmd_common {
	unsigned int command;
	unsigned int sub_cmd;
	unsigned int reserved1;
	unsigned int reserved2;
};

struct cmd_prepare {
	struct cmd_common common;
	unsigned int rate;	/* rate in Hz */
	unsigned char channels;	/* channels */
	unsigned char format;
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned int period;	/* period size */
	unsigned int periods;	/* periods */
};

struct cmd_open {
	struct cmd_common common;
	uint32_t stream_type;
};

struct saudio_msg {
	uint32_t command;
	uint32_t stream_id;
	uint32_t reserved1;
	uint32_t reserved2;
	void *param;
};

enum snd_status {
	SAUDIO_IDLE,
	SAUDIO_OPENNED,
	SAUDIO_CLOSED,
	SAUDIO_STOPPED,
	SAUDIO_PREPARED,
	SAUDIO_TRIGGERED,
	SAUDIO_PARAMIZED,
	SAUDIO_ABORT,
};

struct saudio_dev_ctrl;
struct snd_saudio;

struct saudio_stream {

	struct snd_saudio *saudio;
	struct snd_pcm_substream *substream;
	struct saudio_dev_ctrl *dev_ctrl;
	int stream_id;		/* numeric identification */

	uint32_t stream_state;

	uint32_t dst;
	uint32_t channel;

	int32_t period;
	int32_t periods_avail;
	int32_t periods_tosend;

	uint32_t hwptr_done;

	uint32_t last_elapsed_count;
	uint32_t last_getblk_count;
	uint32_t blk_count;

	struct mutex mutex;
};

struct saudio_dev_ctrl {
	uint32_t dev_state;
	struct mutex mutex;
	uint32_t dst;
	uint32_t monitor_channel;
	uint32_t channel;
	uint8_t name[SAUDIO_CARD_NAME_LEN_MAX];
	struct saudio_stream stream[SAUDIO_STREAM_MAX];
};

struct snd_saudio {
	struct snd_card *card;
	struct snd_pcm *pcm[SAUDIO_DEV_MAX];
	struct saudio_dev_ctrl dev_ctrl[SAUDIO_DEV_MAX];
	wait_queue_head_t wait;
	struct platform_device *pdev;
	struct workqueue_struct *queue;
	struct work_struct card_free_work;
	uint32_t dst;
	uint32_t channel;
	uint32_t in_init;
	struct task_struct * thread_id;
	int32_t state;
	struct mutex mutex;
};

static DEFINE_MUTEX(snd_sound);

static int saudio_snd_card_free(const struct snd_saudio *saudio);


static int saudio_clear_cmd(uint32_t dst, uint32_t channel)
{
	int result = 0;
	int i = 0;
	struct sblock blk = { 0 };
	do {
		result = sblock_receive(dst, channel,  (struct sblock *)&blk, 0);
		if (!result) {
			sblock_release(dst, channel, &blk);
		}
	} while (!result);
	return result;
}


static int saudio_send_common_cmd(uint32_t dst, uint32_t channel,
				  uint32_t cmd, uint32_t subcmd,
				  int32_t timeout)
{
	int result = 0;
	struct sblock blk = { 0 };
	ADEBUG();
	pr_debug(" dst is %d, channel %d, cmd %x, subcmd %x\n", dst, channel,
		 cmd, subcmd);
	saudio_clear_cmd( dst,  channel);
	result = sblock_get(dst, channel, (struct sblock *)&blk, timeout);
	if (result >= 0) {
		struct cmd_common *common = (struct cmd_common *)blk.addr;
		common->command = cmd;
		common->sub_cmd = subcmd;
		blk.length = sizeof(struct cmd_common);
		pr_debug(" dst is %d, channel %d, cmd %x, subcmd %x send ok\n",
			 dst, channel, cmd, subcmd);
		result = sblock_send(dst, channel, (struct sblock *)&blk);
	}
	return result;
}

static int saudio_wait_common_cmd(uint32_t dst, uint32_t channel,
				  uint32_t cmd, uint32_t subcmd,
				  int32_t timeout)
{
	int result = 0;
	struct sblock blk = { 0 };
	struct cmd_common *common = NULL;
	ADEBUG();
	result = sblock_receive(dst, channel, (struct sblock *)&blk, timeout);
	if (result < 0) {
		ETRACE("sblock_receive dst %d, channel %d result is %d \n", dst,
		       channel, result);
		return -1;
	}

	common = (struct cmd_common *)blk.addr;
	pr_debug("dst is %d, channel %d, common->command is %x ,sub cmd %x,\n",
		 dst, channel, common->command, common->sub_cmd);
	if (subcmd) {
		if ((common->command == cmd) && (common->sub_cmd == subcmd)) {
			result = 0;
		} else {
			result = -1;
		}
	} else {
		if (common->command == cmd) {
			result = 0;
		} else {
			result = -1;
		}
	}
	sblock_release(dst, channel, &blk);
	return result;
}

static int saudio_clear_ctrl_cmd(struct snd_saudio *saudio)
{
	int result = 0;
	int i = 0;
	struct sblock blk = { 0 };
	struct saudio_dev_ctrl *dev_ctrl = NULL;

	for (i = 0; i < SAUDIO_DEV_MAX; i++) {	/* now only support  one device */
		dev_ctrl = &saudio->dev_ctrl[i];
		do {
			result =
			    sblock_receive(dev_ctrl->dst, dev_ctrl->channel,
					   (struct sblock *)&blk, 0);
			if (!result) {
				sblock_release(dev_ctrl->dst, dev_ctrl->channel,
					       &blk);
			}
		} while (!result);
	}

	return result;
}


static int saudio_pcm_lib_malloc_pages(struct snd_pcm_substream *substream,
				       size_t size)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dmab = &substream->dma_buffer;

	/* Use the pre-allocated buffer */
	snd_pcm_set_runtime_buffer(substream, dmab);
	runtime->dma_bytes = size;
	return 1;
}

static int saudio_pcm_lib_free_pages(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int saudio_snd_mmap(struct snd_pcm_substream *substream,
			   struct vm_area_struct *vma)
{
	ADEBUG();
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return remap_pfn_range(vma, vma->vm_start,
			       substream->dma_buffer.addr >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static struct snd_pcm_hardware snd_card_saudio_playback = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = USE_FORMATS,
	.rates = USE_RATE,
	.rate_min = USE_RATE_MIN,
	.rate_max = USE_RATE_MAX,
	.channels_min = USE_CHANNELS_MIN,
	.channels_max = USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = USE_PERIODS_MIN,
	.periods_max = USE_PERIODS_MAX,
	.fifo_size = 0,
};

static struct snd_pcm_hardware snd_card_saudio_capture = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = USE_FORMATS,
	.rates = USE_RATE,
	.rate_min = USE_RATE_MIN,
	.rate_max = USE_RATE_MAX,
	.channels_min = USE_CHANNELS_MIN,
	.channels_max = USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = USE_PERIODS_MIN,
	.periods_max = USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int snd_card_saudio_pcm_open(struct snd_pcm_substream *substream)
{
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct saudio_stream *stream = NULL;
	int result = 0;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	ADEBUG();

	mutex_lock(&saudio->mutex);
	if(!saudio->state) {
		mutex_unlock(&saudio->mutex);
		printk("saudio.c: snd_pcm_open error saudio state %d\n",saudio->state);
		return -EIO;
	}
	mutex_unlock(&saudio->mutex);

	pr_info("%s IN, stream_id=%d\n", __func__, stream_id);
	dev_ctrl = (struct saudio_dev_ctrl *)&(saudio->dev_ctrl[dev]);
	stream = (struct saudio_stream *)&(dev_ctrl->stream[stream_id]);
	stream->substream = substream;
	stream->stream_id = stream_id;

	stream->period = 0;
	stream->periods_tosend = 0;
	stream->periods_avail = 0;
	stream->hwptr_done = 0;
	stream->last_getblk_count = 0;
	stream->last_elapsed_count = 0;
	stream->blk_count = SAUDIO_STREAM_BLOCK_COUNT;

	if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
		runtime->hw = snd_card_saudio_playback;
	} else {
		runtime->hw = snd_card_saudio_capture;
	}
	mutex_lock(&dev_ctrl->mutex);
	result = saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					SAUDIO_CMD_OPEN, stream_id,
					CMD_TIMEOUT);
	if (result) {
		ETRACE
		    ("saudio.c: snd_card_saudio_pcm_open: saudio_send_common_cmd result is %d",
		     result);
		if(result != (-ERESTARTSYS))
			saudio_snd_card_free(saudio);
		mutex_unlock(&dev_ctrl->mutex);
		return result;
	}
	pr_info("%s send cmd done\n", __func__);
	result = saudio_wait_common_cmd(dev_ctrl->dst,
					dev_ctrl->channel,
					SAUDIO_CMD_OPEN_RET, 0, CMD_TIMEOUT);
	if (result && (result != (-ERESTARTSYS)))
		saudio_snd_card_free(saudio);
	mutex_unlock(&dev_ctrl->mutex);
	pr_info("%s OUT, result=%d\n", __func__, result);

	return result;
}

static int snd_card_saudio_pcm_close(struct snd_pcm_substream *substream)
{
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	int result = 0;
	ADEBUG();
	mutex_lock(&saudio->mutex);
	if(!saudio->state) {
		mutex_unlock(&saudio->mutex);
		printk("saudio.c: snd_pcm_close error saudio state %d\n",saudio->state);
		return -EIO;
	}
	mutex_unlock(&saudio->mutex);
	dev_ctrl = (struct saudio_dev_ctrl *)&(saudio->dev_ctrl[dev]);
	pr_info("%s IN, stream_id=%d,dst %d, channel %d\n", __func__, stream_id,
		dev_ctrl->dst, dev_ctrl->channel);
	mutex_lock(&dev_ctrl->mutex);
	result = saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					SAUDIO_CMD_CLOSE, stream_id,
					CMD_TIMEOUT);
	if (result) {
		ETRACE
		    ("saudio.c: snd_card_saudio_pcm_close: saudio_send_common_cmd result is %d",
		     result);
		if(result != (-ERESTARTSYS))
			saudio_snd_card_free(saudio);
		mutex_unlock(&dev_ctrl->mutex);
		return result;
	}
	pr_info("%s send cmd done\n", __func__);
	result =
	    saudio_wait_common_cmd(dev_ctrl->dst,
				   dev_ctrl->channel,
				   SAUDIO_CMD_CLOSE_RET, 0, CMD_TIMEOUT);
	if (result && (result != (-ERESTARTSYS)))
		saudio_snd_card_free(saudio);
	mutex_unlock(&dev_ctrl->mutex);
	pr_info("%s OUT, result=%d,dst %d, channel %d\n", __func__, result,
		dev_ctrl->dst, dev_ctrl->channel);

	return result;

}

static int saudio_data_trigger_process(struct saudio_stream *stream,
				       struct saudio_msg *msg);

static int snd_card_saudio_pcm_trigger(struct snd_pcm_substream *substream,
				       int cmd)
{
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	struct saudio_stream *stream = NULL;
	struct saudio_msg msg = { 0 };
	int err = 0;
	int result = 0;
	ADEBUG();
	dev_ctrl = (struct saudio_dev_ctrl *)&(saudio->dev_ctrl[dev]);
	stream = (struct saudio_stream *)&(dev_ctrl->stream[stream_id]);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		pr_info("%s IN, TRIGGER_START, stream_id=%d\n", __func__,
			stream_id);
		msg.stream_id = stream_id;
		stream->stream_state = SAUDIO_TRIGGERED;
		result = saudio_data_trigger_process(stream, &msg);
		result =
		    saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					   SAUDIO_CMD_START, stream->stream_id,
					   0);
		if (result) {
			ETRACE
			    ("saudio.c: snd_card_saudio_pcm_trigger: RESUME, send_common_cmd result is %d",
			     result);
			if(result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
		pr_info("%s OUT, TRIGGER_START, result=%d\n", __func__, result);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		pr_info("%s IN, TRIGGER_STOP, stream_id=%d\n", __func__,
			stream_id);
		stream->stream_state = SAUDIO_STOPPED;
		result =
		    saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					   SAUDIO_CMD_STOP, stream->stream_id,
					   0);
		if (result) {
			ETRACE
			    ("saudio.c: snd_card_saudio_pcm_trigger: SUSPEND, send_common_cmd result is %d",
			     result);
			if(result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
		pr_info("%s OUT, TRIGGER_STOP, result=%d\n", __func__, result);

		break;
	default:
		err = -EINVAL;
		break;
	}

	return 0;
}

static int saudio_cmd_prepare_process(struct saudio_dev_ctrl *dev_ctrl,
				      struct saudio_msg *msg);
static int snd_card_saudio_pcm_prepare(struct snd_pcm_substream *substream)
{
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	struct saudio_msg msg = { 0 };
	int result = 0;

	ADEBUG();
	mutex_lock(&saudio->mutex);
	if(!saudio->state) {
		mutex_unlock(&saudio->mutex);
		printk("saudio.c: snd_pcm_prepare error saudio state %d\n",saudio->state);
		return -EIO;
	}
	mutex_unlock(&saudio->mutex);
	pr_info("%s IN, stream_id=%d\n", __func__, stream_id);
	dev_ctrl = (struct saudio_dev_ctrl *)&(saudio->dev_ctrl[dev]);
	msg.command = SAUDIO_CMD_PREPARE;
	msg.stream_id = stream_id;

	result = saudio_cmd_prepare_process(dev_ctrl, &msg);
	pr_info("%s OUT, result=%d\n", __func__, result);

	return 0;
}

static int snd_card_saudio_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	int32_t result = 0;
	ADEBUG();
	result =
	    saudio_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
	pr_debug("saudio.c: saudio.c: hw_params result is %d", result);
	return result;
}

static int snd_card_saudio_hw_free(struct snd_pcm_substream *substream)
{
	int ret = 0;
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct saudio_stream *stream =
	    (struct saudio_stream *)&(saudio->dev_ctrl[dev].stream[stream_id]);
	ADEBUG();
	mutex_lock(&stream->mutex);
	ret = saudio_pcm_lib_free_pages(substream);
	mutex_unlock(&stream->mutex);
	return ret;
}

static snd_pcm_uframes_t snd_card_saudio_pcm_pointer(struct snd_pcm_substream
						     *substream)
{
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct saudio_stream *stream =
	    (struct saudio_stream *)&(saudio->dev_ctrl[dev].stream[stream_id]);
	unsigned int offset;
	offset =
	    stream->hwptr_done * frames_to_bytes(runtime, runtime->period_size);
	return bytes_to_frames(runtime, offset);
}

static struct snd_pcm_ops snd_card_saudio_playback_ops = {
	.open = snd_card_saudio_pcm_open,
	.close = snd_card_saudio_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_card_saudio_hw_params,
	.hw_free = snd_card_saudio_hw_free,
	.prepare = snd_card_saudio_pcm_prepare,
	.trigger = snd_card_saudio_pcm_trigger,
	.pointer = snd_card_saudio_pcm_pointer,
	.mmap = saudio_snd_mmap,
};

static struct snd_pcm_ops snd_card_saudio_capture_ops = {
	.open = snd_card_saudio_pcm_open,
	.close = snd_card_saudio_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_card_saudio_hw_params,
	.hw_free = snd_card_saudio_hw_free,
	.prepare = snd_card_saudio_pcm_prepare,
	.trigger = snd_card_saudio_pcm_trigger,
	.pointer = snd_card_saudio_pcm_pointer,
	.mmap = saudio_snd_mmap,
};

void saudio_pcm_lib_preallocate_free_for_all(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	int stream;
	pr_debug("saudio.c:saudio_pcm_lib_preallocate_free_for_all");
	for (stream = 0; stream < 2; stream++)
		for (substream = pcm->streams[stream].substream; substream;
		     substream = substream->next) {
			if(substream->dma_buffer.addr) {
			    iounmap(substream->dma_buffer.area);
			    smem_free(substream->dma_buffer.addr,
				      substream->dma_buffer.bytes);
			    substream->dma_buffer.addr = (dma_addr_t) NULL;
			    substream->dma_buffer.area = (int8_t *) NULL;
			    substream->dma_buffer.bytes = 0;
			}
		}
}

int saudio_pcm_lib_preallocate_pages_for_all(struct snd_pcm *pcm,
					     int type, void *data,
					     size_t size, size_t max)
{

	struct snd_pcm_substream *substream;
	int stream;
	pr_debug("saudio.c:saudio_pcm_lib_preallocate_pages_for_all in");
	(void)size;
	for (stream = 0; stream < SAUDIO_STREAM_MAX; stream++) {
		for (substream = pcm->streams[stream].substream; substream;
		     substream = substream->next) {
			struct snd_dma_buffer *dmab = &substream->dma_buffer;

			int addr = smem_alloc(size);

			if(addr) {
			    dmab->dev.type = type;
			    dmab->dev.dev = data;
			    dmab->area = ioremap(addr, size);
			    dmab->addr = addr;
			    dmab->bytes = size;
			    memset(dmab->area, 0x5a, size);
			    pr_debug
				("saudio_pcm_lib_preallocate_pages_for_all:saudio.c: dmab addr is %x, area is %x,size is %d",
				 (uint32_t) dmab->addr, (uint32_t) dmab->area,
				 size);
			    if (substream->dma_buffer.bytes > 0)
				    substream->buffer_bytes_max =
					substream->dma_buffer.bytes;
			    substream->dma_max = max;
			}
			else {
			    printk("saudio: prealloc smem error \n");
			    memset(dmab,0,sizeof(struct snd_dma_buffer));
			    substream->dma_max = 0;
			    substream->buffer_bytes_max = 0;
			    goto ERR;

			}
		}
	}
	return 0;
ERR:
	saudio_pcm_lib_preallocate_free_for_all(pcm);
	return -1;
}

static int  snd_card_saudio_pcm(struct snd_saudio *saudio, int device,
					 int substreams)
{
	struct snd_pcm *pcm;
	int err;
	ADEBUG();
	err = snd_pcm_new(saudio->card, "SAUDIO PCM", device,
			  substreams, substreams, &pcm);
	if (err < 0)
		return err;
	pcm->private_data = saudio;
	saudio->pcm[device] = pcm;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_card_saudio_playback_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,
			&snd_card_saudio_capture_ops);
	pcm->private_data = saudio;
	pcm->info_flags = 0;
	strcpy(pcm->name, "SAUDIO PCM");
	pcm->private_free = saudio_pcm_lib_preallocate_free_for_all;
	err = saudio_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_CONTINUOUS,
						 snd_dma_continuous_data
						 (GFP_KERNEL), MAX_BUFFER_SIZE,
						 MAX_BUFFER_SIZE);
	if(err) {
	    printk("saudio:snd_card_saudio_pcm: prealloc pages error\n");
	    saudio->pcm[device] = NULL;
	}
	return err;
}

static struct snd_saudio *saudio_card_probe(struct saudio_init_data *init_data)
{
	struct snd_saudio *saudio = NULL;

	saudio = kzalloc(sizeof(struct snd_saudio), GFP_KERNEL);
	if (!saudio)
		return NULL;

	saudio->dev_ctrl[0].channel = init_data->ctrl_channel;
	saudio->dev_ctrl[0].monitor_channel = init_data->monitor_channel;
	saudio->dev_ctrl[0].dst = init_data->dst;
	saudio->dst = init_data->dst;
	saudio->channel = init_data->ctrl_channel;
	mutex_init(&saudio->mutex);
	strncpy(saudio->dev_ctrl[0].name, init_data->name,
	       SAUDIO_CARD_NAME_LEN_MAX);
	saudio->dev_ctrl[0].name[SAUDIO_CARD_NAME_LEN_MAX-1] = '\0';
	saudio->dev_ctrl[0].stream[SNDRV_PCM_STREAM_PLAYBACK].channel =
	    init_data->playback_channel;
	saudio->dev_ctrl[0].stream[SNDRV_PCM_STREAM_PLAYBACK].dst =
	    init_data->dst;
	saudio->dev_ctrl[0].stream[SNDRV_PCM_STREAM_CAPTURE].channel =
	    init_data->capture_channel;
	saudio->dev_ctrl[0].stream[SNDRV_PCM_STREAM_CAPTURE].dst =
	    init_data->dst;

	return saudio;
}

static int saudio_data_trigger_process(struct saudio_stream *stream,
				       struct saudio_msg *msg)
{
	int32_t result = 0;
	struct sblock blk = { 0 };
	struct cmd_common *common = NULL;
	struct snd_pcm_runtime *runtime = stream->substream->runtime;
	ADEBUG();

	if (stream->stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
		stream->periods_avail = snd_pcm_playback_avail(runtime) /
		    runtime->period_size;
	} else {
		stream->periods_avail = snd_pcm_capture_avail(runtime) /
		    runtime->period_size;
	}

	pr_debug("saudio.c:stream->periods_avail is %d,block count is %d",
		 stream->periods_avail, sblock_get_free_count(stream->dst,
							      stream->channel));

	stream->periods_tosend = runtime->periods - stream->periods_avail;

	ADEBUG();

	while (stream->periods_tosend) {

		result = sblock_get(stream->dst, stream->channel, &blk, 0);
		if (result) {
			break;
		}
		stream->last_getblk_count++;
		common = (struct cmd_common *)blk.addr;
		blk.length = frames_to_bytes(runtime, runtime->period_size);
		common->command = SAUDIO_DATA_PCM;
		common->sub_cmd = stream->stream_id;
		common->reserved1 =
		    stream->substream->dma_buffer.addr +
		    stream->period * blk.length;

		sblock_send(stream->dst, stream->channel, &blk);

		stream->period++;
		stream->period = stream->period % runtime->periods;
		stream->periods_tosend--;
	}

	pr_debug(":sblock_getblock_count trigger is %d \n",
		 stream->last_getblk_count);

	return result;
}

static int saudio_data_transfer_process(struct saudio_stream *stream,
					struct saudio_msg *msg)
{
	struct snd_pcm_runtime *runtime = stream->substream->runtime;
	struct sblock blk = { 0 };
	int32_t result = 0;
	struct cmd_common *common = NULL;

	int32_t elapsed_blks = 0;
	int32_t periods_avail;
	int32_t periods_tosend;
	int32_t cur_blk_count = 0;

	cur_blk_count = sblock_get_free_count(stream->dst, stream->channel);

	elapsed_blks =
	    (cur_blk_count + stream->last_getblk_count - stream->blk_count) -
	    stream->last_elapsed_count;

	if (stream->stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
		periods_avail = snd_pcm_playback_avail(runtime) /
		    runtime->period_size;
	} else {
		periods_avail = snd_pcm_capture_avail(runtime) /
		    runtime->period_size;
	}

	periods_tosend = stream->periods_avail - periods_avail;
	if (periods_tosend > 0) {
		stream->periods_tosend += periods_tosend;
	}

	if (stream->periods_tosend) {
		while (stream->periods_tosend) {
			result =
			    sblock_get(stream->dst, stream->channel, &blk, 0);
			if (result) {
				break;
			}
			stream->last_getblk_count++;
			common = (struct cmd_common *)blk.addr;
			blk.length =
			    frames_to_bytes(runtime, runtime->period_size);
			common->command = SAUDIO_DATA_PCM;
			common->sub_cmd = stream->stream_id;
			common->reserved1 =
			    stream->substream->dma_buffer.addr +
			    stream->period * blk.length;

			sblock_send(stream->dst, stream->channel, &blk);

			stream->periods_tosend--;
			stream->period++;
			stream->period = stream->period % runtime->periods;
		}

	} else {
		pr_debug("saudio.c: saudio no data to send ");
		if (sblock_get_free_count(stream->dst, stream->channel) ==
		    SAUDIO_STREAM_BLOCK_COUNT) {
			pr_debug
			    ("saudio.c: saudio no data to send and  is empty ");
			result =
			    sblock_get(stream->dst, stream->channel, &blk, 0);
			if (result) {
				ETRACE("saudio.c: no data and no blk\n");
			} else {
				stream->last_getblk_count++;
				common = (struct cmd_common *)blk.addr;
				common->command = SAUDIO_DATA_SILENCE;
				common->sub_cmd = stream->stream_id;

				sblock_send(stream->dst, stream->channel, &blk);
				stream->last_elapsed_count++;
				schedule_timeout_interruptible(msecs_to_jiffies(5));
			}
		}
	}

	while (elapsed_blks > 0) {
		elapsed_blks--;
		stream->hwptr_done++;
		stream->hwptr_done %= runtime->periods;
		snd_pcm_period_elapsed(stream->substream);
		stream->periods_avail++;
		stream->last_elapsed_count++;
	}

	return 0;
}

static int saudio_cmd_prepare_process(struct saudio_dev_ctrl *dev_ctrl,
				      struct saudio_msg *msg)
{
	struct sblock blk;
	struct snd_saudio *saudio = NULL;
	int32_t result = 0;
	struct snd_pcm_runtime *runtime =
	    dev_ctrl->stream[msg->stream_id].substream->runtime;
	ADEBUG();
	saudio = dev_ctrl->stream[msg->stream_id].saudio;
	result =
	    sblock_get(dev_ctrl->dst, dev_ctrl->channel, (struct sblock *)&blk,
		       CMD_TIMEOUT);
	if (!result) {
		struct cmd_prepare *prepare = (struct cmd_prepare *)blk.addr;
		prepare->common.command = SAUDIO_CMD_PREPARE;
		prepare->common.sub_cmd = msg->stream_id;
		prepare->rate = runtime->rate;
		prepare->channels = runtime->channels;
		prepare->format = runtime->format;
		prepare->period =
		    frames_to_bytes(runtime, runtime->period_size);
		prepare->periods = runtime->periods;
		blk.length = sizeof(struct cmd_prepare);

		mutex_lock(&dev_ctrl->mutex);

		sblock_send(dev_ctrl->dst, dev_ctrl->channel,
			    (struct sblock *)&blk);
		result =
		    saudio_wait_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					   SAUDIO_CMD_PREPARE_RET,
					   0, CMD_TIMEOUT);
		if (result && (result != (-ERESTARTSYS)))
			saudio_snd_card_free(saudio);

		mutex_unlock(&dev_ctrl->mutex);
	}
	pr_debug("saudio_cmd_prepare_process result is %d", result);
	return result;

}

static void sblock_notifier(int event, void *data)
{
	struct saudio_stream *stream = data;
	struct saudio_msg msg = { 0 };
	int result = 0;

	if (event == SBLOCK_NOTIFY_GET) {
		if (stream->stream_state == SAUDIO_TRIGGERED) {
			mutex_lock(&stream->mutex);
			result = saudio_data_transfer_process(stream, &msg);
			mutex_unlock(&stream->mutex);
		} else {
			pr_debug("\n: saudio is stopped\n");
		}
	}
}

static int saudio_snd_card_free(const struct snd_saudio *saudio)
{
	int result = 0;
	printk(KERN_INFO "saudio:saudio_snd_card free in dst %d,channel %d\n",
	       saudio->dst, saudio->channel);
	queue_work(saudio->queue, (struct work_struct *)&saudio->card_free_work);
	printk(KERN_INFO
	       "saudio:saudio_snd_card free out %d ,dst %d, channel %d\n",
	       result, saudio->dst, saudio->channel);
	return 0;
}

static void saudio_snd_wait_modem_restart(struct snd_saudio *saudio)
{
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	int result = 0;
	dev_ctrl = &saudio->dev_ctrl[0];
	while (1) {
		result =
		    saudio_wait_common_cmd(dev_ctrl->dst,
					   dev_ctrl->monitor_channel,
					   SAUDIO_CMD_HANDSHAKE, 0, -1);
		if (result) {
			schedule_timeout_interruptible(msecs_to_jiffies(1000));
			printk(KERN_ERR "saudio_wait_monitor_cmd error %d\n",
			       result);
			continue;
		} else {
			while (1) {
				result =
				    saudio_send_common_cmd(dev_ctrl->dst,
							   dev_ctrl->
							   monitor_channel,
							   SAUDIO_CMD_HANDSHAKE_RET,
							   0, -1);
				if (!result) {
					break;
				}
				printk(KERN_ERR
				       "saudio_send_monitor_cmd error %d\n",
				       result);
				schedule_timeout_interruptible(msecs_to_jiffies
							       (1000));
			}
		}
		break;
	}
}

static int saudio_snd_notify_modem_clear(struct snd_saudio *saudio)
{
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	int result = 0;
	dev_ctrl = &saudio->dev_ctrl[0];
	printk(KERN_INFO "saudio.c:saudio_snd_notify_mdem_clear in");
	result =
	    saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->monitor_channel,
				   SAUDIO_CMD_RESET, 0, -1);
	if (result) {
		printk(KERN_ERR "saudio_send_modem_reset_cmd error %d\n",
		       result);
	}
	else {
	    result =
		saudio_wait_common_cmd(dev_ctrl->dst, dev_ctrl->monitor_channel,
				       SAUDIO_CMD_RESET_RET, 0, CMD_MODEM_RESET_TIMEOUT);
	    if (result) {
		    printk(KERN_ERR "saudio_wait_monitor_cmd error %d\n", result);
	    }
	}
	printk(KERN_INFO "saudio.c:saudio_snd_notify_mdem_clear out");
	return result;
}

static int saudio_snd_init_ipc(struct snd_saudio *saudio)
{
	int result = 0;
	int32_t i = 0, j = 0;
	struct saudio_stream *stream = NULL;
	struct saudio_dev_ctrl *dev_ctrl = NULL;

	ADEBUG();

	for (i = 0; i < SAUDIO_DEV_MAX; i++) {	/* now only support  one device */
		dev_ctrl = &saudio->dev_ctrl[i];
		result =
		    sblock_create(dev_ctrl->dst, dev_ctrl->monitor_channel,
				  SAUDIO_MONITOR_BLOCK_COUNT, CMD_BLOCK_SIZE,
				  SAUDIO_MONITOR_BLOCK_COUNT, CMD_BLOCK_SIZE);
		if (result) {
			ETRACE
			    ("saudio:monitor channel create  failed result is %d\n",
			     result);
			goto __nodev;
		}
		result =
		    sblock_create(dev_ctrl->dst, dev_ctrl->channel,
				  SAUDIO_CMD_BLOCK_COUNT, CMD_BLOCK_SIZE,
				  SAUDIO_CMD_BLOCK_COUNT, CMD_BLOCK_SIZE);
		if (result) {
			ETRACE
			    ("saudio_thread sblock create  failed result is %d\n",
			     result);
			goto __nodev;
		}
		pr_debug("saudio_thread sblock create  result is %d\n", result);

		for (j = 0; j < SAUDIO_STREAM_MAX; j++) {
			stream = &dev_ctrl->stream[j];
			result =
			    sblock_create(stream->dst, stream->channel,
					  SAUDIO_STREAM_BLOCK_COUNT,
					  TX_DATA_BLOCK_SIZE,
					  SAUDIO_STREAM_BLOCK_COUNT,
					  RX_DATA_BLOCK_SIZE);
			if (result) {
				ETRACE
				    ("saudio_thread sblock create  failed result is %d\n",
				     result);
				goto __nodev;
			}
			sblock_register_notifier(stream->dst, stream->channel,
						 sblock_notifier, stream);
			pr_debug("saudio_thread sblock create  result is %d\n",
				 result);
		}
	}
	ADEBUG();
	return result;
__nodev:
	ETRACE("initialization failed\n");
	return result;
}

static int saudio_snd_init_card(struct snd_saudio *saudio)
{
	int result = 0;
	int32_t i = 0, j = 0, err = 0;
	struct saudio_stream *stream = NULL;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	struct snd_card *saudio_card = NULL;

	ADEBUG();

	if (!saudio) {
		return -1;
	}

	mutex_lock(&snd_sound);

	result =
	    snd_card_create(SNDRV_DEFAULT_IDX1, saudio->dev_ctrl[0].name,
			    THIS_MODULE, sizeof(struct snd_saudio *),
			    &saudio_card);
	if (!saudio_card) {
		printk(KERN_ERR "saudio:snd_card_create faild result is %d\n",
		       result);
		mutex_unlock(&snd_sound);
		return -1;
	}
	saudio->card = saudio_card;
	saudio_card->private_data = saudio;

	for (i = 0; i < SAUDIO_DEV_MAX; i++) {	/* now only support  one device */
		dev_ctrl = &saudio->dev_ctrl[i];
		mutex_init(&dev_ctrl->mutex);
		err = snd_card_saudio_pcm(saudio, i, 1);
		if (err < 0) {
			mutex_unlock(&snd_sound);
			goto __nodev;
		}
		for (j = 0; j < SAUDIO_STREAM_MAX; j++) {
			stream = &dev_ctrl->stream[j];
			stream->dev_ctrl = dev_ctrl;

			stream->stream_state = SAUDIO_IDLE;
			stream->stream_id = j;
			stream->saudio = saudio;
			mutex_init(&stream->mutex);
		}
	}
	ADEBUG();

	memcpy(saudio->card->driver, dev_ctrl->name, SAUDIO_CARD_NAME_LEN_MAX);
	memcpy(saudio->card->shortname, dev_ctrl->name,
	       SAUDIO_CARD_NAME_LEN_MAX);
	memcpy(saudio->card->longname, dev_ctrl->name,
	       SAUDIO_CARD_NAME_LEN_MAX);

	err = snd_card_register(saudio->card);

	mutex_unlock(&snd_sound);

	if (err == 0) {
		printk(KERN_INFO "saudio.c:snd_card create ok\n");
		return 0;
	}
__nodev:
	if (saudio) {
		if (saudio->card) {
			mutex_lock(&snd_sound);
			snd_card_free(saudio->card);
			saudio->card = NULL;
			mutex_unlock(&snd_sound);
		}
	}
	printk("saudio.c:initialization failed\n");
	return err;
}

static int saudio_ctrl_thread(void *data)
{
	int result = 0;
	struct snd_saudio *saudio = (struct snd_saudio *)data;
	ADEBUG();

	result = saudio_snd_init_ipc(saudio);
	if (result) {
		printk(KERN_ERR "saudio:saudio_snd_init_ipc error %d\n",
		       result);
		return -1;
	}
	while (!kthread_should_stop()) {
		printk(KERN_INFO
		       "%s,saudio: waiting for modem boot handshake,dst %d,channel %d\n",
		       __func__, saudio->dst, saudio->channel);
		saudio_snd_wait_modem_restart(saudio);

		saudio->in_init = 1;

		printk(KERN_INFO
		       "%s,saudio: modem boot and handshake ok,dst %d, channel %d\n",
		       __func__, saudio->dst, saudio->channel);
		saudio_snd_card_free(saudio);
		printk(KERN_INFO
		       "saudio_ctrl_thread flush work queue in dst %d, channel %d\n",
		       saudio->dst, saudio->channel);

		flush_workqueue(saudio->queue);

		saudio->in_init = 0;

		printk(KERN_INFO
		       "saudio_ctrl_thread flush work queue out,dst %d, channel %d\n",
		       saudio->dst, saudio->channel);

		if(saudio_snd_notify_modem_clear(saudio)) {
			printk(KERN_ERR " saudio_ctrl_thread modem error again when notify modem clear \n");
		    continue;
		}

		saudio_clear_ctrl_cmd(saudio);

		if(!saudio->card) {
			result = saudio_snd_init_card(saudio);
			printk(KERN_INFO
			       "saudio: snd card init reulst %d, dst %d, channel %d\n",
			       result, saudio->dst, saudio->channel);
		}
		mutex_lock(&saudio->mutex);
		saudio->state = 1;
		mutex_unlock(&saudio->mutex);
	}
	ETRACE("saudio_ctrl_thread  create  ok\n");

	return 0;
}

static void saudio_work_card_free_handler(struct work_struct *data)
{
	struct snd_saudio *saudio =
	    container_of(data, struct snd_saudio, card_free_work);
	printk(KERN_INFO "saudio: card free handler in\n");

	if (saudio->card) {
		printk(KERN_INFO
		       "saudio: work_handler:snd card free in,dst %d, channel %d\n",
		       saudio->dst, saudio->channel);
		mutex_lock(&saudio->mutex);
		saudio->state = 0;
		mutex_unlock(&saudio->mutex);
		if (!saudio->in_init)
			saudio_send_common_cmd(saudio->dst, saudio->channel, 0,
					       SAUDIO_CMD_HANDSHAKE, -1);
		printk(KERN_INFO
		       "saudio: work_handler:snd card free dst %d, channel %d\n",
		       saudio->dst, saudio->channel);
	}
	printk(KERN_INFO "saudio: card free handler out\n");
}

static int snd_saudio_probe(struct platform_device *devptr)
{
	struct snd_saudio *saudio = NULL;
	static pid_t thread_id = (pid_t) - 1;
#ifdef CONFIG_OF
	int ret, id;
	const char *name = NULL;
	struct saudio_init_data snd_init_data = {0};
	const char *saudio_names = "sprd,saudio-names";
	const char *saudio_dst_id = "sprd,saudio-dst-id";
	struct saudio_init_data *init_data = &snd_init_data;
#else
	struct saudio_init_data *init_data = devptr->dev.platform_data;
#endif

	ADEBUG();

#ifdef CONFIG_OF
	ret = of_property_read_u32(devptr->dev.of_node, saudio_dst_id, &id);
	if (ret) {
		printk("saudio: %s: missing %s in dt node\n", __func__, saudio_dst_id);
		return ret;
	} else {
		if ((id == SIPC_ID_CPT) || (id == SIPC_ID_CPW)) {
			printk("saudio: %s: %s is %d\n", __func__, __func__, saudio_dst_id, id);
		} else {
			printk("saudio: %s: %s is %d and only support 1,2\n", __func__, saudio_dst_id, id);
			return ret;
		}
	}
	ret = of_property_read_string(devptr->dev.of_node, saudio_names, &name);
	if (ret) {
		printk("saudio: %s: missing %s in dt node\n", __func__, saudio_names);
		return ret;
	} else {
		printk("saudio: %s: %s is %s\n", __func__, __func__, saudio_names, name);
	}

	if (!strcmp(name, "saudio_voip")) {
		init_data->name = "saudiovoip";
		init_data->dst = id;
		init_data->ctrl_channel = SMSG_CH_CTRL_VOIP;
		init_data->playback_channel = SMSG_CH_PLAYBACK_VOIP;
		init_data->capture_channel = SMSG_CH_CAPTURE_VOIP;
		init_data->monitor_channel = SMSG_CH_MONITOR_VOIP;
	} else if(id == SIPC_ID_CPT) {
		init_data->name = "VIRTUAL AUDIO";
		init_data->dst = id;
		init_data->ctrl_channel = SMSG_CH_VBC;
		init_data->playback_channel = SMSG_CH_PLAYBACK;
		init_data->capture_channel = SMSG_CH_CAPTURE;
		init_data->monitor_channel = SMSG_CH_MONITOR_AUDIO;
	} else if(id == SIPC_ID_CPW) {
		init_data->name = "VIRTUAL AUDIO W";
		init_data->dst = id;
		init_data->ctrl_channel = SMSG_CH_VBC;
		init_data->playback_channel = SMSG_CH_PLAYBACK;
		init_data->capture_channel = SMSG_CH_CAPTURE;
		init_data->monitor_channel = SMSG_CH_MONITOR_AUDIO;
	} else {
		printk("saudio: %s: get data in dt node failed\n", __func__);
	}
#endif

	if (!(saudio = saudio_card_probe(init_data))) {
		return -1;
	}
	saudio->pdev = devptr;

	saudio->queue = create_singlethread_workqueue("saudio");
	if (!saudio->queue) {
		printk("saudio:workqueue create error %d\n", (int)saudio->queue);
		if (saudio) {
			kfree(saudio);
			saudio = NULL;
		}
		return -1;
	}
	printk("saudio:workqueue create ok");
	INIT_WORK(&saudio->card_free_work, saudio_work_card_free_handler);

	platform_set_drvdata(devptr, saudio);

	saudio->thread_id = kthread_create(saudio_ctrl_thread, saudio, "saudio-%d-%d",saudio->dst,saudio->channel);
	if (IS_ERR(saudio->thread_id)) {
		ETRACE("virtual audio cmd kernel thread creation failure \n");
		destroy_workqueue(saudio->queue);
		saudio->queue = NULL;
		saudio->thread_id = NULL;
		kfree(saudio);
		platform_set_drvdata(devptr, NULL);
		return -1;
	}
	wake_up_process(saudio->thread_id);
	return 0;
}

static int snd_saudio_remove(struct platform_device *devptr)
{
	struct snd_saudio *saudio = platform_get_drvdata(devptr);

	if (saudio) {
		if (!IS_ERR_OR_NULL(saudio->thread_id)) {
		    kthread_stop(saudio->thread_id);
		    saudio->thread_id = NULL;
		}
		if (saudio->queue)
			destroy_workqueue(saudio->queue);
		if (saudio->pdev) {
			platform_device_unregister(saudio->pdev);
		}
		if (saudio->card) {
			mutex_lock(&snd_sound);
			snd_card_free(saudio->card);
			saudio->card = NULL;
			mutex_unlock(&snd_sound);
		}
		kfree(saudio);
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id saudio_dt_match[] = {
	{.compatible = "sprd,saudio"},
	{},
};
#endif

static struct platform_driver snd_saudio_driver = {
	.probe = snd_saudio_probe,
	.remove = snd_saudio_remove,
	.driver = {
		.name = "saudio",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = saudio_dt_match,
#endif
	},
};

static int __init alsa_card_saudio_init(void)
{
	int err;
	err = platform_driver_register(&snd_saudio_driver);
	if (err < 0)
		printk("saudio: platform_driver_register err %d\n", err);
	return 0;
}

static void __exit alsa_card_saudio_exit(void)
{
	platform_driver_unregister(&snd_saudio_driver);
	return;
}

module_init(alsa_card_saudio_init)
module_exit(alsa_card_saudio_exit)
