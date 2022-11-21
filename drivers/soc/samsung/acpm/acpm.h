#ifndef __ACPM_H__
#define __ACPM_H__

struct acpm_plugins {
	unsigned int id;
	const char *fw_name;
	void __iomem *fw_base;
	struct device_node *np;
};

struct acpm_info {
	unsigned int plugin_num;
	struct device *dev;
	void __iomem *mcore_base;
	struct acpm_plugins *plugin;
};

struct acpm_debug_info {
	unsigned int period;
	void __iomem *time_index;
	unsigned int time_len;
	unsigned long long *timestamps;

	void __iomem *log_buff_rear;
	void __iomem *log_buff_front;
	void __iomem *log_buff_base;
	unsigned int log_buff_len;
	unsigned int log_buff_size;
	void __iomem *dump_base;
	unsigned int dump_size;
	void __iomem *dump_dram_base;
	unsigned int debug_logging_level;
	struct delayed_work work;

	spinlock_t lock;
};

extern void acpm_log_print(void);
extern void timestamp_write(void);
extern void *memcpy_align_4(void *dest, const void *src, unsigned int n);

#define EXYNOS_PMU_CORTEX_APM_CONFIGURATION                           (0x0100)
#define EXYNOS_PMU_CORTEX_APM_STATUS                                  (0x0104)
#define EXYNOS_PMU_CORTEX_APM_OPTION                                  (0x0108)
#define EXYNOS_PMU_CORTEX_APM_DURATION0                               (0x0110)
#define EXYNOS_PMU_CORTEX_APM_DURATION1                               (0x0114)
#define EXYNOS_PMU_CORTEX_APM_DURATION2                               (0x0118)
#define EXYNOS_PMU_CORTEX_APM_DURATION3                               (0x011C)

#define APM_LOCAL_PWR_CFG_RESET         (~(0x1 << 0))

#endif
