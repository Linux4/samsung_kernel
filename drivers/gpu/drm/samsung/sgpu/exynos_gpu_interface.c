#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include "sgpu_governor.h"
#include "sgpu_user_interface.h"
#include "sgpu_utilization.h"
#include "amdgpu.h"

#ifdef CONFIG_DRM_SGPU_EXYNOS

#include <linux/notifier.h>
#include <soc/samsung/bts.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/tmu.h>
#include <soc/samsung/exynos-sci.h>
#include "exynos_gpu_interface.h"

#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>

#if IS_ENABLED(CONFIG_GPU_THERMAL)
#include "exynos_tmu.h"
#endif

#include <linux/ems.h>

extern struct amdgpu_device *p_adev;
static struct blocking_notifier_head utilization_notifier_list;

static struct notifier_block freq_trans_notifier;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static struct dev_pm_qos_request	exynos_pm_qos_min;
static struct dev_pm_qos_request	exynos_pm_qos_max;

static struct exynos_pm_qos_request exynos_g3d_mif_min_qos;
static struct exynos_pm_qos_request exynos_mm_gpu_min_qos;
static struct exynos_pm_qos_request exynos_tmu_gpu_max_qos;
static struct exynos_pm_qos_request exynos_ski_gpu_min_qos;
static struct exynos_pm_qos_request exynos_ski_gpu_max_qos;
static struct exynos_pm_qos_request exynos_gpu_siop_max_qos;
static struct exynos_pm_qos_request exynos_umd_gpu_min_qos;
static struct exynos_pm_qos_request exynos_afm_gpu_max_qos;
#endif /*CONFIG_EXYNOS_PM_QOS*/

static unsigned int *mif_freq;
static unsigned int *mif_cl_boost_freq;
static unsigned int *mo_scen;
static unsigned int *llc_ways;
static unsigned int disable_llc_way;
static unsigned long ski_gpu_min_clock;
static unsigned long ski_gpu_max_clock = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;
static unsigned long gpu_siop_max_clock = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;
static unsigned long gpu_afm_max_clock = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;

static struct delayed_work work_mm_min_clock;

static struct delayed_work work_umd_min_clock;


static int prev_scen, prev_ways;
static int exynos_freq_trans_notifier(struct notifier_block *nb,
				       unsigned long event, void *ptr)
{
	struct devfreq *df = p_adev->devfreq;
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)ptr;
	struct sgpu_governor_data *data = df->data;
	int current_scen, current_ways, new_level, i, ret = 0;
	unsigned long mif_minfreq;

	switch(event) {
	case DEVFREQ_POSTCHANGE:
		if (!p_adev->in_suspend)
			gpu_dvfs_update_time_in_state(freqs->old);
		break;
	default:
		break;
	}

	/* in suspend or power_off*/
	if (atomic_read(&df->suspend_count) > 0)
		return NOTIFY_DONE;

	if (freqs->new == freqs->old)
		return NOTIFY_DONE;
	switch (event) {
	case DEVFREQ_PRECHANGE:
		if (freqs->new < freqs->old)
			return NOTIFY_DONE;
		break;
	case DEVFREQ_POSTCHANGE:
		if (freqs->new > freqs->old)
			return NOTIFY_DONE;
		break;
	default:
		break;
	}

	new_level = df->profile->max_state - 1;
	for (i = 0; i < df->profile->max_state; i++) {
		if (df->profile->freq_table[i] == freqs->new)
			new_level = i;
	}
	current_scen = mo_scen[new_level];

	/* set other device freq and mo lock */
	if (current_scen > prev_scen)
		bts_add_scenario(current_scen);
	else if (current_scen < prev_scen)
		bts_del_scenario(prev_scen);
	prev_scen = current_scen;

	if (!data->cl_boost_disable && data->cl_boost_status)
		mif_minfreq = mif_cl_boost_freq[new_level];
	else
		mif_minfreq = mif_freq[new_level];

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(&exynos_g3d_mif_min_qos,
				     mif_minfreq);
#endif /*CONFIG_EXYNOS_PM_QOS*/

	/* Dealloc GPU LLC if not needed anymore
	 * Also it need to be deallocated before allocating different amount
	 * e.g: 0 -> 10 -> 16 -> 0  BAD!!
	 *      0 -> 10 -> 0  -> 16 GOOD!!
	 */
	current_ways = disable_llc_way?0:llc_ways[new_level];
	if (prev_ways != current_ways) {
		if (current_ways == 0 || prev_ways > 0)
			ret = llc_region_alloc(LLC_REGION_GPU, 0, 0);

		if (current_ways > 0 && ret == 0)
			ret = llc_region_alloc(LLC_REGION_GPU, 1, current_ways);
	}
	if (ret)
		DRM_ERROR("%s : failed to allocate llc", __func__);
	else
		prev_ways = current_ways;

	return NOTIFY_DONE;
}

int exynos_dvfs_preset(struct devfreq *df)
{
	int resume_level, resume_scen, resume_ways;
	int i, ret = 0;

	resume_level = df->profile->max_state - 1;
	for (i = 0; i < df->profile->max_state; i++) {
		if (df->profile->freq_table[i] == df->resume_freq)
			resume_level = i;
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(&exynos_g3d_mif_min_qos,
				     mif_freq[resume_level]);
#endif /*CONFIG_EXYNOS_PM_QOS*/
	resume_scen = mo_scen[resume_level];

	if (resume_scen > prev_scen)
		bts_add_scenario(resume_scen);
	else if (resume_scen < prev_scen)
		bts_del_scenario(prev_scen);
	prev_scen = resume_scen;

	resume_ways = disable_llc_way ? 0 : llc_ways[resume_level];
	if (prev_ways != resume_ways) {
		if (resume_ways == 0 || prev_ways > 0)
			ret = llc_region_alloc(LLC_REGION_GPU, 0, 0);

		if (resume_ways > 0 && ret == 0)
			ret = llc_region_alloc(LLC_REGION_GPU, 1, resume_ways);
	}
	if (ret)
		DRM_ERROR("%s : failed to allocate llc", __func__);
	else
		prev_ways = resume_ways;

	return ret;
}

int exynos_dvfs_postclear(struct devfreq *df)
{
	int suspend_scen = bts_get_scenindex("default");
	int ret = 0;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(&exynos_g3d_mif_min_qos, 0);
#endif /*CONFIG_EXYNOS_PM_QOS*/
	if (suspend_scen < prev_scen)
		bts_del_scenario(prev_scen);
	prev_scen = suspend_scen;

	if (prev_ways != 0)
		ret = llc_region_alloc(LLC_REGION_GPU, 0, 0);

	if (ret)
		DRM_ERROR("%s : failed to allocate llc", __func__);
	else
		prev_ways = 0;

	return ret;
}


#define MAX_STRING 20
static unsigned int *sgpu_get_array_mo(struct devfreq_dev_profile *dp,
				       const char *buf)
{
	const char *cp;
	int i, j;
	int ntokens = 1;
	unsigned int *tokenized_data, *array_data;
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
		if ((i & 0x1) == 0) {
			char string[MAX_STRING];

			if (sscanf(cp, "%s", &string) != 1)
				goto err_kfree;
			tokenized_data[i++] = bts_get_scenindex(string);
		} else {
			if (sscanf(cp, "%u", &tokenized_data[i++]) != 1)
				goto err_kfree;

		}

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	array_data = kmalloc(dp->max_state * sizeof(unsigned int), GFP_KERNEL);
	if (!array_data) {
		err = -ENOMEM;
		goto err_kfree;
	}

	for (i = dp->max_state - 1, j = 0; i >= 0; i--) {
		while (j < ntokens - 1 &&
		       dp->freq_table[i] >= tokenized_data[j + 1])
			j += 2;
		array_data[i] = tokenized_data[j];
	}
	kfree(tokenized_data);

	return array_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static int gpu_pm_qos_min_notifier(struct notifier_block *nb,
			       unsigned long val, void *v)
{
	if (!dev_pm_qos_request_active(&exynos_pm_qos_min))
		return NOTIFY_BAD;

	dev_pm_qos_update_request(&exynos_pm_qos_min,
				  val / HZ_PER_KHZ);
	return NOTIFY_OK;
}

static int gpu_pm_qos_max_notifier(struct notifier_block *nb,
			       unsigned long val, void *v)
{
	if (!dev_pm_qos_request_active(&exynos_pm_qos_max))
		return NOTIFY_BAD;
	dev_pm_qos_update_request(&exynos_pm_qos_max,
				  val / HZ_PER_KHZ);
	return NOTIFY_OK;
}
#endif /*CONFIG_EXYNOS_PM_QOS*/

static int exynos_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	struct devfreq *df = p_adev->devfreq;
	uint64_t utilization;
	unsigned long cur_freq;

	/* work only SYSBUSY_CHECK_BOOST mode */
	if (val != SYSBUSY_CHECK_BOOST)
		return NOTIFY_OK;

	if (!df || !df->profile)
		return NOTIFY_OK;

	if (df->profile->get_cur_freq)
		df->profile->get_cur_freq(df->dev.parent, &cur_freq);
	else
		cur_freq = df->previous_freq;

	utilization = div64_u64(df->last_status.busy_time * 100,
				df->last_status.total_time);

	DRM_DEBUG("%s: freq=%lu, util=%llu", __func__, cur_freq, utilization);
	/* if we have proper GPU workload when this is called, return BAD */
	if (cur_freq > SYSBUSY_FREQ_THRESHOLD)
		return NOTIFY_BAD;
	else if (cur_freq == SYSBUSY_FREQ_THRESHOLD &&
		 utilization >= SYSBUSY_UTIL_THRESHOLD)
		return NOTIFY_BAD;
	else
		return NOTIFY_OK;
}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static struct notifier_block gpu_min_qos_notifier = {
	.notifier_call = gpu_pm_qos_min_notifier,
	.priority = INT_MAX,
};

static struct notifier_block gpu_max_qos_notifier = {
	.notifier_call = gpu_pm_qos_max_notifier,
	.priority = INT_MAX,
};
#endif /*CONFIG_EXYNOS_PM_QOS*/

static struct notifier_block exynos_sysbusy_notifier = {
	.notifier_call = exynos_sysbusy_notifier_call,
};

static void gpu_mm_min_reset(struct work_struct *work)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MIN_REQUEST gpu_mm_min=0 (timer reset)");
	DRM_INFO("[sgpu] MIN_REQUEST gpu_mm_min=0 (timer reset)");
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(&exynos_mm_gpu_min_qos, 0);
#endif /*CONFIG_EXYNOS_PM_QOS*/
	mutex_lock(&data->lock);
	data->mm_min_clock = 0;
	mutex_unlock(&data->lock);
}

static ssize_t gpu_mm_min_clock_show(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	return scnprintf(buf, PAGE_SIZE, "%lu\n", data->mm_min_clock);
}

static ssize_t gpu_mm_min_clock_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;
	unsigned long value, delay;
	int ret;
	bool work_ret;

	ret = sscanf(buf, "%lu", &value);
	if (ret != 1)
		return -EINVAL;

	buf = strpbrk(buf, " ");
	if (buf == NULL)
		return -EINVAL;

	buf++;
	ret = sscanf(buf, "%lu", &delay);
	if (ret != 1)
		return -EINVAL;

	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MIN_REQUEST gpu_mm_min=%lu", value);
	DRM_INFO("[sgpu] MIN_REQUEST gpu_mm_min=%lu", value);

	if (sgpu_dvfs_governor_major_level_check(p_adev->devfreq, value)) {
		if (data->minlock_limit && value >= data->minlock_limit_freq)
			return -EINVAL;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		exynos_pm_qos_update_request(&exynos_mm_gpu_min_qos, value);
#endif /*CONFIG_EXYNOS_PM_QOS*/
		if (delay) {
			/* Return: %false if @dwork was idle and queued,
			 * %true if @dwork was pending and its timer was modified.
			 */
			work_ret = mod_delayed_work(system_wq, &work_mm_min_clock,
						    msecs_to_jiffies(delay));

		}
		mutex_lock(&data->lock);
		data->mm_min_clock = value;
		mutex_unlock(&data->lock);
	} else
		return -EINVAL;

	return count;
}
static struct kobj_attribute attr_gpu_mm_min_clock = __ATTR_RW(gpu_mm_min_clock);

/* disable llc ways */
static ssize_t gpu_disable_llc_way_show(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", disable_llc_way);
}

static ssize_t gpu_disable_llc_way_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		return -EFAULT;

	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "LLC_UPDATE gpu_disable_llc_way=%lu", value);
	DRM_INFO("[sgpu] LLC_UPDATE gpu_disable_llc_way=%lu", value);

	disable_llc_way = value;


	return count;
}
static struct kobj_attribute attr_gpu_disable_llc_way = __ATTR_RW(gpu_disable_llc_way);

/* gpu min freq interface through chunk from UMD */
static void gpu_umd_min_reset(struct work_struct *work)
{
	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MIN_REQUEST gpu_umdmin=0 (timer reset)");
	DRM_INFO("[sgpu] MIN_REQUEST gpu_umd_min=0 (timer reset)");

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(&exynos_umd_gpu_min_qos, 0);
#endif /*CONFIG_EXYNOS_PM_QOS*/
}

void gpu_umd_min_clock_set(unsigned int value, unsigned int delay)
{
	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MIN_REQUEST gpu_umd_min=%lu", value);
	DRM_INFO("[sgpu] MIN_REQUEST gpu_umd_min=%lu", value);

	if (sgpu_dvfs_governor_major_level_check(p_adev->devfreq, value)) {
		struct sgpu_governor_data *data = p_adev->devfreq->data;
		if (data->minlock_limit && value >= data->minlock_limit_freq)
			return;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		exynos_pm_qos_update_request(&exynos_umd_gpu_min_qos, value);
#endif /*CONFIG_EXYNOS_PM_QOS*/
		mod_delayed_work(system_wq, &work_umd_min_clock,
				 msecs_to_jiffies(delay));
	}
}

static ssize_t gpu_min_clock_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", ski_gpu_min_clock);
}

static ssize_t gpu_min_clock_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned long freq;
	int ret = 0;

	ret = sscanf(buf, "%lu", &freq);
	if (ret != 1)
		return -EINVAL;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (!exynos_pm_qos_request_active(&exynos_ski_gpu_min_qos))
		return -EINVAL;
#endif /*CONFIG_EXYNOS_PM_QOS*/

	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MIN_REQUEST ski_gpu_min=%lu", freq);
	DRM_INFO("[sgpu] MIN_REQUEST ski_gpu_min=%lu", freq);
	if (sgpu_dvfs_governor_major_level_check(p_adev->devfreq, freq)) {
		struct sgpu_governor_data *data = p_adev->devfreq->data;
		if (data->minlock_limit && freq >= data->minlock_limit_freq)
			return -EINVAL;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		exynos_pm_qos_update_request(&exynos_ski_gpu_min_qos, freq);
#endif /*CONFIG_EXYNOS_PM_QOS*/
		ski_gpu_min_clock = freq;
	} else
		return -EINVAL;

	return count;
}
static struct kobj_attribute attr_gpu_min_clock = __ATTR_RW(gpu_min_clock);

static ssize_t gpu_max_clock_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", ski_gpu_max_clock);
}

static ssize_t gpu_max_clock_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned long freq;
	int ret = 0;

	ret = sscanf(buf, "%lu", &freq);
	if (ret != 1)
		return -EINVAL;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (!exynos_pm_qos_request_active(&exynos_ski_gpu_max_qos))
		return -EINVAL;
#endif /*CONFIG_EXYNOS_PM_QOS*/
	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MAX_REQUEST ski_gpu_max=%lu", freq);
	DRM_INFO("[sgpu] MAX_REQUEST ski_gpu_max=%lu", freq);
	if (sgpu_dvfs_governor_major_level_check(p_adev->devfreq, freq)) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		exynos_pm_qos_update_request(&exynos_ski_gpu_max_qos, freq);
#endif /*CONFIG_EXYNOS_PM_QOS*/
		ski_gpu_max_clock = freq;
	} else
		return -EINVAL;

	return count;
}
static struct kobj_attribute attr_gpu_max_clock = __ATTR_RW(gpu_max_clock);

static ssize_t gpu_siop_max_clock_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", gpu_siop_max_clock);
}

static ssize_t gpu_siop_max_clock_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned int freq;
	int ret = 0;

	ret = kstrtou32(buf, 0, &freq);
	if (ret)
		return -EFAULT;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (!exynos_pm_qos_request_active(&exynos_gpu_siop_max_qos))
		return -EINVAL;
#endif /*CONFIG_EXYNOS_PM_QOS*/
	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MAX_REQUEST gpu_siop_max=%lu", freq);
	DRM_INFO("[sgpu] MAX_REQUEST gpu_siop_max=%lu", freq);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(&exynos_gpu_siop_max_qos, freq);
#endif /*CONFIG_EXYNOS_PM_QOS*/
	gpu_siop_max_clock = freq;

	return count;
}
static struct kobj_attribute attr_gpu_siop_max_clock = __ATTR_RW(gpu_siop_max_clock);

static ssize_t gpu_busy_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	uint64_t utilization;
	struct devfreq *df = p_adev->devfreq;

	if(!df)
	{
		DRM_INFO("ski interface: cannot find devfreq\n");
		return -EINVAL;
	}

	utilization = div64_u64(df->last_status.busy_time * 100,
				df->last_status.total_time);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", utilization);

}
static struct kobj_attribute attr_gpu_busy = __ATTR_RO(gpu_busy);

static ssize_t gpu_clock_show(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	struct devfreq *df = p_adev->devfreq;
	unsigned long freq, cur_clock = 0;
	ssize_t count;

	if (!df || !df->profile)
		return -EINVAL;

	if (df->profile->get_cur_freq &&
		!df->profile->get_cur_freq(df->dev.parent, &freq))
		cur_clock = freq;
	else
		cur_clock = df->previous_freq;

	count = scnprintf(buf, PAGE_SIZE, "%lu\n", cur_clock);

	return count;
}
static struct kobj_attribute attr_gpu_clock = __ATTR_RO(gpu_clock);


static ssize_t gpu_freq_table_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	struct devfreq *df = p_adev->devfreq;
	ssize_t count = 0;
	int level;

	if (!df || !df->profile || !df->profile->freq_table)
		return -EINVAL;

	level = df->profile->max_state - 1;
	sgpu_dvfs_governor_major_current(df, &level);

	while (level >= 0) {
		count += scnprintf(&buf[count], PAGE_SIZE - count,
				   "%lu ", df->profile->freq_table[level]);
		if (level != 0)
			sgpu_dvfs_governor_major_level_down(df, &level);
		else
			break;
	}
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}
static struct kobj_attribute attr_gpu_freq_table = __ATTR_RO(gpu_freq_table);


static ssize_t gpu_governor_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	struct devfreq *df = p_adev->devfreq;
	ssize_t count;

	if (!df || !df->governor || !df->data)
		return -EINVAL;

	count = sgpu_governor_current_info_show(df, buf);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t gpu_governor_store(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	struct devfreq *df = p_adev->devfreq;
	int ret;
	char str_governor[DEVFREQ_NAME_LEN + 1];

	if (!df || !df->governor || !df->data)
		return -EINVAL;

	ret = sscanf(buf, "%" __stringify(DEVFREQ_NAME_LEN) "s", str_governor);
	if (ret != 1)
		return -EINVAL;

	ret = sgpu_governor_change(df, str_governor);
	if (ret) {
		DRM_WARN("%s: Governor %s not started(%d)\n",
			 __func__, df->governor->name, ret);
		return ret;
	}

	if (!ret) {
		sgpu_governor_current_info_show(df, str_governor);
		DRM_INFO("governor : %s\n", str_governor);
		ret = count;
	}
	return ret;
}
static struct kobj_attribute attr_gpu_governor = __ATTR_RW(gpu_governor);

static ssize_t gpu_available_governor_show(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   char *buf)
{
	struct devfreq *df = p_adev->devfreq;

	if (!df || !df->governor || !df->data)
		return -EINVAL;

	return sgpu_governor_all_info_show(df, buf);
}
static struct kobj_attribute attr_gpu_available_governor =
					__ATTR_RO(gpu_available_governor);

#if IS_ENABLED(CONFIG_GPU_THERMAL)
static ssize_t gpu_tmu_show(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	ssize_t count;
	int gpu_temp = 0, gpu_temp_int = 0, gpu_temp_point = 0, ret;
	static struct thermal_zone_device *tz;

	if(!tz)
		tz = thermal_zone_get_zone_by_name("G3D");

	ret = thermal_zone_get_temp(tz, &gpu_temp);
	if (ret) {
		DRM_WARN("Error reading temp of gpu thermal zone: %d\n", ret);
		return -EINVAL;
	}

#if !IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
	gpu_temp *= MCELSIUS;
#endif

	gpu_temp_int = gpu_temp / 1000;
	gpu_temp_point = gpu_temp % 1000; /* % gpu_temp_int */
	count = scnprintf(buf, PAGE_SIZE, "%d.%-3d\n",
			  gpu_temp_int, gpu_temp_point);

	return count;
}
static struct kobj_attribute attr_gpu_tmu = __ATTR_RO(gpu_tmu);
#endif

static ssize_t gpu_model_show(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	const char *product = NULL;
	ssize_t count = 0;

	if (AMDGPU_IS_MGFX0(p_adev->grbm_chip_rev))
		product = "920";
	else if (AMDGPU_IS_MGFX1(p_adev->grbm_chip_rev))
		product = "930";

	count = scnprintf(buf, PAGE_SIZE, "Samsung Xclipse %s\n", product);

	return count;
}
static struct kobj_attribute attr_gpu_model = __ATTR_RO(gpu_model);

static ssize_t gpu_cl_boost_disable_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	return scnprintf(buf, PAGE_SIZE, "%lu\n", data->cl_boost_disable);
}

static ssize_t gpu_cl_boost_disable_store(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  const char *buf, size_t count)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;
	int ret, mode;

	ret = kstrtos32(buf, 0, &mode);

	if (ret != 0)
		return -EINVAL;
	if (mode != 0 && mode != 1)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->cl_boost_disable = mode ? true : false;
	mutex_unlock(&data->lock);

	return count;
}
static struct kobj_attribute attr_gpu_cl_boost_disable = __ATTR_RW(gpu_cl_boost_disable);

static ssize_t total_kernel_pages_show(struct kobject *kobj,
				       struct kobj_attribute *attr, char *buf)
{
	struct drm_device *ddev;
	struct amdgpu_device *adev;

	ddev = adev_to_drm(p_adev);
	if (!ddev)
		return 0;

	adev = ddev->dev_private;
	if (!adev)
		return 0;

	return scnprintf(buf, PAGE_SIZE, "total_kernel_pages = %zd\n", adev->num_kernel_pages);
}
static struct kobj_attribute attr_total_kernel_pages = __ATTR_RO(total_kernel_pages);

static ssize_t mem_info_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	struct drm_device *ddev = adev_to_drm(p_adev);
	struct drm_file *file = NULL;
	struct amdgpu_fpriv *afpriv = NULL;
	ssize_t count = 0;
	size_t total_num_pages = 0, num_pages;
	int r = 0;

	if (!ddev)
		return r;

	r = mutex_lock_interruptible(&ddev->filelist_mutex);

	if (r)
		return r;

	list_for_each_entry(file, &ddev->filelist, lhead) {
		afpriv = file->driver_priv;
		if (!afpriv)
			continue;

		mutex_lock(&afpriv->memory_lock);
		num_pages = afpriv->total_pages;
		mutex_unlock(&afpriv->memory_lock);
		if (!num_pages)
			continue;

		count += scnprintf(&buf[count], PAGE_SIZE - count,
				   "pid: %6d %15zu\n", afpriv->tgid,
				   num_pages * PAGE_SIZE);
		total_num_pages += num_pages;
	}

	mutex_unlock(&ddev->filelist_mutex);

	count += scnprintf(&buf[count], PAGE_SIZE - count, "pid: %6d %15zu\n",
			   0, p_adev->num_kernel_pages * PAGE_SIZE);
	total_num_pages += p_adev->num_kernel_pages;
	count += scnprintf(&buf[count], PAGE_SIZE - count, "total:      %15zu\n",
			   total_num_pages * PAGE_SIZE);
	return count;
}
static struct kobj_attribute attr_mem_info = __ATTR_RO(mem_info);

static ssize_t ctx_mem_info_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	struct drm_device *ddev = adev_to_drm(p_adev);
	struct drm_file *file = NULL;
	struct amdgpu_fpriv *afpriv = NULL;
	struct amdgpu_ctx *ctx = NULL;
	uint32_t id = 0;
	ssize_t count = 0;
	int r = 0;
	u64 total_ctx_mem = 0;

	if (!ddev)
		return r;

	r = mutex_lock_interruptible(&ddev->filelist_mutex);
	if (r)
		return r;

	count += scnprintf(&buf[count], PAGE_SIZE - count,
			   "PID    SIZE\n");

	list_for_each_entry(file, &ddev->filelist, lhead) {
		afpriv = file->driver_priv;
		if (!afpriv)
			continue;

		mutex_lock(&afpriv->memory_lock);
		if (afpriv->total_pages == 0) {
			mutex_unlock(&afpriv->memory_lock);
			continue;
		}

		idr_for_each_entry(&afpriv->ctx_mgr.ctx_handles, ctx, id) {
			if (ctx->mem_size == 0)
				continue;
			total_ctx_mem += ctx->mem_size;
		}
		if (total_ctx_mem > 0) {
			count += scnprintf(&buf[count], PAGE_SIZE - count,
					  "%6d %12llu\n",
					  afpriv->tgid, total_ctx_mem);
		}
		total_ctx_mem = 0;

		mutex_unlock(&afpriv->memory_lock);
	}

	mutex_unlock(&ddev->filelist_mutex);

	return count;
}
static struct kobj_attribute attr_ctx_mem_info = __ATTR_RO(ctx_mem_info);

static ssize_t gpu_ifpo_control_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "current ifpo status : %d\n", p_adev->ifpo);
}

static ssize_t gpu_ifpo_control_store(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_v;
	int ret = 0;

	ret = sscanf(buf, "%lu", &set_v);
	switch (set_v) {
		case 0:
			p_adev->ifpo = 0;
			break;
		case 1:
			p_adev->ifpo = 1;
			break;
		default:
			DRM_INFO("%s: wrong value %lu \n",__func__, &set_v);
			break;
	}

	return count;
}

static struct kobj_attribute attr_gpu_ifpo_control = __ATTR_RW(gpu_ifpo_control);

static ssize_t gpu_ifpo_runtime_control_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "current ifpo_runtime_control status : %d\n",
			 p_adev->ifpo_runtime_control);
}

static ssize_t gpu_ifpo_runtime_control_store(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_v;
	int ret = 0;

	ret = sscanf(buf, "%lu", &set_v);
	switch (set_v) {
	case 0:
		p_adev->ifpo_runtime_control = 0;
		break;
	case 1:
		p_adev->ifpo_runtime_control = 1;
		break;
	default:
		DRM_INFO("%s: wrong value %lu\n", __func__, &set_v);
		break;
	}

	return count;
}

static struct kobj_attribute attr_gpu_ifpo_runtime_control = __ATTR_RW(gpu_ifpo_runtime_control);


/* SKI (Samsung Kernel Interface) */
static struct kobject *ski_kobj;

static struct attribute *ski_gpu_sysfs_entries[] = {
	&attr_gpu_min_clock.attr,
	&attr_gpu_max_clock.attr,
	&attr_gpu_siop_max_clock.attr,
	&attr_gpu_busy.attr,
	&attr_gpu_clock.attr,
	&attr_gpu_freq_table.attr,
	&attr_gpu_governor.attr,
	&attr_gpu_available_governor.attr,
#if IS_ENABLED(CONFIG_GPU_THERMAL)
	&attr_gpu_tmu.attr,
#endif
	&attr_gpu_model.attr,
	&attr_gpu_cl_boost_disable.attr,
	&attr_gpu_mm_min_clock.attr,
	&attr_gpu_disable_llc_way.attr,
	&attr_total_kernel_pages.attr,
	&attr_mem_info.attr,
	&attr_ctx_mem_info.attr,
	&attr_gpu_ifpo_control.attr,
	&attr_gpu_ifpo_runtime_control.attr,
	NULL,
};

static struct attribute_group ski_gpu_sysfs_attr_group = {
	.attrs = ski_gpu_sysfs_entries,
};

int exynos_interface_init(struct devfreq *df)
{
	const char *tmp_str;
	int ret = 0;

	if (of_property_read_string(p_adev->pldev->dev.of_node,
				    "mif_min_lock", &tmp_str)) {
		tmp_str = "0";
	}
	mif_freq = sgpu_get_array_data(df->profile, tmp_str);
	if (IS_ERR(mif_freq)) {
		ret = PTR_ERR(mif_freq);
		DRM_ERROR("fail mif freq tokenized %d\n", ret);
		return ret;
	}
	if (of_property_read_string(p_adev->pldev->dev.of_node,
				    "mif_cl_boost_min_lock", &tmp_str)) {
		tmp_str = "0";
	}
	mif_cl_boost_freq = sgpu_get_array_data(df->profile, tmp_str);
	if (IS_ERR(mif_cl_boost_freq)) {
		ret = PTR_ERR(mif_cl_boost_freq);
		DRM_ERROR("fail mif cl_boost freq tokenized %d\n", ret);
		return ret;
	}
	if (of_property_read_string(p_adev->pldev->dev.of_node,
				    "mo_scenario", &tmp_str)) {
		tmp_str = "0";
	}
	mo_scen = sgpu_get_array_mo(df->profile, tmp_str);
	if (IS_ERR(mo_scen)) {
		ret = PTR_ERR(mo_scen);
		DRM_ERROR("fail mo scenario tokenized %d\n", ret);
		return ret;
	}

	if (of_property_read_string(p_adev->pldev->dev.of_node,
				    "llc_ways", &tmp_str)) {
		tmp_str = "0";
	}
	llc_ways = sgpu_get_array_data(df->profile, tmp_str);

	freq_trans_notifier.notifier_call = exynos_freq_trans_notifier;
	devm_devfreq_register_notifier(df->dev.parent, df,
				       &freq_trans_notifier,
				       DEVFREQ_TRANSITION_NOTIFIER);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_add_request(&exynos_g3d_mif_min_qos, PM_QOS_BUS_THROUGHPUT, 0);
	dev_pm_qos_add_request(df->dev.parent, &exynos_pm_qos_min,
			       DEV_PM_QOS_MIN_FREQUENCY,
			       df->scaling_min_freq / HZ_PER_KHZ);
	dev_pm_qos_add_request(df->dev.parent, &exynos_pm_qos_max,
			       DEV_PM_QOS_MAX_FREQUENCY,
			       df->scaling_max_freq / HZ_PER_KHZ);
	exynos_pm_qos_add_notifier(PM_QOS_GPU_THROUGHPUT_MAX,
				   &gpu_max_qos_notifier);
	exynos_pm_qos_add_notifier(PM_QOS_GPU_THROUGHPUT_MIN,
				   &gpu_min_qos_notifier);
	exynos_pm_qos_add_request(&exynos_mm_gpu_min_qos,
				  PM_QOS_GPU_THROUGHPUT_MIN, 0);
	exynos_pm_qos_add_request(&exynos_tmu_gpu_max_qos,
				  PM_QOS_GPU_THROUGHPUT_MAX,
				  PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	exynos_pm_qos_add_request(&exynos_ski_gpu_min_qos,
				  PM_QOS_GPU_THROUGHPUT_MIN, 0);
	exynos_pm_qos_add_request(&exynos_ski_gpu_max_qos,
				  PM_QOS_GPU_THROUGHPUT_MAX,
				  PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	exynos_pm_qos_add_request(&exynos_gpu_siop_max_qos,
				  PM_QOS_GPU_THROUGHPUT_MAX,
				  PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	exynos_pm_qos_add_request(&exynos_umd_gpu_min_qos,
				  PM_QOS_GPU_THROUGHPUT_MIN, 0);
	exynos_pm_qos_add_request(&exynos_afm_gpu_max_qos,
				  PM_QOS_GPU_THROUGHPUT_MAX,
				  PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
#endif /*CONFIG_EXYNOS_PM_QOS*/
	sysbusy_register_notifier(&exynos_sysbusy_notifier);
	gpu_dvfs_update_time_in_state(0);

	INIT_DELAYED_WORK(&work_mm_min_clock, gpu_mm_min_reset);
	INIT_DELAYED_WORK(&work_umd_min_clock, gpu_umd_min_reset);

	ski_kobj = kobject_create_and_add("gpu", kernel_kobj);
	if(!ski_kobj)
		DRM_ERROR("failed to create kobj for ski");

	ret = sysfs_create_group(ski_kobj, &ski_gpu_sysfs_attr_group);
	if (ret) {
		kobject_put(ski_kobj);
		DRM_ERROR("failed to create sysfs for ski_gpu_sysfs");
	}
	return ret;
}

int exynos_interface_deinit(struct devfreq *df)
{
	devm_devfreq_unregister_notifier(df->dev.parent, df,
					 &freq_trans_notifier,
					 DEVFREQ_TRANSITION_NOTIFIER);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_remove_request(&exynos_mm_gpu_min_qos);
	exynos_pm_qos_remove_request(&exynos_tmu_gpu_max_qos);
	exynos_pm_qos_remove_request(&exynos_ski_gpu_min_qos);
	exynos_pm_qos_remove_request(&exynos_ski_gpu_max_qos);
	exynos_pm_qos_remove_request(&exynos_gpu_siop_max_qos);
	exynos_pm_qos_remove_request(&exynos_afm_gpu_max_qos);
	exynos_pm_qos_remove_notifier(PM_QOS_GPU_THROUGHPUT_MAX,
				      &gpu_max_qos_notifier);
	exynos_pm_qos_remove_notifier(PM_QOS_GPU_THROUGHPUT_MIN,
				      &gpu_min_qos_notifier);
	dev_pm_qos_remove_request(&exynos_pm_qos_min);
	dev_pm_qos_remove_request(&exynos_pm_qos_max);
	exynos_pm_qos_remove_request(&exynos_g3d_mif_min_qos);
#endif /*CONFIG_EXYNOS_PM_QOS*/
	cancel_delayed_work_sync(&work_mm_min_clock);
	cancel_delayed_work_sync(&work_umd_min_clock);

	return 0;
}

void gpu_dvfs_init_utilization_notifier_list(void)
{
	BLOCKING_INIT_NOTIFIER_HEAD(&utilization_notifier_list);
}
EXPORT_SYMBOL(gpu_dvfs_init_utilization_notifier_list);

int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&utilization_notifier_list, nb);
}
EXPORT_SYMBOL(gpu_dvfs_register_utilization_notifier);

int gpu_dvfs_unregister_utilization_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&utilization_notifier_list, nb);
}
EXPORT_SYMBOL(gpu_dvfs_unregister_utilization_notifier);

void gpu_dvfs_notify_utilization(void)
{
	uint64_t utilization;
	struct devfreq *df = p_adev->devfreq;

	if(!df)
	{
		DRM_INFO("exynos interface: cannot find devfreq\n");
		return;
	}

	utilization = div64_u64(df->last_status.busy_time * 100,
				df->last_status.total_time);

	blocking_notifier_call_chain(&utilization_notifier_list,
				     0, (void *)&utilization);
}
EXPORT_SYMBOL(gpu_dvfs_notify_utilization);


/*init freq & voltage table*/
static int freqs[TABLE_MAX];
static int voltages[TABLE_MAX];

int gpu_dvfs_init_table(struct dvfs_rate_volt *tb, int max_state)
{
	int i;

	for (i = 0; i < max_state; i++)
	{
		freqs[i] = (int)tb[i].rate;
		voltages[i] = (int)tb[i].volt;
	}
	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_init_table);

/*frequency table size*/
int gpu_dvfs_get_step(void)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;
	int max_state;

	max_state = (int)data->major_state;
	return max_state;
}
EXPORT_SYMBOL(gpu_dvfs_get_step);

/*frequency table*/
int *gpu_dvfs_get_freq_table(void)
{
	return freqs;
}
EXPORT_SYMBOL(gpu_dvfs_get_freq_table);

/*clock(freq)*/
int gpu_dvfs_get_clock(int level)
{
	return freqs[level];
}
EXPORT_SYMBOL(gpu_dvfs_get_clock);

/*voltage*/
int gpu_dvfs_get_voltage(int clock)
{
	int i;
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	mutex_lock(&df->lock);
	for (i = 0; i < data->major_state; i++)
	{
		if(clock == freqs[i])
		{
			mutex_unlock(&df->lock);
			return voltages[i];
		}
	}
	mutex_unlock(&df->lock);

	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_get_voltage);

/*get_freq_range (for max_freq & min_freq)*/
static void gpu_dvfs_get_freq_range(struct devfreq *devfreq,
                                unsigned long *min_freq,
			        unsigned long *max_freq)
{
	unsigned long *freq_table = devfreq->profile->freq_table;
	s32 qos_min_freq, qos_max_freq;

	lockdep_assert_held(&devfreq->lock);

	/* freq_table is sorted by descending order */
	*min_freq = freq_table[devfreq->profile->max_state - 1];
	*max_freq = freq_table[0];

	qos_min_freq = dev_pm_qos_read_value(devfreq->dev.parent,
					     DEV_PM_QOS_MIN_FREQUENCY);
	qos_max_freq = dev_pm_qos_read_value(devfreq->dev.parent,
					     DEV_PM_QOS_MAX_FREQUENCY);
	*min_freq = max(*min_freq, (unsigned long)HZ_PER_KHZ * qos_min_freq);
	if (qos_max_freq != PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE)
		*max_freq = min(*max_freq,
				(unsigned long)HZ_PER_KHZ * qos_max_freq);

	*min_freq = max(*min_freq, devfreq->scaling_min_freq);
	*max_freq = min(*max_freq, devfreq->scaling_max_freq);

	if (*min_freq > *max_freq)
		*min_freq = *max_freq;
}

/*max freq*/
int gpu_dvfs_get_max_freq(void)
{
	struct devfreq *df = p_adev->devfreq;
	unsigned long ret = 0;
	unsigned long min_freq, max_freq;

	mutex_lock(&df->lock);
	gpu_dvfs_get_freq_range(df, &min_freq, &max_freq);
	ret = (int)max_freq;
	mutex_unlock(&df->lock);

	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_get_max_freq);

inline int gpu_dvfs_get_max_locked_freq(void)
{
	return gpu_dvfs_get_max_freq();
}
EXPORT_SYMBOL(gpu_dvfs_get_max_locked_freq);

int gpu_dvfs_set_max_freq(unsigned long freq)
{
	struct devfreq *df = p_adev->devfreq;
	int ret = 0;

	if (!dev_pm_qos_request_active(&df->user_max_freq_req))
		return -EAGAIN;

	if (freq == 0)
		freq = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;

	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MAX_REQUEST pm_qos=%lu", freq);
	DRM_INFO("[sgpu] MAX_REQEUST pm_qos=%lu", freq);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (sgpu_dvfs_governor_major_level_check(df, freq))
		dev_pm_qos_update_request(&exynos_pm_qos_max, freq);
	else
		return -EINVAL;
#endif /*CONFIG_EXYNOS_PM_QOS*/

	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_set_max_freq);

/*min freq*/
int gpu_dvfs_get_min_freq(void)
{
	struct devfreq *df = p_adev->devfreq;
	unsigned long ret = 0;
	unsigned long min_freq, max_freq;

	mutex_lock(&df->lock);
	gpu_dvfs_get_freq_range(df, &min_freq, &max_freq);
	ret = (int)min_freq;
	mutex_unlock(&df->lock);

	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_get_min_freq);

inline int gpu_dvfs_get_min_locked_freq(void)
{
	return gpu_dvfs_get_min_freq();
}
EXPORT_SYMBOL(gpu_dvfs_get_min_locked_freq);

int gpu_dvfs_set_min_freq(unsigned long freq)
{
	struct devfreq *df = p_adev->devfreq;
	int ret = 0;

	if (!dev_pm_qos_request_active(&df->user_min_freq_req)){
		return -EAGAIN;
	}

	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MIN_REQUEST pm_qos=%lu", freq);
	DRM_INFO("[sgpu] MIN_REQEUST pm_qos=%lu", freq);
	/* Round down to kHz for PM QoS */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (sgpu_dvfs_governor_major_level_check(df, freq)) {
		struct sgpu_governor_data *data = p_adev->devfreq->data;
		if (data->minlock_limit && freq >= data->minlock_limit_freq)
			return -EINVAL;
		dev_pm_qos_update_request(&exynos_pm_qos_min, freq);
	} else
		return -EINVAL;
#endif /*CONFIG_EXYNOS_PM_QOS*/

	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_set_min_freq);

/*current freq*/
int gpu_dvfs_get_cur_clock(void)
{
	struct devfreq *df = p_adev->devfreq;
	unsigned long freq;
	int level, ret = 0;

	if (!df->profile)
		return 0;

	if (df->profile->get_cur_freq &&
		!df->profile->get_cur_freq(df->dev.parent, &freq))
		ret = (int)freq;
	else
		ret = (int)df->previous_freq;

	if (!ret)
		return ret;

	for (level = 0; level < df->profile->max_state; level++) {
		if (ret == df->profile->freq_table[level])
			break;
	}
	sgpu_dvfs_governor_major_current(df, &level);
	ret = df->profile->freq_table[level];

	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_get_cur_clock);

/*maxlock freq*/
unsigned long gpu_dvfs_get_maxlock_freq(void)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;
	unsigned long ret = 0;

	ret = data->sys_max_freq;
	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_get_maxlock_freq);

int gpu_dvfs_set_maxlock_freq(unsigned long freq)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	if (!dev_pm_qos_request_active(&data->sys_pm_qos_max))
		return -EINVAL;

	data->sys_max_freq = freq;
	dev_pm_qos_update_request(&data->sys_pm_qos_max, freq / HZ_PER_KHZ);

	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_set_maxlock_freq);

/*minlock freq*/
unsigned long gpu_dvfs_get_minlock_freq(void)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;
	unsigned long ret = 0;

	ret = data->sys_min_freq;
	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_get_minlock_freq);

int gpu_dvfs_set_minlock_freq(unsigned long freq)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	if (!dev_pm_qos_request_active(&data->sys_pm_qos_min))
		return -EINVAL;


	if (sgpu_dvfs_governor_major_level_check(p_adev->devfreq, freq)) {
		if (data->minlock_limit && freq >= data->minlock_limit_freq)
			return -EINVAL;
		dev_pm_qos_update_request(&data->sys_pm_qos_min, freq / HZ_PER_KHZ);
		data->sys_min_freq = freq;
	} else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_set_minlock_freq);

static ktime_t time_in_state_busy[TABLE_MAX];
ktime_t prev_time;
ktime_t tis_last_update;
ktime_t gpu_dvfs_update_time_in_state(unsigned long freq)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data;
	ktime_t cur_time, state_time, busy_time, weighted_busy_time;
	int lev, prev_lev = 0, major_lev = 0;
	unsigned long major_freq;

	if (!df)
		return 0;

	data = df->data;
	if (prev_time == 0)
		prev_time = ktime_get();

	cur_time = ktime_get();

	if (freq == 0 || df->stop_polling)
		goto time_update;

	for (lev = 0; lev < df->profile->max_state; lev++) {
		if (freq == df->profile->freq_table[lev]) {
			prev_lev = lev;
			break;
		}
	}
	if (prev_lev == df->profile->max_state)
		goto time_update;

	sgpu_dvfs_governor_major_current(df, &prev_lev);
	major_freq = df->profile->freq_table[prev_lev];

	for (lev = 0; lev < data->major_state; lev++) {
		if (major_freq == freqs[lev]) {
			major_lev = lev;
			break;
		}
	}
	if (major_lev == data->major_state)
		goto time_update;

	state_time = cur_time - prev_time;
	busy_time = div64_u64(state_time * df->last_status.busy_time,
			      df->last_status.total_time);
	weighted_busy_time = div64_u64(busy_time * freq, major_freq);

	time_in_state_busy[major_lev] += weighted_busy_time;

time_update:
	prev_time = cur_time;

	return cur_time;
}

ktime_t *gpu_dvfs_get_time_in_state(void)
{
	struct devfreq *df = p_adev->devfreq;

	if (!df)
		return 0;
	tis_last_update = prev_time;

	return time_in_state_busy;

}
EXPORT_SYMBOL(gpu_dvfs_get_time_in_state);

ktime_t gpu_dvfs_get_tis_last_update(void)
{
	return tis_last_update;
}
EXPORT_SYMBOL(gpu_dvfs_get_tis_last_update);

/*polling interval*/
unsigned int gpu_dvfs_get_polling_interval(void)
{
	struct devfreq *df = p_adev->devfreq;
	int ret = 0;

	if (!df->profile)
		return 0;

	ret = df->profile->polling_ms;
	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_get_polling_interval);

int gpu_dvfs_set_polling_interval(unsigned int value)
{
	struct devfreq *df = p_adev->devfreq;

	if (!df->governor)
		return -EINVAL;

	df->governor->event_handler(df, DEVFREQ_GOV_UPDATE_INTERVAL, &value);

	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_set_polling_interval);

/*governor*/
static char governor_name[DEVFREQ_NAME_LEN];
char *gpu_dvfs_get_governor(void)
{
	struct devfreq *df = p_adev->devfreq;
	unsigned long ret = 0;

	ret = sgpu_governor_current_info_show(df, governor_name);

	if(!ret) return 0;
	else return governor_name;
}
EXPORT_SYMBOL(gpu_dvfs_get_governor);

int gpu_dvfs_set_governor(char* str_governor)
{
	struct devfreq *df = p_adev->devfreq;
	int ret;

	if (!df->governor || !df->data)
		return -EINVAL;

	ret = sgpu_governor_change(df, str_governor);

	if(ret){
		return -EINVAL;
	}

	if (!ret) {
		sgpu_governor_current_info_show(df, str_governor);
		dev_info(p_adev->dev, "governor : %s\n", str_governor);
	}

	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_set_governor);

void gpu_dvfs_set_autosuspend_delay(int delay)
{
	struct drm_device *ddev = adev_to_drm(p_adev);

	if (delay > 0)
		pm_runtime_set_autosuspend_delay(ddev->dev, delay);
}
EXPORT_SYMBOL(gpu_dvfs_set_autosuspend_delay);

int gpu_dvfs_set_disable_llc_way(int val)
{
	disable_llc_way = val;
	return disable_llc_way;
}
EXPORT_SYMBOL(gpu_dvfs_set_disable_llc_way);

int gpu_dvfs_get_utilization(void)
{
	struct devfreq *df = p_adev->devfreq;
	int utilization;

	mutex_lock(&df->lock);
	utilization = div64_u64(df->last_status.busy_time * 100,
				df->last_status.total_time);
	mutex_unlock(&df->lock);

	return utilization;
}
EXPORT_SYMBOL(gpu_dvfs_get_utilization);

int gpu_tmu_notifier(struct notifier_block *nb, unsigned long event,
		     void *v)
{
	struct devfreq *df = p_adev->devfreq;
	struct sgpu_governor_data *data;
	int frequency;

	if (df == NULL)
		return 0;

	data = df->data;

	frequency = *(int *)v;
	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MAX_REQUEST tmu_pm_qos=%d", frequency);
	DRM_INFO("[sgpu] MAX_REQUEST tmu_pm_qos=%d", frequency);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (event == GPU_NORMAL) {
		exynos_pm_qos_update_request(&exynos_tmu_gpu_max_qos,
					     PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	} else if (event == GPU_THROTTLING || event == GPU_TRIPPING) {
		if (!sgpu_dvfs_governor_major_level_check(df, frequency))
			return NOTIFY_BAD;
		exynos_pm_qos_update_request(&exynos_tmu_gpu_max_qos,
					     frequency);
	}
#endif /*CONFIG_EXYNOS_PM_QOS*/

	return NOTIFY_OK;
}
EXPORT_SYMBOL(gpu_tmu_notifier);

int gpu_afm_decrease_maxlock(unsigned int down_step)
{
	struct devfreq *df = p_adev->devfreq;
	unsigned long *freq_table = df->profile->freq_table;
	unsigned long freq = gpu_afm_max_clock;
	int i, level = 0;

	if (freq < freq_table[0]) {
		for (level = 1; level < df->profile->max_state; level++) {
			if (freq == freq_table[level])
				break;
		}
	}

	for (i = 0; i < down_step; i++)
		sgpu_dvfs_governor_major_level_up(df, &level);
	freq = freq_table[level];

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(&exynos_afm_gpu_max_qos, freq);
#endif /*CONFIG_EXYNOS_PM_QOS*/
	gpu_afm_max_clock = freq;

	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MAX_REQUEST gpu_afm_max=%lu", freq);
	DRM_INFO("[sgpu] MAX_REQUEST gpu_afm_max=%lu", freq);

	return 0;
}

int gpu_afm_release_maxlock(void)
{
	struct devfreq *df = p_adev->devfreq;
	unsigned long *freq_table = df->profile->freq_table;
	unsigned long freq = freq_table[0];

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(&exynos_afm_gpu_max_qos, freq);
#endif /*CONFIG_EXYNOS_PM_QOS*/
	gpu_afm_max_clock = freq;

	SGPU_LOG(p_adev, DMSG_INFO, DMSG_DVFS,
		 "MAX_REQUEST gpu_afm_max=%lu (release)", freq);
	DRM_INFO("[sgpu] MAX_REQUEST gpu_afm_max=%lu (release)", freq);

	return 0;
}

#endif /* CONFIG_DRM_SGPU_EXYNOS */
