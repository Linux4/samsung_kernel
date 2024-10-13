/*
 * Platform data for the NXP PCA9482 battery charger driver.
 *
 * Copyright (C) 2021-2023 NXP Semiconductor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PCA9482_CHARGER_H_
#define _PCA9482_CHARGER_H_

struct pca9482_platform_data {
	unsigned int	irq_gpio;	/* GPIO pin that's connected to INT# */
	unsigned int	iin_cfg;	/* Input Current Limit - uA unit */
	unsigned int 	ichg_cfg;	/* Charging Current - uA unit */
	unsigned int	vfloat;		/* Float Voltage - uV unit */
	unsigned int 	iin_topoff;	/* Input Topoff current - uV unit */
	unsigned int 	snsres;		/* External sense resistor value for IBAT measurement, 0 - 1mOhm, 1 - 2mOhm, 2 - 5mOhm */
	unsigned int	snsres_cfg;	/* External sense resistor loacation, 0 - bottom side, 1 - top side */
	unsigned int 	fsw_cfg; 	/* Switching frequency, 200kHz ~ 1.75MHz in 50kHz step - kHz unit */
	unsigned int	fsw_cfg_byp;/* Switching frequency for bypass mode, 200kHz ~ 1.75MHz in 50kHz step - kHz unit */
	unsigned int	fsw_cfg_low;/* Switching frequency for low current, 200kHz ~ 1.75MHz in 50kHz step - kHz unit */
	unsigned int	fsw_cfg_fpdo;	/* Switching frequency for fixed pdo, 200kHz ~ 1.75MHz in 50kHz step - kHz unit */
	unsigned int	ntc0_th;	/* NTC0 voltage threshold - cool or cold: 0 ~ 1.5V, 15mV step - uV unit */
	unsigned int 	ntc1_th;	/* NTC1 voltage threshold - warm or hot: 0 ~ 1.5V, 15mV step - uV unit */
	unsigned int	ntc_en;		/* Enable or Disable NTC protection, 0 - Disable, 1 - Enable */
	unsigned int	chg_mode;	/* Default charging mode, 0 - No direct charging, 1 - 2:1 charging mode, 2 - 4:1 charging mode */
	unsigned int	cv_polling; /* CV mode polling time in step1 charging - ms unit */
	unsigned int	step1_vth;	/* Step1 vfloat threshold - uV unit */
	char			*fg_name;	/* fuelgauge power supply name */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	char			*sec_dc_name;

	unsigned int fpdo_dc_iin_topoff;	/* FPDO DC Input Topoff current - uA unit */
	unsigned int fpdo_dc_vnow_topoff;	/* FPDO DC Vnow Topoff condition - uV unit */
	unsigned int fpdo_dc_mainbat_topoff;	/* FPDO DC mainbat Topoff condition - uV unit */
	unsigned int fpdo_dc_subbat_topoff;	/* FPDO DC subbat Topoff condition - uV unit */
	unsigned int fpdo_dc_iin_lowest_limit;	/* FPDO DC IIN lowest limit condition - uA unit */
#endif
	unsigned int	fg_vfloat;	/* Use fuelgauge vfloat method, 0 - Don't use FG Vnow and use PCA9482 VFLOAT, 1 - Use FG Vnow for VFLOAT */
	unsigned int	iin_low_freq;	/* Input current for low frequency threshold */
	unsigned int	dft_vfloat;	/* Default VFLOAT voltage - uV unit */
	unsigned int	bat_conn;	/* Battery connection, 0 - Battery Pack or Cell, 1 - FG VBATP/N */
	unsigned int	ta_volt_offset;	/* Final TA voltage fixed offset - uV unit */
	int	ta_min_vol;	/* TA min voltage preset for factory mode - uV unit */
};

#endif
