/*
 * sm5461_charger.h - SM5461 Charger device driver for SAMSUNG platform
 *
 * Copyright (C) 2023 SiliconMitus Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "sm5461_direct_charger.h"

#ifndef __SM5461_CHARGER_H__
#define __SM5461_CHARGER_H__

#define SM5461_TA_MIN_CURRENT   1000
#define SM5461_CV_OFFSET        0
#define SM5461_CI_OFFSET        300
#define SM5461_SIOP_LEV1        1100
#define SM5461_SIOP_LEV2        1700

/* TA Type*/
enum {
	SM5461_TA_UNKNOWN,
	SM5461_TA_USBPD,
	SM5461_TA_WIRELESS,
	SM5461_TA_USBPD_2P0,
};

enum SM5461_flag1_desc {
	SM5461_FLAG1_VBUSPD         = 1 << 5,
	SM5461_FLAG1_VDSQRB         = 1 << 4,
	SM5461_FLAG1_VBUSOVP        = 1 << 3,
	SM5461_FLAG1_IBUSOCP        = 1 << 2,
	SM5461_FLAG1_CHGON          = 1 << 1,
	SM5461_FLAG1_IBUSUCP        = 1 << 0,
};

enum SM5461_flag2_desc {
	SM5461_FLAG2_VBATOVP        = 1 << 7,
	SM5461_FLAG2_IBATOCP        = 1 << 6,
	SM5461_FLAG2_VBATREG        = 1 << 5,
	SM5461_FLAG2_IBATREG        = 1 << 4,
	SM5461_FLAG2_TSD            = 1 << 3,
	SM5461_FLAG2_RLTVUVP        = 1 << 2,
	SM5461_FLAG2_RLTVOVP        = 1 << 1,
	SM5461_FLAG2_CNSHTP         = 1 << 0,
};

enum SM5461_flag3_desc {
	SM5461_FLAG3_VBUSPOK        = 1 << 7,
	SM5461_FLAG3_VOUTPOK        = 1 << 6,
	SM5461_FLAG3_WTDTMR         = 1 << 5,
	SM5461_FLAG3_VBATALM        = 1 << 4,
	SM5461_FLAG3_VBUSUVLO       = 1 << 3,
	SM5461_FLAG3_CHGONTMR       = 1 << 2,
	SM5461_FLAG3_ADCDONE        = 1 << 1,
	SM5461_FLAG3_CFLYSHTP       = 1 << 0,
};

enum SM5461_flag4_desc {
	SM5461_FLAG4_IBUSOCP_RVS    = 1 << 5,
	SM5461_FLAG4_RVSRDY         = 1 << 4,
	SM5461_FLAG4_THEMP          = 1 << 3,
	SM5461_FLAG4_IBUSREG        = 1 << 2,
	SM5461_FLAG4_TOPOFF         = 1 << 1,
	SM5461_FLAG4_DONE           = 1 << 0,
};

enum SM5461_reg_addr {
	SM5461_REG_CNTL1            = 0x00,
	SM5461_REG_CNTL2            = 0x01,
	SM5461_REG_CNTL3            = 0x02,
	SM5461_REG_DEVICE_ID        = 0x03,
	SM5461_REG_CNTL4            = 0x05,
	SM5461_REG_VBUSOVP          = 0x06,
	SM5461_REG_IBUS_PROT        = 0x07,
	SM5461_REG_VBAT_OVP         = 0x08,
	SM5461_REG_IBAT_OCP         = 0x09,
	SM5461_REG_REG1             = 0x0A,
	SM5461_REG_FLAG1            = 0x0B,
	SM5461_REG_FLAGMSK1         = 0x0C,
	SM5461_REG_FLAG2            = 0x0D,
	SM5461_REG_FLAGMSK2         = 0x0E,
	SM5461_REG_FLAG3            = 0x0F,
	SM5461_REG_FLAGMSK3         = 0x10,
	SM5461_REG_ADC_CNTL         = 0x11,
	SM5461_REG_VBUS_ADC_H       = 0x12,
	SM5461_REG_VBUS_ADC_L       = 0x13,
	SM5461_REG_IBUS_ADC_H       = 0x14,
	SM5461_REG_IBUS_ADC_L       = 0x15,
	SM5461_REG_VBAT_ADC_H       = 0x16,
	SM5461_REG_VBAT_ADC_L       = 0x17,
	SM5461_REG_IBAT_ADC_H       = 0x18,
	SM5461_REG_IBAT_ADC_L       = 0x19,
	SM5461_REG_TDIE_ADC         = 0x1A,
	SM5461_REG_THEM             = 0x60,
	SM5461_REG_CNTL5            = 0x61,
	SM5461_REG_TOPOFF           = 0x62,
	SM5461_REG_FLAG4            = 0x63,
	SM5461_REG_FLAGMSK4         = 0x64,
	SM5461_REG_THEM_ADC_H       = 0x65,
	SM5461_REG_THEM_ADC_L       = 0x66,
	SM5461_REG_VBATALM          = 0x67,
	SM5461_REG_DIS_VL_V2P9      = 0x91,
	SM5461_REG_IBATOCP_DG       = 0x92,
	SM5461_REG_VODIV_REF_OP     = 0x94,
	SM5461_REG_QRB_DG_N_SNSR    = 0x95,
	SM5461_REG_ADC_CLK_SEL      = 0x96,
	SM5461_REG_M_UVPOVP_SEL     = 0x98,
	SM5461_REG_PRECHG_MODE      = 0xA0,
	SM5461_REG_CTRL_STM_0       = 0xBA,
	SM5461_REG_CTRL_STM_1       = 0xBB,
	SM5461_REG_CTRL_STM_2       = 0xBC,
	SM5461_REG_CTRL_STM_3       = 0xBD,
	SM5461_REG_CTRL_STM_5       = 0xBF,
};

enum SM5461_vbatovp_offset {
	SM5461_VBATOVP_50   = 0x0,
	SM5461_VBATOVP_100  = 0x1,
	SM5461_VBATOVP_150  = 0x2,
	SM5461_VBATOVP_200  = 0x3,
};

enum SM5461_ibatocp_offset {
	SM5461_IBATOCP_200  = 0x0,
	SM5461_IBATOCP_300  = 0x1,
	SM5461_IBATOCP_400  = 0x2,
	SM5461_IBATOCP_500  = 0x3,
};

enum SM5461_ibusocp_offset {
	SM5461_IBUSOCP_100  = 0x0,
	SM5461_IBUSOCP_200  = 0x1,
	SM5461_IBUSOCP_300  = 0x2,
	SM5461_IBUSOCP_400  = 0x3,
};

enum SM5461_adc_channel {
	SM5461_ADC_THEM     = 0x0,
	SM5461_ADC_TDIE,
	SM5461_ADC_VBUS,
	SM5461_ADC_IBUS,
	SM5461_ADC_VBAT,
	SM5461_ADC_IBAT,
};

enum SM5461_wdt_tmr {
	WDT_TIMER_S_0P5     = 0x0,
	WDT_TIMER_S_1       = 0x1,
	WDT_TIMER_S_2       = 0x2,
	WDT_TIMER_S_5       = 0x3,
	WDT_TIMER_S_10      = 0x4,
	WDT_TIMER_S_20      = 0x5,
	WDT_TIMER_S_40      = 0x6,
	WDT_TIMER_S_80      = 0x7,
};

enum SM5461_op_mode {
	OP_MODE_INIT        = 0x0,
	OP_MODE_FW_BYPASS   = 0x1,
	OP_MODE_FW_BOOST    = 0x2,
	OP_MODE_REV_BYPASS  = 0x3,
	OP_MODE_REV_BOOST   = 0x4,
};

enum sm5461_chip_id {
	SM5461_ALONE = 0x0,
	SM5461_MAIN = 0x1,
	SM5461_SUB = 0x2,
};

enum SM5461_freq {
	SM5461_FREQ_200KHZ  = 0x0,
	SM5461_FREQ_375KHZ  = 0x1,
	SM5461_FREQ_500KHZ  = 0x2,
	SM5461_FREQ_750KHZ  = 0x3,
	SM5461_FREQ_1000KHZ = 0x4,
	SM5461_FREQ_1250KHZ = 0x5,
	SM5461_FREQ_1500KHZ = 0x6,
};

struct sm5461_platform_data {
	u8 rev_id;
	int irq_gpio;
	u32 r_ttl;
	u32 pps_lr;
	u32 rpcm;
	u32 freq;
	u32 freq_byp;
	u32 freq_fpdo;
	u32 freq_siop[2];
	u32 topoff;
	u32 x2bat_mode;
	u32 en_vbatreg;
	u32 snsr;
	u32 single_mode;
	u32 fpdo_topoff;
	u32 fpdo_mainvbat_reg;
	u32 fpdo_subvbat_reg;
	u32 fpdo_vnow_reg;

	struct {
		u32 chg_float_voltage;
		u32 fpdo_chg_curr;
		char *sec_dc_name;
		char *fuelgauge_name;
	} battery;
};

struct sm5461_charger {
	struct device *dev;
	struct i2c_client *i2c;
	struct sm5461_platform_data *pdata;
	struct power_supply	*psy_chg;
	struct sm_dc_info *pps_dc;
	struct sm_dc_info *x2bat_dc;
	int chip_id;

	struct mutex i2c_lock;
	struct mutex pd_lock;
	struct wakeup_source *chg_ws;

	int irq;
	int cable_online;
	bool vbus_in;
	bool rev_boost;
	u32 max_vbat;
	u32 target_vbat;
	u32 target_ibus;
	u32 target_ibat;
	u32 ta_type;

	bool wdt_disable;

	/* debug */
	struct dentry *debug_root;
	u32 debug_address;
	int addr;
	int size;
};

#endif  /* __SM5461_CHARGER_H__ */
