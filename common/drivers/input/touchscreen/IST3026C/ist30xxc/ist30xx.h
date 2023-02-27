/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __IST30XXC_H__
#define __IST30XXC_H__

/*
 * Support F/W ver : IST30xxC v1.0.0.0
 * Support IC : IST3026C, IST3032C
 * Release : 2015.01.05 by Hoony
 */

#define IMAGIS_DD_VERSION           ("2.0.0.0")

#define IMAGIS_IST3026C             (1) /* 3026C */
#define IMAGIS_IST3032C             (2) /* 3032C */
#define IMAGIS_IST3038C             (3) /* 3038C */
#define IMAGIS_IST3044C             (4) /* 3044C */
#define IMAGIS_IST3048C             (5) /* 3048C */

#define IMAGIS_TSP_IC               IMAGIS_IST3026C

#define TSP_CHIP_VENDOR             ("IMAGIS")

#define IMAGIS_PROTOCOL_A           (0xA)
#define IMAGIS_PROTOCOL_B           (0xB)
#define IMAGIS_PROTOCOL_TYPE        IMAGIS_PROTOCOL_B

#define IST30XXC_DEFAULT_CHIP_ID    (0x300C)
#define IST3048C_DEFAULT_CHIP_ID    (0x3048)
#if (IMAGIS_TSP_IC == IMAGIS_IST3026C)
#define TS_CHIP_NAME                ("IST3026C")
#define IST30XXC_CHIP_ID            (0x032C)
#define IST30XXC_NODE_TOTAL_NUM     (16 * 16)
#elif (IMAGIS_TSP_IC == IMAGIS_IST3032C)
#define TS_CHIP_NAME                ("IST3032C")
#define IST30XXC_CHIP_ID            (0x032C)
#define IST30XXC_NODE_TOTAL_NUM     (16 * 16)
#elif (IMAGIS_TSP_IC == IMAGIS_IST3038C)
#define TS_CHIP_NAME                ("IST3038C")
#define IST30XXC_CHIP_ID            (0x038C)
#define IST30XXC_NODE_TOTAL_NUM     (24 * 24)
#elif (IMAGIS_TSP_IC == IMAGIS_IST3044C)
#define TS_CHIP_NAME                ("IST3044C")
#define IST30XXC_CHIP_ID            (0x038C)
#define IST30XXC_NODE_TOTAL_NUM     (24 * 24)
#elif (IMAGIS_TSP_IC == IMAGIS_IST3048C)
#define TS_CHIP_NAME                ("IST3048C")
#define IST30XXC_CHIP_ID            (0x038C)
#define IST30XXC_NODE_TOTAL_NUM     (24 * 24)
#else
#define TS_CHIP_NAME                ("IST30XXC")
#define IST30XXC_CHIP_ID            (0x300C)
#define IST30XXC_NODE_TOTAL_NUM     (24 * 24)
#endif

#define IST30XXC_INTERNAL_BIN       (1)
#define IST30XXC_CHECK_CALIB    	(0)
#if IST30XXC_INTERNAL_BIN
#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
#define IST30XXC_MULTIPLE_TSP       (0)
#endif
#define IST30XXC_UPDATE_BY_WORKQUEUE    (0)
#define IST30XXC_UPDATE_DELAY       (3 * HZ)
#endif

#define IST30XXC_EVENT_MODE         (1)
#if IST30XXC_EVENT_MODE
#define IST30XXC_NOISE_MODE         (1)
#define IST30XXC_TRACKING_MODE      (1)
#define IST30XXC_ALGORITHM_MODE     (1)
#else
#define IST30XXC_NOISE_MODE         (0) // fixed
#define IST30XXC_TRACKING_MODE      (0) // fixed
#define IST30XXC_ALGORITHM_MODE     (0) // fixed
#endif

#define IST30XXC_TA_RESET           (1)
#define IST30XXC_USE_KEY            (1)
#define IST30XXC_DEBUG              (1)
#define IST30XXC_CMCS_TEST          (1)
#define IST30XXC_STATUS_DEBUG       (0)
#if (IST30XXC_DEBUG && IST30XXC_CMCS_TEST)
#define IST30XXC_JIG_MODE           (1)
#else
#define IST30XXC_JIG_MODE           (0) // fixed
#endif

#define IST30XXC_SEC_FACTORY        (1)

#define IST30XXC_ADDR_LEN           (4)
#define IST30XXC_DATA_LEN           (4)

#define IST30XXC_MAX_MT_FINGERS     (10)
#define IST30XXC_MAX_KEYS           (5)

#if defined (CONFIG_MACH_YOUNG23GDTV) || defined (CONFIG_MACH_J13G)
#define IST30XXC_MAX_X              (320)
#define IST30XXC_MAX_Y              (480)
#elif defined(CONFIG_MACH_VIVALTO5MVE3G)
#define IST30XXC_MAX_X              (480)
#define IST30XXC_MAX_Y              (800)
#else
#define IST30XXC_MAX_X              (240)
#define IST30XXC_MAX_Y              (320)
#endif
#define IST30XXC_MAX_W              (15)

#define IST30XXC_EXCEPT_MASK        (0xFFFFFF00)
#define IST30XXC_EXCEPT_VALUE       (0xE11CE900)
#define IST30XXC_MAX_EXCEPT_SIZE    (2)

#define IST30XXC_JIG_TOUCH  	    (0xC0)
#define IST30XXC_ENABLE  	        (1)
#define IST30XXC_DISABLE  	        (0)

/* retry count */
#define IST30XXC_MAX_RETRY_CNT      (3)

/* Local */
#define TS_LOCAL_EU                 (0)
#define TS_LOCAL_EEU                (1)
#define TS_LOCAL_TD                 (11)
#define TS_LOCAL_CMCC               (12)
#define TS_LOCAL_CU                 (13)
#define TS_LOCAL_SPRD               (14)
#define TS_LOCAL_CTC                (15)
#define TS_LOCAL_INDIA              (21)
#define TS_LOCAL_SWASIA             (22)
#define TS_LOCAL_NA                 (31)
#define TS_LOCAL_LA                 (32)
#define TS_LOCAL_CODE               TS_LOCAL_EU

/* Debug message */
#define TS_DEV_ERR                  (1)
#define TS_DEV_WARN                 (2)
#define TS_DEV_INFO                 (3)
#define TS_DEV_NOTI                 (4)
#define TS_DEV_DEBUG                (5)
#define TS_DEV_VERB                 (6)

#define IST30XXC_DEBUG_TAG          "[ TSP ]"
#define IST30XXC_DEBUG_LEVEL        TS_DEV_NOTI

#define ts_err(fmt, ...)            ts_printk(TS_DEV_ERR, fmt, ## __VA_ARGS__)
#define ts_warn(fmt, ...)           ts_printk(TS_DEV_WARN, fmt, ## __VA_ARGS__)
#define ts_info(fmt, ...)           ts_printk(TS_DEV_INFO, fmt, ## __VA_ARGS__)
#define ts_noti(fmt, ...)           ts_printk(TS_DEV_NOTI, fmt, ## __VA_ARGS__)
#define ts_debug(fmt, ...)          ts_printk(TS_DEV_DEBUG, fmt, ## __VA_ARGS__)
#define ts_verb(fmt, ...)           ts_printk(TS_DEV_VERB, fmt, ## __VA_ARGS__)

#if defined(CONFIG_MACH_GARDAVE)
#define TSP_USE_GPIO_CONTROL
#define TSP_PWR_GPIO_EN	173
#else
#define TSP_USE_LDO_POWER
#endif

/* i2c setting */
// I2C Device info
//#define IST30XXC_DEV_NAME           "IST30XX"
//#define IST30XXC_DEV_ID             (0xA0 >> 1)

// I2C Mode
#define I2C_BURST_MODE              (1)
#define I2C_MONOPOLY_MODE           (0)

// I2C transfer msg number
#define WRITE_CMD_MSG_LEN           (1)
#define READ_CMD_MSG_LEN            (2)

// I2C address/Data length
#define IST30XXC_ADDR_LEN           (4)         /* bytes */
#define IST30XXC_DATA_LEN           (4)         /* bytes */

 //I2C transaction size
#define I2C_MAX_WRITE_SIZE          (0x0100)	/* bytes */
#define I2C_MAX_READ_SIZE           (0x0080)	/* bytes */

// I2C access mode
#define IST30XXC_DIRECT_ACCESS      (1 << 31)
#define IST30XXC_BURST_ACCESS       (1 << 27)
#define IST30XXC_HIB_ACCESS         (0x800B << 16)
#define IST30XXC_DA_ADDR(n)         (n | IST30XXC_DIRECT_ACCESS)
#define IST30XXC_BA_ADDR(n)         (n | IST30XXC_BURST_ACCESS)
#define IST30XXC_HA_ADDR(n)         (n | IST30XXC_HIB_ACCESS)

/* register */
// Info register
#define IST30XXC_REG_CHIPID     	IST30XXC_DA_ADDR(0x40001000)
#define IST30XXC_REG_TSPTYPE    	IST30XXC_DA_ADDR(0x40002010)

// HIB register
#define IST30XXC_HIB_BASE           (0x30000100)
#define IST30XXC_HIB_TOUCH_STATUS   IST30XXC_HA_ADDR(IST30XXC_HIB_BASE | 0x00)
#define IST30XXC_HIB_INTR_MSG       IST30XXC_HA_ADDR(IST30XXC_HIB_BASE | 0x04)
#define IST30XXC_HIB_COORD          IST30XXC_HA_ADDR(IST30XXC_HIB_BASE | 0x08)
#define IST30XXC_HIB_CMD            IST30XXC_HA_ADDR(IST30XXC_HIB_BASE | 0x3C)
#define IST30XXC_HIB_RW_STATUS      IST30XXC_HA_ADDR(IST30XXC_HIB_BASE | 0x40)

/* interrupt macro */
#define IST30XXC_INTR_STATUS        (0x00000C00)
#define CHECK_INTR_STATUS(n)        (((n & IST30XXC_INTR_STATUS) \
				                        == IST30XXC_INTR_STATUS) ? 1 : 0)
#define PARSE_FINGER_CNT(n)         ((n >> 12) & 0xF)
#define PARSE_KEY_CNT(n)            ((n >> 21) & 0x7)
/* Finger status: [9:0] */
#define PARSE_FINGER_STATUS(n)      (n & 0x3FF)
/* Key status: [20:16] */
#define PARSE_KEY_STATUS(n)         ((n >> 16) & 0x1F)
#define PRESSED_FINGER(s, id)       ((s & (1 << (id - 1))) ? true : false)
#define PRESSED_KEY(s, id)          ((s & (1 << (16 + id - 1))) ? true : false)

#define IST30XXC_MAX_CMD_SIZE        (0x20)
#define IST30XXC_CMD_ADDR(n)         (n * 4)
#define IST30XXC_CMD_VALUE(n)        (n / 4)
enum ist30xxc_read_commands {
    eHCOM_GET_CHIP_ID           = IST30XXC_CMD_ADDR(0x00),
    eHCOM_GET_VER_CORE          = IST30XXC_CMD_ADDR(0x01),
    eHCOM_GET_VER_FW            = IST30XXC_CMD_ADDR(0x02),
    eHCOM_GET_VER_PROJECT       = IST30XXC_CMD_ADDR(0x03),
    eHCOM_GET_VER_TEST          = IST30XXC_CMD_ADDR(0x04),
    eHCOM_GET_CRC32             = IST30XXC_CMD_ADDR(0x05),
    eHCOM_GET_CRC32_ALL         = IST30XXC_CMD_ADDR(0x06),
    eHCOM_GET_CAL_RESULT        = IST30XXC_CMD_ADDR(0x07),
    eHCOM_GET_TSP_VENDOR        = IST30XXC_CMD_ADDR(0x08),

    eHCOM_GET_LCD_INFO          = IST30XXC_CMD_ADDR(0x10),
    eHCOM_GET_TSP_INFO          = IST30XXC_CMD_ADDR(0x11),
    eHCOM_GET_KEY_INFO_0        = IST30XXC_CMD_ADDR(0x12),
    eHCOM_GET_KEY_INFO_1        = IST30XXC_CMD_ADDR(0x13),
    eHCOM_GET_KEY_INFO_2        = IST30XXC_CMD_ADDR(0x14),
    eHCOM_GET_SCR_INFO          = IST30XXC_CMD_ADDR(0x15),
    eHCOM_GET_GTX_INFO          = IST30XXC_CMD_ADDR(0x16),
    eHCOM_GET_SWAP_INFO         = IST30XXC_CMD_ADDR(0x17),
    eHCOM_GET_FINGER_INFO       = IST30XXC_CMD_ADDR(0x18),
    eHCOM_GET_BASELINE          = IST30XXC_CMD_ADDR(0x19),
    eHCOM_GET_TOUCH_TH          = IST30XXC_CMD_ADDR(0x1A),

    eHCOM_GET_ZVALUE_BASE       = IST30XXC_CMD_ADDR(0x1C),
    eHCOM_GET_CDC_BASE          = IST30XXC_CMD_ADDR(0x1D),
    eHCOM_GET_ALGO_BASE         = IST30XXC_CMD_ADDR(0x1E),
    eHCOM_GET_COM_CHECKSUM      = IST30XXC_CMD_ADDR(0x1F),
};

enum ist30xxc_write_commands {
    eHCOM_FW_START			    = 0x01,
	eHCOM_FW_HOLD			    = 0x02,

    eHCOM_CP_CORRECT_EN 		= 0x10,
	eHCOM_WDT_EN				= 0x11,
	eHCOM_GESTURE_EN			= 0x12,
	eHCOM_SCALE_EN				= 0x13,
	eHCOM_NEW_POSITION_DIS		= 0x14,
    eHCOM_SLEEP_MODE_EN		    = 0x15,
    
    eHCOM_SET_TIME_ACTIVE		= 0x20,
	eHCOM_SET_TIME_IDLE			= 0x21,
	eHCOM_SET_MODE_SPECIAL		= 0x22,
	eHCOM_SET_LOCAL_MODEL		= 0x23,
    
    eHCOM_RUN_RAMCODE	        = 0x30,
	eHCOM_RUN_CAL_AUTO			= 0x31,
	eHCOM_RUN_CAL_PARAM			= 0x32,
	
    eHCOM_SET_JIG_MODE			= 0x80,
	eHCOM_SET_JIG_SENSITI		= 0x81,
   	
    eHCOM_DEFAULT				= 0xFF,
};

typedef union {
	struct {
		u32	y       : 12;
		u32	x       : 12;
		u32	area    : 4;
		u32	id      : 4;
	} bit_field;
	u32 full_field;
} fingers_info;

struct ist30xxc_status {
	int	power;
	int	update;
    int update_result;
	int	calib;
	int	calib_msg;
    int	cmcs;
	bool event_mode;
	bool noise_mode;
};

struct ist30xxc_version {
    u32 core_ver;
    u32 fw_ver;
    u32 project_ver;
    u32 test_ver;
};

struct ist30xxc_fw {
    struct ist30xxc_version prev;
    struct ist30xxc_version cur;
    struct ist30xxc_version bin;
	u32	index;
	u32	size;
	u32	chksum;
	u32 buf_size;
	u8 *buf;
};

#define IST30XXC_TAG_MAGIC           "ISTV2TAG"
struct ist30xxc_tags {
	char magic1[8];
    u32 rom_base;
    u32 ram_base;
    u32 reserved0;
    u32 reserved1;

	u32	fw_addr;
	u32	fw_size;
	u32	cfg_addr;
	u32	cfg_size;
	u32	sensor_addr;
	u32	sensor_size;
	u32	cp_addr;
	u32	cp_size;
	u32	flag_addr;
	u32	flag_size;
    u32 reserved2;
    u32 reserved3;

    u32 zvalue_base;
    u32 algo_base;
    u32 raw_base;
    u32 filter_base;
    u32 reserved4;
    u32 reserved5;

	u32	chksum;
	u32	chksum_all;
    u32 reserved6;
    u32 reserved7;

	u8 day;
	u8 month;
	u16 year;
	u8 hour;
	u8 min;
	u8 sec;
	u8 reserved8;
	char magic2[8];
};

struct CH_NUM {
	u8 tx;
	u8 rx;
};

struct GTX_INFO {
	u8 num;
	u8 ch_num[4];
};

struct TSP_NODE_BUF {
	u16	raw[IST30XXC_NODE_TOTAL_NUM];
	u16	base[IST30XXC_NODE_TOTAL_NUM];
	u16	filter[IST30XXC_NODE_TOTAL_NUM];
	u16	min_raw;
	u16	max_raw;
	u16	min_base;
	u16	max_base;
	u16	len;
};

struct TSP_DIRECTION {
	bool swap_xy;
	bool flip_x;
	bool flip_y;
};

typedef struct _TSP_INFO {
	struct CH_NUM ch_num;
	struct CH_NUM screen;
	struct GTX_INFO	gtx;
	struct TSP_DIRECTION dir;
	struct TSP_NODE_BUF node;
	int height;
	int width;
	int finger_num;
    u16 baseline;
    u16 threshold; 
} TSP_INFO;

typedef struct _TKEY_INFO {
	int	key_num;
	bool enable;
	struct CH_NUM ch_num[IST30XXC_MAX_KEYS];
	u16	baseline;
    u16 threshold;
} TKEY_INFO;

#if IST30XXC_SEC_FACTORY
#include "ist30xx_sec.h"
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
struct ist30xxc_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
    TSP_INFO tsp_info;
    TKEY_INFO tkey_info;
#ifdef CONFIG_HAS_EARLYSUSPEND	
	struct early_suspend early_suspend;
#endif
	struct ist30xxc_status status;
	struct ist30xxc_fw fw;
	struct ist30xxc_tags tags;
#if IST30XXC_SEC_FACTORY
	struct sec_fac sec;
#endif
    u32	chip_id;
	u32	tsp_type;
	u32	max_fingers;
	u32	max_keys;    
    u32	t_status;
	fingers_info	fingers[IST30XXC_MAX_MT_FINGERS];
#if IST30XXC_JIG_MODE
    u32 z_values[IST30XXC_MAX_MT_FINGERS];
#endif
    volatile bool irq_working;
    u32	irq_enabled;
    bool initialized;
    u32 noise_mode;
    u32 debug_mode;
#if IST30XXC_JIG_MODE
    u32 jig_mode;
#endif
    int report_rate;
    int idle_rate;
    int scan_count;
    int scan_retry;
    int max_scan_retry;
    int irq_err_cnt;
    int max_irq_err_cnt;
    struct delayed_work     work_reset_check;
    struct delayed_work     work_noise_protect;
    struct delayed_work     work_debug_algorithm;
#if IST30XXC_UPDATE_BY_WORKQUEUE
    struct delayed_work     work_fw_update;
#endif
};

extern struct mutex ist30xxc_mutex;
extern int ist30xxc_dbg_level;

void ts_printk(int level, const char *fmt, ...);
int ist30xxc_intr_wait(struct ist30xxc_data *data, long ms);

void ist30xxc_enable_irq(struct ist30xxc_data *data);
void ist30xxc_disable_irq(struct ist30xxc_data *data);
void ist30xxc_set_ta_mode(int status);
void ist30xxc_set_call_mode(int mode);
void ist30xxc_set_cover_mode(int mode);
void ist30xxc_start(struct ist30xxc_data *data);
int ist30xxc_get_ver_info(struct ist30xxc_data *data);

int ist30xxc_read_reg(struct i2c_client *client, u32 reg, u32 *buf);
int ist30xxc_read_cmd(struct ist30xxc_data *data, u32 cmd, u32 *buf);
int ist30xxc_write_cmd(struct i2c_client *client, u32 cmd, u32 val);
int ist30xxc_burst_read(struct i2c_client *client, u32 addr,
			u32 *buf32, u16 len, bool bit_en);
int ist30xxc_burst_write(struct i2c_client *client, u32 addr, 
            u32 *buf32, u16 len);

int ist30xxc_cmd_start_scan(struct ist30xxc_data *data);
int ist30xxc_cmd_calibrate(struct i2c_client *client);
int ist30xxc_cmd_check_calib(struct i2c_client *client);
int ist30xxc_cmd_update(struct i2c_client *client, int cmd);
int ist30xxc_cmd_hold(struct i2c_client *client, int enable);

int ist30xxc_power_on(struct ist30xxc_data *data, bool download);
int ist30xxc_power_off(struct ist30xxc_data *data);
int ist30xxc_reset(struct ist30xxc_data *data, bool download);

int ist30xxc_internal_suspend(struct ist30xxc_data *data);
int ist30xxc_internal_resume(struct ist30xxc_data *data);

int ist30xxc_init_system(struct ist30xxc_data *data);

int ist30xxc_probe(struct i2c_client *client, const struct i2c_device_id *id);
int ist30xxc_suspend(struct device *dev);
int ist30xxc_resume(struct device *dev);
int ist30xxc_remove(struct i2c_client *client);
void ist30xxc_shutdown(struct i2c_client *client);

extern int get_hw_rev(void);
#endif  // __IST30XXC_H__
