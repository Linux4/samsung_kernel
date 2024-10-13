// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
#include <linux/kthread.h>
#endif

#include <exynos-is-sensor.h>
#include "is-device-sensor-peri.h"
#include "is-interface.h"
#include "is-sec-define.h"
#include "is-device-ischain.h"
#include "is-dt.h"
#include "is-device-ois.h"
#include "is-vendor-ois.h"
#include "is-vendor-private.h"
#ifdef CONFIG_AF_HOST_CONTROL
#include "is-device-af.h"
#endif
#include <linux/pinctrl/pinctrl.h>
#include "is-core.h"
#include "is-vendor-aois.h"
#include "is-interface-aois.h"
#include "is-ixc-config.h"

#define AOIS_NAME "Advanced OIS"

static int ois_shift_x[POSITION_NUM];
static int ois_shift_y[POSITION_NUM];
#ifdef OIS_CENTERING_SHIFT_ENABLE
static int ois_centering_shift_x;
static int ois_centering_shift_y;
static int ois_centering_shift_x_rear2;
static int ois_centering_shift_y_rear2;
static bool ois_centering_shift_enable;
#endif
static int ois_shift_x_rear2[POSITION_NUM];
static int ois_shift_y_rear2[POSITION_NUM];
#ifdef USE_OIS_SLEEP_MODE
static bool ois_wide_start;
static bool ois_tele_start;
#else
static bool ois_wide_init;
static bool ois_tele_init;
#endif
static bool ois_hw_check;
static bool ois_fadeupdown;
static u16 ois_center_x;
static u16 ois_center_y;
extern struct is_ois_info ois_minfo;
extern struct is_ois_info ois_pinfo;
extern struct is_ois_info ois_uinfo;
extern struct is_ois_exif ois_exif_data;
static struct mcu_default_data mcu_init;

struct is_mcu *is_get_mcu(struct is_core *core)
{
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	u32 sensor_idx = vendor_priv->mcu_sensor_index;

	if (core->sensor[sensor_idx].mcu != NULL)
		return core->sensor[sensor_idx].mcu;

	return NULL;
};

bool is_mcu_fw_version(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 hwver[4] = {0, };
	u8 vdrinfo[4] = {0, };
	struct is_mcu *mcu = NULL;
	struct is_ois_info *ois_minfo = NULL;
	u16 reg;

	info("%s started", __func__);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("mcu is NULL");
		return -EINVAL;
	}

	reg = OIS_CMD_HW_VERSION;
	ret = is_ois_read_multi(reg, &hwver[0], 4);
	if (ret) {
		MCU_GET_ERR_PRINT(reg);
		goto exit;
	}

	reg = OIS_CMD_VDR_VERSION;
	ret = is_ois_read_multi(reg, &vdrinfo[0], 4);
	if (ret) {
		MCU_GET_ERR_PRINT(reg);
		goto exit;
	}

	is_ois_get_module_version(&ois_minfo);

	memcpy(&mcu->vdrinfo_mcu[0], &vdrinfo[0], 4);
	mcu->hw_mcu[0] = hwver[3];
	mcu->hw_mcu[1] = hwver[2];
	mcu->hw_mcu[2] = hwver[1];
	mcu->hw_mcu[3] = hwver[0];
	memcpy(ois_minfo->header_ver, &mcu->hw_mcu[0], 4);
	memcpy(&ois_minfo->header_ver[4], &vdrinfo[0], 4);

	info("[%s] mcu module hw ver = %c%c%c%c, vdrinfo ver = %c%c%c%c", __func__,
		hwver[3], hwver[2], hwver[1], hwver[0], vdrinfo[0], vdrinfo[1], vdrinfo[2], vdrinfo[3]);

	return true;

exit:
	return false;
}

int is_mcu_fw_revision_vdrinfo(u8 *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_RELEASE_YEAR] - 58) * 10000;
	revision = revision + ((int)fw_ver[FW_RELEASE_MONTH] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_RELEASE_COUNT] - 48) * 10;
	revision = revision + (int)fw_ver[FW_RELEASE_COUNT + 1] - 48;

	return revision;
}

bool is_mcu_version_compare(u8 *fw_ver1, u8 *fw_ver2)
{
	if (fw_ver1[FW_DRIVER_IC] != fw_ver2[FW_DRIVER_IC]
		|| fw_ver1[FW_GYRO_SENSOR] != fw_ver2[FW_GYRO_SENSOR]
		|| fw_ver1[FW_MODULE_TYPE] != fw_ver2[FW_MODULE_TYPE]
		|| fw_ver1[FW_PROJECT] != fw_ver2[FW_PROJECT]) {
		return false;
	}

	return true;
}

void is_mcu_fw_update(struct is_core *core)
{
	int ret = 0;
	int vdrinfo_bin = 0;
	int vdrinfo_mcu = 0;

	struct is_mcu *mcu = NULL;
	struct v4l2_subdev *subdev = NULL;

	mcu = is_get_mcu(core);
	subdev = mcu->subdev;

	info("%s started", __func__);

	msleep(30);

	ret = is_mcu_fw_version(subdev);
	if (ret) {
#ifdef CONFIG_CHECK_HW_VERSION_FOR_MCU_FW_UPLOAD
		int isUpload = 0;

		if (!is_mcu_version_compare(mcu->hw_bin, mcu->hw_mcu))
			isUpload = 1;

		info("HW binary ver = %c%c%c%c, module ver = %c%c%c%c",
			mcu->hw_bin[0], mcu->hw_bin[1], mcu->hw_bin[2], mcu->hw_bin[3],
			mcu->hw_mcu[0], mcu->hw_mcu[1], mcu->hw_mcu[2], mcu->hw_mcu[3]);

		vdrinfo_bin = is_mcu_fw_revision_vdrinfo(mcu->vdrinfo_bin);
		vdrinfo_mcu = is_mcu_fw_revision_vdrinfo(mcu->vdrinfo_mcu);

		if (vdrinfo_bin > vdrinfo_mcu)
			isUpload = 1;

		info("VDRINFO binary ver = %c%c%c%c, module ver = %c%c%c%c",
			mcu->vdrinfo_bin[0], mcu->vdrinfo_bin[1], mcu->vdrinfo_bin[2], mcu->vdrinfo_bin[3],
			mcu->vdrinfo_mcu[0], mcu->vdrinfo_mcu[1], mcu->vdrinfo_mcu[2], mcu->vdrinfo_mcu[3]);

		if (isUpload)
			info("Update MCU firmware!!");
		else {
			info("Do not update MCU firmware");
		}
#else
		if (!is_mcu_version_compare(mcu->hw_bin, mcu->hw_mcu)) {
			info("Do not update MCU firmware. HW binary ver = %c%c%c%c, module ver = %c%c%c%c",
				mcu->hw_bin[0], mcu->hw_bin[1], mcu->hw_bin[2], mcu->hw_bin[3],
				mcu->hw_mcu[0], mcu->hw_mcu[1], mcu->hw_mcu[2], mcu->hw_mcu[3]);
		}

		vdrinfo_bin = is_mcu_fw_revision_vdrinfo(mcu->vdrinfo_bin);
		vdrinfo_mcu = is_mcu_fw_revision_vdrinfo(mcu->vdrinfo_mcu);

		if (vdrinfo_bin <= vdrinfo_mcu) {
			info("Do not update MCU firmware. VDRINFO binary ver = %c%c%c%c, module ver = %c%c%c%c",
				mcu->vdrinfo_bin[0], mcu->vdrinfo_bin[1], mcu->vdrinfo_bin[2], mcu->vdrinfo_bin[3],
				mcu->vdrinfo_mcu[0], mcu->vdrinfo_mcu[1], mcu->vdrinfo_mcu[2], mcu->vdrinfo_mcu[3]);
		}
#endif
	}

	msleep(50);

	info("%s end", __func__);
}

bool is_ois_sine_wavecheck_mcu(struct is_core *core,
	int threshold, int *sinx, int *siny, int *result)
{
	u8 buf = 0, val = 0;
	int ret = 0, retries = 10;
	int sinx_count = 0, siny_count = 0;
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	struct is_mcu *mcu = NULL;
	u16 reg;

	mcu = is_get_mcu(core);

	msleep(100);

	info("%s autotest started", __func__);

	cam_ois_set_aois_fac_mode_on();

	ret = is_ois_write_u8(OIS_CMD_THRESH_ERR_LEV, (u8)threshold); /* error threshold level. */
	ret |= is_ois_write_u8(OIS_CMD_OIS_SEL, 0x01); /* OIS SEL (wide : 1 , tele : 2, both : 3 ). */
	ret |= is_ois_write_u8(OIS_CMD_ERR_VAL_CNT, 0x0); /* count value for error judgement level. */
	ret |= is_ois_write_u8(OIS_CMD_FREQ_LEV, 0x05); /* frequency level for measurement. */
	ret |= is_ois_write_u8(OIS_CMD_AMPLI_LEV, 0x34); /* amplitude level for measurement. */
	ret |= is_ois_write_u8(OIS_CMD_DUM_PULSE, 0x03); /* dummy pulse setting. */
	ret |= is_ois_write_u8(OIS_CMD_VYVLE_LEV, 0x02); /* vyvle level for measurement. */
	ret |= is_ois_write_u8(OIS_CMD_START_WAVE_CHECK, 0x01); /* start sine wave check operation */
	if (ret) {
		err("i2c write fail\n");
		goto exit;
	} else
		info("i2c write success\n");

	retries = 30;
	do {
		reg = OIS_CMD_START_WAVE_CHECK;
		ret = is_ois_read_u8(reg, &val);
		if (ret) {
			MCU_GET_ERR_PRINT(reg);
			goto exit;
		}

		msleep(100);

		if (--retries < 0) {
			err("sine wave operation fail.\n");
			break;
		}
	} while (val);

	reg = OIS_CMD_AUTO_TEST_RESULT;
	ret = is_ois_read_u8(reg, &buf);
	if (ret) {
		MCU_GET_ERR_PRINT(reg);
		goto exit;
	}

	*result = (int)buf;

#ifdef CAMERA_2ND_OIS
	ret = is_ois_read_u16(OIS_CMD_REAR2_SINX_COUNT1, u8_sinx_count);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR2_SINY_COUNT1, u8_siny_count);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR2_SINX_DIFF1, u8_sinx);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR2_SINY_DIFF1, u8_siny);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}
#else
	ret = is_ois_read_u16(OIS_CMD_REAR_SINX_COUNT1, u8_sinx_count);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR_SINY_COUNT1, u8_siny_count);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR_SINX_DIFF1, u8_sinx);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR_SINY_DIFF1, u8_siny);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}
#endif
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	cam_ois_set_aois_fac_mode_off();

	dbg_ois("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d\n",
		threshold, *sinx, *siny, sinx_count, siny_count);

	if (buf == 0x0) {
		return true;
	} else {
		return false;
	}

exit:
	*sinx = -1;
	*siny = -1;
	cam_ois_set_aois_fac_mode_off();

	return false;
}

bool is_ois_auto_test_mcu(struct is_core *core,
					int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
					bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd,
					bool *x_result_3rd, bool *y_result_3rd, int *sin_x_3rd, int *sin_y_3rd)
{
	int result = 0;
	bool value = false;
	struct is_mcu *mcu = NULL;

//#ifdef CONFIG_AF_HOST_CONTROL
	is_af_move_lens(core, SENSOR_POSITION_REAR);
	msleep(100);
//#endif

	info("%s autotest started", __func__);

	mcu = is_get_mcu(core);

	value = is_ois_sine_wavecheck_mcu(core, threshold, sin_x, sin_y, &result);
	if (*sin_x == -1 && *sin_y == -1) {
		err("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;

		return false;
	}

	if (value == true) {
		*x_result = true;
		*y_result = true;

		return true;
	} else {
		dbg_ois("OIS autotest is failed value = 0x%x\n", result);
		if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}

		return false;
	}
}

int is_mcu_set_aperture(struct v4l2_subdev *subdev, int onoff)
{
	int ret = 0;
	u8 data = 0;
	int retry = 5;
	int value = 0;
	struct is_mcu *mcu = NULL;

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	info("%s started onoff = %d", __func__, onoff);

	switch (onoff) {
	case F1_5:
		value = 2;
		break;
	case F2_4:
		value = 1;
		break;
	default:
		info("%s: mode is not set.(mode = %d)\n", __func__, onoff);
		mcu->aperture->step = APERTURE_STEP_STATIONARY;
		goto exit;
	}

	/* wait control register to idle */
	do {
		ret = is_ois_read_u8(0x61, &data);
		if (ret) {
			err("i2c read fail\n");
			goto exit;
		}

		if (retry-- < 0) {
			err("wait idle state failed..");
			break;
		}
	} while (data);

	info("mcu status = %d", data);

	ret = is_ois_write_u8(0x63, value);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	/* start aperture control */
	ret = is_ois_write_u8(0x61, 0x01);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	if (value == 2)
		mcu->aperture->cur_value = F1_5;
	else if (value == 1)
		mcu->aperture->cur_value = F2_4;

	mcu->aperture->step = APERTURE_STEP_STATIONARY;

	msleep(mcu_init.aperture_delay_list[0]);

	return true;

exit:
	info("%s Do not set aperture. onoff = %d", __func__, onoff);

	return false;
}

int is_mcu_deinit_aperture(struct v4l2_subdev *subdev, int onoff)
{
	int ret = 0;
	u8 data = 0;
	int retry = 5;
	struct is_mcu *mcu = NULL;

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	info("%s started onoff = %d", __func__, onoff);

	/* wait control register to idle */
	do {
		ret = is_ois_read_u8(0x61, &data);
		if (ret) {
			err("i2c read fail\n");
			goto exit;
		}

		if (retry-- < 0) {
			err("wait idle state failed..");
			break;
		}
	} while (data);

	info("mcu status = %d", data);

	ret = is_ois_write_u8(0x63, 0x2);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	/* start aperture control */
	ret = is_ois_write_u8(0x61, 0x01);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	mcu->aperture->cur_value = F1_5;

	msleep(mcu_init.aperture_delay_list[0]);

	return true;

exit:
	return false;
}

void is_mcu_set_aperture_onboot(struct is_core *core)
{
	int ret = 0;
	u8 data = 0;
	int retry = 5;
	struct is_device_sensor *device = NULL;

	info("%s : E\n", __func__);

	device = &core->sensor[0];

	if (!device->mcu || !device->mcu->aperture) {
		err("%s, ois subdev is NULL", __func__);
		return;
	}

	/* wait control register to idle */
	do {
		ret = is_ois_read_u8(0x61, &data);
		if (ret) {
			err("i2c read fail\n");
		}

		if (retry-- < 0) {
			err("wait idle state failed..");
			break;
		}
	} while (data);

	info("mcu status = %d", data);

	ret = is_ois_write_u8(0x63, 0x2);
	if (ret) {
		err("i2c read fail\n");
	}

	/* start aperture control */
	ret = is_ois_write_u8(0x61, 0x01);
	if (ret) {
		err("i2c read fail\n");
	}

	device->mcu->aperture->cur_value = F1_5;

	msleep(mcu_init.aperture_delay_list[1]);

	info("%s : X\n", __func__);
}

bool is_mcu_halltest_aperture(struct v4l2_subdev *subdev, u16 *hall_value)
{
	int ret = 0;
	u8 data = 0;
	u8 data_array[2] = {0, };
	int retry = 3;
	bool result = true;
	struct is_mcu *mcu = NULL;

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		return false;
	}

	info("%s started hall check", __func__);

	/* wait control register to idle */
	do {
		ret = is_ois_read_u8(0x61, &data);
		if (ret) {
			err("i2c read fail\n");
			result = false;
			goto exit;
		}

		if (retry-- < 0) {
			err("wait idle state failed..");
			result = false;
			goto exit;
		}

		msleep(15);
	} while (data);

	info("mcu status = %d", data);

	ret = is_ois_write_u8(0x61, 0x10);
	if (ret) {
		err("i2c read fail\n");
		result = false;
		goto exit;
	}

	/* wait control register to idle */
	retry = 3;

	do {
		ret = is_ois_read_u8(0x61, &data);
		if (ret) {
			err("i2c read fail\n");
			result = false;
			goto exit;
		}

		if (retry-- < 0) {
			err("wait idle state failed..");
			result = false;
			goto exit;
		}

		msleep(5);
	} while (data);

	ret = is_ois_read_u16(0x002C, data_array);
	if (ret) {
		err("i2c read fail\n");
		result = false;
		goto exit;
	}

	*hall_value = (data_array[1] << 8) | data_array[0];

exit:
	info("%s aperture mode = %d, hall_value = 0x%04x, result = %d",
		__func__, mcu->aperture->cur_value, *hall_value, result);

	return result;
}

signed long long hex2float_kernel(unsigned int hex_data, int endian)
{
	const signed long long scale = SCALE;
	unsigned int s,e,m;
	signed long long res;

	if (endian == eBIG_ENDIAN)
		hex_data = SWAP32(hex_data);

	s = hex_data >> 31, e = (hex_data >> 23) & 0xff, m = hex_data & 0x7fffff;
	res = (e >= 150) ? ((scale * (8388608 + m)) << (e - 150)) : ((scale * (8388608 + m)) >> (150 - e));
	if (s == 1)
		res *= -1;

	return res;
}

void is_status_check_mcu(void)
{
	u8 ois_status_check = 0;
	int retry_count = 0;

	do {
		is_ois_read_u8(0x000E, &ois_status_check);
		if (ois_status_check == 0x14)
			break;
		usleep_range(1000,1000);
		retry_count++;
	} while (retry_count < OIS_MEM_STATUS_RETRY);

	if (retry_count == OIS_MEM_STATUS_RETRY)
		err("%s, ois status check fail, retry_count(%d)\n", __func__, retry_count);

	if (ois_status_check == 0)
		err("%s, ois Memory access fail\n", __func__);
}

int fimc_calculate_shift_value_mcu(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	short ois_shift_x_cal = 0;
	short ois_shift_y_cal = 0;
	u8 cal_data[2];
	u8 shift_available = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *mcu = NULL;
#ifdef OIS_CENTERING_SHIFT_ENABLE
	char *cal_buf;
	u32 Wide_XGG_Hex, Wide_YGG_Hex, Tele_XGG_Hex, Tele_YGG_Hex;
	signed long long Wide_XGG, Wide_YGG, Tele_XGG, Tele_YGG;
	u32 Image_Shift_x_Hex, Image_Shift_y_Hex;
	signed long long Image_Shift_x, Image_Shift_y;
	u8 read_multi[4] = {0, };
	signed long long Coef_angle_X , Coef_angle_Y;
	signed long long Wide_Xshift, Tele_Xshift, Wide_Yshift, Tele_Yshift;
	const signed long long scale = SCALE*SCALE;
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
	struct cam_hw_param *hw_param = NULL;
	struct is_device_sensor *device = NULL;
#endif

	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = mcu->ois;

	IXC_MUTEX_LOCK(ois->ixc_lock);

	/* use user data */
	ret |= is_ois_write_u8(0x000F, 0x40);
	cal_data[0] = 0x00;
	cal_data[1] = 0x02;
	ret |= is_ois_write_u16(0x0010, cal_data);
	ret |= is_ois_write_u8(0x000E, 0x04);
	if (ret < 0) {
#ifdef USE_CAMERA_HW_BIG_DATA
		device = v4l2_get_subdev_hostdata(subdev);
		if (device) {
			is_sec_get_hw_param(&hw_param, device->position);
		}
		if (hw_param)
			hw_param->i2c_ois_err_cnt++;
#endif
		err("ois user data write is fail");
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}

	is_status_check_mcu();
#ifdef OIS_CENTERING_SHIFT_ENABLE
	is_ois_read_multi(OIS_CMD_REAR_XGG1, read_multi, 4);
	Wide_XGG_Hex = (read_multi[3] << 24) | (read_multi[2] << 16) | (read_multi[1] << 8) | (read_multi[0]);

	is_ois_read_multi(OIS_CMD_REAR2_XGG1, read_multi, 4);
	Tele_XGG_Hex = (read_multi[3] << 24) | (read_multi[2] << 16) | (read_multi[1] << 8) | (read_multi[0]);

	is_ois_read_multi(OIS_CMD_REAR_YGG1, read_multi, 4);
	Wide_YGG_Hex = (read_multi[3] << 24) | (read_multi[2] << 16) | (read_multi[1] << 8) | (read_multi[0]);

	is_ois_read_multi(OIS_CMD_REAR2_YGG1, read_multi, 4);
	Tele_YGG_Hex = (read_multi[3] << 24) | (read_multi[2] << 16) | (read_multi[1] << 8) | (read_multi[0]);

	Wide_XGG = hex2float_kernel(Wide_XGG_Hex, eLIT_ENDIAN); // unit : 1/SCALE
	Wide_YGG = hex2float_kernel(Wide_YGG_Hex, eLIT_ENDIAN); // unit : 1/SCALE
	Tele_XGG = hex2float_kernel(Tele_XGG_Hex, eLIT_ENDIAN); // unit : 1/SCALE
	Tele_YGG = hex2float_kernel(Tele_YGG_Hex, eLIT_ENDIAN); // unit : 1/SCALE

	is_sec_get_cal_buf(&cal_buf);

	Image_Shift_x_Hex = (cal_buf[0x6C7C+3] << 24) | (cal_buf[0x6C7C+2] << 16) | (cal_buf[0x6C7C+1] << 8) | (cal_buf[0x6C7C]);
	Image_Shift_y_Hex = (cal_buf[0x6C80+3] << 24) | (cal_buf[0x6C80+2] << 16) | (cal_buf[0x6C80+1] << 8) | (cal_buf[0x6C80]);

	Image_Shift_x = hex2float_kernel(Image_Shift_x_Hex,eLIT_ENDIAN); // unit : 1/SCALE
	Image_Shift_y = hex2float_kernel(Image_Shift_y_Hex,eLIT_ENDIAN); // unit : 1/SCALE

	Image_Shift_y += 90 * SCALE;

	#define ABS(a)				((a) > 0 ? (a) : -(a))

	// Calc w/t x shift
	//=======================================================
	Coef_angle_X = (ABS(Image_Shift_x) > SH_THRES) ? Coef_angle_max : RND_DIV(ABS(Image_Shift_x), 228);

	Wide_Xshift = Gyrocode * Coef_angle_X * Wide_XGG;
	Tele_Xshift = Gyrocode * Coef_angle_X * Tele_XGG;

	Wide_Xshift = (Image_Shift_x > 0) ? Wide_Xshift	: Wide_Xshift * - 1;
	Tele_Xshift = (Image_Shift_x > 0) ? Tele_Xshift * - 1 : Tele_Xshift;

	// Calc w/t y shift
	//=======================================================
	Coef_angle_Y = (ABS(Image_Shift_y) > SH_THRES) ? Coef_angle_max : RND_DIV(ABS(Image_Shift_y), 228);

	Wide_Yshift = Gyrocode * Coef_angle_Y * Wide_YGG;
	Tele_Yshift = Gyrocode * Coef_angle_Y * Tele_YGG;

	Wide_Yshift = (Image_Shift_y > 0) ? Wide_Yshift * - 1 : Wide_Yshift;
	Tele_Yshift = (Image_Shift_y > 0) ? Tele_Yshift * - 1 : Tele_Yshift;

	// Calc output variable
	//=======================================================
	ois_centering_shift_x = (int)RND_DIV(Wide_Xshift, scale);
	ois_centering_shift_y = (int)RND_DIV(Wide_Yshift, scale);
	ois_centering_shift_x_rear2 = (int)RND_DIV(Tele_Xshift, scale);
	ois_centering_shift_y_rear2 = (int)RND_DIV(Tele_Yshift, scale);
#endif

	is_ois_read_u8(OIS_CMD_BYPASS_DEVICE_ID1, &shift_available);
	if (shift_available != 0x11) {
		ois->ois_shift_available = false;
		dbg_ois("%s, OIS AF CAL(0x%x) does not installed.\n", __func__, shift_available);
	} else {
		ois->ois_shift_available = true;

		/* OIS Shift CAL DATA */
		for (i = 0; i < 9; i++) {
			cal_data[0] = 0;
			cal_data[1] = 0;
			is_ois_read_u16(0x0110 + (i * 2), cal_data);
			ois_shift_x_cal = (cal_data[1] << 8) | (cal_data[0]);

			cal_data[0] = 0;
			cal_data[1] = 0;
			is_ois_read_u16(0x0122 + (i * 2), cal_data);
			ois_shift_y_cal = (cal_data[1] << 8) | (cal_data[0]);

			if (ois_shift_x_cal > (short)32767)
				ois_shift_x[i] = ois_shift_x_cal - 65536;
			else
				ois_shift_x[i] = ois_shift_x_cal;

			if (ois_shift_y_cal > (short)32767)
				ois_shift_y[i] = ois_shift_y_cal - 65536;
			else
				ois_shift_y[i] = ois_shift_y_cal;
		}
	}

	shift_available = 0;
	is_ois_read_u8(OIS_CMD_BYPASS_DEVICE_ID2, &shift_available);
	if (shift_available != 0x11) {
		dbg_ois("%s, REAR2 OIS AF CAL(0x%x) does not installed.\n", __func__, shift_available);
	} else {
		/* OIS Shift CAL DATA REAR2 */
		for (i = 0; i < 9; i++) {
			cal_data[0] = 0;
			cal_data[1] = 0;
			is_ois_read_u16(0x0140 + (i * 2), cal_data);
			ois_shift_x_cal = (cal_data[1] << 8) | (cal_data[0]);

			cal_data[0] = 0;
			cal_data[1] = 0;
			is_ois_read_u16(0x0152 + (i * 2), cal_data);
			ois_shift_y_cal = (cal_data[1] << 8) | (cal_data[0]);

			if (ois_shift_x_cal > (short)32767)
				ois_shift_x_rear2[i] = ois_shift_x_cal - 65536;
			else
				ois_shift_x_rear2[i] = ois_shift_x_cal;

			if (ois_shift_y_cal > (short)32767)
				ois_shift_y_rear2[i] = ois_shift_y_cal - 65536;
			else
				ois_shift_y_rear2[i] = ois_shift_y_cal;

		}
	}

	IXC_MUTEX_UNLOCK(ois->ixc_lock);

	return ret;
}

#if 0
static u32 sec_hw_rev;
static int __init is_mcu_get_hw_rev(char *arg)
{
	get_option(&arg, &sec_hw_rev);
	return 0;
}
early_param("androidboot.revision", is_mcu_get_hw_rev);
#endif

void is_ois_set_gyro_raw(long raw_data_x, long raw_data_y, long raw_data_z) {
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	u8 val[6];

	raw_data_x = raw_data_x * scale_factor;
	raw_data_y = raw_data_y * scale_factor;
	raw_data_z = raw_data_z * scale_factor;

	raw_data_x = raw_data_x / 1000;
	raw_data_y = raw_data_y / 1000;
	raw_data_z = raw_data_z / 1000;

	val[0] = raw_data_x & 0x00FF;
	val[1] = (raw_data_x & 0xFF00) >> 8;
	val[2] = raw_data_y & 0x00FF;
	val[3] = (raw_data_y & 0xFF00) >> 8;
	val[4] = raw_data_z & 0x00FF;
	val[5] = (raw_data_z & 0xFF00) >> 8;

	is_ois_write_multi(OIS_CMD_RAW_DEBUG_X1, val, 6);
}

long ois_mcu_get_efs_data(void)
{
	long efs_size = 0;
	struct is_core *core = NULL;
	struct is_vendor_private *vendor_priv;
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;

	efs_size = vendor_priv->gyro_efs_size;
	if (efs_size == 0) {
		err("efs read failed.");
		goto p_err;
	}

	info("%s : E\n", __func__);

	ois_mcu_parsing_raw_data_mcu(vendor_priv->gyro_efs_data, efs_size, &raw_data_x, &raw_data_y, &raw_data_z);
	if (efs_size > 0)
		is_ois_set_gyro_raw(raw_data_x, raw_data_y, raw_data_z);

p_err:
	return efs_size;
}

int is_ois_init_mcu(struct v4l2_subdev *subdev)
{
	int ret = 0;
#ifdef USE_OIS_SLEEP_MODE
	u8 read_gyrocalcen = 0;
#endif
	u8 val = 0;
	int retries = 10;
	struct is_mcu *mcu = NULL;
	struct is_ois *ois = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_ois_info *ois_pinfo = NULL;
	u8 buf[5];
	u16 reg;

	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = mcu->sensor_peri;
	if (!sensor_peri) {
		err("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_ois_get_phone_version(&ois_pinfo);

	ois = mcu->ois;
	ois->ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->coef = 0;
	ois->pre_coef = 255;
	ois->fadeupdown = false;
	ois->initial_centering_mode = false;
	ois->af_pos_wide = 0;
#ifdef CAMERA_2ND_OIS
	ois->af_pos_tele = 0;
	ois->ois_power_mode = -1;
#endif
	ois_pinfo->reset_check = false;

	if (mcu->aperture) {
		mcu->aperture->cur_value = F2_4;
		mcu->aperture->new_value = F2_4;
		mcu->aperture->start_value = F2_4;
		mcu->aperture->step = APERTURE_STEP_STATIONARY;
	}

#ifdef USE_OIS_SLEEP_MODE
	IXC_MUTEX_LOCK(ois->ixc_lock);
	is_ois_read_u8(0x00BF, &read_gyrocalcen);
	if ((read_gyrocalcen == 0x01 && module->position == SENSOR_POSITION_REAR2) || //tele already enabled
		(read_gyrocalcen == 0x02 && module->position == SENSOR_POSITION_REAR)) { //wide already enabled
		ois->ois_shift_available = true;
		info("%s %d sensor(%d) is already initialized.\n", __func__, __LINE__, ois->device);
		ret = is_ois_write_u8(0x00BF, 0x03);
		if (ret < 0)
			err("ois 0x00BF write is fail");
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}
	IXC_MUTEX_UNLOCK(ois->ixc_lock);
#else
	if ((ois_wide_init == true && module->position == SENSOR_POSITION_REAR) ||
		(ois_tele_init == true && module->position == SENSOR_POSITION_REAR2)) {
		info("%s %d sensor(%d) is already initialized.\n", __func__, __LINE__, ois->device);
		ois_wide_init = ois_tele_init = true;
		ois->ois_shift_available = true;
	}
#endif

	 if (!ois_hw_check) {
		IXC_MUTEX_LOCK(ois->ixc_lock);

		do {
			reg = OIS_CMD_STATUS;
			ret = is_ois_read_u8(reg, &val);
			if (ret != 0) {
				MCU_GET_ERR_PRINT(reg);
				val = -EIO;
				break;
			}
			msleep(3);
			if (--retries < 0) {
				err("Read status failed!!!!, data = 0x%04x\n", val);
				break;
			}
		} while (val != 0x01);

		if (val == 0x01) {
			ret = is_ois_write_multi(OIS_CMD_REAR_XGG1,  ois_pinfo->wide_romdata.xgg, 4);
			ret |= is_ois_write_multi(OIS_CMD_REAR_YGG1, ois_pinfo->wide_romdata.ygg, 4);
#ifdef CAMERA_2ND_OIS
			ret |= is_ois_write_multi(OIS_CMD_REAR2_XGG1, ois_pinfo->tele_romdata.xgg, 4);
			ret |= is_ois_write_multi(OIS_CMD_REAR2_YGG1, ois_pinfo->tele_romdata.ygg, 4);
#endif
			if (ret < 0)
				err("ois gyro data write is fail");

#if !IS_ENABLED(SIMPLIFY_OIS_INIT)
			ret = is_ois_write_u16(OIS_CMD_XCOEF_M1_1,  ois_pinfo->wide_romdata.xcoef);
			ret |= is_ois_write_u16(OIS_CMD_YCOEF_M1_1, ois_pinfo->wide_romdata.ycoef);
#ifdef CAMERA_2ND_OIS
			ret |= is_ois_write_u16(OIS_CMD_XCOEF_M2_1, ois_pinfo->tele_romdata.xcoef);
			ret |= is_ois_write_u16(OIS_CMD_YCOEF_M2_1, ois_pinfo->tele_romdata.ycoef);
#endif
#endif

			if (ret < 0)
				err("ois coef data write is fail");
#ifdef CAMERA_2ND_OIS
			/* ENABLE DUAL SHIFT */
			ret = is_ois_write_u8(OIS_CMD_ENABLE_DUALCAL, 0x01);
			if (ret < 0)
				err("ois dual shift is fail");
#endif
			buf[0] = mcu_init.ois_gyro_direction[0]; /* wx_pole */
			buf[1] = mcu_init.ois_gyro_direction[1]; /* wy_pole */
			buf[2] = mcu_init.ois_gyro_direction[2]; /* gyro_orientation*/
#ifdef CAMERA_2ND_OIS
			buf[3] = mcu_init.ois_gyro_direction[3]; /* tx_pole */
			buf[4] = mcu_init.ois_gyro_direction[4]; /* ty_pole */
#endif

			ret = is_ois_write_multi(OIS_CMD_GYRO_POLA_X, buf, 3);
#ifdef CAMERA_2ND_OIS
			ret = is_ois_write_u16(OIS_CMD_GYRO_POLA_X_M2, buf + 3);
#endif
			info("%s gyro init data applied\n", __func__);
		}

		IXC_MUTEX_UNLOCK(ois->ixc_lock);

		ois_hw_check = true;
	}

	if (module->position == SENSOR_POSITION_REAR2) {
		ois_tele_init = true;
	} else if (module->position == SENSOR_POSITION_REAR) {
		ois_wide_init = true;
	}

	ois_mcu_get_efs_data();

	info("%s\n", __func__);
	return ret;
}

int is_ois_deinit_mcu(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_mcu *mcu = NULL;
	u16 reg;

	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	if (ois_hw_check) {
		reg = OIS_CMD_START;
		ret = is_ois_write_u8(reg, 0x00);	/* 0 : ois servo off, 1 : ois servo on */
		if (ret)
			MCU_SET_ERR_PRINT(reg);

		usleep_range(2000, 2100);
	}

	ois_fadeupdown = false;
	ois_hw_check = false;

	dbg_ois("%s\n", __func__);

	return ret;
}

int is_ois_set_ggfadeupdown_mcu(struct v4l2_subdev *subdev, int up, int down)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *mcu = NULL;
	u8 status = 0;
	int retries = 100;
	u8 data[2];
#if !IS_ENABLED(SIMPLIFY_OIS_INIT)
	u8 write_data[4] = {0, };
#endif
#ifdef USE_OIS_SLEEP_MODE
	u8 read_sensorStart = 0;
#endif
	u16 reg;

	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = mcu->ois;

	dbg_ois("%s up:%d down:%d\n", __func__, up, down);

	IXC_MUTEX_LOCK(ois->ixc_lock);

#ifdef CAMERA_2ND_OIS
	if (ois->ois_power_mode < OIS_POWER_MODE_SINGLE) {
		reg = OIS_CMD_OIS_SEL;
		ret = is_ois_write_u8(reg, 0x03);
		if (ret < 0) {
			MCU_SET_ERR_PRINT(reg);
			IXC_MUTEX_UNLOCK(ois->ixc_lock);
			return ret;
		}
	}
#else
	reg = OIS_CMD_OIS_SEL;
	ret = is_ois_write_u8(reg, 0x01);
	if (ret < 0) {
		MCU_SET_ERR_PRINT(reg);
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}
#endif

	/* Wide af position value */
	reg = OIS_CMD_REAR_AF;
	ret = is_ois_write_u8(reg, 0x00);
	if (ret < 0) {
		MCU_SET_ERR_PRINT(reg);
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}

#ifdef CAMERA_2ND_OIS
	/* Tele af position value */
	reg = OIS_CMD_REAR2_AF;
	ret = is_ois_write_u8(reg, 0x00);
	if (ret < 0) {
		MCU_SET_ERR_PRINT(reg);
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}
#endif	
	reg = OIS_CMD_CACTRL_WRITE;
	ret = is_ois_write_u8(reg, 0x01);
	if (ret < 0) {
		MCU_SET_ERR_PRINT(reg);
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}

	/* angle compensation 1.5->1.25
	 * before addr:0x0000, data:0x01
	 * write 0x3F558106
	 * write 0x3F558106
	 */
#if !IS_ENABLED(SIMPLIFY_OIS_INIT)
	write_data[0] = 0x06;
	write_data[1] = 0x81;
	write_data[2] = 0x55;
	write_data[3] = 0x3F;
	is_ois_write_multi(OIS_CMD_ANGLE_COMP1, write_data, 4);

	write_data[0] = 0x06;
	write_data[1] = 0x81;
	write_data[2] = 0x55;
	write_data[3] = 0x3F;
	is_ois_write_multi(OIS_CMD_ANGLE_COMP5, write_data, 4);
#endif

#ifdef USE_OIS_SLEEP_MODE
	/* if camera is already started, skip VDIS setting */
	is_ois_read_u8(0x00BF, &read_sensorStart);
	if (read_sensorStart == 0x03) {
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}
#endif
	/* set fadeup */
	data[0] = up & 0xFF;
	data[1] = (up >> 8) & 0xFF;
	reg = OIS_CMD_FADE_UP1;
	ret = is_ois_write_u16(reg, data);
	if (ret < 0)
		MCU_SET_ERR_PRINT(reg);

	/* set fadedown */
	data[0] = down & 0xFF;
	data[1] = (down >> 8) & 0xFF;
	reg = OIS_CMD_FADE_DOWN1;
	ret = is_ois_write_u16(reg, data);
	if (ret < 0)
		MCU_SET_ERR_PRINT(reg);

	/* wait idle status
	 * 100msec delay is needed between "ois_power_on" and "ois_mode_s6".
	 */
	do {
		is_ois_read_u8(OIS_CMD_STATUS, &status);
		if (status == 0x01 || status == 0x13)
			break;
		if (--retries < 0) {
			err("%s : read register fail!. status: 0x%x\n", __func__, status);
			ret = -1;
			break;
		}
		usleep_range(1000, 1100);
	} while (status != 0x01);

	IXC_MUTEX_UNLOCK(ois->ixc_lock);

	dbg_ois("%s retryCount = %d , status = 0x%x\n", __func__, 100 - retries, status);

	return ret;
}

int is_set_ois_mode_mcu(struct v4l2_subdev *subdev, int mode)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *mcu = NULL;

	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = mcu->ois;

	if (ois->fadeupdown == false) {
		if (ois_fadeupdown == false) {
			ois_fadeupdown = true;
			is_ois_set_ggfadeupdown_mcu(subdev, 1000, 1000);
		}
		ois->fadeupdown = true;
	}

	if (mode == ois->pre_ois_mode) {
		return ret;
	}

	ois->pre_ois_mode = mode;
	info("%s: ois_mode value(%d)\n", __func__, mode);

	IXC_MUTEX_LOCK(ois->ixc_lock);
	switch(mode) {
		case OPTICAL_STABILIZATION_MODE_STILL:
			is_ois_write_u8(OIS_CMD_MODE, 0x00);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VIDEO:
			is_ois_write_u8(OIS_CMD_MODE, 0x01);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_CENTERING:
			is_ois_write_u8(OIS_CMD_MODE, 0x05);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_STILL_ZOOM:
			is_ois_write_u8(OIS_CMD_MODE, 0x13);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VDIS:
			is_ois_write_u8(OIS_CMD_MODE, 0x14);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_SINE_X:
			is_ois_write_u8(OIS_CMD_SINE_1, 0x01);
			is_ois_write_u8(OIS_CMD_SINE_2, 0x01);
			is_ois_write_u8(OIS_CMD_SINE_3, 0x2D);
			is_ois_write_u8(OIS_CMD_MODE, 0x03);
			msleep(20);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_SINE_Y:
			is_ois_write_u8(OIS_CMD_SINE_1, 0x02);
			is_ois_write_u8(OIS_CMD_SINE_2, 0x01);
			is_ois_write_u8(OIS_CMD_SINE_3, 0x2D);
			is_ois_write_u8(OIS_CMD_MODE, 0x03);
			msleep(20);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		default:
			dbg_ois("%s: ois_mode value(%d)\n", __func__, mode);
			break;
	}
	IXC_MUTEX_UNLOCK(ois->ixc_lock);

	return ret;
}

int is_ois_shift_compensation_mcu(struct v4l2_subdev *subdev, int position, int resolution)
{
	int ret = 0;
	struct is_ois *ois;
	struct is_mcu *mcu = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	int position_changed;
	u16 reg;

	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_peri = mcu->sensor_peri;
	if (!sensor_peri) {
		err("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = mcu->ois;

	position_changed = position >> 4;

	IXC_MUTEX_LOCK(ois->ixc_lock);

	if (module->position == SENSOR_POSITION_REAR && ois->af_pos_wide != position_changed) {
		/* Wide af position value */
		reg = OIS_CMD_REAR_AF;
		ret = is_ois_write_u8(reg, (u8)position_changed);
		if (ret < 0) {
			MCU_SET_ERR_PRINT(reg);
			IXC_MUTEX_UNLOCK(ois->ixc_lock);
			return ret;
		}
		ois->af_pos_wide = position_changed;
	} else if (module->position == SENSOR_POSITION_REAR2 && ois->af_pos_tele != position_changed) {
		/* Tele af position value */
		reg = OIS_CMD_REAR2_AF;
		ret = is_ois_write_u8(reg, (u8)position_changed);
		if (ret < 0) {
			MCU_SET_ERR_PRINT(reg);
			IXC_MUTEX_UNLOCK(ois->ixc_lock);
			return ret;
		}
		ois->af_pos_tele = position_changed;
	}

	IXC_MUTEX_UNLOCK(ois->ixc_lock);

p_err:
	return ret;
}

int is_ois_self_test_mcu(struct is_core *core)
{
	int ret = 0;
	u8 val = 0;
	u8 reg_val = 0, x = 0, y = 0;
	u16 x_gyro_log = 0, y_gyro_log = 0;
	int retries = 30;
	u16 reg;

	info("%s : E\n", __func__);
	cam_ois_set_aois_fac_mode_on();

	reg = OIS_CMD_GYRO_CAL;
	ret = is_ois_write_u8(reg, 0x08);
	if (ret)
		MCU_SET_ERR_PRINT(reg);

	do {
		reg = OIS_CMD_GYRO_CAL;
		ret = is_ois_read_u8(reg, &val);
		if (ret != 0) {
			MCU_GET_ERR_PRINT(reg);
			val = -EIO;
			break;
		}
		msleep(20);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	reg = OIS_CMD_ERROR_STATUS;
	ret = is_ois_read_u8(reg, &val);
	if (ret != 0) {
		MCU_GET_ERR_PRINT(reg);
		val = -EIO;
	}

	/* Gyro selfTest result */
	is_ois_read_u8(OIS_CMD_GYRO_VAL_X, &reg_val);
	x = reg_val;
	is_ois_read_u8(OIS_CMD_GYRO_LOG_X, &reg_val);
	x_gyro_log = (reg_val << 8) | x;

	is_ois_read_u8(OIS_CMD_GYRO_VAL_Y, &reg_val);
	y = reg_val;
	is_ois_read_u8(OIS_CMD_GYRO_LOG_Y, &reg_val);
	y_gyro_log = (reg_val << 8) | y;

	info("%s(GSTLOG0=%d, GSTLOG1=%d)\n", __func__, x_gyro_log, y_gyro_log);

	info("%s(%d) : X\n", __func__, val);
	cam_ois_set_aois_fac_mode_off();

	return (int)val;
}

#ifdef CAMERA_2ND_OIS
bool is_ois_sine_wavecheck_rear2_mcu(struct is_core *core,
					int threshold, int *sinx, int *siny, int *result,
					int *sinx_2nd, int *siny_2nd)
{
	u8 buf = 0, val = 0;
	int ret = 0, retries = 10;
	int sinx_count = 0, siny_count = 0;
	int sinx_count_2nd = 0, siny_count_2nd = 0;
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	u16 reg;

	ret = is_ois_write_u8(OIS_CMD_OIS_SEL, 0x03); /* OIS SEL (wide : 1 , tele : 2, both : 3 ). */
	ret |= is_ois_write_u8(OIS_CMD_THRESH_ERR_LEV, (u8)threshold); /* error threshold level. */
	ret |= is_ois_write_u8(OIS_CMD_THRESH_ERR_LEV_M2, (u8)threshold); /* error threshold level. */
	ret |= is_ois_write_u8(OIS_CMD_ERR_VAL_CNT, 0x0); /* count value for error judgement level. */
	ret |= is_ois_write_u8(OIS_CMD_FREQ_LEV, 0x05); /* frequency level for measurement. */
	ret |= is_ois_write_u8(OIS_CMD_AMPLI_LEV, 0x2A); /* amplitude level for measurement. */
	ret |= is_ois_write_u8(OIS_CMD_DUM_PULSE, 0x03); /* dummy pulse setting. */
	ret |= is_ois_write_u8(OIS_CMD_VYVLE_LEV, 0x02); /* vyvle level for measurement. */
	ret |= is_ois_write_u8(OIS_CMD_START_WAVE_CHECK, 0x01); /* start sine wave check operation */
	if (ret) {
		err("i2c write fail\n");
		goto exit;
	}

	retries = 10;
	do {
		reg = OIS_CMD_START_WAVE_CHECK;
		ret = is_ois_read_u8(reg, &val);
		if (ret) {
			MCU_GET_ERR_PRINT(reg);
			goto exit;
		}

		msleep(100);

		if (--retries < 0) {
			err("sine wave operation fail.\n");
			break;
		}
	} while (val);

	reg = OIS_CMD_AUTO_TEST_RESULT;
	ret = is_ois_read_u8(reg, &buf);
	if (ret) {
		MCU_GET_ERR_PRINT(reg);
		goto exit;
	}

	*result = (int)buf;

	ret = is_ois_read_u16(OIS_CMD_REAR_SINX_COUNT1, u8_sinx_count);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR_SINY_COUNT1, u8_siny_count);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR_SINX_DIFF1, u8_sinx);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR_SINY_DIFF1, u8_siny);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}

	ret |= is_ois_read_u16(OIS_CMD_REAR2_SINX_COUNT1, u8_sinx_count);
	sinx_count_2nd = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count_2nd > 0x7FFF) {
		sinx_count_2nd = -((sinx_count_2nd ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR2_SINY_COUNT1, u8_siny_count);
	siny_count_2nd = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count_2nd > 0x7FFF) {
		siny_count_2nd = -((siny_count_2nd ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR2_SINX_DIFF1, u8_sinx);
	*sinx_2nd = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx_2nd > 0x7FFF) {
		*sinx_2nd = -((*sinx_2nd ^ 0xFFFF) + 1);
	}
	ret |= is_ois_read_u16(OIS_CMD_REAR2_SINY_DIFF1, u8_siny);
	*siny_2nd = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny_2nd > 0x7FFF) {
		*siny_2nd = -((*siny_2nd ^ 0xFFFF) + 1);
	}

	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	info("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d\n",
		threshold, *sinx, *siny, sinx_count, siny_count);

	info("threshold = %d, sinx_2nd = %d, siny_2nd = %d, sinx_count_2nd = %d, syny_count_2nd = %d\n",
		threshold, *sinx_2nd, *siny_2nd, sinx_count_2nd, siny_count_2nd);

	if (buf == 0x0) {
		return true;
	} else {
		return false;
	}

exit:
	*sinx = -1;
	*siny = -1;
	*sinx_2nd = -1;
	*siny_2nd = -1;
	return false;
}

bool is_ois_auto_test_rear2_mcu(struct is_core *core,
					int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
					bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd)
{
	int result = 0;
	bool value = false;

#ifdef CONFIG_AF_HOST_CONTROL
#if defined(CAMERA_2ND_OIS)
	is_af_move_lens_rear2(core, SENSOR_POSITION_REAR2);
	msleep(100);
#endif
	is_af_move_lens(core, SENSOR_POSITION_REAR);
	msleep(100);
#endif

	value = is_ois_sine_wavecheck_rear2_mcu(core, threshold, sin_x, sin_y, &result,
				sin_x_2nd, sin_y_2nd);

	if (*sin_x == -1 && *sin_y == -1) {
		err("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;

		return false;
	}

	if (*sin_x_2nd == -1 && *sin_y_2nd == -1) {
		err("OIS 2 device is not prepared.");
		*x_result_2nd = false;
		*y_result_2nd = false;

		return false;
	}

	if (value == true) {
		*x_result = true;
		*y_result = true;
		*x_result_2nd = true;
		*y_result_2nd = true;

		return true;
	} else {
		err("OIS autotest_2nd is failed result (0x0051) = 0x%x\n", result);
		if ((result & 0x03) == 0x00) {
			*x_result = true;
			*y_result = true;
		} else if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}

		if ((result & 0x30) == 0x00) {
			*x_result_2nd = true;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x10) {
			*x_result_2nd = false;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x20) {
			*x_result_2nd = true;
			*y_result_2nd = false;
		} else {
			*x_result_2nd = false;
			*y_result_2nd = false;
		}

		return false;
	}
}

int is_ois_set_power_mode_mcu(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *mcu = NULL;
	struct is_core *core;
	struct is_dual_info *dual_info = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	mcu = (struct is_mcu*)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = mcu->sensor_peri;
	if (!sensor_peri) {
		err("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	core = is_get_is_core();
	if (!core) {
		err("core is null");
		return -EINVAL;
	}

	ois = mcu->ois;
	if (!ois) {
		err("%s, ois subdev is NULL", __func__);
		return -EINVAL;
	}

	dual_info = &core->dual_info;

	IXC_MUTEX_LOCK(ois->ixc_lock);
	if ((dual_info->mode != IS_DUAL_MODE_NOTHING)
		|| (dual_info->mode == IS_DUAL_MODE_NOTHING &&
			module->position == SENSOR_POSITION_REAR2)) {
		ret = is_ois_write_u8(OIS_CMD_OIS_SEL, 0x03);
		ois->ois_power_mode = OIS_POWER_MODE_DUAL;
	} else {
		ret = is_ois_write_u8(OIS_CMD_OIS_SEL, 0x01);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE;
	}

	if (ret < 0)
		err("ois dual setting is fail");
	else
		info("%s ois power setting is %d\n", __func__, ois->ois_power_mode);

	IXC_MUTEX_UNLOCK(ois->ixc_lock);

	return ret;
}
#endif

bool is_ois_check_fw_mcu(struct is_core *core)
{
	int ret = 0;
	struct is_mcu *mcu = NULL;
	struct v4l2_subdev *subdev = NULL;

	mcu = is_get_mcu(core);
	subdev = mcu->subdev;

	is_mcu_fw_update(core);

	msleep(20);

	ret = is_mcu_fw_version(subdev);
	if (!ret) {
		err("Failed to read ois fw version.");
		return false;
	}

	return true;
}

void is_ois_enable_mcu(struct is_core *core)
{
	int ret = 0;
	u16 reg;

	dbg_ois("%s : E\n", __func__);

	reg = OIS_CMD_MODE;
	ret = is_ois_write_u8(reg, 0x00);
	if (ret)
		MCU_SET_ERR_PRINT(reg);

	reg = OIS_CMD_START;
	ret = is_ois_write_u8(reg, 0x01);
	if (ret)
		MCU_SET_ERR_PRINT(reg);

	dbg_ois("%s : X\n", __func__);
}

#ifdef CONFIG_SENSORCORE_MCU_CONTROL
void is_ois_reset_mcu(void *ois_core)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)ois_core;
	struct is_device_sensor *device = NULL;
	struct is_ois_info *ois_pinfo = NULL;
	bool camera_running = false;
	u16 reg;

	info("%s : E\n", __func__);

	device = &core->sensor[0];

	if (!device->mcu || !device->mcu->ois) {
		err("%s, ois subdev is NULL", __func__);
		return;
	}

	is_ois_get_phone_version(&ois_pinfo);

	camera_running = is_vendor_check_camera_running(SENSOR_POSITION_REAR);

	if (camera_running) {
		info("%s : camera is running. reset ois gyro.\n", __func__);

		reg = OIS_CMD_MODE;
		ret = is_ois_write_u8(reg, 0x16);
		if (ret)
			MCU_SET_ERR_PRINT(reg);

		ois_pinfo->reset_check= true;
	} else {
		ois_pinfo->reset_check= false;
		info("%s : camera is not running.\n", __func__);
	}

	info("%s : X\n", __func__);
}
#endif

bool is_ois_gyro_cal_mcu(struct is_core *core, long *x_value, long *y_value, long *z_value)
{
	int ret = 0;
	u8 val = 0, x = 0, y = 0, z = 0;
	int retries = 30;
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	int x_sum = 0, y_sum = 0, z_sum = 0;
	bool result = false;
	u16 reg;

	info("%s : E\n", __func__);
	cam_ois_set_aois_fac_mode_on();

	/* check ois status */
	do {
		reg = OIS_CMD_STATUS;
		ret = is_ois_read_u8(reg, &val);
		if (ret != 0) {
			MCU_GET_ERR_PRINT(reg);
			val = -EIO;
			break;
		}
		msleep(20);
		if (--retries < 0) {
			err("Read status failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val != 0x01);

	retries = 30;

	reg = OIS_CMD_GYRO_CAL;
	ret = is_ois_write_u8(reg, 0x01);
	if (ret)
		MCU_SET_ERR_PRINT(reg);

	do {
		reg = OIS_CMD_GYRO_CAL;
		ret = is_ois_read_u8(reg, &val);
		if (ret != 0) {
			MCU_GET_ERR_PRINT(reg);
			val = -EIO;
			break;
		}
		msleep(15);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	/* Gyro result check */
	reg = OIS_CMD_ERROR_STATUS;
	ret = is_ois_read_u8(reg, &val);
	if (ret != 0) {
		MCU_GET_ERR_PRINT(reg);
		val = -EIO;
		goto exit;
	}

	if ((val & 0x23) == 0x0) {
		result = true;
	}

	is_ois_read_u8(OIS_CMD_RAW_DEBUG_X1, &val);
	x = val;
	is_ois_read_u8(OIS_CMD_RAW_DEBUG_X2, &val);
	x_sum = (val << 8) | x;
	if (x_sum > 0x7FFF) {
		x_sum = -((x_sum ^ 0xFFFF) + 1);
	}

	is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y1, &val);
	y = val;
	is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y2, &val);
	y_sum = (val << 8) | y;
	if (y_sum > 0x7FFF) {
		y_sum = -((y_sum ^ 0xFFFF) + 1);
	}

	is_ois_read_u8(OIS_CMD_RAW_DEBUG_Z1, &val);
	z = val;
	is_ois_read_u8(OIS_CMD_RAW_DEBUG_Z2, &val);
	z_sum = (val << 8) | z;
	if (z_sum > 0x7FFF) {
		z_sum = -((z_sum ^ 0xFFFF) + 1);
	}

	*x_value = x_sum * 1000 / scale_factor;
	*y_value = y_sum * 1000 / scale_factor;
	*z_value = z_sum * 1000 / scale_factor;

exit:
	cam_ois_set_aois_fac_mode_off();
	info("%s X (x = %ld, y = %ld, z = %ld) : result = %d\n", __func__, *x_value, *y_value, *z_value, result);

	return result;
}

bool is_ois_offset_test_mcu(struct is_core *core, long *raw_data_x, long *raw_data_y, long *raw_data_z)
{
	int ret = 0, i = 0;
	u8 val = 0, x = 0, y = 0, z = 0;
	int x_sum = 0, y_sum = 0, z_sum = 0, sum = 0;
	int retries = 0, avg_count = 30;
	bool result = false;
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	u16 reg;

	info("%s : E\n", __func__);
	cam_ois_set_aois_fac_mode_on();

	reg = OIS_CMD_GYRO_CAL;
	ret = is_ois_write_u8(reg, 0x01);
	if (ret)
		MCU_SET_ERR_PRINT(reg);

	retries = avg_count;
	do {
		reg = OIS_CMD_GYRO_CAL;
		ret = is_ois_read_u8(reg, &val);
		if (ret != 0) {
			MCU_GET_ERR_PRINT(reg);
			break;
		}
		msleep(20);
		if (--retries < 0) {
			err("Read register failed!!!! (0x0014), data = 0x%04x\n", val);
			break;
		}
	} while (val);

	/* Gyro result check */
	reg = OIS_CMD_ERROR_STATUS;
	ret = is_ois_read_u8(reg, &val);
	if (ret != 0) {
		MCU_GET_ERR_PRINT(reg);
		val = -EIO;
		goto exit;
	}

	if ((val & 0x23) == 0x0) {
		info("[%s] Gyro result check success. Result is OK.", __func__);
		result = true;
	} else {
		info("[%s] Gyro result check fail. Result is NG. (0x0004 value is %02X)", __func__, val);
		result = false;
	}

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_X1, &val);
		x = val;
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_X2, &val);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y1, &val);
		y = val;
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y2, &val);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Z1, &val);
		z = val;
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Z2, &val);
		z_sum = (val << 8) | z;
		if (z_sum > 0x7FFF) {
			z_sum = -((z_sum ^ 0xFFFF) + 1);
		}
		sum += z_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_z = sum * 1000 / scale_factor / 10;

exit:
	//is_mcu_fw_version(core);
	cam_ois_set_aois_fac_mode_off();
	info("%s : X raw_x = %ld, raw_y = %ld, raw_z = %ld\n", __func__, *raw_data_x, *raw_data_y, *raw_data_z);

	return result;
}

void is_ois_get_offset_data_mcu(struct is_core *core, long *raw_data_x, long *raw_data_y, long *raw_data_z)
{
	int i = 0;
	int ret = 0;
	u8 val = 0, x = 0, y = 0;
	int x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 30;
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	u16 reg;

	info("%s : E\n", __func__);

	/* check ois status */
	retries = avg_count;
	do {
		reg = OIS_CMD_STATUS;
		ret = is_ois_read_u8(reg, &val);
		if (ret != 0) {
			MCU_GET_ERR_PRINT(reg);
			val = -EIO;
			break;
		}
		msleep(20);
		if (--retries < 0) {
			err("Read status failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val != 0x01);

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_X1, &val);
		x = val;
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_X2, &val);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y1, &val);
		y = val;
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y2, &val);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / scale_factor / 10;

	//is_mcu_fw_version(core);
	info("%s : X raw_x = %ld, raw_y = %ld\n", __func__, *raw_data_x, *raw_data_y);
	return;
}

void is_ois_gyro_sleep_mcu(struct is_core *core)
{
	int ret = 0;
	u8 val = 0;
	int retries = 20;
	u16 reg;

	reg = OIS_CMD_START;
	ret = is_ois_write_u8(reg, 0x00);
	if (ret)
		MCU_SET_ERR_PRINT(reg);

	do {
		reg = OIS_CMD_STATUS;
		ret = is_ois_read_u8(reg, &val);
		if (ret != 0) {
			MCU_GET_ERR_PRINT(reg);
			break;
		}

		if (val == 0x01 || val == 0x13)
			break;

		msleep(1);
	} while (--retries > 0);

	if (retries <= 0) {
		err("Read register failed!!!!, data = 0x%04x\n", val);
	}

	reg = OIS_CMD_GYRO_SLEEP;
	ret = is_ois_write_u8(reg, 0x03);
	if (ret)
		MCU_SET_ERR_PRINT(reg);

	msleep(1);

	return;
}

void is_ois_exif_data_mcu(struct is_core *core)
{
	int ret = 0;
	u8 error_reg[2], status_reg;
	u16 error_sum;
	struct is_ois_exif *ois_exif = NULL;
	u16 reg;

	reg = OIS_CMD_ERROR_STATUS;
	ret = is_ois_read_u8(reg, &error_reg[0]);
	if (ret)
		MCU_GET_ERR_PRINT(reg);

	reg = OIS_CMD_CHECKSUM;
	ret = is_ois_read_u8(reg, &error_reg[1]);
	if (ret)
		MCU_GET_ERR_PRINT(reg);

	error_sum = (error_reg[1] << 8) | error_reg[0];

	reg = OIS_CMD_STATUS;
	ret = is_ois_read_u8(reg, &status_reg);
	if (ret)
		MCU_GET_ERR_PRINT(reg);

	is_ois_get_exif_data(&ois_exif);
	ois_exif->error_data = error_sum;
	ois_exif->status_data = status_reg;

	return;
}

u8 is_ois_read_status_mcu(struct is_core *core)
{
	int ret = 0;
	u8 status = 0;
	u16 reg;

	reg = OIS_CMD_READ_STATUS;
	ret = is_ois_read_u8(reg, &status);
	if (ret)
		MCU_GET_ERR_PRINT(reg);

	return status;
}

u8 is_ois_read_cal_checksum_mcu(struct is_core *core)
{
	int ret = 0;
	u8 status = 0;
	u16 reg;

	reg = OIS_CMD_CHECKSUM;
	ret = is_ois_read_u8(reg, &status);
	if (ret)
		MCU_GET_ERR_PRINT(reg);

	return status;
}

int is_ois_set_coef_mcu(struct v4l2_subdev *subdev, u8 coef)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *mcu = NULL;
	u16 reg;

	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = mcu->ois;

	if (ois->pre_coef == coef)
		return ret;

	dbg_ois("%s %d\n", __func__, coef);

	IXC_MUTEX_LOCK(ois->ixc_lock);
	reg = OIS_CMD_SET_COEF;
	ret = is_ois_write_u8(reg, coef);
	if (ret) {
		MCU_SET_ERR_PRINT(reg);
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}
	IXC_MUTEX_UNLOCK(ois->ixc_lock);

	ois->pre_coef = coef;

	return ret;
}

#ifdef USE_KERNEL_VFS_READ_WRITE
int is_mcu_read_fw_ver(char *name, char *ver)
{
	int ret = 0;
	ulong size = 0;
	char buf[100] = {0, };
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(name, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		info("failed to open fw!!!\n");
		ret = -EIO;
		goto exit;
	}

	size = 4;
	fp->f_pos = 0x80F8;

	nread = kernel_read(fp, (char __user *)(buf), size, &fp->f_pos);
	if (nread != size) {
		err("failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto exit;
	}

exit:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, current->files);
	set_fs(old_fs);

	memcpy(ver, &buf[4], 3);
	memcpy(&ver[3], buf, 4);
	return ret;
}
#else
int is_mcu_read_fw_ver(struct is_core *core, char *name, char *ver)
{
	int ret = 0;
	char buf[100] = {0, };
	mm_segment_t old_fs;
	struct is_binary bin;
	struct is_mcu *mcu = NULL;
	mcu = is_get_mcu(core);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	setup_binary_loader(&bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&bin, NULL, name, mcu->dev);

	if (ret) {
		info("failed to open fw!!!\n");
		goto exit;
	}

	memcpy(buf, (char *)(bin.data + 0x80F8), 4);
exit:
	set_fs(old_fs);

	memcpy(ver, &buf[4], 3);
	memcpy(&ver[3], buf, 4);
	release_binary(&bin);
	return ret;
}
#endif

int is_ois_shift_mcu(struct v4l2_subdev *subdev)
{
	struct is_ois *ois = NULL;
	struct is_mcu *mcu = NULL;
	u8 data[2];
	int ret = 0;
	u16 reg;

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = mcu->ois;

	IXC_MUTEX_LOCK(ois->ixc_lock);

	data[0] = (ois_center_x & 0xFF);
	data[1] = (ois_center_x & 0xFF00) >> 8;
	reg = OIS_CMD_CENTER_X1;
	ret = is_ois_write_u16(reg, data);
	if (ret < 0)
		MCU_SET_ERR_PRINT(reg);

	data[0] = (ois_center_y & 0xFF);
	data[1] = (ois_center_y & 0xFF00) >> 8;
	reg = OIS_CMD_CENTER_Y1;
	ret = is_ois_write_u16(reg, data);
	if (ret < 0)
		MCU_SET_ERR_PRINT(reg);

	reg = OIS_CMD_MODE;
	ret = is_ois_write_u8(reg, 0x02);
	if (ret < 0)
		MCU_SET_ERR_PRINT(reg);

	IXC_MUTEX_UNLOCK(ois->ixc_lock);

	return ret;
}

int is_ois_set_centering_mcu(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *mcu = NULL;
	u16 reg;

	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = mcu->ois;

	reg = OIS_CMD_MODE;
	ret = is_ois_write_u8(reg, 0x05);
	if (ret) {
		MCU_SET_ERR_PRINT(reg);
		return ret;
	}

	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_CENTERING;

	return ret;
}

u8 is_read_ois_mode_mcu(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 mode = OPTICAL_STABILIZATION_MODE_OFF;
	struct is_mcu *mcu = NULL;
	u16 reg;

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		return OPTICAL_STABILIZATION_MODE_OFF;
	}

	reg = OIS_CMD_MODE;
	ret = is_ois_read_u8(reg, &mode);
	if (ret) {
		MCU_GET_ERR_PRINT(reg);
		return OPTICAL_STABILIZATION_MODE_OFF;
	}

	switch(mode) {
		case 0x00:
			mode = OPTICAL_STABILIZATION_MODE_STILL;
			break;
		case 0x01:
			mode = OPTICAL_STABILIZATION_MODE_VIDEO;
			break;
		case 0x05:
			mode = OPTICAL_STABILIZATION_MODE_CENTERING;
			break;
		case 0x13:
			mode = OPTICAL_STABILIZATION_MODE_STILL_ZOOM;
			break;
		case 0x14:
			mode = OPTICAL_STABILIZATION_MODE_VDIS;
			break;
		default:
			dbg_ois("%s: ois_mode value(%d)\n", __func__, mode);
			break;
	}

	return mode;
}

int ois_mcu_init_factory_mcu(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_mcu *mcu = NULL;
	struct is_ois *ois = NULL;
	struct is_ois_info *ois_pinfo = NULL;

	WARN_ON(!subdev);

	info("%s E\n", __func__);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_ois_get_phone_version(&ois_pinfo);

	ois = mcu->ois;
	ois->ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->coef = 0;
	ois->pre_coef = 255;
	ois->fadeupdown = false;
	ois->initial_centering_mode = false;
	ois->af_pos_wide = 0;
	ois->af_pos_tele = 0;
#ifdef CAMERA_2ND_OIS
	ois->ois_power_mode = -1;
#endif
	ois_pinfo->reset_check = false;

	/*****************************************************/
	/* Need to add code for preparation for factory mode */
	/*****************************************************/

	info("%s sensor(%d) X\n", __func__, ois->device);

	return ret;
}

void ois_mcu_check_valid_mcu(struct v4l2_subdev *subdev, u8 *value)
{
	int ret = 0;
	struct is_mcu *mcu = NULL;
	u8 data[2] = {0, };
	u16 temp = 0;
	u16 reg;

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		return;
	}

	ois_mcu_init_factory_mcu(subdev);

	cam_ois_set_aois_fac_mode_on();

	reg = OIS_CMD_ERROR_STATUS;
	ret = is_ois_read_u16(reg, data);
	if (ret != 0) {
		MCU_GET_ERR_PRINT(reg);
		goto p_err;
	} else {
		temp = (data[1] << 8) | data[0];
	}

	err("%s error reg value = 0x%04x", __func__, temp);

	*value = ((temp & 0x0600) >> 8);
p_err:
	cam_ois_set_aois_fac_mode_off();
	return;
}

bool ois_mcu_read_gyro_noise_mcu(struct is_core *core, long *x_value, long *y_value)
{
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	int xgnoise_val = 0, ygnoise_val = 0;
	int retries = 30;
	int ret = 0;
	bool result = true;
	struct is_mcu *mcu = NULL;
	u8 status = 0;
	u8 temp = 0;
	u8 RcvData[2] = {0, };
	int data = 0;
	u16 reg;

	mcu = core->sensor[0].mcu;
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		return false;
	}

	cam_ois_set_aois_fac_mode_on();

	/* OIS Servo Off */
	reg = OIS_CMD_START;
	ret = is_ois_write_u8(reg, 0x00);
	if (ret) {
		MCU_SET_ERR_PRINT(reg);
		cam_ois_set_aois_fac_mode_off();
		return false;
	}

	/* Waiting for Idle */
	do {
		reg = OIS_CMD_STATUS;
		ret = is_ois_read_u8(reg, &status);
		if (ret != 0) {
			MCU_GET_ERR_PRINT(reg);
			status = -EIO;
			break;
		}
		if (status == 0x01)
			break;

		if (--retries < 0) {
			if (ret < 0) {
				ret = -EIO;
				err("Read status failed!!!!, data = 0x%04x\n", status);
				break;
			}
			ret = -EBUSY;
			err("ois status is not idle, current status %d (retries:%d)", status, retries);
			break;
		}
		usleep_range(10000, 11000);
	} while (status != 0x01);

	/* Gyro Noise Measure Start */
	reg = OIS_CMD_SET_GYRO_NOISE;
	ret = is_ois_write_u8(reg, 0x01);
	if (ret) {
		MCU_SET_ERR_PRINT(reg);
		cam_ois_set_aois_fac_mode_off();
		return false;
	}

	/* Check Noise Measure End */
	retries = 100;
	do {
		reg = OIS_CMD_SET_GYRO_NOISE;
		ret = is_ois_read_u8(reg, &temp);
		if (ret) {
			MCU_GET_ERR_PRINT(reg);
			result = 0;
		}

		if (--retries < 0) {
			err("0x0029 is still 0x0. (retries : %d)", temp, retries);
			ret = -1;
			break;
		}
		usleep_range(10000, 11000);
	} while (temp != 0);

	reg = OIS_CMD_READ_GYRO_NOISE_X1;
	ret = is_ois_read_u16(reg, RcvData);
	if (ret) {
		MCU_GET_ERR_PRINT(reg);
		result = 0;
	}
	data = (RcvData[0] << 8) | RcvData[1];

	xgnoise_val = NTOHS(data);
	if (xgnoise_val > 0x7FFF)
		xgnoise_val = -((xgnoise_val ^ 0xFFFF) + 1);

	reg = OIS_CMD_READ_GYRO_NOISE_Y1;
	ret = is_ois_read_u16(reg, RcvData);
	if (ret) {
		MCU_GET_ERR_PRINT(reg);
		result = 0;
	}
	data = (RcvData[0] << 8) | RcvData[1];

	ygnoise_val = NTOHS(data);
	if (ygnoise_val > 0x7FFF)
		ygnoise_val = -((ygnoise_val ^ 0xFFFF) + 1);

	*x_value = xgnoise_val * 1000 / scale_factor;
	*y_value = ygnoise_val * 1000 / scale_factor;

	info("result: %d, stdev_x: %ld (0x%x), stdev_y: %ld (0x%x)", result, *x_value, xgnoise_val, *y_value, ygnoise_val);

	cam_ois_set_aois_fac_mode_off();

	return result;
}

#ifdef USE_OIS_HALL_DATA_FOR_VDIS
int ois_mcu_get_hall_data_mcu(struct v4l2_subdev *subdev, struct is_ois_hall_data *halldata)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *mcu = NULL;
	u16 hall_data[12] = {0, };
	u8 arr[28];
	int count = 0;
	int i = 0;
	u16 reg;
	WARN_ON(!subdev);

	mcu = (struct is_mcu *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = mcu->ois;

	IXC_MUTEX_LOCK(ois->ixc_lock);

	reg = OIS_CMD_VDIS_TIME_STAMP_1;
	ret = is_ois_read_multi(reg, &arr[0], 28);
	if (ret) {
		MCU_GET_ERR_PRINT(reg);
		IXC_MUTEX_UNLOCK(ois->ixc_lock);
		return ret;
	}
	IXC_MUTEX_UNLOCK(ois->ixc_lock);

	halldata->counter = (arr[3] << 24) | (arr[2] << 16) | (arr[1] << 8) | arr[0];

	for (i = 4; i < 28; i += 2) {
		hall_data[count] = arr[i] & 0x00ff;
		hall_data[count] |= (arr[i + 1] << 8) & 0xff00;

		count++;
	}
	halldata->X_AngVel[0] = hall_data[0];
	halldata->Y_AngVel[0] = hall_data[1];
	halldata->Z_AngVel[0] = hall_data[2];
	halldata->X_AngVel[1] = hall_data[3];
	halldata->Y_AngVel[1] = hall_data[4];
	halldata->Z_AngVel[1] = hall_data[5];
	halldata->X_AngVel[2] = hall_data[6];
	halldata->Y_AngVel[2] = hall_data[7];
	halldata->Z_AngVel[2] = hall_data[8];
	halldata->X_AngVel[3] = hall_data[9];
	halldata->Y_AngVel[3] = hall_data[10];
	halldata->Z_AngVel[3] = hall_data[11];

	return ret;
}
#endif

void ois_mcu_get_hall_position_mcu(struct is_core *core, u16 *targetPos, u16 *hallPos)
{
	int ret = 0;
	struct is_mcu *mcu = NULL;
	u8 pos_temp[2] = {0, };
	u16 pos = 0;
	u16 reg;

	info("%s : E\n", __func__);

	mcu = core->sensor[0].mcu;
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		return;
	}

	cam_ois_set_aois_fac_mode_on();

	/* set centering mode */
	reg = OIS_CMD_MODE;
	ret = is_ois_write_u8(reg, 0x05);
	if (ret) {
		MCU_SET_ERR_PRINT(reg);
	}

	/* enable position data read */
	reg = OIS_CMD_FWINFO_CTRL;
	ret = is_ois_write_u8(reg, 0x01);
	if (ret) {
		MCU_SET_ERR_PRINT(reg);
	}

	msleep(150);

	is_ois_read_u16(OIS_CMD_TARGET_POS_REAR_X, pos_temp);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[0] = pos;

	is_ois_read_u16(OIS_CMD_TARGET_POS_REAR_Y, pos_temp);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[1] = pos;

	is_ois_read_u16(OIS_CMD_HALL_POS_REAR_X, pos_temp);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[0] = pos;

	is_ois_read_u16(OIS_CMD_HALL_POS_REAR_Y, pos_temp);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[1] = pos;

	/* disable position data read */
	is_ois_write_u8(OIS_CMD_FWINFO_CTRL, 0x00);

	cam_ois_set_aois_fac_mode_off();

	info("%s : X (wide pos = 0x%04x, 0x%04x, 0x%04x, 0x%04x)\n", __func__, targetPos[0], targetPos[1], hallPos[0], hallPos[1]);
	return;
}

bool ois_mcu_get_active_mcu(void)
{
	return ois_hw_check;
}

void ois_mcu_parsing_raw_data_mcu(uint8_t *buf, long efs_size, long *raw_data_x, long *raw_data_y, long *raw_data_z)
{
	int i = 0;	/* i : Position of string */
	int j = 0;	/* j : Position below decimal point */
	char efs_data_pre[MAX_GYRO_EFS_DATA_LENGTH + 1];
	char efs_data_post[MAX_GYRO_EFS_DATA_LENGTH + 1];
	bool detect_point = false;
	int sign = 1;
	long raw_pre = 0, raw_post = 0;

	memset(efs_data_pre, 0x0, sizeof(efs_data_pre));
	memset(efs_data_post, 0x0, sizeof(efs_data_post));
	i = 0;
	j = 0;
	while ((*(buf + i)) != ',') {
		if (((char)*(buf + i)) == '-' ) {
			sign = -1;
			i++;
		}

		if (((char)*(buf + i)) == '.') {
			detect_point = true;
			i++;
			j = 0;
		}

		if (detect_point) {
			memcpy(efs_data_post + j, buf + i, 1);
			j++;
		} else {
			memcpy(efs_data_pre + j, buf + i, 1);
			j++;
		}

		if (++i > MAX_GYRO_EFS_DATA_LENGTH) {
			err("wrong EFS data.");
			break;
		}
	}
	i++;
	kstrtol(efs_data_pre, 10, &raw_pre);
	kstrtol(efs_data_post, 10, &raw_post);
	*raw_data_x = sign * (raw_pre * 1000 + raw_post);

	detect_point = false;
	j = 0;
	raw_pre = 0;
	raw_post = 0;
	sign = 1;
	memset(efs_data_pre, 0x0, sizeof(efs_data_pre));
	memset(efs_data_post, 0x0, sizeof(efs_data_post));
	while ((*(buf + i)) != ',') {
		if (((char)*(buf + i)) == '-' ) {
			sign = -1;
			i++;
		}

		if (((char)*(buf + i)) == '.') {
			detect_point = true;
			i++;
			j = 0;
		}

		if (detect_point) {
			memcpy(efs_data_post + j, buf + i, 1);
			j++;
		} else {
			memcpy(efs_data_pre + j, buf + i, 1);
			j++;
		}

		if (++i > MAX_GYRO_EFS_DATA_LENGTH) {
			err("wrong EFS data.");
			break;
		}
	}
	i++;
	kstrtol(efs_data_pre, 10, &raw_pre);
	kstrtol(efs_data_post, 10, &raw_post);
	*raw_data_y = sign * (raw_pre * 1000 + raw_post);

	detect_point = false;
	j = 0;
	raw_pre = 0;
	raw_post = 0;
	sign = 1;
	memset(efs_data_pre, 0x0, sizeof(efs_data_pre));
	memset(efs_data_post, 0x0, sizeof(efs_data_post));
	while (i < efs_size) {
		if (((char)*(buf + i)) == '-' ) {
			sign = -1;
			i++;
		}

		if (((char)*(buf + i)) == '.') {
			detect_point = true;
			i++;
			j = 0;
		}

		if (detect_point) {
			memcpy(efs_data_post + j, buf + i, 1);
			j++;
		} else {
			memcpy(efs_data_pre + j, buf + i, 1);
			j++;
		}

		if (i++ > MAX_GYRO_EFS_DATA_LENGTH) {
			err("wrong EFS data.");
			break;
		}
	}
	kstrtol(efs_data_pre, 10, &raw_pre);
	kstrtol(efs_data_post, 10, &raw_post);
	*raw_data_z = sign * (raw_pre * 1000 + raw_post);

	info("%s : X raw_x = %ld, raw_y = %ld, raw_z = %ld\n", __func__, *raw_data_x, *raw_data_y, *raw_data_z);
}

static struct is_ois_ops ois_ops_mcu = {
	.ois_init = is_ois_init_mcu,
	.ois_deinit = is_ois_deinit_mcu,
	.ois_set_mode = is_set_ois_mode_mcu,
	.ois_shift_compensation = is_ois_shift_compensation_mcu,
	.ois_fw_update = is_mcu_fw_update,
	.ois_self_test = is_ois_self_test_mcu,
	.ois_auto_test = is_ois_auto_test_mcu,
#ifdef CAMERA_2ND_OIS
	.ois_auto_test_rear2 = is_ois_auto_test_rear2_mcu,
	.ois_set_power_mode = is_ois_set_power_mode_mcu,
#endif
	.ois_check_fw = is_ois_check_fw_mcu,
	.ois_enable = is_ois_enable_mcu,
	.ois_offset_test = is_ois_offset_test_mcu,
	.ois_get_offset_data = is_ois_get_offset_data_mcu,
	.ois_gyro_sleep = is_ois_gyro_sleep_mcu,
	.ois_exif_data = is_ois_exif_data_mcu,
	.ois_read_status = is_ois_read_status_mcu,
	.ois_read_cal_checksum = is_ois_read_cal_checksum_mcu,
	.ois_set_coef = is_ois_set_coef_mcu,
	.ois_read_fw_ver = is_mcu_read_fw_ver,
	.ois_center_shift = is_ois_shift_mcu,
	.ois_set_center = is_ois_set_centering_mcu,
	.ois_read_mode = is_read_ois_mode_mcu,
	.ois_calibration_test = is_ois_gyro_cal_mcu,
	.ois_init_fac = ois_mcu_init_factory_mcu,
	.ois_check_valid = ois_mcu_check_valid_mcu,
	.ois_read_gyro_noise = ois_mcu_read_gyro_noise_mcu,
#ifdef USE_OIS_HALL_DATA_FOR_VDIS
	.ois_get_hall_data = ois_mcu_get_hall_data_mcu,
#endif
	.ois_get_hall_pos = ois_mcu_get_hall_position_mcu,
	.ois_get_active = ois_mcu_get_active_mcu,
	.ois_parsing_raw_data = ois_mcu_parsing_raw_data_mcu,
};

static struct is_aperture_ops aperture_ops_mcu = {
	.set_aperture_value = is_mcu_set_aperture,
	.aperture_deinit = is_mcu_deinit_aperture,
};

#ifdef CONFIG_SENSORCORE_MCU_CONTROL
struct ois_sensor_interface {
	void *core;
	void (*ois_func)(void *);
};

static struct ois_sensor_interface ois_control;
static struct ois_sensor_interface ois_reset;

extern int ois_fw_update_register(struct ois_sensor_interface *ois);
extern void ois_fw_update_unregister(void);
extern int ois_reset_register(struct ois_sensor_interface *ois);
extern void ois_reset_unregister(void);
#endif

static int is_aois_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct is_core *core;
	struct device_node *dnode;
	struct is_mcu *is_mcu = NULL;
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev_mcu = NULL;
	struct is_vendor_private *vendor_priv;
	struct v4l2_subdev *subdev_ois = NULL;
	struct is_device_ois *ois_device = NULL;
	struct is_ois *ois = NULL;
	struct is_aperture *aperture = NULL;
	struct v4l2_subdev *subdev_aperture = NULL;
	u32 sensor_id_len;
	const u32 *sensor_id_spec;
	const u32 *ois_gyro_spec;
	const u32 *aperture_delay_spec;
	u32 sensor_id[IS_SENSOR_COUNT] = {0, };
	int i = 0;

	ois_wide_init = false;
	ois_tele_init = false;
	ois_hw_check = false;

	core = is_get_is_core();
	if (!core) {
		err("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	dnode = pdev->dev.of_node;

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	if (sensor_id_len <= 0) {
		err("sensor_id_len is not valid");
		goto p_err;
	}

	ois_gyro_spec = of_get_property(dnode, "ois_gyro_direction", &mcu_init.ois_gyro_direction_len);
	if (ois_gyro_spec) {
		mcu_init.ois_gyro_direction_len /= (unsigned int)sizeof(*ois_gyro_spec);
		ret = of_property_read_u32_array(dnode, "ois_gyro_direction",
				mcu_init.ois_gyro_direction, mcu_init.ois_gyro_direction_len);
		if (ret)
			info("ois_gyro_direction read is fail(%d)", ret);
	}

	aperture_delay_spec = of_get_property(dnode, "aperture_control_delay", &mcu_init.aperture_delay_list_len);
	if (aperture_delay_spec) {
		mcu_init.aperture_delay_list_len /= (unsigned int)sizeof(*aperture_delay_spec);
		ret = of_property_read_u32_array(dnode, "aperture_control_delay",
				mcu_init.aperture_delay_list, mcu_init.aperture_delay_list_len);
		if (ret)
				info("aperture_control_delay read is fail(%d)", ret);
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		if (!device) {
			err("sensor device is NULL");
			ret = -EPROBE_DEFER;
			goto p_err;
		}
	}

	is_mcu = kzalloc(sizeof(struct is_mcu) * sensor_id_len, GFP_KERNEL);
	if (!is_mcu) {
		err("is_mcu is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_mcu = kzalloc(sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_mcu) {
		err("subdev_mcu is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ois = kzalloc(sizeof(struct is_ois) * sensor_id_len, GFP_KERNEL);
	if (!ois) {
		err("is_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_ois = kzalloc(sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_ois) {
		err("subdev_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ois_device = kzalloc(sizeof(struct is_device_ois), GFP_KERNEL);
	if (!ois_device) {
		err("is_device_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	aperture = kzalloc(sizeof(struct is_aperture) * sensor_id_len, GFP_KERNEL);
	if (!aperture) {
		err("aperture is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_aperture = kzalloc(sizeof(struct v4l2_subdev)  * sensor_id_len, GFP_KERNEL);
	if (!subdev_aperture) {
		err("subdev_aperture is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	vendor_priv = core->vendor.private_data;
	ois_device->ois_ops = &ois_ops_mcu;

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);

		is_mcu[i].name = MCU_NAME_STM32;
		is_mcu[i].subdev = &subdev_mcu[i];
		is_mcu[i].device = sensor_id[i];
		is_mcu[i].private_data = core;
		is_mcu[i].dev = &pdev->dev;

		ois[i].subdev = &subdev_ois[i];
		ois[i].device = sensor_id[i];
		ois[i].ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
		ois[i].pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
		ois[i].ois_shift_available = false;
		ois[i].ois_ops = &ois_ops_mcu;

		is_mcu[i].subdev_ois = &subdev_ois[i];
		is_mcu[i].ois = &ois[i];
		is_mcu[i].ois_device = ois_device;

		if (i == 0) {
			aperture[i].start_value = F2_4;
			aperture[i].new_value = 0;
			aperture[i].cur_value = 0;
			aperture[i].step = APERTURE_STEP_STATIONARY;
			aperture[i].subdev = &subdev_aperture[i];
			aperture[i].aperture_ops = &aperture_ops_mcu;

			mutex_init(&aperture[i].control_lock);

			is_mcu[i].aperture = &aperture[i];
			is_mcu[i].subdev_aperture = aperture[i].subdev;
			is_mcu[i].aperture_ops = &aperture_ops_mcu;
		}

		device = &core->sensor[sensor_id[i]];
		device->subdev_mcu = &subdev_mcu[i];
		device->mcu = &is_mcu[i];

		v4l2_set_subdevdata(&subdev_mcu[i], &is_mcu[i]);
		v4l2_set_subdev_hostdata(&subdev_mcu[i], device);

		probe_info("%s done\n", __func__);
	}

#ifdef CONFIG_SENSORCORE_MCU_CONTROL
	ois_control.core = core;
	ois_control.ois_func = &is_ois_fw_update_from_sensor;
	ret = ois_fw_update_register(&ois_control);
	if (ret)
		err("ois_fw_update_register failed: %d\n", ret);

	ois_reset.core = core;
	ois_reset.ois_func = &is_ois_reset_mcu;
	ret = ois_reset_register(&ois_reset);
	if (ret)
		err("ois_reset_register failed: %d\n", ret);
#endif

	return ret;

p_err:
	if (is_mcu)
		kfree(is_mcu);

	if (subdev_mcu)
		kfree(subdev_mcu);

	if (ois)
		kfree(ois);

	if (subdev_ois)
		kfree(subdev_ois);

	if (ois_device)
		kfree(ois_device);

	if (aperture)
		kfree(aperture);

	if (subdev_aperture)
		kfree(subdev_aperture);

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id sensor_aois_match[] = {
	{
		.compatible = "samsung,sensor-aois-mcu",
	},
	{},
};
#endif

struct platform_driver sensor_aois_platform_driver = {
	.probe	= is_aois_probe,
	.driver = {
		.name	= AOIS_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sensor_aois_match,
#endif
	},
};

struct platform_driver *get_aois_platform_driver(void)
{
	return &sensor_aois_platform_driver;
}

#ifndef MODULE
static int __init sensor_aois_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_aois_platform_driver,
							is_aois_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_aois_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_aois_init);
#endif

MODULE_DESCRIPTION("Advanced OIS driver for sensorhub");
MODULE_AUTHOR("Sanggyu choi <sgtop9.choi@samsung.com>");
MODULE_LICENSE("GPL v2");
