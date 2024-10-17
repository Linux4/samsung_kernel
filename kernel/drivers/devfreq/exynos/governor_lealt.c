#define pr_fmt(fmt) "mem_lat: " fmt

#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/devfreq.h>
#include "../governor.h"
#include "governor_lealt.h"
#include <soc/samsung/exynos-devfreq.h>
#include <linux/cpufreq.h>
#include <soc/samsung/exynos-sci.h>

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif

#include <trace/events/power.h>
#define CREATE_TRACE_POINTS
#include <trace/events/governor_lealt_trace.h>
#define MAX_LOAD 84;

#define NR_SAMPLES 10

enum lealt_llc_control {
	LEALT_LLC_CONTROL_OFF = 0,
	LEALT_LLC_CONTROL_NONE,
	LEALT_LLC_CONTROL_ON,
};

struct lealt_sample {
	unsigned int seq_no;
	s64 duration; /* us */
	unsigned long mif_freq;
	unsigned long active_load;
	unsigned long target_load;
	unsigned long mif_active_freq;
	unsigned long cpu_active_freq[CONFIG_VENDOR_NR_CPUS];
	unsigned long base_minlock;
	unsigned long base_maxlock;

	unsigned long next_freq;
	enum lealt_llc_control next_llc;
};

struct gov_lealt_data {
	struct devfreq *df;

	spinlock_t work_control_lock;
	struct work_struct	work;
	bool work_started;
	unsigned long last_work_time;

	unsigned int polling_ms;
	unsigned int polling_ms_min;
	unsigned int polling_ms_max;
	struct notifier_block cpufreq_transition_nb;
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	struct notifier_block cpupm_nb;
#endif
	unsigned long expires;
	struct timer_list deferrable_timer[CONFIG_VENDOR_NR_CPUS];


	bool call_flag;
	unsigned long governor_freq;
	enum lealt_llc_control governor_llc;
	unsigned long governor_max;
	enum sysbusy_state sysbusy;
	int ems_mode;
	unsigned int global_prepare_freq;

	unsigned long *time_in_state_old;

	struct lealt_sample samples[NR_SAMPLES];
	int cur_idx;
	unsigned int cur_seq_no;
	s64 hold_time_freq;
	s64 hold_time_llc;

	unsigned long llc_on_th;
	unsigned long llc_off_th;

	unsigned long efficient_freq;
	unsigned long efficient_freq_th;

	unsigned int		*alt_target_load;
	unsigned int		alt_num_target_load;

	u32 trace_on;
};

struct cpufreq_stability {
	int domain_id;
	cpumask_t cpus;

	unsigned int stability;
	unsigned int stability_th;
	unsigned int freq_cur;
	unsigned int freq_high;
	unsigned int freq_low;
	struct list_head node;

	unsigned int prepare_cond;
	unsigned int prepare_freq;
};

static LIST_HEAD(cpufreq_stability_list);
static DEFINE_SPINLOCK(cpufreq_stability_list_lock);
enum cpufreq_stability_state {
	CPUFREQ_STABILITY_NORMAL,
	CPUFREQ_STABILITY_ALERT,
};

enum cpufreq_preprare_state {
	CPUFREQ_PREPARE_NONE,
	CPUFREQ_PREPARE_RELEASE,
	CPUFREQ_PREPARE_HOLD,
};

struct lealt_node {
	unsigned int ratio_ceil;
	unsigned int stall_floor;
	bool mon_started;
	struct list_head list;
	struct lealt_hwmon *hw;
	struct devfreq_governor *gov;
	unsigned long max_freq;
	unsigned long targetload;
	int lat_dev;

	unsigned int base_minlock_ratio;
	unsigned int base_maxlock_ratio;
	unsigned int llc_on_th_cpu;
	unsigned long raw_max_freq;
};


static struct workqueue_struct *lealt_wq;
static struct gov_lealt_data *gov_data_global;

static LIST_HEAD(lealt_list);
static DEFINE_MUTEX(list_lock);

static int lealt_use_cnt;
static DEFINE_MUTEX(state_lock);

static void init_cpufreq_stability_all(void);
static void lealt_set_polling_ms(struct gov_lealt_data *gov_data,
				 unsigned int new_ms);
int get_cpumask(struct lealt_hwmon *hw, struct cpumask *cpus);
int find_domain_id(struct cpumask *cpus);

#define show_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct lealt_node *node;				\
	unsigned int cnt = 0;	\
	mutex_lock(&list_lock);	\
	list_for_each_entry(node, &lealt_list, list) {	\
		if (node->hw->dev != df->dev.parent && \
		    node->hw->of_node != df->dev.parent->of_node)	\
			continue;\
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "%s: %s: %u\n",\
				dev_name(node->hw->dev), #name, node->name);\
	}	\
	mutex_unlock(&list_lock);	\
	return cnt;		\
}

#define store_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct lealt_node *node;				\
	int ret;							\
	char temp_buf[512];	\
	char *temp_buf_ptr = temp_buf;	\
	strcpy(temp_buf, buf);	\
	mutex_lock(&list_lock);	\
	list_for_each_entry(node, &lealt_list, list) {	\
		unsigned int val;						\
		char *cur_ptr;	\
		if (node->hw->dev != df->dev.parent && \
		    node->hw->of_node != df->dev.parent->of_node)	\
			continue;\
		cur_ptr = strsep(&temp_buf_ptr, ",");	\
		if (!cur_ptr) {						\
			mutex_unlock(&list_lock);			\
			return -EINVAL;					\
		}							\
		ret = kstrtouint(cur_ptr, 10, &val);			\
		if (ret) {						\
			mutex_unlock(&list_lock);			\
			return ret;					\
		}							\
		val = max(val, _min);					\
		val = min(val, _max);					\
		node->name = val;					\
	}							\
	mutex_unlock(&list_lock);	\
	return count;							\
}

#define show_stability_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct cpufreq_stability *stability;				\
	unsigned int cnt = 0;						\
	unsigned long flags; \
	spin_lock_irqsave(&cpufreq_stability_list_lock, flags);		\
	list_for_each_entry(stability, &cpufreq_stability_list, node) {	\
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, \
				 "domain_id: %d, %s: %u\n",\
				stability->domain_id,			\
				#name,				\
				 stability->name);		\
	}								\
	spin_unlock_irqrestore(&cpufreq_stability_list_lock, flags);	\
	return cnt;							\
}

#define store_stability_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	struct cpufreq_stability *stability;				\
	int ret;							\
	char temp_buf[512];	\
	char *temp_buf_ptr = temp_buf;	\
	unsigned long flags; \
	strcpy(temp_buf, buf);	\
	spin_lock_irqsave(&cpufreq_stability_list_lock, flags);		\
	list_for_each_entry(stability, &cpufreq_stability_list, node) {	\
		unsigned int val;						\
		char *cur_ptr;	\
		cur_ptr = strsep(&temp_buf_ptr, ",");	\
		if (!cur_ptr) {						\
			spin_unlock_irqrestore(				\
					&cpufreq_stability_list_lock,	\
					flags);				\
			return -EINVAL;					\
		}							\
		ret = kstrtouint(cur_ptr, 10, &val);			\
		if (ret) {						\
			spin_unlock_irqrestore(				\
					&cpufreq_stability_list_lock,	\
					flags);				\
			return ret;					\
		}							\
		val = max(val, _min);					\
		val = min(val, _max);					\
		stability->name = val;					\
	}							\
	spin_unlock_irqrestore(&cpufreq_stability_list_lock, flags);	\
	return count;							\
}

#define show_gov_data_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct gov_lealt_data *gov_data = df->governor_data;	\
	ssize_t count = 0; \
	mutex_lock(&df->lock);	\
	count += snprintf(buf + count, PAGE_SIZE, "%s: %lld\n",\
		#name, (long long)gov_data->name); \
	mutex_unlock(&df->lock);	\
	return count;		\
}

#define store_gov_data_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct gov_lealt_data *gov_data = df->governor_data;	\
	int ret;						\
	unsigned int val;	\
	mutex_lock(&df->lock);	\
	ret = sscanf(buf, "%u", &val);	\
	if (ret != 1) {							\
		mutex_unlock(&df->lock);				\
		return -EINVAL;	\
	}								\
	val = max(val, _min);					\
	val = min(val, _max);					\
	gov_data->name = val;	\
	mutex_unlock(&df->lock);	\
	return count;			\
}

static ssize_t freq_map_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	unsigned int cnt = 0;
	struct lealt_node *node;

	cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt,
				"Core freq (MHz)\t TargetLoad\n");
	mutex_lock(&list_lock);
	list_for_each_entry(node, &lealt_list, list) {
		struct core_dev_map *map = node->hw->freq_map;
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt,
				"dev_name: %s\n", dev_name(node->hw->dev));
		while (map->core_mhz && cnt < PAGE_SIZE) {
			cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "%15u\t%9u\n",
					map->core_mhz, map->target_load);
			map++;
		}
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	}
	mutex_unlock(&list_lock);
	return cnt;
}

static ssize_t freq_map_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct lealt_node *node;
	int ret, core_mhz, target_load;
	char temp_buf[100];

	ret = sscanf(buf, "%s %d %d", temp_buf, &core_mhz, &target_load);
	if (ret != 3)
		return -EINVAL;

	mutex_lock(&list_lock);
	list_for_each_entry(node, &lealt_list, list) {
		struct core_dev_map *map = node->hw->freq_map;
		if (strcmp(dev_name(node->hw->dev), temp_buf))
			continue;
		while (map->core_mhz) {
			if (core_mhz == map->core_mhz) {
				map->target_load = target_load;
				break;
			}
			map++;
		}
	}
	mutex_unlock(&list_lock);
	return count;
}

static DEVICE_ATTR_RW(freq_map);

/* Show Current ALT Parameter Info */
static ssize_t alt_target_load_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct gov_lealt_data *gov_data = df->governor_data;
	ssize_t count = 0;
	int i;

	mutex_lock(&df->lock);	\
	for (i = 0; i < gov_data->alt_num_target_load; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "%d%s",
				  gov_data->alt_target_load[i],
				  (i == gov_data->alt_num_target_load - 1) ?
				  "" : (i % 2) ? ":" : " ");
	}
	count += snprintf(buf + count, PAGE_SIZE, "\n");
	mutex_unlock(&df->lock);
	return count;
}
static DEVICE_ATTR_RO(alt_target_load);

static unsigned long core_to_targetload(struct lealt_node *node,
		unsigned long coref)
{
	struct lealt_hwmon *hw = node->hw;
	struct core_dev_map *map = hw->freq_map;
	unsigned long freq = 0;

	if (!map)
		goto out;

	while (map->core_mhz && map->core_mhz < coref)
		map++;
	if (!map->core_mhz)
		map--;
	freq = map->target_load;

out:
	pr_debug("freq: %lu -> dev: %lu\n", coref, freq);
	return freq;
}

static int start_monitor(struct lealt_node *node)
{
	struct lealt_hwmon *hw = node->hw;
	struct device *dev = node->hw->df->dev.parent;
	int ret;

	ret = hw->start_hwmon(hw);

	if (ret) {
		dev_err(dev, "Unable to start HW monitor! (%d)\n", ret);
		return ret;
	}

	node->mon_started = true;

	return 0;
}

static void stop_monitor(struct lealt_node *node)
{
	struct lealt_hwmon *hw = node->hw;

	node->mon_started = false;

	hw->stop_hwmon(hw);
}


static struct lealt_sample *lealt_get_cur_sample(struct gov_lealt_data *gov_data)
{
	struct lealt_sample *sample = &gov_data->samples[gov_data->cur_idx++];

	// Circular index
	gov_data->cur_idx = gov_data->cur_idx < NR_SAMPLES ?
				 gov_data->cur_idx:
				 0;

	// Initialize sample
	memset(sample, 0, sizeof(struct lealt_sample));
	return sample;
}

static int lealt_get_prev_idx(int index)
{
	// Circular index
	 return index - 1 > 0 ? index - 1 : NR_SAMPLES - 1;
}

static int lealt_backtrace_samples(
				struct gov_lealt_data *gov_data,
				unsigned long *final_freq,
				enum lealt_llc_control *final_llc)
{

	unsigned long sum_duration = 0;
	int idx = gov_data->cur_idx;
	unsigned long max_freq = 0;
	int consider_cnt_freq = 0, consider_cnt_llc = 0;
	enum lealt_llc_control max_llc = LEALT_LLC_CONTROL_OFF;
	int i = 0;
	for (i = 0; i < NR_SAMPLES; i++) {
		struct lealt_sample *sample;
		idx = lealt_get_prev_idx(idx);
		sample = &gov_data->samples[idx];
		sum_duration += sample->duration;
		if (consider_cnt_freq == 0 ||
				sum_duration <= gov_data->hold_time_freq) {
			max_freq = sample->next_freq > max_freq ?
				sample->next_freq :
				max_freq;
			consider_cnt_freq++;
		}
		if (consider_cnt_llc == 0 ||
				sum_duration <= gov_data->hold_time_llc) {
			max_llc = sample->next_llc > max_llc ?
				sample->next_llc :
				max_llc;
			consider_cnt_llc++;
		}
		if (gov_data->trace_on)
			trace_lealt_sample_backtrace(sample->seq_no,
						sample->duration,
						sample->next_freq,
						sample->next_llc);
		if (sum_duration > gov_data->hold_time_freq
			&& sum_duration > gov_data->hold_time_llc)
			break;
	}
	*final_freq = max_freq;
	*final_llc = max_llc;
	return 0;
}

static int lealt_get_base_lock(unsigned long *out_base_minlock,
				 unsigned long *out_base_maxlock)
{
	unsigned long base_minlock = 0;
	unsigned long base_maxlock = 0;
	struct lealt_node *node;
	list_for_each_entry(node, &lealt_list, list) {
		unsigned long cur_base_minlock = node->raw_max_freq
					* node->base_minlock_ratio / 100UL
					* 1000UL;
		unsigned long cur_base_maxlock = node->raw_max_freq
					* node->base_maxlock_ratio / 100UL
					* 1000UL;
		base_minlock = cur_base_minlock > base_minlock ?
			cur_base_minlock : base_minlock;
		base_maxlock = cur_base_maxlock > base_maxlock ?
			cur_base_maxlock : base_maxlock;
	}

	*out_base_minlock = base_minlock;
	*out_base_maxlock = base_maxlock;
	return 0;
}

static enum lealt_llc_control lealt_decide_next_llc(
				struct gov_lealt_data *gov_data,
				unsigned long mif_active_freq)
{
	struct lealt_node *node;
	mutex_lock(&list_lock);
	list_for_each_entry(node, &lealt_list, list) {
		unsigned int llc_on_th_cpu = node->llc_on_th_cpu / 1000U;
		if (llc_on_th_cpu && node->raw_max_freq > llc_on_th_cpu) {
			mutex_unlock(&list_lock);
			return LEALT_LLC_CONTROL_ON;
		}
	}
	mutex_unlock(&list_lock);
	if (gov_data->llc_on_th && mif_active_freq > gov_data->llc_on_th) {
		return LEALT_LLC_CONTROL_ON;
	} else if (mif_active_freq < gov_data->llc_off_th) {
		return LEALT_LLC_CONTROL_OFF;
	}
	return LEALT_LLC_CONTROL_NONE;
}
static int lealt_control_llc(
			struct gov_lealt_data *gov_data,
			enum lealt_llc_control llc_control)
{
	if (llc_control == LEALT_LLC_CONTROL_NONE)
		return 0;

	if (gov_data->governor_llc != llc_control) {
		gov_data->governor_llc = llc_control;
		switch(llc_control) {
		case LEALT_LLC_CONTROL_OFF:
			llc_region_alloc(LLC_REGION_CPU, 0, 0);
			break;
		case LEALT_LLC_CONTROL_ON:
			llc_region_alloc(LLC_REGION_CPU, 1, LLC_WAY_MAX);
			break;
		default:
			break;
		}
	}
	return 0;
}
static unsigned long devfreq_lealt_next_freq(unsigned long base_minlock,
					     unsigned long base_maxlock,
					     unsigned long cur_freq,
					     unsigned long cur_load,
					     unsigned long targetload)
{
	unsigned long next_freq = 0;

	if (targetload)
		next_freq = cur_load * cur_freq / targetload;
	next_freq = next_freq < base_minlock ? base_minlock : next_freq;
	next_freq = next_freq < base_maxlock ? next_freq : base_maxlock;
	return next_freq;
}

static unsigned int lealt_apply_prepare_minlock(unsigned int global_prepare_freq,
						unsigned int governor_freq)
{
	unsigned int ret = global_prepare_freq > governor_freq ?
		global_prepare_freq : governor_freq;

	return ret;
}

static enum cpufreq_preprare_state lealt_update_prepare_holding(
						struct gov_lealt_data *gov_data)
{
	struct cpufreq_stability *stability;
	unsigned int global_prepare_freq_old = gov_data->global_prepare_freq;


	gov_data->global_prepare_freq = 0;
	list_for_each_entry(stability, &cpufreq_stability_list, node) {
		if (stability->freq_cur < stability->prepare_cond)
			continue;

		/* get max */
		gov_data->global_prepare_freq =
			gov_data->global_prepare_freq > stability->prepare_freq
			? gov_data->global_prepare_freq
			: stability->prepare_freq;
		/* get max */
	}

	if (gov_data->global_prepare_freq > global_prepare_freq_old)
		return CPUFREQ_PREPARE_HOLD;
	return CPUFREQ_PREPARE_NONE;
}

static u64 lealt_get_avg_freq(struct exynos_devfreq_data *data,
			struct gov_lealt_data *gov_data)
{
	int i;
	u64 time_sum = 0;
	u64 product_sum = 0;
	mutex_lock(&data->lock);
	exynos_devfreq_update_status_internal(data);
	for (i = 0; i < data->max_state; i++) {
		u64 time_diff = data->time_in_state[i]
				- gov_data->time_in_state_old[i];
		time_sum += time_diff;
		product_sum += (data->devfreq_profile.freq_table[i] * time_diff);
		gov_data->time_in_state_old[i] = data->time_in_state[i];
	}
	mutex_unlock(&data->lock);
	if (!time_sum)
		return data->old_freq;
	return product_sum / time_sum;
}

int exynos_devfreq_get_recommended_freq(struct exynos_devfreq_data *data,
					unsigned long *target_freq, u32 flags);
static int devfreq_lealt_get_freq(struct devfreq *df,
					unsigned long *freq)
{
	struct devfreq_dev_status *stat = &df->last_status;
	struct exynos_devfreq_profile_data *profile_data =
		(struct exynos_devfreq_profile_data *)stat->private_data;
	struct gov_lealt_data *gov_data = df->governor_data;
	struct exynos_devfreq_data *data = df->data;
	struct lealt_node *node, *latency_node = NULL;
	int ret;
	unsigned long flags;
	struct lealt_sample *sample;
	enum lealt_llc_control llc_control = LEALT_LLC_CONTROL_OFF;
	int ems_mode = gov_data->ems_mode, i = 0;

	if(gov_data->sysbusy > SYSBUSY_STATE0) {
		gov_data->governor_freq = data->max_freq;
		llc_control = LEALT_LLC_CONTROL_ON;
		goto llc_out;
	} else if (!gov_data->call_flag) {
		goto freq_out;
	}

	ret = devfreq_update_stats(df);
	if (ret) {
		dev_err(&df->dev, "devfreq_update_stats error (ret: %d)\n",
			ret);
		goto freq_out;
	}

	sample = lealt_get_cur_sample(gov_data);
	sample->seq_no = gov_data->cur_seq_no++;
	sample->duration = ktime_to_us(profile_data->duration);
	sample->mif_freq = lealt_get_avg_freq(data, gov_data);

	sample->target_load = MAX_LOAD;

	mutex_lock(&list_lock);
	list_for_each_entry(node, &lealt_list, list) {
		unsigned int ratio;
		int i;
		struct lealt_hwmon *hw = node->hw;

		node->lat_dev = 0;
		node->max_freq = 0;
		node->raw_max_freq = 0;
		hw->get_cnt(hw);
		for (i = 0; i < hw->num_cores; i++) {
			ratio = hw->core_stats[i].inst_count;

			if (hw->core_stats[i].mem_count)
				ratio /= hw->core_stats[i].mem_count;

			if (gov_data->trace_on) {
				char efreq_str[32], ratio_str[32];
				sprintf(efreq_str, "LEALT: EFREQ_%d",
						hw->core_stats[i].id);
				sprintf(ratio_str, "LEALT: RATIO_%d",
						hw->core_stats[i].id);
				trace_clock_set_rate(efreq_str,
							hw->core_stats[i].freq,
							raw_smp_processor_id());
				trace_clock_set_rate(ratio_str,
							ratio,
							raw_smp_processor_id());
			}

			if (!hw->core_stats[i].freq)
				continue;

			/*trace_lealt_governor_pmu_each(hw->core_stats[i].id,
				       hw->core_stats[i].freq,
				       ratio,
				       hw->core_stats[i].stall_pct); */
			node->raw_max_freq =
				hw->core_stats[i].freq > node->raw_max_freq ?
				hw->core_stats[i].freq : node->raw_max_freq;

			if (ratio <= node->ratio_ceil
			    //&& hw->core_stats[i].stall_pct >= node->stall_floor
			    && hw->core_stats[i].freq > node->max_freq) {
				node->lat_dev = i;
				node->max_freq = hw->core_stats[i].freq;
			}
		}

		if (node->max_freq) {
			node->targetload =
				core_to_targetload(node, node->max_freq);
			if (node->targetload < sample->target_load) {
				sample->target_load = node->targetload;
				latency_node = node;
			}
		}
	}
	lealt_get_base_lock(&sample->base_minlock, &sample->base_maxlock);
	mutex_unlock(&list_lock);

	/* operate like ALT */
	if(ems_mode > 0) {
		for (i = 0; i < gov_data->alt_num_target_load - 1 &&
		     sample->mif_freq >= gov_data->alt_target_load[i + 1]; i += 2);
		sample->target_load = gov_data->alt_target_load[i];
		sample->base_maxlock = gov_data->alt_target_load[gov_data->alt_num_target_load - 2];
		sample->base_minlock = 0;
	}

	if (gov_data->trace_on && latency_node != NULL) {
		trace_lealt_governor_latency_dev(
			latency_node->hw->core_stats[latency_node->lat_dev].id,
			latency_node->max_freq,
			latency_node->targetload);
	}

#if IS_ENABLED(CONFIG_LEALT_MULTI_PATH)
	if (profile_data->wow->total_time_aggregated)
		sample->active_load = profile_data->wow->busy_time_aggregated
			* 100UL / profile_data->wow->total_time_aggregated;
#else
	if (profile_data->wow->total_time_cpu_path)
		sample->active_load = profile_data->wow->busy_time_cpu_path
			* 100UL / profile_data->wow->total_time_cpu_path;
#endif

	sample->mif_active_freq = sample->mif_freq * sample->active_load / 100UL;

	sample->next_freq = devfreq_lealt_next_freq(sample->base_minlock,
						sample->base_maxlock,
						sample->mif_freq,
						sample->active_load,
						sample->target_load);

	sample->next_llc =
		lealt_decide_next_llc(gov_data, sample->mif_active_freq);

	if(ems_mode > 0) {
		sample->next_llc = LEALT_LLC_CONTROL_OFF;
	}

	if (gov_data->trace_on)
		trace_lealt_sample_cur(sample->seq_no, sample->duration,
				sample->next_freq, sample->next_llc,
				sample->mif_freq, sample->active_load,
				sample->target_load, sample->mif_active_freq,
				sample->base_minlock,
				sample->base_maxlock);

	lealt_backtrace_samples(gov_data, &gov_data->governor_freq, &llc_control);

	if (sample->mif_active_freq >= gov_data->efficient_freq_th) {
		gov_data->governor_freq =
			gov_data->governor_freq < gov_data->efficient_freq ?
			gov_data->efficient_freq :
			gov_data->governor_freq;
	}

	gov_data->governor_freq =
			gov_data->governor_freq > gov_data->governor_max ?
			gov_data->governor_max :
			gov_data->governor_freq;

	if (gov_data->trace_on)
		trace_clock_set_rate("LEALT: ActiveLoad",
			sample->active_load,
			raw_smp_processor_id());
	if (gov_data->trace_on)
		trace_clock_set_rate("LEALT: ActiveFreq",
			sample->mif_active_freq,
			raw_smp_processor_id());
llc_out:
	lealt_control_llc(gov_data, llc_control);
	if (gov_data->trace_on)
		trace_clock_set_rate("LEALT: GovernorLLC",
			gov_data->governor_llc,
			raw_smp_processor_id());

freq_out:
	if (gov_data->trace_on)
		trace_clock_set_rate("LEALT: GovernorFreq",
			gov_data->governor_freq,
			raw_smp_processor_id());

	if (ems_mode == 0)
		gov_data->governor_freq =
			exynos_devfreq_freq_mapping(data,
						    gov_data->governor_freq);
	else
		exynos_devfreq_get_recommended_freq(data,
						    &gov_data->governor_freq, 0);

	if (gov_data->governor_llc == LEALT_LLC_CONTROL_ON)
		gov_data->governor_freq =
			gov_data->governor_freq < 845000 ?
			845000 :
			gov_data->governor_freq;

	spin_lock_irqsave(&cpufreq_stability_list_lock, flags);
	gov_data->governor_freq =
		lealt_apply_prepare_minlock(gov_data->global_prepare_freq,
					    gov_data->governor_freq);
	spin_unlock_irqrestore(&cpufreq_stability_list_lock, flags);


	*freq = exynos_devfreq_policy_update(df->data, gov_data->governor_freq);
	return 0;
}

show_attr(ratio_ceil)
store_attr(ratio_ceil, 1U, 20000U)
static DEVICE_ATTR(ratio_ceil, 0644, show_ratio_ceil, store_ratio_ceil);

show_attr(stall_floor)
store_attr(stall_floor, 0U, 100U)
static DEVICE_ATTR(stall_floor, 0644, show_stall_floor, store_stall_floor);

show_attr(base_minlock_ratio)
store_attr(base_minlock_ratio, 0U, 1000U)
static DEVICE_ATTR(base_minlock_ratio, 0644, show_base_minlock_ratio,
						 store_base_minlock_ratio);

show_attr(base_maxlock_ratio)
store_attr(base_maxlock_ratio, 0U, 1000U)
static DEVICE_ATTR(base_maxlock_ratio, 0644, show_base_maxlock_ratio,
						 store_base_maxlock_ratio);

show_attr(llc_on_th_cpu)
store_attr(llc_on_th_cpu, 0U, 3168000U)
static DEVICE_ATTR(llc_on_th_cpu, 0644, show_llc_on_th_cpu, store_llc_on_th_cpu);

show_stability_attr(stability_th)
store_stability_attr(stability_th, 0U, 100U)
static DEVICE_ATTR(stability_th, 0644, show_stability_th, store_stability_th);

show_stability_attr(prepare_cond)
store_stability_attr(prepare_cond, 0U, 2400000U)
static DEVICE_ATTR(prepare_cond, 0644, show_prepare_cond, store_prepare_cond);

show_stability_attr(prepare_freq)
store_stability_attr(prepare_freq, 0U, 4206000U)
static DEVICE_ATTR(prepare_freq, 0644, show_prepare_freq, store_prepare_freq);

show_gov_data_attr(governor_max)
store_gov_data_attr(governor_max, 421000U, 4206000U)
static DEVICE_ATTR(governor_max, 0644, show_governor_max, store_governor_max);

show_gov_data_attr(hold_time_freq)
store_gov_data_attr(hold_time_freq, 1U, 200000U)
static DEVICE_ATTR(hold_time_freq, 0644, show_hold_time_freq, store_hold_time_freq);

show_gov_data_attr(hold_time_llc)
store_gov_data_attr(hold_time_llc, 1U, 200000U)
static DEVICE_ATTR(hold_time_llc, 0644, show_hold_time_llc, store_hold_time_llc);

show_gov_data_attr(llc_on_th)
store_gov_data_attr(llc_on_th, 0U, 4206000U)
static DEVICE_ATTR(llc_on_th, 0644, show_llc_on_th, store_llc_on_th);

show_gov_data_attr(llc_off_th)
store_gov_data_attr(llc_off_th, 0U, 4206000U)
static DEVICE_ATTR(llc_off_th, 0644, show_llc_off_th, store_llc_off_th);

show_gov_data_attr(polling_ms_min)
store_gov_data_attr(polling_ms_min, 4U, 1024U)
static DEVICE_ATTR(polling_ms_min, 0644,
	show_polling_ms_min, store_polling_ms_min);

show_gov_data_attr(polling_ms_max)
store_gov_data_attr(polling_ms_max, 4U, 1024U)
static DEVICE_ATTR(polling_ms_max, 0644,
	show_polling_ms_max, store_polling_ms_max);

show_gov_data_attr(efficient_freq_th)
store_gov_data_attr(efficient_freq_th, 0U, 4206000U)
static DEVICE_ATTR(efficient_freq_th, 0644,
	show_efficient_freq_th, store_efficient_freq_th);

show_gov_data_attr(efficient_freq)
store_gov_data_attr(efficient_freq, 0U, 4206000U)
static DEVICE_ATTR(efficient_freq, 0644,
	show_efficient_freq, store_efficient_freq);

show_gov_data_attr(trace_on)
store_gov_data_attr(trace_on, 0U, 1U)
static DEVICE_ATTR(trace_on, 0644,
	show_trace_on, store_trace_on);

static struct attribute *lealt_dev_attr[] = {
	&dev_attr_ratio_ceil.attr,
	&dev_attr_stall_floor.attr,
	&dev_attr_base_minlock_ratio.attr,
	&dev_attr_base_maxlock_ratio.attr,
	&dev_attr_llc_on_th_cpu.attr,
	&dev_attr_freq_map.attr,
	&dev_attr_alt_target_load.attr,
	&dev_attr_stability_th.attr,
	&dev_attr_prepare_cond.attr,
	&dev_attr_prepare_freq.attr,
	&dev_attr_polling_ms_min.attr,
	&dev_attr_polling_ms_max.attr,
	&dev_attr_governor_max.attr,
	&dev_attr_hold_time_freq.attr,
	&dev_attr_hold_time_llc.attr,
	&dev_attr_llc_on_th.attr,
	&dev_attr_llc_off_th.attr,
	&dev_attr_efficient_freq_th.attr,
	&dev_attr_efficient_freq.attr,
	&dev_attr_trace_on.attr,
	NULL,
};
/*
static struct attribute *compute_dev_attr[] = {
	&dev_attr_freq_map.attr,
	NULL,
};
*/
static struct attribute_group lealt_dev_attr_group = {
	.name = "lealt",
	.attrs = lealt_dev_attr,
};
/*
static struct attribute_group compute_dev_attr_group = {
	.name = "compute",
	.attrs = compute_dev_attr,
}; */

static int gov_lealt_sysbusy_callback(struct devfreq *df,
		enum sysbusy_state state)
{
	unsigned long flags;
	struct gov_lealt_data *gov_data = df->governor_data;

	gov_data->sysbusy = state;
	if(gov_data->trace_on)
		trace_clock_set_rate("LEALT: sysbusy",
			state,
			raw_smp_processor_id());

	if (gov_data->sysbusy > SYSBUSY_STATE0) {
		spin_lock_irqsave(&gov_data->work_control_lock, flags);
		gov_data->work_started = false;
		spin_unlock_irqrestore(&gov_data->work_control_lock, flags);
	} else {
		spin_lock_irqsave(&cpufreq_stability_list_lock, flags);
		init_cpufreq_stability_all();
		spin_unlock_irqrestore(&cpufreq_stability_list_lock, flags);

		spin_lock_irqsave(&gov_data->work_control_lock, flags);
		gov_data->work_started = true;
		lealt_set_polling_ms(gov_data, gov_data->polling_ms_max);
		spin_unlock_irqrestore(&gov_data->work_control_lock, flags);
	}

	return 0;
}

static int gov_lealt_emstune_callback(struct devfreq *df, int mode)
{
	struct gov_lealt_data *gov_data = df->governor_data;

	WRITE_ONCE(gov_data->ems_mode, mode);
	if(gov_data->trace_on)
		trace_clock_set_rate("LEALT: ems_mode",
			mode,
			raw_smp_processor_id());

	return 0;
}

void update_counts_all(void);
static void lealt_monitor_work(struct work_struct *work)
{
	struct gov_lealt_data *gov_data =
		container_of(work, struct gov_lealt_data, work);
	struct devfreq *df = gov_data->df;
	int err;
	unsigned long flags;

	if (gov_data->trace_on)
		trace_clock_set_rate("LEALT: monitor_work_on",
			1,
			raw_smp_processor_id());

	spin_lock_irqsave(&cpufreq_stability_list_lock, flags);
	init_cpufreq_stability_all();
	spin_unlock_irqrestore(&cpufreq_stability_list_lock, flags);

	update_counts_all();

	mutex_lock(&df->lock);
	WRITE_ONCE(gov_data->call_flag, true);
	err = update_devfreq(df);
	if (err)
		dev_err(&df->dev, "%s: update_devfreq failed: %d\n",
			__func__, err);
	WRITE_ONCE(gov_data->call_flag, false);
	mutex_unlock(&df->lock);

	if (gov_data->trace_on)
		trace_clock_set_rate("LEALT: monitor_work_on",
			0,
			raw_smp_processor_id());
}

static enum cpufreq_stability_state update_cpufreq_stability(
		struct cpufreq_stability *stability, unsigned int new_freq)
{
	int ret;

	stability->freq_cur = new_freq;

	if (new_freq > stability->freq_high)
		stability->freq_high = new_freq;
	if (new_freq < stability->freq_low)
		stability->freq_low = new_freq;

	stability->stability = 100 -
		(100 * (stability->freq_high - stability->freq_low))
		/ stability->freq_high;

	ret = stability->stability > stability->stability_th ?
		CPUFREQ_STABILITY_NORMAL :
		CPUFREQ_STABILITY_ALERT;

	return ret;
}

static void init_cpufreq_stability(struct cpufreq_stability *stability,
				    unsigned int initial_freq)
{
	stability->freq_cur = initial_freq;
	stability->freq_high = initial_freq;
	stability->freq_low = initial_freq;
	stability->stability = 100;
}

static void init_cpufreq_stability_all(void)
{
	struct cpufreq_stability *stability;

	list_for_each_entry(stability, &cpufreq_stability_list, node) {
		init_cpufreq_stability(stability, stability->freq_cur);
	}
}

static void lealt_set_polling_ms(struct gov_lealt_data *gov_data,
				 unsigned int new_ms)
{
	unsigned long now = jiffies;
	unsigned long target_next = gov_data->last_work_time +
		msecs_to_jiffies(gov_data->polling_ms_min);

	/* capped by min max */
	new_ms = new_ms < gov_data->polling_ms_min ?
		gov_data->polling_ms_min : new_ms;

	new_ms = new_ms > gov_data->polling_ms_max ?
		gov_data->polling_ms_max : new_ms;

	gov_data->polling_ms = new_ms;

	/* Get max */
	target_next = target_next > now ? target_next : now;

	/* Pull-in next expires */
	if (target_next < gov_data->expires) {
		gov_data->expires = target_next;
	}

	return;
}


static int cpufreq_transition_notifier(struct notifier_block *nb,
					  unsigned long val, void *data)
{
	struct gov_lealt_data *gov_data =
		container_of(nb, struct gov_lealt_data, cpufreq_transition_nb);
	unsigned int domain_id = *(unsigned int *)data;
	unsigned int new_freq = val;
	struct cpufreq_stability *stability;
	enum cpufreq_stability_state stability_ret = CPUFREQ_STABILITY_NORMAL;
	enum cpufreq_preprare_state prepare_ret = CPUFREQ_PREPARE_NONE;
	unsigned long flags;

	spin_lock(&cpufreq_stability_list_lock);
	list_for_each_entry(stability, &cpufreq_stability_list, node) {
		if (stability->domain_id == domain_id) {
			break;
		}
	}

	if (list_entry_is_head(stability, &cpufreq_stability_list, node)) {
		spin_unlock(&cpufreq_stability_list_lock);
		return NOTIFY_OK;
	}

	/* Update */
	stability_ret = update_cpufreq_stability(stability, new_freq);
	prepare_ret = lealt_update_prepare_holding(gov_data);
	spin_unlock(&cpufreq_stability_list_lock);

	if (stability_ret == CPUFREQ_STABILITY_ALERT
	    || prepare_ret > CPUFREQ_PREPARE_NONE) {
		spin_lock_irqsave(&gov_data->work_control_lock, flags);
		lealt_set_polling_ms(gov_data, gov_data->polling_ms_min);
		spin_unlock_irqrestore(&gov_data->work_control_lock, flags);
		if (gov_data->trace_on && prepare_ret > CPUFREQ_PREPARE_NONE)
			trace_lealt_alert_prepare(
				prepare_ret,
				gov_data->global_prepare_freq,
				stability->domain_id,
				stability->freq_cur);
		if (gov_data->trace_on &&
			stability_ret == CPUFREQ_STABILITY_ALERT)
			trace_lealt_alert_stability(
				stability_ret,
				stability->stability,
				stability->domain_id,
				stability->freq_cur,
				stability->freq_high,
				stability->freq_low);
	}


	return NOTIFY_OK;
}

/* get frequency and delay time data from string */
static unsigned int *get_tokenized_data(const char *buf, int *num_tokens)
{
	const char *cp;
	int i;
	int ntokens = 1;
	unsigned int *tokenized_data;
	int err = -EINVAL;

	cp = buf;
	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	if (!(ntokens & 0x1))
		goto err;

	tokenized_data = kmalloc(ntokens * sizeof(unsigned int), GFP_KERNEL);
	if (!tokenized_data) {
		err = -ENOMEM;
		goto err;
	}

	cp = buf;
	i = 0;
	while (i < ntokens) {
		if (sscanf(cp, "%u", &tokenized_data[i++]) != 1)
			goto err_kfree;

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	*num_tokens = ntokens;
	return tokenized_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

void register_get_dev_status(struct exynos_devfreq_data *data,
			     struct devfreq *devfreq,
			     struct device_node *get_dev_np);
static int gov_parse_dt(struct exynos_devfreq_data *data,
			struct devfreq *devfreq,
			struct gov_lealt_data *gov_data)
{
	struct device_node *gov_np, *df_np = data->dev->of_node;
	struct device_node *get_dev_np;
	u32 polling_minmax[2];
	u32 val;
	const char *buf;
	gov_np = of_find_node_by_name(df_np, "governor_data");
	if (!gov_np) {
		dev_err(data->dev, "There is no get_dev node\n");
		return -EINVAL;
	}

	get_dev_np = of_find_node_by_name(gov_np, "get_dev");
	if (!get_dev_np) {
		dev_err(data->dev, "There is no get_dev node\n");
		return -EINVAL;
	}
	register_get_dev_status(data, devfreq, get_dev_np);

	if (!of_property_read_u32_array(gov_np, "polling_ms",
		(u32 *)&polling_minmax, (size_t)(ARRAY_SIZE(polling_minmax)))) {
		gov_data->polling_ms_min = polling_minmax[0];
		gov_data->polling_ms_max = polling_minmax[1];
		gov_data->polling_ms = gov_data->polling_ms_min;
	}

	if (!of_property_read_u32(gov_np, "governor_max",
					 &val)) {
		gov_data->governor_max = val;
	} else {
		dev_err(data->dev, "There is no governor_max");
	}

	if (!of_property_read_u32(gov_np, "hold_time_freq",
					 &val)) {
		gov_data->hold_time_freq = val;
	} else {
		dev_err(data->dev, "There is no hold_time_freq");
	}

	if (!of_property_read_u32(gov_np, "hold_time_llc",
					 &val)) {
		gov_data->hold_time_llc = val;
	} else {
		dev_err(data->dev, "There is no hold_time_llc");
	}

	if (!of_property_read_u32(gov_np, "llc_on_th",
					 &val)) {
		gov_data->llc_on_th = val;
	} else {
		dev_err(data->dev, "There is no llc_on_th");
	}

	if (!of_property_read_u32(gov_np, "llc_off_th",
					 &val)) {
		gov_data->llc_off_th = val;
	} else {
		dev_err(data->dev, "There is no llc_off_th");
	}

	if (!of_property_read_u32(gov_np, "efficient_freq",
					 &val)) {
		gov_data->efficient_freq = val;
	} else {
		dev_err(data->dev, "There is no efficient_freq");
	}

	if (!of_property_read_u32(gov_np, "efficient_freq_th",
					 &val)) {
		gov_data->efficient_freq_th = val;
	} else {
		dev_err(data->dev, "There is no efficient_freq_th");
	}

	if (!of_property_read_string(gov_np, "target_load", &buf)) {
		/* Parse target load table */
		int ntokens;
		gov_data->alt_target_load = get_tokenized_data(buf, &ntokens);
		gov_data->alt_num_target_load = ntokens;
	}

	return 0;
}
static int lealt_cpupm_notifier(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	struct gov_lealt_data *gov_data =
		container_of(self, struct gov_lealt_data, cpupm_nb);
	switch (cmd) {
	case DSUPD_ENTER:
	case SICD_ENTER:
		if(gov_data->trace_on)
			trace_clock_set_rate("LEALT: SICD on",
				1,
				raw_smp_processor_id());
		break;
	case DSUPD_EXIT:
	case SICD_EXIT:
		if(gov_data->trace_on)
			trace_clock_set_rate("LEALT: SICD on",
				0,
				raw_smp_processor_id());
		break;
	}
	return NOTIFY_DONE;
}

static void lealt_deferrable_timer_handler(struct timer_list *timer)
{
	int cpu = raw_smp_processor_id();
	struct gov_lealt_data *gov_data =
		container_of(timer, struct gov_lealt_data, deferrable_timer[cpu]);
	unsigned long now = jiffies;
	unsigned long flags;
	int ret;

	ret = spin_trylock_irqsave(&gov_data->work_control_lock, flags);
	if (!ret)
		goto out;

	if (now < gov_data->expires)
		goto unlock_out;

	if (!gov_data->work_started)
		goto unlock_out;

	queue_work_on(cpu, lealt_wq, &gov_data->work);
	//trace_lealt_queue_work(gov_data->polling_ms);

	gov_data->expires = now +
		msecs_to_jiffies(gov_data->polling_ms);
	gov_data->last_work_time = now;
	if(gov_data->trace_on)
		trace_lealt_queue_work(gov_data->polling_ms);
	/* double polling ms */
	gov_data->polling_ms = gov_data->polling_ms << 1;
	if (gov_data->polling_ms > gov_data->polling_ms_max)
		gov_data->polling_ms = gov_data->polling_ms_max;
unlock_out:
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);
out:
	/* Re-arm deferrable timer */
	mod_timer(timer, now + msecs_to_jiffies(10));
}

static int gov_monitor_initialize(struct gov_lealt_data *gov_data)
{
	int cpu;
	if (!lealt_wq)
		lealt_wq = alloc_workqueue("lealt_wq",
		 __WQ_LEGACY | WQ_FREEZABLE | WQ_MEM_RECLAIM, 1);

	if (!lealt_wq) {
		dev_err(&gov_data->df->dev, "Couldn't create lealt workqueue.\n");
		return -ENOMEM;
	}

	INIT_WORK(&gov_data->work, &lealt_monitor_work);
	spin_lock_init(&gov_data->work_control_lock);
	gov_data->work_started = true;

	for_each_possible_cpu(cpu) {
		if(cpu > 3)
			break;
		timer_setup(&gov_data->deferrable_timer[cpu],
			lealt_deferrable_timer_handler,
			TIMER_DEFERRABLE | TIMER_PINNED);
		gov_data->deferrable_timer[cpu].expires = jiffies +
			msecs_to_jiffies(10);
		add_timer_on(&gov_data->deferrable_timer[cpu], cpu);
	}
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	gov_data->cpupm_nb.notifier_call = lealt_cpupm_notifier;
	exynos_cpupm_notifier_register(&gov_data->cpupm_nb);
#endif
	dev_info(&gov_data->df->dev,"LEALT work initialized with period : %u\n",
		 gov_data->polling_ms);
	return 0;
}
static int lealt_cpuhp_up(unsigned int cpu)
{
	if(cpu > 3)
		return 0;
	gov_data_global->deferrable_timer[cpu].expires =
		jiffies + msecs_to_jiffies(10);
	add_timer_on(&gov_data_global->deferrable_timer[cpu], cpu);
	return 0;
}
static int lealt_cpuhp_down(unsigned int cpu)
{
	if(cpu > 3)
		return 0;
	del_timer_sync(&gov_data_global->deferrable_timer[cpu]);
	return 0;
}

int exynos_cpufreq_register_transition_notifier(struct notifier_block *nb);
static int gov_start(struct devfreq *df)
{
	int ret = 0;
	struct exynos_devfreq_data *data = df->data;
	struct lealt_node *node;
	struct gov_lealt_data *gov_data;


	gov_data = kzalloc(sizeof(struct gov_lealt_data), GFP_KERNEL);
	if (!gov_data)
		return -ENOMEM;

	gov_data->time_in_state_old =
		kzalloc(sizeof(unsigned long) * data->max_state, GFP_KERNEL);
	if (!gov_data->time_in_state_old) {
		ret = -ENOMEM;
		goto out_free_gov_data;
	}

	ret = gov_parse_dt(data, df, gov_data);
	if (ret)
		goto out_free_time_in_state;

	gov_data->df = df;

	data->sysbusy_gov_callback = gov_lealt_sysbusy_callback;
	data->emstune_gov_callback = gov_lealt_emstune_callback;
	mutex_lock(&list_lock);
	list_for_each_entry(node, &lealt_list, list) {
		node->hw->df = df;
		ret = start_monitor(node);
		if (ret)
			goto out_free_time_in_state;
	}
	mutex_unlock(&list_lock);

	ret = sysfs_create_group(&df->dev.kobj, &lealt_dev_attr_group);
	if (ret)
		goto out_free_time_in_state;

	if (gov_data->polling_ms) {
		gov_monitor_initialize(gov_data);
		gov_data->cpufreq_transition_nb.notifier_call
					= cpufreq_transition_notifier;
		exynos_cpufreq_register_transition_notifier(
					&gov_data->cpufreq_transition_nb);
	}

	gov_data_global = gov_data;
	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "governor_lealt",
	  lealt_cpuhp_up, lealt_cpuhp_down);

	df->governor_data = gov_data;
	return 0;

out_free_time_in_state:
	kfree(gov_data->time_in_state_old);

out_free_gov_data:
	kfree(gov_data);
	return ret;
}

static int gov_suspend(struct devfreq *df)
{
	struct gov_lealt_data *gov_data = df->governor_data;
	unsigned long flags;

	spin_lock_irqsave(&gov_data->work_control_lock, flags);
	gov_data->work_started = false;
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);
	return 0;
}

static int gov_resume(struct devfreq *df)
{
	struct gov_lealt_data *gov_data = df->governor_data;
	unsigned long flags;

	spin_lock_irqsave(&cpufreq_stability_list_lock, flags);
	init_cpufreq_stability_all();
	spin_unlock_irqrestore(&cpufreq_stability_list_lock, flags);

	spin_lock_irqsave(&gov_data->work_control_lock, flags);
	gov_data->work_started = true;
	lealt_set_polling_ms(gov_data, gov_data->polling_ms_max);
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);
	return 0;
}

static void gov_stop(struct devfreq *df)
{
	struct gov_lealt_data *gov_data = df->governor_data;
	struct lealt_node *node;

	sysfs_remove_group(&df->dev.kobj, &lealt_dev_attr_group);

	if (gov_data->polling_ms_min)
		devfreq_monitor_stop(df);

	mutex_lock(&list_lock);
	list_for_each_entry(node, &lealt_list, list) {
		stop_monitor(node);
		node->hw->df = NULL;
	}
	mutex_unlock(&list_lock);
	kfree(gov_data->time_in_state_old);
	kfree(gov_data);
}

#define MIN_MS	10U
#define MAX_MS	500U
static int devfreq_lealt_ev_handler(struct devfreq *df,
					unsigned int event, void *data)
{
	int ret;
	unsigned int sample_ms;

	switch (event) {
	case DEVFREQ_GOV_START:
		sample_ms = df->profile->polling_ms;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		df->profile->polling_ms = sample_ms;

		ret = gov_start(df);
		if (ret)
			return ret;

		dev_dbg(df->dev.parent,
			"Enabled LEALT governor\n");
		break;

	case DEVFREQ_GOV_STOP:
		if (df)
			gov_stop(df);
		dev_dbg(df->dev.parent,
			"Disabled LEALT governor\n");
		break;

	case DEVFREQ_GOV_SUSPEND:
		ret = gov_suspend(df);
		if (ret) {
			dev_err(df->dev.parent,
				"Unable to suspend LEALT governor (%d)\n",
				ret);
			return ret;
		}

		dev_dbg(df->dev.parent, "Suspended LEALT governor\n");
		break;

	case DEVFREQ_GOV_RESUME:
		ret = gov_resume(df);
		if (ret) {
			dev_err(df->dev.parent,
				"Unable to resume LEALT governor (%d)\n",
				ret);
			return ret;
		}

		dev_dbg(df->dev.parent, "Resumed LEALT governor\n");
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_gov_lealt = {
	.name = "lealt",
	.get_target_freq = devfreq_lealt_get_freq,
	.event_handler = devfreq_lealt_ev_handler,
};

#define NUM_COLS	2
static struct core_dev_map *init_core_dev_map(struct device *dev,
					struct device_node *of_node,
					char *prop_name)
{
	int len, nf, i, j;
	u32 data;
	struct core_dev_map *tbl;
	int ret;

	if (!of_node)
		of_node = dev->of_node;

	if (!of_find_property(of_node, prop_name, &len))
		return NULL;
	len /= sizeof(data);

	if (len % NUM_COLS || len == 0)
		return NULL;
	nf = len / NUM_COLS;

	tbl = devm_kzalloc(dev, (nf + 1) * sizeof(struct core_dev_map),
			GFP_KERNEL);
	if (!tbl)
		return NULL;

	for (i = 0, j = 0; i < nf; i++, j += 2) {
		ret = of_property_read_u32_index(of_node, prop_name, j,
				&data);
		if (ret)
			return NULL;
		tbl[i].core_mhz = data / 1000;

		ret = of_property_read_u32_index(of_node, prop_name, j + 1,
				&data);
		if (ret)
			return NULL;
		tbl[i].target_load = data;
		pr_debug("Entry%d CPU:%u, TargetLoad:%u\n", i, tbl[i].core_mhz,
				tbl[i].target_load);
	}
	tbl[i].core_mhz = 0;

	return tbl;
}

static struct lealt_node *register_common(struct device *dev,
					   struct lealt_hwmon *hw)
{
	struct lealt_node *node;
	struct device_node *of_child;
	struct cpufreq_stability *stability;

	if (!hw->dev && !hw->of_node)
		return ERR_PTR(-EINVAL);

	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node)
		return ERR_PTR(-ENOMEM);

	node->ratio_ceil = 400;
	node->hw = hw;
	node->llc_on_th_cpu = hw->llc_on_th_cpu;
	node->base_minlock_ratio = hw->base_minlock_ratio;
	node->base_maxlock_ratio = hw->base_maxlock_ratio;

	if (hw->get_child_of_node) {
		of_child = hw->get_child_of_node(dev);
		hw->freq_map = init_core_dev_map(dev, of_child,
					"core-targetload-table");
		of_property_read_u32(of_child, "ratio_ceil", &node->ratio_ceil);
	} else {
		hw->freq_map = init_core_dev_map(dev, NULL,
					"core-targetload-table");
		of_property_read_u32(dev->of_node, "ratio_ceil", &node->ratio_ceil);
	}
	if (!hw->freq_map) {
		dev_err(dev, "Couldn't find the core-targetload table!\n");
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&list_lock);
	list_add_tail(&node->list, &lealt_list);
	mutex_unlock(&list_lock);
	/* Init and Add */
	stability = kzalloc(sizeof(struct cpufreq_stability),
			    GFP_KERNEL);
	get_cpumask(hw, &stability->cpus);
	stability->domain_id = find_domain_id(&stability->cpus);
	stability->prepare_cond = hw->prepare[0];
	stability->prepare_freq = hw->prepare[1];
	stability->stability_th = hw->stability_th;
	init_cpufreq_stability(stability, 0);
	list_add_tail(&stability->node, &cpufreq_stability_list);
	return node;
}

int register_lealt(struct device *dev, struct lealt_hwmon *hw)
{
	struct lealt_node *node;
	int ret = 0;

	node = register_common(dev, hw);
	if (IS_ERR(node)) {
		ret = PTR_ERR(node);
		goto out;
	}

	mutex_lock(&state_lock);
	node->gov = &devfreq_gov_lealt;

	if (!lealt_use_cnt)
		ret = devfreq_add_governor(&devfreq_gov_lealt);
	if (!ret)
		lealt_use_cnt++;
	mutex_unlock(&state_lock);

out:
	if (!ret)
		dev_info(dev, "LEALT-mon registered.\n");
	else
		dev_err(dev, "LEALT-mon registration failed!\n");

	return ret;
}
EXPORT_SYMBOL_GPL(register_lealt);

MODULE_LICENSE("GPL v2");
