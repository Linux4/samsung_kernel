
/*
 * sm5440_charger.h - SM5440 Charger device driver for SAMSUNG platform
 *
 * Copyright (C) 2019 SiliconMitus Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "sm5440_direct_charger.h"

#ifndef __SM5440_CHARGER_H__
#define __SM5440_CHARGER_H__

#define SM5440_TA_MIN_CURRENT   1000
#define SM5440_CV_OFFSET        50
#define SM5440_CI_OFFSET        300
#define SM5440_VBAT_MIN         3300
#define SM5440_SIOP_LEV1        1100
#define SM5440_SIOP_LEV2        1700

enum sm5440_int1_desc {
	SM5440_INT1_VOUTOVP         = 1 << 4,
	SM5440_INT1_VBATOVP         = 1 << 3,
	SM5440_INT1_REVBPOCP        = 1 << 1,
	SM5440_INT1_REVBSTOCP       = 1 << 0,
};

enum sm5440_int2_desc {
	SM5440_INT2_IBUSLIM         = 1 << 7,
	SM5440_INT2_VOUTOVP_ALM     = 1 << 4,
	SM5440_INT2_VBATREG         = 1 << 3,
	SM5440_INT2_THEM_REG        = 1 << 1,
	SM5440_INT2_THEM            = 1 << 0,
};

enum sm5440_int3_desc {
	SM5440_INT3_VBUSOVP         = 1 << 7,
	SM5440_INT3_VBUSUVLO        = 1 << 6,
	SM5440_INT3_VBUSPOK         = 1 << 5,
	SM5440_INT3_THEMSHDN_ALM    = 1 << 4,
	SM5440_INT3_THEMSHDN        = 1 << 3,
	SM5440_INT3_STUP_FAIL       = 1 << 2,
	SM5440_INT3_REVBLK          = 1 << 1,
	SM5440_INT3_CFLY_SHORT      = 1 << 0,
};

enum sm5440_int4_desc {
	SM5440_INT4_RVSBSTRDY       = 1 << 6,
	SM5440_INT4_MIDVBUS2VOUT    = 1 << 5,
	SM5440_INT4_SW_RDY          = 1 << 4,
	SM5440_INT4_WDTMROFF        = 1 << 2,
	SM5440_INT4_CHGTMROFF       = 1 << 1,
	SM5440_INT4_ADCUPDATED      = 1 << 0,
};


enum sm5440_reg_addr {
	SM5440_REG_INT1         = 0x00,
	SM5440_REG_INT2         = 0x01,
	SM5440_REG_INT3         = 0x02,
	SM5440_REG_INT4         = 0X03,
	SM5440_REG_MSK1         = 0X04,
	SM5440_REG_MSK2         = 0X05,
	SM5440_REG_MSK3         = 0X06,
	SM5440_REG_MSK4         = 0X07,
	SM5440_REG_STATUS1      = 0X08,
	SM5440_REG_STATUS2      = 0X09,
	SM5440_REG_STATUS3      = 0X0A,
	SM5440_REG_STATUS4      = 0X0B,
	SM5440_REG_CNTL1        = 0X0C,
	SM5440_REG_CNTL2        = 0X0D,
	SM5440_REG_CNTL3        = 0X0E,
	SM5440_REG_CNTL4        = 0X0F,
	SM5440_REG_CNTL5        = 0X10,
	SM5440_REG_CNTL6        = 0X11,
	SM5440_REG_CNTL7        = 0X12,
	SM5440_REG_VBUSCNTL     = 0X13,
	SM5440_REG_VBATCNTL     = 0X14,
	SM5440_REG_VOUTCNTL     = 0X15,
	SM5440_REG_IBUSCNTL     = 0X16,
	SM5440_REG_PRTNCNTL     = 0X19,
	SM5440_REG_THEMCNTL1    = 0X1A,
	SM5440_REG_THEMCNTL2    = 0X1B,
	SM5440_REG_ADCCNTL1     = 0X1C,
	SM5440_REG_ADCCNTL2     = 0X1D,
	SM5440_REG_ADC_VBUS1    = 0X1E,
	SM5440_REG_ADC_VBUS2    = 0X1F,
	SM5440_REG_ADC_VOUT1    = 0X20,
	SM5440_REG_ADC_VOUT2    = 0X21,
	SM5440_REG_ADC_IBUS1    = 0X22,
	SM5440_REG_ADC_IBUS2    = 0X23,
	SM5440_REG_ADC_THEM1    = 0X24,
	SM5440_REG_ADC_THEM2    = 0X25,
	SM5440_REG_ADC_DIETEMP  = 0X26,
	SM5440_REG_ADC_VBAT1    = 0X27,
	SM5440_REG_ADC_VBAT2    = 0X28,
	SM5440_REG_DEVICEID     = 0X2B,
};

enum sm5440_adc_channel {
	SM5440_ADC_THEM         = 0x0,
	SM5440_ADC_DIETEMP,
	SM5440_ADC_VBAT1,
	SM5440_ADC_VOUT,
	SM5440_ADC_IBUS,
	SM5440_ADC_VBUS,
};

enum sm5440_wdt_tmr {
	WDT_TIMER_S_0P5     = 0x0,
	WDT_TIMER_S_1       = 0x1,
	WDT_TIMER_S_2       = 0x2,
	WDT_TIMER_S_4       = 0x3,
	WDT_TIMER_S_30      = 0x4,
	WDT_TIMER_S_60      = 0x5,
	WDT_TIMER_S_90      = 0x6,
	WDT_TIMER_S_120     = 0x7,
};

enum sm5440_ibus_ocp {
	IBUS_OCP_100        = 0x0,
	IBUS_OCP_200        = 0x1,
	IBUS_OCP_300        = 0x2,
	IBUS_OCP_400        = 0x3,
};

enum sm5440_op_mode {
	OP_MODE_CHG_OFF     = 0x0,
	OP_MODE_CHG_ON      = 0x1,
	OP_MODE_REV_BYPASS  = 0x2,
	OP_MODE_REV_BOOST   = 0x3,
};

struct sm5440_platform_data {
	u8 rev_id;
	int irq_gpio;
	u32 r_ttl;
	u32 freq;
	u32 freq_byp;
	u32 freq_siop[2];
	u32 topoff;
	u32 en_vbatreg;

	struct {
		u32 chg_float_voltage;
		u32 max_charging_charge_power;
		char *sec_dc_name;
		char *fuelgauge_name;
	} battery;
};

struct sm5440_charger {
	struct device *dev;
	struct i2c_client *i2c;
	struct sm5440_platform_data *pdata;
	struct power_supply	*psy_chg;
	struct sm_dc_info *pps_dc;
	struct sm_dc_info *wpc_dc;
	int ps_type;

	struct mutex i2c_lock;
	struct mutex pd_lock;
	struct wakeup_source *chg_ws;
	struct delayed_work adc_work;
	struct delayed_work ocp_check_work;

	int irq;
	int cable_online;
	bool vbus_in;
	bool rev_boost;
	u32 max_vbat;
	u32 target_vbat;
	u32 target_ibus;
	u32 target_ibat;

	bool wdt_disable;

	/* debug */
	struct dentry *debug_root;
	u32 debug_address;
	int addr;
	int size;
};

#endif  /* __SM5440_CHARGER_H__ */
