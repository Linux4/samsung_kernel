/*
 *  sec_wcd_sysfs_cb.c
 *
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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "sec_accdet_sysfs_cb.h"
#include "mt6357-accdet.h"

static struct jack_pdata *jack_pdata;

static int get_jack_status(void)
{
	int value = 0;

	if ((jack_pdata->hph_status == HEADSET_MIC) ||
	    (jack_pdata->hph_status == HEADSET_NO_MIC))
		value = 1;

	pr_info("%s: %d\n", __func__, value);

	return value;
}

static int get_key_status(void)
{
	int value = -1;

	if (jack_pdata->buttons_pressed)
		value = jack_pdata->buttons_pressed;

	pr_info("%s: %d\n", __func__, value);

	return value;
}

static int get_mic_adc(void)
{
	int value = -1;

	value = accdet_get_adc();

	pr_info("%s: %d\n", __func__, value);

	return value;
}

void register_accdet_jack_cb(struct jack_pdata *accdet_pdata)
{
	jack_pdata = accdet_pdata;

	audio_register_jack_state_cb(get_jack_status);
	audio_register_key_state_cb(get_key_status);
	audio_register_mic_adc_cb(get_mic_adc);
}
EXPORT_SYMBOL_GPL(register_accdet_jack_cb);

