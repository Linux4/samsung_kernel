/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <media/v4l2-subdev.h>
#include <mach/gpio.h>
#include <mach/camera.h>

#include <asm/mach-types.h>
#include <mach/vreg.h>
#include <linux/io.h>

#include "msm.h"
#include "s5k5ccgx.h"
#if defined(CONFIG_MACH_ESPRESSO_VZW) || defined(CONFIG_MACH_ESPRESSO_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR)
#include "s5k5ccgx_regs_espresso.h"
#elif defined(CONFIG_MACH_ESPRESSO10_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_SPR)
#include "s5k5ccgx_regs_espresso10.h"
#else /* JASPER */
#include "s5k5ccgx_regs.h"
#endif
#include "msm.h"
#include "msm_ispif.h"
#include "msm_sensor.h"

/*#define CONFIG_LOAD_FILE */

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static char *s5k5ccgx_regs_table;
static int s5k5ccgx_regs_table_size;
static int s5k5ccgx_write_regs_from_sd(char *name);
static int s5k5ccgx_i2c_write_multi(unsigned short addr, unsigned int w_data);
#endif

static int s5k5ccgx_set_whitebalance(int wb);
static int s5k5ccgx_set_effect(int effect);
static void s5k5ccgx_set_ev(int ev);
static int s5k5ccgx_set_af_mode(int mode);
static int s5k5ccgx_set_ae_awb(int lock);
static int s5k5ccgx_reset_AF_region(void);
static void s5k5ccgx_set_scene_mode(int mode);
static void s5k5ccgx_set_metering(int mode);
static int s5k5ccgx_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp);
static void s5k5ccgx_exif_shutter_speed(void);
static void s5k5ccgx_exif_iso(void);

#if defined(CONFIG_MACH_ESPRESSO_VZW)
int cam_mode;
#endif

struct s5k5ccgx_exif_data
{
	unsigned short iso;
	unsigned short shutterspeed;
};

static struct s5k5ccgx_exif_data *s5k5ccgx_exif;
DEFINE_MUTEX(s5k5ccgx_mut);

//#ifdef CONFIG_LOAD_FILE
#define I2C_BURST_WRITE
#ifndef I2C_BURST_WRITE
#define S5K5CCGX_WRT_LIST(A)	\
	 s5k5ccgx_i2c_wrt_list(A, (sizeof(A) / sizeof(A[0])), #A);
#else
#define S5K5CCGX_WRT_LIST(A)	\
	s5k5ccgx_i2c_burst_wrt_list(A, (sizeof(A) / sizeof(A[0])), #A);
#endif

#define CAM_REV ((system_rev <= 1) ? 0 : 1)

struct s5k5ccgx_work {
	struct work_struct work;
};

static struct  i2c_client *s5k5ccgx_client;
static struct msm_sensor_ctrl_t s5k5ccgx_s_ctrl;
static struct device s5k5ccgx_dev;

struct s5k5ccgx_ctrl {
	const struct msm_camera_sensor_info *sensordata;
	struct s5k5ccgx_userset settings;
	struct msm_camera_i2c_client *sensor_i2c_client;
	struct msm_sensor_ctrl_t *s_ctrl;
	struct v4l2_subdev *sensor_dev;
	struct v4l2_subdev sensor_v4l2_subdev;
	struct v4l2_subdev_info *sensor_v4l2_subdev_info;
	uint8_t sensor_v4l2_subdev_info_size;
	struct v4l2_subdev_ops *sensor_v4l2_subdev_ops;

	int op_mode;
	int dtp_mode;
	int cam_mode;
	int vtcall_mode;
	int started;
	int dtpTest;
	int isCapture;
	unsigned int ae_awb_lock;
	int touchaf_enable;
	int af_mode;
};

static unsigned int config_csi2;
static struct s5k5ccgx_ctrl *s5k5ccgx_ctrl;
int af_low_lux;
static bool mode_enable;

struct s5k5ccgx_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	u16 fmt;
	u16 order;
};


#ifdef CONFIG_LOAD_FILE

void s5k5ccgx_regs_table_init(void)
{
	struct file *filp;
	char *dp;
	long lsize;
	loff_t pos;
	int ret;

	/*Get the current address space */
	mm_segment_t fs = get_fs();

	CAM_DEBUG("%s %d", __func__, __LINE__);

	/*Set the current segment to kernel data segment */
	set_fs(get_ds());

	filp = filp_open("/mnt/sdcard/s5k5ccgx_regs.h", O_RDONLY, 0);

	if (IS_ERR_OR_NULL(filp)) {
		cam_err("file open error\n");
		return ;
	}

	lsize = filp->f_path.dentry->d_inode->i_size;
	CAM_DEBUG("size : %ld", lsize);
	dp = vmalloc(lsize);
	if (dp == NULL) {
		cam_err("Out of Memory");
		filp_close(filp, current->files);
	}

	pos = 0;
	memset(dp, 0, lsize);
	ret = vfs_read(filp, (char __user *)dp, lsize, &pos);
	if (ret != lsize) {
		cam_err("Failed to read file ret = %d\n", ret);
		vfree(dp);
		filp_close(filp, current->files);
	}
	/*close the file*/
	filp_close(filp, current->files);

	/*restore the previous address space*/
	set_fs(fs);

	s5k5ccgx_regs_table = dp;

	s5k5ccgx_regs_table_size = lsize;

	*((s5k5ccgx_regs_table + s5k5ccgx_regs_table_size) - 1) = '\0';

	CAM_DEBUG("s5k5ccgx_reg_table_init");

	return;
}
#endif

#ifdef CONFIG_LOAD_FILE

void s5k5ccgx_regs_table_exit(void)
{
	CAM_DEBUG("%s %d", __func__, __LINE__);
	if (s5k5ccgx_regs_table) {
		vfree(s5k5ccgx_regs_table);
		s5k5ccgx_regs_table = NULL;
	}
}

#endif

#ifdef CONFIG_LOAD_FILE
static int s5k5ccgx_write_regs_from_sd(char *name)
{
	char *start, *end, *reg, *size;
	unsigned short addr;
	unsigned int len, value;
	char reg_buf[7], data_buf1[5], data_buf2[7];


	*(reg_buf + 6) = '\0';
	*(data_buf1 + 4) = '\0';
	*(data_buf2 + 6) = '\0';

	CAM_DEBUG("s5k5ccgx_regs_table_write start!");
	CAM_DEBUG("E string = %s", name);

	start = strstr(s5k5ccgx_regs_table, name);
	end = strstr(start, "};");

	while (1) {
		/* Find Address */
		reg = strstr(start, "{0x");

		if ((reg == NULL) || (reg > end))
			break;
		/* Write Value to Address */
		if (reg != NULL) {
			memcpy(reg_buf, (reg + 1), 6);
			memcpy(data_buf2, (reg + 9), 6);

			size = strstr(data_buf2, ",");
			if (size) { /* 1 byte write */
				memcpy(data_buf1, (reg + 9), 4);
				kstrtoint(reg_buf, 16, &addr);
				kstrtoint(data_buf1, 16, &value);

				if (reg)
					start = (reg + 12);
			} else {/* 2 byte write */
				kstrtoint(reg_buf, 16, &addr);
				kstrtoint(data_buf2, 16, &value);
				if (reg)
					start = (reg + 14);
			}
			size = NULL;

			CAM_DEBUG("addr 0x%04x, value 0x%04x", addr, value);

			if (addr == 0xFFFF)
				msleep(value);
			else
				s5k5ccgx_i2c_write_multi(addr, value);

		}
	}
	CAM_DEBUG("s5k5ccgx_regs_table_write end!");

	return 0;
}

#endif

static DECLARE_WAIT_QUEUE_HEAD(s5k5ccgx_wait_queue);

/**
 * s5k5ccgx_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 * @r_data: buffer where data is read
 * @r_len: number of bytes to read
 *
 * Returns 0 on success, <0 on error
 */
#if 0
static int s5k5ccgx_i2c_read_multi(unsigned short subaddr, unsigned long *data)
{
	unsigned char buf[4];
	struct i2c_msg msg = {s5k5ccgx_client->addr, 0, 2, buf};

	int err = 0;

	if (!s5k5ccgx_client->adapter)
		return -EIO;

	buf[0] = subaddr >> 8;
	buf[1] = subaddr & 0xFF;

	err = i2c_transfer(s5k5ccgx_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;

	msg.flags = I2C_M_RD;
	msg.len = 4;

	err = i2c_transfer(s5k5ccgx_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;
	/*
	 * Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */

	*data = *(unsigned long *)(&buf);

	return err;
}
#endif

/**
 * s5k5ccgx_i2c_read: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static int s5k5ccgx_i2c_read(unsigned short subaddr, unsigned short *data)
{
	unsigned char buf[2];
	struct i2c_msg msg = {s5k5ccgx_client->addr, 0, 2, buf};

	int err = 0;

	if (!s5k5ccgx_client->adapter)
		return -EIO;

	buf[0] = subaddr >> 8;
	buf[1] = subaddr & 0xFF;

	err = i2c_transfer(s5k5ccgx_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;

	msg.flags = I2C_M_RD;

	err = i2c_transfer(s5k5ccgx_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;
	/*
		byte swapping needed here
	 */

	*data = ((buf[0] << 8) | buf[1]);

	return err;
}

/**
 * s5k5ccgx_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int s5k5ccgx_i2c_write_multi(unsigned short addr, unsigned int w_data)
{
	unsigned char buf[4];
	struct i2c_msg msg = {s5k5ccgx_client->addr, 0, 4, buf};

	int retry_count = 5;
	int err = 0;

	if (!s5k5ccgx_client->adapter)
		return -EIO;

	buf[0] = addr >> 8;
	buf[1] = addr & 0xFF;
	buf[2] = w_data >> 8;
	buf[3] = w_data & 0xFF;
	/*
	 * Data should be written in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */

	while (retry_count--) {
		err  = i2c_transfer(s5k5ccgx_client->adapter, &msg, 1);
		if (likely(err == 1))
			break;
	}
	return (err == 1) ? 0 : -EIO;
}
#ifdef I2C_BURST_WRITE
#define S5K5CCGX_BURST_DATA_LENGTH 2700
unsigned char s5k5ccgx_buf[S5K5CCGX_BURST_DATA_LENGTH];
static int s5k5ccgx_i2c_burst_wrt_list(struct s5k5ccgx_short_t regs[], int size,
	char *name)
{
	int err = 1;

	unsigned short subaddr = 0;
	unsigned short next_subaddr = 0;
	unsigned short value = 0;

	int retry_count = 5;
	int i = 0, idx = 0;

	struct i2c_msg msg = {s5k5ccgx_client->addr, 0, 0, s5k5ccgx_buf};

	if (!s5k5ccgx_client->adapter) {
		CAM_DEBUG("%s: %d can't search i2c client adapter\n",
			__func__, __LINE__);
		return -EIO;
	}

	for (i = 0; i < size; i++) {
		if (idx > (S5K5CCGX_BURST_DATA_LENGTH - 10)) {
			CAM_DEBUG("%s: %d BURST MODE buffer overflow!!!\n\n",
				__func__, __LINE__);
		}

		subaddr = regs[i].subaddr; /* address */
		if (subaddr == 0x0F12)
			next_subaddr = regs[i+1].subaddr; /* address */
		value = (regs[i].value & 0xFFFF); /* value */

		retry_count = 5;

		switch (subaddr) {
		case 0x0F12:
			/* make and fill buffer for burst mode write */
			if (idx == 0) {
				s5k5ccgx_buf[idx++] = 0x0F;
				s5k5ccgx_buf[idx++] = 0x12;
			}
			s5k5ccgx_buf[idx++] = value >> 8;
			s5k5ccgx_buf[idx++] = value & 0xFF;

			/* write in burstmode */
			if (next_subaddr != 0x0F12) {
				msg.len = idx;
				while (retry_count--) {
					err = i2c_transfer(
					s5k5ccgx_client->adapter, &msg, 1);
					if (likely(err == 1))
						break;
				}
				idx = 0;
			}
			break;

		case 0xFFFF:
			msleep(value);
			break;

		default:
		    idx = 0;
		    s5k5ccgx_i2c_write_multi(subaddr, value);
			break;
		}
	}

	if (err != 1) {
		pr_err("%s: returned error, %d\n", __func__, err);
		return -EIO;
	}

	return 0;
}
#else
static int s5k5ccgx_i2c_wrt_list(struct s5k5ccgx_short_t regs[],
	int size, char *name)
{
#ifdef CONFIG_LOAD_FILE
	s5k5ccgx_write_regs_from_sd(name);
#else
	int err = 0;
	int i = 0;

	CAM_DEBUG("%s", name);

	if (!s5k5ccgx_client->adapter) {
		cam_err("Can't search i2c client adapter");
		return -EIO;
	}

	for (i = 0; i < size; i++) {
		if (regs[i].subaddr == 0xFFFF) {
			msleep(regs[i].value);
			CAM_DEBUG("delay = 0x%04x, value = 0x%04x",
						regs[i].subaddr, regs[i].value);
		} else {
			err = s5k5ccgx_i2c_write_multi(regs[i].subaddr,
								regs[i].value);
			if (unlikely(err < 0)) {
				cam_err("register set failed");
				return -EIO;
				}
			}
		}
#endif

	return 0;
}
#endif

int s5k5ccgx_get_light_level(void)
{
	unsigned short	msb, lsb;
	unsigned short cur_lux = 0;

	msb = lsb = 0;
	s5k5ccgx_i2c_write_multi(0xFCFC, 0xD000);
	s5k5ccgx_i2c_write_multi(0x002C, 0x7000);
	s5k5ccgx_i2c_write_multi(0x002E, 0x2A3C);
	s5k5ccgx_i2c_read(0x0F12, &lsb);
	s5k5ccgx_i2c_read(0x0F12, &msb);

	cur_lux = (msb<<16) | lsb;

	CAM_DEBUG("cur_lux = %d", cur_lux);

	return cur_lux;
}

#ifdef FACTORY_TEST
struct class *sec_class;
struct device *s5k5ccgx_dev;

static ssize_t cameratype_file_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	char sensor_info[30] = "s5k5ccgx";
	return snprintf(buf, "%s\n", sensor_info);
}

static ssize_t cameratype_file_cmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
		/*Reserved*/
	return size;
}

static struct device_attribute s5k5ccgx_camtype_attr = {
	.attr = {
		.name = "camtype",
		.mode = (S_IRUGO | S_IWUGO)},
		.show = cameratype_file_cmd_show,
		.store = cameratype_file_cmd_store
};

static ssize_t cameraflash_file_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
		/*Reserved*/
	return 0;
}

static ssize_t cameraflash_file_cmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	int value;
	sscanf(buf, "%d", &value);

	if (value == 0) {
		CAM_DEBUG("[Factory flash]OFF");
		s5k5ccgx_set_flash(MOVIE_FLASH, 0);
	} else {
		CAM_DEBUG("[Factory flash]ON");
		s5k5ccgx_set_flash(MOVIE_FLASH, 1);
	}
	return size;
}

static struct device_attribute s5k5ccgx_cameraflash_attr = {
	.attr = {
		.name = "cameraflash",
		.mode = (S_IRUGO | S_IWUGO)},
		.show = cameraflash_file_cmd_show,
		.store = cameraflash_file_cmd_store
};
#endif

static int s5k5ccgx_get_exif(int exif_cmd)
{
	unsigned short val=0;
	CAM_DEBUG("E");

	switch (exif_cmd) {
	case EXIF_SHUTTERSPEED:
		val = s5k5ccgx_exif->shutterspeed;
		break;

	case EXIF_ISO:
		val = s5k5ccgx_exif->iso;
		CAM_DEBUG("exif iso value read : %d\n", val);
		break;

	default:
		CAM_DEBUG("invalid cmd: %d", exif_cmd);
		break;
	}

	CAM_DEBUG("X");
	return val;
}

static void s5k5ccgx_exif_shutter_speed()
{
	unsigned short lsb, msb;
	//unsigned val = 0;
	lsb = msb = 0;
	S5K5CCGX_WRT_LIST(s5k5ccgx_sht_spd);
	s5k5ccgx_i2c_read(0x0F12, &lsb);
	s5k5ccgx_i2c_read(0x0F12, &msb);
	s5k5ccgx_exif->shutterspeed = 400000 / ((msb << 16) + lsb);
	CAM_DEBUG("Exposure time = %d\n", s5k5ccgx_exif->shutterspeed);
	return;
}

static void s5k5ccgx_exif_iso()
{
	unsigned short rough_iso, index;
	unsigned int gain = 0;
	unsigned short iso_gain_table[] = {10, 15, 25, 45};
	unsigned short iso_table[] = {0, 50, 100, 200, 400};
	rough_iso = index = 0;

	S5K5CCGX_WRT_LIST(s5k5ccgx_iso_spd);
	s5k5ccgx_i2c_read(0x0F12, &rough_iso);
	gain = rough_iso * 10 / 256;
	for (index = 0; index < 4; index++) {
		if (gain < iso_gain_table[index])
			break;
	}
	CAM_DEBUG("iso=%d,read=%d,i= %d\n", iso_table[index], rough_iso, index);
	rough_iso = iso_table[index];/*ISO*/
	s5k5ccgx_exif->iso = rough_iso;
	return;
}

void s5k5ccgx_set_preview_size(int32_t index)
{
	CAM_DEBUG("index %d", index);

	switch (index) {
	case PREVIEW_SIZE_WVGA:	/*For 3rd party app*/
		CAM_DEBUG("800*480");
		S5K5CCGX_WRT_LIST(s5k5ccgx_preview_wvga);
		msleep(100);
		break;
	case PREVIEW_SIZE_D1:
		CAM_DEBUG("720*480");
		S5K5CCGX_WRT_LIST(s5k5ccgx_720_480_Preview);
		msleep(100);
		break;

	case PREVIEW_SIZE_1024x552:
		CAM_DEBUG("1632*880");
		S5K5CCGX_WRT_LIST(s5k5ccgx_preview_wvga);
		msleep(100);
		break;

#if defined(CONFIG_MACH_ESPRESSO10_ATT) || defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_SPR) || defined(CONFIG_MACH_LT02_ATT) \
				|| defined(CONFIG_MACH_LT02_SPR) || defined(CONFIG_MACH_LT02_TMO)
	case PREVIEW_SIZE_1024x576:
		CAM_DEBUG("1024*576");
		S5K5CCGX_WRT_LIST(s5k5ccgx_preview_size_1024_576);
		msleep(100);
		break;
#endif
	case PREVIEW_SIZE_HD: //Added for scenarios where 720P recording is required without setting recording hint
		CAM_DEBUG("720P Preview");
		S5K5CCGX_WRT_LIST(s5k5ccgx_recording_HD);
		msleep(100);
		break;

	default:
		CAM_DEBUG("640*480");
		S5K5CCGX_WRT_LIST(s5k5ccgx_Preview_vga);
		break;
	}

	S5K5CCGX_WRT_LIST(s5k5ccgx_update_preview_setting);

	s5k5ccgx_ctrl->settings.preview_size_idx = index;
}


void s5k5ccgx_set_preview(void)
{
	CAM_DEBUG("cam_mode = %d", s5k5ccgx_ctrl->cam_mode);
	mode_enable = true;

	if (s5k5ccgx_ctrl->cam_mode == MOVIE_MODE) {
#if defined(CONFIG_MACH_ESPRESSO_VZW)
		cam_mode = 1;
#endif

		CAM_DEBUG("Camcorder_Mode_ON");
		if (s5k5ccgx_ctrl->settings.preview_size_idx ==
				PREVIEW_SIZE_HD) {
			CAM_DEBUG("720P recording");
		S5K5CCGX_WRT_LIST(s5k5ccgx_recording_HD);
	} else {
			CAM_DEBUG("VGA recording");
			S5K5CCGX_WRT_LIST(s5k5ccgx_recording_VGA);
			if (s5k5ccgx_ctrl->settings.fps == 15) {
				CAM_DEBUG("MMS recording");
				S5K5CCGX_WRT_LIST(s5k5ccgx_FPS_15);
				}

		}

		if (s5k5ccgx_ctrl->settings.preview_size_idx ==
				PREVIEW_SIZE_D1) {
			CAM_DEBUG("D1 recording");
		S5K5CCGX_WRT_LIST(s5k5ccgx_recording_D1);
			}

		s5k5ccgx_set_whitebalance\
			(s5k5ccgx_ctrl->settings.wb);
		s5k5ccgx_set_effect\
			(s5k5ccgx_ctrl->settings.effect);
		s5k5ccgx_set_ev\
			(s5k5ccgx_ctrl->settings.brightness);
	} else {
		CAM_DEBUG("Preview_Mode");
		if (s5k5ccgx_ctrl->op_mode == CAMERA_MODE_INIT) {
			S5K5CCGX_WRT_LIST(s5k5ccgx_common);
			s5k5ccgx_set_preview_size\
				(s5k5ccgx_ctrl->settings.preview_size_idx);
			if (s5k5ccgx_ctrl->settings.scene ==
				CAMERA_SCENE_AUTO) {
				s5k5ccgx_set_whitebalance\
					(s5k5ccgx_ctrl->settings.wb);
				s5k5ccgx_set_effect\
					(s5k5ccgx_ctrl->settings.effect);
				s5k5ccgx_set_ev\
					(s5k5ccgx_ctrl->settings.brightness);
				s5k5ccgx_set_af_mode\
					(s5k5ccgx_ctrl->settings.focus_mode);
				s5k5ccgx_set_metering\
					(s5k5ccgx_ctrl->settings.metering);
			} else {
				s5k5ccgx_set_scene_mode\
					(s5k5ccgx_ctrl->settings.scene);
				if (s5k5ccgx_ctrl->settings.scene
					== CAMERA_SCENE_NIGHT) {
					msleep(500);
					CAM_DEBUG("500ms (NIGHTSHOT)");
				} else if (s5k5ccgx_ctrl->settings.scene
					== CAMERA_SCENE_FIRE) {
					msleep(800);
					CAM_DEBUG("800ms (FIREWORK)");
				} else {
					msleep(50);
				}
			}
		} else {
		CAM_DEBUG("Return_preview_Mode");
		s5k5ccgx_set_preview_size\
				(s5k5ccgx_ctrl->settings.preview_size_idx);
		if (s5k5ccgx_ctrl->settings.scene == CAMERA_SCENE_NIGHT)
			msleep(500);
		else if (s5k5ccgx_ctrl->settings.scene == CAMERA_SCENE_FIRE)
			msleep(1200);
		else
			msleep(120);
		}
		s5k5ccgx_ctrl->op_mode = CAMERA_MODE_PREVIEW;
	}
}

void s5k5ccgx_set_capture(void)
{
	int cur_lux;

	CAM_DEBUG("");

	s5k5ccgx_ctrl->op_mode = CAMERA_MODE_CAPTURE;

	cur_lux = s5k5ccgx_get_light_level();
	CAM_DEBUG("CAPTURE light level = %d", cur_lux);

	/** Capture Sequence **/
	if (cur_lux > 0xFFFE) {
		CAM_DEBUG("HighLight Snapshot!");
		S5K5CCGX_WRT_LIST(s5k5ccgx_highlight_snapshot);
		if (af_low_lux)
			CAM_DEBUG("additional delay for Low Lux AF");
	} else if (cur_lux < LOW_LIGHT_LEVEL) {
		if ((s5k5ccgx_ctrl->settings.scene == CAMERA_SCENE_NIGHT)
			|| (s5k5ccgx_ctrl->settings.scene ==\
			CAMERA_SCENE_FIRE)) {
			CAM_DEBUG("Night or Firework  Snapshot!");
			S5K5CCGX_WRT_LIST(s5k5ccgx_night_snapshot);
		} else {
			CAM_DEBUG("LowLight Snapshot delay!");
			S5K5CCGX_WRT_LIST(s5k5ccgx_lowlight_snapshot);
		}
	} else {
		CAM_DEBUG("Normal Snapshot !\n");
		S5K5CCGX_WRT_LIST(s5k5ccgx_snapshot);
		if (af_low_lux)
			CAM_DEBUG("additional delay for Low Lux AF");
	}

	/*Exif data*/
	s5k5ccgx_exif_shutter_speed();
	s5k5ccgx_exif_iso();
	mode_enable = false;
}

static int32_t s5k5ccgx_sensor_setting(int update_type, int rt)
{


	int32_t rc = 0;
	//struct msm_camera_csid_params s5k5ccgx_csid_params;
	//struct msm_camera_csiphy_params s5k5ccgx_csiphy_params;
	#if 0
	struct msm_camera_csid_vc_cfg
							s5k5ccgx_vccfg[] = {
					{0, 0x1E, CSI_DECODE_8BIT},
					/* {0, CSI_RAW10, CSI_DECODE_10BIT}, */
					{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
					};
	#endif
	CAM_DEBUG("Start");
	switch (update_type) {
	case REG_INIT:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE)
			/* Add some condition statements */
		break;
#if 0
	case UPDATE_PERIODIC:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			CAM_DEBUG("UPDATE_PERIODIC");

			v4l2_subdev_notify(s5k5ccgx_ctrl->sensor_dev,
				NOTIFY_ISPIF_STREAM, (void *)ISPIF_STREAM(
				PIX_0, ISPIF_OFF_IMMEDIATELY));

			/* stop streaming */
			/*S5K5CCGX_WRT_LIST(s5k5ccgx_stream_stop);*/
			mdelay(30);

/*			if (config_csi2 == 0) { */
				
			s5k5ccgx_csid_params.lane_cnt = 1;
			s5k5ccgx_csid_params.lane_assign = 0xe4;
			s5k5ccgx_csid_params.lut_params.num_cid =
				ARRAY_SIZE(s5k5ccgx_vccfg);
			s5k5ccgx_csid_params.lut_params.vc_cfg =
				&s5k5ccgx_vccfg[0];
			s5k5ccgx_csiphy_params.lane_cnt = 1;
			//if (system_rev <= 1)
				s5k5ccgx_csiphy_params.settle_cnt = 0x07;
			/*else
				s5k5ccgx_csiphy_params.settle_cnt = 0x1B;*/
			v4l2_subdev_notify(s5k5ccgx_ctrl->sensor_dev,
					NOTIFY_CSID_CFG, &s5k5ccgx_csid_params);
			v4l2_subdev_notify(s5k5ccgx_ctrl->sensor_dev,
					NOTIFY_CID_CHANGE, NULL);
			mb();
			v4l2_subdev_notify(s5k5ccgx_ctrl->sensor_dev,
					NOTIFY_CSIPHY_CFG,
					&s5k5ccgx_csiphy_params);
			mb();
				/*s5k5ccgx_delay_msecs_stdby*/
			mdelay(20);
			config_csi2 = 1;
/*			}*/
			if (rc < 0)
				return rc;

			v4l2_subdev_notify(s5k5ccgx_ctrl->sensor_dev,
				NOTIFY_ISPIF_STREAM, (void *)ISPIF_STREAM(
				PIX_0, ISPIF_ON_FRAME_BOUNDARY));

			/*start stream*/
			if (s5k5ccgx_ctrl->cam_mode != MOVIE_MODE) {
				S5K5CCGX_WRT_LIST(s5k5ccgx_preview);
				msleep(40);
			}
		}
		break;
#else
	case UPDATE_PERIODIC:
	msm_sensor_enable_debugfs(s5k5ccgx_ctrl->s_ctrl);
	if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			CAM_DEBUG("UPDATE_PERIODIC");
	msleep(20);

	CAM_DEBUG(" start AP MIPI setting");
	if(s5k5ccgx_ctrl->s_ctrl->sensordata->
		sensor_platform_info->
		csi_lane_params != NULL) {
		CAM_DEBUG(" lane_assign ="\
			" 0x%x",
			s5k5ccgx_ctrl->s_ctrl->
			sensordata->
			sensor_platform_info->
			csi_lane_params->
			csi_lane_assign);

		CAM_DEBUG(" lane_mask ="\
			" 0x%x",
			s5k5ccgx_ctrl->s_ctrl->
			sensordata->
			sensor_platform_info->
			csi_lane_params->
			csi_lane_mask);
		}

		mb();
	
		mb();
		config_csi2 = 1;
		msleep(20);
		
	}
	break;
#endif
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int32_t s5k5ccgx_video_config(int mode)
{
	int32_t	rc = 0;
	CAM_DEBUG(" E ");
	if (s5k5ccgx_sensor_setting(UPDATE_PERIODIC, RES_PREVIEW) < 0)
		rc = -1;

	return rc;
}

static long s5k5ccgx_set_sensor_mode(int mode)
{
	CAM_DEBUG("%d", mode);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
	case SENSOR_VIDEO_MODE:
		s5k5ccgx_set_preview();
		if (config_csi2 == 0)
			s5k5ccgx_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		s5k5ccgx_set_capture();
		s5k5ccgx_reset_AF_region();
		break;
	default:
		return 0;
	}
	return 0;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static int s5k5ccgx_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	cam_info(" Nothing");

	return rc;
}

#if defined(CONFIG_S5K5CCGX) && defined(CONFIG_DB8131M) /* jasper */
static int s5k5ccgx_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	CAM_DEBUG("=== Start ===");

	int rc = 0;
	int temp = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

#ifdef CONFIG_LOAD_FILE
	s5k5ccgx_regs_table_init();
#endif
	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		pr_err("%s: request gpio failed\n", __func__);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("check VT reset : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("CAM_3M_RST : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("CAM_3M_ISP_INIT : %d", temp);

	/*Power on the LDOs */
	data->sensor_platform_info->sensor_power_on(0);

	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 1,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0)
		pr_err("%s: clk enable failed\n", __func__);

	usleep(50);


	/*standby Main cam */
	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 1);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("[s5k5ccgx] CAM_3M_ISP_INIT : %d", temp);
	usleep(4*1000);

	/*reset Main cam */
	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 1);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("[s5k5ccgx] CAM_3M_RST : %d", temp);
	usleep(10 * 1000);

	/* sensor validation test */
	CAM_DEBUG(" Camera Sensor Validation Test");
	rc = S5K5CCGX_WRT_LIST(s5k5ccgx_pre_common);
	if (rc < 0) {
		pr_info("Error in  Camera Sensor Validation Test");
		return rc;
	}

	s5k5ccgx_ctrl->settings.iso = CAMERA_ISO_MODE_AUTO;
	s5k5ccgx_ctrl->cam_mode = PREVIEW_MODE;
	s5k5ccgx_ctrl->settings.effect = CAMERA_EFFECT_OFF;
	s5k5ccgx_ctrl->settings.wb = CAMERA_WHITE_BALANCE_AUTO;
	s5k5ccgx_ctrl->settings.brightness = CAMERA_EV_DEFAULT;

	config_csi2 = 0;
	mode_enable = false;
	s5k5ccgx_ctrl->op_mode = CAMERA_MODE_INIT;

	return rc;
}
#elif defined(CONFIG_S5K5CCGX) && defined(CONFIG_SR130PC20) /* LT02 */
static int s5k5ccgx_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{


	int rc = 0;
	int temp = 0;
	unsigned short i2c_test_read=0;
	//int status = 0;
	//int count = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CAM_DEBUG("=== Start ===");
#ifdef CONFIG_LOAD_FILE
	s5k5ccgx_regs_table_init();
#endif

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		pr_err("%s: request gpio failed\n", __func__);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("check VT reset : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("CAM_3M_RST : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("CAM_3M_ISP_INIT : %d", temp);

	/*Power on the LDOs */
	data->sensor_platform_info->sensor_power_on(0);
	usleep(20);

	/*standy VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);
	usleep(4 * 1000); /*msleep(4);*/

	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 1,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
		GPIO_CFG_ENABLE);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0)
		pr_err("%s: clk enable failed\n", __func__);

	//usleep(2 * 1000); /*msleep(2);*/
	usleep(10500);

	/*reset VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("check VT reset : %d", temp);
	usleep(1000); /*msleep(1);*/

	/*standy VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);
	//usleep(10);
	usleep(10500);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 1);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("CAM_3M_ISP_INIT : %d", temp);
	usleep(15);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 1);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("CAM_3M_RST : %d", temp);
	usleep(50 * 1000);

	/* sensor validation test */
	CAM_DEBUG(" Camera Sensor Validation Test");
	rc = S5K5CCGX_WRT_LIST(s5k5ccgx_pre_common);
	if (rc < 0) {
		pr_info("Error in Camera Sensor Validation Test");
		return rc;
	}
	rc = s5k5ccgx_i2c_read(0x0F12, &i2c_test_read);
	if (rc < 0) {
		pr_info("Error in i2c test read");
		return rc;
	}

	config_csi2 = 0;
	s5k5ccgx_ctrl->op_mode = CAMERA_MODE_INIT;

	return rc;
}
#else
static int s5k5ccgx_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	printk(KERN_ERR "s5k5ccgx_sensor_power_up");
}
#endif

static void s5k5ccgx_check_dataline(int val)
{
	if (val) {
		CAM_DEBUG("DTP ON");
		s5k5ccgx_ctrl->dtpTest = 1;

	} else {
		CAM_DEBUG("DTP OFF");
		s5k5ccgx_ctrl->dtpTest = 0;
	}
}

static int s5k5ccgx_set_ae_awb(int lock)
{
	if (s5k5ccgx_ctrl->touchaf_enable == 1)
		return 0;

	if (lock) {
		if (s5k5ccgx_ctrl->settings.ae_awb_lock == 0) {
			CAM_DEBUG("AWB_AE_LOCK");
			if (s5k5ccgx_ctrl->settings.wb ==
				CAMERA_WHITE_BALANCE_AUTO) {
				S5K5CCGX_WRT_LIST(s5k5ccgx_ae_lock);
				S5K5CCGX_WRT_LIST(s5k5ccgx_awb_lock);
			} else {
				S5K5CCGX_WRT_LIST(s5k5ccgx_ae_lock);
			}
			s5k5ccgx_ctrl->settings.ae_awb_lock = 1;
		}
	} else {
		if (s5k5ccgx_ctrl->settings.ae_awb_lock == 1) {
			CAM_DEBUG("AWB_AE_UNLOCK");
			if (s5k5ccgx_ctrl->settings.wb ==
				CAMERA_WHITE_BALANCE_AUTO) {
				S5K5CCGX_WRT_LIST(s5k5ccgx_ae_unlock);
				S5K5CCGX_WRT_LIST(s5k5ccgx_awb_unlock);
			} else {
				S5K5CCGX_WRT_LIST(s5k5ccgx_ae_unlock);
			}
			s5k5ccgx_ctrl->settings.ae_awb_lock = 0;
		}
	}

	return 0;
}


static int s5k5ccgx_set_af_mode(int mode)
{
	CAM_DEBUG(" %d", mode);

	if (!mode_enable)
		goto focus_end;

#if defined(CONFIG_MACH_ESPRESSO_VZW) || defined(CONFIG_MACH_ESPRESSO_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO10_SPR) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR) \
				|| defined(CONFIG_MACH_LT02_ATT) \
				|| defined(CONFIG_MACH_LT02_SPR) \
				|| defined(CONFIG_MACH_LT02_TMO)

	return 0;
#endif

	switch (mode) {
	case CAMERA_AF_AUTO:
		CAM_DEBUG("S5K5CCGX_AF_SET_NORMAL");
		S5K5CCGX_WRT_LIST(s5k5ccgx_af_normal_on);
		break;

	case CAMERA_AF_MACRO:
		CAM_DEBUG("S5K5CCGX_AF_SET_MACRO");
		S5K5CCGX_WRT_LIST(s5k5ccgx_af_macro_on);
		break;

	default:
		CAM_DEBUG("default mode is auto");
		S5K5CCGX_WRT_LIST(s5k5ccgx_af_normal_on);
	}

focus_end:
	s5k5ccgx_ctrl->settings.focus_mode = mode;
	return 0;
}

int s5k5ccgx_set_af_status(int status, int initial_pos)
{
	int rc = 0;
	unsigned int cur_lux = 0;

	if (status) {
		CAM_DEBUG("S5K5CCGX_AF_START");

		cur_lux = s5k5ccgx_get_light_level();
		CAM_DEBUG("AF light level = %d", cur_lux);
		if (cur_lux <= LOW_LIGHT_LEVEL) {
			CAM_DEBUG("LOW LUX AF ");
			af_low_lux = 1;
		} else {
			CAM_DEBUG("NORMAL LUX AF ");
			af_low_lux = 0;
		}

		/*AE/AWB lock*/
		s5k5ccgx_set_ae_awb(1);

		S5K5CCGX_WRT_LIST(s5k5ccgx_af_do);
		if (af_low_lux) {
			CAM_DEBUG("200ms delay for Low Lux AF");
			if (s5k5ccgx_ctrl->settings.scene == CAMERA_SCENE_NIGHT
			|| s5k5ccgx_ctrl->settings.scene == CAMERA_SCENE_FIRE)
				msleep(500);
			else
			msleep(200);
		}

	} else {
		CAM_DEBUG("S5K5CCGX_AF_ABORT\n");

		if (initial_pos == 2) {
			S5K5CCGX_WRT_LIST(s5k5ccgx_af_abort);
		} else if (initial_pos == 1) {
			s5k5ccgx_set_af_mode\
				(s5k5ccgx_ctrl->settings.focus_mode);
		} else {
			S5K5CCGX_WRT_LIST(s5k5ccgx_af_abort);
			s5k5ccgx_set_af_mode\
				(s5k5ccgx_ctrl->settings.focus_mode);
		}

		/*AE/AWB unlock*/
		s5k5ccgx_set_ae_awb(0);

		af_low_lux = 0;

		s5k5ccgx_ctrl->touchaf_enable = false;
	}

	s5k5ccgx_ctrl->settings.focus_status = status;

	return rc;
}

int s5k5ccgx_reset_AF_region(void)
{
	u16 mapped_x = 320;
	u16 mapped_y = 240;
	u16 inner_window_start_x = 0;
	u16 inner_window_start_y = 0;
	u16 outer_window_start_x = 0;
	u16 outer_window_start_y = 0;

	s5k5ccgx_ctrl->touchaf_enable = false;

	mapped_x = (mapped_x * 640) / 666;
	mapped_y = (mapped_y * 480) / 500;

	inner_window_start_x    = mapped_x - (INNER_WINDOW_WIDTH_640_480 / 2);
	outer_window_start_x    = mapped_x - (OUTER_WINDOW_WIDTH_640_480 / 2);

	inner_window_start_y    = mapped_y - (INNER_WINDOW_HEIGHT_640_480 / 2);
	outer_window_start_y    = mapped_y - (OUTER_WINDOW_HEIGHT_640_480 / 2);

	/*calculate the start position value*/
	inner_window_start_x = inner_window_start_x * 640 / 640;
	outer_window_start_x = outer_window_start_x * 640 / 640;
	inner_window_start_y = inner_window_start_y * 640 / 480;
	outer_window_start_y = outer_window_start_y * 640 / 480;

	/*Write register*/
	s5k5ccgx_i2c_write_multi(0x0028, 0x7000);

	/* inner_window_start_x */
	s5k5ccgx_i2c_write_multi(0x002A, 0x0234);
	s5k5ccgx_i2c_write_multi(0x0F12, inner_window_start_x);

	/* outer_window_start_x*/
	s5k5ccgx_i2c_write_multi(0x002A, 0x022C);
	s5k5ccgx_i2c_write_multi(0x0F12, outer_window_start_x);

	/* inner_window_start_y */
	s5k5ccgx_i2c_write_multi(0x002A, 0x0236);
	s5k5ccgx_i2c_write_multi(0x0F12, inner_window_start_y);

	/* outer_window_start_y */
	s5k5ccgx_i2c_write_multi(0x002A, 0x022E);
	s5k5ccgx_i2c_write_multi(0x0F12, outer_window_start_y);

	/* Update AF window*/
	s5k5ccgx_i2c_write_multi(0x002A, 0x023C);
	s5k5ccgx_i2c_write_multi(0x0F12, 0x0001);

	return 0;
}

int s5k5ccgx_set_touchaf_pos(int x, int y)
{
	u16 mapped_x = 0;
	u16 mapped_y = 0;
	u16 inner_window_start_x = 0;
	u16 inner_window_start_y = 0;
	u16 outer_window_start_x = 0;
	u16 outer_window_start_y = 0;

	u16 sensor_width = 0;
	u16 sensor_height = 0;
	u16 inner_window_width = 0;
	u16 inner_window_height = 0;
	u16 outer_window_width = 0;
	u16 outer_window_height = 0;

	u16 touch_width = 0;
	u16 touch_height = 0;

	sensor_width        = 640;
	sensor_height       = 480;
	inner_window_width  = INNER_WINDOW_WIDTH_640_480;
	inner_window_height = INNER_WINDOW_HEIGHT_640_480;
	outer_window_width  = OUTER_WINDOW_WIDTH_640_480;
	outer_window_height = OUTER_WINDOW_HEIGHT_640_480;
	touch_width         = 640;
	touch_height        = 480;

	s5k5ccgx_ctrl->touchaf_enable = true;
	CAM_DEBUG("xPos = %d, yPos = %d", x, y);


	/* mapping the touch position on the sensor display*/
	mapped_x = (x * sensor_width) / touch_width;
	mapped_y = (y * sensor_height) / touch_height;
	CAM_DEBUG("mapped xPos = %d, mapped yPos = %d", mapped_x, mapped_y);

	/* set X axis*/
	if (mapped_x <= (inner_window_width / 2)) {
		inner_window_start_x = 0;
		outer_window_start_x = 0;
	} else if (mapped_x <= (outer_window_width / 2)) {
		inner_window_start_x = mapped_x - (inner_window_width / 2);
		outer_window_start_x = 0;
	} else if (mapped_x >=
	((sensor_width - 1) - (inner_window_width / 2))) {
		inner_window_start_x = (sensor_width - 1) - inner_window_width;
		outer_window_start_x = (sensor_width - 1) - outer_window_width;
	} else if (mapped_x >=
	((sensor_width - 1) - (outer_window_width / 2))) {
		inner_window_start_x = mapped_x - (inner_window_width / 2);
		outer_window_start_x = (sensor_width - 1) - outer_window_width;
	} else {
		inner_window_start_x = mapped_x - (inner_window_width / 2);
		outer_window_start_x = mapped_x - (outer_window_width / 2);
	}

	/* set Y axis */
	if (mapped_y <= (inner_window_height / 2)) {
		inner_window_start_y = 0;
		outer_window_start_y = 0;
	} else if (mapped_y <= (outer_window_height / 2)) {
		inner_window_start_y = mapped_y - (inner_window_height / 2);
		outer_window_start_y = 0;
	} else if (mapped_y >=
	((sensor_height - 1) - (inner_window_height / 2))) {
		inner_window_start_y =
			(sensor_height - 1) - inner_window_height;
		outer_window_start_y =
			(sensor_height - 1) - outer_window_height;
	} else if (mapped_y >=
	((sensor_height - 1) - (outer_window_height / 2))) {
		inner_window_start_y = mapped_y - (inner_window_height / 2);
		outer_window_start_y =
			(sensor_height - 1) - outer_window_height;
	} else {
		inner_window_start_y = mapped_y - (inner_window_height / 2);
		outer_window_start_y = mapped_y - (outer_window_height / 2);
	}

	/*calculate the start position value*/
	inner_window_start_x = inner_window_start_x * 640 / sensor_width;
	outer_window_start_x = outer_window_start_x * 640 / sensor_width;
	inner_window_start_y = inner_window_start_y * 640 / sensor_height;
	outer_window_start_y = outer_window_start_y * 640 / sensor_height;
	CAM_DEBUG("calculated inner_window_start_x = %d", inner_window_start_x);
	CAM_DEBUG("calculated inner_window_start_y = %d", inner_window_start_y);
	CAM_DEBUG("calculated outer_window_start_x = %d", outer_window_start_x);
	CAM_DEBUG("calculated outer_window_start_y = %d", outer_window_start_y);


	/*Write register*/
	s5k5ccgx_i2c_write_multi(0x0028, 0x7000);

	/* inner_window_start_x*/
	s5k5ccgx_i2c_write_multi(0x002A, 0x0234);
	s5k5ccgx_i2c_write_multi(0x0F12, inner_window_start_x);

	/* outer_window_start_x*/
	s5k5ccgx_i2c_write_multi(0x002A, 0x022C);
	s5k5ccgx_i2c_write_multi(0x0F12, outer_window_start_x);

	/* inner_window_start_y*/
	s5k5ccgx_i2c_write_multi(0x002A, 0x0236);
	s5k5ccgx_i2c_write_multi(0x0F12, inner_window_start_y);

	/* outer_window_start_y*/
	s5k5ccgx_i2c_write_multi(0x002A, 0x022E);
	s5k5ccgx_i2c_write_multi(0x0F12, outer_window_start_y);

	/* Update AF window*/
	s5k5ccgx_i2c_write_multi(0x002A, 0x023C);
	s5k5ccgx_i2c_write_multi(0x0F12, 0x0001);

	CAM_DEBUG("update AF window and sleep 100ms");
	msleep(100);

	return 0;
}


int s5k5ccgx_get_af_status(int is_search_status)
{
	unsigned short af_status = 0;

	switch (is_search_status) {
	case 0:
		s5k5ccgx_i2c_write_multi(0x002C, 0x7000);
		s5k5ccgx_i2c_write_multi(0x002E, 0x2D12);
		s5k5ccgx_i2c_read(0x0F12, &af_status);
		CAM_DEBUG("1st AF status : %x", af_status);
		break;

	case 1:
		s5k5ccgx_i2c_write_multi(0x002C, 0x7000);
		s5k5ccgx_i2c_write_multi(0x002E, 0x1F2F);
		s5k5ccgx_i2c_read(0x0F12, &af_status);
		CAM_DEBUG("2nd AF status : %x", af_status);
		break;
	default:
		CAM_DEBUG("unexpected mode is comming from hal\n");
		break;
	}

	if (s5k5ccgx_ctrl->settings.scene == CAMERA_SCENE_NIGHT)
		msleep(20);

	return  af_status;
}

static int s5k5ccgx_set_effect(int effect)
{
	CAM_DEBUG("[s5k5ccgx] %s : %d", __func__, effect);
	if (!mode_enable)
		goto effect_end;

	switch (effect) {
	case CAMERA_EFFECT_OFF:
		S5K5CCGX_WRT_LIST(s5k5ccgx_effect_none);
		break;

	case CAMERA_EFFECT_MONO:
		S5K5CCGX_WRT_LIST(s5k5ccgx_effect_mono);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		S5K5CCGX_WRT_LIST(s5k5ccgx_effect_negative);
		break;

	case CAMERA_EFFECT_SEPIA:
		S5K5CCGX_WRT_LIST(s5k5ccgx_effect_sepia);
		break;

	default:
		CAM_DEBUG("[s5k5ccgx] default effect");
		S5K5CCGX_WRT_LIST(s5k5ccgx_effect_none);
		return 0;
	}

effect_end:
	s5k5ccgx_ctrl->settings.effect = effect;

	return 0;
}

static int s5k5ccgx_set_whitebalance(int wb)
{
	if (!mode_enable)
		goto whitebalance_end;

	switch (wb) {
	case CAMERA_WHITE_BALANCE_AUTO:
			S5K5CCGX_WRT_LIST(s5k5ccgx_wb_auto);
		break;

	case CAMERA_WHITE_BALANCE_INCANDESCENT:
			S5K5CCGX_WRT_LIST(s5k5ccgx_wb_incandescent);
		break;

	case CAMERA_WHITE_BALANCE_FLUORESCENT:
			S5K5CCGX_WRT_LIST(s5k5ccgx_wb_fluorescent);
		break;

	case CAMERA_WHITE_BALANCE_DAYLIGHT:
			S5K5CCGX_WRT_LIST(s5k5ccgx_wb_daylight);
		break;

	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
			S5K5CCGX_WRT_LIST(s5k5ccgx_wb_cloudy);
		break;

	default:
		CAM_DEBUG("[s5k5ccgx] unexpected WB mode %s/%d",
			__func__, __LINE__);
		return 0;
	}

whitebalance_end:
	s5k5ccgx_ctrl->settings.wb = wb;

	return 0;
}

static void s5k5ccgx_set_ev(int ev)
{
	CAM_DEBUG("[s5k5ccgx] %s : %d", __func__, ev);
	if (!mode_enable)
		goto exposure_end;

	switch (ev) {
	case CAMERA_EV_M4:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_M4);
		break;

	case CAMERA_EV_M3:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_M3);
		break;

	case CAMERA_EV_M2:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_M2);
		break;

	case CAMERA_EV_M1:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_M1);
		break;

	case CAMERA_EV_DEFAULT:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_default);
		break;

	case CAMERA_EV_P1:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_P1);
		break;

	case CAMERA_EV_P2:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_P2);
		break;

	case CAMERA_EV_P3:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_P3);
		break;

	case CAMERA_EV_P4:
		S5K5CCGX_WRT_LIST(s5k5ccgx_brightness_P4);
		break;

	default:
		CAM_DEBUG("[s5k5ccgx] unexpected ev mode %s/%d",
			__func__, __LINE__);
		break;
	}

exposure_end:
	s5k5ccgx_ctrl->settings.brightness = ev;
}

static void s5k5ccgx_set_scene_mode(int mode)
{
	CAM_DEBUG("mode = %d", mode);

	S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Default);

	switch (mode) {
	case CAMERA_SCENE_AUTO:
		break;

	case CAMERA_SCENE_LANDSCAPE:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Landscape);
		break;

	case CAMERA_SCENE_DAWN:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Dawn);
		break;

	case CAMERA_SCENE_BEACH:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Beach_Snow);
		break;

	case CAMERA_SCENE_SUNSET:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Sunset);
		break;

	case CAMERA_SCENE_NIGHT:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Nightmode);
		break;

	case CAMERA_SCENE_PORTRAIT:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Portrait);
		break;

	case CAMERA_SCENE_AGAINST_LIGHT:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Backlight);
		break;

	case CAMERA_SCENE_SPORT:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Sports);
		break;

	case CAMERA_SCENE_FALL:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Fall_Color);
		break;

	case CAMERA_SCENE_TEXT:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Document);
		break;

	case CAMERA_SCENE_CANDLE:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Candle_Light);
		break;

	case CAMERA_SCENE_FIRE:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Fireworks);
		break;

	case CAMERA_SCENE_PARTY:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Scene_Party_Indoor);
		break;

	default:
		CAM_DEBUG("[s5k5ccgx] unexpected scene mode %s/%d\n",
			  __func__, __LINE__);
		break;
	}

	s5k5ccgx_ctrl->settings.scene = mode;

}
#if 0
static void s5k5ccgx_set_iso(int iso)
{
	CAM_DEBUG("[s5k5ccgx] %s : %d\n", __func__, iso);

	switch (iso) {
	case CAMERA_ISO_MODE_AUTO:
		S5K5CCGX_WRT_LIST(s5k5ccgx_ISO_Auto);
		break;

	case CAMERA_ISO_MODE_100:
		S5K5CCGX_WRT_LIST(s5k5ccgx_ISO_100);
		break;

	case CAMERA_ISO_MODE_200:
		S5K5CCGX_WRT_LIST(s5k5ccgx_ISO_200);
		break;

	case CAMERA_ISO_MODE_400:
		S5K5CCGX_WRT_LIST(s5k5ccgx_ISO_400);
		break;

	default:
		CAM_DEBUG("[s5k5ccgx] unexpected iso mode %s/%d\n",
			  __func__, __LINE__);
		break;
	}

	s5k5ccgx_ctrl->settings.iso = iso;
}
#endif
static void s5k5ccgx_set_metering(int mode)
{
	CAM_DEBUG("[s5k5ccgx] %s : %d\n", __func__, mode);

	if (!mode_enable)
		goto metering_end;

	switch (mode) {
	case CAMERA_CENTER_WEIGHT:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Metering_Center);
		break;

	case CAMERA_AVERAGE:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Metering_Matrix);
		break;

	case CAMERA_SPOT:
		S5K5CCGX_WRT_LIST(s5k5ccgx_Metering_Spot);
		break;

	default:
		CAM_DEBUG("[s5k5ccgx] unexpected metering mode %s/%d\n",
			  __func__, __LINE__);
		break;
	}

metering_end:
	s5k5ccgx_ctrl->settings.metering = mode;

}

void sensor_native_control(void __user *arg)
{
	struct ioctl_native_cmd ctrl_info;
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;
	int rc = 0;

	/*if (copy_from_user((void *)&ctrl_info,
		(const void *)arg, sizeof(ctrl_info)))
		CAM_DEBUG(
			"[s5k5ccgx] %s fail copy_from_user!", __func__);*/
	if (copy_from_user(&ctrl_info,
		(void __user *)ioctl_ptr->ioctl_ptr,
		sizeof(ctrl_info))) {
		cam_err("fail copy_from_user!");
		goto FAIL_END;
	}

	switch (ctrl_info.mode) {
	case EXT_CAM_EV:
		s5k5ccgx_set_ev(ctrl_info.value_1);
		break;

	case EXT_CAM_EFFECT:
		s5k5ccgx_set_effect(ctrl_info.value_1);
		break;

	case EXT_CAM_SCENE_MODE:
		s5k5ccgx_set_scene_mode(ctrl_info.value_1);
		s5k5ccgx_ctrl->settings.scenemode = ctrl_info.value_1;
		/*s5k5ccgx_ctrl->flash_mode = ctrl_info.value_2;*/
		break;

	case EXT_CAM_ISO:
		/* s5k5ccgx_set_iso(ctrl_info.value_1); */
		break;

	case EXT_CAM_METERING:
		s5k5ccgx_set_metering(ctrl_info.value_1);
		break;

	case EXT_CAM_WB:
		s5k5ccgx_set_whitebalance(ctrl_info.value_1);
		break;

	case EXT_CAM_SET_AE_AWB:
		rc = s5k5ccgx_set_ae_awb(ctrl_info.value_1);
		break;

	case EXT_CAM_FOCUS:
		s5k5ccgx_set_af_mode(ctrl_info.value_1);
		break;

	case EXT_CAM_SET_AF_STATUS:
		rc = s5k5ccgx_set_af_status(ctrl_info.value_1,
			ctrl_info.value_2);
		break;

	case EXT_CAM_GET_AF_STATUS:
		rc = s5k5ccgx_get_af_status(ctrl_info.value_1);
			ctrl_info.value_1 = rc;
		break;

	case EXT_CAM_SET_TOUCHAF_POS:
	//	rc = s5k5ccgx_set_touchaf_pos(ctrl_info.value_1,
	//		ctrl_info.value_2);
		break;

	case EXT_CAM_DTP_TEST:
		s5k5ccgx_check_dataline(ctrl_info.value_1);
		break;

	case  EXT_CAM_MOVIE_MODE:
		CAM_DEBUG("MOVIE mode : %d", ctrl_info.value_1);
		s5k5ccgx_ctrl->cam_mode = ctrl_info.value_1;
		break;

	case EXT_CAM_PREVIEW_SIZE:
		s5k5ccgx_ctrl->settings.preview_size_idx = ctrl_info.value_1;
		break;

	case EXT_CAM_EXIF:
		ctrl_info.value_1 = s5k5ccgx_get_exif(ctrl_info.address);
		CAM_DEBUG("exif call value: %d\n", ctrl_info.value_1);
		break;

	case EXT_CAM_SET_FPS:
		s5k5ccgx_ctrl->settings.fps = ctrl_info.value_1;
		break;

	default:
		CAM_DEBUG("[s5k5ccgx] default mode");
		break;
	}

	/*if (copy_to_user((void *)arg,
		(const void *)&ctrl_info, sizeof(ctrl_info)))
		CAM_DEBUG("[s5k5ccgx] %s fail copy_to_user!", __func__);*/
	if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
		  (const void *)&ctrl_info,
			sizeof(ctrl_info))) {
		cam_err("fail copy_to_user!");
		goto FAIL_END;
	}
	return ;
FAIL_END:
	cam_err("Error : can't handle native control");
//	return rc;
}

long s5k5ccgx_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	void __user *argp = (void __user *)arg;
	struct msm_sensor_ctrl_t *s5k5ccgx_s_ctrl = get_sctrl(sd);
	CAM_DEBUG("s5k5ccgx_sensor_subdev_ioctl\n");
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return s5k5ccgx_sensor_config(s5k5ccgx_s_ctrl, argp);
	case VIDIOC_MSM_SENSOR_CSID_INFO:
		{
		struct msm_sensor_csi_info *csi_info =
			(struct msm_sensor_csi_info *)arg;
		s5k5ccgx_s_ctrl->is_csic = csi_info->is_csic;
		return 0;
	}	
	default:
		return -ENOIOCTLCMD;
	}
}

int s5k5ccgx_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data, (void *)argp,
						sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	CAM_DEBUG(" cfgtype = %d, mode = %d",
			cfg_data.cfgtype, cfg_data.mode);
		switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			rc = s5k5ccgx_set_sensor_mode(cfg_data.mode);
			break;
		case CFG_SENSOR_INIT:
		if (config_csi2 == 0)
			rc = s5k5ccgx_sensor_setting(UPDATE_PERIODIC,
				RES_PREVIEW);
		break;
		case CFG_START_STREAM:
		if (s_ctrl->func_tbl->sensor_start_stream == NULL) {
			rc = -EFAULT;
			break;
			}
			s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
			break;

		case CFG_STOP_STREAM:
		if (s_ctrl->func_tbl->sensor_stop_stream == NULL) {
			rc = -EFAULT;
			break;
			}
			s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
			break;
		case CFG_GET_CSI_PARAMS:
		CAM_DEBUG("RInside CFG_GET_CSI_PARAMS");
			if (s_ctrl->func_tbl->sensor_get_csi_params == NULL) {
				CAM_DEBUG("cfgtype = %d, mode = %d\n",
			cfg_data.cfgtype, cfg_data.mode);
				rc = -EFAULT;
				break;
			}

			rc = s_ctrl->func_tbl->sensor_get_csi_params(
				s_ctrl,
				&cfg_data.cfg.csi_lane_params);
			CAM_DEBUG("Inside CFG_GET_CSI_PARAMS");
		 if (copy_to_user((void *)argp,
				 (const void *)&cfg_data,
				sizeof(cfg_data)))
				rc = -EFAULT;

			break;
		case CFG_GET_AF_MAX_STEPS:
		default:
			rc = 0;
			cam_err(" Invalid cfgtype = %d", cfg_data.cfgtype);
			break;
		}
	return rc;
}

#if defined(CONFIG_S5K5CCGX) && defined(CONFIG_DB8131M) /* jasper */
static int s5k5ccgx_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int temp = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	CAM_DEBUG("=== Start ===");

	S5K5CCGX_WRT_LIST(s5k5ccgx_preview);
	S5K5CCGX_WRT_LIST(s5k5ccgx_af_off);

#ifdef CONFIG_LOAD_FILE
	s5k5ccgx_regs_table_exit();
#endif

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("CAM_3M_RST : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("CAM_3M_ISP_INIT : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("check VT reset : %d", temp);
	usleep(10); /* 20clk = 0.833us */

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);

	/*CAM_MCLK0*/
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	data->sensor_platform_info->sensor_power_off(0);

	msm_camera_request_gpio_table(data, 0);

	return rc;
}
#elif defined(CONFIG_S5K5CCGX) && defined(CONFIG_SR130PC20) /* LT02 */
static int s5k5ccgx_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int temp = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	CAM_DEBUG("=== Start ===");

#ifdef CONFIG_LOAD_FILE
	s5k5ccgx_regs_table_exit();
#endif

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);


	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("CAM_3M_RST : %d", temp);
	usleep(50);

	/*CAM_MCLK0*/
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("CAM_3M_ISP_INIT : %d", temp);
	usleep(10);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("check VT reset : %d", temp);
	usleep(10); /* 20clk = 0.833us */

	data->sensor_platform_info->sensor_power_off(0);

	msm_camera_request_gpio_table(data, 0);

#if defined(CONFIG_MACH_ESPRESSO_VZW)
	cam_mode = 0;
#endif

	return rc;
}
#else
static int s5k5ccgx_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	printk(KERN_ERR "s5k5ccgx_sensor_power_down");
}
#endif

void s5k5ccgx_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl) {

//Dummy function
	return;
}


void s5k5ccgx_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl) {

//Dummy function
	return;
}

struct v4l2_subdev_info s5k5ccgx_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static int s5k5ccgx_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code) {
	CAM_DEBUG("Index is %d", index);
	if ((unsigned int)index >= ARRAY_SIZE(s5k5ccgx_subdev_info))
		return -EINVAL;

	*code = s5k5ccgx_subdev_info[index].code;
	return 0;
}

static struct v4l2_subdev_core_ops s5k5ccgx_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = s5k5ccgx_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops s5k5ccgx_subdev_video_ops = {
	.enum_mbus_fmt = s5k5ccgx_enum_fmt,
};

static struct v4l2_subdev_ops s5k5ccgx_subdev_ops = {
	.core = &s5k5ccgx_subdev_core_ops,
	.video  = &s5k5ccgx_subdev_video_ops,
};

static int s5k5ccgx_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	cam_err("%s_i2c_probe called", client->name);

	s5k5ccgx_ctrl = kzalloc(sizeof(struct s5k5ccgx_ctrl), GFP_KERNEL);
	if (!s5k5ccgx_ctrl) {
		CAM_DEBUG("s5k5ccgx_ctrl alloc failed!\n");
		rc  = -ENOMEM;
		goto probe_failure;
	}

	s5k5ccgx_exif = kzalloc(sizeof(struct s5k5ccgx_exif_data), GFP_KERNEL);
	if (!s5k5ccgx_exif) {
		CAM_DEBUG("Failed to allocate memory to EXIF structure!\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		cam_err("i2c_check_functionality failed\n");
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		cam_err("s_ctrl->sensor_i2c_client is NULL\n");
		rc = -EFAULT;
		goto probe_failure;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	if (s_ctrl->sensordata == NULL) {
		pr_err("%s: NULL sensor data\n", __func__);
		return -EFAULT;
		goto probe_failure;
	}

	s5k5ccgx_client = client;
	s5k5ccgx_dev = s_ctrl->sensor_i2c_client->client->dev;

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);

	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		&s5k5ccgx_subdev_ops);

	s5k5ccgx_ctrl->s_ctrl = s_ctrl;
	s5k5ccgx_ctrl->sensor_dev = &s_ctrl->sensor_v4l2_subdev;
	s5k5ccgx_ctrl->sensordata = client->dev.platform_data;

	rc = msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	if (rc) {
		CAM_DEBUG("msm_sensor_register failed\n");
		goto probe_failure;
		}

	cam_err("s5k5ccgx_probe succeeded!");
	return 0;

probe_failure:
	cam_err("s5k5ccgx_probe failed!");
	kfree(s5k5ccgx_ctrl);
	kfree(s5k5ccgx_exif);
	return rc;
}

static const struct i2c_device_id s5k5ccgx_i2c_id[] = {
	{"s5k5ccgx", (kernel_ulong_t)&s5k5ccgx_s_ctrl},
	{},
};

static struct msm_camera_i2c_client s5k5ccgx_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static struct i2c_driver s5k5ccgx_i2c_driver = {
	.id_table = s5k5ccgx_i2c_id,
	.probe  = s5k5ccgx_i2c_probe,
	.driver = {
		.name = "s5k5ccgx",
	},
};

static int __init s5k5ccgx_init(void)
{
	return i2c_add_driver(&s5k5ccgx_i2c_driver);
}

static struct msm_sensor_fn_t s5k5ccgx_func_tbl = {
	.sensor_config = s5k5ccgx_sensor_config,
	.sensor_power_up = s5k5ccgx_sensor_power_up,
	.sensor_power_down = s5k5ccgx_sensor_power_down,
	.sensor_match_id = s5k5ccgx_sensor_match_id,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_start_stream = s5k5ccgx_sensor_start_stream,
	.sensor_stop_stream = s5k5ccgx_sensor_stop_stream,
};


static struct msm_sensor_reg_t s5k5ccgx_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_sensor_ctrl_t s5k5ccgx_s_ctrl = {
	.msm_sensor_reg = &s5k5ccgx_regs,
	.sensor_i2c_client = &s5k5ccgx_sensor_i2c_client,
#if defined(CONFIG_MACH_ESPRESSO_VZW) || defined(CONFIG_MACH_ESPRESSO_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO10_SPR) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR) \
				|| defined(CONFIG_MACH_LT02_ATT) \
				|| defined(CONFIG_MACH_LT02_SPR) \
				|| defined(CONFIG_MACH_LT02_TMO)
	.sensor_i2c_addr = 0x2D,
#else
	.sensor_i2c_addr = 0x3C,
#endif
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &s5k5ccgx_mut,
	.sensor_i2c_driver = &s5k5ccgx_i2c_driver,
	.sensor_v4l2_subdev_info = s5k5ccgx_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k5ccgx_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k5ccgx_subdev_ops,
	.func_tbl = &s5k5ccgx_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};


module_init(s5k5ccgx_init);
