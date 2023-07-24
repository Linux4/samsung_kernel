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
#define DRIVER_NAME "abov_sar_mmi_overlay"
#define USE_SENSORS_CLASS
//#define USE_USB_CHANGE_RECAL
#define WT_ADD_SAR_HARDWARE_INFO 1
#define POWER_ENABLE    1

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/device.h>
#ifdef USE_SENSORS_CLASS
#include <linux/sensors.h>
#endif
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#ifdef USE_USB_CHANGE_RECAL
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/power_supply.h>
#endif
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/async.h>
#include <linux/firmware.h>
#include <linux/input/abov_sar_mmi_overlay.h> /* main struct, interrupt,init,pointers */
#if WT_ADD_SAR_HARDWARE_INFO
#include <linux/hardware_info.h>
#endif

#include <linux/init.h>

//#ifdef CONFIG_PM_WAKELOCKS
#if 0
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#define SLEEP(x)	mdelay(x)
#define C_I2C_FIFO_SIZE 8
#define I2C_MASK_FLAG	(0x00ff)

static u8 checksum_h;
static u8 checksum_h_bin;
static u8 checksum_l;
static u8 checksum_l_bin;

#define IDLE     0
#define ACTIVE   1
#define S_PROX   1
#define S_BODY   2

#define LOG_TAG "<ABOV_LOG>"
#define LOG_INFO(fmt, args...)  pr_info(LOG_TAG "[INFO]" "<%s><%d>"fmt, __func__, __LINE__, ##args)
#define LOG_DBG(fmt, args...)	pr_debug(LOG_TAG "[DBG]" "<%s><%d>"fmt, __func__, __LINE__, ##args)
#define LOG_ERR(fmt, args...)   pr_err(LOG_TAG "[ERR]" "<%s><%d>"fmt, __func__, __LINE__, ##args)

static int mEnabled;
static int programming_done;
pabovXX_t abov_sar_ptr;

#define MAX_INT_COUNT	20
#define SALE_CODE_STR_LENGTH    3
static char sales_code_from_cmdline[SALE_CODE_STR_LENGTH + 1];
static bool is_anfr_sar = false;

#define TEMPORARY_HOLD_TIME	500
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
struct wakeup_source wakesrc;
#else
struct wake_lock wakesrc;
#endif

static void touchProcess(pabovXX_t this);
/**
 * struct abov
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct abov {
	pbuttonInformation_t pbuttonInformation;
	pabov_platform_data_t hw; /* specific platform data settings */
} abov_t, *pabov_t;

static void ForcetoTouched(pabovXX_t this)
{
	pabov_t pDevice = NULL;
	struct input_dev *input_ch0 = NULL;
	struct input_dev *input_ch1 = NULL;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	struct input_dev *input_ch2 = NULL;
#endif
	struct _buttonInfo *pCurrentButton  = NULL;

	pDevice = this->pDevice;
	if (this && pDevice) {
		LOG_DBG("ForcetoTouched()\n");

		pCurrentButton = pDevice->pbuttonInformation->buttons;
		input_ch0 = pDevice->pbuttonInformation->input_ch0;
		input_ch1 = pDevice->pbuttonInformation->input_ch1;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
		input_ch2 = pDevice->pbuttonInformation->input_ch2;
#endif

		pCurrentButton->state = ACTIVE;
		if (mEnabled) {
			input_report_rel(input_ch0, REL_MISC, 1);
			input_report_rel(input_ch0, REL_X, 2);
			input_sync(input_ch0);
			input_report_rel(input_ch1, REL_MISC, 1);
			input_report_rel(input_ch1, REL_X, 2);
			input_sync(input_ch1);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
			input_report_rel(input_ch2, REL_MISC, 1);
			input_report_rel(input_ch2, REL_X, 2);
			input_sync(input_ch2);
#endif
		}
		LOG_DBG("Leaving ForcetoTouched()\n");
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
		LOG_DBG("write_register Addr: 0x%02x Val: 0x%02x Return: %d\n",
				address, value, returnValue);
	}
	if (returnValue < 0) {
		ForcetoTouched(this);
		LOG_ERR("Write_register-ForcetoTouched()\n");
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
		LOG_DBG("read_register Addr: 0x%02x Return: 0x%02x\n",
				address, returnValue);
	}

	if (returnValue >= 0) {
		*value = returnValue;
		return 0;
	} else {
			ForcetoTouched(this);
			LOG_ERR("read_register-ForcetoTouched()\n");
			return returnValue;
	}
	return returnValue;
}

//+bug 492320,20191119,gaojingxuan.wt,add,use SS hal
/*********************************************
*FUNCTION
*  enable_bottom
*DESCRIPTION
*  enable func
*PARAMETERS
*  enable  [in]  1:open 0:close
*RETURN
*  0
*********************************************/
static int enable_ch0(unsigned int enable)
{
	struct abovXX * this = abov_sar_ptr;
	pabov_t pDevice = NULL;
    struct input_dev *input_bottom = NULL;
	struct _buttonInfo *buttons = NULL;
	struct _buttonInfo *pCurrentButton  = NULL;
	u8 i = 0;
    u8 reg_value = 0;
	u8 msb, lsb = 0;

    pDevice = this->pDevice;
    input_bottom = pDevice->pbuttonInformation->input_ch0;
	buttons = pDevice->pbuttonInformation->buttons;
	pCurrentButton = &buttons[0];

    printk("ABOV enable_ch0 \n");
	if (enable == 1) {
		LOG_DBG("enable cap sensor bottom\n");

        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x00);
#ifdef FACTORY_VERSION
            printk("ABOV enable_ch0 factory cali ... \n");
            write_register(this, ABOV_RECALI_REG, 0x01);
            msleep(500);
#else
        if(this->first_cali_flag == false) {
             this->first_cali_flag = true;
             printk("ABOV enable_ch0 first cali ... \n");
             write_register(this, ABOV_RECALI_REG, 0x01);
             msleep(500);
        }

#endif

		//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
		    if(this->irq_disabled == 1){
		        enable_irq(this->irq);
		    	this->irq_disabled = 0;
			    LOG_DBG("abov enable_irq\n");
		    }
		//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 1;
        }

        if(!(this->channel_status & 0x01)) {
            this->channel_status |= 0x01; //bit0 for ch0 status
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);

        
        //CH0 background cap
		read_register(this, ABOV_CAP_CH0_MSB, &reg_value);
		msb = reg_value;
		read_register(this, ABOV_CAP_CH0_LSB, &reg_value);
		lsb = reg_value;
		this->ch0_backgrand_cap = (msb << 8) | lsb;
		printk("ABOV ch0_background_cap=%d;", this->ch0_backgrand_cap);

        read_register(this, ABOV_IRQSTAT_REG, &i);

#if defined(CONFIG_SENSORS)
		if (this->skip_data == true) {
			LOG_INFO("%s - skip grip event\n", __func__);
		} else {
#endif
			if ((i & pCurrentButton->mask) == (pCurrentButton->mask & ABOV_CAP_BUTTON_MASK)) {
				input_report_rel(input_bottom, REL_MISC, 1);
				input_report_rel(input_bottom, REL_X, 2);
				input_sync(input_bottom);
				pCurrentButton->state = S_PROX;
			} else {
				input_report_rel(input_bottom, REL_MISC, 2);
				input_report_rel(input_bottom, REL_X, 2);
				input_sync(input_bottom);
				pCurrentButton->state = IDLE;
			}
#if defined(CONFIG_SENSORS)
		}
#endif
		touchProcess(this);
        this->interrupt_count--;

		//mEnabled = 1;
	} else if (enable == 0) {
		LOG_DBG("disable cap sensor bottom\n");

        if(this->channel_status & 0x01) {
            this->channel_status &= ~0x01;
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x01);

		//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
		    if(this->irq_disabled == 0){
		        disable_irq(this->irq);
			    this->irq_disabled = 1;
			    LOG_DBG("abov disable_irq\n");
		    }
		//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 0;
        }
	} else {
		LOG_DBG("unknown enable symbol\n");
	}

	return 0;
}
/*********************************************
*FUNCTION
*  enable_top
*DESCRIPTION
*  enable func
*PARAMETERS
*  enable  [in]  1:open 0:close
*RETURN
*  0
*********************************************/
static int enable_ch1(unsigned int enable)
{
	struct abovXX * this = abov_sar_ptr;
	pabov_t pDevice = NULL;
    struct input_dev *input_top = NULL;
	struct _buttonInfo *buttons = NULL;
	struct _buttonInfo *pCurrentButton  = NULL;
	u8 i = 0;
  	u8 reg_value = 0;
	u8 msb, lsb = 0;

    pDevice = this->pDevice;
    input_top = pDevice->pbuttonInformation->input_ch1;
	buttons = pDevice->pbuttonInformation->buttons;
	pCurrentButton = &buttons[1];

    printk("ABOV enable_ch1 \n");
	if (enable == 1) {
		LOG_DBG("enable cap sensor top\n");

        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x00);
#ifdef FACTORY_VERSION
            printk("ABOV enable_ch1 factory cali ... \n");
            write_register(this, ABOV_RECALI_REG, 0x01);
            msleep(500);
#else
        if(this->first_cali_flag == false) {
             this->first_cali_flag = true;
             printk("ABOV enable_ch1 first cali ... \n");
             write_register(this, ABOV_RECALI_REG, 0x01);
             msleep(500);
        }
#endif

		//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
		    if(this->irq_disabled == 1){
			    enable_irq(this->irq);
			    this->irq_disabled = 0;
			    LOG_DBG("abov enable_irq\n");
		    }
		//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 1;
        }

        if(!(this->channel_status & 0x02)) {
            this->channel_status |= 0x02; //bit1 for ch1 status
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);

        //CH1 background cap
        read_register(this, ABOV_CAP_CH1_MSB, &reg_value);
        msb = reg_value;
        read_register(this, ABOV_CAP_CH1_LSB, &reg_value);
        lsb = reg_value;
        this->ch1_backgrand_cap = (msb << 8) | lsb;
        printk("ABOV ch1_background_cap=%d;", this->ch1_backgrand_cap);

        read_register(this, ABOV_IRQSTAT_REG, &i);

#if defined(CONFIG_SENSORS)
		if (this->skip_data == true) {
			LOG_INFO("%s - skip grip event\n", __func__);
		} else {
#endif
			if ((i & pCurrentButton->mask) == (pCurrentButton->mask & ABOV_CAP_BUTTON_MASK)) {
				input_report_rel(input_top, REL_MISC, 1);
				input_report_rel(input_top, REL_X, 2);
				input_sync(input_top);
				pCurrentButton->state = S_PROX;
			} else {
				input_report_rel(input_top, REL_MISC, 2);
				input_report_rel(input_top, REL_X, 2);
				input_sync(input_top);
				pCurrentButton->state = IDLE;
			}
#if defined(CONFIG_SENSORS)
		}
#endif
		touchProcess(this);
        this->interrupt_count--;

	} else if (enable == 0) {
		LOG_DBG("disable cap sensor top\n");

        if(this->channel_status & 0x02) {
            this->channel_status &= ~0x02;
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x01);

		//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
		    if(this->irq_disabled == 0){
			    disable_irq(this->irq);
			    this->irq_disabled = 1;
			    LOG_DBG("abov disable_irq\n");
		    }
		//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 0;
        }
	} else {
		LOG_DBG("unknown enable symbol\n");
	}

	return 0;
}

#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
static int enable_ch2(unsigned int enable)
{
	struct abovXX * this = abov_sar_ptr;
	pabov_t pDevice = NULL;
    struct input_dev *input_top = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u8 i = 0;
   	u8 reg_value = 0;
	u8 msb, lsb = 0;

    printk("ABOV enable_ch2 \n");
    pDevice = this->pDevice;
    input_top = pDevice->pbuttonInformation->input_ch2;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[2];

	if (enable == 1) {
		LOG_DBG("enable cap sensor top\n");

        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x00);
#ifdef FACTORY_VERSION
            printk("ABOV enable_ch2 factory cali ... \n");
            write_register(this, ABOV_RECALI_REG, 0x01);
            msleep(500);
#else
        if(this->first_cali_flag == false) {
             this->first_cali_flag = true;
             printk("ABOV enable_ch2 first cali ... \n");
             write_register(this, ABOV_RECALI_REG, 0x01);
             msleep(500);
        }
#endif

		//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
		    if(this->irq_disabled == 1){
			    enable_irq(this->irq);
			    this->irq_disabled = 0;
			    LOG_DBG("abov enable_irq\n");
		    }
		//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 1;
        }

        if(!(this->channel_status & 0x04)) {
            this->channel_status |= 0x04; //bit1 for ch1 status
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);

        //CH2 background cap
		read_register(this, ABOV_CAP_CH2_MSB, &reg_value);
		msb = reg_value;
		read_register(this, ABOV_CAP_CH2_LSB, &reg_value);
		lsb = reg_value;
		this->ch2_backgrand_cap = (msb << 8) | lsb;
		printk("ABOV ch2_background_cap=%d;", this->ch2_backgrand_cap);

        read_register(this, ABOV_IRQSTAT_REG, &i);

#if defined(CONFIG_SENSORS)
		if (this->skip_data == true) {
			LOG_INFO("%s - skip grip event\n", __func__);
		} else {
#endif
			if ((i & pCurrentButton->mask) == (pCurrentButton->mask & ABOV_CAP_BUTTON_MASK)) {
				input_report_rel(input_top, REL_MISC, 1);
				input_report_rel(input_top, REL_X, 2);
				input_sync(input_top);
				pCurrentButton->state = S_PROX;
			} else {
				input_report_rel(input_top, REL_MISC, 2);
				input_report_rel(input_top, REL_X, 2);
				input_sync(input_top);
				pCurrentButton->state = IDLE;
			}
#if defined(CONFIG_SENSORS)
		}
#endif
		touchProcess(this);
        this->interrupt_count--;

	} else if (enable == 0) {
		LOG_DBG("disable cap sensor top\n");

        if(this->channel_status & 0x04) {
            this->channel_status &= ~0x04;
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x01);

		//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
		    if(this->irq_disabled == 0){
			    disable_irq(this->irq);
			    this->irq_disabled = 1;
			    LOG_DBG("abov disable_irq\n");
		    }
		//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 0;
        }
	} else {
		LOG_DBG("unknown enable symbol\n");
	}

	return 0;
}
#endif

//+bug 492320,20191119,gaojingxuan.wt,add,use SS hal
/*********************************************
*FUNCTION
*  show_enable
*DESCRIPTION
*  SS Hal enable node
*PARAMETERS
*  dev  [in] the device
*  attr [in] attribute
*  buf  [in] 1:open 0:close
*RETURN
*  p-buf
*********************************************/
static ssize_t show_enable(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
    pabovXX_t this = abov_sar_ptr;
    int status;
    char *p = buf;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev,struct input_dev,dev);

    LOG_INFO("%s: dev->name:%s\n", __func__,temp_input_dev->name);

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if(this->channel_status & 0x01)
        {
            status = 1;
        }else{
            status = 0;
        }
    }
    if(!strncmp(temp_input_dev->name,"grip_sensor_wifi",sizeof("grip_sensor_wifi")))
    {
        if(this->channel_status & 0x02)
        {
            status = 1;
        }else{
            status = 0;
        }
    }
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
	{
		if(this->channel_status & 0x04)
		{
			status = 1;
		}else{
			status = 0;
		}
	}
#endif

    p += snprintf(p, PAGE_SIZE, "%d", status);

	return (p-buf);
}

/*********************************************
*FUNCTION
*  store_enable
*DESCRIPTION
*  SS Hal enable node
*PARAMETERS
*  dev  [in] the device
*  attr [in] attribute
*  buf  [in] 1:open 0:close
*RETURN
*  count
*********************************************/
static ssize_t store_enable(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev,struct input_dev,dev);

    LOG_INFO("%s: dev->name:%s:%s\n", __func__,temp_input_dev->name,buf);

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if (!strncmp(buf, "1", 1))
        {
            LOG_INFO("name:grip_sensor:enable\n");
            enable_ch0(1);
        }else{
            LOG_INFO("name:grip_sensor:disable\n");
            enable_ch0(0);
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_wifi",sizeof("grip_sensor_wifi")))
    {
        if (!strncmp(buf, "1", 1))
        {
            LOG_INFO("name:grip_sensor_wifi:enable\n");
            enable_ch1(1);
        }else{
            LOG_INFO("name:grip_sensor_wifi:disable\n");
            enable_ch1(0);
        }
    }

#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if (!strncmp(buf, "1", 1))
        {
            LOG_INFO("name:grip_sensor_sub:enable\n");
            enable_ch2(1);
        }else{
            LOG_INFO("name:grip_sensor_sub:disable\n");
            enable_ch2(0);
        }
    }
#endif
	return count;
}
static DEVICE_ATTR(enable, 0660, show_enable, store_enable);
//-bug 492320,20191119,gaojingxuan.wt,add,use SS hal


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

	LOG_DBG("Reading IRQSTAT_REG\n");
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
		LOG_DBG("Performing manual_offset_calibration()\n");
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
	LOG_DBG("Inside hw_init().\n");
	pDevice = this->pDevice;
	pdata = pDevice->hw;
	if (this && pDevice && pdata) {
		while (i < pdata->i2c_reg_num) {
			/* Write all registers/values contained in i2c_reg */
			write_register(this, pdata->pi2c_reg[i].reg,
					pdata->pi2c_reg[i].val);
			i++;
		}
	} else {
		LOG_ERR("ERROR! platform data 0x%p\n", pDevice->hw);
		/* Force to touched if error */
		ForcetoTouched(this);
	}
	LOG_DBG("Exiting hw_init().\n");
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
		LOG_DBG("Inside initialize().\n");
		/* prepare reset by disabling any irq handling */
		if(this->irq_disabled == 0) {
		   disable_irq(this->irq);
		   this->irq_disabled = 1;
		}
		hw_init(this);
		msleep(300); /* make sure everything is running */
		/* re-enable interrupt handling */
		if(this->irq_disabled == 1) {
		   enable_irq(this->irq);
		   this->irq_disabled = 0;
		}
		/* make sure no interrupts are pending since enabling irq will only
		 * work on next falling edge */
		LOG_DBG("Exiting initialize().\n");
		programming_done = ACTIVE;
		return 0;
	}
	programming_done = IDLE;
	return -ENOMEM;
}

static int __init sales_code_setup(char *str)
{
    strlcpy(sales_code_from_cmdline, str, ARRAY_SIZE(sales_code_from_cmdline));
    return 1;
 }
__setup("androidboot.sales_code=", sales_code_setup);

/**
 * brief Handle what to do when a touch occurs
 * param this Pointer to main parent struct
 */
static void touchProcess(pabovXX_t this)
{
	int counter = 0;
	u8  capsense_state = 0;
	int numberOfButtons = 0;
	pabov_t pDevice = NULL;
	struct _buttonInfo *buttons = NULL;
	struct input_dev *input_ch0 = NULL;
	struct input_dev *input_ch1 = NULL;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	struct input_dev *input_ch2 = NULL;
#endif
	struct _buttonInfo *pCurrentButton  = NULL;
	struct abov_platform_data *board;

	pDevice = this->pDevice;
	board = this->board;
	printk("ABOV Inside touchProcess() this->channel_status =0x%x\n",this->channel_status);

    u8 reg_value = 0; 
    u8 msb, lsb = 0; 
    static u16 ch0_result = 0; 
    static u16 ch1_result = 0; 
    static u16 ch2_result = 0; 
 
    this->interrupt_count++;
	if (this->interrupt_count > MAX_INT_COUNT)
		this->interrupt_count = MAX_INT_COUNT + 1;

	if (this && pDevice) {
		LOG_DBG("Inside touchProcess()\n");
		read_register(this, ABOV_IRQSTAT_REG, &capsense_state);
		LOG_DBG("capsense_state=%d\n", capsense_state);

		buttons = pDevice->pbuttonInformation->buttons;
		input_ch0 = pDevice->pbuttonInformation->input_ch0;
		input_ch1 = pDevice->pbuttonInformation->input_ch1;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
		input_ch2 = pDevice->pbuttonInformation->input_ch2;
#endif
		numberOfButtons = pDevice->pbuttonInformation->buttonSize;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
		if (unlikely((buttons == NULL) || (input_ch0 == NULL) || (input_ch1 == NULL) || (input_ch2 == NULL)))
#else
	    if (unlikely((buttons == NULL) || (input_ch0 == NULL) || (input_ch1 == NULL)))
#endif
	   	{
			LOG_ERR("ERROR!! buttons or input NULL!!!\n");
			return;
		}

        printk("ABOV sales_code_from_cmdline=%s,is_anfr_sar = %d\n",sales_code_from_cmdline,(uint8_t)is_anfr_sar);

        if (this->abov_first_boot)	
		{
			if ((this->channel_status & 0x01) == 1) {
				//CH0 background cap
				read_register(this, ABOV_CAP_CH0_MSB, &reg_value);
				msb = reg_value;
				read_register(this, ABOV_CAP_CH0_LSB, &reg_value);
				lsb = reg_value;
				ch0_result = (msb << 8) | lsb;
				printk("ABOV ch0_result=%d;", ch0_result);
			}
	
			if ((this->channel_status & 0x02) >> 1 == 1)  {
				//CH1 background cap
				read_register(this, ABOV_CAP_CH1_MSB, &reg_value);
				msb = reg_value;
				read_register(this, ABOV_CAP_CH1_LSB, &reg_value);
				lsb = reg_value;
				ch1_result = (msb << 8) | lsb;
				printk("ABOV ch1_result=%d;", ch1_result);
			}
	
			if ((this->channel_status & 0x04) >> 2 == 1)  {
				//CH2 background cap
				read_register(this, ABOV_CAP_CH2_MSB, &reg_value);
				msb = reg_value;
				read_register(this, ABOV_CAP_CH2_LSB, &reg_value);
				lsb = reg_value;
				ch2_result = (msb << 8) | lsb;
				printk("ABOV ch2_result=%d;", ch2_result);
			}	
		}
        printk("ABOV ch0_backgrand_cap=%d, ch1_backgrand_cap=%d, ch2_backgrand_cap=%d;", this->ch0_backgrand_cap,this->ch1_backgrand_cap,this->ch2_backgrand_cap);
		printk("ABOV ch0_result=%d, ch1_result=%d, ch2_result=%d;", ch0_result,ch1_result,ch2_result);
		printk("ABOV interrupt_count is %d;", this->interrupt_count);
		if (this->abov_first_boot){
            if((this->interrupt_count <= MAX_INT_COUNT) && is_anfr_sar && (ch0_result >= this->ch0_backgrand_cap) && (ch1_result >= this->ch1_backgrand_cap) && (ch2_result >= this->ch2_backgrand_cap)){
		    	printk("ABOV force all channel State=Near\n");
			    input_report_rel(input_ch0, REL_MISC, 1);
				input_report_rel(input_ch0, REL_X, 1);
			    input_sync(input_ch0);
			    input_report_rel(input_ch1, REL_MISC, 1);
				input_report_rel(input_ch1, REL_X, 1);
			    input_sync(input_ch1);
			    input_report_rel(input_ch2, REL_MISC, 1);
				input_report_rel(input_ch2, REL_X, 1);
			    input_sync(input_ch2);
			    return;
            }
		}

		this->abov_first_boot = false;

#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            LOG_INFO("%s - skip grip event\n", __func__);
            return;
        }
#endif

		for (counter = 0; counter < numberOfButtons; counter++) {
			pCurrentButton = &buttons[counter];
			if (pCurrentButton == NULL) {
				LOG_ERR("ERR!current button index: %d NULL!\n",
						counter);
				return; /* ERRORR!!!! */
			}
                    //printk("ABOV Inside touchProcess() state = %d capsense_state= %x pCurrentButton->mask =%x\n",pCurrentButton->state,capsense_state,pCurrentButton->mask);
#if POWER_ENABLE
            if (this->power_state) {
                if ((capsense_state & pCurrentButton->mask) == (pCurrentButton->mask & ABOV_CAP_BUTTON_MASK)) {
                    if ((board->cap_channel_ch0 == counter) &&  (this->channel_status & 0x01)) {
                        input_report_rel(input_ch0, REL_MISC, 2);
						input_report_rel(input_ch0, REL_X, 2);
                        input_sync(input_ch0);
                    } else if ((board->cap_channel_ch1 == counter) &&  (this->channel_status & 0x02)) {
                        input_report_rel(input_ch1, REL_MISC, 2);
						input_report_rel(input_ch1, REL_X, 2);
                        input_sync(input_ch1);

                    }
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
                    else if ((board->cap_channel_ch2 == counter) &&  (this->channel_status & 0x04)) {
                        input_report_rel(input_ch2, REL_MISC, 2);
						input_report_rel(input_ch2, REL_X, 2);
                        input_sync(input_ch2);
                    }
#endif
                    pCurrentButton->state = IDLE;
                }
            } else {
#endif
                switch (pCurrentButton->state) {
			    case IDLE: /* Button is being in far state! */
				    if ((capsense_state & pCurrentButton->mask) == (pCurrentButton->mask & ABOV_CAP_BUTTON_MASK)) {
					    printk("ABOV CH%d State=NEAR.\n", counter);
					    if ((board->cap_channel_ch0 == counter) &&  (this->channel_status & 0x01)) {
						    input_report_rel(input_ch0, REL_MISC, 1);
						    input_report_rel(input_ch0, REL_X, 2);
    						input_sync(input_ch0);
	    				} else if ((board->cap_channel_ch1 == counter) &&  (this->channel_status & 0x02)) {
						    input_report_rel(input_ch1, REL_MISC, 1);
						    input_report_rel(input_ch1, REL_X, 2);
			    			input_sync(input_ch1);
				    	}
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
				    	else if ((board->cap_channel_ch2 == counter) &&  (this->channel_status & 0x04)) {
						    input_report_rel(input_ch2, REL_MISC, 1);
						    input_report_rel(input_ch2, REL_X, 2);
						    input_sync(input_ch2);
				    	}
#endif
					    pCurrentButton->state = S_PROX;
			    	} else {
				    	printk("ABOV CH%d still in FAR State.\n", counter);
			    	}
				    break;
			    case S_PROX: /* Button is being in proximity! */
				    if ((capsense_state & pCurrentButton->mask) == (pCurrentButton->mask & ABOV_CAP_BUTTON_MASK)) {
				    	printk("ABOV CH%d still in NEAR State.\n", counter);
	    			} else{
		    			printk("ABOV CH%d State=FAR.\n", counter);
			    		if ((board->cap_channel_ch0 == counter) &&  (this->channel_status & 0x01)) {
				    		input_report_rel(input_ch0, REL_MISC, 2);
						    input_report_rel(input_ch0, REL_X, 2);
					    	input_sync(input_ch0);
    					} else if ((board->cap_channel_ch1 == counter) &&  (this->channel_status & 0x02)) {
				    		input_report_rel(input_ch1, REL_MISC, 2);
						    input_report_rel(input_ch1, REL_X, 2);
		    				input_sync(input_ch1);
			    		}
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
				    	else if ((board->cap_channel_ch2 == counter) &&  (this->channel_status & 0x04)) {
				    		input_report_rel(input_ch2, REL_MISC, 2);
						    input_report_rel(input_ch2, REL_X, 2);
						    input_sync(input_ch2);
    					}
#endif
	    				pCurrentButton->state = IDLE;
                    }
				    break;
			    default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
			    	break;
		    	};
#if POWER_ENABLE
            }
#endif
		}
		printk("ABOV Leaving touchProcess()\n");
	}
}

static int abov_get_nirq_state(unsigned irq_gpio)
{
	if (irq_gpio) {
		return !gpio_get_value(irq_gpio);
	} else {
		printk("abov irq_gpio is not set.");
		return -EINVAL;
	}
}

static struct _totalButtonInformation smtcButtonInformation = {
	.buttons = psmtcButtons,
	.buttonSize = ARRAY_SIZE(psmtcButtons),
};

/**
 *fn static void abov_reg_setup_init(struct i2c_client *client)
 *brief read reg val form dts
 *      reg_array_len for regs needed change num
 *      data_array_val's format <reg val ...>
 */
static void abov_reg_setup_init(struct i2c_client *client)
{
	u32 data_array_len = 0;
	u32 *data_array;
	int ret, i, j;
	struct device_node *np = client->dev.of_node;

	ret = of_property_read_u32(np, "reg_array_len", &data_array_len);
	if (ret < 0) {
		LOG_ERR("data_array_len read error");
		return;
	}
	data_array = kmalloc(data_array_len * 2 * sizeof(u32), GFP_KERNEL);
	ret = of_property_read_u32_array(np, "reg_array_val",
			data_array,
			data_array_len*2);
	if (ret < 0) {
		LOG_ERR("data_array_val read error");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(abov_i2c_reg_setup); i++) {
		for (j = 0; j < data_array_len*2; j += 2) {
			if (data_array[j] == abov_i2c_reg_setup[i].reg) {
				abov_i2c_reg_setup[i].val = data_array[j+1];
				LOG_DBG("read dtsi 0x%02x:0x%02x set reg\n",
					data_array[j], data_array[j+1]);
			}
		}
	}
	kfree(data_array);
}

//+bug676569,jiaoyuxin.wt,add,20210715,distinguish sar firmware according to Latin America version
static bool sar_get_boardid(void)
{
	char board_id[64];
	char *br_ptr;
	char *br_ptr_e;
	bool board_is_la = false;

	memset(board_id, 0x0, 64);
	br_ptr = strstr(saved_command_line, "androidboot.board_id=");
	if (br_ptr != 0) {
		br_ptr_e = strstr(br_ptr, " ");
		/* get board id */
		if (br_ptr_e != 0) {
			strncpy(board_id, br_ptr + 21,
					br_ptr_e - br_ptr - 21);
			board_id[br_ptr_e - br_ptr - 21] = '\0';
		}

		printk(" board_id = %s ", board_id);
        /* if it is LA board */
		if ((!strncmp(board_id, "S96902AA1",strlen("S96902AA1")))) {
		     board_is_la = true;
		}
	} else
        board_is_la = false;

	return board_is_la;

}
//-bug676569,jiaoyuxin.wt,add,20210715,distinguish sar firmware according to Latin America version


static void abov_platform_data_of_init(struct i2c_client *client,
		pabov_platform_data_t pplatData)
{
	struct device_node *np = client->dev.of_node;
	u32 cap_channel_ch0,cap_channel_ch1;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	u32 cap_channel_ch2;
#endif
	int ret;
	//bug676569,jiaoyuxin.wt,add,20210715,distinguish sar firmware according to Latin America version
	int boardid=0;



    client->irq = of_get_named_gpio_flags(np, "abov,irq-gpio-std", 0, NULL);
	pplatData->irq_gpio = client->irq;
	LOG_INFO("irq_gpio = %d\n", pplatData->irq_gpio);

	ret = of_property_read_u32(np, "cap,use_channel_ch0", &cap_channel_ch0);
	ret = of_property_read_string(np, "ch0_name", &pplatData->cap_ch0_name);
	ret = of_property_read_u32(np, "cap,use_channel_ch1", &cap_channel_ch1);
	ret = of_property_read_string(np, "ch1_name", &pplatData->cap_ch1_name);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	ret = of_property_read_u32(np, "cap,use_channel_ch2", &cap_channel_ch2);
	ret = of_property_read_string(np, "ch2_name", &pplatData->cap_ch2_name);
#endif
	pplatData->cap_channel_ch0 = (int)cap_channel_ch0;
	pplatData->cap_channel_ch1 = (int)cap_channel_ch1;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	pplatData->cap_channel_ch2 = (int)cap_channel_ch2;
#endif

#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	LOG_INFO("cap_channel_ch0 = %d,cap_channel_ch1 = %d,cap_channel_ch2 = %d\n",
		cap_channel_ch0, cap_channel_ch1, cap_channel_ch2);
	LOG_INFO("cap_ch0_name = %s,cap_ch1_name = %s,cap_ch2_name = %s\n",
		pplatData->cap_ch0_name, pplatData->cap_ch1_name, pplatData->cap_ch2_name);
#else
	LOG_INFO("cap_channel_ch0 = %d,cap_channel_ch1 = %d\n",
		cap_channel_ch0, cap_channel_ch1);
	LOG_INFO("cap_ch0_name = %s,cap_ch1_name = %s\n",
		pplatData->cap_ch0_name, pplatData->cap_ch1_name);
#endif

	pplatData->get_is_nirq_low = abov_get_nirq_state;
	pplatData->init_platform_hw = NULL;
	/*  pointer to an exit function. Here in case needed in the future */
	/*
	 *.exit_platform_hw = abov_exit_ts,
	 */
	pplatData->exit_platform_hw = NULL;
	abov_reg_setup_init(client);
	pplatData->pi2c_reg = abov_i2c_reg_setup;
	pplatData->i2c_reg_num = ARRAY_SIZE(abov_i2c_reg_setup);

	pplatData->pbuttonInformation = &smtcButtonInformation;

	ret = of_property_read_string(np, "label_la", &pplatData->fw_name);
	if (ret < 0) {
		LOG_ERR("firmware name read error!\n");
		return;
        //goto err_find;
	}
	LOG_INFO("firmware name: %s\n", pplatData->fw_name);

}

static ssize_t ch0_cap_diff_dump_show(struct class *class, 
	struct class_attribute *attr, 
	char *buf)
{
   u8 reg_value = 0;
   u8 msb_ch0, lsb_ch0 = 0;
   u16 reg_result = 0;
   pabovXX_t this = abov_sar_ptr;
   char *p = buf;

  //CH0 background cap
  read_register(this, ABOV_CAP_CH0_MSB, &reg_value);
  msb_ch0 = reg_value;
  read_register(this, ABOV_CAP_CH0_LSB, &reg_value);
  lsb_ch0 = reg_value;
  reg_result = ((msb_ch0 << 8) | lsb_ch0) / 100;
  p += snprintf(p, PAGE_SIZE, "CH0_background_cap=%d;", reg_result);
  //CH0 refer channel cap
  read_register(this, ABOV_CAP_REF0_MSB, &reg_value);
  msb_ch0 = reg_value;
  read_register(this, ABOV_CAP_REF0_LSB, &reg_value);
  lsb_ch0 = reg_value;
  reg_result = ((msb_ch0 << 8) | lsb_ch0) / 100;
  p += snprintf(p, PAGE_SIZE, "CH0_refer_channel_cap=%d;", reg_result);
  //CH0 diff
  read_register(this, ABOV_DIFF_CH0_MSB, &reg_value);
  msb_ch0 = reg_value;
  read_register(this, ABOV_DIFF_CH0_LSB, &reg_value);
  lsb_ch0 = reg_value;
  reg_result = (msb_ch0 << 8) | lsb_ch0;
  p += snprintf(p, PAGE_SIZE, "CH0_diff=%d", reg_result);

  return (p-buf);
}

static ssize_t ch2_cap_diff_dump_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
   u8 reg_value = 0;
   u8 msb_ch1, lsb_ch1 = 0;
   u16 reg_result = 0;
   pabovXX_t this = abov_sar_ptr;
   char *p = buf;

  //CH1 background cap
  read_register(this, ABOV_CAP_CH1_MSB, &reg_value);
  msb_ch1 = reg_value;
  read_register(this, ABOV_CAP_CH1_LSB, &reg_value);
  lsb_ch1 = reg_value;
  reg_result = ((msb_ch1 << 8) | lsb_ch1) / 100;
  p += snprintf(p, PAGE_SIZE, "CH2_background_cap=%d;", reg_result);
  //CH1 refer channel cap
  read_register(this, ABOV_CAP_REF1_MSB, &reg_value);
  msb_ch1 = reg_value;
  read_register(this, ABOV_CAP_REF1_LSB, &reg_value);
  lsb_ch1 = reg_value;
  reg_result = ((msb_ch1 << 8) | lsb_ch1) / 100;
  p += snprintf(p, PAGE_SIZE, "CH2_refer_channel_cap=%d;", reg_result);
  //CH1 diff
  read_register(this, ABOV_DIFF_CH1_MSB, &reg_value);
  msb_ch1 = reg_value;
  read_register(this, ABOV_DIFF_CH1_LSB, &reg_value);
  lsb_ch1 = reg_value;
  reg_result = (msb_ch1 << 8) | lsb_ch1;
  p += snprintf(p, PAGE_SIZE, "CH2_diff=%d", reg_result);

  return (p-buf);
}

#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
static ssize_t ch3_cap_diff_dump_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
   u8 reg_value = 0;
   u8 msb_ch2, lsb_ch2 = 0;
   u16 reg_result = 0;
   pabovXX_t this = abov_sar_ptr;
   char *p = buf;

  //CH2 background cap
  read_register(this, ABOV_CAP_CH2_MSB, &reg_value);
  msb_ch2 = reg_value;
  read_register(this, ABOV_CAP_CH2_LSB, &reg_value);
  lsb_ch2 = reg_value;
  reg_result = ((msb_ch2 << 8) | lsb_ch2) / 100;
  p += snprintf(p, PAGE_SIZE, "CH3_background_cap=%d;", reg_result);
  //CH2 refer channel cap
  read_register(this, ABOV_CAP_REF2_MSB, &reg_value);
  msb_ch2 = reg_value;
  read_register(this, ABOV_CAP_REF2_LSB, &reg_value);
  lsb_ch2 = reg_value;
  reg_result = ((msb_ch2 << 8) | lsb_ch2) / 100;
  p += snprintf(p, PAGE_SIZE, "CH2_refer_channel_cap=%d;", reg_result);
  //CH2 diff
  read_register(this, ABOV_DIFF_CH2_MSB, &reg_value);
  msb_ch2 = reg_value;
  read_register(this, ABOV_DIFF_CH2_LSB, &reg_value);
  lsb_ch2 = reg_value;
  reg_result = (msb_ch2 << 8) | lsb_ch2;
  p += snprintf(p, PAGE_SIZE, "CH3_diff=%d", reg_result);

  return (p-buf);
}
#endif

static ssize_t int_state_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	pabovXX_t this = abov_sar_ptr;
	LOG_DBG("Reading INT line state\n");
	return snprintf(buf, 8, "%d\n", this->int_state);
}

static ssize_t enable_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 8, "%d\n", mEnabled);
}

static ssize_t enable_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	pabov_t pDevice = NULL;
	struct input_dev *input_ch0 = NULL;
	struct input_dev *input_ch1 = NULL;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	struct input_dev *input_ch2 = NULL;
#endif

	pDevice = this->pDevice;
	input_ch0 = pDevice->pbuttonInformation->input_ch0;
	input_ch1 = pDevice->pbuttonInformation->input_ch1;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	input_ch2 = pDevice->pbuttonInformation->input_ch2;
#endif
       printk("ABOV abov enable = %c \n", *buf);
	if (!count || (this == NULL))
		return -EINVAL;

	if (!strncmp(buf, "1", 1) && (mEnabled == 0)) {
		LOG_INFO("ABOV enable cap sensor\n");
		initialize(this);

		input_report_rel(input_ch0, REL_MISC, 2);
		input_report_rel(input_ch0, REL_X, 2);
		input_sync(input_ch0);
		input_report_rel(input_ch1, REL_MISC, 2);
		input_report_rel(input_ch1, REL_X, 2);
		input_sync(input_ch1);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
		input_report_rel(input_ch2, REL_MISC, 2);
		input_report_rel(input_ch2, REL_X, 2);
		input_sync(input_ch2);
#endif

		mEnabled = 1;
	} else if (!strncmp(buf, "0", 1) && (mEnabled == 1)) {
		LOG_INFO("disable cap sensor\n");

		write_register(this, ABOV_CTRL_MODE_REG, ABOV_CTRL_MODE_STOP);

		input_report_rel(input_ch0, REL_MISC, 2);
		input_sync(input_ch0);
		input_report_rel(input_ch1, REL_MISC, 2);
		input_sync(input_ch1);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
		input_report_rel(input_ch2, REL_MISC, 2);
		input_sync(input_ch2);
#endif

		mEnabled = 0;
	} else {
		LOG_ERR("unknown enable symbol\n");
	}

	return count;
}

#ifdef USE_SENSORS_CLASS
static int capsensor_set_enable_ch0(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	return enable_ch0(enable);
}
static int capsensor_set_enable_ch1(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	return enable_ch1(enable);
}
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
static int capsensor_set_enable_ch2(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	return enable_ch2(enable);
}
#endif
#endif


static ssize_t reg_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u8 reg_value = 0, i;
	pabovXX_t this = abov_sar_ptr;
	char *p = buf;

	if (this->read_flag) {
		this->read_flag = 0;
		read_register(this, this->read_reg, &reg_value);
		p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n", this->read_reg, reg_value);
		return (p-buf);
	}

	for (i = 0; i < 0x56; i++) {
		read_register(this, i, &reg_value);
		p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n",
				i, reg_value);
	}

	for (i = 0x80; i < 0xB0; i++) {
		read_register(this, i, &reg_value);
		p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n",
				i, reg_value);
	}

	return (p-buf);
}

static ssize_t reg_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	unsigned int val, reg, opt;
    if (sscanf(buf, "%x,%x,%x", &reg, &val, &opt) == 3) {
		LOG_DBG("%s, read reg = 0x%02x\n", __func__, *(u8 *)&reg);
		this->read_reg = *((u8 *)&reg);
		this->read_flag = 1;
	} else if (sscanf(buf, "%x,%x", &reg, &val) == 2) {
		LOG_DBG("%s,reg = 0x%02x, val = 0x%02x\n",
				__func__, *(u8 *)&reg, *(u8 *)&val);
		write_register(this, *((u8 *)&reg), *((u8 *)&val));
	}

	return count;
}

#if defined(CONFIG_SENSORS)
static ssize_t abovxx_onoff_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    pabovXX_t this = abov_sar_ptr;

    return snprintf(buf, PAGE_SIZE, "%u\n", !this->skip_data);
}

static ssize_t abovxx_onoff_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val;
    int ret;
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_ch0 = NULL;
    struct input_dev *input_ch1 = NULL;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
    struct input_dev *input_ch2 = NULL;
#endif

    pDevice = this->pDevice;
    input_ch0 = pDevice->pbuttonInformation->input_ch0;
    input_ch1 = pDevice->pbuttonInformation->input_ch1;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
    input_ch2 = pDevice->pbuttonInformation->input_ch2;
#endif

    ret = kstrtou8(buf, 2, &val);
    if (ret) {
        LOG_ERR("%s - Invalid Argument\n", __func__);
        return ret;
    }

    if (val == 0) {
        this->skip_data = true;
        if (mEnabled) {
            input_report_rel(input_ch0, REL_MISC, 2);
            input_sync(input_ch0);
            input_report_rel(input_ch1, REL_MISC, 2);
            input_sync(input_ch1);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
            input_report_rel(input_ch2, REL_MISC, 2);
            input_sync(input_ch2);
#endif
        }
    } else {
        this->skip_data = false;
    }

    LOG_INFO("%s -%u\n", __func__, val);
    return count;
}

static int abovxx_grip_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    pDevice = this->pDevice;

    input_report_rel(pDevice->pbuttonInformation->input_ch0, REL_MAX, val);
    input_sync(pDevice->pbuttonInformation->input_ch0);

    LOG_INFO("%s val=%u\n", __func__, val);
    return 0;
}

static int abovxx_grip_wifi_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    pDevice = this->pDevice;

    input_report_rel(pDevice->pbuttonInformation->input_ch1, REL_MAX, val);
    input_sync(pDevice->pbuttonInformation->input_ch1);

    LOG_INFO("%s -%u\n", __func__, val);
    return 0;
}

#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
static int abovxx_grip_sub_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    pDevice = this->pDevice;

    input_report_rel(pDevice->pbuttonInformation->input_ch2, REL_MAX, val);
    input_sync(pDevice->pbuttonInformation->input_ch2);

    LOG_INFO("%s -%u\n", __func__, val);
    return 0;
}
#endif

static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
        abovxx_onoff_show, abovxx_onoff_store);
#endif

#ifdef USE_USB_CHANGE_RECAL
static void ps_notify_callback_work(struct work_struct *work)
{
	pabovXX_t this = container_of(work, abovXX_t, ps_notify_work);
	pabov_t pDevice = NULL;
	struct input_dev *input_ch0 = NULL;
	struct input_dev *input_ch1 = NULL;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	struct input_dev *input_ch2 = NULL;
#endif
	int ret = 0;

	pDevice = this->pDevice;
	input_ch0 = pDevice->pbuttonInformation->input_ch0;
	input_ch1 = pDevice->pbuttonInformation->input_ch1;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	input_ch2 = pDevice->pbuttonInformation->input_ch2;
#endif

	LOG_INFO("Usb insert,going to force calibrate\n");
	ret = write_register(this, ABOV_RECALI_REG, 0x01);
	if (ret < 0)
		LOG_ERR(" Usb insert,calibrate cap sensor failed\n");

	input_report_rel(input_ch0, REL_MISC, 2);
	input_report_rel(input_ch0, REL_X, 2);
	input_sync(input_ch0);
	input_report_rel(input_ch1, REL_MISC, 2);
	input_report_rel(input_ch1, REL_X, 2);
	input_sync(input_ch1);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	input_report_rel(input_ch2, REL_MISC, 2);
	input_report_rel(input_ch2, REL_X, 2);
	input_sync(input_ch2);
#endif
}

static int ps_get_state(struct power_supply *psy, bool *present)
{
	union power_supply_propval pval = { 0 };
	int retval;

	retval = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
			&pval);
	if (retval) {
		LOG_ERR("%s psy get property failed\n", psy->desc->name);
		return retval;
	}
	*present = (pval.intval) ? true : false;
	LOG_INFO("%s is %s\n", psy->desc->name,
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

	if (event == PSY_EVENT_PROP_CHANGED
			&& psy && psy->desc->get_property && psy->desc->name &&
			!strncmp(psy->desc->name, "usb", sizeof("usb"))){
		LOG_INFO("ps notification: event = %lu\n", event);
		retval = ps_get_state(psy, &present);
		if (retval) {
			LOG_ERR("psy get property failed\n");
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
	unsigned char buf[36] = {0, };

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
	if (ret != 2) {
		LOG_DBG("write fail(retry:%d)... ret:%d\n", retry_count, ret);
		if (retry_count-- > 0) {
			goto retry;
		}
		return -EIO;
	} else {
		LOG_INFO("success reset & boot mode... ret:%d\n", ret);
		return 0;
	}
}

static int abov_tk_fw_mode_enter(struct i2c_client *client)
{
	int ret = 0;
	unsigned char buf[2] = {0xAC, 0x5B};
	unsigned char device_id = 0;

	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		LOG_ERR("SEND : fail - addr:0x%02x data:0x%02x 0x%02x... ret:%d\n", client->addr, buf[0], buf[1], ret);
		return -EIO;
	}
	LOG_INFO("SEND : succ - addr:0x%02x data:0x%02x 0x%02x... ret:%d\n", client->addr, buf[0], buf[1], ret);
	SLEEP(5);

	ret = i2c_master_recv(client, &device_id, 1);
	if (device_id != ABOV_BOOT_DEVICE_ID) {
		LOG_ERR("Enter fw mode fail,device id:0x%02x... ret:%d\n", device_id, ret);
		return -EIO;
	}

	LOG_INFO("Enter fw mode success ... device id:0x%02x... ret:%d\n", device_id, ret);

	return 0;
}

static int abov_tk_fw_mode_exit(struct i2c_client *client)
{
	int ret = 0;
	unsigned char buf[2] = {0xAC, 0xE1};

	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		LOG_ERR("SEND : fail - addr:0x%02x data:0x%02x 0x%02x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
		return -EIO;
	}
	LOG_INFO("SEND : succ - addr:0x%02x data:0x%02x 0x%02x ... ret:%d\n", client->addr, buf[0], buf[1], ret);

	return 0;
}

static int abov_tk_flash_erase(struct i2c_client *client)
{
	int ret = 0;
	unsigned char buf[2] = {0xAC, 0x2E};

	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		LOG_ERR("SEND : fail - addr:0x%02x data:0x%02x 0x%02x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
		return -EIO;
	}

	LOG_INFO("SEND : succ - addr:0x%02x data:0x%02x 0x%02x ... ret:%d\n", client->addr, buf[0], buf[1], ret);

	return 0;
}

static int abov_tk_i2c_read_checksum(struct i2c_client *client)
{
	unsigned char checksum[6] = {0, };
	unsigned char buf[6] = {0, };
	int ret;

	checksum_h = 0;
	checksum_l = 0;

	buf[0] = 0xAC;
	buf[1] = 0x9E;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = checksum_h_bin;
	buf[5] = checksum_l_bin;
	
	ret = i2c_master_send(client, buf, 6);
	if (ret != 6) {
		LOG_ERR("SEND : fail - addr:0x%02x data:0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x ... ret:%d\n",
			client->addr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], ret);
		return -EIO;
	}
	SLEEP(5);

	buf[0] = 0x00;
	ret = i2c_master_send(client, buf, 1);
	if (ret != 1) {
		LOG_ERR("SEND : fail - addr:0x%02x data:0x%02x ... ret:%d\n", client->addr, buf[0], ret);
		return -EIO;
	}
	SLEEP(5);

	ret = i2c_master_recv(client, checksum, 6);
	if (ret < 0) {
		LOG_ERR("Read checksum fail-data:0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x...ret:%d\n",
				checksum[0], checksum[1], checksum[2], checksum[3], checksum[4], checksum[5], ret);
		return -EIO;
	}
    LOG_INFO("Read checksum succ ...ret:%d\n", ret);

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
			SLEEP(5);
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

	SLEEP(ABOV_FW_FLASH_ERASE_TIME);

	address = 0x800;
	count = size / 32;

	for (ii = 0; ii < count; ii++) {
		/* first 32byte is header */
		addrH = (unsigned char)((address >> 8) & 0xFF);
		addrL = (unsigned char)(address & 0xFF);
		memcpy(data, &image[ii * 32], 32);
		ret = abov_tk_fw_write(client, &addrH, &addrL, data);
		if (ret < 0) {
			LOG_ERR("fw_write.. ii = %d err\r\n", ii);
			return ret;
		}

		address += 0x20;
		memset(data, 0, 32);
	}

	ret = abov_tk_i2c_read_checksum(client);
	ret = abov_tk_fw_mode_exit(client);
	if ((checksum_h == checksum_h_bin) && (checksum_l == checksum_l_bin)) {
		LOG_INFO("Firmware update success. checksum_h=0x%02x,checksum_h_bin=0x%02x,checksum_l=0x%02x,checksum_l_bin=0x%02x\n",
			checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
	} else {
		LOG_ERR("Firmware update fail. checksum_h=0x%02x,checksum_h_bin=0x%02x,checksum_l=0x%02x,checksum_l_bin=0x%02x\n",
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
    //+ExtB P210323-06346,jiaoyuxin.wt,add,20210407,SAR FW first update failed
    #if WT_ADD_SAR_HARDWARE_INFO
    char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];
    #endif
    //-ExtB P210323-06346,jiaoyuxin.wt,add,20210407,SAR FW first update failed

	strcpy(fw_name, this->board->fw_name);
	strlcat(fw_name, ".BIN", NAME_MAX);
	rc = request_firmware(&fw, fw_name, this->pdev);
	if (rc < 0) {
		read_register(this, ABOV_VERSION_REG, &fw_version);
		read_register(this, ABOV_MODELNO_REG, &fw_modelno);
		LOG_ERR("Request firmware failed - %s (%d),current fw info inside IC:Version=0x%02x,ModelNo=0x%02x\n",
				this->board->fw_name, rc, fw_version, fw_modelno);
		return rc;
	}

    if (force == false) {
		read_register(this, ABOV_VERSION_REG, &fw_version);
		read_register(this, ABOV_MODELNO_REG, &fw_modelno);
		LOG_INFO("Version in sensor is:0x%02x ,ModelNo in sensor is:0x%02x\n" ,fw_version ,fw_modelno);
    }

	fw_file_modeno = fw->data[1];
	fw_file_version = fw->data[5];
	checksum_h_bin = fw->data[8];
	checksum_l_bin = fw->data[9];
	LOG_INFO("Version in file is:0x%02x ,ModelNo in file is:0x%02x\n" ,fw_file_version ,fw_file_modeno);

	if ((force) || (fw_version < fw_file_version) || (fw_modelno != fw_file_modeno)) {
		LOG_INFO("Firmware is not latest,going to fw upgrade...\n");
		fw_upgrade = true;
	} else {
		LOG_INFO("Firmware is latest,exiting fw upgrade...\n");
		fw_upgrade = false;
		rc = -EIO;
		goto rel_fw;
	}

	if (fw_upgrade) {
		for (update_loop = 0; update_loop < 10; update_loop++) {
			rc = _abov_fw_update(client, &fw->data[32], fw->size-32);
			if (rc < 0)
				LOG_ERR("retry : %d times!\n", update_loop);
			else {
				break;
			}
		}
		if (update_loop >= 10) {
			rc = -EIO;
		}
	}
    //+ExtB P210323-06346,jiaoyuxin.wt,add,20210407,SAR FW first update failed
    #if WT_ADD_SAR_HARDWARE_INFO
    mdelay(10);
    read_register(this, ABOV_VERSION_REG, &fw_version);
    snprintf(firmware_ver,HARDWARE_MAX_ITEM_LONGTH,"A96T376EFB,FW:0x%x",fw_version);
    hardwareinfo_set_prop(HARDWARE_SAR,firmware_ver);
    #endif
    //-ExtB P210323-06346,jiaoyuxin.wt,add,20210407,SAR FW first update failed

rel_fw:
	release_firmware(fw);
	return rc;
}



static ssize_t update_fw_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u8 fw_version = 0;
	pabovXX_t this = abov_sar_ptr;

	read_register(this, ABOV_VERSION_REG, &fw_version);

	return snprintf(buf, 37, "ABOV CapSensor Firmware Version:0x%02x\n", fw_version);
}

static ssize_t update_fw_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	unsigned long val;
	int rc;

	if (count > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

 	if(this->irq_disabled == 0) {
		disable_irq(this->irq);
		this->irq_disabled = 1;
	}
	mutex_lock(&this->mutex);
	if (!this->loading_fw  && val) {
		this->loading_fw = true;
		abov_fw_update(false);
		this->loading_fw = false;
	}
	mutex_unlock(&this->mutex);

	if(this->irq_disabled == 1) {
		enable_irq(this->irq);
		this->irq_disabled = 0;
	}

	return count;
}



static ssize_t force_update_fw_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u8 fw_version = 0;
	pabovXX_t this = abov_sar_ptr;

	read_register(this, ABOV_VERSION_REG, &fw_version);

	return snprintf(buf, 37, "ABOV CapSensor Firmware Version:0x%02x\n", fw_version);
}

static ssize_t force_update_fw_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	pabovXX_t this = abov_sar_ptr;
	unsigned long val;
	int rc;

	if (count > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

 	if(this->irq_disabled == 0) {
		disable_irq(this->irq);
		this->irq_disabled = 1;
	}

	mutex_lock(&this->mutex);
	if (!this->loading_fw  && val) {
		this->loading_fw = true;
		abov_fw_update(true);
		this->loading_fw = false;
	}
	mutex_unlock(&this->mutex);

	if(this->irq_disabled == 1) {
		enable_irq(this->irq);
		this->irq_disabled = 0;
	}

	return count;
}


static ssize_t bl_show(struct class *class, struct class_attribute *attr, char *buf)
{
    u8 val = 0;
    u16 temp = 0;
    char *p = buf;
    ssize_t len = 0;
    pabovXX_t this = abov_sar_ptr;

    for (size_t index = 0; index <= 14; index++) {
        read_register(this, ABOV_CAP_BL0_MSB + index, &val);
        if (0 == (index + 1) % 2) {
            temp |= val;
            buf += sprintf(buf, "ch%d_bl=%d;", index / 2, temp);
            temp = 0;
        } else
            temp |= val << 8;
    }
    
    return (buf - p);
}


static void capsense_update_work(struct work_struct *work)
{
	pabovXX_t this = container_of(work, abovXX_t, fw_update_work);

	LOG_DBG("%s: start update firmware\n", __func__);

	if(!this->board->fw_name)
	{
		LOG_ERR("%s: fw_name read error!\n",__func__);
		return;
	}
	if(this->irq_disabled == 0) {
	    disable_irq(this->irq);
		this->irq_disabled = 1;
    }
	mutex_lock(&this->mutex);
	this->loading_fw = true;
	abov_fw_update(false);
	this->loading_fw = false;
	mutex_unlock(&this->mutex);
	if(this->irq_disabled == 1) {
		enable_irq(this->irq);
		this->irq_disabled = 0;
	}

	if(mEnabled){
		initialize(this);
	}
	LOG_DBG("%s: update firmware end\n", __func__);
}

static void capsense_fore_update_work(struct work_struct *work)
{
	pabovXX_t this = container_of(work, abovXX_t, fw_update_work);

	LOG_DBG("%s: start force update firmware\n", __func__);
	if(this->irq_disabled == 0) {
		disable_irq(this->irq);
		this->irq_disabled = 1;
	}
	mutex_lock(&this->mutex);
	this->loading_fw = true;
	abov_fw_update(true);
	this->loading_fw = false;
	mutex_unlock(&this->mutex);
	if(this->irq_disabled == 1) {
	    enable_irq(this->irq);
		this->irq_disabled = 0;
	}

	if(mEnabled){
		initialize(this);
	}
	LOG_DBG("%s: force update firmware end\n", __func__);
}

#if POWER_ENABLE
static ssize_t power_enable_show(struct class *class, struct class_attribute *attr, char *buf)
{
    pabovXX_t this = abov_sar_ptr;
    return sprintf(buf, "%d\n", this->power_state);
}
static ssize_t power_enable_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    pabovXX_t this = abov_sar_ptr;

    ret = kstrtoint(buf, 10, &this->power_state);
    if (0 != ret) {
        LOG_ERR("kstrtoint failed\n");
    }
    return count;
}
#endif

static ssize_t user_test_show(struct class *class,
                                 struct class_attribute *attr,
                                 char *buf)
{
    pabovXX_t this = abov_sar_ptr;
    return sprintf(buf, "%d\n", this->user_test);
}
static ssize_t user_test_store(struct class *class,
                                  struct class_attribute *attr,
                                  const char *buf, size_t count)
{
    int ret = -1;
    pabovXX_t this = abov_sar_ptr;

    ret = kstrtoint(buf, 10, &this->user_test);
    if (0 != ret) {
        LOG_ERR("kstrtoint failed\n");
    }

    printk("ABOV:this->user_test:%d", this->user_test);
    if(this->user_test){
        if(is_anfr_sar){
	    this->abov_first_boot = false;
            printk("ABOV: this->abov_first_boot = %d;", this->abov_first_boot);
	    printk("ABOV:user_test mode, exit force near mode!!!");
	}
        printk("ABOV ch user_test mode cali ... \n");
        write_register(this, ABOV_RECALI_REG, 0x01);
        msleep(500);
    }
    return count;
}


static struct class_attribute class_attr_enable = __ATTR(enable, 0644, enable_show, enable_store);
static struct class_attribute class_attr_reg = __ATTR(reg, 0644, reg_show, reg_store);                                               
static struct class_attribute class_attr_update_fw = __ATTR(update_fw, 0644, update_fw_show, update_fw_store);
static struct class_attribute class_attr_force_update_fw = __ATTR(force_update_fw, 0644, force_update_fw_show, force_update_fw_store)     ;
static struct class_attribute class_attr_ch0_cap_diff_dump = __ATTR(ch0_cap_diff_dump, 0660, ch0_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch2_cap_diff_dump = __ATTR(ch2_cap_diff_dump, 0660, ch2_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch3_cap_diff_dump = __ATTR(ch3_cap_diff_dump, 0660, ch3_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_int_state = __ATTR(int_state, 0444, int_state_show, NULL);
static struct class_attribute class_attr_bl = __ATTR(bl, 0444, bl_show, NULL);
#if POWER_ENABLE
static struct class_attribute class_attr_power_enable = __ATTR(power_enable, 0664, power_enable_show, power_enable_store);
#endif
static struct class_attribute class_attr_user_test = __ATTR(user_test, 0664, user_test_show, user_test_store);

static struct attribute *abov_capsense_attrs[] = {
	&class_attr_enable.attr,
	&class_attr_reg.attr,
	&class_attr_update_fw.attr,
	&class_attr_force_update_fw.attr,
	&class_attr_ch0_cap_diff_dump.attr,
	&class_attr_ch2_cap_diff_dump.attr,
	&class_attr_ch3_cap_diff_dump.attr,
	&class_attr_int_state.attr,
    &class_attr_bl.attr,
#if POWER_ENABLE
    &class_attr_power_enable.attr,
#endif
    &class_attr_user_test.attr,
	NULL,
};
ATTRIBUTE_GROUPS(abov_capsense); 

static struct class capsense_class = {
	.name           = "capsense",
	.owner          = THIS_MODULE,
	.class_groups   = abov_capsense_groups,
};

/**
 * detect if abov exist or not
 * return 1 if chip exist else return 0
 */
static int abov_detect(struct i2c_client *client)
{
	s32 returnValue = 0, i;
	u8 address = ABOV_VENDOR_ID_REG;
	u8 value = 0xAB;

	if (client) {
		for (i = 0; i < 10; i++) {
			returnValue = i2c_smbus_read_byte_data(client, address);
            LOG_INFO("abov read_register for %d time Addr: 0x%02x Return: 0x%02x\n",
                    i, address, returnValue);
			LOG_DBG("abov read_register for %d time Addr: 0x%02x Return: 0x%02x\n",
					i, address, returnValue);
			if (returnValue >= 0) {
				if (value == returnValue) {
					LOG_INFO("abov detect success!\n");
					return NORMAL_MODE;
				}
			}
			msleep(10);
		}
		LOG_INFO("abov boot detect start\n");
		for (i = 0; i < 3; i++) {
				if(abov_tk_fw_mode_enter(client) == 0) {
					LOG_INFO("abov boot detect success!\n");
					return BOOTLOADER_MODE;
				}
		}
	}
	LOG_INFO("abov detect failed!!!\n");
		return UNKONOW_MODE;
}

static int abov_gpio_init(pabov_platform_data_t pplatData)
{
    int ret = -1;
    if (gpio_is_valid(pplatData->irq_gpio)) {
        ret = gpio_request(pplatData->irq_gpio, "abov_irq_gpio");
        if (ret < 0) {
            LOG_ERR("gpio_request failed. ret=%d\n", ret);
            return ret;
        }
        ret = gpio_direction_input(pplatData->irq_gpio);
        if (ret < 0) {
            LOG_ERR("gpio_direction_input failed. ret=%d\n", ret);
            gpio_free(pplatData->irq_gpio);
            return ret;
        }
    } else {
        LOG_ERR("Invalid gpio num\n");
        return -1;
    }
    return 0;
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
	int ret;
	int index = 1;
	struct input_dev *input_ch0 = NULL;
	struct input_dev *input_ch1 = NULL;
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
	struct input_dev *input_ch2 = NULL;
#endif
#ifdef USE_USB_CHANGE_RECAL
	struct power_supply *psy = NULL;
#endif
#if WT_ADD_SAR_HARDWARE_INFO
    u8 fw_version = 0;
    char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];
#endif
	printk("ABOV abov_probe() start\n");
	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -EIO;

	pplatData = kzalloc(sizeof(struct abov_platform_data), GFP_KERNEL);
	if (!pplatData) {
		LOG_ERR("platform data is required!\n");
		return -EINVAL;
	}
	abov_platform_data_of_init(client, pplatData);
	client->dev.platform_data = pplatData;

    ret = abov_gpio_init(pplatData);
    if (ret) {
        LOG_ERR("abov_gpio_init failed\n");
        return ret;
    }

#if 0
        pplatData->cap_vdd = regulator_get(&client->dev, "cap_vdd");
        if (IS_ERR(pplatData->cap_vdd)) {
                if (PTR_ERR(pplatData->cap_vdd) == -EPROBE_DEFER) {
                        ret = PTR_ERR(pplatData->cap_vdd);
                        goto err_vdd_defer;
                }
                LOG_ERR("%s: Failed to get regulator\n", __func__);
        } else {
                ret = regulator_enable(pplatData->cap_vdd);

                if (ret) {
                        regulator_put(pplatData->cap_vdd);
                        LOG_ERR("%s: Error %d enable regulator\n",
                                         __func__, ret);
                        goto err_vdd_defer;
                }
                pplatData->cap_vdd_en = true;
                LOG_INFO("cap_vdd regulator is %s\n",
                                 regulator_is_enabled(pplatData->cap_vdd) ?
                                 "on" : "off");
        }

	pplatData->cap_svdd = regulator_get(&client->dev, "cap_svdd");
        if (IS_ERR(pplatData->cap_svdd)) {
                if (PTR_ERR(pplatData->cap_svdd) == -EPROBE_DEFER) {
                        ret = PTR_ERR(pplatData->cap_svdd);
                        goto err_svdd_error;
                }
                LOG_ERR("%s: Failed to get regulator\n", __func__);
        } else {
                ret = regulator_enable(pplatData->cap_svdd);

                if (ret) {
                        regulator_put(pplatData->cap_svdd);
                        LOG_ERR("%s: Error %d enable regulator\n",
                                         __func__, ret);
                        goto err_svdd_error;
                }
                pplatData->cap_svdd_en = true;
                LOG_INFO("cap_svdd regulator is %s\n",
                                 regulator_is_enabled(pplatData->cap_svdd) ?
                                 "on" : "off");
        }
#endif
	/* detect if abov exist or not */
	ret = abov_detect(client);
	if (ret == UNKONOW_MODE) {
		LOG_ERR("sensor is invalid\n");
		ret = -ENODEV;
		goto err_svdd_error;
	}
	if (ret == BOOTLOADER_MODE) {
		LOG_INFO("no firmware and enter boot mode\n");
		isForceUpdate = true;
		index = 0;
	}

	this = kzalloc(sizeof(abovXX_t), GFP_KERNEL); /* create memory for main struct */

	if (this) {
		LOG_INFO("\t Initialized Main Memory: 0x%p\n", this);
		this->sar_pinctl = devm_pinctrl_get(&client->dev);
		if (IS_ERR_OR_NULL(this->sar_pinctl)) {
			ret = PTR_ERR(this->sar_pinctl);
			LOG_ERR("abov Target does not use pinctrl %d\n", ret);
			goto err_pDevice;
		}

		this->pinctrl_state_default = pinctrl_lookup_state(this->sar_pinctl, "abov_int_default");
		if (IS_ERR_OR_NULL(this->pinctrl_state_default)) {
			ret = PTR_ERR(this->pinctrl_state_default);
			LOG_ERR("abov Can not lookup %s pinstate %d\n","abov_int_default", ret);
        }
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
		wakeup_source_init(&wakesrc, "abov");
#else
        wake_lock_init(&wakesrc, WAKE_LOCK_SUSPEND, "abov");
#endif
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
		this->statusFunc = touchProcess; /* RELEASE_STAT */

		/* setup i2c communication */
		this->bus = client;
		i2c_set_clientdata(client, this);

		/* record device struct */
		this->pdev = &client->dev;

		/* create memory for device specific struct */
		this->pDevice = pDevice = kzalloc(sizeof(abov_t), GFP_KERNEL);
		abov_sar_ptr = this;
		if (pDevice) {
			LOG_INFO("\t Initialized Device Specific Memory: 0x%p\n", pDevice);
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
			input_ch0 = input_allocate_device();
			if (!input_ch0) {
				ret = -ENOMEM;
				goto err_ch0_alloc;
			}

			/* Set all the keycodes */
			input_set_capability(input_ch0, EV_REL, REL_MISC);
			input_set_capability(input_ch0, EV_REL, REL_MAX);
			__set_bit(EV_REL, input_ch0->evbit);
            input_set_capability(input_ch0, EV_REL, REL_X);
			/* save the input pointer and finish initialization */
			pDevice->pbuttonInformation->input_ch0 = input_ch0;
			input_ch0->name = this->board->cap_ch0_name;
			if (input_register_device(input_ch0)) {
				LOG_ERR("add ch0 cap touch unsuccess\n");
				input_free_device(input_ch0);
				ret = -ENOMEM; 
				goto err_ch0_register;
			}
			/* Create the input device */
			input_ch1 = input_allocate_device();
			if (!input_ch1){
				ret = -ENOMEM;
				goto err_ch1_alloc;
			}
			/* Set all the keycodes */
			input_set_capability(input_ch1, EV_REL, REL_MISC);
			input_set_capability(input_ch1, EV_REL, REL_MAX);
			__set_bit(EV_REL, input_ch1->evbit); 
            input_set_capability(input_ch1, EV_REL, REL_X);
			/* save the input pointer and finish initialization */
			pDevice->pbuttonInformation->input_ch1 = input_ch1;
			/* save the input pointer and finish initialization */
			input_ch1->name = this->board->cap_ch1_name;
			if (input_register_device(input_ch1)) {
				LOG_INFO("add ch1 cap touch unsuccess\n");
				input_free_device(input_ch1);
				ret = -ENOMEM;
				goto err_ch1_register;

			}
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
			/* Create the input device */
			input_ch2 = input_allocate_device();
			if (!input_ch2){
				ret = -ENOMEM;
				goto err_ch2_alloc;
			}
			/* Set all the keycodes */
			input_set_capability(input_ch2, EV_REL, REL_MISC);
			input_set_capability(input_ch2, EV_REL, REL_MAX);
            __set_bit(EV_REL, input_ch2->evbit);
			input_set_capability(input_ch2, EV_REL, REL_X);
			/* save the input pointer and finish initialization */
			pDevice->pbuttonInformation->input_ch2 = input_ch2;
			/* save the input pointer and finish initialization */
			input_ch2->name = this->board->cap_ch2_name;
			if (input_register_device(input_ch2)) {
				LOG_ERR("add ch2 cap touch unsuccess\n");
				input_free_device(input_ch2);
				ret = -ENOMEM;
				goto err_ch2_register;
			}
#endif
		} else {
           ret = -ENOMEM;
		   goto err_pDevice;
		}

		ret = class_register(&capsense_class);
		if (ret < 0) {
			LOG_ERR("Create fsys class failed (%d)\n", ret);
			 goto err_class_register;
		}

#ifdef USE_SENSORS_CLASS
		sensors_capsensor_ch0_cdev.sensors_enable = capsensor_set_enable_ch0;
		sensors_capsensor_ch0_cdev.sensors_poll_delay = NULL;
		sensors_capsensor_ch0_cdev.sensors_flush = abovxx_grip_flush;
		ret = sensors_classdev_register(&input_ch0->dev, &sensors_capsensor_ch0_cdev);
		if (ret < 0)
			LOG_ERR("create ch0 cap sensor_class  file failed (%d)\n", ret);

		sensors_capsensor_ch1_cdev.sensors_enable = capsensor_set_enable_ch1;
		sensors_capsensor_ch1_cdev.sensors_poll_delay = NULL;;
		sensors_capsensor_ch1_cdev.sensors_flush = abovxx_grip_wifi_flush;
		ret = sensors_classdev_register(&input_ch1->dev, &sensors_capsensor_ch1_cdev);
		if (ret < 0)
			LOG_ERR("create ch1 cap sensor_class file failed (%d)\n", ret);

#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
		sensors_capsensor_ch2_cdev.sensors_enable = capsensor_set_enable_ch2;
		sensors_capsensor_ch2_cdev.sensors_poll_delay = NULL;
		sensors_capsensor_ch2_cdev.sensors_flush = abovxx_grip_sub_flush;
		ret = sensors_classdev_register(&input_ch2->dev, &sensors_capsensor_ch2_cdev);
		if (ret < 0)
			LOG_ERR("create ch2 cap sensor_class file failed (%d)\n", ret);
#endif

#endif
              //bug 492320,20191119,gaojingxuan.wt,add,use SS hal
             device_create_file(&input_ch0->dev, &dev_attr_enable);
             device_create_file(&input_ch1->dev, &dev_attr_enable);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
             device_create_file(&input_ch2->dev, &dev_attr_enable);
#endif
#if defined(CONFIG_SENSORS)
		device_create_file(sensors_capsensor_ch0_cdev.dev, &dev_attr_onoff);
#endif

		abovXX_sar_init(this);

		write_register(this, ABOV_CTRL_MODE_REG, ABOV_CTRL_MODE_STOP);
             this->channel_status = 0;
		mEnabled = 0;

        this->abov_first_boot = true;
        this->user_test = 0;
        if(!strncmp(sales_code_from_cmdline, "XAC",SALE_CODE_STR_LENGTH + 1)){
            is_anfr_sar = true;
        }
        printk("ABOV: is_anfr_sar = %d;", is_anfr_sar);

#ifdef USE_USB_CHANGE_RECAL
		INIT_WORK(&this->ps_notify_work, ps_notify_callback_work);
		this->ps_notif.notifier_call = ps_notify_callback;
		ret = power_supply_reg_notifier(&this->ps_notif);
		if (ret) {
			LOG_ERR("Unable to register ps_notifier: %d\n", ret);
			goto free_ps_notifier;
		}

		psy = power_supply_get_by_name("usb");
		if (psy) {
			ret = ps_get_state(psy, &this->ps_is_present);
			if (ret) {
				LOG_ERR("psy get property failed rc=%d\n", ret);
				goto free_ps_notifier;
			}
		}
#endif
#if WT_ADD_SAR_HARDWARE_INFO
		if(index){
	        read_register(this, ABOV_VERSION_REG, &fw_version);
    	    snprintf(firmware_ver,HARDWARE_MAX_ITEM_LONGTH,"A96T376EFB,FW:0x%x",fw_version);
			hardwareinfo_set_prop(HARDWARE_SAR,firmware_ver);
		}
#endif
		this->loading_fw = false;
		if (isForceUpdate == true) {
		    INIT_WORK(&this->fw_update_work, capsense_fore_update_work);
		} else {
			INIT_WORK(&this->fw_update_work, capsense_update_work);
		}
		schedule_work(&this->fw_update_work);

       LOG_INFO("abov_probe() end\n");
		return  0;
	}
	ret = -ENOMEM;
	goto err_svdd_error;

#ifdef USE_USB_CHANGE_RECAL
free_ps_notifier:
    LOG_ERR("%s free ps notifier:.\n", __func__);
	power_supply_unreg_notifier(&this->ps_notif);
#endif

#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
err_class_register:
    LOG_ERR("%s unregister input_ch2 device.\n", __func__);
	input_unregister_device(input_ch2);

err_ch2_register:
err_ch2_alloc:
    LOG_ERR("%s unregister input_ch1 device.\n", __func__);
	input_unregister_device(input_ch1);
#else
err_class_register:
    LOG_ERR("%s unregister input_ch1 device.\n", __func__);
	input_unregister_device(input_ch1);
#endif

err_ch1_register:
err_ch1_alloc:
    LOG_ERR("%s unregister input_ch0 device.\n", __func__);
	input_unregister_device(input_ch0);

err_ch0_register:
err_ch0_alloc:
	LOG_ERR("%s free pDevice.\n", __func__);
	kfree(this->pDevice);

err_pDevice:
	LOG_ERR("%s free device this.\n", __func__);

	kfree(this);

err_svdd_error:
	LOG_ERR("%s svdd defer.\n", __func__);
	regulator_disable(pplatData->cap_vdd);
	regulator_put(pplatData->cap_vdd);

err_vdd_defer:
	LOG_ERR("%s free pplatData.\n", __func__);
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
  		sensors_classdev_unregister(&sensors_capsensor_ch0_cdev);
		sensors_classdev_unregister(&sensors_capsensor_ch1_cdev);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
		sensors_classdev_unregister(&sensors_capsensor_ch2_cdev);
#endif

#endif
		input_unregister_device(pDevice->pbuttonInformation->input_ch0);
		input_unregister_device(pDevice->pbuttonInformation->input_ch1);
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
		input_unregister_device(pDevice->pbuttonInformation->input_ch2);
#endif

		if (this->board->cap_svdd_en) {
			regulator_disable(this->board->cap_svdd);
			regulator_put(this->board->cap_svdd);
		}

		if (this->board->cap_vdd_en) {
			regulator_disable(this->board->cap_vdd);
			regulator_put(this->board->cap_vdd);
		}

		sysfs_remove_group(&client->dev.kobj, &abov_attr_group);
		pplatData = client->dev.platform_data;
		if (pplatData && pplatData->exit_platform_hw)
			pplatData->exit_platform_hw();
		kfree(this->pDevice);
	}
	return abovXX_sar_remove(this);
}

static int abov_suspend(struct device *dev)
{
	pabovXX_t this = dev_get_drvdata(dev);

	LOG_INFO("%s start.\n", __func__);
	if (this) {
		if (this->irq_disabled == 0) {
			enable_irq_wake(this->irq);
		}
		if (mEnabled) {
			write_register(this, ABOV_CTRL_MODE_REG, ABOV_CTRL_MODE_SLEEP);
		} else {
			write_register(this, ABOV_CTRL_MODE_REG, ABOV_CTRL_MODE_STOP);
		}
	}

	LOG_INFO("%s end.\n", __func__);
	return 0;
}

static int abov_resume(struct device *dev)
{
	pabovXX_t this = dev_get_drvdata(dev);

	LOG_INFO("%s start.\n", __func__);
	if (this) {
		if (this->irq_disabled == 0) {
			disable_irq_wake(this->irq);
		}
		if (mEnabled) {
			write_register(this, ABOV_CTRL_MODE_REG, ABOV_CTRL_MODE_ACTIVE);
		} else {
			write_register(this, ABOV_CTRL_MODE_REG, ABOV_CTRL_MODE_STOP);
		}
		//enable_irq(this->irq);
	}

	LOG_INFO("%s end.\n", __func__);
	return 0;
}
static SIMPLE_DEV_PM_OPS(abov_pm_ops, abov_suspend, abov_resume);


#ifdef CONFIG_OF
static const struct of_device_id abov_match_tbl[] = {
	{ .compatible = "abov,abov_sar_mmi_overlay" },
	{ },
};
MODULE_DEVICE_TABLE(of, abov_match_tbl);
#endif

static struct i2c_device_id abov_idtable[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, abov_idtable);

static struct i2c_driver abov_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = DRIVER_NAME,
		.of_match_table = of_match_ptr(abov_match_tbl),
		.pm     = &abov_pm_ops,
	},
	.id_table = abov_idtable,
	.probe	  = abov_probe,
	.remove	  = abov_remove,
};
static int __init abov_init(void)
{
	printk("ABOV abov init 3\n");
	return i2c_add_driver(&abov_driver);
}
static void __exit abov_exit(void)
{
	i2c_del_driver(&abov_driver);
}

//ExtB P210323-06346,jiaoyuxin.wt,add,20210407,SAR FW first update failed
late_initcall(abov_init);
module_exit(abov_exit);

static void abovXX_process_interrupt(pabovXX_t this, u8 nirqlow)
{
	if (!this) {
		printk("abovXX_worker_func, NULL abovXX_t\n");
		return;
	}
	/* since we are not in an interrupt don't need to disable irq. */
	this->statusFunc(this);
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
			printk("abovXX_worker_func, NULL abovXX_t\n");
			return;
		}
		if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio)) {
			/* only run if nirq is high */
			abovXX_process_interrupt(this, 0);
		}
	} else {
		printk("abovXX_worker_func, NULL work_struct\n");
	}
}

#if 0
static irqreturn_t abovXX_interrupt_thread(int irq, void *data)
{
	pabovXX_t this = 0;
	this = data;

	mutex_lock(&this->mutex);
	LOG_DBG("abovXX_irq\n");
	this->int_state = 1;
	if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio))
		abovXX_process_interrupt(this, 1);
	else
		LOG_ERR("abovXX_irq - nirq read high\n");
	mutex_unlock(&this->mutex);
	__pm_wakeup_event(&wakesrc, TEMPORARY_HOLD_TIME);
	return IRQ_HANDLED;
}
#endif

static irqreturn_t abovXX_interrupt_handle(int irq, void *data)
{
	pabovXX_t this = 0;
	this = data;

	printk("ABOV abovXX_interrupt_handle 2\n");
	this->int_state = 1;

	schedule_delayed_work(&this->dworker, 0);

//#ifdef CONFIG_PM_WAKELOCKS
#if 0
    __pm_wakeup_event(&wakesrc, TEMPORARY_HOLD_TIME);
#else
	wake_lock_timeout(&wakesrc, TEMPORARY_HOLD_TIME);
#endif

	return IRQ_HANDLED;
}


int abovXX_sar_init(pabovXX_t this)
{
	int err = 0;
       printk("ABOV abov star \n");
	if (this && this->pDevice) {

		/* initialize worker function */
		INIT_DELAYED_WORK(&this->dworker, abovXX_worker_func);

		/* initialize mutex */
		mutex_init(&this->mutex);
		/* initailize interrupt reporting */
		this->irq_disabled = 0;
#if 0
		err = request_threaded_irq(this->irq, NULL, abovXX_interrupt_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, this->pdev->driver->name,
				this);
#endif
		err = request_irq(this->irq, abovXX_interrupt_handle, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		this->pdev->driver->name, this);
		if (err) {
			LOG_ERR("irq %d busy?\n", this->irq);
			return err;
		}
		this->int_state = 0;
		LOG_INFO("registered with irq (%d)\n", this->irq);
		/* call init function pointer (this should initialize all registers */
		if (this->init)
			return this->init(this);
		LOG_ERR("No init function!!!!\n");
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

MODULE_AUTHOR("ABOV Corp.");
MODULE_DESCRIPTION("ABOV Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
