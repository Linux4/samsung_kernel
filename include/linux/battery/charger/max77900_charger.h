/*
 * max77900_charger.h
 * Samsung max77900 Charger Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
 * Yeongmi Ha <yeongmi86.ha@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MAX77900_CHARGER_H
#define __MAX77900_CHARGER_H __FILE__

#include <linux/mfd/core.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/battery/sec_charging_common.h>

/* REGISTER MAPS */

/* WPC HEADER */
#define MAX77900_EPT_HEADER_EPT					0x02
#define MAX77900_EPT_HEADER_CS100					0x05

/* END POWER TRANSFER CODES IN WPC */
#define MAX77900_EPT_CODE_UNKOWN					0x00
#define MAX77900_EPT_CODE_CHARGE_COMPLETE			0x01
#define MAX77900_EPT_CODE_INTERNAL_FAULT			0x02
#define MAX77900_EPT_CODE_OVER_TEMPERATURE		0x03
#define MAX77900_EPT_CODE_OVER_VOLTAGE			0x04
#define MAX77900_EPT_CODE_OVER_CURRENT			0x05
#define MAX77900_EPT_CODE_BATTERY_FAILURE			0x06
#define MAX77900_EPT_CODE_RECONFIGURE				0x07
#define MAX77900_EPT_CODE_NO_RESPONSE				0x08

#define MAX77900_POWER_MODE_MASK					(0x1 << 0)
#define MAX77900_SEND_USER_PKT_DONE_MASK			(0x1 << 7)
#define MAX77900_SEND_USER_PKT_ERR_MASK			(0x3 << 5)
#define MAX77900_SEND_ALIGN_MASK					(0x1 << 3)
#define MAX77900_SEND_EPT_CC_MASK					(0x1 << 0)
#define MAX77900_SEND_EOC_MASK					(0x1 << 0)

#define MAX77900_PTK_ERR_NO_ERR					0x00
#define MAX77900_PTK_ERR_ERR						0x01
#define MAX77900_PTK_ERR_ILLEGAL_HD				0x02
#define MAX77900_PTK_ERR_NO_DEF					0x03

/* Four operating modes */
 enum {
	OP_MODE_P2P_TX,
	OP_MODE_RX_WPC,
	OP_MODE_RX_PMA,

	OP_MODE_UNKNOWN
};

/* Characteristics, Default configruation */
#define VUVLO			(3000)		/*mV  Under voltage lockout threshold  */
#define UVLO_RETRY_CNT  (10)

#define FSWITCHING		(160000)	/* Hz */
#define DUTYCICLE		(48)		/* % */

#define TX_EN			(1)
#define TX_DIS			(0)

#define TX_FULL			(1)
#define TX_HALF			(0)

/* Definition of two dimensional array type */
typedef u8 FOD_T [16][2];
typedef u8 (*PFOD_T)[2];

/* VRECT_Target_x, VRECT_Target_y_wpc, VRECT_Target_y_pma */
typedef u8 VRECT_T[8][3];
typedef u8 (*PVRECT_T)[3];
/* Register map */

/* <CLOGIC> */

#define REG_SIDENSE_ENABLE	0x00

/* Chip ID */
#define REG_OTP_ID			0x01

/* Status register */
#define REG_STS0			0x02
#define REG_STS1			0x03

/* REG_STS0 Bits */
#define BIT_TX_EN_S			BIT(6)
#define BIT_TJ120_WARN_S	BIT(5)
#define BIT_TJ100_WARN_S	BIT(4)
#define BIT_TDIE_WARN_S		BIT(3)
#define BIT_TS_S			BIT(2)	
#define BIT_OUTPUT_EN_S		BIT(1)
#define BIT_PAD_DETECT_S	BIT(0)

/* REG_STS1 Bits */
#define BIT_PMA_RX_MODE		BIT(1)
#define BIT_WPC_RX_MODE		BIT(0)

/* Interrupt register & mask bit */
#define REG_INT0			0x04
#define REG_INT1			0x05

#define REG_INTMASK0		0x06
#define REG_INTMASK1		0x07

/* REG_INT0 Bits*/

#define BIT_THERM_INT		BIT(6)

/* usually should not occur, but if something wrong happens on the charger's side.
   The LDO with the ILIM feature is enough to protect itself. */
#define BIT_TX_ILIM_INT	BIT(5)		// Do nothing

#define BIT_LDO_ILIM_INT	BIT(4)      // Increase the frequency on the TX side

/* goes high when an ADC conversion for the coil alignment function is been completed */
#define BIT_ALIGN_CONV_INT	BIT(3)		// Do nothing

/* goes high when the HVLDO is enabled */
#define BIT_OUTPUT_EN_INT	BIT(2)		// TX

#define BIT_PAD_DETECT_INT	BIT(1)		// RX

/* REG_INT1 Bits*/
#define BIT_TJ120_WARN_INT		BIT(3)
#define BIT_TJ100_WARN_INT		BIT(2)
#define BIT_TDIE_WARN_INT		BIT(1)
#define BIT_TS_INT				BIT(0)

/* REG_INTMASK0 */
#define BIT_TX_ILIM_M		BIT(5)
#define BIT_LDO_ILIM_M		BIT(4)
#define BIT_ALIGN_CONV_M	BIT(3)
#define BIT_OUTPUT_EN_M		BIT(2)
#define BIT_PAD_DETECT_M	BIT(1)
#define BIT_IRQ_M			BIT(0)

/* REG_INTMASK1 */
#define BIT_TJ120_WARN_M	BIT(3)
#define BIT_TJ100_WARN_M	BIT(2)
#define BIT_TDIE_WARN_M		BIT(1)
#define BIT_TS_M			BIT(0)

/* RECT, P2PEN */

#define RECT_ADDR_BASE      0x08
#define RECT_ADDR(_offset)  (RECT_ADDR_BASE + (_offset))

#define RECT_CNFG	RECT_ADDR(0)
#define TX_FREQ	RECT_ADDR(1)//Peer-to-peer inverters output frequency setting 
#define TX_DUTY	RECT_ADDR(2)//Peer-to-peer inverters output duty cycle
#define TX_CNFG	RECT_ADDR(3)//Peer-to-peer configuration register

/* RECT_CNFG Bits */
/*
	BIT_FS  Rectifier FET scaling FS[2:1], default 0x3

	0x3: 4/4
	0x2: 3/4
	0x1: 2/4
	0x0: 1/4

	BIT_SLEN TX Tx slew rate enable , default 0x0

*/
#define BIT_TX_COMM_OFF	BIT(3)
#define BIT_FS				BITS(2,1)
#define BIT_SLEN			BIT(0)

#define BIT_TX_COMM_OFF_M	0x08
#define BIT_FS_M			0x06
#define BIT_SLEN_M			0x01


/* TX_CNFG Bits */
#define BIT_TX_WDC			BITS(7,5)
#define BIT_TX_TWD			BITS(4,2)
#define BIT_TX_FULL			BIT(1)
#define BIT_TX_INVERTER_EN	BIT(0)

#define BIT_TX_WDC_M			xE0
#define BIT_TX_TWD_M			0x1C
#define BIT_TX_FULL_M			0x02
#define BIT_TX_INVERTER_EN_M	0x01

/* <LDO> */
/*

LDO_ILIMSET Setup LDO output current limit from 0 mA to 1.26A , 20 mA/LSB, default 0x00

	0x00 = 0 mA
	0x01 = 20 mA
	...
	0x3C = 1200 mA
	...
	0x3f = 1260 mA

LDO_VOUTSET Setup LDO output voltage from 3.65V to 10.0V,  50 mV/LSB, default 0x00

	0x00 = 3.65V
	0x01 = 3.70V
	
	0x1b = 5.00V
	
	0x7f = 10.0V


LDO_SLOPEILIM_EN LDO step time for current limit step of 50mA
	111 - 40ms
	110 - 30ms
	101 - 20ms
	100 - 10ms
	011 - 5.0ms
	010 - 1.0ms
	001 - 0.5ms  default
	000 - 0.2ms

*/

#define LDO_ADDR_BASE           0x0C
#define LDO_ADDR(_offset)      (LDO_ADDR_BASE + (_offset))

#define LDO_ILIMSET_DEF			LDO_ADDR(0x00)
#define LDO_VOUTSET_DEF			LDO_ADDR(0x01)
#define LDO_ILIMSET				LDO_ADDR(0x02)
#define LDO_VOUTSET				LDO_ADDR(0x03)
#define LDO_ILIMSET_TX			LDO_ADDR(0x04)
#define LDO_SLOPEILIM_EN		LDO_ADDR(0x05)
#define LDO_VRECTH				LDO_ADDR(0x06)  // Rectifier voltage sense bit [11:4]
#define LDO_VRECTL				LDO_ADDR(0x07)  // Rectifier voltage sense bit [3:0]
#define LDO_VOUTH				LDO_ADDR(0x08)  // LDO output voltage sense bit [11:4]
#define LDO_VOUTL				LDO_ADDR(0x09)	// LDO output voltage sense bit [3:0]
#define LDO_ISENSEH				LDO_ADDR(0x0A)	// LDO output current sense bit [11:4]
#define LDO_ISENSEL				LDO_ADDR(0x0B)	// LDO output current sense bit [3:0]
#define LDO_AUX_LDO_ENABLE		LDO_ADDR(0x0C)

/* <LDO Bits> */

#define BIT_VOUTSET_OVERRIDE		BIT(7)
#define BIT_VOUTSET					BITS(6,0)
#define BIT_VOUTSET_M				0x3F

#define BIT_ILIMSET_OVERRIDE		BIT(7)
#define BIT_ILIMSET					BITS(5,0)
#define BIT_ILIMSET_M				0x1F

#define BIT_ILIMSET_TX				BITS(4,0)
#define BIT_ILIMSET_TX_M			0x1F
#define AUX_LDO_ENABLE				0x18
/* LDO_AUX_LDO_ENABLE Bits */

#define BIT_V3P3_DISABLE			BIT(1)
#define BIT_V1P8_DISABLE			BIT(0)
#define BIT_V3P3_DISABLE_M			0x02
#define BIT_V1P8_DISABLE_M			0x01

#define TS_THRESHOLD			FSM_ADDR(0x00)
#define TS_VALUE				FSM_ADDR(0x01)
#define TDIE_WARN_THRESHOLD		FSM_ADDR(0x02)
#define TDIE_VALUE				FSM_ADDR(0x03)	

/* <FOD> ( Foreign object detection) function */
#define FOD_ADDR_BASE           0x44
#define FOD_ADDR(_offset)      (FOD_ADDR_BASE + (_offset))

#define FOD_X0		FOD_ADDR(0x00)
#define FOD_Y0		FOD_ADDR(0x01)
#define FOD_X1		FOD_ADDR(0x02)
#define FOD_Y1		FOD_ADDR(0x03)
#define FOD_X2		FOD_ADDR(0x04)
#define FOD_Y2		FOD_ADDR(0x05)
#define FOD_X3		FOD_ADDR(0x06)
#define FOD_Y3		FOD_ADDR(0x07)
#define FOD_X4		FOD_ADDR(0x08)
#define FOD_Y4		FOD_ADDR(0x09)
#define FOD_X5		FOD_ADDR(0x0A)
#define FOD_Y5		FOD_ADDR(0x0B)
#define FOD_X6		FOD_ADDR(0x0C)
#define FOD_Y6		FOD_ADDR(0x0D)
#define FOD_X7		FOD_ADDR(0x0E)
#define FOD_Y7		FOD_ADDR(0x0F)
#define FOD_X8 		FOD_ADDR(0x10)
#define FOD_Y8		FOD_ADDR(0x11)
#define FOD_X9		FOD_ADDR(0x12)
#define FOD_Y9		FOD_ADDR(0x13)
#define FOD_X10		FOD_ADDR(0x14)
#define FOD_Y10		FOD_ADDR(0x15)
#define FOD_X11		FOD_ADDR(0x16)
#define FOD_Y11		FOD_ADDR(0x17)
#define FOD_X12		FOD_ADDR(0x18)
#define FOD_Y12		FOD_ADDR(0x19)
#define FOD_X13		FOD_ADDR(0x1A)
#define FOD_Y13		FOD_ADDR(0x1B)
#define FOD_X14		FOD_ADDR(0x1C)
#define FOD_Y14		FOD_ADDR(0x1D)
#define FOD_X15		FOD_ADDR(0x1E)
#define FOD_Y15		FOD_ADDR(0x1F)

#define VRECT_TARGET_ADDR_BASE           0x64
#define VRECT_TARGET_ADDR(_offset)      (VRECT_TARGET_ADDR_BASE + (_offset))

#define VRECT_TARGET_X0			VRECT_TARGET_ADDR(0x00)		
#define VRECT_TARGET_Y0_WPC		VRECT_TARGET_ADDR(0x01)	
#define VRECT_TARGET_Y0_PMA		VRECT_TARGET_ADDR(0x02)
#define VRECT_TARGET_X1			VRECT_TARGET_ADDR(0x03)
#define VRECT_TARGET_Y1_WPC		VRECT_TARGET_ADDR(0x04)
#define VRECT_TARGET_Y1_PMA		VRECT_TARGET_ADDR(0x05)
#define VRECT_TARGET_X2			VRECT_TARGET_ADDR(0x06)
#define VRECT_TARGET_Y2_WPC		VRECT_TARGET_ADDR(0x07)
#define VRECT_TARGET_Y2_PMA		VRECT_TARGET_ADDR(0x08)
#define VRECT_TARGET_X3			VRECT_TARGET_ADDR(0x09)
#define VRECT_TARGET_Y3_WPC		VRECT_TARGET_ADDR(0x0A)
#define VRECT_TARGET_Y3_PMA		VRECT_TARGET_ADDR(0x0B)
#define VRECT_TARGET_X4			VRECT_TARGET_ADDR(0x0C)
#define VRECT_TARGET_Y4_WPC		VRECT_TARGET_ADDR(0x0D)
#define VRECT_TARGET_Y4_PMA		VRECT_TARGET_ADDR(0x0E)
#define VRECT_TARGET_X5			VRECT_TARGET_ADDR(0x0F)
#define VRECT_TARGET_Y5_WPC		VRECT_TARGET_ADDR(0x10)
#define VRECT_TARGET_Y5_PMA		VRECT_TARGET_ADDR(0x11)
#define VRECT_TARGET_X6			VRECT_TARGET_ADDR(0x12)
#define VRECT_TARGET_Y6_WPC		VRECT_TARGET_ADDR(0x13)
#define VRECT_TARGET_Y6_PMA		VRECT_TARGET_ADDR(0x14)
#define VRECT_TARGET_Y7_WPC		VRECT_TARGET_ADDR(0x15)
#define VRECT_TARGET_Y7			VRECT_TARGET_ADDR(0x16)
#define VRECT_TARGET_Y7_PMA		VRECT_TARGET_ADDR(0x17)
	
/* <Align> */
#define ALIGN_ADDR_BASE           0x80
#define ALIGN_ADDR(_offset)      (ALIGN_ADDR_BASE + (_offset))
#define ALIGN_CNFG0		ALIGN_ADDR(0x00)
#define ALIGN_CNFG1		ALIGN_ADDR(0x01)
#define ALIGN_NH		ALIGN_ADDR(0x02)
#define ALIGN_NL		ALIGN_ADDR(0x03)
#define ALIGN_SH		ALIGN_ADDR(0x04)
#define ALIGN_SL		ALIGN_ADDR(0x05)
#define ALIGN_EH		ALIGN_ADDR(0x06)
#define ALIGN_EL		ALIGN_ADDR(0x07)
#define ALIGN_WH		ALIGN_ADDR(0x08)
#define ALIGN_WL		ALIGN_ADDR(0x09)

/* ALIGN_CNFG0 Bits */

#define BIT_GAIN		BITS(7,4)
#define BIT_FT			BITS(2,1)
#define BIT_ALIGN_EN	BIT(0)
#define BIT_GAIN_M		0xF0
#define BIT_FT_M		0x06
#define BIT_ALIGN_EN_M	0x01


/* <FSM> */
#define FSM_ADDR_BASE           0x20
#define FSM_ADDR(_offset)      (FSM_ADDR_BASE + (_offset))
#define WPC_CFG_POWER			FSM_ADDR(0x00)
#define WPC_CFG_RSVD0			FSM_ADDR(0x01)
#define WPC_CFG_PROP_COUNT		FSM_ADDR(0x02)
#define WPC_CFG_WINDOW			FSM_ADDR(0x03)
#define WPC_CFG_RSVD1			FSM_ADDR(0x04)
#define WPC_ID_VERSION			FSM_ADDR(0x05)
#define WPC_ID_MFR0				FSM_ADDR(0x06)
#define WPC_ID_MFR1				FSM_ADDR(0x07)
#define PMA_RXID_OUI0			FSM_ADDR(0x08)
#define PMA_RXID_OUI1			FSM_ADDR(0x09)
#define PMA_RXID_OUI2			FSM_ADDR(0x0A)
#define RXID0					FSM_ADDR(0x0B)
#define RXID1					FSM_ADDR(0x0C)
#define RXID2					FSM_ADDR(0x0D)
#define RXID3					FSM_ADDR(0x0E)

/* Communications Interface - Logical Layer */

/* ping phase */
#define WPC_PACKET_SSP			FSM_ADDR(0x0F)	// 0x01		Signal Strenth Patcket
#define WPC_PACKET_EPT			FSM_ADDR(0x10)  // 0x02		End Power Transfer Packet

/* power transfer phase */
#define WPC_PACKET_CEP			FSM_ADDR(0x11)	// 0x03		Control Error Packet
#define WPC_PACKET_RPP			FSM_ADDR(0x12)	// 0x04		Received Power Packet

//#define TS_THRESHOLD			FSM_ADDR(0x13)
//#define TS_VALUE				FSM_ADDR(0x14)
//#define TDIE_WARN_THRESHOLD	FSM_ADDR(0x15)
//#define TDIE_VALUE				FSM_ADDR(0x16)

#define PMA_EOC_IMAX			FSM_ADDR(0x17) // PMA mode end of charge output current threshold
#define FSM_ADC_N_SAMPLES		FSM_ADDR(0x18)
#define FSM_MODE_OVERRIDE		FSM_ADDR(0x19)
#define FSM_ADC_RESULTH			FSM_ADDR(0x1A)
#define FSM_ADC_RESULTL			FSM_ADDR(0x1B)

/* WPC_CFG_POWER Bits */
#define BIT_POWER_CLASS			BITS(7,6)
#define BIT_MAXIMUM_POWER		BITS(5,0)
#define BIT_POWER_CLASS_M		0xC0
#define BIT_MAXIMUM_POWER_M		0x3F

/* WPC_CFG_PROP_COUNT Bits */
#define BIT_PROP				BIT(7)
#define BIT_COUNT				BITS(2,0)
#define BIT_PROP_M				0x80
#define BIT_COUNT_M				0x07

/* WPC_CFG_WINDOW Bits */
#define BIT_WINDOW_SIZE			BITS(7,3)
#define BIT_WINDOW_OFFSET		BITS(2,0)
#define BIT_WINDOW_SIZE_M		0xF8
#define BIT_WINDOW_OFFSET_M		0x07

/* WPC_ID_VERSION Bits */
#define BIT_MAJOR_VERSION		BITS(7,4)
#define BIT_MINOR_VERSION		BITS(3,0)
#define BIT_MAJOR_VERSION_M		0xF0
#define BIT_MINOR_VERSION_M		0x0F

/* EPT(End Power Transfer) values  WPC 1.1*/
enum {
	/* Reason  Value */
	UNKNOWN			= 0X00,
	CHARGE_COMPLETE,
	INTERNAL_FAULT, 
	OVER_TEMPERATURE,
	OVER_VOLTAGE,
	OVER_CURRENT, 
	BATTERY_FAILURE,
	RECONFIGURE,
	NO_RESPONSE,
	RESERVED  //0X090XFF
};

/* Chip Interrupts */
enum {
	MAX77900_INT0_IRQ_START,
	MAX77900_PAD_DETECT = MAX77900_INT0_IRQ_START,
	MAX77900_OUTPUT_EN,
	MAX77900_ALIGN_CONV,
	MAX77900_LDO_ILIM,
	MAX77900_TX_ILIM,
	MAX77900_THERM,

	MAX77900_INT1_IRQ_START,
	MAX77900_TS = MAX77900_INT1_IRQ_START,
	MAX77900_TDIE_WARN,
	MAX77900_TJ100_WARN,
	MAX77900_TJ120_WARN,
	MAX77900_NUM_OF_INTS,
};
struct max77900_charger_platform_data {
	int irq_gpio;
	int irq_base;
	int tsb_gpio;
	int cs100_status;
	int pad_mode;
	int wireless_cc_cv;
	int siop_level;
	bool default_voreg;
	char *wireless_charger_name;
	int wpc_int;
	int wpc_det;
	int irq_wpc_int;
	int irq_wpc_det;
};

#define max77900_charger_platform_data_t \
	struct max77900_charger_platform_data

struct max77900_charger_data {
	struct i2c_client				*client;
	struct device           *dev;
	max77900_charger_platform_data_t *pdata;
	struct mutex io_lock;

	struct power_supply psy_chg;
	struct wake_lock wpc_wake_lock;
	struct workqueue_struct *wqueue;
	struct work_struct	wcin_work;
	struct delayed_work	wpc_work;
	struct delayed_work	isr_work;

	int wc_w_state;
};

enum {
    MAX77900_EVENT_IRQ = 0,
    MAX77900_IRQS_NR,
};

enum {
    MAX77900_PAD_MODE_NONE = 0,
    MAX77900_PAD_MODE_WPC,
    MAX77900_PAD_MODE_PMA,
};

#endif /* __P9027S_CHARGER_H */