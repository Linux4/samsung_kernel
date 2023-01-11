/*
 * gpufreq_ondemand.c
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
#define DEF_FREQUENCY_UP_THRESHOLD          (90)
#define DEF_FREQUENCY_DOWN_DIFFERENTIAL     (10)
#define MIN_FREQUENCY_UP_THRESHOLD          (11)
#define MAX_FREQUENCY_UP_THRESHOLD          (100)

#define DEF_DESIRED_HIGH_FREQ               (HZ_TO_KHZ(312000000))
#define USE_POLICY_POWER_BENCH              (1)

static int gpufreq_governor_ondemand(struct gpufreq_policy *policy,
                                     unsigned int event);
static int gpufreq_gov_ondemand_init(void);
static void gpufreq_gov_ondemand_exit(void);

/*********************************************************************
 *                         parameters declaration                    *
 *********************************************************************/
struct gpufreq_governor gpufreq_gov_ondemand = {
    .name       = "ondemand",
    .init       = gpufreq_gov_ondemand_init,
    .exit       = gpufreq_gov_ondemand_exit,
    .governor   = gpufreq_governor_ondemand,
    .refs       = 0,
//    .owner      = THIS_MODULE,
};

struct gpufreq_ondemand_info_s {
    int                     gpu;
    struct gpufreq_policy   *cur_policy;
    void                    *timer;
    struct mutex            timer_mutex;
    int                     ref;
    int                     enabled;
};

static struct gpufreq_ondemand_info_s ondemand_info_s[GPUFREQ_GPU_NUMS];
static unsigned int min_sampling_rate;
static unsigned int desired_high_freq;

static atomic_t ondemand_enable[GPUFREQ_GPU_NUMS] = {ATOMIC_INIT(0)};

struct ondemand_tuners {
    unsigned int sampling_rate;
    unsigned int up_threshold;
    unsigned int down_differential;
    unsigned int sampling_down_factor;
};

static struct ondemand_tuners ondemand_tuners_ins[] = {
    [0] = {
        .up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
        .sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
        .down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL,
    },

    [1] = {
        .up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
        .sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
        .down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL,
    },

#if MRVL_CONFIG_SHADER_CLK_CONTROL
    [2] = {
        .up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
        .sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
        .down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL,
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
    return sprintf(buf, "%u\n", ondemand_tuners_ins[a->gpu].element);       \
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
    {
        return -EINVAL;
    }

    ondemand_tuners_ins[a->gpu].sampling_rate = max(input, min_sampling_rate);
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
        input < MIN_FREQUENCY_UP_THRESHOLD ||
        input > MAX_FREQUENCY_UP_THRESHOLD)
    {
        return -EINVAL;
    }

    ondemand_tuners_ins[a->gpu].up_threshold = input;
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

    ondemand_tuners_ins[a->gpu].sampling_down_factor = input;
    return count;
}

gpufreq_gov_attr_rw(sampling_rate);
gpufreq_gov_attr_rw(up_threshold);
gpufreq_gov_attr_rw(sampling_down_factor);

static struct attribute *ondemand_default_attributes[] = {
    &gov_sampling_rate_min.attr,
    &gov_sampling_rate.attr,
    &gov_up_threshold.attr,
    &gov_sampling_down_factor.attr,
    NULL
};

static struct attribute_group ondemand_default_attr_group = {
    .attrs = ondemand_default_attributes,
    .name = "ondemand",
};

/*********************************************************************
 *                         governor interface                        *
 *********************************************************************/
#if USE_POLICY_POWER_BENCH
static void gov_policy_gpubench(struct gpufreq_ondemand_info_s *ondemand_info)
{
    int load;
    unsigned int gpu = ondemand_info->gpu;
    struct gpufreq_policy * cur_policy = ondemand_info->cur_policy;
    struct ondemand_tuners *ondem_tuners_ins = &ondemand_tuners_ins[gpu];

    load = gpufreq_get_gpu_load(gpu, ondem_tuners_ins->sampling_rate);
    debug_log(GPUFREQ_LOG_DEBUG, "gpu: %d, cur: %d, load %d\n", cur_policy->gpu, cur_policy->cur, load);
    if(load < 0)
    {
        debug_log(GPUFREQ_LOG_WARNING, "fail to get gpu(%d) work load\n", gpu);
        return;
    }

    trace_gc_workload(cur_policy->gpu, cur_policy->cur, load);

    /**************************************
     * check for frequency INCREASE
     **************************************/
    /*
     * The reasonable way to increase frequency is not to make it jump
     * to max speed, but to a desired middle level rate. If the work
     * load which is still higher than up_threshold in the consective
     * sampling point should trigger up_to_max policy.
     */
    if(load >= ondem_tuners_ins->up_threshold)
    {
        unsigned int new_freq;
        unsigned int relation;

        new_freq = cur_policy->max;
        relation = GPUFREQ_RELATION_H;

        if(ondemand_info->ref == 0)
        {
            if(cur_policy->cur == cur_policy->min)
            {
                new_freq = desired_high_freq;
                relation = GPUFREQ_RELATION_L;
            }
            else
            {
                /* aggressive policy, jump to high frequency */
                new_freq = cur_policy->max * load / 100;
                relation = GPUFREQ_RELATION_L;
            }
        }

        ondemand_info->ref++;
        __gpufreq_driver_target(cur_policy, new_freq, relation);
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
    if(load < (ondem_tuners_ins->up_threshold - ondem_tuners_ins->down_differential))
    {
        unsigned int new_freq;

        new_freq = cur_policy->cur * load /
            (ondem_tuners_ins->up_threshold -
            ondem_tuners_ins->down_differential);

        if(new_freq < cur_policy->min)
            new_freq = cur_policy->min;

        ondemand_info->ref = 0;
        __gpufreq_driver_target(cur_policy, new_freq, GPUFREQ_RELATION_L);
    }
}
#else
static void gov_policy_dbs(struct gpufreq_ondemand_info_s *ondemand_info)
{
    int load, load_freq;
    unsigned int gpu = ondemand_info->gpu;
    struct gpufreq_policy * cur_policy = ondemand_info->cur_policy;
    struct ondemand_tuners *ondem_tuners_ins = &ondemand_tuners_ins[gpu];

    load = gpufreq_get_gpu_load(gpu, ondem_tuners_ins->sampling_rate);
    debug_log(GPUFREQ_LOG_DEBUG, "load %d\n", load);
    if(load < 0)
    {
        debug_log(GPUFREQ_LOG_WARNING, "fail to get gpu work load\n");
        return;
    }

    /**************************************
     * check for frequency INCREASE
     **************************************/
    if(load >= ondem_tuners_ins->up_threshold)
    {
        __gpufreq_driver_target(cur_policy, cur_policy->max, GPUFREQ_RELATION_H);
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
    if(load < (ondem_tuners_ins->up_threshold - ondem_tuners_ins->down_differential))
    {
        unsigned int new_freq;

        new_freq = cur_policy->cur * load /
            (ondem_tuners_ins->up_threshold -
            ondem_tuners_ins->down_differential);

        if(new_freq < cur_policy->min)
            new_freq = cur_policy->min;

        __gpufreq_driver_target(cur_policy, new_freq, GPUFREQ_RELATION_L);
    }
}
#endif

static void gov_check_gpu_interval(struct gpufreq_ondemand_info_s *ondemand_info)
{
#if USE_POLICY_POWER_BENCH
    /* policy references gpu bench data */
    gov_policy_gpubench(ondemand_info);
#else
    /* policy is alike cpufreq ondemand governor */
    gov_policy_dbs(ondemand_info);
#endif
}

static void do_ondemand_timer(void* data)
{
    struct gpufreq_ondemand_info_s *this_gov_info = (struct gpufreq_ondemand_info_s *)data;
    unsigned int delay = ondemand_tuners_ins[this_gov_info->gpu].sampling_rate;

    if (this_gov_info->enabled == 0)
        return;

    mutex_lock(&this_gov_info->timer_mutex);

//    debug_log(GPUFREQ_LOG_DEBUG, "do_ondemand_timer...\n");
    gov_check_gpu_interval(this_gov_info);

    /* schedule rountine for next time */
    gpufreq_start_timer(this_gov_info->timer, delay);
    mutex_unlock(&this_gov_info->timer_mutex);
}

static inline void gov_ondemand_init(struct gpufreq_ondemand_info_s *ondemand_info)
{
    unsigned int delay = 1;

    ondemand_info->enabled = 1;
    gpufreq_start_timer(ondemand_info->timer, delay);
}

static inline void gov_ondemand_exit(struct gpufreq_ondemand_info_s *ondemand_info)
{
    ondemand_info->enabled = 0;
    gpufreq_stop_timer(ondemand_info->timer);
}

static inline void gov_ondemand_suspend(struct gpufreq_ondemand_info_s *ondemand_info)
{
    gov_ondemand_exit(ondemand_info);
}

static inline void gov_ondemand_resume(struct gpufreq_ondemand_info_s *ondemand_info)
{
    gov_ondemand_init(ondemand_info);
}

/* govenor callback function */
static int gpufreq_governor_ondemand(struct gpufreq_policy *policy,
                                     unsigned int event)
{
    unsigned int gpu = policy->gpu;
    struct gpufreq_ondemand_info_s *this_gov_info;
    int ret = 0, ret0;

    this_gov_info = &ondemand_info_s[gpu];

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

        if(atomic_inc_return(&ondemand_enable[gpu]) == 1)
        {
            ret0 = sysfs_create_group(&policy->kobj, &ondemand_default_attr_group);
            if(ret0)
                return ret0;

            ondemand_tuners_ins[gpu].sampling_rate = min_sampling_rate;
        }

        this_gov_info->ref = 0;

        mutex_init(&this_gov_info->timer_mutex);
        /* create governor timer to check gpu */
        gov_ondemand_init(this_gov_info);
        break;

    case GPUFREQ_GOV_EVENT_STOP:
        /* destory governor timer */
        gov_ondemand_exit(this_gov_info);
        mutex_destroy(&this_gov_info->timer_mutex);

        if(atomic_dec_and_test(&ondemand_enable[gpu]))
        {
            sysfs_remove_group(&policy->kobj, &ondemand_default_attr_group);
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
        debug_log(GPUFREQ_LOG_INFO, "ondemand governor of gpu %u is suspended\n", this_gov_info->cur_policy->gpu);
        gov_ondemand_suspend(this_gov_info);
        break;

    case GPUFREQ_GOV_EVENT_RESUME:
        debug_log(GPUFREQ_LOG_INFO, "ondemand governor of gpu %u is resumed\n", this_gov_info->cur_policy->gpu);
        gov_ondemand_resume(this_gov_info);
        break;
    }

    return ret;
}

static int gpufreq_gov_ondemand_init(void)
{
    int err, gpu;

    min_sampling_rate = DEF_MIN_SAMPLING_RATE; /* Milliseconds */
    desired_high_freq = DEF_DESIRED_HIGH_FREQ;

    err = gpufreq_register_governor(&gpufreq_gov_ondemand);
    if(err)
        return err;

    /* successfully registered, then init work. */
    for_each_gpu(gpu)
    {
        if(gcvCORE_SH != gpu || has_feat_shader_indept_dfc())
            gpufreq_create_timer(do_ondemand_timer,
                                &ondemand_info_s[gpu],
                                &ondemand_info_s[gpu].timer);
    }

    return 0;
}

static void gpufreq_gov_ondemand_exit(void)
{
    gpufreq_unregister_governor(&gpufreq_gov_ondemand);
}

#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */
