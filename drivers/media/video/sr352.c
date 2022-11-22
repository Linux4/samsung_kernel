/* drivers/media/video/sr352.c
 *
 * Copyright (c) 2010, Samsung Electronics. All rights reserved
 * Author: dongseong.lim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include "sr352.h"

static u32 dbg_level = CAMDBG_LEVEL_DEFAULT;
static u32 stats_power;
static u32 stats_init;
static u32 stats_reset;
static u32 stats_i2c_err;

bool cam_mipi_en = false;
EXPORT_SYMBOL(cam_mipi_en);

#define GPIO_CAM_MCLK			EXYNOS4_GPM2(2)
#define GPIO_CAM_3M_STBY		EXYNOS4_GPM3(4)
#define GPIO_CAM_3M_RST			EXYNOS4_GPX1(2)
#define GPIO_CAM_VT_nSTBY		EXYNOS4_GPM3(6)
#define GPIO_CAM_VT_nRST		EXYNOS4_GPM1(2)

struct class *camera_class;
static struct regulator *cam_sensor_a2v8_regulator = NULL; /*LDO 33*/
static struct regulator *cam_sensor_core_1v2_regulator = NULL; /*LDO 37*/
static struct regulator *cam_sensor_io_1v8_regulator = NULL; /*LDO 26*/
static struct regulator *vt_cam_core_1v8_regulator = NULL; // LDO 28

static struct v4l2_mbus_framefmt default_fmt[SR352_OPRMODE_MAX] = {
	[SR352_OPRMODE_VIDEO] = {
		.width		= DEFAULT_SENSOR_WIDTH,
		.height		= DEFAULT_SENSOR_HEIGHT,
		.code		= DEFAULT_SENSOR_CODE,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	[SR352_OPRMODE_IMAGE] = {
		.width		= 1920,
		.height		= 1080,
		.code		= V4L2_MBUS_FMT_JPEG_1X8,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

static const struct sr352_fps sr352_framerates[] = {
	{ I_FPS_0,	FRAME_RATE_AUTO },
	{ I_FPS_1,	FRAME_RATE_BEAUTY_AUTO },
	{ I_FPS_15,	FRAME_RATE_15 },
	{ I_FPS_20,	FRAME_RATE_20 },
	{ I_FPS_30,	FRAME_RATE_30 },
};

static const struct sr352_framesize sr352_preview_frmsizes[] = {
	{ PREVIEW_SZ_CIF,	352, 288 },
	{ PREVIEW_SZ_VGA,	640, 480 },
	{ PREVIEW_SZ_W1024, 1024, 576 },
	{ PREVIEW_SZ_XGA, 1024, 768 },
	{ PREVIEW_SZ_PVGA,	1280, 720 },
};

static const struct sr352_framesize sr352_capture_frmsizes[] = {
	{ CAPTURE_SZ_VGA,	640, 480 },
	{ CAPTURE_SZ_W09MP, 1280, 720 },
	{ CAPTURE_SZ_2MP,	1600, 1200 },
	{ CAPTURE_SZ_W2MP,	2048, 1152 },
	{ CAPTURE_SZ_3MP,	2048, 1536 },
};

static struct sr352_control sr352_ctrls[] = {
	SR352_INIT_CONTROL(V4L2_CID_CAMERA_FLASH_MODE, \
				FLASH_MODE_OFF),

	SR352_INIT_CONTROL(V4L2_CID_CAM_BRIGHTNESS/*V4L2_CID_CAMERA_BRIGHTNESS*/, \
				EV_DEFAULT),

	/* We set default value of metering to invalid value (METERING_BASE),
	 * to update sensor metering as a user set desired metering value. */
	SR352_INIT_CONTROL(V4L2_CID_CAM_METERING/*V4L2_CID_CAMERA_METERING*/, \
				METERING_BASE),

	SR352_INIT_CONTROL(V4L2_CID_WHITE_BALANCE_PRESET/*V4L2_CID_CAMERA_WHITE_BALANCE*/, \
				WHITE_BALANCE_AUTO),

	SR352_INIT_CONTROL(V4L2_CID_IMAGE_EFFECT/*V4L2_CID_CAMERA_EFFECT*/, \
				IMAGE_EFFECT_NONE),
};

static const struct sr352_regs reg_datas = {
	.ev = {
		REGSET(GET_EV_INDEX(EV_MINUS_4), sr352_Brightness_Minus_4),
		REGSET(GET_EV_INDEX(EV_MINUS_3), sr352_Brightness_Minus_3),
		REGSET(GET_EV_INDEX(EV_MINUS_2), sr352_Brightness_Minus_2),
		REGSET(GET_EV_INDEX(EV_MINUS_1), sr352_Brightness_Minus_1),
		REGSET(GET_EV_INDEX(EV_DEFAULT), sr352_Brightness_Default),
		REGSET(GET_EV_INDEX(EV_PLUS_1), sr352_Brightness_Plus_1),
		REGSET(GET_EV_INDEX(EV_PLUS_2), sr352_Brightness_Plus_2),
		REGSET(GET_EV_INDEX(EV_PLUS_3),sr352_Brightness_Plus_3),
		REGSET(GET_EV_INDEX(EV_PLUS_4), sr352_Brightness_Plus_4),
	},
	.metering = {
		REGSET(METERING_MATRIX, sr352_Metering_Matrix),
		REGSET(METERING_CENTER, sr352_Metering_Center),
		REGSET(METERING_SPOT, sr352_Metering_Spot),
	},
	.iso = {
		REGSET(ISO_AUTO, sr352_ISO_Auto),
		REGSET(ISO_100, sr352_ISO_100),
		REGSET(ISO_200, sr352_ISO_200),
		REGSET(ISO_400, sr352_ISO_400),
	},
	.effect = {
		REGSET(IMAGE_EFFECT_NONE, sr352_Effect_Normal),
		REGSET(IMAGE_EFFECT_BNW, sr352_Effect_Black_White),
		REGSET(IMAGE_EFFECT_SEPIA, sr352_Effect_Sepia),
		REGSET(IMAGE_EFFECT_NEGATIVE, sr352_Effect_Solarization),
	},
	.white_balance = {
		REGSET(WHITE_BALANCE_AUTO, sr352_WB_Auto),
		REGSET(WHITE_BALANCE_SUNNY, sr352_WB_Sunny),
		REGSET(WHITE_BALANCE_CLOUDY, sr352_WB_Cloudy),
		REGSET(WHITE_BALANCE_TUNGSTEN, sr352_WB_Tungsten),
		REGSET(WHITE_BALANCE_FLUORESCENT, sr352_WB_Fluorescent),
	},
	.scene_mode = {
		REGSET(SCENE_MODE_NONE, sr352_scene_off),
		REGSET(SCENE_MODE_NIGHTSHOT, sr352_scene_nightshot),
		REGSET(SCENE_MODE_SPORTS, sr352_scene_sports),
	},
	.fps = {
		REGSET(I_FPS_0, sr352_fps_auto),
		REGSET(I_FPS_1, sr352_beautyface_frame20_regs),
		REGSET(I_FPS_15, sr352_fps_15fix),
		REGSET(I_FPS_20, sr352_fps_20fix),
		REGSET(I_FPS_30, sr352_fps_30fix),
	},
	.preview_size = {
		REGSET(PREVIEW_SZ_CIF, sr352_sz_352),
		REGSET(PREVIEW_SZ_VGA, sr352_sz_640),
		REGSET(PREVIEW_SZ_W1024, sr352_preview_sz_w1024),
		REGSET(PREVIEW_SZ_XGA, sr352_preview_sz_1024), /* 1024x768*/
		REGSET(PREVIEW_SZ_PVGA, sr352_cam_HD),
	},

	/* AWB, AE Lock */
	.ae_lock = REGSET_TABLE(sr352_ae_lock_regs),
	.ae_unlock = REGSET_TABLE(sr352_ae_unlock_regs),
	.awb_lock = REGSET_TABLE(sr352_awb_lock_regs),
	.awb_unlock = REGSET_TABLE(sr352_awb_unlock_regs),

	.init = REGSET_TABLE(sr352_init_regs),

	.stream_stop = REGSET_TABLE(sr352_stream_stop_reg),
//	.preview_mode = REGSET_TABLE(sr352_preview_reg),
	.update_preview = REGSET_TABLE(sr352_update_preview_reg),
	.capture_mode = {
		REGSET(CAPTURE_SZ_VGA, sr352_capture_VGA),
		REGSET(CAPTURE_SZ_W09MP, sr352_capture_1280_720),
		REGSET(CAPTURE_SZ_2MP,sr352_capture_1600_1200),
		REGSET(CAPTURE_SZ_W2MP, sr352_capture_W2M),
		REGSET(CAPTURE_SZ_3MP, sr352_capture_3M),
	},
	.camcorder_on = {
		REGSET(PREVIEW_SZ_CIF, sr352_Camcorder_On_EVT1),
		REGSET(PREVIEW_SZ_VGA, sr352_Camcorder_On_EVT1),
		REGSET(PREVIEW_SZ_W1024, sr352_Camcorder_HD_On_EVT1),
		REGSET(PREVIEW_SZ_XGA, sr352_Camcorder_On_EVT1),
		REGSET(PREVIEW_SZ_PVGA, sr352_Camcorder_HD_On_EVT1),
		},
	.camcorder_off = REGSET_TABLE(sr352_Camcorder_Off_EVT1),
};

static const struct v4l2_mbus_framefmt capture_fmts[] = {
	{
		.code		= V4L2_MBUS_FMT_FIXED,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

/**
 * msleep_debug: wrapper function calling proper sleep()
 * @msecs: time to be sleep (in milli-seconds unit)
 * @dbg_on: whether enable log or not.
 */
static void msleep_debug(u32 msecs, bool dbg_on)
{
	u32 delta_halfrange; /* in us unit */

	if (unlikely(!msecs))
		return;

	if (dbg_on)
		cam_dbg("delay for %dms\n", msecs);

	if (msecs <= 7)
		delta_halfrange = 100;
	else
		delta_halfrange = 300;

	if (msecs <= 20)
		usleep_range((msecs * 1000 - delta_halfrange),
			(msecs * 1000 + delta_halfrange));
	else
		msleep(msecs);
}

#if CONFIG_LOAD_FILE
static int loadFile(void)
{
	struct file *fp = NULL;
	struct dentry *dentry;
	struct test *nextBuf = NULL;

	u8 *nBuf = NULL;
	size_t file_size = 0, max_size = 0, testBuf_size = 0;
	ssize_t nread = 0;
	s32 check = 0, starCheck = 0;
	s32 tmp_large_file = 0;
	s32 i = 0;
	int ret = 0;
	loff_t pos;

	mm_segment_t fs = get_fs();
	set_fs(get_ds());

	fp = filp_open(TUNING_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("file open error: %s\n", TUNING_FILE_PATH);
		return PTR_ERR(fp);
	}

	dentry = fp->f_path.dentry;
	file_size = (size_t)dentry->d_inode->i_size;
	max_size = file_size;

	cam_info("loadfile: %s, size %d bytes\n", dentry->d_iname, file_size);

	nBuf = kmalloc(file_size, GFP_KERNEL);
	if (nBuf == NULL) {
		cam_dbg("Fail to 1st get memory\n");
		nBuf = vmalloc(file_size);
		if (nBuf == NULL) {
			cam_err("ERR: nBuf Out of Memory\n");
			ret = -ENOMEM;
			goto error_out;
		}
		tmp_large_file = 1;
	}

	testBuf_size = sizeof(struct test) * file_size;
	if (tmp_large_file) {
		testBuf = (struct test *)vmalloc(testBuf_size);
		large_file = 1;
	} else {
		testBuf = kmalloc(testBuf_size, GFP_ATOMIC);
		if (testBuf == NULL) {
			cam_dbg("Fail to get mem(%d bytes)\n", testBuf_size);
			testBuf = (struct test *)vmalloc(testBuf_size);
			large_file = 1;
		}
	}
	if (testBuf == NULL) {
		cam_err("ERR: Out of Memory\n");
		ret = -ENOMEM;
		goto error_out;
	}

	pos = 0;
	memset(nBuf, 0, file_size);
	memset(testBuf, 0, file_size * sizeof(struct test));

	nread = vfs_read(fp, (char __user *)nBuf, file_size, &pos);
	if (nread != file_size) {
		cam_err("failed to read file ret = %d\n", nread);
		ret = -1;
		goto error_out;
	}

	set_fs(fs);

	i = max_size;

	/*printk(KERN_DEBUG "i = %d\n", i);*/

	while (i) {
		testBuf[max_size - i].data = *nBuf;
		if (i != 1) {
			testBuf[max_size - i].nextBuf =
				&testBuf[max_size - i + 1];
		} else {
			testBuf[max_size - i].nextBuf = NULL;
			break;
		}
		i--;
		nBuf++;
	}

	i = max_size;
	nextBuf = &testBuf[0];

#if 1
	while (i - 1) {
		if (!check && !starCheck) {
			if (testBuf[max_size - i].data == '/') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data
								== '/') {
						check = 1;/* when find '//' */
						i--;
					} else if (
					    testBuf[max_size-i].nextBuf->data
					    == '*') {
						/* when find '/ *' */
						starCheck = 1;
						i--;
					}
				} else
					break;
			}
			if (!check && !starCheck) {
				/* ignore '\t' */
				if (testBuf[max_size - i].data != '\t') {
					nextBuf->nextBuf = &testBuf[max_size-i];
					nextBuf = &testBuf[max_size - i];
				}
			}
		} else if (check && !starCheck) {
			if (testBuf[max_size - i].data == '/') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data
					    == '*') {
						/* when find '/ *' */
						starCheck = 1;
						check = 0;
						i--;
					}
				} else
					break;
			}

			 /* when find '\n' */
			if (testBuf[max_size - i].data == '\n' && check) {
				check = 0;
				nextBuf->nextBuf = &testBuf[max_size - i];
				nextBuf = &testBuf[max_size - i];
			}

		} else if (!check && starCheck) {
			if (testBuf[max_size - i].data == '*') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data
					    == '/') {
						/* when find '* /' */
						starCheck = 0;
						i--;
					}
				} else
					break;
			}
		}

		i--;

		if (i < 2) {
			nextBuf = NULL;
			break;
		}

		if (testBuf[max_size - i].nextBuf == NULL) {
			nextBuf = NULL;
			break;
		}
	}
#endif

	cam_info("loadfile: loading %s completed!\n", dentry->d_iname);

#if 0 /* for print */
	cam_dbg("i = %d\n", i);
	nextBuf = &testBuf[0];
	while (1) {
		if (nextBuf->nextBuf == NULL)
			break;
		cam_dbg("%c", nextBuf->data);
		nextBuf = nextBuf->nextBuf;
	}
#endif

error_out:
	tmp_large_file ? vfree(nBuf) : kfree(nBuf);

	if (fp)
		filp_close(fp, current->files);
	return ret;
}


#endif

/**
 * sr352_i2c_read: Read 2 bytes from sensor
 */
static int sr352_i2c_read(struct i2c_client *client,
				  u8 subaddr, u8 *data)
{
	int retries = I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	u8 buf[1];
	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = sizeof(subaddr);
	msg[0].buf = (u8 *)&subaddr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	do {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (likely(ret == 2))
			break;

		msleep_debug(POLL_TIME_MS, false);
		err = ret;
		stats_i2c_err++;
	} while (--retries > 0);

	/* Retry occured */
	if (unlikely(retries < I2C_RETRY_COUNT)) {
		cam_err("i2c_read: error %d, read 0x%04X, retry %d\n",
				err, subaddr, I2C_RETRY_COUNT - retries);
	}

	CHECK_ERR_COND_MSG(ret != 2, -EIO, "I2C does not work\n");

	*data = buf[0];
	return 0;
}

/**
 * sr352_i2c_write: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int sr352_i2c_write(struct i2c_client *client,
					 u8 addr, u8 data)
{
	int retries = I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	u8 buf[2] = {0,};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};

	buf[0] = addr;
	buf[1] = data;

#if (0)
	sr352_debug(SR352_DEBUG_I2C, "%s : W(0x%02X%02X)\n",
		__func__, buf[0], buf[1]);
#else
	/* cam_dbg("I2C writing: 0x%02X%02X\n",	buf[0], buf[1]); */
#endif

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;

		msleep_debug(POLL_TIME_MS, false);
		err = ret;
		stats_i2c_err++;
	} while (--retries > 0);

	/* Retry occured */
	if (unlikely(retries < I2C_RETRY_COUNT)) {
		cam_err("i2c_write: error %d, write (%02X, %02X), retry %d\n",
			err, addr, data, I2C_RETRY_COUNT - retries);
	}

	CHECK_ERR_COND_MSG(ret != 1, -EIO, "I2C does not work\n\n");
	return 0;
}

#if CONFIG_LOAD_FILE
static int sr352_write_regs_from_sd(struct v4l2_subdev *sd,
						const u8 s_name[])
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct test *tempData = NULL;

	int ret = 0;
	u32 temp;
	u32 delay = 0;
	u8 data[7];
	s32 searched = 0;
	size_t size = strlen(s_name);
	s32 i;
#if DEBUG_WRITE_REGS
	u8 regs_name[128] = {0,};

	BUG_ON(size > sizeof(regs_name));
#endif

	/* cam_dbg("%s: E size = %d, string = %s\n", __func__, size, s_name);*/
	tempData = &testBuf[0];
	while (!searched) {
		searched = 1;
		for (i = 0; i < size; i++) {
			if (tempData->data != s_name[i]) {
				searched = 0;
				break;
			}
#if DEBUG_WRITE_REGS
			regs_name[i] = tempData->data;
#endif
			tempData = tempData->nextBuf;
		}
#if DEBUG_WRITE_REGS
		if (i > 9) {
			regs_name[i] = '\0';
			/*cam_dbg("Searching: regs_name = %s\n", regs_name);*/
		}
#endif
		tempData = tempData->nextBuf;
	}
	/* structure is get..*/
#if DEBUG_WRITE_REGS
	regs_name[i] = '\0';
	if (strcmp(s_name, regs_name)) {
		cam_err("error, Regs (%s) not found. Searched regs = %s\n\n",
			s_name, regs_name);
		return -ESRCH;
	} else
		cam_info("Searched regs: %s\n", regs_name);
#endif

	while (1) {
		if (tempData->data == '{')
			break;
		else
			tempData = tempData->nextBuf;
	}

	while (1) {
		searched = 0;
		while (1) {
			if (tempData->data == 'x') {
				/* get 6 strings.*/
				data[0] = '0';
				for (i = 1; i < 7; i++) {
					data[i] = tempData->data;
					tempData = tempData->nextBuf;
				}
				/*cam_dbg("%s\n", data);*/
				temp = simple_strtoul(data, NULL, 16);
				break;
			} else if (tempData->data == '}') {
				searched = 1;
				break;
			} else
				tempData = tempData->nextBuf;

			if (tempData->nextBuf == NULL)
				return -1;
		}

		if (searched)
			break;

		if ((temp & SR352_DELAY) == SR352_DELAY) {
			delay = temp & 0x00FF;
			if (delay != 0xFF)
				msleep_debug(delay, true);
			continue;
		}

		/* cam_dbg("I2C writing: 0x%08X,\n",temp);*/
		ret = sr352_i2c_write(client,
			(temp >> 8), (u16)temp);
		CHECK_ERR_MSG(ret, "write registers\n")
	}

	return ret;
}
#else /* !CONFIG_LOAD_FILE */
/* Write register
 * If success, return value: 0
 * If fail, return value: -EIO
 */
/*
static int sr352_write_regs(struct v4l2_subdev *sd, const u16 regs[],
			     int size)
{
	struct sr352_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 delay = 0;
	int i, err = 0;

	mutex_lock(&state->i2c_lock);

	for (i = 0; i < size; i++) {
		if ((regs[i] & SR352_DELAY) == SR352_DELAY) {
			delay = regs[i] & 0xFF;
			if (delay != 0xFF)
				msleep_debug(delay*10, true);

			continue;
		}

		err = sr352_i2c_write(client,
			(u8)(regs[i] >> 8), (u8)regs[i]);
		CHECK_ERR_GOTO_MSG(err, out, "write registers. i %d\n", i);
	}

out:
	mutex_unlock(&state->i2c_lock);
	return err;
}*/

#define BURST_MODE_BUFFER_MAX_SIZE 255
u8 sr352_burstmode_buf[BURST_MODE_BUFFER_MAX_SIZE];

static int sr352_burst_write_regs(struct v4l2_subdev *sd,
			const u16 regs[], u16 size, const char *name)
{
	struct sr352_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;
	int burst_flag = 0;
	int idx = 0;
	int retries = I2C_RETRY_COUNT;
	u16 delay = 0;
	u8 sr352_singlemode_buf[2] = {0,};
	int ret = 0;
	int i = 0;

	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
	};

	cam_trace("E, size = %d, name = %s\n", size, name);

	client->adapter->retries = 1;

	mutex_lock(&state->i2c_lock);

	for (i = 0; i < size; i++) {
		if (unlikely(idx > (BURST_MODE_BUFFER_MAX_SIZE - 10))) {
			cam_err("BURST MOD buffer overflow! idx %d\n", idx);
			err = -ENOBUFS;
			goto out;
		}
		if (burst_flag == 0) {
			if (regs[i] == SR352_BURST_START) {  //BURST_START
				// set the burst flag since we encountered the burst start reg
				burst_flag = 1;
				continue;
			}

			if ((regs[i] & SR352_DELAY) == SR352_DELAY) {
				delay = regs[i] & 0xFF;
				if (delay != 0xFF)
					msleep_debug(delay*10, true);
				continue;
			}

			// reset the byte count
			idx = 0;

			// write the register setting
			sr352_singlemode_buf[0] = (u8)(regs[i] >> 8);
			sr352_singlemode_buf[1] = (u8)regs[i];

			msg.len = 2;
			msg.buf = sr352_singlemode_buf;

			do {
				ret = i2c_transfer(client->adapter, &msg, 1);
				if (likely(ret == 1))
					break;

				msleep_debug(POLL_TIME_MS, false);
				err = ret;
				stats_i2c_err++;
			} while (--retries > 0);

			/* Retry occured */
			if (unlikely(retries < I2C_RETRY_COUNT)) {
				cam_err("i2c_write: error %d, write (%04X), retry %d\n",
					err, regs[i], I2C_RETRY_COUNT - retries);
			}

			if (unlikely(ret != 1)) {
				cam_err("error, I2C does not work\n");
				break;
			}

		} else if (burst_flag == 1) {
			if (regs[i] == SR352_BURST_STOP) {  //BURST_STOP
				// End Burst flag encountered

				msg.len = idx;
				msg.buf = sr352_burstmode_buf;

				do {
					ret = i2c_transfer(client->adapter, &msg, 1);
					if (likely(ret == 1))
						break;

					msleep_debug(POLL_TIME_MS, false);
					err = ret;
					stats_i2c_err++;
				} while (--retries > 0);

				/* Retry occured */
				if (unlikely(retries < I2C_RETRY_COUNT)) {
					cam_err("i2c_write: error %d, write (%04X), retry %d\n",
						err, regs[i], I2C_RETRY_COUNT - retries);
				}

				if (unlikely(ret != 1)) {
					cam_err("error, I2C does not work\n");
					break;
				}

				// reset the byte count and the flags
				idx = 0;
				burst_flag = 0;

			} else{
				if (idx == 0) {
					// Zeroth byte in the burst data
					sr352_burstmode_buf[idx++] = (u8)(regs[i] >> 8);
					sr352_burstmode_buf[idx++] = (u8)regs[i];
				} else {
					sr352_burstmode_buf[idx++] = (u8)regs[i];
				}
			}
		}
	}
	cam_trace("X\n");
out:
	mutex_unlock(&state->i2c_lock);
	return err;
}
#endif /* CONFIG_LOAD_FILE */

static int sr352_set_from_table(struct v4l2_subdev *sd,
				const char *setting_name,
				const struct regset_table *table,
				u32 table_size, s32 index)
{
	int err = 0;

	CHECK_ERR_COND_MSG(((index < 0) || (index >= table_size)),
		-EINVAL, "index(%d) out of range[0:%d] for table for %s\n",
		index, table_size, setting_name);

	table += index;

#if CONFIG_LOAD_FILE
	CHECK_ERR_COND_MSG(!table->name, -EFAULT, \
		"table=%s, index=%d, reg_name = NULL\n", setting_name, index);

	cam_dbg("%s: \"%s\", reg_name=%s\n", __func__,
			setting_name, table->name);

	err = sr352_write_regs_from_sd(sd, table->name);
	CHECK_ERR_MSG(err, "write regs(%s), err=%d\n", setting_name, err);

	return 0;

#else /* !CONFIG_LOAD_FILE */
	CHECK_ERR_COND_MSG(!table->reg, -EFAULT, \
		"table=%s, index=%d, reg = NULL\n", setting_name, index);

#if DEBUG_WRITE_REGS
	cam_dbg("%s: \"%s\", reg_name=%s\n", __func__,
			setting_name, table->name);
#endif

	err = sr352_burst_write_regs(sd, table->reg, table->array_size, table->name);
	CHECK_ERR_MSG(err, "write regs(%s), err=%d\n", setting_name, err);

	return 0;
#endif /* CONFIG_LOAD_FILE */
}

static inline int sr352_save_ctrl(struct v4l2_subdev *sd,
					struct v4l2_control *ctrl)
{
	int i, ctrl_cnt = ARRAY_SIZE(sr352_ctrls);

	cam_trace("ID =0x%08X, val = %d\n", ctrl->id, ctrl->value);

	for (i = 0; i < ctrl_cnt; i++) {
		if (ctrl->id == sr352_ctrls[i].id) {
			sr352_ctrls[i].value = ctrl->value;
			return 0;
		}
	}

	cam_trace("not saved, ID %d(0x%X)\n", ctrl->id & 0xFF, ctrl->id);
	return 0;
}

static int sr352_put_power(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);

	if (SR352_HW_POWER_ON == state->power_on) {
		cam_warn_on("put_power: error, still in power-on!!\n");
		return -EPERM;
	}

	state->power_on = SR352_HW_POWER_NOT_READY;

	return 0;
}

static int sr352_get_power(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int ret = 0;

	state->power_on = SR352_HW_POWER_READY;
	return ret;
}

static inline int sr352_power_on(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	cam_dbg("power-on(new)\n");

	if (gpio_request(GPIO_CAM_VT_nSTBY, "GPM3_6") < 0) {
		pr_err("failed gpio_request(GPM3_6) for camera control\n");
		err = -1;
		goto err_gpio;
	}
	if (gpio_request(GPIO_CAM_VT_nRST, "GPM1_2") < 0) {
		pr_err("failed gpio_request(GPM1_2) for camera control\n");
		err = -1;
		goto err_gpio;
	}
	if (gpio_request(GPIO_CAM_3M_RST, "GPX1_2") < 0) {
		pr_err("failed gpio_request(GPX1_2) for camera control\n");
		err = -1;
		goto err_gpio;
	}
	if (gpio_request(GPIO_CAM_3M_STBY, "GPM3_4") < 0) {
		pr_err("failed gpio_request(GPM3_4) for camera control\n");
		err = -1;
		goto err_gpio;
	}
	if (gpio_request(GPIO_CAM_MCLK, "GPM2_2") < 0) {
		pr_err("failed gpio_request(GPM2_2) for camera control\n");
		err = -1;
		goto err_gpio;
	}

	cam_sensor_io_1v8_regulator = regulator_get(NULL, "main_cam_io_1v8");
	if (IS_ERR(cam_sensor_io_1v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_io_1v8");
		err = -ENODEV;
		goto err_regulator;
	}
	cam_sensor_core_1v2_regulator = regulator_get(NULL, "main_cam_core_1v2");
	if (IS_ERR(cam_sensor_core_1v2_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_core_1v2");
		err = -ENODEV;
		goto err_regulator;
	}
	cam_sensor_a2v8_regulator = regulator_get(NULL, "main_cam_sensor_a2v8");
	if (IS_ERR(cam_sensor_a2v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_sensor_a2v8");
		err = -ENODEV;
		goto err_regulator;
	}
	vt_cam_core_1v8_regulator = regulator_get(NULL, "vt_cam_core_1v8");
	if (IS_ERR(vt_cam_core_1v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "vt_cam_core_1v8");
		err = -ENODEV;
		goto err_regulator;
	}
	gpio_direction_output(GPIO_CAM_VT_nRST, 0);
	gpio_direction_output(GPIO_CAM_VT_nSTBY, 0);

	/* Sensor I/O 1.8v */
	err = regulator_enable(cam_sensor_io_1v8_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		err = -ENODEV;
		goto err_regulator;
	}
	udelay(100);
	/* Sensor AVDD 2.8v */
	err = regulator_enable(cam_sensor_a2v8_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		err = -ENODEV;
		goto err_regulator;
	}
	udelay(100);
	/* VT Core 1.8v */
	err = regulator_enable(vt_cam_core_1v8_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		err = -ENODEV;
		goto err_regulator;
	}
	udelay(100);
	/* Sensor Core 1.2v */
	err = regulator_enable(cam_sensor_core_1v2_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		err = -ENODEV;
		goto err_regulator;
	}
	msleep(3);
	/* MCLK On */
	cam_mipi_en = true;
#if 0
	err = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(3));//MCLK
	if (unlikely(err)) {
		pr_err("%s: fail to enable MCLK (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_NONE);
#else
	clk_enable(state->mclk);
#endif
	/* STBY */
	gpio_direction_output(GPIO_CAM_3M_STBY, 1);
	msleep(11);
	/* RST high */
	gpio_direction_output(GPIO_CAM_3M_RST, 1);
	udelay(1500);

err_regulator:
	regulator_put(cam_sensor_a2v8_regulator);
	regulator_put(cam_sensor_core_1v2_regulator);
	regulator_put(cam_sensor_io_1v8_regulator);
	regulator_put(vt_cam_core_1v8_regulator);

err_gpio:
	gpio_free(GPIO_CAM_MCLK);
	gpio_free(GPIO_CAM_3M_STBY);
	gpio_free(GPIO_CAM_3M_RST);
	gpio_free(GPIO_CAM_VT_nRST);
	gpio_free(GPIO_CAM_VT_nSTBY);

	return err;
}

static inline int sr352_power_off(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	cam_dbg("power-off(new)\n");

	if (gpio_request(GPIO_CAM_VT_nSTBY, "GPM3_6") < 0) {
		pr_err("failed gpio_request(GPM3_6) for camera control\n");
		err = -1;
		goto err_gpio;
	}
	if (gpio_request(GPIO_CAM_VT_nRST, "GPM1_2") < 0) {
		pr_err("failed gpio_request(GPM1_2) for camera control\n");
		err = -1;
		goto err_gpio;
	}
	if (gpio_request(GPIO_CAM_3M_RST, "GPX1_2") < 0) {
		pr_err("failed gpio_request(GPX1_2) for camera control\n");
		err = -1;
		goto err_gpio;
	}

	if (gpio_request(GPIO_CAM_3M_STBY, "GPM3_4") < 0) {
		pr_err("failed gpio_request(GPM3_4) for camera control\n");
		err = -1;
		goto err_gpio;
	}
	if (gpio_request(GPIO_CAM_MCLK, "GPM2_2") < 0) {
		pr_err("failed gpio_request(GPM2_2) for camera control\n");
		err = -1;
		goto err_gpio;
	}

	cam_sensor_io_1v8_regulator = regulator_get(NULL, "main_cam_io_1v8");
	if (IS_ERR(cam_sensor_io_1v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_io_1v8");
		err = -ENODEV;
		goto err_regulator;
	}
	cam_sensor_core_1v2_regulator = regulator_get(NULL, "main_cam_core_1v2");
	if (IS_ERR(cam_sensor_core_1v2_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_core_1v2");
		err = -ENODEV;
		goto err_regulator;
	}
	cam_sensor_a2v8_regulator = regulator_get(NULL, "main_cam_sensor_a2v8");
	if (IS_ERR(cam_sensor_a2v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_sensor_a2v8");
		err = -ENODEV;
		goto err_regulator;
	}
	vt_cam_core_1v8_regulator = regulator_get(NULL, "vt_cam_core_1v8");
	if (IS_ERR(vt_cam_core_1v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "vt_cam_core_1v8");
		err = -ENODEV;
		goto err_regulator;
	}

	gpio_direction_output(GPIO_CAM_VT_nRST, 0);
	gpio_direction_output(GPIO_CAM_VT_nSTBY, 0);

	udelay(1500);

	/* RST high */
	gpio_direction_output(GPIO_CAM_3M_RST, 0);
	udelay(15);

	/* MCLK off */
#if 0
	err = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_DOWN);
	if (unlikely(err)) {
		printk("%s: fail to enable MCLK (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);
#else
	clk_disable(state->mclk);
	udelay(10);
#endif

	/* STBY low*/
	gpio_direction_output(GPIO_CAM_3M_STBY, 0);
	udelay(10);

	/* Sensor Core 1.2v */
	err = regulator_disable(cam_sensor_core_1v2_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		err = -ENODEV;
		goto err_regulator;
	}
	udelay(100);
	/* VT Sensor Core 1.8v */
	err = regulator_disable(vt_cam_core_1v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		err = -ENODEV;
		goto err_regulator;
	}
	udelay(100);
	/* Sensor A2.8v */
	err = regulator_disable(cam_sensor_a2v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		err = -ENODEV;
		goto err_regulator;
	}
	udelay(100);
	/* Sensor I/O 1.8v*/
	err = regulator_disable(cam_sensor_io_1v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		err = -ENODEV;
		goto err_regulator;
	}
	udelay(10);

err_regulator:
	regulator_put(cam_sensor_a2v8_regulator);
	regulator_put(cam_sensor_core_1v2_regulator);
	regulator_put(cam_sensor_io_1v8_regulator);
	regulator_put(vt_cam_core_1v8_regulator);

err_gpio:
	gpio_free(GPIO_CAM_MCLK);
	gpio_free(GPIO_CAM_3M_STBY);
	gpio_free(GPIO_CAM_3M_RST);
	gpio_free(GPIO_CAM_VT_nRST);
	gpio_free(GPIO_CAM_VT_nSTBY);

	return err;
}

static int sr352_power(struct v4l2_subdev *sd, int on)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	cam_info("power %s\n", on ? "on" : "off");

	CHECK_ERR_COND_MSG((SR352_HW_POWER_NOT_READY == state->power_on),
			-EPERM, "power %d not ready\n", on)

	if (on) {
		if (SR352_HW_POWER_ON == state->power_on)
			cam_err("power already on\n");

		err = sr352_power_on(sd);
		CHECK_ERR_MSG(err, "fail to power on %d\n", err);

		state->power_on = SR352_HW_POWER_ON;
	} else {
		if (SR352_HW_POWER_OFF == state->power_on)
			cam_info("power already off\n");
		else {
			err = sr352_power_off(sd);
			CHECK_ERR_MSG(err, "fail to power off %d\n", err);
		}
		state->power_on = SR352_HW_POWER_OFF;
		state->initialized = 0;
	}
	return 0;
}

static inline int __used
sr352_transit_preview_mode(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	cam_dbg("sr352_transit_preview_mode E\n");
	err = sr352_set_from_table(sd, "preview_mode",
			&state->regs->preview_mode, 1, 0);
	CHECK_ERR(err);

	state->need_preview_update = 0;
	cam_dbg("sr352_transit_preview_mode X\n");
	return 0;
}

/**
 * sr352_transit_half_mode: go to a half-release mode
 *
 * Don't forget that half mode is not used in movie mode.
 */
static inline int __used
sr352_transit_half_mode(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);

	/* We do not go to half release mode in movie mode */
	if (state->sensor_mode == SENSOR_MOVIE)
		return 0;

	/*
	 * Write the additional codes here
	 */
	return 0;
}

static inline int __used
sr352_transit_capture_mode(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);

	return sr352_set_from_table(sd, "capture_mode",
			state->regs->capture_mode,
			ARRAY_SIZE(state->regs->capture_mode),
			state->capture.frmsize->index);
}

static int sr352_set_scene_mode(struct v4l2_subdev *sd, s32 val)
{
	struct sr352_state *state = to_state(sd);
	int err = -ENODEV;

	cam_trace("E, value %d\n", val);

	if (state->scene_mode == val)
		return 0;

	/* when scene mode is switched, we frist have to reset */
	if (val > SCENE_MODE_NONE) {
		err = sr352_set_from_table(sd, "scene_mode",
				state->regs->scene_mode,
				ARRAY_SIZE(state->regs->scene_mode),
				SCENE_MODE_NONE);
	}

	switch (val) {
	case SCENE_MODE_NONE:
		sr352_set_from_table(sd, "scene_mode",
			state->regs->scene_mode,
			ARRAY_SIZE(state->regs->scene_mode), val);
			break;
		/* do not break */
	case SCENE_MODE_PORTRAIT:
	case SCENE_MODE_NIGHTSHOT:
	case SCENE_MODE_BACK_LIGHT:
	case SCENE_MODE_LANDSCAPE:
	case SCENE_MODE_SPORTS:
	case SCENE_MODE_PARTY_INDOOR:
	case SCENE_MODE_BEACH_SNOW:
	case SCENE_MODE_SUNSET:
	case SCENE_MODE_DUSK_DAWN:
	case SCENE_MODE_FALL_COLOR:
	case SCENE_MODE_TEXT:
	case SCENE_MODE_CANDLE_LIGHT:
		sr352_set_from_table(sd, "scene_mode",
			state->regs->scene_mode,
			ARRAY_SIZE(state->regs->scene_mode), val);
		break;

	default:
		cam_dbg("set_scene: value is (%d)\n", val);
		return 0;
	}

	state->scene_mode = val;

	cam_trace("X\n");
	return 0;
}

static int sr352_set_metering(struct v4l2_subdev *sd, s32 val)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

retry:
	switch (val) {
	case METERING_MATRIX:
	case METERING_CENTER:
	case METERING_SPOT:
		err = sr352_set_from_table(sd, "metering",
			state->regs->metering,
			ARRAY_SIZE(state->regs->metering), val);
		CHECK_ERR_MSG(err, "set_metering: failed to write\n");
		break;

	default:
		cam_err("set_metering: not supported metering %d\n", val);
		val = METERING_MATRIX;
		goto retry;
	}

	state->need_preview_update = 1;
	return 0;
}

/* PX: Set brightness */
static int sr352_set_exposure(struct v4l2_subdev *sd, s32 val)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;
	cam_info("%s: val = %d\n", __func__, val);

	if (val < EV_MINUS_4)
		val = EV_MINUS_4;
	else if (val > EV_PLUS_4)
		val = EV_PLUS_4;

	err = sr352_set_from_table(sd, "brightness", state->regs->ev,
		ARRAY_SIZE(state->regs->ev), GET_EV_INDEX(val));

	state->exposure.val = val;

	return err;
}

static int sr352_set_effect(struct v4l2_subdev *sd, s32 val)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;
	cam_info("%s: val = %d, effectValue : %d\n", __func__, val, state->effectValue);

retry:
	switch(val) {
	case IMAGE_EFFECT_NONE:
	case IMAGE_EFFECT_BNW:
	case IMAGE_EFFECT_SEPIA:
	case IMAGE_EFFECT_NEGATIVE:
		err = sr352_set_from_table(sd, "effects", state->regs->effect,
			ARRAY_SIZE(state->regs->effect), val);
		break;
	default:
		cam_err("set_effect: error, not supported (%d)\n", val);
		val = IMAGE_EFFECT_NONE;
		goto retry;
	}

	state->effectValue = val;

	return err;
}

/**
 * sr352_set_whitebalance - set whilebalance
 * @val:
 */
static int sr352_set_whitebalance(struct v4l2_subdev *sd, s32 val)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	cam_trace("E, value %d\n", val);

retry:
	switch (val) {
	case WHITE_BALANCE_AUTO:
	case WHITE_BALANCE_SUNNY:
	case WHITE_BALANCE_CLOUDY:
	case WHITE_BALANCE_TUNGSTEN:
	case WHITE_BALANCE_FLUORESCENT:
		err = sr352_set_from_table(sd, "white balance",
				state->regs->white_balance,
				ARRAY_SIZE(state->regs->white_balance),
				val);
		break;

	default:
		cam_err("set_wb: error, not supported (%d)\n", val);
		val = WHITE_BALANCE_AUTO;
		goto retry;
	}

	state->wb.mode = val;

	cam_trace("X\n");
	return err;
}

/* PX: Set sensor mode */
static int sr352_set_sensor_mode(struct v4l2_subdev *sd, s32 val)
{
	struct sr352_state *state = to_state(sd);

	cam_dbg("sensor_mode: %d\n", val);

	switch (val) {
	case SENSOR_MOVIE:
		/* We does not support movie mode when in VT. */
		if (state->vt_mode) {
			state->sensor_mode = SENSOR_CAMERA;
			cam_err("%s: error, not support movie\n", __func__);
			break;
		}
		/* We do not break. */

	case SENSOR_CAMERA:
		state->sensor_mode = val;
		break;

	default:
		cam_err("%s: error, not support.(%d)\n", __func__, val);
		state->sensor_mode = SENSOR_CAMERA;
		WARN_ON(1);
		break;
	}

	return 0;
}

static int sr352_set_aeawb_lock_unlock(struct v4l2_subdev *sd, s32 lock)
{
	int err = -EIO;

	cam_dbg("sr352_set_aeawb_lock_unlock - lock : %d\n", lock);

	switch(lock){
	case V4L2_AE_UNLOCK_AWB_UNLOCK:
		err = SR352_BURST_WRITE_REGS(sd, sr352_ae_unlock_regs);
		CHECK_ERR_MSG(err, "failed to write sr352_ae_unlock_regs\n");
		err = SR352_BURST_WRITE_REGS(sd, sr352_awb_unlock_regs);
		CHECK_ERR_MSG(err, "failed to write sr352_awb_unlock_regs\n");
		break;

	case V4L2_AE_LOCK_AWB_LOCK:
		err = SR352_BURST_WRITE_REGS(sd, sr352_ae_lock_regs);
		CHECK_ERR_MSG(err, "failed to write sr352_ae_lock_regs\n");
		err = SR352_BURST_WRITE_REGS(sd, sr352_awb_lock_regs);
		CHECK_ERR_MSG(err, "failed to write sr352_awb_lock_regs\n");
		break;

	default:
		cam_err("%s: error, not support.(%d)\n", __func__, lock);
		break;
	}

	return 0;
}

static int sr352_set_frame_rate(struct v4l2_subdev *sd, s32 fps)
{
	struct sr352_state *state = to_state(sd);
	int i = 0, fps_index = -1;
	int min = FRAME_RATE_AUTO;
	int max = FRAME_RATE_30;
	int err = -EIO;

	if (state->scene_mode != SCENE_MODE_NONE)
		return 0;

	cam_info("set frame rate %d, current fps : %d\n", fps, state->fps);

	if ((fps < min) || (fps > max)) {
		cam_err("set_frame_rate: error, invalid frame rate %d\n", fps);
		fps = (fps < min) ? min : max;
	}

	if (fps == state->fps)
		return 0;

	if (unlikely(!state->initialized)) {
		cam_dbg("pending fps %d\n", fps);
		state->req_fps = fps;
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(sr352_framerates); i++) {
		if (fps == sr352_framerates[i].fps) {
			fps_index = sr352_framerates[i].index;
			state->fps = fps;
			state->req_fps = -1;
			break;
		}
	}

	if (unlikely(fps_index < 0)) {
		cam_err("set_frame_rate: not supported fps %d\n", fps);
		return 0;
	}

	err = sr352_set_from_table(sd, "fps", state->regs->fps,
			ARRAY_SIZE(state->regs->fps), fps_index);
	CHECK_ERR_MSG(err, "fail to set framerate\n");

	return 0;
}


/**
 * sr352_transit_movie_mode: switch camera mode if needed.
 *
 * Note that this fuction should be called from start_preview().
 */
static inline int __used
sr352_transit_movie_mode(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	/* we'll go from the below modes to RUNNING or RECORDING */
	switch (state->runmode) {
	case RUNMODE_INIT:
		/* case of entering camcorder firstly */

	case RUNMODE_RUNNING_STOP:
		/* case of switching from camera to camcorder */
		if (state->sensor_mode != SENSOR_MOVIE)
			break;

		cam_dbg("switching to recording mode\n");
/*		err = sr352_set_from_table(sd, "camcorder_on",
				state->regs->camcorder_on,
				ARRAY_SIZE(state->regs->camcorder_on),
				state->preview.frmsize->index);
		CHECK_ERR(err);
		sr352_restore_ctrl(sd);
*/
		if (!(state->hd_videomode)) { // HDmode<->normalmode
			err = SR352_BURST_WRITE_REGS(sd, sr352_fps_30fix);
			CHECK_ERR(err);
			state->fps = FRAME_RATE_30;
		}
		err = sr352_set_metering(sd, METERING_MATRIX);
		CHECK_ERR(err);
		err = sr352_set_effect(sd, state->effectValue);
		CHECK_ERR(err);
		err = sr352_set_whitebalance(sd, state->wb.mode);
		CHECK_ERR(err);
		err = sr352_set_exposure(sd, state->exposure.val);
		CHECK_ERR(err);
		break;

	case RUNMODE_RECORDING_STOP:
		/* case of switching from camcorder to camera */
		if (SENSOR_CAMERA == state->sensor_mode) {
			cam_dbg("switching to camera mode\n");
/*			cam_dbg("switching to camera mode\n");
			err = sr352_set_from_table(sd, "camcorder_off",
				&state->regs->camcorder_off, 1, 0);
			CHECK_ERR(err);
*/
			sr352_restore_ctrl(sd);
		} else if (state->sensor_mode == SENSOR_MOVIE) {
			cam_dbg("switching size in recording mode\n");
			if ( state->hd_videomode != state->previousHDmode) {
				if (!(state->hd_videomode)) {
					err = SR352_BURST_WRITE_REGS(sd, sr352_fps_30fix);
					CHECK_ERR(err);
					state->fps = FRAME_RATE_30;
				}
				err = sr352_set_metering(sd, METERING_MATRIX);
				CHECK_ERR(err);
				err = sr352_set_effect(sd, state->effectValue);
				CHECK_ERR(err);
				err = sr352_set_whitebalance(sd, state->wb.mode);
				CHECK_ERR(err);
				err = sr352_set_exposure(sd, state->exposure.val);
				CHECK_ERR(err);
			}
		}
		break;

	default:
		break;
	}

	return 0;
}


static const struct sr352_framesize * __used
sr352_get_framesize_i(const struct sr352_framesize *frmsizes,
				u32 frmsize_count, u32 index)
{
	int i = 0;

	for (i = 0; i < frmsize_count; i++) {
		if (frmsizes[i].index == index)
			return &frmsizes[i];
	}

	return NULL;
}

static const struct sr352_framesize * __used
sr352_get_framesize_sz(const struct sr352_framesize *frmsizes,
				u32 frmsize_count, u32 width, u32 height)
{
	int i;

	for (i = 0; i < frmsize_count; i++) {
		if ((frmsizes[i].width == width) && (frmsizes[i].height == height))
			return &frmsizes[i];
	}

	return NULL;
}

static const struct sr352_framesize * __used
sr352_get_framesize_ratio(const struct sr352_framesize *frmsizes,
				u32 frmsize_count, u32 width, u32 height)
{
	int i = 0;
	int found = -ENOENT;
	const int ratio = FRM_RATIO(width, height);

	for (i = 0; i < frmsize_count; i++) {
		if ((frmsizes[i].width == width) && (frmsizes[i].height == height))
			return &frmsizes[i];

		if (FRAMESIZE_RATIO(&frmsizes[i]) == ratio) {
			if ((-ENOENT == found) ||
			    (frmsizes[found].width < frmsizes[i].width))
				found = i;
		}
	}

	if (found != -ENOENT) {
		cam_dbg("get_framesize: %dx%d -> %dx%d\n", width, height,
			frmsizes[found].width, frmsizes[found].height);
		return &frmsizes[found];
	} else
		return NULL;
}

/* This function is called from the g_ctrl api
 *
 * This function should be called only after the s_fmt call,
 * which sets the required width/height value.
 *
 * It checks a list of available frame sizes and sets the
 * most appropriate frame size.
 *
 * The list is stored in an increasing order (as far as possible).
 * Hence the first entry (searching from the beginning) where both the
 * width and height is more than the required value is returned.
 * In case of no perfect match, we set the last entry (which is supposed
 * to be the largest resolution supported.)
 */
static void sr352_set_framesize(struct v4l2_subdev *sd,
				const struct sr352_framesize *frmsizes,
				u32 num_frmsize, bool preview)
{
	struct sr352_state *state = to_state(sd);
	const struct sr352_framesize **found_frmsize = NULL;
	u32 width = state->req_fmt.width;
	u32 height = state->req_fmt.height;

	cam_dbg("set_framesize: Requested Res %dx%d\n", width, height);

	found_frmsize = /*(const struct sr352_framesize **) */
			(preview ? &state->preview.frmsize: &state->capture.frmsize);

	*found_frmsize = sr352_get_framesize_ratio(
			frmsizes, num_frmsize, width, height);

	if (*found_frmsize == NULL) {
		cam_err("%s: error, invalid frame size %dx%d\n",
			__func__, width, height);
		*found_frmsize = preview ?
			sr352_get_framesize_i(frmsizes, num_frmsize,
				(state->sensor_mode == SENSOR_CAMERA) ?
				PREVIEW_SZ_XGA: PREVIEW_SZ_W1024 ):
			sr352_get_framesize_i(frmsizes, num_frmsize,
					CAPTURE_SZ_3MP);
		BUG_ON(!(*found_frmsize));
	}

	if (preview) {
		cam_info("Preview Res Set: %dx%d, index %d\n",
			(*found_frmsize)->width, (*found_frmsize)->height,
			(*found_frmsize)->index);
	} else {
		cam_info("Capture Res Set: %dx%d, index %d\n",
			(*found_frmsize)->width, (*found_frmsize)->height,
			(*found_frmsize)->index);
	}
}

static int sr352_control_stream(struct v4l2_subdev *sd, u32 cmd)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;

	if (!state->pdata->common.is_mipi)
		return 0;

	if (unlikely(cmd != STREAM_STOP))
		return 0;

	if (!((RUNMODE_RUNNING == state->runmode) && state->capture.pre_req)) {
		if (state->fps == FRAME_RATE_BEAUTY_AUTO) {
			err = SR352_BURST_WRITE_REGS(sd, sr352_beautyface_frameoff_regs);
			CHECK_ERR_MSG(err, "failed to write sr352_beautyface_frameoff_regs\n");
		} else if (state->fps == FRAME_RATE_20) {
			err = SR352_BURST_WRITE_REGS(sd, sr352_fps_auto);
			CHECK_ERR_MSG(err, "failed to write sr352_fps_auto\n");
		}
		cam_info("STREAM STOP!!\n");
		err = sr352_set_from_table(sd, "stream_stop",
				&state->regs->stream_stop, 1, 0);
		CHECK_ERR_MSG(err, "failed to stop stream\n");

//		msleep_debug(state->pdata->common.streamoff_delay, true);
	}

	switch (state->runmode) {
	case RUNMODE_CAPTURING:
		cam_dbg("Capture Stop!\n");
		sr352_get_exif(sd);
		state->runmode = RUNMODE_CAPTURING_STOP;
		state->capture.ready = 0;
		state->flash.on = state->capture.lowlux = 0;

		break;

	case RUNMODE_RUNNING:
		cam_dbg("Preview Stop!\n");
		state->runmode = RUNMODE_RUNNING_STOP;

		if (state->capture.pre_req) {
			sr352_start_fast_capture(sd);
			state->capture.pre_req = 0;
		}
		break;

	case RUNMODE_RECORDING:
		state->runmode = RUNMODE_RECORDING_STOP;
		break;

	default:
		break;
	}

	return 0;
}

static int sr352_check_esd(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 read_value = 0;
	int err = -EINVAL;

	err = sr352_i2c_read(client, 0x0D, &read_value);
	CHECK_ERR(err);

	if (read_value != 0xAA)
		goto esd_out;

	err = sr352_i2c_read(client, 0x0F, &read_value);
	CHECK_ERR(err);

	if (read_value != 0xAA)
		goto esd_out;

	cam_info("Check ESD: not detected\n\n");
	return 0;

esd_out:
	cam_err("Check ESD: error, ESD Shock detected! (val=0x%X)\n\n",
		read_value);
	return -ERESTART;

}

/* returns the real iso currently used by sensor due to lighting
 * conditions, not the requested iso we sent using s_ctrl.
 */
/* PX: */
static inline int sr352_get_exif_iso(struct v4l2_subdev *sd, u16 *iso)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 read_value = 0;

	sr352_i2c_write(client, 0x03, 0x20);
	sr352_i2c_read(client, 0x50, &read_value);

	if (read_value < 0x21)
		*iso = 50;
	else if (read_value < 0x5C)
		*iso = 100;
	else if (read_value < 0x83)
		*iso = 200;
	else if (read_value < 0xF1)
		*iso = 400;
	else
		*iso = 800;

	cam_err("exif iso : %d\n", *iso);

	return 0;
}

/* PX: Set ISO */
static int __used sr352_set_iso(struct v4l2_subdev *sd, s32 val)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;

	cam_trace("E\n");

retry:
	switch (val) {
	case ISO_AUTO:
	case ISO_100:
	case ISO_200:
	case ISO_400:
		err = sr352_set_from_table(sd, "iso",
			state->regs->iso, ARRAY_SIZE(state->regs->iso), val);
		break;

	default:
		cam_err("set_iso: not supported iso %d\n", val);
		val = ISO_AUTO;
		goto retry;
		break;
	}

	switch (val) {
	case ISO_100:
		state->iso = 100;
		break;
	case ISO_200:
		state->iso = 200;
		break;
	case ISO_400:
		state->iso = 400;
		break;
	case ISO_800:
		state->iso = 800;
		break;
	default:
		state->iso = ISO_AUTO;
		break;
	}

	cam_trace("X\n");
	return 0;
}

/**
 * get_exif_exptime: return exposure time (ms)
 */
static inline int sr352_get_exif_exptime(struct v4l2_subdev *sd,
						u32 *exp_time)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	u8 read_value0 = 0;
	u8 read_value1 = 0;
	u8 read_value2 = 0;
	u8 read_value3 = 0;

	sr352_i2c_write(client, 0x03, 0x20);
	sr352_i2c_read(client, 0xa0, &read_value0);
	sr352_i2c_read(client, 0xa1, &read_value1);
	sr352_i2c_read(client, 0xa2, &read_value2);
	sr352_i2c_read(client, 0xa3, &read_value3);
	*exp_time = 27000000 / ((read_value0 << 24)
		+ (read_value1 << 16) + (read_value2 << 8) + read_value3);

	cam_err("exif exptime : %d\n", *exp_time);

	return 0;

}

static inline void sr352_get_exif_flash(struct v4l2_subdev *sd,
					u16 *flash)
{
	struct sr352_state *state = to_state(sd);
	int flash_on = *flash;

	*flash = 0;

	switch (state->flash.mode) {
	case FLASH_MODE_OFF:
		*flash |= EXIF_FLASH_MODE_SUPPRESSION;
		break;

	case FLASH_MODE_AUTO:
		*flash |= EXIF_FLASH_MODE_AUTO;
		break;

	case FLASH_MODE_ON:
	case FLASH_MODE_TORCH:
		*flash |= EXIF_FLASH_MODE_FIRING;
		break;

	default:
		break;
	}

	if (flash_on)
		*flash |= EXIF_FLASH_FIRED;
}

/* PX: */
static int sr352_get_exif(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	u32 exposure_time = 0;
	int err = 0;

	/* exposure time */
	state->exif.exp_time_den = 0;
	err = sr352_get_exif_exptime(sd, &exposure_time);
	if (exposure_time)
		state->exif.exp_time_den = exposure_time;
	else
		cam_err("EXIF: error, exposure time 0. %d\n", err);

	/* iso */
	state->exif.iso = 0;
	err = sr352_get_exif_iso(sd, &state->exif.iso);
	if (unlikely(err))
		cam_err("EXIF: error, fail to get exif iso. %d\n", err);

	/* flash */
	sr352_get_exif_flash(sd, &state->exif.flash);

	cam_dbg("EXIF: ex_time_den=%d, iso=%d, flash=0x%02X\n",
		state->exif.exp_time_den, state->exif.iso, state->exif.flash);

	return 0;
}

static inline int sr352_check_cap_sz_except(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;

	cam_err("capture_size_exception: warning, reconfiguring...\n\n");

	switch (state->wide_cmd) {
	case WIDE_REQ_CHANGE:
		cam_info("%s: Wide Capture setting\n", __func__);
		err = sr352_set_from_table(sd, "change_wide_cap",
			&state->regs->change_wide_cap, 1, 0);
		break;

	case WIDE_REQ_RESTORE:
		cam_info("%s: Restore capture setting\n", __func__);
		err = sr352_set_from_table(sd, "restore_capture",
				&state->regs->restore_cap, 1, 0);
		break;

	default:
		cam_err("%s: WARNING, invalid argument(%d)\n",
				__func__, state->wide_cmd);
		break;
	}

	/* Don't forget the below codes.
	 * We set here state->preview to NULL after reconfiguring
	 * capure config if capture ratio does't match with preview ratio.
	 */
	state->preview.frmsize = NULL;
	CHECK_ERR(err);

	return 0;
}

static inline int sr352_set_capture_flash(struct v4l2_subdev *sd)
{
	cam_dbg("sr352_set_capture_flash : capture_flash is not implemented\n");

	return 0;
}

static int sr352_set_preview_size(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;

	if (!state->preview.update_frmsize)
		return 0;

	cam_trace("E, wide_cmd=%d\n", state->wide_cmd);

	switch (state->wide_cmd) {
	case WIDE_REQ_CHANGE:
		cam_info("preview_size: Wide Capture setting\n");
		err = sr352_set_from_table(sd, "change_wide_cap",
			&state->regs->change_wide_cap, 1, 0);
		break;

	case WIDE_REQ_RESTORE:
		cam_info("preview_size:Restore capture setting\n");
		err = sr352_set_from_table(sd, "restore_capture",
				&state->regs->restore_cap, 1, 0);
		/* We do not break */

	default:
		cam_dbg("set_preview_size\n");
		err = sr352_set_from_table(sd, "preview_size",
				state->regs->preview_size,
				ARRAY_SIZE(state->regs->preview_size),
				state->preview.frmsize->index);
		cam_dbg("set_preview_size is done\n");
		break;
	}
	CHECK_ERR(err);

	state->preview.update_frmsize = 0;

	return 0;
}

static int sr352_set_capture_size(struct v4l2_subdev *sd)
{
//	struct sr352_state *state = to_state(sd);
	int err = 0;

	/* do nothing about size except switching normal(wide)
	 * to wide(normal). */
//	if (unlikely(state->wide_cmd))
//		err = sr352_check_cap_sz_except(sd);

	return err;
}

static int sr352_start_preview(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;

	cam_dbg("Camera Preview start, runmode = %d\n", state->runmode);

	if ((state->runmode == RUNMODE_NOTREADY) ||
	    (state->runmode == RUNMODE_CAPTURING)) {
		cam_err("%s: error - Invalid runmode\n", __func__);
		return -EPERM;
	}

	/* Check pending fps */
	if (state->req_fps >= 0) {
		err = sr352_set_frame_rate(sd, state->req_fps);
		CHECK_ERR(err);
	}

	/* Transit preview mode */
//	err = sr352_transit_preview_mode(sd);
//	CHECK_ERR_MSG(err, "preview_mode(%d)\n", err);

	/* Set preview size */
	err = sr352_set_preview_size(sd);
	CHECK_ERR_MSG(err, "failed to set preview size(%d)\n", err);

	/* Set movie mode if needed. */
	err = sr352_transit_movie_mode(sd);
	CHECK_ERR_MSG(err, "failed to set movie mode(%d)\n", err);

	state->runmode = (state->sensor_mode == SENSOR_CAMERA) ?
			RUNMODE_RUNNING : RUNMODE_RECORDING;

	return 0;
}

static inline int sr352_update_preview(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	if (!state->need_preview_update)
		return 0;

	if ((RUNMODE_RUNNING == state->runmode)
	    || (RUNMODE_RECORDING == state->runmode)) {
		/* Update preview */
		err = sr352_set_from_table(sd, "update_preview",
				&state->regs->update_preview, 1, 0);
		CHECK_ERR_MSG(err, "update_preview %d\n", err);

		state->need_preview_update = 0;
	}

	return 0;
}

/**
 * sr352_start_capture: Start capture
 */
static int sr352_start_capture(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = -ENODEV;

	if (state->capture.ready)
		return 0;

	/* Set capture size */
	err = sr352_set_capture_size(sd);
	CHECK_ERR_MSG(err, "fail to set capture size (%d)\n", err);

	if ((state->scene_mode != SCENE_MODE_NIGHTSHOT)
	    && (state->light_level < LUX_LEVEL_LOW)) {
		state->capture.lowlux = 1;
		err = sr352_set_from_table(sd, "lowlight_cap_on",
				&state->regs->lowlight_cap_on, 1, 0);
		CHECK_ERR(err);
	}

	/* Transit to capture mode */
	err = sr352_transit_capture_mode(sd);

	state->runmode = RUNMODE_CAPTURING;

	CHECK_ERR_MSG(err, "fail to capture_mode (%d)\n", err);

	/*sr352_get_exif(sd); */

	return err;
}

static int sr352_start_fast_capture(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	cam_info("set_fast_capture\n");

	state->req_fmt.width = (state->capture.pre_req >> 16);
	state->req_fmt.height = (state->capture.pre_req & 0xFFFF);
	sr352_set_framesize(sd, sr352_capture_frmsizes,
		ARRAY_SIZE(sr352_capture_frmsizes), false);

	err = sr352_start_capture(sd);
	CHECK_ERR(err);

	state->capture.ready = 1;

	return 0;
}

/**
 * sr352_switch_hd_mode: swith to/from HD mode
 * @fmt:
 *
 */
static inline int sr352_switch_hd_mode(
		struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct sr352_state *state = to_state(sd);
	bool hd_mode = 0;
	u32 ratio = FRM_RATIO(fmt->width, fmt->height);
	int err = -EINVAL;

	cam_trace("EX, width = %d, ratio = %d", fmt->width, ratio);

	if ((SENSOR_MOVIE == state->sensor_mode)
	    && (fmt->width == 1280) && (ratio == FRMRATIO_HD))
		hd_mode = 1;

	if (state->hd_videomode ^ hd_mode) {
		cam_info("transit HD video %d -> %d\n", !hd_mode, hd_mode);
		state->hd_videomode = hd_mode;

		cam_dbg("******* Reset start *******\n");
		err = sr352_reset(sd, 2);
		CHECK_ERR(err);

		sr352_init(sd, 0);
		CHECK_ERR(err);
		cam_dbg("******* Reset end *******\n\n");
	}

	return 0;
}

static void sr352_init_parameter(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);

	state->runmode = RUNMODE_INIT;
	state->scene_mode = SCENE_MODE_NONE;
	state->light_level = 0xFFFFFFFF;

}

static int sr352_restore_ctrl(struct v4l2_subdev *sd)
{
	struct v4l2_control ctrl;
	int i;

	cam_trace("EX\n");

	for (i = 0; i < ARRAY_SIZE(sr352_ctrls); i++) {
		if (sr352_ctrls[i].value !=
		    sr352_ctrls[i].default_value) {
			ctrl.id = sr352_ctrls[i].id;
			ctrl.value = sr352_ctrls[i].value;
			cam_dbg("restore_ctrl: ID 0x%08X, val %d\n",
					ctrl.id, ctrl.value);

			sr352_s_ctrl(sd, &ctrl);
		}
	}

	return 0;
}

static int sr352_check_sensor(struct v4l2_subdev *sd)
{
	cam_dbg("Sensor chip indentification: Success");
	return 0;
}

static int sr352_s_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct sr352_state *state = to_state(sd);
	s32 prev_index = 0;

	cam_dbg("%s: pixelformat = 0x%x, colorspace = 0x%x, width = %d, height = %d\n",
		__func__, fmt->code, fmt->colorspace, fmt->width, fmt->height);

	v4l2_fill_pix_format(&state->req_fmt, fmt);

	state->wide_cmd = WIDE_REQ_NONE;

	if (state->format_mode != V4L2_PIX_FMT_MODE_CAPTURE) {
		prev_index = state->preview.frmsize ?
				state->preview.frmsize->index : -1;

		sr352_set_framesize(sd, sr352_preview_frmsizes,
			ARRAY_SIZE(sr352_preview_frmsizes), true);

		/* preview.frmsize cannot absolutely go to null */
		state->preview.update_frmsize = 1;
		if (state->preview.frmsize->index != prev_index)
			cam_info("checking unmatched ratio skipped\n");

		state->previousHDmode = state->hd_videomode;
		if ((SENSOR_MOVIE == state->sensor_mode)
		    && (PREVIEW_SZ_PVGA == state->preview.frmsize->index))
				state->hd_videomode = 1;
		else
				state->hd_videomode = 0;
	} else {
		sr352_set_framesize(sd, sr352_capture_frmsizes,
			ARRAY_SIZE(sr352_capture_frmsizes), false);
	}

	return 0;
}

static int sr352_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
					enum v4l2_mbus_pixelcode *code)
{
	cam_dbg("enum_mbus_fmt: index = %d\n", index);

	if (index >= ARRAY_SIZE(capture_fmts))
		return -EINVAL;

	*code = capture_fmts[index].code;

	return 0;
}

static int sr352_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int num_entries;
	int i;

	num_entries = ARRAY_SIZE(capture_fmts);

	cam_dbg("try_mbus_fmt: code = 0x%x,"
		" colorspace = 0x%x, num_entries = %d\n",
		fmt->code, fmt->colorspace, num_entries);

	for (i = 0; i < num_entries; i++) {
		if ((capture_fmts[i].code == fmt->code) &&
		    (capture_fmts[i].colorspace == fmt->colorspace)) {
			cam_dbg("%s: match found, returning 0\n", __func__);
			return 0;
		}
	}

	cam_err("%s: no match found, returning -EINVAL\n", __func__);
	return -EINVAL;
}

static int sr352_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	struct sr352_state *state = to_state(sd);

	/*
	* The camera interface should read this value, this is the resolution
	* at which the sensor would provide framedata to the camera i/f
	* In case of image capture,
	* this returns the default camera resolution (VGA)
	*/
	if (state->format_mode != V4L2_PIX_FMT_MODE_CAPTURE) {
		if (unlikely(state->preview.frmsize == NULL)) {
			cam_err("%s: error\n", __func__);
			return -EFAULT;
		}

		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = state->preview.frmsize->width;
		fsize->discrete.height = state->preview.frmsize->height;
	} else {
		if (unlikely(state->capture.frmsize == NULL)) {
			cam_err("%s: error\n", __func__);
			return -EFAULT;
		}

		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = state->capture.frmsize->width;
		fsize->discrete.height = state->capture.frmsize->height;
	}

	return 0;
}

static int sr352_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	return 0;
}

static int sr352_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct sr352_state *state = to_state(sd);
	s32 req_fps;

	req_fps = param->parm.capture.timeperframe.denominator /
			param->parm.capture.timeperframe.numerator;

	cam_dbg("s_parm state->fps=%d, state->req_fps=%d\n",
		state->fps,req_fps);

	return sr352_set_frame_rate(sd, req_fps);
}

static inline bool sr352_is_clear_ctrl(struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_CAMERA_BRIGHTNESS:
	case V4L2_CID_CAM_BRIGHTNESS:
		break;

	default:
		if (ctrl->value < 0) {
			/*cam_dbg("ctrl ID 0x%08X skipped (%d)\n",
				ctrl->id, ctrl->value);*/
			return true;
		}
	}

	return false;
}

static int sr352_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	if (!state->initialized) {
		cam_err("g_ctrl: warning, camera not initialized\n");
		return 0;
	}

	mutex_lock(&state->ctrl_lock);

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_EXIF_EXPTIME:
		if (state->sensor_mode == SENSOR_CAMERA)
			ctrl->value = state->exif.exp_time_den;
		else
			ctrl->value = 24;
		break;

	case V4L2_CID_CAMERA_EXIF_ISO:
		if (state->sensor_mode == SENSOR_CAMERA)
			ctrl->value = state->exif.iso;
		else
			ctrl->value = 100;
		break;

	case V4L2_CID_CAMERA_EXIF_FLASH:
		if (state->sensor_mode == SENSOR_CAMERA)
			ctrl->value = state->exif.flash;
		else
			sr352_get_exif_flash(sd, (u16 *)ctrl->value);
		break;

	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
	case V4L2_CID_CAM_AUTO_FOCUS_RESULT:
		ctrl->value = AF_RESULT_SUCCESS;
		break;

	case V4L2_CID_CAMERA_WHITE_BALANCE:
	case V4L2_CID_CAMERA_EFFECT:
	case V4L2_CID_CAMERA_CONTRAST:
	case V4L2_CID_CAMERA_SATURATION:
	case V4L2_CID_CAMERA_SHARPNESS:
	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
	default:
		cam_err("g_ctrl: warning, unknown Ctrl-ID %d (0x%x)\n",
			ctrl->id - V4L2_CID_PRIVATE_BASE,
			ctrl->id - V4L2_CID_PRIVATE_BASE);
		err = 0; /* we return no error. */
		break;
	}

	mutex_unlock(&state->ctrl_lock);

	return err;
}

static int sr352_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	if (!state->initialized && (ctrl->id != V4L2_CID_CAMERA_SENSOR_MODE)) {

		cam_warn("s_ctrl: warning, camera not initialized. ID %d(0x%X)\n",
			ctrl->id & 0xFF, ctrl->id);
		return 0;
	}

	cam_dbg("s_ctrl: ID =0x%08X, val = %d\n", ctrl->id, ctrl->value);

	mutex_lock(&state->ctrl_lock);

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_SENSOR_MODE:
		err = sr352_set_sensor_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_BRIGHTNESS:
	case V4L2_CID_CAM_BRIGHTNESS:
		err = sr352_set_exposure(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_WHITE_BALANCE:
	case V4L2_CID_WHITE_BALANCE_PRESET:
		err = sr352_set_whitebalance(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_EFFECT:
	case V4L2_CID_IMAGE_EFFECT:
		err = sr352_set_effect(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_METERING:
	case V4L2_CID_CAM_METERING:
		err = sr352_set_metering(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_CONTRAST:
		err = sr352_set_from_table(sd, "contrast",
			state->regs->contrast,
			ARRAY_SIZE(state->regs->contrast), ctrl->value);
		break;

	case V4L2_CID_CAMERA_SATURATION:
		err = sr352_set_from_table(sd, "saturation",
			state->regs->saturation,
			ARRAY_SIZE(state->regs->saturation), ctrl->value);
		break;

	case V4L2_CID_CAMERA_SHARPNESS:
		err = sr352_set_from_table(sd, "sharpness",
			state->regs->sharpness,
			ARRAY_SIZE(state->regs->sharpness), ctrl->value);
		break;

	case V4L2_CID_CAMERA_SCENE_MODE:
	case V4L2_CID_SCENEMODE:
		err = sr352_set_scene_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_CHECK_ESD:
		err = sr352_check_esd(sd);
		break;

	case V4L2_CID_CAMERA_ANTI_BANDING:
		break;

	case V4L2_CID_CAPTURE:
		if (ctrl->value > 1) {
			break;
		}

		if ((SENSOR_CAMERA == state->sensor_mode) && ctrl->value) {
			cam_dbg("Capture Mode!\n");
			state->format_mode = V4L2_PIX_FMT_MODE_CAPTURE;
		} else
			state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
		break;

	case V4L2_CID_CAMERA_ISO:
	case V4L2_CID_CAM_ISO:
		err = sr352_set_iso(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FRAME_RATE:
		err = sr352_set_frame_rate(sd, ctrl->value);
		break;

	case V4L2_CID_CAM_AEAWB_LOCK_UNLOCK:
		err = sr352_set_aeawb_lock_unlock(sd, ctrl->value);
		break;

	default:
		cam_err("s_ctrl: warning, unknown Ctrl-ID %d (0x%08X)\n",
			ctrl->id & 0xFF, ctrl->id );
		break;
	}

	mutex_unlock(&state->ctrl_lock);
	CHECK_ERR_MSG(err, "s_ctrl failed %d\n", err);

	return 0;
}

static int sr352_pre_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;

	if (sr352_is_clear_ctrl(ctrl))
		return 0;

	sr352_save_ctrl(sd, ctrl);

	/* Call s_ctrl */
	err = sr352_s_ctrl(sd, ctrl);
	CHECK_ERR(err);

	if (IS_PREVIEW_UPDATE_SUPPORTED())
		err = sr352_update_preview(sd);

	return err;
}

static int sr352_s_ext_ctrl(struct v4l2_subdev *sd,
			      struct v4l2_ext_control *ctrl)
{
	return 0;
}

static int sr352_s_ext_ctrls(struct v4l2_subdev *sd,
				struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int ret = 0;
	int i;

	for (i = 0; i < ctrls->count; i++, ctrl++) {
		ret = sr352_s_ext_ctrl(sd, ctrl);
		if (unlikely(ret)) {
			ctrls->error_idx = i;
			break;
		}
	}

	return ret;
}

static int sr352_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sr352_state *state = to_state(sd);
	int err = 0;

	cam_info("#### stream mode = %d\n", enable);

	if (unlikely(!state->initialized)) {
		cam_err("s_stream: not initialized device. start init\n\n");
		err = sr352_init(sd, 0);
		CHECK_ERR(err);
	}

	switch (enable) {
	case STREAM_MODE_CAM_OFF:
		err = sr352_control_stream(sd, STREAM_STOP);
		break;

	case STREAM_MODE_CAM_ON:
		if (unlikely(state->need_wait_streamoff)) {
			cam_err("error, must wait for stream-off\n\n");
			err = sr352_s_stream(sd, STREAM_MODE_WAIT_OFF);
			CHECK_ERR(err);
		}
		if (V4L2_PIX_FMT_MODE_CAPTURE == state->format_mode)
			err = sr352_start_capture(sd);
		else
			err = sr352_start_preview(sd);
		break;

	case STREAM_MODE_MOVIE_ON:
		cam_info("movie on");
		state->recording = 1;
		break;

	case STREAM_MODE_MOVIE_OFF:
		cam_info("movie off");
		state->recording = 0;
		break;

	case STREAM_MODE_WAIT_OFF:
		if (state->runmode == RUNMODE_RECORDING || state->runmode == RUNMODE_RECORDING_STOP) {
			if (state->previousHDmode) { // HDmode ->normalmode
				err = SR352_BURST_WRITE_REGS(sd, sr352_init_regs);
				CHECK_ERR_MSG(err, "failed to initialize camera device\n");
				if (state->hd_videomode == 0 && state->preview.frmsize->width != 1024) {
					cam_dbg("HD mode recording -> otherpreview - so use the enterpreview\n");
					err = SR352_BURST_WRITE_REGS(sd, sr352_sz_1024);
					CHECK_ERR_MSG(err, "failed to enterPreview\n");
				}
			}
		}
		break;

	default:
		cam_err("%s: error - Invalid stream mode\n", __func__);
		break;
	}

	CHECK_ERR_MSG(err, "failed\n");

	return 0;
}

/**
 * sr352_reset: reset the sensor device
 * @val: 0 - reset parameter.
 *      1 - power reset
 */
static int sr352_reset(struct v4l2_subdev *sd, u32 val)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;
	int previousRunmode = state->runmode;

	cam_info("reset camera sub-device\n");

	if (state->wq)
		flush_workqueue(state->wq);

	state->initialized = 0;
	state->need_wait_streamoff = 0;
	state->runmode = RUNMODE_NOTREADY;

	if (val) {
		if (SR352_HW_POWER_ON == state->power_on) {
			err = sr352_power(sd, 0);
			CHECK_ERR(err);
			msleep_debug(50, true);
		} else {
			cam_err("reset: sensor is not powered. power status %d\n",
				state->power_on);
		}

		err = sr352_power(sd, 1);
		CHECK_ERR(err);
		if (previousRunmode != state->runmode) {
			err = sr352_init(sd, 0);
			CHECK_ERR(err);
		}
	}

	state->reset_done = 1;
	stats_reset++;

	return 0;
}

static int sr352_init(struct v4l2_subdev *sd, u32 val)
{
	struct sr352_state *state = to_state(sd);
	int err = -EINVAL;

	cam_info("init: start (%s). power %u, init %u, rst %u, i2c %u\n", __DATE__,
		stats_power, stats_init, stats_reset, stats_i2c_err);

#if CONFIG_LOAD_FILE
	err = loadFile();
	if (unlikely(err < 0)) {
		cam_err("failed to load file ERR=%d\n", err);
		return err;
	}
#endif

	err = sr352_check_sensor(sd);
	CHECK_ERR_MSG(err, "failed to indentify sensor chip\n");

	cam_info("init: Cam\n");
	err = SR352_BURST_WRITE_REGS(sd, sr352_init_regs);

	CHECK_ERR_MSG(err, "failed to initialize camera device\n");

#if 0
	sr352_set_from_table(sd, "antibanding",
		&state->regs->antibanding, 1, 0);
#endif

	sr352_init_parameter(sd);
	state->initialized = 1;
	stats_init++;

	if (state->reset_done) {
		state->reset_done = 0;
		sr352_restore_ctrl(sd);
	}

	return 0;
}

static int sr352_s_power(struct v4l2_subdev *sd, int on)
{
	/* struct sr352_state *state = to_state(sd);*/
	int err = 0;

	if (on) {
		stats_power++;
#ifdef CONFIG_CAM_EARYLY_PROBE
		err = sr352_late_probe(sd);
#else
		err = sr352_get_power(sd);
#endif
		CHECK_ERR(err);

		err = sr352_power(sd, 1);
		if (unlikely(err))
			goto out_fail;

		err = sr352_init(sd, 0);
		if (unlikely(err))
			goto out_fail;
	} else {
#ifdef CONFIG_CAM_EARYLY_PROBE
		err = sr352_early_remove(sd);
#else
		err = sr352_power(sd, 0);
		CHECK_ERR(err);

		err = sr352_put_power(sd);
#endif
	}

	return err;

out_fail:
	cam_err("s_power: error, couldn't init device");
	sr352_s_power(sd, 0);
	return err;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize
 * every single opening time therefor,
 * it is not necessary to be initialized on probe time.
 * except for version checking
 * NOTE: version checking is optional
 */
static int sr352_s_config(struct v4l2_subdev *sd,
			int irq, void *platform_data)
{
	struct sr352_state *state = to_state(sd);
	struct sr352_platform_data *pdata = platform_data;
	int i;

	if (!platform_data) {
		cam_err("%s: error, no platform data\n", __func__);
		return -ENODEV;
	}

#if 1 /* DSLIM fix me later */
	pdata->common.default_width = 1024;
	pdata->common.default_height = 768;
	pdata->common.pixelformat = V4L2_PIX_FMT_UYVY;
	pdata->common.freq = 24000000;
	pdata->common.is_mipi = 1;
	pdata->common.streamoff_delay = SR352_STREAMOFF_DELAY;
	pdata->common.dbg_level = CAMDBG_LEVEL_DEFAULT;
#endif

	state->regs = &reg_datas;
	state->pdata = pdata;
	state->focus.support = IS_AF_SUPPORTED();
	state->flash.support = IS_FLASH_SUPPORTED();
	state->dbg_level = &dbg_level;

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	state->req_fmt.width = pdata->common.default_width;
	state->req_fmt.height = pdata->common.default_height;

	if (!pdata->common.pixelformat)
		state->req_fmt.pixelformat = DEFAULT_PIX_FMT;
	else
		state->req_fmt.pixelformat = pdata->common.pixelformat;

	if (!pdata->common.freq)
		state->freq = DEFAULT_MCLK;	/* 24MHz default */
	else
		state->freq = pdata->common.freq;

	state->preview.frmsize = sr352_get_framesize_i(sr352_preview_frmsizes,
					ARRAY_SIZE(sr352_preview_frmsizes),
					PREVIEW_SZ_VGA);
	state->capture.frmsize = sr352_get_framesize_i(sr352_capture_frmsizes,
					ARRAY_SIZE(sr352_capture_frmsizes),
					CAPTURE_SZ_3MP);
	state->sensor_mode = SENSOR_CAMERA;
	state->hd_videomode = 0;
	state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
	state->fps = FRAME_RATE_AUTO;
	state->req_fps = -1;
	state->iso = ISO_AUTO;

	for (i = 0; i < ARRAY_SIZE(sr352_ctrls); i++)
		sr352_ctrls[i].value = sr352_ctrls[i].default_value;

	return 0;
}

static const struct v4l2_subdev_core_ops sr352_core_ops = {
	.s_power = sr352_s_power,
	.init = sr352_init,	/* initializing API */
	.g_ctrl = sr352_g_ctrl,
	.s_ctrl = sr352_pre_s_ctrl,
	.s_ext_ctrls = sr352_s_ext_ctrls,
	.reset = sr352_reset,
};

static const struct v4l2_subdev_video_ops sr352_video_ops = {
	.s_mbus_fmt = sr352_s_mbus_fmt,
	.enum_framesizes = sr352_enum_framesizes,
	.enum_mbus_fmt = sr352_enum_mbus_fmt,
	.try_mbus_fmt = sr352_try_mbus_fmt,
	.g_parm = sr352_g_parm,
	.s_parm = sr352_s_parm,
	.s_stream = sr352_s_stream,
};

static struct v4l2_mbus_framefmt *__find_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				enum v4l2_subdev_format_whence which,
				enum sr352_oprmode type)
{
	struct sr352_state *state = to_state(sd);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return fh ? v4l2_subdev_get_try_format(fh, 0) : NULL;

	return &state->ffmt;
}

/* get format by flite video device command */
static int sr352_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct sr352_state *state = to_state(sd);
	struct v4l2_mbus_framefmt *format;

	if (fmt->pad != 0)
		return -EINVAL;

	format = __find_format(sd, fh, fmt->which, state->res_type);
	if (!format)
		return -EINVAL;

	fmt->format = *format;

	return 0;
}

/* set format by flite video device command */
static int sr352_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct sr352_state *state = to_state(sd);
	struct v4l2_mbus_framefmt *format = &fmt->format;
	struct v4l2_mbus_framefmt *sfmt;
	const struct sr352_framesize *framesize;

	if (fmt->pad != 0)
		return -EINVAL;

	state->oprmode = SR352_OPRMODE_VIDEO;

	sfmt		= &default_fmt[state->oprmode];
	sfmt->width	= format->width;
	sfmt->height	= format->height;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		sr352_s_mbus_fmt(sd, sfmt);  /* set format */

		if (V4L2_PIX_FMT_MODE_CAPTURE == state->format_mode)
			framesize = state->capture.frmsize;
		else
			framesize = state->preview.frmsize;

		/* for enum size of entity by flite */
		state->ffmt.width = format->width = framesize->width;
		state->ffmt.height = format->height = framesize->height;
		state->ffmt.colorspace = format->colorspace;

#ifndef CONFIG_VIDEO_SR352_SENSOR_JPEG
		state->ffmt.code	= V4L2_MBUS_FMT_YUYV8_2X8;
#else
		state->ffmt.code	= format->code;
#endif
	}

	return 0;
}

/* enum code by flite video device command */
static int sr352_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (!code || (code->index >= ARRAY_SIZE(default_fmt)))
		return -EINVAL;

	code->code = default_fmt[code->index].code;

	return 0;
}


static struct v4l2_subdev_pad_ops sr352_pad_ops = {
	.enum_mbus_code	= sr352_enum_mbus_code,
	.get_fmt	= sr352_get_fmt,
	.set_fmt	= sr352_set_fmt,
};

static const struct v4l2_subdev_ops sr352_ops = {
	.core = &sr352_core_ops,
	.pad = &sr352_pad_ops,
	.video = &sr352_video_ops,
};

#ifdef CONFIG_MEDIA_CONTROLLER
static int sr352_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static const struct media_entity_operations sr352_media_ops = {
	.link_setup = sr352_link_setup,
};
#endif

/* internal ops for media controller */
static int sr352_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_err(&client->dev, "%s: \n", __func__);
	memset(&format, 0, sizeof(format));
	format.pad = 0;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = DEFAULT_SENSOR_CODE;
	format.format.width = DEFAULT_SENSOR_WIDTH;
	format.format.height = DEFAULT_SENSOR_HEIGHT;

#ifdef ENABLE
	sr352_set_fmt(sd, fh, &format);
	sr352_s_parm(sd, &state->strm);
#endif

	return 0;
}

static int sr352_subdev_close(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	sr352_debug(SR352_DEBUG_I2C, "%s", __func__);
	pr_info("%s", __func__);
	return 0;
}

static int sr352_subdev_registered(struct v4l2_subdev *sd)
{
	sr352_debug(SR352_DEBUG_I2C, "%s", __func__);
	return 0;
}

static void sr352_subdev_unregistered(struct v4l2_subdev *sd)
{
	sr352_debug(SR352_DEBUG_I2C, "%s", __func__);
}

static const struct v4l2_subdev_internal_ops sr352_v4l2_internal_ops = {
	.open = sr352_init_formats,
	.close = sr352_subdev_close,
	.registered = sr352_subdev_registered,
	.unregistered = sr352_subdev_unregistered,
};

/*
 * sr352_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
#ifdef CONFIG_CAM_EARYLY_PROBE
static int sr352_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sr352_core_state *c_state;
	struct sr352_state *state;
	int err = -EINVAL;

	c_state = kzalloc(sizeof(struct sr352_core_state), GFP_KERNEL);
	if (unlikely(!c_state)) {
		dev_err(&client->dev, "early_probe, fail to get memory\n");
		return -ENOMEM;
	}

	state = kzalloc(sizeof(struct sr352_state), GFP_KERNEL);
	if (unlikely(!state)) {
		dev_err(&client->dev, "early_probe, fail to get memory\n");
		goto err_out2;
	}

	c_state->data = (u32)state;
	sd = &c_state->sd;
	strcpy(sd->name, driver_name);
	strcpy(c_state->c_name, "sr352_core_state"); /* for debugging */

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &sr352_ops);

#ifdef CONFIG_MEDIA_CONTROLLER
	c_state->pad.flags = MEDIA_PAD_FL_SOURCE;
	err = media_entity_init(&sd->entity, 1, &c_state->pad, 0);
	if (unlikely(err)) {
		dev_err(&client->dev, "probe: fail to init media entity\n");
		goto err_out1;
	}

	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->entity.ops = &sr352_media_ops;
#endif

	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &sr352_v4l2_internal_ops;

	c_state->wq = create_workqueue("cam_wq");
	if (unlikely(!c_state->wq)) {
		dev_err(&client->dev,
			"early_probe: fail to create workqueue\n");
		goto err_out1;
	}

	printk(KERN_DEBUG "%s %s: driver probed!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;

err_out1:
	kfree(state);
err_out2:
	kfree(c_state);
	return -ENOMEM;
}

static int sr352_late_probe(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	struct sr352_core_state *c_state = to_c_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	if (unlikely(!c_state || !state)) {
		dev_err(&client->dev, "late_probe, fail to get memory."
			" c_state = 0x%X, state = 0x%X\n", (u32)c_state, (u32)state);
		return -ENOMEM;
	}

	memset(state, 0, sizeof(struct sr352_state));
	state->c_state = c_state;
	state->sd = sd;
	state->wq = c_state->wq;
	strcpy(state->s_name, "sr352_state"); /* for debugging */

	mutex_init(&state->ctrl_lock);
	mutex_init(&state->i2c_lock);

	state->runmode = RUNMODE_NOTREADY;

	err = sr352_s_config(sd, 0, client->dev.platform_data);
	CHECK_ERR_MSG(err, "probe: fail to s_config\n");

	state->mclk = clk_get(NULL, "cam1");
	if (IS_ERR_OR_NULL(state->mclk)) {
		pr_err("failed to get cam1 clk (mclk)");
		return -ENXIO;
	}

	err = sr352_get_power(sd);
	CHECK_ERR_GOTO_MSG(err, err_out, "probe: fail to get power\n");

	printk(KERN_DEBUG "%s %s: driver late probed!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;

err_out:
	sr352_put_power(sd);
	return -ENOMEM;
}

static int sr352_early_remove(struct v4l2_subdev *sd)
{
	struct sr352_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0, ret = 0;

	if (state->wq)
		flush_workqueue(state->wq);

	err = sr352_power(sd, 0);	if (unlikely(err)) {
		cam_info("remove: power off failed. %d\n", err);
		ret = err;
	}

	err = sr352_put_power(sd);
	if (unlikely(err)) {
		cam_info("remove: put power failed. %d\n", err);
		ret = err;
	}

	cam_info("stats: power %u, init %u, rst %u, i2c %u\n",
		stats_power, stats_init, stats_reset, stats_i2c_err);

#if CONFIG_LOAD_FILE
	large_file ? vfree(testBuf) : kfree(testBuf);
	large_file = 0;
	testBuf = NULL;
#endif

	printk(KERN_DEBUG "%s %s: driver early removed!!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));
	return ret;
}

static int sr352_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sr352_core_state *c_state = to_c_state(sd);

	if (c_state->wq)
		destroy_workqueue(c_state->wq);

#ifdef CONFIG_MEDIA_CONTROLLER
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_device_unregister_subdev(sd);
	kfree((void *)(c_state->data));
	kfree(c_state);

	printk(KERN_DEBUG "%s %s: driver removed!!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));
	return 0;
}

#else /* !CONFIG_CAM_EARYLY_PROBE */

static int sr352_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sr352_state *state;
	int err = -EINVAL;

	state = kzalloc(sizeof(struct sr352_state), GFP_KERNEL);
	if (unlikely(!state)) {
		dev_err(&client->dev, "probe, fail to get memory\n");
		return -ENOMEM;
	}

	mutex_init(&state->ctrl_lock);

	state->runmode = RUNMODE_NOTREADY;
	sd = &state->sd;
	strcpy(sd->name, driver_name);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &sr352_ops);
#ifdef CONFIG_MEDIA_CONTROLLER
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	err = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (unlikely(err)) {
		dev_err(&client->dev, "probe: fail to init media entity\n");
		goto err_out;
	}
#endif

	err = sr352_s_config(sd, 0, client->dev.platform_data);
	if (unlikely(err)) {
		dev_err(&client->dev, "probe: fail to s_config\n");
		goto err_out;
	}

	state->wq = create_workqueue("cam_wq");
	if (unlikely(!state->wq)) {
		dev_err(&client->dev,
			"probe: fail to create workqueue\n");
		goto err_out;
	}

/*
	err = sr352_get_power(sd);
	if (unlikely(err)) {
		dev_err(&client->dev, "probe: fail to get power\n");
		goto err_out;
	} */

#ifdef CONFIG_MEDIA_CONTROLLER
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->entity.ops = &sr352_media_ops;
#endif
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &sr352_v4l2_internal_ops;

	printk(KERN_DEBUG "%s %s: driver probed!!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;

err_out:
	/* sr352_put_power(sd); */
	kfree(state);
	return -ENOMEM;
}

static int sr352_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sr352_state *state = to_state(sd);

	if (state->wq)
		destroy_workqueue(state->wq);

	sr352_power(sd, 0);
	sr352_put_power(sd);

#ifdef CONFIG_MEDIA_CONTROLLER
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_device_unregister_subdev(sd);
	mutex_destroy(&state->ctrl_lock);
	kfree(state);

#if CONFIG_LOAD_FILE
	large_file ? vfree(testBuf) : kfree(testBuf);
	large_file = 0;
	testBuf = NULL;
#endif

	printk(KERN_DEBUG "%s %s: driver removed!!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));
	return 0;
}
#endif

#if CONFIG_SUPPORT_CAMSYSFS
static int __init camera_class_init(void)
{
        camera_class = class_create(THIS_MODULE, "camera");
        if (IS_ERR(camera_class)) {
                pr_err("Failed to create class(camera)!\n");
                return PTR_ERR(camera_class);
        }

        return 0;
}

subsys_initcall(camera_class_init);

static ssize_t camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/*pr_info("%s\n", __func__);*/
	return sprintf(buf, "%s_%s\n", "SILICONFILE", driver_name);
}
static DEVICE_ATTR(rear_camtype, S_IRUGO, camtype_show, NULL);

static ssize_t camfw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s N\n", driver_name);

}
static DEVICE_ATTR(rear_camfw, S_IRUGO, camfw_show, NULL);

static ssize_t cam_loglevel_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp_buf[60] = {0,};

	sprintf(buf, "Log Level: ");
	if (dbg_level & CAMDBG_LEVEL_TRACE) {
		sprintf(temp_buf, "trace ");
		strcat(buf, temp_buf);
	}

	if (dbg_level & CAMDBG_LEVEL_DEBUG) {
		sprintf(temp_buf, "debug ");
		strcat(buf, temp_buf);
	}

	if (dbg_level & CAMDBG_LEVEL_INFO) {
		sprintf(temp_buf, "info ");
		strcat(buf, temp_buf);
	}

	sprintf(temp_buf, "\n - warn and error level is always on\n\n");
	strcat(buf, temp_buf);

	return strlen(buf);
}

static ssize_t cam_loglevel_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	printk(KERN_DEBUG "CAM buf=%s, count=%d\n", buf, count);

	if (strstr(buf, "trace"))
		dbg_level |= CAMDBG_LEVEL_TRACE;
	else
		dbg_level &= ~CAMDBG_LEVEL_TRACE;

	if (strstr(buf, "debug"))
		dbg_level |= CAMDBG_LEVEL_DEBUG;
	else
		dbg_level &= ~CAMDBG_LEVEL_DEBUG;

	if (strstr(buf, "info"))
		dbg_level |= CAMDBG_LEVEL_INFO;

	return count;
}

static DEVICE_ATTR(loglevel, 0664, cam_loglevel_show, cam_loglevel_store);

int sr352_create_sysfs(struct class *cls)
{
	struct device *dev = NULL;
	int err = -ENODEV;

	dev = device_create(cls, NULL, 0, NULL, "rear");
	if (IS_ERR(dev)) {
		pr_err("cam_init: failed to create device(rearcam_dev)\n");
		return -ENODEV;
	}

	err = device_create_file(dev, &dev_attr_rear_camtype);
	if (unlikely(err < 0)) {
		pr_err("cam_init: failed to create device file, %s\n",
			dev_attr_rear_camtype.attr.name);
	}

	err = device_create_file(dev, &dev_attr_rear_camfw);
	if (unlikely(err < 0)) {
		pr_err("cam_init: failed to create device file, %s\n",
			dev_attr_rear_camtype.attr.name);
	}

	err = device_create_file(dev, &dev_attr_loglevel);
	if (unlikely(err < 0)) {
		pr_err("cam_init: failed to create device file, %s\n",
			dev_attr_loglevel.attr.name);
	}

	return 0;
}

#endif /* CONFIG_SUPPORT_CAMSYSFS */

static const struct i2c_device_id sr352_id[] = {
	{ SR352_DRIVER_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, sr352_id);

static struct i2c_driver v4l2_i2c_driver = {
	.driver.name	= driver_name,
	.probe		= sr352_probe,
	.remove		= sr352_remove,
	.id_table	= sr352_id,
};

static int __init v4l2_i2c_drv_init(void)
{
	pr_info("%s: %s init\n", __func__, driver_name);

#if CONFIG_SUPPORT_CAMSYSFS
	sr352_create_sysfs(camera_class);
#endif

	return i2c_add_driver(&v4l2_i2c_driver);
}

static void __exit v4l2_i2c_drv_cleanup(void)
{
	pr_info("%s: %s exit\n", __func__, driver_name);

	i2c_del_driver(&v4l2_i2c_driver);
}

module_init(v4l2_i2c_drv_init);
module_exit(v4l2_i2c_drv_cleanup);

MODULE_DESCRIPTION("LSI SR352 3MP SOC camera driver");
MODULE_LICENSE("GPL");
