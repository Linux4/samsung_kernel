/*
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
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
 * VERSION			DATE			AUTHOR
 *    1.0		  2010-01-05			WenFS
 * note: only support mulititouch	Wenfs 2010-10-01
 */

#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c/ft6206_ts.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>


#define I2C_BOARD_INFO_METHOD   1
#define TS_DATA_THRESHOLD_CHECK	0
#define TS_WIDTH_MAX		539
#define	TS_HEIGHT_MAX		1060
#define TOUCH_VIRTUAL_KEYS
#define CONFIG_FT5X0X_MULTITOUCH 1
#include <linux/init.h>

/*#define	FTS_CTL_IIC 1*/
#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif

#ifdef TP_HAVE_PROX
#define PROXIMITY_INPUT_DEV		"proximity_tp"
static int is_incall;
struct input_dev *proximity_input_dev;
static struct class *firmware_class;
static struct device *firmware_cmd_dev;
static int ft5x0x_prox_ctl(int value);
#endif
static int debug_level;

#define TS_DBG(format, ...)	do {					\
		if (debug_level == 1)					\
			pr_info("FT5X0X " format "\n", ## __VA_ARGS__);	\
	} while (0)

struct sprd_i2c_setup_data {
	/* the same number as i2c->adap.nr in adapter probe function */
	unsigned i2c_bus;
	unsigned short i2c_address;
	int irq;
	char type[I2C_NAME_SIZE];
};

static struct ft5x0x_ts_data *g_ft5x0x_ts;
static struct i2c_client *this_client;
/* Attribute */
static unsigned char suspend_flag; /* 0: sleep out; 1: sleep in */

static ssize_t ft5x0x_show_suspend(struct device *cd,
		struct device_attribute *attr, char *buf);
static ssize_t ft5x0x_store_suspend(struct device *cd,
		struct device_attribute *attr, const char *buf, size_t len);
static ssize_t ft5x0x_show_version(struct device *cd,
		struct device_attribute *attr, char *buf);
static ssize_t ft5x0x_update(struct device *cd,
		struct device_attribute *attr, const char *buf, size_t len);
static ssize_t ft5x0x_show_debug(struct device *cd,
		struct device_attribute *attr, char *buf);
static ssize_t ft5x0x_store_debug(struct device *cd,
		struct device_attribute *attr, const char *buf, size_t len);
static unsigned char ft5x0x_read_fw_ver(void);
static int ft5x0x_ts_suspend(struct device *dev);
static int ft5x0x_ts_resume(struct device *dev);
static int fts_ctpm_fw_update(void);
static int fts_ctpm_fw_upgrade_with_i_file(void);
struct ts_event {
	u16	x1;
	u16	y1;
	u16	x2;
	u16	y2;
	u16	x3;
	u16	y3;
	u16	x4;
	u16	y4;
	u16	x5;
	u16	y5;
	u16	pressure;
	u8	touch_point;
#ifdef TP_HAVE_PROX
	int prox_event;
	int prox_value;
#endif
};

struct ft5x0x_ts_data {
	struct input_dev	*input_dev;
#ifdef TP_HAVE_PROX
	struct input_dev        *prox_input_dev;
#endif
	struct i2c_client	*client;
	struct ts_event	event;
	struct work_struct	pen_event_work;
	struct workqueue_struct	*ts_workqueue;
	struct ft5x0x_ts_platform_data	*platform_data;
	struct regulator *v_tsp;
};


static DEVICE_ATTR(suspend, S_IRUGO | S_IWUSR,
		ft5x0x_show_suspend, ft5x0x_store_suspend);
static DEVICE_ATTR(update, S_IRUGO | S_IWUSR,
		ft5x0x_show_version, ft5x0x_update);
static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR,
		ft5x0x_show_debug, ft5x0x_store_debug);

static ssize_t ft5x0x_show_debug(struct device *cd,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "FT5206 Debug %d\n", debug_level);

	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t ft5x0x_store_debug(struct device *cd,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	debug_level = on_off;

	TS_DBG("%s: debug_level=%d\n", __func__, debug_level);

	return len;
}

static ssize_t ft5x0x_show_suspend(struct device *cd,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	if (suspend_flag == 1)
		sprintf(buf, "FT5206 Suspend\n");
	else
		sprintf(buf, "FT5206 Resume\n");

	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t ft5x0x_store_suspend(struct device *cd,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	suspend_flag = on_off;

	if (on_off == 1) {
		TS_DBG("FT5206 Entry Suspend\n");
		ft5x0x_ts_suspend(NULL);
	} else {
		TS_DBG("FT5206 Entry Resume\n");
		ft5x0x_ts_resume(NULL);
	}

	return len;
}

static ssize_t ft5x0x_show_version(struct device *cd,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned char uc_reg_value;

	uc_reg_value = ft5x0x_read_fw_ver();

	sprintf(buf, "ft5x0x firmware id is V%x\n", uc_reg_value);

	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t ft5x0x_update(struct device *cd, struct device_attribute *attr,
		const char *buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	unsigned char uc_reg_value;

	uc_reg_value = ft5x0x_read_fw_ver();

	if (on_off == 1) {
		TS_DBG("ft5x0x update, current firmware id is V%x\n",
				uc_reg_value);
		fts_ctpm_fw_upgrade_with_i_file();
	}

	return len;
}

static int ft5x0x_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	TS_DBG("%s", __func__);

	err = device_create_file(dev, &dev_attr_suspend);
	err = device_create_file(dev, &dev_attr_update);
	err = device_create_file(dev, &dev_attr_debug);

	return err;
}

static int ft5x0x_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

	return ret;
}

static int ft5x0x_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}
/*****************************************************************************
Name	:	 ft5x0x_write_reg

Input	:	addr -- address
para -- parameter

Output	:

function	:	write register of ft5x0x

 *****************************************************************************/
static int ft5x0x_write_reg(u8 addr, u8 para)
{
	u8 buf[3];
	int ret = -1;

	buf[0] = addr;
	buf[1] = para;
	ret = ft5x0x_i2c_txdata(buf, 2);
	if (ret < 0) {
		pr_err("write reg failed! %#x ret: %d", buf[0], ret);
		return -1;
	}

	return 0;
}


/*****************************************************************************
Name	:	ft5x0x_read_reg

Input	:	addr
pdata

Output	:

function	:	read register of ft5x0x

 *****************************************************************************/
static int ft5x0x_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2] = {0};
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf,
		},
	};

	buf[0] = addr;
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

	*pdata = buf[0];
	return ret;
}

#ifdef TOUCH_VIRTUAL_KEYS
static ssize_t virtual_keys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
#if 0 /* kj_v983 */
	return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_MENU)
			":80:890:20:20"	":"
			__stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)
			":240:890:20:20" ":"
			__stringify(EV_KEY) ":" __stringify(KEY_BACK)
			":420:890:20:20" ":"
			__stringify(EV_KEY) ":" __stringify(KEY_SEARCH)
			":480:890:60:60\n");
#endif
#if 0 /* L901 dws */
	return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_MENU)
			":60:1020:20:20" ":"
			__stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)
			":300:1020:20:20" ":"
			__stringify(EV_KEY) ":" __stringify(KEY_BACK)
			":180:1020:20:20\n");
#endif
#if 0 /* L901 yx */
	return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_MENU)
			":80:900:20:20"	":"
			__stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)
			":240:900:20:20" ":"
			__stringify(EV_KEY) ":" __stringify(KEY_BACK)
			":400:900:20:20\n");
#endif
#if 0 /* L920 */
	return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_MENU)
			":240:900:60:60" ":"
			__stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)
			":320:900:60:60" ":"
			__stringify(EV_KEY) ":" __stringify(KEY_BACK)
			":400:900:60:60\n");
#endif
#if 1 /* L960 */
	return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_MENU)
			":80:900:30:30"	":"
			__stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)
			":240:900:30:30" ":"
			__stringify(EV_KEY) ":" __stringify(KEY_BACK)
			":400:900:30:30\n");
#endif
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.ft5x0x_ts",
		.mode = S_IRUGO,
	},
	.show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
	&virtual_keys_attr.attr,
	NULL
};

static struct attribute_group properties_attr_group = {
	.attrs = properties_attrs,
};

static int ft5x0x_ts_virtual_keys_init(struct input_dev *input_dev)
{
	struct kobject *properties_kobj;
	int ret = 0;

	TS_DBG("%s\n", __func__);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&properties_attr_group);
	if (!properties_kobj || ret)
		pr_err("failed to create board_properties\n");

	return 0;
}

#endif

/*****************************************************************************
Name	:	 ft5x0x_read_fw_ver

Input	:	 void

Output	:	 firmware version

function	:	 read TP firmware version

 ****************************************************************************/
static unsigned char ft5x0x_read_fw_ver(void)
{
	unsigned char ver;
	ft5x0x_read_reg(FT5X0X_REG_FIRMID, &ver);
	return ver;
}


#define CONFIG_SUPPORT_FTS_CTP_UPG


#ifdef CONFIG_SUPPORT_FTS_CTP_UPG

typedef enum {
	ERR_OK,
	ERR_MODE,
	ERR_READID,
	ERR_ERASE,
	ERR_STATUS,
	ERR_ECC,
	ERR_DL_ERASE_FAIL,
	ERR_DL_PROGRAM_FAIL,
	ERR_DL_VERIFY_FAIL
} E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;
typedef unsigned short        FTS_WORD;
typedef unsigned int          FTS_DWRD;
typedef unsigned char         FTS_BOOL;

#define FTS_NULL		0x0
#define FTS_TRUE		0x01
#define FTS_FALSE		0x0

#define I2C_CTPM_ADDRESS	0x70

/*
 * [function]:
 * callback: read data from ctpm by i2c interface, implemented by special user;
 * [parameters]:
 * bt_ctpm_addr[in]    :the address of the ctpm;
 * pbt_buf[out]        :data buffer;
 * dw_lenth[in]        :the length of the data buffer;
 * [return]:
 *	FTS_TRUE     :success;
 *	FTS_FALSE    :fail;
 */
FTS_BOOL i2c_read_interface(FTS_BYTE bt_ctpm_addr,
		FTS_BYTE *pbt_buf, FTS_DWRD dw_lenth)
{
	int ret;

	ret = i2c_master_recv(this_client, pbt_buf, dw_lenth);

	if (ret <= 0) {
		pr_err("[TSP]i2c_read_interface error\n");
		return FTS_FALSE;
	}

	return FTS_TRUE;
}

/*
 * [function]:
 * callback: write data to ctpm by i2c interface, implemented by special user;
 * [parameters]:
 * bt_ctpm_addr[in]    :the address of the ctpm;
 * pbt_buf[in]         :data buffer;
 * dw_lenth[in]        :the length of the data buffer;
 * [return]:
 *	FTS_TRUE     :success;
 *	FTS_FALSE    :fail;
 */
FTS_BOOL i2c_write_interface(FTS_BYTE bt_ctpm_addr,
		FTS_BYTE *pbt_buf, FTS_DWRD dw_lenth)
{
	int ret;
	ret = i2c_master_send(this_client, pbt_buf, dw_lenth);
	if (ret <= 0) {
		pr_err("[TSP]i2c_write_interface error line = %d, ret = %d\n",
				__LINE__, ret);
		return FTS_FALSE;
	}

	return FTS_TRUE;
}

/*
 * [function]:
 * send a command to ctpm.
 * [parameters]:
 * btcmd[in]      :command code;
 * btPara1[in]    :parameter 1;
 * btPara2[in]    :parameter 2;
 * btPara3[in]    :parameter 3;
 * num[in]        :the valid input parameter numbers, if only command code
 *                 needed and no parameters followed, then the num is 1;
 * [return]:
 *	FTS_TRUE  :success;
 *	FTS_FALSE :io fail;
 */
FTS_BOOL cmd_write(FTS_BYTE btcmd, FTS_BYTE btPara1,
		FTS_BYTE btPara2, FTS_BYTE btPara3, FTS_BYTE num)
{
	FTS_BYTE write_cmd[4] = {0};

	write_cmd[0] = btcmd;
	write_cmd[1] = btPara1;
	write_cmd[2] = btPara2;
	write_cmd[3] = btPara3;
	return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);
}

/*
 * [function]:
 * write data to ctpm, the destination address is 0.
 * [parameters]:
 * pbt_buf[in]    :point to data buffer;
 * bt_len[in]     :the data numbers;
 * [return]:
 *	FTS_TRUE  :success;
 *	FTS_FALSE :io fail;
 */
FTS_BOOL byte_write(FTS_BYTE *pbt_buf, FTS_DWRD dw_len)
{
	return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}

/*
 * [function]:
 * read out data from ctpm, the destination address is 0.
 * [parameters]:
 * pbt_buf[out]    :point to data buffer;
 * bt_len[in]      :the data numbers;
 * [return]:
 *	FTS_TRUE   :success;
 *	FTS_FALSE  :io fail;
 */
FTS_BOOL byte_read(FTS_BYTE *pbt_buf, FTS_BYTE bt_len)
{
	return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
}


/*
 * [function]:
 * burn the FW to ctpm.
 * [parameters]:(ref. SPEC)
 * pbt_buf[in]  :point to Head+FW ;
 * dw_lenth[in]	:the length of the FW + 6(the Head length);
 * bt_ecc[in]   :the ECC of the FW
 * [return]:
 *	ERR_OK		:no error;
 *	ERR_MODE	:fail to switch to UPDATE mode;
 *	ERR_READID	:read id fail;
 *	ERR_ERASE	:erase chip fail;
 *	ERR_STATUS	:status error;
 *	ERR_ECC		:ecc error.
 */


#define    FTS_PACKET_LENGTH        128

static unsigned char CTPM_FW[] = {
#include "ft6206_dj_konka_v713_v12_app.i"
};

E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE *pbt_buf, FTS_DWRD dw_lenth)
{
	FTS_BYTE reg_val[2] = {0};
	FTS_DWRD i = 0;

	FTS_DWRD  packet_number;
	FTS_DWRD  j;
	FTS_DWRD  temp;
	FTS_DWRD  lenght;
	FTS_BYTE  packet_buf[FTS_PACKET_LENGTH + 6];
	FTS_BYTE  auc_i2c_write_buf[10];
	FTS_BYTE bt_ecc;
	int      i_ret;
	int loop;
	/*********Step 1:Reset  CTPM *****/
	for (loop = 0; loop < 20; loop++) {
		ft5x0x_write_reg(0xbc, 0xaa);
		msleep(80);
		ft5x0x_write_reg(0xbc, 0x55);
		pr_info("[TSP] Step 1: Reset CTPM, bin-length=%d, loop=%d\n",
				dw_lenth, loop);

		msleep(15+loop*3);

		/*********Step 2:Enter upgrade mode *****/
		auc_i2c_write_buf[0] = 0x55;
		auc_i2c_write_buf[1] = 0xaa;
		do {
			i++;
			i_ret = ft5x0x_i2c_txdata(auc_i2c_write_buf, 2);
			usleep_range(5000, 5005);
		} while (i_ret <= 0 && i < 20);
		msleep(50);
		/*********Step 3:check READ-ID***********************/
		cmd_write(0x90, 0x00, 0x00, 0x00, 4);
		byte_read(reg_val, 2);
		pr_info("%s:reg_val[0]=0x%x, reg_val[1]=0x%x\n",
				__func__, reg_val[0], reg_val[1]);

		/* if (reg_val[0] == 0x79 && reg_val[1] == 0x3) 5306 */
		if (reg_val[0] == 0x79 && reg_val[1] == 0x8) { /* 6206, 6306 */
			pr_info("[TSP] Step 3: CTPM ID1 = 0x%x, ID2 = 0x%x\n",
					reg_val[0], reg_val[1]);
			break;
		} else {
			pr_err("%s: ERR_READID, ID1 = 0x%x, ID2 = 0x%x\n",
					__func__, reg_val[0], reg_val[1]);
			if (loop >= 19)
				return ERR_READID;
		}
	}
	/*********Step 4:erase app*******************************/
	cmd_write(0x61, 0x00, 0x00, 0x00, 1);

	msleep(1500);
	pr_info("[TSP] Step 4: erase.\n");

	/*********Step 5:write firmware(FW) to ctpm flash*********/
	bt_ecc = 0;
	pr_info("[TSP] Step 5: start upgrade.\n");
	dw_lenth = dw_lenth - 8;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;
	for (j = 0; j < packet_number; j++) {
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (FTS_BYTE)(temp>>8);
		packet_buf[3] = (FTS_BYTE)temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (FTS_BYTE)(lenght>>8);
		packet_buf[5] = (FTS_BYTE)lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) {
			packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6+i];
		}

		byte_write(&packet_buf[0], FTS_PACKET_LENGTH + 6);
		msleep(FTS_PACKET_LENGTH/6 + 1);
		if ((j * FTS_PACKET_LENGTH % 1024) == 0) {
			pr_info("[TSP] upgrade the 0x%x th byte.\n",
					((unsigned int)j) * FTS_PACKET_LENGTH);
		}
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (FTS_BYTE)(temp>>8);
		packet_buf[3] = (FTS_BYTE)temp;

		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (FTS_BYTE)(temp>>8);
		packet_buf[5] = (FTS_BYTE)temp;

		for (i = 0; i < temp; i++) {
			packet_buf[6+i] =
				pbt_buf[packet_number*FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6+i];
		}

		byte_write(&packet_buf[0], temp+6);
		msleep(20);
	}

	/* send the last six byte */
	for (i = 0; i < 6; i++) {
		temp = 0x6ffa + i;
		packet_buf[2] = (FTS_BYTE)(temp>>8);
		packet_buf[3] = (FTS_BYTE)temp;
		temp = 1;
		packet_buf[4] = (FTS_BYTE)(temp>>8);
		packet_buf[5] = (FTS_BYTE)temp;
		packet_buf[6] = pbt_buf[dw_lenth + i];
		bt_ecc ^= packet_buf[6];

		byte_write(&packet_buf[0], 7);
		msleep(20);
	}

	/*********Step 6: read out checksum***********************/
	/*send the opration head*/
	cmd_write(0xcc, 0x00, 0x00, 0x00, 1);
	byte_read(reg_val, 1);
	pr_info("[TSP] Step 6:  ecc read 0x%x, new firmware 0x%x.\n",
			reg_val[0], bt_ecc);
	if (reg_val[0] != bt_ecc) {
		pr_err("%s: ERR_ECC\n", __func__);
		return ERR_ECC;
	}

	/*********Step 7: reset the new FW***********************/
	cmd_write(0x07, 0x00, 0x00, 0x00, 1);

	return ERR_OK;
}

int fts_ctpm_auto_clb(void)
{
	unsigned char uc_temp;
	unsigned char i;

	TS_DBG("[FTS] start auto CLB.\n");
	msleep(200);
	ft5x0x_write_reg(0, 0x40);
	msleep(100);   /* make sure already enter factory mode */
	ft5x0x_write_reg(2, 0x4);  /* write command to start calibration */
	msleep(300);
	for (i = 0; i < 100; i++) {
		ft5x0x_read_reg(0, &uc_temp);
		/* return to normal mode, calibration finish */
		if (((uc_temp&0x70)>>4) == 0x0)
			break;
		msleep(200);
		TS_DBG("[FTS] waiting calibration %d\n", i);
	}
	TS_DBG("[FTS] calibration OK.\n");

	msleep(300);
	ft5x0x_write_reg(0, 0x40);  /* goto factory mode */
	msleep(100);   /* make sure already enter factory mode */
	ft5x0x_write_reg(2, 0x5);  /* store CLB result */
	msleep(300);
	ft5x0x_write_reg(0, 0x0); /* return to normal mode */
	msleep(300);
	TS_DBG("[FTS] store CLB result OK.\n");
	return 0;
}

#if 1
int fts_ctpm_fw_upgrade_with_i_file(void)
{
	FTS_BYTE *pbt_buf = FTS_NULL;
	int i_ret;

	/* =========FW upgrade========== */
	pbt_buf = CTPM_FW;
	/* call the upgrade function */
	i_ret =  fts_ctpm_fw_upgrade(pbt_buf, sizeof(CTPM_FW));
	if (i_ret != 0) {
		TS_DBG("[FTS] upgrade failed i_ret = %d.\n", i_ret);
		/* TBD: error handling ... */
	} else {
		TS_DBG("[FTS] upgrade successfully.\n");
		fts_ctpm_auto_clb();  /* start auto CLB */
	}

	return i_ret;
}
#endif

static int __maybe_unused fts_ctpm_fw_update(void)
{
	int ret = 0;
	const struct firmware *fw;
	unsigned char *fw_buf;
	struct platform_device *pdev;

	pdev = platform_device_register_simple("ft5206_ts", 0, NULL, 0);
	if (IS_ERR(pdev)) {
		TS_DBG("%s: Failed to register firmware\n", __func__);
		return -1;
	}

	ret = request_firmware(&fw, "ft5306_fw.bin", &pdev->dev);
	if (ret) {
		TS_DBG("%s: request_firmware error\n", __func__);
		platform_device_unregister(pdev);
		return -1;
	}

	platform_device_unregister(pdev);
	TS_DBG("%s:fw size=%zu\n", __func__, fw->size);

	fw_buf = kzalloc(fw->size, GFP_KERNEL | GFP_DMA);
	memcpy(fw_buf, fw->data, fw->size);

	fts_ctpm_fw_upgrade(fw_buf, fw->size);

	TS_DBG("%s: update finish\n", __func__);
	release_firmware(fw);
	kfree(fw_buf);

	return 0;
}

#endif

static void ft5x0x_ts_release(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
#ifdef CONFIG_FT5X0X_MULTITOUCH
	input_report_key(data->input_dev, BTN_TOUCH, 0);
#else
	input_report_abs(data->input_dev, ABS_PRESSURE, 0);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
#endif
	input_sync(data->input_dev);
	TS_DBG("%s", __func__);
}

static int ft5x0x_read_data(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	u8 buf[32] = {0};
	int ret = -1;

#ifdef TP_HAVE_PROX
	int result = 0;
	ft5x0x_read_reg(0x01, &result);
	event->prox_event = 0;
	pr_info("qiao_ft5x0x_ts_pen_irq_work result:%x\n", result);
	if (result == 0xc0 && is_incall == 1) {
		pr_info("qiao_[ft5x0x] face close 0n\n");
		event->prox_event = 1;
		event->prox_value = 1;

		input_report_abs(proximity_input_dev,
				ABS_DISTANCE, event->prox_value);
		input_sync(proximity_input_dev);
	} else if (result == 0xe0 && is_incall == 1) {
		pr_info("qiao_[ft5x0x] face far away\n");
		event->prox_event = 1;
		event->prox_value = 0;

		input_report_abs(proximity_input_dev,
				ABS_DISTANCE, event->prox_value);
		input_sync(proximity_input_dev);
	}
#endif


#ifdef CONFIG_FT5X0X_MULTITOUCH
	ret = ft5x0x_i2c_rxdata(buf, 31);
#else
	ret = ft5x0x_i2c_rxdata(buf, 7);
#endif
	if (ret < 0) {
		pr_err("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}

	memset(event, 0, sizeof(struct ts_event));
	event->touch_point = buf[2] & 0x07;

	if (event->touch_point == 0) {
		ft5x0x_ts_release();
		return 1;
	}

#ifdef CONFIG_FT5X0X_MULTITOUCH
	switch (event->touch_point) {
	case 5:
		event->x5 = (s16)(buf[0x1b] & 0x0F)<<8 | (s16)buf[0x1c];
		event->y5 = (s16)(buf[0x1d] & 0x0F)<<8 | (s16)buf[0x1e];
		TS_DBG("===x5 = %d, y5 = %d ====", event->x5, event->y5);
	case 4:
		event->x4 = (s16)(buf[0x15] & 0x0F)<<8 | (s16)buf[0x16];
		event->y4 = (s16)(buf[0x17] & 0x0F)<<8 | (s16)buf[0x18];
		TS_DBG("===x4 = %d, y4 = %d ====", event->x4, event->y4);
	case 3:
		event->x3 = (s16)(buf[0x0f] & 0x0F)<<8 | (s16)buf[0x10];
		event->y3 = (s16)(buf[0x11] & 0x0F)<<8 | (s16)buf[0x12];
		TS_DBG("===x3 = %d, y3 = %d ====", event->x3, event->y3);
	case 2:
		event->x2 = (s16)(buf[9] & 0x0F)<<8 | (s16)buf[10];
		event->y2 = (s16)(buf[11] & 0x0F)<<8 | (s16)buf[12];
		TS_DBG("===x2 = %d, y2 = %d ====", event->x2, event->y2);
	case 1:
		event->x1 = (s16)(buf[3] & 0x0F)<<8 | (s16)buf[4];
		event->y1 = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
		TS_DBG("===x1 = %d, y1 = %d ====", event->x1, event->y1);
		break;
	default:
		return -1;
	}
#else
	if (event->touch_point == 1) {
		event->x1 = (s16)(buf[3] & 0x0F)<<8 | (s16)buf[4];
		event->y1 = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
	}
#endif
	event->pressure = 200;

#if TS_DATA_THRESHOLD_CHECK
#ifdef CONFIG_FT5X0X_MULTITOUCH
	if ((event->x1 > TS_WIDTH_MAX || event->y1 > TS_HEIGHT_MAX) ||
		(event->x2 > TS_WIDTH_MAX || event->y2 > TS_HEIGHT_MAX) ||
		(event->x3 > TS_WIDTH_MAX || event->y3 > TS_HEIGHT_MAX) ||
		(event->x4 > TS_WIDTH_MAX || event->y4 > TS_HEIGHT_MAX) ||
		(event->x5 > TS_WIDTH_MAX || event->y5 > TS_HEIGHT_MAX)) {
		return -1;
	}
#else
	if (event->x1 > TS_WIDTH_MAX || event->y1 > TS_HEIGHT_MAX)
		return -1;
#endif
#endif

	return 0;
}

static void ft5x0x_report_value(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;

	if (event->touch_point)
		input_report_key(data->input_dev, BTN_TOUCH, 1);
#ifdef CONFIG_FT5X0X_MULTITOUCH
	switch (event->touch_point) {
	case 5:
		input_report_abs(data->input_dev,
				ABS_MT_TOUCH_MAJOR, event->pressure);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_X, event->x5);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_Y, event->y5);
		input_report_abs(data->input_dev,
				ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);

	case 4:
		input_report_abs(data->input_dev,
				ABS_MT_TOUCH_MAJOR, event->pressure);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_X, event->x4);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_Y, event->y4);
		input_report_abs(data->input_dev,
				ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);

	case 3:
		input_report_abs(data->input_dev,
				ABS_MT_TOUCH_MAJOR, event->pressure);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_X, event->x3);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_Y, event->y3);
		input_report_abs(data->input_dev,
				ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);

	case 2:
		input_report_abs(data->input_dev,
				ABS_MT_TOUCH_MAJOR, event->pressure);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_X, event->x2);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_Y, event->y2);
		input_report_abs(data->input_dev,
				ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);

	case 1:
		input_report_abs(data->input_dev,
				ABS_MT_TOUCH_MAJOR, event->pressure);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_X, event->x1);
		input_report_abs(data->input_dev,
				ABS_MT_POSITION_Y, event->y1);
		input_report_abs(data->input_dev,
				ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);

	default:
		break;
	}
#else	/* CONFIG_FT5X0X_MULTITOUCH */
	if (event->touch_point == 1) {
		input_report_abs(data->input_dev, ABS_X, event->x1);
		input_report_abs(data->input_dev, ABS_Y, event->y1);
		input_report_abs(data->input_dev,
				ABS_PRESSURE, event->pressure);
	}
	input_report_key(data->input_dev, BTN_TOUCH, 1);
#endif	/* CONFIG_FT5X0X_MULTITOUCH */
	input_sync(data->input_dev);
}	/*end ft5x0x_report_value*/

static void ft5x0x_ts_pen_irq_work(struct work_struct *work)
{

	int ret = -1;

	ret = ft5x0x_read_data();
	if (ret == 0)
		ft5x0x_report_value();

	enable_irq(this_client->irq);
}

static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{

	struct ft5x0x_ts_data *ft5x0x_ts = (struct ft5x0x_ts_data *)dev_id;

	disable_irq_nosync(this_client->irq);
	if (!work_pending(&ft5x0x_ts->pen_event_work)) {
		queue_work(ft5x0x_ts->ts_workqueue,
				&ft5x0x_ts->pen_event_work);
	}
	return IRQ_HANDLED;
}

static int ft5x0x_ts_reset_down(void)
{
	struct ft5x0x_ts_platform_data *pdata = g_ft5x0x_ts->platform_data;

	if (gpio_request(pdata->reset_gpio_number, "ft5x0x_reset")) {
		pr_err("Failed to request GPIO for touch_reset pin!\n");
		return -1;
	}
	gpio_direction_output(pdata->reset_gpio_number, 0);
	gpio_free(pdata->reset_gpio_number);
	return 0;
}

void ft5x0x_ts_reset(void)
{
	struct ft5x0x_ts_platform_data *pdata = g_ft5x0x_ts->platform_data;

	gpio_direction_output(pdata->reset_gpio_number, 1);
	usleep_range(1000, 1005);
	gpio_direction_output(pdata->reset_gpio_number, 0);
	usleep_range(10000, 10005);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	msleep(200);
	gpio_free(pdata->reset_gpio_number);
}


static int ft5x0x_ts_suspend(struct device *dev)
{
	struct ft5x0x_ts_platform_data *pdata = g_ft5x0x_ts->platform_data;



#ifdef TP_HAVE_PROX
	if (is_incall == 1) {
		TS_DBG("111111111");
	} else {
		TS_DBG("==ft5x0x_ts_suspend=\n");
		ft5x0x_write_reg(FT5X0X_REG_PMODE, PMODE_HIBERNATE);
	}
#else
	disable_irq_nosync(this_client->irq);
	TS_DBG("==ft5x0x_ts_suspend=\n");
	ft5x0x_write_reg(FT5X0X_REG_PMODE, PMODE_HIBERNATE);
	ft5x0x_ts_reset_down();
	pdata->set_power(0);

#endif
	return 0;
}

static int ft5x0x_ts_resume(struct device *dev)
{
	struct ft5x0x_ts_platform_data *pdata = g_ft5x0x_ts->platform_data;

	pdata->set_power(1);
	pdata->set_reset();

	ft5x0x_write_reg(FT5X0X_REG_PERIODACTIVE, 8); /* about 80HZ */
#ifdef TP_HAVE_PROX
	if (is_incall == 1)
		ft5x0x_prox_ctl(1);

#endif
	enable_irq(this_client->irq);
	return 0;
}

static const struct dev_pm_ops ft5x0x_ts_dev_pmops = {
	SET_RUNTIME_PM_OPS(ft5x0x_ts_suspend, ft5x0x_ts_resume, NULL)
};


static void ft5x0x_ts_hw_init(struct ft5x0x_ts_data *ft5x0x_ts)
{
	struct ft5x0x_ts_platform_data *pdata = ft5x0x_ts->platform_data;

	TS_DBG("%s [irq=%d];[rst=%d]\n", __func__,
			pdata->irq_gpio_number, pdata->reset_gpio_number);
	gpio_request(pdata->irq_gpio_number, "ts_irq_pin");
	gpio_request(pdata->reset_gpio_number, "ts_rst_pin");
	gpio_direction_output(pdata->reset_gpio_number, 1);
	gpio_direction_input(pdata->irq_gpio_number);
	ft5x0x_ts_reset();
	gpio_free(pdata->irq_gpio_number);
	gpio_free(pdata->reset_gpio_number);
}

#ifdef TP_HAVE_PROX
int ft5x0x_prox_ctl(int value)
{
	unsigned char  ps_store_data[2];
	if (value == 0) {
		TS_DBG("qiao_[elan]close the ps mod successful\n");
		is_incall = 0;
		ft5x0x_write_reg(0xB0, 0x00);
	} else if (value == 1) {
		TS_DBG("qiao_[elan]open the ps mod successful\n");
		is_incall = 1;
		ft5x0x_write_reg(0xB0, 0x01);
	} else {
		TS_DBG("[elan]open or close the ps mod fail\n");

	}
	return 0;
}
static ssize_t show_proximity_sensor(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	TS_DBG("tp get prox ver\n");
	if (buf != NULL)
		sprintf(buf, "tp prox version\n");

	return 0;
}

static ssize_t store_proximity_sensor(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	unsigned int on_off = simple_strtoul(buf, NULL, 10);

	TS_DBG("qiao_store_proximity_sensor buf=%d, size=%d, on_off=%d\n",
			*buf, size, on_off);
	if (buf != NULL && size != 0) {
		if (0 == on_off) {
			ft5x0x_prox_ctl(0);
			is_incall = 0;
		} else if (1 == on_off) {
			ft5x0x_prox_ctl(1);
			is_incall = 1;
		}
	}

	return size;
}

static DEVICE_ATTR(proximity, 0777, show_proximity_sensor,
		store_proximity_sensor);
#endif

#ifdef CONFIG_OF
static int touch_set_power(int on)
{
	struct i2c_client *client = g_ft5x0x_ts->client;
	static int state;
	int ret = 0;
	if (!g_ft5x0x_ts->v_tsp) {
		g_ft5x0x_ts->v_tsp = regulator_get(&client->dev,
				"focaltech, v_tsp");
		if (IS_ERR(g_ft5x0x_ts->v_tsp)) {
			g_ft5x0x_ts->v_tsp = NULL;
			pr_err("%s: enable v_tsp for touch fail!\n",
					__func__);
			return -EIO;
		}
	}
	if (!state && on) {
		state = 1;
		regulator_set_voltage(g_ft5x0x_ts->v_tsp, 2800000, 2800000);
		ret = regulator_enable(g_ft5x0x_ts->v_tsp);
		if (ret < 0)
			return -1;
	} else if (!on && state) {
		state = 0;
		regulator_disable(g_ft5x0x_ts->v_tsp);
	}

	return 0;
}

static void ft5x0x_touch_reset(void)
{
	struct ft5x0x_ts_platform_data *pdata = g_ft5x0x_ts->platform_data;

	if (gpio_request(pdata->reset_gpio_number, "ft5x0x_reset")) {
		pr_err("Failed to request GPIO for touch_reset pin!\n");
		goto out;
	}

	gpio_direction_output(pdata->reset_gpio_number, 1);
	usleep_range(1000, 1005);
	gpio_direction_output(pdata->reset_gpio_number, 0);
	usleep_range(5000, 5005);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	msleep(200);
	pr_info("ft5x0x_touch reset done.\n");
	gpio_free(pdata->reset_gpio_number);
out:
	return;
}

static const struct of_device_id ft5x0x_dt_ids[] = {
	{ .compatible = "focaltech, ft5x0x_ts", },
	{},
};
MODULE_DEVICE_TABLE(of, ft5x0x_dt_ids);

static int ft5x0x_probe_dt(struct device_node *np, struct device *dev,
		struct ft5x0x_ts_platform_data *pdata)
{
	int ret = 0;
	const struct of_device_id *match;
	if (!np) {
		dev_err(dev, "Device node not found\n");
		return -EINVAL;
	}

	match = of_match_device(ft5x0x_dt_ids, dev);
	if (!match) {
		dev_err(dev, "Compatible mismatch\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "focaltech, abs-x-max",
			&pdata->abs_x_max);
	if (ret) {
		dev_err(dev,
			"Failed to get property max_x from node(%d)\n", ret);
		return ret;
	}
	ret = of_property_read_u32(np, "focaltech, abs-y-max",
			&pdata->abs_y_max);
	if (ret) {
		dev_err(dev,
			"Failed to get property max_y from node(%d)\n", ret);
		return ret;
	}

	pdata->irq_gpio_number = of_get_named_gpio(np, "irq-gpios", 0);
	if (pdata->irq_gpio_number < 0) {
		dev_err(dev, "of_get_named_gpio irq failed.");
		return -EINVAL;
	}
	pdata->reset_gpio_number = of_get_named_gpio(np, "reset-gpios", 0);
	if (pdata->reset_gpio_number < 0) {
		dev_err(dev, "of_get_named_gpio reset failed.");
		return -EINVAL;
	}

	return 0;
}
#endif

	static int
ft5x0x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x0x_ts_data *ft5x0x_ts;
	struct input_dev *input_dev;
	struct ft5x0x_ts_platform_data *pdata = client->dev.platform_data;
	int err = 0;
	unsigned char uc_reg_value;
	struct device_node *np = client->dev.of_node;
	int ret = 0;

	pr_info("%s: ft6206 probe\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft5x0x_ts = kzalloc(sizeof(*ft5x0x_ts), GFP_KERNEL);
	if (!ft5x0x_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}
#ifdef CONFIG_OF
	pdata = kzalloc(sizeof(struct ft5x0x_ts_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = ft5x0x_probe_dt(np, &client->dev, pdata);
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to get platform data from node\n");
		goto err_probe_dt;
	}
#else
	if (!pdata) {
		dev_err(&client->dev, "Platform data not found\n");
		return -EINVAL;
	}
#endif
	pdata->set_power = touch_set_power;
	pdata->set_reset = ft5x0x_touch_reset;
	pdata->set_virtual_key = ft5x0x_ts_virtual_keys_init;
	g_ft5x0x_ts = ft5x0x_ts;
	ft5x0x_ts->platform_data = pdata;
	this_client = client;
	ft5x0x_ts->client = client;
	ft5x0x_ts_hw_init(ft5x0x_ts);
	i2c_set_clientdata(client, ft5x0x_ts);
	client->irq = gpio_to_irq(pdata->irq_gpio_number);

	err = ft5x0x_read_reg(FT5X0X_REG_CIPHER, &uc_reg_value);
	if (err < 0) {
		pr_err("ft5x0x check error %x\n", err);
		goto exit_create_singlethread;
	}
	pr_info("chip id %x\n", uc_reg_value);
	if (uc_reg_value != 0x55) {
		if (uc_reg_value == 0x06) {
			msleep(100);
			fts_ctpm_fw_upgrade_with_i_file();
		} else {
			pr_info("chip id error %x\n", uc_reg_value);
			err = -ENODEV;
			goto exit_alloc_data_failed;
		}
	}

	ft5x0x_write_reg(FT5X0X_REG_PERIODACTIVE, 8);/* about 80HZ */

	INIT_WORK(&ft5x0x_ts->pen_event_work, ft5x0x_ts_pen_irq_work);

	ft5x0x_ts->ts_workqueue =
		create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x0x_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	if (ft5x0x_ts->platform_data->set_virtual_key)
		err = ft5x0x_ts->platform_data->set_virtual_key(input_dev);
	BUG_ON(err != 0);

	ft5x0x_ts->input_dev = input_dev;

#ifdef CONFIG_FT5X0X_MULTITOUCH
	__set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	__set_bit(KEY_MENU, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);
	__set_bit(KEY_HOMEPAGE, input_dev->keybit);
	__set_bit(KEY_SEARCH, input_dev->keybit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	input_set_abs_params(input_dev,
			ABS_MT_POSITION_X, 0, pdata->abs_x_max, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_POSITION_Y, 0, pdata->abs_y_max, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
#else
	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(KEY_MENU, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);
	__set_bit(KEY_HOMEPAGE, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_X, 0, pdata->abs_x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, pdata->abs_y_max, 0, 0);
	input_set_abs_params(input_dev,
			ABS_PRESSURE, 0, PRESS_MAX, 0 , 0);
#endif

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	input_dev->name		= FT5X0X_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"ft5x0x_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

	err = request_irq(client->irq, ft5x0x_ts_interrupt,
			IRQF_TRIGGER_FALLING, client->name, ft5x0x_ts);
	if (err < 0) {
		dev_err(&client->dev,
			"ft5x0x_probe: request irq failed %d\n", err);
		goto exit_irq_request_failed;
	}

	disable_irq_nosync(client->irq);

	TS_DBG("==register_early_suspend =");
	msleep(100);
	/* get some register information */
	uc_reg_value = ft5x0x_read_fw_ver();
	pr_info("[FST] Firmware version = 0x%x\n", uc_reg_value);
	pr_info("[FST] New Firmware version = 0x%x\n",
			CTPM_FW[sizeof(CTPM_FW)-2]);

/*        if (uc_reg_value < CTPM_FW[sizeof(CTPM_FW)-2]) */
/*                fts_ctpm_fw_upgrade_with_i_file(); */

	ft5x0x_create_sysfs(client);
#ifdef FTS_CTL_IIC
	if (ft_rw_iic_drv_init(client) < 0)
		dev_err(&client->dev,
			"%s:[FTS] create fts control iic driver failed\n",
			__func__);
#endif

#ifdef TP_HAVE_PROX
	firmware_class = class_create(THIS_MODULE, "mtk-tpd");

	if (IS_ERR(firmware_class))
		TS_DBG("Failed to create class(firmware)!\n");
	firmware_cmd_dev = device_create(firmware_class,
			NULL, 0, NULL, "device");

	if (IS_ERR(firmware_cmd_dev))
		TS_DBG("Failed to create device(firmware_cmd_dev)!\n");

	if (device_create_file(firmware_cmd_dev, &dev_attr_proximity) < 0)
		TS_DBG("Failed to create device file(%s)!\n",
				dev_attr_proximity.attr.name);

	/* setup prox input device */
	proximity_input_dev = input_allocate_device();

	proximity_input_dev->name = PROXIMITY_INPUT_DEV;
	proximity_input_dev->phys = PROXIMITY_INPUT_DEV;
	proximity_input_dev->id.bustype = BUS_I2C;
	proximity_input_dev->dev.parent = &ft5x0x_ts->client->dev;
	__set_bit(EV_ABS, proximity_input_dev->evbit);
	input_set_abs_params(proximity_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	if (!proximity_input_dev) {
		err = -ENOMEM;
		TS_DBG("%s: failed to allocate input device\n", __func__);
		goto exit_input_dev_alloc_failed;
	}
	err = input_register_device(proximity_input_dev);
	if (err) {
		input_put_device(proximity_input_dev);
		TS_DBG("Unable to register prox device %d!", err);
		goto exit_input_dev_alloc_failed;
	}

#endif

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&client->dev);
	pm_runtime_forbid(&client->dev);
#endif
	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	free_irq(client->irq, ft5x0x_ts);
#ifdef CONFIG_OF
err_probe_dt:
	kfree(pdata);
#endif
exit_irq_request_failed:
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
exit_create_singlethread:
	i2c_set_clientdata(client, NULL);
	kfree(ft5x0x_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

static int  ft5x0x_ts_remove(struct i2c_client *client)
{

	struct ft5x0x_ts_data *ft5x0x_ts = i2c_get_clientdata(client);

	TS_DBG("==ft5x0x_ts_remove=\n");
#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
#endif
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&client->dev);
#endif

	free_irq(client->irq, ft5x0x_ts);
	input_unregister_device(ft5x0x_ts->input_dev);
	kfree(ft5x0x_ts);
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static void ft5x0x_ts_shutdown(struct i2c_client *client)
{
	struct ft5x0x_ts_data *ft5x0x_ts = i2c_get_clientdata(client);
	struct ft5x0x_ts_platform_data *pdata = ft5x0x_ts->platform_data;

	pdata->set_power(0);
}

static const struct i2c_device_id ft5x0x_ts_id[] = {
	{ FT5X0X_NAME, 0 }, { }
};

MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver ft5x0x_ts_driver = {
	.probe		= ft5x0x_ts_probe,
	.remove		= ft5x0x_ts_remove,
	.shutdown	= ft5x0x_ts_shutdown,
	.id_table	= ft5x0x_ts_id,
	.driver	= {
		.name	= FT5X0X_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(ft5x0x_dt_ids),
#endif
#ifdef CONFIG_PM_RUNTIME
		.pm = &ft5x0x_ts_dev_pmops,
#endif
	},
};

#if I2C_BOARD_INFO_METHOD
static int __init ft5x0x_ts_init(void)
{
	int ret;
	TS_DBG("===ft5x0x_ts_init==\n");
	ret = i2c_add_driver(&ft5x0x_ts_driver);
	return ret;
}

static void __exit ft5x0x_ts_exit(void)
{
	TS_DBG("==ft5x0x_ts_exit==\n");
	i2c_del_driver(&ft5x0x_ts_driver);
}
#else /* register i2c device&driver dynamicly */

int sprd_add_i2c_device(struct sprd_i2c_setup_data *i2c_set_data,
		struct i2c_driver *driver)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret, err;


	TS_DBG("%s : i2c_bus=%d; slave_address=0x%x; i2c_name=%s",
			__func__, i2c_set_data->i2c_bus,
			i2c_set_data->i2c_address, i2c_set_data->type);

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = i2c_set_data->i2c_address;
	strlcpy(info.type, i2c_set_data->type, I2C_NAME_SIZE);
	if (i2c_set_data->irq > 0)
		info.irq = i2c_set_data->irq;

	adapter = i2c_get_adapter(i2c_set_data->i2c_bus);
	if (!adapter) {
		pr_err("%s: can't get i2c adapter %d\n",
				__func__, i2c_set_data->i2c_bus);
		err = -ENODEV;
		goto err_driver;
	}

	client = i2c_new_device(adapter, &info);
	if (!client) {
		pr_err("%s:  can't add i2c device at 0x%x\n",
				__func__, (unsigned int)info.addr);
		err = -ENODEV;
		goto err_driver;
	}

	i2c_put_adapter(adapter);

	ret = i2c_add_driver(driver);
	if (ret != 0) {
		pr_err("%s: can't add i2c driver\n", __func__);
		err = -ENODEV;
		goto err_driver;
	}

	return 0;

err_driver:
	return err;
}

void sprd_del_i2c_device(struct i2c_client *client, struct i2c_driver *driver)
{
	TS_DBG("%s : slave_address=0x%x; i2c_name=%s",
			__func__, client->addr, client->name);
	i2c_unregister_device(client);
	i2c_del_driver(driver);
}

static int __init ft5x0x_ts_init(void)
{
	int ft5x0x_irq;

	TS_DBG("%s\n", __func__);

	ft5x0x_irq = ft5x0x_ts_config_pins();
	ft5x0x_ts_setup.i2c_bus = 0;
	ft5x0x_ts_setup.i2c_address = FT5206_TS_ADDR;
	strcpy(ft5x0x_ts_setup.type, FT5206_TS_NAME);
	ft5x0x_ts_setup.irq = ft5x0x_irq;
	return sprd_add_i2c_device(&ft5x0x_ts_setup, &ft5x0x_ts_driver);
}

static void __exit ft5x0x_ts_exit(void)
{
	TS_DBG("%s\n", __func__);
	sprd_del_i2c_device(this_client, &ft5x0x_ts_driver);
}
#endif

module_init(ft5x0x_ts_init);
module_exit(ft5x0x_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");
