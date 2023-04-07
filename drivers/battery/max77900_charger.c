/*
 *  max77900_charger.c
 *  Samsung max77900 Charger Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 * Yeongmi Ha <yeongmi86.ha@samsung.com>
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

#include <linux/battery/charger/max77900_charger.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>

#define ENABLE 1
#define DISABLE 0

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
};

static int max77900_read_device(struct i2c_client *client,
		u8 reg, u8 bytes, void *dest)
{
	int ret;
	if (bytes > 1) {
		ret = i2c_smbus_read_i2c_block_data(client, reg, bytes, dest);
	} else {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret < 0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}
	return ret;
}

static int max77900_reg_read(struct i2c_client *client, u8 reg, u8 * data)
{
	struct max77900_charger_data *charger = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&charger->io_lock);
	ret = max77900_read_device(client, reg, 1, data);
	mutex_unlock(&charger->io_lock);

	if (ret < 0) {
		pr_err("%s: can't read reg(0x%x), ret(%d)\n", __func__, reg, *data);
		return ret;
	} else
		pr_err("%s: read reg(0x%x), ret(%d)\n", __func__, reg, *data);

	return ret;
}

static int max77900_reg_write(struct i2c_client *client, u8 reg, u8 data)
{
	struct max77900_charger_data *charger = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&charger->io_lock);
	ret = i2c_smbus_write_byte_data(client, reg, data);
	mutex_unlock(&charger->io_lock);

	if (ret < 0)
		pr_err("%s: can't write reg(0x%x), ret(%d)\n", __func__, reg, ret);
	else
		max77900_reg_read(client,reg,&data);
	return ret;
}

/*******************************************************************************
 * LDO Section
 ******************************************************************************/

/*
  * Setup the LDOs output voltage
  * the LDO output voltage by writing the LDO_VOUT register and setting the VOUTSET_OVERRIDE bit to 1
  *
  * In TX mode, to convert the MAX77900 to peer-to-peer wireless power transmitter mode,
  * the first operation is to apply a voltage (3.3V to 5V) at the output of the LDO (OUT).
  * The LDOs output voltage can be targeted to any value between 3.65V and 10V with 50mV/LSB.
  * 
  * Default Reset value 0x00 means 3.65V
  *
*During power-up, the LDO is held off until the VRECT-TARGET threshold 1 converges.
*The voltage control loop ensures that the output voltage is maintained at VOUT-REG to power the system.
*/
#if 0
static void max77900_set_ldo_vout(struct max77900_charger_data *charger, u32 input_value)
{
	u8 read_value=0;
	u8 write_value=0;
	u32 ldo_vout=0;

	ldo_vout=(u8)input_value;
	pr_info("%s ldo_vout  0x%X\n",__func__,ldo_vout);  // 0x1B
	max77900_reg_read(charger->client,0x0d,&read_value);

	pr_info("%s read_value  0x%X\n",__func__,read_value); // 0x9B

	// default value was overrided.
	write_value = /*VOUTSET_OVERRIDE || */(read_value & ~0x3F )| ldo_vout; //0x1B <-- check out
	max77900_reg_write(charger->client,0x0f,write_value);

	pr_info("%s write_value  0x%X\n",__func__,write_value); //0x9B
}

static u32 max77900_get_ldo_vout(struct max77900_charger_data *charger)
{
	u8 vout_valh=0;
	u8 vout_vall=0;
	u16 vout_val=0;
	u32 vout=0;

	pr_info("%s\n",__func__);

	max77900_reg_read(charger->client,0x14,&vout_valh);
	max77900_reg_read(charger->client,0x15,&vout_vall);
	pr_info("%s vout_valh 0x%X  vout_vall 0x%X\n",__func__,vout_valh,vout_vall);

	vout_val = vout_valh << 4 | vout_vall; 
	vout	 = DIV_ROUND_UP((( 8 * 1250 * vout_val / 4095 ) ),1) ;   // 4095, 1250 mV

	pr_info("vout_val %d vout %d mV\n", vout_val, vout);
	return vout;
}

static u32 max77900_get_vrect(struct max77900_charger_data *charger)
{
	u8 ldo_vrecth=0;
	u8 ldo_vrectl=0;
	u16 vrect_val=0;
	u32 vrect=0;
	int ret = 0;

	pr_info("%s\n",__func__);

	ret = max77900_reg_read(charger->client,0x12,&ldo_vrecth);//VRECTVAL[7:0]
	if( ret > 0 )
		ret = max77900_reg_read(charger->client,0x13,&ldo_vrectl);//VRECTVAL[3:0]
	if( ret > 0 )	
		pr_info("%s ldo_vrecth 0x%X  ldo_vrectl 0x%X\n",__func__,ldo_vrecth,ldo_vrectl);

	vrect_val = (ldo_vrecth << 4)| ldo_vrectl; //VRECTVAL[11:0 ]
	pr_info("vrect_val %d\n", vrect_val); //4009

	/* vrect = 12* 1.25 * vrecval /4095 */
	vrect	   =  DIV_ROUND_UP(( 12* 1250 * vrect_val / 4095 ),1);//mV

	pr_info("%s [vrect] %d mV\n", __func__, vrect);

	return vrect;
}

static PVRECT_T max77900_get_vrect_target(struct max77900_charger_data *charger)
{
	u8 vrect_x[8]		={0};
	u8 vrect_y_wpc[8]	={0};
	u8 vrect_y_pma[8]	={0};

	u8 vrect[8][3] = {{0,0,0}};
	u8 (*vrect_p)[3];

	int i =0;
	int j =0;
	vrect_p = vrect;

	/* read the register values */
	for( i=0; i < 8 ; i++ ) {
		max77900_reg_read(charger->client,VRECT_TARGET_X0+3*i ,&vrect_x[i]);
		max77900_reg_read(charger->client,VRECT_TARGET_Y0_WPC+3*i,&vrect_y_wpc[i]);
		max77900_reg_read(charger->client,VRECT_TARGET_Y0_PMA+3*i,&vrect_y_pma[i]);
	}

	/*  move to array */
	for( i=0; i < 8 ; i++ ) {
		*(*(vrect+i)+0) = vrect_x[i];
		*(*(vrect+i)+1) = vrect_y_wpc[i];
		*(*(vrect+i)+2) = vrect_y_pma[i];
	}

	/* print all elements */
	for(i=0; i < 8 ; i++ ) {
		for(j=0; j < 3 ; j++ )
		{
			pr_info(" vrect_target[%d][%d] 0x%X",i,j, *(*(vrect_p+i)+j));
		}
		pr_info("\n");

	}
	return vrect_p;
}

void max77900_wireless_chg_init(struct max77900_charger_data *charger)
{
	u32 vrect_mv = 0;

	pr_info("%s -------------------------------------------------- \n", __func__);
	max77900_reg_write(charger->client, 0x18, 0x02); // ldo out enable

	vrect_mv = max77900_get_vrect(charger);
	pr_info("%s (before) vrect_mv %d mV\n",__func__,vrect_mv);
	max77900_get_vrect_target(charger);

	max77900_reg_write(charger->client, 0x7c, 87);
	max77900_reg_write(charger->client, 0x7d, 80);
	max77900_reg_write(charger->client, 0x7e, 48);
	max77900_reg_write(charger->client, 0x7f, 49);
	max77900_reg_write(charger->client, 0x65, 0xa4); // 9V
	msleep(5);

	max77900_reg_write(charger->client, 0x7c, 87);
	max77900_reg_write(charger->client, 0x7d, 80);
	max77900_reg_write(charger->client, 0x7e, 48);
	max77900_reg_write(charger->client, 0x7f, 49);
	max77900_reg_write(charger->client, 0x68, 0xa4);
	msleep(5);

	max77900_reg_write(charger->client, 0x7c, 87);
	max77900_reg_write(charger->client, 0x7d, 80);
	max77900_reg_write(charger->client, 0x7e, 48);
	max77900_reg_write(charger->client, 0x7f, 49);
	max77900_reg_write(charger->client, 0x6b, 0xa4);
	msleep(5);

	max77900_reg_write(charger->client, 0x7c, 87);
	max77900_reg_write(charger->client, 0x7d, 80);
	max77900_reg_write(charger->client, 0x7e, 48);
	max77900_reg_write(charger->client, 0x7f, 49);
	max77900_reg_write(charger->client, 0x6e, 0xa4);
	msleep(5);

	max77900_reg_write(charger->client, 0x7c, 87);
	max77900_reg_write(charger->client, 0x7d, 80);
	max77900_reg_write(charger->client, 0x7e, 48);
	max77900_reg_write(charger->client, 0x7f, 49);
	max77900_reg_write(charger->client, 0x71, 0xa3);
	msleep(5);

	max77900_reg_write(charger->client, 0x7c, 87);
	max77900_reg_write(charger->client, 0x7d, 80);
	max77900_reg_write(charger->client, 0x7e, 48);
	max77900_reg_write(charger->client, 0x7f, 49);
	max77900_reg_write(charger->client, 0x74, 0xa3);
	msleep(5);

	max77900_reg_write(charger->client, 0x7c, 87);
	max77900_reg_write(charger->client, 0x7d, 80);
	max77900_reg_write(charger->client, 0x7e, 48);
	max77900_reg_write(charger->client, 0x7f, 49);
	max77900_reg_write(charger->client, 0x77, 0xa3);
	msleep(5);

	max77900_reg_write(charger->client, 0x7c, 87);
	max77900_reg_write(charger->client, 0x7d, 80);
	max77900_reg_write(charger->client, 0x7e, 48);
	max77900_reg_write(charger->client, 0x7f, 49);
	max77900_reg_write(charger->client, 0x7a, 0xa3);
	msleep(500);

	vrect_mv = max77900_get_vrect(charger);
	pr_info("%s (after) vrect_mv %d mV \n",__func__,vrect_mv);
	max77900_get_vrect_target(charger);

	max77900_get_ldo_vout(charger);
	max77900_set_ldo_vout(charger, 0xf5); // 0xeb 9V
	max77900_get_ldo_vout(charger);

	max77900_reg_write(charger->client, 0x0e, 0x9a); // 0x99 - 1A

	pr_info("%s -------------------------------------------------- \n", __func__);
}
#endif

static int max77900_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	//struct max77900_charger_data *charger =
	//	container_of(psy, struct max77900_charger_data, psy_chg);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
		case POWER_SUPPLY_PROP_HEALTH:
		case POWER_SUPPLY_PROP_ONLINE:
		case POWER_SUPPLY_PROP_CURRENT_NOW:
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			return -ENODATA;
		default:
			return -EINVAL;
	}
	return 0;
}

static int max77900_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct max77900_charger_data *charger =
		container_of(psy, struct max77900_charger_data, psy_chg); 

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			max77900_reg_write(charger->client, 0x18, 0x02); // ldo out enable
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			break;	
		default:
			return -EINVAL;
	}

	return 0;
}

static void max77900_wpc_isr_work(struct work_struct *work)
{
	//struct bq51221_charger_data *charger =
	//	container_of(work, struct bq51221_charger_data, isr_work.work);

	pr_info("%s \n",__func__);
}

static void max77900_detect_work(
		struct work_struct *work)
{
	struct max77900_charger_data *charger =
		container_of(work, struct max77900_charger_data, wpc_work.work);
	int wc_w_state;
	int wc_w_state_irq;
	union power_supply_propval value;

	pr_info("%s\n",__func__);

	wc_w_state = gpio_get_value(charger->pdata->wpc_det);
	wc_w_state_irq = gpio_get_value(charger->pdata->wpc_int);
	pr_info("%s wc_w_state = %d , wc_w_state_irq = %d\n", __func__, wc_w_state, wc_w_state_irq);

	if ((charger->wc_w_state == 0) && (wc_w_state == 1)) {
		value.intval = false;
		psy_do_property("max77833-charger", set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);

		value.intval = SEC_WIRELESS_PAD_WPC;
		psy_do_property("wireless", set,
				POWER_SUPPLY_PROP_ONLINE, value);

		pr_info("%s: wpc activated, set V_INT as PN\n",__func__);

	} else if ((charger->wc_w_state == 1) && (wc_w_state == 0)) {
		value.intval = SEC_WIRELESS_PAD_NONE;
		psy_do_property("wireless", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: wpc deactivated, set V_INT as PD\n",__func__);
	}

	pr_info("%s: w(%d to %d)\n", __func__,
		charger->wc_w_state, wc_w_state);

	charger->wc_w_state = wc_w_state;

	wc_w_state = gpio_get_value(charger->pdata->wpc_det);
	wc_w_state_irq = gpio_get_value(charger->pdata->wpc_int);

	pr_info("%s wc_w_state = %d , wc_w_state_irq = %d\n", __func__, wc_w_state, wc_w_state_irq);
	wake_unlock(&charger->wpc_wake_lock);
}


static irqreturn_t max77900_wpc_irq_thread(int irq, void *irq_data)
{
	struct max77900_charger_data *charger = irq_data;

	pr_info("%s \n",__func__);
	max77900_reg_write(charger->client, 0x18, 0x02); // ldo out enable

	return IRQ_HANDLED;
}

static irqreturn_t max77900_wpc_det_thread(int irq, void *irq_data)
{
	struct max77900_charger_data *charger = irq_data;
	unsigned long delay;

	pr_info("%s \n",__func__);
	wake_lock(&charger->wpc_wake_lock);
	if (charger->wc_w_state)
		delay = msecs_to_jiffies(500);
	else
		delay = msecs_to_jiffies(0);
	queue_delayed_work(charger->wqueue, &charger->wpc_work,
			delay);

	return IRQ_HANDLED;
}

static int max77900_chg_parse_dt(struct device *dev,
		max77900_charger_platform_data_t *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	enum of_gpio_flags irq_gpio_flags;

	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {

		ret = of_property_read_string(np,
			"battery,wirelss_charger_name", (char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: Vendor is Empty\n", __func__);
	}

	/* wpc_det */
	ret = pdata->wpc_det = of_get_named_gpio_flags(np, "battery,wpc_det",
			0, &irq_gpio_flags);
	if (ret < 0) {
		dev_err(dev, "%s : can't get wpc_det\r\n", __FUNCTION__);
	} else {
		pdata->irq_wpc_det = gpio_to_irq(pdata->wpc_det);
		pr_info("%s wpc_det = 0x%x, irq_wpc_det = 0x%x \n",__func__, pdata->wpc_det, pdata->irq_wpc_det);
	}
	/* wpc_int */
	ret = pdata->wpc_int = of_get_named_gpio_flags(np, "battery,wpc_int",
			0, &irq_gpio_flags);
	if (ret < 0) {
		dev_err(dev, "%s : can't wpc_int\r\n", __FUNCTION__);
	} else {
		pdata->irq_wpc_int = gpio_to_irq(pdata->wpc_int);
		pr_info("%s wpc_int = 0x%x, irq_wpc_int = 0x%x \n",__func__, pdata->wpc_int, pdata->irq_wpc_int);
	}

	return ret;
}

static int max77900_charger_probe(
						struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct device_node *of_node = client->dev.of_node;
	struct max77900_charger_data *charger;
	max77900_charger_platform_data_t *pdata = client->dev.platform_data;
	int ret = 0;

	dev_info(&client->dev,
		"%s: max77900 Charger Driver Loading\n", __func__);

	if (of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = max77900_chg_parse_dt(&client->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		pdata = client->dev.platform_data;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (charger == NULL) {
		dev_err(&client->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_wpc_nomem;
	}
	charger->dev = &client->dev;

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(client->adapter);
		dev_err(charger->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}

	charger->client = client;
	charger->pdata = pdata;

	pr_info("%s: %s\n", __func__, charger->pdata->wireless_charger_name );

	i2c_set_clientdata(client, charger);

	charger->psy_chg.name		= pdata->wireless_charger_name;
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= max77900_chg_get_property;
	charger->psy_chg.set_property	= max77900_chg_set_property;
	charger->psy_chg.properties	= sec_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(sec_charger_props);

	mutex_init(&charger->io_lock);

	ret = power_supply_register(&client->dev, &charger->psy_chg);
	if (ret) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_chg\n", __func__);
		goto err_supply_unreg;
	}

	charger->wqueue = create_singlethread_workqueue("max77900_workqueue");
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}

	wake_lock_init(&charger->wpc_wake_lock, WAKE_LOCK_SUSPEND,
			"max77900_wakelock");
	INIT_DELAYED_WORK(&charger->wpc_work, max77900_detect_work);

	/* wpc_det */
	if (charger->pdata->irq_wpc_det) {
		ret = request_threaded_irq(charger->pdata->irq_wpc_det,
				NULL, max77900_wpc_det_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
				| IRQF_ONESHOT,
				"max77900-det", charger);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_pdata_free;
		}

		ret = enable_irq_wake(charger->pdata->irq_wpc_det);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: Failed to Enable Wakeup Source(%d)\n",
				__func__, ret);
	}

	/* wpc_irq */
	if (charger->pdata->irq_wpc_int) {
		INIT_DELAYED_WORK(
			&charger->isr_work, max77900_wpc_isr_work);

		ret = request_threaded_irq(charger->pdata->irq_wpc_int,
				NULL, max77900_wpc_irq_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
				| IRQF_ONESHOT,
				"max77900-irq", charger);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_pdata_free;
		}

		ret = enable_irq_wake(charger->pdata->irq_wpc_int);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: Failed to Enable Wakeup Source(%d)\n",
				__func__, ret);
	}

	dev_info(&client->dev,
		"%s: max77900 Charger Driver Loaded\n", __func__);

	return 0;

err_pdata_free:
	power_supply_unregister(&charger->psy_chg);
err_supply_unreg:
	mutex_destroy(&charger->io_lock);
err_i2cfunc_not_support:
//irq_base_err:
	kfree(charger);
err_wpc_nomem:
err_parse_dt:
	devm_kfree(&client->dev,pdata);
	return ret;
}

static int max77900_charger_remove(struct i2c_client *client)
{
	return 0;
}

#if defined CONFIG_PM
static int max77900_charger_suspend(struct i2c_client *client,
				pm_message_t state)
{


	return 0;
}

static int max77900_charger_resume(struct i2c_client *client)
{


	return 0;
}
#else
#define max77900_charger_suspend NULL
#define max77900_charger_resume NULL
#endif

static void max77900_charger_shutdown(struct i2c_client *client)
{

}

static const struct i2c_device_id max77900_charger_id_table[] = {
	{ "max77900-charger", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max77900_id_table);

#ifdef CONFIG_OF
static struct of_device_id max77900_charger_match_table[] = {
	{ .compatible = "maxim,max77900-charger",},
	{},
};
#else
#define max77900_charger_match_table NULL
#endif

static struct i2c_driver max77900_charger_driver = {
	.driver = {
		.name	= "max77900-charger",
		.owner	= THIS_MODULE,
		.of_match_table = max77900_charger_match_table,
	},
	.shutdown	= max77900_charger_shutdown,
	.suspend	= max77900_charger_suspend,
	.resume		= max77900_charger_resume,
	.probe	= max77900_charger_probe,
	.remove	= max77900_charger_remove,
	.id_table	= max77900_charger_id_table,
};

static int __init max77900_charger_init(void)
{
	pr_info("%s \n",__func__);
	return i2c_add_driver(&max77900_charger_driver);
}

static void __exit max77900_charger_exit(void)
{
	pr_info("%s \n",__func__);
	i2c_del_driver(&max77900_charger_driver);
}

module_init(max77900_charger_init);
module_exit(max77900_charger_exit);

MODULE_DESCRIPTION("Samsung max77900 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
