/*
 * gpufreq.c
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
#include <linux/list.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/slab.h>

unsigned int log_level = GPUFREQ_LOG_DEFAULT;
/* variables declaration  */
static LIST_HEAD(gpufreq_governor_list);
static DEFINE_MUTEX(gpufreq_governor_mutex);
static DEFINE_SPINLOCK(gpufreq_driver_lock);
static struct gpufreq_driver *gpufreq_driver;

/* notifier list: to notify GPU clock rate changes. */
static unsigned int inited_gpufreq_trans_notifier_list = 0;
static struct srcu_notifier_head gpufreq_trans_notifier_list;

static gckOS gpu_os = NULL;

static const char* const _gpu_type[] = {
    [gcvCORE_MAJOR]     = "3D",
    [gcvCORE_2D]        = "2D",
    [gcvCORE_SH]        = "Shader",
};

extern struct gpufreq_governor gpufreq_gov_ondemand;
extern struct gpufreq_governor gpufreq_gov_conservative;
extern struct gpufreq_governor gpufreq_gov_userspace;
extern struct gpufreq_governor gpufreq_gov_performance;
extern struct gpufreq_governor gpufreq_gov_powersave;

struct gpufreq_governor *gov_ptrs[] = {
    &gpufreq_gov_ondemand,
    &gpufreq_gov_conservative,
    &gpufreq_gov_userspace,
    &gpufreq_gov_performance,
    &gpufreq_gov_powersave,
    NULL
};

struct __gpufreq_global {
    int                     gpufreq_policy_gpu;
    struct gpufreq_policy * gpufreq_policy_data;
    struct rw_semaphore     gpufreq_policy_rwsem;
}gpufreq_global[GPUFREQ_GPU_NUMS];

#define GPU_ELEM(var, gpu) gpufreq_global[gpu].var

static int lock_policy_rwsem_read(int gpu)
{
    int policy_gpu = GPU_ELEM(gpufreq_policy_gpu, gpu);
    BUG_ON(policy_gpu == -1);
    down_read(&GPU_ELEM(gpufreq_policy_rwsem, policy_gpu));
    return 0;
}

static int lock_policy_rwsem_write(int gpu)
{
    int policy_gpu = GPU_ELEM(gpufreq_policy_gpu, gpu);
    BUG_ON(policy_gpu == -1);
    down_write(&GPU_ELEM(gpufreq_policy_rwsem, policy_gpu));
    return 0;
}

static void unlock_policy_rwsem_read(int gpu)
{
    int policy_gpu = GPU_ELEM(gpufreq_policy_gpu, gpu);
    BUG_ON(policy_gpu == -1);
    up_read(&GPU_ELEM(gpufreq_policy_rwsem, policy_gpu));
}

static void unlock_policy_rwsem_write(int gpu)
{
    int policy_gpu = GPU_ELEM(gpufreq_policy_gpu, gpu);
    BUG_ON(policy_gpu == -1);
    up_write(&GPU_ELEM(gpufreq_policy_rwsem, policy_gpu));
}

/***************************************************
 *  function declaration
 ***************************************************/
static struct gpufreq_governor * _gpufreq_find_governor(const char *str_governor);

static int _gpufreq_set_policy(struct gpufreq_policy *cur_policy,
                                struct gpufreq_policy *policy);

static int _gpufreq_governor_notifier(struct gpufreq_policy *policy,
                                unsigned int event);

/***************************************************
 *  sysfs interfaces
 ***************************************************/
/* [RW] attr: print_level */
static ssize_t show_print_level(struct gpufreq_policy *policy, char *buf)
{
    return sprintf(buf, "[show] current log level: %u\nlog levels:\n1\tdebug\n2\tinfo\n3\twarning\n4\terror\n", log_level);
}

static ssize_t store_print_level(struct gpufreq_policy *policy, const char *buf, size_t count)
{
    unsigned int value = ~1;
    int ret = -EINVAL;

    ret = sscanf(buf, "%u", &value);
    if(ret != 1)
        goto err_out;

    if(value < GPUFREQ_LOG_DEBUG)
        log_level = GPUFREQ_LOG_DEBUG;
    else if (value > GPUFREQ_LOG_ERROR)
        log_level = GPUFREQ_LOG_ERROR;
    else
        log_level = value;

    return count;
err_out:
    return (ssize_t)ret;
}

gpufreq_freq_attr_rw(print_level);

#define show_attr(attr_name, element)                                \
static ssize_t show_##attr_name(struct gpufreq_policy *policy, char *buf)        \
{                                                                   \
    return sprintf(buf, "%u\n", policy->element);       \
}

/* [RO] attr: gpuinfo_max_freq
 *            gpuinfo_min_freq
 *            scaling_cur_freq
 */
show_attr(gpuinfo_max_freq, gpuinfo.max_freq);
show_attr(gpuinfo_min_freq, gpuinfo.min_freq);

gpufreq_freq_attr_ro(gpuinfo_max_freq);
gpufreq_freq_attr_ro(gpuinfo_min_freq);

static ssize_t show_scaling_cur_freq(struct gpufreq_policy *policy, char *buf)
{
    unsigned int freq = ~0, gpu = 0;

    gpu = policy->gpu;
    if(gpufreq_driver->get)
        freq = gpufreq_driver->get(gpu);

    return sprintf(buf, "%u\n", freq);
}
gpufreq_freq_attr_ro(scaling_cur_freq);

static ssize_t show_gputype(struct gpufreq_policy *policy, char *buf)
{
    unsigned int gpu = 0;

    gpu = policy->gpu;
    return sprintf(buf, "%s\n", _gpu_type[gpu]);
}
gpufreq_freq_attr_ro(gputype);

/* [RW] attr: scaling_max_freq */
show_attr(scaling_max_freq, max);
static ssize_t store_scaling_max_freq(struct gpufreq_policy *policy, const char *buf, size_t count)
{
    int ret = -EINVAL;
    struct gpufreq_policy new_policy;

    ret = gpufreq_get_cur_policy(&new_policy, policy->gpu);
    if(ret)
        goto err_out;

    ret = sscanf(buf, "%u", &new_policy.max);
    if(ret != 1)
        goto err_out;

    ret = _gpufreq_set_policy(policy, &new_policy);

    policy->real_policy.max_freq = policy->max;

    if(ret)
        goto err_out;

    return count;
err_out:
    return (ssize_t)ret;
}

gpufreq_freq_attr_rw(scaling_max_freq);

/* [RW] attr: scaling_min_freq */
show_attr(scaling_min_freq, min);
static ssize_t store_scaling_min_freq(struct gpufreq_policy *policy, const char *buf, size_t count)
{
    int ret = -EINVAL;
    struct gpufreq_policy new_policy;

    ret = gpufreq_get_cur_policy(&new_policy, policy->gpu);
    if(ret)
        goto err_out;

    ret = sscanf(buf, "%u", &new_policy.min);
    if(ret != 1)
        goto err_out;

    ret = _gpufreq_set_policy(policy, &new_policy);

    policy->real_policy.min_freq = policy->min;

    if(ret)
        goto err_out;

    return count;
err_out:
    return (ssize_t)ret;
}

gpufreq_freq_attr_rw(scaling_min_freq);

/* [RW] attr: scaling_governor */
static ssize_t show_scaling_governor(struct gpufreq_policy *policy, char *buf)
{
    return scnprintf(buf, GPUFREQ_NAME_LEN, "%s\n", policy->governor->name);
}

static ssize_t store_scaling_governor(struct gpufreq_policy *policy, const char *buf, size_t count)
{
    int ret = -EINVAL;
    char gov_str[GPUFREQ_NAME_LEN];
    struct gpufreq_policy new_policy;
    struct gpufreq_governor *t;

    ret = gpufreq_get_cur_policy(&new_policy, policy->gpu);
    if(ret)
        goto err_out;

    /* parse input string */
    memset(&gov_str[0], GPUFREQ_NAME_LEN, 0);
    ret = sscanf(buf, "%15s", &gov_str[0]);
    if(ret != 1)
        goto err_out;

    /* search string for corresponding governor in list */
    mutex_lock(&gpufreq_governor_mutex);
    t = _gpufreq_find_governor(&gov_str[0]);

    if(t == NULL)
    {
        debug_log(GPUFREQ_LOG_ERROR, "No registration found for governor %s\n", &gov_str[0]);
        mutex_unlock(&gpufreq_governor_mutex);
        ret = -EINVAL;
        goto err_out;
    }

    new_policy.governor = t;
    mutex_unlock(&gpufreq_governor_mutex);

    /* set this new governor */
    ret = _gpufreq_set_policy(policy, &new_policy);

    /* update new governor no matter failure or success */
    policy->real_policy.governor = policy->governor;

    if(ret)
        goto err_out;

    return count;
err_out:
    return ret;
}
gpufreq_freq_attr_rw(scaling_governor);

/* [RO] attr: scaling_available_governor */
static ssize_t show_scaling_available_governors(struct gpufreq_policy *policy, char *buf)
{
    struct gpufreq_governor *t;
    ssize_t index = 0;

    list_for_each_entry(t, &gpufreq_governor_list, governor_list)
    {
        index += scnprintf(&buf[index], GPUFREQ_NAME_LEN, "%s ", t->name);
    }

    index += sprintf(&buf[index], "\n");

    return index;
}

gpufreq_freq_attr_ro(scaling_available_governors);

/* attribute group */
static struct attribute *default_attrs[] = {
    &print_level.attr,
    &gpuinfo_max_freq.attr,
    &gpuinfo_min_freq.attr,
    &scaling_cur_freq.attr,
    &scaling_max_freq.attr,
    &scaling_min_freq.attr,
    &scaling_governor.attr,
    &scaling_available_governors.attr,
    &gputype.attr,
    NULL
};

#define to_policy(k) container_of(k, struct gpufreq_policy, kobj)
#define to_attr(a) container_of(a, struct gpufreq_freq_attr, attr)

static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct gpufreq_policy *policy = to_policy(kobj);
    struct gpufreq_freq_attr *fattr = to_attr(attr);
    ssize_t ret = -EINVAL;

    policy = gpufreq_policy_get(policy->gpu);
    if(!policy)
        goto no_policy;

    if (lock_policy_rwsem_read(policy->gpu) < 0)
        goto fail;

    if (fattr->show)
        ret = fattr->show(policy, buf);
    else
        ret = -EIO;

    unlock_policy_rwsem_read(policy->gpu);

fail:
    gpufreq_policy_put(policy);
no_policy:
    return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
                const char *buf, size_t count)
{
    struct gpufreq_policy *policy = to_policy(kobj);
    struct gpufreq_freq_attr *fattr = to_attr(attr);
    ssize_t ret = -EINVAL;

    policy = gpufreq_policy_get(policy->gpu);
    if(!policy)
        goto no_policy;

    if (lock_policy_rwsem_write(policy->gpu) < 0)
        goto fail;

    if (fattr->show)
        ret = fattr->store(policy, buf, count);
    else
        ret = -EIO;

    unlock_policy_rwsem_write(policy->gpu);

fail:
    gpufreq_policy_put(policy);
no_policy:
    return ret;
}

static void gpufreq_sysfs_release(struct kobject *kobj)
{
    struct gpufreq_policy *policy = to_policy(kobj);
    debug_log(GPUFREQ_LOG_INFO, "last reference is dropped\n");
    complete(&policy->kobj_unregister);
}

static const struct sysfs_ops sysfs_ops = {
//  ssize_t (*show)(struct kobject *, struct attribute *,char *);
//  ssize_t (*store)(struct kobject *,struct attribute *,const char *, size_t);
    .show   = show,
    .store  = store,
};

static struct kobj_type ktype_gpufreq = {
    .sysfs_ops      = &sysfs_ops,
    .default_attrs  = default_attrs,
    .release        = gpufreq_sysfs_release,
};

static struct gpufreq_governor * _gpufreq_find_governor(const char *str_governor)
{
    struct gpufreq_governor *t;

    list_for_each_entry(t, &gpufreq_governor_list, governor_list)
        if (!strnicmp(str_governor, t->name, GPUFREQ_NAME_LEN))
            return t;

    return NULL;
}

/**
 * gpufreq_add_dev_interface
 *     - Adds sysfs interfaces for gpufreq
 *     - Start governor
 */
static int _gpufreq_add_dev_interface(unsigned int gpu,
                                     struct gpufreq_policy *policy,
                                     struct gcsDEVOBJECT *pDevObj)
{
    int ret = 0;
    unsigned long flags;
    struct gpufreq_policy  new_policy;
    struct gpufreq_freq_attr **drv_attr;

    {
        /* set up sysfs files for common attributes */
        ret = kobject_init_and_add(&policy->kobj, &ktype_gpufreq,
                                   (struct kobject*)pDevObj->kobj, "gpufreq");
        if(ret)
            return ret;

        /* set up files for this specific gpu core */
        drv_attr = gpufreq_driver->attr;
        while ((drv_attr) && (*drv_attr))
        {
            ret = sysfs_create_file(&policy->kobj, &((*drv_attr)->attr));
            if (ret)
                goto err_out_kobj_put;
            drv_attr++;
        }
    }

    /* save policy pointer to global structure */
    spin_lock_irqsave(&gpufreq_driver_lock, flags);

    GPU_ELEM(gpufreq_policy_data, gpu) = policy;

    spin_unlock_irqrestore(&gpufreq_driver_lock, flags);

    /*
     * !!TRICK: duplicate once, and start governor,
     *          using governor switch ....
     */
    memcpy(&new_policy, policy, sizeof(struct gpufreq_policy));
    /* make current governor to NULL */
    policy->governor = NULL;

    ret = _gpufreq_set_policy(policy, &new_policy);

    /* update default governor */
    policy->real_policy.governor = policy->governor;

    if(ret)
    {
        debug_log(GPUFREQ_LOG_WARNING, "failed in setting policy.\n");
    }
    return ret;

err_out_kobj_put:
    kobject_put(&policy->kobj);
    wait_for_completion(&policy->kobj_unregister);
    return ret;
}

/**
 * gpufreq_add_dev - Adds the gpufreq interfaces for GPU device
 */
static int gpufreq_add_dev(struct gcsDEVOBJECT *pDevObj)
{
    unsigned int gpu = pDevObj->id;
    int ret = 0;
    struct gpufreq_policy *policy;

    debug_log(GPUFREQ_LOG_INFO, "add gpu %d\n", gpu);

    ret = -ENOMEM;
    policy = kzalloc(sizeof(struct gpufreq_policy), GFP_KERNEL);
    if(!policy)
        goto nomem_out;

    policy->gpu = gpu;
    GPU_ELEM(gpufreq_policy_gpu, gpu) = gpu;
    ret = lock_policy_rwsem_write(gpu);

    /* completion for sysfs create/release */
    init_completion(&policy->kobj_unregister);

    /* initialze current governor with default */
    policy->governor = GPUFREQ_DEFAULT_GOVERNOR;

    /* HERE we need to initialize driver, hence on, gpufreq must
       be able to accept all calls ->verify and ->setpolicy for this gpu.
    */
    ret = gpufreq_driver->init(policy);
    if(ret)
    {
        debug_log(GPUFREQ_LOG_ERROR, "initialize gpufreq driver failed.\n");
        goto err_out;
    }

    /* update real_policy with INITIALIZED policy */
    policy->real_policy.min_freq = policy->min;
    policy->real_policy.max_freq = policy->max;

    /* add sysfs nodes, attributes and interfaces */
    ret = _gpufreq_add_dev_interface(gpu, policy, pDevObj);
    if(ret)
    {
        debug_log(GPUFREQ_LOG_ERROR, "add dev interface failed.\n");
        goto err_out;
    }

    unlock_policy_rwsem_write(gpu);
    kobject_uevent(&policy->kobj, KOBJ_ADD);

    debug_log(GPUFREQ_LOG_INFO, "add dev complete.\n");

    return 0;

err_out:
    unlock_policy_rwsem_write(gpu);
nomem_out:
    return ret;
}

static int gpufreq_remove_dev(struct gcsDEVOBJECT *pDevObj)
{
    unsigned int gpu = pDevObj->id;
    unsigned long flags;
    struct gpufreq_policy *data;

    /* release data */
    debug_log(GPUFREQ_LOG_INFO, "remove gpu %d\n", gpu);

    spin_lock_irqsave(&gpufreq_driver_lock, flags);
    data = GPU_ELEM(gpufreq_policy_data, gpu);

    if (!data) {
        spin_unlock_irqrestore(&gpufreq_driver_lock, flags);
        return -EINVAL;
    }
    /* reset policy data for gpu */
    GPU_ELEM(gpufreq_policy_data, gpu) = NULL;
    spin_unlock_irqrestore(&gpufreq_driver_lock, flags);

    /* stop governor if it's needed */
    if(gpufreq_driver->target)
        _gpufreq_governor_notifier(data, GPUFREQ_GOV_EVENT_STOP);

    /* remove all sysfs links. */
    kobject_put(&data->kobj);
    wait_for_completion(&data->kobj_unregister);

    kfree(data);

    return 0;
}

static int gpufreq_handle_gov_timer(struct gcsDEVOBJECT *pDevObj, unsigned long action)
{
    int ret = 0;
    unsigned int gpu = pDevObj->id;
    struct gpufreq_policy *policy;

    policy = gpufreq_policy_get(gpu);
    if(!policy)
    {
        ret = -EINVAL;
        goto out_policy;
    }

    if(gpufreq_driver->target)
        _gpufreq_governor_notifier(policy, action);

    gpufreq_policy_put(policy);

out_policy:
    return ret;
}
/***************************************************
 *  gpufreq governor interfaces
 ***************************************************/
int gpufreq_driver_target(IN struct gpufreq_policy *policy,
                          IN unsigned int target_freq,
                          IN unsigned int relation)
{
    int ret = -EINVAL;

    policy = gpufreq_policy_get(policy->gpu);
    if(!policy)
        goto no_policy;

    if(unlikely(lock_policy_rwsem_write(policy->gpu)))
        goto lock_fail;

    ret = __gpufreq_driver_target(policy, target_freq, relation);

    unlock_policy_rwsem_write(policy->gpu);

lock_fail:
    gpufreq_policy_put(policy);
no_policy:
    return ret;
}

int __gpufreq_driver_target(IN struct gpufreq_policy *policy,
                            IN unsigned int target_freq,
                            IN unsigned int relation)
{
    int ret = -EINVAL;

    if(gpufreq_driver->target)
        ret = gpufreq_driver->target(policy, target_freq, relation);

    return ret;
}

int __gpufreq_driver_get(IN unsigned int gpu)
{
    int ret = -EINVAL;

    if(gpufreq_driver->get)
        ret = gpufreq_driver->get(gpu);

    return ret;
}

static int gpufreq_governor_get(struct gpufreq_governor *governor, unsigned int event)
{
    /* this governor was not added to governor list */
    if (!_gpufreq_find_governor(governor->name))
        return -EINVAL;

    /* increase refs only when starting governor. */
    if(event == GPUFREQ_GOV_EVENT_START)
        ++governor->refs;
    return 0;
}

static int gpufreq_governor_put(struct gpufreq_governor *governor)
{
    --governor->refs;
    return 0;
}

/**
 *  gpufreq_register_governor
 *      add this governor to gpufreq_governor_list
 *                             if it has not been added.
 */
int gpufreq_register_governor(IN struct gpufreq_governor *governor)
{
    int err;

    if (!governor)
        return -EINVAL;

    mutex_lock(&gpufreq_governor_mutex);

    err = -EBUSY;
    if (_gpufreq_find_governor(governor->name) == NULL) {
        err = 0;
        list_add(&governor->governor_list, &gpufreq_governor_list);
    }

    ++governor->refs;
    mutex_unlock(&gpufreq_governor_mutex);
    return err;
}

/**
 *  gpufreq_unregister_governor: delete this governor if possible
 */
void gpufreq_unregister_governor(IN struct gpufreq_governor *governor)
{
    if (!governor)
        return;

    mutex_lock(&gpufreq_governor_mutex);

    --governor->refs;
    if(governor->refs == 0)
    {
        list_del(&governor->governor_list);
    }
    else
    {
        debug_log(GPUFREQ_LOG_WARNING, "warning: governor %s has imbalance refcount %d\n", governor->name, governor->refs);
    }

    mutex_unlock(&gpufreq_governor_mutex);
}


/***************************************************
 *  gpufreq driver interfaces
 ***************************************************/
static int gpufreq_notifier_call(struct notifier_block * nb,
                            unsigned long action, void * pdev_obj)
{
    int ret = NOTIFY_OK;
    struct gcsDEVOBJECT *pDevObj = (struct gcsDEVOBJECT *)pdev_obj;

    if(pDevObj)
    {
        switch(action) {
        case GPUFREQ_GPU_EVENT_INIT:
            gpufreq_add_dev(pDevObj);
            break;
        case GPUFREQ_GPU_EVENT_DESTORY:
            gpufreq_remove_dev(pDevObj);
            break;
        case GPUFREQ_GPU_EVENT_POWER_ON:
            gpufreq_handle_gov_timer(pDevObj, GPUFREQ_GOV_EVENT_RESUME);
            break;
        case GPUFREQ_GPU_EVENT_POWER_OFF:
            gpufreq_handle_gov_timer(pDevObj, GPUFREQ_GOV_EVENT_SUSPEND);
            break;
        default:
            ret = NOTIFY_BAD;
        }
    }

    return ret;
}

#if MRVL_PULSE_EATER_COUNT_NUM
static int pulseEater_notifier_call (struct notifier_block *nb,
                    unsigned long action, void *data)
{
    struct gpufreq_freqs *freqs = (struct gpufreq_freqs *)data;
    int ret = NOTIFY_DONE;

    if(!freqs ||
        /*GC320 not support*/
        freqs->gpu == gcvCORE_2D)
    {
        return ret;
    }

    st_trans(action);

    switch(action)
    {
        case GPUFREQ_PRECHANGE:
            break;
        case GPUFREQ_POSTCHANGE:
            if(gpu_os)
            {
                gckOS_ModifyPulseEaterPollingPeriod(gpu_os, freqs->new_freq, freqs->gpu);
            }
            break;
        default:
            debug_log(GPUFREQ_LOG_WARNING, "[gpufreq] %s (%d)", __func__, __LINE__);
            break;
    }

    return ret;
}
#endif

/*
struct notifier_block {
    int (*notifier_call)(struct notifier_block *, unsigned long, void *);
    struct notifier_block __rcu *next;
    int priority;
};
*/
static struct notifier_block gpufreq_notifier_block = {
    .notifier_call = gpufreq_notifier_call,
};

#if MRVL_PULSE_EATER_COUNT_NUM
static struct notifier_block pulseEater_gpufreq_notifier_block = {
    .notifier_call = pulseEater_notifier_call,
};
#endif

/***************************************************
 *  gpufreq notifier interfaces
 ***************************************************/
static int _gpufreq_trans_notifier_list_init(void)
{
    srcu_init_notifier_head(&gpufreq_trans_notifier_list);
    inited_gpufreq_trans_notifier_list = 1;
    return 0;
}

int gpufreq_register_notifier(struct notifier_block *nb, unsigned int list)
{
    int ret;

    WARN_ON(!inited_gpufreq_trans_notifier_list);

    switch (list)
    {
    case GPUFREQ_TRANSITION_NOTIFIER:
        ret = srcu_notifier_chain_register(
                    &gpufreq_trans_notifier_list, nb);
        break;
    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

int gpufreq_unregister_notifier(struct notifier_block *nb, unsigned int list)
{
    int ret;

    WARN_ON(!inited_gpufreq_trans_notifier_list);

    switch (list)
    {
    case GPUFREQ_TRANSITION_NOTIFIER:
        ret = srcu_notifier_chain_unregister(
                    &gpufreq_trans_notifier_list, nb);
        break;
    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

void gpufreq_notify_transition(struct gpufreq_freqs *freqs, unsigned int state)
{
    struct gpufreq_policy *policy;
    unsigned int action;

    action = state;
    st_trans(action);
    policy = GPU_ELEM(gpufreq_policy_data, freqs->gpu);
    switch(action)
    {
    case GPUFREQ_PRECHANGE:
        debug_log(GPUFREQ_LOG_INFO, "PRE_CHANGE : gpu %u, freq %u -> %u",
                                freqs->gpu, freqs->old_freq, freqs->new_freq);
        srcu_notifier_call_chain(&gpufreq_trans_notifier_list,
                        state, freqs);
        break;

    case GPUFREQ_POSTCHANGE:
        debug_log(GPUFREQ_LOG_INFO, "POST_CHANGE: gpu %u, freq %u -> %u",
                                freqs->gpu, freqs->old_freq, freqs->new_freq);

        srcu_notifier_call_chain(&gpufreq_trans_notifier_list,
                        state, freqs);

        /* update current_freq of policy with new value */
        if(likely(policy) && likely(policy->gpu == freqs->gpu))
            policy->cur = freqs->new_freq;
        break;
    }
}

/**
 * gpufreq_register_driver - register a GPU Frequency driver
 */
int gpufreq_register_driver(gckOS Os, struct gpufreq_driver *driver_data)
{
    unsigned long flags;

    /* check gpufreq_driver callback functions */
    if (!driver_data || !driver_data->init || !driver_data->verify ||
        ((!driver_data->setpolicy) && (!driver_data->target)))
        return -EINVAL;

    /* save this driver to global gpufreq_driver */
    spin_lock_irqsave(&gpufreq_driver_lock, flags);
    if (gpufreq_driver)
    {
        spin_unlock_irqrestore(&gpufreq_driver_lock, flags);
        return -EBUSY;
    }
    gpufreq_driver = driver_data;
    spin_unlock_irqrestore(&gpufreq_driver_lock, flags);

    if(!gpu_os)
        gpu_os = Os;

#if MRVL_PULSE_EATER_COUNT_NUM
    if(has_feat_pulse_eater_profiler())
    {
        gpufreq_register_notifier(&pulseEater_gpufreq_notifier_block,
                                    GPUFREQ_TRANSITION_NOTIFIER);
    }
#endif

#if MRVL_CONFIG_DEVFREQ_GOV_THROUGHPUT
    gpufeq_register_dev_notifier(&gpufreq_trans_notifier_list);
#endif
    gckOS_GPUFreqNotifierRegister(Os, &gpufreq_notifier_block);
    debug_log(GPUFREQ_LOG_INFO, "driver %s up and running\n", driver_data->name);

    return 0;
}

/**
 * gpufreq_unregister_driver - unregister the current GPUFreq driver
 */
int gpufreq_unregister_driver(gckOS Os, struct gpufreq_driver *driver)
{
    unsigned long flags;

    if (!driver || (driver != gpufreq_driver))
        return -EINVAL;

    debug_log(GPUFREQ_LOG_INFO, "unregister driver %s\n", driver->name);

#if MRVL_PULSE_EATER_COUNT_NUM
    if(has_feat_pulse_eater_profiler())
    {
        gpufreq_unregister_notifier(&pulseEater_gpufreq_notifier_block,
                                    GPUFREQ_TRANSITION_NOTIFIER);
    }
#endif

    gckOS_GPUFreqNotifierUnregister(Os, &gpufreq_notifier_block);

    spin_lock_irqsave(&gpufreq_driver_lock, flags);
    gpufreq_driver = NULL;
    spin_unlock_irqrestore(&gpufreq_driver_lock, flags);

    return 0;
}

/**
 * gpufreq_early_init - initialize global variables
 *
 *  @usage  need to be invoked before driver registration
 */
int gpufreq_early_init(void)
{
    int gpu;
    struct gpufreq_governor **pgov;

    /* initialize global variables */
    for_each_gpu(gpu)
    {
        GPU_ELEM(gpufreq_policy_gpu, gpu) = -1;
        GPU_ELEM(gpufreq_policy_data, gpu) = NULL;
        init_rwsem(&GPU_ELEM(gpufreq_policy_rwsem, gpu));
    }

    /* init nb list */
    _gpufreq_trans_notifier_list_init();

    /* register governors */
    pgov = gov_ptrs;
    while((pgov) && (*pgov))
    {
        struct gpufreq_governor *governor = (*pgov);

        if(governor->init)
            governor->init();
        pgov++;
    }

    return 0;
}

int gpufreq_late_exit(void)
{
    struct gpufreq_governor **pgov;

    /* unregister governors */
    pgov = gov_ptrs;
    while((pgov) && (*pgov))
    {
        struct gpufreq_governor *governor = (*pgov);

        if(governor->exit)
            governor->exit();
        pgov++;
    }

    return 0;
}

/***************************************************
 *  gpufreq policy interfaces
 ***************************************************/
/**
 * _gpufreq_governor_notifier
 *  @brief          call registered governor callback function
 *                  to process the event
 *
 *  @param policy   policy to process
 *  @param event    event type for governor
 */
static int _gpufreq_governor_notifier(struct gpufreq_policy *policy, unsigned int event)
{
    int ret = 0;

    ret = gpufreq_governor_get(policy->governor, event);
    if(ret)
    {
        debug_log(GPUFREQ_LOG_ERROR, "error: fail to get governor %s\n", policy->governor->name);
        goto err_out;
    }

    debug_log(GPUFREQ_LOG_DEBUG, "schedule governor routine, event %d, %s\n", event, policy->governor->name);

    ret = policy->governor->governor(policy, event);

    /* return value is error, but with START event, then decrease ref count */
    if(event == GPUFREQ_GOV_EVENT_START && ret)
        gpufreq_governor_put(policy->governor);

    /* return value is success, but with STOP event, then decrease ref count */
    if(event == GPUFREQ_GOV_EVENT_STOP && !ret)
        gpufreq_governor_put(policy->governor);

err_out:
    return ret;
}

/**
 * gpufreq_policy
 *  @brief          set new policy
 *
 *  @param data     current policy
 *  @param policy   new policy to be set
 */
static int _gpufreq_set_policy(struct gpufreq_policy *cur_policy,
                struct gpufreq_policy *new_policy)
{
    int ret = 0;

    /* copy gpuinfo to new policy */
    memcpy(&new_policy->gpuinfo, &cur_policy->gpuinfo, sizeof(struct gpufreq_gpuinfo));

    if(new_policy->min > cur_policy->max)
    {
        debug_log(GPUFREQ_LOG_ERROR, "frequency range has no joint\n");
        goto err_out;
    }

   /*
    * siop requirement,
    * Maxlock freq is higher priority than Minlock freq.
    */
    if(new_policy->max < cur_policy->min)
       debug_log(GPUFREQ_LOG_DEBUG, "frequency range has thermal constraints\n");

    /* verify driver to see whether it can set freq
          within new policy's limites, and updata min/max
          frequency limit for input policy
    */
    ret = gpufreq_driver->verify(new_policy);
    if(ret)
    {
        goto err_out;
    }

    /* update minimum and maximize frequency of current policy */
    cur_policy->min = new_policy->min;
    cur_policy->max = new_policy->max;

    /* update governor */
    /* new governor doesn't match current governor, switching. */
    if(cur_policy->governor != new_policy->governor)
    {
        /* save current governor */
        struct gpufreq_governor *old_governor = cur_policy->governor;
        int status = 0;

        /* stop current governor */
        if(cur_policy->governor)
            status = _gpufreq_governor_notifier(cur_policy, GPUFREQ_GOV_EVENT_STOP);

        debug_log(GPUFREQ_LOG_DEBUG, "stop old governor, ret %d\n", status);
        /* governor switches */
        cur_policy->governor = new_policy->governor;

        /* start new governor */
        status = _gpufreq_governor_notifier(cur_policy, GPUFREQ_GOV_EVENT_START);

        debug_log(GPUFREQ_LOG_DEBUG, "start new governor, ret %d\n", status);
        if(status)
        {
            /* failure in switching governor, then fall back */
            if(old_governor)
            {
                cur_policy->governor = old_governor;
                status = _gpufreq_governor_notifier(cur_policy, GPUFREQ_GOV_EVENT_START);
                kobject_uevent(&cur_policy->kobj, KOBJ_ADD);
                debug_log(GPUFREQ_LOG_DEBUG, "start new old governor, ret %d\n", status);
            }
            ret = -EINVAL;
            goto err_out;
        }
        kobject_uevent(&cur_policy->kobj, KOBJ_ADD);
    }

    debug_log(GPUFREQ_LOG_DEBUG, "governor: change or update limits\n");
    /* limits may be changed, update limits for current policy */
    _gpufreq_governor_notifier(cur_policy, GPUFREQ_GOV_EVENT_LIMITS);

    return 0;
err_out:
    return ret;
}

/**
 * gpufreq_get_cur_policy - get the current gpufreq_policy
 */
int gpufreq_get_cur_policy(OUT struct gpufreq_policy *policy, IN unsigned int gpu)
{
    struct gpufreq_policy *cur_policy;

    if(!policy)
        return -EINVAL;

    cur_policy = gpufreq_policy_get(gpu);

    if(!cur_policy)
        return -EINVAL;

    memcpy(policy, cur_policy, sizeof(struct gpufreq_policy));

    gpufreq_policy_put(cur_policy);
    return 0;
}

struct gpufreq_policy * gpufreq_policy_get(unsigned int gpu)
{
    struct gpufreq_policy *policy;
    unsigned long flags;

    spin_lock_irqsave(&gpufreq_driver_lock, flags);

    if(!gpufreq_driver)
        goto err_to_unlock;

    policy = GPU_ELEM(gpufreq_policy_data, gpu);

    if(policy == NULL)
        goto err_to_unlock;

    if(!kobject_get(&policy->kobj))
        goto err_to_unlock;

    spin_unlock_irqrestore(&gpufreq_driver_lock, flags);

    return policy;

err_to_unlock:
    spin_unlock_irqrestore(&gpufreq_driver_lock, flags);
    return NULL;
}

void gpufreq_policy_put(struct gpufreq_policy *policy_data)
{
    kobject_put(&policy_data->kobj);
}

/***************************************************
 *  gpufreq frequency table interfaces
 ***************************************************/
int gpufreq_get_gpu_load(unsigned int gpu, unsigned int t)
{
    int load = -EINVAL, dutycycle = -EINVAL, ret = 0;
    unsigned int idle, core, interval = t;
    gcePulseEaterDomain domain = gcvPulse_Invali;

    if(!gpu_os)
    {
        debug_log(GPUFREQ_LOG_ERROR, "error: gc os object is not initialized\n");
        return -EINVAL;
    }

    core = gpu;

    switch(gpu)
    {
        case gcvCORE_SH:
            domain = gcvPulse_Shader;
            core   = gcvCORE_MAJOR;
            break;
        case gcvCORE_MAJOR:
            domain = gcvPulse_Core;
            break;
        case gcvCORE_2D:
            break;
        default:
            debug_log(GPUFREQ_LOG_ERROR, "error: invalidate gpu num\n");
            return -EINVAL;
    }

    /*2D core doesn't support HW counter*/
    if(gpu != gcvCORE_2D)
    {
        ret = gckOS_QueryPulseCountProfile(gpu_os, 0, domain, &dutycycle);
        if(ret)
        {
            debug_log(GPUFREQ_LOG_ERROR, "error: get domain: %d pulse count failed\n", domain);
            return -EINVAL;
        }
    }

    /* original way to get Profile*/
    ret = gckOS_QueryIdleProfile(gpu_os, core, &interval, &idle);

    if(ret)
    {
        debug_log(GPUFREQ_LOG_ERROR, "error: get idle profile failed\n");
        return -EINVAL;
    }

    load = (100 * (interval - idle))/interval;

    if(gpu != gcvCORE_2D)
    {
        debug_log(GPUFREQ_LOG_DEBUG, "domain(%d) On-suspend polling: %d, Pulse-Eater: %d", domain, load, dutycycle);
        /*load fall in [90, 93), dutycycle fall in [0, 80], keep it.*/
        if((load >= 90) && (load < 94) &&
           (dutycycle <= 80))
        {
            load -= 5;
        }
        /*load fall in [85, 90), dutycycle fall in [95, 100], increase the freq*/
        else if((load >= 85) && (load < 90) &&
            (dutycycle >= 95))
        {
            load += 5;
        }
    }

    return load;
}

void gpufreq_verify_within_limits(
            struct gpufreq_policy *policy,
            unsigned int min,
            unsigned int max)
{
    if (policy->min < min)
        policy->min = min;
    if (policy->max < min)
        policy->max = min;
    if (policy->min > max)
        policy->min = max;
    if (policy->max > max)
        policy->max = max;
    if (policy->min > policy->max)
        policy->min = policy->max;
    return;
}

int gpufreq_frequency_table_gpuinfo(struct gpufreq_policy *policy,
                                    struct gpufreq_frequency_table *table)
{
    unsigned int min_freq = ~0;
    unsigned int max_freq = 0;
    unsigned int i;

    for(i = 0; (table[i].frequency != GPUFREQ_TABLE_END); i++)
    {
        unsigned int freq = table[i].frequency;

        if(freq == GPUFREQ_ENTRY_INVALID)
        {
            debug_log(GPUFREQ_LOG_DEBUG, "table entry %u is invalid, skipping...\n", i);
            continue;
        }

        if(freq < min_freq)
            min_freq = freq;
        if(freq > max_freq)
            max_freq = freq;
    }

    /* update max/min frequency for policy */
    policy->min = policy->gpuinfo.min_freq = min_freq;
    policy->max = policy->gpuinfo.max_freq = max_freq;

    /* check max/min frequecy again */
    if(min_freq == ~0)
        return -EINVAL;
    else
        return 0;
}

int gpufreq_frequency_table_target(struct gpufreq_policy *policy,
                                   struct gpufreq_frequency_table *table,
                                   unsigned int target_freq,
                                   unsigned int relation,
                                   unsigned int *index)
{
    unsigned int i;
    struct gpufreq_frequency_table optimal = {
        .index = ~0,
        .frequency = 0,
    };
    struct gpufreq_frequency_table suboptimal = {
        .index = ~0,
        .frequency = 0,
    };

    debug_log(GPUFREQ_LOG_DEBUG, "request for target frequency %u (relation: %u)\n", target_freq, relation);

    /* init frequency for vary relation */
    switch(relation)
    {
    case GPUFREQ_RELATION_H:
        suboptimal.frequency = ~0;
        break;
    case GPUFREQ_RELATION_L:
        optimal.frequency = ~0;
        break;
    }

    /* search best fit frequency according to relation */
    for(i = 0; (table[i].frequency != GPUFREQ_TABLE_END); i++)
    {
        unsigned int freq = table[i].frequency;

        if(freq == GPUFREQ_ENTRY_INVALID)
            continue;

        /*
            remove this quick path if qos is enabled with such case:
                qos_max < policy->min
        */
#if 0
        if(freq < policy->min || freq > policy->max)
            continue;
#endif

        switch(relation)
        {
        case GPUFREQ_RELATION_H:
            if(freq <= target_freq)
            {
                /* highest but no higher than target_freq */
                if(freq >= optimal.frequency)
                {
                    optimal.frequency = freq;
                    optimal.index     = i;
                }
            }
            else
            {
                /* lowest but no lower than target_freq */
                if(freq <= suboptimal.frequency)
                {
                    suboptimal.frequency = freq;
                    suboptimal.index     = i;
                }
            }
            break;
        case GPUFREQ_RELATION_L:
            if(freq >= target_freq)
            {
                /* lowest but no lower than target_freq */
                if(freq <= optimal.frequency)
                {
                    optimal.frequency = freq;
                    optimal.index     = i;
                }
            }
            else
            {
                /* highest but no higher than target_freq */
                if(freq >= suboptimal.frequency)
                {
                    suboptimal.frequency = freq;
                    suboptimal.index     = i;
                }
            }
            break;
        }
    }

    if(optimal.index > i)
    {
        if(suboptimal.index > i)
            return -EINVAL;

        *index = suboptimal.index;
    }
    else
    {
        /* return index of best fit frequency */
        *index = optimal.index;
    }

    debug_log(GPUFREQ_LOG_DEBUG, "target frequency is index %u (freq %u, index %u)\n", *index,
                table[*index].frequency, table[*index].index);

    return 0;
}

#endif /* END of MRVL_CONFIG_ENABLE_GPUFREQ */
