/*
 * Copyright (C) 2010 - 2022 Novatek, Inc.
 *
 * $Revision: 107367 $
 * $Date: 2022-10-26 08:30:52 +0800 (週三, 26 十月 2022) $
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
#ifndef _LINUX_NVT_TOUCH_H
#define _LINUX_NVT_TOUCH_H

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/version.h>
/*Spinel code for charging by wangxin77 at 2023/03/06 start*/
#include <linux/power_supply.h>
/*Spinel code for charging by wangxin77 at 2023/03/06 end*/
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx_mem_map.h"

/*Spinel code for control pen state by zhangyd22 at 2023/04/04 start*/
//#include "../../../nfc/ctn730/ctn730.h"
/*Spinel code for control pen state by zhangyd22 at 2023/04/04 end*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif

#define NVT_PEN_RAW 1
#if NVT_PEN_RAW
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#endif

#define NVT_DEBUG 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_RISING

//---bus transfer length---
#define BUS_TRANSFER_LENGTH  256

//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"

#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_PEN_NAME "NVTCapacitivePen"

//---Touch info.---
/* Spinel code for OSPINEL-2739 by zhangyd22 at 2023/4/12 start */
#define TOUCH_DEFAULT_MAX_WIDTH 1840
#define TOUCH_DEFAULT_MAX_HEIGHT 2944

/* Spinel code for OSPINEL-2739 by zhangyd22 at 2023/4/12 end */
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
#define NVT_TOUCH_SUPPORT_HW_RST 1

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define NVT_SAVE_TEST_DATA_IN_FILE 0 //changed 1 to 0,closed it because kernel-6.1 not support mm_segment_t
#define MT_PROTOCOL_B 1
#define NVT_CUST_PROC_CMD 1
#define NVT_EDGE_REJECT 1
#define NVT_EDGE_GRID_ZONE 1
#define NVT_PALM_MODE 1
#define NVT_SUPPORT_PEN 1
#define WAKEUP_GESTURE 1
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 start */
#define NVT_DPR_SWITCH 1
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 end */
/*Spinel code for charging by wangxin77 at 2023/03/06 start*/
#define USB_DETECT_IN 1
#define USB_DETECT_OUT 0
#define CMD_CHARGER_ON	(0x53)
#define CMD_CHARGER_OFF (0x51)
/*Spinel code for charging by wangxin77 at 2023/03/06 end*/
#if WAKEUP_GESTURE
/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 start */
#define WAKEUP_OFF	0x00
#define WAKEUP_ON	0x01
extern bool nvt_gesture_flag;
/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 end */
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1
extern char *BOOT_UPDATE_FIRMWARE_NAME;
extern char *MP_UPDATE_FIRMWARE_NAME;
//#define BOOT_UPDATE_FIRMWARE_NAME "novatek_ts_fw_boe.bin"
//#define MP_UPDATE_FIRMWARE_NAME   "novatek_ts_mp_boe.bin"

/*Spinel code for Touching 10x resolution by zhangyd22 at 2023/05/04 start*/
#define NVT_SUPER_RESOLUTION_N 10
#if NVT_SUPER_RESOLUTION_N
#define POINT_DATA_LEN 108
#else
#define POINT_DATA_LEN 65
#endif
/*Spinel code for Touching 10x resolution by zhangyd22 at 2023/05/04 end*/
#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define CHECK_PEN_DATA_CHECKSUM 0

#if BOOT_UPDATE_FIRMWARE
#define SIZE_4KB 4096
#define FLASH_SECTOR_SIZE SIZE_4KB
#define FW_BIN_VER_OFFSET (fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)
#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (fw_need_write_size - NVT_FLASH_END_FLAG_LEN)
#endif
/* Spinel code for OSPINEL-3680 by zhangyd22 at 2023/5/11 start */
#define NVT_HALL_CHECK 1
#if NVT_HALL_CHECK
#define NVT_HALL_WORK_DELAY 1000
#endif
/* Spinel code for OSPINEL-3680 by zhangyd22 at 2023/5/11 end */
#if NVT_CUST_PROC_CMD
struct edge_grid_zone_info {
    uint8_t degree;
    uint8_t direction;
    uint16_t y1;
    uint16_t y2;
};
#endif

struct nvt_ts_data {
	struct spi_device *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];/*
#if defined(CONFIG_DRM_PANEL)
	struct notifier_block drm_panel_notif;*/
#if defined(CONFIG_DRM_MEDIATEK_V2)
	struct notifier_block disp_notifier;
#elif defined(_MSM_DRM_NOTIFY_H_)
	struct notifier_block drm_notif;
#elif defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	uint8_t fw_ver;
	uint8_t x_num;
	uint8_t y_num;
	uint16_t abs_x_max;
	uint16_t abs_y_max;
	uint8_t max_touch_num;
	uint8_t max_button_num;
	uint32_t int_trigger_type;
	int32_t irq_gpio;
	uint32_t irq_flags;
	int32_t reset_gpio;
	uint32_t reset_flags;
	struct mutex lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t hw_crc;
	uint8_t auto_copy;
	uint16_t nvt_pid;
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	bool pen_support;
	bool stylus_resol_double;
	uint8_t x_gang_num;
	uint8_t y_gang_num;
	struct input_dev *pen_input_dev;
	int8_t pen_phys[32];
	uint32_t chip_ver_trim_addr;
	uint32_t swrst_sif_addr;
	uint32_t crc_err_flag_addr;
#ifdef CONFIG_PM
	bool dev_pm_suspend;
	struct completion dev_pm_resume_completion;
#endif
/*Spinel code for charging by wangxin77 at 2023/03/06 start*/
	struct notifier_block charger_notif;
	struct workqueue_struct *nvt_charger_notify_wq;
	struct work_struct charger_notify_work;
	int usb_plug_status;
	int fw_update_stat;
/*Spinel code for charging by wangxin77 at 2023/03/06 end*/
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX
    struct mtk_chip_config spi_ctrl;
#endif
#if NVT_CUST_PROC_CMD
	int32_t edge_reject_state;
	struct edge_grid_zone_info edge_grid_zone_info;
	uint8_t game_mode_state;
	uint8_t pen_state;
	uint8_t pen_version;
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 start */
#if NVT_DPR_SWITCH
	uint8_t fw_pen_state;
#endif
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 end */
#endif
/*Spinel code for control pen state by zhangyd22 at 2023/04/04 start*/
	uint8_t nfc_state;
/*Spinel code for control pen state by zhangyd22 at 2023/04/04 end*/
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
    EVENT_MAP_RESET_COMPLETE                = 0x60,
    EVENT_MAP_FWINFO                        = 0x78,
    EVENT_MAP_PROJECTID                     = 0x9A,
} SPI_EVENT_MAP;

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(63*1024)
#define NVT_READ_LEN		(2*1024)
#define NVT_XBUF_LEN		(NVT_TRANSFER_LEN+1+DUMMY_BYTES)

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;
/* Spinel code for update kernel patch by zhangyd22 at 2023/4/6 start */
#if NVT_PEN_RAW
#define LENOVO_MAX_BUFFER 32
#define MAX_IO_CONTROL_REPORT	16

enum{
    DATA_TYPE_RAW = 0
};

struct lenovo_pen_coords_buffer {
	signed char status;
	signed char tool_type;
	signed char tilt_x;
	signed char tilt_y;
	unsigned long int x;
	unsigned long int y;
	unsigned long int p;
};

struct lenovo_pen_info {
	unsigned char frame_no;
	unsigned char data_type;
	u16 frame_t;
	struct lenovo_pen_coords_buffer coords;
};

struct io_pen_report {
	unsigned char report_num;
	unsigned char reserve[3];
	struct lenovo_pen_info pen_info[MAX_IO_CONTROL_REPORT];
};

enum pen_status{
	TS_NONE,
	TS_RELEASE,
	TS_TOUCH,
};
#endif
/* Spinel code for update kernel patch by zhangyd22 at 2023/4/6 end */
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
void nvt_fw_crc_enable(void);
void nvt_tx_auto_copy_mode(void);
void nvt_read_fw_history(uint32_t fw_history_addr);
int32_t nvt_update_firmware(char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_wait_auto_copy(void);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_PEN_RAW
void nvt_lenovo_report(enum pen_status status, uint32_t pen_x, uint32_t pen_y, uint32_t pen_tilt_x, uint32_t pen_tilt_y, uint32_t pen_pressure);
#endif

/*Spinel code for control pen state by zhangyd22 at 2023/04/04 start*/
int32_t nvt_support_pen_set_nfc(uint8_t state);
int32_t nvt_support_pen_set(uint8_t state, uint8_t version);
/*Spinel code for control pen state by zhangyd22 at 2023/04/04 end*/
#endif /* _LINUX_NVT_TOUCH_H */
