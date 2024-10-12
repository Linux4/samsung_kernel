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
//#define DEBUG
#define DRIVER_NAME "sx932x"

#define MAX_WRITE_ARRAY_SIZE 32


//+S96818AA1-2208, liuling3.wt,ADD, 2023/07/03, add sar power reduction control switch
#define POWER_ENABLE    1
//-S96818AA1-2208, liuling3.wt,ADD, 2023/07/03, add sar power reduction control switch
#define USER_TEST       1

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
#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/sort.h> 
#include <linux/gpio.h>
#include <linux/of_gpio.h>

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
#include <linux/sensors.h>
#include <linux/hardware_info.h>
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */

#include "sx932x.h" 	/* main struct, interrupt,init,pointers */

#define IDLE			0
#define ACTIVE			1

/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
#define SAR_STATE_NEAR  1
#define SAR_STATE_FAR   2
/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
#define SAR_STATE_NONE  -1

#define SX932x_NIRQ		34

#define MAIN_SENSOR		1 //CS1

/* Failer Index */
#define SX932x_ID_ERROR 	1
#define SX932x_NIRQ_ERROR	2
#define SX932x_CONN_ERROR	3
#define SX932x_I2C_ERROR	4

#define PROXOFFSET_LOW			1500

#define SX932x_ANALOG_GAIN		1
#define SX932x_DIGITAL_GAIN		1
#define SX932x_ANALOG_RANGE		2.65

#define	TOUCH_CHECK_REF_AMB      0 // 44523
#define	TOUCH_CHECK_SLOPE        0 // 50
#define	TOUCH_CHECK_MAIN_AMB     0 // 151282

/*! \struct sx932x
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct sx932x
{
	pbuttonInformation_t pbuttonInformation;
	psx932x_platform_data_t hw;		/* specific platform data settings */
} sx932x_t, *psx932x_t;

static int irq_gpio_num;
psx93XX_t pSx9328Data;

static struct wake_lock sx9328_wake_lock;
static volatile uint8_t sx9328_irq_from_suspend_flag = 0;

//static void sx93XX_worker_func(struct work_struct *work);
static void sx932x_process_func(psx93XX_t this);
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

/*********************************************************************
compensate the chip by turn off/on phases
 */
static int manual_offset_calibration(psx93XX_t this)
{
	s32 ret = 0;
	u8 ori_val, reg_value = 0;	
	
	ret |= read_register(this,SX932x_CTRL1_REG,&ori_val);

	//turn off all phases
	reg_value = ori_val & 0xF0;
	ret |= write_register(this, SX932x_CTRL1_REG, reg_value);
	msleep(100);
	ret |= write_register(this, SX932x_CTRL1_REG, ori_val);

    printk("sx9328: manual_offset_calibration!");
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

/*+S96818AA1-1936, liuling3.wt,ADD, 2023/07/11, add chip_id check judgment*/
	if(failcode != SX932x_WHOAMI_VALUE){
        pr_info("smtc: sx9328 Hardware_Check SX932x_WHOAMI_REG = 0x%x, failcode!=target_value:0x%x\n", failcode, SX932x_WHOAMI_VALUE);
        return -1;
	}
/*-S96818AA1-1936, liuling3.wt,ADD, 2023/07/11, add chip_id check judgment*/
	
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

/*
set/unset bits of [3:0] to enable/disable the corresponding phases

for example, below command enables phase 0 and 1
echo 0x3 > eanble
*/
static ssize_t sx932x_enable_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	u8 reg_val=0;
	u32 mask = 0;
	psx93XX_t this = dev_get_drvdata(dev);	

	if (sscanf(buf, "%x", &mask) != 1) {
		pr_err("[SX932x]: %s - The number of data are wrong\n", __func__);
		return -EINVAL;
	}
	dev_info(this->pdev, "enable phases=0x%X\n", mask);
	
	read_register(this, SX932x_CTRL1_REG, &reg_val);
	reg_val &= ~0xF;
	reg_val |= (mask & 0xF);
	write_register(this, SX932x_CTRL1_REG, reg_val);
	

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

static DEVICE_ATTR(manual_calibrate, 0664, manual_offset_calibration_show,manual_offset_calibration_store);
static DEVICE_ATTR(register_write,  0664, NULL,sx932x_register_write_store);
static DEVICE_ATTR(register_read,0664, NULL,sx932x_register_read_store);
static DEVICE_ATTR(raw_data,0664,sx932x_raw_data_show,NULL);
static DEVICE_ATTR(sx932x_enable, 0664, NULL,sx932x_enable_store);


static struct attribute *sx932x_attributes[] = {
	&dev_attr_manual_calibrate.attr,
	&dev_attr_register_write.attr,
	&dev_attr_register_read.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_sx932x_enable.attr,
	NULL,
};
static struct attribute_group sx932x_attr_group = {
	.attrs = sx932x_attributes,
};

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, get input dev */
static ssize_t show_enable(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    size_t i = 0;
    int status = 0;
    char *p = buf;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    pr_info("smtc:%s: dev->name:%s\n", __func__, temp_input_dev->name);

    for (i = 0; i < ARRAY_SIZE(psmtcButtons); i++) {
        if (!strcmp(temp_input_dev->name, psmtcButtons[i].name)) {
            if (true == psmtcButtons[i].enabled)
                status = 1;
            break;
        }
    }
    p += snprintf(p, PAGE_SIZE, "%d", status);
    return 1;
}
static ssize_t store_enable(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    size_t i = 0;
    u8 reg_val=0;
    u32 pahse = 0;
    struct input_dev* temp_input_dev;
    bool enable = simple_strtol(buf, NULL, 10) ? 1 : 0;
    psx93XX_t this = pSx9328Data;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    pr_info("smtc:%s: dev->name:%s:%s\n", __func__, temp_input_dev->name, buf);

    for (i = 0; i < ARRAY_SIZE(psmtcButtons); i++) {
        if (!strcmp(temp_input_dev->name, psmtcButtons[i].name)) {
            if (enable) {
                wake_lock_timeout(&sx9328_wake_lock, HZ * 1);
                psmtcButtons[i].enabled = true;
                if (0 == i) {
                    pahse = (0x01 << i) | (0x01 << i+1) | 0x20;
                } else {
                    pahse = (0x01 << i) | 0x20;
                }
				read_register(this, SX932x_CTRL1_REG, &reg_val);
                pr_info("sx932x:before enable psmtcButtons value is 0X%X\n", reg_val);
				reg_val &= 0xF;
				reg_val |= pahse;
				write_register(this, SX932x_CTRL1_REG, reg_val);
                manual_offset_calibration(this);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(psmtcButtons[i].input, REL_MISC, SAR_STATE_FAR);
                input_report_rel(psmtcButtons[i].input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
				input_sync(psmtcButtons[i].input);
                pr_info("smtc:sx932x:ph%d: enable, now psmtcButtons value is 0X%X\n", i, reg_val);
            } else {
                wake_lock_timeout(&sx9328_wake_lock, HZ * 1);
                psmtcButtons[i].enabled = false;
                if (0 == i) {
                    pahse = (~(0x01 << i) & ~(0x01 << i+1));
                } else {
                    pahse = ~(0x01 << i);
                }
                read_register(this, SX932x_CTRL1_REG, &reg_val);
                pr_info("smtc:sx932x:before disable psmtcButtons value is 0X%X\n", reg_val);
                reg_val &= 0x2F;
                reg_val &= pahse;
                write_register(this, SX932x_CTRL1_REG, reg_val);
                manual_offset_calibration(this);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(psmtcButtons[i].input, REL_MISC, SAR_STATE_NONE);
                input_report_rel(psmtcButtons[i].input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_sync(psmtcButtons[i].input);
                pr_info("smtc:sx932x:ph%d: disable, now psmtcButtons value is 0X%X\n", i, reg_val);
            }
        }
    }
    return count;
}
static DEVICE_ATTR(enable, 0660, show_enable, store_enable);
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, get input dev */

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt factory app test need */
static ssize_t cap_diff_dump_show(psx93XX_t this, int ch, char *buf)
{
    ssize_t len = 0;
    u16 offset;
    s32 diff;
    u8 msb=0, lsb=0;
    
    write_register(this,SX932x_CPSRD, ch);
    read_register(this,SX932x_OFFSETMSB, &msb);
    read_register(this,SX932x_OFFSETLSB, &lsb);
    offset = (u16)((msb << 8) | lsb);

    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_background_cap=%d;", ch, offset);

    read_register(this,SX932x_DIFFMSB, &msb);
    read_register(this,SX932x_DIFFLSB, &lsb);
    diff = (s32)((msb << 8) | lsb);

    if (diff > 32767)
        diff -= 65536;
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_diff=%d\n", ch, diff); 

    return len;
}
static ssize_t ch0_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    psx93XX_t this = pSx9328Data;
    return cap_diff_dump_show(this, 0, buf);
}
static ssize_t ch1_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    psx93XX_t this = pSx9328Data;
    return cap_diff_dump_show(this, 1, buf);
}
static ssize_t ch2_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    psx93XX_t this = pSx9328Data;
    return cap_diff_dump_show(this, 2, buf);
}

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
static ssize_t power_enable_show(struct class *class,
                                 struct class_attribute *attr,
                                 char *buf)
{
    return sprintf(buf, "%d\n", pSx9328Data->power_state);
}
static ssize_t power_enable_store(struct class *class,
                                  struct class_attribute *attr,
                                  const char *buf, size_t count)
{
    int ret = -1;

    ret = kstrtoint(buf, 10, &pSx9328Data->power_state);
    if (0 != ret) {
        printk("sx9328: kstrtoint failed\n");
    }
    return count;
}
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch

#if USER_TEST
static ssize_t user_test_store(struct class *class,
                               struct class_attribute *attr,
                               const char *buf, size_t count)
{
    int ret;
    int val = 0;
    psx93XX_t this = pSx9328Data;

    ret = kstrtoint(buf, 10, &val);
    if (0 != ret) {
        printk("sx932x:kstrtoint failed\n");
    }

    printk("smtc:sx9338 user_test val = %d\n", val);
    if (val) {
        manual_offset_calibration(this);
    }
    return count;
}
#endif

//+S96818AA1-4548, liuling3.wt,ADD, 2023/06/27, add sar channels switch for wt factory test need
#if 1
static ssize_t sx9328_channel_en_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ch_id = 0;
    int en = 0;
    u8 reg_val=0, pahse = 0;
    psx93XX_t this = pSx9328Data;

    if (sscanf(buf, "%d,%d", &ch_id, &en) != 2) {
        pr_info("please input two DEC numbers: ch_id,en (ch_id: channel number, en: 1=enable, 0=disable)\n");
        return -EINVAL;
    }

    if((ch_id >= ARRAY_SIZE(psmtcButtons)) || (ch_id < 0)) {
        pr_info("channel number out of range, the effective number is 0~%d\n", ARRAY_SIZE(psmtcButtons) - 1);
        return -EINVAL;
    }

    if(en > 0) {
        wake_lock_timeout(&sx9328_wake_lock, HZ * 1);
        pahse = (0x01 << ch_id) | 0x20;
        read_register(this, SX932x_CTRL1_REG, &reg_val);
        pr_info("sx932x:[channel_en] before psmtcButtons value is 0X%X\n", reg_val);
        reg_val &= 0xF;
        reg_val |= pahse;
        write_register(this, SX932x_CTRL1_REG, reg_val);
        manual_offset_calibration(this);
        input_report_rel(psmtcButtons[ch_id].input, REL_MISC, SAR_STATE_FAR);
        input_report_rel(psmtcButtons[ch_id].input, REL_X, 2);
        input_sync(psmtcButtons[ch_id].input);
        pr_info("smtc:sx932x:ph%d: [channel_en] enable, now psmtcButtons value is 0X%X\n", ch_id, reg_val);
    } else if (0 == en) {
        wake_lock_timeout(&sx9328_wake_lock, HZ * 1);
        pahse = ~(0x01 << ch_id);
        read_register(this, SX932x_CTRL1_REG, &reg_val);
        pr_info("smtc:sx932x:[channel_en] before disable psmtcButtons value is 0X%X\n", reg_val);
        reg_val &= 0x2F;
        reg_val &= pahse;
        write_register(this, SX932x_CTRL1_REG, reg_val);
        manual_offset_calibration(this);
        input_report_rel(psmtcButtons[ch_id].input, REL_MISC, SAR_STATE_NONE);
        input_report_rel(psmtcButtons[ch_id].input, REL_X, 2);
        input_sync(psmtcButtons[ch_id].input);
        pr_info("smtc:sx932x:ph%d: [channel_en] disable, now psmtcButtons value is 0X%X\n", ch_id, reg_val);
    }
    return count;
}
static ssize_t sx9328_channel_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;
    int enabled = 0;
    u8 reg_val=0, pahse = 0;
    psx93XX_t this = pSx9328Data;

    read_register(this, SX932x_CTRL1_REG, &reg_val);
    pr_info("sx932x:[channel_en_show] psmtcButtons value is 0X%X\n", reg_val);
    for(ii = 0; ii < ARRAY_SIZE(psmtcButtons); ii++) {
        pahse = 0x01;
        enabled = (reg_val >> ii) & pahse;
        p += snprintf(p, PAGE_SIZE, "sx9328_ch[%d]: enabled=%d\n", ii, enabled);
    }
    return (p - buf);
}
#endif
//-S96818AA1-4548, liuling3.wt,ADD, 2023/06/27, add sar channels switch for wt factory test need

static struct class_attribute class_attr_ch0_cap_diff_dump = __ATTR(ch0_cap_diff_dump, 0664, ch0_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch1_cap_diff_dump = __ATTR(ch1_cap_diff_dump, 0664, ch1_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch2_cap_diff_dump = __ATTR(ch2_cap_diff_dump, 0664, ch2_cap_diff_dump_show, NULL);
//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
static struct class_attribute class_attr_power_enable = __ATTR(power_enable, 0664, power_enable_show, power_enable_store);
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if USER_TEST
static struct class_attribute class_attr_user_test = __ATTR(user_test, 0664, NULL, user_test_store);
#endif
//+S96818AA1-4548, liuling3.wt,ADD, 2023/06/27, add sar channels switch for wt factory test need
static struct class_attribute class_attr_channel_en = __ATTR(channel_en, 0664, sx9328_channel_en_show, sx9328_channel_en_store);
//-S96818AA1-4548, liuling3.wt,ADD, 2023/06/27, add sar channels switch for wt factory test need

static struct attribute *sx9328_capsense_attrs[] = {
	&class_attr_ch0_cap_diff_dump.attr,
	&class_attr_ch1_cap_diff_dump.attr,
	&class_attr_ch2_cap_diff_dump.attr,
//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
    &class_attr_power_enable.attr,
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if USER_TEST
    &class_attr_user_test.attr,
#endif
//+S96818AA1-4548, liuling3.wt,ADD, 2023/06/27, add sar channels switch for wt factory test need
    &class_attr_channel_en.attr,
//-S96818AA1-4548, liuling3.wt,ADD, 2023/06/27, add sar channels switch for wt factory test need
	NULL,
};
ATTRIBUTE_GROUPS(sx9328_capsense);

static struct class capsense_class = {
	.name           = "capsense",
	.owner          = THIS_MODULE,
	.class_groups   = sx9328_capsense_groups,
};

/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt factory app test need */

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

		/* re-enable interrupt handling */
		enable_irq(this->irq);
		
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
	struct input_dev *input = NULL;

	struct _buttonInfo *pCurrentButton  = NULL;

	if (this && (pDevice = this->pDevice))
	{
		//dev_info(this->pdev, "Inside touchProcess()\n");
		read_register(this, SX932x_STAT0_REG, &i);

		buttons = pDevice->pbuttonInformation->buttons;
		numberOfButtons = pDevice->pbuttonInformation->buttonSize;

		if (unlikely( buttons==NULL )) {
			dev_err(this->pdev, "ERROR!! buttons  NULL!!!\n");
			return;
		}

		for (counter = 0; counter < ARRAY_SIZE(psmtcButtons); counter++) {
			pCurrentButton = &buttons[counter];
			if (pCurrentButton==NULL) {
				dev_err(this->pdev,"ERROR!! current button at index: %d NULL!!!\n", counter);
				return; // ERRORR!!!!
			}
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, get input dev */
            input = pCurrentButton->input;
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, get input dev */
            if (input==NULL)
            {
                printk("sx9328: ERROR!! current button input at index: %d NULL!!!\n", counter);
            }
//+S96818AA1-2208, liuling3.wt,ADD, 2023/07/03, add sar power reduction control switch
#if POWER_ENABLE
            if (pSx9328Data->power_state) {
                if ((IDLE == pCurrentButton->state) && ((i & pCurrentButton->mask) != pCurrentButton->mask)) {
                    dev_info(this->pdev, "[power_enable]: %s already released (far).\n", pCurrentButton->name);
                } else {
                    wake_lock_timeout(&sx9328_wake_lock, HZ * 1);
                    dev_info(this->pdev, "[power_enable]: %s report far.\n", pCurrentButton->name);
                    input_report_rel(input, REL_MISC, SAR_STATE_FAR);
                    input_report_rel(input, REL_X, 2);
                    input_sync(input);
                    pCurrentButton->state = IDLE;
                }
                continue;
            }
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/07/03, add sar power reduction control switch
			switch (pCurrentButton->state) {
				case IDLE: /* Button is not being touched! */
					if (((i & pCurrentButton->mask) == pCurrentButton->mask)) {
						/* User pressed button */
                        wake_lock_timeout(&sx9328_wake_lock, HZ * 1);
						dev_info(this->pdev, "%s touched near\n", pCurrentButton->name);
						/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
						input_report_rel(input, REL_MISC, SAR_STATE_NEAR);
						input_report_rel(input, REL_X, 2);
						/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                        input_sync(input);
						pCurrentButton->state = ACTIVE;
					} else {
						dev_info(this->pdev, "%s already released (far).\n", pCurrentButton->name);
					}
					break;
				case ACTIVE: /* Button is being touched! */ 
					if (((i & pCurrentButton->mask) != pCurrentButton->mask)) {
						/* User released button */
                        wake_lock_timeout(&sx9328_wake_lock, HZ * 1);
                        dev_info(this->pdev, "%s released far\n", pCurrentButton->name);
						/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
						input_report_rel(input, REL_MISC, SAR_STATE_FAR);
						input_report_rel(input, REL_X, 2);
						/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                        input_sync(input);
						pCurrentButton->state = IDLE;
					} else {
						dev_info(this->pdev, "%s already touched (near).\n", pCurrentButton->name);
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
	int ret,i;
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
		 pdata->pi2c_reg = devm_kzalloc(dev,sizeof(smtc_reg_data_t) * pdata->i2c_reg_num, GFP_KERNEL);
		 if (unlikely(pdata->pi2c_reg == NULL)) {
			return -ENOMEM;
		}

	 // initialize the array
	if (of_property_read_u8_array(dNode,"Semtech,reg-init",(u8*)&(pdata->pi2c_reg[0]), (sizeof(smtc_reg_data_t)*pdata->i2c_reg_num)))
		return -ENOMEM;
        for (i = 0; i < pdata->i2c_reg_num; i++)
            printk("wnn add addr:0x%x, val:0x%x\n", pdata->pi2c_reg[i].reg, pdata->pi2c_reg[i].val);
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

/* +S96818AA1-3871, liuling3.wt,ADD, 2023/05/30, distinguish sar driver for Australia */
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

		printk("smtc:sx9328  board_id = %s ", board_id);
        /* if it is LA board */
	    if ((!strncmp(board_id, "S96818CA1",strlen("S96818CA1")))) {
		     board_is_la = true;
		}
	} else
        board_is_la = false;
	return board_is_la;

}
/* -S96818AA1-3871, liuling3.wt,ADD, 2023/05/30, distinguish sar driver for Australia */

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
static int sx9328_flush_classdev(struct sensors_classdev *sensors_cdev, unsigned char flush)
{
    int ii = 0;

    for (ii = 0; ii < ARRAY_SIZE(psmtcButtons); ii++) {
        if (sensors_cdev->type == psmtcButtons[ii].type) {
            wake_lock_timeout(&sx9328_wake_lock, HZ * 1);
            input_report_rel(psmtcButtons[ii].input, REL_MAX, flush);
			input_sync(psmtcButtons[ii].input);
            break;
        }
    }
    return 0;
}
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/


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
	struct input_dev *input = NULL;
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

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/07, add hardware check */
    this->bus = client;
    err = sx932x_Hardware_Check(this);
/*+S96818AA1-1936, liuling3.wt,ADD, 2023/07/11, add chip_id check judgment*/
    if (2 == err || 4 == err || -1 == err){
/*-S96818AA1-1936, liuling3.wt,ADD, 2023/07/11, add chip_id check judgment*/
        printk("sx932x_Hardware_Check error!!");
        return -1;
    }
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/07, add hardware check */

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

	/* setup i2c communication */
	this->bus = client;
	i2c_set_clientdata(client, this);
	
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

		/* record device struct */
		this->pdev = &client->dev;

		/* create memory for device specific struct */
		this->pDevice = pDevice = devm_kzalloc(&client->dev,sizeof(sx932x_t), GFP_KERNEL);
		dev_info(&client->dev, "\t Initialized Device Specific Memory: 0x%p\n",pDevice);

        pSx9328Data = this;

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

            wake_lock_init(&sx9328_wake_lock, WAKE_LOCK_SUSPEND, "sx9328_wakelock");

			/* Create the input device */
            for (i = 0; i < pButtonInformationData->buttonSize; i++) {
				input = input_allocate_device();
				if (!input) {
					pr_err("input_allocate_device failed %s\n", pButtonInformationData->buttons[i].name);
					return -ENOMEM;
				}

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
				input->name = pButtonInformationData->buttons[i].name;
				/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
				__set_bit(EV_REL, input->evbit);
				input_set_capability(input, EV_REL, REL_MISC);
				__set_bit(EV_REL, input->evbit);
				input_set_capability(input, EV_REL, REL_X);
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
                input_set_capability(input, EV_REL, REL_MAX);
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
				/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
				pButtonInformationData->buttons[i].state = IDLE;

				/* save the input pointer and finish initialization */
				input->id.bustype = BUS_I2C;
				err = input_register_device(input);
				if (err) {
					pr_err("Failed to register input device %s. ret= %d", pButtonInformationData->buttons[i].name, err);
					return -ENOMEM;
				}
                printk("sx9328ll: input_register_device %s success\n", pButtonInformationData->buttons[i].name);

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
				if (0 != device_create_file(&input->dev, &dev_attr_enable)) {
					pr_info("%s attribute ENABLE create fail\n", input->name);
                   	return -1;
				}
                printk("sx9328ll:%s attribute ENABLE create success\n", input->name);

				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensors_enable = NULL;
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensors_poll_delay = NULL;
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
                pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensors_flush = sx9328_flush_classdev;
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensor_name = pButtonInformationData->buttons[i].name;
                pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.name = "SX9325";
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.vendor = "semtech";
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.version = 1;
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.type = pButtonInformationData->buttons[i].type;
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.max_range = "5";
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.resolution = "5.0";
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensor_power = "3";
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.min_delay = 0;
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.fifo_reserved_event_count = 0;
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.fifo_max_event_count = 0;
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.delay_msec = 100;
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.enabled = 0;

				err = sensors_classdev_register(&input->dev, &pButtonInformationData->buttons[i].sx9328_sensors_class_cdev);

				/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
				pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensor_name = "SX9325";
				/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */

                if (err < 0) {
	                pr_info("create %d cap sensor_class  file failed (%d)\n", i, err);
                	return -1;
                }
                printk("sx9328ll:%d / %s sensors_classdev_register success\n", i, input->name);

				pButtonInformationData->buttons[i].input = input;
				/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
				input_report_rel(pButtonInformationData->buttons[i].input, REL_MISC, SAR_STATE_NONE);
				input_report_rel(pButtonInformationData->buttons[i].input, REL_X, 2);
				/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
				input_sync(pButtonInformationData->buttons[i].input);
            }
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
		}

		sx9328_IRQ_init(this);
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


/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/29, wt Adaptive sensor hal */
    err = class_register(&capsense_class);
    if (err < 0) {
        printk("sx932x:register capsense class failed (%d)\n", &capsense_class);
        return err;
    }
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/29, wt Adaptive sensor hal */

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
    pSx9328Data->power_state = 0;
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch

	hardwareinfo_set_prop(HARDWARE_SAR, "sx9328_sar");
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
	struct _buttonInfo *pCurrentbutton;
    size_t index = 0;
	psx93XX_t this = i2c_get_clientdata(client);
	if (this && (pDevice = this->pDevice))
	{
    	for (index = 0; index < pDevice->pbuttonInformation->buttonSize; index++) {
        	pCurrentbutton = &(pDevice->pbuttonInformation->buttons[index]);
            sensors_classdev_unregister(&(pCurrentbutton->sx9328_sensors_class_cdev));
            input_unregister_device(pCurrentbutton->input);
        }

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
	dev_info(dev, "sx932x_suspend() enter\n");
	//sx9328_suspend(this);
    sx9328_irq_from_suspend_flag = 1;
    enable_irq_wake(this->irq);
	dev_info(dev, "sx932x_suspend() exit\n");
	return 0;
}
/***** Kernel Resume *****/
static int sx932x_resume(struct device *dev)
{
	psx93XX_t this = dev_get_drvdata(dev);
	dev_info(dev, "sx932x_resume() enter\n");
	//sx9328_resume(this);
    sx9328_irq_from_suspend_flag = 0;
    disable_irq_wake(this->irq);

	dev_info(dev, "sx932x_resume() update status after resume\n");
	sx932x_process_func(this);
	dev_info(dev, "sx932x_resume() exit\n");
	return 0;
}
/*====================================================*/
#else
#define sx932x_suspend		NULL
#define sx932x_resume		NULL
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
		.owner			= THIS_MODULE,
		.name			= DRIVER_NAME,
		.of_match_table	= sx932x_match_table,
		.pm				= &sx932x_pm_ops,
	},
	.id_table		= sx932x_idtable,
	.probe			= sx932x_probe,
	.remove			= sx932x_remove,
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
		dev_info(this->pdev, "sx932x:sx93XX_irq enter\n");

		if ((!this->get_nirq_low) || this->get_nirq_low()) {
            if(1 == sx9328_irq_from_suspend_flag) {
                sx9328_irq_from_suspend_flag = 0;
                printk("delay 50ms for waiting the i2c controller enter working mode\n");
                msleep(50);//如果从suspend被中断唤醒，该延时确保i2c控制器也从休眠唤醒并进入工作状态
            }
		
		dev_info(this->pdev, "sx932x:sx93XX_irq call worker func\n");
		sx932x_process_func(this);
		dev_info(this->pdev, "sx932x:sx93XX_irq exit\n");
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
static void sx932x_process_func(psx93XX_t this)
{
	//psx93XX_t this = 0;
	int status = 0;
	int counter = 0;
	u8 nirqLow = 0;
	if (1) {
		//this = container_of(work,sx93XX_t,dworker.work);

		if (!this) {
			printk(KERN_ERR "sx932x_process_func, NULL sx93XX_t\n");
			return;
		}
		dev_info(this->pdev, "sx932x:sx932x_process_func enter\n");

		if (unlikely(this->useIrqTimer)) {
			if ((!this->get_nirq_low) || this->get_nirq_low()) {
				nirqLow = 1;
			}
		}
		/* since we are not in an interrupt don't need to disable irq. */
		status = this->refreshStatus(this);
		counter = -1;
		dev_info(this->pdev, "sx932x:sx932x_process_func Worker - Refresh Status=0x%X\n",status);
		
		while((++counter) < MAX_NUM_STATUS_BITS) { /* counter start from MSB */
			if (((status>>counter) & 0x01) && (this->statusFunc[counter])) {
				dev_info(this->pdev, "SX932x Function Pointer Found. Calling\n");
				this->statusFunc[counter](this);
			}
		}

		if (status == 0){
			dev_info(this->pdev, "sx932x:sx932x_process_func Force to update statuw\n");
			touchProcess(this);
		}

		if (unlikely(this->useIrqTimer && nirqLow))
		{	/* Early models and if RATE=0 for newer models require a penup timer */
			/* Queue up the function again for checking on penup */
			sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
		}
		dev_info(this->pdev, "sx932x:sx932x_process_func exit\n");
	} else {
		printk(KERN_ERR "sx932x_process_func, NULL work_struct\n");
	}
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
		dev_info(this->pdev, "sx932x:sx93XX_worker_func enter\n");

		if (unlikely(this->useIrqTimer)) {
			if ((!this->get_nirq_low) || this->get_nirq_low()) {
				nirqLow = 1;
			}
		}
		/* since we are not in an interrupt don't need to disable irq. */
		status = this->refreshStatus(this);
		counter = -1;
		dev_info(this->pdev, "sx932x:sx93XX_worker_func Worker - Refresh Status=0x%X\n",status);
		
		while((++counter) < MAX_NUM_STATUS_BITS) { /* counter start from MSB */
			if (((status>>counter) & 0x01) && (this->statusFunc[counter])) {
				dev_info(this->pdev, "SX932x Function Pointer Found. Calling\n");
				this->statusFunc[counter](this);
			}
		}

		if (status == 0){
			dev_info(this->pdev, "sx932x:sx93XX_worker_func Force to update statuw\n");
			touchProcess(this);
		}

		if (unlikely(this->useIrqTimer && nirqLow))
		{	/* Early models and if RATE=0 for newer models require a penup timer */
			/* Queue up the function again for checking on penup */
			sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
		}
		dev_info(this->pdev, "sx932x:sx93XX_worker_func exit\n");
	} else {
		printk(KERN_ERR "sx93XX_worker_func, NULL work_struct\n");
	}
}

int sx93XX_remove(psx93XX_t this)
{
	if (this) {
        wake_lock_destroy(&sx9328_wake_lock);
		cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
		/*destroy_workqueue(this->workq); */
		free_irq(this->irq, this);
		kfree(this);
		return 0;
	}
	return -ENOMEM;
}
void sx9328_suspend(psx93XX_t this)
{
	//if (this)
	//	disable_irq(this->irq);
	
	//write_register(this,SX932x_CTRL1_REG,0x20);//make sx932x in Sleep mode
}
void sx9328_resume(psx93XX_t this)
{

	if (this) {
		sx93XX_schedule_work(this,0);
		//if (this->init)
			//this->init(this);
	//enable_irq(this->irq);
	}
	write_register(this,SX932x_CTRL1_REG,0x27);//resume from sleep, need to modify based on number of channel.
}

int sx9328_IRQ_init(psx93XX_t this)
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
		err = request_threaded_irq(this->irq, NULL, sx93XX_irq, IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
							this->pdev->driver->name, this);
		if (err) {
			dev_err(this->pdev, "irq %d busy?\n", this->irq);
			return err;
		}
//        enable_irq_wake(this->irq);
		dev_info(this->pdev, "registered with irq (%d)\n", this->irq);
	}
	return -ENOMEM;
}
