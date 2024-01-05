/*
 *
 * Zinitix zt touchscreen driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */


#undef TSP_VERBOSE_DEBUG

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/timekeeping.h>

#include "zinitix_ts.h"
#include "sec_input.h"

#define CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M
#define CONFIG_INPUT_ENABLED

#define MAX_FW_PATH 255

#define TSP_PATH_EXTERNAL_FW		"/sdcard/Firmware/TSP/tsp.bin"
#define TSP_PATH_EXTERNAL_FW_SIGNED	"/sdcard/Firmware/TSP/tsp_signed.bin"
#define TSP_PATH_SPU_FW_SIGNED		"/spu/TSP/ffu_tsp.bin"

#define TSP_TYPE_BUILTIN_FW			0
#define TSP_TYPE_EXTERNAL_FW			1
#define TSP_TYPE_EXTERNAL_FW_SIGNED		2
#define TSP_TYPE_SPU_FW_SIGNED			3

#define ZINITIX_DEBUG					0
#define PDIFF_DEBUG					1
#define USE_MISC_DEVICE

/* added header file */

#define TOUCH_POINT_MODE			0
#define MAX_SUPPORTED_FINGER_NUM	10 /* max 10 */

/* if you use isp mode, you must add i2c device :
   name = "zinitix_isp" , addr 0x50*/

/* resolution offset */
#define ABS_PT_OFFSET			(-1)

#define USE_CHECKSUM			1
#define CHECK_HWID			0

#define CHIP_OFF_DELAY			300 /*ms*/
#define CHIP_ON_DELAY			100 /*ms*/
#define FIRMWARE_ON_DELAY		150 /*ms*/

#define DELAY_FOR_SIGNAL_DELAY		30 /*us*/
#define DELAY_FOR_TRANSCATION		50
#define DELAY_FOR_POST_TRANSCATION	10

#define CMD_RESULT_WORD_LEN	10

enum power_control {
	POWER_OFF,
	POWER_ON,
	POWER_ON_SEQUENCE,
};

/* ESD Protection */
/*second : if 0, no use. if you have to use, 3 is recommended*/
#define ESD_TIMER_INTERVAL			0
#define SCAN_RATE_HZ				1000
#define CHECK_ESD_TIMER				5

/*Test Mode (Monitoring Raw Data) */
#define TSP_INIT_TEST_RATIO  100

#define	SEC_MUTUAL_AMP_V_SEL	0x0232

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

#define SEC_SY_SAT_FREQUENCY	200
#define SEC_SY_SAT_N_COUNT		9
#define SEC_SY_SAT_U_COUNT		9
#define SEC_SY_SAT_RS0_TIME		0x00FF
#define SEC_SY_SAT_RBG_SEL		0x0404
#define SEC_SY_SAT_AMP_V_SEL	0x0434
#define SEC_SY_SAT_SUB_V_SEL	0x0044

#define SEC_SY_SAT2_FREQUENCY	200
#define SEC_SY_SAT2_N_COUNT		9
#define SEC_SY_SAT2_U_COUNT		3
#define SEC_SY_SAT2_RS0_TIME	0x00FF
#define SEC_SY_SAT2_RBG_SEL		0x0404
#define SEC_SY_SAT2_AMP_V_SEL	0x0434
#define SEC_SY_SAT2_SUB_V_SEL	0x0011

#define MAX_RAW_DATA_SZ				792 /* 36x22 */  /* need to read from ic */
#define MAX_TRAW_DATA_SZ	\
	(MAX_RAW_DATA_SZ + 4 * MAX_SUPPORTED_FINGER_NUM + 2)

#define RAWDATA_DELAY_FOR_HOST		20000

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
#define TOUCH_RAW_MODE				7
#define TOUCH_REFERENCE_MODE			8
#define TOUCH_DND_MODE				11
#define TOUCH_HFDND_MODE			12
#define TOUCH_TXSHORT_MODE			13
#define TOUCH_RXSHORT_MODE			14
#define TOUCH_CHANNEL_TEST_MODE			14
#define TOUCH_JITTER_MODE			15
#define TOUCH_SELF_DND_MODE			17
#define TOUCH_SENTIVITY_MEASUREMENT_MODE	21
#define TOUCH_CHARGE_PUMP_MODE			25
#define TOUCH_REF_ABNORMAL_TEST_MODE		33
#define DEF_RAW_SELF_SSR_DATA_MODE		39	/* SELF SATURATION RX */
#define TOUCH_AGING_MODE		40
#define TOUCH_AMP_CHECK_MODE	50

#define TOUCH_SENTIVITY_MEASUREMENT_COUNT	9

/*  Other Things */
#define INIT_RETRY_CNT				3
#define I2C_SUCCESS					0
#define I2C_FAIL					1

/*---------------------------------------------------------------------*/

/* chip code */
#define BT43X_CHIP_CODE		0xE200
#define BT53X_CHIP_CODE		0xF400
#define ZT7548_CHIP_CODE	0xE548
#define ZT7538_CHIP_CODE	0xE538
#define ZT7532_CHIP_CODE	0xE532
#define ZT7554_CHIP_CODE	0xE700
#define ZT7650_CHIP_CODE	0xE650
#define ZT7650M_CHIP_CODE	0x650E

/////////////////////////////////////////////////////
//[Judge download type]
/////////////////////////////////////////////////////
#define NEED_FULL_DL			0
#define NEED_PARTIAL_DL_CUSTOM		1
#define NEED_PARTIAL_DL_REG		2

/////////////////////////////////////////////////////
//[VCMD]
/////////////////////////////////////////////////////

#define VCMD_UPGRADE_PART_ERASE_START			0x01DA

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650) || defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
#define VCMD_UPGRADE_INIT_FLASH				0x20F0
#define VCMD_UPGRADE_WRITE_FLASH			0x21F0
#define VCMD_UPGRADE_READ_FLASH				0x22F0
#define VCMD_UPGRADE_MODE				0x25F0
#define VCMD_UPGRADE_WRITE_MODE				0x27F0
#define VCMD_UPGRADE_MASS_ERASE				0x28F0
#define VCMD_UPGRADE_START_PAGE				0x29F0
#define VCMD_UPGRADE_BLOCK_ERASE			0x2AF0

#define VCMD_NVM_PROG_START				0x11F0
#define VCMD_NVM_INIT					0x12F0
#define VCMD_NVM_WRITE_ENABLE				0x13F0
#define VCMD_INTN_CLR					0x14F0
#define VCMD_OSC_FREQ_SEL					0x1AF0

#define VCMD_ID						0x17F0
#define VCMD_ENABLE					0x10F0
#define VCMD_NVM_ERASE					0x2AF0
#define VCMD_NVM_WRITE					0x34F0
#define VCMD_REG_READ					0x18F0
#define VCMD_REG_WRITE					0x19F0
#else
#define VCMD_UPGRADE_INIT_FLASH				0x01D0
#define VCMD_UPGRADE_WRITE_FLASH			0x01D1
#define VCMD_UPGRADE_READ_FLASH				0x01D2
#define VCMD_UPGRADE_MODE				0x01D5
#define VCMD_UPGRADE_WRITE_MODE				0x01DE
#define VCMD_UPGRADE_MASS_ERASE				0x01DF

#define VCMD_NVM_PROG_START				0xC001
#define VCMD_NVM_INIT					0xC002
#define VCMD_NVM_WRITE_ENABLE				0xC003
#define VCMD_INTN_CLR					0xC004
#define VCMD_OSC_FREQ_SEL					0xC201

#define VCMD_ID						0xCC00
#define VCMD_ENABLE					0xC000
#define VCMD_NVM_ERASE					0xC10D
#define VCMD_NVM_WRITE					0xC10B

#define VCMD_REG_READ					0xCC01
#define VCMD_REG_WRITE					0xCC02
#endif

/* Register Map*/
#define ZT_SWRESET_CMD					0x0000
#define ZT_WAKEUP_CMD					0x0001
#define ZT_IDLE_CMD					0x0004
#define ZT_SLEEP_CMD					0x0005
#define ZT_CLEAR_INT_STATUS_CMD				0x0003
#define ZT_CALIBRATE_CMD				0x0006
#define ZT_SAVE_STATUS_CMD				0x0007
#define ZT_SAVE_CALIBRATION_CMD				0x0008
#define ZT_RECALL_FACTORY_CMD				0x000f
#define ZT_THRESHOLD					0x0020
#define ZT_DEBUG_REG					0x0115
#define ZT_TOUCH_MODE					0x0010
#define ZT_CHIP_REVISION				0x0011
#define ZT_FIRMWARE_VERSION				0x0012
#define ZT_MINOR_FW_VERSION				0x0121
#define ZT_VENDOR_ID					0x001C
#define ZT_HW_ID					0x0014
#define ZT_DATA_VERSION_REG				0x0013
#define ZT_SUPPORTED_FINGER_NUM				0x0015
#define ZT_EEPROM_INFO					0x0018
#define ZT_INITIAL_TOUCH_MODE				0x0019
#define ZT_TOTAL_NUMBER_OF_X				0x0061
#define ZT_TOTAL_NUMBER_OF_Y				0x0060
#define ZT_CONNECTION_CHECK_REG				0x0062
#define ZT_POWER_STATE_FLAG				0x007E
#define ZT_DELAY_RAW_FOR_HOST				0x007f
#define ZT_BUTTON_SUPPORTED_NUM				0x00B0
#define ZT_BUTTON_SENSITIVITY				0x00B2
#define ZT_DUMMY_BUTTON_SENSITIVITY			0X00C8
#define ZT_X_RESOLUTION					0x00C0
#define ZT_Y_RESOLUTION					0x00C1
#define ZT_CALL_AOT_REG				0x00D3
#define ZT_STATUS_REG				0x0080
#define ZT_POINT_STATUS_REG				0x0200
#define ZT_POINT_STATUS_REG1				0x0201
#define ZT_FOD_STATUS_REG				0x020A
#define ZT_VI_STATUS_REG				0x020B

#define ZT_OSC_TIMER_LSB			0x019F
#define ZT_OSC_TIMER_MSB			0x01A0

#define ZT_ICON_STATUS_REG				0x00AA
#define ZT_JITTER_RESULT				0x01A3

#define ZT_SET_AOD_X_REG				0x00AB
#define ZT_SET_AOD_Y_REG				0x00AC
#define ZT_SET_AOD_W_REG				0x00AD
#define ZT_SET_AOD_H_REG				0x00AE
#define ZT_LPM_MODE_REG					0x00AF

#define ZT_GET_AOD_X_REG				0x0191
#define ZT_GET_AOD_Y_REG				0x0192

#define ZT_GET_FOD_WITH_FINGER_PACKET			0x019A
#define ZT_SET_SIP_MODE					0x019D

#define ZT_DND_SHIFT_VALUE				0x012B
#define ZT_AFE_FREQUENCY					0x0100
#define ZT_DND_N_COUNT					0x0122
#define ZT_DND_U_COUNT					0x0135

#define ZT_RAWDATA_REG					0x0200

#define ZT_INT_ENABLE_FLAG				0x00f0
#define ZT_PERIODICAL_INTERRUPT_INTERVAL	0x00f1
#define ZT_BTN_WIDTH						0x0316
#define ZT_REAL_WIDTH					0x03A6

#define ZT_CHECKSUM_RESULT				0x012c


#define ZINITIX_INTERNAL_FLAG_03		0x011f

#define ZT_OPTIONAL_SETTING				0x0116

#define ZT_SET_WIRELESSCHARGER_MODE		0x0199
#define ZT_SET_NOTE_MODE			0x019B
#define ZT_SET_GAME_MODE			0x019C

#define ZT_SET_SCANRATE				0x01A0
#define ZT_SET_SCANRATE_ENABLE			0x01A1
#define ZT_VSYNC_TEST_RESULT			0x01A2

#define ZT_COVER_CONTROL_REG			0x023E

#define ZT_REJECT_ZONE_AREA			0x01AD

#define	ZT_EDGE_LANDSCAPE_MODE					0x0038
#define	ZT_EDGE_REJECT_PORT_SIDE_UP_DOWN_DIV	0x0039
#define	ZT_EDGE_REJECT_PORT_SIDE_UP_WIDTH		0x003A
#define	ZT_EDGE_REJECT_PORT_SIDE_DOWN_WIDTH		0x003E
#define	ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_SEL		0x003F
#define	ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_START	0x0040
#define	ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_END		0x0041
//#define	ZT_EDGE_GRIP_PORT_TOP_BOT_WIDTH			0x0042
#define	ZT_EDGE_GRIP_PORT_SIDE_WIDTH			0x0045
#define	ZT_EDGE_REJECT_LAND_SIDE_WIDTH			0x0046
#define	ZT_EDGE_REJECT_LAND_TOP_BOT_WIDTH		0x0047
#define	ZT_EDGE_GRIP_LAND_SIDE_WIDTH			0x0048
#define	ZT_EDGE_GRIP_LAND_TOP_BOT_WIDTH			0x0049

#define ZT_RESOLUTION_EXPANDER			0x0186
#define ZT_MUTUAL_AMP_V_SEL			0x02F9
#define ZT_SX_AMP_V_SEL				0x02DF
#define ZT_SX_SUB_V_SEL				0x02E0
#define ZT_SY_AMP_V_SEL				0x02EC
#define ZT_SY_SUB_V_SEL				0x02ED
#define ZT_CHECKSUM					0x03DF
#define ZT_JITTER_SAMPLING_CNT			0x001F

#define ZT_SY_SAT_FREQUENCY			0x03E0
#define ZT_SY_SAT_N_COUNT			0x03E1
#define ZT_SY_SAT_U_COUNT			0x03E2
#define ZT_SY_SAT_RS0_TIME			0x03E3
#define ZT_SY_SAT_RBG_SEL			0x03E4
#define ZT_SY_SAT_AMP_V_SEL			0x03E5
#define ZT_SY_SAT_SUB_V_SEL			0x03E6

#define ZT_SY_SAT2_FREQUENCY		0x03E7
#define ZT_SY_SAT2_N_COUNT			0x03E8
#define ZT_SY_SAT2_U_COUNT			0x03E9
#define ZT_SY_SAT2_RS0_TIME			0x03EA
#define ZT_SY_SAT2_RBG_SEL			0x03EB
#define ZT_SY_SAT2_AMP_V_SEL		0x03EC
#define ZT_SY_SAT2_SUB_V_SEL		0x03ED

#define ZT_PROXIMITY_XDATA			0x030E
#define ZT_PROXIMITY_YDATA			0x030F
#define ZT_PROXIMITY_DETECT			0x0024
#define ZT_PROXIMITY_THRESHOLD		0x023F
#define ABS_MT_CUSTOM2		0x3f	/* custom event only for sensor */


#define ZT_POCKET_DETECT			0x0037

#define CORRECT_CHECK_SUM			0x55AA

#define REG_FOD_AREA_STR_X				0x013B
#define REG_FOD_AREA_STR_Y				0x013C
#define REG_FOD_AREA_END_X				0x013E
#define REG_FOD_AREA_END_Y				0x013F

#define REG_FOD_MODE_SET				0x0142

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
#define BIT_POCKET_MODE		8
#define BIT_PT_NO_CHANGE	9
#define BIT_REJECT		10
#define BIT_PT_EXIST		11
#define BIT_PROXIMITY		12
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

/* zt_DEBUG_REG */
#define DEF_DEVICE_STATUS_NPM			0
#define DEF_DEVICE_STATUS_WALLET_COVER_MODE	1
#define DEF_DEVICE_STATUS_NOISE_MODE		2
#define DEF_DEVICE_STATUS_WATER_MODE		3
#define DEF_DEVICE_STATUS_LPM__MODE		4
#define BIT_GLOVE_TOUCH				5
#define DEF_DEVICE_STATUS_PALM_DETECT		10
#define DEF_DEVICE_STATUS_SVIEW_MODE		11

/* zt_zt_COVER_CONTROL_REG */
#define WALLET_COVER_CLOSE	0x0000
#define VIEW_COVER_CLOSE	0x0100
#define COVER_OPEN			0x0200
#define LED_COVER_CLOSE		0x0700
#define CLEAR_COVER_CLOSE	0x0800
#define CLEAR_SIDE_VIEW_COVER_CLOSE	0x0F00
#define MINI_SVIEW_WALLET_COVER_CLOSE	0x1000

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
	ZT_CLEAR_SIDE_VIEW_COVER = 15,
	ZT_MINI_SVIEW_WALLET_COVER = 16,
	ZT_MONTBLANC_COVER = 100,
};

#define zinitix_bit_set(val, n)		((val) &= ~(1<<(n)), (val) |= (1<<(n)))
#define zinitix_bit_clr(val, n)		((val) &= ~(1<<(n)))
#define zinitix_bit_test(val, n)	((val) & (1<<(n)))
#define zinitix_swap_v(a, b, t)		((t) = (a), (a) = (b), (b) = (t))
#define zinitix_swap_16(s)			(((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)))

/* REG_USB_STATUS : optional setting from AP */
#define DEF_OPTIONAL_MODE_USB_DETECT_BIT		0
#define	DEF_OPTIONAL_MODE_SVIEW_DETECT_BIT		1
#define DEF_OPTIONAL_MODE_SENSITIVE_BIT		2
#define DEF_OPTIONAL_MODE_EDGE_SELECT			3
#define	DEF_OPTIONAL_MODE_DUO_TOUCH		4
#define DEF_OPTIONAL_MODE_TOUCHABLE_AREA		5
#define DEF_OPTIONAL_MODE_EAR_DETECT		6
#define DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL		7
#define DEF_OPTIONAL_MODE_POCKET_MODE		8
#define DEF_OPTIONAL_MODE_OTG_MODE		15

/* end header file */

#define DEF_MIS_CAL_SPEC_MIN 40
#define DEF_MIS_CAL_SPEC_MAX 160
#define DEF_MIS_CAL_SPEC_MID 100
#define ZT_MIS_CAL_SET		0x00D0
#define ZT_MIS_CAL_RESULT	0x00D1

#define TOUCH_PRINT_INFO_DWORK_TIME 30000 /* 30 secs */

/* Touch Screen */
#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN		3264	//34*16*6
#define TSP_CMD_PARAM_NUM		8
#define TSP_CMD_X_NUM			34
#define TSP_CMD_Y_NUM			16
#define TSP_CMD_NODE_NUM		(TSP_CMD_Y_NUM * TSP_CMD_X_NUM)
#define tostring(x) #x

struct tsp_raw_data {
	s16 cnd_data[TSP_CMD_NODE_NUM];
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
	s16 self_vgap_data[TSP_CMD_NODE_NUM];
	s16 self_hgap_data[TSP_CMD_NODE_NUM];
	s16 jitter_data[TSP_CMD_NODE_NUM];
	s16 amp_check_data[TSP_CMD_NODE_NUM];
	s16 reference_data[TSP_CMD_NODE_NUM];
	s16 reference_data_abnormal[TSP_CMD_NODE_NUM];
	s16 charge_pump_data[TSP_CMD_NODE_NUM];
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
		} __attribute__((packed));
		unsigned char data[1];
	};
};
#define TEST_OCTA_MODULE	1
#define TEST_OCTA_ASSAY		2

#define TEST_OCTA_NONE		0
#define TEST_OCTA_FAIL		1
#define TEST_OCTA_PASS		2

#define TSP_NORMAL_EVENT_MSG	1
static int m_ts_debug_mode = ZINITIX_DEBUG;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *tsp_cb, bool mode);
};

static bool g_ta_connected =0;
typedef union {
	u16 optional_mode;
	struct select_mode {
		u16 flag;
	} select_mode;
} zt_setting;

#if ESD_TIMER_INTERVAL
static struct workqueue_struct *esd_tmr_workqueue;
#endif

#define TOUCH_V_FLIP	0x01
#define TOUCH_H_FLIP	0x02
#define TOUCH_XY_SWAP	0x04

#define RETRY_CNT	3
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
	u16 n_cnt;
	u16 u_cnt;
	u16 is_zmt200;
	u16 sx_amp_v_sel;
	u16 sx_sub_v_sel;
	u16 sy_amp_v_sel;
	u16 sy_sub_v_sel;
	u16 current_touch_mode;
	u8 gesture_support;
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
	SPU,
};

struct zt_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *input_dev_pad;
	struct input_dev *input_dev_proximity;
	struct zt_ts_platform_data *pdata;
	char phys[32];
	struct capa_info cap_info;
	struct point_info touch_info[MAX_SUPPORTED_FINGER_NUM];
	struct point_info touch_fod_info;
	struct ts_coordinate cur_coord[MAX_SUPPORTED_FINGER_NUM];
	struct ts_coordinate old_coord[MAX_SUPPORTED_FINGER_NUM];
	unsigned char *fw_data;
	u16 icon_event_reg;
	u16 prev_icon_event;
	int irq;
	u8 work_state;
	u8 finger_cnt1;
	unsigned int move_count[MAX_SUPPORTED_FINGER_NUM];
	struct mutex set_reg_lock;
	struct mutex set_lpmode_lock;
	struct mutex modechange;
	struct mutex work_lock;
	struct mutex raw_data_lock;
	struct mutex i2c_mutex;
	struct mutex sponge_mutex;
	struct mutex power_init;

	void (*register_cb)(void *);
	struct tsp_callbacks callbacks;

#if ESD_TIMER_INTERVAL
	struct work_struct tmr_work;
	struct timer_list esd_timeout_tmr;
	struct timer_list *p_esd_timeout_tmr;
	struct mutex lock;
#endif
	u16 touch_mode;
	s16 cur_data[MAX_TRAW_DATA_SZ];
	s16 sensitivity_data[TOUCH_SENTIVITY_MEASUREMENT_COUNT];
	u8 update;

	struct tsp_raw_data *raw_data;
	struct delayed_work work_read_info;
	bool info_work_done;

	u8 tsp_dump_lock;

	struct completion resume_done;

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
	bool tsp_pwr_enabled;
	u8 cover_type;
	bool flip_enable;
	bool spay_enable;
	bool fod_enable;
	bool fod_lp_mode;
	bool singletap_enable;
	bool aod_enable;
	bool aot_enable;
	bool sleep_mode;
	bool glove_touch;
	u8 lpm_mode;
	zt_setting m_optional_mode;

	bool fod_with_finger_packet;
	u8 fod_mode_set;
	u8 fod_info_vi_trx[3];
	u16 fod_rect[4];

	u16 aod_rect[4];
	u16 aod_active_area[3];

	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

	u8 grip_edgehandler_direction;
	int grip_edgehandler_start_y;
	int grip_edgehandler_end_y;
	u16 grip_edge_range;
	u8 grip_deadzone_up_x;
	u8 grip_deadzone_dn_x;
	int grip_deadzone_y;
	u8 grip_landscape_mode;
	int grip_landscape_edge;
	u16 grip_landscape_deadzone;
	u16 grip_landscape_top_deadzone;
	u16 grip_landscape_bottom_deadzone;
	u16 grip_landscape_top_gripzone;
	u16 grip_landscape_bottom_gripzone;

	u8 check_multi;
	unsigned int multi_count;
	unsigned int wet_count;
	u8 touched_finger_num;
	struct delayed_work work_print_info;
	int print_info_cnt_open;
	int print_info_cnt_release;
	unsigned int comm_err_count;
	u16 pressed_x[MAX_SUPPORTED_FINGER_NUM];
	u16 pressed_y[MAX_SUPPORTED_FINGER_NUM];
	long prox_power_off;

	u8 ed_enable;
	int pocket_enable;
	u16 hover_event; /* keystring for protos */
	u16 store_reg_data;

	int noise_flag;
	int flip_cover_flag;

	u8 ito_test[4];
};

void zt_print_info(struct zt_ts_info *info);
bool shutdown_is_on_going_tsp;
static int ts_set_touchmode(u16 value);

#if ESD_TIMER_INTERVAL
static void esd_timer_stop(struct zt_ts_info *info);
static void esd_timer_start(u16 sec, struct zt_ts_info *info);
#endif

void zt_delay(int ms)
{
	if (ms > 20)
		msleep(ms);
	else
		usleep_range(ms * 1000, ms * 1000);
}

/* define i2c sub functions*/
static inline s32 read_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

	mutex_lock(&info->i2c_mutex);

retry:
	/* select register*/
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: send failed %d, retry %d\n", __func__, ret, count);
		zt_delay(1);

		if (++count < RETRY_CNT)
			goto retry;

		info->comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}
	/* for setup tx transaction. */
	usleep_range(DELAY_FOR_TRANSCATION, DELAY_FOR_TRANSCATION);
	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: recv failed %d\n", __func__, ret);
		info->comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	mutex_unlock(&info->i2c_mutex);
	return length;

I2C_ERROR:
	if (info->work_state == PROBE) {
		input_err(true, &client->dev,
				"%s work state is PROBE.\n", __func__);
		return ret;
	}
	if (info->work_state == ESD_TIMER) {
		input_err(true, &client->dev,
				"%s reset work queue be working.\n", __func__);
		return -EIO;
	}
	info->work_state = NOTHING;
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	queue_work(esd_tmr_workqueue, &info->tmr_work);
#endif
	return ret;
}

static inline s32 write_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;
	u8 pkt[66]; /* max packet */

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

	mutex_lock(&info->i2c_mutex);

	pkt[0] = (reg) & 0xff; /* reg addr */
	pkt[1] = (reg >> 8)&0xff;
	memcpy((u8 *)&pkt[2], values, length);

retry:
	ret = i2c_master_send(client , pkt , length + 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed %d, retry %d\n", __func__, ret, count);
		usleep_range(1 * 1000, 1 * 1000);

		if (++count < RETRY_CNT)
			goto retry;

		info->comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	mutex_unlock(&info->i2c_mutex);
	return length;
I2C_ERROR:
	if (info->work_state == PROBE) {
		input_err(true, &client->dev,
				"%s work state is PROBE.\n", __func__);
		return ret;
	}
	if (info->work_state == ESD_TIMER) {
		input_err(true, &client->dev,
				"%s reset work queue be working.\n", __func__);
		return -EIO;
	}
	info->work_state = NOTHING;
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	queue_work(esd_tmr_workqueue, &info->tmr_work);
#endif
	return ret;

}

static inline s32 write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	if (write_data(client, reg, (u8 *)&value, 2) < 0)
		return I2C_FAIL;

	return I2C_SUCCESS;
}

static inline s32 write_cmd(struct i2c_client *client, u16 reg)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

	mutex_lock(&info->i2c_mutex);

retry:
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed %d, retry %d\n", __func__, ret, count);
		zt_delay(1);

		if (++count < RETRY_CNT)
			goto retry;

		info->comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	mutex_unlock(&info->i2c_mutex);
	return I2C_SUCCESS;

I2C_ERROR:
	if (info->work_state == PROBE) {
		input_err(true, &client->dev,
				"%s work state is PROBE.\n", __func__);
		return ret;
	}
	if (info->work_state == ESD_TIMER) {
		input_err(true, &client->dev,
				"%s reset work queue be working.\n", __func__);
		return -EIO;
	}
	info->work_state = NOTHING;
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	queue_work(esd_tmr_workqueue, &info->tmr_work);
#endif
	return ret;

}

static inline s32 read_raw_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

	mutex_lock(&info->i2c_mutex);

retry:
	/* select register */
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: send failed %d, retry %d\n", __func__, ret, count);
		zt_delay(1);

		if (++count < RETRY_CNT)
			goto retry;

		info->comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	/* for setup tx transaction. */
	usleep_range(200, 200);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: recv failed %d\n", __func__, ret);
		info->comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	mutex_unlock(&info->i2c_mutex);
	return length;

I2C_ERROR:
	if (info->work_state == PROBE) {
		input_err(true, &client->dev,
				"%s work state is PROBE.\n", __func__);
		return ret;
	}
	if (info->work_state == ESD_TIMER) {
		input_err(true, &client->dev,
				"%s reset work be working.\n", __func__);
		return -EIO;
	}
	info->work_state = NOTHING;
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	queue_work(esd_tmr_workqueue, &info->tmr_work);
#endif
	return ret;
}

static inline s32 read_firmware_data(struct i2c_client *client,
		u16 addr, u8 *values, u16 length)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

	mutex_lock(&info->i2c_mutex);

	/* select register*/
	ret = i2c_master_send(client , (u8 *)&addr , 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: send failed %d\n", __func__, ret);
		info->comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		return ret;
	}

	/* for setup tx transaction. */
	zt_delay(1);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: recv failed %d\n", __func__, ret);
		info->comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		return ret;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	mutex_unlock(&info->i2c_mutex);
	return length;
}

static void zt_set_optional_mode(struct zt_ts_info *info, int event, bool enable)
{
	mutex_lock(&info->power_init);
	mutex_lock(&info->set_reg_lock);
	if (enable)
		zinitix_bit_set(info->m_optional_mode.select_mode.flag, event);
	else
		zinitix_bit_clr(info->m_optional_mode.select_mode.flag, event);

	if (write_reg(info->client, ZT_OPTIONAL_SETTING, info->m_optional_mode.optional_mode) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s, fail optional mode set\n", __func__);

	mutex_unlock(&info->set_reg_lock);
	mutex_unlock(&info->power_init);
}

static int ts_read_from_sponge(struct zt_ts_info *info, u16 offset, u8* value, int len)
{
	int ret = 0;
	u8 pkt[66];

	pkt[0] = offset & 0xFF;
	pkt[1] = (offset >> 8) & 0xFF;

	pkt[2] = len & 0xFF;
	pkt[3] = (len >> 8) & 0xFF;

	mutex_lock(&info->power_init);
	mutex_lock(&info->sponge_mutex);
	if (write_data(info->client, ZT_SPONGE_READ_REG, (u8 *)&pkt, 4) < 0) {
		input_err(true, &info->client->dev, "%s: fail to write sponge command\n", __func__);
		ret = -EIO;
	}

	if (read_data(info->client, ZT_SPONGE_READ_REG, value, len) < 0) {
		input_err(true, &info->client->dev, "%s: fail to read sponge command\n", __func__);
		ret = -EIO;
	}
	mutex_unlock(&info->sponge_mutex);
	mutex_unlock(&info->power_init);

	return ret;
}

static int ts_write_to_sponge(struct zt_ts_info *info, u16 offset, u8 *value, int len)
{
	int ret = 0;
	u8 pkt[66];

	mutex_lock(&info->power_init);
	mutex_lock(&info->sponge_mutex);

	pkt[0] = offset & 0xFF;
	pkt[1] = (offset >> 8) & 0xFF;

	pkt[2] = len & 0xFF;
	pkt[3] = (len >> 8) & 0xFF;

	memcpy((u8 *)&pkt[4], value, len);

	if (write_data(info->client, ZT_SPONGE_WRITE_REG, (u8 *)&pkt, len + 4) < 0) {
		input_err(true, &info->client->dev, "%s: Failed to write offset\n", __func__);
		ret = -EIO;
	}

	if (write_cmd(info->client, ZT_SPONGE_SYNC_REG) != I2C_SUCCESS) {
		input_err(true, &info->client->dev, "%s: Failed to send notify\n", __func__);
		ret = -EIO;
	}
	mutex_unlock(&info->sponge_mutex);
	mutex_unlock(&info->power_init);

	return ret;
}

static void ts_set_utc_sponge(struct zt_ts_info *info)
{
	struct timespec64 current_time;
	int ret;
	u8 data[4] = {0, 0};

	ktime_get_ts64(&current_time);
	data[0] = (0xFF & (u8)((current_time.tv_sec) >> 0));
	data[1] = (0xFF & (u8)((current_time.tv_sec) >> 8));
	data[2] = (0xFF & (u8)((current_time.tv_sec) >> 16));
	data[3] = (0xFF & (u8)((current_time.tv_sec) >> 24));
	input_info(true, &info->client->dev, "Write UTC to Sponge = %X\n", (int)(current_time.tv_sec));

	ret = ts_write_to_sponge(info, ZT_SPONGE_UTC, (u8*)&data, 4);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: Failed to write sponge\n", __func__);
}

int get_fod_info(struct zt_ts_info *info)
{
	u8 data[6] = {0, };
	int ret, i;

	ret = ts_read_from_sponge(info, ZT_SPONGE_AOD_ACTIVE_INFO, data, 6);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: Failed to read rect\n", __func__);
		return ret;
	}

	for (i = 0; i < 3; i++)
		info->aod_active_area[i] = (data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);

	input_info(true, &info->client->dev, "%s: top:%d, edge:%d, bottom:%d\n",
			__func__, info->aod_active_area[0], info->aod_active_area[1], info->aod_active_area[2]);

	return ret;
}

#ifdef CONFIG_INPUT_ENABLED
static int  zt_ts_open(struct input_dev *dev);
static void zt_ts_close(struct input_dev *dev);
#endif

static bool zt_power_control(struct zt_ts_info *info, u8 ctl);
static int zt_pinctrl_configure(struct zt_ts_info *info, bool active);

static bool init_touch(struct zt_ts_info *info);
static bool mini_init_touch(struct zt_ts_info *info);
static void clear_report_data(struct zt_ts_info *info);

#if ESD_TIMER_INTERVAL
static void esd_timer_init(struct zt_ts_info *info);
static void esd_timeout_handler(struct timer_list *t);
#endif

void zt_set_grip_type(struct zt_ts_info *info, u8 set_type);

void location_detect(struct zt_ts_info *info, char *loc, int x, int y);

#ifdef USE_MISC_DEVICE
static long ts_misc_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int ts_misc_fops_open(struct inode *inode, struct file *filp);
static int ts_misc_fops_close(struct inode *inode, struct file *filp);

static const struct file_operations ts_misc_fops = {
	.owner = THIS_MODULE,
	.open = ts_misc_fops_open,
	.release = ts_misc_fops_close,
	//.unlocked_ioctl = ts_misc_fops_ioctl,
	.compat_ioctl = ts_misc_fops_ioctl,
};

static struct miscdevice touch_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "zinitix_touch_misc",
	.fops = &ts_misc_fops,
};

#define TOUCH_IOCTL_BASE	0xbc
#define TOUCH_IOCTL_GET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 0, int)
#define TOUCH_IOCTL_SET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 1, int)
#define TOUCH_IOCTL_GET_CHIP_REVISION		_IOW(TOUCH_IOCTL_BASE, 2, int)
#define TOUCH_IOCTL_GET_FW_VERSION			_IOW(TOUCH_IOCTL_BASE, 3, int)
#define TOUCH_IOCTL_GET_REG_DATA_VERSION	_IOW(TOUCH_IOCTL_BASE, 4, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_SIZE		_IOW(TOUCH_IOCTL_BASE, 5, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_DATA		_IOW(TOUCH_IOCTL_BASE, 6, int)
#define TOUCH_IOCTL_START_UPGRADE			_IOW(TOUCH_IOCTL_BASE, 7, int)
#define TOUCH_IOCTL_GET_X_NODE_NUM			_IOW(TOUCH_IOCTL_BASE, 8, int)
#define TOUCH_IOCTL_GET_Y_NODE_NUM			_IOW(TOUCH_IOCTL_BASE, 9, int)
#define TOUCH_IOCTL_GET_TOTAL_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 10, int)
#define TOUCH_IOCTL_SET_RAW_DATA_MODE		_IOW(TOUCH_IOCTL_BASE, 11, int)
#define TOUCH_IOCTL_GET_RAW_DATA			_IOW(TOUCH_IOCTL_BASE, 12, int)
#define TOUCH_IOCTL_GET_X_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 13, int)
#define TOUCH_IOCTL_GET_Y_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 14, int)
#define TOUCH_IOCTL_HW_CALIBRAION			_IOW(TOUCH_IOCTL_BASE, 15, int)
#define TOUCH_IOCTL_GET_REG					_IOW(TOUCH_IOCTL_BASE, 16, int)
#define TOUCH_IOCTL_SET_REG					_IOW(TOUCH_IOCTL_BASE, 17, int)
#define TOUCH_IOCTL_SEND_SAVE_STATUS		_IOW(TOUCH_IOCTL_BASE, 18, int)
#define TOUCH_IOCTL_DONOT_TOUCH_EVENT		_IOW(TOUCH_IOCTL_BASE, 19, int)
#endif

struct zt_ts_info *misc_info;

static void zt_ts_esd_timer_stop(struct zt_ts_info *info)
{
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(info->client, ZT_CLEAR_INT_STATUS_CMD);
#endif
}

static void zt_ts_esd_timer_start(struct zt_ts_info *info)
{
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}
static void set_cover_type(struct zt_ts_info *info, bool enable)
{
	struct i2c_client *client = info->client;

	mutex_lock(&info->power_init);

	if (enable) {
		switch (info->cover_type) {
		case ZT_FLIP_WALLET:
			write_reg(client, ZT_COVER_CONTROL_REG, WALLET_COVER_CLOSE);
			break;
		case ZT_VIEW_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, VIEW_COVER_CLOSE);
			break;
		case ZT_CLEAR_FLIP_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, CLEAR_COVER_CLOSE);
			break;
		case ZT_NEON_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, LED_COVER_CLOSE);
			break;
		case ZT_CLEAR_SIDE_VIEW_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, CLEAR_SIDE_VIEW_COVER_CLOSE);
			break;
		case ZT_MINI_SVIEW_WALLET_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, MINI_SVIEW_WALLET_COVER_CLOSE);
			break;
		default:
			input_err(true, &info->client->dev, "%s: touch is not supported for %d cover\n",
					__func__, info->cover_type);
		}
	} else {
		write_reg(client, ZT_COVER_CONTROL_REG, COVER_OPEN);
	}

	mutex_unlock(&info->power_init);

	input_info(true, &info->client->dev, "%s: type %d enable %d\n", __func__, info->cover_type, enable);
}

static bool get_raw_data(struct zt_ts_info *info, u8 *buff, int skip_cnt)
{
	struct i2c_client *client = info->client;
	struct zt_ts_platform_data *pdata = info->pdata;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;
	int i, j = 0;

	disable_irq(info->irq);

	mutex_lock(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "other process occupied.. (%d)\n",
				info->work_state);
		enable_irq(info->irq);
		mutex_unlock(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(pdata->gpio_int)) {
			zt_delay(1);
			if (++j > 3000) {
				input_err(true, &info->client->dev, "%s: (skip_cnt) wait int timeout\n", __func__);
				break;
			}
		}

		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		zt_delay(1);
	}

	input_dbg(true, &info->client->dev, "%s read raw data\r\n", __func__);
	sz = total_node*2;

	j = 0;
	while (gpio_get_value(pdata->gpio_int)) {
		zt_delay(1);
		if (++j > 3000) {
			input_err(true, &info->client->dev, "%s: wait int timeout\n", __func__);
			break;
		}
	}

	if (read_raw_data(client, ZT_RAWDATA_REG, (char *)buff, sz) < 0) {
		input_info(true, &client->dev, "%s: error : read zinitix tc raw data\n", __func__);
		info->work_state = NOTHING;
		clear_report_data(info);
		enable_irq(info->irq);
		mutex_unlock(&info->work_lock);
		return false;
	}

	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	clear_report_data(info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	mutex_unlock(&info->work_lock);

	return true;
}

static bool ts_get_raw_data(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;

	if (!mutex_trylock(&info->raw_data_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy mutex\n", __func__);
		return true;
	}

	sz = total_node * 2 + sizeof(struct point_info) * MAX_SUPPORTED_FINGER_NUM;

	if (read_raw_data(info->client, ZT_RAWDATA_REG,
				(char *)info->cur_data, sz) < 0) {
		input_err(true, &client->dev, "%s: Failed to read raw data\n", __func__);
		mutex_unlock(&info->raw_data_lock);
		return false;
	}

	info->update = 1;
	memcpy((u8 *)(&info->touch_info[0]),
			(u8 *)&info->cur_data[total_node],
			sizeof(struct point_info) * MAX_SUPPORTED_FINGER_NUM);
	mutex_unlock(&info->raw_data_lock);

	return true;
}

static void zt_ts_fod_event_report(struct zt_ts_info *info, struct point_info touch_info)
{
	if (!info->fod_enable)
		return;

	if ((touch_info.byte01.value_u8bit == 0)
			 || (touch_info.byte01.value_u8bit == 1)) {
		info->scrub_id = SPONGE_EVENT_TYPE_FOD_PRESS;

		info->scrub_x = ((touch_info.byte02.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0xF0) >> 4);
		info->scrub_y = ((touch_info.byte03.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0x0F));

		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
		input_sync(info->input_dev);
		input_info(true, &info->client->dev, "%s: FOD %s PRESS: %d, %d, %d\n", __func__,
				touch_info.byte01.value_u8bit ? "NORMAL" : "LONG",
				info->scrub_id, info->scrub_x, info->scrub_y);
	} else if (touch_info.byte01.value_u8bit == 2) {
		info->scrub_id = SPONGE_EVENT_TYPE_FOD_RELEASE;

		info->scrub_x = ((touch_info.byte02.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0xF0) >> 4);
		info->scrub_y = ((touch_info.byte03.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0x0F));

		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
		input_sync(info->input_dev);
		input_info(true, &info->client->dev, "%s: FOD RELEASE: %d, %d, %d\n",
				__func__, info->scrub_id, info->scrub_x, info->scrub_y);
	} else if (touch_info.byte01.value_u8bit == 3) {
		info->scrub_id = SPONGE_EVENT_TYPE_FOD_OUT;

		info->scrub_x = ((touch_info.byte02.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0xF0) >> 4);
		info->scrub_y = ((touch_info.byte03.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0x0F));

		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
		input_sync(info->input_dev);
		input_info(true, &info->client->dev, "%s: FOD OUT: %d, %d, %d\n",
				__func__, info->scrub_id, info->scrub_x, info->scrub_y);
	}
}

static bool ts_read_coord(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;
	u16 status_data;
	u16 pocket_data;

	/* for  Debugging Tool */

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (ts_get_raw_data(info) == false)
			return false;

		if (info->touch_mode == TOUCH_SENTIVITY_MEASUREMENT_MODE) {
			for (i = 0; i < TOUCH_SENTIVITY_MEASUREMENT_COUNT; i++) {
				info->sensitivity_data[i] = info->cur_data[i];
			}
		}

		goto out;
	}

	if (info->pocket_enable) {
		if (read_data(info->client, ZT_STATUS_REG, (u8 *)&status_data, 2) < 0) {
			input_err(true, &client->dev, "%s: fail to read status reg\n", __func__);
		}

		if (zinitix_bit_test(status_data, BIT_POCKET_MODE)) {
			if (read_data(info->client, ZT_POCKET_DETECT, (u8 *)&pocket_data, 2) < 0) {
				input_err(true, &client->dev, "%s: fail to read pocket reg\n", __func__);
			} else if (info->input_dev_proximity) {
				input_info(true, &client->dev, "Pocket %s \n", pocket_data == 11 ? "IN" : "OUT");
				input_report_abs(info->input_dev_proximity, ABS_MT_CUSTOM, pocket_data);
				input_sync(info->input_dev_proximity);
			} else {
				input_err(true, &client->dev, "%s: no dev for proximity\n", __func__);
			}
		}
	}

	memset(info->touch_info, 0x0, sizeof(struct point_info) * MAX_SUPPORTED_FINGER_NUM);

	if (read_data(info->client, ZT_POINT_STATUS_REG,
				(u8 *)(&info->touch_info[0]), sizeof(struct point_info)) < 0) {
		input_err(true, &client->dev, "Failed to read point info\n");
		return false;
	}

	if (info->fod_enable && info->fod_with_finger_packet) {
		memset(&info->touch_fod_info, 0x0, sizeof(struct point_info));

		if (read_data(info->client, ZT_FOD_STATUS_REG,
			(u8 *)(&info->touch_fod_info), sizeof(struct point_info)) < 0) {
			input_err(true, &client->dev, "Failed to read Touch FOD info\n");
			return false;
		}

		if (info->touch_fod_info.byte00.value.eid == GESTURE_EVENT
				&& info->touch_fod_info.byte00.value.tid == FINGERPRINT)
			zt_ts_fod_event_report(info, info->touch_fod_info);
	}

	if (info->touch_info[0].byte00.value.eid == COORDINATE_EVENT) {
		info->touched_finger_num = info->touch_info[0].byte07.value.left_event;

		#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
		if(info->touched_finger_num > (info->cap_info.multi_fingers-1)){
			input_err(true, &client->dev, "Invalid touched_finger_num(%d)\n", info->touched_finger_num);
			info->touched_finger_num = 0;
		}
		#endif

		if (info->touched_finger_num > 0) {
			if (read_data(info->client, ZT_POINT_STATUS_REG1, (u8 *)(&info->touch_info[1]),
						(info->touched_finger_num)*sizeof(struct point_info)) < 0) {
				input_err(true, &client->dev, "Failed to read touched point info\n");
				return false;
			}
		}
	} else if (info->touch_info[0].byte00.value.eid == GESTURE_EVENT) {
		if (info->touch_info[0].byte00.value.tid == SWIPE_UP) {
			input_info(true, &client->dev, "%s: Spay Gesture\n", __func__);

			info->scrub_id = SPONGE_EVENT_TYPE_SPAY;
			info->scrub_x = 0;
			info->scrub_y = 0;

			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
			input_sync(info->input_dev);
		} else if (info->touch_info[0].byte00.value.tid == FINGERPRINT) {
				zt_ts_fod_event_report(info, info->touch_info[0]);
		} else if (info->touch_info[0].byte00.value.tid == SINGLE_TAP) {
			if (info->singletap_enable) {
				info->scrub_id = SPONGE_EVENT_TYPE_SINGLE_TAP;

				info->scrub_x = ((info->touch_info[0].byte02.value_u8bit << 4) & 0xFF0)
					| ((info->touch_info[0].byte04.value_u8bit & 0xF0) >> 4);
				info->scrub_y = ((info->touch_info[0].byte03.value_u8bit << 4) & 0xFF0)
					| ((info->touch_info[0].byte04.value_u8bit & 0x0F));

				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
				input_sync(info->input_dev);
				input_info(true, &client->dev, "%s: SINGLE TAP: %d, %d, %d\n",
						__func__, info->scrub_id, info->scrub_x, info->scrub_y);
			}
		} else if (info->touch_info[0].byte00.value.tid == DOUBLE_TAP) {
			if (info->aot_enable && (info->touch_info[0].byte01.value_u8bit == 1)) {
				input_report_key(info->input_dev, KEY_WAKEUP, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_WAKEUP, 0);
				input_sync(info->input_dev);

				/* request from sensor team */
				if (info->input_dev_proximity) {
					input_report_abs(info->input_dev_proximity, ABS_MT_CUSTOM2, 1);
					input_sync(info->input_dev_proximity);
					input_report_abs(info->input_dev_proximity, ABS_MT_CUSTOM2, 0);
					input_sync(info->input_dev_proximity);
				}

				input_info(true, &client->dev, "%s: AOT Doubletab\n", __func__);
			} else if (info->aod_enable && (info->touch_info[0].byte01.value_u8bit == 0)) {
				info->scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;

				info->scrub_x = ((info->touch_info[0].byte02.value_u8bit << 4) & 0xFF0)
					| ((info->touch_info[0].byte04.value_u8bit & 0xF0) >> 4);
				info->scrub_y = ((info->touch_info[0].byte03.value_u8bit << 4) & 0xFF0)
					| ((info->touch_info[0].byte04.value_u8bit & 0x0F));

				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
				input_sync(info->input_dev);

				input_info(true, &client->dev, "%s: AOD Doubletab: %d, %d, %d\n",
						__func__, info->scrub_id, info->scrub_x, info->scrub_y);
			}
		}
	}
out:
	write_cmd(info->client, ZT_CLEAR_INT_STATUS_CMD);
	return true;
}

#if ESD_TIMER_INTERVAL
static void esd_timeout_handler(struct timer_list *t)
{
	struct zt_ts_info *info = from_timer(info, t, esd_timeout_tmr);

	info->p_esd_timeout_tmr = NULL;
	queue_work(esd_tmr_workqueue, &info->tmr_work);
}

static void esd_timer_start(u16 sec, struct zt_ts_info *info)
{
	if (info->sleep_mode) {
		input_info(true, &info->client->dev, "%s skip (sleep_mode)!\n", __func__);
		return;
	}

	mutex_lock(&info->lock);
	if (info->p_esd_timeout_tmr != NULL)
#ifdef CONFIG_SMP
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
		del_timer(info->p_esd_timeout_tmr);
#endif
	info->p_esd_timeout_tmr = NULL;

	timer_setup(&info->esd_timeout_tmr, esd_timeout_handler, 0);
	mod_timer(&info->esd_timeout_tmr, jiffies + (HZ * sec));
	info->p_esd_timeout_tmr = &info->esd_timeout_tmr;
	mutex_unlock(&info->lock);
}

static void esd_timer_stop(struct zt_ts_info *info)
{
	mutex_lock(&info->lock);
	if (info->p_esd_timeout_tmr)
#ifdef CONFIG_SMP
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
		del_timer(info->p_esd_timeout_tmr);
#endif

	info->p_esd_timeout_tmr = NULL;
	mutex_unlock(&info->lock);
}

static void esd_timer_init(struct zt_ts_info *info)
{
	mutex_lock(&info->lock);
	timer_setup(&info->esd_timeout_tmr, esd_timeout_handler, 0);
	info->p_esd_timeout_tmr = NULL;
	mutex_unlock(&info->lock);
}

static void ts_tmr_work(struct work_struct *work)
{
	struct zt_ts_info *info =
		container_of(work, struct zt_ts_info, tmr_work);
	struct i2c_client *client = info->client;

	input_info(true, &client->dev, "%s++\n", __func__);

	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "%s: Other process occupied (%d)\n",
				__func__, info->work_state);

		return;
	}

	info->work_state = ESD_TIMER;

	disable_irq_nosync(info->irq);
	zt_power_control(info, POWER_OFF);
	zt_power_control(info, POWER_ON_SEQUENCE);

	clear_report_data(info);
	if (mini_init_touch(info) == false)
		goto fail_time_out_init;

	info->work_state = NOTHING;
	enable_irq(info->irq);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s--\n", __func__);
#endif

	return;
fail_time_out_init:
	input_err(true, &client->dev, "%s: Failed to restart\n", __func__);
	esd_timer_start(CHECK_ESD_TIMER, info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	return;
}
#endif

static bool zt_power_sequence(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	u16 chip_code;
	u16 checksum;

	if (read_data(client, ZT_CHECKSUM_RESULT, (u8 *)&checksum, 2) < 0) {
		input_err(true, &client->dev, "%s: Failed to read checksum\n", __func__);
		goto fail_power_sequence;
	}

	if (checksum == CORRECT_CHECK_SUM)
		return true;

	if (write_reg(client, VCMD_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: Failed to send power sequence(vendor cmd enable)\n", __func__);
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	if (read_data(client, VCMD_ID, (u8 *)&chip_code, 2) < 0) {
		input_err(true, &client->dev, "%s: Failed to read chip code\n", __func__);
		goto fail_power_sequence;
	}

	input_info(true, &client->dev, "%s: chip code = 0x%x\n", __func__, chip_code);
	usleep_range(10, 10);

	if (write_cmd(client, VCMD_INTN_CLR) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: Failed to send power sequence(intn clear)\n", __func__);
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	if (write_reg(client, VCMD_NVM_INIT, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: Failed to send power sequence(nvm init)\n", __func__);
		goto fail_power_sequence;
	}
	zt_delay(2);

	if (write_reg(client, VCMD_NVM_PROG_START, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: Failed to send power sequence(program start)\n", __func__);
		goto fail_power_sequence;
	}

	zt_delay(FIRMWARE_ON_DELAY);	/* wait for checksum cal */

	if (read_data(client, ZT_CHECKSUM_RESULT, (u8 *)&checksum, 2) < 0)
		input_err(true, &client->dev, "%s: Failed to read checksum (retry)\n", __func__);

	if (checksum == CORRECT_CHECK_SUM)
		return true;
	else
		input_err(true, &client->dev, "%s: Failed to read checksum 0x%x\n", __func__, checksum);

fail_power_sequence:
	return false;
}

static bool zt_power_control(struct zt_ts_info *info, u8 ctl)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	input_info(true, &client->dev, "[TSP] %s, %d\n", __func__, ctl);

	mutex_lock(&info->power_init);
	if (ctl == POWER_OFF)
		info->tsp_pwr_enabled = ctl;

	ret = info->pdata->tsp_power(info, ctl);
	if (ret) {
		mutex_unlock(&info->power_init);
		return false;
	}

	zt_pinctrl_configure(info, ctl);

	if (ctl == POWER_ON_SEQUENCE) {
		zt_delay(CHIP_ON_DELAY);
		info->tsp_pwr_enabled = ctl;
		input_info(true, &client->dev, "[TSP] %s, info->tsp_pwr_enabled %d\n", __func__, info->tsp_pwr_enabled);
		ret =  zt_power_sequence(info);
		mutex_unlock(&info->power_init);
		return ret;
	} else if (ctl == POWER_OFF) {
		zt_delay(CHIP_OFF_DELAY);
	} else if (ctl == POWER_ON) {
		zt_delay(CHIP_ON_DELAY);
		info->tsp_pwr_enabled = ctl;
	}

	mutex_unlock(&info->power_init);

	input_info(true, &client->dev, "[TSP] %s, info->tsp_pwr_enabled %d\n", __func__, info->tsp_pwr_enabled);

	return true;
}

static void zt_charger_status_cb(struct tsp_callbacks *cb, bool ta_status)
{
	struct zt_ts_info *info =
		container_of(cb, struct zt_ts_info, callbacks);
	if (!ta_status)
		g_ta_connected = false;
	else
		g_ta_connected = true;

	zt_set_optional_mode(info, DEF_OPTIONAL_MODE_USB_DETECT_BIT, g_ta_connected);
	input_info(true, &info->client->dev, "TA %s\n", ta_status ? "connected" : "disconnected");
}

static bool crc_check(struct zt_ts_info *info)
{
	u16 chip_check_sum = 0;

	if (read_data(info->client, ZT_CHECKSUM_RESULT,
				(u8 *)&chip_check_sum, 2) < 0) {
		input_err(true, &info->client->dev, "%s: read crc fail", __func__);
	}

	input_info(true, &info->client->dev, "%s: 0x%04X\n", __func__, chip_check_sum);

	if (chip_check_sum == CORRECT_CHECK_SUM)
		return true;
	else
		return false;
}

static bool ts_check_need_upgrade(struct zt_ts_info *info,
		u16 cur_version, u16 cur_minor_version, u16 cur_reg_version, u16 cur_hw_id)
{
	u16	new_version;
	u16	new_minor_version;
	u16	new_reg_version;
#if CHECK_HWID
	u16	new_hw_id;
#endif

	new_version = (u16) (info->fw_data[52] | (info->fw_data[53]<<8));
	new_minor_version = (u16) (info->fw_data[56] | (info->fw_data[57]<<8));
	new_reg_version = (u16) (info->fw_data[60] | (info->fw_data[61]<<8));

#if CHECK_HWID
	new_hw_id = (u16) (fw_data[0x7528] | (fw_data[0x7529]<<8));
	input_info(true, &info->client->dev, "cur HW_ID = 0x%x, new HW_ID = 0x%x\n",
			cur_hw_id, new_hw_id);
	if (cur_hw_id != new_hw_id)
		return false;
#endif

	input_info(true, &info->client->dev, "cur version = 0x%x, new version = 0x%x\n",
			cur_version, new_version);
	input_info(true, &info->client->dev, "cur minor version = 0x%x, new minor version = 0x%x\n",
			cur_minor_version, new_minor_version);
	input_info(true, &info->client->dev, "cur reg data version = 0x%x, new reg data version = 0x%x\n",
			cur_reg_version, new_reg_version);

	if (info->pdata->bringup == 3) {
		input_info(true, &info->client->dev, "%s: bringup 3, update when version is different\n", __func__);
		if (cur_version == new_version
				&& cur_minor_version == new_minor_version
				&& cur_reg_version == new_reg_version)
			return false;
		else
			return true;
	} else if (info->pdata->bringup == 2) {
		input_info(true, &info->client->dev, "%s: bringup 2, skip update\n", __func__);
		return false;
	}

	if (cur_version > 0xFF)
		return true;
	if (cur_version < new_version)
		return true;
	else if (cur_version > new_version)
		return false;
	if (cur_minor_version < new_minor_version)
		return true;
	else if (cur_minor_version > new_minor_version)
		return false;
	if (cur_reg_version < new_reg_version)
		return true;

	return false;
}

#define TC_SECTOR_SZ		8
#define TC_NVM_SECTOR_SZ	64
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
#define TSP_PAGE_SIZE	1024
#define FUZING_UDELAY 28000
#endif

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
#define TSP_PAGE_SIZE	128
#define FUZING_UDELAY 15000
#endif

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7554)
#define TSP_PAGE_SIZE		128
#define FUZING_UDELAY	8000
#endif

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7548) || defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7538)
#define TSP_PAGE_SIZE		64
#define FUZING_UDELAY	8000
#endif

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7532)
#define TSP_PAGE_SIZE		64
#define FUZING_UDELAY	30000
#endif

#define USB_POR_OFF_DELAY	1500
#define USB_POR_ON_DELAY	1500

#define ZT76XX_UPGRADE_INFO_SIZE 1024

u32 ic_info_size, ic_info_checksum;
u32 ic_core_size, ic_core_checksum;
u32 ic_cust_size, ic_cust_checksum;
u32 ic_regi_size, ic_regi_checksum;
u32 fw_info_size, fw_info_checksum;
u32 fw_core_size, fw_core_checksum;
u32 fw_cust_size, fw_cust_checksum;
u32 fw_regi_size, fw_regi_checksum;
u8 kind_of_download_method;

static bool check_upgrade_method(struct zt_ts_info *info, const u8 *firmware_data, u16 chip_code)
{
	char bindata[0x80];
	u16 buff16[64];
	u16 flash_addr;
	int nsectorsize = 8;
	u32 chk_info_checksum;

	nvm_binary_info fw_info;
	nvm_binary_info ic_info;
	int i;

	struct i2c_client *client = info->client;

	if (write_reg(client, VCMD_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: check upgrade error (vcmd)\n", __func__);
		return false;
	}

	zt_delay(1);

	if (write_cmd(client, VCMD_INTN_CLR) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: check upgrade error (intn clear)\n", __func__);
		return false;
	}

	if (write_reg(client, VCMD_NVM_INIT, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: check upgrade error (nvm init)\n", __func__);
		return false;
	}

	zt_delay(5);
	memset(bindata, 0xff, sizeof(bindata));

	if (write_reg(client, VCMD_UPGRADE_INIT_FLASH, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: check upgrade error (init flash)\n", __func__);
		return false;
	}

	for (flash_addr = 0; flash_addr < 0x80; flash_addr += nsectorsize) {
		if (read_firmware_data(client, VCMD_UPGRADE_READ_FLASH,
					(u8*)&bindata[flash_addr], TC_SECTOR_SZ) < 0) {
			input_err(true, &client->dev, "%s: failed to read firmware\n", __func__);
			return false;
		}
	}

	memcpy(ic_info.buff32, bindata, 0x80);	//copy data
	memcpy(buff16, bindata, 0x80);			//copy data

	//BIG ENDIAN ==> LITTLE ENDIAN
	ic_info_size = (((u32)ic_info.val.info_size[1] << 16 & 0x00FF0000) | ((u32)ic_info.val.info_size[2] << 8 & 0x0000FF00) | ((u32)ic_info.val.info_size[3] & 0x000000FF));
	ic_core_size = (((u32)ic_info.val.core_size[1] << 16 & 0x00FF0000) | ((u32)ic_info.val.core_size[2] << 8 & 0x0000FF00) | ((u32)ic_info.val.core_size[3] & 0x000000FF));
	ic_cust_size = (((u32)ic_info.val.custom_size[1] << 16 & 0x00FF0000) | ((u32)ic_info.val.custom_size[2] << 8 & 0x0000FF00) | ((u32)ic_info.val.custom_size[3] & 0x000000FF));
	ic_regi_size = (((u32)ic_info.val.register_size[1] << 16 & 0x00FF0000) | ((u32)ic_info.val.register_size[2] << 8 & 0x0000FF00) | ((u32)ic_info.val.register_size[3] & 0x000000FF));

	memcpy(fw_info.buff32, firmware_data, 0x80);

	fw_info_size = (((u32)fw_info.val.info_size[1] << 16 & 0x00FF0000) | ((u32)fw_info.val.info_size[2] << 8 & 0x0000FF00) | ((u32)fw_info.val.info_size[3] & 0x000000FF));
	fw_core_size = (((u32)fw_info.val.core_size[1] << 16 & 0x00FF0000) | ((u32)fw_info.val.core_size[2] << 8 & 0x0000FF00) | ((u32)fw_info.val.core_size[3] & 0x000000FF));
	fw_cust_size = (((u32)fw_info.val.custom_size[1] << 16 & 0x00FF0000) | ((u32)fw_info.val.custom_size[2] << 8 & 0x0000FF00) | ((u32)fw_info.val.custom_size[3] & 0x000000FF));
	fw_regi_size = (((u32)fw_info.val.register_size[1] << 16 & 0x00FF0000) | ((u32)fw_info.val.register_size[2] << 8 & 0x0000FF00) | ((u32)fw_info.val.register_size[3] & 0x000000FF));


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//[DESC] Determining the execution conditions of [FULL D/L] or [Partial D/L]
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	kind_of_download_method = NEED_FULL_DL;

	if ((ic_core_size == fw_core_size)
			&& (ic_cust_size == fw_cust_size)
			&& (ic_info.val.core_checksum == fw_info.val.core_checksum)
			&& (ic_info.val.custom_checksum == fw_info.val.custom_checksum)
			&& ((ic_regi_size != fw_regi_size)
				|| (ic_info.val.register_checksum != fw_info.val.register_checksum)))
		kind_of_download_method = NEED_PARTIAL_DL_REG;

	if ((ic_core_size == fw_core_size)
			&& (ic_info.val.core_checksum == fw_info.val.core_checksum)
			&& ((ic_cust_size != fw_cust_size)
				|| (ic_info.val.custom_checksum != fw_info.val.custom_checksum)))
		kind_of_download_method = NEED_PARTIAL_DL_CUSTOM;


	if (ic_info_size == 0 || ic_core_size == 0 ||
		ic_cust_size == 0 || ic_regi_size == 0 ||
		fw_info_size == 0 || fw_core_size == 0 ||
		fw_cust_size == 0 || fw_regi_size == 0 ||
		ic_info_size == 0xFFFFFFFF || ic_core_size == 0xFFFFFFFF ||
		ic_cust_size == 0xFFFFFFFF || ic_regi_size == 0xFFFFFFFF)
		kind_of_download_method = NEED_FULL_DL;

	if (kind_of_download_method != NEED_FULL_DL) {
		chk_info_checksum = 0;

		//info checksum.
		buff16[0x20 / 2] = 0;
		buff16[0x22 / 2] = 0;

		//Info checksum
		for (i = 0; i < 0x80 / 2; i++) {
			chk_info_checksum += buff16[i];
		}

		if (chk_info_checksum != ic_info.val.info_checksum) {
			kind_of_download_method = NEED_FULL_DL;
		}
	}
	//////////////////////////////////////////////////////////////////////
	return true;
}

static bool upgrade_fw_full_download(struct zt_ts_info *info, const u8 *firmware_data, u16 chip_code, u32 total_size)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	int i;
	int nmemsz;
	int nrdsectorsize = 8;
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
	unsigned short int erase_info[2];
#endif
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
	// change erase/program time
	u32	icNvmDelayRegister = 0x001E002C;
	u32	icNvmDelayTime = 0x00004FC2;
	u8 cData[8];
#endif

	// calculate FW size
	nmemsz = fw_info_size + fw_core_size + fw_cust_size + fw_regi_size;
	if (nmemsz % nrdsectorsize > 0)
		nmemsz = (nmemsz / nrdsectorsize) * nrdsectorsize + nrdsectorsize;


	//[DEBUG]
	input_info(true, &client->dev, "%s: [UPGRADE] ENTER Firmware Upgrade(FULL)\n", __func__);

	if (write_reg(client, VCMD_NVM_INIT, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (nvm init)\n", __func__);
		return false;
	}
	zt_delay(5);


	#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
	//====================================================
	// change erase/program time

	// set default OSC Freq - 44Mhz
	if (write_reg(client, VCMD_OSC_FREQ_SEL , 128) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: fail to write VCMD_OSC_FREQ_SEL\n", __func__);
		return false;
	}
	zt_delay(5);

	// address : Talpgm - 4.5msec
	cData[0] = (icNvmDelayRegister) & 0xFF;
	cData[1] = (icNvmDelayRegister >> 8) & 0xFF;
	cData[2] = (icNvmDelayRegister >> 16) & 0xFF;
	cData[3] = (icNvmDelayRegister >> 24) & 0xFF;
	// data
	cData[4] = (icNvmDelayTime) & 0xFF;
	cData[5] = (icNvmDelayTime >> 8) & 0xFF;
	cData[6] = (icNvmDelayTime >> 16) & 0xFF;
	cData[7] = (icNvmDelayTime >> 24) & 0xFF;

	if (write_data(client, VCMD_REG_WRITE, (u8 *)&cData[0], 8) < 0) {
		input_err(true, &client->dev, "%s: fail to change erase/program time\n", __func__);
		return false;
	}
	zt_delay(5);
	#endif

	input_info(true, &client->dev, "%s: init flash\n", __func__);

	if (write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm vpp on\n", __func__);
		return false;
	}
	zt_delay(10);

	if (write_cmd(client, VCMD_UPGRADE_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		return false;
	}

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
	input_info(true, &client->dev, "%s: [UPGRADE] Erase start\n", __func__);	//[DEBUG]
	// Mass Erase
	//====================================================

	// only program mode
	if (write_reg(client, VCMD_UPGRADE_WRITE_MODE , 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter write mode\n", __func__);
		return false;
	}
	zt_delay(1);

	for (i = 0; i < 3; i++) {
		erase_info[0] = 0x0001;
		erase_info[1] = i; //Section num.
		write_data(client, VCMD_UPGRADE_BLOCK_ERASE , (u8 *)&erase_info[0] , 4);
		zt_delay(50);
	}

	for (i = 95; i < 126; i++) {
		erase_info[0] = 0x0000;
		erase_info[1] = i; //Page num.
		write_data(client, VCMD_UPGRADE_BLOCK_ERASE , (u8 *)&erase_info[0] , 4);
		zt_delay(50);
	}

	zt_delay(100);
	if (write_cmd(client, VCMD_UPGRADE_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		return false;
	}

	input_info(true, &client->dev, "%s: [UPGRADE] Erase End.\n", __func__);	//[DEBUG]
	// Mass Erase End
	//====================================================
#else
	// Erase + Program mode
	if (write_reg(client, VCMD_UPGRADE_WRITE_MODE , 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter write mode\n", __func__);
		return false;
	}
#endif

	//====================================================
	// Upgrade Start

	input_info(true, &client->dev, "%s: [UPGRADE] UPGRADE START.\n", __func__);	//[DEBUG]
	zt_delay(1);

	for (flash_addr = 0; flash_addr < nmemsz; ) {
		for (i = 0; i < TSP_PAGE_SIZE/TC_SECTOR_SZ; i++) {
			if (write_data(client,
						VCMD_UPGRADE_WRITE_FLASH,
						(u8 *)&firmware_data[flash_addr],TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "%s: error: write zinitix tc firmware\n", __func__);
				return false;
			}
			flash_addr += TC_SECTOR_SZ;
			usleep_range(100, 100);
		}

		usleep_range(FUZING_UDELAY, FUZING_UDELAY); /*for fuzing delay*/
	}

	input_err(true, &client->dev, "%s: [UPGRADE] UPGRADE END. VERIFY START.\n", __func__);	//[DEBUG]

	//====================================================
	// Verify Skip...


	//====================================================
	// Power Reset
	zt_power_control(info, POWER_OFF);

	if (zt_power_control(info, POWER_ON_SEQUENCE) == true) {
		zt_delay(10);
		input_info(true, &client->dev, "%s: upgrade finished\n", __func__);
		return true;
	}

	input_info(true, &client->dev, "%s: [UPGRADE] VERIFY FAIL.\n", __func__);	//[DEBUG]

	return false;

}

static bool upgrade_fw_partial_download(struct zt_ts_info *info, const u8 *firmware_data, u16 chip_code, u32 total_size)
{
	struct i2c_client *client = info->client;

	int i;
	int nrdsectorsize;
	int por_off_delay, por_on_delay;
	int idx, nmemsz;
	u32 erase_start_page_num;
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
	unsigned short int erase_info[2];
#endif
	int nsectorsize = 8;

	nrdsectorsize = 8;
	por_off_delay = USB_POR_OFF_DELAY;
	por_on_delay = USB_POR_ON_DELAY;

	nmemsz = fw_info_size + fw_core_size + fw_cust_size + fw_regi_size;
	if (nmemsz % nrdsectorsize > 0)
		nmemsz = (nmemsz / nrdsectorsize) * nrdsectorsize + nrdsectorsize;

	erase_start_page_num = (fw_info_size + fw_core_size) / TSP_PAGE_SIZE;

	if (write_reg(client, VCMD_NVM_INIT, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (nvm init)\n", __func__);
		return false;
	}

	zt_delay(5);
	input_err(true, &client->dev, "%s: init flash\n", __func__);

	if (write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm vpp on\n", __func__);
		return false;
	}
	zt_delay(10);

	if (write_cmd(client, VCMD_UPGRADE_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		return false;
	}

	/* Erase Area */
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
	// only program mode
	if (write_reg(client, VCMD_UPGRADE_WRITE_MODE , 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter write mode\n", __func__);
		return false;

	}
	zt_delay(1);
	erase_info[0] = 0x0000;
	erase_info[1] = 0x0000;	/* Page num. */
	write_data(client, VCMD_UPGRADE_BLOCK_ERASE , (u8 *)&erase_info[0] , 4);

	for (i = erase_start_page_num; i < nmemsz/TSP_PAGE_SIZE; i++) {
		zt_delay(50);
		erase_info[0] = 0x0000;
		erase_info[1] = i;	/* Page num.*/
		write_data(client, VCMD_UPGRADE_BLOCK_ERASE , (u8 *)&erase_info[0] , 4);
	}
	zt_delay(50);

	if (write_cmd(client, VCMD_UPGRADE_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		return false;
	}
	zt_delay(1);

	if (write_reg(client, VCMD_UPGRADE_START_PAGE , 0x00) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to start addr. (erase end)\n", __func__);
		return false;
	}
	zt_delay(5);
#else
	// Erase + Program mode
	if (write_reg(client, VCMD_UPGRADE_WRITE_MODE , 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter write mode\n", __func__);
		return false;

	}
	zt_delay(1);
#endif

	for (i = 0; i < fw_info_size; ) {
		for (idx = 0; idx < TSP_PAGE_SIZE / nsectorsize; idx++) {
			if (write_data(client, VCMD_UPGRADE_WRITE_FLASH, (char *)&firmware_data[i], nsectorsize) != 0) {
				input_err(true, &client->dev, "%s: failed to write flash\n", __func__);
				return false;
			}
			i += nsectorsize;
			usleep_range(100, 100);
		}

		usleep_range(FUZING_UDELAY, FUZING_UDELAY); /*for fuzing delay*/
	}

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650) || defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
	if (write_reg(client, VCMD_UPGRADE_START_PAGE , erase_start_page_num) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to start addr. (erase end)\n", __func__);
		return false;

	}
	zt_delay(5);
#endif

	for (i = TSP_PAGE_SIZE * erase_start_page_num; i < nmemsz; ) {
		for (idx = 0; idx < TSP_PAGE_SIZE  / nsectorsize; idx++) {	//npagesize = 1024 // nsectorsize : 8
			if (write_data(client, VCMD_UPGRADE_WRITE_FLASH, (char *)&firmware_data[i], nsectorsize) != 0) {
				input_err(true, &client->dev, "%s: failed to write flash\n", __func__);
				return false;
			}
			i += nsectorsize;
			usleep_range(100, 100);
		}

		usleep_range(FUZING_UDELAY, FUZING_UDELAY); /*for fuzing delay*/
	}

	input_info(true, &client->dev, "%s: [UPGRADE] PARTIAL UPGRADE END. VERIFY START.\n", __func__);

	//VERIFY
	zt_power_control(info, POWER_OFF);

	if (zt_power_control(info, POWER_ON_SEQUENCE) == true) {
		zt_delay(10);
		input_info(true, &client->dev, "%s: upgrade finished\n", __func__);
		return true;
	}

	input_info(true, &client->dev, "%s: [UPGRADE] VERIFY FAIL.\n", __func__);

	return false;
}

static u8 ts_upgrade_firmware(struct zt_ts_info *info,
		const u8 *firmware_data)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	int retry_cnt = 0;
	u16 chip_code;
	bool ret = false;
	u32 size = 0;

retry_upgrade:
	zt_power_control(info, POWER_OFF);
	zt_power_control(info, POWER_ON);
	zt_delay(10);

	if (write_reg(client, VCMD_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (vendor cmd enable)\n", __func__);
		goto fail_upgrade;
	}
	zt_delay(10);

	if (write_cmd(client, VCMD_UPGRADE_MODE) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: upgrade mode error\n", __func__);
		goto fail_upgrade;
	}
	zt_delay(5);

	if (write_reg(client, VCMD_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (vendor cmd enable)\n", __func__);
		goto fail_upgrade;
	}
	zt_delay(1);

	if (write_cmd(client, VCMD_INTN_CLR) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: vcmd int clr error\n", __func__);
		goto fail_upgrade;
	}

	if (read_data(client, VCMD_ID, (u8 *)&chip_code, 2) < 0) {
		input_err(true, &client->dev, "%s: failed to read chip code\n", __func__);
		goto fail_upgrade;
	}

	zt_delay(5);

	input_info(true, &client->dev, "chip code = 0x%x\n", chip_code);

	flash_addr = ((firmware_data[0x59]<<16) | (firmware_data[0x5A]<<8) | firmware_data[0x5B]); //Info
	flash_addr += ((firmware_data[0x5D]<<16) | (firmware_data[0x5E]<<8) | firmware_data[0x5F]); //Core
	flash_addr += ((firmware_data[0x61]<<16) | (firmware_data[0x62]<<8) | firmware_data[0x63]); //Custom
	flash_addr += ((firmware_data[0x65]<<16) | (firmware_data[0x66]<<8) | firmware_data[0x67]); //Register

	info->cap_info.ic_fw_size = ((firmware_data[0x69]<<16) | (firmware_data[0x6A]<<8) | firmware_data[0x6B]); //total size

	input_info(true, &client->dev, "f/w ic_fw_size = %d\n", info->cap_info.ic_fw_size);

	if (flash_addr != 0)
		size = flash_addr;
	else
		size = info->cap_info.ic_fw_size;

	input_info(true, &client->dev, "f/w size = 0x%x Page_sz = %d\n", size, TSP_PAGE_SIZE);
	usleep_range(10, 10);

	/////////////////////////////////////////////////////////////////////////////////////////
	ret = check_upgrade_method(info, firmware_data, chip_code);
	if (ret == false) {
		input_err(true, &client->dev, "%s: check upgrade method error\n", __func__);
		goto fail_upgrade;
	}
	/////////////////////////////////////////////////////////////////////////////////////////

	if (kind_of_download_method == NEED_FULL_DL)
		ret = upgrade_fw_full_download(info, firmware_data, chip_code, size);
	else
		ret = upgrade_fw_partial_download(info, firmware_data, chip_code, size);

	if (ret == true)
		return true;

fail_upgrade:
	zt_power_control(info, POWER_OFF);

	if (retry_cnt++ < INIT_RETRY_CNT) {
		input_err(true, &client->dev, "upgrade failed: so retry... (%d)\n", retry_cnt);
		goto retry_upgrade;
	}

	input_info(true, &client->dev, "%s: Failed to upgrade\n", __func__);

	return false;
}

static bool ts_hw_calibration(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	u16	chip_eeprom_info;
	int time_out = 0;

	input_info(true, &client->dev, "%s start\n", __func__);

	if (write_reg(client, ZT_TOUCH_MODE, 0x07) != I2C_SUCCESS)
		return false;

	zt_delay(10);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	zt_delay(10);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	zt_delay(50);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	zt_delay(10);

	if (write_cmd(client, ZT_CALIBRATE_CMD) != I2C_SUCCESS)
		return false;

	if (write_cmd(client, ZT_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
		return false;

	zt_delay(10);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);

	/* wait for h/w calibration*/
	do {
		zt_delay(200);
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);

		if (read_data(client, ZT_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0)
			return false;

		input_dbg(true, &client->dev, "touch eeprom info = 0x%04X\r\n",
				chip_eeprom_info);

		if (!zinitix_bit_test(chip_eeprom_info, 0))
			break;

		if (time_out == 4) {
			write_cmd(client, ZT_CALIBRATE_CMD);
			zt_delay(10);
			write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
			input_err(true, &client->dev, "%s: h/w calibration retry timeout.\n", __func__);
		}

		if (time_out++ > 10) {
			input_err(true, &client->dev, "%s: h/w calibration timeout.\n", __func__);
			write_reg(client, ZT_TOUCH_MODE, TOUCH_POINT_MODE);
			zt_delay(50);
			write_cmd(client, ZT_SWRESET_CMD);
			return false;
		}
	} while (1);

	write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001);
	usleep_range(100, 100);
	if (write_cmd(client, ZT_SAVE_CALIBRATION_CMD) != I2C_SUCCESS)
		return false;

	zt_delay(1100);
	write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0000);

	if (write_reg(client, ZT_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS)
		return false;

	zt_delay(50);
	if (write_cmd(client, ZT_SWRESET_CMD) != I2C_SUCCESS)
		return false;

	return true;
}

static int ic_version_check(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	int ret;
	u8 data[8] = {0};

	/* get chip information */
	ret = read_data(client, ZT_VENDOR_ID, (u8 *)&cap->vendor_id, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail vendor id\n", __func__);
		goto error;
	}

	ret = read_data(client, ZT_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail fw_minor_version\n", __func__);
		goto error;
	}

	ret = read_data(client, ZT_CHIP_REVISION, data, 8);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail chip_revision\n", __func__);
		goto error;
	}

	cap->ic_revision = data[0] | (data[1] << 8);
	cap->fw_version = data[2] | (data[3] << 8);
	cap->reg_data_version = data[4] | (data[5] << 8);
	cap->hw_id = data[6] | (data[7] << 8);

error:
	return ret;
}

static int fw_update_work(struct zt_ts_info *info, bool force_update)
{
	struct zt_ts_platform_data *pdata = info->pdata;
	struct capa_info *cap = &(info->cap_info);
	int ret;
	bool need_update = false;
	const struct firmware *tsp_fw = NULL;
	char fw_path[MAX_FW_PATH];
	u16 chip_eeprom_info;

	if (pdata->bringup == 1) {
		input_info(true, &info->client->dev, "%s: bringup 1 skip update\n", __func__);
		return 0;
	}

	snprintf(fw_path, MAX_FW_PATH, "%s", pdata->firmware_name);
	input_info(true, &info->client->dev,
			"%s: start\n", __func__);

	ret = request_firmware(&tsp_fw, fw_path, &(info->client->dev));
	if (ret < 0) {
		input_info(true, &info->client->dev,
				"%s: Firmware image %s not available\n", __func__, fw_path);
		goto fw_request_fail;
	}
	else
		info->fw_data = (unsigned char *)tsp_fw->data;

	need_update = ts_check_need_upgrade(info, cap->fw_version,
			cap->fw_minor_version, cap->reg_data_version, cap->hw_id);
	if (!need_update) {
		if (!crc_check(info))
			need_update = true;
	}

	if (need_update == true || force_update == true) {
		ret = ts_upgrade_firmware(info, info->fw_data);
		if (!ret)
			input_err(true, &info->client->dev, "%s: failed fw update\n", __func__);

		ret = ic_version_check(info);
		if (ret < 0)
			input_err(true, &info->client->dev, "%s: failed ic version check\n", __func__);
	}

	if (read_data(info->client, ZT_EEPROM_INFO,
				(u8 *)&chip_eeprom_info, 2) < 0) {
		ret = -1;
		goto fw_request_fail;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) { /* hw calibration bit*/
		if (ts_hw_calibration(info) == false) {
			ret = -1;
			goto fw_request_fail;
		}
	}

fw_request_fail:
	if (tsp_fw)
		release_firmware(tsp_fw);
	return ret;
}

static bool init_touch(struct zt_ts_info *info)
{
	struct zt_ts_platform_data *pdata = info->pdata;
	#if ESD_TIMER_INTERVAL
	u16 reg_val = 0;
	#endif
	u8 data[6] = {0};

	/* get x,y data */
	read_data(info->client, ZT_TOTAL_NUMBER_OF_Y, data, 4);
	info->cap_info.x_node_num = data[2] | (data[3] << 8);
	info->cap_info.y_node_num = data[0] | (data[1] << 8);

	info->cap_info.MaxX= pdata->x_resolution;
	info->cap_info.MaxY = pdata->y_resolution;

	info->cap_info.total_node_num = info->cap_info.x_node_num * info->cap_info.y_node_num;
	info->cap_info.multi_fingers = MAX_SUPPORTED_FINGER_NUM;

	input_info(true, &info->client->dev, "node x %d, y %d  resolution x %d, y %d\n",
			info->cap_info.x_node_num, info->cap_info.y_node_num, info->cap_info.MaxX, info->cap_info.MaxY	);

#if ESD_TIMER_INTERVAL
	if (write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
				SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_init;

	read_data(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, (u8 *)&reg_val, 2);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &info->client->dev, "Esd timer register = %d\n", reg_val);
#endif
#endif
	if (!mini_init_touch(info))
		goto fail_init;

	return true;
fail_init:
	return false;
}

static int zt_set_fod_rect(struct zt_ts_info *info)
{
	int i, ret;
	u8 data[8];
	u32 sum = 0;

	for (i = 0; i < 4; i++) {
		data[i * 2] = info->fod_rect[i] & 0xFF;
		data[i * 2 + 1] = (info->fod_rect[i] >> 8) & 0xFF;
		sum += info->fod_rect[i];
	}

	if (!sum) /* no data */
		return 0;

	input_info(true, &info->client->dev, "%s: %u,%u,%u,%u\n",
			__func__, info->fod_rect[0], info->fod_rect[1],
			info->fod_rect[2], info->fod_rect[3]);

	ret = ts_write_to_sponge(info, ZT_SPONGE_FOD_RECT, data, sizeof(data));
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);

	return ret;
}

static int zt_set_aod_rect(struct zt_ts_info *info)
{
	u8 data[8] = {0};
	int i;
	int ret;

	for (i = 0; i < 4; i++) {
		data[i * 2] = info->aod_rect[i] & 0xFF;
		data[i * 2 + 1] = (info->aod_rect[i] >> 8) & 0xFF;
	}

	ret = ts_write_to_sponge(info, ZT_SPONGE_TOUCHBOX_W_OFFSET, data, sizeof(data));
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: fail set custom lib \n", __func__);

	return ret;
}

static bool mini_init_touch(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;

	if (write_cmd(client, ZT_SWRESET_CMD) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Failed to write reset command\n", __func__);
		goto fail_mini_init;
	}

	if (write_reg(client, ZT_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		goto fail_mini_init;

	/* cover_set */
	if (write_reg(client, ZT_COVER_CONTROL_REG, COVER_OPEN) != I2C_SUCCESS)
		goto fail_mini_init;

	if (info->flip_enable)
		set_cover_type(info, info->flip_enable);

	if (write_reg(client, ZT_OPTIONAL_SETTING, info->m_optional_mode.optional_mode) != I2C_SUCCESS)
		goto fail_mini_init;

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);
	}

#if ESD_TIMER_INTERVAL
	if (write_reg(client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
				SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_mini_init;

	esd_timer_start(CHECK_ESD_TIMER, info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s: Started esd timer\n", __func__);
#endif
#endif

	ts_write_to_sponge(info, ZT_SPONGE_LP_FEATURE, &info->lpm_mode, 2);
	zt_set_fod_rect(info);

	if (info->sleep_mode) {
#if ESD_TIMER_INTERVAL
		esd_timer_stop(info);
#endif
		write_cmd(info->client, ZT_SLEEP_CMD);
		input_info(true, &info->client->dev, "%s, sleep mode\n", __func__);

		zt_set_aod_rect(info);
	} else {
		zt_set_grip_type(info, ONLY_EDGE_HANDLER);
	}

	input_info(true, &client->dev, "%s: Successfully mini initialized\r\n", __func__);
	return true;

fail_mini_init:
	input_err(true, &client->dev, "%s: Failed to initialize mini init\n", __func__);
	return false;
}

static void clear_report_data(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;
	u8 reported = 0;
	char location[7] = "";

	if (info->prox_power_off) {
		input_report_key(info->input_dev, KEY_INT_CANCEL, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_INT_CANCEL, 0);
		input_sync(info->input_dev);
	}

	info->prox_power_off = 0;

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if (info->cur_coord[i].touch_status > FINGER_NONE) {
			input_mt_slot(info->input_dev, i);
			input_report_abs(info->input_dev, ABS_MT_CUSTOM, 0);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
			reported = true;
			if (!m_ts_debug_mode && TSP_NORMAL_EVENT_MSG) {
				location_detect(info, location, info->cur_coord[i].x, info->cur_coord[i].y);
				input_info(true, &client->dev, "[RA] tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d p:%d\n",
						i, location,
						info->cur_coord[i].x - info->pressed_x[i],
						info->cur_coord[i].y - info->pressed_y[i],
						info->move_count[i], info->finger_cnt1,
						info->cur_coord[i].x,
						info->cur_coord[i].y,
						info->cur_coord[i].palm_count);
			}
		}
		memset(&info->old_coord[i], 0, sizeof(struct ts_coordinate));
		memset(&info->cur_coord[i], 0, sizeof(struct ts_coordinate));
		info->move_count[i] = 0;
	}

	info->glove_touch = 0;

	if (reported)
		input_sync(info->input_dev);

	info->finger_cnt1 = 0;
	info->check_multi = 0;
}

#define	PALM_REPORT_WIDTH	200
#define	PALM_REJECT_WIDTH	255

void location_detect(struct zt_ts_info *info, char *loc, int x, int y)
{
	memset(loc, 0x00, 7);
	strncpy(loc, "xy:", 3);
	if (x < info->pdata->area_edge)
		strncat(loc, "E.", 2);
	else if (x < (info->pdata->x_resolution - info->pdata->area_edge))
		strncat(loc, "C.", 2);
	else
		strncat(loc, "e.", 2);
	if (y < info->pdata->area_indicator)
		strncat(loc, "S", 1);
	else if (y < (info->pdata->y_resolution - info->pdata->area_navigation))
		strncat(loc, "C", 1);
	else
		strncat(loc, "N", 1);
}

static irqreturn_t zt_touch_work(int irq, void *data)
{
	struct zt_ts_info* info = (struct zt_ts_info*)data;
	struct i2c_client *client = info->client;
	int i;
	u8 reported = false;
	u8 tid = 0;
	u8 ttype, tstatus;
	u16 x, y, z, maxX, maxY, sen_max;
	u16 st;
	u16 prox_data = 0;
	u8 info_major_w = 0;
	u8 info_minor_w = 0;
	char location[7] = "";
	int ret;
	char pos[5];
	char cur = 0;
	char old = 0;

	if (info->sleep_mode) {
		pm_wakeup_event(info->input_dev->dev.parent, 500);

		/* waiting for blsp block resuming, if not occurs i2c error */
		ret = wait_for_completion_interruptible_timeout(&info->resume_done, msecs_to_jiffies(500));
		if (ret == 0) {
			input_err(true, &info->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return IRQ_HANDLED;
		} else if (ret < 0) {
			input_err(true, &info->client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return IRQ_HANDLED;
		}
	}

	#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
	// wait info_work is done
	if (!info->info_work_done)
	{
		input_err(true, &client->dev, "%s: info_work is not done yet\n", __func__);
		return IRQ_HANDLED;
	}
	#endif

	if (gpio_get_value(info->pdata->gpio_int)) {
		input_err(true, &client->dev, "%s: Invalid interrupt\n", __func__);

		return IRQ_HANDLED;
	}

	if (!mutex_trylock(&info->work_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy work lock\n", __func__);
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		clear_report_data(info);
		return IRQ_HANDLED;
	}
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif

	if (info->work_state != NOTHING) {
		input_err(true, &client->dev, "%s: Other process occupied\n", __func__);
		usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);

		if (!gpio_get_value(info->pdata->gpio_int)) {
			write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
			clear_report_data(info);
			usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);
		}

		goto out;
	}

	if (ts_read_coord(info) == false) { /* maybe desirable reset */
		input_err(true, &client->dev, "%s: Failed to read info coord\n", __func__);
		goto out;
	}

	info->work_state = NORMAL;
	reported = false;

	if (info->touch_info[0].byte00.value.eid == CUSTOM_EVENT || info->touch_info[0].byte00.value.eid == GESTURE_EVENT)
		goto out;

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		info->old_coord[i] = info->cur_coord[i];
		memset(&info->cur_coord[i], 0, sizeof(struct ts_coordinate));
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		ttype = (info->touch_info[i].byte06.value.touch_type23 << 2) | (info->touch_info[i].byte07.value.touch_type01);
		tstatus = info->touch_info[i].byte00.value.touch_status;

		if (tstatus == FINGER_NONE && ttype != TOUCH_PROXIMITY)
			continue;

		tid = info->touch_info[i].byte00.value.tid;

		info->cur_coord[tid].id = tid;
		info->cur_coord[tid].touch_status = tstatus;
		info->cur_coord[tid].x = (info->touch_info[i].byte01.value.x_coord_h << 4) | (info->touch_info[i].byte03.value.x_coord_l);
		info->cur_coord[tid].y = (info->touch_info[i].byte02.value.y_coord_h << 4) | (info->touch_info[i].byte03.value.y_coord_l);
		info->cur_coord[tid].z = info->touch_info[i].byte06.value.z_value;
		info->cur_coord[tid].ttype = ttype;
		info->cur_coord[tid].major = info->touch_info[i].byte04.value_u8bit;
		info->cur_coord[tid].minor = info->touch_info[i].byte05.value_u8bit;
		info->cur_coord[tid].noise = info->touch_info[i].byte08.value_u8bit;
		info->cur_coord[tid].max_sense= info->touch_info[i].byte09.value_u8bit;

		if (!info->cur_coord[tid].palm && (info->cur_coord[tid].ttype == TOUCH_PALM))
			info->cur_coord[tid].palm_count++;
		info->cur_coord[tid].palm = (info->cur_coord[tid].ttype == TOUCH_PALM);

		if (info->cur_coord[tid].z <= 0)
			info->cur_coord[tid].z = 1;
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if ((info->cur_coord[i].ttype == TOUCH_PROXIMITY)
				&& (info->pdata->support_ear_detect)) {
			if (read_data(info->client, ZT_PROXIMITY_DETECT, (u8 *)&prox_data, 2) < 0)
				input_err(true, &client->dev, "%s: fail to read proximity detect reg\n", __func__);

			info->hover_event = prox_data;

			input_info(true, &client->dev, "PROXIMITY DETECT. LVL = %d \n", prox_data);
			input_report_abs(info->input_dev_proximity, ABS_MT_CUSTOM, prox_data);
			input_sync(info->input_dev_proximity);
			break;
		}
	}

	info->noise_flag = -1;
	info->flip_cover_flag = 0;

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if (info->cur_coord[i].touch_status == FINGER_NONE)
			continue;

		if ((info->noise_flag == -1) && (info->old_coord[i].noise != info->cur_coord[i].noise)) {
			info->noise_flag = info->cur_coord[i].noise;
			input_info(true, &client->dev, "NOISE MODE %s [%d]\n", info->noise_flag > 0 ? "ON":"OFF", info->noise_flag);
		}

		if (info->flip_cover_flag == 0) {
			if (info->old_coord[i].ttype != TOUCH_FLIP_COVER && info->cur_coord[i].ttype == TOUCH_FLIP_COVER) {
				info->flip_cover_flag = 1;
				input_info(true, &client->dev, "%s: FLIP COVER MODE ON\n", __func__);
			}

			if (info->old_coord[i].ttype == TOUCH_FLIP_COVER && info->cur_coord[i].ttype != TOUCH_FLIP_COVER) {
				info->flip_cover_flag = 1;
				input_info(true, &client->dev, "%s: FLIP COVER MODE OFF\n", __func__);
			}
		}

		if (info->old_coord[i].ttype != info->cur_coord[i].ttype) {
			if (info->cur_coord[i].touch_status == FINGER_PRESS)
				snprintf(pos, 5, "P");
			else if (info->cur_coord[i].touch_status == FINGER_MOVE)
				snprintf(pos, 5, "M");
			else
				snprintf(pos, 5, "R");

			if (info->cur_coord[i].ttype == TOUCH_PALM)
				cur = 'P';
			else if (info->cur_coord[i].ttype == TOUCH_GLOVE)
				cur = 'G';
			else
				cur = 'N';

			if (info->old_coord[i].ttype == TOUCH_PALM)
				old = 'P';
			else if (info->old_coord[i].ttype == TOUCH_GLOVE)
				old = 'G';
			else
				old = 'N';

			if (cur != old)
				input_info(true, &client->dev, "tID:%d ttype(%c->%c) : %s\n", i, old, cur, pos);
		}

		if ((info->cur_coord[i].touch_status == FINGER_PRESS || info->cur_coord[i].touch_status == FINGER_MOVE)) {
			x = info->cur_coord[i].x;
			y = info->cur_coord[i].y;
			z = info->cur_coord[i].z;
			info_major_w = info->cur_coord[i].major;
			info_minor_w = info->cur_coord[i].minor;
			sen_max = info->cur_coord[i].max_sense;

			maxX = info->cap_info.MaxX;
			maxY = info->cap_info.MaxY;

			if (x > maxX || y > maxY) {
				input_err(true, &client->dev,
						"Invalid coord %d : x=%d, y=%d\n", i, x, y);
				continue;
			}

			st = sen_max & 0x0F;
			if (st < 1)
				st = 1;

			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);

			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, (u32)info_major_w);

			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR, (u32)info_major_w);
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, info_minor_w);

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(info->input_dev, ABS_MT_CUSTOM, info->cur_coord[i].palm);

			input_report_key(info->input_dev, BTN_TOUCH, 1);

			if ((info->cur_coord[i].touch_status == FINGER_PRESS) && (info->cur_coord[i].touch_status != info->old_coord[i].touch_status)) {
				info->pressed_x[i] = x; /*for getting coordinates of pressed point*/
				info->pressed_y[i] = y;
				info->finger_cnt1++;

				if ((info->finger_cnt1 > 4) && (info->check_multi == 0)) {
					info->check_multi = 1;
					info->multi_count++;
					input_info(true, &client->dev,"data : pn=%d mc=%d \n", info->finger_cnt1, info->multi_count);
				}

				location_detect(info, location, x, y);
				input_info(true, &client->dev, "[P] tID:%d,%d x:%d y:%d z:%d(st:%d) max:%d major:%d minor:%d loc:%s tc:%d touch_type:%x noise:%x\n",
						i, (info->input_dev->mt->trkid - 1) & TRKID_MAX, x, y, z, st, sen_max, info_major_w,
						info_minor_w, location, info->finger_cnt1, info->cur_coord[i].ttype, info->cur_coord[i].noise);
			} else if (info->cur_coord[i].touch_status == FINGER_MOVE) {
				info->move_count[i]++;
			}
		} else if (info->cur_coord[i].touch_status == FINGER_RELEASE) {
			input_mt_slot(info->input_dev, i);
			input_report_abs(info->input_dev, ABS_MT_CUSTOM, 0);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

			location_detect(info, location, info->cur_coord[i].x, info->cur_coord[i].y);

			if (info->finger_cnt1 > 0)
				info->finger_cnt1--;

			if (info->finger_cnt1 == 0) {
				input_report_key(info->input_dev, BTN_TOUCH, 0);
				info->check_multi = 0;
			}

			input_info(true, &client->dev,
					"[R] tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d p:%d\n",
					i, location,
					info->cur_coord[i].x - info->pressed_x[i],
					info->cur_coord[i].y - info->pressed_y[i],
					info->move_count[i], info->finger_cnt1,
					info->cur_coord[i].x,
					info->cur_coord[i].y,
					info->cur_coord[i].palm_count);

			info->move_count[i] = 0;
			memset(&info->cur_coord[i], 0, sizeof(struct ts_coordinate));
		}
	}

	input_sync(info->input_dev);

out:
	if (info->work_state == NORMAL) {
#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		info->work_state = NOTHING;
	}

	mutex_unlock(&info->work_lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_INPUT_ENABLED
static int  zt_ts_open(struct input_dev *dev)
{
	struct zt_ts_info *info = misc_info;
	int ret = 0;

	if (info == NULL)
		return 0;

	if (!info->info_work_done) {
		input_err(true, &info->client->dev, "%s not finished info work\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev, "%s, %d \n", __func__, __LINE__);

	if (info->sleep_mode) {
		mutex_lock(&info->work_lock);
		info->work_state = SLEEP_MODE_OUT;
		info->sleep_mode = 0;
		input_info(true, &info->client->dev, "%s, wake up\n", __func__);

		write_cmd(info->client, ZT_WAKEUP_CMD);
		write_reg(info->client, ZT_OPTIONAL_SETTING, info->m_optional_mode.optional_mode);
		info->work_state = NOTHING;
		mutex_unlock(&info->work_lock);

#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		if (device_may_wakeup(&info->client->dev))
			disable_irq_wake(info->irq);
	} else {
		mutex_lock(&info->work_lock);
		if (info->work_state != RESUME
				&& info->work_state != EALRY_SUSPEND) {
			input_info(true, &info->client->dev, "invalid work proceedure (%d)\r\n",
					info->work_state);
			mutex_unlock(&info->work_lock);
			return 0;
		}

		ret = zt_power_control(info, POWER_ON_SEQUENCE);
		if (ret == false) {
			zt_power_control(info, POWER_OFF);
			zt_power_control(info, POWER_ON_SEQUENCE);
		}

		crc_check(info);

		if (mini_init_touch(info) == false)
			goto fail_late_resume;
		enable_irq(info->irq);
		info->work_state = NOTHING;

		if (g_ta_connected)
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_USB_DETECT_BIT, g_ta_connected);

		mutex_unlock(&info->work_lock);
		input_dbg(true, &info->client->dev, "%s--\n", __func__);
		return 0;

fail_late_resume:
		input_info(true, &info->client->dev, "%s: failed to late resume\n", __func__);
		enable_irq(info->irq);
		info->work_state = NOTHING;
		mutex_unlock(&info->work_lock);
	}

	cancel_delayed_work(&info->work_print_info);
	info->print_info_cnt_open = 0;
	info->print_info_cnt_release = 0;
	if (!shutdown_is_on_going_tsp)
		schedule_work(&info->work_print_info.work);

	return 0;
}

static void zt_ts_close(struct input_dev *dev)
{
	struct zt_ts_info *info = misc_info;
	int i;
	u8 prev_work_state;

	if (info == NULL)
		return;

	if (!info->info_work_done) {
		input_err(true, &info->client->dev, "%s not finished info work\n", __func__);
		return;
	}

	input_info(true, &info->client->dev,
			"%s, spay:%d aod:%d aot:%d singletap:%d prox:%ld pocket:%d ed:%d\n",
			__func__, info->spay_enable, info->aod_enable,
			info->aot_enable, info->singletap_enable, info->prox_power_off,
			info->pocket_enable, info->ed_enable);

#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	if (info->touch_mode == TOUCH_AGING_MODE) {
		ts_set_touchmode(TOUCH_POINT_MODE);
#if ESD_TIMER_INTERVAL
		write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
		input_info(true, &info->client->dev, "%s, set touch mode\n", __func__);
	}

	if (((info->spay_enable || info->aod_enable || info->aot_enable || info->singletap_enable))
			|| info->pocket_enable || info->ed_enable || info->fod_enable || info->fod_lp_mode) {
		mutex_lock(&info->work_lock);
		prev_work_state = info->work_state;
		info->work_state = SLEEP_MODE_IN;
		input_info(true, &info->client->dev, "%s, sleep mode\n", __func__);

#if ESD_TIMER_INTERVAL
		esd_timer_stop(info);
#endif
		ts_set_utc_sponge(info);

		if (info->prox_power_off && info->aot_enable)
			zinitix_bit_clr(info->lpm_mode, ZT_SPONGE_MODE_DOUBLETAP_WAKEUP);

		input_info(true, &info->client->dev,
				"%s: write lpm_mode 0x%02x (spay:%d, aod:%d, singletap:%d, aot:%d, fod:%d, fod_lp:%d)\n",
				__func__, info->lpm_mode,
				(info->lpm_mode & (1 << ZT_SPONGE_MODE_SPAY)) ? 1 : 0,
				(info->lpm_mode & (1 << ZT_SPONGE_MODE_AOD)) ? 1 : 0,
				(info->lpm_mode & (1 << ZT_SPONGE_MODE_SINGLETAP)) ? 1 : 0,
				(info->lpm_mode & (1 << ZT_SPONGE_MODE_DOUBLETAP_WAKEUP)) ? 1 : 0,
				info->fod_enable, info->fod_lp_mode);

		ts_write_to_sponge(info, ZT_SPONGE_LP_FEATURE, &info->lpm_mode, 2);
		zt_set_fod_rect(info);

		write_cmd(info->client, ZT_SLEEP_CMD);
		info->sleep_mode = 1;

		if (info->aot_enable)
			zinitix_bit_set(info->lpm_mode, ZT_SPONGE_MODE_DOUBLETAP_WAKEUP);

		/* clear garbage data */
		for (i = 0; i < 2; i++) {
			zt_delay(10);
			write_cmd(info->client, ZT_CLEAR_INT_STATUS_CMD);
		}
		clear_report_data(info);

		info->work_state = prev_work_state;
		if (device_may_wakeup(&info->client->dev))
			enable_irq_wake(info->irq);
	} else {
		disable_irq(info->irq);
		mutex_lock(&info->work_lock);
		if (info->work_state != NOTHING) {
			input_info(true, &info->client->dev, "invalid work proceedure (%d)\r\n",
					info->work_state);
			mutex_unlock(&info->work_lock);
			enable_irq(info->irq);
			return;
		}
		info->work_state = EALRY_SUSPEND;

		clear_report_data(info);

#if ESD_TIMER_INTERVAL
		/*write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);*/
		esd_timer_stop(info);
#endif

		zt_power_control(info, POWER_OFF);
	}

	cancel_delayed_work(&info->work_print_info);
	zt_print_info(info);

	input_info(true, &info->client->dev, "%s --\n", __func__);
	mutex_unlock(&info->work_lock);
	return;
}
#endif	/* CONFIG_INPUT_ENABLED */

static int ts_set_touchmode(u16 value)
{
	int i, ret = 1;
	int retry_cnt = 0;
	struct capa_info *cap = &(misc_info->cap_info);

	disable_irq(misc_info->irq);

	mutex_lock(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		input_info(true, &misc_info->client->dev, "other process occupied.. (%d)\n",
				misc_info->work_state);
		enable_irq(misc_info->irq);
		mutex_unlock(&misc_info->work_lock);
		return -1;
	}

retry_ts_set_touchmode:
	//wakeup cmd
	write_cmd(misc_info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_cmd(misc_info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	misc_info->work_state = SET_MODE;

	if (value == TOUCH_SEC_MODE)
		misc_info->touch_mode = TOUCH_POINT_MODE;
	else
		misc_info->touch_mode = value;

	input_info(true, &misc_info->client->dev, "[zinitix_touch] tsp_set_testmode %d\r\n", misc_info->touch_mode);

	if (!((misc_info->touch_mode == TOUCH_POINT_MODE) ||
				(misc_info->touch_mode == TOUCH_SENTIVITY_MEASUREMENT_MODE))) {
		if (write_reg(misc_info->client, ZT_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			input_info(true, &misc_info->client->dev, "%s: Fail to set zt_DELAY_RAW_FOR_HOST.\r\n", __func__);
	}

	if (write_reg(misc_info->client, ZT_TOUCH_MODE,
				misc_info->touch_mode) != I2C_SUCCESS)
		input_info(true, &misc_info->client->dev, "[zinitix_touch] TEST Mode : "
				"Fail to set ZINITX_TOUCH_MODE %d.\r\n", misc_info->touch_mode);

	input_info(true, &misc_info->client->dev, "%s: tsp_set_testmode. write regiter end \n", __func__);

	ret = read_data(misc_info->client, ZT_TOUCH_MODE, (u8 *)&cap->current_touch_mode, 2);
	if (ret < 0) {
		input_err(true, &misc_info->client->dev,"%s: fail touch mode read\n", __func__);
		goto out;
	}

	if (cap->current_touch_mode != misc_info->touch_mode) {
		if (retry_cnt < 1) {
			retry_cnt++;
			goto retry_ts_set_touchmode;
		}
		input_info(true, &misc_info->client->dev, "%s: fail to set touch_mode %d (current_touch_mode %d).\n",
				__func__, misc_info->touch_mode, cap->current_touch_mode);
		ret = -1;
		goto out;
	}

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		zt_delay(20);
		write_cmd(misc_info->client, ZT_CLEAR_INT_STATUS_CMD);
	}

	clear_report_data(misc_info);

	input_info(true, &misc_info->client->dev, "%s: tsp_set_testmode. garbage data end \n", __func__);

out:
	misc_info->work_state = NOTHING;
	enable_irq(misc_info->irq);
	mutex_unlock(&misc_info->work_lock);
	return ret;
}

#define REG_CHANNEL_TEST_RESULT				0x0296
#define TEST_CHANNEL_OPEN				0x0D
#define TEST_PATTERN_OPEN				0x04
#define TEST_SHORT					0x08
#define TEST_PASS					0xFF

static bool get_channel_test_result(struct zt_ts_info *info, int skip_cnt)
{
	struct i2c_client *client = info->client;
	struct zt_ts_platform_data *pdata = info->pdata;
	int i;
	int retry = 150;

	disable_irq(info->irq);

	mutex_lock(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "other process occupied.. (%d)\n",
				info->work_state);
		enable_irq(info->irq);
		mutex_unlock(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(pdata->gpio_int)) {
			zt_delay(7);
			if (--retry < 0)
				break;
		}
		retry = 150;
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		zt_delay(1);
	}

	retry = 100;
	input_info(true, &client->dev, "%s: channel_test_result read\n", __func__);

	while (gpio_get_value(pdata->gpio_int)) {
		zt_delay(30);
		if (--retry < 0)
			break;
		else
			input_info(true, &client->dev, "%s: retry:%d\n", __func__, retry);
	}

	read_data(info->client, REG_CHANNEL_TEST_RESULT, (u8 *)info->raw_data->channel_test_data, 10);

	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	clear_report_data(info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	mutex_unlock(&info->work_lock);

	return true;
}

static void run_test_open_short(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;

	zt_ts_esd_timer_stop(info);
	ts_set_touchmode(TOUCH_CHANNEL_TEST_MODE);
	get_channel_test_result(info, 2);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &client->dev, "channel_test_result : %04X\n", raw_data->channel_test_data[0]);
	input_info(true, &client->dev, "RX Channel : %08X\n",
			raw_data->channel_test_data[1] | ((raw_data->channel_test_data[2] << 16) & 0xffff0000));
	input_info(true, &client->dev, "TX Channel : %08X\n",
			raw_data->channel_test_data[3] | ((raw_data->channel_test_data[4] << 16) & 0xffff0000));

	if (raw_data->channel_test_data[0] == TEST_SHORT) {
		info->ito_test[3] |= 0x0F;
	} else if (raw_data->channel_test_data[0] == TEST_CHANNEL_OPEN || raw_data->channel_test_data[0] == TEST_PATTERN_OPEN) {
		if (raw_data->channel_test_data[3] | ((raw_data->channel_test_data[4] << 16) & 0xffff0000))
			info->ito_test[3] |= 0x10;

		if (raw_data->channel_test_data[1] | ((raw_data->channel_test_data[2] << 16) & 0xffff0000))
			info->ito_test[3] |= 0x20;
	}

	zt_ts_esd_timer_start(info);
}

/*
 *	flag     1  :  set edge handler
 *		2  :  set (portrait, normal) edge zone data
 *		4  :  set (portrait, normal) dead zone data
 *		8  :  set landscape mode data
 *		16 :  mode clear
 *	data
 *		0xAA, FFF (y start), FFF (y end),  FF(direction)
 *		0xAB, FFFF (edge zone)
 *		0xAC, FF (up x), FF (down x), FFFF (y)
 *		0xAD, FF (mode), FFF (edge), FFF (dead zone x), FF (dead zone top y), FF (dead zone bottom y)
 *	case
 *		edge handler set :  0xAA....
 *		booting time :  0xAA...  + 0xAB...
 *		normal mode : 0xAC...  (+0xAB...)
 *		landscape mode : 0xAD...
 *		landscape -> normal (if same with old data) : 0xAD, 0
 *		landscape -> normal (etc) : 0xAC....  + 0xAD, 0
 */

static void set_grip_data_to_ic(struct zt_ts_info *ts, u8 flag)
{
	struct i2c_client *client = ts->client;

	input_info(true, &ts->client->dev, "%s: flag: %02X (clr,lan,nor,edg,han)\n", __func__, flag);

	mutex_lock(&ts->power_init);

	if (flag & G_SET_EDGE_HANDLER) {
		if (ts->grip_edgehandler_direction == 0) {
			ts->grip_edgehandler_start_y = 0x0;
			ts->grip_edgehandler_end_y = 0x0;
		}

		if (write_reg(client, ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_START,
					ts->grip_edgehandler_start_y) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set except start y error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_END,
					ts->grip_edgehandler_end_y) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set except end y error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_SEL,
					(ts->grip_edgehandler_direction) & 0x0003) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set except direct error\n", __func__);
		}

		input_info(true, &ts->client->dev, "%s: 0x%02X %02X, 0x%02X %02X, 0x%02X %02X\n", __func__,
				ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_SEL, ts->grip_edgehandler_direction,
				ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_START, ts->grip_edgehandler_start_y,
				ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_END, ts->grip_edgehandler_end_y);
	}

	if (flag & G_SET_EDGE_ZONE) {
		if (write_reg(client, ZT_EDGE_GRIP_PORT_SIDE_WIDTH, ts->grip_edge_range) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set grip side width error\n", __func__);
		}

		input_info(true, &ts->client->dev, "%s: 0x%02X %02X\n", __func__,
				ZT_EDGE_GRIP_PORT_SIDE_WIDTH, ts->grip_edge_range);
	}

	if (flag & G_SET_NORMAL_MODE) {
		if (write_reg(client, ZT_EDGE_REJECT_PORT_SIDE_UP_WIDTH, ts->grip_deadzone_up_x) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set dead zone up x error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_REJECT_PORT_SIDE_DOWN_WIDTH, ts->grip_deadzone_dn_x) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set dead zone down x error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_REJECT_PORT_SIDE_UP_DOWN_DIV, ts->grip_deadzone_y) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set dead zone up/down div location error\n", __func__);
		}

		input_info(true, &ts->client->dev, "%s: 0x%02X %02X, 0x%02X %02X, 0x%02X %02X\n", __func__,
				ZT_EDGE_REJECT_PORT_SIDE_UP_WIDTH, ts->grip_deadzone_up_x,
				ZT_EDGE_REJECT_PORT_SIDE_DOWN_WIDTH, ts->grip_deadzone_dn_x,
				ZT_EDGE_REJECT_PORT_SIDE_UP_DOWN_DIV, ts->grip_deadzone_y);
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		if (write_reg(client, ZT_EDGE_LANDSCAPE_MODE, ts->grip_landscape_mode & 0x1) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set landscape mode error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_GRIP_LAND_SIDE_WIDTH, ts->grip_landscape_edge) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set landscape side edge error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_REJECT_LAND_SIDE_WIDTH, ts->grip_landscape_deadzone) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set landscape side deadzone error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_REJECT_LAND_TOP_BOT_WIDTH,
					(((ts->grip_landscape_top_deadzone << 8) & 0xFF00) | (ts->grip_landscape_bottom_deadzone & 0x00FF)))
				!= I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set landscape top bot deazone error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_GRIP_LAND_TOP_BOT_WIDTH,
					(((ts->grip_landscape_top_gripzone << 8) & 0xFF00) | (ts->grip_landscape_bottom_gripzone & 0x00FF)))
				!= I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set landscape top bot gripzone error\n", __func__);
		}

		input_info(true, &ts->client->dev,
				"%s: 0x%02X %02X, 0x%02X %02X, 0x%02X %02X, 0x%02X %02X, 0x%02X %02X\n", __func__,
				ZT_EDGE_LANDSCAPE_MODE, ts->grip_landscape_mode & 0x1,
				ZT_EDGE_GRIP_LAND_SIDE_WIDTH, ts->grip_landscape_edge,
				ZT_EDGE_REJECT_LAND_SIDE_WIDTH, ts->grip_landscape_deadzone,
				ZT_EDGE_REJECT_LAND_TOP_BOT_WIDTH,
				((ts->grip_landscape_top_deadzone << 8) & 0xFF00) | (ts->grip_landscape_bottom_deadzone & 0x00FF),
				ZT_EDGE_GRIP_LAND_TOP_BOT_WIDTH,
				((ts->grip_landscape_top_gripzone << 8) & 0xFF00) | (ts->grip_landscape_bottom_gripzone & 0x00FF));
	}

	if (flag & G_CLR_LANDSCAPE_MODE) {
		if (write_reg(client, ZT_EDGE_LANDSCAPE_MODE, ts->grip_landscape_mode) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: clr landscape mode error\n", __func__);
		}

		input_info(true, &ts->client->dev, "%s: 0x%02X %02X\n", __func__,
				ZT_EDGE_LANDSCAPE_MODE, ts->grip_landscape_mode);
	}
	mutex_unlock(&ts->power_init);
}

void zt_set_grip_type(struct zt_ts_info *ts, u8 set_type)
{
	u8 mode = G_NONE;

	input_info(true, &ts->client->dev, "%s: re-init grip(%d), edh:%d, edg:%d, lan:%d\n", __func__,
			set_type, ts->grip_edgehandler_direction, ts->grip_edge_range, ts->grip_landscape_mode);

	/* edge handler */
	if (ts->grip_edgehandler_direction != 0)
		mode |= G_SET_EDGE_HANDLER;

	if (set_type == GRIP_ALL_DATA) {
		/* edge */
		if (ts->grip_edge_range != 60)
			mode |= G_SET_EDGE_ZONE;

		/* dead zone */
		if (ts->grip_landscape_mode == 1)	/* default 0 mode, 32 */
			mode |= G_SET_LANDSCAPE_MODE;
		else
			mode |= G_SET_NORMAL_MODE;
	}

	if (mode)
		set_grip_data_to_ic(ts, mode);
}

int zt_tclm_execute_force_calibration(struct i2c_client *client, int cal_mode)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);

	if (ts_hw_calibration(info) == false)
		return -1;

	return 0;
}

#ifdef USE_MISC_DEVICE
static int ts_misc_fops_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ts_misc_fops_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long ts_misc_fops_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct raw_ioctl raw_ioctl;
	u8 *u8Data;
	int ret = 0;
	size_t sz = 0;
	u16 mode;
	struct reg_ioctl reg_ioctl;
	u16 val;
	int nval = 0;
#ifdef CONFIG_COMPAT
	void __user *argp = compat_ptr(arg);
#else
	void __user *argp = (void __user *)arg;
#endif

	if (misc_info == NULL) {
		pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
		return -1;
	}

	switch (cmd) {
	case TOUCH_IOCTL_GET_DEBUGMSG_STATE:
		ret = m_ts_debug_mode;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_SET_DEBUGMSG_STATE:
		if (copy_from_user(&nval, argp, sizeof(nval))) {
			input_err(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}
		if (nval)
			input_info(true, &misc_info->client->dev, "[zinitix_touch] on debug mode (%d)\n", nval);
		else
			input_info(true, &misc_info->client->dev, "[zinitix_touch] off debug mode (%d)\n", nval);
		m_ts_debug_mode = nval;
		break;

	case TOUCH_IOCTL_GET_CHIP_REVISION:
		ret = misc_info->cap_info.ic_revision;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_FW_VERSION:
		ret = misc_info->cap_info.fw_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_REG_DATA_VERSION:
		ret = misc_info->cap_info.reg_data_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_SIZE:
		if (copy_from_user(&sz, argp, sizeof(size_t)))
			return -1;
		if (misc_info->cap_info.ic_fw_size != sz) {
			input_info(true, &misc_info->client->dev, "%s: firmware size error\n", __func__);
			return -1;
		}
		break;

	case TOUCH_IOCTL_GET_X_RESOLUTION:
		ret = misc_info->pdata->x_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_RESOLUTION:
		ret = misc_info->pdata->y_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_X_NODE_NUM:
		ret = misc_info->cap_info.x_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_NODE_NUM:
		ret = misc_info->cap_info.y_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_TOTAL_NODE_NUM:
		ret = misc_info->cap_info.total_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_HW_CALIBRAION:
		ret = -1;
		disable_irq(misc_info->irq);
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = HW_CALIBRAION;
		zt_delay(100);

		/* h/w calibration */
		if (ts_hw_calibration(misc_info) == true)
			ret = 0;

		mode = misc_info->touch_mode;
		if (write_reg(misc_info->client,
					ZT_TOUCH_MODE, mode) != I2C_SUCCESS) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: failed to set touch mode %d.\n",
					mode);
			goto fail_hw_cal;
		}

		if (write_cmd(misc_info->client,
					ZT_SWRESET_CMD) != I2C_SUCCESS)
			goto fail_hw_cal;

		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;
fail_hw_cal:
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return -1;

	case TOUCH_IOCTL_SET_RAW_DATA_MODE:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		if (copy_from_user(&nval, argp, sizeof(nval))) {
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			misc_info->work_state = NOTHING;
			return -1;
		}
		ts_set_touchmode((u16)nval);

		return 0;

	case TOUCH_IOCTL_GET_REG:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]:other process occupied.. (%d)\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;

		if (copy_from_user(&reg_ioctl, argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}

		if (read_data(misc_info->client,
					(u16)reg_ioctl.addr, (u8 *)&val, 2) < 0)
			ret = -1;

		nval = (int)val;

#ifdef CONFIG_COMPAT
		if (copy_to_user(compat_ptr(reg_ioctl.val), (u8 *)&nval, 4)) {
#else
		if (copy_to_user((void __user *)(reg_ioctl.val), (u8 *)&nval, 4)) {
#endif
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_to_user\n", __func__);
			return -1;
		}

		input_info(true, &misc_info->client->dev, "%s read : reg addr = 0x%x, val = 0x%x\n", __func__,
				reg_ioctl.addr, nval);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_SET_REG:

		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (copy_from_user(&reg_ioctl,
					argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user(1)\n", __func__);
			return -1;
		}

#ifdef CONFIG_COMPAT
		if (copy_from_user(&val, compat_ptr(reg_ioctl.val), sizeof(val))) {
#else
		if (copy_from_user(&val,(void __user *)(reg_ioctl.val), sizeof(val))) {
#endif
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user(2)\n", __func__);
			return -1;
		}

		if (write_reg(misc_info->client,
					(u16)reg_ioctl.addr, val) != I2C_SUCCESS)
			ret = -1;

		input_info(true, &misc_info->client->dev, "%s write : reg addr = 0x%x, val = 0x%x\r\n", __func__,
				reg_ioctl.addr, val);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_DONOT_TOUCH_EVENT:

		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (write_reg(misc_info->client,
					ZT_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			ret = -1;
		input_info(true, &misc_info->client->dev, "%s write : reg addr = 0x%x, val = 0x0\r\n", __func__,
				ZT_INT_ENABLE_FLAG);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_SEND_SAVE_STATUS:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.." \
					"(%d)\r\n", misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = SET_MODE;
		ret = 0;
		write_reg(misc_info->client, VCMD_NVM_WRITE_ENABLE, 0x0001);
		if (write_cmd(misc_info->client,
					ZT_SAVE_STATUS_CMD) != I2C_SUCCESS)
			ret =  -1;

		zt_delay(1000);	/* for fusing eeprom */
		write_reg(misc_info->client, VCMD_NVM_WRITE_ENABLE, 0x0000);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_GET_RAW_DATA:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}

		if (misc_info->touch_mode == TOUCH_POINT_MODE)
			return -1;

		mutex_lock(&misc_info->raw_data_lock);
		if (misc_info->update == 0) {
			mutex_unlock(&misc_info->raw_data_lock);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
					argp, sizeof(struct raw_ioctl))) {
			mutex_unlock(&misc_info->raw_data_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}

		misc_info->update = 0;

		u8Data = (u8 *)&misc_info->cur_data[0];
		if (raw_ioctl.sz > MAX_TRAW_DATA_SZ*2)
			raw_ioctl.sz = MAX_TRAW_DATA_SZ*2;
#ifdef CONFIG_COMPAT
		if (copy_to_user(compat_ptr(raw_ioctl.buf), (u8 *)u8Data, raw_ioctl.sz)) {
#else
		if (copy_to_user((void __user *)(raw_ioctl.buf), (u8 *)u8Data, raw_ioctl.sz)) {
#endif
			mutex_unlock(&misc_info->raw_data_lock);
			return -1;
		}

		mutex_unlock(&misc_info->raw_data_lock);
		return 0;

	default:
		break;
	}
	return 0;
}
#endif

#ifdef CONFIG_OF
static int zt_pinctrl_configure(struct zt_ts_info *info, bool active)
{
	struct device *dev = &info->client->dev;
	struct pinctrl_state *pinctrl_state;
	int retval = 0;

	input_dbg(true, dev, "%s: pinctrl %d\n", __func__, active);

	if (active)
		pinctrl_state = pinctrl_lookup_state(info->pinctrl, "on_state");
	else
		pinctrl_state = pinctrl_lookup_state(info->pinctrl, "off_state");

	if (IS_ERR(pinctrl_state)) {
		input_err(true, dev, "%s: Failed to lookup pinctrl.\n", __func__);
	} else {
		retval = pinctrl_select_state(info->pinctrl, pinctrl_state);
		if (retval)
			input_err(true, dev, "%s: Failed to configure pinctrl.\n", __func__);
	}
	return 0;
}

static int zt_power_ctrl(void *data, bool on)
{
	struct zt_ts_info* info = (struct zt_ts_info*)data;
	struct zt_ts_platform_data *pdata = info->pdata;
	struct device *dev = &info->client->dev;
	struct regulator *regulator_dvdd = NULL;
	struct regulator *regulator_avdd;
	int retval = 0;
	static bool enabled;

	if (enabled == on)
		return retval;

	if (!pdata->gpio_ldo_en) {
		regulator_dvdd = regulator_get(NULL, pdata->regulator_dvdd);
		if (IS_ERR(regulator_dvdd)) {
			input_err(true, dev, "%s: Failed to get %s regulator.\n",
				 __func__, pdata->regulator_dvdd);
			return PTR_ERR(regulator_dvdd);
		}
	}
	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_avdd);
		return PTR_ERR(regulator_avdd);
	}

	input_info(true, dev, "%s: %s\n", __func__, on ? "on" : "off");

	if (on) {
		retval = regulator_enable(regulator_avdd);
		if (retval) {
			input_err(true, dev, "%s: Failed to enable avdd: %d\n", __func__, retval);
			return retval;
		}
		if (!pdata->gpio_ldo_en) {
			retval = regulator_enable(regulator_dvdd);
			if (retval) {
				input_err(true, dev, "%s: Failed to enable vdd: %d\n", __func__, retval);
				return retval;
			}
		}
	} else {
		if (!pdata->gpio_ldo_en) {
			if (regulator_is_enabled(regulator_dvdd))
				regulator_disable(regulator_dvdd);
		}
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);
	}

	enabled = on;
	if (!pdata->gpio_ldo_en)
		regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);

	return retval;
}


static int zinitix_init_gpio(struct zt_ts_platform_data *pdata)
{
	int ret = 0;

	ret = gpio_request(pdata->gpio_int, "zinitix_tsp_irq");
	if (ret) {
		pr_err("[TSP]%s: unable to request zinitix_tsp_irq [%d]\n",
				__func__, pdata->gpio_int);
		return ret;
	}

	return ret;
}

static int zt_ts_parse_dt(struct device_node *np,
		struct device *dev,
		struct zt_ts_platform_data *pdata)
{
	int ret = 0;
	u32 temp;
	u32 px_zone[3] = { 0 };

	ret = of_property_read_u32(np, "zinitix,x_resolution", &temp);
	if (ret) {
		input_info(true, dev, "%s: Unable to read controller version\n", __func__);
		return ret;
	} else {
		pdata->x_resolution = (u16) temp;
	}

	ret = of_property_read_u32(np, "zinitix,y_resolution", &temp);
	if (ret) {
		input_info(true, dev, "%s: Unable to read controller version\n", __func__);
		return ret;
	} else {
		pdata->y_resolution = (u16) temp;
	}

	if (of_property_read_u32_array(np, "zinitix,area-size", px_zone, 3)) {
		dev_info(dev, "%s: Failed to get zone's size\n", __func__);
		pdata->area_indicator = 48;
		pdata->area_navigation = 96;
		pdata->area_edge = 60;
	} else {
		pdata->area_indicator = (u8) px_zone[0];
		pdata->area_navigation = (u8) px_zone[1];
		pdata->area_edge = (u8) px_zone[2];
	}

	ret = of_property_read_u32(np, "zinitix,page_size", &temp);
	if (ret) {
		input_info(true, dev, "%s: Unable to read controller version\n", __func__);
		return ret;
	} else {
		pdata->page_size = (u16) temp;
	}

	pdata->gpio_int = of_get_named_gpio(np, "zinitix,irq_gpio", 0);
	if (pdata->gpio_int < 0) {
		pr_err("%s: of_get_named_gpio failed: tsp_gpio %d\n", __func__,
				pdata->gpio_int);
		return -EINVAL;
	}

	if (of_get_property(np, "zinitix,gpio_ldo_en", NULL)) {
			pdata->gpio_ldo_en = true;
	} else {
		if (of_property_read_string(np, "zinitix,regulator_dvdd", &pdata->regulator_dvdd)) {
			input_err(true, dev, "Failed to get regulator_dvdd name property\n");
			return -EINVAL;
		}
	}

	if (of_property_read_string(np, "zinitix,regulator_avdd", &pdata->regulator_avdd)) {
		input_err(true, dev, "Failed to get regulator_avdd name property\n");
		return -EINVAL;
	}

	pdata->tsp_power = zt_power_ctrl;

	/* Optional parmeters(those values are not mandatory)
	 * do not return error value even if fail to get the value
	 */
	of_property_read_string(np, "zinitix,firmware_name", &pdata->firmware_name);
	of_property_read_string(np, "zinitix,chip_name", &pdata->chip_name);

	pdata->support_spay = of_property_read_bool(np, "zinitix,spay");
	pdata->support_aod = of_property_read_bool(np, "zinitix,aod");
	pdata->support_aot = of_property_read_bool(np, "zinitix,aot");
	pdata->support_ear_detect = of_property_read_bool(np, "support_ear_detect_mode");
	pdata->mis_cal_check = of_property_read_bool(np, "zinitix,mis_cal_check");
	pdata->support_dex = of_property_read_bool(np, "support_dex_mode");
	pdata->support_open_short_test = of_property_read_bool(np, "support_open_short_test");

	of_property_read_u32(np, "zinitix,bringup", &pdata->bringup);

	input_err(true, dev, "%s: x_r:%d, y_r:%d FW:%s, CN:%s,"
			" Spay:%d, AOD:%d, AOT:%d, ED:%d, Bringup:%d, MISCAL:%d, DEX:%d, OPEN/SHORT:%d"
			" \n",  __func__, pdata->x_resolution, pdata->y_resolution,
			pdata->firmware_name, pdata->chip_name,
			pdata->support_spay, pdata->support_aod, pdata->support_aot,
			pdata->support_ear_detect, pdata->bringup, pdata->mis_cal_check,
			pdata->support_dex, pdata->support_open_short_test);

	return 0;
}
#endif

/* print raw data at booting time */
static void zt_display_rawdata_boot(struct zt_ts_info *info, struct tsp_raw_data *raw_data, int *min, int *max, bool is_mis_cal)
{
	int x_num = info->cap_info.x_node_num;
	int y_num = info->cap_info.y_node_num;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int tmp_rawdata;
	int i, j;

	input_raw_info(true, &info->client->dev, "%s: %s\n", __func__, is_mis_cal ? "mis_cal ": "dnd ");

	pStr = kzalloc(6 * (x_num + 1), GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, 6 * (x_num + 1));
	snprintf(pTmp, sizeof(pTmp), "      Rx");
	strlcat(pStr, pTmp, 6 * (x_num + 1));

	for (i = 0; i < x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d  ", i);
		strlcat(pStr, pTmp, 6 * (x_num + 1));
	}

	input_raw_info(true, &info->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, 6 * (x_num + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, 6 * (x_num + 1));

	for (i = 0; i < x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "-----");
		strlcat(pStr, pTmp, 6 * (x_num + 1));
	}

	input_raw_info(true, &info->client->dev, "%s\n", pStr);

	for (i = 0; i < y_num; i++) {
		memset(pStr, 0x0, 6 * (x_num + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", i);
		strlcat(pStr, pTmp, 6 * (x_num + 1));

		for (j = 0; j < x_num; j++) {

			if (is_mis_cal) {
				/* print mis_cal data (value - DEF_MIS_CAL_SPEC_MID) */
				tmp_rawdata = raw_data->reference_data_abnormal[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);

				if (tmp_rawdata < *min)
					*min = tmp_rawdata;

				if (tmp_rawdata > *max)
					*max = tmp_rawdata;

			} else {
				/* print dnd data */
				tmp_rawdata = raw_data->dnd_data[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);

				if (tmp_rawdata < *min && tmp_rawdata != 0)
					*min = tmp_rawdata;

				if (tmp_rawdata > *max)
					*max = tmp_rawdata;
			}
			strlcat(pStr, pTmp, 6 * (x_num + 1));
		}
		input_raw_info(true, &info->client->dev, "%s\n", pStr);
	}

	input_raw_info(true, &info->client->dev, "Max/Min %d,%d ##\n", *max, *min);

	kfree(pStr);
}

static void zt_run_dnd(struct zt_ts_info *info)
{
	struct tsp_raw_data *raw_data = info->raw_data;
	int min = 0xFFFF, max = -0xFF;
	int ret;

	zt_ts_esd_timer_stop(info);
	ret = ts_set_touchmode(TOUCH_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	zt_display_rawdata_boot(info, raw_data, &min, &max, false);

out:
	zt_ts_esd_timer_start(info);
	return;
}

static void zt_run_mis_cal(struct zt_ts_info *info)
{
	struct zt_ts_platform_data *pdata = info->pdata;
	struct tsp_raw_data *raw_data = info->raw_data;

	char mis_cal_data = 0xF0;
	int ret = 0;
	u16 chip_eeprom_info;
	int min = 0xFFFF, max = -0xFF;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	if (pdata->mis_cal_check == 0) {
		input_info(true, &info->client->dev, "%s: [ERROR] not support\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",__func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	if (read_data(info->client, ZT_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0) {
		input_info(true, &info->client->dev, "%s: read eeprom_info i2c fail!\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		input_info(true, &info->client->dev, "%s: eeprom cal 0, skip !\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	ret = ts_set_touchmode(TOUCH_REF_ABNORMAL_TEST_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ret = get_raw_data(info, (u8 *)raw_data->reference_data_abnormal, 2);
	if (!ret) {
		input_info(true, &info->client->dev, "%s:[ERROR] i2c fail!\n", __func__);
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ts_set_touchmode(TOUCH_POINT_MODE);

	zt_display_rawdata_boot(info, raw_data, &min, &max, true);
	if ((min + DEF_MIS_CAL_SPEC_MID) < DEF_MIS_CAL_SPEC_MIN ||
			(max + DEF_MIS_CAL_SPEC_MID) > DEF_MIS_CAL_SPEC_MAX) {
		mis_cal_data = 0xFD;
		goto NG;
	}

	mis_cal_data = 0x00;
	input_info(true, &info->client->dev, "%s : mis_cal_data: %X\n", __func__, mis_cal_data);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return;
NG:
	input_err(true, &info->client->dev, "%s : mis_cal_data: %X\n", __func__, mis_cal_data);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return;
}

static void zt_run_rawdata(struct zt_ts_info *info)
{
	info->tsp_dump_lock = 1;
	input_raw_data_clear();
	input_raw_info(true, &info->client->dev, "%s: start ##\n", __func__);
	zt_run_dnd(info);
	zt_run_mis_cal(info);
	input_raw_info(true, &info->client->dev, "%s: done ##\n", __func__);
	info->tsp_dump_lock = 0;
}

static void zt_read_info_work(struct work_struct *work)
{
	struct zt_ts_info *info = container_of(work, struct zt_ts_info,
			work_read_info.work);
	
	mutex_lock(&info->modechange);

	run_test_open_short(info);

	input_log_fix();
	zt_run_rawdata(info);
	info->info_work_done = true;
	mutex_unlock(&info->modechange);
}

void zt_print_info(struct zt_ts_info *info)
{
	u16 fw_version = 0;

	if (!info)
		return;

	if (!info->client)
		return;

	info->print_info_cnt_open++;

	if (info->print_info_cnt_open > 0xfff0)
		info->print_info_cnt_open = 0;

	if (info->finger_cnt1 == 0)
		info->print_info_cnt_release++;

	fw_version =  ((info->cap_info.hw_id & 0xff) << 8) | (info->cap_info.reg_data_version & 0xff);

	input_info(true, &info->client->dev,
			"tc:%d noise:%s(%d) cover:%d lp:(%x) fod:%d ED:%d // v:%04X C%02XT%04X.%4s%s // #%d %d\n",
			info->finger_cnt1, info->noise_flag > 0 ? "ON":"OFF", info->noise_flag, info->flip_cover_flag,
			info->lpm_mode, info->fod_mode_set, info->ed_enable,
			fw_version, 0,0," "," ",
			info->print_info_cnt_open, info->print_info_cnt_release);
}

static void touch_print_info_work(struct work_struct *work)
{
	struct zt_ts_info *info = container_of(work, struct zt_ts_info,
			work_print_info.work);

	zt_print_info(info);

	if (!shutdown_is_on_going_tsp)
		schedule_delayed_work(&info->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

static void zt_ts_set_input_prop(struct zt_ts_info *info, struct input_dev *dev, u8 propbit)
{
	static char zt_phys[64] = { 0 };

	snprintf(zt_phys, sizeof(zt_phys),
			"%s/input1", dev->name);
	dev->id.bustype = BUS_I2C;
	dev->phys = zt_phys;
	dev->dev.parent = &info->client->dev;

	set_bit(EV_SYN, dev->evbit);
	set_bit(EV_KEY, dev->evbit);
	set_bit(EV_ABS, dev->evbit);
	set_bit(BTN_TOUCH, dev->keybit);
	set_bit(propbit, dev->propbit);
	set_bit(KEY_INT_CANCEL,dev->keybit);
	set_bit(EV_LED, dev->evbit);
	set_bit(LED_MISC, dev->ledbit);

	set_bit(KEY_BLACK_UI_GESTURE,dev->keybit);
	set_bit(KEY_WAKEUP, dev->keybit);


	input_set_abs_params(dev, ABS_MT_POSITION_X,
			0, info->pdata->x_resolution + ABS_PT_OFFSET,	0, 0);
	input_set_abs_params(dev, ABS_MT_POSITION_Y,
			0, info->pdata->y_resolution + ABS_PT_OFFSET,	0, 0);
	input_set_abs_params(dev, ABS_MT_TOUCH_MAJOR,
			0, 255, 0, 0);
	input_set_abs_params(dev, ABS_MT_WIDTH_MAJOR,
			0, 255, 0, 0);
	input_set_abs_params(dev, ABS_MT_TOUCH_MINOR,
			0, 255, 0, 0);
	input_set_abs_params(dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);

	set_bit(MT_TOOL_FINGER, dev->keybit);
	if (propbit == INPUT_PROP_POINTER)
		input_mt_init_slots(dev, MAX_SUPPORTED_FINGER_NUM, INPUT_MT_POINTER);
	else
		input_mt_init_slots(dev, MAX_SUPPORTED_FINGER_NUM, INPUT_MT_DIRECT);

	input_set_drvdata(dev, info);
}

static void zt_ts_set_input_prop_proximity(struct zt_ts_info *info, struct input_dev *dev)
{
	static char zt_phys[64] = { 0 };

	snprintf(zt_phys, sizeof(zt_phys), "%s/input1", dev->name);
	dev->phys = zt_phys;
	dev->id.bustype = BUS_I2C;
	dev->dev.parent = &info->client->dev;

	set_bit(EV_SYN, dev->evbit);
	set_bit(EV_SW, dev->evbit);

	set_bit(INPUT_PROP_DIRECT, dev->propbit);

	input_set_abs_params(dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);
	input_set_abs_params(dev, ABS_MT_CUSTOM2, 0, 0xFFFFFFFF, 0, 0);
	input_set_drvdata(dev, info);
}

static int zt_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct zt_ts_platform_data *pdata = client->dev.platform_data;
	struct zt_ts_info *info;
	struct device_node *np = client->dev.of_node;
	int ret = 0;
	bool force_update = false;
	int lcdtype = 0;
	
	input_info(true, &client->dev, "%s: lcdtype: %X\n", __func__, lcdtype);

	if (client->dev.of_node) {
		if (!pdata) {
			pdata = devm_kzalloc(&client->dev,
					sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = zt_ts_parse_dt(np, &client->dev, pdata);
		if (ret) {
			input_err(true, &client->dev, "Error parsing dt %d\n", ret);
			goto err_no_platform_data;
		}
		ret = zinitix_init_gpio(pdata);
		if (ret < 0)
			goto err_gpio_request;

	} else if (!pdata) {
		input_err(true, &client->dev, "%s: Not exist platform data\n", __func__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s: Not compatible i2c function\n", __func__);
		return -EIO;
	}

	info = kzalloc(sizeof(struct zt_ts_info), GFP_KERNEL);
	if (!info) {
		input_err(true, &client->dev, "%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, info);
	info->client = client;
	info->pdata = pdata;

	INIT_DELAYED_WORK(&info->work_read_info, zt_read_info_work);
	INIT_DELAYED_WORK(&info->work_print_info, touch_print_info_work);

	mutex_init(&info->modechange);
	mutex_init(&info->set_reg_lock);
	mutex_init(&info->set_lpmode_lock);
	mutex_init(&info->work_lock);
	mutex_init(&info->raw_data_lock);
	mutex_init(&info->i2c_mutex);
	mutex_init(&info->sponge_mutex);
	mutex_init(&info->power_init);
#if ESD_TIMER_INTERVAL
	mutex_init(&info->lock);
	INIT_WORK(&info->tmr_work, ts_tmr_work);
#endif

	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		input_err(true, &client->dev, "%s: Failed to allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (pdata->support_dex) {
		info->input_dev_pad = input_allocate_device();
		if (!info->input_dev_pad) {
			input_err(true, &client->dev, "%s: allocate device err!\n", __func__);
			ret = -ENOMEM;
			goto err_allocate_input_dev_pad;
		}
	}

	if (pdata->support_ear_detect) {
		info->input_dev_proximity = input_allocate_device();
		if (!info->input_dev_proximity) {
			input_err(true, &client->dev, "%s: allocate input_dev_proximity err!\n", __func__);
			ret = -ENOMEM;
			goto err_allocate_input_dev_proximity;
		}

		info->input_dev_proximity->name = "sec_touchproximity";
		zt_ts_set_input_prop_proximity(info, info->input_dev_proximity);
	}

	info->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(info->pinctrl)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl data\n", __func__);
		ret = PTR_ERR(info->pinctrl);
		goto err_get_pinctrl;
	}

	info->work_state = PROBE;

	// power on
	if (zt_power_control(info, POWER_ON_SEQUENCE) == false) {
		input_err(true, &info->client->dev,
				"%s: POWER_ON_SEQUENCE failed", __func__);
		force_update = true;
	}

	/* init touch mode */
	info->touch_mode = TOUCH_POINT_MODE;
	misc_info = info;

#if ESD_TIMER_INTERVAL
	esd_tmr_workqueue =
		create_singlethread_workqueue("esd_tmr_workqueue");

	if (!esd_tmr_workqueue) {
		input_err(true, &client->dev, "%s: Failed to create esd tmr work queue\n", __func__);
		ret = -EPERM;

		goto err_esd_sequence;
	}

	esd_timer_init(info);
#endif

	ret = ic_version_check(info);
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: fail version check", __func__);
		force_update = true;
	}

	ret = fw_update_work(info, force_update);
	if (ret < 0) {
		ret = -EPERM;
		input_err(true, &info->client->dev,
				"%s: fail update_work", __func__);
		goto err_fw_update;
	}

	info->input_dev->name = "sec_touchscreen";
	zt_ts_set_input_prop(info, info->input_dev, INPUT_PROP_DIRECT);
	ret = input_register_device(info->input_dev);
	if (ret) {
		input_info(true, &client->dev, "unable to register %s input device\r\n",
				info->input_dev->name);
		goto err_input_register_device;
	}

	if (pdata->support_dex) {
		info->input_dev_pad->name = "sec_touchpad";
		zt_ts_set_input_prop(info, info->input_dev_pad, INPUT_PROP_POINTER);
	}

	if (pdata->support_dex) {
		ret = input_register_device(info->input_dev_pad);
		if (ret) {
			input_err(true, &client->dev, "%s: Unable to register %s input device\n", __func__, info->input_dev_pad->name);
			goto err_input_pad_register_device;
		}
	}

	if (pdata->support_ear_detect) {
		ret = input_register_device(info->input_dev_proximity);
		if (ret) {
			input_err(true, &client->dev, "%s: Unable to register %s input device\n",
					__func__, info->input_dev_proximity->name);
			goto err_input_proximity_register_device;
		}
	}

	if (init_touch(info) == false) {
		ret = -EPERM;
		goto err_init_touch;
	}

	info->work_state = NOTHING;

	init_completion(&info->resume_done);
	complete_all(&info->resume_done);

	/* configure irq */
	info->irq = gpio_to_irq(pdata->gpio_int);
	if (info->irq < 0) {
		input_info(true, &client->dev, "%s: error. gpio_to_irq(..) function is not \
				supported? you should define GPIO_TOUCH_IRQ.\r\n", __func__);
		ret = -EINVAL;
		goto error_gpio_irq;
	}

	/* ret = request_threaded_irq(info->irq, ts_int_handler, zt_touch_work,*/
	ret = request_threaded_irq(info->irq, NULL, zt_touch_work,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT , ZT_TS_DEVICE, info);

	if (ret) {
		input_info(true, &client->dev, "unable to register irq.(%s)\r\n",
				info->input_dev->name);
		goto err_request_irq;
	}
	input_info(true, &client->dev, "%s\n", __func__);

	info->raw_data = kzalloc(sizeof(struct tsp_raw_data), GFP_KERNEL);
	if (unlikely(!info->raw_data)) {
		input_err(true, &info->client->dev, "%s: Failed to allocate memory\n",
			__func__);
		ret = -ENOMEM;

	    goto err_alloc;
	}

#ifdef CONFIG_INPUT_ENABLED
	info->input_dev->open = zt_ts_open;
	info->input_dev->close = zt_ts_close;
#endif

#ifdef USE_MISC_DEVICE
	ret = misc_register(&touch_misc_device);
	if (ret) {
		input_err(true, &client->dev, "%s: Failed to register touch misc device\n", __func__);
		goto err_misc_register;
	}
#endif
	info->register_cb = info->pdata->register_cb;

	info->callbacks.inform_charger = zt_charger_status_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);
	
	device_init_wakeup(&client->dev, true);

	schedule_delayed_work(&info->work_read_info, msecs_to_jiffies(5000));

	input_log_fix();
	return 0;

#ifdef USE_MISC_DEVICE
err_misc_register:
	free_irq(info->irq, info);
#endif
err_request_irq:
error_gpio_irq:
err_init_touch:
	if (pdata->support_ear_detect) {
		input_unregister_device(info->input_dev_proximity);
		info->input_dev_proximity = NULL;
	}
err_input_proximity_register_device:
	if (pdata->support_dex) {
		input_unregister_device(info->input_dev_pad);
		info->input_dev_pad = NULL;
	}
err_input_pad_register_device:
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;
err_input_register_device:
err_fw_update:
#if ESD_TIMER_INTERVAL
	del_timer(&(info->esd_timeout_tmr));
err_esd_sequence:
#endif
	zt_power_control(info, POWER_OFF);
err_get_pinctrl:
	if (pdata->support_ear_detect) {
		if (info->input_dev_proximity)
			input_free_device(info->input_dev_proximity);
	}
err_allocate_input_dev_proximity:
	if (pdata->support_dex) {
		if (info->input_dev_pad)
			input_free_device(info->input_dev_pad);
	}
err_allocate_input_dev_pad:
	if (info->input_dev)
		input_free_device(info->input_dev);
err_alloc:
	kfree(info);
err_gpio_request:
err_no_platform_data:
	if (IS_ENABLED(CONFIG_OF))
		devm_kfree(&client->dev, (void *)pdata);

	input_info(true, &client->dev, "%s: Failed to probe\n", __func__);
	input_log_fix();
	return ret;
}

static int zt_ts_remove(struct i2c_client *client)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	struct zt_ts_platform_data *pdata = info->pdata;

	disable_irq(info->irq);
	mutex_lock(&info->work_lock);

	info->work_state = REMOVE;

	cancel_delayed_work_sync(&info->work_read_info);
	cancel_delayed_work_sync(&info->work_print_info);
	kfree(info->raw_data);

#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);
	esd_timer_stop(info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s: Stopped esd timer\n", __func__);
#endif
	destroy_workqueue(esd_tmr_workqueue);
#endif

	if (info->irq)
		free_irq(info->irq, info);
#ifdef USE_MISC_DEVICE
	misc_deregister(&touch_misc_device);
#endif

	if (gpio_is_valid(pdata->gpio_int) != 0)
		gpio_free(pdata->gpio_int);

	if (pdata->support_dex) {
		input_mt_destroy_slots(info->input_dev_pad);
		input_unregister_device(info->input_dev_pad);
	}

	if (pdata->support_ear_detect) {
		input_mt_destroy_slots(info->input_dev_proximity);
		input_unregister_device(info->input_dev_proximity);
	}

	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	mutex_unlock(&info->work_lock);
	kfree(info);

	return 0;
}

void zt_ts_shutdown(struct i2c_client *client)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);

	input_info(true, &client->dev, "%s++\n",__func__);
	shutdown_is_on_going_tsp = true;
	disable_irq(info->irq);
	mutex_lock(&info->work_lock);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	esd_timer_stop(info);
#endif
	mutex_unlock(&info->work_lock);
	zt_power_control(info, POWER_OFF);
	input_info(true, &client->dev, "%s--\n",__func__);
}

#ifdef CONFIG_PM
static int zt_ts_pm_suspend(struct device *dev)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);

	reinit_completion(&info->resume_done);

	return 0;
}

static int zt_ts_pm_resume(struct device *dev)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);

	complete_all(&info->resume_done);

	return 0;
}

static const struct dev_pm_ops zt_ts_dev_pm_ops = {
	.suspend = zt_ts_pm_suspend,
	.resume = zt_ts_pm_resume,
};
#endif

static struct i2c_device_id zt_idtable[] = {
	{ZT_TS_DEVICE, 0},
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id zinitix_match_table[] = {
	{ .compatible = "zinitix,zt_ts_device",},
	{},
};
#endif

static struct i2c_driver zt_ts_driver = {
	.probe	= zt_ts_probe,
	.remove	= zt_ts_remove,
	.shutdown = zt_ts_shutdown,
	.id_table	= zt_idtable,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= ZT_TS_DEVICE,
#ifdef CONFIG_OF
		.of_match_table = zinitix_match_table,
#endif
#ifdef CONFIG_PM
		.pm = &zt_ts_dev_pm_ops,
#endif
	},
};

static int __init zt_ts_init(void)
{
	pr_info("[TSP]: %s\n", __func__);
	sec_tsp_log_init();
	return i2c_add_driver(&zt_ts_driver);
}

static void __exit zt_ts_exit(void)
{
	i2c_del_driver(&zt_ts_driver);
}

module_init(zt_ts_init);
module_exit(zt_ts_exit);

MODULE_DESCRIPTION("touch-screen device driver using i2c interface");
MODULE_AUTHOR("<mika.kim@samsung.com>");
MODULE_LICENSE("GPL");
