/*
 * Platform data for the NXP PCA9468 battery charger driver.
 *
 * Copyright 2018-2023 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PCA9468_CHARGER_H_
#define _PCA9468_CHARGER_H_

#define BITS(_end, _start) ((BIT(_end) - BIT(_start)) + BIT(_end))
#define MASK2SHIFT(_mask)	__ffs(_mask)
#define MIN(a, b)	((a < b) ? (a):(b))
#define MAX(a, b)	((a > b) ? (a):(b))

//
// Register Map
//
#define PCA9468_REG_DEVICE_INFO 		0x00	// Device ID, revision
#define PCA9468_BIT_DEV_REV				BITS(7,4)
#define PCA9468_BIT_DEV_ID				BITS(3,0)
#define PCA9468_DEVICE_ID				0x18	// Default ID

#define PCA9468_REG_INT1				0x01	// Interrupt register
#define PCA9468_BIT_V_OK_INT			BIT(7)
#define PCA9468_BIT_NTC_TEMP_INT		BIT(6)
#define PCA9468_BIT_CHG_PHASE_INT		BIT(5)
#define PCA9468_BIT_CTRL_LIMIT_INT		BIT(3)
#define PCA9468_BIT_TEMP_REG_INT		BIT(2)
#define PCA9468_BIT_ADC_DONE_INT		BIT(1)
#define PCA9468_BIT_TIMER_INT			BIT(0)

#define PCA9468_REG_INT1_MSK			0x02	// INT mask for INT1 register
#define PCA9468_BIT_V_OK_M				BIT(7)
#define PCA9468_BIT_NTC_TEMP_M			BIT(6)
#define PCA9468_BIT_CHG_PHASE_M			BIT(5)
#define PCA9468_BIT_RESERVED_M			BIT(4)
#define PCA9468_BIT_CTRL_LIMIT_M		BIT(3)
#define PCA9468_BIT_TEMP_REG_M			BIT(2)
#define PCA9468_BIT_ADC_DONE_M			BIT(1)
#define PCA9468_BIT_TIMER_M				BIT(0)

#define PCA9468_REG_INT1_STS			0x03	// INT1 status register
#define PCA9468_BIT_V_OK_STS			BIT(7)
#define PCA9468_BIT_NTC_TEMP_STS		BIT(6)
#define PCA9468_BIT_CHG_PHASE_STS		BIT(5)
#define PCA9468_BIT_CTRL_LIMIT_STS		BIT(3)
#define PCA9468_BIT_TEMP_REG_STS		BIT(2)
#define PCA9468_BIT_ADC_DONE_STS		BIT(1)
#define PCA9468_BIT_TIMER_STS			BIT(0)

#define PCA9468_REG_STS_A				0x04	// INT1 status register
#define PCA9468_BIT_IIN_LOOP_STS		BIT(7)
#define PCA9468_BIT_CHG_LOOP_STS		BIT(6)
#define PCA9468_BIT_VFLT_LOOP_STS		BIT(5)
#define PCA9468_BIT_CFLY_SHORT_STS		BIT(4)
#define PCA9468_BIT_VOUT_UV_STS			BIT(3)
#define PCA9468_BIT_VBAT_OV_STS			BIT(2)
#define PCA9468_BIT_VIN_OV_STS			BIT(1)
#define PCA9468_BIT_VIN_UV_STS			BIT(0)

#define PCA9468_REG_STS_B				0x05	// INT1 status register
#define PCA9468_BIT_BATT_MISS_STS		BIT(7)
#define PCA9468_BIT_OCP_FAST_STS		BIT(6)
#define PCA9468_BIT_OCP_AVG_STS			BIT(5)
#define PCA9468_BIT_ACTIVE_STATE_STS	BIT(4)
#define PCA9468_BIT_SHUTDOWN_STATE_STS	BIT(3)
#define PCA9468_BIT_STANDBY_STATE_STS	BIT(2)
#define PCA9468_BIT_CHARGE_TIMER_STS	BIT(1)
#define PCA9468_BIT_WATCHDOG_TIMER_STS	BIT(0)

#define PCA9468_REG_STS_C				0x06	// IIN status
#define PCA9468_BIT_IIN_STS				BITS(7,2)

#define PCA9468_REG_STS_D				0x07	// ICHG status
#define PCA9468_BIT_ICHG_STS			BITS(7,1)

#define PCA9468_REG_STS_ADC_1			0x08	// ADC register
#define PCA9468_BIT_ADC_IIN7_0			BITS(7,0)

#define PCA9468_REG_STS_ADC_2			0x09	// ADC register
#define PCA9468_BIT_ADC_IOUT5_0			BITS(7,2)
#define PCA9468_BIT_ADC_IIN9_8			BITS(1,0)

#define PCA9468_REG_STS_ADC_3			0x0A	// ADC register
#define PCA9468_BIT_ADC_VIN3_0			BITS(7,4)
#define PCA9468_BIT_ADC_IOUT9_6			BITS(3,0)

#define PCA9468_REG_STS_ADC_4			0x0B	// ADC register
#define PCA9468_BIT_ADC_VOUT1_0			BITS(7,6)
#define PCA9468_BIT_ADC_VIN9_4			BITS(5,0)

#define PCA9468_REG_STS_ADC_5			0x0C	// ADC register
#define PCA9468_BIT_ADC_VOUT9_2			BITS(7,0)

#define PCA9468_REG_STS_ADC_6			0x0D	// ADC register
#define PCA9468_BIT_ADC_VBAT7_0			BITS(7,0)

#define PCA9468_REG_STS_ADC_7			0x0E	// ADC register
#define PCA9468_BIT_ADC_DIETEMP5_0		BITS(7,2)
#define PCA9468_BIT_ADC_VBAT9_8			BITS(1,0)

#define PCA9468_REG_STS_ADC_8			0x0F	// ADC register
#define PCA9468_BIT_ADC_NTCV3_0			BITS(7,4)
#define PCA9468_BIT_ADC_DIETEMP9_6		BITS(3,0)

#define PCA9468_REG_STS_ADC_9			0x10	// ADC register
#define PCA9468_BIT_ADC_NTCV9_4			BITS(5,0)

#define PCA9468_REG_ICHG_CTRL			0x20	// Change current configuration
#define PCA9468_BIT_ICHG_SS				BIT(7)
#define PCA9468_BIT_ICHG_CFG			BITS(6,0)

#define PCA9468_REG_IIN_CTRL			0x21	// Input current configuration
#define PCA9468_BIT_LIMIT_INCREMENT_EN	BIT(7)
#define PCA9468_BIT_IIN_SS				BIT(6)
#define PCA9468_BIT_IIN_CFG				BITS(5,0)

#define PCA9468_REG_START_CTRL			0x22	// Device initialization configuration
#define PCA9468_BIT_SNSRES				BIT(7)
#define PCA9468_BIT_EN_CFG				BIT(6)
#define PCA9468_BIT_STANDBY_EN			BIT(5)
#define PCA9468_BIT_REV_IIN_DET			BIT(4)
#define PCA9468_BIT_FSW_CFG				BITS(3,0)

#define PCA9468_REG_ADC_CTRL			0x23	// ADC configuration
#define PCA9468_BIT_FORCE_ADC_MODE		BITS(7,6)
#define PCA9468_BIT_ADC_SHUTDOWN_CFG	BIT(5)
#define PCA9468_BIT_HIBERNATE_DELAY		BITS(4,3)

#define PCA9468_REG_ADC_CFG				0x24	// ADC channel configuration
#define PCA9468_BIT_CH7_EN				BIT(7)
#define PCA9468_BIT_CH6_EN				BIT(6)
#define PCA9468_BIT_CH5_EN				BIT(5)
#define PCA9468_BIT_CH4_EN				BIT(4)
#define PCA9468_BIT_CH3_EN				BIT(3)
#define PCA9468_BIT_CH2_EN				BIT(2)
#define PCA9468_BIT_CH1_EN				BIT(1)

#define PCA9468_REG_TEMP_CTRL			0x25	// Temperature configuration
#define PCA9468_BIT_TEMP_REG			BITS(7,6)
#define PCA9468_BIT_TEMP_DELTA			BITS(5,4)
#define PCA9468_BIT_TEMP_REG_EN			BIT(3)
#define PCA9468_BIT_NTC_PROTECTION_EN	BIT(2)
#define PCA9468_BIT_TEMP_MAX_EN			BIT(1)

#define PCA9468_REG_PWR_COLLAPSE		0x26	// Power collapse configuration
#define PCA9468_BIT_UV_DELTA			BITS(7,6)
#define PCA9468_BIT_IIN_FORCE_COUNT		BIT(4)
#define PCA9468_BIT_BAT_MISS_DET_EN		BIT(3)

#define PCA9468_REG_V_FLOAT				0x27	// Voltage regulation configuration
#define PCA9468_BIT_V_FLOAT				BITS(7,0)

#define PCA9468_REG_SAFETY_CTRL			0x28	// Safety configuration
#define PCA9468_BIT_WATCHDOG_EN			BIT(7)
#define PCA9468_BIT_WATCHDOG_CFG		BITS(6,5)
#define PCA9468_BIT_CHG_TIMER_EN		BIT(4)
#define PCA9468_BIT_CHG_TIMER_CFG		BITS(3,2)
#define PCA9468_BIT_OV_DELTA			BITS(1,0)

#define PCA9468_REG_NTC_TH_1			0x29	// Thermistor threshold configuration
#define PCA9468_BIT_NTC_THRESHOLD7_0	BITS(7,0)

#define PCA9468_REG_NTC_TH_2			0x2A	// Thermistor threshold configuration
#define PCA9468_BIT_NTC_THRESHOLD9_8	BITS(1,0)

#define PCA9468_REG_ADC_ACCESS			0x30

#define PCA9468_REG_ADC_ADJUST			0x31
#define PCA9468_BIT_ADC_GAIN			BITS(7,4)
#define PCA9468_BIT_OTP_VERSION			BITS(3,0)

#define PCA9468_REG_ADC_IMPROVE			0x3D
#define PCA9468_BIT_ADC_IIN_IMP			BIT(3)

#define PCA9468_REG_ADC_MODE			0x3F
#define PCA9468_BIT_ADC_MODE			BIT(4)

#define PCA9468_MAX_REGISTER			0x4F

/* Regulation voltage and current */
#define PCA9468_IIN_CFG_STEP			100000	// input current step, unit - uA
#define PCA9468_VFLOAT_STEP				5000	// v float voltage step, unit - uV

#define PCA9468_IIN_CFG_MAX				5000000	// 5A
#define PCA9468_VFLOAT_MAX				5000000	// 5V

#define PCA9468_IIN_CFG_MIN				500000	// 500mA
#define PCA9468_VFLOAT_MIN				3725000	// 3725mV

#define PCA9468_IIN_CFG(_input_current)	(_input_current/PCA9468_IIN_CFG_STEP)	// input current, unit - uA
#define PCA9468_ICHG_CFG(_chg_current)	(_chg_current/100000)					// charging current, uint - uA
#define PCA9468_V_FLOAT(_v_float)		((_v_float - PCA9468_VFLOAT_MIN)/PCA9468_VFLOAT_STEP)	// v_float voltage, unit - uV

#define PCA9468_SNSRES_5mOhm			0x00
#define PCA9468_SNSRES_10mOhm			PCA9468_BIT_SNSRES

#define PCA9468_NTC_TH_STEP				2346	// 2.346mV, unit - uV

/* VIN Overvoltage setting from 2*VOUT */
enum {
	OV_DELTA_10P,
	OV_DELTA_30P,
	OV_DELTA_20P,
	OV_DELTA_40P,
};

enum {
	WDT_DISABLE,
	WDT_ENABLE,
};

enum {
	WDT_1SEC,
	WDT_2SEC,
	WDT_4SEC,
	WDT_8SEC,
};

enum {
	AUTO_MODE = 0,
	FORCE_SHUTDOWN_MODE,
	FORCE_HIBERNATE_MODE,
	FORCE_NORMAL_MODE,
};

/* Switching frequency */
enum {
	FSW_CFG_833KHZ,
	FSW_CFG_893KHZ,
	FSW_CFG_935KHZ,
	FSW_CFG_980KHZ,
	FSW_CFG_1020KHZ,
	FSW_CFG_1080KHZ,
	FSW_CFG_1120KHZ,
	FSW_CFG_1160KHZ,
	FSW_CFG_440KHZ,
	FSW_CFG_490KHZ,
	FSW_CFG_540KHZ,
	FSW_CFG_590KHZ,
	FSW_CFG_630KHZ,
	FSW_CFG_680KHZ,
	FSW_CFG_730KHZ,
	FSW_CFG_780KHZ
};

/* Enable pin polarity selection */
#define PCA9468_EN_ACTIVE_H		0x00
#define PCA9468_EN_ACTIVE_L		PCA9468_BIT_EN_CFG
#define PCA9468_STANDBY_FORCED	PCA9468_BIT_STANDBY_EN
#define PCA9468_STANDBY_DONOT	0

/* ADC Channel */
enum {
	ADCCH_VOUT = 1,
	ADCCH_VIN,
	ADCCH_VBAT,
	ADCCH_ICHG,
	ADCCH_IIN,
	ADCCH_DIETEMP,
	ADCCH_NTC,
	ADCCH_MAX
};

/* ADC step */
#define VIN_STEP		16000	// 16mV(16000uV) LSB, Range(0V ~ 16.368V)
#define VBAT_STEP		5000	// 5mV(5000uV) LSB, Range(0V ~ 5.115V)
#define IIN_STEP		4890 	// 4.89mA(4890uA) LSB, Range(0A ~ 5A)
#define ICHG_STEP		9780 	// 9.78mA(9780uA) LSB, Range(0A ~ 10A)
#define DIETEMP_STEP  	435		// 0.435C LSB, Range(-25C ~ 160C)
#define DIETEMP_DENOM	1000	// 1000, denominator
#define DIETEMP_MIN 	-25  	// -25C
#define DIETEMP_MAX		160		// 160C
#define VOUT_STEP		5000 	// 5mV(5000uV) LSB, Range(0V ~ 5.115V)
#define NTCV_STEP		2346 	// 2.346mV(2346uV) LSB, Range(0V ~ 2.4V)
#define ADC_IIN_OFFSET	900000	// 900mA

/* adc_gain bit[7:4] of reg 0x31 - 2's complement */
static int adc_gain[16] = { 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1 };

/* Timer definition */
#if defined(CONFIG_SEC_FACTORY)
#define PCA9468_VBATMIN_CHECK_T	0		// 0ms
#else
#define PCA9468_VBATMIN_CHECK_T	1000	// 1000ms
#endif
#define PCA9468_CCMODE_CHECK_T	2000	// 2000ms
#define PCA9468_CVMODE_CHECK_T	2000	// 2000ms
#define PCA9468_CVMODE_CHECK2_T 1000	// 1000ms
#define PCA9468_CVMODE_CHECK3_T	5000	// 5000ms
#define PCA9468_BYPMODE_CHECK_T	10000	// 10000ms
#define PCA9468_PDMSG_WAIT_T	200		// 200ms
#define PCA9468_ENABLE_DELAY_T	150		// 150ms
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define PCA9468_PPS_PERIODIC_T	7500	// 7500ms
#else
#define PCA9468_PPS_PERIODIC_T	10000	// 10000ms
#endif
#define PCA9468_BYPASS_WAIT_T	200		// 200ms
#define PCA9468_INIT_WAKEUP_T	10000	// 10000ms
#define PCA9468_DISABLE_DELAY_T	200		// 200ms
#define PCA9468_IIN_CFG_WAIT_T	150		// 150ms

/* Battery Threshold */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define PCA9468_DC_VBAT_MIN		3100000	// 3100000uV
#else
#define PCA9468_DC_VBAT_MIN		3100000	// 3100000uV
#endif
/* Input Current Limit default value */
#define PCA9468_IIN_CFG_DFT		2000000	// 2000000uA
/* Charging Current default value */
#define PCA9468_ICHG_CFG_DFT	6000000	// 6000000uA
#define PCA9468_ICHG_CFG_MAX	8000000	//uA
/* Charging Float Voltage default value */
#define PCA9468_VFLOAT_DFT		4500000	// 4500000uV
/* Sense Resistance default value */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define PCA9468_SENSE_R_DFT		0		// 5mOhm
#else
#define PCA9468_SENSE_R_DFT		1		// 10mOhm
#endif
/* Switching Frequency default value */
#define PCA9468_FSW_CFG_DFT		3		// 980kHz
/* Switching Frequency defalut value for low input current */
#define PCA9468_FSW_CFG_LOW_DFT	8		// 440kHz
/* NTC threshold voltage default value */
#define PCA9468_NTC_TH_DFT		0		// 0uV

/* Charging Done Condition */
#define PCA9468_ICHG_DONE_DFT	600000	// 600mA
#define PCA9468_IIN_DONE_DFT	500000	// 500mA

/* Maximum TA voltage threshold */
#define PCA9468_TA_MAX_VOL		10500000 // 10500000uV
/* Minimum TA voltage threshold */
#define PCA9468_TA_MIN_VOL		7000000	// 7000000uV
/* Maximum TA current threshold */
#define PCA9468_TA_MAX_CUR		2450000	// 2450000uA
/* Minimum TA current threshold */
#define PCA9468_TA_MIN_CUR		1000000	// 1000000uA - PPS minimum current

/* Minimum TA voltage threshold in Preset mode */
#if defined(CONFIG_SEC_FACTORY)
#define PCA9468_TA_MIN_VOL_PRESET	8800000	// 8800000uV
#else
#define PCA9468_TA_MIN_VOL_PRESET	8000000	// 8000000uV
#endif
/* TA voltage offset for the initial TA voltage */
#define PCA9468_TA_VOL_PRE_OFFSET	500000	// 500000uV
/* Adjust CC mode TA voltage step */
#if defined(CONFIG_SEC_FACTORY)
#define PCA9468_TA_VOL_STEP_ADJ_CC	80000	// 80000uV
#else
#define PCA9468_TA_VOL_STEP_ADJ_CC	40000	// 40000uV
#endif
/* Pre CV mode TA voltage step */
#define PCA9468_TA_VOL_STEP_PRE_CV	20000	// 20000uV
/* Pre CC mode TA voltage step */
#define PCA9468_TA_VOL_STEP_PRE_CC	100000	// 100000uV
/* IIN_CC adc offset for accuracy */
#define PCA9468_IIN_ADC_OFFSET		20000	// 20000uA
/* IIN_CC compensation offset */
#define PCA9468_IIN_CC_COMP_OFFSET	50000	// 50000uA
/* IIN_CC compensation offset in Power Limit Mode(Constant Power) TA */
#define PCA9468_IIN_CC_COMP_OFFSET_CP	20000	// 20000uA
/* TA maximum voltage that can support constant current in Constant Power Mode */
#define PCA9468_TA_MAX_VOL_CP		10000000	// 10000000uV
/* maximum retry counter for restarting charging */
#define PCA9468_MAX_RETRY_CNT		3		// 3times
/* TA IIN tolerance */
#define PCA9468_TA_IIN_OFFSET		100000	// 100mA
/* TA current low offset for reducing input current */
#define PCA9468_TA_CUR_LOW_OFFSET	200000	// 200mA
/* TA voltage offset value for bypass mode */
#define PCA9468_TA_VOL_OFFSET_BYPASS	200000	// 200mV
/* Input low current threshold to change switching frequency */
#define PCA9468_IIN_LOW_TH_SW_FREQ	1100000	// 1100000uA
/* TA voltage offset value for frequency change */
#define PCA9468_TA_VOL_OFFSET_SW_FREQ	600000	// 600mV
/* TA voltage offset value for final TA voltage */
#define PCA9468_TA_VOL_OFFSET_FINAL	300000	// 300mV

/* PD Message Voltage and Current Step */
#define PD_MSG_TA_VOL_STEP			20000	// 20mV
#define PD_MSG_TA_CUR_STEP			50000	// 50mA

#define DENOM_U_M		1000 // 1000, denominator for change between micro and mili unit

/* Step1 vfloat threshold */
#define STEP1_VFLOAT_THRESHOLD		4200000	// 4200000uV

/* RCP_DONE Error */
#define ERROR_DCRCP		99	/* RCP Error - 99 */

/* FPDO Charging Done counter */
#define FPDO_DONE_CNT	3

/* Battery connection */
#define VBAT_TO_VBPACK_OR_VBCELL	0
#define VBAT_TO_FG_VBAT				1

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define PCA9468_SEC_DENOM_U_M		1000 // 1000, denominator
#define PCA9468_SEC_FPDO_DC_IV		9	// 9V
#define PCA9468_BATT_WDT_CONTROL_T		30000	// 30s
#endif

/* INT1 Register Buffer */
enum {
	REG_INT1,
	REG_INT1_MSK,
	REG_INT1_STS,
	REG_INT1_MAX
};

/* STS Register Buffer */
enum {
	REG_STS_A,
	REG_STS_B,
	REG_STS_C,
	REG_STS_D,
	REG_STS_MAX
};

/* Direct Charging State */
enum {
	DC_STATE_NO_CHARGING,	/* No charging */

	DC_STATE_CHECK_VBAT,	/* Check min battery level */
	DC_STATE_PRESET_DC,		/* Preset TA voltage/current for the direct charging */
	DC_STATE_CHECK_ACTIVE,	/* Check active status before entering Adjust CC mode */
	DC_STATE_ADJUST_CC,		/* Adjust CC mode */
	DC_STATE_START_CC,		/* Start CC mode */
	DC_STATE_CC_MODE,		/* Check CC mode status */
	DC_STATE_START_CV,		/* Start CV mode */
	DC_STATE_CV_MODE,		/* Check CV mode status */
	DC_STATE_CHARGING_DONE,	/* Charging Done */
	DC_STATE_ADJUST_TAVOL,	/* Adjust TA voltage to set new TA current under 1000mA input */

	DC_STATE_ADJUST_TACUR,	/* Adjust TA current to set new TA current over 1000mA input */
	DC_STATE_WC_CV_MODE,	/* Check WC(Wireless Charger) CV mode status */
	DC_STATE_BYPASS_MODE,	/* Check Bypass mode status */
	DC_STATE_DCMODE_CHANGE, /* DC mode change from Normal to bypass mode */
	DC_STATE_FPDO_CV_MODE,	/* Check FPDO CV mode status */
	DC_STATE_MAX,
};

/* CC Mode Status */
enum {
	CCMODE_CHG_LOOP,
	CCMODE_VFLT_LOOP,
	CCMODE_IIN_LOOP,
	CCMODE_LOOP_INACTIVE,
	CCMODE_VIN_UVLO,
};

/* CV Mode Status */
enum {
	CVMODE_CHG_LOOP,
	CVMODE_VFLT_LOOP,
	CVMODE_IIN_LOOP,
	CVMODE_LOOP_INACTIVE,
	CVMODE_CHG_DONE,
	CVMODE_VIN_UVLO,
};

/* Timer ID */
enum {
	TIMER_ID_NONE,

	TIMER_VBATMIN_CHECK,
	TIMER_PRESET_DC,
	TIMER_PRESET_CONFIG,
	TIMER_CHECK_ACTIVE,
	TIMER_ADJUST_CCMODE,
	TIMER_ENTER_CCMODE,
	TIMER_CHECK_CCMODE,
	TIMER_ENTER_CVMODE,
	TIMER_CHECK_CVMODE,
	TIMER_PDMSG_SEND,

	TIMER_ADJUST_TAVOL,
	TIMER_ADJUST_TACUR,
	TIMER_CHECK_WCCVMODE,
	TIMER_CHECK_BYPASSMODE,
	TIMER_DCMODE_CHANGE,
	TIMER_CHECK_FPDOCVMODE,
};

/* PD Message Type */
enum {
	PD_MSG_REQUEST_APDO,
	PD_MSG_REQUEST_FIXED_PDO,
};

/* TA increment Type */
enum {
	INC_NONE,	/* No increment */
	INC_TA_VOL, /* TA voltage increment */
	INC_TA_CUR, /* TA current increment */
};

/* TA Type for the direct charging */
enum {
	TA_TYPE_UNKNOWN,
	TA_TYPE_USBPD,
	TA_TYPE_WIRELESS,
	TA_TYPE_USBPD_20,	/* USBPD 2.0 - fixed PDO */
};

/* TA Control method for the direct charging */
enum {
	TA_CTRL_CL_MODE,
	TA_CTRL_CV_MODE,
};

/* Direct Charging Mode for the direct charging */
enum {
	CHG_NO_DC_MODE,
	CHG_2TO1_DC_MODE,
	CHG_4TO1_DC_MODE,
	WC_DC_MODE,
};

/* Switching Frequency change request sequence */
enum {
	REQ_SW_FREQ_0,	/* No need or frequency change done */
	REQ_SW_FREQ_1,	/* Decrease TA voltage */
	REQ_SW_FREQ_2,	/* Disable DC */
	REQ_SW_FREQ_3,	/* Set switching frequency */
	REQ_SW_FREQ_4,	/* Set TA current */
	REQ_SW_FREQ_5,	/* Enable DC */
	REQ_SW_FREQ_6,	/* Increase TA voltage */
};

/* Revision information */
#define PCA9468_REVISION_B3	0x08
#define PCA9468_REVISION_B4	0x09

enum {
	REV_B3,		/* B3 */
	REV_B4,		/* B4 */
};

/* IIN offset as the switching frequency in uA*/
static int iin_fsw_cfg[16] = { 9990, 10540, 11010, 11520, 12000, 12520, 12990, 13470,
								5460, 6050, 6580, 7150, 7670, 8230, 8720, 9260 };

/* switching frequency in kHz */
static int sw_freq[16] = { 833, 893, 935, 980, 1020, 1080, 1120, 1160,
						440, 490, 540, 590, 630, 680, 730, 780 };

struct pca9468_platform_data {
	int	irq_gpio;	/* GPIO pin that's connected to INT# */
	unsigned int	iin_cfg;	/* Input Current Limit - uA unit */
	unsigned int 	ichg_cfg;	/* Charging Current - uA unit */
	unsigned int	ta_max_vol; /* Maximum TA voltage threshold - uV unit */
	unsigned int	v_float;	/* V_Float Voltage - uV unit */
	unsigned int 	iin_topoff;	/* Input Topoff current -uV unit */
	unsigned int 	snsres;		/* Current sense resister, 0 - 5mOhm, 1 - 10mOhm */
	unsigned int 	fsw_cfg; 	/* Switching frequency, refer to the datasheet, 0 - 833kHz, ... , 3 - 980kHz */
	unsigned int 	fsw_cfg_byp;/* Switching frequency for pass through mode, refer to the datasheet, 0 - 833kHz, ... , 3 - 980kHz */
	unsigned int 	fsw_cfg_low;/* Switching frequency for low current, refer to the datasheet, 0 - 833kHz, ... , 3 - 980kHz */
	unsigned int 	fsw_cfg_fpdo;	/* Switching frequency for fixed pdo, refer to the datasheet, 0 - 833kHz, ... , 3 - 980kHz */
	unsigned int	ntc_th;		/* NTC voltage threshold : 0~2.4V - uV unit */
	unsigned int	ntc_en;		/* Enable or Disable NTC protection, 0 - Disable, 1 - Enable */
	unsigned int	chg_mode;	/* Default chg mode, 0 - No direct charging, 1 - 2:1 charging mode, 2 - 4:1 charging mode */
	unsigned int	cv_polling;	/* CV mode polling time in step1 charging - ms unit */
	unsigned int	step1_vth;	/* Step1 vfloat threshold - uV unit */
	char			*fg_name;	/* fuelgauge power supply name */
	unsigned int	fg_vfloat;	/* Use fuelgauge vfloat method, 0 - Don't use FG Vnow and use PCA9468 VFLOAT, 1 - Use FG Vnow for VFLOAT */
	unsigned int	iin_low_freq;	/* Input current for low frequency threshold */
	unsigned int	dft_vfloat;	/* Default VFLOAT voltage - uV unit */
	unsigned int	bat_conn;	/* Battery connection, 0 - Battery Pack or Cell, 1 - FG VBATP/N */
	unsigned int	ta_volt_offset;	/* Final TA voltage fixed offset - uV unit */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int fpdo_dc_iin_topoff;	/* FPDO DC Input Topoff current - uA unit */
	unsigned int fpdo_dc_vnow_topoff;	/* FPDO DC Vnow Topoff condition - uV unit */
	unsigned int fpdo_dc_iin_lowest_limit;	/* FPDO DC IIN lowest limit condition - uA unit */
	unsigned int fpdo_dc_mainbat_topoff;	/* FPDO DC mainbat Topoff condition - uV unit */
	unsigned int fpdo_dc_subbat_topoff;	/* FPDO DC subbat Topoff condition - uV unit */

	int 			chgen_gpio;
	char 			*sec_dc_name;
#endif
};

/**
 * struct pca9468_charger - pca9468 charger instance
 * @monitor_wake_lock: lock to enter the suspend mode
 * @lock: protects concurrent access to online variables
 * @dev: pointer to device
 * @regmap: pointer to driver regmap
 * @mains: power_supply instance for AC/DC power
 * @timer_work: timer work for charging
 * @timer_id: timer id for timer_work
 * @timer_period: timer period for timer_work
 * @last_update_time: last update time after sleep
 * @pps_work: pps work for PPS periodic time
 * @pd: phandle for qualcomm PMI usbpd-phy
 * @mains_online: is AC/DC input connected
 * @bat_psy: power_supply instance for battery
 * @charging_state: direct charging state
 * @ret_state: return direct charging state after DC_STATE_ADJUST_TAVOL is done
 * @iin_cc: input current for the direct charging in cc mode, uA
 * @iin_cfg: input current limit, uA
 * @vfloat: floating voltage, uV
 * @max_vfloat: maximum float voltage, uV
 * @ichg_cfg: charging current limit, uA
 * @iin_topoff: input topoff current, uA
 * @dc_mode: direct charging mode, normal, 1:1 bypass, or 2:1 bypass mode
 * @ta_cur: AC/DC(TA) current, uA
 * @ta_vol: AC/DC(TA) voltage, uV
 * @ta_objpos: AC/DC(TA) PDO object position
 * @ta_target_vol: TA target voltage before any compensation
 * @ta_max_cur: TA maximum current of APDO, uA
 * @ta_max_vol: TA maximum voltage for the direct charging, uV
 * @ta_max_pwr: TA maximum power, uW
 * @prev_iin: Previous IIN ADC of PCA9468, uA
 * @prev_inc: Previous TA voltage or current increment factor
 * @req_new_iin: Request for new input current limit, true or false
 * @req_new_vfloat: Request for new vfloat, true or false
 * @req_new_dc_mode: Request for new dc mode, true or false
 * @new_iin: New request input current limit, uA
 * @new_vfloat: New request vfloat, uV
 * @new_dc_mode: New request dc mode, Normal mode or Bypass mode
 * @adc_comp_gain: adc gain for compensation
 * @retry_cnt: retry counter for re-starting charging if charging stop happens
 * @ta_type: TA type for the direct charging, USBPD TA or Wireless Charger.
 * @ta_ctrl: TA control method for the direct charging, Current Limit mode or Constant Voltage mode.
 * @chg_mode: Charging mode that TA can support for the direct charging, 2:1 or 4:1 mode
 * @revision: PCA9468 revision information, B3 or B4
 * @fsw_cfg: Switching frequency, FSW_CFG[3:0] bit value
 * @req_sw_freq: Switching frequency change sequence.
 * @dec_vfloat: flag for vfloat decrement, true or false
 * @req_enable: flag for enable or disable charging by battery driver, true or false.
 * @enable: flag for enable status of pca9468, true or false.
 * @prev_vbat: Previous VBAT_ADC in start cv and cv state, uV
 * @done_cnt: Charging done counter.
 * @pdata: pointer to platform data
 * @debug_root: debug entry
 * @debug_address: debug register address
 */
struct pca9468_charger {
	struct wakeup_source	*monitor_wake_lock;
	struct mutex		lock;
	struct mutex		i2c_lock;
	struct device		*dev;
	struct regmap		*regmap;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct power_supply	*psy_chg;
#else
	struct power_supply	*mains;
#endif
	struct power_supply *bat_psy;

	struct delayed_work timer_work;
	unsigned int		timer_id;
	unsigned long      	timer_period;
	unsigned long		last_update_time;

	struct delayed_work	pps_work;
#ifdef CONFIG_USBPD_PHY_QCOM
	struct usbpd 		*pd;
#endif

	bool				mains_online;
	unsigned int 		charging_state;
	unsigned int		ret_state;

	unsigned int		iin_cc;
	unsigned int		iin_cfg;
	unsigned int		vfloat;
	unsigned int		max_vfloat;
	unsigned int		ichg_cfg;
	unsigned int 		iin_topoff;
	unsigned int		dc_mode;

	unsigned int		ta_cur;
	unsigned int		ta_vol;
	unsigned int		ta_objpos;

	unsigned int		ta_target_vol;

	unsigned int		ta_max_cur;
	unsigned int		ta_max_vol;
	unsigned int		ta_max_pwr;

	unsigned int		prev_iin;
	unsigned int		prev_inc;

	bool				req_new_iin;
	bool				req_new_vfloat;
	bool				req_new_dc_mode;
	unsigned int		new_iin;
	unsigned int		new_vfloat;
	unsigned int		new_dc_mode;

	int					adc_comp_gain;

	int					retry_cnt;
	int					ta_type;
	int					ta_ctrl;
	int					chg_mode;

	int					revision;

	unsigned int		fsw_cfg;
	unsigned int		req_sw_freq;

	bool				dec_vfloat;

	bool				req_enable;
	bool				enable;

	int					prev_vbat;

	int					done_cnt;

	struct pca9468_platform_data *pdata;

	/* debug */
	struct dentry		*debug_root;
	u32					debug_address;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int		fpdo_dc_iin_topoff;
	unsigned int		fpdo_dc_vnow_topoff;
	unsigned int		fpdo_dc_mainbat_topoff;
	unsigned int		fpdo_dc_subbat_topoff;

	int input_current;
	int charging_current;
	int float_voltage;
	int chg_status;
	int health_status;
	bool wdt_kick;

	int adc_val[ADCCH_MAX];

	unsigned int pdo_index;
	unsigned int pdo_max_voltage;
	unsigned int pdo_max_current;

	struct delayed_work wdt_control_work;
#endif
};

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
extern int sec_pd_select_pps(int num, int ppsVol, int ppsCur);
extern int sec_pd_get_apdo_max_current(unsigned int *pdo_pos, unsigned int taMaxVol, unsigned int *taMaxCur);
#endif
#endif
