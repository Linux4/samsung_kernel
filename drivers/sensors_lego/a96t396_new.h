/* a96t396.h -- Linux driver for A96T396 chip as grip sensor
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 * Author: YunJae Hwang <yjz.hwang@samsung.com>
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

#ifndef __LINUX_A96T396_H
#define __LINUX_A96T396_H

//#include <linux/sensor/sensors_core.h>
#define VENDOR_NAME	"ABOV"

#define UNKNOWN_ON  1
#define UNKNOWN_OFF 2

#define NOTI_MODULE_NAME        "grip_notifier"
/* ioctl */
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
struct a96t396_fw_data {
	char fw_data[14500];
	int fw_size;
	int pass;
};
#define SDCARD_FW_LOARDING 10
#define A96T396_IOCTL_MAGIC 254
#define A96T396_SET_FW_DATA \
	_IOWR(A96T396_IOCTL_MAGIC, 0x01, struct a96t396_fw_data)
#endif

enum ic_num {
	MAIN_GRIP = 0,
	SUB_GRIP,
	SUB2_GRIP,
	WIFI_GRIP,
	GRIP_MAX_CNT
};

const char *grip_name[GRIP_MAX_CNT] = {
	"MAIN",
	"SUB",
	"SUB2",
	"WIFI"
};

const char *device_name[GRIP_MAX_CNT] = {
	"A96T3X6",
	"A96T3X6_SUB",
	"A96T3X6_SUB2",
	"A96T3X6_WIFI"
};

const char *module_name[GRIP_MAX_CNT] = {
	"grip_sensor",
	"grip_sensor_sub",
	"grip_sensor_sub2",
	"grip_sensor_wifi"
};

const char *sdcard_fw_path[GRIP_MAX_CNT] = {
	"/sdcard/Firmware/Grip/abov_fw.bin",
	"/sdcard/Firmware/GripSub/abov_fw.bin",
	"/sdcard/Firmware/GripSub2/abov_fw.bin",
	"/sdcard/Firmware/GripWifi/abov_fw.bin"
};

/* registers */
#define REG_FW_VER                      0x02
#define REG_BTNSTATUS                   0x00
#define REG_SAR_TOTALCAP_READ           0x10
#define REG_SAR_TOTALCAP_READ_2CH       0x12
#define REG_VENDORID                    0x05
#define REG_TSPTA                       0x08
#define REG_MODEL_NO                    0x04
#define REG_SW_RESET                    0x06
#define REG_SAR_ENABLE                  0x09
#define REG_SAR_NOISE_THRESHOLD         0x2A
#define REG_SAR_NOISE_THRESHOLD_2CH     0x2C
#define REG_SAR_THRESHOLD               0x22
#define REG_SAR_THRESHOLD_2CH           0x24
#define REG_SAR_RELEASE_THRESHOLD       0x26
#define REG_SAR_RELEASE_THRESHOLD_2CH   0x28
#define REG_SAR_RAWDATA                 0x1A
#define REG_SAR_RAWDATA_2CH             0x1C
#define REG_SAR_BASELINE                0x1E
#define REG_SAR_BASELINE_2CH            0x20
#define REG_SAR_DIFFDATA                0x16
#define REG_SAR_DIFFDATA_D_2CH          0x18
#define REG_REF_CAP			0x14
#define REG_GAINDATA		0x2E
#ifdef CONFIG_SENSORS_A96T396_2CH
#define REG_GAINDATA_2CH	0x2F
#endif

#define REG_REF_GAINDATA	0x30
#define REG_GRIP_ALWAYS_ACTIVE	0x07

/* command */
#define CMD_ON			0x20
#define CMD_OFF			0x10
#define GRIP_ALWAYS_ACTIVE_ENABLE	0x0
#define GRIP_ALWAYS_ACTIVE_DISABLE	0x1
#define CMD_SW_RESET		0x10
#define BOOT_DELAY		45000
#define RESET_DELAY		150000
#define FLASH_DELAY		1400000
#define FLASH_MODE		0x18
#define CRC_FAIL		0
#define CRC_PASS		1

#define TK_FW_PATH_BIN		"abov/abov_noble.fw"
#define TK_FW_PATH_SDCARD	"/sdcard/Firmware/Grip/abov_fw.bin"
/* depends on FW address configuration */
#define USER_CODE_ADDRESS	0x400
#define I2C_M_WR 0		/* for i2c */
#define GRIP_LOG_TIME	15 /* 30s */
#define FIRMWARE_VENDOR_CALL_CNT 3
#define TEST_FIRMWARE_DETECT_VER 0xa0


enum {
	BUILT_IN = 0,
	SDCARD,
};

enum {
	OFF = 0,
	ON,
};

#define GRIP_ERR(fmt, ...) pr_err("[GRIP_%s] %s: "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)
#define GRIP_INFO(fmt, ...) pr_info("[GRIP_%s] %s: "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)
#define GRIP_WARN(fmt, ...) pr_warn("[GRIP_%s] %s: "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)

#ifndef CONFIG_SENSORS_CORE_AP
extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device *dev, void *drvdata,
		struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
		struct device_attribute *attributes[]);
#endif

#endif /* LINUX_A96T396_H */
