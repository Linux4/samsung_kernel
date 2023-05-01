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
#include "sr200pc20m.h"

#include "msm.h"
#include "msm_ispif.h"
#include "msm_sensor.h"

#include "sr200pc20m.h"

#include <linux/gpio.h>
#include <mach/gpiomux.h>
#include <mach/msm8930-gpio.h>
#include <mach/socinfo.h>

#ifdef CONFIG_REGULATOR_LP8720
#include <linux/regulator/lp8720.h>
#endif
/*#define CONFIG_LOAD_FILE */

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
//static char *sr200pc20m_regs_table;
//static int sr200pc20m_regs_table_size;
//static int sr200pc20m_write_regs_from_sd(char *name);
//static int sr200pc20m_i2c_write_multi(unsigned short addr, unsigned int w_data);
struct test {
	u8 data;
	struct test *nextBuf;
};
static struct test *testBuf;
static s32 large_file;
#endif
static int sr200pc20m_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
static void sr200pc20m_set_ev(int ev);
static void sr200pc20m_set_flip(int flip);
DEFINE_MUTEX(sr200pc20m_mut);

#define sr200pc20m_WRT_LIST(A)	\
	sr200pc20m_i2c_wrt_list(A, (sizeof(A) / sizeof(A[0])), #A);

#define CAM_REV ((system_rev <= 1) ? 0 : 1)

#include "sr200pc20m_regs.h"

static unsigned int config_csi2;
static unsigned int stop_stream;
#if defined(CONFIG_MACH_CRATER_CHN_CTC)
static struct regulator *l35,*l36;
#else
static struct regulator *l29, *l32, *l34,*l35;
#endif
#ifdef CONFIG_LOAD_FILE

void sr200pc20m_regs_table_init(void)
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
	
	/*Get the current address space */
	mm_segment_t fs = get_fs();
      cam_err("CONFIG_LOAD_FILE is enable!!\n");
	CAM_DEBUG("%s %d", __func__, __LINE__);

	/*Set the current segment to kernel data segment */
	set_fs(get_ds());

	//fp = filp_open("/mnt/sdcard/sr200pc20m_regs.h", O_RDONLY, 0);
	fp = filp_open("/data/log//sr200pc20m_regs.h", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open /mnt/sdcard/sr200pc20m_regs.h");
		return ;// PTR_ERR(fp);
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

#if 1 //FOR_DEBUG /* for print */
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
	return;// ret;
}
static inline int sr200pc20m_write(struct i2c_client *client,
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

	msg.addr = sr200pc20m_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	do {
		err = i2c_transfer(sr200pc20m_client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d\n",
		       packet, err);
		usleep(3000);

	} while (retry_count <= 5);

	return -1;
}


static int sr200pc20m_write_regs_from_sd(char *name)
{
	struct test *tempData = NULL;

	int ret = -EAGAIN;
	u16 temp;//, temp_2;
	u16 delay = 0;
	u8 data[7];
	s32 searched = 0;
	size_t size = strlen(name);
	s32 i;
	/*msleep(10000);*/
	/*printk("E size = %d, string = %s\n", size, name);*/
	tempData = &testBuf[0];

	*(data + 6) = '\0';
CAM_DEBUG("reg name = %s\n", name);
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
				CAM_DEBUG("%s\n", data);
				CAM_DEBUG("kstrtoul data = 0x%x\n", temp);
				break;
			} else if (tempData->data == '}') {
				searched = 1;
				break;
			} else
				tempData = tempData->nextBuf;

			if (tempData->nextBuf == NULL)
				return  -1;//NULL;
		}

		if (searched)
			break;
		if ((temp & 0xFF00) == SR200PC20M_DELAY) {
			delay = temp & 0xFF;
			cam_info("SR200PC20M delay(%d)", delay);
			/*step is 10msec */
			msleep(delay);
			continue;
		}
		ret = sr200pc20m_write(sr200pc20m_client, temp);

		/* In error circumstances */
		/* Give second shot */
		if (unlikely(ret)) {
			ret = sr200pc20m_write(sr200pc20m_client, temp);

			/* Give it one more shot */
			if (unlikely(ret))
				ret = sr200pc20m_write(sr200pc20m_client, temp);
			}
		}

	return 0;
}

void sr200pc20m_regs_table_exit(void)
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

static DECLARE_WAIT_QUEUE_HEAD(sr200pc20m_wait_queue);

/**
 * sr200pc20m_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor
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
 * sr200pc20m_i2c_read: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static int sr200pc20m_i2c_read(unsigned char subaddr, unsigned char *data)
{
	unsigned char buf[1];
	struct i2c_msg msg = {sr200pc20m_client->addr, 0, 1, buf};

	int err = 0;
	buf[0] = subaddr;

	if (!sr200pc20m_client->adapter)
		return -EIO;

	err = i2c_transfer(sr200pc20m_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;

	msg.flags = I2C_M_RD;

	err = i2c_transfer(sr200pc20m_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;
	/*
	 * Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */

	*data = buf[0];

	return err;
}

#if 0//ifdef CONFIG_LOAD_FILE
/**
 * sr200pc20m_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int sr200pc20m_i2c_write_multi(unsigned short addr, unsigned int w_data)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (addr >> 8);
	buf[1] = (u8) (w_data & 0xff);

	msg.addr = sr200pc20m_client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = buf;


	cam_err("I2C CHIP ID=0x%x, DATA 0x%x 0x%x\n",
			sr200pc20m_client->addr, buf[0], buf[1]);


	do {
		rc = i2c_transfer(sr200pc20m_client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		cam_err("retry_count %d\n", retry_count);
		usleep(3000);

	} while (retry_count <= 5);

	return 0;
}
#endif

static int32_t sr200pc20m_i2c_write_16bit(u16 packet)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (packet >> 8);
	buf[1] = (u8) (packet & 0xff);

	msg.addr = sr200pc20m_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

#if defined(CAM_I2C_DEBUG)
	cam_err("I2C CHIP ID=0x%x, DATA 0x%x 0x%x\n",
			sr200pc20m_client->addr, buf[0], buf[1]);
#endif

	do {
		rc = i2c_transfer(sr200pc20m_client->adapter, &msg, 1);
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

static int sr200pc20m_i2c_wrt_list(const u16 *regs,
	int size, char *name)
{
#ifdef CONFIG_LOAD_FILE
	sr200pc20m_write_regs_from_sd(name);
#else

	int i;
	u8 m_delay = 0;

	u16 temp_packet;


	CAM_DEBUG("%s, size=%d \n", name, size);
	for (i = 0; i < size; i++) {
		temp_packet = regs[i];

		if ((temp_packet & SR200PC20M_DELAY) == SR200PC20M_DELAY) {
			m_delay = temp_packet & 0xFF;
			cam_info("delay = %d", m_delay*10);
			msleep(m_delay*10);/*step is 10msec*/
			continue;
		}

		if (sr200pc20m_i2c_write_16bit(temp_packet) < 0) {
			cam_err("fail(0x%x, 0x%x:%d)",
					sr200pc20m_client->addr, temp_packet, i);
			return -EIO;
		}
		/*udelay(10);*/
	}
#endif

	return 0;
}

static int sr200pc20m_set_exif(void)
{
	int err = 0;
	u8 read_value1 = 0;
	u8 read_value2 = 0;
	u8 read_value3 = 0;
	u8 read_value4 = 0;
	unsigned short gain_value = 0;

	sr200pc20m_i2c_write_16bit(0x0320);
	sr200pc20m_i2c_read(0x80, &read_value1);
	sr200pc20m_i2c_read(0x81, &read_value2);
	sr200pc20m_i2c_read(0x82, &read_value3);
	sr200pc20m_exif->shutterspeed = 24000000 / ((read_value1 << 19)
		+ (read_value2 << 11) + (read_value3 << 3));

	CAM_DEBUG("Exposure time = %d\n", sr200pc20m_exif->shutterspeed);
	sr200pc20m_i2c_write_16bit(0x0320);
	sr200pc20m_i2c_read(0xb0, &read_value4);
	gain_value = (read_value4 / 16) * 1000;

	if (gain_value < 875)
		sr200pc20m_exif->iso = 50;
	else if (gain_value < 1750)
		sr200pc20m_exif->iso = 100;
	else if (gain_value < 4625)
		sr200pc20m_exif->iso = 200;
	else
		sr200pc20m_exif->iso = 400;

	CAM_DEBUG("ISO = %d\n", sr200pc20m_exif->iso);
	return err;
}


static int sr200pc20m_get_exif(int exif_cmd, unsigned short value2)
{
	unsigned short val = 0;

	switch (exif_cmd) {
	case EXIF_SHUTTERSPEED:
		val = sr200pc20m_exif->shutterspeed;
		break;

	case EXIF_ISO:
		val = sr200pc20m_exif->iso;
		break;

	default:
		break;
	}

	return val;
}

static void sr200pc20m_set_init_mode(void)
{
	config_csi2 = 0;
	sr200pc20m_ctrl->cam_mode = PREVIEW_MODE;
	sr200pc20m_ctrl->op_mode = CAMERA_MODE_INIT;
	sr200pc20m_ctrl->mirror_mode = 0;
	sr200pc20m_ctrl->vtcall_mode = 0;
	sr200pc20m_ctrl->settings.preview_size_idx = 0;
	stop_stream = 1;
}
void sr200pc20m_set_preview_size(int32_t index)
{
	CAM_DEBUG("index %d", index);

	sr200pc20m_ctrl->settings.preview_size_idx = index;
}

/* Supporting effects and WB-Start */
static int sr200pc20m_set_effect(int effect)
{
	CAM_DEBUG(" %d", effect);

	if((sr200pc20m_ctrl->op_mode == CAMERA_MODE_INIT) && (stop_stream == 1))
	{
		sr200pc20m_ctrl->settings.effect = effect;
		return 0;
	}

	switch (effect) {
	case CAMERA_EFFECT_OFF:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Normal);
		break;

	case CAMERA_EFFECT_MONO:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Gray);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Negative);
		break;

	case CAMERA_EFFECT_SEPIA:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Sepia);
		break;
		
	case CAMERA_EFFECT_VINTAGE_COLD:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Vintage_Cold);
		break;
		
	case CAMERA_EFFECT_VINTAGE_WARM:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Vintage_Warm);
		break;
		
	case CAMERA_EFFECT_POSTERIZE:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Posterize);	
		break;
				
	case CAMERA_EFFECT_SOLARIZE:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Solarize);
		break;
				
	case CAMERA_EFFECT_WASHED:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Washed);
		break;
				
	case CAMERA_EFFECT_POINT_COLOR_1:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Color1);
		break;
				
	case CAMERA_EFFECT_POINT_COLOR_2:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Color2);	
		break;
		
	case CAMERA_EFFECT_POINT_COLOR_3:
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Color3);
		break;		
		
	default:
		CAM_DEBUG(" effect : default");
		sr200pc20m_WRT_LIST(sr200pc20_Effect_Normal);
		return 0;
	}

	sr200pc20m_ctrl->settings.effect = effect;

	return 0;
}


void sr200pc20m_set_whitebalance(int wb)
{
	CAM_DEBUG("sr200pc20m_set_whitebalance %d", wb);

	if((sr200pc20m_ctrl->op_mode == CAMERA_MODE_INIT) && (stop_stream == 1))
	{
		sr200pc20m_ctrl->settings.wb = wb;
		return;
	}

	switch (wb) {
	case CAMERA_WHITE_BALANCE_AUTO:
		sr200pc20m_WRT_LIST(sr200pc20_WB_Auto);
		break;

	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		sr200pc20m_WRT_LIST(sr200pc20_WB_Incandescent);
		break;

	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		sr200pc20m_WRT_LIST(sr200pc20_WB_Fluorescent);
		break;

	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		sr200pc20m_WRT_LIST(sr200pc20_WB_Daylight);
		break;

	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		sr200pc20m_WRT_LIST(sr200pc20_WB_Cloudy);
		break;

	default:
		CAM_DEBUG(" WB : default");
		return ;
	}

	sr200pc20m_ctrl->settings.wb = wb;

	return ;
}
/* Supporting effects and WB-End */
void sr200pc20m_set_preview(void)
{

	CAM_DEBUG("cam_mode = %d op_mode = %d", sr200pc20m_ctrl->cam_mode, sr200pc20m_ctrl->op_mode);

	if (sr200pc20m_ctrl->cam_mode == MOVIE_MODE)
	{
		CAM_DEBUG("Camcorder_Mode_ON");
		if (sr200pc20m_ctrl->settings.preview_size_idx == PREVIEW_SIZE_D1)
		{
              	CAM_DEBUG("720 480 recording chris.shen");
        		 //sr200pc20m_WRT_LIST(sr200pc20_D1_20fps);
		}
		else
		{
			CAM_DEBUG("VGA recording");
			if (sr200pc20m_ctrl->op_mode == CAMERA_MODE_INIT ||
				sr200pc20m_ctrl->op_mode == CAMERA_MODE_PREVIEW)
			{
				if (stop_stream == 0) {
					sr200pc20m_WRT_LIST(sr200pc20_Init_Reg);
					sr200pc20m_WRT_LIST(sr200pc20_20fps_60Hz);
				}

				if (sr200pc20m_ctrl->mirror_mode == 1)
					sr200pc20m_set_flip(sr200pc20m_ctrl->mirror_mode);
				
				if(sr200pc20m_ctrl->op_mode == CAMERA_MODE_PREVIEW)
				{
					sr200pc20m_WRT_LIST(sr200pc20_20fps);

					if(sr200pc20m_ctrl->settings.effect!= CAMERA_EFFECT_OFF)	
					{
						sr200pc20m_set_effect(sr200pc20m_ctrl->settings.effect);
					}
					else
					{
						sr200pc20m_set_effect(CAMERA_EFFECT_OFF);
					}
					
					//Songww 20130504 whitebalance not remain
					if(sr200pc20m_ctrl->settings.wb != CAMERA_WHITE_BALANCE_AUTO)
					{
						sr200pc20m_set_whitebalance(sr200pc20m_ctrl->settings.wb);
					}
					else
					{
						sr200pc20m_set_whitebalance(CAMERA_WHITE_BALANCE_AUTO);
					}
					
					if(sr200pc20m_ctrl->settings.brightness != CAMERA_EV_DEFAULT)
					{
						sr200pc20m_set_ev(sr200pc20m_ctrl->settings.brightness);
					}
					else
					{
						sr200pc20m_set_ev(CAMERA_EV_DEFAULT);
					}

					//sr200pc20m_WRT_LIST(sr200pc20_Preview);
				}
			}
		}
         	sr200pc20m_ctrl->op_mode = CAMERA_MODE_RECORDING;
	}
	else
	{

		CAM_DEBUG("Init_Mode");
		if (sr200pc20m_ctrl->op_mode == CAMERA_MODE_INIT)
		{
			if (stop_stream == 0)
			{
				if (sr200pc20m_ctrl->vtcall_mode == 1) {
					CAM_DEBUG(" VT common");
					sr200pc20m_WRT_LIST(sr200pc20_VT_Init_Reg);
				} else if (sr200pc20m_ctrl->vtcall_mode == 2) {
					CAM_DEBUG(" WIFI VT common");
                 		} else if (sr200pc20m_ctrl->settings.preview_size_idx == PREVIEW_SIZE_D1) {
			              CAM_DEBUG("720 480 recording");
			              sr200pc20m_WRT_LIST(sr200pc20_D1_20fps);
		             }else {
					sr200pc20m_WRT_LIST(sr200pc20_Init_Reg);
					CAM_DEBUG("Common Registers written\n");
				}
			}
			if (sr200pc20m_ctrl->mirror_mode == 1)
				sr200pc20m_set_flip(sr200pc20m_ctrl->mirror_mode);
		}
		else if(sr200pc20m_ctrl->op_mode == CAMERA_MODE_CAPTURE)
		{
			sr200pc20m_WRT_LIST(sr200pc20_Preview);
			sr200pc20m_ctrl->op_mode = CAMERA_MODE_PREVIEW; 
		}
		else if(sr200pc20m_ctrl->op_mode == CAMERA_MODE_RECORDING)
		{
			sr200pc20m_WRT_LIST(sr200pc20_Auto_fps);

			if(sr200pc20m_ctrl->settings.effect!= CAMERA_EFFECT_OFF)	
			{
				sr200pc20m_set_effect(sr200pc20m_ctrl->settings.effect);
			}
			else
			{
				sr200pc20m_set_effect(CAMERA_EFFECT_OFF);
			}
			
			//Songww 20130504 whitebalance not remain
			if(sr200pc20m_ctrl->settings.wb != CAMERA_WHITE_BALANCE_AUTO)
			{
				sr200pc20m_set_whitebalance(sr200pc20m_ctrl->settings.wb);
			}
			else
			{
				sr200pc20m_set_whitebalance(CAMERA_WHITE_BALANCE_AUTO);
			}
			
			if(sr200pc20m_ctrl->settings.brightness != CAMERA_EV_DEFAULT)
			{
				sr200pc20m_set_ev(sr200pc20m_ctrl->settings.brightness);
			}
			else
			{
				sr200pc20m_set_ev(CAMERA_EV_DEFAULT);
			}

			//sr200pc20m_WRT_LIST(sr200pc20_Preview);
			
			sr200pc20m_ctrl->op_mode = CAMERA_MODE_PREVIEW; 
		}
/*
		if (stop_stream == 0) 
		{
			sr200pc20m_WRT_LIST(sr200pc20_Preview);
			sr200pc20m_ctrl->op_mode = CAMERA_MODE_PREVIEW; 
			
			CAM_DEBUG("Preview Registers written\n");
		}
*/
		
	}
	/*sr200pc20m_set_ev(CAMERA_EV_DEFAULT);*/
	/*sr200pc20m_set_ev(sr200pc20m_ctrl->settings.brightness);*/

	return;

}

void sr200pc20m_set_capture(void)
{
	CAM_DEBUG("");
	sr200pc20m_ctrl->op_mode = CAMERA_MODE_CAPTURE;
	sr200pc20m_WRT_LIST(sr200pc20_Capture);
	sr200pc20m_set_exif();
}

static int32_t sr200pc20m_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0;

	CAM_DEBUG("Start");

	switch (update_type) {
	case REG_INIT:
		break;

	case UPDATE_PERIODIC:
		msm_sensor_enable_debugfs(sr200pc20m_ctrl->s_ctrl);
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			CAM_DEBUG("UPDATE_PERIODIC");

				if (sr200pc20m_ctrl->s_ctrl->sensordata->
					sensor_platform_info->
					csi_lane_params != NULL) {
					CAM_DEBUG(" lane_assign ="\
						" 0x%x",
						sr200pc20m_ctrl->s_ctrl->
						sensordata->
						sensor_platform_info->
						csi_lane_params->
						csi_lane_assign);

					CAM_DEBUG(" lane_mask ="\
						" 0x%x",
						sr200pc20m_ctrl->s_ctrl->
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
static int32_t sr200pc20m_video_config(int mode)
{
	int32_t	rc = 0;

	if (sr200pc20m_sensor_setting(UPDATE_PERIODIC, RES_PREVIEW) < 0)
		rc = -1;

	CAM_DEBUG("hoon rc : %d", rc);

	return rc;
}
*/
static long sr200pc20m_set_sensor_mode(int mode)
{
	CAM_DEBUG("%d", mode);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		sr200pc20m_ctrl->cam_mode = PREVIEW_MODE;
		sr200pc20m_set_preview();
		break;
	case SENSOR_VIDEO_MODE:
		sr200pc20m_ctrl->cam_mode = MOVIE_MODE;
		sr200pc20m_set_preview();
		
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		sr200pc20m_set_capture();
		break;
	default:
		return 0;
	}
	return 0;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static int sr200pc20m_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	cam_info(" Nothing");

	return rc;
}
#if defined(CONFIG_MACH_CRATERTD_CHN_3G)||defined(CONFIG_MACH_BAFFINVETD_CHN_3G)
static int sr200pc20m_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	unsigned char readdata = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		CAM_DEBUG("%s: request gpio failed", __func__);

	/*Power on the LDOs */
	if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917) {
		int ret = 0;
		/*Power off the LDOs */
		/* CAM_ISP_CORE_1P2 set to low*/
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_IO_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 0);
		ret = gpio_get_value(GPIO_CAM_IO_EN);
		if (!ret)
		cam_err("[sww CAM_CORE_EN::CAM_ISP_CORE_1P2::ret::%d]::Disable OK\n", ret);
		else
		cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2]::Disable Fail\n");
		//usleep(1*1000);

		/* MAIN CAM RESET  set to low */
		gpio_set_value_cansleep(GPIO_CAM1_RST_N, 0);
		ret = gpio_get_value(GPIO_CAM1_RST_N);
		if (!ret)
		cam_err("[CAM1_RST_N::ret::%d]::Disable OK\n", ret);
		else
		cam_err("[CAM1_RST_N]::Disable Fail\n");
		//usleep(1*1000);

             /*VT CAM STBY set to low*/
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM2_RST_N, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_VT_STBY, 0);
		ret = gpio_get_value(GPIO_VT_STBY);
		CAM_DEBUG("check VT standby : %d", ret);

		/* VT CAM RESET set to low*/
		gpio_set_value_cansleep(GPIO_CAM2_RST_N, 0);
		ret = gpio_get_value(GPIO_CAM2_RST_N);
		if (!ret)
		cam_err("[CAM2_RST_N::ret::%d]::Disable OK\n", ret);
		else
		cam_err("[CAM2_RST_N]::Disable Fail\n");
		
		//usleep(1*1000);
		/* CAM_ISP_CORE_1P2 */
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_IO_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 1);
		ret = gpio_get_value(GPIO_CAM_IO_EN);
		if (ret)
			cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2::ret::%d]::SET OK\n", ret);
		else
			cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2]::SET Fail\n");
		mdelay(5);

             //IO->AVDD->DVDD->VT STBY->MCLK->RST->I2C command
		//Vt IO 1.8
		l35 = regulator_get(NULL, "8917_l35");
		if(IS_ERR(l35))
			cam_err("[CAM_DVDD_1P8]::regulator_get l35 fail\n");
		ret = regulator_set_voltage(l35 , 1800000, 1800000);
		if (ret)
			cam_err("[CAM_DVDD_1P8]::error setting voltage\n");
		ret = regulator_enable(l35);
		if (ret)
			cam_err("[CAM_DVDD_1P8]::SET Fail\n");
		else
			cam_err("[CAM_DVDD_1P8]::SET OK\n");
		//msleep(2);
		usleep(1*1000);
		
		/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
		l32 = regulator_get(NULL, "8917_l32");
		ret = regulator_set_voltage(l32 , 2800000, 2800000);
		if (ret)
			cam_err("[CAM_SENSOR_A2P8]::error setting voltage\n");
		ret = regulator_enable(l32);
		if (ret)
			cam_err("[CAM_SENSOR_A2P8]::SET Fail\n");
		else
			cam_err("[CAM_SENSOR_A2P8]::SET OK\n");
		usleep(1*1000);

		/*Sensor VT IO 1.8V -  */
		l29 = regulator_get(NULL, "8921_l29");
		ret = regulator_set_voltage(l29 , 1800000, 1800000);
		if (ret)
			cam_err("[CAM_DVDD_1P8]::error setting voltage\n");
		ret = regulator_enable(l29);
		if (!ret)
			cam_err("[CAM_DVDD_1P8]::SET OK\n");
		else
			cam_err("[CAM_DVDD_1P8]::SET Fail\n");
		msleep(2);

		/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
		l34 = regulator_get(NULL, "8917_l34");
		ret = regulator_set_voltage(l34 , 1800000, 1800000);
		if (ret)
			cam_err("[CAM_SENSOR_IO_1P8]::error setting voltage\n");
		ret = regulator_enable(l34);
		if (ret)
			cam_err("[CAM_SENSOR_IO_1P8]::SET Fail\n");
		else
			cam_err("[CAM_SENSOR_IO_1P8]::SET OK\n");
		mdelay(5);	
		
		//VT standby
		gpio_tlmm_config(GPIO_CFG(GPIO_VT_STBY, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_VT_STBY, 1);
		ret = gpio_get_value(GPIO_VT_STBY );
		cam_err("check VT standby ccc: %d", ret);
		msleep(30);
			  
		/*Set Sub clock */
		gpio_tlmm_config(GPIO_CFG(GPIO_SUB_CAM_MCLK, 2,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		if (s_ctrl->clk_rate != 0)
			cam_clk_info->clk_rate = s_ctrl->clk_rate;
		cam_err("check s_ctrl->clk_rate ccc: %ld", s_ctrl->clk_rate );
		rc = msm_cam_clk_enable(dev, cam_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
		if (rc < 0)
			cam_err("[CAM_MCLK0]::SET Fail\n");
		usleep(1*1000);
		if (rc != 0)
		goto FAIL_END;
		
		/* CAM2_RST_N */
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM2_RST_N, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_CAM2_RST_N, 1);
		ret = gpio_get_value(GPIO_CAM2_RST_N);
		if (ret)
			cam_err("[CAM2_RST_N::ret::%d]::Set OK\n", ret);
		else
			cam_err("[CAM2_RST_N]::Set Fail\n");
		cam_err("check RST ccc: %d", ret);
		//usleep(1*1000);
		msleep(15);


		if (data->sensor_platform_info->i2c_conf &&
			data->sensor_platform_info->i2c_conf->use_i2c_mux)
			msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

		sr200pc20m_set_init_mode();

		config_csi2 = 0;

		sr200pc20m_ctrl->op_mode = CAMERA_MODE_INIT;
			/* VT RESET */
		sr200pc20m_i2c_read(0x04, &readdata);
		cam_err("[sr200pc20m] read sensor ID :0x%xl!\n", readdata);

#ifdef CONFIG_LOAD_FILE
       	sr200pc20m_regs_table_init();
#endif
		//sr200pc20m_WRT_LIST(sr200pc20_Init_Reg);
		/*
	ret = sr200pc20m_WRT_LIST(sr200pc20_i2c_check);
	if (ret == -EIO) {
		cam_err("[sr200pc20m] start1 fail!\n");
		goto FAIL_END;
		}
		usleep(3*1000);
*/
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

#else // for craterTD and baffin 0.3 hw
static int sr200pc20m_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	unsigned char readdata = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;

	//cam_err(" sr200pc20m_sensor_match_id Nothing 11chris.shen : s_ctrl->sensor_device_type:%d", s_ctrl->sensor_device_type);
	//sr200pc20m_client = (struct i2c_client *)s_ctrl->sensor_i2c_client->client;
	//cam_err(" sr200pc20m_sensor_match_id Nothing 222 chris.shen :0x%x", sr200pc20m_client->addr);
	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0)
		CAM_DEBUG("%s: request gpio failed", __func__);

	/*Power on the LDOs */
	if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917) {
#if 1
		int ret = 0;
#if defined(CONFIG_MACH_CRATER_CHN_CTC)
	/*Power on the LDOs */

	      gpio_tlmm_config(GPIO_CFG(GPIO_CAM_SENSOR_EN, 0,
					GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);

		gpio_tlmm_config(GPIO_CFG(GPIO_VT_STBY, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM2_RST_N, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);

		gpio_set_value_cansleep(GPIO_CAM_SENSOR_EN, 0);
		gpio_set_value_cansleep(GPIO_VT_STBY, 0);
		gpio_set_value_cansleep(GPIO_CAM2_RST_N, 0);
#else
		/* CAM_ISP_CORE_1P2 */
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_IO_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 0);
		ret = gpio_get_value(GPIO_CAM_IO_EN);
		if (!ret)
		cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2::ret::%d]::Disable OK\n", ret);
		else
		cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2]::Disable Fail\n");
		usleep(1*1000);

		/* MAIN CAM RESET */
		gpio_set_value_cansleep(GPIO_CAM1_RST_N, 0);
		ret = gpio_get_value(GPIO_CAM1_RST_N);
		if (!ret)
		cam_err("[CAM1_RST_N::ret::%d]::Disable OK\n", ret);
		else
		cam_err("[CAM1_RST_N]::Disable Fail\n");
		usleep(1*1000);

		gpio_set_value_cansleep(GPIO_VT_STBY, 0);
		ret = gpio_get_value(GPIO_VT_STBY);
		CAM_DEBUG("check VT standby : %d", ret);

		gpio_tlmm_config(GPIO_CFG(GPIO_CAM2_RST_N, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		/* VT CAM RESET */
		gpio_set_value_cansleep(GPIO_CAM2_RST_N, 0);
		ret = gpio_get_value(GPIO_CAM2_RST_N);
		if (!ret)
		cam_err("[CAM2_RST_N::ret::%d]::Disable OK\n", ret);
		else
		cam_err("[CAM2_RST_N]::Disable Fail\n");
		usleep(1*1000);
#if 1
		/* CAM_ISP_CORE_1P2 */
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_IO_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 1);
		ret = gpio_get_value(GPIO_CAM_IO_EN);
		if (ret)
			cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2::ret::%d]::SET OK\n", ret);
		else
			cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2]::SET Fail\n");
		mdelay(5);
#endif
#endif
#endif
#if defined (CONFIG_MACH_CRATER_CHN_CTC)

             //IO->AVDD->DVDD->VT STBY->MCLK->RST->I2C command
		//IO 1.8
		l36 = regulator_get(NULL, "8917_l36");
		if(IS_ERR(l36))
			cam_err("[CAM_IO_1P8]::regulator_get l36 fail\n");
		ret = regulator_set_voltage(l36 , 1800000, 1800000);
		if (ret)
			cam_err("[CAM_IO_1P8]::error setting voltage\n");
		ret = regulator_enable(l36);
		if (ret)
			cam_err("[CAM_IO_1P8]::SET Fail\n");
		else
			cam_err("[CAM_IO_1P8]::SET OK\n");
               msleep(2);

		//AVDD 2.8
		gpio_set_value_cansleep(GPIO_CAM_SENSOR_EN, 1);
		ret = gpio_get_value(GPIO_CAM_SENSOR_EN);
		if (ret)
			cam_err("[CAM2_AVDD_2P8::ret::%d]::Set OK\n", ret);
		else
			cam_err("[CAM2_AVDD_2P8]::Set Fail\n");
	      msleep(2);

		//DVDD 1.8
		l35 = regulator_get(NULL, "8917_l35");
		if(IS_ERR(l35))
			cam_err("[CAM_DVDD_1P8]::regulator_get l35 fail\n");
		ret = regulator_set_voltage(l35 , 1800000, 1800000);
		if (ret)
			cam_err("[CAM_DVDD_1P8]::error setting voltage\n");
		ret = regulator_enable(l35);
		if (ret)
			cam_err("[CAM_DVDD_1P8]::SET Fail\n");
		else
			cam_err("[CAM_DVDD_1P8]::SET OK\n");
              msleep(5);


#else
		/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
		l32 = regulator_get(NULL, "8917_l32");
		ret = regulator_set_voltage(l32 , 2800000, 2800000);
		if (ret)
			cam_err("[CAM_SENSOR_A2P8]::error setting voltage\n");
		ret = regulator_enable(l32);
		if (ret)
			cam_err("[CAM_SENSOR_A2P8]::SET Fail\n");
		else
			cam_err("[CAM_SENSOR_A2P8]::SET OK\n");
		usleep(1*1000);

		gpio_tlmm_config(GPIO_CFG(GPIO_VT_STBY, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);

		gpio_set_value_cansleep(GPIO_VT_STBY, 1);
		ret = gpio_get_value(GPIO_VT_STBY );
		cam_err("check VT standby ccc: %d", ret);
		msleep(30);

		l35 = regulator_get(NULL, "8917_l35");
			ret = regulator_set_voltage(l35 , 1800000, 1800000);
			if (ret)
				cam_err("[VT_CAM_IO1.8]::error chris setting voltage\n");
			ret = regulator_enable(l35);
			if (ret)
				cam_err("[VT_CAM_IO1.8]::error enabling regulator\n");
			else
				cam_err("[VT_CAM_IO1.8]::SET OK\n");
		usleep(1*1000);

		/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
		l29 = regulator_get(NULL, "8921_l29");
		ret = regulator_set_voltage(l29 , 1800000, 1800000);
		if (ret)
			cam_err("[CAM_DVDD_1P8]::error setting voltage\n");
		ret = regulator_enable(l29);
		if (!ret)
			cam_err("[CAM_DVDD_1P8]::SET OK\n");
		else
			cam_err("[CAM_DVDD_1P8]::SET Fail\n");
		usleep(1*1000);

		/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
		l34 = regulator_get(NULL, "8917_l34");
		ret = regulator_set_voltage(l34 , 1800000, 1800000);
		if (ret)
			cam_err("[CAM_SENSOR_IO_1P8]::error setting voltage\n");
		ret = regulator_enable(l34);
		if (ret)
			cam_err("[CAM_SENSOR_IO_1P8]::SET Fail\n");
		else
			cam_err("[CAM_SENSOR_IO_1P8]::SET OK\n");
		mdelay(5);
#endif

#if defined(CONFIG_MACH_CRATER_CHN_CTC)
		/* VT STBY */
		gpio_set_value_cansleep(GPIO_VT_STBY, 1);
		ret = gpio_get_value(GPIO_VT_STBY );
		cam_err("check VT standby ccc: %d", ret);
		if (ret)
			cam_err("[CAM2_VT_STBY::ret::%d]::Set OK\n", ret);
		else
			cam_err("[CAM2_VT_STBY]::Set Fail\n");
		msleep(5);
#endif
	/*Set Sub clock */
		gpio_tlmm_config(GPIO_CFG(GPIO_SUB_CAM_MCLK, 2,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		if (s_ctrl->clk_rate != 0)
			cam_clk_info->clk_rate = s_ctrl->clk_rate;
		cam_err("check s_ctrl->clk_rate ccc: %ld", s_ctrl->clk_rate );
		rc = msm_cam_clk_enable(dev, cam_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
		if (rc < 0)
			cam_err("[CAM_MCLK0]::SET Fail\n");
#if defined(CONFIG_MACH_CRATER_CHN_CTC)
              msleep(50);
#else
		usleep(1*1000);
#endif

	if (rc != 0)
		goto FAIL_END;
#if !defined(CONFIG_MACH_CRATER_CHN_CTC)

		/* CAM2_RST_N */
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM2_RST_N, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif
	gpio_set_value_cansleep(GPIO_CAM2_RST_N, 1);
		ret = gpio_get_value(GPIO_CAM2_RST_N);
		if (ret)
			cam_err("[CAM2_RST_N::ret::%d]::Set OK\n", ret);
		else
			cam_err("[CAM2_RST_N]::Set Fail\n");
		cam_err("check RST ccc: %d", ret);
		usleep(1*1000);
		msleep(15);


	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

	sr200pc20m_set_init_mode();

	config_csi2 = 0;

	sr200pc20m_ctrl->op_mode = CAMERA_MODE_INIT;
		/* VT RESET */
	sr200pc20m_i2c_read(0x04, &readdata);
	cam_err("[sr200pc20m] read sensor ID :0x%xl!\n", readdata);

#ifdef CONFIG_LOAD_FILE
       sr200pc20m_regs_table_init();
#endif
		//sr200pc20m_WRT_LIST(sr200pc20_Init_Reg);
		/*
	ret = sr200pc20m_WRT_LIST(sr200pc20_i2c_check);
	if (ret == -EIO) {
		cam_err("[sr200pc20m] start1 fail!\n");
		goto FAIL_END;
		}
		usleep(3*1000);
*/
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
#endif

static void sr200pc20m_set_contrast(int contrast)
{
	CAM_DEBUG("[sr200pc20m] %s : %d", __func__, contrast);
	switch (contrast) {
	case CAMERA_CONTRAST_LV0:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_M4);
		break;

	case CAMERA_CONTRAST_LV1:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_M3);
		break;

	case CAMERA_CONTRAST_LV2:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_M2);
		break;

	case CAMERA_CONTRAST_LV3:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_M1);
		break;

	case CAMERA_CONTRAST_LV4://default
		sr200pc20m_WRT_LIST(sr200pc20_contrast_default);
		break;

	case CAMERA_CONTRAST_LV5:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_P1);
		break;

	case CAMERA_CONTRAST_LV6:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_P2);
		break;

	case CAMERA_CONTRAST_LV7:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_P3);
		break;

	case CAMERA_CONTRAST_LV8:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_P4);
		break;

	case CAMERA_CONTRAST_LV9:
		sr200pc20m_WRT_LIST(sr200pc20_contrast_P4);
		break;
		
		CAM_DEBUG("[sr200pc20m] unexpected contrast mode %s/%d",
			__func__, __LINE__);
		break;
	}

	sr200pc20m_ctrl->settings.contrast= contrast;

}
static void sr200pc20m_set_ev(int ev)
{
	CAM_DEBUG("[sr200pc20m] %s : %d", __func__, ev);

	if((sr200pc20m_ctrl->op_mode == CAMERA_MODE_INIT) && (stop_stream == 1))
	{
		sr200pc20m_ctrl->settings.brightness = ev;
		return;
	}
		
#if 1 
	switch (ev) {
	case CAMERA_EV_M4:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_M4);
		break;

	case CAMERA_EV_M3:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_M3);
		break;

	case CAMERA_EV_M2:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_M2);
		break;

	case CAMERA_EV_M1:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_M1);
		break;

	case CAMERA_EV_DEFAULT:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_default);
		break;

	case CAMERA_EV_P1:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_P1);
		break;

	case CAMERA_EV_P2:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_P2);
		break;

	case CAMERA_EV_P3:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_P3);
		break;

	case CAMERA_EV_P4:
		sr200pc20m_WRT_LIST(sr200pc20_brightness_P4);
		break;

	default:
		CAM_DEBUG("[sr200pc20m] unexpected ev mode %s/%d",
			__func__, __LINE__);
		break;
	}
#endif
	sr200pc20m_ctrl->settings.brightness = ev;
}

static void sr200pc20m_set_flip(int flip)
{
	//unsigned char r_data, checkSubSampling = 0;
	CAM_DEBUG("flip : %d", flip);
#if 0
	sr200pc20m_i2c_write_16bit(0xFF87);
	sr200pc20m_i2c_read(0xD5, &r_data);
	if (r_data & 0x02)
		checkSubSampling = 1;
	switch (flip) {
	case 0:
		if (sr200pc20m_ctrl->cam_mode == MOVIE_MODE) {
			sr200pc20m_WRT_LIST(sr200pc20m_flip_off_No15fps);
		} else {					/* for 15fps */
			sr200pc20m_WRT_LIST(sr200pc20m_flip_off);
		}
		break;

	case 1:
		if (sr200pc20m_ctrl->cam_mode == MOVIE_MODE) {
			sr200pc20m_WRT_LIST(sr200pc20m_hflip_No15fps);
		} else {					/* for 15fps */
			sr200pc20m_WRT_LIST(sr200pc20m_hflip);
		}
		break;

	default:
		CAM_DEBUG("flip : default");
		break;
	}
#endif
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
		sr200pc20m_set_ev(ctrl_info.value_1);
		break;

	case  EXT_CAM_MOVIE_MODE:
		CAM_DEBUG("MOVIE mode : %d", ctrl_info.value_1);
		//sr200pc20m_ctrl->cam_mode = ctrl_info.value_1;
		break;

	case EXT_CAM_EXIF:
		ctrl_info.value_1 = sr200pc20m_get_exif(ctrl_info.address,
			ctrl_info.value_2);
		break;
	case EXT_CAM_VT_MODE:
		/*CAM_DEBUG(" VT mode : %d", ctrl_info.value_1);*/
		sr200pc20m_ctrl->vtcall_mode = ctrl_info.value_1;
		break;
	case EXT_CAM_SET_FLIP:
		/*CAM_DEBUG(" Dhana- FLIP mode : %d", ctrl_info.value_1);*/
		sr200pc20m_set_flip(ctrl_info.value_1);
		sr200pc20m_ctrl->mirror_mode = ctrl_info.value_1;
		break;
	case EXT_CAM_EFFECT:
		sr200pc20m_set_effect(ctrl_info.value_1); 
		break;

	case EXT_CAM_WB:
		sr200pc20m_set_whitebalance(ctrl_info.value_1);
		break;
/*
	case EXT_CAM_DTP_TEST:
		sr200pc20m_check_dataline(ctrl_info.value_1);
		break;
*/
	case EXT_CAM_PREVIEW_SIZE:
		sr200pc20m_set_preview_size(ctrl_info.value_1);
		break;

	case EXT_CAM_CONTRAST:
		sr200pc20m_set_contrast(ctrl_info.value_1);
		break;
		
	default:
		CAM_DEBUG("[sr200pc20m] default mode");
		break;
	}
	if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
		(const void *)&ctrl_info,
			sizeof(ctrl_info))) {
		cam_err("fail copy_to_user!");
	}

}

long sr200pc20m_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	void __user *argp = (void __user *)arg;
	struct msm_sensor_ctrl_t *sr200pc20m_s_ctrl = get_sctrl(sd);

	CAM_DEBUG("sr200pc20m_sensor_subdev_ioctl\n");
	CAM_DEBUG("%s: cmd %d\n", __func__, _IOC_NR(cmd));
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return sr200pc20m_sensor_config(sr200pc20m_s_ctrl, argp);
	/*case VIDIOC_MSM_SENSOR_RELEASE:*/
	/*	return msm_sensor_release(sr200pc20m_s_ctrl);*/
	case VIDIOC_MSM_SENSOR_CSID_INFO:
		{
		struct msm_sensor_csi_info *csi_info =
			(struct msm_sensor_csi_info *)arg;
		/*sr200pc20m_s_ctrl->csid_version = csi_info->csid_version;*/
		sr200pc20m_s_ctrl->is_csic = csi_info->is_csic;
		return 0;
		}
	default:
		return -ENOIOCTLCMD;
	}

}

int sr200pc20m_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
			rc = sr200pc20m_set_sensor_mode(cfg_data.mode);
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
			rc = sr200pc20m_sensor_setting(UPDATE_PERIODIC,
				RES_PREVIEW);
		break;
	case CFG_SET_MODE:
		rc = sr200pc20m_set_sensor_mode(cfg_data.mode);
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

#if defined(CONFIG_MACH_CRATERTD_CHN_3G)||defined(CONFIG_MACH_BAFFINVETD_CHN_3G)
static int sr200pc20m_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t ret = 0;

	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;

	if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917) {
            //RST->MCLK->VT STBY->DVDD->AVDD->IO
		/*GPIO76 - CAM2_RST_N*/
		gpio_set_value_cansleep(GPIO_CAM2_RST_N, 0);
		ret = gpio_get_value(GPIO_CAM2_RST_N);
		if (!ret)
			cam_err("[CAM2_RST_N::ret::%d]::OFF OK\n", ret);
		else
			cam_err("[CAM2_RST_N]::OFF Fail\n");
             msleep(5);

		/*CAM_MCLK0*/
		msm_cam_clk_enable(dev, cam_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
		rc = msm_camera_request_gpio_table(data, 0);
		if (rc < 0)
			CAM_DEBUG("%s: request gpio failed", __func__);
		gpio_tlmm_config(GPIO_CFG(GPIO_SUB_CAM_MCLK, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
             msleep(5);		

		//GPIO_VT_STBY
		gpio_set_value_cansleep(GPIO_VT_STBY,0);
		ret = gpio_get_value(GPIO_VT_STBY );
		cam_err("check VT standby ccc: %d", ret);
		msleep(5);
		
		/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
		l34 = regulator_get(NULL, "8917_l34");
		if (l34) {
			ret = regulator_disable(l34);	
			if (ret)
				cam_err("[CAM_SENSOR_IO_1P8]::SET Fail\n");
			else
				cam_err("[CAM_SENSOR_IO_1P8]::SET OK\n");
			}
		mdelay(5);	
		
		/*PMIC8921_l29 - CAM_DVDD_1P8*/
		l29 = regulator_get(NULL, "8921_l29");
		if (l29) {
			ret = regulator_disable(l29);
			if (!ret)
				cam_err("[CAM_DVDD_1P8]::OFF OK\n");
			else
				cam_err("[CAM_DVDD_1P8]::OFF Fail\n");
		}
		msleep(5);
		
		/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
		l32 = regulator_get(NULL, "8917_l32");
		if (l32) {
			ret = regulator_disable(l32);
			if (!ret)
				cam_err("[CAM_DVDD_1P8]::OFF OK\n");
			else
				cam_err("[CAM_DVDD_1P8]::OFF Fail\n");
		}
		msleep(5);
		
		//Vt IO 1.8
		l35 = regulator_get(NULL, "8917_l35");			 
		if (l35) {
			ret = regulator_disable(l35);
			if (!ret)
				cam_err("[CAM_DVDD_1P8]::OFF OK\n");
			else
				cam_err("[CAM_DVDD_1P8]::OFF Fail\n");
		}
		msleep(5);

		/* CAM_ISP_CORE_1P2 */
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_IO_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 0);
		ret = gpio_get_value(GPIO_CAM_IO_EN);
		if (!ret)
		cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2::ret::%d]::Disable OK\n", ret);
		else
		cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2]::Disable Fail\n");
		usleep(1*1000);
		
	}
	return rc;
}
#else //For craterTd and Baffin 0.3 HW
static int sr200pc20m_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	//return 0;
	int rc = 0;
	uint32_t ret = 0;

	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;

	if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917) {
#if !defined(CONFIG_MACH_CRATER_CHN_CTC)
		/*PMIC8917_L34 - CAM_SENSOR_IO_1P8  */
		l34 = regulator_get(NULL, "8917_l34");
		if (l34) {
			ret = regulator_disable(l34);
			if (ret)
				cam_err("[CAM_SENSOR_IO_1P8]::OFF Fail\n");
			else
				cam_err("[CAM_SENSOR_IO_1P8]::OFF OK\n");
		}
		udelay(1000);
#endif
		/*GPIO76 - CAM2_RST_N*/
		gpio_set_value_cansleep(GPIO_CAM2_RST_N, 0);
		ret = gpio_get_value(GPIO_CAM2_RST_N);
		if (!ret)
			cam_err("[CAM2_RST_N::ret::%d]::OFF OK\n", ret);
		else
			cam_err("[CAM2_RST_N]::OFF Fail\n");
#if defined(CONFIG_MACH_CRATER_CHN_CTC)
            //RST->MCLK->VT STBY->DVDD->AVDD->IO
             msleep(5);
#else
		udelay(1000);
#endif
#if !defined(CONFIG_MACH_CRATER_CHN_CTC)
#if defined(CONFIG_MACH_KS02)
		/*PMIC8917_L34 - CAM_SENSOR_IO_1P8	*/
		sub_ldo4 = regulator_get(NULL, "lp8720_ldo4");
		if (sub_ldo4) {
			ret = regulator_disable(sub_ldo4);
			if (ret)
				pr_err("set_voltage sub_ldo4 failed, rc=%d\n", rc);
		}
		udelay(1000);
#endif

		/*PMIC8921_l29 - CAM_DVDD_1P8*/
		l29 = regulator_get(NULL, "8921_l29");
		if (l29) {
			ret = regulator_disable(l29);
			if (!ret)
				cam_err("[CAM_DVDD_1P8]::OFF OK\n");
			else
				cam_err("[CAM_DVDD_1P8]::OFF Fail\n");
		}
		udelay(1000);

#if defined(CONFIG_MACH_MELIUS)
		/* CAM_SENSOR_A2P8 */
#if defined(CONFIG_MACH_MELIUS_VZW) || defined(CONFIG_MACH_MELIUS_SPR) || defined(CONFIG_MACH_MELIUS_USC)
		if (system_rev < 0X01) {
#else
		if (system_rev < 0x07) {
#endif
			l32 = regulator_get(NULL, "8917_l32");
			if (l32) {
				ret = regulator_disable(l32);
				if (ret)
					cam_err("[CAM_SENSOR_A2P8]::OFF Fail\n");
				else
					cam_err("[CAM_SENSOR_A2P8]::OFF OK\n");
			}
		} else {
			gpio_set_value_cansleep(GPIO_CAM_SENSOR_EN, 0);
			ret = gpio_get_value(GPIO_CAM_SENSOR_EN);
			if (!ret)
				cam_err("[GPIO_CAM_SENSOR_EN::CAM_SENSOR_A2P8::ret::%d]::OFF OK\n", ret);
			else
				cam_err("[GPIO_CAM_SENSOR_EN::CAM_SENSOR_A2P8]::OFF Fail\n");
		}
		mdelay(5);
#elif defined(CONFIG_MACH_KS02)
		sub_ldo3 = regulator_get(NULL, "lp8720_ldo3");
		if (sub_ldo3) {
			ret = regulator_disable(sub_ldo3);
			if (ret)
				pr_err("set_voltage sub_ldo3 failed, rc=%d\n", rc);
		}
		udelay(1000);

#else
		/*PMIC8917_L32 - CAM_SENSOR_A2P8*/
		l32 = regulator_get(NULL, "8917_l32");
		if (l32) {
			ret = regulator_disable(l32);
			if (ret)
				cam_err("[CAM_SENSOR_A2P8]::OFF Fail\n");
			else
				cam_err("[CAM_SENSOR_A2P8]::OFF OK\n");
		}
		udelay(1000);
#endif
	}
		gpio_set_value_cansleep(GPIO_VT_STBY,0);
		ret = gpio_get_value(GPIO_VT_STBY );
		cam_err("check VT standby ccc: %d", ret);
		usleep(30);
#endif
	/*CAM_MCLK0*/
	msm_cam_clk_enable(dev, cam_clk_info,
		s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	rc = msm_camera_request_gpio_table(data, 0);
	if (rc < 0)
		CAM_DEBUG("%s: request gpio failed", __func__);

	gpio_tlmm_config(GPIO_CFG(GPIO_SUB_CAM_MCLK, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
#if defined(CONFIG_MACH_CRATER_CHN_CTC)
              msleep(5);
		/*STBY*/
              gpio_set_value_cansleep(GPIO_VT_STBY,0);
		ret = gpio_get_value(GPIO_VT_STBY );
		if (!ret)
			cam_err("[CAM2_VT_STBY_N::ret::%d]::OFF OK\n", ret);
		else
			cam_err("[CAM2_VT_STBY_N]::OFF Fail\n");
		mdelay(5);
		/*DVDD*/
		l35 = regulator_get(NULL, "8917_l35");
		if (IS_ERR(l35))
			cam_err("[CAM_SENSOR_DVDD_1P8]::regulator_get l35 fail\n");

		ret = regulator_disable(l35);
		if (ret)
			cam_err("[CAM_SENSOR_DVDD_1P8]::OFF Fail\n");
		else
			cam_err("[CAM_SENSOR_DVDD_1P8]::OFF OK\n");
               mdelay(5);

		/*AVDD*/
		gpio_set_value_cansleep(GPIO_CAM_SENSOR_EN, 0);
		ret = gpio_get_value(GPIO_CAM_SENSOR_EN);
		if (!ret)
			cam_err("[CAM2_SENSOR_AVDD_1::ret::%d]::OFF OK\n", ret);
		else
			cam_err("[CAM2_SENSOR_AVDD_1]::OFF Fail\n");
		mdelay(5);

		/*IO*/
		l36 = regulator_get(NULL, "8917_l36");
		if (IS_ERR(l36))
			cam_err("[CAM_SENSOR_IO_1P8]::regulator_get l36 fail\n");

		ret = regulator_disable(l36);
		if (ret)
			cam_err("[CAM_SENSOR_IO_1P8]::OFF Fail\n");
		else
			cam_err("[CAM_SENSOR_IO_1P8]::OFF OK\n");
               usleep(1000);
}

#else
l35 = regulator_get(NULL, "8917_l35");
			if (l35) {
				ret = regulator_disable(l35);
				if (ret)
					cam_err("[CAM_SENSOR_IO1.8]::OFF Fail\n");
				else
					cam_err("[CAM_SENSOR_IO1.8]::OFF OK\n");
				}
		mdelay(5);
/* CAM_ISP_CORE_1P2 */
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_IO_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 0);
		ret = gpio_get_value(GPIO_CAM_IO_EN);
		if (!ret)
		cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2::ret::%d]::Disable OK\n", ret);
		else
		cam_err("[CAM_CORE_EN::CAM_ISP_CORE_1P2]::Disable Fail\n");
		usleep(1*1000);
#endif

	return rc;
}
#endif

static struct v4l2_subdev_info sr200pc20m_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static int sr200pc20m_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code) {
	CAM_DEBUG("Index is %d", index);
	if ((unsigned int)index >= ARRAY_SIZE(sr200pc20m_subdev_info))
		return -EINVAL;

	*code = sr200pc20m_subdev_info[index].code;
	return 0;
}

void sr200pc20m_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	CAM_DEBUG("start_stream");
	stop_stream = 0;

	if (sr200pc20m_ctrl->op_mode == CAMERA_MODE_CAPTURE)
		return ;

	if (sr200pc20m_ctrl->cam_mode == MOVIE_MODE)
	{
       	CAM_DEBUG("VGA recording op_mode : %d\n",sr200pc20m_ctrl->op_mode);
		if (sr200pc20m_ctrl->settings.preview_size_idx == PREVIEW_SIZE_D1)
		{
             		CAM_DEBUG("720 480 sr200pc20m_sensor_start_stream MOVIE MODE");
		       sr200pc20m_WRT_LIST(sr200pc20_D1_20fps);
		}
		if (sr200pc20m_ctrl->op_mode == CAMERA_MODE_INIT ||
			sr200pc20m_ctrl->op_mode == CAMERA_MODE_PREVIEW) 
		{
		
	     	if (sr200pc20m_ctrl->settings.preview_size_idx != PREVIEW_SIZE_D1) 
	 		{
				sr200pc20m_WRT_LIST(sr200pc20_Auto_fps);
	 		}			

		}
		else if   (sr200pc20m_ctrl->op_mode == CAMERA_MODE_RECORDING)
	     {

			if(sr200pc20m_ctrl->settings.effect!= CAMERA_EFFECT_OFF)	
			{
				sr200pc20m_set_effect(sr200pc20m_ctrl->settings.effect);
			}
			else
			{
				sr200pc20m_set_effect(CAMERA_EFFECT_OFF);
			}
			
			//Songww 20130504 whitebalance not remain
			if(sr200pc20m_ctrl->settings.wb != CAMERA_WHITE_BALANCE_AUTO)
			{
				sr200pc20m_set_whitebalance(sr200pc20m_ctrl->settings.wb);
			}
			else
			{
				sr200pc20m_set_whitebalance(CAMERA_WHITE_BALANCE_AUTO);
			}
			
			if(sr200pc20m_ctrl->settings.brightness != CAMERA_EV_DEFAULT)
			{
				sr200pc20m_set_ev(sr200pc20m_ctrl->settings.brightness);
			}
			else
			{
				sr200pc20m_set_ev(CAMERA_EV_DEFAULT);
			}

		}
			
	} 
	else
	{
		CAM_DEBUG("Camera Mode");
		if (sr200pc20m_ctrl->op_mode == CAMERA_MODE_INIT) 
		{
		 	if (sr200pc20m_ctrl->settings.preview_size_idx == PREVIEW_SIZE_D1)
			{
             			CAM_DEBUG("720 480 sr200pc20m_sensor_start_stream INITMODE");
			       sr200pc20m_WRT_LIST(sr200pc20_D1_20fps);
			}
			else
				sr200pc20m_WRT_LIST(sr200pc20_Init_Reg);
			
			CAM_DEBUG("sr200pc20m Common chris.shen Registers written\n");

			//Songww 20130504 effect not remain
			if(sr200pc20m_ctrl->settings.effect!= CAMERA_EFFECT_OFF)	
			{
				sr200pc20m_set_effect(sr200pc20m_ctrl->settings.effect);
			}
			else
			{
				sr200pc20m_set_effect(CAMERA_EFFECT_OFF);
			}
			
			//Songww 20130504 whitebalance not remain
			if(sr200pc20m_ctrl->settings.wb != CAMERA_WHITE_BALANCE_AUTO)
			{
				sr200pc20m_set_whitebalance(sr200pc20m_ctrl->settings.wb);
			}
			else
			{
				sr200pc20m_set_whitebalance(CAMERA_WHITE_BALANCE_AUTO);
			}
			
			if(sr200pc20m_ctrl->settings.brightness != CAMERA_EV_DEFAULT)
			{
				sr200pc20m_set_ev(sr200pc20m_ctrl->settings.brightness);
			}
			else
			{
				sr200pc20m_set_ev(CAMERA_EV_DEFAULT);
			}

			//sr200pc20m_WRT_LIST(sr200pc20_Preview);

			sr200pc20m_ctrl->op_mode = CAMERA_MODE_PREVIEW; 
			
		}

		CAM_DEBUG("MIPI TIMING : 0x1d0e");
		
		//sr200pc20m_WRT_LIST(sr200pc20_Preview);

	}

}

void sr200pc20m_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	if (sr200pc20m_ctrl->op_mode == CAMERA_MODE_CAPTURE)
		return ;

	stop_stream = 1;
	CAM_DEBUG(" sr200pc20m_sensor_stop_stream E");
//	sr200pc20m_WRT_LIST(sr200pc20m_stop_stream);
	CAM_DEBUG(" sr200pc20m_sensor_stop_stream X");
}

static struct v4l2_subdev_core_ops sr200pc20m_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = sr200pc20m_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops sr200pc20m_subdev_video_ops = {
	.enum_mbus_fmt = sr200pc20m_enum_fmt,
};

static struct v4l2_subdev_ops sr200pc20m_subdev_ops = {
	.core = &sr200pc20m_subdev_core_ops,
	.video  = &sr200pc20m_subdev_video_ops,
};

static int sr200pc20m_i2c_probe(struct i2c_client *client,
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

	sr200pc20m_client = client;
	sr200pc20m_dev = s_ctrl->sensor_i2c_client->client->dev;

	sr200pc20m_ctrl = kzalloc(sizeof(struct sr200pc20m_ctrl), GFP_KERNEL);
	if (!sr200pc20m_ctrl) {
		CAM_DEBUG("sr200pc20m_ctrl alloc failed!\n");
		return -ENOMEM;
	}

	sr200pc20m_exif = kzalloc(sizeof(struct sr200pc20m_exif_data),
		GFP_KERNEL);
	if (!sr200pc20m_exif) {
		cam_err("Cannot allocate memory fo EXIF structure!");
		kfree(sr200pc20m_exif);
		rc = -ENOMEM;
	}

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);

	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		&sr200pc20m_subdev_ops);

	sr200pc20m_ctrl->s_ctrl = s_ctrl;
	sr200pc20m_ctrl->sensor_dev = &s_ctrl->sensor_v4l2_subdev;
	sr200pc20m_ctrl->sensordata = client->dev.platform_data;

	rc = msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);

	if (rc < 0) {
		cam_err("msm_sensor_register failed!");
		kfree(sr200pc20m_exif);
		kfree(sr200pc20m_ctrl);
		goto probe_failure;
		}

	cam_err("sr200pc20m_probe succeeded!");
	return 0;

probe_failure:
	cam_err("sr200pc20m_probe failed!");
	return rc;
}



static struct msm_sensor_id_info_t sr200pc20m_id_info = {
	.sensor_id_reg_addr = 0x04,
	.sensor_id = 0xb4,
};

static const struct i2c_device_id sr200pc20m_i2c_id[] = {
	{"sr200pc20m", (kernel_ulong_t)&sr200pc20m_s_ctrl},
	{},
};

static struct msm_camera_i2c_client sr200pc20m_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct i2c_driver sr200pc20m_i2c_driver = {
	.id_table = sr200pc20m_i2c_id,
	.probe  = sr200pc20m_i2c_probe,
	.driver = {
		.name = "sr200pc20m",
	},
};

static int __init sr200pc20m_init(void)
{
	return i2c_add_driver(&sr200pc20m_i2c_driver);
}

static struct msm_sensor_fn_t sr200pc20m_func_tbl = {
	.sensor_config = sr200pc20m_sensor_config,
	.sensor_power_up = sr200pc20m_sensor_power_up,
	.sensor_power_down = sr200pc20m_sensor_power_down,
	.sensor_match_id = sr200pc20m_sensor_match_id,
	/*.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,*/
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_start_stream = sr200pc20m_sensor_start_stream,
	.sensor_stop_stream = sr200pc20m_sensor_stop_stream,
	/*.sensor_get_lens_info = sensor_get_lens_info,*/
};


static struct msm_sensor_reg_t sr200pc20m_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_sensor_ctrl_t sr200pc20m_s_ctrl = {
	.msm_sensor_reg = &sr200pc20m_regs,
	.sensor_i2c_client = &sr200pc20m_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_id_info = &sr200pc20m_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	/*.csic_params = &sr200pc20m_csic_params_array[0],
	.csi_params = &sr200pc20m_csi_params_array[0],*/
	.msm_sensor_mutex = &sr200pc20m_mut,
	.sensor_i2c_driver = &sr200pc20m_i2c_driver,
	.sensor_v4l2_subdev_info = sr200pc20m_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sr200pc20m_subdev_info),
	.sensor_v4l2_subdev_ops = &sr200pc20m_subdev_ops,
	.func_tbl = &sr200pc20m_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};


module_init(sr200pc20m_init);
