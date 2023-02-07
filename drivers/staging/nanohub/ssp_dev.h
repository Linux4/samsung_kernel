/*
 *  Copyright (C) 2018, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef __SSP_DEV_H__
#define __SSP_DEV_H__

#include "ssp.h"

struct ssp_data *get_ssp_data(void);

int sync_sensor_data(struct ssp_data *data);
int open_sensor_calibration_data(struct ssp_data *data);

int queue_refresh_task(struct ssp_data *data, int delay);
int queue_power_on_task(struct ssp_data *data, int delay);

int ssp_probe(struct platform_device *);
void ssp_shutdown(struct platform_device *);
int ssp_suspend(struct device *);
void ssp_resume(struct device *);
#endif