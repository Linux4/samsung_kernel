#include <v1/exynos-main-profiler.h>

#define PROFILER_LLC_GFX_BIT 1
#define PROFILER_LLC_GPU_BIT 2
#define PROFILER_LLC_HEAVY_BIT 4

/* 9925 ~ 9945 has 512KB * 16 ways, total 8MB */
#define MAX_LLC_WAY 16

/******************************************************************************/
/*                               Global Variables                             */
/******************************************************************************/
static struct profiler_fops_priv profiler_private_data[NUM_OF_PROFILER_SVC];

/******************************************************************************/
/*                               Func decleration                             */
/******************************************************************************/
static void wakeup_gfx(int enable);
static void wakeup_job_submit(int enable);

static void profiler_start_profile(void);
static void profiler_update_profile(void);
static void profiler_finish_profile(void);

static void control_llc(int value);
/******************************************************************************/
/*                               Helper functions                             */
/******************************************************************************/
static inline int is_cpu_enabled(void)
{
	int is_cpu_domain_enabled = 0;
	int idx;

	for (idx = 0; idx < NUM_OF_CPU_DOMAIN; idx++)
		is_cpu_domain_enabled |= profiler.domains[idx].enabled;

	return is_cpu_domain_enabled;
}

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
/*                               Profiler Workqueue                           */
/******************************************************************************/
#define PROFILER_INTERVAL_MS 100
#define PROFILER_STAY_TIMES (1 * 1000 / PROFILER_INTERVAL_MS)
static void profiler_sched(void)
{
	int ret = atomic_xchg(&profiler.profiler_running, 1);

	profiler.profiler_stay_counter = PROFILER_STAY_TIMES;
	if (ret == 0)
		schedule_work_on(0, &profiler.profiler_wq);
}

static void profiler_sleep(void)
{
	if (profiler.profiler_stay_counter > 0)
		profiler.profiler_stay_counter = 0;
}

static int profiler_check_job_submit(void)
{
	int ret = (profiler.profiler_state & PROFILER_STATE_JOB_SUBMIT)? 1:0;
	int mainbw = psd.stats1_sum + psd.stats2_sum;
	int ratio = (mainbw > 0)? (psd.stats2_sum *100 /mainbw) : 0;

	if (ret) {
		if (mainbw < profiler.perf_mainbw_thr || ratio < profiler.perf_gpubw_dn_ratio)
			ret = 0;
	} else {
		if (mainbw > profiler.perf_mainbw_thr && ratio > profiler.perf_gpubw_up_ratio)
			ret = 1;
	}

	return ret;
}

static void profiler_work(struct work_struct *work)
{
	profiler_start_profile();
	msleep_interruptible(PROFILER_INTERVAL_MS);

	while (profiler.profiler_stay_counter > 0) {
		ktime_t ts = ktime_get();
		int remained_ms = PROFILER_INTERVAL_MS - (int)(ktime_to_ms(ts) % PROFILER_INTERVAL_MS);

		profiler.profiler_stay_counter--;
		profiler_update_profile();

		if (profiler.profiler_stay_counter > 0 && profiler_check_job_submit()) {
			if (!(profiler.profiler_state & PROFILER_STATE_JOB_SUBMIT))
				wakeup_job_submit(1);
		} else if (profiler.profiler_state & PROFILER_STATE_JOB_SUBMIT)
			wakeup_job_submit(0);

		if (remained_ms < (PROFILER_INTERVAL_MS / 10))
			remained_ms += PROFILER_INTERVAL_MS;
		msleep_interruptible(remained_ms);
	}

	profiler_finish_profile();
	atomic_set(&profiler.profiler_running, 0);
}

/******************************************************************************/
/*                                      fops                                  */
/******************************************************************************/
static ssize_t profiler_fops_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct profiler_fops_priv *priv = (struct profiler_fops_priv *)filp->private_data;
	int id = 0;

	if (!access_ok(buf, sizeof(int)))
		return -EFAULT;

	if (copy_from_user(&id, buf, sizeof(int))) {
		pr_info("PROFILER : fops write doesn't work!");
		return -EINVAL;
	}

	if (id >= SVC_RESERVED) {
		pr_info("%s: invalid id(%d)", __func__, id);
		return -EINVAL;
	}

	priv = &profiler_private_data[id];

	priv->id = id;
	priv->pid = (int)(current->pid);
	priv->poll_mask = 0;

	filp->private_data = priv;

	return 0;
}

static ssize_t profiler_fops_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct profiler_fops_priv *priv = (struct profiler_fops_priv *)filp->private_data;

	if (!priv || priv->id >= SVC_RESERVED)
		return -EINVAL;

	if (!access_ok(buf, sizeof(s32)))
		return -EFAULT;

	if (copy_to_user(buf, &profiler.profiler_state, sizeof(s32)))
		pr_info("PROFILER : Reading doesn't work!");

	return count;
}

static __poll_t profiler_fops_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct profiler_fops_priv *priv = (struct profiler_fops_priv *)filp->private_data;
	__poll_t mask = 0;

	if (!priv || priv->id >= SVC_RESERVED)
		return -EINVAL;

	if (priv == NULL) {
		return 0;
	}

	poll_wait(filp, &profiler.wq, wait);

	if (priv->poll_mask == 0) {
		mask = EPOLLIN | EPOLLRDNORM;
		priv->poll_mask = mask;
	}

	return mask;
}

static long profiler_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct profiler_fops_priv *priv = (struct profiler_fops_priv *)filp->private_data;

	if (!priv || priv->id >= SVC_RESERVED)
		return -EINVAL;

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

static int profiler_fops_open(struct inode *node, struct file *filp)
{
	struct profiler_fops_priv *priv;

	priv = &profiler_private_data[SVC_RESERVED];

	priv->id = SVC_RESERVED;
	priv->pid = -1;
	priv->poll_mask = 0;

	filp->private_data = priv;

	return 0;
}

static int profiler_fops_release(struct inode *node, struct file *filp)
{
	struct profiler_fops_priv *priv = (struct profiler_fops_priv *)filp->private_data;

	if (!priv || priv->id >= SVC_RESERVED)
		return -EINVAL;

	if (priv->id == SVC_GFX) {
		if (psd.enable_gfx && (profiler.profiler_state & PROFILER_STATE_GFX)) {
			pr_info("PROFILER : release service id (%d)", priv->id);
			psd.enable_gfx = 0;
			profiler.profiler_state ^= PROFILER_STATE_GFX;
			exynos_profiler_set_profiler_governor(0);
		}
	}
#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GFLOW)
	else if (priv->id == SVC_JOB_SUBMIT) {
		if (psd.enable_gfx && (profiler.profiler_state & PROFILER_STATE_JOB_SUBMIT)) {
			pr_info("PROFILER : release service id (%d)", priv->id);
			profiler.profiler_state ^= PROFILER_STATE_JOB_SUBMIT;
		}
	}
#endif

	priv->id = SVC_RESERVED;
	priv->pid = -1;
	priv->poll_mask = 0;

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
	if (id < NUM_OF_CPU_DOMAIN) {
		struct private_data_cpu *private = dom->private;

		private->fn->cpu_active_pct(id, psd.cpu_active_pct[id]);
	} else if (id == PROFILER_GPU) {
		struct private_data_gpu *private = dom->private;

		private->fn->get_timeinfo(&psd.rtimes[0]);
		private->fn->get_pidinfo(&psd.pid_list[0], &psd.rendercore_list[0]);
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

		if (id < NUM_OF_CPU_DOMAIN) {
			struct private_data_cpu *private = dom->private;

			tsd.first_cpu[id] = private->pm_qos_cpu;
			tsd.num_of_cpu[id] = private->num_of_cpu;
			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;
		} else if (id == PROFILER_GPU) {
			struct private_data_gpu *private = dom->private;

			tsd.gpu_hw_status = private->fn->get_gpu_hw_status();
			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;
		} else if (id == PROFILER_MIF) {
			struct private_data_mif *private = dom->private;

			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;
		}
	}
}

/******************************************************************************/
/*                              Profile functions                             */
/******************************************************************************/
static void profiler_enable_profiler_govs(int enable)
{
	if (enable) {
		/* Enable governors */
		if (profiler.dm_dynamic == 1) {
			profiler.dm_dynamic = 0;
			exynos_dm_dynamic_disable(profiler.dm_dynamic);
		}
		if (profiler.gpu_profiler_gov == 0) {
			profiler.gpu_profiler_gov = 1;
			exynos_profiler_set_profiler_governor(profiler.gpu_profiler_gov);
		}
	} else {
		/* Disable governors */
		if (profiler.dm_dynamic == 0 && psd.enable_gfx && (profiler.profiler_state & PROFILER_STATE_GFX)) {
			profiler.dm_dynamic = 1;
			exynos_dm_dynamic_disable(profiler.dm_dynamic);
		}
		if (profiler.gpu_profiler_gov == 1) {
			profiler.gpu_profiler_gov = 0;
			exynos_profiler_set_profiler_governor(profiler.gpu_profiler_gov);
		}
	}
}

static void profiler_update_profile(void)
{
	struct domain_data *dom;
	ktime_t time = 0;
	s32 id;

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

static void profiler_start_profile(void)
{
	struct domain_data *dom;
	ktime_t time = 0;
	s32 id;

	profiler.start_time = ktime_get();
	if (profiler.get_frame_cnt)
		profiler.get_frame_cnt(&profiler.start_frame_cnt, &time);
	if (profiler.get_vsync_cnt)
		profiler.get_vsync_cnt(&profiler.start_frame_vsync_cnt, &time);

	for_each_mdomain(dom, id) {
		int i;

		dom->fn->update_mode(id, 1);
		profiler_get_profile_data(dom);

		if (id < NUM_OF_CPU_DOMAIN) {
			struct private_data_cpu *private = dom->private;

			for (i = 0; i < EPOLL_MAX_PID; i++)
				private->local_min_freq[i] = 0;
		} else if (id == PROFILER_MIF) {
			struct private_data_mif *private = dom->private;

			for (i = 0; i < EPOLL_MAX_PID; i++)
				private->local_min_freq[i] = 0;
		} else if (id == PROFILER_GPU) {
			struct private_data_gpu *private = dom->private;

			for (i = 0; i < EPOLL_MAX_PID; i++)
				private->local_min_freq[i] = 0;
		}
	}

	profiler_set_tunable_data();
}

static void profiler_finish_profile(void)
{
	struct domain_data *dom;
	s32 id;

	for_each_mdomain(dom, id) {
		dom->fn->update_mode(id, 0);

		if (id < NUM_OF_CPU_DOMAIN) {
			struct private_data_cpu *private = dom->private;

			if (freq_qos_request_active(&private->pm_qos_min_req))
				freq_qos_update_request(&private->pm_qos_min_req,
						PM_QOS_DEFAULT_VALUE);
		} else if (id == PROFILER_MIF) {
			struct private_data_mif *private = dom->private;

			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req,
						EXYNOS_PM_QOS_DEFAULT_VALUE);
		} else {
			struct private_data_gpu *private = dom->private;
			/* gpu use negative value for unlock */
			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req,
						EXYNOS_PM_QOS_DEFAULT_VALUE);
		}
	}

	if (profiler.bts_added) {
		bts_del_scenario(profiler.bts_idx);
		profiler.bts_added = 0;
		psd.gpu_mo = 0;
	}

	/* forcely disable profiler govs */
	profiler_enable_profiler_govs(0);

	if (profiler.disable_llc_way) {
		exynos_profiler_set_disable_llc_way(0);
		profiler.disable_llc_way = 0;
	}

	if (profiler.llc_config)
		control_llc(0);
}

static void wakeup_gfx(int enable)
{
	int flag = 0;

	if (enable && !(profiler.profiler_state & PROFILER_STATE_GFX)) {
		profiler.profiler_state |= PROFILER_STATE_GFX;
		if (!profiler.disable_gfx)
			psd.enable_gfx = 1;
		flag = 1;
	} else if (!enable && (profiler.profiler_state & PROFILER_STATE_GFX)) {
		profiler.profiler_state ^= PROFILER_STATE_GFX;
		psd.enable_gfx = 0;
		flag = 1;
	}

	if (flag) {
		profiler_private_data[0].poll_mask = 0;
		wake_up_interruptible(&profiler.wq);
	}
}

static void wakeup_job_submit(int enable)
{
#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GFLOW)
	int flag = 0;

	if (enable && !(profiler.profiler_state & PROFILER_STATE_JOB_SUBMIT)) {
		profiler.profiler_state |= PROFILER_STATE_JOB_SUBMIT;
		flag = 1;
	} else if (!enable && (profiler.profiler_state & PROFILER_STATE_JOB_SUBMIT)) {
		profiler.profiler_state ^= PROFILER_STATE_JOB_SUBMIT;
		flag = 1;
	}

	if (flag) {
		profiler_private_data[1].poll_mask = 0;
		wake_up_interruptible(&profiler.wq);
	}
#endif
}

/******************************************************************************/
/*                               SYSFS functions                              */
/******************************************************************************/
/* only for fps profile without profiler running and profile only mode */
static ssize_t running_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = (profiler.profiler_state & PROFILER_STATE_GFX)? 1:0;
	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}
static ssize_t running_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (!profiler.profiler_gov) {
		u32 v;

		if (sscanf(buf, "%u", &v) != 1)
			return -EINVAL;

		profiler.forced_running = v;
		wakeup_gfx(v);
	}
	return count;
}
static DEVICE_ATTR_RW(running);

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

static ssize_t monitor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", psd.monitor);
}
static ssize_t monitor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 v;

	if (sscanf(buf, "%u", &v) != 1)
		return -EINVAL;

	psd.monitor = v;
	tsd.monitor = v;

	return count;
}
static DEVICE_ATTR_RW(monitor);

static ssize_t disable_gfx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", profiler.disable_gfx);
}
static ssize_t disable_gfx_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 v;

	if (sscanf(buf, "%u", &v) != 1)
		return -EINVAL;

	profiler.disable_gfx = v;

	return count;
}
static DEVICE_ATTR_RW(disable_gfx);

static ssize_t disable_job_submit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", psd.disable_job_submit);
}
static ssize_t disable_job_submit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 v;

	if (sscanf(buf, "%u", &v) != 1)
		return -EINVAL;

	psd.disable_job_submit = v;

	return count;
}
static DEVICE_ATTR_RW(disable_job_submit);

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

static ssize_t disable_dyn_mo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", psd.disable_dyn_mo);
}
static ssize_t disable_dyn_mo_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 v;

	if (sscanf(buf, "%u", &v) != 1)
		return -EINVAL;

	psd.disable_dyn_mo = v;

	return count;
}
static DEVICE_ATTR_RW(disable_dyn_mo);

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
TUNABLE_ATTR_RW(window_period);
TUNABLE_ATTR_RW(window_number);
TUNABLE_ATTR_RW(dt_up_step);
TUNABLE_ATTR_RW(dt_down_step);

#define PER_CPU_TUNABLE_ATTR_RW(_name)						\
static ssize_t _name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)			\
{										\
	struct domain_data *dom;						\
	s32 id, ret = 0;							\
										\
	for_each_mdomain(dom, id) {						\
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%d]%s: %d\n",	\
				id, profiler.domain_name[id], tsd._name[id]);		\
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

static struct attribute *profiler_attrs[] = {
	&dev_attr_running.attr,
	&dev_attr_disable_gfx.attr,
	&dev_attr_disable_job_submit.attr,
	&dev_attr_fps_profile.attr,
	&dev_attr_version.attr,
	&dev_attr_monitor.attr,
	&dev_attr_window_period.attr,
	&dev_attr_window_number.attr,
	&dev_attr_dt_up_step.attr,
	&dev_attr_dt_down_step.attr,
	&dev_attr_disable_dyn_mo.attr,
	NULL,
};
static struct attribute_group profiler_attr_group = {
	.name = "profiler",
	.attrs = profiler_attrs,
};

#define RATIO_UNIT	1000
static ssize_t set_margin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static void control_min_qos(int daemonid, int domid, int value)
{
	struct domain_data *dom = &profiler.domains[domid];
	int i;
	int freq = 0;

	if (domid < NUM_OF_CPU_DOMAIN) {
		struct private_data_cpu *private = dom->private;

		private->local_min_freq[daemonid] = value;

		for (i = 0; i < EPOLL_MAX_PID; i++)
			if (freq < private->local_min_freq[i])
				freq = private->local_min_freq[i];

		if (freq_qos_request_active(&private->pm_qos_min_req))
			freq_qos_update_request(&private->pm_qos_min_req, freq);
	} else if (domid == PROFILER_GPU) {
		struct private_data_gpu *private = dom->private;

		private->local_min_freq[daemonid] = value;

		for (i = 0; i < EPOLL_MAX_PID; i++)
			if (freq < private->local_min_freq[i])
				freq = private->local_min_freq[i];

		if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
			exynos_pm_qos_update_request(&private->pm_qos_min_req, freq);
	} else if (domid == PROFILER_MIF) {
		struct private_data_mif *private = dom->private;

		private->local_min_freq[daemonid] = value;

		for (i = 0; i < EPOLL_MAX_PID; i++)
			if (freq < private->local_min_freq[i])
				freq = private->local_min_freq[i];

		if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
			exynos_pm_qos_update_request(&private->pm_qos_min_req, freq);
	}
}

static void control_margin(int id, int value)
{
	struct domain_data *dom = &profiler.domains[id];

	/* CPU uses pelt margin via ems service */
	if (id < NUM_OF_CPU_DOMAIN) {
		if (is_enabled(PROFILER_GPU)) {
			struct domain_data *gpudom = &profiler.domains[PROFILER_GPU];

			gpudom->fn->set_margin(id, value);
		}
		return;
	}

	if (likely(dom->fn->set_margin))
		dom->fn->set_margin(id, value);
}
static void control_mo(int id, int value)
{
	int idx;

	if (!profiler.bts_idx || !profiler.len_mo_id)
		return;

	if (value > 0) {
		if (!profiler.bts_added) {
			bts_add_scenario(profiler.bts_idx);
			profiler.bts_added = 1;
		}

		for (idx = 0; idx < profiler.len_mo_id; idx++)
			bts_change_mo(profiler.bts_idx, profiler.mo_id[idx], value, value);
	} else if (profiler.bts_added) {
		bts_del_scenario(profiler.bts_idx);
		profiler.bts_added = 0;
	}

	psd.gpu_mo = value;
	return;
}

static void control_llc(int value)
{
	int prev_gpu_llc = 0;
	int next_gpu_llc = 0;

	/* GFX LLC */
	if ((value & PROFILER_LLC_GFX_BIT) && !(profiler.llc_config & PROFILER_LLC_GFX_BIT)) {
		if (llc_region_alloc(LLC_REGION_PROFILER, 1, profiler.perf_max_llc_way) == 0)
			profiler.llc_config |= PROFILER_LLC_GFX_BIT;
	} else if (!(value & PROFILER_LLC_GFX_BIT) && (profiler.llc_config & PROFILER_LLC_GFX_BIT)) {
		if (llc_region_alloc(LLC_REGION_PROFILER, 0, 0) == 0)
			profiler.llc_config &= PROFILER_LLC_GPU_BIT;
	}

	/* Job-submit LLC */
	if (profiler.llc_config & PROFILER_LLC_GPU_BIT)
		prev_gpu_llc = (profiler.llc_config & PROFILER_LLC_HEAVY_BIT)? profiler.perf_max_llc_way : profiler.perf_light_llc_way;

	if (value & PROFILER_LLC_GPU_BIT)
		next_gpu_llc = (value & PROFILER_LLC_HEAVY_BIT)? profiler.perf_max_llc_way : profiler.perf_light_llc_way;

	if (prev_gpu_llc != next_gpu_llc) {
		if (prev_gpu_llc > 0) {
			if (llc_region_alloc(LLC_REGION_GPU, 0, 0) == 0)
				profiler.llc_config &= PROFILER_LLC_GFX_BIT;
		} else {
			if (llc_region_alloc(LLC_REGION_GPU, 1, next_gpu_llc) == 0)
				profiler.llc_config |= PROFILER_LLC_GPU_BIT | (value & PROFILER_LLC_HEAVY_BIT);
		}
	}

	psd.llc_config = profiler.llc_config;
}

static void control_targetframetime(s32 value)
{
	if (is_enabled(PROFILER_GPU))
	{
		struct domain_data *dom = &profiler.domains[PROFILER_GPU];
		struct private_data_gpu *private = dom->private;

		private->fn->set_targetframetime(value);
	}
}

/* kernel only to decode */
#define get_op_code(x) (((x) >> OP_CODE_SHIFT) & OP_CODE_MASK)
#define get_daemon_id(x) (((x) >> DAEMON_ID_SHIFT) & DAEMON_ID_MASK)
#define get_ip_id(x) ((x) & IP_ID_MASK)
static ssize_t set_margin_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 id, did, op, ctrl, value;

	if (sscanf(buf, "%d %d", &ctrl, &value) != 2)
		return -EINVAL;

	id = get_ip_id(ctrl);
	did = get_daemon_id(ctrl);
	op = get_op_code(ctrl);

	if (!is_enabled(id) || op == OP_INVALID)
		return -EINVAL;

	switch (op) {
	case OP_PM_QOS_MIN:
		if (did < 0 || did >= EPOLL_MAX_PID) {
			return -EINVAL;
		}
		control_min_qos(did, id, value);
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
	case OP_TARGETFRAMETIME:
		control_targetframetime(value);
		break;
	default:
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(set_margin);

static ssize_t control_profile_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s32 ret = 0;
	int mainbw = psd.stats1_sum + psd.stats2_sum;
	int ratio = (mainbw > 0)? (psd.stats2_sum *100 /mainbw) : 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_state=%d\n", profiler.profiler_state);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_time_ms=%lld\n", psd.profile_time_ms);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_frame_cnt=%llu\n", psd.profile_frame_cnt);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "user_target_fps=%d\n", psd.user_target_fps);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "user_target_fps=%d\n", psd.user_target_fps);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "max_freq=");
	for (int id = 0 ; id < NUM_OF_DOMAIN ; id++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d ", psd.max_freq[id]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "min_freq=");
	for (int id = 0 ; id < NUM_OF_DOMAIN ; id++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d ", psd.min_freq[id]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "freq=");
	for (int id = 0 ; id < NUM_OF_DOMAIN ; id++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d ", psd.freq[id]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "dyn_power=");
	for (int id = 0 ; id < NUM_OF_DOMAIN ; id++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%lld ", psd.dyn_power[id]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "st_power=");
	for (int id = 0 ; id < NUM_OF_DOMAIN ; id++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%lld ", psd.st_power[id]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "temp=");
	for (int id = 0 ; id < NUM_OF_DOMAIN ; id++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d ", psd.temp[id]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "active_pct=");
	for (int id = 0 ; id < NUM_OF_DOMAIN ; id++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d ", psd.active_pct[id]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "times=%llu %llu %llu %llu %llu\n",
			psd.rtimes[PREAVG], psd.rtimes[CPUAVG], psd.rtimes[V2SAVG],
			psd.rtimes[GPUAVG], psd.rtimes[V2FAVG]);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats0_sum=%llu\n", psd.stats0_sum);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats0_avg=%llu\n", psd.stats0_avg);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats_ratio=%llu\n", psd.stats0_ratio);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "mif_pm_qos_cur_freq=%d\n", psd.mif_pm_qos_cur_freq);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "perf_mainbw_thr=%d\n", profiler.perf_mainbw_thr);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "perf_gpubw_up_ratio=%d\n", profiler.perf_gpubw_up_ratio);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "perf_gpubw_dn_ratio=%d\n", profiler.perf_gpubw_dn_ratio);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "perf_max_llc_way=%d\n", profiler.perf_max_llc_way);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "perf_light_llc_way=%d\n", profiler.perf_light_llc_way);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "llc_config=%d\n", profiler.llc_config);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "main_bw=%d, gpubw%%=%d\n", mainbw, ratio);

	return ret;
}
static ssize_t control_profile_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 mode;

	if (sscanf(buf, "%u", &mode) != 1)
		return -EINVAL;

	if (mode == 1)
		profiler_sched();
	else {
		u32 cmd = mode >> 16;
		u32 value = mode & ((1 << 16) - 1);

		switch (cmd) {
			case CONTROL_CMD_PERF_MAINBW_THR:
				profiler.perf_mainbw_thr = value;
				break;
			case CONTROL_CMD_PERF_UP_RATIO:
				profiler.perf_gpubw_up_ratio = value;
				break;
			case CONTROL_CMD_PERF_DN_RATIO:
				profiler.perf_gpubw_dn_ratio = value;
				break;
			case CONTROL_CMD_DISABLE_LLC_WAY:
				exynos_profiler_set_disable_llc_way(value);
				profiler.disable_llc_way = (bool)value;
				break;
			case CONTROL_CMD_ENABLE_PROFILER_GOVERNOR:
				profiler_enable_profiler_govs(value);
				break;
			case CONTROL_CMD_MAX_LLC_WAY:
				profiler.perf_max_llc_way = value;
				break;
			case CONTROL_CMD_LIGHT_LLC_WAY:
				profiler.perf_light_llc_way = value;
				break;
			default:
				break;
		}
	}
	return count;
}
static DEVICE_ATTR_RW(control_profile);

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
	&dev_attr_user_target_fps.attr,
	NULL,
};
static struct attribute_group control_attr_group = {
	.name = "control",
	.attrs = control_attrs,
};

/*********************************************************************/
/*                  PROFILER mode change notifier                    */
/*********************************************************************/
static int profiler_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *set = (struct emstune_set *)v;

	if (profiler.forced_running)
		return NOTIFY_OK;

	if (!profiler.disable_gfx && (set->mode == EMSTUNE_GAME_MODE)) {
		profiler.profiler_gov = 1;
		wakeup_gfx(1);
	} else {
		profiler.profiler_gov = 0;
		wakeup_gfx(0);
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

	dn = of_get_child_by_name(root, profiler.domain_name[id]);
	if (!dn) {
		pr_err("profiler: Failed to get node of domain(id=%d)\n", id);
		return -ENODEV;
	}

	tsd.freq_table_cnt[id] = dom->fn->get_table_cnt(id);
	cnt = dom->fn->get_freq_table(id, tsd.freq_table[id]);
	if (tsd.freq_table_cnt[id] != cnt) {
		pr_err("profiler: table cnt(%u) is un-synced with freq table cnt(%u)(id=%d)\n",
					id, tsd.freq_table_cnt[id], cnt);
		return -EINVAL;
	}

	if (id < NUM_OF_CPU_DOMAIN) {
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
		private->pm_qos_min_freq = PM_QOS_DEFAULT_VALUE;

		if (of_property_read_s32(dn, "pid-util-max", &private->pid_util_max))
			private->pid_util_max = 800;
		if (of_property_read_s32(dn, "pid-util-min", &private->pid_util_min))
			private->pid_util_min = 600;

		freq_qos_tracer_add_request(&policy->constraints,
				&private->pm_qos_min_req, FREQ_QOS_MIN,
				FREQ_QOS_MIN_DEFAULT_VALUE);

		if (!ret) {
			dom->private = private;
		} else {
			kfree(private);
		}
	} else if (id == PROFILER_GPU) {
		struct private_data_gpu *private;

		private = kzalloc(sizeof(struct private_data_gpu), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "pm-qos-min-class",
				&private->pm_qos_min_class);

		private->pm_qos_min_freq = PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE;

		if (!ret) {
			/* gpu use negative value for unlock */
			exynos_pm_qos_add_request(&private->pm_qos_min_req,
					private->pm_qos_min_class, EXYNOS_PM_QOS_DEFAULT_VALUE);

			dom->private = private;
		} else {
			kfree(private);
		}
	} else if (id == PROFILER_MIF) {
		struct private_data_mif *private;

		private = kzalloc(sizeof(struct private_data_mif), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "pm-qos-min-class", &private->pm_qos_min_class);

		private->pm_qos_min_freq = PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE;

		if (of_property_read_s32(dn, "pid-util-max", &private->pid_util_max))
			private->pid_util_max = 980;
		if (of_property_read_s32(dn, "pid-util-min", &private->pid_util_min))
			private->pid_util_min = 920;

		if (!ret) {
			exynos_pm_qos_add_request(&private->pm_qos_min_req,
					private->pm_qos_min_class, EXYNOS_PM_QOS_DEFAULT_VALUE);

			dom->private = private;
		} else {
			kfree(private);
		}
	}

	return ret;
}

s32 exynos_profiler_register_misc_device(void)
{
	s32 ret;

	profiler.gov_fops.fops.owner		= THIS_MODULE;
	profiler.gov_fops.fops.llseek		= no_llseek;
	profiler.gov_fops.fops.write	= profiler_fops_write;
	profiler.gov_fops.fops.read		= profiler_fops_read;
	profiler.gov_fops.fops.poll		= profiler_fops_poll;
	profiler.gov_fops.fops.unlocked_ioctl	= profiler_fops_ioctl;
	profiler.gov_fops.fops.compat_ioctl	= profiler_fops_ioctl;
	profiler.gov_fops.fops.open			= profiler_fops_open;
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
	int num_of_domains = 0;
	s32 ret = 0;

	ret |= of_property_read_s32(dn, "version", &tsd.version);
	ret |= of_property_read_s32(dn, "window-period", &tsd.window_period);
	ret |= of_property_read_s32(dn, "window-number", &tsd.window_number);
	ret |= of_property_read_s32(dn, "frame-src", &tsd.frame_src);
	ret |= of_property_read_s32(dn, "max-fps", &tsd.max_fps);
	ret |= of_property_read_s32(dn, "dt-up-step", &tsd.dt_up_step);
	ret |= of_property_read_s32(dn, "dt-down-step", &tsd.dt_down_step);
	profiler.bts_idx = bts_get_scenindex("g3d_heavy");
	profiler.len_mo_id = of_property_count_u32_elems(dn, "mo-id");
	if (profiler.len_mo_id > 0) {
		profiler.mo_id = kzalloc(sizeof(int) * profiler.len_mo_id, GFP_KERNEL);
		if (!profiler.mo_id)
			return ret;

		ret |= of_property_read_u32_array(dn, "mo-id", profiler.mo_id, profiler.len_mo_id);
	}

	num_of_domains = of_property_count_strings(dn, "domain-names");
	if (num_of_domains == NUM_OF_DOMAIN) {
		int rc = 0;
		rc = of_property_read_string_array(dn, "domain-names", profiler.domain_name, NUM_OF_DOMAIN);
		ret |= (rc - NUM_OF_DOMAIN);
	}

	return ret;
}

static s32 exynos_profiler_suspend(struct device *dev)
{
	profiler_sleep();

	return 0;
}

static s32 exynos_profiler_resume(struct device *dev)
{
	profiler_sched();
	return 0;
}

static s32 exynos_profiler_probe(struct platform_device *pdev)
{
	s32 ret;

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

	profiler.forced_running = 0;
	profiler.disable_gfx = 0;
	profiler.profiler_gov = 0;
	profiler.gpu_profiler_gov = 0;
	profiler.disable_llc_way = 0;
	profiler.bts_added = 0;
	psd.enable_gfx = 0;
	psd.disable_job_submit = 0;
	psd.disable_dyn_mo = 0;
	init_waitqueue_head(&profiler.wq);
	mutex_init(&profiler_dsd_lock);
	exynos_profiler_register_misc_device();

	emstune_register_notifier(&profiler_mode_update_notifier);

	profiler.perf_mainbw_thr = DEF_PERF_MAINBW_THR;
	profiler.perf_gpubw_up_ratio = DEF_PERF_GPUBW_UP_RATIO;
	profiler.perf_gpubw_dn_ratio = DEF_PERF_GPUBW_DN_RATIO;
	profiler.perf_max_llc_way = MAX_LLC_WAY;
	profiler.perf_light_llc_way = MAX_LLC_WAY >> 1;
	profiler.profiler_state = PROFILER_STATE_SLEEP;
	exynos_profiler_register_wakeup_func(profiler_sched);
	atomic_set(&profiler.profiler_running, 0);
	profiler.profiler_stay_counter = 0;
	INIT_WORK(&profiler.profiler_wq, profiler_work);

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

MODULE_DESCRIPTION("Exynos Main Profiler v1");
MODULE_SOFTDEP("pre: exynos_thermal_v2 exynos-cpufreq sgpu exynos_devfreq");
MODULE_LICENSE("GPL");
