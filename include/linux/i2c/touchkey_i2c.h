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

#ifndef __LINUX_CYPRESS_TOUCHKEY_H
#define __LINUX_CYPRESS_TOUCHKEY_H
extern struct class *sec_class;

#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


/* Touchkey Register */
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
#define CYPRESS_IDAC_MENU		0x0D
#define CYPRESS_IDAC_BACK		0x0E
#define CYPRESS_COMPIDAC_MENU		0x11
#define CYPRESS_COMPIDAC_BACK		0x12
#define CYPRESS_DIFF_MENU		0x16
#define CYPRESS_DIFF_BACK		0x18
#define CYPRESS_RAW_DATA_MENU		0x1E
#define CYPRESS_RAW_DATA_BACK		0x20
#define CYPRESS_BASE_DATA_MENU		0x26
#define CYPRESS_BASE_DATA_BACK		0x28
#define CYPRESS_CRC			0x30
#define CYPRESS_DATA_UPDATE		0X40
#define USE_OPEN_CLOSE

#ifndef CONFIG_SEC_KLEOS_PROJECT
#define CRC_CHECK_INTERNAL
#endif

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

/*
#define TOUCHKEY_LOG(k, v) dev_notice(&info->client->dev, "key[%d] %d\n", k, v);
#define FUNC_CALLED dev_notice(&info->client->dev, "%s: called.\n", __func__);
*/
#define NUM_OF_RETRY_UPDATE	5
/*#define NUM_OF_KEY		4*/

#undef DO_NOT_USE_FUNC_PARAM


#undef CONFIG_GLOVE_TOUCH
#ifdef CONFIG_GLOVE_TOUCH
#define	TKEY_GLOVE_DWORK_TIME	300
#endif

#define CHECK_TKEY_IC_STATUS
#ifdef CHECK_TKEY_IC_STATUS
#define TKEY_CHECK_STATUS_TIME		3000
#endif

/* Flip cover*/
#undef TKEY_FLIP_MODE
/* 1MM stylus */
#undef TKEY_1MM_MODE

//#define TK_INFORM_CHARGER
#if defined(CONFIG_KEYBOARD_CYPRESS_TOUCHKEY) && !defined(CONFIG_EXTCON)
#undef TK_INFORM_CHARGER
#else
#undef TK_INFORM_CHARGER
#endif

#define CYPRESS_55_IC_MASK	0x20
#define CYPRESS_65_IC_MASK	0x04

#define NUM_OF_KEY		4

enum {
	CORERIVER_TOUCHKEY,
	CYPRESS_TOUCHKEY,
};

#ifdef TK_INFORM_CHARGER
struct touchkey_callbacks {
	void (*inform_charger)(struct touchkey_callbacks *, bool);
};
#endif

enum {
	UPDATE_NONE,
	DOWNLOADING,
	UPDATE_FAIL,
	UPDATE_PASS,
};

#define USE_TKEY_UPDATE_WORK
#define TKEY_REQUEST_FW_UPDATE

#ifdef TKEY_REQUEST_FW_UPDATE
#define TKEY_FW_BUILTIN_PATH	"tkey"
#define TKEY_FW_IN_SDCARD_PATH	"/sdcard/"

#ifdef CONFIG_SEC_KLEOS_PROJECT
#define TKEY_CYPRESS_FW_NAME	"kleos_cypress_tkey"
#else
#define TKEY_CYPRESS_FW_NAME	"heat_cypress_tkey"
#endif

enum {
	FW_BUILT_IN = 0,
	FW_IN_SDCARD,
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

struct cypress_touchkey_platform_data {
	unsigned	gpio_led_en;
	u32	touchkey_keycode[4];
	int	keycodes_size;
	void	(*power_onoff) (int);
	bool	skip_fw_update;
	bool	touchkey_order;
	void	(*register_cb)(void *);
	bool i2c_pull_up;
	bool vcc_flag;
	bool ldo_flag;
	int gpio_int;
	u32 irq_gpio_flags;
	int gpio_sda;
	u32 sda_gpio_flags;
	int gpio_scl;
	u32 scl_gpio_flags;
	int gpio_touchkey_id;
	u32	gpio_touchkey_id_flags;
	int vdd_led;
	int vcc_en;
};

struct cypress_touchkey_info {
	struct i2c_client			*client;
	struct cypress_touchkey_platform_data	*pdata;
	struct input_dev			*input_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend			early_suspend;
#endif
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	char			phys[32];
	unsigned char			keycode[NUM_OF_KEY];
	u8			sensitivity[NUM_OF_KEY];
	int			irq;
	void (*power_onoff)(int);
	u8			touchkey_update_status;
	u8			ic_vendor;
	struct regulator *vcc_en;
	struct regulator *vdd_led;
#ifdef CONFIG_GLOVE_TOUCH
	struct delayed_work	glove_work;
	struct mutex	tkey_glove_lock;
	int glove_value;
#endif
#ifdef USE_TKEY_UPDATE_WORK
	struct workqueue_struct	*fw_wq;
	struct work_struct		update_work;
#endif
	bool is_powering_on;
	bool enabled;
	bool done_ta_setting;
	bool irq_enable;

#ifdef TKEY_FLIP_MODE
	bool enabled_flip;
#endif
#ifdef TKEY_1MM_MODE
	bool enabled_1mm;
#endif

#ifdef TKEY_BOOSTER
	struct input_booster *tkey_booster;
#endif

#ifdef TK_INFORM_CHARGER
	struct touchkey_callbacks callbacks;
	bool charging_mode;
#endif
#ifdef TKEY_REQUEST_FW_UPDATE
	const struct firmware		*fw;
	struct fw_image	*fw_img;
	u8	cur_fw_path;
	const u8 *fw_data;
	int fw_size;
#endif
	int (*power)(struct device *dev, bool on);
	int	src_fw_ver;
	int	src_md_ver;
	int	ic_fw_ver;
	int	module_ver;
	int	device_ver;
	u32 fw_id;
	u8	touchkeyid;
	bool	support_fw_update;
	bool	do_checksum;
	struct wake_lock fw_wakelock;
#ifdef CRC_CHECK_INTERNAL
	int crc;
	int fw_crc;
#endif
#ifdef CHECK_TKEY_IC_STATUS
	struct delayed_work status_work;
	int status_count;
#endif
};

#ifdef TK_INFORM_CHARGER
void touchkey_charger_infom(bool en);
#endif

#endif /* __LINUX_CYPRESS_TOUCHKEY_H */
