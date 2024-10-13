/*
 * sound/soc/sprd/dai/vbc/r3p0/vbc-dai.c
 *
 * SPRD SoC VBC -- SpreadTrum SOC for VBC DAI function.
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
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
#define pr_fmt(fmt) pr_sprd_fmt(" VBC ") fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "sprd-asoc-common.h"
#include <../../../../../drivers/mcdt/mcdt_hw.h>


#ifndef CONFIG_SND_SOC_SPRD_AUDIO_DMA_ENGINE
#include "sprd-pcm.h"
#elif (defined(CONFIG_SND_SOC_SPRD_MACHINE_TI) || defined(CONFIG_SND_SOC_SPRD_MACHINE_4AUDIENCE) || defined(CONFIG_SND_SOC_SPRD_MACHINE_4REALTEK))
#include "sprd4whale-dmaengine-pcm.h"
#else
#include "sprd-dmaengine-pcm.h"
#endif
#include "vbc-dai.h"

#include <linux/dma-mapping.h>

static struct sprd_pcm_dma_params vbc_pcm_normal_p_outdsp = {
	.name = "VBC PCM Normal P Out DSP",
	.irq_type = BLK_DONE,
	.stage = 1,	//test
	.desc = {
		{
		 .datawidth = 1,
		 .fragmens_len = (VBC_FIFO_FRAME_NUM - VB_AUDPLY_EMPTY_WATERMARK) * 2,
		 //src_step, dst_step:sprd-dmaengine-pcm.c
		},
	},
};

static struct sprd_pcm_dma_params vbc_pcm_normal_c_outdsp = {
	.name = "VBC PCM Normal C Out DSP",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = 1,
		 .fragmens_len = (VBC_FIFO_FRAME_NUM - VB_AUDPLY_EMPTY_WATERMARK) * 2,
		},
	},
};

static struct sprd_pcm_dma_params vbc_pcm_normal_p_withdsp = {
	.name = "VBC PCM Normal P With DSP",
	.irq_type = BLK_DONE,
	.stage = 2,
	.desc = {
		{
		 .datawidth = 1,
		 .fragmens_len = VBC_FIFO_FRAME_NUM * 2,
		 .src_step = 2,
		 .des_step = 0,
		},
		{
		 .datawidth = 1,
		 .fragmens_len = VBC_FIFO_FRAME_NUM * 2,
		 .src_step = 2,
		 .des_step = 0,
		},
	},
};

static struct sprd_pcm_dma_params vbc_pcm_normal_c_withdsp = {
	.name = "VBC PCM Normal C With DSP",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = SHORT_WIDTH,
		 .fragmens_len = VBC_FIFO_FRAME_NUM * 2,
		 .src_step = 0,
		 .des_step = 2,
		},
	},
};

static struct sprd_pcm_dma_params pcm_fast_play_mcdt = {
	.name = "VBC PCM Fast P",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = WORD_WIDTH,
		 .fragmens_len = (512 - MCDT_EMPTY_WMK_FAST_PLAY) * 4,
		 .src_step = 4,
		 .des_step = 0,
		},
	},
};

static struct sprd_pcm_dma_params vbc_pcm_offload_c = {
	.name = "VBC Offload C",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = SHORT_WIDTH,
		 .fragmens_len = VBC_FIFO_FRAME_NUM * 2,
		 .src_step = 2,
		 .des_step = 0,
		},
	},
};

static struct sprd_pcm_dma_params vbc_pcm_voice_capture_mcdt = {
	.name = "VBC PCM voice C With MCDT",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = WORD_WIDTH,
		 .fragmens_len = (MCDT_FULL_WMK_VOICE_CAPTURE) * 4,
		 .src_step = 0,/*ignore, config in sprd-dmaengine-pcm.c*/
		 .des_step = 4,/*ignore, config in sprd-dmaengine-pcm.c*/
		},
	},
};

static struct sprd_pcm_dma_params pcm_loop_record_mcdt = {
	.name = "PCM loop record With MCDT",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = WORD_WIDTH,
		 .fragmens_len = (MCDT_FULL_WMK_LOOP) * 4,
		 .src_step = 0,/*ignore, config in sprd-dmaengine-pcm.c*/
		 .des_step = 4,/*ignore, config in sprd-dmaengine-pcm.c*/
		},
	},
};

static struct sprd_pcm_dma_params pcm_loop_play_mcdt = {
	.name = "PCM loop play With MCDT",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = WORD_WIDTH,
		 .fragmens_len = (MCDT_EMPTY_WMK_LOOP) * 4,
		 .src_step = 0,/*ignore, config in sprd-dmaengine-pcm.c*/
		 .des_step = 4,/*ignore, config in sprd-dmaengine-pcm.c*/
		},
	},
};
/*voip*/
static struct sprd_pcm_dma_params pcm_voip_record_mcdt = {
	.name = "PCM voip record With MCDT",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = WORD_WIDTH,
		 .fragmens_len = (MCDT_FULL_WMK_VOIP) * 4,
		 .src_step = 0,/*ignore, config in sprd-dmaengine-pcm.c*/
		 .des_step = 4,/*ignore, config in sprd-dmaengine-pcm.c*/
		},
	},
};

static struct sprd_pcm_dma_params pcm_voip_play_mcdt = {
	.name = "PCM voip play With MCDT",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = WORD_WIDTH,
		 .fragmens_len = (MCDT_EMPTY_WMK_VOIP) * 4,
		 .src_step = 0,/*ignore, config in sprd-dmaengine-pcm.c*/
		 .des_step = 4,/*ignore, config in sprd-dmaengine-pcm.c*/
		},
	},
};
/*fm_caputre*/
static struct sprd_pcm_dma_params vbc_pcm_fm_caputre = {
	.name = "VBC PCM fm capture",
	.irq_type = BLK_DONE,
	.stage = 1,
	.desc = {
		{
		 .datawidth = 1,
		 .fragmens_len = (VBC_FIFO_FRAME_NUM - VB_AUDPLY_EMPTY_WATERMARK) * 2,
		},
	},
};


struct sprd_vbc_priv g_vbc[VBC_DAI_ID_MAX] = {// index=dai_id
	/* normal outdsp */
	[VBC_DAI_ID_NORMAL_OUTDSP]={
		.dac_id = VBC_DA0,
		.adc_id = VBC_AD0,
		.out_sel = VBC_MUX_DAC0_OUT_SEL-VBC_MUX_START,
		.adc_source_sel = VBC_MUX_ADC0_SOURCE-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC0_IIS_SEL-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC0_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_PLAYBACK] = &vbc_pcm_normal_p_outdsp,
		.dma_params[SND_PCM_CAPTURE] = &vbc_pcm_normal_c_outdsp,
	},
	/* normal withdsp */
	[VBC_DAI_ID_NORMAL_WITHDSP]={
		.dac_id = VBC_DA0,
		.adc_id = VBC_AD1,
		.out_sel = VBC_MUX_DAC0_OUT_SEL-VBC_MUX_START,
		.adc_source_sel = VBC_MUX_ADC1_SOURCE-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC0_IIS_SEL-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC1_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_PLAYBACK] = &vbc_pcm_normal_p_withdsp,
		.dma_params[SND_PCM_CAPTURE] = &vbc_pcm_normal_c_withdsp,
	},
	/* fast play */
	[VBC_DAI_ID_FAST_P]={
		.dac_id = VBC_DA0,
		.out_sel = VBC_MUX_DAC0_OUT_SEL-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC0_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_PLAYBACK] = &pcm_fast_play_mcdt,
	},
	/* offload */
	[VBC_DAI_ID_OFFLOAD]={
		.dac_id = VBC_DA0,
		.adc_id = VBC_AD1,
		.adc_source_sel = VBC_MUX_ADC1_SOURCE-VBC_MUX_START,
		.out_sel = VBC_MUX_DAC0_OUT_SEL-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC0_IIS_SEL-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC1_IIS_SEL-VBC_MUX_START,
	},
	/* voice */
	[VBC_DAI_ID_VOICE]={
		.dac_id = VBC_DA1,
		.adc_id = VBC_AD2,
		.out_sel = VBC_MUX_DAC1_OUT_SEL-VBC_MUX_START,
		.adc_source_sel = VBC_MUX_ADC2_SOURCE-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC1_IIS_SEL-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC2_IIS_SEL-VBC_MUX_START,
	},
	/* voip record */
	[VBC_DAI_ID_VOIP_RECORD]={
		.dac_id = VBC_DA1,
		.adc_id = VBC_AD2,
		.out_sel = VBC_MUX_DAC1_OUT_SEL-VBC_MUX_START,
		.adc_source_sel = VBC_MUX_ADC2_SOURCE-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC1_IIS_SEL-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC2_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_CAPTURE] = &pcm_voip_record_mcdt,
	},
	/* voip play */
	[VBC_DAI_ID_VOIP_PLAY]={
		.dac_id = VBC_DA1,
		.adc_id = VBC_AD2,
		.out_sel = VBC_MUX_DAC1_OUT_SEL-VBC_MUX_START,
		.adc_source_sel = VBC_MUX_ADC2_SOURCE-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC1_IIS_SEL-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC2_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_PLAYBACK] = &pcm_voip_play_mcdt,
	},
	/* fm */
	[VBC_DAI_ID_FM]={
		.dac_id = VBC_DA0,
		.adc_id = VBC_AD3,
		.out_sel = VBC_MUX_DAC0_OUT_SEL-VBC_MUX_START,
		.adc_source_sel = VBC_MUX_ADC3_SOURCE-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC0_IIS_SEL-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC3_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_CAPTURE] = NULL,
		.dma_params[SND_PCM_PLAYBACK] = NULL,
	},
	/* fm capture withdsp */
	[VBC_DAI_ID_FM_C_WITHDSP]={
		.adc_id = VBC_AD3,
		.adc_source_sel = VBC_MUX_ADC3_SOURCE-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC3_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_CAPTURE] = &vbc_pcm_normal_c_withdsp,
	},
	/* voice-capture */
	[VBC_DAI_ID_VOICE_CAPTURE]={
		.dac_id = VBC_DA1,
		.adc_id = VBC_AD1,
		.out_sel = VBC_MUX_DAC1_OUT_SEL-VBC_MUX_START,
		.adc_source_sel = VBC_MUX_ADC1_SOURCE-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC1_IIS_SEL-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC1_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_CAPTURE] = &vbc_pcm_voice_capture_mcdt,
	},
	/* loop record
	*/
	[VBC_DAI_ID_LOOP_RECORD]={
		.adc_id = VBC_AD1,
		.dac_id = VBC_DA0,
		.adc_source_sel = VBC_MUX_ADC1_SOURCE-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC1_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_CAPTURE] = &pcm_loop_record_mcdt,
	},
	/* loop play 
	*/
	[VBC_DAI_ID_LOOP_PLAY]={
		.adc_id = VBC_AD1,
		.dac_id = VBC_DA0,
		.out_sel = VBC_MUX_DAC0_OUT_SEL-VBC_MUX_START,
		.dac_iis_port = VBC_MUX_DAC0_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_PLAYBACK] = &pcm_loop_play_mcdt,
	},
	/* fm_capture */
	[VBC_DAI_ID_FM_CAPTURE]={
		.adc_id = VBC_AD3,
		.adc_source_sel = VBC_MUX_ADC3_SOURCE-VBC_MUX_START,
		.adc_iis_port = VBC_MUX_ADC3_IIS_SEL-VBC_MUX_START,
		.dma_params[SND_PCM_CAPTURE] = &vbc_pcm_fm_caputre,
	},
};

static int is_ap_vbc_ctl(int id, int stream)
{
	int ret = 0;
	  switch(id){
		  case VBC_DAI_ID_NORMAL_OUTDSP:
          {
			  ret = 1;
			  break;
          }
          case VBC_DAI_ID_FM_CAPTURE:
          {
              ret = 1;
              break;
          }
		  default:
			  ret = 0;
			  break;
	  }
	  return ret;
}

extern unsigned int sprd_ap_vbc_phy_base;
static void sprd_dma_config_init(struct platform_device *pdev)
{
	/*normal playback*/
	#if 1
	vbc_pcm_normal_p_outdsp.dev_paddr[0].dev_addr[0] = VBC_PHY_AP_TO_AGCP(VBC_AUDPLY_FIFO_WR_0 - VBC_OFLD_ADDR + sprd_ap_vbc_phy_base);
	vbc_pcm_normal_p_outdsp.dev_paddr[0].dev_addr[1] = VBC_PHY_AP_TO_AGCP(VBC_AUDPLY_FIFO_WR_1 - VBC_OFLD_ADDR + sprd_ap_vbc_phy_base);
	#else
	vbc_pcm_normal_p_outdsp.dev_paddr[0].dev_addr[0] = 0x89451840;
	vbc_pcm_normal_p_outdsp.dev_paddr[0].dev_addr[1] = 0x89451842;
	#endif
	vbc_pcm_normal_p_outdsp.channels[0] = DMA_REQ_DA0_DEV_ID;
	vbc_pcm_normal_p_outdsp.channels[1] = DMA_REQ_DA1_DEV_ID;
	/*normal capture*/
	vbc_pcm_normal_c_outdsp.dev_paddr[0].dev_addr[0] = VBC_PHY_AP_TO_AGCP(VBC_AUDRCD_FIFO_RD_0 - VBC_OFLD_ADDR + sprd_ap_vbc_phy_base);
	vbc_pcm_normal_c_outdsp.dev_paddr[0].dev_addr[1] = VBC_PHY_AP_TO_AGCP(VBC_AUDRCD_FIFO_RD_1 - VBC_OFLD_ADDR + sprd_ap_vbc_phy_base);
	vbc_pcm_normal_c_outdsp.channels[0] = DMA_REQ_AD0_DEV_ID;
	vbc_pcm_normal_c_outdsp.channels[1] = DMA_REQ_AD1_DEV_ID;
    /*voic capture*/
    vbc_pcm_voice_capture_mcdt.dev_paddr[0].dev_addr[0] = mcdt_adc_dma_phy_addr(MCDT_CHAN_VOICE_CAPTURE);// dma src address
	#if 1
	/*loop back*/
	pcm_loop_record_mcdt.dev_paddr[0].dev_addr[0] = mcdt_adc_dma_phy_addr(MCDT_CHAN_LOOP);
	pcm_loop_play_mcdt.dev_paddr[0].dev_addr[0] = mcdt_dac_dma_phy_addr(MCDT_CHAN_LOOP);
	#else
	pcm_loop_record_mcdt.dev_paddr[0].dev_addr[0] = 0x89450000;
	pcm_loop_play_mcdt.dev_paddr[0].dev_addr[0]   = 0x89450004;
	#endif
	/*fast playback*/
	#if 1
	pcm_fast_play_mcdt.dev_paddr[0].dev_addr[0] = mcdt_dac_dma_phy_addr(MCDT_CHAN_FAST_PLAY);
	#else
	pcm_fast_play_mcdt.dev_paddr[0].dev_addr[0] = 0x89450000;
	#endif
	/*voip*/
	pcm_voip_record_mcdt.dev_paddr[0].dev_addr[0] = mcdt_adc_dma_phy_addr(MCDT_CHAN_VOIP);
	pcm_voip_play_mcdt.dev_paddr[0].dev_addr[0] = mcdt_dac_dma_phy_addr(MCDT_CHAN_VOIP);
    /*fm capture*/
    vbc_pcm_fm_caputre.dev_paddr[0].dev_addr[0] = VBC_PHY_AP_TO_AGCP(VBC_AUDRCD_FIFO_RD_0 - VBC_OFLD_ADDR + sprd_ap_vbc_phy_base);
	vbc_pcm_fm_caputre.dev_paddr[0].dev_addr[1] = VBC_PHY_AP_TO_AGCP(VBC_AUDRCD_FIFO_RD_1 - VBC_OFLD_ADDR + sprd_ap_vbc_phy_base);
	vbc_pcm_fm_caputre.channels[0] = DMA_REQ_AD0_DEV_ID;
	vbc_pcm_fm_caputre.channels[1] = DMA_REQ_AD1_DEV_ID;
}
static int vbc_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int ret = 0;
	sp_asoc_pr_info("%s VBC(%s-%s)\n",__func__, dai->driver->name,
		(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ? "Capture" : "Playback");
	/*offload mixer normal: 1.1 startup dsp disable dac0 fifo, enable ofld_dp  1.2 hwparam ap pre data,enable audpply fifo, dsp enable dac0 fifo*/
    /*fm caputre no startup*/
    if(VBC_DAI_ID_FM_CAPTURE != dai->id) {
        ret = vbc_dsp_func_startup(dai->dev, dai->id,
                    substream->stream, dai->driver->name);
        if (ret < 0) {
            return -EIO;
        }
    }
	if (is_ap_vbc_ctl(dai->id, substream->stream)) {
		/* clear aud fifo */
		ap_vbc_fifo_clear(substream->stream);
		/* set aud fifo size */
		ap_vbc_set_fifo_size(substream->stream, VB_AUDRCD_FULL_WATERMARK, VB_AUDPLY_EMPTY_WATERMARK);
	}
	return 0;
}

static void vbc_shutdown(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	int ret;

	sp_asoc_pr_info("%s VBC(%s-%s)\n",__func__, dai->driver->name,
		(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ? "Capture" : "Playback");

	if (is_ap_vbc_ctl(dai->id, substream->stream)) {
		/* fifo disable */
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			ap_vbc_fifo_enable(0, substream->stream, VBC_ALL_CHAN);
		}
		/* clear aud fifo */
		ap_vbc_fifo_clear(substream->stream);
	}
	if(VBC_DAI_ID_VOICE_CAPTURE == dai->id){
		/*voic capture dma uid*/
		mcdt_adc_dma_disable(MCDT_CHAN_VOICE_CAPTURE);
	}
	if(VBC_DAI_ID_LOOP_RECORD == dai->id){
		mcdt_adc_dma_disable(MCDT_CHAN_LOOP);
	}
	if(VBC_DAI_ID_LOOP_PLAY == dai->id){
		mcdt_dac_dma_disable(MCDT_CHAN_LOOP);
	}
	if(VBC_DAI_ID_FAST_P== dai->id){
		mcdt_dac_dma_disable(MCDT_CHAN_FAST_PLAY);
	}
	if(VBC_DAI_ID_VOIP_RECORD == dai->id){
		mcdt_adc_dma_disable(MCDT_CHAN_VOIP);
	}
	if(VBC_DAI_ID_VOIP_PLAY == dai->id){
		mcdt_dac_dma_disable(MCDT_CHAN_VOIP);
	}

    if (VBC_DAI_ID_FM_CAPTURE != dai->id) {
        ret = vbc_dsp_func_shutdown(dai->dev, dai->id,
        substream->stream, dai->driver->name);
        if (ret < 0) {
            return;
        }
    }
}

static int vbc_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	int ret;
	int rate;
	struct sprd_pcm_dma_params *dma_data = NULL;

	sp_asoc_pr_info("%s VBC(%s-%s)\n",__func__, dai->driver->name,
		(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ? "Capture" : "Playback");

	dma_data = ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		? g_vbc[dai->id].dma_params[SND_PCM_PLAYBACK]
		: g_vbc[dai->id].dma_params[SND_PCM_CAPTURE]);
	if(VBC_DAI_ID_VOICE_CAPTURE == dai->id){
		/*voic capture dma uid*/
		vbc_pcm_voice_capture_mcdt.channels[0] =
		mcdt_adc_dma_enable(MCDT_CHAN_VOICE_CAPTURE, MCDT_FULL_WMK_VOICE_CAPTURE);
		//get_mcdt_adc_dma_uid(MCDT_CHAN_VOICE_CAPTURE, MCDT_AP_DMA_CHAN_VOICE_CAPTURE, MCDT_FULL_WMK_VOICE_CAPTURE);
		if(vbc_pcm_voice_capture_mcdt.channels[0] < 0){
			pr_err("%s vbc_pcm_voice_capture_mcdt.channels[0](dma device id) =%d\n",
				__func__, vbc_pcm_voice_capture_mcdt.channels[0]);
			return -EIO;
		}
		sp_asoc_pr_dbg("%s dai->id =%d, vbc_pcm_voice_capture_mcdt.channels[0](dma device id) =%d\n",
				__func__,dai->id, vbc_pcm_voice_capture_mcdt.channels[0]);
	}
	if(VBC_DAI_ID_LOOP_RECORD == dai->id){
		/*loop record dma uid*/
		pcm_loop_record_mcdt.channels[0] =
		mcdt_adc_dma_enable(MCDT_CHAN_LOOP, MCDT_FULL_WMK_LOOP);
		//get_mcdt_adc_dma_uid(MCDT_CHAN_LOOP, MCDT_AP_DMA_CHAN_LOOP, MCDT_FULL_WMK_LOOP);
		if(pcm_loop_record_mcdt.channels[0] < 0){
			pr_err("%s pcm_loop_record_mcdt.channels[0](dma device id) =%d\n",
				__func__, pcm_loop_record_mcdt.channels[0]);
			return -EIO;
		}
		sp_asoc_pr_dbg("%s dai->id =%d, pcm_loop_record_mcdt.channels[0](dma device id) =%d\n",
				__func__,dai->id, pcm_loop_record_mcdt.channels[0]);
	}
	if(VBC_DAI_ID_LOOP_PLAY == dai->id){
		/*loop play dma uid*/
		pcm_loop_play_mcdt.channels[0] =
		mcdt_dac_dma_enable(MCDT_CHAN_LOOP, MCDT_EMPTY_WMK_LOOP);
		//get_mcdt_dac_dma_uid(MCDT_CHAN_LOOP, MCDT_AP_DMA_CHAN_LOOP, MCDT_EMPTY_WMK_LOOP);
		if(pcm_loop_play_mcdt.channels[0] < 0){
			pr_err("%s pcm_loop_play_mcdt.channels[0](dma device id) =%d\n",
				__func__, pcm_loop_play_mcdt.channels[0]);
			return -EIO;
		}
		sp_asoc_pr_dbg("%s dai->id =%d, pcm_loop_play_mcdt.channels[0](dma device id) =%d\n",
				__func__,dai->id, pcm_loop_play_mcdt.channels[0]);
	}
	if(VBC_DAI_ID_FAST_P== dai->id){
		/*fast play dma uid*/
		pcm_fast_play_mcdt.channels[0] =
		mcdt_dac_dma_enable(MCDT_CHAN_FAST_PLAY, MCDT_EMPTY_WMK_FAST_PLAY);
		//get_mcdt_dac_dma_uid(MCDT_CHAN_FAST_PLAY, MCDT_AP_DMA_CHAN_FAST_PLAY, MCDT_EMPTY_WMK_FAST_PLAY);
		if(pcm_fast_play_mcdt.channels[0] < 0){
			pr_err("%s error pcm_fast_play_mcdt.channels[0](dma device id) =%d\n",
				__func__, pcm_fast_play_mcdt.channels[0]);
			return -EIO;
		}
		sp_asoc_pr_dbg("%s dai->id =%d, pcm_fast_play_mcdt.channels[0](dma device id) =%d\n",
				__func__,dai->id, pcm_fast_play_mcdt.channels[0]);
	}
	if(VBC_DAI_ID_VOIP_RECORD == dai->id){
		/*voip record dma uid*/
		pcm_voip_record_mcdt.channels[0] =
		mcdt_adc_dma_enable(MCDT_CHAN_VOIP, MCDT_FULL_WMK_VOIP);
		if(pcm_voip_record_mcdt.channels[0] < 0){
			pr_err("%s pcm_voip_record_mcdt.channels[0](dma device id) =%d\n",
				__func__, pcm_voip_record_mcdt.channels[0]);
			return -EIO;
		}
		sp_asoc_pr_dbg("%s dai->id =%d, pcm_voip_record_mcdt.channels[0](dma device id) =%d\n",
				__func__,dai->id, pcm_voip_record_mcdt.channels[0]);
	}
	if(VBC_DAI_ID_VOIP_PLAY == dai->id){
		/*voip play dma uid*/
		pcm_voip_play_mcdt.channels[0] =
		mcdt_dac_dma_enable(MCDT_CHAN_VOIP, MCDT_EMPTY_WMK_VOIP);
		if(pcm_voip_play_mcdt.channels[0] < 0){
			pr_err("%s pcm_voip_play_mcdt.channels[0](dma device id) =%d\n",
				__func__, pcm_voip_play_mcdt.channels[0]);
			return -EIO;
		}
		sp_asoc_pr_dbg("%s dai->id =%d, pcm_voip_play_mcdt.channels[0](dma device id) =%d\n",
				__func__,dai->id, pcm_voip_play_mcdt.channels[0]);
	}
	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		if(VBC_DAI_ID_VOICE_CAPTURE != dai->id){
			pr_err("%s, ERR:VBC Only Supports Format S16_LE\n", __func__);
		}
		break;
	}

	if (params_channels(params) > 2) {
		pr_err("%s, ERR:VBC Can NOT Supports Grate 2 Channels\n", __func__);
	}
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		g_vbc[dai->id].dac_used_chan_count = params_channels(params);	
	}
	else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE){
		g_vbc[dai->id].adc_used_chan_count = params_channels(params);
	}
	if (is_ap_vbc_ctl(dai->id, substream->stream)) {
		if(SNDRV_PCM_STREAM_PLAYBACK == substream->stream){
			/*offload mixer normal need in hw_params*/
			sp_asoc_pr_dbg("ap_vbc_fifo_enable 1 %s\n", __func__);
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_0,0);
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_1,0);
			ap_vbc_fifo_enable(1, substream->stream, VBC_ALL_CHAN);
		}
		/* set data format */
		ap_vbc_aud_dat_format_set(substream->stream, VBC_DAT_L16);
		/* open AP SRC */
		if (params_rate(params) != 48000) {
			ap_vbc_src_set(1, substream->stream, params_rate(params));
		}
	}
	rate = params_rate(params);
	if(VBC_DAI_ID_VOICE_CAPTURE != dai->id ||
        VBC_DAI_ID_FM_CAPTURE != dai->id){
		ret = vbc_dsp_func_hwparam(dai->dev, dai->id, substream->stream, dai->driver->name,
				params_channels(params), VBC_DAT_L16, dsp_vbc_get_src_mode(rate));
		if (ret < 0) {
			return -EIO;
		}
	}
	return 0;
}

static int vbc_hw_free(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int ret;
	sp_asoc_pr_info("%s VBC(%s-%s)\n",__func__, dai->driver->name,
		(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ? "Capture" : "Playback");
    if (VBC_DAI_ID_FM_CAPTURE != dai->id) {
        ret = vbc_dsp_func_hw_free(dai->dev, dai->id,
        substream->stream, dai->driver->name);
        if (ret < 0) {
            return -EIO;
        }
    }
	return 0;
}

static int vbc_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	int ret;
	int chan;

	sp_asoc_pr_info("%s VBC(%s-%s), cmd:%d \n",__func__, dai->driver->name,
		(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ? "Capture" : "Playback", cmd);
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (g_vbc[dai->id].dac_used_chan_count == 1) {
			chan = VBC_LEFT;
		} else if (g_vbc[dai->id].dac_used_chan_count == 2) {
			chan = VBC_ALL_CHAN;
		}
	}
	else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (g_vbc[dai->id].adc_used_chan_count == 1) {
			chan = VBC_LEFT;
		} else if (g_vbc[dai->id].adc_used_chan_count == 2) {
			chan = VBC_ALL_CHAN;
		}
	}
	if (is_ap_vbc_ctl(dai->id, substream->stream)) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			/* fifo enable */
			if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				ap_vbc_fifo_enable(1, substream->stream, VBC_ALL_CHAN);
			}
			/* vbc dma enable */
			ap_vbc_aud_dma_chn_en(1, substream->stream, chan);
		break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			/* fifo disable */
			if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				ap_vbc_fifo_enable(0, substream->stream, VBC_ALL_CHAN);
			}
			/* vbc dma disable */
			ap_vbc_aud_dma_chn_en(0, substream->stream, chan);
		break;
		}
	}
    if (VBC_DAI_ID_FM_CAPTURE != dai->id) {
        ret = vbc_dsp_func_trigger(dai->dev, dai->id,
        substream->stream, dai->driver->name, cmd);
        if (ret < 0) {
            return -EIO;
        }
    }
	return 0;
}

static struct snd_soc_dai_ops vbc_dai_ops = {
	.startup = vbc_startup,
	.shutdown = vbc_shutdown,
	.hw_params = vbc_hw_params,
	.trigger = vbc_trigger,
	.hw_free = vbc_hw_free,
};

static struct snd_soc_dai_driver vbc_dai[] = {
	{
	 .name = "normal-without-dsp",
	 .id = VBC_DAI_ID_NORMAL_OUTDSP,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "normal-with-dsp",
	 .id = VBC_DAI_ID_NORMAL_WITHDSP,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	{
	 .name = "fast-playback",
	 .id = VBC_DAI_ID_FAST_P,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "offload",
	 .id = VBC_DAI_ID_OFFLOAD,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	.capture = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "voice",
	 .id = VBC_DAI_ID_VOICE,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "voip_record",
	 .id = VBC_DAI_ID_VOIP_RECORD,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "voip_play",
	 .id = VBC_DAI_ID_VOIP_PLAY,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "fm",
	 .id = VBC_DAI_ID_FM,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "fm-c-withdsp",
	 .id = VBC_DAI_ID_FM_C_WITHDSP,
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "voice-capture",
	 .id = VBC_DAI_ID_VOICE_CAPTURE,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 #if 1
	 {
	 .name = "null-cpu",
	 .id = -1,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 },
	 #endif
	 {
	 .name = "loop_record",
	 .id = VBC_DAI_ID_LOOP_RECORD,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "loop_play",
	 .id = VBC_DAI_ID_LOOP_PLAY,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 {
	 .name = "fm_capture",
	 .id = VBC_DAI_ID_FM_CAPTURE,
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 #if 0
	 {
	 .name = "btcall",
	 .id = VBC_DAI_ID_BT_CALL,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	 #endif
};

static const struct snd_soc_component_driver sprd_vbc_component = {
	.name = "vbc",
};

static int vbc_drv_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;
	struct audio_sipc_of_data sipc_info;

	sp_asoc_pr_dbg("%s\n", __func__);

	if (node) {
		ret = vbc_of_setup(node, &sipc_info, &pdev->dev);
		if (ret < 0) {
			pr_err("%s: failed to setup vbc dt, ret=%d\n", __func__, ret);
			return -ENODEV;
		}
	}
	ret = snd_vbc_sipc_init(&sipc_info);
	if (ret < 0) {
		pr_err("%s: failed to init vbc-sipc, ret=%d\n", __func__, ret);
		return -ENODEV;
	}
	sprd_dma_config_init(pdev);

	/* 1. probe CODEC */
	ret = sprd_vbc_codec_probe(pdev);

	if (ret < 0) {
		goto probe_err;
	}

	/* 2. probe DAIS */
	ret =
	    snd_soc_register_component(&pdev->dev, &sprd_vbc_component, vbc_dai,
				       ARRAY_SIZE(vbc_dai));

	if (ret < 0) {
		pr_err("%s, Register VBC to DAIS Failed!\n", __func__);
		goto probe_err;
	}

	return ret;
probe_err:
	snd_vbc_sipc_deinit();
	pr_err("%s, error return %i\n", __func__, ret);

	return ret;
}

static int vbc_drv_remove(struct platform_device *pdev)
{
	snd_vbc_sipc_deinit();
	snd_soc_unregister_component(&pdev->dev);

	sprd_vbc_codec_remove(pdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vbc_of_match[] = {
	{.compatible = "sprd,vbc-r3p0",},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_of_match);
#endif

static struct platform_driver vbc_driver = {
	.driver = {
		   .name = "vbc-r3p0",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(vbc_of_match),
		   },

	.probe = vbc_drv_probe,
	.remove = vbc_drv_remove,
};

static int __init sprd_vbc_driver_init(void)
{
	return platform_driver_register(&vbc_driver);
}

late_initcall(sprd_vbc_driver_init);


MODULE_DESCRIPTION("SPRD ASoC VBC CPU-DAI driver");
MODULE_AUTHOR("Jian chen <jian.chen@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cpu-dai:vbc-r3p0");
