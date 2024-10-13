/*
 * leds_an30259a.c - driver for Panasonic AN30259A led control chip
 *
 * Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/leds.h>
#include <linux/leds-an30259a.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>

/* AN30259A register map */
#include "leds-an30259a_reg.h"

#define LED_DEFAULT_IMAX		0x01
#define LED_DEFAULT_CLK_MODE	0x00
#define LED_DEFAULT_CURRENT		0x28
#define LED_LOWPOWER_CURRENT	0x05
#define LED_OFFSET_CURRENT		0x00

#define MAX_SMARTGLOW_LEDS		4

struct i2c_client	*gclient[MAX_SMARTGLOW_LEDS];
int probecnt = 0;
struct an30259a_data	*g_an30259a_data;
unsigned char g_LEDState = 0;
extern struct class *sec_class;

enum an30259a_led_channel {
	LED_R,
	LED_G,
	LED_B,
	MAX_LED_CHANNEL,
};

struct an30259a_led {
	u8	channel;
	u8	intensity;
	u8	IsSleep;
	enum led_brightness	brightness;
	struct led_classdev	cdev;
	struct work_struct	brightness_work;
	struct work_struct	blink_work;
	struct	i2c_client	*client;   // TSN:: TEMP Change, as the container is not working as expected
	unsigned long delay_on_time_ms;
	unsigned long delay_off_time_ms;
};

struct an30259_led_conf {
	const char      *name;
	//const char      led_num;
	enum led_brightness   brightness;
	enum led_brightness  max_brightness;
	unsigned char   default_current;
	unsigned char   lowpower_current;
	unsigned char   offset_current;
	unsigned char   flags;
};

struct an30259a_data {
	struct	i2c_client	*client;
	struct	mutex	mutex;
	struct	an30259a_led	led_channel[MAX_LED_CHANNEL];
	struct	an30259a_led	led;
	struct  an30259_led_conf led_conf[MAX_LED_CHANNEL];
	char * name;
	u8		shadow_reg[AN30259A_REG_MAX];
	u8		iMax;
	u8		clk_mode;
	u8		led_num;
};

/**
* struct smartglow_input - user input for animation
* @color - color in 0xRRGGBB format
* @ontime - if 0, switch off the LED
*		Incase of blink, on time in ms. 
* 		For the smooth/breathing animation ontime/2 is for raise 
*		and ontime/2 is for falling intensity of light
* @offtime - if 0, led constant glow. else blink on time
* @repeat_count - In case of blink, how many times it has to blink.
*		If 0, blink forever.
* @led_num - which LED to be glow. 
*		If it is 0, all LED to be accessed.
* @reserved - reserved
**/
struct smartglow_input {
	u32 color;
	u16 ontime;
	u16 offtime;
	u16 repeat_count;
	//u16 reserved;
};

struct an30259a_smartglow_data {
	struct platform_device *pdata;
	struct mutex mutex;
	struct led_classdev	class_dev;
	struct work_struct animate_work;
	struct i2c_client *clients[MAX_SMARTGLOW_LEDS];
	struct smartglow_input led[MAX_SMARTGLOW_LEDS];
	u8 in_ledaccess;
	u8 led_state;
	u8 master_clk_led;
	u8 probed_led_cnt;
	u8 intensity;
	u8 IsSleep;
}*g_smartglow_data;

#include "SquricleFlash-an30259a_tuning.h"

// TSN :: SVC LED porting is pending
//#define SEC_LED_SPECIFIC
#define LED_DEEP_DEBUG

static void leds_on(struct i2c_client *client, 
	enum an30259a_led_channel channel, bool on, bool slopemode, u8 ledcc);

static inline struct an30259a_led *cdev_to_led(struct led_classdev *cdev)
{
	return container_of(cdev, struct an30259a_led, cdev);
}

static inline struct an30259a_led *cdev_to_data(struct led_classdev *cdev)
{
	return container_of(
		container_of(cdev, struct an30259a_led, cdev),
		struct an30259a_data,
		client
		);
}

#ifdef LED_DEEP_DEBUG
/**
* an30259a_debug - Display all register values for debugging 
**/
static void an30259a_debug(struct i2c_client *client)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	int ret;
	u8 buff[21] = {0,};
	ret = i2c_smbus_read_i2c_block_data(client,
		AN30259A_REG_SRESET|AN30259A_CTN_RW_FLG,
		sizeof(buff), buff);
	if (ret != sizeof(buff)) {
		dev_err(&data->client->dev,
			"%s: failure on i2c_smbus_read_i2c_block_data\n",
			__func__);
	}
	print_hex_dump(KERN_ERR, "an30259a: ",
		DUMP_PREFIX_OFFSET, 32, 1, buff,
		sizeof(buff), false);
}
#endif

/**
* leds_set_imax - To set the IMAX register value
**/
static int leds_set_imax(struct i2c_client *client, u8 imax)
{
	int ret;
	struct an30259a_data *data = i2c_get_clientdata(client);

	data->shadow_reg[AN30259A_REG_SEL] &= ~AN30259A_MASK_IMAX;
	data->shadow_reg[AN30259A_REG_SEL] |= imax << LED_IMAX_SHIFT;

#if 0	
	ret = i2c_smbus_write_byte_data(client, AN30259A_REG_SEL,
			data->shadow_reg[AN30259A_REG_SEL]);
	if (ret < 0) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c write\n",
			__func__);
	}
#endif 	
	return 0;
}


/**
* leds_set_clk - To set the clk out put for the 1st led and 
*		enable external PWM for the Other LEDs.
**/
static int leds_set_clk(struct i2c_client *client, u8 clk_mode)
{
	int ret;
	struct an30259a_data *data = i2c_get_clientdata(client);

	data->shadow_reg[AN30259A_REG_SEL] &= ~AN30259A_MASK_CLK_MODE;
	data->shadow_reg[AN30259A_REG_SEL] |= clk_mode << LED_CLK_MODE_SHIFT;

#if 0	
	ret = i2c_smbus_write_byte_data(client, AN30259A_REG_SEL,
			data->shadow_reg[AN30259A_REG_SEL]);
	if (ret < 0) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c write\n",
			__func__);
	}
#endif
	return 0;
}

static int __inline an30259a_reset(struct i2c_client *client)
{	
	struct an30259a_data *data = i2c_get_clientdata(client);
	int ret;
	
	/* Reset the IC */
	if(unlikely((ret = i2c_smbus_write_byte_data(client, 
			AN30259A_REG_SRESET, AN30259A_SRESET)) < 0)) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c reset\n",
			__func__);
	}
			
	/* Make a copy of IC register values to the driver */	
	if(unlikely((ret = i2c_smbus_read_i2c_block_data(client,
			AN30259A_REG_SRESET | AN30259A_CTN_RW_FLG,
			AN30259A_REG_MAX, data->shadow_reg)) < 0)) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c read block(ledxcc)\n",
			__func__);
	}
	
	/* Set imax */
	leds_set_imax(client, data->iMax);
	
	return ret;
}

/**
* smartglow_reset - Reset all an30259a LEDs.
**/
static void smartglow_reset()
{	
#if 0
	an30259a_reset(g_smartglow_data->clients[0]);
	an30259a_reset(g_smartglow_data->clients[1]);
	an30259a_reset(g_smartglow_data->clients[2]);
	an30259a_reset(g_smartglow_data->clients[3]);
#else
	struct an30259a_data *data;
	static u8 rstdata[] = {
		AN30259A_REG_SRESET,
		AN30259A_SRESET
	};
	static struct i2c_msg smartglow_i2c_rst_chain[] = {	
		{
			.addr = 0x33,
			.flags = 0,
			.len = 2,
			.buf = rstdata,
		},
		{
			.addr = 0x30,
			.flags = 0,
			.len = 2,
			.buf = rstdata,
		},
		{
			.addr = 0x31,
			.flags = 0,
			.len = 2,
			.buf = rstdata,
		},
		{
			.addr = 0x32,
			.flags = 0,
			.len = 2,
			.buf = rstdata,
		},
	};
	//mutex_lock(&g_smartglow_data->mutex);
	/* Taking 1st index, because some development HW does not have 0th LED */
	i2c_transfer(g_smartglow_data->clients[1]->adapter,
			smartglow_i2c_rst_chain, 4);
		
	data = i2c_get_clientdata(g_smartglow_data->clients[0]);
	//data->shadow_reg[AN30259A_REG_SEL] = ((data->iMax << LED_IMAX_SHIFT) |
		//						(data->clk_mode << LED_CLK_MODE_SHIFT));
	data->shadow_reg[AN30259A_REG_SEL] = 0;
	data = i2c_get_clientdata(g_smartglow_data->clients[1]);
	data->shadow_reg[AN30259A_REG_SEL] = 0;
	data = i2c_get_clientdata(g_smartglow_data->clients[2]);
	data->shadow_reg[AN30259A_REG_SEL] = 0;
	data = i2c_get_clientdata(g_smartglow_data->clients[3]);
	data->shadow_reg[AN30259A_REG_SEL] = 0;	
	//mutex_unlock(&g_smartglow_data->mutex);
			
	//TSN: REM: Shadow reg copy may not required 
	
#endif	

	
	//memset(g_smartglow_data->led, 0, sizeof(g_smartglow_data->led));
	//g_smartglow_data->led_state = 0;
}

/**
* leds_i2c_write_all - Writes all an30259a register values through I2C 
**/
static int leds_i2c_write_all(struct i2c_client *client)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	int ret;

	/*we need to set all the configs setting first, then LEDON later*/
	mutex_lock(&data->mutex);
	if( unlikely((ret = i2c_smbus_write_i2c_block_data(client,
			AN30259A_REG_SEL | AN30259A_CTN_RW_FLG,
			AN30259A_REG_MAX - AN30259A_REG_SEL,
			&data->shadow_reg[AN30259A_REG_SEL]))) < 0) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c block write, %d\n",
			__func__, ret);
		goto exit;
	}
	
	if(unlikely((ret = i2c_smbus_write_byte_data(client, AN30259A_REG_LEDON,
					data->shadow_reg[AN30259A_REG_LEDON]))) < 0) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c byte write, %d\n",
			__func__, ret);
		goto exit;
	}
	mutex_unlock(&data->mutex);
	return 0;

exit:
	mutex_unlock(&data->mutex);
	return ret;
}


/**
* smartglow_an30259a_i2c_write_all - Writes all an30259a register values 
*   through I2C 
**/
static int smartglow_an30259a_i2c_write_all()
{
#if 0
	leds_i2c_write_all(g_smartglow_data->clients[1]);
	leds_i2c_write_all(g_smartglow_data->clients[2]);
	leds_i2c_write_all(g_smartglow_data->clients[3]);	
	leds_i2c_write_all(g_smartglow_data->clients[0]); // Move Master to Down
#else	
#if 1	
	int i;
	struct an30259a_data *data;
	int ret;
	
	//mutex_lock(&g_smartglow_data->mutex);
	
	/* we need to set all the configs setting first, then LEDON later*/
	for(i = 0; i < MAX_SMARTGLOW_LEDS; i++)
	{
		data = i2c_get_clientdata(g_smartglow_data->clients[i]);
		
		if( unlikely((ret = i2c_smbus_write_i2c_block_data(
				g_smartglow_data->clients[i],
				AN30259A_REG_SEL | AN30259A_CTN_RW_FLG,
				AN30259A_REG_MAX - AN30259A_REG_SEL,
				&data->shadow_reg[AN30259A_REG_SEL]))) < 0) {
			dev_err(&g_smartglow_data->clients[i]->adapter->dev,
				"%s: failure on i2c block write, %d\n",
				__func__, ret);
			//goto exit;
		}
	}
	
	for(i = MAX_SMARTGLOW_LEDS-1; i >= 0; i--)
	{
		data = i2c_get_clientdata(g_smartglow_data->clients[i]);
		
		if(unlikely((ret = i2c_smbus_write_byte_data(
						g_smartglow_data->clients[i], 
						AN30259A_REG_LEDON,
						data->shadow_reg[AN30259A_REG_LEDON]))) < 0) {
			dev_err(&g_smartglow_data->clients[i]->adapter->dev,
				"%s: failure on i2c byte write, %d\n",
				__func__, ret);
			//goto exit;
		}

	}
	
	//mutex_unlock(&g_smartglow_data->mutex);
	return ret;
#else	
/* Try to use I2C Chain instead of multiple broken I2C calls */	
	//ret = i2c_transfer(client->adapter, pos_msg, READ_CMD_MSG_LEN);
#error "I2C chain write is not used yet"	
#endif	
#endif	
}

/**
* an30259a_reset_register_work - Reset an30259a register values through I2C 
**/
static void an30259a_reset_register_work(struct i2c_client *client)
{
	int retval;
	
	leds_on(client, LED_R, false, false, 0);
	leds_on(client, LED_G, false, false, 0);
	leds_on(client, LED_B, false, false, 0);

	retval = leds_i2c_write_all(client);
	if (retval)
		printk(KERN_WARNING "leds_i2c_write_all failed\n");
}

/**
* an30259a_standby - Reset an30259a register values through I2C 
**/
static void an30259a_standby(struct i2c_client *client)
{
	int retval;
	
	leds_on(client, LED_R, true, false, 0);
	leds_on(client, LED_G, true, false, 0);
	leds_on(client, LED_B, true, false, 0);

	retval = leds_i2c_write_all(client);
	if (retval)
		printk(KERN_WARNING "leds_i2c_write_all failed\n");
}

/**
 * leds_set_slope_mode() - sets correct values to corresponding shadow registers.
 * @channel: stands for LED_R or LED_G or LED_B.
 * @delay: represents for starting delay time in multiple of .5 second.
 * @dutymax: led at slope lighting maximum PWM Duty setting.
 * @dutymid: led at slope lighting middle PWM Duty setting.
 * @dutymin: led at slope lighting minimum PWM Duty Setting.
 * @slptt1: total time of slope operation 1 and 2, in multiple of .5 second.
 * @slptt2: total time of slope operation 3 and 4, in multiple of .5 second.
 * @dt1: detention time at each step in slope operation 1, in multiple of 4ms.
 * @dt2: detention time at each step in slope operation 2, in multiple of 4ms.
 * @dt3: detention time at each step in slope operation 3, in multiple of 4ms.
 * @dt4: detention time at each step in slope operation 4, in multiple of 4ms.
 **/
static void leds_set_slope_mode(struct i2c_client *client,
				enum an30259a_led_channel channel, u8 delay,
				u8 dutymax, u8 dutymid, u8 dutymin,
				u8 slptt1, u8 slptt2,
				u8 dt1, u8 dt2, u8 dt3, u8 dt4)
{
	struct an30259a_data *data = i2c_get_clientdata(client);

	data->shadow_reg[AN30259A_REG_LED1CNT1 + channel * 4] =
							dutymax << 4 | dutymid;
	data->shadow_reg[AN30259A_REG_LED1CNT2 + channel * 4] =
							delay << 4 | dutymin;
	data->shadow_reg[AN30259A_REG_LED1CNT3 + channel * 4] = dt2 << 4 | dt1;
	data->shadow_reg[AN30259A_REG_LED1CNT4 + channel * 4] = dt4 << 4 | dt3;
	data->shadow_reg[AN30259A_REG_LED1SLP + channel] = slptt2 << 4 | slptt1;
}

/**
* an30259a_set_color - To Set the RGB LED current
**/
static void __inline an30259a_set_color(struct i2c_client *client, u32 color)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	
	data->shadow_reg[AN30259A_REG_LED_R] = AN30259A_EXTRACT_R(color);
	data->shadow_reg[AN30259A_REG_LED_G] = AN30259A_EXTRACT_G(color);
	data->shadow_reg[AN30259A_REG_LED_B] = AN30259A_EXTRACT_B(color);
}

/**
* an30259a_set_constant_current - To Set the LED intensity and enable them
**/
static void __inline an30259a_set_constant_current(struct i2c_client *client)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	
	/* Set the LED mode register for constant current */
	data->shadow_reg[AN30259A_REG_LEDON] &= ~ALL_LED_SLOPE_MODE;

#if 0	// TSN: Check.... If current will not be zero, enable only current particular LED
	/* Enable each color LED, if non-zero */
	if(data->shadow_reg[AN30259A_REG_LED_R])
		data->shadow_reg[AN30259A_REG_LEDON] |= LED_ON << LED_R;
	else
		data->shadow_reg[AN30259A_REG_LEDON] &= ~(LED_ON << LED_R);
	
	if(data->shadow_reg[AN30259A_REG_LED_G])
		data->shadow_reg[AN30259A_REG_LEDON] |= LED_ON << LED_G;
	else
		data->shadow_reg[AN30259A_REG_LEDON] &= ~(LED_ON << LED_G);
	
	if(data->shadow_reg[AN30259A_REG_LED_B])
		data->shadow_reg[AN30259A_REG_LEDON] |= LED_ON << LED_B;
	else
		data->shadow_reg[AN30259A_REG_LEDON] &= ~(LED_ON << LED_B);
	
#else // TSN: Check.... If current will be really zero, then, can write Enable ALL
	data->shadow_reg[AN30259A_REG_LEDON] |= ALL_LED_ON;
#endif
}

/**
* an30259a_set_slope_current - To Set the LED intensity and enable them
**/
static void an30259a_set_slope_current(struct i2c_client *client, 
				u16 ontime, u16 offtime)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	
	u8 delay, dutymax, dutymid, dutymin, slptt1, slptt2, 
			dt1, dt2, dt3, dt4;
			
	delay = 0;
	
	dutymid = dutymax = 0xF;
	dt2 = dt3 = 0;

	/* Keep duty min always zero, as in blink has to be off for a while */
	dutymin = 0;
	slptt1 = ontime/500;
	slptt2 = offtime/500;
	if(!slptt1) slptt1 = 1;
	if(!slptt2) slptt2 = 1;
	
	if(slptt1 > 1)	
	{
		slptt2 += (slptt1 >> 1);
		
		// Check if odd number, store ceiling value to slptt1
		// in blink, for breathing effect ontime/2 is used for rasing and falling
		slptt1 = (slptt1 & 1) ? (slptt1 >> 1) + 1 : (slptt1 >> 1);
		
		dt4 = slptt1;
	}
	else 
	{
		dt4 = 0;
	}
	dt1 = slptt1;
	
	data->shadow_reg[AN30259A_REG_LED1CNT1] = 
		data->shadow_reg[AN30259A_REG_LED2CNT1] = 
		data->shadow_reg[AN30259A_REG_LED3CNT1] = dutymax << 4 | dutymid;

	data->shadow_reg[AN30259A_REG_LED1CNT2] =  
		data->shadow_reg[AN30259A_REG_LED2CNT2] = 
		data->shadow_reg[AN30259A_REG_LED3CNT2] = delay << 4 | dutymin;
	
	data->shadow_reg[AN30259A_REG_LED1CNT3] = 
		data->shadow_reg[AN30259A_REG_LED2CNT3] = 	
		data->shadow_reg[AN30259A_REG_LED3CNT3] = dt2 << 4 | dt1;
	
	data->shadow_reg[AN30259A_REG_LED1CNT4] = 
		data->shadow_reg[AN30259A_REG_LED2CNT4] = 
		data->shadow_reg[AN30259A_REG_LED3CNT4] = dt4 << 4 | dt3;
	
	data->shadow_reg[AN30259A_REG_LED1SLP] = 
		data->shadow_reg[AN30259A_REG_LED2SLP] =  
		data->shadow_reg[AN30259A_REG_LED3SLP] = slptt2 << 4 | slptt1;

#if 0	// TSN: Check.... If current will not be zero, enable only current particular LED	
	data->shadow_reg[AN30259A_REG_LEDON] |= ALL_LED_SLOPE_MODE;
	
	/* Enable each color LED, if non-zero */
	if(data->shadow_reg[AN30259A_REG_LED_R])
		data->shadow_reg[AN30259A_REG_LEDON] |= LED_ON << LED_R;
	else
		data->shadow_reg[AN30259A_REG_LEDON] &= ~(LED_ON << LED_R);
	
	if(data->shadow_reg[AN30259A_REG_LED_G])
		data->shadow_reg[AN30259A_REG_LEDON] |= LED_ON << LED_G;
	else
		data->shadow_reg[AN30259A_REG_LEDON] &= ~(LED_ON << LED_G);
	
	if(data->shadow_reg[AN30259A_REG_LED_B])
		data->shadow_reg[AN30259A_REG_LEDON] |= LED_ON << LED_B;
	else
		data->shadow_reg[AN30259A_REG_LEDON] &= ~(LED_ON << LED_B);
	
#else // TSN: Check.... If current will be really zero, then, can write Enable ALL
	data->shadow_reg[AN30259A_REG_LEDON] |= ALL_LED_SLOPE_MODE | ALL_LED_ON;
#endif
	
}

/**
* leds_on - To Set the LED intensity and enable them
**/
static void leds_on(struct i2c_client *client, enum an30259a_led_channel channel, 
				bool on, bool slopemode, u8 ledcc)
{
	struct an30259a_data *data = i2c_get_clientdata(client);

	if (ledcc > 0)
		ledcc += data->led_conf[channel].offset_current;

	if (on)
		data->shadow_reg[AN30259A_REG_LEDON] |= LED_ON << channel;
	else {
		data->shadow_reg[AN30259A_REG_LEDON] &= ~(LED_ON << channel);
		data->shadow_reg[AN30259A_REG_LED1CNT2 + channel * 4] &=
							~AN30259A_MASK_DELAY;
	}
	if (slopemode)
		data->shadow_reg[AN30259A_REG_LEDON] |= LED_SLOPE_MODE << channel;
	else
		data->shadow_reg[AN30259A_REG_LEDON] &=
						~(LED_SLOPE_MODE << channel);

	data->shadow_reg[AN30259A_REG_LED1CC + channel] = ledcc;
}

/**
* smartglow_set_brightness - Get the brightness value from upper 
* 		layer through sysfs and schedule the work
**/
void smartglow_set_brightness(struct led_classdev *cdev,
			enum led_brightness brightness)
{
	printk(KERN_ERR" [SmartGlow] No function Yet!.");
	return;
}

/**
* an30259a_set_brightness - Get the brightness value from upper 
* 		layer through sysfs and schedule the work
**/
void an30259a_set_brightness(struct led_classdev *cdev,
			enum led_brightness brightness)
{
	struct an30259a_led *led = cdev_to_led(cdev);
	
	if(led->IsSleep)
	{
		printk(KERN_ERR" [SmartGlow] Cant glow in sleep");
		return; 
	}
	
	led->brightness = brightness;
	printk(KERN_ERR" [SmartGlow] an30259a_set_brightness %X", led->brightness);
	schedule_work(&led->brightness_work);
}

/**
* an30259a_led_brightness_work - set the brightness value written 
*		from upper layer
**/
static void an30259a_led_brightness_work(struct work_struct *work)
{
	struct an30259a_led *led = container_of(work,
			struct an30259a_led, brightness_work);
			
	printk(KERN_ERR"[SmartGlow] an30259a_led_brightness_work %X, LED STATE : %d", led->brightness, g_LEDState);
#if 0 // TSN : Temp changes as kernel panic				
	struct an30259a_data *data = (struct an30259a_data *)container_of( 
			(led - led->channel), struct an30259a_data, led_channel);
	
	if(led->channel == MAX_LED_CHANNEL)
	{
		unsigned char intensity = (led->brightness && 0xFF000000);
		
		/* Get the fixed color map */
		led->brightness = an30259a_fixed_color_map(led->brightness & 0x00FFFFFF);
		
		/* Restore Intensity, as blink may require it */
		led->brightness |= intensity;
		intensity = (led->brightness >> 24) / 0xFF ;
		
		leds_on(data->client, LED_R, true, false, 
				((led->brightness >> 16) & 0xFF) * intensity);
		leds_on(data->client, LED_G, true, false, 
				((led->brightness >> 8) & 0xFF) * intensity);
		leds_on(data->client, LED_B, true, false, 
				(led->brightness & 0xFF) * intensity );
	}
	else
	{
		leds_on(data->client, led->channel, true, false, led->brightness);
	}
	leds_i2c_write_all(data->client);
#else
	if(led->channel == MAX_LED_CHANNEL)
	{
		struct an30259a_data *data = (struct an30259a_data *)container_of( 
						led, struct an30259a_data, led);
						
		if(led->brightness)
		{		
			enum led_brightness color;
				
			/* Set Master imax */
			leds_set_imax(data->client, data->iMax);

			/* set Master clk mode */
			leds_set_clk(data->client, data->clk_mode);
			
			/* If ALL LED OFF to 1st LED ON && current client is not the master */
			if(!g_LEDState && (g_an30259a_data->client != led->client))
			{
				printk(KERN_ERR"[SmartGlow]Master is written");
				
				/* Master LED to stand-by mode */
				an30259a_reset_register_work(g_an30259a_data->client);
				
				/* Set Master imax */
				leds_set_imax(g_an30259a_data->client, g_an30259a_data->iMax);

				/* set Master clk mode */
				leds_set_clk(g_an30259a_data->client, g_an30259a_data->clk_mode);
				
				/* Enable */
				an30259a_standby(g_an30259a_data->client);
			}
			
			/* Keep The LED ON status */
			g_LEDState |= 1 << (data->led_num - 1);
				
			/* Get the fixed color map */
			color = an30259a_fixed_color_map( \
								led->brightness, led->intensity);	
			leds_on(led->client, LED_R, true, false, AN30259A_EXTRACT_R(color));
			leds_on(led->client, LED_G, true, false, AN30259A_EXTRACT_G(color));
			leds_on(led->client, LED_B, true, false, AN30259A_EXTRACT_B(color));
		}
		else
		{
			/* Update the LED ON status */
			g_LEDState &= ~(1 << (data->led_num - 1));
			
			/* Put the slave LED to sleep state */
			if(g_an30259a_data->client != led->client)
			{
				/* Disable the LED state */
				an30259a_reset_register_work(led->client);
				
				an30259a_reset(led->client);
			}
			else
			{
				/* To Keep the Master in Stand by mode */
				an30259a_standby(g_an30259a_data->client);
			}
			
			/* If all the LEDs are Off, Disable the Master too */
			if(!g_LEDState)
			{
				/* Disable the LED state */
				an30259a_reset_register_work(g_an30259a_data->client);
				
				an30259a_reset(g_an30259a_data->client);
			}
			
			return;
		}
	}
	else
	{
		leds_on(led->client, led->channel, true, false, led->brightness);
	}
	leds_i2c_write_all(led->client);

#endif	
}

/**
* an30259a_led_blink_work - set the brightness value written 
*		from upper layer
**/
static void an30259a_led_blink_work(struct work_struct *work)
{
	struct an30259a_led *led = container_of(work,
			struct an30259a_led, blink_work);
	unsigned int ontime = led->delay_on_time_ms;
	unsigned int offtime = led->delay_off_time_ms;
	
	printk(KERN_ERR"[SmartGlow] an30259a_led_blink_work %X, LED State: %d", led->brightness, g_LEDState);

	/* Breathing effect 
		OnTime/4, OnTime/2, OnTime/4, OffTime
		Raise, Stay, Hold, Nill
	*/
	
	// breath Factor is in 1/4. Then, Converting to 4ms scale
	unsigned int breathfactor = (ontime >> 2) >> 2;
	unsigned int slptt1 = ontime/500;
	unsigned int slptt2 = offtime/500;
	
	unsigned int dt1 = 4;
	unsigned int dt2 = 0; 
	unsigned int dt3 = 0; 
	unsigned int dt4 =  0; 
;
	unsigned int dutymax = 0xF;
	unsigned int dutymid = 0xF; //0x4;
	unsigned int dutymin = 0;
	

	if(led->channel == MAX_LED_CHANNEL)
	{
		struct an30259a_data *data = (struct an30259a_data *)container_of( 
						led, struct an30259a_data, led);
		
		if(led->brightness)
		{		
			enum led_brightness color;
				
			/* Set Master imax */
			leds_set_imax(data->client, data->iMax);

			/* set Master clk mode */
			leds_set_clk(data->client, data->clk_mode);
			
			/* If ALL LED OFF to 1st LED ON && current client is not the master */
			if(!g_LEDState && (g_an30259a_data->client != led->client))
			{
				printk(KERN_ERR"[SmartGlow]Master is written");
				
				/* Master LED to stand-by mode */
				an30259a_reset_register_work(g_an30259a_data->client);
				
				/* Set Master imax */
				leds_set_imax(g_an30259a_data->client, g_an30259a_data->iMax);

				/* set Master clk mode */
				leds_set_clk(g_an30259a_data->client, g_an30259a_data->clk_mode);
				
				/* Enable Slope Stnad by mode */				
				leds_on(g_an30259a_data->client, LED_R, true, true, 0);
				leds_set_slope_mode(g_an30259a_data->client, LED_R, 0,  dutymax, dutymid, 0,
					slptt2, slptt2, dt1, dt2, dt3, dt4);
					
				leds_on(g_an30259a_data->client, LED_G, true, true, 0);
				leds_set_slope_mode(g_an30259a_data->client, LED_G, 0,  dutymax, dutymid, 0,
						slptt2, slptt2, dt1, dt2, dt3, dt4);
					
				leds_on(g_an30259a_data->client, LED_B, true, true, 0);
				leds_set_slope_mode(g_an30259a_data->client, LED_B, 0,  dutymax, dutymid, 0,
						slptt2, slptt2, dt1, dt2, dt3, dt4);
			}
			
			/* Keep The LED ON status */
			g_LEDState |= 1 << (data->led_num - 1);
			
			/* Get the fixed color map */
			color = an30259a_fixed_color_map(
							led->brightness, led->intensity);	
			
			printk(KERN_ERR "%s: %u %u %u %u %u &u %u %u &u %u %u %u %u", __func__,
					  AN30259A_EXTRACT_R(color), 
					  AN30259A_EXTRACT_G(color), 
					  AN30259A_EXTRACT_B(color), 
					  0, dutymax, dutymid, dutymin,
					slptt1, slptt2, dt1, dt2,  dt3, dt4);		
			
			leds_on(led->client, LED_R, true, true, 
					AN30259A_EXTRACT_R(color));
			leds_set_slope_mode(led->client, LED_R, 0,  dutymax, dutymid, 0,
				slptt2, slptt2, dt1, dt2, dt3, dt4);
				
			leds_on(led->client, LED_G, true, true, 
					AN30259A_EXTRACT_G(color));
			leds_set_slope_mode(led->client, LED_G, 0,  dutymax, dutymid, 0,
					slptt2, slptt2, dt1, dt2, dt3, dt4);
				
			leds_on(led->client, LED_B, true, true, 
					AN30259A_EXTRACT_B(color));
			leds_set_slope_mode(led->client, LED_B, 0,  dutymax, dutymid, 0,
					slptt2, slptt2, dt1, dt2, dt3, dt4);
		}
		else
		{
			/* Update the LED ON status */
			g_LEDState &= ~(1 << (data->led_num - 1));
			
			/* Put the slave LED to sleep state */
			if(g_an30259a_data->client != led->client)
			{
				/* Disable the LED state */
				an30259a_reset_register_work(led->client);
			
				an30259a_reset(led->client);
			}
			else
			{
				/* Just disable the out put */
				leds_on(g_an30259a_data->client, LED_R, true, true, 0);
				leds_on(g_an30259a_data->client, LED_G, true, true, 0);
				leds_on(g_an30259a_data->client, LED_B, true, true, 0);
			}
			
			/* If all the LEDs are Off, Disable the Master too */
			if(!g_LEDState)
			{
				/* Disable the LED state */
				an30259a_reset_register_work(g_an30259a_data->client);
				
				an30259a_reset(g_an30259a_data->client);
			}
			
			return;
		}
	}
	else
	{
		leds_on(led->client, led->channel, true, true, led->brightness);
	}
	leds_i2c_write_all(led->client);
}

/* Added for led common class */
/**
* led_delay_on_show - returns blink OFF time ON sysfs read.
**/
static ssize_t led_delay_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct an30259a_led *led = cdev_to_led(led_cdev);

	return snprintf(buf, 10, "%lu\n", led->delay_on_time_ms);
}


static ssize_t led_reg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct an30259a_led *led = cdev_to_led(led_cdev);
	unsigned int OneGo, r, g, b, delay, dutymax, dutymid, dutymin,
				slptt1, slptt2,	dt1, dt2, dt3, dt4;
	int i;

	sscanf(buf, "%u %u %u %u %u %u %u %u %u %u %u %u %u %u",
				 &OneGo, &r, &g, &b, &delay, &dutymax, &dutymid, &dutymin,
				&slptt1, &slptt2, &dt1, &dt2,  &dt3, &dt4);
	printk(KERN_ERR "%s: %u %u %u %u %u %u %u %u %u %u %u %u %u %u", __func__,
				  OneGo, r, g, b, delay, dutymax, dutymid, dutymin,
				slptt1, slptt2, dt1, dt2,  dt3, dt4);
	printk(KERN_ERR"%s : %s",__func__, buf); 

	for(i=0;i<probecnt;i++)
	{
		struct an30259a_data *data = i2c_get_clientdata(gclient[i]);
		
		/* Set Master imax */
		leds_set_imax(gclient[i], data->iMax);

		/* set Master clk mode */
		leds_set_clk(gclient[i], data->clk_mode);
	}

	if(!OneGo)
	{
		an30259a_reset_register_work(led->client);
		
		leds_on(led->client, LED_R, true, true, r);
		leds_set_slope_mode(led->client, LED_R, delay,
			dutymax, dutymid, dutymin, slptt1, slptt2, dt1, dt2, dt3, dt4);
		
		leds_on(led->client, LED_G, true, true, g);
		leds_set_slope_mode(led->client, LED_G, delay,
			dutymax, dutymid, dutymin, slptt1, slptt2, dt1, dt2, dt3, dt4);
		
		leds_on(led->client, LED_B, true, true, b);
		leds_set_slope_mode(led->client, LED_B, delay,
			dutymax, dutymid, dutymin, slptt1, slptt2, dt1, dt2, dt3, dt4);
			
		leds_i2c_write_all(led->client);
	}
	else
	{
		/* prepare and write */
		for(i=0;i<probecnt;i++)
		{
			an30259a_reset_register_work(gclient[i]);
			
			leds_on(gclient[i], LED_R, true, true, r);
			leds_set_slope_mode(gclient[i], LED_R, delay,
				dutymax, dutymid, dutymin, slptt1, slptt2, dt1, dt2, dt3, dt4);
			
			leds_on(gclient[i], LED_G, true, true, g);
			leds_set_slope_mode(gclient[i], LED_G, delay,
				dutymax, dutymid, dutymin, slptt1, slptt2, dt1, dt2, dt3, dt4);
			
			leds_on(gclient[i], LED_B, true, true, b);
			leds_set_slope_mode(gclient[i], LED_B, delay,
				dutymax, dutymid, dutymin, slptt1, slptt2, dt1, dt2, dt3, dt4);
		}
		
		for(i=0;i<probecnt;i++)
		{
			leds_i2c_write_all(gclient[i]);
		}
	}
	
	return len;
}

/**
* led_delay_on_store - stores the ON time on blink.
*  		delay_on will be used by led_blink_store
**/
static ssize_t led_delay_on_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct an30259a_led *led = cdev_to_led(led_cdev);
	unsigned long time;

	if (kstrtoul(buf, 0, &time))
		return -EINVAL;

	led->delay_on_time_ms = (int)time;
	return len;
}

/**
* led_delay_off_show - returns blink OFF time on sysfs read.
**/
static ssize_t led_delay_off_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct an30259a_led *led = cdev_to_led(led_cdev);

	return snprintf(buf, 10, "%lu\n", led->delay_off_time_ms);
}

/**
* led_delay_off_store - stores the OFF time on blink.
*  		delay_off will be used by led_blink_store
**/
static ssize_t led_delay_off_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct an30259a_led *led = cdev_to_led(led_cdev);
	unsigned long time;

	if (kstrtoul(buf, 0, &time))
		return -EINVAL;

	led->delay_off_time_ms = (int)time;

	return len;
}

/**
* led_intensity_store - set the LED intensity
**/
static ssize_t led_intensity_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct an30259a_led *led = cdev_to_led(led_cdev);
	unsigned int intensity;
	
	if (kstrtoul(buf, 0, &intensity))
		return -EINVAL;

	led->intensity = intensity;

	return len;
}

/**
* led_intensity_show - set the LED intensity
**/
static ssize_t led_intensity_show(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct an30259a_led *led = cdev_to_led(led_cdev);

	return snprintf(buf, 10, "%lu\n", led->intensity);
}


/**
* led_blink_store - set/unset blink request through sysfs,
*		blink will take previously set delay_on and delay_on time
* @buf - if integer value is 1, trigger blink. disable, if 0.
**/
static ssize_t led_blink_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct an30259a_led *led = cdev_to_led(led_cdev);
	unsigned long blink_set;
	
	if(led->IsSleep)
	{
		printk(KERN_ERR" [SmartGlow] led_blink_store Cant Blink in sleep");
		return len;
	}

	if(led->channel == MAX_LED_CHANNEL){
		sscanf(buf, "%u %u %u", &blink_set, &led->delay_on_time_ms, \
				&led->delay_off_time_ms);
	}
	else {
		if (kstrtoul(buf, 0, &blink_set))
			return -EINVAL;
	}
	
	if (!blink_set) {
		led->delay_on_time_ms = LED_OFF;
		/* write dummy data, as both On time and off time 
			can not be 0 for blink disable */
		led->delay_off_time_ms = 1000;
		an30259a_set_brightness(led_cdev, LED_OFF);
#if defined(AN30259A_USE_HW_BLINK)		
		return len;
#else		
		//led_stop_software_blink(led_cdev);
#endif	
	}
	
#if defined(AN30259A_USE_HW_BLINK)
	//led->brightness = blink_set;
	printk(KERN_ERR" [SmartGlow] led_blink_store %X", blink_set);
	schedule_work(&led->blink_work);
#else
	led_blink_set(led_cdev,
			&led->delay_on_time_ms, &led->delay_off_time_ms);
#endif	

	return len;
}

/**
* Device attribute set for sysfs interface for blinking
**/
// TSN :: Need to check for CLASS_ATTR Vs DEVICE_ATTR, 
//	even though we are registering class
/* permission for sysfs node */
static DEVICE_ATTR(delay_on, 0644, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, 0644, led_delay_off_show, led_delay_off_store);
static DEVICE_ATTR(blink, 0644, NULL, led_blink_store);
static DEVICE_ATTR(intensity, 0644, led_intensity_show, led_intensity_store);
static DEVICE_ATTR(reg, 0644, NULL, led_reg_store);

static struct attribute *led_class_attrs[] = {
	&dev_attr_delay_on.attr,
	&dev_attr_delay_off.attr,
	&dev_attr_blink.attr,
	&dev_attr_intensity.attr,
	&dev_attr_reg.attr,
	NULL,
};

static struct attribute_group common_led_attr_group = {
	.attrs = led_class_attrs,
};

/**
* an30259a_smartglow_config_led - Animate smartglow by the config written 
*		from upper layer
**/
int an30259a_smartglow_config_led(u8 led_num)
{
	int master_led;
	
	//if(!g_smartglow_data->led[led_num].color || !g_smartglow_data->led[led_num].ontime)	
	if(!g_smartglow_data->led[led_num].color)	
	{
		// Switch off the LED
		
		if(g_smartglow_data->master_clk_led == led_num) 
		{
			/* Update the LED ON status */
			g_smartglow_data->led_state &= ~(1 << led_num);
				
			// If all LEDs are switched off, can simply power off
			if(g_smartglow_data->led_state)	
			{
				for(master_led = MAX_SMARTGLOW_LEDS-1; master_led >= MAX_SMARTGLOW_LEDS; master_led--)	
				{
					/* No need to check the current LED as, the status is already set to 0 */
					if(g_smartglow_data->led_state & (1 << master_led)) 
					{
						leds_set_clk(g_smartglow_data->clients[master_led], AN30259A_CLK_OUT_MODE);
						
						g_smartglow_data->master_clk_led = master_led;
						break;
					}
				}
			}
		}

		an30259a_reset(g_smartglow_data->clients[led_num]);
	}
	else 
	{// Switch ON the LED 

		// Constant Light
		if(!g_smartglow_data->led_state)  
		{
			//if first LED to be turned ON, configure clock mode enable (master clk)
			leds_set_clk(g_smartglow_data->clients[led_num], AN30259A_CLK_OUT_MODE);
			
			g_smartglow_data->master_clk_led = led_num;
		}
		else if(g_smartglow_data->master_clk_led != led_num) 
		{
			//If its not first LED configure as clk input mode
			leds_set_clk(g_smartglow_data->clients[led_num], AN30259A_CLK_IN_MODE);
		}
		else
		{/* If enabled led is already master do nothing */}
		
		/* Update the LED ON status */
		g_smartglow_data->led_state |= (1 << led_num);
		
		/* Get the fixed color map */
		g_smartglow_data->led[led_num].color = an30259a_fixed_color_map(
					g_smartglow_data->led[led_num].color & 0x00FFFFFF,
					DEFAULT_BRIGHTNESS);
					
		leds_set_imax(g_smartglow_data->clients[led_num], LED_DEFAULT_IMAX);
		
		/* Set LED color */
		an30259a_set_color(g_smartglow_data->clients[led_num], g_smartglow_data->led[led_num].color);
		
		if(!g_smartglow_data->led[led_num].offtime) 
		{
			// Constant Light
			an30259a_set_constant_current(g_smartglow_data->clients[led_num]);
		}
		else 
		{
			// Set LED slope current
			an30259a_set_slope_current(g_smartglow_data->clients[led_num], 
						g_smartglow_data->led[led_num].ontime, 
						g_smartglow_data->led[led_num].offtime);
			
			// LED blink with slope raise and fall
			
			if(g_smartglow_data->led[led_num].repeat_count)
			{
				// Calculate and start timer
				
			}
		}	
	}
		
	return 0;
}

/**
* an30259a_smartglow_animate_work - Animate smartglow by the config written 
*		from upper layer
**/
static void an30259a_smartglow_animate_work(struct work_struct *work)
{
	int led_num;
	printk(KERN_ERR" [SmartGlow] %s", __func__);
	
	if((g_smartglow_data->in_ledaccess & 0xF0) == 0)
	{
		printk(KERN_ERR" [SmartGlow] %s: %d", __func__, __LINE__);
		smartglow_reset();
		g_smartglow_data->led_state = 0;
		
		if(!g_smartglow_data->in_ledaccess)
		{
			memset(g_smartglow_data->led, 0, sizeof(g_smartglow_data->led));
			return;
		}
	}
	
	/* Ignore the Reset indicator */
	g_smartglow_data->in_ledaccess &= 0xF;
	
	if(g_smartglow_data->in_ledaccess <= MAX_SMARTGLOW_LEDS)
	{
		printk(KERN_ERR " [SmartGlow] %s: %d", __func__, __LINE__);
		g_smartglow_data->in_ledaccess--;
		
		an30259a_smartglow_config_led(g_smartglow_data->in_ledaccess);
		leds_i2c_write_all(g_smartglow_data->clients[g_smartglow_data->in_ledaccess]);
	}
	else
	{
		if(g_smartglow_data->in_ledaccess == 0xF)
		{
			printk(KERN_ERR" [SmartGlow] %s: %d", __func__, __LINE__);
			for(led_num = 0; led_num < MAX_SMARTGLOW_LEDS; led_num++)	
			{
				an30259a_smartglow_config_led(led_num);
			}
		}
		else if(g_smartglow_data->in_ledaccess == 0xA)
		{
			struct an30259a_data *master_data = i2c_get_clientdata(g_smartglow_data->clients[0]);
			
			printk(KERN_ERR" [SmartGlow] %s: %d", __func__, __LINE__);
			
			/* Set the illusion of a existing master. Later after preparing all the slave bufer make as a master */
			g_smartglow_data->led_state = 0b1000;
			g_smartglow_data->master_clk_led = 3;
		
			//Configure the First LED
			an30259a_smartglow_config_led(0);
			
			// Copy First LED configurations to other LEDs
			for(led_num = 1; led_num < MAX_SMARTGLOW_LEDS; led_num++)	
			{
				struct an30259a_data *data = i2c_get_clientdata(
						g_smartglow_data->clients[led_num]);
						
				printk(KERN_ERR" [SmartGlow] %s: %d", __func__, __LINE__);
				memcpy(data->shadow_reg, master_data->shadow_reg, AN30259A_REG_MAX);
			}
			/* Set Master Clk */
			leds_set_clk(g_smartglow_data->clients[0], AN30259A_CLK_OUT_MODE);
			
			/* Update the master */
			g_smartglow_data->master_clk_led = 0;
			
			/* Update LED status */
			g_smartglow_data->led_state = 0b1111;
		}
		
		smartglow_an30259a_i2c_write_all();
	}
	
	printk(KERN_ERR " [SmartGlow] %s: %d", __func__, __LINE__);
}

/**
* animate_store - get the animation configuration from user space.
**/
static ssize_t animate_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)			
{
	unsigned short led_no = 0xE;
	dev_info(dev, " [SmartGlow] %s: %s", __func__, buf);
	
	sscanf(buf, "%hX ", &led_no);
	g_smartglow_data->in_ledaccess = led_no;
	
	if(led_no == 0)
	{
		dev_info(dev, "[SmartGlow] %s: %d", __func__, __LINE__);
	}
	else if(led_no == 0xF)
	{
		dev_info(dev, " [SmartGlow] %s: %d", __func__, __LINE__);
		
		sscanf(buf+2, "%X %hu %hu %hu %X %hu %hu %hu %X %hu %hu %hu %X %hu %hu %hu",
			&g_smartglow_data->led[0].color, 
			&g_smartglow_data->led[0].ontime, 
			&g_smartglow_data->led[0].offtime, 
			&g_smartglow_data->led[0].repeat_count, 
			&g_smartglow_data->led[1].color, 
			&g_smartglow_data->led[1].ontime, 
			&g_smartglow_data->led[1].offtime, 
			&g_smartglow_data->led[1].repeat_count, 
			&g_smartglow_data->led[2].color, 
			&g_smartglow_data->led[2].ontime, 
			&g_smartglow_data->led[2].offtime, 
			&g_smartglow_data->led[2].repeat_count, 
			&g_smartglow_data->led[3].color, 
			&g_smartglow_data->led[3].ontime, 
			&g_smartglow_data->led[3].offtime, 
			&g_smartglow_data->led[3].repeat_count
		);		
	}
	else if(led_no == 0xA)
	{	
		dev_info(dev, " [SmartGlow] %s: %d", __func__, __LINE__);

		sscanf(buf+2, "%X %hu %hu %hu", 
			&g_smartglow_data->led[0].color, 
			&g_smartglow_data->led[0].ontime, 
			&g_smartglow_data->led[0].offtime, 
			&g_smartglow_data->led[0].repeat_count
		);
		
		if(g_smartglow_data->led[0].color)
		{			
			g_smartglow_data->led[3] = g_smartglow_data->led[2] =
				g_smartglow_data->led[1] = g_smartglow_data->led[0];
		}
		else
		{
			/* If ALL LED to be off, change to reset mode */
			g_smartglow_data->in_ledaccess = 0;			
		}
	}	
	else 
	{
		led_no &= 0xF;

		if(led_no <= MAX_SMARTGLOW_LEDS)
		{
			led_no--;
			dev_info(dev, " [SmartGlow] %s: %d", __func__, __LINE__);
			
			sscanf(buf+2, "%X %hu %hu %hu", 
				&g_smartglow_data->led[led_no].color, 
				&g_smartglow_data->led[led_no].ontime, 
				&g_smartglow_data->led[led_no].offtime, 
				&g_smartglow_data->led[led_no].repeat_count
			);
		}
		else
		{
			dev_err(dev, "[SmartGlow] Invalid input: %d", led_no);
			return len;
		}
	}
		
	schedule_work(&g_smartglow_data->animate_work);
	
	dev_info(dev, " [SmartGlow] %s: %d", __func__, __LINE__);
	return len;
}

static ssize_t blink_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	return animate_store(dev, attr, buf, len);
}

			
/**
* animate_show - returns smart glow animation config ON sysfs read.
**/
static ssize_t animate_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR" [SmartGlow] %s", __func__);
	
	return sprintf(buf, "0x%X %hu %hu %hu\n0x%X %hu %hu %hu\n0x%X %hu %hu %hu\n0x%X %hu %hu %hu\n", 
		g_smartglow_data->led[0].color, 
		g_smartglow_data->led[0].ontime, 
		g_smartglow_data->led[0].offtime, 
		g_smartglow_data->led[0].repeat_count, 
		g_smartglow_data->led[1].color, 
		g_smartglow_data->led[1].ontime, 
		g_smartglow_data->led[1].offtime, 
		g_smartglow_data->led[1].repeat_count, 
		g_smartglow_data->led[2].color, 
		g_smartglow_data->led[2].ontime, 
		g_smartglow_data->led[2].offtime, 
		g_smartglow_data->led[2].repeat_count, 
		g_smartglow_data->led[3].color, 
		g_smartglow_data->led[3].ontime, 
		g_smartglow_data->led[3].offtime, 
		g_smartglow_data->led[3].repeat_count);
}

static ssize_t blink_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return animate_show(dev, attr, buf);
}

/**
* Device attribute set for sysfs interface for Animations
**/
// TSN :: Need to check for CLASS_ATTR Vs DEVICE_ATTR, 
//	even though we are registering class
/* permission for sysfs node */
//DEVICE_ATTR_RW(animate);
static DEVICE_ATTR_RW(animate);
//static DEVICE_ATTR_RW(blink);
static DEVICE_ATTR_RW(led_intensity);
static struct device_attribute dev_attr_smartglow_blink  = {
	.attr = {.name = "blink", .mode = 0644 },
	.show = blink_show,
	.store = blink_store,
	};

static struct attribute *smartglow_class_attrs[] = {
	&dev_attr_animate.attr,
	&dev_attr_smartglow_blink.attr,
	&dev_attr_led_intensity.attr,
	NULL,
};

static struct attribute_group common_smartglow_attr_group = {
	.attrs = smartglow_class_attrs,
};


#ifdef CONFIG_OF
/**
* an30259a_parse_dt - Parse the device enumerations.
**/
static int an30259a_parse_dt(struct device *dev, struct an30259a_data *data) {
	struct device_node *np = dev->of_node;
	int ret, temp;
	enum an30259a_led_channel ch;
	
	char *s[] = {"led_r", "led_g", "led_b"};
	static int led_num = 0;
	
	led_num++;
	
	/* LED Number */
	ret = of_property_read_u32(np, "an30259a,led-num", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Error in getting an30259a,led-num.\n");
		
		data->led_num = led_num;
	}
	else{
		data->led_num = temp;
	}
	
	/* Imax */
	ret = of_property_read_u32(np, "an30259a,iMax", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Error in getting an30259a,iMax.\n");
		data->iMax = LED_DEFAULT_IMAX;
	}
	else{
		data->iMax = temp;
	}
	
	/* Clk synchronization mode */
	ret = of_property_read_u32(np, "an30259a,clk_mode", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Error in getting an30259a,clk_mode.\n");
		data->clk_mode = LED_DEFAULT_CLK_MODE;
	}
	else{
		data->clk_mode = temp;
	}
	
	/* LED Name */
	ret = of_property_read_string_index(np, "an30259a,led-name", ch,
					(const char **)&data->name);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Error in getting LED Name.\n");
		
		/* If could not get the led name, assign a running name */
		data->led_conf[ch].name = devm_kzalloc(dev, sizeof(char)*8, GFP_KERNEL);
	
		sprintf(data->led_conf[ch].name, "led_%d",data->led_num);		
	}
	
	for (ch = LED_R; ch < MAX_LED_CHANNEL; ch++)	{
		
		/* LED Name */
		ret = of_property_read_string_index(np, "an30259a,rgb-name", ch,
						(const char **)&data->led_conf[ch].name);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "Error in getting LED Name.\n");
			
			/* If could not get the led name, assign a running name */
			data->led_conf[ch].name = devm_kzalloc(dev, sizeof(char)*8, GFP_KERNEL);
#if defined(SEC_LED_SPECIFIC)
			sprintf(data->led_conf[ch].name, "%s", s[ch]);
#else	
			sprintf(data->led_conf[ch].name, "%s_%d", s[ch], data->led_num);
#endif			
		}

		/* Default Brightness value of LED class */
		ret = of_property_read_u32_index(np, "an30259a,brightness", ch, &temp);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "Error in getting an30259a,brightness.\n");
			data->led_conf[ch].brightness = LED_OFF;
		}
		else{
			data->led_conf[ch].brightness = temp;
		}

		/* Maximum Brightness Scale for LED Class */
		ret = of_property_read_u32_index(np, "an30259a,maxbrightness", ch, &temp);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "Error in getting an30259a,maxbrightness.\n");
			data->led_conf[ch].max_brightness = LED_FULL;
		}
		else{
			data->led_conf[ch].max_brightness = temp;
		}
		
		/* default_current of IC */
		ret = of_property_read_u32_index(np, "an30259a,default_current", ch, &temp);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "Error in getting an30259a,default_current.\n");
			data->led_conf[ch].default_current = LED_DEFAULT_CURRENT;
		}
		else{
			data->led_conf[ch].default_current = temp;
		}
		
		/* Max current in low power mode of IC*/
		ret = of_property_read_u32_index(np, "an30259a,lowpower_current", ch, &temp);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "Error in getting an30259a,lowpower_current.\n");
			data->led_conf[ch].lowpower_current = LED_LOWPOWER_CURRENT;
		}
		else{
			data->led_conf[ch].lowpower_current = temp;
		}

		/* offset_current of IC*/
		ret = of_property_read_u32_index(np, "an30259a,offset_current", ch, &temp);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "Error in getting an30259a,offset_current.\n");
			data->led_conf[ch].offset_current = LED_OFFSET_CURRENT;
		}
		else{
			data->led_conf[ch].offset_current = temp;			
		}		
		
		/* LED Class flag */
		ret = of_property_read_u32_index(np, "an30259a,flags", ch, &temp);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "Error in getting an30259a,flags.\n");
			data->led_conf[ch].flags = 0;
		}
		else{
			data->led_conf[ch].flags = temp;			
		}
	}

	return 0;
}
#endif

/**
* an30259a_initialize - Initializes an30259a.
**/
static int an30259a_initialize(struct i2c_client *client)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	int ret = 0;

	/* Reset an30259a*/
	if( unlikely((ret = an30259a_reset(client)) < 0)) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c write (reg = 0x%2x)\n",
			__func__, AN30259A_REG_SRESET);
		return ret;
	}
	
	/* Make a copy of IC register values to the driver */
	if(unlikely((ret = i2c_smbus_read_i2c_block_data(client,
			AN30259A_REG_SRESET | AN30259A_CTN_RW_FLG,
			AN30259A_REG_MAX, data->shadow_reg)) < 0)) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c read block(ledxcc)\n",
			__func__);
		return ret;
	}
	
	/* Dont keep the device is stand by mode to save power */
#if 0	
	/* Set default imax */
	leds_set_imax(client, data->iMax);
	
	/* set clk mode */
	leds_set_clk(client, data->clk_mode);
#endif	

	return 0;
}


/**
* an30259a_register_rgbclass - Initializes each LED channels / colors.
**/
static int an30259a_register_rgbclass(struct i2c_client *client,
				struct an30259a_led *led, enum an30259a_led_channel channel)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	struct device *dev = &client->dev;
	int ret = 0;
	
	led->channel = channel;
	led->cdev.brightness_set = an30259a_set_brightness;
	led->cdev.name = data->led_conf[channel].name;
	led->cdev.brightness = data->led_conf[channel].brightness;
	//led->cdev.max_brightness = data->led_conf[channel].max_brightness;
	led->cdev.max_brightness = LED_FULL;
	led->cdev.flags = data->led_conf[channel].flags;
	
	// TSN :: For call back Ref
	// Temp add, as dereferencing from container is not working.
	led->client = client;

	/* Register LED Class device interface */
	if(unlikely((ret = led_classdev_register(dev, &led->cdev)) < 0)) {
		dev_err(dev, "can not register led channel : %d\n", channel);
		return ret;
	}

	/* Create additional class attribute to expose blink 
		related functionalities */
	if(unlikely((ret = sysfs_create_group(&led->cdev.dev->kobj,
			&common_led_attr_group)) < 0 )) {
		dev_err(dev, "can not register sysfs attribute\n");
		goto ERR_SYSFS;
	}
	
	/*WorkQ to schedule request, came from user space */
	INIT_WORK(&(data->led_channel[channel].brightness_work),
				 an30259a_led_brightness_work);
	INIT_WORK(&(data->led_channel[channel].blink_work),
				 an30259a_led_blink_work);

	return 0;
	
ERR_SYSFS:
	led_classdev_unregister(&led->cdev);
	
	return ret;
}

/**
* smartglow_register_ledclass - Registers LED class for smart glow.
**/
static int smartglow_register_ledclass()
{
	struct device *dev = &g_smartglow_data->pdata->dev;
	int ret = 0;
	
	//g_smartglow_data->class_dev.channel = MAX_LED_CHANNEL;  
	g_smartglow_data->class_dev.brightness_set = smartglow_set_brightness;
	g_smartglow_data->class_dev.name = "smartglow"; //g_smartglow_data->name;
	g_smartglow_data->class_dev.brightness = 0;
	g_smartglow_data->class_dev.max_brightness = 0xFFFFFFFF;
	g_smartglow_data->class_dev.flags = 0;	
	
	g_smartglow_data->intensity = DEFAULT_BRIGHTNESS;

	/* Register LED Class device interface */
	if(unlikely((ret = led_classdev_register(dev, &g_smartglow_data->class_dev)) < 0)) {
		dev_err(dev, "can not register led : smartglow\n");
		return ret;
	}

	/* Create additional class attribute to expose blink 
		related functionalities */
	if(unlikely((ret = sysfs_create_group(&g_smartglow_data->class_dev.dev->kobj,
			&common_smartglow_attr_group)) < 0 )) {
		dev_err(dev, "can not register sysfs attribute\n");
		goto ERR_SYSFS;
	}
	
	/*WorkQ to schedule request, came from user space */
	INIT_WORK(&(g_smartglow_data->animate_work), an30259a_smartglow_animate_work);

	return 0;
	
ERR_SYSFS:
	led_classdev_unregister(&g_smartglow_data->class_dev);
	
	return ret;
}

/**
* an30259a_register_ledclass - Initializes each LED channels / colors.
**/
static int an30259a_register_ledclass(struct i2c_client *client,
				struct an30259a_led *led)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	struct device *dev = &client->dev;
	int ret = 0;
	
	// led->channel = data->led_num;
	led->channel = MAX_LED_CHANNEL;    // Temp add for LED class
	led->cdev.brightness_set = an30259a_set_brightness;
	led->cdev.name = data->name;
	//led->cdev.brightness = data->brightness;
	led->cdev.brightness = 0;
	//led->cdev.max_brightness = data->led_conf[channel].max_brightness;
	led->cdev.max_brightness = 0xFFFFFFFF;
	//led->cdev.flags = data->flags;
	led->cdev.flags = 0;
	
	// TSN :: For call back Ref
	// Temp add, as dereferencing from container is not working.
	led->client = client;
	
	led->intensity = DEFAULT_BRIGHTNESS;

	/* Register LED Class device interface */
	if(unlikely((ret = led_classdev_register(dev, &led->cdev)) < 0)) {
		dev_err(dev, "can not register led : %d\n", data->led_num);
		return ret;
	}

	/* Create additional class attribute to expose blink 
		related functionalities */
	if(unlikely((ret = sysfs_create_group(&led->cdev.dev->kobj,
			&common_led_attr_group)) < 0 )) {
		dev_err(dev, "can not register sysfs attribute\n");
		goto ERR_SYSFS;
	}
	
	/*WorkQ to schedule request, came from user space */
	INIT_WORK(&(data->led.brightness_work),
				 an30259a_led_brightness_work);
	INIT_WORK(&(data->led.blink_work),
				 an30259a_led_blink_work);

	return 0;
	
ERR_SYSFS:
	led_classdev_unregister(&led->cdev);
	
	return ret;
}

/**
* an30259a_deinitialize - Deinitializes each LED channels / colors.
**/
void an30259a_deinitialize(struct an30259a_data *data, enum an30259a_led_channel channel)
{
	/* Reset the device on failure */
	an30259a_reset(data->client);
	
	sysfs_remove_group(&data->led_channel[channel].cdev.dev->kobj,
						&common_led_attr_group);
	led_classdev_unregister(&data->led_channel[channel].cdev);
	cancel_work_sync(&data->led_channel[channel].brightness_work);	
	cancel_work_sync(&data->led_channel[channel].blink_work);	
}

/**
* an30259a_probe -Allocate the resource and initialize on matched  
* 	an30259a LEDdevice found.
**/
static int an30259a_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct an30259a_data *data;
	int ret;
	int ch;
	
	//static int probecnt = 0;

	dev_info(&client->adapter->dev, "%s\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "need I2C_FUNC_I2C.\n");
		return -ENODEV;
	}
 
	if(!(data = kzalloc(sizeof(*data), GFP_KERNEL))) {
		dev_err(&client->adapter->dev,
			"failed to allocate driver data.\n");
		return -ENOMEM;
	}
	
#ifdef CONFIG_OF
	if(unlikely((ret = an30259a_parse_dt(&client->dev, data)))) {
		dev_err(&client->adapter->dev, 
			"[%s] an30259a parse dt failed\n", __func__);
		goto exit_free;
	}
#else
#error "No Pdata from board file is implemented!"
#endif

	i2c_set_clientdata(client, data);
	data->client = client;
	
	/* Keep The Master Data Reference for clock */
	if(data->clk_mode == AN30259A_CLK_OUT_MODE)
	{
		dev_info(&client->adapter->dev, "Master Device Registered!");
		g_an30259a_data = data;
		
		//Register a Master Mood light device
		
	}
	
	mutex_init(&data->mutex);
	
	if(unlikely((ret = an30259a_initialize(client)) < 0)) {
		dev_err(&client->adapter->dev, "an30259a Init failed\n");
		goto exit_free;
	}
	
	if(unlikely((ret = an30259a_register_ledclass(client, 
					&data->led)) < 0)) {
		dev_err(&client->adapter->dev, "an30259a rgb class register failed\n");
		goto exit_init;
	}
	
	/* initialize LED */
	for (ch = LED_R; ch < MAX_LED_CHANNEL; ch++) {
		if(unlikely((ret = an30259a_register_rgbclass(client, 
					 &data->led_channel[ch], ch)) < 0)) {
			dev_err(&client->adapter->dev, "an30259a rgb class register failed\n");
			goto exit_init;
		}
	}
	
	// TSN::::::::::::::::::::  NEED TO CHANGE------------->
	/* If it is a Master device create single access Nodes */

#if defined (CONFIG_SEC_FACTORY)
	//TSN: Need to add on demand from HW team
	//GlowOnJigMode();
#endif

#ifdef SEC_LED_SPECIFIC
	// TSN:: Note, parameter needs to be defined
	sec_led_sysfs_create();
#endif

	g_smartglow_data->clients[g_smartglow_data->probed_led_cnt] = client;
	g_smartglow_data->probed_led_cnt++;
	gclient[probecnt] = client;
	probecnt++;
	
	return ret;

#ifdef SEC_LED_SPECIFIC
exit1:
   //device_destroy(led_dev, 0);
   sec_led_sysfs_destroy();
#endif

exit_init:
	// TSN :: Check Really need to de initialize????, As it can function with handicapped channels.
	while(--ch >= LED_R) {
		an30259a_deinitialize(data, ch);
	}
	
	mutex_destroy(&data->mutex);
	
exit_free:
	kfree(data);
	return ret;
}

/**
* an30259a_remove - cleans up the resource allocated for 
*		an30259a LED on probe
**/
static int /* __devexit */ an30259a_remove(struct i2c_client *client)
{
	struct an30259a_data *data = i2c_get_clientdata(client);
	enum an30259a_led_channel ch;
	
	dev_dbg(&client->adapter->dev, "%s\n", __func__);
#ifdef SEC_LED_SPECIFIC
	// TSN:: Note, parameter needs to be defined
	sec_led_sysfs_destroy();
#endif

	for (ch = LED_R; ch < MAX_LED_CHANNEL; ch++) {
		an30259a_deinitialize(data, ch);
	}
	mutex_destroy(&data->mutex);
	kfree(data);
	return 0;
}

#if defined(CONFIG_PM)
/**
* Device suspend event handler
*/
int an30259a_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct an30259a_data *data = i2c_get_clientdata(client);
	
	dev_dbg(&client->dev, "%s [START]\n", __func__);
	
	data->led.IsSleep = \
		data->led_channel[LED_R].IsSleep = \
		data->led_channel[LED_G].IsSleep = \
		data->led_channel[LED_B].IsSleep = 1;
	
	an30259a_reset_register_work(client);	
	an30259a_reset(client);
	
	dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return 0;

}

/**
* Device resume event handler
*/
int an30259a_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct an30259a_data *data = i2c_get_clientdata(client);
	int ret = 0;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	/* Set default imax */
	leds_set_imax(client, data->iMax);
	
	/* set clk mode */
	leds_set_clk(client, data->clk_mode);
	
	/* Reset Resgister work */
	an30259a_reset_register_work(client);
	
	data->led.IsSleep = \
		data->led_channel[LED_R].IsSleep = \
		data->led_channel[LED_G].IsSleep = \
		data->led_channel[LED_B].IsSleep = 0;

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return ret;
}

/**
* PM info
*/
const struct dev_pm_ops an30259a_pm_ops = {
	.suspend = an30259a_suspend,
	.resume = an30259a_resume,	
};
#endif //CONFIG_PM

/**
* list of an30259a, i2c client ids
**/
static struct i2c_device_id an30259a_id[] = {
	{"an30259a", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, an30259a_id);

/**
* i2c driver structure for an30259a for matching devices
**/
static struct of_device_id an30259a_match_table[] = {
	{ .compatible = "an30259a,smartglow",},
	{ },
};

/**
* i2c driver structure for an30259a
**/
static struct i2c_driver an30259a_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "an30259a",
		.of_match_table = an30259a_match_table,
	},
	.id_table = an30259a_id,
	.probe = an30259a_probe,
	//.remove = __devexit_p(an30259a_remove),
	.remove = an30259a_remove,
#if defined(CONFIG_PM)
	//	.pm = &an30259a_pm_ops,
#endif	//CONFIG_PM
};

/**
* Init and Register i2c client driver for an30259a
**/
//module_i2c_driver(an30259a_i2c_driver);


static int smartglow_probe(struct platform_device *pdev)
{
	int ret;
	
	dev_info(&pdev->dev, "%s\n", __func__);
	
#if 0 //def CONFIG_OF  // No platform data required yet
	if(unlikely((ret = smartglow_parse_dt(&pdev, g_smartglow_data)))) {
		dev_err(&pdev->dev, 
			"[%s] smartglow parse dt failed\n", __func__);
		goto exit_free;
	}
#else
//#error "No Pdata from board file is implemented!"
#endif

	platform_set_drvdata(pdev, g_smartglow_data);
	g_smartglow_data->pdata = pdev;
	
	mutex_init(&g_smartglow_data->mutex);
	
	if(unlikely((ret = smartglow_register_ledclass()) < 0)) {
		dev_err(&pdev->dev, "[SmartGlow] class register failed\n");
		goto exit_init;
	}
	
	return ret;
exit_init:
	mutex_destroy(&g_smartglow_data->mutex);
	
exit_free:
	kfree(g_smartglow_data);
	return ret;
	return 0;
}

#ifdef CONFIG_PM
static int smartglow_suspend(struct platform_device *dev, pm_message_t state)
{
	struct an30259a_smartglow_data *pdata = platform_get_drvdata(dev);
	
	printk("[SmartGlow] %s", __func__);
	return 0;
}

static int smartglow_resume(struct platform_device *dev)
{
	printk("[SmartGlow] %s", __func__);
	return 0;
}
#else
#define smartglow_suspend	NULL
#define smartglow_resume	NULL
#endif

static int smartglow_remove(struct platform_device *pdev)
{
	printk("[SmartGlow] %s", __func__);
	return 0;
}

static struct of_device_id smartglow_match_table[] = {
	{ .compatible = "smartglow", },
	{ },
};

struct platform_driver smartglow_driver = {
	.probe = smartglow_probe,
	.remove = smartglow_remove,
	.suspend = smartglow_suspend,
	.resume = smartglow_resume,
	.driver = {
	   .name = "smartglow",
	   .owner = THIS_MODULE,
	   .of_match_table = smartglow_match_table,
	   },
};

/**
* Init driver
*/
static int __init an30259a_init(void)
{
	int ret;
	
	printk("[SmartGlow] %s", __func__);
	
	if(!(g_smartglow_data = kzalloc(sizeof(*g_smartglow_data), GFP_KERNEL))) {
		printk("[SmartGlow] failed to allocate driver data.\n");
		return -ENOMEM;
	}
	
	ret = platform_driver_register(&smartglow_driver);
	if(ret)
	{
		printk("[SmartGlow] %s: Platform Registration filed", __func__);
	}
	
	return i2c_add_driver(&an30259a_i2c_driver);
}

/**
* Exit driver
*/
static void __exit an30259a_exit(void)
{
	i2c_del_driver(&an30259a_i2c_driver);
	platform_driver_unregister(&smartglow_driver);
}

module_init(an30259a_init);
module_exit(an30259a_exit);


MODULE_DESCRIPTION("AN30259A LED driver for Squiricle Mood Ring Flash");
MODULE_AUTHOR("Sankara Narayanan T <sankaranarayanan.t@samsung.com>");
MODULE_LICENSE("GPL v2");