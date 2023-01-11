#ifndef __88PM80X_FG_H
#define __88PM80X_FG_H

/*
 * This header file defines the parameters for battery:
 * EB-L1M7FLU 1500mAh
 * SN: AA1D202LS/2-B
 */

#define FG_INTERVAL			(HZ * 1)
#define MONITOR_INTERVAL		(HZ * 60)
#define LOW_BAT_INTERVAL		(HZ * 20)
#define LOW_BAT_THRESHOLD		(3300)

/* bit definitions of PM800_RTC_MISC6 Register */
#define SLP_CNT_RD_LSB			(1 << 7)

/* bit definitions of PM800_POWER_DOWN_LOG2 Register */
#define HYB_DONE			(1 << 0)

/* battery internal parameters */
struct pm80x_sw_bat_params {
	int present;
	unsigned int status;

	int volt;	/* mV */
	int total_cap;

	unsigned int percent;	/* percents: 0~100% */
	unsigned int health;
	unsigned int tech;
	int temp;

	int r1;
	int r2;
	int rs;
	int r_off;
	int r_init_off;
	int r_tot;

	int online_gp_id;
	int temp_gp_id;

	int times_in_zero;
	int offset_in_zero;
	int times_in_ten;
	int offset_in_ten;

	int online_gp_bias_curr;
	int hi_volt_online;
	int lo_volt_online;

	int hi_volt_temp;
	int lo_volt_temp;

};

struct pm80x_sw_bat_info {
	struct pm80x_chip		*chip;
	struct device			*dev;

	struct pm80x_sw_bat_params	bat_data;

	struct power_supply		bat_psy;
	struct delayed_work		fg_work;
	struct delayed_work		monitor_work;
	struct delayed_work		charged_work;
	struct workqueue_struct		*bat_wqueue;
	struct mutex			lock;

	/* board specific parameters */
	bool				use_ntc;

	unsigned int safe_power_off_th;
	unsigned int power_off_th;
	unsigned int soc_ramp_up_th;
	bool need_offset_trim;

	int irq;
};

#endif
