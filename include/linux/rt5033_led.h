/* include/linux/lights/rt5033_fled.h
 * Header of Richtek RT5033 Flash LED Driver
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef LINUX_LIGHTS_RT5033_FLED_H
#define LINUX_LIGHTS_RT5033_FLED_H
#include <linux/kernel.h>

/* RT5033_IFLASH1 */
#define RT5033_FLASH_IOUT1		0x3F

/* RT5033_IFLASH2 */
#define RT5033_FLASH_IOUT2		0x3F

/* RT5033_ITORCH */
#define RT5033_TORCH_IOUT1		0x0F
#define RT5033_TORCH_IOUT2		0xF0

/* RT5033_TORCH_TIMER */
#define RT5033_TORCH_TMR_DUR		0x0F
#define RT5033_DIS_TORCH_TMR		0x40
#define RT5033_TORCH_TMR_MODE		0x80
#define RT5033_TORCH_TMR_MODE_ONESHOT	0x00
#define RT5033_TORCH_TMR_MDOE_MAXTIMER	0x01

/* RT5033_FLASH_TIMER */
#define RT5033_FLASH_TMR_DUR		0x0F
#define RT5033_FLASH_TMR_MODE		0x80
/* RT5033_FLASH_TMR_MODE value */
#define RT5033_FLASH_TMR_MODE_ONESHOT	0x00
#define RT5033_FLASH_TMR_MDOE_MAXTIMER	0x01

/* RT5033_FLASH_EN */
#define RT5033_TORCH_FLED2_EN		0x03
#define RT5033_TORCH_FLED1_EN		0x0C
#define RT5033_FLASH_FLED2_EN		0x30
#define RT5033_FLASH_FLED1_EN		0xC0
/* RT5033_TORCH_FLEDx_EN value */
#define RT5033_TORCH_OFF		0x00
#define RT5033_TORCH_BY_FLASHEN	0x01
#define RT5033_TORCH_BY_TORCHEN	0x02
#define RT5033_TORCH_BY_I2C		0X03
/* RT5033_FLASH_FLEDx_EN value */
#define RT5033_FLASH_OFF		0x00
#define RT5033_FLASH_BY_FLASHEN	0x01
#define RT5033_FLASH_BY_TORCHEN	0x02
#define RT5033_FLASH_BY_I2C		0x03

/* RT5033_VOUT_CNTL */
#define RT5033_BOOST_FLASH_MODE	0x07
#define RT5033_BOOST_FLASH_FLEDNUM	0x80
/* RT5033_BOOST_FLASH_MODE vaule*/
#define RT5033_BOOST_FLASH_MODE_OFF	0x00
#define RT5033_BOOST_FLASH_MODE_FLED1	0x01
#define RT5033_BOOST_FLASH_MODE_FLED2	0x02
#define RT5033_BOOST_FLASH_MODE_BOTH	0x03
#define RT5033_BOOST_FLASH_MODE_FIXED	0x04
/* RT5033_BOOST_FLASH_FLEDNUM vaule*/
#define RT5033_BOOST_FLASH_FLEDNUM_1	0x00
#define RT5033_BOOST_FLASH_FLEDNUM_2	0x80

/* RT5033_VOUT_FLASH1 */
#define RT5033_BOOST_VOUT_FLASH	0x7F
#define RT5033_BOOST_VOUT_FLASH_FROM_VOLT(mV)				\
		((mV) <= 3300 ? 0x00 :					\
		((mV) <= 5500 ? (((mV) - 3300) / 25 + 0x0C) : 0x7F))

#define MAX_FLASH_CURRENT	1000	/* 1000mA(0x1f) */
#define MAX_TORCH_CURRENT	250	/* 250mA(0x0f) */
#define MAX_FLASH_DRV_LEVEL	63	/* 15.625 + 15.625*63 mA */
#define MAX_TORCH_DRV_LEVEL	15	/* 15.625 + 15.625*15 mA */

enum rt5033_led_id
{
	RT5033_FLASH_LED_1,
	RT5033_FLASH_LED_2,
	RT5033_TORCH_LED_1,
	RT5033_TORCH_LED_2,
	RT5033_LED_MAX,
};

enum rt5033_led_time
{
	RT5033_FLASH_TIME_62P5MS,
	RT5033_FLASH_TIME_125MS,
	RT5033_FLASH_TIME_187P5MS,
	RT5033_FLASH_TIME_250MS,
	RT5033_FLASH_TIME_312P5MS,
	RT5033_FLASH_TIME_375MS,
	RT5033_FLASH_TIME_437P5MS,
	RT5033_FLASH_TIME_500MS,
	RT5033_FLASH_TIME_562P5MS,
	RT5033_FLASH_TIME_625MS,
	RT5033_FLASH_TIME_687P5MS,
	RT5033_FLASH_TIME_750MS,
	RT5033_FLASH_TIME_812P5MS,
	RT5033_FLASH_TIME_875MS,
	RT5033_FLASH_TIME_937P5MS,
	RT5033_FLASH_TIME_1000MS,
	RT5033_FLASH_TIME_MAX,
};

enum rt5033_torch_time
{
	RT5033_TORCH_TIME_262MS,
	RT5033_TORCH_TIME_524MS,
	RT5033_TORCH_TIME_786MS,
	RT5033_TORCH_TIME_1048MS,
	RT5033_TORCH_TIME_1572MS,
	RT5033_TORCH_TIME_2096MS,
	RT5033_TORCH_TIME_2620MS,
	RT5033_TORCH_TIME_3114MS,
	RT5033_TORCH_TIME_4193MS,
	RT5033_TORCH_TIME_5242MS,
	RT5033_TORCH_TIME_6291MS,
	RT5033_TORCH_TIME_7340MS,
	RT5033_TORCH_TIME_9437MS,
	RT5033_TORCH_TIME_11534MS,
	RT5033_TORCH_TIME_13631MS,
	RT5033_TORCH_TIME_15728MS,
	RT5033_TORCH_TIME_MAX,
};

enum rt5033_timer_mode
{
	RT5033_TIMER_MODE_ONE_SHOT,
	RT5033_TIMER_MODE_MAX_TIMER,
};

enum rt5033_led_cntrl_mode
{
	RT5033_LED_CTRL_BY_FLASHSTB,
	RT5033_LED_CTRL_BY_I2C,
};

struct rt5033_led
{
	const char			*name;
	const char			*default_trigger;
	int				id;
	int				timer;
	int				brightness;
	enum rt5033_timer_mode	timer_mode;
	enum rt5033_led_cntrl_mode	cntrl_mode;
};

struct rt5033_led_platform_data
{
	int num_leds;
	struct rt5033_led leds[RT5033_LED_MAX];
};

#define RT5033_STROBE_CURRENT(mA) (((mA - 50) / 25) & 0x1f)
#define RT5033_TIMEOUT_LEVEL(mA) (((mA - 50) / 25) & 0x07)
#define RT5033_STROBE_TIMEOUT(ms) (((ms - 64) / 32) & 0x3f)
#define RT5033_TORCH_CURRENT(mA) (((mA * 10 - 125) / 125) & 0x0f)
#define RT5033_LV_PROTECTION(mV) (((mV-2900) / 100) & 0x07)
#define RT5033_MID_REGULATION_LEVEL(mV) (((mV - 3625) / 25) & 0x3f)

typedef struct rt5033_fled_platform_data {
    unsigned int fled1_en : 1;
    unsigned int fled2_en : 1;
    unsigned int fled_mid_track_alive : 1;
    unsigned int fled_mid_auto_track_en : 1;
    unsigned int fled_timeout_current_level;
    unsigned int fled_strobe_current;
    unsigned int fled_strobe_timeout;
    unsigned int fled_torch_current;
    unsigned int fled_lv_protection;
    unsigned int fled_mid_level;
} rt5033_fled_platform_data_t;

#define RT5033_FLED_RESET           0x25

#define RT5033_FLED_FUNCTION1       0x21
#define RT5033_FLED_FUNCTION2       0x22
#define RT5033_FLED_STROBE_CONTROL1 0x23
#define RT5033_FLED_STROBE_CONTROL2 0x24
#define RT5033_FLED_CONTROL1        0x25
#define RT5033_FLED_CONTROL2        0x26
#define RT5033_FLED_CONTROL3        0x27
#define RT5033_FLED_CONTROL4        0x28
#define RT5033_FLED_CONTROL5        0x29

#define RT5033_CHG_FLED_CTRL        0x67

#endif /*LINUX_LIGHTS_RT5033_FLED_H*/
