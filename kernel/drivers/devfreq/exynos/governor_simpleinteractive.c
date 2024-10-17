/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/pm_opp.h>
#include "../governor.h"

#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
#include <linux/sched/clock.h>
#include <trace/events/exynos_devfreq.h>
#endif

#include <soc/samsung/exynos-devfreq.h>
#include <linux/devfreq.h>

#define LOAD_BUFFER_MAX			10
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
#define MAX_LOG_TIME 300
#endif
struct devfreq_alt_load {
	unsigned long long	delta;
	unsigned int		load;
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
	unsigned long long clock;
	unsigned int alt_freq;
#endif
};

#define ALTDVFS_MIN_SAMPLE_TIME 	15
#define ALTDVFS_HOLD_SAMPLE_TIME	100
#define ALTDVFS_TARGET_LOAD		75
#define ALTDVFS_NUM_TARGET_LOAD 	1
#define ALTDVFS_HISPEED_LOAD		99
#define ALTDVFS_HISPEED_FREQ		1000000
#define ALTDVFS_TOLERANCE		1

struct devfreq_alt_dvfs_data {
	struct devfreq_alt_load	buffer[LOAD_BUFFER_MAX];
	struct devfreq_alt_load	*front;
	struct devfreq_alt_load	*rear;

	unsigned long long	busy;
	unsigned long long	total;
	unsigned int		min_load;
	unsigned int		max_load;
	unsigned long long	max_spent;

	struct devfreq_alt_dvfs_param *alt_param;
	struct devfreq_alt_dvfs_param *alt_param_set;
	struct devfreq_alt_dvfs_param *alt_user_mode;
	int default_mode, current_mode, num_modes;
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
	bool				load_track;
	unsigned int		log_top;
	struct devfreq_alt_load *log;
#endif
};

struct devfreq_alt_dvfs_param {
	/* ALT-DVFS parameter */
	unsigned int		*target_load;
	unsigned int		num_target_load;
	unsigned int		min_sample_time;
	unsigned int		hold_sample_time;
	unsigned int		hispeed_load;
	unsigned int		hispeed_freq;
	unsigned int		tolerance;
};

#define DEFAULT_DELAY_TIME		10 /* msec */
#define DEFAULT_NDELAY_TIME		1
#define DELAY_TIME_RANGE		10
#define BOUND_CPU_NUM			0

struct devfreq_simple_interactive_data {
	int *delay_time;
	int ndelay_time;
	unsigned long prev_freq;
	u64 changed_time;
	struct timer_list freq_timer;
	struct timer_list freq_slack_timer;
	struct task_struct *change_freq_task;

	struct devfreq_alt_dvfs_data alt_data;
	unsigned int governor_freq;
};

#define NEXTBUF(x, b)	if (++(x) > &(b)[LOAD_BUFFER_MAX - 1]) (x) = (b)
#define POSTBUF(x, b)	((x) = ((--(x) < (b)) ?				\
			&(b)[LOAD_BUFFER_MAX - 1] : (x)))
static unsigned long update_load(struct devfreq_dev_status *stat,
				struct devfreq_simple_interactive_data *gov_data)
{
	struct devfreq_alt_load *ptr;
	struct devfreq_alt_dvfs_data *alt_data = &(gov_data->alt_data);
	struct devfreq_alt_dvfs_param *alt_param = alt_data->alt_param;
	unsigned int targetload;
	unsigned int freq;
	unsigned int max_freq;
	int i;
	struct exynos_devfreq_profile_data *profile_data =
		(struct exynos_devfreq_profile_data *)stat->private_data;
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
	struct devfreq_alt_load *cur_load;
	bool trace_flag = false;
#endif

	if (!profile_data->wow->total_time_aggregated)
		return stat->current_frequency;
	for (i = 0; i < alt_param->num_target_load - 1 &&
	     stat->current_frequency >= alt_param->target_load[i + 1]; i += 2);
	targetload = alt_param->target_load[i];
	max_freq = alt_param->target_load[alt_param->num_target_load - 2];

	/* if frequency is changed then reset the load */
	if (!stat->current_frequency ||
	    stat->current_frequency != gov_data->prev_freq) {
		alt_data->rear = alt_data->front;
		alt_data->front->delta = 0;
		alt_data->total = 0;
		alt_data->busy = 0;
		if (alt_data->max_load >= targetload)
			alt_data->max_load = targetload;
		else
			alt_data->max_load = 0;

		alt_data->max_spent = 0;
		alt_data->min_load = targetload;
	}
	ptr = alt_data->front;
	ptr->delta += profile_data->duration;
	alt_data->max_spent += profile_data->duration;
	alt_data->total += profile_data->wow->total_time_aggregated;
	alt_data->busy += profile_data->wow->busy_time_aggregated;

	/* if too short time, then not counting */
	if (ptr->delta > alt_param->min_sample_time * NSEC_PER_MSEC) {
		NEXTBUF(alt_data->front, alt_data->buffer);
		alt_data->front->delta = 0;

		if (alt_data->front == alt_data->rear)
			NEXTBUF(alt_data->rear, alt_data->buffer);
		ptr->load = alt_data->total ?
			alt_data->busy * 100 / alt_data->total : 0;

#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
		cur_load = ptr;
		cur_load->clock = sched_clock();
		// Save load data ptr to log buffer when logging is true
		if (alt_data->load_track) {
			alt_data->log[alt_data->log_top] = *cur_load;
		}
#endif

		alt_data->busy = 0;
		alt_data->total = 0;

		/* if ptr load is higher than pervious or too small load */
		if (alt_data->max_load <= ptr->load) {
			alt_data->min_load = ptr->load;
			alt_data->max_spent = 0;
			alt_data->max_load = ptr->load;
			goto out;
		} else if (ptr->load < alt_data->min_load) {
			alt_data->min_load = ptr->load;
			if (ptr->load < alt_param->tolerance) {
				alt_data->max_load = ptr->load;
				alt_data->max_spent = 0;
				gov_data->governor_freq = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
				if (alt_data->load_track) {
					alt_data->log[alt_data->log_top].alt_freq = gov_data->governor_freq;
					if (alt_data->log_top < (MSEC_PER_SEC / alt_param->min_sample_time * MAX_LOG_TIME))
						alt_data->log_top++;
					else
						alt_data->load_track = false;
				}
//				trace_alt_dvfs_load_tracking(cur_load->delta, cur_load->load, gov_data->governor_freq);
#endif
				return 0;
			}
		}
	}

	/* new max load */
	if (alt_data->max_spent > alt_param->hold_sample_time * NSEC_PER_MSEC) {
		unsigned long long spent = 0;
		/* if not valid data, then skip */
		if (alt_data->front == ptr) {
			spent += ptr->delta;
			POSTBUF(ptr, alt_data->buffer);
		}
		alt_data->max_load = ptr->load;
		alt_data->max_spent = spent;
		/* if there is downtrend, then reflect current load */
		if (ptr->load > alt_data->min_load + alt_param->tolerance) {
			alt_data->min_load = ptr->load;
			spent += ptr->delta;
			POSTBUF(ptr, alt_data->buffer);
			for (; spent < alt_param->hold_sample_time *
			     NSEC_PER_MSEC && ptr != alt_data->rear;
			     POSTBUF(ptr, alt_data->buffer)) {
				if (alt_data->max_load < ptr->load) {
					alt_data->max_load = ptr->load;
					alt_data->max_spent = spent;
				} else if (alt_data->min_load > ptr->load) {
					alt_data->min_load = ptr->load;
				}
				spent += ptr->delta;
			}
		} else {
			alt_data->min_load = ptr->load;
		}
	}
out:
	/* a few measurement */
	if (alt_data->max_load == targetload || alt_data->total)
		freq = gov_data->governor_freq;
	else {
		freq = alt_data->max_load * stat->current_frequency / targetload;
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
		if (alt_data->load_track) {
			alt_data->log[alt_data->log_top].alt_freq = freq;
			if (alt_data->log_top < (MSEC_PER_SEC / alt_param->min_sample_time * MAX_LOG_TIME))
				alt_data->log_top++;
			else
				alt_data->load_track = false;
		}
		trace_flag = true;
#endif
	}

	if (alt_data->max_load > alt_param->hispeed_load &&
			alt_param->hispeed_freq > freq) {
		freq = alt_param->hispeed_freq;
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
		if (alt_data->load_track) {
			alt_data->log[alt_data->log_top-1].alt_freq = freq;
		}
		trace_flag = true;
#endif
	}

	freq = (freq <= max_freq ? freq : max_freq);
	gov_data->governor_freq = freq;
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
//	if (trace_flag == true)
//			trace_alt_dvfs_load_tracking(cur_load->delta, cur_load->load, freq);
#endif
	return freq;

}

static int set_next_alt_timers(struct devfreq_simple_interactive_data *gov_data,
			u64 suspend, bool slack_on_condition)
{
	unsigned long expires = jiffies;

	if (suspend) {
		del_timer_sync(&gov_data->freq_timer);
		del_timer(&gov_data->freq_slack_timer);
		return 0;
	}

	mod_timer(&gov_data->freq_timer, expires +
		msecs_to_jiffies(gov_data->alt_data.alt_param->min_sample_time * 2));

	/* A slack timer with a nop handler
	 * intentionally wakes the CPU from idle.
	 * Thus, it prevent freq_timer from being defferred infinitely.
	 */
	if (slack_on_condition) {
		mod_timer(&gov_data->freq_slack_timer, expires +
				 msecs_to_jiffies(gov_data->alt_data.alt_param->hold_sample_time));
	} else {
		if (timer_pending(&gov_data->freq_slack_timer))
			del_timer(&gov_data->freq_slack_timer);
	}

	return 0;
}

static int devfreq_simple_interactive_func(struct devfreq *df,
					unsigned long *freq)
{
	struct exynos_devfreq_data *data = df->data;
	struct devfreq_simple_interactive_data *gov_data = df->governor_data;
	int err;

	if (!gov_data) {
		pr_err("%s: failed to find governor data\n", __func__);
		return -ENODATA;
	}

	if (data->devfreq_disabled)
		return -EAGAIN;

	/* Get governor freq */
	err = devfreq_update_stats(df);
	if (err)
		goto out;

	gov_data->governor_freq = update_load(&df->last_status, gov_data);
	gov_data->prev_freq = data->old_freq;

out:
	*freq = exynos_devfreq_policy_update(data, gov_data->governor_freq);
	set_next_alt_timers(gov_data, data->suspend_req, data->policy.governor_win);

	return 0;
}

static int devfreq_change_freq_task(void *data)
{
	struct devfreq *df = data;

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);

		schedule();

		set_current_state(TASK_RUNNING);

		mutex_lock(&df->lock);
		update_devfreq(df);
		mutex_unlock(&df->lock);
	}

	return 0;
}

static void alt_dvfs_nop_timer(struct timer_list *timer)
{
}

/*timer callback function send a signal */
static void simple_interactive_timer(struct timer_list *timer)
{
	struct devfreq_simple_interactive_data *gov_data = from_timer(gov_data, timer, freq_timer);

	wake_up_process(gov_data->change_freq_task);
}


static int exynos_devfreq_alt_mode_change(struct devfreq *df, int new_mode)
{
	struct devfreq_simple_interactive_data *gov_data = df->governor_data;
	struct devfreq_alt_dvfs_data *alt_data = &gov_data->alt_data;

	if (new_mode < alt_data->num_modes) {
		alt_data->current_mode = new_mode;
		if (new_mode != -1)
			alt_data->alt_param = &alt_data->alt_param_set[new_mode];
		else
			alt_data->alt_param = alt_data->alt_user_mode;
	} else {
		pr_err("There has no mode number %d", new_mode);
		return -EINVAL;
	}

	return 0;
}

static int exynos_interactive_sysbusy_callback(struct devfreq *df,
		enum sysbusy_state state)
{
	int new_mode;

	new_mode = !!(state > SYSBUSY_STATE0);
	return exynos_devfreq_alt_mode_change(df, new_mode);
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


static int change_target_load(struct exynos_devfreq_data *data, const char *buf)
{
	int ntokens;
	int *new_target_load = NULL;
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_user_mode = gov_data->alt_data.alt_user_mode;

	new_target_load = get_tokenized_data(buf , &ntokens);
	if (IS_ERR(new_target_load))
		return PTR_ERR_OR_ZERO(new_target_load);

	mutex_lock(&data->devfreq->lock);
	kfree(alt_user_mode->target_load);
	alt_user_mode->target_load = new_target_load;
	alt_user_mode->num_target_load = ntokens;
	mutex_unlock(&data->devfreq->lock);

	return 0;
}
/* Show Current ALT Parameter Info */
static ssize_t show_current_target_load(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_param = gov_data->alt_data.alt_param;
	ssize_t count = 0;
	int i;

	mutex_lock(&data->devfreq->lock);
	for (i = 0; i < alt_param->num_target_load; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "%d%s",
				  alt_param->target_load[i],
				  (i == alt_param->num_target_load - 1) ?
				  "" : (i % 2) ? ":" : " ");
	}
	count += snprintf(buf + count, PAGE_SIZE, "\n");
	mutex_unlock(&data->devfreq->lock);
	return count;
}

static ssize_t show_current_hold_sample_time(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_param = gov_data->alt_data.alt_param;
	ssize_t count = 0;

	mutex_lock(&data->devfreq->lock);
	count += snprintf(buf, PAGE_SIZE, "%u\n",
			  alt_param->hold_sample_time);
	mutex_unlock(&data->devfreq->lock);
	return count;
}


/* Show and Store User ALT Paramter Info */
static ssize_t show_user_target_load(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_user_mode = gov_data->alt_data.alt_user_mode;
	ssize_t count = 0;
	int i;

	mutex_lock(&data->devfreq->lock);
	for (i = 0; i < alt_user_mode->num_target_load; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "%d%s",
				  alt_user_mode->target_load[i],
				  (i == alt_user_mode->num_target_load - 1) ?
				  "" : (i % 2) ? ":" : " ");
	}
	count += snprintf(buf + count, PAGE_SIZE, "\n");
	mutex_unlock(&data->devfreq->lock);
	return count;
}

static ssize_t store_user_target_load(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);

	change_target_load(data, buf);

	return count;
}

static ssize_t show_user_hold_sample_time(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_user_mode = gov_data->alt_data.alt_user_mode;
	ssize_t count = 0;

	mutex_lock(&data->devfreq->lock);
	count += snprintf(buf, PAGE_SIZE, "%u\n",
			  alt_user_mode->hold_sample_time);
	mutex_unlock(&data->devfreq->lock);
	return count;
}

static ssize_t store_user_hold_sample_time(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_user_mode = gov_data->alt_data.alt_user_mode;
	int ret;

	mutex_lock(&data->devfreq->lock);
	ret = sscanf(buf, "%u", &alt_user_mode->hold_sample_time);
	mutex_unlock(&data->devfreq->lock);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t show_user_hispeed_load(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_user_mode = gov_data->alt_data.alt_user_mode;
	ssize_t count = 0;

	mutex_lock(&data->devfreq->lock);
	count += snprintf(buf, PAGE_SIZE, "%u\n",
			  alt_user_mode->hispeed_load);
	mutex_unlock(&data->devfreq->lock);
	return count;
}

static ssize_t store_user_hispeed_load(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_user_mode = gov_data->alt_data.alt_user_mode;
	int ret;

	mutex_lock(&data->devfreq->lock);
	ret = sscanf(buf, "%u", &alt_user_mode->hispeed_load);
	mutex_unlock(&data->devfreq->lock);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t show_user_hispeed_freq(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_user_mode = gov_data->alt_data.alt_user_mode;
	ssize_t count = 0;

	mutex_lock(&data->devfreq->lock);
	count += snprintf(buf, PAGE_SIZE, "%u\n",
			  alt_user_mode->hispeed_freq);
	mutex_unlock(&data->devfreq->lock);
	return count;
}

static ssize_t store_user_hispeed_freq(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_param *alt_user_mode = gov_data->alt_data.alt_user_mode;
	int ret;

	mutex_lock(&data->devfreq->lock);
	ret = sscanf(buf, "%u", &alt_user_mode->hispeed_freq);
	mutex_unlock(&data->devfreq->lock);
	if (ret != 1)
		return -EINVAL;

	return count;
}

/* Sysfs for ALT mode Info */
static ssize_t show_current_mode(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_data *alt_data = &gov_data->alt_data;
	ssize_t count = 0;

	mutex_lock(&data->devfreq->lock);
	count += snprintf(buf, PAGE_SIZE, "%d\n", alt_data->current_mode);
	mutex_unlock(&data->devfreq->lock);
	return count;
}

static ssize_t store_current_mode(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int new_mode;
	int ret;

	ret = sscanf(buf, "%d", &new_mode);
	if (ret != 1)
		return -EINVAL;

	if (new_mode < -1)
		return -EINVAL;

	mutex_lock(&data->devfreq->lock);
	exynos_devfreq_alt_mode_change(data->devfreq, new_mode);
	mutex_unlock(&data->devfreq->lock);

	return count;
}

static ssize_t show_default_mode(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_data *alt_data = &gov_data->alt_data;
	ssize_t count = 0;

	mutex_lock(&data->devfreq->lock);
	count += snprintf(buf, PAGE_SIZE, "%d\n", alt_data->default_mode);
	mutex_unlock(&data->devfreq->lock);
	return count;
}

/* Show whole ALT DVFS Info */
static void print_alt_dvfs_info(struct devfreq_alt_dvfs_param *alt_param, char *buf, ssize_t *count)
{
	int i;

	for (i = 0; i < alt_param->num_target_load; i++) {
		*count += snprintf(buf + *count, PAGE_SIZE, "%d%s",
				  alt_param->target_load[i],
				  (i == alt_param->num_target_load - 1) ?
				  "" : (i % 2) ? ":" : " ");
	}
	*count += snprintf(buf + *count, PAGE_SIZE, "\n");
	/* Parameters */
	*count += snprintf(buf + *count, PAGE_SIZE, "MIN SAMPLE TIME: %u\n",
				alt_param->min_sample_time);
	*count += snprintf(buf + *count, PAGE_SIZE, "HOLD SAMPLE TIME: %u\n",
				alt_param->hold_sample_time);
	*count += snprintf(buf + *count, PAGE_SIZE, "HISPEED LOAD: %u\n",
				alt_param->hispeed_load);
	*count += snprintf(buf + *count, PAGE_SIZE, "HISPEED FREQ: %u\n",
				alt_param->hispeed_freq);

}

static ssize_t show_alt_dvfs_info(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
						    struct platform_device,
						    dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_data *alt_data = &gov_data->alt_data;
	struct devfreq_alt_dvfs_param *alt_param_set = gov_data->alt_data.alt_param_set;
	ssize_t count = 0;
	int i;

	mutex_lock(&data->devfreq->lock);

	count += snprintf(buf + count, PAGE_SIZE, "Current Mode >> %d, # Modes >> %d\n", alt_data->current_mode, alt_data->num_modes);

	for (i = 0; i < alt_data->num_modes; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "\n<< MODE %d >>\n", i);
		print_alt_dvfs_info(&alt_param_set[i], buf, &count);
	}

	count += snprintf(buf + count, PAGE_SIZE, "\n<< MODE USER >>\n");
	print_alt_dvfs_info(alt_data->alt_user_mode, buf, &count);

	mutex_unlock(&data->devfreq->lock);

	return count;
}

#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
/* Show Load Tracking Information */
static ssize_t store_load_tracking(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
			struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_data *alt_data = &gov_data->alt_data;
	unsigned int enable;

	sscanf(buf, "%u", &enable);

	if (enable == 1) {
		if (alt_data->log == NULL)
			alt_data->log = vmalloc(sizeof(struct devfreq_alt_load) *
				(MSEC_PER_SEC / alt_data->alt_param->min_sample_time * MAX_LOG_TIME));
		alt_data->log_top = 0;
		alt_data->load_track = true;
	}
	else if (enable == 0) {
		alt_data->load_track = false;
	}

	return count;
}

static ssize_t show_load_tracking(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t offset, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent,
			struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_interactive_data *gov_data = data->devfreq->governor_data;
	struct devfreq_alt_dvfs_data *alt_data = &gov_data->alt_data;
	char line[128];
	ssize_t len, size = 0;
	static int printed = 0;
	int i;

	for (i = printed; i < alt_data->log_top; i++) {
		len = snprintf(line, 128, "%llu %llu %u %u\n", alt_data->log[i].clock
				, alt_data->log[i].delta, alt_data->log[i].load, alt_data->log[i].alt_freq);
		if (len + size <= count) {
			memcpy(buf + size, line, len);
			size += len;
			printed++;
		}
		else
			break;
	}

	if (!size)
		printed = 0;

	return size;
}
#endif /* CONFIG_EXYNOS_ALT_DVFS_DEBUG */

static DEVICE_ATTR(current_target_load, 0440, show_current_target_load, NULL);
static DEVICE_ATTR(current_hold_sample_time, 0440, show_current_hold_sample_time, NULL);
static DEVICE_ATTR(user_target_load, 0640, show_user_target_load, store_user_target_load);
static DEVICE_ATTR(user_hold_sample_time, 0640, show_user_hold_sample_time, store_user_hold_sample_time);
static DEVICE_ATTR(user_hispeed_load, 0640, show_user_hispeed_load, store_user_hispeed_load);
static DEVICE_ATTR(user_hispeed_freq, 0640, show_user_hispeed_freq, store_user_hispeed_freq);
static DEVICE_ATTR(current_mode, 0640, show_current_mode, store_current_mode);
static DEVICE_ATTR(default_mode, 0440, show_default_mode, NULL);
static DEVICE_ATTR(alt_dvfs_info, 0440, show_alt_dvfs_info, NULL);
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
static BIN_ATTR(load_tracking, 0640, show_load_tracking, store_load_tracking, 0);
#endif
static struct attribute *devfreq_interactive_sysfs_entries[] = {
	&dev_attr_current_target_load.attr,
	&dev_attr_current_hold_sample_time.attr,
	&dev_attr_user_target_load.attr,
	&dev_attr_user_hold_sample_time.attr,
	&dev_attr_user_hispeed_load.attr,
	&dev_attr_user_hispeed_freq.attr,
	&dev_attr_current_mode.attr,
	&dev_attr_default_mode.attr,
	&dev_attr_alt_dvfs_info.attr,
	NULL,
};

#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
static struct bin_attribute *devfreq_interactive_sysfs_bin_entries[] = {
	&bin_attr_load_tracking,
	NULL,
};
#endif

static struct attribute_group devfreq_interactive_attr_group = {
	.name = "interactive",
	.attrs = devfreq_interactive_sysfs_entries,
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS_DEBUG)
	.bin_attrs = devfreq_interactive_sysfs_bin_entries,
#endif
};

void register_get_dev_status(struct exynos_devfreq_data *data, struct devfreq *devfreq,
		struct device_node *get_dev_np);
static int devfreq_simple_interactive_parse_dt(struct exynos_devfreq_data *data,
		struct devfreq_simple_interactive_data *gov_data,
		struct devfreq *devfreq)
{
	struct device_node *np = data->dev->of_node;
	struct device_node *governor_np, *get_dev_np;
	int ntokens;
	const char *buf;
	/* Parse ALT-DVFS related parameters */
	int default_mode, i = 0;
	struct device_node *alt_mode, *child;
	struct devfreq_alt_dvfs_data *alt_data = &gov_data->alt_data;
	struct devfreq_alt_dvfs_param *alt_param;

	get_dev_np = of_find_node_by_name(np, "get_dev");
	if (!get_dev_np) {
		dev_err(data->dev, "There is no get_dev node\n");
		return -EINVAL;
	}

	register_get_dev_status(data, devfreq, get_dev_np);

	governor_np = of_find_node_by_name(np, "governor_data");
	if (!governor_np) {
		dev_err(data->dev, "There is no governor_data node\n");
		return -EINVAL;
	}

	alt_mode = of_find_node_by_name(governor_np, "alt_mode");

	of_property_read_u32(alt_mode, "default_mode", &default_mode);
	alt_data->default_mode = alt_data->current_mode = default_mode;

	alt_data->num_modes = of_get_child_count(alt_mode);
	alt_data->alt_param_set = kzalloc(sizeof(struct devfreq_alt_dvfs_param) * alt_data->num_modes, GFP_KERNEL);

	for_each_available_child_of_node(alt_mode, child) {
		alt_param = &(alt_data->alt_param_set[i++]);

		if (!of_property_read_string(child, "target_load", &buf)) {
			/* Parse target load table */
			alt_param->target_load =
			get_tokenized_data(buf, &ntokens);
			alt_param->num_target_load = ntokens;
		} else {
			/* Fix target load as defined ALTDVFS_TARGET_LOAD */
			alt_param->target_load =
			kmalloc(sizeof(unsigned int), GFP_KERNEL);
			if(!alt_param->target_load) {
				dev_err(data->dev, "Failed to allocate memory\n");
				return -ENOMEM;
			}
			*(alt_param->target_load) = ALTDVFS_TARGET_LOAD;
			alt_param->num_target_load = ALTDVFS_NUM_TARGET_LOAD;
		}

		if (of_property_read_u32(child, "min_sample_time", &alt_param->min_sample_time))
			alt_param->min_sample_time = ALTDVFS_MIN_SAMPLE_TIME;
		if (of_property_read_u32(child, "hold_sample_time", &alt_param->hold_sample_time))
			alt_param->hold_sample_time = ALTDVFS_HOLD_SAMPLE_TIME;
		if (of_property_read_u32(child, "hispeed_load", &alt_param->hispeed_load))
			alt_param->hispeed_load = ALTDVFS_HISPEED_LOAD;
		if (of_property_read_u32(child, "hispeed_freq", &alt_param->hispeed_freq))
			alt_param->hispeed_freq = ALTDVFS_HISPEED_FREQ;
		if (of_property_read_u32(child, "tolerance", &alt_param->tolerance))
			alt_param->tolerance = ALTDVFS_TOLERANCE;
		dev_info(data->dev, "[%s] p1 %d\n", __func__, alt_param->min_sample_time);
	}

	/* Initial buffer and load setup */
	alt_data->front = alt_data->buffer;
	alt_data->rear = alt_data->buffer;
	alt_data->min_load = 100;

	alt_data->alt_param = &alt_data->alt_param_set[default_mode];
	dev_info(data->dev, "[%s] default mode %d\n", __func__, default_mode);

	/* copy default parameter to user param */
	alt_data->alt_user_mode = kzalloc(sizeof(struct devfreq_alt_dvfs_param), GFP_KERNEL);
	memcpy(alt_data->alt_user_mode, &alt_data->alt_param_set[default_mode], sizeof(struct devfreq_alt_dvfs_param));
	alt_data->alt_user_mode->num_target_load = alt_data->alt_param->num_target_load;
	alt_data->alt_user_mode->target_load = kzalloc(sizeof(unsigned int) * alt_data->alt_param->num_target_load, GFP_KERNEL);
	memcpy(alt_data->alt_user_mode->target_load, alt_data->alt_param_set[default_mode].target_load, sizeof(unsigned int) * alt_data->alt_param->num_target_load);

	/* Initial governor freq setup */
	gov_data->governor_freq = 0;

	return 0;
}

static int devfreq_simple_interactive_start(struct devfreq *df)
{
	int ret;
	struct exynos_devfreq_data *data = df->data;
	struct devfreq_simple_interactive_data *gov_data;

	gov_data = kzalloc(sizeof(struct devfreq_simple_interactive_data), GFP_KERNEL);
	df->governor_data = gov_data;

	devfreq_simple_interactive_parse_dt(data, gov_data, df);

	/* timer of governor for delay time initialize */
	timer_setup(&gov_data->freq_timer, simple_interactive_timer, TIMER_DEFERRABLE);
	timer_setup(&gov_data->freq_slack_timer, alt_dvfs_nop_timer, 0);

	gov_data->change_freq_task = kthread_create(devfreq_change_freq_task, df, "simpleinteractive");

	if (IS_ERR(gov_data->change_freq_task)) {
		pr_err("%s: failed kthread_create for simpleinteractive governor\n", __func__);
		ret = PTR_ERR(gov_data->change_freq_task);

		destroy_timer_on_stack(&gov_data->freq_timer);
		destroy_timer_on_stack(&gov_data->freq_slack_timer);
		goto err;
	}
	data->sysbusy_gov_callback = exynos_interactive_sysbusy_callback;

	gov_data->freq_timer.expires = jiffies +
		msecs_to_jiffies(gov_data->alt_data.alt_param->min_sample_time * 2);
	add_timer(&gov_data->freq_timer);
	gov_data->freq_slack_timer.expires = jiffies +
		msecs_to_jiffies(gov_data->alt_data.alt_param->hold_sample_time);
	add_timer_on(&gov_data->freq_slack_timer, BOUND_CPU_NUM);

	wake_up_process(gov_data->change_freq_task);

	ret = sysfs_create_group(&df->dev.kobj, &devfreq_interactive_attr_group);
	if (ret)
		dev_warn(&df->dev, "failed create sysfs for devfreq data\n");

	return 0;

err:
	return ret;
}

static int devfreq_simple_interactive_stop(struct devfreq *df)
{
	int ret = 0;
	struct exynos_devfreq_data *data = df->data;
	struct devfreq_simple_interactive_data *gov_data = df->governor_data;

	if (!gov_data)
		return -EINVAL;

	data->sysbusy_gov_callback = NULL;
	destroy_timer_on_stack(&gov_data->freq_timer);
	kthread_stop(gov_data->change_freq_task);

	return ret;
}

static int devfreq_simple_interactive_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	int ret;

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = devfreq_simple_interactive_start(devfreq);
		if (ret)
			return ret;
		break;
	case DEVFREQ_GOV_STOP:
		ret = devfreq_simple_interactive_stop(devfreq);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_simple_interactive = {
	.name = "interactive",
	.get_target_freq = devfreq_simple_interactive_func,
	.event_handler = devfreq_simple_interactive_handler,
};

int devfreq_simple_interactive_init(void)
{
	return devfreq_add_governor(&devfreq_simple_interactive);
}

MODULE_LICENSE("GPL");
