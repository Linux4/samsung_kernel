/*
 * gpufreq_powersave.c
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

static int gpufreq_governor_powersave(struct gpufreq_policy *policy,
                                      unsigned int event);
static int gpufreq_gov_powersave_init(void);
static void gpufreq_gov_powersave_exit(void);

struct gpufreq_governor gpufreq_gov_powersave = {
    .name       = "powersave",
    .init       = gpufreq_gov_powersave_init,
    .exit       = gpufreq_gov_powersave_exit,
    .governor   = gpufreq_governor_powersave,
    .refs       = 0,
//    .owner      = THIS_MODULE,
};

static int gpufreq_governor_powersave(struct gpufreq_policy *policy,
                                      unsigned int event)
{
    switch (event) {
    case GPUFREQ_GOV_EVENT_START:
    case GPUFREQ_GOV_EVENT_LIMITS:
    case GPUFREQ_GOV_EVENT_RESUME:
        debug_log(GPUFREQ_LOG_DEBUG, "powersave: freq %u, event %d", policy->min, event);
        __gpufreq_driver_target(policy, policy->min, GPUFREQ_RELATION_L);
        break;
    default:
        /* do nothing, no need to stop */
        break;
    }
    return 0;
}

static int gpufreq_gov_powersave_init(void)
{
    return gpufreq_register_governor(&gpufreq_gov_powersave);
}

static void gpufreq_gov_powersave_exit(void)
{
    gpufreq_unregister_governor(&gpufreq_gov_powersave);
}

#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */
