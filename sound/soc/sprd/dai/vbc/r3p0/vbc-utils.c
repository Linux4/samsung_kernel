/*
 * sound/soc/sprd/dai/vbc/r3p0/vbc-utils.c
 *
 * SPRD SoC VBC -- SpreadTrum SOC utils function for VBC DAI.
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/io.h>
#include <sound/soc.h>

#include "sprd-asoc-common.h"
#include <sound/vbc-utils.h>
#include <sound/audio-sipc.h>

static const char *sprd_stream_name[] = {
	"Playback",
	"Capture",
};

const char *vbc_get_stream_name(int stream)
{
	return sprd_stream_name[stream];
}

static const char *sprd_profile_name[] = {
	"audio_structure",
	"dsp_vbc",
	"nxp",
};

const char *vbc_get_profile_name(int profile_id)
{
	return sprd_profile_name[profile_id];
}

static const char *sprd_vbc_mdg_name[] = {
	"DAC0 DSP",
	"DAC1 DSP",
	"DAC0 AUD",
	"DAC1 AUD"
};

const char *vbc_get_mdg_name(int mdg_id)
{
	return sprd_vbc_mdg_name[mdg_id];
}

static const char *sprd_vbc_dg_name[] = {
	"DAC0",
	"DAC1",
	"ADC0",
	"ADC1",
	"ADC2",
	"ADC3",
	"FM",
	"ST",
	"OFFLOAD"
};

const char *vbc_get_dg_name(int dg_id)
{
	return sprd_vbc_dg_name[dg_id];
}

static const char *sprd_vbc_smthdg_name[] = {
	"DAC0",
	"DAC1"
};

const char *vbc_get_smthdg_name(int smthdg_id)
{
	return sprd_vbc_smthdg_name[smthdg_id];
}

static const char *sprd_vbc_mixerdg_name[] = {
	"DAC0",
	"DAC1"
};

const char *vbc_get_mixerdg_name(int mixerdg_id)
{
	return sprd_vbc_mixerdg_name[mixerdg_id];
}

static const char *sprd_vbc_chan_name[] = {
	"Left",
	"Right",
	"All Chan",
};

const char *vbc_get_chan_name(int chan_id)
{
	return sprd_vbc_chan_name[chan_id];
}

static const char *sprd_vbc_mux_name[] = {
	"adc0 source mux",
	"adc0 insel mux",
	"adc1 source mux",
	"adc1 insel mux",
	"adc2 source mux",
	"adc2 insel mux",
	"adc3 source mux",
	"adc3 insel mux",
	"fm insel mux",
	"st insel mux",
	"dac0 adc loop mux",
	"dac1 adc loop mux",
	"dac0/dac1 insel loop mux",
	"audrcd insel1 mux",
	//"audrcd insel2 mux",
	"dac0 outsel mux",
	"dac1 outsel mux",
	"dac0 iis sel mux",
	"dac1 iis sel mux",
	"adc0 iis sel mux",
	"adc1 iis sel mux",
	"adc2 iis sel mux",
	"adc3 iis sel mux",
};

const char *vbc_get_mux_name(int mux_id)
{
	return sprd_vbc_mux_name[mux_id];
}

static const char *sprd_adder_name[] = {
	"AUDPLY DAC0",
	"FM DAC0",
	"SMTHDG_VOICE DAC0",
	"MBDRC_VOICE DAC0",
	"ST DAC0",
	"AUDPLY DAC1",
	"FM DAC1",
	"SMTHDG_VOICE DAC1",
	"MBDRC_VOICE DAC1",
	"ST DAC1",
};

const char *vbc_get_adder_name(int adder_id)
{
	return sprd_adder_name[adder_id];
}

void __safe_mem_release(void **free)
{
	if (*free) {
		kfree(*free);
		*free = NULL;
	}
}

int vbc_sipc_parse_dt(struct audio_sipc_of_data *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val_arr[5];
	int ret = 0;
	int i;

	/* get sipc name */
	ret = of_property_read_string(np, "sprd,name", (const char **)&info->name);
	if (ret) {
		pr_err("%s, error sipc name, ret:%d \n", __func__, ret);
		return -EINVAL;
	}

	/* get cmd-para buffer information */
	if (!of_property_read_u32_array(np, "sprd,cmdaddr", &val_arr[0], 2)) {
		info->cmdaddr_base= val_arr[0];
		info->cmdaddr_size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the cmdaddr!\n", __func__);
		return -EINVAL;
	}
	/* get smsg buffer information */
	if (!of_property_read_u32_array(np, "sprd,smsg_txaddr", &val_arr[0], 2)) {
		info->smsg_txbase = val_arr[0];
		info->smsg_txsize = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the txaddr!\n", __func__);
		return -EINVAL;
	}
	if (!of_property_read_u32_array(np, "sprd,smsg_rxsize", &val_arr[0], 1)) {
		info->smsg_rxsize = val_arr[0];
	} else {
		pr_err("%s, ERR:Must give me the rxaddr!\n", __func__);
		return -EINVAL;
	}
	/* get mailbox targe core id */
#ifdef CONFIG_SPRD_MAILBOX
	ret = of_property_read_u32(np, "mailbox,core", &info->target_id);
	if (ret) {
		pr_err("%s, error target core id, ret:%d \n", __func__, ret);
		return -EIO;
	}
#endif
	/* get sharemem buffer information */
	if (!of_property_read_u32_array(np, "sprd,shmaddr", &val_arr[0], SND_VBC_SHM_MAX+1)) {
		info->shm_base= val_arr[0];
		for (i = 0; i < SND_VBC_SHM_MAX; i++) {
				info->shm_size[i] = val_arr[i+1];
				printk(KERN_EMERG "info->shm_size[i(%d)]=%#x\n", i, info->shm_size[i]);
		}
	} else {
		pr_err("%s, ERR:Must give me the shm addr!\n", __func__);
		return -EINVAL;
	}

	return 0;
}
