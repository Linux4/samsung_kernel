/*
 * gpufreq_conservative.c
 *
 * Author: Watson Wang <zswang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */
#include "gpufreq.h"

#if MRVL_CONFIG_ENABLE_GPUFREQ
#include <linux/kernel.h>
#include <linux/atomic.h>

#include "gc_trace.h"

#define DEF_SAMPLING_DOWN_FACTOR            (1)
#define MAX_SAMPLING_DOWN_FACTOR            (100000)
#define DEF_MIN_SAMPLING_RATE               (50)
#define DEF_FREQUENCY_UP_THRESHOLD          (85)
#define DEF_FREQUENCY_DOWN_THRESHOLD        (60)
#define DEF_FREQUENCY_FREQ_STEP             (10)
#define MIN_FREQUENCY_THRESHOLD             (11)
#define MAX_FREQUENCY_THRESHOLD             (100)

static int gpufreq_governor_conservative(struct gpufreq_policy *policy,
                                     unsigned int event);
static int gpufreq_gov_conservative_init(void);
static void gpufreq_gov_conservative_exit(void);

/*********************************************************************
 *                         parameters declaration                    *
 *********************************************************************/
struct gpufreq_governor gpufreq_gov_conservative = {
    .name       = "conservative",
    .init       = gpufreq_gov_conservative_init,
    .exit       = gpufreq_gov_conservative_exit,
    .governor   = gpufreq_governor_conservative,
    .refs       = 0,
//    .owner      = THIS_MODULE,
};

struct gpufreq_conservative_info_s {
    int                     gpu;
    struct gpufreq_policy   *cur_policy;
    struct delayed_work     work;
    struct mutex            timer_mutex;
    unsigned int            requested_freq;
    unsigned int            enabled;
};

static struct gpufreq_conservative_info_s conservative_info_s[GPUFREQ_GPU_NUMS];
static unsigned int min_sampling_rate;

static atomic_t conservative_enable[GPUFREQ_GPU_NUMS] = {ATOMIC_INIT(0)};

struct conservative_tuners {
    unsigned int sampling_rate;
    unsigned int up_threshold;
    unsigned int down_threshold;
    unsigned int freq_step;
    unsigned int sampling_down_factor;
};

static struct conservative_tuners conservative_tuners_ins[] = {
    [0] = {
        .up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
        .down_threshold = DEF_FREQUENCY_DOWN_THRESHOLD,
        .sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
        .freq_step = DEF_FREQUENCY_FREQ_STEP,
    },

    [1] = {
        .up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
        .down_threshold = DEF_FREQUENCY_DOWN_THRESHOLD,
        .sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
        .freq_step = DEF_FREQUENCY_FREQ_STEP,
    },

#if MRVL_CONFIG_SHADER_CLK_CONTROL
    [2] = {
        .up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
        .down_threshold = DEF_FREQUENCY_DOWN_THRESHOLD,
        .sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
        .freq_step = DEF_FREQUENCY_FREQ_STEP,
    },
#endif
};

/*********************************************************************
 *                         sysfs interfaces                          *
 *********************************************************************/
#define gpufreq_gov_attr_ro(_name)          \
static struct gpufreq_freq_attr gov_##_name =       \
    __ATTR(_name, 0444, show_##_name, NULL)

#define gpufreq_gov_attr_rw(_name)          \
static struct gpufreq_freq_attr gov_##_name =       \
    __ATTR(_name, 0644, show_##_name, store_##_name)

#define show_attr(gov_name, element)                                \
static ssize_t show_##gov_name(struct gpufreq_policy *a, char *buf) \
{                                                                   \
    return sprintf(buf, "%u\n", conservative_tuners_ins[a->gpu].element);       \
}

/* sampling_rate_min */
static ssize_t show_sampling_rate_min(struct gpufreq_policy *a, char *buf)
{
    return sprintf(buf, "%u\n", min_sampling_rate);
}
gpufreq_gov_attr_ro(sampling_rate_min);

/* sampling_rate */
show_attr(sampling_rate, sampling_rate);
static ssize_t store_sampling_rate(struct gpufreq_policy *a,
                        const char *buf, size_t count)
{
    int ret;
    unsigned int input;

    ret = sscanf(buf, "%u", &input);

    if (ret != 1)
        return -EINVAL;

    conservative_tuners_ins[a->gpu].sampling_rate = max(input, min_sampling_rate);
    return count;
}

/* up_threshold */
show_attr(up_threshold, up_threshold);
static ssize_t store_up_threshold(struct gpufreq_policy *a,
                        const char *buf, size_t count)
{
    int ret;
    unsigned int input;

    ret = sscanf(buf, "%u", &input);

    if (ret != 1 ||
        input > MAX_FREQUENCY_THRESHOLD ||
        input <= conservative_tuners_ins[a->gpu].down_threshold)
    {
        return -EINVAL;
    }

    conservative_tuners_ins[a->gpu].up_threshold = input;
    return count;
}

/* down_threshold */
show_attr(down_threshold, down_threshold);
static ssize_t store_down_threshold(struct gpufreq_policy *a,
                        const char *buf, size_t count)
{
    int ret;
    unsigned int input;

    ret = sscanf(buf, "%u", &input);

    if (ret != 1 ||
        input < MIN_FREQUENCY_THRESHOLD ||
        input > MAX_FREQUENCY_THRESHOLD ||
        input >= conservative_tuners_ins[a->gpu].up_threshold)
    {
        return -EINVAL;
    }

    conservative_tuners_ins[a->gpu].down_threshold = input;
    return count;
}

/* sampling_down_factor */
show_attr(sampling_down_factor, sampling_down_factor);
static ssize_t store_sampling_down_factor(struct gpufreq_policy *a,
                        const char *buf, size_t count)
{
    int ret;
    unsigned int input;

    ret = sscanf(buf, "%u", &input);

    if (ret != 1 || input < 1 ||
        input > MAX_SAMPLING_DOWN_FACTOR)
    {
        return -EINVAL;
    }

    conservative_tuners_ins[a->gpu].sampling_down_factor = input;
    return count;
}

/* freq_step */
show_attr(freq_step, freq_step);
static ssize_t store_freq_step(struct gpufreq_policy *a,
                        const char *buf, size_t count)
{
    int ret;
    unsigned int input;

    ret = sscanf(buf, "%u", &input);

    if (ret != 1)
        return -EINVAL;

    if (input < 1)
        input = 1;

    if (input > 100)
        input = 100;

    conservative_tuners_ins[a->gpu].freq_step = input;
    return count;
}

gpufreq_gov_attr_rw(sampling_rate);
gpufreq_gov_attr_rw(up_threshold);
gpufreq_gov_attr_rw(down_threshold);
gpufreq_gov_attr_rw(sampling_down_factor);
gpufreq_gov_attr_rw(freq_step);

static struct attribute *conservative_default_attributes[] = {
    &gov_sampling_rate_min.attr,
    &gov_sampling_rate.attr,
    &gov_up_threshold.attr,
    &gov_down_threshold.attr,
    &gov_sampling_down_factor.attr,
    &gov_freq_step.attr,
    NULL
};

static struct attribute_group conservative_default_attr_group = {
    .attrs = conservative_default_attributes,
    .name = "conservative",
};

/*********************************************************************
 *                         governor interface                        *
 *********************************************************************/
static int gov_gpufreq_notifier_call(struct notifier_block *nb,
                    unsigned long action, void *data)
{
    struct gpufreq_freqs *freqs = (struct gpufreq_freqs *)data;
    struct gpufreq_conservative_info_s * this_gov_info = &conservative_info_s[freqs->gpu];
    struct gpufreq_policy *policy = this_gov_info->cur_policy;

    if(this_gov_info->enabled == 0)
        return NOTIFY_DONE;

    st_trans(action);

    /*
     * We only update internal tracked freq when it's out of the ranges
     * of available frequency, otherwise we leave it untouched.
     */
    switch(action)
    {
    case GPUFREQ_PRECHANGE:
    case GPUFREQ_POSTCHANGE:
        if((this_gov_info->requested_freq > policy->max
            || this_gov_info->requested_freq < policy->min)
           && (freqs->new_freq != freqs->old_freq))
        {
            this_gov_info->requested_freq = freqs->new_freq;
        }
        break;
    }

    return NOTIFY_DONE;
}

static struct notifier_block conservative_gpufreq_notifier_block = {
    .notifier_call = gov_gpufreq_notifier_call,
};

static void gov_policy_dfc(struct gpufreq_conservative_info_s *conservative_info)
{
    int load;
    unsigned int freq_differential;
    unsigned int gpu = conservative_info->gpu;
    struct gpufreq_policy * cur_policy = conservative_info->cur_policy;
    struct conservative_tuners *conserv_tuners_ins = &conservative_tuners_ins[gpu];

    load = gpufreq_get_gpu_load(gpu, conserv_tuners_ins->sampling_rate);
    debug_log(GPUFREQ_LOG_DEBUG, "load %d\n", load);
    if(load < 0)
    {
        debug_log(GPUFREQ_LOG_WARNING, "fail to get gpu work load\n");
        return;
    }

    trace_gc_workload(cur_policy->gpu, cur_policy->cur, load);

    if(conserv_tuners_ins->freq_step == 0)
        return;

    /**************************************
     * check for frequency INCREASE
     **************************************/
    /*
     * Every sampling_rate, we check, if current busy time is greater
     * than [up_threshold], then we try to increase frequency. Likewise,
     * if current busy time is less then [down_threshold], then we try to
     * decrease frequency. Every frequency growth/reduction happens at
     * steps of maximum frequency.
     */
    if(load >= conserv_tuners_ins->up_threshold)
    {
        /* bail out early if we're at full speed */
        if(conservative_info->requested_freq == cur_policy->max)
            return;

        freq_differential = (conserv_tuners_ins->freq_step * cur_policy->max) / 100;

        conservative_info->requested_freq += freq_differential;
        if(conservative_info->requested_freq > cur_policy->max)
            conservative_info->requested_freq = cur_policy->max;

        __gpufreq_driver_target(cur_policy, conservative_info->requested_freq, GPUFREQ_RELATION_H);
        return;
    }

    /**************************************
     * check for frequency DECREASE
     **************************************/
    /* current frequency cannot be decreased any more, then bail out. */
    if(cur_policy->cur == cur_policy->min)
        return;

    /*
     * The optimal frequency is the frequency that is the lowest that
     * can support the current GPU usage without triggering the up
     * policy. To be safe, we focus 10 points under the threshold.
     */
    if(load < (conserv_tuners_ins->down_threshold - 10))
    {
        freq_differential = (conserv_tuners_ins->freq_step * cur_policy->max) / 100;

        if(conservative_info->requested_freq >= freq_differential)
            conservative_info->requested_freq -= freq_differential;
        else
            conservative_info->requested_freq = cur_policy->min;

        if(conservative_info->requested_freq < cur_policy->min)
            conservative_info->requested_freq = cur_policy->min;

        /* target freq is highest but not higher than requested freq */
        __gpufreq_driver_target(cur_policy, conservative_info->requested_freq, GPUFREQ_RELATION_H);
    }
}

static inline void gov_check_gpu_interval(struct gpufreq_conservative_info_s *conservative_info)
{
    /* policy is alike cpufreq conservative governor */
    gov_policy_dfc(conservative_info);
}

static void do_conservative_timer(struct work_struct *work)
{
    struct gpufreq_conservative_info_s *this_gov_info =
        container_of(work, struct gpufreq_conservative_info_s, work.work);
    unsigned int delay = msecs_to_jiffies(conservative_tuners_ins[this_gov_info->gpu].sampling_rate);

    if (this_gov_info->enabled == 0)
        return;

    mutex_lock(&this_gov_info->timer_mutex);

    delay -= jiffies % delay;
    gov_check_gpu_interval(this_gov_info);

    /* schedule routine for next time */
    schedule_delayed_work(&this_gov_info->work, delay);
    mutex_unlock(&this_gov_info->timer_mutex);
}

static inline void gov_conservative_suspend(struct gpufreq_conservative_info_s *conservative_info)
{
    conservative_info->enabled = 0;
    cancel_delayed_work_sync(&conservative_info->work);
}

static inline void gov_conservative_resume(struct gpufreq_conservative_info_s *conservative_info)
{
    unsigned int delay;
    unsigned int gpu = conservative_info->gpu;

    delay = msecs_to_jiffies(conservative_tuners_ins[gpu].sampling_rate);
    delay -= jiffies % delay;
    conservative_info->enabled = 1;
    schedule_delayed_work(&conservative_info->work, delay);
}

static inline void gov_conservative_init(struct gpufreq_conservative_info_s *conservative_info)
{
    gov_conservative_resume(conservative_info);
}

static inline void gov_conservative_exit(struct gpufreq_conservative_info_s *conservative_info)
{
    gov_conservative_suspend(conservative_info);
}

/* govenor callback function */
static int gpufreq_governor_conservative(struct gpufreq_policy *policy,
                                     unsigned int event)
{
    unsigned int gpu = policy->gpu;
    struct gpufreq_conservative_info_s *this_gov_info;
    int ret = 0, ret0;

    this_gov_info = &conservative_info_s[gpu];

    switch(event)
    {
    case GPUFREQ_GOV_EVENT_START:
        if(!policy->cur)
        {
            debug_log(GPUFREQ_LOG_ERROR, "cur: %d\n", policy->cur);
            return -EINVAL;
        }

        this_gov_info->cur_policy = policy;
        this_gov_info->gpu        = gpu;
        debug_log(GPUFREQ_LOG_INFO, "start: gpu %d\n", this_gov_info->cur_policy->gpu);

        if(atomic_inc_return(&conservative_enable[gpu]) == 1)
        {
            ret0 = sysfs_create_group(&policy->kobj, &conservative_default_attr_group);
            if(ret0)
                return ret0;

            gpufreq_register_notifier(
                    &conservative_gpufreq_notifier_block,
                    GPUFREQ_TRANSITION_NOTIFIER,
                    gpu);

            conservative_tuners_ins[gpu].sampling_rate = min_sampling_rate;
        }

        this_gov_info->requested_freq = policy->cur;

        mutex_init(&this_gov_info->timer_mutex);
        /* create governor timer to check gpu */
        gov_conservative_init(this_gov_info);
        break;

    case GPUFREQ_GOV_EVENT_STOP:
        /* destory governor timer */
        gov_conservative_exit(this_gov_info);
        mutex_destroy(&this_gov_info->timer_mutex);

        if(atomic_dec_and_test(&conservative_enable[gpu]))
        {
            gpufreq_unregister_notifier(
                    &conservative_gpufreq_notifier_block,
                    GPUFREQ_TRANSITION_NOTIFIER,
                    gpu);
            sysfs_remove_group(&policy->kobj, &conservative_default_attr_group);
        }

        break;

    case GPUFREQ_GOV_EVENT_LIMITS:
        mutex_lock(&this_gov_info->timer_mutex);
        debug_log(GPUFREQ_LOG_INFO, "limit: gpu %d, cur:%u, max %u, min %u\n",
            this_gov_info->cur_policy->gpu, this_gov_info->cur_policy->cur, policy->max, policy->min);
        if(policy->max < this_gov_info->cur_policy->cur)
        {
            __gpufreq_driver_target(this_gov_info->cur_policy,
                policy->max, GPUFREQ_RELATION_H);
        }
        else if(policy->min > this_gov_info->cur_policy->cur)
        {
            __gpufreq_driver_target(this_gov_info->cur_policy,
                policy->min, GPUFREQ_RELATION_L);
        }
        mutex_unlock(&this_gov_info->timer_mutex);
        break;

    case GPUFREQ_GOV_EVENT_SUSPEND:
        debug_log(GPUFREQ_LOG_INFO, "conservative governor of gpu %u is suspended\n", this_gov_info->cur_policy->gpu);
        gov_conservative_suspend(this_gov_info);
        break;

    case GPUFREQ_GOV_EVENT_RESUME:
        debug_log(GPUFREQ_LOG_INFO, "conservative governor of gpu %u is resumed\n", this_gov_info->cur_policy->gpu);
        gov_conservative_resume(this_gov_info);
        break;
    }

    return ret;
}

static int gpufreq_gov_conservative_init(void)
{
    int err, gpu;

    min_sampling_rate = DEF_MIN_SAMPLING_RATE; /* Milliseconds */

    err = gpufreq_register_governor(&gpufreq_gov_conservative);
    if(err)
        return err;

    /* successfully registered, then init work. */
    for_each_gpu(gpu)
    {
        if(gcvCORE_SH != gpu || has_feat_shader_indept_dfc())
        {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
            INIT_DEFERRABLE_WORK(&conservative_info_s[gpu].work,
                                     do_conservative_timer);
#else
            INIT_DELAYED_WORK_DEFERRABLE(&conservative_info_s[gpu].work,
                                         do_conservative_timer);
#endif
        }
    }

    return 0;
}

static void gpufreq_gov_conservative_exit(void)
{
    gpufreq_unregister_governor(&gpufreq_gov_conservative);
}

#endif /* END of MRVL_CONFIG_ENABLE_GPUFREQ */
