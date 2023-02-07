/* abov_touchkey.h -- Linux driver for abov chip as touchkey
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Junkyeong Kim <jk0430.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#ifndef __LINUX_ABOV_TOUCHKEY_H
#define __LINUX_ABOV_TOUCHKEY_H

#define ABOV_TK_NAME "abov-touchkey"
#define ABOV_ID 0x40
#define MAX_KEY_NUM	        3

#define SECLOG "[sec_input]"

#define input_dbg(mode, dev, fmt, ...)						\
({										\
	dev_dbg(dev, SECLOG " " fmt, ## __VA_ARGS__);				\
})
#define input_info(mode, dev, fmt, ...)						\
({										\
	dev_info(dev, SECLOG " " fmt, ## __VA_ARGS__);				\
})
#define input_err(mode, dev, fmt, ...)						\
({										\
	dev_err(dev, SECLOG " " fmt, ## __VA_ARGS__);				\
})
/* registers */
#define ABOV_BTNSTATUS_01		0x00
#define ABOV_BTNSTATUS_02		0x01

#define ABOV_FW_VER			0x02
//#define ABOV_COMMAND		0x03
#define ABOV_THRESHOLD		0x02
//#define ABOV_SENS			0x05
//#define ABOV_SETIDAC		0x06
#define ABOV_BTNSTATUS_NEW	0x07
#define ABOV_LED_RECENT		0x08		//LED Dimming (0x01~0x1F)
#define ABOV_LED_BACK		0x09		//LED Dimming (0x01~0x1F)
#define ABOV_DIFFDATA		0x2E
#define ABOV_RAWDATA		0x22
#define ABOV_VENDORID		0x12
#define ABOV_TSPTA			0x13
#define ABOV_GLOVE			0x13
#define ABOV_KEYBOARD		0x13
#define ABOV_MODEL_NUMBER	0x03		//Model No.
#define ABOV_FLIP			0x15
#define ABOV_SW_RESET		0x1A

/* command */
#define CMD_LED_ON			0x10
#define CMD_LED_OFF			0x20

#define CMD_SAR_TOTALCAP	0x16
#define CMD_SAR_MODE		0x17
#define CMD_SAR_TOTALCAP_READ		0x18
#define CMD_SAR_ENABLE		0x24
#define CMD_SAR_SENSING		0x25
#define CMD_SAR_NOISE_THRESHOLD	0x26
#define CMD_SAR_BASELINE	0x28
#define CMD_SAR_DIFFDATA	0x2A
#define CMD_SAR_RAWDATA		0x2E
#define CMD_SAR_THRESHOLD	0x32

#define CMD_DATA_UPDATE		0x40
#define CMD_MODE_CHECK		0x41
#define CMD_LED_CTRL_ON		0x60
#define CMD_LED_CTRL_OFF	0x70
#define CMD_STOP_MODE		0x80
#define CMD_GLOVE_OFF		0x10
#define CMD_GLOVE_ON		0x20
#define CMD_MOBILE_KBD_OFF	0x10
#define CMD_MOBILE_KBD_ON	0x20
#define CMD_FLIP_OFF		0x10
#define CMD_FLIP_ON			0x20
#define CMD_OFF			0x10
#define CMD_ON			0x20

#define ABOV_BOOT_DELAY		45
#define ABOV_RESET_DELAY	250

#define USER_CODE_ADDRESS	0x400

//static struct device *sec_touchkey;

#define TK_FW_PATH_BIN "abov/abov_noble.fw"
#define TK_FW_PATH_SDCARD "/sdcard/abov_fw.bin"

#define I2C_M_WR 0		/* for i2c */

enum {
	BUILT_IN = 0,
	SDCARD,
};

#define ABOV_ISP_FIRMUP_ROUTINE	0

static int touchkey_keycode[] = { 0,
	KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_PLAYPAUSE
};

struct abov_tk_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct device *dev;
	struct abov_touchkey_platform_data *pdata;

	struct mutex lock;
	struct pinctrl *pinctrl;
	struct pinctrl *pinctrl_det;
	struct pinctrl_state *pins_default;

	struct completion resume_done;
	struct wakeup_source *sec_touchkey_ws;
	volatile int power_state;

	const struct firmware *firm_data_bin;
	const u8 *firm_data_ums;
	char phys[32];
	long firm_size;
	int irq;
	u16 keys_s[MAX_KEY_NUM];
	u16 keys_raw[MAX_KEY_NUM];
	int (*power) (bool on);
	void (*input_event)(void *data);
	int touchkey_count;
	u8 fw_update_state;
	u8 fw_ver;
	u8 fw_ver_bin;
	u8 fw_model_number;
	u8 checksum_h;
	u8 checksum_h_bin;
	u8 checksum_l;
	u8 checksum_l_bin;
	bool glovemode;
	bool keyboard_mode;
	bool flip_mode;
	unsigned int key_num;
	int irq_key_count[MAX_KEY_NUM];
	bool irq_checked;
};


struct abov_touchkey_platform_data {
	unsigned long irq_flag;
	int gpio_int;
	const char *dvdd_vreg_name;	/* regulator name */
	struct regulator *dvdd_vreg;
	void (*input_event) (void *data);
	int (*power) (void *, bool on);
	char *fw_path;
	bool boot_on_ldo;
	int firmup_cmd;
};

enum power_mode {
	SEC_INPUT_STATE_POWER_OFF = 0,
	SEC_INPUT_STATE_LPM,
	SEC_INPUT_STATE_POWER_ON
};

#endif /* LINUX_ABOV_TOUCHKEY_H */
