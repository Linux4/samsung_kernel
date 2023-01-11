/*
 * db8221a Camera Driver
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
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>

#include "db8221a.h"
#include "db8221a_regs.h"

MODULE_DESCRIPTION("DB8221A Camera Driver");
MODULE_LICENSE("GPL");

extern void soc_camera_set_ddr_qos(s32 value);

//#define ENABLE_WQ_I2C_DB8221A (1)

#ifdef ENABLE_WQ_I2C_DB8221A
static DEFINE_MUTEX(i2c_lock_db8221a);
#endif

//Tunning Binary define
//#define CONFIG_LOAD_FILE

static int debug;
module_param(debug, int, 0644);

extern struct class *camera_class;
struct device *cam_dev_front;

#ifdef CONFIG_LOAD_FILE
static int db8221a_regs_table_write(struct i2c_client *c, char *name);
static int db8221a_regs_table_init(void);
#endif

static const struct db8221a_datafmt db8221a_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_VYUY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YVYU8_2X8, V4L2_COLORSPACE_JPEG},
};

static const struct v4l2_queryctrl db8221a_controls[] = {
	{
		.id = V4L2_CID_BRIGHTNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "brightness",
	}
};

static int video_mode;
static struct db8221a_regval *regs_resolution;
static char *regs_resolution_name;
static struct db8221a_regval *regs_display_setting;

static const struct v4l2_ctrl_ops db8221a_ctrl_ops;
static struct v4l2_ctrl_config db8221a_ctrl_video_mode_cfg = {
	.ops = &db8221a_ctrl_ops,
	.id = V4L2_CID_PRIVATE_DB8221A_VIDEO_MODE,
	.name = "video mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = VIDEO_TO_NORMAL,
	.max = VIDEO_TO_CALL,
	.step = 1,
	.def = VIDEO_TO_NORMAL,
};

static struct db8221a_regval *regs_res_all[] = {
	[DB8221A_FMT_VGA] = db8221a_preview_640_480,
/*	[DB8221A_FMT_800X600] = db8221a_preview_800_600,*/
	[DB8221A_FMT_2M] = db8221a_Capture_1600_1200,
};

static struct db8221a *to_db8221a(const struct i2c_client
						 *client)
{
	return container_of(i2c_get_clientdata(client),
				struct db8221a, subdev);
}


extern int camera_antibanding_get (void);

static int db8221a_copy_files_for_60hz(void){

#define COPY_FROM_60HZ_TABLE(TABLE_NAME, ANTI_BANDING_SETTING) \
	memcpy (TABLE_NAME, TABLE_NAME##_##ANTI_BANDING_SETTING, \
			sizeof(TABLE_NAME))

	printk("%s: Enter \n",__func__);

	COPY_FROM_60HZ_TABLE (db8221a_anti_banding_flicker, 60hz);
	
	printk("%s: copy done!\n", __func__);
	return 0;
}

static int db8221a_check_table_size_for_60hz(void){
#define IS_SAME_NUM_OF_ROWS(TABLE_NAME) \
	(sizeof(TABLE_NAME) == sizeof(TABLE_NAME##_60hz))

	if ( !IS_SAME_NUM_OF_ROWS(db8221a_anti_banding_flicker) ) return (-1);

	printk("%s: Success !\n", __func__);
	return 0;
}

#if 1
static int db8221a_i2c_read(struct i2c_client *client, u8 reg, u8 *val)
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

static int db8221a_i2c_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct i2c_msg msg;
	struct db8221a_regval buf;

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
#endif

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int db8221a_write(struct i2c_client *c, u8 addr, u8 val)
{
#ifdef ENABLE_WQ_I2C_DB8221A
	int ret;
	mutex_lock(&i2c_lock_db8221a);
	ret = db8221a_i2c_write(c, addr, val);
	mutex_unlock(&i2c_lock_db8221a);
	return ret;
#else
	return db8221a_i2c_write(c, addr, val);
#endif
}
#endif

/* The command register read, assumes Command_Rd_addH = 0x7000. */
static int db8221a_read(struct i2c_client *client, u8 addr, u8 *val)
{
#ifdef ENABLE_WQ_I2C_DB8221A
	int ret;
	mutex_lock(&i2c_lock_db8221a);
	ret = db8221a_i2c_read(client, addr, val);
	mutex_unlock(&i2c_lock_db8221a);
	return ret;
#else
	return db8221a_i2c_read(client, addr, val);
#endif
}

#ifdef ENABLE_WQ_I2C_DB8221A

static struct workqueue_struct *db8221a_wq;

typedef struct {  
	struct work_struct db8221a_work;  
	struct v4l2_subdev *sd;
	const struct db8221a_regval *msg; 
	char *name;
}db8221a_work_t;

db8221a_work_t *db8221a_work;

static void db8221a_wq_function( struct work_struct *work)
{ 
  	db8221a_work_t *db8221a_work = (db8221a_work_t *)work;  

	struct i2c_client *client = v4l2_get_subdevdata(db8221a_work->sd);
	int ret = 0;

	while (1) {
		if(db8221a_work->msg->addr == DB8221A_TERM && db8221a_work->msg->val == 0)
			break;
		else if(db8221a_work->msg->addr == DB8221A_TERM && db8221a_work->msg->val != 0)		
		{
			msleep(db8221a_work->msg->val);
			db8221a_work->msg++;
			continue;
		}
		
		ret = db8221a_i2c_write(client, db8221a_work->msg->addr, db8221a_work->msg->val);
		if (ret < 0)
			break;
		/* Assume that msg->addr is always less than 0xfffc */
		db8221a_work->msg++;
	}

	mutex_unlock(&i2c_lock_db8221a);
  	return;
}

#endif

static int db8221a_write_raw_array(struct v4l2_subdev *sd,
				  const struct db8221a_regval *msg, char *name)
{


#ifdef ENABLE_WQ_I2C_DB8221A	

	mutex_lock(&i2c_lock_db8221a);

	printk("=========%s : %s wq call ============ \n ",__func__,name);
	db8221a_work->sd = sd;
	db8221a_work->msg = msg; 
	db8221a_work->name = name;
	queue_work( db8221a_wq, (struct work_struct *)db8221a_work );
	return 0;

#else
	int ret = 0;

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk("=========%s : %s ============ \n ",__func__,name);

#ifdef CONFIG_LOAD_FILE
	ret = db8221a_regs_table_write(client, name);
#else
	
	while (1) {
		if(msg->addr == DB8221A_TERM && msg->val == 0)
			break;
		else if(msg->addr == DB8221A_TERM && msg->val != 0)		
		{
			msleep(msg->val);
			msg++;
			continue;
		}
		
		ret = db8221a_i2c_write(client, msg->addr, msg->val);
		if (ret < 0)
			break;
		/* Assume that msg->addr is always less than 0xfffc */
		msg++;
	}
#endif
	return ret;

#endif
	
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int db8221a_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return db8221a_read(client, (u8) reg->reg,
				   (u8 *)&(reg->val));
}

static int db8221a_s_register(struct v4l2_subdev *sd,
				 const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return db8221a_write(client, (u8) reg->reg,
				(u8)reg->val);
}
#endif

static int db8221a_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
struct i2c_client *client = v4l2_get_subdevdata(sd);
struct db8221a *db8221a = to_db8221a(client);
	if (enable)
//		ret = db8221a_write_raw_array(sd, regs_start_stream, "regs_start_stream");
		printk("=========start_stream===========\n");
	else {
		if(db8221a->rect.width == 1600) {
			soc_camera_set_ddr_qos(PM_QOS_DEFAULT_VALUE);	
		}	
		ret = db8221a_write_raw_array(sd, db8221a_stream_stop, "db8221a_stream_stop");
	}
	return ret;
}

static int db8221a_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(db8221a_colour_fmts))
		return -EINVAL;
	*code = db8221a_colour_fmts[index].code;
	return 0;
}

static inline enum db8221a_res_support find_res_code(int width, int height)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(db8221a_resolutions); i++) {
		if (db8221a_resolutions[i].width == width &&
			db8221a_resolutions[i].height == height)
			return db8221a_resolutions[i].res_code;
	}
	return -EINVAL;
}

static struct db8221a_regval *db8221a_get_res_regs(int width, int height)
{
	enum db8221a_res_support res_code;

	res_code = find_res_code(width, height);
	if (res_code == -EINVAL)
		return NULL;
	return regs_res_all[res_code];
}

static int db8221a_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct db8221a *db8221a = to_db8221a(client);
	if(mf->width == 1600 ) {
		soc_camera_set_ddr_qos(312000);
	}
	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
		/* for db8221a, only in normal preview status, it need to get res.*/
		if (video_mode == NORMAL_STATUS) {
			regs_resolution = db8221a_get_res_regs(mf->width, mf->height);
			if (!regs_resolution) {
				/* set default resolution */
				dev_warn(&client->dev,
					"db8221a: res not support, set to VGA\n");
				mf->width = DB8221A_OUT_WIDTH_DEF;
				mf->height = DB8221A_OUT_HEIGHT_DEF;
				regs_resolution =
					db8221a_get_res_regs(mf->width, mf->height);
			}
		}
		db8221a->rect.width = mf->width;
		db8221a->rect.height = mf->height;
		/* colorspace should be set in host driver */
		mf->colorspace = V4L2_COLORSPACE_JPEG;
/*
		if (mf->code == V4L2_MBUS_FMT_UYVY8_2X8)
			regs_display_setting = regs_display_uyvy_setting;
		else if (mf->code == V4L2_MBUS_FMT_VYUY8_2X8)
			regs_display_setting = regs_display_vyuy_setting;
		else if (mf->code == V4L2_MBUS_FMT_YUYV8_2X8)
			regs_display_setting = regs_display_yuyv_setting;
		else
			regs_display_setting = regs_display_yvyu_setting;
*/
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
static int db8221a_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	int ret = 0;
#ifdef CONFIG_LOAD_FILE
	switch (video_mode) {
	case VIDEO_TO_NORMAL:
		regs_resolution_name = "db8221a_preview_640_480";
		break;
	case NORMAL_TO_VIDEO:
		regs_resolution_name = "db8221a_24fps_Camcoder";
		break;
	case VIDEO_TO_CALL:
		regs_resolution_name = "db8221a_preview_704_576";
		break;
	default:
		if (mf->width == 640){
			db8221a_write_raw_array(sd, db8221a_Init_Reg, "db8221a_Init_Reg");
			regs_resolution_name = "db8221a_preview_640_480";}
		else
			regs_resolution_name = "db8221a_Capture_1600_1200";
		break;
	}
#else
	switch (video_mode) {
	case VIDEO_TO_NORMAL:
		regs_resolution_name = "db8221a_preview_640_480";
		break;
	case NORMAL_TO_VIDEO:
		regs_resolution_name = "db8221a_24fps_Camcoder";
		break;
	case VIDEO_TO_CALL:
		regs_resolution_name = "db8221a_preview_704_576";
		break;
	default:
		if (mf->width == 640){
			regs_resolution_name = "db8221a_preview_640_480";}
		else
			regs_resolution_name = "db8221a_Capture_1600_1200";
		break;
	}

#endif
	/*
	 * for db8221a sensor output uyvy, if change, pls open this
	 * ret |= db8221a_write_raw_array(sd, regs_display_setting);
	 */
        /** When Return to Preview from recording, it should be written th Init value  
            DongBu Hitek request to Samsung : dhee79.lee@samsung.com                  **/ 
	if(video_mode == VIDEO_TO_NORMAL){
		printk("[DHL] VIDEO_TO_NORMAL..!!\n");
		db8221a_write_raw_array(sd, db8221a_Init_Reg, "db8221a_Init_Reg");
	}
	if(video_mode != NORMAL_TO_VIDEO)
	{
		db8221a_write_raw_array(sd, db8221a_anti_banding_flicker, "db8221a_anti_banding_flicker");
	}

	ret = db8221a_write_raw_array(sd, regs_resolution, regs_resolution_name);

	if(video_mode == NORMAL_TO_VIDEO)
	{
		db8221a_write_raw_array(sd, db8221a_anti_banding_flicker, "db8221a_anti_banding_flicker");
	}

	/* restore video_mode to normal preview status */
	video_mode = NORMAL_STATUS;
	return ret;
}

static int db8221a_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct db8221a *db8221a = to_db8221a(client);

	mf->width		= db8221a->rect.width;
	mf->height		= db8221a->rect.height;
	/* all belows are fixed */
	mf->code		= V4L2_MBUS_FMT_UYVY8_2X8;
	mf->field		= V4L2_FIELD_NONE;
	mf->colorspace		= V4L2_COLORSPACE_JPEG;
	return 0;
}

static int db8221a_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *crop)
{
	return 0;
}

static int db8221a_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *crop)
{
	return 0;
}

static int db8221a_enum_fsizes(struct v4l2_subdev *sd,
				struct v4l2_frmsizeenum *fsize)
{
	if (!fsize)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	if (fsize->index >= ARRAY_SIZE(db8221a_resolutions))
		return -EINVAL;

	fsize->discrete.height = db8221a_resolutions[fsize->index].height;
	fsize->discrete.width = db8221a_resolutions[fsize->index].width;
	return 0;
}

#if 0 //Fix warning: due to CL#: 277589
static int db8221a_set_brightness(struct i2c_client *client, int value)
{
	return 0;
}
#endif

static int db8221a_set_video_mode(int value)
{
	switch (value) {
	case VIDEO_TO_NORMAL:
		regs_resolution = db8221a_preview_640_480;
		break;
	case NORMAL_TO_VIDEO:
		regs_resolution = db8221a_24fps_Camcoder;
		break;
	case VIDEO_TO_CALL:
		regs_resolution = db8221a_VT_Init_Reg;
		break;
	default:
		value = NORMAL_STATUS;
		break;
	}

	video_mode = value;
	return 0;
}

static int db8221a_init(struct v4l2_subdev *sd, u32 plat)
{
	int ret;
#ifdef CONFIG_LOAD_FILE
	ret= db8221a_regs_table_init();
	if (ret > 0){		
		printk("***** db8221a_regs_table_init  FAILED. Check the Filie in MMC\n");
		return ret;
	}
	ret =0;
#endif
	if(camera_antibanding_get() == 3) { /* CAM_BANDFILTER_60HZ  = 3 */
		printk("*****%s : Antibanding value is 3.\n", __func__);

		ret = db8221a_check_table_size_for_60hz();
		if(ret != 0) {
			printk(KERN_ERR "%s: Fail - the table num is %d \n", __func__, ret);
		}
		db8221a_copy_files_for_60hz();
	}
	ret = db8221a_write_raw_array(sd, db8221a_Init_Reg, "db8221a_Init_Reg");
	return ret;
}

static int db8221a_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE;
	return 0;
}

static int db8221a_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_DB8221A_VIDEO_MODE:
		ret = db8221a_set_video_mode(ctrl->val);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
static const struct v4l2_ctrl_ops db8221a_ctrl_ops = {
	.s_ctrl = db8221a_s_ctrl,
};

static struct v4l2_subdev_core_ops db8221a_subdev_core_ops = {
	.init		= db8221a_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= db8221a_g_register,
	.s_register	= db8221a_s_register,
#endif
};

static struct v4l2_subdev_video_ops db8221a_subdev_video_ops = {
	.s_stream = db8221a_s_stream,
	.g_mbus_fmt = db8221a_g_fmt,
	.s_mbus_fmt = db8221a_s_fmt,
	.try_mbus_fmt = db8221a_try_fmt,
	.enum_framesizes = db8221a_enum_fsizes,
	.enum_mbus_fmt = db8221a_enum_fmt,
	.g_crop = db8221a_g_crop,
	.s_crop = db8221a_s_crop,
	.g_mbus_config = db8221a_g_mbus_config,
};

static struct v4l2_subdev_ops db8221a_subdev_ops = {
	.core = &db8221a_subdev_core_ops,
	.video = &db8221a_subdev_video_ops,
};

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

static char *db8221a_regs_table = NULL;

static int db8221a_regs_table_size;

static int db8221a_regs_table_init(void)
{
	struct file *filp;

	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	printk("***** %s %d\n", __func__, __LINE__);

	set_fs(get_ds());

	filp = filp_open("/data/media/0/db8221a_regs.h", O_RDONLY, 0);
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

	db8221a_regs_table = dp;
	db8221a_regs_table_size = l;
	*((db8221a_regs_table + db8221a_regs_table_size) - 1) = '\0';

	printk("*****Compeleted %s %d\n", __func__, __LINE__);
	return 0;
}

void db8221a_regs_table_exit(void)
{
	/* release allocated memory when exit preview */
	if (db8221a_regs_table) {
		kfree(db8221a_regs_table);
		db8221a_regs_table = NULL;
		db8221a_regs_table_size = 0;
	}
	else
		printk("*****db8221a_regs_table is already null\n");
	
	printk("*****%s done\n", __func__);

}

static int db8221a_regs_table_write(struct i2c_client *c, char *name)
{
	char *start, *end, *reg;//, *data;	
	unsigned short addr, value;
	char reg_buf[7], data_buf[7];

	value = 0;

	printk("*****%s entered.\n", __func__);

	*(data_buf + 6) = '\0';

	start = strstr(db8221a_regs_table, name);

	end = strstr(start, "};");

	while (1) {	
		/* Find Address */	
		reg = strstr(start,"{0x");		
		if (reg)
			start = (reg + 7);
		if ((reg == NULL) || (reg > end))
			break;
		/* Write Value to Address */
			memcpy(reg_buf, (reg + 1), 6);
			memcpy(data_buf, (reg + 7), 6);
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16);
			value = (unsigned short)simple_strtoul(data_buf, NULL, 16);
//			printk(KERN_INFO "addr 0x%02x, value 0x%02x\n", addr, value);

#if 1	
			{
				if(addr == DB8221A_TERM)
				{
					if(!value){
						msleep(value);
						continue;
					}else{ 
						break;
					}
				} else {
					if(db8221a_write(c, addr, value) < 0 )
					{
						printk("<=PCAM=> %s fail on sensor_write\n", __func__);
				}
				}
			}
#endif
	}
	printk(KERN_ERR "***** Writing [%s] Ended\n",name);

	return 0;
}

#endif  /* CONFIG_LOAD_FILE */


static int db8221a_detect(struct i2c_client *client)
{
	int ret;
	u8 val=0xFF;

	/* revision */
	ret = db8221a_read(client, 0x00, &val);
	if (ret)
		return ret;
	printk("===============%s : %x=============\n",__func__,val);
	if (val != 0x50){
			printk("===============%s Fail!!!: %x=============\n",__func__,val);
		return -ENODEV;
		}
	return 0;
}

static ssize_t front_camera_fw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char front_cam_fw_ver[25] = "DB8221A N\n";
	return sprintf(buf, "%s", front_cam_fw_ver);
}

static ssize_t front_camera_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char cam_type[] = "DB_DB8221A\n";
	return sprintf(buf, "%s", cam_type);
}

static ssize_t front_camera_flash_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char cam_flash[] = "DB8221A_NONE\n";

	return sprintf(buf, "%s", cam_flash);
}

char front_cam_fw_full_ver[40] = "DB8221A N N\n";

ssize_t front_camera_full(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_type: %s\n", front_cam_fw_full_ver);
	return sprintf(buf, "%s\n", front_cam_fw_full_ver);
}

char *front_camera_info_val = "ISP=SOC;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;";
ssize_t front_camera_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("front_camera_info_val : %s\n", front_camera_info_val);

	return sprintf(buf, "%s\n", front_camera_info_val);
}

static DEVICE_ATTR(front_camtype, S_IRUGO | S_IXOTH, front_camera_type_show, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO | S_IXOTH, front_camera_fw_show, NULL);
static DEVICE_ATTR(front_flash, S_IRUGO | S_IXOTH, front_camera_flash_show, NULL);
static DEVICE_ATTR(front_camfw_full, S_IRUGO | S_IXOTH,  front_camera_full, NULL);
static DEVICE_ATTR(front_caminfo, S_IRUGO | S_IXOTH,  front_camera_info, NULL);

static int db8221a_probe(struct i2c_client *client,
				const struct i2c_device_id *did)
{
	struct db8221a *db8221a;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	static int sysfs_init = 0;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	ret = db8221a_detect(client);
	if (ret)
		return ret;
	dev_info(&adapter->dev, "cam: DB8221A detect!\n");

	db8221a = kzalloc(sizeof(struct db8221a), GFP_KERNEL);
	if (!db8221a)
		return -ENOMEM;

#ifdef ENABLE_WQ_I2C_DB8221A
	db8221a_wq = create_workqueue("db8221a_queue"); 
 	if (db8221a_wq) {    
		db8221a_work = (db8221a_work_t *)kmalloc(sizeof(db8221a_work_t), GFP_KERNEL);    
		if (db8221a_work) {     
		 	INIT_WORK( (struct work_struct *)db8221a_work, db8221a_wq_function );      
 		}    
	} 
#endif
	v4l2_i2c_subdev_init(&db8221a->subdev, client, &db8221a_subdev_ops);
	v4l2_ctrl_handler_init(&db8221a->hdl, 1);
	v4l2_ctrl_new_custom(&db8221a->hdl, &db8221a_ctrl_video_mode_cfg, NULL);
	db8221a->subdev.ctrl_handler = &db8221a->hdl;
	if (db8221a->hdl.error)
		return db8221a->hdl.error;

	db8221a->rect.left = 0;
	db8221a->rect.top = 0;
	db8221a->rect.width = DB8221A_OUT_WIDTH_DEF;
	db8221a->rect.height = DB8221A_OUT_HEIGHT_DEF;
	db8221a->pixfmt = V4L2_PIX_FMT_UYVY;
	/*++ added for camera sysfs++*/
	if(sysfs_init == 0){
		sysfs_init = 1;
		if(camera_class == NULL)
			camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class))
			printk("Failed to create camera_class!\n");

		if(cam_dev_front == NULL)
			cam_dev_front = device_create(camera_class, NULL,0, "%s", "front");
		if (IS_ERR(cam_dev_front))
			printk("Failed to create deivce (cam_dev_front)\n");

		if (device_create_file(cam_dev_front, &dev_attr_front_camtype) < 0)
			printk("Failed to create device file: %s\n", dev_attr_front_camtype.attr.name);
		if (device_create_file(cam_dev_front, &dev_attr_front_camfw) < 0)
			printk("Failed to create device file: %s\n", dev_attr_front_camfw.attr.name);
		if (device_create_file(cam_dev_front, &dev_attr_front_flash) < 0)
			printk("Failed to create device file: %s\n", dev_attr_front_flash.attr.name);
		if (device_create_file(cam_dev_front, &dev_attr_front_camfw_full) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_front_camfw_full.attr.name);
		if (device_create_file(cam_dev_front, &dev_attr_front_caminfo) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_front_caminfo.attr.name);

		/*-- camera sysfs --*/
	}
	return ret;
}

static int db8221a_remove(struct i2c_client *client)
{
	struct db8221a *db8221a = to_db8221a(client);

	v4l2_ctrl_handler_free(&db8221a->hdl);
	kfree(db8221a);
#ifdef ENABLE_WQ_I2C_DB8221A
	kfree(db8221a_work);
#endif
	return 0;
}

static const struct i2c_device_id db8221a_idtable[] = {
	{"db8221a", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, db8221a_idtable);

static struct i2c_driver db8221a_driver = {
	.driver = {
		   .name = "db8221a",
		   },
	.probe = db8221a_probe,
	.remove = db8221a_remove,
	.id_table = db8221a_idtable,
};

static int __init db8221a_mod_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&db8221a_driver);
	return ret;
}

static void __exit db8221a_mod_exit(void)
{
	i2c_del_driver(&db8221a_driver);
}

module_init(db8221a_mod_init);
module_exit(db8221a_mod_exit);
