/*
 * drivers/input/touchscreen/msg2133_ts.c
 *
 * FocalTech msg2133 TouchScreen driver.
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
 * VERSION		DATE			AUTHOR
 *    1.0		2010-01-05		WenFS
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
#include <linux/i2c/msg2133.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#define TS_DEBUG_MSG 0
#if TS_DEBUG_MSG
#define MSG2133_DBG(format, ...)	printk(KERN_INFO "MSG2133 " format "\n", ## __VA_ARGS__)
#else
#define MSG2133_DBG(format, ...)
#endif

static struct msg2133_ts_data *g_msg2133_ts;
static struct i2c_client *this_client;
static void msg2133_device_power_on(void);


#ifdef MSG2133_UPDATE

struct class *firmware_class;
struct device *firmware_cmd_dev;

/* Temp solution for special MENU/BACK coordinate */
/* flag be used to identify DKB or FF             */
static int virtual_key_cfg;

static void msg2133_ts_init(struct msg2133_ts_data *msg2133_ts)
{
	struct msg2133_ts_platform_data *pdata = msg2133_ts->platform_data;

	pr_info("%s [irq=%d];[rst=%d]\n",
		__func__, pdata->irq_gpio_number, pdata->reset_gpio_number);
	gpio_request(pdata->irq_gpio_number, "ts_irq_pin");
	gpio_request(pdata->reset_gpio_number, "ts_rst_pin");
	gpio_direction_output(pdata->reset_gpio_number, 0);
	msleep(100);
	gpio_direction_input(pdata->irq_gpio_number);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	msleep(20);
}

static void msg2133_reset(void)
{
	struct msg2133_ts_platform_data *pdata = g_msg2133_ts->platform_data;

	gpio_direction_output(pdata->reset_gpio_number, 0);
	msg2133_device_power_on();
	msleep(20);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	msleep(300);
}

static void i2c_write_msg2133(unsigned char *pbt_buf, int dw_lenth)
{

	this_client->addr = MSG2133_FW_ADDR;
	i2c_master_send(this_client, pbt_buf, dw_lenth);	/* 0xC4_8bit */
	this_client->addr = MSG2133_TS_ADDR;
}

static void i2c_read_update_msg2133(unsigned char *pbt_buf, int dw_lenth)
{

	this_client->addr = MSG2133_FW_UPDATE_ADDR;
	i2c_master_recv(this_client, pbt_buf, dw_lenth);	/* 0x93_8bit */
	this_client->addr = MSG2133_TS_ADDR;
}

static void i2c_write_update_msg2133(unsigned char *pbt_buf, int dw_lenth)
{
	this_client->addr = MSG2133_FW_UPDATE_ADDR;
	i2c_master_send(this_client, pbt_buf, dw_lenth);	/* 0x92_8bit */
	this_client->addr = MSG2133_TS_ADDR;
}



void dbbusDWIICEnterSerialDebugMode(void)
{
	unsigned char data[5];
	/* Enter the Serial Debug Mode */
	data[0] = 0x53;
	data[1] = 0x45;
	data[2] = 0x52;
	data[3] = 0x44;
	data[4] = 0x42;
	i2c_write_msg2133(data, 5);
}

void dbbusDWIICStopMCU(void)
{
	unsigned char data[1];
	/* Stop the MCU */
	data[0] = 0x37;
	i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICUseBus(void)
{
	unsigned char data[1];
	/* IIC Use Bus */
	data[0] = 0x35;
	i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICReshape(void)
{
	unsigned char data[1];
	/* IIC Re-shape */
	data[0] = 0x71;
	i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICNotUseBus(void)
{
	unsigned char data[1];
	/* IIC Not Use Bus */
	data[0] = 0x34;
	i2c_write_msg2133(data, 1);
}

void dbbusDWIICNotStopMCU(void)
{
	unsigned char data[1];
	/* Not Stop the MCU */
	data[0] = 0x36;
	i2c_write_msg2133(data, 1);
}

void dbbusDWIICExitSerialDebugMode(void)
{
	unsigned char data[1];
	/* Exit the Serial Debug Mode */
	data[0] = 0x45;
	i2c_write_msg2133(data, 1);
	/* Delay some interval to guard the next transaction */
}

void drvISP_EntryIspMode(void)
{
	unsigned char bWriteData[5] = {0x4D, 0x53, 0x54, 0x41, 0x52};
	i2c_write_update_msg2133(bWriteData, 5);
	/* delay about 10ms */
	usleep_range(10000, 10005);
}

void drvISP_WriteEnable(void)
{
	unsigned char bWriteData[2] = {0x10, 0x06};
	unsigned char bWriteData1 = 0x12;
	i2c_write_update_msg2133(bWriteData, 2);
	i2c_write_update_msg2133(&bWriteData1, 1);
}

/* First it needs send 0x11 to notify we want to get flash data back */
unsigned char drvISP_Read(unsigned char n, unsigned char *pDataToRead)
{
	unsigned char Read_cmd = 0x11;
	unsigned char i = 0;
	unsigned char dbbus_rx_data[16] = {0};
	i2c_write_update_msg2133(&Read_cmd, 1);
	i2c_read_update_msg2133(&dbbus_rx_data[0], n + 1);

	for (i = 0; i < n; i++)
		*(pDataToRead + i) = dbbus_rx_data[i + 1];
	return 0;
}

unsigned char drvISP_ReadStatus(void)
{
	unsigned char bReadData = 0;
	unsigned char bWriteData[2] = { 0x10, 0x05 };
	unsigned char bWriteData1 = 0x12;
	i2c_write_update_msg2133(bWriteData, 2);
	drvISP_Read(1, &bReadData);
	i2c_write_update_msg2133(&bWriteData1, 1);
	return bReadData;
}

void drvISP_BlockErase(unsigned int addr)
{
	unsigned char bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	unsigned char bWriteData1 = 0x12;
	unsigned int timeOutCount = 0;

	drvISP_WriteEnable();
	/* Enable write status register */
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x50;
	i2c_write_update_msg2133(bWriteData, 2);
	i2c_write_update_msg2133(&bWriteData1, 1);
	/* Write Status */
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x01;
	bWriteData[2] = 0x00;
	i2c_write_update_msg2133(bWriteData, 3);
	i2c_write_update_msg2133(&bWriteData1, 1);
	/* Write disable */
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x04;
	i2c_write_update_msg2133(bWriteData, 2);
	i2c_write_update_msg2133(&bWriteData1, 1);

	timeOutCount = 0;
	usleep_range(1000, 1005);
	/* delay about 100us */
	while ((drvISP_ReadStatus() & 0x01) == 0x01) {
		timeOutCount++;
		if (timeOutCount > 10000)
			break; /* around 1 sec timeout */
	}

	drvISP_WriteEnable();
	bWriteData[0] = 0x10;
	bWriteData[1] = 0xC7;
	/* Block Erase */
	i2c_write_update_msg2133(bWriteData, 2);
	i2c_write_update_msg2133(&bWriteData1, 1);

	timeOutCount = 0;
	usleep_range(1000, 1005);
	/* delay about 100us */
	while ((drvISP_ReadStatus() & 0x01) == 0x01) {
		timeOutCount++;
		if (timeOutCount > 10000)
			break; /* around 1 sec timeout */
	}
}

void drvISP_Program(unsigned short k, unsigned char *pDataToWrite)
{
	unsigned short i = 0;
	unsigned short j = 0;
	unsigned char TX_data[133];

	unsigned char bWriteData1 = 0x12;
	unsigned int addr = k * 1024;

	for (j = 0; j < 8; j++) { /* 128*8 cycle */
		TX_data[0] = 0x10;
		TX_data[1] = 0x02;	/* Page Program CMD */
		TX_data[2] = (addr + 128 * j) >> 16;
		TX_data[3] = (addr + 128 * j) >> 8;
		TX_data[4] = (addr + 128 * j);

		for (i = 0; i < 128; i++)
			TX_data[5 + i] = pDataToWrite[j * 128 + i];

		while ((drvISP_ReadStatus() & 0x01) == 0x01)
			;
		/* wait until not in write operation */

		drvISP_WriteEnable();
		i2c_write_update_msg2133(TX_data, 133);
		/* write 133 byte per cycle */
		i2c_write_update_msg2133(&bWriteData1, 1);
	}
}

void drvISP_ExitIspMode(void)
{
	unsigned char bWriteData = 0x24;
	i2c_write_update_msg2133(&bWriteData, 1);
}

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

static ssize_t firmware_update_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t firmware_update_store(struct device *dev,
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

static ssize_t firmware_clear_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t firmware_clear_store(struct device *dev,
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
static DEVICE_ATTR(clear, 0755,
	firmware_clear_show, firmware_clear_store);

void msg2133_init_class(void)
{
	firmware_class = class_create(THIS_MODULE, "ms-touchscreen-msg20xx");

	if (IS_ERR(firmware_class))
		pr_err("Failed to create class(firmware)!\n");

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

	if (device_create_file(firmware_cmd_dev, &dev_attr_clear) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_clear.attr.name);
}

#endif /* endif MSG2133_UPDATE */

static void msg2133_device_power_on(void)
{
	if (g_msg2133_ts->platform_data->set_power)
		g_msg2133_ts->platform_data->set_power(1);

	MSG2133_DBG("msg2133: power on\n");
}

static void msg2133_device_power_off(void)
{
	if (g_msg2133_ts->platform_data->set_power)
		g_msg2133_ts->platform_data->set_power(0);

	MSG2133_DBG("msg2133: power off\n");
}


#ifdef VIRTUAL_KEYS
static ssize_t virtual_keys_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
#if 0
	return sprintf(buf,
		__stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":40:525:60:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":120:525:60:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":200:525:60:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":280:525:60:60"
		"\n");
#else
	return sprintf(buf,
		__stringify(EV_KEY) ":" __stringify(KEY_BACK) ":300:900:60:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE) ":180:900:60:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":60:900:60:60"
		"\n");
#endif
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.msg2133",
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

static void virtual_keys_init(void)
{
	int ret;
	struct kobject *properties_kobj;

	MSG2133_DBG("%s\n", __func__);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
			&properties_attr_group);
	if (!properties_kobj || ret)
		pr_err("failed to create board_properties\n");
}
#endif

unsigned char msg2133_check_sum(unsigned char *pval)
{
	int i, sum = 0;

	for (i = 0; i < 7; i++)
		sum += pval[i];

	return (unsigned char)((-sum) & 0xFF);
}

static int msg2133_read_data(struct i2c_client *client)
{

	int ret, keycode;
	unsigned char reg_val[8] = {0};
	int dst_x = 0, dst_y = 0;
	unsigned int temp_checksum;
	struct TouchScreenInfo_t touchData;
	struct msg2133_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	static int error_count;
	struct msg2133_ts_platform_data *pdata = g_msg2133_ts->platform_data;

	event->touch_point = 0;
	ret = i2c_master_recv(client, reg_val, 8);
	temp_checksum = msg2133_check_sum(reg_val);
	if ((ret <= 0) || (temp_checksum != reg_val[7])) {
		MSG2133_DBG("%s: ret = %d\n", __func__, ret);
		error_count++;
		if (error_count > 10) {
			error_count = 0;
			pr_info("[TSP] ERROR: msg2133 reset");
			msg2133_reset();
		}
		return ret;
	}

	MSG2133_DBG("[TSP]---buf 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,---\n",
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
			event->x1 = 0; /* final X coordinate */
			event->y1 = 0; /* final Y coordinate */

			if ((reg_val[5] == 0x0) || (reg_val[5] == 0xFF)) {
				event->touch_point  = 0; /* touch end */
				/* TouchKeyMode */
				touchData.nTouchKeyCode = 0;
				/* TouchKeyMode */
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
					/* final X coordinate */
					event->x1 = 180;
					/* final Y coordinate */
					event->y1 = 900;
					event->touch_point  = 1;
				} else if (keycode == 0x04) {
					/* final X coordinate */
					event->x1 = 300;
					/* final Y coordinate */
					event->y1 = 900;
					event->touch_point  = 1;
				} else if (keycode == 0x08) {
					/* final X coordinate */
					event->x1 = 420;
					/* final Y coordinate */
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

static void msg2133_report_value(struct i2c_client *client)
{
	struct msg2133_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;

	MSG2133_DBG("%s, point = %d\n", __func__, event->touch_point);
	if (event->touch_point) {
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		switch (event->touch_point) {
		case 2:
			MSG2133_DBG("%s, x1=%d, y1=%d\n",
				__func__, event->x1, event->y1);
			input_report_abs(data->input_dev,
					ABS_MT_TOUCH_MAJOR, 15);
			input_report_abs(data->input_dev,
					ABS_MT_POSITION_X, event->x1);
			input_report_abs(data->input_dev,
					ABS_MT_POSITION_Y, event->y1);
			input_mt_sync(data->input_dev);
			MSG2133_DBG("%s, x2=%d, y2=%d\n",
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
			MSG2133_DBG("%s, x1=%d, y1=%d\n",
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
			MSG2133_DBG("%s, x1=%d, y1=%d\n",
				__func__, event->x1, event->y1);
		}
	} else {
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	}

	input_sync(data->input_dev);
}	/*end msg2133_report_value*/

static void msg2133_ts_pen_irq_work(struct work_struct *work)
{
	int ret = -1;
	MSG2133_DBG("%s\n", __func__);
	ret = msg2133_read_data(this_client);
	MSG2133_DBG("%s, ret = %d\n", __func__, ret);
	if (ret)
		msg2133_report_value(this_client);
	enable_irq(this_client->irq);

}

static irqreturn_t msg2133_ts_interrupt(int irq, void *dev_id)
{
	struct msg2133_ts_data *msg2133_ts = (struct msg2133_ts_data *)dev_id;

	MSG2133_DBG("%s\n", __func__);

	disable_irq_nosync(this_client->irq);
	if (!work_pending(&msg2133_ts->pen_event_work))
		queue_work(msg2133_ts->ts_workqueue,
			&msg2133_ts->pen_event_work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM_RUNTIME
static int msg2133_ts_suspend(struct device *dev)
{
	struct msg2133_ts_platform_data *pdata = g_msg2133_ts->platform_data;

	MSG2133_DBG("==%s==\n", __func__);
	pr_info("==%s==\n", __func__);
	disable_irq_nosync(this_client->irq);
	usleep_range(3000, 3005);
	gpio_direction_output(pdata->reset_gpio_number, 0);
	usleep_range(10000, 10005);
	msg2133_device_power_off();
	return 0;
}

static int msg2133_ts_resume(struct device *dev)
{
	struct msg2133_ts_platform_data *pdata = g_msg2133_ts->platform_data;

	MSG2133_DBG("==%s==start==\n", __func__);
	pr_info("==%s==start==\n", __func__);
	gpio_direction_output(pdata->reset_gpio_number, 0);
	msg2133_device_power_on();
	usleep_range(20000, 20005);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	usleep_range(100000, 100005);
	enable_irq(this_client->irq);

	return 0;
}

static const struct dev_pm_ops msg2133_ts_dev_pmops = {
		SET_RUNTIME_PM_OPS(msg2133_ts_suspend, msg2133_ts_resume, NULL)
};
#endif

#ifdef CONFIG_OF
static int touch_set_power(int on)
{
	struct i2c_client *client = g_msg2133_ts->client;
	static int state;
	int ret = 0;
	if (!g_msg2133_ts->v_tsp) {
		g_msg2133_ts->v_tsp =
			regulator_get(&client->dev, "focaltech,v_tsp");
		if (IS_ERR(g_msg2133_ts->v_tsp)) {
			g_msg2133_ts->v_tsp = NULL;
			pr_err("%s: enable v_tsp for touch fail!\n",
							__func__);
			return -EIO;
		}
	}
	if (!state && on) {
		state = 1;
		regulator_set_voltage(g_msg2133_ts->v_tsp, 2800000, 2800000);
		ret = regulator_enable(g_msg2133_ts->v_tsp);
		if (ret < 0)
			return -1;
	} else if (!on && state) {
		state = 0;
		regulator_disable(g_msg2133_ts->v_tsp);
	}

	return 0;
}

static void msg2133_touch_reset(void)
{
	struct msg2133_ts_platform_data *pdata = g_msg2133_ts->platform_data;

	if (gpio_request(pdata->reset_gpio_number, "msg2133_reset")) {
		pr_err("Failed to request GPIO for touch_reset pin!\n");
		goto out;
	}

	gpio_direction_output(pdata->reset_gpio_number, 1);
	usleep_range(1000, 1005);
	gpio_direction_output(pdata->reset_gpio_number, 0);
	usleep_range(5000, 5005);
	gpio_direction_output(pdata->reset_gpio_number, 1);
	usleep_range(200000, 200005);
	pr_info("msg2133_touch reset done.\n");
	gpio_free(pdata->reset_gpio_number);
out:
	return;
}

static const struct of_device_id msg2133_dt_ids[] = {
	{ .compatible = "focaltech,msg2133", },
	{},
};
MODULE_DEVICE_TABLE(of, msg2133_dt_ids);

static int msg2133_probe_dt(struct device_node *np, struct device *dev,
				struct msg2133_ts_platform_data *pdata)
{
	int ret = 0;
	const struct of_device_id *match;
	if (!np) {
		dev_err(dev, "Device node not found\n");
		return -EINVAL;
	}

	match = of_match_device(msg2133_dt_ids, dev);
	if (!match) {
		dev_err(dev, "Compatible mismatch\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np,
		"focaltech,abs-x-max", &pdata->abs_x_max);
	if (ret) {
		dev_err(dev,
			"Failed to get property max_x from node(%d)\n", ret);
		return ret;
	}
	ret = of_property_read_u32(np,
		"focaltech,abs-y-max", &pdata->abs_y_max);
	if (ret) {
		dev_err(dev,
			"Failed to get property max_y from node(%d)\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np,
		"focaltech,virtual_key_cfg", &virtual_key_cfg);
	if (ret)
		virtual_key_cfg = 0;

	pdata->irq_gpio_number =
		of_get_named_gpio(np, "focaltech,irq-gpios", 0);
	if (pdata->irq_gpio_number < 0) {
		MSG2133_DBG("of_get_named_gpio irq failed.");
		return -EINVAL;
	}
	pdata->reset_gpio_number =
		of_get_named_gpio(np, "focaltech,reset-gpios", 0);
	if (pdata->reset_gpio_number < 0) {
		MSG2133_DBG("of_get_named_gpio reset failed.");
		return -EINVAL;
	}

	return 0;
}

#endif

#define VIRT_KEYS(x...)  __stringify(x)
static ssize_t virtual_keys_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	if (0 == virtual_key_cfg) {
		return sprintf(buf,
				VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_MENU)
				":300:900:135:100" ":"
				VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_HOMEPAGE)
				":180:900:135:100" ":"
				VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_BACK)
				":80:900:135:100\n");
	} else {
		return sprintf(buf,
				VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_BACK)
				":300:900:135:100" ":"
				VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_HOMEPAGE)
				":180:900:135:100" ":"
				VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_MENU)
				":80:900:135:100\n");
	}
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.msg2133",
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

static int msg2133_set_virtual_key(struct input_dev *input_dev)
{
	struct kobject *props_kobj;
	int ret = 0;

	props_kobj = kobject_create_and_add("board_properties", NULL);
	if (props_kobj)
		ret = sysfs_create_group(props_kobj, &props_attr_group);
	if (!props_kobj || ret)
		pr_err("failed to create board_properties\n");

	return 0;
}

static int msg2133_ts_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct msg2133_ts_data *msg2133_ts;
	struct input_dev *input_dev;
	struct device_node *np = client->dev.of_node;
	int err = 0;
	struct msg2133_ts_platform_data *pdata = client->dev.platform_data;

	MSG2133_DBG("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	msg2133_ts = kzalloc(sizeof(*msg2133_ts), GFP_KERNEL);
	if (!msg2133_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

#ifdef CONFIG_OF
	pdata = kzalloc(sizeof(struct msg2133_ts_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	err = msg2133_probe_dt(np, &client->dev, pdata);
	if (err < 0) {
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
	pdata->set_reset = msg2133_touch_reset;
	pdata->set_virtual_key = msg2133_set_virtual_key;
	msg2133_ts->platform_data = pdata;

	g_msg2133_ts = msg2133_ts;
	msg2133_ts->client = client;
	msg2133_ts_init(msg2133_ts);

	msg2133_device_power_on();
	err = i2c_smbus_read_byte_data(client, 0x00);
	if (err < 0) {
		pr_err("read msg2133 failed\n");
		goto ts_dev_probe_fail;
	}
	i2c_set_clientdata(client, msg2133_ts);
	MSG2133_DBG("I2C addr=%x", client->addr);

	client->irq = gpio_to_irq(pdata->irq_gpio_number);
	INIT_WORK(&msg2133_ts->pen_event_work, msg2133_ts_pen_irq_work);

	msg2133_ts->ts_workqueue =
		create_singlethread_workqueue(dev_name(&client->dev));
	if (!msg2133_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		MSG2133_DBG("%s: failed to allocate input device\n", __func__);
		goto exit_input_dev_alloc_failed;
	}

	msg2133_ts->input_dev = input_dev;


	__set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);

	__set_bit(KEY_MENU,  input_dev->keybit);
	__set_bit(KEY_BACK,  input_dev->keybit);
	__set_bit(KEY_HOMEPAGE,  input_dev->keybit);
	__set_bit(KEY_SEARCH,  input_dev->keybit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0,
		pdata->abs_x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0,
		pdata->abs_y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0,
		255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,
		0, 200, 0, 0);

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_SYN, input_dev->evbit);

	input_dev->name = MSG2133_TS_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"msg2133_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

	MSG2133_DBG("%s: %s IRQ number is %d", __func__,
		client->name, client->irq);
	err = request_irq(client->irq, msg2133_ts_interrupt,
		IRQF_TRIGGER_FALLING, client->name, msg2133_ts);
	if (err < 0) {
		MSG2133_DBG("%s: msg2133_probe: request irq failed\n",
			__func__);
		goto exit_irq_request_failed;
	}

	disable_irq(client->irq);
	MSG2133_DBG("==register_early_suspend =");

#ifdef MSG2133_UPDATE
	msg2133_init_class();
#endif

	if (msg2133_ts->platform_data->set_virtual_key)
		err = msg2133_ts->platform_data->set_virtual_key(input_dev);
	BUG_ON(err != 0);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&client->dev);
	pm_runtime_forbid(&client->dev);
#endif

	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	free_irq(client->irq, msg2133_ts);

exit_irq_request_failed:
	cancel_work_sync(&msg2133_ts->pen_event_work);
	destroy_workqueue(msg2133_ts->ts_workqueue);
exit_create_singlethread:
	MSG2133_DBG("==singlethread error =\n");

	i2c_set_clientdata(client, NULL);
ts_dev_probe_fail:
	pdata->set_power(0);
	kfree(msg2133_ts);
#ifdef CONFIG_OF
err_probe_dt:
		kfree(pdata);
#endif
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

static int msg2133_ts_remove(struct i2c_client *client)
{

	struct msg2133_ts_data *msg2133_ts = i2c_get_clientdata(client);

	MSG2133_DBG("==msg2133_ts_remove=\n");

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&client->dev);
#endif
	free_irq(client->irq, msg2133_ts);
	input_unregister_device(msg2133_ts->input_dev);
	cancel_work_sync(&msg2133_ts->pen_event_work);
	destroy_workqueue(msg2133_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	kfree(msg2133_ts);
	msg2133_device_power_off();
	return 0;
}

static void msg2133_ts_shutdown(struct i2c_client *client)
{
	struct msg2133_ts_data *msg2133_ts = i2c_get_clientdata(client);
	struct msg2133_ts_platform_data *pdata = msg2133_ts->platform_data;
	pdata->set_power(0);
}


static const struct i2c_device_id msg2133_ts_id[] = {
	{ MSG2133_TS_NAME, 0 }, { }
};

MODULE_DEVICE_TABLE(i2c, msg2133_ts_id);

static struct i2c_driver msg2133_ts_driver = {
	.probe = msg2133_ts_probe,
	.remove = msg2133_ts_remove,
	.shutdown = msg2133_ts_shutdown,
	.id_table = msg2133_ts_id,
	.driver	= {
		.name = MSG2133_TS_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(msg2133_dt_ids),
#endif
#ifdef CONFIG_PM_RUNTIME
		.pm = &msg2133_ts_dev_pmops,
#endif
	},
};

static int __maybe_unused msg2133_add_i2c_device(struct i2c_board_info *info)
{
	int err;
	struct i2c_adapter *adapter;
	struct i2c_client *client;

	adapter = i2c_get_adapter(MSG2133_BUS_NUM);
	if (!adapter) {
		MSG2133_DBG("%s: can't get i2c adapter %d\n",
			__func__,  MSG2133_BUS_NUM);
		err = -ENODEV;
		goto err_driver;
	}

	client = i2c_new_device(adapter, info);
	if (!client) {
		MSG2133_DBG("%s:  can't add i2c device at 0x%x\n",
			__func__, (unsigned int)info->addr);
		err = -ENODEV;
		goto err_driver;
	}

	i2c_put_adapter(adapter);

	return 0;

err_driver:
	return err;
}

static int __init msg2133_init_module(void)
{
	MSG2133_DBG("%s\n", __func__);

	return i2c_add_driver(&msg2133_ts_driver);
}

static void __exit msg2133_exit_module(void)
{
	MSG2133_DBG("%s\n", __func__);
	i2c_del_driver(&msg2133_ts_driver);
}

module_init(msg2133_init_module);
module_exit(msg2133_exit_module);

MODULE_LICENSE("GPL");
