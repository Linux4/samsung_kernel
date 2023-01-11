/*****************************************************************************
 *
 * Copyright (c) 2013 mCube, Inc.  All rights reserved.
 *
 * This source is subject to the mCube Software License.
 * This software is protected by Copyright and the information and source code
 * contained herein is confidential. The software including the source code
 * may not be copied and the information contained herein may not be used or
 * disclosed except with the written permission of mCube Inc.
 *
 * All other rights reserved.
 *
 * This code and information are provided "as is" without warranty of any
 * kind, either expressed or implied, including but not limited to the
 * implied warranties of merchantability and/or fitness for a
 * particular purpose.
 *
 * The following software/firmware and/or related documentation ("mCube
 * Software") have been modified by mCube Inc. All revisions are subject to
 * any receiver's applicable license agreements with mCube Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include <linux/regulator/consumer.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/i2c/mc3xxx.h>


#define MC3XXX_XOUT_REG			0x00
#define MC3XXX_YOUT_REG			0x01
#define MC3XXX_ZOUT_REG			0x02
#define MC3XXX_TILT_REG			0x03
#define MC3XXX_OPSTAT_REG		0x04
#define MC3XXX_SC_REG			0x05
#define MC3XXX_INTEN_REG		0x06
#define MC3XXX_MODE_REG			0x07
#define MC3XXX_SAMPR_REG		0x08
#define MC3XXX_TAPEN_REG		0x09
#define MC3XXX_TAPP_REG			0x0A
#define MC3XXX_DROP_REG			0x0B
#define MC3XXX_SHDB_REG			0x0C
#define MC3XXX_XOUT_EX_L_REG		0x0D
#define MC3XXX_XOUT_EX_H_REG		0x0E
#define MC3XXX_YOUT_EX_L_REG		0x0F
#define MC3XXX_YOUT_EX_H_REG		0x10
#define MC3XXX_ZOUT_EX_L_REG		0x11
#define MC3XXX_ZOUT_EX_H_REG		0x12

#define MC3XXX_CHIPID_REG		0x18

#define MC3XXX_OUTCFG_REG		0x20
#define MC3XXX_XOFFL_REG		0x21
#define MC3XXX_XOFFH_REG		0x22
#define MC3XXX_YOFFL_REG		0x23
#define MC3XXX_YOFFH_REG		0x24
#define MC3XXX_ZOFFL_REG		0x25
#define MC3XXX_ZOFFH_REG		0x26
#define MC3XXX_XGAIN_REG		0x27
#define MC3XXX_YGAIN_REG		0x28
#define MC3XXX_ZGAIN_REG		0x29

#define MC3XXX_SHAKE_TH_REG		0x2B
#define MC3XXX_UD_Z_TH_REG		0x2C
#define MC3XXX_UD_X_TH_REG		0x2D
#define MC3XXX_RL_Z_TH_REG		0x2E
#define MC3XXX_RL_Y_TH_REG		0x2F
#define MC3XXX_FB_Z_TH_REG		0x30
#define MC3XXX_DROP_TH_REG		0x31
#define MC3XXX_TAP_TH_REG		0x32

#define MC3XXX_PCODE_REG		0x3B

#define MC3XXX_MODE_AUTO		0
#define MC3XXX_MODE_WAKE		1
#define MC3XXX_MODE_SNIFF		2
#define MC3XXX_MODE_STANDBY		3

#define MC3XXX_RANGE_2G			0
#define MC3XXX_RANGE_4G			1
#define MC3XXX_RANGE_8G_10BIT		2
#define MC3XXX_RANGE_8G_14BIT		3

#define MC3XXX_BW_512HZ			0
#define MC3XXX_BW_256HZ			1
#define MC3XXX_BW_128HZ			2
#define MC3XXX_BW_64HZ			3
#define MC3XXX_BW_32HZ			4
#define MC3XXX_BW_16HZ			5
#define MC3XXX_BW_8HZ			6

#define MC3XXX_RANGE_SET		MC3XXX_RANGE_8G_14BIT /* +/-8g,14bit */
#define MC3XXX_BW_SET			MC3XXX_BW_128HZ /* 128HZ  */
#define MC3XXX_MAX_DELAY		200
#define ABSMIN_8G			(-8 * 1024)
#define ABSMAX_8G			(8 * 1024)
#define ABSMIN_1_5G			(-128)
#define ABSMAX_1_5G			(128)

/* 1g constant value */
#define GRAVITY_1G_VALUE			1024

/* product code */
#define MC3XXX_PCODE_3210		0x90
#define MC3XXX_PCODE_3230		0x19
#define MC3XXX_PCODE_3250		0x88
#define MC3XXX_PCODE_3410		0xA8
#define MC3XXX_PCODE_3410N		0xB8
#define MC3XXX_PCODE_3430		0x29
#define MC3XXX_PCODE_3430N		0x39
#define MC3XXX_PCODE_3510B		0x40
#define MC3XXX_PCODE_3530B		0x30
#define MC3XXX_PCODE_3510C		0x10
#define MC3XXX_PCODE_3530C		0x60

#define MC3XXX_IOC_MAGIC		77 /* 0x1D */
#define MC3XXX_ACC_IOCTL_SET_DELAY	_IOW(MC3XXX_IOC_MAGIC, 0, int)
#define MC3XXX_ACC_IOCTL_GET_DELAY	_IOR(MC3XXX_IOC_MAGIC, 1, int)
#define MC3XXX_ACC_IOCTL_SET_ENABLE	_IOW(MC3XXX_IOC_MAGIC, 2, int)
#define MC3XXX_ACC_IOCTL_GET_ENABLE	_IOR(MC3XXX_IOC_MAGIC, 3, int)
#define MC3XXX_ACC_IOCTL_CALIBRATION	_IOW(MC3XXX_IOC_MAGIC, 4, int)


#define delay_to_jiffies(d) ((d)?msecs_to_jiffies(d):1)
#define actual_delay(d)     (jiffies_to_msecs(delay_to_jiffies(d)))

enum mc3xxx_orientation {
	MC3XXX_TOP_LEFT_DOWN = 0,
	MC3XXX_TOP_RIGHT_DOWN,
	MC3XXX_TOP_RIGHT_UP,
	MC3XXX_TOP_LEFT_UP,
	MC3XXX_BOTTOM_LEFT_DOWN,
	MC3XXX_BOTTOM_RIGHT_DOWN,
	MC3XXX_BOTTOM_RIGHT_UP,
	MC3XXX_BOTTOM_LEFT_UP
};

enum mc3xxx_axis {
	MC3XXX_AXIS_X = 0,
	MC3XXX_AXIS_Y,
	MC3XXX_AXIS_Z,
	MC3XXX_AXIS_NUM
};

struct mc3xxxacc {
	signed short x;
	signed short y;
	signed short z;
};

struct mc3xxx_hwmsen_convert {
	signed char sign[3];
	unsigned char map[3];
};

struct mc3xxx_data {
	atomic_t enable;                /* attribute value */
	atomic_t delay;                 /* attribute value */
	atomic_t position;              /* attribute value */
	atomic_t threshold;             /* attribute value */
	struct mc3xxxacc value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;
	unsigned char product_code;
	unsigned char resolution;
};

/******************************
 *** CONFIGURATIONS
 ******************************/
#define MCUBE_DOT_CALIBRATION
/* #define MCUBE_FUNC_DEBUG */
/* #define I2C_BUS_NUM_STATIC_ALLOC */
#define MCUBE_I2C_BURST_LIMIT_2_BYTES
/* #define SUPPORT_VPROXIMITY_SENSOR */

#define MC3XXX_DEV_NAME       MC3XXX_ACC_I2C_NAME
#define MC3XXX_DEV_VERSION    "2.0.3"
#define MC3XXX_INPUT_NAME     "accelerometer"
#define MC3XXX_I2C_ADDR       MC3XXX_ACC_I2C_ADDR

static unsigned char mc3xxx_current_placement = MC3XXX_BOTTOM_LEFT_UP;

#ifdef MCUBE_FUNC_DEBUG
#define MC_PRINT(x...)        printk(x)
#define MC_ERR_PRINT(x...)    printk(x)
#else
#define MC_PRINT(x...)
#define MC_ERR_PRINT(x...)
#endif

/* Transformation matrix for chip mounting position */
static const struct mc3xxx_hwmsen_convert mc3xxx_cvt[] = {
	/* 0: top   , left-down */
	{{  1,   1,   1 }, { MC3XXX_AXIS_X,  MC3XXX_AXIS_Y,  MC3XXX_AXIS_Z} },
	/* 1: top   , right-down */
	{{ -1,   1,   1 }, { MC3XXX_AXIS_Y,  MC3XXX_AXIS_X,  MC3XXX_AXIS_Z} },
	/* 2: top   , right-up */
	{{ -1,  -1,   1 }, { MC3XXX_AXIS_X,  MC3XXX_AXIS_Y,  MC3XXX_AXIS_Z} },
	/* 3: top   , left-up */
	{{  1,  -1,   1 }, { MC3XXX_AXIS_Y,  MC3XXX_AXIS_X,  MC3XXX_AXIS_Z} },
	/* 4: bottom, left-down */
	{{ -1,   1,  -1 }, { MC3XXX_AXIS_X,  MC3XXX_AXIS_Y,  MC3XXX_AXIS_Z} },
	/* 5: bottom, right-down */
	{{  1,   1,  -1 }, { MC3XXX_AXIS_Y,  MC3XXX_AXIS_X,  MC3XXX_AXIS_Z} },
	/* 6: bottom, right-up */
	{{  1,  -1,  -1 }, { MC3XXX_AXIS_X,  MC3XXX_AXIS_Y,  MC3XXX_AXIS_Z} },
	/* 7: bottom, left-up */
	{{ -1,  -1,  -1 }, { MC3XXX_AXIS_Y,  MC3XXX_AXIS_X,  MC3XXX_AXIS_Z} },
};

static struct i2c_client *mc3xxx_client;

static int mc3xxx_data_resolution[4] = {
	256, /* +/- 2g, 10bit */
	128, /* +/- 4g, 10bit */
	64,  /* +/- 8g, 10bit */
	1024 /* +/- 8g, 14bit */
};

static GSENSOR_VECTOR3D mc3xxx_gain = {0};

#ifdef I2C_BUS_NUM_STATIC_ALLOC
#define I2C_STATIC_BUS_NUM       0

static struct i2c_board_info  mc3xxx_i2c_boardinfo = {
	I2C_BOARD_INFO(MC3XXX_DEV_NAME, MC3XXX_I2C_ADDR)
};
#endif

static unsigned char s_bPCODE      = 0x00;
static unsigned short mc3xxx_i2c_auto_probe_addr[] = {
		0x4C, 0x6C, 0x4E, 0x6D, 0x6E, 0x6F };

#define REMAP_IF_MC3250_READ(nDataX, nDataY)	do {		\
		if (MC3XXX_PCODE_3250 == s_bPCODE) {		\
			int _nTemp = 0;				\
			_nTemp = nDataX;			\
			nDataX = nDataY;			\
			nDataY = -_nTemp;			\
		}						\
	} while (0)

#define REMAP_IF_MC3250_WRITE(nDataX, nDataY)	do {		\
		if (MC3XXX_PCODE_3250 == s_bPCODE) {		\
			int _nTemp = 0;				\
			_nTemp = nDataX;			\
			nDataX = -nDataY;			\
			nDataY = _nTemp;			\
		}						\
	} while (0)

#define REMAP_IF_MC34XX_N(nDataX, nDataY)	do {		\
		if ((MC3XXX_PCODE_3410N == s_bPCODE) ||		\
			(MC3XXX_PCODE_3430N == s_bPCODE)) {	\
			nDataX = -nDataX;			\
			nDataY = -nDataY;			\
		}						\
	} while (0)

#define IS_MC35XX()                            \
	((MC3XXX_PCODE_3510B == s_bPCODE) ||   \
	 (MC3XXX_PCODE_3510C == s_bPCODE) ||   \
	 (MC3XXX_PCODE_3530B == s_bPCODE) ||   \
	 (MC3XXX_PCODE_3530C == s_bPCODE))

#define REMAP_IF_MC35XX(nDataX, nDataY)		do {		\
		if (IS_MC35XX()) {				\
			nDataX = -nDataX;			\
			nDataY = -nDataY;			\
		}						\
	} while (0)


#ifdef MCUBE_DOT_CALIBRATION
#define DATA_PATH			"/sdcard/mcube-register-map.txt"
static char file_path[128] = "/data/data/com.mcube.acc/files/mcube-calib.txt";

static SENSOR_DATA gain_data = {0}, offset_data = {0};
static unsigned char offset_buf[9] = {0};
static struct file *fd_file;
static int load_cali_cnt = 30;
static bool IsRbmMode;
static mm_segment_t oldfs;
static SENSOR_DATA mc3xxx_cali_data = {0};

static void mc3xxx_read_cali_file(struct mc3xxx_data *mc3xxx);
static int mc3xxx_read_true_data(struct mc3xxx_data *mc3xxx,
		struct mc3xxxacc *acc);
#endif

static inline int mc3xxx_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	signed int dummy = 0;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
		return dummy;
	*data = dummy & 0x000000ff;

	return 0;
}

static inline int mc3xxx_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	signed int dummy = 0;

	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
	if (dummy < 0)
		return dummy;
	return 0;
}

static inline int mc3xxx_smbus_read_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	signed int dummy = 0;

#ifdef MCUBE_I2C_BURST_LIMIT_2_BYTES
	for (; len > 1; (len -= 2)) {
		dummy = i2c_smbus_read_i2c_block_data(client,
				reg_addr, 2, data);
		if (dummy < 0)
			return dummy;
		reg_addr += 2;
		data += 2;
	}

	if (0 == len)
		return 0;
#endif

	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return dummy;

	return 0;
}

static inline int mc3xxx_smbus_write_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	signed int dummy = 0;

#ifdef MCUBE_I2C_BURST_LIMIT_2_BYTES
	for (; len > 1; (len -= 2)) {
		dummy = i2c_smbus_write_i2c_block_data(client,
				reg_addr, 2, data);
		if (dummy < 0)
			return dummy;
		reg_addr += 2;
		data += 2;
	}

	if (0 == len)
		return 0;
#endif

	dummy = i2c_smbus_write_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return dummy;
	return 0;
}

static bool mc3xxx_is_high_end(unsigned char product_code)
{
	if ((MC3XXX_PCODE_3230 == product_code)
			|| (MC3XXX_PCODE_3430 == product_code)
			|| (MC3XXX_PCODE_3430N == product_code)
			|| (MC3XXX_PCODE_3530B == product_code)
			|| (MC3XXX_PCODE_3530C == product_code))
		return false;
	else
		return true;
}

static bool mc3xxx_is_mc3510(unsigned char product_code)
{
	if ((MC3XXX_PCODE_3510B == product_code)
			|| (MC3XXX_PCODE_3510C == product_code))
		return true;
	else
		return false;
}

static bool mc3xxx_is_mc3530(unsigned char product_code)
{
	if ((MC3XXX_PCODE_3530B == product_code)
			|| (MC3XXX_PCODE_3530C == product_code))
		return true;
	else
		return false;
}

static bool mc3xxx_validate_pcode(unsigned char bPCode)
{
	if ((MC3XXX_PCODE_3210 == bPCode) || (MC3XXX_PCODE_3230 == bPCode)
			|| (MC3XXX_PCODE_3250 == bPCode)
			|| (MC3XXX_PCODE_3410 == bPCode)
			|| (MC3XXX_PCODE_3430  == bPCode)
			|| (MC3XXX_PCODE_3410N == bPCode)
			|| (MC3XXX_PCODE_3430N == bPCode)
			|| (MC3XXX_PCODE_3510B == bPCode)
			|| (MC3XXX_PCODE_3530B == bPCode)
			|| (MC3XXX_PCODE_3510C == bPCode)
			|| (MC3XXX_PCODE_3530C == bPCode))
		return true;

	return false;
}

static int mc3xxx_detect_pcode(struct i2c_client *client)
{
	int ret = 0;
	unsigned char product_code = 0;
	int _nProbeAddrCount = (sizeof(mc3xxx_i2c_auto_probe_addr)
			/ sizeof(mc3xxx_i2c_auto_probe_addr[0]));
	int _nCount = 0;

	MC_PRINT("%s: entered\n", __func__);

	for (_nCount = 0; _nCount < _nProbeAddrCount; _nCount++) {
		client->addr = mc3xxx_i2c_auto_probe_addr[_nCount];

		ret = mc3xxx_smbus_read_byte(client, MC3XXX_PCODE_REG,
				&product_code);
		if (ret) {
			MC_PRINT("%s: read error: %x\n", __func__, ret);
			continue;
		}

		if (true == mc3xxx_validate_pcode(product_code)) {
			s_bPCODE = product_code;
			return product_code;
		}
	}

	return -1;
}

static int mc3xxx_read_chip_id(struct i2c_client *client, char *buf)
{
	u8 bChipID[4] = { 0 };
	int res = 0;
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	res = mc3xxx_smbus_read_block(mc3xxx->client, 0x3C, bChipID, 4);

	if (res) {
		MC_ERR_PRINT("%s: i2c error!\n", __func__);
		bChipID[0] = 0;
		bChipID[1] = 0;
		bChipID[2] = 0;
		bChipID[3] = 0;
	}

	MC_PRINT("%s: %02X-%02X-%02X-%02X\n", __func__,
			bChipID[0], bChipID[1], bChipID[2], bChipID[3]);

	return sprintf(buf, "%02X-%02X-%02X-%02X\n",
			bChipID[0], bChipID[1], bChipID[2], bChipID[3]);
}

static int mc3xxx_set_mode(struct i2c_client *client, unsigned char mode)
{
	int comres = 0;
	unsigned char data = 0;

	if (4 > mode) {
		comres = mc3xxx_smbus_read_byte(client,
				MC3XXX_MODE_REG, &data);
		data &= ~0x03;
		data |= mode;

		comres += mc3xxx_smbus_write_byte(client,
				MC3XXX_MODE_REG, &data);
	} else {
		comres = -1;
	}

	return comres;
}

static int mc3xxx_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;
	unsigned char data = 0;

	comres = mc3xxx_smbus_read_byte(client, MC3XXX_MODE_REG, &data);

	*mode = data & 0x03;

	return comres;
}

static int mc3xxx_set_range(struct i2c_client *client, unsigned char range)
{
	int comres = 0;
	unsigned char data = 0;
	unsigned char pcode = 0;

	if (4 > range) {
		comres += mc3xxx_smbus_read_byte(client, MC3XXX_PCODE_REG,
				&pcode);
		if (mc3xxx_is_mc3510(pcode)) {
			data = 0x25;
			comres = mc3xxx_smbus_write_byte(client,
					MC3XXX_OUTCFG_REG, &data);
			mc3xxx_gain.x = mc3xxx_gain.y = mc3xxx_gain.z = 1024;
			return comres;
		} else if (mc3xxx_is_mc3530(pcode)) {
			data = 0x02;
			comres = mc3xxx_smbus_write_byte(client,
					MC3XXX_OUTCFG_REG, &data);
			mc3xxx_gain.x = mc3xxx_gain.y = mc3xxx_gain.z = 64;
			return comres;
		}

		if (mc3xxx_is_high_end(pcode)) {
			data = (range << 2) | 0x33;
			comres = mc3xxx_smbus_write_byte(client,
					MC3XXX_OUTCFG_REG, &data);
			if (0 == comres)
				mc3xxx_gain.x = mc3xxx_gain.y = mc3xxx_gain.z
					= mc3xxx_data_resolution[range];
		} else {
			mc3xxx_gain.x = mc3xxx_gain.y = mc3xxx_gain.z = 86;
		}
	} else {
		comres = -1;
	}

	return comres;
}

static int mc3xxx_get_range(struct i2c_client *client, unsigned char *range)
{
	int comres = 0;
	unsigned char data = 0;

	comres = mc3xxx_smbus_read_byte(client, MC3XXX_OUTCFG_REG, &data);
	*range = ((data >> 2) & 0x03);

	return comres;
}

static int mc3xxx_set_bandwidth(struct i2c_client *client, unsigned char BW)
{
	int comres = 0;
	unsigned char data = 0;

	if (7 > BW) {
		comres = mc3xxx_smbus_read_byte(client, MC3XXX_OUTCFG_REG,
				&data);
		data &= ~(0x07 << 4);
		data |= (BW << 4);
		comres += mc3xxx_smbus_write_byte(client, MC3XXX_OUTCFG_REG,
				&data);
	} else {
		comres = -1;
	}

	return comres;
}

static int mc3xxx_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres = 0;
	unsigned char data = 0;

	comres = mc3xxx_smbus_read_byte(client, MC3XXX_OUTCFG_REG, &data);
	*BW = ((data >> 4) & 0x07);

	return comres;
}

static int mc3xxx_init(struct mc3xxx_data *mc3xxx)
{
	int rc = 0;

	if (true == mc3xxx_is_high_end(mc3xxx->product_code)) {
		rc = mc3xxx_set_bandwidth(mc3xxx->client, MC3XXX_BW_SET);
		rc += mc3xxx_set_range(mc3xxx->client, MC3XXX_RANGE_SET);
	} else if (mc3xxx_is_mc3530(mc3xxx->product_code)) {
		rc = mc3xxx_set_range(mc3xxx->client, MC3XXX_RANGE_SET);
		mc3xxx_gain.x = mc3xxx_gain.y = mc3xxx_gain.z = 64;
	} else {
		mc3xxx_gain.x = mc3xxx_gain.y = mc3xxx_gain.z = 86;
	}

	return rc;
}

static int mc3xxx_read_accel_xyz(struct mc3xxx_data *mc3xxx,
		struct mc3xxxacc *acc, int orient)
{
	int comres = -1;
	unsigned char data[6] = { 0 };
	s16 raw[3] = { 0 };
	const struct mc3xxx_hwmsen_convert *pCvt = NULL;

	if (true == mc3xxx_is_high_end(mc3xxx->product_code)) {
		comres = mc3xxx_smbus_read_block(mc3xxx->client,
				MC3XXX_XOUT_EX_L_REG, data, 6);
		raw[0] = (s16)(data[0] + (data[1] << 8));
		raw[1] = (s16)(data[2] + (data[3] << 8));
		raw[2] = (s16)(data[4] + (data[5] << 8));
	} else {
		comres = mc3xxx_smbus_read_block(mc3xxx->client,
				MC3XXX_XOUT_REG, data, 3);
		raw[0] = (s8)data[0];
		raw[1] = (s8)data[1];
		raw[2] = (s8)data[2];
	}

	if (comres) {
		MC_ERR_PRINT("%s: i2c error!\n", __func__);
		return comres;
	}

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s: %d, %d, %d\n", __func__, raw[0], raw[1], raw[2]);
#endif

	raw[0] = raw[0] * GRAVITY_1G_VALUE / mc3xxx_gain.x;
	raw[1] = raw[1] * GRAVITY_1G_VALUE / mc3xxx_gain.y;
	raw[2] = raw[2] * GRAVITY_1G_VALUE / mc3xxx_gain.z;

	REMAP_IF_MC3250_READ(raw[0], raw[1]);
	REMAP_IF_MC34XX_N(raw[0], raw[1]);
	REMAP_IF_MC35XX(raw[0], raw[1]);

	pCvt = &mc3xxx_cvt[orient];
	acc->x = pCvt->sign[MC3XXX_AXIS_X] * raw[pCvt->map[MC3XXX_AXIS_X]];
	acc->y = pCvt->sign[MC3XXX_AXIS_Y] * raw[pCvt->map[MC3XXX_AXIS_Y]];
	acc->z = pCvt->sign[MC3XXX_AXIS_Z] * raw[pCvt->map[MC3XXX_AXIS_Z]];

	return comres;
}

static void mc3xxx_work_func(struct work_struct *work)
{
	static struct mc3xxxacc acc = { 0 };
	struct mc3xxx_data *mc3xxx = container_of((struct delayed_work *)work,
			struct mc3xxx_data, work);
	unsigned long delay = msecs_to_jiffies(atomic_read(&mc3xxx->delay));

#ifdef MCUBE_DOT_CALIBRATION
	mc3xxx_read_cali_file(mc3xxx);
	mc3xxx_read_true_data(mc3xxx, &acc);
#else
	mc3xxx_read_accel_xyz(mc3xxx, &acc, mc3xxx_current_placement);
#endif

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s: %d, %d, %d\n", __func__, acc.x, acc.y, acc.z);
#endif

	input_report_abs(mc3xxx->input, ABS_X, acc.x);
	input_report_abs(mc3xxx->input, ABS_Y, acc.y);
	input_report_abs(mc3xxx->input, ABS_Z, acc.z);
	input_sync(mc3xxx->input);

	mutex_lock(&mc3xxx->value_mutex);
	mc3xxx->value = acc;
	mutex_unlock(&mc3xxx->value_mutex);

	schedule_delayed_work(&mc3xxx->work, delay);
}

static int mc3xxx_input_init(struct mc3xxx_data *mc3xxx)
{
	struct input_dev *dev = NULL;
	int err = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s: can't allocate device!\n", __func__);
		return -ENOMEM;
	}

	dev->name = MC3XXX_INPUT_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);

	if (true == mc3xxx_is_high_end(mc3xxx->product_code)) {
		input_set_abs_params(dev, ABS_X, ABSMIN_8G, ABSMAX_8G, 0, 0);
		input_set_abs_params(dev, ABS_Y, ABSMIN_8G, ABSMAX_8G, 0, 0);
		input_set_abs_params(dev, ABS_Z, ABSMIN_8G, ABSMAX_8G, 0, 0);
	} else {
		input_set_abs_params(dev, ABS_X, ABSMIN_1_5G,
				ABSMAX_1_5G, 0, 0);
		input_set_abs_params(dev, ABS_Y, ABSMIN_1_5G,
				ABSMAX_1_5G, 0, 0);
		input_set_abs_params(dev, ABS_Z, ABSMIN_1_5G,
				ABSMAX_1_5G, 0, 0);
	}

	input_set_drvdata(dev, mc3xxx);

	err = input_register_device(dev);
	if (err < 0) {
		pr_err("%s: can't register device!\n", __func__);
		input_free_device(dev);
		return err;
	}

	mc3xxx->input = dev;

	return 0;
}

static void mc3xxx_input_deinit(struct mc3xxx_data *mc3xxx)
{
	struct input_dev *dev = mc3xxx->input;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	input_unregister_device(dev);
	input_free_device(dev);
}

static ssize_t mc3xxx_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int address = 0, value = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	sscanf(buf, "%d %d", &address, &value);
	if (0 != mc3xxx_smbus_write_byte(mc3xxx->client,
			(unsigned char)address, (unsigned char *)&value))
		return -EINVAL;
	return count;
}

static ssize_t mc3xxx_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	size_t count = 0;
	u8 reg[0x40] = { 0 };
	int i = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	for (i = 0; i < 0x40; i++) {
		mc3xxx_smbus_read_byte(mc3xxx->client, i, reg+i);

		count += sprintf(&buf[count], "0x%x: %d\n", i, reg[i]);
	}

	return count;
}

static ssize_t mc3xxx_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (mc3xxx_get_range(mc3xxx->client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t mc3xxx_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data = 0;
	int error = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (mc3xxx_set_range(mc3xxx->client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t mc3xxx_bandwidth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (mc3xxx_get_bandwidth(mc3xxx->client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t mc3xxx_bandwidth_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data = 0;
	int error = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (mc3xxx_set_bandwidth(mc3xxx->client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t mc3xxx_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return sprintf(buf, "%d\n", mc3xxx_current_placement);
}

static ssize_t mc3xxx_position_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long position = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	position = simple_strtoul(buf, NULL, 10);
	if ((position >= 0) && (position <= 7))
		mc3xxx_current_placement = position;

	return count;
}

static ssize_t mc3xxx_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (mc3xxx_get_mode(mc3xxx->client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t mc3xxx_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data = 0;
	int error = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (mc3xxx_set_mode(mc3xxx->client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t mc3xxx_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct mc3xxx_data *mc3xxx = input_get_drvdata(input);
	struct mc3xxxacc acc_value = { 0 };

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	mutex_lock(&mc3xxx->value_mutex);
	acc_value = mc3xxx->value;
	mutex_unlock(&mc3xxx->value_mutex);

	return sprintf(buf, "%d %d %d\n", acc_value.x, acc_value.y,
			acc_value.z);
}

static ssize_t mc3xxx_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return sprintf(buf, "%d\n", atomic_read(&mc3xxx->delay));
}

static ssize_t mc3xxx_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return sprintf(buf, "%s\n", MC3XXX_DEV_VERSION);
}

static ssize_t mc3xxx_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return mc3xxx_read_chip_id(client, buf);
}

static ssize_t mc3xxx_product_code_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return sprintf(buf, "0x%02X\n", mc3xxx->product_code);
}

static int mc3xxx_get_enable(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return atomic_read(&mc3xxx->enable);
}

static void mc3xxx_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&mc3xxx->enable);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called %d\n", __func__, enable);
#endif

	mutex_lock(&mc3xxx->enable_mutex);
	if (enable) {
		if (pre_enable == 0) {
			mc3xxx_set_mode(mc3xxx->client,
					MC3XXX_MODE_WAKE);
			schedule_delayed_work(&mc3xxx->work,
				msecs_to_jiffies(atomic_read(&mc3xxx->delay)));
			atomic_set(&mc3xxx->enable, 1);
		}

	} else {
		if (pre_enable == 1) {
			mc3xxx_set_mode(mc3xxx->client,
					MC3XXX_MODE_STANDBY);
			cancel_delayed_work_sync(&mc3xxx->work);
			atomic_set(&mc3xxx->enable, 0);
		}
	}
	mutex_unlock(&mc3xxx->enable_mutex);
}

static ssize_t mc3xxx_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return sprintf(buf, "%d\n", atomic_read(&mc3xxx->enable));
}

static ssize_t mc3xxx_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int enable = simple_strtoul(buf, NULL, 10);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called %d\n", __func__, enable);
#endif

	if (enable)
		mc3xxx_set_enable(dev, 1);
	else
		mc3xxx_set_enable(dev, 0);

	return count;
}

static int mc3xxx_get_delay(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc32x0 = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return atomic_read(&mc32x0->delay);
}

static void mc3xxx_set_delay(struct device *dev, int delay)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called %d\n", __func__, delay);
#endif

	atomic_set(&mc3xxx->delay, delay);

	mutex_lock(&mc3xxx->enable_mutex);

	if (mc3xxx_get_enable(dev)) {
		cancel_delayed_work_sync(&mc3xxx->work);
		schedule_delayed_work(&mc3xxx->work,
				delay_to_jiffies(delay) + 1);
	}

	mutex_unlock(&mc3xxx->enable_mutex);
}

static ssize_t mc3xxx_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);
	unsigned long delay = simple_strtoul(buf, NULL, 10);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (delay > MC3XXX_MAX_DELAY)
		delay = MC3XXX_MAX_DELAY;
	atomic_set(&mc3xxx->delay, delay);

	return count;
}

static DEVICE_ATTR(enable      , 0666                           ,
			mc3xxx_enable_show      , mc3xxx_enable_store);
static DEVICE_ATTR(delay       , 0664                           ,
			mc3xxx_delay_show       , mc3xxx_delay_store);
static DEVICE_ATTR(value       , S_IRUGO                        ,
			mc3xxx_value_show       , NULL);
static DEVICE_ATTR(range       , S_IRUGO|S_IWUSR|S_IWGRP|S_IROTH,
			mc3xxx_range_show       , mc3xxx_range_store);
static DEVICE_ATTR(bandwidth   , S_IRUGO|S_IWUSR|S_IWGRP|S_IROTH,
			mc3xxx_bandwidth_show   , mc3xxx_bandwidth_store);
static DEVICE_ATTR(position    , 0666                           ,
			mc3xxx_position_show    , mc3xxx_position_store);
static DEVICE_ATTR(mode        , S_IRUGO|S_IWUSR|S_IWGRP|S_IROTH,
			mc3xxx_mode_show        , mc3xxx_mode_store);
static DEVICE_ATTR(reg         , S_IRUGO|S_IWUSR|S_IWGRP|S_IROTH,
			mc3xxx_register_show    , mc3xxx_register_store);
static DEVICE_ATTR(product_code, S_IRUGO                        ,
			mc3xxx_product_code_show, NULL);
static DEVICE_ATTR(chip_id     , S_IRUGO                        ,
			mc3xxx_chip_id_show     , NULL);
static DEVICE_ATTR(version     , S_IRUGO                        ,
			mc3xxx_version_show     , NULL);

static struct attribute *mc3xxx_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_position.attr,
	&dev_attr_mode.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_reg.attr,
	&dev_attr_product_code.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group mc3xxx_attribute_group = {
	.attrs = mc3xxx_attributes
};

#ifdef MCUBE_DOT_CALIBRATION
static struct file *openFile(char *path, int flag, int mode)
{
	struct file *fp = NULL;

	fp = filp_open(path, flag, mode);
	if (IS_ERR(fp) || !fp->f_op) {
		MC_ERR_PRINT("%s: open return NULL!\n", __func__);
		return NULL;
	} else
		return fp;
}

static int readFile(struct file *fp, char *buf, int readlen)
{
	if (fp->f_op && fp->f_op->read)
		return fp->f_op->read(fp, buf, readlen, &fp->f_pos);
	else
		return -1;
}

static int writeFile(struct file *fp, char *buf, int writelen)
{
	if (fp->f_op && fp->f_op->write)
		return fp->f_op->write(fp, buf, writelen, &fp->f_pos);
	else
		return -1;
}

static int closeFile(struct file *fp)
{
	filp_close(fp, NULL);
	return 0;
}

static void initKernelEnv(void)
{
	oldfs = get_fs();
	set_fs(KERNEL_DS);
}

static int mc3xxx_read_rbm_xyz(struct mc3xxx_data *mc3xxx,
		struct mc3xxxacc *acc, int orient)
{
	int comres = -1;
	unsigned char data[6] = { 0 };
	s16 raw[3] = { 0 };
	const struct mc3xxx_hwmsen_convert *pCvt = NULL;

	if ((0 == gain_data.x) || (0 == gain_data.y) || (0 == gain_data.z)) {
		acc->x = 0;
		acc->y = 0;
		acc->z = 0;
		return 0;
	}

	comres = mc3xxx_smbus_read_block(mc3xxx->client,
			MC3XXX_XOUT_EX_L_REG, data, 6);
	if (comres) {
		MC_ERR_PRINT("%s: i2c error!\n", __func__);
		return comres;
	}

	raw[0] = (s16)(data[0] + (data[1] << 8));
	raw[1] = (s16)(data[2] + (data[3] << 8));
	raw[2] = (s16)(data[4] + (data[5] << 8));

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called: %d, %d, %d\n", __func__, raw[0], raw[1], raw[2]);
#endif

	raw[0] = (raw[0] + offset_data.x / 2) * mc3xxx_gain.x / gain_data.x;
	raw[1] = (raw[1] + offset_data.y / 2) * mc3xxx_gain.y / gain_data.y;
	raw[2] = (raw[2] + offset_data.z / 2) * mc3xxx_gain.z / gain_data.z;

	raw[0] = raw[0] * GRAVITY_1G_VALUE / mc3xxx_gain.x;
	raw[1] = raw[1] * GRAVITY_1G_VALUE / mc3xxx_gain.y;
	raw[2] = raw[2] * GRAVITY_1G_VALUE / mc3xxx_gain.z;

#if 1
	REMAP_IF_MC3250_READ(raw[0], raw[1]);
	REMAP_IF_MC34XX_N(raw[0], raw[1]);
	REMAP_IF_MC35XX(raw[0], raw[1]);
#else
	if (true == mc3xxx_is_new_2x2(mc3xxx->product_code)) {
		raw[0] = -raw[0];
		raw[1] = -raw[1];
	} else if  (true == mc3xxx_is_mc3250(mc3xxx->product_code)) {
		tmp = raw[0];
		raw[0] = raw[1];
		raw[1] = -tmp;
	}
#endif

	pCvt = &mc3xxx_cvt[orient];
	acc->x = pCvt->sign[MC3XXX_AXIS_X] * raw[pCvt->map[MC3XXX_AXIS_X]];
	acc->y = pCvt->sign[MC3XXX_AXIS_Y] * raw[pCvt->map[MC3XXX_AXIS_Y]];
	acc->z = pCvt->sign[MC3XXX_AXIS_Z] * raw[pCvt->map[MC3XXX_AXIS_Z]];

	return comres;
}

static int mc3xxx_read_true_data(struct mc3xxx_data *mc3xxx,
		struct mc3xxxacc *acc)
{
	int err = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called: %d\n", __func__, IsRbmMode);
#endif

	if (true == IsRbmMode)
		err = mc3xxx_read_rbm_xyz(mc3xxx, acc,
				mc3xxx_current_placement);
	else
		err = mc3xxx_read_accel_xyz(mc3xxx, acc,
				mc3xxx_current_placement);

	if (err) {
		MC_ERR_PRINT("%s: read error!\n", __func__);
		return err;
	}

	return err;
}

#ifdef SUPPORT_VPROXIMITY_SENSOR
static int mc3xxx_read_psensor_data(struct mc3xxx_data *mc3xxx, char *buf)
{
	int err = 0;
	struct mc3xxxacc acc = { 0 };
	signed long psensor_data[3] = { 0 };
	int pre_enable = atomic_read(&mc3xxx->enable);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (!buf) {
		pr_info("%s: invalid buffer pointer!\n", __func__);
		return -EINVAL;
	}
	if (pre_enable == 0) {
		mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_WAKE);
		atomic_set(&mc3xxx->enable, 1);
	}
	err = mc3xxx_read_true_data(mc3xxx, &acc);

	if (err) {
		pr_info("%s: read error!\n", __func__);
		return err;
	}

	psensor_data[0] = acc.x*10;
	psensor_data[1] = acc.y*10;
	psensor_data[2] = acc.z*10;

	sprintf(buf, "%04x %04x %04x", psensor_data[0], psensor_data[1],
			psensor_data[2]);

	return err;
}
#endif

static int mc3xxx_read_raw_data(struct mc3xxx_data *mc3xxx, char *buf)
{
	int err = 0;
	struct mc3xxxacc acc = { 0 };

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (!buf) {
		MC_ERR_PRINT("%s: invalid buffer pointer!\n", __func__);
		return -EINVAL;
	}

	err = mc3xxx_read_true_data(mc3xxx, &acc);

	if (err) {
		MC_ERR_PRINT("%s: read error!\n", __func__);
		return err;
	}

	sprintf(buf, "%04x %04x %04x", acc.x, acc.y, acc.z);

	return err;
}

static int mc3xxx_read_rbm_data(struct mc3xxx_data *mc3xxx, char *buf)
{
	int err = 0;
	struct mc3xxxacc acc = { 0 };

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (!buf) {
		MC_ERR_PRINT("%s: invalid buffer pointer!\n", __func__);
		return -EINVAL;
	}

	err = mc3xxx_read_rbm_xyz(mc3xxx, &acc, mc3xxx_current_placement);

	if (err)
		MC_ERR_PRINT("%s: read error!\n", __func__);

	sprintf(buf, "%04x %04x %04x", acc.x, acc.y, acc.z);

	return err;
}

static void mc3xxx_get_offset_gain(u8 *buf, SENSOR_DATA *pOffset,
		SENSOR_DATA *pGain)
{
	s16 tmp = 0;

	/* get x,y,z offset */
	tmp = ((buf[1] & 0x3f) << 8) + buf[0];
	if (tmp & 0x2000)
		tmp |= 0xc000;
	pOffset->x = tmp;

	tmp = ((buf[3] & 0x3f) << 8) + buf[2];
	if (tmp & 0x2000)
		tmp |= 0xc000;
	pOffset->y = tmp;

	tmp = ((buf[5] & 0x3f) << 8) + buf[4];
	if (tmp & 0x2000)
		tmp |= 0xc000;
	pOffset->z = tmp;

	/* get x,y,z gain */
	pGain->x = ((buf[1] >> 7) << 8) + buf[6];
	pGain->y = ((buf[3] >> 7) << 8) + buf[7];
	pGain->z = ((buf[5] >> 7) << 8) + buf[8];
}

static int mc3xxx_write_calibration(struct mc3xxx_data *mc3xxx,
		const SENSOR_DATA *pSensorData)
{
	int err = 0;
	u8 buf[9] = { 0 };
	int raw[3] = { 0 };
	SENSOR_DATA offset = { 0 }, gain = { 0 };
	struct mc3xxxacc acc = { 0 };
	const struct mc3xxx_hwmsen_convert *pCvt = NULL;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called: %d, %d, %d\n", __func__, pSensorData->x,
			pSensorData->y, pSensorData->z);
#endif

	err = mc3xxx_smbus_read_block(mc3xxx->client, MC3XXX_XOFFL_REG, buf, 9);
	if (err) {
		MC_ERR_PRINT("%s: read error!\n", __func__);
		return err;
	}

	acc.x = pSensorData->x;
	acc.y = pSensorData->y;
	acc.z = pSensorData->z;

	/* get raw from dat */
	pCvt = &mc3xxx_cvt[mc3xxx_current_placement];
	raw[pCvt->map[MC3XXX_AXIS_X]] = pCvt->sign[MC3XXX_AXIS_X] * acc.x;
	raw[pCvt->map[MC3XXX_AXIS_Y]] = pCvt->sign[MC3XXX_AXIS_Y] * acc.y;
	raw[pCvt->map[MC3XXX_AXIS_Z]] = pCvt->sign[MC3XXX_AXIS_Z] * acc.z;
	raw[MC3XXX_AXIS_X] = raw[MC3XXX_AXIS_X]
		* mc3xxx_gain.x / GRAVITY_1G_VALUE;
	raw[MC3XXX_AXIS_Y] = raw[MC3XXX_AXIS_Y]
		* mc3xxx_gain.y / GRAVITY_1G_VALUE;
	raw[MC3XXX_AXIS_Z] = raw[MC3XXX_AXIS_Z]
		* mc3xxx_gain.z / GRAVITY_1G_VALUE;
	MC_PRINT("mc3xxx_write_calibration gain: %d, %d, %d\n",
			mc3xxx_gain.x, mc3xxx_gain.y, mc3xxx_gain.z);

	REMAP_IF_MC3250_WRITE(raw[0], raw[1]);
	REMAP_IF_MC34XX_N(raw[0], raw[1]);
	REMAP_IF_MC35XX(raw[0], raw[1]);

	/* get offset and gain */
	mc3xxx_get_offset_gain(buf, &offset, &gain);
	MC_PRINT("mc3xxx_write_calibration og: %x, %x, %x, %x, %x, %x\n",
			offset.x, offset.y, offset.z, gain.x, gain.y, gain.z);

	/* prepare new offset */
	offset.x = offset.x + 16 * raw[MC3XXX_AXIS_X] * 256 *
		128 / 3 / mc3xxx_gain.x / (40 + gain.x);
	offset.y = offset.y + 16 * raw[MC3XXX_AXIS_Y] * 256 *
		128 / 3 / mc3xxx_gain.y / (40 + gain.y);
	offset.z = offset.z + 16 * raw[MC3XXX_AXIS_Z] * 256 *
		128 / 3 / mc3xxx_gain.z / (40 + gain.z);

	/* add for over range */
	if (offset.x > 0x1fff)
		offset.x = 0x1fff;
	else if (offset.x < -0x2000)
		offset.x = -0x2000;

	if (offset.y > 0x1fff)
		offset.y = 0x1fff;
	else if (offset.y < -0x2000)
		offset.y = -0x2000;

	if (offset.z > 0x1fff)
		offset.z = 0x1fff;
	else if (offset.z < -0x2000)
		offset.z = -0x2000;

	/* write offset registers */
	err = mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_STANDBY);

	buf[0] = offset.x & 0xff;
	buf[1] = ((offset.x >> 8) & 0x3f) | (gain.x & 0x0100 ? 0x80 : 0);
	buf[2] = offset.y & 0xff;
	buf[3] = ((offset.y >> 8) & 0x3f) | (gain.y & 0x0100 ? 0x80 : 0);
	buf[4] = offset.z & 0xff;
	buf[5] = ((offset.z >> 8) & 0x3f) | (gain.z & 0x0100 ? 0x80 : 0);

	err += mc3xxx_smbus_write_block(mc3xxx->client,
			MC3XXX_XOFFL_REG, buf, 6);

	err += mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_WAKE);

	if (err)
		MC_ERR_PRINT("%s: write error!\n", __func__);

	/* save offset and gain of DOT format for later use */
	offset_data.x = offset.x;
	offset_data.y = offset.y;
	offset_data.z = offset.z;

	gain_data.x = 256 * 8 * 128 / 3 / (40 + gain.x);
	gain_data.y = 256 * 8 * 128 / 3 / (40 + gain.y);
	gain_data.z = 256 * 8 * 128 / 3 / (40 + gain.z);

	msleep(50);

	return err;
}

static int mc3xxx_reset_calibration(struct mc3xxx_data *mc3xxx)
{
	int err = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	err = mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_STANDBY);

	err += mc3xxx_smbus_write_block(mc3xxx->client,
			MC3XXX_XOFFL_REG, offset_buf, 6);

	err += mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_WAKE);

	if (err)
		MC_ERR_PRINT("%s: write error!\n", __func__);

	/* save offset and gain of DOT format for later use */
	mc3xxx_get_offset_gain(offset_buf, &offset_data, &gain_data);

	gain_data.x = 256 * 8 * 128 / 3 / (40 + gain_data.x);
	gain_data.y = 256 * 8 * 128 / 3 / (40 + gain_data.y);
	gain_data.z = 256 * 8 * 128 / 3 / (40 + gain_data.z);

	return err;
}

static int mc3xxx_soft_reset(struct i2c_client *client)
{
	int err = 0;
	u8 tmp = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	tmp = 0x6d;
	err = mc3xxx_smbus_write_byte(client, 0x1B, &tmp);

	tmp = 0x43;
	err += mc3xxx_smbus_write_byte(client, 0x1B, &tmp);
	usleep_range(5000, 5005);

	tmp = 0x43;
	err += mc3xxx_smbus_write_byte(client, 0x07, &tmp);

	tmp = 0x80;
	err += mc3xxx_smbus_write_byte(client, 0x1C, &tmp);

	tmp = 0x80;
	err += mc3xxx_smbus_write_byte(client, 0x17, &tmp);
	usleep_range(5000, 5005);

	tmp = 0x00;
	err += mc3xxx_smbus_write_byte(client, 0x1C, &tmp);

	tmp = 0x00;
	err += mc3xxx_smbus_write_byte(client, 0x17, &tmp);
	usleep_range(5000, 5005);

	memset(offset_buf, 0, sizeof(offset_buf));
	err += mc3xxx_smbus_read_block(client, MC3XXX_XOFFL_REG,
			&offset_buf[0], 9);
	if (err) {
		MC_ERR_PRINT("%s: read error!\n", __func__);
		return err;
	}

	/* save offset and gain of DOT format for later use */
	mc3xxx_get_offset_gain(offset_buf, &offset_data, &gain_data);

	gain_data.x = 256 * 8 * 128 / 3 / (40 + gain_data.x);
	gain_data.y = 256 * 8 * 128 / 3 / (40 + gain_data.y);
	gain_data.z = 256 * 8 * 128 / 3 / (40 + gain_data.z);

	/* initial calibration data */
	mc3xxx_cali_data.x = 0;
	mc3xxx_cali_data.y = 0;
	mc3xxx_cali_data.z = 0;

	return err;
}

static void mc3xxx_read_calibration(struct mc3xxx_data *mc3xxx,
		SENSOR_DATA *pSensorData)
{
#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	pSensorData->x = mc3xxx_cali_data.x;
	pSensorData->y = mc3xxx_cali_data.y;
	pSensorData->z = mc3xxx_cali_data.z;
}

static int mc3xxx_rbm_mode(struct mc3xxx_data *mc3xxx, bool enable)
{
	int rc = 0;
	unsigned char data = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called: %d\n", __func__, enable);
#endif

	rc = mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_STANDBY);
	rc += mc3xxx_smbus_read_byte(mc3xxx->client, 0x04, &data);

	if (0x00 == (data & 0x40)) {
		data = 0x6D;
		rc += mc3xxx_smbus_write_byte(mc3xxx->client, 0x1B, &data);

		data = 0x43;
		rc += mc3xxx_smbus_write_byte(mc3xxx->client, 0x1B, &data);
	}


	if (true == enable) {
		data = 0;
		rc += mc3xxx_smbus_write_byte(mc3xxx->client, 0x3B, &data);

		data = 0x02;
		rc += mc3xxx_smbus_write_byte(mc3xxx->client, 0x14, &data);

		IsRbmMode = 1;
	} else {
		data = 0;
		rc += mc3xxx_smbus_write_byte(mc3xxx->client, 0x14, &data);

		data = s_bPCODE;
		rc += mc3xxx_smbus_write_byte(mc3xxx->client, 0x3B, &data);

		IsRbmMode = 0;
	}

	rc += mc3xxx_smbus_read_byte(mc3xxx->client, 0x04, &data);

	if (data & 0x40) {
		data = 0x6D;
		rc += mc3xxx_smbus_write_byte(mc3xxx->client, 0x1B, &data);

		data = 0x43;
		rc += mc3xxx_smbus_write_byte(mc3xxx->client, 0x1B, &data);
	}

	rc += mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_WAKE);

	msleep(220);

	return rc;
}

static void mc3xxx_read_cali_file(struct mc3xxx_data *mc3xxx)
{
	SENSOR_DATA cali_data = { 0 };
	int err = 0;
	char buf[64] = { 0 };

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (load_cali_cnt > 0)
		load_cali_cnt--;
	else
		return;

	initKernelEnv();
	fd_file = openFile(file_path, O_RDONLY, 0);
	if (fd_file == NULL) {
		MC_ERR_PRINT("%s: fail to open!\n", __func__);
		cali_data.x = 0;
		cali_data.y = 0;
		cali_data.z = 0;
		return;
	} else {
		memset(buf, 0, sizeof(buf));
		err = readFile(fd_file, buf, 64);
		if (err <= 0)
			MC_ERR_PRINT("%s: read file error %d!\n",
					__func__, err);
		else
			MC_PRINT("%s: %s\n", __func__, buf);

		set_fs(oldfs);
		closeFile(fd_file);

		sscanf(buf, "%d %d %d", &cali_data.x, &cali_data.y,
				&cali_data.z);

		mc3xxx_write_calibration(mc3xxx, &cali_data);

		load_cali_cnt = 0;

		return;
	}
}

static int mc3xxx_write_log_data(const unsigned char data[64])
{
#define _WRT_LOG_DATA_BUFFER_SIZE    (66 * 50)

	s16 rbm_data[3] = { 0 }, raw_data[3] = { 0 };
	int err = 0;
	char *_pszBuffer = NULL;
	int n = 0, i = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	initKernelEnv();
	fd_file = openFile(DATA_PATH, O_RDWR | O_CREAT, 0);
	if (fd_file == NULL) {
		MC_ERR_PRINT("%s: can't create file!\n", __func__);
		return -1;
	} else {
		rbm_data[0] = (s16)((data[0x0d]) | (data[0x0e] << 8));
		rbm_data[1] = (s16)((data[0x0f]) | (data[0x10] << 8));
		rbm_data[2] = (s16)((data[0x11]) | (data[0x12] << 8));

		raw_data[0] = (rbm_data[0] + offset_data.x/2) * mc3xxx_gain.x
			/ gain_data.x;
		raw_data[1] = (rbm_data[1] + offset_data.y/2) * mc3xxx_gain.y
			/ gain_data.y;
		raw_data[2] = (rbm_data[2] + offset_data.z/2) * mc3xxx_gain.z
			/ gain_data.z;

		_pszBuffer = kzalloc(_WRT_LOG_DATA_BUFFER_SIZE, GFP_KERNEL);
		if (NULL == _pszBuffer) {
			MC_ERR_PRINT("%s: fail to allocate mem for buffer!\n",
					__func__);
			return -1;
		}
		memset(_pszBuffer, 0, _WRT_LOG_DATA_BUFFER_SIZE);

		n += sprintf(_pszBuffer+n,
				"G-sensor RAW X = %d  Y = %d  Z = %d\n",
				raw_data[0] , raw_data[1] , raw_data[2]);
		n += sprintf(_pszBuffer+n,
				"G-sensor RBM X = %d  Y = %d  Z = %d\n",
				rbm_data[0] , rbm_data[1] , rbm_data[2]);
		for (i = 0; i < 64; i++) {
			n += sprintf(_pszBuffer+n,
				"mCube register map Register[%x] = 0x%x\n",
				i, data[i]);
		}
		msleep(50);
		err = writeFile(fd_file, _pszBuffer, n);
		if (err <= 0)
			MC_ERR_PRINT("%s: write file error %d!\n",
					__func__, err);

		kfree(_pszBuffer);

		set_fs(oldfs);
		closeFile(fd_file);
	}

	return 0;
}

static int mc3xxx_read_reg_map(struct mc3xxx_data *mc3xxx)
{
	u8 data[64] = { 0 };
	int err = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	err = mc3xxx_smbus_read_block(mc3xxx->client, 0, data, 64);

	if (err) {
		MC_ERR_PRINT("%s: read reg fail!\n", __func__);
		return err;
	}

	msleep(50);

	mc3xxx_write_log_data(data);

	msleep(50);

	return err;
}
#endif

static long mc3xxx_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int err = 0;
	void __user *argp = (void __user *)arg;
	struct mc3xxx_data *mc3xxx = file->private_data;
	int temp = 0;

#ifdef MCUBE_DOT_CALIBRATION
	char strbuf[256] = { 0 };
	SENSOR_DATA sensor_data = { 0 };
#endif

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s: %x\n", __func__, cmd);
#endif

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
				_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
				_IOC_SIZE(cmd));

	if (err) {
		MC_ERR_PRINT("%s: access isn't ok!\n", __func__);
		return -EFAULT;
	}

	if (NULL == mc3xxx_client) {
		MC_ERR_PRINT("%s: i2c client isn't exist!\n", __func__);
		return -EFAULT;
	}

	switch (cmd) {
	case MC3XXX_ACC_IOCTL_SET_DELAY:
		if (copy_from_user(&temp, argp, sizeof(temp))) {
			MC_ERR_PRINT("%s: set delay copy error!\n",
					__func__);
			return -EFAULT;
		}
		if (temp < 0 || temp > 1000) {
			MC_ERR_PRINT("%s: set delay over limit!\n",
					__func__);
			return -EINVAL;
		}
		mc3xxx_set_delay(&(mc3xxx->client->dev), temp);
		break;

	case MC3XXX_ACC_IOCTL_GET_DELAY:
		temp = mc3xxx_get_delay(&(mc3xxx->client->dev));
		if (copy_to_user(argp, &temp, sizeof(temp))) {
			MC_ERR_PRINT("%s: get delay copy error!\n", __func__);
			return -EFAULT;
		}
		break;

	case MC3XXX_ACC_IOCTL_SET_ENABLE:
		if (copy_from_user(&temp, argp, sizeof(temp))) {
			MC_ERR_PRINT("%s: set enable copy error!\n", __func__);
			return -EFAULT;
		}

		if (1 == temp)
			mc3xxx_set_enable(&(mc3xxx->client->dev), 1);
		else if (0 == temp)
			mc3xxx_set_enable(&(mc3xxx->client->dev), 0);
		else {
			MC_ERR_PRINT("%s: set enable over limit!\n", __func__);
			return -EINVAL;
		}

		break;

	case MC3XXX_ACC_IOCTL_GET_ENABLE:
		temp = mc3xxx_get_enable(&(mc3xxx->client->dev));
		if (copy_to_user(argp, &temp, sizeof(temp))) {
			MC_ERR_PRINT("%s: get enable copy error!\n", __func__);
			return -EINVAL;
		}
		break;

	case MC3XXX_ACC_IOCTL_CALIBRATION:
		MC_ERR_PRINT("%s: don't handle the command!\n", __func__);
		return -EINVAL;

#ifdef MCUBE_DOT_CALIBRATION
	case GSENSOR_IOCTL_READ_SENSORDATA:

#ifdef SUPPORT_VPROXIMITY_SENSOR
		MC_PRINT("%s: GSENSOR_IOCTL_READ_SENSORDATA\n", __func__);
		mc3xxx_read_psensor_data(mc3xxx, strbuf);
		if (copy_to_user(argp, strbuf, strlen(strbuf)+1)) {
			pr_info("%s: read rawdata fail to copy!\n", __func__);
			return -EFAULT;
		}
		break;
#endif

	case GSENSOR_IOCTL_READ_RAW_DATA:
		MC_PRINT("%s: GSENSOR_IOCTL_READ_SENSORDATA\n", __func__);
		mc3xxx_read_raw_data(mc3xxx, strbuf);
		if (copy_to_user(argp, strbuf, strlen(strbuf)+1)) {
			MC_ERR_PRINT("%s: read rawdata fail to copy!\n",
					__func__);
			return -EFAULT;
		}
		break;

	case GSENSOR_MCUBE_IOCTL_SET_CALI:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_SET_CALI\n", __func__);
		if (copy_from_user(&sensor_data, argp, sizeof(sensor_data))) {
			MC_ERR_PRINT("%s: set cali fail to copy!\n", __func__);
			return -EFAULT;
		} else {
			mutex_lock(&mc3xxx->enable_mutex);
			err = mc3xxx_write_calibration(mc3xxx, &sensor_data);
			mutex_unlock(&mc3xxx->enable_mutex);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		MC_PRINT("%s: GSENSOR_IOCTL_CLR_CALI\n", __func__);
		mutex_lock(&mc3xxx->enable_mutex);
		err = mc3xxx_reset_calibration(mc3xxx);
		mutex_unlock(&mc3xxx->enable_mutex);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		MC_PRINT("%s: GSENSOR_IOCTL_GET_CALI\n", __func__);
		mc3xxx_read_calibration(mc3xxx, &sensor_data);

		if (copy_to_user(argp, &sensor_data, sizeof(sensor_data))) {
			MC_ERR_PRINT("%s: get cali fail to copy!\n", __func__);
			err = -EFAULT;
			return err;
		}
		break;

	case GSENSOR_IOCTL_SET_CALI_MODE:
		MC_PRINT("%s: GSENSOR_IOCTL_SET_CALI_MODE\n", __func__);
		break;

	case GSENSOR_MCUBE_IOCTL_READ_RBM_DATA:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_READ_RBM_DATA\n", __func__);
		mc3xxx_read_rbm_data(mc3xxx, strbuf);
		if (copy_to_user(argp, &strbuf, strlen(strbuf)+1)) {
			MC_ERR_PRINT("%s: read rawdata fail to copy!\n",
					__func__);
			return -EFAULT;
		}
		break;

	case GSENSOR_MCUBE_IOCTL_SET_RBM_MODE:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_SET_RBM_MODE\n", __func__);
		mutex_lock(&mc3xxx->enable_mutex);
		err = mc3xxx_rbm_mode(mc3xxx, 1);
		mutex_unlock(&mc3xxx->enable_mutex);
		break;

	case GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE\n", __func__);
		mutex_lock(&mc3xxx->enable_mutex);
		err = mc3xxx_rbm_mode(mc3xxx, 0);
		mutex_unlock(&mc3xxx->enable_mutex);
		break;

	case GSENSOR_MCUBE_IOCTL_REGISTER_MAP:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_REGISTER_MAP\n", __func__);
		err = mc3xxx_read_reg_map(mc3xxx);
		break;

	case GSENSOR_MCUBE_IOCTL_READ_PRODUCT_ID:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_READ_PRODUCT_ID\n", __func__);
		temp = mc3xxx_detect_pcode(mc3xxx->client);
		if (temp > 0)
			temp = 0;
		else
			temp = -1;
		if (copy_to_user(argp, &temp, sizeof(temp))) {
			MC_ERR_PRINT("%s: read pcode fail to copy!\n",
					__func__);
			return -EFAULT;
		}
		break;

	case GSENSOR_MCUBE_IOCTL_READ_CHIP_ID:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_READ_CHIP_ID\n", __func__);
		mc3xxx_read_chip_id(mc3xxx->client, strbuf);
		if (copy_to_user(argp, &strbuf, strlen(strbuf))) {
			MC_ERR_PRINT("%s: read chip id fail to copy!\n",
					__func__);
			return -EFAULT;
		}
		break;

	case GSENSOR_MCUBE_IOCTL_READ_FILEPATH:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_READ_FILEPATH\n", __func__);
		if (copy_to_user(argp, file_path, (strlen(file_path)+1))) {
			MC_ERR_PRINT("%s: read file path fail to copy!\n",
					__func__);
			return -EFAULT;
		}
		break;
#endif

	default:
		MC_ERR_PRINT("%s: can't recognize the cmd!\n", __func__);
		return 0;
	}

	return 0;
}

static ssize_t mc3xxx_read(struct file *file, char __user *buf,
		size_t count, loff_t *offset)
{
	int ret = 0;
	struct mc3xxxacc acc = { 0 };
	struct mc3xxx_data *mc3xxx = NULL;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	if (NULL == mc3xxx_client) {
		MC_ERR_PRINT("%s: I2C driver not install!", __func__);
		return -1;
	}

	mc3xxx = i2c_get_clientdata(mc3xxx_client);

#ifdef MCUBE_DOT_CALIBRATION
	mc3xxx_read_cali_file(mc3xxx);
	mc3xxx_read_true_data(mc3xxx, &acc);
#else
	mc3xxx_read_accel_xyz(mc3xxx, &acc, mc3xxx_current_placement);
#endif

	mutex_lock(&mc3xxx->value_mutex);
	mc3xxx->value = acc;
	mutex_unlock(&mc3xxx->value_mutex);

	ret = copy_to_user(buf, &acc, sizeof(acc));
	if (ret) {
		MC_ERR_PRINT("%s: fail to copy_to_user: %d\n", __func__, ret);
		return 0;
	}

	return sizeof(acc);
}

static ssize_t mc3xxx_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return 0;
}

static int mc3xxx_open(struct inode *inode, struct file *file)
{
	int err = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	err = nonseekable_open(inode, file);
	if (err < 0) {
		MC_ERR_PRINT("%s: open fail!\n", __func__);
		return err;
	}

	file->private_data = i2c_get_clientdata(mc3xxx_client);

	return 0;
}

static int mc3xxx_close(struct inode *inode, struct file *file)
{
#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	return 0;
}

static const struct file_operations mc3xxx_fops = {
	.owner = THIS_MODULE,
	.read = mc3xxx_read,
	.write = mc3xxx_write,
	.open = mc3xxx_open,
	.release = mc3xxx_close,
	.unlocked_ioctl = mc3xxx_ioctl,
};

static int board_mc3xxx_power(struct device *dev, bool status)
{
	static struct regulator *v_sensor;
	static bool currentStatus;
	int ret = 0;

	if (!v_sensor) {
		v_sensor = regulator_get(dev, "vdd");
		if (IS_ERR(v_sensor)) {
			v_sensor = NULL;
			return -EIO;
		}
		regulator_set_voltage(v_sensor, 2800000, 2800000);
		currentStatus = false;
	}

	if (currentStatus != status) {
		dev_err(dev, "%s: unexpected state %d\n", __func__, status);
		return 0;
	}

	if (status) {
		ret = regulator_enable(v_sensor);
		if (ret < 0)
			return -1;
		currentStatus = status;
	} else {
		regulator_disable(v_sensor);
		currentStatus = status;
	}

	/* modified to brute force capture of gpio
	   - when stolen by other driver */
	dev_err(dev, "%s: state %d\n", __func__, status);

	return 0;
}

static int mc3xxx_remove(struct i2c_client *client)
{
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	mc3xxx_set_enable(&client->dev, 0);

	sysfs_remove_group(&mc3xxx->input->dev.kobj, &mc3xxx_attribute_group);
	mc3xxx_input_deinit(mc3xxx);
	kfree(mc3xxx);

	return 0;
}

static int mc3xxx_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	mutex_lock(&mc3xxx->enable_mutex);

	if (mc3xxx_get_enable(&client->dev)) {
		cancel_delayed_work_sync(&mc3xxx->work);
		mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_STANDBY);
	}

	mutex_unlock(&mc3xxx->enable_mutex);

	return 0;
}

static int mc3xxx_resume(struct i2c_client *client)
{
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);
	int delay = mc3xxx_get_delay(&(client->dev));

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	mc3xxx_set_delay(&client->dev, delay);

	mutex_lock(&mc3xxx->enable_mutex);

	if (mc3xxx_get_enable(&client->dev)) {
		mc3xxx_set_mode(mc3xxx->client, MC3XXX_MODE_WAKE);
		schedule_delayed_work(&mc3xxx->work,
				delay_to_jiffies(delay) + 1);
	}

	mutex_unlock(&mc3xxx->enable_mutex);

	return 0;
}

static int mc3xxx_i2c_remove(struct i2c_client *client)
{
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif
	board_mc3xxx_power(&client->dev, 0);

	return mc3xxx_remove(mc3xxx->client);
}

static int mc3xxx_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif
	mc3xxx_suspend(mc3xxx->client, mesg);

	return 0;
}

static int mc3xxx_i2c_resume(struct i2c_client *client)
{
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif
	mc3xxx_resume(mc3xxx->client);

	return 0;
}

static struct miscdevice mc3xxx_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MC3XXX_DEV_NAME,
	.fops = &mc3xxx_fops,
};

static int mc3xxx_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct mc3xxx_data *mc3xxx = NULL;
	unsigned char product_code = 0;
	int err = 0;

	board_mc3xxx_power(&client->dev, 1);

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	err = mc3xxx_detect_pcode(client);
	if (err < 0) {
		MC_ERR_PRINT("%s: isn't mcube g-sensor!\n", __func__);
		return -ENODEV;
	}

#ifdef MCUBE_DOT_CALIBRATION
	mc3xxx_soft_reset(client);
	err = mc3xxx_detect_pcode(client);
	if (err < 0) {
		MC_ERR_PRINT("%s: can't confirm mcube g-sensor!\n", __func__);
		return -ENODEV;
	}
#endif

	product_code = (unsigned char)err;

	/* setup private data */
	mc3xxx = kzalloc(sizeof(struct mc3xxx_data), GFP_KERNEL);
	if (!mc3xxx) {
		MC_ERR_PRINT("%s: can't allocate memory for mc3xxx_data!\n",
				__func__);
		err = -ENOMEM;
		return err;
	}

	mc3xxx->product_code = product_code;

	mutex_init(&mc3xxx->enable_mutex);
	mutex_init(&mc3xxx->value_mutex);

	/* setup i2c client */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		MC_ERR_PRINT("%s: i2c function not support!\n", __func__);
		kfree(mc3xxx);
		err = -ENODEV;
		return err;
	}

	i2c_set_clientdata(client, mc3xxx);
	mc3xxx->client = client;
	mc3xxx_client = client;

	dev_info(&client->dev, "%s found\n", id->name);

	atomic_set(&mc3xxx->position, mc3xxx_current_placement);
	atomic_set(&mc3xxx->delay, MC3XXX_MAX_DELAY);

	mc3xxx_init(mc3xxx);

	/* setup driver interfaces */
	INIT_DELAYED_WORK(&mc3xxx->work, mc3xxx_work_func);

	err = mc3xxx_input_init(mc3xxx);
	if (err < 0) {
		MC_ERR_PRINT("%s: input init fail!\n", __func__);
		kfree(mc3xxx);
		return err;
	}

	err = sysfs_create_group(&mc3xxx->input->dev.kobj,
			&mc3xxx_attribute_group);
	if (err < 0) {
		MC_ERR_PRINT("%s: create group fail!\n", __func__);
		mc3xxx_input_deinit(mc3xxx);
		kfree(mc3xxx);
		return err;
	}

	err = misc_register(&mc3xxx_device);
	if (err) {
		MC_ERR_PRINT("%s: create register fail!\n", __func__);
		sysfs_remove_group(&mc3xxx->input->dev.kobj,
				&mc3xxx_attribute_group);
		mc3xxx_input_deinit(mc3xxx);
		kfree(mc3xxx);
		return err;
	}

	return 0;
}

static const struct of_device_id mc3xxx_of_match[] = {
	{.compatible = "mc3, mc3xxx",},
	{},
};

static const struct i2c_device_id mc3xxx_id[] = {
	{MC3XXX_DEV_NAME, 0},
	{ }
};

static struct i2c_driver mc3xxx_driver = {
	.driver = {
		.name = MC3XXX_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mc3xxx_of_match,
	},
	.probe    = mc3xxx_i2c_probe,
	.remove   = mc3xxx_i2c_remove,
	.suspend  = mc3xxx_i2c_suspend,
	.resume   = mc3xxx_i2c_resume,
	.id_table = mc3xxx_id,
};

#ifdef I2C_BUS_NUM_STATIC_ALLOC
int i2c_static_add_device(struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = NULL;
	struct i2c_client *client = NULL;
	int err = 0;

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	adapter = i2c_get_adapter(I2C_STATIC_BUS_NUM);
	if (!adapter) {
		MC_ERR_PRINT("%s: can't get i2c adapter!\n", __func__);
		err = -ENODEV;
		return err;
	}

	client = i2c_new_device(adapter, info);
	if (!client) {
		MC_ERR_PRINT("%s: can't add i2c device at 0x%x!\n",
				__func__, (unsigned int)info->addr);
		err = -ENODEV;
		return err;
	}

	i2c_put_adapter(adapter);

	return 0;
}
#endif /* I2C_BUS_NUM_STATIC_ALLOC */

static int __init mc3xxx_i2c_init(void)
{
#ifdef I2C_BUS_NUM_STATIC_ALLOC
	int ret = 0;
#endif

#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

#ifdef I2C_BUS_NUM_STATIC_ALLOC
	ret = i2c_static_add_device(&mc3xxx_i2c_boardinfo);
	if (ret < 0) {
		MC_ERR_PRINT("%s: add i2c device error %d\n", __func__, ret);
		return ret;
	}
#endif

	return i2c_add_driver(&mc3xxx_driver);
}

static void __exit mc3xxx_i2c_exit(void)
{
#ifdef MCUBE_FUNC_DEBUG
	pr_info("%s called\n", __func__);
#endif

	i2c_del_driver(&mc3xxx_driver);

#ifdef I2C_BUS_NUM_STATIC_ALLOC
	i2c_unregister_device(mc3xxx_client);
#endif
}

module_init(mc3xxx_i2c_init);
module_exit(mc3xxx_i2c_exit);

MODULE_AUTHOR("Vichard Wei<jjwei@marvell.com>");
MODULE_DESCRIPTION("MC mc3xxx accelerometer driver");
MODULE_LICENSE("GPL");
