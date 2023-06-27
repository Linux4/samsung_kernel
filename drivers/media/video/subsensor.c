/*
 * A V4L2 driver for SYS.LSI S5K4ECGX cameras.
 * 
 * Copyright 2006 One Laptop Per Child Association, Inc.  Written
 * by Jonathan Corbet with substantial inspiration from Mark
 * McClelland's ovcamchip code.
 *
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 *jpeg
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>
#include <mach/camera.h>
#include <mach/samsung_camera.h>

MODULE_AUTHOR("Vincent Wan <zswan@marvell.com>");
MODULE_DESCRIPTION("A low-level driver for sub sensors");
MODULE_LICENSE("GPL");

/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define WQXGA_WIDTH		2560
#define WQXGA_HEIGHT	1920

#define Wide4M_WIDTH	2560
#define Wide4M_HEIGHT	1536

#define QXGA_WIDTH		2048
#define QXGA_HEIGHT	1536

#define Wide2_4M_WIDTH	2048
#define Wide2_4M_HEIGHT	1232

#define UXGA_WIDTH		1600
#define UXGA_HEIGHT     	1200

#define Wide1_5M_WIDTH	1600
#define Wide1_5M_HEIGHT	 960

#define SXGA_WIDTH		1280
#define SXGA_HEIGHT     	960
#define XGA_WIDTH		1024
#define XGA_HEIGHT     	768
#define D1_WIDTH		720
#define D1_HEIGHT      	480

#define Wide4K_WIDTH	800
#define Wide4K_HEIGHT	480

#define VGA_WIDTH		640
#define VGA_HEIGHT		480
#define QVGA_WIDTH		320
#define QVGA_HEIGHT	240
#define CIF_WIDTH		352
#define CIF_HEIGHT		288
#define QCIF_WIDTH		176
#define QCIF_HEIGHT		144
#define ROTATED_QCIF_WIDTH	144
#define ROTATED_QCIF_HEIGHT	176
#define THUMB_WIDTH	160
#define THUMB_HEIGHT	120

struct subsensor_info {
	struct v4l2_subdev subdev;

};

struct subsensor_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

static const struct subsensor_datafmt subsensor_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
};

static struct subsensor_win_size {
	int	width;
	int	height;
} subsensor_win_sizes[] = {
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
	},
		/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
	},
	/* 720*480 */
	{
		.width		= D1_WIDTH,
		.height		= D1_HEIGHT,
	},
};
static int subsensor_enum_fsizes(struct v4l2_subdev *sd,
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
		if (fsize->index >= ARRAY_SIZE(subsensor_win_sizes)) {
			dev_warn(&client->dev,
				"subsensor unsupported size %d!\n", fsize->index);
			return -EINVAL;
		}
		fsize->discrete.height = subsensor_win_sizes[fsize->index].height;
		fsize->discrete.width = subsensor_win_sizes[fsize->index].width;
		break;

	default:
		dev_err(&client->dev, "subsensor unsupported format!\n");
		return -EINVAL;
	}
	return 0;
}


static int subsensor_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(subsensor_colour_fmts))
		return -EINVAL;
	*code = subsensor_colour_fmts[index].code;
	return 0;
}

static struct v4l2_subdev_core_ops subsensor_core_ops = {
};
static struct v4l2_subdev_video_ops subsensor_video_ops = {
	.enum_mbus_fmt		= subsensor_enum_fmt,
	.enum_mbus_fsizes	= subsensor_enum_fsizes,
};

static struct v4l2_subdev_ops subsensor_subdev_ops = {
	.core			= &subsensor_core_ops,
	.video			= &subsensor_video_ops,
};

static int subsensor_video_probe(struct soc_camera_device *icd,
			      struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "=========Marvell sub sensor detected is ok!==========\n");
	return 0;
}

static int subsensor_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct soc_camera_device *icd	= client->dev.platform_data;
	struct subsensor_info *priv;
	int ret;

	printk("------------subsensor_probe--------------\n");

	if (!icd) {
		dev_err(&client->dev, "Missing soc-camera data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct subsensor_info), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &subsensor_subdev_ops);

	ret = subsensor_video_probe(icd, client);
	if (ret < 0) {
		kfree(priv);
	}
	
	printk("------------subsensor_probe---return --ret = %d---------\n", ret);
	return ret;
}

static int subsensor_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_device_id subsensor_idtable[] = {
	{ "subsensor", 1 },
	{ }
};

static struct i2c_driver subsensor_driver = {
	.driver = {
		.name	= "subsensor",
	},
	.id_table       = subsensor_idtable,	
	.probe		= subsensor_probe,
	.remove		= subsensor_remove,
};

/*
 * Module initialization
 */
static int __init subsensor_mod_init(void)
{
	int ret =0;
	Cam_Printk(KERN_NOTICE "Marvell sub sensor driver, at your service\n");
	ret = i2c_add_driver(&subsensor_driver);
	Cam_Printk(KERN_NOTICE "Marvell sub sensor :%d \n ",ret);
	return ret;
}

static void __exit subsensor_mod_exit(void)
{
	i2c_del_driver(&subsensor_driver);
}

module_init(subsensor_mod_init);
module_exit(subsensor_mod_exit);

