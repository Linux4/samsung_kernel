/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef WCD9306_H
#define WCD9306_H

#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>
#include "wcd9xxx-mbhc.h"
#include "wcd9xxx-resmgr.h"

#define TAPAN_NUM_REGISTERS 0x400
#define TAPAN_MAX_REGISTER (TAPAN_NUM_REGISTERS-1)
#define TAPAN_CACHE_SIZE TAPAN_NUM_REGISTERS

#define TAPAN_REG_VAL(reg, val)		{reg, 0, val}
#define TAPAN_MCLK_ID 0

#define TAPAN_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
				SND_JACK_BTN_2 | SND_JACK_BTN_3 | \
				SND_JACK_BTN_4 | SND_JACK_BTN_5 | \
				SND_JACK_BTN_6 | SND_JACK_BTN_7)

extern const u8 tapan_reg_readable[TAPAN_CACHE_SIZE];
extern const u8 tapan_reset_reg_defaults[TAPAN_CACHE_SIZE];
struct tapan_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};

enum tapan_pid_current {
	TAPAN_PID_MIC_2P5_UA,
	TAPAN_PID_MIC_5_UA,
	TAPAN_PID_MIC_10_UA,
	TAPAN_PID_MIC_20_UA,
};

struct tapan_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

enum tapan_mbhc_analog_pwr_cfg {
	TAPAN_ANALOG_PWR_COLLAPSED = 0,
	TAPAN_ANALOG_PWR_ON,
	TAPAN_NUM_ANALOG_PWR_CONFIGS,
};

/* Number of input and output Slimbus port */
enum {
	TAPAN_RX1 = 0,
	TAPAN_RX2,
	TAPAN_RX3,
	TAPAN_RX4,
	TAPAN_RX5,
	TAPAN_RX6,
	TAPAN_RX7,
	TAPAN_RX8,
	TAPAN_RX9,
	TAPAN_RX10,
	TAPAN_RX11,
	TAPAN_RX12,
	TAPAN_RX13,
	TAPAN_RX_MAX,
};

enum {
	TAPAN_TX1 = 0,
	TAPAN_TX2,
	TAPAN_TX3,
	TAPAN_TX4,
	TAPAN_TX5,
	TAPAN_TX6,
	TAPAN_TX7,
	TAPAN_TX8,
	TAPAN_TX9,
	TAPAN_TX10,
	TAPAN_TX11,
	TAPAN_TX12,
	TAPAN_TX13,
	TAPAN_TX14,
	TAPAN_TX15,
	TAPAN_TX16,
	TAPAN_TX_MAX,
};

struct anc_header {
	u32 reserved[3];
	u32 num_anc_slots;
};

extern int tapan_mclk_enable(struct snd_soc_codec *codec, int mclk_enable,
			     bool dapm);
extern int tapan_hs_detect(struct snd_soc_codec *codec,
			   struct wcd9xxx_mbhc_config *mbhc_cfg);
extern void tapan_register_mclk_cb(struct snd_soc_codec *codec,
			int (*mclk_cb_fn) (struct snd_soc_codec*, int, bool));

#endif
