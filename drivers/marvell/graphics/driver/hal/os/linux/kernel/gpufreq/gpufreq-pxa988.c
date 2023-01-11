/*
 * gpufreq-pxa988.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gpufreq.h"
#include <linux/kallsyms.h>

#if MRVL_CONFIG_ENABLE_GPUFREQ
#include <linux/clk.h>
#include <linux/err.h>
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
#include <linux/pm_qos.h>
#endif

#define GPUFREQ_FREQ_TABLE_MAX_NUM  10

#define AXI_MIN(g) gcmMAX((g[gcvCORE_2D].freq), (g[gcvCORE_MAJOR].freq))

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#define GPUFREQ_SET_FREQ_TABLE(_table, _index, _freq, _afreq) \
{ \
    _table[_index].index     = _index; \
    _table[_index].frequency = _freq;  \
    _table[_index].busfreq   = _afreq; \
}
#else
#define GPUFREQ_SET_FREQ_TABLE(_table, _index, _freq) \
{ \
    _table[_index].index     = _index; \
    _table[_index].frequency = _freq;  \
}
#endif

typedef int (*PFUNC_GET_FREQS_TBL)(unsigned long *, unsigned int *, unsigned int);

struct gpufreq_axi
{
    /* bool for axi clk downgrade*/
    gctBOOL                     dec;

    /* decreased axi clk records*/
    gctUINT32                   freq;
};

struct gpufreq_pxa988 {
    /* predefined freqs_table */
    struct gpufreq_frequency_table freq_table[GPUFREQ_FREQ_TABLE_MAX_NUM+1];

    /* function pointer to get freqs table */
    PFUNC_GET_FREQS_TBL pf_get_freqs_table;
};

struct gpufreq_pxa988 gh[GPUFREQ_GPU_NUMS];
static gckOS gpu_os;

#if GPUFREQ_REQUEST_DDR_QOS
static DDR_QOS_NODE gpufreq_ddr_constraint[GPUFREQ_GPU_NUMS] = {
        {
            .qos_node = {
                .name = "gpu3d_ddr_min",
            }
        },
        {
            .qos_node = {
                .name = "gpu2d_ddr_min",
            }
        },
        {
            .qos_node = {
                .name = "gpush_ddr_min",
            }
        },

    };

static int gpu_high_threshold = 312000;
#endif

static struct gpufreq_axi ga[GPUFREQ_GPU_NUMS-1];
static bool _AllowSetAxiFreq(unsigned int old_freq, int gpu, unsigned int new_freq)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)

    unsigned int cnt = 0, i =0;

    /* return false directly if shader core*/
    if( gcvCORE_SH == gpu)
        return gcvFALSE;

    /* axi increase request*/
    if(new_freq > old_freq)
    {
        ga[gpu].dec  = false;
        ga[gpu].freq = new_freq;

        return true;
    }

    /* axi decrease request*/
    ga[gpu].dec  = true;
    ga[gpu].freq = new_freq;

    while(i < GPUFREQ_GPU_NUMS - 1)
    {
        if( true == ga[i++].dec)
            cnt++;
        else
            break;
    }

    /* decrease axi freq only if 3D/2D cores have same request*/
    if(GPUFREQ_GPU_NUMS-1 == cnt)
        return true;
    else
        return false;
#else
    return false;
#endif
}

static int gpufreq_frequency_table_get(unsigned int gpu, struct gpufreq_frequency_table *table_freqs, void * srcFreqTable)
{
    int rnt = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    struct gpufreq_frequency_table * gft = (struct gpufreq_frequency_table *)srcFreqTable;
    int i = 0;

    if(!gft || (gft+i)->index == GPUFREQ_ENTRY_INVALID)
    {
        /* failed to get freq table, return error*/
        debug_log(GPUFREQ_LOG_ERROR, "[%d] freqtable is not found, gft(%p), index: %d\n", gpu, gft, (gft+i)->index);
        rnt = -1;
        goto out;
    }

    while((gft+i)->index != GPUFREQ_TABLE_END)
    {
        debug_log(GPUFREQ_LOG_INFO, "%d[%d], gft: %p, fclk: %d, bclk: %d\n", gpu, (gft+i)->index, gft, (gft+i)->frequency, (gft+i)->busfreq);
        GPUFREQ_SET_FREQ_TABLE(table_freqs, i, (gft+i)->frequency, (gft+i)->busfreq);
        i++;
    }
    GPUFREQ_SET_FREQ_TABLE(table_freqs, i, GPUFREQ_TABLE_END, GPUFREQ_TABLE_END);
#else
    int ret, i;
    unsigned long gcu_freqs_table[GPUFREQ_FREQ_TABLE_MAX_NUM];
    unsigned int freq_table_item_count = 0;

    WARN_ON(gpu >= GPUFREQ_GPU_NUMS);
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
        debug_log(GPUFREQ_LOG_DEBUG, "[%d] entry %d is %lu KHZ\n", gpu, i, HZ_TO_KHZ(gcu_freqs_table[i]));
        GPUFREQ_SET_FREQ_TABLE(table_freqs, i, HZ_TO_KHZ(gcu_freqs_table[i]));
    }
    GPUFREQ_SET_FREQ_TABLE(table_freqs, i, GPUFREQ_TABLE_END);
#endif

out:
    return rnt;
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

static int pxa988_gpufreq_init (gctPOINTER freqTable ,struct gpufreq_policy *policy);
static int pxa988_gpufreq_exit (struct gpufreq_policy *policy);
static int pxa988_gpufreq_verify (struct gpufreq_policy *policy);
static int pxa988_gpufreq_target (struct gpufreq_policy *policy, unsigned int target_freq, unsigned int relation);
static int pxa988_gpufreq_set(struct gpufreq_freqs* freqs);
static int pxa988_gpufreq_suspend(struct gpufreq_policy *policy);
static int pxa988_gpufreq_resume(struct gpufreq_policy *policy);
static int pxa988_gpufreq_qos_update(unsigned int chip, unsigned int min, unsigned int freq);


static unsigned int pxa988_gpufreq_get (unsigned int chip);
static unsigned int pxa988_gpufreq_get_axi(unsigned int chip);

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

DECLARE_META_REQUEST(sh, min);
IMPLEMENT_META_NOTIFIER(2, sh, min, GPUFREQ_RELATION_L);
DECLARE_META_REQUEST(sh, max);
IMPLEMENT_META_NOTIFIER(2, sh, max, GPUFREQ_RELATION_H);

static struct _gc_qos gc_qos[] = {
    DECLARE_META_GC_QOS_3D,
    DECLARE_META_GC_QOS_2D,
    DECLARE_META_GC_QOS_SH,
};

void pxa988_gpufreq_register_qos(unsigned int gpu)
{
    pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
    pm_qos_add_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
}

void pxa988_gpufreq_unregister_qos(unsigned int gpu)
{
    pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_min, gc_qos[gpu].notifier_min);
    pm_qos_remove_notifier(gc_qos[gpu].pm_qos_class_max, gc_qos[gpu].notifier_max);
}
#endif /* MRVL_CONFIG_ENABLE_QOS_SUPPORT */

struct gpufreq_driver pxa988_gpufreq_driver = {
    .init   = pxa988_gpufreq_init,
    .verify = pxa988_gpufreq_verify,
    .get    = pxa988_gpufreq_get,
    .target = pxa988_gpufreq_target,
    .suspend= pxa988_gpufreq_suspend,
    .resume = pxa988_gpufreq_resume,
    .qos_upd= pxa988_gpufreq_qos_update,
    .name   = "pxa988-gpufreq",
    .exit   = pxa988_gpufreq_exit,
    .attr   = driver_attrs,
};

static void pxa988_init_freqs_table_func(unsigned int gpu)
{
#ifdef CONFIG_KALLSYMS
    switch (gpu) {
    case 0:
        gh[gpu].pf_get_freqs_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gcu_freqs_table");
        break;
    case 1:
        gh[gpu].pf_get_freqs_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gcu2d_freqs_table");
        break;
#if MRVL_CONFIG_SHADER_CLK_CONTROL
    case 2:
        if(has_feat_shader_indept_dfc())
        {
            gh[gpu].pf_get_freqs_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gc_shader_freqs_table");
            break;
        }
        /* else bail through */
#endif

    default:
        debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %u\n", gpu);
    }

#elif (defined CONFIG_CPU_PXA988)
extern int get_gcu_freqs_table(unsigned long *gcu_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

extern int get_gcu2d_freqs_table(unsigned long *gcu2d_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

extern int get_gc_shader_freqs_table(unsigned long *gcush_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

    switch (gpu) {
    case 0:
        gh[gpu].pf_get_freqs_table = get_gcu_freqs_table;
        break;
    case 1:
        gh[gpu].pf_get_freqs_table = get_gcu2d_freqs_table;
        break;
#if MRVL_CONFIG_SHADER_CLK_CONTROL
    case 2:
        if(has_feat_shader_indept_dfc())
        {
            gh[gpu].pf_get_freqs_table = get_gc_shader_freqs_table;
            break;
        }
        /* else bail through */
#endif

    default:
        debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %u\n", gpu);
    }
#else
    debug_log(GPUFREQ_LOG_ERROR, "cannot get func pointer for gpu %u\n", gpu);
#endif
}

static int pxa988_gpufreq_init (gctPOINTER freqTable, struct gpufreq_policy *policy)
{
    unsigned int gpu = policy->gpu;

    /* get func pointer from kernel functions. */
    pxa988_init_freqs_table_func(gpu);

    /* build freqs table */
    if(gpufreq_frequency_table_get(gpu, gh[gpu].freq_table, freqTable))
    {
        debug_log(GPUFREQ_LOG_ERROR, "failed to get core(%d) freq table, initialize failed", gpu);
        return -1;
    }

    gpufreq_frequency_table_gpuinfo(policy, gh[gpu].freq_table);

    policy->cur = pxa988_gpufreq_get(policy->gpu);

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    if(unlikely(!(is_qos_inited & (1 << gpu))))
    {
        pxa988_gpufreq_register_qos(gpu);
        pm_qos_add_request(gc_qos[gpu].pm_qos_req_min,
                           gc_qos[gpu].pm_qos_class_min,
                           policy->gpuinfo.min_freq);
        pm_qos_add_request(gc_qos[gpu].pm_qos_req_max,
                           gc_qos[gpu].pm_qos_class_max,
                           policy->gpuinfo.max_freq);
        is_qos_inited |= (1 << gpu);
    }
#endif

    if(has_feat_axi_freq_change() &&
        gpu != gcvCORE_SH)
    {
        policy->axi_cur = pxa988_gpufreq_get_axi(policy->gpu);
        ga[gpu].freq    = policy->axi_cur;
        ga[gpu].dec     = gcvTRUE;
    }

#if GPUFREQ_REQUEST_DDR_QOS
    gpufreq_ddr_constraint_init(&gpufreq_ddr_constraint[gpu]);
#endif

    debug_log(GPUFREQ_LOG_INFO, "GPUFreq for gpu %u initialized, cur_freq %u\n", gpu, policy->cur);

    return 0;
}

static int pxa988_gpufreq_exit (struct gpufreq_policy *policy)
{
    unsigned gpu = policy->gpu;

#if GPUFREQ_REQUEST_DDR_QOS
    gpufreq_ddr_constraint_deinit(&gpufreq_ddr_constraint[gpu]);
#endif

    pxa988_gpufreq_unregister_qos(gpu);

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
    int ret = 0;
    struct gpufreq_freqs freqs, freqs_sh;
    unsigned int gpu = policy->gpu;
    struct gpufreq_frequency_table *freq_table = gh[gpu].freq_table;

#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
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

    if(has_feat_axi_freq_change() &&
      (gpu != gcvCORE_SH))
    {
        freqs.axi_old_freq = policy->axi_cur;
        freqs.axi_new_freq = freq_table[index].busfreq;
    }

    if(!has_feat_shader_indept_dfc() &&
        gcvCORE_MAJOR == gpu)
    {
        gckOS_QueryClkRate(gpu_os, gcvCORE_SH, gcvFALSE, &freqs_sh.old_freq);
        freqs_sh.gpu = gcvCORE_SH;
        freqs_sh.new_freq = freqs.new_freq;
    }

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

    ret = pxa988_gpufreq_set(&freqs);

    //set shader core frequency
    if(!has_feat_shader_indept_dfc() &&
        gcvCORE_MAJOR == gpu)
        pxa988_gpufreq_set(&freqs_sh);

    if(has_feat_axi_freq_change() &&
        (gpu != gcvCORE_SH))
        policy->axi_cur = freqs.axi_new_freq;

#if GPUFREQ_REQUEST_DDR_QOS
    gpufreq_ddr_constraint_update(&gpufreq_ddr_constraint[gpu],
                                   freqs.new_freq,
                                   freqs.old_freq,
                                   gpu_high_threshold);
#endif

    gpufreq_notify_transition(&freqs, GPUFREQ_POSTCHANGE);

    return ret;
}

static int pxa988_gpufreq_set(struct gpufreq_freqs* freqs)
{
    int ret = 0;
    unsigned int rate = 0;
    gceSTATUS status;
    unsigned int gpu = freqs->gpu;

    if (has_feat_dfc_protect_clk_op())
        gpufreq_acquire_clock_mutex(gpu);

    /* update new frequency of FuncClk to hw */
    status = gckOS_SetClkRate(gpu_os, gpu, gcvFALSE, freqs->new_freq);
    if(gcmIS_ERROR(status))
    {
        debug_log(GPUFREQ_LOG_WARNING, "[%d] failed to set target rate %u KHZ\n",
                        gpu, freqs->new_freq);
    }

    if(has_feat_axi_freq_change() &&
        gpu != gcvCORE_SH &&
       _AllowSetAxiFreq(freqs->axi_old_freq, gpu, freqs->axi_new_freq))
    {
        /* update new frequency of AxiClk to hw */
        status = gckOS_SetClkRate(gpu_os, gpu, gcvTRUE, AXI_MIN(ga));
        freqs->axi_new_freq = AXI_MIN(ga);
    }

    if(gcmIS_ERROR(status))
    {
        debug_log(GPUFREQ_LOG_WARNING, "[%d] failed to set target rate %u KHZ\n",
                        gpu, freqs->new_freq);
    }

    if (has_feat_dfc_protect_clk_op())
        gpufreq_release_clock_mutex(gpu);

    /* update current frequency after adjustment */
    rate = pxa988_gpufreq_get(gpu);
    if(rate == -EINVAL)
    {
        debug_log(GPUFREQ_LOG_WARNING, "failed get rate for gpu %u\n", gpu);
        freqs->new_freq = freqs->old_freq;
        ret = -EINVAL;
    }
    else
    {
        freqs->new_freq = rate;
    }

    return ret;
}

static unsigned int pxa988_gpufreq_get (unsigned int gpu)
{
    gceSTATUS status;
    unsigned int rate = ~0;

    gcmkONERROR(gckOS_QueryClkRate(gpu_os, gpu, gcvFALSE, &rate));
    return rate;

OnError:
    debug_log(GPUFREQ_LOG_ERROR, "failed to get clk rate of gpu %u\n", gpu);
    return -EINVAL;
}

static int pxa988_gpufreq_suspend(struct gpufreq_policy *policy)
{
#if GPUFREQ_REQUEST_DDR_QOS
    unsigned int gpu = policy->gpu;

    /* release ddr qos if freq > gpu_high_threshold when power off,
    * because gpu doesn't need ddr anymore
    */
    if(policy->cur >= gpu_high_threshold)
        gpufreq_ddr_constraint_update(&gpufreq_ddr_constraint[gpu],
                                       0,
                                       policy->cur,
                                       gpu_high_threshold);
#endif
    return 0;
}

static int pxa988_gpufreq_resume(struct gpufreq_policy *policy)
{
#if GPUFREQ_REQUEST_DDR_QOS
    unsigned int gpu = policy->gpu;

    /* re-hold ddr qos if freq > gpu_high_threshold when power on*/
    if(policy->cur >= gpu_high_threshold)
        gpufreq_ddr_constraint_update(&gpufreq_ddr_constraint[gpu],
                                      policy->cur,
                                      0,
                                      gpu_high_threshold);
#endif
    return 0;
}

static int pxa988_gpufreq_qos_update(unsigned int chip, unsigned int min, unsigned int freq)
{
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT
    struct pm_qos_request *qos = NULL;

    if(min)
        qos = gc_qos[chip].pm_qos_req_min;
    else
        qos = gc_qos[chip].pm_qos_req_max;

    pm_qos_update_request(qos, freq);
#endif

    return 0;
}

static unsigned int pxa988_gpufreq_get_axi (unsigned int gpu)
{
    gceSTATUS status;
    unsigned int rate = ~0;

    gcmkONERROR(gckOS_QueryClkRate(gpu_os, gpu, gcvTRUE, &rate));
    return rate;

OnError:
    debug_log(GPUFREQ_LOG_ERROR, "failed to get clk rate of gpu %u\n", gpu);
    return -EINVAL;
}

#endif /* End of MRVL_CONFIG_ENABLE_GPUFREQ */


