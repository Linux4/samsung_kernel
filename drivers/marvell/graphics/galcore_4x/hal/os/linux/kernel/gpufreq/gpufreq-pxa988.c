/*
 * gpufreq-pxa988.c
 *      gpufreq driver for pxa988 serie platforms: pxa988/986/1088/1l88
 *
 * Author: Watson Wang <zswang@marvell.com>
 * Copyright (C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gpufreq.h"

#if MRVL_CONFIG_ENABLE_GPUFREQ
#if MRVL_PLATFORM_PXA988_FAMILY
#include <linux/clk.h>
#include <linux/err.h>
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
#include <linux/pm_qos.h>
#endif

#if MRVL_CONFIG_DEVFREQ_GOV_THROUGHPUT
#define GPU_HIGH_THRESHOLD_4_DDR 416000
#define _config_state(cur_freq, old_freq, state) \
{ \
    if((cur_freq) >= GPU_HIGH_THRESHOLD_4_DDR) \
    { \
        state = GPUFREQ_POSTCHANGE_UP; \
    } \
    else \
    { \
        state = GPUFREQ_POSTCHANGE_DOWN; \
    } \
}
#else
#define _config_state(cur_freq, old_freq, state) \
{ \
    state = GPUFREQ_POSTCHANGE; \
}
#endif

#define GPUFREQ_FREQ_TABLE_MAX_NUM  10

#define GPUFREQ_SET_FREQ_TABLE(_table, _index, _freq) \
{ \
    _table[_index].index     = _index; \
    _table[_index].frequency = _freq;  \
}

extern int get_gcu_freqs_table(unsigned long *gcu_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

extern int get_gcu2d_freqs_table(unsigned long *gcu2d_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

#if MRVL_CONFIG_SHADER_CLK_CONTROL
extern int get_gc_shader_freqs_table(unsigned long *gcuSh_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);
#endif

typedef int (*PFUNC_GET_FREQS_TBL)(unsigned long *, unsigned int *, unsigned int);

struct gpufreq_pxa988 {
    /* 2d/3d clk*/
    struct clk *gc_clk;

    /* predefined freqs_table */
    struct gpufreq_frequency_table freq_table[GPUFREQ_FREQ_TABLE_MAX_NUM+1];

    /* function pointer to get freqs table */
    PFUNC_GET_FREQS_TBL pf_get_freqs_table;
};

struct gpufreq_pxa988 gh[GPUFREQ_GPU_NUMS];

static int gpufreq_frequency_table_get(unsigned int gpu, struct gpufreq_frequency_table *table_freqs)
{
    int ret, i;
    unsigned long gcu_freqs_table[GPUFREQ_FREQ_TABLE_MAX_NUM];
    unsigned int freq_table_item_count = 0;

    WARN_ON(gpu >= has_avail_gpu_numbers());
    WARN_ON(gh[gpu].pf_get_freqs_table == NULL);

    ret = (* gh[gpu].pf_get_freqs_table)(&gcu_freqs_table[0],
                                           &freq_table_item_count,
                                           GPUFREQ_FREQ_TABLE_MAX_NUM);
    if(unlikely(ret != 0))
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
        debug_log(GPUFREQ_LOG_DEBUG, "[%d] entry %d is %d KHZ\n", gpu, i, HZ_TO_KHZ(gcu_freqs_table[i]));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, i, HZ_TO_KHZ(gcu_freqs_table[i]));
    }
    GPUFREQ_SET_FREQ_TABLE(table_freqs, i, GPUFREQ_TABLE_END);

out:
    debug_log(GPUFREQ_LOG_DEBUG, "[%d] tbl[0] = %d, %d\n", gpu, gcu_freqs_table[0], freq_table_item_count);
    return 0;
}

/* [RO] attr: scaling_available_frequencies */
static ssize_t show_scaling_available_frequencies(struct gpufreq_policy *policy, char *buf)
{
    unsigned int i;
    unsigned int gpu = policy->gpu;
    struct gpufreq_frequency_table *freq_table = gh[gpu].freq_table;
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

gpufreq_freq_attr_ro(scaling_available_frequencies);

static struct gpufreq_freq_attr *driver_attrs[] = {
    &scaling_available_frequencies,
    NULL
};

static int pxa988_gpufreq_init (struct gpufreq_policy *policy);
static int pxa988_gpufreq_verify (struct gpufreq_policy *policy);
static int pxa988_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation);
static unsigned int pxa988_gpufreq_get (unsigned int chip);

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
static unsigned int is_qos_inited = 0;

DECLARE_META_REQUEST(3d, min);
IMPLEMENT_META_NOTIFIER(0, 3d, min, GPUFREQ_RELATION_L);
DECLARE_META_REQUEST(3d, max);
IMPLEMENT_META_NOTIFIER(0, 3d, max, GPUFREQ_RELATION_H);

DECLARE_META_REQUEST(2d, min);
IMPLEMENT_META_NOTIFIER(1, 2d, min, GPUFREQ_RELATION_L);
DECLARE_META_REQUEST(2d, max);
IMPLEMENT_META_NOTIFIER(1, 2d, max, GPUFREQ_RELATION_H);

#if MRVL_CONFIG_SHADER_CLK_CONTROL
DECLARE_META_REQUEST(sh, min);
IMPLEMENT_META_NOTIFIER(2, sh, min, GPUFREQ_RELATION_L);
DECLARE_META_REQUEST(sh, max);
IMPLEMENT_META_NOTIFIER(2, sh, max, GPUFREQ_RELATION_H);
#endif

static struct _gc_qos gc_qos_pxa988[] = {
    DECLARE_META_GC_QOS_3D,
};

static struct _gc_qos gc_qos_pxa1088[] = {
    DECLARE_META_GC_QOS_3D,
    DECLARE_META_GC_QOS_2D,
};

static struct _gc_qos gc_qos_pxa1l88[] = {
    DECLARE_META_GC_QOS_3D,
    DECLARE_META_GC_QOS_2D,
#if MRVL_CONFIG_SHADER_CLK_CONTROL
    DECLARE_META_GC_QOS_SH,
#endif
};

static struct _gc_qos* gc_qos = NULL;

#endif

static struct gpufreq_driver pxa988_gpufreq_driver = {
    .init   = pxa988_gpufreq_init,
    .verify = pxa988_gpufreq_verify,
    .get    = pxa988_gpufreq_get,
    .target = pxa988_gpufreq_target,
    .name   = "pxa988-gpufreq",
    .attr   = driver_attrs,
};

static int pxa988_gpufreq_init (struct gpufreq_policy *policy)
{
    unsigned gpu = policy->gpu;
    struct clk *gc_clk = gh[gpu].gc_clk;

    /* get func pointer from kernel functions. */
    switch (gpu) {
    case 0:
        gh[gpu].pf_get_freqs_table = get_gcu_freqs_table;
        break;
    case 1:
        if(has_feat_separated_gc_clock())
        {
            gh[gpu].pf_get_freqs_table = get_gcu2d_freqs_table;
            break;
        }
#if MRVL_CONFIG_SHADER_CLK_CONTROL
    case 2:
        if(has_feat_gc_shader())
        {
            gh[gpu].pf_get_freqs_table = get_gc_shader_freqs_table;
            break;
        }
        /* else bail through */
#endif
    default:
        debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %d\n", gpu);
    }

    debug_log(GPUFREQ_LOG_DEBUG, "gpu %d, pfunc %p\n", gpu, gh[gpu].pf_get_freqs_table);

    /* build freqs table */
    gpufreq_frequency_table_get(gpu, gh[gpu].freq_table);
    gpufreq_frequency_table_gpuinfo(policy, gh[gpu].freq_table);

    if(unlikely(!gc_clk))
    {
        switch (gpu) {
        case 0:
            gc_clk = clk_get(NULL, "GCCLK");
            break;
        case 1:
            if(has_feat_separated_gc_clock())
            {
                gc_clk = clk_get(NULL, "GC2DCLK");
                break;
            }
#if MRVL_CONFIG_SHADER_CLK_CONTROL
        case 2:
            if(has_feat_gc_shader())
            {
                gc_clk = clk_get(NULL, "GC_SHADER_CLK");
                break;
            }
            /* else bail through */
#endif
        default:
            debug_log(GPUFREQ_LOG_ERROR, "cannot get clk for gpu %d\n", gpu);
        }

        if(IS_ERR(gc_clk))
        {
            debug_log(GPUFREQ_LOG_ERROR, "get clk of gpu %d, ret %d\n", gpu, PTR_ERR(gc_clk));
            return PTR_ERR(gc_clk);
        }

        /* store in global variable. */
        gh[gpu].gc_clk = gc_clk;
    }

    policy->cur = pxa988_gpufreq_get(policy->gpu);

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(has_feat_qos_support())
    {
        if(unlikely(!(is_qos_inited & (1 << gpu))))
        {
            pm_qos_add_request(gc_qos[gpu].pm_qos_req_min,
                               gc_qos[gpu].pm_qos_class_min,
                               policy->gpuinfo.min_freq);

            pm_qos_add_request(gc_qos[gpu].pm_qos_req_max,
                               gc_qos[gpu].pm_qos_class_max,
                               policy->gpuinfo.max_freq);

            is_qos_inited |= (1 << gpu);
        }
    }
#endif

    debug_log(GPUFREQ_LOG_INFO, "GPUFreq for HelenLTE/Helan/emei gpu %d initialized, cur_freq %u\n", gpu, policy->cur);

    return 0;
}

static int pxa988_gpufreq_verify (struct gpufreq_policy *policy)
{
    gpufreq_verify_within_limits(policy, policy->gpuinfo.min_freq,
                     policy->gpuinfo.max_freq);
    return 0;
}

static int pxa988_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation)
{
    int index;
    int ret = 0, rate = 0;
    struct gpufreq_freqs freqs;
    unsigned int gpu = policy->gpu, state = GPUFREQ_POSTCHANGE;
    struct clk *gc_clk = gh[gpu].gc_clk;
    struct gpufreq_frequency_table *freq_table = gh[gpu].freq_table;

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(has_feat_qos_support())
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
        debug_log(GPUFREQ_LOG_ERROR, "[%d] error: invalid target frequency: %u\n", gpu, target_freq);
        return ret;
    }

    freqs.gpu      = policy->gpu;
    freqs.old_freq = policy->cur;
    freqs.new_freq = freq_table[index].frequency;

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(has_feat_qos_support())
    {
        pr_debug("[%d] Qos_min: %d, Qos_max: %d, Target: %d (KHZ)\n",
                gpu,
                pm_qos_request(gc_qos[gpu].pm_qos_class_min),
                pm_qos_request(gc_qos[gpu].pm_qos_class_max),
                freqs.new_freq);
    }
#endif

    if(freqs.old_freq == freqs.new_freq)
        return ret;

    gpufreq_notify_transition(&freqs, GPUFREQ_PRECHANGE);

    ret = clk_set_rate(gc_clk, KHZ_TO_HZ(freqs.new_freq));
    if(ret)
    {
        debug_log(GPUFREQ_LOG_WARNING, "[%d] failed to set target rate %u to clk %p\n",
                        gpu, freqs.new_freq, gc_clk);
    }

    /* update current frequency after adjustment */
    rate = pxa988_gpufreq_get(policy->gpu);
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

    _config_state(rate, freqs.old_freq, state);

    debug_log(GPUFREQ_LOG_INFO, "current state(0x%04x) after frequency post change\n", state);

    gpufreq_notify_transition(&freqs, state);

    return ret;
}

static unsigned int pxa988_gpufreq_get (unsigned int gpu)
{
    unsigned int rate = ~0;
    struct clk *gc_clk = gh[gpu].gc_clk;

    if(!gc_clk)
    {
        debug_log(GPUFREQ_LOG_ERROR, "gc clk of gpu %d is invalid\n", gpu);
        return -EINVAL;
    }

    rate = clk_get_rate(gc_clk);

    if(rate == (unsigned int)-1)
        return -EINVAL;

    return HZ_TO_KHZ(rate);
}

/***************************************************
**  interfaces exported to GC driver
****************************************************/
static int __gpufreq_platform_init(void)
{
    if(cpu_is_pxa988() || cpu_is_pxa986())
    {
        gc_qos = &gc_qos_pxa988[0];
    }
    else if(cpu_is_pxa1088())
    {
        gc_qos = &gc_qos_pxa1088[0];
    }
    else if(cpu_is_pxa1L88())
    {
        gc_qos = &gc_qos_pxa1l88[0];
    }

    WARN_ON(gc_qos == NULL);

    return 0;
}

int __GPUFREQ_EXPORT_TO_GC gpufreq_init(gckOS Os)
{
    gpufreq_early_init();
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(has_feat_qos_support())
    {
        __gpufreq_platform_init();
    }
#endif
    /* make sure gpufreq_driver has been initialized
    * before add qos notfier callback.
    */
    gpufreq_register_driver(Os, &pxa988_gpufreq_driver);
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(has_feat_qos_support())
    {
        unsigned int gpu = 0;

        for_each_gpu(gpu)
        {
            pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
            pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
        }
    }
#endif


    return 0;
}

void __GPUFREQ_EXPORT_TO_GC gpufreq_exit(gckOS Os)
{
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(has_feat_qos_support())
    {
        unsigned int gpu = 0;
        for_each_gpu(gpu)
        {
            pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
            pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
        }
    }
#endif
    gpufreq_unregister_driver(Os, &pxa988_gpufreq_driver);
    gpufreq_late_exit();
}

#endif /* End of MRVL_PLATFORM_PXA988_FAMILY */
#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */
