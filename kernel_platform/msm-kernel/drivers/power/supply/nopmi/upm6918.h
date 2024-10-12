#include "qcom-pmic-voter.h"

#ifndef _LINUX_UPM6918_I2C_H
#define _LINUX_UPM6918_I2C_H

#include <linux/power_supply.h>

#define PROFILE_CHG_VOTER		"PROFILE_CHG_VOTER"
#define MAIN_SET_VOTER			"MAIN_SET_VOTER"
#define BMS_FC_VOTER			"BMS_FC_VOTER"
#define DCP_CHG_VOTER			"DCP_CHG_VOTER"
#define PD_CHG_VOTER			"PD_CHG_VOTER"
#define STORE_MODE_VOTER		"STORE_MODE_VOTER"
#define AFC_CHG_DARK_VOTER		"AFC_CHG_DARK_VOTER"


#define UPM6918_MAX_ICL 		3000
#define UPM6918_MAX_FCC 		4900

#define CHG_FCC_CURR_MAX		3600
#define CHG_ICL_CURR_MAX		2020
#define DCP_ICL_CURR_MAX		2000
#define AFC_ICL_CURR_MAX        1700
#define AFC_COMMON_ICL_CURR_MAX 1900
#define PD_ICL_CURR_MAX		    1100

#define CHG_CDP_CURR_MAX        1500
#define CHG_SDP_CURR_MAX        500
#define CHG_AFC_CURR_MAX        3000
#define CHG_AFC_COMMON_CURR_MAX 2500
#define CHG_DCP_CURR_MAX        3000

#define AFC_CHARGE_ITERM        480

#define AFC_QUICK_CHARGE_POWER_CODE          0x46
#define AFC_COMMIN_CHARGE_POWER_CODE         0x08


struct upm6918_charge_param {
	int vlim;
	int ilim;
	int ichg;
	int vreg;
};

enum stat_ctrl {
	STAT_CTRL_STAT,
	STAT_CTRL_ICHG,
	STAT_CTRL_INDPM,
	STAT_CTRL_DISABLE,
};

enum vboost {
	BOOSTV_4850 = 4850,
	BOOSTV_5000 = 5000,
	BOOSTV_5150 = 5150,
	BOOSTV_5300 = 5300,
};

enum iboost {
	BOOSTI_500 = 500,
	BOOSTI_1200 = 1200,
};

enum vac_ovp {
	VAC_OVP_5500 = 5500,
	VAC_OVP_6500 = 6500,
	VAC_OVP_10500 = 10500,
	VAC_OVP_14000 = 14000,
};

enum charg_stat {
	CHRG_STAT_NOT_CHARGING,
	CHRG_STAT_PRE_CHARGINT,
	CHRG_STAT_FAST_CHARGING,
	CHRG_STAT_CHARGING_TERM,
};

struct upm6918_platform_data {
	struct upm6918_charge_param usb;
	int iprechg;
	int iterm;
	int switch_iterm;
	enum stat_ctrl statctrl;
	enum vboost boostv;	// options are 4850,
	enum iboost boosti; // options are 500mA, 1200mA
	enum vac_ovp vac_ovp;
	
};

enum upm6918_iio_type {
	DS28E16,
	BMS,
	NOPMI,
};

extern int afc_set_voltage_workq(unsigned int afc_code);
#endif
