/*
 * leds-s2mu107.h - Flash-led driver for Samsung S2MU107
 *
 * Copyright (C) 2023 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LEDS_AW36518_FLASH_H__
#define __LEDS_AW36518_FLASH_H__
#include <linux/leds.h>

#ifndef AW36518_DTNAME_I2C
#define AW36518_DTNAME_I2C "awinic,aw36518"
#endif
#define AW36518_NAME "aw36518"

#define AW36518_DRIVER_VERSION "v0.0.6"

#define AW36518_REG_BOOST_CONFIG	(0x07)
#define AW36518_BIT_SOFT_RST_MASK	(~(1<<7))
#define AW36518_BIT_SOFT_RST_ENABLE	(1<<7)
#define AW36518_BIT_SOFT_RST_DISABLE	(0<<7)

/* define registers */
#define AW36518_REG_ENABLE		(0x01)
#define AW36518_MASK_ENABLE_LED1	(0x01)
#define AW36518_DISABLE			(0x00)
#define AW36518_ENABLE_LED1		(0x03)
#define AW36518_ENABLE_LED1_TORCH	(0x0B)
#define AW36518_ENABLE_LED1_FLASH	(0x0F)

#define AW36518_REG_FLASH_LEVEL_LED1	(0x03)
#define AW36518_REG_TORCH_LEVEL_LED1	(0x05)


#define AW36518_REG_TIMING_CONF		(0x08)
#define AW36518_TORCH_RAMP_TIME		(0x10)
#define AW36518_FLASH_TIMEOUT		(0x0A)
#define AW36518_CHIP_STANDBY		(0x80)

#define AW36518_DEFAULT_FLASH_CUR	(800) //mA
#define AW36518_DEFAULT_FLASH_DURATION	(600) //ms
#define AW36518_MAX_FLASH_DURATION	(1600) //ms

#define AW36518_REG_FLAG1					(0x0A)
#define AW36518_BIT_TX_FLAG				(1<<7)
#define AW36518_BIT_VOUT_SHORT_FAULT		(1<<6)
#define AW36518_BIT_LED_SHORT_FAULT_2	(1<<5)
#define AW36518_BIT_LED_SHORT_FAULT_1	(1<<4)
#define AW36518_BIT_CURRENT_LIMIT_FLAG	(1<<3)
#define AW36518_BIT_TSD_FAULT			(1<<2)
#define AW36518_BIT_UVLO_FAULT			(1<<1)
#define AW36518_BIT_FLASH_TIMEOUT_FLAG	(1<<0)

#define AW36518_REG_FLAG2				(0x0B)
#define AW36518_BIT_IVFM_TRIP_FLAG		(1<<2)
#define AW36518_BIT_OVP_FAULT			(1<<1)

#define AW36518_REG_CHIP_VENDOR_ID		(0x25)
#define AW36518_CHIP_VENDOR_ID			(0x04)

#define AW36518_REG_CTRL1				(0x31)
#define AW36518_REG_CTRL2				(0x69)
#define AW36518_LED_STANDBY_MODE_MASK		(~(0X03<<2))
#define AW36518_TORCH_LOWEST_CUR_REG_VAL	(0x00)
#define AW36518_TORCH_HIGTEST_CUR_REG_VAL	(0xff)
#define AW36518_TORCH_DEFAULT_CUR_REG_VAL	(0x82) //125mA
#define AW36518_FLASH_LOWEST_CUR_REG_VAL	(0x00)

#define AW36518_FLASH_FACTORY_CUR_REG_VAL	(0x26) //225mA

#define AW36518_MIN_FLASH_CUR	(0) //mA
#define AW36518_MAX_FLASH_CUR	(1500) //mA
#define AW36518_MIN_TORCH_CUR	(0) //mA
#define AW36518_VENDOR_A_MAX_TORCH_CUR						(386) //mA
#define AW36518_VENDOR_B_MAX_TORCH_CUR						(345) //mA

/* define channel, level */
#define AW36518_CHANNEL_NUM	1
#define AW36518_CHANNEL_CH1	0

#define AW36518_LEVEL_NUM	26
#define AW36518_LEVEL_TORCH	7

#define AW_I2C_RETRIES		5
#define AW_I2C_RETRY_DELAY	2

/* Product Version Information */
enum aw36518_chip_version {
	AW36518_CHIP_A = 1,	/* TSMC	*/
	AW36518_CHIP_B,		/* HHGRACE */
	AW36518_CHIP_ID_MAX,
};

/* aw36518 chip data */
struct aw36518_chip_data {
	struct i2c_client *client;
	//struct aw36518_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
	uint32_t torch_level[5];
	int fen_pin;
	bool used_gpio_ctrl;
	enum aw36518_chip_version chip_version;
	int gpio_request;
	uint32_t pre_flash_cur;
	uint32_t main_flash_cur;
	uint32_t video_torch_cur;
	int flash_time_out_ms;
	int max_torch_cur;	/* torch mode max current. unit: mA */
	struct delayed_work delay_work;
};

/*
 * Off-Mode (To turn off torch and flash)
 * Main-Flash-Mode (For Main-flash during capture)
 * Torch-flash-mode (For Torch during video)
 * pre-flash-mode (For Pre-flash during capture. Same as torch mode but brightness might defer)
 * prepare-flash-mode (To prepare flash and keep it ready to turn ON)
 * close-flash-mode (To reset any settings after flash and camera off)
 */
/* For AW36518 Flash LED */
enum aw36518_fled_mode {
	AW36518_FLED_MODE_OFF = 1,
	AW36518_FLED_MODE_MAIN_FLASH,
	AW36518_FLED_MODE_TORCH_FLASH,
	AW36518_FLED_MODE_PREPARE_FLASH,
	AW36518_FLED_MODE_CLOSE_FLASH,
	AW36518_FLED_MODE_PRE_FLASH,
};

enum {
	FLED_MODE_OFF		= 0x0,
	FLED_MODE_TORCH		= 0x1,
	FLED_MODE_FLASH		= 0x2,
	FLED_MODE_EXTERNAL	= 0x3,// Control by i2c
};

int32_t aw36518_fled_mode_ctrl(int state, uint32_t brightness);
void aw36518_enable_flicker(int curr, bool is_enable);
extern ssize_t aw36518_store(const char *buf);
extern ssize_t aw36518_show(char *buf);
#endif
