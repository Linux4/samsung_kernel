/*
 * fan5405.c  --  FAN5405 Switching Charger driver
 *
 * Copyright (C) 2012 Fairchild semiconductor Co.Ltd
 * Author: Bright Yang <bright.yang@fairchildsemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/param.h>
#include <linux/stat.h>

#include "fan5405.h"

#define FAN5405_DEBUG_FS


/******************************************************************************
* Register addresses	
******************************************************************************/
#define FAN5405_REG_CONTROL0	              0
#define FAN5405_REG_CONTROL1	              1
#define FAN5405_REG_OREG	              2
#define FAN5405_REG_IC_INFO                   3
#define FAN5405_REG_IBAT	              4
#define FAN5405_REG_SP_CHARGER                5
#define FAN5405_REG_SAFETY                    6
#define FAN5405_REG_MONITOR                  16

/******************************************************************************
* Register bits	
******************************************************************************/
/* FAN5405_REG_CONTROL0 (0x00) */
#define FAN5405_FAULT                   (0x07)
#define FAN5405_FAULT_SHIFT                  0	
#define FAN5405_BOOST              (0x01 << 3) 
#define FAN5405_BOOST_SHIFT                  3
#define FAN5405_STAT               (0x3 <<  4) 
#define FAN5405_STAT_SHIFT                   4
#define FAN5405_EN_STAT            (0x01 << 6)
#define FAN5405_EN_STAT_SHIFT                6
#define FAN5405_TMR_RST_OTG        (0x01 << 7)  // writing a 1 resets the t32s timer, writing a 0 has no effect
#define FAN5405_TMR_RST_OTG_SHIFT            7

/* FAN5405_REG_CONTROL1 (0x01) */
#define FAN5405_OPA_MODE                (0x01)
#define FAN5405_OPA_MODE_SHIFT               0
#define FAN5405_HZ_MODE            (0x01 << 1)
#define FAN5405_HZ_MODE_SHIFT                1  
#define FAN5405_CE_N               (0x01 << 2)
#define FAN5405_CE_N_SHIFT                   2 
#define FAN5405_TE                 (0x01 << 3)
#define FAN5405_TE_SHIFT                     3
#define FAN5405_VLOWV              (0x03 << 4)
#define FAN5405_VLOWV_SHIFT                  4
#define FAN5405_IINLIM             (0x03 << 6)
#define FAN5405_IINLIM_SHIFT                 6

/* FAN5405_REG_OREG (0x02) */
#define FAN5405_OTG_EN                  (0x01)
#define FAN5405_OTG_EN_SHIFT                 0
#define FAN5405_OTG_PL             (0x01 << 1)
#define FAN5405_OTG_PL_SHIFT                 1
#define FAN5405_OREG               (0x3f << 2)
#define FAN5405_OREG_SHIFT                   2

/* FAN5405_REG_IC_INFO (0x03) */
#define FAN5405_REV                     (0x03)
#define FAN5405_REV_SHIFT                    0
#define FAN5405_PN                 (0x07 << 2)
#define FAN5405_PN_SHIFT                     2
#define FAN5405_VENDOR_CODE        (0x07 << 5)
#define FAN5405_VENDOR_CODE_SHIFT            5

/* FAN5405_REG_IBAT (0x04) */
#define FAN5405_ITERM                   (0x07)
#define FAN5405_ITERM_SHIFT                  0
#define FAN5405_IOCHARGE           (0x07 << 4)
#define FAN5405_IOCHARGE_SHIFT               4
#define FAN5405_RESET              (0x01 << 7)
#define FAN5405_RESET_SHIFT                  7

/* FAN5405_REG_SP_CHARGER (0x05) */
#define FAN5405_VSP                     (0x07)
#define FAN5405_VSP_SHIFT                    0
#define FAN5405_EN_LEVEL           (0x01 << 3)
#define FAN5405_EN_LEVEL_SHIFT               3
#define FAN5405_SP                 (0x01 << 4)
#define FAN5405_SP_SHIFT                     4
#define FAN5405_IO_LEVEL           (0x01 << 5)
#define FAN5405_IO_LEVEL_SHIFT               5
#define FAN5405_DIS_VREG           (0x01 << 6)
#define FAN5405_DIS_VREG_SHIFT               6

/* FAN5405_REG_SAFETY (0x06) */
#define FAN5405_VSAFE                   (0x0f)
#define FAN5405_VSAFE_SHIFT                  0
#define FAN5405_ISAFE              (0x07 << 4)
#define FAN5405_ISAFE_SHIFT                  4

/* FAN5405_REG_MONITOR (0x10) */
#define FAN5405_CV                      (0x01)
#define FAN5405_CV_SHIFT                     0
#define FAN5405_VBUS_VALID         (0x01 << 1)
#define FAN5405_VBUS_VALID_SHIFT             1
#define FAN5405_IBUS               (0x01 << 2)
#define FAN5405_IBUS_SHIFT                   2
#define FAN5405_ICHG               (0x01 << 3)
#define FAN5405_ICHG_SHIFT                   3
#define FAN5405_T_120              (0x01 << 4)
#define FAN5405_T_120_SHIFT                  4
#define FAN5405_LINCHG             (0x01 << 5)
#define FAN5405_LINCHG_SHIFT                 5
#define FAN5405_VBAT_CMP           (0x01 << 6)
#define FAN5405_VBAT_CMP_SHIFT               6
#define FAN5405_ITERM_CMP          (0x01 << 7)
#define FAN5405_ITERM_CMP_SHIFT              7

/******************************************************************************
* bit definitions
******************************************************************************/
/********** FAN5405_REG_CONTROL0 (0x00) **********/
// EN_STAT [6]
#define ENSTAT 1
#define DISSTAT 0
// TMR_RST [7]
#define RESET32S 1

/********** FAN5405_REG_CONTROL1 (0x01) **********/
// OPA_MODE [0]
#define CHARGEMODE 0
#define BOOSTMODE 1
//HZ_MODE [1]
#define NOTHIGHIMP 0
#define HIGHIMP 1
// CE/ [2]
#define ENCHARGER 0
#define DISCHARGER 1
// TE [3]
#define DISTE 0
#define ENTE 1
// VLOWV [5:4]
#define VLOWV3P4 0
#define VLOWV3P5 1
#define VLOWV3P6 2
#define VLOWV3P7 3
// IINLIM [7:6]
#define IINLIM100 0
#define IINLIM500 1
#define IINLIM800 2
#define NOLIMIT 3

/********** FAN5405_REG_OREG (0x02) **********/
// OTG_EN [0]
#define DISOTG 0
#define ENOTG 1
// OTG_PL [1]
#define OTGACTIVELOW 0
#define OTGACTIVEHIGH 1
// OREG [7:2]
#define VOREG4P2 35  // refer to table 3

/********** FAN5405_REG_IC_INFO (0x03) **********/

/********** FAN5405_REG_IBAT (0x04) **********/
// ITERM [2:0] - 68mOhm
#define ITERM49 0
#define ITERM97 1
#define ITERM146 2
#define ITERM194 3
#define ITERM243 4
#define ITERM291 5
#define ITERM340 6
#define ITERM388 7
// IOCHARGE [6:4] - 68mOhm
#define IOCHARGE550 0
#define IOCHARGE650 1
#define IOCHARGE750 2
#define IOCHARGE850 3
#define IOCHARGE1050 4
#define IOCHARGE1150 5
#define IOCHARGE1350 6
#define IOCHARGE1450 7

/********** FAN5405_REG_SP_CHARGER (0x05) **********/
// VSP [2:0] 
#define VSP4P213 0
#define VSP4P293 1
#define VSP4P373 2
#define VSP4P453 3
#define VSP4P533 4
#define VSP4P613 5
#define VSP4P693 6
#define VSP4P773 7
// IO_LEVEL [5]
#define ENIOLEVEL 0
#define DISIOLEVEL 1
// DIS_VREG [6]
#define VREGON 0
#define VREGOFF 1

/********** FAN5405_REG_SAFETY (0x06) **********/
// VSAFE [3:0]
#define VSAFE4P20 0
#define VSAFE4P22 1
#define VSAFE4P24 2
#define VSAFE4P26 3
#define VSAFE4P28 4
#define VSAFE4P30 5
#define VSAFE4P32 6
#define VSAFE4P34 7
#define VSAFE4P36 8
#define VSAFE4P38 9
#define VSAFE4P40 10
#define VSAFE4P42 11
#define VSAFE4P44 12
// ISAFE [6:4] - 68mOhm
#define ISAFE550 0
#define ISAFE650 1
#define ISAFE750 2
#define ISAFE850 3
#define ISAFE1050 4
#define ISAFE1150 5
#define ISAFE1350 6
#define ISAFE1450 7

/* reset the T32s timer every 10 seconds   */
#define T32S_RESET_INTERVAL	10

static struct timer_list t32s_timer;

static const BYTE fan5405_def_reg[17] = {
    0x40,    // #0x00(CONTROL0)
    0x30,    // #0x01(CONTROL1)
    0x0a,    // #0x02(OREG)
    0x84,    // #0x03(IC_INFO)
    0x09,    // #0x04(IBAT) default is 0x89 but writing 1 to IBAT[7] resets charge parameters, except the safety reg, so 
    0x24,    // #0x05(SP_CHARGER)
    0x40,    // #0x06(SAFETY)
    0x00,    // #0x07 - unused
    0x00,    // #0x08 - unused
    0x00,    // #0x09 - unused
    0x00,    // #0x0a - unused
    0x00,    // #0x0b - unused
    0x00,    // #0x0c - unused
    0x00,    // #0x0d - unused
    0x00,    // #0x0e - unused
    0x00,    // #0x0f - unused    
    0x00,    // #0x10(MONITOR)
};

static BYTE fan5405_curr_reg[17];

static struct i2c_client *this_client;
static struct fan5405_platform_data *pdata; 


static int reset_flag = 0;      // use when assert reset
static int write_error_count = 0;

static int fan5405_WriteReg(int reg, int val)
{
    int ret;
	printk("####pengwei writereg reg = %d val = %d\n",reg,val);
	ret = i2c_smbus_write_byte_data(this_client, reg, val);
		
	if (ret < 0)
		{
		write_error_count ++;
		pr_debug("%s: error = %d", __func__, ret);
		}

	if((reset_flag == 1) ||  
		((reg == FAN5405_REG_IBAT) && (val & FAN5405_RESET)))  
	{
	    memcpy(fan5405_curr_reg,fan5405_def_reg,6);  // resets charge paremeters, except the safety register(#6)
	    reset_flag = 0;
	}
	else
	{
	    fan5405_curr_reg[reg] = val;
	}

    return ret;
}

//pengwei added for i2c read error
static int read_err_flag = 0;
static int read_err_count = 0;
static int fan5405_ReadReg(int reg)
{
    int ret;
	
	printk("######fan5405_readreg reg  = %d value =%d\n",reg,ret);
	
	ret = i2c_smbus_read_byte_data(this_client, reg);
		
	if (ret < 0)
		{
		read_err_flag = 1;
		read_err_count ++;
		pr_debug("%s: error = %d", __func__, ret);
		}


    return ret;
}

static void fan5405_SetValue(BYTE reg, BYTE reg_bit,BYTE reg_shift, BYTE val)
{
	BYTE tmp;

	tmp = fan5405_curr_reg[reg] & (~reg_bit);
	printk("test value of i2c fan5405_curr_reg[%d] = 0x%x , temp = %d\n",reg,fan5405_curr_reg[reg],tmp);
	tmp |= (val << reg_shift);
	printk("continue test tmp =0x%x\n",tmp);
	if(reg_bit == FAN5405_RESET)
	{
	    reset_flag = 1;
	}
	
	fan5405_WriteReg(reg,tmp);
}

static BYTE fan5405_GetValue(BYTE reg, BYTE reg_bit, BYTE reg_shift)
{
	BYTE tmp,ret;

	tmp = (BYTE)fan5405_ReadReg(reg);
	ret = (tmp & reg_bit) >> reg_shift;

	return ret;
}




/******************************************************************************
* Function: 	
* Parameters: None
* Return: None
*
* Description:	if VBUS present(charging & boost),write 1 every 10 seconds
*
******************************************************************************/
static void fan5405_Reset32sTimer(unsigned long data)
{
	printk("fan 5405 reset rimer\n");
	printk("#####################################read error times is %d\n,writ error times is %d\n",read_err_count,write_error_count);
    fan5405_SetValue(FAN5405_REG_CONTROL0, FAN5405_TMR_RST_OTG,FAN5405_TMR_RST_OTG_SHIFT, RESET32S);
    mod_timer(&t32s_timer, jiffies + (T32S_RESET_INTERVAL * HZ));
}


/******************************************************************************
* Function: 	
* Parameters: None
* Return: None
*
* Description:	if VBUS present(charging & boost),write 1 every 10 seconds
*
******************************************************************************/

void fan5405_Reset32sTimer_ap(void)
{
	printk("fan 5405 reset rimer for ap\n");
	printk("#####################################read error times is %d\n,writ error times is %d\n",read_err_count,write_error_count);
    fan5405_SetValue(FAN5405_REG_CONTROL0, FAN5405_TMR_RST_OTG,FAN5405_TMR_RST_OTG_SHIFT, RESET32S);
    mod_timer(&t32s_timer, jiffies + (T32S_RESET_INTERVAL * HZ));
}


/******************************************************************************
* Function: FAN5405_Initialization 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
static void fan5405_Initialization(void)
{
    memcpy(fan5405_curr_reg,fan5405_def_reg,sizeof(fan5405_curr_reg));
	printk("pengwei test fan5405init\n");
    /* Initialize timer */
	init_timer(&t32s_timer);
	t32s_timer.function = fan5405_Reset32sTimer;
	t32s_timer.data = 0;
	/* start the timer */
	mod_timer(&t32s_timer, jiffies + (T32S_RESET_INTERVAL * HZ));
	
//reg 6
	fan5405_SetValue(FAN5405_REG_SAFETY, FAN5405_VSAFE,FAN5405_VSAFE_SHIFT, VSAFE4P20);  // VSAFE = 4.20V
	fan5405_SetValue(FAN5405_REG_SAFETY, FAN5405_ISAFE, FAN5405_ISAFE_SHIFT, ISAFE1450);  // ISAFE = 1050mA (68mOhm)
//reg 1
	fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_VLOWV, FAN5405_VLOWV_SHIFT,VLOWV3P4);  // VLOWV = 3.4V
	fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_IINLIM, FAN5405_IINLIM_SHIFT,IINLIM500);  // INLIM = 500mA
//reg 2	
	fan5405_SetValue(FAN5405_REG_OREG, FAN5405_OREG,FAN5405_OREG_SHIFT, VOREG4P2);  //OREG = 4.20V
//reg 5
	fan5405_SetValue(FAN5405_REG_SP_CHARGER, FAN5405_IO_LEVEL,FAN5405_IO_LEVEL_SHIFT, ENIOLEVEL);  //IO_LEVEL is 0. Output current is controlled by IOCHARGE bits.
//reg 5
	fan5405_SetValue(FAN5405_REG_SP_CHARGER, FAN5405_VSP,FAN5405_VSP_SHIFT, VSP4P213);

}

void fan5405_init(void)
{
	//reg 6
		fan5405_SetValue(FAN5405_REG_SAFETY, FAN5405_VSAFE,FAN5405_VSAFE_SHIFT, VSAFE4P20);  // VSAFE = 4.20V
		fan5405_SetValue(FAN5405_REG_SAFETY, FAN5405_ISAFE, FAN5405_ISAFE_SHIFT, ISAFE1450);  // ISAFE = 1050mA (68mOhm)
	//reg 1
		fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_VLOWV, FAN5405_VLOWV_SHIFT,VLOWV3P4);  // VLOWV = 3.4V
		fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_IINLIM, FAN5405_IINLIM_SHIFT,IINLIM500);  // INLIM = 500mA
	//reg 2 
		fan5405_SetValue(FAN5405_REG_OREG, FAN5405_OREG,FAN5405_OREG_SHIFT, VOREG4P2);	//OREG = 4.20V
	//reg 5
		fan5405_SetValue(FAN5405_REG_SP_CHARGER, FAN5405_IO_LEVEL,FAN5405_IO_LEVEL_SHIFT, ENIOLEVEL);  //IO_LEVEL is 0. Output current is controlled by IOCHARGE bits.

	//reg 5
		fan5405_SetValue(FAN5405_REG_SP_CHARGER, FAN5405_VSP,FAN5405_VSP_SHIFT, VSP4P213);
}


/******************************************************************************
* Function: fan5405_OTG_Enable 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
void fan5405_OTG_Enable(int enable)
{
	printk("#######pengwei  OTG enable\n");
    fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_OPA_MODE,FAN5405_OPA_MODE_SHIFT, enable); 

	if(enable){
	fan5405_SetValue(FAN5405_REG_OREG, FAN5405_OTG_EN,FAN5405_OTG_EN_SHIFT, enable);
 	}else{
	fan5405_init();
	}
}


EXPORT_SYMBOL_GPL(fan5405_OTG_Enable);



/******************************************************************************
* Function: FAN5405_TA_StartCharging 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
void fan5405_TA_StartCharging(void)
{
	printk("pengwei enter TA charging fan5405\n");

    fan5405_SetValue(FAN5405_REG_IBAT, FAN5405_IOCHARGE,FAN5405_IOCHARGE_SHIFT, IOCHARGE1450);  //1050mA
    fan5405_SetValue(FAN5405_REG_IBAT, FAN5405_ITERM,FAN5405_ITERM_SHIFT, ITERM97);  //97ma
    fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_IINLIM, FAN5405_IINLIM_SHIFT,NOLIMIT);  // no limit
    fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_TE,FAN5405_TE_SHIFT, ENTE);
    fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_CE_N,FAN5405_CE_N_SHIFT, ENCHARGER);
}
EXPORT_SYMBOL_GPL(fan5405_TA_StartCharging);

/******************************************************************************
* Function: FAN5405_USB_StartCharging 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
void fan5405_USB_StartCharging(void)
{
	printk("pengwei enter USB charging fan5405\n");
    fan5405_SetValue(FAN5405_REG_IBAT, FAN5405_IOCHARGE,FAN5405_IOCHARGE_SHIFT, IOCHARGE550);  //550mA
 	fan5405_SetValue(FAN5405_REG_IBAT, FAN5405_ITERM, FAN5405_ITERM_SHIFT,ITERM97);  //97ma
    fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_IINLIM,FAN5405_IINLIM_SHIFT, IINLIM500);  // limit 500mA (default)   
    fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_TE,FAN5405_TE_SHIFT, ENTE);
    fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_CE_N,FAN5405_CE_N_SHIFT, ENCHARGER);
}
EXPORT_SYMBOL_GPL(fan5405_USB_StartCharging);

/******************************************************************************
* Function: 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
void fan5405_StopCharging(void)
{
	printk("pengwei STOP charging fan5405\n");

    fan5405_SetValue(FAN5405_REG_CONTROL1, FAN5405_CE_N,FAN5405_CE_N_SHIFT, DISCHARGER);
}
EXPORT_SYMBOL_GPL(fan5405_StopCharging);

/******************************************************************************
* Function: FAN5405_Monitor	
* Parameters: None
* Return: status 
*
* Description:	enable the host procdessor to monitor the status of the IC.
*
******************************************************************************/
fan5405_monitor_status fan5405_Monitor(void)
{
    fan5405_monitor_status status;
	
    status = (fan5405_monitor_status)fan5405_ReadReg(FAN5405_REG_MONITOR);

	switch(status){
		case FAN5405_MONITOR_NONE:
			break;
		case FAN5405_MONITOR_CV:
			// if need something to do, add
			break;
		case FAN5405_MONITOR_VBUS_VALID:
			// if need something to do, add
			break;
		case FAN5405_MONITOR_IBUS:
			// if need something to do, add
			break;
		case FAN5405_MONITOR_ICHG:
			// if need something to do, add
			break;
		case FAN5405_MONITOR_T_120:
			// if need something to do, add
			break;
		case FAN5405_MONITOR_LINCHG:
			// if need something to do, add
			break;
		case FAN5405_MONITOR_VBAT_CMP:
			// if need something to do, add
			break;
		case FAN5405_MONITOR_ITERM_CMP:
			// if need something to do, add
			break;
		default :
			break;
			
		}

	return status;
}
EXPORT_SYMBOL_GPL(fan5405_Monitor);

static int stat_last = 0;
int fan5405_GetCharge_Stat(void)
{
	int stat;
	int num ;
	for(num=6;num>=0;num--)
		{
		printk("###### pengwei test getcharge stat fan5405_ReadReg(%d) = 0x%x\n",num,fan5405_ReadReg(num));
		}
	printk("###### pengwei test getcharge stat value 0x10 = 0x%x\n",fan5405_ReadReg(0x10));
	stat = fan5405_GetValue(FAN5405_REG_CONTROL0, FAN5405_STAT, FAN5405_STAT_SHIFT);
//pengwei added for i2c read err
	if (read_err_flag == 1)
		{
		read_err_flag = 0;
		}
	else
		{
		stat_last = stat;
		}
		return stat_last;
}

EXPORT_SYMBOL_GPL(fan5405_GetCharge_Stat);

#ifdef FAN5405_DEBUG_FS

static ssize_t set_regs_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long set_value;
	int reg;  // bit 16:8
	int val;  //bit7:0
	int ret;
	set_value = simple_strtoul(buf, NULL, 16);

	reg = (set_value & 0xff00) >> 8;
	val = set_value & 0xff;
	printk("fan54015 set reg = %d value = %d\n",reg, val);
	ret = fan5405_WriteReg(reg, val);
	
	if (ret < 0){
		printk("set_regs_store error\n");
		return -EINVAL;
		}

	return count;
}


static ssize_t dump_regs_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
 	const int regaddrs[] = {0x00, 0x01, 0x02, 0x03, 0x4, 0x05, 0x06, 0x10 };
	const char str[] = "0123456789abcdef";
	BYTE fan5405_regs[0x60];

	int i = 0, index;
	char val = 0;

	for (i=0; i<0x60; i++) {
		if ((i%3)==2)
			buf[i]=' ';
		else
			buf[i] = 'x';
	}
	buf[0x5d] = '\n';
	buf[0x5e] = 0;
	buf[0x5f] = 0;
#if 0

	for ( i = 0; i<0x07; i++) {
		fan5405_read_reg(usb_fan5405_chg->client, i, &fan5405_regs[i]);
	}
	fan5405_read_reg(usb_fan5405_chg->client, 0x10, &fan5405_regs[0x10]);

#else
	for ( i = 0; i<0x07; i++) {
		fan5405_regs[i] = fan5405_ReadReg(i);
	}
//	fan5405_ReadReg(fan5405_regs[0x10]);
#endif
	

	for (i=0; i<ARRAY_SIZE(regaddrs); i++) {
		index = regaddrs[i];
		val = fan5405_regs[index];
        	buf[3*index] = str[(val&0xf0)>>4];
		buf[3*index+1] = str[val&0x0f];
		buf[3*index+1] = str[val&0x0f];
	}
	
	return 0x60;
}


static DEVICE_ATTR(dump_regs, S_IRUGO | S_IWUSR, dump_regs_show, NULL);
static DEVICE_ATTR(set_regs, S_IRUGO | S_IWUSR, NULL, set_regs_store);
#endif

static void fan54015_start_type(int type)
{
	if(type){
		fan5405_TA_StartCharging();
		}
	else{
		fan5405_USB_StartCharging();
		}
}

struct sprd_ext_ic_operations sprd_extic_op ={
	.ic_init = fan5405_init,
	.charge_start_ext = fan54015_start_type,
	.charge_stop_ext = fan5405_StopCharging,
	.get_charging_status = fan5405_GetCharge_Stat,
	.timer_callback_ext = fan5405_Reset32sTimer_ap,
	.otg_charge_ext = fan5405_OTG_Enable,
};

const struct sprd_ext_ic_operations *sprd_get_ext_ic_ops(void){
	return &sprd_extic_op;
}

extern int sprd_otg_enable_power_on(void);


#ifdef CONFIG_OF
static struct fan5405_platform_data *fan5405_parse_dt(struct device *dev)
{
//	struct fan5405_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Could not allocate struct fan5405_platform_data");
		return NULL;
	}
	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif

static int fan5405_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{

	int rc = 0;
	int err = -1;
	printk("@@@@@@@fan5405_probe\n");
	pdata = client->dev.platform_data;

#ifdef CONFIG_OF
		struct device_node *np = client->dev.of_node;
		if (np && !pdata){
			fan5405_parse_dt(&client->dev);
			if(!pdata){
				err = -ENOMEM;
				goto exit_alloc_platform_data_failed;
			}
			client->dev.platform_data = pdata;
		}
#else

	if (pdata == NULL) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			rc = -ENOMEM;
			pr_err("%s: platform data is NULL\n", __func__);
			goto err_alloc_data_failed;
		}
	}
#endif

	this_client = client;	

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c check functionality error\n", __func__);
		rc = -ENODEV;
		goto check_funcionality_failed;
	}

	fan5405_Initialization();
	if(sprd_otg_enable_power_on()){
		printk("enable OTG mode when power on\n");
		fan5405_OTG_Enable(1);
	}

#ifdef FAN5405_DEBUG_FS	
	device_create_file(&client->dev, &dev_attr_dump_regs);
	device_create_file(&client->dev, &dev_attr_set_regs);
#endif
	printk("@@@@@@@fan5405_probe ok\n");
	return rc;

check_funcionality_failed:

err_alloc_data_failed:
	return rc;	
#ifdef CONFIG_OF
exit_alloc_platform_data_failed:
	return err;
#endif
}
static int fan5405_remove(struct i2c_client *client)
{
	struct fan5405_platform_data *p54013data = i2c_get_clientdata(client);
	kfree(p54013data);
	del_timer(&t32s_timer);

	return 0;
}

static int  fan5405_suspend(struct i2c_client *client, pm_message_t message)
{
	return 0;
}

static int  fan5405_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id fan5405_i2c_id[] = {
	{ "fairchild_fan5405", 0 },
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id fan5405_of_match[] = {
	{.compatible = "fairchild,fairchild_fan5405",},
	{}
};
#endif

static struct i2c_driver fan5405_i2c_driver = {
	.driver = {
		.name = "fairchild_fan5405",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(fan5405_of_match),
#endif
	},
	.probe    = fan5405_probe,
//	.remove   = __devexit_p(fan5405_remove),
	.remove   = fan5405_remove,
	.suspend  = fan5405_suspend,
	.resume	  = fan5405_resume,
	.id_table = fan5405_i2c_id,
};

static __init int fan5405_i2c_init(void)
{
	return i2c_add_driver(&fan5405_i2c_driver);
}

static __exit void fan5405_i2c_exit(void)
{
	i2c_del_driver(&fan5405_i2c_driver);
}

module_init(fan5405_i2c_init);
module_exit(fan5405_i2c_exit);

//MODULE_AUTHOR("Bright Yang<bright.yang@fairchildsemi.com>");
//MODULE_DESCRIPTION("I2C bus driver for FAN5405 Switching Charger");
//MODULE_LICENSE("GPL v2");

