/* Marvell ISP OV5648 Driver
 *
 * Copyright (C) 2009-2014 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image OV5648
 *
 * Copyright (c) 2009 Mauro Carvalho Chehab (mchehab@redhat.com)
 * This code is placed under the terms of the GNU General Public License v2
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>

#include <asm/div64.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include "ov5648.h"
/*static int OV5648_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	return 0;
}*/

static int OV5648_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	return 0;
}

static int OV5648_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	return 0;
}
/*static void OV5648_write_i2c(struct b52_sensor *sensor, u16 reg, u8 val)
{
	b52_sensor_call(sensor, i2c_write, reg, val, 1);
}
static u8 OV5648_read_i2c(struct b52_sensor *sensor, u16 reg)
{
	int temp1;
	b52_sensor_call(sensor, i2c_read, reg, &temp1, 1);
	return temp1;
}
static int OV5648_OTP_access_start(struct b52_sensor *sensor)
{
	return 0;
}
static int OV5648_OTP_access_end(struct b52_sensor *sensor)
{
	return 0;
}
static int check_otp_info(struct b52_sensor *sensor, int index)
{
	return 0;
}
static int check_otp_wb(struct b52_sensor *sensor, int index)
{
	return 0;
}
static int check_otp_lenc(struct b52_sensor *sensor, int index)
{
	return 0;
}
static int read_otp_info(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp)
{
	return 0;
}
static int read_otp_wb(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp)
{
	return 0;
}
static int read_otp_lenc(struct b52_sensor *sensor, int index, u8 *lenc)
{
	return 0;
}
static int update_awb_gain(struct b52_sensor *sensor, int r_gain,
				int g_gain, int b_gain)
{
	return 0;
}
static int update_lenc(struct b52_sensor *sensor, u8 *lenc)
{
	return 0;
}*/
static int update_otp_info(struct b52_sensor *sensor,
					struct b52_sensor_otp *otp)
{
	return 0;
}

static int update_otp_wb(struct b52_sensor *sensor, struct b52_sensor_otp *otp)
{
	return 0;
}
static int update_otp_lenc(struct b52_sensor *sensor)
{
	return 0;
}
static int OV5648_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	int ret = 0;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (otp->otp_type == SENSOR_TO_SENSOR) {
		ret = update_otp_info(sensor, otp);
		if (ret < 0)
			return ret;
		ret = update_otp_wb(sensor, otp);
		if (ret < 0)
			return ret;
		ret = update_otp_lenc(sensor);
		if (ret < 0)
			return ret;
	}
	return 0;
}

