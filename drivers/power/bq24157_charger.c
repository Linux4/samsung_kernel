
/*
 *  bq24157_charger.c
 *  charger driver  
 *
 *
 *  Copyright (C) 2011, Samsung Electronics
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */ 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/gpio-pxa.h>
#include <mach/bq24157_charger.h>
#include <linux/mfd/88pm80x.h>
#ifdef CONFIG_BATTERY_D2199
#include <linux/d2199/d2199_battery.h>
#endif
#if defined(CONFIG_SPA)
#include <mach/spa.h>
static void (*spa_external_event)(int, int) = NULL;
struct bq24157_platform_data *pdata_current_limit = NULL;
#endif

static struct i2c_client *bq24157_client = NULL;
static unsigned int bq24157_chg_en = 0;
static int bq24157_is_ovp = 0;

static int bq24157_i2c_write(int length, int reg , unsigned char *values)
{
	struct i2c_client *client = bq24157_client;
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}


static int bq24157_i2c_read(int length, int reg , unsigned char *values)
{
	struct i2c_client *client = bq24157_client;
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int bq24157_read_chip_info(void)
{
	unsigned char data = 0;	
	int ret = 0;

	ret = bq24157_i2c_read(1, BQ24157_VENDER_PART_REVISION, &data);
	if(ret < 0)
	{
		printk("  %s bq24157_i2c_read failed %d\n", __func__, ret);
		return ret;
	}

	printk("  %s chip info 0x%x\n", __func__, data);

	return 0;
}


static int bq24157_set_bit(unsigned int reg_num, unsigned char mask, unsigned char bit)
{
	unsigned char data = 0;
	int ret = 0;
	
	ret = bq24157_i2c_read(1, reg_num, &data);
	if(ret < 0)
	{
		printk("  %s bq24157_i2c_read failed %d\n", __func__, ret);
		return ret;
	}

	data &= ~mask;
	data |= (mask & bit); 

	ret = bq24157_i2c_write(1, reg_num, &data);
	if(ret < 0)
	{
		printk("  %s bq24157_i2c_read failed %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int bq24157_set_battery_voltage(unsigned char voltage)
{
	int ret = 0;
    
	ret = bq24157_set_bit(BQ24157_BATTERY_VOLTAGE, VO_REG, voltage);
	if(ret < 0)
	{
		printk("  %s bq24157_set_bit failed\n", __func__);
		return ret; 
	}

	printk("  %s voltage : %x\n", __func__, VOLTAGE_4350MV);

	return 0;
}

static int bq24157_set_safety_limit_voltage(unsigned char voltage)
{
	int ret = 0;

	ret = bq24157_set_bit(BQ24157_SAFETY_LIMIT, LIMIT_VOLTAGE_CURRENT_MASK, voltage);
	if(ret < 0)
	{
		printk("  %s bq24157_set_bit failed\n", __func__);
		return ret; 
	}

	return 0;
}

static int bq24157_set_charger_current_limit(unsigned char current_limit)
{
	int ret = 0;

	ret = bq24157_set_bit(BQ24157_CONTROL, LIN_LIMIT, current_limit);
	if(ret < 0)
	{
		printk("  %s bq24157_set_bit failed\n", __func__);
		return ret; 
	}

	return 0;
}

static int bq24157_set_weak_battery_voltage(unsigned char voltage)
{
	int ret = 0;

	ret = bq24157_set_bit(BQ24157_CONTROL, V_LOWV, voltage);
	if(ret < 0)
	{
		printk("  %s bq24157_set_bit failed\n", __func__);
		return ret; 
	}

	return 0;
}

static int bq24157_set_charge_termination_enable(unsigned char enable)
{
	int ret = 0;

	ret = bq24157_set_bit(BQ24157_CONTROL, BQ_TE, enable);
	if(ret < 0)
	{
		printk("  %s bq24157_set_bit failed\n", __func__);
		return ret;
	}

	return 0;
}

static int bq24157_set_low_chg_current(unsigned char low_chg)
{
	int ret = 0;
	ret = bq24157_set_bit(BQ24157_CHARGER_VOLTAGE, LOW_CHG, low_chg);
	if(ret < 0)
	{
		printk("  %s bq24157_set_bit failed\n", __func__);
		return ret;
	}
	
	return 0;
}

static int bq24157_set_charge_termination_current(unsigned char termination_current)
{
	int ret = 0;
	ret = bq24157_set_bit(BQ24157_BATTERY_TERMINATION, VI_TERM, termination_current);
	if(ret < 0)
	{
		printk("  %s bq24157_set_bit failed\n", __func__);
		return ret;
	}
	
	return 0;
}

static int bq24157_set_charge_current(unsigned char charge_current)
{
        int ret = 0;
        ret = bq24157_set_bit(BQ24157_BATTERY_TERMINATION, VI_CHRG, charge_current);
        if(ret < 0)
        {
                printk("  %s bq24157_set_bit failed\n", __func__);
                return ret;
        }

        return 0;
}


#if defined(CONFIG_SPA)
static void bq24157_set_current_limit_work(struct work_struct *work)
{
	struct bq24157_platform_data *pdata = pdata_current_limit;
	int ret = 0;

	printk("  %s \n", __func__);

	ret = bq24157_set_charger_current_limit(NO_LIMIT); //set charge current limit
	if(ret < 0)
	{
		printk("  %s bq24157_set_charger_current_limit failed %d\n", __func__, ret);
		return ;
	}
	wake_unlock(&pdata->set_current_limit_wakelock);
}


static void bq24157_enable_charge(enum power_supply_type type)
{
	int ret = 0;
	unsigned char chg_current = 0;
#if 0
	struct bq24157_platform_data *pdata = pdata_current_limit;
#endif
	printk("#############################%s\n", __func__);

	if(type == POWER_SUPPLY_TYPE_MAINS)
	{
#ifdef CONFIG_MACH_WILCOX
		chg_current = VI_CHRG_1050MA;
#else
		chg_current = VI_CHRG_650MA;
#endif
#ifdef CONFIG_BATTERY_D2199
#ifdef CONFIG_MACH_WILCOX
		d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 1050);
#else
		d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 650);
#endif
#endif
	}
	else
	{
	     chg_current = VI_CHRG_550MA;
#ifdef CONFIG_BATTERY_D2199
		 d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 550);
#endif
	}

	ret = bq24157_set_charge_current(chg_current);
	if(ret < 0)
	{
		printk("  %s bq24157_set_charge_current failed %d\n", __func__, ret);
	}

#if 0
	ret = bq24157_set_charger_current_limit(USB_100MA); //set charge current limit
	if(ret < 0)
	{
		printk("  %s bq24157_set_charger_current_limit failed %d\n", __func__, ret);
	}
	else
	{
		if(delayed_work_pending(&pdata->set_current_limit_work))
		{
			wake_unlock(&pdata->set_current_limit_wakelock);
			cancel_delayed_work_sync(&pdata->set_current_limit_work);
		}

		wake_lock(&pdata->set_current_limit_wakelock);
		schedule_delayed_work(&pdata->set_current_limit_work, BQ24157_SET_CURRENT_LIMIT_DELAY);
	}

#if 1
	chg_current = 0;
	bq24157_i2c_read(1, BQ24157_BATTERY_VOLTAGE, &chg_current);	
#endif
#endif
	gpio_direction_output(bq24157_chg_en, 0);

	return; 	
}

static void bq24157_disable_charge(unsigned char end_of_charge)
{
	switch(end_of_charge)
	{
		case SPA_END_OF_CHARGE_NONE:
			bq24157_is_ovp = 0;
			gpio_direction_output(bq24157_chg_en, 1);
#ifdef CONFIG_BATTERY_D2199
			d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 0);
#endif
			break;
		case SPA_END_OF_CHARGE_BY_FULL:
		case SPA_END_OF_CHARGE_BY_TEMPERATURE:
		case SPA_END_OF_CHARGE_BY_TIMER:
		case SPA_END_OF_CHARGE_BY_VF_OPEN:
		case SPA_END_OF_CHARGE_BY_QUICKSTART:
			gpio_direction_output(bq24157_chg_en, 1);
#ifdef CONFIG_BATTERY_D2199
			d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 0);
#endif
			break;
		case SPA_END_OF_CHARGE_BY_OVP:
			bq24157_is_ovp = 1;
			break;
	}

	return; 	
}
#endif

static int bq24157_init_data(void)
{
	int ret = 0; 
    
#ifdef CONFIG_MACH_WILCOX    
	ret = bq24157_set_safety_limit_voltage(LIMIT_4400MV_1350MA);
#else
	ret = bq24157_set_safety_limit_voltage(LIMIT_4340MV_1150MA);
#endif
	if(ret < 0)
	{
		printk("  %s bq24157_set_safety_limit_voltage failed %d\n", __func__, ret);
		return ret;
	}
#ifdef CONFIG_MACH_WILCOX
	ret = bq24157_set_battery_voltage(VOLTAGE_4350MV); //set max battery voltage to 4.35V
#else
	ret = bq24157_set_battery_voltage(VOLTAGE_4350MV); //set max battery voltage to 4.35V
#endif
	if(ret < 0)
	{
		printk("  %s bq24157_set_battery_voltage failed %d\n", __func__, ret);
		return ret;
	}

	ret = bq24157_set_charger_current_limit(NO_LIMIT); //set charge current limit
	if(ret < 0)
	{
		printk("  %s bq24157_set_charger_current_limit failed %d\n", __func__, ret);
		return ret;
	}

	ret = bq24157_set_weak_battery_voltage(WEAK_BATTERY_3400MV);
	if(ret < 0)
	{
		printk("  %s bq24157_set_weak_battery_voltage failed %d\n", __func__, ret);
		return ret;
	}

	ret = bq24157_set_charge_current(VI_CHRG_550MA);
	if(ret < 0)
	{
		printk("  %s bq24157_set_charge_current failed %d\n", __func__, ret);
		return ret;
	}

	ret = bq24157_set_low_chg_current(NORMAL_CHG_CURRENT);
	if(ret < 0)
	{
		printk("  %s bq24157_set_log_chg_current failed %d\n", __func__, ret);
		return ret;	
	}

	ret = bq24157_set_charge_termination_current(VI_TERM_100MA);
	if(ret < 0)
	{
		printk("  %s bq24157_set_charge_termination_current failed %d\n", __func__, ret);
		return ret;
	}

	ret = bq24157_set_charge_termination_enable(TE_ENABLE);
	if(ret < 0)
	{
		printk("  %s bq24157_set_charge_termination_enable failed %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void bq24157_stat_irq_work(struct work_struct *work)
{
	struct bq24157_platform_data *pdata = container_of(work, struct bq24157_platform_data, stat_irq_work);

	if(delayed_work_pending(&pdata->stat_irq_delayed_work))
	{
		printk("%s cancel_delayed_work_sync\n",__func__);
		wake_unlock(&pdata->stat_irq_wakelock);
		cancel_delayed_work_sync(&pdata->stat_irq_delayed_work);
	}

	wake_lock(&pdata->stat_irq_wakelock);
	schedule_delayed_work(&pdata->stat_irq_delayed_work, BQ24157_STAT_IRQ_DELAY);
}

static void bq24157_stat_irq_delayed_work(struct work_struct *work)
{
	struct bq24157_platform_data *pdata = container_of(work, struct bq24157_platform_data, stat_irq_delayed_work.work);
	unsigned char data = 0;
	int ret = 0;
	int stat = gpio_get_value(pxa_irq_to_gpio(bq24157_client->irq))? 1 : 0; 
	if((bq24157_is_ovp==1) && (stat == 0))
	{
		bq24157_is_ovp = 0;	
#if defined(CONFIG_SPA)
                if(spa_external_event)
		{
			spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_OVP_CHARGE_RESTART);
		}
#endif
		goto ovp_restart;
	}

	ret = bq24157_i2c_read(1, BQ24157_STATUS_CONTROL, &data);
	if(ret < 0)
	{		 
		goto err_read_status_control;
	}	 
	switch(data & STAT)
	{
		case STAT_READY:
			break;
		case STAT_INPROGRESS:
			break;
		case STAT_CHARGE_DONE:
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				if(bq24157_is_ovp==1)
				{
					printk("%s STAT_CHARGE_DONE but bq24157_is_ovp is set\n", __func__);
					bq24157_is_ovp=0;
					gpio_direction_output(bq24157_chg_en, 1);
					spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_OVP_CHARGE_RESTART);
				}
				else
				spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_CHARGE_FULL);
			}
#endif
			break;
		case STAT_FAULT:
			switch(data & FAULT)
			{
				case FAULT_NORMAL:
					break;
				case FAULT_VBUS_OVP:
#if defined(CONFIG_SPA)
		                        if(spa_external_event)
					{
						bq24157_is_ovp = 1;
						spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_OVP_CHARGE_STOP);
					}
#endif
					break;
				case FAULT_SLEEP_MODE:
					break;
				case FAULT_BAD_ADAPTOR:
					break;
				case FAULT_OUTPUT_OVP:
					break;
				case FAULT_THEMAL_SHUTDOWN:
					break;
				case FAULT_TIMER_FAULT:
					break;
				case FAULT_NO_BATTERY:
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}

ovp_restart:
err_read_status_control:
	wake_unlock(&pdata->stat_irq_wakelock);
}

#ifdef CONFIG_BATTERY_D2199
extern int d2199_extern_read_temperature(int *tbat);
#endif


static irqreturn_t bq24157_stat_irq(int irq, void *dev_id)
{
	struct bq24157_platform_data *pdata = dev_id;

	schedule_work(&pdata->stat_irq_work);
	return IRQ_HANDLED;	
}

static int __devinit bq24157_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq24157_platform_data *pdata = client->dev.platform_data;

	/*First check the functionality supported by the host*/
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
		printk("  %s functionality check failed 1 \n", __func__);
		return -EIO;
	}
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)) {
		printk("  %s functionality check failed 2 \n", __func__);
		return -EIO;
	}

	if(bq24157_client == NULL)
	{
		bq24157_client = client; 
	}
	else
	{
		printk("  %s bq24157_client is not NULL. bq24157_client->name : %s\n", __func__, bq24157_client->name);
		ret = -ENXIO;
		goto err_bq24157_client_is_not_NULL;
	}

#if defined(CONFIG_SPA)
	spa_external_event = spa_get_external_event_handler();
#endif

	ret = bq24157_read_chip_info();
	if(ret)
	{
		printk("  %s fail to read chip info\n", __func__);
		goto err_read_chip_info;
	}

	ret = bq24157_init_data();
	if(ret)
	{
		printk("  %s fail to init data\n", __func__);
		goto err_init_data;
	}

	if(pdata->cd == 0)
	{
		printk("  %s please assign cd pin GPIO\n", __func__);
		ret = -1;
		goto err_gpio_request_cd;
	}

	ret = gpio_request(pdata->cd, "bq24157_CD");
        if(ret)
        {
                dev_err(&client->dev,"bq24157: Unable to get gpio %d\n", pdata->cd);
                goto err_gpio_request_cd;
        }
	bq24157_chg_en = pdata->cd;

	INIT_WORK(&pdata->stat_irq_work, bq24157_stat_irq_work);
	INIT_DELAYED_WORK(&pdata->stat_irq_delayed_work, bq24157_stat_irq_delayed_work);
	wake_lock_init(&pdata->stat_irq_wakelock, WAKE_LOCK_SUSPEND, "bq24157_stat_irq");	
	
#if 0
	INIT_DELAYED_WORK(&pdata->set_current_limit_work, bq24157_set_current_limit_work);
	wake_lock_init(&pdata->set_current_limit_wakelock, WAKE_LOCK_SUSPEND, "bq24157_current_limit");	

	pdata_current_limit = pdata;
#endif
#if defined(CONFIG_SPA)
        ret = spa_chg_register_enable_charge(bq24157_enable_charge);
        if(ret)
        {
                printk("%s fail to register enable_charge function\n", __func__);
                goto err_register_enable_charge;
        }
        ret = spa_chg_register_disable_charge(bq24157_disable_charge);
        if(ret)
        {
                printk("%s fail to register disable_charge function\n", __func__);
                goto err_register_disable_charge;
        }
        spa_external_event = spa_get_external_event_handler();
#endif

#ifndef CONFIG_BATTERY_D2199
	ret = spa_bat_register_read_temperature(pm80x_read_temperature);
	if(ret)
	{
		printk("%s fail to register spa_bat_register_read_temperature function\n", __func__);
	}
#endif

	if(client->irq){
                printk("  %s irq : %d\n", __func__, client->irq);

		/* check init status */ 
		if((gpio_get_value(pxa_irq_to_gpio(bq24157_client->irq))? 1 : 0) == 1)
		{
			wake_lock(&pdata->stat_irq_wakelock);
			schedule_delayed_work(&pdata->stat_irq_delayed_work, 0);
		}

                ret = gpio_request(pxa_irq_to_gpio(client->irq), "bq24157_stat");
                if(ret)
                {
                        printk("  %s gpio_request failed\n", __func__);
                        goto err_gpio_request_bq24157_stat;
                }
                gpio_direction_input(pxa_irq_to_gpio(client->irq));
                ret = request_irq(client->irq, bq24157_stat_irq, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "bq24157_stat", pdata);
                if(ret)
                {
                        printk("  %s request_irq failed\n", __func__);
                        goto err_request_irq_bq24157_stat;
                }
        }

	return 0;

err_request_irq_bq24157_stat:
	gpio_free(MMP_TRQ_TO_GPIO(client->irq));
err_gpio_request_bq24157_stat:
#if defined(CONFIG_SPA)
	spa_chg_unregister_disable_charge(bq24157_disable_charge);
err_register_disable_charge:
	spa_chg_unregister_enable_charge(bq24157_enable_charge);
err_register_enable_charge:
#endif
err_gpio_request_cd:
err_init_data:
err_read_chip_info:
#if defined(CONFIG_SPA)
	spa_external_event = NULL; 
#endif
	bq24157_client = NULL;
err_bq24157_client_is_not_NULL:
	return ret;
}

static int __devexit bq24157_remove(struct i2c_client *client)
{
	return 0;
}

static void bq24157_shutdown(struct i2c_client *client)
{
	return;
}

static int bq24157_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int bq24157_resume(struct i2c_client *client)
{
	return 0;
}

/* Every chip have a unique id */
static const struct i2c_device_id bq24157_id[] = {
	{ "bq24157_6A", 0 },
	{ }
};

/* Every chip have a unique id and we need to register this ID using MODULE_DEVICE_TABLE*/
MODULE_DEVICE_TABLE(i2c, bq24157_id);


static struct i2c_driver bq24157_i2c_driver = {
	.driver = {
		.name = "bq24157_charger",
	},
	.probe		= bq24157_probe,
	.remove		= __devexit_p(bq24157_remove),
	.shutdown	= bq24157_shutdown, 
	.suspend	= bq24157_suspend,
	.resume		= bq24157_resume,
	.id_table	= bq24157_id,
};

static int __init bq24157_init(void)
{
	int ret;
	if ((ret = i2c_add_driver(&bq24157_i2c_driver)) < 0) {
		printk(KERN_ERR "%s i2c_add_driver failed.\n", __func__);
	}

	return ret;
}
module_init(bq24157_init);

static void __exit bq24157_exit(void)
{
	i2c_del_driver(&bq24157_i2c_driver);
}
module_exit(bq24157_exit);

MODULE_AUTHOR("YongTaek Lee <ytk.lee@samsung.com>");
MODULE_DESCRIPTION("Linux Driver for charger");
MODULE_LICENSE("GPL");
