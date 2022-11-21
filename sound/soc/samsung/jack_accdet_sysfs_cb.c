/*
 *  jack_accdet_sysfs_cb.c
 *  Copyright (c) Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/printk.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "jack_accdet_sysfs_cb.h"

static struct accdet_data *accdet_data;

static int get_jack_status(void)
{
	int status = accdet_data->jack_state;
	int report = 0;

	if (status) {
		report = 1;
	}

	pr_info("%s: %d\n", __func__, report);

	return report;
}

static int get_key_status(void)
{
	int report = 0;

	report = accdet_data->key_state;

	pr_info("%s: %d\n", __func__, report);

	return report;
}

static int get_mic_adc(void)
{
	int adc = 0;

	adc = accdet_data->mic_adc;

	pr_info("%s: %d\n", __func__, adc);

	return adc;
}

void register_accdet_jack_cb(struct accdet_data *accdet_pdata)
{
	accdet_data = accdet_pdata;

	pr_info("%s\n", __func__);

	audio_register_jack_state_cb(get_jack_status);
	audio_register_key_state_cb(get_key_status);
	audio_register_mic_adc_cb(get_mic_adc);
}
EXPORT_SYMBOL_GPL(register_accdet_jack_cb);