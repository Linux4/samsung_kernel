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
#include "sr352.h"

#include "msm.h"
#include "msm_ispif.h"
#include "msm_sensor.h"
#include "../../../../../arch/arm/mach-msm/include/mach/lt02_ctc-gpio.h"

//#define CONFIG_LOAD_FILE 

#define BURST_MODE_BUFFER_MAX_SIZE 255
#define BURST_REG 0x0e
#define DELAY_REG 0xff
unsigned char sr352_buf_for_burstmode[BURST_MODE_BUFFER_MAX_SIZE];
static bool mode_enable;
int movie_mode = 0;



#ifdef CONFIG_LOAD_FILE


#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
static char *sr352_regs_table;
static int sr352_regs_table_size;

static int sr352_write_regs_from_sd(char *name);
static int sr352_i2c_write_multi(unsigned short addr, unsigned int w_data);

struct test {
	u8 data;
	struct test *nextBuf;
};
static struct test *testBuf;
static s32 large_file;
#endif
static int sr352_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
static void sr352_set_ev(int ev);
int sr352_set_effect(int effect);
int sr352_set_whitebalance(int wb);

DEFINE_MUTEX(sr352_mut);

#define sr352_WRT_LIST(A)	\
	sr352_i2c_wrt_list(A, (sizeof(A) / sizeof(A[0])), #A);

#define sr352_WRITE_LIST_BURST(A)\
	sr352_sensor_burst_write(A, (sizeof(A) / sizeof(A[0])), #A);


#define CAM_REV ((system_rev <= 1) ? 0 : 1)

#include "sr352_regs.h"

static unsigned int config_csi2;
static unsigned int stop_stream;
static int start_flag = 0;


#ifdef CONFIG_LOAD_FILE 

int sr352_regs_table_init(void)
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

	fp = filp_open("/mnt/sdcard/sr352_regs.h", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open /mnt/sdcard/sr352_regs.h");
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
	return 0;
error_out:
	if (fp)
		filp_close(fp, current->files);
	return ret;
}
static inline int sr352_write(struct i2c_client *client,
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

	msg.addr = sr352_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	do {
		err = i2c_transfer(sr352_client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d\n",
		       packet, err);
		usleep(3000);

	} while (retry_count <= 5);

	return (err != 1) ? -1 : 0;
}


static int sr352_write_regs_from_sd(char *name)
{
	struct test *tempData = NULL;

	int ret = -EAGAIN;
	u16 temp;
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
		if ((temp & 0xFF00) == sr352_DELAY) {
			delay = temp & 0xFF;
			cam_info("sr352 delay(%d)", delay);
			/*step is 10msec */
			msleep(delay);
			continue;
		}
		ret = sr352_write(sr352_client, temp);

		/* In error circumstances */
		/* Give second shot */
		if (unlikely(ret)) {
			ret = sr352_write(sr352_client, temp);

			/* Give it one more shot */
			if (unlikely(ret))
				ret = sr352_write(sr352_client, temp);
			}
		}

	return 0;
}

static int sr300pc20_regs_table_burst_write(char *name)
{
	char *start = NULL, *end = NULL;
	char *reg = NULL;
	unsigned char addr = 0, value = 0;
	unsigned short data = 0;
	char data_buf[7] = { 0 };
	int idx = 0;
	int err = 0;
	int burst_flag = 0;
	int burst_cnt = 0;
	struct i2c_msg msg = { sr300pc20_client->addr >> 1,
		0, 0, sr352_regs_table_size
	};

	CAM_DEBUG("Enter!!\n");

	addr = value = 0;

	*(data_buf + 6) = '\0';

	start = strnstr(sr352_regs_table_size, name, sr352_regs_table_size);
	if (start == NULL) {
		pr_err("[%s : %d] start is NULL\n", __func__, __LINE__);
		err = -EIO;
		return err;
	}

	end = strnstr(start, "};", sr352_regs_table_size);
	if (end == NULL) {
		pr_err("[%s : %d] end is NULL\n", __func__, __LINE__);
		err = -EIO;
		return err;
	}

	while (1) {
		/* Find Address */
		reg = strnstr(start, "0x", sr352_regs_table_size);
		if (reg)
			start = (reg + 6);

		if ((reg == NULL) || (reg > end)) {
			pr_err("[%s : %d] write end of %s\n",
			       __func__, __LINE__, name);
			break;
		}
		/* Write Value to Address */
		memcpy(data_buf, reg, 6);

		if (sr352_is_hexnum(data_buf) == 0) {
			pr_err("[%s : %d] it's not hex number %s\n",
			       __func__, __LINE__, data_buf);
			continue;
		}

		err = kstrtou16(data_buf, 16, &data);
		if (err < 0) {
			pr_err("[%s : %d] kstrtou16 failed\n",
			       __func__, __LINE__);
		}
		addr = (data >> 8);
		value = (data & 0xff);

		if (idx > (BURST_MODE_BUFFER_MAX_SIZE - 10)) {
			pr_err("[%s : %d]Burst mode buffer overflow! "
			       "Burst Count %d\n",
			       __func__, __LINE__, burst_cnt);
			pr_err("[%s : %d] addr %x "
			       "value %x\n", __func__, __LINE__,
			       (data >> 8) & 0xff, data & 0xFF);

			err = -EIO;
			return err;
		}

		if (burst_flag == 0) {
			switch (addr) {
			case BURST_REG:
				if (value != 0x00) {
					burst_flag = 1;
					burst_cnt++;
				}
				break;
			case DELAY_REG:
				msleep(value * 10);	/* a step is 10ms */
				break;
			default:
				idx = 0;
				err = sr300pc20_sensor_write(addr, value);
				break;
			}
		} else if (burst_flag == 1) {
			if (addr == BURST_REG && value == 0x00) {
				msg.len = idx;
				err = i2c_transfer(sr300pc20_client->adapter,
						   &msg, 1) == 1 ? 0 : -EIO;
				idx = 0;
				burst_flag = 0;
			} else {
				if (idx == 0) {
					sr352_buf_for_burstmode[idx++] =
					    addr;
					sr352_buf_for_burstmode[idx++] =
					    value;
				} else
					sr352_buf_for_burstmode[idx++] =
					    value;
			}
		}
	}

	CAM_DEBUG("Exit!!\n");

	return err;
}

static int sr352_is_hexnum(char *num)
{
	int i = 0;
	for (i = 2; num[i] != '\0'; i++) {
		if (!((num[i] >= '0' && num[5] <= '9')
		      || (num[5] >= 'a' && num[5] <= 'f') || (num[5] >= 'A'
							      && num[5] <=
							      'F'))) {
			return 0;
		}
	}
	return 1;
}


void sr352_regs_table_exit(void)
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

static DECLARE_WAIT_QUEUE_HEAD(sr352_wait_queue);

/**
 * sr352_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor
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
 * sr352_i2c_read: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static int sr352_i2c_read(unsigned char subaddr, unsigned char *data)
{
	unsigned char buf[1];
	struct i2c_msg msg = {sr352_client->addr, 0, 1, buf};

	int err = 0;
	buf[0] = subaddr;

	if (!sr352_client->adapter)
		return -EIO;

	err = i2c_transfer(sr352_client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;

	msg.flags = I2C_M_RD;

	err = i2c_transfer(sr352_client->adapter, &msg, 1);
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
 * sr352_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int sr352_i2c_write_multi(unsigned short addr, unsigned int w_data)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (addr >> 8);
	buf[1] = (u8) (w_data & 0xff);

	msg.addr = sr352_client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = buf;


	cam_err("I2C CHIP ID=0x%x, DATA 0x%x 0x%x\n",
			sr352_client->addr, buf[0], buf[1]);


	do {
		rc = i2c_transfer(sr352_client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		cam_err("retry_count %d\n", retry_count);
		usleep(3000);

	} while (retry_count <= 5);

	return 0;
}
#endif

static int32_t sr352_i2c_write_16bit(u16 packet)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (packet >> 8);
	buf[1] = (u8) (packet & 0xff);

	msg.addr = sr352_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

#if defined(CAM_I2C_DEBUG)
	cam_err("I2C CHIP ID=0x%x, DATA 0x%x 0x%x\n",
			sr352_client->addr, buf[0], buf[1]);
#endif

	do {
		rc = i2c_transfer(sr352_client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d\n",
		       packet, rc);
		usleep(100);

	} while (retry_count <= 5);

	return (rc==1)?0:-EIO;
}

static int sr352_i2c_wrt_list(const u16 *regs,
	int size, char *name)
{
#ifdef CONFIG_LOAD_FILE
	sr352_write_regs_from_sd(name);
#else

	int i;
	u8 m_delay = 0;

	u16 temp_packet;


	CAM_DEBUG("%s, size=%d", name, size);
	for (i = 0; i < size; i++) {
		temp_packet = regs[i];

		if ((temp_packet & sr352_DELAY) == sr352_DELAY) {
			m_delay = temp_packet & 0xFF;
			cam_info("delay = %d", m_delay*10);
			msleep(m_delay*10);/*step is 10msec*/
			continue;
		}

		if (sr352_i2c_write_16bit(temp_packet) < 0) {
			cam_err("fail(0x%x, 0x%x:%d)",
					sr352_client->addr, temp_packet, i);
			return -EIO;
		}
		/*udelay(10);*/
	}
#endif

	return 0;
}


static int sr352_sensor_burst_write(const unsigned short *list, int size,
						char *name)
{

#ifdef CONFIG_LOAD_FILE
	//err = sr352_regs_table_burst_write(name);
#else
	int i;
	int idx=0;
	int burst_flag = 0;
	int burst_cnt = 0;
	int err = -EINVAL;
	struct i2c_msg msg = { sr352_client->addr,
		0, 0, sr352_buf_for_burstmode
	};
	unsigned char subaddr;
	unsigned char value;

	CAM_DEBUG("%s, size = %d\n", name, size);

	for (i = 0; i < size; i++) {
		if (idx > (BURST_MODE_BUFFER_MAX_SIZE - 10)) {
			pr_err("[%s:%d]Burst mode buffer overflow! "
			       "Burst Count %d\n",
			       __func__, __LINE__, burst_cnt);
			pr_err("[%s:%d]count %d, addr %x "
			       "value %x\n", __func__, __LINE__, i,
			       (list[i] >> 8) & 0xff, list[i] & 0xFF);
			pr_err("[%s:%d]addr %x value %x\n",
			       __func__, __LINE__,
			       (list[i - 1] >> 8) & 0xff, list[i - 1] & 0xFF);
			pr_err("[%s:%d]addr %x value %x\n",
			       __func__, __LINE__,
			       (list[i - 2] >> 8) & 0xff, list[i - 2] & 0xFF);
			err = -EIO;
			return err;
		}
		subaddr = (list[i] >> 8);
		value = (list[i] & 0xFF);
		if (burst_flag == 0) {
			switch (subaddr) {
			case BURST_REG:
				if (value != 0x00) {
					burst_flag = 1;
					burst_cnt++;
				}
				break;
			case DELAY_REG:
				msleep(value * 10);	// a step is 10ms 
				break;
			default:
				idx = 0;
				err = sr352_i2c_write_16bit(list[i]);
				break;
			}
		} else if (burst_flag == 1) {
			if (subaddr == BURST_REG && value == 0x00) {
				msg.len = idx;
				//CAM_DEBUG("burst_cnt %d, idx %d\n",
				//	     burst_cnt, idx);
				err = i2c_transfer(sr352_client->adapter,
						   &msg, 1);
				if (err < 0) {
					pr_err("[%s:%d]Burst write fail!\n",
					       __func__, __LINE__);
					return err;
				}
				idx = 0;
				burst_flag = 0;
			} else {
				if (idx == 0) {
					sr352_buf_for_burstmode[idx++] =
					    subaddr;
					sr352_buf_for_burstmode[idx++] =
					    value;
				} else {
					sr352_buf_for_burstmode[idx++] =
					    value;
				}
			}
		}
	}
#endif

	if (unlikely(err < 0)) {
		pr_err("[%s:%d] register set failed\n", __func__, __LINE__);
		return err;
	}

	return err;
}


static int sr352_set_exif(void)
{
	int err = 0;
	u8 read_value0 = 0;
	u8 read_value1 = 0;
	u8 read_value2 = 0;
	u8 read_value3 = 0;
	u8 read_value4 = 0;
	//unsigned short gain_value = 0;

	sr352_i2c_write_16bit(0x0320);
	sr352_i2c_read(0xa0, &read_value0);
	sr352_i2c_read(0xa1, &read_value1);
	sr352_i2c_read(0xa2, &read_value2);
	sr352_i2c_read(0xa3, &read_value3);
	sr352_exif->shutterspeed = 27000000 / ((read_value0 << 24)
		+ (read_value1 << 16) + (read_value2 << 8) + read_value3);

	CAM_DEBUG("Exposure time = %d\n", sr352_exif->shutterspeed);
	sr352_i2c_write_16bit(0x0320);
	sr352_i2c_read(0x50, &read_value4);
	//gain_value = (read_value4 / 16) * 1000;

	if (read_value4 < 0x21)
		sr352_exif->iso = 50;
	else if (read_value4 < 0x5C)
		sr352_exif->iso = 100;
	else if (read_value4 < 0x83)
		sr352_exif->iso = 200;
	else if (read_value4 < 0xF1)
		sr352_exif->iso = 400;
	else
		sr352_exif->iso = 800;

	CAM_DEBUG("ISO = %d\n", sr352_exif->iso);
	return err;
}


static int sr352_get_exif(int exif_cmd, unsigned short value2)
{
	unsigned short val = 0;

	switch (exif_cmd) {
	case EXIF_SHUTTERSPEED:
		val = sr352_exif->shutterspeed;
		break;

	case EXIF_ISO:
		val = sr352_exif->iso;
		break;

	default:
		break;
	}

	return val;
}

static void sr352_set_init_mode(void)
{
	config_csi2 = 0;
	sr352_ctrl->cam_mode = PREVIEW_MODE;
	sr352_ctrl->op_mode = CAMERA_MODE_INIT;
	sr352_ctrl->vtcall_mode = 0;
	sr352_ctrl->settings.preview_size_idx = 0;

	sr352_WRITE_LIST_BURST(sr352_Init_Reg);
	sr352_WRT_LIST(sr352_preview);  //eunice : change init sequence
	
	start_flag = 1;
}


void sr352_set_preview_size(int32_t index)
{
	CAM_DEBUG("index %d", index);
	
	switch (index) {
	
		case PREVIEW_SIZE_VGA:
			if((sr352_ctrl->cam_mode==1) && (start_flag == 1) && (sr352_ctrl->settings.preview_size_idx == 4)){
			sr352_WRT_LIST(sr352_Init_Reg);
			}
			sr352_WRT_LIST(sr352_preview_640_480);
			//mdelay(100);
			break;
			
	/*	case PREVIEW_SIZE_D1:
			sr352_WRT_LIST(sr352_preview_720_480);
			mdelay(100);
			break;*/

		case PREVIEW_SIZE_WVGA:
			sr352_WRT_LIST(sr352_preview_800_480);
			//mdelay(100);
			break;
/*
		case PREVIEW_SIZE_XGA:
			sr352_WRT_LIST(sr352_preview_1024_768);
			mdelay(100);
			break;*/

		case PREVIEW_SIZE_MMS:
			sr352_WRT_LIST(sr352_preview_176_144);
			//mdelay(100);
			break;
			
		case PREVIEW_SIZE_HD:
			break;

		case PREVIEW_SIZE_1024x576:
			sr352_WRT_LIST(sr352_preview_1024_576);
			break;
			
		default:
			break;
		}
		//mdelay(100);
	
	sr352_ctrl->settings.preview_size_idx = index;
}

#if 0
void sr352_set_picture_size(int32_t index)
{
	cam_info(" %d -> %d\n", sr352_ctrl->settings.capture_size, index);
	return;
	switch (index) {
	
	case MSM_V4L2_PICTURE_SIZE_2048x1536_3M:
		sr352_WRT_LIST(sr352_Capture_2048_1536);
		break;

	case MSM_V4L2_PICTURE_SIZE_2048x1152:
		sr352_WRT_LIST(sr352_Capture_2048_1152);
		break;

	case MSM_V4L2_PICTURE_SIZE_1600x1200_2M:
		sr352_WRT_LIST(sr352_Capture_1600_1200);
		break;

	case MSM_V4L2_PICTURE_SIZE_1280x960_1M:
		sr352_WRT_LIST(sr352_Capture_1280_960);
		break;

	case MSM_V4L2_PICTURE_SIZE_1280x720:
			sr352_WRT_LIST(sr352_Capture_1280_720);
			break;

	case MSM_V4L2_PICTURE_SIZE_1024x768_8K:
		sr352_WRT_LIST(sr352_Capture_1536_864);
		break;

	case MSM_V4L2_PICTURE_SIZE_960x720:
		sr352_WRT_LIST(sr352_Capture_960_720);
		break;

	case MSM_V4L2_PICTURE_SIZE_640x480_VGA:
		sr352_WRT_LIST(sr352_Capture_640_480);
		break;

	/*case MSM_V4L2_PICTURE_SIZE_2560x1536_4M_WIDE:
		testcamera_WRITE_LIST(testcamera_4M_WIDE_Capture);
		break;
*/
	/*case MSM_V4L2_PICTURE_SIZE_2048x1232_2_4M_WIDE:
		sr352_WRT_LIST(sr352_3M_WIDE_Capture);
		break;*/

	/*case MSM_V4L2_PICTURE_SIZE_1600x960_1_5M_WIDE:
		testcamera_WRITE_LIST(testcamera_1_5M_WIDE_Capture);
		break;
*/
/*	case MSM_V4L2_PICTURE_SIZE_800x480_4K_WIDE:
		sr352_WRT_LIST(sr352_Capture_);
		break;*/

	default:
		//sr352_WRT_LIST(sr352_Capture_2048_1536);
		break;
	}


	sr352_ctrl->settings.capture_size = index;
}
#endif

/*void sr352_set_frame_rate(int32_t fps)

{
	cam_info(" %d", fps);
	switch (fps) {

	case 15:
		sr352_WRT_LIST(sr352_recording_50Hz_15fps);
		break;

	case 25:
		sr352_WRT_LIST(sr352_recording_50Hz_25fps);
		break;

	case 30:
		sr352_WRT_LIST(sr352_recording_50Hz_30fps);
		break;

	default:
		sr352_WRT_LIST(sr352_recording_50Hz_modeOff);
		break;
	}

	sr352_ctrl->settings.fps = fps;
}*/

void sr352_set_preview(void)
{
	mode_enable = true;
	
	CAM_DEBUG("sr352_set_preview");

	if (sr352_ctrl->cam_mode == MOVIE_MODE) {
		CAM_DEBUG("VGA recording");
		if (sr352_ctrl->op_mode == CAMERA_MODE_INIT ||
			sr352_ctrl->op_mode == CAMERA_MODE_PREVIEW) {
			//sr352_WRITE_LIST_BURST(sr352_stream_stop);
			//sr352_WRITE_LIST_BURST(sr352_Init_Reg);
			
			if(sr352_ctrl->settings.preview_size_idx == PREVIEW_SIZE_HD){
			sr352_WRITE_LIST_BURST(sr352_recording_50Hz_HD);
			CAM_DEBUG("sr352_recording_50Hz_HD");
			//sr352_WRT_LIST(sr352_metering_matrix);
			}else{
			//sr352_WRT_LIST(sr352_metering_matrix);
			sr352_WRITE_LIST_BURST(sr352_recording_50Hz_30fps);
			//sr352_WRT_LIST(sr352_preview_640_480);
			CAM_DEBUG("sr352_recording_50Hz_30fps");
			}

			if(sr352_ctrl->settings.effect!= CAMERA_EFFECT_OFF){
				sr352_set_effect(sr352_ctrl->settings.effect);
			}else{
				sr352_set_effect(CAMERA_EFFECT_OFF);
			}
			if(sr352_ctrl->settings.wb != CAMERA_WHITE_BALANCE_AUTO){
				sr352_set_whitebalance(sr352_ctrl->settings.wb);
			}else{
				sr352_set_whitebalance(CAMERA_WHITE_BALANCE_AUTO);
			}				
			if(sr352_ctrl->settings.brightness != CAMERA_EV_DEFAULT){
				sr352_set_ev(sr352_ctrl->settings.brightness);
			}else{
				sr352_set_ev(CAMERA_EV_DEFAULT);
			}
			
			mode_enable = true;
			movie_mode = 1;
			sr352_ctrl->op_mode = CAMERA_MODE_RECORDING;
		//sr352_WRT_LIST(sr352_preview_640_480);     //eunice//

			/*if (sr352_ctrl->mirror_mode == 1)
					sr352_set_flip( \
					sr352_ctrl->mirror_mode);*/
			}
		return ;
		}
	if (sr352_ctrl->op_mode == CAMERA_MODE_INIT) {
		CAM_DEBUG("sr352_set_preview_INIT");
	//	sr352_WRITE_LIST_BURST(sr352_Init_Reg);
	//	sr352_WRT_LIST(sr352_preview);  //eunice : change init sequence
	//	sr352_set_preview_size(sr352_ctrl->settings.preview_size_idx);

		}else if (sr352_ctrl->op_mode == CAMERA_MODE_CAPTURE){
		CAM_DEBUG("sr352_set_preview_AFTER CAPTURE");
		//msleep(100);   //reduce delay 
		sr352_WRT_LIST(sr352_preview);
		sr352_set_preview_size(sr352_ctrl->settings.preview_size_idx);
		
		}else{
		CAM_DEBUG("sr352_set_preview_RETURN PREVIEW");
		if(movie_mode == 1){
			CAM_DEBUG("sr352_set_preview_RETURN PREVIEW _ MOVIE MODE 1 ");
			sr352_WRITE_LIST_BURST(sr352_Init_Reg);
			sr352_WRT_LIST(sr352_preview);
			sr352_set_preview_size(sr352_ctrl->settings.preview_size_idx);
			movie_mode = 0;
		}else{
		sr352_WRT_LIST(sr352_preview);
		sr352_set_preview_size(sr352_ctrl->settings.preview_size_idx);
		}

		if(sr352_ctrl->settings.effect!= CAMERA_EFFECT_OFF){
			sr352_set_effect(sr352_ctrl->settings.effect);
		}else{
			sr352_set_effect(CAMERA_EFFECT_OFF);
		}
		if(sr352_ctrl->settings.wb != CAMERA_WHITE_BALANCE_AUTO){
			sr352_set_whitebalance(sr352_ctrl->settings.wb);
		}else{
			sr352_set_whitebalance(CAMERA_WHITE_BALANCE_AUTO);
		}				
		if(sr352_ctrl->settings.brightness != CAMERA_EV_DEFAULT){
			sr352_set_ev(sr352_ctrl->settings.brightness);
		}else{
			sr352_set_ev(CAMERA_EV_DEFAULT);
		}

		}

			
		sr352_ctrl->op_mode = CAMERA_MODE_PREVIEW;
		start_flag = 0;

		/*if (sr352_ctrl->mirror_mode == 1)
				sr352_set_flip( \
				sr352_ctrl->mirror_mode);*/
}



void sr352_set_capture(void)
{
	CAM_DEBUG("");
	sr352_ctrl->op_mode = CAMERA_MODE_CAPTURE;

	sr352_WRT_LIST(sr352_Capture);
    sr352_WRT_LIST(sr352_Capture_2048_1536);
	//msleep(100);
//	sr352_WRT_LIST(sr352_stream_stop);
	//sr352_WRT_LIST(sr352_preview_640_480);
	sr352_set_exif();
	mode_enable = false;

}

static void sr352_set_scene_mode(int mode)
{
	CAM_DEBUG(" %d", mode);
	if( sr352_ctrl->settings.scenemode == mode) {
		printk("SCENE MODE IS SAME WITH BEFORE");
		return;
	}
/* kk0704.park :: sensor company request */
	/*if (mode != CAMERA_SCENE_OFF && mode != CAMERA_SCENE_AUTO)
		testcamera_WRITE_LIST(testcamera_Scene_Default);*///repeat set
	switch (mode) {

	case CAMERA_SCENE_OFF:
	case CAMERA_SCENE_AUTO:
		sr352_WRT_LIST(sr352_SceneOff);
		break;

	case CAMERA_SCENE_LANDSCAPE:
		sr352_WRT_LIST(sr352_Landscape);
		break;

	case CAMERA_SCENE_DAWN:
		sr352_WRT_LIST(sr352_Dawn);
		break;

	case CAMERA_SCENE_BEACH:
		sr352_WRT_LIST(sr352_Beach);
		break;

	case CAMERA_SCENE_SUNSET:
		sr352_WRT_LIST(sr352_Sunset);
		break;

	case CAMERA_SCENE_NIGHT:
		sr352_WRT_LIST(sr352_Nightshot);
		break;

	case CAMERA_SCENE_PORTRAIT:
		sr352_WRT_LIST(sr352_Portrait);
		break;

	case CAMERA_SCENE_AGAINST_LIGHT:
		CAM_DEBUG("SCENE AGAINST_LIGHT SETTING is same with Scene Default");
/* kk0704.park :: sensor company request 
    Against Light setting is same with Scene_Default */
		sr352_WRT_LIST(sr352_Backlight);	
		break;

	case CAMERA_SCENE_SPORT:
		sr352_WRT_LIST(sr352_Sports);
		break;

	case CAMERA_SCENE_FALL:
		sr352_WRT_LIST(sr352_Fall);
		break;

	/*case CAMERA_SCENE_TEXT:
		sr352_WRT_LIST(sr352_Text);
		break;*/

	case CAMERA_SCENE_CANDLE:
		sr352_WRT_LIST(sr352_Candle);
		break;

	case CAMERA_SCENE_FIRE:
		sr352_WRT_LIST(sr352_Firework);
		break;

	case CAMERA_SCENE_PARTY:
		sr352_WRT_LIST(sr352_Party);
		break;

	default:
		cam_info(" scene : default");
		break;
	}


	sr352_ctrl->settings.scenemode = mode;
}

int sr352_set_effect(int effect)
{
	CAM_DEBUG(" %d", effect);
	/*if (!mode_enable)
		goto effect_end;*/

	switch (effect) {
	case CAMERA_EFFECT_OFF:
		sr352_WRT_LIST(sr352_effect_none);
		break;

	case CAMERA_EFFECT_MONO:
		sr352_WRT_LIST(sr352_effect_gray);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		sr352_WRT_LIST(sr352_effect_negative);
		break;

	case CAMERA_EFFECT_SEPIA:
		sr352_WRT_LIST(sr352_effect_sepia);
		break;

	default:
		cam_info(" effect : default");
		sr352_WRT_LIST(sr352_effect_none);
		return 0;
	}

//effect_end:
	sr352_ctrl->settings.effect = effect;

	return 0;
}


void sr352_set_ev(int ev)
{
	CAM_DEBUG("[sr352] %s : %d", __func__, ev);
	/*if (!mode_enable)
		goto ev_end;*/
	
	switch (ev) {
	case CAMERA_EV_M4:
		sr352_WRT_LIST(sr352_bright_m4);
		break;

	case CAMERA_EV_M3:
		sr352_WRT_LIST(sr352_bright_m3);
		break;

	case CAMERA_EV_M2:
		sr352_WRT_LIST(sr352_bright_m2);
		break;

	case CAMERA_EV_M1:
		sr352_WRT_LIST(sr352_bright_m1);
		break;

	case CAMERA_EV_DEFAULT:
		sr352_WRT_LIST(sr352_bright_default);
		break;

	case CAMERA_EV_P1:
		sr352_WRT_LIST(sr352_bright_p1);
		break;

	case CAMERA_EV_P2:
		sr352_WRT_LIST(sr352_bright_p2);
		break;

	case CAMERA_EV_P3:
		sr352_WRT_LIST(sr352_bright_p3);
		break;

	case CAMERA_EV_P4:
		sr352_WRT_LIST(sr352_bright_p4);
		break;

	default:
		CAM_DEBUG("[sr352] unexpected ev mode %s/%d",
			__func__, __LINE__);
		break;
	}

//ev_end:
	sr352_ctrl->settings.brightness = ev;
}

int sr352_set_whitebalance(int wb)
{
	CAM_DEBUG(" %d", wb);

	/*if (!mode_enable)
		goto whitebalance_end;*/


	switch (wb) {
	case CAMERA_WHITE_BALANCE_AUTO:
		sr352_WRT_LIST(sr352_wb_auto);
		break;

	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		sr352_WRT_LIST(sr352_wb_incandescent);
		break;

	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		sr352_WRT_LIST(sr352_wb_fluorescent);
		break;

	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		sr352_WRT_LIST(sr352_wb_sunny);
		break;

	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		sr352_WRT_LIST(sr352_wb_cloudy);
		break;

	default:
		cam_info(" WB : default");
		return 0;
	}

//whitebalance_end:
	sr352_ctrl->settings.wb = wb;

	return 0;
}

static void sr352_set_metering(int mode)
{
	CAM_DEBUG(" %d", mode);
	/*if (!mode_enable)
	goto metering_end;*/
	
	switch (mode) {
	case CAMERA_CENTER_WEIGHT:
		sr352_WRT_LIST(sr352_metering_center);
		break;

	case CAMERA_AVERAGE:
		sr352_WRT_LIST(sr352_metering_matrix);
		break;

	case CAMERA_SPOT:
		sr352_WRT_LIST(sr352_metering_spot);
		break;

	default:
		cam_info(" AE : default");
		break;
	}
	
//metering_end:
	sr352_ctrl->settings.metering = mode;
}

static int32_t sr352_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0;

	CAM_DEBUG("Start");

	switch (update_type) {
	case REG_INIT:
		break;

	case UPDATE_PERIODIC:
		msm_sensor_enable_debugfs(sr352_ctrl->s_ctrl);
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			CAM_DEBUG("UPDATE_PERIODIC");

				if (sr352_ctrl->s_ctrl->sensordata->
					sensor_platform_info->
					csi_lane_params != NULL) {
					CAM_DEBUG(" lane_assign ="\
						" 0x%x",
						sr352_ctrl->s_ctrl->
						sensordata->
						sensor_platform_info->
						csi_lane_params->
						csi_lane_assign);

					CAM_DEBUG(" lane_mask ="\
						" 0x%x",
						sr352_ctrl->s_ctrl->
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
static int32_t sr352_video_config(int mode)
{
	int32_t	rc = 0;

	if (sr352_sensor_setting(UPDATE_PERIODIC, RES_PREVIEW) < 0)
		rc = -1;

	CAM_DEBUG("hoon rc : %d", rc);

	return rc;
}
*/
static long sr352_set_sensor_mode(int mode)
{
	CAM_DEBUG("%d", mode);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
	case SENSOR_VIDEO_MODE:
		sr352_set_preview();
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		sr352_set_capture();
		break;
	default:
		return 0;
	}
	return 0;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static int sr352_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	cam_info(" Nothing");

	return rc;
}

static void sr352_check_dataline(int val)
{
	if (val) {
		CAM_DEBUG(" DTP ON");
		sr352_ctrl->dtpTest = 1;

	} else {
		CAM_DEBUG(" DTP OFF");
		sr352_ctrl->dtpTest = 0;
	}
}

#if defined(CONFIG_ISX012) || defined(CONFIG_SR352)
static int sr352_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int temp = 0;

	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;


#ifdef CONFIG_LOAD_FILE
	sr352_regs_table_init();
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

	usleep(1200);

	/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(GPIO_MAIN_CAM_MCLK, 1,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(dev,cam_clk_info, 
		s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0)
		cam_err(" Mclk enable failed");

	if (rc != 0)
		goto FAIL_END;

	usleep(15);

	/*3M standy  */
	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 1);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	cam_err(" CAM_3M_STBY : %d", temp);

	msleep(10);

	/*3M reset */
	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 1);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("CAM_3M_RST : %d", temp);
	usleep(1000);


	/* sensor validation test */
	CAM_DEBUG(" Camera Sensor Validation Test");
	rc = sr352_WRT_LIST(sr352_stream_stop);
	if (rc < 0) {
		pr_info("Error in Camera Sensor Validation Test");
		goto FAIL_END;
	}
	
	sr352_set_init_mode();
	mode_enable = false;

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
static int sr352_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	printk(KERN_DEBUG "sr352_sensor_power_up");

	return 0;
}
#endif

void sensor_native_control(void __user *arg)
{
	struct ioctl_native_cmd ctrl_info;

	struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;

	if (copy_from_user(&ctrl_info,
		(void __user *)ioctl_ptr->ioctl_ptr,
		sizeof(ctrl_info))) {
		cam_err("fail copy_from_user!");
	}
	cam_info("mode : %d", ctrl_info.mode);

	
	switch (ctrl_info.mode) {

	case EXT_CAM_EV:
		sr352_set_ev(ctrl_info.value_1);
		break;

	case  EXT_CAM_MOVIE_MODE:
		CAM_DEBUG("MOVIE mode : %d", ctrl_info.value_1);
		sr352_ctrl->cam_mode = ctrl_info.value_1;
		if((sr352_ctrl->cam_mode==1) && (start_flag == 0)){
			sr352_WRT_LIST(sr352_Init_Reg);     //eunice
		}
		//start_flag = 0;
		//sr352_WRT_LIST(sr352_preview_640_480);
		break;

	case EXT_CAM_EXIF:
		ctrl_info.value_1 = sr352_get_exif(ctrl_info.address,
			ctrl_info.value_2);
		break;
		
	case EXT_CAM_EFFECT:
		sr352_set_effect(ctrl_info.value_1);
		break;

	case EXT_CAM_SCENE_MODE:
		printk("EXT_CAM_SCENE_MODE\n");
		sr352_set_scene_mode(ctrl_info.value_1);
		break;

	case EXT_CAM_METERING:
		printk("EXT_CAM_METERING\n");
		sr352_set_metering(ctrl_info.value_1);
		break;

	case EXT_CAM_WB:
		sr352_set_whitebalance(ctrl_info.value_1);
		break;

	case EXT_CAM_DTP_TEST:
		sr352_check_dataline(ctrl_info.value_1);
		break;

	case EXT_CAM_PREVIEW_SIZE:
		sr352_set_preview_size(ctrl_info.value_1);
		sr352_ctrl->settings.preview_size_idx = ctrl_info.value_1;
		break;

	case CFG_SET_PICTURE_SIZE:
		printk("CFG_SET_PICTURE_SIZE\n");
		//sr352_set_picture_size(ctrl_info.value_1);
		break;

	case EXT_CAM_SET_FPS:
		printk("EXT_CAM_SET_FPS\n");
		//sr352_set_frame_rate(ctrl_info.value_1);   eunice
		sr352_ctrl->settings.fps = ctrl_info.value_1;
		break;

	default:

		CAM_DEBUG("[sr352] default mode");
		break;
	}
	if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
		(const void *)&ctrl_info,
			sizeof(ctrl_info))) {
		cam_err("fail copy_to_user!");
	}

}

long sr352_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	void __user *argp = (void __user *)arg;
	struct msm_sensor_ctrl_t *sr352_s_ctrl = get_sctrl(sd);

	CAM_DEBUG("sr352_sensor_subdev_ioctl\n");
	CAM_DEBUG("%s: cmd %d\n", __func__, _IOC_NR(cmd));
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return sr352_sensor_config(sr352_s_ctrl, argp);
	/*case VIDIOC_MSM_SENSOR_RELEASE:*/
	/*	return msm_sensor_release(sr352_s_ctrl);*/
	case VIDIOC_MSM_SENSOR_CSID_INFO:
		{
		struct msm_sensor_csi_info *csi_info =
			(struct msm_sensor_csi_info *)arg;
		/*sr352_s_ctrl->csid_version = csi_info->csid_version;*/
		sr352_s_ctrl->is_csic = csi_info->is_csic;
		return 0;
		}
	default:
		return -ENOIOCTLCMD;
	}

}

int sr352_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
			rc = sr352_set_sensor_mode(cfg_data.mode);
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
			rc = sr352_sensor_setting(UPDATE_PERIODIC,
				RES_PREVIEW);
		break;
	case CFG_SET_MODE:
		rc = sr352_set_sensor_mode(cfg_data.mode);
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

static int sr352_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int temp = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	CAM_DEBUG("=== POWER DOWN Start ===");

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_reset, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_reset);
	CAM_DEBUG("CAM_3M_RST : %d", temp);

	/*CAM_MCLK0*/
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	gpio_tlmm_config(GPIO_CFG(GPIO_MAIN_CAM_MCLK, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
//	gpio_tlmm_config(GPIO_CFG(data->sensor_platform_info->mclk, 0,
//		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
//		GPIO_CFG_ENABLE);

	usleep(10);

	gpio_set_value_cansleep(data->sensor_platform_info->sensor_stby, 0);
	temp = gpio_get_value(data->sensor_platform_info->sensor_stby);
	CAM_DEBUG("CAM_3M_ISP_INIT : %d", temp);

	usleep(1200); /* 20clk = 0.833us */

	data->sensor_platform_info->sensor_power_off(1);

	msm_camera_request_gpio_table(data, 0);

#ifdef CONFIG_LOAD_FILE
	sr352_regs_table_exit();
#endif
	return rc;
}

static struct v4l2_subdev_info sr352_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static int sr352_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code) {
	CAM_DEBUG("Index is %d", index);
	if ((unsigned int)index >= ARRAY_SIZE(sr352_subdev_info))
		return -EINVAL;

	*code = sr352_subdev_info[index].code;
	return 0;
}

void sr352_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
#if 0
	
	CAM_DEBUG("Start_stream");
	stop_stream = 0;

	if (sr352_ctrl->op_mode == CAMERA_MODE_CAPTURE){
		CAM_DEBUG("Start_stream : CAMERA_MODE_CAPTURE");
		return;
		}
	
	if (sr352_ctrl->cam_mode == MOVIE_MODE) {
		CAM_DEBUG("VGA recording");
		if (sr352_ctrl->op_mode == CAMERA_MODE_INIT ||
			sr352_ctrl->op_mode == CAMERA_MODE_PREVIEW) {
		//	sr352_WRT_LIST(sr352_Init_Reg);
		//	sr352_WRT_LIST(sr352_recording_50Hz_25fps);

			/*if (sr352_ctrl->mirror_mode == 1)
					sr352_set_flip( \
					sr352_ctrl->mirror_mode);*/
			}
		} else {
		CAM_DEBUG("VGA recording222");
	if (sr352_ctrl->op_mode == CAMERA_MODE_INIT ||
			sr352_ctrl->op_mode == CAMERA_MODE_RECORDING ||
			sr352_ctrl->op_mode == CAMERA_MODE_PREVIEW) {
		//sr352_WRT_LIST(sr352_Init_Reg);
		//sr352_WRT_LIST(sr352_preview_640_480);     //eunice

		/*if (sr352_ctrl->mirror_mode == 1)
				sr352_set_flip( \
				sr352_ctrl->mirror_mode);*/
			}
		}

#endif
}

void sr352_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
   /* if (sr352_ctrl->op_mode == CAMERA_MODE_CAPTURE)
		return 0;*/

	stop_stream = 1;
	if (sr352_ctrl->op_mode == CAMERA_MODE_INIT )
        return;

	cam_err(" sr352_sensor_stop_stream E");
	if(sr352_ctrl->op_mode == CAMERA_MODE_RECORDING){
	sr352_WRT_LIST(sr352_stream_stop);
	cam_err(" sr352_sensor_stop_stream  CAMERA_MODE_RECORDING E");
	sr352_ctrl->op_mode = CAMERA_MODE_PREVIEW;
	}
	//sr352_WRT_LIST(sr352_preview_640_480);
	cam_err(" sr352_sensor_stop_stream X");
   
}

static struct v4l2_subdev_core_ops sr352_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = sr352_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops sr352_subdev_video_ops = {
	.enum_mbus_fmt = sr352_enum_fmt,
};

static struct v4l2_subdev_ops sr352_subdev_ops = {
	.core = &sr352_subdev_core_ops,
	.video  = &sr352_subdev_video_ops,
};

static int sr352_i2c_probe(struct i2c_client *client,
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

	sr352_client = client;
	sr352_dev = s_ctrl->sensor_i2c_client->client->dev;

	sr352_ctrl = kzalloc(sizeof(struct sr352_ctrl), GFP_KERNEL);
	if (!sr352_ctrl) {
		CAM_DEBUG("sr352_ctrl alloc failed!\n");
		return -ENOMEM;
	}

	sr352_exif = kzalloc(sizeof(struct sr352_exif_data),
		GFP_KERNEL);
	if (!sr352_exif) {
		cam_err("Cannot allocate memory fo EXIF structure!");
		kfree(sr352_exif);
		rc = -ENOMEM;
	}

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);

	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		&sr352_subdev_ops);

	sr352_ctrl->s_ctrl = s_ctrl;
	sr352_ctrl->sensor_dev = &s_ctrl->sensor_v4l2_subdev;
	sr352_ctrl->sensordata = client->dev.platform_data;

	rc = msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);

	if (rc < 0) {
		cam_err("msm_sensor_register failed!");
		kfree(sr352_exif);
		kfree(sr352_ctrl);
		goto probe_failure;
		}

	cam_err("sr352_probe succeeded!");
	return 0;

probe_failure:
	cam_err("sr352_probe failed!");
	return rc;
}

static const struct i2c_device_id sr352_i2c_id[] = {
	{"sr352", (kernel_ulong_t)&sr352_s_ctrl},
	{},
};

static struct msm_camera_i2c_client sr352_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,  //eunice
};

static struct i2c_driver sr352_i2c_driver = {
	.id_table = sr352_i2c_id,
	.probe  = sr352_i2c_probe,
	.driver = {
		.name = "sr352",
	},
};

static int __init sr352_init(void)
{
	return i2c_add_driver(&sr352_i2c_driver);
}

static struct msm_sensor_fn_t sr352_func_tbl = {
	.sensor_config = sr352_sensor_config,
	.sensor_power_up = sr352_sensor_power_up,
	.sensor_power_down = sr352_sensor_power_down,
	.sensor_match_id = sr352_sensor_match_id,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_start_stream = sr352_sensor_start_stream,
	.sensor_stop_stream = sr352_sensor_stop_stream,
	/*.sensor_get_lens_info = sensor_get_lens_info,*/
};


static struct msm_sensor_reg_t sr352_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_sensor_ctrl_t sr352_s_ctrl = {
	.msm_sensor_reg = &sr352_regs,
	.sensor_i2c_client = &sr352_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	/*.csic_params = &sr352_csic_params_array[0],
	.csi_params = &sr352_csi_params_array[0],*/
	.msm_sensor_mutex = &sr352_mut,
	.sensor_i2c_driver = &sr352_i2c_driver,
	.sensor_v4l2_subdev_info = sr352_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sr352_subdev_info),
	.sensor_v4l2_subdev_ops = &sr352_subdev_ops,
	.func_tbl = &sr352_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};


module_init(sr352_init);
MODULE_DESCRIPTION("Samsung SR352 Camera driver");
MODULE_LICENSE("GPL v2");

