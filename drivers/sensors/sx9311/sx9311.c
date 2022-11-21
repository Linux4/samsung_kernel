/*! \file sx9311.c
 * \brief  SX9311 Driver
 *
 * Driver for the SX9311 
 * Copyright (c) 2011 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define DRIVER_NAME "sx9311"

#define MAX_WRITE_ARRAY_SIZE 32

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
//#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/sort.h> 
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "sx9311.h" 	/* main struct, interrupt,init,pointers */

#define IDLE			0
#define S_PROX			1
#define S_BODY			2


#define MAIN_SENSOR		1 //CS1

/* Failer Index */
#define SX9311_ID_ERROR 	1
#define SX9311_NIRQ_ERROR	2
#define SX9311_CONN_ERROR	3
#define SX9311_I2C_ERROR	4

/*HS60 code for AR-ZQL1695-01000000041 by zhuqiang at 2019/7/24 start*/
extern char *sar_name;
/*HS60 code for AR-ZQL1695-01000000041 by zhuqiang at 2019/7/24 end*/

/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 end*/
static bool enable_flags = 1;
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 end*/

/*! \struct sx9311
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct sx9311
{
	pbuttonInformation_t pbuttonInformation;
	psx9311_platform_data_t hw;		/* specific platform data settings */
} sx9311_t, *psx9311_t;

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
	char buffer[2] = {0};
	int returnValue = 0;

	buffer[0] = address;
	buffer[1] = value;
	returnValue = -ENOMEM;

	if (this && this->bus) {
		i2c = this->bus;
		returnValue = i2c_master_send(i2c,buffer,2);
		dev_info(&i2c->dev,"write_register Address: 0x%x Value: 0x%x Return: %d\n",
														address,value,returnValue);
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

//static int sx9311_set_mode(psx93XX_t this, unsigned char mode);

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
		if (read_register(this,SX9311_IRQSTAT_REG,&data) == 0)
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
	s32 returnValue = 0;
	returnValue = write_register(this,SX9311_IRQSTAT_REG,0xFF);
	return returnValue;
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
	read_register(this,SX9311_IRQSTAT_REG,&reg_value);
	return sprintf(buf, "%d\n", reg_value);
}

/*! \brief sysfs store function for manual calibration
 */
static ssize_t manual_offset_calibration_store(struct device *dev,
			struct device_attribute *attr,const char *buf, size_t count)
{
	unsigned long val = 0;
	psx93XX_t this = dev_get_drvdata(dev);
	if (kstrtoul(buf, 0, &val))
	return -EINVAL;
	if (val) {
		dev_info( this->pdev, "Performing manual_offset_calibration()\n");
		manual_offset_calibration(this);
	}
	return count;
}

static int sx9311_Hardware_Check(psx93XX_t this)
{
	int ret = 0;
	u8 failcode = 0;
	u8 loop = 0;
	this->failStatusCode = 0;
	
	//Check th IRQ Status
	while(this->get_nirq_low && this->get_nirq_low()){
		read_regStat(this);
		msleep(100);
		if(++loop >10){
			this->failStatusCode = SX9311_NIRQ_ERROR;
			break;
		}
	}
	
	//Check I2C Connection
	ret = read_register(this, SX9311_WHOAMI_REG, &failcode);
	if(ret < 0){
		this->failStatusCode = SX9311_I2C_ERROR;
	}
		
	if(failcode != SX9311_WHOAMI_VALUE){
		this->failStatusCode = SX9311_ID_ERROR;
	}
	
	dev_info(this->pdev, "sx9311 chip_id = 0x%x, failcode = 0x%x\n", failcode, this->failStatusCode);
	return (int)this->failStatusCode;
}


/*********************************************************************/
static int sx9311_global_variable_init(psx93XX_t this)
{
	this->irq_disabled = 0;
	this->failStatusCode = 0;
	this->reg_in_dts = true;
	return 0;
}

static ssize_t sx9311_register_write_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int reg_address = 0, val = 0;
	psx93XX_t this = dev_get_drvdata(dev);

	if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
		pr_err("[SX9311]: %s - The number of data are wrong\n",__func__);
		return -EINVAL;
	}

	write_register(this, (unsigned char)reg_address, (unsigned char)val);
	pr_info("[SX9311]: %s - Register(0x%x) data(0x%x)\n",__func__, reg_address, val);

	return count;
}
//read registers not include the advanced one
static ssize_t sx9311_register_read_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val=0;
	int regist = 0;
	psx93XX_t this = dev_get_drvdata(dev);

	dev_info(this->pdev, "Reading register\n");

	if (sscanf(buf, "%x", &regist) != 1) {
		pr_err("[SX9311]: %s - The number of data are wrong\n",__func__);
		return -EINVAL;
	}

	read_register(this, regist, &val);
	pr_info("[SX9311]: %s - Register(0x%2x) data(0x%2x)\n",__func__, regist, val);

	return count;
}

static void read_rawData(psx93XX_t this)
{
	u8 msb = 0, lsb = 0;
	u8 csx = 0;
	s32 useful = 0;
	s32 average = 0;
	s32 diff = 0;
	u16 offset = 0;
	if(this){
		for(csx =0; csx<3; csx++){
			write_register(this,SX9311_CPSRD,csx);//here to check the CS1, also can read other channel		
			read_register(this,SX9311_USEMSB,&msb);
			read_register(this,SX9311_USELSB,&lsb);
			useful = (s32)((msb << 8) | lsb);
			
			read_register(this,SX9311_AVGMSB,&msb);
			read_register(this,SX9311_AVGLSB,&lsb);
			average = (s32)((msb << 8) | lsb);
			
			read_register(this,SX9311_DIFFMSB,&msb);
			read_register(this,SX9311_DIFFLSB,&lsb);
			diff = (s32)((msb << 8) | lsb);
			
			read_register(this,SX9311_OFFSETMSB,&msb);
			read_register(this,SX9311_OFFSETLSB,&lsb);
			offset = (u16)((msb << 8) | lsb);
			if (useful > 32767)
				useful -= 65536;
			if (average > 32767)
				average -= 65536;
			if (diff > 4095)
				diff -= 8192;
			dev_info(this->pdev, " [CS: %d] Useful = %d Average = %d, DIFF = %d Offset = %d \n",csx,useful,average,diff,offset);
		}
	}
}

static ssize_t sx9311_raw_data_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	psx93XX_t this = dev_get_drvdata(dev);
	read_rawData(this);
	return 0;
}

/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 start*/
/* Sensor disable and enable control and report state
 * Enable: state=1  report state based on sensor
 * Disable:state=0, report IDLE state not related to sensor
*/
static void EnableDisableSarSensor(psx93XX_t this, int state)
{
	int counter = 0;
	u8 i = 0;
	int numberOfButtons = 0;
	psx9311_t pDevice = NULL;
	struct _buttonInfo *buttons = NULL;
	struct input_dev *input = NULL;

	struct _buttonInfo *pCurrentButton  = NULL;

	if (this && (pDevice = this->pDevice))
	{
		dev_info(this->pdev, "Inside EnableDisable()\n");
		read_register(this, SX9311_STAT0_REG, &i);
		dev_info(this->pdev, "read_register i = %d\n", i);

		buttons = pDevice->pbuttonInformation->buttons;
		input = pDevice->pbuttonInformation->input;
		numberOfButtons = pDevice->pbuttonInformation->buttonSize;

		if (unlikely( (buttons==NULL) || (input==NULL) )) {
			dev_err(this->pdev, "ERROR!! buttons or input NULL!!!\n");
			return;
		}

		for (counter = 0; counter < numberOfButtons; counter++)
		{
			pCurrentButton = &buttons[counter];
			if (pCurrentButton==NULL) {
				dev_err(this->pdev,"ERROR!! current button at index: %d NULL!!!\n", counter);
				return; // ERRORR!!!!
			}
			if(state == 0)
			{
				#ifdef HQ_FACTORY_BUILD
				input_report_key(input, KEY_SAR_FAR, 1);
				input_report_key(input, KEY_SAR_FAR, 0);
				#else
				input_report_abs(input, ABS_DISTANCE, 5);
				#endif
				pCurrentButton->state = IDLE;
				dev_info(this->pdev, "cap Button %d IDLE.\n",counter);
			}
			else if (counter == 1)
			{
				if ((i & pCurrentButton->mask) == pCurrentButton->mask) {
					dev_info(this->pdev, "cap button %d S_BODY\n", counter);
					#ifdef HQ_FACTORY_BUILD
					input_report_key(input, KEY_SAR_CLOSE, 1);
					input_report_key(input, KEY_SAR_CLOSE, 0);
					#else
					input_report_abs(input, ABS_DISTANCE, 0);
					#endif
					pCurrentButton->state = S_BODY;
				} else if((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x0F)){
					dev_info(this->pdev, "cap button %d S_PROX\n", counter);
					#ifdef HQ_FACTORY_BUILD
					input_report_key(input, KEY_SAR_BODY, 1);
					input_report_key(input, KEY_SAR_BODY, 0);
					#else
					input_report_abs(input, ABS_DISTANCE, 10);
					#endif
					pCurrentButton->state = S_PROX;
				}else {
					dev_info(this->pdev, "cap button %d IDLE\n", counter);
					#ifdef HQ_FACTORY_BUILD
					input_report_key(input, KEY_SAR_FAR, 1);
					input_report_key(input, KEY_SAR_FAR, 0);
					#else
					input_report_abs(input, ABS_DISTANCE, 5);
					#endif
					pCurrentButton->state = IDLE;
					dev_info(this->pdev, "cap Button %d IDLE.\n",counter);
				}
			}
		}
		input_sync(input);
	}
    //dev_info(this->pdev, "Leaving EnableDisable()\n");
}

static ssize_t sx9311_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	pr_info("[SX9311]: %s enable_flags = %d\n",__func__, enable_flags);
	return sprintf(buf, "%d\n", enable_flags);
}

static ssize_t sx9311_enable_store (struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	psx93XX_t this = dev_get_drvdata(dev);

	if (sscanf(buf, "%x", &val) != 1) {
		pr_err("[SX9311]: %s - The number of data are wrong\n",__func__);
		return -EINVAL;
	}

	if (val == 0)
	{
		enable_flags = 0;
		disable_irq(this->irq);
		write_register(this,SX9311_IRQ_ENABLE_REG,0x00);
		write_register(this,SX9311_CPS_CTRL0_REG,0x83);//400ms check
		EnableDisableSarSensor(this, 0);
	}
	if (val == 1)
	{
		enable_flags = 1;
		enable_irq(this->irq);
		write_register(this,SX9311_IRQ_ENABLE_REG,0xE0);
		/*HS60 code for HS60-352 by zhuqiang at 2019/08/06 start*/
		write_register(this,SX9311_CPS_CTRL0_REG,0x27);//30ms check
		/*HS60 code for HS60-352 by zhuqiang at 2019/08/06 end*/
		EnableDisableSarSensor(this, 1);
	}
	pr_info("[SX9311]: %s - enable_flags = %d\n",__func__, enable_flags);
	return count;
}

static DEVICE_ATTR(manual_calibrate, 0664, manual_offset_calibration_show,manual_offset_calibration_store);
static DEVICE_ATTR(register_write,  0664, NULL,sx9311_register_write_store);
static DEVICE_ATTR(register_read,0664, NULL,sx9311_register_read_store);
static DEVICE_ATTR(raw_data,0664,sx9311_raw_data_show,NULL);
static DEVICE_ATTR(enable, 0664, sx9311_enable_show, sx9311_enable_store);
static struct attribute *sx9311_attributes[] = {
	&dev_attr_manual_calibrate.attr,
	&dev_attr_register_write.attr,
	&dev_attr_register_read.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_enable.attr,
	NULL,
};
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 end*/

static struct attribute_group sx9311_attr_group = {
	.attrs = sx9311_attributes,
};

/**************************************/

/*! \brief  Initialize I2C config from platform data
 * \param this Pointer to main parent struct 
 */
static void sx9311_reg_init(psx93XX_t this)
{
	psx9311_t pDevice = 0;
	psx9311_platform_data_t pdata = 0;
	int i = 0;
	/* configure device */ 
	dev_info(this->pdev, "Going to Setup I2C Registers\n");
	if (!this->pDevice)
	{
		dev_err(this->pdev, "ERROR! pDevice is NULL\n");
		return;
	}
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
			while ( i < ARRAY_SIZE(sx9311_i2c_reg_setup)) {
				/* Write all registers/values contained in i2c_reg */
				dev_info(this->pdev, "Going to Write Reg: 0x%x Value: 0x%x\n",
				sx9311_i2c_reg_setup[i].reg,sx9311_i2c_reg_setup[i].val);
				write_register(this, sx9311_i2c_reg_setup[i].reg,sx9311_i2c_reg_setup[i].val);
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
	int ret = 0;
	if (this) {
		pr_info("SX9311 income initialize\n");
		/* prepare reset by disabling any irq handling */
		this->irq_disabled = 1;
		disable_irq(this->irq);
		/* perform a reset */
		write_register(this,SX9311_SOFTRESET_REG,SX9311_SOFTRESET);
		/* wait until the reset has finished by monitoring NIRQ */
		dev_info(this->pdev, "Sent Software Reset. Waiting until device is back from reset to continue.\n");
		/* just sleep for awhile instead of using a loop with reading irq status */
		msleep(100);
		
		ret = sx9311_global_variable_init(this);
		
		sx9311_reg_init(this);
		msleep(100); /* make sure everything is running */
		manual_offset_calibration(this);

		/* re-enable interrupt handling */
		enable_irq(this->irq);
		/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
		enable_irq_wake(this->irq);
		/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/
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
	psx9311_t pDevice = NULL;
	struct _buttonInfo *buttons = NULL;
	struct input_dev *input = NULL;

	struct _buttonInfo *pCurrentButton  = NULL;

	if (this && (pDevice = this->pDevice))
	{
		read_register(this, SX9311_STAT0_REG, &i);

		buttons = pDevice->pbuttonInformation->buttons;
		input = pDevice->pbuttonInformation->input;
		numberOfButtons = pDevice->pbuttonInformation->buttonSize;

		if (unlikely( (buttons==NULL) || (input==NULL) )) {
			dev_err(this->pdev, "ERROR!! buttons or input NULL!!!\n");
			return;
		}

		for (counter = 0; counter < numberOfButtons; counter++) {//循环更新所有通道的状态
			pCurrentButton = &buttons[counter]; //counter代表的是当前通道号
			if (pCurrentButton==NULL) {
				dev_err(this->pdev,"ERROR!! current button at index: %d NULL!!!\n", counter);
				return; // ERRORR!!!!
			}
			/*HS60 code for AR-ZQL1695-01000000070 by zhuqiang at 2019/7/18 start*/
			switch (pCurrentButton->state) {//IDLE：远离态， S_PROX： 1级靠近， S_BODY：2级靠近
				case IDLE: /* Button is not being touched! 当前通道的上一个状态是远离 */
					if ((i & pCurrentButton->mask) == pCurrentButton->mask) { //根据键值判断当前通道新的状态是2级靠近
						dev_info(this->pdev, "cap button %d S_BODY\n", counter);
						#ifdef HQ_FACTORY_BUILD
						input_report_key(input, KEY_SAR_CLOSE, 1);
						input_report_key(input, KEY_SAR_CLOSE, 0);
						#else
						input_report_abs(input, ABS_DISTANCE, 0);
						#endif
						pCurrentButton->state = S_BODY; //更新状态
					} else if((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x0F)){//根据键值判断当前通道新的状态是1级靠近
						dev_info(this->pdev, "cap button %d S_PROX\n", counter);
						#ifdef HQ_FACTORY_BUILD
						input_report_key(input, KEY_SAR_BODY, 1);
						input_report_key(input, KEY_SAR_BODY, 0);
						#else
						input_report_abs(input, ABS_DISTANCE, 10);
						#endif
						pCurrentButton->state = S_PROX;//更新状态
					}
					else { //根据键值判断当前通道新的状态还是远离态，不用更新
						dev_info(this->pdev, "cap Button %d already IDLE.\n",counter);
					}
					break;
				case S_PROX: /* Button is being touched level-1! 此通道的上一个状态是1级靠近 */ 
					if ((i & pCurrentButton->mask) == pCurrentButton->mask) {//根据键值判断当前通道新的状态是2级靠近
						dev_info(this->pdev, "cap button %d BODY\n",counter);
						#ifdef HQ_FACTORY_BUILD
						input_report_key(input, KEY_SAR_CLOSE, 1);
						input_report_key(input, KEY_SAR_CLOSE, 0);
						#else
						input_report_abs(input, ABS_DISTANCE, 0);
						#endif
						pCurrentButton->state = S_BODY;//更新状态
					} else if((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x0F)){//根据键值判断当前通道新的状态还是1级靠近，不用更新
						dev_info(this->pdev, "cap Button %d still S_PROX.\n",counter);
					} else { //根据键值判断当前通道新的状态是远离态
						dev_info(this->pdev, "cap button %d IDLE\n",counter);
						#ifdef HQ_FACTORY_BUILD
						input_report_key(input, KEY_SAR_FAR, 1);
						input_report_key(input, KEY_SAR_FAR, 0);
						#else
						input_report_abs(input, ABS_DISTANCE, 5);
						#endif
						pCurrentButton->state = IDLE;//更新状态
					}
					break;
				case S_BODY: /* Button is being touched level-2! 此通道的上一个状态是2级靠近*/
					if ((i & pCurrentButton->mask) == pCurrentButton->mask) {	//根据键值判断当前通道新的状态还是2级靠近，不用更新
						dev_info(this->pdev, "cap button %d still BODY\n",counter);
					} else if((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x0F)){	//根据键值判断当前通道新的状态是1级靠近
						dev_info(this->pdev, "cap button %d S_PROX\n", counter);
						#ifdef HQ_FACTORY_BUILD
						input_report_key(input, KEY_SAR_BODY, 1);
						input_report_key(input, KEY_SAR_BODY, 0);
						#else
						input_report_abs(input, ABS_DISTANCE, 10);
						#endif
						pCurrentButton->state = S_PROX; //更新状态
					} else {	//根据键值判断当前通道新的状态是远离态
						dev_info(this->pdev, "cap button %d IDLE\n",counter);
						#ifdef HQ_FACTORY_BUILD
						input_report_key(input, KEY_SAR_FAR, 1);
						input_report_key(input, KEY_SAR_FAR, 0);
						#else
						input_report_abs(input, ABS_DISTANCE, 5);
						#endif
						pCurrentButton->state = IDLE; //更新状态
					}
					/*HS60 code for AR-ZQL1695-01000000070 by zhuqiang at 2019/7/18 end*/
					break;
				default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
					break;
			};
		}
		input_sync(input);
  }
}

static int sx9311_parse_dt(struct sx9311_platform_data *pdata, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;
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
	pr_info("[SX9311]:%s -  size of elements %d \n", __func__,pdata->i2c_reg_num);
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
	pr_info("[SX9311]: %s -[%d] parse_dt complete\n", __func__,pdata->irq_gpio);
	return 0;
}

/* get the NIRQ state (1->NIRQ-low, 0->NIRQ-high) */
static int sx9311_init_platform_hw(struct i2c_client *client)
{
	psx93XX_t this = i2c_get_clientdata(client);
	struct sx9311 *pDevice = NULL;
	struct sx9311_platform_data *pdata = NULL;

	int rc = 0;

	pr_info("[SX9311] : %s init_platform_hw start!",__func__);

	if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw)) {
		if (gpio_is_valid(pdata->irq_gpio)) {
			rc = gpio_request(pdata->irq_gpio, "sx9311_irq_gpio");
			if (rc < 0) {
				dev_err(this->pdev, "SX9311 Request gpio. Fail![%d]\n", rc);
				return rc;
			}
			rc = gpio_direction_input(pdata->irq_gpio);
			if (rc < 0) {
				dev_err(this->pdev, "SX9311 Set gpio direction. Fail![%d]\n", rc);
				return rc;
			}
			this->irq = client->irq = gpio_to_irq(pdata->irq_gpio);
		}
		else {
			dev_err(this->pdev, "SX9311 Invalid irq gpio num.(init)\n");
		}
	}
	else {
		pr_err("[SX9311] : %s - Do not init platform HW", __func__);
	}
	
	pr_err("[SX9311]: %s - sx9311_irq_debug\n",__func__);
	return rc;
}

static void sx9311_exit_platform_hw(struct i2c_client *client)
{
	psx93XX_t this = i2c_get_clientdata(client);
	struct sx9311 *pDevice = NULL;
	struct sx9311_platform_data *pdata = NULL;

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

static int sx9311_get_nirq_state(void)
{
	return  !gpio_get_value(irq_gpio_num);
}

/*! \fn static int sx9311_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * \brief Probe function
 * \param client pointer to i2c_client
 * \param id pointer to i2c_device_id
 * \return Whether probe was successful
 */
static int sx9311_probe(struct i2c_client *client, const struct i2c_device_id *id)
{    
	int err = 0;

	psx93XX_t this = 0;
	psx9311_t pDevice = 0;
	psx9311_platform_data_t pplatData = 0;
	struct totalButtonInformation *pButtonInformationData = NULL;
	struct input_dev *input = NULL;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	dev_info(&client->dev, "sx9311_probe()\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)) {
		dev_err(&client->dev, "Check i2c functionality.Fail!\n");
		return -EIO;
	}

	this = devm_kzalloc(&client->dev,sizeof(sx93XX_t), GFP_KERNEL); /* create memory for main struct */
	dev_info(&client->dev, "Initialized Main Memory: 0x%p\n",this);

	pButtonInformationData = devm_kzalloc(&client->dev , sizeof(struct totalButtonInformation), GFP_KERNEL);
	if (!pButtonInformationData) {
		dev_err(&client->dev, "Failed to allocate memory(totalButtonInformation)\n");
		return -ENOMEM;
	}

	pButtonInformationData->buttonSize = ARRAY_SIZE(psmtcButtons);
	pButtonInformationData->buttons =  psmtcButtons;
	pplatData = devm_kzalloc(&client->dev,sizeof(struct sx9311_platform_data), GFP_KERNEL);
	if (!pplatData) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}
	
	pplatData->vddio = regulator_get(&client->dev, "vddio");
	if (IS_ERR(pplatData->vddio))
	{
		dev_info(&client->dev, "%s: Failed to get vddio regulator\n", __func__);
	}
	else
	{

		err = regulator_enable(pplatData->vddio);
		if (err) {
			regulator_put(pplatData->vddio);
			dev_err(&client->dev, "%s: Error %d enable regulator\n", __func__, err);
		}
		dev_info(&client->dev, "vddio regulator is %s\n", regulator_is_enabled(pplatData->vddio) ? "on" : "off");
	}

	pplatData->vdd = regulator_get(&client->dev, "vdd");
	if (IS_ERR(pplatData->vdd)) {
		dev_info(&client->dev, "%s: Failed to get vdd regulator\n", __func__);
	} 
	else
	{
		err = regulator_enable(pplatData->vdd);

		if (err) {
			regulator_put(pplatData->vdd);
			dev_err(&client->dev, "%s: Error %d enable regulator\n", __func__, err);
		}
		dev_info(&client->dev, "vdd regulator is %s\n", regulator_is_enabled(pplatData->vdd) ? "on" : "off");
	}


	pplatData->get_is_nirq_low = sx9311_get_nirq_state;
	pplatData->pbuttonInformation = pButtonInformationData;
 
	client->dev.platform_data = pplatData; 
	err = sx9311_parse_dt(pplatData, &client->dev);
	if (err) {
		dev_err(&client->dev, "could not setup pin\n");
		return ENODEV;
	}

	pplatData->init_platform_hw = sx9311_init_platform_hw;
	dev_err(&client->dev, "SX9311 init_platform_hw done!\n");
	
	if (this){
		dev_info(&client->dev, "SX9311 initialize start!!");
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
		if (MAX_NUM_STATUS_BITS>= 2)
		{
			this->statusFunc[0] = touchProcess; /* RELEASE_STAT */
			this->statusFunc[1] = touchProcess; /* TOUCH_STAT  */
		}

		/* setup i2c communication */
		this->bus = client;
		i2c_set_clientdata(client, this);

        /*HS60 code for AR-ZQL1695-01000000041 by zhuqiang at 2019/7/24 start*/
		err = sx9311_Hardware_Check(this);
		if (err > 0)
		{
			/* HS60 code for HS60-352 by zhuqiang at 2019/08/27 start */
			regulator_disable(pplatData->vddio);
			regulator_put(pplatData->vddio);
			regulator_disable(pplatData->vdd);
			regulator_put(pplatData->vdd);
			dev_err(&client->dev, "sx9311 whoami check fail, free vdd/vddio regulator source!\n");
			/* HS60 code for HS60-352 by zhuqiang at 2019/08/27 end */
			return err;
		}
		sar_name = "sx9311";
		/*HS60 code for AR-ZQL1695-01000000041 by zhuqiang at 2019/7/24 end*/
		/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
		wakeup_source_init(&this->sx9311_wakelock, "sar-sx9311");
		/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/
		/* record device struct */
		this->pdev = &client->dev;

		/* create memory for device specific struct */
		this->pDevice = pDevice = devm_kzalloc(&client->dev,sizeof(sx9311_t), GFP_KERNEL);
		dev_info(&client->dev, "\t Initialized Device Specific Memory: 0x%p\n",pDevice);

		if (pDevice){
			/* for accessing items in user data (e.g. calibrate) */
			err = sysfs_create_group(&client->dev.kobj, &sx9311_attr_group);
			//sysfs_create_group(client, &sx9311_attr_group);

			/* Add Pointer to main platform data struct */
			pDevice->hw = pplatData;

			/* Check if we hava a platform initialization function to call*/
			if (pplatData->init_platform_hw)
			pplatData->init_platform_hw(client);

			/* Initialize the button information initialized with keycodes */
			pDevice->pbuttonInformation = pplatData->pbuttonInformation;
			/* Create the input device */
			input = input_allocate_device();
			if (!input) {
				return -ENOMEM;
			}
			/* Set all the keycodes */

			/*HS60 code for AR-ZQL1695-01000000070 by zhuqiang at 2019/7/18 start*/
			#ifdef HQ_FACTORY_BUILD
			__set_bit(EV_KEY, input->evbit);
			__set_bit(KEY_SAR_FAR, input->keybit);
			__set_bit(KEY_SAR_BODY, input->keybit);
			__set_bit(KEY_SAR_CLOSE, input->keybit);
			#else
			__set_bit(EV_ABS, input->evbit);
			input_set_abs_params(input, ABS_DISTANCE, -1, 100, 0, 0);
			#endif
			/*HS60 code for AR-ZQL1695-01000000070 by zhuqiang at 2019/7/18 end*/

			/* save the input pointer and finish initialization */
			pButtonInformationData->input = input;
			input->name = "sx9311_sar";
			input->id.bustype = BUS_I2C;
			if(input_register_device(input)){
				return -ENOMEM;
			}
		}

		sx93XX_IRQ_init(this);
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

	pplatData->exit_platform_hw = sx9311_exit_platform_hw;

	dev_info(&client->dev, "sx9311_probe() Done\n");

	return 0;
}

/*! \fn static int sx9311_remove(struct i2c_client *client)
 * \brief Called when device is to be removed
 * \param client Pointer to i2c_client struct
 * \return Value from sx93XX_remove()
 */
//static int __devexit sx9311_remove(struct i2c_client *client)
static int sx9311_remove(struct i2c_client *client)
{
	psx9311_platform_data_t pplatData =0;
	psx9311_t pDevice = 0;
	psx93XX_t this = i2c_get_clientdata(client);
	if (this && (pDevice = this->pDevice))
	{
		input_unregister_device(pDevice->pbuttonInformation->input);

		sysfs_remove_group(&client->dev.kobj, &sx9311_attr_group);
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
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
static int sx9311_suspend(struct device *dev)
{
	if (enable_flags)
	{
		psx93XX_t this = dev_get_drvdata(dev);
		sx93XX_suspend(this);
		write_register(this,SX9311_CPS_CTRL0_REG,0x83);//400ms check
	}
	return 0;
}
/***** Kernel Resume *****/
static int sx9311_resume(struct device *dev)
{
	if (enable_flags)
	{
		psx93XX_t this = dev_get_drvdata(dev);
		sx93XX_resume(this);
		write_register(this,SX9311_CPS_CTRL0_REG,0x27);//30ms check
	}
	return 0;
}
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/
/*====================================================*/
#else
#define sx9311_suspend		NULL
#define sx9311_resume		NULL
#endif /* CONFIG_PM */

static struct i2c_device_id sx9311_idtable[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sx9311_idtable);
#ifdef CONFIG_OF
static struct of_device_id sx9311_match_table[] = {
	{ .compatible = "Semtech,sx9311",},
	{ },
};
#else
#define sx9311_match_table NULL
#endif
static const struct dev_pm_ops sx9311_pm_ops = {
	.suspend = sx9311_suspend,
	.resume = sx9311_resume,
};
static struct i2c_driver sx9311_driver = {
	.driver = {
		.owner			= THIS_MODULE,
		.name			= DRIVER_NAME,
		.of_match_table	= sx9311_match_table,
		.pm				= &sx9311_pm_ops,
	},
	.id_table		= sx9311_idtable,
	.probe			= sx9311_probe,
	.remove			= sx9311_remove,
};
static int __init sx9311_I2C_init(void)
{
	return i2c_add_driver(&sx9311_driver);
}
static void __exit sx9311_I2C_exit(void)
{
	i2c_del_driver(&sx9311_driver);
}

module_init(sx9311_I2C_init);
module_exit(sx9311_I2C_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX9311 Capacitive Touch Controller Driver");
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
			/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
			__pm_wakeup_event(&this->sx9311_wakelock, 500);
			sx93XX_schedule_work(this,0);
			/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/
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
		dev_dbg(this->pdev, "Worker - Refresh Status %d\n",status);
		
		while(counter < 2) { /* counter start from MSB */
			if (((status>>counter) & 0x20) && (this->statusFunc[counter])) {
				dev_info(this->pdev, "SX9311 Function Pointer Found. Calling\n");
				this->statusFunc[counter](this);
			}
			counter ++;
		}
		if (unlikely(this->useIrqTimer && nirqLow))
		{	/* Early models and if RATE=0 for newer models require a penup timer */
			/* Queue up the function again for checking on penup */
			sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
		}
	} else {
		printk(KERN_ERR "sx93XX_worker_func, NULL work_struct\n");
	}
}

static int sx93XX_remove(psx93XX_t this)
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
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
static void sx93XX_suspend(psx93XX_t this)
{
	if (0)
	{
		dev_info(this->pdev, "Enter sx93XX_suspend\n");
		disable_irq(this->irq);
		write_register(this,SX9311_CPS_CTRL0_REG,0x83);//400ms check
	}
}
static void sx93XX_resume(psx93XX_t this)
{
	if (0) {
		dev_info(this->pdev, "Enter sx93XX_resume\n");
		enable_irq(this->irq);
		sx93XX_schedule_work(this,0);
		write_register(this,SX9311_CPS_CTRL0_REG,0x27);//30ms check
	}
}
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/
static int sx93XX_IRQ_init(psx93XX_t this)
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
