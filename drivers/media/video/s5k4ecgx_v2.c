/* drivers/media/video/s5k4ecgx.c
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
#include "s5k4ecgx.h"

static u32 dbg_level = CAMDBG_LEVEL_DEFAULT;
static u32 stats_power;
static u32 stats_init;
static u32 stats_reset;
static u32 stats_i2c_err;

#if 1 /* DSLIM fix me later */
struct class *camera_class;

#ifdef CONFIG_MACH_GARDA
static atomic_t flash_status = ATOMIC_INIT(S5K4ECGX_FLASH_OFF);
static spinlock_t flash_op_lock; /* Flash operation lock */
#endif
static struct regulator *main_cam_sensor_a2v8_regulator = NULL;
static struct regulator *main_cam_af_2v8_regulator = NULL;
static struct regulator *main_cam_core_1v2_regulator = NULL;
static struct regulator *main_cam_io_1v8_regulator = NULL;

static struct regulator *vt_cam_core_1v8_regulator = NULL;

bool cam_mipi_en = false;
EXPORT_SYMBOL(cam_mipi_en);

#ifdef CONFIG_ARCH_EXYNOS5
#define GPIO_CAM_MCLK			EXYNOS5_GPG2(1)
#define GPIO_CAM_MEGA_EN		EXYNOS5_GPF1(0)
#define GPIO_CAM_MEGA_nRST		EXYNOS5_GPX1(2)
#define GPIO_CAM_PCLK			EXYNOS5_GPG0(0)

#elif CONFIG_ARCH_EXYNOS4
#define GPIO_CAM_MCLK			EXYNOS4_GPM2(2)

#ifdef CONFIG_MACH_GARDA
#define GPIO_CAM_FLASH_EN		EXYNOS4_GPM3(4)
#define GPIO_CAM_FLASH_SET		EXYNOS4_GPM3(6)
#define GPIO_CAM_MEGA_EN		EXYNOS4_GPX2(3)

#define GPIO_VT_CAM_MEGA_EN		EXYNOS4_GPM1(3)
#define GPIO_VT_CAM_MEGA_nRST		EXYNOS4_GPM1(2)
#else
#define GPIO_CAM_MEGA_EN		EXYNOS4_GPM4(4)
#endif /* CONFIG_MACH_GARDA */

#define GPIO_CAM_MEGA_nRST		EXYNOS4_GPX1(2)
#define GPIO_CAM_PCLK			EXYNOS4_GPM0(0)
#endif /* CONFIG_ARCH_EXYNOS4 */

#endif /* #if 1 */

#define s5k4ecgx_readw(sd, addr, data, flag) s5k4ecgx_read(sd, addr, data, 1, flag)

static struct v4l2_mbus_framefmt default_fmt[S5K4ECGX_OPRMODE_MAX] = {
	[S5K4ECGX_OPRMODE_VIDEO] = {
		.width		= DEFAULT_SENSOR_WIDTH,
		.height		= DEFAULT_SENSOR_HEIGHT,
		.code		= DEFAULT_SENSOR_CODE,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	[S5K4ECGX_OPRMODE_IMAGE] = {
		.width		= 1920,
		.height		= 1080,
		.code		= V4L2_MBUS_FMT_JPEG_1X8,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

static const struct s5k4ecgx_fps s5k4ecgx_framerates[] = {
	{ I_FPS_0,	FRAME_RATE_AUTO },
	{ I_FPS_15,	FRAME_RATE_15 },
	{ I_FPS_30,	FRAME_RATE_30 },
};

static const struct s5k4ecgx_framesize s5k4ecgx_preview_frmsizes[] = {
	{ PREVIEW_SZ_CIF,	352, 288 },
	{ PREVIEW_SZ_VGA,	640, 480 },
	{ PREVIEW_SZ_PVGA,	1280, 720 },
};

/* For video */
static const struct s5k4ecgx_framesize
s5k4ecgx_hidden_preview_frmsizes[0] = {
/*	{ PREVIEW_SZ_PVGA,	1280, 720 },*/
};

static const struct s5k4ecgx_framesize s5k4ecgx_capture_frmsizes[] = {
	{ CAPTRUE_SZ_W3MP,	2560, 1440 },
	{ CAPTURE_SZ_5MP,	2560, 1920 },
};

static struct s5k4ecgx_control s5k4ecgx_ctrls[] = {
	S5K4ECGX_INIT_CONTROL(V4L2_CID_CAMERA_FLASH_MODE, \
				FLASH_MODE_OFF),

	S5K4ECGX_INIT_CONTROL(V4L2_CID_CAM_BRIGHTNESS/*V4L2_CID_CAMERA_BRIGHTNESS*/, \
				EV_DEFAULT),

	/* We set default value of metering to invalid value (METERING_BASE),
	 * to update sensor metering as a user set desired metering value. */
	S5K4ECGX_INIT_CONTROL(V4L2_CID_CAM_METERING/*V4L2_CID_CAMERA_METERING*/, \
				METERING_BASE),

	S5K4ECGX_INIT_CONTROL(V4L2_CID_WHITE_BALANCE_PRESET/*V4L2_CID_CAMERA_WHITE_BALANCE*/, \
				WHITE_BALANCE_AUTO),

	S5K4ECGX_INIT_CONTROL(V4L2_CID_IMAGE_EFFECT/*V4L2_CID_CAMERA_EFFECT*/, \
				IMAGE_EFFECT_NONE),
};

static const struct s5k4ecgx_regs reg_datas = {
	.ev = {
		REGSET(GET_EV_INDEX(EV_MINUS_4), s5k4ecgx_EV_Minus_4),
		REGSET(GET_EV_INDEX(EV_MINUS_3), s5k4ecgx_EV_Minus_3),
		REGSET(GET_EV_INDEX(EV_MINUS_2), s5k4ecgx_EV_Minus_2),
		REGSET(GET_EV_INDEX(EV_MINUS_1), s5k4ecgx_EV_Minus_1),
		REGSET(GET_EV_INDEX(EV_DEFAULT), s5k4ecgx_EV_Default),
		REGSET(GET_EV_INDEX(EV_PLUS_1), s5k4ecgx_EV_Plus_1),
		REGSET(GET_EV_INDEX(EV_PLUS_2), s5k4ecgx_EV_Plus_2),
		REGSET(GET_EV_INDEX(EV_PLUS_3),s5k4ecgx_EV_Plus_3),
		REGSET(GET_EV_INDEX(EV_PLUS_4), s5k4ecgx_EV_Plus_4),
	},
	.metering = {
		REGSET(METERING_MATRIX, s5k4ecgx_Metering_Matrix),
		REGSET(METERING_CENTER, s5k4ecgx_Metering_Center),
		REGSET(METERING_SPOT, s5k4ecgx_Metering_Spot),
	},
	.iso = {
		REGSET(ISO_AUTO, s5k4ecgx_ISO_Auto),
		REGSET(ISO_100, s5k4ecgx_ISO_100),
		REGSET(ISO_200, s5k4ecgx_ISO_200),
		REGSET(ISO_400, s5k4ecgx_ISO_400),
	},
	.effect = {
		REGSET(IMAGE_EFFECT_NONE, s5k4ecgx_Effect_Normal),
		REGSET(IMAGE_EFFECT_BNW, s5k4ecgx_Effect_Black_White),
		REGSET(IMAGE_EFFECT_SEPIA, s5k4ecgx_Effect_Sepia),
		REGSET(IMAGE_EFFECT_NEGATIVE, s5k4ecgx_Effect_Solarization),
	},
	.white_balance = {
		REGSET(WHITE_BALANCE_AUTO, s5k4ecgx_WB_Auto),
		REGSET(WHITE_BALANCE_SUNNY, s5k4ecgx_WB_Sunny),
		REGSET(WHITE_BALANCE_CLOUDY, s5k4ecgx_WB_Cloudy),
		REGSET(WHITE_BALANCE_TUNGSTEN, s5k4ecgx_WB_Tungsten),
		REGSET(WHITE_BALANCE_FLUORESCENT, s5k4ecgx_WB_Fluorescent),
	},
	.scene_mode = {
		REGSET(SCENE_MODE_NONE, s5k4ecgx_scene_off),
		REGSET(SCENE_MODE_NIGHTSHOT, s5k4ecgx_scene_nightshot),
		REGSET(SCENE_MODE_SPORTS, s5k4ecgx_scene_sports),
	},
	.fps = {
		REGSET(I_FPS_0, s5k4ecgx_fps_auto_regs),
		REGSET(I_FPS_15, s5k4ecgx_fps_15_regs),
		REGSET(I_FPS_30, s5k4ecgx_fps_30_regs),
	},
	.preview_size = {
		REGSET(PREVIEW_SZ_CIF, s5k4ecgx_preview_sz_352),
		REGSET(PREVIEW_SZ_VGA, s5k4ecgx_preview_sz_640),
		REGSET(PREVIEW_SZ_PVGA, s5k4ecgx_preview_sz_w1280),
	},

	/* Flash*/
	.flash_start = REGSET_TABLE(s5k4ecgx_Main_Flash_Start_EVT1),
	.flash_end = REGSET_TABLE(s5k4ecgx_Main_Flash_End_EVT1),
	.preflash_start = REGSET_TABLE(s5k4ecgx_Pre_Flash_Start_EVT1),
	.preflash_end = REGSET_TABLE(s5k4ecgx_Pre_Flash_End_EVT1),
	.fast_ae_on = REGSET_TABLE(s5k4ecgx_FAST_AE_On_EVT1),
	.fast_ae_off = REGSET_TABLE(s5k4ecgx_FAST_AE_Off_EVT1),

	/* AWB, AE Lock */
	.ae_lock = REGSET_TABLE(s5k4ecgx_ae_lock_regs),
	.ae_unlock = REGSET_TABLE(s5k4ecgx_ae_unlock_regs),
	.awb_lock = REGSET_TABLE(s5k4ecgx_awb_lock_regs),
	.awb_unlock = REGSET_TABLE(s5k4ecgx_awb_unlock_regs),

	.lowlight_cap_on = REGSET_TABLE(s5k4ecgx_Low_Cap_On),
	.lowlight_cap_off = REGSET_TABLE(s5k4ecgx_Low_Cap_Off),

	/* AF */
	.af_macro_mode = REGSET_TABLE(s5k4ecgx_focus_mode_macro_regs),
	.af_normal_mode = REGSET_TABLE(s5k4ecgx_focus_mode_auto_regs),
#if !defined(CONFIG_MACH_P2)
	/*.af_night_normal_mode = REGSET_TABLE(s5k5ccgx_af_night_normal_on),*/
#endif
	.single_af_pre_start = REGSET_TABLE(s5k4ecgx_single_af_pre_start_regs),
	.single_af_pre_start_macro = REGSET_TABLE(s5k4ecgx_single_af_pre_start_macro_regs),
	.single_af_start = REGSET_TABLE(s5k4ecgx_single_af_start_regs),

	.init = REGSET_TABLE(s5k4ecgx_init_regs),

	.stream_stop = REGSET_TABLE(s5k4ecgx_stream_stop_reg),
	.get_light_level = REGSET_TABLE(s5k4ecgx_get_light_status),
	.get_ae_stable = REGSET_TABLE(s5k4ecgx_get_ae_stable_reg),

	.preview_mode = REGSET_TABLE(s5k4ecgx_preview_reg),
	.update_preview = REGSET_TABLE(s5k4ecgx_update_preview_reg),
	.capture_mode = {
		REGSET(CAPTRUE_SZ_W3MP, s5k4ecgx_capture_W3M),
		REGSET(CAPTURE_SZ_5MP, s5k4ecgx_capture_5M),
	},
	.camcorder_on = {
		REGSET(PREVIEW_SZ_CIF, s5k4ecgx_Camcorder_On_EVT1),
		REGSET(PREVIEW_SZ_VGA, s5k4ecgx_Camcorder_On_EVT1),
		REGSET(PREVIEW_SZ_PVGA, s5k4ecgx_Camcorder_HD_On_EVT1),			
		},
	.camcorder_off = REGSET_TABLE(s5k4ecgx_Camcorder_Off_EVT1),

	.softlanding = REGSET_TABLE(s5k4ecgx_VCM_Off_regs),
	.softlanding_cap = REGSET_TABLE(s5k4ecgx_VCM_Off_cap_regs),
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
 * __s5k4ecgx_i2c_readw: Read 2 bytes from sensor
 */
static int __s5k4ecgx_i2c_readw(struct i2c_client *client,
				  u16 subaddr, u16 *data)
{
	int retries = I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	u8 buf[2];
	struct i2c_msg msg[2];

	cpu_to_be16s(&subaddr);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = (u8 *)&subaddr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
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

	*data = ((buf[0] << 8) | buf[1]);
	return 0;
}

/**
 * __s5k4ecgx_i2c_writew: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int __s5k4ecgx_i2c_writew(struct i2c_client *client,
					 u16 addr, u16 data)
{
	int retries = I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	u8 buf[4] = {0,};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 4,
		.buf	= buf,
	};

	buf[0] = addr >> 8;
	buf[1] = addr;
	buf[2] = data >> 8;
	buf[3] = data & 0xff;

#if (0)
	s5k4ecgx_debug(S5K4ECGX_DEBUG_I2C, "%s : W(0x%02X%02X%02X%02X)\n",
		__func__, buf[0], buf[1], buf[2], buf[3]);
#else
	/* cam_dbg("I2C writing: 0x%02X%02X%02X%02X\n",
			buf[0], buf[1], buf[2], buf[3]); */
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
		cam_err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
			err, addr, data, I2C_RETRY_COUNT - retries);
	}

	CHECK_ERR_COND_MSG(ret != 1, -EIO, "I2C does not work\n\n");
	return 0;
}

static inline int s5k4ecgx_read(struct v4l2_subdev *sd,
				u32 addr, u16 *data, u32 count, u16 flag)

{
	struct s5k4ecgx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const u16 cmd_page_r = 0x002C;
	const u16 cmd_addr_r = 0x002E;
	const u16 cmd_data = 0x0F12;
	u16 page = addr >> 16;
	u16 subaddr = addr & 0xFFFF;
	int i, err = 0;

	mutex_lock(&state->i2c_lock);

	if (flag & S5K_I2C_PAGE) {
		err = __s5k4ecgx_i2c_writew(client, 0xFCFC, 0xD000);
		err |= __s5k4ecgx_i2c_writew(client, cmd_page_r, page);
		CHECK_ERR_GOTO(err, out);
	}

	err = __s5k4ecgx_i2c_writew(client, cmd_addr_r, subaddr);
	CHECK_ERR_GOTO(err, out);

	for (i = 0; i < count; i++, data++) {
		err = __s5k4ecgx_i2c_readw(client, cmd_data, data);
		CHECK_ERR_GOTO(err, out);
	}

out:
	mutex_unlock(&state->i2c_lock);
	return err;
}

static inline int s5k4ecgx_writew(struct v4l2_subdev *sd,
				u32 addr, u16 data, u16 flag)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const u16 cmd_page_w = 0x0028;
	const u16 cmd_addr_w = 0x002A;
	const u16 cmd_data = 0x0F12;
	u16 page = addr >> 16;
	u16 subaddr = addr & 0xFFFF;
	int err = 0;

	mutex_lock(&state->i2c_lock);

	if (flag & S5K_I2C_PAGE) {
		err = __s5k4ecgx_i2c_writew(client, cmd_page_w, page);
		CHECK_ERR_GOTO(err, out);
	}

	err = __s5k4ecgx_i2c_writew(client, cmd_addr_w, subaddr);
	err |= __s5k4ecgx_i2c_writew(client, cmd_data, data);
	CHECK_ERR_GOTO(err, out);

out:
	mutex_unlock(&state->i2c_lock);
	return err;
}

#if CONFIG_LOAD_FILE
static int s5k4ecgx_write_regs_from_sd(struct v4l2_subdev *sd,
						const u8 s_name[])
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct test *tempData = NULL;

	int ret = -EAGAIN;
	u32 temp;
	u32 delay = 0;
	u8 data[11];
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
				/* get 10 strings.*/
				data[0] = '0';
				for (i = 1; i < 11; i++) {
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

		if ((temp & S5K4ECGX_DELAY) == S5K4ECGX_DELAY) {
			delay = temp & 0x0FFFF;
			if (delay != 0xFFFF)
				msleep_debug(delay, true);
			continue;
		}

		/* cam_dbg("I2C writing: 0x%08X,\n",temp);*/
		ret = __s5k4ecgx_i2c_writew(client,
			(temp >> 16), (u16)temp);
		CHECK_ERR_MSG(ret, "write registers\n")
	}

	return ret;
}
#endif

/* Write register
 * If success, return value: 0
 * If fail, return value: -EIO
 */
static int s5k4ecgx_write_regs(struct v4l2_subdev *sd, const u32 regs[],
			     int size)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 delay = 0;
	int i, err = 0;

	mutex_lock(&state->i2c_lock);

	for (i = 0; i < size; i++) {
		if ((regs[i] & S5K4ECGX_DELAY) == S5K4ECGX_DELAY) {
			delay = regs[i] & 0xFFFF;
			if (delay != 0xFFFF)
				msleep_debug(delay, true);

			continue;
		}

		err = __s5k4ecgx_i2c_writew(client,
			(u16)(regs[i] >> 16), (u16)regs[i]);
		CHECK_ERR_GOTO_MSG(err, out, "write registers. i %d\n", i);
	}

out:
	mutex_unlock(&state->i2c_lock);
	return err;
}

#define BURST_MODE_BUFFER_MAX_SIZE /*2700*/ 3700
u8 s5k4ecgx_burstmode_buf[BURST_MODE_BUFFER_MAX_SIZE];

/* PX: */
static int s5k4ecgx_burst_write_regs(struct v4l2_subdev *sd,
			const u32 list[], u32 size, char *name)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	int i = 0, idx = 0;
	u16 subaddr = 0, next_subaddr = 0, value = 0;
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 0,
		.buf	= s5k4ecgx_burstmode_buf,
	};

	cam_trace("E\n");

	mutex_lock(&state->i2c_lock);

	for (i = 0; i < size; i++) {
		if (unlikely(idx > (BURST_MODE_BUFFER_MAX_SIZE - 10))) {
			cam_err("BURST MOD buffer overflow! idx %d\n", idx);
			err = -ENOBUFS;
			goto out;
		}

		subaddr = (list[i] & 0xFFFF0000) >> 16;
		if (subaddr == 0x0F12)
			next_subaddr = (list[i+1] & 0xFFFF0000) >> 16;

		value = list[i] & 0x0000FFFF;

		switch (subaddr) {
		case 0x0F12:
			/* make and fill buffer for burst mode write. */
			if (idx == 0) {
				s5k4ecgx_burstmode_buf[idx++] = 0x0F;
				s5k4ecgx_burstmode_buf[idx++] = 0x12;
			}
			s5k4ecgx_burstmode_buf[idx++] = value >> 8;
			s5k4ecgx_burstmode_buf[idx++] = value & 0xFF;

			/* write in burstmode*/
			if (next_subaddr != 0x0F12) {
				msg.len = idx;
				err = i2c_transfer(client->adapter,
					&msg, 1) == 1 ? 0 : -EIO;
				if (unlikely(err)) {
					stats_i2c_err++;
					cam_err("burst_write_regs: error, i2c_transfer\n");
					goto out;
				}
				/* cam_dbg("s5k4ecgx_sensor_burst_write,
						idx = %d\n", idx); */
				idx = 0;
			}
			break;

		case 0xFFFF:
			if (value != 0xFFFF)
				msleep_debug(value, true);

			break;

		default:
			idx = 0;
			err = __s5k4ecgx_i2c_writew(client,
						subaddr, value);
			CHECK_ERR_GOTO(err, out);
			break;
		}
	}

out:
	mutex_unlock(&state->i2c_lock);
	return err;
}

/* PX: */
static int s5k4ecgx_set_from_table(struct v4l2_subdev *sd,
				const char *setting_name,
				const struct regset_table *table,
				u32 table_size, s32 index)
{
	int err = 0;

	/* cam_dbg("%s: set %s index %d\n",
		__func__, setting_name, index); */
	CHECK_ERR_COND_MSG(((index < 0) || (index >= table_size)),
		-EINVAL, "index(%d) out of range[0:%d] for table for %s\n",
		index, table_size, setting_name);

	table += index;

#if CONFIG_LOAD_FILE
	CHECK_ERR_COND_MSG(!table->name, -EFAULT, \
		"table=%s, index=%d, reg_name = NULL\n", setting_name, index);

	cam_dbg("%s: \"%s\", reg_name=%s\n", __func__,
			setting_name, table->name);
	return s5k4ecgx_write_regs_from_sd(sd, table->name);

#else /* !CONFIG_LOAD_FILE */
	CHECK_ERR_COND_MSG(!table->reg, -EFAULT, \
		"table=%s, index=%d, reg = NULL\n", setting_name, index);

# if DEBUG_WRITE_REGS
	cam_dbg("%s: \"%s\", reg_name=%s\n", __func__,
			setting_name, table->name);
# endif /* DEBUG_WRITE_REGS */

	err = s5k4ecgx_write_regs(sd, table->reg, table->array_size);
	CHECK_ERR_MSG(err, "write regs(%s), err=%d\n", setting_name, err);

	return 0;
#endif /* CONFIG_LOAD_FILE */
}

/* PX: */
static inline int s5k4ecgx_save_ctrl(struct v4l2_subdev *sd,
					struct v4l2_control *ctrl)
{
	int i, ctrl_cnt = ARRAY_SIZE(s5k4ecgx_ctrls);

	cam_trace("ID =0x%08X, val = %d\n", ctrl->id, ctrl->value);

	for (i = 0; i < ctrl_cnt; i++) {
		if (ctrl->id == s5k4ecgx_ctrls[i].id) {
			s5k4ecgx_ctrls[i].value = ctrl->value;
			return 0;
		}
	}

	cam_trace("not saved, ID %d(0x%X)\n", ctrl->id & 0xFF, ctrl->id);
	return 0;
}

static int s5k4ecgx_put_power(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);

	if (S5K4ECGX_HW_POWER_ON == state->power_on) {
		cam_warn_on("put_power: error, still in power-on!!\n");
		return -EPERM;
	}
		
	if (!IS_ERR_OR_NULL(main_cam_sensor_a2v8_regulator))
		regulator_put(main_cam_sensor_a2v8_regulator);

	if (!IS_ERR_OR_NULL(main_cam_af_2v8_regulator))
		regulator_put(main_cam_af_2v8_regulator);

	if (!IS_ERR_OR_NULL(main_cam_core_1v2_regulator))
		regulator_put(main_cam_core_1v2_regulator);

	if (!IS_ERR_OR_NULL(main_cam_io_1v8_regulator))
		regulator_put(main_cam_io_1v8_regulator);

	if (!IS_ERR_OR_NULL(vt_cam_core_1v8_regulator))
		regulator_put(vt_cam_core_1v8_regulator);

	main_cam_sensor_a2v8_regulator = NULL;
	main_cam_af_2v8_regulator = NULL;
	main_cam_core_1v2_regulator = NULL;
	main_cam_io_1v8_regulator = NULL;

	vt_cam_core_1v8_regulator = NULL;

	state->power_on = S5K4ECGX_HW_POWER_NOT_READY;

	return 0;
}

static int s5k4ecgx_get_power(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int ret = 0;

	main_cam_io_1v8_regulator = regulator_get(NULL, "main_cam_io_1v8");
	if (IS_ERR(main_cam_io_1v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_io_1v8");
		ret = -ENODEV;
		goto err_regulator;
	}

	main_cam_core_1v2_regulator = regulator_get(NULL, "main_cam_core_1v2");
	if (IS_ERR(main_cam_core_1v2_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_core_1v2");
		ret = -ENODEV;
		goto err_regulator;
	}

	main_cam_af_2v8_regulator = regulator_get(NULL, "main_cam_af_2v8");
	if (IS_ERR(main_cam_af_2v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_af_2v8");
		ret = -ENODEV;
		goto err_regulator;
	}

	main_cam_sensor_a2v8_regulator = regulator_get(NULL, "main_cam_sensor_a2v8");
	if (IS_ERR(main_cam_sensor_a2v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "main_cam_sensor_a2v8");
		ret = -ENODEV;
		goto err_regulator;
	}

	vt_cam_core_1v8_regulator = regulator_get(NULL, "vt_cam_core_1v8");
	if (IS_ERR(vt_cam_core_1v8_regulator)) {
		pr_info("%s: failed to get %s\n", __func__, "vt_cam_core_1v8");
		ret = -ENODEV;
		goto err_regulator;
	}

	state->power_on = S5K4ECGX_HW_POWER_READY;

	return 0;

err_regulator:
	s5k4ecgx_put_power(sd);
	return ret;
}

#if CONFIG_POWER_STANDBY_SUPPORT
static inline int s5k4ecgx_power_on(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	cam_dbg("power-on(new)\n");

	if (gpio_request(GPIO_CAM_MEGA_nRST, "GPM2_2") < 0) {
		pr_err("failed gpio_request(GPX1_2) for camera control\n");
		return -1;
	}
	/* s3c_gpio_setpull(GPIO_CAM_MEGA_nRST, S3C_GPIO_PULL_NONE); */

	if (gpio_request(GPIO_CAM_MEGA_EN, "GPM4_4") < 0) {
		pr_err("failed gpio_request(GPF1_0) for camera control\n");
		return -1;
	}
	/* s3c_gpio_setpull(GPIO_CAM_MEGA_EN, S3C_GPIO_PULL_NONE); */

	if (gpio_request(GPIO_VT_CAM_MEGA_EN, "GPM1_3") < 0) {
		pr_err("failed gpio_request(GPM1_3) for camera control\n");
		return -1;
	}
	/* s3c_gpio_setpull(GPIO_VT_CAM_MEGA_EN, S3C_GPIO_PULL_NONE); */
	
	if (gpio_request(GPIO_VT_CAM_MEGA_nRST, "GPM1_2") < 0) {
		pr_err("failed gpio_request(GPM1_2) for camera control\n");
		return -1;
	}
	/* s3c_gpio_setpull(GPIO_VT_CAM_MEGA_nRST, S3C_GPIO_PULL_NONE); */
	

	/* Sensor A2.8v */
	err = regulator_enable(main_cam_sensor_a2v8_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	/* VT Core 1.5v */
	err = regulator_enable(vt_cam_core_1v8_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	/* Sensor I/O 1.8v */
	err = regulator_enable(main_cam_io_1v8_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(50);

	/* VT STBY high*/
	gpio_direction_output(GPIO_VT_CAM_MEGA_EN, 1);
	udelay(10);

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
	usleep_range(4000, 4100);
	
	/* VT RST high */
	gpio_direction_output(GPIO_VT_CAM_MEGA_nRST, 1);
	usleep_range(1000, 1100);

	/* VT STBY low*/
	gpio_direction_output(GPIO_VT_CAM_MEGA_EN, 0);
	udelay(10);

	/* 5M core 1.2v */
	err = regulator_enable(main_cam_core_1v2_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(200);

	/* 5M STBY high */
	err = gpio_direction_output(GPIO_CAM_MEGA_EN, 1);
	if (unlikely(err)) {
		pr_err("%s: fail to enable MEGA_EN (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(200);

	/* 5M RST high */
	err = gpio_direction_output(GPIO_CAM_MEGA_nRST, 1);
	if (unlikely(err)) {
		pr_err("%s: fail to enable MEGA_nRST (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(16);

	err = regulator_enable(main_cam_af_2v8_regulator);
	if (unlikely(err)) {
		pr_err("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}

#if 0
	// PCLK high
	err = s3c_gpio_cfgpin(GPIO_CAM_PCLK, S3C_GPIO_SFN(3));//PCLK
	if (unlikely(err)) {
		printk("%s: fail to enable PCLK (%d line)\n",
			__func__, __LINE__);
		return err;
	}
#endif

	gpio_free(GPIO_CAM_MEGA_nRST);
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_VT_CAM_MEGA_EN);
	gpio_free(GPIO_VT_CAM_MEGA_nRST);

	return 0;
}

#else
static int s5k4ecgx_power_on(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	//powerdown
	if (gpio_request(GPIO_CAM_MEGA_nRST, "GPM2_2") < 0)
		pr_err("failed gpio_request(GPX1_2) for camera control\n");

	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	s3c_gpio_setpull(GPIO_CAM_MEGA_nRST, S3C_GPIO_PULL_NONE);

	if (gpio_request(GPIO_CAM_MEGA_EN, "GPM4_4") < 0)
		pr_err("failed gpio_request(GPF1_0) for camera control\n");

	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	s3c_gpio_setpull(GPIO_CAM_MEGA_EN, S3C_GPIO_PULL_NONE);

	err = regulator_enable(main_cam_sensor_a2v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	err = regulator_enable(main_cam_af_2v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	err = regulator_enable(main_cam_core_1v2_regulator);
	if (unlikely(err)) {
		printk("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	err = regulator_enable(main_cam_io_1v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to enable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	cam_mipi_en = true;

#if 1
	clk_enable(state->mclk);
#else
	err = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(3));//MCLK
	if (unlikely(err)) {
		printk("%s: fail to enable MCLK (%d line)\n",
			__func__, __LINE__);
		return err;
	}
#endif
	mdelay(1);

	// STBYN high
	err = gpio_direction_output(GPIO_CAM_MEGA_EN, 1);
	if (unlikely(err)) {
		printk("%s: fail to enable MEGA_EN (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	mdelay(1);

	// RSTN high
	err = gpio_direction_output(GPIO_CAM_MEGA_nRST, 1);
	if (unlikely(err)) {
		printk("%s: fail to enable MEGA_nRST (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	mdelay(1);

/*
	// PCLK high
	err = s3c_gpio_cfgpin(GPIO_CAM_PCLK, S3C_GPIO_SFN(3));//PCLK
	if (unlikely(err)) {
		printk("%s: fail to enable PCLK (%d line)\n",
			__func__, __LINE__);
		return err;
	}*/

	gpio_free(GPIO_CAM_MEGA_nRST);
	gpio_free(GPIO_CAM_MEGA_EN);

	return 0;
}
#endif

#if CONFIG_POWER_STANDBY_SUPPORT
static inline int s5k4ecgx_power_off(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	cam_dbg("power-off(new)\n");

	if (gpio_request(GPIO_CAM_MEGA_nRST, "GPM2_2") < 0) {
		pr_err("failed gpio_request(GPF1_4) for camera control\n");
		return -1;
	}

	if (gpio_request(GPIO_CAM_MEGA_EN, "GPM4_4") < 0) {
		pr_err("failed gpio_request(GPF1_5) for camera control\n");
		return -1;
	}

	/*if (gpio_request(GPIO_VT_CAM_MEGA_EN, "GPM1_3") < 0) {
		pr_err("failed gpio_request(GPM1_3) for camera control\n");
		return -1;
	}
	s3c_gpio_setpull(GPIO_VT_CAM_MEGA_EN, S3C_GPIO_PULL_NONE);*/
	
	if (gpio_request(GPIO_VT_CAM_MEGA_nRST, "GPM1_2") < 0) {
		pr_err("failed gpio_request(GPM1_2) for camera control\n");
		return -1;
	}
	s3c_gpio_setpull(GPIO_VT_CAM_MEGA_nRST, S3C_GPIO_PULL_NONE);

	/* 5M RST low */
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	udelay(60);

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

	/* 5M STBY low */
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	udelay(11);

	/* VT RST low */
	gpio_direction_output(GPIO_VT_CAM_MEGA_nRST, 0);
	udelay(1);

	/* Sensor I/O 1.8v*/
	err = regulator_disable(main_cam_io_1v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(3);

	/* VT Core 1.5v */
	err = regulator_disable(vt_cam_core_1v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(3);

	/* Sensor A2.8v */
	err = regulator_disable(main_cam_sensor_a2v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(3);

	/* 5M core 1.2v */
	err = regulator_disable(main_cam_core_1v2_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	
	err = regulator_disable(main_cam_af_2v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}

	gpio_free(GPIO_CAM_MEGA_nRST);
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_VT_CAM_MEGA_nRST);

	return 0;
}

#else
static int s5k4ecgx_power_off(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	if (!state->power_on) {
		cam_info("power already off\n");
		return 0;
	}

	if (gpio_request(GPIO_CAM_MEGA_nRST, "GPM2_2"/*"GPJ1"*/) < 0)
		pr_err("failed gpio_request(GPF1_4) for camera control\n");

	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	mdelay(1);
	// STBYN high
	if (gpio_request(GPIO_CAM_MEGA_EN, "GPM4_4"/*"GPJ0"*/) < 0)
		pr_err("failed gpio_request(GPF1_5) for camera control\n");
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	mdelay(1);

	gpio_free(GPIO_CAM_MEGA_nRST);
	gpio_free(GPIO_CAM_MEGA_EN);

	clk_disable(state->mclk);
	udelay(30);

	err = regulator_disable(main_cam_sensor_a2v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	err = regulator_disable(main_cam_af_2v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	err = regulator_disable(main_cam_core_1v2_regulator);
			if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	err = regulator_disable(main_cam_io_1v8_regulator);
	if (unlikely(err)) {
		printk("%s: fail to disable regulator (%d line)\n",
			__func__, __LINE__);
		return err;
	}
	udelay(10);

	state->power_on = S5K4ECGX_HW_POWER_OFF;

	return 0;
}
#endif

#ifdef CONFIG_MACH_TAB3
static int s5k4ecgx_power(struct v4l2_subdev *sd, int on)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_platform_data *pdata = state->pdata;
	int err = 0;

	if (pdata->common.power) {
		err = pdata->common.power(on);
		CHECK_ERR_MSG(err, "failed to power(%d)\n", on);
	}

	return 0;
}

#else

static int s5k4ecgx_power(struct v4l2_subdev *sd, int on)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	cam_info("power %s\n", on ? "on" : "off");

	CHECK_ERR_COND_MSG((S5K4ECGX_HW_POWER_NOT_READY == state->power_on),
			-EPERM, "power %d not ready\n", on)
		
	if (on) {
		if (S5K4ECGX_HW_POWER_ON == state->power_on)
			cam_err("power already on\n");

		err = s5k4ecgx_power_on(sd);
		CHECK_ERR_MSG(err, "fail to power on %d\n", err);

		state->power_on = S5K4ECGX_HW_POWER_ON;
	} else {
		if (S5K4ECGX_HW_POWER_OFF == state->power_on)
			cam_info("power already off\n");
		else {
#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
			err = work_busy(&state->streamoff_work);
#endif
#ifdef CONFIG_VIDEO_FAST_CAPTURE
			err |= work_busy(&state->capmode_check);
#endif
			if (unlikely(err))
				cam_err("power off: warn, work not finished yet\n");
		}

		err = s5k4ecgx_power_off(sd);
		CHECK_ERR_MSG(err, "fail to power off %d\n", err);

		state->power_on = S5K4ECGX_HW_POWER_OFF;
		state->initialized = 0;
	}

	return 0;
}
#endif

static inline int __used
s5k4ecgx_transit_preview_mode(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	if (0 /*state->hd_videomode*/)
		return 0;

	if (state->runmode == RUNMODE_CAPTURING_STOP) {
		if (SCENE_MODE_NIGHTSHOT == state->scene_mode) {
			state->pdata->common.streamoff_delay = 
				S5K4ECGX_STREAMOFF_DELAY;
		}
	}

	err = s5k4ecgx_set_from_table(sd, "preview_mode",
			&state->regs->preview_mode, 1, 0);
	CHECK_ERR(err);

	state->need_preview_update = 0;
	return 0;
}

/**
 * s5k4ecgx_transit_half_mode: go to a half-release mode
 *
 * Don't forget that half mode is not used in movie mode.
 */
static inline int __used
s5k4ecgx_transit_half_mode(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);

	/* We do not go to half release mode in movie mode */
	if (state->sensor_mode == SENSOR_MOVIE)
		return 0;

	/*
	 * Write the additional codes here
	 */
	return 0;
}

static inline int __used
s5k4ecgx_transit_capture_mode(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);

	return s5k4ecgx_set_from_table(sd, "capture_mode",
			state->regs->capture_mode,
			ARRAY_SIZE(state->regs->capture_mode),
			state->capture.frmsize->index);
}

/**
 * s5k4ecgx_transit_movie_mode: switch camera mode if needed.
 *
 * Note that this fuction should be called from start_preview().
 */
static inline int __used
s5k4ecgx_transit_movie_mode(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
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
		err = s5k4ecgx_set_from_table(sd, "camcorder_on",
				state->regs->camcorder_on,
				ARRAY_SIZE(state->regs->camcorder_on),
				state->preview.frmsize->index);
		CHECK_ERR(err);

		s5k4ecgx_restore_ctrl(sd);
		break;

	case RUNMODE_RECORDING_STOP:
		/* case of switching from camcorder to camera */
		if (SENSOR_CAMERA == state->sensor_mode) {
			cam_dbg("switching to camera mode\n");
			err = s5k4ecgx_set_from_table(sd, "camcorder_off",
				&state->regs->camcorder_off, 1, 0);
			CHECK_ERR(err);

			s5k4ecgx_restore_ctrl(sd);
		}
		break;

	default:
		break;
	}

	return 0;
}

#ifdef CONFIG_MACH_GARDA
static void __hwflash_flashtimer_handler(unsigned long data)
{
	int ret = -ENODEV;

	cam_info("********** flashtimer_handler **********\n");

	ret = gpio_direction_output(GPIO_CAM_FLASH_EN, 0);
	ret |= gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
	if (unlikely(ret))
		cam_err("flash_timer: ERROR, failed to oneshot flash off\n");

	atomic_set(&flash_status, S5K4ECGX_FLASH_OFF);
}

static int __is_hwflash_on(void)
{
	return atomic_read(&flash_status);
}

static int __hwflash_init(int val) {
	int err = 0;

	err = gpio_direction_output(GPIO_CAM_FLASH_EN, 0);
	CHECK_ERR(err);

	err = gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
	CHECK_ERR(err);

	spin_lock_init(&flash_op_lock);

	return err;
}

static int __hwflash_set_register(int reg_no, int val)
{
#ifdef CONFIG_DEBUG_FLASH_SET_INTERVAL
	struct s5k4ecgx_time_interval flash_interval;
	s32 interval = 0;
	bool log = false;
#endif
	unsigned long flags;
	int i, err;

	cam_dbg("hwflash: register %d, val %d\n", reg_no, val);

	/* Go to idle mode from shutdown mode and make start condition.*/
	err = gpio_direction_output(GPIO_CAM_FLASH_SET, 1);
	CHECK_ERR(err);

	spin_lock_irqsave(&flash_op_lock, flags);

#ifdef CONFIG_DEBUG_FLASH_SET_INTERVAL
	do_gettimeofday(&flash_interval.start);
#endif

	/* Set register number */
	for (i = 0; i <= reg_no; i++) {
		udelay(1);
		err = gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
		CHECK_ERR_GOTO(err, error);

		udelay(1);
		err = gpio_direction_output(GPIO_CAM_FLASH_SET, 1);
		CHECK_ERR_GOTO(err, error);
	}
	udelay(150);

	/* Set data written to register */
	for (i = 0; i <= val; i++) {
		udelay(1);
		err= gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
		CHECK_ERR_GOTO(err, error);

		udelay(1);
		err =gpio_direction_output(GPIO_CAM_FLASH_SET, 1);
		CHECK_ERR_GOTO(err, error);
	}
#ifdef CONFIG_DEBUG_FLASH_SET_INTERVAL
	do_gettimeofday(&flash_interval.end);
#endif

	spin_unlock_irqrestore(&flash_op_lock, flags);
	udelay(410);

#ifdef CONFIG_DEBUG_FLASH_SET_INTERVAL
	interval = GET_ELAPSED_TIME(flash_interval.start, flash_interval.end);
	switch (reg_no) {
	case LB_TH_REG:
		if (interval >= 190)
			log = true;
		break;
	case TEN_TCUR_REG:
		if (interval >= 280)
			log = true;
		break;
	default:
		break;
	}

	if (log) {
		cam_err("hwflash interval: %d us. (reg %d, val %d)\n",
			interval, reg_no, val);
	}
#endif

	/* Idle mode after latching */
	return 0;

error:
	spin_unlock_irqrestore(&flash_op_lock, flags);
	cam_err("hwflash set_register: error %d, reg %d, val %d\n", err, reg_no, val);
	return err;
}

static int __hwflash_en(u32 mode, u32 on)
{
	static int flash_mode = S5K4ECGX_FLASH_MODE_NORMAL;
	static DEFINE_MUTEX(flash_lock);
	static DEFINE_TIMER(flash_timer, __hwflash_flashtimer_handler,
			0, (unsigned long)&flash_status);
	int err = 0;

	cam_info("hwflash(new1): %s (mode %d)\n", on ? "on" : "off", mode);

	mutex_lock(&flash_lock);
	if (atomic_read(&flash_status) == on) {
		mutex_unlock(&flash_lock);
		cam_info("hwflash: warning, already flash %s (mode %d)\n",
			on ? "On" : "Off", flash_mode);
		return 0;
	}

	switch (on) {
	case S5K4ECGX_FLASH_ON:
		/* Go to idle mode from shutdown mode */
		err = gpio_direction_output(GPIO_CAM_FLASH_SET, 1);
		CHECK_ERR_GOTO_MSG(err, out, "hwflash: fail to go to idle. %d\n", err);
		udelay(10);
		
		/* Disable low battery threshold */
		err = __hwflash_set_register(LB_TH_REG,0);
		CHECK_ERR_GOTO(err, out);

		/* Flash On */
		if (S5K4ECGX_FLASH_MODE_MOVIE == mode) {
			/* Set torch current. 16 (high) ~ 31 (low)  */
			err = __hwflash_set_register(TEN_TCUR_REG, 18); /* 21 -> 18 */
		} else {
			err = gpio_direction_output(GPIO_CAM_FLASH_EN, 1);
			flash_timer.expires = get_jiffies_64() + (6 * HZ / 10);
			add_timer(&flash_timer);
		}
		CHECK_ERR_GOTO_MSG(err, out,
			"hwflash: error, fail to turn flash on. mode %d\n", mode);

		flash_mode = mode;
		break;

	case S5K4ECGX_FLASH_OFF:
		if (flash_mode != mode) {
			cam_warn_on("hwflash: error, unmatched flash mode(%d, %d)\n",
				flash_mode, mode);
			err = -EPERM;
			goto out;
		}

		/* Flash Off */
		if (S5K4ECGX_FLASH_MODE_MOVIE == mode) {
			err = gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
		} else {
			cam_info("hwflash: terminate flash timer...\n");
			del_timer_sync(&flash_timer);
			err = gpio_direction_output(GPIO_CAM_FLASH_EN, 0);
			err |= gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
		}
		CHECK_ERR_GOTO_MSG(err, out,
			"hwflash: error, flash off. mode %d\n", mode);
		break;

	default:
		cam_err("hwflash: error, invalid flash cmd %d\n", on);
		goto out;
		break;
	}

	atomic_set(&flash_status, on);

	mutex_unlock(&flash_lock);
	return 0;

out:
	gpio_direction_output(GPIO_CAM_FLASH_EN, 0);
	gpio_direction_output(GPIO_CAM_FLASH_SET, 0);

	mutex_unlock(&flash_lock);
	return err;
}
#endif /* CONFIG_MACH_GARDA */

/**
 * s5k4ecgx_is_hwflash_on - check whether flash device is on
 *
 * Refer to state->flash.on to check whether flash is in use in driver.
 */
static inline int s5k4ecgx_is_hwflash_on(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_platform_data *pdata = state->pdata;
	int err = 0;

	if (!IS_FLASH_SUPPORTED())
		return 0;

	if (pdata->is_flash_on)
		err = pdata->is_flash_on();
		
	return err;
}

/**
 * s5k4ecgx_flash_en - contro Flash LED
 * @mode: S5K4ECGX_FLASH_MODE_NORMAL or S5K4ECGX_FLASH_MODE_MOVIE
 * @onoff: S5K4ECGX_FLASH_ON or S5K4ECGX_FLASH_OFF
 */
static int s5k4ecgx_flash_en(struct v4l2_subdev *sd, s32 mode, s32 onoff)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_platform_data *pdata = state->pdata;
	int err = 0;

	if (!IS_FLASH_SUPPORTED() || state->flash.ignore_flash) {
		cam_info("flash_en: ignore %d\n", state->flash.ignore_flash);
		return 0;
	}

	if (pdata->common.flash_en)
		err = pdata->common.flash_en(mode, onoff);
		
	return err;
}

/**
 * s5k4ecgx_flash_torch - turn flash on/off as torch for preflash, recording
 * @onoff: S5K4ECGX_FLASH_ON or S5K4ECGX_FLASH_OFF
 *
 * This func set state->flash.on properly.
 */
static inline int s5k4ecgx_flash_torch(struct v4l2_subdev *sd, s32 onoff)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	if (!IS_FLASH_SUPPORTED())
		return 0;

	/* On/Off flash */
	err = s5k4ecgx_flash_en(sd, S5K4ECGX_FLASH_MODE_MOVIE, onoff);
	if (unlikely(!err))
		state->flash.on = (onoff == S5K4ECGX_FLASH_ON) ? 1 : 0;

	return err;
}

/**
 * s5k4ecgx_flash_oneshot - turn main flash on for capture
 * @onoff: S5K4ECGX_FLASH_ON or S5K4ECGX_FLASH_OFF
 *
 * Main flash is turn off automatically in some milliseconds.
 */
static inline int s5k4ecgx_flash_oneshot(struct v4l2_subdev *sd, s32 onoff)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	if (!IS_FLASH_SUPPORTED())
		return 0;

	/* On/Off flash */
	err = s5k4ecgx_flash_en(sd, S5K4ECGX_FLASH_MODE_NORMAL, onoff);
	if (unlikely(!err)) {
		/* The flash_on here is only used for EXIF */
		state->flash.on = (onoff == S5K4ECGX_FLASH_ON) ? 1 : 0;
	}

	return err;
}

/* PX: Set scene mode */
static int s5k4ecgx_set_scene_mode(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -ENODEV;

	cam_trace("E, value %d\n", val);

	if (state->scene_mode == val)
		return 0;

	/* when scene mode is switched, we frist have to reset */
	if (val > SCENE_MODE_NONE) {
		err = s5k4ecgx_set_from_table(sd, "scene_mode",
				state->regs->scene_mode,
				ARRAY_SIZE(state->regs->scene_mode),
				SCENE_MODE_NONE);
	}

//retry:
	switch (val) {
	case SCENE_MODE_NONE:
		s5k4ecgx_set_from_table(sd, "scene_mode",
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
		s5k4ecgx_set_from_table(sd, "scene_mode",
			state->regs->scene_mode,
			ARRAY_SIZE(state->regs->scene_mode), val);
		break;

	default:
		cam_dbg("set_scene: value is (%d)\n", val);
		return 0;//yuncam
		//val = SCENE_MODE_NONE;
		//goto retry;
	}

	state->scene_mode = val;

	cam_trace("X\n");
	return 0;
}

static int s5k4ecgx_set_metering(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

retry:
	switch (val) {
	case METERING_MATRIX:
	case METERING_CENTER:
	case METERING_SPOT:
		err = s5k4ecgx_set_from_table(sd, "metering",
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
static int s5k4ecgx_set_exposure(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EINVAL;
	cam_info("%s: val = %d\n", __func__, val);

	if (val < EV_MINUS_4)
		val = EV_MINUS_4;
	else if (val > EV_PLUS_4)
		val = EV_PLUS_4;

	err = s5k4ecgx_set_from_table(sd, "brightness", state->regs->ev,
		ARRAY_SIZE(state->regs->ev), GET_EV_INDEX(val));

	state->exposure.val = val;

	return err;
}

/**
 * s5k4ecgx_set_whitebalance - set whilebalance
 * @val:
 */
static int s5k4ecgx_set_whitebalance(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	cam_trace("E, value %d\n", val);

retry:
	switch (val) {
	case WHITE_BALANCE_AUTO:
	case WHITE_BALANCE_SUNNY:
	case WHITE_BALANCE_CLOUDY:
	case WHITE_BALANCE_TUNGSTEN:
	case WHITE_BALANCE_FLUORESCENT:
		err = s5k4ecgx_set_from_table(sd, "white balance",
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

/* PX: Check light level */
static u32 s5k4ecgx_get_light_level(struct v4l2_subdev *sd, u32 *light_level)
{
	u16 buf[2] = {0,};
	u16 *val_lsb = &buf[0], *val_msb = &buf[1];
	int err = -ENODEV;

	err = s5k4ecgx_read(sd, REG_LIGHT_STATUS, buf, 2, S5K_I2C_PAGE);
	CHECK_ERR_MSG(err, "fail to read light level\n");

	*light_level = *val_lsb | (*val_msb << 16);

	/* cam_dbg("light level = 0x%X", *light_level); */

	return 0;
}

/* PX: Set sensor mode */
static int s5k4ecgx_set_sensor_mode(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);

	cam_dbg("sensor_mode: %d\n", val);

#ifdef CONFIG_MACH_PX
	state->hd_videomode = 0;
#endif

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

#ifdef CONFIG_MACH_PX
	case 2:	/* 720p HD video mode */
		state->sensor_mode = SENSOR_MOVIE;
		state->hd_videomode = 1;
		break;
#endif

	default:
		cam_err("%s: error, not support.(%d)\n", __func__, val);
		state->sensor_mode = SENSOR_CAMERA;
		WARN_ON(1);
		break;
	}

	return 0;
}

static int s5k4ecgx_set_frame_rate(struct v4l2_subdev *sd, s32 fps)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int i = 0, fps_index = -1;
	int min = FRAME_RATE_AUTO;
	int max = FRAME_RATE_30;
	int err = -EIO;

	if (state->scene_mode != SCENE_MODE_NONE)
		return 0;

	cam_info("set frame rate %d\n", fps);

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

	for (i = 0; i < ARRAY_SIZE(s5k4ecgx_framerates); i++) {
		if (fps == s5k4ecgx_framerates[i].fps) {
			fps_index = s5k4ecgx_framerates[i].index;
			state->fps = fps;
			state->req_fps = -1;
			break;
		}
	}

	if (unlikely(fps_index < 0)) {
		cam_err("set_frame_rate: not supported fps %d\n", fps);
		return 0;
	}

	err = s5k4ecgx_set_from_table(sd, "fps", state->regs->fps,
			ARRAY_SIZE(state->regs->fps), fps_index);
	CHECK_ERR_MSG(err, "fail to set framerate\n");

	return 0;
}

static int s5k4ecgx_set_ae_lock(struct v4l2_subdev *sd, s32 val, bool force)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	switch (val) {
	case AE_LOCK:
		err = s5k4ecgx_set_from_table(sd, "ae_lock",
				&state->regs->ae_lock, 1, 0);
		state->exposure.ae_lock = 1;
		cam_info("AE lock by user\n");
		break;

	case AE_UNLOCK:
		if (unlikely(!force && !state->exposure.ae_lock))
			return 0;

		err = s5k4ecgx_set_from_table(sd, "ae_unlock",
				&state->regs->ae_unlock, 1, 0);
		state->exposure.ae_lock = 0;
		cam_info("AE unlock\n");
		break;

	default:
		cam_err("ae_lock: warning, invalid argument(%d)\n", val);
	}

	CHECK_ERR_MSG(err, "fail to lock AE(%d), err %d\n", val, err);

	return 0;
}

static int s5k4ecgx_set_awb_lock(struct v4l2_subdev *sd, s32 val, bool force)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	switch (val) {
	case AWB_LOCK:
		if ((state->wb.mode != WHITE_BALANCE_AUTO) ||
		    state->flash.on)
			return 0;

		err = s5k4ecgx_set_from_table(sd, "awb_lock",
				&state->regs->awb_lock, 1, 0);
		state->wb.awb_lock = 1;
		cam_info("AWB lock by user\n");
		break;

	case AWB_UNLOCK:
		if (unlikely(!force && !state->wb.awb_lock))
			return 0;
		if (state->wb.mode != WHITE_BALANCE_AUTO) /* Fix me */
			return 0;

		err = s5k4ecgx_set_from_table(sd, "awb_unlock",
			&state->regs->awb_unlock, 1, 0);
		state->wb.awb_lock = 0;
		cam_info("AWB unlock\n");
		break;

	default:
		cam_err("awb_lock: warning, invalid argument(%d)\n", val);
	}

	CHECK_ERR_MSG(err, "fail to lock AWB(%d), err %d\n", val, err);

	return 0;
}

/* PX: Set AE, AWB Lock */
static int s5k4ecgx_set_lock(struct v4l2_subdev *sd, s32 lock, bool force)
{
	int err = -EIO;

	if (unlikely((u32)lock >= AEAWB_LOCK_MAX)) {
		cam_err("%s: error, invalid argument\n", __func__);
		return -EINVAL;
	}

	err = s5k4ecgx_set_ae_lock(sd, (lock == AEAWB_LOCK) ?
			AE_LOCK : AE_UNLOCK, force);
	CHECK_ERR_GOTO(err, out_err);

	err = s5k4ecgx_set_awb_lock(sd, (lock == AEAWB_LOCK) ?
			AWB_LOCK : AWB_UNLOCK, force);
	CHECK_ERR_GOTO(err, out_err);

	return 0;

out_err:
	cam_err("set_lock: error, fail\n");
	return err;
}

#if 0
static int s5k4ecgx_return_focus(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EINVAL;

	if (!IS_AF_SUPPORTED())
		return 0;

	cam_trace("E\n");

	switch (state->focus.mode) {
	case FOCUS_MODE_MACRO:
		err = s5k4ecgx_set_from_table(sd, "af_macro_mode",
				&state->regs->af_macro_mode, 1, 0);
		break;

	default:
		err = s5k4ecgx_set_from_table(sd, "af_norma_mode",
				&state->regs->af_normal_mode, 1, 0);
		break;
	}

	CHECK_ERR(err);
	return 0;
}
#endif

#ifdef DEBUG_FILTER_DATA
static void __used s5k4ecgx_display_AF_win_info(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_rect first_win = {0, 0, 0, 0};
	struct s5k4ecgx_rect second_win = {0, 0, 0, 0};

	__s5k4ecgx_i2c_writew(client, 0x002C, 0x7000);
	__s5k4ecgx_i2c_writew(client, 0x002E, 0x022C);
	__s5k4ecgx_i2c_readw(client, 0x0F12, (u16 *)&first_win.x);
	__s5k4ecgx_i2c_writew(client, 0x002C, 0x7000);
	__s5k4ecgx_i2c_writew(client, 0x002E, 0x022E);
	__s5k4ecgx_i2c_readw(client, 0x0F12, (u16 *)&first_win.y);
	__s5k4ecgx_i2c_writew(client, 0x002C, 0x7000);
	__s5k4ecgx_i2c_writew(client, 0x002E, 0x0230);
	__s5k4ecgx_i2c_readw(client, 0x0F12, (u16 *)&first_win.width);
	__s5k4ecgx_i2c_writew(client, 0x002C, 0x7000);
	__s5k4ecgx_i2c_writew(client, 0x002E, 0x0232);
	__s5k4ecgx_i2c_readw(client, 0x0F12, (u16 *)&first_win.height);

	__s5k4ecgx_i2c_writew(client, 0x002C, 0x7000);
	__s5k4ecgx_i2c_writew(client, 0x002E, 0x0234);
	__s5k4ecgx_i2c_readw(client, 0x0F12, (u16 *)&second_win.x);
	__s5k4ecgx_i2c_writew(client, 0x002C, 0x7000);
	__s5k4ecgx_i2c_writew(client, 0x002E, 0x0236);
	__s5k4ecgx_i2c_readw(client, 0x0F12, (u16 *)&second_win.y);
	__s5k4ecgx_i2c_writew(client, 0x002C, 0x7000);
	__s5k4ecgx_i2c_writew(client, 0x002E, 0x0238);
	__s5k4ecgx_i2c_readw(client, 0x0F12, (u16 *)&second_win.width);
	__s5k4ecgx_i2c_writew(client, 0x002C, 0x7000);
	__s5k4ecgx_i2c_writew(client, 0x002E, 0x023A);
	__s5k4ecgx_i2c_readw(client, 0x0F12, (u16 *)&second_win.height);

	cam_info("------- AF Window info -------\n");
	cam_info("Firtst Window: (%4d %4d %4d %4d)\n",
		first_win.x, first_win.y, first_win.width, first_win.height);
	cam_info("Second Window: (%4d %4d %4d %4d)\n",
		second_win.x, second_win.y,
		second_win.width, second_win.height);
	cam_info("------- AF Window info -------\n\n");
}
#endif /* DEBUG_FILTER_DATA */

#ifdef CONFIG_DEBUG_FLASH_AE
int __print_ae_thread(void *data)
{
	struct v4l2_subdev *sd = data;
	u16 buf[2];

	while(1) {
		if (kthread_should_stop()) {
			cam_info("[AE-CHECK] stopped!\n");
			break;
		}

		buf[0] = buf[1] = 0xDEAD;
		s5k4ecgx_read(sd, REG_MON_AAIO_ME_LEI_EXP, buf, 2, S5K_I2C_PAGE);
		cam_dbg("[AE-CHECK] exposure msb 0x%04X, lsb 0x%04X\n", buf[1], buf[0]);

		msleep_debug(10, false);
	}

	return 0;
}
#endif

static int s5k4ecgx_af_start_preflash(struct v4l2_subdev *sd,
				u32 touch, u32 flash_mode)
{
	struct s5k4ecgx_state *state = to_state(sd);
	u16 read_value = 0;
	int count = 0;
	int err = 0;

	cam_trace("E\n");

	if (state->sensor_mode == SENSOR_MOVIE)
		return 0;

	cam_dbg("Start SINGLE AF, touch %d, flash mode %d, status 0x%X\n",
		touch, flash_mode, state->focus.status);

	/* in case user calls auto_focus repeatedly without a cancel
	 * or a capture, we need to cancel here to allow ae_awb
	 * to work again, or else we could be locked forever while
	 * that app is running, which is not the expected behavior.
	 */
	err = s5k4ecgx_set_lock(sd, AEAWB_UNLOCK, true);
	CHECK_ERR_MSG(err, "fail to set lock\n");

	state->flash.preflash = PREFLASH_OFF;
	state->light_level = LUX_LEVEL_MAX;

	s5k4ecgx_get_light_level(sd, &state->light_level);

	switch (flash_mode) {
	case FLASH_MODE_AUTO:
		if (state->light_level >= LUX_LEVEL_LOW) {
			/* flash not needed */
			break;
		}

	case FLASH_MODE_ON:
		s5k4ecgx_set_from_table(sd, "fast_ae_on",
			&state->regs->fast_ae_on, 1, 0);
		s5k4ecgx_set_from_table(sd, "preflash_start",
			&state->regs->preflash_start, 1, 0);
#ifdef CONFIG_DEBUG_FLASH_AE
		state->print_ae_thread = kthread_run(__print_ae_thread, sd,
						"s5k4ecgx: debug_ae");
		if (IS_ERR_OR_NULL(state->print_ae_thread)) {
			cam_err("start_preflash: error, fail to run thread %ld",
				PTR_ERR(state->print_ae_thread));
			state->print_ae_thread = NULL;
		}
#endif
		s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_ON);
		state->flash.preflash = PREFLASH_ON;
		break;

	case FLASH_MODE_OFF:
		if (state->light_level < LUX_LEVEL_LOW)
			state->one_frame_delay_ms = ONE_FRAME_DELAY_MS_LOW;
		break;

	default:
		break;
	}

	/* Check AE-stable */
	if (state->flash.preflash == PREFLASH_ON) {
		/* We wait for 200ms after pre flash on.
		 * check whether AE is stable.*/
		msleep_debug(300, true); /* 200 -> 250 -> 300 for fixing flash problem */

		/* Do checking AE-stable */
		for (count = 0; count < AE_STABLE_SEARCH_COUNT; count++) {
			if (state->focus.start == AUTO_FOCUS_OFF) {
				cam_info("af_start_preflash: \
					AF is cancelled!\n");
				state->focus.status = AF_RESULT_CANCELLED;
				break;
			}

			read_value = 0x0;
			s5k4ecgx_readw(sd, REG_MON_AE_STABLE, &read_value,
					S5K_I2C_PAGE);
			/* af_dbg("Check AE-Stable: 0x%04X\n", read_value); */
			if (read_value == 0x0001) {
				af_dbg("AE-stable success. count %d, delay %dms\n",
					count, AE_STABLE_SEARCH_DELAY);
				break;
			}

			msleep_debug(AE_STABLE_SEARCH_DELAY, false);
		}

		/* restore write mode */
		/*__s5k4ecgx_i2c_writew(client, 0x0028, 0x7000);*/

		if (unlikely(count >= AE_STABLE_SEARCH_COUNT)) {
			cam_err("start_preflash: fail, AE unstable\n");
			/* return -ENODEV; */
		}
	} else if (state->focus.start == AUTO_FOCUS_OFF) {
		cam_info("af_start_preflash: AF is cancelled!\n");
		state->focus.status = AF_RESULT_CANCELLED;
	}

#ifdef CONFIG_DEBUG_FLASH_AE
	if (state->print_ae_thread) {
		kthread_stop(state->print_ae_thread);
		state->print_ae_thread = NULL;
	}
#endif

	/* If AF cancel, finish pre-flash process. */
	if (state->focus.status == AF_RESULT_CANCELLED) {
		if (state->flash.preflash == PREFLASH_ON) {
			s5k4ecgx_set_from_table(sd, "fast_ae_off",
				&state->regs->fast_ae_off, 1, 0);
			s5k4ecgx_set_from_table(sd, "preflash_end",
				&state->regs->preflash_end, 1, 0);
			s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_OFF);
			state->flash.preflash = PREFLASH_NONE;
		}
	}

	cam_trace("X\n");

	return 0;
}

static int s5k4ecgx_do_af(struct v4l2_subdev *sd, u32 touch)
{
	struct s5k4ecgx_state *state = to_state(sd);
	u32 focus_mode = state->focus.mode;
	u16 read_value = 0;
	u32 count = 0;
#ifdef CONFIG_DEBUG_FLASH_AE
	u16 buf[2];
#endif

	cam_trace("E\n");

	/* AE, AWB Lock */
	if (!touch)
		s5k4ecgx_set_lock(sd, AEAWB_LOCK, false);

	/* Pre Start AF: Lens Init */
	if (FOCUS_MODE_MACRO == focus_mode) {
		s5k4ecgx_set_from_table(sd, "single_af_pre_start_macro",
			&state->regs->single_af_pre_start_macro, 1, 0);
	} else {
		s5k4ecgx_set_from_table(sd, "single_af_pre_start",
			&state->regs->single_af_pre_start, 1, 0);
	}

	/* Check lens Init
	 * We check for 500ms (delay + polling time) */
	cam_dbg("AF Lens Position Init\n");
	for (count = 0; count < LENS_POS_CHECK_COUNT; count++) {
		if (state->focus.start == AUTO_FOCUS_OFF) {
			cam_dbg("do_af: AF is cancelled while doing(Lens init)\n");
			state->focus.status = AF_RESULT_CANCELLED;
			goto check_done;
		}

		read_value = 0x11;
		s5k4ecgx_readw(sd, REG_MON_AFD_CUR_DRV_POS,
				&read_value, S5K_I2C_PAGE);
		af_dbg("Lens position status(%02d) = 0x%04X\n",
					count, read_value);
		/* cam_dbg("Lens position status(%02d) = 0x%04X\n",
					count, read_value); */

		if (FOCUS_MODE_MACRO == focus_mode) {
			if (read_value >= 0xD0)
				break;
		} else if (0x00 == read_value)
			break;

		msleep_debug(LENS_MOVE_CHECK_DELAY, false);
	}

	if (count >= LENS_POS_CHECK_COUNT) {
		cam_err("do_af: error, Lens init failed. focus-mode %d"
			", count %d, val 0x%X\n\n", focus_mode, count, read_value);
	}

	/* Start AF */
	s5k4ecgx_set_from_table(sd, "single_af_start",
			&state->regs->single_af_start, 1, 0);

	/* Sleep while 2frame */
	if (state->hd_videomode)
		msleep_debug(100, true); /* 100ms */
	else if (SCENE_MODE_NIGHTSHOT == state->scene_mode)
		msleep_debug(ONE_FRAME_DELAY_MS_NIGHTMODE * 2, true); /* 330ms */
	else
		msleep_debug(ONE_FRAME_DELAY_MS_LOW * 2, true); /* 200ms */

	/*AF 1st Searching*/
	cam_dbg("AF 1st search\n");
	for (count = 0; count < FIRST_AF_SEARCH_COUNT; count++) {
		if (state->focus.start == AUTO_FOCUS_OFF) {
			cam_dbg("do_af: AF is cancelled while doing(1st)\n");
			state->focus.status = AF_RESULT_CANCELLED;
			goto check_done;
		}

#ifdef CONFIG_DEBUG_FLASH_AE
		buf[0] = buf[1] = 0xDEAD;
		s5k4ecgx_read(sd, REG_MON_AAIO_ME_LEI_EXP, buf, 2, S5K_I2C_PAGE);
		cam_dbg("AF 1st: exposure msb 0x%04X, lsb 0x%04X\n", buf[1], buf[0]);
#endif

		read_value = 0x01;
		s5k4ecgx_readw(sd, REG_MON_AF_1ST_SEARCH,
				&read_value, S5K_I2C_PAGE);
		af_dbg("1st AF status(%02d) = 0x%04X\n",
					count, read_value);
		if (read_value != 0x01)
			break;

		msleep_debug(AF_SEARCH_DELAY, false);
	}

	/* Check success */
	if (read_value != 0x02) {
		cam_err("do_af: error, 1st AF failed. count %d, val 0x%X\n\n",
			count, read_value);
		state->focus.status = AF_RESULT_FAILED;
		goto check_done;
	}

	/* AF 2nd Searching */
	cam_dbg("AF 2nd search\n");
	for (count = 0; count < SECOND_AF_SEARCH_COUNT; count++) {
		if (state->focus.start == AUTO_FOCUS_OFF) {
			cam_dbg("do_af: AF is cancelled while doing(2nd)\n");
			state->focus.status = AF_RESULT_CANCELLED;
			goto check_done;
		}

		msleep_debug(AF_SEARCH_DELAY, false);

		read_value = 0x0FFFF;
		s5k4ecgx_readw(sd, REG_MON_AF_2ND_SEARCH,
				&read_value, S5K_I2C_PAGE);
		af_dbg("2nd AF status(%02d) = 0x%04X\n", count, read_value);

		if (!(read_value & 0x0ff00))
			break;
	}

	if (count >= SECOND_AF_SEARCH_COUNT) {
		/* 0x01XX means "not Finish". */
		cam_err("do_af: error, AF not finished yet. 0x%X\n\n",
			read_value & 0x0ff00);
		state->focus.status = AF_RESULT_FAILED;
		goto check_done;
	}

	/* AF Success */
	cam_info("AF Success!\n");
	state->focus.status = AF_RESULT_SUCCESS;

check_done:
	/* restore write mode */

	/* We only unlocked AE,AWB in case of being cancelled.
	 * But we now unlock it unconditionally if AF is started,
	 */
	if (state->focus.status == AF_RESULT_CANCELLED) {
		cam_dbg("do_af: Single AF cancelled\n");
		s5k4ecgx_set_lock(sd, AEAWB_UNLOCK, false);
	} else {
		state->focus.start = AUTO_FOCUS_OFF;
		cam_dbg("do_af: Single AF finished\n");
	}

	if ((state->flash.preflash == PREFLASH_ON) &&
	    (state->sensor_mode == SENSOR_CAMERA)) {
		s5k4ecgx_set_from_table(sd, "fast_ae_off",
			&state->regs->fast_ae_off, 1, 0);
		s5k4ecgx_set_lock(sd, AEAWB_UNLOCK, true);
		s5k4ecgx_set_from_table(sd, "preflash_end",
			&state->regs->preflash_end, 1, 0);
		s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_OFF);
		if (state->focus.status == AF_RESULT_CANCELLED) {
			state->flash.preflash = PREFLASH_NONE;
		}
	}

	return 0;
}

static int s5k4ecgx_set_af(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	if (!IS_AF_SUPPORTED()) {
		cam_info("set_af: not supported\n");
		return 0;
	}

	cam_info("set_af: %s, focus mode %d\n", val ? "start" : "stop",
		state->focus.mode);

	if (unlikely((u32)val >= AUTO_FOCUS_MAX))
		val = AUTO_FOCUS_ON;

	if (state->focus.start == val)
		return 0;
	
	state->focus.start = val;

	if (val == AUTO_FOCUS_ON) {
		if ((state->runmode != RUNMODE_RUNNING) &&
		    (state->runmode != RUNMODE_RECORDING)) {
			cam_err("error, AF can't start, not in preview\n");
			state->focus.start = AUTO_FOCUS_OFF;
			return -ESRCH;
		}

		/* Fix me: AF_RESULT_DOING problem */
		if (!mutex_is_locked(&state->af_lock))
			state->focus.status = AF_RESULT_DOING;

		err = queue_work(state->wq, &state->af_work);
		if (unlikely(!err))
			cam_warn("warning, AF is still processing.\n");
	} else {
		/* Cancel AF */
		cam_info("set_af: AF cancel requested!\n");
	}

	cam_trace("X\n");
	return 0;
}

static int s5k4ecgx_start_af(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_time_interval *win_stable = &state->focus.win_stable;
	u32 touch, flash_mode, touch_win_delay = 0;
	s32 interval = 0;
	int err = 0;

	cam_trace("E\n");

	mutex_lock(&state->af_lock);
	state->focus.status = AF_RESULT_DOING; /* Don't fix me */
	state->focus.reset_done = 0;
	state->af_pid = task_pid_nr(current);
	touch = state->focus.touch;
	state->focus.touch = 0;

	flash_mode = state->flash.mode;

	/* Do preflash */
	if (state->sensor_mode == SENSOR_CAMERA) {
		state->one_frame_delay_ms = ONE_FRAME_DELAY_MS_NORMAL;
		touch_win_delay = ONE_FRAME_DELAY_MS_LOW;

		err = s5k4ecgx_af_start_preflash(sd, touch, flash_mode);
		if (unlikely(err || (state->focus.status == AF_RESULT_CANCELLED)))
			goto out;
	} else
		state->one_frame_delay_ms = touch_win_delay = 50;

	/* sleep here for the time needed for af window before do_af. */
	if (touch) {
		do_gettimeofday(&win_stable->end);
		interval = GET_ELAPSED_TIME(win_stable->start, \
				win_stable->end) / 1000;
		if (interval < touch_win_delay) {
			cam_dbg("window stable: %dms + %dms\n", interval,
				touch_win_delay - interval);
			msleep_debug(touch_win_delay - interval, true);
		} else
			cam_dbg("window stable: %dms\n", interval);
	}

	/* Do AF */
	s5k4ecgx_do_af(sd, touch);

out:
	state->af_pid = 0;
	mutex_unlock(&state->af_lock);
	cam_trace("X\n");
	return 0;
}

static int s5k4ecgx_stop_af(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	cam_trace("E\n");

	if (check_af_pid(sd)) {
		cam_err("stop_af: I should not be here\n");
		return -EPERM;
	}

	mutex_lock(&state->af_lock);

	switch (state->focus.status) {
	case AF_RESULT_FAILED:
	case AF_RESULT_SUCCESS:
		cam_dbg("Stop AF, focus mode %d, AF result %d\n",
			state->focus.mode, state->focus.status);

		err = s5k4ecgx_set_lock(sd, AEAWB_UNLOCK, false);
		if (unlikely(err)) {
			cam_err("%s: error, fail to set lock\n", __func__);
			goto err_out;
		}
		state->focus.status = AF_RESULT_CANCELLED;
		state->flash.preflash = PREFLASH_NONE;
		break;

	case AF_RESULT_CANCELLED:
		break;

	default:
		cam_warn("stop_af: warn, unnecessary calling. AF status=%d\n",
			state->focus.status);
		/* Return 0. */
		goto err_out;
		break;
	}

	if (state->focus.touch)
		state->focus.touch = 0;

	mutex_unlock(&state->af_lock);
	cam_trace("X\n");
	return 0;

err_out:
	mutex_unlock(&state->af_lock);
	return err;
}

/**
 * s5k4ecgx_check_wait_af_complete: check af and wait
 * @cancel: if true, force to cancel AF.
 *
 * check whether AF is running and then wait to be fininshed.
 */
static int s5k4ecgx_check_wait_af_complete(struct v4l2_subdev *sd, bool cancel)
{
	struct s5k4ecgx_state *state = to_state(sd);

	if (check_af_pid(sd)) {
		cam_err("check_wait_af_complete: I should not be here\n");
		return -EPERM;
	}

	if (mutex_is_locked(&state->af_lock))
		cam_info("check_wait_af_complete: AF operation not finished yet\n");

	if ((AF_RESULT_DOING == state->focus.status) && cancel)
		s5k4ecgx_set_af(sd, AUTO_FOCUS_OFF);

	mutex_lock(&state->af_lock);
	mutex_unlock(&state->af_lock);
	return 0;
}

static void s5k4ecgx_af_worker(struct work_struct *work)
{
#ifdef CONFIG_CAM_EARYLY_PROBE
	s5k4ecgx_start_af(TO_STATE(work, af_work)->sd);
#else
	s5k4ecgx_start_af(&TO_STATE(work, af_work)->sd);
#endif
}

static int s5k4ecgx_set_focus_mode(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	u32 cancel = 0;
	u8 focus_mode = (u8)val;
	int err = -EINVAL;

	cam_info("set_focus_mode %d(0x%X)\n", val, val);

	if (!IS_AF_SUPPORTED() || (state->focus.mode == val))
		return 0;

	cancel = (u32)val & FOCUS_MODE_DEFAULT;

	if (cancel) {
		/* Do nothing if cancel request occurs when af is being finished*/
		if (AF_RESULT_DOING == state->focus.status) {
			state->focus.start = AUTO_FOCUS_OFF;
			return 0;
		}
		
		s5k4ecgx_stop_af(sd);
		if (state->focus.reset_done) {
			cam_dbg("AF is already cancelled fully\n");
			goto out;
		}
		state->focus.reset_done = 1;
	}

	mutex_lock(&state->af_lock);

	switch (focus_mode) {
	case FOCUS_MODE_MACRO:
		err = s5k4ecgx_set_from_table(sd, "af_macro_mode",
				&state->regs->af_macro_mode, 1, 0);
		CHECK_ERR_GOTO_MSG(err, err_out,
			"fail to af_macro_mode (%d)\n", err);

		state->focus.mode = focus_mode;
		break;

	case FOCUS_MODE_INFINITY:
	case FOCUS_MODE_AUTO:
	case FOCUS_MODE_FIXED:
		err = s5k4ecgx_set_from_table(sd, "af_normal_mode",
				&state->regs->af_normal_mode, 1, 0);
		CHECK_ERR_GOTO_MSG(err, err_out,
			"fail to af_normal_mode (%d)\n", err);

		state->focus.mode = focus_mode;
		break;

	case FOCUS_MODE_FACEDETECT:
	/*case FOCUS_MODE_CONTINOUS:*/
	case FOCUS_MODE_TOUCH:
		break;

	default:
		cam_err("%s: error, invalid val(0x%X)\n:",
			__func__, val);
		goto err_out;
		break;
	}

	mutex_unlock(&state->af_lock);

out:
	return 0;

err_out:
	mutex_unlock(&state->af_lock);
	return err;
}

static int s5k4ecgx_set_af_softlanding(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EINVAL;

	cam_trace("E\n");

	if (!state->initialized || !IS_AF_SUPPORTED())
		return 0;

	if (V4L2_PIX_FMT_MODE_CAPTURE == state->format_mode)
		err =s5k4ecgx_set_from_table(sd, "softlanding_cap",
				&state->regs->softlanding_cap, 1, 0);
	else {
		err =s5k4ecgx_set_from_table(sd, "softlanding",
				&state->regs->softlanding, 1, 0);
	}
	CHECK_ERR_MSG(err, "fail to set softlanding\n");

	cam_trace("X\n");
	return 0;
}

static inline void s5k4ecgx_verify_window(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_rect first_window, second_window;

	memset(&first_window, 0, sizeof (struct s5k4ecgx_rect));
	memset(&second_window, 0, sizeof (struct s5k4ecgx_rect));

	s5k4ecgx_readw(sd, REG_AF_FSTWIN_SIZEX,
		(u16 *)(&first_window.width), S5K_I2C_PAGE);
	s5k4ecgx_readw(sd, REG_AF_FSTWIN_SIZEY,
		(u16 *)(&first_window.height), 0);
	s5k4ecgx_readw(sd, REG_AF_SCNDWIN_SIZEX,
		(u16 *)(&second_window.width), 0);
	s5k4ecgx_readw(sd, REG_AF_SCNDWIN_SIZEY,
		(u16 *)(&second_window.height), 0);

	if ((first_window.width != FIRST_WINSIZE_X)
	    || (first_window.height != FIRST_WINSIZE_Y)
	    || (second_window.width != SCND_WINSIZE_X)
	    || (second_window.height != SCND_WINSIZE_Y)) {
		cam_err("verify_window: error, invalid window size"
			" (0x%X, 0x%X) (0x%X, 0x%X)\n",
			first_window.width, first_window.height,
			second_window.width, second_window.height);
	} else
		state->focus.window_verified = 1;

	return;
}

static int s5k4ecgx_set_af_window(struct v4l2_subdev *sd)
{
	int err = 0;
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_rect inner_window = {0, 0, 0, 0};
	struct s5k4ecgx_rect outter_window = {0, 0, 0, 0};
	struct s5k4ecgx_rect first_window = {0, 0, 0, 0};
	struct s5k4ecgx_rect second_window = {0, 0, 0, 0};
	const s32 mapped_x = state->focus.pos_x;
	const s32 mapped_y = state->focus.pos_y;
	const u32 preview_width = state->preview.frmsize->width;
	const u32 preview_height = state->preview.frmsize->height;
	u32 inner_half_width = 0, inner_half_height = 0;
	u32 outter_half_width = 0, outter_half_height = 0;

	cam_trace("E\n");

	mutex_lock(&state->af_lock);
	state->af_pid = task_pid_nr(current);

	/* Verify 1st, 2nd window size */
	if (!state->focus.window_verified)
		s5k4ecgx_verify_window(sd);

	inner_window.width = SCND_WINSIZE_X * preview_width / 1024;
	inner_window.height = SCND_WINSIZE_Y * preview_height / 1024;
	outter_window.width = FIRST_WINSIZE_X * preview_width / 1024;
	outter_window.height = FIRST_WINSIZE_Y * preview_height / 1024;

	inner_half_width = inner_window.width / 2;
	inner_half_height = inner_window.height / 2;
	outter_half_width = outter_window.width / 2;
	outter_half_height = outter_window.height / 2;

	af_dbg("Preview width=%d, height=%d\n", preview_width, preview_height);
	af_dbg("inner_window_width=%d, inner_window_height=%d, " \
		"outter_window_width=%d, outter_window_height=%d\n ",
		inner_window.width, inner_window.height,
		outter_window.width, outter_window.height);

	/* Get X */
	if (mapped_x <= inner_half_width) {
		inner_window.x = outter_window.x = 0;
		af_dbg("inner & outter window over sensor left."
			"in_x=%d, out_x=%d\n", inner_window.x, outter_window.x);
	} else if (mapped_x <= outter_half_width) {
		inner_window.x = mapped_x - inner_half_width;
		outter_window.x = 0;
		af_dbg("outter window over sensor left. in_x=%d, out_x=%d\n",
					inner_window.x, outter_window.x);
	} else if (mapped_x >= ((preview_width - 1) - inner_half_width)) {
		inner_window.x = (preview_width - 1) - inner_window.width;
		outter_window.x = (preview_width - 1) - outter_window.width;
		af_dbg("inner & outter window over sensor right." \
			"in_x=%d, out_x=%d\n", inner_window.x, outter_window.x);
	} else if (mapped_x >= ((preview_width - 1) - outter_half_width)) {
		inner_window.x = mapped_x - inner_half_width;
		outter_window.x = (preview_width - 1) - outter_window.width;
		af_dbg("outter window over sensor right. in_x=%d, out_x=%d\n",
					inner_window.x, outter_window.x);
	} else {
		inner_window.x = mapped_x - inner_half_width;
		outter_window.x = mapped_x - outter_half_width;
		af_dbg("inner & outter window within sensor area." \
			"in_x=%d, out_x=%d\n", inner_window.x, outter_window.x);
	}

	/* Get Y */
	if (mapped_y <= inner_half_height) {
		inner_window.y = outter_window.y = 0;
		af_dbg("inner & outter window over sensor top." \
			"in_y=%d, out_y=%d\n", inner_window.y, outter_window.y);
	} else if (mapped_y <= outter_half_height) {
		inner_window.y = mapped_y - inner_half_height;
		outter_window.y = 0;
		af_dbg("outter window over sensor top. in_y=%d, out_y=%d\n",
					inner_window.y, outter_window.y);
	} else if (mapped_y >= ((preview_height - 1) - inner_half_height)) {
		inner_window.y = (preview_height - 1) - inner_window.height;
		outter_window.y = (preview_height - 1) - outter_window.height;
		af_dbg("inner & outter window over sensor bottom." \
			"in_y=%d, out_y=%d\n", inner_window.y, outter_window.y);
	} else if (mapped_y >= ((preview_height - 1) - outter_half_height)) {
		inner_window.y = mapped_y - inner_half_height;
		outter_window.y = (preview_height - 1) - outter_window.height;
		af_dbg("outter window over sensor bottom. in_y=%d, out_y=%d\n",
					inner_window.y, outter_window.y);
	} else {
		inner_window.y = mapped_y - inner_half_height;
		outter_window.y = mapped_y - outter_half_height;
		af_dbg("inner & outter window within sensor area." \
			"in_y=%d, out_y=%d\n", inner_window.y, outter_window.y);
	}

	af_dbg("==> inner_window top=(%d,%d), bottom=(%d, %d)\n",
		inner_window.x, inner_window.y,
		inner_window.x + inner_window.width,
		inner_window.y + inner_window.height);
	af_dbg("==> outter_window top=(%d,%d), bottom=(%d, %d)\n",
		outter_window.x, outter_window.y,
		outter_window.x + outter_window.width ,
		outter_window.y + outter_window.height);

	second_window.x = inner_window.x * 1024 / preview_width;
	second_window.y = inner_window.y * 1024 / preview_height;
	first_window.x = outter_window.x * 1024 / preview_width;
	first_window.y = outter_window.y * 1024 / preview_height;

	af_dbg("=> second_window top=(%d, %d)\n",
		second_window.x, second_window.y);
	af_dbg("=> first_window top=(%d, %d)\n",
		first_window.x, first_window.y);

	/* restore write mode */
	err |= s5k4ecgx_writew(sd, REG_AF_FSTWIN_STARTX,
				(u16)(first_window.x), S5K_I2C_PAGE);
	err |= s5k4ecgx_writew(sd, REG_AF_FSTWIN_STARTY,
				(u16)(first_window.y), 0);

	/* Set second widnow x, y */
	err |= s5k4ecgx_writew(sd, REG_AF_SCNDWIN_STARTX,
				(u16)(second_window.x), 0);
	err |= s5k4ecgx_writew(sd, REG_AF_SCNDWIN_STARTY,
				(u16)(second_window.y), 0);

	/* Update AF window */
	err |= s5k4ecgx_writew(sd, REG_AF_WINSIZES_UPDATED,
				0x0001, 0);

	do_gettimeofday(&state->focus.win_stable.start);

	state->focus.touch = 1;
	state->af_pid = 0;
	mutex_unlock(&state->af_lock);

	CHECK_ERR(err);
	cam_info("AF window position completed.\n");

	cam_trace("X\n");
	return 0;
}

static void s5k4ecgx_af_win_worker(struct work_struct *work)
{
#ifdef CONFIG_CAM_EARYLY_PROBE
	s5k4ecgx_set_af_window(TO_STATE(work, af_win_work)->sd);
#else
	s5k4ecgx_set_af_window(&TO_STATE(work, af_win_work)->sd);
#endif
}

static int s5k4ecgx_set_touch_af(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EIO;

	if (!IS_TOUCH_AF_SUPPORTED())
		return 0;

	cam_trace("%s, x=%d y=%d\n", val ? "start" : "stop",
			state->focus.pos_x, state->focus.pos_y);

	if (val) {
		if (mutex_is_locked(&state->af_lock)) {
			cam_warn("%s: AF is still operating!\n", __func__);
			return 0;
		}

		err = queue_work(state->wq, &state->af_win_work);
		if (likely(!err))
			cam_warn("WARNING, AF window is still processing\n");
	} else
		cam_info("set_touch_af: invalid value %d\n", val);

	cam_trace("X\n");
	return 0;
}

static const struct s5k4ecgx_framesize * __used
s5k4ecgx_get_framesize_i(const struct s5k4ecgx_framesize *frmsizes,
				u32 frmsize_count, u32 index)
{
	int i = 0;

	for (i = 0; i < frmsize_count; i++) {
		if (frmsizes[i].index == index)
			return &frmsizes[i];
	}

	return NULL;
}

static const struct s5k4ecgx_framesize * __used
s5k4ecgx_get_framesize_sz(const struct s5k4ecgx_framesize *frmsizes,
				u32 frmsize_count, u32 width, u32 height)
{
	int i;

	for (i = 0; i < frmsize_count; i++) {
		if ((frmsizes[i].width == width) && (frmsizes[i].height == height))
			return &frmsizes[i];
	}

	return NULL;
}

static const struct s5k4ecgx_framesize * __used
s5k4ecgx_get_framesize_ratio(const struct s5k4ecgx_framesize *frmsizes,
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
static void s5k4ecgx_set_framesize(struct v4l2_subdev *sd,
				const struct s5k4ecgx_framesize *frmsizes,
				u32 num_frmsize, bool preview)
{
	struct s5k4ecgx_state *state = to_state(sd);
	const struct s5k4ecgx_framesize **found_frmsize = NULL;
	u32 width = state->req_fmt.width;
	u32 height = state->req_fmt.height;

	cam_dbg("set_framesize: Requested Res %dx%d\n", width, height);

	found_frmsize = /*(const struct s5k4ecgx_framesize **) */
			(preview ? &state->preview.frmsize: &state->capture.frmsize);

	if (state->hd_videomode) {
		*found_frmsize = s5k4ecgx_get_framesize_sz(
				s5k4ecgx_hidden_preview_frmsizes, 
				ARRAY_SIZE(s5k4ecgx_hidden_preview_frmsizes),
				width, height);

		if (*found_frmsize == NULL)
			*found_frmsize = s5k4ecgx_get_framesize_ratio(
					frmsizes, num_frmsize, width, height);
	} else
		*found_frmsize = s5k4ecgx_get_framesize_ratio(
				frmsizes, num_frmsize, width, height);

	if (*found_frmsize == NULL) {
		cam_err("%s: error, invalid frame size %dx%d\n",
			__func__, width, height);
		*found_frmsize = preview ?
			s5k4ecgx_get_framesize_i(frmsizes, num_frmsize,
				(state->sensor_mode == SENSOR_CAMERA) ?
				PREVIEW_SZ_XGA: PREVIEW_SZ_1024x576):
			s5k4ecgx_get_framesize_i(frmsizes, num_frmsize,
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

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
static int s5k4ecgx_start_streamoff_checker(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) && !CONFIG_DEBUG_STREAMOFF
	return 0;
#endif

	cam_trace("EX");

#ifdef CONFIG_VIDEO_FAST_MODECHANGE_V2
	INIT_COMPLETION(state->streamoff_complete);
#endif
	atomic_set(&state->streamoff_check, true);

	err = queue_work(state->wq, &state->streamoff_work);
	if (unlikely(!err))
		cam_info("streamoff_checker is already operating!\n");

	return 0;
}

static void s5k4ecgx_stop_streamoff_checker(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);

	cam_trace("EX");

	atomic_set(&state->streamoff_check, false);

	if (flush_work(&state->streamoff_work))
		cam_dbg("wait... streamoff_checker stopped\n");
}

static void s5k4ecgx_streamoff_checker(struct work_struct *work)
{
	struct s5k4ecgx_state *state = TO_STATE(work, streamoff_work);
#ifdef CONFIG_CAM_EARYLY_PROBE
	struct v4l2_subdev *sd = state->sd;
#else
	struct v4l2_subdev *sd = &state->sd;
#endif
	struct timeval start_time, end_time;
	s32 elapsed_msec = 0;
	const u32 interval = 5, wait = 600; /* night capture 1 frame */
	const u32 max = wait / (interval + 1); /* i2c time is 1 */
	u16 status = 0xFF; /* don't fix me 0xFF */
	int count, err = 0;

	/* cam_dbg("streamoff_checker start\n");*/

	do_gettimeofday(&start_time);

	for (count = 0; count < max; count++) {
		if (!atomic_read(&state->streamoff_check))
			goto out;

		err = s5k4ecgx_readw(sd, REG_ENABLE_PREVIEW_CHANGED,
				&status, S5K_I2C_PAGE);
		CHECK_ERR_GOTO_MSG(err, out_err, "streamoff_checker: error, readb\n");

		/* complete if 0 or abort */
		if (!(status & 0x01) || !atomic_read(&state->streamoff_check))
			goto out;

		/* cam_info("wait_steamoff: status = 0x%X\n", status);*/
		msleep_debug(interval, false);
	}

out:
	do_gettimeofday(&end_time);
	elapsed_msec = GET_ELAPSED_TIME(start_time, end_time) / 1000;

	if (unlikely(count >= max))
		cam_err("streamoff_checker: error, time-out\n\n");
	else if (status & 0x01) {
		cam_dbg("streamoff_checker aborted %d (%dms)\n",
			atomic_read(&state->streamoff_check), elapsed_msec);
		return;
	} else {
		cam_dbg("**** streamoff_checker: complete! %dms ****\n", elapsed_msec);
#ifdef CONFIG_VIDEO_FAST_MODECHANGE_V2
		complete(&state->streamoff_complete);
#endif
	}

out_err:
	atomic_set(&state->streamoff_check, false);
	return;
}
#endif /* CONFIG_VIDEO_FAST_MODECHANGE_V2 || CONFIG_VIDEO_FAST_MODECHANGE */

#ifdef CONFIG_VIDEO_FAST_CAPTURE
static int s5k4ecgx_start_capmode_checker(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	cam_trace("EX");

	INIT_COMPLETION(state->streamoff_complete);
	atomic_set(&state->capmode_check, true);

	err = queue_work(state->wq, &state->capmode_work);
	if (unlikely(!err))
		cam_info("capmode_checker is already operating!\n");

	return 0;
}

static void s5k4ecgx_stop_capmode_checker(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);

	cam_trace("EX");

	atomic_set(&state->capmode_check, false);

	if (flush_work(&state->capmode_work))
		cam_dbg("wait... capmode_checker stopped\n");
}

static void s5k4ecgx_capmode_checker(struct work_struct *work)
{
	struct s5k4ecgx_state *state = TO_STATE(work, capmode_work);
#ifdef CONFIG_CAM_EARYLY_PROBE
	struct v4l2_subdev *sd = state->sd;
#else
	struct v4l2_subdev *sd = &state->sd;
#endif
	struct timeval start_time, end_time;
	s32 elapsed_msec = 0;
	const u32 interval = 5, wait = 300; /* night preview 1 frame */
	const u32 max = wait / (interval + 1); /* i2c time is 1 */
	u16 status = 0xFF; /* don't fix me 0xFF */
	int count, err = 0;

	cam_dbg("capmode_checker start\n");

	do_gettimeofday(&start_time);

	for (count = 0; count < max; count++) {
		if (!atomic_read(&state->capmode_check))
			goto out;

		err = s5k4ecgx_readw(sd, REG_ENABLE_CAPTURE_CHANGED,
				&status, S5K_I2C_PAGE);
		CHECK_ERR_GOTO_MSG(err, out_err, "capmode_checker: error, readb\n");

		/* complete if 0 or abort */
		if (!(status & 0x01) || !atomic_read(&state->capmode_check))
			goto out;

		/* cam_info("wait_steamoff: status = 0x%X\n", status);*/
		msleep_debug(interval, false);
	}

out:
	do_gettimeofday(&end_time);
	elapsed_msec = GET_ELAPSED_TIME(start_time, end_time) / 1000;

	if (unlikely(count >= max))
		cam_err("capmode_checker: error, time-out\n\n");
	else if (status & 0x01) {
		cam_info("capmode_checker aborted %d (%dms)\n",
			atomic_read(&state->capmode_check), elapsed_msec);
		return;
	} else {
		cam_dbg("**** capmode_checker: complete! %dms ****\n", elapsed_msec);
		complete(&state->streamoff_complete);
	}

out_err:
	atomic_set(&state->capmode_check, false);
	return;
}
#endif

#ifdef CONFIG_VIDEO_FAST_CAPTURE
static int s5k4ecgx_is_capture_transited(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_platform_data *pdata = state->pdata;
	u64 timeout = 3 * HZ / 10; /* Minimum Prevew 1 frame time */
	int ret, err = 0;

	cam_trace("E\n");

	ret = wait_for_completion_interruptible_timeout(&state->streamoff_complete, 
						timeout);
	if (unlikely(ret < 0)) {
		cam_err("capture_transited: error, wait for completion %d\n", ret);
		err = ret;
		goto out;
	} else if (unlikely(!ret)) {
		cam_info("capture_transited: timeout!\n");
		err = -ETIMEDOUT;
		goto out;
	}

	cam_dbg("capture_transited: %dms\n", jiffies_to_msecs(timeout - ret));

out:
	s5k4ecgx_stop_capmode_checker(sd);

	state->need_wait_streamoff = 0;
	return err;
}
#endif

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
#ifdef CONFIG_VIDEO_FAST_MODECHANGE_V2
static int s5k4ecgx_do_wait_streamoff(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_time_interval *stream_time = &state->stream_time;
	u64 timeout = 6 * HZ / 10; /* Minimum Capture frame time + 30 */
	s32 elapsed_msec = 0;
	int ret, err = 0;

	cam_trace("E\n");

	do_gettimeofday(&stream_time->end);

	elapsed_msec = GET_ELAPSED_TIME(stream_time->start, \
			stream_time->end) / 1000;

	ret = wait_for_completion_interruptible_timeout(&state->streamoff_complete,
						timeout);
	if (unlikely(ret < 0)) {
		cam_err("wait_steamoff: error, wait for completion %d\n", ret);
		err = ret;
		goto out;
	} else if (unlikely(!ret)) {
		cam_info("wait_steamoff: timeout!\n");
		err = -ETIMEDOUT;
		goto out;
	}

	cam_dbg("wait_steamoff: %dms + %dms\n", elapsed_msec,
		jiffies_to_msecs(timeout - ret));

out:
	s5k4ecgx_stop_streamoff_checker(sd);

	state->need_wait_streamoff = 0;
	return err;
}

#else /* !CONFIG_VIDEO_FAST_MODECHANGE_V2 */
static int s5k4ecgx_do_wait_streamoff(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_platform_data *pdata = state->pdata;
	struct s5k4ecgx_time_interval *stream_time = &state->stream_time;
	s32 elapsed_msec = 0;

	if (unlikely(!(pdata->common.is_mipi & state->need_wait_streamoff)))
		return 0;

	cam_trace("E\n");

	do_gettimeofday(&stream_time->end);

	elapsed_msec = GET_ELAPSED_TIME(stream_time->start, \
				stream_time->end) / 1000;

	if (pdata->common.streamoff_delay > elapsed_msec) {
		cam_info("stream-off: %dms + %dms\n", elapsed_msec,
			pdata->common.streamoff_delay - elapsed_msec);
		msleep_debug(pdata->common.streamoff_delay - elapsed_msec, true);
	} else
		cam_info("stream-off: %dms\n", elapsed_msec);

#if CONFIG_DEBUG_STREAMOFF
	s5k4ecgx_stop_streamoff_checker(sd);
#endif

	state->need_wait_streamoff = 0;

	return 0;
}
#endif /* CONFIG_VIDEO_FAST_MODECHANGE_V2 */

static int s5k4ecgx_wait_steamoff(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_platform_data *pdata = state->pdata;
	int err = 0;

	if (unlikely(!pdata->common.is_mipi))
		return 0;

	if (state->need_wait_streamoff)
		err = s5k4ecgx_do_wait_streamoff(sd);
#ifdef CONFIG_VIDEO_FAST_CAPTURE
	else if (V4L2_PIX_FMT_MODE_CAPTURE == state->format_mode)
		err = s5k4ecgx_is_capture_transited(sd);
#endif
	else
		cam_dbg("wait_steamoff: do nothing\n");

	cam_trace("EX\n");

	return err;
}
#endif /* CONFIG_VIDEO_FAST_MODECHANGE || CONFIG_VIDEO_FAST_MODECHANGE_V2 */

static int s5k4ecgx_control_stream(struct v4l2_subdev *sd, u32 cmd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	bool flash_on, lowlux_cap;
	int err = -EINVAL;

	if (!state->pdata->common.is_mipi)
		return 0;

	if (unlikely(cmd != STREAM_STOP))
		return 0;

	if (IS_AF_SUPPORTED())
		s5k4ecgx_check_wait_af_complete(sd, true);

	if (!((RUNMODE_RUNNING == state->runmode) && state->capture.pre_req)) {
		cam_info("STREAM STOP!!\n");
		err = s5k4ecgx_set_from_table(sd, "stream_stop",
				&state->regs->stream_stop, 1, 0);
		CHECK_ERR_MSG(err, "failed to stop stream\n");

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
		do_gettimeofday(&state->stream_time.start);
		state->need_wait_streamoff = 1;

		s5k4ecgx_start_streamoff_checker(sd);
#else
		msleep_debug(state->pdata->common.streamoff_delay, true);
#endif
	}

	switch (state->runmode) {
	case RUNMODE_CAPTURING:
		flash_on = state->flash.on;
		lowlux_cap = state->capture.lowlux;

		cam_dbg("Capture Stop!\n");
		s5k4ecgx_get_exif(sd);
		state->runmode = RUNMODE_CAPTURING_STOP;
		state->capture.ready = 0;
		state->flash.on = state->capture.lowlux = 0;

		/* We turn flash off if one-shot flash is still on. */
		if (flash_on) {
			err = s5k4ecgx_set_from_table(sd, "flash_end",
					&state->regs->flash_end, 1, 0);
			CHECK_ERR(err);

			if (s5k4ecgx_is_hwflash_on(sd))
				s5k4ecgx_flash_oneshot(sd, S5K4ECGX_FLASH_OFF);
		}

		if (lowlux_cap) {
			err = s5k4ecgx_set_from_table(sd, "lowlight_cap_off",
					&state->regs->lowlight_cap_off, 1, 0);
			CHECK_ERR(err);
		}

		err = s5k4ecgx_set_lock(sd, AEAWB_UNLOCK, true);
		CHECK_ERR(err);

#ifdef CONFIG_VIDEO_FAST_CAPTURE
		s5k4ecgx_stop_capmode_checker(sd);
#endif
#ifndef CONFIG_MACH_PX
		/* Fix me */
		if (SCENE_MODE_NIGHTSHOT == state->scene_mode) {
			state->pdata->common.streamoff_delay = 
				S5K4ECGX_STREAMOFF_NIGHT_DELAY;
		}
#endif
		break;

	case RUNMODE_RUNNING:
		cam_dbg("Preview Stop!\n");
		state->runmode = RUNMODE_RUNNING_STOP;

		if (state->capture.pre_req) {
			s5k4ecgx_start_fast_capture(sd);
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

/* PX: Set flash mode */
static int s5k4ecgx_set_flash_mode(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);

	/* movie flash mode should be set when recording is started */
	/* if (state->sensor_mode == SENSOR_MOVIE && !state->recording)
		return 0;*/

	if (!IS_FLASH_SUPPORTED() || (state->flash.mode == val)) {
		cam_dbg("not support or the same flash mode=%d\n", val);
		return 0;
	}

	if (val == FLASH_MODE_TORCH)
		s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_ON);

	if ((state->flash.mode == FLASH_MODE_TORCH)
	    && (val == FLASH_MODE_OFF))
		s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_OFF);

	state->flash.mode = val;

	cam_dbg("Flash mode = %d\n", val);
	return 0;
}

static int s5k4ecgx_check_esd(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;
	u16 read_value = 0;

	err = s5k4ecgx_set_from_table(sd, "get_esd_status",
		&state->regs->get_esd_status, 1, 0);
	CHECK_ERR(err);
	err = __s5k4ecgx_i2c_readw(client, 0x0F12, &read_value);
	CHECK_ERR(err);

	if (read_value != 0xAAAA)
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
static inline int s5k4ecgx_get_exif_iso(struct v4l2_subdev *sd, u16 *iso)
{
	struct s5k4ecgx_state *state = to_state(sd);
	/*struct i2c_client *client = v4l2_get_subdevdata(sd);*/
	int err = -EIO;
	u16 val = 0, buf[2] = {0,};
	u16 *gain_a = &buf[0], *gain_d = &buf[1];
	int temp = 0;

	err = s5k4ecgx_read(sd, REG_MON_AAIO_ME_AGAIN, buf, 2, S5K_I2C_PAGE);
	CHECK_ERR(err);

	val = (((*gain_a) * (*gain_d)) / 256 ) / 2;

	if (state->iso == ISO_AUTO) {
		if (val < 256)
			temp = 50;
		else if (val >= 256 && val < 512)
			temp = 100;
		else if (val >= 512 && val < 896)
			temp = 200;
		else
			temp = 400;
	} else
		temp = state->iso;

	*iso = temp;
	cam_dbg("EXIF: A gain=%d, D gain=%d, ISO=%d\n", *gain_a, *gain_d, temp);

	/* We do not restore write mode */

	return 0;
}

/* PX: Set ISO */
static int __used s5k4ecgx_set_iso(struct v4l2_subdev *sd, s32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EINVAL;

	cam_trace("E\n");

retry:
	switch (val) {
	case ISO_AUTO:
	case ISO_100:
	case ISO_200:
	case ISO_400:
		err = s5k4ecgx_set_from_table(sd, "iso",
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
static inline int s5k4ecgx_get_exif_exptime(struct v4l2_subdev *sd,
						u32 *exp_time)
{
	/* struct i2c_client *client = v4l2_get_subdevdata(sd);*/
	u16 buf[2] = {0,};
	u32 val;
	bool retry_done = false;
	int err = 0;

retry:
	buf[0] = buf[1] = 0;
	err = s5k4ecgx_read(sd, REG_MON_AAIO_ME_LEI_EXP, buf, 2, S5K_I2C_PAGE);
	CHECK_ERR(err);

	val = ((buf[1] << 16) | (buf[0] & 0xFFFF));
	*exp_time = (val * 1000) / 400;

	cam_dbg("EXIF: msb = 0x%X, lsb = 0x%X, exp time = %d\n",
		buf[1], buf[0], *exp_time);

	if (unlikely(val <= 4000)) {
		val = 0;
		err = s5k4ecgx_readw(sd, 0x700029A4,
				(u16 *)&val, S5K_I2C_PAGE);
		CHECK_ERR_MSG(err, "fail to read NB register\n");

		cam_err("EXIF: invalid exposure time. NB 0x%X\n", val);
		if (!retry_done) {
			retry_done = true;
			goto retry;
		}
	}
	/* We do not restore write mode */

	return 0;
}

static inline void s5k4ecgx_get_exif_flash(struct v4l2_subdev *sd,
					u16 *flash)
{
	struct s5k4ecgx_state *state = to_state(sd);
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
static int s5k4ecgx_get_exif(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	u32 exposure_time = 0;
	int err = 0;

	/* exposure time */
	state->exif.exp_time_den = 0;
	err = s5k4ecgx_get_exif_exptime(sd, &exposure_time);
	if (exposure_time)
		state->exif.exp_time_den = 1000 * 1000 / exposure_time;
	else
		cam_err("EXIF: error, exposure time 0. %d\n", err);

	/* iso */
	state->exif.iso = 0;
	err = s5k4ecgx_get_exif_iso(sd, &state->exif.iso);
	if (unlikely(err))
		cam_err("EXIF: error, fail to get exif iso. %d\n", err);

	/* flash */
	s5k4ecgx_get_exif_flash(sd, &state->exif.flash);

	cam_dbg("EXIF: ex_time_den=%d, iso=%d, flash=0x%02X\n",
		state->exif.exp_time_den, state->exif.iso, state->exif.flash);

	return 0;
}

static inline int s5k4ecgx_check_cap_sz_except(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EINVAL;

	cam_err("capture_size_exception: warning, reconfiguring...\n\n");

	switch (state->wide_cmd) {
	case WIDE_REQ_CHANGE:
		cam_info("%s: Wide Capture setting\n", __func__);
		err = s5k4ecgx_set_from_table(sd, "change_wide_cap",
			&state->regs->change_wide_cap, 1, 0);
		break;

	case WIDE_REQ_RESTORE:
		cam_info("%s: Restore capture setting\n", __func__);
		err = s5k4ecgx_set_from_table(sd, "restore_capture",
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

static inline int s5k4ecgx_set_capture_flash(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	u32 light_level = 0xFFFFFFFF;
	int err = 0;

	if ((SCENE_MODE_NIGHTSHOT == state->scene_mode)
	    || !IS_FLASH_SUPPORTED())
		return 0;

	/* Set flash */
	switch (state->flash.mode) {
	case FLASH_MODE_AUTO:
		/* 3rd party App may do capturing without AF. So we check
		 * whether AF is executed  before capture and  turn on flash
		 * if needed. But we do not consider low-light capture of Market
		 * App. */
		if (state->flash.preflash == PREFLASH_NONE) {
			s5k4ecgx_get_light_level(sd, &light_level);
			if (light_level >= LUX_LEVEL_LOW)
				break;
		} else if (state->flash.preflash == PREFLASH_OFF)
			break;
		/* We do not break. */

	case FLASH_MODE_ON:
		s5k4ecgx_flash_oneshot(sd, S5K4ECGX_FLASH_ON);
		/* We here don't need to set state->flash.on to 1 */

		/*err = s5k4ecgx_set_lock(sd, AEAWB_UNLOCK, true);
		CHECK_ERR_MSG(err, "fail to set lock\n");*/

		/* Full flash start */
		err = s5k4ecgx_set_from_table(sd, "flash_start",
			&state->regs->flash_start, 1, 0);
		/* AE delay */
		msleep_debug(ONE_FRAME_DELAY_MS_LOW * 2, true); 
		break;

	case FLASH_MODE_OFF:
		if (state->light_level < LUX_LEVEL_LOW)
			msleep_debug(ONE_FRAME_DELAY_MS_LOW, true);
	default:
		break;
	}
	CHECK_ERR(err);

	state->exif.flash = state->flash.on;
	return 0;	
}

static int s5k4ecgx_set_preview_size(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EINVAL;

	if (!state->preview.update_frmsize)
		return 0;

	if (0 /*state->hd_videomode*/)
		goto out;

	cam_trace("E, wide_cmd=%d\n", state->wide_cmd);

	switch (state->wide_cmd) {
	case WIDE_REQ_CHANGE:
		cam_info("preview_size: Wide Capture setting\n");
		err = s5k4ecgx_set_from_table(sd, "change_wide_cap",
			&state->regs->change_wide_cap, 1, 0);
		break;

	case WIDE_REQ_RESTORE:
		cam_info("preview_size:Restore capture setting\n");
		err = s5k4ecgx_set_from_table(sd, "restore_capture",
				&state->regs->restore_cap, 1, 0);
		/* We do not break */

	default:
		cam_dbg("set_preview_size\n");
		err = s5k4ecgx_set_from_table(sd, "preview_size",
				state->regs->preview_size,
				ARRAY_SIZE(state->regs->preview_size),
				state->preview.frmsize->index);
#ifdef CONFIG_MACH_PX
		BUG_ON(state->preview.frmsize->index == PREVIEW_SZ_PVGA);
#endif
		break;
	}
	CHECK_ERR(err);

out:
	state->preview.update_frmsize = 0;

	return 0;
}

static int s5k4ecgx_set_capture_size(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	/* do nothing about size except switching normal(wide) 
	 * to wide(normal). */
	if (unlikely(state->wide_cmd))
		err = s5k4ecgx_check_cap_sz_except(sd);

	return err;
}

static int s5k4ecgx_start_preview(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EINVAL;

	cam_dbg("Camera Preview start, runmode = %d\n", state->runmode);

	if ((state->runmode == RUNMODE_NOTREADY) ||
	    (state->runmode == RUNMODE_CAPTURING)) {
		cam_err("%s: error - Invalid runmode\n", __func__);
		return -EPERM;
	}

	if (IS_FLASH_SUPPORTED() || IS_AF_SUPPORTED()) {
		state->flash.preflash = PREFLASH_NONE;
		state->focus.status = AF_RESULT_NONE;
		state->focus.touch = 0;
	}

	/* Check pending fps */
	if (state->req_fps >= 0) {
		err = s5k4ecgx_set_frame_rate(sd, state->req_fps);
		CHECK_ERR(err);
	}

	/* Set movie mode if needed. */
	err = s5k4ecgx_transit_movie_mode(sd);
	CHECK_ERR_MSG(err, "failed to set movide mode(%d)\n", err);

	/* Set preview size */
	err = s5k4ecgx_set_preview_size(sd);
	CHECK_ERR_MSG(err, "failed to set preview size(%d)\n", err);

	/* Transit preview mode */
	err = s5k4ecgx_transit_preview_mode(sd);
	CHECK_ERR_MSG(err, "preview_mode(%d)\n", err);

	state->runmode = (state->sensor_mode == SENSOR_CAMERA) ?
			RUNMODE_RUNNING : RUNMODE_RECORDING;

	return 0;
}

static inline int s5k4ecgx_update_preview(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	if (!state->need_preview_update)
		return 0;

	if ((RUNMODE_RUNNING == state->runmode)
	    || (RUNMODE_RECORDING == state->runmode)) {
		/* Update preview */
		err = s5k4ecgx_set_from_table(sd, "update_preview",
				&state->regs->update_preview, 1, 0);
		CHECK_ERR_MSG(err, "update_preview %d\n", err);

		state->need_preview_update = 0;
	}

	return 0;
}

/**
 * s5k4ecgx_start_capture: Start capture 
 */
static int s5k4ecgx_start_capture(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -ENODEV;

	if (state->capture.ready)
		return 0;
		
	/* Set capture size */
	err = s5k4ecgx_set_capture_size(sd);
	CHECK_ERR_MSG(err, "fail to set capture size (%d)\n", err);

	if ((state->scene_mode != SCENE_MODE_NIGHTSHOT)
	    && (state->light_level < LUX_LEVEL_LOW)) {
	    	state->capture.lowlux = 1;
		err = s5k4ecgx_set_from_table(sd, "lowlight_cap_on",
				&state->regs->lowlight_cap_on, 1, 0);
		CHECK_ERR(err);
	}

	/* Set flash for capture */
	err = s5k4ecgx_set_capture_flash(sd);
	CHECK_ERR(err);

	/* Transit to capture mode */
	err = s5k4ecgx_transit_capture_mode(sd);

#ifdef CONFIG_VIDEO_FAST_CAPTURE
	s5k4ecgx_start_capmode_checker(sd);
#endif

	/*if (state->scene_mode == SCENE_MODE_NIGHTSHOT)
		msleep_debug(140, true); legacy code*/

	state->runmode = RUNMODE_CAPTURING;

	CHECK_ERR_MSG(err, "fail to capture_mode (%d)\n", err);

	/*s5k4ecgx_get_exif(sd); */

	return err;
}

static int s5k4ecgx_start_fast_capture(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	cam_info("set_fast_capture\n");

	state->req_fmt.width = (state->capture.pre_req >> 16);
	state->req_fmt.height = (state->capture.pre_req & 0xFFFF);
	s5k4ecgx_set_framesize(sd, s5k4ecgx_capture_frmsizes,
		ARRAY_SIZE(s5k4ecgx_capture_frmsizes), false);

	err = s5k4ecgx_start_capture(sd);
	CHECK_ERR(err);

	state->capture.ready = 1;

	return 0;
}

/**
 * s5k4ecgx_switch_hd_mode: swith to/from HD mode
 * @fmt: 
 *
 */
static inline int s5k4ecgx_switch_hd_mode(
		struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct s5k4ecgx_state *state = to_state(sd);
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
		err = s5k4ecgx_reset(sd, 2);
		CHECK_ERR(err);

		s5k4ecgx_init(sd, 0);
		CHECK_ERR(err);
		cam_dbg("******* Reset end *******\n\n");
	}

	return 0;
}

static void s5k4ecgx_init_parameter(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);

	state->runmode = RUNMODE_INIT;

	/* Default state values */
	if (IS_FLASH_SUPPORTED()) {
		state->flash.mode = FLASH_MODE_OFF;
		state->flash.on = 0;
	}

	state->scene_mode = SCENE_MODE_NONE;
	state->light_level = 0xFFFFFFFF;

	if (IS_AF_SUPPORTED()) {
		memset(&state->focus, 0, sizeof(state->focus));
		state->focus.support = IS_AF_SUPPORTED(); /* Don't fix me */
	}
}

static int s5k4ecgx_restore_ctrl(struct v4l2_subdev *sd)
{
	struct v4l2_control ctrl;
	int i;

	cam_trace("EX\n");

	for (i = 0; i < ARRAY_SIZE(s5k4ecgx_ctrls); i++) {
		if (s5k4ecgx_ctrls[i].value !=
		    s5k4ecgx_ctrls[i].default_value) {
			ctrl.id = s5k4ecgx_ctrls[i].id;
			ctrl.value = s5k4ecgx_ctrls[i].value;
			cam_dbg("restore_ctrl: ID 0x%08X, val %d\n",
					ctrl.id, ctrl.value);

			s5k4ecgx_s_ctrl(sd, &ctrl);
		}
	}

	return 0;
}

static int s5k4ecgx_check_sensor(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 read_value = 0;
	int err = -ENODEV;

	err = s5k4ecgx_readw(sd, REG_FW_VER_SENSOR_ID, &read_value,
				S5K_I2C_PAGE);
	if (unlikely(read_value != S5K4ECGX_CHIP_ID)) {
		cam_err("Sensor ChipID: unknown ChipID %04X\n", read_value);
		goto fail;
	}

	err = s5k4ecgx_readw(sd, REG_FW_VER_HW_REV, &read_value, 0);
	if (unlikely(read_value != S5K4ECGX_CHIP_REV)) {
		cam_err("Sensor Rev: unknown Rev 0x%04X\n", read_value);
		goto fail;
	}

	cam_dbg("Sensor chip indentification: Success");

fail:
	/* restore write mode */
	err = __s5k4ecgx_i2c_writew(client, 0x0028, 0x7000);
	CHECK_ERR_COND(err < 0, -ENODEV);

	return 0;
}

static int s5k4ecgx_s_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct s5k4ecgx_state *state = to_state(sd);
	s32 prev_index = 0;

	cam_dbg("%s: pixelformat = 0x%x, colorspace = 0x%x, width = %d, height = %d\n",
		__func__, fmt->code, fmt->colorspace, fmt->width, fmt->height);

	v4l2_fill_pix_format(&state->req_fmt, fmt);

	state->wide_cmd = WIDE_REQ_NONE;

	if (state->format_mode != V4L2_PIX_FMT_MODE_CAPTURE) {
		prev_index = state->preview.frmsize ?
				state->preview.frmsize->index : -1;

		s5k4ecgx_set_framesize(sd, s5k4ecgx_preview_frmsizes,
			ARRAY_SIZE(s5k4ecgx_preview_frmsizes), true);

		/* preview.frmsize cannot absolutely go to null */
#if 0
		if (state->preview.frmsize->index != prev_index) {
			state->preview.update_frmsize = 1;

			if ((state->preview.frmsize->index == PREVIEW_WIDE_SIZE)
			    && !state->hd_videomode) {
				cam_dbg("preview, need to change to WIDE\n");
				state->wide_cmd = WIDE_REQ_CHANGE;
			} else if ((prev_index == PREVIEW_WIDE_SIZE)
			    && !state->hd_videomode) {
				cam_dbg("preview, need to restore form WIDE\n");
				state->wide_cmd = WIDE_REQ_RESTORE;
			}
		}
#else
		state->preview.update_frmsize = 1;
		if (state->preview.frmsize->index != prev_index)
			cam_info("checking unmatched ratio skipped\n");

		if ((SENSOR_MOVIE == state->sensor_mode)
		    && (PREVIEW_SZ_PVGA == state->preview.frmsize->index))
				state->hd_videomode = 1;
		else
				state->hd_videomode = 0;
#endif
	} else {
		s5k4ecgx_set_framesize(sd, s5k4ecgx_capture_frmsizes,
			ARRAY_SIZE(s5k4ecgx_capture_frmsizes), false);

#if 0

		/* For maket app.
		 * Samsung camera app does not use unmatched ratio.*/
		if (unlikely(!state->preview.frmsize))
			cam_warn("warning, capture without preview\n");
		else if (unlikely(FRAMESIZE_RATIO(state->preview.frmsize)
		    != FRAMESIZE_RATIO(state->capture.frmsize))) {
			cam_warn("warning, preview, capture ratio not matched\n\n");
			if (state->capture.frmsize->index == CAPTURE_WIDE_SIZE) {
				cam_dbg("captre: need to change to WIDE\n");
				state->wide_cmd = WIDE_REQ_CHANGE;
			} else {
				cam_dbg("capture, need to restore form WIDE\n");
				state->wide_cmd = WIDE_REQ_RESTORE;
			}
		}
#else
		/*cam_info("checking unmatched ratio skipped\n");*/
#endif
	}

	return 0;
}

#ifdef CONFIG_MACH_TAB3
/**
 * s5k4ecgx_pre_s_mbus_fmt: wrapper function of s_fmt() with HD-checking codes
 * @fmt: 
 *
 * We reset sensor to switch between HD and camera mode before calling the orignal 
 * s_fmt function.
 * I don't want to make this routine...
 */
static int s5k4ecgx_pre_s_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	/* check whether to be HD or normal video, and then transit */
	err = s5k4ecgx_switch_hd_mode(sd, fmt);
	CHECK_ERR(err);

	/* check whether sensor is initialized, against exception cases */
	if (unlikely(!state->initialized)) {
		cam_warn("s_fmt: warning, not initialized\n");
		err = s5k4ecgx_init(sd, 0);
		CHECK_ERR(err);
	}

	/* call original s_mbus_fmt() */
	s5k4ecgx_s_mbus_fmt(sd, fmt);

	return 0;
}
#endif

static int s5k4ecgx_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
					enum v4l2_mbus_pixelcode *code)
{
	cam_dbg("enum_mbus_fmt: index = %d\n", index);

	if (index >= ARRAY_SIZE(capture_fmts))
		return -EINVAL;

	*code = capture_fmts[index].code;

	return 0;
}

static int s5k4ecgx_try_mbus_fmt(struct v4l2_subdev *sd,
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

static int s5k4ecgx_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	struct s5k4ecgx_state *state = to_state(sd);

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

static int s5k4ecgx_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	return 0;
}

static int s5k4ecgx_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct s5k4ecgx_state *state = to_state(sd);
	s32 req_fps;
	
	req_fps = param->parm.capture.timeperframe.denominator /
			param->parm.capture.timeperframe.numerator;

	cam_dbg("s_parm state->fps=%d, state->req_fps=%d\n",
		state->fps,req_fps);

	return s5k4ecgx_set_frame_rate(sd, req_fps);
}

static inline bool s5k4ecgx_is_clear_ctrl(struct v4l2_control *ctrl)
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

static int s5k4ecgx_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k4ecgx_state *state = to_state(sd);
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
			s5k4ecgx_get_exif_flash(sd, (u16 *)ctrl->value);
		break;

	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
	case V4L2_CID_CAM_AUTO_FOCUS_RESULT:
		if (IS_AF_SUPPORTED())
			ctrl->value = state->focus.status;
		else
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

static int s5k4ecgx_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	if (!state->initialized && (ctrl->id != V4L2_CID_CAMERA_SENSOR_MODE)) {
#ifdef CONFIG_MACH_PX
		if (state->sensor_mode == SENSOR_MOVIE)
			return 0;
#endif

		cam_warn("s_ctrl: warning, camera not initialized. ID %d(0x%X)\n",
			ctrl->id & 0xFF, ctrl->id);
		return 0;
	}

	cam_dbg("s_ctrl: ID =0x%08X, val = %d\n", ctrl->id, ctrl->value);

	mutex_lock(&state->ctrl_lock);

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_SENSOR_MODE:
		err = s5k4ecgx_set_sensor_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
	case V4L2_CID_CAM_OBJECT_POSITION_X:
		state->focus.pos_x = ctrl->value;
		break;

	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
	case V4L2_CID_CAM_OBJECT_POSITION_Y:
		state->focus.pos_y = ctrl->value;
		break;

	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
		err = s5k4ecgx_set_touch_af(sd, 1);
		break;

	case V4L2_CID_CAMERA_FOCUS_MODE:
	case V4L2_CID_FOCUS_MODE:
		err = s5k4ecgx_set_focus_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
	case V4L2_CID_CAM_SET_AUTO_FOCUS:
		err = s5k4ecgx_set_af(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FLASH_MODE:
		err = s5k4ecgx_set_flash_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_BRIGHTNESS:
	case V4L2_CID_CAM_BRIGHTNESS:
		err = s5k4ecgx_set_exposure(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_WHITE_BALANCE:
	case V4L2_CID_WHITE_BALANCE_PRESET:
		err = s5k4ecgx_set_whitebalance(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_EFFECT:
	case V4L2_CID_IMAGE_EFFECT:
		err = s5k4ecgx_set_from_table(sd, "effects",
			state->regs->effect,
			ARRAY_SIZE(state->regs->effect), ctrl->value);
		break;

	case V4L2_CID_CAMERA_METERING:
	case V4L2_CID_CAM_METERING:
		err = s5k4ecgx_set_metering(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_CONTRAST:
		err = s5k4ecgx_set_from_table(sd, "contrast",
			state->regs->contrast,
			ARRAY_SIZE(state->regs->contrast), ctrl->value);
		break;

	case V4L2_CID_CAMERA_SATURATION:
		err = s5k4ecgx_set_from_table(sd, "saturation",
			state->regs->saturation,
			ARRAY_SIZE(state->regs->saturation), ctrl->value);
		break;

	case V4L2_CID_CAMERA_SHARPNESS:
		err = s5k4ecgx_set_from_table(sd, "sharpness",
			state->regs->sharpness,
			ARRAY_SIZE(state->regs->sharpness), ctrl->value);
		break;

	case V4L2_CID_CAMERA_SCENE_MODE:
	case V4L2_CID_SCENEMODE:
		err = s5k4ecgx_set_scene_mode(sd, ctrl->value);
		break;
/*
	case V4L2_CID_CAMERA_AE_LOCK_UNLOCK:
		err = s5k4ecgx_set_ae_lock(sd, ctrl->value, false);
		break;

	case V4L2_CID_CAMERA_AWB_LOCK_UNLOCK:
		err = s5k4ecgx_set_awb_lock(sd, ctrl->value, false);
		break;*/

	case V4L2_CID_CAMERA_CHECK_ESD:
		err = s5k4ecgx_check_esd(sd);
		break;

	case V4L2_CID_CAMERA_ANTI_BANDING:
		break;

	case V4L2_CID_CAPTURE:
		if (ctrl->value > 1) {
#ifdef CONFIG_VIDEO_FAST_CAPTURE
			cam_dbg("Fast Capture mode\n");
			state->capture.pre_req = ctrl->value;
#endif
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
		err = s5k4ecgx_set_iso(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FRAME_RATE:
		err = s5k4ecgx_set_frame_rate(sd, ctrl->value);
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

static int s5k4ecgx_pre_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;

	if (s5k4ecgx_is_clear_ctrl(ctrl))
		return 0;

	s5k4ecgx_save_ctrl(sd, ctrl);

	/* Call s_ctrl */
	err = s5k4ecgx_s_ctrl(sd, ctrl);
	CHECK_ERR(err);

	if (IS_PREVIEW_UPDATE_SUPPORTED())
		err = s5k4ecgx_update_preview(sd);

	return err;
}

static int s5k4ecgx_s_ext_ctrl(struct v4l2_subdev *sd,
			      struct v4l2_ext_control *ctrl)
{
	return 0;
}

static int s5k4ecgx_s_ext_ctrls(struct v4l2_subdev *sd,
				struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int ret = 0;
	int i;

	for (i = 0; i < ctrls->count; i++, ctrl++) {
		ret = s5k4ecgx_s_ext_ctrl(sd, ctrl);
		if (unlikely(ret)) {
			ctrls->error_idx = i;
			break;
		}
	}

	return ret;
}

static int s5k4ecgx_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = 0;

	cam_info("#### stream mode = %d\n", enable);

	if (unlikely(!state->initialized)) {
		cam_err("s_stream: not initialized device. start init\n\n");
		err = s5k4ecgx_init(sd, 0);
		CHECK_ERR(err);
	}

	switch (enable) {
	case STREAM_MODE_CAM_OFF:
		err = s5k4ecgx_control_stream(sd, STREAM_STOP);
		break;

	case STREAM_MODE_CAM_ON:
		if (unlikely(state->need_wait_streamoff)) {
			cam_err("error, must wait for stream-off\n\n");
			err = s5k4ecgx_s_stream(sd, STREAM_MODE_WAIT_OFF);
			CHECK_ERR(err);
		}

		if (V4L2_PIX_FMT_MODE_CAPTURE == state->format_mode)
			err = s5k4ecgx_start_capture(sd);
		else
			err = s5k4ecgx_start_preview(sd);
		break;

	case STREAM_MODE_MOVIE_ON:
		cam_info("movie on");
		state->recording = 1;
		if (state->flash.mode != FLASH_MODE_OFF)
			s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_ON);
		break;

	case STREAM_MODE_MOVIE_OFF:
		cam_info("movie off");
		state->recording = 0;
		if (state->flash.on)
			s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_OFF);
		break;

	case STREAM_MODE_WAIT_OFF:
#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
		err = s5k4ecgx_wait_steamoff(sd);
#endif
		break;

	default:
		cam_err("%s: error - Invalid stream mode\n", __func__);
		break;
	}

	CHECK_ERR_MSG(err, "failed\n");

	return 0;
}

/**
 * s5k4ecgx_reset: reset the sensor device
 * @val: 0 - reset parameter.
 *      1 - power reset
 */
static int s5k4ecgx_reset(struct v4l2_subdev *sd, u32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
	int err = -EINVAL;

	cam_info("reset camera sub-device\n");

	s5k4ecgx_check_wait_af_complete(sd, true);
	s5k4ecgx_set_af_softlanding(sd);

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	atomic_set(&state->streamoff_check, false);
#endif

	if (state->wq)
		flush_workqueue(state->wq);

	state->initialized = 0;
	state->need_wait_streamoff = 0;
	state->runmode = RUNMODE_NOTREADY;

	if (val) {
		if (S5K4ECGX_HW_POWER_ON == state->power_on) {
			err = s5k4ecgx_power(sd, 0);
			CHECK_ERR(err);
			msleep_debug(50, true);
		} else {
			cam_err("reset: sensor is not powered. power status %d\n",
				state->power_on);
		}

		err = s5k4ecgx_power(sd, 1);
		CHECK_ERR(err);
	}

	state->reset_done = 1;
	stats_reset++;

	return 0;
}

static int s5k4ecgx_init(struct v4l2_subdev *sd, u32 val)
{
	struct s5k4ecgx_state *state = to_state(sd);
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

	err = s5k4ecgx_check_sensor(sd);
	CHECK_ERR_MSG(err, "failed to indentify sensor chip\n");

	if (0 /* state->hd_videomode*/) {
		cam_info("init: HD mode\n");
		/*err = S5K4ECGX_BURST_WRITE_REGS(sd, s5k5ccgx_hd_init_reg);
		CHECK_ERR_MSG(err, "failed to initialize camera device\n");*/
	} else {
		cam_info("init: Cam, Non-HD mode\n");
		err = S5K4ECGX_BURST_WRITE_REGS(sd, s5k4ecgx_init_regs);
		CHECK_ERR_MSG(err, "failed to initialize camera device\n");

	}

#if 0
	s5k4ecgx_set_from_table(sd, "antibanding",
		&state->regs->antibanding, 1, 0);
#endif

	s5k4ecgx_init_parameter(sd);
	state->initialized = 1;
	stats_init++;

	if (state->reset_done) {
		state->reset_done = 0;
		s5k4ecgx_restore_ctrl(sd);
	}

	return 0;
}

static int s5k4ecgx_s_power(struct v4l2_subdev *sd, int on)
{
	/* struct s5k4ecgx_state *state = to_state(sd);*/
	int tries = 3, err = 0;

	if (on) {
		stats_power++;
#ifdef CONFIG_CAM_EARYLY_PROBE
		err = s5k4ecgx_late_probe(sd);
#else
		err = s5k4ecgx_get_power(sd);
#endif
		CHECK_ERR(err);
retry:
		err = s5k4ecgx_power(sd, 1);
		if (unlikely(err))
			goto out_fail;

		err = s5k4ecgx_init(sd, 0);
		if (err && --tries) {
			cam_err("fail to init device. retry to init (%d)", tries);
			s5k4ecgx_power(sd, 0);
			msleep_debug(50, true);
			goto retry;
		} else if (!tries)
			goto out_fail;
	} else {
#ifdef CONFIG_CAM_EARYLY_PROBE
		err = s5k4ecgx_early_remove(sd);
#else
		err = s5k4ecgx_power(sd, 0);
		CHECK_ERR(err);

		err = s5k4ecgx_put_power(sd);
#endif
	}

	return err;

out_fail:
	cam_err("s_power: error, couldn't init device");
	s5k4ecgx_s_power(sd, 0);
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
static int s5k4ecgx_s_config(struct v4l2_subdev *sd,
			int irq, void *platform_data)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_platform_data *pdata = platform_data;
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
	pdata->common.streamoff_delay = S5K4ECGX_STREAMOFF_DELAY;
	pdata->common.dbg_level = CAMDBG_LEVEL_DEFAULT;
	pdata->common.flash_en = __hwflash_en;
	pdata->is_flash_on = __is_hwflash_on;
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

	state->preview.frmsize = s5k4ecgx_get_framesize_i(s5k4ecgx_preview_frmsizes,
					ARRAY_SIZE(s5k4ecgx_preview_frmsizes),
					PREVIEW_SZ_VGA);
	state->capture.frmsize = s5k4ecgx_get_framesize_i(s5k4ecgx_capture_frmsizes,
					ARRAY_SIZE(s5k4ecgx_capture_frmsizes),
					CAPTURE_SZ_5MP);
	state->sensor_mode = SENSOR_CAMERA;
	state->hd_videomode = 0;
	state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
	state->fps = FRAME_RATE_AUTO;
	state->req_fps = -1;
	state->iso = ISO_AUTO;

	for (i = 0; i < ARRAY_SIZE(s5k4ecgx_ctrls); i++)
		s5k4ecgx_ctrls[i].value = s5k4ecgx_ctrls[i].default_value;

	if (s5k4ecgx_is_hwflash_on(sd))
		state->flash.ignore_flash = 1;

	return 0;
}

static const struct v4l2_subdev_core_ops s5k4ecgx_core_ops = {
	.s_power = s5k4ecgx_s_power,
	.init = s5k4ecgx_init,	/* initializing API */
	.g_ctrl = s5k4ecgx_g_ctrl,
	.s_ctrl = s5k4ecgx_pre_s_ctrl,
	.s_ext_ctrls = s5k4ecgx_s_ext_ctrls,
	.reset = s5k4ecgx_reset,
};

static const struct v4l2_subdev_video_ops s5k4ecgx_video_ops = {
#ifdef CONFIG_MACH_TAB3
	.s_mbus_fmt = s5k4ecgx_pre_s_mbus_fmt,
#else
	.s_mbus_fmt = s5k4ecgx_s_mbus_fmt,
#endif
	.enum_framesizes = s5k4ecgx_enum_framesizes,
	.enum_mbus_fmt = s5k4ecgx_enum_mbus_fmt,
	.try_mbus_fmt = s5k4ecgx_try_mbus_fmt,
	.g_parm = s5k4ecgx_g_parm,
	.s_parm = s5k4ecgx_s_parm,
	.s_stream = s5k4ecgx_s_stream,
};

static struct v4l2_mbus_framefmt *__find_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				enum v4l2_subdev_format_whence which,
				enum s5k4ecgx_oprmode type)
{
	struct s5k4ecgx_state *state = to_state(sd);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return fh ? v4l2_subdev_get_try_format(fh, 0) : NULL;

	return &state->ffmt;
}

/* get format by flite video device command */
static int s5k4ecgx_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct s5k4ecgx_state *state = to_state(sd);
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
static int s5k4ecgx_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct v4l2_mbus_framefmt *format = &fmt->format;
	struct v4l2_mbus_framefmt *sfmt;
	const struct s5k4ecgx_framesize *framesize;

	if (fmt->pad != 0)
		return -EINVAL;

	state->oprmode = S5K4ECGX_OPRMODE_VIDEO;

	sfmt		= &default_fmt[state->oprmode];
	sfmt->width	= format->width;
	sfmt->height	= format->height;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		s5k4ecgx_s_mbus_fmt(sd, sfmt);  /* set format */

		if (V4L2_PIX_FMT_MODE_CAPTURE == state->format_mode)
			framesize = state->capture.frmsize;
		else
			framesize = state->preview.frmsize;

		/* for enum size of entity by flite */
		state->ffmt.width = format->width = framesize->width;
		state->ffmt.height = format->height = framesize->height;
		state->ffmt.colorspace = format->colorspace;

#ifndef CONFIG_VIDEO_S5K4ECGX_SENSOR_JPEG
		state->ffmt.code 	= V4L2_MBUS_FMT_YUYV8_2X8;
#else
		state->ffmt.code 	= format->code;
#endif
	}

	return 0;
}

/* enum code by flite video device command */
static int s5k4ecgx_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (!code || (code->index >= ARRAY_SIZE(default_fmt)))
		return -EINVAL;

	code->code = default_fmt[code->index].code;

	return 0;
}


static struct v4l2_subdev_pad_ops s5k4ecgx_pad_ops = {
	.enum_mbus_code	= s5k4ecgx_enum_mbus_code,
	.get_fmt	= s5k4ecgx_get_fmt,
	.set_fmt	= s5k4ecgx_set_fmt,
};

static const struct v4l2_subdev_ops s5k4ecgx_ops = {
	.core = &s5k4ecgx_core_ops,
	.pad = &s5k4ecgx_pad_ops,
	.video = &s5k4ecgx_video_ops,
};

#ifdef CONFIG_MEDIA_CONTROLLER
static int s5k4ecgx_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static const struct media_entity_operations s5k4ecgx_media_ops = {
	.link_setup = s5k4ecgx_link_setup,
};
#endif

/* internal ops for media controller */
static int s5k4ecgx_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
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
	s5k4ecgx_set_fmt(sd, fh, &format);
	s5k4ecgx_s_parm(sd, &state->strm);
#endif

	return 0;
}

static int s5k4ecgx_subdev_close(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	s5k4ecgx_debug(S5K4ECGX_DEBUG_I2C, "%s", __func__);
	pr_info("%s", __func__);
	return 0;
}

static int s5k4ecgx_subdev_registered(struct v4l2_subdev *sd)
{
	s5k4ecgx_debug(S5K4ECGX_DEBUG_I2C, "%s", __func__);
	return 0;
}

static void s5k4ecgx_subdev_unregistered(struct v4l2_subdev *sd)
{
	s5k4ecgx_debug(S5K4ECGX_DEBUG_I2C, "%s", __func__);
}

static const struct v4l2_subdev_internal_ops s5k4ecgx_v4l2_internal_ops = {
	.open = s5k4ecgx_init_formats,
	.close = s5k4ecgx_subdev_close,
	.registered = s5k4ecgx_subdev_registered,
	.unregistered = s5k4ecgx_subdev_unregistered,
};

/*
 * s5k4ecgx_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
#ifdef CONFIG_CAM_EARYLY_PROBE
static int s5k4ecgx_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct s5k4ecgx_core_state *c_state;
	struct s5k4ecgx_state *state;
	int err = -EINVAL;

	c_state = kzalloc(sizeof(struct s5k4ecgx_core_state), GFP_KERNEL);
	if (unlikely(!c_state)) {
		dev_err(&client->dev, "early_probe, fail to get memory\n");
		return -ENOMEM;
	}

	state = kzalloc(sizeof(struct s5k4ecgx_state), GFP_KERNEL);
	if (unlikely(!state)) {
		dev_err(&client->dev, "early_probe, fail to get memory\n");
		goto err_out2;
	}

	c_state->data = (u32)state;
	sd = &c_state->sd;
	strcpy(sd->name, driver_name);
	strcpy(c_state->c_name, "s5k4ecgx_core_state"); /* for debugging */

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &s5k4ecgx_ops);

#ifdef CONFIG_MEDIA_CONTROLLER
	c_state->pad.flags = MEDIA_PAD_FL_SOURCE;
	err = media_entity_init(&sd->entity, 1, &c_state->pad, 0);
	if (unlikely(err)) {
		dev_err(&client->dev, "probe: fail to init media entity\n");
		goto err_out1;
	}

	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->entity.ops = &s5k4ecgx_media_ops;
#endif

	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &s5k4ecgx_v4l2_internal_ops;

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

static int s5k4ecgx_late_probe(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct s5k4ecgx_core_state *c_state = to_c_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	if (unlikely(!c_state || !state)) {
		dev_err(&client->dev, "late_probe, fail to get memory."
			" c_state = 0x%X, state = 0x%X\n", (u32)c_state, (u32)state);
		return -ENOMEM;
	}

	memset(state, 0, sizeof(struct s5k4ecgx_state));
	state->c_state = c_state;
	state->sd = sd;
	state->wq = c_state->wq;
	strcpy(state->s_name, "s5k4ecgx_state"); /* for debugging */

	mutex_init(&state->ctrl_lock);
	mutex_init(&state->af_lock);
	mutex_init(&state->i2c_lock);
#if defined(CONFIG_VIDEO_FAST_CAPTURE) || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	init_completion(&state->streamoff_complete);
#endif

	state->runmode = RUNMODE_NOTREADY;

	err = s5k4ecgx_s_config(sd, 0, client->dev.platform_data);
	CHECK_ERR_MSG(err, "probe: fail to s_config\n");

	if (IS_AF_SUPPORTED()) {
		INIT_WORK(&state->af_work, s5k4ecgx_af_worker);
		INIT_WORK(&state->af_win_work, s5k4ecgx_af_win_worker);
	}

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	INIT_WORK(&state->streamoff_work, s5k4ecgx_streamoff_checker);
#endif
#ifdef CONFIG_VIDEO_FAST_CAPTURE
	INIT_WORK(&state->capmode_work, s5k4ecgx_capmode_checker);
#endif

	state->mclk = clk_get(NULL, "cam1");
	if (IS_ERR_OR_NULL(state->mclk)) {
		pr_err("failed to get cam1 clk (mclk)");
		return -ENXIO;
	}

	err = s5k4ecgx_get_power(sd);
	CHECK_ERR_GOTO_MSG(err, err_out, "probe: fail to get power\n");

	printk(KERN_DEBUG "%s %s: driver late probed!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;

err_out:
	s5k4ecgx_put_power(sd);
	return -ENOMEM;
}

static int s5k4ecgx_early_remove(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0, ret = 0;

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	if (atomic_read(&state->streamoff_check))
		s5k4ecgx_stop_streamoff_checker(sd);
#endif
#ifdef CONFIG_VIDEO_FAST_CAPTURE
	if (atomic_read(&state->capmode_check))
		s5k4ecgx_stop_capmode_checker(sd);
#endif

	if (state->wq)
		flush_workqueue(state->wq);

	/* for softlanding */
	s5k4ecgx_set_af_softlanding(sd);

	/* Check whether flash is on when unlolading driver,
	 * to preventing Market App from controlling improperly flash.
	 * It isn't necessary in case that you power flash down
	 * in power routine to turn camera off.*/
	if (unlikely(state->flash.on && !state->flash.ignore_flash))
		s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_OFF);

	err = s5k4ecgx_power(sd, 0);
	if (unlikely(err)) {
		cam_info("remove: power off failed. %d\n", err);
		ret = err;
	}

	err = s5k4ecgx_put_power(sd);
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

static int s5k4ecgx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4ecgx_core_state *c_state = to_c_state(sd);

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

static int s5k4ecgx_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct s5k4ecgx_state *state;
	int err = -EINVAL;

	state = kzalloc(sizeof(struct s5k4ecgx_state), GFP_KERNEL);
	if (unlikely(!state)) {
		dev_err(&client->dev, "probe, fail to get memory\n");
		return -ENOMEM;
	}

	mutex_init(&state->ctrl_lock);
	mutex_init(&state->af_lock);

	state->runmode = RUNMODE_NOTREADY;
	sd = &state->sd;
	strcpy(sd->name, driver_name);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &s5k4ecgx_ops);
#ifdef CONFIG_MEDIA_CONTROLLER
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	err = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (unlikely(err)) {
		dev_err(&client->dev, "probe: fail to init media entity\n");
		goto err_out;
	}
#endif

	err = s5k4ecgx_s_config(sd, 0, client->dev.platform_data);
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

	if (IS_AF_SUPPORTED()) {
		INIT_WORK(&state->af_work, s5k4ecgx_af_worker);
		INIT_WORK(&state->af_win_work, s5k4ecgx_af_win_worker);
	}

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	INIT_WORK(&state->streamoff_work, s5k4ecgx_streamoff_checker);
#endif
#ifdef CONFIG_VIDEO_FAST_CAPTURE
	INIT_WORK(&state->capmode_work, s5k4ecgx_capmode_checker);
#endif

/*
	err = s5k4ecgx_get_power(sd);
	if (unlikely(err)) {
		dev_err(&client->dev, "probe: fail to get power\n");
		goto err_out;
	} */

#ifdef CONFIG_MEDIA_CONTROLLER
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->entity.ops = &s5k4ecgx_media_ops;
#endif
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &s5k4ecgx_v4l2_internal_ops;
	
	printk(KERN_DEBUG "%s %s: driver probed!!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;

err_out:
	/* s5k4ecgx_put_power(sd); */
	kfree(state);
	return -ENOMEM;
}

static int s5k4ecgx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4ecgx_state *state = to_state(sd);

#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	if (atomic_read(&state->streamoff_check))
		s5k4ecgx_stop_streamoff_checker(sd);
#endif
#ifdef CONFIG_VIDEO_FAST_CAPTURE
	if (atomic_read(&state->capmode_check))
		s5k4ecgx_stop_capmode_checker(sd);
#endif

	if (state->wq)
		destroy_workqueue(state->wq);

	/* for softlanding */
	s5k4ecgx_set_af_softlanding(sd);

	/* Check whether flash is on when unlolading driver,
	 * to preventing Market App from controlling improperly flash.
	 * It isn't necessary in case that you power flash down
	 * in power routine to turn camera off.*/
	if (unlikely(state->flash.on && !state->flash.ignore_flash))
		s5k4ecgx_flash_torch(sd, S5K4ECGX_FLASH_OFF);

	s5k4ecgx_power(sd, 0);
	s5k4ecgx_put_power(sd);

#ifdef CONFIG_MEDIA_CONTROLLER
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_device_unregister_subdev(sd);
	mutex_destroy(&state->ctrl_lock);
	mutex_destroy(&state->af_lock);
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
	return sprintf(buf, "%s_%s\n", "LSI", driver_name);
}
static DEVICE_ATTR(rear_camtype, S_IRUGO, camtype_show, NULL);

static ssize_t camfw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s %s\n", driver_name, driver_name);

}
static DEVICE_ATTR(rear_camfw, S_IRUGO, camfw_show, NULL);

#if CONFIG_SUPPORT_FLASH
static ssize_t flash_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%s\n", __is_hwflash_on() ? "on" : "off");
}

static ssize_t flash_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	__hwflash_en(S5K4ECGX_FLASH_MODE_MOVIE,
		(*buf == '0') ? S5K4ECGX_FLASH_OFF : S5K4ECGX_FLASH_ON);

	return count;
}
static DEVICE_ATTR(rear_flash, 0664, flash_show, flash_store);
#endif /* CONFIG_SUPPORT_FLASH */

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

int s5k4ecgx_create_sysfs(struct class *cls)
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

#if CONFIG_SUPPORT_FLASH
int s5k4ecgx_create_flash_sysfs(void)
{
        struct device *dev = NULL;
        int err = -ENODEV;

        dev = device_create(camera_class, NULL, 0, NULL, "flash");
        if (IS_ERR(dev)) {
                pr_err("cam_init: failed to create device(flash)\n");
                return -ENODEV;
        }    

        err = device_create_file(dev, &dev_attr_rear_flash);
        if (unlikely(err < 0))
                pr_err("cam_init: failed to create device file, %s\n",
                        dev_attr_rear_flash.attr.name);

        return 0;
}
#endif

#endif /* CONFIG_SUPPORT_CAMSYSFS */

static const struct i2c_device_id s5k4ecgx_id[] = {
	{ S5K4ECGX_DRIVER_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, s5k4ecgx_id);

static struct i2c_driver v4l2_i2c_driver = {
	.driver.name	= driver_name,
	.probe		= s5k4ecgx_probe,
	.remove		= s5k4ecgx_remove,
	.id_table	= s5k4ecgx_id,
};

static int __init v4l2_i2c_drv_init(void)
{
	int err = 0;

	pr_info("%s: %s init\n", __func__, driver_name);

#if CONFIG_SUPPORT_FLASH
#ifdef CONFIG_MACH_GARDA
	err = gpio_request(GPIO_CAM_FLASH_SET, "GPM3");
	if (unlikely(err))
		pr_err("i2c_drv_init: fail to get gpio(FLASH_SET), %d\n", err);

	err = gpio_request(GPIO_CAM_FLASH_EN, "GPM3");
	if (unlikely(err))
		pr_err("i2c_drv_init: fail to get gpio(FLASH_EN), %d\n", err);

	err = __hwflash_init(1);
	if (unlikely(err))
		pr_err("i2c_drv_init: fail to init flash. %d\n", err);
#endif /* CONFIG_MACH_GARDA */
#endif /* CONFIG_SUPPORT_FLASH */

#if CONFIG_SUPPORT_CAMSYSFS
	s5k4ecgx_create_sysfs(camera_class);
#if CONFIG_SUPPORT_FLASH
	s5k4ecgx_create_flash_sysfs();
#endif
#endif /* CONFIG_SUPPORT_CAMSYSFS */

	return i2c_add_driver(&v4l2_i2c_driver);
}

static void __exit v4l2_i2c_drv_cleanup(void)
{
	pr_info("%s: %s exit\n", __func__, driver_name);

#ifdef CONFIG_MACH_GARDA
	gpio_free(GPIO_CAM_FLASH_SET);
	gpio_free(GPIO_CAM_FLASH_EN);
#endif

	i2c_del_driver(&v4l2_i2c_driver);
}

module_init(v4l2_i2c_drv_init);
module_exit(v4l2_i2c_drv_cleanup);

MODULE_DESCRIPTION("LSI S5K4ECGX 5MP SOC camera driver");
MODULE_AUTHOR("Dong-Seong Lim <dongseong.lim@samsung.com>");
MODULE_LICENSE("GPL");
