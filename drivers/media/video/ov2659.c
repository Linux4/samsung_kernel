/*
 * ov2659 Camera Driver
 *
 * Copyright (c) 2010 Marvell Ltd.
 * Angela Wan <jwan@marvell.com>
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

#include "ov2659.h"

MODULE_DESCRIPTION("OmniVision OV2659 Camera Driver");
MODULE_LICENSE("GPL");

#define REG_PIDH    0x300a
#define REG_PIDL    0x300b

static const struct ov2659_datafmt ov2659_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
};

static struct ov2659_win_size {
	int width;
	int height;
} ov2659_win_sizes[] = {
	/* QCIF */
	{
		.width = 176,
		.height = 144,
	},
	/* 240*160 */
	{
		.width = 240,
		.height = 160,
	},
	/* QVGA */
	{
		.width = 320,
		.height = 240,
	},
	/* CIF */
	{
		.width = 352,
		.height = 288,
	},
	/* VGA */
	{
		.width = 640,
		.height = 480,
	},
	/* 480P */
	{
		.width = 720,
		.height = 480,
	},
	/* SVGA */
	{
		.width = 800,
		.height = 600,
	},
	/* UXGA */
	{
		.width = 1600,
		.height = 1200,
	},
};

/* Find a data format by a pixel code in an array */
static const struct ov2659_datafmt *ov2659_find_datafmt(
	enum v4l2_mbus_pixelcode code, const struct ov2659_datafmt *fmt,
	int n)
{
	int i;
	for (i = 0; i < n; i++)
		if (fmt[i].code == code)
			return fmt + i;

	return NULL;
}

static struct ov2659 *to_ov2659(const struct i2c_client
					     *client)
{
	return container_of(i2c_get_clientdata(client),
			    struct ov2659, subdev);
}

int ov2659_read(struct i2c_client *i2c, u16 reg, unsigned char *value)
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

int ov2659_write(struct i2c_client *c, u16 reg, unsigned char value)
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
static int ov2659_write_array(struct i2c_client *c, struct regval_list * vals)
{
	int i = 0;
	int ret = 0;
	while (vals->reg_num != OV2659_END_ADDR
	       || vals->value != OV2659_END_VAL) {
		ret = ov2659_write(c, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
		i++;
	}
	return ret;
}

static int ov2659_detect(struct i2c_client *client)
{
	unsigned char v = 0;
	int ret = 0;

	ret = ov2659_read(client, REG_PIDH, &v);

	if (ret < 0)
		return ret;
	if (v != 0x26)
		return -ENODEV;

	ret = ov2659_read(client, REG_PIDL, &v);
	if (ret < 0)
		return ret;
	if (v != 0x56)
		return -ENODEV;
	dev_err(&client->dev, "ov2659 detected 0x%x\n", v);
	return 0;
}

static int ov2659_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret;

	switch (ctrl->id) {
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int ov2659_g_chip_ident(struct v4l2_subdev *sd,
				   struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2659 *ov2659 = to_ov2659(client);

	id->ident = ov2659->model;
	id->revision = 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov2659_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return ov2659_read(client, (u16) reg->reg,
			       (unsigned char *)&(reg->val));
}

static int ov2659_s_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return ov2659_write(client, (u16) reg->reg,
				(unsigned char)reg->val);
}
#endif

static int set_stream(struct i2c_client *client, int enable)
{
	return 0;
}

static int ov2659_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = set_stream(client, enable);
	if (ret < 0)
		dev_err(&client->dev, "ov2659 set stream error\n");
	return ret;
}

static int ov2659_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(ov2659_colour_fmts))
		return -EINVAL;
	*code = ov2659_colour_fmts[index].code;
	return 0;
}

static int ov2659_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct ov2659_datafmt *fmt;

	fmt = ov2659_find_datafmt(mf->code, ov2659_colour_fmts,
				   ARRAY_SIZE(ov2659_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "ov2659 unsupported color format!\n");
		return -EINVAL;
	}

	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_JPEG;

	return 0;
}

static int ov2659_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct ov2659_datafmt *fmt;
	struct regval_list *pregs = NULL;

	fmt = ov2659_find_datafmt(mf->code, ov2659_colour_fmts,
				   ARRAY_SIZE(ov2659_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "ov2659 unsupported color format!\n");
		return -EINVAL;
	}

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
		switch (mf->width) {
		case 176:
			pregs = yuv_QCIF_tab;
			dev_info(&client->dev, "choose QCIF setting!\n");
			break;
		case 240:
			pregs = yuv_240_160_tab;
			dev_info(&client->dev, "choose 240*160 setting!\n");
			break;
		case 320:
			pregs = yuv_QVGA_tab;
			dev_info(&client->dev, "choose 320*240 setting!\n");
			break;
		case 352:
			pregs = yuv_CIF_tab;
			dev_info(&client->dev, "choose CIF setting!\n");
			break;
		case 640:
			pregs = yuv_VGA_tab;
			dev_info(&client->dev, "choose VGA setting!\n");
			break;
		case 720:
			pregs = yuv_480P_tab;
			dev_info(&client->dev, "choose 720*480 setting!\n");
			break;
		case 800:
			pregs = yuv_SVGA_tab;
			dev_info(&client->dev, "choose SVGA setting!\n");
			break;
		case 1600:
			pregs = yuv_UXGA_tab;
			dev_info(&client->dev, "choose UXGA setting!\n");
			break;
		default:
			dev_err(&client->dev, "unsupported YUV size"
					"%s %d!\n", __func__, __LINE__);
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

	ret = ov2659_write_array(client, init_global_tab);
	if (pregs)
		ret = ov2659_write_array(client, pregs);
out:
	return ret;
}

static int ov2659_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2659 *ov2659 = to_ov2659(client);

	mf->width		= ov2659->rect.width;
	mf->height		= ov2659->rect.height;
	mf->code		= V4L2_MBUS_FMT_UYVY8_2X8;
	mf->field		= V4L2_FIELD_NONE;
	mf->colorspace		= V4L2_COLORSPACE_JPEG;
	return 0;
}

static int ov2659_enum_fsizes(struct v4l2_subdev *sd,
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
		if (fsize->index >= ARRAY_SIZE(ov2659_win_sizes))
			return -EINVAL;

		fsize->discrete.height = ov2659_win_sizes[fsize->index].height;
		fsize->discrete.width = ov2659_win_sizes[fsize->index].width;
		break;
	default:
		dev_err(&client->dev, "ov2659 unsupported format!\n");
		return -EINVAL;
	}
	return 0;
}

static int ov2659_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;

	switch (ctrl->id) {
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int ov2659_init(struct v4l2_subdev *sd, u32 plat)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	ov2659_write(client, 0x0103, 0x01);
	mdelay(5);
	/* ret = ov2659_write_array(client, init_global_tab); */
	return ret;
}

/* Request bus settings on camera side */
static int ov2659_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);

	cfg->flags = V4L2_MBUS_PCLK_SAMPLE_RISING |
		V4L2_MBUS_VSYNC_ACTIVE_HIGH | V4L2_MBUS_HSYNC_ACTIVE_HIGH;
	cfg->type = V4L2_MBUS_PARALLEL;
	cfg->flags = soc_camera_apply_board_flags(icl, cfg);

	return 0;
}

static struct v4l2_subdev_core_ops ov2659_subdev_core_ops = {
	.g_ctrl	= ov2659_g_ctrl,
	.s_ctrl	= ov2659_s_ctrl,
	.g_chip_ident = ov2659_g_chip_ident,
	.init = ov2659_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov2659_g_register,
	.s_register = ov2659_s_register,
#endif
};

static struct v4l2_subdev_video_ops ov2659_subdev_video_ops = {
	.s_stream = ov2659_s_stream,
	.g_mbus_fmt = ov2659_g_fmt,
	.s_mbus_fmt = ov2659_s_fmt,
	.try_mbus_fmt = ov2659_try_fmt,
	.enum_mbus_fsizes = ov2659_enum_fsizes,
	.enum_mbus_fmt = ov2659_enum_fmt,
	.g_mbus_config	= ov2659_g_mbus_config,
};

static struct v4l2_subdev_ops ov2659_subdev_ops = {
	.core = &ov2659_subdev_core_ops,
	.video = &ov2659_subdev_video_ops,
};

static int ov2659_video_probe(struct soc_camera_device *icd,
				   struct i2c_client *client)
{
	struct ov2659 *ov2659 = to_ov2659(client);
	int ret = 0;

	ret = ov2659_detect(client);

	if (ret)
		goto out;
	dev_err(&client->dev, "OmniVision ov2659 sensor detected\n");

	ov2659->model = V4L2_IDENT_OV2659;

out:
	return ret;

}

static int ov2659_probe(struct i2c_client *client,
			    const struct i2c_device_id *did)
{
	struct ov2659 *ov2659;
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

	ov2659 = kzalloc(sizeof(struct ov2659), GFP_KERNEL);
	if (!ov2659)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&ov2659->subdev, client, &ov2659_subdev_ops);
	ov2659->rect.left = 0;
	ov2659->rect.top = 0;
	ov2659->rect.width = 176;
	ov2659->rect.height = 144;
	ov2659->pixfmt = V4L2_PIX_FMT_YUV420;

	ret = ov2659_video_probe(icd, client);
	if (ret)
		kfree(ov2659);

	return ret;
}

static int ov2659_remove(struct i2c_client *client)
{
	struct ov2659 *ov2659 = to_ov2659(client);

	kfree(ov2659);
	return 0;
}

static const struct i2c_device_id ov2659_idtable[] = {
	{"ov2659", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ov2659_idtable);

static struct i2c_driver ov2659_driver = {
	.driver = {
		   .name = "ov2659",
		   },
	.probe = ov2659_probe,
	.remove = ov2659_remove,
	.id_table = ov2659_idtable,
};

static int __init ov2659_mod_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&ov2659_driver);
	return ret;
}

static void __exit ov2659_mod_exit(void)
{
	i2c_del_driver(&ov2659_driver);
}

module_init(ov2659_mod_init);
module_exit(ov2659_mod_exit);
