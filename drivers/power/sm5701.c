/*
 * sm5701.c  --  SiliconMitus SM5701 Charger Driver
 *
 *  Copyright (C) 2014 SiliconMitus
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
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
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/mfd/sprd_sm5701_core.h>
#include <linux/power/sm5701.h>

#define SM5701_DEBUG_FS


static struct i2c_client *this_client;
static struct sm5701_platform_data *pdata; 

static int sm5701_id = 0;
static int otg_enable = 0;

static int led_ready_state = 0;
static int led_state_charger = LED_DISABLE;

extern void SM5701_set_bstout(int bstout_mv);


void sm5701_init(void)
{
	BYTE reg_data = 0;
	BYTE status1 = 0;
    	
	SM5701_reg_read(this_client,SM5701_DEVICE_ID, &sm5701_id);
	pr_info("%s: SM5701 Charger init, CHIP REV : %2d !! \n",__func__, sm5701_id);

	SM5701_reg_read(this_client,SM5701_VBUSCNTL, &reg_data);
	reg_data &= ~SM5701_VBUSCNTL_AICLEN;
	SM5701_reg_write(this_client,SM5701_VBUSCNTL, reg_data);

	SM5701_reg_read(this_client,SM5701_STATUS1, &status1);
	pr_info("%s : SM5701_STATUS1 : 0x%02x\n", __func__, status1);

/* NOBAT, Enable OVP, UVLO, VBUSOK interrupts */
	reg_data = 0xDC;//0x63
	SM5701_reg_write(this_client,SM5701_INTMASK1, reg_data);

/* Mask CHGON, FASTTMROFFM */
	reg_data = 0xFF;//0xFC
	SM5701_reg_write(this_client,SM5701_INTMASK2, reg_data);

/* Set OVPSEL to 6.35V
	SM5701_reg_read(SM5701_CNTL, &reg_data);
	reg_data |= 0x1A;
	SM5701_reg_write(SM5701_CNTL, reg_data);
*/

	/* Operating Frequency in PWM BUCK mode : 2.4KHz */
	SM5701_reg_read(this_client,SM5701_CNTL, &reg_data);
	reg_data &= ~0xC0;
	reg_data |= (FREQ_24 | 0x4);
	SM5701_reg_write(this_client,SM5701_CNTL, reg_data);

	/* Disable AUTOSTOP */
	SM5701_reg_read(this_client,SM5701_CHGCNTL1, &reg_data);
	reg_data &= ~SM5701_CHGCNTL1_AUTOSTOP;
	SM5701_reg_write(this_client,SM5701_CHGCNTL1, reg_data);


}

void sm5701_Reset32sTimer_ap(void)
{
	pr_info("%s: no need\n", __func__);
}


static int sm5701_get_battery_present(void)
{
	BYTE data = 0;

	SM5701_reg_read(this_client,SM5701_STATUS1, &data);

	pr_info("%s: SM5701_STATUS1 (0x%02x)\n", __func__, data);

	data = ((data & SM5701_STATUS1_NOBAT) >> SM5701_STATUS1_NOBAT_SHIFT);

	return !data;
}

int sm5701_GetCharge_Stat(void)
{
	int status = CHARGER_READY;
	int nCHG;
	BYTE stat1,stat2, chg_en, cln;

	SM5701_reg_read(this_client,SM5701_STATUS1, &stat1);

	SM5701_reg_read(this_client,SM5701_STATUS2, &stat2);


//	Clear interrupt register
	SM5701_reg_read(this_client,SM5701_INT1, &cln);
	SM5701_reg_read(this_client,SM5701_INT2, &cln);

	SM5701_reg_read(this_client,SM5701_CNTL, &chg_en);
	chg_en &= SM5701_CNTL_OPERATIONMODE;

	pr_info("%s : SM5701_STATUS2 : 0x%02x,SM5701 mode:0x%02x\n", __func__, stat2,chg_en);
	//nCHG = gpio_get_value(charger->pdata->chg_gpio_en);

	if((stat2 & SM5701_STATUS2_DONE) || (stat2 & SM5701_STATUS2_TOPOFF)) {
		status = CHARGER_DONE;
		pr_info("%s : Status, charger Full \n", __func__);
	} else if((stat2 & SM5701_STATUS2_CHGON) || (stat1 & SM5701_STATUS1_CHGRSTF)){
		status = CHARGER_DOING;
		pr_info("%s : Status, charging \n", __func__);
	} else if((stat1 & SM5701_STATUS1_NOBAT) || (stat1 & SM5701_STATUS1_VBUSOVP) || (stat1 & SM5701_STATUS1_VBUSUVLO)) {
		status = CHARGER_FAULT;
		pr_info("%s : Status, charger fault \n", __func__);
	} else {
		status = CHARGER_READY;
		pr_info("%s : Status, charger ready \n", __func__);
	}

	return (int)status;
}

EXPORT_SYMBOL_GPL(sm5701_GetCharge_Stat);


static int sm5701_set_batreg_voltage( int batreg_voltage)
{
	BYTE data = 0;

	SM5701_reg_read(this_client,SM5701_CHGCNTL3, &data);

	data &= ~BATREG_MASK;

	if (sm5701_id < 3) {
		if ((batreg_voltage*10) < 40125)
			data = 0x00;
		else if ((batreg_voltage*10) > 44000)
			batreg_voltage = 4400;
		else
			data = ((batreg_voltage*10 - 40125) / 125);
	} else {
		if (batreg_voltage <= 3725)
			data = 0x0;
		else if (batreg_voltage >= 4400)
			data = 0x1F;
		else if (batreg_voltage <= 4300)
			data = (batreg_voltage - 3725) / 25;
		else
			data = (batreg_voltage - 4300) * 10 / 125 + 23;
	}

	SM5701_reg_write(this_client,SM5701_CHGCNTL3, data);

	SM5701_reg_read(this_client,SM5701_CHGCNTL3, &data);
	pr_info("%s : SM5701_CHGCNTL3 (Battery regulation voltage) : 0x%02x\n",__func__, data);

	return data;
}

static int sm5701_set_vbuslimit_current(int input_current)
{
	BYTE data = 0, temp = 0;
	SM5701_reg_read(this_client,SM5701_VBUSCNTL, &data);
	data &= ~SM5701_VBUSCNTL_VBUSLIMIT;

	if (input_current >= 1200)
		input_current = 1200;

	if(input_current <= 100)
		data &= ~SM5701_VBUSCNTL_VBUSLIMIT;
	else if(input_current <= 500)
		data |= 0x01;
	else {
		temp = (input_current/100)-5;
		data |= temp;
	}

	SM5701_reg_write(this_client,SM5701_VBUSCNTL, data);

	SM5701_reg_read(this_client,SM5701_VBUSCNTL, &data);
	pr_info("%s : SM5701_VBUSCNTL (Input current limit) : 0x%02x\n",__func__, data);

	return data;
}

static int sm5701_set_topoff(int topoff_current)
{
	BYTE data = 0;

	SM5701_reg_read(this_client,SM5701_CHGCNTL1, &data);

	data &= ~SM5701_CHGCNTL1_TOPOFF;

	if(topoff_current < 100)
		topoff_current = 100;
	else if (topoff_current > 475)
		topoff_current = 475;

	data |= (topoff_current - 100) / 25;

	SM5701_reg_write(this_client,SM5701_CHGCNTL1, data);

	SM5701_reg_read(this_client,SM5701_CHGCNTL1, &data);
	pr_info("%s : SM5701_CHGCNTL1 (Top-off current threshold) : 0x%02x\n",__func__, data);

	return data;
}


static int sm5701_set_fastchg_current(int fast_charging_current)
{
	BYTE data = 0;

	if(fast_charging_current < 100)
		fast_charging_current = 100;
	else if (fast_charging_current > 1600)
		fast_charging_current = 1600;

	data = (fast_charging_current - 100) / 25;

	SM5701_reg_write(this_client,SM5701_CHGCNTL2, data);

	SM5701_reg_read(this_client,SM5701_CHGCNTL2, &data);
	pr_info("%s : SM5701_CHGCNTL2 (fastchg current) : 0x%02x\n",__func__, data);

	return data;
}


static int sm5701_toggle_charger(int enable)
{
	BYTE chg_en = 0, mask = 0;

	SM5701_reg_read(this_client,SM5701_CNTL, &chg_en);

	mask = sm5701_id < 3 ? OP_MODE_CHG_ON : OP_MODE_CHG_ON_REV3;
	chg_en &= ~mask;

	if ((led_state_charger == LED_DISABLE) || (sm5701_id != 4)) {
		if (enable)
			chg_en |= mask;

		SM5701_reg_write(this_client,SM5701_CNTL, chg_en);
		//gpio_direction_output((charger->pdata->chg_gpio_en), !enable);//need to check if gpio en should set in charger code or pinmap

		pr_info("%s: SM5701 Charger toggled!! \n", __func__);

		SM5701_reg_read(this_client,SM5701_CNTL, &chg_en);
		pr_info("%s : CNTL register (0x09) : 0x%02x\n", __func__, chg_en);

	} else {
		 SM5701_set_operationmode(SM5701_OPERATIONMODE_FLASH_ON);
		pr_info("%s: SM5701 Charger toggled!! - flash on!! \n", __func__);
		//gpio_direction_output((charger->pdata->chg_gpio_en), !enable);//need to check if gpio en should set in charger code or pinmap
	}

	return chg_en;
}



/******************************************************************************
* Function: sm5701_OTG_Enable 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
void sm5701_OTG_Enable(int enable)
{
	pr_info("sw5701 OTG enable\n");
    	
	SM5701_set_bstout(SM5701_BSTOUT_5P0);

	if(enable){
		SM5701_set_operationmode(SM5701_OPERATIONMODE_OTG_ON);
 	}else{
		sm5701_init();
	}
}


EXPORT_SYMBOL_GPL(sm5701_OTG_Enable);



/******************************************************************************
* Function: SM5701_TA_StartCharging 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
void sm5701_TA_StartCharging(void)
{
	pr_info("pengwei enter TA charging sm5701\n");

	sm5701_set_vbuslimit_current(1000);
	sm5701_set_topoff(150);
	sm5701_set_fastchg_current(700);

    	sm5701_toggle_charger(1);
}
EXPORT_SYMBOL_GPL(sm5701_TA_StartCharging);

/******************************************************************************
* Function: SM5701_USB_StartCharging 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
void sm5701_USB_StartCharging(void)
{
	pr_info("pengwei enter USB charging sm5701\n");

	sm5701_set_vbuslimit_current(500);
	sm5701_set_topoff(150);
	sm5701_set_fastchg_current(500);

    	sm5701_toggle_charger(1);
}
EXPORT_SYMBOL_GPL(sm5701_USB_StartCharging);

/******************************************************************************
* Function: 	
* Parameters: None
* Return: None
*
* Description:	
*
******************************************************************************/
void sm5701_StopCharging(void)
{
	pr_info("pengwei STOP charging sm5701\n");

    sm5701_toggle_charger(0);
}
EXPORT_SYMBOL_GPL(sm5701_StopCharging);

/******************************************************************************
* Function: SM5701_Monitor	
* Parameters: None
* Return: status 
*
* Description:	enable the host procdessor to monitor the status of the IC.
*
******************************************************************************/
/*
sm5701_monitor_status sm5701_Monitor(void)
{
    sm5701_monitor_status status;
	
    status = (sm5701_monitor_status)SM5701_reg_read(SM5701_REG_MONITOR);

	switch(status){
		case SM5701_MONITOR_NONE:
			break;
		case SM5701_MONITOR_CV:
			// if need something to do, add
			break;
		case SM5701_MONITOR_VBUS_VALID:
			// if need something to do, add
			break;
		case SM5701_MONITOR_IBUS:
			// if need something to do, add
			break;
		case SM5701_MONITOR_ICHG:
			// if need something to do, add
			break;
		case SM5701_MONITOR_T_120:
			// if need something to do, add
			break;
		case SM5701_MONITOR_LINCHG:
			// if need something to do, add
			break;
		case SM5701_MONITOR_VBAT_CMP:
			// if need something to do, add
			break;
		case SM5701_MONITOR_ITERM_CMP:
			// if need something to do, add
			break;
		default :
			break;
			
		}

	return status;
}
EXPORT_SYMBOL_GPL(sm5701_Monitor);
*/


#ifdef SM5701_DEBUG_FS

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
	pr_info("fan54015 set reg = %d value = %d\n",reg, val);
	ret = SM5701_reg_write(this_client,reg, val);
	
	if (ret < 0){
		pr_info("set_regs_store error\n");
		return -EINVAL;
		}

	return count;
}


static ssize_t dump_regs_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
 	const int regaddrs[] = {0x06, 0x07, 0x08, 0x09, 0xa, 0x0b, 0x0c, 0xd, 0xe, 0xf, 0x11, 0x12, 0x13, 0x14 };
	const char str[] = "0123456789abcdef";
	BYTE sm5701_regs[0x60];

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
		sm5701_read_reg(usb_sm5701_chg->client, i, &sm5701_regs[i]);
	}
	sm5701_read_reg(usb_sm5701_chg->client, 0x10, &sm5701_regs[0x10]);

#else
	for ( i = 0; i<14; i++) {
		SM5701_reg_read(this_client,regaddrs[i],&sm5701_regs[i]);
	}
//	SM5701_reg_read(sm5701_regs[0x10]);
#endif
	

	for (i=0; i<ARRAY_SIZE(regaddrs); i++) {

		val = sm5701_regs[i];
        	buf[3*i] = str[(val&0xf0)>>4];
		buf[3*i+1] = str[val&0x0f];
		buf[3*i+1] = str[val&0x0f];
	}
	
	return 0x60;
}


static DEVICE_ATTR(dump_regs, S_IRUGO | S_IWUSR, dump_regs_show, NULL);
static DEVICE_ATTR(set_regs, S_IRUGO | S_IWUSR, NULL, set_regs_store);
#endif

static void sm5701_start_type(int type)
{
	if(type){
		sm5701_TA_StartCharging();
		}
	else{
		sm5701_USB_StartCharging();
		}
}

struct sprd_ext_ic_operations sprd_extic_op ={
	.ic_init = sm5701_init,
	.charge_start_ext = sm5701_start_type,
	.charge_stop_ext = sm5701_StopCharging,
	.get_charging_status = sm5701_GetCharge_Stat,
	.timer_callback_ext = sm5701_Reset32sTimer_ap,
	.otg_charge_ext = sm5701_OTG_Enable,
};

const struct sprd_ext_ic_operations *sprd_get_ext_ic_ops(void){
	return &sprd_extic_op;
}

extern int sprd_otg_enable_power_on(void);


#ifdef CONFIG_OF
static int sec_chg_dt_init(struct device_node *np,
			 struct device *dev,
			 struct SM5701_dt_platform_data *pdata)
{
	int ret = 0, len = 0;
	unsigned int chg_irq_attr = 0;
	int chg_gpio_en = 0;
	int chg_irq_gpio = 0;

	if (!np)
		return -EINVAL;

	chg_gpio_en = of_get_named_gpio(np, "chgen-gpio", 0);
	if (chg_gpio_en < 0) {
		pr_err("%s: of_get_named_gpio failed: %d\n", __func__,
							chg_gpio_en);
		return chg_gpio_en;
	}

	ret = gpio_request(chg_gpio_en, "chgen-gpio");
	if (ret) {
		pr_err("%s gpio_request failed: %d\n", __func__, chg_gpio_en);
		return ret;
	}

	chg_irq_gpio = of_get_named_gpio(np, "chgirq-gpio", 0);
	if (chg_irq_gpio < 0) 
		pr_err("%s: chgirq gpio get failed: %d\n", __func__, chg_irq_gpio);

	ret = gpio_request(chg_irq_gpio, "chgirq-gpio");
	if (ret)
		pr_err("%s gpio_request failed: %d\n", __func__, chg_irq_gpio);

	pdata->chg_gpio_en = chg_gpio_en;
	if (chg_irq_gpio) {
		pdata->chg_irq_attr = chg_irq_attr;
		pdata->chg_irq = gpio_to_irq(chg_irq_gpio);
	}
	
	return ret;
}
#endif

#ifdef CONFIG_OF
static struct of_device_id SM5701_charger_match_table[] = {
        { .compatible = "sm,sm5701-charger",},
        {},
};
#else
#define SM5701_charger_match_table NULL
#endif /* CONFIG_OF */

static int SM5701_charger_probe(struct platform_device *pdev)
{
	struct SM5701_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct SM5701_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct SM5701_charger_data *charger;
	int ret = 0;

	pr_info("%s: SM5701 Charger Probe Start\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	if (pdev->dev.parent->of_node) {
		pdev->dev.of_node = of_find_compatible_node(
			of_node_get(pdev->dev.parent->of_node), NULL,
			SM5701_charger_match_table[0].compatible);
    }

	charger->SM5701 = iodev;
	charger->SM5701->dev = &pdev->dev;

    if (pdev->dev.of_node) {
		charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
		if (!charger->pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
		ret = sec_chg_dt_init(pdev->dev.of_node, &pdev->dev, charger->pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else
		charger->pdata = pdata->charger_data;

	platform_set_drvdata(pdev, charger);
	this_client = charger->SM5701->i2c;
	charger->siop_level = 100;

	charger->input_curr_limit_step = 500;
	charger->charging_curr_step= 25;
	
	sm5701_init();
	if(sprd_otg_enable_power_on()){
		pr_info("enable OTG mode when power on\n");
		sm5701_OTG_Enable(1);
	}

    	SM5701_set_charger_data(charger);
#if 0
	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK(&charger->isr_work, SM5701_isr_work);

		ret = request_threaded_irq(charger->pdata->chg_irq,
				NULL, SM5701_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				/* charger->pdata->chg_irq_attr, */
				"charger-irq", charger);
		if (ret) {
			dev_err(&pdev->dev,
					"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_request_irq;
		}

		ret = enable_irq_wake(charger->pdata->chg_irq);
		if (ret < 0)
			dev_err(&pdev->dev,
					"%s: Failed to Enable Wakeup Source(%d)\n",
					__func__, ret);
	}
#endif
	pr_info("%s: SM5701 Charger Probe Loaded\n", __func__);
	return 0;

err_request_irq:
err_power_supply_register:
	pr_info("%s: err_power_supply_register error \n", __func__);
err_parse_dt:
	pr_info("%s: err_parse_dt error \n", __func__);
err_parse_dt_nomem:
	pr_info("%s: err_parse_dt_nomem error \n", __func__);
	kfree(charger);
	return ret;
}

static int SM5701_charger_remove(struct platform_device *pdev)
{
	struct SM5701_charger_data *charger =
				platform_get_drvdata(pdev);
	kfree(charger);

	return 0;
}

bool SM5701_charger_suspend(struct SM5701_charger_data *charger)
{
	pr_info("%s: CHARGER - SM5701(suspend mode)!!\n", __func__);
	return true;
}

bool SM5701_charger_resume(struct SM5701_charger_data *charger)
{
	pr_info("%s: CHARGER - SM5701(resume mode)!!\n", __func__);
	return true;
}

static void SM5701_charger_shutdown(struct device *dev)
{
	struct SM5701_charger_data *charger =
				dev_get_drvdata(dev);

	pr_info("%s: SM5701 Charger driver shutdown\n", __func__);
	if (!charger->SM5701->i2c) {
		pr_err("%s: no SM5701 i2c client\n", __func__);
		return;
	}
}

static const struct platform_device_id SM5701_charger_id[] = {
	{ "sm5701-charger", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, SM5701_charger_id);

static struct platform_driver SM5701_charger_driver = {
	.driver = {
		.name = "sm5701-charger",
		.owner = THIS_MODULE,
		.of_match_table = SM5701_charger_match_table,
		.shutdown = SM5701_charger_shutdown,
	},
	.probe = SM5701_charger_probe,
	.remove = SM5701_charger_remove,
	.id_table = SM5701_charger_id,
};

static int __init SM5701_charger_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&SM5701_charger_driver);
}

subsys_initcall(SM5701_charger_init);

static void __exit SM5701_charger_exit(void)
{
	platform_driver_unregister(&SM5701_charger_driver);
}

module_exit(SM5701_charger_exit);


MODULE_DESCRIPTION("SILICONMITUS SM5701 Charger Driver");
MODULE_AUTHOR("SPRD PENG.HU <peng.hu@spreadtrum.com>");
MODULE_LICENSE("GPL");
