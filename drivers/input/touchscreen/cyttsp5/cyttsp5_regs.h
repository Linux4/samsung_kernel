/*
 * cyttsp5_regs.h
 * Cypress TrueTouch(TM) Standard Product V5 registers.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _CYTTSP5_REGS_H
#define _CYTTSP5_REGS_H

#include <asm/unaligned.h>

#define CY_FW_FILE_NAME "cyttsp5_fw.bin"

#define CY_MAX_PRBUF_SIZE           PIPE_BUF
#define CY_PR_TRUNCATED             " truncated..."

#define CY_DEFAULT_CORE_ID          "main_ttsp_core"
#define CY_MAX_NUM_CORE_DEVS        5

#define HID_CYVENDOR                0xff010000

#define HID_TOUCH_REPORT_ID         0x1
#define HID_BTN_REPORT_ID           0x3
#define HID_WAKEUP_REPORT_ID        0x4
#define HID_TRACKING_HEATMAP_REPOR_ID 0xE
#define HID_SENSOR_DATA_REPORT_ID   0xF

/*  Timeout in ms */
#define CY_REQUEST_EXCLUSIVE_TIMEOUT 500
#define CY_WATCHDOG_TIMEOUT         1000
#define CY_WATCHDOG_REQUEST_EXCLUSIVE_TIMEOUT 3000

/* maximum number of concurrent tracks */
#define MAX_TOUCH_NUMBER            10
#define TOUCH_REPORT_SIZE           10
#define TOUCH_INPUT_HEADER_SIZE     7
#define TOUCH_COUNT_BYTE_OFFSET     5
#define BTN_REPORT_SIZE             9
#define BTN_INPUT_HEADER_SIZE       5
#define SENSOR_REPORT_SIZE          150
#define SENSOR_HEADER_SIZE          4

/* helpers */
#define GET_NUM_TOUCHES(x)          ((x) & 0x1F)
#define IS_LARGE_AREA(x)            ((x) & 0x20)
#define IS_BAD_PKT(x)               ((x) & 0x20)
#define IS_TMO(t)                   ((t) == 0)

#define HI_BYTE(x)                  (u8)(((x) >> 8) & 0xFF)
#define LOW_BYTE(x)                 (u8)((x) & 0xFF)

#define SET_CMD_LOW(byte, bits)	\
	((byte) = (((byte) & 0xF0) | ((bits) & 0x0F)))
#define SET_CMD_HIGH(byte, bits)\
	((byte) = (((byte) & 0x0F) | ((bits) & 0xF0)))

#define GET_MASK(length) \
	((1 << length) - 1)
#define GET_FIELD(name, length, shift) \
	((name >> shift) & GET_MASK(length))

#define HID_ITEM_SIZE_MASK	0x03
#define HID_ITEM_TYPE_MASK	0x0C
#define HID_ITEM_TAG_MASK	0xF0

#define HID_ITEM_SIZE_SHIFT	0
#define HID_ITEM_TYPE_SHIFT	2
#define HID_ITEM_TAG_SHIFT	4

#define HID_GET_ITEM_SIZE(x)  \
	((x & HID_ITEM_SIZE_MASK) >> HID_ITEM_SIZE_SHIFT)
#define HID_GET_ITEM_TYPE(x) \
	((x & HID_ITEM_TYPE_MASK) >> HID_ITEM_TYPE_SHIFT)
#define HID_GET_ITEM_TAG(x) \
	((x & HID_ITEM_TAG_MASK) >> HID_ITEM_TAG_SHIFT)

/* drv_debug commands */
#define CY_DBG_SUSPEND                  4
#define CY_DBG_RESUME                   5
#define CY_DBG_SOFT_RESET               97
#define CY_DBG_RESET                    98
#define CY_DBG_HID_RESET                50
#define CY_DBG_HID_GET_REPORT           51
#define CY_DBG_HID_SET_REPORT           52
#define CY_DBG_HID_SET_POWER_ON         53
#define CY_DBG_HID_SET_POWER_SLEEP      54
#define CY_DBG_HID_NULL                 100
#define CY_DBG_HID_ENTER_BL             101
#define CY_DBG_HID_SYSINFO              102
#define CY_DBG_HID_SUSPEND_SCAN         103
#define CY_DBG_HID_RESUME_SCAN          104
#define CY_DBG_HID_STOP_WD              105
#define CY_DBG_HID_START_WD             106

/* Recognized usages */
/* undef them first for possible redefinition in Linux */
#undef HID_DI_PRESSURE
#undef HID_DI_TIP
#undef HID_DI_CONTACTID
#undef HID_DI_CONTACTCOUNT
#undef HID_DI_SCANTIME
#define HID_DI_PRESSURE		0x000d0030
#define HID_DI_TIP		0x000d0042
#define HID_DI_CONTACTID	0x000d0051
#define HID_DI_CONTACTCOUNT	0x000d0054
#define HID_DI_SCANTIME		0x000d0056

/* Cypress vendor specific usages */
#define HID_CY_UNDEFINED	0xff010000
#define HID_CY_BOOTLOADER	0xff010001
#define HID_CY_TOUCHAPPLICATION	0xff010002
#define HID_CY_BUTTONS		0xff010020
#define HID_CY_GENERICITEM	0xff010030
#define HID_CY_LARGEOBJECT	0xff010040
#define HID_CY_NOISEEFFECTS	0xff010041
#define HID_CY_REPORTCOUNTER	0xff010042
#define HID_CY_TOUCHTYPE	0xff010060
#define HID_CY_EVENTID		0xff010061
#define HID_CY_MAJORAXISLENGTH	0xff010062
#define HID_CY_MINORAXISLENGTH	0xff010063
#define HID_CY_ORIENTATION	0xff010064
#define HID_CY_BUTTONSIGNAL	0xff010065
#define HID_CY_TCH_COL_USAGE_PG 0x000D0022
#define HID_CY_BTN_COL_USAGE_PG 0xFF010020

enum hid_command {
	HID_CMD_RESERVED,
	HID_CMD_RESET,
	HID_CMD_GET_REPORT,
	HID_CMD_SET_REPORT,
	HID_CMD_GET_IDLE,
	HID_CMD_SET_IDLE,
	HID_CMD_GET_PROTOCOL,
	HID_CMD_SET_PROTOCOL,
	HID_CMD_SET_POWER,
	HID_CMD_VENDOR = 0xE,
};

enum hid_output_cmd_type {
	HID_OUTPUT_CMD_APP,
	HID_OUTPUT_CMD_BL,
};

enum hid_output {
	HID_OUTPUT_NULL,
	HID_OUTPUT_START_BOOTLOADER,
	HID_OUTPUT_GET_SYSINFO,
	HID_OUTPUT_SUSPEND_SCANNING,
	HID_OUTPUT_RESUME_SCANNING,
	HID_OUTPUT_GET_PARAM,
	HID_OUTPUT_SET_PARAM,
	HID_OUTPUT_GET_NOISE_METRICS,
	HID_OUTPUT_RESERVED,
	HID_OUTPUT_ENTER_EASYWAKE_STATE,
	HID_OUTPUT_VERIFY_CONFIG_BLOCK_CRC = 0x20,
	HID_OUTPUT_GET_CONFIG_ROW_SIZE,
	HID_OUTPUT_READ_CONF_BLOCK,
	HID_OUTPUT_WRITE_CONF_BLOCK,
	HID_OUTPUT_GET_DATA_STRUCTURE,
	HID_OUTPUT_LOAD_SELF_TEST_PARAM,
	HID_OUTPUT_RUN_SELF_TEST,
	HID_OUTPUT_GET_SELF_TEST_RESULT,
	HID_OUTPUT_CALIBRATE_IDACS,
	HID_OUTPUT_INITIALIZE_BASELINES,
	HID_OUTPUT_EXEC_PANEL_SCAN,
	HID_OUTPUT_RETRIEVE_PANEL_SCAN,
	HID_OUTPUT_START_SENSOR_DATA_MODE,
	HID_OUTPUT_STOP_SENSOR_DATA_MODE,
	HID_OUTPUT_START_TRACKING_HEATMAP_MODE,
	HID_OUTPUT_INT_PIN_OVERRIDE = 0x40,
	HID_OUTPUT_STORE_PANEL_SCAN = 0x60,
	HID_OUTPUT_PROCESS_PANEL_SCAN,
	HID_OUTPUT_DISCARD_INPUT_REPORT,
	HID_OUTPUT_LAST,
	HID_OUTPUT_USER_CMD,
};

enum hid_output_bl {
	HID_OUTPUT_BL_VERIFY_APP_INTEGRITY = 0x31,
	HID_OUTPUT_BL_APPEND_DATA_BUFF = 0x37,
	HID_OUTPUT_BL_GET_INFO,
	HID_OUTPUT_BL_PROGRAM_AND_VERIFY,
	HID_OUTPUT_BL_LAUNCH_APP = 0x3B,
	HID_OUTPUT_BL_INITIATE_BL = 0x48,
	HID_OUTPUT_BL_LAST,
};

#define HID_OUTPUT_BL_SOP	0x1
#define HID_OUTPUT_BL_EOP	0x17

enum hid_output_bl_status {
	ERROR_SUCCESS,
	ERROR_KEY,
	ERROR_VERIFICATION,
	ERROR_LENGTH,
	ERROR_DATA,
	ERROR_COMMAND,
	ERROR_CRC = 8,
	ERROR_FLASH_ARRAY,
	ERROR_FLASH_ROW,
	ERROR_FLASH_PROTECTION,
	ERROR_UKNOWN = 15,
	ERROR_INVALID,
};

enum cyttsp5_mode {
	CY_MODE_UNKNOWN,
	CY_MODE_BOOTLOADER,
	CY_MODE_OPERATIONAL,
};

enum {
	CY_IC_GRPNUM_RESERVED,
	CY_IC_GRPNUM_CMD_REGS,
	CY_IC_GRPNUM_TCH_REP,
	CY_IC_GRPNUM_DATA_REC,
	CY_IC_GRPNUM_TEST_REC,
	CY_IC_GRPNUM_PCFG_REC,
	CY_IC_GRPNUM_TCH_PARM_VAL,
	CY_IC_GRPNUM_TCH_PARM_SIZE,
	CY_IC_GRPNUM_RESERVED1,
	CY_IC_GRPNUM_RESERVED2,
	CY_IC_GRPNUM_OPCFG_REC,
	CY_IC_GRPNUM_DDATA_REC,
	CY_IC_GRPNUM_MDATA_REC,
	CY_IC_GRPNUM_TEST_REGS,
	CY_IC_GRPNUM_BTN_KEYS,
	CY_IC_GRPNUM_TTHE_REGS,
	CY_IC_GRPNUM_SENSING_CONF,
	CY_IC_GRPNUM_NUM,
};

enum cyttsp5_event_id {
	CY_EV_NO_EVENT,
	CY_EV_TOUCHDOWN,
	CY_EV_MOVE,		/* significant displacement (> act dist) */
	CY_EV_LIFTOFF,		/* record reports last position */
};

enum cyttsp5_object_id {
	CY_OBJ_STANDARD_FINGER,
	CY_OBJ_PROXIMITY,
	CY_OBJ_STYLUS,
	CY_OBJ_HOVER,
	CY_OBJ_GLOVE,
};

#define CY_NUM_MFGID                8

/* TTSP System Information interface definitions */
struct cyttsp5_cydata_dev {
	u8 pip_ver_major;
	u8 pip_ver_minor;
	__le16 fw_pid;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	__le32 revctrl;
	__le16 fw_ver_conf;
	u8 bl_ver_major;
	u8 bl_ver_minor;
	__le16 jtag_si_id_l;
	__le16 jtag_si_id_h;
	u8 mfg_id[CY_NUM_MFGID];
	__le16 post_code;
} __packed;

struct cyttsp5_sensing_conf_data_dev {
	u8 electrodes_x;
	u8 electrodes_y;
	__le16 len_x;
	__le16 len_y;
	__le16 res_x;
	__le16 res_y;
	__le16 max_z;
	u8 origin_x;
	u8 origin_y;
	u8 panel_id;
	u8 btn;
	u8 scan_mode;
	u8 max_num_of_tch_per_refresh_cycle;
} __packed;

struct cyttsp5_cydata {
	u8 pip_ver_major;
	u8 pip_ver_minor;
	u8 bl_ver_major;
	u8 bl_ver_minor;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u16 fw_pid;
	u16 fw_ver_conf;
	u16 post_code;
	u32 revctrl;
	u16 jtag_id_l;
	u16 jtag_id_h;
	u8 mfg_id[CY_NUM_MFGID];
};

struct cyttsp5_sensing_conf_data {
	u16 res_x;
	u16 res_y;
	u16 max_z;
	u16 len_x;
	u16 len_y;
	u8 electrodes_x;
	u8 electrodes_y;
	u8 origin_x;
	u8 origin_y;
	u8 panel_id;
	u8 btn;
	u8 scan_mode;
	u8 max_num_of_tch_per_refresh_cycle;
};

enum cyttsp5_tch_abs {	/* for ordering within the extracted touch data array */
	CY_TCH_X,	/* X */
	CY_TCH_Y,	/* Y */
	CY_TCH_P,	/* P (Z) */
	CY_TCH_T,	/* TOUCH ID */
	CY_TCH_E,	/* EVENT ID */
	CY_TCH_O,	/* OBJECT ID */
	CY_TCH_TIP,	/* OBJECT ID */
	CY_TCH_MAJ,	/* TOUCH_MAJOR */
	CY_TCH_MIN,	/* TOUCH_MINOR */
	CY_TCH_OR,	/* ORIENTATION */
	CY_TCH_NUM_ABS,
};

enum cyttsp5_tch_hdr {
	CY_TCH_TIME,	/* SCAN TIME */
	CY_TCH_NUM,	/* NUMBER OF RECORDS */
	CY_TCH_LO,	/* LARGE OBJECT */
	CY_TCH_NOISE,	/* NOISE EFFECT */
	CY_TCH_COUNTER,	/* REPORT_COUNTER */
	CY_TCH_NUM_HDR,
};

static const char * const cyttsp5_tch_abs_string[] = {
	[CY_TCH_X]	= "X",
	[CY_TCH_Y]	= "Y",
	[CY_TCH_P]	= "P",
	[CY_TCH_T]	= "T",
	[CY_TCH_E]	= "E",
	[CY_TCH_O]	= "O",
	[CY_TCH_TIP]	= "TIP",
	[CY_TCH_MAJ]	= "MAJ",
	[CY_TCH_MIN]	= "MIN",
	[CY_TCH_OR]	= "OR",
	[CY_TCH_NUM_ABS] = "INVALID",
};

static const char * const cyttsp5_tch_hdr_string[] = {
	[CY_TCH_TIME]	= "SCAN TIME",
	[CY_TCH_NUM]	= "NUMBER OF RECORDS",
	[CY_TCH_LO]	= "LARGE OBJECT",
	[CY_TCH_NOISE]	= "NOISE EFFECT",
	[CY_TCH_COUNTER] = "REPORT_COUNTER",
	[CY_TCH_NUM_HDR] = "INVALID",
};

static const int cyttsp5_tch_abs_field_map[] = {
	[CY_TCH_X]	= 0x00010030 /* HID_GD_X */,
	[CY_TCH_Y]	= 0x00010031 /* HID_GD_Y */,
	[CY_TCH_P]	= HID_DI_PRESSURE,
	[CY_TCH_T]	= HID_DI_CONTACTID,
	[CY_TCH_E]	= HID_CY_EVENTID,
	[CY_TCH_O]	= HID_CY_TOUCHTYPE,
	[CY_TCH_TIP]	= HID_DI_TIP,
	[CY_TCH_MAJ]	= HID_CY_MAJORAXISLENGTH,
	[CY_TCH_MIN]	= HID_CY_MINORAXISLENGTH,
	[CY_TCH_OR]	= HID_CY_ORIENTATION,
	[CY_TCH_NUM_ABS] = 0,
};

static const int cyttsp5_tch_hdr_field_map[] = {
	[CY_TCH_TIME]	= HID_DI_SCANTIME,
	[CY_TCH_NUM]	= HID_DI_CONTACTCOUNT,
	[CY_TCH_LO]	= HID_CY_LARGEOBJECT,
	[CY_TCH_NOISE]	= HID_CY_NOISEEFFECTS,
	[CY_TCH_COUNTER] = HID_CY_REPORTCOUNTER,
	[CY_TCH_NUM_HDR] = 0,
};


#define CY_NUM_EXT_TCH_FIELDS   3

struct cyttsp5_tch_abs_params {
	size_t ofs;	/* abs byte offset */
	size_t size;	/* size in bits */
	size_t min;	/* min value */
	size_t max;	/* max value */
	size_t bofs;	/* bit offset */
	u8 report;
};

struct cyttsp5_touch {
	int hdr[CY_TCH_NUM_HDR];
	int abs[CY_TCH_NUM_ABS];
};

/* button to keycode support */
#define CY_BITS_PER_BTN		1
#define CY_NUM_BTN_EVENT_ID	((1 << CY_BITS_PER_BTN) - 1)

enum cyttsp5_btn_state {
	CY_BTN_RELEASED = 0,
	CY_BTN_PRESSED = 1,
	CY_BTN_NUM_STATE
};

struct cyttsp5_btn {
	bool enabled;
	int state;	/* CY_BTN_PRESSED, CY_BTN_RELEASED */
	int key_code;
};

enum cyttsp5_ic_ebid {
	CY_TCH_PARM_EBID,
	CY_MDATA_EBID,
	CY_DDATA_EBID,
};

struct cyttsp4_ttconfig {
	u16 version;
	u16 crc;
};

struct cyttsp5_report_desc_data {
	u16 tch_report_id;
	u16 tch_record_size;
	u16 tch_header_size;
	u16 btn_report_id;
};

struct cyttsp5_sysinfo {
	bool ready;
	struct cyttsp5_cydata cydata;
	struct cyttsp5_sensing_conf_data sensing_conf_data;
	struct cyttsp5_report_desc_data desc;
	int num_btns;
	struct cyttsp5_btn *btn;
	struct cyttsp4_ttconfig ttconfig;
	struct cyttsp5_tch_abs_params tch_hdr[CY_TCH_NUM_HDR];
	struct cyttsp5_tch_abs_params tch_abs[CY_TCH_NUM_ABS];
	u8 *xy_mode;
	u8 *xy_data;
};

#endif /* _CYTTSP5_REGS_H */
