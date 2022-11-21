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

#ifndef __SSP_PLATFORM_H__
#define __SSP_PLATFORM_H__


#include <linux/kernel.h>


#define SSP_BOOTLOADER_FILE	"sensorhub/shub_bl.fw"
#define SSP_UPDATE_BIN_FILE	"shub.bin"

void ssp_platform_init(void *ssp_data, void *);
//void ssp_handle_recv_packet(void *ssp_data, char *, int);
//void ssp_platform_start_refrsh_task(void *ssp_data);

int sensorhub_platform_probe(void *ssp_data);
int sensorhub_platform_shutdown(void *ssp_data);

void sensorhub_fs_ready(void *ssp_data);
int sensorhub_reset(void *ssp_data);
int sensorhub_firmware_download(void *ssp_data);

int sensorhub_comms_write(void* ssp_data, u8* buf, int length, int timeout);

bool is_sensorhub_working(void *ssp_data);

int sensorhub_refresh_func(void *ssp_data);
#endif /* __SSP_PLATFORM_H__ */
