/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "ssp.h"

#if defined(CONFIG_SENSORS_SSP_CANVAS)
#define SSP_FIRMWARE_REVISION_BCM_OLD	20062201		// bcm4776 (rev:17 ~ 20)
#define SSP_FIRMWARE_REVISION_BCM_Q		20102900		// bcm4775 (rev:~16, 21~)
#define SSP_FIRMWARE_REVISION_BCM_R		22112300		// bcm4775 (rev:~16, 21~)
#elif defined(CONFIG_SENSORS_SSP_PICASSO)
#define SSP_FIRMWARE_REVISION_BCM_Q	20110200
#define SSP_FIRMWARE_REVISION_BCM_R	22112300
#elif defined(CONFIG_SENSORS_SSP_R8)
#define SSP_FIRMWARE_REVISION_BCM_Q	21011900
#define SSP_FIRMWARE_REVISION_BCM_R	22112300
#else
#define SSP_FIRMWARE_REVISION_BCM_Q	00000000
#define SSP_FIRMWARE_REVISION_BCM_R	00000000
#endif

unsigned int get_module_rev(struct ssp_data *data)
{
	unsigned int version = 00000000;
	switch(android_version){
		case 10:
			version = SSP_FIRMWARE_REVISION_BCM_Q;
			break;
		case 11:
			version = SSP_FIRMWARE_REVISION_BCM_R;
			break;
		case 12:
			version = SSP_FIRMWARE_REVISION_BCM_R;
			break;
		default:
			pr_err("%s : unknown android_version: %d (default,%d)", __func__, android_version, SSP_FIRMWARE_REVISION_BCM_R);
			version = SSP_FIRMWARE_REVISION_BCM_R;
			break;
	}
#if defined(CONFIG_SENSORS_SSP_CANVAS)
	if(get_patch_version(data->ap_type, data->ap_rev) == bbd_old)
		return SSP_FIRMWARE_REVISION_BCM_OLD;
#endif
	return version;
}
