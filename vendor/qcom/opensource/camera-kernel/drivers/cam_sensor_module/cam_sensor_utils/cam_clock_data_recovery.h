/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "cam_sensor_dev.h"
#include <linux/ktime.h>

#ifndef _CAM_CDR_TEST_UTIL_H_
#define _CAM_CDR_TEST_UTIL_H_

struct cam_clock_data_recovery_info {
	int is_requested;
	uint64_t timestamp[2];
	char value[50];
	char result[40];
};

enum cam_clock_data_recovery_error {
	CDR_ERROR_MIPI = 0,
	CDR_ERROR_I2C,
	MAX_CDR_ERROR,
};

enum cam_clock_data_recovery_timestamp {
	CDR_START_TS = 0,
	CDR_END_TS,
	MAX_CDR_TS,
};

int cam_clock_data_recovery_write_register(void __iomem *csiphybase);

void cam_clock_data_recovery_set_value(const char* buf);
char* cam_clock_data_recovery_get_value(void);

void cam_clock_data_recovery_set_result(enum cam_clock_data_recovery_error error_type);
char* cam_clock_data_recovery_get_result(void);

int cam_clock_data_recovery_is_requested(void);
void cam_clock_data_recovery_reset_request(void);
void cam_clock_data_recovery_get_timestamp(enum cam_clock_data_recovery_timestamp type);


#endif /* _CAM_CDR_TEST_UTIL_H_ */
