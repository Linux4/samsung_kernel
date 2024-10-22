// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 */

#define pr_fmt(fmt) "[D/D]" fmt

#include <linux/module.h>
#include "imgsensor_otp_cal.h"
#include "imgsensor_sysfs.h"

unsigned int imgsensor_read_otp_cal(struct i2c_client *client, unsigned int sensor_id,
		unsigned int addr, unsigned char *data, unsigned int size)
{
	int ret = 0;

	pr_info("[%s]- E, sensor_id: %#06x\n", __func__, sensor_id);
	switch (sensor_id) {
#if defined(HI847_MIPI_RAW)
	case HI847_SENSOR_ID:
		ret = hi847_read_otp_cal(client, data, size);
		break;
#endif
#if defined(HI1337FU_MIPI_RAW)
	case HI1337FU_SENSOR_ID:
		ret = hi1337fu_read_otp_cal(client, data, size);
		break;
#endif
	default:
		pr_err("[%s] no searched otp cal\n", __func__);
		ret = -1;
		break;
	}

	if (ret < 0) {
		pr_err("[%s] fail\n", __func__);
		return 0;
	}

	pr_info("[%s] - X\n", __func__);
	return ret;
}
EXPORT_SYMBOL(imgsensor_read_otp_cal);
