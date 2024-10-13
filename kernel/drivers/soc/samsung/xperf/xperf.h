/*
 * Exynos Performance
 * Author: Jungwook Kim, jwook1.kim@samsung.com
 * Created: 2014
 * Updated: 2015, 2019, 2021
 */

#ifndef EXYNOS_PERF_INTERNAL_H
#define EXYNOS_PERF_TNTERNAL_H __FILE__

#ifndef CONFIG_VENDOR_NR_CPUS
/* FIXME: This should be fixed in the arch's Kconfig */
#define CONFIG_VENDOR_NR_CPUS   1
#endif

#define VENDOR_NR_CPUS		CONFIG_VENDOR_NR_CPUS
#define MAX_CLUSTER		5
#define MAX_TNAMES		16
#define DEVFREQ_MIF 		0
#define DEFAULT_POLLING_MS	100
#define DEFAULT_PID		0
#define DEFAULT_UTIL_THD	300

/* emstune */
#define EMSTUNE_NORMAL_MODE	0
#define EMSTUNE_HIGH_MODE	6
#define EMSTUNE_DEFAULT_LEVEL	0

#define for_each_cluster(cluster)			\
	for ((cluster) = 0; (cluster) < cl_cnt; (cluster)++)

#define for_each_cluster_rev(cluster)			\
	for ((cluster) = (cl_cnt - 1); (cluster) >= 0; (cluster)--)

struct freq {
	unsigned int freq;
	unsigned int volt;
	unsigned int dyn_power;
	unsigned int sta_power;
};

#define XPERF_TZ_NAME_LEN	(16)

static char tz_name[MAX_CLUSTER][XPERF_TZ_NAME_LEN];

struct cluster_info {
	int first_cpu;
	int last_cpu;
	int cl_idx;
	int coeff;

	struct cpumask *siblings;
	struct freq *freqs;

	unsigned int freq_size;
	unsigned int multi_idx;
};

struct xperf_cpu {
	struct cluster_info *cl_info;
	int cpu;
};

struct profile_info {
	struct task_struct *task;

	unsigned int power_coeffs[MAX_CLUSTER];
	unsigned int polling_ms;
	unsigned int pid;
	unsigned int util_thd;

	int is_running;

	char tns[MAX_TNAMES][TASK_COMM_LEN + 1];
};

extern int cl_cnt;
extern struct cluster_info cls[MAX_CLUSTER];

extern int xperf_prof_init(struct platform_device *pdev, struct kobject *kobj);
extern int xperf_core_init(struct kobject *kobj);
extern int xperf_memcpy_init(struct platform_device *pdev, struct kobject *kobj);
extern int xperf_gmc_init(struct platform_device *pdev, struct kobject *kobj);
extern int xperf_pago_init(struct platform_device *pdev, struct kobject *kobj);

extern int get_cl_idx(int cpu);
extern unsigned long ml_cpu_util(int cpu);
extern unsigned int et_cur_freq_idx(int cpu);

#if IS_ENABLED(CONFIG_EXYNOS_UFCC)
extern unsigned int get_cpufreq_max_limit(void);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_AFM)
extern unsigned int get_afm_clipped_freq(int cpu);
#endif

#if IS_ENABLED(CONFIG_DRM_SGPU_DVFS)
extern int gpu_dvfs_get_utilization(void);
extern int gpu_dvfs_get_cur_clock(void);
#endif
#endif /* EXYNOS_PERF_TNTERNAL_H */
