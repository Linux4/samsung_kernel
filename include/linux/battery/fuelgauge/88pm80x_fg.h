#ifndef __88PM80X_FG_H
#define __88PM80X_FG_H

#include <linux/mfd/88pm822.h>

#define MONITOR_INTERVAL		(HZ * 1)

#define SLP_CNT_RD_LSB			(1 << 7)
#define HYB_DONE			(1 << 0)
#define BAT_WU_LOG			(1 << 6)

#if 0
#define dev_dbg(dev, format, arg...)		\
dev_printk(KERN_EMERG, dev, format, ##arg)
#endif

#define sec_fuelgauge_dev_t     struct pm822_chip
#define sec_fuelgauge_pdata_t   struct pm822_platform_data

struct pm80x_battery_params {
	int status;
	int present;
	int volt;	/* µV */
	int cap;	/* percents: 0~100% */
	int health;
	int tech;
	int temp;
	int chg_full;
	int chg_now;
};

struct sec_fg_info {
	struct pm822_chip	*chip;
	struct device	*dev;
	struct pm80x_battery_params	bat_params;

	struct delayed_work	monitor_work;
	struct mutex		lock;
	struct workqueue_struct *bat_wqueue;

	unsigned	present:1;

	/* State Of Connect */
	int online;
	/* battery SOC (capacity) */
	int batt_soc;
	/* battery voltage */
	int batt_voltage;
	/* battery AvgVoltage */
	int batt_avgvoltage;
	/* battery OCV */
	int batt_ocv;
	/* Current */
	int batt_current;
	/* battery Avg Current */
	int batt_avgcurrent;
	/* battery temperatue */
	u32 current_temp_adc;
};
#endif /* __88PM80X_FG_H */
