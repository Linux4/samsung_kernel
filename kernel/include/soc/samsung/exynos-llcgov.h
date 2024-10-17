#ifndef __EXYNOS_LLCGOV_H_
#define __EXYNOS_LLCGOV_H_

struct llc_gov_data {
	struct mutex			lock;
	struct delayed_work		get_noti_work;

	struct exynos_sci_data		*sci_data;
	struct devfreq			*devfreq;

	bool				llc_gov_en;
	bool				llc_req_flag;
	unsigned int			hfreq_rate;
	unsigned int			on_time_th;
	unsigned int			off_time_th;
	unsigned int			freq_th;

	u64				start_time;
	u64				last_time;
	u64				high_time;
	u64				enabled_time;
};

#if defined(CONFIG_EXYNOS_LLCGOV) || defined(CONFIG_EXYNOS_LLCGOV_MODULE)
int sci_register_llc_governor(struct exynos_sci_data *data);
#else
static inline int sci_register_llc_governor(struct exynos_sci_data *data) { return 0; }
#endif

#endif /* __EXYNOS_LLCGOV_H_ */
