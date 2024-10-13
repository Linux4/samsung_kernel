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


//+P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/30, add sar power reduction control switch
#define POWER_ENABLE    1
//-P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/30, add sar power reduction control switch
#define SAR_IN_FRANCE              1

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
//#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/sort.h> 
#include <linux/gpio.h>
#include <linux/of_gpio.h>

/* + P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
#include <linux/sensors.h>
#include <linux/hardware_info.h>
/* - P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, wt Adaptive sensor hal */

#include "sx932x.h" 	/* main struct, interrupt,init,pointers */
#if SAR_IN_FRANCE
#define MAX_INT_COUNT 20
static int32_t enable_data[3] = {1};
#endif
#define IDLE			0
#define ACTIVE			1

#define SAR_STATE_NEAR  1
#define SAR_STATE_FAR   2
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

#define IRQ_NAME "sx932x IRQ"

#define USE_THREAD_IRQ
#define ENABLE_IRQ_WAKE

#define SMTC_LOG_DBG(fmt, args...)   pr_debug ("[DBG] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)
#define SMTC_LOG_INF(fmt, args...)   pr_info ("[INF] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)
#define SMTC_LOG_ERR(fmt, args...)   pr_err  ("[ERR] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)

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

#ifndef USE_THREAD_IRQ
static u32 _irq_mask = 0x70;
#endif

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
	//u8 loop = 0;
	this->failStatusCode = 0;
	
	/*Check th IRQ Status
	while(this->get_nirq_low && this->get_nirq_low()){
		read_regStat(this);
		msleep(100);
		if(++loop >10){
			this->failStatusCode = SX932x_NIRQ_ERROR;
			break;
		}
	}*/
	
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
#if SAR_IN_FRANCE
static void enable_get_offset(struct work_struct *work) 
{
    size_t i = 0;
    psx93XX_t this = pSx9328Data;
    u8 msb=0, lsb=0;
    if (this->sar_first_boot && enable_data[i] == 2) {
        read_register(this,SX932x_OFFSETMSB, &msb);
        read_register(this,SX932x_OFFSETLSB, &lsb);
        this->ch0_backgrand_cap = (u16)((msb << 8) | lsb);
    pr_info("sx928[store_enable]:ch0_backgrand_cap:%d\n", this->ch0_backgrand_cap);
    enable_data[i] = 0; 
    }
    pr_info("sx9328[store_enable]:ch0_backgrand_cap:%d\n", this->ch0_backgrand_cap);
}
#endif
//+ P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev
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
                psmtcButtons[i].enabled = true;
                if (0 == i) {
                    pahse = (0x01 << i) | (0x01 << (i+2)) | 0x20;
                } else {
                    pahse = (0x01 << i) | 0x20;
                }
                read_register(this, SX932x_CTRL1_REG, &reg_val);
                pr_info("sx932x:before enable psmtcButtons value is 0X%X\n", reg_val);
                //reg_val &= 0xF;
                reg_val |= pahse;
                write_register(this, SX932x_CTRL1_REG, reg_val);
                manual_offset_calibration(this);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(psmtcButtons[i].input, REL_MISC, SAR_STATE_FAR);
                input_report_rel(psmtcButtons[i].input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_sync(psmtcButtons[i].input);
                pr_info("smtc:sx932x:ph%d: enable, now psmtcButtons value is 0X%X\n", i, reg_val);
#if SAR_IN_FRANCE
                if (this->sar_first_boot && enable_data[0] == 1) {
                    enable_data[0] = 2;
                    schedule_delayed_work(&this->delay_get_offset, msecs_to_jiffies(1200));
                }
#endif
               } else {
                psmtcButtons[i].enabled = false;
                if (0 == i) {
                    pahse = (~(0x01 << i) & ~(0x01 << (i+2))); 
                } else {
                    pahse = ~(0x01 << i);
                }
                read_register(this, SX932x_CTRL1_REG, &reg_val);
                pr_info("smtc:sx932x:before disable psmtcButtons value is 0X%X\n", reg_val);
               // reg_val &= 0x2F;
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
//- P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev

static ssize_t sx932x_prox_state_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    int state, ch, len=0;
    u8 state1, state2;    
    u8 prox1, prox2, prox3, prox4;
    psx93XX_t this = pSx9328Data;
    if ( read_register(this, SX932x_STAT0_REG, &state1) < 0 ){
        dev_err(this->pdev, "Failed to read prox status\n");
    }
    prox1 = state1 & 0xF;
    prox2 = state1 >> 4 & 0xF;

    if ( read_register(this, SX932x_STAT1_REG, &state2) < 0 ){
        dev_err(this->pdev, "Failed to read prox status\n");
    }
    
    prox4 = state2 & 0xF;
    prox3 = state2 >> 4 & 0xF;

    dev_info(this->pdev, "state1=0x%X state2=0x%X prox1=0x%X prox2=0x%X prox3=0x%X prox4=0x%X\n",
        state1, state2, prox1, prox2, prox3, prox4
    );

    for (ch=0; ch<3; ch++)
    {
        state = (prox1 & 1<<ch) || (prox2 & 1<<ch) || (prox3 & 1<<ch) || (prox4 & 1<<ch);
        len += snprintf(buf+len, PAGE_SIZE, "ch%d_state=%d,", ch, state);
    }
    
    return len;
}

static ssize_t cap_diff_dump_show(psx93XX_t this, int ch, char *buf)
{
    ssize_t len = 0;
    u16 offset;
    s32 diff;
    u8 msb=0, lsb=0;
	write_register(this,SX932x_CPSRD, 0);
	read_register(this,SX932x_OFFSETMSB, &msb);
	read_register(this,SX932x_OFFSETLSB, &lsb);
	offset = (u16)((msb << 8) | lsb);
	len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_background_cap=%d;", ch, offset);
	write_register(this,SX932x_CPSRD, 0);
	read_register(this,SX932x_DIFFMSB, &msb);
	read_register(this,SX932x_DIFFLSB, &lsb);
	diff = (s32)((msb << 8) | lsb);
	if (diff > 32767)
            diff -= 65536;
	len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_diff=%d;", ch, diff); 
	
	write_register(this,SX932x_CPSRD, 2);
	read_register(this,SX932x_OFFSETMSB, &msb);
	read_register(this,SX932x_OFFSETLSB, &lsb);
	offset = (u16)((msb << 8) | lsb);
	len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_refer_channel_cap=%d;", ch, offset);
        return len;
}
static ssize_t ch0_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    psx93XX_t this = pSx9328Data;
    return cap_diff_dump_show(this, 0, buf);
}

//+P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/30, add sar power reduction control switch
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
//-P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/30, add sar power reduction control switch

#if SAR_IN_FRANCE
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
        this->sar_first_boot = false;
        pr_info("this->sar_first_boot:%d\n", this->sar_first_boot);
        pr_info("sx9328:user_test mode1, exit force input near mode!!!\n");
        manual_offset_calibration(this);
    }
    return count;
}
#endif

static struct class_attribute class_attr_ch0_cap_diff_dump = __ATTR(ch0_cap_diff_dump, 0664, ch0_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_near_state = __ATTR(near_state, 0664, sx932x_prox_state_dump_show, NULL);
//+P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
static struct class_attribute class_attr_power_enable = __ATTR(power_enable, 0664, power_enable_show, power_enable_store);
#endif
//-P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/30, add sar power reduction control switch
#if SAR_IN_FRANCE
static struct class_attribute class_attr_user_test = __ATTR(user_test, 0664, NULL, user_test_store);
#endif

static struct attribute *sx9328_capsense_attrs[] = {
	&class_attr_ch0_cap_diff_dump.attr,
	&class_attr_near_state.attr,
//+P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
    &class_attr_power_enable.attr,
#endif
//-P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/30, add sar power reduction control switch
#if SAR_IN_FRANCE
    &class_attr_user_test.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(sx9328_capsense);

static struct class capsense_class = {
	.name           = "capsense",
	.owner          = THIS_MODULE,
	.class_groups   = sx9328_capsense_groups,
};

/* - P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, wt factory app test need */

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
#if SAR_IN_FRANCE
    u16 ch0_result = 0;
    u8 msb=0, lsb=0;
#endif
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
#if SAR_IN_FRANCE
        if (this->interrupt_count <= MAX_INT_COUNT) {
            this->interrupt_count++;
            pr_info("sx9328 interrupt_count is %d;", this->interrupt_count);
        }

        if (this->sar_first_boot) {
           read_register(this,SX932x_OFFSETMSB, &msb);
           read_register(this,SX932x_OFFSETLSB, &lsb);
           ch0_result = (u16)((msb << 8) | lsb);
        }
        pr_info("sx9328: this->sar_first_boot=%d\n",this->sar_first_boot);
        pr_info("sx9328: ch0_result: %d\n", ch0_result);
        pr_info("sx9328: this->sar_first_boot=%d\n",this->sar_first_boot);
#endif
              for (counter = 0; counter < numberOfButtons; counter++) {
                   pCurrentButton = &buttons[counter];
                     if (pCurrentButton==NULL) {
                        dev_err(this->pdev,"ERROR!! current button at index: %d NULL!!!\n", counter);
                        return; // ERRORR!!!!
                   }
/* + P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev */
            input = pCurrentButton->input;
/* + P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev */
            if (input==NULL)
            {
                printk("sx9328: ERROR!! current button input at index: %d NULL!!!\n", counter);
            }
#if SAR_IN_FRANCE
            if (this->sar_first_boot) {
                if ((this->interrupt_count < MAX_INT_COUNT) && ((ch0_result - this->ch0_backgrand_cap) >= -3)) {
                    input_report_rel(input, REL_MISC, SAR_STATE_NEAR);
                    input_report_rel(input, REL_X, 2);
                    input_sync(input);
                    pCurrentButton->state = ACTIVE;
                    pr_info("sx9328: force %s State=Near\n", pCurrentButton->name);
                    continue;
                }
                this->sar_first_boot = false;
                write_register(this, SX932x_PROX_CTRL2_REG, 0x20);
                write_register(this, SX932x_PROX_CTRL3_REG, 0x20);
                pr_info("sx9328: exit force input near mode!!!\n");
            }
#endif
#if POWER_ENABLE
            if (pSx9328Data->power_state) {
                if ((IDLE == pCurrentButton->state) && ((i & pCurrentButton->mask) != pCurrentButton->mask)) {
                    dev_info(this->pdev, "[power_enable]: %s already released (far).\n", pCurrentButton->name);
                } else {
                    dev_info(this->pdev, "[power_enable]: %s report far.\n", pCurrentButton->name);
                    input_report_rel(input, REL_MISC, SAR_STATE_FAR);
                    input_report_rel(input, REL_X, 2);
                    input_sync(input);
                    pCurrentButton->state = IDLE;
                }
                continue;
            }
#endif
            switch (pCurrentButton->state) {
                   case IDLE: /* Button is not being touched! */
#if SAR_IN_FRANCE	   
                     if (((i & pCurrentButton->mask) == pCurrentButton->mask && this->anfr_export_exit == false)) {
#else    
                     if (((i & pCurrentButton->mask) == pCurrentButton->mask)) {
#endif
                           /* User pressed button */
                           dev_info(this->pdev, "%s touched near\n", pCurrentButton->name);
/* + P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev */
                          input_report_rel(input, REL_MISC, SAR_STATE_NEAR);
                          input_report_rel(input, REL_X, 2);
/* + P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev */
                           input_sync(input);
                           pCurrentButton->state = ACTIVE;
                        } else {
                           dev_info(this->pdev, "%s already released (far).\n", pCurrentButton->name);
                        }
                          break;
                   case ACTIVE: /* Button is being touched! */
#if SAR_IN_FRANCE   
                    if (((i & pCurrentButton->mask) != pCurrentButton->mask && this->anfr_export_exit == false)) {
#else
                        if (((i & pCurrentButton->mask) != pCurrentButton->mask)) {
#endif
                           /* User released button */
                           dev_info(this->pdev, "%s released far\n", pCurrentButton->name);
/* + P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev */
                          input_report_rel(input, REL_MISC, SAR_STATE_FAR);
                          input_report_rel(input, REL_X, 2);
/* + P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev */
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
#if SAR_IN_FRANCE
void exit_anfr_sx9328(void )
{
    psx93XX_t this = pSx9328Data;

    if (this->sar_first_boot) {
        this->sar_first_boot = false;
        this->anfr_export_exit = true;
        pr_info("this->sar_first_boot:%d\n", this->sar_first_boot);
        pr_info("sx9328:exit force input near mode, and report sar once!!\n");
        touchProcess(this);
        this->anfr_export_exit = false;
    }
}
EXPORT_SYMBOL(exit_anfr_sx9328);
#endif
static int sx932x_parse_dt(struct sx932x_platform_data *pdata, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;
	int i;
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

// + wt add Adaptive ss sensor hal-vts fail,ludandan2 20230815, start
static int sx9328_flush_classdev(struct sensors_classdev *sensors_cdev, unsigned char flush)
{
    int ii = 0;

    for (ii = 0; ii < ARRAY_SIZE(psmtcButtons); ii++) {
        if (sensors_cdev->type == psmtcButtons[ii].type) {
            input_report_rel(psmtcButtons[ii].input, REL_MAX, flush);
            input_sync(psmtcButtons[ii].input);
            break;
        }
    }
    return 0;
}
// - wt add Adaptive ss sensor hal-vts fail,ludandan2 20230815, end

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

    this->bus = client;
    err = sx932x_Hardware_Check(this);
	if (2 == err | 4 == err | 1 == err){
		printk("sx932x_Hardware_Check error!!");
		return -1;
	}

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

			/* Create the input device */
            for (i = 0; i < pButtonInformationData->buttonSize; i++) {
				input = input_allocate_device();
				if (!input) {
					pr_err("input_allocate_device failed %s\n", pButtonInformationData->buttons[i].name);
					return -ENOMEM;
				}

/*+ P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, get input dev */
                                input->name = pButtonInformationData->buttons[i].name;
                                __set_bit(EV_REL, input->evbit);
                                input_set_capability(input, EV_REL, REL_MISC);
                                __set_bit(EV_REL, input->evbit);
                                input_set_capability(input, EV_REL, REL_X);
// + wt add Adaptive ss sensor hal-vts fail,ludandan2 20230815, start
                                input_set_capability(input, EV_REL, REL_MAX);
// - wt add Adaptive ss sensor hal-vts fail,ludandan2 20230815, end
/* P86801AA1-1937,Ludandan2.wt,ADD 2023/05/26, wt Adaptive sensor hal */
				pButtonInformationData->buttons[i].state = IDLE;

				/* save the input pointer and finish initialization */
				input->id.bustype = BUS_I2C;
				err = input_register_device(input);
				if (err) {
					pr_err("Failed to register input device %s. ret= %d", pButtonInformationData->buttons[i].name, err);
					return -ENOMEM;
				}
                printk("sx9328ll: input_register_device %s success\n", pButtonInformationData->buttons[i].name);

/* +P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
                                if (0 != device_create_file(&input->dev, &dev_attr_enable)) {
                                   pr_info("%s attribute ENABLE create fail\n", input->name);
                                   return -1;
                                   }
                printk("sx9328ll:%s attribute ENABLE create success\n", input->name);
                pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensors_enable = NULL;
                pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensors_poll_delay = NULL;
// + wt add Adaptive ss sensor hal-vts fail,ludandan2 20230815, start
                pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.sensors_flush = sx9328_flush_classdev;
// - wt add Adaptive ss sensor hal-vts fail,ludandan2 20230815, end
                pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.name = pButtonInformationData->buttons[i].name;
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
                pButtonInformationData->buttons[i].sx9328_sensors_class_cdev.name = "SX9375";
                if (err < 0) {
                   pr_info("create %d cap sensor_class  file failed (%d)\n", i, err);
                return -1;
                }
                printk("sx9328ll:%d / %s sensors_classdev_register success\n", i, input->name);
                pButtonInformationData->buttons[i].input = input;
               input_report_rel(pButtonInformationData->buttons[i].input, REL_MISC, SAR_STATE_NONE);
               input_report_rel(pButtonInformationData->buttons[i].input, REL_X, 2);
               input_sync(pButtonInformationData->buttons[i].input);
            }
/* - P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
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


/* + P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/29, wt Adaptive sensor hal */
    err = class_register(&capsense_class);
    if (err < 0) {
        printk("sx932x:register capsense class failed (%d)\n", &capsense_class);
        return err;
    }
/* - P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/29, wt Adaptive sensor hal */

//+P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
    pSx9328Data->power_state = 0;
#endif
#if SAR_IN_FRANCE
    this->sar_first_boot = true;
    this->user_test  = 0;
    write_register(this, SX932x_PROX_CTRL2_REG, 0x03);
    write_register(this, SX932x_PROX_CTRL3_REG, 0x21);
    INIT_DELAYED_WORK(&this->delay_get_offset, enable_get_offset);
#endif
//-P86801AA1-1937,Ludandan2.wt,ADD, 2023/05/15, add sar power reduction control switch

	hardwareinfo_set_prop(HARDWARE_SAR, "sx9328_sar");
	device_init_wakeup(&client->dev, 1);
	
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

static int sx932x_suspend(struct device *dev)
{
    int ret;
#ifndef ENABLE_IRQ_WAKE	
    u8 irq_src;
#endif
    psx93XX_t this = dev_get_drvdata(dev);    
    SMTC_LOG_INF("Enter");

#ifdef ENABLE_IRQ_WAKE
#if 1 //remove it on normal version
    ret = device_may_wakeup(dev);
    dev_info(this->pdev, "device_may_wakeup ret=%d", ret);
#endif

    ret = enable_irq_wake(this->irq);
    dev_info(this->pdev, "enable_irq_wake ret=%d", ret);

#else
    disable_irq(this->irq);
    read_register(this, 0x5, &_irq_mask);
    write_register(this, 0x5, 0x0);
    read_register(this, 0x0, &irq_src);
    SMTC_LOG_INF("Disable IRQ _irq_mask=0x%X irq_src=0x%X", _irq_mask, irq_src);
#endif

    return 0;
}
static int sx932x_resume(struct device *dev)
{
    int ret;
    psx93XX_t this = dev_get_drvdata(dev);

#ifdef ENABLE_IRQ_WAKE
    ret = disable_irq_wake(this->irq);
    dev_info(this->pdev, "sx932x disable_irq_wake ret=%d", ret);
#else
    dev_info(this->pdev, "sx932x _irq_mask=0x%X", _irq_mask);

    //in case status updated during suspend
#ifdef USE_THREAD_IRQ
    sx932x_process_irq(this);
#else
    schedule_delayed_work(&this->worker, 0);
#endif
        
    enable_irq(this->irq);
    write_register(this, REG_IRQ_MASK, _irq_mask);
#endif

    return 0;
}

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

rootfs_initcall(sx932x_I2C_init);
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


static void sx932x_process_irq(psx93XX_t this)
{
    int irq_src = 0;
    int counter = 0;
    u8 nirqLow = 0;

    if (!this) {
        printk(KERN_ERR "sx932x_worker_func, NULL sx93XX_t\n");
        return;
        }
    if (unlikely(this->useIrqTimer)) {
        if ((!this->get_nirq_low) || this->get_nirq_low()) {
            nirqLow = 1;
            }
        }
/* since we are not in an interrupt don't need to disable irq. */
    irq_src = this->refreshStatus(this);
    SMTC_LOG_INF("irq_src= 0x%x\n", irq_src);
#if SAR_IN_FRANCE
    if ((irq_src>>4) & 0x01)
        this->interrupt_count--;
#endif
    counter = -1;
    while((++counter) < MAX_NUM_STATUS_BITS) { /* counter start from MSB */
        if (((irq_src>>counter) & 0x01) && (this->statusFunc[counter])) {
            dev_info(this->pdev, "sx932x Function Pointer Found. Calling\n");
            this->statusFunc[counter](this);
        }
    }
    /*if (irq_src == 0){
        SMTC_LOG_INF("enter");
       // touchProcess(this);
    }*/
    if (unlikely(this->useIrqTimer && nirqLow)){
        /* Early models and if RATE=0 for newer models require a penup timer */
        /* Queue up the function again for checking on penup */
        sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
    }
}
#ifndef USE_THREAD_IRQ
static void sx932x_worker_func(struct work_struct *work)
{
    psx93XX_t this = container_of(work, sx93XX_t,dworker.work);
    sx932x_process_irq(this);
    pm_relax(this->pdev);
}
#endif

static irqreturn_t sx932x_irq_isr(int irq, void *pvoid)
{
    psx93XX_t this = (psx93XX_t)pvoid;
    if ((!this->get_nirq_low) || this->get_nirq_low()) {
        pm_stay_awake(this->pdev); 
#ifdef USE_THREAD_IRQ
        SMTC_LOG_INF("Update status directly when using thread IRQ");
		msleep(100);
        sx932x_process_irq(this);
        pm_relax(this->pdev);
#else
        SMTC_LOG_INF("Update status in worker when using hard IRQ");
        sx93XX_schedule_work(this, 0);
#endif
    }
    else{
        dev_err(this->pdev, "sx932x_irq - nirq read high\n");
    }

    return IRQ_HANDLED;
}

int sx93XX_remove(psx93XX_t this)
{
	if (this) {
		cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
		/*destroy_workqueue(this->workq); */
		cancel_delayed_work_sync(&this->delay_get_offset);
		free_irq(this->irq, this);
		kfree(this);
		return 0;
	}
	return -ENOMEM;
}


int sx9328_IRQ_init(psx93XX_t this)
{
	int err = 0;
	if (this && this->pDevice)
	{
		/* initialize spin lock */
		spin_lock_init(&this->lock);
		/* initialize worker function */
#ifdef USE_THREAD_IRQ
        dev_info(this->pdev, "sx932x use thread IRQ\n", this->irq);
        err = request_threaded_irq(this->irq, NULL, sx932x_irq_isr,
             IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
             IRQ_NAME, this);
        this->irq_disabled = 0;
#else
        dev_info(this->pdev, "sx932x use hard IRQ\n", this->irq);
        INIT_DELAYED_WORK(&this->dworker, sx932x_worker_func);
        /* initailize interrupt reporting */
        this->irq_disabled = 0;
        err = request_irq(this->irq, sx93XX_irq, IRQF_TRIGGER_FALLING,
                          this->pdev->driver->name, this);
#endif
		if (err) {
			dev_err(this->pdev, "irq %d busy?\n", this->irq);
			return err;
		}
		dev_info(this->pdev, "registered with irq (%d)\n", this->irq);
	}
	return -ENOMEM;
}
