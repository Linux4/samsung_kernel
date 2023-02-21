/* SPDX-License-Identifier: GPL-2.0 */
/*
 *
 * Zinitix zt75xx touch driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_ZT75XX_TS_H
#define _LINUX_ZT75XX_TS_H

#define TSP_VERBOSE_DEBUG

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/completion.h>
//#include <linux/wakelock.h>

#include <linux/input/mt.h>

#if IS_ENABLED(CONFIG_SEC_SYSFS)
#include <linux/sec_sysfs.h>
#elif IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#else
extern struct class *sec_class;
#endif

#include "../../../../sec_input/sec_cmd.h"
#include "../../../../sec_input/sec_input.h"
//#include <linux/input/sec_cmd.h>
//#include <linux/input/sec_tclm_v2.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/muic/common/muic.h>
#include <linux/muic/common/muic_notifier.h>
#include <linux/vbus_notifier.h>
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_SUPPORT_SENSOR_FOLD)
#include <linux/sensor/sensors_core.h>
#endif
#endif

#if IS_ENABLED(CONFIG_SPU_VERIFY)
#include <linux/spu-verify.h>
#endif

#define CONFIG_INPUT_ENABLED
#define SEC_FACTORY_TEST

#define NOT_SUPPORTED_TOUCH_DUMMY_KEY

//#define GLOVE_MODE

#define MAX_FW_PATH 255
#define TSP_EXTERNAL_FW		"tsp.bin"

#if IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
#include <linux/trustedui.h>
#endif

#undef SUPPORTED_PALM_TOUCH

extern char *saved_command_line;

#define ZINITIX_DEBUG			0
#define PDIFF_DEBUG			1
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#define USE_MISC_DEVICE
#endif

/* added header file */

#define TOUCH_POINT_MODE		0
#define MAX_SUPPORTED_FINGER_NUM	2 /* max 10 */

#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
#define MAX_SUPPORTED_BUTTON_NUM	2 /* max 8 */
#define SUPPORTED_BUTTON_NUM		2
#else
#define MAX_SUPPORTED_BUTTON_NUM	6 /* max 8 */
#define SUPPORTED_BUTTON_NUM		2
#endif

/* Upgrade Method*/
#define TOUCH_ONESHOT_UPGRADE		1
/*
 * if you use isp mode, you must add i2c device :
 * name = "zinitix_isp" , addr 0x50
 */

/* resolution offset */
#define ABS_PT_OFFSET			(-1)

#define TOUCH_FORCE_UPGRADE		1
#define USE_CHECKSUM			1
#define CHECK_HWID			0

#define CHIP_OFF_DELAY			50 /*ms*/
#define CHIP_ON_DELAY			50 /*ms*/
#define FIRMWARE_ON_DELAY		150 /*ms*/

#define DELAY_FOR_SIGNAL_DELAY		30 /*us*/
#define DELAY_FOR_TRANSCATION		50
#define DELAY_FOR_POST_TRANSCATION	10

#define ZINITIX_I2C_RETRY_CNT		2

#define ZINITIX_STATUS_UNFOLDING	0x00
#define ZINITIX_STATUS_FOLDING		0x01

enum tsp_status_call_pos {
	ZINITIX_STATE_CHK_POS_OPEN = 0,
	ZINITIX_STATE_CHK_POS_CLOSE,
	ZINITIX_STATE_CHK_POS_HALL,
	ZINITIX_STATE_CHK_POS_SYSFS,
};

/* ZINITIX_TS_DEBUG : Print event contents */
#define ZINITIX_TS_DEBUG_PRINT_ALLEVENT		0x01
#define ZINITIX_TS_DEBUG_PRINT_ONEEVENT		0x02
#define ZINITIX_TS_DEBUG_PRINT_I2C_READ_CMD	0x04
#define ZINITIX_TS_DEBUG_PRINT_I2C_WRITE_CMD	0x08
#define ZINITIX_TS_DEBUG_SEND_UEVENT		0x80

enum power_control {
	POWER_OFF,
	POWER_ON,
	POWER_ON_SEQUENCE,
};

/* Key Enum */
enum key_event {
	ICON_BUTTON_UNCHANGE,
	ICON_BUTTON_DOWN,
	ICON_BUTTON_UP,
};

/* ESD Protection */
/*second : if 0, no use. if you have to use, 3 is recommended*/
#define ESD_TIMER_INTERVAL			1
#define SCAN_RATE_HZ				5000
#define CHECK_ESD_TIMER				7

#define TOUCH_PRINT_INFO_DWORK_TIME		30000 /* 30s */

/*Test Mode (Monitoring Raw Data) */
#define TSP_INIT_TEST_RATIO		100

#define	SEC_MUTUAL_AMP_V_SEL		0x0232

#define	SEC_DND_N_COUNT			11
#define	SEC_DND_U_COUNT			16
#define	SEC_DND_FREQUENCY		139

#define	SEC_HFDND_N_COUNT		11
#define	SEC_HFDND_U_COUNT		16
#define	SEC_HFDND_FREQUENCY		104

#define	SEC_SX_AMP_V_SEL		0x0434
#define	SEC_SX_SUB_V_SEL		0x0055
#define	SEC_SY_AMP_V_SEL		0x0232
#define	SEC_SY_SUB_V_SEL		0x0022
#define	SEC_SHORT_N_COUNT		2
#define	SEC_SHORT_U_COUNT		1

#define SEC_SY_SAT_FREQUENCY		200
#define SEC_SY_SAT_N_COUNT		9
#define SEC_SY_SAT_U_COUNT		9
#define SEC_SY_SAT_RS0_TIME		0x00FF
#define SEC_SY_SAT_RBG_SEL		0x0404
#define SEC_SY_SAT_AMP_V_SEL		0x0434
#define SEC_SY_SAT_SUB_V_SEL		0x0044

#define SEC_SY_SAT2_FREQUENCY		200
#define SEC_SY_SAT2_N_COUNT		9
#define SEC_SY_SAT2_U_COUNT		3
#define SEC_SY_SAT2_RS0_TIME		0x00FF
#define SEC_SY_SAT2_RBG_SEL		0x0404
#define SEC_SY_SAT2_AMP_V_SEL		0x0434
#define SEC_SY_SAT2_SUB_V_SEL		0x0011

#define MAX_RAW_DATA_SZ			792 /* 36x22 */
#define MAX_TRAW_DATA_SZ		(MAX_RAW_DATA_SZ + 4*MAX_SUPPORTED_FINGER_NUM + 2)

#define RAWDATA_DELAY_FOR_HOST		10000

struct raw_ioctl {
	u32 sz;
	u32 buf;
};

struct reg_ioctl {
	u32 addr;
	u32 val;
};

#define TOUCH_SEC_MODE				48
#define TOUCH_REF_MODE				10
#define TOUCH_NORMAL_MODE			5
#define TOUCH_DELTA_MODE			3
//#define TOUCH_SDND_MODE				6
#define TOUCH_RAW_MODE				7
#define TOUCH_REFERENCE_MODE			8
#define TOUCH_DND_MODE				11
#define TOUCH_HFDND_MODE			12
#define TOUCH_TXSHORT_MODE			13
#define TOUCH_RXSHORT_MODE			14
#define TOUCH_CHANNEL_TEST_MODE			14
#define TOUCH_JITTER_MODE			15
#define TOUCH_SELF_DND_MODE			17
#define TOUCH_HYBRID_BASELINED_DATA_MODE	20
#define TOUCH_SENTIVITY_MEASUREMENT_MODE	21
#define TOUCH_REF_ABNORMAL_TEST_MODE		33
#define DEF_RAW_SELF_SSR_DATA_MODE		39	/* SELF SATURATION RX */
#define DEF_RAW_SELF_SFR_UNIT_DATA_MODE		40
#define DEF_RAW_LP_DUMP				41
#define TOUCH_NO_OPERATION_MODE			55

#define TOUCH_SENTIVITY_MEASUREMENT_COUNT	9

/*  Other Things */
#define INIT_RETRY_CNT				3
#define I2C_SUCCESS				0
#define I2C_FAIL				1

/*---------------------------------------------------------------------*/

/* chip code */
#define BT43X_CHIP_CODE		0xE200
#define BT53X_CHIP_CODE		0xF400
#define ZT7548_CHIP_CODE	0xE548
#define ZT7538_CHIP_CODE	0xE538
#define ZT7532_CHIP_CODE	0xE532
#define ZT7554_CHIP_CODE	0xE700
#define ZTW522_CHIP_CODE	0xE532

/* Register Map*/
#define ZT75XX_SWRESET_CMD			0x0000
#define ZT75XX_WAKEUP_CMD			0x0001
#define ZT75XX_DEEP_SLEEP_CMD			0x0004
#define ZT75XX_IDLE_CMD				0x0004
#define ZT75XX_SLEEP_CMD			0x0005

#define ZT75XX_CLEAR_INT_STATUS_CMD		0x0003
#define ZT75XX_CALIBRATE_CMD			0x0006
#define ZT75XX_SAVE_STATUS_CMD			0x0007
#define ZT75XX_SAVE_CALIBRATION_CMD		0x0008
#define ZT75XX_DRIVE_INT_STATUS_CMD		0x000C
#define ZT75XX_RECALL_FACTORY_CMD		0x000f

#define ZT75XX_THRESHOLD			0x0020

#define ZT75XX_DEBUG_REG			0x0115

#define ZT75XX_TOUCH_MODE			0x0010
#define ZT75XX_CHIP_REVISION			0x0011
#define ZT75XX_FIRMWARE_VERSION			0x0012

#define ZT75XX_MINOR_FW_VERSION			0x0121

#define ZT75XX_VENDOR_ID			0x001C
#define ZT75XX_HW_ID				0x0014

#define ZT75XX_DATA_VERSION_REG			0x0013
#define ZT75XX_SUPPORTED_FINGER_NUM		0x0015
#define ZT75XX_EEPROM_INFO			0x0018
#define ZT75XX_INITIAL_TOUCH_MODE		0x0019

#define ZT75XX_TOTAL_NUMBER_OF_X		0x0060
#define ZT75XX_TOTAL_NUMBER_OF_Y		0x0061

#define ZT75XX_CONNECTION_CHECK_REG		0x0062

#define ZT75XX_POWER_STATE_FLAG			0x007E
#define ZT75XX_DELAY_RAW_FOR_HOST		0x007F

#define ZT75XX_BUTTON_SUPPORTED_NUM		0x00B0
#define ZT75XX_BUTTON_SENSITIVITY		0x00B2
#define ZT75XX_DUMMY_BUTTON_SENSITIVITY		0X00C8

#define ZT75XX_X_RESOLUTION			0x00C0
#define ZT75XX_Y_RESOLUTION			0x00C1

#define ZT75XX_POINT_STATUS_REG			0x0080
#define ZT_STATUS_REG				0x0080
#define ZT_POINT_STATUS_REG				0x0200
#define ZT_POINT_STATUS_REG1				0x0201

#define ZT75XX_ICON_STATUS_REG			0x00AA

#define ZT75XX_SET_AOD_X_REG			0x00AB
#define ZT75XX_SET_AOD_Y_REG			0x00AC
#define ZT75XX_SET_AOD_W_REG			0x00AD
#define ZT75XX_SET_AOD_H_REG			0x00AE
#define ZT75XX_LPM_MODE_REG			0x00AF

#define ZT75XX_GET_AOD_X_REG			0x0191
#define ZT75XX_GET_AOD_Y_REG			0x0192

#define ZT75XX_DND_SHIFT_VALUE			0x012B
#define ZT75XX_AFE_FREQUENCY			0x0100
#define ZT75XX_DND_N_COUNT			0x0122
#define ZT75XX_DND_U_COUNT			0x0135

#define ZT75XX_RAW_DIFF				0x0146

#define ZT75XX_LPDUMP_UTC_VAL_MSB_REG		0x01FC
#define ZT75XX_LPDUMP_UTC_VAL_LSB_REG		0x01FD

#define ZT75XX_RAWDATA_REG			0x0200
#define ZT75XX_LP_DUMP_REG			0x022F

#define ZT75XX_INT_ENABLE_FLAG			0x00f0
#define ZT75XX_PERIODICAL_INTERRUPT_INTERVAL	0x00f1
#define ZT75XX_BTN_WIDTH			0x0316
#define ZT75XX_REAL_WIDTH			0x03A6

#define ZT75XX_CHECKSUM_RESULT			0x012c

#define ZT75XX_INIT_FLASH			0x01d0
#define ZT75XX_WRITE_FLASH			0x01d1
#define ZT75XX_READ_FLASH			0x01d2

#define ZINITIX_INTERNAL_FLAG_03		0x011f

#define ZT75XX_OPTIONAL_SETTING			0x0116
#define ZT75XX_COVER_CONTROL_REG		0x023E

#define ZT75XX_RESOLUTION_EXPANDER		0x0186
#define ZT75XX_MUTUAL_AMP_V_SEL			0x02F9
#define ZT75XX_SX_AMP_V_SEL			0x02DF
#define ZT75XX_SX_SUB_V_SEL			0x02E0
#define ZT75XX_SY_AMP_V_SEL			0x02EC
#define ZT75XX_SY_SUB_V_SEL			0x02ED
#define ZT75XX_CHECKSUM				0x03DF
#define ZT75XX_JITTER_SAMPLING_CNT		0x001F

#define ZT75XX_SY_SAT_FREQUENCY			0x03E0
#define ZT75XX_SY_SAT_N_COUNT			0x03E1
#define ZT75XX_SY_SAT_U_COUNT			0x03E2
#define ZT75XX_SY_SAT_RS0_TIME			0x03E3
#define ZT75XX_SY_SAT_RBG_SEL			0x03E4
#define ZT75XX_SY_SAT_AMP_V_SEL			0x03E5
#define ZT75XX_SY_SAT_SUB_V_SEL			0x03E6

#define ZT75XX_SY_SAT2_FREQUENCY		0x03E7
#define ZT75XX_SY_SAT2_N_COUNT			0x03E8
#define ZT75XX_SY_SAT2_U_COUNT			0x03E9
#define ZT75XX_SY_SAT2_RS0_TIME			0x03EA
#define ZT75XX_SY_SAT2_RBG_SEL			0x03EB
#define ZT75XX_SY_SAT2_AMP_V_SEL		0x03EC
#define ZT75XX_SY_SAT2_SUB_V_SEL		0x03ED

/* Interrupt & status register flag bit */
#define BIT_PT_CNT_CHANGE	0
#define BIT_DOWN		1
#define BIT_MOVE		2
#define BIT_UP			3
#define BIT_PALM		4
#define BIT_PALM_REJECT		5
#define BIT_GESTURE		6
#define RESERVED_1		7
#define BIT_WEIGHT_CHANGE	8
#define BIT_PT_NO_CHANGE	9
#define BIT_REJECT		10
#define BIT_PT_EXIST		11
#define RESERVED_2		12
#define BIT_MUST_ZERO		13
#define BIT_DEBUG		14
#define BIT_ICON_EVENT		15

/* button */
#define BIT_O_ICON0_DOWN	0
#define BIT_O_ICON1_DOWN	1
#define BIT_O_ICON2_DOWN	2
#define BIT_O_ICON3_DOWN	3
#define BIT_O_ICON4_DOWN	4
#define BIT_O_ICON5_DOWN	5
#define BIT_O_ICON6_DOWN	6
#define BIT_O_ICON7_DOWN	7

#define BIT_O_ICON0_UP		8
#define BIT_O_ICON1_UP		9
#define BIT_O_ICON2_UP		10
#define BIT_O_ICON3_UP		11
#define BIT_O_ICON4_UP		12
#define BIT_O_ICON5_UP		13
#define BIT_O_ICON6_UP		14
#define BIT_O_ICON7_UP		15


#define SUB_BIT_EXIST		0
#define SUB_BIT_DOWN		1
#define SUB_BIT_MOVE		2
#define SUB_BIT_UP		3
#define SUB_BIT_UPDATE		4
#define SUB_BIT_WAIT		5

/* ZT75XX_DEBUG_REG */
#define DEF_DEVICE_STATUS_NPM			0
#define DEF_DEVICE_STATUS_WALLET_COVER_MODE	1
#define DEF_DEVICE_STATUS_NOISE_MODE		2
#define DEF_DEVICE_STATUS_WATER_MODE		3
#define DEF_DEVICE_STATUS_LPM__MODE		4
#define BIT_GLOVE_TOUCH				5
#define DEF_DEVICE_STATUS_PALM_DETECT		10
#define DEF_DEVICE_STATUS_SVIEW_MODE		11

/* ZT75XX_COVER_CONTROL_REG */
#define WALLET_COVER_CLOSE	0x0000
#define VIEW_COVER_CLOSE	0x0100
#define COVER_OPEN		0x0200
#define LED_COVER_CLOSE		0x0700
#define CLEAR_COVER_CLOSE	0x0800

enum zt_cover_id {
	ZT_FLIP_WALLET = 0,
	ZT_VIEW_COVER,
	ZT_COVER_NOTHING1,
	ZT_VIEW_WIRELESS,
	ZT_COVER_NOTHING2,
	ZT_CHARGER_COVER,
	ZT_VIEW_WALLET,
	ZT_LED_COVER,
	ZT_CLEAR_FLIP_COVER,
	ZT_QWERTY_KEYBOARD_EUR,
	ZT_QWERTY_KEYBOARD_KOR,
	ZT_NEON_COVER,
	ZT_MONTBLANC_COVER = 100,
};

#define zinitix_bit_set(val, n)		((val) &= ~(1<<(n)), (val) |= (1<<(n)))
#define zinitix_bit_clr(val, n)		((val) &= ~(1<<(n)))
#define zinitix_bit_test(val, n)	((val) & (1<<(n)))
#define zinitix_swap_v(a, b, t)		((t) = (a), (a) = (b), (b) = (t))
#define zinitix_swap_16(s)		(((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)))

/* REG_USB_STATUS : optional setting from AP */
#define DEF_OPTIONAL_MODE_USB_DETECT_BIT	0
#define	DEF_OPTIONAL_MODE_SVIEW_DETECT_BIT	1
#define	DEF_OPTIONAL_MODE_SENSITIVE_BIT		2
#define DEF_OPTIONAL_MODE_EDGE_SELECT		3
#define	DEF_OPTIONAL_MODE_DUO_TOUCH		4
/* end header file */

#define DEF_MIS_CAL_SPEC_MIN 40
#define DEF_MIS_CAL_SPEC_MAX 160
#define DEF_MIS_CAL_SPEC_MID 100

#define BIT_EVENT_SPAY	1
#define BIT_EVENT_AOD	2
#define BIT_EVENT_SINGLE_TAP	 3
#define BIT_EVENT_AOT	5

enum ZT_SPONGE_EVENT_TYPE {
	ZT_SPONGE_EVENT_TYPE_SPAY			= 0x04,
	ZT_SPONGE_EVENT_TYPE_AOD			= 0x08,
	ZT_SPONGE_EVENT_TYPE_AOD_PRESS		= 0x09,
	ZT_SPONGE_EVENT_TYPE_AOD_LONGPRESS		= 0x0A,
	ZT_SPONGE_EVENT_TYPE_AOD_DOUBLETAB		= 0x0B
};

#ifdef SEC_FACTORY_TEST
/* Touch Screen */
#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN		3240	//30*18*6
#define TSP_CMD_PARAM_NUM		8
#define TSP_CMD_X_NUM			30
#define TSP_CMD_Y_NUM			18
#define TSP_CMD_NODE_NUM		(TSP_CMD_Y_NUM * TSP_CMD_X_NUM)
#define tostring(x) #x

struct tsp_raw_data {
	s16 dnd_data[TSP_CMD_NODE_NUM];
	s16 hfdnd_data[TSP_CMD_NODE_NUM];
	s16 delta_data[TSP_CMD_NODE_NUM];
	s16 vgap_data[TSP_CMD_NODE_NUM];
	s16 hgap_data[TSP_CMD_NODE_NUM];
	s16 rxshort_data[TSP_CMD_NODE_NUM];
	s16 txshort_data[TSP_CMD_NODE_NUM];
	s16 selfdnd_data[TSP_CMD_NODE_NUM];
	u16 ssr_data[TSP_CMD_NODE_NUM];
	s16 self_sat_dnd_data[TSP_CMD_NODE_NUM];
	s16 self_hgap_data[TSP_CMD_NODE_NUM];
	s16 jitter_data[TSP_CMD_NODE_NUM];
	s16 reference_data[TSP_CMD_NODE_NUM];
	s16 reference_data_abnormal[TSP_CMD_NODE_NUM];
	u16 channel_test_data[5];
};

/* ----------------------------------------
 * write 0xE4 [ 11 | 10 | 01 | 00 ]
 * MSB <-------------------> LSB
 * read 0xE4
 * mapping sequnce : LSB -> MSB
 * struct sec_ts_test_result {
 * * assy : front + OCTA assay
 * * module : only OCTA
 *	 union {
 *		 struct {
 *			 u8 assy_count:2;	-> 00
 *			 u8 assy_result:2;	-> 01
 *			 u8 module_count:2;	-> 10
 *			 u8 module_result:2;	-> 11
 *		 } __attribute__ ((packed));
 *		 unsigned char data[1];
 *	 };
 *};
 * ----------------------------------------
 */
struct ts_test_result {
	union {
		struct {
			u8 assy_count:2;
			u8 assy_result:2;
			u8 module_count:2;
			u8 module_result:2;
		} __packed((packed));
		unsigned char data[1];
	};
};
#define TEST_OCTA_MODULE	1
#define TEST_OCTA_ASSAY		2

#define TEST_OCTA_NONE		0
#define TEST_OCTA_FAIL		1
#define TEST_OCTA_PASS		2

#endif /* SEC_FACTORY_TEST */

#define TSP_NORMAL_EVENT_MSG 1
static int m_ts_debug_mode = ZINITIX_DEBUG;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *tsp_cb, bool mode);
};

static bool g_ta_connected;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#define SECURE_TOUCH_ENABLED	1
#define SECURE_TOUCH_DISABLED	0
static bool old_ta_status;
#endif

union zt7538_setting {
	u16 optional_mode;
	struct select_mode {
		u8 flag;
		u8 cover_type;
	} select_mode;
};

union zt7538_setting m_optional_mode;
union zt7538_setting m_prev_optional_mode;

struct zt7538_lpm_setting {
	u8 flag;
	u8 data;
};

#if ESD_TIMER_INTERVAL
static struct workqueue_struct *esd_tmr_workqueue;
#endif

struct coord {
	u16 x;
	u16 y;
	u8 width;
	u8 sub_status;
#if (TOUCH_POINT_MODE == 2)
	u8 minor_width;
	u8 angle;
#endif
};

#define POWER_STATE_OFF	0x00
#define POWER_STATE_LPM	0x01
#define POWER_STATE_NPM	0x02
#define POWER_STATE_ON	POWER_STATE_NPM

#define TOUCH_V_FLIP	0x01
#define TOUCH_H_FLIP	0x02
#define TOUCH_XY_SWAP	0x04

struct capa_info {
	u16 vendor_id;
	u16 ic_revision;
	u16 fw_version;
	u16 fw_minor_version;
	u16 reg_data_version;
	u16 threshold;
	u16 key_threshold;
	u16 dummy_threshold;
	u32 ic_fw_size;
	u32 MaxX;
	u32 MaxY;
	u8 gesture_support;
	u16 multi_fingers;
	u16 button_num;
	u16 ic_int_mask;
	u16 x_node_num;
	u16 y_node_num;
	u16 total_node_num;
	u16 hw_id;
	u16 afe_frequency;
	u16 shift_value;
	u16 mutual_amp_v_sel;
	u16 N_cnt;
	u16 u_cnt;
	u16 is_zmt200;
	u16 sx_amp_v_sel;
	u16 sx_sub_v_sel;
	u16 sy_amp_v_sel;
	u16 sy_sub_v_sel;
	u16 current_touch_mode;
};

enum work_state {
	NOTHING = 0,
	NORMAL,
	ESD_TIMER,
	EALRY_SUSPEND,
	SUSPEND,
	RESUME,
	LATE_RESUME,
	UPGRADE,
	REMOVE,
	SET_MODE,
	HW_CALIBRAION,
	RAW_DATA,
	PROBE,
	SLEEP_MODE_IN,
	SLEEP_MODE_OUT,
};

enum {
	BUILT_IN = 0,
	UMS,
	REQ_FW,
};

struct gesture_info {
	u8 eid:2;
	u8 gesture_type:4;
	u8 sf:2;
	u8 gesture_id;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 reserved_byte_5;
	u8 reserved_byte_6;
	u8 left_event:5;
	u8 reservied_vyte_7:3;
	u8 reserved_byte_8;
	u8 reserved_byte_9;
	u8 reserved_byte_10;
	u8 reserved_byte_11;
	u8 reserved_byte_12;
	u8 reserved_byte_13;
	u8 reserved_byte_14;
	u8 reserved_byte_15;
} __packed;

struct point_info {
	u8 eid:2;
	u8 tid:4;
	u8 touch_status:2;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 major;
	u8 minor;
	u8 z:6;
	u8 touch_type_3_2:2;
	u8 left_event:5;
	u8 max_enerty:1;
	u8 touch_type_1_0:2;
	u8 noise_level;
	u8 max_sensitivity;
	u8 hover_id_num:4;
	u8 location_area_3_0:4;
	u8 reserved_byte_11;
	u8 reserved_byte_12;
	u8 reserved_byte_13;
	u8 reserved_byte_14;
	u8 reserved_byte_15;
} __packed;

struct ts_coordinate {
	u8 id;
	u8 ttype;
	u8 touch_status;
	u16 x;
	u16 y;
	u8 z;
	u8 hover_flag;
	u8 glove_flag;
	u8 touch_height;
	u16 mcount;
	u8 major;
	u8 minor;
	u8 noise;
	u8 max_sense;
	bool palm;
	int palm_count;
	u8 left_event;
};

enum EVENT_ID {
	COORDINATE_EVENT = 0,
	STATUS_EVENT,
	GESTURE_EVENT,
	CUSTOM_EVENT,
};

enum TOUCH_STATUS_TYPE {
	FINGER_NONE = 0,
	FINGER_PRESS,
	FINGER_MOVE,
	FINGER_RELEASE,
};


enum TOUCH_TYPE {
	TOUCH_NOTYPE = -1,
	TOUCH_NORMAL,
	TOUCH_HOVER,
	TOUCH_FLIP_COVER,
	TOUCH_GLOVE,
	TOUCH_STYLUS,
	TOUCH_PALM,
	TOUCH_WET,
	TOUCH_PROXIMITY,
	TOUCH_JIG,
};

enum GESTURE_TYPE {
	SWIPE_UP = 0,
	DOUBLE_TAP,
	PRESSURE,
	FINGERPRINT,
	SINGLE_TAP,
	GESTURE_NONE = 255,
};

enum GESTURE_ID {
	ID_SWIPE_UP = 0,
	ID_AOD_DOUBLE_TAP = 0,
	ID_DOUBLE_TAP_TO_WAKEUP = 1,
	ID_PRESSURE_KEY_PRESS = 0,
	ID_PRESSURE_KEY_RELEASE = 1,
	ID_FOD_LONG_PRESS = 0,
	ID_FOD_PRESS = 1,
	ID_FOD_RELEASE = 2,
	ID_SINGLE_TAP = 0,
};

struct zt75xx_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct zt75xx_ts_platform_data *pdata;
	char phys[32];
	/*struct task_struct *task;*/
	/*wait_queue_head_t wait;*/

	/*struct semaphore update_lock;*/
	/*u32 i2c_dev_addr;*/
	struct capa_info cap_info;
	struct point_info touch_info[MAX_SUPPORTED_FINGER_NUM];
	struct ts_coordinate cur_coord[MAX_SUPPORTED_FINGER_NUM];
	struct ts_coordinate old_coord[MAX_SUPPORTED_FINGER_NUM];
	unsigned char *fw_data;

	int enabled;
	struct regulator *avdd;

	u16 fw_ver_bin;
	u16 fw_minor_ver_bin;
	u16 fw_reg_ver_bin;
	u16 fw_hw_id_bin;
	u16 fw_ic_revision_bin;

	struct zt7538_lpm_setting lpm_mode_reg; /* ic setting */
	u8 lowpower_mode; /* AP setting */

	u16 icon_event_reg;
	u16 prev_icon_event;
	/*u16 event_type;*/
	int irq;
	u8 button[MAX_SUPPORTED_BUTTON_NUM];
	u8 work_state;
	u8 power_state;
	volatile int display_state;
	struct semaphore work_lock;
	u8 finger_cnt1;
	unsigned int move_count[MAX_SUPPORTED_FINGER_NUM];
	struct mutex set_reg_lock;
	struct mutex modechange;

	/*u16 debug_reg[8];*/ /* for debug */
	void (*register_cb)(void *callbacks);
	struct tsp_callbacks callbacks;

#if ESD_TIMER_INTERVAL
	struct work_struct tmr_work;
	struct timer_list esd_timeout_tmr;
	struct timer_list *p_esd_timeout_tmr;
	spinlock_t lock;
#endif
	struct semaphore raw_data_lock;
	u16 touch_mode;
	s16 cur_data[MAX_TRAW_DATA_SZ];
	s16 sensitivity_data[TOUCH_SENTIVITY_MEASUREMENT_COUNT];
	u8 update;

#ifdef SEC_FACTORY_TEST
	struct tsp_raw_data *raw_data;
	struct sec_cmd_data sec;
#endif

	int noise_flag;
	int flip_cover_flag;
	int flip_status;
	int flip_status_current;
	int change_flip_status;
	struct mutex switching_mutex;
	struct delayed_work switching_work;
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	struct notifier_block hall_ic_nb;
#endif
#if IS_ENABLED(CONFIG_SUPPORT_SENSOR_FOLD)
	struct notifier_block hall_ic_nb_ssh;
#endif
#endif
	struct notifier_block nb;
	int main_tsp_status;

	struct delayed_work work_read_info;
	bool info_work_done;

	struct delayed_work ghost_check;
	u8 tsp_dump_lock;

	struct delayed_work work_print_info;
	u32 print_info_cnt_open;
	u32 print_info_cnt_release;

	struct completion resume_done;
	struct wakeup_source *wakelock;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
#endif

	struct ts_test_result	test_result;

	s16 Gap_max_x;
	s16 Gap_max_y;
	s16 Gap_max_val;
	s16 Gap_min_x;
	s16 Gap_min_y;
	s16 Gap_min_val;
	s16 Gap_Gap_val;
	s16 Gap_node_num;
	struct pinctrl *pinctrl;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
#endif
	u8 cover_type;
	bool flip_enable;
	bool glove_touch;

	bool aot_enable;

	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

	u8 check_multi;
	unsigned int multi_count;
	unsigned int wet_count;
	bool wet_mode;
	u8 touched_finger_num;
	unsigned int comm_err_count;
	u16 pressed_x[MAX_SUPPORTED_FINGER_NUM];
	u16 pressed_y[MAX_SUPPORTED_FINGER_NUM];

	struct sec_tclm_data *tdata;
	bool is_cal_done;
	int debug_flag;
	u8 ito_test[4];

	u16 store_reg_data;
	u8 *lp_dump;
	int lp_dump_index;
	int lp_dump_readmore;
};


#define TS_DRVIER_VERSION	"1.0.18_1"

#define ZT75XX_TS_DEVICE	"zt75xx_ts"

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include <../../../../sec_input/sec_secure_touch.h>
#endif

#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
//#define TCLM_CONCEPT
#endif

/* TCLM_CONCEPT */
#define ZT75XX_TS_NVM_OFFSET_FAC_RESULT			0
#define ZT75XX_TS_NVM_OFFSET_DISASSEMBLE_COUNT		2

#define ZT75XX_TS_NVM_OFFSET_CAL_COUNT			4
#define ZT75XX_TS_NVM_OFFSET_TUNE_VERSION		5
#define ZT75XX_TS_NVM_OFFSET_CAL_POSITION		7
#define ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT	8
#define ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP	9
#define ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO		10
#define ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_SIZE		20

#define ZT75XX_TS_NVM_OFFSET_LENGTH		(ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO + ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_SIZE)

#define ZT75XX_TS_SPONGE_LP_DUMP_LENGTH		100
#define ZT75XX_TS_SPONGE_LP_DUMP_1DATA_LEN	12

enum switch_system_mode {
	TO_TOUCH_MODE			= 0,
	TO_LOWPOWER_MODE		= 1,
};

struct zt75xx_ts_platform_data {
	u32 irq_gpio;
	u32 gpio_int;
	u32 gpio_scl;
	u32 gpio_sda;
	u32 gpio_ldo_en;
	int gpio_vdd;
	int (*tsp_power)(void *data, bool on);
	u16 x_resolution;
	u16 y_resolution;
	u8 x_channel;
	u8 y_channel;
	u8 area_indicator;
	u8 area_navigation;
	u8 area_edge;
	u16 page_size;
	u8 orientation;
	bool support_spay;
	bool support_aod;
	bool support_aot;
	bool support_lpm_mode;
	int bringup;
	bool mis_cal_check;
	bool support_hall_ic;
	int support_sensor_hall;
	bool use_deep_sleep;
	u16 pat_function;
	u16 afe_base;
	const char *project_name;
	void (*register_cb)(void *callbacks);

	const char *regulator_dvdd;
	const char *regulator_avdd;
	const char *regulator_tkled;
	const char *firmware_name;
	const char *chip_name;
	struct pinctrl *pinctrl;
	int item_version;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	int ss_touch_num;
#endif
	bool enable_sysinput_enabled;
};

static int  zt75xx_ts_open(struct input_dev *dev);
static void zt75xx_ts_close(struct input_dev *dev);
static int zt75xx_ts_set_lowpowermode(struct zt75xx_ts_info *info, u8 mode);

static bool zt75xx_power_control(struct zt75xx_ts_info *info, u8 ctl);
static int zt75xx_pinctrl_configure(struct zt75xx_ts_info *info, bool active);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
static void zinitix_chk_tsp_ic_status(struct zt75xx_ts_info *info, int call_pos);
#endif

static bool init_touch(struct zt75xx_ts_info *info);
static bool mini_init_touch(struct zt75xx_ts_info *info);
static void clear_report_data(struct zt75xx_ts_info *info);
#if ESD_TIMER_INTERVAL
static void esd_timer_start(u16 sec, struct zt75xx_ts_info *info);
static void esd_timer_stop(struct zt75xx_ts_info *info);
static void esd_timer_init(struct zt75xx_ts_info *info);
static void esd_timeout_handler(struct timer_list *t);
#endif

#ifdef TCLM_CONCEPT
int get_zt_tsp_nvm_data(struct zt75xx_ts_info *info, u8 addr, u8 *values, u16 length);
int set_zt_tsp_nvm_data(struct zt75xx_ts_info *info, u8 addr, u8 *values, u16 length);
#endif

#if IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
struct zt75xx_ts_info *tui_tsp_info;
extern int tui_force_close(uint32_t arg);
#endif

extern struct class *sec_class;

void tsp_charger_infom(bool en);

#endif /* LINUX_ZT75XX_TS_H */
