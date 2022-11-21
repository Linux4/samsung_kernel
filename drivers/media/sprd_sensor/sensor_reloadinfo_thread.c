/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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


#include "sensor_reloadinfo_thread.h"

int sensor_reloadinfo_thread(void *data)
{
	int ret = 0;
	Sensor_ReadOtp(data);
	msleep(500);
	sensor_identify(data);

	return ret;
}
