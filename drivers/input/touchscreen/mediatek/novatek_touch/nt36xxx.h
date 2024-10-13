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
#include <linux/platform_device.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"
/*Tab A7 lite_U code for SR-AX3565U-01-4  by zhengkunbang at 20230807 start*/
#include "hal_kpd.h"
/*Tab A7 lite_U code for SR-AX3565U-01-4  by zhengkunbang at 20230807 end*/
#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943

//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
//#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_FALLING

#define NVT_GPIO_AS_INT(pin) tpd_gpio_as_int(pin)
#define NVT_GPIO_OUTPUT(pin, level) tpd_gpio_output(pin, level)

//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"

//---Input device info.---
//#define NVT_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_TS_NAME "sec_touchscreen"
#define NVT_PEN_NAME "sec_capacitivepen"


//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 800
#define TOUCH_DEFAULT_MAX_HEIGHT 1340
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000
//---for Pen---
#define PEN_PRESSURE_MAX (4095)
#define PEN_DISTANCE_MAX (1)
#define PEN_TILT_MIN (-60)
#define PEN_TILT_MAX (60)

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

// If sec relative function is not present, turn SEC_2_NVT_DEBUG on
// Or turn it off
#define SEC_2_NVT_DEBUG 1
#define NVT_DEBUG 1

#if SEC_2_NVT_DEBUG
#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)

#ifdef input_info
#undef input_info
#endif
#ifdef input_err
#undef input_err
#endif

#define input_info(en, dev_ptr,fmt, args...)   NVT_LOG(fmt, ##args)
#define input_err(en, dev_ptr,fmt, args...)   NVT_ERR(fmt, ##args)
#endif

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define LCD_SETTING 0
#define PROXIMITY_FUNCTION 0
#define PROXIMITY_CUSTOM_REPORT 0	//relative to proximity hover event
#define SEC_TOUCH_CMD 1
/*TabA7 Lite code for SR-AX3565-01-740 by fengzhigang at 20210126 start*/
#define SEC_CUSTOM_REPORT 1	//relative to palm/metal_jig
/*TabA7 Lite code for SR-AX3565-01-740 by fengzhigang at 20210126 end*/
#define SEC_SYSFS 0

#if SEC_TOUCH_CMD
#include <linux/input/sec_cmd.h>
#endif

//---Customized Test---
#define LPWG_TEST 0
#if LPWG_TEST
#define FDM_TEST  1	//Depend on LPWG_TEST
#else
#define FDM_TEST  0	//Depend on LPWG_TEST
#endif

extern const uint16_t gesture_key_array[];

#define BOOT_UPDATE_FIRMWARE 1
//#define BOOT_UPDATE_FIRMWARE_NAME "tsp_novatek/nt36525_a01core_tp.bin"
//#define MP_UPDATE_FIRMWARE_NAME   "tsp_novatek/nt36525_a01core_mp.bin"
#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65
#define CHECK_PEN_DATA_CHECKSUM 0

#define NVT_TS_DEFAULT_UMS_FW		"/sdcard/firmware/tsp/nvt.bin"

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define TOUCH_PRINT_INFO_DWORK_TIME	30000	/* 30s */

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
#if SEC_CUSTOM_REPORT
	u8 palm;
	u8 metal_jig;
#endif
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
#if PROXIMITY_FUNCTION
	bool support_ear_detect;	// if support proximity mode feature
#endif
	bool enable_settings_aot;	// if support aot feature
	u32 open_test_spec[2];
	u32 short_test_spec[2];
	u32 diff_test_frame;
/*TabA7 Lite code for SR-AX3565-01-714 by liupengtao at 20210204 start*/
	char md_name[32];
	char firmware_name[32];
	char firmware_name_mp[32];
/*TabA7 Lite code for SR-AX3565-01-714 by liupengtao at 20210204 end*/
#if (FDM_TEST && LPWG_TEST)
	u32 fdm_x_num;
	u32 fdm_diff_test_frame;
#endif
	int lcd_id;
#if LCD_SETTING
	const char *regulator_lcd_vdd;
	const char *regulator_lcd_reset;
	const char *regulator_lcd_bl;
#endif
};
/*TabA7 Lite code for OT8-2503 by liufurong at 20210210 start*/
struct nvt_ito_data {
	char filepath[128];//const char *filepath;
	char *ito_data;
	uint32_t data_len;
	uint32_t offset;
};

/*TabA7 Lite code for OT8-2503 by liufurong at 20210210 end*/
struct nvt_ts_data {
	struct spi_device *client;
	struct nvt_ts_platdata *platdata;
	struct nvt_ts_coord coords[TOUCH_MAX_FINGER_NUM];
	u8 touch_count;
	struct input_dev *input_dev;
#if PROXIMITY_FUNCTION
	struct input_dev *input_dev_proximity;
#endif
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
	int32_t ic_idx;
//	uint8_t x_num;
//	uint8_t y_num;
//	uint16_t abs_x_max;
//	uint16_t abs_y_max;
//	uint8_t max_touch_num;
	uint8_t max_button_num;
//	uint32_t int_trigger_type;
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
	uint8_t cascade;
	bool pen_support;
	bool wgp_stylus;
	uint8_t x_gang_num;
	uint8_t y_gang_num;
	struct input_dev *pen_input_dev;
	int8_t pen_phys[32];
	u8 fw_ver_ic[4];
	u8 fw_ver_ic_bar;
	u8 fw_ver_bin[4];
	u8 fw_ver_bin_bar;
	volatile int power_status;
	u8 aot_enable;
#if SEC_TOUCH_CMD
	struct sec_cmd_data sec;
	u16 sec_function;
#endif
	bool isUMS;
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX
	struct mtk_chip_config spi_ctrl;
#endif
#if PROXIMITY_FUNCTION
	unsigned int ear_detect_enable;	//proximity mode
	long prox_power_off;	//proximity mode report off(sleep in)
	u8 hover_event;	//virtual_prox
#endif
//	struct delayed_work work_read_info;
	struct delayed_work work_print_info;
	u32 print_info_cnt_open;
	u32 print_info_cnt_release;
	u16 print_info_currnet_mode;
#if LCD_SETTING
	struct regulator *regulator_vdd;
	struct regulator *regulator_lcd_reset;
	struct regulator *regulator_lcd_bl_en;
#endif
	u8 noise_mode;
	/*TabA7 Lite code for OT8-2503 by liufurong at 20210210 start*/
	//struct nvt_ito_data ito_data[8];
	//int ito_item_index;
	struct delayed_work ito_test_wq;
	struct workqueue_struct *ito_wq;
	/*TabA7 Lite code for OT8-2503 by liufurong at 20210210 end*/
	/*TabA7 Lite code for OT8-5211 by gaozhengwei at 20211021 start*/
	struct mutex ito_lock;
	/*TabA7 Lite code for OT8-5211 by gaozhengwei at 20211021 end*/
	struct completion pm_completion;
	bool pm_suspend;
	/*Tab A7 lite_U code for SR-AX3565AU-21  by zhengkunbang at 20230810 start*/
	bool tp_is_enabled;
	/*Tab A7 lite_U code for SR-AX3565AU-21  by zhengkunbang at 20230810 end*/
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
	EVENT_MAP_FWINFO                        = 0x78,
	EVENT_MAP_PANEL                         = 0x8F,
	EVENT_MAP_PROJECTID                     = 0x9A,
	EVENT_MAP_SENSITIVITY_DIFF              = 0x9D,	// 18 bytes
#if PROXIMITY_FUNCTION
	EVENT_MAP_PROXIMITY_SUM 				= 0xB0, // 2 bytes
	EVENT_MAP_PROXIMITY_THD 				= 0xB2, // 2 bytes
#endif
} SPI_EVENT_MAP;

typedef enum {
	TEST_RESULT_PASS = 0x00,
	TEST_RESULT_FAIL = 0x01,
} TEST_RESULT;

enum {
	POWER_OFF_STATUS = 0,
	POWER_LPM_STATUS,
#if PROXIMITY_FUNCTION
	POWER_PROX_STATUS,
#endif
	POWER_ON_STATUS
};
/*TabA7 Lite code for SR-AX3565-01-714 by liupengtao at 20210204 start*/
enum NVT_TP_MODEL {
	MODEL_DEFAULT = 0,
	MODEL_LS_HSD_PID721F,
	MODEL_HLT_MDT_PID7227,
/*TabA7 Lite code for SR-AX3565-01-821 by liupengtao at 20210309 start*/
	MODEL_QUNCHUANG_INX_PID7221,
/*TabA7 Lite code for SR-AX3565-01-821 by liupengtao at 20210309 end*/
	/*TabA7 Lite code for SR-AX3565-01-902 by yuli at 20220119 start*/
	MODEL_LS_HSD_NTFPC_PID721F,
	/*TabA7 Lite code for SR-AX3565-01-902 by yuli at 20220119 end*/
	/*TabA7 Lite code for OT8-5318 by suyurui at 20220217 start*/
	MODEL_TXD_MDT_PID724A,
	/*TabA7 Lite code for OT8-5318 by suyurui at 20220217 end*/
};
/*TabA7 Lite code for SR-AX3565-01-714 by liupengtao at 20210204 end*/
//u8 aot_enable;
//bit control
enum {
	LPM_OFF = 0,
	LPM_DC = 0x01,	//Double Click
	LPM_Spay = 0x02	//Spay, slide up
};

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(63*1024)
#define NVT_READ_LEN		(2*1024)
#define BLOCK_64KB_NUM 4

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

#if defined(CONFIG_EXYNOS_DPU30)
int get_lcd_info(char *arg);
#endif
#if LCD_SETTING
extern unsigned int lcdtype;
#endif

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
void nvt_tx_auto_copy_mode(void);
int32_t nvt_update_firmware(const char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_check_spi_dma_tx_info(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
#if defined(HQ_D85_BUILD) || defined(FTY_TP_GESTURE)
int nvt_ts_mode_switch(struct nvt_ts_data *ts, u8 cmd, bool stored);
#endif

#endif /* _LINUX_NVT_TOUCH_H */
