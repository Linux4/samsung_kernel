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
#include <linux/platform_device.h>
#include <linux/device.h>

#define SSP_BOOTLOADER_FILE     "sensorhub/shub_neus_bl.bin"

int ssp_device_probe(struct platform_device *pdev);
void ssp_device_remove(struct platform_device *pdev);
int ssp_device_suspend(struct device *dev);
void ssp_device_resume(struct device *dev);

void ssp_platform_init(void*);
void ssp_handle_recv_packet(char*, int);
void ssp_platform_start_refrsh_task(void);


int sensorhub_reset(void *ssp_data);
int sensorhub_firmware_download(void *ssp_data);

int sensorhub_comms_write(struct platform_device *pdev, u8 *buf, int length, int timeout);

void save_ram_dump(void* ssp_data);
int get_sensorhub_dump_size(void);
void* get_sensorhub_dump_address(void);
void ssp_dump_write_file(void);

bool is_sensorhub_working(void *ssp_data);

void ssp_set_firmware_name(const char *fw_name);

void sensorhub_fs_ready(void *ssp_data);
int sensorhub_platform_probe(void *ssp_data);
int sensorhub_platform_shutdown(void *ssp_data);
int sensorhub_refresh_func(void *ssp_data);

#endif /* __SSP_PLATFORM_H__ */
