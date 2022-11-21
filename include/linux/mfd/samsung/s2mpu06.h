/*
 * s2mpu06.h - Driver for the s2mpu06
 *
 *  Copyright (C) 2015 Samsung Electrnoics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __S2MPU06_MFD_H__
#define __S2MPU06_MFD_H__
#include <linux/platform_device.h>
#include <linux/regmap.h>
#if defined(CONFIG_SEC_CHARGER_S2MU005)
#include <linux/power/sec_charging_common.h>
#else
#if defined(CONFIG_BATTERY_SAMSUNG_V2)
#include "../../../../drivers/battery_v2/include/sec_charging_common.h"
#else
#include <linux/battery/sec_charging_common.h>
#endif
#endif

#define MFD_DEV_NAME "s2mpu06"
#define M2SH(m) ((m) & 0x0F ? ((m) & 0x03 ? ((m) & 0x01 ? 0 : 1) : \
		((m) & 0x04 ? 2 : 3)) : \
		((m) & 0x30 ? ((m) & 0x10 ? 4 : 5) : ((m) & 0x40 ? 6 : 7)))


#if defined(CONFIG_SEC_CHARGER_S2MPU06) || defined(CONFIG_CHARGER_S2MPU06)
typedef struct s2mpu06_charger_platform_data {
	struct sec_charging_current *charging_current;
	int chg_float_voltage;
	char *charger_name;
	char *fuelgauge_name;
	bool chg_eoc_dualpath;
	uint32_t is_1MHz_switching:1;
	/* 2nd full check */
	sec_battery_full_charged_t full_check_type_2nd;
	int battery_type;
	/* USA concept - vf and gnd short detection */
	bool vf_gnd_short_detection;
	int vf_gnd_short_det_gpio;
	int vf_gnd_short_det_irq;
	/* IVR Threshold */
	int ivr_threshold;
	/* Slow charging current */
	int slow_charging_current;
} s2mpu06_charger_platform_data_t;
#endif


#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06) || defined(CONFIG_FUELGAUGE_S2MPU06)
typedef struct s2mpu06_fuelgauge_platform_data {
	int capacity_max;
	int capacity_max_margin;
	int capacity_min;
	int capacity_full;
	int capacity_calculation_type;
	int fuel_alert_soc;
	int fullsocthr;
	int fg_irq;

	char *fuelgauge_name;

	bool repeated_fuelalert;

	struct sec_charging_current *charging_current;
} s2mpu06_fuelgauge_platform_data_t;
#endif
/**
 * sec_regulator_data - regulator data
 * @id: regulator id
 * @initdata: regulator init data (contraints, supplies, ...)
 */
struct s2mpu06_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */
struct sec_opmode_data {
	int id;
	unsigned int mode;
};

/*
 * samsung regulator operation mode
 * SEC_OPMODE_OFF	Regulator always OFF
 * SEC_OPMODE_ON	Regulator always ON
 * SEC_OPMODE_LOWPOWER  Regulator is on in low-power mode
 * SEC_OPMODE_SUSPEND   Regulator is changed by PWREN pin
 *			If PWREN is high, regulator is on
 *			If PWREN is low, regulator is off
 */
enum sec_opmode {
	SEC_OPMODE_OFF,
	SEC_OPMODE_SUSPEND,
	SEC_OPMODE_LOWPOWER,
	SEC_OPMODE_ON,
	SEC_OPMODE_TCXO = 0x2,
};

/**
 * struct sec_wtsr_smpl - settings for WTSR/SMPL
 * @wtsr_en:		WTSR Function Enable Control
 * @smpl_en:		SMPL Function Enable Control
 * @wtsr_timer_val:	Set the WTSR timer Threshold
 * @smpl_timer_val:	Set the SMPL timer Threshold
 * @check_jigon:	if this value is true, do not enable SMPL function when
 *			JIGONB is low(JIG cable is attached)
 */
struct sec_wtsr_smpl {
	bool wtsr_en;
	bool smpl_en;
	int wtsr_timer_val;
	int smpl_timer_val;
	bool check_jigon;
};

struct s2mpu06_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;

#if defined(CONFIG_SEC_CHARGER_S2MPU06) || defined(CONFIG_CHARGER_S2MPU06)
	s2mpu06_charger_platform_data_t *pdata;
#endif
#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06) || defined(CONFIG_FUELGAUGE_S2MPU06)
	s2mpu06_fuelgauge_platform_data_t *fuelgauge_pdata;
#endif

	int num_regulators;
	struct	s2mpu06_regulator_data *regulators;
	struct	sec_opmode_data		*opmode;
	struct	mfd_cell *sub_devices;
	int 	num_subdevs;

	int	(*cfg_pmic_irq)(void);
	int	device_type;
	int	ono;
	int	buck_ramp_delay;
	bool	dvs_en;

	/* ---- RTC ---- */
	struct sec_wtsr_smpl *wtsr_smpl;
	struct rtc_time *init_time;

	bool	use_i2c_speedy;
	bool	cache_data;
};

struct s2mpu06 {
	struct regmap *regmap;
};

#endif /* __S2MPU06_MFD_H__ */

