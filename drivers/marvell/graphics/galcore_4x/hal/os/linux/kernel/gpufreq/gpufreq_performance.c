/*
 * gpufreq_performance.c
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

static int gpufreq_governor_performance(struct gpufreq_policy *policy,
                                        unsigned int event);
static int gpufreq_gov_performance_init(void);
static void gpufreq_gov_performance_exit(void);

struct gpufreq_governor gpufreq_gov_performance = {
    .name       = "performance",
    .init       = gpufreq_gov_performance_init,
    .exit       = gpufreq_gov_performance_exit,
    .governor   = gpufreq_governor_performance,
    .refs       = 0,
//    .owner      = THIS_MODULE,
};

static int gpufreq_governor_performance(struct gpufreq_policy *policy,
                                        unsigned int event)
{
    switch (event) {
    case GPUFREQ_GOV_EVENT_START:
    case GPUFREQ_GOV_EVENT_LIMITS:
        debug_log(GPUFREQ_LOG_DEBUG, "performance: freq %u, event %d", policy->max, event);
        __gpufreq_driver_target(policy, policy->max, GPUFREQ_RELATION_H);
        break;
    default:
        /* do nothing, no need to stop */
        break;
    }
    return 0;
}

static int gpufreq_gov_performance_init(void)
{
    return gpufreq_register_governor(&gpufreq_gov_performance);
}

static void gpufreq_gov_performance_exit(void)
{
    gpufreq_unregister_governor(&gpufreq_gov_performance);
}

#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */
