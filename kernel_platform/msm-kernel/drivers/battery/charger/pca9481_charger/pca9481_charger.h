/*
 * Platform data for the NXP PCA9481 battery charger driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PCA9481_CHARGER_H_
#define _PCA9481_CHARGER_H_

/* IIN offset as the switching frequency in uA*/
static int iin_fsw_cfg[16] = { 9990, 10540, 11010, 11520, 12000, 12520, 12990, 13470,
								5460, 6050, 6580, 7150, 7670, 8230, 8720, 9260 };

struct pca9481_platform_data {
	int irq_gpio;	/* GPIO pin that's connected to INT# */
	unsigned int iin_cfg;	/* Input Current Limit - uA unit */
	unsigned int ichg_cfg;	/* Charging Current - uA unit */
	unsigned int vfloat;	/* Float Voltage - uV unit */
	unsigned int iin_topoff;	/* Input Topoff current - uV unit */
	unsigned int snsres;		/* External sense resistor value for IBAT measurement, 0 - 1mOhm, 1 - 2mOhm, 2 - 5mOhm */
	unsigned int snsres_cfg;	/* External sense resistor loacation, 0 - bottom side, 1 - top side */
	unsigned int fsw_cfg; 	/* Switching frequency, 200kHz ~ 1.75MHz in 50kHz step - kHz unit */
	unsigned int fsw_cfg_byp;	/* Switching frequency for bypass mode, 200kHz ~ 1.75MHz in 50kHz step - kHz unit */
	unsigned int ntc0_th;	/* NTC0 voltage threshold - cool or cold: 0 ~ 1.5V, 15mV step - uV unit */
	unsigned int ntc1_th;	/* NTC1 voltage threshold - warm or hot: 0 ~ 1.5V, 15mV step - uV unit */
	unsigned int ntc_en;	/* Enable or Disable NTC protection, 0 - Disable, 1 - Enable */
	unsigned int chg_mode;	/* Default charging mode, 0 - No direct charging, 1 - 2:1 charging mode, 2 - 4:1 charging mode */
	unsigned int cv_polling; /* CV mode polling time in step1 charging - ms unit */
	unsigned int step1_vth;	/* Step1 vfloat threshold - uV unit */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	char *sec_dc_name;
#endif
};

#define BITS(_end, _start) ((BIT(_end) - BIT(_start)) + BIT(_end))
#define MASK2SHIFT(_mask)	__ffs(_mask)
#define MIN(a, b)	((a < b) ? (a):(b))
#define MAX(a, b)	((a > b) ? (a):(b))

/************************/
/* PCA9481 Register Map */
/************************/
#define PCA9481_REG_DEVICE_ID 					0x00	// Device ID Register
#define PCA9481_BIT_DEV_REV						BITS(7, 4)
#define PCA9481_BIT_DEV_ID						BITS(3, 0)
#define PCA9481_DEVICE_ID						0x10	// Default ID

#define PCA9481_REG_INT_DEVICE_0				0x01	// Interrupt Register 0 for device operation
#define PCA9481_BIT_VOUT_MAX_OV					BIT(7)
#define PCA9481_BIT_RCP_DETECTED				BIT(6)
#define PCA9481_BIT_VIN_UNPLUG					BIT(5)
#define PCA9481_BIT_VIN_OVP						BIT(4)
#define PCA9481_BIT_VIN_OV_TRACKING				BIT(3)
#define PCA9481_BIT_VIN_UV_TRACKING				BIT(2)
#define PCA9481_BIT_VIN_NOT_VALID				BIT(1)
#define PCA9481_BIT_VIN_VALID					BIT(0)

#define PCA9481_REG_INT_DEVICE_1				0x02	// Interrupt Register 1 for device operation
#define PCA9481_BIT_NTC_1_DETECTED				BIT(6)
#define PCA9481_BIT_NTC_0_DETECTED				BIT(5)
#define PCA9481_BIT_ADC_READ_DONE				BIT(4)
#define PCA9481_BIT_VIN_CURRENT_LIMITED			BIT(3)
#define PCA9481_BIT_VIN_OCP_21_11				BIT(2)
#define PCA9481_BIT_SINK_RCP_TIMEOUT			BIT(1)
#define PCA9481_BIT_SINK_RCP_ENABLED			BIT(0)

#define PCA9481_REG_INT_DEVICE_2				0x03	// Interrupt Register 2 for device operation
#define PCA9481_BIT_VIN_OCP_12_11				BIT(6)
#define PCA9481_BIT_VFET_IN_OK					BIT(5)
#define PCA9481_BIT_THEM_REG_EXIT				BIT(4)
#define PCA9481_BIT_THEM_REG					BIT(3)
#define PCA9481_BIT_THSD_EXIT					BIT(2)
#define PCA9481_BIT_THSD						BIT(1)
#define PCA9481_BIT_WATCHDOG_TIMER_OUT			BIT(0)

#define PCA9481_REG_INT_DEVICE_3				0x04	// Interrupt Register 3 for device operation
#define PCA9481_BIT_STATUS_CHANGE_INT			BIT(6)
#define PCA9481_BIT_VFET_IN_OVP_EXIT			BIT(5)
#define PCA9481_BIT_VFET_IN_OVP					BIT(4)
#define PCA9481_BIT_VIN_OCP_ALARM_12_11_EXIT	BIT(3)
#define PCA9481_BIT_VIN_OCP_ALARM_12_11			BIT(2)
#define PCA9481_BIT_VIN_OCP_ALARM_21_11_EXIT	BIT(1)
#define PCA9481_BIT_VIN_OCP_ALARM_21_11			BIT(0)

#define PCA9481_REG_INT_CHARGING				0x05	// Interrupt Register for Charging operation
#define PCA9481_BIT_CHG_SAFETY_TIMER			BIT(5)
#define PCA9481_BIT_VBAT_OVP_EXIT				BIT(4)
#define PCA9481_BIT_VBAT_OVP					BIT(3)
#define PCA9481_BIT_VBAT_REG_LOOP				BIT(2)
#define PCA9481_BIT_I_VBAT_CC_LOOP				BIT(1)
#define PCA9481_BIT_I_VIN_CC_LOOP				BIT(0)

#define PCA9481_REG_INT_SC_0					0x06	// Interrupt Register 0 for Switched Capacitor(SC) operation
#define PCA9481_BIT_PHASE_B_FAULT				BIT(6)
#define PCA9481_BIT_PHASE_A_FAULT				BIT(5)
#define PCA9481_BIT_PIN_SHORT					BIT(4)
#define PCA9481_BIT_STANDBY_EXIT				BIT(3)
#define PCA9481_BIT_STANDBY_ENTER				BIT(2)
#define PCA9481_BIT_SWITCHING_ENABLED			BIT(1)
#define PCA9481_BIT_SC_OFF						BIT(0)

#define PCA9481_REG_INT_SC_1					0x07	// Interrupt Register 1 for Switched Capacitor(SC) operation
#define PCA9481_BIT_OVPOUT_ERRLO				BIT(2)
#define PCA9481_BIT_11_ENABLED					BIT(1)
#define PCA9481_BIT_REVERSE_SW_SS_OC			BIT(0)

#define PCA9481_REG_INT_DEVICE_0_MASK			0x08	// Interrupt Mask Register for INT_DEVICE_0 register
#define PCA9481_REG_INT_DEVICE_1_MASK			0x09	// Interrupt Mask Register for INT_DEVICE_1 register
#define PCA9481_REG_INT_DEVICE_2_MASK			0x0A	// Interrupt Mask Register for INT_DEVICE_2 register
#define PCA9481_REG_INT_DEVICE_3_MASK			0x0B	// Interrupt Mask Register for INT_DEVICE_3 register
#define PCA9481_REG_INT_CHARGING_MASK			0x0C	// Interrupt Mask Register for INT_CHARGING register
#define PCA9481_REG_INT_SC_0_MASK				0x0D	// Interrupt Mask Register for INT_SC_0 register
#define PCA9481_REG_INT_SC_1_MASK				0x0E	// Interrupt Mask Register for INT_SC_1 register

#define PCA9481_REG_DEVICE_0_STS				0x0F	// Status Register for INT_DEVICE_0 register
#define PCA9481_REG_DEVICE_1_STS				0x10	// Status Register for INT_DEVICE_1 register
#define PCA9481_REG_DEVICE_2_STS				0x11	// Status Register for INT_DEVICE_2 register
#define PCA9481_REG_DEVICE_3_STS				0x12	// Status Register for INT_DEVICE_3 register
#define PCA9481_BIT_STATUS_CHANGE				BITS(7, 6)
#define PCA9481_REG_CHARGING_STS				0x13	// Status Register for INT_CHARGING register
#define PCA9481_REG_SC_0_STS					0x14	// Status Register for INT_SC_0 registger
#define PCA9481_REG_SC_1_STS					0x15	// Status Register for INT_SC_1 registger

#define PCA9481_REG_DEVICE_CNTL_0				0x16	// Device control register 0
#define PCA9481_BIT_STANDBY_BY_NTC_EN			BIT(7)
#define PCA9481_BIT_THERMAL_SHUTDOWN_CFG		BITS(6, 5)
#define PCA9481_BIT_WATCHDOG_TIMER_DOUBLE_EN	BIT(4)
#define PCA9481_BIT_WATCHDOG_CFG				BITS(3, 2)
#define PCA9481_BIT_WATCHDOG_EN					BIT(1)
#define PCA9481_BIT_SOFT_RESET					BIT(0)

#define PCA9481_REG_DEVICE_CNTL_1				0x17	// Device control register 1
#define PCA9481_BIT_HALF_VIN_OVP_EN				BIT(7)
#define PCA9481_BIT_LOW_POWER_MODE_DISABLE		BIT(6)
#define PCA9481_BIT_VIN_OVP_CFG					BITS(5, 4)
#define PCA9481_BIT_VIN_FIXED_OVP_EN			BIT(3)
#define PCA9481_BIT_THERMAL_REGULATION_CFG		BITS(2, 1)
#define PCA9481_BIT_THERMAL_REGULATION_EN		BIT(0)

#define PCA9481_REG_DEVICE_CNTL_2				0x18	// Device control register 2
#define PCA9481_BIT_IBAT_SENSE_R_CFG			BIT(7)
#define PCA9481_BIT_EN_CFG						BIT(6)
#define PCA9481_BIT_VFET_IN_OVP_CFG				BITS(5, 3)
#define PCA9481_BIT_EXT_OVP_FUNCTION_EN			BIT(2)
#define PCA9481_BIT_VIN_VALID_DEGLITCH			BITS(1, 0)

#define PCA9481_REG_DEVICE_CNTL_3				0x19	// Device control register 3
#define PCA9481_BIT_SYNC_FUNCTION_EN			BIT(7)
#define PCA9481_BIT_SYNC_SECONDARY_EN			BIT(6)
#define PCA9481_BIT_FORCE_SHUTDOWN				BIT(5)
#define PCA9481_BIT_VIN_OCP_ALRAM_CURRENT_12_11	BITS(4, 1)
#define PCA9481_BIT_VIN_ALARM_OCP_12_11_EN		BIT(0)

#define PCA9481_REG_AUTO_RESTART_CNTL_0				0x1A	// Device auto restart register 0
#define PCA9481_BIT_AUTO_RESTART_NTC_EN				BIT(7)
#define PCA9481_BIT_AUTO_RESTART_FIXED_OV_EN		BIT(6)
#define PCA9481_BIT_AUTO_RESTART_OV_TRACKING_EN		BIT(5)
#define PCA9481_BIT_AUTO_RESTART_UV_TRACKING_EN		BIT(4)
#define PCA9481_BIT_AUTO_RESTART_THEM_EN			BIT(3)
#define PCA9481_BIT_AUTO_RESTART_VBAT_OVP_EN		BIT(2)
#define PCA9481_BIT_AUTO_RESTART_VIN_OCP_21_11_EN	BIT(1)
#define PCA9481_BIT_AUTO_RESTART_RCP_EN				BIT(0)

#define PCA9481_REG_AUTO_RESTART_CNTL_1			0x1B	// Device auto restart register 1

#define PCA9481_REG_RCP_CNTL					0x1C	// RCP control register
#define PCA9481_BIT_I_RCP_CURRENT_DEGLITCH		BITS(7, 6)
#define PCA9481_BIT_I_SINK_RCP_TIMER			BIT(5)
#define PCA9481_BIT_I_RCP_THRESHOLD				BITS(4, 2)
#define PCA9481_BIT_I_SINK_RCP					BIT(1)
#define PCA9481_BIT_RCP_EN						BIT(0)

#define PCA9481_REG_CHARGING_CNTL_0				0x1D	// Charging control register 0
#define PCA9481_BIT_VIN_CURRENT_SLOPE			BIT(7)
#define PCA9481_BIT_OCP_DEGLITCH_TIME_21_11		BIT(6)
#define PCA9481_BIT_VIN_CURRENT_OCP_21_11		BIT(5)
#define PCA9481_BIT_VIN_OCP_21_11_EN			BIT(4)
#define PCA9481_BIT_CSP_CSN_MEASURE_EN			BIT(3)
#define PCA9481_BIT_VBAT_LOOP_EN				BIT(2)
#define PCA9481_BIT_I_VBAT_LOOP_EN				BIT(1)
#define PCA9481_BIT_I_VIN_LOOP_EN				BIT(0)

#define PCA9481_REG_CHARGING_CNTL_1				0x1E	// Charging control register 1
#define PCA9481_BIT_VIN_REGULATION_CURRENT		BITS(7, 0)

#define PCA9481_REG_CHARGING_CNTL_2				0x1F	// Charging control register 2
#define PCA9481_BIT_VBAT_REGULATION				BITS(7, 0)

#define PCA9481_REG_CHARGING_CNTL_3				0x20	// Charging control register 3
#define PCA9481_BIT_I_VBAT_REGULATION			BITS(7, 0)

#define PCA9481_REG_CHARGING_CNTL_4				0x21	// Charging control register 4
#define PCA9481_BIT_IBAT_SENSE_R_SEL			BITS(7, 6)
#define PCA9481_BIT_VIN_ALARM_OCP_21_11_EN		BIT(5)
#define PCA9481_BIT_CHARGER_SAFETY_TIMER		BITS(4, 3)
#define PCA9481_BIT_CHARGER_SAFETY_TIMER_EN		BIT(2)
#define PCA9481_BIT_VBAT_OVP_DEGLITCH_TIME		BITS(1, 0)

#define PCA9481_REG_CHARGING_CNTL_5				0x22	// Charging control register 5
#define PCA9481_BIT_VBAT_OVP_EN					BIT(6)
#define PCA9481_BIT_VIN_OCP_CURRENT_12_11		BITS(5, 2)
#define PCA9481_BIT_OCP_DEGLITCH_TIME_12_11		BIT(1)
#define PCA9481_BIT_VIN_OCP_12_11_EN			BIT(0)

#define PCA9481_REG_CHARGING_CNTL_6				0x23	// Charging control register 6
#define PCA9481_BIT_VIN_OCP_ALARM_CURRENT		BITS(7, 0)

#define PCA9481_REG_NTC_0_CNTL					0x24	// NTC 0 control register
#define PCA9481_BIT_NTC_0_TRIGGER_VOLTAGE		BITS(7, 1)
#define PCA9481_BIT_NTC_EN						BIT(0)

#define PCA9481_REG_NTC_1_CNTL					0x25	// NTC 1 control register
#define PCA9481_BIT_NTC_1_TRIGGER_VOLTAGE		BITS(6, 0)

#define PCA9481_REG_SC_CNTL_0					0x26	// Switched Capacitor converter control register 0
#define PCA9481_BIT_FSW_CFG						BITS(4, 0)

#define PCA9481_REG_SC_CNTL_1					0x27	// Switched Capacitor converter control register 1
#define PCA9481_BIT_VIN_UV_TRACKING_DEGLITCH	BITS(5, 4)
#define PCA9481_BIT_UV_TRACKING_HYSTERESIS		BIT(3)
#define PCA9481_BIT_UV_TRACK_DELTA				BITS(2, 1)
#define PCA9481_BIT_UV_TRACKING_EN				BIT(0)

#define PCA9481_REG_SC_CNTL_2					0x28	// Switched Capacitor converter control register 2
#define PCA9481_BIT_VOUT_MAX_OV_EN				BIT(4)
#define PCA9481_BIT_OV_TRACK_DELTA				BITS(3, 2)
#define PCA9481_BIT_OV_TRACKING_HYSTERESIS		BIT(1)
#define PCA9481_BIT_OV_TRACKING_EN				BIT(0)

#define PCA9481_REG_SC_CNTL_3					0x29	// Switched Capacitor converter control register 3
#define PCA9481_BIT_STANDBY_EN					BIT(7)
#define PCA9481_BIT_SC_OPERATION_MODE			BITS(6, 5)
#define PCA9481_BIT_PRECHARGE_CFLY_TIME_OUT		BITS(4, 3)
#define PCA9481_BIT_PRECHARGE_CFLY_I			BITS(2, 0)

#define PCA9481_REG_ADC_CNTL					0x2A	// ADC control register
#define PCA9481_BIT_ADC_IN_SHUTDOWN_STATE		BIT(7)
#define PCA9481_BIT_ADC_MODE_CFG				BITS(6, 5)
#define PCA9481_BIT_ADC_HIBERNATE_READ_INTERVAL	BITS(4, 3)
#define PCA9481_BIT_ADC_AVERAGE_TIMES			BITS(2, 1)
#define PCA9481_BIT_ADC_EN						BIT(0)

#define PCA9481_REG_ADC_EN_CNTL_0				0x2B	// ADC enable control register 0
#define PCA9481_BIT_ADC_READ_I_BAT_CURRENT_EN	BIT(7)
#define PCA9481_BIT_ADC_READ_VIN_CURRENT_EN		BIT(6)
#define PCA9481_BIT_ADC_READ_DIE_TEMP_EN		BIT(5)
#define PCA9481_BIT_ADC_READ_NTC_EN				BIT(4)
#define PCA9481_BIT_ADC_READ_VOUT_EN			BIT(3)
#define PCA9481_BIT_ADC_READ_BATP_BATN_EN		BIT(2)
#define PCA9481_BIT_ADC_READ_OVP_OUT_EN			BIT(1)
#define PCA9481_BIT_ADC_READ_VIN_EN				BIT(0)

#define PCA9481_REG_ADC_EN_CNTL_1				0x2C	// ADC enable control register 1
#define PCA9481_BIT_ADC_READ_VFET_IN_EN			BIT(0)

#define PCA9481_REG_ADC_READ_VIN_0				0x2D	// ADC VIN read register 0
#define PCA9481_REG_ADC_READ_VIN_1				0x2E	// ADC VIN read register  1
#define PCA9481_REG_ADC_READ_VFET_IN_0			0x2F	// ADC VFET_IN read register 0
#define PCA9481_REG_ADC_READ_VFET_IN_1			0x30	// ADC VFET_IN read register 1
#define PCA9481_REG_ADC_READ_OVP_OUT_0			0x31	// ADC OVP_OUT read registe 0
#define PCA9481_REG_ADC_READ_OVP_OUT_1			0x32	// ADC OVP_OUT read registe 1
#define PCA9481_REG_ADC_READ_BATP_BATN_0		0x33	// ADC BATP_BATN read registe 0
#define PCA9481_REG_ADC_READ_BATP_BATN_1		0x34	// ADC BATP_BATN read registe 1
#define PCA9481_REG_ADC_READ_VOUT_0				0x35	// ADC VOUT read registe 0
#define PCA9481_REG_ADC_READ_VOUT_1				0x36	// ADC VOUT read registe 1
#define PCA9481_REG_ADC_READ_NTC_0				0x37	// ADC NTC read registe 0
#define PCA9481_REG_ADC_READ_NTC_1				0x38	// ADC NTC read registe 1
#define PCA9481_REG_ADC_READ_DIE_TEMP_0			0x39	// ADC Die temperature read registe 0
#define PCA9481_REG_ADC_READ_DIE_TEMP_1			0x3A	// ADC Die temperature read registe 1
#define PCA9481_REG_ADC_READ_VIN_CURRENT_0		0x3B	// ADC VIN current read registe 0
#define PCA9481_REG_ADC_READ_VIN_CURRENT_1		0x3C	// ADC VIN current read registe 1
#define PCA9481_REG_ADC_READ_I_VBAT_CURRENT_0	0x3D	// ADC I_VBAT charge current read registe 0
#define PCA9481_REG_ADC_READ_I_VBAT_CURRENT_1	0x3E	// ADC I_VBAT charge current read registe 1
#define PCA9481_BIT_ADC_READ_7_0				BITS(7, 0)	// ADC bit[7:0]
#define PCA9481_BIT_ADC_READ_11_8				BITS(3, 0)	// ADC bit[11:8]

#define PCA9481_MAX_REGISTER			0x4F

#define PCA9481_REG_BIT_AI				BIT(7)	// Auto Increment bit


/* Regulation voltage and current */
#define PCA9481_IIN_REG_STEP			25000	// VIN_REGULATION_CURRENT, input current step, unit - uA
#define PCA9481_IBAT_REG_STEP			50000	// I_VBAT_REGUATION, charge current step, unit - uA
#define PCA9481_VBAT_REG_STEP			5000	// VBAT_REGULATION, battery regulation voltage step, unit - uV

#define PCA9481_IIN_REG_MAX				6000000		// 6A
#define PCA9481_IBAT_REG_MAX			10000000	// 10A
#define PCA9481_VBAT_REG_MAX			5000000		// 5V

#define PCA9481_IIN_REG_MIN				500000		// 500mA
#define PCA9481_IBAT_REG_MIN			1000000		// 1000mA
#define PCA9481_VBAT_REG_MIN			3725000		// 3725mV

#define PCA9481_IIN_REG(_input_current)	((_input_current - PCA9481_IIN_REG_MIN)/PCA9481_IIN_REG_STEP)	// input current, unit - uA
#define PCA9481_IBAT_REG(_chg_current)	((_chg_current - PCA9481_IBAT_REG_MIN)/PCA9481_IBAT_REG_STEP)	// charge current, unit - uA
#define PCA9481_VBAT_REG(_bat_voltage)	((_bat_voltage - PCA9481_VBAT_REG_MIN)/PCA9481_VBAT_REG_STEP)	// battery voltage, unit - uV

/* NTC trigger voltage */
#define PCA9481_NTC_TRIGGER_VOLTAGE_STEP	15000	// 15mV, unit - uV
#define PCA9481_NTC_TRIGGER_VOLTAGE(_ntc_vol)	(_ntc_vol/PCA9481_NTC_TRIGGER_VOLTAGE_STEP)	// ntc voltage, unit - uV

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define PCA9481_SEC_DENOM_U_M		1000 // 1000, denominator
#define PCA9481_BATT_WDT_CONTROL_T		30000	// 30s
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

/* Switching frequency */
#define PCA9481_FSW_MIN			200
#define PCA9481_FSW_MAX			1750
#define PCA9481_FSW_CFG_STEP	50	// 50kHz
#define PCA9481_FSW_CFG(_frequency)	((_frequency - 200)/PCA9481_FSW_CFG_STEP)	// switching frequency, unit - kHz

/* Enable pin polarity selection */
#define PCA9481_EN_ACTIVE_H		0x00
#define PCA9481_EN_ACTIVE_L		PCA9481_BIT_EN_CFG
#define PCA9481_STANDBY_FORCE	PCA9481_BIT_STANDBY_EN
#define PCA9481_STANDBY_DONOT	0

/* Operation SC mode */
#define PCA9481_SC_OP_21	0x0		// 00b: 2:1 Switching Mode
#define PCA9481_SC_OP_12	0x20	// 01b: 1:2 Switching Mode
#define PCA9481_SC_OP_F_11	0x40	// 10b: Forward 1:1 mode
#define PCA9481_SC_OP_R_11	0x60	// 11b: Reverse 1:1 mode

/* Device current status */
#define PCA9481_SHUTDOWN_STATE	0x00	// 00b: in shutdown state
#define PCA9481_STANDBY_STATE	0x40	// 01b: in standby state
#define PCA9481_21SW_F11_MODE	0x80	// 10b: 2:1 switching or forward 1:1 mode
#define PCA9481_12SW_R11_MODE	0xC0	// 11b: 1:2 switching or reverse 1:1 mode

/* ADC Channel */
enum {
	ADCCH_VIN,
	ADCCH_VFET_IN,
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
#define VFET_IN_STEP		5250		// 5.25mV(5250uV) LSB, Range(0V ~ 20V)
#define VFET_IN_MAX			20000000	// 20.0V(20000mV, 20000000uV)
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
#if defined(CONFIG_SEC_FACTORY)
#define VBATMIN_CHECK_T	0
#else
#define VBATMIN_CHECK_T	1000	// Vbat min check timer - 1000ms
#endif
#define CCMODE_CHECK_T	5000	// CC mode polling timer - 5000ms
#define CVMODE_CHECK_T	10000	// CV mode polling timer - 10000ms
#define BYPMODE_CHECK_T	10000	// Bypass mode polling timer - 10000ms
#define PDMSG_WAIT_T	200		// PD message waiting time - 200ms
#define ENABLE_DELAY_T	150		// DC Enable waiting time - 150ms
#define PPS_PERIODIC_T	10000	// PPS periodic request message timer - 10000ms
#define UVLO_CHECK_T	1000	// UVLO check timer - 1000ms
#define BYPASS_WAIT_T	200		// Bypass mode waiting time - 200ms
#define INIT_WAKEUP_T	10000	// Initial wakeup timeout - 10000ms

/* Battery minimum voltage Threshold for direct charging */
#define DC_VBAT_MIN				3100000	// 3100000uV
/* Battery minimum voltage threshold for direct charging error */
#define DC_VBAT_MIN_ERR		3100000	// 3100000uV
/* Input Current Limit default value - Input Current Regulation */
#define PCA9481_IIN_REG_DFT		3000000	// 3000000uA
/* Input Current Limit offset value - Input Current Regulation */
#define PCA9481_IIN_REG_OFFSET1	300000	// 300mA
#define PCA9481_IIN_REG_OFFSET2	350000	// 350mA
#define PCA9481_IIN_REG_OFFSET3	400000	// 400mA
#define PCA9481_IIN_REG_OFFSET1_TH	2000000	// 2000mA
#define PCA9481_IIN_REG_OFFSET2_TH	3000000	// 3000mA
/* Charging Current default value - Charge Current Regulation */
#define PCA9481_IBAT_REG_DFT	6000000	// 6000000uA
#define PCA9481_IBAT_REG_OFFSET	100000	// 100mA - Software offset for DC algorithm
/* Charging Float Voltage default value - Battery Voltage Regulation */
#define PCA9481_VBAT_REG_DFT	4350000	// 4350000uV

/* IBAT Sense Resistor default value */
#define PCA9481_SENSE_R_DFT		IBAT_SENSE_R_1mOhm	// 1mOhm
/* IBAT Sense Resistor location default value */
#define PCA9481_SENSE_R_CFG_DFT	IBAT_SENSE_R_BOTTOM_SIDE	// Bottom side - connect to BATN
/* Switching Frequency default value */
#define PCA9481_FSW_CFG_DFT		1000	// 1.0MHz(1000kHz)
/* Switching Frequency default value for bypass */
#define PCA9481_FSW_CFG_BYP_DFT	500		// 500kHz
/* NTC_0 threshold voltage default value */
#define PCA9481_NTC_0_TH_DFT	1110000	// 1.11V(1110000uV)
/* NTC_1 threshold voltage default value */
#define PCA9481_NTC_1_TH_DFT	495000	// 0.495V(495000uV)

/* IIN ADC compensation gain */
#define PCA9481_IIN_ADC_COMP_GAIN	1867	// uA unit
/* IIN ADC compensation offset */
#define PCA9481_IIN_ADC_COMP_OFFSET	-44178	// uA unit

/* Charging Done Condition */
#define ICHG_DONE_DFT	1000000	// 1000mA
#define IIN_DONE_DFT	500000	// 500mA

/* Maximum TA voltage threshold */
#define TA_MAX_VOL		10200000 // 10200000uV
/* Minimum TA voltage threshold */
#define TA_MIN_VOL		7000000	// 7000000uV
/* Maximum TA current threshold */
#define TA_MAX_CUR		4500000	// 4500000uA
/* Minimum TA current threshold */
#define TA_MIN_CUR		1000000	// 1000000uA - PPS minimum current

/* Minimum TA voltage threshold in Preset mode */
#if defined(CONFIG_SEC_FACTORY)
#define TA_MIN_VOL_PRESET	8300000	// 8300000uV
#else
#define TA_MIN_VOL_PRESET	7700000	// 7700000uV
#endif
/* TA voltage offset for the initial TA voltage */
#define TA_VOL_PRE_OFFSET	500000	// 500000uV
/* Adjust CC mode TA voltage step */
#if defined(CONFIG_SEC_FACTORY)
#define TA_VOL_STEP_ADJ_CC	80000	// 80000uV
#else
#define TA_VOL_STEP_ADJ_CC	40000	// 40000uV
#endif
/* Pre CC mode TA voltage step */
#define TA_VOL_STEP_PRE_CC	100000	// 100000uV
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
/* TA current low offset for reducing input current */
#define TA_CUR_LOW_OFFSET	200000	// 200mA
/* TA voltage offset for 1:1 bypass mode */
#define TA_VOL_OFFSET_1TO1_BYPASS	100000	// 100mV
/* TA voltalge offset for 2:1 bypass mode */
#define TA_VOL_OFFSET_2TO1_BYPASS	200000	// 200mV

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

/* ADC operation mode */
enum {
	AUTO_MODE = 0,
	FORCE_SHUTDOWN_MODE,
	FORCE_HIBERNATE_MODE,
	FORCE_NORMAL_MODE,
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


/**
 * struct pca9481_charger - pca9481 charger instance
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
 * @tcpm_psy: power supply instance for TYPEC PD IC
 * @mains_online: is AC/DC input connected
 * @charging_state: direct charging state
 * @ret_state: return direct charging state after DC_STATE_ADJUST_TAVOL is done
 * @iin_cc: input current for the direct charging in cc mode, uA
 * @iin_cfg: input current limit, uA
 * @vfloat: floating voltage, uV
 * @ichg_cfg: charging current limit, uA
 * @dc_mode: direct charging mode, normal, 1:1 bypass, or 2:1 bypass mode
 * @iin_topoff: input topoff current, uA
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
 * @pdata: pointer to platform data
 * @debug_root: debug entry
 * @debug_address: debug register address
 */
struct pca9481_charger {
	struct wakeup_source	*monitor_wake_lock;
	struct mutex		lock;
	struct mutex		i2c_lock;
	struct device		*dev;
	struct regmap		*regmap;
	struct power_supply	*mains;
	struct workqueue_struct *dc_wq;
	struct delayed_work timer_work;
	unsigned int		timer_id;
	unsigned long		timer_period;
	unsigned long		last_update_time;

	struct delayed_work	pps_work;
#ifdef CONFIG_USBPD_PHY_QCOM
	struct usbpd		*pd;
#endif

	bool				mains_online;
	unsigned int		charging_state;
	unsigned int		ret_state;

	unsigned int		iin_cc;
	unsigned int		iin_cfg;
	unsigned int		vfloat;
	unsigned int		ichg_cfg;
	unsigned int		iin_topoff;
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

	struct pca9481_platform_data *pdata;

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

#endif
