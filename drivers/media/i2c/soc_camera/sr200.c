/*
 * sr200 Camera Driver
 *
 * Copyright (c) 2013 Marvell Ltd.
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
#include <linux/platform_data/camera-mmp.h>
#include <linux/v4l2-mediabus.h>
#include "sr200.h"
#include "sr200pc20m_regs.h"

MODULE_DESCRIPTION("SR200 Camera Driver");
MODULE_LICENSE("GPL");

static int debug;
module_param(debug, int, 0644);

#ifdef CONFIG_LOAD_FILE
static int sr200_regs_table_write(struct i2c_client *c, char *name);
static int sr200_regs_table_init(void);
#endif
static const struct sr200_datafmt sr200_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_VYUY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YVYU8_2X8, V4L2_COLORSPACE_JPEG},
};

static int video_mode;
static int video_mode_previous;
static int width_previous, height_previous;
static struct sr200_regval *regs_resolution;
static struct sr200_regval *regs_display_setting;

static const struct v4l2_ctrl_ops sr200_ctrl_ops;
static struct v4l2_ctrl_config sr200_ctrl_video_mode_cfg = {
	.ops = &sr200_ctrl_ops,
	.id = V4L2_CID_PRIVATE_SR200_VIDEO_MODE,
	.name = "video mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = VIDEO_TO_NORMAL,
	.max = VIDEO_TO_CALL,
	.step = 1,
	.def = VIDEO_TO_NORMAL,
};

static struct sr200_regval *regs_res_all[] = {
	[SR200_FMT_QVGA] = regs_res_qvga_vt,
	[SR200_FMT_VGA] = regs_res_vga,
/*	[SR200_FMT_800X600] = regs_res_800x600,*/
	[SR200_FMT_2M] = regs_res_2m,
};

static struct sr200 *to_sr200(const struct i2c_client
						 *client)
{
	return container_of(i2c_get_clientdata(client),
				struct sr200, subdev);
}

extern int camera_antibanding_get (void);
static int sr130pc10_copy_files_for_60hz(void){
#define COPY_FROM_60HZ_TABLE(TABLE_NAME, ANTI_BANDING_SETTING) \
	memcpy (TABLE_NAME, TABLE_NAME##_##ANTI_BANDING_SETTING, \
			sizeof(TABLE_NAME))
	printk("%s: Enter \n",__func__);
	COPY_FROM_60HZ_TABLE (regs_start_setting, 60hz);
	COPY_FROM_60HZ_TABLE (regs_res_vga, 60hz);
	COPY_FROM_60HZ_TABLE (regs_res_vga_cam, 60hz);
	COPY_FROM_60HZ_TABLE (regs_res_vga_vt, 60hz);
	COPY_FROM_60HZ_TABLE (regs_res_auto_fps, 60hz);
	printk("%s: copy done!\n", __func__);
}
static int sr130pc10_check_table_size_for_60hz(void){
#define IS_SAME_NUM_OF_ROWS(TABLE_NAME) \
	(sizeof(TABLE_NAME) == sizeof(TABLE_NAME##_60hz))
	if ( !IS_SAME_NUM_OF_ROWS(regs_start_setting) ) return (-1);
	if ( !IS_SAME_NUM_OF_ROWS(regs_res_vga) ) return (-2);
	if ( !IS_SAME_NUM_OF_ROWS(regs_res_vga_cam) ) return (-3);
	if ( !IS_SAME_NUM_OF_ROWS(regs_res_vga_vt) ) return (-4);
	if ( !IS_SAME_NUM_OF_ROWS(regs_res_auto_fps) ) return (-5);
	printk("%s: Success !\n", __func__);
	return 0;
}
static int sr200_i2c_read(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret;
	struct i2c_msg msg[] = {
		{
			.addr   = client->addr,
			.flags  = 0,
			.len    = 1,
			.buf    = (u8 *)&reg,
		},
		{
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.len    = 1,
			.buf    = val,
		},
	};

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_err(&client->dev, "Failed reading register 0x%x!\n", reg);
		return ret;
	}
	v4l2_dbg(3, debug, client, "i2c_read: 0x%X : 0x%x\n", reg, *val);

	return 0;
}

static int sr200_i2c_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct i2c_msg msg;
	struct sr200_regval buf;

	buf.addr = reg;
	buf.val = val;

	msg.addr    = client->addr;
	msg.flags   = 0;
	msg.len     = 2;
	msg.buf     = (u8 *)&buf;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "Failed writing register 0x%04x!\n", reg);
		return ret;
	}
	v4l2_dbg(3, debug, client, "i2c_write: 0x%0X : 0x%0x\n", reg, val);

	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sr200_write(struct i2c_client *c, u8 addr, u8 val)
{
	return sr200_i2c_write(c, addr, val);
}
#endif

/* The command register read, assumes Command_Rd_addH = 0x7000. */
static int sr200_read(struct i2c_client *client, u8 addr, u8 *val)
{
	return sr200_i2c_read(client, addr, val);
}

static int sr200_write_raw_array(struct v4l2_subdev *sd,
				  const struct sr200_regval *msg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
#ifdef CONFIG_LOAD_FILE
	ret = sr200_regs_table_write(client, name);
#else

	while (msg->addr != SR200_TERM || msg->val != SR200_TERM) {
		if(msg->addr == SR200_TERM) {
			mdelay(10 * msg->val);
		} else {
		ret = sr200_i2c_write(client, msg->addr, msg->val);
		if (ret < 0)
			break;
		}
		/* Assume that msg->addr is always less than 0xfffc */
		msg++;
	}
#endif
	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sr200_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return sr200_read(client, (u8) reg->reg,
				   (u8 *)&(reg->val));
}

static int sr200_s_register(struct v4l2_subdev *sd,
				 const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return sr200_write(client, (u8) reg->reg,
				(u8)reg->val);
}
#endif

static int sr200_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = sr200_write_raw_array(sd, regs_start_stream);
	else
		ret = sr200_write_raw_array(sd, regs_stop_stream);
	return ret;
}

static int sr200_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(sr200_colour_fmts))
		return -EINVAL;
	*code = sr200_colour_fmts[index].code;
	return 0;
}

static inline enum sr200_res_support find_res_code(int width, int height)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(sr200_resolutions); i++) {
		if (sr200_resolutions[i].width == width &&
			sr200_resolutions[i].height == height)
			return sr200_resolutions[i].res_code;
	}
	return -EINVAL;
}

static struct sr200_regval *sr200_get_res_regs(int width, int height)
{
	enum sr200_res_support res_code;

	res_code = find_res_code(width, height);
	if (res_code == -EINVAL)
		return NULL;
	return regs_res_all[res_code];
}

static int sr200_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr200 *sr200 = to_sr200(client);

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
		/* for sr200, only in normal preview status, it need to get res.*/
		if (video_mode == NORMAL_STATUS) {
			regs_resolution = sr200_get_res_regs(mf->width, mf->height);
			if (!regs_resolution) {
				/* set default resolution */
				dev_warn(&client->dev,
					"sr200: res not support, set to VGA\n");
				mf->width = SR200_OUT_WIDTH_DEF;
				mf->height = SR200_OUT_HEIGHT_DEF;
				regs_resolution =
					sr200_get_res_regs(mf->width, mf->height);
			}
		}
		sr200->rect.width = mf->width;
		sr200->rect.height = mf->height;
		/* colorspace should be set in host driver */
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		if (mf->code == V4L2_MBUS_FMT_UYVY8_2X8)
			regs_display_setting = regs_display_uyvy_setting;
		else if (mf->code == V4L2_MBUS_FMT_VYUY8_2X8)
			regs_display_setting = regs_display_vyuy_setting;
		else if (mf->code == V4L2_MBUS_FMT_YUYV8_2X8)
			regs_display_setting = regs_display_yuyv_setting;
		else
			regs_display_setting = regs_display_yvyu_setting;
		break;
	default:
		/* This should never happen */
		mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
		return -EINVAL;
	}
	return 0;
}

/*
 * each time before start streaming,
 * this function must be called
 */
static int sr200_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr200 *sr200 = to_sr200(client);
#ifdef CONFIG_LOAD_FILE
	switch (video_mode) {
	case VIDEO_TO_NORMAL:
		regs_resolution_name = "regs_res_auto_fps";
		break;
	case NORMAL_TO_VIDEO:
		regs_resolution_name = "regs_res_vga_cam";
		break;
	case VIDEO_TO_CALL:
		regs_resolution_name = "regs_res_qvga_vt";
		break;
	default:
		if (mf->width == 640)
		regs_resolution_name = "regs_res_vga";
		else
		regs_resolution_name = "regs_res_2m";
		break;
	}
#endif
	/*
	 * for sr200 sensor output uyvy, if change, pls open this
	 * ret |= sr200_write_raw_array(sd, regs_display_setting);
	 */
	if(video_mode_previous != video_mode || width_previous != sr200->rect.width ||
			height_previous != sr200->rect.height) {
	ret = sr200_write_raw_array(sd, regs_resolution);
		video_mode_previous = video_mode;
		width_previous = sr200->rect.width;
		height_previous = sr200->rect.height;
	}
	/* restore video_mode to normal preview status */
	video_mode = NORMAL_STATUS;
	return ret;
}

static int sr200_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr200 *sr200 = to_sr200(client);

	mf->width		= sr200->rect.width;
	mf->height		= sr200->rect.height;
	/* all belows are fixed */
	mf->code		= V4L2_MBUS_FMT_UYVY8_2X8;
	mf->field		= V4L2_FIELD_NONE;
	mf->colorspace		= V4L2_COLORSPACE_JPEG;
	return 0;
}

static int sr200_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *crop)
{
	return 0;
}

static int sr200_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *crop)
{
	return 0;
}

static int sr200_enum_fsizes(struct v4l2_subdev *sd,
				struct v4l2_frmsizeenum *fsize)
{
	if (!fsize)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	if (fsize->index >= ARRAY_SIZE(sr200_resolutions))
		return -EINVAL;

	fsize->discrete.height = sr200_resolutions[fsize->index].height;
	fsize->discrete.width = sr200_resolutions[fsize->index].width;
	return 0;
}

static int sr200_set_video_mode(int value)
{
	switch (value) {
	case VIDEO_TO_NORMAL:
		regs_resolution = regs_res_auto_fps;
		break;
	case NORMAL_TO_VIDEO:
		regs_resolution = regs_res_vga_cam;
		break;
	case VIDEO_TO_CALL:
		regs_resolution = regs_res_qvga_vt;
		break;
	default:
		value = NORMAL_STATUS;
		break;
	}

	video_mode = value;
	return 0;
}

static int sr200_init(struct v4l2_subdev *sd, u32 plat)
{
	int ret;
#ifdef CONFIG_LOAD_FILE
	ret= sr200_regs_table_init();
	if (ret > 0){		
		printk("***** sr200_regs_table_init  FAILED. Check the Filie in MMC\n");
		return ret;
	}
	ret =0;
#endif
	if(camera_antibanding_get() == 3) { /* CAM_BANDFILTER_60HZ  = 3 */
		ret = sr130pc10_check_table_size_for_60hz();
		if(ret != 0) {
			printk(KERN_ERR "%s: Fail - the table num is %d \n", __func__, ret);
		}
		sr130pc10_copy_files_for_60hz();
	}
	ret = sr200_write_raw_array(sd, regs_start_setting);
	video_mode_previous = NORMAL_STATUS;
	return ret;
}

static int sr200_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE;
	return 0;
}

static int sr200_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_SR200_VIDEO_MODE:
		ret = sr200_set_video_mode(ctrl->val);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
static const struct v4l2_ctrl_ops sr200_ctrl_ops = {
	.s_ctrl = sr200_s_ctrl,
};

static struct v4l2_subdev_core_ops sr200_subdev_core_ops = {
	.init		= sr200_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= sr200_g_register,
	.s_register	= sr200_s_register,
#endif
};

static struct v4l2_subdev_video_ops sr200_subdev_video_ops = {
	.s_stream = sr200_s_stream,
	.g_mbus_fmt = sr200_g_fmt,
	.s_mbus_fmt = sr200_s_fmt,
	.try_mbus_fmt = sr200_try_fmt,
	.enum_framesizes = sr200_enum_fsizes,
	.enum_mbus_fmt = sr200_enum_fmt,
	.g_crop = sr200_g_crop,
	.s_crop = sr200_s_crop,
	.g_mbus_config = sr200_g_mbus_config,
};

static struct v4l2_subdev_ops sr200_subdev_ops = {
	.core = &sr200_subdev_core_ops,
	.video = &sr200_subdev_video_ops,
};
#ifdef CONFIG_LOAD_FILE
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
static char *sr200_regs_table = NULL;
static int sr200_regs_table_size;
static int sr200_regs_table_init(void)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();
	printk("***** %s %d\n", __func__, __LINE__);
	set_fs(get_ds());
	filp = filp_open("/data/media/0/sr200pc20m_regs.h", O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk("***** file open error\n");
		return 1;
	}
	else
		printk(KERN_ERR "***** File is opened \n");
	l = filp->f_path.dentry->d_inode->i_size;	
	printk("l = %ld\n", l);
	dp = kmalloc(l, GFP_KERNEL);
	if (dp == NULL) {
		printk("*****Out of Memory\n");
		filp_close(filp, current->files);
		return 1;
	}
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	if (ret != l) {
		printk("*****Failed to read file ret = %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return 1;
	}
	filp_close(filp, current->files);
	set_fs(fs);
	sr200_regs_table = dp;
	sr200_regs_table_size = l;
	*((sr200_regs_table + sr200_regs_table_size) - 1) = '\0';
	printk("*****Compeleted %s %d\n", __func__, __LINE__);
	return 0;
}
void sr200_regs_table_exit(void)
{
	if (sr200_regs_table) {
		kfree(sr200_regs_table);
		sr200_regs_table = NULL;
		sr200_regs_table_size = 0;
	}
	else
		printk("*****sr200_regs_table is already null\n");
	printk("*****%s done\n", __func__);
}
static int sr200_regs_table_write(struct i2c_client *c, char *name)
{
	char *start, *end, *reg;//, *data;	
	unsigned short addr, value;
	char reg_buf[7], data_buf[7];
	value = 0;
	printk("*****%s entered.\n", __func__);
	*(data_buf + 6) = '\0';
	start = strstr(sr200_regs_table, name);
	end = strstr(start, "};");
	while (1) {	
		reg = strstr(start,"{0x");		
		if (reg)
			start = (reg + 7);
		if ((reg == NULL) || (reg > end))
			break;
			memcpy(reg_buf, (reg + 1), 6);
			memcpy(data_buf, (reg + 7), 6);
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16);
			value = (unsigned short)simple_strtoul(data_buf, NULL, 16);
#if 1	
			{
				if(sr200_write(c, addr, value) < 0 )
				{
					printk("<=PCAM=> %s fail on sensor_write\n", __func__);
				}
			}
#endif
	}
	printk(KERN_ERR "***** Writing [%s] Ended\n",name);
	return 0;
}
#endif  /* CONFIG_LOAD_FILE */

static int sr200_detect(struct i2c_client *client)
{
	int ret;
	u8 val;

	/* revision */
	ret = sr200_read(client, 0x04, &val);
	if (ret)
		return ret;
	if (val != 0xB4)
		return -ENODEV;
	return 0;
}

static int sr200_probe(struct i2c_client *client,
				const struct i2c_device_id *did)
{
	struct sr200 *sr200;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	ret = sr200_detect(client);
	if (ret)
		return ret;
	dev_info(&adapter->dev, "cam: SR200 detect!\n");

	sr200 = kzalloc(sizeof(struct sr200), GFP_KERNEL);
	if (!sr200)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&sr200->subdev, client, &sr200_subdev_ops);
	v4l2_ctrl_handler_init(&sr200->hdl, 1);
	v4l2_ctrl_new_custom(&sr200->hdl, &sr200_ctrl_video_mode_cfg, NULL);
	sr200->subdev.ctrl_handler = &sr200->hdl;
	if (sr200->hdl.error)
		return sr200->hdl.error;

	sr200->rect.left = 0;
	sr200->rect.top = 0;
	sr200->rect.width = SR200_OUT_WIDTH_DEF;
	sr200->rect.height = SR200_OUT_HEIGHT_DEF;
	sr200->pixfmt = V4L2_PIX_FMT_UYVY;
	return ret;
}

static int sr200_remove(struct i2c_client *client)
{
	struct sr200 *sr200 = to_sr200(client);

	v4l2_ctrl_handler_free(&sr200->hdl);
	kfree(sr200);
	return 0;
}

static const struct i2c_device_id sr200_idtable[] = {
	{"sr200", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sr200_idtable);

static struct i2c_driver sr200_driver = {
	.driver = {
		   .name = "sr200",
		   },
	.probe = sr200_probe,
	.remove = sr200_remove,
	.id_table = sr200_idtable,
};

static int __init sr200_mod_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&sr200_driver);
	return ret;
}

static void __exit sr200_mod_exit(void)
{
	i2c_del_driver(&sr200_driver);
}

module_init(sr200_mod_init);
module_exit(sr200_mod_exit);
