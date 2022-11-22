/*
 * file abov_sar.c
 * brief abov Driver for two channel SAP using
 *
 * Driver for the ABOV
 * Copyright (c) 2015-2016 ABOV Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG
#define DRIVER_NAME "abov_sar"
#define USE_SENSORS_CLASS

#define MAX_WRITE_ARRAY_SIZE 32
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/sensors.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/power_supply.h>
#include <linux/hardware_info.h>
#include <linux/input/abov_sar_single.h> /* main struct, interrupt,init,pointers */

#define BOOT_UPDATE_ABOV_FIRMWARE 1

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/async.h>
#include <linux/firmware.h>
#define SLEEP(x)	mdelay(x)

#define C_I2C_FIFO_SIZE 8

#define I2C_MASK_FLAG	(0x00ff)

static u8 checksum_h;
static u8 checksum_h_bin;
static u8 checksum_l;
static u8 checksum_l_bin;

#define IDLE 0
#define ACTIVE 1
#define S_PROX   1
#define S_BODY   2
#define SAR_FAR   0
#define SAR_CLOSED   1
#define FACTORY_VERSION

#define ABOV_DEBUG 1
#define LOG_TAG "ABOV"

//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
#define TEMPORARY_HOLD_TIME	500
struct wakeup_source wakesrc;
//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

#if ABOV_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

#define LOG_DBG(fmt, args...)	pr_info(LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)   pr_err(LOG_TAG fmt, ##args)

static int mEnabled =0;
//+bug 483940 ,lishuai_wt,20190911,add ,sar sensor threshold
static int backcapTemp =0;
static int checkcapTemp =0;
//-bug 483940 ,lishuai_wt,20190911,add ,sar sensor threshold
static int programming_done = 0;

pabovXX_t abov_sar_ptr;
static int abov_tk_fw_mode_enter(struct i2c_client *client);

/**
 * struct abov
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct abov {
	pbuttonInformation_t pbuttonInformation;
	pabov_platform_data_t hw; /* specific platform data settings */
} abov_t, *pabov_t;

extern char Sar_name[HARDWARE_MAX_ITEM_LONGTH];

static void ForcetoTouched(pabovXX_t this)
{
	pabov_t pDevice = NULL;
	struct input_dev *input_bottom = NULL;
	struct _buttonInfo *pCurrentButton  = NULL;

	pDevice = this->pDevice;
	if (this && pDevice) {
		LOG_INFO("ForcetoTouched()\n");

		pCurrentButton = pDevice->pbuttonInformation->buttons;
		input_bottom = pDevice->pbuttonInformation->input_bottom;
		pCurrentButton->state = ACTIVE;
		if (mEnabled) {
			input_report_abs(input_bottom, ABS_DISTANCE, 1);
			input_sync(input_bottom);
		}
		LOG_INFO("Leaving ForcetoTouched()\n");
	}
}

/**
 * fn static int write_register(pabovXX_t this, u8 address, u8 value)
 * brief Sends a write register to the device
 * param this Pointer to main parent struct
 * param address 8-bit register address
 * param value   8-bit register value to write to address
 * return Value from i2c_master_send
 */
static int write_register(pabovXX_t this, u8 address, u8 value)
{
	struct i2c_client *i2c = 0;
	char buffer[2];
	int returnValue = 0;

	buffer[0] = address;
	buffer[1] = value;
	returnValue = -ENOMEM;
	if (this && this->bus) {
		i2c = this->bus;

		returnValue = i2c_master_send(i2c, buffer, 2);
		LOG_INFO("write_register Addr: 0x%x Val: 0x%x Return: %d\n",
				address, value, returnValue);
	}
	if (returnValue < 0) {
		ForcetoTouched(this);
		LOG_DBG("Write_register-Fail\n");
	}
	return returnValue;
}

/**
 * fn static int read_register(pabovXX_t this, u8 address, u8 *value)
 * brief Reads a register's value from the device
 * param this Pointer to main parent struct
 * param address 8-Bit address to read from
 * param value Pointer to 8-bit value to save register value to
 * return Value from i2c_smbus_read_byte_data if < 0. else 0
 */
static int read_register(pabovXX_t this, u8 address, u8 *value)
{
	struct i2c_client *i2c = 0;
	s32 returnValue = 0;

	if (this && value && this->bus) {
		i2c = this->bus;
		returnValue = i2c_smbus_read_byte_data(i2c, address);
		LOG_INFO("read_register Addr: 0x%x Return: 0x%x\n",
				address, returnValue);
		if (returnValue >= 0) {
			*value = returnValue;
			return 0;
		} else {
			return returnValue;
		}
	}
	ForcetoTouched(this);
	LOG_INFO("read_register-Fail\n");
	return -ENOMEM;
}

/**
 * detect if abov exist or not
 * return 1 if chip exist else return 0
 */
static int abov_detect(struct i2c_client *client)
{
	s32 returnValue = 0, i;
	u8 address = ABOV_ABOV_WHOAMI_REG;
	u8 value = 0xAB;

	if (client) {
		for (i = 0; i < 3; i++) {
			returnValue = i2c_smbus_read_byte_data(client, address);
			LOG_INFO("abov read_register for %d time Addr: 0x%02x Return: 0x%02x\n",
					i, address, returnValue);
			if (returnValue >= 0) {
				if (value == returnValue) {
					LOG_INFO("abov detect success!\n");
					return 1;
				}
			}
		}
		for (i = 0; i < 3; i++) {
				if(abov_tk_fw_mode_enter(client) == 0) {
					LOG_INFO("abov boot detect success!\n");
					return 2;
			}
		}
	}
	LOG_INFO("abov detect failed!!!\n");
	return 0;
}

/**
 * brief Perform a manual offset calibration
 * param this Pointer to main parent struct
 * return Value return value from the write register
 */
static int manual_offset_calibration(pabovXX_t this)
{
	s32 returnValue = 0;

	returnValue = write_register(this, ABOV_RECALI_REG, 0x01);
	return returnValue;
}

/**
 * brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static ssize_t manual_offset_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 reg_value = 0;
	pabovXX_t this = dev_get_drvdata(dev);

	LOG_INFO("Reading IRQSTAT_REG\n");
	read_register(this, ABOV_IRQSTAT_REG, &reg_value);
	return scnprintf(buf, PAGE_SIZE, "%d\n", reg_value);
}

/* brief sysfs store function for manual calibration */
static ssize_t manual_offset_calibration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;
	if (val) {
		LOG_INFO("Performing manual_offset_calibration()\n");
		manual_offset_calibration(this);
	}
	return count;
}

static DEVICE_ATTR(calibrate, 0644, manual_offset_calibration_show,
		manual_offset_calibration_store);
static struct attribute *abov_attributes[] = {
	&dev_attr_calibrate.attr,
	NULL,
};
static struct attribute_group abov_attr_group = {
	.attrs = abov_attributes,
};

/**
 * brief  Initialize I2C config from platform data
 * param this Pointer to main parent struct
 *
 */
static void hw_init(pabovXX_t this)
{
	pabov_t pDevice = 0;
	pabov_platform_data_t pdata = 0;
	int i = 0;
	/* configure device */
	LOG_INFO("Going to Setup I2C Registers\n");
	pDevice = this->pDevice;
	pdata = pDevice->hw;
	if (this && pDevice && pdata) {
		while (i < pdata->i2c_reg_num) {
			/* Write all registers/values contained in i2c_reg */
			LOG_INFO("Going to Write Reg: 0x%x Value: 0x%x\n",
					pdata->pi2c_reg[i].reg, pdata->pi2c_reg[i].val);
			write_register(this, pdata->pi2c_reg[i].reg,
					pdata->pi2c_reg[i].val);
			i++;
		}
	} else {
		LOG_DBG("ERROR! platform data 0x%p\n", pDevice->hw);
		/* Force to touched if error */
		ForcetoTouched(this);
	}
}

/**
 * fn static int initialize(pabovXX_t this)
 * brief Performs all initialization needed to configure the device
 * param this Pointer to main parent struct
 * return Last used command's return value (negative if error)
 */
static int initialize(pabovXX_t this)
{
	if (this) {
		LOG_INFO("enter initialize().\n");

		hw_init(this);
		msleep(300); /* make sure everything is running */

		LOG_INFO("Exiting initialize(). \n");
		programming_done = ACTIVE;
		return 0;
	}

	programming_done = IDLE;
	return -ENOMEM;
}

/**
 * brief Handle what to do when a touch occurs
 * param this Pointer to main parent struct
 */
static void touchProcess(pabovXX_t this)
{
	u8 sar_value = 0;
	pabov_t pDevice = NULL;
	struct input_dev *input_bottom = NULL;

	pDevice = this->pDevice;
	if (this && pDevice) {
		LOG_INFO("Inside touchProcess()\n");
		read_register(this, ABOV_IRQSTAT_REG, &sar_value);

		input_bottom = pDevice->pbuttonInformation->input_bottom;

		if (unlikely (input_bottom == NULL)) {
			LOG_DBG("ERROR!! input_bottom NULL!!!\n");
			return;
		}
		switch (sar_value & 0x01) {
		case SAR_FAR: /* Button is being in far state! */
			LOG_INFO("Sar State=far.\n");
			input_report_abs(input_bottom, ABS_DISTANCE, 0);
			input_sync(input_bottom);
			break;
		case SAR_CLOSED: /* Button is being in proximity! */
			LOG_INFO("Sar State=near.\n");
			input_report_abs(input_bottom, ABS_DISTANCE, 1);
			input_sync(input_bottom);
			break;
		default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
			break;
			};
	}
	LOG_INFO("Leaving touchProcess()\n");
}

static int abov_get_nirq_state(unsigned irq_gpio)
{
	if (irq_gpio) {
		return !gpio_get_value(irq_gpio);
	} else {
		LOG_INFO("abov irq_gpio is not set.");
		return -EINVAL;
	}
}

static struct _totalButtonInformation smtcButtonInformation = {
	.buttons = psmtcButtons,
	.buttonSize = ARRAY_SIZE(psmtcButtons),
};

static void abov_platform_data_of_init(struct i2c_client *client,
		pabov_platform_data_t pplatData)
{
	struct device_node *np = client->dev.of_node;
	int ret;
	//static const char *sar_info;

	client->irq = of_get_named_gpio_flags(np, "abov,irq-gpio",
                      0, &pplatData->irq_gpio_flags);

	pplatData->irq_gpio = client->irq;

	strlcpy(Sar_name, "A96T346HW", HARDWARE_MAX_ITEM_LONGTH);

	pplatData->get_is_nirq_low = abov_get_nirq_state;
	pplatData->init_platform_hw = NULL;
	/*  pointer to an exit function. Here in case needed in the future */
	/*
	 *.exit_platform_hw = abov_exit_ts,
	 */
	pplatData->exit_platform_hw = NULL;

	pplatData->pi2c_reg = abov_i2c_reg_setup;
	pplatData->i2c_reg_num = ARRAY_SIZE(abov_i2c_reg_setup);

	pplatData->pbuttonInformation = &smtcButtonInformation;

	ret = of_property_read_string(np, "label", &pplatData->fw_name);
	if (ret < 0) {
		LOG_DBG("firmware name read error!\n");
		return;
	}
}

static ssize_t capsense_mcalibrate_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 8, "%d\n", programming_done);
}

static ssize_t capsense_mcalibrate_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	pabov_t pDevice = NULL;
	struct input_dev *input_bottom = NULL;

	pDevice = this->pDevice;
	input_bottom = pDevice->pbuttonInformation->input_bottom;

	if (!count || (this == NULL))
		return -EINVAL;

	if ( !strncmp(buf, "1", 1))
		write_register(this, ABOV_RECALI_REG, 0x01);

	input_report_abs(input_bottom, ABS_DISTANCE, 0);
	input_sync(input_bottom);

	return count;
}

static CLASS_ATTR(mcalibrate, 0660, capsense_mcalibrate_show, capsense_mcalibrate_store);



static ssize_t capsense_enable_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 8, "%d\n", mEnabled);
}

static ssize_t capsense_enable_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	pabov_t pDevice = NULL;
	struct input_dev *input_bottom = NULL;
	u8 sar_value = 0;

	pDevice = this->pDevice;
	input_bottom = pDevice->pbuttonInformation->input_bottom;

	if (!count || (this == NULL))
		return -EINVAL;

	if (!strncmp(buf, "1", 1)) {
		LOG_DBG("enable capsensor this->irq_disabled:%d,\n", this->irq_disabled);
		if(this->irq_disabled == 0){
			enable_irq(this->irq);
			this->irq_disabled = 1;
			LOG_DBG("abov enable_irq\n");
		}

		write_register(this, ABOV_CTRL_MODE_RET, 0x00);
		write_register(this, ABOV_RECALI_REG, 0x01);
		msleep(500);

		read_register(this, ABOV_IRQSTAT_REG, &sar_value);
		LOG_DBG("enable capsensor sar_value=%d, \n", sar_value);
		input_report_abs(input_bottom, ABS_DISTANCE, 0);
		input_sync(input_bottom);
		mEnabled = 1;
	} else if (!strncmp(buf, "0", 1)) {
		LOG_DBG("disable capsensor\n");
		if(this->irq_disabled == 1){
			disable_irq(this->irq);
			this->irq_disabled = 0;
			LOG_DBG("capsensor disable_irq\n");
		}
		write_register(this, ABOV_CTRL_MODE_RET, 0x02);

		input_report_abs(input_bottom, ABS_DISTANCE, -1);
		input_sync(input_bottom);
		mEnabled = 0;
	} else {
		LOG_DBG("unknown enable symbol\n");
	}

		return count;
}

#ifdef USE_SENSORS_CLASS
static int capsensor_set_enable(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	pabovXX_t this = abov_sar_ptr;
	pabov_t pDevice = NULL;
	struct input_dev *input_bottom = NULL;
	u8 sar_value =0;

	pDevice = this->pDevice;
	input_bottom = pDevice->pbuttonInformation->input_bottom;

	if (enable == 1) {
		LOG_DBG("enable cap sensor\n");

		if(this->irq_disabled == 0){
			enable_irq(this->irq);
			this->irq_disabled = 1;
			LOG_DBG("abov enable_irq\n");
		}

		write_register(this, ABOV_CTRL_MODE_RET, 0x00);
		write_register(this, ABOV_RECALI_REG, 0x01);
		msleep(500);

		read_register(this, ABOV_IRQSTAT_REG, &sar_value);
		LOG_DBG("enable capsensor sar_value=%d, \n", sar_value);

		input_report_abs(input_bottom, ABS_DISTANCE, 0);
		input_sync(input_bottom);
		mEnabled = 1;
	} else if (enable == 0) {
		LOG_DBG("disable capsensor\n");

		if(this->irq_disabled == 1){
			disable_irq(this->irq);
			this->irq_disabled = 0;
			LOG_DBG("capsensor disable_irq\n");
		}

		write_register(this, ABOV_CTRL_MODE_RET, 0x02);

		input_report_abs(input_bottom, ABS_DISTANCE, -1);
		input_sync(input_bottom);
		mEnabled = 0;
	} else {
		LOG_DBG("unknown enable symbol\n");
	}

	return 0;
}
#endif

static CLASS_ATTR(enable, 0660, capsense_enable_show, capsense_enable_store);

static ssize_t reg_dump_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u8 reg_value = 0, i;
	pabovXX_t this = abov_sar_ptr;
	char *p = buf;

	if (this->read_flag) {
		this->read_flag = 0;
		read_register(this, this->read_reg, &reg_value);
		p += snprintf(p, PAGE_SIZE, "%02x\n", reg_value);
		return (p-buf);
	}

	for (i = 0; i < 0x26; i++) {
		read_register(this, i, &reg_value);
		p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n",
				i, reg_value);
	}

	for (i = 0x80; i < 0x8C; i++) {
		read_register(this, i, &reg_value);
		p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n",
				i, reg_value);
	}

	return (p-buf);
}

static ssize_t reg_dump_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	unsigned int val, reg, opt;

	if (sscanf(buf, "%x,%x,%x", &reg, &val, &opt) == 3) {
		LOG_INFO("%s, read reg = 0x%02x\n", __func__, *(u8 *)&reg);
		this->read_reg = *((u8 *)&reg);
		this->read_flag = 1;
	} else if (sscanf(buf, "%x,%x", &reg, &val) == 2) {
		LOG_INFO("%s,reg = 0x%02x, val = 0x%02x\n",
				__func__, *(u8 *)&reg, *(u8 *)&val);
		write_register(this, *((u8 *)&reg), *((u8 *)&val));
	}

	return count;
}

static CLASS_ATTR(reg, 0660, reg_dump_show, reg_dump_store);

//+bug 483940 ,lishuai_wt,20190911,add ,sar sensor threshold
static ssize_t reg_dump_show_backcap(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u8 reg_value = 0;
	pabovXX_t this = abov_sar_ptr;

	read_register(this, 0x24, &reg_value);
	backcapTemp=reg_value<<8;
	read_register(this, 0x25, &reg_value);
	backcapTemp+=reg_value;

	return snprintf(buf, 8, "%d\n", backcapTemp);
}

static ssize_t reg_dump_store_backcap(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{

	return count;
}

static CLASS_ATTR(reg_backcap, 0660, reg_dump_show_backcap, reg_dump_store_backcap);

static ssize_t reg_dump_show_checkcap(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u8 reg_value = 0;
	pabovXX_t this = abov_sar_ptr;

	read_register(this, 0x20, &reg_value);
	checkcapTemp=reg_value<<8;
	read_register(this, 0x21, &reg_value);
	checkcapTemp+=reg_value;

	return snprintf(buf, 8, "%d\n", checkcapTemp);
}

static ssize_t reg_dump_store_checkcap(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{

	return count;
}

static CLASS_ATTR(reg_checkcap, 0660, reg_dump_show_checkcap, reg_dump_store_checkcap);
//-bug 483940 ,lishuai_wt,20190911,add ,sar sensor threshold

static struct class capsense_class = {
	.name			= "capsense",
	.owner			= THIS_MODULE,
};

#ifdef SAR_USB_CHANGE
static void ps_notify_callback_work(struct work_struct *work)
{
	pabovXX_t this = container_of(work, abovXX_t, ps_notify_work);
	pabov_t pDevice = NULL;
	struct input_dev *input_bottom = NULL;
	int ret = 0;

	pDevice = this->pDevice;
	input_bottom = pDevice->pbuttonInformation->input_bottom;

	LOG_INFO("Usb insert,going to force calibrate\n");
	ret = write_register(this, ABOV_RECALI_REG, 0x01);
	if (ret < 0)
		LOG_DBG(" Usb insert,calibrate cap sensor failed\n");

	input_report_abs(input_bottom, ABS_DISTANCE, 0);
	input_sync(input_bottom);
}

static int ps_get_state(struct power_supply *psy, bool *present)
{
	union power_supply_propval pval = { 0 };
	int retval;

	retval = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
			&pval);
	if (retval) {
		LOG_DBG("%s psy get property failed\n", psy->name);
		return retval;
	}
	*present = (pval.intval) ? true : false;
	LOG_INFO("%s is %s\n", psy->name,
			(*present) ? "present" : "not present");
	return 0;
}

static int ps_notify_callback(struct notifier_block *self,
		unsigned long event, void *p)
{
	pabovXX_t this = container_of(self, abovXX_t, ps_notif);
	struct power_supply *psy = p;
	bool present;
	int retval;

	if ((event == PSY_EVENT_PROP_ADDED || event == PSY_EVENT_PROP_CHANGED)
			&& psy && psy->get_property && psy->name &&
			!strncmp(psy->name, "usb", sizeof("usb"))) {
		LOG_INFO("ps notification: event = %lu\n", event);
		retval = ps_get_state(psy, &present);
		if (retval) {
			LOG_DBG("psy get property failed\n");
			return retval;
		}

		if (event == PSY_EVENT_PROP_CHANGED) {
			if (this->ps_is_present == present) {
				LOG_INFO("ps present state not change\n");
				return 0;
			}
		}
		this->ps_is_present = present;
		schedule_work(&this->ps_notify_work);
	}

	return 0;
}
#endif

static int _i2c_adapter_block_write(struct i2c_client *client, u8 *data, u8 len)
{
	u8 buffer[C_I2C_FIFO_SIZE];
	u8 left = len;
	u8 offset = 0;
	u8 retry = 0;

	struct i2c_msg msg = {
		.addr = client->addr & I2C_MASK_FLAG,
		.flags = 0,
		.buf = buffer,
	};

	if (data == NULL || len < 1) {
		LOG_ERR("Invalid : data is null or len=%d\n", len);
		return -EINVAL;
	}

	while (left > 0) {
		retry = 0;
		if (left >= C_I2C_FIFO_SIZE) {
			msg.buf = &data[offset];
			msg.len = C_I2C_FIFO_SIZE;
			left -= C_I2C_FIFO_SIZE;
			offset += C_I2C_FIFO_SIZE;
		} else {
			msg.buf = &data[offset];
			msg.len = left;
			left = 0;
		}

		while (i2c_transfer(client->adapter, &msg, 1) != 1) {
			retry++;
			if (retry > 10) {
				LOG_ERR("OUT : fail - addr:%#x len:%d \n", client->addr, msg.len);
				return -EIO;
			}
		}
	}
	return 0;
}

static int i2c_adapter_block_write_nodatalog(struct i2c_client *client, u8 *data, u8 len)
{
	return _i2c_adapter_block_write(client, data, len);
}

static int abov_tk_check_busy(struct i2c_client *client)
{
	int ret, count = 0;
	unsigned char val = 0x00;

	do {
		ret = i2c_master_recv(client, &val, sizeof(val));
		if (val & 0x01) {
			count++;
			if (count > 1000) {
				LOG_INFO("%s: val = 0x%x\r\n", __func__, val);
				return ret;
			}
		} else {
			break;
		}
	} while (1);

	return ret;
}

static int abov_tk_fw_write(struct i2c_client *client, unsigned char *addrH,
						unsigned char *addrL, unsigned char *val)
{
	int length = 36, ret = 0;
	unsigned char buf[40] = {0, };

	buf[0] = 0xAC;
	buf[1] = 0x7A;
	memcpy(&buf[2], addrH, 1);
	memcpy(&buf[3], addrL, 1);
	memcpy(&buf[4], val, 32);
	ret = i2c_adapter_block_write_nodatalog(client, buf, length);
	if (ret < 0) {
		LOG_ERR("Firmware write fail ...\n");
		return ret;
	}

	SLEEP(3);
	abov_tk_check_busy(client);

	return 0;
}

static int abov_tk_reset_for_bootmode(struct i2c_client *client)
{
	int ret, retry_count = 10;
	unsigned char buf[16] = {0, };

retry:
	buf[0] = 0xF0;
	buf[1] = 0xAA;
	ret = i2c_master_send(client, buf, 2);
	if (ret < 0) {
		LOG_INFO("write fail(retry:%d)\n", retry_count);
		if (retry_count-- > 0) {
			goto retry;
		}
		return -EIO;
	} else {
		LOG_INFO("success reset & boot mode\n");
		return 0;
	}
}

static int abov_tk_fw_mode_enter(struct i2c_client *client)
{
	int ret = 0;
	unsigned char buf[40] = {0, };

	buf[0] = 0xAC;
	buf[1] = 0x5B;
	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		LOG_ERR("SEND : fail - addr:%#x data:%#x %#x... ret:%d\n", client->addr, buf[0], buf[1], ret);
		return -EIO;
	}
	LOG_INFO("SEND : succ - addr:%#x data:%#x %#x... ret:%d\n", client->addr, buf[0], buf[1], ret);
	SLEEP(5);

       ret = i2c_master_recv(client, buf, 1);
	if (ret < 0) {
		LOG_ERR("Enter fw mode fail ... device id:%#x\n",buf[0]);
		return -EIO;
	}

	LOG_INFO("succ ... device id:%#x\n", buf[0]);

	return 0;
}

static int abov_tk_fw_mode_exit(struct i2c_client *client)
{
	int ret = 0;
	unsigned char buf[40] = {0, };

	buf[0] = 0xAC;
	buf[1] = 0xE1;
	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		LOG_ERR("SEND : fail - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
		return -EIO;
	}
	LOG_INFO("SEND : succ - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);

	return 0;
}

static int abov_tk_flash_erase(struct i2c_client *client)
{
	int ret = 0;
	unsigned char buf[16] = {0, };

	buf[0] = 0xAC;
#ifdef ABOV_POWER_CONFIG
	buf[1] = 0x2D;
#else
	buf[1] = 0x2E;
#endif

	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		LOG_ERR("SEND : fail - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
		return -EIO;
	}

	LOG_INFO("SEND : succ - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);

	return 0;
}

static int abov_tk_i2c_read_checksum(struct i2c_client *client)
{
	unsigned char checksum[6] = {0, };
	unsigned char buf[16] = {0, };
	int ret;

	checksum_h = 0;
	checksum_l = 0;

#ifdef ABOV_POWER_CONFIG
	buf[0] = 0xAC;
	buf[1] = 0x9E;
	buf[2] = 0x10;
	buf[3] = 0x00;
	buf[4] = 0x3F;
	buf[5] = 0xFF;
	ret = i2c_master_send(client, buf, 6);
#else
	buf[0] = 0xAC;
	buf[1] = 0x9E;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = checksum_h_bin;
	buf[5] = checksum_l_bin;
	ret = i2c_master_send(client, buf, 6);
#endif
	if (ret != 6) {
		LOG_ERR("SEND : fail - addr:%#x len:%d ... ret:%d\n", client->addr, 6, ret);
		return -EIO;
	}
	SLEEP(5);

	buf[0] = 0x00;
	ret = i2c_master_send(client, buf, 1);
	if (ret != 1) {
		LOG_ERR("SEND : fail - addr:%#x data:%#x ... ret:%d\n", client->addr, buf[0], ret);
		return -EIO;
	}
	SLEEP(5);

	ret = i2c_master_recv(client, checksum, 6);
	if (ret < 0) {
		LOG_ERR("Read checksum fail ... \n");
		return -EIO;
	}

	checksum_h = checksum[4];
	checksum_l = checksum[5];

	return 0;
}

static int _abov_fw_update(struct i2c_client *client, const u8 *image, u32 size)
{
	int ret, ii = 0;
	int count;
	unsigned short address;
	unsigned char addrH, addrL;
	unsigned char data[32] = {0, };

	LOG_INFO("%s: call in\r\n", __func__);

	if (abov_tk_reset_for_bootmode(client) < 0) {
		LOG_ERR("don't reset(enter boot mode)!");
		return -EIO;
	}

	SLEEP(45);

	for (ii = 0; ii < 10; ii++) {
		if (abov_tk_fw_mode_enter(client) < 0) {
			LOG_ERR("don't enter the download mode! %d", ii);
			SLEEP(40);
			continue;
		}
		break;
	}

	if (10 <= ii) {
		return -EAGAIN;
	}

	if (abov_tk_flash_erase(client) < 0) {
		LOG_ERR("don't erase flash data!");
		return -EIO;
	}

	SLEEP(1400);

	address = 0x800;
	count = size / 32;

	for (ii = 0; ii < count; ii++) {
		/* first 32byte is header */
		addrH = (unsigned char)((address >> 8) & 0xFF);
		addrL = (unsigned char)(address & 0xFF);
		memcpy(data, &image[ii * 32], 32);
		ret = abov_tk_fw_write(client, &addrH, &addrL, data);
		if (ret < 0) {
			LOG_INFO("fw_write.. ii = 0x%x err\r\n", ii);
			return ret;
		}

		address += 0x20;
		memset(data, 0, 32);
	}

	ret = abov_tk_i2c_read_checksum(client);
	ret = abov_tk_fw_mode_exit(client);
	if ((checksum_h == checksum_h_bin) && (checksum_l == checksum_l_bin)) {
		LOG_INFO("checksum successful. checksum_h:%x=%x && checksum_l:%x=%x\n",
			checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
	} else {
		LOG_INFO("checksum error. checksum_h:%x=%x && checksum_l:%x=%x\n",
			checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
		ret = -1;
	}
	SLEEP(100);

	return ret;
}

static int abov_fw_update(bool force)
{
	int update_loop;
	pabovXX_t this = abov_sar_ptr;
	struct i2c_client *client = this->bus;
	int rc;
	bool fw_upgrade = false;
	u8 fw_version = 0, fw_file_version = 0;
	u8 fw_modelno = 0, fw_file_modeno = 0;
	const struct firmware *fw = NULL;
	char fw_name[32] = {0};

	strlcpy(fw_name, this->board->fw_name, NAME_MAX);
	strlcat(fw_name, ".BIN", NAME_MAX);
	rc = request_firmware(&fw, fw_name, this->pdev);
	if (rc < 0) {
		read_register(this, ABOV_VERSION_REG, &fw_version);
		read_register(this, ABOV_MODELNO_REG, &fw_modelno);

		LOG_INFO("Request firmware failed - %s (%d)\n", this->board->fw_name, rc);
		return rc;
	}

	if (force == false) {
		read_register(this, ABOV_VERSION_REG, &fw_version);
		read_register(this, ABOV_MODELNO_REG, &fw_modelno);
		LOG_INFO("In phone fw_version=%d,fw_modelno=0x%x, \n", fw_version, fw_modelno);
	}

	fw_file_modeno = fw->data[1];
	fw_file_version = fw->data[5];
	checksum_h_bin = fw->data[8];
	checksum_l_bin = fw->data[9];

	LOG_INFO("file fw_file_version=%d,fw_file_modeno=0x%x, \n", fw_file_version, fw_file_modeno);

	if ((force) || (fw_version < fw_file_version) || (fw_modelno != fw_file_modeno))
		fw_upgrade = true;
	else {
		LOG_INFO("Exiting fw upgrade...\n");
		fw_upgrade = false;
		rc = -EIO;
		goto rel_fw;
	}

	if (fw_upgrade) {
		for (update_loop = 0; update_loop < 5; update_loop++) {
			rc = _abov_fw_update(client, &fw->data[32], fw->size-32);
			if (rc < 0)
				LOG_INFO("retry : %d times!\n", update_loop);
			else {
				initialize(this);
				break;
			}
		}
		if (update_loop >= 5)
			rc = -EIO;
	}

rel_fw:
	release_firmware(fw);
	return rc;
}

static ssize_t capsense_fw_ver_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u8 fw_version = 0;
	pabovXX_t this = abov_sar_ptr;

	read_register(this, ABOV_VERSION_REG, &fw_version);
	return snprintf(buf, 16, "0x%x\n", fw_version);
}

static ssize_t capsense_update_fw_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	unsigned long val;
	bool irq_status_keeper = 0;
	int rc;

	if (count > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

	if(this->irq_disabled == 1){
		irq_status_keeper = 1;
		this->irq_disabled = 0;
		disable_irq(this->irq);
	}

	mutex_lock(&this->mutex);
	if (!this->loading_fw  && val) {
		this->loading_fw = true;
		abov_fw_update(false);
		this->loading_fw = false;
	}
	mutex_unlock(&this->mutex);

	if(irq_status_keeper == 1){
		enable_irq(this->irq);
		this->irq_disabled = 1;
	}

	return count;
}
static CLASS_ATTR(update_fw, 0660, capsense_fw_ver_show, capsense_update_fw_store);

static ssize_t capsense_force_update_fw_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	unsigned long val;
	bool irq_status_keeper = 0;
	int rc;

	if (count > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

	if(this->irq_disabled == 1){
		irq_status_keeper = 1;
		this->irq_disabled = 0;
		disable_irq(this->irq);
	}

	mutex_lock(&this->mutex);
	if (!this->loading_fw  && val) {
		this->loading_fw = true;
		abov_fw_update(true);
		this->loading_fw = false;
	}
	mutex_unlock(&this->mutex);

	if(irq_status_keeper == 1){
		enable_irq(this->irq);
		this->irq_disabled = 1;
	}

	return count;
}
static CLASS_ATTR(force_update_fw, 0660, capsense_fw_ver_show, capsense_force_update_fw_store);

static void capsense_update_work(struct work_struct *work)
{
	pabovXX_t this = container_of(work, abovXX_t, fw_update_work);
	LOG_INFO("%s: start update firmware\n", __func__);
	disable_irq(this->irq);

	mutex_lock(&this->mutex);
	this->loading_fw = true;
	abov_fw_update(false);
	this->loading_fw = false;
	mutex_unlock(&this->mutex);
	enable_irq(this->irq);

	LOG_INFO("%s: update firmware end\n", __func__);
}

static void capsense_fore_update_work(struct work_struct *work)
{
	pabovXX_t this = container_of(work, abovXX_t, fw_update_work);
	LOG_INFO("%s: start force update firmware\n", __func__);

	disable_irq(this->irq);
	mutex_lock(&this->mutex);
	this->loading_fw = true;
	abov_fw_update(true);
	this->loading_fw = false;
	mutex_unlock(&this->mutex);
	enable_irq(this->irq);

	LOG_INFO("%s: force update firmware end\n", __func__);
}
/**
 * fn static int abov_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * brief Probe function
 * param client pointer to i2c_client
 * param id pointer to i2c_device_id
 * return Whether probe was successful
 */
static int abov_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	pabovXX_t this = 0;
	pabov_t pDevice = 0;
	pabov_platform_data_t pplatData = 0;
	bool isForceUpdate = false;
	int ret=0;
	int err =0;

	struct input_dev *input_bottom = NULL;

	#ifdef SAR_USB_CHANGE
	struct power_supply *psy = NULL;
	#endif

	printk("abov_probe start()\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)){
		LOG_DBG("i2c_check_functionality failed \n");
		return -EIO;
	}

	pplatData = kzalloc(sizeof(pplatData), GFP_KERNEL);
	if (!pplatData) {
		LOG_DBG("platform data is required!\n");
		return -EINVAL;
	}
	abov_platform_data_of_init(client, pplatData);
	client->dev.platform_data = pplatData;

	pplatData->cap_vdd = regulator_get(&client->dev, "cap_vdd");
	if (IS_ERR(pplatData->cap_vdd)) {
		if (PTR_ERR(pplatData->cap_vdd) == -EPROBE_DEFER) {
			ret = PTR_ERR(pplatData->cap_vdd);
			goto err_vdd_defer;
		}
		LOG_INFO("%s: Failed to get regulator\n",
				__func__);
	} else {
		int error = regulator_enable(pplatData->cap_vdd);
		if (error) {
			regulator_put(pplatData->cap_vdd);
			LOG_DBG("%s: Error %d enable regulator\n",
					__func__, error);
			ret =error;
			goto err_vdd_defer;
		}
		pplatData->cap_vdd_en = true;
		LOG_INFO("cap_vdd regulator is %s\n",
				regulator_is_enabled(pplatData->cap_vdd) ?
				"on" : "off");
	}

	pplatData->cap_svdd = regulator_get(&client->dev, "cap_svdd");
	if (!IS_ERR(pplatData->cap_svdd)) {
		ret = regulator_enable(pplatData->cap_svdd);
		if (ret) {
			regulator_put(pplatData->cap_svdd);
			LOG_INFO("Failed to enable cap_svdd\n");
			goto err_svdd_error;
		}
		pplatData->cap_svdd_en = true;
		LOG_INFO("cap_svdd regulator is %s\n",
				regulator_is_enabled(pplatData->cap_svdd) ?
				"on" : "off");
	} else {
		ret = PTR_ERR(pplatData->cap_vdd);
		if (ret == -EPROBE_DEFER)
			goto err_svdd_error;
	}
	SLEEP(100);

       /* detect if abov exist or not */
	ret = abov_detect(client);
	if (ret == 0) {
		return -ENODEV;
	}
	if (ret == 2) {
		isForceUpdate = true;
	}
	/*-Bug 440975 lishuai_wt,20190501,add,open sar sensor for sda450*/

	this = kzalloc(sizeof(struct abovXX), GFP_KERNEL); /* create memory for main struct */
	if (!this) {
		LOG_DBG("failed to alloc for main struct\n");
		ret = -EINVAL;
		goto err_platform_init;
	}

	LOG_INFO("\t Initialized Main Memory: 0x%p\n", this);

	//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
	this->sar_pinctl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(this->sar_pinctl)) {
		ret = PTR_ERR(this->sar_pinctl);
		printk("abov Target does not use pinctrl %d\n", ret);
	}

	this->pinctrl_state_default
		= pinctrl_lookup_state(this->sar_pinctl, "abov_int_default");
	if (IS_ERR_OR_NULL(this->pinctrl_state_default)) {
		ret = PTR_ERR(this->pinctrl_state_default);
		printk("abov Can not lookup %s pinstate %d\n","abov_int_default", ret);
	}
	wakeup_source_init(&wakesrc, "abov");
	//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

	if (this) {
		/* In case we need to reinitialize data
		 * (e.q. if suspend reset device) */
		this->init = initialize;
		/* pointer to function from platform data to get pendown
		 * (1->NIRQ=0, 0->NIRQ=1) */
		this->get_nirq_low = pplatData->get_is_nirq_low;
		/* save irq in case we need to reference it */
		this->irq = gpio_to_irq(client->irq);
		/* do we need to create an irq timer after interrupt ? */
		this->useIrqTimer = 0;
		this->board = pplatData;
		/* Setup function to call on corresponding reg irq source bit */
		if (MAX_NUM_STATUS_BITS >= 2) {
			this->statusFunc[0] = 0; /* TXEN_STAT */
			this->statusFunc[1] = touchProcess;
		}

		/* setup i2c communication */
		this->bus = client;
		i2c_set_clientdata(client, this);

		/* record device struct */
		this->pdev = &client->dev;

		/* create memory for device specific struct */
		this->pDevice = pDevice = kzalloc(sizeof(abov_t), GFP_KERNEL);
		LOG_INFO("\t Initialized Device Specific Memory: 0x%p\n",
				pDevice);
		abov_sar_ptr = this;
		if (pDevice) {
			/* for accessing items in user data (e.g. calibrate) */
			ret = sysfs_create_group(&client->dev.kobj, &abov_attr_group);


			/* Check if we hava a platform initialization function to call*/
			if (pplatData->init_platform_hw)
				pplatData->init_platform_hw();

			/* Add Pointer to main platform data struct */
			pDevice->hw = pplatData;

			/* Initialize the button information initialized with keycodes */
			pDevice->pbuttonInformation = pplatData->pbuttonInformation;

			/* Create the input device */
			input_bottom = input_allocate_device();
			if (!input_bottom){
				LOG_DBG("input_allocate_device failed \n");
				return -ENOMEM;
			}
			/* Set all the keycodes */
			__set_bit(EV_ABS, input_bottom->evbit);
			input_set_abs_params(input_bottom, ABS_DISTANCE, -1, 100, 0, 0);
			/* save the input pointer and finish initialization */
			pDevice->pbuttonInformation->input_bottom = input_bottom;
			/* save the input pointer and finish initialization */
			input_bottom->name = "ABOV Cap Touch bottom";
			if (input_register_device(input_bottom)) {
				LOG_INFO("add bottom cap touch unsuccess\n");
				return -ENOMEM;
			}
		}
		ret = class_register(&capsense_class);
		if (ret < 0) {
			LOG_DBG("class_register failed (%d)\n", ret);
			return ret;
		}

		ret = class_create_file(&capsense_class, &class_attr_mcalibrate);
		if (ret < 0) {
			LOG_DBG("Create mcalibrate file failed (%d)\n", ret);
			return ret;
		}

		ret = class_create_file(&capsense_class, &class_attr_enable);
		if (ret < 0) {
			LOG_DBG("Create enable file failed (%d)\n", ret);
			return ret;
		}

		ret = class_create_file(&capsense_class, &class_attr_reg);
		if (ret < 0) {
			LOG_DBG("Create reg file failed (%d)\n", ret);
			return ret;
		}
//+bug 483940 ,lishuai_wt,20190911,add ,sar sensor threshold
		ret = class_create_file(&capsense_class, &class_attr_reg_backcap);
		if (ret < 0) {
			LOG_DBG("Create reg file failed (%d)\n", ret);
			return ret;
		}
		ret = class_create_file(&capsense_class, &class_attr_reg_checkcap);
		if (ret < 0) {
			LOG_DBG("Create reg file failed (%d)\n", ret);
			return ret;
		}
//-bug 483940 ,lishuai_wt,20190911,add ,sar sensor threshold
		ret = class_create_file(&capsense_class, &class_attr_update_fw);
		if (ret < 0) {
			LOG_DBG("Create update_fw file failed (%d)\n", ret);
			return ret;
		}

		ret = class_create_file(&capsense_class, &class_attr_force_update_fw);
		if (ret < 0) {
			LOG_DBG("Create update_fw file failed (%d)\n", ret);
			return ret;
		}
#ifdef USE_SENSORS_CLASS
		sensors_capsensor_bottom_cdev.sensors_enable = capsensor_set_enable;
		sensors_capsensor_bottom_cdev.sensors_poll_delay = NULL;
		ret = sensors_classdev_register(&input_bottom->dev, &sensors_capsensor_bottom_cdev);
		if (ret < 0)
			LOG_DBG("create bottom cap sensor_class file failed (%d)\n", ret);
#endif

		//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
		if (this->sar_pinctl ) {
			err = pinctrl_select_state(this->sar_pinctl,
				this->pinctrl_state_default);
			if (err)
				pr_err("%s:%d cannot set pin to default state",
					__func__, __LINE__);
		}
		//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

		abovXX_sar_init(this);
             write_register(this, ABOV_CTRL_MODE_RET, 0x02);
		mEnabled = 0;
		#ifdef SAR_USB_CHANGE
		INIT_WORK(&this->ps_notify_work, ps_notify_callback_work);
		this->ps_notif.notifier_call = ps_notify_callback;
		ret = power_supply_reg_notifier(&this->ps_notif);
		if (ret) {
			LOG_DBG(
				"Unable to register ps_notifier: %d\n", ret);
			goto free_ps_notifier;
		}

		psy = power_supply_get_by_name("usb");
		if (psy) {
			ret = ps_get_state(psy, &this->ps_is_present);
			if (ret) {
				LOG_DBG(
					"psy get property failed rc=%d\n",
					ret);
				goto free_ps_notifier;
			}
		}
		#endif

		this->loading_fw = false;
		if (isForceUpdate == true) {
		    INIT_WORK(&this->fw_update_work, capsense_fore_update_work);
		} else {
		    INIT_WORK(&this->fw_update_work, capsense_update_work);
		}
		schedule_work(&this->fw_update_work);

                printk("abov_probe end()\n");
		return  0;
	}
	return -ENOMEM;

#ifdef SAR_USB_CHANGE
free_ps_notifier:
	power_supply_unreg_notifier(&this->ps_notif);

	LOG_INFO("%s input free device.\n", __func__);
	input_free_device(input_bottom);
#endif

err_platform_init:
err_svdd_error:
	LOG_INFO("%s svdd defer.\n", __func__);
	regulator_disable(pplatData->cap_vdd);
	regulator_put(pplatData->cap_vdd);

err_vdd_defer:
	kfree(pplatData);

	return ret;
}

/**
 * fn static int abov_remove(struct i2c_client *client)
 * brief Called when device is to be removed
 * param client Pointer to i2c_client struct
 * return Value from abovXX_sar_remove()
 */
static int abov_remove(struct i2c_client *client)
{
	pabov_platform_data_t pplatData = 0;
	pabov_t pDevice = 0;
	pabovXX_t this = i2c_get_clientdata(client);

	pDevice = this->pDevice;
	if (this && pDevice) {
#ifdef USE_SENSORS_CLASS
		sensors_classdev_unregister(&sensors_capsensor_bottom_cdev);
#endif
		input_unregister_device(pDevice->pbuttonInformation->input_bottom);

		if (this->board->cap_svdd_en) {
			regulator_disable(this->board->cap_svdd);
			regulator_put(this->board->cap_svdd);
		}

		if (this->board->cap_vdd_en) {
			regulator_disable(this->board->cap_vdd);
			regulator_put(this->board->cap_vdd);
		}
#ifdef USE_SENSORS_CLASS
		sensors_classdev_unregister(&sensors_capsensor_bottom_cdev);
#endif
		sysfs_remove_group(&client->dev.kobj, &abov_attr_group);
		pplatData = client->dev.platform_data;
		if (pplatData && pplatData->exit_platform_hw)
			pplatData->exit_platform_hw();
		kfree(this->pDevice);
	}
	return abovXX_sar_remove(this);
}

//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
/***** Kernel Suspend *****/
static int abov_suspend(struct device *dev)
{
  struct i2c_client *client = to_i2c_client(dev);
  pabovXX_t this = i2c_get_clientdata(client);

  LOG_INFO("%s start.\n", __func__);

  if(this->irq_disabled == 1){
      enable_irq_wake(this->irq);
      write_register(this, ABOV_CTRL_MODE_RET, 0x01);
  } else {
        write_register(this, ABOV_CTRL_MODE_RET, 0x02);
  }

  LOG_INFO("%s end.\n", __func__);

  return 0;
}

/***** Kernel Resume *****/
static int abov_resume(struct device *dev)
{
   struct i2c_client *client = to_i2c_client(dev);
   pabovXX_t this = i2c_get_clientdata(client);

   LOG_INFO("%s start.\n", __func__);

   if(this->irq_disabled == 1){
        disable_irq_wake(this->irq);
        write_register(this, ABOV_CTRL_MODE_RET, 0x00);
   }

   LOG_INFO("%s end.\n", __func__);

   return 0;
}
//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

#ifdef CONFIG_OF
static const struct of_device_id synaptics_rmi4_match_tbl[] = {
	{ .compatible = "abov," DRIVER_NAME },
	{ },
};
MODULE_DEVICE_TABLE(of, synaptics_rmi4_match_tbl);
#endif

static struct i2c_device_id abov_idtable[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, abov_idtable);

//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
static SIMPLE_DEV_PM_OPS(abov_pm_ops, abov_suspend, abov_resume);
//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

static struct i2c_driver abov_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = DRIVER_NAME,
		.pm = &abov_pm_ops,
	},
	.id_table = abov_idtable,
	.probe	  = abov_probe,
	.remove	  = abov_remove,
};
static int __init abov_init(void)
{
	return i2c_add_driver(&abov_driver);
}
static void __exit abov_exit(void)
{
	i2c_del_driver(&abov_driver);
}

module_init(abov_init);
module_exit(abov_exit);

MODULE_AUTHOR("ABOV Corp.");
MODULE_DESCRIPTION("ABOV Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

#ifdef USE_THREADED_IRQ
static void abovXX_process_interrupt(pabovXX_t this, u8 nirqlow)
{
	if (!this) {
		pr_err("abovXX_worker_func, NULL abovXX_t\n");
		return;
	}

	this->statusFunc[1](this);
	if (unlikely(this->useIrqTimer && nirqlow)) {
		/* In case we need to send a timer for example on a touchscreen
		 * checking penup, perform this here
		 */
		cancel_delayed_work(&this->dworker);
		schedule_delayed_work(&this->dworker, msecs_to_jiffies(this->irqTimeout));
		LOG_INFO("Schedule Irq timer");
	}
}


static void abovXX_worker_func(struct work_struct *work)
{
	pabovXX_t this = 0;

	if (work) {
		this = container_of(work, abovXX_t, dworker.work);
		if (!this) {
			LOG_DBG("abovXX_worker_func, NULL abovXX_t\n");
			return;
		}
		if ((!this->get_nirq_low) || (!this->get_nirq_low(this->board->irq_gpio))) {
			/* only run if nirq is high */
			abovXX_process_interrupt(this, 0);
		}
	} else {
		LOG_INFO("abovXX_worker_func, NULL work_struct\n");
	}
}
static irqreturn_t abovXX_interrupt_thread(int irq, void *data)
{
	pabovXX_t this = 0;
	this = data;

	LOG_INFO("abovXX_irq begin\n");

	mutex_lock(&this->mutex);
	LOG_INFO("abovXX_irq\n");

	if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio))
		abovXX_process_interrupt(this, 1);
	else
		LOG_DBG("abovXX_irq - nirq read high\n");
	mutex_unlock(&this->mutex);
	//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
	__pm_wakeup_event(&wakesrc, TEMPORARY_HOLD_TIME);
	//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
	return IRQ_HANDLED;
}
#endif


int abovXX_sar_init(pabovXX_t this)
{
	int err = 0;

	if (this && this->pDevice) {
#ifdef USE_THREADED_IRQ
	/* initialize worker function */
	INIT_DELAYED_WORK(&this->dworker, abovXX_worker_func);

	/* initialize mutex */
	mutex_init(&this->mutex);
	/* initailize interrupt reporting */

       this->board->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
       err = request_threaded_irq(this->irq, NULL, abovXX_interrupt_thread,
              this->board->irq_gpio_flags, this->pdev->driver->name,
              this);
	 if (err) {
		LOG_DBG("irq %d busy?\n", this->irq);
		return err;
	}
	disable_irq(this->irq);
#endif

      this->irq_disabled = 0;
#ifdef USE_THREADED_IRQ
	LOG_DBG("registered with threaded irq (%d)\n", this->irq);
#endif
	/* call init function pointer (this should initialize all registers */
	if (this->init)
		return this->init(this);
	LOG_DBG("No init function!!!!\n");
	}
	return -ENOMEM;
}

int abovXX_sar_remove(pabovXX_t this)
{
	if (this) {
		cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
		free_irq(this->irq, this);
		kfree(this);
		return 0;
	}
	return -ENOMEM;
}
