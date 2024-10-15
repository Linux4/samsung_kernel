#ifndef _GOVERNOR_LEALT_H
#define _GOVERNOR_LEALT_H

#include <linux/kernel.h>
#include <linux/devfreq.h>

enum common_ev_idx {
	INST_IDX,
	CYC_IDX,
	STALL_IDX,
	NUM_COMMON_EVS
};

struct dev_stats {
	int id;
	unsigned long inst_count;
	unsigned long mem_count;
	unsigned long freq;
	unsigned long stall_pct;
};

struct core_dev_map {
	unsigned int core_mhz;
	unsigned int target_load;
};

struct lealt_hwmon {
	int (*start_hwmon)(struct lealt_hwmon *hw);
	void (*stop_hwmon)(struct lealt_hwmon *hw);
	unsigned long (*get_cnt)(struct lealt_hwmon *hw);
	struct device_node *(*get_child_of_node)(struct device *dev);
	void (*request_update_ms)(struct lealt_hwmon *hw,
				  unsigned int update_ms);
	struct device *dev;
	struct device_node *of_node;

	unsigned int num_cores;
	struct dev_stats *core_stats;

	struct devfreq *df;
	struct core_dev_map *freq_map;
	bool should_ignore_df_monitor;

	unsigned int prepare[2];
	unsigned int stability_th;
	unsigned int llc_on_th_cpu;
	unsigned int base_minlock_ratio;
	unsigned int base_maxlock_ratio;
};

int register_lealt(struct device *dev, struct lealt_hwmon *hw);

#endif /* _GOVERNOR_LEALT_H */
