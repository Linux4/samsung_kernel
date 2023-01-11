/* Marvell ISP OV2680 Driver
 *
 * Copyright (C) 2009-2014 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image OV2680
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
#include "ov2680.h"

static int OV2680_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int temp1, temp2;
	int pre_div0, pre_div2x, div_loop, VCO, sp_div, lane_div;
	int pre_div2x_map[] = {2, 3, 4, 5, 6, 8, 12, 16};

	struct b52_sensor *sensor = to_b52_sensor(sd);

	b52_sensor_call(sensor, i2c_read, 0x3088, &temp1, 1);
	if (temp1 & 0x10)
		pre_div0 = 2;
	else
		pre_div0 = 1;

	b52_sensor_call(sensor, i2c_read, 0x3080, &temp1, 1);
	temp2 = temp1 & 0x07;
	pre_div2x = pre_div2x_map[temp2];

	b52_sensor_call(sensor, i2c_read, 0x3081, &temp1, 1);
	temp2 = ((temp1 & 0x03) << 2);
	b52_sensor_call(sensor, i2c_read, 0x3082, &temp1, 1);
	div_loop = temp1 + temp2;

	VCO = mclk * 2 / pre_div0 / pre_div2x * div_loop;

	b52_sensor_call(sensor, i2c_read, 0x3086, &temp1, 1);
	sp_div = (temp1 & 0x0f) + 1;
	b52_sensor_call(sensor, i2c_read, 0x3087, &temp1, 1);
	lane_div = 1 << (temp1 & 0x03);

	*rate = VCO / sp_div / lane_div;

	return 0;
}

static int OV2680_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	OV2680_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 70;
	dphy_desc->hs_zero  = 100;

	return 0;
}

static int OV2680_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int temp1, temp2;
	int pre_div0, pre_div2x, div_loop, VCO, sp_div, sys_div;
	int pre_div2x_map[] = {2, 3, 4, 5, 6, 8, 12, 16};
	struct b52_sensor *sensor = to_b52_sensor(sd);

	b52_sensor_call(sensor, i2c_read, 0x3088, &temp1, 1);
	if (temp1 & 0x10)
		pre_div0 = 2;
	else
		pre_div0 = 1;

	b52_sensor_call(sensor, i2c_read, 0x3080, &temp1, 1);
	temp2 = temp1 & 0x07;
	pre_div2x = pre_div2x_map[temp2];

	b52_sensor_call(sensor, i2c_read, 0x3081, &temp1, 1);
	temp2 = ((temp1 & 0x03) << 2);
	b52_sensor_call(sensor, i2c_read, 0x3082, &temp1, 1);
	div_loop = temp1 + temp2;

	VCO = mclk * 2 / pre_div0 / pre_div2x * div_loop;

	b52_sensor_call(sensor, i2c_read, 0x3086, &temp1, 1);
	sp_div = (temp1 & 0x0f) + 1;
	b52_sensor_call(sensor, i2c_read, 0x3084, &temp1, 1);
	sys_div = (temp1 & 0x0f) + 1;

	*rate = VCO / sp_div / sys_div;

	return 0;
}
