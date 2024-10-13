/*
 * sound/soc/sprd/dai/sprd-dmaengine-pcm.c
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
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt(" PCM ") fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/of.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/vbc-utils.h>

#include "sprd-asoc-common.h"
#include "sprd4whale-dmaengine-pcm.h"
#include "vaudio.h"
#include "dfm.h"

#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/genalloc.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/sched.h>

#include <../../../drivers/mcdt/mcdt_hw.h>


struct smem_pool {
	struct list_head 	smem_head;
	spinlock_t 		lock;

	uint32_t		addr;
	uint32_t		size;
	atomic_t 		used;

	struct gen_pool		*gen;
};

struct smem_record {
	struct list_head smem_list;
	struct task_struct *task;
	uint32_t size;
	uint32_t addr;
};

static struct smem_pool audio_mem_pool;

static int audio_smem_init(uint32_t addr, uint32_t size)
{
	struct smem_pool *spool = &audio_mem_pool;

	spool->addr = addr;
	spool->size = PAGE_ALIGN(size);
	atomic_set(&spool->used, 0);
	spin_lock_init(&spool->lock);
	INIT_LIST_HEAD(&spool->smem_head);

	/* allocator block size is times of pages */
	spool->gen = gen_pool_create(PAGE_SHIFT, -1);
	if (!spool->gen) {
		pr_err("Failed to create smem gen pool!\n");
		return -1;
	}

	if (gen_pool_add(spool->gen, spool->addr, spool->size, -1) != 0) {
		pr_err("Failed to add smem gen pool!\n");
		return -1;
	}

	return 0;
}

/* ****************************************************************** */

uint32_t audio_smem_alloc(uint32_t size)
{
	struct smem_pool *spool = &audio_mem_pool;
	struct smem_record *recd;
	unsigned long flags;
	uint32_t addr;

	recd = kzalloc(sizeof(struct smem_record), GFP_KERNEL);
	if (!recd) {
		pr_err("failed to alloc smem record\n");
		addr = 0;
		goto error;
	}

	size = PAGE_ALIGN(size);
	addr = gen_pool_alloc(spool->gen, size);
	if (!addr) {
		pr_err("failed to alloc smem from gen pool\n");
		kfree(recd);
		goto error;
	}

	/* record smem alloc info */
	atomic_add(size, &spool->used);
	recd->size = size;
	recd->task = current;
	recd->addr = addr;
	spin_lock_irqsave(&spool->lock, flags);
	list_add_tail(&recd->smem_list, &spool->smem_head);
	spin_unlock_irqrestore(&spool->lock, flags);

error:
	return addr;
}

void audio_smem_free(uint32_t addr, uint32_t size)
{
	struct smem_pool *spool = &audio_mem_pool;
	struct smem_record *recd, *next;
	unsigned long flags;

	size = PAGE_ALIGN(size);
	atomic_sub(size, &spool->used);
	gen_pool_free(spool->gen, addr, size);
	/* delete record node from list */
	spin_lock_irqsave(&spool->lock, flags);
	list_for_each_entry_safe(recd, next, &spool->smem_head, smem_list) {
		if (recd->addr == addr) {
			list_del(&recd->smem_list);
			kfree(recd);
			break;
		}
	}
	spin_unlock_irqrestore(&spool->lock, flags);
}


#define CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32

#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)

#define SPRD_DDR32_DMA_DATA_SIZE  (32 * 1024)
#define SPRD_DDR32_DMA_CFG_SIZE (2 * 1024)
#endif
#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
#define SPRD_AUDIO_DMA_NODE_SIZE (1024)
#endif
#ifndef  DMA_LINKLIST_CFG_NODE_SIZE
#define DMA_LINKLIST_CFG_NODE_SIZE  (sizeof(struct sprd_dma_cfg))
#endif

struct sprd_dma_callback_data {
	struct snd_pcm_substream *substream;
	struct dma_chan *dma_chn;
};

struct sprd_runtime_data {
	int dma_addr_offset;
	struct sprd_pcm_dma_params *params;
	struct dma_chan *dma_chn[4];
	struct sprd_dma_cfg *dma_cfg_array;
	struct sprd_dma_callback_data *dma_pdata;
	int dma_stage;
	dma_addr_t dma_cfg_phy[4];
	void *dma_cfg_virt[4];
	struct dma_async_tx_descriptor *dma_tx_des[4];
	dma_cookie_t cookie[4];
	int int_pos_update[4];
	int burst_len;
	int hw_chan;
	int dma_pos_pre[4];
	int interleaved;
	int cb_called;
#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
	int buffer_in_ddr32;
#endif

#ifdef CONFIG_SND_VERBOSE_PROCFS
	struct snd_info_entry *proc_info_entry;
#endif
};

static const struct snd_pcm_hardware sprd_pcm_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
	    SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_NONINTERLEAVED |
	    SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME |
	    SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	/* 16bits, stereo-2-channels */
	.period_bytes_min = VBC_FIFO_FRAME_NUM * 4,/*2048*/
	/* non limit */
	.period_bytes_max = VBC_FIFO_FRAME_NUM * 4 * 100,
	.periods_min = 1,
	/* non limit */
	#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
	.periods_max = SPRD_AUDIO_DMA_NODE_SIZE / DMA_LINKLIST_CFG_NODE_SIZE, // 1k/sizeof(struct sprd_dma_cfg)=1024/92=11
	#else
	.periods_max = PAGE_SIZE / DMA_LINKLIST_CFG_NODE_SIZE, // 4k/sizeof(struct sprd_dma_cfg)
	#endif
	#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
	.buffer_bytes_max = SPRD_DDR32_DMA_DATA_SIZE,
	#else
	.buffer_bytes_max = VBC_BUFFER_BYTES_MAX,
	#endif
};

atomic_t lightsleep_refcnt;

int sprd_lightsleep_disable(const char *id, int disalbe)
    __attribute__ ((weak, alias("__sprd_lightsleep_disable")));

static int __sprd_lightsleep_disable(const char *id, int disable)
{
	sp_asoc_pr_dbg("NO lightsleep control function %d\n", disable);
	return 0;
}

static inline const char *sprd_dai_pcm_name(struct snd_soc_dai *cpu_dai)
{
	return "VBC pcm";
}

static inline int sprd_pcm_is_interleaved(struct snd_pcm_runtime *runtime)
{
	return (runtime->access == SNDRV_PCM_ACCESS_RW_INTERLEAVED ||
		runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED);
}

#define PCM_DIR_NAME(stream) (stream == SNDRV_PCM_STREAM_PLAYBACK ? "Playback" : "Captrue")
int mcdt_buf[MCDT_FULL_WMK_LOOP] = {0};
static void sprd_pcm_dma_buf_done(void *data)
{
	struct sprd_dma_callback_data *dma_cb_data = (struct sprd_dma_callback_data*)data;
	struct snd_pcm_runtime *runtime = dma_cb_data->substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = dma_cb_data->substream->private_data;
	unsigned int * temp = (unsigned int*)runtime->dma_area;
	int i = 0;
	int t = 0;
	int j = 0;
	int ret = 0;
	if (!rtd->cb_called) {
		#if 1
		rtd->cb_called = 1;
		#else
		rtd->cb_called = 0;
		#endif
		sp_asoc_pr_info("DMA Callback CALL cpu_dai->id =%d\n", srtd->cpu_dai->id);
		#if 0
		// test for mcdt
		while (j < 20) {
			ret = mcdt_read(MCDT_CHAN_LOOP, mcdt_buf, sizeof(mcdt_buf));
			j++;
		}
		for(t = 0; t <  MCDT_FULL_WMK_LOOP; t++){
			printk(KERN_EMERG "%x\n", mcdt_buf[t]);
		}
		printk(KERN_EMERG "===========================ret = %d\n", ret);
		for(t = 0; t < MCDT_FULL_WMK_LOOP ;t++) {
			printk(KERN_EMERG "%x\n",temp[t]);
		}
		#endif
	}
	if (rtd->hw_chan == 1)
		goto irq_fast;

	for (i = 0; i < 2; i++) {
		if (dma_cb_data->dma_chn == rtd->dma_chn[i]) {
			rtd->int_pos_update[i] = 1;

			if (rtd->dma_chn[1 - i]) {
				if (rtd->int_pos_update[1 - i])
					goto irq_ready;
			} else {
				goto irq_ready;
			}
		}
	}
	goto irq_ret;
irq_ready:
	rtd->int_pos_update[0] = 0;
	rtd->int_pos_update[1] = 0;
irq_fast:
	snd_pcm_period_elapsed(dma_cb_data->substream);
irq_ret:
	return;
}

int sprd_pcm_config_sprd_rtd(struct sprd_runtime_data *rtd,
						struct snd_pcm_substream *substream,
						int used_dma_stage, int chan_count)
{
	int ret = 0;
	int i, count;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;

	if (!rtd) {
		pr_err("%s, rtd hasn't been allocated yet \n", __func__);
		ret = -1;
		goto out;
	}
	if (used_dma_stage > 2) {
		pr_err("%s, (%d)over the max dma stages \n",
				__func__, used_dma_stage);
		ret = -1;
		goto out;
	}
	if (chan_count > 2) {
		pr_err("%s, (%d)over the max dma stages \n",
				__func__, chan_count);
		ret = -1;
		goto out;
	}
	sp_asoc_pr_dbg("%s, used_dma_stage:%d, chan_count:%d \n",
			__func__, used_dma_stage, chan_count);

#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
	{
		rtd->buffer_in_ddr32= 1;
		rtd->dma_cfg_phy[0] =  audio_smem_alloc(SPRD_DDR32_DMA_CFG_SIZE);
		rtd->dma_cfg_virt[0] = ioremap_nocache(rtd->dma_cfg_phy[0], SPRD_DDR32_DMA_CFG_SIZE);
		rtd->dma_cfg_virt[1] = (((unsigned long)rtd->dma_cfg_virt[0] + runtime->hw.periods_max*(sizeof(struct sprd_dma_cfg))) + 15) & (~0xf);
		rtd->dma_cfg_phy[1] = (((unsigned long)rtd->dma_cfg_phy[0] + runtime->hw.periods_max*(sizeof(struct sprd_dma_cfg)))+ 15) & (~0xf);
		sp_asoc_pr_dbg("rtd->dma_cfg_virt[0] =%#lx, rtd->dma_cfg_phy[0] =%#lx, rtd->dma_cfg_virt[1]=%#lx,rtd->dma_cfg_phy[1]=%#lx, runtime->hw.periods_max*(sizeof(struct sprd_dma_cfg)=%d\n ",
			rtd->dma_cfg_virt[0], rtd->dma_cfg_phy[0], rtd->dma_cfg_virt[1], rtd->dma_cfg_phy[1], runtime->hw.periods_max*(sizeof(struct sprd_dma_cfg)));
	}
if (atomic_inc_return(&lightsleep_refcnt) == 1)
	sprd_lightsleep_disable("audio", 1);
#else
		for (i = 0; i < (used_dma_stage*2); i++) {
			rtd->dma_cfg_virt[i] =
					(void *)dma_alloc_coherent(substream->pcm->card->dev, runtime->hw.periods_max * sizeof(struct sprd_dma_cfg),
								&rtd->dma_cfg_phy[i], GFP_KERNEL);//runtime->hw.periods_max=page_size(4k)/sizeof(struct sprd_dma_cfg)
			if (!rtd->dma_cfg_virt[i]) {
				goto err0;
			}
			sp_asoc_pr_dbg("%s, dma_cfg_virt[%d]:0x%lx, dma_cfg_phy:0x%lx \n", __func__, i, rtd->dma_cfg_virt[i], rtd->dma_cfg_phy[i]);
			count = i+1;
			if (atomic_inc_return(&lightsleep_refcnt) == 1)
				sprd_lightsleep_disable("audio", 1);
		}
#endif
		sp_asoc_pr_dbg("runtime->hw.periods_max=%d\n", runtime->hw.periods_max);
		rtd->dma_cfg_array =
			(struct sprd_dma_cfg *)kzalloc(used_dma_stage * rtd->hw_chan * runtime->hw.periods_max * sizeof(struct sprd_dma_cfg),GFP_KERNEL);
		if (!rtd->dma_cfg_array)
			goto err1;

		rtd->dma_pdata =
			(struct sprd_dma_callback_data *)kzalloc(used_dma_stage* rtd->hw_chan * sizeof(struct sprd_dma_callback_data),GFP_KERNEL);
		if (!rtd->dma_pdata)
			goto err2;
		rtd->dma_stage = used_dma_stage;
		rtd->hw_chan = chan_count;
		rtd->dma_chn[0] = rtd->dma_chn[1] = rtd->dma_chn[2] = rtd->dma_chn[3] = NULL;
		rtd->dma_tx_des[0] = rtd->dma_tx_des[1] = rtd->dma_tx_des[2] = rtd->dma_tx_des[3] = NULL;
		rtd->cookie[0] = rtd->cookie[1] = rtd->cookie[2] = rtd->cookie[3] = 0;
		goto out;
err2:
	pr_err("ERR:dma_pdata alloc failed!\n");
	kfree(rtd->dma_cfg_array);
err1:
	pr_err("ERR:dma_cfg_array alloc failed!\n");
err0:
	pr_err("ERR:dma_cfg_virt alloc failed, count:%d \n", count);

#if (!defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32))
	if (count) {
		for (;count > 0; count--)
			dma_free_coherent(substream->pcm->card->dev, runtime->hw.periods_max * sizeof(struct sprd_dma_cfg),
				rtd->dma_cfg_virt[count-1], rtd->dma_cfg_phy[count-1]);
	}
#endif
	if (!atomic_dec_return(&lightsleep_refcnt))
		sprd_lightsleep_disable("audio", 0);
	ret = -ENOMEM;
out:
	return ret;
}

static int sprd_pcm_config_dma_channel(struct dma_chan *dma_chn, struct sprd_dma_cfg *dma_config,
		struct dma_async_tx_descriptor **dma_tx_des, struct sprd_dma_callback_data *dma_pdata, unsigned int flag)
{
	int ret;
	/* config dma channel */
	ret = dmaengine_device_control(dma_chn,
							DMA_SLAVE_CONFIG,
							(unsigned long)(dma_config));
	if(ret < 0){
		sp_asoc_pr_dbg("%s, DMA chan ID %d config is failed!\n", __func__, dma_chn->chan_id);
		return ret;
	}
	/* get dma desc from dma config */
	*dma_tx_des =
			dma_chn->device->device_prep_dma_memcpy(dma_chn, 0, 0, 0,
					DMA_CFG_FLAG|/*DMA_SOFTWARE_FLAG*/DMA_HARDWARE_FLAG);
	if(!(*dma_tx_des)) {
		sp_asoc_pr_dbg("%s, DMA chan ID %d memcpy is failed!\n", __func__, dma_chn->chan_id);
		ret = -1;
		return ret;
	}

	if (!(flag & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP)) {
		sp_asoc_pr_info("%s, Register Callback func for DMA chan ID %d\n",
				__func__,dma_chn->chan_id);
		(*dma_tx_des)->callback = sprd_pcm_dma_buf_done;
		(*dma_tx_des)->callback_param = (void *)(dma_pdata);
	}
	return ret;
}

static int sprd_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_runtime_data *rtd;
	int burst_len;
	int hw_chan;
	int ret;

	sp_asoc_pr_info("%s Open %s\n", sprd_dai_pcm_name(srtd->cpu_dai),
			PCM_DIR_NAME(substream->stream));
	snd_soc_set_runtime_hwparams(substream, &sprd_pcm_hardware);
	burst_len = (VBC_FIFO_FRAME_NUM * 4);
	hw_chan = 2;
	pr_info("burst_len=%#x\n", burst_len);

	/*
	 * For mysterious reasons (and despite what the manual says)
	 * playback samples are lost if the DMA count is not a multiple
	 * of the DMA burst size.  Let's add a rule to enforce that.
	 */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
					 SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
					 burst_len);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
					 SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
					 burst_len);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	ret = -ENOMEM;
	rtd = kzalloc(sizeof(*rtd), GFP_KERNEL);
	if (!rtd)
		goto out;

	rtd->burst_len = burst_len;
	rtd->hw_chan = hw_chan;
	runtime->private_data = rtd;
	ret = 0;
out:

	return ret;
}

static int sprd_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	int i;
	sp_asoc_pr_info("%s Close %s\n", sprd_dai_pcm_name(srtd->cpu_dai),
			PCM_DIR_NAME(substream->stream));

	if (!atomic_dec_return(&lightsleep_refcnt))
		sprd_lightsleep_disable("audio", 0);
	if (rtd->dma_pdata)
		kfree(rtd->dma_pdata);
	if (rtd->dma_cfg_array)
		kfree(rtd->dma_cfg_array);
#if (defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32))
	//free dma cfg
	if(rtd->dma_cfg_virt[0]){
		iounmap(rtd->dma_cfg_virt[0]);
		rtd->dma_cfg_virt[0] = NULL;
	}
	if(rtd->dma_cfg_phy[0]){
		audio_smem_free(rtd->dma_cfg_phy[0],SPRD_DDR32_DMA_CFG_SIZE);
		rtd->dma_cfg_phy[0] = NULL;
	}
#else
	for(i = 0; i < rtd->dma_stage*2; i++) {
		dma_free_coherent(substream->pcm->card->dev, runtime->hw.periods_max * sizeof(struct sprd_dma_cfg),
				rtd->dma_cfg_virt[i], rtd->dma_cfg_phy[i]);
	}
#endif

	if (rtd)
		kfree(rtd);
	return 0;
}

/*
 * proc interface
 */

//#ifdef CONFIG_SND_VERBOSE_PROCFS
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
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	int i;

	for (i = 0; i < rtd->hw_chan; i++) {
		if (rtd->uid_cid_map[i] >= 0) {
			snd_iprintf(buffer, "Channel%d Config\n",
				    rtd->uid_cid_map[i]);
			sprd_pcm_proc_dump_reg(rtd->uid_cid_map[i], buffer);
		}
	}
}

static void sprd_pcm_proc_init(struct snd_pcm_substream *substream)
{
	struct snd_info_entry *entry;
	struct snd_pcm_str *pstr = substream->pstr;
	struct snd_pcm *pcm = pstr->pcm;
	struct sprd_runtime_data *rtd = substream->runtime->private_data;

	if ((entry =
	     snd_info_create_card_entry(pcm->card, "DMA",
					pstr->proc_root)) != NULL) {
		snd_info_set_text_ops(entry, substream, sprd_pcm_proc_read);
		if (snd_info_register(entry) < 0) {
			snd_info_free_entry(entry);
			entry = NULL;
		}
	}
	rtd->proc_info_entry = entry;
}

static void sprd_pcm_proc_done(struct snd_pcm_substream *substream)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	snd_info_free_entry(rtd->proc_info_entry);
	rtd->proc_info_entry = NULL;
}
#else /* !CONFIG_SND_VERBOSE_PROCFS */
static inline void sprd_pcm_proc_init(struct snd_pcm_substream *substream)
{
}

static void sprd_pcm_proc_done(struct snd_pcm_substream *substream)
{
}
#endif


void * v_a_l = 0;
void * v_a_r = 0;
unsigned long long   p_a_l = 0;
unsigned long long   p_a_r = 0;

volatile struct sprd_dma_callback_data *g_dma_pdata_ptr_0 = NULL;
volatile struct sprd_dma_callback_data *g_dma_pdata_ptr_1 = NULL;

volatile struct sprd_dma_cfg * g_dma_config_ptr_0 = NULL;
volatile struct sprd_dma_cfg * g_dma_config_ptr_1 = NULL;

volatile struct sprd_dma_cfg * g_dma_cfg_array = NULL;
volatile struct sprd_dma_callback_data *g_dma_pdata = NULL;

volatile dma_addr_t g_dma_src_addr = 0;

int sprd_pcm_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma_data;
	struct dma_chan *dma_chn_request;
	struct sprd_dma_cfg *dma_config_ptr[4];
	struct sprd_dma_callback_data *dma_pdata_ptr[4];
	dma_cap_mask_t mask;
	size_t totsize = params_buffer_bytes(params);
	size_t period = params_period_bytes(params);
	dma_addr_t dma_buff_phys[4];
	enum dma_filter_param chn_type;
	int ret = 0;
	int i, j =0;
	int s = 0;
	int used_chan_count;
	int used_dma_stage;
	sp_asoc_pr_info("(pcm) %s, cpudai_id=%d\n", __func__, srtd->cpu_dai->id);
	dma_data = snd_soc_dai_get_dma_data(srtd->cpu_dai, substream);
	if (!dma_data)
		goto no_dma;

	used_chan_count = params_channels(params);
	if(VBC_DAI_ID_VOICE_CAPTURE == srtd->cpu_dai->id){
		used_chan_count = 1;
	}
	/*loop mcdt use one channel*/
	if(VBC_DAI_ID_LOOP_RECORD== srtd->cpu_dai->id){
		used_chan_count = 1;
	}
	if(VBC_DAI_ID_LOOP_PLAY== srtd->cpu_dai->id){
		used_chan_count = 1;
	}
	if(VBC_DAI_ID_FAST_P== srtd->cpu_dai->id){
		used_chan_count = 1;
	}
	if(VBC_DAI_ID_VOIP_RECORD== srtd->cpu_dai->id){
		used_chan_count = 1;
	}
	if(VBC_DAI_ID_VOIP_PLAY== srtd->cpu_dai->id){
		used_chan_count = 1;
	}
	used_dma_stage = dma_data->stage;
	sp_asoc_pr_info("chan=%d totsize=%d period=%d used_dma_stage=%d stream=%d\n", used_chan_count,
			totsize, period, used_dma_stage, substream->stream);

	ret = sprd_pcm_config_sprd_rtd(rtd, substream, used_dma_stage, used_chan_count);
	if (ret < 0) {
		goto hw_param_err;
	}
	for(s = 0; s < used_dma_stage; s++) {
		rtd->interleaved = (used_chan_count == 2)
		    && sprd_pcm_is_interleaved(runtime);
		if (rtd->interleaved) {
			sp_asoc_pr_dbg("Interleaved Access\n");
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				dma_data->desc[s].src_step = 4;
				/*dest step interleaved*/
				#if 1
				dma_data->desc[s].des_step = 0;
				#else
				dma_data->desc[s].des_step = 4;
				#endif
			} else {
				dma_data->desc[s].src_step = 0;
				dma_data->desc[s].des_step = 4;
			}
		} else {
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				if(VBC_DAI_ID_LOOP_PLAY == srtd->cpu_dai->id ||
					VBC_DAI_ID_FAST_P == srtd->cpu_dai->id ||
					VBC_DAI_ID_VOIP_PLAY == srtd->cpu_dai->id){
					dma_data->desc[s].src_step = 4;
					/*dest step*/
					#if 1
					dma_data->desc[s].des_step = 0;
					#else
					dma_data->desc[s].des_step = 4;
					#endif

				}else{
					dma_data->desc[s].src_step = 2;
					dma_data->desc[s].des_step = 0;
				}
			} else {
				if(VBC_DAI_ID_VOICE_CAPTURE == srtd->cpu_dai->id ||
					VBC_DAI_ID_LOOP_RECORD == srtd->cpu_dai->id ||
					VBC_DAI_ID_VOIP_RECORD== srtd->cpu_dai->id){
					dma_data->desc[s].src_step = 0;
					dma_data->desc[s].des_step = 4;
					sp_asoc_pr_dbg("src_step=%d, dest_step=%d\n",
						dma_data->desc[s].src_step, dma_data->desc[s].des_step);
				}else{
					dma_data->desc[s].src_step = 0;
					dma_data->desc[s].des_step = 2;
				}
			}
		}
	}

	/* this may get called several times by oss emulation
	 * with different params */
	if (rtd->params == NULL) {
		rtd->params = dma_data;
		for(s = 0; s < used_dma_stage; s++) {
			for (i = 0; i < used_chan_count; i++) {
				dma_cap_zero(mask);
				dma_cap_set(DMA_MEMCPY|DMA_SG,mask);
				/* request dma channel */
#ifndef CONFIG_SND_SOC_SPRD_AUDIO_USE_AON_DMA
				chn_type = AGCP_FULL_DMA;
#else
				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					chn_type = AON_FULL_DMA;
				} else {
					chn_type = AGCP_FULL_DMA;
				}
#endif
				dma_chn_request = dma_request_channel(mask, sprd_dma_filter_fn, (void *)&chn_type);
				if (!dma_chn_request) {
					pr_err("ERR:PCM Request DMA Error %d\n",
					       dma_data->channels[2*s+i]);
					for (i--; i >= 0; i--) {
						rtd->dma_chn[2*s+i] = NULL;
						rtd->dma_tx_des[2*s+i] = NULL;
						rtd->cookie[2*s+i] = 0;
						rtd->params = NULL;
					}
					goto hw_param_err;
				}
				rtd->dma_chn[2*s+i] = dma_chn_request;
				sp_asoc_pr_dbg("Chan%d DMA ID=%d, chn_type:%d \n",
					       rtd->dma_chn[2*s+i]->chan_id,
					       rtd->params->channels[2*s+i], chn_type);
			}
		}
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totsize;

	rtd->dma_addr_offset = (totsize / used_chan_count);
	if (sprd_pcm_is_interleaved(runtime)){
		rtd->dma_addr_offset = 2;
		sp_asoc_pr_dbg("sprd_pcm_is_interleaved\n");
	}
	sp_asoc_pr_dbg("rtd->dma_addr_offset=%d\n", rtd->dma_addr_offset);
	g_dma_pdata  = rtd->dma_pdata;
	g_dma_cfg_array = rtd->dma_cfg_array;
	g_dma_src_addr = runtime->dma_addr;
	for (s = 0; s < used_dma_stage; s++) {
		for (i = 0; i < used_chan_count; i++) {
			dma_config_ptr[2*s+i] = rtd->dma_cfg_array + (2*s+i) * (runtime->hw.periods_max);
			memset(dma_config_ptr[2*s+i],0,sizeof(struct sprd_dma_cfg) * runtime->hw.periods_max);
			dma_pdata_ptr[2*s+i] = rtd->dma_pdata + (2*s+i);
			memset(dma_pdata_ptr[2*s+i],0,sizeof(struct sprd_dma_callback_data));
			dma_pdata_ptr[2*s+i]->dma_chn = rtd->dma_chn[(2*s+i)];
			dma_pdata_ptr[2*s+i]->substream = substream; 
			dma_buff_phys[2*s+i] = runtime->dma_addr + (2*s+i) * rtd->dma_addr_offset;
			sp_asoc_pr_dbg("runtime->dma_addr=%lx\n", runtime->dma_addr);
		}
	}
#if 1

	do {
		for (s = 0; s < used_dma_stage; s++) {
			for (i = 0; i < used_chan_count; i++) {
				(dma_config_ptr[2*s+i]+j)->datawidth = dma_data->desc[s].datawidth;
				(dma_config_ptr[2*s+i]+j)->fragmens_len = dma_data->desc[s].fragmens_len;
				(dma_config_ptr[2*s+i]+j)->block_len = period/used_chan_count;
				(dma_config_ptr[2*s+i]+j)->transcation_len = period/used_chan_count;
				(dma_config_ptr[2*s+i]+j)->req_mode = FRAG_REQ_MODE;
				if (!(params->flags & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP)) {
					(dma_config_ptr[2*s+i]+j)->irq_mode= BLK_DONE;
				} else {
					(dma_config_ptr[2*s+i]+j)->irq_mode= NO_INT;
				}

				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					(dma_config_ptr[2*s+i]+j)->src_step = dma_data->desc[s].src_step;
					(dma_config_ptr[2*s+i]+j)->des_step = dma_data->desc[s].des_step;
					(dma_config_ptr[2*s+i]+j)->src_addr = dma_buff_phys[2*s+i];
					sp_asoc_pr_dbg("dma src_addr = dma_buff_phys[2*s+i]= %lx", dma_buff_phys[2*s+i]);
					(dma_config_ptr[2*s+i]+j)->des_addr = dma_data->dev_paddr[s].dev_addr[i];

				} else {
					(dma_config_ptr[2*s+i]+j)->src_step = dma_data->desc[s].src_step;
					(dma_config_ptr[2*s+i]+j)->des_step = dma_data->desc[s].des_step;
					(dma_config_ptr[2*s+i]+j)->src_addr = dma_data->dev_paddr[s].dev_addr[i];
					(dma_config_ptr[2*s+i]+j)->des_addr = dma_buff_phys[2*s+i];
				}
				dma_buff_phys[2*s+i] += (dma_config_ptr[2*s+i]+j)->block_len;
				if (rtd->interleaved)
					dma_buff_phys[2*s+i] += (dma_config_ptr[2*s+i]+j)->block_len;

				(dma_config_ptr[2*s+i]+j)->link_cfg_v = (unsigned long)(rtd->dma_cfg_virt[2*s+i]);
				(dma_config_ptr[2*s+i]+j)->link_cfg_p = (unsigned long)(rtd->dma_cfg_phy[2*s+i]);
				sp_asoc_pr_dbg("rtd->dma_cfg_phy[2*s+i=%d]) = %#llx", 2*s+i,rtd->dma_cfg_phy[2*s+i]);
				(dma_config_ptr[2*s+i]+j)->dev_id = dma_data->channels[2*s+i];//DMA_UID_SOFTWARE;				
				sp_asoc_pr_dbg("%s, dma_config_ptr[%d]+%d: datawidth:%d, fragmens_len:%d, block_len:%d, src_step:%d, des_step:%d, link_cfg_v:0x%lx, link_cfg_p:0x%x, dev_id:%d \n",
						__func__,2*s+i, j, (dma_config_ptr[2*s+i]+j)->datawidth, (dma_config_ptr[2*s+i]+j)->fragmens_len, (dma_config_ptr[2*s+i]+j)->block_len, (dma_config_ptr[2*s+i]+j)->src_step, (dma_config_ptr[2*s+i]+j)->des_step, (dma_config_ptr[2*s+i]+j)->link_cfg_v, (dma_config_ptr[2*s+i]+j)->link_cfg_p, (dma_config_ptr[2*s+i]+j)->dev_id);
			}
		}
		if (period > totsize)
			period = totsize;
		j++;
	} while (totsize -= period);
	for (s = 0; s < used_dma_stage; s++) {
		for (i = 0; i < used_chan_count; i++) {
			(dma_config_ptr[2*s+i]+j-1)->is_end = 2;
		}
	}

	sp_asoc_pr_info("Node Size:%d\n", j);
#else
/* dma wrap */
do {
               for (s = 0; s < used_dma_stage; s++) {                                                                                                                                                               
         for (i = 0; i < used_chan_count; i++) {
                 (dma_config_ptr[2*s+i]+j)->datawidth = dma_data->desc[s].datawidth;
                 (dma_config_ptr[2*s+i]+j)->fragmens_len = dma_data->desc[s].fragmens_len;
                 (dma_config_ptr[2*s+i]+j)->block_len = 0x10000;
                 (dma_config_ptr[2*s+i]+j)->transcation_len = totsize/used_chan_count * 16; 
                 (dma_config_ptr[2*s+i]+j)->req_mode = FRAG_REQ_MODE;
                 if (!(params->flags & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP)) {
                         (dma_config_ptr[2*s+i]+j)->irq_mode= FRAG_DONE;
                 } else {
                         (dma_config_ptr[2*s+i]+j)->irq_mode= NO_INT;
                 }

                 if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
                         (dma_config_ptr[2*s+i]+j)->src_step = dma_data->desc[s].src_step;
                         (dma_config_ptr[2*s+i]+j)->des_step = dma_data->desc[s].des_step;
                         (dma_config_ptr[2*s+i]+j)->src_addr = dma_buff_phys[2*s+i];
                         (dma_config_ptr[2*s+i]+j)->des_addr = dma_data->dev_paddr[s].dev_addr[i];
                         (dma_config_ptr[2*s+i]+j)->wrap_ptr = ((u32)dma_buff_phys[2*s+i]+totsize);
                         (dma_config_ptr[2*s+i]+j)->wrap_to = dma_buff_phys[2*s+i];
                 } else {
                         (dma_config_ptr[2*s+i]+j)->src_step = dma_data->desc[s].src_step;
                         (dma_config_ptr[2*s+i]+j)->des_step = dma_data->desc[s].des_step;
                         (dma_config_ptr[2*s+i]+j)->src_addr = dma_data->dev_paddr[s].dev_addr[i];
                         (dma_config_ptr[2*s+i]+j)->des_addr = dma_buff_phys[2*s+i];
                         (dma_config_ptr[2*s+i]+j)->wrap_ptr = ((u32)dma_buff_phys[2*s+i]+totsize);
                         (dma_config_ptr[2*s+i]+j)->wrap_to = dma_buff_phys[2*s+i];
                 }
                 (dma_config_ptr[2*s+i]+j)->dev_id = dma_data->channels[2*s+i];
                 (dma_config_ptr[2*s+i]+j)->link_cfg_v = (unsigned long)(rtd->dma_cfg_virt[2*s+i]);
                 (dma_config_ptr[2*s+i]+j)->link_cfg_p = (unsigned long)(rtd->dma_cfg_phy[2*s+i]);
                 //sp_asoc_pr_info("%s, dma_data->desc[%d].fragmens_len:%d, src_step:%d, dma_cfg_virt:0x%lx \n", __func__, s, dma_data->desc[s].fragmens_len, dma_data->desc[s].src_step, (rtd->dma_c
                 //sp_asoc_pr_info("%s, dma_config_ptr[%d]: datawidth:%d, fragmens_len:%d, block_len:%d, src_step:%d, des_step:%d, link_cfg_v:0x%lx, link_cfg_p:0x%x, dev_id:%d \n",
                                 //__func__,2*s+i, (dma_config_ptr[2*s+i])->datawidth, (dma_config_ptr[2*s+i])->fragmens_len, (dma_config_ptr[2*s+i])->block_len, (dma_config_ptr[2*s+i])->src_step, 
         }
 }
 j++;
 } while (j < 2);
 for (s = 0; s < used_dma_stage; s++) {
         for (i = 0; i < used_chan_count; i++) {
                 (dma_config_ptr[2*s+i]+j-1)->is_end = 2;
         }
 }

#endif
	g_dma_pdata_ptr_0 = dma_pdata_ptr[0];
	g_dma_pdata_ptr_1 = dma_pdata_ptr[1];

	g_dma_config_ptr_0 = dma_config_ptr[0];
	g_dma_config_ptr_1 = dma_config_ptr[1];
	/* config dma channel */
	for (s = 0; s < used_dma_stage; s++) {
		for (i = 0; i < used_chan_count; i++) {
			ret = sprd_pcm_config_dma_channel(rtd->dma_chn[2*s+i], dma_config_ptr[2*s+i],
					&(rtd->dma_tx_des[2*s+i]), dma_pdata_ptr[2*s+i], params->flags);
			if (ret < 0) {
				goto hw_param_err;
			}
		}
	}
	sprd_pcm_proc_init(substream);

	goto ok_go_out;

no_dma:
	sp_asoc_pr_info("no dma\n");
	rtd->params = NULL;
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totsize;
	return ret;
hw_param_err:
	pr_err("hw_param_err\n");
ok_go_out:
	pr_info("return %i\n", ret);
	return ret;
}

static int sprd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	struct sprd_pcm_dma_params *dma = rtd->params;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	int i,s;
	#if 0
	printk(KERN_EMERG "before dump stack\n");
	dump_stack();
	printk(KERN_EMERG "after dump stack\n");
	#endif
	sp_asoc_pr_info("(pcm) %s, cpudai_id=%d\n", __func__, srtd->cpu_dai->id);
	snd_pcm_set_runtime_buffer(substream, NULL);

	if (dma) {
		for (s = 0; s < rtd->dma_stage; s++) {
			for (i = 0; i < rtd->hw_chan; i++) {
				if (rtd->dma_chn[2*s+i]) {
					dma_release_channel(rtd->dma_chn[2*s+i]);
					rtd->dma_chn[2*s+i] = NULL;
					rtd->dma_tx_des[2*s+i] = NULL;
					rtd->cookie[2*s+i] = 0;
				}
			}
		}
		rtd->params = NULL;
	}

	sprd_pcm_proc_done(substream);

	return 0;
}

static int sprd_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int sprd_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma = rtd->params;
	int ret = 0;
	int i,s;
	sp_asoc_pr_info("%s, %s cpu_dai->id = %d Trigger %s cmd:%d\n", __func__, sprd_dai_pcm_name(srtd->cpu_dai),srtd->cpu_dai->id,
			PCM_DIR_NAME(substream->stream), cmd);
	if (!dma) {
		sp_asoc_pr_info("no trigger");
		return 0;
	}
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		for(s = 0; s < rtd->dma_stage; s++) {
			for (i = 0; i < rtd->hw_chan; i++) {
				if (rtd->dma_tx_des[2*s+i]) {
					rtd->cookie[2*s+i] = dmaengine_submit(rtd->dma_tx_des[2*s+i]);
				}
			}
		}
		dma_issue_pending_all();
		sp_asoc_pr_info("S\n");
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		for(s = 0; s < rtd->dma_stage; s++) {
			for (i = 0; i < rtd->hw_chan; i++) {
				if (rtd->dma_chn[2*s+i]) {
					dmaengine_pause(rtd->dma_chn[2*s+i]);
				}
			}
		}
		rtd->cb_called = 0;
		sp_asoc_pr_info("E\n");
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t sprd_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	snd_pcm_uframes_t x;
	int now_pointer;
	int  bytes_of_pointer = 0;
	int  shift = 1;
	int sel_max = 0;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;

	if (rtd->interleaved)
		shift = 0;
	if (rtd->dma_chn[0]) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn[0],
					rtd->cookie[0], substream) -
			runtime->dma_addr;
		bytes_of_pointer = now_pointer;
	}
	if (rtd->dma_chn[1]) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn[1],
					rtd->cookie[1], substream) -
			runtime->dma_addr - rtd->dma_addr_offset;
		if (!bytes_of_pointer) {
			bytes_of_pointer = now_pointer;
		} else {
			sel_max = (bytes_of_pointer < rtd->dma_pos_pre[0]);
			sel_max ^= (now_pointer < rtd->dma_pos_pre[1]);
			rtd->dma_pos_pre[0] = bytes_of_pointer;
			rtd->dma_pos_pre[1] = now_pointer;
			if (sel_max) {
				bytes_of_pointer =
					max(bytes_of_pointer, now_pointer) << shift;
			} else {
				bytes_of_pointer =
					min(bytes_of_pointer, now_pointer) << shift;
			}
		}
	}
	x = bytes_to_frames(runtime, bytes_of_pointer);

	if (x == runtime->buffer_size)
		x = 0;
	return x;
}

static int sprd_pcm_mmap(struct snd_pcm_substream *substream,
			 struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	sp_asoc_pr_info("(vbc) %s,  runtime->dma_area=%#lx, runtime->dma_addr=%#lx runtime->dma_bytes=%#lx\n", __func__,  runtime->dma_area, runtime->dma_addr, runtime->dma_bytes);
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr, runtime->dma_bytes);
}

static struct snd_pcm_ops sprd_pcm_ops = {
	.open = sprd_pcm_open,
	.close = sprd_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = sprd_pcm_hw_params,
	.hw_free = sprd_pcm_hw_free,
	.prepare = sprd_pcm_prepare,
	.trigger = sprd_pcm_trigger,
	.pointer = sprd_pcm_pointer,
	.mmap = sprd_pcm_mmap,
};

void * s_v_buf = 0;
unsigned long long   s_p_buf = 0;
int sprd_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = AUDIO_BUFFER_BYTES_MAX;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
	{
		buf->private_data = NULL;
		buf->addr = audio_smem_alloc(SPRD_DDR32_DMA_DATA_SIZE);
		buf->area = ioremap_nocache(buf->addr, SPRD_DDR32_DMA_DATA_SIZE);
		size = SPRD_DDR32_DMA_DATA_SIZE;
		memset(buf->area, 0, size);
		sp_asoc_pr_info("%s, sprd_pcm_preallocate_dma_buffer buf->area(virt)=%#lx, buf->addr(phy)=%#x\n",__func__,buf->area, buf->addr);
	}
#else
	size = VBC_BUFFER_BYTES_MAX;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
#endif
	s_v_buf=buf->area;
	s_p_buf=buf->addr;
	sp_asoc_pr_dbg("s_v_buf=%#llx, s_p_buf=%#llx", s_v_buf, s_p_buf);
	if (!buf->area){
		return -ENOMEM;
	}
	buf->bytes = size;
	return 0;
}
#ifdef CONFIG_ARM64
static u64 sprd_pcm_dmamask = DMA_BIT_MASK(64);
#else
static u64 sprd_pcm_dmamask = DMA_BIT_MASK(32);
#endif
static struct snd_dma_buffer *save_p_buf = 0;
#define VBC_CHAN (VBC_DAI_ID_MAX)
static struct snd_dma_buffer *save_c_buf[VBC_CHAN] = { 0 };

static int sprd_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_pcm_substream *substream;
	int ret = 0;
	sp_asoc_pr_info("%s %s cpu_dai_id=%d\n", __func__, sprd_dai_pcm_name(cpu_dai), cpu_dai->id);
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sprd_pcm_dmamask;
	#ifdef CONFIG_ARM64
	card->dev->coherent_dma_mask = DMA_BIT_MASK(64);
	#else
	card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	#endif
	substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (substream) {
		struct snd_dma_buffer *buf = &substream->dma_buffer;
//		if (!save_p_buf) {
			ret = sprd_pcm_preallocate_dma_buffer(pcm,
							      SNDRV_PCM_STREAM_PLAYBACK);
			if (ret)
				goto out;
//			save_p_buf = buf;
			sp_asoc_pr_info("Playback alloc memery\n");
//		} else {
//			memcpy(buf, save_p_buf, sizeof(*buf));
//			pr_info("Playback share memery\n");
//		}
	}

	substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (substream) {
		int id = cpu_dai->driver->id;
		struct snd_dma_buffer *buf = &substream->dma_buffer;

//		if (!save_c_buf[id]) {
			ret = sprd_pcm_preallocate_dma_buffer(pcm,
							      SNDRV_PCM_STREAM_CAPTURE);
			if (ret)
				goto out;
//			save_c_buf[id] = buf;
			pr_info("Capture alloc memery %d\n", id);
//		} else {
//			memcpy(buf, save_c_buf[id], sizeof(*buf));
//			pr_info("Capture share memery %d\n", id);
//}
	}
out:
	sp_asoc_pr_info("return %d\n", ret);
	return ret;
}

void sprd_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;
	int i;

	sp_asoc_pr_info("%s\n", __func__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;
		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
#if (!defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32))
		dma_free_coherent(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
#endif
#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
	if(buf->area){
		iounmap(buf->area);
		buf->area = NULL;
	}
	if(buf->addr){
		audio_smem_free(buf->addr,buf->bytes);
		buf->addr = NULL;
	}
#endif
		buf->area = NULL;
//if (buf == save_p_buf) {
//	save_p_buf = 0;
//}
//for (i = 0; i < VBC_CHAN; i++) {
//	if (buf == save_c_buf[i]) {
//		save_c_buf[i] = 0;
//	}
//}
	}
}

static struct snd_soc_platform_driver sprd_soc_platform = {
	.ops = &sprd_pcm_ops,
	.pcm_new = sprd_pcm_new,
	.pcm_free = sprd_pcm_free_dma_buffers,
};

#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
static int parse_ddr32_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct resource res ={0};
	struct resource *pres = NULL;
	struct device_node *np = pdev->dev.of_node;
	if(!np){
		pr_err("%s, of_node failed np=%#lx\n",__func__, np);
		ret = -EINVAL;
		goto err;
	}
	#if 1
	if(of_can_translate_address(np)){
		ret = of_address_to_resource(np, 0, &res);
		if (0 != ret) {
			pr_err("of_address_to_resource failed ret =%d\n", ret);
			goto err;
		}
	}else{
		pr_err("of_can_translate_address failed np=%#lx\n", np);
		goto err;
	}
	sp_asoc_pr_dbg("%s res.start=%#x, res.end=%#x\n",__func__, res.start, res.end);
	audio_smem_init((unsigned int)res.start, (unsigned int)(res.end - res.start + 1));
	#else
	pres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!pres) {
			pr_err("Error: Can't get SOURCE DMA address!\n");
			ret = -EBUSY;
	}
	printk(KERN_EMERG "pres->start=%#x, size=%#x\n", pres->start, resource_size(pres));
	init_data->sprd_ddr32_phys_dma_base= (unsigned int)pres->start;
	init_data->sprd_ddr32_phys_total_size = (unsigned int)resource_size(pres);
	#endif
err:
	return ret;
}
#endif
static int sprd_soc_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	#if defined(CONFIG_SND_SOC_SPRD_AUDIO_BUFFER_USE_DDR32)
	ret = parse_ddr32_dt(pdev);
	if(ret < 0){
		pr_err("%s parse_dt err %d\n", __func__, ret);
		return -EINVAL;
	}
	#endif
	return snd_soc_register_platform(&pdev->dev, &sprd_soc_platform);
}

static int sprd_soc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sprd_pcm_of_match[] = {
	{.compatible = "sprd,sprd-pcm",},
	{},
};

MODULE_DEVICE_TABLE(of, sprd_pcm_of_match);
#endif

static struct platform_driver sprd_pcm_driver = {
	.driver = {
		   .name = "sprd-pcm-audio",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(sprd_pcm_of_match),
		   },

	.probe = sprd_soc_platform_probe,
	.remove = sprd_soc_platform_remove,
};

module_platform_driver(sprd_pcm_driver);

MODULE_DESCRIPTION("SPRD ASoC PCM DMA");
MODULE_AUTHOR("Jian chen <jian.chen@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sprd-audio");
