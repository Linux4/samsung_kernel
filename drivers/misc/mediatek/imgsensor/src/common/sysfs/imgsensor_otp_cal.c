// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 */

#include "imgsensor_otp_cal.h"
#include "imgsensor_sensor.h"
#include "imgsensor_sysfs.h"

extern struct IMGSENSOR_SENSOR *imgsensor_sensor_get_inst(enum IMGSENSOR_SENSOR_IDX idx);
unsigned int imgsensor_read_otp_cal(unsigned int dual_device_id, unsigned int sensor_id,
		unsigned int addr, unsigned char *data, unsigned int size)
{
	int ret = 0;
	struct IMGSENSOR_SENSOR *psensor = NULL;

	psensor = imgsensor_sensor_get_inst(IMGSENSOR_SENSOR_IDX_MAP(dual_device_id));
	if (psensor != NULL)
		imgsensor_i2c_set_device(&psensor->inst.i2c_cfg);

	pr_info("[%s]- E, sensor_id: %#06x", __func__, sensor_id);
	switch (sensor_id) {
#if defined(GC5035_MIPI_RAW)
	case GC5035_SENSOR_ID:
		ret = gc5035_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(GC5035B_MIPI_RAW)
	case GC5035B_SENSOR_ID:
		ret = gc5035b_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(GC5035U_MIPI_RAW)
	case GC5035U_SENSOR_ID:
		ret = gc5035_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(GC5035F_MIPI_RAW)
	case GC5035F_SENSOR_ID:
		ret = gc5035_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(S5K3L6_MIPI_RAW)
	case S5K3L6_SENSOR_ID:
		ret = s5k3l6_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(SR846D_MIPI_RAW)
	case SR846D_SENSOR_ID:
		ret = sr846d_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(GC02M1B_MIPI_MONO)
	case GC02M1B_SENSOR_ID:
		ret = gc02m1_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(GC02M1B_MIPI_RAW)
	case GC02M1B_SENSOR_ID:
		ret = gc02m1_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(HI2021Q_MIPI_RAW)
	case HI2021Q_SENSOR_ID:
		ret = hi2021q_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(S5K4HAYXF_MIPI_RAW)
	case S5K4HAYXF_SENSOR_ID:
		ret = s5k4hayx_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(S5K4HAYX_MIPI_RAW)
	case S5K4HAYX_SENSOR_ID:
		ret = s5k4hayx_read_otp_cal(addr, data, size);
		break;
#endif
#if defined(HI1339_MIPI_RAW)
	case HI1339_SENSOR_ID:
		ret = hi1339_read_otp_cal(addr, data, size);
		break;
#endif
	default:
		pr_err("[%s] no searched otp cal\n", __func__);
		ret = 0;
		break;
	}

	if (psensor != NULL)
		imgsensor_i2c_set_device(NULL);

	if (ret < 0) {
		pr_err("[%s] fail\n", __func__);
		return 0;
	}

	pr_info("[%s] - X\n", __func__);
	return ret;
}
