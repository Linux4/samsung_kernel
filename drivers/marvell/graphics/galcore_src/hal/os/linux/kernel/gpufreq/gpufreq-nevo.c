/*
 * gpufreq-nevo.c
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
#if MRVL_PLATFORM_NEVO
#include <linux/clk.h>
#include <plat/clock.h>
#include <linux/err.h>

#define GPUFREQ_FREQ_TABLE_MAX_NUM  20

#define GPUFREQ_SET_FREQ_TABLE(_table, _index, _freq) \
{ \
    _table[_index].index     = _index; \
    _table[_index].frequency = _freq;  \
}

static struct clk *gc_clk;
static struct gpufreq_frequency_table freq_table[GPUFREQ_FREQ_TABLE_MAX_NUM + 1];

extern int get_gcu_freqs_table(int *gcu_freqs_table, int *item_counts,
                     int max_item_counts);

static int gpufreq_frequency_table_get(struct gpufreq_frequency_table *table_freqs)
{
    int ret, i;
    int gcu_freqs_table[GPUFREQ_FREQ_TABLE_MAX_NUM];
    int freq_table_item_count = 0;

    ret = get_gcu_freqs_table(&gcu_freqs_table[0],
                              &freq_table_item_count,
                              GPUFREQ_FREQ_TABLE_MAX_NUM);
    if(ret == -1)
    {
        /* failed in getting table, make a default one */
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 0, HZ_TO_KHZ(156000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 1, HZ_TO_KHZ(208000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 2, HZ_TO_KHZ(312000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 3, HZ_TO_KHZ(416000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 4, HZ_TO_KHZ(498000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 5, HZ_TO_KHZ(600000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 6, GPUFREQ_TABLE_END);
        goto out;
    }

    /* initialize frequency table */
    for(i = 0; i < freq_table_item_count; i++)
    {
        debug_log(GPUFREQ_LOG_DEBUG, "entry %d is %d KHZ\n", i, HZ_TO_KHZ(gcu_freqs_table[i]));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, i, HZ_TO_KHZ(gcu_freqs_table[i]));
    }
    GPUFREQ_SET_FREQ_TABLE(table_freqs, i, GPUFREQ_TABLE_END);

out:
    debug_log(GPUFREQ_LOG_DEBUG, "%p, %d\n", &gcu_freqs_table[0], freq_table_item_count);
    return 0;
}

/* [RO] attr: scaling_available_freqs */
static ssize_t show_scaling_available_freqs(struct gpufreq_policy *policy, char *buf)
{
    unsigned int i;
    ssize_t count = 0;

    for (i = 0; (freq_table[i].frequency != GPUFREQ_TABLE_END); ++i)
    {
        if (freq_table[i].frequency == GPUFREQ_ENTRY_INVALID)
            continue;

        count += sprintf(&buf[count], "%d ", freq_table[i].frequency);
    }

    count += sprintf(&buf[count], "\n");

    return count;
}

gpufreq_freq_attr_ro(scaling_available_freqs);

static struct gpufreq_freq_attr *driver_attrs[] = {
    &scaling_available_freqs,
    NULL
};

static int nevo_gpufreq_init (struct gpufreq_policy *policy);
static int nevo_gpufreq_verify (struct gpufreq_policy *policy);
static int nevo_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation);
static unsigned int nevo_gpufreq_get (unsigned int chip);

static struct gpufreq_driver nevo_gpufreq_driver = {
    .init   = nevo_gpufreq_init,
    .verify = nevo_gpufreq_verify,
    .get    = nevo_gpufreq_get,
    .target = nevo_gpufreq_target,
    .name   = "nevo-gpufreq",
    .attr   = driver_attrs,
};

static int nevo_gpufreq_init (struct gpufreq_policy *policy)
{
    gpufreq_frequency_table_get(freq_table);
    gpufreq_frequency_table_gpuinfo(policy, freq_table);

    if(!gc_clk)
    {
        gc_clk = clk_get(NULL, "GCCLK");
        if(IS_ERR(gc_clk))
            return PTR_ERR(gc_clk);
    }

    policy->cur = nevo_gpufreq_get(policy->gpu);

    debug_log(GPUFREQ_LOG_INFO, "GPUFreq for Nevo initialized, cur_freq %u\n", policy->cur);

    return 0;
}

static int nevo_gpufreq_verify (struct gpufreq_policy *policy)
{
    gpufreq_verify_within_limits(policy, policy->gpuinfo.min_freq,
                     policy->gpuinfo.max_freq);
    return 0;
}

static int nevo_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation)
{
    int index;
    int ret = 0, rate = 0;
    struct gpufreq_freqs freqs;

    /* find a nearest freq in freq_table for target_freq */
    ret = gpufreq_frequency_table_target(policy, freq_table, target_freq, relation, &index);
    if(ret){
        debug_log(GPUFREQ_LOG_ERROR, "error: invalid target frequency: %u\n", target_freq);
        return ret;
    }

    freqs.gpu      = policy->gpu;
    freqs.old_freq = policy->cur;
    freqs.new_freq = freq_table[index].frequency;

    if(freqs.old_freq == freqs.new_freq)
        return ret;

    gpufreq_notify_transition(&freqs, GPUFREQ_PRECHANGE);

    ret = clk_set_rate(gc_clk, KHZ_TO_HZ(freqs.new_freq));
    if(ret)
    {
        debug_log(GPUFREQ_LOG_WARNING, "failed to set target rate %u\n",
                        freqs.new_freq);
    }

    /* update current frequency after adjustment */
    rate = nevo_gpufreq_get(policy->gpu);
    if(rate == -EINVAL)
    {
        debug_log(GPUFREQ_LOG_WARNING, "failed get rate for gpu %d\n", policy->gpu);
        freqs.new_freq = freqs.old_freq;
        ret = -EINVAL;
    }
    else
    {
        freqs.new_freq = rate;
    }

    gpufreq_notify_transition(&freqs, GPUFREQ_POSTCHANGE);

    return ret;
}

static unsigned int nevo_gpufreq_get (unsigned int chip)
{
    unsigned int rate = ~0;

    if(!gc_clk)
        return -EINVAL;

    rate = clk_get_rate(gc_clk);

    if(rate == (unsigned int)-1)
        return -EINVAL;

    return HZ_TO_KHZ(rate);
}

/***************************************************
**  interfaces exported to GC driver
****************************************************/
int __GPUFREQ_EXPORT_TO_GC gpufreq_init(gckOS Os)
{
    gpufreq_early_init();
    gpufreq_register_driver(Os, &nevo_gpufreq_driver);
    return 0;
}

void __GPUFREQ_EXPORT_TO_GC gpufreq_exit(gckOS Os)
{
    gpufreq_unregister_driver(Os, &nevo_gpufreq_driver);
    gpufreq_late_exit();
}

#endif /* End of MRVL_PLATFORM_NEVO */
#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */
