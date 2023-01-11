/*
 * drivers/input/touchscreen/msg2138.c
 *
 * FocalTech msg2138 TouchScreen driver.
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
 * VERSION		DATE				AUTHOR
 * 1.0			2010-01-05			WenFS
 *
 * note: only support mulititouch	Wenfs 2010-10-01
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/firmware.h>
#include <linux/platform_device.h>

#include <linux/slab.h>
#include <linux/i2c/msg2138.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <linux/wakelock.h>
#if TS_DEBUG_MSG
#define MSG2138_DBG(format, ...) \
	pr_err(KERN_INFO "MSG2138 " format "\n", ## __VA_ARGS__)
#else
#define MSG2138_DBG(format, ...)
#endif
#if 0
#define MSG2138_P_DBG(format, ...) \
	pr_err(KERN_INFO "MSG2138 psensor" format "\n", ## __VA_ARGS__)
#else
#define MSG2138_P_DBG(format, ...)
#endif
#if 0
#define MSG2138_FWUP_DBG(format, ...) \
	pr_err(KERN_INFO "MSG2138 update" format "\n", ## __VA_ARGS__)
#else
#define MSG2138_FWUP_DBG(format, ...)
#endif
#ifndef CTP_PROXIMITY_FUN
#define CTP_PROXIMITY_FUN	1	/* Mstar Mark.Li Add  2013-01-10; */
#endif
#define	__FIRMWARE_UPDATE__
#if CTP_PROXIMITY_FUN
struct class *msg2138_class;
char msg2138_name[10];
struct cdev cdev;
static dev_t msg2138_dev_number;
#define MSTARALSPS_DEVICE_NAME	"MStar-alsps-tpd-dev"
#define MSTARALSPS_INPUT_NAME	"msg2138_proximity"
#define MSTARALSPS_IOCTL_MAGIC	0XDF
#define MSTARALSPS_IOCTL_PROX_ON _IOW(MSTARALSPS_IOCTL_MAGIC, 0, char *)
#define MSTARALSPS_IOCTL_PROX_OFF _IOW(MSTARALSPS_IOCTL_MAGIC, 1, char *)
#define FW_ADDR_MSG20XX_TP	(0x4C>>1) /* device address of msg20xx */
static int ps_state;
static int mstar2138_ps_opened;
static bool ps_Enable_status;
struct input_dev *input_dev;
static struct input_dev *input_proximity;
struct hrtimer ps_timer;
static struct wake_lock pls_delayed_work_wake_lock;
static void Msg21XX_proximity_enable(bool enableflag);
ktime_t proximity_poll_delay;
#define PROXIMITY_TIMER_VAL   (300)
#endif
static int msg2138_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int msg2138_ts_remove(struct i2c_client *client);
static void msg2138_device_power_on(void);

static struct msg2138_ts_data *g_msg2138_ts;
static struct i2c_client *this_client;

#ifdef MSG2138_UPDATE

struct device *proximity_cmd_dev;


static void msg2138_ts_init(struct msg2138_ts_data *msg2138_ts)
{
	struct msg2138_ts_platform_data *pdata = msg2138_ts->platform_data;

	pr_info("%s [irq=%d];[rst=%d]\n",
			__func__, pdata->irq_gpio_number,
			pdata->reset_gpio_number);
	gpio_request(pdata->irq_gpio_number, "ts_irq_pin");
	gpio_request(pdata->reset_gpio_number, "ts_rst_pin");
	gpio_direction_output(pdata->reset_gpio_number, 0);
	msleep(100);
	gpio_direction_input(pdata->irq_gpio_number);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	msleep(20);
}


static void msg2138_reset(void)
{
	struct msg2138_ts_platform_data *pdata = g_msg2138_ts->platform_data;

	gpio_direction_output(pdata->reset_gpio_number, 0);
	msg2138_device_power_on();
	msleep(20);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	msleep(300);
}


#ifdef __FIRMWARE_UPDATE__
#define FW_ADDR_MSG21XX   (0xC4>>1)
#define FW_ADDR_MSG21XX_TP   (0x4C>>1)
#define FW_UPDATE_ADDR_MSG21XX   (0x92>>1)
static  char *fw_version;
static unsigned char temp[94][1024];
unsigned char Fmr_Loader[1024];
u32 crc_tab[256];
static unsigned char g_dwiic_info_data[8*1024];
static int FwDataCnt;
static struct class *firmware_class;
static struct device *firmware_cmd_dev;
static void HalTscrCReadI2CSeq(unsigned char addr,
		unsigned char *read_data, u16 size)
{
	int rc;

	struct i2c_msg msgs[] =	{
		{
			.addr = addr,
			.flags = I2C_M_RD,
			.len = size,
			.buf = read_data,
		},
	};

	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if (rc < 0)
		pr_err("HalTscrCReadI2CSeq error %d\n", rc);


}

static void HalTscrCDevWriteI2CSeq(unsigned char addr,
		unsigned char *data, u16 size)
{
	int rc;
	struct i2c_msg msgs[] =	{
		{
			.addr = addr,
			.flags = 0,
			.len = size,
			.buf = data,
		},
	};
	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if (rc < 0)
		pr_err("HalTscrCDevWriteI2CSeq error %d,addr = %d\n",
				rc, addr);
}

static void dbbusDWIICEnterSerialDebugMode(void)
{
	unsigned char data[5];
	/* Enter the Serial Debug Mode */
	data[0] = 0x53;
	data[1] = 0x45;
	data[2] = 0x52;
	data[3] = 0x44;
	data[4] = 0x42;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 5);
}

static void dbbusDWIICStopMCU(void)
{
	unsigned char data[1];
	/* Stop the MCU */
	data[0] = 0x37;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICUseBus(void)
{
	unsigned char data[1];
	/* IIC Use Bus */
	data[0] = 0x35;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICReshape(void)
{
	unsigned char data[1];
	/* IIC Re-shape */
	data[0] = 0x71;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICNotUseBus(void)
{
	unsigned char data[1];
	/* IIC Not Use Bus */
	data[0] = 0x34;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICNotStopMCU(void)
{
	unsigned char data[1];
	/* Not Stop the MCU */
	data[0] = 0x36;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICExitSerialDebugMode(void)
{
	unsigned char data[1];
	/* Exit the Serial Debug Mode */
	data[0] = 0x45;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
	/* Delay some interval to guard the next transaction */
	udelay(150);
}

static void drvISP_EntryIspMode(void)
{
	unsigned char bWriteData[5] = {0x4D, 0x53, 0x54, 0x41, 0x52};
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 5);
	udelay(150);
}

/* First it needs send 0x11 to notify we want to get flash data back */
static unsigned char drvISP_Read(unsigned char n, unsigned char *pDataToRead)
{
	unsigned char Read_cmd = 0x11;
	unsigned char dbbus_rx_data[2] = {0};
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &Read_cmd, 1);
	udelay(800);
	if (n == 1) {
		HalTscrCReadI2CSeq(FW_UPDATE_ADDR_MSG21XX,
				&dbbus_rx_data[0], 2);
		*pDataToRead = dbbus_rx_data[0];
		MSG2138_FWUP_DBG("dbbus=%d,%d===drvISP_Read=====\n",
				dbbus_rx_data[0], dbbus_rx_data[1]);
	} else {
		HalTscrCReadI2CSeq(FW_UPDATE_ADDR_MSG21XX, pDataToRead, n);
	}

	return 0;
}
static void drvISP_WriteEnable(void)
{
	unsigned char bWriteData[2] = {	0x10, 0x06 };
	unsigned char bWriteData1 = 0x12;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
	udelay(150);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
}

static void drvISP_ExitIspMode(void)
{
	unsigned char bWriteData = 0x24;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData, 1);
	udelay(150);
}

static unsigned char drvISP_ReadStatus(void)
{
	unsigned char bReadData = 0;
	unsigned char bWriteData[2] = { 0x10, 0x05 };
	unsigned char bWriteData1 = 0x12;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
	udelay(150);
	drvISP_Read(1, &bReadData);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
	return bReadData;
}

static void drvISP_Program(u16 k, unsigned char *pDataToWrite)
{
	u16 i = 0;
	u16 j = 0;
	unsigned char TX_data[133];

	unsigned char bWriteData1 = 0x12;
	u32 addr = k * 1024;
	u32 timeOutCount = 0;
	for (j = 0; j < 8; j++) {   /* 128*8 cycle */
		TX_data[0] = 0x10;
		TX_data[1] = 0x02; /* Page Program CMD */
		TX_data[2] = (addr + 128 * j) >> 16;
		TX_data[3] = (addr + 128 * j) >> 8;
		TX_data[4] = (addr + 128 * j);

		for (i = 0; i < 128; i++)
			TX_data[5 + i] = pDataToWrite[j * 128 + i];

		udelay(150);

		timeOutCount = 0;
		while ((drvISP_ReadStatus() & 0x01) == 0x01) {
			timeOutCount++;
			if (timeOutCount >= 100000)
				break; /* around 1 sec timeout */
		}

		drvISP_WriteEnable();
		/* write 133 byte per cycle */
		HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, TX_data, 133);
		HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
	}
}

static ssize_t firmware_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", fw_version);
}
static void _HalTscrHWReset(void)
{
	msg2138_reset();
}

static void drvISP_Verify(u16 k, unsigned char *pDataToVerify)
{
	u16 i = 0, j = 0;
	unsigned char bWriteData[5] = { 0x10, 0x03, 0, 0, 0 };
	unsigned char RX_data[256];
	unsigned char bWriteData1 = 0x12;
	u32 addr = k * 1024;
	unsigned char index = 0;
	u32 timeOutCount;
	for (j = 0; j < 8; j++)	{
		bWriteData[2] = (unsigned char)((addr + j * 128) >> 16);
		bWriteData[3] = (unsigned char)((addr + j * 128) >> 8);
		bWriteData[4] = (unsigned char)(addr + j * 128);
		udelay(100);

		timeOutCount = 0;
		while ((drvISP_ReadStatus() & 0x01) == 0x01) {
			timeOutCount++;
			if (timeOutCount >= 100000)
				break;
		}
		HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 5);
		udelay(100);
		drvISP_Read(128, RX_data);
		HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX,
				&bWriteData1, 1);
		for (i = 0; i < 128; i++) {
			if ((RX_data[i] != 0) && index < 10)
				index++;
			if (RX_data[i] != pDataToVerify[128 * j + i])
				pr_err("k=%d, j=%d, i=%d", k, j, i);
				pr_err("===Update Firmware Error===\n");
		}
	}
}

static void drvISP_ChipErase(void)
{
	unsigned char bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	unsigned char bWriteData1 = 0x12;
	u32 timeOutCount = 0;
	drvISP_WriteEnable();
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x50;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x01;
	bWriteData[2] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 3);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x04;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
	udelay(100);
	timeOutCount = 0;
	while ((drvISP_ReadStatus() & 0x01) == 0x01) {
		timeOutCount++;
		if (timeOutCount >= 100000)
			break; /* around 1 sec timeout */
	}

	drvISP_WriteEnable();
	bWriteData[0] = 0x10;
	bWriteData[1] = 0xC7;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
	udelay(100);
	timeOutCount = 0;
	while ((drvISP_ReadStatus() & 0x01) == 0x01) {
		timeOutCount++;
		if (timeOutCount >= 500000)
			break; /* around 5 sec timeout */
	}
}
static ssize_t firmware_update_c2(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char i;
	unsigned char dbbus_tx_data[4];
	unsigned char dbbus_rx_data[2] = {0};
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x11;
	dbbus_tx_data[2] = 0xE2;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	dbbus_rx_data[0] = 0;
	dbbus_rx_data[1] = 0;
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	pr_err("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x22;
	dbbus_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x23;
	dbbus_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x08;
	dbbus_tx_data[2] = 0x0c;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	pr_err("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = (dbbus_rx_data[0] | 0x20);
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x0E;
	dbbus_tx_data[3] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x10;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbusDWIICIICNotUseBus();
	dbbusDWIICNotStopMCU();
	dbbusDWIICExitSerialDebugMode();
	drvISP_EntryIspMode();
	drvISP_ChipErase();
	_HalTscrHWReset();
	mdelay(300);
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x60;
	dbbus_tx_data[3] = 0x55;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x61;
	dbbus_tx_data[3] = 0xAA;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x0F;
	dbbus_tx_data[2] = 0xE6;
	dbbus_tx_data[3] = 0x01;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x11;
	dbbus_tx_data[2] = 0xE2;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	dbbus_rx_data[0] = 0;
	dbbus_rx_data[1] = 0;
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	pr_err("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x22;
	dbbus_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x23;
	dbbus_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x08;
	dbbus_tx_data[2] = 0x0c;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	pr_err("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = (dbbus_rx_data[0] | 0x20);
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x0E;
	dbbus_tx_data[3] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x10;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbusDWIICIICNotUseBus();
	dbbusDWIICNotStopMCU();
	dbbusDWIICExitSerialDebugMode();
	drvISP_EntryIspMode();
	for (i = 0; i < 94; i++) { /* total  94 KB : 1 byte per R/W */
		drvISP_Program(i, temp[i]);	/* program to slave's flash */
		drvISP_Verify(i, temp[i]);	/* verify data */
	}
	drvISP_ExitIspMode();
	FwDataCnt = 0;
	enable_irq(this_client->irq);
	return size;
}
static u32 Reflect(u32 ref, char ch)
{
	u32 value = 0;
	u32 i = 0;
	for (i = 1; i < (ch + 1); i++) {
		if (ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}
u32 Get_CRC(u32 text, u32 prevCRC, u32 *crc32_table)
{
	u32  ulCRC = prevCRC;
	ulCRC = (ulCRC >> 8) ^ crc32_table[(ulCRC & 0xFF) ^ text];
	return ulCRC;
}
static void Init_CRC32_Table(u32 *crc32_table)
{
	u32 magicnumber = 0x04c11db7;
	u32 i = 0, j;
	for (i = 0; i <= 0xFF; i++) {
		crc32_table[i] = Reflect(i, 8) << 24;
		for (j = 0; j < 8; j++)
			crc32_table[i] = (crc32_table[i] << 1)
			^ (crc32_table[i] & (0x80000000L) ? magicnumber : 0);
		crc32_table[i] = Reflect(crc32_table[i], 32);
	}
}
enum EMEM_TYPE_t {
	EMEM_ALL = 0,
	EMEM_MAIN,
	EMEM_INFO
};

static void drvDB_WriteReg8Bit(unsigned char bank,
		unsigned char addr, unsigned char data)
{
	unsigned char tx_data[4] = {0x10, bank, addr, data};
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, tx_data, 4);
}
static void drvDB_WriteReg(unsigned char bank,
		unsigned char addr, u16 data)
{
	unsigned char tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, tx_data, 5);
}
static unsigned short drvDB_ReadReg(unsigned char bank, unsigned char addr)
{
	unsigned char tx_data[3] = {0x10, bank, addr};
	unsigned char rx_data[2] = {0};
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, tx_data, 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &rx_data[0], 2);
	return rx_data[1] << 8 | rx_data[0];
}
static int drvTP_erase_emem_c32(void)
{
	drvDB_WriteReg(0x16, 0x1E, 0xBEAF);
	drvDB_WriteReg(0x16, 0x08, 0x0000);
	drvDB_WriteReg8Bit(0x16, 0x0E, 0x10);
	mdelay(10);
	drvDB_WriteReg8Bit(0x16, 0x08, 0x04);
	drvDB_WriteReg8Bit(0x16, 0x0E, 0x10);
	mdelay(10);
	drvDB_WriteReg8Bit(0x16, 0x09, 0x60);
	drvDB_WriteReg8Bit(0x16, 0x0E, 0x10);
	drvDB_WriteReg8Bit(0x16, 0x08, 0x44);
	drvDB_WriteReg8Bit(0x16, 0x09, 0x68);
	drvDB_WriteReg8Bit(0x16, 0x08, 0xC4);
	drvDB_WriteReg8Bit(0x16, 0x08, 0xE4);
	drvDB_WriteReg8Bit(0x16, 0x09, 0x6A);
	drvDB_WriteReg8Bit(0x16, 0x0E, 0x10);
	return 1;
}
static ssize_t firmware_update_c32(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size, enum EMEM_TYPE_t emem_type)
{
	u32 i, j;
	u32 crc_main, crc_main_tp;
	u32 crc_info, crc_info_tp;
	u16 reg_data = 0;
	crc_main = 0xffffffff;
	crc_info = 0xffffffff;
#if 1
	drvTP_erase_emem_c32();
	mdelay(1000);
	_HalTscrHWReset();
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	mdelay(300);
	drvDB_WriteReg8Bit(0x3C, 0x60, 0x55);
	drvDB_WriteReg8Bit(0x3C, 0x61, 0xAA);
	do {
		reg_data = drvDB_ReadReg(0x3C, 0xE4);
	} while (reg_data != 0x1C70);

	drvDB_WriteReg(0x3C, 0xE4, 0xE38F);  /* for all-blocks */
	do {
		reg_data = drvDB_ReadReg(0x3C, 0xE4);
	} while (reg_data != 0x2F43);
	Init_CRC32_Table(&crc_tab[0]);
	for (i = 0; i < 33; i++) { /* total  33 KB : 2 byte per R/W */
		if (i < 32) {
			if (i == 31) {
				temp[i][1014] = 0x5A;
				temp[i][1015] = 0xA5;
				for (j = 0; j < 1016; j++)
					crc_main =
						Get_CRC(temp[i][j],
						crc_main, &crc_tab[0]);
			} else {
				for (j = 0; j < 1024; j++)
					crc_main =
						Get_CRC(temp[i][j],
						crc_main, &crc_tab[0]);
			}
		} else {
			for (j = 0; j < 1024; j++)
				crc_info =
					Get_CRC(temp[i][j],
					crc_info, &crc_tab[0]);
		}
		HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, temp[i], 1024);
		do {
			reg_data = drvDB_ReadReg(0x3C, 0xE4);
		} while (reg_data != 0xD0BC);
		drvDB_WriteReg(0x3C, 0xE4, 0x2F43);
	}
	drvDB_WriteReg(0x3C, 0xE4, 0x1380);
	mdelay(10);
	do {
		reg_data = drvDB_ReadReg(0x3C, 0xE4);
	} while (reg_data != 0x9432);
	crc_main = crc_main ^ 0xffffffff;
	crc_info = crc_info ^ 0xffffffff;
	crc_main_tp = drvDB_ReadReg(0x3C, 0x80);
	crc_main_tp = (crc_main_tp << 16) | drvDB_ReadReg(0x3C, 0x82);
	crc_info_tp = drvDB_ReadReg(0x3C, 0xA0);
	crc_info_tp = (crc_info_tp << 16) | drvDB_ReadReg(0x3C, 0xA2);
	MSG2138_FWUP_DBG("crc_main=0x%x, crc_info=0x%x,"
			crc_main, crc_info);
	MSG2138_FWUP_DBG("crc_main_tp=0x%x, crc_info_tp=0x%x\n",
			crc_main_tp, crc_info_tp);
	if ((crc_main_tp != crc_main) || (crc_info_tp != crc_info)) {
		pr_err("update FAILED\n");
		_HalTscrHWReset();
		FwDataCnt = 0;
		enable_irq(this_client->irq);
		return 0;
	}
	pr_info("update OK\n");
	_HalTscrHWReset();
	FwDataCnt = 0;
	enable_irq(this_client->irq);
	return size;
#endif
}

static int drvTP_erase_emem_c33(enum EMEM_TYPE_t emem_type)
{
	drvDB_WriteReg(0x0F, 0xE6, 0x0001);
	drvDB_WriteReg8Bit(0x3C, 0x60, 0x55);
	drvDB_WriteReg8Bit(0x3C, 0x61, 0xAA);
	drvDB_WriteReg8Bit(0x16, 0x1A, 0xBA);
	drvDB_WriteReg8Bit(0x16, 0x1B, 0xAB);
	drvDB_WriteReg8Bit(0x16, 0x18, 0x80);
	if (emem_type == EMEM_ALL)
		drvDB_WriteReg8Bit(0x16, 0x08, 0x10);
	drvDB_WriteReg8Bit(0x16, 0x18, 0x40);
	mdelay(10);
	drvDB_WriteReg8Bit(0x16, 0x18, 0x80);
	if (emem_type == EMEM_MAIN)
		drvDB_WriteReg8Bit(0x16, 0x0E, 0x04); /* erase main */
	else
		drvDB_WriteReg8Bit(0x16, 0x0E, 0x08); /* erase all block */
	return 1;
}

static int drvTP_read_info_dwiic_c33(void)
{
	unsigned char dwiic_tx_data[5];
	u16 reg_data = 0;
	mdelay(300);
	drvDB_WriteReg8Bit(0x3C, 0x60, 0x55);
	drvDB_WriteReg8Bit(0x3C, 0x61, 0xAA);
	drvDB_WriteReg(0x3C, 0xE4, 0xA4AB);
	drvDB_WriteReg(0x1E, 0x04, 0x7d60);
	drvDB_WriteReg(0x1E, 0x04, 0x829F);
	mdelay(1);
	dwiic_tx_data[0] = 0x10;
	dwiic_tx_data[1] = 0x0F;
	dwiic_tx_data[2] = 0xE6;
	dwiic_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dwiic_tx_data, 4);
	mdelay(100);
	do {
		reg_data = drvDB_ReadReg(0x3C, 0xE4);
	} while (reg_data != 0x5B58);
	dwiic_tx_data[0] = 0x72;
	dwiic_tx_data[1] = 0x80;
	dwiic_tx_data[2] = 0x00;
	dwiic_tx_data[3] = 0x04;
	dwiic_tx_data[4] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, dwiic_tx_data, 5);
	mdelay(50);
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX_TP, &g_dwiic_info_data[0], 1024);
	return 1;
}

static ssize_t firmware_update_c33(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size, enum EMEM_TYPE_t emem_type)
{
	unsigned char  life_counter[2];
	u32 i, j;
	u32 crc_main, crc_main_tp;
	u32 crc_info, crc_info_tp;
	int update_pass = 1;
	u16 reg_data = 0;
	crc_main = 0xffffffff;
	crc_info = 0xffffffff;
	drvTP_read_info_dwiic_c33();
	if (g_dwiic_info_data[0] == 'M' && g_dwiic_info_data[1] == 'S'
		&& g_dwiic_info_data[2] == 'T' && g_dwiic_info_data[3] == 'A'
		&& g_dwiic_info_data[4] == 'R' && g_dwiic_info_data[5] == 'T'
		&& g_dwiic_info_data[6] == 'P' && g_dwiic_info_data[7] == 'C') {
		g_dwiic_info_data[8] = temp[32][8];
		g_dwiic_info_data[9] = temp[32][9];
		g_dwiic_info_data[10] = temp[32][10];
		g_dwiic_info_data[11] = temp[32][11];
		life_counter[1] = ((((g_dwiic_info_data[13] << 8)
				| g_dwiic_info_data[12]) + 1) >> 8) & 0xFF;
		life_counter[0] = (((g_dwiic_info_data[13] << 8)
				| g_dwiic_info_data[12]) + 1) & 0xFF;
		g_dwiic_info_data[12] = life_counter[0];
		g_dwiic_info_data[13] = life_counter[1];
		drvDB_WriteReg(0x3C, 0xE4, 0x78C5);
		drvDB_WriteReg(0x1E, 0x04, 0x7d60);
		drvDB_WriteReg(0x1E, 0x04, 0x829F);
		mdelay(50);
		do {
			reg_data = drvDB_ReadReg(0x3C, 0xE4);
		} while (reg_data != 0x2F43);
		HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP,
				&g_dwiic_info_data[0], 1024);
		do {
			reg_data = drvDB_ReadReg(0x3C, 0xE4);
		} while (reg_data != 0xD0BC);
	}
	drvTP_erase_emem_c33(EMEM_MAIN);
	mdelay(1000);
	_HalTscrHWReset();
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	mdelay(300);
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		do {
			reg_data = drvDB_ReadReg(0x3C, 0xE4);
		} while (reg_data != 0x1C70);
	}
	switch (emem_type) {
	case EMEM_ALL:
		drvDB_WriteReg(0x3C, 0xE4, 0xE38F);  /* for all-blocks */
		break;
	case EMEM_MAIN:
		drvDB_WriteReg(0x3C, 0xE4, 0x7731);  /* for main block */
		break;
	case EMEM_INFO:
		drvDB_WriteReg(0x3C, 0xE4, 0x7731);  /* for info block */
		drvDB_WriteReg8Bit(0x0F, 0xE6, 0x01);
		drvDB_WriteReg8Bit(0x3C, 0xE4, 0xC5);
		drvDB_WriteReg8Bit(0x3C, 0xE5, 0x78);
		drvDB_WriteReg8Bit(0x1E, 0x04, 0x9F);
		drvDB_WriteReg8Bit(0x1E, 0x05, 0x82);
		drvDB_WriteReg8Bit(0x0F, 0xE6, 0x00);
		mdelay(100);
		break;
	}
	do {
		reg_data = drvDB_ReadReg(0x3C, 0xE4);
	} while (reg_data != 0x2F43);
	Init_CRC32_Table(&crc_tab[0]);
	for (i = 0; i < 33; i++) { /* total  33 KB : 2 byte per R/W */
		if (emem_type == EMEM_INFO)
			i = 32;
		if (i < 32) {  /* emem_main */
			if (i == 31) {
				temp[i][1014] = 0x5A;
				temp[i][1015] = 0xA5;
				for (j = 0; j < 1016; j++)
					crc_main = Get_CRC(temp[i][j],
							crc_main, &crc_tab[0]);
			} else {
				for (j = 0; j < 1024; j++)
					crc_main = Get_CRC(temp[i][j],
							crc_main, &crc_tab[0]);
			}
		} else {  /* emem_info */
			for (j = 0; j < 1024; j++)
				crc_info = Get_CRC(g_dwiic_info_data[j],
						crc_info, &crc_tab[0]);
			if (emem_type == EMEM_MAIN)
				break;
		}
		HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, temp[i], 1024);
		do {
			reg_data = drvDB_ReadReg(0x3C, 0xE4);
		} while (reg_data != 0xD0BC);
		drvDB_WriteReg(0x3C, 0xE4, 0x2F43);
	}
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN))
		drvDB_WriteReg(0x3C, 0xE4, 0x1380);
	mdelay(10); /* MCR_CLBK_DEBUG_DELAY(10, MCU_LOOP_DELAY_COUNT_MS); */
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		do {
			reg_data = drvDB_ReadReg(0x3C, 0xE4);
		} while (reg_data != 0x9432);
	}
	crc_main = crc_main ^ 0xffffffff;
	crc_info = crc_info ^ 0xffffffff;
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		crc_main_tp = drvDB_ReadReg(0x3C, 0x80);
		crc_main_tp = (crc_main_tp << 16) | drvDB_ReadReg(0x3C, 0x82);
		crc_info_tp = drvDB_ReadReg(0x3C, 0xA0);
		crc_info_tp = (crc_info_tp << 16) | drvDB_ReadReg(0x3C, 0xA2);
	}
	update_pass = 1;
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		if (crc_main_tp != crc_main)
			update_pass = 0;
		if (crc_info_tp != crc_info)
			update_pass = 0;
	}
	if (!update_pass) {
		pr_err("update FAILED\n");
		_HalTscrHWReset();
		FwDataCnt = 0;
		enable_irq(this_client->irq);
		return 0;
	}
	pr_err("update OK\n");
	_HalTscrHWReset();
	FwDataCnt = 0;
	enable_irq(this_client->irq);
	return size;
}
#define _FW_UPDATE_C3_
#ifdef _FW_UPDATE_C3_
static ssize_t firmware_update_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char dbbus_tx_data[4];
	unsigned char dbbus_rx_data[2] = {0};
	disable_irq_nosync(this_client->irq);
	_HalTscrHWReset();
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	mdelay(300);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x60;
	dbbus_tx_data[3] = 0x55;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x61;
	dbbus_tx_data[3] = 0xAA;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x0F;
	dbbus_tx_data[2] = 0xE6;
	dbbus_tx_data[3] = 0x01;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0xCC;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	if (dbbus_rx_data[0] == 2) {
		dbbus_tx_data[0] = 0x10;
		dbbus_tx_data[1] = 0x3C;
		dbbus_tx_data[2] = 0xEA;
		HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
		HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
		MSG2138_FWUP_DBG("dbbus_rx version[0]=0x%x", dbbus_rx_data[0]);
		if (dbbus_rx_data[0] == 3)
			return firmware_update_c33(dev, attr,
					buf, size, EMEM_MAIN);
		else
			return firmware_update_c32(dev, attr,
					buf, size, EMEM_ALL);
	} else
		return firmware_update_c2(dev, attr, buf, size);
}
#else
static ssize_t firmware_update_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char i;
	unsigned char dbbus_tx_data[4];
	unsigned char dbbus_rx_data[2] = {0};

	_HalTscrHWReset();

	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	mdelay(300);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x60;
	dbbus_tx_data[3] = 0x55;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x61;
	dbbus_tx_data[3] = 0xAA;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x0F;
	dbbus_tx_data[2] = 0xE6;
	dbbus_tx_data[3] = 0x01;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x11;
	dbbus_tx_data[2] = 0xE2;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	dbbus_rx_data[0] = 0;
	dbbus_rx_data[1] = 0;
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	MSG2138_FWUP_DBG("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x22;
	dbbus_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x23;
	dbbus_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x08;
	dbbus_tx_data[2] = 0x0c;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	MSG2138_FWUP_DBG("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = (dbbus_rx_data[0] | 0x20);
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x0E;
	dbbus_tx_data[3] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x10;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbusDWIICIICNotUseBus();
	dbbusDWIICNotStopMCU();
	dbbusDWIICExitSerialDebugMode();
	drvISP_EntryIspMode();
	drvISP_ChipErase();
	_HalTscrHWReset();
	mdelay(300);
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x60;
	dbbus_tx_data[3] = 0x55;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x61;
	dbbus_tx_data[3] = 0xAA;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x0F;
	dbbus_tx_data[2] = 0xE6;
	dbbus_tx_data[3] = 0x01;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x11;
	dbbus_tx_data[2] = 0xE2;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	dbbus_rx_data[0] = 0;
	dbbus_rx_data[1] = 0;
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	MSG2138_FWUP_DBG("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x22;
	dbbus_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x23;
	dbbus_tx_data[3] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x08;
	dbbus_tx_data[2] = 0x0c;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
	MSG2138_FWUP_DBG("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = (dbbus_rx_data[0] | 0x20);
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x0E;
	dbbus_tx_data[3] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x10;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, dbbus_tx_data, 4);
	dbbusDWIICIICNotUseBus();
	dbbusDWIICNotStopMCU();
	dbbusDWIICExitSerialDebugMode();
	drvISP_EntryIspMode();
	for (i = 0; i < 94; i++) { /* total  94 KB : 1 byte per R/W */
		drvISP_Program(i, temp[i]); /* program to slave's flash */
		drvISP_Verify(i, temp[i]); /* verify data */
	}
	MSG2138_FWUP_DBG("update OK\n");
	drvISP_ExitIspMode();
	FwDataCnt = 0;
	return size;
}
#endif
static ssize_t firmware_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t firmware_version_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return 0;
}

static ssize_t firmware_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t firmware_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(version, 0755,
		firmware_version_show, firmware_version_store);
static DEVICE_ATTR(update, 0755,
		firmware_update_show, firmware_update_store);
static DEVICE_ATTR(data, 0755,
		firmware_data_show, firmware_data_store);
#endif  /* __FIRMWARE_UPDATE__ */

void msg2138_init_class(void)
{
	firmware_class = class_create(THIS_MODULE, "ms-touchscreen-msg20xx");

	if (IS_ERR(firmware_class))
		pr_err("Failed to create class(firmware)!\n");
	MSG2138_FWUP_DBG("%s\n", __func__);

	firmware_cmd_dev = device_create(firmware_class,
			NULL, 0, NULL, "device");

	if (IS_ERR(firmware_cmd_dev))
		pr_err("Failed to create device(firmware_cmd_dev)!\n");

	if (device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_version.attr.name);

	if (device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_update.attr.name);

	if (device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_data.attr.name);

}

#endif /* endif MSG2138_UPDATE */

#if CTP_PROXIMITY_FUN
static ssize_t firmware_psensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	MSG2138_DBG("%s\n", __func__);

	if (ps_Enable_status)
		return sprintf(buf, "on\n");
	else
		return sprintf(buf, "off\n");
}
static ssize_t firmware_psensor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	MSG2138_DBG("%s buf = %s\n", __func__, buf);

	if (ps_Enable_status)
		Msg21XX_proximity_enable(false);
	else {
		Msg21XX_proximity_enable(true);
		input_report_abs(input_proximity, ABS_DISTANCE, 1);
		input_sync(input_proximity);
	}
	return size;
}
static DEVICE_ATTR(psensor, 0774, firmware_psensor_show,
		firmware_psensor_store);
#endif
static void msg2138_device_power_on(void)
{
	if (g_msg2138_ts->platform_data->set_power)
		g_msg2138_ts->platform_data->set_power(1);

	MSG2138_DBG("msg2138: power on\n");
}

static void msg2138_device_power_off(void)
{
	if (g_msg2138_ts->platform_data->set_power)
		g_msg2138_ts->platform_data->set_power(0);

	MSG2138_DBG("msg2138: power off\n");
}

unsigned char msg2138_check_sum(unsigned char *pval)
{
	int i, sum = 0;

	for (i = 0; i < 7; i++)
		sum += pval[i];

	return (unsigned char)((-sum) & 0xFF);
}

#if CTP_PROXIMITY_FUN
static void Msg21XX_proximity_enable(bool enableflag)
/* 1:Enable or 0:Disable  CTP Proximity function */
{
	unsigned char dbbus_tx_data[4];
	dbbus_tx_data[0] = 0x52;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x4A;
	ps_Enable_status = enableflag;
	MSG2138_P_DBG("%s\n", __func__);
	if (enableflag)
		dbbus_tx_data[3] = 0xA0;
	else
		dbbus_tx_data[3] = 0xA1;

	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG20XX_TP, &dbbus_tx_data[0], 4);
}
static enum hrtimer_restart mstar2138_pls_timer_func(struct hrtimer *timer)
{
	MSG2138_P_DBG("%s\n", __func__);
	input_report_abs(input_dev, ABS_DISTANCE, ps_state);
	input_sync(input_dev);
	hrtimer_forward_now(&ps_timer, proximity_poll_delay);
	return HRTIMER_RESTART;
}
static int mstar2138_pls_enable(void)
{
	MSG2138_P_DBG("%s\n", __func__);
	Msg21XX_proximity_enable(true);
	mstar2138_ps_opened = 1;
	input_report_abs(input_proximity, ABS_DISTANCE, 1);
	input_sync(input_proximity);
	hrtimer_start(&ps_timer, proximity_poll_delay, HRTIMER_MODE_REL);
	return 0;
}
static int mstar2138_pls_disable(void)
{
	MSG2138_P_DBG("%s\n", __func__);
	Msg21XX_proximity_enable(false);
	mstar2138_ps_opened = 0;
	hrtimer_cancel(&ps_timer);
	return 0;
}
static int mstar2138_pls_open(struct inode *inode, struct file *file)
{
	MSG2138_P_DBG(" mstar2138_ps_opened is %d\n", mstar2138_ps_opened);
	return 0;
}
static int mstar2138_pls_release(struct inode *inode, struct file *file)
{
	MSG2138_P_DBG("%s\n", __func__);
	return 0;
}
static long mstar2138_pls_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	MSG2138_P_DBG("%s\n", __func__);
	switch (cmd) {
	case MSTARALSPS_IOCTL_PROX_ON:
		wake_lock(&pls_delayed_work_wake_lock);
		mstar2138_pls_enable();
		break;
	case MSTARALSPS_IOCTL_PROX_OFF:
		mstar2138_pls_disable();
		wake_unlock(&pls_delayed_work_wake_lock);
		break;
	default:
		pr_err("%s: invalid cmd %d\n", __func__, _IOC_NR(cmd));
		return -EINVAL;
	}
	return 0;
}
static const struct file_operations mstar2138_pls_fops = {
	.owner			= THIS_MODULE,
	.open			= mstar2138_pls_open,
	.release		= mstar2138_pls_release,
	.unlocked_ioctl		= mstar2138_pls_ioctl,
};
#endif
static int msg2138_read_data(struct i2c_client *client)
{

	int ret, keycode;
	unsigned char reg_val[8] = {0};
	int dst_x = 0, dst_y = 0;
	unsigned int temp_checksum;
	struct TouchScreenInfo_t touchData;
	struct msg2138_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	static int error_count;
	struct msg2138_ts_platform_data *pdata = g_msg2138_ts->platform_data;

	event->touch_point = 0;
	ret = i2c_master_recv(client, reg_val, 8);
	temp_checksum = msg2138_check_sum(reg_val);
	if ((ret <= 0) || (temp_checksum != reg_val[7])) {
		MSG2138_DBG("%s: ret = %d\n", __func__, ret);
		error_count++;
		if (error_count > 10) {
			error_count = 0;
			pr_err("[TSP] ERROR: msg2138 reset");
			msg2138_reset();
		}
		return ret;
	}

	MSG2138_DBG("[TSP]---buf 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,---\n",
		reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4],
		reg_val[5], reg_val[6], reg_val[7]);

	event->x1 =  ((reg_val[1] & 0xF0) << 4) | reg_val[2];
	event->y1  = ((reg_val[1] & 0x0F) << 8) | reg_val[3];
	dst_x = ((reg_val[4] & 0xF0) << 4) | reg_val[5];
	dst_y = ((reg_val[4] & 0x0F) << 8) | reg_val[6];

	error_count = 0;
	if ((reg_val[0] != 0x52)) {
		return 0;
	} else {
		if ((reg_val[1] == 0xFF) && (reg_val[2] == 0xFF) &&
			(reg_val[3] == 0xFF) && (reg_val[4] == 0xFF) &&
			(reg_val[6] == 0xFF)) {
#ifdef CTP_PROXIMITY_FUN
			MSG2138_DBG("keycode = 0x%x\n", reg_val[5]);
			if (reg_val[5] == 0x80)	{
				if (ps_Enable_status == 1) {
					ps_state = 0;
					MSG2138_DBG("ps_state = %d\n",
							ps_state);
					/* report near */
					input_report_abs(input_proximity,
							ABS_DISTANCE, ps_state);
					input_sync(input_proximity);
				}
				return 0;
			} else if (reg_val[5] == 0x40) {
				if (ps_Enable_status == 1) {
					ps_state = 1;
					MSG2138_DBG("ps_state = %d\n",
							ps_state);
					/* report far */
					input_report_abs(input_proximity,
							ABS_DISTANCE, ps_state);
					input_sync(input_proximity);
				}
				return 0;
			}
#endif
			event->x1 = 0; /* final X coordinate */
			event->y1 = 0; /* final Y coordinate */

			if ((reg_val[5] == 0x0) || (reg_val[5] == 0xFF)) {
				event->touch_point  = 0; /* touch end */
				touchData.nTouchKeyCode = 0;
				touchData.nTouchKeyMode = 0;
				keycode = 0;
			} else {
				/* TouchKeyMode */
				touchData.nTouchKeyMode = 1;
				/* TouchKeyCode */
				touchData.nTouchKeyCode = reg_val[5];
				keycode = reg_val[5];
				if (keycode == 0x01) {
					/* final X coordinate */
					event->x1 = 60;
					/* final Y coordinate */
					event->y1 = 900;
					event->touch_point  = 1;
				} else if (keycode == 0x02) {
					event->x1 = 180;
					event->y1 = 900;
					event->touch_point  = 1;
				} else if (keycode == 0x04) {
					event->x1 = 300;
					event->y1 = 900;
					event->touch_point  = 1;
				} else if (keycode == 0x08) {
					event->x1 = 420;
					event->y1 = 900;
					event->touch_point  = 1;
				}
			}
		} else {
			touchData.nTouchKeyMode = 0; /* Touch on screen... */

			if ((dst_x == 0) && (dst_y == 0)) {
				event->touch_point  = 1; /* one touch */
				event->x1 =
					(event->x1 * pdata->abs_x_max) / 2048;
				event->y1 =
					(event->y1 * pdata->abs_y_max) / 2048;
			} else {
				event->touch_point  = 2; /* two touch */
				if (dst_x > 2048) {
					/* transform the unsigh
					   value to sign value */
					dst_x -= 4096;
				}
				if (dst_y > 2048)
					dst_y -= 4096;

				event->x2 = (event->x1 + dst_x);
				event->y2 = (event->y1 + dst_y);

				event->x1 =
					(event->x1 * pdata->abs_x_max) / 2048;
				event->y1 =
					(event->y1 * pdata->abs_y_max) / 2048;

				event->x2 =
					(event->x2 * pdata->abs_x_max) / 2048;
				event->y2 =
					(event->y2 * pdata->abs_y_max) / 2048;
			}
		}
		return 1;
	}
	return 1;
}

static void msg2138_report_value(struct i2c_client *client)
{
	struct msg2138_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;

	MSG2138_DBG("%s, point = %d\n", __func__, event->touch_point);
	if (event->touch_point) {
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		switch (event->touch_point) {
		case 2:
			MSG2138_DBG("%s, x1=%d, y1=%d\n",
					__func__, event->x1, event->y1);
			input_report_abs(data->input_dev,
					ABS_MT_TOUCH_MAJOR, 15);
			input_report_abs(data->input_dev,
					ABS_MT_POSITION_X, event->x1);
			input_report_abs(data->input_dev,
					ABS_MT_POSITION_Y, event->y1);
			input_mt_sync(data->input_dev);
			MSG2138_DBG("%s, x2=%d, y2=%d\n",
					__func__, event->x2, event->y2);
			input_report_abs(data->input_dev,
					ABS_MT_TOUCH_MAJOR, 15);
			input_report_abs(data->input_dev,
					ABS_MT_POSITION_X, event->x2);
			input_report_abs(data->input_dev,
					ABS_MT_POSITION_Y, event->y2);
			input_mt_sync(data->input_dev);
			break;
		case 1:
			MSG2138_DBG("%s, x1=%d, y1=%d\n",
					__func__, event->x1, event->y1);
			input_report_abs(data->input_dev,
					ABS_MT_TOUCH_MAJOR, 15);
			input_report_abs(data->input_dev,
					ABS_MT_POSITION_X, event->x1);
			input_report_abs(data->input_dev,
					ABS_MT_POSITION_Y, event->y1);
			input_mt_sync(data->input_dev);
			break;
		default:
			MSG2138_DBG("%s, x1=%d, y1=%d\n",
				__func__, event->x1, event->y1);
		}
	} else {
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	}

	input_sync(data->input_dev);
}	/* end msg2138_report_value */

static void msg2138_ts_pen_irq_work(struct work_struct *work)
{
	int ret = -1;
	ret = msg2138_read_data(this_client);
	MSG2138_DBG("%s, ret = %d\n", __func__, ret);
	if (ret)
		msg2138_report_value(this_client);
	enable_irq(this_client->irq);

}

static irqreturn_t msg2138_ts_interrupt(int irq, void *dev_id)
{
	struct msg2138_ts_data *msg2138_ts = (struct msg2138_ts_data *)dev_id;

	MSG2138_DBG("%s\n", __func__);

	disable_irq_nosync(this_client->irq);
	if (!work_pending(&msg2138_ts->pen_event_work))
		queue_work(msg2138_ts->ts_workqueue,
				&msg2138_ts->pen_event_work);
	return IRQ_HANDLED;
}

#ifdef CONFIG_PM_RUNTIME
static int msg2138_ts_suspend(struct device *dev)
{
	struct msg2138_ts_platform_data *pdata = g_msg2138_ts->platform_data;

	MSG2138_DBG("==%s== suspend mstar2138_ps_opened is %d\n",
			__func__, ps_Enable_status);
	if (ps_Enable_status) {
		pr_info("==%s== ps open\n", __func__);
		return 0;
	}
	disable_irq_nosync(this_client->irq);
	usleep_range(3000, 3005);
	gpio_direction_output(pdata->reset_gpio_number, 0);
	usleep_range(10000, 10005);
	msg2138_device_power_off();
	return 0;
}

static int msg2138_ts_resume(struct device *dev)
{
	struct msg2138_ts_platform_data *pdata = g_msg2138_ts->platform_data;

	MSG2138_DBG("==%s== resume mstar2138_ps_opened is %d\n",
			__func__, ps_Enable_status);
	if (ps_Enable_status) {
		pr_info("==%s== ps open\n", __func__);
		return 0;
	}
	gpio_direction_output(pdata->reset_gpio_number, 0);
	msg2138_device_power_on();
	usleep_range(20000, 20005);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	usleep_range(100000, 100005);
	enable_irq(this_client->irq);

	return 0;
}

static const struct dev_pm_ops msg2138_ts_dev_pmops = {
	SET_RUNTIME_PM_OPS(msg2138_ts_suspend, msg2138_ts_resume, NULL)
};
#endif

#ifdef CONFIG_OF
static int touch_set_power(int on)
{
	struct i2c_client *client = g_msg2138_ts->client;
	static int state;
	int ret = 0;
	if (!g_msg2138_ts->v_tsp) {
		g_msg2138_ts->v_tsp = regulator_get(&client->dev,
				"focaltech,v_tsp");
		if (IS_ERR(g_msg2138_ts->v_tsp)) {
			g_msg2138_ts->v_tsp = NULL;
			pr_err("%s: enable v_tsp for touch fail!\n",
					__func__);
			return -EIO;
		}
	}
	if (!state && on) {
		state = 1;
		regulator_set_voltage(g_msg2138_ts->v_tsp, 2800000, 2800000);
		ret = regulator_enable(g_msg2138_ts->v_tsp);
		if (ret < 0)
			return -1;
	} else if (!on && state) {
		state = 0;
		regulator_disable(g_msg2138_ts->v_tsp);
	}

	return 0;
}

static void msg2138_touch_reset(void)
{
	struct msg2138_ts_platform_data *pdata = g_msg2138_ts->platform_data;

	if (gpio_request(pdata->irq_gpio_number, "msg2138_reset")) {
		pr_err("Failed to request GPIO for touch_reset pin!\n");
		goto out;
	}

	gpio_direction_output(pdata->irq_gpio_number, 1);
	usleep_range(1000, 1005);
	gpio_direction_output(pdata->irq_gpio_number, 0);
	usleep_range(5000, 5005);
	gpio_direction_output(pdata->irq_gpio_number, 1);
	msleep(200);
	pr_err(KERN_INFO "msg2138_touch reset done.\n");
	gpio_free(pdata->irq_gpio_number);
out:
	return;
}

static const struct of_device_id msg2138_dt_ids[] = {
	{ .compatible = "focaltech,msg2138", },
	{},
};
MODULE_DEVICE_TABLE(of, msg2138_dt_ids);

static int msg2138_probe_dt(struct device_node *np, struct device *dev,
		struct msg2138_ts_platform_data *pdata)
{
	int ret = 0;
	const struct of_device_id *match;
	if (!np) {
		dev_err(dev, "Device node not found\n");
		return -EINVAL;
	}

	match = of_match_device(msg2138_dt_ids, dev);
	if (!match) {
		dev_err(dev, "Compatible mismatch\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "focaltech,abs-x-max",
			&pdata->abs_x_max);
	if (ret) {
		dev_err(dev, "Failed to get property max_x from node(%d)\n",
				ret);
		return ret;
	}
	ret = of_property_read_u32(np, "focaltech,abs-y-max",
			&pdata->abs_y_max);
	if (ret) {
		dev_err(dev, "Failed to get property max_y from node(%d)\n",
				ret);
		return ret;
	}

	pdata->irq_gpio_number = of_get_named_gpio(np, "irq-gpios", 0);
	if (pdata->irq_gpio_number < 0) {
		MSG2138_DBG("of_get_named_gpio irq failed.");
		return -EINVAL;
	}
	pdata->reset_gpio_number = of_get_named_gpio(np, "reset-gpios", 0);
	if (pdata->reset_gpio_number < 0) {
		MSG2138_DBG("of_get_named_gpio reset failed.");
		return -EINVAL;
	}

	return 0;
}

#endif

#define VIRT_KEYS(x...)  __stringify(x)
static ssize_t virtual_keys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
			VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_BACK)
			":300:900:135:100" ":"
			VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_HOMEPAGE)
			":180:900:135:100" ":"
			VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_MENU)
			":80:900:135:100\n");
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.msg2138",
		.mode = S_IRUGO,
	},
	.show = &virtual_keys_show,
};

static struct attribute *props_attrs[] = {
	&virtual_keys_attr.attr,
	NULL
};

static struct attribute_group props_attr_group = {
	.attrs = props_attrs,
};

static int msg2138_set_virtual_key(struct input_dev *input_dev)
{
	struct kobject *props_kobj;
	int ret = 0;

	MSG2138_DBG("%s\n", __func__);
	props_kobj = kobject_create_and_add("board_properties", NULL);
	if (props_kobj)
		ret = sysfs_create_group(props_kobj, &props_attr_group);
	if (!props_kobj || ret)
		pr_err("failed to create board_properties\n");

	return 0;
}
static const struct i2c_device_id msg2138_ts_id[] = {
	{ MSG2138_TS_NAME, 0 }, { }
};

MODULE_DEVICE_TABLE(i2c, msg2138_ts_id);
static struct i2c_driver msg2138_ts_driver = {
	.probe		= msg2138_ts_probe,
	.remove		= msg2138_ts_remove,
	.id_table	= msg2138_ts_id,
	.driver	= {
		.name	= MSG2138_TS_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(msg2138_dt_ids),
#endif
#ifdef CONFIG_PM_RUNTIME
		.pm = &msg2138_ts_dev_pmops,
#endif
	},
};
static int msg2138_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct msg2138_ts_data *msg2138_ts;
	struct device_node *np = client->dev.of_node;
	int err = 0;
	int ret = 0;
	struct msg2138_ts_platform_data *pdata = client->dev.platform_data;

	MSG2138_DBG("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	msg2138_ts = kzalloc(sizeof(*msg2138_ts), GFP_KERNEL);
	if (!msg2138_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

#ifdef CONFIG_OF
	pdata = kzalloc(sizeof(struct msg2138_ts_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = msg2138_probe_dt(np, &client->dev, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to get platform data from node\n");
		goto err_probe_dt;
	}
#else
	if (!pdata) {
		dev_err(&client->dev, "Platform data not found\n");
		return -EINVAL;
	}
#endif

	this_client = client;
	pdata->set_power = touch_set_power;
	pdata->set_reset = msg2138_touch_reset;
	pdata->set_virtual_key = msg2138_set_virtual_key;
	msg2138_ts->platform_data = pdata;
	g_msg2138_ts = msg2138_ts;
	msg2138_ts->client = client;
	msg2138_ts_init(msg2138_ts);

	msg2138_device_power_on();
	i2c_set_clientdata(client, msg2138_ts);

	MSG2138_DBG("I2C addr=%x", client->addr);
	client->irq = gpio_to_irq(pdata->irq_gpio_number);
	INIT_WORK(&msg2138_ts->pen_event_work, msg2138_ts_pen_irq_work);

	msg2138_ts->ts_workqueue =
		create_singlethread_workqueue(dev_name(&client->dev));
	if (!msg2138_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		MSG2138_DBG("%s: failed to allocate input device\n", __func__);
		goto exit_input_dev_alloc_failed;
	}

	msg2138_ts->input_dev = input_dev;

	__set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);

	__set_bit(KEY_MENU,  input_dev->keybit);
	__set_bit(KEY_BACK,  input_dev->keybit);
	__set_bit(KEY_HOMEPAGE,  input_dev->keybit);
	__set_bit(KEY_SEARCH,  input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0,
			pdata->abs_x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0,
			pdata->abs_y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0,
			255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0,
			200, 0, 0);

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	input_dev->name	= MSG2138_TS_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"msg2138_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

	MSG2138_DBG("%s: %s IRQ number is %d", __func__,
			client->name, client->irq);
	err = request_irq(client->irq, msg2138_ts_interrupt,
			IRQF_TRIGGER_FALLING, client->name, msg2138_ts);
	if (err < 0) {
		MSG2138_DBG("%s: msg2138_probe: request irq failed\n",
				__func__);
		goto exit_irq_request_failed;
	}

	disable_irq(client->irq);
#ifdef MSG2138_UPDATE
	msg2138_init_class();
#endif

	if (msg2138_ts->platform_data->set_virtual_key)
		err = msg2138_ts->platform_data->set_virtual_key(input_dev);
	BUG_ON(err != 0);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&client->dev);
	pm_runtime_forbid(&client->dev);
#endif

#if CTP_PROXIMITY_FUN
	err = alloc_chrdev_region(&msg2138_dev_number, 0, 3,
			MSTARALSPS_INPUT_NAME);
	if (err < 0) {
		pr_err("msg2138: alloc_chrdev_region() failed\n");
		goto alloc_chrdev_region_failed;
	}

	cdev_init(&cdev, &mstar2138_pls_fops);
	strcpy(msg2138_name, MSTARALSPS_INPUT_NAME);
	cdev.owner = THIS_MODULE;
	err = cdev_add(&cdev, msg2138_dev_number, 1);
	if (ret < 0) {
		pr_err("msg2138:cdev_add fail in msg2138_init()\n");
		goto error_cdev_cdev_add;
	}
	msg2138_class = class_create(THIS_MODULE, MSTARALSPS_INPUT_NAME);
	proximity_cmd_dev = device_create(msg2138_class,
			NULL, MKDEV(MAJOR(msg2138_dev_number), 0),
			&msg2138_ts_driver, "msg2138_proximity");
	if (IS_ERR(proximity_cmd_dev))
		pr_err("Failed to create device(proximity_cmd_dev)!\n");
	if (device_create_file(proximity_cmd_dev, &dev_attr_psensor) < 0) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_psensor.attr.name);
		goto free_fw_name_psensor;
	}
	input_proximity = input_allocate_device();
	if (!input_proximity) {
		input_free_device(input_proximity);
		pr_err("%s: input allocate device failed\n", __func__);
	} else {
		input_proximity->name = MSTARALSPS_INPUT_NAME;
		input_proximity->phys  = MSTARALSPS_INPUT_NAME;
		input_proximity->id.bustype = BUS_I2C;
		input_proximity->id.vendor = 0x0001;
		input_proximity->id.product = 0x0001;
		input_proximity->id.version = 0x0010;
		__set_bit(EV_ABS, input_proximity->evbit);
		input_set_abs_params(input_proximity,
				ABS_DISTANCE, 0, 1, 0, 0);
		input_set_abs_params(input_proximity,
				ABS_MISC, 0, 100001, 0, 0);
		err = input_register_device(input_proximity);
		if (err < 0)
			dev_err(&client->dev,
				"%s: input device regist failed err = %d\n",
				__func__, err);
		wake_lock_init(&pls_delayed_work_wake_lock,
				WAKE_LOCK_SUSPEND, "msg2138-wake-lock");
		hrtimer_init(&ps_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		proximity_poll_delay =
			ns_to_ktime(PROXIMITY_TIMER_VAL * NSEC_PER_MSEC);
		ps_timer.function = mstar2138_pls_timer_func;
	}
#endif

	return 0;
free_fw_name_psensor:
error_cdev_cdev_add:
	unregister_chrdev_region(msg2138_dev_number, 3);
alloc_chrdev_region_failed:
	input_free_device(input_dev);
exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	free_irq(client->irq, msg2138_ts);

#ifdef CONFIG_OF
err_probe_dt:
	kfree(pdata);
#endif

exit_irq_request_failed:
	cancel_work_sync(&msg2138_ts->pen_event_work);
	destroy_workqueue(msg2138_ts->ts_workqueue);
exit_create_singlethread:
	MSG2138_DBG("==singlethread error =\n");
	i2c_set_clientdata(client, NULL);
	kfree(msg2138_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

static int msg2138_ts_remove(struct i2c_client *client)
{

	struct msg2138_ts_data *msg2138_ts = i2c_get_clientdata(client);

	MSG2138_DBG("==msg2138_ts_remove=\n");
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&client->dev);
#endif
	free_irq(client->irq, msg2138_ts);
	input_unregister_device(msg2138_ts->input_dev);
	kfree(msg2138_ts);
	cancel_work_sync(&msg2138_ts->pen_event_work);
	destroy_workqueue(msg2138_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);

	msg2138_device_power_off();
	return 0;
}



static int __maybe_unused msg2138_add_i2c_device(struct i2c_board_info *info)
{
	int err;
	struct i2c_adapter *adapter;
	struct i2c_client *client;

	adapter = i2c_get_adapter(MSG2138_BUS_NUM);
	if (!adapter) {
		MSG2138_DBG("%s: can't get i2c adapter %d\n",
				__func__,  MSG2138_BUS_NUM);
		err = -ENODEV;
		goto err_driver;
	}

	client = i2c_new_device(adapter, info);
	if (!client) {
		MSG2138_DBG("%s:  can't add i2c device at 0x%x\n",
				__func__, (unsigned int)info->addr);
		err = -ENODEV;
		goto err_driver;
	}

	i2c_put_adapter(adapter);

	return 0;

err_driver:
	return err;
}

static int __init msg2138_init_module(void)
{
	MSG2138_DBG("%s\n", __func__);
	return i2c_add_driver(&msg2138_ts_driver);
}

static void __exit msg2138_exit_module(void)
{
	MSG2138_DBG("%s\n", __func__);
	i2c_del_driver(&msg2138_ts_driver);
}

module_init(msg2138_init_module);
module_exit(msg2138_exit_module);

MODULE_LICENSE("GPL");
