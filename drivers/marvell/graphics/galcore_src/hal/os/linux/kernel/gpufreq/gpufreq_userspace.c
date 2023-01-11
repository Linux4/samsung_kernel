/*
 * gpufreq_userspace.c
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

static int gpufreq_governor_userspace(struct gpufreq_policy *policy,
                                     unsigned int event);
static int gpufreq_gov_userspace_init(void);
static void gpufreq_gov_userspace_exit(void);

/*********************************************************************
 *                         parameters declaration                    *
 *********************************************************************/
struct gpufreq_governor gpufreq_gov_userspace = {
    .name       = "userspace",
    .init       = gpufreq_gov_userspace_init,
    .exit       = gpufreq_gov_userspace_exit,
    .governor   = gpufreq_governor_userspace,
    .refs       = 0,
};

struct gpufreq_userspace_info_s {
    unsigned int    userspace_cur_freq;
    unsigned int    userspace_min_freq;
    unsigned int    userspace_max_freq;
    unsigned int    userspace_target_freq;
};

static struct gpufreq_userspace_info_s userspace_info_s[GPUFREQ_GPU_NUMS];

static DEFINE_MUTEX(userspace_mutex);
static atomic_t userspace_enable[GPUFREQ_GPU_NUMS] = {ATOMIC_INIT(0)};

/*********************************************************************
 *                         sysfs interfaces                          *
 *********************************************************************/
static ssize_t show_customize_rate(struct gpufreq_policy *policy, char *buf)
{
    return sprintf(buf, "current frequency: %u\n", userspace_info_s[policy->gpu].userspace_cur_freq);
}

static ssize_t store_customize_rate(struct gpufreq_policy *policy,
            const char *buf, size_t count)
{
    unsigned int ret, freq = 0;
    struct gpufreq_userspace_info_s *this_gov_info = &userspace_info_s[policy->gpu];

    ret = sscanf(buf, "%u", &freq);
    if(ret != 1)
        return -EINVAL;

    if(freq < policy->real_policy.min_freq)
        freq = policy->real_policy.min_freq;
    if(freq > policy->real_policy.max_freq)
        freq = policy->real_policy.max_freq;

    mutex_lock_nested(&userspace_mutex, 0);

    this_gov_info->userspace_target_freq = freq;
    ret = __gpufreq_driver_target(policy, freq, GPUFREQ_RELATION_L);

    /* success, update */
    if(!ret)
    {
        this_gov_info->userspace_cur_freq = policy->cur;
    }

    mutex_unlock(&userspace_mutex);
    return count;
}

gpufreq_freq_attr_rw(customize_rate);

static struct attribute *userspace_attrs[] = {
    &customize_rate.attr,
    NULL
};

static struct attribute_group userspace_attr_group = {
    .attrs = userspace_attrs,
    .name = "userspace",
};

/*********************************************************************
 *                         governor interface                        *
 *********************************************************************/
/* govenor callback function */
static int gpufreq_governor_userspace(struct gpufreq_policy *policy,
                                     unsigned int event)
{
    unsigned int gpu = policy->gpu;
    struct gpufreq_userspace_info_s *this_gov_info;
    int ret = 0, ret0;

    this_gov_info = &userspace_info_s[gpu];

    switch(event)
    {
    case GPUFREQ_GOV_EVENT_START:
        if(!policy->cur)
        {
            debug_log(GPUFREQ_LOG_ERROR, "cur: %d\n", policy->cur);
            return -EINVAL;
        }

        mutex_lock_nested(&userspace_mutex, GPUFREQ_GOV_EVENT_START);
        if(atomic_inc_return(&userspace_enable[gpu]) == 1)
        {
            ret0 = sysfs_create_group(&policy->kobj, &userspace_attr_group);
            if(ret0)
            {
                mutex_unlock(&userspace_mutex);
                return ret0;
            }
        }

        this_gov_info->userspace_cur_freq    = policy->cur;
        this_gov_info->userspace_min_freq    = policy->min;
        this_gov_info->userspace_max_freq    = policy->max;
        this_gov_info->userspace_target_freq = policy->cur;

        debug_log(GPUFREQ_LOG_INFO, "start: gpu %d, max %u, min %u, cur:%u\n",
                                    gpu, policy->max, policy->min,
                                    this_gov_info->userspace_cur_freq);
        mutex_unlock(&userspace_mutex);
        break;

    case GPUFREQ_GOV_EVENT_STOP:
        mutex_lock_nested(&userspace_mutex, GPUFREQ_GOV_EVENT_STOP);

        if(atomic_dec_and_test(&userspace_enable[gpu]))
        {
            sysfs_remove_group(&policy->kobj, &userspace_attr_group);
        }

        this_gov_info->userspace_min_freq    = 0;
        this_gov_info->userspace_max_freq    = 0;
        this_gov_info->userspace_target_freq = 0;

        debug_log(GPUFREQ_LOG_INFO, "stop: gpu %d\n", gpu);
        mutex_unlock(&userspace_mutex);
        break;

    case GPUFREQ_GOV_EVENT_LIMITS:
        mutex_lock_nested(&userspace_mutex, GPUFREQ_GOV_EVENT_LIMITS);

        debug_log(GPUFREQ_LOG_INFO, "limit: gpu %d, max %u, min %u, cur:%u, set to %u\n",
                                    gpu, policy->max, policy->min,
                                    this_gov_info->userspace_cur_freq,
                                    this_gov_info->userspace_target_freq);

        if(policy->max < this_gov_info->userspace_cur_freq)
        {
            __gpufreq_driver_target(policy, policy->max, GPUFREQ_RELATION_H);
        }
        else if(policy->min > this_gov_info->userspace_cur_freq)
        {
            __gpufreq_driver_target(policy, policy->min, GPUFREQ_RELATION_L);
        }
        else
        {
            __gpufreq_driver_target(policy, this_gov_info->userspace_target_freq, GPUFREQ_RELATION_L);
        }

        this_gov_info->userspace_min_freq = policy->min;
        this_gov_info->userspace_max_freq = policy->max;
        this_gov_info->userspace_cur_freq = policy->cur;

        mutex_unlock(&userspace_mutex);
        break;

    case GPUFREQ_GOV_EVENT_RESUME:
        mutex_lock_nested(&userspace_mutex, GPUFREQ_GOV_EVENT_RESUME);

        __gpufreq_driver_target(policy, this_gov_info->userspace_cur_freq, GPUFREQ_RELATION_L);
        this_gov_info->userspace_cur_freq = policy->cur;

        mutex_unlock(&userspace_mutex);
        break;
    }

    return ret;
}

static int gpufreq_gov_userspace_init(void)
{
    return gpufreq_register_governor(&gpufreq_gov_userspace);
}

static void gpufreq_gov_userspace_exit(void)
{
    gpufreq_unregister_governor(&gpufreq_gov_userspace);
}

#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */
