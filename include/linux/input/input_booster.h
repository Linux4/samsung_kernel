#ifndef _LINUX_INPUT_BOOSTER_H
#define _LINUX_INPUT_BOOSTER_H

#define INPUT_BOOSTER_NAME "input_booster"

/* Booster levels are used as below comment.
 * 0	: Disable
 * 1	: SIP (Default SIP)
 * 2	: Default
 * 3	: Browser, Google talk
 * 9	: Max for benchmark tool
 */
enum booster_level {
	BOOSTER_LEVEL0 = 0,
	BOOSTER_LEVEL1,
	BOOSTER_LEVEL2,
	BOOSTER_LEVEL3,
	BOOSTER_LEVEL_MAX,
};

/* Booster devie
 * Current we support Touch, Pen booster
 * If you want to add booster device, add it this after touch type
 */
enum booster_device_type {
	BOOSTER_DEVICE_TOUCH = 0,
	BOOSTER_DEVICE_MAX,
	BOOSTER_DEVICE_NOT_DEFINED,
};

enum booster_mode {
	BOOSTER_MODE_OFF = 0,
	BOOSTER_MODE_ON,
	BOOSTER_MODE_FORCE_OFF,
};

/* Booster times.
 *
 * (1) head_time
 * (2) tail_time
 * If head has phase, phase_time represent that.
 *		|----
 *		|1	|
 *		|	---------
 *_ ____|		2	|___
 */
struct dvfs_time {
	int head_time;
	int tail_time;
	int phase_time;
};

/* Frequency type
 * cpu_freq : Big frequency
 */
struct dvfs_freq {
	s32 cpu_freq;
	s32 chg_cpu_freq;
};

struct booster_device {
	const char *desc;
	int type;
	const struct dvfs_freq *freq_table;
	const struct dvfs_time *time_table;
};

struct input_booster_platform_data {
	struct booster_device *devices;
	int ndevices;
	struct dvfs_freq *freq_tables[BOOSTER_DEVICE_MAX];
	struct dvfs_time *time_tables[BOOSTER_DEVICE_MAX];
};

extern void input_booster_send_event(unsigned int type, int value);

/* Now we support sysfs file to change booster level under input_booster class.
 * But previous sysfs is scatterd into each driver, so we need to support it also.
 * Enable below define. if you want to use previous sysfs files.
 */
extern void change_booster_level_for_tsp(unsigned char level);

#endif /* _LINUX_INPUT_BOOSTER_H */
