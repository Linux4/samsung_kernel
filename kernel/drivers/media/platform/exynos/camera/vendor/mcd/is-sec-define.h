/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_SEC_DEFINE_H
#define IS_SEC_DEFINE_H

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/zlib.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"

#include "is-device-sensor.h"
#include "is-device-ischain.h"
#include "crc32.h"
#include "is-dt.h"
#include "is-device-ois.h"
#include "is-device-rom.h"

struct dualize_match_entry {
	char *module_header;
	int sensor_id;
	char *sensor_name;
};

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
ssize_t read_data_rom_file(char *name, char *buf, size_t count, loff_t *pos);
bool is_sec_file_exist(char *name);
int is_sec_get_rom_info(struct is_vendor_rom **rom_info, int rom_id);
int is_sec_check_dualize(int position);
int is_sec_run_fw_sel(int rom_id);
int is_sec_compare_ver(int rom_id);
bool is_sec_check_rom_ver(struct is_core *core, int rom_id);
bool is_sec_check_cal_crc32(char *buf, int id);
bool is_sec_check_awb_lsc_crc32_post_sec2lsi(char *buf, int position);
int is_sec_rom_power_on(struct is_core *core, int position);
int is_sec_rom_power_off(struct is_core *core, int position);
void remove_dump_fw_file(void);
int is_sec_sensor_find_front_tof_mode_id(struct is_core *core, char *buf);
int is_sec_sensor_find_rear_tof_mode_id(struct is_core *core, char *buf);
int is_sec_read_rom(int rom_id);
void is_sec_select_dualized_sensor(int position);
int is_sec_cal_reload(void);

#endif /* IS_SEC_DEFINE_H */
