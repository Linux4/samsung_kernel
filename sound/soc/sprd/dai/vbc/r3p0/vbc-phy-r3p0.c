/*
 * sound/soc/sprd/dai/vbc/r3p0/vbc-phy-r3p0.c
 *
 * SPRD SoC VBC -- SpreadTrum SOC for VBC driver function.
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
#include <linux/kernel.h>
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
#include <linux/of.h>
#include <linux/io.h>
#include <sound/soc.h>
#include <sound/audio-sipc.h>

#include "sprd-asoc-common.h"
#include "vbc-phy-r3p0.h"
#include "vbc-codec.h"

unsigned long sprd_ap_vbc_virt_base = 0;
unsigned int sprd_ap_vbc_phy_base = 0;
/* vbc local power suppliy and chan on */
static struct vbc_refcount {
	atomic_t vbc_power_on;	/*vbc reg power enable */
	atomic_t vbc_on;	/*vbc enable */
	atomic_t chan_on[2][SND_PCM_DIRECTION_MAX+1];
} vbc_refcnt;

static DEFINE_SPINLOCK(vbc_lock);
extern struct sprd_vbc_priv g_vbc[];
extern int vbc_sipc_parse_dt(struct audio_sipc_of_data *info, struct device *dev);

/* ap vbc phy driver start */
static void ap_vbc_phy_fifo_enable_raw(int enable, int stream, int chan)
{
	sp_asoc_pr_dbg("%s %d, enable=%d, chan=%d\n", __func__, __LINE__, enable, chan);
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (chan == VBC_LEFT) {
			ap_vbc_reg_update(VBC_AUD_EN, ((enable ? 1 : 0) << (AUDPLY_AP_FIFO0_EN)),
			       (1 << AUDPLY_AP_FIFO0_EN));
		} else if (chan == VBC_RIGHT) {
			ap_vbc_reg_update(VBC_AUD_EN, ((enable ? 1 : 0) << (AUDPLY_AP_FIFO1_EN)),
			       (1 << AUDPLY_AP_FIFO1_EN));
		} else if (chan == VBC_ALL_CHAN) {
			ap_vbc_reg_update(VBC_AUD_EN, ((enable ? 1 : 0) << (AUDPLY_AP_FIFO0_EN)),
			       (1 << AUDPLY_AP_FIFO0_EN));
			ap_vbc_reg_update(VBC_AUD_EN, ((enable ? 1 : 0) << (AUDPLY_AP_FIFO1_EN)),
			       (1 << AUDPLY_AP_FIFO1_EN));
		}
	}
}

static void ap_vbc_phy_fifo_clear(int stream)
{
	int shift = 0;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		shift = AUDPLY_AP_FIFO_CLR;
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		shift = AUDRCD_AP_FIFO_CLR;
	} else {
		pr_err("%s, wrong stream(%d) \n", __func__, stream);
		return;
	}
	ap_vbc_reg_update(VBC_AUD_CLR, (1 << shift),
			       (1 << shift));
}

static void ap_vbc_phy_reg_raw_write(unsigned int reg, int val)
{
	__raw_writel(val, (void *__iomem)(reg - VBC_OFLD_ADDR + sprd_ap_vbc_virt_base));

}

static void dsp_vbc_phy_reg_raw_write(unsigned int reg, int val)
{
	int ret = 0;
	struct sprd_vbc_kcontrol vbc_reg = {0};
	vbc_reg.reg = reg;
	vbc_reg.value = val;
	vbc_reg.mask = 0xFFFFFFFF;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_REG, -1, SND_VBC_DSP_IO_KCTL_SET,
		&vbc_reg, sizeof(struct sprd_vbc_kcontrol), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
}

static void vbc_phy_audply_dma_chn_en(int en, int chan)
{
	if (chan == VBC_LEFT) {
		ap_vbc_reg_update(VBC_AUD_EN, (en << AUDPLY_DMA_DA0_EN),
			       (1 << AUDPLY_DMA_DA0_EN));
	} else if (chan == VBC_RIGHT) {
		ap_vbc_reg_update(VBC_AUD_EN, (en << AUDPLY_DMA_DA1_EN),
			       (1 << AUDPLY_DMA_DA1_EN));
	} else if (chan == VBC_ALL_CHAN) {
		ap_vbc_reg_update(VBC_AUD_EN, (en << AUDPLY_DMA_DA0_EN),
			       (1 << AUDPLY_DMA_DA0_EN));
		ap_vbc_reg_update(VBC_AUD_EN, (en << AUDPLY_DMA_DA1_EN),
			       (1 << AUDPLY_DMA_DA1_EN));
	}
}

static void vbc_phy_audrcd_dma_chn_en(int en, int chan)
{
	if (chan == VBC_LEFT) {
		ap_vbc_reg_update(VBC_AUD_EN, (en << AUDRCD_DMA_AD0_EN),
			       (1 << AUDRCD_DMA_AD0_EN));
	} else if (chan == VBC_RIGHT) {
		ap_vbc_reg_update(VBC_AUD_EN, (en << AUDRCD_DMA_AD1_EN),
			       (1 << AUDRCD_DMA_AD1_EN));
	} else if (chan == VBC_ALL_CHAN) {
		ap_vbc_reg_update(VBC_AUD_EN, (en << AUDRCD_DMA_AD0_EN),
			       (1 << AUDRCD_DMA_AD0_EN));
		ap_vbc_reg_update(VBC_AUD_EN, (en << AUDRCD_DMA_AD1_EN),
			       (1 << AUDRCD_DMA_AD1_EN));
	}
}

static void vbc_phy_audply_src0_en(int en)
{
	ap_vbc_reg_update(VBC_AUD_EN, (en << AUDPLY_SRC_EN_0),
			       (1 << AUDPLY_SRC_EN_0));
}

static void vbc_phy_audply_src1_en(int en)
{
	ap_vbc_reg_update(VBC_AUD_EN, (en << AUDPLY_SRC_EN_1),
			       (1 << AUDPLY_SRC_EN_1));
}

static void vbc_phy_audrcd_src0_en(int en)
{
	ap_vbc_reg_update(VBC_AUD_EN, (en << AUDRCD_SRC_EN_0),
			       (1 << AUDRCD_SRC_EN_0));
}

static void vbc_phy_audrcd_src1_en(int en)
{
	ap_vbc_reg_update(VBC_AUD_EN, (en << AUDRCD_SRC_EN_1),
			       (1 << AUDRCD_SRC_EN_1));
}

static void vbc_phy_audply_set_src_mode(AUD_AP_SRC_MODE_E mode)
{
	ap_vbc_reg_update(VBC_AUD_SRC_CTRL, ((mode << AUDPLY_AP_SRC_MODE) & (AUDPLY_AP_SRC_MODE_MASK)),
					(AUDPLY_AP_SRC_MODE_MASK << AUDPLY_AP_SRC_MODE));
}

static void vbc_phy_audrcd_set_src_mode(AUD_AP_SRC_MODE_E mode)
{
	ap_vbc_reg_update(VBC_AUD_SRC_CTRL, ((mode << AUDRCD_AP_SRC_MODE) & (AUDRCD_AP_SRC_MODE_MASK)),
					(AUDRCD_AP_SRC_MODE_MASK << AUDRCD_AP_SRC_MODE));
}

static void vbc_phy_audply_set_watermark(int full, int empty)
{
	ap_vbc_reg_update(VBC_AUDPLY_FIFO_CTRL, (full << AUDPLY_AP_FIFO_FULL_LVL) | (empty << AUDPLY_AP_FIFO_EMPTY_LVL),
			     (AUDPLY_AP_FIFO_FULL_LVL_MASK << AUDPLY_AP_FIFO_FULL_LVL) | (AUDPLY_AP_FIFO_EMPTY_LVL_MASK << AUDPLY_AP_FIFO_EMPTY_LVL));
}

static void vbc_phy_audrcd_set_watermark(int full, int empty)
{
	ap_vbc_reg_update(VBC_AUDRCD_FIFO_CTRL, (full << AUDRCD_AP_FIFO_FULL_LVL) | (empty << AUDRCD_AP_FIFO_EMPTY_LVL),
			     (AUDRCD_AP_FIFO_FULL_LVL_MASK << AUDRCD_AP_FIFO_FULL_LVL) | (AUDRCD_AP_FIFO_EMPTY_LVL_MASK << AUDRCD_AP_FIFO_EMPTY_LVL));
}

static void to_stream_info(int id, int stream,
		const char *name, struct snd_pcm_stream_info *stream_info)
{
	stream_info->id = id;
	strcpy(stream_info->name, name);
	stream_info->stream = stream;
}

int vbc_of_setup(struct device_node *node, struct audio_sipc_of_data *info, struct device *dev)
{
	int ret = 0;
	unsigned int val_arr[2] = {0};
	if (node) {
		if (!of_property_read_u32_array(node, "sprd,reg_ap_vbc", &val_arr[0], 2)) {
			sprd_ap_vbc_virt_base = ioremap_nocache(val_arr[0], val_arr[1]);
			if(!sprd_ap_vbc_virt_base) {
				pr_err("ERR: cannot create iomap address for AP-VBC!\n");
				return -EINVAL;
			}
			sprd_ap_vbc_phy_base = val_arr[0];
		} else {
			pr_err("ERR:Must give me the AP-VBC reg address!\n");
			return -EINVAL;
		}
		ret = vbc_sipc_parse_dt(info, dev);
		if (ret < 0) {
			pr_err("%s: failed to parse sipc dt, ret=%d\n", __func__, ret);
			return -ENODEV;
		}
	}
	return 0;
}

int dsp_vbc_get_src_mode(int rate)
{
	int mode;
	switch (rate) {
	case 48000:
		mode = AUD_AP_SRC_MODE_48000;
		break;
	case 44100:
		mode = AUD_AP_SRC_MODE_44100;
		break;
	case 32000:
		mode = AUD_AP_SRC_MODE_32000;
		break;
	case 24000:
		mode = AUD_AP_SRC_MODE_24000;
		break;
	case 22050:
		mode = AUD_AP_SRC_MODE_22050;
		break;
	case 16000:
		mode = AUD_AP_SRC_MODE_16000;
		break;
	case 12000:
		mode = AUD_AP_SRC_MODE_12000;
		break;
	case 11025:
		mode = AUD_AP_SRC_MODE_11025;
		break;
	case 8000:
		mode = AUD_AP_SRC_MODE_8000;
		break;
	default:
		sp_asoc_pr_info("%s, not supported samplerate (%d), set default \n",
				__func__, rate);
		mode = AUD_AP_SRC_MODE_48000;
		break;
	}
	return mode;
}

/*
 * Returns 1 for change, 0 for no change, or negative error code.
 */
int ap_vbc_reg_update(unsigned int reg, int val, int mask)
{
	unsigned int new, old;
	spin_lock(&vbc_lock);
	old = ap_vbc_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	ap_vbc_phy_reg_raw_write(reg, new);
	spin_unlock(&vbc_lock);
	sp_asoc_pr_reg("[0x%04x] U:[0x%08x] R:[0x%08x]\n", reg & 0xFFFFFFFF, new,
		       ap_vbc_reg_read(reg));
	return old != new;
}

int ap_vbc_reg_write(unsigned int reg, int val)
{
	spin_lock(&vbc_lock);
	ap_vbc_phy_reg_raw_write(reg, val);
	spin_unlock(&vbc_lock);
	sp_asoc_pr_reg("AP-VBC:[0x%04x] W:[0x%08x] R:[0x%08x]\n", (reg) & 0xFFFFFFFF, val,
		       ap_vbc_reg_read(reg));
	return 0;
}

unsigned int dsp_vbc_reg_read(unsigned int reg)
{
	int ret = 0;
	uint32_t val = 0;
	struct sprd_vbc_kcontrol vbc_reg = {0};
	vbc_reg.reg = reg;
	vbc_reg.mask = 0xFFFFFFFF;
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_REG, -1, SND_VBC_DSP_IO_KCTL_GET,
		&vbc_reg, sizeof(struct sprd_vbc_kcontrol), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	} else {
	#if 0
		ret = snd_audio_recv_cmd(SMSG_CH_VBC_CTL, SND_VBC_DSP_IO_KCTL_GET, &val, AUDIO_SIPC_WAIT_FOREVER);
		if (ret < 0) {
			pr_err("%s, Failed to get, ret: %d\n", __func__, ret);
		}
    #endif
	}
	return vbc_reg.value;
}

int dsp_vbc_reg_write(unsigned int reg, int val)
{
	spin_lock(&vbc_lock);
	dsp_vbc_phy_reg_raw_write(reg, val);
	spin_unlock(&vbc_lock);
	sp_asoc_pr_reg("%s DSP-VBC:[0x%04x] W:[0x%08x] R:[0x%08x]\n", __func__, (reg) & 0xFFFFFFFF, val,
		       dsp_vbc_reg_read(reg));
	return 0;
}

unsigned int ap_vbc_reg_read(unsigned int reg)
{
	unsigned int ret = 0;
	ret = __raw_readl((void *__iomem)(reg - VBC_OFLD_ADDR + sprd_ap_vbc_virt_base));
	return ret;
}

void ap_vbc_src_clear(int stream)
{
	int shift = 0;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		shift = AUDPLY_AP_SRC_CLR;
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		shift = AUDRCD_AP_SRC_CLR;
	} else {
		pr_err("%s, wrong stream(%d) \n", __func__, stream);
		return;
	}
	ap_vbc_reg_update(VBC_AUD_CLR, (1 << shift),
			       (1 << shift));
}

void ap_vbc_aud_int_en(int stream, int en)
{
	int shift = 0;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		shift = AUDPLY_AP_INT_EN;
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		shift = AUDRCD_AP_INT_EN;
	} else {
		pr_err("%s, wrong stream(%d) \n", __func__, stream);
		return;
	}
	ap_vbc_reg_update(VBC_AUD_INT_EN, (1 << shift),
			       (1 << shift));
}

int ap_vbc_fifo_enable(int enable, int stream, int chan)
{
	atomic_t *chan_on = &vbc_refcnt.chan_on[stream][chan];
	sp_asoc_pr_dbg("ap_vbc_fifo_enable:enable %d, stream %d,chan %d,chan_on %d",enable, stream,chan,atomic_read(chan_on));
	if (enable) {
		#if 0
		atomic_inc(chan_on);
		if (atomic_read(chan_on) == 1) {
		#endif
			ap_vbc_phy_fifo_enable_raw(1, stream, chan);
			sp_asoc_pr_dbg(" %s, %s-%s On\n", __func__, vbc_get_stream_name(stream),
					vbc_get_chan_name(chan));
		#if 0
		}
		#endif
	} else {
		#if 0
		if (atomic_dec_and_test(chan_on)) {
		#endif
			ap_vbc_phy_fifo_enable_raw(0, stream, chan);
			sp_asoc_pr_dbg("%s, %s-%s Off\n", __func__, vbc_get_stream_name(stream),
					vbc_get_chan_name(chan));
		#if 0
		}
		#endif
		if (atomic_read(chan_on) < 0) {
			atomic_set(chan_on, 0);
		}
	}
	sp_asoc_pr_dbg("%s-%s REF: %d", vbc_get_stream_name(stream), vbc_get_chan_name(chan),
		       atomic_read(chan_on));
	return 0;
}

void ap_vbc_fifo_clear(int stream)
{
	ap_vbc_phy_fifo_clear(stream);	/* clear audrcd data fifo */
}

void ap_vbc_aud_dat_format_set(int stream, VBC_DAT_FORMAT dat_fmt)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ap_vbc_reg_update(VBC_AUDPLY_FIFO_CTRL, (dat_fmt << AUDPLY_AP_DAT_FMT_CTL),
			       (AUDPLY_AP_DAT_FMT_CTL_MASK << AUDPLY_AP_DAT_FMT_CTL));
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		ap_vbc_reg_update(VBC_AUDRCD_FIFO_CTRL, (dat_fmt << AUDRCD_AP_DAT_FMT_CTL),
			       (AUDRCD_AP_DAT_FMT_CTL_MASK << AUDRCD_AP_DAT_FMT_CTL));
	}
}

void ap_vbc_aud_dma_chn_en(int enable, int stream, int chan)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		vbc_phy_audply_dma_chn_en(enable, chan);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		vbc_phy_audrcd_dma_chn_en(enable, chan);
	}
}

int ap_vbc_set_fifo_size(int stream, int full, int empty)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		vbc_phy_audply_set_watermark(full, empty);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		vbc_phy_audrcd_set_watermark(full, empty);
	}
	return 0;
}

void ap_vbc_src_set(int en, int stream, int rate)
{
	int mode = dsp_vbc_get_src_mode(rate);
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		vbc_phy_audply_set_src_mode(mode);
		vbc_phy_audply_src0_en(en ? 1:0);
		vbc_phy_audply_src1_en(en ? 1:0);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		vbc_phy_audrcd_set_src_mode(mode);
		vbc_phy_audrcd_src0_en(en ? 1:0);
		vbc_phy_audrcd_src1_en(en ? 1:0);
	}
}

/* ap vbc phy driver end */

/* dsp vbc phy driver start */
int dsp_vbc_mdg_set(int id, int enable, int mdg_step)
{
	int ret = 0;
	struct vbc_mute_dg_para mdg_para = {0};
	mdg_para.mdg_id = id;
	mdg_para.mdg_mute = enable;
	mdg_para.mdg_step = mdg_step;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_MDG, -1, SND_VBC_DSP_IO_KCTL_SET,
		&mdg_para, sizeof(struct vbc_mute_dg_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_dg_set(int id, int dg_l, int dg_r)
{
	int ret = 0;
	struct vbc_dg_para dg_para = {0};
	dg_para.dg_id = id;
	dg_para.dg_left= dg_l;
	dg_para.dg_right = dg_r;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_DG, -1, SND_VBC_DSP_IO_KCTL_SET,
		&dg_para, sizeof(struct vbc_dg_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_offload_dg_set(int id, int dg_l, int dg_r)
{
	int ret = 0;
	struct vbc_dg_para dg_para = {0};
	dg_para.dg_id = id;
	dg_para.dg_left = dg_l;
	dg_para.dg_right = dg_r;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_MP3_OFFLOAD, SND_KCTL_TYPE_DG, -1, SND_VBC_DSP_IO_KCTL_SET,
		&dg_para, sizeof(struct vbc_dg_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}


int dsp_vbc_smthdg_set(int id, int smthdg_l, int smthdg_r)
{
	int ret = 0;
	struct vbc_smthdg_para smthdg_para = {0};
	smthdg_para.smthdg_id = id;
	smthdg_para.smthdg_left= smthdg_l;
	smthdg_para.smthdg_right = smthdg_r;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_SMTHDG, -1, SND_VBC_DSP_IO_KCTL_SET,
		&smthdg_para, sizeof(struct vbc_smthdg_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_smthdg_step_set(int id, int smthdg_step)
{
	int ret = 0;
	struct vbc_smthdg_step_para smthdg_step_para = {0};
	smthdg_step_para.smthdg_id = id;
	smthdg_step_para.smthdg_step = smthdg_step;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_SMTHDG_STEP, -1, SND_VBC_DSP_IO_KCTL_SET,
		&smthdg_step_para, sizeof(struct vbc_smthdg_step_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_mixerdg_step_set(int mixerdg_step)
{
	int ret = 0;
	int16_t mixerdg_step_para = 0;
	mixerdg_step_para = mixerdg_step;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_MIXERDG_STEP, -1, SND_VBC_DSP_IO_KCTL_SET,
		&mixerdg_step_para, sizeof(int16_t), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_mixerdg_mainpath_set(int id, int mixerdg_main_l, int mixerdg_main_r)
{
	int ret = 0;
	struct vbc_mixerdg_mainpath_para mixerdg_mainpath_para = {0};
	mixerdg_mainpath_para.mixerdg_id = id;
	mixerdg_mainpath_para.mixerdg_main_left= mixerdg_main_l;
	mixerdg_mainpath_para.mixerdg_main_right = mixerdg_main_r;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_MIXERDG_MAIN, -1, SND_VBC_DSP_IO_KCTL_SET,
		&mixerdg_mainpath_para, sizeof(struct vbc_mixerdg_mainpath_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_mixerdg_mixpath_set(int id, int mixerdg_mix_l, int mixerdg_mix_r)
{
	int ret = 0;
	struct vbc_mixerdg_mixpath_para mixerdg_mixpath_para = {0};
	mixerdg_mixpath_para.mixerdg_id = id;
	mixerdg_mixpath_para.mixerdg_mix_left= mixerdg_mix_l;
	mixerdg_mixpath_para.mixerdg_mix_right = mixerdg_mix_r;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_MIXERDG_MIX, -1, SND_VBC_DSP_IO_KCTL_SET,
		&mixerdg_mixpath_para, sizeof(struct vbc_mixerdg_mixpath_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_mux_set(int id, int mux_sel)
{
	int ret = 0;
	struct vbc_mux_para mux_para = {0};
	mux_para.mux_id = id;
	mux_para.mux_sel = mux_sel;
    sp_asoc_pr_dbg("%s mux_id = %d mux_sel=%d\n", __func__, mux_para.mux_id, mux_para.mux_sel);
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX, -1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_adder_set(int id, int adder_mode_l, int adder_mode_r)
{
	int ret = 0;
	struct vbc_adder_para adder_para = {0};
	adder_para.adder_id = id;
	adder_para.adder_mode_l = adder_mode_l;
	adder_para.adder_mode_r = adder_mode_r;
	sp_asoc_pr_dbg("%s, adder_para.adder_mode_l=%d, adder_para.adder_mode_r=%d\n",__func__, adder_para.adder_mode_l, adder_para.adder_mode_r);
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_ADDER, -1, SND_VBC_DSP_IO_KCTL_SET,
		&adder_para, sizeof(struct vbc_adder_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}
/* dsp vbc phy driver end */

/* vbc driver function start */
int vbc_dsp_func_startup(struct device *dev, int id,
			int stream, const char *name)
{
	int ret;
	struct sprd_vbc_stream_startup_shutdown startup_info = {0};
	/* fill the cmd para */
	to_stream_info(id, stream, name, &startup_info.stream_info);
	vbc_codec_getpathinfo(dev, &startup_info.startup_para, &g_vbc[id]);
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, id, stream, SND_VBC_DSP_FUNC_STARTUP,
		&startup_info, sizeof(struct sprd_vbc_stream_startup_shutdown), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		return -EIO;
	}
	return 0;
}

int vbc_dsp_func_shutdown(struct device *dev, int id,
			int stream, const char *name)
{
	int ret;
	struct sprd_vbc_stream_startup_shutdown shutdown_info = {0};
	/* fill the cmd para */
	to_stream_info(id, stream, name, &shutdown_info.stream_info);
	vbc_codec_getpathinfo(dev, &shutdown_info.startup_para, &g_vbc[id]);
	/* send audio cmd */
	sp_asoc_pr_dbg("shutdown_info.startup_para.dac_id=%d\n",shutdown_info.startup_para.dac_id);
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, id, stream, SND_VBC_DSP_FUNC_SHUTDOWN,
		&shutdown_info, sizeof(struct sprd_vbc_stream_startup_shutdown), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		return -EIO;
	}
	return 0;
}

int vbc_dsp_func_hwparam(struct device *dev, int id, int stream,
			const char *name, unsigned int channels, unsigned int format, unsigned int rate)
{
	int ret;
	struct sprd_vbc_stream_hw_paras stream_param = {0};
	/* fill the cmd para */
	to_stream_info(id, stream, name, &stream_param.stream_info);
	stream_param.hw_params_info.channels = channels;
	stream_param.hw_params_info.format = format;
	stream_param.hw_params_info.rate = rate;
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, id, stream, SND_VBC_DSP_FUNC_HW_PARAMS,
		&stream_param, sizeof(struct sprd_vbc_stream_hw_paras), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		return -EIO;
	}
	return 0;
}

int vbc_dsp_func_hw_free(struct device *dev, int id,
			int stream, const char *name)
{
	int ret;
	struct snd_pcm_stream_info stream_info = {0};
	/* fill the cmd para */
	to_stream_info(id, stream, name, &stream_info);
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, id, stream, SND_VBC_DSP_FUNC_HW_FREE,
		&stream_info, sizeof(struct snd_pcm_stream_info), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		return -EIO;
	}
	return 0;
}

int vbc_dsp_func_trigger(struct device *dev, int id, int stream,
			const char *name, int cmd)
{
	int ret;
	struct sprd_vbc_stream_trigger stream_trigger = {0};
	/* fill the cmd para */
	to_stream_info(id, stream, name, &stream_trigger.stream_info);
	stream_trigger.pcm_trigger_cmd = cmd;
	/* send audio cmd */
	ret = snd_audio_send_cmd_norecev(SMSG_CH_VBC_CTL, id, stream, SND_VBC_DSP_FUNC_HW_TRIGGER,
		&stream_trigger, sizeof(struct sprd_vbc_stream_trigger), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		return -EIO;
	}
	return 0;
}

int dsp_vbc_src_set(int id, int enable, VBC_SRC_MODE_E src_mode)
{
	int ret = 0;
	struct vbc_src_para src_para = {0};
	src_para.enable = enable;
	src_para.src_id = id;
	src_para.src_mode = src_mode;
	sp_asoc_pr_dbg("%s, id=%d,enable=%d, src_mode=%d\n", __func__, id, enable, src_mode);
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_SRC, -1, SND_VBC_DSP_IO_KCTL_SET,
		&src_para, sizeof(struct vbc_src_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_bt_call_set(int id, int enable, VBC_SRC_MODE_E src_mode)
{
	int ret = 0;
	struct vbc_bt_call_para bt_call_para = {0};
	/*id not used*/
	bt_call_para.enable = enable;
	bt_call_para.src_mode = src_mode;
	sp_asoc_pr_dbg("%s,enable=%d, src_mode=%d\n", __func__, enable, src_mode);
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_BT_CALL, -1, SND_VBC_DSP_IO_KCTL_SET,
		&bt_call_para, sizeof(struct vbc_bt_call_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_set_volume(struct snd_soc_codec *codec){
	int value = 0;
	int ret = 0;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	value = vbc_codec->volume;
	sp_asoc_pr_dbg("%s value = %d\n", __func__, value);
	/*dsp will get it from parameter1 other than iram;*/
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_VOLUME, value, SND_VBC_DSP_IO_KCTL_SET,
		&value, sizeof(value), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_loopback_set(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_loopback_para loopback = {0};
	loopback.loopback_para = vbc_codec->loopback_type;
	loopback.voice_fmt     = vbc_codec->voice_fmt;
	loopback.amr_rate      = vbc_codec->arm_rate;
	sp_asoc_pr_dbg("%s loopback_type = %d,loopback.voice_fmt =%d, loopback.amr_rate  =%d\n", __func__, loopback.loopback_para, loopback.voice_fmt, loopback.amr_rate  );
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_LOOPBACK_TYPE, -1, SND_VBC_DSP_IO_KCTL_SET,
		&loopback, sizeof(struct vbc_loopback_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}

int dsp_vbc_dp_en_set(struct snd_soc_codec *codec, int id)
{
	int ret = 0;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
    struct vbc_dp_en_para dp_en = {0};

    dp_en.id = id;
    dp_en.enable = vbc_codec->vbc_dp_en[id];
    sp_asoc_pr_dbg("%s dp_en.id=%d dp_en.enable=%hu\n", __func__, dp_en.id, dp_en.enable);
	/* send audio cmd */
	ret = snd_audio_send_cmd(SMSG_CH_VBC_CTL, SND_KCTL_TYPE_DATAPATH, -1, SND_VBC_DSP_IO_KCTL_SET,
		&dp_en, sizeof(struct vbc_dp_en_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	}
	return 0;
}
/* vbc driver function end */

