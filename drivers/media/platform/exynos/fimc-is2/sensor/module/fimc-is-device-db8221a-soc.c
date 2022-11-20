/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera_ext.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is2-sensor.h>

#include "fimc-is-core.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-hw.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-db8221a-soc.h"
#include "fimc-is-device-db8221a-soc-reg.h"
#define SENSOR_NAME "db8221a_soc"
#define SENSOR_VER "DB8221A-M01F0"

static const struct sensor_regset_table db8221a_regset_table = {
	.init			= SENSOR_REGISTER_REGSET(db8221a_Init_Reg),
	.init_vt_640_480	= SENSOR_REGISTER_REGSET(db8221a_Init_VT_Reg_640_480),
	.init_vt_352_288	= SENSOR_REGISTER_REGSET(db8221a_Init_VT_Reg_352_288),
	.init_vt_176_144	= SENSOR_REGISTER_REGSET(db8221a_Init_VT_Reg_176_144),

	.effect_none		= SENSOR_REGISTER_REGSET(db8221a_Effect_Normal),
	.effect_negative	= SENSOR_REGISTER_REGSET(db8221a_Effect_Negative),
	.effect_gray		= SENSOR_REGISTER_REGSET(db8221a_Effect_Gray),
	.effect_sepia		= SENSOR_REGISTER_REGSET(db8221a_Effect_Sepia),
	.effect_aqua		= SENSOR_REGISTER_REGSET(db8221a_Effect_Aqua),

	.fps_auto		= SENSOR_REGISTER_REGSET(db8221a_fps_auto),
	.fps_7			= SENSOR_REGISTER_REGSET(db8221a_fps_7_fixed),
	.fps_15			= SENSOR_REGISTER_REGSET(db8221a_fps_15_fixed),

	.wb_auto		= SENSOR_REGISTER_REGSET(db8221a_wb_auto),
	.wb_daylight		= SENSOR_REGISTER_REGSET(db8221a_wb_daylight),
	.wb_cloudy		= SENSOR_REGISTER_REGSET(db8221a_wb_cloudy),
	.wb_fluorescent		= SENSOR_REGISTER_REGSET(db8221a_wb_fluorescent),
	.wb_incandescent	= SENSOR_REGISTER_REGSET(db8221a_wb_incandescent),

	.brightness_default	= SENSOR_REGISTER_REGSET(db8221a_brightness_default),
	.brightness_m4		= SENSOR_REGISTER_REGSET(db8221a_brightness_m4),
	.brightness_m3		= SENSOR_REGISTER_REGSET(db8221a_brightness_m3),
	.brightness_m2		= SENSOR_REGISTER_REGSET(db8221a_brightness_m2),
	.brightness_m1		= SENSOR_REGISTER_REGSET(db8221a_brightness_m1),
	.brightness_p4		= SENSOR_REGISTER_REGSET(db8221a_brightness_p4),
	.brightness_p3		= SENSOR_REGISTER_REGSET(db8221a_brightness_p3),
	.brightness_p2		= SENSOR_REGISTER_REGSET(db8221a_brightness_p2),
	.brightness_p1		= SENSOR_REGISTER_REGSET(db8221a_brightness_p1),

	.contrast_default	= SENSOR_REGISTER_REGSET(db8221a_contrast_default),
	.contrast_m4		= SENSOR_REGISTER_REGSET(db8221a_contrast_m4),
	.contrast_m3		= SENSOR_REGISTER_REGSET(db8221a_contrast_m3),
	.contrast_m2		= SENSOR_REGISTER_REGSET(db8221a_contrast_m2),
	.contrast_m1		= SENSOR_REGISTER_REGSET(db8221a_contrast_m1),
	.contrast_p4		= SENSOR_REGISTER_REGSET(db8221a_contrast_p4),
	.contrast_p3		= SENSOR_REGISTER_REGSET(db8221a_contrast_p3),
	.contrast_p2		= SENSOR_REGISTER_REGSET(db8221a_contrast_p2),
	.contrast_p1		= SENSOR_REGISTER_REGSET(db8221a_contrast_p1),

	.stop_stream		= SENSOR_REGISTER_REGSET(db8221a_stop_stream),
	.start_stream		= SENSOR_REGISTER_REGSET(db8221a_start_stream),

	.resol_176_144		= SENSOR_REGISTER_REGSET(db8221a_resol_176_144),
	.resol_352_288		= SENSOR_REGISTER_REGSET(db8221a_resol_352_288),
	.resol_320_240		= SENSOR_REGISTER_REGSET(db8221a_resol_320_240),
	.resol_640_480		= SENSOR_REGISTER_REGSET(db8221a_resol_640_480),
	.resol_704_576		= SENSOR_REGISTER_REGSET(db8221a_resol_704_576),
	.resol_800_600		= SENSOR_REGISTER_REGSET(db8221a_resol_800_600),
	.resol_1280_960		= SENSOR_REGISTER_REGSET(db8221a_resol_1280_960),
	.resol_1600_1200	= SENSOR_REGISTER_REGSET(db8221a_resol_1600_1200),
	.camcoder_640_480	= SENSOR_REGISTER_REGSET(db8221a_24fps_camcoder_640_480),
	.camcoder_352_288	= SENSOR_REGISTER_REGSET(db8221a_24fps_camcoder_352_288),
	.camcoder_176_144	= SENSOR_REGISTER_REGSET(db8221a_24fps_camcoder_176_144),

	.dtp_on			= SENSOR_REGISTER_REGSET(db8221a_DTP_On),
	.dtp_off		= SENSOR_REGISTER_REGSET(db8221a_DTP_Off),

	.hflip			= SENSOR_REGISTER_REGSET(db8221a_hflip),
	.vflip			= SENSOR_REGISTER_REGSET(db8221a_vflip),
	.flip_off		= SENSOR_REGISTER_REGSET(db8221a_flip_off),

	.anti_banding_flicker_50hz	= SENSOR_REGISTER_REGSET(db8221a_anti_banding_flicker_50hz),
	.anti_banding_flicker_60hz	= SENSOR_REGISTER_REGSET(db8221a_anti_banding_flicker_60hz),

	.metering_matrix	= SENSOR_REGISTER_REGSET(db8221a_metering_matrix),
	.metering_center	= SENSOR_REGISTER_REGSET(db8221a_metering_center),
	.metering_spot		= SENSOR_REGISTER_REGSET(db8221a_metering_spot),
};

static struct fimc_is_sensor_cfg settle_db8221a[] = {
	FIMC_IS_SENSOR_CFG(1600, 1200, 15, 12, 0),
	FIMC_IS_SENSOR_CFG(1280, 960, 15, 12, 1),
	FIMC_IS_SENSOR_CFG(800, 600, 15, 12, 2),
	FIMC_IS_SENSOR_CFG(704, 576, 15, 12, 3),
	FIMC_IS_SENSOR_CFG(640, 480, 30, 6, 4),
	FIMC_IS_SENSOR_CFG(640, 480, 24, 6, 5),
	FIMC_IS_SENSOR_CFG(352, 288, 15, 6, 6),
	FIMC_IS_SENSOR_CFG(176, 144, 15, 12, 7),
};

static const struct db8221a_fps db8221a_framerates[] = {
	{ I_FPS_0,	FRAME_RATE_AUTO },
	{ I_FPS_7,	FRAME_RATE_7 },
	{ I_FPS_15,	FRAME_RATE_15 },
	{ I_FPS_24,	FRAME_RATE_24 },
};

static const struct db8221a_framesize db8221a_preview_frmsizes[] = {
	{ PREVIEW_SZ_QCIF,	176,  144 },
	{ PREVIEW_SZ_CIF,	352,  288 },
	{ PREVIEW_SZ_320x240,	320,  240 },
	{ PREVIEW_SZ_VGA,	640,  480 },
	{ PREVIEW_SZ_704x576,	704,  576 },
	{ PREVIEW_SZ_SVGA,	800,  600 },
	{ PREVIEW_SZ_1280x960,	1280, 960},
	{ PREVIEW_SZ_UXGA,	1600, 1200},
};

static const struct db8221a_framesize db8221a_capture_frmsizes[] = {
	{ CAPTURE_SZ_VGA,	640,  480  },
	{ CAPTURE_SZ_704x576,	704,  576  },
	{ CAPTURE_SZ_SVGA,	800,  600  },
	{ CAPTURE_SZ_1MP,	1280, 960  },
	{ CAPTURE_SZ_2MP,	1600, 1200 },
};

static struct fimc_is_vci vci_db8221a[] = {
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.config = {{0, HW_FORMAT_YUV422_8BIT}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_USER}, {3, 0}}
	}
};

static inline struct fimc_is_module_enum *to_module(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return module;
}

static inline struct db8221a_state *to_state(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return (struct db8221a_state *)module->private_data;
}

static inline struct i2c_client *to_client(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return (struct i2c_client *)module->client;
}

static int fimc_is_db8221a_read8(struct i2c_client *client, u8 addr, u8 *val)
{
	int ret = 0;
	struct i2c_msg msg[2];
	u8 wbuf[2];

	if (!client->adapter) {
		cam_err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 1;
	msg[0].buf = wbuf;
	/* TODO : consider other size of buffer */
	wbuf[0] = addr;

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		cam_err("i2c transfer fail(%d)", ret);
		cam_info("I2CW08(0x%04X) [0x%04X] : 0x%04X\n", client->addr, addr, *val);
		goto p_err;
	}

#ifdef PRINT_I2CCMD
	cam_info("I2CR08(0x%04X) [0x%04X] : 0x%04X\n", client->addr, addr, *val);
#endif

p_err:
	return ret;
}

static int fimc_is_db8221a_write8(struct i2c_client *client, u8 addr, u8 val)
{
	int ret = 0;
	struct i2c_msg msg[1];
	u8 wbuf[3];

	if (!client->adapter) {
		cam_err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = wbuf;
	wbuf[0] = addr;
	wbuf[1] = val;

	if (addr == 0xFE) {
		cam_info("%s : use delay (%d ms) in I2C Write \n", __func__, val);
		msleep(val);
	} else {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret < 0) {
			cam_err("i2c transfer fail(%d)", ret);
			cam_info("I2CW08(0x%04X) [0x%04X] : 0x%04X\n", client->addr, addr, val);
		}
	}

#ifdef PRINT_I2CCMD
	cam_info("I2CW08(0x%04X) [0x%04X] : 0x%04X\n", client->addr, addr, val);
#endif

p_err:
	return ret;
}

#ifndef CONFIG_LOAD_FILE
static int db8221a_write_regs(struct v4l2_subdev *sd, const struct sensor_reg regs[], int size)
{
	int err = 0, i;
	struct i2c_client *client = to_client(sd);

	for (i = 0; i < size; i++) {
		err = fimc_is_db8221a_write8(client, regs[i].addr, regs[i].data);
		CHECK_ERR_MSG(err, "register set failed\n");
	}

	return 0;
}
#else
#define TABLE_MAX_NUM 500
static char *db8221a_regs_table;
static int db8221a_regs_table_size;

int db8221a_regs_table_init(void)
{
	struct file *filp;
	char *dp;
	long size;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	cam_info("%s : E\n", __func__);

	set_fs(get_ds());

	filp = filp_open(LOAD_FILE_PATH, O_RDONLY, 0);

	if (IS_ERR_OR_NULL(filp)) {
		cam_err("file open error\n");
		return PTR_ERR(filp);
	}

	size = filp->f_path.dentry->d_inode->i_size;
	cam_dbg("size = %ld\n", size);
	//dp = kmalloc(size, GFP_KERNEL);
	dp = vmalloc(size);
	if (unlikely(!dp)) {
		cam_err("Out of Memory\n");
		filp_close(filp, current->files);
		return -ENOMEM;
	}

	pos = 0;
	memset(dp, 0, size);
	ret = vfs_read(filp, (char __user *)dp, size, &pos);

	if (unlikely(ret != size)) {
		cam_err("Failed to read file ret = %d\n", ret);
		/*kfree(dp);*/
		vfree(dp);
		filp_close(filp, current->files);
		return -EINVAL;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	db8221a_regs_table = dp;

	db8221a_regs_table_size = size;

	*((db8221a_regs_table + db8221a_regs_table_size) - 1) = '\0';

	cam_info("%s : X\n", __func__);
	return 0;
}

void db8221a_regs_table_exit(void)
{
	printk(KERN_DEBUG "%s %d\n", __func__, __LINE__);

	if (db8221a_regs_table) {
		vfree(db8221a_regs_table);
		db8221a_regs_table = NULL;
	}
}

static int db8221a_write_regs_from_sd(struct v4l2_subdev *sd, const char *name)
{
	char *start = NULL, *end = NULL, *reg = NULL, *temp_start = NULL;
	u8 addr = 0, value = 0;
	u8 data = 0;
	char data_buf[5] = {0, };
	u32 len = 0;
	int err = 0;
	struct i2c_client *client = to_client(sd);

	cam_info("%s : E, db8221a_regs_table_size - %d\n", __func__, db8221a_regs_table_size);

	addr = value = 0;

	*(data_buf + 4) = '\0';

	start = strnstr(db8221a_regs_table, name, db8221a_regs_table_size);
	CHECK_ERR_COND_MSG(start == NULL, -ENODATA, "start is NULL\n");

	end = strnstr(start, "};", db8221a_regs_table_size);
	CHECK_ERR_COND_MSG(start == NULL, -ENODATA, "end is NULL\n");

	while (1) {
		len = end -start;
		temp_start = strnstr(start, "{", len);
		if (!temp_start || (temp_start > end)) {
			cam_info("write end of %s\n", name);
			break;
		}
		start = temp_start;

		len = end -start;
		/* Find Address */
		reg = strnstr(start, "0x", len);
		if (!reg || (reg > end)) {
			cam_info("write end of %s\n", name);
			break;
		}

		start = (reg + 4);

		/* Write Value to Address */
		memcpy(data_buf, reg, 4);

		err = kstrtou8(data_buf, 16, &data);
		CHECK_ERR_MSG(err, "kstrtou16 failed\n");

		addr = data;
		len = end -start;
		/* Find Data */
		reg = strnstr(start, "0x", len);
		if (!reg || (reg > end)) {
			cam_info("write end of %s\n", name);
			break;
		}

		/* Write Value to Address */
		memcpy(data_buf, reg, 4);

		err = kstrtou8(data_buf, 16, &data);
		CHECK_ERR_MSG(err, "kstrtou16 failed\n");

		value = data;

		err = fimc_is_db8221a_write8(client, addr, value);
		CHECK_ERR_MSG(err, "register set failed\n");
	}

	cam_info("%s : X\n", __func__);

	return (err > 0) ? 0 : err;
}
#endif


static int sensor_db8221a_apply_set(struct v4l2_subdev *sd,
	const char * setting_name, const struct sensor_regset *table)
{
	int ret = 0;

	cam_info("%s : E, setting_name : %s\n", __func__, setting_name);

#ifdef CONFIG_LOAD_FILE
	cam_info("COFIG_LOAD_FILE feature is enabled\n");
	ret = db8221a_write_regs_from_sd(sd, setting_name);
	CHECK_ERR_MSG(ret, "[COFIG_LOAD_FILE]regs set apply is fail\n");
#else
	ret = db8221a_write_regs(sd, table->reg, table->size);
	CHECK_ERR_MSG(ret, "regs set apply is fail\n");
#endif
	cam_info("%s : X\n", __func__);

	return ret;
}

static int __find_resolution(struct v4l2_subdev *subdev,
			     struct v4l2_mbus_framefmt *mf,
			     enum db8221a_oprmode *type,
			     u32 *resolution)
{
	struct db8221a_state *state = to_state(subdev);
	const struct db8221a_resolution *fsize = &db8221a_resolutions[0];
	const struct db8221a_resolution *match = NULL;

	enum db8221a_oprmode stype = state->oprmode;
	int i = ARRAY_SIZE(db8221a_resolutions);
	unsigned int min_err = ~0;
	int err;

	while (i--) {
		if (stype == fsize->type) {
			err = abs(fsize->width - mf->width)
				+ abs(fsize->height - mf->height);

			if (err < min_err) {
				min_err = err;
				match = fsize;
				stype = fsize->type;
			}
		}
		fsize++;
	}
	cam_info("LINE(%d): mf width: %d, mf height: %d, mf code: %d\n", __LINE__,
		mf->width, mf->height, stype);
	if (match) {
		mf->width  = match->width;
		mf->height = match->height;
		*resolution = match->value;
		*type = stype;
		cam_info("LINE(%d): match width: %d, match height: %d, match code: %d\n", __LINE__,
			match->width, match->height, stype);
		return 0;
	}
	cam_info("LINE(%d): mf width: %d, mf height: %d, mf code: %d\n", __LINE__,
		mf->width, mf->height, stype);

	return -EINVAL;
}

static struct v4l2_mbus_framefmt *__find_format(struct db8221a_state *state,
				struct v4l2_subdev_fh *fh,
				enum v4l2_subdev_format_whence which,
				enum db8221a_oprmode type)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return fh ? v4l2_subdev_get_try_format(fh, 0) : NULL;

	return &state->ffmt[type];
}

static const struct db8221a_framesize *db8221a_get_framesize
	(const struct db8221a_framesize *frmsizes,
	u32 frmsize_count, u32 index)
{
	int i = 0;

	for (i = 0; i < frmsize_count; i++) {
		if (frmsizes[i].index == index)
			return &frmsizes[i];
	}

	return NULL;
}

static void db8221a_set_framesize(struct v4l2_subdev *subdev,
				const struct db8221a_framesize *frmsizes,
				u32 num_frmsize, bool preview)
{
	struct db8221a_state *state = to_state(subdev);
	const struct db8221a_framesize **found_frmsize = NULL;
	u32 width = state->req_fmt.width;
	u32 height = state->req_fmt.height;
	int i = 0;

	cam_info("%s: Requested Res %dx%d\n", __func__, width, height);

	found_frmsize = preview ?
		&state->preview.frmsize : &state->capture.frmsize;

	for (i = 0; i < num_frmsize; i++) {
		if ((frmsizes[i].width == width) &&
			(frmsizes[i].height == height)) {
			*found_frmsize = &frmsizes[i];
			break;
		}
	}

	if (*found_frmsize == NULL) {
		cam_err("%s: error, invalid frame size %dx%d\n",
			__func__, width, height);
		*found_frmsize = preview ?
			db8221a_get_framesize(frmsizes, num_frmsize,
					PREVIEW_SZ_SVGA) :
			db8221a_get_framesize(frmsizes, num_frmsize,
					CAPTURE_SZ_SVGA);
		//BUG_ON(!(*found_frmsize));
	}

	if (preview)
		cam_info("Preview Res Set: %dx%d, index %d\n",
			(*found_frmsize)->width, (*found_frmsize)->height,
			(*found_frmsize)->index);
	else
		cam_info("Capture Res Set: %dx%d, index %d\n",
			(*found_frmsize)->width, (*found_frmsize)->height,
			(*found_frmsize)->index);
}

static int db8221a_set_anti_banding(struct v4l2_subdev *subdev, int anti_banding)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	cam_info("%s( %d ) : E\n",__func__, anti_banding);

	switch (anti_banding) {
	case ANTI_BANDING_AUTO :
	case ANTI_BANDING_OFF :
	case ANTI_BANDING_50HZ :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_anti_banding_flicker_50hz",
			&db8221a_regset_table.anti_banding_flicker_50hz);
		break;
	case ANTI_BANDING_60HZ :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_anti_banding_flicker_60hz",
			&db8221a_regset_table.anti_banding_flicker_60hz);
		break;
	default:
		cam_dbg("%s: Not support.(%d)\n", __func__, anti_banding);
		break;
	}

	state->anti_banding = anti_banding;
	return ret;
}

static int db8221a_set_frame_rate(struct v4l2_subdev *subdev, s32 fps)
{
	struct db8221a_state *state = to_state(subdev);
	int min = FRAME_RATE_AUTO;
	int max = FRAME_RATE_24;
	int ret = 0;
	int i = 0, fps_index = -1;

	cam_info("set frame rate %d\n", fps);

	if ((fps < min) || (fps > max)) {
		cam_warn("set_frame_rate: error, invalid frame rate %d\n", fps);
		fps = (fps < min) ? min : max;
	}

	if (unlikely(!state->initialized)) {
		cam_info("pending fps %d\n", fps);
		state->req_fps = fps;
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(db8221a_framerates); i++) {
		if (fps == db8221a_framerates[i].fps) {
			fps_index = db8221a_framerates[i].index;
			state->req_fps = -1;
			break;
		}
	}

	if (unlikely(fps_index < 0)) {
		cam_warn("set_fps: warning, not supported fps %d\n", fps);
		return 0;
	}

	if (fps == state->fps) {
		cam_info("skip same fps: fps:%d, state->fps:%d\n", fps, state->fps);
		return 0;
	}

	if(state->fps == FRAME_RATE_24)
	{
		sensor_db8221a_apply_set(subdev, "db8221a_Init_Reg",
				&db8221a_regset_table.init);
	}

	switch (fps) {
	case FRAME_RATE_AUTO :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_fps_auto", &db8221a_regset_table.fps_auto);
		db8221a_set_anti_banding(subdev, state->anti_banding);
		break;
	case FRAME_RATE_7 :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_fps_7", &db8221a_regset_table.fps_7);
		db8221a_set_anti_banding(subdev, state->anti_banding);
		break;
	case FRAME_RATE_15 :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_fps_15", &db8221a_regset_table.fps_15);
		db8221a_set_anti_banding(subdev, state->anti_banding);
		break;
	case FRAME_RATE_24 :
		{
			u32 width, height;
			if (unlikely(!state->preview.frmsize)) {
				cam_warn("warning, preview resolution not set\n");
				state->preview.frmsize = db8221a_get_framesize(db8221a_preview_frmsizes,
				                                               ARRAY_SIZE(db8221a_preview_frmsizes),
				                                               PREVIEW_SZ_SVGA);
			}
			width = state->preview.frmsize->width;
			height = state->preview.frmsize->height;

			if (width == 352 && height == 288) {
				ret = sensor_db8221a_apply_set(subdev, "db8221a_24fps_camcoder_352_288",
					&db8221a_regset_table.camcoder_352_288);
			} else if (width == 176 && height == 144) {
				ret = sensor_db8221a_apply_set(subdev, "db8221a_24fps_camcoder_176_144",
					&db8221a_regset_table.camcoder_176_144);
			} else {
				ret = sensor_db8221a_apply_set(subdev, "db8221a_24fps_camcoder_640_480",
					&db8221a_regset_table.camcoder_640_480);
			}
			db8221a_set_anti_banding(subdev, state->anti_banding);
		}
		break;
	default:
		cam_dbg("%s: Not supported fps (%d)\n", __func__, fps);
		break;
	}

	state->fps = fps;
	CHECK_ERR_MSG(ret, "fail to set framerate\n");

	return 0;
}

static int db8221a_set_effect(struct v4l2_subdev *subdev, int effect)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	cam_info("%s( %d ) : E\n",__func__, effect);

	switch (effect) {
	case V4L2_IMAGE_EFFECT_BASE :
	case V4L2_IMAGE_EFFECT_NONE :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_Effect_Normal", &db8221a_regset_table.effect_none);
		break;
	case V4L2_IMAGE_EFFECT_BNW :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_Effect_Gray", &db8221a_regset_table.effect_gray);
		break;
	case V4L2_IMAGE_EFFECT_SEPIA :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_Effect_Sepia", &db8221a_regset_table.effect_sepia);
		break;
	case V4L2_IMAGE_EFFECT_NEGATIVE :
		ret = sensor_db8221a_apply_set(subdev, "db8221a_Effect_Negative", &db8221a_regset_table.effect_negative);
		break;
	default:
		cam_dbg("%s: Not support.(%d)\n", __func__, effect);
		break;
	}

	state->effect = effect;
	return ret;
}

static int db8221a_set_metering(struct v4l2_subdev *subdev, int metering)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	cam_info("%s( %d ) : E\n",__func__, metering);

	switch (metering) {
	case METERING_MATRIX:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_metering_matrix",
			&db8221a_regset_table.metering_matrix);
		break;
	case METERING_CENTER:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_metering_center",
			&db8221a_regset_table.metering_center);
		break;
	case METERING_SPOT:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_metering_spot",
			&db8221a_regset_table.metering_spot);
		break;
	default:
		cam_dbg("%s: Not support.(%d)\n", __func__, metering);
		break;
	}

	state->metering = metering;
	return ret;
}

static int db8221a_set_brightness(struct v4l2_subdev *subdev, int brightness)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	cam_info("%s( %d ) : E\n",__func__, brightness);

	switch (brightness) {
	case EV_MINUS_4:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_m4",
			&db8221a_regset_table.brightness_m4);
		break;
	case EV_MINUS_3:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_m3",
			&db8221a_regset_table.brightness_m3);
		break;
	case EV_MINUS_2:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_m2",
			&db8221a_regset_table.brightness_m2);
		break;
	case EV_MINUS_1:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_m1",
			&db8221a_regset_table.brightness_m1);
		break;
	case EV_DEFAULT:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_default",
			&db8221a_regset_table.brightness_default);
		break;
	case EV_PLUS_1:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_p1",
			&db8221a_regset_table.brightness_p1);
		break;
	case EV_PLUS_2:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_p2",
			&db8221a_regset_table.brightness_p2);
		break;
	case EV_PLUS_3:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_p3",
			&db8221a_regset_table.brightness_p3);
		break;
	case EV_PLUS_4:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_brightness_p4",
			&db8221a_regset_table.brightness_p4);
		break;
	default:
		cam_dbg("%s: Not support.(%d)\n", __func__, brightness);
		break;
	}

	state->brightness = brightness;
	return ret;
}

static int db8221a_set_contrast(struct v4l2_subdev *subdev, int contrast)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	cam_info("%s( %d ) : E\n",__func__, contrast);

	switch (contrast) {
	case CONTRAST_MINUS_2:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_contrast_m2",
			&db8221a_regset_table.contrast_m2);
		break;
	case CONTRAST_MINUS_1:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_contrast_m1",
			&db8221a_regset_table.contrast_m1);
		break;
	case CONTRAST_DEFAULT:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_contrast_default",
			&db8221a_regset_table.contrast_default);
		break;
	case CONTRAST_PLUS_1:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_contrast_p1",
			&db8221a_regset_table.contrast_p1);
		break;
	case CONTRAST_PLUS_2:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_contrast_p2",
			&db8221a_regset_table.contrast_p2);
		break;
	default:
		cam_dbg("%s: Not support.(%d)\n", __func__, contrast);
		break;
	}

	state->contrast = contrast;
	return ret;
}

static int db8221a_set_wb(struct v4l2_subdev *subdev, int wb)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	cam_info("%s( %d ) : E\n",__func__, wb);

	switch (wb) {
	case WHITE_BALANCE_BASE:
	case WHITE_BALANCE_AUTO:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_wb_auto",
			&db8221a_regset_table.wb_auto);
		break;
	case WHITE_BALANCE_SUNNY:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_wb_daylight",
			&db8221a_regset_table.wb_daylight);
		break;
	case WHITE_BALANCE_CLOUDY:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_wb_cloudy",
			&db8221a_regset_table.wb_cloudy);
		break;
	case WHITE_BALANCE_TUNGSTEN:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_wb_incandescen",
			&db8221a_regset_table.wb_incandescent);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_wb_fluorescent",
			&db8221a_regset_table.wb_fluorescent);
		break;
	default:
		cam_dbg("%s: Not support.(%d)\n", __func__, wb);
		break;
	}

	state->white_balance = wb;
	return ret;
}

static int db8221a_set_hflip(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("[%s:%d] E \n",__func__, __LINE__);

	ret = sensor_db8221a_apply_set(subdev, "db8221a_HFlip", &db8221a_regset_table.hflip);

	return ret;
}

static int db8221a_set_vflip(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("[%s:%d] E \n",__func__, __LINE__);

	ret = sensor_db8221a_apply_set(subdev, "db8221a_VFlip", &db8221a_regset_table.vflip);

	return ret;
}

static int db8221a_set_flip_off(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("[%s:%d] E \n",__func__, __LINE__);

	ret = sensor_db8221a_apply_set(subdev, "db8221a_Flip_Off", &db8221a_regset_table.flip_off);

	return ret;
}

#if 0
static int db8221a_set_camcorder_flip(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("[%s:%d] E \n",__func__, __LINE__);

	ret = sensor_db8221a_apply_set(subdev, "db8221a_Camcorder_Flip", &db8221a_regset_table.camcorder_flip);

	return ret;
}
#endif

static int db8221a_set_vt_mode(struct v4l2_subdev *subdev, int vt_mode)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	cam_info("%s( %d ) : E\n",__func__, vt_mode);

	switch (vt_mode) {
	case VT_MODE_OFF:
		ret = sensor_db8221a_apply_set(subdev, "db8221a_Init_Reg",
			&db8221a_regset_table.init);
		break;
	case VT_MODE_1:
		break;
	default:
		cam_dbg("%s: Not support.(%d)\n", __func__, vt_mode);
		break;
	}

	state->vt_mode = vt_mode;
	return ret;
}

int sensor_db8221a_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;

	cam_info("stream on\n");

	ret = sensor_db8221a_apply_set(subdev, "db8221a_start_stream", &db8221a_regset_table.start_stream);

	return ret;
}

int sensor_db8221a_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct db8221a_state *state = to_state(subdev);

	cam_info("stream off\n");
	ret = sensor_db8221a_apply_set(subdev, "db8221a_stop_stream", &db8221a_regset_table.stop_stream);
	state->preview.update_frmsize = 1;

	switch (state->runmode) {
	case RUNMODE_CAPTURING:
		cam_dbg("Capture Stop!\n");
		state->runmode = RUNMODE_CAPTURING_STOP;
		ret  = db8221a_set_flip_off(subdev);
		break;

	case RUNMODE_RUNNING:
		cam_dbg("Preview Stop!\n");
		state->runmode = RUNMODE_RUNNING_STOP;
		break;

	case RUNMODE_RECORDING:
		cam_dbg("Recording Stop!\n");
		state->runmode = RUNMODE_RECORDING_STOP;
		break;

	default:
		break;
	}

	return ret;
}

static int db8221a_set_sensor_mode(struct v4l2_subdev *subdev, s32 val)
{
	struct db8221a_state *state = to_state(subdev);

	cam_info("mode=%d\n", val);

	switch (val) {
	case SENSOR_MOVIE:
		/* We does not support movie mode when in VT. */
		if (state->vt_mode) {
			state->sensor_mode = SENSOR_CAMERA;
			cam_warn("%s: error, Not support movie\n", __func__);
		} else {
			state->sensor_mode = SENSOR_MOVIE;
		}
		break;
	case SENSOR_CAMERA:
		state->sensor_mode = SENSOR_CAMERA;
		break;

	default:
		cam_err("%s: error, Not support.(%d)\n", __func__, val);
		state->sensor_mode = SENSOR_CAMERA;
		break;
	}

	return 0;
}

static inline int db8221a_get_exif_exptime(struct v4l2_subdev *subdev, u32 *exp_time)
{
	struct i2c_client *client = to_client(subdev);
	int err = 0;
	u8 coarseTimeH = 0;
	u8 coarseTimeL = 0;
	u16 coarseTime = 0;
	u8 lineLengthH = 0;
	u8 lineLengthL = 0;
	u16 lineLength = 0;
	int SensorInputClock = 72000000;
	u32 exposureTime = 0;

	err = fimc_is_db8221a_write8(client, 0xFF, 0xE8);
	CHECK_ERR_COND(err < 0, -ENODEV);

	fimc_is_db8221a_read8(client, 0x06, &coarseTimeH);
	fimc_is_db8221a_read8(client, 0x07, &coarseTimeL);
	coarseTime = (coarseTimeH << 8) | coarseTimeL;
	cam_info("coarseTimeH = 0x%x, coarseTimeL = 0x%x, coarseTime = 0x%x\n",
		coarseTimeH, coarseTimeL, coarseTime);

	err = fimc_is_db8221a_write8(client, 0xFF, 0xE8);
	CHECK_ERR_COND(err < 0, -ENODEV);

	fimc_is_db8221a_read8(client, 0x0A, &lineLengthH);
	fimc_is_db8221a_read8(client, 0x0B, &lineLengthL);
	lineLength = (lineLengthH << 8) | lineLengthL;
	cam_info("lineLengthH = 0x%x, lineLengthL = 0x%x, lineLength = 0x%x\n",
		lineLengthH, lineLengthL, lineLength);

	exposureTime = SensorInputClock/(coarseTime*lineLength*2);

	cam_info("exposureTime = %d\n", exposureTime);
	*exp_time = exposureTime;

	return err;
}

static inline int db8221a_get_exif_iso(struct v4l2_subdev *subdev, u16 *iso)
{
	struct db8221a_state *state = to_state(subdev);
	if (state->exif.exp_time_den <= 10)
		*iso = 400;
	else if (state->exif.exp_time_den > 10 && state->exif.exp_time_den <= 20)
		*iso = 200;
	else if (state->exif.exp_time_den > 20 && state->exif.exp_time_den <= 30)
		*iso = 100;
	else
		*iso = 50;

	cam_dbg("ISO=%d\n", *iso);
	return 0;
}

static inline void db8221a_get_exif_flash(struct v4l2_subdev *subdev,
					u16 *flash)
{
	*flash = 0;
}

static int db8221a_get_exif(struct v4l2_subdev *subdev)
{
	struct db8221a_state *state = to_state(subdev);

	/* exposure time */
	state->exif.exp_time_den = 0;
	db8221a_get_exif_exptime(subdev, &state->exif.exp_time_den);
	/* iso */
	state->exif.iso = 0;
	db8221a_get_exif_iso(subdev, &state->exif.iso);
	/* flash */
	db8221a_get_exif_flash(subdev, &state->exif.flash);

	return 0;
}

static int sensor_db8221a_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	u8 id;

	struct db8221a_state *state = to_state(subdev);
	struct i2c_client *client = to_client(subdev);

	cam_info("%s : E \n", __func__);

	state->system_clock = 146 * 1000 * 1000;
	state->line_length_pck = 146 * 1000 * 1000;

#ifdef CONFIG_LOAD_FILE
	ret = db8221a_regs_table_init();
	CHECK_ERR_MSG(ret, "[CONFIG_LOAD_FILE] init fail \n");
#endif
	ret = fimc_is_db8221a_read8(client, 0x4, &id);
	if (ret < 0) {
		cam_err("%s: read id failed\n", __func__);
		goto p_err;
	}

	ret = sensor_db8221a_apply_set(subdev, "db8221a_Init_Reg", &db8221a_regset_table.init);
	if (ret) {
		cam_err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	ret = sensor_db8221a_apply_set(subdev, "db8221a_stop_stream", &db8221a_regset_table.stop_stream);
	if (ret) {
		cam_err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	state->initialized = 1;
	state->power_on =  db8221a_HW_POWER_ON;
	state->runmode = RUNMODE_INIT;
	state->preview.update_frmsize = 1;
	state->sensor_mode = SENSOR_CAMERA;
	state->contrast = CONTRAST_DEFAULT;
	state->effect = V4L2_IMAGE_EFFECT_NONE;
	state->metering = METERING_MATRIX;
	state->white_balance = WHITE_BALANCE_AUTO;
	state->fps = FRAME_RATE_AUTO;
	state->req_fps = FRAME_RATE_AUTO;
	state->recording = 0;
	state->vt_mode = VT_MODE_OFF;
	state->anti_banding = ANTI_BANDING_AUTO;
	state->brightness = EV_DEFAULT;
	state->hflip = false;
	state->vflip = false;

	cam_info("%s(id : %X) : X \n", __func__, id);

p_err:
	return ret;
}


static int db8221a_s_power(struct v4l2_subdev *subdev, int on)
{
	struct db8221a_state *state = to_state(subdev);
	int tries = 3;
	int err = 0;

	cam_info("db8221a_s_power=%d\n", on);

	if (on) {

		state->runmode = RUNMODE_NOTREADY;

		if (!state->initialized) {
retry:
			err = sensor_db8221a_init(subdev, 0);
			if (err && --tries) {
				cam_err("s_stream: retry to init...\n");
				// err = db8221a_reset(subdev, 1);
				cam_err("s_stream: power-on failed\n");
				goto retry;
			}
		}
	} else {
		state->initialized = 0;
		state->power_on = db8221a_HW_POWER_OFF;
	}

	return err;
}

static int sensor_db8221a_s_capture_mode(struct v4l2_subdev *subdev, int value)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	cam_info("%s : value(%d) - E\n",__func__, value);

	if ((SENSOR_CAMERA == state->sensor_mode) && value) {
		cam_info("s_ctrl : Capture Mode!\n");
		state->format_mode = V4L2_PIX_FMT_MODE_CAPTURE;
	} else {
		cam_info("s_ctrl : Preview Mode!\n");
		state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
	}

	return ret;
}

static int db8221a_set_capture_size(struct v4l2_subdev *subdev)
{
	struct db8221a_state *state = to_state(subdev);

	u32 width, height;
	int err = 0;

	cam_info("%s - E\n",__func__);

	if (unlikely(!state->capture.frmsize)) {
		cam_warn("warning, capture resolution not set\n");
		state->capture.frmsize = db8221a_get_framesize(
					db8221a_capture_frmsizes,
					ARRAY_SIZE(db8221a_capture_frmsizes),
					CAPTURE_SZ_SVGA);
	}

	width = state->capture.frmsize->width;
	height = state->capture.frmsize->height;
	state->preview.update_frmsize = 1;

	/* Transit to capture mode */
	if (state->capture.lowlux_night) {
		cam_info("capture_mode: night lowlux\n");
	/*	err = sensor_db8221a_apply_set(client, &db8221a_regset_table.lowcapture);*/
	}

	cam_info("%s - width * height = %d * %d \n",__func__, width, height);
	if(width == 1600 && height == 1200)
		err = sensor_db8221a_apply_set(subdev, "db8221a_resol_1600_1200", &db8221a_regset_table.resol_1600_1200);
	else if(width == 1280 && height == 960)
		err = sensor_db8221a_apply_set(subdev, "db8221a_resol_1280_960", &db8221a_regset_table.resol_1280_960);
	else if(width == 800 && height == 600)
		err = sensor_db8221a_apply_set(subdev, "db8221a_resol_800_600", &db8221a_regset_table.resol_800_600);
	else if(width == 704 && height == 576)
		err = sensor_db8221a_apply_set(subdev, "db8221a_resol_704_576", &db8221a_regset_table.resol_704_576);
	else if(width == 352 && height == 288)
		err = sensor_db8221a_apply_set(subdev, "db8221a_resol_352_288", &db8221a_regset_table.resol_352_288);
	else if(width == 176 && height == 144)
		err = sensor_db8221a_apply_set(subdev, "db8221a_resol_176_144", &db8221a_regset_table.resol_176_144);
	else
		err = sensor_db8221a_apply_set(subdev, "db8221a_resol_640_480", &db8221a_regset_table.resol_640_480);

	CHECK_ERR_MSG(err, "fail to capture_mode (%d)\n", err);

	cam_info("%s - X\n",__func__);

	return err;
}

static int db8221a_start_capture(struct v4l2_subdev *subdev)
{
	struct db8221a_state *state = to_state(subdev);
	int err = 0;
	u32 capture_delay;

	cam_info("%s: E\n", __func__);

	err = db8221a_set_capture_size(subdev);
	CHECK_ERR(err);

	if (state->hflip == 1)
		err  = db8221a_set_hflip(subdev);
	else if (state->vflip == 1)
		err  = db8221a_set_vflip(subdev);

	err = sensor_db8221a_stream_on(subdev);
	CHECK_ERR(err);

	state->runmode = RUNMODE_CAPTURING;
	capture_delay = 10;
	msleep(capture_delay);

	/* Get EXIF */
	db8221a_get_exif(subdev);

	cam_info("%s: X\n", __func__);

	return 0;
}

static int db8221a_set_preview_size(struct v4l2_subdev *subdev)
{
	struct db8221a_state *state = to_state(subdev);
	int err = 0;

	u32 width, height;

	if (!state->preview.update_frmsize)
		return 0;

	if (unlikely(!state->preview.frmsize)) {
		cam_warn("warning, preview resolution not set\n");
		state->preview.frmsize = db8221a_get_framesize(
					db8221a_preview_frmsizes,
					ARRAY_SIZE(db8221a_preview_frmsizes),
					PREVIEW_SZ_SVGA);
	}

	width = state->preview.frmsize->width;
	height = state->preview.frmsize->height;

	cam_info("set preview size(%dx%d) sensor_mode = %d\n",
		width, height, state->sensor_mode);

	if (state->sensor_mode == SENSOR_MOVIE || state->fps == FRAME_RATE_24) {
		cam_info("Skip setting preview size on movie or fixed 24fps mode.\n");
	} else {
		if (state->vt_mode == VT_MODE_OFF) {
			if (width == 1600 && height == 1200) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_resol_1600_1200",
					&db8221a_regset_table.resol_1600_1200);
			} else if (width == 1280 && height == 960) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_resol_1280_960",
					&db8221a_regset_table.resol_1280_960);
			} else if (width == 800 && height == 600) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_resol_800_600",
					&db8221a_regset_table.resol_800_600);
			} else if (width == 704 && height == 576) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_resol_704_576",
					&db8221a_regset_table.resol_704_576);
			} else if (width == 352 && height == 288) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_resol_352_288",
					&db8221a_regset_table.resol_352_288);
			} else if (width == 176 && height == 144) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_resol_176_144",
					&db8221a_regset_table.resol_176_144);
			} else if (width == 320 && height == 240) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_resol_320_240",
					&db8221a_regset_table.resol_320_240);
			} else {
				err = sensor_db8221a_apply_set(subdev, "db8221a_resol_640_480",
					&db8221a_regset_table.resol_640_480);
			}
		} else {
			if (width == 352 && height == 288) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_Init_VT_Reg_352_288",
						&db8221a_regset_table.init_vt_352_288);
			} else if (width == 176 && height == 144) {
				err = sensor_db8221a_apply_set(subdev, "db8221a_Init_VT_Reg_176_144",
						&db8221a_regset_table.init_vt_176_144);
			} else {
				err = sensor_db8221a_apply_set(subdev, "db8221a_Init_VT_Reg_640_480",
						&db8221a_regset_table.init_vt_640_480);
			}
		}
	}
	CHECK_ERR_MSG(err, "fail to set preview size\n");

	state->preview.update_frmsize = 0;

	return 0;
}

static int db8221a_start_preview(struct v4l2_subdev *subdev)
{
	struct db8221a_state *state = to_state(subdev);
	int err = 0;

	cam_info("Camera Preview start : E - runmode = %d\n", state->runmode);

	if ((state->runmode == RUNMODE_NOTREADY) ||
	    (state->runmode == RUNMODE_CAPTURING)) {
		cam_err("%s: error - Invalid runmode\n", __func__);
		return -EPERM;
	}

	/* Check pending fps */
	if (state->req_fps >= 0) {
		err = db8221a_set_frame_rate(subdev, state->req_fps);
		CHECK_ERR(err);
	}

	/* Set preview size */
	err = db8221a_set_preview_size(subdev);
	CHECK_ERR_MSG(err, "failed to set preview size(%d)\n", err);

	err = sensor_db8221a_stream_on(subdev);
	CHECK_ERR(err);

	if (RUNMODE_INIT == state->runmode)
		msleep(100);

	state->runmode = (state->sensor_mode == SENSOR_CAMERA) ?
			RUNMODE_RUNNING : RUNMODE_RECORDING;

	cam_info("Camera Preview start : X - runmode = %d\n", state->runmode);
	return 0;
}

static int sensor_db8221a_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	if (!state->initialized && ctrl->id != V4L2_CID_CAMERA_SENSOR_MODE) {
		cam_warn("s_ctrl: warning, camera not initialized. ID %d(0x%08X)\n",
			ctrl->id & 0xFF, ctrl->id);
		return 0;
	}

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_SENSOR_MODE:
		cam_info("V4L2_CID_CAMERA_SENSOR_MODE : %d\n", ctrl->value);
		 db8221a_set_sensor_mode(subdev, ctrl->value);
		break;
	case V4L2_CID_CAMERA_INIT:
		cam_info("V4L2_CID_CAMERA_INIT\n");
/*  // Move to set_power function
		ret = sensor_db8221a_init(subdev, 0);
		if (ret) {
			cam_err("sensor_db8221a_init is fail(%d)", ret);
			goto p_err;
		}
*/
		break;
	case V4L2_CID_CAPTURE:
		cam_info("V4L2_CID_CAPTURE : %d \n", ctrl->value);
		ret = sensor_db8221a_s_capture_mode(subdev, ctrl->value);
		if (ret) {
			cam_err("sensor_db8221a_s_capture_mode is fail(%d)", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_CAMERA_SET_SNAPSHOT_SIZE:
/* capture size of SoC sensor is to be set up in start_capture.
		cam_info("V4L2_CID_CAMERA_SET_SNAPSHOT_SIZE : %d\n", ctrl->value);
		ret = db8221a_set_capture_size(subdev, ctrl->value);
		if (ret) {
			cam_err("db8221a_set_snapshot_size is fail(%d)", ret);
			goto p_err;
		}
*/
		break;
	case V4L2_CID_IMAGE_EFFECT:
	case V4L2_CID_CAMERA_EFFECT:
		cam_info("V4L2_CID_IMAGE_EFFECT :%d\n", ctrl->value);
		ret = db8221a_set_effect(subdev, ctrl->value);
		break;
	case V4L2_CID_HFLIP:
		cam_info("V4L2_CID_HFLIP : %d\n", ctrl->value);
		state->hflip = ctrl->value;
		break;
	case V4L2_CID_VFLIP:
		cam_info("V4L2_CID_VFLIP : %d\n", ctrl->value);
		state->vflip = ctrl->value;
		break;
	case V4L2_CID_CAMERA_ANTI_BANDING:
		cam_info("V4L2_CID_CAMERA_ANTI_BANDING :%d\n", ctrl->value);
		ret = db8221a_set_anti_banding(subdev, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_METERING:
	case V4L2_CID_CAMERA_METERING:
		cam_info("V4L2_CID_CAMERA_METERING :%d\n", ctrl->value);
		ret = db8221a_set_metering(subdev, ctrl->value);
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
	case V4L2_CID_CAM_BRIGHTNESS:
		cam_info("V4L2_CID_CAMEAR_BRIGHTNESS :%d\n", ctrl->value);
		ret = db8221a_set_brightness(subdev, ctrl->value);
		break;
	case V4L2_CID_CAMERA_CONTRAST:
	case V4L2_CID_CAM_CONTRAST:
		cam_info("V4L2_CID_CAMEAR_CONTRAST :%d\n", ctrl->value);
		ret = db8221a_set_contrast(subdev, ctrl->value);
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		cam_info("V4L2_CID_WHITE_BALANCE_PRESET :%d\n", ctrl->value);
		ret = db8221a_set_wb(subdev, ctrl->value);
		break;
	case V4L2_CID_CAM_FRAME_RATE:
		cam_info("V4L2_CID_CAM_FRAME_RATE :%d\n", ctrl->value);
		ret = db8221a_set_frame_rate(subdev, ctrl->value);
		break;
	case V4L2_CID_CAMERA_VT_MODE:
		cam_info("V4L2_CID_CAMERA_VT_MODE :%d\n", ctrl->value);
		ret = db8221a_set_vt_mode(subdev, ctrl->value);
		break;
	case V4L2_CID_CAMERA_ISO:
	case V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK:
	case V4L2_CID_CAMERA_SET_POSTVIEW_SIZE:
	case V4L2_CID_JPEG_QUALITY:
	case V4L2_CID_CAMERA_FOCUS_MODE:
	case V4L2_CID_CAMERA_POWER_OFF:
		cam_dbg("s_ctrl : unsupported id is requested. ID (0x%08X)\n", ctrl->id);
		break;
	default:
		cam_warn("s_ctrl : invalid id is requested. ID (0x%08X)\n", ctrl->id);
		break;
	}

p_err:
	return ret;
}

static int sensor_db8221a_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	struct db8221a_state *state = to_state(subdev);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_EXIF_EXPOSURE_TIME_DEN:
		ctrl->value = state->exif.exp_time_den;
		cam_dbg("V4L2_CID_CAMERA_EXIF_EXPTIME :%d\n", ctrl->value);
		break;
	case V4L2_CID_EXIF_EXPOSURE_TIME_NUM:
		ctrl->value = 1;
		cam_dbg("V4L2_CID_EXIF_EXPOSURE_TIME_NUM :%d\n", ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_FLASH:
		ctrl->value = state->exif.flash;
		cam_dbg("V4L2_CID_CAMERA_EXIF_FLASH :%d\n", ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_ISO:
		ctrl->value = state->exif.iso;
		cam_dbg("V4L2_CID_CAMERA_EXIF_ISO :%d\n", ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_BV:
		cam_dbg("g_ctrl : unsupported id is requested. ID (0x%08X)\n", ctrl->id);
		break;
	default:
		cam_warn("g_ctrl : invalid ioctl(0x%08X) is requested", ctrl->id);
		break;
	}

	return ret;
}

static int sensor_db8221a_g_ext_ctrls(struct v4l2_subdev *subdev, struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_CAM_SENSOR_FW_VER:
		strcpy(ctrl->string, SENSOR_VER);
		cam_dbg("V4L2_CID_CAM_SENSOR_FW_VER :%s\n", ctrl->string);
		break;
	default:
		cam_warn("g_ext_ctrl : invalid ioctl(0x%08X) is requested", ctrl->id);
		break;
	}

	return ret;
}

static int db8221a_s_mbus_fmt(struct v4l2_subdev *subdev, struct v4l2_mbus_framefmt *fmt)
{
	struct db8221a_state *state = to_state(subdev);
	s32 previous_index = 0;

	BUG_ON(!subdev);
	BUG_ON(!fmt);

	cam_info("%s: E\n", __func__);

	cam_info("%s: pixelformat = 0x%x, colorspace = 0x%x, width = %d, height = %d\n",
		__func__, fmt->code, fmt->colorspace, fmt->width, fmt->height);

	v4l2_fill_pix_format(&state->req_fmt, fmt);

	if (state->format_mode != V4L2_PIX_FMT_MODE_CAPTURE) {
		previous_index = state->preview.frmsize ?
				state->preview.frmsize->index : -1;
		db8221a_set_framesize(subdev, db8221a_preview_frmsizes,
			ARRAY_SIZE(db8221a_preview_frmsizes), true);

		if (state->preview.frmsize) {
			if (previous_index != state->preview.frmsize->index)
				state->preview.update_frmsize = 1;
		}
	} else {
		db8221a_set_framesize(subdev, db8221a_capture_frmsizes,
			ARRAY_SIZE(db8221a_capture_frmsizes), false);
	}

	cam_info("output size : %d x %d\n", fmt->width, fmt->height);

	return 0;
}

static int db8221a_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct db8221a_state *state = to_state(subdev);
	int err = 0;

	cam_info("stream mode = %d\n", enable);

	switch (enable) {
	case STREAM_MODE_CAM_OFF:
		err = sensor_db8221a_stream_off(subdev);
		break;

	case STREAM_MODE_CAM_ON:
		if (state->format_mode == V4L2_PIX_FMT_MODE_CAPTURE)
			err = db8221a_start_capture(subdev);
		else
			err = db8221a_start_preview(subdev);
		break;

	case STREAM_MODE_MOVIE_OFF:
		cam_info("movie off");
		state->recording = 0;
		break;

	case STREAM_MODE_MOVIE_ON:
		cam_info("movie on");
		state->recording = 1;
		break;

	default:
		cam_err("%s: error - Invalid stream mode\n", __func__);
		break;
	}

	return err;
}

static int db8221a_s_param(struct v4l2_subdev *subdev, struct v4l2_streamparm *param)
{
	struct db8221a_state *state = to_state(subdev);
	s32 req_fps;

	req_fps = param->parm.capture.timeperframe.denominator /
			param->parm.capture.timeperframe.numerator;

	cam_info("s_parm state->fps=%d, state->req_fps=%d\n",
		state->fps, req_fps);

	return db8221a_set_frame_rate(subdev, req_fps);
}

static const struct v4l2_subdev_core_ops core_ops = {
	.s_power	= db8221a_s_power,
	.init		= sensor_db8221a_init,
	.s_ctrl		= sensor_db8221a_s_ctrl,
	.g_ctrl		= sensor_db8221a_g_ctrl,
	.g_ext_ctrls	= sensor_db8221a_g_ext_ctrls,
//	.s_ext_ctrls	= sensor_db8221a_g_ctrl,
};

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = db8221a_s_stream,
	.s_parm = db8221a_s_param,
	.s_mbus_fmt = db8221a_s_mbus_fmt
};

/* enum code by flite video device command */
static int db8221a_enum_mbus_code(struct v4l2_subdev *subdev,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	cam_err("Need to check\n");

	if (!code || code->index >= SIZE_DEFAULT_FFMT)
		return -EINVAL;

	code->code = default_fmt[code->index].code;

	return 0;
}

/* get format by flite video device command */
static int db8221a_get_fmt(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct db8221a_state *state = to_state(subdev);
	struct v4l2_mbus_framefmt *format;

	cam_err("Need to check\n");

	if (fmt->pad != 0)
		return -EINVAL;

	format = __find_format(state, fh, fmt->which, state->res_type);
	if (!format)
		return -EINVAL;

	fmt->format = *format;

	return 0;
}

/* set format by flite video device command */
static int db8221a_set_fmt(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct db8221a_state *state = to_state(subdev);
	struct v4l2_mbus_framefmt *format = &fmt->format;
	struct v4l2_mbus_framefmt *sfmt;
	enum db8221a_oprmode type;
	u32 resolution = 0;
	int ret;

	cam_info("%s: E\n", __func__);
	if (fmt->pad != 0)
		return -EINVAL;

	cam_info("%s: fmt->format.width = %d, fmt->format.height = %d",
			__func__, fmt->format.width, fmt->format.height);

	ret = __find_resolution(subdev, format, &type, &resolution);
	if (ret < 0)
		return ret;

	sfmt = __find_format(state, fh, fmt->which, type);
	if (!sfmt)
		return 0;

	sfmt		= &default_fmt[type];
	sfmt->width	= format->width;
	sfmt->height	= format->height;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		/* for enum size of entity by flite */
		state->ffmt[type].width		= format->width;
		state->ffmt[type].height	= format->height;
		state->ffmt[type].code		= V4L2_MBUS_FMT_YUYV8_2X8;

		/* find adaptable resolution */
		state->resolution		= resolution;
		state->code				= V4L2_MBUS_FMT_YUYV8_2X8;
		state->res_type			= type;

		/* for set foramat */
		state->req_fmt.width	= format->width;
		state->req_fmt.height	= format->height;

		if ((state->power_on == db8221a_HW_POWER_ON)
		    && (state->runmode != RUNMODE_CAPTURING))
			db8221a_s_mbus_fmt(subdev, sfmt);  /* set format */
	}

	return 0;
}

static struct v4l2_subdev_pad_ops pad_ops = {
	.enum_mbus_code	= db8221a_enum_mbus_code,
	.get_fmt	= db8221a_get_fmt,
	.set_fmt	= db8221a_set_fmt,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.pad	= &pad_ops,
	.video = &video_ops
};

/*
 * @ brief
 * frame duration time
 * @ unit
 * nano second
 * @ remarks
 */
int sensor_db8221a_s_duration(struct v4l2_subdev *subdev, u64 duration)
{
	int ret = 0;
	u32 framerate;
	struct fimc_is_module_enum *module;
	struct db8221a_state *module_db8221a;
	struct i2c_client *client;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		cam_err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_db8221a = module->private_data;
	if (unlikely(!module_db8221a)) {
		cam_err("module_db8221a is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = module->client;
	if (unlikely(!client)) {
		cam_err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	framerate = duration;
	if (framerate > FRAME_RATE_MAX) {
		cam_err("framerate is invalid(%d)", framerate);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_db8221a_g_min_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_db8221a_g_max_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_db8221a_s_exposure(struct v4l2_subdev *subdev, u64 exposure)
{
	int ret = 0;
	u8 value;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		cam_err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		cam_err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	value = exposure & 0xFF;


p_err:
	return ret;
}

int sensor_db8221a_g_min_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_db8221a_g_max_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_db8221a_s_again(struct v4l2_subdev *subdev, u64 sensitivity)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_db8221a_g_min_again(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_db8221a_g_max_again(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_db8221a_s_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_db8221a_g_min_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_db8221a_g_max_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

struct fimc_is_sensor_ops module_db8221a_ops = {
	.stream_on	= sensor_db8221a_stream_on,
	.stream_off	= sensor_db8221a_stream_off,
	.s_duration	= sensor_db8221a_s_duration,
	.g_min_duration	= sensor_db8221a_g_min_duration,
	.g_max_duration	= sensor_db8221a_g_max_duration,
	.s_exposure	= sensor_db8221a_s_exposure,
	.g_min_exposure	= sensor_db8221a_g_min_exposure,
	.g_max_exposure	= sensor_db8221a_g_max_exposure,
	.s_again	= sensor_db8221a_s_again,
	.g_min_again	= sensor_db8221a_g_min_again,
	.g_max_again	= sensor_db8221a_g_max_again,
	.s_dgain	= sensor_db8221a_s_dgain,
	.g_min_dgain	= sensor_db8221a_g_min_dgain,
	.g_max_dgain	= sensor_db8221a_g_max_dgain
};

#ifdef CONFIG_OF
static int sensor_db8221a_power_setpin(struct i2c_client *client,
		struct exynos_platform_fimc_is_module *pdata)
{
	struct device_node *dnode;
	int gpio_reset = 0;
	int gpio_standby = 0;
	int gpio_none = 0;
	int gpio_mclk = 0;
#if defined (VDD_CAM_SENSOR_A2P8_GPIO_CONTROL)
	int gpio_cam_avdd_en = 0;
#endif
//	u8 id;

	BUG_ON(!client);
	BUG_ON(!client->dev.of_node);

	dnode = client->dev.of_node;

	cam_info("%s E\n", __func__);

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		cam_err("%s failed to get PIN_RESET\n",__func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
	if (!gpio_is_valid(gpio_standby)) {
		cam_err("%s failed to get PIN_RESET\n",__func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_standby, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_standby);
	}

	gpio_mclk = of_get_named_gpio(dnode, "gpio_mclk", 0);
	if (!gpio_is_valid(gpio_mclk)) {
		cam_err("%s failed to get PIN_RESET\n", __func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_mclk, GPIOF_OUT_INIT_LOW, "CAM_MCLK_OUTPUT_LOW");
		gpio_free(gpio_mclk);
	}

#if defined (VDD_CAM_SENSOR_A2P8_GPIO_CONTROL)
	gpio_cam_avdd_en = of_get_named_gpio(dnode, "gpio_cam_avdd_en", 0);
	if (!gpio_is_valid(gpio_cam_avdd_en)) {
		cam_err("%s failed to get gpio_cam_avdd_en\n",__func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_cam_avdd_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_cam_avdd_en);
	}
#endif

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF);

	/* FRONT CAMERA - POWER ON */
#if defined (VDD_CAM_SENSOR_A2P8_GPIO_CONTROL)
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_cam_avdd_en, NULL, PIN_OUTPUT, 1, 1);
#else
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "VDD_CAM_SENSOR_A2P8", PIN_REGULATOR, 1, 11000);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 1);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 1, 1100);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 0, 1);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 0, 1);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_standby, NULL, PIN_OUTPUT, 1, 20);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_reset, NULL, PIN_OUTPUT, 1, 11000);

	/* FRONT CAMERA - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_reset, NULL, PIN_OUTPUT, 0, 15);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_standby, NULL, PIN_OUTPUT, 0, 50);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 1, 1);
#if defined (VDD_CAM_SENSOR_A2P8_GPIO_CONTROL)
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_cam_avdd_en, NULL, PIN_OUTPUT, 0, 0);
#else
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_none, "VDD_CAM_SENSOR_A2P8", PIN_REGULATOR, 0, 1);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 1);

/*
	sensor_db8221a_apply_set(client, &db8221a_regset_table.init);
	sensor_db8221a_apply_set(client, &db8221a_regset_table.stop_stream);
	fimc_is_db8221a_read8(client, 0x4, &id);
	cam_info(" %s(id%X)\n", __func__, id);
*/
	cam_info("%s X\n", __func__);
	return 0;
}
#endif /* CONFIG_OF */

int sensor_db8221a_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct exynos_platform_fimc_is_module *pdata;

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_err("core device is not yet probed");
		return -EPROBE_DEFER;
	}
	device = &core->sensor[SENSOR_DB8221A_INSTANCE];

	core->client_soc = client;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_module), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	fimc_is_sensor_module_soc_parse_dt(client, pdata, sensor_db8221a_power_setpin);
#endif
	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		probe_err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/* db8221a */
	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	module->pdata = pdata;
	module->sensor_id = SENSOR_DB8221A_NAME;
	module->subdev = subdev_module;
	module->device = SENSOR_DB8221A_INSTANCE;
	module->ops = &module_db8221a_ops;
	module->client = client;

	module->active_width = 1600;
	module->active_height = 1200;
	module->pixel_width = module->active_width + 20;
	module->pixel_height = module->active_height + 20;
	module->max_framerate = 30;
	module->position = SENSOR_POSITION_FRONT;
	module->mode = CSI_MODE_CH0_ONLY;
	module->lanes = CSI_DATA_LANES_1;
	module->vcis = ARRAY_SIZE(vci_db8221a);
	module->vci = vci_db8221a;
	module->sensor_maker = "DB";
	module->sensor_name = "DB8221A";
	module->cfgs = ARRAY_SIZE(settle_db8221a);
	module->cfg = settle_db8221a;
	module->private_data = kzalloc(sizeof(struct db8221a_state), GFP_KERNEL);
	if (!module->private_data) {
		probe_err("private_data is NULL");
		ret = -ENOMEM;
		kfree(subdev_module);
		goto p_err;
	}

	v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);

p_err:
	probe_info("%s(%d)\n", __func__, ret);
	return ret;
}

static int sensor_db8221a_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_db8221a_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-db8221a-soc",
	},
	{},
};
#endif

static const struct i2c_device_id sensor_db8221a_idt[] = {
	{ SENSOR_NAME, 0 },
};

static struct i2c_driver sensor_db8221a_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_db8221a_match
#endif
	},
	.probe	= sensor_db8221a_probe,
	.remove	= sensor_db8221a_remove,
	.id_table = sensor_db8221a_idt
};

module_i2c_driver(sensor_db8221a_driver);

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor db8221a driver");
MODULE_LICENSE("GPL v2");
