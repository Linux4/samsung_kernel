/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 46000 $
 * $Date: 2019-06-12 14:25:52 +0800 (週三, 12 六月 2019) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef 	_LINUX_NVT_TOUCH_H
#define		_LINUX_NVT_TOUCH_H

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/syscalls.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>

#include <linux/input/sec_cmd.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"

#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif

#ifdef CONFIG_HW_INFO
#include <linux/hw_info.h>
#endif

#define NVT_DEBUG 1

#define NVT_USE_ENABLE_NODE 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_FALLING


//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"

#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"


//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 720
#define TOUCH_DEFAULT_MAX_HEIGHT 1600
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 1
#define NOVA_POWER_SOURCE_CUST_EN 1
#define PROXIMITY_FUNCTION 0
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1

#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65

#define USB_DETECT_IN 1
#define USB_DETECT_OUT 0
#define CMD_CHARGER_ON	(0x53)
#define CMD_CHARGER_OFF (0x51)

#if NOVA_POWER_SOURCE_CUST_EN
#define LCM_LAB_MIN_UV                      5500000
#define LCM_LAB_MAX_UV                      6000000
#define LCM_IBB_MIN_UV                      5500000
#define LCM_IBB_MAX_UV                      6000000
#endif

#define NVT_TS_DEFAULT_UMS_FW		"/sdcard/firmware/tsp/novatek_ts_fw.bin"

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 1
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#if PROXIMITY_FUNCTION
struct nvt_ts_event_proximity {
	u8 reserved_1:3;
	u8 id:5;
	u8 data_page;
	u8 status;
	u8 reserved_2;
} __attribute__ ((packed));
#endif

enum TP_FW_UPGRADE_STATUS {
	FW_STAT_INIT = 0,
	FW_UPDATING = 90,
	FW_UPDATE_PASS = 100,
	FW_UPDATE_FAIL = -1
};

struct nvt_ts_event_coord {
	u8 status:2;
	u8 reserved_1:1;
	u8 id:5;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 area;
	u8 pressure_7_0;
} __attribute__ ((packed));

struct nvt_ts_coord {
	u16 x;
	u16 y;
	u16 p;
	u16 p_x;
	u16 p_y;
	u8 area;
	u8 status;
	u8 p_status;
	bool press;
	bool p_press;
	int move_count;
};

struct nvt_ts_platdata {
	int irq_gpio;
	u32 irq_flags;
	u8 x_num;
	u8 y_num;
	u16 abs_x_max;
	u16 abs_y_max;
	u32 area_indicator;
	u32 area_navigation;
	u32 area_edge;
	u8 max_touch_num;
/*	u8 bringup;
	const char *firmware_name;
*/	u32 open_test_spec[2];
	u32 short_test_spec[2];
	int diff_test_frame;
/*	int item_version;
*/};

struct nvt_ts_data {
	struct spi_device *client;
	struct nvt_ts_platdata *platdata;
	struct nvt_ts_coord coords[TOUCH_MAX_FINGER_NUM];
	u8 touch_count;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
	char md_name[32];
	char md_nomalfw_rq_name[32];
	char md_mpfw_rq_name[32];
#if defined(CONFIG_FB)
#ifdef _MSM_DRM_NOTIFY_H_
	struct notifier_block drm_notif;
#else
	struct notifier_block fb_notif;
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	uint8_t fw_ver;
//	uint8_t x_num;
//	uint8_t y_num;
//	uint16_t abs_x_max;
//	uint16_t abs_y_max;
//	uint8_t max_touch_num;
	uint8_t max_button_num;
	uint32_t int_trigger_type;
//	int32_t irq_gpio;
//	uint32_t irq_flags;
	int32_t reset_gpio;
	uint32_t reset_flags;
	struct mutex lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t carrier_system;
	uint8_t hw_crc;
	uint16_t nvt_pid;
	uint8_t rbuf[1025];
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	bool tp_is_enabled;
	int fw_update_stat;
	u8 fw_ver_ic[4];
	u8 fw_ver_ic_bar;
	u8 fw_ver_bin[4];
	u8 fw_ver_bin_bar;
	struct sec_cmd_data sec;
	volatile int power_status;
	u16 sec_function;
	bool gesture_mode;
#if NOVA_POWER_SOURCE_CUST_EN
	struct regulator *lcm_lab;
	struct regulator *lcm_ibb;
	atomic_t lcm_lab_power;
	atomic_t lcm_ibb_power;
#endif
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX
    struct mtk_chip_config spi_ctrl;
#endif
	struct notifier_block charger_notif;
	struct workqueue_struct *nvt_charger_notify_wq;
	struct work_struct charger_notify_work;
	int usb_plug_status;
};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
    EVENT_MAP_HOST_CMD                      = 0x50,
    EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE   = 0x51,
    EVENT_MAP_FUNCT_STATE                   = 0x5C,
    EVENT_MAP_RESET_COMPLETE                = 0x60,
#if PROXIMITY_FUNCTION
	EVENT_MAP_PROXIMITY_SUM					= 0x6A,	// 2 bytes
#endif
    EVENT_MAP_FWINFO                        = 0x78,
	EVENT_MAP_PANEL                         = 0x8F,
	EVENT_MAP_PROJECTID                     = 0x9A,
	EVENT_MAP_SENSITIVITY_DIFF              = 0x9D,	// 18 bytes
} SPI_EVENT_MAP;

typedef enum {
	TEST_RESULT_PASS	= 0x00,
	TEST_RESULT_FAIL = 0x01,
} TEST_RESULT;

enum {
	POWER_OFF_STATUS = 0,
	POWER_ON_STATUS
};

enum NVT_TP_MODEL {
	MODEL_DEFAULT = 0,
	MODEL_TRULY_TRULY_PID7211,
	/*HS50 code for SR-QL3095-01-755 by weiqiang at 2020/10/02 start*/
	MODEL_TRULY_TRULY_PID721A
	/*HS50 code for SR-QL3095-01-755 by weiqiang at 2020/10/02 end*/
};

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(63*1024)
#define BLOCK_64KB_NUM 4

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

//---extern structures---
extern struct nvt_ts_data *ts;

//---extern functions---
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len);
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len);
void nvt_bootloader_reset(void);
void nvt_eng_reset(void);
void nvt_sw_reset(void);
void nvt_sw_reset_idle(void);
void nvt_boot_ready(void);
void nvt_bld_crc_enable(void);
void nvt_fw_crc_enable(void);
int32_t nvt_update_firmware(char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#endif /* _LINUX_NVT_TOUCH_H */
