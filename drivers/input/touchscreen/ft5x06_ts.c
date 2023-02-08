/*
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver.
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
 * VERSION		DATE			AUTHOR        Note
 *    1.0		  2010-01-05			WenFS    only support mulititouch	Wenfs 2010-10-01
 *    2.0          2011-09-05                   Duxx      Add touch key, and project setting update, auto CLB command
 *    3.0		  2011-09-09			Luowj
 *
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/i2c/ft5x06_ts.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/module.h>

#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#if FT5x0x_EXTEND_FUN
#include "ft5x06_ex_fun.h"
#endif

static struct i2c_client *this_client;

#define CONFIG_FT5X0X_MULTITOUCH 1
#if 0
struct ts_event {
    u16 au16_x[CFG_MAX_TOUCH_POINTS];              //x coordinate
    u16 au16_y[CFG_MAX_TOUCH_POINTS];              //y coordinate
    u8  au8_touch_event[CFG_MAX_TOUCH_POINTS];     //touch event:  0 -- down; 1-- contact; 2 -- contact
    u8  au8_finger_id[CFG_MAX_TOUCH_POINTS];       //touch ID
    u16 pressure;
    u8  touch_point;
};

struct ft5x0x_ts_data {
	struct input_dev	*input_dev;
	struct ts_event		event;
	struct work_struct	pen_event_work;
	struct workqueue_struct *ts_workqueue;
//	struct early_suspend	early_suspend;
//	struct mutex device_mode_mutex;   /* Ensures that only one function can specify the Device Mode at a time. */
};
#endif
#if POLLING_OR_INTERRUPT
static void ft5x0x_polling(unsigned long data);
static struct timer_list test_timer;
#define POLLING_CYCLE 10
#define POLLING_CHECK_TOUCH	0x01
#define POLLING_CHECK_NOTOUCH		0x00
#endif

#if CFG_SUPPORT_TOUCH_KEY
int tsp_keycodes[CFG_NUMOFKEYS] ={
        KEY_MENU,
        KEY_HOME,
        KEY_BACK,
        KEY_SEARCH
};
char *tsp_keyname[CFG_NUMOFKEYS] ={
        "Menu",
        "Home",
        "Back",
        "Search"
};
static bool tsp_keystatus[CFG_NUMOFKEYS];
#endif


/***********************************************************************************************
Name	:	ft5x0x_i2c_rxdata

Input	:	*rxdata
                     *length

Output	:	ret

function	:

***********************************************************************************************/
int ft5x0x_i2c_Read(char * writebuf, int writelen, char *readbuf, int readlen)
{
	int ret;

	if(writelen > 0)
	{
		struct i2c_msg msgs[] = {
			{
				.addr	= this_client->addr,
				.flags	= 0,
				.len	= writelen,
				.buf	= writebuf,
			},
			{
				.addr	= this_client->addr,
				.flags	= I2C_M_RD,
				.len	= readlen,
				.buf	= readbuf,
			},
		};
		ret = i2c_transfer(this_client->adapter, msgs, 2);
		if (ret < 0)
			DbgPrintk("msg %s i2c read error: %d\n", __func__, ret);
	}
	else
	{
		struct i2c_msg msgs[] = {
			{
				.addr	= this_client->addr,
				.flags	= I2C_M_RD,
				.len	= readlen,
				.buf	= readbuf,
			},
		};
		ret = i2c_transfer(this_client->adapter, msgs, 1);
		if (ret < 0)
			DbgPrintk("msg %s i2c read error: %d\n", __func__, ret);
	}
	return ret;
}EXPORT_SYMBOL(ft5x0x_i2c_Read);
/***********************************************************************************************
Name	:	 ft5x0x_i2c_Write

Input	:


Output	:0-write success
		other-error code
function	:	write data by i2c

***********************************************************************************************/
int ft5x0x_i2c_Write(char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= writelen,
			.buf	= writebuf,
		},
	};

	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		DbgPrintk("%s i2c write error: %d\n", __func__, ret);

	return ret;
}EXPORT_SYMBOL(ft5x0x_i2c_Write);

void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            udelay(1);
        }
    }
}

/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/
static void ft5x0x_ts_release(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	input_mt_sync(data->input_dev);
	input_sync(data->input_dev);
}


//read touch point information
static int ft5x0x_read_Touchdata(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	u8 buf[CFG_POINT_READ_BUF] = {0};
	int ret = -1;
	int i;

	ret = ft5x0x_i2c_Read(buf, 1, buf, CFG_POINT_READ_BUF);
	if (ret < 0) {
		DbgPrintk("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}
	memset(event, 0, sizeof(struct ts_event));
#if USE_EVENT_POINT
	event->touch_point = buf[2] & 0x07;
#else
	event->touch_point = buf[2] >>4;
#endif
	if (event->touch_point > CFG_MAX_TOUCH_POINTS)
	{
		event->touch_point = CFG_MAX_TOUCH_POINTS;
	}

	for (i = 0; i < event->touch_point; i++)
	{
		event->au16_x[i] = (s16)(buf[3 + 6*i] & 0x0F)<<8 | (s16)buf[4 + 6*i];
		event->au16_y[i] = (s16)(buf[5 + 6*i] & 0x0F)<<8 | (s16)buf[6 + 6*i];
		event->au8_touch_event[i] = buf[0x3 + 6*i] >> 6;
		event->au8_finger_id[i] = (buf[5 + 6*i])>>4;
	}

	event->pressure = 200;

	return 0;
}

/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/


#if CFG_SUPPORT_TOUCH_KEY
int ft5x0x_touch_key_process(struct input_dev *dev, int x, int y, int touch_event)
{
    int i;
    int key_id;

    if ( y < 517&&y > 497)
    {
        key_id = 1;
    }
    else if ( y < 367&&y > 347)
    {
        key_id = 0;
    }

    else if ( y < 217&&y > 197)
    {
        key_id = 2;
    }
    else if (y < 67&&y > 47)
    {
        key_id = 3;
    }
    else
    {
        key_id = 0xf;
    }

    for(i = 0; i <CFG_NUMOFKEYS; i++ )
    {
        if(tsp_keystatus[i])
        {
            input_report_key(dev, tsp_keycodes[i], 0);

            DbgPrintk("[FTS] %s key is release. Keycode : %d\n", tsp_keyname[i], tsp_keycodes[i]);

            tsp_keystatus[i] = KEY_RELEASE;
        }
        else if( key_id == i )
        {
            if( touch_event == 0)                                  // detect
            {
                input_report_key(dev, tsp_keycodes[i], 1);
                DbgPrintk( "[FTS] %s key is pressed. Keycode : %d\n", tsp_keyname[i], tsp_keycodes[i]);
                tsp_keystatus[i] = KEY_PRESS;
            }
        }
    }
    return 0;

}
#endif

static void ft5x0x_report_value(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	int i;
	static int pressed = 0;

	if (event->touch_point == 0 ){
		if (pressed == 1) {
			ft5x0x_ts_release();
			pressed = 0;
		}
		return;
	}

	for (i  = 0; i < event->touch_point; i++)
	{
		// LCD view area
		if (event->au16_x[i] < TSP_MAX_X && event->au16_y[i] < TSP_MAX_Y)
		{
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]
					* SCREEN_MAX_X / TSP_MAX_X);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]
					* SCREEN_MAX_Y / TSP_MAX_Y);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->au8_finger_id[i]);
			if (event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2)
			{
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			}
			else
			{
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			}
		}
		else //maybe the touch key area
		{
#if CFG_SUPPORT_TOUCH_KEY
			if (event->au16_x[i] >= SCREEN_MAX_X)
			{
				ft5x0x_touch_key_process(data->input_dev, event->au16_x[i], event->au16_y[i], event->au8_touch_event[i]);
			}
#endif
		}


		input_mt_sync(data->input_dev);
	}
	input_sync(data->input_dev);
	pressed = 1;

}	/*end ft5x0x_report_value*/


/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/
static void ft5x0x_ts_pen_irq_work(struct work_struct *work)
{

	int ret = -1;

	ret = ft5x0x_read_Touchdata();
	if (ret == 0) {
		ft5x0x_report_value();
	}
#if POLLING_OR_INTERRUPT
	del_timer(&test_timer);
	add_timer(&test_timer);
#else
	enable_irq(this_client->irq);
#endif

}

#if POLLING_OR_INTERRUPT
static void ft5x0x_polling(unsigned long data)
{
	struct ft5x0x_ts_data *ft5x0x_ts = i2c_get_clientdata(this_client);

	if (!work_pending(&ft5x0x_ts->pen_event_work)) {
		queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
	}
}
#endif
/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/
static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x0x_ts_data *ft5x0x_ts = dev_id;

	disable_irq_nosync(irq);

	if (!work_pending(&ft5x0x_ts->pen_event_work)) {
		queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
	}

	return IRQ_HANDLED;
}


static int ft5x0x_write_reg(unsigned char regaddr, unsigned char regvalue)
{
	unsigned char buf[2];
	buf[0] = regaddr;
	buf[1] = regvalue;
	return ft5x0x_i2c_Write(buf, sizeof(buf));
}

static int ft5x0x_read_reg(unsigned char regaddr, unsigned char * regvalue)
{
	return ft5x0x_i2c_Read(&regaddr, 1, regvalue, 1);
}

static int fts_ctpm_auto_clb(void)
{
    unsigned char uc_temp;
    unsigned char i ;

    printk("[FTS] start auto CLB.\n");
    msleep(200);
    ft5x0x_write_reg(0, 0x40);
    delay_qt_ms(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x4);  //write command to start calibration
    delay_qt_ms(300);
    for(i=0;i<100;i++)
    {
        ft5x0x_read_reg(0,&uc_temp);
        if ( ((uc_temp&0x70)>>4) == 0x0)  //return to normal mode, calibration finish
        {
            break;
        }
        delay_qt_ms(200);
        printk("[FTS] waiting calibration %d\n",i);

    }
    printk("[FTS] calibration OK.\n");

    msleep(300);
    ft5x0x_write_reg(0, 0x40);  //goto factory mode
    delay_qt_ms(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x5);  //store CLB result
    delay_qt_ms(300);
    ft5x0x_write_reg(0, 0x0); //return to normal mode
    msleep(300);
    printk("[FTS] store CLB result OK.\n");
    return 0;
}

/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/
static int
ft5x0x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x0x_ts_data *ft5x0x_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value;
	unsigned char uc_reg_addr;
#if CFG_SUPPORT_TOUCH_KEY
	int i;
#endif

	DbgPrintk("[FTS] ft5x0x_ts_probe, driver version is %s.\n", CFG_FTS_CTP_DRIVER_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft5x0x_ts = kzalloc(sizeof(struct ft5x0x_ts_data), GFP_KERNEL);

	if (!ft5x0x_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	this_client = client;
	i2c_set_clientdata(client, ft5x0x_ts);
//	this_client->irq = TOUCH_INT_IRQ;
	DbgPrintk("INT irq=%d\n", this_client->irq);
//	mutex_init(&ft5x0x_ts->device_mode_mutex);
	INIT_WORK(&ft5x0x_ts->pen_event_work, ft5x0x_ts_pen_irq_work);

	ft5x0x_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x0x_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
#if POLLING_OR_INTERRUPT
	DbgPrintk("Read TouchData by Polling\n");
#else
	err = request_irq(this_client->irq, ft5x0x_ts_interrupt, IRQF_TRIGGER_FALLING, "ft5x0x_ts", ft5x0x_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x0x_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}
	disable_irq(this_client->irq);
#endif
	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	ft5x0x_ts->input_dev = input_dev;

	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);

	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TRACKING_ID, 0, 5, 0, 0);


	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);

#if CFG_SUPPORT_TOUCH_KEY
    //setup key code area
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	input_dev->keycode = tsp_keycodes;
	for(i = 0; i < CFG_NUMOFKEYS; i++)
	{
		input_set_capability(input_dev, EV_KEY, ((int*)input_dev->keycode)[i]);
		tsp_keystatus[i] = KEY_RELEASE;
	}
#endif

	input_dev->name		= FT5X0X_NAME;		//dev_name(&client->dev)
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
				"ft5x0x_ts_probe: failed to register input device: %s\n",
				dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	DbgPrintk("==register_early_suspend =\n");
	//ft5x0x_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	//ft5x0x_ts->early_suspend.suspend = ft5x0x_ts_suspend;
	//ft5x0x_ts->early_suspend.resume	= ft5x0x_ts_resume;
	//register_early_suspend(&ft5x0x_ts->early_suspend);
#endif

	 msleep(150);  //make sure CTP already finish startup process

	//get some register information
	uc_reg_addr = FT5x0x_REG_FW_VER;
	ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
	DbgPrintk("[FTS] Firmware version = 0x%x\n", uc_reg_value);

	uc_reg_addr = FT5x0x_REG_POINT_RATE;
	ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
	DbgPrintk("[FTS] report rate is %dHz.\n", uc_reg_value * 10);

	uc_reg_addr = FT5X0X_REG_THGROUP;
	ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
	DbgPrintk("[FTS] touch threshold is %d.\n", uc_reg_value * 4);

#if POLLING_OR_INTERRUPT
	test_timer.function = ft5x06_polling;
	test_timer.expires = jiffies + HZ*2;//POLLING_CYCLE*100; // 100/10
	test_timer.data = 1;
	init_timer(&test_timer);
	add_timer(&test_timer);
#else
	enable_irq(this_client->irq);
#endif
	fts_ctpm_auto_clb();
	//you can add sysfs for test
	//ft5x0x_create_sysfs(client);
	DbgPrintk("[FTS] ==probe over =\n");
    return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);

exit_input_dev_alloc_failed:
	free_irq(this_client->irq, ft5x0x_ts);

exit_irq_request_failed:
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);

exit_create_singlethread:
	DbgPrintk("==singlethread error =\n");
	i2c_set_clientdata(client, NULL);
	kfree(ft5x0x_ts);

exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}
/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/
static int __devexit ft5x0x_ts_remove(struct i2c_client *client)
{
	struct ft5x0x_ts_data *ft5x0x_ts;
	DbgPrintk("==ft5x0x_ts_remove=\n");
	ft5x0x_ts = i2c_get_clientdata(client);
	//unregister_early_suspend(&ft5x0x_ts->early_suspend);
	//mutex_destroy(&ft5x0x_ts->device_mode_mutex);
	input_unregister_device(ft5x0x_ts->input_dev);
	kfree(ft5x0x_ts);
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
#if POLLING_OR_INTERRUPT
	del_timer(&test_timer);
#else
	free_irq(client->irq, ft5x0x_ts);
#endif

	return 0;
}

static const struct i2c_device_id ft5x0x_ts_id[] = {
	{ FT5X0X_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver ft5x0x_ts_driver = {
	.probe		= ft5x0x_ts_probe,
	.remove		= __devexit_p(ft5x0x_ts_remove),
	.id_table	= ft5x0x_ts_id,
	.driver	= {
		.name	= FT5X0X_NAME,
		.owner	= THIS_MODULE,
	},
};

/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/
static int __init ft5x0x_ts_init(void)
{
	int ret;
	DbgPrintk("==ft5x0x_ts_init==\n");
	ret = i2c_add_driver(&ft5x0x_ts_driver);
	DbgPrintk("ret=%d\n",ret);
	return ret;
}

/***********************************************************************************************
Name	:

Input	:


Output	:

function	:

***********************************************************************************************/
static void __exit ft5x0x_ts_exit(void)
{
	DbgPrintk("==ft5x0x_ts_exit==\n");
	i2c_del_driver(&ft5x0x_ts_driver);
}

module_init(ft5x0x_ts_init);
module_exit(ft5x0x_ts_exit);

MODULE_AUTHOR("<luowj@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");
