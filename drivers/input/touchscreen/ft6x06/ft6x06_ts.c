/* drivers/input/touchscreen/ft5x06_ts.c
 *
 * FocalTech ft6x06 TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
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

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <soc/sprd/i2c-sprd.h>
#include <linux/power_supply.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

//#define FTS_CTL_FACE_DETECT
#define FTS_CTL_IIC
#define SYSFS_DEBUG
#define FTS_APK_DEBUG
#define FT6X06_DOWNLOAD  // JASON  임시로 막음.
#define TA_NOTIFY  


#include "ft6x06_ts.h"

#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif
#ifdef FTS_CTL_FACE_DETECT
#include "ft_psensor_drv.h"
#endif
#ifdef SYSFS_DEBUG
#include "ft6x06_ex_fun.h"
#endif
#ifdef SEC_FACTORY_TEST
#include "ft6x06_sec.h"
#endif

#ifdef TA_NOTIFY
extern void (*tsp_charger_status_cb)(int);
#endif

/* PMIC Regulator based supply to TSP */
#define TSP_REGULATOR_SUPPLY		1
/* gpio controlled LDO based supply to TSP */
#define TSP_LDO_SUPPLY			0


#define FTS_POINT_UP		0x01
#define FTS_POINT_DOWN		0x00
#define FTS_POINT_CONTACT	0x02

static int cal_mode;
static int get_boot_mode(char *str)
{

	get_option(&str, &cal_mode);
	printk("get_boot_mode, uart_mode : %d\n", cal_mode);
	return 1;
}
__setup("calmode=",get_boot_mode);

/*
*ft6x06_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
int ft6x06_i2c_Read(struct i2c_client *client, char *writebuf,
		    int writelen, char *readbuf, int readlen)
{
	int ret;
	int count = 0;
	
retry:
	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error(count:%d).\n", __func__, count);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error(count:%d).\n", __func__, count);
	}

	if (ret < 0) {
		mdelay(1);
		if (++count < 3)
			goto retry;
	}
	
	return ret;
}
/*write data by i2c*/
int ft6x06_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;
	int count = 0;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

retry:
	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error(count:%d).\n", __func__, count);

	if (ret < 0) {
			mdelay(1);
			if (++count < 3)
				goto retry;
	}

	return ret;
}

#ifdef TA_NOTIFY
struct i2c_client *client2;

enum driver_setup_t {DRIVER_SETUP_OK, DRIVER_SETUP_INCOMPLETE};
static enum driver_setup_t driver_setup = DRIVER_SETUP_INCOMPLETE;

int pre_charger_mode = -1;         // PREVIOUS CHARGER MODE
int current_charger_mode = -1;  // CURRENT CHARGER MODE

void tsp_charger_enable(int status)
{
	unsigned char reg_val = 0;
	unsigned int   i = 0;
	int      i_ret =0;

	if (status == POWER_SUPPLY_TYPE_BATTERY ||
			status == POWER_SUPPLY_TYPE_UNKNOWN)
		status = 0;
	else
		status = 1;

	if (status)
		current_charger_mode = 1;
	else
		current_charger_mode = 0;

	printk(KERN_WARNING"[FTS] %d , %d, current_charger_mode:%d \n", driver_setup, pre_charger_mode, current_charger_mode);

	if (driver_setup != DRIVER_SETUP_OK)
		return;

	if(pre_charger_mode == status)  
		return;

	pre_charger_mode = status;

	do
	{
		i++;
		if(status){ // CHARGER IN
		i_ret = ft6x06_write_reg(client2, 0x8b, 0x01);
		}
		else{	// NO CHARGER
			i_ret = ft6x06_write_reg(client2, 0x8b, 0x00);
		}
	}while(i_ret < 0 && i < 5 );

		ft6x06_read_reg(client2, 0x8b, &reg_val);

	printk(KERN_WARNING"[FTS] current char mode8b: %d \n", reg_val);
}
EXPORT_SYMBOL(tsp_charger_enable);
#endif

static int g_last_keycode = -1;
static int g_last_keystate =0; 
static int g_last_pointflag[CFG_MAX_TOUCH_POINTS] = {0};

/*Read touch point information when the interrupt  is asserted.*/
static int ft6x06_read_Touchdata(struct ft6x06_ts_data *data)
{
	struct ts_event *event = &data->event;
	u8 buf[POINT_READ_BUF] = { 0 };
	int ret = -1;
	int i = 0;
	u8 pointid = FT_MAX_ID;

	ret = ft6x06_i2c_Read(data->client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) {
		dev_err(&data->client->dev, "%s read touchdata failed.\n",
			__func__);
		return ret;
	}
	memset(event, 0, sizeof(struct ts_event));

	//event->touch_point = buf[2] & 0x0F;

	//event->touch_point = 0;
	
	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++)
	{
		pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		if (pointid >= FT_MAX_ID)
			break;
		else
			event->touch_point++;
		event->au16_x[pointid] =
		    (s16) (buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i];
		event->au16_y[pointid] =
		    (s16) (buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i];
		event->au8_touch_event[pointid] =
		    buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
		event->au8_finger_id[pointid] =
		    (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		event->pressure[pointid] =(buf[FT_TOUCH_WEIGHT+ FT_TOUCH_STEP * i]);

	        if (g_last_pointflag[pointid] == 0x00)
		{
	            g_last_pointflag[pointid] = 0x02;
		}		
	}
	
	//event->pressure = FT_PRESS;
	event->pressure[i] = FT_PRESS;

	return 0;
}

/*
*report the point information
*/

static void ft6x06_report_value(struct ft6x06_ts_data *data)
{
	struct ts_event *event = &data->event;
	int i = 0;
	int uppoint = 0;
	int downpoint = 0;
	int key_code = 0;

	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++)
	{
		if (g_last_pointflag[i] > 0)
		{
			downpoint++;
		}
	}

	if ((downpoint > 0) && (event->touch_point != downpoint))
	{
		event->touch_point = downpoint;
	}

	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) 
	{
		if (0 == g_last_pointflag[i])
		{
			continue;
		}

		if (event->au16_x[i] <= data->x_max
			&& event->au16_y[i] <= data->y_max)
		{
			if (g_last_pointflag[i] == 0x10) 
			{
				input_report_key(data->input_dev, g_last_keycode, 0);
				g_last_keystate = 0;
				g_last_keycode = -1;
			}
			input_mt_slot(data->input_dev, event->au8_finger_id[i]);
	
			if (event->au8_touch_event[i] == 0
				|| event->au8_touch_event[i] == 2)
			{
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
				//input_report_abs(data->input_dev, ABS_MT_PRESSURE, (u32)event->pressure);
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, (u32)event->pressure);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X, (u32)event->au16_x[i]);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y, (u32)event->au16_y[i]);
				g_last_pointflag[i] = 0x01;
				if(event->au8_touch_event[i] == 0)
					dev_err(&data->client->dev, "[FTS] [%d]PRESS [%d][%d]\n", event->au8_finger_id[i], event->au16_x[i], event->au16_y[i]);
			}
			else
			{
				uppoint++;
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,false);
				g_last_pointflag[i] = 0;
				dev_err(&data->client->dev, "[FTS] [%d]RELEASE\n",event->au8_finger_id[i]);
			}	
		}
		else
		{
			if (g_last_pointflag[i] == 0x01)
			{
				/*id is from va area to key area*/
				input_mt_slot(data->input_dev, event->au8_finger_id[i]);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
				input_report_key(data->input_dev, BTN_TOUCH, 0);
			}
	
			if (event->au16_x[i] <= 405 && event->au16_x[i] >= 395)
			{
				key_code = KEY_BACK;	 //158
			}
			else if (event->au16_x[i] <= 85 && event->au16_x[i] >= 75)
			{
				key_code = KEY_RECENT;	//139
			}
			else
			{
			//	key_code = KEY_BACK;	 //158
			//	printk(KERN_WARNING "key_code = KEY_BACK 20140710\n");
				dev_err(&data->client->dev, "unknown key type [%d] : x=%d , y=%d\n", i,event->au16_x[i],event->au16_y[i]);
			}
	
			if (event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2)
			{
				if (1 == g_last_keystate)
				{
					if (g_last_keycode != key_code)
					{
						input_report_key(data->input_dev, g_last_keycode, 0);
						input_report_key(data->input_dev, key_code, 1);
						g_last_keycode = key_code;
					}
				}
				else
				{
					g_last_keystate = 1;
					g_last_keycode = key_code;
					input_report_key(data->input_dev, key_code, 1);
				}
				g_last_pointflag[i] = 0x10;
			}
			else
			{
				uppoint++;
				g_last_keystate = 0;
				g_last_keycode = -1;
	
				g_last_pointflag[i] = 0x00;
				input_report_key(data->input_dev, key_code, 0); 
			}
		}
	}
		
	if (event->touch_point == uppoint)
	{
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		memset(event, 0, sizeof(struct ts_event));
		memset(g_last_pointflag, 0, sizeof(g_last_pointflag));
	}
	else
	{
		input_report_key(data->input_dev, BTN_TOUCH, 1);//	still be touched
	}
		
	input_sync(data->input_dev);
}



/*The ft6x06 device will signal the host about TRIGGER_FALLING.
*Processed when the interrupt is asserted.
*/
static irqreturn_t ft6x06_ts_interrupt(int irq, void *dev_id)
{
	struct ft6x06_ts_data *ft6x06_ts = dev_id;
	int ret = 0;
	disable_irq_nosync(ft6x06_ts->irq);

	ret = ft6x06_read_Touchdata(ft6x06_ts);
	if (ret == 0)
		ft6x06_report_value(ft6x06_ts);

	enable_irq(ft6x06_ts->irq);

	//printk(KERN_WARNING "interrupt \n");

	return IRQ_HANDLED;
}


static struct regulator *touch_regulator;
static int ft6x06_ts_power(struct ft6x06_ts_data *info, int on)
{
	int ret = 0;
	static u8 is_power_on;
	if (touch_regulator == NULL) {
		touch_regulator = regulator_get(&info->client->dev, "vddsim2");
		if (IS_ERR(touch_regulator)) {
			touch_regulator = NULL;
			dev_err(&info->client->dev, "regulator get error\n");
			return -EIO;
		}
	}
	if (on == 0) {
		if (is_power_on) {
			is_power_on = 0;
			ret = regulator_disable(touch_regulator);
			if (ret) {
				is_power_on = 1;
				dev_err(&info->client->dev, "power off error!\n");
				return -EIO;
			}
			dev_info(&info->client->dev, "power off!\n");
			mdelay(50);
		}
	} else {
		if (!is_power_on) {
			is_power_on = 1;
			regulator_set_voltage(touch_regulator, 3000000, 3000000);
			ret = regulator_enable(touch_regulator);
			if (ret) {
				is_power_on = 0;
				dev_err(&info->client->dev, "power on error!");
				return -EIO;
			}
			dev_info(&info->client->dev, "power on!\n");
		}
	}

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft6x06_ts_early_suspend(struct early_suspend *handler)
{
	struct ft6x06_ts_data *ts = container_of(handler, struct ft6x06_ts_data,
						early_suspend);

	dev_info(&ts->client->dev, "[FTS] ft6x06  to early suspend\n");
	disable_irq(ts->pdata->irq);

#ifdef TA_NOTIFY
	driver_setup = DRIVER_SETUP_INCOMPLETE;
#endif

#if 1 // JASON
// HW RESET PIN 없는 경우 TOUCH VDD를 전원을 OFF
ft6x06_ts_power(ts, 0);
ts->work_state = EALRY_SUSPEND;
#endif
}

static void ft6x06_ts_late_resume(struct early_suspend *handler)
{
	struct ft6x06_ts_data *ts = container_of(handler, struct ft6x06_ts_data,
						early_suspend);

	dev_info(&ts->client->dev, "[FTS]ft6x06 to late resume.\n");

#if 1 // JASON
// HW RESET PIN 없는 경우 TOUCH VDD를 전원을 ON
	ft6x06_ts_power(ts, 1);
	ts->work_state = LATE_RESUME;
	msleep(100);
#else
	gpio_set_value(ts->pdata->reset, 0);
	msleep(20);
	gpio_set_value(ts->pdata->reset, 1);
#endif
#ifdef TA_NOTIFY
	driver_setup = DRIVER_SETUP_OK;
	pre_charger_mode = -1;
	tsp_charger_enable(current_charger_mode);
#endif
	
	enable_irq(ts->pdata->irq);
}
#endif

static int ts_probe_dt(struct device_node *np,
			 struct device *dev,
			 struct ft6x06_platform_data *pdata)
{
	int ret;

	if (!np)
		return -EINVAL;

/*	pdata->gpio_int = of_get_named_gpio(np,"gpios", 0);
	if (pdata->gpio_int < 0) {
			pr_err("%s: of_get_named_gpio failed: %d\n", __func__,
				pdata->gpio_int);
			return -EINVAL;
	}*/
	ret = of_property_read_u32(np, "ft6x06,x_resolution",
						&pdata->x_resolution);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(np, "ft6x06,y_resolution",
						&pdata->y_resolution);
	return 0;
}

static int ft6x06_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{

	struct ft6x06_platform_data *pdata =
	    (struct ft6x06_platform_data *)client->dev.platform_data;
	struct ft6x06_ts_data *ft6x06_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value;
	unsigned char uc_reg_addr;
	struct device_node *np = client->dev.of_node;


	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&client->dev,
					sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		err = ts_probe_dt(np, &client->dev, pdata);
		if (err)
			goto exit_no_platform_data;

	} else if (!pdata) {
		dev_err(&client->dev, "Not exist platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft6x06_ts = kzalloc(sizeof(struct ft6x06_ts_data), GFP_KERNEL);

	if (!ft6x06_ts) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

#ifdef TA_NOTIFY
	client2 = client;
#endif
	i2c_set_clientdata(client, ft6x06_ts);

	ft6x06_ts->irq = client->irq;
	ft6x06_ts->client = client;
	ft6x06_ts->pdata = pdata;
	ft6x06_ts->x_max = pdata->x_resolution;
	ft6x06_ts->y_max = pdata->y_resolution;
	ft6x06_ts->pdata->irq = ft6x06_ts->irq;
	ft6x06_ts->irq = client->irq;
	pr_info("irq = %d\n", ft6x06_ts->irq);

	ft6x06_ts_power(ft6x06_ts, 1);
	msleep(300);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	ft6x06_ts->input_dev = input_dev;
	ft6x06_ts->work_state = NOTHING;

	sprd_i2c_ctl_chg_clk(1, 400000); // up h/w i2c 1 400k
	
// fw_update 하면서 connection 여부 확인 => error처리 ymsong	
#ifdef FT6X06_DOWNLOAD 
	if (cal_mode)
		dev_info(&client->dev, "didn't update TSP F/W!! in CAL MODE\n");
	else 
	{
		err = fts_check_need_upgrade(client);
		if(err < 0) /*It seems no connection with tsp chipset*/
			goto	exit_input_register_device_failed;
		else if(err == true)
		{
			err = fts_ctpm_auto_upgrade(client);
			if(err < 0)
				goto	exit_input_register_device_failed;
		}
	}
#endif

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	//__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	
	input_mt_init_slots(input_dev, MT_MAX_TOUCH_POINTS,0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, ft6x06_ts->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, ft6x06_ts->y_max, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
//					 0, PRESS_MAX, 0, 0);
	set_bit(KEY_RECENT, input_dev->keybit);
	set_bit(KEY_BACK, input_dev->keybit);

	//input_dev->name = FT6X06_NAME;  ymsong 확인필요 
	input_dev->name = "sec_touchscreen";
	input_set_drvdata(input_dev, ft6x06_ts);
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
			"ft6x06_ts_probe: failed to register input device: %s\n",
			dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}
	/*make sure CTP already finish startup process */

	/*get some register information */
	uc_reg_addr = FT6x06_REG_FW_VER;
	ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	dev_info(&client->dev, "[FTS] Firmware version = 0x%x\n", uc_reg_value);
	ft6x06_ts->fw_version = uc_reg_value;

	uc_reg_addr = FT6x06_REG_POINT_RATE;
	ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	dev_info(&client->dev, "[FTS] report rate is %dHz.\n",
		uc_reg_value * 10);

	uc_reg_addr = FT6x06_REG_THGROUP;
	ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	dev_info(&client->dev, "[FTS] touch threshold is %d.\n",
		uc_reg_value * 4);
	ft6x06_ts->threshold = uc_reg_value;
	ft6x06_ts->key_threshold = uc_reg_value; // ymsong 임시 

	err = request_threaded_irq(client->irq, NULL, ft6x06_ts_interrupt,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->dev.driver->name,
				   ft6x06_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft6x06_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}
	
	//disable_irq(client->irq); // ymsong

#ifdef CONFIG_HAS_EARLYSUSPEND
	ft6x06_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ft6x06_ts->early_suspend.suspend = ft6x06_ts_early_suspend;
	ft6x06_ts->early_suspend.resume = ft6x06_ts_late_resume;
	register_early_suspend(&ft6x06_ts->early_suspend);
#endif


#ifdef SYSFS_DEBUG
	ft6x06_create_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	if (ft_rw_iic_drv_init(client) < 0)
		dev_err(&client->dev, "%s:[FTS] create fts control iic driver failed\n",
				__func__);
#endif

#ifdef FTS_APK_DEBUG
	ft6x06_create_apk_debug_channel(client);
#endif

#ifdef FTS_CTL_FACE_DETECT
		if (ft_psensor_drv_init(client) < 0)
			dev_err(&client->dev, "%s:[FTS] create fts control psensor driver failed\n",
					__func__);
#endif

#ifdef TA_NOTIFY
	tsp_charger_status_cb = tsp_charger_enable;
	driver_setup = DRIVER_SETUP_OK;
#endif

#ifdef SEC_FACTORY_TEST
	err = init_sec_factory(ft6x06_ts);
	if (err) {
		dev_err(&client->dev, "Failed to init sec factory device\n");
		goto exit_factory_init_failed;
	}
#endif

	//enable_irq(client->irq); // ymsong
	return 0;

#ifdef SEC_FACTORY_TEST
exit_factory_init_failed:
	kfree(ft6x06_ts->factory_info);
	kfree(ft6x06_ts->raw_data);
	free_irq(client->irq, ft6x06_ts);
#endif
exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
exit_irq_request_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ft6x06_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
exit_no_platform_data:
	if (IS_ENABLED(CONFIG_OF))
		devm_kfree(&client->dev, (void *)pdata);

	tsp_charger_status_cb = NULL;

	return err;
}


static int ft6x06_ts_remove(struct i2c_client *client)//zax
{
	struct ft6x06_ts_data *ft6x06_ts;
	ft6x06_ts = i2c_get_clientdata(client);
	
	input_unregister_device(ft6x06_ts->input_dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ft6x06_ts->early_suspend);
#endif
#ifdef SEC_FACTORY_TEST
	kfree(ft6x06_ts->factory_info);
	kfree(ft6x06_ts->raw_data);
#endif
#ifdef SYSFS_DEBUG
	ft6x06_release_sysfs(client);
#endif
#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
#endif
#ifdef FTS_CTL_FACE_DETECT
	ft_psensor_drv_exit();
#endif
	free_irq(client->irq, ft6x06_ts);
	kfree(ft6x06_ts);
	i2c_set_clientdata(client, NULL);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id focaltech_dt_ids[] = {
	{ .compatible = "focaltech,ft6x06_ts", },
	{},
};
MODULE_DEVICE_TABLE(of, focaltech_dt_ids);
#endif

static const struct i2c_device_id ft6x06_ts_id[] = {
	{FT6X06_NAME, 0},
};

MODULE_DEVICE_TABLE(i2c, ft6x06_ts_id);

static struct i2c_driver ft6x06_ts_driver = {
	.probe = ft6x06_ts_probe,
	.remove = ft6x06_ts_remove,
	.id_table = ft6x06_ts_id,
	.driver = {
		.name =FT6X06_NAME,
		.owner = THIS_MODULE,
		.of_match_table = focaltech_dt_ids,
	},
};

extern unsigned int lpcharge;

static int __init ft6x06_ts_init(void)
{
	int ret;

	pr_info("[FTS]: %s\n", __func__);

	if (!lpcharge)
		return i2c_add_driver(&ft6x06_ts_driver);
	else
		return 0;
}

static void __exit ft6x06_ts_exit(void)
{
	i2c_del_driver(&ft6x06_ts_driver);
}

module_init(ft6x06_ts_init);
module_exit(ft6x06_ts_exit);

MODULE_AUTHOR("<luowj>");
MODULE_DESCRIPTION("FocalTech ft6x06 TouchScreen driver");
MODULE_LICENSE("GPL");
