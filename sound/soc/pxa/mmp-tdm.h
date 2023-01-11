/*
 * linux/sound/soc/pxa/mmp-tdm.h
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MMP_TDM_H
#define _MMP_TDM_H

#include <linux/mfd/mmp-map.h>
/*
 * TDM Registers
 */
#define TDM_CTRL_REG1		(0x3c)
#define TDM_CTRL_REG2		(0x40)
#define TDM_CTRL_REG3		(0x44)
#define TDM_CTRL_REG4		(0x48)
#define TDM_CTRL_REG5		(0x4c)
#define TDM_CTRL_REG6		(0x50)
#define TDM_CTRL_REG7		(0x54)
#define TDM_CTRL_REG8		(0x58)
#define TDM_CTRL_REG9		(0x5c)
#define TDM_CTRL_REG10		(0x60)
#define TDM_CTRL_REG11		(0x64)
#define TDM_CTRL_REG12		(0x68)
#define TDM_CTRL_REG13		(0x6c)

/* TDM Control Register 1*/
#define	TDM_EN			(1 << 20)
#define TDM_MST			(1 << 19)
#define TDM_FIFO_BYPASS		(1 << 18)
#define TDM_MNT_FIFO_EN		(1 << 17)
#define TDM_DELAY_MODE		(1 << 16)
#define TDM_DOUBLE_EDGE		(1 << 15)
#define TDM_APPLY_CKG_CONF	(1 << 14)
#define TDM_APPLY_TDM_CONF	(1 << 13)
#define TDM_SLOT_SIZE_MASK	(0x3 << 11)
#define TDM_SLOT_SIZE(x)	((x) << 11)
#define TDM_SLOT_SPACE_MASK	(0x7 << 8)
#define TDM_SLOT_SPACE(x)	((x) << 8)
#define	TDM_MAX_SLOT_MASK	(0xf << 4)
#define TDM_MAX_SLOT(x)		((x) << 4)
#define	TDM_START_SLOT_MASK	(0xf << 0)
#define TDM_START_SLOT(x)	((x) << 0)

/* TDM Control Register 2*/
#define TDM_CHO_SLOT_1_MASK	(0xffffff)
#define TDM_CHO_SLOT_1(x)	((x))

/* TDM Control Register 3*/
#define TDM_CHO_SLOT_2_MASK	(0xffffff)
#define TDM_CHO_SLOT_2(x)	((x))

/* TDM Control Register 4*/
#define TDM_CHI_SLOT_HIGH_20BIT_MASK	(0xfffff)
#define TDM_CHI_SLOT_HIGH_20BIT(x)	((x))

/* TDM Control Register 5*/
#define TDM_CHI_SLOT_LOW_32BIT_MASK	(0xffffffff)
#define TDM_CHI_SLOT_LOW_32BIT(x)	((x))

/* TDM Control Register 6*/
#define TDM_CHO_DL_MASK		0x3
#define TDM_CHO_DL_LEN		0x2
#define TDM_CHO_DL_20		0x0
#define TDM_CHO_DL_16		0x1
#define TDM_CHO_DL_24		0x2
#define TDM_CHO_DL_32		0x3

/* TDM Control Register 7*/
#define TDM_CHI_DL_MASK		0x3
#define TDM_CHI_DL_LEN		0x2
#define TDM_CHI_DL_20		0x0
#define TDM_CHI_DL_16		0x1
#define TDM_CHI_DL_24		0x2
#define TDM_CHI_DL_32		0x3

/* TDM Control Register 8*/
#define TDM_FSYN_DITH		(1 << 30)
#define TDM_FSYN_DITH_MODE	(1 << 29)
#define TDM_FSYN_POLARITY	(1 << 28)
#define	TDM_FSYN_PULSE_WIDTH_MASK	(0x3ff << 18)
#define	TDM_FSYN_PULSE_WIDTH(x)		((x) << 18)
#define TDM_FSYN_RATE_MASK	(0x3ffff)
#define TDM_FSYN_RATE(x)	(x)

/* TDM Control Register 9*/
#define TDM_BCLK_DITH		(1 << 15)
#define TDM_BCLK_DITH_MODE	(1 << 14)
#define TDM_BCLK_POLARITY	(1 << 13)
#define TDM_BCLK_RATE_MASK	(0x1fff)
#define TDM_BCLK_RATE(x)	(x)

/* SOC TDM output stream source */
enum {
	NO_OUTPUT = 0, /* default value: no output for this slot */
	OUT1_CH1, /* HEADSET_L, EARPHONE, BT SPKR */
	OUT1_CH2, /* HEADSET_R */
	OUT2_CH1, /* SPKR_L, MONO SPKR */
	OUT2_CH2, /* SPKR_R */
};

/* SOC TDM input stream source */
enum {
	NO_INPUT = 0, /* default value: no input for this slot */
	IN1_CH1, /* mic1 ch1 */
	IN1_CH2, /* mic1 ch2 */
	IN2_CH1, /* mic2 ch1 */
	IN2_CH2, /* mic2_ch2 */
};

enum tdm_out_reg {
	NO_OUT_CONTROL_REG,
	TDM_CONTRL_REG2,
	TDM_CONTRL_REG3
};

struct tdm_used_entity *get_tdm_entity(struct snd_pcm_substream *substream);
unsigned int mmp_tdm_request_slot(struct snd_pcm_substream *substream,
							int channel);
unsigned int mmp_tdm_free_slot(struct snd_pcm_substream *substream);
int mmp_tdm_static_slot_alloc(struct snd_pcm_substream *substream,
				int *tx, int tx_num, int *rx, int rx_num);
int mmp_tdm_static_slot_free(struct snd_pcm_substream *substream);
void tdm_clk_enable(struct map_private *map_priv, bool enable);
#endif /* _MMP_TDM_H */
