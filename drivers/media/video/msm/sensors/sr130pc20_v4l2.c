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

#include "sr130pc20.h"
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
static char *sr130pc20_regs_table;
static int sr130pc20_regs_table_size;
static int sr130pc20_write_regs_from_sd(char *name);
static int sr130pc20_i2c_write_multi(unsigned short addr, unsigned int w_data);
struct test {
	u8 data;
	struct test *nextBuf;
};
static struct test *testBuf;
static s32 large_file;
#endif
static int sr130pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
static void sr130pc20_set_ev(int ev);
static void sr130pc20_set_flip(int flip);
DEFINE_MUTEX(sr130pc20_mut);

#define sr130pc20_WRT_LIST(A)	\
	sr130pc20_i2c_wrt_list(A, (sizeof(A) / sizeof(A[0])), #A);

#define CAM_REV ((system_rev <= 1) ? 0 : 1)

#if defined(CONFIG_MACH_LT02_CHN_CTC)
#include "sr130pc20_lt02_ctc_regs.h"
#elif defined(CONFIG_MACH_LT02_SEA)
#include "sr130pc20_lt02_sea_regs.h"
#else
#include "sr130pc20_regs.h"
#endif
static unsigned int config_csi2;
static unsigned int stop_stream;
static unsigned int effect_mode = CAMERA_EFFECT_OFF;
static unsigned int wb_mode = CAMERA_WHITE_BALANCE_AUTO;
static unsigned int capture_mode = 0;

#ifdef CONFIG_LOAD_FILE

void sr130pc20_regs_table_init(void)
{
	struct file *fp = NULL;
	struct test *nextBuf = NULL;

	u8 *nBuf = NULL;
	size_t file_size = 0, max_size = 0, testBuf_size = 0;
	ssize_t nread = 0;
	s32 check = 0, starCheck = 0;
	s32 tmp_large_file = 0;
	s32 i = 0;
	int ret = 0;
	loff_t pos;
	cam_err("CONFIG_LOAD_FILE is enable!!\n");

	/*Get the current address space */
	mm_segment_t fs = get_fs();

	CAM_DEBUG("%s %d", __func__, __LINE__);

	/*Set the current segment to kernel data segment */
	set_fs(get_ds());

	fp = filp_open("/mnt/sdcard/sr130pc20_regs.h", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open /mnt/sdcard/sr130pc20_regs.h");
		return PTR_ERR(fp);
	}

	file_size = (size_t) fp->f_path.dentry->d_inode->i_size;
	max_size = file_size;
	cam_err("file_size = %d", file_size);
	nBuf = kmalloc(file_size, GFP_ATOMIC);
	if (nBuf == NULL) {
		cam_err("Fail to 1st get memory");
		nBuf = vmalloc(file_size);
		if (nBuf == NULL) {
			cam_err("ERR: nBuf Out of Memory");
			ret = -ENOMEM;
			goto error_out;
		}
		tmp_large_file = 1;
	}

	testBuf_size = sizeof(struct test) * file_size;
	if (tmp_large_file) {
		testBuf = vmalloc(testBuf_size);
		large_file = 1;
	} else {
		testBuf = kmalloc(testBuf_size, GFP_ATOMIC);
		if (testBuf == NULL) {
			cam_err("Fail to get mem(%d bytes)", testBuf_size);
			testBuf = vmalloc(testBuf_size);
			large_file = 1;
		}
	}
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

#if FOR_DEBUG /* for print */
	printk(KERN_DEBUG "i = %d\n", i);
	nextBuf = &testBuf[0];
	while (1) {
		if (nextBuf->nextBuf == NULL)
			break;
		/*printk(KERN_DEBUG "DATA---%c", nextBuf->data);*/
		nextBuf = nextBuf->nextBuf;
	}
#endif
	tmp_large_file ? vfree(nBuf) : kfree(nBuf);

error_out:
	if (fp)
		filp_close(fp, current->files);
	return ret;
}
static inline int sr130pc20_write(struct i2c_client *client,
		u16 packet)
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

	msg.addr = sr130pc20_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	do {
		err = i2c_transfer(sr130pc20_client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d\n",
		       packet, err);
		usleep(3000);

	} while (retry_count <= 5);

	return (err != 1) ? -1 : 0;
}


static int sr130pc20_write_regs_from_sd(char *name)
{
	struct test *tempData = NULL;

	int ret = -EAGAIN;
	u16 temp, temp_2;
	u16 delay = 0;
	u8 data[7];
	s32 searched = 0;
	size_t size = strlen(name);
	s32 i;
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
				kstrtoul(data, 16, &temp);
				/*CAM_DEBUG("%s\n", data);
				CAM_DEBUG("kstrtoul data = 0x%x\n", temp);*/
				break;
			} else if (tempData->data == '}') {
				searched = 1;
				break;
			} else
				tempData = tempData->nextBuf;

			if (tempData->nextBuf == NULL)
				return NULL;
		}

		if (searched)
			break;
		if ((temp & 0xFF00) == SR130PC20_DELAY) {
			delay = temp & 0xFF;
			cam_info("SR130PC20 delay(%d)", delay);
			/*step is 10msec */
			msleep(delay);
			continue;
		}
		ret = sr130pc20_write(sr130pc20_client, temp);

		/* In error circumstances */
		/* Give second shot */
		if (unlikely(ret)) {
			ret = sr130pc20_write(sr130pc20_client, temp);

			/* Give it one more shot */
			if (unlikely(ret))
				ret = sr130pc20_write(sr130pc20_client, temp);
			}
		}

	return 0;
}

void sr130pc20_regs_table_exit(void)
{
	if (testBuf == NULL)
		return;
	else {
		large_file ? vfree(testBuf) : kfree(testBuf);
		large_file = 0;
		testBuf = NULL;
	}
}

#endif

static DECLARE_WAIT_QUEUE_HEAD(sr130pc20_wait_queue);

/**
 * sr130pc20_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor
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
 * sr130pc20_i2c_read: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static int sr130pc20_i2c_read(unsigned char subaddr, unsigned char *data)
{
	unsigned char buf[1];
	struct i2c_msg msg = {sr130pc20_client->addr, 0, 1, buf};

	int err = 0;
	buf[0] = subaddr;

	if (!sr130pc20_client->adapter)
		return -EIO;

	err = i2c_transfer(sr130pc20_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;

	msg.flags = I2C_M_RD;

	err = i2c_transfer(sr130pc20_client->adapter, &msg, 1);
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
 * sr130pc20_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int sr130pc20_i2c_write_multi(unsigned short addr, unsigned int w_data)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (addr >> 8);
	buf[1] = (u8) (w_data & 0xff);

	msg.addr = sr130pc20_client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = buf;


	cam_err("I2C CHIP ID=0x%x, DATA 0x%x 0x%x\n",
			sr130pc20_client->addr, buf[0], buf[1]);


	do {
		rc = i2c_transfer(sr130pc20_client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		cam_err("retry_count %d\n", retry_count);
		usleep(3000);

	} while (retry_count <= 5);

	return 0;
}
#endif

static int32_t sr130pc20_i2c_write_16bit(u16 packet)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (packet >> 8);
	buf[1] = (u8) (packet & 0xff);

	msg.addr = sr130pc20_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

#if defined(CAM_I2C_DEBUG)
	cam_err("I2C CHIP ID=0x%x, DATA 0x%x 0x%x\n",
			sr130pc20_client->addr, buf[0], buf[1]);
#endif

	do {
		rc = i2c_transfer(sr130pc20_client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d\n",
		       packet, rc);
		usleep(3000);
		return -EIO;

	} while (retry_count <= 5);

	return 0;
}

static int sr130pc20_i2c_wrt_list(const u16 *regs,
	int size, char *name)
{
#ifdef CONFIG_LOAD_FILE
	sr130pc20_write_regs_from_sd(name);
#else

	int i;
	u8 m_delay = 0;

	u16 temp_packet;


	CAM_DEBUG("%s, size=%d", name, size);
	for (i = 0; i < size; i++) {
		temp_packet = regs[i];

		if ((temp_packet & SR130PC20_DELAY) == SR130PC20_DELAY) {
			m_delay = temp_packet & 0xFF;
			cam_info("delay = %d", m_delay*10);
			msleep(m_delay*10);/*step is 10msec*/
			continue;
		}

		if (sr130pc20_i2c_write_16bit(temp_packet) < 0) {
			cam_err("fail(0x%x, 0x%x:%d)",
					sr130pc20_client->addr, temp_packet, i);
			return -EIO;
		}
		/*udelay(10);*/
	}
#endif

	return 0;
}

static int sr130pc20_set_exif(void)
{
	int err = 0;
	u8 read_value1 = 0;
	u8 read_value2 = 0;
	u8 read_value3 = 0;
	u8 read_value4 = 0;
	unsigned short gain_value = 0;

	sr130pc20_i2c_write_16bit(0x0320);
	sr130pc20_i2c_read(0x80, &read_value1);
	sr130pc20_i2c_read(0x81, &read_value2);
	sr130pc20_i2c_read(0x82, &read_value3);
	sr130pc20_exif->shutterspeed = 24000000 / ((read_value1 << 19)
		+ (read_value2 << 11) + (read_value3 << 3));

	CAM_DEBUG("Exposure time = %d\n", sr130pc20_exif->shutterspeed);
	sr130pc20_i2c_write_16bit(0x0320);
	sr130pc20_i2c_read(0xb0, &read_value4);
	gain_value = (read_value4 / 16) * 1000;

	if (gain_value < 875)
		sr130pc20_exif->iso = 50;
	else if (gain_value < 1750)
		sr130pc20_exif->iso = 100;
	else if (gain_value < 4625)
		sr130pc20_exif->iso = 200;
	else
		sr130pc20_exif->iso = 400;

	CAM_DEBUG("ISO = %d\n", sr130pc20_exif->iso);
	return err;
}


static int sr130pc20_get_exif(int exif_cmd, unsigned short value2)
{
	unsigned short val = 0;

	switch (exif_cmd) {
	case EXIF_SHUTTERSPEED:
		val = sr130pc20_exif->shutterspeed;
		break;

	case EXIF_ISO:
		val = sr130pc20_exif->iso;
		break;

	default:
		break;
	}

	return val;
}

static void sr130pc20_set_init_mode(void)
{
	config_csi2 = 0;
	sr130pc20_ctrl->cam_mode = PREVIEW_MODE;
	sr130pc20_ctrl->op_mode = CAMERA_MODE_INIT;
	sr130pc20_ctrl->mirror_mode = 0;
	sr130pc20_ctrl->vtcall_mode = 0;
}
void sr130pc20_set_preview_size(int32_t index)
{
	CAM_DEBUG("index %d", index);

	sr130pc20_ctrl->settings.preview_size_idx = index;
}

/* Supporting effects and WB-Start */

static int sr130pc20_set_effect(int effect)
{
	CAM_DEBUG(" %d", effect);

	switch (effect) {
	case CAMERA_EFFECT_OFF:
		sr130pc20_WRT_LIST(sr130pc20_effect_none);
		break;

	case CAMERA_EFFECT_MONO:
		sr130pc20_WRT_LIST(sr130pc20_effect_gray);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		sr130pc20_WRT_LIST(sr130pc20_effect_negative);
		break;

	case CAMERA_EFFECT_SEPIA:
		sr130pc20_WRT_LIST(sr130pc20_effect_sepia);
		break;

	default:
		CAM_DEBUG(" effect : default");
		sr130pc20_WRT_LIST(sr130pc20_effect_none);
		return 0;
	}

	sr130pc20_ctrl->settings.effect = effect;
	sr130pc20_set_ev(sr130pc20_ctrl->settings.brightness);//setting again as exposure is reset by above register settings

	return 0;
}


void sr130pc20_set_whitebalance(int wb)
{
	CAM_DEBUG(" %d", wb);

	switch (wb) {
	case CAMERA_WHITE_BALANCE_AUTO:
		sr130pc20_WRT_LIST(sr130pc20_wb_auto);
		break;

	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		sr130pc20_WRT_LIST(sr130pc20_wb_tungsten);
		break;

	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		sr130pc20_WRT_LIST(sr130pc20_wb_fluorescent);
		break;

	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		sr130pc20_WRT_LIST(sr130pc20_wb_sunny);
		break;

	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		sr130pc20_WRT_LIST(sr130pc20_wb_cloudy);
		break;

	default:
		CAM_DEBUG(" WB : default");
		return ;
	}

	sr130pc20_ctrl->settings.wb = wb;
	sr130pc20_set_ev(sr130pc20_ctrl->settings.brightness);//setting again as exposure is reset by above register settings

	return ;
}
/* Supporting effects and WB-End */
void sr130pc20_set_preview(void)
{
	CAM_DEBUG("cam_mode = %d", sr130pc20_ctrl->cam_mode);

	if (sr130pc20_ctrl->cam_mode == MOVIE_MODE) {
		CAM_DEBUG("Camcorder_Mode_ON");
		if (sr130pc20_ctrl->settings.preview_size_idx ==
				PREVIEW_SIZE_HD) {
			CAM_DEBUG("720P recording");
		/*sr130pc20_WRT_LIST(sr130pc20_720p_common);*/
	} else {
			CAM_DEBUG("VGA recording");
		if (sr130pc20_ctrl->op_mode == CAMERA_MODE_INIT ||
			sr130pc20_ctrl->op_mode == CAMERA_MODE_PREVIEW) {
			if (stop_stream == 0) {
				sr130pc20_WRT_LIST(sr130pc20_Init_Reg);
				sr130pc20_WRT_LIST(sr130pc20_20_fps_60Hz);
				}

			if (sr130pc20_ctrl->mirror_mode == 1)
					sr130pc20_set_flip( \
					sr130pc20_ctrl->mirror_mode);
		}
		}
		sr130pc20_ctrl->op_mode = CAMERA_MODE_RECORDING;
	} else {

		CAM_DEBUG("Init_Mode");
		if (sr130pc20_ctrl->op_mode == CAMERA_MODE_INIT ||
			sr130pc20_ctrl->op_mode == CAMERA_MODE_RECORDING) {
			if (stop_stream == 0) {
				if (sr130pc20_ctrl->vtcall_mode == 1) {
					CAM_DEBUG(" VT common");
					sr130pc20_WRT_LIST\
						(sr130pc20_VT_Init_Reg);
				} else if (sr130pc20_ctrl->vtcall_mode == 2) {
					CAM_DEBUG(" WIFI VT common");
				} else {
				sr130pc20_WRT_LIST(sr130pc20_Init_Reg);
				CAM_DEBUG("Common Registers written\n");
				}
			}
			if (sr130pc20_ctrl->mirror_mode == 1)
				sr130pc20_set_flip(sr130pc20_ctrl->mirror_mode);
			}

		if (stop_stream == 0) {
			sr130pc20_WRT_LIST(sr130pc20_Preview);
			CAM_DEBUG("Preview Registers written\n");
		}
		sr130pc20_ctrl->op_mode = CAMERA_MODE_PREVIEW;
	}
	/*sr130pc20_set_ev(CAMERA_EV_DEFAULT);*/
	/*sr130pc20_set_ev(sr130pc20_ctrl->settings.brightness);*/
}

void sr130pc20_set_capture(void)
{
	CAM_DEBUG("");
	sr130pc20_ctrl->op_mode = CAMERA_MODE_CAPTURE;
	sr130pc20_WRT_LIST(sr130pc20_Capture);
	sr130pc20_set_exif();
	capture_mode = 1;
}

static int32_t sr130pc20_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0;

	CAM_DEBUG("Start");

	switch (update_type) {
	case REG_INIT:
		break;

	case UPDATE_PERIODIC:
		msm_sensor_enable_debugfs(sr130pc20_ctrl->s_ctrl);
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			CAM_DEBUG("UPDATE_PERIODIC");

				if (sr130pc20_ctrl->s_ctrl->sensordata->
					sensor_platform_info->
					csi_lane_params != NULL) {
					CAM_DEBUG(" lane_assign ="\
						" 0x%x",
						sr130pc20_ctrl->s_ctrl->
						sensordata->
						sensor_platform_info->
						csi_lane_params->
						csi_lane_assign);

					CAM_DEBUG(" lane_mask ="\
						" 0x%x",
						sr130pc20_ctrl->s_ctrl->
						sensordata->
						sensor_platform_info->
						csi_lane_params->
						csi_lane_mask);

				}

				mb();

				mb();

			config_csi2 = 1;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}
/*
static int32_t sr130pc20_video_config(int mode)
{
	int32_t	rc = 0;

	if (sr130pc20_sensor_setting(UPDATE_PERIODIC, RES_PREVIEW) < 0)
		rc = -1;

	CAM_DEBUG("hoon rc : %d", rc);

	return rc;
}
*/
static long sr130pc20_set_sensor_mode(int mode)
{
	CAM_DEBUG("%d", mode);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
	case SENSOR_VIDEO_MODE:
		sr130pc20_set_preview();
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		sr130pc20_set_capture();
		break;
	default:
		return 0;
	}
	return 0;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static int sr130pc20_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	cam_info(" Nothing");

	return rc;
}

#if defined(CONFIG_ISX012) && defined(CONFIG_SR130PC20)
static int sr130pc20_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int err = 0;
	int rc = 0;
	int temp = 0;

	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
#ifdef CONFIG_LOAD_FILE
	sr130pc20_regs_table_init();
#endif

	CAM_DEBUG("=== Start ===");

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		cam_err(" request gpio failed");

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
	data->sensor_platform_info->sensor_power_on(1);

	/*standy VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);

	usleep(10);

#if defined(CONFIG_MACH_EXPRESS)
	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 1,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
#else
	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 2,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
#endif
	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0)
		cam_err(" Mclk enable failed");


	if (rc != 0)
		goto FAIL_END;

	usleep(12000);

	/*reset VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("check VT reset : %d", temp);
	usleep(1500);
	sr130pc20_set_init_mode();

	config_csi2 = 0;

	sr130pc20_ctrl->op_mode = CAMERA_MODE_INIT;

	err = sr130pc20_WRT_LIST(sr130pc20_i2c_check);
	if (err == -EIO) {
		cam_err("[sr130pc20] start1 fail!\n");
		return -EIO;
		}

FAIL_END:
	if (rc) {
		cam_err("Power up Failed!!");
		msm_camera_request_gpio_table(data, 0);
	} else {
		cam_err(" X");
		}

	return rc;
}
#elif defined(CONFIG_S5K5CCGX) && defined(CONFIG_SR130PC20)
static int sr130pc20_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int err = 0;
	int rc = 0;
	int temp = 0;

	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
#ifdef CONFIG_LOAD_FILE
	sr130pc20_regs_table_init();
#endif

	CAM_DEBUG("=== Start ===");
	capture_mode = 0;//reset capture mode status, pro paint camera is not working due to this

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		cam_err(" request gpio failed");

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
	data->sensor_platform_info->sensor_power_on(1);

	/*standy VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);

	usleep(10);


	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 2,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_6MA),
		GPIO_CFG_ENABLE);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0)
		cam_err(" Mclk enable failed");


	if (rc != 0)
		goto FAIL_END;

	usleep(12000);

	/*reset VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("check VT reset : %d", temp);
	usleep(1500);

	/*3M STBY*/
	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("CAM_3M_ISP_INIT : %d", temp);

	/*3M Reset*/
	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("CAM_3M_RST : %d", temp);

	sr130pc20_set_init_mode();

	config_csi2 = 0;

	sr130pc20_ctrl->op_mode = CAMERA_MODE_INIT;

	err = sr130pc20_WRT_LIST(sr130pc20_i2c_check);
	if (err == -EIO) {
		cam_err("[sr130pc20] start1 fail!\n");
		msm_camera_request_gpio_table(data, 0);
		return -EIO;
		}

FAIL_END:
	if (rc) {
		cam_err("Power up Failed!!");
		msm_camera_request_gpio_table(data, 0);
	} else {
		cam_err(" X");
		}

	return rc;
}
#elif defined(CONFIG_SR352) && defined(CONFIG_SR130PC20)
static int sr130pc20_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{

	int err = 0;
	int rc = 0;
	int temp = 0;

	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
#ifdef CONFIG_LOAD_FILE
	sr130pc20_regs_table_init();
#endif

	CAM_DEBUG("=== Start SR352  ===");

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		cam_err(" request gpio failed");

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
	data->sensor_platform_info->sensor_power_on(1);

	mdelay(2);

#if defined(CONFIG_MACH_EXPRESS)
	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 1,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
#else
	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 2,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
#endif
	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0)
		cam_err(" Mclk enable failed");


	if (rc != 0)
		goto FAIL_END;

	
	/*standy VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);

	
	msleep(30);

	/*reset VT */
	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 1);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);
	CAM_DEBUG("check VT reset : %d", temp);
	usleep(1500);
	
	sr130pc20_set_init_mode();

	config_csi2 = 0;

	sr130pc20_ctrl->op_mode = CAMERA_MODE_INIT;

	err = sr130pc20_WRT_LIST(sr130pc20_i2c_check);
	if (err == -EIO) {
		cam_err("[sr130pc20] start1 fail!\n");
		return -EIO;
		}

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
static int sr130pc20_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	printk(KERN_DEBUG "sr130pc20_sensor_power_up");
}
#endif

static void sr130pc20_set_ev(int ev)
{
	CAM_DEBUG("[sr130pc20] %s : %d", __func__, ev);

	switch (ev) {
	case CAMERA_EV_M4:
		sr130pc20_WRT_LIST(sr130pc20_brightness_M4);
		break;

	case CAMERA_EV_M3:
		sr130pc20_WRT_LIST(sr130pc20_brightness_M3);
		break;

	case CAMERA_EV_M2:
		sr130pc20_WRT_LIST(sr130pc20_brightness_M2);
		break;

	case CAMERA_EV_M1:
		sr130pc20_WRT_LIST(sr130pc20_brightness_M1);
		break;

	case CAMERA_EV_DEFAULT:
		sr130pc20_WRT_LIST(sr130pc20_brightness_default);
		break;

	case CAMERA_EV_P1:
		sr130pc20_WRT_LIST(sr130pc20_brightness_P1);
		break;

	case CAMERA_EV_P2:
		sr130pc20_WRT_LIST(sr130pc20_brightness_P2);
		break;

	case CAMERA_EV_P3:
		sr130pc20_WRT_LIST(sr130pc20_brightness_P3);
		break;

	case CAMERA_EV_P4:
		sr130pc20_WRT_LIST(sr130pc20_brightness_P4);
		break;

	default:
		CAM_DEBUG("[sr130pc20] unexpected ev mode %s/%d",
			__func__, __LINE__);
		break;
	}

	sr130pc20_ctrl->settings.brightness = ev;
}

static void sr130pc20_set_flip(int flip)
{
	unsigned char r_data, checkSubSampling = 0;
	CAM_DEBUG("flip : %d", flip);

	sr130pc20_i2c_write_16bit(0xFF87);
	sr130pc20_i2c_read(0xD5, &r_data);
	if (r_data & 0x02)
		checkSubSampling = 1;
	switch (flip) {
	case 0:
		if (sr130pc20_ctrl->cam_mode == MOVIE_MODE) {
			sr130pc20_WRT_LIST(sr130pc20_flip_off_No15fps);
		} else {					/* for 15fps */
			sr130pc20_WRT_LIST(sr130pc20_flip_off);
		}
		break;

	case 1:
		if (sr130pc20_ctrl->cam_mode == MOVIE_MODE) {
			sr130pc20_WRT_LIST(sr130pc20_hflip_No15fps);
		} else {					/* for 15fps */
			sr130pc20_WRT_LIST(sr130pc20_hflip);
		}
		break;

	default:
		CAM_DEBUG("flip : default");
		break;
	}
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
		sr130pc20_set_ev(ctrl_info.value_1);
		break;

	case  EXT_CAM_MOVIE_MODE:
		/*CAM_DEBUG("MOVIE mode : %d", ctrl_info.value_1);*/
		sr130pc20_ctrl->cam_mode = ctrl_info.value_1;
		break;

	case EXT_CAM_EXIF:
		ctrl_info.value_1 = sr130pc20_get_exif(ctrl_info.address,
			ctrl_info.value_2);
		break;
	case EXT_CAM_VT_MODE:
		/*CAM_DEBUG(" VT mode : %d", ctrl_info.value_1);*/
		sr130pc20_ctrl->vtcall_mode = ctrl_info.value_1;
		break;
	case EXT_CAM_SET_FLIP:
		/*CAM_DEBUG(" Dhana- FLIP mode : %d", ctrl_info.value_1);*/
		sr130pc20_set_flip(ctrl_info.value_1);
		sr130pc20_ctrl->mirror_mode = ctrl_info.value_1;
		break;
	case EXT_CAM_EFFECT:
		effect_mode = ctrl_info.value_1;
		sr130pc20_set_effect(ctrl_info.value_1);
		break;

	case EXT_CAM_WB:
		wb_mode = ctrl_info.value_1;
		sr130pc20_set_whitebalance(ctrl_info.value_1);
		break;
/*
	case EXT_CAM_DTP_TEST:
		sr130pc20_check_dataline(ctrl_info.value_1);
		break;
*/
	case EXT_CAM_PREVIEW_SIZE:
		sr130pc20_set_preview_size(ctrl_info.value_1);
		break;
	default:

		CAM_DEBUG("[sr130pc20] default mode");
		break;
	}
	if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
		(const void *)&ctrl_info,
			sizeof(ctrl_info))) {
		cam_err("fail copy_to_user!");
	}

}

long sr130pc20_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	void __user *argp = (void __user *)arg;
	struct msm_sensor_ctrl_t *sr130pc20_s_ctrl = get_sctrl(sd);

	CAM_DEBUG("sr130pc20_sensor_subdev_ioctl\n");
	CAM_DEBUG("%s: cmd %d\n", __func__, _IOC_NR(cmd));
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return sr130pc20_sensor_config(sr130pc20_s_ctrl, argp);
	/*case VIDIOC_MSM_SENSOR_RELEASE:*/
	/*	return msm_sensor_release(sr130pc20_s_ctrl);*/
	case VIDIOC_MSM_SENSOR_CSID_INFO:
		{
		struct msm_sensor_csi_info *csi_info =
			(struct msm_sensor_csi_info *)arg;
		/*sr130pc20_s_ctrl->csid_version = csi_info->csid_version;*/
		sr130pc20_s_ctrl->is_csic = csi_info->is_csic;
		return 0;
		}
	default:
		return -ENOIOCTLCMD;
	}

}

int sr130pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data, (void *)argp,
						sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	CAM_DEBUG(" cfgtype = %d, mode = %d",
			cfg_data.cfgtype, cfg_data.mode);
/*
		switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			rc = sr130pc20_set_sensor_mode(cfg_data.mode);
			break;

		case CFG_GET_AF_MAX_STEPS:
		default:
			rc = 0;
			cam_err(" Invalid cfgtype = %d", cfg_data.cfgtype);
			break;
		}
*/
	switch (cfg_data.cfgtype) {
	case CFG_SENSOR_INIT:
		if (config_csi2 == 0)
			rc = sr130pc20_sensor_setting(UPDATE_PERIODIC,
				RES_PREVIEW);
		break;
	case CFG_SET_MODE:
		rc = sr130pc20_set_sensor_mode(cfg_data.mode);
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
				CAM_DEBUG(" RAJ 2.1 cfgtype = %d, mode = %d\n",
			cfg_data.cfgtype, cfg_data.mode);
				rc = -EFAULT;
				break;
			}

			rc = s_ctrl->func_tbl->sensor_get_csi_params(
				s_ctrl,
				&cfg_data.cfg.csi_lane_params);
			CAM_DEBUG("RAJ2 :: Inside CFG_GET_CSI_PARAMS");
		if (copy_to_user((void *)argp,
				 (const void *)&cfg_data,
				sizeof(cfg_data)))
				rc = -EFAULT;

			break;

	case CFG_GET_AF_MAX_STEPS:
	default:
		rc = 0;
		break;
	}

	return rc;
}

static int sr130pc20_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int temp = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	CAM_DEBUG("=== POWER DOWN Start ===");

	usleep(1000);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_reset);

	usleep(1200); /* 20clk = 0.833us */

	/*CAM_MCLK0*/
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	usleep(10);

	gpio_set_value_cansleep(data->sensor_platform_info->vt_sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->vt_sensor_stby);
	CAM_DEBUG("check VT standby : %d", temp);

	usleep(10);

	data->sensor_platform_info->sensor_power_off(1);

	msm_camera_request_gpio_table(data, 0);

#ifdef CONFIG_LOAD_FILE
	sr130pc20_regs_table_exit();
#endif
	return rc;
}

static struct v4l2_subdev_info sr130pc20_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static int sr130pc20_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code) {
	CAM_DEBUG("Index is %d", index);
	if ((unsigned int)index >= ARRAY_SIZE(sr130pc20_subdev_info))
		return -EINVAL;

	*code = sr130pc20_subdev_info[index].code;
	return 0;
}

void sr130pc20_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	CAM_DEBUG("start_stream");
	stop_stream = 0;

	if (sr130pc20_ctrl->op_mode == CAMERA_MODE_CAPTURE)
		return ;

	if (sr130pc20_ctrl->cam_mode == MOVIE_MODE) {
		CAM_DEBUG("VGA recording");
		if (sr130pc20_ctrl->op_mode == CAMERA_MODE_INIT ||
			sr130pc20_ctrl->op_mode == CAMERA_MODE_RECORDING ||
			sr130pc20_ctrl->op_mode == CAMERA_MODE_PREVIEW) {
			sr130pc20_WRT_LIST(sr130pc20_25fix_camcorder_Reg);

			sr130pc20_set_effect(effect_mode);
			sr130pc20_set_whitebalance(wb_mode);
			sr130pc20_set_ev(sr130pc20_ctrl->settings.brightness);
/*			sr130pc20_set_ev(sr130pc20_ctrl->settings.brightness);
			if (sr130pc20_ctrl->mirror_mode == 1)
					sr130pc20_set_flip( \
					sr130pc20_ctrl->mirror_mode);*/
			}
		} else {
		CAM_DEBUG("Camera Mode");
	if (capture_mode == 0){  //after capture when getting back to Preview not to set Init Reg.
	if (sr130pc20_ctrl->op_mode == CAMERA_MODE_INIT ||
		sr130pc20_ctrl->op_mode == CAMERA_MODE_RECORDING ||
		sr130pc20_ctrl->op_mode == CAMERA_MODE_PREVIEW) {
		sr130pc20_WRT_LIST(sr130pc20_Init_Reg);
		CAM_DEBUG("sr130pc20 Common Registers written\n");

		sr130pc20_set_effect(effect_mode);
		sr130pc20_set_whitebalance(wb_mode);
		sr130pc20_set_ev(sr130pc20_ctrl->settings.brightness);
		}
		}
		CAM_DEBUG("CAPTURE_MODE22 : %d", capture_mode);
		CAM_DEBUG("MIPI TIMING : 0x1d0e");
		sr130pc20_WRT_LIST(sr130pc20_Preview);
		capture_mode = 0;
		}
	//sr130pc20_set_ev(sr130pc20_ctrl->settings.brightness);
	if (sr130pc20_ctrl->mirror_mode == 1)
			sr130pc20_set_flip(sr130pc20_ctrl->mirror_mode);
}

void sr130pc20_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	if (sr130pc20_ctrl->op_mode == CAMERA_MODE_CAPTURE)
		return ;

	stop_stream = 1;
	CAM_DEBUG(" sr130pc20_sensor_stop_stream E");
	sr130pc20_WRT_LIST(sr130pc20_stop_stream);
	CAM_DEBUG(" sr130pc20_sensor_stop_stream X");
}

static struct v4l2_subdev_core_ops sr130pc20_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = sr130pc20_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops sr130pc20_subdev_video_ops = {
	.enum_mbus_fmt = sr130pc20_enum_fmt,
};

static struct v4l2_subdev_ops sr130pc20_subdev_ops = {
	.core = &sr130pc20_subdev_core_ops,
	.video  = &sr130pc20_subdev_video_ops,
};

static int sr130pc20_i2c_probe(struct i2c_client *client,
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

	sr130pc20_client = client;
	sr130pc20_dev = s_ctrl->sensor_i2c_client->client->dev;

	sr130pc20_ctrl = kzalloc(sizeof(struct sr130pc20_ctrl), GFP_KERNEL);
	if (!sr130pc20_ctrl) {
		CAM_DEBUG("sr130pc20_ctrl alloc failed!\n");
		return -ENOMEM;
	}

	sr130pc20_exif = kzalloc(sizeof(struct sr130pc20_exif_data),
		GFP_KERNEL);
	if (!sr130pc20_exif) {
		cam_err("Cannot allocate memory fo EXIF structure!");
		kfree(sr130pc20_exif);
		rc = -ENOMEM;
	}

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);

	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		&sr130pc20_subdev_ops);

	sr130pc20_ctrl->s_ctrl = s_ctrl;
	sr130pc20_ctrl->sensor_dev = &s_ctrl->sensor_v4l2_subdev;
	sr130pc20_ctrl->sensordata = client->dev.platform_data;

	rc = msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);

	if (rc < 0) {
		cam_err("msm_sensor_register failed!");
		kfree(sr130pc20_exif);
		kfree(sr130pc20_ctrl);
		goto probe_failure;
		}

	cam_err("sr130pc20_probe succeeded!");
	return 0;

probe_failure:
	cam_err("sr130pc20_probe failed!");
	return rc;
}



static struct msm_sensor_id_info_t sr130pc20_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x0074,
};

static const struct i2c_device_id sr130pc20_i2c_id[] = {
	{"sr130pc20", (kernel_ulong_t)&sr130pc20_s_ctrl},
	{},
};

static struct msm_camera_i2c_client sr130pc20_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static struct i2c_driver sr130pc20_i2c_driver = {
	.id_table = sr130pc20_i2c_id,
	.probe  = sr130pc20_i2c_probe,
	.driver = {
		.name = "sr130pc20",
	},
};

static int __init sr130pc20_init(void)
{
	return i2c_add_driver(&sr130pc20_i2c_driver);
}

static struct msm_sensor_fn_t sr130pc20_func_tbl = {
	.sensor_config = sr130pc20_sensor_config,
	.sensor_power_up = sr130pc20_sensor_power_up,
	.sensor_power_down = sr130pc20_sensor_power_down,
	.sensor_match_id = sr130pc20_sensor_match_id,
	/*.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,*/
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_start_stream = sr130pc20_sensor_start_stream,
	.sensor_stop_stream = sr130pc20_sensor_stop_stream,
	/*.sensor_get_lens_info = sensor_get_lens_info,*/
};


static struct msm_sensor_reg_t sr130pc20_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_sensor_ctrl_t sr130pc20_s_ctrl = {
	.msm_sensor_reg = &sr130pc20_regs,
	.sensor_i2c_client = &sr130pc20_sensor_i2c_client,
#if defined(CONFIG_MACH_EXPRESS)
	.sensor_i2c_addr = 0x20,
#elif defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_LT02_CHN_CTC)
	.sensor_i2c_addr = 0x28,
#else
	.sensor_i2c_addr = 0x20,
#endif
	.sensor_id_info = &sr130pc20_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	/*.csic_params = &sr130pc20_csic_params_array[0],
	.csi_params = &sr130pc20_csi_params_array[0],*/
	.msm_sensor_mutex = &sr130pc20_mut,
	.sensor_i2c_driver = &sr130pc20_i2c_driver,
	.sensor_v4l2_subdev_info = sr130pc20_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sr130pc20_subdev_info),
	.sensor_v4l2_subdev_ops = &sr130pc20_subdev_ops,
	.func_tbl = &sr130pc20_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};


module_init(sr130pc20_init);
