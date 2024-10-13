/*
 * sound/soc/sprd/dai/i2s/i2s.c
 *
 * SPRD SoC CPU-DAI -- SpreadTrum SOC DAI i2s.
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
#define pr_fmt(fmt) pr_sprd_fmt(" I2S ") fmt

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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include <mach/hardware.h>

#include "sprd-asoc-common.h"
#include "sprd-pcm.h"
#include <mach/i2s.h>

/* register offset */
#define IIS_TXD			(0x0000)
#define IIS_CLKD		(0x0004)
#define IIS_CTRL0		(0x0008)
#define IIS_CTRL1		(0x000C)
#define IIS_CTRL2		(0x0010)
#define IIS_CTRL3		(0x0014)
#define IIS_INT_IEN		(0x0018)
#define IIS_INT_CLR		(0x001C)
#define IIS_INT_RAW		(0x0020)
#define IIS_INT_STS		(0x0024)
#define IIS_STS1		(0x0028)
#define IIS_STS2		(0x002C)
#define IIS_STS3		(0x0030)
#define IIS_DSPWAIT		(0x0034)
#define IIS_CTRL4		(0x0038)
#define IIS_STS4		(0x003C)

#define I2S_REG(i2s, offset) ((unsigned int)((i2s)->membase + (offset)))
#define I2S_PHY_REG(i2s, offset) (((unsigned int)(i2s)->memphys + (offset)))

struct i2s_rtx {
	int dma_no;
};
#define DAI_NAME_SIZE 20
struct i2s_priv {
	int id;
	int hw_port;
	struct device *dev;
	struct list_head list;
	struct i2s_rtx rx;
	struct i2s_rtx tx;
	atomic_t open_cnt;
	char dai_name[DAI_NAME_SIZE];
	int irq_no;
	void __iomem *membase;
	unsigned int *memphys;
	struct i2s_config config;
	struct clk *i2s_clk;
	struct snd_soc_dai_driver i2s_dai_driver[2];
};

static struct sprd_pcm_dma_params i2s_pcm_stereo_out = {
	.name = "I2S PCM Stereo out",
	.irq_type = BLK_DONE,
	.desc = {
		 .datawidth = WORD_WIDTH,
		 .src_step = 4,
		 .des_step = 0,
		 },
};

static struct sprd_pcm_dma_params i2s_pcm_stereo_in = {
	.name = "I2S PCM Stereo in",
	.irq_type = BLK_DONE,
	.desc = {
		 .datawidth = WORD_WIDTH,
		 .src_step = 0,
		 .des_step = 4,
		 },
};

/* default pcm config */
const static struct i2s_config def_pcm_config = {
	.hw_port = 0,
	.fs = 8000,
	.slave_timeout = 0xF11,
	.bus_type = PCM_BUS,
	.byte_per_chan = I2S_BPCH_16,
	.mode = I2S_MASTER,
	.lsb = I2S_LSB,
	.rtx_mode = I2S_RTX_MODE,
	.sync_mode = I2S_LRCK,
	.lrck_inv = I2S_L_LEFT,
	.clk_inv = I2S_CLK_N,
	.pcm_bus_mode = I2S_SHORT_FRAME,
	.pcm_slot = 0x1,
	.pcm_cycle = 1,
	.tx_watermark = 12,
	.rx_watermark = 20,
};

#if 1
/* default i2s config */
static struct i2s_config def_i2s_config = {
		 .hw_port = 0,
		 .fs = 8000,
		 .slave_timeout = 0xF11,
		 .bus_type = I2S_BUS,
		 .byte_per_chan = I2S_BPCH_16,
		 .mode = I2S_MASTER,
		 .lsb = I2S_MSB,
		 .rtx_mode = I2S_RTX_MODE,
		 .sync_mode = I2S_LRCK,
		 .lrck_inv = I2S_CLK_N,
		 .clk_inv = I2S_CLK_R,
		 .i2s_bus_mode = I2S_COMPATIBLE,
		 .tx_watermark = 12,
		 .rx_watermark = 20,
 };
#else
static struct i2s_config def_i2s_config = {
        .hw_port = 0,
        .fs = 8000,
        .slave_timeout = 0xF11,
        .bus_type = I2S_BUS,
        .byte_per_chan = I2S_BPCH_16,
        .mode = I2S_SLAVE,
        .lsb = I2S_MSB,
        .rtx_mode = I2S_RX_MODE,
        .sync_mode = I2S_LRCK,
        .lrck_inv = I2S_L_LEFT,
        .clk_inv = I2S_CLK_N,
        .i2s_bus_mode = I2S_COMPATIBLE,
        .tx_watermark = 12,
        .rx_watermark = 20,
};
#endif


static DEFINE_SPINLOCK(i2s_lock);

static inline int i2s_reg_read(unsigned int reg)
{
	return __raw_readl((void *__iomem)reg);
}

static inline void i2s_reg_raw_write(unsigned int reg, int val)
{
	__raw_writel(val, (void *__iomem)reg);
}

#if 0
static int i2s_reg_write(unsigned int reg, int val)
{
	spin_lock(&i2s_lock);
	i2s_reg_raw_write(reg, val);
	spin_unlock(&i2s_lock);
	sp_asoc_pr_reg("[0x%04x] W:[0x%08x] R:[0x%08x]\n", reg & 0xFFFF, val,
		       i2s_reg_read(reg));
	return 0;
}
#endif


static inline struct i2s_dai *to_info(struct snd_soc_dai *dai)
{
	return snd_soc_dai_get_drvdata(dai);
}

/*
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int i2s_reg_update(unsigned int reg, int val, int mask)
{
	int new, old;
	spin_lock(&i2s_lock);
	old = i2s_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	i2s_reg_raw_write(reg, new);
	spin_unlock(&i2s_lock);
	sp_asoc_pr_reg("[0x%04x] U:[0x%08x] R:[0x%08x]\n", reg & 0xFFFF, new,
		       i2s_reg_read(reg));
	return old != new;
}

static int i2s_global_disable(struct i2s_priv *i2s)
{
	sp_asoc_pr_dbg("%s\n", __func__);
	return arch_audio_i2s_disable(i2s->hw_port);
}

static int i2s_global_enable(struct i2s_priv *i2s)
{
	sp_asoc_pr_dbg("%s\n", __func__);
	arch_audio_i2s_enable(i2s->hw_port);
	return arch_audio_i2s_switch(i2s->hw_port, AUDIO_TO_ARM_CTRL);
}

static int i2s_soft_reset(struct i2s_priv *i2s)
{
	sp_asoc_pr_dbg("%s\n", __func__);
	return arch_audio_i2s_reset(i2s->hw_port);
}

static int i2s_calc_clk(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int bit_clk;
	int cycle;
	int source_clk;
	int bit;
	int val;
	struct clk *clk_parent;
	switch (config->fs) {
	case 8000:
	case 16000:
	case 32000:
                clk_parent = clk_get(NULL, "clk_128m");


		break;
	case 9600:
	case 12000:
	case 24000:
	case 48000:
		clk_parent = clk_get(NULL, "clk_153m6");


		break;
	default:
		pr_err("ERR:I2S Can't Support %d Clock\n", config->fs);
		return -ENOTSUPP;
	}

	if (IS_ERR(clk_parent)) {
		int ret = PTR_ERR(clk_parent);
		pr_err("ERR:I2S Get Clock Source Error %d!\n", ret);
		return ret;
	}

	clk_set_parent(i2s->i2s_clk, clk_parent);
	source_clk = clk_get_rate(clk_parent);
	clk_set_rate(i2s->i2s_clk, source_clk);
	clk_put(clk_parent);

	sp_asoc_pr_dbg("I2S Source Clock is %d HZ\n", source_clk);
	cycle = (PCM_BUS == config->bus_type) ? (config->pcm_cycle + 1) : 2;
	bit = 8 << config->byte_per_chan;
	bit_clk = config->fs * cycle * bit;
	sp_asoc_pr_dbg("I2S BIT Clock is %d HZ\n", bit_clk);
	if (source_clk % (bit_clk << 1))
		return -ENOTSUPP;
	val = (source_clk / bit_clk) >> 1;
	--val;
	return val;
}

static int i2s_set_clkd(struct i2s_priv *i2s)
{
	int shift = 0;
	int mask = 0xFFFF << shift;
	int val = 0;
	unsigned int reg = I2S_REG(i2s, IIS_CLKD);
	sp_asoc_pr_dbg("%s\n", __func__);
	val = i2s_calc_clk(i2s);
	if (val < 0)
		return val;
	i2s_reg_update(reg, val, mask);
	return 0;
}

static int i2s_set_clk_m_n(struct i2s_priv *i2s)
{
	/* This will support futher */
	return -ENOTSUPP;
}

static void i2s_set_bus_type(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(15);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (PCM_BUS == config->bus_type) ? mask : 0, mask);
}

static void i2s_set_byte_per_channal(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 4;
	int mask = 0x3 << shift;
	int val = 0;
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->byte_per_chan;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(3);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s mode %d\n", __func__, config->mode);
	i2s_reg_update(reg, (I2S_SLAVE == config->mode) ? mask : 0, mask);
}

static void i2s_set_lsb(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(2);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (I2S_LSB == config->lsb) ? mask : 0, mask);
}

static void i2s_set_rtx_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 6;
	int mask = 0x3 << shift;
	int val = 0;
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->rtx_mode;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_sync_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(9);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (I2S_SYNC == config->sync_mode) ? mask : 0, mask);
}

static void i2s_set_lrck_invert(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(10);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (I2S_L_RIGTH == config->lrck_inv) ? mask : 0, mask);
}

static void i2s_set_clk_invert(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(11);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (I2S_CLK_R == config->clk_inv) ? mask : 0, mask);
}

static void i2s_set_i2s_bus_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(8);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (I2S_COMPATIBLE == config->i2s_bus_mode) ? mask : 0,
		       mask);
}

static void i2s_set_pcm_bus_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(8);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg,
		       (I2S_SHORT_FRAME == config->pcm_bus_mode) ? mask : 0,
		       mask);
}

static void i2s_set_pcm_slot(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 0;
	int mask = 0x7 << shift;
	int val = 0;
	unsigned int reg = I2S_REG(i2s, IIS_CTRL2);
	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->pcm_slot;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_pcm_cycle(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 3;
	int mask = 0x7F << shift;
	int val = 0;
	unsigned int reg = I2S_REG(i2s, IIS_CTRL2);
	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->pcm_cycle;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_rx_watermark(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 0;
	int mask = 0x1F1F << shift;
	int val = 0;
	unsigned int reg = I2S_REG(i2s, IIS_CTRL3);
	sp_asoc_pr_dbg("%s\n", __func__);
	/* full watermark */
	val = config->rx_watermark;
	/* empty watermark */
	val |= (I2S_FIFO_DEPTH - config->rx_watermark) << 8;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_tx_watermark(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 0;
	int mask = 0x1F1F << shift;
	int val = 0;
	unsigned int reg = I2S_REG(i2s, IIS_CTRL4);
	sp_asoc_pr_dbg("%s\n", __func__);
	/* empty watermark */
	val = config->tx_watermark << 8;
	/* full watermark */
	val |= I2S_FIFO_DEPTH - config->tx_watermark;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_slave_timeout(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 0;
	int mask = 0xFFF << shift;
	int val = 0;
	unsigned int reg = I2S_REG(i2s, IIS_CTRL1);
	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->slave_timeout;
	i2s_reg_update(reg, val << shift, mask);
}

static int i2s_get_dma_data_width(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	if (PCM_BUS == config->bus_type) {
		if (config->byte_per_chan == I2S_BPCH_16) {
			return SHORT_WIDTH;
		}
	}
	return WORD_WIDTH;
}

static int i2s_get_dma_step(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	if (PCM_BUS == config->bus_type) {
		if (config->byte_per_chan == I2S_BPCH_16) {
			return 2;
		}
	}
	return 4;
}

static int i2s_get_data_position(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	if (PCM_BUS == config->bus_type) {
		if (config->byte_per_chan == I2S_BPCH_16) {
			if (I2S_SLAVE == config->mode) {
				return 2;
			}
		}
	}
	return 0;
}

static int i2s_config_rate(struct i2s_priv *i2s)
{
	int ret = 0;
	struct i2s_config *config = &i2s->config;
	if (I2S_MASTER == config->mode) {
		ret = i2s_set_clkd(i2s);
	}
	if (ret) {
		ret = i2s_set_clk_m_n(i2s);
	}
	return ret;
}

static int i2s_config_apply(struct i2s_priv *i2s)
{
	int ret = 0;
	struct i2s_config *config = &i2s->config;
	sp_asoc_pr_dbg("%s\n", __func__);

	if (I2S_BPCH_8 == config->byte_per_chan) {
		pr_err("ERR:The I2S can't Support 8byte Mode in this code\n");
		ret = -ENOTSUPP;
	}

	i2s_set_bus_type(i2s);
	i2s_set_mode(i2s);
	i2s_set_byte_per_channal(i2s);
	i2s_set_rtx_mode(i2s);
	i2s_set_sync_mode(i2s);
	i2s_set_lsb(i2s);
	i2s_set_lrck_invert(i2s);
	i2s_set_clk_invert(i2s);
	i2s_set_rx_watermark(i2s);
	i2s_set_tx_watermark(i2s);
	if (I2S_SLAVE == config->mode) {
		i2s_set_slave_timeout(i2s);
	}
	if (I2S_BUS == config->bus_type) {
		i2s_set_i2s_bus_mode(i2s);
	}
	if (PCM_BUS == config->bus_type) {
		i2s_set_pcm_bus_mode(i2s);
		i2s_set_pcm_slot(i2s);
		i2s_set_pcm_cycle(i2s);
	}

	ret = i2s_config_rate(i2s);

	return ret;
}

static void i2s_dma_ctrl(struct i2s_priv *i2s, int enable)
{
	int mask = BIT(14);
	unsigned int reg = I2S_REG(i2s, IIS_CTRL0);
	sp_asoc_pr_dbg("%s Enable = %d\n", __func__, enable);
	if (!enable) {
		if (atomic_read(&i2s->open_cnt) <= 0) {
			i2s_reg_update(reg, 0, mask);
		}
	} else {
		i2s_reg_update(reg, mask, mask);
	}
}

static int i2s_close(struct i2s_priv *i2s)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, atomic_read(&i2s->open_cnt));
	if (atomic_dec_and_test(&i2s->open_cnt)) {
		i2s_soft_reset(i2s);
		i2s_global_disable(i2s);
		if (!IS_ERR(i2s->i2s_clk)) {
			clk_disable(i2s->i2s_clk);
			clk_put(i2s->i2s_clk);
		}
	}
	return 0;
}

static int i2s_open(struct i2s_priv *i2s)
{
	int ret = 0;

	sp_asoc_pr_dbg("%s %d\n", __func__, atomic_read(&i2s->open_cnt));

	atomic_inc(&i2s->open_cnt);
	if (atomic_read(&i2s->open_cnt) == 1) {
		i2s->i2s_clk =
		    clk_get(NULL, arch_audio_i2s_clk_name(i2s->hw_port));
		if (IS_ERR(i2s->i2s_clk)) {
			ret = PTR_ERR(i2s->i2s_clk);
			pr_err("ERR:I2S Get clk Error %d!\n", ret);
			return ret;
		}
		i2s_global_enable(i2s);
		i2s_soft_reset(i2s);
		i2s_dma_ctrl(i2s, 0);
		ret = i2s_config_apply(i2s);
		clk_enable(i2s->i2s_clk);
	}

	return ret;
}

static int i2s_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int ret;
	struct i2s_priv *i2s = to_info(dai);
	printk("i2s->id %d \n",i2s->id);
	dai->ac97_pdata = &i2s->config;
	ret = i2s_open(i2s);
	if (ret < 0) {
		pr_err("ERR:I2S Open Error!\n");
		return ret;
	}

	return 0;
}

static void i2s_shutdown(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	struct i2s_config *config = dai->ac97_pdata;
	struct i2s_priv *i2s = container_of(config, struct i2s_priv, config);

	sp_asoc_pr_dbg("%s\n", __func__);

	i2s_close(i2s);
}

static int i2s_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	int ret = 0;
	struct sprd_pcm_dma_params *dma_data;
	struct i2s_config *config = dai->ac97_pdata;
	struct i2s_priv *i2s = container_of(config, struct i2s_priv, config);

	sp_asoc_pr_dbg("%s Port %d\n", __func__, i2s->hw_port);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_data = &i2s_pcm_stereo_out;
		dma_data->channels[0] = i2s->tx.dma_no;
	} else {
		dma_data = &i2s_pcm_stereo_in;
		dma_data->channels[0] = i2s->rx.dma_no;
	}
	dma_data->desc.datawidth = i2s_get_dma_data_width(i2s);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data->desc.src_step = i2s_get_dma_step(i2s);
	else
		dma_data->desc.des_step = i2s_get_dma_step(i2s);

	dma_data->dev_paddr[0] =
	    I2S_PHY_REG(i2s, IIS_TXD) + i2s_get_data_position(i2s);

	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	i2s->config.fs = params_rate(params);
	ret = i2s_config_rate(i2s);
	if (ret) {
		return ret;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		ret = -ENOTSUPP;
		pr_err("ERR:I2S Only Supports format S16_LE now!\n");
		break;
	}

	if (params_channels(params) > 2) {
		ret = -ENOTSUPP;
		pr_err("ERR:I2S Can not Supports Grate 2 Channels\n");
	}

	return ret;
}

static int i2s_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	struct i2s_config *config = dai->ac97_pdata;
	struct i2s_priv *i2s = container_of(config, struct i2s_priv, config);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		i2s_dma_ctrl(i2s, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		i2s_dma_ctrl(i2s, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct snd_soc_dai_ops sprd_i2s_dai_ops = {
	.startup = i2s_startup,
	.shutdown = i2s_shutdown,
	.hw_params = i2s_hw_params,
	.trigger = i2s_trigger,
};



static const struct snd_soc_component_driver sprd_i2s_component = {
	.name = "i2s",
};

static int i2s_config_from_node(struct device_node *node, struct i2s_priv *i2s)
{
	if (!of_find_property(node, "sprd,config", NULL)) {
		i2s->config = def_i2s_config;
		sp_asoc_pr_dbg("Use I2S default config!\n");
	} else {
		u32 val;
		struct device_node *config_node;
		config_node = of_parse_phandle(node, "sprd,config", 0);
		if (of_find_property(config_node, "sprd,def_pcm_config", NULL)) {
			i2s->config = def_pcm_config;
			sp_asoc_pr_dbg("Use PCM default config!\n");
		} else {
			i2s->config = def_i2s_config;
			sp_asoc_pr_dbg("Use I2S default config!\n");
		}
		if (!of_property_read_u32
		    (config_node, "sprd,slave_timeout", &val)) {
			i2s->config.slave_timeout = val;
			sp_asoc_pr_dbg("Change slave_timeout to %d!\n", val);
		}
		if (!of_property_read_u32
		    (config_node, "sprd,byte_per_chan", &val)) {
			i2s->config.byte_per_chan = val;
			sp_asoc_pr_dbg("Change byte per channal to %d!\n", val);
		}
		if (!of_property_read_u32(config_node, "sprd,slave_mode", &val)) {
			if (val) {
				i2s->config.mode = I2S_SLAVE;
			} else {
				i2s->config.mode = I2S_MASTER;
			}
			sp_asoc_pr_dbg("Change to %s!\n",
				       val ? "Slave" : "Master");
		}
		if (!of_property_read_u32(config_node, "sprd,lsb", &val)) {
			if (val) {
				i2s->config.lsb = I2S_LSB;
			} else {
				i2s->config.lsb = I2S_MSB;
			}
			sp_asoc_pr_dbg("Change to %s!\n", val ? "LSB" : "MSB");
		}
		if (!of_property_read_u32(config_node, "sprd,lrck", &val)) {
			if (val) {
				i2s->config.sync_mode = I2S_LRCK;
			} else {
				i2s->config.sync_mode = I2S_SYNC;
			}
			sp_asoc_pr_dbg("Change to %s!\n",
				       val ? "LRCK" : "SYNC");
		}
		if (!of_property_read_u32
		    (config_node, "sprd,low_for_left", &val)) {
			if (val) {
				i2s->config.lrck_inv = I2S_L_LEFT;
			} else {
				i2s->config.lrck_inv = I2S_L_RIGTH;
			}
			sp_asoc_pr_dbg("Change to %s!\n",
				       val ? "Low for Left" : "Low for Right");
		}
		if (!of_property_read_u32(config_node, "sprd,clk_inv", &val)) {
			if (val) {
				i2s->config.clk_inv = I2S_CLK_R;
			} else {
				i2s->config.clk_inv = I2S_CLK_N;
			}
			sp_asoc_pr_dbg("Change to %s!\n",
				       val ? "CLK INV" : "CLK Normal");
		}
		if (!of_property_read_u32
		    (config_node, "sprd,i2s_compatible", &val)) {
			if (val) {
				i2s->config.i2s_bus_mode = I2S_COMPATIBLE;
			} else {
				i2s->config.i2s_bus_mode = I2S_MSBJUSTFIED;
			}
			sp_asoc_pr_dbg("Change to %s!\n",
				       val ? "Compatible" : "MSBJustfied");
		}
		if (!of_property_read_u32
		    (config_node, "sprd,pcm_short_frame", &val)) {
			if (val) {
				i2s->config.pcm_bus_mode = I2S_SHORT_FRAME;
			} else {
				i2s->config.pcm_bus_mode = I2S_LONG_FRAME;
			}
			sp_asoc_pr_dbg("Change to %s!\n",
				       val ? "Short Frame" : "Long Frame");
		}
		if (!of_property_read_u32(config_node, "sprd,pcm_slot", &val)) {
			i2s->config.pcm_slot = val;
			sp_asoc_pr_dbg("Change PCM Slot to 0x%x!\n", val);
		}
		if (!of_property_read_u32(config_node, "sprd,pcm_cycle", &val)) {
			i2s->config.pcm_cycle = val;
			sp_asoc_pr_dbg("Change PCM Cycle to %d!\n", val);
		}
		if (!of_property_read_u32
		    (config_node, "sprd,tx_watermark", &val)) {
			i2s->config.tx_watermark = val;
			sp_asoc_pr_dbg("Change TX Watermark to %d!\n", val);
		}
		if (!of_property_read_u32
		    (config_node, "sprd,rx_watermark", &val)) {
			i2s->config.rx_watermark = val;
			sp_asoc_pr_dbg("Change RX Watermark to %d!\n", val);
		}
		of_node_put(config_node);
	}
	return 0;
}

static int i2s_drv_probe(struct platform_device *pdev)
{
	int ret;
	struct i2s_priv *i2s;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;

	sp_asoc_pr_dbg("%s\n", __func__);

	i2s = devm_kzalloc(&pdev->dev, sizeof(struct i2s_priv), GFP_KERNEL);
	if (!i2s) {
		pr_err("ERR:No memery!\n");
		return -ENOMEM;
	}
	i2s->dev = &pdev->dev;
	if (node) {
		u32 val[2];
		if (of_property_read_u32(node, "sprd,id", &val[0])) {
			pr_err("ERR:Must give me the id!\n");
			goto probe_err;
		}
		i2s->id = val[0];
		if (of_property_read_u32(node, "sprd,hw_port", &val[0])) {
			pr_err("ERR:Must give me the hw_port!\n");
			goto probe_err;
		}
		i2s->hw_port = val[0];
		if (of_property_read_u32_array(node, "sprd,base", &val[0], 2)) {
			pr_err("ERR:Must give me the base address!\n");
			goto probe_err;
		}
		i2s->memphys = (unsigned int *)val[0];
		i2s->membase = (void __iomem *)val[1];
		if (of_property_read_u32(node, "sprd,dma_rx_no", &val[0])) {
			pr_err("ERR:Must give me the dma rx number!\n");
			goto probe_err;
		}
		i2s->rx.dma_no = val[0];
		if (of_property_read_u32(node, "sprd,dma_tx_no", &val[0])) {
			pr_err("ERR:Must give me the dma tx number!\n");
			goto probe_err;
		}
		i2s->tx.dma_no = val[0];

		const char *dai_name  = NULL;
		if (of_property_read_string(node, "sprd,dai_name", &dai_name)) {
			pr_err("ERR:Must give me the dma tx number!\n");
			//return -EINVAL;
		}
		if (dai_name != NULL)
			strcpy(i2s->dai_name,dai_name);
		printk("dt version i2s->dai_name %s \n",i2s->dai_name);
		ret = i2s_config_from_node(node, i2s);
	} else {
		i2s->config =
		    *((struct i2s_config *)(dev_get_platdata(&pdev->dev)));
		i2s->id = pdev->id;
		snprintf(i2s->dai_name,DAI_NAME_SIZE,"%s%d","i2s_bt_sco",i2s->id);
		printk("not bt version i2s->dai_name %s \n",i2s->dai_name);
		i2s->hw_port = i2s->config.hw_port;
		sp_asoc_pr_dbg("i2s.%d(%d) default fs is (%d)\n", i2s->id,
			       i2s->hw_port, i2s->config.fs);

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if(!res)
			goto probe_err;
		i2s->membase = (void __iomem *)res->start;
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if(!res)
			goto probe_err;
		i2s->memphys = (unsigned int *)res->start;

		res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
		if(!res)
			goto probe_err;
		i2s->tx.dma_no = res->start;
		i2s->rx.dma_no = res->end;
	}

	i2s->i2s_dai_driver[0].id = I2S_MAGIC_ID;
	i2s->i2s_dai_driver[0].name = i2s->dai_name;
	printk("i2s->i2s_dai_driver[0].name %s \n",i2s->i2s_dai_driver[0].name);
	i2s->i2s_dai_driver[0].playback.channels_min = 1;
	i2s->i2s_dai_driver[0].playback.channels_max = 2;
	i2s->i2s_dai_driver[0].playback.rates = SNDRV_PCM_RATE_CONTINUOUS;
	i2s->i2s_dai_driver[0].playback.rate_max = 96000;
	i2s->i2s_dai_driver[0].playback.formats = SNDRV_PCM_FMTBIT_S16_LE;

	i2s->i2s_dai_driver[0].capture.channels_min = 1;
	i2s->i2s_dai_driver[0].capture.channels_max = 2;
	i2s->i2s_dai_driver[0].capture.rates = SNDRV_PCM_RATE_CONTINUOUS;
	i2s->i2s_dai_driver[0].capture.rate_max = 96000;
	i2s->i2s_dai_driver[0].capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
	i2s->i2s_dai_driver[0].ops = &sprd_i2s_dai_ops;

	i2s->i2s_dai_driver[1].id = I2S_MAGIC_ID + 1;
	i2s->i2s_dai_driver[1].name = "i2s_bt_sco_dummy";
	printk("i2s->i2s_dai_driver[1].name %s \n",i2s->i2s_dai_driver[1].name);

	arch_audio_i2s_switch(i2s->hw_port, AUDIO_TO_ARM_CTRL);
	sp_asoc_pr_dbg("membase = 0x%x memphys = 0x%x\n", (int)i2s->membase,
		       (int)i2s->memphys);
	sp_asoc_pr_dbg("DMA Number tx = %d rx = %d\n", i2s->tx.dma_no,
		       i2s->rx.dma_no);

	platform_set_drvdata(pdev, i2s);

	atomic_set(&i2s->open_cnt, 0);

	ret =
	    snd_soc_register_component(&pdev->dev, &sprd_i2s_component,
				       i2s->i2s_dai_driver, ARRAY_SIZE(i2s->i2s_dai_driver));
	sp_asoc_pr_dbg("return %i\n", ret);

	return ret;
probe_err:
	devm_kfree(&pdev->dev, i2s);
	return -EINVAL;
}

static int i2s_drv_remove(struct platform_device *pdev)
{
	struct i2s_priv *i2s;
	i2s = platform_get_drvdata(pdev);

	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2s_of_match[] = {
	{.compatible = "sprd,i2s",},
	{},
};

MODULE_DEVICE_TABLE(of, i2s_of_match);
#endif

static struct platform_driver i2s_driver = {
	.driver = {
		   .name = "i2s",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(i2s_of_match),
		   },

	.probe = i2s_drv_probe,
	.remove = i2s_drv_remove,
};

module_platform_driver(i2s_driver);

MODULE_DESCRIPTION("SPRD ASoC I2S CUP-DAI driver");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cpu-dai:i2s");
