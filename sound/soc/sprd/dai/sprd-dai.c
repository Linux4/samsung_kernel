/*
 * SPRD SoC CPU-DAI -- SpreadTrum SOC CPU DAI.
 *
 * Copyright (C) 2021 SpreadTrum Ltd.
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include "sprd-audio.h"
#include "sprd-i2s.h"
#include "sprd-dai.h"
#ifdef CONFIG_SND_SOC_SPRD_AUDIO_TWO_STAGE_DMAENGINE_SURPPORT
#include "sprd-2stage-dmaengine-pcm.h"
#else
#include "sprd-dmaengine-pcm.h"
#endif

/* AGCP_AHB registers offset */
#define REG_AGCP_AHB_EB		0x0000
#define REG_AGCP_AHB_SEL	0x0004
#define REG_AGCP_AHB_RST	0x0008

/* register offset */
#define IIS_TXD         (0x0000)
#define IIS_RXD         (0x0000)
#define IIS_CLKD        (0x0004)
#define IIS_CTRL0       (0x0008)
#define IIS_CTRL1       (0x000C)
#define IIS_CTRL2       (0x0010)
#define IIS_CTRL3       (0x0014)
#define IIS_INT_IEN     (0x0018)
#define IIS_INT_CLR     (0x001C)
#define IIS_INT_RAW     (0x0020)
#define IIS_INT_STS     (0x0024)
#define IIS_STS1        (0x0028)
#define IIS_STS2        (0x002C)
#define IIS_STS3        (0x0030)
#define IIS_DSPWAIT     (0x0034)
#define IIS_CTRL4       (0x0038)
#define IIS_STS4        (0x003C)
#define IIS_CTRL5       (0x0040)
#define IIS_CLKML       (0x0050)
#define IIS_CLKMH       (0x0054)
#define IIS_CLKNL       (0x0058)
#define IIS_CLKNH       (0x005C)
#define IIS_TXD3        (0x0060)
#define IIS_TXD2        (0x0064)
#define IIS_TXD1        (0x0068)

/* bus type */
#define I2S_MODE        0
#define PCM_MODE        1
/* work mode */
#define I2S_MASTER      0
#define I2S_SLAVE       1
/* trans mode */
#define I2S_MSB         0
#define I2S_LSB         1
/* rtx mode */
#define I2S_RTX_DIS     0
#define I2S_RX_MODE     1
#define I2S_TX_MODE     2
#define I2S_RTX_MODE    3
/* sync mode */
#define I2S_LRCK        0
#define I2S_SYNC        1
/* bus or frame mode */
#define MSBJUSTFIED_OR_LONG_FRAME   0
#define COMPATIBLE_OR_SHORT_FRAME   1
/* left channel level */
#define LOW_LEFT        0
#define HIGH_RIGTH      1
/* clk invert */
#define RX_N_TX_P_CLK_N	    0
#define RX_N_TX_P_CLK_R     1
#define RX_P_TX_N_CLK_N     2
#define RX_P_TX_N_CLK_R     3
#define RX_N_TX_N_CLK_N     4
#define RX_N_TX_N_CLK_R	    5
#define RX_P_TX_P_CLK_N     6
#define RX_P_TX_P_CLK_R     7

struct sprd_agcp_i2s_config {
	u32 hw_port;
	u32 bus_type;
	u32 work_mode;
	u32 trans_mode;
	u32 rtx_mode;
	u32 sync_mode;
	u32 bus_frame_mode;
	u32 multi_ch;
	u32 left_ch_lvl;
	u32 clk_inv;
	u32 tx_watermark;
	u32 rx_watermark;
	u32 slave_timeout;
	u32 pcm_slot;
	u32 pcm_cycle;
};

struct sprd_agcp_i2s_priv {
	struct sprd_agcp_i2s_config config;
	struct sprd_pcm_dma_params dma_data;
	int rx_dma_no;
	int tx_dma_no;
	u32 div_mode;
	u32 rate_multi;
	u32 fifo_depth;
	atomic_t open_cnt;
	void __iomem *clk_membase;
	void __iomem *membase;
	unsigned int *memphys;
	unsigned int reg_size;
	struct clk *i2s_clk;
	struct clk *clk_128m;
	struct clk *clk_153m6;
};

struct sprd_dai_data {
	union {
		struct sprd_agcp_i2s_priv agcp_i2s;
	};
	struct device *dev;
	u32 id;
};

static char *sprd_use_dma_name[] = {
	"iis0_tx", "iis0_rx",
	"iis1_tx", "iis1_tx",
	"iis2_tx", "iis2_tx",
	"iis3_tx", "iis3_tx",
};

static const struct sprd_agcp_i2s_config default_agcp_i2s_config = {
	.hw_port        = 0,
	.bus_type       = I2S_MODE,
	.work_mode      = I2S_MASTER,
	.trans_mode     = I2S_MSB,
	.rtx_mode       = I2S_RTX_MODE,
	.sync_mode      = I2S_LRCK,
	.bus_frame_mode = MSBJUSTFIED_OR_LONG_FRAME,
	.multi_ch       = 0x1,
	.left_ch_lvl    = LOW_LEFT,
	.clk_inv        = RX_N_TX_P_CLK_N,
	.tx_watermark   = 0x40,
	.rx_watermark   = 0xc0,
	.slave_timeout  = 0xF11,
};

static const struct sprd_agcp_i2s_config default_agcp_pcm_config = {
	.hw_port        = 0,
	.bus_type       = PCM_MODE,
	.work_mode      = I2S_MASTER,
	.trans_mode     = I2S_MSB,
	.rtx_mode       = I2S_RTX_MODE,
	.sync_mode      = I2S_SYNC,
	.bus_frame_mode = MSBJUSTFIED_OR_LONG_FRAME,
	.multi_ch       = 0x1,
	.left_ch_lvl    = LOW_LEFT,
	.clk_inv        = RX_N_TX_P_CLK_N,
	.tx_watermark   = 0x40,
	.rx_watermark   = 0xc0,
	.slave_timeout  = 0xF11,
	.pcm_slot       = 0x1,
	.pcm_cycle      = 1,
};

static inline int sprd_dai_reg_read(unsigned long reg)
{
	return readl_relaxed((void *__iomem)reg);
}

static inline void sprd_dai_reg_raw_write(unsigned long reg, int val)
{
	writel_relaxed(val, (void *__iomem)reg);
}

static int sprd_dai_reg_update(unsigned long reg, int val, int mask)
{
	int new, old;

	old = sprd_dai_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	sprd_dai_reg_raw_write(reg, new);

	pr_debug("%s: [0x%04lx] [0x%08x] [0x%08x]\n", __func__,
		reg & 0xFFFF, new, sprd_dai_reg_read(reg));

	return old != new;
}

static void sprd_agcp_i2s_set_clk_inv(struct sprd_agcp_i2s_priv *agcp_i2s)
{
	unsigned long reg = (unsigned long)(agcp_i2s->membase + IIS_CTRL0);
	u32 val = agcp_i2s->config.clk_inv;

	pr_info("%s: clk_inv = %u\n", __func__, val);

	switch (agcp_i2s->config.clk_inv) {
	case RX_N_TX_P_CLK_N:
		sprd_dai_reg_update(reg, 0x0 << 11, 0x1 << 11);
		sprd_dai_reg_update(reg, 0x1, 0x3);
		break;
	case RX_N_TX_P_CLK_R:
		sprd_dai_reg_update(reg, 0x1 << 11, 0x1 << 11);
		sprd_dai_reg_update(reg, 0x1, 0x3);
		break;
	case RX_P_TX_N_CLK_N:
		if (agcp_i2s->config.work_mode == I2S_MASTER) {
			sprd_dai_reg_update(reg, 0x0 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x2, 0x3);
		} else {
			sprd_dai_reg_update(reg, 0x1 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x1, 0x3);
		}
		break;
	case RX_P_TX_N_CLK_R:
		if (agcp_i2s->config.work_mode == I2S_MASTER) {
			sprd_dai_reg_update(reg, 0x1 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x2, 0x3);
		} else {
			sprd_dai_reg_update(reg, 0x0 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x1, 0x3);
		}
		break;
	case RX_N_TX_N_CLK_N:
		if (agcp_i2s->config.work_mode == I2S_MASTER) {
			sprd_dai_reg_update(reg, 0x0 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x3, 0x3);
		} else {
			sprd_dai_reg_update(reg, 0x1 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x1, 0x3);
		}
		break;
	case RX_N_TX_N_CLK_R:
		if (agcp_i2s->config.work_mode == I2S_MASTER) {
			sprd_dai_reg_update(reg, 0x1 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x3, 0x3);
		} else {
			sprd_dai_reg_update(reg, 0x0 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x1, 0x3);
		}
		break;
	case RX_P_TX_P_CLK_N:
		if (agcp_i2s->config.work_mode == I2S_MASTER) {
			sprd_dai_reg_update(reg, 0x0 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x0, 0x3);
		} else {
			sprd_dai_reg_update(reg, 0x0 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x1, 0x3);
		}
		break;
	case RX_P_TX_P_CLK_R:
		if (agcp_i2s->config.work_mode == I2S_MASTER) {
			sprd_dai_reg_update(reg, 0x1 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x0, 0x3);
		} else {
			sprd_dai_reg_update(reg, 0x1 << 11, 0x1 << 11);
			sprd_dai_reg_update(reg, 0x1, 0x3);
		}
		break;
	default:
		pr_warn("%s: unsupported clk_inv = %u\n",
			__func__, agcp_i2s->config.clk_inv);
		break;
	}
}

static void sprd_agcp_i2s_set_tx_watermark(struct sprd_agcp_i2s_priv *agcp_i2s)
{
	unsigned long reg = (unsigned long)(agcp_i2s->membase + IIS_CTRL4);
	u32 val = agcp_i2s->config.tx_watermark << 8;
	u32 mask = 0xFFFF;

	/* full watermark */
	val |= (agcp_i2s->fifo_depth - agcp_i2s->config.tx_watermark);

	pr_info("%s: tx watermark = 0x%x\n", __func__, val);

	sprd_dai_reg_update(reg, val, mask);
}

static void sprd_agcp_i2s_set_rx_watermark(struct sprd_agcp_i2s_priv *agcp_i2s)
{
	unsigned long reg = (unsigned long)(agcp_i2s->membase + IIS_CTRL3);
	u32 val = agcp_i2s->config.rx_watermark;
	u32 mask = 0xFFFF;

	/* empty watermark */
	val |= (agcp_i2s->fifo_depth - agcp_i2s->config.rx_watermark) << 8;

	pr_info("%s: rx watermark = 0x%x\n", __func__, val);

	sprd_dai_reg_update(reg, val, mask);
}

static void sprd_agcp_i2s_set_dummy_mode(struct sprd_agcp_i2s_priv *agcp_i2s,
		int rate, int channel)
{
	unsigned long reg = (unsigned long)(agcp_i2s->membase + IIS_CTRL5);
	u32 bus_frame_mode = agcp_i2s->config.bus_frame_mode;
	u32 sync_mode = agcp_i2s->config.sync_mode;

	if (agcp_i2s->config.bus_type != I2S_MODE)
		return;

	pr_info("%s: div_mode = %u bus_frame_mode = %u sync_mode = %u\n",
		__func__, agcp_i2s->div_mode, bus_frame_mode, sync_mode);

	switch (agcp_i2s->div_mode) {
	case 0:
	case 1:
		if (bus_frame_mode == COMPATIBLE_OR_SHORT_FRAME &&
			sync_mode == I2S_LRCK)
			sprd_dai_reg_update(reg, 0x2, 0x3);
		break;
	case 2:
		sprd_dai_reg_update(reg, bus_frame_mode + 1, 0x3);
		if (rate > 96000 || (channel == 2
				&& bus_frame_mode == COMPATIBLE_OR_SHORT_FRAME))
			sprd_dai_reg_update(reg, 0x1 << 7, 0x1 << 7);
		break;
	default:
		pr_warn("%s: unsupported div_mode = %u\n",
			__func__, agcp_i2s->div_mode);
	}

}

static void sprd_agcp_i2s_dma_enable(struct sprd_agcp_i2s_priv *agcp_i2s,
		int enable)
{
	unsigned long reg = (unsigned long)(agcp_i2s->membase + IIS_CTRL0);

	pr_debug("%s: enable = %d\n", __func__, enable);

	if (enable)
		sprd_dai_reg_update(reg, 0x1 << 14, 0x1 << 14);
	else
		sprd_dai_reg_update(reg, 0, 0x1 << 14);
}

static int sprd_agcp_i2s_config_param(struct sprd_agcp_i2s_priv *agcp_i2s,
		int rate, int channel, int bit_width)
{
	struct regmap *agcp_ahb_gpr = arch_audio_get_agcp_ahb_gpr();
	struct sprd_agcp_i2s_config *config = &agcp_i2s->config;
	unsigned long reg_base = (unsigned long)(agcp_i2s->membase);
	u32 bytes_per_ch;
	u32 src_clk, iis_sck;
	u32 iis_clkd, clkm, clkn;
	int ret;

	pr_info("%s: rate (%d) channel (%d) width (%d) mode (%s)\n",
			__func__, rate, channel, bit_width,
			(config->work_mode == I2S_SLAVE) ?
			"I2S_SLAVE" : "I2S_MASTER");

	if (!agcp_ahb_gpr)
		return -1;
	ret = agdsp_access_enable();
	if (ret)
		return ret;
	sprd_dai_reg_update(
			(unsigned long)(agcp_i2s->clk_membase + 0xa58),
			0, 0x7ff << 13);
	switch (rate) {
	case 8000:
	case 16000:
	case 32000:
		sprd_dai_reg_update(
				(unsigned long)(agcp_i2s->clk_membase + 0xa58),
				0x5 << 13, 0x7 << 13);
		src_clk = 163840000;
		break;
	case 9600:
	case 12000:
	case 24000:
	case 48000:
		sprd_dai_reg_update(
				(unsigned long)(agcp_i2s->clk_membase + 0xa58),
				0x3 << 13, 0x7 << 13);
		src_clk = 245760000;
		break;
	case 96000:
	case 192000:
		sprd_dai_reg_update(
				(unsigned long)(agcp_i2s->clk_membase + 0xa58),
				BIT(1)|BIT(2)|BIT(15)|BIT(16)|BIT(23),
				BIT(1)|BIT(2)|BIT(15)|BIT(16)|BIT(23));
		src_clk = 196608000;
		break;
	default:
		pr_err("%s: unsupported sample rate (%d)\n", __func__, rate);
		return -ENOTSUPP;
	}
	agdsp_access_disable();

	pr_info("%s: src_clk = %dHZ\n", __func__, src_clk);

	sprd_dai_reg_update(reg_base + IIS_CTRL0,
			config->bus_type << 15, 0x1 << 15);
	sprd_dai_reg_update(reg_base + IIS_CTRL0,
			config->work_mode << 3, 0x1 << 3);
	sprd_dai_reg_update(reg_base + IIS_CTRL0,
			config->trans_mode << 2, 0x1 << 2);
	sprd_dai_reg_update(reg_base + IIS_CTRL0,
			config->rtx_mode << 6, 0x3 << 6);
	sprd_dai_reg_update(reg_base + IIS_CTRL0,
			config->sync_mode << 9, 0x1 << 9);
	sprd_dai_reg_update(reg_base + IIS_CTRL0,
			config->bus_frame_mode << 8, 0x1 << 8);

	sprd_dai_reg_update(reg_base + IIS_DSPWAIT,
			config->multi_ch << 12, 0xf << 12);
	sprd_dai_reg_update(reg_base + IIS_DSPWAIT,
			(~config->multi_ch) << 16, 0xf << 16);

	switch (bit_width) {
	case 8:
		bytes_per_ch = 0x0;
		break;
	case 16:
		bytes_per_ch = 0x1;
		break;
	case 24:
	case 32:
		bytes_per_ch = 0x2;
		break;
	default:
		pr_err("%s: unsupported width (%d)\n", __func__, bit_width);
		return -ENOTSUPP;
	}
	sprd_dai_reg_update(
		reg_base + IIS_CTRL0, bytes_per_ch << 4, 0x3 << 4);

	sprd_agcp_i2s_set_clk_inv(agcp_i2s);
	sprd_agcp_i2s_set_tx_watermark(agcp_i2s);
	sprd_agcp_i2s_set_rx_watermark(agcp_i2s);

	if (config->sync_mode == I2S_LRCK) {
		sprd_dai_reg_update(reg_base + IIS_CTRL0,
				config->left_ch_lvl << 10, 0x1 << 10);
	}
	if (config->work_mode == I2S_SLAVE) {
		sprd_dai_reg_update(reg_base + IIS_CTRL1,
				config->slave_timeout, 0xFFF);
	}

	sprd_agcp_i2s_set_dummy_mode(agcp_i2s, rate, channel);

	if (agcp_i2s->config.bus_type == PCM_MODE) {
		sprd_dai_reg_update(reg_base + IIS_CTRL2,
				config->pcm_slot, 0x7);
		sprd_dai_reg_update(reg_base + IIS_CTRL2,
				config->pcm_cycle << 3, 0x7F << 3);
	}

	if (config->work_mode == I2S_SLAVE)
		return 0;

	if (agcp_i2s->div_mode == 0 || agcp_i2s->div_mode == 1) {
		iis_sck = rate * channel * bit_width;
		iis_clkd = src_clk / (2 * iis_sck) - 1;
		if (src_clk % (2 * iis_sck) == 0) {
			pr_info("%s: iis_sck = %uHZ iis_clkd = 0x%x\n",
					__func__, iis_sck, iis_clkd);
			sprd_dai_reg_update(
					reg_base + IIS_CTRL5, 0, 0x3 << 4);
			sprd_dai_reg_update(
					reg_base + IIS_CLKD, iis_clkd, 0xffff);
			return 0;
		}
		clkm = 2 * iis_sck / 100;
		clkn = src_clk / 100;
		pr_info("%s: iis_sck = %uHZ clkm = 0x%x clkn = 0x%x\n",
				__func__, iis_sck, clkm, clkn);
		sprd_dai_reg_update(reg_base + IIS_CTRL5, 0x1 << 4, 0x3 << 4);
		sprd_dai_reg_update(reg_base + IIS_CLKML, clkm, 0xffff);
		sprd_dai_reg_update(reg_base + IIS_CLKMH, clkm >> 16, 0x3f);
		sprd_dai_reg_update(reg_base + IIS_CLKNL, clkn, 0xffff);
		sprd_dai_reg_update(reg_base + IIS_CLKNH, clkn >> 16, 0x3f);
	} else if (agcp_i2s->div_mode == 2 &&
			agcp_i2s->config.work_mode == I2S_MASTER) {
		iis_sck = agcp_i2s->rate_multi * rate;
		if (rate == 192000)
			iis_clkd = 0;
		else
			iis_clkd = src_clk / (2 * iis_sck) - 1;
		clkm = 2 * (iis_clkd + 1) * rate * channel * bit_width / 100;
		clkn = src_clk / 100;

		pr_info("%s: iis_sck = %uHZ iis_clkd = 0x%x\n",
				__func__, iis_sck, iis_clkd);
		pr_info("%s: clkm = 0x%x clkn = 0x%x\n", __func__, clkm, clkn);

		sprd_dai_reg_update(reg_base + IIS_CTRL5, 0x2 << 4, 0x3 << 4);
		sprd_dai_reg_update(reg_base + IIS_CLKML, clkm, 0xffff);
		sprd_dai_reg_update(reg_base + IIS_CLKMH, clkm >> 16, 0x3f);
		sprd_dai_reg_update(reg_base + IIS_CLKNL, clkn, 0xffff);
		sprd_dai_reg_update(reg_base + IIS_CLKNH, clkn >> 16, 0x3f);
		sprd_dai_reg_update(reg_base + IIS_CLKD, iis_clkd, 0xffff);
	}

	sprd_agcp_i2s_dma_enable(agcp_i2s, 0);

	return 0;
}

static int sprd_agcp_i2s_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct sprd_dai_data *dai_data = snd_soc_dai_get_drvdata(dai);
	struct sprd_agcp_i2s_priv *agcp_i2s = &dai_data->agcp_i2s;
	struct regmap *agcp_ahb_gpr = arch_audio_get_agcp_ahb_gpr();
	int ret;

	pr_info("%s: open_cnt = %d\n", __func__,
			atomic_read(&agcp_i2s->open_cnt));

	if (atomic_read(&agcp_i2s->open_cnt) == 0) {
		if (!agcp_ahb_gpr)
			return -1;
		ret = agdsp_access_enable();
		if (ret)
			return ret;
		regmap_update_bits(agcp_ahb_gpr,
					REG_AGCP_AHB_RST, 0x1 << 12, 0x1 << 12);
		udelay(10);
		regmap_update_bits(agcp_ahb_gpr,
					REG_AGCP_AHB_RST, 0x1 << 12, 0);
		udelay(10);
		regmap_update_bits(agcp_ahb_gpr,
					REG_AGCP_AHB_SEL, 0x1 << 15, 0);
		regmap_update_bits(agcp_ahb_gpr,
					REG_AGCP_AHB_SEL, 0x1 << 29, 0x1 << 29);
		regmap_update_bits(agcp_ahb_gpr,
					REG_AGCP_AHB_EB, 0x1, 0x1);
		agdsp_access_disable();
	}
	atomic_inc(&agcp_i2s->open_cnt);

	return 0;
}

static void sprd_agcp_i2s_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct sprd_dai_data *dai_data = snd_soc_dai_get_drvdata(dai);
	struct sprd_agcp_i2s_priv *agcp_i2s = &dai_data->agcp_i2s;
	struct regmap *agcp_ahb_gpr = arch_audio_get_agcp_ahb_gpr();
	int ret;

	pr_info("%s: open_cnt = %d\n", __func__,
			atomic_read(&agcp_i2s->open_cnt));

	if (atomic_read(&agcp_i2s->open_cnt) == 1) {
		if (!agcp_ahb_gpr)
			return;
		ret = agdsp_access_enable();
		if (ret)
			return;
		regmap_update_bits(agcp_ahb_gpr,
					REG_AGCP_AHB_EB, 0x1, 0);
		regmap_update_bits(agcp_ahb_gpr,
					REG_AGCP_AHB_SEL, 0x1 << 29, 0);
		agdsp_access_disable();
	}
	atomic_dec(&agcp_i2s->open_cnt);
}

static int sprd_agcp_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct sprd_dai_data *dai_data = snd_soc_dai_get_drvdata(dai);
	struct sprd_agcp_i2s_priv *agcp_i2s = &dai_data->agcp_i2s;
	struct sprd_pcm_dma_params *dma_data = &agcp_i2s->dma_data;
	int port = agcp_i2s->config.hw_port;
	int rate = params_rate(params);
	int channel = params_channels(params);
	int bit_width;
	int ret = 0;

	pr_info("%s: port (%d)\n", __func__, port);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_data->channels[0] = agcp_i2s->tx_dma_no;
		dma_data->used_dma_channel_name[0] =
						sprd_use_dma_name[2 * port];
		dma_data->desc.fragmens_len =
			agcp_i2s->fifo_depth - agcp_i2s->config.tx_watermark;
	} else {
		dma_data->channels[0] = agcp_i2s->rx_dma_no;
		dma_data->used_dma_channel_name[0] =
						sprd_use_dma_name[2 * port + 1];
		dma_data->desc.fragmens_len = agcp_i2s->config.rx_watermark;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dma_data->desc.datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dma_data->desc.src_step = 2;
			dma_data->desc.des_step = 0;
		} else {
			dma_data->desc.src_step = 0;
			dma_data->desc.des_step = 2;
		}
		bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		dma_data->desc.datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dma_data->desc.src_step = 4;
			dma_data->desc.des_step = 0;
		} else {
			dma_data->desc.src_step = 0;
			dma_data->desc.des_step = 4;
		}
		bit_width = 32;
		break;
	default:
		pr_err("%s: unsupported format %d\n",
			__func__, params_format(params));
		return -ENOTSUPP;
	}
	pr_info("%s: format %d, width %d, src step %d, des step %d\n",
			__func__, params_format(params),
			dma_data->desc.datawidth,
			dma_data->desc.src_step,
			dma_data->desc.des_step);

	dma_data->dev_paddr[0] = (phys_addr_t)(agcp_i2s->memphys + IIS_TXD);
	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	ret = sprd_agcp_i2s_config_param(agcp_i2s, rate, channel, bit_width);
	if (ret) {
		pr_err("%s: sprd_agcp_i2s_config_param() failed\n", __func__);
		return ret;
	}

	return ret;
}

static int sprd_agcp_i2s_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct sprd_dai_data *dai_data = snd_soc_dai_get_drvdata(dai);
	struct sprd_agcp_i2s_priv *agcp_i2s = &dai_data->agcp_i2s;
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		sprd_agcp_i2s_dma_enable(agcp_i2s, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sprd_agcp_i2s_dma_enable(agcp_i2s, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprd_agcp_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_route intercon;

	memset(&intercon, 0, sizeof(intercon));
	if (dai->driver->playback.stream_name &&
			dai->driver->playback.aif_name) {
		intercon.source = dai->driver->playback.aif_name;
		intercon.sink = dai->driver->playback.stream_name;
		snd_soc_dapm_add_routes(&dai->component->dapm, &intercon, 1);
	}
	if (dai->driver->capture.stream_name &&
			dai->driver->capture.aif_name) {
		intercon.source = dai->driver->capture.stream_name;
		intercon.sink = dai->driver->capture.aif_name;
		snd_soc_dapm_add_routes(&dai->component->dapm, &intercon, 1);
	}

	return 0;
}

static struct snd_soc_dai_ops sprd_agcp_i2s_dai_ops = {
	.startup = sprd_agcp_i2s_startup,
	.shutdown = sprd_agcp_i2s_shutdown,
	.hw_params = sprd_agcp_i2s_hw_params,
	.trigger = sprd_agcp_i2s_trigger,
};

static struct snd_soc_dai_driver sprd_agcp_i2s_dai[] = {
	{
		.name = "AGCP_IIS0_TX",
		.id = AGCP_IIS0_TX,
		.playback = {
			.stream_name  = "AGCP_IIS0_TX Playback",
			.aif_name	  = "AGCP_IIS0_TX",
			.formats	  = (SNDRV_PCM_FMTBIT_S16_LE |
						    SNDRV_PCM_FMTBIT_S24_LE |
						    SNDRV_PCM_FMTBIT_S32_LE),
			.rates        = SNDRV_PCM_RATE_8000_192000,
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.probe = sprd_agcp_i2s_dai_probe,
		.ops = &sprd_agcp_i2s_dai_ops,
	},
};

static const struct snd_soc_component_driver sprd_agcp_i2s_component = {
	.name = "sprd-agcp-i2s",
};

#if 0
static int sprd_agcp_i2s_get_clks(struct platform_device *pdev,
		struct sprd_agcp_i2s_priv *agcp_i2s)
{
	struct device *dev = &pdev->dev;

	agcp_i2s->i2s_clk = devm_clk_get(dev,
			arch_audio_i2s_clk_name(agcp_i2s->config.hw_port));
	if (IS_ERR(agcp_i2s->i2s_clk)) {
		dev_err(dev, "%s: failed to get i2s_clk\n", __func__);
		return PTR_ERR(agcp_i2s->i2s_clk);
	}
	agcp_i2s->clk_128m = devm_clk_get(dev, "clk_twpll_128m");
	if (IS_ERR(agcp_i2s->clk_128m)) {
		dev_err(dev, "%s: failed to get clk_128m\n", __func__);
		return PTR_ERR(agcp_i2s->clk_128m);
	}
	agcp_i2s->clk_153m6 = devm_clk_get(dev, "clk_twpll_153m6");
	if (IS_ERR(agcp_i2s->clk_153m6)) {
		dev_err(dev, "%s: failed to get clk_153m6\n", __func__);
		return PTR_ERR(agcp_i2s->clk_153m6);
	}

	return 0;
}
#endif

static int sprd_agcp_i2s_parse_dt(struct device *dev,
		struct sprd_agcp_i2s_priv *agcp_i2s)
{
	struct device_node *node = dev->of_node;
	u32 val;
	int ret;

	ret = of_property_read_u32(node, "sprd,tx-dma-no", &val);
	if (!ret) {
		pr_debug("%s: tx_dma_no = %u\n", __func__, val);
		agcp_i2s->tx_dma_no = val;
	} else {
		pr_err("%s: sprd,tx-dma-no not found\n", __func__);
		return ret;
	}
	ret = of_property_read_u32(node, "sprd,rx-dma-no", &val);
	if (!ret) {
		pr_debug("%s: rx_dma_no = %u\n", __func__, val);
		agcp_i2s->rx_dma_no = val;
	} else {
		pr_err("%s: sprd,rx-dma-no not found\n", __func__);
		return ret;
	}
	ret = of_property_read_u32(node, "sprd,div-mode", &val);
	if (ret) {
		pr_warn("%s: sprd,div-mode not found\n", __func__);
		agcp_i2s->div_mode = 0;
	} else {
		pr_debug("%s: div_mode = %u\n", __func__, val);
		agcp_i2s->div_mode = val;
	}
	ret = of_property_read_u32(node, "sprd,rate-multi", &val);
	if (ret) {
		pr_warn("%s: sprd,rate-multi not found\n", __func__);
		agcp_i2s->rate_multi = 1;
	} else {
		pr_debug("%s: rate_multi = %u\n", __func__, val);
		agcp_i2s->rate_multi = val;
	}
	ret = of_property_read_u32(node, "sprd,fifo-depth", &val);
	if (ret) {
		pr_warn("%s: sprd,fifo-depth not found\n", __func__);
		agcp_i2s->fifo_depth = 256;
	} else {
		pr_debug("%s: fifo_depth = %u\n", __func__, val);
		agcp_i2s->fifo_depth = val;
	}

	ret = of_property_read_u32(node, "sprd,bus-type", &val);
	if (!ret) {
		pr_debug("%s: bus_type = %u\n", __func__, val);
		if (val == I2S_MODE)
			agcp_i2s->config = default_agcp_i2s_config;
		else
			agcp_i2s->config = default_agcp_pcm_config;
		agcp_i2s->config.bus_type = val;
	} else {
		pr_err("%s: sprd,bus-type not found\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(node, "sprd,hw-port", &val);
	if (!ret) {
		pr_debug("%s: hw_port = %u\n", __func__, val);
		agcp_i2s->config.hw_port = val;
	}
	ret = of_property_read_u32(node, "sprd,work-mode", &val);
	if (!ret) {
		pr_debug("%s: work_mode = %u\n", __func__, val);
		agcp_i2s->config.work_mode = val;
	}
	ret = of_property_read_u32(node, "sprd,trans-mode", &val);
	if (!ret) {
		pr_debug("%s: trans_mode = %u\n", __func__, val);
		agcp_i2s->config.trans_mode = val;
	}
	ret = of_property_read_u32(node, "sprd,rtx-mode", &val);
	if (!ret) {
		pr_debug("%s: rtx_mode = %u\n", __func__, val);
		agcp_i2s->config.rtx_mode = val;
	}
	ret = of_property_read_u32(node, "sprd,sync-mode", &val);
	if (!ret) {
		pr_debug("%s: sync_mode = %u\n", __func__, val);
		agcp_i2s->config.sync_mode = val;
	}
	ret = of_property_read_u32(node, "sprd,bus-frame-mode", &val);
	if (!ret) {
		pr_debug("%s: bus_frame_mode = %u\n", __func__, val);
		agcp_i2s->config.bus_frame_mode = val;
	}
	ret = of_property_read_u32(node, "sprd,multi-ch", &val);
	if (!ret) {
		pr_debug("%s: multi_ch = %u\n", __func__, val);
		agcp_i2s->config.multi_ch = val;
	}
	ret = of_property_read_u32(node, "sprd,left-ch-lvl", &val);
	if (!ret) {
		pr_debug("%s: left_ch_lvl = %u\n", __func__, val);
		agcp_i2s->config.left_ch_lvl = val;
	}
	ret = of_property_read_u32(node, "sprd,clk-inv", &val);
	if (!ret) {
		pr_debug("%s: clk_inv = %u\n", __func__, val);
		agcp_i2s->config.clk_inv = val;
	}
	ret = of_property_read_u32(node, "sprd,tx-watermark", &val);
	if (!ret) {
		pr_debug("%s: tx_watermark = %u\n", __func__, val);
		agcp_i2s->config.tx_watermark = val;
	}
	ret = of_property_read_u32(node, "sprd,rx-watermark", &val);
	if (!ret) {
		pr_debug("%s: rx_watermark = %u\n", __func__, val);
		agcp_i2s->config.rx_watermark = val;
	}
	ret = of_property_read_u32(node, "sprd,slave-timeout", &val);
	if (!ret) {
		pr_debug("%s: slave_timeout = %u\n", __func__, val);
		agcp_i2s->config.slave_timeout = val;
	}
	ret = of_property_read_u32(node, "sprd,pcm-slot", &val);
	if (!ret) {
		pr_debug("%s: pcm_slot = %u\n", __func__, val);
		agcp_i2s->config.pcm_slot = val;
	}
	ret = of_property_read_u32(node, "sprd,pcm-cycle", &val);
	if (!ret) {
		pr_debug("%s: pcm_cycle = %u\n", __func__, val);
		agcp_i2s->config.pcm_cycle = val;
	}

	return 0;
}

static int sprd_agcp_i2s_probe(struct platform_device *pdev)
{
	struct sprd_dai_data *dai_data = dev_get_drvdata(&pdev->dev);
	struct sprd_agcp_i2s_priv *agcp_i2s = &dai_data->agcp_i2s;
	struct device *dev = &pdev->dev;
	struct resource *mem_res;
	int ret;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	agcp_i2s->membase = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR(agcp_i2s->membase)) {
		dev_err(&pdev->dev,
			"%s: devm_ioremap_resource() failed\n", __func__);
		return -EINVAL;
	}
	agcp_i2s->memphys = (unsigned int *)mem_res->start;
	agcp_i2s->reg_size = (unsigned int)resource_size(mem_res);

	agcp_i2s->clk_membase = ioremap(0x56250000, 0x3000);
	if (IS_ERR(agcp_i2s->membase)) {
		dev_err(&pdev->dev,
			"%s: ioremap() clk_membase failed\n", __func__);
		return -EINVAL;
	}

	ret = sprd_agcp_i2s_parse_dt(dev, agcp_i2s);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s: sprd_agcp_i2s_parse_dt() failed\n", __func__);
		return ret;
	}
#if 0
	ret = sprd_agcp_i2s_get_clks(pdev, agcp_i2s);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: sprd_agcp_i2s_get_clks() failed\n", __func__);
		return ret;
	}
#endif
	atomic_set(&agcp_i2s->open_cnt, 0);

	pr_info("%s: membase(%pK) memphys(%pK) dma number tx(%d) rx(%d)\n",
			__func__, agcp_i2s->membase, agcp_i2s->memphys,
			agcp_i2s->tx_dma_no, agcp_i2s->rx_dma_no);

	return snd_soc_register_component(&pdev->dev,
						&sprd_agcp_i2s_component,
						sprd_agcp_i2s_dai,
						ARRAY_SIZE(sprd_agcp_i2s_dai));
}

static int sprd_agcp_i2s_remove(struct platform_device *pdev)
{
	struct sprd_dai_data *dai_data = dev_get_drvdata(&pdev->dev);
	struct sprd_agcp_i2s_priv *agcp_i2s = &dai_data->agcp_i2s;

	iounmap(agcp_i2s->clk_membase);
	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static int sprd_dai_probe(struct platform_device *pdev)
{
	struct sprd_dai_data *dai_data;
	struct device *dev = &pdev->dev;
	struct regmap *agcp_ahb_gpr;
	int ret;

	if (!arch_audio_get_agcp_ahb_gpr()) {
		agcp_ahb_gpr = syscon_regmap_lookup_by_phandle(
				pdev->dev.of_node, "sprd,syscon-agcp-ahb");
		if (IS_ERR(agcp_ahb_gpr))
			return -EPROBE_DEFER;
		arch_audio_set_agcp_ahb_gpr(agcp_ahb_gpr);
	}

	dai_data = devm_kzalloc(&pdev->dev,
			sizeof(struct sprd_dai_data), GFP_KERNEL);
	if (!dai_data)
		return -ENOMEM;
	dai_data->dev = dev;
	dev_set_drvdata(dev, dai_data);

	ret = of_property_read_u32(dev->of_node, "sprd,id", &dai_data->id);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to get port id\n", __func__);
		return ret;
	}
	switch (dai_data->id) {
	case AGCP_IIS0_TX:
	case AGCP_IIS0_RX:
		return sprd_agcp_i2s_probe(pdev);
	default:
		dev_err(&pdev->dev, "%s: unknown port id (%u)\n",
			__func__, dai_data->id);
		return -EINVAL;
	}
}

static int sprd_dai_remove(struct platform_device *pdev)
{
	struct sprd_dai_data *dai_data = dev_get_drvdata(&pdev->dev);

	switch (dai_data->id) {
	case AGCP_IIS0_TX:
	case AGCP_IIS0_RX:
		return sprd_agcp_i2s_remove(pdev);
	default:
		dev_err(&pdev->dev, "%s: unknown port id (%u)\n",
			__func__, dai_data->id);
		return -EINVAL;
	}
}

static const struct of_device_id sprd_dai_of_match[] = {
	{.compatible = "sprd,agcp-i2s-dai"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_dai_of_match);

static struct platform_driver sprd_dai_driver = {
	.driver = {
		.name = "sprd-dai",
		.owner = THIS_MODULE,
		.of_match_table = sprd_dai_of_match,
	},

	.probe = sprd_dai_probe,
	.remove = sprd_dai_remove,
};
module_platform_driver(sprd_dai_driver);

MODULE_DESCRIPTION("SPRD ASoC DAI driver");
MODULE_AUTHOR("Bin Pang <bin.pang@unisoc.com>");
MODULE_LICENSE("GPL");
