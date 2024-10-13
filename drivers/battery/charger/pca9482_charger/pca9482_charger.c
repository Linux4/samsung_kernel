/*
 * Driver for the NXP PCA9482 battery charger.
 *
 * Copyright 2021-2023 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/debugfs.h>
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "pca9482_charger.h"
#include "../../common/sec_charging_common.h"
#include "../../common/sec_direct_charger.h"
#include <linux/battery/sec_pd.h>
#else
#include <linux/power/pca9482_charger.h>
#endif
#include <linux/completion.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include <linux/pm_wakeup.h>
#ifdef CONFIG_USBPD_PHY_QCOM
#include <linux/usb/usbpd.h>		// Use Qualcomm USBPD PHY
#endif

#define DRV_MODULE_VERSION	"1.0.9S"

#define BITS(_end, _start) ((BIT(_end) - BIT(_start)) + BIT(_end))
#define MASK2SHIFT(_mask)	__ffs(_mask)
#define MIN(a, b)	((a < b) ? (a):(b))
#define MAX(a, b)	((a > b) ? (a):(b))

/************************/
/* PCA9482 Register Map */
/************************/
#define PCA9482_REG_DEVICE_ID 					0x00	// Device ID Register
#define PCA9482_BIT_DEV_REV						BITS(7, 4)
#define PCA9482_BIT_DEV_ID						BITS(3, 0)
#define PCA9482_DEVICE_ID						0x00	// Default ID, N/A now

#define PCA9482_REG_INT_DEVICE_0				0x01	// Interrupt Register 0 for device operation
#define PCA9482_BIT_VOUT_MAX_OV					BIT(7)
#define PCA9482_BIT_RCP_DETECTED				BIT(6)
#define PCA9482_BIT_VIN_UNPLUG					BIT(5)
#define PCA9482_BIT_VIN_OVP						BIT(4)
#define PCA9482_BIT_VIN_OV_TRACKING				BIT(3)
#define PCA9482_BIT_VIN_UV_TRACKING				BIT(2)
#define PCA9482_BIT_VIN_NOT_VALID				BIT(1)
#define PCA9482_BIT_VIN_VALID					BIT(0)

#define PCA9482_REG_INT_DEVICE_1				0x02	// Interrupt Register 1 for device operation
#define PCA9482_BIT_NTC_1_DETECTED				BIT(6)
#define PCA9482_BIT_NTC_0_DETECTED				BIT(5)
#define PCA9482_BIT_ADC_READ_DONE				BIT(4)
#define PCA9482_BIT_VIN_OCP_21_11				BIT(2)
#define PCA9482_BIT_SINK_RCP_TIMEOUT			BIT(1)
#define PCA9482_BIT_SINK_RCP_ENABLED			BIT(0)

#define PCA9482_REG_INT_DEVICE_2				0x03	// Interrupt Register 2 for device operation
#define PCA9482_BIT_VIN_OCP_12_11				BIT(6)
#define PCA9482_BIT_THEM_REG_EXIT				BIT(4)
#define PCA9482_BIT_THEM_REG					BIT(3)
#define PCA9482_BIT_THSD_EXIT					BIT(2)
#define PCA9482_BIT_THSD						BIT(1)
#define PCA9482_BIT_WATCHDOG_TIMER_OUT			BIT(0)

#define PCA9482_REG_INT_DEVICE_3				0x04	// Interrupt Register 3 for device operation
#define PCA9482_BIT_STATUS_CHANGE_INT			BIT(6)
#define PCA9482_BIT_VIN_OCP_ALARM_12_11_EXIT	BIT(3)
#define PCA9482_BIT_VIN_OCP_ALARM_12_11			BIT(2)
#define PCA9482_BIT_VIN_OCP_ALARM_21_11_EXIT	BIT(1)
#define PCA9482_BIT_VIN_OCP_ALARM_21_11			BIT(0)

#define PCA9482_REG_INT_CHARGING				0x05	// Interrupt Register for Charging operation
#define PCA9482_BIT_CHG_SAFETY_TIMER			BIT(5)
#define PCA9482_BIT_VBAT_OVP_EXIT				BIT(4)
#define PCA9482_BIT_VBAT_OVP					BIT(3)
#define PCA9482_BIT_VBAT_REG_LOOP				BIT(2)
#define PCA9482_BIT_I_VBAT_CC_LOOP				BIT(1)
#define PCA9482_BIT_I_VIN_CC_LOOP				BIT(0)

#define PCA9482_REG_INT_SC_0					0x06	// Interrupt Register 0 for Switched Capacitor(SC) operation
#define PCA9482_BIT_PHASE_B_FAULT				BIT(6)
#define PCA9482_BIT_PHASE_A_FAULT				BIT(5)
#define PCA9482_BIT_11_ENABLED					BIT(4)
#define PCA9482_BIT_STANDBY_EXIT				BIT(3)
#define PCA9482_BIT_STANDBY_ENTER				BIT(2)
#define PCA9482_BIT_SWITCHING_ENABLED			BIT(1)
#define PCA9482_BIT_SC_OFF						BIT(0)

#define PCA9482_REG_INT_SC_1					0x07	// Interrupt Register 1 for Switched Capacitor(SC) operation
#define PCA9482_BIT_CBST_SHORT					BIT(6)
#define PCA9482_BIT_CFLY_SHORT					BIT(5)
#define PCA9482_BIT_VIN_SHORT					BIT(4)
#define PCA9482_BIT_OVPOUT_SHORT				BIT(3)
#define PCA9482_BIT_OVPOUT_ERRLO				BIT(2)
#define PCA9482_BIT_SW_SHORT					BIT(1)
#define PCA9482_BIT_REVERSE_SW_SS_OC			BIT(0)

#define PCA9482_REG_INT_DEVICE_0_MASK			0x08	// Interrupt Mask Register for INT_DEVICE_0 register
#define PCA9482_REG_INT_DEVICE_1_MASK			0x09	// Interrupt Mask Register for INT_DEVICE_1 register
#define PCA9482_REG_INT_DEVICE_2_MASK			0x0A	// Interrupt Mask Register for INT_DEVICE_2 register
#define PCA9482_REG_INT_DEVICE_3_MASK			0x0B	// Interrupt Mask Register for INT_DEVICE_3 register
#define PCA9482_REG_INT_CHARGING_MASK			0x0C	// Interrupt Mask Register for INT_CHARGING register
#define PCA9482_REG_INT_SC_0_MASK				0x0D	// Interrupt Mask Register for INT_SC_0 register
#define PCA9482_REG_INT_SC_1_MASK				0x0E	// Interrupt Mask Register for INT_SC_1 register

#define PCA9482_REG_DEVICE_0_STS				0x0F	// Status Register for INT_DEVICE_0 register
#define PCA9482_REG_DEVICE_1_STS				0x10	// Status Register for INT_DEVICE_1 register
#define PCA9482_REG_DEVICE_2_STS				0x11	// Status Register for INT_DEVICE_2 register
#define PCA9482_REG_DEVICE_3_STS				0x12	// Status Register for INT_DEVICE_3 register
#define PCA9482_BIT_STATUS_CHANGE				BITS(7, 6)
#define PCA9482_REG_CHARGING_STS				0x13	// Status Register for INT_CHARGING register
#define PCA9482_REG_SC_0_STS					0x14	// Status Register for INT_SC_0 registger
#define PCA9482_REG_SC_1_STS					0x15	// Status Register for INT_SC_1 registger

#define PCA9482_REG_DEVICE_CNTL_0				0x16	// Device control register 0
#define PCA9482_BIT_STANDBY_BY_NTC_EN			BIT(7)
#define PCA9482_BIT_THERMAL_SHUTDOWN_CFG		BITS(6, 5)
#define PCA9482_BIT_WATCHDOG_TIMER_DOUBLE_EN	BIT(4)
#define PCA9482_BIT_WATCHDOG_CFG				BITS(3, 2)
#define PCA9482_BIT_WATCHDOG_EN					BIT(1)
#define PCA9482_BIT_SOFT_RESET					BIT(0)

#define PCA9482_REG_DEVICE_CNTL_1				0x17	// Device control register 1
#define PCA9482_BIT_HALF_VIN_OVP_EN				BIT(7)
#define PCA9482_BIT_LOW_POWER_MODE_DISABLE		BIT(6)
#define PCA9482_BIT_VIN_OVP_CFG					BITS(5, 4)
#define PCA9482_BIT_VIN_FIXED_OVP_EN			BIT(3)
#define PCA9482_BIT_THERMAL_REGULATION_CFG		BITS(2, 1)
#define PCA9482_BIT_THERMAL_REGULATION_EN		BIT(0)

#define PCA9482_REG_DEVICE_CNTL_2				0x18	// Device control register 2
#define PCA9482_BIT_IBAT_SENSE_R_CFG			BIT(7)
#define PCA9482_BIT_EN_CFG						BIT(6)

#define PCA9482_BIT_VIN_VALID_DEGLITCH			BITS(1, 0)

#define PCA9482_REG_DEVICE_CNTL_3				0x19	// Device control register 3
#define PCA9482_BIT_FORCE_SHUTDOWN				BIT(5)
#define PCA9482_BIT_VIN_OCP_ALRAM_CURRENT_12_11	BITS(4, 1)
#define PCA9482_BIT_VIN_ALARM_OCP_12_11_EN		BIT(0)

#define PCA9482_REG_AUTO_RESTART_CNTL_0				0x1A	// Device auto restart register 0
#define PCA9482_BIT_AUTO_RESTART_NTC_EN				BIT(7)
#define PCA9482_BIT_AUTO_RESTART_FIXED_OV_EN		BIT(6)
#define PCA9482_BIT_AUTO_RESTART_OV_TRACKING_EN		BIT(5)
#define PCA9482_BIT_AUTO_RESTART_UV_TRACKING_EN		BIT(4)
#define PCA9482_BIT_AUTO_RESTART_THEM_EN			BIT(3)
#define PCA9482_BIT_AUTO_RESTART_VBAT_OVP_EN		BIT(2)
#define PCA9482_BIT_AUTO_RESTART_VIN_OCP_21_11_EN	BIT(1)
#define PCA9482_BIT_AUTO_RESTART_RCP_EN				BIT(0)



#define PCA9482_REG_RCP_CNTL					0x1C	// RCP control register
#define PCA9482_BIT_I_RCP_CURRENT_DEGLITCH		BITS(7, 6)
#define PCA9482_BIT_I_SINK_RCP_TIMER			BIT(5)
#define PCA9482_BIT_I_RCP_THRESHOLD				BITS(4, 2)
#define PCA9482_BIT_I_SINK_RCP					BIT(1)
#define PCA9482_BIT_RCP_EN						BIT(0)

#define PCA9482_REG_CHARGING_CNTL_0				0x1D	// Charging control register 0
#define PCA9482_BIT_VIN_CURRENT_SLOPE			BIT(7)
#define PCA9482_BIT_OCP_DEGLITCH_TIME_21_11		BIT(6)
#define PCA9482_BIT_VIN_CURRENT_OCP_21_11		BIT(5)
#define PCA9482_BIT_VIN_OCP_21_11_EN			BIT(4)
#define PCA9482_BIT_CSP_CSN_MEASURE_EN			BIT(3)
#define PCA9482_BIT_VBAT_LOOP_EN				BIT(2)
#define PCA9482_BIT_I_VBAT_LOOP_EN				BIT(1)
#define PCA9482_BIT_I_VIN_LOOP_EN				BIT(0)

#define PCA9482_REG_CHARGING_CNTL_1				0x1E	// Charging control register 1
#define PCA9482_BIT_VIN_REGULATION_CURRENT		BITS(7, 0)

#define PCA9482_REG_CHARGING_CNTL_2				0x1F	// Charging control register 2
#define PCA9482_BIT_VBAT_REGULATION				BITS(7, 0)

#define PCA9482_REG_CHARGING_CNTL_3				0x20	// Charging control register 3
#define PCA9482_BIT_I_VBAT_REGULATION			BITS(7, 0)

#define PCA9482_REG_CHARGING_CNTL_4				0x21	// Charging control register 4
#define PCA9482_BIT_IBAT_SENSE_R_SEL			BITS(7, 6)
#define PCA9482_BIT_VIN_ALARM_OCP_21_11_EN		BIT(5)
#define PCA9482_BIT_CHARGER_SAFETY_TIMER		BITS(4, 3)
#define PCA9482_BIT_CHARGER_SAFETY_TIMER_EN		BIT(2)
#define PCA9482_BIT_VBAT_OVP_DEGLITCH_TIME		BITS(1, 0)

#define PCA9482_REG_CHARGING_CNTL_5				0x22	// Charging control register 5
#define PCA9482_BIT_VBAT_OVP_EN					BIT(6)
#define PCA9482_BIT_VIN_OCP_CURRENT_12_11		BITS(5, 2)
#define PCA9482_BIT_OCP_DEGLITCH_TIME_12_11		BIT(1)
#define PCA9482_BIT_VIN_OCP_12_11_EN			BIT(0)

#define PCA9482_REG_CHARGING_CNTL_6				0x23	// Charging control register 6
#define PCA9482_BIT_VIN_OCP_ALARM_CURRENT		BITS(7, 0)

#define PCA9482_REG_NTC_0_CNTL					0x24	// NTC 0 control register
#define PCA9482_BIT_NTC_0_TRIGGER_VOLTAGE		BITS(7, 1)
#define PCA9482_BIT_NTC_EN						BIT(0)

#define PCA9482_REG_NTC_1_CNTL					0x25	// NTC 1 control register
#define PCA9482_BIT_NTC_1_TRIGGER_VOLTAGE		BITS(6, 0)

#define PCA9482_REG_SC_CNTL_0					0x26	// Switched Capacitor converter control register 0
#define PCA9482_BIT_FSW_CFG						BITS(4, 0)

#define PCA9482_REG_SC_CNTL_1					0x27	// Switched Capacitor converter control register 1
#define PCA9482_BIT_VIN_UV_TRACKING_DEGLITCH	BITS(5, 4)
#define PCA9482_BIT_UV_TRACKING_HYSTERESIS		BIT(3)
#define PCA9482_BIT_UV_TRACK_DELTA				BITS(2, 1)
#define PCA9482_BIT_UV_TRACKING_EN				BIT(0)

#define PCA9482_REG_SC_CNTL_2					0x28	// Switched Capacitor converter control register 2
#define PCA9482_BIT_VOUT_MAX_OV_EN				BIT(4)
#define PCA9482_BIT_OV_TRACK_DELTA				BITS(3, 2)
#define PCA9482_BIT_OV_TRACKING_HYSTERESIS		BIT(1)
#define PCA9482_BIT_OV_TRACKING_EN				BIT(0)

#define PCA9482_REG_SC_CNTL_3					0x29	// Switched Capacitor converter control register 3
#define PCA9482_BIT_STANDBY_EN					BIT(7)
#define PCA9482_BIT_SC_OPERATION_MODE			BITS(6, 5)
#define PCA9482_BIT_PRECHARGE_CFLY_TIME_OUT		BITS(4, 3)
#define PCA9482_BIT_PRECHARGE_CFLY_I			BITS(2, 0)

#define PCA9482_REG_ADC_CNTL					0x2A	// ADC control register
#define PCA9482_BIT_ADC_IN_SHUTDOWN_STATE		BIT(7)
#define PCA9482_BIT_ADC_MODE_CFG				BITS(6, 5)
#define PCA9482_BIT_ADC_HIBERNATE_READ_INTERVAL	BITS(4, 3)
#define PCA9482_BIT_ADC_AVERAGE_TIMES			BITS(2, 1)
#define PCA9482_BIT_ADC_EN						BIT(0)

#define PCA9482_REG_ADC_EN_CNTL_0				0x2B	// ADC enable control register 0
#define PCA9482_BIT_ADC_READ_I_BAT_CURRENT_EN	BIT(7)
#define PCA9482_BIT_ADC_READ_VIN_CURRENT_EN		BIT(6)
#define PCA9482_BIT_ADC_READ_DIE_TEMP_EN		BIT(5)
#define PCA9482_BIT_ADC_READ_NTC_EN				BIT(4)
#define PCA9482_BIT_ADC_READ_VOUT_EN			BIT(3)
#define PCA9482_BIT_ADC_READ_BATP_BATN_EN		BIT(2)
#define PCA9482_BIT_ADC_READ_OVP_OUT_EN			BIT(1)
#define PCA9482_BIT_ADC_READ_VIN_EN				BIT(0)

#define PCA9482_REG_ADC_EN_CNTL_1				0x2C	// ADC enable control register 1

#define PCA9482_REG_ADC_READ_VIN_0				0x2D	// ADC VIN read register 0
#define PCA9482_REG_ADC_READ_VIN_1				0x2E	// ADC VIN read register  1
#define PCA9482_REG_ADC_READ_RESERVED_0			0x2F	// ADC XXX read register 0
#define PCA9482_REG_ADC_READ_RESERVED_1			0x30	// ADC XXX read register 1
#define PCA9482_REG_ADC_READ_OVP_OUT_0			0x31	// ADC OVP_OUT read registe 0
#define PCA9482_REG_ADC_READ_OVP_OUT_1			0x32	// ADC OVP_OUT read registe 1
#define PCA9482_REG_ADC_READ_BATP_BATN_0		0x33	// ADC BATP_BATN read registe 0
#define PCA9482_REG_ADC_READ_BATP_BATN_1		0x34	// ADC BATP_BATN read registe 1
#define PCA9482_REG_ADC_READ_VOUT_0				0x35	// ADC VOUT read registe 0
#define PCA9482_REG_ADC_READ_VOUT_1				0x36	// ADC VOUT read registe 1
#define PCA9482_REG_ADC_READ_NTC_0				0x37	// ADC NTC read registe 0
#define PCA9482_REG_ADC_READ_NTC_1				0x38	// ADC NTC read registe 1
#define PCA9482_REG_ADC_READ_DIE_TEMP_0			0x39	// ADC Die temperature read registe 0
#define PCA9482_REG_ADC_READ_DIE_TEMP_1			0x3A	// ADC Die temperature read registe 1
#define PCA9482_REG_ADC_READ_VIN_CURRENT_0		0x3B	// ADC VIN current read registe 0
#define PCA9482_REG_ADC_READ_VIN_CURRENT_1		0x3C	// ADC VIN current read registe 1
#define PCA9482_REG_ADC_READ_I_VBAT_CURRENT_0	0x3D	// ADC I_VBAT charge current read registe 0
#define PCA9482_REG_ADC_READ_I_VBAT_CURRENT_1	0x3E	// ADC I_VBAT charge current read registe 1
#define PCA9482_BIT_ADC_READ_7_0				BITS(7, 0)	// ADC bit[7:0]
#define PCA9482_BIT_ADC_READ_11_8				BITS(3, 0)	// ADC bit[11:8]

#define PCA9482_MAX_REGISTER			0x4F

#define PCA9482_REG_BIT_AI				BIT(7)	// Auto Increment bit


/* Regulation voltage and current */
#define PCA9482_IIN_REG_STEP			25000	// VIN_REGULATION_CURRENT, input current step, unit - uA
#define PCA9482_IBAT_REG_STEP			50000	// I_VBAT_REGUATION, charge current step, unit - uA
#define PCA9482_VBAT_REG_STEP			5000	// VBAT_REGULATION, battery regulation voltage step, unit - uV

#define PCA9482_IIN_REG_MAX				3500000		// 3.5A
#define PCA9482_IBAT_REG_MAX			7000000		// 7A
#define PCA9482_VBAT_REG_MAX			5000000		// 5V

#define PCA9482_IIN_REG_MIN				500000		// 500mA
#define PCA9482_IBAT_REG_MIN			1000000		// 1000mA
#define PCA9482_VBAT_REG_MIN			3725000		// 3725mV

#define PCA9482_IIN_REG(_input_current)	((_input_current - PCA9482_IIN_REG_MIN)/PCA9482_IIN_REG_STEP)	// input current, unit - uA
#define PCA9482_IBAT_REG(_chg_current)	((_chg_current - PCA9482_IBAT_REG_MIN)/PCA9482_IBAT_REG_STEP)	// charge current, unit - uA
#define PCA9482_VBAT_REG(_bat_voltage)	((_bat_voltage - PCA9482_VBAT_REG_MIN)/PCA9482_VBAT_REG_STEP)	// battery voltage, unit - uV

/* VIN OCP current for 1:2 switching and 1:1 reverse mode*/
#define PCA9482_VIN_OCP_12_11_STEP		100000		// 100mA
#define PCA9482_VIN_OCP_12_11_MIN		500000		// 500mA
#define PCA9482_VIN_OCP_12_11_MAX		2000000		// 2000mA
#define PCA9482_VIN_OCP_12_11(_ocp_current)	((_ocp_current - PCA9482_VIN_OCP_12_11_MIN)/PCA9482_VIN_OCP_12_11_STEP)	// ocp current, unit - uA

/* NTC trigger voltage */
#define PCA9482_NTC_TRIGGER_VOLTAGE_STEP	15000	// 15mV, unit - uV
#define PCA9482_NTC_TRIGGER_VOLTAGE(_ntc_vol)	(_ntc_vol/PCA9482_NTC_TRIGGER_VOLTAGE_STEP)	// ntc voltage, unit - uV

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define PCA9482_SEC_DENOM_U_M		1000 // 1000, denominator
#define PCA9482_SEC_FPDO_DC_IV		9	// 9V
#define PCA9482_BATT_WDT_CONTROL_T		30000	// 30s
#endif

/* IBAT sense location */
enum {
	IBAT_SENSE_R_BOTTOM_SIDE,
	IBAT_SENSE_R_TOP_SIDE,
};

/* IBAT sense resistor */
enum {
	IBAT_SENSE_R_1mOhm,
	IBAT_SENSE_R_2mOhm,
	IBAT_SENSE_R_5mOhm,
};

/* VIN OV TRACK DELTA */
enum {
	OV_TRACK_DELTA_200mV,
	OV_TRACK_DELTA_400mV,
	OV_TRACK_DELTA_600mV,
	OV_TRACK_DELTA_800mV,
};

/* VIN UV TRACK DELTA */
enum {
	UV_TRACK_DELTA_0mV,
	UV_TRACK_DELTA_200mV,
	UV_TRACK_DELTA_400mV,
	UV_TRACK_DELTA_600mV,
};

/* ADC_AVERAGE_TIMES  */
enum {
	ADC_AVG_2sample = 0,
	ADC_AVG_4sample,
	ADC_AVG_8sample,
	ADC_AVG_16sample,
};

/* ADC operation mode */
enum {
	AUTO_MODE = 0,
	FORCE_SHUTDOWN_MODE,
	FORCE_HIBERNATE_MODE,
	FORCE_NORMAL_MODE,
};

/* Switching frequency */
#define PCA9482_FSW_CFG_STEP	50	// 50kHz
#define PCA9482_FSW_CFG(_frequency)	((_frequency - 200)/PCA9482_FSW_CFG_STEP)	// switching frequency, unit - kHz

/* Enable pin polarity selection */
#define PCA9482_EN_ACTIVE_H		0x00
#define PCA9482_EN_ACTIVE_L		PCA9482_BIT_EN_CFG
#define PCA9482_STANDBY_FORCE	PCA9482_BIT_STANDBY_EN
#define PCA9482_STANDBY_DONOT	0

/* Operation SC mode */
#define PCA9482_SC_OP_21	0x0		// 00b: 2:1 Switching Mode
#define PCA9482_SC_OP_12	0x20	// 01b: 1:2 Switching Mode
#define PCA9482_SC_OP_F_11	0x40	// 10b: Forward 1:1 mode
#define PCA9482_SC_OP_R_11	0x60	// 11b: Reverse 1:1 mode

/* Device current status */
#define PCA9482_SHUTDOWN_STATE	0x00	// 00b: in shutdown state
#define PCA9482_STANDBY_STATE	0x40	// 01b: in standby state
#define PCA9482_21SW_F11_MODE	0x80	// 10b: 2:1 switching or forward 1:1 mode
#define PCA9482_12SW_R11_MODE	0xC0	// 11b: 1:2 switching or reverse 1:1 mode

/* ADC Channel */
enum {
	ADCCH_VIN,
	ADCCH_RESERVED,
	ADCCH_OVP_OUT,
	ADCCH_BATP_BATN,
	ADCCH_VOUT,
	ADCCH_NTC,
	ADCCH_DIE_TEMP,
	ADCCH_VIN_CURRENT,
	ADCCH_BAT_CURRENT,
	ADC_READ_MAX,
};

/* ADC step and maximum value */
#define VIN_STEP			4000		// 4mV(4000uV) LSB, Range(0V ~ 15.36V)
#define VIN_MAX				15360000	// 15.36V(15360mV, 15360000uV)
#define OVP_OUT_STEP		4000		// 4mV(4000uV) LSB, Range(0V ~ 15.36V)
#define OVP_OUT_MAX			15360000	// 15.36V(15360mV, 15360000uV)
#define BATP_BATN_STEP		2000		// 2mV(2000uV) LSB, Range(0V ~ 5V)
#define BATP_BATN_MAX		5000000		// 5V(5000mV, 5000000uV)
#define VOUT_STEP			2000		// 2mV(2000uV) LSB, Range(0V ~ 5V)
#define VOUT_MAX			5000000		// 5V(5000mV, 5000000uV)
#define NTC_STEP			1000		// 1mV(1000uV) LSB, Range(0V ~ 3.3V)
#define NTC_MAX				1500000		// 1.5V(1500mV, 1500000uV)
#define DIE_TEMP_STEP		500			// 0.5C LSB, Range(0 ~ 150C)
#define DIE_TEMP_DENOM		1000		// 1000, denominator
#define DIE_TEMP_MAX		150			// 150C
#define VIN_CURRENT_STEP	2000		// 2mA(2000uA) LSB, Range(0A ~ 6.5A)
#define VIN_CURRENT_MAX		6500000		// 6.5A(6500mA, 6500000uA)
#define BAT_CURRENT_STEP	5000		// 5mA(5000uA) LSB, Range(0A ~ 10A)
#define BAT_CURRENT_MAX		10000000	// 10A(10000mA, 10000000uA)


/* Timer definition */
#define VBATMIN_CHECK_T	0	// Vbat min check timer - 0ms
#define CCMODE_CHECK_T	2000	// CC mode polling timer - 5000ms --> 2000ms
#define CVMODE_CHECK_T	2000	// CV mode polling timer - 10000ms --> 2000ms
#define CVMODE_CHECK2_T	1000	// CV mode polling timer2 - 1000ms
#define CVMODE_CHECK3_T	5000	// CV mode polling timer3 for fixed PDO - 5000ms
#define BYPMODE_CHECK_T	10000	// Bypass mode polling timer - 10000ms
#define PDMSG_WAIT_T	200		// PD message waiting time - 200ms
#define ENABLE_DELAY_T	150		// DC Enable waiting time - 150ms
#define PPS_PERIODIC_T	10000	// PPS periodic request message timer - 10000ms
#define UVLO_CHECK_T	1000	// UVLO check timer - 1000ms
#define BYPASS_WAIT_T	200		// Bypass mode waiting time - 200ms
#define INIT_WAKEUP_T	10000	// Initial wakeup timeout - 10000ms
#define DISABLE_DELAY_T	300		// DC Disable waiting time for sw_freq change - 300ms
#define REVERSE_WAIT_T	10		// Reverse mode waiting time - 200ms --> 10ms
#define REVERSE_CHECK_T	5000	// Reverse mode polling timer - 5000ms
#define IIN_CFG_WAIT_T	150		// Input regulation settle time for soft start - 150ms

/* Battery minimum voltage Threshold for direct charging */
#define DC_VBAT_MIN				3100000	// 3100000uV
/* Battery minimum voltage threshold for direct charging error */
#define DC_VBAT_MIN_ERR			3100000	// 3100000uV
/* Input Current Limit default value - Input Current Regulation */
#define PCA9482_IIN_REG_DFT		2500000	// 2500000uA
#define PCA9482_IIN_REG_OFFSET	200000	// 200mA - Software offset for DC algorithm
#define PCA9482_IIN_REG_OFFSET_FPDO	200000	// 200mA - for FPDO
/* Charging Current default value - Charge Current Regulation */
#define PCA9482_IBAT_REG_DFT	5000000	// 5000000uA
#define PCA9482_IBAT_REG_OFFSET	100000	// 100mA - Software offset for DC algorithm
/* Charging Float Voltage default value - Battery Voltage Regulation */
#define PCA9482_VBAT_REG_DFT	4500000	// 4500000uV --> 4500mV

/* IBAT Sense Resistor default value */
#define PCA9482_SENSE_R_DFT		IBAT_SENSE_R_1mOhm	// 1mOhm
/* IBAT Sense Resistor location default value */
#define PCA9482_SENSE_R_CFG_DFT	IBAT_SENSE_R_BOTTOM_SIDE	// Bottom side - connect to BATN
/* Switching Frequency default value */
#define PCA9482_FSW_CFG_DFT		1000	// 1.0MHz(1000kHz)
/* Switching Frequency default value for bypass */
#define PCA9482_FSW_CFG_BYP_DFT	500		// 500kHz
/* Switching Frequency default value for low current */
#define PCA9482_FSW_CFG_LOW_DFT	500		// 500kHz
/* NTC_0 threshold voltage default value */
#define PCA9482_NTC_0_TH_DFT	1110000	// 1.11V(1110000uV)
/* NTC_1 threshold voltage default value */
#define PCA9482_NTC_1_TH_DFT	495000	// 0.495V(495000uV)

/* Charging Done Condition */
#define ICHG_DONE_DFT	1000000	// 1000mA
#define IIN_DONE_DFT	500000	// 500mA

/* Maximum TA voltage threshold */
#define TA_MAX_VOL		10200000 // 10200000uV
/* Minimum TA voltage threshold */
#define TA_MIN_VOL		7000000	// 7000000uV
/* Maximum TA current threshold */
#define TA_MAX_CUR		2500000	// 2500000uA
/* Minimum TA current threshold */
#define TA_MIN_CUR		1000000	// 1000000uA - PPS minimum current

/* Minimum TA voltage threshold in Preset mode */
#define TA_MIN_VOL_PRESET	7700000	// 7700000uV
/* TA voltage offset for the initial TA voltage */
#define TA_VOL_PRE_OFFSET	500000	// 500000uV
/* Adjust CC mode TA voltage step */
#define TA_VOL_STEP_ADJ_CC	40000	// 40000uV
/* Pre CC mode TA voltage step */
#ifdef CONFIG_SEC_FACTORY
#define TA_VOL_STEP_PRE_CC	200000	// 200000uV
#else
#define TA_VOL_STEP_PRE_CC	100000	// 100000uV
#endif
/* Pre CV mode TA voltage step */
#define TA_VOL_STEP_PRE_CV	40000	// 40000uV
/* IIN_CC adc offset for accuracy */
#define IIN_ADC_OFFSET		20000	// 20000uA
/* IIN_CC compensation offset */
#define IIN_CC_COMP_OFFSET	50000	// 50000uA
/* IIN_CC compensation offset in Power Limit Mode(Constant Power) TA */
#define IIN_CC_COMP_OFFSET_CP	20000	// 20000uA
/* TA maximum voltage that can support constant current in Constant Power Mode */
#define TA_MAX_VOL_CP		10000000	// 9760000uV --> 9800000uV --> 10000000uV
/* maximum retry counter for restarting charging */
#define MAX_RETRY_CNT		3		// 3times
/* TA IIN tolerance */
#define TA_IIN_OFFSET		100000	// 100mA
#ifdef CONFIG_SEC_FACTORY
/* TA decrement step in CC state for factory test */
#define TA_VOL_DEC_STEP_CC	80000	// 80mV - for fast test
#endif
/* TA current low offset for reducing input current */
#ifdef CONFIG_SEC_FACTORY
#define TA_CUR_LOW_OFFSET	0	// 0mA - UCT300 does not work CL mode
#else
#define TA_CUR_LOW_OFFSET	200000	// 200mA
#endif
/* TA voltage offset for 1:1 bypass mode */
#define TA_VOL_OFFSET_1TO1_BYPASS	100000	// 100mV
/* TA voltalge offset for 2:1 bypass mode */
#define TA_VOL_OFFSET_2TO1_BYPASS	200000	// 200mV
/* Input low current threshold to change switching frequency */
#define IIN_LOW_TH_SW_FREQ		1100000	// 1100000uA
/* TA voltage offset value for frequency change */
#define TA_VOL_OFFSET_SW_FREQ	600000	// 600mV
/* TA voltage offset value for final TA voltage */
#define PCA9482_TA_VOL_OFFSET_FINAL	300000	// 300mV

/* PD Message Voltage and Current Step */
#define PD_MSG_TA_VOL_STEP			20000	// 20mV
#define PD_MSG_TA_CUR_STEP			50000	// 50mA

#define DENOM_U_M		1000 // 1000, denominator for change between micro and mili unit

/* Maximum WCRX voltage threshold */
#define WCRX_MAX_VOL	10000000	// 10000000uV
/* WCRX voltage Step */
#define WCRX_VOL_STEP	100000		// 100mV

/* Switching charger minimum current */
#define SWCHG_ICL_MIN				100000	// 100mA
#define SWCHG_ICL_NORMAL			3000000 // 3000mA

/* Step1 vfloat threshold */
#define STEP1_VFLOAT_THRESHOLD		4200000	// 4200000uV

/* RCP_DONE Error */
#define ERROR_DCRCP		99	/* RCP Error - 99 */

/* FPDO Charging Done counter */
#define FPDO_DONE_CNT	3

/* Battery connection */
#define VBAT_TO_VBPACK_OR_VBCELL	0
#define VBAT_TO_FG_VBAT				1

enum {
	WDT_DISABLE,
	WDT_ENABLE,
};

enum {
	WDT_4SEC,
	WDT_8SEC,
	WDT_16SEC,
	WDT_32SEC,
};

/* Interrupt and Status Register Buffer */
enum {
	REG_DEVICE_0,
	REG_DEVICE_1,
	REG_DEVICE_2,
	REG_DEVICE_3,
	REG_CHARGING,
	REG_SC_0,
	REG_SC_1,
	REG_BUFFER_MAX
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
	DC_STATE_BYPASS_MODE,	/* Check Bypass mode status */
	DC_STATE_DCMODE_CHANGE,	/* DC mode change from Normal to 1:1 or 2:1 bypass */
	DC_STATE_REVERSE_MODE,	/* Reverse 1:2 switching or reverse 1:1 bypass */
	DC_STATE_FPDO_CV_MODE,	/* Check FPDO CV mode status */
	DC_STATE_MAX,
};

/* DC Mode Status */
enum {
	DCMODE_CHG_LOOP,
	DCMODE_VFLT_LOOP,
	DCMODE_IIN_LOOP,
	DCMODE_LOOP_INACTIVE,
	DCMODE_CHG_DONE,
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
	TIMER_CHECK_BYPASSMODE,
	TIMER_DCMODE_CHANGE,
	TIMER_START_REVERSE,
	TIMER_CHECK_REVERSE_ACTIVE,
	TIMER_CHECK_REVERSE_MODE,
	TIMER_CHECK_FPDOCVMODE,
};

/* PD Message Type */
enum {
	PD_MSG_REQUEST_APDO,
	PD_MSG_REQUEST_FIXED_PDO,
	WCRX_REQUEST_VOLTAGE,
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

/**
 * struct pca9482_charger - pca9482 charger instance
 * @monitor_wake_lock: lock to enter the suspend mode
 * @lock: protects concurrent access to online variables
 * @dev: pointer to device
 * @regmap: pointer to driver regmap
 * @mains: power_supply instance for AC/DC power
 * @dc_wq: work queue for the algorithm and monitor timer
 * @timer_work: timer work for charging
 * @timer_id: timer id for timer_work
 * @timer_period: timer period for timer_work
 * @pps_work: pps work for PPS periodic time
 * @pd: phandle for qualcomm PMI usbpd-phy
 * @mains_online: is AC/DC input connected
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
 * @prev_iin: Previous IIN ADC, uA
 * @prev_inc: Previous TA voltage or current increment factor
 * @req_new_iin: Request for new input current limit, true or false
 * @req_new_vfloat: Request for new vfloat, true or false
 * @req_new_dc_mode: Request for new dc mode, true or false
 * @new_iin: New request input current limit, uA
 * @new_vfloat: New request vfloat, uV
 * @new_dc_mode: New request dc mode, normal, 1:1 bypass, or 2:1 bypass mode
 * @adc_comp_gain: adc gain for compensation
 * @retry_cnt: retry counter for re-starting charging if charging stop happens
 * @ta_type: TA type for the direct charging, USBPD TA or Wireless Charger.
 * @ta_ctrl: TA control method for the direct charging, Current Limit mode or Constant Voltage mode.
 * @chg_mode: charging mode that TA can support for the direct charging, 2:1 or 4:1 mode
 * @fsw_cfg: Switching frequency, kHz
 * @req_sw_freq: Switching frequency change sequence.
 * @dec_vfloat: flag for vfloat decrement, true or false
 * @req_enable: flag for enable or disable charging by battery driver, true or false.
 * @enable: flag for enable status of pca9482, true or false.
 * @rev_mode: reverse mode, 1:2 switching, reverse 1:1, or stop reverse mode.
 * @iin_rev: vin_ocp current for 1:2 switching or reverse 1:1 mode
 * @prev_vbat: Previous VBAT_ADC in start cv and cv state, uV
 * @done_cnt: Charging done counter.
 * @pdata: pointer to platform data
 * @debug_root: debug entry
 * @debug_address: debug register address
 */
struct pca9482_charger {
	struct wakeup_source	*monitor_wake_lock;
	struct mutex		lock;
	struct mutex		i2c_lock;
	struct device		*dev;
	struct regmap		*regmap;
	struct power_supply	*mains;

	struct workqueue_struct *dc_wq;
	struct delayed_work timer_work;
	unsigned int		timer_id;
	unsigned long      	timer_period;

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
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int		fpdo_dc_iin_topoff;
	unsigned int		fpdo_dc_vnow_topoff;
	unsigned int		fpdo_dc_mainbat_topoff;
	unsigned int		fpdo_dc_subbat_topoff;
#endif
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
	unsigned int		fsw_cfg;
	unsigned int		req_sw_freq;

	bool				dec_vfloat;

	bool				req_enable;
	bool				enable;

	int					rev_mode;
	int					iin_rev;

	int					prev_vbat;

	int					done_cnt;

	struct pca9482_platform_data *pdata;

	/* debug */
	struct dentry		*debug_root;
	u32					debug_address;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int input_current;
	int charging_current;
	int float_voltage;
	int chg_status;
	int health_status;
	bool wdt_kick;

	int adc_val[ADC_READ_MAX];

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

#ifdef CONFIG_USBPD_PHY_QCOM
static int pca9482_usbpd_setup(struct pca9482_charger *pca9482);
#endif


/**************************************/
/* Switching charger control function */
/**************************************/
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int pca9482_send_pd_message(struct pca9482_charger *pca9482, unsigned int msg_type);

/*******************************/
/* Switching charger control function */
/*******************************/
char *charging_state_str[] = {
	"NO_CHARGING", "CHECK_VBAT", "PRESET_DC", "CHECK_ACTIVE", "ADJUST_CC",
	"START_CC", "CC_MODE", "START_CV", "CV_MODE", "CHARGING_DONE",
	"ADJUST_TAVOL", "ADJUST_TACUR", "BYPASS_MODE", "DCMODE_CHANGE", "REVERSE_MODE",
	"FPDO_CV_MODE",
};

static int pca9482_read_reg(struct pca9482_charger *pca9482, unsigned reg, void *val)
{
	int ret = 0;

	mutex_lock(&pca9482->i2c_lock);
	ret = regmap_read(pca9482->regmap, reg, val);
	mutex_unlock(&pca9482->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9482_bulk_read_reg(struct pca9482_charger *pca9482, int reg, void *val, int count)
{
	int ret = 0;

	mutex_lock(&pca9482->i2c_lock);
	ret = regmap_bulk_read(pca9482->regmap, reg, val, count);
	mutex_unlock(&pca9482->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9482_write_reg(struct pca9482_charger *pca9482, int reg, u8 val)
{
	int ret = 0;

	mutex_lock(&pca9482->i2c_lock);
	ret = regmap_write(pca9482->regmap, reg, val);
	mutex_unlock(&pca9482->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9482_update_reg(struct pca9482_charger *pca9482, int reg, u8 mask, u8 val)
{
	int ret = 0;

	mutex_lock(&pca9482->i2c_lock);
	ret = regmap_update_bits(pca9482->regmap, reg, mask, val);
	mutex_unlock(&pca9482->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9482_read_adc(struct pca9482_charger *pca9482, u8 adc_ch);

static int pca9482_set_charging_state(struct pca9482_charger *pca9482, unsigned int charging_state)
{
	union power_supply_propval value = {0,};
	static int prev_val = DC_STATE_NO_CHARGING;

	pca9482->charging_state = charging_state;

	switch (charging_state) {
	case DC_STATE_NO_CHARGING:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
		break;
	case DC_STATE_CHECK_VBAT:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
		break;
	case DC_STATE_PRESET_DC:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
		break;
	case DC_STATE_CHECK_ACTIVE:
	case DC_STATE_START_CC:
	case DC_STATE_START_CV:
	case DC_STATE_ADJUST_TAVOL:
	case DC_STATE_ADJUST_TACUR:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
		break;
	case DC_STATE_CC_MODE:
	case DC_STATE_CV_MODE:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON;
		break;
	case DC_STATE_CHARGING_DONE:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
		break;
	case DC_STATE_BYPASS_MODE:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_BYPASS;
		break;
	default:
		return -1;
	}

	if (prev_val == value.intval)
		return -1;

	prev_val = value.intval;
	psy_do_property(pca9482->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, value);

	return 0;
}

static void pca9482_init_adc_val(struct pca9482_charger *pca9482, int val)
{
	int i = 0;

	for (i = 0; i < ADC_READ_MAX; ++i)
		pca9482->adc_val[i] = val;
}

static void pca9482_test_read(struct pca9482_charger *pca9482)
{
	int address = 0;
	unsigned int val;
	char str[1024] = { 0, };

	for (address = PCA9482_REG_DEVICE_0_STS; address <= PCA9482_REG_ADC_READ_I_VBAT_CURRENT_1; address++) {
		pca9482_read_reg(pca9482, address, &val);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", address, val);
	}
	pr_info("%s : %s\n", __func__, str);
}

static void pca9482_monitor_work(struct pca9482_charger *pca9482)
{
	int ta_vol = pca9482->ta_vol / PCA9482_SEC_DENOM_U_M;
	int ta_cur = pca9482->ta_cur / PCA9482_SEC_DENOM_U_M;

	if (pca9482->charging_state == DC_STATE_NO_CHARGING)
		return;
	/* update adc value */
	pca9482_read_adc(pca9482, ADCCH_VIN);
	pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
	pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
	pca9482_read_adc(pca9482, ADCCH_DIE_TEMP);
	pr_info("%s: state(%s), iin_cc(%dmA), v_float(%dmV), vbat(%dmV), vin(%dmV), iin(%dmA), die_temp(%d), pps_requested(%d/%dmV/%dmA)", __func__,
		charging_state_str[pca9482->charging_state],
		pca9482->iin_cc / PCA9482_SEC_DENOM_U_M, pca9482->vfloat / PCA9482_SEC_DENOM_U_M,
		pca9482->adc_val[ADCCH_BATP_BATN], pca9482->adc_val[ADCCH_VIN],
		pca9482->adc_val[ADCCH_VIN_CURRENT], pca9482->adc_val[ADCCH_DIE_TEMP],
		pca9482->ta_objpos, ta_vol, ta_cur);
}

/**************************************/
/* Switching charger control function */
/**************************************/
/* This function needs some modification by a customer */
static void pca9482_set_wdt_enable(struct pca9482_charger *pca9482, bool enable)
{
	int ret;
	unsigned int val;

	val = enable << MASK2SHIFT(PCA9482_BIT_WATCHDOG_EN);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0,
			PCA9482_BIT_WATCHDOG_EN, val);
	pr_info("%s: set wdt enable = %d\n", __func__, enable);
}

static void pca9482_set_wdt_timer(struct pca9482_charger *pca9482, int time)
{
	int ret;
	unsigned int val;

	val = time << MASK2SHIFT(PCA9482_BIT_WATCHDOG_CFG);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0,
			PCA9482_BIT_WATCHDOG_CFG, val);
	pr_info("%s: set wdt time = %d\n", __func__, time);
}

static void pca9482_check_wdt_control(struct pca9482_charger *pca9482)
{
	struct device *dev = pca9482->dev;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (pca9482->wdt_kick) {
		pca9482_set_wdt_timer(pca9482, WDT_16SEC);
		schedule_delayed_work(&pca9482->wdt_control_work, msecs_to_jiffies(PCA9482_BATT_WDT_CONTROL_T));
	} else {
		pca9482_set_wdt_timer(pca9482, WDT_16SEC);
		if (client->addr == 0xff)
			client->addr = 0x57;
	}
}

static void pca9482_wdt_control_work(struct work_struct *work)
{
	struct pca9482_charger *pca9482 = container_of(work, struct pca9482_charger,
						 wdt_control_work.work);
	struct device *dev = pca9482->dev;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	int vin, iin;

	pca9482_set_wdt_timer(pca9482, WDT_4SEC);

	/* this is for kick watchdog */
	vin = pca9482_read_adc(pca9482, ADCCH_VIN);
	iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);

	pca9482_send_pd_message(pca9482, PD_MSG_REQUEST_APDO);

	client->addr = 0xff;

	pr_info("## %s: disable client addr (vin:%dmV, iin:%dmA)\n",
		__func__, vin / PCA9482_SEC_DENOM_U_M, iin / PCA9482_SEC_DENOM_U_M);
}

static void pca9482_set_done(struct pca9482_charger *pca9482, bool enable)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	value.intval = enable;
	psy_do_property(pca9482->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_DIRECT_DONE, value);

	if (ret < 0)
		pr_info("%s: error set_done, ret=%d\n", __func__, ret);
}

static void pca9482_set_switching_charger(struct pca9482_charger *pca9482, bool enable)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	value.intval = enable;
	psy_do_property(pca9482->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, value);

	if (ret < 0)
		pr_info("%s: error switching_charger, ret=%d\n", __func__, ret);
}
#else
static int pca9482_set_switching_charger(bool enable,
	unsigned int input_current, unsigned int charging_current, unsigned int vfloat)
{
	int ret;
	struct power_supply *psy_swcharger;
	union power_supply_propval val;

	pr_info("%s: enable=%d, iin=%d, ichg=%d, vfloat=%d\n",
		__func__, enable, input_current, charging_current, vfloat);

	/* Insert Code */

	/* Get power supply name */
#ifdef CONFIG_USBPD_PHY_QCOM
	psy_swcharger = power_supply_get_by_name("usb");
#else
	/* Change "sw-charger" to the customer's switching charger name */
	psy_swcharger = power_supply_get_by_name("sw-charger");
#endif
	if (psy_swcharger == NULL) {
		pr_err("%s: cannot get power_supply_name-usb\n", __func__);
		ret = -ENODEV;
		goto error;
	}

	if (enable == true)	{
		/* Set Switching charger */

		/* input current */
		val.intval = input_current;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
		if (ret < 0)
			goto error;
		/* charging current */
		val.intval = charging_current;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
		if (ret < 0)
			goto error;
		/* vfloat voltage */
		val.intval = vfloat;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
		if (ret < 0)
			goto error;

		/* it depends on customer's code to enable charger */
#ifdef CONFIG_USBPD_PHY_QCOM
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
#else
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_ONLINE, &val);
#endif
		if (ret < 0)
			goto error;
	} else {
		/* disable charger */
		/* it depends on customer's code to disable charger */
#ifdef CONFIG_USBPD_PHY_QCOM
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
#else
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_ONLINE, &val);
#endif
		if (ret < 0)
			goto error;

		/* input_current */
		val.intval = input_current;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
		if (ret < 0)
			goto error;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

static int pca9482_get_switching_charger_property(enum power_supply_property prop,
						  union power_supply_propval *val)
{
	int ret;
	struct power_supply *psy_swcharger;

	/* Get power supply name */
#ifdef CONFIG_USBPD_PHY_QCOM
	psy_swcharger = power_supply_get_by_name("usb");
#else
	psy_swcharger = power_supply_get_by_name("sw-charger");
#endif
	if (psy_swcharger == NULL) {
		ret = -EINVAL;
		goto error;
	}
	ret = power_supply_get_property(psy_swcharger, prop, val);

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}
#endif

/*******************/
/* Send PD message */
/*******************/
/* Send Request message to the source */
/* This function needs some modification by a customer */
static int pca9482_send_pd_message(struct pca9482_charger *pca9482, unsigned int msg_type)
{
#ifdef CONFIG_USBPD_PHY_QCOM
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int pdo_idx, pps_vol, pps_cur;
#else
	u8 msg_buf[4];	/* Data Buffer for raw PD message */
	unsigned int max_cur;
	unsigned int op_cur, out_vol;
#endif
	int ret = 0;

	pr_info("%s: ======Start========\n", __func__);

	/* Cancel pps request timer */
	cancel_delayed_work(&pca9482->pps_work);

	mutex_lock(&pca9482->lock);

	if (((pca9482->charging_state == DC_STATE_NO_CHARGING) &&
		(msg_type == PD_MSG_REQUEST_APDO)) ||
		(pca9482->mains_online == false)) {
		/* Vbus reset happened in the previous PD communication */
		goto out;
	}

#ifdef CONFIG_USBPD_PHY_QCOM
	/* check the phandle */
	if (pca9482->pd == NULL) {
		pr_info("%s: get phandle\n", __func__);
		ret = pca9482_usbpd_setup(pca9482);
		if (ret != 0) {
			dev_err(pca9482->dev, "Error usbpd setup!\n");
			pca9482->pd = NULL;
			goto out;
		}
	}
#endif

	/* Check whether requested TA voltage and current are in valid range or not */
	if ((msg_type == PD_MSG_REQUEST_APDO) &&
		(pca9482->dc_mode != PTM_1TO1) &&
		((pca9482->ta_vol < TA_MIN_VOL) || (pca9482->ta_cur < TA_MIN_CUR)))	{
		/* request TA voltage or current is less than minimum threshold */
		/* This is abnormal case, too low input voltage and current */
		/* Normally VIN_UVLO already happened */
		pr_err("%s: Abnormal low RDO, ta_vol=%d, ta_cur=%d\n", __func__, pca9482->ta_vol, pca9482->ta_cur);
		ret = -EINVAL;
		goto out;
	}

	pr_info("%s: msg_type=%d, ta_cur=%d, ta_vol=%d, ta_objpos=%d\n",
		__func__, msg_type, pca9482->ta_cur, pca9482->ta_vol, pca9482->ta_objpos);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pdo_idx = pca9482->ta_objpos;
	pps_vol = pca9482->ta_vol / PCA9482_SEC_DENOM_U_M;
	pps_cur = pca9482->ta_cur / PCA9482_SEC_DENOM_U_M;
	pr_info("## %s: msg_type=%d, pdo_idx=%d, pps_vol=%dmV(max_vol=%dmV), pps_cur=%dmA(max_cur=%dmA)\n",
		__func__, msg_type, pdo_idx,
		pps_vol, pca9482->pdo_max_voltage,
		pps_cur, pca9482->pdo_max_current);
#endif

	switch (msg_type) {
	case PD_MSG_REQUEST_APDO:
#ifdef CONFIG_USBPD_PHY_QCOM
		ret = usbpd_request_pdo(pca9482->pd, pca9482->ta_objpos, pca9482->ta_vol, pca9482->ta_cur);
		if (ret == -EBUSY) {
			/* wait 100ms */
			msleep(100);
			/* try again */
			ret = usbpd_request_pdo(pca9482->pd, pca9482->ta_objpos, pca9482->ta_vol, pca9482->ta_cur);
		}
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
		if (ret == -EBUSY) {
			pr_info("%s: request again ret=%d\n", __func__, ret);
			msleep(100);
			ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
		}
#else
		op_cur = pca9482->ta_cur/50000;		// Operating Current 50mA units
		out_vol = pca9482->ta_vol/20000;	// Output Voltage in 20mV units
		msg_buf[0] = op_cur & 0x7F;			// Operating Current 50mA units - B6...0
		msg_buf[1] = (out_vol<<1) & 0xFE;	// Output Voltage in 20mV units - B19..(B15)..B9
		msg_buf[2] = (out_vol>>7) & 0x0F;	// Output Voltage in 20mV units - B19..(B16)..B9,
		msg_buf[3] = pca9482->ta_objpos<<4;	// Object Position - B30...B28

		/* Send the PD message to CC/PD chip */
		/* Todo - insert code */
#endif
		/* Start pps request timer */
		if (ret == 0) {
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->pps_work,
								msecs_to_jiffies(PPS_PERIODIC_T));
		}
		break;

	case PD_MSG_REQUEST_FIXED_PDO:
#ifdef CONFIG_USBPD_PHY_QCOM
		ret = usbpd_request_pdo(pca9482->pd, pca9482->ta_objpos, pca9482->ta_vol, pca9482->ta_cur);
		if (ret == -EBUSY) {
			/* wait 100ms */
			msleep(100);
			/* try again */
			ret = usbpd_request_pdo(pca9482->pd, pca9482->ta_objpos, pca9482->ta_vol, pca9482->ta_cur);
		}
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (pca9482->ta_type == TA_TYPE_USBPD_20) {
			pr_err("%s: ta_type(%d)! skip pd_select_pps\n", __func__, pca9482->ta_type);
		} else {
			ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
			if (ret == -EBUSY) {
				pr_info("%s: request again ret=%d\n", __func__, ret);
				msleep(100);
				ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
			}
		}
#else
		max_cur = pca9482->ta_cur/10000;	// Maximum Operation Current 10mA units
		op_cur = max_cur;					// Operating Current 10mA units
		msg_buf[0] = max_cur & 0xFF;		// Maximum Operation Current -B9..(7)..0
		msg_buf[1] = ((max_cur>>8) & 0x03) | ((op_cur<<2) & 0xFC);	// Operating Current - B19..(15)..10
		msg_buf[2] = ((op_cur>>6) & 0x0F);	// Operating Current - B19..(16)..10, Unchunked Extended Messages Supported  - not support
		msg_buf[3] = pca9482->ta_objpos<<4; // Object Position - B30...B28

		/* Send the PD message to CC/PD chip */
		/* Todo - insert code */
#endif
		break;

	default:
		break;
	}

out:
	if (((pca9482->charging_state == DC_STATE_NO_CHARGING) &&
		(msg_type == PD_MSG_REQUEST_APDO)) ||
		(pca9482->mains_online == false)) {
		/* Even though PD communication success, Vbus reset might happen
			So, check the charging state again */
		ret = -EINVAL;
	}

	pr_info("%s: ret=%d\n", __func__, ret);
	mutex_unlock(&pca9482->lock);
	return ret;
}

/************************/
/* Get APDO max power   */
/************************/
/* Get the max current/voltage/power of APDO from the CC/PD driver */
/* This function needs some modification by a customer */
static int pca9482_get_apdo_max_power(struct pca9482_charger *pca9482)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int ta_max_vol_mv = (pca9482->ta_max_vol / PCA9482_SEC_DENOM_U_M);
	unsigned int ta_max_cur_ma = 0;
	unsigned int ta_max_pwr_uw = 0;
#endif

#ifdef CONFIG_USBPD_PHY_QCOM
	/* check the phandle */
	if (pca9482->pd == NULL) {
		pr_info("%s: get phandle\n", __func__);
		ret = pca9482_usbpd_setup(pca9482);
		if (ret != 0) {
			dev_err(pca9482->dev, "Error usbpd setup!\n");
			pca9482->pd = NULL;
			goto out;
		}
	}

	/* Put ta_max_vol to the desired ta maximum value, ex) 9800mV */
	/* Get new ta_max_vol and ta_max_cur, ta_max_power and proper object position by CC/PD IC */
	ret = usbpd_get_apdo_max_power(pca9482->pd, &pca9482->ta_objpos,
									&pca9482->ta_max_vol, &pca9482->ta_max_cur, &pca9482->ta_max_pwr);
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ret = sec_pd_get_apdo_max_power(&pca9482->ta_objpos,
				&ta_max_vol_mv, &ta_max_cur_ma, &ta_max_pwr_uw);
	/* mA,mV,uW --> uA,uV,uW */
	pca9482->ta_max_vol = ta_max_vol_mv * PCA9482_SEC_DENOM_U_M;
	pca9482->ta_max_cur = ta_max_cur_ma * PCA9482_SEC_DENOM_U_M;
	pca9482->ta_max_pwr = ta_max_pwr_uw;

	pr_info("%s: ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d\n",
		__func__, pca9482->ta_max_vol, pca9482->ta_max_cur, pca9482->ta_max_pwr);

	pca9482->pdo_index = pca9482->ta_objpos;
	pca9482->pdo_max_voltage = ta_max_vol_mv;
	pca9482->pdo_max_current = ta_max_cur_ma;
#else
	/* Need to implement by a customer */
	/* Get new ta_max_vol and ta_max_cur, ta_max_power and proper object position by CC/PD IC */
#endif

#ifdef CONFIG_USBPD_PHY_QCOM
out:
#endif
	return ret;
}


/******************/
/* Set RX voltage */
/******************/
/* Send RX voltage to RX IC */
/* This function needs some modification by a customer */
static int pca9482_send_rx_voltage(struct pca9482_charger *pca9482, unsigned int msg_type)
{
	struct power_supply *psy;
	union power_supply_propval pro_val;
	int ret = 0;

	mutex_lock(&pca9482->lock);

	if (pca9482->mains_online == false) {
		/* Vbus reset happened in the previous PD communication */
		goto out;
	}

	pr_info("%s: rx_vol=%d\n", __func__, pca9482->ta_vol);

	/* Need to implement send RX voltage to wireless RX IC */

	/* The below code should be modified by the customer */
	/* Get power supply name */
	psy = power_supply_get_by_name("wireless");
	if (!psy) {
		dev_err(pca9482->dev, "Cannot find wireless power supply\n");
		ret = -ENODEV;
		goto out;
	}

	/* Set the RX voltage */
	pro_val.intval = pca9482->ta_vol;

	/* Set the property */
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pro_val);
	power_supply_put(psy);
	if (ret < 0) {
		dev_err(pca9482->dev, "Cannot set RX voltage\n");
		goto out;
	}

out:
	if (pca9482->mains_online == false) {
		/* Even though PD communication success, Vbus reset might happen
			So, check the charging state again */
		ret = -EINVAL;
	}

	pr_info("%s: ret=%d\n", __func__, ret);
	mutex_unlock(&pca9482->lock);
	return ret;
}


/************************/
/* Get RX max power     */
/************************/
/* Get the max current/voltage/power of RXIC from the WCRX driver */
/* This function needs some modification by a customer */
static int pca9482_get_rx_max_power(struct pca9482_charger *pca9482)
{
	struct power_supply *psy;
	union power_supply_propval pro_val;
	int ret = 0;

	/* insert code */

	/* Get power supply name */
	psy = power_supply_get_by_name("wireless");
	if (!psy) {
		dev_err(pca9482->dev, "Cannot find wireless power supply\n");
		return -ENODEV;
	}

	/* Get the maximum voltage */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &pro_val);
	if (ret < 0) {
		dev_err(pca9482->dev, "Cannot get the maximum RX voltage\n");
		goto error;
	}

	if (pca9482->ta_max_vol > pro_val.intval) {
		/* RX IC cannot support the request maximum voltage */
		ret = -EINVAL;
		goto error;
	} else {
		pca9482->ta_max_vol = pro_val.intval;
	}

	/* Get the maximum current */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &pro_val);
	if (ret < 0) {
		dev_err(pca9482->dev, "Cannot get the maximum RX current\n");
		goto error;
	}

	pca9482->ta_max_cur = pro_val.intval;
	pca9482->ta_max_pwr = (pca9482->ta_max_vol/1000)*(pca9482->ta_max_cur/1000);

error:
	power_supply_put(psy);
	return ret;
}

/**************************/
/* PCA9482 Local function */
/**************************/
/* ADC Read function */
static int pca9482_read_adc(struct pca9482_charger *pca9482, u8 adc_ch)
{
	u8 reg_data[2];
	u16 raw_adc = 0;	// raw ADC value
	int conv_adc;	// conversion ADC value
	int ret;
	union power_supply_propval pval;

	switch (adc_ch) {
	case ADCCH_VIN:
		// Read ADC value
		ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_VIN_0 | PCA9482_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * VIN_STEP;
		break;
	case ADCCH_RESERVED:
		// Reserved ADC value
		conv_adc = 0;
		raw_adc = 0;
		break;
	case ADCCH_OVP_OUT:
		// Read ADC value
		ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_OVP_OUT_0 | PCA9482_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * OVP_OUT_STEP;
		break;
	case ADCCH_BATP_BATN:
		// Read ADC value
		ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_BATP_BATN_0 | PCA9482_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * BATP_BATN_STEP;
		pr_info("%s: pca9482 vbatt_adc, convert_val=%d\n", __func__, conv_adc);

		/* Check VFLOAT method */
		if (pca9482->pdata->fg_vfloat == true) {
			/* Use FG Vnow for VFLOAT */
			// Read fuelgauge ADC
			ret = psy_do_property(pca9482->pdata->fg_name, get,
						POWER_SUPPLY_PROP_VOLTAGE_NOW, pval);
			if (ret < 0) {
				conv_adc = ret;
				goto error;
			}
			conv_adc = pval.intval * DENOM_U_M;
			pr_info("%s: pca9482 fg_vnow, convert_val=%d\n", __func__, conv_adc);
		}
		break;
	case ADCCH_VOUT:
		// Read ADC value
		ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_VOUT_0 | PCA9482_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * VOUT_STEP;
		break;
	case ADCCH_NTC:
		// Read ADC value
		ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_NTC_0 | PCA9482_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * NTC_STEP;
		break;
	case ADCCH_DIE_TEMP:
		// Read ADC value
		ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_DIE_TEMP_0 | PCA9482_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * DIE_TEMP_STEP / DIE_TEMP_DENOM;	// Temp, unit - C
		if (conv_adc > DIE_TEMP_MAX)
			conv_adc = DIE_TEMP_MAX;
		break;
	case ADCCH_VIN_CURRENT:
		// Read ADC value
		ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_VIN_CURRENT_0 | PCA9482_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * VIN_CURRENT_STEP;
		break;
	case ADCCH_BAT_CURRENT:
		// Read ADC value
		ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_I_VBAT_CURRENT_0 | PCA9482_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * BAT_CURRENT_STEP;
		break;
	default:
		conv_adc = -EINVAL;
		break;
	}

error:
	pr_info("%s: adc_ch=%d, raw_adc=0x%x, convert_val=%d\n", __func__, adc_ch, raw_adc, conv_adc);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (adc_ch == ADCCH_DIE_TEMP)
		pca9482->adc_val[adc_ch] = conv_adc;
	else
		pca9482->adc_val[adc_ch] = conv_adc / PCA9482_SEC_DENOM_U_M;
#endif
	return conv_adc;
}

/* PCA9482 VBAT_ADC Read function */
static int pca9482_read_vbat_adc(struct pca9482_charger *pca9482)
{
	u8 reg_data[2];
	u16 raw_adc;	// raw ADC value
	int conv_adc;	// conversion ADC value
	int ret;

	// Read ADC value
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_BATP_BATN_0 | PCA9482_REG_BIT_AI, reg_data, 2);
	if (ret < 0) {
		pr_info("%s: Error - ret=%d\n", __func__, ret);
		conv_adc = ret;
	} else {
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9482_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * BATP_BATN_STEP;
		pr_info("%s: pca9482 vbatt_adc, raw_adc=0x%x, convert_val=%d\n", __func__, raw_adc, conv_adc);
	}

	return conv_adc;
}

static int pca9482_set_vfloat(struct pca9482_charger *pca9482, unsigned int vfloat)
{
	int ret, val;

	pr_info("%s: vfloat=%d\n", __func__, vfloat);

	/* float voltage - battery regulation voltage */

	/* maximum value is 5V */
	if (vfloat > PCA9482_VBAT_REG_MAX)
		vfloat = PCA9482_VBAT_REG_MAX;
	/* minimum value is 3.725V */
	if (vfloat < PCA9482_VBAT_REG_MIN)
		vfloat = PCA9482_VBAT_REG_MIN;

	val = PCA9482_VBAT_REG(vfloat);
	ret = pca9482_write_reg(pca9482, PCA9482_REG_CHARGING_CNTL_2, val);
	return ret;
}

static int pca9482_set_charging_current(struct pca9482_charger *pca9482, unsigned int ichg)
{
	int ret, val;

	pr_info("%s: ichg=%d\n", __func__, ichg);

	/* charging current - battery regulation current */

	/* round-up charging current with ibat regulation resolution */
	if (ichg % PCA9482_IBAT_REG_STEP)
		ichg = ichg + PCA9482_IBAT_REG_STEP;

	/* Add 100mA offset to avoid frequent battery current regulation */
	ichg = ichg + PCA9482_IBAT_REG_OFFSET;

	/* maximum value is 7A */
	if (ichg > PCA9482_IBAT_REG_MAX)
		ichg = PCA9482_IBAT_REG_MAX;
	/* minimum value is 1A */
	if (ichg < PCA9482_IBAT_REG_MIN)
		ichg = PCA9482_IBAT_REG_MIN;

	val = PCA9482_IBAT_REG(ichg);
	ret = pca9482_write_reg(pca9482, PCA9482_REG_CHARGING_CNTL_3, val);
	return ret;
}

static int pca9482_set_input_current(struct pca9482_charger *pca9482, unsigned int iin)
{
	int ret, val;

	pr_info("%s: iin=%d\n", __func__, iin);

	/* input current - input regulation current */

	/* round-up input current with input regulation resolution */
	if (iin % PCA9482_IIN_REG_STEP)
		iin = iin + PCA9482_IIN_REG_STEP;

	/* Check TA type */
	if (pca9482->ta_type == TA_TYPE_USBPD_20) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (iin < pca9482->pdata->fpdo_dc_iin_lowest_limit) {
			pr_info("%s: IIN LOWEST LIMIT! IIN %d -> %d\n", __func__,
					iin, pca9482->pdata->fpdo_dc_iin_lowest_limit);
			iin = pca9482->pdata->fpdo_dc_iin_lowest_limit;
		}
#endif
		/* Apply FPDO offset */
		iin = iin + PCA9482_IIN_REG_OFFSET_FPDO;
	} else {
		/* Add iin offset for input current regulation */
		iin = iin + PCA9482_IIN_REG_OFFSET;
	}
	/* maximum value is 3.5A */
	if (iin > PCA9482_IIN_REG_MAX)
		iin = PCA9482_IIN_REG_MAX;
	/* minimum value is 500mA */
	if (iin < PCA9482_IIN_REG_MIN)
		iin = PCA9482_IIN_REG_MIN;

	val = PCA9482_IIN_REG(iin);
	ret = pca9482_write_reg(pca9482, PCA9482_REG_CHARGING_CNTL_1, val);

	pr_info("%s: real iin_cfg=%d\n", __func__, val*PCA9482_IIN_REG_STEP + PCA9482_IIN_REG_MIN);
	return ret;
}

static int pca9482_set_charging(struct pca9482_charger *pca9482, bool enable)
{
	int ret, val;
	u8 sc_op_reg;

	pr_info("%s: enable=%d\n", __func__, enable);

	if (pca9482->dc_mode == PTM_1TO1) {
		/* Forward 1:1 mode */
		sc_op_reg = PCA9482_SC_OP_F_11;
	} else {
		/* 2:1 Switching mode */
		sc_op_reg = PCA9482_SC_OP_21;

		/* Disable HALF_VIN_OVP_EN */
		val = 0;
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_1,
								PCA9482_BIT_HALF_VIN_OVP_EN, val);
	}

	/* Check device's current status */
	/* Read DEVICE_3_STS register */
	ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_3_STS, &val);
	if (ret < 0)
		goto error;
	pr_info("%s: power state = 0x%x\n", __func__, val);

	if (enable == true) {
		/* Set NTC Protection */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0,
								PCA9482_BIT_STANDBY_BY_NTC_EN,
								pca9482->pdata->ntc_en << MASK2SHIFT(PCA9482_BIT_STANDBY_BY_NTC_EN));

		/* Set STANDBY_EN bit to 1 */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_3,
								PCA9482_BIT_STANDBY_EN | PCA9482_BIT_SC_OPERATION_MODE,
								PCA9482_STANDBY_FORCE | sc_op_reg);

		/* Set EN_CFG to active low */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_2,
								PCA9482_BIT_EN_CFG, PCA9482_EN_ACTIVE_L);

		/* Set STANDBY_EN bit to 0 */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_3,
								PCA9482_BIT_STANDBY_EN | PCA9482_BIT_SC_OPERATION_MODE,
								PCA9482_STANDBY_DONOT | sc_op_reg);

		/* Set enable flag to true */
		pca9482->enable = true;
	} else {
		/* Disable NTC Protection */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0,
								PCA9482_BIT_STANDBY_BY_NTC_EN, 0);

		/* Set EN_CFG to active high - Disable PCA9482 */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_2,
								PCA9482_BIT_EN_CFG, PCA9482_EN_ACTIVE_H);

		/* Set enable flag to false */
		pca9482->enable = false;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		/* wait delay time for prevent Reverse Current issues */
		msleep(200);
#endif
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

static int pca9482_set_vin_ocp(struct pca9482_charger *pca9482, unsigned int vin_ocp)
{
	int ret, val;

	pr_info("%s: vin_ocp=%d\n", __func__, vin_ocp);

	/* maximum value is 2A */
	if (vin_ocp > PCA9482_VIN_OCP_12_11_MAX)
		vin_ocp = PCA9482_VIN_OCP_12_11_MAX;
	/* minimum value is 500mA */
	if (vin_ocp < PCA9482_VIN_OCP_12_11_MIN)
		vin_ocp = PCA9482_VIN_OCP_12_11_MIN;

	/* Set VIN OCP current */
	val = PCA9482_VIN_OCP_12_11(vin_ocp);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_5,
							PCA9482_BIT_VIN_OCP_CURRENT_12_11,
							val << MASK2SHIFT(PCA9482_BIT_VIN_OCP_CURRENT_12_11));
	return ret;
}

static int pca9482_set_reverse_mode(struct pca9482_charger *pca9482, bool enable)
{
	int ret, val;
	u8 sc_op_reg;

	pr_info("%s: enable=%d\n", __func__, enable);

	if (pca9482->rev_mode == POWER_SUPPLY_DC_REVERSE_1TO2) {
		/* 1:2 switching mode */
		sc_op_reg = PCA9482_SC_OP_12;
		/* Disable HALF_VIN_OVP_EN */
		val = 0;
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_1,
								PCA9482_BIT_HALF_VIN_OVP_EN, val);
	} else {
		/* Reverse 1:1 bypass mode */
		sc_op_reg = PCA9482_SC_OP_R_11;
	}

	/* Check device's current status */
	/* Read DEVICE_3_STS register */
	ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_3_STS, &val);
	if (ret < 0)
		goto error;
	pr_info("%s: power state = 0x%x\n", __func__, val);

	if (enable == true) {
		/* Enable VIN_OCP_12_11_EN */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_5,
								PCA9482_BIT_VIN_OCP_12_11_EN, PCA9482_BIT_VIN_OCP_12_11_EN);

		/* Set NTC Protection */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0,
								PCA9482_BIT_STANDBY_BY_NTC_EN,
								pca9482->pdata->ntc_en << MASK2SHIFT(PCA9482_BIT_STANDBY_BY_NTC_EN));

		/* Set STANDBY_EN bit to 0 */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_3,
								PCA9482_BIT_STANDBY_EN | PCA9482_BIT_SC_OPERATION_MODE,
								PCA9482_STANDBY_DONOT | sc_op_reg);

		/* Set STANDBY_EN bit to 1 */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_3,
								PCA9482_BIT_STANDBY_EN | PCA9482_BIT_SC_OPERATION_MODE,
								PCA9482_STANDBY_FORCE | sc_op_reg);

		/* Set EN_CFG to active low */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_2,
								PCA9482_BIT_EN_CFG, PCA9482_EN_ACTIVE_L);

		/* Set STANDBY_EN bit to 0 */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_3,
								PCA9482_BIT_STANDBY_EN | PCA9482_BIT_SC_OPERATION_MODE,
								PCA9482_STANDBY_DONOT | sc_op_reg);

		/* Set enable flag to true */
		pca9482->enable = true;
	} else {
		/* Disable NTC Protection */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0,
								PCA9482_BIT_STANDBY_BY_NTC_EN, 0);

		/* Disable VIN_OCP_12_11_EN */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_5,
								PCA9482_BIT_VIN_OCP_12_11_EN, 0);

		/* Set EN_CFG to active high - Disable PCA9482 */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_2,
								PCA9482_BIT_EN_CFG, PCA9482_EN_ACTIVE_H);

		/* Set enable flag to false */
		pca9482->enable = false;
	}

	/* Read DEVICE_3_STS register */
	ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_3_STS, &val);
	if (ret < 0)
		goto error;
	pr_info("%s: power state after setting = 0x%x\n", __func__, val);

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

static int pca9482_softreset(struct pca9482_charger *pca9482)
{
	int ret, val;
	u8 reg_val[10]; /* Dump for control register */

	pr_info("%s: do soft reset\n", __func__);


	/* Check the current register before softreset */
	pr_info("%s: Before softreset\n", __func__);

	/* Read all status registers for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_DEVICE_0_STS | PCA9482_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: status reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read all control registers for debugging */
	/* Read device, auto restart, and rcp control registers for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0 | PCA9482_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x16]=0x%x,[0x17]=0x%x,[0x18]=0x%x,[0x19]=0x%x,[0x1A]=0x%x,[0x1B]=0x%x,[0x1C]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read charging control registers for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_CHARGING_CNTL_0 | PCA9482_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x1D]=0x%x,[0x1E]=0x%x,[0x1F]=0x%x,[0x20]=0x%x,[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x\n",
		__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read ntc, sc, and adc control registers for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_NTC_0_CNTL | PCA9482_REG_BIT_AI,
							&reg_val[0], 9);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x24]=0x%x,[0x25]=0x%x,[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5]);
	pr_info("%s: control reg[0x2A]=0x%x,[0x2B]=0x%x,[0x2C]=0x%x\n",
			__func__, reg_val[6], reg_val[7], reg_val[8]);

	/* Do softreset */

	/* Set softreset register */
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0,
							PCA9482_BIT_SOFT_RESET, PCA9482_BIT_SOFT_RESET);

	/* Wait 5ms */
	usleep_range(5000, 6000);

	/* Reset PCA9482 and all regsiters values go to POR values */
	/* Check the current register after softreset */
	/* Read all control registers for debugging */
	pr_info("%s: After softreset\n", __func__);

	/* Read all status registers for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_DEVICE_0_STS | PCA9482_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: status reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read all control registers for debugging */
	/* Read device, auto restart, and rcp control registers for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0 | PCA9482_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x16]=0x%x,[0x17]=0x%x,[0x18]=0x%x,[0x19]=0x%x,[0x1A]=0x%x,[0x1B]=0x%x,[0x1C]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read charging control registers for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_CHARGING_CNTL_0 | PCA9482_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x1D]=0x%x,[0x1E]=0x%x,[0x1F]=0x%x,[0x20]=0x%x,[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x\n",
		__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read ntc, sc, and adc control registers for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_NTC_0_CNTL | PCA9482_REG_BIT_AI,
							&reg_val[0], 9);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x24]=0x%x,[0x25]=0x%x,[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5]);
	pr_info("%s: control reg[0x2A]=0x%x,[0x2B]=0x%x,[0x2C]=0x%x\n",
			__func__, reg_val[6], reg_val[7], reg_val[8]);

	/* Set the initial register value */

	/* Set VIN_CURRENT_OCP_21_11 to 1000mA */
	val = PCA9482_BIT_VIN_CURRENT_OCP_21_11;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_0,
							PCA9482_BIT_VIN_CURRENT_OCP_21_11, val);
	if (ret < 0)
		goto error;

	/* Set Reverse Current Detection */
	val = PCA9482_BIT_RCP_EN;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_RCP_CNTL,
							PCA9482_BIT_RCP_EN, val);
	if (ret < 0)
		goto error;

	/* VIN_OVP 10.8V */
	/* Die Temperature regulation 120'C */
	val = (0x3 << MASK2SHIFT(PCA9482_BIT_VIN_OVP_CFG)) | (0x3 << MASK2SHIFT(PCA9482_BIT_THERMAL_REGULATION_CFG));
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_1,
							PCA9482_BIT_VIN_OVP_CFG | PCA9482_BIT_THERMAL_REGULATION_CFG | PCA9482_BIT_THERMAL_REGULATION_EN,
							val | PCA9482_BIT_THERMAL_REGULATION_EN);
	if (ret < 0)
		goto error;

	/* Set external sense resistor value */
	val = pca9482->pdata->snsres << MASK2SHIFT(PCA9482_BIT_IBAT_SENSE_R_SEL);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_4,
							PCA9482_BIT_IBAT_SENSE_R_SEL, val);
	if (ret < 0)
		goto error;

	/* Set external sense resistor location */
	val = pca9482->pdata->snsres_cfg << MASK2SHIFT(PCA9482_BIT_IBAT_SENSE_R_CFG);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_2,
							PCA9482_BIT_IBAT_SENSE_R_CFG, val);
	if (ret < 0)
		goto error;

	/* Disable battery charge current regulation loop */
	/* Disable current measurement through CSP and CSN */
	val = 0;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_0,
							PCA9482_BIT_CSP_CSN_MEASURE_EN | PCA9482_BIT_I_VBAT_LOOP_EN,
							val);
	if (ret < 0)
		goto error;

	/* Set the ADC channel */
	val = (PCA9482_BIT_ADC_READ_VIN_CURRENT_EN |	/* IIN ADC */
		   PCA9482_BIT_ADC_READ_DIE_TEMP_EN |		/* DIE_TEMP ADC */
		   PCA9482_BIT_ADC_READ_NTC_EN |			/* NTC ADC */
		   PCA9482_BIT_ADC_READ_VOUT_EN |			/* VOUT ADC */
		   PCA9482_BIT_ADC_READ_BATP_BATN_EN |		/* VBAT ADC */
		   PCA9482_BIT_ADC_READ_OVP_OUT_EN |		/* OVP_OUT ADC */
		   PCA9482_BIT_ADC_READ_VIN_EN);			/* VIN ADC */

	ret = pca9482_write_reg(pca9482, PCA9482_REG_ADC_EN_CNTL_0, val);
	if (ret < 0)
		goto error;

	/* Enable ADC */
	val = PCA9482_BIT_ADC_EN | (ADC_AVG_16sample << MASK2SHIFT(PCA9482_BIT_ADC_AVERAGE_TIMES));
	ret = pca9482_write_reg(pca9482, PCA9482_REG_ADC_CNTL, val);
	if (ret < 0)
		goto error;

	/*
	 * Configure the Mask Register for interrupts: disable all interrupts by default.
	 */
	reg_val[REG_DEVICE_0] = 0xFF;
	reg_val[REG_DEVICE_1] = 0xFF;
	reg_val[REG_DEVICE_2] = 0xFF;
	reg_val[REG_DEVICE_3] = 0xFF;
	reg_val[REG_CHARGING] = 0xFF;
	reg_val[REG_SC_0] = 0xFF;
	reg_val[REG_SC_1] = 0xFF;

	ret = regmap_bulk_write(pca9482->regmap, PCA9482_REG_INT_DEVICE_0_MASK | PCA9482_REG_BIT_AI,
							&reg_val[0], REG_BUFFER_MAX);
	if (ret < 0)
		goto error;

	return 0;

error:
	pr_info("%s: i2c error, ret=%d\n", __func__, ret);
	return ret;
}

/* Read all registers for debugging */
static int pca9482_show_debug(struct pca9482_charger *pca9482)
{
	int rt = 0;
	u8 val[REG_BUFFER_MAX]; /* Read all status register for debugging */
	u8 deg_val[10]; /* Dump for control register */

	/* Read all interrupt registers for debugging */
	rt = regmap_bulk_read(pca9482->regmap, PCA9482_REG_INT_DEVICE_0 | PCA9482_REG_BIT_AI,
							&val[REG_DEVICE_0], REG_BUFFER_MAX);
	if (rt < 0)
		return rt;
	pr_err("%s: interrupt reg[0x01]=0x%x,[0x02]=0x%x,[0x03]=0x%x,[0x04]=0x%x,[0x05]=0x%x,[0x06]=0x%x,[0x07]=0x%x\n",
			__func__, val[0], val[1], val[2], val[3], val[4], val[5], val[6]);

	/* Read all control registers for debugging */
	/* Read device, auto restart, and rcp control registers for debugging */
	rt = regmap_bulk_read(pca9482->regmap, PCA9482_REG_DEVICE_CNTL_0 | PCA9482_REG_BIT_AI,
							&deg_val[0], 7);
	if (rt < 0)
		return rt;
	pr_info("%s: control reg[0x16]=0x%x,[0x17]=0x%x,[0x18]=0x%x,[0x19]=0x%x,[0x1A]=0x%x,[0x1B]=0x%x,[0x1C]=0x%x\n",
			__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5], deg_val[6]);

	/* Read charging control registers for debugging */
	rt = regmap_bulk_read(pca9482->regmap, PCA9482_REG_CHARGING_CNTL_0 | PCA9482_REG_BIT_AI,
							&deg_val[0], 7);
	if (rt < 0)
		return rt;
	pr_info("%s: control reg[0x1D]=0x%x,[0x1E]=0x%x,[0x1F]=0x%x,[0x20]=0x%x,[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x\n",
		__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5], deg_val[6]);

	/* Read ntc, sc, and adc control registers for debugging */
	rt = regmap_bulk_read(pca9482->regmap, PCA9482_REG_NTC_0_CNTL | PCA9482_REG_BIT_AI,
							&deg_val[0], 9);
	if (rt < 0)
		return rt;
	pr_info("%s: control reg[0x24]=0x%x,[0x25]=0x%x,[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x\n",
			__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
	pr_info("%s: control reg[0x2A]=0x%x,[0x2B]=0x%x,[0x2C]=0x%x\n",
			__func__, deg_val[6], deg_val[7], deg_val[8]);

	/* Read ADC register for debugging */
	rt = regmap_bulk_read(pca9482->regmap, PCA9482_REG_ADC_READ_VIN_0 | PCA9482_REG_BIT_AI,
							&deg_val[0], 10);
	if (rt < 0)
		return rt;
	pr_info("%s: adc reg[0x2D]=0x%x,[0x2E]=0x%x,[0x2F]=0x%x,[0x30]=0x%x,[0x31]=0x%x,[0x32]=0x%x\n",
			__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
	pr_info("%s: adc reg[0x33]=0x%x,[0x34]=0x%x,[0x35]=0x%x,[0x36]=0x%x\n",
			__func__, deg_val[6], deg_val[7], deg_val[8], deg_val[9]);

	rt = regmap_bulk_read(pca9482->regmap, PCA9482_REG_ADC_READ_NTC_0 | PCA9482_REG_BIT_AI,
							&deg_val[0], 8);
	if (rt < 0)
		return rt;
	pr_info("%s: adc reg[0x37]=0x%x,[0x38]=0x%x,[0x39]=0x%x,[0x3A]=0x%x,[0x3B]=0x%x,[0x3C]=0x%x\n",
			__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
	pr_info("%s: adc reg[0x3D]=0x%x,[0x3E]=0x%x\n", __func__, deg_val[6], deg_val[7]);

	return rt;
}

/* Check Active status */
static int pca9482_check_error(struct pca9482_charger *pca9482)
{
	int ret;
	unsigned int reg_val;
	int vbatt;
	int rt;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482->chg_status = POWER_SUPPLY_STATUS_CHARGING;
	pca9482->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif

	/* Read DEVICE_3_STS register */
	ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_3_STS, &reg_val);
	if (ret < 0)
		goto error;

	/* Read VBAT_ADC */
	vbatt = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

	/* Check Active status */
	if ((reg_val & PCA9482_BIT_STATUS_CHANGE) == PCA9482_21SW_F11_MODE) {
		/* PCA9482 is in 2:1 switching mode */
		/* Check whether the battery voltage is over the minimum voltage level or not */
		if (vbatt > DC_VBAT_MIN) {
			/* Normal charging battery level */
			/* Check temperature regulation loop */
			/* Read DEVICE_2_STS register */
			ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_2_STS, &reg_val);
			if (reg_val & PCA9482_BIT_THEM_REG) {
				/* Thermal regulation happened */
				pr_err("%s: Device is in temperature regulation\n", __func__);
				ret = -EINVAL;
				/* Read all registers for debugging */
				rt = pca9482_show_debug(pca9482);
				if (rt < 0) {
					pr_info("%s: i2c error, rt=%d\n", __func__, rt);
					goto error;
				}
			} else {
				/* Normal temperature */
				ret = 0;
			}
		} else {
			/* Abnormal battery level */
			pr_err("%s: Error abnormal battery voltage=%d\n", __func__, vbatt);
			ret = -EINVAL;
		}
	} else {
		/* PCA9482 is not in 2:1 switching mode - standby or shutdown state */
		/* Stop charging in timer_work */

		/* Read all status register for debugging */
		u8 val[REG_BUFFER_MAX];

		rt = pca9482_bulk_read_reg(pca9482, PCA9482_REG_DEVICE_0_STS | PCA9482_REG_BIT_AI,
								&val[REG_DEVICE_0], REG_BUFFER_MAX);
		if (rt < 0)
			goto error;
		pr_err("%s: Error reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
				__func__, val[0], val[1], val[2], val[3], val[4], val[5], val[6]);

		/* Check status register */
		if ((val[REG_DEVICE_0] & PCA9482_BIT_VIN_VALID) != PCA9482_BIT_VIN_VALID) {
			/* Invalid VIN or VOUT */
			if (val[REG_DEVICE_0] & PCA9482_BIT_VOUT_MAX_OV) {
				pr_err("%s: VOUT MAX_OV\n", __func__);		/* VOUT > VOUT_MAX */
				ret = -EINVAL;
			} else if (val[REG_DEVICE_0] & PCA9482_BIT_RCP_DETECTED) {
				pr_err("%s: RCP_DETECTED\n", __func__);		/* Reverse Current Protection */
				ret = -EINVAL;
			} else if (val[REG_DEVICE_0] & PCA9482_BIT_VIN_UNPLUG) {
				pr_err("%s: VIN_UNPLUG\n", __func__);			/* VIN < VIN_UNPLUG */
				ret = -EINVAL;
			} else if (val[REG_DEVICE_0] & PCA9482_BIT_VIN_OVP) {
				pr_err("%s: VIN_OVP\n", __func__);			/* VIN > VIN_OVP */
				ret = -EINVAL;
			} else if (val[REG_DEVICE_0] & PCA9482_BIT_VIN_OV_TRACKING) {
				pr_err("%s: VIN_OV_TRACKING\n", __func__);	/* VIN > VIN_OV_TRACKING */
				ret = -EAGAIN;
			} else if (val[REG_DEVICE_0] & PCA9482_BIT_VIN_UV_TRACKING) {
				pr_err("%s: VIN_UV_TRACKING\n", __func__);	/* VIN < VIN_UV_TRACKING */
				ret = -EINVAL;
			} else {
				pr_err("%s: Invalid VIN or VOUT\n", __func__);
				ret = -EINVAL;
			}
		} else if (val[REG_DEVICE_0] & PCA9482_BIT_RCP_DETECTED) {
			/* Valid VIN and RCP happens - ignore it */
			/* If Vin increase, PCA9482 will exit RCP condition */
			pr_err("%s: RCP_DETECTED\n", __func__);		/* Reverse Current Protection */
			/* Check charging state - Only retry in check active state */
			if (pca9482->charging_state == DC_STATE_CHECK_ACTIVE)
				ret = -EAGAIN;
			else
				ret = -ERROR_DCRCP;
		} else if (val[REG_DEVICE_1] & PCA9482_BIT_SINK_RCP_ENABLED) {
			/* Valid VIN and RCP happens - ignore it */
			/* If Vin increase, PCA9482 will exit RCP condition */
			pr_err("%s: SINK_RCP_ENABLED\n", __func__);	/* Isink_rcp Enabled */
			/* Check charging state - Only retry in check active state */
			if (pca9482->charging_state == DC_STATE_CHECK_ACTIVE)
				ret = -EAGAIN;
			else
				ret = -ERROR_DCRCP;
		} else if (val[REG_DEVICE_1] & PCA9482_BIT_SINK_RCP_TIMEOUT) {
			/* Valid VIN and RCP happens - ignore it */
			/* If Vin increase, PCA9482 will exit RCP condition */
			pr_err("%s: SINK_RCP_TIMEOUT\n", __func__);	/* discharge timer expired */
			/* Check charging state - Only retry in check active state */
			if (pca9482->charging_state == DC_STATE_CHECK_ACTIVE)
				ret = -EAGAIN;
			else
				ret = -ERROR_DCRCP;
		} else if (val[REG_DEVICE_1] & (PCA9482_BIT_NTC_0_DETECTED | PCA9482_BIT_NTC_1_DETECTED)) {
			if (pca9482->pdata->ntc_en == 0) {
				/* NTC protection function is disabled */
				/* ignore it */
				pr_err("%s: NTC_THRESHOLD - retry\n", __func__);	/* NTC_THRESHOLD */
				ret = -EAGAIN;
			} else {
				pr_err("%s: NTC_THRESHOLD\n", __func__);	/* NTC_THRESHOLD */
				ret = -EINVAL;
			}
		} else if (val[REG_DEVICE_1] & PCA9482_BIT_VIN_OCP_21_11) {
			pr_err("%s: VIN_OCP_21_11\n", __func__);	/* VIN_OCP */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_2] & PCA9482_BIT_THEM_REG) {
			pr_err("%s: THEM_REGULATION\n", __func__);	/* Thermal Regulation */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_2] & PCA9482_BIT_THSD) {
			pr_err("%s: THERMAL_SHUTDOWN\n", __func__);	/* Thermal Shutdown */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_2] & PCA9482_BIT_WATCHDOG_TIMER_OUT) {
			pr_err("%s: WATCHDOG_TIMER_OUT\n", __func__);	/* Watchdog timer out */
			ret = -EINVAL;
		} else if (val[REG_CHARGING] & PCA9482_BIT_CHARGER_SAFETY_TIMER) {
			pr_err("%s: CHARGER_SAFETY_TIMER_OUT\n", __func__);	/* Charger Safety Timer is expired */
			ret = -EINVAL;
		} else if (val[REG_CHARGING] & PCA9482_BIT_VBAT_OVP) {
			pr_err("%s: VBAT_OVP\n", __func__);			/* VBAT > VBAT_OVP */
			ret = -EINVAL;
		} else if (val[REG_SC_0] & PCA9482_BIT_PHASE_B_FAULT) {
			pr_err("%s: PHASE_B_FAULT\n", __func__);	/* Malfunction on Phase B */
			ret = -EINVAL;
		} else if (val[REG_SC_0] & PCA9482_BIT_PHASE_A_FAULT) {
			pr_err("%s: PHASE_A_FAULT\n", __func__);	/* Malfunction on Phase A */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_CBST_SHORT) {
			pr_err("%s: CBST_SHORT\n", __func__); 		/* CBST(Boostrap capacitor) short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_CFLY_SHORT) {
			pr_err("%s: CFLY_SHORT\n", __func__); 		/* CFLY(Flying capacitor) short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_VIN_SHORT) {
			pr_err("%s: VIN_SHORT\n", __func__); 		/* VIN short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_OVPOUT_SHORT) {
			pr_err("%s: OVPOUT_SHORT\n", __func__);		/* OVPOUT short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_OVPOUT_ERRLO) {
			pr_err("%s: OVPOUT_ERRLO\n", __func__);		/* OVP_OUT is too low */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_SW_SHORT) {
			pr_err("%s: SW_SHORT\n", __func__); 		/* SW1/2/3/4 short */
			ret = -EINVAL;
		} else {
			pr_err("%s: Power state error\n", __func__);	/* Power State error */
			/* Check charging state - Only retry in check active state */
			if (pca9482->charging_state == DC_STATE_CHECK_ACTIVE)
				ret = -EAGAIN;	/* retry */
			else
				ret = -EINVAL;
		}
		/* Read all registers for debugging */
		rt = pca9482_show_debug(pca9482);
		if (rt < 0) {
			pr_info("%s: i2c error, rt=%d\n", __func__, rt);
			goto error;
		}
	}

error:
	/* Check RCP DONE case */
	if (ret == -ERROR_DCRCP) {
		/* Check DC state first */
		if ((pca9482->charging_state == DC_STATE_START_CV) ||
			(pca9482->charging_state == DC_STATE_CV_MODE)) {
			/* Now present state is start_cv or cv_mode */
			/* Compare VBAT_ADC with Vfloat threshold */
			if (pca9482->prev_vbat > pca9482->vfloat) {
				/* Keep RCP DONE error */
				pr_info("%s: Keep RCP_DONE error type(%d)\n",
						__func__, ret);
			} else {
				/* Overwrite error type to -EINVAL */
				ret = -EINVAL;
				pr_info("%s: Overwrite RCP_DONE error, prev_vbat=%duV\n",
						__func__, pca9482->prev_vbat);
			}
		} else {
			/* Overwrite error type to -EINVAL */
			ret = -EINVAL;
			pr_info("%s: Overwrite RCP_DONE error, charging_state=%d\n",
					__func__, pca9482->charging_state);
		}
	}
	pr_info("%s: Active Status=%d\n", __func__, ret);
	return ret;
}

/* Check Reverse active status */
static int pca9482_check_reverse_error(struct pca9482_charger *pca9482)
{
	int ret;
	unsigned int reg_val;
	int vbatt;
	int rt;

	/* Read DEVICE_3_STS register */
	ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_3_STS, &reg_val);
	if (ret < 0)
		goto error;

	/* Read VBAT_ADC */
	vbatt = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

	/* Check Active status */
	if ((reg_val & PCA9482_BIT_STATUS_CHANGE) == PCA9482_12SW_R11_MODE) {
		/* PCA9482 is in 1:2 switching mode or reverse 1:1 mode */
		/* Check temperature regulation loop */
		/* Read DEVICE_2_STS register */
		ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_2_STS, &reg_val);
		if (reg_val & PCA9482_BIT_THEM_REG) {
			/* Thermal regulation happened */
			pr_err("%s: Device is in temperature regulation\n", __func__);
			ret = -EINVAL;
		} else {
			/* Normal temperature */
			ret = 0;
		}
	} else {
		/* PCA9482 is not in 1:2 switching mode or reverse 1:1 mode - standby or shutdown state */
		/* Stop charging in timer_work */

		/* Read all status register for debugging */
		u8 val[REG_BUFFER_MAX];
		u8 deg_val[10]; /* Dump for control register */

		rt = pca9482_bulk_read_reg(pca9482, PCA9482_REG_DEVICE_0_STS | PCA9482_REG_BIT_AI,
								&val[REG_DEVICE_0], REG_BUFFER_MAX);
		if (rt < 0)
			goto error;
		pr_err("%s: Error reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
				__func__, val[0], val[1], val[2], val[3], val[4], val[5], val[6]);

		/* Check status register */
		if (val[REG_DEVICE_0] & PCA9482_BIT_VOUT_MAX_OV) {
			pr_err("%s: VOUT MAX_OV\n", __func__);		/* VOUT > VOUT_MAX */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_1] & (PCA9482_BIT_NTC_0_DETECTED | PCA9482_BIT_NTC_1_DETECTED)) {
			if (pca9482->pdata->ntc_en == 0) {
				/* NTC protection function is disabled */
				/* ignore it */
				pr_err("%s: NTC_THRESHOLD - ignore\n", __func__);	/* NTC_THRESHOLD */
				ret = -EAGAIN;
			} else {
				pr_err("%s: NTC_THRESHOLD\n", __func__);	/* NTC_THRESHOLD */
				ret = -EINVAL;
			}
		} else if (val[REG_DEVICE_2] & PCA9482_BIT_VIN_OCP_12_11) {
			pr_err("%s: VIN_OCP_12_11\n", __func__);	/* VIN_OCP */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_2] & PCA9482_BIT_THSD) {
			pr_err("%s: THERMAL_SHUTDOWN\n", __func__);	/* Thermal Shutdown */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_2] & PCA9482_BIT_WATCHDOG_TIMER_OUT) {
			pr_err("%s: WATCHDOG_TIMER_OUT\n", __func__);	/* Watchdog timer out */
			ret = -EINVAL;
		} else if (val[REG_SC_0] & PCA9482_BIT_PHASE_B_FAULT) {
			pr_err("%s: PHASE_B_FAULT\n", __func__);	/* Malfunction on Phase B */
			ret = -EINVAL;
		} else if (val[REG_SC_0] & PCA9482_BIT_PHASE_A_FAULT) {
			pr_err("%s: PHASE_A_FAULT\n", __func__);	/* Malfunction on Phase A */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_CBST_SHORT) {
			pr_err("%s: CBST_SHORT\n", __func__);		/* CBST(Boostrap capacitor) short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_CFLY_SHORT) {
			pr_err("%s: CFLY_SHORT\n", __func__);		/* CFLY(Flying capacitor) short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_VIN_SHORT) {
			pr_err("%s: VIN_SHORT\n", __func__);		/* VIN short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_OVPOUT_SHORT) {
			pr_err("%s: OVPOUT_SHORT\n", __func__); 	/* OVPOUT short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_SW_SHORT) {
			pr_err("%s: SW_SHORT\n", __func__); 		/* SW1/2/3/4 short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9482_BIT_REVERSE_SW_SS_OC) {
			pr_err("%s: REVERSE_SW_SS_OC\n", __func__); /* OC detect during 1:2 or reverse 1:1 mode soft-start */
			ret = -EINVAL;
		} else {
			pr_err("%s: Power state error\n", __func__);	/* Power State error */
			ret = -EAGAIN;	/* retry */
		}

		/* Read all interrupt registers for debugging */
		rt = pca9482_bulk_read_reg(pca9482, PCA9482_REG_INT_DEVICE_0 | PCA9482_REG_BIT_AI,
								&val[REG_DEVICE_0], REG_BUFFER_MAX);
		if (rt < 0)
			goto error;
		pr_err("%s: interrupt reg[0x01]=0x%x,[0x02]=0x%x,[0x03]=0x%x,[0x04]=0x%x,[0x05]=0x%x,[0x06]=0x%x,[0x07]=0x%x\n",
				__func__, val[0], val[1], val[2], val[3], val[4], val[5], val[6]);

		/* Read all control registers for debugging */
		/* Read device, auto restart, and rcp control registers for debugging */
		rt = pca9482_bulk_read_reg(pca9482, PCA9482_REG_DEVICE_CNTL_0 | PCA9482_REG_BIT_AI,
								&deg_val[0], 7);
		if (rt < 0)
			goto error;
		pr_info("%s: control reg[0x16]=0x%x,[0x17]=0x%x,[0x18]=0x%x,[0x19]=0x%x,[0x1A]=0x%x,[0x1B]=0x%x,[0x1C]=0x%x\n",
				__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5], deg_val[6]);

		/* Read charging control registers for debugging */
		rt = pca9482_bulk_read_reg(pca9482, PCA9482_REG_CHARGING_CNTL_0 | PCA9482_REG_BIT_AI,
								&deg_val[0], 7);
		if (rt < 0)
			goto error;
		pr_info("%s: control reg[0x1D]=0x%x,[0x1E]=0x%x,[0x1F]=0x%x,[0x20]=0x%x,[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x\n",
			__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5], deg_val[6]);

		/* Read ntc, sc, and adc control registers for debugging */
		rt = pca9482_bulk_read_reg(pca9482, PCA9482_REG_NTC_0_CNTL | PCA9482_REG_BIT_AI,
								&deg_val[0], 9);
		if (rt < 0)
			goto error;
		pr_info("%s: control reg[0x24]=0x%x,[0x25]=0x%x,[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x\n",
				__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
		pr_info("%s: control reg[0x2A]=0x%x,[0x2B]=0x%x,[0x2C]=0x%x\n",
				__func__, deg_val[6], deg_val[7], deg_val[8]);

		/* Read ADC register for debugging */
		rt = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_VIN_0 | PCA9482_REG_BIT_AI,
								&deg_val[0], 10);
		if (rt < 0)
			goto error;
		pr_info("%s: adc reg[0x2D]=0x%x,[0x2E]=0x%x,[0x2F]=0x%x,[0x30]=0x%x,[0x31]=0x%x,[0x32]=0x%x\n",
				__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
		pr_info("%s: adc reg[0x33]=0x%x,[0x34]=0x%x,[0x35]=0x%x,[0x36]=0x%x\n",
				__func__, deg_val[6], deg_val[7], deg_val[8], deg_val[9]);

		rt = pca9482_bulk_read_reg(pca9482, PCA9482_REG_ADC_READ_NTC_0 | PCA9482_REG_BIT_AI,
								&deg_val[0], 8);
		if (rt < 0)
			goto error;
		pr_info("%s: adc reg[0x37]=0x%x,[0x38]=0x%x,[0x39]=0x%x,[0x3A]=0x%x,[0x3B]=0x%x,[0x3C]=0x%x\n",
				__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
		pr_info("%s: adc reg[0x3D]=0x%x,[0x3E]=0x%x\n", __func__, deg_val[6], deg_val[7]);
	}

error:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (ret == -EINVAL) {
		pca9482->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		pca9482->health_status = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
	}
#endif
	pr_info("%s: Active Status=%d\n", __func__, ret);
	return ret;
}

/* Check DC Mode status */
static int pca9482_check_dcmode_status(struct pca9482_charger *pca9482)
{
	unsigned int reg_val;
	int ret, i;
	int vbatt;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_DUAL_BATTERY)
	union power_supply_propval value = {0,};

	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_DIRECT_VBAT_CHECK, value);
	if (value.intval) {
		if (pca9482->charging_state <= DC_STATE_CC_MODE)
			ret = DCMODE_VFLT_LOOP;
		else
			ret = DCMODE_CHG_DONE;
		pr_info("%s: CC or CV MODE will be done by main or sub vbatt\n", __func__);
		goto error;
	}
#endif

	/* Read CHARGING_STS */
	ret = pca9482_read_reg(pca9482, PCA9482_REG_CHARGING_STS, &reg_val);
	if (ret < 0)
		goto error;

	/* Read battery voltage from fuelgauge */
	vbatt = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
	if (vbatt < 0) {
		ret = vbatt;
		goto error;
	}

	/* Check VFLOAT method */
	if (pca9482->pdata->fg_vfloat == true) {
		/* Compare FG Vnow with VFLOAT threshold and then check STS_A */
		if (vbatt > pca9482->vfloat) {
			ret = DCMODE_VFLT_LOOP;
			pr_info("%s: FG Vnow=%d, FG Vfloat\n", __func__, vbatt);
		} else if (reg_val & PCA9482_BIT_VBAT_REG_LOOP) {
			ret = DCMODE_VFLT_LOOP;
		} else if (reg_val & PCA9482_BIT_I_VBAT_CC_LOOP) {
			ret = DCMODE_CHG_LOOP;
		} else if (reg_val & PCA9482_BIT_I_VIN_CC_LOOP) {
			ret = DCMODE_IIN_LOOP;
			/* Check IIN_LOOP again to avoid unstable IIN_LOOP period */
			for (i = 0; i < 4; i++) {
				/* Wait 2ms */
				usleep_range(2000, 3000);
				/* Read CHARGING_STS again */
				ret = pca9482_read_reg(pca9482, PCA9482_REG_CHARGING_STS, &reg_val);
				if (ret < 0)
					goto error;
				/* Overwrite status */
				ret = DCMODE_IIN_LOOP;
				/* Check CHARGING_STS again */
				if ((reg_val & PCA9482_BIT_I_VIN_CC_LOOP) != PCA9482_BIT_I_VIN_CC_LOOP) {
					/* Now pca9482 is in unstable IIN_LOOP period */
					/* Ignore IIN_LOOP status */
					pr_info("%s: Unstable IIN_LOOP\n", __func__);
					ret = DCMODE_LOOP_INACTIVE;
					break;
				}
			}
		} else {
			ret = DCMODE_LOOP_INACTIVE;
		}
	} else {
		/* Check CHARGING_STS */
		if (reg_val & PCA9482_BIT_VBAT_REG_LOOP) {
			ret = DCMODE_VFLT_LOOP;
		} else if (reg_val & PCA9482_BIT_I_VBAT_CC_LOOP) {
			ret = DCMODE_CHG_LOOP;
		} else if (reg_val & PCA9482_BIT_I_VIN_CC_LOOP) {
			ret = DCMODE_IIN_LOOP;
			/* Check IIN_LOOP again to avoid unstable IIN_LOOP period */
			for (i = 0; i < 4; i++) {
				/* Wait 2ms */
				usleep_range(2000, 3000);
				/* Read CHARGING_STS again */
				ret = pca9482_read_reg(pca9482, PCA9482_REG_CHARGING_STS, &reg_val);
				if (ret < 0)
					goto error;
				/* Overwrite status */
				ret = DCMODE_IIN_LOOP;
				/* Check CHARGING_STS again */
				if ((reg_val & PCA9482_BIT_I_VIN_CC_LOOP) != PCA9482_BIT_I_VIN_CC_LOOP) {
					/* Now pca9482 is in unstable IIN_LOOP period */
					/* Ignore IIN_LOOP status */
					pr_info("%s: Unstable IIN_LOOP\n", __func__);
					ret = DCMODE_LOOP_INACTIVE;
					break;
				}
			}
		} else {
			ret = DCMODE_LOOP_INACTIVE;
		}
	}

error:
	pr_info("%s: DCMODE Status=%d\n", __func__, ret);
	return ret;
}

/* Stop Charging */
static int pca9482_stop_charging(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int val;

	/* Check the current state and pca9482 enable status */
	if ((pca9482->charging_state != DC_STATE_NO_CHARGING) ||
		(pca9482->enable == true)) {
		/* Recover switching charger ICL */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9482_set_switching_charger(pca9482, true);
#else
		ret = pca9482_set_switching_charger(true, SWCHG_ICL_NORMAL,
											pca9482->ichg_cfg,
											pca9482->vfloat);
#endif
		if (ret < 0) {
			pr_err("%s: Error-set_switching charger ICL\n", __func__);
			goto error;
		}

		/* Stop Direct charging */
		cancel_delayed_work(&pca9482->timer_work);
		cancel_delayed_work(&pca9482->pps_work);
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_ID_NONE;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);

		/* Clear parameter */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9482_set_charging_state(pca9482, DC_STATE_NO_CHARGING);
		pca9482_init_adc_val(pca9482, -1);
#else
		pca9482->charging_state = DC_STATE_NO_CHARGING;
#endif
		pca9482->ret_state = DC_STATE_NO_CHARGING;
		pca9482->ta_target_vol = TA_MAX_VOL;
		pca9482->prev_iin = 0;
		pca9482->prev_inc = INC_NONE;
		pca9482->req_new_iin = false;
		pca9482->req_new_vfloat = false;
		pca9482->chg_mode = CHG_NO_DC_MODE;
		pca9482->ta_ctrl = TA_CTRL_CL_MODE;

		/* Keep iin_cfg, ichg_cfg, vfloat and iin_topoff to the current value */
		/* Keep new vfloat and new iin */

		/* Clear new DC mode and DC mode */
		pca9482->new_dc_mode = PTM_NONE;
		pca9482->dc_mode = PTM_NONE;
		pca9482->req_new_dc_mode = false;

		/* Clear switching frequency change sequence */
		pca9482->req_sw_freq = REQ_SW_FREQ_0;

		/* Set vfloat decrement flag to false */
		pca9482->dec_vfloat = false;

		/* Clear reverse mode */
		pca9482->rev_mode = POWER_SUPPLY_DC_REVERSE_STOP;
		pca9482->iin_rev = 0;

		/* Clear previous VBAT_ADC */
		pca9482->prev_vbat = 0;

		/* Clear charging done counter */
		pca9482->done_cnt = 0;

		/* Disable VIN_OCP_12_11_EN */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_5,
								PCA9482_BIT_VIN_OCP_12_11_EN, 0);
		if (ret < 0) {
			pr_err("%s: Error-set VIN_OCP_12_11_EN\n", __func__);
			goto error;
		}

		/* Enable reverse current detection */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_RCP_CNTL,
								PCA9482_BIT_RCP_EN, PCA9482_BIT_RCP_EN);
		if (ret < 0) {
			pr_err("%s: Error-set RCP detection\n", __func__);
			goto error;
		}

		/* Enable OV_TRACKING_EN */
		ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_2,
									PCA9482_BIT_OV_TRACKING_EN, PCA9482_BIT_OV_TRACKING_EN);
		if (ret < 0) {
			pr_err("%s: Error-set OV_TRACKING_EN\n", __func__);
			goto error;
		}

		/* Clear retry counter */
		pca9482->retry_cnt = 0;

		/* Recover ADC mode to AUTO mode */
		/* Configure ADC operation mode to Auto mode */
		val = AUTO_MODE << MASK2SHIFT(PCA9482_BIT_ADC_MODE_CFG);
		ret = pca9482_update_reg(pca9482, PCA9482_REG_ADC_CNTL,
								PCA9482_BIT_ADC_MODE_CFG, val);
		if (ret < 0) {
			pr_err("%s: Error-set ADC_MODE\n", __func__);
			goto error;
		}
		/* Read ADC_CNTL register to confirm recover */
		ret = pca9482_read_reg(pca9482, PCA9482_REG_ADC_CNTL, &val);
		pr_info("%s: ADC_CTRL : 0x%02x\n", __func__, val);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		/* Set watchdog timer disable */
		pca9482_set_wdt_enable(pca9482, WDT_DISABLE);
#endif
		ret = pca9482_set_charging(pca9482, false);
		if (ret < 0) {
			pr_err("%s: Error-set_charging(main)\n", __func__);
			goto error;
		}
	}

error:
	__pm_relax(pca9482->monitor_wake_lock);
	pr_info("%s: END, ret=%d\n", __func__, ret);

	return ret;
}

/* Compensate TA current for the target input current */
static int pca9482_set_ta_current_comp(struct pca9482_charger *pca9482)
{
	int iin, ichg;

	/* Read IIN ADC */
	iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
	/* Read ICHG ADC */
	ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);

	pr_info("%s: iin=%d, ichg=%d\n", __func__, iin, ichg);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9482->iin_cc + IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Compare TA current with IIN_CC - LOW_OFFSET */
		if (pca9482->ta_cur > pca9482->iin_cc - TA_CUR_LOW_OFFSET) {
			/* TA current is higher than IIN_CC - LOW_OFFSET */
			/* Assume that TA operation mode is CL mode, so decrease TA current */
			/* Decrease TA current (50mA) */
			pca9482->ta_cur = pca9482->ta_cur - PD_MSG_TA_CUR_STEP;
			pr_info("%s: Comp. Cont1: ta_cur=%d\n", __func__, pca9482->ta_cur);
		} else {
			/* TA current is already lower than IIN_CC - LOW_OFFSET */
			/* IIN_ADC is stiil in invalid range even though TA current is less than IIN_CC - LOW_OFFSET */
			/* TA has abnormal behavior */
			/* Decrease TA voltage (20mV) */
			pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
			pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9482->ta_vol);
			/* Update TA target voltage */
			pca9482->ta_target_vol = pca9482->ta_vol;
		}
		/* Send PD Message */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
	} else {
		if (iin < (pca9482->iin_cc - IIN_CC_COMP_OFFSET)) {
			/* TA current is lower than the target input current */
			/* Compare present TA voltage with TA maximum voltage first */
			if (pca9482->ta_vol == pca9482->ta_max_vol) {
				/* TA voltage is already the maximum voltage */
				/* Compare present TA current with TA maximum current */
				if (pca9482->ta_cur == pca9482->ta_max_cur) {
					/* Both of present TA voltage and current are already maximum values */
					pr_info("%s: Comp. End1(Max value): ta_vol=%d, ta_cur=%d\n", __func__, pca9482->ta_vol, pca9482->ta_cur);
					/* Set timer */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_CHECK_CCMODE;
					pca9482->timer_period = CCMODE_CHECK_T;
					mutex_unlock(&pca9482->lock);
				} else {
					/* TA voltage is maximum voltage, but TA current is not maximum current */
					/* Increase TA current (50mA) */
					pca9482->ta_cur = pca9482->ta_cur + PD_MSG_TA_CUR_STEP;
					if (pca9482->ta_cur > pca9482->ta_max_cur)
						pca9482->ta_cur = pca9482->ta_max_cur;
					pr_info("%s: Comp. Cont3: ta_cur=%d\n", __func__, pca9482->ta_cur);
					/* Send PD Message */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_PDMSG_SEND;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);

					/* Set TA increment flag */
					pca9482->prev_inc = INC_TA_CUR;
				}
			} else {
				/* TA voltage is not maximum voltage */
				/* Compare IIN ADC with previous IIN ADC + 20mA */
				if (iin > (pca9482->prev_iin + IIN_ADC_OFFSET)) {
					/* In this case, TA voltage is not enough to supply the operating current of RDO.
					   So, increase TA voltage */
					/* Increase TA voltage (20mV) */
					pca9482->ta_vol = pca9482->ta_vol + PD_MSG_TA_VOL_STEP;
					if (pca9482->ta_vol > pca9482->ta_max_vol)
						pca9482->ta_vol = pca9482->ta_max_vol;
					pr_info("%s: Comp. Cont4: ta_vol=%d\n", __func__, pca9482->ta_vol);
					/* Update TA target voltage */
					pca9482->ta_target_vol = pca9482->ta_vol;
					/* Send PD Message */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_PDMSG_SEND;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);

					/* Set TA increment flag */
					pca9482->prev_inc = INC_TA_VOL;
				} else {
					/* Input current increment is too low */
					/* It is possible that TA is in current limit mode or has low TA voltage */
					/* Increase TA current or voltage */
					/* Check the previous TA increment */
					if (pca9482->prev_inc == INC_TA_VOL) {
						/* The previous increment is TA voltage, but input current does not increase */
						/* Try to increase TA current */
						/* Compare present TA current with TA maximum current */
						if (pca9482->ta_cur == pca9482->ta_max_cur) {
							/* TA current is already the maximum current */

							/* Increase TA voltage (20mV) */
							pca9482->ta_vol = pca9482->ta_vol + PD_MSG_TA_VOL_STEP;
							if (pca9482->ta_vol > pca9482->ta_max_vol)
								pca9482->ta_vol = pca9482->ta_max_vol;
							pr_info("%s: Comp. Cont5: ta_vol=%d\n", __func__, pca9482->ta_vol);
							/* Update TA target voltage */
							pca9482->ta_target_vol = pca9482->ta_vol;
							/* Send PD Message */
							mutex_lock(&pca9482->lock);
							pca9482->timer_id = TIMER_PDMSG_SEND;
							pca9482->timer_period = 0;
							mutex_unlock(&pca9482->lock);

							/* Set TA increment flag */
							pca9482->prev_inc = INC_TA_VOL;
						} else {
							/* Check the present TA current */
							/* Consider tolerance offset(100mA) */
							if (pca9482->ta_cur >= (pca9482->iin_cc + TA_IIN_OFFSET)) {
								/* Maybe TA supply current is enough, but TA voltage is low */
								/* Increase TA voltage (20mV) */
								pca9482->ta_vol = pca9482->ta_vol + PD_MSG_TA_VOL_STEP;
								if (pca9482->ta_vol > pca9482->ta_max_vol)
									pca9482->ta_vol = pca9482->ta_max_vol;
								pr_info("%s: Comp. Cont6: ta_vol=%d\n", __func__, pca9482->ta_vol);
								/* Update TA target voltage */
								pca9482->ta_target_vol = pca9482->ta_vol;
								/* Send PD Message */
								mutex_lock(&pca9482->lock);
								pca9482->timer_id = TIMER_PDMSG_SEND;
								pca9482->timer_period = 0;
								mutex_unlock(&pca9482->lock);

								/* Set TA increment flag */
								pca9482->prev_inc = INC_TA_VOL;
							} else {
								/* It is possible that TA is in current limit mode */
								/* Increase TA current (50mA) */
								pca9482->ta_cur = pca9482->ta_cur + PD_MSG_TA_CUR_STEP;
								if (pca9482->ta_cur > pca9482->ta_max_cur)
									pca9482->ta_cur = pca9482->ta_max_cur;
								pr_info("%s: Comp. Cont7: ta_cur=%d\n", __func__, pca9482->ta_cur);
								/* Send PD Message */
								mutex_lock(&pca9482->lock);
								pca9482->timer_id = TIMER_PDMSG_SEND;
								pca9482->timer_period = 0;
								mutex_unlock(&pca9482->lock);

								/* Set TA increment flag */
								pca9482->prev_inc = INC_TA_CUR;
							}
						}
					} else {
						/* The previous increment is TA current, but input current does not increase */
						/* Try to increase TA voltage */
						/* Increase TA voltage (20mV) */
						pca9482->ta_vol = pca9482->ta_vol + PD_MSG_TA_VOL_STEP;
						if (pca9482->ta_vol > pca9482->ta_max_vol)
							pca9482->ta_vol = pca9482->ta_max_vol;
						pr_info("%s: Comp. Cont8: ta_vol=%d\n", __func__, pca9482->ta_vol);
						/* Update TA target voltage */
						pca9482->ta_target_vol = pca9482->ta_vol;
						/* Send PD Message */
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_PDMSG_SEND;
						pca9482->timer_period = 0;
						mutex_unlock(&pca9482->lock);

						/* Set TA increment flag */
						pca9482->prev_inc = INC_TA_VOL;
					}
				}
			}
		} else {
			/* IIN ADC is in valid range */
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			pr_info("%s: Comp. End2(valid): ta_vol=%d, ta_cur=%d\n", __func__, pca9482->ta_vol, pca9482->ta_cur);
			/* Clear TA increment flag */
			pca9482->prev_inc = INC_NONE;
			/* Set timer */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_CCMODE;
			pca9482->timer_period = CCMODE_CHECK_T;
			mutex_unlock(&pca9482->lock);
		}
	}

	/* Save previous iin adc */
	pca9482->prev_iin = iin;

	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));

	return 0;
}

/* Compensate TA current for constant power mode */
static int pca9482_set_ta_current_comp2(struct pca9482_charger *pca9482)
{
	int iin, ichg;
	unsigned int val;
	unsigned int iin_apdo;

	/* Read IIN ADC */
	iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
	/* Read ICHG ADC */
	ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);

	pr_info("%s: iin=%d, ichg=%d\n", __func__, iin, ichg);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9482->iin_cfg + IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Compare TA current with IIN_CC - LOW_OFFSET */
		if (pca9482->ta_cur > pca9482->iin_cc - TA_CUR_LOW_OFFSET) {
			/* TA current is higher than IIN_CC - LOW_OFFSET */
			/* Assume that TA operation mode is CL mode, so decrease TA current */
			/* Decrease TA current (50mA) */
			pca9482->ta_cur = pca9482->ta_cur - PD_MSG_TA_CUR_STEP;
			pr_info("%s: Comp. Cont1: ta_cur=%d\n", __func__, pca9482->ta_cur);
		} else {
			/* TA current is already lower than IIN_CC - LOW_OFFSET */
			/* IIN_ADC is stiil in invalid range even though TA current is less than IIN_CC - LOW_OFFSET */
			/* TA has abnormal behavior */
			/* Decrease TA voltage (20mV) */
			pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
			pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9482->ta_vol);
			/* Update TA target voltage */
			pca9482->ta_target_vol = pca9482->ta_vol;
		}

		/* Send PD Message */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
	} else if (iin < (pca9482->iin_cc - IIN_CC_COMP_OFFSET_CP)) {
		/* TA current is lower than the target input current */
		/* IIN_ADC < IIN_CC -20mA */
		if (pca9482->ta_vol == pca9482->ta_max_vol) {
			/* Check IIN_ADC < IIN_CC - 50mA */
			if (iin < (pca9482->iin_cc - IIN_CC_COMP_OFFSET)) {
				/* Compare the TA current with IIN_CC and maximum current of APDO */
				if ((pca9482->ta_cur >= (pca9482->iin_cc/pca9482->chg_mode)) ||
					(pca9482->ta_cur == pca9482->ta_max_cur)) {
					/* TA current is higher than IIN_CC or maximum TA current */
					/* Set new IIN_CC to IIN_CC - 50mA */
					pca9482->iin_cc = pca9482->iin_cc - IIN_CC_COMP_OFFSET;
					/* Set new TA_MAX_VOL to TA_MAX_PWR/IIN_CC */
					/* Adjust new IIN_CC with APDO resolution */
					iin_apdo = pca9482->iin_cc/PD_MSG_TA_CUR_STEP;
					iin_apdo = iin_apdo*PD_MSG_TA_CUR_STEP;
					val = pca9482->ta_max_pwr/(iin_apdo/pca9482->chg_mode/1000);	/* mV */
					val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
					val = val*PD_MSG_TA_VOL_STEP; /* uV */
					/* Set new TA_MAX_VOL */
					pca9482->ta_max_vol = MIN(val, TA_MAX_VOL*pca9482->chg_mode);
					/* Increase TA voltage(40mV) */
					pca9482->ta_vol = pca9482->ta_vol + PD_MSG_TA_VOL_STEP*2;
					if (pca9482->ta_vol > pca9482->ta_max_vol)
						pca9482->ta_vol = pca9482->ta_max_vol;
					pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9482->ta_vol);
					/* Update TA target voltage */
					pca9482->ta_target_vol = pca9482->ta_vol;

					/* Send PD Message */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_PDMSG_SEND;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);
				} else {
					/* TA current is less than IIN_CC and not maximum current */
					/* Increase TA current (50mA) */
					pca9482->ta_cur = pca9482->ta_cur + PD_MSG_TA_CUR_STEP;
					if (pca9482->ta_cur > pca9482->ta_max_cur)
						pca9482->ta_cur = pca9482->ta_max_cur;
					pr_info("%s: Comp. Cont3: ta_cur=%d\n", __func__, pca9482->ta_cur);

					/* Send PD Message */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_PDMSG_SEND;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);
				}
			} else {
				/* Wait for next current step compensation */
				/* IIN_CC - 50mA < IIN ADC < IIN_CC - 20mA */
				pr_info("%s: Comp.(wait): ta_vol=%d\n", __func__, pca9482->ta_vol);
				/* Set timer */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_CHECK_CCMODE;
				pca9482->timer_period = CCMODE_CHECK_T;
				mutex_unlock(&pca9482->lock);
			}
		} else {
			/* Increase TA voltage(40mV) */
			pca9482->ta_vol = pca9482->ta_vol + PD_MSG_TA_VOL_STEP*2;
			if (pca9482->ta_vol > pca9482->ta_max_vol)
				pca9482->ta_vol = pca9482->ta_max_vol;
			pr_info("%s: Comp. Cont4: ta_vol=%d\n", __func__, pca9482->ta_vol);
			/* Update TA target voltage */
			pca9482->ta_target_vol = pca9482->ta_vol;

			/* Send PD Message */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
		}
	} else {
		/* IIN ADC is in valid range */
		/* IIN_CC - 20mA < IIN ADC < IIN_CFG + 50mA */
		pr_info("%s: Comp. End(valid): ta_vol=%d\n", __func__, pca9482->ta_vol);
		/* Set timer */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_CHECK_CCMODE;
		pca9482->timer_period = CCMODE_CHECK_T;
		mutex_unlock(&pca9482->lock);
	}

	/* Save previous iin adc */
	pca9482->prev_iin = iin;

	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));

	return 0;
}

/* Compensate TA voltage for the target input current */
static int pca9482_set_ta_voltage_comp(struct pca9482_charger *pca9482)
{
	int iin, ichg;

	pr_info("%s: ======START=======\n", __func__);

	/* Read IIN ADC */
	iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
	/* Read ICHG ADC */
	ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);

	pr_info("%s: iin=%d, ichg=%d\n", __func__, iin, ichg);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9482->iin_cc + IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Decrease TA voltage (20mV) */
		pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
		pr_info("%s: Comp. Cont1: ta_vol=%d\n", __func__, pca9482->ta_vol);

		/* Send PD Message */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
	} else {
		if (iin < (pca9482->iin_cc - IIN_CC_COMP_OFFSET)) {
			/* TA current is lower than the target input current */
			/* Compare TA max voltage */
			if (pca9482->ta_vol == pca9482->ta_max_vol) {
				/* TA current is already the maximum voltage */
				pr_info("%s: Comp. End1(max TA vol): ta_vol=%d\n", __func__, pca9482->ta_vol);
				/* Set timer */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_CHECK_CCMODE;
				pca9482->timer_period = CCMODE_CHECK_T;
				mutex_unlock(&pca9482->lock);
			} else {
				/* Increase TA voltage (20mV) */
				pca9482->ta_vol = pca9482->ta_vol + PD_MSG_TA_VOL_STEP;
				pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9482->ta_vol);

				/* Send PD Message */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_PDMSG_SEND;
				pca9482->timer_period = 0;
				mutex_unlock(&pca9482->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			pr_info("%s: Comp. End(valid): ta_vol=%d\n", __func__, pca9482->ta_vol);
			/* Set timer */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_CCMODE;
			pca9482->timer_period = CCMODE_CHECK_T;
			mutex_unlock(&pca9482->lock);
		}
	}

	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));

	return 0;
}

/* Compensate RX voltage for the target input current */
static int pca9482_set_rx_voltage_comp(struct pca9482_charger *pca9482)
{
	int iin, ichg;

	pr_info("%s: ======START=======\n", __func__);

	/* Read IIN ADC */
	iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
	/* Read ICHG ADC */
	ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);

	pr_info("%s: iin=%d, ichg=%d\n", __func__, iin, ichg);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9482->iin_cc + IIN_CC_COMP_OFFSET)) {
		/* RX current is higher than the target input current */
		/* Decrease RX voltage (100mV) */
		pca9482->ta_vol = pca9482->ta_vol - WCRX_VOL_STEP;
		pr_info("%s: Comp. Cont1: rx_vol=%d\n", __func__, pca9482->ta_vol);

		/* Set RX Voltage */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
	} else {
		if (iin < (pca9482->iin_cc - IIN_CC_COMP_OFFSET)) {
			/* RX current is lower than the target input current */
			/* Compare RX max voltage */
			if (pca9482->ta_vol == pca9482->ta_max_vol) {
				/* TA current is already the maximum voltage */
				pr_info("%s: Comp. End1(max RX vol): rx_vol=%d\n", __func__, pca9482->ta_vol);
				/* Set timer */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_CHECK_CCMODE;
				pca9482->timer_period = CCMODE_CHECK_T;
				mutex_unlock(&pca9482->lock);
			} else {
				/* Increase RX voltage (100mV) */
				pca9482->ta_vol = pca9482->ta_vol + WCRX_VOL_STEP;
				pr_info("%s: Comp. Cont2: rx_vol=%d\n", __func__, pca9482->ta_vol);

				/* Set RX Voltage */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_PDMSG_SEND;
				pca9482->timer_period = 0;
				mutex_unlock(&pca9482->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			pr_info("%s: Comp. End(valid): rx_vol=%d\n", __func__, pca9482->ta_vol);
			/* Set timer */
			/* Check the current charging state */
			/* Set timer */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_CCMODE;
			pca9482->timer_period = CCMODE_CHECK_T;
			mutex_unlock(&pca9482->lock);
		}
	}

	/* Set TA target voltage to TA voltage */
	pca9482->ta_target_vol = pca9482->ta_vol;

	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));

	return 0;
}

/* Change switching frequency */
static int pca9482_change_sw_freq(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int vbat;
	unsigned int val;
	unsigned int power_state;

	pr_info("%s: ======START=======\n", __func__);

	/* Check request sequence */
	switch (pca9482->req_sw_freq) {
	case REQ_SW_FREQ_1:
		/* REQ_SW_FREQ_1 - Decrease TA voltage */
		/* Read VBAT_ADC */
		vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
		/* Set TA voltage to MIN[(2*VBAT_ADC + offset), TA target voltage] */
		pca9482->ta_vol = MIN((2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET), pca9482->ta_target_vol);
		val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
		pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;

		pr_info("%s: REQ_SW_FREQ_%d, ta_cur=%d, ta_vol=%d\n",
			__func__, pca9482->req_sw_freq, pca9482->ta_cur, pca9482->ta_vol);

		/* Set next request sequence */
		pca9482->req_sw_freq = REQ_SW_FREQ_2;

		/* Send PD Message */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		break;

	case REQ_SW_FREQ_2:
		/* REQ_SW_FREQ_2 - Disable DC */
		/* Disable charging */
		ret = pca9482_set_charging(pca9482, false);
		if (ret < 0)
			goto error;

		pr_info("%s: REQ_SW_FREQ_%d, disable DC\n",	__func__, pca9482->req_sw_freq);

		/* Set next request sequence */
		pca9482->req_sw_freq = REQ_SW_FREQ_3;

		/* Set timer */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_ADJUST_TACUR;
		pca9482->timer_period = DISABLE_DELAY_T; /* Wait 200ms */
		mutex_unlock(&pca9482->lock);
		break;

	case REQ_SW_FREQ_3:
		/* REQ_SW_FREQ_3 - Set switching frequency */
		if (pca9482->iin_cc > pca9482->pdata->iin_low_freq) {
			/* Set switching frequency to high frequency */
			pca9482->fsw_cfg = pca9482->pdata->fsw_cfg;
		} else {
			/* Set switching frequency to low frequency */
			pca9482->fsw_cfg = pca9482->pdata->fsw_cfg_low;
		}
		ret = pca9482_write_reg(pca9482, PCA9482_REG_SC_CNTL_0, PCA9482_FSW_CFG(pca9482->fsw_cfg));
		if (ret < 0)
			goto error;

		pr_info("%s: REQ_SW_FREQ_%d, sw_freq=%dkHz\n",	__func__, pca9482->req_sw_freq, pca9482->fsw_cfg);

		/* Set next request sequence */
		pca9482->req_sw_freq = REQ_SW_FREQ_4;

		/* Set timer */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_ADJUST_TACUR;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		break;

	case REQ_SW_FREQ_4:
		/* REQ_SW_FREQ_4 - Set TA current */

		/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
		val = pca9482->iin_cc/PD_MSG_TA_CUR_STEP;
		pca9482->iin_cc = val*PD_MSG_TA_CUR_STEP;
		/* Set TA current to IIN_CC */
		pca9482->ta_cur = pca9482->iin_cc/pca9482->chg_mode;

		/* Set TA voltage again before enable charging */
		/* Read VBAT_ADC */
		vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
		/* Set TA voltage to MIN[(2*VBAT_ADC + offset), TA target voltage] */
		pca9482->ta_vol = MIN((2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET), pca9482->ta_target_vol);
		val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
		pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;

		pr_info("%s: REQ_SW_FREQ_%d, ta_cur=%d, ta_vol=%d\n",
			__func__, pca9482->req_sw_freq, pca9482->ta_cur, pca9482->ta_vol);

		/* Set next request sequence */
		pca9482->req_sw_freq = REQ_SW_FREQ_5;

		/* Send PD Message */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		break;

	case REQ_SW_FREQ_5:
		/* REQ_SW_FREQ_5 - Enable DC */
		ret = pca9482_set_charging(pca9482, true);
		if (ret < 0)
			goto error;

		pr_info("%s: REQ_SW_FREQ_%d, enable DC\n",	__func__, pca9482->req_sw_freq);

		/* Set next request sequence */
		pca9482->req_sw_freq = REQ_SW_FREQ_6;

		/* Set timer */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_ADJUST_TACUR;
		pca9482->timer_period = ENABLE_DELAY_T; /* Wait 150ms */
		mutex_unlock(&pca9482->lock);
		break;

	case REQ_SW_FREQ_6:
		/* REQ_SW_FREQ_6 - Increase TA voltage */

		/* Check power state */
		/* Read DEVICE_3_STS register */
		ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_3_STS, &power_state);
		if (ret < 0)
			goto error;
		if ((power_state & PCA9482_BIT_STATUS_CHANGE) == PCA9482_21SW_F11_MODE) {
			/* TA voltage is initial voltage for switching frequency change */
			/* Increase TA voltage to TA target voltage */

			/* Calculate new TA maximum current and voltage that used in the direct charging */
			/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
			val = pca9482->iin_cc/(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
			pca9482->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
			/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
			val = pca9482->ta_max_pwr/(pca9482->iin_cc/pca9482->chg_mode/1000); /* mV */
			val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
			val = val*PD_MSG_TA_VOL_STEP; /* uV */
			pca9482->ta_max_vol = MIN(val, TA_MAX_VOL*pca9482->chg_mode);
			/* Recover IIN_CC to the original value(new_iin) */
			pca9482->iin_cc = pca9482->new_iin;

			/* Set TA voltage to TA target voltage */
			pca9482->ta_vol = pca9482->ta_target_vol;
			/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
			val = pca9482->iin_cc/PD_MSG_TA_CUR_STEP;
			pca9482->iin_cc = val*PD_MSG_TA_CUR_STEP;
			/* Set TA current to IIN_CC */
			pca9482->ta_cur = pca9482->iin_cc/pca9482->chg_mode;

			pr_info("%s: REQ_SW_FREQ_%d, ta_cur=%d, ta_vol=%d, ta_max_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9482->req_sw_freq, pca9482->ta_cur, pca9482->ta_vol, pca9482->ta_max_vol,
					pca9482->iin_cc, pca9482->chg_mode);

			/* Set next request sequence */
			pca9482->req_sw_freq = REQ_SW_FREQ_0;

			/* Send PD Message */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
		} else {
			/* pca9482 already set to enable charging, but power state is in standby state */
			/* DC error happens and check error type */
			pr_info("%s: Error - REQ_SW_FREQ_%d\n",	__func__, pca9482->req_sw_freq);
			ret = pca9482_check_error(pca9482);
			/* Overwrite error type to invalid error */
			/* Will stop charging in timer_work */
			ret = -EINVAL;
			goto error;
		}
		break;

	default:
		/* Cannot enter here */
		pr_info("%s: Error - REQ_SW_FREQ_%d\n",	__func__, pca9482->req_sw_freq);
		pca9482->req_sw_freq = REQ_SW_FREQ_0;
		ret = -EINVAL;
		goto error;
		break;
	}
	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set TA current for target current */
static int pca9482_adjust_ta_current(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int vbat;
	unsigned int val;

	pr_info("%s: ======START=======\n", __func__);

	/* Set charging state to ADJUST_TACUR */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_ADJUST_TACUR);
#else
	pca9482->charging_state = DC_STATE_ADJUST_TACUR;
#endif

	/* Check switching frequency change request */
	if (pca9482->req_sw_freq != REQ_SW_FREQ_0) {
		/* There is request for switching frequency change */
		ret = pca9482_change_sw_freq(pca9482);
		if (ret < 0)
			goto error;
	} else {
		/* There is no request for switching frequency change */

		/* Check whether TA current is same as IIN_CC or not */
		if (pca9482->ta_cur == pca9482->iin_cc/pca9482->chg_mode) {
			/* finish sending PD message */
			/* Recover IIN_CC to the original value(new_iin) */
			pca9482->iin_cc = pca9482->new_iin;

			/* Update iin_cfg */
			pca9482->iin_cfg = pca9482->iin_cc;
			/* Set IIN_CFG to new IIN */
			ret = pca9482_set_input_current(pca9482, pca9482->iin_cc);
			if (ret < 0)
				goto error;

			/* Clear req_new_iin */
			mutex_lock(&pca9482->lock);
			pca9482->req_new_iin = false;
			mutex_unlock(&pca9482->lock);

			pr_info("%s: adj. End, ta_cur=%d, ta_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9482->ta_cur, pca9482->ta_vol, pca9482->iin_cc, pca9482->chg_mode);

			/* Go to return state  */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_charging_state(pca9482, pca9482->ret_state);
#else
			pca9482->charging_state = pca9482->ret_state;
#endif
			/* Set timer */
			mutex_lock(&pca9482->lock);
			if (pca9482->charging_state == DC_STATE_CC_MODE)
				pca9482->timer_id = TIMER_CHECK_CCMODE;
			else
				pca9482->timer_id = TIMER_CHECK_CVMODE;
			pca9482->timer_period = 1000;	/* Wait 1s */
			mutex_unlock(&pca9482->lock);
		} else {
			/* Compare new IIN with current IIN_CFG */
			if (pca9482->iin_cc > pca9482->iin_cfg) {
				/* New iin is higher than current iin_cc(iin_cfg) */
				/* Compare new IIN with IIN_LOW_TH */
				if (pca9482->iin_cc > pca9482->pdata->iin_low_freq) {
					/* New IIN is high current */
					/* Check current switching frequency */
					if (pca9482->fsw_cfg == pca9482->pdata->fsw_cfg) {
						/* Current switching frequency is high frequency */
						/* Don't need to change switching frequency */
						/* Update iin_cfg */
						pca9482->iin_cfg = pca9482->iin_cc;
						/* Set IIN_CFG to new IIN */
						ret = pca9482_set_input_current(pca9482, pca9482->iin_cc);
						if (ret < 0)
							goto error;

						/* Set ICHG_CFG to enough high value */
						if (pca9482->ichg_cfg < 2*pca9482->iin_cfg)
							pca9482->ichg_cfg = 2*pca9482->iin_cfg;
						ret = pca9482_set_charging_current(pca9482, pca9482->ichg_cfg);
						if (ret < 0)
							goto error;

						/* Clear Request flag */
						mutex_lock(&pca9482->lock);
						pca9482->req_new_iin = false;
						mutex_unlock(&pca9482->lock);

						/* Set new TA voltage and current */
						/* Read VBAT ADC */
						vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

						/* Calculate new TA maximum current and voltage that used in the direct charging */
						/* Set IIN_CC to MIN[IIN, TA_MAX_CUR*chg_mode]*/
						pca9482->iin_cc = MIN(pca9482->iin_cfg, pca9482->ta_max_cur*pca9482->chg_mode);
						/* Set the current IIN_CC to iin_cfg for recovering it after resolution adjustment */
						pca9482->iin_cfg = pca9482->iin_cc;
						/* Calculate new TA max voltage */
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
						val = pca9482->iin_cc/(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
						pca9482->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
						/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
						val = pca9482->ta_max_pwr/(pca9482->iin_cc/pca9482->chg_mode/1000);	/* mV */
						val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
						val = val*PD_MSG_TA_VOL_STEP; /* uV */
						pca9482->ta_max_vol = MIN(val, TA_MAX_VOL*pca9482->chg_mode);

						/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
						pca9482->ta_vol = max(pca9482->pdata->ta_min_vol*pca9482->chg_mode, (2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET));
						val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
						pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;
						/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL] */
						pca9482->ta_vol = MIN(pca9482->ta_vol, pca9482->ta_max_vol);
						/* Set TA current to IIN_CC */
						pca9482->ta_cur = pca9482->iin_cc/pca9482->chg_mode;
						/* Recover IIN_CC to the original value(iin_cfg) */
						pca9482->iin_cc = pca9482->iin_cfg;

						pr_info("%s: New IIN(1), ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
							__func__, pca9482->ta_max_vol, pca9482->ta_max_cur, pca9482->ta_max_pwr, pca9482->iin_cc, pca9482->chg_mode);

						pr_info("%s: New IIN(1), ta_vol=%d, ta_cur=%d, sw_freq=%d\n",
							__func__, pca9482->ta_vol, pca9482->ta_cur, pca9482->fsw_cfg);

						/* Clear previous IIN ADC */
						pca9482->prev_iin = 0;
						/* Clear TA increment flag */
						pca9482->prev_inc = INC_NONE;

						/* Send PD Message and go to Adjust CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
						pca9482_set_charging_state(pca9482, DC_STATE_ADJUST_CC);
#else
						pca9482->charging_state = DC_STATE_ADJUST_CC;
#endif
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_PDMSG_SEND;
						pca9482->timer_period = IIN_CFG_WAIT_T;
						mutex_unlock(&pca9482->lock);
					} else {
						/* Need switching frequency change */
						/* Compare TA voltage with TA target voltage */
						if (pca9482->ta_vol == pca9482->ta_target_vol) {
							/* Decrease TA voltage first before disable DC to avoid VIN voltage jumping */
							/* Read VBAT ADC */
							vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
							/* Set TA voltage to 2*VBAT_ADC + offset */
							pca9482->ta_vol = 2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET;
							val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
							pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;

							pr_info("%s: New IIN(2), ta_vol=%d, ta_cur=%d, sw_freq=%d\n",
								__func__, pca9482->ta_vol, pca9482->ta_cur, pca9482->fsw_cfg);

							/* Send PD Message */
							mutex_lock(&pca9482->lock);
							pca9482->timer_id = TIMER_PDMSG_SEND;
							pca9482->timer_period = 0;
							mutex_unlock(&pca9482->lock);
						} else {
							/* Disable DC */
							ret = pca9482_set_charging(pca9482, false);
							if (ret < 0)
								goto error;

							/* Update iin_cfg */
							pca9482->iin_cfg = pca9482->iin_cc;

							/* Clear req_new_iin */
							mutex_lock(&pca9482->lock);
							pca9482->req_new_iin = false;
							mutex_unlock(&pca9482->lock);
							pr_info("%s: New IIN(3), iin_cc=%d, go to DC_STATE_PRESET_DC\n", __func__, pca9482->iin_cc);

							/* Go to DC_STATE_PRESET_DC */
							mutex_lock(&pca9482->lock);
							pca9482->timer_id = TIMER_PRESET_DC;
							pca9482->timer_period = DISABLE_DELAY_T;
							mutex_unlock(&pca9482->lock);
						}
					}
				} else {
					/* Current switching frequency is low frequency */
					/* New IIN is also low current */
					/* Don't need to change switching frequency */
					/* Update iin_cfg */
					pca9482->iin_cfg = pca9482->iin_cc;
					/* Set IIN_CFG to new IIN */
					ret = pca9482_set_input_current(pca9482, pca9482->iin_cc);
					if (ret < 0)
						goto error;

					/* Set ICHG_CFG to enough high value */
					if (pca9482->ichg_cfg < 2*pca9482->iin_cfg)
						pca9482->ichg_cfg = 2*pca9482->iin_cfg;
					ret = pca9482_set_charging_current(pca9482, pca9482->ichg_cfg);
					if (ret < 0)
						goto error;

					/* Clear Request flag */
					mutex_lock(&pca9482->lock);
					pca9482->req_new_iin = false;
					mutex_unlock(&pca9482->lock);

					/* Set new TA voltage and current */
					/* Read VBAT ADC */
					vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

					/* Calculate new TA maximum current and voltage that used in the direct charging */
					/* Set IIN_CC to MIN[IIN, TA_MAX_CUR*chg_mode]*/
					pca9482->iin_cc = MIN(pca9482->iin_cfg, pca9482->ta_max_cur*pca9482->chg_mode);
					/* Set the current IIN_CC to iin_cfg for recovering it after resolution adjustment */
					pca9482->iin_cfg = pca9482->iin_cc;
					/* Calculate new TA max voltage */
					/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
					val = pca9482->iin_cc/(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
					pca9482->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
					/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
					val = pca9482->ta_max_pwr/(pca9482->iin_cc/pca9482->chg_mode/1000); /* mV */
					val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
					val = val*PD_MSG_TA_VOL_STEP; /* uV */
					pca9482->ta_max_vol = MIN(val, TA_MAX_VOL*pca9482->chg_mode);

					/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
					pca9482->ta_vol = max(pca9482->pdata->ta_min_vol*pca9482->chg_mode, (2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET));
					val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
					pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;
					/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL] */
					pca9482->ta_vol = MIN(pca9482->ta_vol, pca9482->ta_max_vol);
					/* Set TA current to IIN_CC */
					pca9482->ta_cur = pca9482->iin_cc/pca9482->chg_mode;
					/* Recover IIN_CC to the original value(iin_cfg) */
					pca9482->iin_cc = pca9482->iin_cfg;

					pr_info("%s: New IIN(4), ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
						__func__, pca9482->ta_max_vol, pca9482->ta_max_cur, pca9482->ta_max_pwr, pca9482->iin_cc, pca9482->chg_mode);

					pr_info("%s: New IIN(4), ta_vol=%d, ta_cur=%d, sw_freq=%d\n",
						__func__, pca9482->ta_vol, pca9482->ta_cur, pca9482->fsw_cfg);

					/* Clear previous IIN ADC */
					pca9482->prev_iin = 0;
					/* Clear TA increment flag */
					pca9482->prev_inc = INC_NONE;

					/* Send PD Message and go to Adjust CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
					pca9482_set_charging_state(pca9482, DC_STATE_ADJUST_CC);
#else
					pca9482->charging_state = DC_STATE_ADJUST_CC;
#endif
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_PDMSG_SEND;
					pca9482->timer_period = IIN_CFG_WAIT_T;
					mutex_unlock(&pca9482->lock);
				}
			} else {
				/* New iin is lower than current iin_cc(iin_cfg) */
				/* Compare new IIN with IIN_LOW_TH */
				if (pca9482->iin_cc > pca9482->pdata->iin_low_freq) {
					/* New IIN is high current */
					/* Check current switching frequency */
					if (pca9482->fsw_cfg == pca9482->pdata->fsw_cfg) {
						/* Current switching frequency is high frequency */
						/* Don't need to change switching frequency */
						/* Calculate new TA_MAX_VOL */
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
						val = pca9482->iin_cc/(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
						pca9482->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
						/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
						val = pca9482->ta_max_pwr/(pca9482->iin_cc/pca9482->chg_mode/1000); /* mV */
						val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
						val = val*PD_MSG_TA_VOL_STEP; /* uV */
						pca9482->ta_max_vol = MIN(val, TA_MAX_VOL*pca9482->chg_mode);
						/* Recover IIN_CC to the original value(new_iin) */
						pca9482->iin_cc = pca9482->new_iin;

						/* Read VBAT ADC */
						vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
						/* Set TA voltage to 2*VBAT_ADC + offset */
						pca9482->ta_vol = 2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET;
						val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
						pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
						val = pca9482->iin_cc/PD_MSG_TA_CUR_STEP;
						pca9482->iin_cc = val*PD_MSG_TA_CUR_STEP;
						/* Set TA current to IIN_CC */
						pca9482->ta_cur = pca9482->iin_cc/pca9482->chg_mode;

						pr_info("%s: adj. cont1, ta_cur=%d, ta_vol=%d, ta_max_vol=%d, iin_cc=%d, chg_mode=%d\n",
								__func__, pca9482->ta_cur, pca9482->ta_vol, pca9482->ta_max_vol, pca9482->iin_cc, pca9482->chg_mode);

						/* Send PD Message */
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_PDMSG_SEND;
						pca9482->timer_period = 0;
						mutex_unlock(&pca9482->lock);
					} else {
						/* Current switching frequency is low frequency */
						/* Need to change switching frequency */
						/* Set switching frequency request sequence #1 */
						pca9482->req_sw_freq = REQ_SW_FREQ_1;
						pr_info("%s: adj. cont2, go to switching freq change sequence #1(high freq)\n", __func__);

						/* Set timer */
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_ADJUST_TACUR;
						pca9482->timer_period = 0;
						mutex_unlock(&pca9482->lock);
					}
				} else {
					/* New IIN is low current */
					/* Check current switching frequency */
					if (pca9482->fsw_cfg == pca9482->pdata->fsw_cfg_low) {
						/* Current switching frequency is low frequency */
						/* Don't need to change switching frequency */
						/* Calculate new TA_MAX_VOL */
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
						val = pca9482->iin_cc/(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
						pca9482->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
						/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
						val = pca9482->ta_max_pwr/(pca9482->iin_cc/pca9482->chg_mode/1000); /* mV */
						val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
						val = val*PD_MSG_TA_VOL_STEP; /* uV */
						pca9482->ta_max_vol = MIN(val, TA_MAX_VOL*pca9482->chg_mode);
						/* Recover IIN_CC to the original value(new_iin) */
						pca9482->iin_cc = pca9482->new_iin;

						/* Read VBAT ADC */
						vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
						/* Set TA voltage to 2*VBAT_ADC + offset */
						pca9482->ta_vol = 2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET;
						val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
						pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
						val = pca9482->iin_cc/PD_MSG_TA_CUR_STEP;
						pca9482->iin_cc = val*PD_MSG_TA_CUR_STEP;
						/* Set TA current to IIN_CC */
						pca9482->ta_cur = pca9482->iin_cc/pca9482->chg_mode;

						pr_info("%s: adj. cont3, ta_cur=%d, ta_vol=%d, ta_max_vol=%d, iin_cc=%d, chg_mode=%d\n",
								__func__, pca9482->ta_cur, pca9482->ta_vol, pca9482->ta_max_vol, pca9482->iin_cc, pca9482->chg_mode);

						/* Send PD Message */
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_PDMSG_SEND;
						pca9482->timer_period = 0;
						mutex_unlock(&pca9482->lock);
					} else {
						/* Current switching frequency is high frequency */
						/* Need to change switching frequency */
						/* Set switching frequency request sequence #1 */
						pca9482->req_sw_freq = REQ_SW_FREQ_1;
						pr_info("%s: adj. cont4, go to switching freq change sequence #1(low freq)\n", __func__);

						/* Set timer */
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_ADJUST_TACUR;
						pca9482->timer_period = 0;
						mutex_unlock(&pca9482->lock);
					} /* if (pca9482->fsw_cfg == pca9482->pdata->fsw_cfg_low) else */
				}  /* if (pca9482->iin_cc > pca9482->pdata->iin_low_freq) else */
			} /* if (pca9482->iin_cc > pca9482->iin_cfg) else */
		} /* if (pca9482->ta_cur == pca9482->iin_cc/pca9482->chg_mode) else */
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
	} /* if (pca9482->req_sw_freq != REQ_SW_FREQ_0) else */

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}


/* Set TA voltage for target current */
static int pca9482_adjust_ta_voltage(struct pca9482_charger *pca9482)
{
	int iin;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_ADJUST_TAVOL);
#else
	pca9482->charging_state = DC_STATE_ADJUST_TAVOL;
#endif
	/* Read IIN ADC */
	iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);

	/* Compare IIN ADC with targer input current */
	if (iin > (pca9482->iin_cc + PD_MSG_TA_CUR_STEP)) {
		/* TA current is higher than the target input current */
		/* Decrease TA voltage (20mV) */
		pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;

		pr_info("%s: adj. Cont1, ta_vol=%d\n", __func__, pca9482->ta_vol);

		/* Send PD Message */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
	} else {
		if (iin < (pca9482->iin_cc - PD_MSG_TA_CUR_STEP)) {
			/* TA current is lower than the target input current */
			/* Compare TA max voltage */
			if (pca9482->ta_vol == pca9482->ta_max_vol) {
				/* TA current is already the maximum voltage */
				/* Clear req_new_iin */
				mutex_lock(&pca9482->lock);
				pca9482->req_new_iin = false;
				mutex_unlock(&pca9482->lock);
				/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9482_set_charging_state(pca9482, pca9482->ret_state);
#else
				pca9482->charging_state = pca9482->ret_state;
#endif
				pr_info("%s: adj. End1, ta_cur=%d, ta_vol=%d, iin_cc=%d, chg_mode=%d\n",
						__func__, pca9482->ta_cur, pca9482->ta_vol, pca9482->iin_cc, pca9482->chg_mode);

				/* Go to return state  */
				/* Set timer */
				mutex_lock(&pca9482->lock);
				if (pca9482->charging_state == DC_STATE_CC_MODE)
					pca9482->timer_id = TIMER_CHECK_CCMODE;
				else
					pca9482->timer_id = TIMER_CHECK_CVMODE;
				pca9482->timer_period = 1000;	/* Wait 1000ms */
				mutex_unlock(&pca9482->lock);
			} else {
				/* Increase TA voltage (20mV) */
				pca9482->ta_vol = pca9482->ta_vol + PD_MSG_TA_VOL_STEP;

				pr_info("%s: adj. Cont2, ta_vol=%d\n", __func__, pca9482->ta_vol);

				/* Send PD Message */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_PDMSG_SEND;
				pca9482->timer_period = 0;
				mutex_unlock(&pca9482->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* Clear req_new_iin */
			mutex_lock(&pca9482->lock);
			pca9482->req_new_iin = false;
			mutex_unlock(&pca9482->lock);

			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_charging_state(pca9482, pca9482->ret_state);
#else
			pca9482->charging_state = pca9482->ret_state;
#endif
			pr_info("%s: adj. End2, ta_cur=%d, ta_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9482->ta_cur, pca9482->ta_vol, pca9482->iin_cc, pca9482->chg_mode);

			/* Go to return state  */
			/* Set timer */
			mutex_lock(&pca9482->lock);
			if (pca9482->charging_state == DC_STATE_CC_MODE)
				pca9482->timer_id = TIMER_CHECK_CCMODE;
			else
				pca9482->timer_id = TIMER_CHECK_CVMODE;
			pca9482->timer_period = 1000;	/* Wait 1000ms */
			mutex_unlock(&pca9482->lock);
		}
	}
	queue_delayed_work(pca9482->dc_wq,
					&pca9482->timer_work,
					msecs_to_jiffies(pca9482->timer_period));

	return 0;
}


/* Set RX voltage for target current */
static int pca9482_adjust_rx_voltage(struct pca9482_charger *pca9482)
{
	int iin;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_ADJUST_TAVOL);
#else
	pca9482->charging_state = DC_STATE_ADJUST_TAVOL;
#endif
	/* Read IIN ADC */
	iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);

	/* Compare IIN ADC with targer input current */
	if (iin > (pca9482->iin_cc + IIN_CC_COMP_OFFSET)) {
		/* RX current is higher than the target input current */
		/* Decrease RX voltage (100mV) */
		pca9482->ta_vol = pca9482->ta_vol - WCRX_VOL_STEP;

		pr_info("%s: adj. Cont1, rx_vol=%d\n", __func__, pca9482->ta_vol);

		/* Set RX voltage */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
	} else {
		if (iin < (pca9482->iin_cc - IIN_CC_COMP_OFFSET)) {
			/* RX current is lower than the target input current */
			/* Compare RX max voltage */
			if (pca9482->ta_vol == pca9482->ta_max_vol) {
				/* RX current is already the maximum voltage */
				/* Clear req_new_iin */
				mutex_lock(&pca9482->lock);
				pca9482->req_new_iin = false;
				mutex_unlock(&pca9482->lock);

				pr_info("%s: adj. End1(max vol), rx_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9482->ta_vol, pca9482->iin_cc, pca9482->chg_mode);

				/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9482_set_charging_state(pca9482, pca9482->ret_state);
#else
				pca9482->charging_state = pca9482->ret_state;
#endif
				/* Go to return state  */
				/* Set timer */
				mutex_lock(&pca9482->lock);
				if (pca9482->charging_state == DC_STATE_CC_MODE)
					pca9482->timer_id = TIMER_CHECK_CCMODE;
				else
					pca9482->timer_id = TIMER_CHECK_CVMODE;
				pca9482->timer_period = 1000;	/* Wait 1000ms */
				mutex_unlock(&pca9482->lock);
			} else {
				/* Increase RX voltage (100mV) */
				pca9482->ta_vol = pca9482->ta_vol + WCRX_VOL_STEP;
				if (pca9482->ta_vol > pca9482->ta_max_vol)
					pca9482->ta_vol = pca9482->ta_max_vol;

				pr_info("%s: adj. Cont2, rx_vol=%d\n", __func__, pca9482->ta_vol);

				/* Set RX voltage */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_PDMSG_SEND;
				pca9482->timer_period = 0;
				mutex_unlock(&pca9482->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* Clear req_new_iin */
			mutex_lock(&pca9482->lock);
			pca9482->req_new_iin = false;
			mutex_unlock(&pca9482->lock);

			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			pr_info("%s: adj. End2(valid), rx_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9482->ta_vol, pca9482->iin_cc, pca9482->chg_mode);

			/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_charging_state(pca9482, pca9482->ret_state);
#else
			pca9482->charging_state = pca9482->ret_state;
#endif
			/* Go to return state  */
			/* Set timer */
			mutex_lock(&pca9482->lock);
			if (pca9482->charging_state == DC_STATE_CC_MODE)
				pca9482->timer_id = TIMER_CHECK_CCMODE;
			else
				pca9482->timer_id = TIMER_CHECK_CVMODE;
			pca9482->timer_period = 1000;	/* Wait 1000ms */
			mutex_unlock(&pca9482->lock);
		}
	}
	queue_delayed_work(pca9482->dc_wq,
					&pca9482->timer_work,
					msecs_to_jiffies(pca9482->timer_period));

	return 0;
}

/* Set TA voltage for bypass mode */
static int pca9482_set_bypass_ta_voltage_by_soc(struct pca9482_charger *pca9482, int delta_soc)
{
	int ret = 0;
	unsigned int prev_ta_vol = pca9482->ta_vol;

	if (delta_soc < 0) { // increase soc (soc_now - ref_soc)
		pca9482->ta_vol += PD_MSG_TA_VOL_STEP;
	} else if (delta_soc > 0) { // decrease soc (soc_now - ref_soc)
		pca9482->ta_vol -= PD_MSG_TA_VOL_STEP;
	} else {
		pr_info("%s: abnormal delta_soc=%d\n", __func__, delta_soc);
		return -1;
	}

	pr_info("%s: delta_soc=%d, prev_ta_vol=%d, ta_vol=%d, ta_cur=%d\n",
		__func__, delta_soc, prev_ta_vol, pca9482->ta_vol, pca9482->ta_cur);

	/* Send PD Message */
	mutex_lock(&pca9482->lock);
	pca9482->timer_id = TIMER_PDMSG_SEND;
	pca9482->timer_period = 0;
	mutex_unlock(&pca9482->lock);
	schedule_delayed_work(&pca9482->timer_work, msecs_to_jiffies(pca9482->timer_period));

	return ret;
}

/* Set TA current for bypass mode */
static int pca9482_set_bypass_ta_current(struct pca9482_charger *pca9482)
{
	int ret = 0;
	unsigned int val;

	/* Set charging state to BYPASS mode state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_BYPASS_MODE);
#else
	pca9482->charging_state = DC_STATE_BYPASS_MODE;
#endif
	pr_info("%s: new_iin=%d\n", __func__, pca9482->new_iin);

	/* Set IIN_CFG to new_IIN */
	pca9482->iin_cfg = pca9482->new_iin;
	pca9482->iin_cc = pca9482->new_iin;
	ret = pca9482_set_input_current(pca9482, pca9482->iin_cc);
	if (ret < 0)
		goto error;

	/* Clear Request flag */
	mutex_lock(&pca9482->lock);
	pca9482->req_new_iin = false;
	mutex_unlock(&pca9482->lock);

	/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
	val = pca9482->iin_cc/PD_MSG_TA_CUR_STEP;
	pca9482->iin_cc = val*PD_MSG_TA_CUR_STEP;
	/* Set TA current to IIN_CC */
	pca9482->ta_cur = pca9482->iin_cc/pca9482->chg_mode;

	pr_info("%s: ta_cur=%d, ta_vol=%d\n", __func__, pca9482->ta_cur, pca9482->ta_vol);

	/* Recover IIN_CC to the original value(new_iin) */
	pca9482->iin_cc = pca9482->new_iin;

	/* Send PD Message */
	mutex_lock(&pca9482->lock);
	pca9482->timer_id = TIMER_PDMSG_SEND;
	pca9482->timer_period = 0;
	mutex_unlock(&pca9482->lock);
	queue_delayed_work(pca9482->dc_wq,
				&pca9482->timer_work,
				msecs_to_jiffies(pca9482->timer_period));

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set TA voltage for bypass mode */
static int pca9482_set_bypass_ta_voltage(struct pca9482_charger *pca9482)
{
	int ret = 0;
	unsigned int val;
	int vbat;

	/* Set charging state to BYPASS mode state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_BYPASS_MODE);
#else
	pca9482->charging_state = DC_STATE_BYPASS_MODE;
#endif
	pr_info("%s: new_vfloat=%d\n", __func__, pca9482->new_vfloat);

	/* Set VFLOAT to new vfloat */
	pca9482->vfloat = pca9482->new_vfloat;
	ret = pca9482_set_vfloat(pca9482, pca9482->vfloat);
	if (ret < 0)
		goto error;

	/* Clear Request flag */
	mutex_lock(&pca9482->lock);
	pca9482->req_new_vfloat = false;
	mutex_unlock(&pca9482->lock);


	/* It needs to optimize TA voltage as calculating TA voltage with battery voltage later */
	/* Read VBAT ADC */
	vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

	/* Check DC mode */
	if (pca9482->dc_mode == PTM_1TO1) {
		/* Set TA voltage to VBAT_ADC + Offset */
		val = vbat + TA_VOL_OFFSET_1TO1_BYPASS;
	} else {
		/* Set TA voltage to 2*VBAT_ADC + Offset */
		val = 2*vbat + TA_VOL_OFFSET_2TO1_BYPASS;
	}
	pca9482->ta_vol = val;

	pr_info("%s: vbat=%d, ta_vol=%d, ta_cur=%d\n",
			__func__, vbat, pca9482->ta_vol, pca9482->ta_cur);

	/* Send PD Message */
	mutex_lock(&pca9482->lock);
	pca9482->timer_id = TIMER_PDMSG_SEND;
	pca9482->timer_period = 0;
	mutex_unlock(&pca9482->lock);
	queue_delayed_work(pca9482->dc_wq,
				&pca9482->timer_work,
				msecs_to_jiffies(pca9482->timer_period));

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set new input current  */
static int pca9482_set_new_iin(struct pca9482_charger *pca9482)
{
	int ret = 0;

	pr_info("%s: new_iin=%d\n", __func__, pca9482->new_iin);

	/* Check current DC mode */
	if ((pca9482->dc_mode == PTM_1TO1) ||
		(pca9482->dc_mode == PTM_2TO1)) {
		/* DC mode is bypass mode */
		/* Set new iin for bypass mode */
		ret = pca9482_set_bypass_ta_current(pca9482);
	} else {
		/* DC mode is normal mode */
		/* Set new IIN to IIN_CC */
		pca9482->iin_cc = pca9482->new_iin;
		/* Save return state */
		pca9482->ret_state = pca9482->charging_state;

		/* Check the TA type first */
		if (pca9482->ta_type == TA_TYPE_WIRELESS) {
			/* Wireless Charger is connected */
			ret = pca9482_adjust_rx_voltage(pca9482);
		} else {
			/* USBPD TA is connected */
			/* Check new IIN with the minimum TA current */
			if (pca9482->iin_cc < (TA_MIN_CUR * pca9482->chg_mode)) {
				/* Set the TA current to TA_MIN_CUR(1.0A) */
				pca9482->ta_cur = TA_MIN_CUR;
				/* Need to control TA voltage for request current */
				ret = pca9482_adjust_ta_voltage(pca9482);
			} else {
				/* Need to control TA current for request current */
				ret = pca9482_adjust_ta_current(pca9482);
			}
		}
	}

	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}


/* Set new float voltage */
static int pca9482_set_new_vfloat(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int vbat;
	unsigned int val;

	/* Check current DC mode */
	if ((pca9482->dc_mode == PTM_1TO1) ||
		(pca9482->dc_mode == PTM_2TO1)) {
		/* DC mode is bypass mode */
		/* Set new vfloat for bypass mode */
		ret = pca9482_set_bypass_ta_voltage(pca9482);
	} else {
		/* DC mode is normal mode */
		/* Read VBAT ADC */
		vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

		/* Compare the new VBAT with present vfloat */
		if (pca9482->new_vfloat > pca9482->vfloat) {
			/* cancel delayed_work */
			cancel_delayed_work(&pca9482->timer_work);

			/* Set vfloat decrement flag to false */
			pca9482->dec_vfloat = false;

			/* Set VFLOAT to new vfloat */
			pca9482->vfloat = pca9482->new_vfloat;
			/* Check VFLOAT method */
			if (pca9482->pdata->fg_vfloat == true) {
				/* Set PCA9482 VFLOAT to default value */
				ret = pca9482_set_vfloat(pca9482, pca9482->pdata->dft_vfloat);
			} else {
				/* Set PCA9482 VFLOAT to new vfloat */
				ret = pca9482_set_vfloat(pca9482, pca9482->vfloat);
			}
			if (ret < 0)
				goto error;
			/* Set IIN_CFG to the current IIN_CC */
			/* save the current iin_cc in iin_cfg */
			pca9482->iin_cfg = pca9482->iin_cc;
			pca9482->iin_cfg = MIN(pca9482->iin_cfg, pca9482->ta_max_cur*pca9482->chg_mode);
			ret = pca9482_set_input_current(pca9482, pca9482->iin_cfg);
			if (ret < 0)
				goto error;

			pca9482->iin_cc = pca9482->iin_cfg;

			/* Set ICHG_CFG to enough high value */
			if (pca9482->ichg_cfg < 2*pca9482->iin_cfg)
				pca9482->ichg_cfg = 2*pca9482->iin_cfg;
			ret = pca9482_set_charging_current(pca9482, pca9482->ichg_cfg);
			if (ret < 0)
				goto error;

			/* Clear req_new_vfloat */
			mutex_lock(&pca9482->lock);
			pca9482->req_new_vfloat = false;
			mutex_unlock(&pca9482->lock);

			/* Check the TA type */
			if (pca9482->ta_type == TA_TYPE_WIRELESS) {
				/* Wireless Charger is connected */
				/* Set RX voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
				pca9482->ta_vol = max(pca9482->pdata->ta_min_vol*pca9482->chg_mode, (2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET));
				val = pca9482->ta_vol/WCRX_VOL_STEP;	/* RX voltage resolution is 100mV */
				pca9482->ta_vol = val*WCRX_VOL_STEP;
				/* Set RX voltage to MIN[RX voltage, RX_MAX_VOL] */
				pca9482->ta_vol = MIN(pca9482->ta_vol, pca9482->ta_max_vol);

				pr_info("%s: New VFLOAT, rx_max_vol=%d, rx_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9482->ta_max_vol, pca9482->ta_vol, pca9482->iin_cc, pca9482->chg_mode);
			} else {
				/* USBPD TA is connected */
				/* Calculate new TA maximum voltage that used in the direct charging */
				/* Calculate new TA max voltage */
				/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
				val = pca9482->iin_cc/(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
				pca9482->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9482->chg_mode);
				/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
				val = pca9482->ta_max_pwr/(pca9482->iin_cc/pca9482->chg_mode/1000); /* mV */
				val = val*1000/PD_MSG_TA_VOL_STEP;	/* uV */
				val = val*PD_MSG_TA_VOL_STEP; /* Adjust values with APDO resolution(20mV) */
				pca9482->ta_max_vol = MIN(val, TA_MAX_VOL*pca9482->chg_mode);

				/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
				pca9482->ta_vol = max(pca9482->pdata->ta_min_vol*pca9482->chg_mode, (2*vbat*pca9482->chg_mode + TA_VOL_PRE_OFFSET));
				val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
				pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;
				/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL] */
				pca9482->ta_vol = MIN(pca9482->ta_vol, pca9482->ta_max_vol);
				/* Set TA current to IIN_CC */
				pca9482->ta_cur = pca9482->iin_cc/pca9482->chg_mode;
				/* Recover IIN_CC to the original value(iin_cfg) */
				pca9482->iin_cc = pca9482->iin_cfg;

				pr_info("%s: New VFLOAT, ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9482->ta_max_vol, pca9482->ta_max_cur, pca9482->ta_max_pwr, pca9482->iin_cc, pca9482->chg_mode);
			}

			/* Clear previous IIN ADC */
			pca9482->prev_iin = 0;
			/* Clear TA increment flag */
			pca9482->prev_inc = INC_NONE;

			/* Send PD Message and go to Adjust CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_charging_state(pca9482, DC_STATE_ADJUST_CC);
#else
			pca9482->charging_state = DC_STATE_ADJUST_CC;
#endif
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
		} else if (pca9482->new_vfloat == pca9482->vfloat) {
			/* New vfloat is same as the present vfloat */
			/* Don't need any setting */
			/* cancel delayed_work */
			cancel_delayed_work(&pca9482->timer_work);

			/* Clear req_new_vfloat */
			mutex_lock(&pca9482->lock);
			pca9482->req_new_vfloat = false;
			mutex_unlock(&pca9482->lock);

			/* Go to the present state */
			pr_info("%s: New vfloat is same as present vfloat and go to the present state\n", __func__);

			/* Set timer */
			mutex_lock(&pca9482->lock);
			if (pca9482->charging_state == DC_STATE_CC_MODE)
				pca9482->timer_id = TIMER_CHECK_CCMODE;
			else
				pca9482->timer_id = TIMER_CHECK_CVMODE;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
		} else {
			/* The new vfloat is lower than present vfloat */
			/* cancel delayed_work */
			cancel_delayed_work(&pca9482->timer_work);

			/* Set vfloat decrement flag */
			pca9482->dec_vfloat = true;

			/* Set VFLOAT to new vfloat */
			pca9482->vfloat = pca9482->new_vfloat;
			/* Check VFLOAT method */
			if (pca9482->pdata->fg_vfloat == true) {
				/* Set PCA9482 VFLOAT to default value */
				ret = pca9482_set_vfloat(pca9482, pca9482->pdata->dft_vfloat);
			} else {
				/* Set PCA9482 VFLOAT to new vfloat */
				ret = pca9482_set_vfloat(pca9482, pca9482->vfloat);
			}
			if (ret < 0)
				goto error;

			/* Clear req_new_vfloat */
			mutex_lock(&pca9482->lock);
			pca9482->req_new_vfloat = false;
			mutex_unlock(&pca9482->lock);

			pr_info("%s: New vfloat is lower than present vfloat and go to Pre-CV state\n", __func__);

			/* Go to Pre-CV mode */
			pca9482->charging_state = DC_STATE_START_CV;
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_ENTER_CVMODE;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
		}
	}

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set new direct charging mode */
static int pca9482_set_new_dc_mode(struct pca9482_charger *pca9482)
{
	int ret = 0;
	unsigned int val;
	int vbat;

	pr_info("%s: ======START=======\n", __func__);

	/* Read VBAT ADC */
	vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

	/* Check new dc mode */
	switch (pca9482->new_dc_mode) {
	case PTM_1TO1:
	case PTM_2TO1:
		/* Change normal mode to 1:1 or 2:1 bypass mode */
		/* Check current dc mode */
		if ((pca9482->dc_mode == PTM_1TO1) ||
			(pca9482->dc_mode == PTM_2TO1)) {
			/* TA voltage already changed to 1:1 or 2:1 mode */
			/* Disable reverse current detection */
			val = 0;
			ret = pca9482_update_reg(pca9482, PCA9482_REG_RCP_CNTL,
									PCA9482_BIT_RCP_EN, val);
			if (ret < 0)
				goto error;

			/* Set FSW_CFG to fsw_cfg_byp */
			val = PCA9482_FSW_CFG(pca9482->pdata->fsw_cfg_byp);
			ret = pca9482_write_reg(pca9482, PCA9482_REG_SC_CNTL_0, val);
			if (ret < 0)
				goto error;
			pr_info("%s: New DC mode, BYP mode=%d, sw_freq=%dkHz\n",
					__func__, pca9482->dc_mode, pca9482->pdata->fsw_cfg_byp);

			/* Enable Charging - recover dc as 1:1 or 2:1 bypass mode */
			ret = pca9482_set_charging(pca9482, true);
			if (ret < 0)
				goto error;

			/* Clear request flag */
			mutex_lock(&pca9482->lock);
			pca9482->req_new_dc_mode = false;
			mutex_unlock(&pca9482->lock);

			pr_info("%s: New DC mode, Normal->BYP mode(%d) done\n", __func__, pca9482->dc_mode);

			/* Wait 500ms and go to bypass state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_charging_state(pca9482, DC_STATE_BYPASS_MODE);
#else
			pca9482->charging_state = DC_STATE_BYPASS_MODE;
#endif
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_BYPASSMODE;
			pca9482->timer_period = BYPASS_WAIT_T;	/* 200ms */
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
		} else {
			/* DC mode is normal mode */
			/* TA voltage is not changed to 1:1 or 2:1 bypass mode yet */
			/* Disable charging */
			ret = pca9482_set_charging(pca9482, false);
			if (ret < 0)
				goto error;
			/* Set dc mode to new dc mode, 1:1 or 2:1 bypass mode */
			pca9482->dc_mode = pca9482->new_dc_mode;
			if (pca9482->dc_mode == PTM_2TO1) {
				/* Set TA voltage to 2:1 bypass voltage */
				pca9482->ta_vol = 2*vbat + TA_VOL_OFFSET_2TO1_BYPASS;
				pr_info("%s: New DC mode, Normal->2:1BYP, ta_vol=%d, ta_cur=%d\n",
						__func__, pca9482->ta_vol, pca9482->ta_cur);
			} else {
				/* Set TA voltage to 1:1 voltage */
				pca9482->ta_vol = vbat + TA_VOL_OFFSET_1TO1_BYPASS;
				pr_info("%s: New DC mode, Normal->1:1BYP, ta_vol=%d, ta_cur=%d\n",
						__func__, pca9482->ta_vol, pca9482->ta_cur);
			}

			/* Send PD Message and go to dcmode change state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_charging_state(pca9482, DC_STATE_DCMODE_CHANGE);
#else
			pca9482->charging_state = DC_STATE_DCMODE_CHANGE;
#endif
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
		}
		break;

	case PTM_NONE:
		/* Change bypass mode to normal mode */
		/* Disable charging */
		ret = pca9482_set_charging(pca9482, false);
		if (ret < 0)
			goto error;

		/* Enable reverse current detection */
		val = PCA9482_BIT_RCP_EN;
		ret = pca9482_update_reg(pca9482, PCA9482_REG_RCP_CNTL,
								PCA9482_BIT_RCP_EN, val);
		if (ret < 0)
			goto error;

		/* Set dc mode to new dc mode, normal mode */
		pca9482->dc_mode = pca9482->new_dc_mode;
		/* Clear request flag */
		mutex_lock(&pca9482->lock);
		pca9482->req_new_dc_mode = false;
		mutex_unlock(&pca9482->lock);
		pr_info("%s: New DC mode, BYP->Normal\n", __func__);

		/* Go to DC_STATE_PRESET_DC */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PRESET_DC;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		break;

	default:
		ret = -EINVAL;
		pr_info("%s: New DC mode, Invalid mode=%d\n", __func__, pca9482->new_dc_mode);
		break;
	}

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging Adjust CC MODE control */
static int pca9482_charge_adjust_ccmode(struct pca9482_charger *pca9482)
{
	int iin, ichg, vbatt, ccmode;
	int val;
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_ADJUST_CC);
#else
	pca9482->charging_state = DC_STATE_ADJUST_CC;
#endif

	ret = pca9482_check_error(pca9482);
	if (ret != 0)
		goto error; // This is not active mode.
	/* Check the status */
	ccmode = pca9482_check_dcmode_status(pca9482);
	if (ccmode < 0) {
		ret = ccmode;
		goto error;
	}

	switch (ccmode) {
	case DCMODE_IIN_LOOP:
	case DCMODE_CHG_LOOP:
		/* Read IIN ADC */
		iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
		/* Read ICHG ADC */
		ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);
		/* Read VBAT ADC */
		vbatt = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

		/* Check the TA type first */
		if (pca9482->ta_type == TA_TYPE_WIRELESS) {
			/* Decrease RX voltage (100mV) */
			pca9482->ta_vol = pca9482->ta_vol - WCRX_VOL_STEP;
			pr_info("%s: CC adjust End(LOOP): iin=%d, ichg=%d, vbatt=%d, rx_vol=%d\n",
					__func__, iin, ichg, vbatt, pca9482->ta_vol);

			/* Set TA target voltage to TA voltage */
			pca9482->ta_target_vol = pca9482->ta_vol;
			/* Clear TA increment flag */
			pca9482->prev_inc = INC_NONE;
			/* Send PD Message and then go to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_charging_state(pca9482, DC_STATE_CC_MODE);
#else
			pca9482->charging_state = DC_STATE_CC_MODE;
#endif
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
		} else {
			/* Check TA current */
			if ((pca9482->ta_cur > TA_MIN_CUR) &&
				(pca9482->ta_ctrl == TA_CTRL_CL_MODE)) {
				/* TA current is higher than 1.0A */
				/* Decrease TA current (50mA) */
				pca9482->ta_cur = pca9482->ta_cur - PD_MSG_TA_CUR_STEP;

				/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*CHG_mode + 100mV */
				val = pca9482->ta_vol + (pca9482->vfloat - vbatt)*2*pca9482->chg_mode + 100000;
				val = val/PD_MSG_TA_VOL_STEP;
				pca9482->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
				if (pca9482->ta_target_vol > pca9482->ta_max_vol)
					pca9482->ta_target_vol = pca9482->ta_max_vol;
				pr_info("%s: CC adjust End(LOOP): iin=%d, ichg=%d, vbatt=%d, ta_cur=%d, ta_vol=%d, ta_target_vol=%d\n",
						__func__, iin, ichg, vbatt, pca9482->ta_cur, pca9482->ta_vol, pca9482->ta_target_vol);
				/* Clear TA increment flag */
				pca9482->prev_inc = INC_NONE;
				/* Go to Start CC mode */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_ENTER_CCMODE;
				pca9482->timer_period = 0;
				mutex_unlock(&pca9482->lock);
			} else {
				/* Decrease TA voltage (20mV) */
				pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;

				/* Set TA target voltage to TA voltage */
				pca9482->ta_target_vol = pca9482->ta_vol;
				/* Clear TA increment flag */
				pca9482->prev_inc = INC_NONE;
				/* Send PD Message and then go to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9482_set_charging_state(pca9482, DC_STATE_CC_MODE);
#else
				pca9482->charging_state = DC_STATE_CC_MODE;
#endif
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_PDMSG_SEND;
				pca9482->timer_period = 0;
				mutex_unlock(&pca9482->lock);

				pr_info("%s: CC adjust End(LOOP): iin=%d, ichg=%d, vbatt=%d, ta_cur=%d, ta_vol=%d\n",
						__func__, iin, ichg, vbatt, pca9482->ta_cur, pca9482->ta_vol);
			}
		}
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		break;

	case DCMODE_VFLT_LOOP:
		/* Read IIN ADC */
		iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
		/* Read ICHG ADC */
		ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);
		/* Read VBAT ADC */
		vbatt = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
		pr_info("%s: CC adjust End(VFLOAT): vbatt=%d, iin=%d, ichg=%d, ta_vol=%d\n",
				__func__, vbatt, iin, ichg, pca9482->ta_vol);

		/* Save TA target voltage*/
		pca9482->ta_target_vol = pca9482->ta_vol;
		/* Clear TA increment flag */
		pca9482->prev_inc = INC_NONE;
		/* Go to Pre-CV mode */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_ENTER_CVMODE;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		break;

	case DCMODE_LOOP_INACTIVE:
		/* Check IIN ADC with IIN */
		iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
		/* Check ICHG ADC */
		ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);
		/* Read VBAT ADC */
		vbatt = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

		/* Check the TA type first */
		if (pca9482->ta_type == TA_TYPE_WIRELESS) {
			/* IIN_ADC > IIN_CC -20mA ? */
			if (iin > (pca9482->iin_cc - IIN_ADC_OFFSET)) {
				/* Input current is already over IIN_CC */
				/* End RX voltage adjustment */
				/* change charging state to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9482_set_charging_state(pca9482, DC_STATE_CC_MODE);
#else
				pca9482->charging_state = DC_STATE_CC_MODE;
#endif
				pr_info("%s: CC adjust End: iin=%d, ichg=%d, vbatt=%d, rx_vol=%d\n",
						__func__, iin, ichg, vbatt, pca9482->ta_vol);

				/* Save TA target voltage*/
				pca9482->ta_target_vol = pca9482->ta_vol;
				/* Clear TA increment flag */
				pca9482->prev_inc = INC_NONE;
				/* Go to CC mode */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_CHECK_CCMODE;
				pca9482->timer_period = 0;
				mutex_unlock(&pca9482->lock);
			} else {
				/* Check RX voltage */
				if (pca9482->ta_vol == pca9482->ta_max_vol) {
					/* RX voltage is already max value */
					pr_info("%s: CC adjust End: MAX value, iin=%d, ichg=%d, vbatt=%d, rx_vol=%d\n",
							__func__, iin, ichg, vbatt, pca9482->ta_vol);

					/* Save TA target voltage*/
					pca9482->ta_target_vol = pca9482->ta_vol;
					/* Clear TA increment flag */
					pca9482->prev_inc = INC_NONE;
					/* Go to CC mode */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_CHECK_CCMODE;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);
				} else {
					/* Try to increase RX voltage(100mV) */
					pca9482->ta_vol = pca9482->ta_vol + WCRX_VOL_STEP;
					if (pca9482->ta_vol > pca9482->ta_max_vol)
						pca9482->ta_vol = pca9482->ta_max_vol;
					pr_info("%s: CC adjust Cont: iin=%d, ichg=%d, vbatt=%d, rx_vol=%d\n",
							__func__, iin, ichg, vbatt, pca9482->ta_vol);
					/* Set RX voltage */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_PDMSG_SEND;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);
				}
			}
		} else {
			/* USBPD TA is connected */

			/* IIN_ADC > IIN_CC -20mA ? */
			if (iin > (pca9482->iin_cc - IIN_ADC_OFFSET)) {
				if (pca9482->ta_ctrl == TA_CTRL_CL_MODE) {
					/* TA control method is CL mode */
					/* Input current is already over IIN_CC */
					/* End TA voltage and current adjustment */
					/* change charging state to Start CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
					pca9482_set_charging_state(pca9482, DC_STATE_START_CC);
#else
					pca9482->charging_state = DC_STATE_START_CC;
#endif

					pr_info("%s: CC adjust End(Normal): iin=%d, ichg=%d, vbatt=%d, ta_vol=%d, ta_cur=%d\n",
							__func__, iin, ichg, vbatt, pca9482->ta_vol, pca9482->ta_cur);

					/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*CHG_mode + 100mV */
					val = pca9482->ta_vol + (pca9482->vfloat - vbatt)*2*pca9482->chg_mode + 100000;
					val = val/PD_MSG_TA_VOL_STEP;
					pca9482->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
					if (pca9482->ta_target_vol > pca9482->ta_max_vol)
						pca9482->ta_target_vol = pca9482->ta_max_vol;
					pr_info("%s: CC adjust End: ta_target_vol=%d\n", __func__, pca9482->ta_target_vol);
					/* Clear TA increment flag */
					pca9482->prev_inc = INC_NONE;
					/* Go to Start CC mode */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_ENTER_CCMODE;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);
				} else {
					/* TA control method is CV mode */
					pr_info("%s: CC adjust End(Normal,CV): iin=%d, ichg=%d, vbatt=%d, ta_vol=%d, ta_cur=%d\n",
							__func__, iin, ichg, vbatt, pca9482->ta_vol, pca9482->ta_cur);
					/* Save TA target voltage */
					pca9482->ta_target_vol = pca9482->ta_vol;
					pr_info("%s: CC adjust End(Normal): ta_ctrl=%d, ta_target_vol=%d\n",
							__func__, pca9482->ta_ctrl, pca9482->ta_target_vol);
					/* Clear TA increment flag */
					pca9482->prev_inc = INC_NONE;
					/* Go to CC mode */
					mutex_lock(&pca9482->lock);
					pca9482->timer_id = TIMER_CHECK_CCMODE;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);
				}
			} else {
				/* Compare TA maximum voltage */
				if (pca9482->ta_vol == pca9482->ta_max_vol) {
					/* Compare TA maximum current */
					if (pca9482->ta_cur == pca9482->ta_max_cur) {
						/* TA voltage and current are already max value */
						pr_info("%s: CC adjust End(MAX_VOL/CUR): iin=%d, ichg=%d, ta_vol=%d, ta_cur=%d\n",
								__func__, iin, ichg, pca9482->ta_vol, pca9482->ta_cur);
						/* Save TA target voltage */
						pca9482->ta_target_vol = pca9482->ta_vol;
						/* Clear TA increment flag */
						pca9482->prev_inc = INC_NONE;
						/* Go to CC mode */
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_CHECK_CCMODE;
						pca9482->timer_period = 0;
						mutex_unlock(&pca9482->lock);
					} else {
						/* TA current is not maximum value */
						/* Increase TA current(50mA) */
						pca9482->ta_cur = pca9482->ta_cur + PD_MSG_TA_CUR_STEP;
						if (pca9482->ta_cur > pca9482->ta_max_cur)
							pca9482->ta_cur = pca9482->ta_max_cur;
						pr_info("%s: CC adjust Cont(1): iin=%d, ichg=%d, ta_cur=%d\n",
								__func__, iin, ichg, pca9482->ta_cur);
						/* Set TA increment flag */
						pca9482->prev_inc = INC_TA_CUR;
						/* Send PD Message */
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_PDMSG_SEND;
						pca9482->timer_period = 0;
						mutex_unlock(&pca9482->lock);
					}
				} else {
					/* Check TA tolerance */
					/* The current input current compares the final input current(IIN_CC) with 100mA offset */
					/* PPS current tolerance has +/-150mA, so offset defined 100mA(tolerance +50mA) */
					if (iin < (pca9482->iin_cc - TA_IIN_OFFSET)) {
						/* TA voltage too low to enter TA CC mode, so we should increae TA voltage */
						pca9482->ta_vol = pca9482->ta_vol + TA_VOL_STEP_ADJ_CC*pca9482->chg_mode;
						if (pca9482->ta_vol > pca9482->ta_max_vol)
							pca9482->ta_vol = pca9482->ta_max_vol;
						pr_info("%s: CC adjust Cont(2): iin=%d, ichg=%d, ta_vol=%d\n",
								__func__, iin, ichg, pca9482->ta_vol);
						/* Set TA increment flag */
						pca9482->prev_inc = INC_TA_VOL;
						/* Send PD Message */
						mutex_lock(&pca9482->lock);
						pca9482->timer_id = TIMER_PDMSG_SEND;
						pca9482->timer_period = 0;
						mutex_unlock(&pca9482->lock);
					} else {
						/* compare IIN ADC with previous IIN ADC + 20mA */
						if (iin > (pca9482->prev_iin + IIN_ADC_OFFSET)) {
							/* TA can supply more current if TA voltage is high */
							/* TA voltage too low to enter TA CC mode, so we should increae TA voltage */
							pca9482->ta_vol = pca9482->ta_vol + TA_VOL_STEP_ADJ_CC*pca9482->chg_mode;
							if (pca9482->ta_vol > pca9482->ta_max_vol)
								pca9482->ta_vol = pca9482->ta_max_vol;
							pr_info("%s: CC adjust Cont(3): iin=%d, ichg=%d, ta_vol=%d\n",
									__func__, iin, ichg, pca9482->ta_vol);
							/* Set TA increment flag */
							pca9482->prev_inc = INC_TA_VOL;
							/* Send PD Message */
							mutex_lock(&pca9482->lock);
							pca9482->timer_id = TIMER_PDMSG_SEND;
							pca9482->timer_period = 0;
							mutex_unlock(&pca9482->lock);
						} else {
							/* Check the previous increment */
							if (pca9482->prev_inc == INC_TA_CUR) {
								/* The previous increment is TA current, but input current does not increase */
								/* Try to increase TA voltage(40mV) */
								pca9482->ta_vol = pca9482->ta_vol + TA_VOL_STEP_ADJ_CC*pca9482->chg_mode;
								if (pca9482->ta_vol > pca9482->ta_max_vol)
									pca9482->ta_vol = pca9482->ta_max_vol;
								pr_info("%s: CC adjust Cont(4): iin=%d, ichg=%d, ta_vol=%d\n",
										__func__, iin, ichg, pca9482->ta_vol);
								/* Set TA increment flag */
								pca9482->prev_inc = INC_TA_VOL;
								/* Send PD Message */
								mutex_lock(&pca9482->lock);
								pca9482->timer_id = TIMER_PDMSG_SEND;
								pca9482->timer_period = 0;
								mutex_unlock(&pca9482->lock);
							} else {
								/* The previous increment is TA voltage, but input current does not increase */
								/* Try to increase TA current */
								/* Check APDO max current */
								if (pca9482->ta_cur == pca9482->ta_max_cur) {
									if (pca9482->ta_ctrl == TA_CTRL_CL_MODE) {
										/* Current TA control method is CL mode */
										/* TA current is maximum current */
										pr_info("%s: CC adjust End(MAX_CUR): iin=%d, ichg=%d, ta_vol=%d, ta_cur=%d\n",
												__func__, iin, ichg, pca9482->ta_vol, pca9482->ta_cur);

										/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*CHG_mode + 100mV */
										val = pca9482->ta_vol + (pca9482->vfloat - vbatt)*2*pca9482->chg_mode + 100000;
										val = val/PD_MSG_TA_VOL_STEP;
										pca9482->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
										if (pca9482->ta_target_vol > pca9482->ta_max_vol)
											pca9482->ta_target_vol = pca9482->ta_max_vol;
										pr_info("%s: CC adjust End: ta_target_vol=%d\n", __func__, pca9482->ta_target_vol);

										/* Clear TA increment flag */
										pca9482->prev_inc = INC_NONE;
										/* Go to Start CC mode */
										mutex_lock(&pca9482->lock);
										pca9482->timer_id = TIMER_ENTER_CCMODE;
										pca9482->timer_period = 0;
										mutex_unlock(&pca9482->lock);
									} else {
										/* Current TA control method is CV mode */
										pr_info("%s: CC adjust End(MAX_CUR,CV): iin=%d, ichg=%d, ta_vol=%d, ta_cur=%d\n",
												__func__, iin, ichg, pca9482->ta_vol, pca9482->ta_cur);
										/* Save TA target voltage */
										pca9482->ta_target_vol = pca9482->ta_vol;
										pr_info("%s: CC adjust End(Normal): ta_ctrl=%d, ta_target_vol=%d\n",
												__func__, pca9482->ta_ctrl, pca9482->ta_target_vol);
										/* Clear TA increment flag */
										pca9482->prev_inc = INC_NONE;
										/* Go to CC mode */
										mutex_lock(&pca9482->lock);
										pca9482->timer_id = TIMER_CHECK_CCMODE;
										pca9482->timer_period = 0;
										mutex_unlock(&pca9482->lock);
									}
								} else {
									/* Check the present TA current */
									/* Consider tolerance offset(100mA) */
									if (pca9482->ta_cur >= (pca9482->iin_cc + TA_IIN_OFFSET)) {
										/* TA voltage increment has high priority than TA current increment */
										/* Try to increase TA voltage(40mV) */
										pca9482->ta_vol = pca9482->ta_vol + TA_VOL_STEP_ADJ_CC*pca9482->chg_mode;
										if (pca9482->ta_vol > pca9482->ta_max_vol)
											pca9482->ta_vol = pca9482->ta_max_vol;
										pr_info("%s: CC adjust Cont(5): iin=%d, ichg=%d, ta_vol=%d\n",
												__func__, iin, ichg, pca9482->ta_vol);
										/* Set TA increment flag */
										pca9482->prev_inc = INC_TA_VOL;
										/* Send PD Message */
										mutex_lock(&pca9482->lock);
										pca9482->timer_id = TIMER_PDMSG_SEND;
										pca9482->timer_period = 0;
										mutex_unlock(&pca9482->lock);
									} else {
										/* TA has tolerance and compensate it as real current */
										/* Increase TA current(50mA) */
										pca9482->ta_cur = pca9482->ta_cur + PD_MSG_TA_CUR_STEP;
										if (pca9482->ta_cur > pca9482->ta_max_cur)
											pca9482->ta_cur = pca9482->ta_max_cur;
										pr_info("%s: CC adjust Cont(6): iin=%d, ichg=%d, ta_cur=%d\n",
												__func__, iin, ichg, pca9482->ta_cur);
										/* Set TA increment flag */
										pca9482->prev_inc = INC_TA_CUR;
										/* Send PD Message */
										mutex_lock(&pca9482->lock);
										pca9482->timer_id = TIMER_PDMSG_SEND;
										pca9482->timer_period = 0;
										mutex_unlock(&pca9482->lock);
									}
								}
							}
						}
					}
				}
			}
		}
		/* Save previous iin adc */
		pca9482->prev_iin = iin;
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		break;

	default:
		break;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging Start CC MODE control - Pre CC MODE */
/* Increase TA voltage to TA target voltage */
static int pca9482_charge_start_ccmode(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int ccmode;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_START_CC);
#else
	pca9482->charging_state = DC_STATE_START_CC;
#endif
	ret = pca9482_check_error(pca9482);
	if (ret != 0)
		goto error; // This is not active mode.

	/* Check the status */
	ccmode = pca9482_check_dcmode_status(pca9482);
	if (ccmode < 0) {
		ret = ccmode;
		goto error;
	}

	/* Increase TA voltage */
	pca9482->ta_vol = pca9482->ta_vol + TA_VOL_STEP_PRE_CC * pca9482->chg_mode;
	/* Check TA target voltage */
	if (pca9482->ta_vol >= pca9482->ta_target_vol) {
		pca9482->ta_vol = pca9482->ta_target_vol;
		pr_info("%s: PreCC End: ta_vol=%d\n", __func__, pca9482->ta_vol);

		/* Change to DC state to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9482_set_charging_state(pca9482, DC_STATE_CC_MODE);
#else
		pca9482->charging_state = DC_STATE_CC_MODE;
#endif
	} else {
		pr_info("%s: PreCC Cont: ta_vol=%d\n", __func__, pca9482->ta_vol);
	}

	/* Send PD Message */
	mutex_lock(&pca9482->lock);
	pca9482->timer_id = TIMER_PDMSG_SEND;
	pca9482->timer_period = 0;
	mutex_unlock(&pca9482->lock);

	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging CC MODE control */
static int pca9482_charge_ccmode(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int ccmode;
	int iin, ichg;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_CC_MODE);
#else
	pca9482->charging_state = DC_STATE_CC_MODE;
#endif
	/* Check the charging type */
	ret = pca9482_check_error(pca9482);
	if (ret < 0) {
		if (ret == -EAGAIN) {
			/* DC error happens, but it is retry case */
			if (pca9482->ta_ctrl == TA_CTRL_CL_MODE) {
				/* Current TA control method is Current Limit mode */
				/* Retry DC as Constant Voltage mode */
				pr_info("%s: Retry DC : ta_ctrl=%d\n", __func__, pca9482->ta_ctrl);

				/* Disable charging */
				ret = pca9482_set_charging(pca9482, false);
				if (ret < 0)
					goto error;

				/* Softreset */
				ret = pca9482_softreset(pca9482);
				if (ret < 0)
					goto error;

				/* Set TA control method to Constant Voltage mode */
				pca9482->ta_ctrl = TA_CTRL_CV_MODE;

				/* Go to DC_STATE_PRESET_DC */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_PRESET_DC;
				pca9482->timer_period = 0;
				mutex_unlock(&pca9482->lock);
				queue_delayed_work(pca9482->dc_wq,
									&pca9482->timer_work,
									msecs_to_jiffies(pca9482->timer_period));
				ret = 0;
				goto error;
			} else {
				/* Current TA control method is Constant Voltage mode */
				/* Don't retry DC */
				pr_info("%s: Retry DC, but still failed - stop DC\n", __func__);
				goto error;
			}
		} else {
			/* Don't retry DC */
			goto error;
		}
	}

	/* Check new request */
	if (pca9482->req_new_dc_mode == true) {
		ret = pca9482_set_new_dc_mode(pca9482);
	} else if (pca9482->req_new_iin == true) {
		ret = pca9482_set_new_iin(pca9482);
	} else if (pca9482->req_new_vfloat == true) {
		ret = pca9482_set_new_vfloat(pca9482);
	} else {
		/* Check the charging type */
		ccmode = pca9482_check_dcmode_status(pca9482);
		if (ccmode < 0) {
			ret = ccmode;
			goto error;
		}

		switch (ccmode) {
		case DCMODE_LOOP_INACTIVE:
			/* Set input current compensation */
			/* Check the TA type */
			if (pca9482->ta_type == TA_TYPE_WIRELESS) {
				/* Need RX voltage compensation */
				ret = pca9482_set_rx_voltage_comp(pca9482);
				pr_info("%s: CC INACTIVE: rx_vol=%d\n", __func__, pca9482->ta_vol);
			} else {
				/* Check the current TA current with TA_MIN_CUR */
				if ((pca9482->ta_cur <= TA_MIN_CUR) ||
					(pca9482->ta_ctrl == TA_CTRL_CV_MODE)) {
					/* Need input voltage compensation */
					ret = pca9482_set_ta_voltage_comp(pca9482);
				} else {
					if (pca9482->ta_max_vol >= TA_MAX_VOL_CP) {
						/* This TA can support the input current without power limit */
						/* Need input current compensation */
						ret = pca9482_set_ta_current_comp(pca9482);
					} else {
						/* This TA has the power limitation for the input current compenstaion */
						/* The input current cannot increase over the constant power */
						/* Need input current compensation in constant power mode */
						ret = pca9482_set_ta_current_comp2(pca9482);
					}
				}
				pr_info("%s: CC INACTIVE: ta_cur=%d, ta_vol=%d\n", __func__, pca9482->ta_cur, pca9482->ta_vol);
			}
			break;

		case DCMODE_VFLT_LOOP:
			/* Read IIN_ADC */
			iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
			/* Read ICHG_ADC */
			ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);
			pr_info("%s: CC VFLOAT: iin=%d, ichg=%d\n", __func__, iin, ichg);
			/* go to Pre-CV mode */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_ENTER_CVMODE;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
			break;

		case DCMODE_IIN_LOOP:
		case DCMODE_CHG_LOOP:
			/* Read IIN_ADC */
			iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
			/* Read ICHG_ADC */
			ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);
			/* Check the TA type */
			if (pca9482->ta_type == TA_TYPE_WIRELESS) {
				/* Wireless Charger is connected */
				/* Decrease RX voltage (100mV) */
				pca9482->ta_vol = pca9482->ta_vol - WCRX_VOL_STEP;
				pr_info("%s: CC LOOP(WC):iin=%d, ichg=%d, next_rx_vol=%d\n", __func__, iin, ichg, pca9482->ta_vol);
			} else {
				/* USBPD TA is connected */

				/* Check the current TA current with TA_MIN_CUR */
				if ((pca9482->ta_cur <= TA_MIN_CUR) ||
					(pca9482->ta_ctrl == TA_CTRL_CV_MODE)) {
					/* Decrease TA voltage (20mV) */
					pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
					pr_info("%s: CC LOOP(1):iin=%d, ichg=%d, next_ta_vol=%d\n", __func__, iin, ichg, pca9482->ta_vol);
				} else {
					/* Check TA current and compare it with IIN_CC */
					if (pca9482->ta_cur <= pca9482->iin_cc - TA_CUR_LOW_OFFSET) {
						/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
						/* TA has abnormal behavior */
						/* Decrease TA voltage (20mV) */
#ifdef CONFIG_SEC_FACTORY
						pca9482->ta_vol = pca9482->ta_vol - TA_VOL_DEC_STEP_CC;
#else
						pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
#endif
						pr_info("%s: CC LOOP(2):iin=%d, ichg=%d, ta_cur=%d, next_ta_vol=%d\n",
								__func__, iin, ichg, pca9482->ta_cur, pca9482->ta_vol);
						/* Update TA target voltage */
						pca9482->ta_target_vol = pca9482->ta_vol;
					} else {
						/* TA current is higher than IIN_CC - 200mA */
						/* Decrease TA current first to reduce input current */
						/* Decrease TA current (50mA) */
						pca9482->ta_cur = pca9482->ta_cur - PD_MSG_TA_CUR_STEP;
						pr_info("%s: CC LOOP(3):iin=%d, ichg=%d, ta_vol=%d, next_ta_cur=%d\n",
								__func__, iin, ichg, pca9482->ta_vol, pca9482->ta_cur);
					}
				}
			}
			/* Send PD Message */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
			break;

		default:
			break;
		}
	}
error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* 2:1 Direct Charging Start CV MODE control - Pre CV MODE */
static int pca9482_charge_start_cvmode(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int cvmode = 0;
	int iin, ichg, vbat;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_START_CV);
#else
	pca9482->charging_state = DC_STATE_START_CV;
#endif

	/* Check the charging type */
	ret = pca9482_check_error(pca9482);
	if (ret != 0) {
		/* Check error type */
		if (ret == -ERROR_DCRCP) {
			/* Set dcmode to DCMODE_CHG_DONE */
			cvmode = DCMODE_CHG_DONE;
			pr_info("%s: dcmode is DCMODE_CHG_DONE by RCP\n", __func__);
		} else {
			/* DC error */
			goto error; // This is not active mode.
		}
	}

	/* Check the status */
	if (cvmode != DCMODE_CHG_DONE) {
		cvmode = pca9482_check_dcmode_status(pca9482);
		if (cvmode < 0) {
			ret = cvmode;
			goto error;
		}

		/* Read IIN_ADC */
		iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
		/* Read ICHG_ADC */
		ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);
		/* Read VBAT_ADC */
		vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
		/* Store VBAT_ADC to previous vbat */
		pca9482->prev_vbat = vbat;

		/* Check charging done state */
		/* Compare iin with input topoff current */
		pr_info("%s: iin=%d, ichg=%d, iin_topoff=%d\n",
				__func__, iin, ichg, pca9482->iin_topoff);
		if (iin < pca9482->iin_topoff) {
			/* Change cvmode status to charging done */
			cvmode = DCMODE_CHG_DONE;
			pr_info("%s: start CVMODE Status=%d\n", __func__, cvmode);
		}
	}

	switch (cvmode) {
	case DCMODE_CHG_DONE:
		/* Charging Done */
		/* Keep start CV mode until battery driver send stop charging */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9482_set_charging_state(pca9482, DC_STATE_CHARGING_DONE);
#else
		pca9482->charging_state = DC_STATE_CHARGING_DONE;
#endif
		pr_info("%s: Start CV - Charging Done\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9482_set_done(pca9482, true);
#endif

		/* Check the charging status after notification function */
		if (pca9482->charging_state != DC_STATE_NO_CHARGING) {
			/* Notification function does not stop timer work yet */
			/* Keep the charging done state */
			/* Set timer */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_ENTER_CVMODE;
			pca9482->timer_period = CVMODE_CHECK_T;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
		} else {
			/* Already called stop charging by notification function */
			pr_info("%s: Already stop DC\n", __func__);
		}
		break;

	case DCMODE_CHG_LOOP:
	case DCMODE_IIN_LOOP:
		/* Check the TA type */
		if (pca9482->ta_type == TA_TYPE_WIRELESS) {
			/* Decrease RX voltage (100mV) */
			pca9482->ta_vol = pca9482->ta_vol - WCRX_VOL_STEP;
			pr_info("%s: PreCV Cont(IIN_LOOP): rx_vol=%d\n", __func__, pca9482->ta_vol);
		} else {
			/* Check TA current */
			if (pca9482->ta_cur > TA_MIN_CUR) {
				/* TA current is higher than 1.0A */
				/* Decrease TA current (50mA) */
				pca9482->ta_cur = pca9482->ta_cur - PD_MSG_TA_CUR_STEP;
				pr_info("%s: PreCV Cont: ta_cur=%d\n", __func__, pca9482->ta_cur);
			} else {
				/* TA current is less than 1.0A */
				/* Decrease TA voltage (20mV) */
				pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: PreCV Cont(IIN_LOOP): ta_vol=%d\n", __func__, pca9482->ta_vol);
			}
		}
		/* Send PD Message */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		break;

	case DCMODE_VFLT_LOOP:
		/* Check the TA type */
		if (pca9482->ta_type == TA_TYPE_WIRELESS) {
			/* Decrease RX voltage (100mV) */
			pca9482->ta_vol = pca9482->ta_vol - WCRX_VOL_STEP;
			pr_info("%s: PreCV Cont: rx_vol=%d\n", __func__, pca9482->ta_vol);
		} else {
			/* Check present vfloat */
			if (pca9482->vfloat >= pca9482->max_vfloat) {
				/* Decrease TA voltage (40mV) */
				pca9482->ta_vol = pca9482->ta_vol - TA_VOL_STEP_PRE_CV * pca9482->chg_mode;
				pr_info("%s: PreCV Cont(40mV): ta_vol=%d\n", __func__, pca9482->ta_vol);
			} else {
				/* Decrease TA voltage (20mV) */
				pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP * pca9482->chg_mode;
				pr_info("%s: PreCV Cont(20mV): ta_vol=%d\n", __func__, pca9482->ta_vol);
			}
		}
		/* Send PD Message */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PDMSG_SEND;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		break;

	case DCMODE_LOOP_INACTIVE:
		/* Exit Pre CV mode */
		pr_info("%s: PreCV End: ta_vol=%d, ta_cur=%d\n", __func__, pca9482->ta_vol, pca9482->ta_cur);

		/* Set TA target voltage to TA voltage */
		pca9482->ta_target_vol = pca9482->ta_vol;

		/* Need to implement notification to other driver */
		/* To do here */

		/* Go to CV mode */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_CHECK_CVMODE;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		break;

	default:
		break;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging CV MODE control */
static int pca9482_charge_cvmode(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int cvmode = 0;
	int iin, ichg, vbat;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_CV_MODE);
#else
	pca9482->charging_state = DC_STATE_CV_MODE;
#endif

	ret = pca9482_check_error(pca9482);
	if (ret != 0) {
		/* Check error type */
		if (ret == -ERROR_DCRCP) {
			/* Set dcmode to DCMODE_CHG_DONE */
			cvmode = DCMODE_CHG_DONE;
			pr_info("%s: dcmode is DCMODE_CHG_DONE by RCP\n", __func__);
		} else {
			/* DC error */
			goto error; // This is not active mode.
		}
	}

	/* Check new request */
	if (pca9482->req_new_dc_mode == true) {
		ret = pca9482_set_new_dc_mode(pca9482);
	} else if (pca9482->req_new_iin == true) {
		ret = pca9482_set_new_iin(pca9482);
	} else if (pca9482->req_new_vfloat == true) {
		ret = pca9482_set_new_vfloat(pca9482);
	} else {
		/* Check the status */
		if (cvmode != DCMODE_CHG_DONE) {
			cvmode = pca9482_check_dcmode_status(pca9482);
			if (cvmode < 0) {
				ret = cvmode;
				goto error;
			}

			/* Read IIN_ADC */
			iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
			/* Read ICHG_ADC */
			ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);
			/* Read VBAT_ADC */
			vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
			/* Store VBAT_ADC to previous vbat */
			pca9482->prev_vbat = vbat;

			/* Check charging done state */
			if (cvmode == DCMODE_LOOP_INACTIVE) {
				/* Compare iin with input topoff current */
				pr_info("%s: iin=%d, ichg=%d, iin_topoff=%d\n",
						__func__, iin, ichg, pca9482->iin_topoff);
				if (iin < pca9482->iin_topoff) {
					/* Change cvmode status to charging done */
					cvmode = DCMODE_CHG_DONE;
					pr_info("%s: CVMODE Status=%d\n", __func__, cvmode);
				}
			}
		}

		switch (cvmode) {
		case DCMODE_CHG_DONE:
			/* Charging Done */
			/* Keep CV mode until battery driver send stop charging */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_charging_state(pca9482, DC_STATE_CHARGING_DONE);
#else
			pca9482->charging_state = DC_STATE_CHARGING_DONE;
#endif
			/* Need to implement notification function */
			/* A customer should insert code */

			pr_info("%s: CV Done\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_done(pca9482, true);
#endif
			/* Check the charging status after notification function */
			if (pca9482->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the charging done state */
				/* Set timer */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_CHECK_CVMODE;

				/* Add to check charging step and set the polling time */
				if (pca9482->vfloat < pca9482->pdata->step1_vth) {
					/* Step1 charging - polling time is cv_polling */
					pca9482->timer_period = pca9482->pdata->cv_polling;
				} else if ((pca9482->dec_vfloat == true) || (pca9482->vfloat >= pca9482->max_vfloat)) {
					/* present vfloat is lower than previous vfloat or present vfloat is maximum vfloat */
					/* pollig time is CVMODE_CHECK2_T */
					pca9482->timer_period = CVMODE_CHECK2_T;
				} else {
					/* Step2 or 3 charging - polling time is CVMODE_CHECK_T */
					pca9482->timer_period = CVMODE_CHECK_T;
				}
				mutex_unlock(&pca9482->lock);
				queue_delayed_work(pca9482->dc_wq,
									&pca9482->timer_work,
									msecs_to_jiffies(pca9482->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case DCMODE_CHG_LOOP:
		case DCMODE_IIN_LOOP:
			/* Check the TA type */
			if (pca9482->ta_type == TA_TYPE_WIRELESS) {
				/* Decrease RX Voltage (100mV) */
				pca9482->ta_vol = pca9482->ta_vol - WCRX_VOL_STEP;
				pr_info("%s: CV LOOP(WC), Cont: iin=%d, ichg=%d, rx_vol=%d\n",
						__func__, iin, ichg, pca9482->ta_vol);
			} else {
				/* Check TA current */
				if (pca9482->ta_cur > TA_MIN_CUR) {
					/* TA current is higher than (1.0A*chg_mode) */
					/* Check TA current and compare it with IIN_CC */
					if (pca9482->ta_cur <= pca9482->iin_cc - TA_CUR_LOW_OFFSET) {
						/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
						/* TA has abnormal behavior */
						/* Decrease TA voltage (20mV) */
						pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
						pr_info("%s: CV LOOP(1):iin=%d, ichg=%d, ta_cur=%d, next_ta_vol=%d\n",
								__func__, iin, ichg, pca9482->ta_cur, pca9482->ta_vol);
						/* Update TA target voltage */
						pca9482->ta_target_vol = pca9482->ta_vol;
					} else {
						/* TA current is higher than IIN_CC - 200mA */
						/* Decrease TA current first to reduce input current */
						/* Decrease TA current (50mA) */
						pca9482->ta_cur = pca9482->ta_cur - PD_MSG_TA_CUR_STEP;
						pr_info("%s: CV LOOP(2):iin=%d, ichg=%d, ta_vol=%d, next_ta_cur=%d\n",
								__func__, iin, ichg, pca9482->ta_vol, pca9482->ta_cur);
					}
				} else {
					/* TA current is less than (1.0A*chg_mode) */
					/* Decrease TA Voltage (20mV) */
					pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
					pr_info("%s: CV LOOP(3), Cont: iin=%d, ichg=%d, ta_vol=%d\n",
							__func__, iin, ichg, pca9482->ta_vol);
				}
			}
			/* Send PD Message */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
			break;

		case DCMODE_VFLT_LOOP:
			/* Check the TA type */
			if (pca9482->ta_type == TA_TYPE_WIRELESS) {
				/* Decrease RX voltage */
				pca9482->ta_vol = pca9482->ta_vol - WCRX_VOL_STEP;
				pr_info("%s: CV VFLOAT, Cont: iin=%d, ichg=%d, rx_vol=%d\n",
						__func__, iin, ichg, pca9482->ta_vol);
			} else {
				/* Decrease TA voltage */
				pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: CV VFLOAT, Cont: iin=%d, ichg=%d, ta_vol=%d\n",
						__func__, iin, ichg, pca9482->ta_vol);
			}

			/* Set TA target voltage to TA voltage */
			pca9482->ta_target_vol = pca9482->ta_vol;

			/* Send PD Message */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
			break;

		case DCMODE_LOOP_INACTIVE:
			/* Set timer */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_CVMODE;
			/* Add to check charging step and set the polling time */
			if (pca9482->vfloat < pca9482->pdata->step1_vth) {
				/* Step1 charging - polling time is cv_polling */
				pca9482->timer_period = pca9482->pdata->cv_polling;
			} else if ((pca9482->dec_vfloat == true) || (pca9482->vfloat >= pca9482->max_vfloat)) {
				/* present vfloat is lower than previous vfloat or present vfloat is maximum vfloat */
				/* pollig time is CVMODE_CHECK2_T */
				pca9482->timer_period = CVMODE_CHECK2_T;
			} else {
				/* Step2 or 3 charging - polling time is CVMODE_CHECK_T */
				pca9482->timer_period = CVMODE_CHECK_T;
			}
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
			break;

		default:
			break;
		}
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging FPDO CV MODE control */
static int pca9482_charge_fpdo_cvmode(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int cvmode;
	int iin, ichg;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval val;
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	int mainbat_vol = 0;
	int subbat_vol = 0;
#endif
#endif

	pr_info("%s: ======START=======\n", __func__);

	pca9482->charging_state = DC_STATE_FPDO_CV_MODE;

	ret = pca9482_check_error(pca9482);
	if (ret != 0)
		goto error; // This is not active mode.

	/* Check new request */
	if (pca9482->req_new_iin == true) {
		/* Set IIN_CC to new iin */
		pca9482->iin_cc = pca9482->new_iin;
		/* Update iin_cfg */
		pca9482->iin_cfg = pca9482->iin_cc;
		/* Set IIN_CFG to new IIN */
		ret = pca9482_set_input_current(pca9482, pca9482->iin_cc);
		if (ret < 0)
			goto error;

		/* Clear req_new_iin */
		mutex_lock(&pca9482->lock);
		pca9482->req_new_iin = false;
		mutex_unlock(&pca9482->lock);

		/* Set timer - 1s */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_CHECK_FPDOCVMODE;
		pca9482->timer_period = CVMODE_CHECK2_T;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq, &pca9482->timer_work, msecs_to_jiffies(pca9482->timer_period));
	} else if (pca9482->req_new_vfloat == true) {
		/* Set VFLOAT to new vfloat */
		pca9482->vfloat = pca9482->new_vfloat;
		ret = pca9482_set_vfloat(pca9482, pca9482->vfloat);
		if (ret < 0)
			goto error;

		/* Clear req_new_vfloat */
		mutex_lock(&pca9482->lock);
		pca9482->req_new_vfloat = false;
		mutex_unlock(&pca9482->lock);

		/* Set timer - 1s */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_CHECK_FPDOCVMODE;
		pca9482->timer_period = CVMODE_CHECK2_T;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq, &pca9482->timer_work, msecs_to_jiffies(pca9482->timer_period));
	} else {
		cvmode = pca9482_check_dcmode_status(pca9482);
		if (cvmode < 0) {
			ret = cvmode;
			goto error;
		}

		/* Read IIN_ADC */
		iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
		/* Read ICHG_ADC */
		ichg = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);

		/* Check charging done state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_VOLTAGE_PACK_MAIN, val);
		mainbat_vol = val.intval * PCA9482_SEC_DENOM_U_M;
		psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_VOLTAGE_PACK_SUB, val);
		subbat_vol = val.intval * PCA9482_SEC_DENOM_U_M;

		if (cvmode == DCMODE_LOOP_INACTIVE ||
				mainbat_vol >= pca9482->fpdo_dc_mainbat_topoff ||
				subbat_vol >= pca9482->fpdo_dc_subbat_topoff) {
			/* Compare iin with input topoff current */
			pr_info("%s: iin=%d, ichg=%d, fpdo_dc_iin_topoff=%d\n",
					__func__, iin, ichg, pca9482->fpdo_dc_iin_topoff);
			pr_info("%s: fpdo_dc_mainbat_topoff=%d/%d, fpdo_dc_subbat_topoff=%d/%d\n", __func__,
					mainbat_vol, pca9482->fpdo_dc_mainbat_topoff,
					subbat_vol, pca9482->fpdo_dc_subbat_topoff);
			if (iin < pca9482->fpdo_dc_iin_topoff ||
					mainbat_vol >= pca9482->fpdo_dc_mainbat_topoff ||
					subbat_vol >= pca9482->fpdo_dc_subbat_topoff) {
#else
		psy_do_property("battery", get, POWER_SUPPLY_PROP_VOLTAGE_NOW, val);

		if (cvmode == DCMODE_LOOP_INACTIVE || val.intval >= pca9482->fpdo_dc_vnow_topoff) {
			/* Compare iin with input topoff current */
			pr_info("%s: iin=%d, ichg=%d, vnow=%d, fpdo_dc_iin_topoff=%d, fpdo_dc_vnow_topoff=%d\n",
					__func__, iin, ichg, val.intval,
					pca9482->fpdo_dc_iin_topoff, pca9482->fpdo_dc_vnow_topoff);
			if (val.intval >= pca9482->fpdo_dc_vnow_topoff || iin < pca9482->fpdo_dc_iin_topoff) {
#endif	/* CONFIG_DUAL_BATTERY */
#else
		if (cvmode == DCMODE_LOOP_INACTIVE) {
			/* Compare iin with input topoff current */
			pr_info("%s: iin=%d, ichg=%d, iin_topoff=%d\n",
					__func__, iin, ichg, pca9482->iin_topoff);
			if (iin < pca9482->iin_topoff) {
#endif	/* CONFIG_BATTERY_SAMSUNG */
				/* Check charging done counter */
				if (pca9482->done_cnt < FPDO_DONE_CNT) {
					/* Keep cvmode status */
					pr_info("%s: Keep FPDO CVMODE Status=%d\n", __func__, cvmode);
					/* Increase charging done counter */
					pca9482->done_cnt++;
				} else {
					/* Change cvmode status to charging done */
					cvmode = DCMODE_CHG_DONE;
					pr_info("%s: FPDO_CVMODE Status=%d\n", __func__, cvmode);
					/* Clear charging done counter */
					pca9482->done_cnt = 0;
				}
			} else {
				/* Clear charging done counter */
				pca9482->done_cnt = 0;
			}
		}

		switch (cvmode) {
		case DCMODE_CHG_DONE:
			/* Charging Done */
			/* Keep FPDO CV mode until battery driver send stop charging */
			pca9482->charging_state = DC_STATE_CHARGING_DONE;
			/* Need to implement notification function */
			/* A customer should insert code */

			pr_info("%s: FPDO CV Done\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_done(pca9482, true);
#endif

			/* Check the charging status after notification function */
			if (pca9482->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the charging done state */
				/* Set timer */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_CHECK_FPDOCVMODE;
				pca9482->timer_period = CVMODE_CHECK2_T;
				mutex_unlock(&pca9482->lock);
				queue_delayed_work(pca9482->dc_wq,
									&pca9482->timer_work,
									msecs_to_jiffies(pca9482->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case DCMODE_CHG_LOOP:
		case DCMODE_IIN_LOOP:
			/* IIN_LOOP happens */
			pr_info("%s: FPDO CV IIN_LOOP\n", __func__);
			/* Need to stop DC by battery driver */

			/* Need to implement notification function */
			/* A customer should insert code */
			/* To do here */

			/* Check the charging status after notification function */
			if (pca9482->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the current state */
				/* Set timer - 1s */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_CHECK_FPDOCVMODE;
				pca9482->timer_period = CVMODE_CHECK2_T;
				mutex_unlock(&pca9482->lock);
				queue_delayed_work(pca9482->dc_wq,
									&pca9482->timer_work,
									msecs_to_jiffies(pca9482->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case DCMODE_VFLT_LOOP:
			/* VFLOAT_LOOP happens */
			pr_info("%s: FPDO CV VFLOAT_LOOP\n", __func__);
			/* Need to stop DC and transit to switching charger by battery driver */

			/* Need to implement notification function */
			/* A customer should insert code */
			/* To do here */

			/* Check the charging status after notification function */
			if (pca9482->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the current state */
				/* Set timer */
				mutex_lock(&pca9482->lock);
				pca9482->timer_id = TIMER_CHECK_FPDOCVMODE;
				pca9482->timer_period = CVMODE_CHECK2_T;
				mutex_unlock(&pca9482->lock);
				queue_delayed_work(pca9482->dc_wq,
									&pca9482->timer_work,
									msecs_to_jiffies(pca9482->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case DCMODE_LOOP_INACTIVE:
			/* Set timer */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_FPDOCVMODE;
			pca9482->timer_period = CVMODE_CHECK3_T;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
			break;

		default:
			break;
		}
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Direct Charging Bypass Mode Control */
static int pca9482_charge_bypass_mode(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int dc_status;
	int vbat, iin;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_BYPASS_MODE);
#else
	pca9482->charging_state = DC_STATE_BYPASS_MODE;
#endif

	ret = pca9482_check_error(pca9482);
	if (ret < 0)
		goto error;	// This is not active mode.

	/* Check new request */
	if (pca9482->req_new_dc_mode == true) {
		ret = pca9482_set_new_dc_mode(pca9482);
	} else if (pca9482->req_new_iin == true) {
		ret = pca9482_set_new_iin(pca9482);
	} else if (pca9482->req_new_vfloat == true) {
		ret = pca9482_set_new_vfloat(pca9482);
	} else {
		dc_status = pca9482_check_dcmode_status(pca9482);
		if (dc_status < 0) {
			ret = dc_status;
			goto error;
		}

		/* Read IIN ADC */
		iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
		/* Read VBAT ADC */
		vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);

		pr_info("%s: iin=%d, vbat=%d\n", __func__, iin, vbat);

		if (dc_status == DCMODE_IIN_LOOP) {
			/* Decrease input current */
			/* Check TA current and compare it with IIN_CC */
			if (pca9482->ta_cur <= pca9482->iin_cc - TA_CUR_LOW_OFFSET) {
				/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
				/* TA has abnormal behavior */
				/* Decrease TA voltage (20mV) */
				pca9482->ta_vol = pca9482->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: IIN LOOP:iin=%d, ta_cur=%d, next_ta_vol=%d\n",
						__func__, iin, pca9482->ta_cur, pca9482->ta_vol);
			} else {
				/* TA current is higher than IIN_CC - 200mA */
				/* Decrease TA current first to reduce input current */
				/* Decrease TA current (50mA) */
				pca9482->ta_cur = pca9482->ta_cur - PD_MSG_TA_CUR_STEP;
				pr_info("%s: IIN LOOP:iin=%d, next_ta_cur=%d\n",
						__func__, iin, pca9482->ta_cur);
			}

			/* Send PD Message */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PDMSG_SEND;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
		} else {
			/* Ignore other status */
			/* Keep Bypass mode */
			pr_info("%s: Bypass mode, status=%d, ta_cur=%d, ta_vol=%d\n",
					__func__, dc_status, pca9482->ta_cur, pca9482->ta_vol);
			/* Set timer - 10s */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_BYPASSMODE;
			pca9482->timer_period = BYPMODE_CHECK_T;	/* 10s */
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
		}
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Direct Charging DC mode Change Control */
static int pca9482_charge_dcmode_change(struct pca9482_charger *pca9482)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_DCMODE_CHANGE);
#else
	pca9482->charging_state = DC_STATE_DCMODE_CHANGE;
#endif

	ret = pca9482_set_new_dc_mode(pca9482);

	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Preset TA voltage and current for Direct Charging Mode */
static int pca9482_preset_dcmode(struct pca9482_charger *pca9482)
{
	int vbat;
	unsigned int val;
	int ret = 0;
	int chg_mode;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_PRESET_DC);
#else
	pca9482->charging_state = DC_STATE_PRESET_DC;
#endif
	/* Check TA type */
	if (pca9482->ta_type == TA_TYPE_USBPD_20) {
		/* TA type is USBPD 2.0 and support only FPDO */
		pr_info("%s: ta type : fixed PDO\n", __func__);

		/* Disable OV_TRACKING_EN */
		val = 0;
		ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_2,
									PCA9482_BIT_OV_TRACKING_EN, val);
		/* Set PDO object position to 9V FPDO */
		pca9482->ta_objpos = 2;
		/* Set TA voltage to 9V */
		pca9482->ta_vol = 9000000;
		/* Set TA maximum voltage to 9V */
		pca9482->ta_max_vol = 9000000;
		/* Set IIN_CC to iin_cfg */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9482->iin_cc = pca9482->pdata->fpdo_dc_iin_lowest_limit;
#else
		pca9482->iin_cc = pca9482->iin_cfg;
#endif
		/* Set TA operating current and maximum current to iin_cc */
		pca9482->ta_cur = pca9482->iin_cc;
		pca9482->ta_max_cur = pca9482->iin_cc;
		/* Calculate TA maximum power */
		pca9482->ta_max_pwr = (pca9482->ta_max_vol/DENOM_U_M)*(pca9482->ta_max_cur/DENOM_U_M);

		pr_info("%s: Preset DC(FPDO), ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
			__func__, pca9482->ta_max_vol, pca9482->ta_max_cur, pca9482->ta_max_pwr, pca9482->iin_cc, pca9482->chg_mode);
	} else {
		/* Read VBAT ADC */
		vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
		if (vbat < 0) {
			ret = vbat;
			goto error;
		}

		/* Compare VBAT with VBAT ADC */
		if (vbat > pca9482->vfloat)	{
			/* Warn "Invalid battery voltage to start direct charging" */
			pr_err("%s: Warning - vbat adc(%duV) is higher than VFLOAT(%duV)\n",
						__func__, vbat, pca9482->vfloat);
		}

		/* Check minimum VBAT level */
		if (vbat <= DC_VBAT_MIN_ERR) {
			/* Invalid battery level to start direct charging */
			pr_err("%s: This vbat(%duV) will make VIN_OV_TRACKING error\n", __func__, vbat);
			ret = -EINVAL;
			goto error;
		}

		/* Check the TA type and set the charging mode */
		if (pca9482->ta_type == TA_TYPE_WIRELESS) {
			/* Wireless Charger is connected */
			/* Set the RX max current to input request current(iin_cfg) initially
			   to get RX maximum current from RX IC */
			pca9482->ta_max_cur = pca9482->iin_cfg;
			/* Set the RX max voltage to enough high value to find RX maximum voltage initially */
			pca9482->ta_max_vol = WCRX_MAX_VOL * pca9482->pdata->chg_mode;
			/* Get the RX max current/voltage(RX_MAX_CUR/VOL) */
			ret = pca9482_get_rx_max_power(pca9482);
			if (ret < 0) {
				/* RX IC does not have the desired maximum voltage */
				/* Check the desired mode */
				if (pca9482->pdata->chg_mode == CHG_4TO1_DC_MODE) {
					/* RX IC doesn't have any maximum voltage to support 4:1 mode */
					/* Get the RX max current/voltage with 2:1 mode */
					pca9482->ta_max_vol = WCRX_MAX_VOL;
					ret = pca9482_get_rx_max_power(pca9482);
					if (ret < 0) {
						pr_err("%s: RX IC doesn't have any RX voltage to support 2:1 or 4:1\n", __func__);
						pca9482->chg_mode = CHG_NO_DC_MODE;
						goto error;
					} else {
						/* RX IC has the maximum RX voltage to support 2:1 mode */
						pca9482->chg_mode = CHG_2TO1_DC_MODE;
					}
				} else {
					/* The desired CHG mode is 2:1 mode */
					/* RX IC doesn't have any RX voltage to support 2:1 mode*/
					pr_err("%s: RX IC doesn't have any RX voltage to support 2:1\n", __func__);
					pca9482->chg_mode = CHG_NO_DC_MODE;
					goto error;
				}
			} else {
				/* RX IC has the desired RX voltage */
				pca9482->chg_mode = pca9482->pdata->chg_mode;
			}

			chg_mode = pca9482->chg_mode;

			/* Set IIN_CC to MIN[IIN, (RX_MAX_CUR by RX IC)*chg_mode]*/
			pca9482->iin_cc = MIN(pca9482->iin_cfg, (pca9482->ta_max_cur*chg_mode));
			/* Set the current IIN_CC to iin_cfg */
			pca9482->iin_cfg = pca9482->iin_cc;

			/* Set RX voltage to MAX[(2*VBAT_ADC*chg_mode + offset), TA_MIN_VOL_PRESET*chg_mode] */
			pca9482->ta_vol = max(pca9482->pdata->ta_min_vol*chg_mode, (2*vbat*chg_mode + TA_VOL_PRE_OFFSET));
			val = pca9482->ta_vol/WCRX_VOL_STEP;	/* RX voltage resolution is 100mV */
			pca9482->ta_vol = val*WCRX_VOL_STEP;
			/* Set RX voltage to MIN[RX voltage, RX_MAX_VOL*chg_mode] */
			pca9482->ta_vol = MIN(pca9482->ta_vol, pca9482->ta_max_vol);

			pr_info("%s: Preset DC, rx_max_vol=%d, rx_max_cur=%d, rx_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
				__func__, pca9482->ta_max_vol, pca9482->ta_max_cur, pca9482->ta_max_pwr, pca9482->iin_cc, pca9482->chg_mode);

			pr_info("%s: Preset DC, rx_vol=%d\n", __func__, pca9482->ta_vol);

		} else {
			/* USBPD TA is connected */
			/* Set the TA max current to input request current(iin_cfg) initially
			   to get TA maximum current from PD IC */
			pca9482->ta_max_cur = pca9482->iin_cfg;
			/* Set the TA max voltage to enough high value to find TA maximum voltage initially */
			pca9482->ta_max_vol = TA_MAX_VOL * pca9482->pdata->chg_mode;
			/* Search the proper object position of PDO */
			pca9482->ta_objpos = 0;
			/* Get the APDO max current/voltage(TA_MAX_CUR/VOL) */
			ret = pca9482_get_apdo_max_power(pca9482);
			if (ret < 0) {
				/* TA does not have the desired APDO */
				/* Check the desired mode */
				if (pca9482->pdata->chg_mode == CHG_4TO1_DC_MODE) {
					/* TA doesn't have any APDO to support 4:1 mode */
					/* Get the APDO max current/voltage with 2:1 mode */
					pca9482->ta_max_vol = TA_MAX_VOL;
					pca9482->ta_objpos = 0;
					ret = pca9482_get_apdo_max_power(pca9482);
					if (ret < 0) {
						pr_err("%s: TA doesn't have any APDO to support 2:1 or 4:1\n", __func__);
						pca9482->chg_mode = CHG_NO_DC_MODE;
						goto error;
					} else {
						/* TA has APDO to support 2:1 mode */
						pca9482->chg_mode = CHG_2TO1_DC_MODE;
					}
				} else {
					/* The desired TA mode is 2:1 mode */
					/* TA doesn't have any APDO to support 2:1 mode*/
					pr_err("%s: TA doesn't have any APDO to support 2:1\n", __func__);
					pca9482->chg_mode = CHG_NO_DC_MODE;
					goto error;
				}
			} else {
				/* TA has the desired APDO */
				pca9482->chg_mode = pca9482->pdata->chg_mode;
			}

			chg_mode = pca9482->chg_mode;

			/* Calculate new TA maximum current and voltage that used in the direct charging */
			/* Set IIN_CC to MIN[IIN, (TA_MAX_CUR by APDO)*chg_mode]*/
			pca9482->iin_cc = MIN(pca9482->iin_cfg, (pca9482->ta_max_cur*chg_mode));
			/* Set the current IIN_CC to iin_cfg for recovering it after resolution adjustment */
			pca9482->iin_cfg = pca9482->iin_cc;
			/* Calculate new TA max voltage */
			/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
			val = pca9482->iin_cc/PD_MSG_TA_CUR_STEP;
			pca9482->iin_cc = val*PD_MSG_TA_CUR_STEP;
			/* Set TA_MAX_VOL to MIN[TA_MAX_VOL*chg_mode, TA_MAX_PWR/(IIN_CC/chg_mode)] */
			val = pca9482->ta_max_pwr/(pca9482->iin_cc/chg_mode/1000);	/* mV */
			val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
			val = val*PD_MSG_TA_VOL_STEP; /* uV */
			pca9482->ta_max_vol = MIN(val, TA_MAX_VOL*chg_mode);

			/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
			pca9482->ta_vol = max(pca9482->pdata->ta_min_vol*chg_mode, (2*vbat*chg_mode + TA_VOL_PRE_OFFSET));
			val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
			pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;
			/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL*chg_mode] */
			pca9482->ta_vol = MIN(pca9482->ta_vol, pca9482->ta_max_vol);
			/* Set the initial TA current to IIN_CC/chg_mode */
			pca9482->ta_cur = pca9482->iin_cc/chg_mode;
			/* Recover IIN_CC to the original value(iin_cfg) */
			pca9482->iin_cc = pca9482->iin_cfg;

			pr_info("%s: Preset DC, ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
				__func__, pca9482->ta_max_vol, pca9482->ta_max_cur, pca9482->ta_max_pwr, pca9482->iin_cc, pca9482->chg_mode);

			pr_info("%s: Preset DC, ta_vol=%d, ta_cur=%d\n",
				__func__, pca9482->ta_vol, pca9482->ta_cur);
		}
	}

	/* Send PD Message */
	mutex_lock(&pca9482->lock);
	pca9482->timer_id = TIMER_PDMSG_SEND;
	pca9482->timer_period = 0;
	mutex_unlock(&pca9482->lock);
	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* Preset direct charging configuration */
static int pca9482_preset_config(struct pca9482_charger *pca9482)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_PRESET_DC);
#else
	pca9482->charging_state = DC_STATE_PRESET_DC;
#endif

	/* Set IIN_CFG to IIN_CC */
	ret = pca9482_set_input_current(pca9482, pca9482->iin_cc);
	if (ret < 0)
		goto error;

	/* Set ICHG_CFG to enough high value */
	if (pca9482->ichg_cfg < 2*pca9482->iin_cfg)
		pca9482->ichg_cfg = 2*pca9482->iin_cfg;
	ret = pca9482_set_charging_current(pca9482, pca9482->ichg_cfg);
	if (ret < 0)
		goto error;

	/* Check TA type */
	if (pca9482->ta_type == TA_TYPE_USBPD_20) {
		/* Set VFLOAT to VBAT */
		ret = pca9482_set_vfloat(pca9482, pca9482->vfloat);
		if (ret < 0)
			goto error;
		/* Set switching frequency */
		pca9482->fsw_cfg = pca9482->pdata->fsw_cfg_fpdo;
	} else {
		/* Check VFLOAT method */
		if (pca9482->pdata->fg_vfloat == true) {
			/* Set PCA9482 VFLOAT to default value */
			ret = pca9482_set_vfloat(pca9482, pca9482->pdata->dft_vfloat);
		} else {
			/* Set PCA9482 VFLOAT to new vfloat */
			ret = pca9482_set_vfloat(pca9482, pca9482->vfloat);
		}

		if (ret < 0)
			goto error;

		/* Set switching frequency */
		if (pca9482->iin_cc > pca9482->pdata->iin_low_freq)
			pca9482->fsw_cfg = pca9482->pdata->fsw_cfg;
		else
			pca9482->fsw_cfg = pca9482->pdata->fsw_cfg_low;
	}
	ret = pca9482_write_reg(pca9482, PCA9482_REG_SC_CNTL_0, PCA9482_FSW_CFG(pca9482->fsw_cfg));
	if (ret < 0)
		goto error;
	pr_info("%s: sw_freq=%dkHz\n", __func__, pca9482->fsw_cfg);

	/* Enable PCA9482 */
	ret = pca9482_set_charging(pca9482, true);
	if (ret < 0)
		goto error;

	/* Clear previous iin adc */
	pca9482->prev_iin = 0;

	/* Clear TA increment flag */
	pca9482->prev_inc = INC_NONE;

	/* Go to CHECK_ACTIVE state after 150ms*/
	mutex_lock(&pca9482->lock);
	pca9482->timer_id = TIMER_CHECK_ACTIVE;
	pca9482->timer_period = ENABLE_DELAY_T;
	mutex_unlock(&pca9482->lock);
	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));
	ret = 0;

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Check the charging status before entering the adjust cc mode */
static int pca9482_check_active_state(struct pca9482_charger *pca9482)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_CHECK_ACTIVE);
#else
	pca9482->charging_state = DC_STATE_CHECK_ACTIVE;
#endif
	ret = pca9482_check_error(pca9482);

	if (ret == 0) {
		/* PCA9482 is in active state */
		/* Clear retry counter */
		pca9482->retry_cnt = 0;
		/* Check TA type */
		if (pca9482->ta_type == TA_TYPE_USBPD_20) {
			/* Go to FPDO CV mode */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_FPDOCVMODE;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
		} else {
			/* Go to Adjust CC mode */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_ADJUST_CCMODE;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
		}
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
	} else if (ret == -EAGAIN) {
		/* It is the retry condition */
		/* Check the retry counter */
		if (pca9482->retry_cnt < MAX_RETRY_CNT) {
			/* Disable charging */
			ret = pca9482_set_charging(pca9482, false);
			if (ret < 0)
				goto error;
			/* Softreset */
			ret = pca9482_softreset(pca9482);
			if (ret < 0)
				goto error;
			/* Increase retry counter */
			pca9482->retry_cnt++;
			pr_err("%s: retry charging start - retry_cnt=%d\n", __func__, pca9482->retry_cnt);
			/* Go to DC_STATE_PRESET_DC */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_PRESET_DC;
			pca9482->timer_period = 0;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
			ret = 0;
		} else {
			pr_err("%s: retry fail\n", __func__);
			/* Disable charging */
			ret = pca9482_set_charging(pca9482, false);
			if (ret < 0)
				goto error;
			/* Softreset */
			ret = pca9482_softreset(pca9482);
			if (ret < 0)
				goto error;
			/* Notify maximum retry error */
			ret = -EINVAL;
			/* Stop charging in timer_work */
		}
	} else {
		pr_err("%s: dc start fail\n", __func__);
		/* Implement error handler function if it is needed */
		/* Disable charging */
		ret = pca9482_set_charging(pca9482, false);
		if (ret < 0)
			goto error;
		/* Softreset */
		ret = pca9482_softreset(pca9482);
		if (ret < 0)
			goto error;
		/* Notify invalid error */
		ret = -EINVAL;
		/* Stop charging in timer_work */
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Start reverse mode setting */
static int pca9482_charge_start_reverse(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int val;
	u8 reg_val[REG_BUFFER_MAX];

	pr_info("%s: ======START=======\n", __func__);

	pca9482->charging_state = DC_STATE_REVERSE_MODE;

	/* Clear all interrupt registers before starting DC for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_INT_DEVICE_0 | PCA9482_REG_BIT_AI,
							&reg_val[REG_DEVICE_0], REG_BUFFER_MAX);
	if (ret < 0)
		goto error;
	pr_info("%s: reg[0x01]=0x%x,[0x02]=0x%x,[0x03]=0x%x,[0x04]=0x%x,[0x05]=0x%x,[0x06]=0x%x,[0x07]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);
	/* Shutdown ADC operation to reduce leakage current */
	/* Configure ADC operation mode to Force shutdown mode */
	val = FORCE_SHUTDOWN_MODE << MASK2SHIFT(PCA9482_BIT_ADC_MODE_CFG);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_ADC_CNTL,
							PCA9482_BIT_ADC_MODE_CFG, val);
	if (ret < 0)
		goto error;
	/* Read ADC_CNTL register to confirm shutdown */
	ret = pca9482_read_reg(pca9482, PCA9482_REG_ADC_CNTL, &val);
	pr_info("%s: ADC_CTRL : 0x%02x\n", __func__, val);


	ret = pca9482_set_reverse_mode(pca9482, true);
	if (ret < 0)
		goto error;

	/* Go to reverse mode */
	mutex_lock(&pca9482->lock);
	pca9482->timer_id = TIMER_CHECK_REVERSE_ACTIVE;
	pca9482->timer_period = REVERSE_WAIT_T; // ms unit
	mutex_unlock(&pca9482->lock);
	queue_delayed_work(pca9482->dc_wq,
						&pca9482->timer_work,
						msecs_to_jiffies(pca9482->timer_period));

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Check active status before entering reverse mode */
static int pca9482_check_reverse_active_state(struct pca9482_charger *pca9482)
{
	int ret = 0;
	int val;

	pr_info("%s: ======START=======\n", __func__);

	pca9482->charging_state = DC_STATE_REVERSE_MODE;

	ret = pca9482_check_reverse_error(pca9482);

	if (ret == 0) {
		/* PCA9482 is in active state */
		/* Clear retry counter */
		pca9482->retry_cnt = 0;
		/* Go to Reverse mode */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_CHECK_REVERSE_MODE;
		pca9482->timer_period = REVERSE_CHECK_T;
		mutex_unlock(&pca9482->lock);

		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
	} else if (ret == -EAGAIN) {
		/* It is the retry condition */
		/* Check the retry counter */
		if (pca9482->retry_cnt < MAX_RETRY_CNT) {
			/* Disable reverse mode */
			ret = pca9482_set_reverse_mode(pca9482, false);
			if (ret < 0)
				goto error;
			/* Softreset */
			ret = pca9482_softreset(pca9482);
			if (ret < 0)
				goto error;
			/* Increase retry counter */
			pca9482->retry_cnt++;
			pr_err("%s: retry to set reverse mode - retry_cnt=%d\n", __func__, pca9482->retry_cnt);
			/* Set VIN_OCP_CURRENT_12_11 again */
			ret = pca9482_set_vin_ocp(pca9482, pca9482->iin_rev);
			if (ret < 0)
				goto error;

			/* Shutdown ADC operation to reduce leakage current */
			/* Configure ADC operation mode to Force shutdown mode */
			val = FORCE_SHUTDOWN_MODE << MASK2SHIFT(PCA9482_BIT_ADC_MODE_CFG);
			ret = pca9482_update_reg(pca9482, PCA9482_REG_ADC_CNTL,
									PCA9482_BIT_ADC_MODE_CFG, val);
			if (ret < 0)
				goto error;

			/* Set reverse mode */
			ret = pca9482_set_reverse_mode(pca9482, true);
			if (ret < 0)
				goto error;

			/* Go to DC_STATE_REVERSE_MODE */
			mutex_lock(&pca9482->lock);
			pca9482->timer_id = TIMER_CHECK_REVERSE_ACTIVE;
			pca9482->timer_period = REVERSE_WAIT_T;
			mutex_unlock(&pca9482->lock);
			queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
			ret = 0;
		} else {
			pr_err("%s: retry fail\n", __func__);
			/* Disable reverse mode */
			ret = pca9482_set_reverse_mode(pca9482, false);
			if (ret < 0)
				goto error;
			/* Softreset */
			ret = pca9482_softreset(pca9482);
			if (ret < 0)
				goto error;
			/* Notify maximum retry error */
			ret = -EINVAL;
			/* Stop charging in timer_work */
		}
	} else {
		pr_err("%s: reverse mode setting fail\n", __func__);
		/* Implement error handler function if it is needed */
		/* Disable reverse mode */
		ret = pca9482_set_reverse_mode(pca9482, false);
		if (ret < 0)
			goto error;
		/* Softreset */
		ret = pca9482_softreset(pca9482);
		if (ret < 0)
			goto error;
		/* Stop charging in timer_work */
		ret = -EINVAL;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;

}

/* Check reverse mode status in polling time */
static int pca9482_charge_reverse_mode(struct pca9482_charger *pca9482)
{
	int ret;
	int vin, iin, vbat;

	pr_info("%s: =========START=========\n", __func__);

	ret = pca9482_check_reverse_error(pca9482);
	if (ret < 0) {
		/* Error happens and stop reverse mode */
		pr_info("%s: Error happens in reverse mode\n", __func__);
	} else {
		/* Check reverse mode status in polling time */
		/* Read VIN_ADC, IIN_ADC, and VBAT_ADC */
		vin = pca9482_read_adc(pca9482, ADCCH_VIN);
		iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
		vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
		pr_info("%s: reverse mode, vin=%d, iin=%d, vbat=%d\n",
				__func__, vin, iin, vbat);

		/* Set timer - 5s */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_CHECK_REVERSE_MODE;
		pca9482->timer_period = REVERSE_CHECK_T;	/* 5s */
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
	}

	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Enter direct charging algorithm */
static int pca9482_start_direct_charging(struct pca9482_charger *pca9482)
{
	int ret;
	unsigned int val;
	u8 reg_val[REG_BUFFER_MAX];
	union power_supply_propval prop_val;

	pr_info("%s: =========START=========\n", __func__);

#ifdef CONFIG_SEC_FACTORY
	/* Set OV_TRACK_EN to disable */
	/* Factory mode just use VIN_FIXED_OVP_EN(10.5V) and don't use OV_TRACKING_EN */
	val = 0 << MASK2SHIFT(PCA9482_BIT_OV_TRACKING_EN);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_2,
							PCA9482_BIT_OV_TRACKING_EN, val);
#else
	/* Set OV_TRACK_DELTA to 800mV */
	val = OV_TRACK_DELTA_800mV << MASK2SHIFT(PCA9482_BIT_OV_TRACK_DELTA);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_2,
							PCA9482_BIT_OV_TRACK_DELTA, val);
#endif
	if (ret < 0)
		return ret;

	/* Set UV_TRACK_DELTA to 0mV */
	val = UV_TRACK_DELTA_0mV << MASK2SHIFT(PCA9482_BIT_UV_TRACK_DELTA);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_1,
							PCA9482_BIT_UV_TRACK_DELTA, val);
	if (ret < 0)
		return ret;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* Set watchdog timer enable */
	pca9482_set_wdt_enable(pca9482, WDT_ENABLE);
	pca9482_check_wdt_control(pca9482);
#endif

	/* Set Switching Frequency - kHz unit */
	val = PCA9482_FSW_CFG(pca9482->pdata->fsw_cfg);
	ret = pca9482_write_reg(pca9482, PCA9482_REG_SC_CNTL_0, val);
	if (ret < 0)
		return ret;
	pr_info("%s: sw_freq=%dkHz\n", __func__, pca9482->pdata->fsw_cfg);

	/* Set VIN_CURRENT_OCP_21_11 to 1000mA */
	val = PCA9482_BIT_VIN_CURRENT_OCP_21_11;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_0,
							PCA9482_BIT_VIN_CURRENT_OCP_21_11, val);
	if (ret < 0)
		return ret;

	/* Set EN_CFG to active high - Disable PCA9482 */
	val =  PCA9482_EN_ACTIVE_H;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_2,
							PCA9482_BIT_EN_CFG, val);
	if (ret < 0)
		return ret;

	/* Set NTC0 voltage threshold - cold or cool condition */
	val = PCA9482_NTC_TRIGGER_VOLTAGE(pca9482->pdata->ntc0_th);
	val = val << MASK2SHIFT(PCA9482_BIT_NTC_0_TRIGGER_VOLTAGE);
	ret = pca9482_write_reg(pca9482, PCA9482_REG_NTC_0_CNTL, val | PCA9482_BIT_NTC_EN);
	if (ret < 0)
		return ret;

	/* Set NTC1 voltage threshold - warm or hot condition */
	val = PCA9482_NTC_TRIGGER_VOLTAGE(pca9482->pdata->ntc1_th);
	ret = pca9482_write_reg(pca9482, PCA9482_REG_NTC_1_CNTL, val);
	if (ret < 0)
		return ret;

	/* Get TA type information from battery psy */

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_ONLINE, prop_val);

	if (prop_val.intval == POWER_SUPPLY_TYPE_WIRELESS) {
		/* The present power supply type is wireless charger */
		pca9482->ta_type = TA_TYPE_WIRELESS;
	} else if (prop_val.intval == SEC_BATTERY_CABLE_FPDO_DC) {
		/* The present power supply type is USBPD charger with only fixed PDO */
		pca9482->ta_type = TA_TYPE_USBPD_20;
	} else if (prop_val.intval == SEC_BATTERY_CABLE_PDIC_APDO) {
		/* The present power supply type is USBPD with APDO */
		pca9482->ta_type = TA_TYPE_USBPD;
	} else {
		/* DC cannot support the present power supply type - unknown power supply type */
		pca9482->ta_type = TA_TYPE_UNKNOWN;
	}
	pr_info("%s: ta_type = %d\n", __func__, pca9482->ta_type);
	/* wake lock */
	__pm_stay_awake(pca9482->monitor_wake_lock);

	/* Clear all interrupt registers before starting DC for debugging */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_INT_DEVICE_0 | PCA9482_REG_BIT_AI,
							&reg_val[REG_DEVICE_0], REG_BUFFER_MAX);
	if (ret < 0)
		return ret;
	pr_info("%s: reg[0x01]=0x%x,[0x02]=0x%x,[0x03]=0x%x,[0x04]=0x%x,[0x05]=0x%x,[0x06]=0x%x,[0x07]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Preset charging configuration and TA condition */
	ret = pca9482_preset_dcmode(pca9482);

	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* Check Vbat minimum level to start direct charging */
static int pca9482_check_vbatmin(struct pca9482_charger *pca9482)
{
	int vbat;
	int ret;
	union power_supply_propval val;

	pr_info("%s: =========START=========\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_set_charging_state(pca9482, DC_STATE_CHECK_VBAT);
#else
	pca9482->charging_state = DC_STATE_CHECK_VBAT;
#endif
	/* Check Vbat */
	vbat = pca9482_read_adc(pca9482, ADCCH_BATP_BATN);
	if (vbat < 0) {
		ret = vbat;
	}

	/* Read switching charger status */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ret = psy_do_property(pca9482->pdata->sec_dc_name, get,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
#else
	ret = pca9482_get_switching_charger_property(POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
#endif
	if (ret < 0) {
		/* Start Direct Charging again after 1sec */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_VBATMIN_CHECK;
		pca9482->timer_period = VBATMIN_CHECK_T;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		goto error;
	}

	if (val.intval == 0) {
		/* already disabled switching charger */
		/* Clear retry counter */
		pca9482->retry_cnt = 0;
		/* Preset TA voltage and DC parameters */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_PRESET_DC;
		pca9482->timer_period = 0;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
	} else {
		/* Switching charger is enabled */
		if (vbat > DC_VBAT_MIN) {
			/* Start Direct Charging */
			/* now switching charger is enabled */
			/* disable switching charger first */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9482_set_switching_charger(pca9482, false);
#else
			ret = pca9482_set_switching_charger(false, 0, 0, 0);
#endif
		}

		/* Wait 1sec for stopping switching charger or Start 1sec timer for battery check */
		mutex_lock(&pca9482->lock);
		pca9482->timer_id = TIMER_VBATMIN_CHECK;
		pca9482->timer_period = VBATMIN_CHECK_T;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* delayed work function for charging timer */
static void pca9482_timer_work(struct work_struct *work)
{
	struct pca9482_charger *pca9482 = container_of(work, struct pca9482_charger,
						 timer_work.work);
	int ret = 0;
	unsigned int val;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval value = {0,};
	int wire_status = 0, ta_alert_mode = 0;

	psy_do_property("battery", get,
			POWER_SUPPLY_EXT_PROP_CHARGE_COUNTER_SHADOW, value);
	wire_status = value.intval;

	psy_do_property("battery", get,
			POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, value);
	ta_alert_mode = value.intval;

	if ((wire_status == SEC_BATTERY_CABLE_NONE) && pca9482->mains_online) {
		if (ta_alert_mode > OCP_NONE)
			goto error;
		else
			return;
	}
#endif

	pr_info("%s: timer id=%d, charging_state=%d\n",
		__func__, pca9482->timer_id, pca9482->charging_state);

	/* Check req_enable flag */
	if (pca9482->req_enable == false) {
		/* This case is when battery driver set to stop DC during timer_work is workinig */
		/* And after resuming time_work, timer_id is overwritten by pca9482 function */
		/* Timer id shall be TIMER_ID_NONE */
		pca9482->timer_id = TIMER_ID_NONE;
		pr_info("%s: req_enable=%d, timer id=%d, charging_state=%d\n",
			__func__, pca9482->req_enable, pca9482->timer_id, pca9482->charging_state);
	}

	switch (pca9482->timer_id) {
	case TIMER_VBATMIN_CHECK:
		ret = pca9482_check_vbatmin(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PRESET_DC:
		ret = pca9482_start_direct_charging(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PRESET_CONFIG:
		ret = pca9482_preset_config(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_ACTIVE:
		ret = pca9482_check_active_state(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ADJUST_CCMODE:
		ret = pca9482_charge_adjust_ccmode(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ENTER_CCMODE:
		ret = pca9482_charge_start_ccmode(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_CCMODE:
		ret = pca9482_charge_ccmode(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ENTER_CVMODE:
		/* Enter Pre-CV mode */
		ret = pca9482_charge_start_cvmode(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_CVMODE:
		ret = pca9482_charge_cvmode(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PDMSG_SEND:
		/* Adjust TA current and voltage step */
		if (pca9482->ta_type == TA_TYPE_WIRELESS) {
			val = pca9482->ta_vol/WCRX_VOL_STEP;		/* RX voltage resolution is 100mV */
			pca9482->ta_vol = val*WCRX_VOL_STEP;

			/* Set RX voltage */
			ret = pca9482_send_rx_voltage(pca9482, WCRX_REQUEST_VOLTAGE);
		} else if (pca9482->ta_type == TA_TYPE_USBPD_20) {
			/* Send PD Message */
			ret = pca9482_send_pd_message(pca9482, PD_MSG_REQUEST_FIXED_PDO);
		} else {
			val = pca9482->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
			pca9482->ta_vol = val*PD_MSG_TA_VOL_STEP;
			val = pca9482->ta_cur/PD_MSG_TA_CUR_STEP;	/* PPS current resolution is 50mA */
			pca9482->ta_cur = val*PD_MSG_TA_CUR_STEP;
			if (pca9482->ta_cur < TA_MIN_CUR)	/* PPS minimum current is 1000mA */
				pca9482->ta_cur = TA_MIN_CUR;

			/* Send PD Message */
			ret = pca9482_send_pd_message(pca9482, PD_MSG_REQUEST_APDO);
		}
		if (ret < 0)
			goto error;

		/* Go to the next state */
		mutex_lock(&pca9482->lock);
		switch (pca9482->charging_state) {
		case DC_STATE_PRESET_DC:
			pca9482->timer_id = TIMER_PRESET_CONFIG;
			break;
		case DC_STATE_ADJUST_CC:
			pca9482->timer_id = TIMER_ADJUST_CCMODE;
			break;
		case DC_STATE_START_CC:
			pca9482->timer_id = TIMER_ENTER_CCMODE;
			break;
		case DC_STATE_CC_MODE:
			pca9482->timer_id = TIMER_CHECK_CCMODE;
			break;
		case DC_STATE_START_CV:
			pca9482->timer_id = TIMER_ENTER_CVMODE;
			break;
		case DC_STATE_CV_MODE:
			pca9482->timer_id = TIMER_CHECK_CVMODE;
			break;
		case DC_STATE_ADJUST_TAVOL:
			pca9482->timer_id = TIMER_ADJUST_TAVOL;
			break;
		case DC_STATE_ADJUST_TACUR:
			pca9482->timer_id = TIMER_ADJUST_TACUR;
			break;
		case DC_STATE_BYPASS_MODE:
			pca9482->timer_id = TIMER_CHECK_BYPASSMODE;
			break;
		case DC_STATE_DCMODE_CHANGE:
			pca9482->timer_id = TIMER_DCMODE_CHANGE;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		pca9482->timer_period = PDMSG_WAIT_T;
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
		break;

	case TIMER_ADJUST_TAVOL:
		ret = pca9482_adjust_ta_voltage(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ADJUST_TACUR:
		ret = pca9482_adjust_ta_current(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_BYPASSMODE:
		ret = pca9482_charge_bypass_mode(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_DCMODE_CHANGE:
		ret = pca9482_charge_dcmode_change(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_START_REVERSE:
		ret = pca9482_charge_start_reverse(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_REVERSE_ACTIVE:
		ret = pca9482_check_reverse_active_state(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_REVERSE_MODE:
		ret = pca9482_charge_reverse_mode(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_FPDOCVMODE:
		ret = pca9482_charge_fpdo_cvmode(pca9482);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ID_NONE:
		ret = pca9482_stop_charging(pca9482);
		if (ret < 0)
			goto error;
		break;

	default:
		break;
	}

	/* Check the charging state again */
	if (pca9482->charging_state == DC_STATE_NO_CHARGING) {
		/* Cancel work queue again */
		cancel_delayed_work(&pca9482->timer_work);
		cancel_delayed_work(&pca9482->pps_work);
	}
	return;

error:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	pca9482->health_status = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
#endif
	pca9482_stop_charging(pca9482);
}

/* delayed work function for pps periodic timer */
static void pca9482_pps_request_work(struct work_struct *work)
{
	struct pca9482_charger *pca9482 = container_of(work, struct pca9482_charger,
						 pps_work.work);

	int ret = 0;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int vin, iin;

	/* this is for wdt */
	vin = pca9482_read_adc(pca9482, ADCCH_VIN);
	iin = pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
	pr_info("%s: pps_work_start (vin:%dmV, iin:%dmA)\n",
			__func__, vin/PCA9482_SEC_DENOM_U_M, iin/PCA9482_SEC_DENOM_U_M);
#else
	pr_info("%s: pps_work_start\n", __func__);
#endif

#if defined(CONFIG_SEND_PDMSG_IN_PPS_REQUEST_WORK)
	/* Send PD message */
	ret = pca9482_send_pd_message(pca9482, PD_MSG_REQUEST_APDO);
#endif
	pr_info("%s: End, ret=%d\n", __func__, ret);
}

static int pca9482_hw_init(struct pca9482_charger *pca9482)
{
	unsigned int val;
	int ret;
	u8 mask[REG_BUFFER_MAX];

	pr_info("%s: =========START=========\n", __func__);

	/* Read Device info register */
	ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_ID, &val);
	if ((ret < 0) || (val != PCA9482_DEVICE_ID)) {
		/* Read Device info register again */
		ret = pca9482_read_reg(pca9482, PCA9482_REG_DEVICE_ID, &val);
		if ((ret < 0) || (val != PCA9482_DEVICE_ID)) {
			dev_err(pca9482->dev, "reading DEVICE_INFO failed, val=0x%x\n", val);
			ret = -EINVAL;
			return ret;
		}
	}
	dev_info(pca9482->dev, "%s: reading DEVICE_INFO, val=0x%x\n", __func__, val);

	/*
	 * Program the platform specific configuration values to the device
	 * first.
	 */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	pca9482->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif

#ifdef CONFIG_SEC_FACTORY
	/* Set OV_TRACK_EN to disable */
	/* Factory mode just use VIN_FIXED_OVP_EN(10.5V) and don't use OV_TRACKING_EN */
	val = 0 << MASK2SHIFT(PCA9482_BIT_OV_TRACKING_EN);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_2,
							PCA9482_BIT_OV_TRACKING_EN, val);
#else
	/* Set OV_TRACK_DELTA to 800mV */
	val = OV_TRACK_DELTA_800mV << MASK2SHIFT(PCA9482_BIT_OV_TRACK_DELTA);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_2,
							PCA9482_BIT_OV_TRACK_DELTA, val);
#endif
	if (ret < 0)
		return ret;

	/* Set UV_TRACK_DELTA to 0mV */
	val = UV_TRACK_DELTA_0mV << MASK2SHIFT(PCA9482_BIT_UV_TRACK_DELTA);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_1,
							PCA9482_BIT_UV_TRACK_DELTA, val);
	if (ret < 0)
		return ret;

	/* Set Switching Frequency - kHz unit */
	val = PCA9482_FSW_CFG(pca9482->pdata->fsw_cfg);
	ret = pca9482_write_reg(pca9482, PCA9482_REG_SC_CNTL_0, val);
	if (ret < 0)
		return ret;

	/* Set VIN_CURRENT_OCP_21_11 to 1000mA */
	val = PCA9482_BIT_VIN_CURRENT_OCP_21_11;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_0,
							PCA9482_BIT_VIN_CURRENT_OCP_21_11, val);
	if (ret < 0)
		return ret;

	/* Set Reverse Current Detection */
	val = PCA9482_BIT_RCP_EN;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_RCP_CNTL,
							PCA9482_BIT_RCP_EN, val);
	if (ret < 0)
		return ret;

	/* Set EN pin polarity - Disable PCA9482 */
	val = PCA9482_EN_ACTIVE_H;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_2,
							PCA9482_BIT_EN_CFG, val);
	if (ret < 0)
		return ret;

	/* Set standby mode*/
	val = PCA9482_STANDBY_FORCE;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_SC_CNTL_3,
							PCA9482_BIT_STANDBY_EN | PCA9482_BIT_SC_OPERATION_MODE,
							val | PCA9482_SC_OP_21);
	if (ret < 0)
		return ret;

	/* VIN_OVP 10.8V */
	/* Die Temperature regulation 120'C */
	val = (0x3 << MASK2SHIFT(PCA9482_BIT_VIN_OVP_CFG)) | (0x3 << MASK2SHIFT(PCA9482_BIT_THERMAL_REGULATION_CFG));
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_1,
							PCA9482_BIT_VIN_OVP_CFG | PCA9482_BIT_THERMAL_REGULATION_CFG | PCA9482_BIT_THERMAL_REGULATION_EN,
							val | PCA9482_BIT_THERMAL_REGULATION_EN);
	if (ret < 0)
		return ret;

	/* Set external sense resistor value */
	val = pca9482->pdata->snsres << MASK2SHIFT(PCA9482_BIT_IBAT_SENSE_R_SEL);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_4,
							PCA9482_BIT_IBAT_SENSE_R_SEL, val);
	if (ret < 0)
		return ret;

	/* Set external sense resistor location */
	val = pca9482->pdata->snsres_cfg << MASK2SHIFT(PCA9482_BIT_IBAT_SENSE_R_CFG);
	ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_2,
							PCA9482_BIT_IBAT_SENSE_R_CFG, val);
	if (ret < 0)
		return ret;

	/* Disable battery charge current regulation loop */
	/* Disable current measurement through CSP and CSN */
	val = 0;
	ret = pca9482_update_reg(pca9482, PCA9482_REG_CHARGING_CNTL_0,
							PCA9482_BIT_CSP_CSN_MEASURE_EN | PCA9482_BIT_I_VBAT_LOOP_EN,
							val);
	if (ret < 0)
		return ret;

	/* Set the ADC channel */
	val = (PCA9482_BIT_ADC_READ_VIN_CURRENT_EN |	/* IIN ADC */
		   PCA9482_BIT_ADC_READ_DIE_TEMP_EN |		/* DIE_TEMP ADC */
		   PCA9482_BIT_ADC_READ_NTC_EN |			/* NTC ADC */
		   PCA9482_BIT_ADC_READ_VOUT_EN |			/* VOUT ADC */
		   PCA9482_BIT_ADC_READ_BATP_BATN_EN |		/* VBAT ADC */
		   PCA9482_BIT_ADC_READ_OVP_OUT_EN |		/* OVP_OUT ADC */
		   PCA9482_BIT_ADC_READ_VIN_EN);			/* VIN ADC */

	ret = pca9482_write_reg(pca9482, PCA9482_REG_ADC_EN_CNTL_0, val);
	if (ret < 0)
		return ret;

	/* Enable ADC */
	val = PCA9482_BIT_ADC_EN | (ADC_AVG_16sample << MASK2SHIFT(PCA9482_BIT_ADC_AVERAGE_TIMES));
	ret = pca9482_write_reg(pca9482, PCA9482_REG_ADC_CNTL, val);
	if (ret < 0)
		return ret;

	/*
	 * Configure the Mask Register for interrupts: disable all interrupts by default.
	 */
	mask[REG_DEVICE_0] = 0xFF;
	mask[REG_DEVICE_1] = 0xFF;
	mask[REG_DEVICE_2] = 0xFF;
	mask[REG_DEVICE_3] = 0xFF;
	mask[REG_CHARGING] = 0xFF;
	mask[REG_SC_0] = 0xFF;
	mask[REG_SC_1] = 0xFF;
	ret = regmap_bulk_write(pca9482->regmap, PCA9482_REG_INT_DEVICE_0_MASK | PCA9482_REG_BIT_AI,
							mask, REG_BUFFER_MAX);
	if (ret < 0)
		return ret;

	/* input current - uA*/
	ret = pca9482_set_input_current(pca9482, pca9482->pdata->iin_cfg);
	if (ret < 0)
		return ret;

	/* charging current */
	ret = pca9482_set_charging_current(pca9482, pca9482->pdata->ichg_cfg);
	if (ret < 0)
		return ret;

	/* float voltage */
	ret = pca9482_set_vfloat(pca9482, pca9482->pdata->vfloat);
	if (ret < 0)
		return ret;

	/* Save initial charging parameters */
	pca9482->iin_cfg = pca9482->pdata->iin_cfg;
	pca9482->ichg_cfg = pca9482->pdata->ichg_cfg;
	pca9482->vfloat = pca9482->pdata->vfloat;
	pca9482->max_vfloat = pca9482->pdata->vfloat;
	pca9482->iin_topoff = pca9482->pdata->iin_topoff;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482->fpdo_dc_iin_topoff = pca9482->pdata->fpdo_dc_iin_topoff;
	pca9482->fpdo_dc_vnow_topoff = pca9482->pdata->fpdo_dc_vnow_topoff;
	pca9482->fpdo_dc_mainbat_topoff = pca9482->pdata->fpdo_dc_mainbat_topoff;
	pca9482->fpdo_dc_subbat_topoff = pca9482->pdata->fpdo_dc_subbat_topoff;
#endif

	/* Clear new iin and new vfloat */
	pca9482->new_iin = 0;
	pca9482->new_vfloat = 0;

	/* Initial TA control method is Current Limit mode */
	pca9482->ta_ctrl = TA_CTRL_CL_MODE;

	/* Clear switching frequency change sequence */
	pca9482->req_sw_freq = REQ_SW_FREQ_0;

	/* Set vfloat decrement flag to false by default */
	pca9482->dec_vfloat = false;

	/* Clear charging done counter */
	pca9482->done_cnt = 0;

	/* Read PCA9482 VBAT_ADC */
	pca9482_read_vbat_adc(pca9482);
	return ret;
}


static irqreturn_t pca9482_interrupt_handler(int irq, void *data)
{
	struct pca9482_charger *pca9482 = data;
	u8 int_reg[REG_BUFFER_MAX], sts_reg[REG_BUFFER_MAX], mask_reg[REG_BUFFER_MAX];
	u8 masked_int[REG_BUFFER_MAX];	/* masked int */
	bool handled = false;
	int ret, i;

	/* Read interrupt registers */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_INT_DEVICE_0 | PCA9482_REG_BIT_AI, int_reg, REG_BUFFER_MAX);
	if (ret < 0) {
		dev_err(pca9482->dev, "reading Interrupt registers failed\n");
		handled = false;
		goto error;
	}
	pr_info("%s: INT reg[0x01]=0x%x,[0x02]=0x%x,[0x03]=0x%x,[0x04]=0x%x,[0x05]=0x%x,[0x06]=0x%x,[0x07]=0x%x\n",
			__func__, int_reg[0], int_reg[1], int_reg[2], int_reg[3], int_reg[4], int_reg[5], int_reg[6]);

	/* Read mask registers */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_INT_DEVICE_0_MASK | PCA9482_REG_BIT_AI, mask_reg, REG_BUFFER_MAX);
	if (ret < 0) {
		dev_err(pca9482->dev, "reading Mask registers failed\n");
		handled = false;
		goto error;
	}
	pr_info("%s: MASK reg[0x08]=0x%x,[0x09]=0x%x,[0x0A]=0x%x,[0x0B]=0x%x,[0x0C]=0x%x,[0x0D]=0x%x,[0x0E]=0x%x\n",
			__func__, mask_reg[0], mask_reg[1], mask_reg[2], mask_reg[3], mask_reg[4], mask_reg[5], mask_reg[6]);

	/* Read status registers */
	ret = pca9482_bulk_read_reg(pca9482, PCA9482_REG_DEVICE_0_STS | PCA9482_REG_BIT_AI, sts_reg, REG_BUFFER_MAX);
	if (ret < 0) {
		dev_err(pca9482->dev, "reading Status registers failed\n");
		handled = false;
		goto error;
	}
	pr_info("%s: STS reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
			__func__, sts_reg[0], sts_reg[1], sts_reg[2], sts_reg[3], sts_reg[4], sts_reg[5], sts_reg[6]);

	/* Check the masked interrupt */
	for (i = 0; i < REG_BUFFER_MAX; i++) {
		masked_int[i] = int_reg[i] & !mask_reg[i];
	}
	pr_info("%s: Masked INT reg[0x01]=0x%x,[0x02]=0x%x,[0x03]=0x%x,[0x04]=0x%x,[0x05]=0x%x,[0x06]=0x%x,[0x07]=0x%x\n",
			__func__, masked_int[0], masked_int[1], masked_int[2], masked_int[3], masked_int[4], masked_int[5], masked_int[6]);

	handled = true;

	/* Should implement code by a customer if pca9482 needs additional functions or actions */

error:
	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int pca9482_irq_init(struct pca9482_charger *pca9482,
			   struct i2c_client *client)
{
	const struct pca9482_platform_data *pdata = pca9482->pdata;
	int ret, msk[REG_BUFFER_MAX], irq;

	pr_info("%s: =========START=========\n", __func__);

	irq = gpio_to_irq(pdata->irq_gpio);

	ret = gpio_request_one(pdata->irq_gpio, GPIOF_IN, client->name);
	if (ret < 0)
		goto fail;

	ret = request_threaded_irq(irq, NULL, pca9482_interrupt_handler,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   client->name, pca9482);
	if (ret < 0)
		goto fail_gpio;

	/*
	 * Configure the Mask Register for interrupts: disable all interrupts by default.
	 */
	msk[REG_DEVICE_0] = 0xFF;
	msk[REG_DEVICE_1] = 0xFF;
	msk[REG_DEVICE_2] = 0xFF;
	msk[REG_DEVICE_3] = 0xFF;
	msk[REG_CHARGING] = 0xFF;
	msk[REG_SC_0] = 0xFF;
	msk[REG_SC_1] = 0xFF;

	ret = regmap_bulk_write(pca9482->regmap, PCA9482_REG_INT_DEVICE_0_MASK | PCA9482_REG_BIT_AI,
							msk, REG_BUFFER_MAX);
	if (ret < 0)
		goto fail_write;

	client->irq = irq;
	return 0;

fail_write:
	free_irq(irq, pca9482);
fail_gpio:
	gpio_free(pdata->irq_gpio);
fail:
	client->irq = 0;
	return ret;
}


/*
 * Returns the input current limit programmed
 * into the charger in uA.
 */
static int get_input_current_limit(struct pca9482_charger *pca9482)
{
	int ret, intval;
	unsigned int val;

	ret = pca9482_read_reg(pca9482, PCA9482_REG_CHARGING_CNTL_1, &val);
	if (ret < 0)
		return ret;

	intval = val * PCA9482_IIN_REG_STEP + PCA9482_IIN_REG_MIN;

	if (intval > PCA9482_IIN_REG_MAX)
		intval = PCA9482_IIN_REG_MAX;

	return intval;
}

/*
 * Returns the constant charge current programmed
 * into the charger in uA.
 */
static int get_const_charge_current(struct pca9482_charger *pca9482)
{
	int ret, intval;
	unsigned int val;

	ret = pca9482_read_reg(pca9482, PCA9482_REG_CHARGING_CNTL_3, &val);
	if (ret < 0)
		return ret;

	intval = val * PCA9482_IBAT_REG_STEP + PCA9482_IBAT_REG_MIN;

	if (intval > PCA9482_IBAT_REG_MAX)
		intval = PCA9482_IBAT_REG_MAX;

	return intval;
}

/*
 * Returns the constant charge voltage programmed
 * into the charger in uV.
 */
static int get_const_charge_voltage(struct pca9482_charger *pca9482)
{
	int ret, intval;
	unsigned int val;

	ret = pca9482_read_reg(pca9482, PCA9482_REG_CHARGING_CNTL_2, &val);
	if (ret < 0)
		return ret;

	intval = val * PCA9482_VBAT_REG_STEP + PCA9482_VBAT_REG_MIN;

	if (intval > PCA9482_VBAT_REG_MAX)
		intval = PCA9482_VBAT_REG_MAX;

	return intval;
}

/*
 * Returns the enable or disable value.
 * into 1 or 0.
 */
static int get_charging_enabled(struct pca9482_charger *pca9482)
{
	int ret, intval;
	unsigned int val;

	ret = pca9482_read_reg(pca9482, PCA9482_REG_SC_CNTL_3, &val);
	if (ret < 0)
		return ret;

	intval = (val & PCA9482_BIT_STANDBY_EN) ? 0 : 1;

	return intval;
}

static int pca9482_chg_set_adc_force_mode(struct pca9482_charger *pca9482, u8 enable)
{
	unsigned int temp = 0;
	int ret = 0;

	if (enable) {
		/* Disable low power mode */
		temp = PCA9482_BIT_LOW_POWER_MODE_DISABLE;
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_1,
				PCA9482_BIT_LOW_POWER_MODE_DISABLE, temp);
		if (ret < 0)
			return ret;

		/* Set Forced Normal Mode to ADC mode */
		temp = FORCE_NORMAL_MODE << MASK2SHIFT(PCA9482_BIT_ADC_MODE_CFG);
		ret = pca9482_update_reg(pca9482, PCA9482_REG_ADC_CNTL,
				PCA9482_BIT_ADC_MODE_CFG, temp);
		if (ret < 0)
			return ret;

		/* Wait 2ms to update ADC */
		usleep_range(2000, 3000);
	} else {
		/* Set Auto Mode to ADC mode */
		temp = AUTO_MODE << MASK2SHIFT(PCA9482_BIT_ADC_MODE_CFG);
		ret = pca9482_update_reg(pca9482, PCA9482_REG_ADC_CNTL,
				PCA9482_BIT_ADC_MODE_CFG, temp);
		if (ret < 0)
			return ret;

		/* Enable low power mode */
		temp = 0;
		ret = pca9482_update_reg(pca9482, PCA9482_REG_DEVICE_CNTL_1,
				PCA9482_BIT_LOW_POWER_MODE_DISABLE, temp);
		if (ret < 0)
			return ret;
	}

	ret = pca9482_read_reg(pca9482, PCA9482_REG_ADC_CNTL, &temp);
	pr_info("%s: ADC_CTRL : 0x%02x\n", __func__, temp);

	return ret;
}

static int pca9482_chg_set_property(struct power_supply *psy,
				     enum power_supply_property prop,
				     const union power_supply_propval *val)
{
	struct pca9482_charger *pca9482 = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) prop;
	unsigned int temp = 0;
#endif
	int ret = 0;

	pr_info("%s: =========START=========\n", __func__);
	pr_info("%s: prop=%d, val=%d\n", __func__, prop, val->intval);

	switch ((int)prop) {
	/* Todo - Insert code */
	/* It needs modification by a customer */
	case POWER_SUPPLY_PROP_ONLINE:
		/* Check whether pca9482 is in reverse mode */
		if (pca9482->rev_mode == POWER_SUPPLY_DC_REVERSE_STOP) {
			if (val->intval == 0) {
				pca9482->mains_online = false;
				/* Check TA detachment and clear new_iin */
				pca9482->new_iin = 0;
			} else {
				pca9482->mains_online = true;
			}
		} else {
			/* PCA9482 is in reverse bypass mode or reverse 1:2 switching mode */
			/* Ignore online property setting */
			pr_info("%s: pca9482 is in reverse mode(%d), so ignore ONLINE property setting\n",
				__func__, pca9482->rev_mode);
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		/* Set the USBPD-TA is plugged in or out */
		pca9482->mains_online = val->intval;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		/* Set power supply type */
		if (val->intval == POWER_SUPPLY_TYPE_WIRELESS) {
			/* The current power supply type is wireless charger */
			pca9482->ta_type = TA_TYPE_WIRELESS;
			pr_info("%s: The current power supply type is WC, ta_type=%d\n", __func__, pca9482->ta_type);
		} else {
			/* Default TA type is USBPD TA */
			pca9482->ta_type = TA_TYPE_USBPD;
			pr_info("%s: The current power supply type is USBPD, ta_type=%d\n", __func__, pca9482->ta_type);
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9482->float_voltage = val->intval;
		temp = pca9482->float_voltage * PCA9482_SEC_DENOM_U_M;
		if (temp != pca9482->new_vfloat) {
			/* request new float voltage */
			pca9482->new_vfloat = temp;
			/* Check the charging state */
			if ((pca9482->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9482->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new vfloat when the direct charging is started */
				pca9482->vfloat = pca9482->new_vfloat;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9482->req_new_vfloat == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New vfloat\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9482->lock);
					pca9482->req_new_vfloat = true;
					mutex_unlock(&pca9482->lock);

					/* Check the charging state */
					if ((pca9482->charging_state == DC_STATE_CC_MODE) ||
						(pca9482->charging_state == DC_STATE_CV_MODE) ||
						(pca9482->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9482->charging_state == DC_STATE_CHARGING_DONE) ||
						(pca9482->charging_state == DC_STATE_FPDO_CV_MODE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9482->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9482->lock);
						pca9482->timer_period = 0;	// ms unit
						mutex_unlock(&pca9482->lock);
						queue_delayed_work(pca9482->dc_wq,
											&pca9482->timer_work,
											msecs_to_jiffies(pca9482->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new vfloat yet in charging state=%d\n",
								__func__, pca9482->charging_state);
					}
				}
			}
		}
#else
		if (val->intval != pca9482->new_vfloat) {
			/* request new float voltage */
			pca9482->new_vfloat = val->intval;
			/* Check the charging state */
			if ((pca9482->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9482->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new vfloat when the direct charging is started */
				pca9482->vfloat = pca9482->new_vfloat;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9482->req_new_vfloat == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New vfloat\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9482->lock);
					pca9482->req_new_vfloat = true;
					mutex_unlock(&pca9482->lock);

					/* Check the charging state */
					if ((pca9482->charging_state == DC_STATE_CC_MODE) ||
						(pca9482->charging_state == DC_STATE_CV_MODE) ||
						(pca9482->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9482->charging_state == DC_STATE_CHARGING_DONE) ||
						(pca9482->charging_state == DC_STATE_FPDO_CV_MODE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9482->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9482->lock);
						pca9482->timer_period = 0;	// ms unit
						mutex_unlock(&pca9482->lock);
						queue_delayed_work(pca9482->dc_wq,
											&pca9482->timer_work,
											msecs_to_jiffies(pca9482->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new vfloat yet in charging state=%d\n",
								__func__, pca9482->charging_state);
					}
				}
			}
		}
#endif
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9482->input_current = val->intval;
		temp = pca9482->input_current * PCA9482_SEC_DENOM_U_M;
		if (pca9482->ta_type == TA_TYPE_USBPD_20 && temp < pca9482->pdata->fpdo_dc_iin_lowest_limit) {
			pr_info("%s: PSP_ICL, IIN LOWEST LIMIT! IIN %d -> %d\n", __func__,
					temp, pca9482->pdata->fpdo_dc_iin_lowest_limit);
			temp = pca9482->pdata->fpdo_dc_iin_lowest_limit;
		}
		if (temp != pca9482->new_iin) {
			/* Compare with topoff current */
			if (temp < pca9482->iin_topoff) {
				/* This new iin is abnormal input current */
				pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, temp);
				ret = -EINVAL;
				break;
			}
			/* request new input current */
			pca9482->new_iin = temp;
			/* Check the charging state */
			if ((pca9482->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9482->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new iin when the direct charging is started */
				pca9482->iin_cfg = pca9482->new_iin;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9482->req_new_iin == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New iin\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9482->lock);
					pca9482->req_new_iin = true;
					mutex_unlock(&pca9482->lock);
					/* Check the charging state */
					if ((pca9482->charging_state == DC_STATE_CC_MODE) ||
						(pca9482->charging_state == DC_STATE_CV_MODE) ||
						(pca9482->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9482->charging_state == DC_STATE_FPDO_CV_MODE) ||
						(pca9482->charging_state == DC_STATE_CHARGING_DONE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9482->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9482->lock);
						pca9482->timer_period = 0;	// ms unit
						mutex_unlock(&pca9482->lock);
						queue_delayed_work(pca9482->dc_wq,
											&pca9482->timer_work,
											msecs_to_jiffies(pca9482->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new iin yet in charging state=%d\n",
								__func__, pca9482->charging_state);
					}
				}
			}
		} else {
			/* Compare with topoff current */
			if (temp < pca9482->iin_topoff) {
				/* This new iin is abnormal input current */
				pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, temp);
				ret = -EINVAL;
			} else {
				/* new iin is same as previous new_iin, but iin_cfg is different from it */
				/* Check the charging state */
				if ((pca9482->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9482->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9482->iin_cfg = pca9482->new_iin;
					pr_info("%s: charging state=%d, new iin(%uA) and iin_cfg(%uA)\n",
						__func__, pca9482->charging_state, pca9482->new_iin, pca9482->iin_cfg);
				}
			}
		}
#else
		if (val->intval != pca9482->new_iin) {
			/* Compare with topoff current */
			if (val->intval < pca9482->iin_topoff) {
				/* This new iin is abnormal input current */
				pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, val->intval);
				ret = -EINVAL;
			} else {
				/* request new input current */
				pca9482->new_iin = val->intval;
				/* Check the charging state */
				if ((pca9482->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9482->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9482->iin_cfg = pca9482->new_iin;
				} else {
					/* Check whether the previous request is done or not */
					if (pca9482->req_new_iin == true) {
						/* The previous request is not done yet */
						pr_err("%s: There is the previous request for New iin\n", __func__);
						ret = -EBUSY;
					} else {
						/* Set request flag */
						mutex_lock(&pca9482->lock);
						pca9482->req_new_iin = true;
						mutex_unlock(&pca9482->lock);
						/* Check the charging state */
						if ((pca9482->charging_state == DC_STATE_CC_MODE) ||
							(pca9482->charging_state == DC_STATE_CV_MODE) ||
							(pca9482->charging_state == DC_STATE_BYPASS_MODE) ||
							(pca9482->charging_state == DC_STATE_FPDO_CV_MODE) ||
							(pca9482->charging_state == DC_STATE_CHARGING_DONE)) {
							/* cancel delayed_work */
							cancel_delayed_work(&pca9482->timer_work);
							/* do delayed work at once */
							mutex_lock(&pca9482->lock);
							pca9482->timer_period = 0;	// ms unit
							mutex_unlock(&pca9482->lock);
							queue_delayed_work(pca9482->dc_wq,
												&pca9482->timer_work,
												msecs_to_jiffies(pca9482->timer_period));
						} else {
							/* Wait for next valid state - cc, cv, or bypass state */
							pr_info("%s: Not support new iin yet in charging state=%d\n",
									__func__, pca9482->charging_state);
						}
					}
				}
			}
		} else {
			/* Compare with topoff current */
			if (val->intval < pca9482->iin_topoff) {
				/* This new iin is abnormal input current */
				pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, val->intval);
				ret = -EINVAL;
			} else {
				/* new iin is same as previous new_iin, but iin_cfg is different from it */
				/* Check the charging state */
				if ((pca9482->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9482->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9482->iin_cfg = pca9482->new_iin;
					pr_info("%s: charging state=%d, new iin(%uA) and iin_cfg(%uA)\n",
						__func__, pca9482->charging_state, pca9482->new_iin, pca9482->iin_cfg);
				}
			}
		}
#endif
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		pca9482->pdata->vfloat = val->intval * PCA9482_SEC_DENOM_U_M;
		pr_info("%s: v_float(%duV)\n", __func__, pca9482->pdata->vfloat);
		/* Save maximum vfloat to max_vfloat */
		pca9482->max_vfloat = pca9482->pdata->vfloat;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pca9482->charging_current = val->intval;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			if (val->intval) {
				pca9482->wdt_kick = true;
			} else {
				pca9482->wdt_kick = false;
				cancel_delayed_work(&pca9482->wdt_control_work);
			}
			pr_info("%s: wdt kick (%d)\n", __func__, pca9482->wdt_kick);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			pca9482->input_current = val->intval;
			temp = pca9482->input_current * PCA9482_SEC_DENOM_U_M;
			if (pca9482->ta_type == TA_TYPE_USBPD_20 && temp < pca9482->pdata->fpdo_dc_iin_lowest_limit) {
				pr_info("%s: PSP_DCM, IIN LOWEST LIMIT! IIN %d -> %d\n", __func__,
						temp, pca9482->pdata->fpdo_dc_iin_lowest_limit);
				temp = pca9482->pdata->fpdo_dc_iin_lowest_limit;
			}
			if (temp != pca9482->new_iin) {
				/* request new input current */
				pca9482->new_iin = temp;
				/* Check the charging state */
				if ((pca9482->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9482->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9482->iin_cfg = pca9482->new_iin;
				} else {
					/* Check whether the previous request is done or not */
					if (pca9482->req_new_iin == true) {
						/* The previous request is not done yet */
						pr_err("%s: There is the previous request for New iin\n", __func__);
						ret = -EBUSY;
					} else {
						/* Set request flag */
						mutex_lock(&pca9482->lock);
						pca9482->req_new_iin = true;
						mutex_unlock(&pca9482->lock);
						/* Check the charging state */
						if ((pca9482->charging_state == DC_STATE_CC_MODE) ||
							(pca9482->charging_state == DC_STATE_CV_MODE) ||
							(pca9482->charging_state == DC_STATE_BYPASS_MODE) ||
							(pca9482->charging_state == DC_STATE_CHARGING_DONE)) {
							/* cancel delayed_work */
							cancel_delayed_work(&pca9482->timer_work);
							/* do delayed work at once */
							mutex_lock(&pca9482->lock);
							pca9482->timer_period = 0;	// ms unit
							mutex_unlock(&pca9482->lock);
							queue_delayed_work(pca9482->dc_wq,
												&pca9482->timer_work,
												msecs_to_jiffies(pca9482->timer_period));
						} else {
							/* Wait for next valid state - cc or cv state */
							pr_info("%s: Not support new iin yet in charging state=%d\n",
									__func__, pca9482->charging_state);
						}
					}
				}
				pr_info("## %s: input current(new_iin: %duA)\n", __func__, pca9482->new_iin);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
			pca9482_chg_set_adc_force_mode(pca9482, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval == 0) {
				/* Set req_enable flag to false */
				pca9482->req_enable = false;
				/* Stop direct charging */
				ret = pca9482_stop_charging(pca9482);
				pca9482->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
				pca9482->health_status = POWER_SUPPLY_HEALTH_GOOD;
				if (ret < 0)
					goto error;
			} else {
				if (pca9482->charging_state != DC_STATE_NO_CHARGING) {
					pr_info("## %s: duplicate charging enabled(%d)\n", __func__, val->intval);
					goto error;
				}
				if (!pca9482->mains_online) {
					pr_info("## %s: mains_online is not attached(%d)\n", __func__, val->intval);
					goto error;
				}
				/* Start Direct Charging */
				/* Set req_enable flag to true */
				pca9482->req_enable = true;
				/* Set initial wake up timeout - 10s */
				pm_wakeup_ws_event(pca9482->monitor_wake_lock, INIT_WAKEUP_T, false);
				/* Start 1sec timer for battery check */
				mutex_lock(&pca9482->lock);
				pca9482_set_charging_state(pca9482, DC_STATE_CHECK_VBAT);
				pca9482->timer_id = TIMER_VBATMIN_CHECK;
				pca9482->timer_period = VBATMIN_CHECK_T;	/* The delay time for PD state goes to PE_SNK_STATE */
				mutex_unlock(&pca9482->lock);
				queue_delayed_work(pca9482->dc_wq,
									&pca9482->timer_work,
									msecs_to_jiffies(pca9482->timer_period));
				ret = 0;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			pr_info("[PASS_THROUGH] %s: called\n", __func__);
			if (val->intval != pca9482->new_dc_mode) {
				/* Request new dc mode */
				pca9482->new_dc_mode = val->intval;
				/* Check the charging state */
				if (pca9482->charging_state == DC_STATE_NO_CHARGING) {
					/* Not support state */
					pr_info("%s: Not support dc mode in charging state=%d\n", __func__, pca9482->charging_state);
					ret = -EINVAL;
				} else {
					/* Check whether the previous request is done or not */
					if (pca9482->req_new_dc_mode == true) {
						/* The previous request is not done yet */
						pr_err("%s: There is the previous request for New DC mode\n", __func__);
						ret = -EBUSY;
					} else {
						/* Set request flag */
						mutex_lock(&pca9482->lock);
						pca9482->req_new_dc_mode = true;
						mutex_unlock(&pca9482->lock);
						/* Check the charging state */
						if ((pca9482->charging_state == DC_STATE_CC_MODE) ||
							(pca9482->charging_state == DC_STATE_CV_MODE) ||
							(pca9482->charging_state == DC_STATE_BYPASS_MODE)) {
							/* cancel delayed_work */
							cancel_delayed_work(&pca9482->timer_work);
							/* do delayed work at once */
							mutex_lock(&pca9482->lock);
							pca9482->timer_period = 0;	// ms unit
							mutex_unlock(&pca9482->lock);
							queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
						} else {
							/* Wait for next valid state - cc, cv, or bypass state */
							pr_info("%s: Not support new dc mode yet in charging state=%d\n",
									__func__, pca9482->charging_state);
						}
					}
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			if ((pca9482->charging_state == DC_STATE_BYPASS_MODE) &&
				(pca9482->dc_mode != PTM_NONE)) {
				pr_info("[PASS_THROUGH_VOL] %s, bypass mode\n", __func__);
				/* Set TA voltage for bypass mode */
				pca9482_set_bypass_ta_voltage_by_soc(pca9482, val->intval);
			} else {
				pr_info("[PASS_THROUGH_VOL] %s, not bypass mode\n", __func__);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_DC_REVERSE_MODE:
			/* Set reverse mode */
			pca9482->rev_mode = val->intval;
			/* Set reverse mode */
			if (pca9482->rev_mode == POWER_SUPPLY_DC_REVERSE_STOP) {
				/* Set req_enable flag to false */
				pca9482->req_enable = false;
				/* Cancel delayed_work */
				cancel_delayed_work(&pca9482->timer_work);
				/* Stop reverse mode */
				ret = pca9482_stop_charging(pca9482);
			} else {
				if (pca9482->charging_state == DC_STATE_NO_CHARGING) {
					/* Set req_enable flag to true */
					pca9482->req_enable = true;
					/* Start Reverse Mode */
					mutex_lock(&pca9482->lock);
					pca9482->charging_state = DC_STATE_REVERSE_MODE;
					pca9482->timer_id = TIMER_START_REVERSE;
					pca9482->timer_period = 0;
					mutex_unlock(&pca9482->lock);
					queue_delayed_work(pca9482->dc_wq,
								&pca9482->timer_work,
								msecs_to_jiffies(pca9482->timer_period));
				} else {
					/* Charging state does not support reverse mode */
					pr_info("%s: Not support reverse mode in charging state=%d\n",
							__func__, pca9482->charging_state);
					ret = -EINVAL;
				}
			}
			break;
		default:
			return -EINVAL;
		}
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

static int pca9482_chg_get_property(struct power_supply *psy,
				     enum power_supply_property prop,
				     union power_supply_propval *val)
{
	int ret = 0;
	struct pca9482_charger *pca9482 = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) prop;
#endif

	pr_debug("%s: =========START=========\n", __func__);
	pr_debug("%s: prop=%d\n", __func__, prop);

	switch ((int)prop) {
	case POWER_SUPPLY_PROP_PRESENT:
		/* TA present */
		val->intval = pca9482->mains_online;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		if (pca9482->ta_type == TA_TYPE_WIRELESS)
			val->intval = POWER_SUPPLY_TYPE_WIRELESS;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* DC online */
		if (pca9482->timer_id == TIMER_ID_NONE)
			val->intval = 0;
		else
			val->intval = 1;
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = pca9482->chg_status;
		pr_info("%s: CHG STATUS : %d\n", __func__, pca9482->chg_status);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (pca9482->charging_state >= DC_STATE_CHECK_ACTIVE &&
			pca9482->charging_state <= DC_STATE_CV_MODE)
			ret = pca9482_check_error(pca9482);

		val->intval = pca9482->health_status;
		pr_info("%s: HEALTH STATUS : %d, ret = %d\n",
			__func__, pca9482->health_status, ret);
		ret = 0;
		break;
#endif
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = get_const_charge_voltage(pca9482);
		if (ret < 0) {
			val->intval = 0;
		} else {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			val->intval = pca9482->float_voltage;
#else
			val->intval = ret;
#endif
			ret = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		/* Maximum vfloat */
		val->intval = pca9482->pdata->vfloat / DENOM_U_M;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = get_const_charge_current(pca9482);
		if (ret < 0) {
			val->intval = 0;
		} else {
			val->intval = ret;
			ret = 0;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = get_input_current_limit(pca9482);
		if (ret < 0) {
			val->intval = 0;
		} else {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			val->intval = ret / PCA9482_SEC_DENOM_U_M;
#else
			val->intval = ret;
#endif
			ret = 0;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		/* return NTC voltage  - uV unit */
		ret = pca9482_read_adc(pca9482, ADCCH_NTC);
		if (ret < 0) {
			val->intval = 0;
		} else {
			val->intval = ret;
			ret = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* return the output current - uA unit */
		/* check charging status */
		if (pca9482->charging_state == DC_STATE_NO_CHARGING) {
			/* return invalid */
			val->intval = 0;
			ret = 0;
		} else {
			int iin;
			/* get ibat current */
			iin = pca9482_read_adc(pca9482, ADCCH_BAT_CURRENT);
			if (ret < 0) {
				dev_err(pca9482->dev, "Invalid IBAT ADC\n");
				val->intval = 0;
			} else {
				val->intval = iin;
				ret = 0;
			}
		}
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			pca9482_monitor_work(pca9482);
			pca9482_test_read(pca9482);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			switch (val->intval) {
			case SEC_BATTERY_IIN_MA:
				pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
				val->intval = pca9482->adc_val[ADCCH_VIN_CURRENT];
				break;
			case SEC_BATTERY_IIN_UA:
				pca9482_read_adc(pca9482, ADCCH_VIN_CURRENT);
				val->intval = pca9482->adc_val[ADCCH_VIN_CURRENT] * PCA9482_SEC_DENOM_U_M;
				break;
			case SEC_BATTERY_VIN_MA:
				pca9482_read_adc(pca9482, ADCCH_VIN);
				val->intval = pca9482->adc_val[ADCCH_VIN];
				break;
			case SEC_BATTERY_VIN_UA:
				pca9482_read_adc(pca9482, ADCCH_VIN);
				val->intval = pca9482->adc_val[ADCCH_VIN] * PCA9482_SEC_DENOM_U_M;
				break;
			default:
				val->intval = 0;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			/* get_system_current function isn't supported. Cannot get accurate value of Isys */
			val->intval = 0;
			pr_info("%s: get_system_current function isn't supported. Cannot get accurate value of Isys\n", __func__);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			val->strval = charging_state_str[pca9482->charging_state];
			pr_info("%s: CHARGER_STATUS(%s)\n", __func__, val->strval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			ret = get_charging_enabled(pca9482);
			if (ret < 0)
				return ret;

			val->intval = ret;
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			break;
		case POWER_SUPPLY_EXT_PROP_DCHG_READ_BATP_BATN:
			pr_info("%s: POWER_SUPPLY_EXT_PROP_DCHG_READ_BATP_BATN\n", __func__);
			ret = pca9482_hw_init(pca9482);
			if (ret < 0)
				pr_info("%s: EXT_PROP_DCHG_READ_BATP_BATN, hw_init fail.\n", __func__);

			pca9482_chg_set_adc_force_mode(pca9482, 1);

			val->intval = pca9482_read_vbat_adc(pca9482);
			break;
		default:
			return -EINVAL;
		}
		break;
#endif
	default:
		ret = -EINVAL;
	}

	pr_debug("%s: End, prop=%d, val=%d, ret=%d\n", __func__, prop, val->intval, ret);
	return ret;
}

static enum power_supply_property pca9482_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct regmap_config pca9482_regmap = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= PCA9482_MAX_REGISTER,
};

static char *pca9482_supplied_to[] = {
	"pca9482-charger",
};

static const struct power_supply_desc pca9482_mains_desc = {
	.name		= "pca9482-charger",
	.type		= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= pca9482_chg_get_property,
	.set_property	= pca9482_chg_set_property,
	.properties	= pca9482_charger_props,
	.num_properties	= ARRAY_SIZE(pca9482_charger_props),
};

#if defined(CONFIG_OF)
static int pca9482_charger_parse_dt(struct device *dev,
				struct pca9482_platform_data *pdata)
{
	struct device_node *np_pca9482 = dev->of_node;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct device_node *np;
#endif
	int ret;
	if (!np_pca9482)
		return -EINVAL;

	/* irq gpio */
	pdata->irq_gpio = of_get_named_gpio(np_pca9482, "pca9482,irq-gpio", 0);
	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

	/* input current limit */
	ret = of_property_read_u32(np_pca9482, "pca9482,input-current-limit",
						   &pdata->iin_cfg);
	if (ret) {
		pr_info("%s: pca9482,input-current-limit is Empty\n", __func__);
		pdata->iin_cfg = PCA9482_IIN_REG_DFT;
	}
	pr_info("%s: pca9482,iin_cfg is %d\n", __func__, pdata->iin_cfg);

	/* charging current */
	ret = of_property_read_u32(np_pca9482, "pca9482,charging-current",
							   &pdata->ichg_cfg);
	if (ret) {
		pr_info("%s: pca9482,charging-current is Empty\n", __func__);
		pdata->ichg_cfg = PCA9482_IBAT_REG_DFT;
	}
	pr_info("%s: pca9482,ichg_cfg is %d\n", __func__, pdata->ichg_cfg);

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* charging float voltage */
	ret = of_property_read_u32(np_pca9482, "pca9482,float-voltage",
							   &pdata->vfloat);
	if (ret) {
		pr_info("%s: pca9482,float-voltage is Empty\n", __func__);
		pdata->vfloat = PCA9482_VBAT_REG_DFT;
	}
	pr_info("%s: pca9482,vfloat is %d\n", __func__, pdata->vfloat);
#endif

	/* input topoff current */
	ret = of_property_read_u32(np_pca9482, "pca9482,input-itopoff",
							   &pdata->iin_topoff);
	if (ret) {
		pr_info("%s: pca9482,input-itopoff is Empty\n", __func__);
		pdata->iin_topoff = IIN_DONE_DFT;
	}
	pr_info("%s: pca9482,iin_topoff is %d\n", __func__, pdata->iin_topoff);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* fpdo_dc input topoff current */
	ret = of_property_read_u32(np_pca9482, "pca9482,fpdo_dc_input-itopoff",
							   &pdata->fpdo_dc_iin_topoff);
	if (ret) {
		pr_info("%s: pca9482,fpdo_dc_input-itopoff is Empty\n", __func__);
		pdata->fpdo_dc_iin_topoff = 1700000; /* 1700mA */
	}
	pr_info("%s: pca9482,fpdo_dc_iin_topoff is %d\n", __func__, pdata->fpdo_dc_iin_topoff);

	/* fpdo_dc Vnow topoff condition */
	ret = of_property_read_u32(np_pca9482, "pca9482,fpdo_dc_vnow-topoff",
							&pdata->fpdo_dc_vnow_topoff);
	if (ret) {
		pr_info("%s: pca9482,fpdo_dc_vnow-topoff is Empty\n", __func__);
		pdata->fpdo_dc_vnow_topoff = 5000000; /* Vnow 5000000uV means disable */
	}
	pr_info("%s: pca9482,fpdo_dc_vnow_topoff is %d\n", __func__, pdata->fpdo_dc_vnow_topoff);

	ret = of_property_read_u32(np_pca9482, "pca9482,fpdo_dc_mainbat-topoff",
							&pdata->fpdo_dc_mainbat_topoff);
	if (ret) {
		pr_info("%s: pca9482,fpdo_dc_mainbat-topoff is Empty\n", __func__);
		pdata->fpdo_dc_mainbat_topoff = 5000000; /* mainbat 5000000uV means disable */
	}
	pr_info("%s: pca9482,fpdo_dc_mainbat_topoff is %d\n", __func__, pdata->fpdo_dc_mainbat_topoff);

	ret = of_property_read_u32(np_pca9482, "pca9482,fpdo_dc_subbat-topoff",
							&pdata->fpdo_dc_subbat_topoff);
	if (ret) {
		pr_info("%s: pca9482,fpdo_dc_subbat-topoff is Empty\n", __func__);
		pdata->fpdo_dc_subbat_topoff = 5000000; /* subbat 5000000uV means disable */
	}
	pr_info("%s: pca9482,fpdo_dc_subbat_topoff is %d\n", __func__, pdata->fpdo_dc_subbat_topoff);
#endif

	/* external sense resistor value */
	ret = of_property_read_u32(np_pca9482, "pca9482,sense-resistance",
							   &pdata->snsres);
	if (ret) {
		pr_info("%s: pca9482,sense-resistance is Empty\n", __func__);
		pdata->snsres = PCA9482_SENSE_R_DFT;
	}
	pr_info("%s: pca9482,snsres is %d\n", __func__, pdata->snsres);

	/* external sense resistor location */
	ret = of_property_read_u32(np_pca9482, "pca9482,sense-config",
							   &pdata->snsres_cfg);
	if (ret) {
		pr_info("%s: pca9482,sense-config is Empty\n", __func__);
		pdata->snsres_cfg = PCA9482_SENSE_R_CFG_DFT;
	}
	pr_info("%s: pca9482,snsres_cfg is %d\n", __func__, pdata->snsres_cfg);

	/* switching frequency */
	ret = of_property_read_u32(np_pca9482, "pca9482,switching-frequency",
							   &pdata->fsw_cfg);
	if (ret) {
		pr_info("%s: pca9482,switching frequency is Empty\n", __func__);
		pdata->fsw_cfg = PCA9482_FSW_CFG_DFT;
	}
	pr_info("%s: pca9482,fsw_cfg is %d\n", __func__, pdata->fsw_cfg);

	/* switching frequency - bypass */
	ret = of_property_read_u32(np_pca9482, "pca9482,switching-frequency-byp",
							   &pdata->fsw_cfg_byp);
	if (ret) {
		pr_info("%s: pca9482,switching frequency bypass is Empty\n", __func__);
		pdata->fsw_cfg_byp = PCA9482_FSW_CFG_BYP_DFT;
	}
	pr_info("%s: pca9482,fsw_cfg_byp is %d\n", __func__, pdata->fsw_cfg_byp);

	/* switching frequency for low input current */
	ret = of_property_read_u32(np_pca9482, "pca9482,switching-frequency-low",
							   &pdata->fsw_cfg_low);
	if (ret) {
		pr_info("%s: pca9482,switching frequency for low input current is Empty\n", __func__);
		pdata->fsw_cfg_low = PCA9482_FSW_CFG_LOW_DFT;
	}
	pr_info("%s: pca9482,fsw_cfg_low is %d\n", __func__, pdata->fsw_cfg_low);

	/* switching frequency for fixed pdo */
	ret = of_property_read_u32(np_pca9482, "pca9482,switching-frequency-fpdo",
							   &pdata->fsw_cfg_fpdo);
	if (ret) {
		pr_info("%s: pca9482,switching frequency for FPDO is Empty\n", __func__);
		pdata->fsw_cfg_fpdo = PCA9482_FSW_CFG_DFT;
	}
	pr_info("%s: pca9482,fsw_cfg_fpdo is %d\n", __func__, pdata->fsw_cfg_fpdo);
	/* NTC0 threshold voltage */
	ret = of_property_read_u32(np_pca9482, "pca9482,ntc0-threshold",
							   &pdata->ntc0_th);
	if (ret) {
		pr_info("%s: pca9482,ntc0 threshold voltage is Empty\n", __func__);
		pdata->ntc0_th = PCA9482_NTC_0_TH_DFT;
	}
	pr_info("%s: pca9482,ntc0_th is %d\n", __func__, pdata->ntc0_th);

	/* NTC1 threshold voltage */
	ret = of_property_read_u32(np_pca9482, "pca9482,ntc1-threshold",
							   &pdata->ntc1_th);
	if (ret) {
		pr_info("%s: pca9482,ntc1 threshold voltage is Empty\n", __func__);
		pdata->ntc1_th = PCA9482_NTC_1_TH_DFT;
	}
	pr_info("%s: pca9482,ntc1_th is %d\n", __func__, pdata->ntc1_th);

	/* NTC protection */
	ret = of_property_read_u32(np_pca9482, "pca9482,ntc-protection",
							   &pdata->ntc_en);
	if (ret) {
		pr_info("%s: pca9482,ntc protection is Empty\n", __func__);
		pdata->ntc_en = 0;	/* Disable */
	}
	pr_info("%s: pca9482,ntc_en is %d\n", __func__, pdata->ntc_en);

	/* Charging mode */
	ret = of_property_read_u32(np_pca9482, "pca9482,chg-mode",
							   &pdata->chg_mode);
	if (ret) {
		pr_info("%s: pca9482,charging mode is Empty\n", __func__);
		pdata->chg_mode = CHG_2TO1_DC_MODE;
	}
	pr_info("%s: pca9482,chg_mode is %d\n", __func__, pdata->chg_mode);

	/* cv mode polling time in step1 charging */
	ret = of_property_read_u32(np_pca9482, "pca9482,cv-polling",
							   &pdata->cv_polling);
	if (ret) {
		pr_info("%s: pca9482,cv-polling is Empty\n", __func__);
		pdata->cv_polling = CVMODE_CHECK_T;
	}
	pr_info("%s: pca9482,cv polling is %d\n", __func__, pdata->cv_polling);

	/* vfloat threshold in step1 charging */
	ret = of_property_read_u32(np_pca9482, "pca9482,step1-vth",
							   &pdata->step1_vth);
	if (ret) {
		pr_info("%s: pca9482,step1-vfloat-threshold is Empty\n", __func__);
		pdata->step1_vth = STEP1_VFLOAT_THRESHOLD;
	}
	pr_info("%s: pca9482,step1_vth is %d\n", __func__, pdata->step1_vth);

	/* VFLOAT method - FG VFLOAT or PCA9482 VFLOAT */
	ret = of_property_read_u32(np_pca9482, "pca9482,fg-vfloat",
							   &pdata->fg_vfloat);
	if (ret) {
		pr_info("%s: pca9482,fg-vfloat is Empty\n", __func__);
		pdata->fg_vfloat = 0;
	}
	pr_info("%s: pca9482,fg-vfloat is %d\n", __func__, pdata->fg_vfloat);

	/* Input current for low frequency threhold */
	ret = of_property_read_u32(np_pca9482, "pca9482,iin-low-frequency",
							   &pdata->iin_low_freq);
	if (ret) {
		pr_info("%s: pca9482,iin-low-frequency is Empty\n", __func__);
		pdata->iin_low_freq = IIN_LOW_TH_SW_FREQ;
	}
	pr_info("%s: pca9482,iin-low-frequency is %d\n", __func__, pdata->iin_low_freq);

	/* Default VFLOAT voltage */
	ret = of_property_read_u32(np_pca9482, "pca9482,default-vfloat",
								 &pdata->dft_vfloat);
	if (ret) {
		pr_info("%s: pca9482,default-vfloat is Empty\n", __func__);
		pdata->dft_vfloat = PCA9482_VBAT_REG_DFT;
	}
	pr_info("%s: pca9482,default-vfloat is %d\n", __func__, pdata->dft_vfloat);

	/* PCA9482 VBAT connection */
	ret = of_property_read_u32(np_pca9482, "pca9482,battery-connection",
								 &pdata->bat_conn);
	if (ret) {
		pr_info("%s: pca9482,battery-connection is Empty\n", __func__);
		pdata->bat_conn = VBAT_TO_VBPACK_OR_VBCELL;
	}
	pr_info("%s: pca9482,battery-connection is %d\n", __func__, pdata->bat_conn);

	/* TA voltage fixed offset in fianl target voltage calculation */
	ret = of_property_read_u32(np_pca9482, "pca9482,ta-volt-offset",
								 &pdata->ta_volt_offset);
	if (ret) {
		pr_info("%s: pca9482,ta-volt-offset is Empty\n", __func__);
		pdata->ta_volt_offset = PCA9482_TA_VOL_OFFSET_FINAL;
	}
	pr_info("%s: pca9482,ta-volt-offset is %d\n", __func__, pdata->ta_volt_offset);

	/* TA min vol preset for factory mode */
	ret = of_property_read_u32(np_pca9482, "pca9482,ta-min-vol",
								&pdata->ta_min_vol);
	if (ret) {
		pr_info("%s: pca9482,ta-min-vol is Empty\n", __func__);
		pdata->ta_min_vol = TA_MIN_VOL_PRESET;
	}
#ifndef CONFIG_SEC_FACTORY
	pdata->ta_min_vol = TA_MIN_VOL_PRESET;
#endif
	pr_info("%s: pca9482,ta-min-vol is %d\n", __func__, pdata->ta_min_vol);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("## %s: np(battery) NULL\n", __func__);
	} else {
		ret = of_property_read_string(np, "battery,charger_name",
				(char const **)&pdata->sec_dc_name);
		if (ret) {
			pr_err("## %s: direct_charger is Empty\n", __func__);
			pdata->sec_dc_name = "sec-direct-charger";
		}
		pr_info("%s: battery,charger_name is %s\n", __func__, pdata->sec_dc_name);

		/* Fuelgauge power supply name */
		ret = of_property_read_string(np, "battery,fuelgauge_name",
				(char const **)&pdata->fg_name);
		if (ret) {
			pr_info("## %s: fuelgauge_name name is Empty\n", __func__);
			pdata->fg_name = "battery";
		}
		pr_info("%s: fuelgauge name is %s\n", __func__, pdata->fg_name);

		/* charging float voltage */
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
								   &pdata->vfloat);
		pdata->vfloat *= PCA9482_SEC_DENOM_U_M;
		if (ret) {
			pr_info("%s: battery,dc_float_voltage is Empty\n", __func__);
			pdata->vfloat = PCA9482_VBAT_REG_DFT;
		}
		pr_info("%s: battery,v_float is %d\n", __func__, pdata->vfloat);

		/* the lowest limit to FPDO DC IIN */
		ret = of_property_read_u32(np, "battery,fpdo_dc_charge_power",
								   &pdata->fpdo_dc_iin_lowest_limit);
		pdata->fpdo_dc_iin_lowest_limit *= PCA9482_SEC_DENOM_U_M;
		pdata->fpdo_dc_iin_lowest_limit /= PCA9482_SEC_FPDO_DC_IV;
		if (ret) {
			pr_info("%s: battery,fpdo_dc_charge_power is Empty\n", __func__);
			pdata->fpdo_dc_iin_lowest_limit = 10000000; /* 10A */
		}
		pr_info("%s: fpdo_dc_iin_lowest_limit is %d\n", __func__, pdata->fpdo_dc_iin_lowest_limit);
	}
#endif

	return 0;
}
#else
static int of_pca9482_dt(struct device *dev, struct pca9482_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_USBPD_PHY_QCOM
static int pca9482_usbpd_setup(struct pca9482_charger *pca9482)
{
	int ret = 0;
	const char *pd_phandle = "usbpd-phy";

	pca9482->pd = devm_usbpd_get_by_phandle(pca9482->dev, pd_phandle);

	if (IS_ERR(pca9482->pd)) {
		pr_err("get_usbpd phandle failed (%ld)\n",
				PTR_ERR(pca9482->pd));
		return PTR_ERR(pca9482->pd);
	}

	return ret;
}
#endif

static int read_reg(void *data, u64 *val)
{
	struct pca9482_charger *chip = data;
	int rc;
	unsigned int temp;

	rc = regmap_read(chip->regmap, chip->debug_address, &temp);
	if (rc) {
		pr_err("Couldn't read reg %x rc = %d\n",
			chip->debug_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int write_reg(void *data, u64 val)
{
	struct pca9482_charger *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = regmap_write(chip->regmap, chip->debug_address, temp);
	if (rc) {
		pr_err("Couldn't write 0x%02x to 0x%02x rc= %d\n",
			temp, chip->debug_address, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, read_reg, write_reg, "0x%02llx\n");

static int pca9482_create_debugfs_entries(struct pca9482_charger *chip)
{
	struct dentry *ent;
	int rc = 0;

	chip->debug_root = debugfs_create_dir("charger-pca9482", NULL);
	if (!chip->debug_root) {
		dev_err(chip->dev, "Couldn't create debug dir\n");
		rc = -ENOENT;
	} else {
		/* After version 5.5.0 debugfs_create_x32 changed to a void type function */
		debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->debug_address));

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &register_debug_ops);
		if (!ent) {
			dev_err(chip->dev,
				"Couldn't create data debug file\n");
			rc = -ENOENT;
		}
	}

	return rc;
}

static int pca9482_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct power_supply_config mains_cfg = {};

	struct pca9482_platform_data *pdata;
	struct device *dev = &client->dev;
	struct pca9482_charger *pca9482_chg;
	int ret;

	dev_info(&client->dev, "%s: PCA9482 Charger Driver Loading\n", __func__);
	dev_info(&client->dev, "%s: =========START=========\n", __func__);

	pca9482_chg = devm_kzalloc(dev, sizeof(*pca9482_chg), GFP_KERNEL);
	if (!pca9482_chg)
		return -ENOMEM;

#if defined(CONFIG_OF)
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(struct pca9482_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = pca9482_charger_parse_dt(&client->dev, pdata);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to get device of_node\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
	} else {
		pdata = client->dev.platform_data;
	}
#else
	pdata = dev->platform_data;
#endif
	if (!pdata)
		return -EINVAL;

	i2c_set_clientdata(client, pca9482_chg);

	mutex_init(&pca9482_chg->lock);
	mutex_init(&pca9482_chg->i2c_lock);
	pca9482_chg->dev = &client->dev;
	pca9482_chg->pdata = pdata;
	pca9482_chg->charging_state = DC_STATE_NO_CHARGING;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_chg->wdt_kick = false;
#endif

	/* Create a work queue for the direct charger */
	pca9482_chg->dc_wq = alloc_ordered_workqueue("pca9482_dc_wq", WQ_MEM_RECLAIM);
	if (pca9482_chg->dc_wq == NULL) {
		dev_err(pca9482_chg->dev, "failed to create work queue\n");
		return -ENOMEM;
	}

	pca9482_chg->monitor_wake_lock = wakeup_source_register(&client->dev, "pca9482-charger-monitor");
	if (!pca9482_chg->monitor_wake_lock) {
		dev_err(dev, "%s: Cannot register wakeup source\n", __func__);
		return -ENOMEM;
	}

	/* initialize work */
	INIT_DELAYED_WORK(&pca9482_chg->timer_work, pca9482_timer_work);
	mutex_lock(&pca9482_chg->lock);
	pca9482_chg->timer_id = TIMER_ID_NONE;
	pca9482_chg->timer_period = 0;
	mutex_unlock(&pca9482_chg->lock);

	INIT_DELAYED_WORK(&pca9482_chg->pps_work, pca9482_pps_request_work);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	INIT_DELAYED_WORK(&pca9482_chg->wdt_control_work, pca9482_wdt_control_work);
#endif

	pca9482_chg->regmap = devm_regmap_init_i2c(client, &pca9482_regmap);
	if (IS_ERR(pca9482_chg->regmap)) {
		ret = -EINVAL;
		goto error;
	}

#ifdef CONFIG_USBPD_PHY_QCOM
	if (pca9482_usbpd_setup(pca9482_chg)) {
		dev_err(dev, "Error usbpd setup!\n");
		pca9482_chg->pd = NULL;
	}
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9482_init_adc_val(pca9482_chg, -1);
#endif
	ret = pca9482_hw_init(pca9482_chg);
	if (ret < 0)
		goto error;

	mains_cfg.supplied_to = pca9482_supplied_to;
	mains_cfg.num_supplicants = ARRAY_SIZE(pca9482_supplied_to);
	mains_cfg.drv_data = pca9482_chg;
	pca9482_chg->mains = power_supply_register(dev, &pca9482_mains_desc,
					   &mains_cfg);
	if (IS_ERR(pca9482_chg->mains)) {
		ret = -ENODEV;
		goto error;
	}

	/*
	 * Interrupt pin is optional. If it is connected, we setup the
	 * interrupt support here.
	 */
	if (pdata->irq_gpio >= 0) {
		ret = pca9482_irq_init(pca9482_chg, client);
		if (ret < 0) {
			dev_warn(dev, "failed to initialize IRQ: %d\n", ret);
			dev_warn(dev, "disabling IRQ support\n");
		}
		/* disable interrupt */
		disable_irq(client->irq);
	}

	ret = pca9482_create_debugfs_entries(pca9482_chg);
	if (ret < 0)
		goto error;

	sec_chg_set_dev_init(SC_DEV_DIR_CHG);
	dev_info(&client->dev, "%s: pca9482_driver version:%s\n", __func__, DRV_MODULE_VERSION);
	dev_info(&client->dev, "%s: =========END=========\n", __func__);

	return 0;

error:
	destroy_workqueue(pca9482_chg->dc_wq);
	mutex_destroy(&pca9482_chg->lock);
	wakeup_source_unregister(pca9482_chg->monitor_wake_lock);
	return ret;
}

static int pca9482_remove(struct i2c_client *client)
{
	struct pca9482_charger *pca9482_chg = i2c_get_clientdata(client);

	/* stop charging if it is active */
	pca9482_stop_charging(pca9482_chg);

	if (client->irq) {
		free_irq(client->irq, pca9482_chg);
		gpio_free(pca9482_chg->pdata->irq_gpio);
	}

	/* Delete the work queue */
	destroy_workqueue(pca9482_chg->dc_wq);

	wakeup_source_unregister(pca9482_chg->monitor_wake_lock);
	if (pca9482_chg->mains) {
		power_supply_put(pca9482_chg->mains);
		power_supply_unregister(pca9482_chg->mains);
	}

	debugfs_remove(pca9482_chg->debug_root);
	return 0;
}

static void pca9482_shutdown(struct i2c_client *client)
{
	struct pca9482_charger *pca9482_chg = i2c_get_clientdata(client);

	pr_info("%s: ++\n", __func__);

	if (pca9482_set_charging(pca9482_chg, false) < 0)
		pr_info("%s: failed to disable charging\n", __func__);

	if (client->irq)
		free_irq(client->irq, pca9482_chg);

	cancel_delayed_work(&pca9482_chg->timer_work);
	cancel_delayed_work(&pca9482_chg->pps_work);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	cancel_delayed_work(&pca9482_chg->wdt_control_work);
#endif

	pr_info("%s: --\n", __func__);
}

static const struct i2c_device_id pca9482_id[] = {
	{ "pca9482", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pca9482_id);

#if defined(CONFIG_OF)
static const struct of_device_id pca9482_i2c_dt_ids[] = {
	{ .compatible = "nxp,pca9482" },
	{ },
};
MODULE_DEVICE_TABLE(of, pca9482_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int pca9482_suspend(struct device *dev)
{
	struct pca9482_charger *pca9482 = dev_get_drvdata(dev);

	if (pca9482->timer_id != TIMER_ID_NONE) {
		pr_debug("%s: cancel delayed work\n", __func__);

		/* cancel delayed_work */
		cancel_delayed_work(&pca9482->timer_work);
	}
	return 0;
}

static int pca9482_resume(struct device *dev)
{
	struct pca9482_charger *pca9482 = dev_get_drvdata(dev);

	if (pca9482->timer_id != TIMER_ID_NONE) {
		pr_debug("%s: update_timer\n", __func__);

		/* Update the current timer */
		mutex_lock(&pca9482->lock);
		pca9482->timer_period = 0;	// ms unit
		mutex_unlock(&pca9482->lock);
		queue_delayed_work(pca9482->dc_wq,
							&pca9482->timer_work,
							msecs_to_jiffies(pca9482->timer_period));
	}
	return 0;
}
#else
#define pca9482_suspend		NULL
#define pca9482_resume		NULL
#endif

const struct dev_pm_ops pca9482_pm_ops = {
	.suspend = pca9482_suspend,
	.resume = pca9482_resume,
};

static struct i2c_driver pca9482_driver = {
	.driver = {
		.name = "pca9482-charger",
#if defined(CONFIG_OF)
		.of_match_table = pca9482_i2c_dt_ids,
#endif /* CONFIG_OF */
#if defined(CONFIG_PM)
		.pm = &pca9482_pm_ops,
#endif
	},
	.probe        = pca9482_probe,
	.remove       = pca9482_remove,
	.shutdown     = pca9482_shutdown,
	.id_table     = pca9482_id,
};

module_i2c_driver(pca9482_driver);

MODULE_DESCRIPTION("PCA9482 charger driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);
