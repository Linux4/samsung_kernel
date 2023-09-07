/*
 * leds_et6312b.c - driver for Panasonic et6312b led control chip
 *
 * Copyright (C) 2017, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

 
#include "leds-et6312b.h"


#if defined(CONFIG_SEC_FACTORY)
#include <linux/power_supply.h>
#include <linux/battery/sec_charging_common.h>
#endif /* CONFIG_SEC_FACTORY */


enum et6312b_led_channel{
	LED_R,
	LED_G,
	LED_B,
	LED_W
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


/**
* struct et6312b_data - holds all smart glow related data
* @client - i2c handle
* @led_classdev - class device handle for interface between \
*		user space and et6312b driver.
* @mutex - to avoid con current i2c transfer by multiple requests.
*
* @animate_work - work handle to deferred execution of user request \
*		to avoid app lag.
*
* @in_ledaccess & @smartglow_input - holds current user request data
* @intensity - holds current smartglow intensity of light
* @IsResetMode - To know whether et6312b is already in reset mode
* @rgb_state - holds all smart glow LED ON/OFF status.
* @slope_state - says, whether slope current mode or not
* 
* @shadow_reg - et6312b shadow register data
* @pseudo_reg - likes an pseudo IC, holds desired software value configured
* @iMax - et6312b max current setting for each LED
* @clk_mode - holds et6312b clk synchronisation mode
* @led_conf - holds each LED tuning data
**/
struct et6312b_data {
	/* et6312b enumeration data */
	struct i2c_client  *client;
	struct led_classdev	class_dev;
	struct mutex 		mutex;
	
	struct work_struct animate_work;
	
#if defined(CONFIG_SEC_FACTORY)	
	struct delayed_work boot_work;
#endif 	/* CONFIG_SEC_FACTORY */
	
	/* et6312b pinctrl settings */
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_state[2];
	unsigned int s_led_en;
	
	struct smartglow_input led[MAX_SMARTGLOW_LEDS];
	u8 in_ledaccess;
	//u8 intensity;   //To Do : Check 
	u8 IsResetMode;
	u8 rgb_state;
	u8 slope_state;
	
	/* smartglow tuning data*/	
#ifdef ET6312B_DEBUG_TUNING
	u32 rgb_current;
	u32 rgb_color;
	int a;
	int r;
	int g;
	int b;
#endif /* ET6312B_DEBUG_TUNING */
	
	/* et6312b registers & settings */
	//u8 shadow_reg[ET6312B_REG_MAX];
	u8 pseudo_reg[ET6312B_REG_MAX];
	u8 iMax;
	u8 clk_mode;	
}*g_et6312b_data;


/* smartglow tuning data*/
u8 min_current[4];
u8 max_current[4];
u8 lower_supported_color[4];
u8 higher_supported_color[4];
u8 et6312b_color_map[10][3][2];
u8 et6312b_current_map[10][3][2];


/**
* et6312b_i2c_write_block - writes et6312b register value through I2C
* @command - et6312b register offset (Starting register address)
* @length - total number of registers to be written.
* @values - buffer holds, register configurations to be written
**/
s32 et6312b_i2c_write_block(const struct i2c_client *client,
        u8 command, u8 length, u8 *values)
{	

	u8 buf[ET6312B_REG_MAX + 3];
	struct i2c_msg msg = {
			.addr = client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf
			};
	
	buf[0] = command;
	memcpy(&buf[1], values, length);
	
	return i2c_transfer(client->adapter, &msg, 1);
}



/*-----------------------------------------------------------------------------
 *    S M A R T  G L O W E     I N T E R F A C E S   &   F U N C T I O N S 
 *---------------------------------------------------------------------------*/
 
/**
* et6312b_blink_store - get the animation configuration from user space.
* 
* user space app/framework and driver shall follow the below command format. 
* The command can be up to 17 parameters.
* 	"<LedToAccess> 
*	 <LED1COLOR> <LED1ONTIME> <LED1OFFTIME> <LED1REPEATCOUNT> 
*	 <LED2COLOR> <LED2ONTIME> <LED2OFFTIME> <LED2REPEATCOUNT> 
*	 <LED3COLOR> <LED3ONTIME> <LED3OFFTIME> <LED3REPEATCOUNT> 
*	 <LED4COLOR> <LED4ONTIME> <LED4OFFTIME> <LED4REPEATCOUNT>"
* <LedToAccess>:
*	-> Only in hex format & Max 2 digit
*	-> If 0, Reset all LED.
*	-> if MSB 4 bit is 0, Before setting the data reset all LED, 
*		else just over write the following LED data (dont not reset all)
*	-> If LSB 4 bit is 1~4 -> Access the particular LED
*	-> if LSB 4 bit is 0xA -> Write all LED with same data
*	-> if LSB 4 bit is 0xF -> Get all LED data from user and write them all
* <LEDxCOLOR>: 
*	LED COLOR in RRGGBB (Only in Hex format), if 0 - switch off the LED
* <LEDxONTIME>: 
*	for blinking on time in ms. (if 0, switch off the LED)
* <LEDxOFFTIME>:
*	For blinking, off time in ms. (if 0, LED will glow constantly)
* <LEDxREPEATCOUNT>:
*	For blinking, specifies number of blinks (if 0, it blinks forever)
*
**/
static ssize_t et6312b_blink_store(struct /* class */ device *dev,
			struct device_attribute *attr, const char *buf, size_t len)			
{
	unsigned short led_no = 0xE;
	dev_info(dev, " [SmartGlow] %s: %s", __func__, buf);
	
	sscanf(buf, "%hX ", &led_no);
	
	mutex_lock(&g_et6312b_data->mutex);
	
	g_et6312b_data->in_ledaccess = led_no;
	
	if(led_no)
	{
		if(led_no == 0xF)
		{
			//dev_info(dev, " [SmartGlow] %s: %d", __func__, __LINE__);
			
			sscanf(buf+2, "%X %hu %hu %hu %X %hu %hu %hu %X %hu %hu %hu %X %hu %hu %hu",
				&g_et6312b_data->led[0].color, 
				&g_et6312b_data->led[0].ontime, 
				&g_et6312b_data->led[0].offtime, 
				&g_et6312b_data->led[0].repeat_count, 
				&g_et6312b_data->led[1].color, 
				&g_et6312b_data->led[1].ontime, 
				&g_et6312b_data->led[1].offtime, 
				&g_et6312b_data->led[1].repeat_count, 
				&g_et6312b_data->led[2].color, 
				&g_et6312b_data->led[2].ontime, 
				&g_et6312b_data->led[2].offtime, 
				&g_et6312b_data->led[2].repeat_count, 
				&g_et6312b_data->led[3].color, 
				&g_et6312b_data->led[3].ontime, 
				&g_et6312b_data->led[3].offtime, 
				&g_et6312b_data->led[3].repeat_count
			);		
		}
		else if(led_no == 0xA)
		{	
			//dev_info(dev, " [SmartGlow] %s: %d", __func__, __LINE__);

			sscanf(buf+2, "%X %hu %hu %hu", 
				&g_et6312b_data->led[0].color, 
				&g_et6312b_data->led[0].ontime, 
				&g_et6312b_data->led[0].offtime, 
				&g_et6312b_data->led[0].repeat_count
			);
			
			if(g_et6312b_data->led[0].color)
			{			
				g_et6312b_data->led[3] = g_et6312b_data->led[2] =
					g_et6312b_data->led[1] = g_et6312b_data->led[0];
			}
			else
			{
				/* If ALL LED to be off, change to reset mode */
				g_et6312b_data->in_ledaccess = 0;			
			}
		}	
		else 
		{
			led_no &= 0xF;

			if(led_no <= MAX_SMARTGLOW_LEDS)
			{
				led_no--;
				//dev_info(dev, " [SmartGlow] %s: %d\n", __func__, __LINE__);
				
				sscanf(buf+2, "%X %hu %hu %hu", 
					&g_et6312b_data->led[led_no].color, 
					&g_et6312b_data->led[led_no].ontime, 
					&g_et6312b_data->led[led_no].offtime, 
					&g_et6312b_data->led[led_no].repeat_count
				);
			}
			else
			{
				dev_err(dev, "[SmartGlow] Invalid input: %d\n", led_no);
				mutex_unlock(&g_et6312b_data->mutex);
				return len;
			}
		}
	}
	mutex_unlock(&g_et6312b_data->mutex);
		
	schedule_work(&g_et6312b_data->animate_work);
	
	//dev_info(dev, " [SmartGlow] %s: %d", __func__, __LINE__);
	return len;
}

/**
* et6312b_blink_show - returns smart glow animation config on sysfs read.
**/
static ssize_t et6312b_blink_show(struct /* class */ device  *dev,
			struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR" [SmartGlow] %s", __func__);
	
	return sprintf(buf, "0x%X %hu %hu %hu\n0x%X %hu %hu %hu\n0x%X %hu %hu %hu\n0x%X %hu %hu %hu\n", 
		g_et6312b_data->led[0].color, 
		g_et6312b_data->led[0].ontime, 
		g_et6312b_data->led[0].offtime, 
		g_et6312b_data->led[0].repeat_count, 
		g_et6312b_data->led[1].color, 
		g_et6312b_data->led[1].ontime, 
		g_et6312b_data->led[1].offtime, 
		g_et6312b_data->led[1].repeat_count, 
		g_et6312b_data->led[2].color, 
		g_et6312b_data->led[2].ontime, 
		g_et6312b_data->led[2].offtime, 
		g_et6312b_data->led[2].repeat_count, 
		g_et6312b_data->led[3].color, 
		g_et6312b_data->led[3].ontime, 
		g_et6312b_data->led[3].offtime, 
		g_et6312b_data->led[3].repeat_count);
}


#ifdef ET6312B_DEBUG 
/**
* et6312b_reg_store - get the animation configuration from user space.
**/
static ssize_t et6312b_reg_store(struct /* class */ device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)			
{
	u8 pseudo_reg[ET6312B_REG_MAX+1]; 
	unsigned short u16pseudo_reg[ET6312B_REG_MAX+1]; 
	dev_info(dev, " [SmartGlow] %s: %s: None implemented yet!", __func__, buf);
	
	
	sscanf(buf, "%hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu \
	%hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu \
	%hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu",
	&u16pseudo_reg[0],
	&u16pseudo_reg[1],
	&u16pseudo_reg[2],
	&u16pseudo_reg[3],
	&u16pseudo_reg[4],
	&u16pseudo_reg[5],
	&u16pseudo_reg[6],
	&u16pseudo_reg[7],
	&u16pseudo_reg[8],
	&u16pseudo_reg[9],
	&u16pseudo_reg[10],
	&u16pseudo_reg[11],
	&u16pseudo_reg[12],
	&u16pseudo_reg[13],
	&u16pseudo_reg[14],
	&u16pseudo_reg[15],
	&u16pseudo_reg[16],
	&u16pseudo_reg[17],
	&u16pseudo_reg[18],
	&u16pseudo_reg[19],
	&u16pseudo_reg[20],
	&u16pseudo_reg[21],
	&u16pseudo_reg[22],
	&u16pseudo_reg[23],
	&u16pseudo_reg[24],
	&u16pseudo_reg[25],
	&u16pseudo_reg[26],
	&u16pseudo_reg[27],
	&u16pseudo_reg[28],
	&u16pseudo_reg[29],
	&u16pseudo_reg[30],
	&u16pseudo_reg[31],
	&u16pseudo_reg[32],
	&u16pseudo_reg[33],
	&u16pseudo_reg[34],
	&u16pseudo_reg[35],
	&u16pseudo_reg[36],
	&u16pseudo_reg[37],
	&u16pseudo_reg[38]
	);
	
	{
		int i;
		for(i=0; i<=ET6312B_REG_MAX; i++)
		{
			pseudo_reg[i] = (u8) u16pseudo_reg[i];
		}
	}
	
	if(unlikely((et6312b_i2c_write_block(g_et6312b_data->client, \
				ET6312B_REG_FLASH_PERIOD_RGB_BASE, ET6312B_REG_SLOPE_CURR_MAX, \
				&pseudo_reg[ET6312B_REG_FLASH_PERIOD_RGB_BASE])) < 0)) {
			dev_err(&g_et6312b_data->client->dev, \
				"%s: failure on i2c\n", __func__);
		}
	
	if(unlikely((i2c_smbus_write_byte_data(g_et6312b_data->client, 
			ET6312B_REG_CHIPCTR, pseudo_reg[ET6312B_REG_CHIPCTR])) < 0)) {
		dev_err(&g_et6312b_data->client->dev, \
			"%s: failure on i2c \n", __func__);
	}
		

	return len;
}

/**
* et6312b_reg_show - returns smart glow animation config on sysfs read.
**/
static ssize_t et6312b_reg_show(struct /* class */ device  *dev,
			struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR" [SmartGlow] %s : No implementation yet!", __func__);
	
	return sprintf(buf, "NA\n");
}

#endif /* ET6312B_DEBUG */


/*-----------------------------------------------------------------------------
 *    E T 6 3 1 2 B     D E V I C E     F U N C T I O N S 
 *---------------------------------------------------------------------------*/

/**
* s_led_en_ctrl - set pinctrl settings.
**/
static int s_led_en_ctrl(bool on)
{
	if (on && !g_et6312b_data->s_led_en) {
		if(unlikely(pinctrl_select_state(g_et6312b_data->pinctrl, \
				g_et6312b_data->pinctrl_state[ET6312B_ON])))
			dev_err(&g_et6312b_data->client->dev, "%s: Failed to configure s_led_en pinctrl.\n", __func__);
		else {
			g_et6312b_data->s_led_en = true;
			return 0;
		}
	} else if (!on && g_et6312b_data->s_led_en) {
		if(unlikely(pinctrl_select_state(g_et6312b_data->pinctrl, \
				g_et6312b_data->pinctrl_state[ET6312B_OFF])))
			dev_err(&g_et6312b_data->client->dev, "%s: Failed to configure default pinctrl.\n", __func__);
		else {
			g_et6312b_data->s_led_en = false;
			return 0;
		}
	} else
		return 1;
	
	return -1;
}

 
 
/**
* et6312b_reset - Reset all led to Off state.
**/
static int __inline et6312b_reset(struct i2c_client *client)
{	
	int ret;
	
	/* Reset the IC */
	if(unlikely((ret = i2c_smbus_write_byte_data(client, 
			ET6312B_REG_CHIPCTR, ET6312B_RESET)) < 0)) {
		dev_err(&client->dev,
			"%s: failure on i2c reset\n",
			__func__);
	}
	
	if (unlikely(s_led_en_ctrl(false) < 0)) {
		dev_err(&client->dev, "%s: failure disable - pinctrl\n",
			__func__);
		//return -1;
	}
		
	/* as the default register configurations are not desired, 
	have pseudo config required for future settings */
	memset(&g_et6312b_data->pseudo_reg[1], 0, ET6312B_REG_MAX - 2);

	usleep_range(ET6312B_RESET_DELAY, ET6312B_RESET_DELAY);
	
	return ret;
} 

/**
* et6312b_i2c_chip_enable - Enables all LED output
**/
static int 
#ifndef ET6312B_DEBUG
__inline 
#endif
et6312b_i2c_chip_enable(void)
{
	int ret;
	
	printk(KERN_ERR "[smartglow] %s\n", __func__);
	
	if(unlikely((ret = i2c_smbus_write_byte_data(g_et6312b_data->client, 
			ET6312B_REG_CHIPCTR, g_et6312b_data->pseudo_reg[ET6312B_REG_CHIPCTR])) < 0)) {
		dev_err(&g_et6312b_data->client->dev, \
			"%s: failure on i2c \n", __func__);
	}
	
#ifdef ET6312B_DEBUG_REG	
	{
		int i;
		u8 pseudo_reg[ET6312B_REG_MAX + 10];
		
		for(i=0; i<ET6312B_REG_MAX; i++)
		{
			/*
			printk(KERN_ERR"[smartglow][et6312b] REG: %x - %d, 0x%X  :: %d, 0x%X", i, 
					g_et6312b_data->pseudo_reg[i],
					g_et6312b_data->pseudo_reg[i], pseudo_reg[i], pseudo_reg[i]
					);
					*/
					
			if(unlikely((ret = i2c_smbus_read_i2c_block_data(g_et6312b_data->client, \
				i, \
				1,
				&pseudo_reg[i])) < 0)) {
				dev_err(&g_et6312b_data->client->dev, \
					"%s: failure on i2c\n", __func__);
			}
					
			printk(KERN_ERR"[smartglow][et6312b] REG: %x - %d, 0x%X  :: %d, 0x%X", i, 
					g_et6312b_data->pseudo_reg[i],
					g_et6312b_data->pseudo_reg[i], pseudo_reg[i], pseudo_reg[i]
					);
		}
	}
#endif /* ET6312B_DEBUG_REG */
	
	return ret;
}


/**
* et6312b_i2c_write_all - Writes all et6312b register values through I2C
**/
static int __inline et6312b_i2c_write_all(void)
{
	int ret;
	
	printk(KERN_ERR "[smartglow] %s\n", __func__);
	
	if (unlikely(s_led_en_ctrl(true) < 0))
		dev_err(&g_et6312b_data->client->dev, "%s: et6312b led enable ctrl failed\n", __func__);
	
	if(g_et6312b_data->slope_state) {
		if(unlikely((ret = et6312b_i2c_write_block(g_et6312b_data->client, \
				ET6312B_REG_FLASH_PERIOD_RGB_BASE, ET6312B_REG_SLOPE_CURR_MAX, \
				&g_et6312b_data->pseudo_reg[ET6312B_REG_FLASH_PERIOD_RGB_BASE])) < 0)) {
			dev_err(&g_et6312b_data->client->dev, \
				"%s: failure on i2c\n", __func__);
		}
	}
	else{
		if(unlikely((ret = et6312b_i2c_write_block(g_et6312b_data->client, \
				ET6312B_REG_LEDXMD_RGBx_BASE, ET6312B_REG_CONST_CURR_MAX, \
				&g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGBx_BASE])) < 0)) {
			dev_err(&g_et6312b_data->client->dev, \
				"%s: failure on i2c\n", __func__);
		}
	}
	
	return ret;
}

/**
* smartglow_reset - Reset all et6312b LEDs.
**/
static void __inline smartglow_reset(void)
{
	printk(KERN_ERR "[smartglow] %s\n", __func__);
	et6312b_reset(g_et6312b_data->client);	
	
	g_et6312b_data->rgb_state = g_et6312b_data->slope_state = 0;
}


#define ET6312B_EXTRACT_A(ARGB)	(ARGB >> 24)
#define ET6312B_EXTRACT_R(ARGB)	((ARGB >> 16) & 0xFF)
#define ET6312B_EXTRACT_G(ARGB)	((ARGB >> 8) & 0xFF)
#define ET6312B_EXTRACT_B(ARGB)	(ARGB & 0xFF)

#define ET6312B_FROM_COLOR(R,G,B)	((R << 16) | (G << 8) | B)

/**
* et6312b_color_scale - maps real color value to et6312b register 
*				value (current value)
**/

static enum led_brightness __inline et6312b_color_scale(
					u8 color, u8 led)
{
	/*
	 * y = mx + c;
	 * Supported Color Range(x): LED_LOWER_SUPPORTED_COLOR to LED_HIGHER_SUPPORTED_COLOR
	 * Current Register Range(y):  Min_current to MAX_CURRENT_R,MAX_CURRENT_G or MAX_CURRENT_B
	 * If y1 = mx1+c and y2 = mx2+c, 
	 * Then m =(y2-y1)/(x2-x1) and c = (y1x2 - y2x1)/(x2-x1)
	 * Here y1 is Min_current, y2 is MAX_CURRENT_R,MAX_CURRENT_G or MAX_CURRENT_B
	 * And x1 is LED_LOWER_SUPPORTED_COLOR, x2 is LED_HIGHER_SUPPORTED_COLOR
	 */
	 
#ifdef PARABOLIC_MAP
	return color ? 
		(((max_current[led] - min_current[led]) * (color - LED_LOWER_SUPPORTED_COLOR) \
		* (color - LED_LOWER_SUPPORTED_COLOR)) / COLOR_TUNING_DIV_FACTOR) + min_current[led] \
		: 255;
		
#else /* LINEAR */
	return color ? (((color * (max_current[led] - min_current[led])) + (LED_HIGHER_SUPPORTED_COLOR * min_current[led])\
		- (LED_LOWER_SUPPORTED_COLOR * max_current[led])) / (LED_HIGHER_SUPPORTED_COLOR - LED_LOWER_SUPPORTED_COLOR)) : 255;
#endif /* PARABOLIC_MAP */
	 
	 
}


static void et6312b_sort_RGB(u8 *a, u8 *b, u8 *c)
{
	u8 lo;
	u8 hi;
	u8 mid;
	if (*a<*b&&*a<*c)lo = *a;
	else
	{
		if (*b<*c)lo = *b;
		else lo = *c;
	}

	if (*a>*b&&*a>*c)hi = *a;
	else
	{
		if (*b<*c)hi = *c;
		else hi = *b;
	}

	if ((*a>=*b && *a<=*c) || (*a<=*b && *a>=*c))mid = *a;
	else
	{
		if ((*a>=*b && *b>=*c) || (*a<=*b && *c>=*b))mid = *b;
		else mid = *c;
	}
	*a = lo;
	*b = mid;
	*c = hi;
}


static void et6312b_color_correction(u8 *red, u8 *green, u8 *blue)
{
	u8 R, G, B;
	u8 r, g, b;
	u8 lo, mid, hi;
	
	u8 nlo = 0;
	u8 nmid = LED_LOWER_SUPPORTED_COLOR;
	u8 nhi = LED_HIGHER_SUPPORTED_COLOR;

	r = R = *red;
	g = G = *green;
	b = B = *blue;

	lo = *red;
	mid = *green;
	hi = *blue;
	et6312b_sort_RGB(&lo, &mid, &hi);



	if (hi >= LED_LOWER_SUPPORTED_COLOR && mid >= LED_LOWER_SUPPORTED_COLOR && lo >= LED_LOWER_SUPPORTED_COLOR)
	{
		nlo = lo;
		nmid = mid;
		nhi = hi;
	}
	else
	{
		if (hi <= REAL_LOWER_THRESHOLD && mid <= REAL_LOWER_THRESHOLD && lo <= REAL_LOWER_THRESHOLD)
		{
			nhi = 0;
			nmid = 0;
			nlo = 0;
		}
		else
		{
			if (lo < REAL_LOWER_THRESHOLD && mid < REAL_LOWER_THRESHOLD)
			{
				if (hi < LED_LOWER_SUPPORTED_COLOR)
				{
					nlo = 0;
					nmid = 0;
					nhi = LED_LOWER_SUPPORTED_COLOR;
				}
				else
				{
					nlo = 0;
					nmid = 0;
					nhi = hi;
				}
			}
			else if (lo < REAL_LOWER_THRESHOLD)
			{
				if (mid < LED_LOWER_SUPPORTED_COLOR)
				{
					nlo = 0;
					nmid = LED_LOWER_SUPPORTED_COLOR;
					if (hi + LED_LOWER_SUPPORTED_COLOR - mid < LED_LOWER_SUPPORTED_COLOR){
						nhi = hi + LED_LOWER_SUPPORTED_COLOR - mid;
					}
					else
					{
						nhi = LED_LOWER_SUPPORTED_COLOR;
					}
				}
				else
				{
					nlo = 0;
					nmid = mid;
					nhi = hi;
				}

			}
			else if (lo < LED_LOWER_SUPPORTED_COLOR && mid < LED_LOWER_SUPPORTED_COLOR && hi < LED_LOWER_SUPPORTED_COLOR)
			{
				nlo = LED_LOWER_SUPPORTED_COLOR;
				nmid = mid + (LED_LOWER_SUPPORTED_COLOR - lo);
				nhi = hi + (LED_LOWER_SUPPORTED_COLOR - lo);

			}
			else if (lo < LED_LOWER_SUPPORTED_COLOR && mid<LED_LOWER_SUPPORTED_COLOR && hi>LED_LOWER_SUPPORTED_COLOR)
			{
				nlo = LED_LOWER_SUPPORTED_COLOR;
				if (hi + LED_LOWER_SUPPORTED_COLOR - lo < LED_HIGHER_SUPPORTED_COLOR)
				{
					nmid = mid + (LED_LOWER_SUPPORTED_COLOR - lo);
					nhi = hi + (LED_LOWER_SUPPORTED_COLOR - lo);
				}
				else
				{
					nhi = LED_HIGHER_SUPPORTED_COLOR;
					nmid = nlo + (nhi - nlo)*(mid - lo) / (hi - lo);
				}
			}
			else if (lo < LED_LOWER_SUPPORTED_COLOR && mid > LED_LOWER_SUPPORTED_COLOR && hi > LED_LOWER_SUPPORTED_COLOR)
			{
				if (lo < 75)
				{
					nlo = 0;
					nmid = mid;
					nhi = hi;
				}
				else
				{
					nlo = LED_LOWER_SUPPORTED_COLOR;
					if (hi + LED_LOWER_SUPPORTED_COLOR - lo < LED_HIGHER_SUPPORTED_COLOR)
					{
						nmid = mid + (LED_LOWER_SUPPORTED_COLOR - lo);
						nhi = hi + (LED_LOWER_SUPPORTED_COLOR - lo);
					}
					else
					{
						nhi = LED_HIGHER_SUPPORTED_COLOR;
						nmid = nlo + (nhi - nlo)*(mid - lo) / (hi - lo);
					}
				}
			}
		}
	}

	if (R == lo)
	{
		r = nlo;
		if (G == mid){ g = nmid; b = nhi; }
		else
		{
			b = nmid; g = nhi;
		}
	}

	 else if (G == lo)
	{
		g = nlo;
		if (R == mid){ r = nmid; b = nhi; }
		else
		{
			b = nmid; r = nhi;
		}
	}

	 else if (B ==lo )
	{
		b = nlo;
		if (R == mid){ r = nmid; g = nhi; }
		else
		{
			g = nmid; r = nhi;
		}
	}
	
	if (hi-lo <= WHITE_COLOR_WINDOW_THRESHOLD && hi > WHITE_COLOR_LOWER_LIMIT)
	{
		*red = *blue = *green = nhi;
	}
	else
	{
		*red = r;
		*blue = b;
		*green = g;
	}
}

/**
* et6312b_tune_color - Maps natural colors to LED current 
**/
static enum led_brightness et6312b_tune_color( \
		enum led_brightness color, u8 intensity)
{
	u8 r = ET6312B_EXTRACT_R(color);
	u8 g = ET6312B_EXTRACT_G(color);
	u8 b = ET6312B_EXTRACT_B(color);
	u8 idx;	
	
	et6312b_color_correction(&r, &g, &b);
	
	if(r==g && g==b)
	{
		idx = 8; // For White color
	}
	else{
		idx = ((r!=0) << 2) | ((g!=0) << 1) | (b!=0);
	}
	
	min_current[LED_R] = et6312b_current_map[idx][LED_R][LOWER_RANGE_IDX];
	min_current[LED_G] = et6312b_current_map[idx][LED_G][LOWER_RANGE_IDX];
	min_current[LED_B] = et6312b_current_map[idx][LED_B][LOWER_RANGE_IDX];		
	max_current[LED_R] = et6312b_current_map[idx][LED_R][HIGHER_RANGE_IDX];
	max_current[LED_G] = et6312b_current_map[idx][LED_G][HIGHER_RANGE_IDX];
	max_current[LED_B] = et6312b_current_map[idx][LED_B][HIGHER_RANGE_IDX];	
	
	return ET6312B_FROM_COLOR(et6312b_color_scale(r, LED_R), \
		et6312b_color_scale(g, LED_G), et6312b_color_scale(b, LED_B)); 
}


#define ET6312B_DISABLE_RGBLED(CURR_ON, MODE , CURR_MODE_BIT_POS) \
	(CURR_ON ^ (MODE << CURR_MODE_BIT_POS))

/**
* et6312b_set_constant_current - Sets LED intensity & enable constant light
* @rgb_num - rgb LED number to be configured in constant current mode
**/
static void __inline et6312b_set_constant_current(u8 rgb_num, u32 color)
{
	u8 const_curr = (u8) ET6312B_RGBx_CONST_CURR_ON;
	//for red
	if(ET6312B_EXTRACT_R(color) == 255)
	{	
		const_curr = ET6312B_DISABLE_RGBLED(const_curr, ET6312B_LED_ALWAYS_ON, ET6312B_LED_R_CURR_MODE_BIT_POS);
	}
	//for green
	if(ET6312B_EXTRACT_G(color) == 255)
	{
		const_curr = ET6312B_DISABLE_RGBLED(const_curr, ET6312B_LED_ALWAYS_ON, ET6312B_LED_G_CURR_MODE_BIT_POS);
	}
	//for blue
	if(ET6312B_EXTRACT_B(color) == 255)
	{
		const_curr = ET6312B_DISABLE_RGBLED(const_curr, ET6312B_LED_ALWAYS_ON, ET6312B_LED_B_CURR_MODE_BIT_POS);
	}
	
	//printk(KERN_ERR "[smartglow] %s\n", __func__);
	
	g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGBx_BASE + rgb_num] = const_curr;
}


/**
* et6312b_set_slope_current - To Set the LED intensity and enable them
* @rgb_num - rgb LED number to be configured in slope current mode
* @ontime - In blink/breath pattern, it says LED ON time in ms.
*		in this driver, its rescaled to multiples of 128ms.
* @offtime - In blink/breath pattern, it says LED OFF time in ms.
*		in this driver, its rescaled to multiples of 128ms.
*  Note: total flash duration = ontime + offtime, is multiples of 128ms.
*		max duration is 16380ms
**/
static void et6312b_set_slope_current(u8 rgb_num, u16 ontime, u16 offtime, u32 color)
{
	u16 flash_period;
	u8 rfscale;
	u8 slope_curr = (u8) ET6312B_RGBx_SLOPE_CURR_ON;
	
		//for red
	if(ET6312B_EXTRACT_R(color) == 255)
	{
		slope_curr = ET6312B_DISABLE_RGBLED(slope_curr, ET6312B_LED_THREAD1, ET6312B_LED_R_CURR_MODE_BIT_POS);
	}
	//for green
	if(ET6312B_EXTRACT_G(color) == 255)
	{
		slope_curr = ET6312B_DISABLE_RGBLED(slope_curr, ET6312B_LED_THREAD1, ET6312B_LED_G_CURR_MODE_BIT_POS);
	}
	//for blue
	if(ET6312B_EXTRACT_B(color) == 255)
	{
		slope_curr = ET6312B_DISABLE_RGBLED(slope_curr, ET6312B_LED_THREAD1, ET6312B_LED_B_CURR_MODE_BIT_POS);	
	}
	
	//printk(KERN_ERR "[smartglow] %s\n", __func__);
	
	g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGBx_BASE + rgb_num] = slope_curr;
				
	/* flash period is total LED ON time and off time.
	the register value steps 0,1,..127 is 128ms, 256ms,.., 16380ms 
	*/
	flash_period = (ontime + offtime);
	if(unlikely(flash_period > ET6312B_MAX_FLASH_PERIOD))
		flash_period = ET6312B_MAX_FLASH_PERIOD;
		
	/* set flash period and type as linear */
	g_et6312b_data->pseudo_reg[ET6312B_REG_FLASH_PERIOD_RGB_BASE + \
				(rgb_num * ET6312B_REG_FLASH_PERIOD_RGB_OFFSET)] = \
				/* ET6312B_RGBx_FLASH_TYPE | */ ((flash_period >> 7) + 1);
	
	// Ton1 = Trise + full ON. used as % of flash period.
	// Here, We need Ontime = Trise + full ON (=~0) + Tfall
	// so, Ton1 = Ontime/2. (Which is trise), 
	// Therefore, Ton % is ((Ontime/2) / (ontime + offtime) * 100)
	// Ton1 register = (Ton %) * 10 / 4
	ontime >>= 1;
	g_et6312b_data->pseudo_reg[ET6312B_REG_FLASH_TON1_RGB_BASE + \
				(rgb_num * ET6312B_REG_FLASH_PERIOD_RGB_OFFSET)] = \
				(((250 * ontime) / flash_period) + 1);
		
	//TON2 masrk 0 as we don not need it. 
	//(As the buffer is already zero no need to update again)
	/* g_et6312b_data->pseudo_reg[ET6312B_REG_FLASH_TON2_RGB_BASE + 
				(rgb_num * ET6312B_REG_FLASH_PERIOD_RGB_OFFSET)] = 0; */
				
	if(ontime < ET6312B_RAMP_8x_FASTER_MAX_TIME){
		rfscale = ET6312B_RAMP_8x_FASTER_VAL;
		ontime >>= 4;		
	}
	else if(ontime < ET6312B_RAMP_1x_NORMAL_MAX_TIME){
		rfscale = ET6312B_RAMP_1x_NORMAL_VAL;
		ontime >>= 7;		
	}
	else if(ontime < ET6312B_RAMP_2x_SLOWER_MAX_TIME){
		rfscale = ET6312B_RAMP_2x_SLOWER_VAL;
		ontime >>= 8;
	}
	else  /* if(ontime < ET6312B_RAMP_4x_SLOWER_MAX_TIME) */ {
		rfscale = ET6312B_RAMP_4x_SLOWER_VAL;
		ontime >>= 9;
	}
	
	g_et6312b_data->pseudo_reg[ET6312B_REG_RFSCALE] |= \
				rfscale << (ET6312B_RGBx_RFSCALE_BITS * rgb_num);
	/* Mark rise and fall duration same */
	g_et6312b_data->pseudo_reg[ET6312B_REG_RAMP_RATE_RGB_BASE + \
				(rgb_num * ET6312B_REG_FLASH_PERIOD_RGB_OFFSET)] = \
				ontime | (ontime << ET6312B_RAMP_FALL_POS);
}


#define BUFFSIZE 3
int front =0;
u32 real_color[BUFFSIZE] ={0};
u32 matched_color[BUFFSIZE] ={0};

/**
* et6312b_config_led - Animate smartglow by the config written 
*		from upper layer
* returns 1 - register configuration updated. 
* 		  0 - Register values to be updated through I2C.
**/
int et6312b_config_led(u8 rgb_num)
{
	if(!g_et6312b_data->led[rgb_num].color)	{// Switch off the LED
		
		/* Update the LED ON status */
		g_et6312b_data->rgb_state &= ~(1 << rgb_num);
		g_et6312b_data->slope_state &= ~(1 << rgb_num);
		
		// If all LEDs are switched off, can simply power off
		if(!g_et6312b_data->rgb_state) {
			smartglow_reset();
		}
		else {	
			u8 offset = ET6312B_REG_RGBx_BASE + (ET6312B_REG_RGBx_OFFSET * rgb_num);
			
			memset(&g_et6312b_data->pseudo_reg[offset], 0, ET6312B_REG_RGBx_OFFSET);
			g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGBx_BASE + rgb_num] = ET6312B_RESET;
		}
	}
	else {// Switch ON the LED 
		u8 offset;
		u32 color;
		bool matched=0;
		int i;
		for(i=0;i<BUFFSIZE;i++){
			if(real_color[i]==g_et6312b_data->led[rgb_num].color){
				color = matched_color[i];
				matched = 1;
				break;
			}	
		}
		/* Apply color tuning */
		if(!matched){
			color = et6312b_tune_color(
					g_et6312b_data->led[rgb_num].color & 0x00FFFFFF,
					DEFAULT_BRIGHTNESS);
			matched_color[front] = color;
			real_color[front]=g_et6312b_data->led[rgb_num].color;
			front=(front+1)%BUFFSIZE;
		}
		/* Set LED color */		
		offset = ET6312B_REG_RGBx_BASE + (ET6312B_REG_RGBx_OFFSET * rgb_num);
		
		#ifdef ET6312B_DEBUG_TUNING
		g_et6312b_data->r = g_et6312b_data->pseudo_reg[offset + ET6312B_REG_RGBx_LED_R_OFFSET] = \
					ET6312B_EXTRACT_R(color);
		g_et6312b_data->g = g_et6312b_data->pseudo_reg[offset + ET6312B_REG_RGBx_LED_G_OFFSET] = \
					ET6312B_EXTRACT_G(color);
		g_et6312b_data->b = g_et6312b_data->pseudo_reg[offset + ET6312B_REG_RGBx_LED_B_OFFSET] = \
					ET6312B_EXTRACT_B(color);
		#else
		g_et6312b_data->pseudo_reg[offset + ET6312B_REG_RGBx_LED_R_OFFSET] = \
					ET6312B_EXTRACT_R(color);
		g_et6312b_data->pseudo_reg[offset + ET6312B_REG_RGBx_LED_G_OFFSET] = \
					ET6312B_EXTRACT_G(color);
		g_et6312b_data->pseudo_reg[offset + ET6312B_REG_RGBx_LED_B_OFFSET] = \
					ET6312B_EXTRACT_B(color);
		#endif /* ET6312B_DEBUG_TUNING */
		
		if(!g_et6312b_data->led[rgb_num].offtime) {
			// Constant Light
			et6312b_set_constant_current(rgb_num, color);
			
			/* Update the LED slope current status as 0 */
			g_et6312b_data->slope_state &= ~(1 << rgb_num);
		}
		else {
			// Set LED slope current
			et6312b_set_slope_current(rgb_num,  \
					g_et6312b_data->led[rgb_num].ontime,  \
					g_et6312b_data->led[rgb_num].offtime, color);
					
			/* Update the LED slope current status */
			g_et6312b_data->slope_state |= (1 << rgb_num);

#if 0	// To Do : As UI framework needs to know the status. its handled at
		//	upper layer to avoid redundant execution.
			if(g_et6312b_data->led[rgb_num].repeat_count){
				// Calculate and start timer				
			}
#endif			
		}	

		/* Update the LED ON status */
		g_et6312b_data->rgb_state |= (1 << rgb_num);		
	}
		
	return 0;
}
 
/**
* et6312b_animate_work - Animate smartglow by the config written 
*		from upper layer
**/
static void et6312b_animate_work(struct work_struct *work)
{
	printk(KERN_ERR" [SmartGlow] %s", __func__);
	
	mutex_lock(&g_et6312b_data->mutex);
	if((g_et6312b_data->in_ledaccess & 0xF0) == 0)
	{
		if(!g_et6312b_data->IsResetMode) {	
			smartglow_reset();
		}
		
		if(!g_et6312b_data->in_ledaccess)
		{
			if(g_et6312b_data->IsResetMode) {
				memset(g_et6312b_data->led, 0, sizeof(g_et6312b_data->led));
			
				// Mark smartglow is in reset mode
				g_et6312b_data->IsResetMode  = 1;
			}
			
			goto work_exit;
		}
	}
	
	g_et6312b_data->IsResetMode  = 0;
	
	/* Ignore the Reset indicator */
	g_et6312b_data->in_ledaccess &= 0xF;
	
	if(g_et6312b_data->in_ledaccess <= MAX_SMARTGLOW_LEDS)
	{
		//printk(KERN_ERR " [SmartGlow] %s: %d", __func__, __LINE__);
		g_et6312b_data->in_ledaccess--;
		
		et6312b_config_led(g_et6312b_data->in_ledaccess);
	}
	else
	{
		int rgb_num;
		
		if(g_et6312b_data->in_ledaccess == 0xF)
		{
			//printk(KERN_ERR" [SmartGlow] %s: %d", __func__, __LINE__);
			for(rgb_num = 0; rgb_num < MAX_SMARTGLOW_LEDS; rgb_num++)	
			{
				et6312b_config_led(rgb_num);
			}
		}
		else if(g_et6312b_data->in_ledaccess == 0xA)
		{	
			u8 offset1, offset2, rfscale;
			//printk(KERN_ERR" [SmartGlow] %s: %d", __func__, __LINE__);
		
			//Configure the First LED
			et6312b_config_led(0);
			
			// Copy First LED configurations to other LEDs
			for(rgb_num = 1, offset1 = ET6312B_REG_RGBx_BASE + ET6312B_REG_RGBx_OFFSET,
					offset2 = ET6312B_REG_FLASH_PERIOD_RGB_BASE + ET6312B_REG_FLASH_PERIOD_RGB_OFFSET; 
				rgb_num < MAX_SMARTGLOW_LEDS; 
				rgb_num++, offset1 += ET6312B_REG_RGBx_OFFSET,
					offset2 += ET6312B_REG_FLASH_PERIOD_RGB_OFFSET)	
			{									
				//printk(KERN_ERR" [SmartGlow] %s: %d", __func__, __LINE__);
				memcpy(&g_et6312b_data->pseudo_reg[offset1], \
					&g_et6312b_data->pseudo_reg[ET6312B_REG_RGBx_BASE], \
					ET6312B_REG_RGBx_OFFSET);
				memcpy(&g_et6312b_data->pseudo_reg[offset2], \
					&g_et6312b_data->pseudo_reg[ET6312B_REG_FLASH_PERIOD_RGB_BASE], \
					ET6312B_REG_FLASH_PERIOD_RGB_OFFSET);
				g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGBx_BASE + \
					rgb_num] = g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGBx_BASE];
			}
			rfscale = g_et6312b_data->pseudo_reg[ET6312B_REG_RFSCALE] & ET6312B_RGBx_RFSCALE_BITS;
			g_et6312b_data->pseudo_reg[ET6312B_REG_RFSCALE] =  (rfscale | \
					(rfscale << ET6312B_RGB2_RFSCALE_BIT_POS) | \
					(rfscale << ET6312B_RGB3_RFSCALE_BIT_POS) | \
					(rfscale << ET6312B_RGB4_RFSCALE_BIT_POS) );
			
			/* Update LED status */
			g_et6312b_data->rgb_state = 0b1111;
			
			if(g_et6312b_data->slope_state)
				g_et6312b_data->slope_state = 0b1111;
		}
	}
	et6312b_i2c_write_all();
	et6312b_i2c_chip_enable();
	
work_exit:	
	mutex_unlock(&g_et6312b_data->mutex);
	
	//printk(KERN_ERR " [SmartGlow] %s: %d", __func__, __LINE__);
}
 
 
/**
* et6312b_initialize - initialize the basic configurations required for 
*		smartglow
**/
static int et6312b_initialize(struct i2c_client *client)
{
	int ret; 
	
	/* update iMax */
	g_et6312b_data->pseudo_reg[ET6312B_REG_CHIPCTR] = \
			(g_et6312b_data->iMax << ET6312B_IMAX_BIT_POS) | \
			(1 << ET6312B_AUTO_BRECON_BIT_POS) | \
			(1 << ET6312B_SOFTDN_EN_BIT_POS);
			
	/* Thread mode, clk mode are already zero, so not required to update */

	/* Check whether, smartglow IC communication is OK by doing a simple 
	write */
	if(unlikely((ret = i2c_smbus_write_byte_data(client, 
			ET6312B_REG_CHIPCTR, g_et6312b_data->pseudo_reg[ET6312B_REG_CHIPCTR])) < 0)) {
		dev_err(&g_et6312b_data->client->dev, \
			"%s: failure on i2c \n", __func__);
		return ret;
	}
	
	return 0;
}



/**
* smartglow_set_brightness - Get the brightness value from upper 
* 		layer through sysfs and schedule the work
**/
void et6312b_set_brightness(struct led_classdev *cdev,
			enum led_brightness brightness)
{
	printk(KERN_ERR" [SmartGlow] No function Yet!.");
	return;
}


////////////////////////// SMART GLOW TUNING /////////////////////////


#ifdef ET6312B_DEBUG_TUNING

/**
* et6312b_current_reg_store - .
**/
static ssize_t et6312b_current_reg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)			
{
	dev_info(dev, " [SmartGlow] %s: %s", __func__, buf);
	
	sscanf(buf, "%d %d %d %d", &g_et6312b_data->a, \
		&g_et6312b_data->r, &g_et6312b_data->g, &g_et6312b_data->b);
	
	
	mutex_lock(&g_et6312b_data->mutex);
	
	smartglow_reset();
	if(g_et6312b_data->a)
	{
		u8 const_curr = ET6312B_RGBx_CONST_CURR_ON;
		
		g_et6312b_data->pseudo_reg[ET6312B_REG_RGB1_LED_R] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB2_LED_R] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB3_LED_R] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB4_LED_R] = g_et6312b_data->r;
			
		g_et6312b_data->pseudo_reg[ET6312B_REG_RGB1_LED_G] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB2_LED_G] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB3_LED_G] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB4_LED_G] = g_et6312b_data->g;
			
		g_et6312b_data->pseudo_reg[ET6312B_REG_RGB1_LED_B] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB2_LED_B] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB3_LED_B] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_RGB4_LED_B] = g_et6312b_data->b;
			
		if(g_et6312b_data->r == -1)
		{	
			const_curr = ET6312B_DISABLE_RGBLED(const_curr, ET6312B_LED_ALWAYS_ON, ET6312B_LED_R_CURR_MODE_BIT_POS);
		}
		if(g_et6312b_data->g == -1)
		{
			const_curr = ET6312B_DISABLE_RGBLED(const_curr, ET6312B_LED_ALWAYS_ON, ET6312B_LED_G_CURR_MODE_BIT_POS);
		}
		if(g_et6312b_data->b == -1)
		{
			const_curr = ET6312B_DISABLE_RGBLED(const_curr, ET6312B_LED_ALWAYS_ON, ET6312B_LED_B_CURR_MODE_BIT_POS);
		}
		
		g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGB0] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGB1] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGB2] = \
			g_et6312b_data->pseudo_reg[ET6312B_REG_LEDXMD_RGB3] = \
			const_curr;
			
		et6312b_i2c_write_all();
		et6312b_i2c_chip_enable();
	}	
	
	mutex_unlock(&g_et6312b_data->mutex);
	return len;
}

/**
* et6312b_current_reg_show - .
**/
static ssize_t et6312b_current_reg_show(struct device  *dev,
			struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR" [SmartGlow] %s\n", __func__);
	
	return sprintf(buf, "Status:%d R:%d G:%d B:%d", g_et6312b_data->a, \
		g_et6312b_data->r, g_et6312b_data->g, g_et6312b_data->b);
}


/**
* et6312b_current_range_store - 
**/
static ssize_t et6312b_current_range_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)			
{
	int data[8];
	
	dev_info(dev, "[SmartGlow] %s \n", __func__);
	
	sscanf(buf, "R:%d-%d G:%d-%d B:%d-%d W:%d-%d", \
		&data[0], &data[1],	&data[2], &data[3],	\
		&data[4], &data[5], &data[6], &data[7]);
		
	min_current[LED_R] = data[0];
	max_current[LED_R] = data[1];	
	min_current[LED_G] = data[2];
	max_current[LED_G] = data[3];	
	min_current[LED_B] = data[4];
	max_current[LED_B] = data[5];
	min_current[LED_W] = data[6];
	max_current[LED_W] = data[7];
	
	return len;
}

/**
* et6312b_current_range_show - .
**/
static ssize_t et6312b_current_range_show(struct device  *dev,
			struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR" [SmartGlow] %s\n", __func__);
	
	return sprintf(buf, \
		"\nCurrent Register Range:\nR:%d-%d\nG:%d-%d\nB:%d-%d\n", \
		min_current[LED_R], max_current[LED_R], \
		min_current[LED_G], max_current[LED_G], \
		min_current[LED_B], max_current[LED_B]);
}


/**
* et6312b_color_range_store - 
**/
static ssize_t et6312b_color_range_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)			
{
	int data[8];
	
	dev_info(dev, " [SmartGlow] %s \n", __func__);
	
	sscanf(buf, "R:%d-%d G:%d-%d B:%d-%d W:%d-%d", \
		&data[0], &data[1],	&data[2], &data[3],	\
		&data[4], &data[5], &data[6], &data[7]);
		
	lower_supported_color[LED_R] = data[0];
	higher_supported_color[LED_R] = data[1];	
	lower_supported_color[LED_G] = data[2];
	higher_supported_color[LED_G] = data[3];	
	lower_supported_color[LED_B] = data[4];
	higher_supported_color[LED_B] = data[5];	
	lower_supported_color[LED_W] = data[6];
	higher_supported_color[LED_W] = data[7];		

	return len;
}

/**
* et6312b_color_range_show - .
**/
static ssize_t et6312b_color_range_show(struct device  *dev,
			struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR" [SmartGlow] %s\n", __func__);
	
	return sprintf(buf, "\nSupported Color Range:\nR:%d-%d\nG:%d-%d\nB:%d-%d\nW:%d-%d\n", \
		lower_supported_color[LED_R], higher_supported_color[LED_R], \
		lower_supported_color[LED_G], higher_supported_color[LED_G], \
		lower_supported_color[LED_B], higher_supported_color[LED_B], \
		lower_supported_color[LED_W], higher_supported_color[LED_W]);
}


#endif /* ET6312B_DEBUG_TUNING */
 


/*-----------------------------------------------------------------------------
 *    E T 6 3 1 2 B     D E V I C E     E N U M E R A T I O N 
 *---------------------------------------------------------------------------*/


/**
* Device attribute set for sysfs interface for Animations
**/
/* permission for sysfs node */
static DEVICE_ATTR(blink, S_IRUGO | S_IWUSR | S_IWGRP, 
	et6312b_blink_show, et6312b_blink_store);
#ifdef ET6312B_DEBUG
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR | S_IWGRP, 
	et6312b_reg_show, et6312b_reg_store);
#endif /* ET6312B_DEBUG */
#ifdef ET6312B_DEBUG_TUNING
static DEVICE_ATTR(current_reg, S_IRUGO | S_IWUSR | S_IWGRP, 
	et6312b_current_reg_show, et6312b_current_reg_store);
static DEVICE_ATTR(current_range, S_IRUGO | S_IWUSR | S_IWGRP, 
	et6312b_current_range_show, et6312b_current_range_store);
static DEVICE_ATTR(color_range, S_IRUGO | S_IWUSR | S_IWGRP, 
	et6312b_color_range_show, et6312b_color_range_store);
#endif /* ET6312B_DEBUG_TUNING */


static struct attribute *et6312b_class_attrs[] = {
	&dev_attr_blink.attr,
#ifdef ET6312B_DEBUG
	&dev_attr_reg.attr,	
#endif /* ET6312B_DEBUG */
#ifdef ET6312B_DEBUG_TUNING
	&dev_attr_current_reg.attr,	
	&dev_attr_current_range.attr,	
	&dev_attr_color_range.attr,	
#endif /* ET6312B_DEBUG_TUNING */
	NULL,
};

static struct attribute_group et6312b_attr_group = {
	.attrs = et6312b_class_attrs,
};

/**
* et6312b_register_ledclass - Registers LED class for smart glow.
**/
static int et6312b_register_ledclass(struct et6312b_data *data)
{
	struct device *dev = &data->client->dev;
	int ret = 0;
	
	data->class_dev.brightness_set = et6312b_set_brightness;
	data->class_dev.name = ET6312B_LED_DEVICE_NAME;
	data->class_dev.brightness = 0;
	data->class_dev.max_brightness = 0xFFFFFFFF;
	data->class_dev.flags = 0;		

	/* Register LED Class device interface */
	if(unlikely((ret = led_classdev_register(dev, &data->class_dev)) < 0)) {
		dev_err(dev, "can not register led : smartglow\n");
		return ret;
	}

	/* Create additional class attribute to expose blink 
		related functionalities */
	if(unlikely((ret = sysfs_create_group(&data->class_dev.dev->kobj,
			&et6312b_attr_group)) < 0 )) {
		dev_err(dev, "can not register sysfs attribute\n");
		goto ERR_SYSFS;
	}
	
	/*WorkQ to schedule request comes from user space */
	INIT_WORK(&(data->animate_work), et6312b_animate_work);

	return 0;
	
ERR_SYSFS:
	led_classdev_unregister(&data->class_dev);
	
	return ret;
}

#ifdef CONFIG_OF
/**
* et6312b_parse_dt - Parse the device enumerations.
**/
static int et6312b_parse_dt(struct device *dev, struct et6312b_data *data) {
	struct device_node *np = dev->of_node;
	int ret, temp;
	int combinations, idx, mapidx, ledno;
	
	/* Imax */
	ret = of_property_read_u32(np, "et6312b,iMax", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Error in getting et6312b,iMax.\n");
		data->iMax = ET6312B_DEFAULT_IMAX;
	}
	else{
		data->iMax = temp;
	}
	
	/* Clk synchronization mode */
	ret = of_property_read_u32(np, "et6312b,clk_mode", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Error in getting et6312b,clk_mode.\n");
		data->clk_mode = ET6312B_DEFAULT_CLK;
	}
	else{
		data->clk_mode = temp;
	}
	
	/* Supported real color range */
	ret = of_property_read_u32_index(np, "et6312b,color_range", 0, &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Error in getting et6312b,color_range.\n");
	}
	else{
		combinations = temp;
		
		for(idx = 0; idx < combinations; idx++) {			
			ret = of_property_read_u32_index(np, "et6312b,color_range", \
					1 + (idx * 7), &temp);
			if (IS_ERR_VALUE(ret)) {
				dev_err(dev, "Error in getting et6312b,color_range.\n");
			}
			else{
				mapidx = temp;
			
				for(ledno = 0; ledno < 3; ledno++) {			
					ret = of_property_read_u32_index(np, "et6312b,color_range", \
							2 + (idx * 7) + (ledno * 2) + LOWER_RANGE_IDX, &temp);
					if (IS_ERR_VALUE(ret)) {
						dev_err(dev, "Error in getting et6312b,color_range.\n");
					}
					else{
						et6312b_color_map[mapidx][ledno][LOWER_RANGE_IDX] = temp;
					}
					ret = of_property_read_u32_index(np, "et6312b,color_range", \
							2 + (idx * 7) + (ledno * 2) + HIGHER_RANGE_IDX, &temp);
					if (IS_ERR_VALUE(ret)) {
						dev_err(dev, "Error in getting et6312b,color_range.\n");
					}
					else{
						et6312b_color_map[mapidx][ledno][HIGHER_RANGE_IDX] = temp;
					}
				}
			}
		}
	}
	
	/* Supported real current range */
	ret = of_property_read_u32_index(np, "et6312b,current_range", 0, &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Error in getting et6312b,current_range.\n");
	}
	else{
		combinations = temp;
		
		for(idx = 0; idx < combinations; idx++) {
			
			/* Supported real color range */
			ret = of_property_read_u32_index(np, "et6312b,current_range", \
					1 + (idx * 7), &temp);
			if (IS_ERR_VALUE(ret)) {
				dev_err(dev, "Error in getting et6312b,current_range.\n");
			}
			else{
				mapidx = temp;
			
				for(ledno = 0; ledno < 3; ledno++) {			
					ret = of_property_read_u32_index(np, "et6312b,current_range", \
							2 + (idx * 7) + (ledno * 2) + LOWER_RANGE_IDX, &temp);
					if (IS_ERR_VALUE(ret)) {
						dev_err(dev, "Error in getting et6312b,current_range.\n");
					}
					else{
						et6312b_current_map[mapidx][ledno][LOWER_RANGE_IDX] = temp;
					}
					ret = of_property_read_u32_index(np, "et6312b,current_range", \
							2 + (idx * 7) + (ledno * 2) + HIGHER_RANGE_IDX, &temp);
					if (IS_ERR_VALUE(ret)) {
						dev_err(dev, "Error in getting et6312b,current_range.\n");
					}
					else{
						et6312b_current_map[mapidx][ledno][HIGHER_RANGE_IDX] = temp;
					}
				}
			}
		}
	}
	
	//To Do: For any tuning parameter calcultation optimization.

	return 0;
}
#endif


#if defined(CONFIG_SEC_FACTORY)
/**
* IsLCDConnected - gets whether LCD/OCTA is connected to PBA or not by LCD ID.
**/
bool get_lcd_status(void)
{
	char buf[24] = {0};
	struct file *fp = filp_open(LCD_ID_PATH, O_RDONLY, 0664);
	
	if(unlikely((IS_ERR(fp))))
	{
		printk(KERN_ERR "%s %s open failed\n", __func__, LCD_ID_PATH);
		goto lcd_exit;
	}
	
	kernel_read(fp, fp->f_pos, buf, 4);
	
	filp_close(fp, current->files);
	
lcd_exit:
	printk("[smartglow] LCD : %s - %c \n", buf, buf[0]);
	
	/* If LCD buffer itself NULL, retry for a while */
	if(buf[0] == 0)
	{
		static int smartglow_lcd_chk_cnt = 0;
		
		if(smartglow_lcd_chk_cnt++ < BOOT_POLLING_CNT)
		{
			schedule_delayed_work(&g_et6312b_data->boot_work, \
					msecs_to_jiffies(BOOT_WORK_POLL_DELAY));
		}

		return -1;
	}
	
	return (strstr(buf, "NULL") == NULL);
}

/**
* GlowOnNoLCDBooting - Glow smartglow based on battery capacity,
*			When no LCD is connected only in factory binary. 
**/
void GlowOnNoLCDBooting(void)
{
	/* Only if LCD is not attached proceed further */
	if(get_lcd_status() == 0)
	{
		union power_supply_propval capacity;
		
		capacity.intval = 0;
		psy_do_property("battery", get, POWER_SUPPLY_PROP_CAPACITY, capacity);
		
		printk("[smartglow] battery : %d", capacity.intval);
		
		if(capacity.intval == 0)
		{
			g_et6312b_data->led[0].color = 0xFF0000;
			
			schedule_delayed_work(&g_et6312b_data->boot_work, \
				msecs_to_jiffies(BOOT_WORK_POLL_DELAY));
				
			g_et6312b_data->in_ledaccess = 0xA;
			g_et6312b_data->led[0].ontime = 500;
				g_et6312b_data->led[0].offtime = 5000;
			
			schedule_work(&g_et6312b_data->animate_work);
			
			return;
		}
		else if(capacity.intval < 32)
		{
			g_et6312b_data->led[0].color = 0xFF00; 			
		}
		else
		{
			g_et6312b_data->led[0].color = 0xFF; 			
		}
		
		g_et6312b_data->in_ledaccess = 0xA;
		g_et6312b_data->led[0].ontime = \
			g_et6312b_data->led[0].offtime = 0;
		
		schedule_work(&g_et6312b_data->animate_work);
	}
}

/**
* et6312b_boot_work - Glow smartglow based on battery capacity,
*			When no LCD is connected only in factory binary. 
**/
static void et6312b_boot_work(struct work_struct *work)
{
	GlowOnNoLCDBooting();	
}

#endif /* CONFIG_SEC_FACTORY */


/**
* et6312b_probe - Allocate the resource and initialize on matched  
* 	et6312b LED device found.
**/
static int et6312b_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct et6312b_data *data;
	int ret;
	
#ifdef CONFIG_BATTERY_SAMSUNG	
	extern unsigned int lpcharge;
	
	if(lpcharge)
	{
		dev_info(&client->dev, "%s Skip loading - LPM Charging mode!!..\n", __func__);
		
		return 0;
	}
#endif	

	dev_info(&client->dev, "%s start\n", __func__);
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "need I2C_FUNC_I2C\n");
		return -ENXIO;
	}
	
	if(!(data = kzalloc(sizeof(*data), GFP_KERNEL))) {
		dev_err(&client->dev,
			"failed to allocate driver data\n");
		return -ENOMEM;
	}
	
	g_et6312b_data = data;  //To Do : Check to encapsulate.
	
#ifdef CONFIG_OF
	if(unlikely((ret = et6312b_parse_dt(&client->dev, data)))) {
		dev_err(&client->dev, 
			"[%s] et6312b parse dt failed\n", __func__);
		goto exit_free;
	}
#else
#error "No Pdata from board file is implemented!"
#endif

	i2c_set_clientdata(client, data);
	data->client = client;
	
	data->pinctrl = devm_pinctrl_get(&client->dev);
	if (unlikely(IS_ERR(data->pinctrl))) {
		dev_err(&client->dev, "%s: Failed to get pinctrl data\n", __func__);
		ret = PTR_ERR(data->pinctrl);
		goto err_get_pinctrl;
	}
	
	data->pinctrl_state[ET6312B_OFF] = pinctrl_lookup_state(data->pinctrl, "default");
	if (unlikely(IS_ERR(data->pinctrl_state[ET6312B_OFF]))) {
		dev_err(&data->client->dev, "%s: Failed to lookup default pinctrl.\n", __func__);
		goto err_get_pinctrl;
	}
	data->pinctrl_state[ET6312B_ON] = pinctrl_lookup_state(data->pinctrl, "s_led_en");
	if (unlikely(IS_ERR(data->pinctrl_state[ET6312B_ON]))) {
		dev_err(&data->client->dev, "%s: Failed to lookup s_led_en pinctrl.\n", __func__);
		goto err_get_pinctrl;
	}
	
	if(unlikely((ret = et6312b_initialize(client)) < 0)) {
		dev_err(&client->dev, "et6312b Init failed\n");
		goto exit_free;
	}
	
	mutex_init(&data->mutex);
	
	if(unlikely((ret = et6312b_register_ledclass(data)) < 0)) {
		dev_err(&client->dev, "[SmartGlow] class register failed\n");
		goto exit_init;
	}	
	
#if defined (CONFIG_SEC_FACTORY)
	//To Do: Need to add on demand from HW team
	//GlowOnJigMode();

	INIT_DELAYED_WORK(&(data->boot_work), et6312b_boot_work);
	
	schedule_delayed_work(&data->boot_work, \
				msecs_to_jiffies(BOOT_WORK_DELAY));
#endif /* CONFIG_SEC_FACTORY */

	dev_info(&client->dev, "%s done\n", __func__);

	return ret;
	
err_get_pinctrl:
exit_init:
	mutex_destroy(&data->mutex);

exit_free:
	kfree(data);
	return ret;
}


/**
* et6312b_remove - cleans up the resource allocated for 
*		et6312b LED on probe
**/
static int et6312b_remove(struct i2c_client *client)
{
	struct et6312b_data *data = i2c_get_clientdata(client);
	
	dev_dbg(&client->dev, "%s\n", __func__);
	
	led_classdev_unregister(&data->class_dev);
	cancel_work_sync(&data->animate_work);	
	
	/* Switch off all LEDs */
	et6312b_reset(client);	
	
	mutex_destroy(&data->mutex);
	kfree(data);

	return 0;
}

/**
* et6312b_shutdown - Things to be done on shut down call.
**/
static void et6312b_shutdown(struct device *dev)
{
	/* Switch off all LEDs; avoids LED to glow in power down*/
	et6312b_reset(g_et6312b_data->client);
}


/**
* i2c driver structure for et6312b for matching devices
**/
#ifdef CONFIG_OF
static struct of_device_id et6312b_match_table[] = {
	{ .compatible = "etek,et6312b", },
	{ },
};
#else
#define et6312b_match_table NULL
#endif

/**
* list of et6312b, i2c client ids
**/
static const struct i2c_device_id et6312b_ids[] = {
	{ "et6312b", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, et6312b_ids);

/**
* i2c driver structure for et6312b
**/
static struct i2c_driver et6312b_i2c_driver = {
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= ET6312B_LED_DEVICE_NAME,
		.of_match_table = et6312b_match_table,
		.shutdown = et6312b_shutdown,
	},
	.id_table	= et6312b_ids,
	.probe		= et6312b_probe,
	.remove		= et6312b_remove,
};


/**
* Init and Register i2c client driver for et6312b
**/
module_i2c_driver(et6312b_i2c_driver);


MODULE_AUTHOR("Sankara Narayanan T <sankaranarayanan.t@samsung.com>");
MODULE_DESCRIPTION("smartglow led driver based on et6312b");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:"ET6312B_LED_DEVICE_NAME);
MODULE_VERSION("1.0");


