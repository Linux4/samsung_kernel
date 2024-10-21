/*SPDX-License-Identifier: GPL-2.0*/
/*
 * drivers/input/sec_input/stm_fst1ba90a_i2c/stm_dev.h
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_STM_TS_H_
#define _LINUX_STM_TS_H_

#include <linux/input/sec_input.h>

#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
#define TCLM_CONCEPT
#endif

#define TOUCH_PRINT_INFO_DWORK_TIME	30000	/* 30s */

#define STM_TS_MAX_FW_PATH			64
#define STM_TS_DRV_NAME			"stm_ts_i2c"

#define STM_TS_I2C_RETRY_CNT		3
#define STM_TS_MODE_CHANGE_RETRY_CNT	5

/*
 * fst1b: ID0: 0x39
 * fst9c: ID0: 0x50
 */
#define STM_TS_ID0				0x39
#define STM_TS_ID1				0x36

#define STM_TS_FIFO_MAX			32
#define STM_TS_EVENT_SIZE			16
#define STM_TS_VERSION_SIZE		9

#define STM_TS_CMD_SPONGE_ACCESS				0x0000

/* COMMANDS */
#define STM_TS_CMD_SENSE_ON				0x10
#define STM_TS_CMD_SENSE_OFF				0x11
#define STM_TS_CMD_SW_RESET				0x12
#define STM_TS_CMD_FORCE_CALIBRATION			0x13
#define STM_TS_CMD_FACTORY_PANELCALIBRATION		0x14

#define STM_TS_READ_GPIO_STATUS				0x20
#define STM_TS_READ_FIRMWARE_INTEGRITY			0x21
#define STM_TS_READ_DEVICE_ID				0x22
#define STM_TS_READ_PANEL_INFO				0x23
#define STM_TS_READ_FW_VERSION				0x24

#define STM_TS_CMD_SET_GET_TOUCH_FUNCTION			0x30
#define STM_TS_CMD_SET_GET_OPMODE				0x31
#define STM_TS_CMD_SET_GET_CHARGER_MODE			0x32
#define STM_TS_CMD_SET_GET_NOISE_MODE			0x33
#define STM_TS_CMD_SET_GET_REPORT_RATE			0x34
#define STM_TS_CMD_SET_GET_TOUCH_MODE_FOR_THRESHOLD	0x35
#define STM_TS_CMD_SET_GET_TOUCH_THRESHOLD			0x36
#define STM_TS_CMD_SET_GET_KEY_THRESHOLD			0x37
#define STM_TS_CMD_SET_GET_COVERTYPE			0x38
#define STM_TS_CMD_WRITE_WAKEUP_GESTURE			0x39
#define STM_TS_CMD_WRITE_COORDINATE_FILTER			0x3A
#define STM_TS_CMD_SET_FOD_FINGER_MERGE			0x3B

#define STM_TS_READ_ONE_EVENT				0x60
#define STM_TS_READ_ALL_EVENT				0x61
#define STM_TS_CMD_CLEAR_ALL_EVENT				0x62

#define STM_TS_CMD_SENSITIVITY_MODE			0x70
#define STM_TS_READ_SENSITIVITY_VALUE			0x72
#define STM_TS_CMD_RUN_SRAM_TEST				0x78

#define STM_TS_CMD_SET_FUNCTION_ONOFF			0xC1

/* STM_TS SPONGE COMMAND */
#define STM_TS_CMD_SPONGE_READ_WRITE_CMD			0xAA
#define STM_TS_CMD_SPONGE_NOTIFY_CMD			0xC0

#define STM_TS_CMD_SPONGE_OFFSET_MODE			0x00
#define STM_TS_CMD_SPONGE_OFFSET_AOD_RECT			0x02
#define STM_TS_CMD_SPONGE_OFFSET_UTC			0x10
#define STM_TS_CMD_SPONGE_PRESS_PROPERTY			0x14
#define STM_TS_CMD_SPONGE_FOD_INFO				0x15
#define STM_TS_CMD_SPONGE_FOD_POSITION			0x19
#define STM_TS_CMD_SPONGE_FOD_RECT				0x4B
#define STM_TS_CMD_SPONGE_LP_DUMP				0xF0

/* First byte of ONE EVENT */
#define STM_TS_EVENT_PASS_REPORT				0x03
#define STM_TS_EVENT_STATUS_REPORT				0x43
#define STM_TS_EVENT_JITTER_RESULT				0x49
#define STM_TS_EVENT_ERROR_REPORT				0xF3

/* Test Event */
#define STM_TS_EVENT_JITTER_MUTUAL_TEST			0x01
#define STM_TS_EVENT_JITTER_SELF_TEST			0x02

#define STM_TS_EVENT_JITTER_MUTUAL_MAX			0x01
#define STM_TS_EVENT_JITTER_MUTUAL_MIN			0x02
#define STM_TS_EVENT_JITTER_SELF_TX_P2P			0x05
#define STM_TS_EVENT_JITTER_SELF_RX_P2P			0x06

#define STM_TS_EVENT_SRAM_TEST_RESULT			0xD0

/* Status Event */
#define STM_TS_COORDINATE_EVENT			0
#define STM_TS_STATUS_EVENT			1
#define STM_TS_GESTURE_EVENT			2
#define STM_TS_VENDOR_EVENT			3

#define STM_TS_GESTURE_CODE_SPAY			0x00
#define STM_TS_GESTURE_CODE_DOUBLE_TAP		0x01

#define STM_TS_COORDINATE_ACTION_NONE		0
#define STM_TS_COORDINATE_ACTION_PRESS		1
#define STM_TS_COORDINATE_ACTION_MOVE		2
#define STM_TS_COORDINATE_ACTION_RELEASE		3

#define STM_TS_TOUCHTYPE_NORMAL		0
#define STM_TS_TOUCHTYPE_HOVER		1
#define STM_TS_TOUCHTYPE_FLIPCOVER		2
#define STM_TS_TOUCHTYPE_GLOVE		3
#define STM_TS_TOUCHTYPE_STYLUS		4
#define STM_TS_TOUCHTYPE_PALM		5
#define STM_TS_TOUCHTYPE_WET			6
#define STM_TS_TOUCHTYPE_PROXIMITY		7
#define STM_TS_TOUCHTYPE_JIG			8

/* Status - ERROR event */
#define STM_TS_EVENT_STATUSTYPE_CMDDRIVEN		0
#define STM_TS_EVENT_STATUSTYPE_ERROR		1
#define STM_TS_EVENT_STATUSTYPE_INFORMATION	2
#define STM_TS_EVENT_STATUSTYPE_USERINPUT		3
#define STM_TS_EVENT_STATUSTYPE_VENDORINFO		7

#define STM_TS_ERR_EVNET_CORE_ERR			0x00
#define STM_TS_ERR_EVENT_QUEUE_FULL		0x01
#define STM_TS_ERR_EVENT_ESD			0x02

/* Status - Information report */
#define STM_TS_INFO_READY_STATUS			0x00
#define STM_TS_INFO_WET_MODE			0x01
#define STM_TS_INFO_NOISE_MODE			0x02


// Scan mode for A0 command
#define STM_TS_SCAN_MODE_SCAN_OFF			0
#define STM_TS_SCAN_MODE_MS_SS_SCAN		(1 << 0)
#define STM_TS_SCAN_MODE_KEY_SCAN			(1 << 1)
#define STM_TS_SCAN_MODE_HOVER_SCAN		(1 << 2)
#define STM_TS_SCAN_MODE_FORCE_TOUCH_SCAN		(1 << 4)
#define STM_TS_SCAN_MODE_DEFAULT			STM_TS_SCAN_MODE_MS_SS_SCAN


/* Control Command */

// For 0x30 command - touch type
#define STM_TS_TOUCHTYPE_BIT_TOUCH		(1 << 0)
#define STM_TS_TOUCHTYPE_BIT_HOVER		(1 << 1)
#define STM_TS_TOUCHTYPE_BIT_COVER		(1 << 2)
#define STM_TS_TOUCHTYPE_BIT_GLOVE		(1 << 3)
#define STM_TS_TOUCHTYPE_BIT_STYLUS	(1 << 4)
#define STM_TS_TOUCHTYPE_BIT_PALM		(1 << 5)
#define STM_TS_TOUCHTYPE_BIT_WET		(1 << 6)
#define STM_TS_TOUCHTYPE_BIT_PROXIMITY	(1 << 7)
#define STM_TS_TOUCHTYPE_DEFAULT_ENABLE	(STM_TS_TOUCHTYPE_BIT_TOUCH | STM_TS_TOUCHTYPE_BIT_PALM | STM_TS_TOUCHTYPE_BIT_WET)

// For 0x31 command - touch operation mode
#define STM_TS_OPMODE_NORMAL		0
#define STM_TS_OPMODE_LOWPOWER		1

// For 0x32 command - charger mode
#define STM_TS_CHARGER_MODE_NORMAL				0
#define STM_TS_CHARGER_MODE_WIRE_CHARGER			1
#define STM_TS_CHARGER_MODE_WIRELESS_CHARGER		2
#define STM_TS_CHARGER_MODE_WIRELESS_BATTERY_PACK		3

// For 0xC1 command - on/off function
#define STM_TS_FUNCTION_ENABLE_SIP_MODE			0x00
#define STM_TS_FUNCTION_SET_MONITOR_NOISE_MODE		0x01
#define STM_TS_FUNCTION_ENABLE_BRUSH_MODE			0x02
#define STM_TS_FUNCTION_ENABLE_DEAD_ZONE			0x04	/* *#0*# */
#define STM_TS_FUNCTION_ENABLE_SPONGE_LIB			0x05
#define STM_TS_FUNCTION_EDGE_AREA				0x07	/* used for grip cmd */
#define STM_TS_FUNCTION_DEAD_ZONE				0x08	/* used for grip cmd */
#define STM_TS_FUNCTION_LANDSCAPE_MODE			0x09	/* used for grip cmd */
#define STM_TS_FUNCTION_LANDSCAPE_TOP_BOTTOM		0x0A	/* used for grip cmd */
#define STM_TS_FUNCTION_EDGE_HANDLER			0x0C	/* used for grip cmd */
#define STM_TS_FUNCTION_ENABLE_VSYNC			0x0D
#define STM_TS_FUNCTION_SET_TOUCHABLE_AREA			0x0F
#define STM_TS_FUNCTION_SET_NOTE_MODE			0x10
#define STM_TS_FUNCTION_SET_GAME_MODE			0x11
#define STM_TS_FUNCTION_SET_HOVER_DETECTION		0x12

#define STM_TS_RETRY_COUNT					10

/* gesture SF */
#define STM_TS_GESTURE_SAMSUNG_FEATURE			1

/* gesture type */
#define STM_TS_SPONGE_EVENT_SWIPE_UP			0
#define STM_TS_SPONGE_EVENT_DOUBLETAP			1
#define STM_TS_SPONGE_EVENT_PRESS				3
#define STM_TS_SPONGE_EVENT_SINGLETAP			4

/* gesture ID */
#define STM_TS_SPONGE_EVENT_GESTURE_ID_AOD			0
#define STM_TS_SPONGE_EVENT_GESTURE_ID_DOUBLETAP_TO_WAKEUP	1
#define STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_LONG		0
#define STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_NORMAL		1
#define STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_RELEASE		2
#define STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_OUT		3

#define STM_TS_MAX_X_RESOLUTION	1599
#define STM_TS_MAX_Y_RESOLUTION	2559
#define STM_TS_MAX_NUM_FORCE		53	/* Number of TX CH */
#define STM_TS_MAX_NUM_SENSE		33	/* Number of RX CH */

enum {
	TYPE_RAW_DATA = 0x31,
	TYPE_BASELINE_DATA = 0x32,
	TYPE_STRENGTH_DATA = 0x33,
};

enum stm_ts_error_return {
	STM_TS_NOT_ERROR = 0,
	STM_TS_I2C_ERROR,
	STM_TS_ERROR_INVALID_CHIP_ID,
	STM_TS_ERROR_INVALID_CHIP_VERSION_ID,
	STM_TS_ERROR_INVALID_SW_VERSION,
	STM_TS_ERROR_EVENT_ID,
	STM_TS_ERROR_FW_CORRUPTION,
	STM_TS_ERROR_TIMEOUT,
	STM_TS_ERROR_TIMEOUT_ZERO,
	STM_TS_ERROR_FW_UPDATE_FAIL,
	STM_TS_ERROR_BROKEN_OSC_TRIM,
};

enum stm_ts_fw_update_status {
	STM_TS_NOT_UPDATE = 10,
	STM_TS_NEED_FW_UPDATE,
	STM_TS_NEED_CALIBRATION_ONLY,
	STM_TS_NEED_FW_UPDATE_N_CALIBRATION,
};

struct stm_ts_sponge_information {
	u8 sponge_use;  // 0 : don't use, 1 : use
	u16 sponge_ver;
	u16 sponge_model_number;
	u16 sponge_model_name_size;
	u8 sponge_model_name[32];
} __packed;

enum stm_ts_config_value_feature {
	STM_TS_CFG_NONE = 0,
	STM_TS_CFG_APWR = 1,
	STM_TS_CFG_AUTO_TUNE_PROTECTION = 2,
};

enum stm_ts_system_information_address {
	STM_TS_SI_CONFIG_CHECKSUM = 0x58, /* 4 bytes */
};

enum stm_ts_ito_test_mode {
	OPEN_TEST = 0,
	OPEN_SHORT_CRACK_TEST,
};

enum stm_ts_ito_test_result {
	ITO_PASS = 0,
	ITO_FAIL,
	ITO_FAIL_OPEN,
	ITO_FAIL_SHORT,
};

/* 16 byte */
struct stm_ts_event_coordinate {
	u8 eid:2;
	u8 tid:4;
	u8 tchsta:2;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 major;
	u8 minor;
	u8 z:6;
	u8 ttype_3_2:2;
	u8 left_event:6;
	u8 ttype_1_0:2;
	u8 noise_level;
	u8 max_strength;
	u8 hover_id_num:4;
	u8 noise_status:2;
	u8 eom:1;
	u8 reserved_10:1;
	u8 freq_id:4;
	u8 fod_debug:4;
	u8 reserved_12;
	u8 reserved_13;
	u8 reserved_14;
	u8 reserved_15;
} __packed;

/* 16 byte */
struct stm_ts_event_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 status_id;
	u8 status_data_1;
	u8 status_data_2;
	u8 status_data_3;
	u8 status_data_4;
	u8 status_data_5;
	u8 left_event_5_0:6;
	u8 reserved:2;
	u8 reserved_8;
	u8 reserved_9;
	u8 reserved_10;
	u8 reserved_11;
	u8 reserved_12;
	u8 reserved_13;
	u8 reserved_14;
	u8 reserved_15;
} __packed;


/* 8 byte */
struct stm_ts_gesture_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 gesture_id;
	u8 gesture_data_1;
	u8 gesture_data_2;
	u8 gesture_data_3;
	u8 gesture_data_4;
	u8 reserved_1;
	u8 left_event_5_0:6;
	u8 reserved_2:2;
	u8 reserved_8;
	u8 reserved_9;
	u8 reserved_10;
	u8 reserved_11;
	u8 reserved_12;
	u8 reserved_13;
	u8 reserved_14;
	u8 reserved_15;
} __packed;

struct stm_ts_syncframeheader {
	u8		header; // 0
	u8		host_data_mem_id; // 1
	u16	  cnt;  // 2~3
	u8		dbg_frm_len;  // 4
	u8		ms_force_len; // 5
	u8		ms_sense_len; // 6
	u8		ss_force_len; // 7
	u8		ss_sense_len; // 8
	u8		key_len;  // 9
	u16	  reserved1;  // 10~11
	u32	  reserved2;  // 12~15
} __packed;

struct stm_ts_snr_result_cmd {
	s16	status;
	s16	point;
	s16	average;
} __packed;

struct tsp_snr_result_of_point {
	s16	max;
	s16	min;
	s16	average;
	s16	nontouch_peak_noise;
	s16	touch_peak_noise;
	s16	snr1;
	s16	snr2;
} __packed;

struct stm_ts_snr_result {
	s16	status;
	s16	reserved[6];
	struct tsp_snr_result_of_point result[9];
} __packed;

struct stm_ts_data {
	struct i2c_client *client;
	struct sec_ts_plat_data *plat_data;
	struct mutex lock;
	bool probe_done;

	struct sec_cmd_data sec;
	u8 *cx_data;
	s8 *vp_cap_data;
	struct sec_ts_test_result test_result;
	u8 disassemble_count;
	u8 fac_nv;

	struct sec_tclm_data *tdata;
	bool is_cal_done;

	bool fw_corruption;
	bool osc_trim_error;
	bool glove_enabled;
	bool flip_enable;
	u8 cover_cmd;
	u8 external_noise_mode;

	u8 charger_mode;

	u8 scan_mode;
	u8 vsync_scan;
	u8 dead_zone;
	u8 sip_mode;
	u8 note_mode;
	u8 game_mode;

	struct delayed_work work_print_info;

	unsigned int debug_string;
	struct delayed_work reset_work;
	struct delayed_work work_read_info;
	struct delayed_work debug_work;
	bool rawdata_read_lock;
	atomic_t reset_is_on_going;
	atomic_t fw_update_is_running;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_interrupt;
	struct sec_touch_driver *ss_drv;
#endif
	struct mutex i2c_mutex;
	struct mutex device_mutex;
	struct mutex eventlock;
	struct mutex sponge_mutex;
	struct mutex wait_for;
	bool reinit_done;

	u8 factory_position;

	bool fix_active_mode;
	bool touch_aging_mode;

	int lpmode_change_delay;
	bool disable_vsync_scan;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	struct notifier_block stm_input_nb;
#endif
	int (*stm_ts_write)(struct stm_ts_data *ts, u8 *reg, u16 num_com);
	int (*stm_ts_read)(struct stm_ts_data *ts, u8 *reg, int cnum, u8 *buf, int num);
	int (*stm_ts_read_from_sponge)(struct stm_ts_data *ts, u16 offset, u8 *data, int length);
	int (*stm_ts_write_to_sponge)(struct stm_ts_data *ts, u16 offset, u8 *data, int length);
	int (*stm_ts_systemreset)(struct stm_ts_data *ts, unsigned int msec);
	int (*stm_ts_wait_for_ready)(struct stm_ts_data *ts);
	int (*stm_ts_command)(struct stm_ts_data *ts, u8 cmd, bool checkEcho);

	int (*stm_ts_get_version_info)(struct stm_ts_data *ts);
	int (*stm_ts_get_sysinfo_data)(struct stm_ts_data *ts, u8 sysinfo_addr, u8 read_cnt, u8 *data);


};
/* stm_sec.c */
int stm_ts_get_channel_info(struct stm_ts_data *ts);
int stm_ts_fn_init(struct stm_ts_data *ts);
void stm_ts_fn_remove(struct stm_ts_data *ts);
int stm_ts_set_fod_rect(struct stm_ts_data *ts);
void run_cx_data_read(void *device_data);
void run_rawcap_read(void *device_data);
void run_delta_read(void *device_data);
int stm_ts_set_aod_rect(struct stm_ts_data *ts);
/* stm_ts.c */
int stm_ts_fw_update_on_probe(struct stm_ts_data *ts);
int stm_ts_fw_update_on_hidden_menu(struct stm_ts_data *ts, int update_type);
int stm_ts_execute_autotune(struct stm_ts_data *ts, bool IsSaving);
int stm_ts_wait_for_echo_event(struct stm_ts_data *ts, u8 *cmd, u8 cmd_cnt, int delay);
int stm_ts_get_tsp_test_result(struct stm_ts_data *ts);
void stm_ts_release_all_finger(struct stm_ts_data *ts);
int stm_ts_set_opmode(struct stm_ts_data *ts, u8 mode);
int stm_ts_set_scanmode(struct stm_ts_data *ts, u8 scan_mode);
int stm_ts_osc_trim_recovery(struct stm_ts_data *ts);
void stm_ts_set_grip_data_to_ic(struct device *dev, u8 flag);
void stm_ts_set_cover_type(struct stm_ts_data *ts, bool enable);
int stm_ts_charger_mode(struct stm_ts_data *ts);
int stm_ts_fw_corruption_check(struct stm_ts_data *ts);
int stm_ts_set_touch_function(struct stm_ts_data *ts);
void stm_ts_change_scan_rate(struct stm_ts_data *ts, u8 rate);
int stm_ts_set_press_property(struct stm_ts_data *ts);
void stm_ts_set_fod_finger_merge(struct stm_ts_data *ts);
void stm_ts_run_rawdata_all(struct stm_ts_data *ts);
int stm_ts_sip_mode_enable(struct stm_ts_data *ts);
int stm_ts_game_mode_enable(struct stm_ts_data *ts);
int stm_ts_note_mode_enable(struct stm_ts_data *ts);
int stm_ts_dead_zone_enable(struct stm_ts_data *ts);
int stm_ts_check_custom_library(struct stm_ts_data *ts);
int stm_ts_set_custom_library(struct stm_ts_data *ts);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
int stm_ts_tool_proc_init(struct stm_ts_data *ts);
int stm_ts_tool_proc_remove(void);
#endif

#ifdef TCLM_CONCEPT
int stm_tclm_data_read(struct device *dev, int address);
int stm_tclm_data_write(struct device *dev, int address);
int stm_ts_tclm_execute_force_calibration(struct device *dev, int cal_mode);
#endif
int set_nvm_data(struct stm_ts_data *ts, u8 type, u8 *buf);
int get_nvm_data(struct stm_ts_data *ts, int type, u8 *nvdata);

int stm_ts_panel_ito_test(struct stm_ts_data *ts, int testmode);
int stm_ts_set_external_noise_mode(struct stm_ts_data *ts, u8 mode);
int stm_ts_fix_active_mode(struct stm_ts_data *ts, bool enable);
#endif /* _LINUX_STM_TS_H_ */
