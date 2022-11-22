/*
 *  smb358_charger.c
 *  Samsung SMB358 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
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
 */
#define DEBUG
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/smb358_charger.h>


struct i2c_client * global_client;

static int smb358_write_reg(struct i2c_client *client, u8 reg, u8 data)
{
       int ret;
       u8 buf[2];
       struct i2c_msg msg[1];

       buf[0] = reg;
       buf[1] = data;

       msg[0].addr = client->addr;
       msg[0].flags = 0;
       msg[0].len = 2;
       msg[0].buf = buf;

       ret = i2c_transfer(client->adapter, msg, 1);
       if (ret != 1) {
               printk("\n [smb358] i2c Write Failed (ret=%d) \n", ret);
               return -1;
       }
       
       return ret;
}
static int smb358_read_reg(struct i2c_client *client, u8 reg, u8 *data)
{
       int ret;
       u8 buf[1];
       struct i2c_msg msg[2];

       buf[0] = reg;

        msg[0].addr = client->addr;
        msg[0].flags = 0;
        msg[0].len = 1;
        msg[0].buf = buf;

        msg[1].addr = client->addr;
        msg[1].flags = I2C_M_RD; 
        msg[1].len = 1;
        msg[1].buf = buf;
		
       ret = i2c_transfer(client->adapter, msg, 2);
       if (ret != 2) {
               printk("\n [smb358] i2c Read Failed (ret=%d) \n", ret);
               return -1;
       }
       *data = buf[0];

       return 0;
}

static void smb358_test_read(struct i2c_client *client)
{
	u8 data = 0,reg;

	pr_info("%s\n",__func__);
	for (reg = 0; reg <= 0x0E; reg++) {
		smb358_read_reg(client, reg, &data);
		pr_info("read smb358 addr : 0x%02x data : 0x%02x\n", reg, data);
	}

	for (reg = 0x30; reg <= 0x3F; reg++) {
		smb358_read_reg(client, reg, &data);
		pr_info("read smb358 addr : 0x%02x data : 0x%02x\n", reg, data);
	}	
}
void smb358_stop_chg()
{
	struct i2c_client * client;
	u8 data=0;
	pr_info("%s\n",__func__);	
	client = global_client;
	if( !client) return;

	data = ALLOW_VOLATILE_WRITE & ~CHG_ENABLE;	
	pr_info("%s : write data = 0x%x\n",__func__,data);
	smb358_write_reg(client,SMB358_COMMAND_A,data);		
}
EXPORT_SYMBOL(smb358_stop_chg);

void smb358_start_chg(int type)
{
	struct i2c_client * client;
	u8 data=0;

	pr_info("%s\n",__func__);	
	client = global_client;
	if( !client) return;

	data = ALLOW_VOLATILE_WRITE | CHG_ENABLE;	
	pr_info("%s : data = 0x%x\n",__func__,data);
	smb358_write_reg(client,SMB358_COMMAND_A,data);		
}
EXPORT_SYMBOL(smb358_start_chg);

void smb358_set_chg_current(int chg_current)
{
	struct i2c_client * client;
	u8 data=0,set_current=0;
	client = global_client;
	if( !client) return;

	pr_info("%s chg_current=%d\n",__func__,chg_current);

	smb358_read_reg(client,SMB358_CHARGE_CURRENT,&data);	

	switch( chg_current)
	{
	case 100:	
	case 200: 	set_current = FAST_CHG_200mA;	break;		
	case 450:	set_current = FAST_CHG_450mA;	break;
	case 1300:	set_current = FAST_CHG_1300mA;	break;
	case 1500:	set_current = FAST_CHG_1500mA;	break;
	case 1800:	set_current = FAST_CHG_1800mA;	break;		
	case 2000:	set_current = FAST_CHG_2000mA;	break;		
	default : 	set_current = FAST_CHG_1500mA;	break;	

	}

	data &= FAST_CHG_MASK;
	data |= set_current;
	smb358_write_reg(client,SMB358_CHARGE_CURRENT,data);	
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_CHARGE_CURRENT, data);
	
}
EXPORT_SYMBOL(smb358_set_chg_current);

void smb358_set_eoc(int eoc_value)
{
	struct i2c_client * client;
	u8 data=0,set_eoc=0;
	client = global_client;
	if( !client) return;

	pr_info("%s eoc_value=%d\n",__func__,eoc_value);

	smb358_read_reg(client,SMB358_CHARGE_CURRENT,&data);	

	switch( eoc_value)
	{
	case 30:		set_eoc = EOC_30mA;		break;	
	case 40:		set_eoc = EOC_40mA;		break;
	case 60:		set_eoc = EOC_60mA;		break;
	case 80:		set_eoc = EOC_80mA;		break;
	case 100:	set_eoc = EOC_100mA;		break;
	case 125:	set_eoc = EOC_125mA;		break;
	case 150:	set_eoc = EOC_150mA;		break;
	case 200:	set_eoc = EOC_200mA;		break;		
	default : 	set_eoc = EOC_40mA;		break;	
	}

	data &= EOC_MASK;
	data |= set_eoc;

	smb358_write_reg(client,SMB358_CHARGE_CURRENT,data);	
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_CHARGE_CURRENT, data);
	
}
EXPORT_SYMBOL(smb358_set_eoc);

bool smb358_chg_init(struct i2c_client *client)
{
	pr_info("%s\n",__func__);
	u8 data;

	data = ALLOW_VOLATILE_WRITE;
	smb358_write_reg(client, SMB358_COMMAND_A, data);
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_COMMAND_A, data);

	/* Command B : USB1 mode, USB mode */
	data = USB_5_MODE | HC_MODE;
	smb358_write_reg(client, SMB358_COMMAND_B, data);
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_COMMAND_B, data);		

	/* Allow volatile writes to CONFIG registers */
	data = FAST_CHG_2000mA|PRE_CHG_450mA|EOC_100mA;
	smb358_write_reg(client, SMB358_CHARGE_CURRENT, data); //0
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_CHARGE_CURRENT, data);

	data = LIMIT_2000mA|CHG_INHIBIT_THR_100mV;
	smb358_write_reg(client, SMB358_INPUT_CURRENTLIMIT, data); // 1
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_INPUT_CURRENTLIMIT, data);

	data = 0xD7;
	smb358_write_reg(client, SMB358_VARIOUS_FUNCTIONS, data); // 2
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_VARIOUS_FUNCTIONS, data);

	data = VOLTAGE_4_35V;
	smb358_write_reg(client, SMB358_FLOAT_VOLTAGE, data); // 3
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_FLOAT_VOLTAGE, data);

	data = AUTO_RECHG_DISABLE;
	smb358_write_reg(client, SMB358_CHARGE_CONTROL, data); // 4
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_CHARGE_CONTROL, data);

	data = 0x0F;
	smb358_write_reg(client, SMB358_STAT_TIMERS_CONTROL, data);	// 5
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_STAT_TIMERS_CONTROL, data);

	data = 0x09;
	smb358_write_reg(client, SMB358_PIN_ENABLE_CONTROL, data); // 6
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_PIN_ENABLE_CONTROL, data);

	data = 0xF0;
	smb358_write_reg(client, SMB358_THERM_CONTROL_A, data); //7 
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_THERM_CONTROL_A, data);

	data = 0x09;
	smb358_write_reg(client, SMB358_SYSOK_USB30_SELECTION, data); //8 
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_SYSOK_USB30_SELECTION, data);

	data = 0x00;
	smb358_write_reg(client, SMB358_OTHER_CONTROL_A, data); // 9
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_OTHER_CONTROL_A, data);

	data = 0xF6;
	smb358_write_reg(client, SMB358_OTG_TLIM_THERM_CONTROL, data); // A
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_OTG_TLIM_THERM_CONTROL, data);

	data = 0xA5;
	smb358_write_reg(client, SMB358_LIMIT_CELL_TEMPERATURE_MONITOR, data);	// B
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_LIMIT_CELL_TEMPERATURE_MONITOR, data);
	
	data = 0x00;
	smb358_write_reg(client, SMB358_FAULT_INTERRUPT, data); // C
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_FAULT_INTERRUPT, data);

	data = 0x00;
	smb358_write_reg(client, SMB358_STATUS_INTERRUPT, data); // D
	pr_info("write smb358 addr : 0x%02x data : 0x%02x\n", SMB358_STATUS_INTERRUPT, data);

	return true;
}

static int __devinit smb358_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sec_charger_info *charger;
	int ret=0;

	pr_info("smb358_probe\n");
	global_client = client;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->client = client;
	charger->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, charger);
	smb358_chg_init(charger->client);
	return ret;
}

static int __devexit smb358_remove(struct i2c_client *client)
{

	return 0;
}

static int smb358_resume(struct i2c_client *client)
{
	return 0;
}


static const struct i2c_device_id smb358_id[] = {
	{"smb358", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, smb358_id);

static struct i2c_driver smb358_i2c_driver = {
	.driver = {
		.name = "smb358",
	},
	.probe = smb358_probe,
	.remove = __devexit_p(smb358_remove),
	.resume = smb358_resume,
	.id_table = smb358_id,
};

static int __init smb358_init(void)
{
	return i2c_add_driver(&smb358_i2c_driver);
}
module_init(smb358_init);

static void __exit smb358_exit(void)
{
	i2c_del_driver(&smb358_i2c_driver);
}
module_exit(smb358_exit);

MODULE_AUTHOR("Sehyoung Park <sh16.park@samsung.com>");
MODULE_DESCRIPTION("SMB358 USB Switch driver");
MODULE_LICENSE("GPL");


