#include <legacy/exynos-main-profiler.h>

/******************************************************************************/
/*                               Helper functions                             */
/******************************************************************************/
static inline bool is_enabled(s32 id)
{
	if (id >= NUM_OF_DOMAIN || id < 0)
		return false;

	return profiler.domains[id].enabled;
}

static inline struct domain_data *next_domain(s32 *id)
{
	s32 idx = *id;

	for (++idx; idx < NUM_OF_DOMAIN; idx++)
		if (profiler.domains[idx].enabled)
			break;
	*id = idx;

	return idx == NUM_OF_DOMAIN ? NULL : &profiler.domains[idx];
}

/******************************************************************************/
/*                                      fops                                  */
/******************************************************************************/

ssize_t profiler_fops_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	if (!access_ok(buf, sizeof(s32)))
		return -EFAULT;

	if (copy_to_user(buf, &profiler.running_event, sizeof(s32)))
		pr_info("PROFILER : Reading doesn't work!");

	profiler.running_event = PROFILE_INVALID;

	return count;
}

__poll_t profiler_fops_poll(struct file *filp, struct poll_table_struct *wait)
{
	__poll_t mask = 0;

	poll_wait(filp, &profiler.wq, wait);

	switch (profiler.running_event) {
	case PROFILE_START_BY_GAME:
	case PROFILE_START_BY_GPU:
	case PROFILE_FINISH:
		mask = EPOLLIN | EPOLLRDNORM;
		break;
	default:
		break;
	}

	return mask;
}

long profiler_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case IOCTL_READ_TSD:
		{
			struct __user tunable_sharing_data *user_tsd = (struct __user tunable_sharing_data *)arg;
			if (!access_ok(user_tsd, sizeof(struct __user tunable_sharing_data)))
				return -EFAULT;
			if (copy_to_user(user_tsd, &tsd, sizeof(struct tunable_sharing_data)))
				pr_info("PROFILER : IOCTL_READ_TSD doesn't work!");
		}
		break;
	case IOCTL_READ_PSD:
		{
			struct __user profile_sharing_data *user_psd = (struct __user profile_sharing_data *)arg;
			if (!access_ok(user_psd, sizeof(struct __user profile_sharing_data)))
				return -EFAULT;
			if (copy_to_user(user_psd, &psd, sizeof(struct profile_sharing_data)))
				pr_info("PROFILER : IOCTL_READ_PSD doesn't work!");
		}
		break;
	case IOCTL_WR_DSD:
		{
			struct __user delta_sharing_data *user_dsd = (struct __user delta_sharing_data *)arg;
			if (!access_ok(user_dsd, sizeof(struct __user delta_sharing_data)))
				return -EFAULT;
			mutex_lock(&profiler_dsd_lock);
			if (!copy_from_user(&dsd, user_dsd, sizeof(struct delta_sharing_data))) {
				struct domain_data *dom;
				if (!is_enabled(dsd.id)) {
					mutex_unlock(&profiler_dsd_lock);
					return -EINVAL;
				}
				dom = &profiler.domains[dsd.id];
				dom->fn->get_power_change(dsd.id, dsd.freq_delta_pct,
						&dsd.freq, &dsd.dyn_power, &dsd.st_power);
				if (copy_to_user(user_dsd, &dsd, sizeof(struct delta_sharing_data)))
					pr_info("PROFILER : IOCTL_RW_DSD doesn't work!");
			}
			mutex_unlock(&profiler_dsd_lock);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int profiler_fops_release(struct inode *node, struct file *filp)
{
	return 0;
}

/******************************************************************************/
/*                                FPS functions                               */
/******************************************************************************/
void exynos_profiler_register_frame_cnt(void (*fn)(u64 *cnt, ktime_t *time))
{
	profiler.get_frame_cnt = fn;
}
EXPORT_SYMBOL_GPL(exynos_profiler_register_frame_cnt);

void exynos_profiler_register_vsync_cnt(void (*fn)(u64 *cnt, ktime_t *time))
{
	profiler.get_vsync_cnt = fn;
}
EXPORT_SYMBOL_GPL(exynos_profiler_register_vsync_cnt);

void exynos_profiler_register_fence_cnt(void (*fn)(u64 *cnt, ktime_t *time))
{
	profiler.get_fence_cnt = fn;
}
EXPORT_SYMBOL_GPL(exynos_profiler_register_fence_cnt);

void exynos_profiler_update_fps_change(u32 new_fps)
{
	if (profiler.max_fps != new_fps * 10) {
		profiler.max_fps = new_fps * 10;
		schedule_work(&profiler.fps_work);
	}
}
EXPORT_SYMBOL_GPL(exynos_profiler_update_fps_change);

static void profiler_fps_change_work(struct work_struct *work)
{
	tsd.max_fps = profiler.max_fps;
}

/******************************************************************************/
/*                             profile functions                              */
/******************************************************************************/
static void profiler_get_profile_data(struct domain_data *dom)
{
	s32 id = dom->id;

	/* dvfs */
	psd.max_freq[id] = dom->fn->get_max_freq(id);
	psd.min_freq[id] = dom->fn->get_min_freq(id);
	psd.freq[id] = dom->fn->get_freq(id);

	/* power */
	dom->fn->get_power(id, &psd.dyn_power[id], &psd.st_power[id]);

	/* temperature */
	psd.temp[id] = dom->fn->get_temp(id);

	/* active pct */
	psd.active_pct[id] = dom->fn->get_active_pct(id);

	/* private */
	if (id <= PROFILER_CL2) {
		struct private_data_cpu *private = dom->private;

		private->fn->cpu_active_pct(dom->id, psd.cpu_active_pct[dom->id]);
	} else if (id == PROFILER_GPU) {
		struct private_data_gpu *private = dom->private;
		int i;

		psd.swapcaller_id = private->fn->get_target_cpuid(&psd.cputracer);
		for (i = 0; i < NUM_OF_TIMEINFO; i++)
			psd.rtimes[i] = private->fn->get_timeinfo(i);
		for (i = 0; i < NUM_OF_PID; i++)
			psd.pid_list[i] = private->fn->get_pidinfo(i);
	} else if (id == PROFILER_MIF) {
		struct private_data_mif *private = dom->private;

		psd.mif_pm_qos_cur_freq = exynos_pm_qos_request(private->pm_qos_min_class);
		psd.stats0_sum = private->fn->get_stats0_sum();
		psd.stats0_ratio = private->fn->get_stats0_ratio();
		psd.stats0_avg = private->fn->get_stats0_avg();
		psd.stats1_sum = private->fn->get_stats1_sum();
		psd.stats1_ratio = private->fn->get_stats1_ratio();
		psd.stats2_sum = private->fn->get_stats2_sum();
		psd.stats2_ratio = private->fn->get_stats2_ratio();
		psd.llc_status = private->fn->get_llc_status();
	}
}

void profiler_set_tunable_data(void)
{
	struct domain_data *dom;
	s32 id;

	for_each_mdomain(dom, id) {
		tsd.enabled[id] = dom->enabled;

		if (id <= PROFILER_CL2) {
			struct private_data_cpu *private = dom->private;

			tsd.first_cpu[id] = private->pm_qos_cpu;
			tsd.num_of_cpu[id] = private->num_of_cpu;
			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;
			tsd.maxlock_high_limit[id] = private->pm_qos_max_freq;
			tsd.hp_minlock_low_limit[id] = private->hp_minlock_low_limit;
			tsd.lp_minlock_low_limit[id] = private->lp_minlock_low_limit;

			tsd.pid_util_max[id] = private->pid_util_max;
			tsd.pid_util_min[id] = private->pid_util_min;
			tsd.pid_util_lpmax[id] = private->pid_util_lpmax;
			tsd.pid_util_lpmin[id] = private->pid_util_lpmin;
		} else if (id == PROFILER_GPU) {
			struct private_data_gpu *private = dom->private;

			tsd.gpu_hw_status = private->fn->get_gpu_hw_status();
			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;
		} else if (id == PROFILER_MIF) {
			struct private_data_mif *private = dom->private;

			tsd.stats0_mode_min_freq = private->stats0_mode_min_freq;
			tsd.stats0_sum_thr = private->stats0_sum_thr;
			tsd.stats0_updown_delta_pct_thr = private->stats0_updown_delta_pct_thr;
			tsd.mif_hp_minlock_low_limit = private->hp_minlock_low_limit;
			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;

			tsd.pid_util_max[id] = private->pid_util_max;
			tsd.pid_util_min[id] = private->pid_util_min;
			tsd.pid_util_lpmax[id] = private->pid_util_lpmax;
			tsd.pid_util_lpmin[id] = private->pid_util_lpmin;
		}
	}
}

/******************************************************************************/
/*                              Profile functions                             */
/******************************************************************************/
extern void kbase_get_create_info(u64 *cnt, ktime_t *us);
static void control_llc(int value);

void profiler_update_profile(void)
{
	struct domain_data *dom;
	ktime_t time = 0;
	s32 id;

	if (!profiler.running)
		return;

	psd.monitor = tsd.monitor;

	profiler.end_time = ktime_get();
	psd.profile_time_ms = ktime_to_ms(ktime_sub(profiler.end_time, profiler.start_time));

	if (profiler.get_frame_cnt)
		profiler.get_frame_cnt(&profiler.end_frame_cnt, &time);
	psd.profile_frame_cnt = profiler.end_frame_cnt - profiler.start_frame_cnt;
	psd.frame_done_time_us = ktime_to_us(time);

	if (profiler.get_vsync_cnt)
		profiler.get_vsync_cnt(&profiler.end_frame_vsync_cnt, &time);

	psd.frame_vsync_time_us = ktime_to_us(time);

	exynos_profiler_get_frame_info(&psd.frame_cnt_by_swap,
			&psd.profile_frame_vsync_cnt,
			&psd.delta_ms_by_swap);

	if (profiler.get_fence_cnt)
		profiler.get_fence_cnt(&profiler.end_fence_cnt, &time);
	psd.profile_fence_cnt = profiler.end_fence_cnt - profiler.start_fence_cnt;
	psd.profile_fence_time_us = ktime_to_us(time);

	for_each_mdomain(dom, id) {
		dom->fn->update_mode(id, 1);
		profiler_get_profile_data(dom);
	}

	profiler.start_time = profiler.end_time;
	profiler.start_frame_cnt = profiler.end_frame_cnt;
	profiler.start_frame_vsync_cnt = profiler.end_frame_vsync_cnt;
	profiler.start_fence_cnt = profiler.end_fence_cnt;
}

void profiler_start_profile(u32 mode)
{
	struct domain_data *dom;
	ktime_t time = 0;
	s32 id;

	if (profiler.running)
		return;

	profiler.running = true;
	profiler.cur_mode = mode;
	profiler.start_time = ktime_get();
	if (profiler.get_frame_cnt)
		profiler.get_frame_cnt(&profiler.start_frame_cnt, &time);
	if (profiler.get_vsync_cnt)
		profiler.get_vsync_cnt(&profiler.start_frame_vsync_cnt, &time);

	if (!(tsd.profile_only || mode == PROFILE_START_BY_GPU)) {
		if (is_enabled(PROFILER_CL0) || is_enabled(PROFILER_CL1L) || is_enabled(PROFILER_CL1H) || is_enabled(PROFILER_CL2))
			exynos_dm_dynamic_disable(1);

		if (is_enabled(PROFILER_GPU))
			exynos_profiler_set_profiler_governor(1);

		if (tsd.dyn_mo_control && profiler.bts_idx)
			bts_add_scenario(profiler.bts_idx);
	}

	for_each_mdomain(dom, id) {
		dom->fn->update_mode(dom->id, 1);

		if (tsd.profile_only || mode == PROFILE_START_BY_GPU)
			continue;

		if (id <= PROFILER_CL2) {
			struct private_data_cpu *private = dom->private;

			if (freq_qos_request_active(&private->pm_qos_min_req)) {
				freq_qos_update_request(&private->pm_qos_min_req,
						private->pm_qos_min_freq);
			}
			if (freq_qos_request_active(&private->pm_qos_max_req)) {
				freq_qos_update_request(&private->pm_qos_max_req, 0);
				freq_qos_update_request(&private->pm_qos_max_req,
						private->pm_qos_max_freq);
			}
		} else if (id == PROFILER_MIF) {
			struct private_data_mif *private = dom->private;
			if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
				exynos_pm_qos_update_request(&private->pm_qos_max_req,
							private->pm_qos_max_freq);
			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req,
							private->pm_qos_min_freq);
		} else {
			struct private_data_gpu *private = dom->private;
			if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
				exynos_pm_qos_update_request(&private->pm_qos_max_req,
							private->pm_qos_max_freq);
			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req,
							private->pm_qos_min_freq);
		}
	}

	profiler_set_tunable_data();
}

void profiler_finish_profile(void)
{
	struct domain_data *dom;
	s32 id;

	if (!profiler.running)
		return;

	if (!tsd.profile_only) {
		if (is_enabled(PROFILER_CL0) || is_enabled(PROFILER_CL1L) || is_enabled(PROFILER_CL1H) || is_enabled(PROFILER_CL2))
			exynos_dm_dynamic_disable(0);

		if (is_enabled(PROFILER_GPU))
			exynos_profiler_set_profiler_governor(0);

		if (tsd.dyn_mo_control && profiler.bts_idx)
			bts_del_scenario(profiler.bts_idx);
	}

	for_each_mdomain(dom, id) {
		dom->fn->update_mode(id, 0);

		if (tsd.profile_only)
			continue;

		if (id <= PROFILER_CL2) {
			struct private_data_cpu *private = dom->private;

			if (freq_qos_request_active(&private->pm_qos_min_req))
				freq_qos_update_request(&private->pm_qos_min_req,
						PM_QOS_DEFAULT_VALUE);
			if (freq_qos_request_active(&private->pm_qos_max_req))
				freq_qos_update_request(&private->pm_qos_max_req,
						PM_QOS_DEFAULT_VALUE);
		} else if (id == PROFILER_MIF) {
			struct private_data_mif *private = dom->private;

			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req,
						EXYNOS_PM_QOS_DEFAULT_VALUE);

			if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
				exynos_pm_qos_update_request(&private->pm_qos_max_req,
							PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
		} else {
			struct private_data_gpu *private = dom->private;
			/* gpu use negative value for unlock */
			if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
				exynos_pm_qos_update_request(&private->pm_qos_max_req, EXYNOS_PM_QOS_DEFAULT_VALUE);
			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req, EXYNOS_PM_QOS_DEFAULT_VALUE);
		}
	}

	if (profiler.llc_enable) {
		control_llc(false);
	}

	profiler.running = false;
	profiler.cur_mode = PROFILE_FINISH;
}

static void wakeup_polling_task(void)
{
	if (profiler.disable && !profiler.running)
		return;

	if (profiler.game_mode)
		profiler.running_event = PROFILE_START_BY_GAME;
	else if (profiler.heavy_gpu_mode)
		profiler.running_event = PROFILE_START_BY_GPU;
	else
		profiler.running_event = PROFILE_FINISH;

	wake_up_interruptible(&profiler.wq);
}

static void profiler_fragutil_change_work(struct work_struct *work)
{
	u32 gpu_freq = exynos_profiler_get_cur_clock();
	s64 elapsed_ms = 0;
	bool is_gpu_heavy = false;

	if (profiler.fragutil >= profiler.fragutil_upper_thr && gpu_freq >= profiler.gpu_freq_thr)
		is_gpu_heavy = true;
	else if (profiler.fragutil <= profiler.fragutil_lower_thr || gpu_freq < profiler.gpu_freq_thr)
		is_gpu_heavy = false;
	else
		return;

	if (profiler.was_gpu_heavy != is_gpu_heavy) {
		profiler.was_gpu_heavy = is_gpu_heavy;
		profiler.heavy_gpu_start_time = profiler.heavy_gpu_end_time;
	}
	profiler.heavy_gpu_end_time = ktime_get();

	if ((!is_gpu_heavy && profiler.cur_mode == PROFILE_START_BY_GPU)
			|| (is_gpu_heavy && profiler.cur_mode == PROFILE_FINISH)) {
		elapsed_ms = ktime_to_ms(ktime_sub(profiler.heavy_gpu_end_time,
					profiler.heavy_gpu_start_time));
		if (elapsed_ms >= profiler.heavy_gpu_ms_thr) {
			profiler.heavy_gpu_mode = is_gpu_heavy;
			wakeup_polling_task();
		}
	}
}

static int profiler_fragutil_change_callback(struct notifier_block *nb,
				unsigned long val, void *info)
{
	int util = *(int *)info;

	if (util > 100)
		return NOTIFY_DONE;

	profiler.fragutil = util;
	schedule_work(&profiler.fragutil_work);

	return NOTIFY_OK;
}

static struct notifier_block profiler_fragutil_change_notifier = {
	.notifier_call = profiler_fragutil_change_callback,
};

/******************************************************************************/
/*                               SYSFS functions                              */
/******************************************************************************/
/* only for fps profile without profiler running and profile only mode */
static ssize_t fps_profile_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s32 ret = 0;
	u64 fps_ = fps_profile.fps / 10;
	u64 _fps = fps_profile.fps % 10;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_time: %llu sec\n", fps_profile.profile_time);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "fps: %llu.%llu\n", fps_, _fps);

	return ret;
}
static ssize_t fps_profile_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 mode;

	if (sscanf(buf, "%u", &mode) != 1)
		return -EINVAL;

	if (!profiler.get_frame_cnt)
		return -EINVAL;

	/* start */
	if (mode) {
		profiler.get_frame_cnt(&fps_profile.fps, &fps_profile.profile_time);
	} else {
		u64 cur_cnt;
		ktime_t cur_time;

		profiler.get_frame_cnt(&cur_cnt, &cur_time);
		fps_profile.profile_time = ktime_sub(cur_time, fps_profile.profile_time) / NSEC_PER_SEC;
		fps_profile.fps = ((cur_cnt - fps_profile.fps) * 10) / fps_profile.profile_time;
	}

	return count;
}
static DEVICE_ATTR_RW(fps_profile);

static ssize_t running_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", profiler.running);
}
static ssize_t running_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 mode;

	if (sscanf(buf, "%u", &mode) != 1)
		return -EINVAL;

	profiler.game_mode = (bool)mode;
	profiler.forced_running = (bool) mode;
	wakeup_polling_task();

	return count;
}
static DEVICE_ATTR_RW(running);

#define PROFILER_ATTR_RW(_name)							\
static ssize_t _name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)			\
{										\
	return snprintf(buf, PAGE_SIZE, "%d\n", profiler._name);			\
}										\
static ssize_t _name##_store(struct device *dev,				\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	s32 val;								\
										\
	if (sscanf(buf, "%d", &val) != 1)					\
		return -EINVAL;							\
										\
	profiler._name = val;							\
										\
	return count;								\
}										\
static DEVICE_ATTR_RW(_name);

PROFILER_ATTR_RW(disable);
PROFILER_ATTR_RW(gpu_freq_thr);
PROFILER_ATTR_RW(heavy_gpu_ms_thr);

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

TUNABLE_ATTR_RW(version);
TUNABLE_ATTR_RW(monitor);
TUNABLE_ATTR_RW(profile_only);
TUNABLE_ATTR_RW(window_period);
TUNABLE_ATTR_RW(window_number);
TUNABLE_ATTR_RW(active_pct_thr);
TUNABLE_ATTR_RW(valid_freq_delta_pct);
TUNABLE_ATTR_RW(min_sensitivity);
TUNABLE_ATTR_RW(cpu_bottleneck_thr);
TUNABLE_ATTR_RW(gpu_bottleneck_thr);
TUNABLE_ATTR_RW(mif_bottleneck_thr);
TUNABLE_ATTR_RW(dt_ctrl_en);
TUNABLE_ATTR_RW(dt_over_thr);
TUNABLE_ATTR_RW(dt_under_thr);
TUNABLE_ATTR_RW(dt_up_step);
TUNABLE_ATTR_RW(dt_down_step);
TUNABLE_ATTR_RW(dpat_upper_thr);
TUNABLE_ATTR_RW(dpat_lower_thr);
TUNABLE_ATTR_RW(dpat_lower_cnt_thr);
TUNABLE_ATTR_RW(dpat_up_step);
TUNABLE_ATTR_RW(dpat_down_step);
TUNABLE_ATTR_RW(inc_perf_temp_thr);
TUNABLE_ATTR_RW(inc_perf_power_thr);
TUNABLE_ATTR_RW(inc_perf_thr);
TUNABLE_ATTR_RW(dec_perf_thr);
TUNABLE_ATTR_RW(hp_minlock_fps_delta_pct_thr);
TUNABLE_ATTR_RW(hp_minlock_power_upper_thr);
TUNABLE_ATTR_RW(hp_minlock_power_lower_thr);
TUNABLE_ATTR_RW(dyn_mo_control);

static ssize_t stc_config_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return exynos_profiler_stc_config_show(PAGE_SIZE, buf);
}

static ssize_t stc_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	exynos_profiler_stc_config_store(buf);
	return count;
}
static DEVICE_ATTR_RW(stc_config);


#define PER_CPU_TUNABLE_ATTR_RW(_name)						\
static ssize_t _name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)			\
{										\
	struct domain_data *dom;						\
	s32 id, ret = 0;							\
										\
	for_each_mdomain(dom, id) {						\
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
	if (!is_enabled(id))							\
		return -EINVAL;							\
	tsd._name[id] = val;							\
										\
	return count;								\
}										\
static DEVICE_ATTR_RW(_name);

PER_CPU_TUNABLE_ATTR_RW(max_margin);
PER_CPU_TUNABLE_ATTR_RW(min_margin);
PER_CPU_TUNABLE_ATTR_RW(margin_up_step);
PER_CPU_TUNABLE_ATTR_RW(margin_down_step);
PER_CPU_TUNABLE_ATTR_RW(margin_default_step);

static ssize_t runtime_thr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret += snprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d\n",
			tsd.runtime_thr[0], tsd.runtime_thr[1], tsd.runtime_thr[2],
			tsd.runtime_thr[3], tsd.runtime_thr[4], tsd.runtime_thr[5]);

	return ret;
}
static ssize_t runtime_thr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int index;
	int val;

	if (sscanf(buf, "%u %d", &index, &val) != 2)
		return -EINVAL;

	if (index >= 6)
		return -EINVAL;

	tsd.runtime_thr[index] = val;

	return count;
}
static DEVICE_ATTR_RW(runtime_thr);

static ssize_t pid_cpu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret += snprintf(buf, PAGE_SIZE, "%d %d %d\n",
			tsd.pid_cpu[0], tsd.pid_cpu[1], tsd.pid_cpu[2]);
	return ret;
}
static ssize_t pid_cpu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int index;
	int val;

	if (sscanf(buf, "%u %d", &index, &val) != 2)
                return -EINVAL;

	if (index >= 3)
                return -EINVAL;

	tsd.pid_cpu[index] = val;

	return count;
}
static DEVICE_ATTR_RW(pid_cpu);

static ssize_t pid_gpu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret += snprintf(buf, PAGE_SIZE, "%d %d %d\n",
			tsd.pid_gpu[0], tsd.pid_gpu[1], tsd.pid_gpu[2]);
	return ret;
}
static ssize_t pid_gpu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int index;
	int val;

	if (sscanf(buf, "%u %d", &index, &val) != 2)
                return -EINVAL;

	if (index >= 3)
                return -EINVAL;

	tsd.pid_gpu[index] = val;

	return count;
}
static DEVICE_ATTR_RW(pid_gpu);

static ssize_t pid_util_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret += snprintf(buf, PAGE_SIZE, "%d %d %d\n",
			tsd.pid_util[0], tsd.pid_util[1], tsd.pid_util[2]);
	return ret;
}
static ssize_t pid_util_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int index;
	int val;

	if (sscanf(buf, "%u %d", &index, &val) != 2)
                return -EINVAL;

	if (index >= 3)
                return -EINVAL;

	tsd.pid_util[index] = val;

	return count;
}
static DEVICE_ATTR_RW(pid_util);

static struct attribute *profiler_attrs[] = {
	&dev_attr_running.attr,
	&dev_attr_fps_profile.attr,
	&dev_attr_profile_only.attr,
	&dev_attr_version.attr,
	&dev_attr_monitor.attr,
	&dev_attr_disable.attr,
	&dev_attr_window_period.attr,
	&dev_attr_window_number.attr,
	&dev_attr_active_pct_thr.attr,
	&dev_attr_valid_freq_delta_pct.attr,
	&dev_attr_min_sensitivity.attr,
	&dev_attr_cpu_bottleneck_thr.attr,
	&dev_attr_gpu_bottleneck_thr.attr,
	&dev_attr_mif_bottleneck_thr.attr,
	&dev_attr_dt_ctrl_en.attr,
	&dev_attr_dt_over_thr.attr,
	&dev_attr_dt_under_thr.attr,
	&dev_attr_dt_up_step.attr,
	&dev_attr_dt_down_step.attr,
	&dev_attr_dpat_upper_thr.attr,
	&dev_attr_dpat_lower_thr.attr,
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
	&dev_attr_inc_perf_thr.attr,
	&dev_attr_dec_perf_thr.attr,
	&dev_attr_gpu_freq_thr.attr,
	&dev_attr_heavy_gpu_ms_thr.attr,
	&dev_attr_hp_minlock_fps_delta_pct_thr.attr,
	&dev_attr_hp_minlock_power_upper_thr.attr,
	&dev_attr_hp_minlock_power_lower_thr.attr,
	&dev_attr_dyn_mo_control.attr,
	&dev_attr_runtime_thr.attr,
	&dev_attr_stc_config.attr,
	&dev_attr_pid_cpu.attr,
	&dev_attr_pid_gpu.attr,
	&dev_attr_pid_util.attr,
	NULL,
};
static struct attribute_group profiler_attr_group = {
	.name = "profiler",
	.attrs = profiler_attrs,
};

#define PRIVATE_ATTR_RW(_priv, _id, _ip, _name)					\
static ssize_t _ip##_##_name##_show(struct device *dev,				\
		struct device_attribute *attr, char *buf)			\
{										\
	struct domain_data *dom = &profiler.domains[_id];				\
	struct private_data_##_priv *private;					\
										\
	if (!dom)								\
		return 0;							\
	private = dom->private;							\
	return snprintf(buf, PAGE_SIZE, "%d\n", (int)private->_name);		\
}										\
static ssize_t _ip##_##_name##_store(struct device *dev,			\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	struct domain_data *dom = &profiler.domains[_id];				\
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

CPU_PRIVATE_ATTR_RW(PROFILER_CL0, cl0, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(PROFILER_CL0, cl0, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(PROFILER_CL0, cl0, hp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(PROFILER_CL0, cl0, lp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(PROFILER_CL0, cl0, pid_util_max);
CPU_PRIVATE_ATTR_RW(PROFILER_CL0, cl0, pid_util_min);
CPU_PRIVATE_ATTR_RW(PROFILER_CL0, cl0, pid_util_lpmax);
CPU_PRIVATE_ATTR_RW(PROFILER_CL0, cl0, pid_util_lpmin);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1L, cl1l, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1L, cl1l, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1L, cl1l, hp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1L, cl1l, lp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1L, cl1l, pid_util_max);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1L, cl1l, pid_util_min);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1L, cl1l, pid_util_lpmax);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1L, cl1l, pid_util_lpmin);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1H, cl1h, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1H, cl1h, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1H, cl1h, hp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1H, cl1h, lp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1H, cl1h, pid_util_max);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1H, cl1h, pid_util_min);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1H, cl1h, pid_util_lpmax);
CPU_PRIVATE_ATTR_RW(PROFILER_CL1H, cl1h, pid_util_lpmin);
CPU_PRIVATE_ATTR_RW(PROFILER_CL2, cl2, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(PROFILER_CL2, cl2, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(PROFILER_CL2, cl2, hp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(PROFILER_CL2, cl2, lp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(PROFILER_CL2, cl2, pid_util_max);
CPU_PRIVATE_ATTR_RW(PROFILER_CL2, cl2, pid_util_min);
CPU_PRIVATE_ATTR_RW(PROFILER_CL2, cl2, pid_util_lpmax);
CPU_PRIVATE_ATTR_RW(PROFILER_CL2, cl2, pid_util_lpmin);
GPU_PRIVATE_ATTR_RW(PROFILER_GPU, gpu, gpu_active_pct_thr);
GPU_PRIVATE_ATTR_RW(PROFILER_GPU, gpu, pm_qos_max_freq);
GPU_PRIVATE_ATTR_RW(PROFILER_GPU, gpu, pm_qos_min_freq);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, stats0_mode_min_freq);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, stats0_sum_thr);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, stats0_updown_delta_pct_thr);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, hp_minlock_low_limit);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, pm_qos_max_freq);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, pm_qos_min_freq);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, pid_util_max);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, pid_util_min);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, pid_util_lpmax);
MIF_PRIVATE_ATTR_RW(PROFILER_MIF, mif, pid_util_lpmin);

#define MAXNUM_OF_PRIV_ATTR 12
static struct attribute *private_attrs[NUM_OF_DOMAIN][MAXNUM_OF_PRIV_ATTR] = {
	{
		&dev_attr_cl0_pm_qos_min_freq.attr,
		&dev_attr_cl0_pm_qos_max_freq.attr,
		&dev_attr_cl0_hp_minlock_low_limit.attr,
		&dev_attr_cl0_lp_minlock_low_limit.attr,
		&dev_attr_cl0_pid_util_max.attr,
		&dev_attr_cl0_pid_util_min.attr,
		&dev_attr_cl0_pid_util_lpmax.attr,
		&dev_attr_cl0_pid_util_lpmin.attr,
		NULL,
	},
	{
		&dev_attr_cl1l_pm_qos_min_freq.attr,
		&dev_attr_cl1l_pm_qos_max_freq.attr,
		&dev_attr_cl1l_hp_minlock_low_limit.attr,
		&dev_attr_cl1l_lp_minlock_low_limit.attr,
		&dev_attr_cl1l_pid_util_max.attr,
		&dev_attr_cl1l_pid_util_min.attr,
		&dev_attr_cl1l_pid_util_lpmax.attr,
		&dev_attr_cl1l_pid_util_lpmin.attr,
		NULL,
	},
	{
		&dev_attr_cl1h_pm_qos_min_freq.attr,
		&dev_attr_cl1h_pm_qos_max_freq.attr,
		&dev_attr_cl1h_hp_minlock_low_limit.attr,
		&dev_attr_cl1h_lp_minlock_low_limit.attr,
		&dev_attr_cl1h_pid_util_max.attr,
		&dev_attr_cl1h_pid_util_min.attr,
		&dev_attr_cl1h_pid_util_lpmax.attr,
		&dev_attr_cl1h_pid_util_lpmin.attr,
		NULL,
	},
	{
		&dev_attr_cl2_pm_qos_min_freq.attr,
		&dev_attr_cl2_pm_qos_max_freq.attr,
		&dev_attr_cl2_hp_minlock_low_limit.attr,
		&dev_attr_cl2_lp_minlock_low_limit.attr,
		&dev_attr_cl2_pid_util_max.attr,
		&dev_attr_cl2_pid_util_min.attr,
		&dev_attr_cl2_pid_util_lpmax.attr,
		&dev_attr_cl2_pid_util_lpmin.attr,
		NULL,
	},
	{
		&dev_attr_gpu_pm_qos_max_freq.attr,
		&dev_attr_gpu_pm_qos_min_freq.attr,
		&dev_attr_gpu_gpu_active_pct_thr.attr,
		NULL,
	},
	{
		&dev_attr_mif_stats0_mode_min_freq.attr,
		&dev_attr_mif_stats0_sum_thr.attr,
		&dev_attr_mif_stats0_updown_delta_pct_thr.attr,
		&dev_attr_mif_hp_minlock_low_limit.attr,
		&dev_attr_mif_pm_qos_max_freq.attr,
		&dev_attr_mif_pm_qos_min_freq.attr,
		&dev_attr_mif_pid_util_max.attr,
		&dev_attr_mif_pid_util_min.attr,
		&dev_attr_mif_pid_util_lpmax.attr,
		&dev_attr_mif_pid_util_lpmin.attr,
		NULL,
	},
};

#define RATIO_UNIT	1000
static ssize_t set_margin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static void control_max_qos(int id, int value)
{
	struct domain_data *dom = &profiler.domains[id];

	switch (id) {
	case PROFILER_CL0:
	case PROFILER_CL1L:
	case PROFILER_CL1H:
	case PROFILER_CL2:
		{
		struct private_data_cpu *private = dom->private;
		if (freq_qos_request_active(&private->pm_qos_max_req))
			freq_qos_update_request(&private->pm_qos_max_req, value);
		}
		break;
	case PROFILER_MIF:
		{
		struct private_data_mif *private = dom->private;
		if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
			exynos_pm_qos_update_request(&private->pm_qos_max_req, value);
		}
		break;
	case PROFILER_GPU:
		{
		struct private_data_gpu *private = dom->private;
		if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
			exynos_pm_qos_update_request(&private->pm_qos_max_req, value);
		}
		break;
	default:
		break;
	}
}

static void control_min_qos(int id, int value)
{
	struct domain_data *dom = &profiler.domains[id];

	switch (id) {
	case PROFILER_CL0:
	case PROFILER_CL1L:
	case PROFILER_CL1H:
	case PROFILER_CL2:
		{
		struct private_data_cpu *private = dom->private;
		if (freq_qos_request_active(&private->pm_qos_min_req))
			freq_qos_update_request(&private->pm_qos_min_req, value);
		}
		break;
	case PROFILER_MIF:
		{
		struct private_data_mif *private = dom->private;
		if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
			exynos_pm_qos_update_request(&private->pm_qos_min_req, value);
		}
		break;
	case PROFILER_GPU:
		{
		struct private_data_gpu *private = dom->private;
		if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
			exynos_pm_qos_update_request(&private->pm_qos_min_req, value);
		}
		break;
	default:
		break;
	}
}

static void control_margin(int id, int value)
{
	struct domain_data *dom = &profiler.domains[id];

	/* CPU uses pelt margin via ems service */
	if (id <= PROFILER_CL2)
		return;

	if (likely(dom->fn->set_margin))
		dom->fn->set_margin(id, value);
}
static void control_mo(int id, int value)
{
	int idx;

	if (!profiler.bts_idx || !profiler.len_mo_id)
		return;

	for (idx = 0; idx < profiler.len_mo_id; idx++)
		bts_change_mo(profiler.bts_idx, profiler.mo_id[idx], value, value);

	return;
}

static void control_llc(int value)
{
	if (profiler.llc_enable == !!value)
		return;

	profiler.llc_enable = !!value;

	if (profiler.llc_enable) {
		llc_enable(profiler.llc_enable);
		llc_region_alloc(LLC_REGION_PROFILER, 1, 16);
	} else {
		llc_region_alloc(LLC_REGION_PROFILER, 0, 0);
		llc_enable(profiler.llc_enable);
	}
}

static void control_rtp(int id, int value)
{
	struct domain_data *dom = &profiler.domains[PROFILER_GPU];
	struct private_data_gpu *private = dom->private;
	switch (id) {
	case OP_RTP_TARGETFRAMETIME:
		private->fn->set_targetframetime(value);
		break;
	case OP_RTP_TARGETTIMEMARGIN:
		private->fn->set_targettime_margin(value);
		break;
	case OP_RTP_UTIL_MARGIN:
		private->fn->set_util_margin(value);
		break;
	case OP_RTP_DECON_TIME:
		private->fn->set_decon_time(value);
		break;
	case OP_RTP_COMBCTRL:
		private->fn->set_comb_ctrl(value);
		break;
	case OP_CPUTRACER_SIZE:
		private->fn->set_cputracer_size(value);
		break;
	}
}

/* kernel only to decode */
#define get_op_code(x) (((x) >> OP_CODE_SHIFT) & OP_CODE_MASK)
#define get_ip_id(x) ((x) & IP_ID_MASK)
static ssize_t set_margin_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 id, op, ctrl, value;

	if (tsd.profile_only)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &ctrl, &value) != 2)
		return -EINVAL;

	id = get_ip_id(ctrl);
	op = get_op_code(ctrl);

	if (!is_enabled(id) || op == OP_INVALID)
		return -EINVAL;

	switch (op) {
	case OP_PM_QOS_MAX:
		control_max_qos(id, value);
		break;
	case OP_PM_QOS_MIN:
		control_min_qos(id, value);
		break;
	case OP_MARGIN:
		control_margin(id, value);
		break;
	case OP_MO:
		control_mo(id, value);
		break;
	case OP_LLC:
		control_llc(value);
		break;
	default:
		control_rtp(op, value);
		break;
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
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "user_target_fps=%d\n", psd.user_target_fps);
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
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "times=%llu %llu %llu %llu %llu\n",
			psd.rtimes[PREAVG], psd.rtimes[CPUAVG], psd.rtimes[V2SAVG],
			psd.rtimes[GPUAVG], psd.rtimes[V2FAVG]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats0_sum=%llu\n", psd.stats0_sum);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats0_avg=%llu\n", psd.stats0_avg);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats_ratio=%llu\n", psd.stats0_ratio);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "mif_pm_qos_cur_freq=%d\n", psd.mif_pm_qos_cur_freq);

	return ret;
}
static ssize_t control_profile_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 mode;

	if (sscanf(buf, "%u", &mode) != 1)
		return -EINVAL;

	if (!profiler.running && (mode == PROFILE_START_BY_GAME || mode == PROFILE_START_BY_GPU))
		profiler_start_profile(mode);
	else if (profiler.running && mode == PROFILE_FINISH)
		profiler_finish_profile();
	else if (profiler.running && mode == PROFILE_UPDATE)
		profiler_update_profile();

	return count;
}
static DEVICE_ATTR_RW(control_profile);

#define FRAGUTIL_THR_MARGIN	70
static ssize_t fragutil_thr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "upper=%d lower=%d\n",
			profiler.fragutil_upper_thr, profiler.fragutil_lower_thr);
}
static ssize_t fragutil_thr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 val;

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	if (val > 100 | val < 0)
		return -EINVAL;

	profiler.fragutil_upper_thr = val;
	profiler.fragutil_lower_thr = val * FRAGUTIL_THR_MARGIN / 100;
	profiler.fragutil_lower_thr = max(0, profiler.fragutil_lower_thr);

	return count;
}
static DEVICE_ATTR_RW(fragutil_thr);

static ssize_t user_target_fps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", psd.user_target_fps / 10);
}
static ssize_t user_target_fps_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 val;

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	psd.user_target_fps = val * 10;

	return count;
}
static DEVICE_ATTR_RW(user_target_fps);

static struct attribute *control_attrs[] = {
	&dev_attr_set_margin.attr,
	&dev_attr_control_profile.attr,
	&dev_attr_fragutil_thr.attr,
	&dev_attr_user_target_fps.attr,
	NULL,
};
static struct attribute_group control_attr_group = {
	.name = "control",
	.attrs = control_attrs,
};

/*********************************************************************/
/*		      PROFILER mode change notifier		     */
/*********************************************************************/
static int profiler_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *set = (struct emstune_set *)v;

	if (profiler.forced_running)
		return NOTIFY_OK;

	if (!profiler.game_mode) {
		/* start profiler with game when */
		if (set->mode == EMSTUNE_GAME_MODE && set->level != EMSTUNE_BOOST_LEVEL) {
			profiler.game_mode = true;
			wakeup_polling_task();
		}
	} else {
		/* finish profiler */
		if (set->mode != EMSTUNE_GAME_MODE || set->level == EMSTUNE_BOOST_LEVEL) {
			profiler.game_mode = false;
			wakeup_polling_task();
		}
	}

	return NOTIFY_OK;
}
static struct notifier_block profiler_mode_update_notifier = {
	.notifier_call = profiler_mode_update_callback,
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
		pr_err("profiler: Failed to get node of domain(id=%d)\n", dom->id);
		return -ENODEV;
	}

	tsd.freq_table_cnt[id] = dom->fn->get_table_cnt(id);
	cnt = dom->fn->get_freq_table(id, tsd.freq_table[id]);
	if (tsd.freq_table_cnt[id] != cnt) {
		pr_err("profiler: table cnt(%u) is un-synced with freq table cnt(%u)(id=%d)\n",
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

	if (id <= PROFILER_CL2) {
		struct private_data_cpu *private = dom->private;
		struct cpufreq_policy *policy;

		private = kzalloc(sizeof(struct private_data_cpu), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		tsd.asv_ids[id] = private->fn->cpu_asv_ids(id);

		ret |= of_property_read_s32(dn, "pm-qos-cpu", &private->pm_qos_cpu);
		policy = cpufreq_cpu_get(private->pm_qos_cpu);
		if (!policy) {
			kfree(private);
			return -EINVAL;
		}
		private->num_of_cpu = cpumask_weight(policy->related_cpus);

		if (of_property_read_s32(dn, "pm-qos-min-freq", &private->pm_qos_min_freq))
			private->pm_qos_min_freq = PM_QOS_DEFAULT_VALUE;
		if (of_property_read_s32(dn, "pm-qos-max-freq", &private->pm_qos_max_freq))
			private->pm_qos_max_freq = PM_QOS_DEFAULT_VALUE;
		if (of_property_read_s32(dn, "hp-minlock-low-limit", &private->hp_minlock_low_limit))
			private->hp_minlock_low_limit = PM_QOS_DEFAULT_VALUE;
		if (of_property_read_s32(dn, "lp-minlock-low-limit", &private->lp_minlock_low_limit))
			private->lp_minlock_low_limit = PM_QOS_DEFAULT_VALUE;

		if (of_property_read_s32(dn, "pid-util-max", &private->pid_util_max))
			private->pid_util_max = 700;
		if (of_property_read_s32(dn, "pid-util-min", &private->pid_util_min))
			private->pid_util_min = 600;
		if (of_property_read_s32(dn, "pid-util-lpmax", &private->pid_util_lpmax))
			private->pid_util_lpmax = 900;
		if (of_property_read_s32(dn, "pid-util-lpmin", &private->pid_util_lpmin))
			private->pid_util_lpmin = 800;

		freq_qos_tracer_add_request(&policy->constraints, &private->pm_qos_min_req,
				FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);
		freq_qos_tracer_add_request(&policy->constraints, &private->pm_qos_max_req,
				FREQ_QOS_MAX, PM_QOS_DEFAULT_VALUE);

		if (!ret) {
			dom->private = private;
		} else {
			kfree(private);
		}
	} else if (dom->id == PROFILER_GPU) {
		struct private_data_gpu *private;
		s32 val;

		private = kzalloc(sizeof(struct private_data_gpu), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "pm-qos-max-class",
				&private->pm_qos_max_class);
		if (of_property_read_s32(dn, "pm-qos-max-freq", &private->pm_qos_max_freq))
			private->pm_qos_max_freq = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;

		ret |= of_property_read_s32(dn, "pm-qos-min-class",
				&private->pm_qos_min_class);
		if (of_property_read_s32(dn, "pm-qos-min-freq", &private->pm_qos_min_freq))
			private->pm_qos_min_freq = PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE;

		ret |= of_property_read_s32(dn, "active-pct-thr", &val);
		private->gpu_active_pct_thr = val;

		if (!ret) {
			/* gpu use negative value for unlock */
			exynos_pm_qos_add_request(&private->pm_qos_min_req,
					private->pm_qos_min_class, EXYNOS_PM_QOS_DEFAULT_VALUE);
			exynos_pm_qos_add_request(&private->pm_qos_max_req,
					private->pm_qos_max_class, EXYNOS_PM_QOS_DEFAULT_VALUE);

			dom->private = private;
		} else {
			kfree(private);
		}
	} else if (dom->id == PROFILER_MIF) {
		struct private_data_mif *private;
		s32 val;

		private = kzalloc(sizeof(struct private_data_mif), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "pm-qos-max-class", &private->pm_qos_max_class);
		ret |= of_property_read_s32(dn, "pm-qos-min-class", &private->pm_qos_min_class);
		ret |= of_property_read_s32(dn, "freq-stats0-mode-min-freq",
				&private->stats0_mode_min_freq);
		ret |= of_property_read_s32(dn, "freq-stats0-thr", &val);
		private->stats0_sum_thr = val;
		ret |= of_property_read_s32(dn, "freq-stats0-updown-delta-pct-thr", &val);
		private->stats0_updown_delta_pct_thr = val;
		ret |= of_property_read_s32(dn, "hp-minlock-low-limit", &private->hp_minlock_low_limit);

		if (of_property_read_s32(dn, "pm-qos-max-freq", &private->pm_qos_max_freq))
			private->pm_qos_max_freq = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;
		if (of_property_read_s32(dn, "pm-qos-min-freq", &private->pm_qos_min_freq))
			private->pm_qos_min_freq = PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE;

		if (of_property_read_s32(dn, "pid-util-max", &private->pid_util_max))
			private->pid_util_max = 980;
		if (of_property_read_s32(dn, "pid-util-min", &private->pid_util_min))
			private->pid_util_min = 920;
		if (of_property_read_s32(dn, "pid-util-max", &private->pid_util_lpmax))
			private->pid_util_lpmax = 980;
		if (of_property_read_s32(dn, "pid-util-min", &private->pid_util_lpmin))
			private->pid_util_lpmin = 950;

		if (!ret) {
			exynos_pm_qos_add_request(&private->pm_qos_min_req,
					private->pm_qos_min_class, EXYNOS_PM_QOS_DEFAULT_VALUE);
			exynos_pm_qos_add_request(&private->pm_qos_max_req,
					private->pm_qos_max_class, EXYNOS_PM_QOS_DEFAULT_VALUE);

			dom->private = private;
		} else {
			kfree(private);
		}
	}

	if (!ret) {
		dom->attr_group.name = domain_name[dom->id];
		dom->attr_group.attrs = private_attrs[dom->id];
		ret+= sysfs_create_group(profiler.kobj, &dom->attr_group);
	}

	return 0;
}

s32 exynos_profiler_register_misc_device(void)
{
	s32 ret;

	profiler.gov_fops.fops.owner		= THIS_MODULE;
	profiler.gov_fops.fops.llseek		= no_llseek;
	profiler.gov_fops.fops.read		= profiler_fops_read;
	profiler.gov_fops.fops.poll		= profiler_fops_poll;
	profiler.gov_fops.fops.unlocked_ioctl	= profiler_fops_ioctl;
	profiler.gov_fops.fops.compat_ioctl	= profiler_fops_ioctl;
	profiler.gov_fops.fops.release		= profiler_fops_release;

	profiler.gov_fops.miscdev.minor		= MISC_DYNAMIC_MINOR;
	profiler.gov_fops.miscdev.name		= "exynos-profiler";
	profiler.gov_fops.miscdev.fops		= &profiler.gov_fops.fops;

	ret = misc_register(&profiler.gov_fops.miscdev);
        if (ret) {
                pr_err("exynos-profiler couldn't register misc device!");
                return ret;
        }

	return 0;
}

s32 exynos_profiler_register_domain(s32 id, struct domain_fn *fn, void *private_fn)
{
	struct domain_data *dom = &profiler.domains[id];

	if (id >= NUM_OF_DOMAIN || (dom->enabled)) {
		pr_err("profiler: invalid id or duplicated register (id: %d)\n", id);
		return -EINVAL;
	}

	if (!fn) {
		pr_err("profiler: there is no callback address (id: %d)\n", id);
		return -EINVAL;
	}

	dom->id = id;
	dom->fn = fn;

	if (init_domain_data(profiler.dn, dom, private_fn)) {
		pr_err("profiler: failed to init domain data(id=%d)\n", id);
		return -EINVAL;
	}

	dom->enabled = true;

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_profiler_register_domain);

static s32 parse_profiler_dt(struct device_node *dn)
{
	char name[16];
	s32 ret = 0;
	s32 i;

	ret |= of_property_read_s32(dn, "version", &tsd.version);
	ret |= of_property_read_s32(dn, "window-period", &tsd.window_period);
	ret |= of_property_read_s32(dn, "window-number", &tsd.window_number);
	ret |= of_property_read_s32(dn, "active-pct-thr", &tsd.active_pct_thr);
	ret |= of_property_read_s32(dn, "valid-freq-delta-pct", &tsd.valid_freq_delta_pct);
	ret |= of_property_read_s32(dn, "min-sensitivity", &tsd.min_sensitivity);
	ret |= of_property_read_s32(dn, "cpu-bottleneck-thr", &tsd.cpu_bottleneck_thr);
	ret |= of_property_read_s32(dn, "gpu-bottleneck-thr", &tsd.gpu_bottleneck_thr);
	ret |= of_property_read_s32(dn, "mif-bottleneck-thr", &tsd.mif_bottleneck_thr);
	ret |= of_property_read_s32(dn, "frame-src", &tsd.frame_src);
	ret |= of_property_read_s32(dn, "max-fps", &tsd.max_fps);
	ret |= of_property_read_s32(dn, "dt-ctrl-en", &tsd.dt_ctrl_en);
	ret |= of_property_read_s32(dn, "dt-over-thr", &tsd.dt_over_thr);
	ret |= of_property_read_s32(dn, "dt-under-thr", &tsd.dt_under_thr);
	ret |= of_property_read_s32(dn, "dt-up-step", &tsd.dt_up_step);
	ret |= of_property_read_s32(dn, "dt-down-step", &tsd.dt_down_step);
	ret |= of_property_read_s32(dn, "dpat-upper-thr", &tsd.dpat_upper_thr);
	ret |= of_property_read_s32(dn, "dpat-lower-thr", &tsd.dpat_lower_thr);
	ret |= of_property_read_s32(dn, "dpat-lower-cnt-thr", &tsd.dpat_lower_cnt_thr);
	ret |= of_property_read_s32(dn, "dpat-up-step", &tsd.dpat_up_step);
	ret |= of_property_read_s32(dn, "dpat-down-step", &tsd.dpat_down_step);
	ret |= of_property_read_s32(dn, "inc-perf-temp-thr", &tsd.inc_perf_temp_thr);
	ret |= of_property_read_s32(dn, "inc-perf-power-thr", &tsd.inc_perf_power_thr);
	ret |= of_property_read_s32(dn, "inc-perf-thr", &tsd.inc_perf_thr);
	ret |= of_property_read_s32(dn, "dec-perf-thr", &tsd.dec_perf_thr);
	ret |= of_property_read_s32(dn, "fragutil-thr", &profiler.fragutil_upper_thr);
	profiler.fragutil_lower_thr = profiler.fragutil_upper_thr * FRAGUTIL_THR_MARGIN / 100;
	profiler.fragutil_lower_thr = max(0, profiler.fragutil_lower_thr);
	ret |= of_property_read_s32(dn, "gpu-freq-thr", &profiler.gpu_freq_thr);
	ret |= of_property_read_s32(dn, "heavy-gpu-ms-thr", &profiler.heavy_gpu_ms_thr);
	ret |= of_property_read_s32(dn, "hp-minlock-fps-delta-pct-thr", &tsd.hp_minlock_fps_delta_pct_thr);
	ret |= of_property_read_s32(dn, "hp-minlock-power-upper-thr", &tsd.hp_minlock_power_upper_thr);
	ret |= of_property_read_s32(dn, "hp-minlock-power-lower-thr", &tsd.hp_minlock_power_lower_thr);
	ret |= of_property_read_s32(dn, "dyn-mo-control", &tsd.dyn_mo_control);
	profiler.bts_idx = bts_get_scenindex("g3d_heavy");
	profiler.len_mo_id = of_property_count_u32_elems(dn, "mo-id");
	if (profiler.len_mo_id > 0) {
		profiler.mo_id = kzalloc(sizeof(int) * profiler.len_mo_id, GFP_KERNEL);
		if (!profiler.mo_id)
			return ret;

		ret |= of_property_read_u32_array(dn, "mo-id", profiler.mo_id, profiler.len_mo_id);
	}

	for (i = 0; i < 6; i++) {
		snprintf(name, sizeof(name), "runtime-thr-%d", i);
		ret |= of_property_read_s32(dn, name, &tsd.runtime_thr[i]);
	}

	ret |= of_property_read_u32_array(dn, "pid-cpu", tsd.pid_cpu, 3);
	ret |= of_property_read_u32_array(dn, "pid-gpu", tsd.pid_gpu, 3);
	ret |= of_property_read_u32_array(dn, "pid-util", tsd.pid_util, 3);

	return ret;
}

static s32 exynos_profiler_suspend(struct device *dev)
{
	if (profiler.running) {
		profiler.game_mode = false;
		profiler.heavy_gpu_mode = false;
		wakeup_polling_task();
	}

	return 0;
}

static s32 exynos_profiler_resume(struct device *dev)
{
	return 0;
}

static s32 exynos_profiler_probe(struct platform_device *pdev)
{
	s32 ret;

	profiler.disable = false;
	profiler.running = false;

	profiler.dn = pdev->dev.of_node;
	if (!profiler.dn) {
		pr_err("profiler: Failed to get device tree\n");
		return -EINVAL;
	}

	ret = parse_profiler_dt(profiler.dn);
	if (ret) {
		pr_err("profiler: Failed to parse device tree\n");
		return ret;
	}

	profiler.kobj = &pdev->dev.kobj;
	ret += sysfs_create_group(profiler.kobj, &profiler_attr_group);
	ret += sysfs_create_group(profiler.kobj, &control_attr_group);
	if (ret) {
		pr_err("profiler: Failed to init sysfs\n");
		return ret;
	}

	INIT_WORK(&profiler.fps_work, profiler_fps_change_work);
	INIT_WORK(&profiler.fragutil_work, profiler_fragutil_change_work);
	ret = exynos_profiler_register_utilization_notifier(&profiler_fragutil_change_notifier);
	if (ret) {
		pr_err("profiler: Failed to register fragutil change notifier\n");
		return ret;
	}

	profiler.running_event = PROFILE_INVALID;
	init_waitqueue_head(&profiler.wq);
	mutex_init(&profiler_dsd_lock);
	exynos_profiler_register_misc_device();

	emstune_register_notifier(&profiler_mode_update_notifier);

	pr_info("profiler: complete to probe profiler\n");

	return ret;
}

static const struct of_device_id exynos_profiler_match[] = {
	{ .compatible	= "samsung,exynos-profiler", },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_profiler_match);

static const struct dev_pm_ops exynos_profiler_pm_ops = {
	.suspend	= exynos_profiler_suspend,
	.resume		= exynos_profiler_resume,
};

static struct platform_driver exynos_profiler_driver = {
	.probe		= exynos_profiler_probe,
	.driver	= {
		.name	= "exynos-profiler",
		.owner	= THIS_MODULE,
		.pm	= &exynos_profiler_pm_ops,
		.of_match_table = exynos_profiler_match,
	},
};

static s32 exynos_profiler_init(void)
{
	return platform_driver_register(&exynos_profiler_driver);
}
late_initcall(exynos_profiler_init);

MODULE_DESCRIPTION("Exynos Main Profiler v4");
MODULE_LICENSE("GPL");
