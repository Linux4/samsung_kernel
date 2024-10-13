/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SYSFS_HW_BIGDATA_H_
#define _CAM_SYSFS_HW_BIGDATA_H_

#include <linux/sysfs.h>
#include "cam_sensor_cmn_header.h"

extern const struct device_attribute *hwb_rear_attrs[];
extern const struct device_attribute *hwb_front_attrs[];

int camera_hw_param_buf_init(void);
void camera_hw_param_buf_exit(void);
void camera_hw_param_check_avail_cam(void);
ssize_t fill_hw_bigdata_sysfs_node(char *buf, struct cam_hw_param *ec_param, char *moduleid, uint32_t hw_param_id);
int16_t is_hw_param_valid_module_id(char *moduleid);

#endif /* _CAM_SYSFS_HW_BIGDATA_H_ */
