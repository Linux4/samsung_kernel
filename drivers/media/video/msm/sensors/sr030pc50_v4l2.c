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
#if defined (CONFIG_MACH_CANE) || defined (CONFIG_MACH_LOGANRE)
#include <mach/msm8930-gpio.h>
#endif

#include <asm/mach-types.h>
#include <mach/vreg.h>
#include <linux/io.h>

#include "msm.h"
#include "sr030pc50.h"

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
#include <linux/string.h>
//static char *sr030pc50_regs_table;
//static int sr030pc50_regs_table_size;
static int sr030pc50_write_regs_from_sd(char *name);
//static int sr030pc50_i2c_write_multi(unsigned short addr, unsigned int w_data);

struct test {
	u8 data;
	struct test *nextBuf;
};

static struct test *testBuf;

#endif

#if defined (CONFIG_MACH_CANE) || defined (CONFIG_MACH_LOGANRE)
static struct regulator *l29, *l32, *l34;
#endif

static int sr030pc50_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
static void sr030pc50_set_ev(int ev);
static void sr030pc50_set_flip(int flip);
DEFINE_MUTEX(sr030pc50_mut);

#define sr030pc50_WRT_LIST(A)	\
	sr030pc50_i2c_wrt_list(A, (sizeof(A) / sizeof(A[0])), #A);

#define CAM_REV ((system_rev <= 1) ? 0 : 1)
#if defined(CONFIG_MACH_WILCOX_EUR_LTE)
#include "sr030pc50_wilcox_regs.h"
#elif defined(CONFIG_MACH_LOGANRE_LTN_LTE)
#include "sr030pc50_loganre_60hz.h"
#elif defined(CONFIG_MACH_LOGANRE)
#include "sr030pc50_loganre_regs.h"
#else
#if defined(CONFIG_MACH_CANE_SKT) || defined(CONFIG_MACH_CANE_KTT)
#include "sr030pc50_regs_cane_kor_60Hz.h"
#else
#include "sr030pc50_regs.h"
#endif
#endif
static unsigned int config_csi2;

#ifdef CONFIG_LOAD_FILE

void sr030pc50_regs_table_init(void)
{
	struct file *fp = NULL;
	struct test *nextBuf = NULL;

	u8 *nBuf = NULL;
	size_t file_size = 0, max_size = 0, testBuf_size = 0;
	ssize_t nread = 0;
	s32 check = 0, starCheck = 0;
	s32 i = 0;
	int ret = 0;
	loff_t pos;
	/*Get the current address space */
	mm_segment_t fs = get_fs();

	cam_info("CONFIG_LOAD_FILE is enable!!");

	/*Set the current segment to kernel data segment */
	set_fs(get_ds());

	fp = filp_open("/mnt/sdcard/sr030pc50_regs.h", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open /mnt/sdcard/sr030pc50_regs.h");
		return ;
	}

	file_size = (size_t) fp->f_path.dentry->d_inode->i_size;
	max_size = file_size;
	cam_info("regs_table_init : file_size = %d", file_size);
	nBuf = vmalloc(file_size);
	if (nBuf == NULL) {
		cam_err("ERR: nBuf Out of Memory");
		ret = -ENOMEM;
		goto error_out;
	}

	testBuf_size = sizeof(struct test) * file_size;

	testBuf = vmalloc(testBuf_size);
	if (testBuf == NULL) {
		cam_err("ERR: Out of Memory");
		ret = -ENOMEM;
		goto error_out;
	}

	pos = 0;
	memset(nBuf, 0, file_size);
	memset(testBuf, 0, file_size * sizeof(struct test));
	nread = vfs_read(fp, (char __user *)nBuf, file_size, &pos);
	if (nread != file_size) {
		cam_err("failed to read file ret = %d", nread);
		ret = -1;
		goto error_out;
	}
	/*close the file*/
	filp_close(fp, current->files);

	/*restore the previous address space*/
	set_fs(fs);

	i = max_size;

	cam_err("i = %d", i);

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

	while (i - 1) {
		if (!check && !starCheck) {
			if (testBuf[max_size - i].data == '/') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data
								== '/') {
						check = 1;/* when find '//' */
						i--;
					} else if (testBuf[max_size-i].nextBuf->
								data == '*') {
						starCheck = 1;/*when'/ *' */
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
					if (testBuf[max_size-i].nextBuf->
								data == '*') {
						starCheck = 1; /*when '/ *' */
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
			if (testBuf[max_size - i].data
						== '*') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->
								data == '/') {
						starCheck = 0; /*when'* /' */
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

#ifdef FOR_DEBUG /* for print */
	printk(KERN_DEBUG "i = %d\n", i);
	nextBuf = &testBuf[0];
	while (1) {
		if (nextBuf->nextBuf == NULL)
			break;
		/*printk(KERN_DEBUG "DATA---%c", nextBuf->data);*/
		nextBuf = nextBuf->nextBuf;
	}
#endif
	vfree(nBuf);
	CAM_DEBUG(" : X");
	return ;
error_out:
	CAM_DEBUG(" : error out X");
	vfree(nBuf);
	if (fp)
		filp_close(fp, current->files);
	return ;
}

static inline int sr030pc50_write(struct i2c_client *client, u16 packet)
{
	u8 buf[2];
	int err = 0, retry_count = 5;

	struct i2c_msg msg;

	if (!client->adapter) {
		cam_err("ERR - can't search i2c client adapter");
		return -EIO;
	}

	buf[0] = (u8) (packet >> 8);
	buf[1] = (u8) (packet & 0xff);

	msg.addr = sr030pc50_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	do {
		err = i2c_transfer(sr030pc50_client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d",
				packet, err);
		usleep(3000);

	} while (retry_count <= 5);

	return (err != 1) ? -1 : 0;
}


static int sr030pc50_write_regs_from_sd(char *name)
{
	struct test *tempData;

	int ret = -EAGAIN;
	u16 temp;
	u16 delay = 0;
	u8 data[7];
	s32 searched = 0;
	size_t size = strlen(name);
	s32 i;

	CAM_DEBUG(" : E");

	if (testBuf == NULL){
		cam_err("sr030pc50_write_regs_from_sd : fail");
		return 0;
	}
	/*msleep(10000);*/
	/*printk("E size = %d, string = %s\n", size, name);*/
	tempData = &testBuf[0];

	*(data + 6) = '\0';

	while (!searched) {
		searched = 1;
		for (i = 0; i < size; i++) {
			if (tempData != NULL) {
				if (tempData->data != name[i]) {
					searched = 0;
					break;
				}
			} else {
				cam_err("tempData is NULL");
			}
			tempData = tempData->nextBuf;
		}
		tempData = tempData->nextBuf;
	}

	/* structure is get..*/
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
				for (i = 1; i < 6; i++) {
					data[i] = tempData->data;
					tempData = tempData->nextBuf;
				}
				//kstrtoul(data, 16, &temp);
				temp = (unsigned short)simple_strtoul(data, NULL, 16);
				/*CAM_DEBUG("%s\n", data);
				CAM_DEBUG("kstrtoul data = 0x%x\n", temp);*/
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
		if ((temp & 0xFF00) == SR030PC50_DELAY) {
			delay = temp & 0xFF;
			cam_info("SR030PC50 delay(%d)", delay);
			/*step is 10msec */
			msleep(delay);
			continue;
		}
		ret = sr030pc50_write(sr030pc50_client, temp);

		/* In error circumstances */
		/* Give second shot */
		if (unlikely(ret)) {
			ret = sr030pc50_write(sr030pc50_client, temp);

			/* Give it one more shot */
			if (unlikely(ret))
				ret = sr030pc50_write(sr030pc50_client, temp);
			}
		}

	CAM_DEBUG(" : X");

	return 0;
}

void sr030pc50_regs_table_exit(void)
{
	CAM_DEBUG(" : E");
	if (testBuf == NULL) {
		CAM_DEBUG("testBuf is NULL");
		return;
	} else {
		vfree(testBuf);
		testBuf = NULL;
		CAM_DEBUG("free the testBuf and initialized ");
	}
	CAM_DEBUG(" : X");
}

#endif

static DECLARE_WAIT_QUEUE_HEAD(sr030pc50_wait_queue);

/**
 * sr030pc50_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 * @r_data: buffer where data is read
 * @r_len: number of bytes to read
 *
 * Returns 0 on success, <0 on error
 */

/**
 * sr030pc50_i2c_read: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static int sr030pc50_i2c_read(unsigned char subaddr, unsigned char *data)
{
	unsigned char buf[1];
	struct i2c_msg msg = {sr030pc50_client->addr, 0, 1, buf};

	int err = 0;
	buf[0] = subaddr;

	if (!sr030pc50_client->adapter)
		return -EIO;

	err = i2c_transfer(sr030pc50_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;

	msg.flags = I2C_M_RD;

	err = i2c_transfer(sr030pc50_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;
	/*
	 * Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */

	*data = buf[0];

	return err;
}
#ifdef CONFIG_LOAD_FILE
/**
 * sr030pc50_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
 /*
static int sr030pc50_i2c_write_multi(unsigned short addr, unsigned int w_data)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (addr >> 8);
	buf[1] = (u8) (w_data & 0xff);

	msg.addr = sr030pc50_client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = buf;


	cam_info("I2C CHIP ID=0x%x, DATA 0x%x 0x%x",
			sr030pc50_client->addr, buf[0], buf[1]);


	do {
		rc = i2c_transfer(sr030pc50_client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		cam_err("retry_count %d\n", retry_count);
		usleep(3000);

	} while (retry_count <= 5);

	return 0;
}
*/
#endif

static int32_t sr030pc50_i2c_write_16bit(u16 packet)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (packet >> 8);
	buf[1] = (u8) (packet & 0xff);

	msg.addr = sr030pc50_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

#if defined(CAM_I2C_DEBUG)
	cam_info("I2C CHIP ID=0x%x, DATA 0x%x 0x%x",
			sr030pc50_client->addr, buf[0], buf[1]);
#endif

	do {
		rc = i2c_transfer(sr030pc50_client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d", packet, rc);
		usleep(3000);

	} while (retry_count <= 5);

	return -EIO;
}

static int sr030pc50_i2c_wrt_list(const u16 *regs,
	int size, char *name)
{
#ifdef CONFIG_LOAD_FILE
	sr030pc50_write_regs_from_sd(name);
#else

	int i;
	u8 m_delay = 0;

	u16 temp_packet;


	CAM_DEBUG("%s, size=%d", name, size);
	for (i = 0; i < size; i++) {
		temp_packet = regs[i];

		if ((temp_packet & SR030PC50_DELAY) == SR030PC50_DELAY) {
			m_delay = temp_packet & 0xFF;
			cam_info("delay = %d", m_delay*10);
			msleep(m_delay*10);/*step is 10msec*/
			continue;
		}

		if (sr030pc50_i2c_write_16bit(temp_packet) < 0) {
			cam_err("fail(0x%x, 0x%x:%d)",
					sr030pc50_client->addr, temp_packet, i);
			return -EIO;
		}
		/*udelay(10);*/
	}
#endif

	return 0;
}

static int sr030pc50_set_exif(void)
{
	int err = 0;
	u8 read_value1 = 0;
	u8 read_value2 = 0;
	u8 read_value3 = 0;
	u8 read_value4 = 0;
	unsigned int gain_value = 0;

	sr030pc50_i2c_write_16bit(0x0320);
	sr030pc50_i2c_read(0x80, &read_value1);
	sr030pc50_i2c_read(0x81, &read_value2);
	sr030pc50_i2c_read(0x82, &read_value3);
	sr030pc50_exif->shutterspeed = 12000000 / ((read_value1 << 19)
		+ (read_value2 << 11) + (read_value3 << 3));

	CAM_DEBUG("Exposure time = %d", sr030pc50_exif->shutterspeed);
	sr030pc50_i2c_write_16bit(0x0320);
/*	sr030pc50_i2c_read(0xb0, &read_value4);
	gain_value = (read_value4 / 16) * 1000;

	if (gain_value < 875)
		sr030pc50_exif->iso = 50;
	else if (gain_value < 1750)
		sr030pc50_exif->iso = 100;
	else if (gain_value < 4625)
		sr030pc50_exif->iso = 200;
	else
		sr030pc50_exif->iso = 400;
*/
/*	sr030pc50_i2c_read(0xb0, &read_value4);

	gain_value = (read_value4 / 32) * 1000 + 500;

	//calculate ISO
	if(gain_value < 1126)
		sr030pc50_exif->iso = 50;
	else if(gain_value < 1526)
		sr030pc50_exif->iso = 100;
	else if(gain_value < 2760)
		sr030pc50_exif->iso = 200;
	else
		sr030pc50_exif->iso = 400;
*/
	sr030pc50_i2c_read(0xb0, &read_value4);

	gain_value = (read_value4 ) * 1000 + 16000;

	//calculate ISO
	if(gain_value < 36032)
		sr030pc50_exif->iso = 50;
	else if(gain_value < 48832)
		sr030pc50_exif->iso = 100;
	else if(gain_value < 88320)
		sr030pc50_exif->iso = 200;
	else
		sr030pc50_exif->iso = 400;

	CAM_DEBUG("ISO = %d", sr030pc50_exif->iso);
	return err;
}


static int sr030pc50_get_exif(int exif_cmd, unsigned short value2)
{
	unsigned short val = 0;

	switch (exif_cmd) {
	case EXIF_SHUTTERSPEED:
		val = sr030pc50_exif->shutterspeed;
		break;

	case EXIF_ISO:
		val = sr030pc50_exif->iso;
		break;

	default:
		break;
	}

	return val;
}

static void sr030pc50_set_init_mode(void)
{
	config_csi2 = 0;
	sr030pc50_ctrl->cam_mode = PREVIEW_MODE;
	sr030pc50_ctrl->op_mode = CAMERA_MODE_INIT;
	sr030pc50_ctrl ->mirror_mode = 0;
	sr030pc50_ctrl->vtcall_mode = 0;
}


void sr030pc50_set_preview_size(int32_t index)
{
	CAM_DEBUG("index %d", index);

	sr030pc50_ctrl->settings.preview_size_idx = index;
}
/* Supporting effects */

static int sr030pc50_set_effect(int effect)
{
	CAM_DEBUG(" %d", effect);

	switch (effect) {
	case CAMERA_EFFECT_OFF:
		sr030pc50_WRT_LIST(sr030pc50_effect_none);
		break;

	case CAMERA_EFFECT_MONO:
		sr030pc50_WRT_LIST(sr030pc50_effect_gray);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		sr030pc50_WRT_LIST(sr030pc50_effect_negative);
		break;

	case CAMERA_EFFECT_SEPIA:
		sr030pc50_WRT_LIST(sr030pc50_effect_sepia);
		break;

	default:
		CAM_DEBUG("effect : default");
		sr030pc50_WRT_LIST(sr030pc50_effect_none);
		return 0;
	}

	sr030pc50_ctrl->settings.effect = effect;

	return 0;
}

void sr030pc50_set_frame_rate(int32_t fps)
{
	CAM_DEBUG("fps : %d", fps);

	switch (fps) {
	case 15:
#if defined(CONFIG_MACH_LOGANRE_LTN_LTE)
		sr030pc50_WRT_LIST(sr030pc50_15_fps_60Hz);
#else
		sr030pc50_WRT_LIST(sr030pc50_15_fps_50Hz);
#endif
		break;
	case 24:
	case 25:
	case 30:
#if defined(CONFIG_MACH_LOGANRE_LTN_LTE)
		sr030pc50_WRT_LIST(sr030pc50_24_fps_60Hz);
#else
		sr030pc50_WRT_LIST(sr030pc50_25_fps_50Hz);
#endif
		break;
	default:
		sr030pc50_WRT_LIST(sr030pc50_fps_Auto);
		break;
	}
	sr030pc50_ctrl->settings.fps = fps;
}

void sr030pc50_set_preview(void)
{

	u8 read_value1 = 0;
	u8 read_value2 = 0;
	u8 read_value3 = 0;
	u8 read_value4 = 0;
	u32 Expmax = 0, Exptime = 0;

	if (sr030pc50_ctrl->cam_mode == MOVIE_MODE) {
		CAM_DEBUG("Camcorder_Mode_ON");
		sr030pc50_ctrl->op_mode = CAMERA_MODE_RECORDING;
#if defined(CONFIG_MACH_LOGANRE_LTN_LTE)
		sr030pc50_WRT_LIST(sr030pc50_24_fps_60Hz);
#else
		sr030pc50_WRT_LIST(sr030pc50_25_fps_50Hz);
#endif
	} else if (sr030pc50_ctrl->cam_mode == VT_MODE) {
		CAM_DEBUG("VT_Mode");
		sr030pc50_ctrl->op_mode = CAMERA_MODE_PREVIEW;
/* Already set 10 FPS in sr030pc50_vt_mode_regs*/
/*
#if defined(CONFIG_MACH_LOGANRE_LTN_LTE)
		sr030pc50_WRT_LIST(sr030pc50_15_fps_60Hz);
#else
		sr030pc50_WRT_LIST(sr030pc50_15_fps_50Hz);
#endif
*/
	} else {
		if (sr030pc50_ctrl->op_mode == CAMERA_MODE_CAPTURE) {
			CAM_DEBUG("Preview_Mode from capture");
		}
		else {
			CAM_DEBUG("Preview_Mode from recording");
			sr030pc50_i2c_write_16bit(0x0320);
			sr030pc50_i2c_read(0x10, &read_value1);
			CAM_DEBUG("read_value1 = 0x%2x", read_value1);
			if(read_value1 & 0x10) {
				sr030pc50_i2c_read(0xA0, &read_value2);
				sr030pc50_i2c_read(0xA1, &read_value3);
				sr030pc50_i2c_read(0xA2, &read_value4);
				Expmax = (read_value2 << 16) | (read_value3 << 8) | read_value4;
				CAM_DEBUG("read_value = 0x%2x, 0x%2x, 0x%2x", read_value2, read_value3, read_value4);
				CAM_DEBUG("Expmax = 0x%8x", Expmax);
			} else {
				sr030pc50_i2c_read(0x88, &read_value2);
				sr030pc50_i2c_read(0x89, &read_value3);
				sr030pc50_i2c_read(0x8A, &read_value4);
				Expmax = (read_value2 << 16) | (read_value3 << 8) | read_value4;
				CAM_DEBUG("read_value = 0x%2x, 0x%2x, 0x%2x", read_value2, read_value3, read_value4);
				CAM_DEBUG("Expmax = 0x%8x", Expmax);
			}

			sr030pc50_i2c_read(0x80, &read_value2);
			sr030pc50_i2c_read(0x81, &read_value3);
			sr030pc50_i2c_read(0x82, &read_value4);
			Exptime = (read_value2 << 16) | (read_value3 << 8) | read_value4;
			CAM_DEBUG("read_value = 0x%2x, 0x%2x, 0x%2x", read_value2, read_value3, read_value4);
			CAM_DEBUG("Exptime = 0x%8x", Exptime);

			if (Exptime < Expmax) {
				sr030pc50_WRT_LIST(sr030pc50_fps_auto_normal_regs);
			} else {
				sr030pc50_WRT_LIST(sr030pc50_fps_auto_dark_regs);
			}
		}

		sr030pc50_ctrl->op_mode = CAMERA_MODE_PREVIEW;
	}

	if (sr030pc50_ctrl->mirror_mode == 1)
		sr030pc50_set_flip(sr030pc50_ctrl->mirror_mode);
}

void sr030pc50_set_capture(void)
{
	CAM_DEBUG("");
	sr030pc50_ctrl->op_mode = CAMERA_MODE_CAPTURE;

	if (sr030pc50_ctrl->mirror_mode == 1)
		sr030pc50_set_flip(sr030pc50_ctrl->mirror_mode);

	/*sr030pc50_WRT_LIST(sr030pc50_Capture);*/
	sr030pc50_set_exif();
}

static int32_t sr030pc50_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0;

	CAM_DEBUG("Start");

	switch (update_type) {
	case REG_INIT:
		break;

	case UPDATE_PERIODIC:
		msm_sensor_enable_debugfs(sr030pc50_ctrl->s_ctrl);
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			CAM_DEBUG("UPDATE_PERIODIC");

				if (sr030pc50_ctrl->s_ctrl->sensordata->
					sensor_platform_info->
					csi_lane_params != NULL) {
					CAM_DEBUG(" lane_assign ="\
						" 0x%x",
						sr030pc50_ctrl->s_ctrl->
						sensordata->
						sensor_platform_info->
						csi_lane_params->
						csi_lane_assign);

					CAM_DEBUG(" lane_mask ="\
						" 0x%x",
						sr030pc50_ctrl->s_ctrl->
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
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}
/*
static int32_t sr030pc50_video_config(int mode)
{
	int32_t	rc = 0;

	if (sr030pc50_sensor_setting(UPDATE_PERIODIC, RES_PREVIEW) < 0)
		rc = -1;

	CAM_DEBUG("hoon rc : %d", rc);

	return rc;
}
*/
static long sr030pc50_set_sensor_mode(int mode)
{
	cam_info("sensor_mode : %d", mode);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
	case SENSOR_VIDEO_MODE:
		sr030pc50_set_preview();
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		sr030pc50_set_capture();
		break;
	default:
		return 0;
	}
	return 0;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static int sr030pc50_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	cam_info("sensor_match_id : do nothing");

	return rc;
}

#if (defined(CONFIG_ISX012) || defined(CONFIG_S5K4ECGX)) && defined(CONFIG_SR030PC50)
static int sr030pc50_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int temp = 0;
#if defined (CONFIG_MACH_CANE) || defined (CONFIG_MACH_LOGANRE)
	int ret = 0;
#endif

	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
#ifdef CONFIG_LOAD_FILE
	sr030pc50_regs_table_init();
#endif

	cam_info("sensor_power_up : E");

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		cam_err("request gpio failed");

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("VT STBY : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("VT RST : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("5M RST : %d", temp);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("5M STBY : %d", temp);

	/*Power on the LDOs */
#if defined (CONFIG_MACH_CANE) || defined (CONFIG_MACH_LOGANRE)
	/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	l32 = regulator_get(NULL, "8917_l32");
	ret = regulator_set_voltage(l32, 2800000, 2800000);
	if (ret)
		cam_err("error setting voltage");
	ret = regulator_enable(l32);
	if (ret)
		cam_err("error enabling regulator");

	udelay(150);

	/*Sensor vt core 1.8 - VT_CORE_1P8 */
	l29 = regulator_get(NULL, "8921_l29");
	ret = regulator_set_voltage(l29, 1800000, 1800000);
	if (ret)
		cam_err("error setting voltage");
	ret = regulator_enable(l29);
	if (ret)
		cam_err("error enabling regulator");
	udelay(250);

	/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	l34 = regulator_get(NULL, "8917_l34");
	ret = regulator_set_voltage(l34, 1800000, 1800000);
	if (ret)
		cam_err("error setting voltage");
	ret = regulator_enable(l34);
	if (ret)
		cam_err("error enabling regulator");
	udelay(1000);

	/*5M core 1.2V - CAM_ISP_CORE_1P2 */
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_CORE_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);

	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	usleep(1700);
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);
	usleep(4500);
#else
	data->sensor_platform_info->sensor_power_on(1);
	usleep(1200);
#endif


	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 2,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0)
		cam_err(" Mclk enable failed");

	if (rc != 0)
		goto FAIL_END;

	usleep(15);

	/*standy VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("VT STBY : %d", temp);

	usleep(2500);

	/*reset VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("VT RST : %d", temp);
	msleep(60);

	sr030pc50_set_init_mode();

	rc = sr030pc50_WRT_LIST(sr030pc50_Init_Reg);

FAIL_END:
	if (rc) {
		cam_err("Power up Failed!!");
		msm_camera_request_gpio_table(data, 0);
	} else {
		cam_err(" X");
	}

	return rc;
}
#else
static int sr030pc50_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	printk(KERN_DEBUG "sr030pc50_sensor_power_up");
	return 0;
}
#endif

static void sr030pc50_set_ev(int ev)
{
	CAM_DEBUG("EV(%d)", ev);

	switch (ev) {
	case CAMERA_EV_M4:
		sr030pc50_WRT_LIST(sr030pc50_brightness_M4);
		break;

	case CAMERA_EV_M3:
		sr030pc50_WRT_LIST(sr030pc50_brightness_M3);
		break;

	case CAMERA_EV_M2:
		sr030pc50_WRT_LIST(sr030pc50_brightness_M2);
		break;

	case CAMERA_EV_M1:
		sr030pc50_WRT_LIST(sr030pc50_brightness_M1);
		break;

	case CAMERA_EV_DEFAULT:
		sr030pc50_WRT_LIST(sr030pc50_brightness_default);
		break;

	case CAMERA_EV_P1:
		sr030pc50_WRT_LIST(sr030pc50_brightness_P1);
		break;

	case CAMERA_EV_P2:
		sr030pc50_WRT_LIST(sr030pc50_brightness_P2);
		break;

	case CAMERA_EV_P3:
		sr030pc50_WRT_LIST(sr030pc50_brightness_P3);
		break;

	case CAMERA_EV_P4:
		sr030pc50_WRT_LIST(sr030pc50_brightness_P4);
		break;

	default:
		CAM_DEBUG("unexpected ev mode");
		break;
	}

	sr030pc50_ctrl->settings.brightness = ev;
}

void sensor_native_control_front(void __user *arg)
{
	struct ioctl_native_cmd ctrl_info;

	struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;

	if (copy_from_user(&ctrl_info,
		(void __user *)ioctl_ptr->ioctl_ptr,
		sizeof(ctrl_info))) {
		cam_err("fail copy_from_user!");
	}

	CAM_DEBUG("mode : %d", ctrl_info.mode);

	switch (ctrl_info.mode) {

	case EXT_CAM_EV:
		sr030pc50_set_ev(ctrl_info.value_1);
		break;

	case  EXT_CAM_MOVIE_MODE:
		cam_info("EXT_CAM_MOVIE_MODE: %d", ctrl_info.value_1);
		sr030pc50_ctrl->cam_mode = ctrl_info.value_1;
		break;

	case EXT_CAM_EXIF:
		cam_info("EXT_CAM_EXIF: %d", ctrl_info.address);
		ctrl_info.value_1 = sr030pc50_get_exif(ctrl_info.address,
			ctrl_info.value_2);
		break;
	case EXT_CAM_SET_FLIP:
		cam_info("EXT_CAM_SET_FLIP: %d", ctrl_info.value_1);
		sr030pc50_set_flip(ctrl_info.value_1);
		sr030pc50_ctrl->mirror_mode = ctrl_info.value_1;
		break;

	case EXT_CAM_EFFECT:
		cam_info("EXT_CAM_EFFECT: %d", ctrl_info.value_1);
		sr030pc50_set_effect(ctrl_info.value_1);
		break;

	case EXT_CAM_SET_FPS:  // 31
		cam_info("EXT_CAM_SET_FPS: %d", ctrl_info.value_1);
		//sr030pc50_set_frame_rate(ctrl_info.value_1);
		break;

	case EXT_CAM_VT_MODE:  // 37
		cam_info("EXT_CAM_VT_MODE: %d", ctrl_info.value_1);
		if (ctrl_info.value_1 == 1) {
			sr030pc50_ctrl->cam_mode = VT_MODE;
			sr030pc50_WRT_LIST(sr030pc50_vt_mode_regs);
		}
		break;
/*
	case EXT_CAM_WB:
		sr030pc50_set_whitebalance(ctrl_info.value_1);
		break;

	case EXT_CAM_DTP_TEST:
		sr030pc50_check_dataline(ctrl_info.value_1);
		break;

	case EXT_CAM_PREVIEW_SIZE:
		sr030pc50_set_preview_size(ctrl_info.value_1);
		break;
*/
	default:
		CAM_DEBUG("default mode");
		break;
	}
	if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
		(const void *)&ctrl_info,
			sizeof(ctrl_info))) {
		cam_err("fail copy_to_user!");
	}

}

long sr030pc50_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	void __user *argp = (void __user *)arg;
	struct msm_sensor_ctrl_t *sr030pc50_s_ctrl = get_sctrl(sd);

	CAM_DEBUG("");
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return sr030pc50_sensor_config(sr030pc50_s_ctrl, argp);
	/*case VIDIOC_MSM_SENSOR_RELEASE:*/
	/*	return msm_sensor_release(sr030pc50_s_ctrl);*/
	case VIDIOC_MSM_SENSOR_CSID_INFO:
		{
		struct msm_sensor_csi_info *csi_info =
			(struct msm_sensor_csi_info *)arg;
		/*sr030pc50_s_ctrl->csid_version = csi_info->csid_version;*/
		sr030pc50_s_ctrl->is_csic = csi_info->is_csic;
		return 0;
		}
	default:
		return -ENOIOCTLCMD;
	}

}

int sr030pc50_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data, (void *)argp,
						sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	cam_info("sensor_cfgtype = %d, mode = %d",
			cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
	case CFG_SENSOR_INIT:
		if (config_csi2 == 0)
			rc = sr030pc50_sensor_setting(UPDATE_PERIODIC,
				RES_PREVIEW);
		break;
	case CFG_SET_MODE:
		rc = sr030pc50_set_sensor_mode(cfg_data.mode);
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
			CAM_DEBUG("CFG_GET_CSI_PARAMS Failed!!");
			rc = -EFAULT;
			break;
		}

		rc = s_ctrl->func_tbl->sensor_get_csi_params(
		s_ctrl, &cfg_data.cfg.csi_lane_params);
		CAM_DEBUG("Inside CFG_GET_CSI_PARAMS");
		if (copy_to_user((void *)argp,
				(const void *)&cfg_data, sizeof(cfg_data)))
			rc = -EFAULT;

		break;

	case CFG_GET_AF_MAX_STEPS:
	default:
		rc = 0;
		CAM_DEBUG(" Invalid cfgtype = %d", cfg_data.cfgtype);
		break;
	}

	return rc;
}

static int sr030pc50_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int temp = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	cam_info("sensor_power_down : E");

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("VT RST : %d", temp);

	usleep(15);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("VT STBY : %d", temp);

	usleep(1200); /* 20clk = 0.833us */

	/*CAM_MCLK0*/
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	usleep(10);

#if defined (CONFIG_MACH_CANE) || defined (CONFIG_MACH_LOGANRE)
	/*VT core 1.8 - CAM_DVDD_1P8*/
	if (l29) {
		rc = regulator_disable(l29);
		if (rc)
			cam_err("error disabling regulator");
	}

	usleep(10);

	/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	if (l32) {
		rc = regulator_disable(l32);
		if (rc)
			cam_err("error disabling regulator");
	}
	usleep(10);

	/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (l34) {
			rc = regulator_disable(l34);
			if (rc)
				cam_err("error disabling regulator");
	}
	usleep(10);

	/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);
#else
	data->sensor_platform_info->sensor_power_off(1);
#endif

	msm_camera_request_gpio_table(data, 0);

#ifdef CONFIG_LOAD_FILE
	sr030pc50_regs_table_exit();
#endif
	return rc;
}

static struct v4l2_subdev_info sr030pc50_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static int sr030pc50_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code) {
	CAM_DEBUG("Index is %d", index);
	if ((unsigned int)index >= ARRAY_SIZE(sr030pc50_subdev_info))
		return -EINVAL;

	*code = sr030pc50_subdev_info[index].code;
	return 0;
}

static void sr030pc50_set_flip(int flip)
{
	unsigned char r_data, checkSubSampling = 0;
	CAM_DEBUG("flip : %d", flip);

	sr030pc50_i2c_write_16bit(0xFF87);
	sr030pc50_i2c_read(0xD5, &r_data);
	if (r_data & 0x02)
		checkSubSampling = 1;
	switch (flip) {
	case 0:
		if (sr030pc50_ctrl->cam_mode == MOVIE_MODE) {
			sr030pc50_WRT_LIST(sr030pc50_flip_off_No15fps);
		} else {					/* for 15fps */
			sr030pc50_WRT_LIST(sr030pc50_flip_off);
		}
		break;

	case 1:
		if (sr030pc50_ctrl->cam_mode == MOVIE_MODE) {
			sr030pc50_WRT_LIST(sr030pc50_hflip_No15fps);
		} else {					/* for 15fps */
			sr030pc50_WRT_LIST(sr030pc50_hflip);
		}
		break;

	default:
		CAM_DEBUG("flip : default");
		break;
	}
}

void sr030pc50_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	cam_info("sensor_start_stream");
	sr030pc50_WRT_LIST(sr030pc50_start_stream);
	msleep(100);
}

void sr030pc50_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	cam_info("sensor_stop_stream");
	sr030pc50_WRT_LIST(sr030pc50_stop_stream);
}

static struct v4l2_subdev_core_ops sr030pc50_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = sr030pc50_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops sr030pc50_subdev_video_ops = {
	.enum_mbus_fmt = sr030pc50_enum_fmt,
};

static struct v4l2_subdev_ops sr030pc50_subdev_ops = {
	.core = &sr030pc50_subdev_core_ops,
	.video  = &sr030pc50_subdev_video_ops,
};

static int sr030pc50_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	cam_err("%s_i2c_probe called", client->name);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		cam_err("i2c_check_functionality failed\n");
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
		return rc;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	if (s_ctrl->sensordata == NULL) {
		cam_err("NULL sensor data");
		rc = -EFAULT;
		goto probe_failure;
	}

	sr030pc50_client = client;
	sr030pc50_dev = s_ctrl->sensor_i2c_client->client->dev;

	sr030pc50_ctrl = kzalloc(sizeof(struct sr030pc50_ctrl), GFP_KERNEL);
	if (!sr030pc50_ctrl) {
		CAM_DEBUG("sr030pc50_ctrl alloc failed!\n");
		return -ENOMEM;
	}

	sr030pc50_exif = kzalloc(sizeof(struct sr030pc50_exif_data),
		GFP_KERNEL);
	if (!sr030pc50_exif) {
		cam_err("Cannot allocate memory fo EXIF structure!");
		kfree(sr030pc50_ctrl);
		rc = -ENOMEM;
		goto probe_failure;
	}

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);

	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		&sr030pc50_subdev_ops);

	sr030pc50_ctrl->s_ctrl = s_ctrl;
	sr030pc50_ctrl->sensor_dev = &s_ctrl->sensor_v4l2_subdev;
	sr030pc50_ctrl->sensordata = client->dev.platform_data;

	rc = msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);

	if (rc < 0) {
		cam_err("msm_sensor_register failed!");
		kfree(sr030pc50_exif);
		kfree(sr030pc50_ctrl);
		goto probe_failure;
		}

	cam_err("sr030pc50_probe succeeded!");
	return 0;

probe_failure:
	cam_err("sr030pc50_probe failed!");
	return rc;
}



static struct msm_sensor_id_info_t sr030pc50_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x0074,
};

static const struct i2c_device_id sr030pc50_i2c_id[] = {
	{"sr030pc50", (kernel_ulong_t)&sr030pc50_s_ctrl},
	{},
};

static struct msm_camera_i2c_client sr030pc50_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static struct i2c_driver sr030pc50_i2c_driver = {
	.id_table = sr030pc50_i2c_id,
	.probe  = sr030pc50_i2c_probe,
	.driver = {
		.name = "sr030pc50",
	},
};

static int __init sr030pc50_init(void)
{
	return i2c_add_driver(&sr030pc50_i2c_driver);
}

static struct msm_sensor_fn_t sr030pc50_func_tbl = {
	.sensor_config = sr030pc50_sensor_config,
	.sensor_power_up = sr030pc50_sensor_power_up,
	.sensor_power_down = sr030pc50_sensor_power_down,
	.sensor_match_id = sr030pc50_sensor_match_id,
	/*.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,*/
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_start_stream = sr030pc50_sensor_start_stream,
	.sensor_stop_stream = sr030pc50_sensor_stop_stream,
	/*.sensor_get_lens_info = sensor_get_lens_info,*/
};


static struct msm_sensor_reg_t sr030pc50_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_sensor_ctrl_t sr030pc50_s_ctrl = {
	.msm_sensor_reg = &sr030pc50_regs,
	.sensor_i2c_client = &sr030pc50_sensor_i2c_client,
	.sensor_i2c_addr = 0x30,
	.sensor_id_info = &sr030pc50_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	/*.csic_params = &sr030pc50_csic_params_array[0],
	.csi_params = &sr030pc50_csi_params_array[0],*/
	.msm_sensor_mutex = &sr030pc50_mut,
	.sensor_i2c_driver = &sr030pc50_i2c_driver,
	.sensor_v4l2_subdev_info = sr030pc50_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sr030pc50_subdev_info),
	.sensor_v4l2_subdev_ops = &sr030pc50_subdev_ops,
	.func_tbl = &sr030pc50_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};


module_init(sr030pc50_init);
