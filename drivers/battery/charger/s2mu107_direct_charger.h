/*
 * s2mu107_charger.h - Header of S2MU107 Charger Driver
 *
 * Copyright (C) 2019 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef S2MU107_DIRECT_CHARGER_H
#define S2MU107_DIRECT_CHARGER_H
#include <linux/mfd/samsung/s2mu107.h>
#include <linux/wakelock.h>

#include "../common/include/sec_direct_charger.h"

/* Test debug log enable */
#define EN_TEST_READ 1

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

#define ENABLE 1
#define DISABLE 0

#define S2MU107_DC_INT0		0x03
#define S2MU107_DC_INT1		0x04
#define S2MU107_DC_INT2		0x05
#define S2MU107_DC_INT3		0x06

#define S2MU107_DC_INT0_MASK		0x0B
#define S2MU107_DC_INT1_MASK		0x0C
#define S2MU107_DC_INT2_MASK		0x0D
#define S2MU107_DC_INT3_MASK		0x0E

#define S2MU107_DC_STATUS0		0x17
#define S2MU107_SC_CTRL0		0x18
#define S2MU107_SC_CTRL8_DC		0x20

#define S2MU107_DC_CTRL0		0x41
#define S2MU107_DC_CTRL1		0x42
#define S2MU107_DC_CTRL2		0x43
#define S2MU107_DC_CTRL3		0x44
#define S2MU107_DC_CTRL4		0x45
#define S2MU107_DC_CTRL5		0x46
#define S2MU107_DC_CTRL6		0x47
#define S2MU107_DC_CTRL7		0x48
#define S2MU107_DC_CTRL8		0x49
#define S2MU107_DC_CTRL9		0x4A
#define S2MU107_DC_CTRL10		0x4B

#define S2MU107_DC_CTRL11		0x4C
#define S2MU107_DC_CTRL12		0x4D
#define S2MU107_DC_CTRL13		0x4E
#define S2MU107_DC_CTRL14		0x4F
#define S2MU107_DC_CTRL15		0x50
#define S2MU107_DC_CTRL16		0x51
#define S2MU107_DC_CTRL17		0x52
#define S2MU107_DC_CTRL18		0x53
#define S2MU107_DC_CTRL19		0x54
#define S2MU107_DC_CTRL20		0x55

#define S2MU107_DC_CTRL21		0x56
#define S2MU107_DC_CTRL22		0x57
#define S2MU107_DC_CTRL23		0x58
#define S2MU107_DC_CTRL24		0x59
#define S2MU107_DC_CTRL25		0x5A

#define S2MU107_DC_TEST0		0x5B
#define S2MU107_DC_TEST1		0x5C
#define S2MU107_DC_TEST2		0x5D
#define S2MU107_DC_TEST3		0x5E
#define S2MU107_DC_TEST4		0x5F
#define S2MU107_DC_TEST5		0x60
#define S2MU107_DC_TEST6		0x61
#define S2MU107_DC_TEST7		0x62

#define S2MU107_SC_OTP103		0xD3
#define S2MU107_DC_CD_OTP_01		0xD8
#define S2MU107_DC_CD_OTP_02		0xD9
#define S2MU107_DC_INPUT_OTP_04		0xDF

/* S2MU107_DC_STATUS0 */
#define CV_REGULATION_STATUS		0x10
#define LONG_CC_STATUS			0x08
#define THERMAL_STATUS			0x04
#define ICL_STATUS			0x02
#define NORMAL_CHARGING_STATUS		0x01

/* S2MU107_SC_CTRL0 */
#define SC_REG_MODE_SHIFT		0
#define SC_REG_MODE_WIDTH		4
#define SC_REG_MODE_MASK		MASK(SC_REG_MODE_WIDTH, SC_REG_MODE_SHIFT)
#define SC_REG_MODE_DUAL_BUCK		(0x0d)

/* S2MU107_DC_CTRL0 */
#define FREQ_SEL_SHIFT			5
#define FREQ_SEL_WIDTH			3
#define FREQ_SEL_MASK			MASK(FREQ_SEL_WIDTH, FREQ_SEL_SHIFT)

#define DC_EN_MASK			0x01
#define DC_FORCED_EN_SHIFT		1
#define DC_FORCED_EN_MASK		BIT(DC_FORCED_EN_SHIFT)

/* S2MU107_DC_CTRL1 */
#define SET_CV_SHIFT			0
#define SET_CV_WIDTH			7
#define SET_CV_MASK			MASK(SET_CV_WIDTH, SET_CV_SHIFT)
#define SET_CV_STEP_UV			(10000)

/* S2MU107_DC_CTRL2 */
#define DC_SET_CHGIN_ICHG_SHIFT		0
#define	DC_SET_CHGIN_ICHG_WIDTH		7
#define DC_SET_CHGIN_ICHG_MASK		MASK(DC_SET_CHGIN_ICHG_WIDTH,\
					DC_SET_CHGIN_ICHG_SHIFT)
/* S2MU107_DC_CTRL3 */
#define DC_SET_CHGIN_IDISCHG_SHIFT	0
#define	DC_SET_CHGIN_IDISCHG_WIDTH	7
#define DC_SET_CHGIN_IDISCHG_MASK	MASK(DC_SET_CHGIN_IDISCHG_WIDTH,\
					DC_SET_CHGIN_IDISCHG_SHIFT)
/* S2MU107_DC_CTRL4 */
#define DC_SET_WCIN_ICHG_SHIFT		0
#define	DC_SET_WCIN_ICHG_WIDTH		7
#define DC_SET_WCIN_ICHG_MASK		MASK(DC_SET_WCIN_ICHG_WIDTH,\
					DC_SET_WCIN_ICHG_SHIFT)
/* S2MU107_DC_CTRL5 */
#define DC_SET_WCIN_IDISCHG_SHIFT	0
#define	DC_SET_WCIN_IDISCHG_WIDTH	7
#define DC_SET_WCIN_IDISCHG_MASK	MASK(DC_SET_WCIN_IDISCHG_WIDTH,\
					DC_SET_WCIN_IDISCHG_SHIFT)

/* S2MU107_DC_CTRL6 */
#define CHGIN_OVP_RISE_SHIFT		4
#define CHGIN_OVP_RISE_WIDTH		4
#define CHGIN_OVP_RISE_MASK		MASK(CHGIN_OVP_RISE_WIDTH, CHGIN_OVP_RISE_SHIFT)

#define CHGIN_OVP_FALL_SHIFT		0
#define CHGIN_OVP_FALL_WIDTH		4
#define CHGIN_OVP_FALL_MASK		MASK(CHGIN_OVP_FALL_WIDTH, CHGIN_OVP_FALL_SHIFT)

/* S2MU107_DC_CTRL7 */
#define DC_SET_VBYP_OVP_SHIFT		0
#define DC_SET_VBYP_OVP_WIDTH		7
#define DC_SET_VBYP_OVP_MASK		MASK(DC_SET_VBYP_OVP_WIDTH, DC_SET_VBYP_OVP_SHIFT)

/* S2MU107_DC_CTRL8 */
#define DC_SET_VBAT_OVP_SHIFT		0
#define DC_SET_VBAT_OVP_WIDTH		8
#define DC_SET_VBAT_OVP_MASK		MASK(DC_SET_VBYP_OVP_WIDTH, DC_SET_VBYP_OVP_SHIFT)

/* S2MU107_DC_CTRL13 */
#define DC_SET_CC_SHIFT			0
#define DC_SET_CC_WIDTH			7
#define DC_SET_CC_MASK			MASK(DC_SET_CC_WIDTH, DC_SET_CC_SHIFT)

/* S2MU107_DC_CTRL15 */
#define DC_TOPOFF_SHIFT			4
#define DC_TOPOFF_WIDTH			4
#define DC_TOPOFF_MASK			MASK(DC_TOPOFF_WIDTH, DC_TOPOFF_SHIFT)

#define DC_CC_BAND_WIDTH_SHIFT		0
#define DC_CC_BAND_WIDTH_WIDTH		3
#define DC_CC_BAND_WIDTH_MASK		MASK(DC_CC_BAND_WIDTH_WIDTH, DC_CC_BAND_WIDTH_SHIFT)
//#define DC_CC_BAND_WIDTH_MIN_MA		(150)
//#define DC_CC_BAND_WIDTH_STEP_MA	(50)
#define DC_CC_BAND_WIDTH_MIN_MA		(300)
#define DC_CC_BAND_WIDTH_400_MA		(400)
#define DC_CC_BAND_WIDTH_STEP_MA	(100)

/* S2MU107_DC_CTRL16 */
#define DC_EN_LONG_CC_SHIFT		7
#define	DC_EN_LONG_CC_MASK		BIT(DC_EN_LONG_CC_SHIFT)
#define	CV_OFF_SHIFT			6
#define	CV_OFF_MASK			BIT(CV_OFF_SHIFT)

#define	DC_LONG_CC_STEP_SHIFT		0
#define DC_LONG_CC_STEP_WIDTH		5
#define DC_LONG_CC_STEP_MASK		MASK(DC_LONG_CC_STEP_WIDTH, DC_LONG_CC_STEP_SHIFT)

/* S2MU107_DC_CTRL20 */
#define	DC_EN_THERMAL_LOOP_SHIFT	7
#define DC_EN_THERMAL_LOOP_MASK		BIT(DC_EN_THERMAL_LOOP_SHIFT)

#define TEMPERATURE_SOURCE_MASK		0x40
#define CHIP_JUNCTION			0
#define	EXTERNAL_NTC			1

#define THERMAL_RISING_SHIFT		0
#define	THERMAL_RISING_WIDTH		6
#define THERMAL_RISING_MASK		MASK(THERMAL_RISING_WIDTH, THERMAL_RISING_SHIFT)

/* S2MU107_DC_CTRL21 */
#define THERMAL_FALLING_SHIFT		0
#define	THERMAL_FALLING_WIDTH		6
#define THERMAL_FALLING_MASK		MASK(THERMAL_FALLING_WIDTH, THERMAL_FALLING_SHIFT)

/* S2MU107_DC_CTRL22 */
#define	TA_COMMUNICATION_FAIL_MASK	BIT(1)
#define TA_TRANSIENT_DONE_MASK		BIT(0)

/* S2MU107_DC_CTRL23 */
#define THERMAL_WAIT_SHIFT		4
#define THERMAL_WAIT_WIDTH		3
#define THERMAL_WAIT_MASK		MASK(THERMAL_WAIT_WIDTH, THERMAL_WAIT_SHIFT)

#define DC_SEL_BAT_OK_SHIFT		0
#define DC_SEL_BAT_OK_WIDTH		4
#define DC_SEL_BAT_OK_MASK		MASK(DC_SEL_BAT_OK_WIDTH, DC_SEL_BAT_OK_SHIFT)

/* S2MU107_DC_TEST2 */
#define DC_EN_OCP_FLAG_SHIFT		6
#define DC_EN_OCP_FLAG_WIDTH		2
#define DC_EN_OCP_FLAG_MASK		MASK(DC_EN_OCP_FLAG_WIDTH, DC_EN_OCP_FLAG_SHIFT)

/* S2MU107_DC_TEST4 */
#define T_DC_EN_WCIN_PWRTR_MODE_SHIFT	5
#define T_DC_EN_WCIN_PWRTR_MODE_MASK	BIT(T_DC_EN_WCIN_PWRTR_MODE_SHIFT)

/* S2MU107_DC_CD_OTP_01 */
#define SET_IIN_OFF_SHIFT		4
#define SET_IIN_OFF_WIDTH		4
#define SET_IIN_OFF_MASK		MASK(SET_IIN_OFF_WIDTH, SET_IIN_OFF_SHIFT)
#define SET_IIN_OFF_MIN_MA		(100)
#define SET_IIN_OFF_STEP_MA		(50)
#define SET_IIN_OFF_TARGET		(200)

#define DIG_CV_SHIFT		0
#define DIG_CV_WIDTH		4
#define DIG_CV_MASK		MASK(DIG_CV_WIDTH, DIG_CV_SHIFT)
#define DIG_CV_MAX_UV		(250000)
#define DIG_CV_STEP_UV		(15625)

/* S2MU107_SC_OTP103 */
#define T_SET_VF_VBAT_MASK		0x07

#define FAKE_BAT_LEVEL          50

typedef enum {
	DC_STATE_OFF = 0,
	DC_STATE_CHECK_VBAT,
	DC_STATE_PRESET,
	DC_STATE_START_CC,
	DC_STATE_CC,
	DC_STATE_SLEEP_CC,
	DC_STATE_WAKEUP_CC,
	DC_STATE_DONE,
	DC_STATE_MAX_NUM,
} s2mu107_dc_state_t;

typedef enum {
	DC_TRANS_INVALID = 0,
	DC_TRANS_CHG_ENABLED,
	DC_TRANS_NO_COND,
	DC_TRANS_VBATT_RETRY,
	DC_TRANS_WAKEUP_DONE,
	DC_TRANS_RAMPUP,
	DC_TRANS_RAMPUP_FINISHED,
	DC_TRANS_BATTERY_OK,
	DC_TRANS_BATTERY_NG,
	DC_TRANS_CHGIN_OKB_INT,
	DC_TRANS_BAT_OKB_INT,
	DC_TRANS_NORMAL_CHG_INT,
	DC_TRANS_DC_DONE_INT,
	DC_TRANS_CC_STABLED,
	DC_TRANS_CC_WAKEUP,
	DC_TRANS_DETACH,
	DC_TRANS_FAIL_INT,
	DC_TRANS_MAX_NUM,
} s2mu107_dc_trans_t;

typedef enum {
	PMETER_ID_VCHGIN = 0,
	PMETER_ID_ICHGIN,
	PMETER_ID_IWCIN,
	PMETER_ID_VBATT,
	PMETER_ID_VSYS,
	PMETER_ID_TDIE,
	PMETER_ID_MAX_NUM,
} s2mu107_pmeter_id_t;

typedef enum {
	S2MU107_DC_DISABLE = 0,
	S2MU107_DC_ENABLE
} s2mu107_dc_ctrl_t;

typedef enum {
	S2MU107_DC_PPS_SIGNAL_DEC = 0,
	S2MU107_DC_PPS_SIGNAL_INC
} s2mu107_dc_pps_signal_t;

enum {
	TA_PWR_TYPE_25W = 0,
	TA_PWR_TYPE_45W,
};

#define SC_CHARGING_ENABLE_CHECK_VBAT	5
//#define DC_TA_MIN_CURRENT	1000
#define DC_TA_MIN_CURRENT	2000
#define DC_MIN_VBAT		3400
#define DC_TA_CURR_STEP_MA	50
#define DC_TA_VOL_STEP_MV	20
#define DC_MAX_SOC		95
#define DC_MAX_SHIFTING_CNT	60

#define CONV_STR(x, r) { case x: r = #x; break; }

ssize_t s2mu107_dc_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

ssize_t s2mu107_dc_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define S2MU107_DIRECT_CHARGER_ATTR(_name)				\
{							                    \
	.attr = {.name = #_name, .mode = 0664},	    \
	.show = s2mu107_dc_show_attrs,			    \
	.store = s2mu107_dc_store_attrs,			\
}

typedef struct s2mu107_dc_platform_data {
	char *dc_name;
	char *pm_name;
	char *fg_name;
	char *sc_name;
	char *sec_dc_name;
	int input_current_limit;
	int charging_current;
	int topoff_current;
	int sense_resistance;
	int recharge_vcell;
	int chg_switching_freq;
	int temperature_source; /* 0 : chip junction, 1 : external ntc for FG */
	unsigned int step_charge_level;
	unsigned int *dc_step_voltage_45w;
	unsigned int *dc_step_current_45w;
	unsigned int *dc_step_voltage_25w;
	unsigned int *dc_step_current_25w;
	unsigned int *dc_c_rate;
} s2mu107_dc_platform_data_t;

typedef struct s2mu107_dc_pd_data {
	unsigned int pdo_pos;
	unsigned int taMaxVol;
	unsigned int taMaxCur;
	unsigned int taMaxPwr;
} s2mu107_dc_pd_data_t;

typedef struct s2mu107_dc_prop {
	int prop;
	union power_supply_propval value;
} s2mu107_dc_prop_t;

struct s2mu107_dc_data {
	struct i2c_client       *i2c;
	struct i2c_client       *i2c_common;
	struct device *dev;
	struct s2mu107_platform_data *s2mu107_pdata;

	struct delayed_work timer_wqueue;
	struct delayed_work check_vbat_wqueue;
	struct delayed_work start_cc_wqueue;
	struct delayed_work state_manager_wqueue;
	struct delayed_work set_prop_wqueue;

	/* step check monitor */
	struct delayed_work step_monitor_work;
	struct workqueue_struct *step_monitor_wqueue;
	struct wake_lock step_monitor_wake_lock;
	struct alarm step_monitor_alarm;
	unsigned int alarm_interval;
	int step_now;

	struct power_supply *psy_dc;
	struct power_supply_desc psy_dc_desc;

	s2mu107_dc_platform_data_t *pdata;
	s2mu107_dc_prop_t prop_data;

	int dev_id;
	int input_current;
	int charging_current;
	int topoff_current;
	int cable_type;
	bool is_charging;
	bool is_longCC;
	int ta_pwr_type;
	struct mutex dc_mutex;
	struct mutex timer_mutex;
	struct mutex dc_state_mutex;
	struct mutex pps_isr_mutex;
	struct wake_lock wake_lock;
	struct wake_lock step_mon;
	struct wake_lock mode_irq;
	struct wake_lock fail_irq;
	struct wake_lock thermal_irq;

	s2mu107_dc_state_t dc_state;
	s2mu107_dc_trans_t dc_trans;
	void (*dc_state_fp[DC_STATE_MAX_NUM])(void *);
	s2mu107_dc_pd_data_t *pd_data;
	struct power_supply *psy_pmeter;
	int pmeter[PMETER_ID_MAX_NUM];
	unsigned int targetIIN;
	unsigned int adjustIIN;
	unsigned int minIIN;
	unsigned int chgIIN;
	unsigned int ppsVol;
	unsigned int prev_ppsVol;
	unsigned int prev_ppsCurr;
	unsigned int floatVol;
	unsigned int cnt_vsys_warn;
	int vchgin_okb_retry;
	unsigned int cc_count;
	unsigned int step_mon_cnt;

	bool is_shifting;
	unsigned int shifting_cnt;
	unsigned int prev_chg_curr;
	unsigned int next_chg_curr;
	wait_queue_head_t wait;
	bool is_pps_ready;
	int rise_speed_dly_ms;

	bool ovp;

	int unhealth_cnt;
	int dc_chg_status;
	int health;

	int irq_base;

	/* mode irq */
	int irq_cc;
	int irq_done;

	/* error irq */
	int irq_cv;
	int irq_pps_fail;

	int irq_vchgin_okb;
	int irq_vbat_okb;

	int irq_byp2out_ovp;
	int irq_byp_ovp;
	int irq_out_ovp;

	int irq_ocp;
	int irq_rcp;

	int irq_plug_out;
	int irq_diod_prot;

	int irq_thermal;
	int irq_tsd;
	int irq_tfd;

	int irq_off;

	int charge_mode;
};

extern int sec_pd_get_apdo_max_power(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
extern int sec_pd_select_pps(int num, int ppsVol, int ppsCur);
extern int sec_pps_enable(int num, int ppsVol, int ppsCur, int enable);
extern int sec_get_pps_voltage(void);

#endif /*S2MU107_CHARGER_H*/
