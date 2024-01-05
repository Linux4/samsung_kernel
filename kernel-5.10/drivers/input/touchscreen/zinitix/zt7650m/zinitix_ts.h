/*
 *
 * Zinitix ZT touch driver
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

#ifndef _LINUX_ZT_TS_H
#define _LINUX_ZT_TS_H

#undef TSP_VERBOSE_DEBUG

#include <linux/miscdevice.h>
#include <linux/timer.h>
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/muic/common/muic.h>
#include <linux/muic/common/muic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#endif
#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
#include <linux/t-base-tui.h>
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include "../../../sec_input/sec_secure_touch.h"
#endif

//#define SPU_FW_SIGNED
#ifdef SPU_FW_SIGNED
#include <linux/spu-verify.h>
#endif

#include "../../../sec_input/sec_input.h"

#define ZINITIX_REG_ADDR_LENGTH	2

#define TSP_TYPE_BUILTIN_FW			0
#define TSP_TYPE_EXTERNAL_FW			1
#define TSP_TYPE_EXTERNAL_FW_SIGNED		2
#define TSP_TYPE_SPU_FW_SIGNED			3

#define ZINITIX_DEBUG					0
#define PDIFF_DEBUG					1
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#define USE_MISC_DEVICE
#endif

#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
#include "../../../sec_input/sec_tclm_v2.h"
#define TCLM_CONCEPT
#endif

#define TOUCH_POINT_MODE			0
#define MAX_SUPPORTED_FINGER_NUM	10 /* max 10 */

#define ABS_PT_OFFSET			(-1)	/* resolution offset */

#define USE_CHECKSUM			1
#define CHECK_HWID			0

#define CHIP_OFF_DELAY			300 /*ms*/
#define CHIP_ON_DELAY			100 /*ms*/
#define FIRMWARE_ON_DELAY		150 /*ms*/

#define DELAY_FOR_SIGNAL_DELAY		30 /*us*/
#define DELAY_FOR_TRANSCATION		50
#define DELAY_FOR_POST_TRANSCATION	10

#define TS_DRVIER_VERSION	"1.0.18_1"

#define ZT_TS_DEVICE		"zt_ts_device"
#define ZT_VENDOR_NAME "ZINITIX"
#define ZT_CHIP_NAME "ZT"

#define	PALM_REPORT_WIDTH	200
#define	PALM_REJECT_WIDTH	255

/* TCLM_CONCEPT */
#define ZT_TS_NVM_OFFSET_FAC_RESULT			0
#define ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT		2

#define ZT_TS_NVM_OFFSET_CAL_COUNT			4
#define ZT_TS_NVM_OFFSET_TUNE_VERSION			5
#define ZT_TS_NVM_OFFSET_CAL_POSITION			7
#define ZT_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT		8
#define ZT_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP		9
#define ZT_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO		10
#define ZT_TS_NVM_OFFSET_HISTORY_QUEUE_SIZE		20

#define ZT_TS_NVM_OFFSET_LENGTH		(ZT_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO + ZT_TS_NVM_OFFSET_HISTORY_QUEUE_SIZE)

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
#define DEF_IUM_ADDR_OFFSET		0xB000
#else
#define DEF_IUM_ADDR_OFFSET		0xF0A0
#endif
#define DEF_IUM_LOCK			0xF0F6
#define DEF_IUM_UNLOCK			0xF0FA
#define DEF_IUM_SAVE_CMD		0xF0F8

/* sponge enable feature */
#define ZT_SPONGE_MODE_SPAY				1
#define ZT_SPONGE_MODE_AOD				2
#define ZT_SPONGE_MODE_SINGLETAP		3
#define ZT_SPONGE_MODE_PRESS			4
#define ZT_SPONGE_MODE_DOUBLETAP_WAKEUP	5

#define ZT_SPONGE_LP_FEATURE		0x0000
#define ZT_SPONGE_TOUCHBOX_W_OFFSET 0x0002
#define ZT_SPONGE_TOUCHBOX_H_OFFSET 0x0004
#define ZT_SPONGE_TOUCHBOX_X_OFFSET 0x0006
#define ZT_SPONGE_TOUCHBOX_Y_OFFSET 0x0008
#define ZT_SPONGE_AOD_ACTIVE_INFO	0x000A

#define ZT_SPONGE_UTC				0x0010
#define ZT_SPONGE_FOD_PROPERTY		0x0014
#define ZT_SPONGE_FOD_INFO			0x0015
#define ZT_SPONGE_FOD_POSITION		0x0019
#define ZT_SPONGE_FOD_RECT			0x004B

#define ZT_SPONGE_DUMP_FORMAT		0x00F0
#define ZT_SPONGE_DUMP_CNT			0x00F1
#define ZT_SPONGE_DUMP_CUR_IDX		0x00F2
#define ZT_SPONGE_DUMP_START		0x00F4

#define ZT_SPONGE_READ_INFO			0xA401
#define ZT_SPONGE_READ_REG			0xA402
#define ZT_SPONGE_WRITE_REG			0xA403
#define ZT_SPONGE_SYNC_REG			0xA404

#define REG_CHANNEL_TEST_RESULT				0x0296
#define TEST_CHANNEL_OPEN				0x0D
#define TEST_PATTERN_OPEN				0x04
#define TEST_SHORT					0x08
#define TEST_PASS					0xFF


/* ESD Protection */
/*second : if 0, no use. if you have to use, 3 is recommended*/
#define ESD_TIMER_INTERVAL			2
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
#define TOUCH_NO_OPERATION_MODE     55

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

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
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
#define VCMD_OSC_FREQ_SEL				0x1AF0

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
#define VCMD_OSC_FREQ_SEL				0xC201

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
#define ZT_DRIVE_INT_STATUS_CMD  		0x000C
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

#define ZT_JITTER_SAMPLING_CNT			0x001F
#define ZT_JITTER_TEST_STEP				0x0146		// 0(MAX), 1(MIN), 2(AVG)
#define ZT_JITTER_RESULT_MAX			0x01A3
#define ZT_JITTER_RESULT_MIN			0x0147

#define REG_SNR_TEST_N_DATA_CNT			0x001F		// 1 ~ 1000
#define	REG_SNR_TEST_START_CMD			0x0296		// 0(Stop), 1(Non-Touch Start), 2~10(Touch Start)
#define	REG_SNR_TEST_RESULT_STATUS		0x0297
#define	REG_SNR_TEST_RESULT_POINT		0x0298
#define	REG_SNR_TEST_RESULT_AVERAGE		0x0299
#define	REG_SNR_TEST_RESULT				0x020A

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

#define ZT_SET_SCANRATE						0x01A0
#define ZT_SET_SCANRATE_ENABLE		0x01A1
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

#define	ZT_REG_DEBUG19					0x00EB


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


#define TEST_OCTA_MODULE	1
#define TEST_OCTA_ASSAY		2

#define TEST_OCTA_NONE		0
#define TEST_OCTA_FAIL		1
#define TEST_OCTA_PASS		2

#define TSP_NORMAL_EVENT_MSG	1


#define TC_SECTOR_SZ		8
#define TC_NVM_SECTOR_SZ	64
#ifdef TCLM_CONCEPT
#define TC_SECTOR_SZ_WRITE		8
#define TC_SECTOR_SZ_READ		8
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
#define TSP_PAGE_SIZE	128
#define FUZING_UDELAY 15000
#elif IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
#define TSP_PAGE_SIZE	1024
#define FUZING_UDELAY 28000
#elif defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7554)
#define TSP_PAGE_SIZE		128
#define FUZING_UDELAY	8000
#elif defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7548) || defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7538)
#define TSP_PAGE_SIZE		64
#define FUZING_UDELAY	8000
#elif defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT7532)
#define TSP_PAGE_SIZE		64
#define FUZING_UDELAY	30000
#else
#define TSP_PAGE_SIZE	1024
#define FUZING_UDELAY 28000
#endif

#define USB_POR_OFF_DELAY	1500
#define USB_POR_ON_DELAY	1500

#define ZT76XX_UPGRADE_INFO_SIZE 1024

#define TOUCH_V_FLIP	0x01
#define TOUCH_H_FLIP	0x02
#define TOUCH_XY_SWAP	0x04


struct zt_ts_platform_data {
	u32 gpio_ldo_en;
	bool mis_cal_check;
	void (*register_cb)(void *);

	struct regulator *dvdd;
	struct regulator *avdd;
	const char *chip_name;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	int ss_touch_num;
#endif
};

union coord_byte00_t {
	u8 value_u8bit;
	struct {
		u8 eid : 2;
		u8 tid : 4;
		u8 touch_status : 2;
	} value;
};

union coord_byte01_t {
	u8 value_u8bit;
	struct {
		u8 x_coord_h : 8;
	} value;
};

union coord_byte02_t {
	u8 value_u8bit;
	struct {
		u8 y_coord_h : 8;
	} value;
};

union coord_byte03_t {
	u8 value_u8bit;
	struct {
		u8 y_coord_l : 4;
		u8 x_coord_l : 4;
	} value;
};

union coord_byte04_t {
	u8 value_u8bit;
	struct {
		u8 major : 8;
	} value;
};

union coord_byte05_t {
	u8 value_u8bit;
	struct {
		u8 minor : 8;
	} value;
};

union coord_byte06_t {
	u8 value_u8bit;
	struct {
		u8 z_value : 6;
		u8 touch_type23 : 2;
	} value;
};

union coord_byte07_t {
	u8 value_u8bit;
	struct {
		u8 left_event : 4;
		u8 max_energy : 2;
		u8 touch_type01 : 2;
	} value;
};

union coord_byte08_t {
	u8 value_u8bit;
	struct {
		u8 noise_level : 8;
	} value;
};

union coord_byte09_t {
	u8 value_u8bit;
	struct {
		u8 max_sensitivity : 8;
	} value;
};

union coord_byte10_t {
	u8 value_u8bit;
	struct {
		u8 hover_id_num  : 4;
		u8 location_area : 4;
	} value;
};

struct point_info {
	union coord_byte00_t byte00;
	union coord_byte01_t byte01;
	union coord_byte02_t byte02;
	union coord_byte03_t byte03;
	union coord_byte04_t byte04;
	union coord_byte05_t byte05;
	union coord_byte06_t byte06;
	union coord_byte07_t byte07;
	union coord_byte08_t byte08;
	union coord_byte09_t byte09;
	union coord_byte10_t byte10;
	u8 byte11;
	u8 byte12;
	u8 byte13;
	u8 byte14;
	u8 byte15;
};

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

enum {
	COORDINATE_EVENT = 0,
	STATUS_EVENT,
	GESTURE_EVENT,
	CUSTOM_EVENT,
} EVENT_ID;

enum {
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
} TOUCH_TYPE;

enum {
	SWIPE_UP = 0,
	DOUBLE_TAP,
	PRESSURE,
	FINGERPRINT,
	SINGLE_TAP,
	GESTURE_NONE = 255,
} GESTURE_TYPE;

typedef union _nvm_binary_info {
	u32 buff32[32]; //128byte
	struct _val {
		u32 RESERVED0;
		u32 auto_boot_flag1;
		u32 auto_boot_flag2;
		u32 slave_addr_flag;
		u32 RESERVED1[4];
		u32 info_checksum;
		u32 core_checksum;
		u32 custom_checksum;
		u32 register_checksum;
		u16 hw_id;
		u16 RESERVED2;
		u16 major_ver;
		u16 RESERVED3;
		u16 minor_ver;
		u16 RESERVED4;
		u16 release_ver;
		u16 RESERVED5;
		u16 chip_id;
		u16 chip_id_reverse;
		u16 chip_naming_number;
		u16 RESERVED6[9];
		u8  info_size[4];
		u8  core_size[4];
		u8  custom_size[4];
		u8  register_size[4];
		u8  total_size[4];
		u32 RESERVED7[5];
	} val;
} nvm_binary_info;

struct raw_ioctl {
	u32 sz;
	u32 buf;
};

struct reg_ioctl {
	u32 addr;
	u32 val;
};

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
		} __attribute__ ((packed));
		unsigned char data[1];
	};
};

static int m_ts_debug_mode = ZINITIX_DEBUG;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *tsp_cb, bool mode);
};

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#define SECURE_TOUCH_ENABLED	1
#define SECURE_TOUCH_DISABLED	0
static bool old_ta_status;
#endif

static bool g_ta_connected;

typedef union {
	u16 optional_mode;
	struct select_mode {
		u16 flag;
	} select_mode;
} zt_setting;

#if ESD_TIMER_INTERVAL
static struct workqueue_struct *esd_tmr_workqueue;
#endif

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
	struct zt_ts_platform_data *pdata;
	struct sec_ts_plat_data *plat_data;

	char phys[32];
	struct capa_info cap_info;
	struct point_info touch_info[MAX_SUPPORTED_FINGER_NUM];
	struct point_info touch_fod_info;
	u8 fod_touch_vi_data[SEC_CMD_STR_LEN];
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
	struct sec_cmd_data sec;

	struct delayed_work work_read_info;
	bool info_work_done;

	struct delayed_work ghost_check;
	u8 tsp_dump_lock;

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

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
#endif
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
	u16 fod_info_vi_data_len;
	u16 fod_rect[4];

	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

	u8 touched_finger_num;
	struct delayed_work work_print_info;
	u16 pressed_x[MAX_SUPPORTED_FINGER_NUM];
	u16 pressed_y[MAX_SUPPORTED_FINGER_NUM];

	u16 hover_event; /* keystring for protos */
	u16 store_reg_data;

	int noise_flag;
	int flip_cover_flag;

	int tsp_page_size;
	int fuzing_udelay;
	struct sec_tclm_data *tdata;

	int debug_flag;
	int fix_active_mode;

	u32 ic_info_size;
	u32 ic_core_size;
	u32 ic_cust_size;
	u32 ic_regi_size;
	u32 fw_info_size;
	u32 fw_core_size;
	u32 fw_cust_size;
	u32 fw_regi_size;
	u8 kind_of_download_method;
};

struct zt_ts_snr_result_cmd {
	s16	status;
	s16	point;
	s16	average;
};

struct tsp_snr_result_of_point {
	s16	max;
	s16	min;
	s16	average;
	s16	nontouch_peak_noise;
	s16	touch_peak_noise;
	s16	snr1;
	s16	snr2;
};

struct zt_ts_snr_result {
	s16	status;
	s16	reserved[6];
	struct tsp_snr_result_of_point result[9];
};

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
struct zt_ts_info *tui_tsp_info;
extern int tui_force_close(uint32_t arg);
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern void stui_tsp_init(int (*stui_tsp_enter)(void), int (*stui_tsp_exit)(void), int (*stui_tsp_type)(void));
#endif

#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
extern int get_lcd_attached(char *mode);
#endif
#if IS_ENABLED(CONFIG_EXYNOS_DPU30)
int get_lcd_info(char *arg);
#endif
void zt_print_info(struct zt_ts_info *info);
bool shutdown_is_on_going_tsp;
static int ts_set_touchmode(struct zt_ts_info *info, u16 value);

#if ESD_TIMER_INTERVAL
static void esd_timer_stop(struct zt_ts_info *info);
static void esd_timer_start(u16 sec, struct zt_ts_info *info);
#endif

void tsp_charger_infom(bool en);

static bool zt_power_control(struct zt_ts_info *info, bool ctl);

static bool init_touch(struct zt_ts_info *info);
static bool mini_init_touch(struct zt_ts_info *info);
static void clear_report_data(struct zt_ts_info *info);

#if ESD_TIMER_INTERVAL
static void esd_timer_init(struct zt_ts_info *info);
static void esd_timeout_handler(struct timer_list *t);
#endif

static void zt_display_rawdata(struct zt_ts_info *info, struct tsp_raw_data *raw_data, int type, int gap);
void zt_set_grip_type(struct zt_ts_info *info, u8 set_type);

#ifdef TCLM_CONCEPT
int get_zt_tsp_nvm_data(struct zt_ts_info *info, u8 addr, u8 *values, u16 length);
int set_zt_tsp_nvm_data(struct zt_ts_info *info, u8 addr, u8 *values, u16 length);
int zt_tclm_data_read(struct device *dev, int address);
#endif

void location_detect(struct zt_ts_info *info, char *loc, int x, int y);

#endif /* LINUX_ZT_TS_H */
