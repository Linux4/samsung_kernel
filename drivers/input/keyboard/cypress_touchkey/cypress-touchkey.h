/*
 * cypress_touchkey.h - Platform data for cypress touchkey driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __T_CYPRESS_TOUCHKEY_H
#define __T_CYPRESS_TOUCHKEY_H

#ifdef CONFIG_DRV_SAMSUNG
extern struct class *sec_class;
#define CYPRESS_SYSFS_DEVT_NUM	10
#endif

#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif

#include <linux/kernel.h>
#include <asm/unaligned.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/qpnp/pin.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/err.h>

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

#if defined(CONFIG_SEC_LUCKYQLTE_PROJECT)
#define USE_SENSITIVITY_OUTER

#else
#undef USE_SENSITIVITY_OUTER
#endif

#define USE_TKEY_UPDATE_WORK
#define TKEY_REQUEST_FW_UPDATE

#define CYPRESS_GEN			0X00
#define CYPRESS_OP_MODE			0X01
#define CYPRESS_SP_REG			0X02
#define CYPRESS_BUTTON_STATUS		0X03
#define CYPRESS_FW_VER			0X04
#define CYPRESS_MODULE_VER		0X05
#define CYPRESS_DEVICE_VER		0X06
#define CYPRESS_STATUS_FLAG		0X07
#define CYPRESS_THRESHOLD		0X09
#define CYPRESS_THRESHOLD2		0X0A

#ifdef USE_SENSITIVITY_OUTER
#define CYPRESS_IDAC_MENU_OUTER		0x0D
#define CYPRESS_IDAC_MENU_INNER		0x0E
#define CYPRESS_IDAC_BACK_OUTER		0x0F
#define CYPRESS_IDAC_BACK_INNER		0x10
#else
#define CYPRESS_IDAC_MENU		0x0D
#define CYPRESS_IDAC_BACK		0x0E
#endif

#define CYPRESS_COMPIDAC_MENU		0x11
#define CYPRESS_COMPIDAC_BACK		0x12

#ifdef USE_SENSITIVITY_OUTER
#define CYPRESS_DIFF_MENU_OUTER		0x16
#define CYPRESS_DIFF_MENU_INNER		0x18
#define CYPRESS_DIFF_BACK_OUTER		0x1A
#define CYPRESS_DIFF_BACK_INNER		0x1C
#define CYPRESS_RAW_DATA_MENU_OUTER	0x1E
#define CYPRESS_RAW_DATA_MENU_INNER	0x20
#define CYPRESS_RAW_DATA_BACK_OUTER	0x22
#define CYPRESS_RAW_DATA_BACK_INNER	0x24
#else
#define CYPRESS_DIFF_MENU		0x16
#define CYPRESS_DIFF_BACK		0x18
#define CYPRESS_RAW_DATA_MENU		0x1E
#define CYPRESS_RAW_DATA_BACK		0x20
#endif

#define CYPRESS_BASE_DATA_MENU		0x26
#define CYPRESS_BASE_DATA_BACK		0x28
#define CYPRESS_CRC			0x30
#define CYPRESS_DATA_UPDATE		0X40
#define USE_OPEN_CLOSE
#define CRC_CHECK_INTERNAL

/* OP MODE CMD */
#define TK_BIT_CMD_LEDCONTROL	0x40    /* Owner for LED on/off control (0:host / 1:TouchIC) */
#define TK_BIT_CMD_INSPECTION	0x20    /* Inspection mode */
#define TK_BIT_CMD_1MM		0x10    /* 1mm stylus mode */
#define TK_BIT_CMD_FLIP		0x08    /* flip mode */
#define TK_BIT_CMD_GLOVE	0x04    /* glove mode */
#define TK_BIT_CMD_TA_ON	0x02    /* Ta mode */
#define TK_BIT_CMD_REGULAR	0x01    /* regular mode = normal mode */

#define TK_BIT_WRITE_CONFIRM	0xAA
#define TK_BIT_EXIT_CONFIRM	0xBB

/* STATUS FLAG */
#define TK_BIT_LEDCONTROL	0x40    /* Owner for LED on/off control (0:host / 1:TouchIC) */
#define TK_BIT_1MM		0x20    /* 1mm stylus mode */
#define TK_BIT_FLIP		0x10    /* flip mode */
#define TK_BIT_GLOVE		0x08    /* glove mode */
#define TK_BIT_TA_ON		0x04    /* Ta mode */
#define TK_BIT_REGULAR		0x02    /* regular mode = normal mode */
#define TK_BIT_LED_STATUS	0x01    /* LED status */

/* bit masks*/
#define PRESS_BIT_MASK		0X08
#define KEYCODE_BIT_MASK	0X07

#define TK_CMD_LED_ON		0x10
#define TK_CMD_LED_OFF		0x20

#define NUM_OF_RETRY_UPDATE	5

#define CONFIG_GLOVE_TOUCH
#ifdef CONFIG_GLOVE_TOUCH
#define	TKEY_GLOVE_DWORK_TIME	300
#endif

#define CHECH_TKEY_IC_STATUS
#ifdef CHECH_TKEY_IC_STATUS
#define TKEY_CHECK_STATUS_TIME	3000
#define TK_CHECK_STATUS_REGISTER	0x32
#define TKEY_STATUS_CHECK_FW_VER	0x0C
#endif

/* Flip cover*/
#define TKEY_FLIP_MODE
/* 1MM stylus */
#define TKEY_1MM_MODE

#define NUM_OF_KEY		4

enum {
	UPDATE_NONE,
	DOWNLOADING,
	UPDATE_FAIL,
	UPDATE_PASS,
};

#ifdef TKEY_REQUEST_FW_UPDATE
#define TKEY_FW_BUILTIN_PATH	"tkey"
#define TKEY_FW_IN_SDCARD_PATH	"/sdcard/"
#define TKEY_FW_FFU_PATH	"ffu_tk.bin"

#if defined(CONFIG_SEC_MSM8976LTE_PROJECT)
#define TKEY_CYPRESS_FW_NAME	"zerof_cypress_tkey"
#else
#define TKEY_CYPRESS_FW_NAME	"cypress_tkey"
#endif

enum {
	FW_BUILT_IN = 0,
	FW_IN_SDCARD,
	FW_FFU,
};

struct fw_image {
	u8 hdr_ver;
	u8 hdr_len;
	u16 first_fw_ver;
	u16 second_fw_ver;
	u16 third_ver;
	u32 fw_len;
	u16 checksum;
	u16 alignment_dummy;
	u8 data[0];
} __attribute__ ((packed));
#else

#define BIN_FW_VERSION	0
#endif

enum cypress_pm_gpio_state {
	CYPRESS_GPIO_STATE_WAKE = 0,
	CYPRESS_GPIO_STATE_SLEEP,
};

struct touchkey_device_tree_data {
	u32	touchkey_keycode[4];
	int	keycodes_size;
	int gpio_int;
	u32 irq_gpio_flags;
	int gpio_sda;
	u32 sda_gpio_flags;
	int gpio_scl;
	u32 scl_gpio_flags;
	int gpio_sub_det;
	u32 sub_det_gpio_flags;
	u32 regulator_boot_on;
	int bringup;
};

struct cypress_touchkey_info {
	struct i2c_client *client;
	struct touchkey_device_tree_data *pdata;
	struct input_dev			*input_dev;
	char			phys[32];
	unsigned char		keycode[NUM_OF_KEY];
	int			irq;
	u8			touchkey_update_status;
	struct mutex i2c_mutex;
	struct regulator	*vcc_en;
	struct regulator	*touch_3p3;
#ifdef CONFIG_GLOVE_TOUCH
	struct delayed_work	glove_work;
	struct mutex		tkey_glove_lock;
	int			glove_value;
#endif
#ifdef USE_TKEY_UPDATE_WORK
	struct workqueue_struct	*fw_wq;
	struct work_struct	update_work;
#endif
	struct pinctrl		*ts_pinctrl;
	struct pinctrl_state	*gpio_state_active;
	struct pinctrl_state	*gpio_state_suspend;
	bool is_powering_on;
	bool enabled;

	bool irq_enable;
	bool init_done;
#ifdef TKEY_FLIP_MODE
	bool enabled_flip;
#endif
#ifdef CHECH_TKEY_IC_STATUS
	struct delayed_work status_work;
	int status_count;
	char status_value;
	char status_value_old;
#endif

#ifdef TKEY_BOOSTER
	struct input_booster *tkey_booster;
#endif

#ifdef TKEY_REQUEST_FW_UPDATE
	const struct firmware	*fw;
	struct fw_image		*fw_img;
	u8			cur_fw_path;
#endif
	int (*power)(struct device *dev, bool on);
	int	src_fw_ver;
	int	src_md_ver;
	int	ic_fw_ver;
	int	module_ver;
	int	device_ver;
	bool	support_fw_update;
	struct wake_lock fw_wakelock;
#ifdef CRC_CHECK_INTERNAL
	int crc;
#endif
	int led_status;
	int led_cmd_reversed;

/* TEMP CODE */
	struct delayed_work reboot_work;

};

#endif /* __T_CYPRESS_TOUCHKEY_H */
