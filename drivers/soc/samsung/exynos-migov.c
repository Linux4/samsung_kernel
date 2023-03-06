#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/ems.h>
#include <linux/pm_qos.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>

#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-dm.h>

#include "exynos-gpu-api.h"
#include "../../video/fbdev/exynos/dpu30/decon.h"
#include "../../../kernel/sched/ems/ems.h"

/******************************************************************************/
/*                            Structure for migov                             */
/******************************************************************************/
#define MAXNUM_OF_DVFS		30
#define NUM_OF_CPU_DOMAIN	(MIGOV_CL2 + 1)

enum profile_mode {
	PROFILE_START,
	PROFILE_FINISH,
	PROFILE_UPDATE,
	PROFILE_INVALID,
};

/* Structure for IP's Private Data */
struct private_data_cpu {
	struct private_fn_cpu *fn;

	struct pm_qos_request pm_qos_min_req;
	struct pm_qos_request pm_qos_max_req;
	s32	pm_qos_min_class;
	s32	pm_qos_max_class;
	s32	pm_qos_min_freq;
	s32	pm_qos_max_freq;

	/* Tunables */
	u64	stall_pct_thr;
};

struct private_data_gpu {
	struct private_fn_gpu *fn;

	/* Tunables */
	u64	q0_empty_pct_thr;
	u64	q1_empty_pct_thr;
	u64	gpu_active_pct_thr;
};

struct private_data_mif {
	struct private_fn_mif *fn;

	struct pm_qos_request pm_qos_min_req;
	s32	pm_qos_min_class;
	s32	pm_qos_min_freq;

	/* Tunables */
	s32	stats0_mode_min_freq;
	s32	stats_ratio_mode_cnt_thr;
	u64	stats0_sum_thr;
	u64	stats0_updown_delta_pct_thr;
};

struct domain_data {
	bool			enabled;
	s32			id;

	struct domain_fn	*fn;

	void			*private;

	struct attribute_group	attr_group;
};

struct device_file_operation {
	struct file_operations          fops;
	struct miscdevice               miscdev;
};

static struct migov {
	bool				running;
	bool				profile_only;

	struct domain_data		domains[NUM_OF_DOMAIN];

	ktime_t				start_time;
	ktime_t				end_time;

	u64				start_frame_cnt;
	u64				end_frame_cnt;
	u64				start_frame_vsync_cnt;
	u64				end_frame_vsync_cnt;

#if IS_ENABLED(CONFIG_EXYNOS_FPS_CHANGE_NOTIFY)
	s32				notified_fps;
	struct work_struct		fps_work;
#endif

	struct device_node		*dn;
	struct kobject			*kobj;
	struct mutex			lock;

	s32				running_event;
	struct device_file_operation 	gov_fops;
	wait_queue_head_t		wq;
} migov;

struct profile_sharing_data {
	s64	profile_time_ms;
	u64	profile_frame_cnt;
	u64	profile_frame_vsync_cnt;
	s32	max_fps;

	/* Domain common data */
	s32	max_freq[NUM_OF_DOMAIN];
	s32	min_freq[NUM_OF_DOMAIN];
	s32	freq[NUM_OF_DOMAIN];
	u64	dyn_power[NUM_OF_DOMAIN];
	u64	st_power[NUM_OF_DOMAIN];
	s32	temp[NUM_OF_DOMAIN];
	s32	active_pct[NUM_OF_DOMAIN];

	/* CPU domain private data */
	u64	stall_pct[NUM_OF_CPU_DOMAIN];

	/* GPU domain private data */
	u64	q0_empty_pct;
	u64	q1_empty_pct;
	u64	input_nr_avg_cnt;

	/* MIF domain private data */
	u64	stats0_sum;
	u64	stats0_avg;
	u64	stats_ratio;
	s32	mif_pm_qos_cur_freq;
} psd;

struct tunable_sharing_data {
	s32	monitor;
	s32	window_period;
	s32	window_number;
	s32	active_pct_thr;
	s32	valid_freq_delta_pct;
	s32	min_sensitivity;
	s32	cpu_bottleneck_thr;
	s32	mif_bottleneck_thr;
	s32	gpu_bottleneck_thr;
	s32	gpu_ar_bottleneck_thr;
	s32	mif_stats_ratio_bottleneck_thr;
	s32	uncontrol_fps_delta_up;
	s32	uncontrol_fps_delta_down;
	s32	frame_src;
	s32	max_fps;
	s32	dt_ctrl_en;
	s32	dt_over_thr;
	s32	dt_under_thr;
	s32	dt_up_step;
	s32	dt_down_step;
	s32	dpat_upper_bound_thr;
	s32	dpat_lower_bound_thr;
	s32	dpat_lower_cnt_thr;
	s32	dpat_up_step;
	s32	dpat_down_step;
	s32	inc_perf_temp_thr;
	s32	inc_perf_power_thr;
	s32	v_max_fps_count;

	/* Domain common data */
	bool	enabled[NUM_OF_DOMAIN];
	s32	freq_table[NUM_OF_DOMAIN][MAXNUM_OF_DVFS];
	s32	freq_table_cnt[NUM_OF_DOMAIN];
	s32	max_margin[NUM_OF_DOMAIN];
	s32	min_margin[NUM_OF_DOMAIN];
	s32	margin_up_step[NUM_OF_DOMAIN];
	s32	margin_down_step[NUM_OF_DOMAIN];
	s32	margin_default_step[NUM_OF_DOMAIN];

	/* CPU domain private data */
	u64	stall_pct_thr[NUM_OF_CPU_DOMAIN];

	/* GPU domain private data */
	u64	q0_empty_pct_thr;
	u64	q1_empty_pct_thr;
	u64	gpu_active_pct_thr;

	/* MIF domain private data */
	s32	stats0_mode_min_freq;
	s32	stats_ratio_mode_cnt_thr;
	u64	stats0_sum_thr;
	u64	stats0_updown_delta_pct_thr;
} tsd;

struct delta_sharing_data {
	s32	id;
	s32	freq_delta_pct;
	u32	freq;
	u64	dyn_power;
	u64	st_power;
} dsd;

/******************************************************************************/
/*                               Helper functions                             */
/******************************************************************************/
static inline bool is_enabled(s32 id)
{
	if (id >= NUM_OF_DOMAIN)
		return false;

	return migov.domains[id].enabled;
}

static inline struct domain_data *next_domain(s32 *id)
{
	s32 idx = *id;

	for (++idx; idx < NUM_OF_DOMAIN; idx++)
		if (migov.domains[idx].enabled)
			break;
	*id = idx;

	return idx == NUM_OF_DOMAIN ? NULL : &migov.domains[idx];
}

#define for_each_domain(dom, id)	\
	for (id = -1; (dom) = next_domain(&id), id < NUM_OF_DOMAIN;)

/******************************************************************************/
/*                                      fops                                  */
/******************************************************************************/
#define	IOCTL_MAGIC	'M'

#define IOCTL_READ_TSD	_IOR(IOCTL_MAGIC, 0x50, struct tunable_sharing_data)
#define IOCTL_READ_PSD	_IOR(IOCTL_MAGIC, 0x51, struct profile_sharing_data)
#define IOCTL_WR_DSD	_IOWR(IOCTL_MAGIC, 0x52, struct delta_sharing_data)

ssize_t migov_fops_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	if (!access_ok(VERIFY_WRITE, buf, sizeof(s32)))
		return -EFAULT;

	if (copy_to_user(buf, &migov.running_event, sizeof(s32)))
		pr_info("MIGOV : Reading doesn't work!");

	migov.running_event = PROFILE_INVALID;

	return count;
}

__poll_t migov_fops_poll(struct file *filp, struct poll_table_struct *wait)
{
	__poll_t mask = 0;

	poll_wait(filp, &migov.wq, wait);

	mutex_lock(&migov.lock);
	switch (migov.running_event) {
	case PROFILE_START:
	case PROFILE_FINISH:
		mask = EPOLLIN | EPOLLRDNORM;
		break;
	default:
		break;
	}
	mutex_unlock(&migov.lock);

	return mask;
}

long migov_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case IOCTL_READ_TSD:
		{
			struct __user tunable_sharing_data *user_tsd = (struct __user tunable_sharing_data *)arg;
			if (!access_ok(VERIFY_WRITE, user_tsd, sizeof(struct __user tunable_sharing_data)))
				return -EFAULT;
			if (copy_to_user(user_tsd, &tsd, sizeof(struct tunable_sharing_data)))
				pr_info("MIGOV : IOCTL_READ_TSD doesn't work!");
		}
		break;
	case IOCTL_READ_PSD:
		{
			struct __user profile_sharing_data *user_psd = (struct __user profile_sharing_data *)arg;
			if (!access_ok(VERIFY_WRITE, user_psd, sizeof(struct __user profile_sharing_data)))
				return -EFAULT;
			if (copy_to_user(user_psd, &psd, sizeof(struct profile_sharing_data)))
				pr_info("MIGOV : IOCTL_READ_PSD doesn't work!");
		}
		break;
	case IOCTL_WR_DSD:
		{
			struct __user delta_sharing_data *user_dsd = (struct __user delta_sharing_data *)arg;
			if (!access_ok(VERIFY_READ, user_dsd, sizeof(struct __user delta_sharing_data)) ||
					!access_ok(VERIFY_WRITE, user_dsd, sizeof(struct __user delta_sharing_data)))
				return -EFAULT;
			if (!copy_from_user(&dsd, user_dsd, sizeof(struct delta_sharing_data))) {
				struct domain_data *dom;
				if (!is_enabled(dsd.id))
					return -EINVAL;
				dom = &migov.domains[dsd.id];
				dom->fn->get_power_change(dsd.id, dsd.freq_delta_pct,
						&dsd.freq, &dsd.dyn_power, &dsd.st_power);
				if (copy_to_user(user_dsd, &dsd, sizeof(struct delta_sharing_data)))
					pr_info("MIGOV : IOCTL_RW_DSD doesn't work!");
			}
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int migov_fops_release(struct inode *node, struct file *filp)
{
	return 0;
}

/******************************************************************************/
/*                                FPS functions                               */
/******************************************************************************/
extern ems_frame_cnt;
static u64 get_frame_cnt(void)
{
	return ems_frame_cnt;
}

extern u64 frame_vsync_cnt;
static u64 get_frame_vsync_cnt(void)
{
	return frame_vsync_cnt;
}

#if IS_ENABLED(CONFIG_EXYNOS_FPS_CHANGE_NOTIFY)
static void migov_fps_change_work(struct work_struct *work)
{
	mutex_lock(&migov.lock);

	tsd.max_fps = migov.notified_fps;
	psd.max_fps = migov.notified_fps;

	mutex_unlock(&migov.lock);
}

static int migov_fps_change_callback(struct notifier_block *nb,
				unsigned long val, void *info)
{
	if (val < 0)
		return NOTIFY_DONE;

	if (migov.notified_fps != val * 10) {
		migov.notified_fps = val * 10;
		schedule_work(&migov.fps_work);
	}

	return NOTIFY_OK;
}

static struct notifier_block migov_fps_change_notifier = {
	.notifier_call = migov_fps_change_callback,
};
#endif

/******************************************************************************/
/*                             profile functions                              */
/******************************************************************************/
static void migov_get_cpu_private_data(struct domain_data *dom)
{
	struct private_data_cpu *private = dom->private;

	psd.stall_pct[dom->id] = private->fn->get_stall_pct(dom->id);
}

static void migov_get_gpu_private_data(struct domain_data *dom)
{
	struct private_data_gpu *private = dom->private;

	psd.q0_empty_pct = private->fn->get_q_empty_pct(0);
	psd.q1_empty_pct = private->fn->get_q_empty_pct(1);
	psd.input_nr_avg_cnt = private->fn->get_input_nr_avg_cnt();
}

static void migov_get_mif_private_data(struct domain_data *dom)
{
	struct private_data_mif *private = dom->private;

	psd.mif_pm_qos_cur_freq = pm_qos_request(private->pm_qos_min_class);
	psd.stats0_sum = private->fn->get_stats0_sum();
	psd.stats0_avg = private->fn->get_stats0_avg();
	psd.stats_ratio = private->fn->get_stats_ratio();
}

static void migov_get_profile_data(struct domain_data *dom)
{
	s32 id = dom->id;

	/* dvfs */
	psd.max_freq[id] = dom->fn->get_max_freq(id);
	psd.min_freq[id] = dom->fn->get_min_freq(id);
	/* Only for Exysos9830 */
	if (dom->id <= MIGOV_CL2) {
		struct private_data_cpu *private = dom->private;

		psd.max_freq[id] = min(psd.max_freq[id], pm_qos_request(private->pm_qos_max_class));
		psd.min_freq[id] = max(psd.min_freq[id], pm_qos_request(private->pm_qos_min_class));
	}
	psd.freq[id] = dom->fn->get_freq(id);

	/* power */
	dom->fn->get_power(id, &psd.dyn_power[id], &psd.st_power[id]);

	/* temperature */
	psd.temp[id] = dom->fn->get_temp(id);

	/* active pct */
	psd.active_pct[id] = dom->fn->get_active_pct(id);

	/* private */
	if (id <= MIGOV_CL2) {
		migov_get_cpu_private_data(dom);
	} else if (id == MIGOV_GPU) {
		migov_get_gpu_private_data(dom);
	} else if (id == MIGOV_MIF) {
		migov_get_mif_private_data(dom);
	}
}

void migov_set_tunable_data(void)
{
	struct domain_data *dom;
	s32 id;

	for_each_domain(dom, id) {
		tsd.enabled[id] = dom->enabled;

		if (id <= MIGOV_CL2) {
			struct private_data_cpu *private = dom->private;

			tsd.stall_pct_thr[id] = private->stall_pct_thr;
		} else if (id == MIGOV_GPU) {
			struct private_data_gpu *private = dom->private;

			tsd.q0_empty_pct_thr = private->q0_empty_pct_thr;
			tsd.q1_empty_pct_thr = private->q1_empty_pct_thr;
			tsd.gpu_active_pct_thr = private->gpu_active_pct_thr;

		} else if (id == MIGOV_MIF) {
			struct private_data_mif *private = dom->private;

			tsd.stats0_mode_min_freq = private->stats0_mode_min_freq;
			tsd.stats_ratio_mode_cnt_thr = private->stats_ratio_mode_cnt_thr;
			tsd.stats0_sum_thr = private->stats0_sum_thr;
			tsd.stats0_updown_delta_pct_thr = private->stats0_updown_delta_pct_thr;
		}
	}
}

/******************************************************************************/
/*                              Profile functions                             */
/******************************************************************************/
void migov_update_profile(void)
{
	struct domain_data *dom;
	s32 id;

	mutex_lock(&migov.lock);

	migov.end_time = ktime_get();
	psd.profile_time_ms = ktime_to_ms(ktime_sub(migov.end_time, migov.start_time));
	migov.end_frame_cnt = get_frame_cnt();
	migov.end_frame_vsync_cnt = get_frame_vsync_cnt();
	psd.profile_frame_cnt = migov.end_frame_cnt - migov.start_frame_cnt;
	psd.profile_frame_vsync_cnt = migov.end_frame_vsync_cnt - migov.start_frame_vsync_cnt;

	for_each_domain(dom, id) {
		dom->fn->update_mode(id, 1);
		migov_get_profile_data(dom);
	}

	migov.start_time = migov.end_time;
	migov.start_frame_cnt = migov.end_frame_cnt;
	migov.start_frame_vsync_cnt = migov.end_frame_vsync_cnt;

	mutex_unlock(&migov.lock);
}

void migov_start_profile(void)
{
	struct domain_data *dom;
	s32 id;

	mutex_lock(&migov.lock);

	if (migov.running) {
		mutex_unlock(&migov.lock);
		return;
	}

	migov.running = true;
	migov.start_time = ktime_get();
	migov.start_frame_cnt = get_frame_cnt();
	migov.start_frame_vsync_cnt = get_frame_vsync_cnt();

	if (!migov.profile_only) {
		set_lbt_overutil_with_migov(1);
		if (is_enabled(MIGOV_CL0) || is_enabled(MIGOV_CL1) || is_enabled(MIGOV_CL2))
			exynos_dm_dynamic_disable(1);
		if (is_enabled(MIGOV_GPU))
			exynos_migov_set_mode(1);
	}

	for_each_domain(dom, id) {
		dom->fn->update_mode(dom->id, 1);

		if (migov.profile_only)
			continue;

		if (id <= MIGOV_CL2) {
			struct private_data_cpu *private = dom->private;

			if (pm_qos_request_active(&private->pm_qos_min_req)) {
				pm_qos_update_request(&private->pm_qos_min_req,
						private->pm_qos_min_freq);
			}
			if (pm_qos_request_active(&private->pm_qos_max_req)) {
				pm_qos_update_request(&private->pm_qos_max_req, 0);
				pm_qos_update_request(&private->pm_qos_max_req,
						private->pm_qos_max_freq);
			}
		}
	}

	migov_set_tunable_data();

	mutex_unlock(&migov.lock);

	pr_info("MIGOV: %s:%d: running=%d\n", __func__, __LINE__, migov.running);
}

void migov_finish_profile(void)
{
	struct domain_data *dom;
	s32 id;

	mutex_lock(&migov.lock);

	if (!migov.running) {
		mutex_unlock(&migov.lock);
		return;
	}

	if (!migov.profile_only) {
		set_lbt_overutil_with_migov(0);
		if (is_enabled(MIGOV_CL0) || is_enabled(MIGOV_CL1) || is_enabled(MIGOV_CL2))
			exynos_dm_dynamic_disable(0);
		if (is_enabled(MIGOV_GPU))
			exynos_migov_set_mode(0);
	}

	for_each_domain(dom, id) {
		dom->fn->update_mode(id, 0);

		if (migov.profile_only)
			continue;

		if (id <= MIGOV_CL2) {
			struct private_data_cpu *private = dom->private;

			if (pm_qos_request_active(&private->pm_qos_min_req))
				pm_qos_update_request(&private->pm_qos_min_req,
						PM_QOS_DEFAULT_VALUE);
			if (pm_qos_request_active(&private->pm_qos_max_req))
				pm_qos_update_request(&private->pm_qos_max_req,
						PM_QOS_DEFAULT_VALUE);
		}
	}

	migov.running = false;

	mutex_unlock(&migov.lock);

	pr_info("MIGOV: %s:%d: running=%d\n", __func__, __LINE__, migov.running);
}

static void wakeup_polling_task(bool new_mode)
{
	mutex_lock(&migov.lock);
	if (!migov.running && new_mode)
		migov.running_event = PROFILE_START;
	else if (migov.running && !new_mode)
		migov.running_event = PROFILE_FINISH;
	else
		migov.running_event = PROFILE_INVALID;
	mutex_unlock(&migov.lock);

	pr_info("MIGOV: running=%d, mode=%d, running_event=%d\n",
			migov.running, new_mode, migov.running_event);

	if (migov.running_event != PROFILE_INVALID)
		wake_up_interruptible(&migov.wq);
}

static int migov_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;

	wakeup_polling_task(cur_set->migov.migov_en);

	return NOTIFY_OK;
}
static struct notifier_block migov_update_notifier = {
	.notifier_call = migov_update_callback,
};

/******************************************************************************/
/*                               SYSFS functions                              */
/******************************************************************************/
static ssize_t running_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", migov.running);
}
static ssize_t running_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 mode;

	if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

	wakeup_polling_task(mode);

	return count;
}
static DEVICE_ATTR_RW(running);

static ssize_t profile_only_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", migov.profile_only);
}
static ssize_t profile_only_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	migov.profile_only = (bool)val;

	return count;
}
static DEVICE_ATTR_RW(profile_only);

#define TUNABLE_ATTR_RW(_name)							\
static ssize_t _name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)			\
{										\
	return snprintf(buf, PAGE_SIZE, "%d\n", tsd._name);			\
}										\
static ssize_t _name##_store(struct device *dev,				\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	s32 val;								\
										\
	if (sscanf(buf, "%d", &val) != 1)					\
		return -EINVAL;							\
										\
	tsd._name = val;							\
										\
	return count;								\
}										\
static DEVICE_ATTR_RW(_name);

TUNABLE_ATTR_RW(monitor);
TUNABLE_ATTR_RW(window_period);
TUNABLE_ATTR_RW(window_number);
TUNABLE_ATTR_RW(active_pct_thr);
TUNABLE_ATTR_RW(valid_freq_delta_pct);
TUNABLE_ATTR_RW(min_sensitivity);
TUNABLE_ATTR_RW(cpu_bottleneck_thr);
TUNABLE_ATTR_RW(gpu_bottleneck_thr);
TUNABLE_ATTR_RW(gpu_ar_bottleneck_thr);
TUNABLE_ATTR_RW(mif_bottleneck_thr);
TUNABLE_ATTR_RW(mif_stats_ratio_bottleneck_thr);
TUNABLE_ATTR_RW(uncontrol_fps_delta_up);
TUNABLE_ATTR_RW(uncontrol_fps_delta_down);
TUNABLE_ATTR_RW(dt_ctrl_en);
TUNABLE_ATTR_RW(dt_over_thr);
TUNABLE_ATTR_RW(dt_under_thr);
TUNABLE_ATTR_RW(dt_up_step);
TUNABLE_ATTR_RW(dt_down_step);
TUNABLE_ATTR_RW(dpat_upper_bound_thr);
TUNABLE_ATTR_RW(dpat_lower_bound_thr);
TUNABLE_ATTR_RW(dpat_lower_cnt_thr);
TUNABLE_ATTR_RW(dpat_up_step);
TUNABLE_ATTR_RW(dpat_down_step);
TUNABLE_ATTR_RW(inc_perf_temp_thr);
TUNABLE_ATTR_RW(inc_perf_power_thr);

#define PER_CPU_TUNABLE_ATTR_RW(_name)						\
static ssize_t _name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)			\
{										\
	struct domain_data *dom;						\
	s32 id, ret = 0;							\
										\
	for_each_domain(dom, id) {						\
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%d]%s: %d\n",	\
				id, domain_name[id], tsd._name[id]);		\
	}									\
										\
	return ret;								\
}										\
static ssize_t _name##_store(struct device *dev,				\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	s32 id, val;								\
										\
	if (sscanf(buf, "%d %d", &id, &val) != 2)				\
		return -EINVAL;							\
										\
	mutex_lock(&migov.lock);						\
	if (!is_enabled(id))							\
		return -EINVAL;							\
	tsd._name[id] = val;							\
	mutex_unlock(&migov.lock);						\
										\
	return count;								\
}										\
static DEVICE_ATTR_RW(_name);

PER_CPU_TUNABLE_ATTR_RW(max_margin);
PER_CPU_TUNABLE_ATTR_RW(min_margin);
PER_CPU_TUNABLE_ATTR_RW(margin_up_step);
PER_CPU_TUNABLE_ATTR_RW(margin_down_step);
PER_CPU_TUNABLE_ATTR_RW(margin_default_step);

static struct attribute *migov_attrs[] = {
	&dev_attr_running.attr,
	&dev_attr_profile_only.attr,
	&dev_attr_monitor.attr,
	&dev_attr_window_period.attr,
	&dev_attr_window_number.attr,
	&dev_attr_active_pct_thr.attr,
	&dev_attr_valid_freq_delta_pct.attr,
	&dev_attr_min_sensitivity.attr,
	&dev_attr_cpu_bottleneck_thr.attr,
	&dev_attr_gpu_bottleneck_thr.attr,
	&dev_attr_gpu_ar_bottleneck_thr.attr,
	&dev_attr_mif_bottleneck_thr.attr,
	&dev_attr_mif_stats_ratio_bottleneck_thr.attr,
	&dev_attr_uncontrol_fps_delta_up.attr,
	&dev_attr_uncontrol_fps_delta_down.attr,
	&dev_attr_dt_ctrl_en.attr,
	&dev_attr_dt_over_thr.attr,
	&dev_attr_dt_under_thr.attr,
	&dev_attr_dt_up_step.attr,
	&dev_attr_dt_down_step.attr,
	&dev_attr_dpat_upper_bound_thr.attr,
	&dev_attr_dpat_lower_bound_thr.attr,
	&dev_attr_dpat_lower_cnt_thr.attr,
	&dev_attr_dpat_up_step.attr,
	&dev_attr_dpat_down_step.attr,
	&dev_attr_max_margin.attr,
	&dev_attr_min_margin.attr,
	&dev_attr_margin_up_step.attr,
	&dev_attr_margin_down_step.attr,
	&dev_attr_margin_default_step.attr,
	&dev_attr_inc_perf_temp_thr.attr,
	&dev_attr_inc_perf_power_thr.attr,
	NULL,
};
static struct attribute_group migov_attr_group = {
	.name = "migov",
	.attrs = migov_attrs,
};

#define PRIVATE_ATTR_RW(_priv, _id, _ip, _name)					\
static ssize_t _ip##_##_name##_show(struct device *dev,				\
		struct device_attribute *attr, char *buf)			\
{										\
	struct domain_data *dom = &migov.domains[_id];				\
	struct private_data_##_priv *private;					\
										\
	if (!dom)								\
		return 0;							\
	private = dom->private;							\
	return snprintf(buf, PAGE_SIZE, "%d\n", private->_name);		\
}										\
static ssize_t _ip##_##_name##_store(struct device *dev,			\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	struct domain_data *dom = &migov.domains[_id];				\
	struct private_data_##_priv *private;					\
	s32 val;								\
										\
	if (sscanf(buf, "%d", &val) != 1)					\
		return -EINVAL;							\
	if (!dom)								\
		return 0;							\
	private = dom->private;							\
	private->_name = val;							\
	return count;								\
}										\
static DEVICE_ATTR_RW(_ip##_##_name);
#define CPU_PRIVATE_ATTR_RW(_id, _ip, _name)					\
	PRIVATE_ATTR_RW(cpu, _id, _ip, _name)
#define GPU_PRIVATE_ATTR_RW(_id, _ip, _name)					\
	PRIVATE_ATTR_RW(gpu, _id, _ip, _name)
#define MIF_PRIVATE_ATTR_RW(_id, _ip, _name)					\
	PRIVATE_ATTR_RW(mif, _id, _ip, _name)

CPU_PRIVATE_ATTR_RW(MIGOV_CL0, cl0, stall_pct_thr);
CPU_PRIVATE_ATTR_RW(MIGOV_CL0, cl0, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL0, cl0, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL1, cl1, stall_pct_thr);
CPU_PRIVATE_ATTR_RW(MIGOV_CL1, cl1, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL1, cl1, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL2, cl2, stall_pct_thr);
CPU_PRIVATE_ATTR_RW(MIGOV_CL2, cl2, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL2, cl2, pm_qos_max_freq);
GPU_PRIVATE_ATTR_RW(MIGOV_GPU, gpu, q0_empty_pct_thr);
GPU_PRIVATE_ATTR_RW(MIGOV_GPU, gpu, q1_empty_pct_thr);
GPU_PRIVATE_ATTR_RW(MIGOV_GPU, gpu, gpu_active_pct_thr);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, stats0_mode_min_freq);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, stats_ratio_mode_cnt_thr);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, stats0_sum_thr);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, stats0_updown_delta_pct_thr);

#define MAXNUM_OF_PRIV_ATTR 10
static struct attribute *private_attrs[NUM_OF_DOMAIN][MAXNUM_OF_PRIV_ATTR] = {
	{
		&dev_attr_cl0_stall_pct_thr.attr,
		&dev_attr_cl0_pm_qos_min_freq.attr,
		&dev_attr_cl0_pm_qos_max_freq.attr,
		NULL,
	},
	{
		&dev_attr_cl1_stall_pct_thr.attr,
		&dev_attr_cl1_pm_qos_min_freq.attr,
		&dev_attr_cl1_pm_qos_max_freq.attr,
		NULL,
	},
	{
		&dev_attr_cl2_stall_pct_thr.attr,
		&dev_attr_cl2_pm_qos_min_freq.attr,
		&dev_attr_cl2_pm_qos_max_freq.attr,
		NULL,
	},
	{
		&dev_attr_gpu_q0_empty_pct_thr.attr,
		&dev_attr_gpu_q1_empty_pct_thr.attr,
		&dev_attr_gpu_gpu_active_pct_thr.attr,
		NULL,
	},
	{
		&dev_attr_mif_stats0_mode_min_freq.attr,
		&dev_attr_mif_stats_ratio_mode_cnt_thr.attr,
		&dev_attr_mif_stats0_sum_thr.attr,
		&dev_attr_mif_stats0_updown_delta_pct_thr.attr,
		NULL,
	},
};

static ssize_t set_margin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}
static ssize_t set_margin_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 id, margin;

	if (sscanf(buf, "%d %d", &id, &margin) != 2)
		return -EINVAL;

	if (is_enabled(id) && !migov.profile_only) {
		struct domain_data *dom = &migov.domains[id];

		if (id != MIGOV_MIF) {
			dom->fn->set_margin(id, margin);
		} else {
			struct private_data_mif *private = migov.domains[id].private;

			pm_qos_update_request(&private->pm_qos_min_req, margin);
		}
	}

	return count;
}
static DEVICE_ATTR_RW(set_margin);

static ssize_t control_profile_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s32 ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_time_ms=%lld\n", psd.profile_time_ms);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_frame_cnt=%llu\n", psd.profile_frame_cnt);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "max_fps=%d\n", psd.max_fps);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "max_freq=%d, %d, %d, %d, %d\n",
			psd.max_freq[0], psd.max_freq[1], psd.max_freq[2], psd.max_freq[3], psd.max_freq[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "min_freq=%d, %d, %d, %d, %d\n",
			psd.min_freq[0], psd.min_freq[1], psd.min_freq[2], psd.min_freq[3], psd.min_freq[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "freq=%d, %d, %d, %d, %d\n",
			psd.freq[0], psd.freq[1], psd.freq[2], psd.freq[3], psd.freq[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "dyn_power=%llu, %llu, %llu, %llu, %llu\n",
			psd.dyn_power[0], psd.dyn_power[1], psd.dyn_power[2], psd.dyn_power[3], psd.dyn_power[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "st_power=%llu, %llu, %llu, %llu, %llu\n",
			psd.st_power[0], psd.st_power[1], psd.st_power[2], psd.st_power[3], psd.st_power[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "temp=%d, %d, %d, %d, %d\n",
			psd.temp[0], psd.temp[1], psd.temp[2], psd.temp[3], psd.temp[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "active_pct=%d, %d, %d, %d, %d\n",
			psd.active_pct[0], psd.active_pct[1], psd.active_pct[2], psd.active_pct[3], psd.active_pct[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stall_pct=%llu, %llu, %llu\n",
			psd.stall_pct[0], psd.stall_pct[1], psd.stall_pct[2]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "q0_empty_pct=%llu\n", psd.q0_empty_pct);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "q1_empty_pct=%llu\n", psd.q1_empty_pct);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "input_nr_avg_cnt=%llu\n", psd.input_nr_avg_cnt);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats0_sum=%llu\n", psd.stats0_sum);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats0_avg=%llu\n", psd.stats0_avg);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats_ratio=%llu\n", psd.stats_ratio);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "mif_pm_qos_cur_freq=%d\n", psd.mif_pm_qos_cur_freq);

	return ret;
}
static ssize_t control_profile_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 mode;

	if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

	if (!migov.running && mode == PROFILE_START)
		migov_start_profile();
	else if (migov.running && mode == PROFILE_FINISH)
		migov_finish_profile();
	else if (migov.running && mode == PROFILE_UPDATE)
		migov_update_profile();

	return count;
}
static DEVICE_ATTR_RW(control_profile);

static struct attribute *control_attrs[] = {
	&dev_attr_set_margin.attr,
	&dev_attr_control_profile.attr,
	NULL,
};
static struct attribute_group control_attr_group = {
	.name = "control",
	.attrs = control_attrs,
};

/******************************************************************************/
/*                                Initialize                                  */
/******************************************************************************/
static s32 init_domain_data(struct device_node *root,
			struct domain_data *dom, void *private_fn)
{
	struct device_node *dn;
	s32 cnt, ret = 0, id = dom->id;

	dn = of_get_child_by_name(root, domain_name[dom->id]);
	if (!dn) {
		pr_err("migov: Failed to get node of domain(id=%d)\n", dom->id);
		return -ENODEV;
	}

	tsd.freq_table_cnt[id] = dom->fn->get_table_cnt(id);
	cnt = dom->fn->get_freq_table(id, tsd.freq_table[id]);
	if (tsd.freq_table_cnt[id] != cnt) {
		pr_err("migov: table cnt(%u) is un-synced with freq table cnt(%u)(id=%d)\n",
					id, tsd.freq_table_cnt[id], cnt);
		return -EINVAL;
	}

	ret |= of_property_read_s32(dn, "max", &tsd.max_margin[id]);
	ret |= of_property_read_s32(dn, "min", &tsd.min_margin[id]);
	ret |= of_property_read_s32(dn, "up-step", &tsd.margin_up_step[id]);
	ret |= of_property_read_s32(dn, "down-step", &tsd.margin_down_step[id]);
	ret |= of_property_read_s32(dn, "default-step", &tsd.margin_default_step[id]);
	if (ret)
		return -EINVAL;

	if (dom->id <= MIGOV_CL2) {
		struct private_data_cpu *private;
		s32 val;

		private = kzalloc(sizeof(struct private_data_cpu), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "stall-pct-thr", &val);
		private->stall_pct_thr = val;

		if (!of_property_read_s32(dn, "pm-qos-min-class", &private->pm_qos_min_class)) {
			if (of_property_read_s32(dn, "pm-qos-min-freq", &private->pm_qos_min_freq))
				private->pm_qos_min_freq = PM_QOS_DEFAULT_VALUE;

			pm_qos_add_request(&private->pm_qos_min_req,
					private->pm_qos_min_class, PM_QOS_DEFAULT_VALUE);
		}

		if (!of_property_read_s32(dn, "pm-qos-max-class", &private->pm_qos_max_class)) {
			if (of_property_read_s32(dn, "pm-qos-max-freq", &private->pm_qos_max_freq))
				private->pm_qos_max_freq = PM_QOS_DEFAULT_VALUE;

			pm_qos_add_request(&private->pm_qos_max_req,
					private->pm_qos_max_class, PM_QOS_DEFAULT_VALUE);
		}

		if (!ret) {
			dom->private = private;
		} else {
			kfree(private);
		}
	} else if (dom->id == MIGOV_GPU) {
		struct private_data_gpu *private;
		s32 val;

		private = kzalloc(sizeof(struct private_data_gpu), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "q0-empty-pct-thr", &val);
		private->q0_empty_pct_thr = val;
		ret |= of_property_read_s32(dn, "q1-empty-pct-thr", &val);
		private->q1_empty_pct_thr = val;
		ret |= of_property_read_s32(dn, "active-pct-thr", &val);
		private->gpu_active_pct_thr = val;

		if (!ret) {
			dom->private = private;
		} else {
			kfree(private);
		}
	} else if (dom->id == MIGOV_MIF) {
		struct private_data_mif *private;
		s32 val;

		private = kzalloc(sizeof(struct private_data_mif), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "pm-qos-min-class",
				&private->pm_qos_min_class);
		ret |= of_property_read_s32(dn, "freq-stats0-mode-min-freq",
				&private->stats0_mode_min_freq);
		ret |= of_property_read_s32(dn, "freq-stats-ratio-mode-cnt-thr",
				&private->stats_ratio_mode_cnt_thr);
		ret |= of_property_read_s32(dn, "freq-stats0-thr", &val);
		private->stats0_sum_thr = val;
		ret |= of_property_read_s32(dn, "freq-stats0-updown-delta-pct-thr", &val);
		private->stats0_updown_delta_pct_thr = val;

		if (!ret) {
			pm_qos_add_request(&private->pm_qos_min_req,
					private->pm_qos_min_class, PM_QOS_DEFAULT_VALUE);
			dom->private = private;
		} else {
			kfree(private);
		}
	}

	if (!ret) {
		dom->attr_group.name = domain_name[dom->id];
		dom->attr_group.attrs = private_attrs[dom->id];
		sysfs_create_group(migov.kobj, &dom->attr_group);
	}

	return 0;
}

s32 exynos_migov_register_misc_device(void)
{
	s32 ret;

	migov.gov_fops.fops.owner		= THIS_MODULE;
	migov.gov_fops.fops.llseek		= no_llseek;
	migov.gov_fops.fops.read		= migov_fops_read;
	migov.gov_fops.fops.poll		= migov_fops_poll;
	migov.gov_fops.fops.unlocked_ioctl	= migov_fops_ioctl;
	migov.gov_fops.fops.compat_ioctl	= migov_fops_ioctl;
	migov.gov_fops.fops.release		= migov_fops_release;

	migov.gov_fops.miscdev.minor		= MISC_DYNAMIC_MINOR;
	migov.gov_fops.miscdev.name		= "exynos-migov";
	migov.gov_fops.miscdev.fops		= &migov.gov_fops.fops;

	ret = misc_register(&migov.gov_fops.miscdev);
        if (ret) {
                pr_err("exynos-migov couldn't register misc device!");
                return ret;
        }

	return 0;
}

s32 exynos_migov_register_domain(s32 id, struct domain_fn *fn, void *private_fn)
{
	struct domain_data *dom = &migov.domains[id];

	if (id >= NUM_OF_DOMAIN || (dom->enabled)) {
		pr_err("migov: invalid id or duplicated register (id: %d)\n", id);
		return -EINVAL;
	}

	if (!fn) {
		pr_err("migov: there is no callback address (id: %d)\n", id);
		return -EINVAL;
	}

	dom->id = id;
	dom->fn = fn;

	if (init_domain_data(migov.dn, dom, private_fn)) {
		pr_err("migov: failed to init domain data(id=%d)\n", id);
		return -EINVAL;
	}

	dom->enabled = true;

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_migov_register_domain);

static s32 parse_migov_dt(struct device_node *dn)
{
	s32 ret = 0;

	ret |= of_property_read_s32(dn, "window-period", &tsd.window_period);
	ret |= of_property_read_s32(dn, "window-number", &tsd.window_number);
	ret |= of_property_read_s32(dn, "active-pct-thr", &tsd.active_pct_thr);
	ret |= of_property_read_s32(dn, "valid-freq-delta-pct", &tsd.valid_freq_delta_pct);
	ret |= of_property_read_s32(dn, "min-sensitivity", &tsd.min_sensitivity);
	ret |= of_property_read_s32(dn, "cpu-bottleneck-thr", &tsd.cpu_bottleneck_thr);
	ret |= of_property_read_s32(dn, "gpu-bottleneck-thr", &tsd.gpu_bottleneck_thr);
	ret |= of_property_read_s32(dn, "gpu-ar-bottleneck-thr", &tsd.gpu_ar_bottleneck_thr);
	ret |= of_property_read_s32(dn, "mif-bottleneck-thr", &tsd.mif_bottleneck_thr);
	ret |= of_property_read_s32(dn, "mif-freq-stats-ratio-bottleneck-thr", &tsd.mif_stats_ratio_bottleneck_thr);
	ret |= of_property_read_s32(dn, "frame-src", &tsd.frame_src);
	ret |= of_property_read_s32(dn, "max-fps", &tsd.max_fps);
	psd.max_fps = tsd.max_fps;
	ret |= of_property_read_s32(dn, "uncontrol-fps-delta-up", &tsd.uncontrol_fps_delta_up);
	ret |= of_property_read_s32(dn, "uncontrol-fps-delta-down", &tsd.uncontrol_fps_delta_down);
	ret |= of_property_read_s32(dn, "dt-ctrl-en", &tsd.dt_ctrl_en);
	ret |= of_property_read_s32(dn, "dt-over-thr", &tsd.dt_over_thr);
	ret |= of_property_read_s32(dn, "dt-under-thr", &tsd.dt_under_thr);
	ret |= of_property_read_s32(dn, "dt-up-step", &tsd.dt_up_step);
	ret |= of_property_read_s32(dn, "dt-down-step", &tsd.dt_down_step);
	ret |= of_property_read_s32(dn, "dpat-upper-bound-thr", &tsd.dpat_upper_bound_thr);
	ret |= of_property_read_s32(dn, "dpat-lower-bound-thr", &tsd.dpat_lower_bound_thr);
	ret |= of_property_read_s32(dn, "dpat-lower-cnt-thr", &tsd.dpat_lower_cnt_thr);
	ret |= of_property_read_s32(dn, "dpat-up-step", &tsd.dpat_up_step);
	ret |= of_property_read_s32(dn, "dpat-down-step", &tsd.dpat_down_step);
	ret |= of_property_read_s32(dn, "inc-perf-temp-thr", &tsd.inc_perf_temp_thr);
	ret |= of_property_read_s32(dn, "inc-perf-power-thr", &tsd.inc_perf_power_thr);
	ret |= of_property_read_s32(dn, "v-max-fps-count", &tsd.v_max_fps_count);

	return ret;
}

static s32 exynos_migov_suspend(struct device *dev)
{
	if (migov.running)
		wakeup_polling_task(0);

	return 0;
}

static s32 exynos_migov_resume(struct device *dev)
{
	return 0;
}

static s32 exynos_migov_probe(struct platform_device *pdev)
{
	s32 ret;

	migov.running = false;

	migov.dn = pdev->dev.of_node;
	if (!migov.dn) {
		pr_err("migov: Failed to get device tree\n");
		return -EINVAL;
	}

	ret = parse_migov_dt(migov.dn);
	if (ret) {
		pr_err("migov: Failed to parse device tree\n");
		return ret;
	}

	migov.kobj = &pdev->dev.kobj;
	ret += sysfs_create_group(migov.kobj, &migov_attr_group);
	ret += sysfs_create_group(migov.kobj, &control_attr_group);
	if (ret) {
		pr_err("migov: Failed to init sysfs\n");
		return ret;
	}

#if IS_ENABLED(CONFIG_EXYNOS_FPS_CHANGE_NOTIFY)
	INIT_WORK(&migov.fps_work, migov_fps_change_work);
	ret = register_fps_change_notifier(&migov_fps_change_notifier);
	if (ret) {
		pr_err("migov: Failed to register fps change notifier\n");
		return ret;
	}
#endif

	mutex_init(&migov.lock);

	emstune_register_mode_update_notifier(&migov_update_notifier);

	migov.running_event = PROFILE_INVALID;
	init_waitqueue_head(&migov.wq);
	exynos_migov_register_misc_device();

	pr_info("migov: complete to probe migov\n");

	return ret;
}

static const struct of_device_id exynos_migov_match[] = {
	{ .compatible	= "samsung,exynos-migov", },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_migov_match);

static const struct dev_pm_ops exynos_migov_pm_ops = {
	.suspend	= exynos_migov_suspend,
	.resume		= exynos_migov_resume,
};

static struct platform_driver exynos_migov_driver = {
	.probe		= exynos_migov_probe,
	.driver	= {
		.name	= "exynos-migov",
		.owner	= THIS_MODULE,
		.pm	= &exynos_migov_pm_ops,
		.of_match_table = exynos_migov_match,
	},
};

static s32 exynos_migov_init(void)
{
	return platform_driver_register(&exynos_migov_driver);
}
late_initcall(exynos_migov_init);

MODULE_SOFTDEP("pre: exynos9-decon");
MODULE_DESCRIPTION("Exynos MIGOV");
MODULE_LICENSE("GPL");
