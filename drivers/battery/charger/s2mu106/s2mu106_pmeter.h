/*
 * s2mu106_pmeter.h - Header of S2MU106 Powermeter Driver
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef S2MU106_PMETER_H
#define S2MU106_PMETER_H
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/mfd/samsung/s2mu106.h>
#if defined(CONFIG_PM_S2M_WATER)
#include "linux/usb/typec/slsi/s2mu106/usbpd-s2mu106.h"
#include <linux/time.h>
#include <linux/alarmtimer.h>
#endif
#include "../../common/include/sec_charging_common.h"

#define S2MU106_PM_VALUP1	0x03
#define S2MU106_PM_VALUP2	0x04
#define S2MU106_PM_INT1		0x05
#define S2MU106_PM_INT2		0x06
#define S2MU106_PM_VALUP1_MASK	0x0B
#define S2MU106_PM_VALUP2_MASK	0x0C
#define S2MU106_PM_INT1_MASK	0x0D
#define S2MU106_PM_INT2_MASK	0x0E

#define S2MU106_PM_CO_MASK1	0x59
#define S2MU106_PM_CO_MASK2	0x5A
#define S2MU106_PM_CAL_DIS1	0x5B
#define S2MU106_PM_CAL_DIS2	0x5C
#define S2MU106_PM_REQ_BOX_RR1	0x5D
#define S2MU106_PM_REQ_BOX_RR2	0x5E
#define S2MU106_PM_REQ_BOX_CO1	0x5F
#define S2MU106_PM_REQ_BOX_CO2	0x60
#define S2MU106_PM_CTRL1	0x61
#define S2MU106_PM_CTRL2	0x62
#define S2MU106_PM_CTRL3	0x63
#define S2MU106_PM_CTRL4	0x64

#define S2MU106_PM_VAL1_VCHGIN	0x12
#define S2MU106_PM_VAL2_VCHGIN	0x13
#define S2MU106_PM_VAL1_VWCIN	0x14
#define S2MU106_PM_VAL2_VWCIN	0x15
#define S2MU106_PM_VAL1_VBYP	0x16
#define S2MU106_PM_VAL2_VBYP	0x17
#define S2MU106_PM_VAL1_VSYS	0x18
#define S2MU106_PM_VAL2_VSYS	0x19
#define S2MU106_PM_VAL1_VBAT	0x1A
#define S2MU106_PM_VAL2_VBAT	0x1B
#define S2MU106_PM_VAL1_VCC1	0x1C
#define S2MU106_PM_VAL2_VCC1	0x1D
#define S2MU106_PM_VAL1_VGPADC	0x1E
#define S2MU106_PM_VAL2_VGPADC	0x1F
#define S2MU106_PM_VAL1_ICHGIN	0x20
#define S2MU106_PM_VAL2_ICHGIN	0x21
#define S2MU106_PM_VAL1_IWCIN	0x22
#define S2MU106_PM_VAL2_IWCIN	0x23
#define S2MU106_PM_VAL1_VCC2	0x24
#define S2MU106_PM_VAL2_VCC2	0x25
#define S2MU106_PM_VAL1_IOTG	0x26
#define S2MU106_PM_VAL2_IOTG	0x27
#define S2MU106_PM_VAL1_ITX	0x28
#define S2MU106_PM_VAL2_ITX	0x29
#define S2MU106_PM_HYST_LEVEL1	0xA3
#define S2MU106_PM_HYST_LEVEL4	0xA6
#define S2MU106_PM_HYST_CONTI1	0xA9

/* Register Description */
/* 0x0D PM_INT_MASK1 */
#define S2MU106_PM_INT_MASK1_VGPADC_SHIFT	1
#define S2MU106_PM_INT_MASK1_VGPADC		(0x1 << S2MU106_PM_INT_MASK1_VGPADC_SHIFT)

/* 0x5F REQ_BOX_CO1  */
#define S2MU106_PM_REQ_BOX_CO1_VGPADCC_SHIFT	1
#define S2MU106_PM_REQ_BOX_CO1_VGPADCC		(0x1 << S2MU106_PM_INT_MASK1_VGPADC_SHIFT)

/* 0x64 PM_CTRL4 */
#define S2MU106_PM_CTRL4_PM_IOUT_SEL_SHIFT	5
#define S2MU106_PM_CTRL4_PM_IOUT_SEL		(0x3 << S2MU106_PM_CTRL4_PM_IOUT_SEL_SHIFT)
#define S2MU106_PM_CTRL4_PM_IOUT_SEL_RMODE	(0x1 << S2MU106_PM_CTRL4_PM_IOUT_SEL_SHIFT)
#define S2MU106_PM_CTRL4_PM_IOUT_SEL_VMODE	(0x3 << S2MU106_PM_CTRL4_PM_IOUT_SEL_SHIFT)

#define IS_INVALID_CODE(x)	(x > 2450)

#define IS_VCHGIN_IN(x)		(x >= 2000)
#define IS_VBUS_LOW(x)		(x < 4000)


/* GPADC Table */
/* Vadc table */
#define PM_VWATER		(100)
#define IS_VGPADC_OPEN(x) 	(x >= 0 && x < PM_VWATER)
#define IS_VGPADC_WATER(x) 	(x >= PM_VWATER)
#define IS_VGPADC_VBUS_SHORT(x)	(x >= 500)
#define IS_VGPADC_GND(x) 	(x < 100)

/* Radc table */
#define PM_RWATER		(1500)
#define IS_RGPADC_WATER(x) 	((x >= 1) && (x <= PM_RWATER))
#define IS_RGPADC_OPEN(x) 	(x > 5000)
#define IS_RGPADC_GND(x) 	(x <= 50)
/* pull-down
 * uct100 : 0 ohm
 * uct300 : 200 kohm
 */
#define IS_RGPADC_FACWATER(x) 	(x <= 200)

/* Water Detection Sensitivity */
#define PM_WATER_DET_CNT_LOOP		(5)
#define PM_WATER_DET_CNT_SEN_LOW	(2)
#define PM_WATER_DET_CNT_SEN_MIDDLE	(3)
#define PM_WATER_DET_CNT_SEN_HIGH	(5)

#define PM_WATER2_WORK_PHASE0_ITV	(60000) /* 1min */
#define PM_WATER2_WORK_PHASE1_ITV	(300000)/* 5min */
#define PM_WATER2_WORK_PHASE2_ITV	(600000)/* 10min */
#define PM_WATER2_WORK_RESUME_ITV	(3000)	/* 3sec */

#define PM_WATER1_ALARM_ITV_SEC		(10)	/* 10sec */

#define DEV_DRV_VERSION			(0x2D)

enum pm_type {
	PM_TYPE_VCHGIN = 0,
	PM_TYPE_VWCIN,
	PM_TYPE_VBYP,
	PM_TYPE_VSYS,
	PM_TYPE_VBAT,
	PM_TYPE_VCC1,
	PM_TYPE_VGPADC,
	PM_TYPE_ICHGIN,
	PM_TYPE_IWCIN,
	PM_TYPE_VCC2,
	PM_TYPE_IOTG,
	PM_TYPE_ITX,
	PM_TYPE_MAX,
};

enum {
	CONTINUOUS_MODE = 0,
	REQUEST_RESPONSE_MODE,
};


enum pm_setprop_opmode {
	PM_OPMODE_PROP_GPADC_DISABLE,
	PM_OPMODE_PROP_GPADC_ENABLE,
	PM_OPMODE_PROP_GPADC_RMEASURE,
	PM_OPMODE_PROP_GPADC_VMEASURE,
};

typedef enum {
	PM_MODE_NONE = 0,
	PM_RMODE,
	PM_VMODE,
} pm_mode_t;

typedef enum {
	PM_DRY_IDLE = 0,
	PM_DRY_DETECTING,
	PM_WATER1_IDLE,
	PM_WATER2_IDLE,
	PM_WATER_DETECTING,
} pm_water_status_t;

typedef enum {
	PM_WATER_SEN_NONE = 0,
	PM_WATER_SEN_LOW,
	PM_WATER_SEN_MIDDLE,
	PM_WATER_SEN_HIGH,
	PM_WATER_SEN_CNT_MAX,
} pm_water_sensitivity_t;

#if 0
enum power_supply_pm_property {
	POWER_SUPPLY_PROP_VWCIN,
	POWER_SUPPLY_PROP_VBYP,
	POWER_SUPPLY_PROP_VSYS,
	POWER_SUPPLY_PROP_VBAT,
	POWER_SUPPLY_PROP_VGPADC,
	POWER_SUPPLY_PROP_VCC1,
	POWER_SUPPLY_PROP_VCC2,
	POWER_SUPPLY_PROP_ICHGIN,
	POWER_SUPPLY_PROP_IWCIN,
	POWER_SUPPLY_PROP_IOTG,
	POWER_SUPPLY_PROP_ITX,
	POWER_SUPPLY_PROP_CO_ENABLE,
	POWER_SUPPLY_PROP_RR_ENABLE,
};
#endif

#define HYST_LEV_VCHGIN_SHIFT	4
#define HYST_LEV_VCHGIN_MASK	0x70

enum {
	HYST_LEV_VCHGIN_80mV	=	0x1,
	HYST_LEV_VCHGIN_160mV	=	0x2,
	HYST_LEV_VCHGIN_320mV	=	0x3,
	HYST_LEV_VCHGIN_640mV	=	0x4,
	HYST_LEV_VCHGIN_1280mV	=	0x5,
	HYST_LEV_VCHGIN_2560mV	=	0x6,
	HYST_LEV_VCHGIN_5120mV	=	0x7,
};

#define HYST_LEV_VGPADC_SHIFT	4
#define HYST_LEV_VGPADC_MASK	0x70
#define HYST_LEV_VGPADC_DEFALUT HYST_LEV_VGPADC_640mV
#define HYST_LEV_VGPADC_MAX	HYST_LEV_VGPADC_2560mV

enum {
	HYST_LEV_VGPADC_20mV	=	0x0,
	HYST_LEV_VGPADC_40mV	=	0x1,
	HYST_LEV_VGPADC_80mV	=	0x2,
	HYST_LEV_VGPADC_160mV	=	0x3,
	HYST_LEV_VGPADC_320mV	=	0x4,
	HYST_LEV_VGPADC_640mV	=	0x5,
	HYST_LEV_VGPADC_1280mV	=	0x6,
	HYST_LEV_VGPADC_2560mV	=	0x7,
};

#define HYST_CON_VCHGIN_SHIFT	6
#define HYST_CON_VCHGIN_MASK	0xC0
enum {
	HYST_CON_VCHGIN_1	=	0x1,
	HYST_CON_VCHGIN_4	=	0x2,
	HYST_CON_VCHGIN_16	=	0x3,
	HYST_CON_VCHGIN_32	=	0x4,
};

struct s2mu106_pmeter_data {
	struct i2c_client       *i2c;
	struct device *dev;
	struct s2mu106_platform_data *s2mu106_pdata;

	int irq_vchgin;
	int irq_gpadc;

	struct power_supply	*psy_pm;
	struct power_supply_desc psy_pm_desc;

	struct mutex 	pmeter_mutex;

#if defined(CONFIG_PM_S2M_WATER)
	struct mutex water_mutex;
	pm_water_status_t water_status;
	struct power_supply *pdic_psy;
	struct power_supply *muic_psy;
	pm_water_sensitivity_t water_sen;
	int water_det_cnt[PM_WATER_SEN_CNT_MAX];
	struct timeval last_gpadc_irq;
	struct delayed_work late_init_work;
	struct wake_lock water1_mon_wake_lock;
	struct wake_lock water2_work_wake_lock;
	struct alarm water1_mon;
	struct delayed_work water2_handler;
	struct delayed_work water2_work;
	bool is_water2_idle;
	int water2_work_call_cnt;
	int water2_work_itv;
	struct delayed_work pwr_off_chk_work;
	bool is_pwr_off_water;
#endif
};

#endif /*S2MU106_PMETER_H*/
