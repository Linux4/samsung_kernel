
#ifndef _INPUT_BOOSTER_H_
#define _INPUT_BOOSTER_H_

#include <linux/cpufreq.h>

enum input_booster_id {
	INPUT_BOOSTER_ID_TSP = 0,
	INPUT_BOOSTER_ID_TKEY,
	INPUT_BOOSTER_ID_WACOM,
};

#define DVFS_STAGE_NONE		1 << 0	// 0000 0000 0000 0001
#define DVFS_STAGE_SINGLE	1 << 1	// 0000 0000 0000 0010
#define DVFS_STAGE_DUAL		1 << 2	// 0000 0000 0000 0100
#define DVFS_STAGE_TRIPLE	1 << 3	// 0000 0000 0000 1000

/* Set DVFS STAGE for each model */
/* TSP BOOSTER */
#if defined(CONFIG_SEC_KLEOS_PROJECT) | defined(CONFIG_SEC_HEAT_PROJECT) \
	| defined(CONFIG_SEC_FORTUNA_PROJECT) | defined(CONFIG_SEC_A3_PROJECT)
#define DVFS_TSP_STAGE		(DVFS_STAGE_NONE | DVFS_STAGE_SINGLE \
				| DVFS_STAGE_DUAL)
#endif

/* Do not modify */
#ifndef DVFS_TSP_STAGE
#define DVFS_TSP_STAGE		0
#endif
#ifndef DVFS_TKEY_STAGE
#define DVFS_TKEY_STAGE		0
#endif
#ifndef DVFS_WACOM_STAGE
#define DVFS_WACOM_STAGE	0
#endif

/* Do not modify */
#if DVFS_TSP_STAGE
#define TSP_BOOSTER
#endif
#if DVFS_TKEY_STAGE
#define TKEY_BOOSTER
#endif
#if DVFS_WACOM_STAGE
#define WACOM_BOOSTER
#endif

/* TSP */
#define INPUT_BOOSTER_OFF_TIME_TSP		500
#define INPUT_BOOSTER_CHG_TIME_TSP		130

/* Touchkey */
#define INPUT_BOOSTER_OFF_TIME_TKEY		500
#define INPUT_BOOSTER_CHG_TIME_TKEY		500

/* Wacom */
#define INPUT_BOOSTER_OFF_TIME_WACOM		500
#define INPUT_BOOSTER_CHG_TIME_WACOM		130

struct input_booster {
	struct delayed_work	work_dvfs_off;
	struct delayed_work	work_dvfs_chg;
	struct mutex		dvfs_lock;

	bool dvfs_lock_status;
	int dvfs_old_stauts;
	int dvfs_boost_mode;
	int dvfs_freq;
	int dvfs_id;
	int dvfs_stage;

	int (*dvfs_off)(struct input_booster *);
	void (*dvfs_set)(struct input_booster *, int);
};

void input_booster_init_dvfs(struct input_booster *booster, int id);

#endif /* _INPUT_BOOSTER_H_ */

