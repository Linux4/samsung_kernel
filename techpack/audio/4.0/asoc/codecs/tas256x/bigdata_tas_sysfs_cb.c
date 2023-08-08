/*
 *  bigdata_cirrus_sysfs_cb.c
 *  Copyright (c) Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 */

#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "algo/inc/tas_smart_amp_v2.h"
#include "algo/src/tas25xx-calib-validation.h"
#include "bigdata_tas_sysfs_cb.h"

static struct snd_soc_component *tas25xx_component;
struct tas25xx_algo *tas25xx_bd;

static uint8_t trans_val_to_user_m(uint32_t val, uint8_t qformat)
{
	uint32_t ret = (uint32_t)(((long long)val * 1000) >> qformat) % 1000;

	return (uint8_t)(ret / 10);
}

static uint8_t trans_val_to_user_i(uint32_t val, uint8_t qformat)
{
	return ((val * 100) >> qformat) / 100;
}

static int get_tas25xx_amp_temperature_max(enum amp_id id)
{
	struct snd_soc_component *component = tas25xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  MAX_CHANNELS) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tas25xx_bd->b_data[id].temp_max;
	tas25xx_bd->b_data[id].temp_max = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tas25xx_amp_temperature_keep_max(enum amp_id id)
{
	struct snd_soc_component *component = tas25xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  MAX_CHANNELS) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tas25xx_bd->b_data[id].temp_max_persist;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tas25xx_amp_temperature_overcount(enum amp_id id)
{
	struct snd_soc_component *component = tas25xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  MAX_CHANNELS) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tas25xx_bd->b_data[id].temp_over_count;
	tas25xx_bd->b_data[id].temp_over_count = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tas25xx_amp_excursion_max(enum amp_id id)
{
	struct snd_soc_component *component = tas25xx_component;
	int value = 0;
	int value_i = 0;
	int value_m = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  MAX_CHANNELS) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tas25xx_bd->b_data[id].exc_max;
	tas25xx_bd->b_data[id].exc_max = 0;

	value_i = (int32_t)trans_val_to_user_i(value, QFORMAT31);
	value_m	= (int32_t)trans_val_to_user_m(value, QFORMAT31);
	value = (value_i * 100) + value_m;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tas25xx_amp_excursion_overcount(enum amp_id id)
{
	struct snd_soc_component *component = tas25xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  MAX_CHANNELS) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tas25xx_bd->b_data[id].exc_over_count;
	tas25xx_bd->b_data[id].exc_over_count = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

void register_tas25xx_bigdata_cb(struct snd_soc_component *component)
{
	tas25xx_component = component;
	tas25xx_bd = smartamp_get_sysfs_ptr();

	dev_info(component->dev, "%s\n", __func__);

	audio_register_temperature_max_cb(get_tas25xx_amp_temperature_max);
	audio_register_temperature_keep_max_cb(get_tas25xx_amp_temperature_keep_max);
	audio_register_temperature_overcount_cb(get_tas25xx_amp_temperature_overcount);
	audio_register_excursion_max_cb(get_tas25xx_amp_excursion_max);
	audio_register_excursion_overcount_cb(get_tas25xx_amp_excursion_overcount);
}
EXPORT_SYMBOL_GPL(register_tas25xx_bigdata_cb);
