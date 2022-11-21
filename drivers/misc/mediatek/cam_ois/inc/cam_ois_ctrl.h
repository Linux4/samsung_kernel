/*
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _CAM_OIS_CTRL_H
#define _CAM_OIS_CTRL_H

#include <linux/i2c.h>
#include "cam_ois_info.h"

void set_global_ois_i2c_client(struct i2c_client *client);
struct i2c_client *get_ois_i2c_client(enum OIS_I2C_DEV_IDX ois_i2c_ch);
int ois_cam_i2c_write(u8 *write_buf, u16 length, u16 write_per_cycle, int speed);
int ois_cam_i2c_read(u16 addr, u32 length, u8 *read_buf, int speed);

#endif /*_CAM_OIS_CTRL_H*/
