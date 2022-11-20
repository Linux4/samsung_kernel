// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _FF_PROT_SPK_H
#define _FF_PROT_SPK_H

#include <linux/types.h>
#include <sound/samsung/abox.h>
#include <sound/soc.h>
#include <linux/of.h>

#define FF_PROT_SPK

/* Below should be same as in aDSP code */
#define IRON_DEVICE_VENDOR_ID	8282

#define SMA_PAYLOAD_SIZE    14
#define SMA_GET_PARAM		1
#define SMA_SET_PARAM		0

#define SMA_APS_TEMP_STAT   1000

#define MAX_CHANNELS    2
#define LEFT			0
#define RIGHT			1
#define CHANNEL0		1
#define CHANNEL1		2

#define TRUE		1
#define FALSE		0

#define WR_DATA					0
#define RD_DATA					1

struct ff_prot_spk_data {
	uint32_t payload[SMA_PAYLOAD_SIZE];
};

int ff_prot_spk_read(void *prm_data, int offset, int size);
int ff_prot_spk_write(void *prm_data, int offset, int size);
#ifdef CONFIG_SND_SOC_APS_ALGO
void sma_amp_update_big_data(void);
#endif

#endif /*_FF_PROT_SPK_H*/
