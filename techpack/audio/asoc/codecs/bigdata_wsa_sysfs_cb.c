/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include <dsp/sp_params.h>
#include "bigdata_wsa_sysfs_cb.h"

#define Q22 (1<<22)
#define Q13 (1<<13)
#define Q7 (1<<7)

static struct snd_soc_component *wsa_component;
struct afe_spk_ctl *wsa_bd;

static int get_wsa_amp_temperature_max(enum amp_id id)
{
	struct snd_soc_component *component = wsa_component;
	int value = 0;

	if (!component || !wsa_bd) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = wsa_bd->xt_logging.max_temperature[id]/Q22;
	wsa_bd->xt_logging.max_temperature[id] = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_wsa_amp_temperature_keep_max(enum amp_id id)
{
	struct snd_soc_component *component = wsa_component;
	int value = 0;

	if (!component || !wsa_bd) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = wsa_bd->max_temperature_rd[id]/Q22;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_wsa_amp_temperature_overcount(enum amp_id id)
{
	struct snd_soc_component *component = wsa_component;
	int value = 0;

	if (!component || !wsa_bd) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = wsa_bd->xt_logging.count_exceeded_temperature[id];
	wsa_bd->xt_logging.count_exceeded_temperature[id] = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_wsa_amp_excursion_max(enum amp_id id)
{
	struct snd_soc_component *component = wsa_component;
	int value = 0;
	int32_t ex_q27 = 0;

	if (!component || !wsa_bd) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	ex_q27 = wsa_bd->xt_logging.max_excursion[id];
	value = ex_q27/Q13;
	value = (value * 10000)/(Q7 * Q7);
	value /= 100;
	wsa_bd->xt_logging.max_excursion[id] = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_wsa_amp_excursion_overcount(enum amp_id id)
{
	struct snd_soc_component *component = wsa_component;
	int value = 0;

	if (!component || !wsa_bd) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id < 0 || id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = wsa_bd->xt_logging.count_exceeded_excursion[id];
	wsa_bd->xt_logging.count_exceeded_excursion[id] = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

void register_wsa_bd_cb(struct snd_soc_component *component)
{
	wsa_component = component;
	wsa_bd = get_wsa_sysfs_ptr();

	dev_info(component->dev, "%s\n", __func__);

	audio_register_temperature_max_cb(get_wsa_amp_temperature_max);
	audio_register_temperature_keep_max_cb(get_wsa_amp_temperature_keep_max);
	audio_register_temperature_overcount_cb(get_wsa_amp_temperature_overcount);
	audio_register_excursion_max_cb(get_wsa_amp_excursion_max);
	audio_register_excursion_overcount_cb(get_wsa_amp_excursion_overcount);
}
EXPORT_SYMBOL_GPL(register_wsa_bd_cb);
