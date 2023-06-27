/*
 * gpufreq-emei.c
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
#if MRVL_PLATFORM_988
#include <linux/clk.h>
#include <plat/clock.h>
#include <linux/err.h>
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
#include <linux/pm_qos.h>
#endif

#define GPUFREQ_FREQ_TABLE_MAX_NUM  10

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
    if(ret != 0)
    {
        /* failed in getting table, make a default one */
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 0, HZ_TO_KHZ(156000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 1, HZ_TO_KHZ(312000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 2, HZ_TO_KHZ(416000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 3, HZ_TO_KHZ(624000000));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, 4, GPUFREQ_TABLE_END);
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

static int emei_gpufreq_init (struct gpufreq_policy *policy);
static int emei_gpufreq_verify (struct gpufreq_policy *policy);
static int emei_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation);
static unsigned int emei_gpufreq_get (unsigned int chip);

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
static unsigned int is_qos_inited = 0;

DECLARE_META_REQUEST(3d, min);
IMPLEMENT_META_NOTIFIER(0, 3d, min, GPUFREQ_RELATION_L);
DECLARE_META_REQUEST(3d, max);
IMPLEMENT_META_NOTIFIER(0, 3d, max, GPUFREQ_RELATION_H);

static struct _gc_qos gc_qos[] = {
    DECLARE_META_GC_QOS_3D,
};

#endif

static struct gpufreq_driver emei_gpufreq_driver = {
    .init   = emei_gpufreq_init,
    .verify = emei_gpufreq_verify,
    .get    = emei_gpufreq_get,
    .target = emei_gpufreq_target,
    .name   = "emei-gpufreq",
    .attr   = driver_attrs,
};

static int emei_gpufreq_init (struct gpufreq_policy *policy)
{
    gpufreq_frequency_table_get(freq_table);
    gpufreq_frequency_table_gpuinfo(policy, freq_table);

    if(unlikely(!gc_clk))
    {
        gc_clk = clk_get(NULL, "GCCLK");
        if(IS_ERR(gc_clk))
            return PTR_ERR(gc_clk);
    }

    policy->cur = emei_gpufreq_get(policy->gpu);

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(unlikely(!is_qos_inited))
    {
        unsigned int gpu = policy->gpu;
        pm_qos_add_request(gc_qos[gpu].pm_qos_req_min,
                           gc_qos[gpu].pm_qos_class_min,
                           policy->gpuinfo.min_freq);

        pm_qos_add_request(gc_qos[gpu].pm_qos_req_max,
                           gc_qos[gpu].pm_qos_class_max,
                           policy->gpuinfo.max_freq);

        is_qos_inited = 1;
    }
#endif

    debug_log(GPUFREQ_LOG_INFO, "GPUFreq for Emei initialized, cur_freq %u\n", policy->cur);

    return 0;
}

static int emei_gpufreq_verify (struct gpufreq_policy *policy)
{
    gpufreq_verify_within_limits(policy, policy->gpuinfo.min_freq,
                     policy->gpuinfo.max_freq);
    return 0;
}

static int emei_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation)
{
    int index;
    int ret = 0, rate = 0;
    struct gpufreq_freqs freqs;

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    unsigned int gpu = policy->gpu;
    {
        unsigned int qos_min = (unsigned int)pm_qos_request(gc_qos[gpu].pm_qos_class_min);
        unsigned int qos_max = (unsigned int)pm_qos_request(gc_qos[gpu].pm_qos_class_max);

        pr_debug("[%d] target %d | policy [%d, %d] | Qos [%d, %d]\n",
                gpu, target_freq, policy->min, policy->max, qos_min, qos_max);

        /*
          - policy max and qos max has higher priority than policy min and qos min
          - policy min and qos min has no priority order, so are policy max and qos max
        */
        target_freq = max(policy->min, target_freq);
        target_freq = max(qos_min, target_freq);
        target_freq = min(policy->max, target_freq);
        target_freq = min(qos_max, target_freq);

        /* seek a target_freq <= min_value_of(policy->max, qos_max) */
        if((target_freq == policy->max) || (target_freq == qos_max))
            relation = GPUFREQ_RELATION_H;
    }
#endif

    /* find a nearest freq in freq_table for target_freq */
    ret = gpufreq_frequency_table_target(policy, freq_table, target_freq, relation, &index);
    if(ret)
    {
        debug_log(GPUFREQ_LOG_ERROR, "error: invalid target frequency: %u\n", target_freq);
        return ret;
    }

    freqs.gpu      = policy->gpu;
    freqs.old_freq = policy->cur;
    freqs.new_freq = freq_table[index].frequency;

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    pr_debug("[%d] Qos_min: %d, Qos_max: %d, Target: %d (KHZ)\n",
            gpu,
            pm_qos_request(gc_qos[gpu].pm_qos_class_min),
            pm_qos_request(gc_qos[gpu].pm_qos_class_max),
            freqs.new_freq);
#endif

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
    rate = emei_gpufreq_get(policy->gpu);
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

static unsigned int emei_gpufreq_get (unsigned int chip)
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
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    {
        unsigned int gpu = 0;
        pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
        pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
    }
#endif
    gpufreq_register_driver(Os, &emei_gpufreq_driver);
    return 0;
}

void __GPUFREQ_EXPORT_TO_GC gpufreq_exit(gckOS Os)
{
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    {
        unsigned int gpu = 0;
        pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
        pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
    }
#endif
    gpufreq_unregister_driver(Os, &emei_gpufreq_driver);
    gpufreq_late_exit();
}

#endif /* End of MRVL_PLATFORM_988 */
#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */
