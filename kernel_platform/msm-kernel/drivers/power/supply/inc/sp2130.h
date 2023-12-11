
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __SP2130_H__
#define __SP2130_H__

#define pr_fmt(fmt)	"[SP2130] %s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/version.h>
//#include <linux/battmngr/battmngr_notifier.h>

typedef enum {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC1,
	ADC_VAC2,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TSBUS,
	ADC_TSBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
}ADC_CH;

#define SP2130_ROLE_STDALONE   	0
#define SP2130_ROLE_SLAVE		1
#define SP2130_ROLE_MASTER		2

enum {
	SP2130_STDALONE,
	SP2130_SLAVE,
	SP2130_MASTER,
};

static int sp2130_mode_data[] = {
	[SP2130_STDALONE] = SP2130_ROLE_STDALONE,
	[SP2130_MASTER] = SP2130_ROLE_MASTER,
	[SP2130_SLAVE] = SP2130_ROLE_SLAVE,
};

#define	BAT_OVP_ALARM		BIT(7)
#define BAT_OCP_ALARM		BIT(6)
#define	BUS_OVP_ALARM		BIT(5)
#define	BUS_OCP_ALARM		BIT(4)
#define	BAT_UCP_ALARM		BIT(3)
#define	VBUS_INSERT			BIT(7)
#define VBAT_INSERT			BIT(1)
#define	ADC_DONE			BIT(0)

#define BAT_OVP_FAULT		BIT(7)
#define BAT_OCP_FAULT		BIT(6)
#define BUS_OVP_FAULT		BIT(5)
#define BUS_OCP_FAULT		BIT(4)
#define TBUS_TBAT_ALARM		BIT(3)
#define TS_BAT_FAULT		BIT(2)
#define	TS_BUS_FAULT		BIT(1)
#define	TS_DIE_FAULT		BIT(0)

/*below used for comm with other module*/
#define	BAT_OVP_FAULT_SHIFT			0
#define	BAT_OCP_FAULT_SHIFT			1
#define	BUS_OVP_FAULT_SHIFT			2
#define	BUS_OCP_FAULT_SHIFT			3
#define	BAT_THERM_FAULT_SHIFT		4
#define	BUS_THERM_FAULT_SHIFT		5
#define	DIE_THERM_FAULT_SHIFT		6

#define	BAT_OVP_FAULT_MASK			(1 << BAT_OVP_FAULT_SHIFT)
#define	BAT_OCP_FAULT_MASK			(1 << BAT_OCP_FAULT_SHIFT)
#define	BUS_OVP_FAULT_MASK			(1 << BUS_OVP_FAULT_SHIFT)
#define	BUS_OCP_FAULT_MASK			(1 << BUS_OCP_FAULT_SHIFT)
#define	BAT_THERM_FAULT_MASK		(1 << BAT_THERM_FAULT_SHIFT)
#define	BUS_THERM_FAULT_MASK		(1 << BUS_THERM_FAULT_SHIFT)
#define	DIE_THERM_FAULT_MASK		(1 << DIE_THERM_FAULT_SHIFT)

#define	BAT_OVP_ALARM_SHIFT			0
#define	BAT_OCP_ALARM_SHIFT			1
#define	BUS_OVP_ALARM_SHIFT			2
#define	BUS_OCP_ALARM_SHIFT			3
#define	BAT_THERM_ALARM_SHIFT		4
#define	BUS_THERM_ALARM_SHIFT		5
#define	DIE_THERM_ALARM_SHIFT		6
#define BAT_UCP_ALARM_SHIFT			7

#define	BAT_OVP_ALARM_MASK			(1 << BAT_OVP_ALARM_SHIFT)
#define	BAT_OCP_ALARM_MASK			(1 << BAT_OCP_ALARM_SHIFT)
#define	BUS_OVP_ALARM_MASK			(1 << BUS_OVP_ALARM_SHIFT)
#define	BUS_OCP_ALARM_MASK			(1 << BUS_OCP_ALARM_SHIFT)
#define	BAT_THERM_ALARM_MASK		(1 << BAT_THERM_ALARM_SHIFT)
#define	BUS_THERM_ALARM_MASK		(1 << BUS_THERM_ALARM_SHIFT)
#define	DIE_THERM_ALARM_MASK		(1 << DIE_THERM_ALARM_SHIFT)
#define	BAT_UCP_ALARM_MASK			(1 << BAT_UCP_ALARM_SHIFT)

#define VBAT_REG_STATUS_SHIFT		0
#define IBAT_REG_STATUS_SHIFT		1

#define VBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)


#define ADC_REG_BASE SP2130_REG_17
#define SP2130_MAX_SHOW_REG_ADDR     0x36

#define sc_err(fmt, ...)								\
do {											\
	if (sc->mode == SP2130_ROLE_MASTER)						\
		printk(KERN_ERR "[sp2130-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SP2130_ROLE_SLAVE)					\
		printk(KERN_ERR "[sp2130-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_ERR "[sp2130-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define sc_info(fmt, ...)								\
do {											\
	if (sc->mode == SP2130_ROLE_MASTER)						\
		printk(KERN_INFO "[sp2130-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SP2130_ROLE_SLAVE)					\
		printk(KERN_INFO "[sp2130-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_INFO "[sp2130-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define sc_dbg(fmt, ...)								\
do {											\
	if (sc->mode == SP2130_ROLE_MASTER)						\
		printk(KERN_DEBUG "[sp2130-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SP2130_ROLE_SLAVE)					\
		printk(KERN_DEBUG "[sp2130-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_DEBUG "[sp2130-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);


struct sp2130_cfg {
	bool bat_ovp_disable;
	bool bat_ocp_disable;
	bool bat_ovp_alm_disable;
	bool bat_ocp_alm_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_ocp_th;
	int bat_ocp_alm_th;

	bool bus_ovp_alm_disable;
	bool bus_ocp_disable;
	bool bus_ocp_alm_disable;

	int bus_ovp_th;
	int bus_ovp_alm_th;
	int bus_ocp_th;
	int bus_ocp_alm_th;

	bool bat_ucp_alm_disable;

	int bat_ucp_alm_th;
	int ac_ovp_th;

	bool bat_therm_disable;
	bool bus_therm_disable;
	bool die_therm_disable;

	int bat_therm_th; /*in %*/
	int bus_therm_th; /*in %*/
	int die_therm_th; /*in degC*/

	int sense_r_mohm;
};

struct sp2130 {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	struct mutex charging_disable_lock;
	struct mutex irq_complete;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	bool batt_present;
	bool vbus_present;

	bool usb_present;
	bool charge_enabled;	/* Register bit status */

	bool acdrv1_enable;
	bool otg_enable;

	bool is_sp2130;
	int  vbus_error;

	/* ADC reading */
	int vbat_volt;
	int vbus_volt;
	int vout_volt;
	int vac_volt;

	int ibat_curr;
	int ibus_curr;

	int bat_temp;
	int bus_temp;
	int die_temp;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;

	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int  prev_alarm;
	int  prev_fault;

	int chg_ma;
	int chg_mv;
	int adc_status;
	int charge_state;

	struct sp2130_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct power_supply_desc psy_desc;
    	struct power_supply_config psy_cfg;
   	struct power_supply *fc2_psy;

	struct delayed_work monitor_work;
	struct dentry *debug_root;
	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;
};

int sp2130_read_byte(struct sp2130 *sc, u8 reg, u8 *data);
int sp2130_enable_charge(struct sp2130 *sc, bool enable);
int sp2130_check_charge_enabled(struct sp2130 *sc, bool *enabled);
int sp2130_enable_wdt(struct sp2130 *sc, bool enable);
int sp2130_set_reg_reset(struct sp2130 *sc);
int sp2130_enable_batovp(struct sp2130 *sc, bool enable);
int sp2130_set_batovp_th(struct sp2130 *sc, int threshold);
int sp2130_enable_batovp_alarm(struct sp2130 *sc, bool enable);
int sp2130_set_batovp_alarm_th(struct sp2130 *sc, int threshold);
int sp2130_enable_batocp(struct sp2130 *sc, bool enable);
int sp2130_set_batocp_th(struct sp2130 *sc, int threshold);
int sp2130_enable_batocp_alarm(struct sp2130 *sc, bool enable);
int sp2130_set_batocp_alarm_th(struct sp2130 *sc, int threshold);
int sp2130_set_busovp_th(struct sp2130 *sc, int threshold);
int sp2130_enable_busovp_alarm(struct sp2130 *sc, bool enable);
int sp2130_set_busovp_alarm_th(struct sp2130 *sc, int threshold);
int sp2130_enable_busocp(struct sp2130 *sc, bool enable);
int sp2130_set_busocp_th(struct sp2130 *sc, int threshold);
int sp2130_enable_busocp_alarm(struct sp2130 *sc, bool enable);
int sp2130_set_busocp_alarm_th(struct sp2130 *sc, int threshold);
int sp2130_enable_batucp_alarm(struct sp2130 *sc, bool enable);
int sp2130_set_batucp_alarm_th(struct sp2130 *sc, int threshold);
int sp2130_set_acovp_th(struct sp2130 *sc, int threshold);
int sp2130_set_vdrop_th(struct sp2130 *sc, int threshold);
int sp2130_set_vdrop_deglitch(struct sp2130 *sc, int us);
int sp2130_enable_bat_therm(struct sp2130 *sc, bool enable);
int sp2130_set_bat_therm_th(struct sp2130 *sc, u8 threshold);
int sp2130_enable_bus_therm(struct sp2130 *sc, bool enable);
int sp2130_set_bus_therm_th(struct sp2130 *sc, u8 threshold);
int sp2130_set_die_therm_th(struct sp2130 *sc, u8 threshold);
int sp2130_enable_adc(struct sp2130 *sc, bool enable);
int sp2130_set_adc_scanrate(struct sp2130 *sc, bool oneshot);
int sp2130_get_adc_data(struct sp2130 *sc, int channel,  int *result);
int sp2130_set_adc_scan(struct sp2130 *sc, int channel, bool enable);
//int sp2130_set_alarm_int_mask(struct sp2130 *sc, u8 mask);
//int sp2130_set_sense_resistor(struct sp2130 *sc, int r_mohm);
//int sp2130_enable_regulation(struct sp2130 *sc, bool enable);
int sp2130_set_ss_timeout(struct sp2130 *sc, int timeout);
//int sp2130_set_ibat_reg_th(struct sp2130 *sc, int th_ma);
//int sp2130_set_vbat_reg_th(struct sp2130 *sc, int th_mv);
int sp2130_check_vbus_error_status(struct sp2130 *sc);
int sp2130_detect_device(struct sp2130 *sc);
void sp2130_check_alarm_status(struct sp2130 *sc);
void sp2130_check_fault_status(struct sp2130 *sc);
int sp2130_set_present(struct sp2130 *sc, bool present);

bool get_sp2130_device_config(void);
int sp2130_enable_acdrv1(struct sp2130 *sc, bool enable);
int sp2130_enable_acdrv2(struct sp2130 *sc, bool enable);
int sp2130_check_enable_acdrv1(struct sp2130 *sc, bool *enable);
int sp2130_check_enable_acdrv2(struct sp2130 *sc, bool *enable);

int sp2130_enable_otg(struct sp2130 *sc, bool enable);
int sp2130_check_enable_otg(struct sp2130 *sc, bool *enable);



#endif /* __SP2130_H__ */

