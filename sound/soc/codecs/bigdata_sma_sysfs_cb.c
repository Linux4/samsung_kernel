/*
 *  bigdata_sma_sysfs_cb.c
 *  Copyright (c) Iron Device Corporation
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 */

#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "bigdata_sma_sysfs_cb.h"
#include "sma1305.h"
#include <sound/ff_prot_spk.h>

static struct snd_soc_component *sma1305_component;

static int get_sma1305_amp_curr_temperature(enum amp_id id)
{
	struct snd_soc_component *component = sma1305_component;
	int value = 0;
	uint8_t iter = 0;
	int32_t data[8] = {0};
	uint32_t param_id = 0;
	int32_t ret = 0;

	if (!component) {
		dev_err(component->dev, "%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	if (get_amp_pwr_status()) {
		/* Reset data */
		memset(data, 0, 8*sizeof(int32_t));
		param_id = (SMA_APS_TEMP_CURR)|((iter+1)<<24)|
			((sizeof(data)/sizeof(int32_t))<<16);
		ret = afe_ff_prot_algo_ctrl((int *)(&(data[0])), param_id,
				SMA_GET_PARAM, sizeof(data));
		if (ret < 0) {
			dev_err(component->dev,
				"%s: Failed to get Current Temperature",
				__func__);
		} else
			value = data[1];
	} else {
		dev_info(component->dev, "%s: amp off\n", __func__);
		return -EINVAL;
	}

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int set_sma1305_amp_surface_temperature(enum amp_id id, int temperature)
{
	struct snd_soc_component *component = sma1305_component;
	int ret = 0;
	uint8_t iter = 0;
	uint32_t param_id = 0;
	int surface_temp = temperature;

	if (!component) {
		dev_err(component->dev, "%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	if (get_amp_pwr_status()) {
		param_id = (SMA_APS_SURFACE_TEMP)|((iter+1)<<24);
		ret = afe_ff_prot_algo_ctrl(&surface_temp, param_id,
				SMA_SET_PARAM, sizeof(int));
		if (ret < 0)
			dev_err(component->dev,
				"%s: Failed to set Surface Temperature",
				__func__);
	} else {
		dev_info(component->dev, "%s: amp off\n", __func__);
		return -EINVAL;
	}

	dev_info(component->dev,
		"%s: id %d temperature %d\n", __func__, id, temperature);

	return ret;
}

void register_sma1305_bigdata_cb(struct snd_soc_component *component)
{
	sma1305_component = component;
	dev_info(component->dev, "%s\n", __func__);

	audio_register_curr_temperature_cb(get_sma1305_amp_curr_temperature);
	audio_register_surface_temperature_cb(set_sma1305_amp_surface_temperature);
}
EXPORT_SYMBOL(register_sma1305_bigdata_cb);
