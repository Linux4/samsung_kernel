/*
 * include/gpufreq.h
 *
 * Author: Watson Wang <zswang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */
#ifndef __GPUFREQ_H__
#define __GPUFREQ_H__

#include "gc_hal.h"
#include "gc_hal_kernel_features.h"

#if MRVL_CONFIG_ENABLE_GPUFREQ
#include <linux/kobject.h>
#include <linux/completion.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>

#include <plat/devfreq.h>

#if defined(TUNE_DDR_UPTHESHOLD_WITH_GPU)
#define MRVL_CONFIG_DEVFREQ_GOV_THROUGHPUT      1
#else
#define MRVL_CONFIG_DEVFREQ_GOV_THROUGHPUT      0
#endif

#if MRVL_CONFIG_DEVFREQ_GOV_THROUGHPUT
#include <linux/platform_data/gpu4dev.h>
#endif

#define IN
#define OUT
#define INOUT
#define __GPUFREQ_EXPORT_TO_GC

/*
 * reserve extra empty slots for platform like 988/986/1088
 * in order to be compatible with pxa1L88 alike which has
 * three cores.
 */
#if MRVL_PLATFORM_PXA988_FAMILY
#   define GPUFREQ_GPU_NUMS     3
#endif

#define GPUFREQ_NAME_LEN        16

#define GPUFREQ_LOG_ERROR       4
#define GPUFREQ_LOG_WARNING     3
#define GPUFREQ_LOG_INFO        2
#define GPUFREQ_LOG_DEBUG       1
#define GPUFREQ_LOG_DEFAULT     GPUFREQ_LOG_WARNING

/*
#if defined(DEBUG)
#define debug_log(fmt, ...)                 \
     printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define debug_log(fmt, ...)                 \
     no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif
*/
extern unsigned int log_level;
#define debug_log(level, fmt, ...)      \
    if(level >= log_level)    \
    {                                   \
        gcmkPRINT("%d| gpufreq: %s: " fmt, level, __FUNCTION__, ##__VA_ARGS__); \
    }

#define for_each_gpu(gpu)           \
    for ((gpu) = 0; (gpu) < has_avail_gpu_numbers(); (gpu)++)

#define KHZ_TO_HZ(val)     ((val) * 1000)
#define HZ_TO_KHZ(val)     ((val) / 1000)

 /*
  * gpufreq structure declaration
 */
struct gpufreq_gpuinfo;
struct gpufreq_policy;
struct gpufreq_governor;
struct gpufreq_freqs;
struct gpufreq_driver;
struct gpufreq_frequency_table;

struct gpufreq_gpuinfo {
    unsigned int        min_freq;
    unsigned int        max_freq;
};

struct gpufreq_real_policy {
    unsigned int        min_freq;
    unsigned int        max_freq;
    struct gpufreq_governor *governor;
};

#define GPUFREQ_PRECHANGE   (0)
#define GPUFREQ_POSTCHANGE  (1)

#if MRVL_CONFIG_DEVFREQ_GOV_THROUGHPUT
#define st_trans(action)                         \
    if(((action) == (GPUFREQ_POSTCHANGE_UP))||  \
        ((action) == (GPUFREQ_POSTCHANGE_DOWN)))\
    {                                            \
        action = GPUFREQ_POSTCHANGE;             \
    }
#else
#define st_trans(action)
#endif

struct gpufreq_freqs {
    unsigned int gpu;
    unsigned int old_freq;
    unsigned int new_freq;
};

/* enum for "setpolicy" type policy */
enum {
    GPUFREQ_POLICY_PERFORMANCE  = 0X1,
    GPUFREQ_POLICY_POWERSAVE    = 0X2
};

struct gpufreq_policy {
    /* current policy in which gpu? */
    unsigned int        gpu;
    struct gpufreq_gpuinfo      gpuinfo;

    /* present settings (may not be applied) */
    unsigned int        min;
    unsigned int        max;
    unsigned int        cur;
    struct gpufreq_governor     *governor;

    /* hold the actual setting gpu's running */
    struct gpufreq_real_policy  real_policy;

    /* to sync sysfs creation/deletion */
    struct kobject      kobj;
    struct completion   kobj_unregister;
};

int gpufreq_early_init(void);
int gpufreq_late_exit(void);
int __GPUFREQ_EXPORT_TO_GC gpufreq_init(gckOS Os);
void __GPUFREQ_EXPORT_TO_GC gpufreq_exit(gckOS Os);

/*********************************************************************
 *                      gpufreq notifier interface                   *
 *********************************************************************/
#define GPUFREQ_TRANSITION_NOTIFIER     (0)

int gpufreq_register_notifier(struct notifier_block *nb, unsigned int list);
int gpufreq_unregister_notifier(struct notifier_block *nb, unsigned int list);

void gpufreq_notify_transition(struct gpufreq_freqs *freqs, unsigned int state);
/*********************************************************************
 *                      gpufreq governor interface                   *
 *********************************************************************/
/* defined marco for governor events */
#define GPUFREQ_GOV_EVENT_START     (0x0)
#define GPUFREQ_GOV_EVENT_STOP      (0x1)
#define GPUFREQ_GOV_EVENT_LIMITS    (0x2)
#define GPUFREQ_GOV_EVENT_SUSPEND   (0x3)
#define GPUFREQ_GOV_EVENT_RESUME    (0x4)

struct gpufreq_governor {
    char        name[GPUFREQ_NAME_LEN];
    int (*init)(void);
    void (*exit)(void);
    int (*governor) (struct gpufreq_policy *policy, unsigned int event);
    struct list_head    governor_list;
    int refs;
    struct module       *owner;
};

extern int gpufreq_driver_target(IN struct gpufreq_policy *policy,
                                 IN unsigned int target_freq,
                                 IN unsigned int relation);

extern int __gpufreq_driver_target(IN struct gpufreq_policy *policy,
                                   IN unsigned int target_freq,
                                   IN unsigned int relation);

extern int __gpufreq_driver_get(IN unsigned int gpu);

int gpufreq_register_governor(IN struct gpufreq_governor *governor);
void gpufreq_unregister_governor(IN struct gpufreq_governor *governor);

/*********************************************************************
 *                      gpufreq driver interface                     *
 *********************************************************************/
#define GPUFREQ_RELATION_L  0  /* lowest frequency at or above target */
#define GPUFREQ_RELATION_H  1  /* highest frequency below or at target */

struct gpufreq_driver {
    struct module   *owner;
    char        name[GPUFREQ_NAME_LEN];

    int (*init) (struct gpufreq_policy *policy);
    int (*verify) (struct gpufreq_policy *policy);

    /*
       - define one out of two.
            @setpolicy: for policy_performance and policy_powersave
            @target:    for other policies which dynamically change frequency
    */
    int (*setpolicy) (struct gpufreq_policy *policy);
    int (*target) (struct gpufreq_policy *policy,
                unsigned int target_freq,
                unsigned int relation);

    unsigned int (*get) (unsigned int chip);

    int (*exit) (struct gpufreq_policy *policy);
    struct gpufreq_freq_attr    **attr;
};

int gpufreq_register_driver(gckOS Os, struct gpufreq_driver *driver_data);
int gpufreq_unregister_driver(gckOS Os, struct gpufreq_driver *driver);

void gpufreq_verify_within_limits(
            struct gpufreq_policy *policy,
            unsigned int min,
            unsigned int max);

/*********************************************************************
 *                            QoS interfaces                         *
 *********************************************************************/
#if MRVL_CONFIG_ENABLE_QOS_SUPPORT

#ifndef gpufreq_min
#define gpufreq_min     gcmMIN
#endif

#ifndef gpufreq_max
#define gpufreq_max     gcmMAX
#endif

struct _gc_qos {
    unsigned int gpu;
    int pm_qos_class_min;
    struct pm_qos_request *pm_qos_req_min;
    struct notifier_block *notifier_min;
    int pm_qos_class_max;
    struct pm_qos_request *pm_qos_req_max;
    struct notifier_block *notifier_max;
};

#define DECLARE_META_REQUEST(CORENAME, MINMAX)                                 \
    static struct pm_qos_request gpufreq_qos_req_##CORENAME##_##MINMAX = {     \
        .name = "gpufreq_" #CORENAME "_" #MINMAX,                              \
    };

#define IMPLEMENT_META_NOTIFIER(GPU, CORENAME, MINMAX, RELATION)               \
    static int gpufreq_##CORENAME##_##MINMAX##_notify(struct notifier_block *b, \
                        unsigned long VALUE, void *v)                          \
    {                                                                          \
        int ret;                                                               \
        unsigned int gpu = GPU;                                                \
        unsigned long freq, val = VALUE;                                       \
        struct gpufreq_policy *policy;                                         \
                                                                               \
        pr_debug("[%d] gpufreq: qos notify %lu (KHZ)\n", gpu, VALUE);          \
                                                                               \
        freq = __gpufreq_driver_get(gpu);                                      \
        if(freq == (unsigned long)-EINVAL)                                     \
            return NOTIFY_BAD;                                                 \
                                                                               \
        policy = gpufreq_policy_get(gpu);                                      \
        if(!policy)                                                            \
            return NOTIFY_BAD;                                                 \
                                                                               \
        ret = __gpufreq_driver_target(policy, val, RELATION);                  \
        gpufreq_policy_put(policy);                                            \
        if(ret < 0)                                                            \
            return NOTIFY_BAD;                                                 \
                                                                               \
        return NOTIFY_OK;                                                      \
    }                                                                          \
                                                                               \
    static struct notifier_block gpufreq_##CORENAME##_##MINMAX##_notifier = {  \
        .notifier_call = gpufreq_##CORENAME##_##MINMAX##_notify,               \
    };

#define DECLARE_META_GC_QOS(GPU, CORENAMELOW, CORENAMEUPP)      \
{                                                               \
    .gpu              = GPU,                                    \
    .pm_qos_class_min = PM_QOS_GPUFREQ_##CORENAMEUPP##_MIN,     \
    .pm_qos_req_min   = &gpufreq_qos_req_##CORENAMELOW##_min,   \
    .notifier_min     = &gpufreq_##CORENAMELOW##_min_notifier,  \
    .pm_qos_class_max = PM_QOS_GPUFREQ_##CORENAMEUPP##_MAX,     \
    .pm_qos_req_max   = &gpufreq_qos_req_##CORENAMELOW##_max,   \
    .notifier_max     = &gpufreq_##CORENAMELOW##_max_notifier,  \
}

#define DECLARE_META_GC_QOS_3D      DECLARE_META_GC_QOS(0, 3d, 3D)
#define DECLARE_META_GC_QOS_2D      DECLARE_META_GC_QOS(1, 2d, 2D)
#define DECLARE_META_GC_QOS_SH      DECLARE_META_GC_QOS(2, sh, SH)

#endif
/*********************************************************************
 *                            other interface                        *
 *********************************************************************/
struct gpufreq_freq_attr {
    struct attribute attr;
    ssize_t (*show)(struct gpufreq_policy *, char *);
    ssize_t (*store)(struct gpufreq_policy *, const char *, size_t count);
};

#define gpufreq_freq_attr_ro(_name) \
static struct gpufreq_freq_attr _name =     \
__ATTR(_name, 0444, show_##_name, NULL)

#define gpufreq_freq_attr_rw(_name)     \
static struct gpufreq_freq_attr _name =         \
__ATTR(_name, 0644, show_##_name, store_##_name)

int gpufreq_get_cur_policy(OUT struct gpufreq_policy *policy, IN unsigned int chip);
int gpufreq_get_gpu_load(unsigned int gpu, unsigned int t);

/*********************************************************************
 *                       gpufreq default governor                    *
 *********************************************************************/
extern struct gpufreq_governor gpufreq_gov_ondemand;
#define GPUFREQ_DEFAULT_GOVERNOR    (&gpufreq_gov_ondemand)

/*********************************************************************
 *                            frequency table                        *
 *********************************************************************/
#define GPUFREQ_ENTRY_INVALID   ~0
#define GPUFREQ_TABLE_END       ~1

struct gpufreq_frequency_table {
    unsigned int    index;
    unsigned int    frequency;
};

int gpufreq_frequency_table_gpuinfo(struct gpufreq_policy *policy,
                                    struct gpufreq_frequency_table *table);

int gpufreq_frequency_table_target(struct gpufreq_policy *policy,
                                   struct gpufreq_frequency_table *table,
                                   unsigned int target_freq,
                                   unsigned int relation,
                                   unsigned int *index);

struct gpufreq_policy * gpufreq_policy_get(unsigned int gpu);
void gpufreq_policy_put(struct gpufreq_policy *policy_data);

#endif /* END of MRVL_CONFIG_ENABLE_GPUFREQ */
#endif /* END of __GPUFREQ_H__ */
