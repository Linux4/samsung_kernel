#ifndef __88PM830_FG_H
#define __88PM830_FG_H

#include <linux/mfd/88pm830.h>
#include <linux/mfd/88pm80x.h>
#include <linux/power_supply.h>

#define LOW_BAT_CAP			(15)

enum {
	PMIC_PWRDOWN_LOG,
	PMIC_STORED_DATA,
	PMIC_SLP_CNT,
	PMIC_VBAT_SLP,
};

struct pm830_battery_params {
	int status;
	int present;
	int volt;	/* µV */
	int ibat;	/* mA */
	int cap;	/* percents: 0~100% */
	int health;
	int tech;
	int temp;
};

struct ntc_convert {
	int temp;
	int voltage;
};

enum {
	NTC_BIAS_LOW = 0,
	NTC_BIAS_MID,
	NTC_BIAS_HI,
};

struct pm830_battery_info {
	struct pm830_chip	*chip;
	struct device	*dev;
	struct pm830_battery_params	bat_params;

	struct power_supply	battery;
	struct delayed_work	monitor_work;
	struct delayed_work	charged_work;
	struct delayed_work	cc_work; /* cc_work for sigma-delta offset compensation */
	struct work_struct	tbat_work;
	struct workqueue_struct *bat_wqueue;
	atomic_t		cc_done;

	int		r_int;
	unsigned int	bat_ntc;

	int		use_ocv;
	int		range_low_th;
	int		range_high_th;
	int		slp_con;	/* slp cnt for relaxed battery */
	int		power_off_threshold;
	int		safe_power_off_threshold;

	unsigned int	temp_range_num;
	unsigned int	*gp0_bias_curr;
	int		*switch_thr;
	int		*tbat_thr;
	unsigned int	ntc_table_size;
	struct ntc_convert *ntc_table[3];
	int		bias;

	int irq_nums;
	int irq[7];

	struct pm80x_chip *extern_chip;
};

extern const int vbat_correction_table[];
extern const int ccnt_correction_table[];

extern unsigned int get_extern_data(struct pm830_battery_info *info, int flag);
extern void set_extern_data(struct pm830_battery_info *info, int flag, int data);
extern bool sys_is_reboot(struct pm830_battery_info *info);
#endif
