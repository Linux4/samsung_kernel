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
#include <linux/input/sec_cmd.h>
#include <linux/regulator/consumer.h>

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

#ifdef CONFIG_SAMSUNG_TUI
#include <linux/input/stui_inf.h>
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif

extern struct device *ptsp;

#define NVT_DEBUG 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_FALLING


//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"
#if 0
#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif

//---Input device info.---
//#define NVT_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_TS_NAME "sec_touchscreen"


//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 720
#define TOUCH_DEFAULT_MAX_HEIGHT 1480
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
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1
//#define BOOT_UPDATE_FIRMWARE_NAME "tsp_novatek/nt36525_a01core_tp.bin"
//#define MP_UPDATE_FIRMWARE_NAME   "tsp_novatek/nt36525_a01core_mp.bin"
#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65

#define TSP_PATH_EXTERNAL_FW		"/sdcard/firmware/tsp/tsp.bin"
#define TSP_PATH_EXTERNAL_FW_SIGNED		"/sdcard/firmware/tsp/tsp_signed.bin"

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define TOUCH_PRINT_INFO_DWORK_TIME	30000	/* 30s */

#define PROXIMITY_FUNCTION	1

#if PROXIMITY_FUNCTION
struct nvt_ts_event_proximity {
	u8 reserved_1:3;
	u8 id:5;
	u8 data_page;
	u8 status;
	u8 reserved_2;
} __attribute__ ((packed));
#endif

struct nvt_ts_event_coord {
	u8 status:3;
	u8 id:5;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 w_major;
	u8 pressure_7_0;
} __attribute__ ((packed));

struct nvt_ts_coord {
	u16 x;
	u16 y;
	u16 p;
	u16 p_x;
	u16 p_y;
	u8 w_major;
	u8 w_minor;
	u8 status;
	u8 p_status;
	bool press;
	bool p_press;
	int move_count;
};

struct nvt_ts_platdata {
	int irq_gpio;
	u32 irq_flags;
	int cs_gpio;
	u8 x_num;
	u8 y_num;
	u16 abs_x_max;
	u16 abs_y_max;
	u32 area_indicator;
	u32 area_navigation;
	u32 area_edge;
	u8 max_touch_num;
	struct pinctrl *pinctrl;
	bool support_ear_detect;
	bool enable_settings_aot;
	bool enable_sysinput_enabled;
	bool prox_lp_scan_enabled;
	bool support_spay;
	const char *firmware_name;
	const char *firmware_name_mp;
	u32 open_test_spec[2];
	u32 short_test_spec[2];
	int diff_test_frame;
	u32 fdm_x_num;
	int lcd_id;
	const char *regulator_lcd_vdd;
	const char *regulator_lcd_reset;
	const char *regulator_lcd_bl;

	u32 resume_lp_delay;
};

struct nvt_ts_data {
	struct spi_device *client;
	struct nvt_ts_platdata *platdata;
	struct sec_ts_plat_data *plat_data;	/* only for tui */
	struct nvt_ts_coord coords[TOUCH_MAX_FINGER_NUM];
	u8 touch_count;
	struct input_dev *input_dev;
	struct input_dev *input_dev_proximity;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
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
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	u8 fw_ver_ic[4];
	u8 fw_ver_ic_bar;
	u8 fw_ver_bin[4];
	u8 fw_ver_bin_bar;
	volatile int power_status;
	u16 sec_function;
	u8 cover_mode_restored;
	bool isUMS;
	u8 lowpower_mode;
	bool ear_detect_force_enable;
	u32 early_resume_cnt;
	u32 resume_cnt;
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX
    struct mtk_chip_config spi_ctrl;
#endif
	struct sec_cmd_data sec;
	unsigned int ear_detect_mode;
	bool ed_reset_flag;
	long prox_power_off;
	u8 hover_event;	//virtual_prox
	bool lcdoff_test;

	u16 landscape_deadzone[2];

//	struct delayed_work work_read_info;
	struct delayed_work work_print_info;
	u32 print_info_cnt_open;
	u32 print_info_cnt_release;
	u16 print_info_currnet_mode;
	struct regulator *regulator_vdd;
	struct regulator *regulator_lcd_reset;
	struct regulator *regulator_lcd_bl_en;

	u8 noise_mode;

	unsigned int scrub_id;
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
	EVENT_MAP_HOST_CMD		= 0x50,
	EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE	= 0x51,
	EVENT_MAP_FUNCT_STATE		= 0x5C,
	EVENT_MAP_RESET_COMPLETE	= 0x60,
	EVENT_MAP_FWINFO			= 0x78,
	EVENT_MAP_PROXIMITY_SCAN	= 0x7F,
	EVENT_MAP_PANEL			= 0x8F,
	EVENT_MAP_PROJECTID		= 0x9A,
	EVENT_MAP_SENSITIVITY_DIFF	= 0x9D,	// 18 bytes
	EVENT_MAP_PROXIMITY_SUM		= 0xB0, // 2 bytes
	EVENT_MAP_PROXIMITY_THD		= 0xB2, // 2 bytes
} SPI_EVENT_MAP;

typedef enum {
	TEST_RESULT_PASS = 0x00,
	TEST_RESULT_FAIL = 0x01,
} TEST_RESULT;

enum {
	POWER_OFF_STATUS = 0,
	POWER_ON_STATUS,
	LP_MODE_STATUS,
	LP_MODE_EXIT
};

enum {
	SERVICE_SHUTDOWN = -1,
	LCD_NONE = 0,
	LCD_OFF,
	LCD_ON,
	LCD_DOZE1,
	LCD_DOZE2
};

enum {
	LCD_EARLY_EVENT = 0,
	LCD_LATE_EVENT
};

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(63*1024)
#define NVT_READ_LEN		(2*1024)
#define BLOCK_64KB_NUM 4

#define DEEP_SLEEP_ENTER		0x11
#define LPWG_ENTER		0x13
#define PROX_SCAN_START	0x08
#define PROX_SCAN_STOP	0x09

#if PROXIMITY_FUNCTION
#define PROXIMITY_ENTER		0x85
#define PROXIMITY_LEAVE		0x86
#define PROXIMITY_INSENSITIVITY	0x00
#define PROXIMITY_NORMAL	0x03
#endif

#define SHOW_NOT_SUPPORT_CMD	0x00

#define NORMAL_MODE		0x00
#define TEST_MODE_1		0x21
#define TEST_MODE_2		0x22
#define MP_MODE_CC		0x41
#define FREQ_HOP_DISABLE	0x66
#define FREQ_HOP_ENABLE		0x65
#define HANDSHAKING_HOST_READY	0xBB

#define CHARGER_PLUG_OFF		0x51
#define CHARGER_PLUG_AC			0x53
#define GLOVE_ENTER				0xB1
#define GLOVE_LEAVE				0xB2
#define HOLSTER_ENTER			0xB5
#define HOLSTER_LEAVE			0xB6
#define EDGE_REJ_VERTICLE_MODE	0xBA
#define EDGE_REJ_LEFT_UP_MODE	0xBB
#define EDGE_REJ_RIGHT_UP_MODE	0xBC
#define BLOCK_AREA_ENTER		0x71
#define BLOCK_AREA_LEAVE		0x72
#define EDGE_AREA_ENTER			0x73 //min:7
#define EDGE_AREA_LEAVE			0x74 //0~6
#define HOLE_AREA_ENTER			0x75 //0~120
#define HOLE_AREA_LEAVE			0x76 //no report
#define SPAY_SWIPE_ENTER		0x77
#define SPAY_SWIPE_LEAVE		0x78
#define DOUBLE_CLICK_ENTER		0x79
#define DOUBLE_CLICK_LEAVE		0x7A
#define SENSITIVITY_ENTER		0x7B
#define SENSITIVITY_LEAVE		0x7C
#define EXTENDED_CUSTOMIZED_CMD	0x7F

typedef enum {
	SET_GRIP_EXECPTION_ZONE = 1,
	SET_GRIP_PORTRAIT_MODE = 2,
	SET_GRIP_LANDSCAPE_MODE = 3,
	SET_TOUCH_DEBOUNCE = 5,	// for SIP
	SET_GAME_MODE = 6,
#if PROXIMITY_FUNCTION
	PROX_SLEEP_IN = 8,
	PROX_SLEEP_OUT = 9,
#endif
} EXTENDED_CUSTOMIZED_CMD_TYPE;

typedef enum {
	DEBOUNCE_NORMAL = 0,
	DEBOUNCE_LOWER = 1,		//to optimize tapping performance for SIP
} TOUCH_DEBOUNCE;

typedef enum {
	GAME_MODE_DISABLE = 0,
	GAME_MODE_ENABLE = 1,
} GAME_MODE;

typedef enum {
	SPONGE_EVENT_TYPE_SPAY			= 0x04,
} SPONGE_EVENT_TYPE;

#define BUS_TRANSFER_LENGTH  256

#define XDATA_SECTOR_SIZE	256

#define CMD_RESULT_WORD_LEN	10

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))


/* function bit combination code */
#define EDGE_REJ_VERTICLE	1
#define EDGE_REJ_LEFT_UP	2
#define EDGE_REJ_RIGHT_UP	3

#define ORIENTATION_0		0
#define ORIENTATION_90		1
#define ORIENTATION_180		2
#define ORIENTATION_270		3

#define OPEN_SHORT_TEST		1
#define CHECK_ONLY_OPEN_TEST	1
#define CHECK_ONLY_SHORT_TEST	2

#define SEC_TS_MODE_SWIPE		(1 << 1)
#define SEC_TS_MODE_DOUBLETAP_TO_WAKEUP	(1 << 5)

typedef enum {
	GLOVE = 1,
	CHARGER,
#ifdef PROXIMITY_FUNCTION
	PROXIMITY = 3,
#endif
	HOLSTER = 4,
	EDGE_REJECT_L = 6,
	EDGE_REJECT_H,
	EDGE_PIXEL,
	HOLE_PIXEL,
	SPAY_SWIPE,
	DOUBLE_CLICK,
	SENSITIVITY,
	BLOCK_AREA,
	FUNCT_MAX,
} FUNCT_BIT;

typedef enum {
	GLOVE_MASK		= 0x0002,	// bit 1
	CHARGER_MASK		= 0x0004,	// bit 2
#ifdef PROXIMITY_FUNCTION
	PROXIMITY_MASK	= 0x0008,	// bit 3
#endif
	HOLSTER_MASK = 0x0010,	//bit 4
	EDGE_REJECT_MASK	= 0x00C0,	// bit [6|7]
	EDGE_PIXEL_MASK		= 0x0100,	// bit 8
	HOLE_PIXEL_MASK		= 0x0200,	// bit 9
	SPAY_SWIPE_MASK		= 0x0400,	// bit 10
	DOUBLE_CLICK_MASK	= 0x0800,	// bit 11
	BLOCK_AREA_MASK		= 0x1000,	// bit 12
	NOISE_MASK			= 0x2000,	// bit 13
	SENSITIVITY_MASK	= 0x4000,	// bit 14
#ifdef PROXIMITY_FUNCTION
	FUNCT_ALL_MASK		= 0x7FDE,
#else
	FUNCT_ALL_MASK		= 0x7FD6,
#endif
} FUNCT_MASK;

enum {
	BUILT_IN = 0,
	UMS,
	NONE,
	SPU,
};


typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

#if defined(CONFIG_EXYNOS_DPU30)
int get_lcd_info(char *arg);
#endif
extern unsigned int lcdtype;

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
int32_t nvt_update_firmware(const char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
int nvt_ts_mode_restore(struct nvt_ts_data *ts);
int nvt_get_fw_info(void);
int nvt_ts_fw_update_from_bin(struct nvt_ts_data *ts);
int nvt_ts_fw_update_from_mp_bin(struct nvt_ts_data *ts, bool is_start);
int nvt_ts_fw_update_from_external(struct nvt_ts_data *ts, const char *file_path);
//int nvt_ts_resume_pd(struct nvt_ts_data *ts);
int nvt_get_checksum(struct nvt_ts_data *ts, u8 *csum_result, u8 csum_size);
int32_t nvt_set_page(uint32_t addr);
#if PROXIMITY_FUNCTION
int set_ear_detect(struct nvt_ts_data *ts, int mode);
int nvt_ts_mode_switch_extened(struct nvt_ts_data *ts, u8 *cmd, u8 len, bool print_log);
#endif

#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

extern int smcdsd_fb_register_client(struct notifier_block *nb);
extern int smcdsd_fb_unregister_client(struct notifier_block *nb);

void nvt_ts_early_resume(struct device *dev);
int32_t nvt_ts_resume(struct device *dev);
int32_t nvt_ts_suspend(struct device *dev);
void nvt_ts_shutdown(struct spi_device *client);

#endif /* _LINUX_NVT_TOUCH_H */
