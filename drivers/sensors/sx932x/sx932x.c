/*! \file sx932x.c
 * \brief  SX932x Driver
 *
 * Driver for the SX932x
 * Copyright (c) 2011 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#define DEBUG
#define DRIVER_NAME "sx932x"

#define MAX_WRITE_ARRAY_SIZE 32

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
//#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/sort.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#if defined(CONFIG_SENSORS)
#include <linux/sensor/sensors_core.h>
#endif

#include "sx932x.h"     /* main struct, interrupt,init,pointers */

#if defined(CONFIG_SENSORS)
#define VENDOR_NAME      "SEMTECH"
#define MODEL_NAME       "SX9325"
#define MODULE_NAME      "grip_sensor"
#define SUB_MODULE_NAME  "grip_sensor_sub"
#endif

psx93XX_t this_enable = NULL;

#define IDLE            0
#define ACTIVE            1

#define SX932x_NIRQ        34

#define MAIN_SENSOR        1 //CS1

/* Failer Index */
#define SX932x_ID_ERROR     1
#define SX932x_NIRQ_ERROR    2
#define SX932x_CONN_ERROR    3
#define SX932x_I2C_ERROR    4

#define PROXOFFSET_LOW            1500

#define SX932x_ANALOG_GAIN        1
#define SX932x_DIGITAL_GAIN        1
#define SX932x_ANALOG_RANGE        2.65

#define    TOUCH_CHECK_REF_AMB      0 // 44523
#define    TOUCH_CHECK_SLOPE        0 // 50
#define    TOUCH_CHECK_MAIN_AMB     0 // 151282

extern char *sar_name;

static bool mEnabled = 1;

#if defined(CONFIG_SENSORS)
static void touchProcess(psx93XX_t this);
#endif

/*! \struct sx932x
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct sx932x
{
    pbuttonInformation_t pbuttonInformation;
    psx932x_platform_data_t hw;        /* specific platform data settings */
} sx932x_t, *psx932x_t;

static int irq_gpio_num;

/*! \fn static int write_register(psx93XX_t this, u8 address, u8 value)
 * \brief Sends a write register to the device
 * \param this Pointer to main parent struct
 * \param address 8-bit register address
 * \param value   8-bit register value to write to address
 * \return Value from i2c_master_send
 */
static int write_register(psx93XX_t this, u8 address, u8 value)
{
    struct i2c_client *i2c = 0;
    char buffer[2];
    int returnValue = 0;

    buffer[0] = address;
    buffer[1] = value;
    returnValue = -ENOMEM;

    if (this && this->bus) {
        i2c = this->bus;
        returnValue = i2c_master_send(i2c,buffer,2);
        #ifdef DEBUG
        dev_info(&i2c->dev,"write_register Address: 0x%x Value: 0x%x Return: %d\n",
                                                        address,value,returnValue);
        #endif
    }
    return returnValue;
}

/*! \fn static int read_register(psx93XX_t this, u8 address, u8 *value)
* \brief Reads a register's value from the device
* \param this Pointer to main parent struct
* \param address 8-Bit address to read from
* \param value Pointer to 8-bit value to save register value to
* \return Value from i2c_smbus_read_byte_data if < 0. else 0
*/
static int read_register(psx93XX_t this, u8 address, u8 *value)
{
    struct i2c_client *i2c = 0;
    s32 returnValue = 0;

    if (this && value && this->bus) {
        i2c = this->bus;
        returnValue = i2c_smbus_read_byte_data(i2c,address);

        #ifdef DEBUG
        dev_info(&i2c->dev, "read_register Address: 0x%x Return: 0x%x\n",
                                                        address,returnValue);
        #endif

        if (returnValue >= 0) {
            *value = returnValue;
            return 0;
        }
        else {
            return returnValue;
        }
    }
    return -ENOMEM;
}

//static int sx932x_set_mode(psx93XX_t this, unsigned char mode);

/*! \fn static int read_regStat(psx93XX_t this)
 * \brief Shortcut to read what caused interrupt.
 * \details This is to keep the drivers a unified
 * function that will read whatever register(s)
 * provide information on why the interrupt was caused.
 * \param this Pointer to main parent struct
 * \return If successful, Value of bit(s) that cause interrupt, else 0
 */
static int read_regStat(psx93XX_t this)
{
    u8 data = 0;
    if (this) {
        if (read_register(this,SX932x_IRQSTAT_REG,&data) == 0)
        return (data & 0x00FF);
    }
    return 0;
}

/*********************************************************************/
/*! \brief Perform a manual offset calibration
* \param this Pointer to main parent struct
* \return Value return value from the write register
 */
static int manual_offset_calibration(psx93XX_t this)
{
    s32 ret = 0;

    ret |= write_register(this, SX932x_CTRL1_REG, 0x20);
    if (ret < 0) {
        dev_err(this->pdev,"write_register error!\n");
        return ret;
    }
    msleep(100);
    ret |= write_register(this, SX932x_CTRL1_REG, 0x27);
    if (ret < 0) {
        dev_err(this->pdev,"write_register error!\n");
        return ret;
    }
    return ret;
}
/*! \brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static ssize_t manual_offset_calibration_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    u8 reg_value = 0;
    psx93XX_t this = dev_get_drvdata(dev);

    dev_info(this->pdev, "Reading IRQSTAT_REG\n");
    read_register(this,SX932x_IRQSTAT_REG,&reg_value);
    return sprintf(buf, "%d\n", reg_value);
}

/*! \brief sysfs store function for manual calibration
 */
static ssize_t manual_offset_calibration_store(struct device *dev,
            struct device_attribute *attr,const char *buf, size_t count)
{
    psx93XX_t this = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val))
    return -EINVAL;
    if (val) {
        dev_info( this->pdev, "Performing manual_offset_calibration()\n");
        manual_offset_calibration(this);
    }
    return count;
}

static int sx932x_Hardware_Check(psx93XX_t this)
{
    int ret;
    u8 failcode = 0;
    u8 loop = 0;
    this->failStatusCode = 0;

    //Check th IRQ Status
    while(this->get_nirq_low && this->get_nirq_low()){
        read_regStat(this);
        msleep(100);
        if(++loop >10){
            this->failStatusCode = SX932x_NIRQ_ERROR;
            break;
        }
    }

    //Check I2C Connection
    ret = read_register(this, SX932x_WHOAMI_REG, &failcode);
    if(ret < 0){
        this->failStatusCode = SX932x_I2C_ERROR;
    }

    if(failcode!= SX932x_WHOAMI_VALUE){
        this->failStatusCode = SX932x_ID_ERROR;
    }

    dev_info(this->pdev, "sx932x failcode = 0x%x\n",this->failStatusCode);
    return (int)this->failStatusCode;
}


/*********************************************************************/
static int sx932x_global_variable_init(psx93XX_t this)
{
    this->irq_disabled = 0;
    this->failStatusCode = 0;
    this->reg_in_dts = true;
    return 0;
}

static ssize_t sx932x_register_write_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t count)
{
    int reg_address = 0, val = 0;
    psx93XX_t this = dev_get_drvdata(dev);

    if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
        pr_err("[SX932x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }

    write_register(this, (unsigned char)reg_address, (unsigned char)val);
    pr_info("[SX932x]: %s - Register(0x%x) data(0x%x)\n",__func__, reg_address, val);

    return count;
}
//read registers not include the advanced one
static ssize_t sx932x_register_read_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val=0;
    int regist = 0;
    psx93XX_t this = dev_get_drvdata(dev);

    dev_info(this->pdev, "Reading register\n");

    if (sscanf(buf, "%x", &regist) != 1) {
        pr_err("[SX932x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }

    read_register(this, regist, &val);
    pr_info("[SX932x]: %s - Register(0x%2x) data(0x%2x)\n",__func__, regist, val);

    return count;
}

static void read_rawData(psx93XX_t this)
{
    u8 msb=0, lsb=0;
    u8 csx;
    s32 useful;
    s32 average;
    s32 diff;
    u16 offset;
    if(this){
        for(csx =0; csx<4; csx++){
            write_register(this,SX932x_CPSRD,csx);//here to check the CS1, also can read other channel
            read_register(this,SX932x_USEMSB,&msb);
            read_register(this,SX932x_USELSB,&lsb);
            useful = (s32)((msb << 8) | lsb);

            read_register(this,SX932x_AVGMSB,&msb);
            read_register(this,SX932x_AVGLSB,&lsb);
            average = (s32)((msb << 8) | lsb);

            read_register(this,SX932x_DIFFMSB,&msb);
            read_register(this,SX932x_DIFFLSB,&lsb);
            diff = (s32)((msb << 8) | lsb);

            read_register(this,SX932x_OFFSETMSB,&msb);
            read_register(this,SX932x_OFFSETLSB,&lsb);
            offset = (u16)((msb << 8) | lsb);
            if (useful > 32767)
                useful -= 65536;
            if (average > 32767)
                average -= 65536;
            if (diff > 32767)
                diff -= 65536;
            dev_info(this->pdev, " [CS: %d] Useful = %d Average = %d, DIFF = %d Offset = %d \n",csx,useful,average,diff,offset);
        }
    }
}

static ssize_t sx932x_raw_data_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    psx93XX_t this = dev_get_drvdata(dev);
    read_rawData(this);
    return 0;
}

#ifndef CONFIG_SENSORS
static ssize_t enable_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", mEnabled);
}

static ssize_t enable_store(struct device *dev,
            struct device_attribute *attr,const char *buf, size_t count)
{
    psx93XX_t this = dev_get_drvdata(dev);
    psx932x_t pDevice = NULL;
    struct input_dev *input_bottom_sar = NULL;
    struct input_dev *input_top_sar = NULL;

    if (this && (pDevice = this->pDevice))
    {
        input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
        input_top_sar = pDevice->pbuttonInformation->input_top_sar;

        if (!strncmp(buf, "1", 1)) {
            dev_info(this->pdev, "enable sx9328\n");
            enable_irq(this->irq);
            enable_irq_wake(this->irq);
            write_register(this, SX932x_CTRL1_REG, 0x27);
            mEnabled = 1;
        } else if (!strncmp(buf, "0", 1)) {
            dev_info(this->pdev, "disable sx9328\n");
            disable_irq(this->irq);
            disable_irq_wake(this->irq);
            write_register(this, SX932x_CTRL1_REG, 0x07);//make sx932x in Pause mode
            #ifdef HQ_FACTORY_BUILD
            input_report_key(input_bottom_sar, KEY_SAR2_FAR, 1);
            input_report_key(input_bottom_sar, KEY_SAR2_FAR, 0);
            input_report_key(input_top_sar, KEY_SAR_FAR, 1);
            input_report_key(input_top_sar, KEY_SAR_FAR, 0);
            #else
            input_report_abs(input_bottom_sar, ABS_DISTANCE, 5);
            input_report_abs(input_top_sar, ABS_DISTANCE, 5);
            #endif
            input_sync(input_bottom_sar);
            input_sync(input_top_sar);
            mEnabled = 0;
        } else {
            dev_info(this->pdev, "unknown enable symbol\n");
        }
    }
    return count;
}
#endif

static DEVICE_ATTR(manual_calibrate, 0664, manual_offset_calibration_show,manual_offset_calibration_store);
static DEVICE_ATTR(register_write,  0664, NULL,sx932x_register_write_store);
static DEVICE_ATTR(register_read,0664, NULL,sx932x_register_read_store);
static DEVICE_ATTR(raw_data,0664,sx932x_raw_data_show,NULL);
#ifndef CONFIG_SENSORS
static DEVICE_ATTR(enable, 0664, enable_show,enable_store);
#endif
static struct attribute *sx932x_attributes[] = {
    &dev_attr_manual_calibrate.attr,
    &dev_attr_register_write.attr,
    &dev_attr_register_read.attr,
    &dev_attr_raw_data.attr,
#ifndef CONFIG_SENSORS
    &dev_attr_enable.attr,
#endif
    NULL,
};
static struct attribute_group sx932x_attr_group = {
    .attrs = sx932x_attributes,
};

#if defined(CONFIG_SENSORS)
static int enable_bottom(psx93XX_t this, unsigned int enable)
{
    psx932x_t pDevice = NULL;
    struct input_dev *input_bottom_sar = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u8 i = 0;

    pDevice = this->pDevice;
    input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[2];

    if (enable == 1) {
        if(this->channel_status == 0) {
            if(this->irq_disabled == 1){
                enable_irq(this->irq);
                this->irq_disabled = 0;
            }
            enable_irq_wake(this->irq);
            write_register(this,SX932x_CTRL1_REG,0x27);
            mEnabled = 1;
        }

        if(!(this->channel_status & 0x01)) {
            this->channel_status |= 0x01; //bit0 for ch0 status
        }
        pr_info("this->channel_status = 0x%x \n",this->channel_status);

        if (this->skip_data == true) {
            pr_info("%s - skip grip event\n", __func__);
        } else {
            read_register(this, SX932x_STAT0_REG, &i);
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask)) {
                input_report_rel(input_bottom_sar, REL_MISC, 1);
                input_sync(input_bottom_sar);
                pCurrentButton->state = ACTIVE;
            } else {
                input_report_rel(input_bottom_sar, REL_MISC, 2);
                input_sync(input_bottom_sar);
                pCurrentButton->state = IDLE;
            }
        }

        touchProcess(this);
    } else if (enable == 0) {
        if(this->channel_status & 0x01) {
            this->channel_status &= ~0x01;
        }
        pr_info("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            if(this->irq_disabled == 0){
                disable_irq(this->irq);
                this->irq_disabled = 1;
            }
            disable_irq_wake(this->irq);
            write_register(this,SX932x_CTRL1_REG, 0x07);
            mEnabled = 0;
        }
    } else {
        pr_err("unknown enable symbol\n");
    }

    return 0;
}

static int enable_top(psx93XX_t this, unsigned int enable)
{
    psx932x_t pDevice = NULL;
    struct input_dev *input_top_sar = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u8 i = 0;

    pDevice = this->pDevice;
    input_top_sar = pDevice->pbuttonInformation->input_top_sar;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[1];

    if (enable == 1) {
        if(this->channel_status == 0) {
            if(this->irq_disabled == 1){
                enable_irq(this->irq);
                this->irq_disabled = 0;
            }
            enable_irq_wake(this->irq);
            write_register(this,SX932x_CTRL1_REG,0x27);
            mEnabled = 1;
        }

        if(!(this->channel_status & 0x02)) {
            this->channel_status |= 0x02; //bit1 for ch1 status
        }
        pr_info("this->channel_status = 0x%x \n",this->channel_status);

        if (this->skip_data == true) {
            pr_info("%s - skip grip event\n", __func__);
        } else {
            read_register(this, SX932x_STAT0_REG, &i);
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask)) {
                input_report_rel(input_top_sar, REL_MISC, 1);
                input_sync(input_top_sar);
                pCurrentButton->state = ACTIVE;
            } else {
                input_report_rel(input_top_sar, REL_MISC, 2);
                input_sync(input_top_sar);
                pCurrentButton->state = IDLE;
            }
        }

        touchProcess(this);
    } else if (enable == 0) {
        if(this->channel_status & 0x02) {
            this->channel_status &= ~0x02;
        }
        pr_info("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            if(this->irq_disabled == 0){
                disable_irq(this->irq);
                this->irq_disabled = 1;
            }
            disable_irq_wake(this->irq);
            write_register(this,SX932x_CTRL1_REG, 0x07);
            mEnabled = 0;
        }
    } else {
        pr_err("unknown enable symbol\n");
    }

    return 0;
}

static ssize_t sx932x_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    psx93XX_t this = dev_get_drvdata(dev);
    int status = -1;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    pr_info("%s: dev->name:%s\n", __func__, temp_input_dev->name); 

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if(this->channel_status & 0x01) {
            status = 1;    
        } else {
            status = 0;
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if(this->channel_status & 0x02) {
            status = 1;    
        } else {
            status = 0;
        }
    }

    return snprintf(buf, 8, "%d\n", status);
}

static ssize_t sx932x_enable_store (struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    psx93XX_t this = dev_get_drvdata(dev);
    u8 enable;
    int ret;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    ret = kstrtou8(buf, 2, &enable);
    if (ret) {
        pr_err("%s - Invalid Argument\n", __func__);
        return ret;
    }

    pr_info("%s: dev->name:%s new_value = %u\n", __func__, temp_input_dev->name, enable); 

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if (enable == 1) {
            enable_bottom(this, 1);
        } else {
            enable_bottom(this, 0);
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if (enable == 1) {
            enable_top(this, 1);
        } else {
            enable_top(this, 0);
        }
    }

    return count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
        sx932x_enable_show, sx932x_enable_store);

static struct attribute *ss_sx932x_attributes[] = {
    &dev_attr_enable.attr,
    NULL,
};

static struct attribute_group ss_sx932x_attributes_group = {
    .attrs = ss_sx932x_attributes
};

static ssize_t sx932x_vendor_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx932x_name_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t sx932x_onoff_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    psx93XX_t this = dev_get_drvdata(dev);

    return snprintf(buf, PAGE_SIZE, "%u\n", !this->skip_data);
}

static ssize_t sx932x_onoff_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val;
    int ret;
    psx93XX_t this = dev_get_drvdata(dev);
    psx932x_t pDevice = NULL;
    struct input_dev *input_bottom_sar = NULL;
    struct input_dev *input_top_sar = NULL;

    pDevice = this->pDevice;
    input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
    input_top_sar = pDevice->pbuttonInformation->input_top_sar;

    ret = kstrtou8(buf, 2, &val);
    if (ret) {
        pr_err("[SX932x]: %s - Invalid Argument\n", __func__);
        return ret;
    }

    if (val == 0) {
        this->skip_data = true;
        if (mEnabled) {
            input_report_rel(input_bottom_sar, REL_MISC, 2);
            input_report_rel(input_top_sar, REL_MISC, 2);
            input_sync(input_bottom_sar);
            input_sync(input_top_sar);
        }
    } else {
        this->skip_data = false;
    }

    pr_info("[SX932x]: %s -%u\n", __func__, val);
    return count;
}

static ssize_t sx932x_grip_flush_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val = 0;
    int ret = 0;
    psx93XX_t this = dev_get_drvdata(dev);
    psx932x_t pDevice = NULL;
    struct input_dev *input_bottom_sar = NULL;
    struct input_dev *input_top_sar = NULL;

    pDevice = this->pDevice;
    input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
    input_top_sar = pDevice->pbuttonInformation->input_top_sar;

    ret = kstrtou8(buf, 10, &val);
    if (ret < 0) {
        pr_err("%s: Invalid value of input, input=%ld\n", __func__, ret);
        return -EINVAL;
    }

    if(!strncmp(dev->kobj.name,"grip_sensor",sizeof("grip_sensor"))) {
        input_report_rel(input_bottom_sar, REL_MAX, val);
        input_sync(input_bottom_sar);
    } else if(!strncmp(dev->kobj.name,"grip_sensor_sub",sizeof("grip_sensor_sub"))) {
        input_report_rel(input_top_sar, REL_MAX, val);
        input_sync(input_top_sar);
    }
    else {
        pr_err("%s: unknown type\n", __func__);
    }   

    pr_info("%s ret=%u\n", __func__, val);
    return count;
}

static DEVICE_ATTR(name, S_IRUGO, sx932x_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sx932x_vendor_show, NULL);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
        sx932x_onoff_show, sx932x_onoff_store);
static DEVICE_ATTR(grip_flush, 0220, NULL, sx932x_grip_flush_store);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_name,
    &dev_attr_vendor,
    &dev_attr_onoff,
    &dev_attr_grip_flush,
    NULL,
};

static struct device_attribute *sub_sensor_attrs[] = {
    &dev_attr_name,
    &dev_attr_vendor,
    &dev_attr_grip_flush,
    NULL,
};
#endif

static int sx932x_charger_notifier_callback(struct notifier_block *nb,
                                unsigned long val, void *v) {
    int ret = 0;
    struct power_supply *psy = NULL;
    union power_supply_propval prop;

    dev_info(this_enable->pdev, "sar_charger_notifier_callback\n");
    psy= power_supply_get_by_name("usb");
    if (!psy) {
        dev_info(this_enable->pdev, "Couldn't get usbpsy\n");
        return -EINVAL;
    }

    dev_info(this_enable->pdev, "sar_charger_notifier_callback val = %ld\n", val);
    if (!strcmp(psy->desc->name, "usb")) {
        if (psy && val == POWER_SUPPLY_PROP_STATUS) {
            ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &prop);

            dev_info(this_enable->pdev, "power_supply_get_property prop.intval = %d\n", prop.intval);

            if (ret < 0) {
                dev_info(this_enable->pdev, "Couldn't get POWER_SUPPLY_PROP_ONLINE rc = %d\n", ret);
                return ret;
            } else {
                if(this_enable->usb_plug_status == 2)
                    this_enable->usb_plug_status = prop.intval;

                if(this_enable->usb_plug_status != prop.intval) {
                    dev_info(this_enable->pdev, "usb prop.intval = %d\n", prop.intval);
                    this_enable->usb_plug_status = prop.intval;

                    if(this_enable->charger_notify_wq != NULL)
                        queue_work(this_enable->charger_notify_wq, &this_enable->update_charger);
                }
            }
        }
    }
    return 0;
}

static void sx932x_update_charger(struct work_struct *work)
{
    int ret = 0;

    ret = manual_offset_calibration(this_enable);

    if(ret != 2) {
        dev_info(this_enable->pdev, "manual_offset_calibration failed\n");
    }
}

void sx932x_plat_charger_init(void)
{
    int ret = 0;

    this_enable->usb_plug_status = 2;
    this_enable->charger_notify_wq = create_singlethread_workqueue("sar_charger_wq");

    if (!this_enable->charger_notify_wq) {
        dev_info(this_enable->pdev, "allocate sar_charger_notify_wq failed\n");
        return;
    }

    INIT_WORK(&this_enable->update_charger, sx932x_update_charger);

    this_enable->notifier_charger.notifier_call = sx932x_charger_notifier_callback;
    ret = power_supply_reg_notifier(&this_enable->notifier_charger);

    if (ret < 0)
        dev_info(this_enable->pdev, "power_supply_reg_notifier failed\n");
}

/****************************************************/
/*! \brief  Initialize I2C config from platform data
 * \param this Pointer to main parent struct
 */
static void sx932x_reg_init(psx93XX_t this)
{
    psx932x_t pDevice = 0;
    psx932x_platform_data_t pdata = 0;
    int i = 0;
    /* configure device */
    dev_info(this->pdev, "Going to Setup I2C Registers\n");
    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
    {
        /*******************************************************************************/
        // try to initialize from device tree!
        /*******************************************************************************/
        if (this->reg_in_dts == true) {
            while ( i < pdata->i2c_reg_num) {
                /* Write all registers/values contained in i2c_reg */
                dev_info(this->pdev, "Going to Write Reg from dts: 0x%x Value: 0x%x\n",
                pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
                write_register(this, pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
                i++;
            }
        } else { // use static ones!!
            while ( i < ARRAY_SIZE(sx932x_i2c_reg_setup)) {
                /* Write all registers/values contained in i2c_reg */
                dev_info(this->pdev, "Going to Write Reg: 0x%x Value: 0x%x\n",
                sx932x_i2c_reg_setup[i].reg,sx932x_i2c_reg_setup[i].val);
                write_register(this, sx932x_i2c_reg_setup[i].reg,sx932x_i2c_reg_setup[i].val);
                i++;
            }
        }
    /*******************************************************************************/
    } else {
        dev_err(this->pdev, "ERROR! platform data 0x%p\n",pDevice->hw);
    }

}


/*! \fn static int initialize(psx93XX_t this)
 * \brief Performs all initialization needed to configure the device
 * \param this Pointer to main parent struct
 * \return Last used command's return value (negative if error)
 */
static int initialize(psx93XX_t this)
{
    int ret;
    if (this) {
        pr_info("SX932x income initialize\n");
        /* prepare reset by disabling any irq handling */
        this->irq_disabled = 1;
        disable_irq(this->irq);
        /* perform a reset */
        write_register(this,SX932x_SOFTRESET_REG,SX932x_SOFTRESET);
        /* wait until the reset has finished by monitoring NIRQ */
        dev_info(this->pdev, "Sent Software Reset. Waiting until device is back from reset to continue.\n");
        /* just sleep for awhile instead of using a loop with reading irq status */
        msleep(100);

        ret = sx932x_global_variable_init(this);

        sx932x_reg_init(this);
        msleep(100); /* make sure everything is running */
        manual_offset_calibration(this);

#if defined(CONFIG_SENSORS)
        this->skip_data = false;
#endif

        /* re-enable interrupt handling */
        enable_irq(this->irq);
        enable_irq_wake(this->irq);

        /* make sure no interrupts are pending since enabling irq will only
        * work on next falling edge */
        read_regStat(this);
        return 0;
    }
    return -ENOMEM;
}

/*!
 * \brief Handle what to do when a touch occurs
 * \param this Pointer to main parent struct
 */
static void touchProcess(psx93XX_t this)
{
    int counter = 0;
    u8 i = 0;
    int numberOfButtons = 0;
    psx932x_t pDevice = NULL;
    struct _buttonInfo *buttons = NULL;
    struct input_dev *input_top_sar = NULL;
    struct input_dev *input_bottom_sar = NULL;

    struct _buttonInfo *pCurrentButton  = NULL;

    if (this && (pDevice = this->pDevice))
    {
        //dev_info(this->pdev, "Inside touchProcess()\n");
        read_register(this, SX932x_STAT0_REG, &i);

        buttons = pDevice->pbuttonInformation->buttons;
        input_top_sar = pDevice->pbuttonInformation->input_top_sar;
        input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
        numberOfButtons = pDevice->pbuttonInformation->buttonSize;

        if (unlikely( (buttons==NULL) || (input_top_sar==NULL) || (input_bottom_sar==NULL))) {
            dev_err(this->pdev, "ERROR!! buttons or input NULL!!!\n");
            return;
        }

#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            dev_info(this->pdev, "%s - skip grip event\n", __func__);
            return;
        }
#endif

        for (counter = 0; counter < numberOfButtons; counter++) {
            pCurrentButton = &buttons[counter];
            if (pCurrentButton==NULL) {
                dev_err(this->pdev,"ERROR!! current button at index: %d NULL!!!\n", counter);
                return; // ERRORR!!!!
            }
            switch (pCurrentButton->state) {
                case IDLE: /* Button is not being touched! */
                    if (((i & pCurrentButton->mask) == pCurrentButton->mask)) {
                        /* User pressed button */
                        dev_info(this->pdev, "cap button %d touched\n", counter);
                        if (counter == 1) {
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input_top_sar, KEY_SAR_CLOSE, 1);
                            input_report_key(input_top_sar, KEY_SAR_CLOSE, 0);
                            #else
                            input_report_rel(input_top_sar, REL_MISC, 1);
                            #endif
                            input_sync(input_top_sar);
                        } else if (counter == 2)
                        {
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 1);
                            input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 0);
                            #else
                            input_report_rel(input_bottom_sar, REL_MISC, 1);
                            #endif
                            input_sync(input_bottom_sar);
                        }
                        pCurrentButton->state = ACTIVE;
                    } else {
                        dev_info(this->pdev, "Button %d already released.\n",counter);
                    }
                    break;
                case ACTIVE: /* Button is being touched! */
                    if (((i & pCurrentButton->mask) != pCurrentButton->mask)) {
                        /* User released button */
                        dev_info(this->pdev, "cap button %d released\n",counter);
                        if (counter == 1) {
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input_top_sar, KEY_SAR_FAR, 1);
                            input_report_key(input_top_sar, KEY_SAR_FAR, 0);
                            #else
                            input_report_rel(input_top_sar, REL_MISC, 2);
                            #endif
                            input_sync(input_top_sar);
                        } else if (counter == 2)
                        {
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input_bottom_sar, KEY_SAR2_FAR, 1);
                            input_report_key(input_bottom_sar, KEY_SAR2_FAR, 0);
                            #else
                            input_report_rel(input_bottom_sar, REL_MISC, 2);
                            #endif
                            input_sync(input_bottom_sar);
                        }
                        pCurrentButton->state = IDLE;
                    } else {
                        dev_info(this->pdev, "Button %d still touched.\n",counter);
                    }
                    break;
                default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
                    break;
            };
        }

   //dev_info(this->pdev, "Leaving touchProcess()\n");
  }
}

static int sx932x_parse_dt(struct sx932x_platform_data *pdata, struct device *dev)
{
    struct device_node *dNode = dev->of_node;
    enum of_gpio_flags flags;
//    int ret;
    if (dNode == NULL)
        return -ENODEV;

    pdata->irq_gpio= of_get_named_gpio_flags(dNode,
                                            "Semtech,nirq-gpio", 0, &flags);
    irq_gpio_num = pdata->irq_gpio;
    if (pdata->irq_gpio < 0) {
        pr_err("[SENSOR]: %s - get irq_gpio error\n", __func__);
        return -ENODEV;
    }

    /***********************************************************************/
    // load in registers from device tree
    of_property_read_u32(dNode,"Semtech,reg-num",&pdata->i2c_reg_num);
    // layout is register, value, register, value....
    // if an extra item is after just ignore it. reading the array in will cause it to fail anyway
    pr_info("[SX932x]:%s -  size of elements %d \n", __func__,pdata->i2c_reg_num);
    if (pdata->i2c_reg_num > 0) {
         // initialize platform reg data array
         pdata->pi2c_reg = devm_kzalloc(dev,sizeof(struct smtc_reg_data)*pdata->i2c_reg_num, GFP_KERNEL);
         if (unlikely(pdata->pi2c_reg == NULL)) {
            return -ENOMEM;
        }

     // initialize the array
        if (of_property_read_u8_array(dNode,"Semtech,reg-init",(u8*)&(pdata->pi2c_reg[0]),sizeof(struct smtc_reg_data)*pdata->i2c_reg_num))
        return -ENOMEM;
    }
    /***********************************************************************/
    pr_info("[SX932x]: %s -[%d] parse_dt complete\n", __func__,pdata->irq_gpio);
    return 0;
}

/* get the NIRQ state (1->NIRQ-low, 0->NIRQ-high) */
static int sx932x_init_platform_hw(struct i2c_client *client)
{
    psx93XX_t this = i2c_get_clientdata(client);
    struct sx932x *pDevice = NULL;
    struct sx932x_platform_data *pdata = NULL;

    int rc = 0;

    pr_info("[SX932x] : %s init_platform_hw start!",__func__);

    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw)) {
        if (gpio_is_valid(pdata->irq_gpio)) {
            rc = gpio_request(pdata->irq_gpio, "sx932x_irq_gpio");
            if (rc < 0) {
                dev_err(this->pdev, "SX932x Request gpio. Fail![%d]\n", rc);
                return rc;
            }
            rc = gpio_direction_input(pdata->irq_gpio);
            if (rc < 0) {
                dev_err(this->pdev, "SX932x Set gpio direction. Fail![%d]\n", rc);
                return rc;
            }
            this->irq = client->irq = gpio_to_irq(pdata->irq_gpio);
        }
        else {
            dev_err(this->pdev, "SX932x Invalid irq gpio num.(init)\n");
        }
    }
    else {
        pr_err("[SX932x] : %s - Do not init platform HW", __func__);
    }

    pr_err("[SX932x]: %s - sx932x_irq_debug\n",__func__);
    return rc;
}

static void sx932x_exit_platform_hw(struct i2c_client *client)
{
    psx93XX_t this = i2c_get_clientdata(client);
    struct sx932x *pDevice = NULL;
    struct sx932x_platform_data *pdata = NULL;

    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw)) {
        if (gpio_is_valid(pdata->irq_gpio)) {
            gpio_free(pdata->irq_gpio);
        }
        else {
            dev_err(this->pdev, "Invalid irq gpio num.(exit)\n");
        }
    }
    return;
}

static int sx932x_get_nirq_state(void)
{
    return  !gpio_get_value(irq_gpio_num);
}

/*! \fn static int sx932x_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * \brief Probe function
 * \param client pointer to i2c_client
 * \param id pointer to i2c_device_id
 * \return Whether probe was successful
 */
static int sx932x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i = 0;
    int err = 0;

    psx93XX_t this = 0;
    psx932x_t pDevice = 0;
    psx932x_platform_data_t pplatData = 0;
    struct totalButtonInformation *pButtonInformationData = NULL;
    struct input_dev *input_top_sar = NULL;
    struct input_dev *input_bottom_sar = NULL;
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

    dev_info(&client->dev, "sx932x_probe()\n");

    client->addr = 0x28;
    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)) {
        dev_err(&client->dev, "Check i2c functionality.Fail!\n");
        err = -EIO;
        return err;
    }

    this = devm_kzalloc(&client->dev,sizeof(sx93XX_t), GFP_KERNEL); /* create memory for main struct */
    dev_info(&client->dev, "\t Initialized Main Memory: 0x%p\n",this);

    pButtonInformationData = devm_kzalloc(&client->dev , sizeof(struct totalButtonInformation), GFP_KERNEL);
    if (!pButtonInformationData) {
        dev_err(&client->dev, "Failed to allocate memory(totalButtonInformation)\n");
        err = -ENOMEM;
        return err;
    }

    pButtonInformationData->buttonSize = ARRAY_SIZE(psmtcButtons);
    pButtonInformationData->buttons =  psmtcButtons;
    pplatData = devm_kzalloc(&client->dev,sizeof(struct sx932x_platform_data), GFP_KERNEL);
    if (!pplatData) {
        dev_err(&client->dev, "platform data is required!\n");
        return -EINVAL;
    }
    pplatData->get_is_nirq_low = sx932x_get_nirq_state;
    pplatData->pbuttonInformation = pButtonInformationData;

    client->dev.platform_data = pplatData;
    err = sx932x_parse_dt(pplatData, &client->dev);
    if (err) {
        dev_err(&client->dev, "could not setup pin\n");
        return ENODEV;
    }

    pplatData->init_platform_hw = sx932x_init_platform_hw;
    dev_err(&client->dev, "SX932x init_platform_hw done!\n");

    if (this){
        dev_info(&client->dev, "SX932x initialize start!!");
        /* In case we need to reinitialize data
        * (e.q. if suspend reset device) */
        this->init = initialize;
        /* shortcut to read status of interrupt */
        this->refreshStatus = read_regStat;
        /* pointer to function from platform data to get pendown
        * (1->NIRQ=0, 0->NIRQ=1) */
        this->get_nirq_low = pplatData->get_is_nirq_low;
        /* save irq in case we need to reference it */
        this->irq = client->irq;
        /* do we need to create an irq timer after interrupt ? */
        this->useIrqTimer = 0;

        /* Setup function to call on corresponding reg irq source bit */
        if (MAX_NUM_STATUS_BITS>= 8)
        {
            this->statusFunc[0] = 0; /* TXEN_STAT */
            this->statusFunc[1] = 0; /* UNUSED */
            this->statusFunc[2] = 0; /* UNUSED */
            this->statusFunc[3] = read_rawData; /* CONV_STAT */
            this->statusFunc[4] = touchProcess; /* COMP_STAT */
            this->statusFunc[5] = touchProcess; /* RELEASE_STAT */
            this->statusFunc[6] = touchProcess; /* TOUCH_STAT  */
            this->statusFunc[7] = 0; /* RESET_STAT */
        }

        /* setup i2c communication */
        this->bus = client;
        i2c_set_clientdata(client, this);

        err = sx932x_Hardware_Check(this);
        if (err > 0)
        {
            dev_err(&client->dev, "sx932x whoami check fail, return %d!\n", err);
            return err;
        }
        sar_name = "sx9328";

        /* record device struct */
        this->pdev = &client->dev;

        /* create memory for device specific struct */
        this->pDevice = pDevice = devm_kzalloc(&client->dev,sizeof(sx932x_t), GFP_KERNEL);
        dev_info(&client->dev, "\t Initialized Device Specific Memory: 0x%p\n",pDevice);

        if (pDevice){
            /* for accessing items in user data (e.g. calibrate) */
            err = sysfs_create_group(&client->dev.kobj, &sx932x_attr_group);
            //sysfs_create_group(client, &sx932x_attr_group);

            /* Add Pointer to main platform data struct */
            pDevice->hw = pplatData;

            /* Check if we hava a platform initialization function to call*/
            if (pplatData->init_platform_hw)
            pplatData->init_platform_hw(client);

            /* Initialize the button information initialized with keycodes */
            pDevice->pbuttonInformation = pplatData->pbuttonInformation;

            /* Create the input top device */
            input_top_sar = input_allocate_device();
            if (!input_top_sar) {
                return -ENOMEM;
            }
            /* Set all the keycodes */

            #ifdef HQ_FACTORY_BUILD
            __set_bit(EV_KEY, input_top_sar->evbit);
            __set_bit(KEY_SAR_FAR, input_top_sar->keybit);
            __set_bit(KEY_SAR_CLOSE, input_top_sar->keybit);
            #else
            input_set_capability(input_top_sar, EV_REL, REL_MISC);
            input_set_capability(input_top_sar, EV_REL, REL_MAX);
            #endif
            /* save the input pointer and finish initialization */
            pButtonInformationData->input_top_sar = input_top_sar;
            input_top_sar->name = "grip_sensor_sub";
            input_top_sar->id.bustype = BUS_I2C;
            if(input_register_device(input_top_sar)){
                return -ENOMEM;
            }
            input_set_drvdata(input_top_sar, this);

            /* Create the input bottom device */
            input_bottom_sar = input_allocate_device();
            if (!input_bottom_sar) {
                return -ENOMEM;
            }
            /* Set all the keycodes */

            #ifdef HQ_FACTORY_BUILD
            __set_bit(EV_KEY, input_bottom_sar->evbit);
            __set_bit(KEY_SAR2_FAR, input_bottom_sar->keybit);
            __set_bit(KEY_SAR2_CLOSE, input_bottom_sar->keybit);
            #else
            input_set_capability(input_bottom_sar, EV_REL, REL_MISC);
            input_set_capability(input_bottom_sar, EV_REL, REL_MAX);
            #endif

            /* save the input pointer and finish initialization */
            pButtonInformationData->input_bottom_sar = input_bottom_sar;
            input_bottom_sar->name = "grip_sensor";
            input_bottom_sar->id.bustype = BUS_I2C;
            if(input_register_device(input_bottom_sar)){
                return -ENOMEM;
            }
            input_set_drvdata(input_bottom_sar, this);

            for (i = 0; i < pButtonInformationData->buttonSize; i++) {
                pButtonInformationData->buttons[i].state = IDLE;
            }
        }

#if defined(CONFIG_SENSORS)
        this->channel_status = 0;
        err = sensors_register(&pplatData->factory_device, this, sensor_attrs, MODULE_NAME);
        if (err) {
            dev_err(&client->dev, "%s - cound not register sensor(%d).\n", __func__, err);
            return -ENOMEM;
        }

        err = sensors_register(&pplatData->factory_device_sub, this, sub_sensor_attrs, SUB_MODULE_NAME);
        if (err) {
            dev_err(&client->dev, "%s - cound not register sensor(%d).\n", __func__, err);
            return -ENOMEM;
        }

        err = sysfs_create_group(&input_bottom_sar->dev.kobj, &ss_sx932x_attributes_group);
        if (err) {
            dev_err(&client->dev, "cound not create input bottom group\n");
            return -ENOMEM;
        }

        err = sysfs_create_group(&input_top_sar->dev.kobj, &ss_sx932x_attributes_group);
        if (err) {
            dev_err(&client->dev, "cound not create input top group\n");
            return -ENOMEM;
        }
#endif

        sx93XX_IRQ_init(this);
        this_enable = this;
        sx932x_plat_charger_init();

        /* call init function pointer (this should initialize all registers */
        if (this->init){
            this->init(this);
        }else{
            dev_err(this->pdev,"No init function!!!!\n");
            return -ENOMEM;
        }
    }else{
        return -1;
    }

    pplatData->exit_platform_hw = sx932x_exit_platform_hw;

    dev_info(&client->dev, "sx932x_probe() Done\n");

    return 0;
}

/*! \fn static int sx932x_remove(struct i2c_client *client)
 * \brief Called when device is to be removed
 * \param client Pointer to i2c_client struct
 * \return Value from sx93XX_remove()
 */
//static int __devexit sx932x_remove(struct i2c_client *client)
static int sx932x_remove(struct i2c_client *client)
{
    psx932x_platform_data_t pplatData =0;
    psx932x_t pDevice = 0;
    psx93XX_t this = i2c_get_clientdata(client);
    if (this && (pDevice = this->pDevice))
    {
        input_unregister_device(pDevice->pbuttonInformation->input_top_sar);
        input_unregister_device(pDevice->pbuttonInformation->input_bottom_sar);

        sysfs_remove_group(&client->dev.kobj, &sx932x_attr_group);
        pplatData = client->dev.platform_data;
        if (pplatData && pplatData->exit_platform_hw)
            pplatData->exit_platform_hw(client);
        kfree(this->pDevice);
    }
    return sx93XX_remove(this);
}
#if 1//def CONFIG_PM
/*====================================================*/
/***** Kernel Suspend *****/
static int sx932x_suspend(struct device *dev)
{
    psx93XX_t this = dev_get_drvdata(dev);
    if (this){
        if (mEnabled == 0)
            sx93XX_suspend(this);
    }
    return 0;
}
/***** Kernel Resume *****/
static int sx932x_resume(struct device *dev)
{
    psx93XX_t this = dev_get_drvdata(dev);
    if (this){
        if (mEnabled == 0)
            sx93XX_resume(this);
    }
    return 0;
}
/*====================================================*/
#else
#define sx932x_suspend        NULL
#define sx932x_resume        NULL
#endif /* CONFIG_PM */

static struct i2c_device_id sx932x_idtable[] = {
    { DRIVER_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sx932x_idtable);
#ifdef CONFIG_OF
static struct of_device_id sx932x_match_table[] = {
    { .compatible = "Semtech,sx932x",},
    { },
};
#else
#define sx932x_match_table NULL
#endif
static const struct dev_pm_ops sx932x_pm_ops = {
    .suspend = sx932x_suspend,
    .resume = sx932x_resume,
};
static struct i2c_driver sx932x_driver = {
    .driver = {
        .owner            = THIS_MODULE,
        .name             = DRIVER_NAME,
        .of_match_table   = sx932x_match_table,
        .pm               = &sx932x_pm_ops,
    },
    .id_table        = sx932x_idtable,
    .probe           = sx932x_probe,
    .remove          = sx932x_remove,
};
static int __init sx932x_I2C_init(void)
{
    return i2c_add_driver(&sx932x_driver);
}
static void __exit sx932x_I2C_exit(void)
{
    i2c_del_driver(&sx932x_driver);
}

module_init(sx932x_I2C_init);
module_exit(sx932x_I2C_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX932x Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

static void sx93XX_schedule_work(psx93XX_t this, unsigned long delay)
{
    unsigned long flags;
    if (this) {
        dev_info(this->pdev, "sx93XX_schedule_work()\n");
        spin_lock_irqsave(&this->lock,flags);
        /* Stop any pending penup queues */
        cancel_delayed_work(&this->dworker);
        //after waiting for a delay, this put the job in the kernel-global workqueue. so no need to create new thread in work queue.
        schedule_delayed_work(&this->dworker,delay);
        spin_unlock_irqrestore(&this->lock,flags);
    }
    else
        printk(KERN_ERR "sx93XX_schedule_work, NULL psx93XX_t\n");
}

static irqreturn_t sx93XX_irq(int irq, void *pvoid)
{
    psx93XX_t this = 0;
    if (pvoid) {
        this = (psx93XX_t)pvoid;
        if ((!this->get_nirq_low) || this->get_nirq_low()) {
        sx93XX_schedule_work(this,0);
        }
        else{
            dev_err(this->pdev, "sx93XX_irq - nirq read high\n");
        }
    }
    else{
        printk(KERN_ERR "sx93XX_irq, NULL pvoid\n");
    }
    return IRQ_HANDLED;
}

static void sx93XX_worker_func(struct work_struct *work)
{
    psx93XX_t this = 0;
    int status = 0;
    int counter = 0;
    u8 nirqLow = 0;
    if (work) {
        this = container_of(work,sx93XX_t,dworker.work);

        if (!this) {
            printk(KERN_ERR "sx93XX_worker_func, NULL sx93XX_t\n");
            return;
        }
        if (unlikely(this->useIrqTimer)) {
            if ((!this->get_nirq_low) || this->get_nirq_low()) {
                nirqLow = 1;
            }
        }
        /* since we are not in an interrupt don't need to disable irq. */
        status = this->refreshStatus(this);
        counter = -1;
        dev_dbg(this->pdev, "Worker - Refresh Status %d\n",status);

        while((++counter) < MAX_NUM_STATUS_BITS) { /* counter start from MSB */
            if (((status>>counter) & 0x01) && (this->statusFunc[counter])) {
                dev_info(this->pdev, "SX932x Function Pointer Found. Calling\n");
                this->statusFunc[counter](this);
            }
        }
        if (unlikely(this->useIrqTimer && nirqLow))
        {    /* Early models and if RATE=0 for newer models require a penup timer */
            /* Queue up the function again for checking on penup */
            sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
        }
    } else {
        printk(KERN_ERR "sx93XX_worker_func, NULL work_struct\n");
    }
}

int sx93XX_remove(psx93XX_t this)
{
    if (this) {
        cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
        /*destroy_workqueue(this->workq); */
        free_irq(this->irq, this);
        kfree(this);
        return 0;
    }
    return -ENOMEM;
}
void sx93XX_suspend(psx93XX_t this)
{
    if (0){
        dev_info(this->pdev, "sx932x_suspend!\n");
        write_register(this,SX932x_CTRL1_REG,0x07);//make sx932x in Stop mode
    }
}
void sx93XX_resume(psx93XX_t this)
{
    if (0) {
        dev_info(this->pdev, "sx932x_resume!\n");
        //write_register(this,SX932x_CTRL1_REG,0x27);//make sx932x in Active mode
    }
}

int sx93XX_IRQ_init(psx93XX_t this)
{
    int err = 0;
    if (this && this->pDevice)
    {
        /* initialize spin lock */
        spin_lock_init(&this->lock);
        /* initialize worker function */
        INIT_DELAYED_WORK(&this->dworker, sx93XX_worker_func);
        /* initailize interrupt reporting */
        this->irq_disabled = 0;
        err = request_irq(this->irq, sx93XX_irq, IRQF_TRIGGER_FALLING,
                            this->pdev->driver->name, this);
        if (err) {
            dev_err(this->pdev, "irq %d busy?\n", this->irq);
            return err;
        }
        dev_info(this->pdev, "registered with irq (%d)\n", this->irq);
    }
    return -ENOMEM;
}
