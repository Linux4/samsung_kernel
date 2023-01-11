#ifndef __VOLTAGE_MRVL_H__
#define __VOLTAGE_MRVL_H__

#include <linux/thermal.h>
#include <linux/cpumask.h>

#define CPU_MAX_NUM	num_possible_cpus()
#define MAX_FREQ_PP	20
#define KHZ_TO_HZ	1000
#define VL_MAX	8
#define CMD_BUF_SIZE	120

enum trip_range {
	TRIP_RANGE_0 = 0,
	TRIP_RANGE_1,
	TRIP_RANGE_2,
	TRIP_RANGE_3,
	TRIP_RANGE_4,
	TRIP_RANGE_5,
	TRIP_RANGE_6,
	TRIP_RANGE_7,
	TRIP_RANGE_8,
	TRIP_RANGE_9,
	TRIP_RANGE_MAX = TRIP_RANGE_9,
	TRIP_RANGE_NUM = TRIP_RANGE_MAX + 1,
};

enum throttle {
	THROTTLE_VL = 0,
	THROTTLE_CORE,
#ifdef CONFIG_PXA1936_THERMAL
	THROTTLE_CORE1,
#endif
	THROTTLE_HOTPLUG,
	THROTTLE_DDR,
	THROTTLE_GC3D,
	THROTTLE_GC2D,
	THROTTLE_GCSH,
	THROTTLE_VPU,
	THROTTLE_NUM,
};

struct tsen_cooling_device {
	int max_state, cur_state;
	struct thermal_cooling_device *cool_tsen;
};

struct state_freq_tbl {
	int freq_num;
	unsigned int freq_tbl[MAX_FREQ_PP];
};

struct freq_cooling_device {
	struct thermal_cooling_device *cool_cpufreq;
	struct thermal_cooling_device *cool_cpu1freq;
	struct thermal_cooling_device *cool_cpuhotplug;
	struct thermal_cooling_device *cool_ddrfreq;
	struct thermal_cooling_device *cool_gc2dfreq;
	struct thermal_cooling_device *cool_gc3dfreq;
	struct thermal_cooling_device *cool_gcshfreq;
	struct thermal_cooling_device *cool_vpufreq;
};

enum thermal_policy {
	POWER_SAVING_MODE,
	BENCHMARK_MODE,
	POLICY_NUMBER,
};

struct pxa_voltage_thermal {
	struct freq_cooling_device cool_pxa;
	struct thermal_zone_device *therm_max;
	int tsen_trips_temp[POLICY_NUMBER][THERMAL_MAX_TRIPS];
	int tsen_trips_temp_d[POLICY_NUMBER][THERMAL_MAX_TRIPS];
	unsigned int (*tsen_throttle_tbl)[THROTTLE_NUM][THERMAL_MAX_TRIPS+1];
	int (*set_threshold)(int range);
	int profile;
	enum thermal_policy therm_policy;
	int vl_master;
	struct mutex policy_lock;
	struct mutex data_lock;
	char cpu_name[20];
	unsigned int trip_num;
	unsigned int trip_active_num;
	unsigned int range_num;
	unsigned int range_max;
	struct state_freq_tbl cpufreq_tbl;
	struct state_freq_tbl cpu1freq_tbl;
	struct state_freq_tbl ddrfreq_tbl;
	struct state_freq_tbl gc3dfreq_tbl;
	struct state_freq_tbl gc2dfreq_tbl;
	struct state_freq_tbl gcshfreq_tbl;
	struct state_freq_tbl vpufreq_tbl;
};


extern int register_debug_interface(void);
extern int tsen_policy_dump(char *buf, int size);
extern int tsen_update_policy(void);
extern int voltage_mrvl_init(struct pxa_voltage_thermal *voltage_thermal);

#endif /* __VOLTAGE_MRVL_H__ */
