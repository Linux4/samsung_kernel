/*
 * ov5640 Camera Driver
 *
 * Copyright (c) 2010 Marvell Ltd.
 * Angela Wan <jwan@marvell.com>
 * Kassey Lee <ygli@marvell.com>
 *
 * Based on linux/drivers/media/video/mt9m001.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <mach/camera.h>

#include "ov5640.h"

MODULE_DESCRIPTION("OmniVision OV5640 Camera Driver");
MODULE_LICENSE("GPL");

#define REG_PIDH    0x300a
#define REG_PIDL    0x300b

int frame_rate;

static const struct ov5640_datafmt ov5640_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_JPEG_1X8, V4L2_COLORSPACE_JPEG},
};

static struct ov5640_win_size {
	int width;
	int height;
#ifdef CONFIG_PXA95x
	int phy_cfg_id;
#endif
} ov5640_win_sizes[] = {
#ifdef CONFIG_PXA95x
	/* QCIF */
	{
		.width = 176,
		.height = 144,
		.phy_cfg_id = 0,
	},
	/* QHVGA */
	{
		.width = 240,
		.height = 160,
		.phy_cfg_id = 0,
	},
	/* QVGA */
	{
		.width = 320,
		.height = 240,
		.phy_cfg_id = 0,
	},
	/* VGA */
	{
		.width = 640,
		.height = 480,
		.phy_cfg_id = 0,
	},
	/* 720 */
	{
		.width = 1280,
		.height = 720,
		.phy_cfg_id = 1,
	},
	/* 1080 */
	{
		.width = 1920,
		.height = 1080,
		.phy_cfg_id = 1,
	},
	/* full */
	{
		.width = 2592,
		.height = 1944,
		.phy_cfg_id = 2,
	},
#else
	/* QCIF */
	{
		.width = 176,
		.height = 144,
	},
	/* QVGA */
	{
		.width = 320,
		.height = 240,
	},
	/* 480*320 */
	{
		.width = 480,
		.height = 320,
	},
	/* VGA */
	{
		.width = 640,
		.height = 480,
	},
	/* D1 */
	{
		.width = 720,
		.height = 480,
	},
	/* full */
	{
		.width = 2592,
		.height = 1944,
	},
#endif
};

#define N_OV5640_RESOLUTIONS_YUV (ARRAY_SIZE(ov5640_win_sizes))

/* capture jpeg size */
static struct ov5640_win_size ov5640_win_sizes_jpeg[] = {
#ifdef CONFIG_PXA95x
	/* full */
	{
		.width = 2592,
		.height = 1944,
		.phy_cfg_id = 0,
	},
#else
	/* full */
	{
		.width = 2592,
		.height = 1944,
	},
	/* 3M */
	{
		.width = 2048,
		.height = 1536,
	},
#endif
};

#define N_OV5640_RESOLUTIONS_JPEG (ARRAY_SIZE(ov5640_win_sizes_jpeg))

#ifdef CONFIG_PXA95x
static struct mipi_phy ov5640_timings[] = {
	{ /* ov5640 default mipi PHY config */
		.cl_termen	= 0x00,
		.cl_settle	= 0x04,
		.hs_termen	= 0x05,
		.hs_settle	= 0x09,
		.hs_rx_to	= 0xFFFF,
		.lane		= 2,
	},
	{ /* For 384Mhz: 720p or 1080p */
		.cl_termen	= 0x00,
		.cl_settle	= 0x04,
		.hs_termen	= 0x0F,
		.hs_settle	= 0x19,
		.hs_rx_to	= 0xFFFF,
		.lane		= 2,
	},
	{ /* For 5M YUV */
		.cl_termen	= 0x00,
		.cl_settle	= 0x04,
		.hs_termen	= 0x07,
		.hs_settle	= 0x0F,
		.hs_rx_to	= 0xFFFF,
		.lane		= 2,
	},
};
#endif

/* Find a data format by a pixel code in an array */
static const struct ov5640_datafmt *ov5640_find_datafmt(
	enum v4l2_mbus_pixelcode code, const struct ov5640_datafmt *fmt,
	int n)
{
	int i;
	for (i = 0; i < n; i++)
		if (fmt[i].code == code)
			return fmt + i;

	return NULL;
}

static struct ov5640 *to_ov5640(const struct i2c_client
					     *client)
{
	return container_of(i2c_get_clientdata(client),
			    struct ov5640, subdev);
}

int ov5640_read(struct i2c_client *i2c, u16 reg, unsigned char *value)
{
	unsigned char msgbuf0[2];
	unsigned char msgbuf1[1];
	struct i2c_adapter *adap = i2c->adapter;
	struct i2c_msg msg[2] = {{i2c->addr, i2c->flags, 2, msgbuf0},
				 {i2c->addr, i2c->flags | I2C_M_RD, 1, msgbuf1},
				};
	int num = 2, ret;

	if (value == NULL)
		return -EINVAL;

	msgbuf0[0] = (unsigned char)(reg>>8);	/* command */
	msgbuf0[1] = (unsigned char)reg;	/* command */

	ret = i2c_transfer(adap, msg, num);
	if (ret < 0)
		goto out;
	memcpy(value, msgbuf1, 1);
out:
	return (ret < 0) ? ret : 0;
}

int ov5640_write(struct i2c_client *c, u16 reg, unsigned char value)
{
	u8 data[3];
	int ret = 0;
	data[0] = reg >> 8;
	data[1] = reg;
	data[2] = value;
	ret = i2c_master_send(c, data, 3);
	return (ret < 0) ? ret : 0;
}

/*
 * Write a list of register settings; ff/ff stops the process.
 */
static int ov5640_write_array(struct i2c_client *c, struct regval_list * vals)
{
	int i = 0;
	int ret = 0;
	while (vals->reg_num != OV5640_MIPI_END_ADDR
	       || vals->value != OV5640_MIPI_END_VAL) {
		ret = ov5640_write(c, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;

		vals++;
		i++;
	}
	return ret;
}

static int ov5640_detect(struct i2c_client *client)
{
	unsigned char v = 0;
	int ret = 0;

	ret = ov5640_read(client, REG_PIDH, &v);

	if (ret < 0)
		return ret;
	if (v != 0x56)
		return -ENODEV;

	ret = ov5640_read(client, REG_PIDL, &v);
	if (ret < 0)
		return ret;
	if (v != 0x40)
		return -ENODEV;
	dev_err(&client->dev, "ov5640 detected 0x%x\n", v);

	return 0;
}

static int ov5640_firmware_download(struct i2c_client *client)
{
	int ret = 0, i, j, size;
	char data[258];

	size = N_FIRMWARE_SIZES_OV5640;
	if (unlikely(size <= 0)) {
		dev_err(&client->dev, "No AF firmware available\n");
		return -ENODEV;
	}

	/* Actually start to download firmware */
	for (i = 0; i < size; i++) {
		data[0] = firmware_regs_ov5640[i].reg_base >> 8;
		data[1] = firmware_regs_ov5640[i].reg_base;
		for (j = 0; j < firmware_regs_ov5640[i].len; j++)
			data[j+2] = firmware_regs_ov5640[i].value[j];
		ret = i2c_master_send(client, data, firmware_regs_ov5640[i].len+2);
		if (ret < 0) {
			dev_err(&client->dev, "i2c error %s %d\n",
				__func__, __LINE__);
			break;
		}
	}

	dev_info(&client->dev, "AF firmware downloaded\n");

	return (ret > 0) ? 0 : ret;
}

#ifdef CONFIG_PXA95x
static int ov5640_get_mipi_phy(struct i2c_client *client, __s32 *value)
{
	struct ov5640 *ov5640 = to_ov5640(client);
	int idx = 0;

	if (unlikely((void *)value == NULL))
		return -EPERM;

	if (ov5640->fmt_is_jpeg == 1) {
		idx = ov5640_win_sizes_jpeg[ov5640->res_idx].phy_cfg_id;
	} else {
		idx = ov5640_win_sizes[ov5640->res_idx].phy_cfg_id;
	}

	*value = (__s32)&(ov5640_timings[idx]);
	return 0;
}
#endif

#ifdef CONFIG_PXA95x
static int ov5640_control_flash(struct v4l2_subdev *sd, struct v4l2_control *ctrl, bool op)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	int ret = 0;

	icl = to_soc_camera_link(icd);
	if (!icl) {
		dev_err(&client->dev, "ov5640 driver needs platform data\n");
		return -EINVAL;
	}

	if (icl->priv && icl->flags & 0x80000000) {
		struct sensor_platform_data *sensor = icl->priv;
		if (sensor->v4l2_flash_if)
			ret = sensor->v4l2_flash_if((void *)ctrl, op);
	}

	return ret;
}
#endif

static int ov5640_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret;

	switch (ctrl->id) {
#ifdef CONFIG_PXA95x
	case V4L2_CID_PRIVATE_GET_MIPI_PHY:
		ret = ov5640_get_mipi_phy(client, &ctrl->value);
		break;
	case V4L2_CID_FLASH_FAULT:
	case V4L2_CID_FLASH_STROBE_STATUS:
		ret = ov5640_control_flash(sd, ctrl, 0);
		break;
#endif
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int ov5640_g_chip_ident(struct v4l2_subdev *sd,
				   struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *ov5640 = to_ov5640(client);

	id->ident = ov5640->model;
	id->revision = 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5640_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return ov5640_read(client, (u16) reg->reg,
			       (unsigned char *)&(reg->val));
}

static int ov5640_s_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return ov5640_write(client, (u16) reg->reg,
				(unsigned char)reg->val);
}
#endif

static int set_stream(struct i2c_client *client, int enable)
{
	int ret = 0;
	if (enable) {
		ret = ov5640_write(client, 0x4202, 0x00);
		if (ret < 0)
			goto out;
	} else {
		ret = ov5640_write(client, 0x4202, 0x0f);
		if (ret < 0)
			goto out;
	}
out:
	return ret;
}

static int ov5640_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = set_stream(client, enable);
	if (ret < 0)
		dev_err(&client->dev, "ov5640 set stream error\n");
	return ret;
}

static int ov5640_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(ov5640_colour_fmts))
		return -EINVAL;
	*code = ov5640_colour_fmts[index].code;
	return 0;
}

/* select nearest higher resolution */
static int ov5640_res_roundup(struct i2c_client *client, u32 *width, u32 *height)
{
	int i;
	struct ov5640 *ov5640 = to_ov5640(client);

	if (ov5640->fmt_is_jpeg == 1) {
		for (i = 0; i < N_OV5640_RESOLUTIONS_JPEG; i++)
			if ((ov5640_win_sizes_jpeg[i].width >= *width) &&
			    (ov5640_win_sizes_jpeg[i].height >= *height)) {
				*width = ov5640_win_sizes_jpeg[i].width;
				*height = ov5640_win_sizes_jpeg[i].height;
				return i;
			}
	} else {
		for (i = 0; i < N_OV5640_RESOLUTIONS_YUV; i++)
			if ((ov5640_win_sizes[i].width >= *width) &&
			    (ov5640_win_sizes[i].height >= *height)) {
				*width = ov5640_win_sizes[i].width;
				*height = ov5640_win_sizes[i].height;
				return i;
			}
	}

	return -1;
}

static int ov5640_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *ov5640 = to_ov5640(client);
	const struct ov5640_datafmt *fmt;

	fmt = ov5640_find_datafmt(mf->code, ov5640_colour_fmts,
				   ARRAY_SIZE(ov5640_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "ov5640 unsupported color format!\n");
		return -EINVAL;
	}

	/* distinguish JPEG from YUV for different d-phy config */
	ov5640->fmt_is_jpeg = 0;

	if (mf->code == V4L2_MBUS_FMT_JPEG_1X8) {
		ov5640->fmt_is_jpeg = 1;
	}

	ov5640->res_idx = ov5640_res_roundup(client, &mf->width, &mf->height);

	if (ov5640->res_idx < 0) {
		dev_err(&client->dev, "ov5640 unsupported resolution!\n");
		return -EINVAL;
	}

	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_JPEG;

	return 0;
}

static int ov5640_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct ov5640_datafmt *fmt;
	struct regval_list *pregs = NULL;
	struct regval_list *pregs_default = NULL;

	fmt = ov5640_find_datafmt(mf->code, ov5640_colour_fmts,
				   ARRAY_SIZE(ov5640_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "ov5640 unsupported color format!\n");
		return -EINVAL;
	}

#ifdef CONFIG_PXA95x
	/*
	 * for pxa97x, we need send global init settings
	 * for all resolutions except 720p/1080p.
	 */
	pregs_default = init_global_tab;
#endif

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
		switch (mf->width) {
		case 176:
			pregs = yuv_QCIF_tab;
			dev_info(&client->dev, "choose qcif setting!\n");
			break;
		case 240:
			pregs = yuv_QHVGA_tab;
			dev_info(&client->dev, "choose qhvga setting!\n");
			break;
		case 320:
			pregs = yuv_QVGA_tab;
			dev_info(&client->dev, "choose qvga setting!\n");
			break;
		case 480:
			pregs = yuv_Wallpaper_tab;
			dev_info(&client->dev, "choose 480*320 setting!\n");
			break;
		case 640:
			pregs = yuv_VGA_tab;
			dev_info(&client->dev, "choose vga setting!\n");
			break;
		case 720:
			pregs = yuv_D1_tab;
			dev_info(&client->dev, "choose d1 setting!\n");
			break;
		case 1280:
#ifdef CONFIG_PXA95x
			pregs_default = NULL;
#endif
			pregs = yuv_720P_tab;
			dev_info(&client->dev, "choose 720P setting!\n");
			break;
		case 1920:
#ifdef CONFIG_PXA95x
			pregs_default = NULL;
#endif
			pregs = yuv_1080P_tab;
			dev_info(&client->dev, "choose 1080P setting!\n");
			break;
		case 2592:
			pregs = yuv_5M_tab;
			dev_info(&client->dev, "choose 5M setting!\n");
			break;
		default:
			dev_err(&client->dev, "unsupported YUV size"
					"%s %d!\n", __func__, __LINE__);
			ret = -EINVAL;
			goto out;
			break;
		}
		break;
	case V4L2_MBUS_FMT_JPEG_1X8:
		switch (mf->width) {
		case 2592:
			pregs_default = jpg_default_tab;
			dev_info(&client->dev, "choose 5M jpeg setting!\n");
			break;
		case 2048:
			pregs_default = jpg_default_tab;
			pregs = jpg_QXGA_tab;
			dev_info(&client->dev, "choose 3M jpeg setting!\n");
			break;
		default:
			dev_info(&client->dev, "unsupported JPEG size!\n");
			ret = -EINVAL;
			goto out;
			break;
		}
		break;
	default:
		dev_err(&client->dev, "unsupported format"
				"%s %d!\n", __func__, __LINE__);
		ret = -EINVAL;
		goto out;
		break;
	}
	if (pregs_default) {
		ret = ov5640_write_array(client, pregs_default);
		if (ret < 0) {
			dev_err(&client->dev, "set default registers err\n");
			goto out;
		}
	}
	if (pregs)
		ret = ov5640_write_array(client, pregs);
out:
	return ret;
}

static int ov5640_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *ov5640 = to_ov5640(client);

	mf->width		= ov5640->rect.width;
	mf->height		= ov5640->rect.height;
	mf->code		= V4L2_MBUS_FMT_UYVY8_2X8;
	mf->field		= V4L2_FIELD_NONE;
	mf->colorspace		= V4L2_COLORSPACE_JPEG;
	return 0;
}

static int ov5640_enum_fsizes(struct v4l2_subdev *sd,
				struct v4l2_frmsizeenum *fsize)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!fsize)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	/* abuse pixel_format, in fact, it is xlate->code*/
	switch (fsize->pixel_format) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
		if (fsize->index >= ARRAY_SIZE(ov5640_win_sizes)) {
			return -EINVAL;
		}
		fsize->discrete.height = ov5640_win_sizes[fsize->index].height;
		fsize->discrete.width = ov5640_win_sizes[fsize->index].width;
		break;
	case V4L2_MBUS_FMT_JPEG_1X8:
		if (fsize->index >= ARRAY_SIZE(ov5640_win_sizes_jpeg)) {
			return -EINVAL;
		}
		fsize->discrete.height =
			ov5640_win_sizes_jpeg[fsize->index].height;
		fsize->discrete.width =
			ov5640_win_sizes_jpeg[fsize->index].width;
		break;
	default:
		dev_err(&client->dev, "ov5640 unsupported format!\n");
		return -EINVAL;
	}
	return 0;
}

static const struct v4l2_queryctrl ov5640_controls[] = {
	{
		.id = V4L2_CID_PRIVATE_FIRMWARE_DOWNLOAD,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "AF/FD etc. firmware download",
	},
#ifdef CONFIG_PXA95x
	{
		.id = V4L2_CID_PRIVATE_GET_MIPI_PHY,
		.type = V4L2_CTRL_TYPE_CTRL_CLASS,
		.name = "get mipi timing"
	},
	{
		.id = V4L2_CID_FLASH_LED_MODE,
		.type = V4L2_CTRL_TYPE_CTRL_CLASS,
		.name = "set LED flash mode"
	},
#endif
};

static int ov5640_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_FIRMWARE_DOWNLOAD:
		ret = ov5640_firmware_download(client);
		break;
#ifdef CONFIG_PXA95x
	case V4L2_CID_FLASH_LED_MODE:
	case V4L2_CID_FLASH_STROBE_SOURCE:
	case V4L2_CID_FLASH_STROBE:
	case V4L2_CID_FLASH_STROBE_STOP:
	case V4L2_CID_FLASH_TIMEOUT:
	case V4L2_CID_FLASH_INTENSITY:
	case V4L2_CID_FLASH_TORCH_INTENSITY:
		ret = ov5640_control_flash(sd, ctrl, 1);
		break;
#endif
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int ov5640_init(struct v4l2_subdev *sd, u32 plat)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	ov5640_write(client, 0x3103, 0x11);
	ov5640_write(client, 0x3008, 0x82);
	mdelay(5);
	ret = ov5640_write_array(client, init_global_tab);
	return ret;
}

static int ov5640_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *inter)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int	pre_div, multiplier, vco_freq;
	int sys_div, pll_rdiv, bit_div, sclk_rdiv, mipi_div;
	int sys_clk, vts, hts, mipi_bit_rate, mipi_clk;
	int mclk;
	u8 val;

	mclk = inter->pad;
	/* get sensor system clk */
	/* vco frequency */
	ov5640_read(client, 0x3037, &val);
	/* There should be a complicated algorithm
	   including double member.
	 */
	pll_rdiv = (val >> 4) & 0x1;
	val = val & 0xf;
	pre_div = (val == 0) ? 2 : ((val == 5) ? 3: ((val == 7) ? 5 : ((val > 8) ? 2 : (val * 2))));
	ov5640_read(client, 0x3036, &val);
	multiplier = (val < 128) ? val : (val/2*2);

	vco_freq = mclk / pre_div * multiplier * 2;
	dev_dbg(&client->dev, "vco_freq: %d, mclk:%d,pre_div:%d,"
			"multiplier:%d\n", vco_freq, mclk, pre_div, multiplier);

	ov5640_read(client, 0x3035, &val);
	val = (val >> 4) & 0xf;
	sys_div = (val > 0) ? val : 16;
	pll_rdiv = (pll_rdiv == 0) ? 1 : 2;
	ov5640_read(client, 0x3034, &val);
	val = val & 0xf;
	bit_div = (val / 4 == 2) ? 2 : 1;
	ov5640_read(client, 0x3108, &val);
	val = val & 0x3;
	sclk_rdiv = (val == 0) ? 1 : ((val == 1) ? 2
			: ((val == 2) ? 4 : 8));

	sys_clk = vco_freq / sys_div / pll_rdiv / bit_div
		/ sclk_rdiv;
	dev_dbg(&client->dev, "sys_clk: %d, sys_div:%d,pll_rdiv:%d,"
			"bit_div:%d,sclk_rdiv:%d\n", sys_clk, sys_div,
			pll_rdiv, bit_div, sclk_rdiv);

	/* mipi bit rate */
	ov5640_read(client, 0x3035, &val);
	val = val & 0xf;
	mipi_div = (val > 0) ? val : 16;

	mipi_bit_rate = vco_freq / sys_div / mipi_div;

	/* mipi clk */
	mipi_clk = mipi_bit_rate / 2;
	dev_dbg(&client->dev, "MIPI bit rate: %d, SYS clk: %d, MIPI Clock"
			"clk: %d\n", mipi_bit_rate, sys_clk, mipi_clk);

	inter->pad = mipi_clk;

	/* get sensor hts & vts */
	ov5640_read(client, 0x380c, &val);
	hts = val & 0xf;
	hts <<= 8;
	ov5640_read(client, 0x380d, &val);
	hts += val & 0xff;
	ov5640_read(client, 0x380e, &val);
	vts = val & 0xf;
	vts <<= 8;
	ov5640_read(client, 0x380f, &val);
	vts += val & 0xff;

	if (!hts || !vts)
		return -EINVAL;
	frame_rate = sys_clk * 1000000 / (hts * vts);
	dev_dbg(&client->dev, "frame_rate: %d,"
			"hts:%x, vts:%x\n", frame_rate, hts, vts);

	inter->interval.numerator = frame_rate;
	inter->interval.denominator = 1;

	return 0;
}

static int ov5640_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_device *icd = client->dev.platform_data;
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	unsigned char v;

	/*
	 * Pls note the following setting is only valid for
	 * MIPI mode. DVP mode is not verified.
	 */
	if (on) {
		if (icl->power)
			icl->power(icd->pdev, POWER_RESTORE);
		ov5640_read(client, (u16)0x300e, &v);
		v  &= ~0x18;
		ov5640_write(client, (u16)0x300e, v);
	} else {
		ov5640_read(client, (u16)0x300e, &v);
		v  |= 0x18;
		ov5640_write(client, (u16)0x300e, v);
		if (icl->power)
			icl->power(icd->pdev, POWER_SAVING);
	}

	return 0;
}

static struct v4l2_subdev_core_ops ov5640_subdev_core_ops = {
	.g_ctrl	= ov5640_g_ctrl,
	.s_ctrl	= ov5640_s_ctrl,
	.g_chip_ident = ov5640_g_chip_ident,
	.init = ov5640_init,
	.s_power = ov5640_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov5640_g_register,
	.s_register = ov5640_s_register,
#endif
};

static struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.s_stream = ov5640_s_stream,
	.g_mbus_fmt = ov5640_g_fmt,
	.s_mbus_fmt = ov5640_s_fmt,
	.try_mbus_fmt = ov5640_try_fmt,
	.enum_mbus_fsizes = ov5640_enum_fsizes,
	.enum_mbus_fmt = ov5640_enum_fmt,
	.g_frame_interval =  ov5640_g_frame_interval,
};

static struct v4l2_subdev_ops ov5640_subdev_ops = {
	.core = &ov5640_subdev_core_ops,
	.video = &ov5640_subdev_video_ops,
};

static int ov5640_video_probe(struct soc_camera_device *icd,
				   struct i2c_client *client)
{
	struct ov5640 *ov5640 = to_ov5640(client);
	int ret = 0;

	ret = ov5640_detect(client);

	if (ret)
		goto out;
	dev_err(&client->dev, "OmniVision ov5640 sensor detected\n");

#ifdef CONFIG_VIDEO_MV
	mv_set_sensor_attached(true);
#endif

	ov5640->model = V4L2_IDENT_OV5640;

out:
	return ret;

}

static int ov5640_probe(struct i2c_client *client,
			    const struct i2c_device_id *did)
{
	struct ov5640 *ov5640;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret;

	if (!icd) {
		dev_err(&client->dev, "missing soc-camera data!\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	ov5640 = kzalloc(sizeof(struct ov5640), GFP_KERNEL);
	if (!ov5640)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&ov5640->subdev, client, &ov5640_subdev_ops);
	ov5640->rect.left = 0;
	ov5640->rect.top = 0;
	ov5640->rect.width = 640;
	ov5640->rect.height = 480;
	ov5640->pixfmt = V4L2_PIX_FMT_YUV420;

	ret = ov5640_video_probe(icd, client);
	if (ret)
		kfree(ov5640);

	return ret;
}

static int ov5640_remove(struct i2c_client *client)
{
	struct ov5640 *ov5640 = to_ov5640(client);

	kfree(ov5640);
	return 0;
}

static const struct i2c_device_id ov5640_idtable[] = {
	{"ov5640", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ov5640_idtable);

static struct i2c_driver ov5640_driver = {
	.driver = {
		   .name = "ov5640",
		   },
	.probe = ov5640_probe,
	.remove = ov5640_remove,
	.id_table = ov5640_idtable,
};

static int __init ov5640_mod_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&ov5640_driver);
	return ret;
}

static void __exit ov5640_mod_exit(void)
{
	i2c_del_driver(&ov5640_driver);
}

module_init(ov5640_mod_init);
module_exit(ov5640_mod_exit);
