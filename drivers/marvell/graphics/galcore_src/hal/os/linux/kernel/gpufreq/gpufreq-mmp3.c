/*
 * gpufreq-mmp3.c
 *
 * Author: Watson Wang <zswang@marvell.com>
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gpufreq.h"


#if MRVL_CONFIG_ENABLE_GPUFREQ
#if MRVL_PLATFORM_MMP3
#include <linux/clk.h>
#include <plat/clock.h>
#include <linux/err.h>
#include <mach/mmp3_pm.h>

static struct clk *gc_clk;

static struct gpufreq_frequency_table freq_table[] = {
    [0] = {
        .index = 0,
        .frequency = 100000000,
    },
    [1] = {
        .index = 1,
        .frequency = 200000000,
    },
    [2] = {
        .index = 2,
        .frequency = 355000000,
    },
    [3] = {
        .index = 3,
        .frequency = 400000000,
    },
    [4] = {
        .index = 4,
        .frequency = 533000000,
    },
    [5] = {
        .index = 5,
        .frequency = GPUFREQ_TABLE_END,
    },
};

static int mmp3_gpufreq_init (struct gpufreq_policy *policy);
static int mmp3_gpufreq_verify (struct gpufreq_policy *policy);
static int mmp3_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation);
static unsigned int mmp3_gpufreq_get (unsigned int chip);

static struct gpufreq_driver mmp3_gpufreq_driver = {
    .init   = mmp3_gpufreq_init,
    .verify = mmp3_gpufreq_verify,
    .get    = mmp3_gpufreq_get,
    .target = mmp3_gpufreq_target,
    .name   = "mmp3-gpufreq",
//    .attr   =  available_freq_table,
};

static int mmp3_gpufreq_init (struct gpufreq_policy *policy)
{
    int ret = 0;
    ret = gpufreq_frequency_table_gpuinfo(policy, freq_table);

    if(!gc_clk)
    {
        gc_clk = clk_get(NULL, "GCCLK");
        if(IS_ERR(gc_clk))
            return PTR_ERR(gc_clk);
    }

    policy->cur = mmp3_gpufreq_get(policy->gpu);

    debug_log(GPUFREQ_LOG_INFO, "GPUFreq for MMP3 initialized, cur_freq %u\n", policy->cur);

    return 0;
}

static int mmp3_gpufreq_verify (struct gpufreq_policy *policy)
{
    gpufreq_verify_within_limits(policy, policy->gpuinfo.min_freq,
                    policy->gpuinfo.max_freq);
    return 0;
}

static int mmp3_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation)
{
    int index;
    int ret = 0;

    /* find a nearest freq in freq_table for target_freq */
    if(gpufreq_frequency_table_target(policy, freq_table, target_freq, relation, &index))
    {
        debug_log(GPUFREQ_LOG_ERROR, "error: invalid target frequency: %u\n", target_freq);
        ret = -EINVAL;
        goto out;
    }

    /* current freq is the target freq */
    if(policy->cur == freq_table[index].frequency)
        goto out;

    /* internal or external? */
    // TODO: add internal clk scaling
    clk_set_rate(gc_clk, freq_table[index].frequency);

    /* update current frequency after adjustment */
    policy->cur = freq_table[index].frequency;
out:
    return ret;
}

static unsigned int mmp3_gpufreq_get (unsigned int chip)
{
    unsigned int rate = ~0;

    if(gc_clk)
    {
        rate = clk_get_rate(gc_clk);
    }

    return rate;
}

/***************************************************
**  interfaces exported to GC driver
****************************************************/
int __GPUFREQ_EXPORT_TO_GC gpufreq_init(gckOS Os)
{
    gpufreq_early_init();
    gpufreq_register_driver(Os, &mmp3_gpufreq_driver);
    return 0;
}

void __GPUFREQ_EXPORT_TO_GC gpufreq_exit(gckOS Os)
{
    gpufreq_unregister_driver(Os, &mmp3_gpufreq_driver);
    gpufreq_late_exit();
}

#endif /* End of MRVL_PLATFORM_MMP3 */
#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */
