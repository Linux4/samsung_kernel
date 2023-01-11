/*
 * devfreq: Generic Dynamic Voltage and Frequency Scaling (DVFS) Framework
 *	    for Non-CPU Devices.
 *
 * Copyright (C) 2011 Samsung Electronics
 *	MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_DEVFREQ_H__
#define __LINUX_DEVFREQ_H__

#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>

#define DEVFREQ_NAME_LEN 16

struct devfreq;

/**
 * struct devfreq_dev_status - Data given from devfreq user device to
 *			     governors. Represents the performance
 *			     statistics.
 * @total_time:		The total time represented by this instance of
 *			devfreq_dev_status
 * @busy_time:		The time that the device was working among the
 *			total_time.
 * @current_frequency:	The operating frequency.
 * @private_data:	An entry not specified by the devfreq framework.
 *			A device and a specific governor may have their
 *			own protocol with private_data. However, because
 *			this is governor-specific, a governor using this
 *			will be only compatible with devices aware of it.
 */
struct devfreq_dev_status {
	/* both since the last measure */
	unsigned long total_time;
	unsigned long busy_time;
	unsigned long current_frequency;
	int throughput;
	void *private_data;
};

/*
 * The resulting frequency should be at most this. (this bound is the
 * least upper bound; thus, the resulting freq should be lower or same)
 * If the flag is not set, the resulting frequency should be at most the
 * bound (greatest lower bound)
 */
#define DEVFREQ_FLAG_LEAST_UPPER_BOUND		0x1

/**
 * struct devfreq_pm_qos_table - An PM QoS requiement entry for devfreq dev.
 * @freq:		Lowest frequency to meet the QoS requirement
 *			represented by qos_value. If freq=0, it means that
 *			this element is the last in the array.
 * @qos_value:		The qos value defined in pm_qos_params.h
 *
 * Note that the array of devfreq_pm_qos_table should be sorted by freq
 * in the ascending order except for the last element, which should be 0.
 */
struct devfreq_pm_qos_table {
	unsigned long freq; /* 0 if this is the last element */
	int qos_value;
};

/**
 * struct devfreq_dev_profile - Devfreq's user device profile
 * @initial_freq:	The operating frequency when devfreq_add_device() is
 *			called.
 * @polling_ms:		The polling interval in ms. 0 disables polling.
 * @qos_type:		QoS type (defined in pm_qos_params.h)
 *			0 (PM_QOS_RESERVED) if not used.
 * @qos_list:		Array of QoS requirements ending with .freq = 0
 *			NULL if not used. It should be either NULL or
 *			have a length > 1 with a first element effective.
 * @target:		The device should set its operating frequency at
 *			freq or lowest-upper-than-freq value. If freq is
 *			higher than any operable frequency, set maximum.
 *			Before returning, target function should set
 *			freq at the current frequency.
 *			The "flags" parameter's possible values are
 *			explained above with "DEVFREQ_FLAG_*" macros.
 * @get_dev_status:	The device should provide the current performance
 *			status to devfreq, which is used by governors.
 * @get_cur_freq:	The device should provide the current frequency
 *			at which it is operating.
 * @exit:		An optional callback that is called when devfreq
 *			is removing the devfreq object due to error or
 *			from devfreq_remove_device() call. If the user
 *			has registered devfreq->nb at a notifier-head,
 *			this is the time to unregister it.
 * @freq_table:	Optional list of frequencies to support statistics.
 * @max_state:	The size of freq_table.
 *
 * Note that the array of qos_list should be sorted by freq
 * in the ascending order.
 */
struct devfreq_dev_profile {
	char *name;
	unsigned long initial_freq;
	unsigned int polling_ms;
	int min_qos_type;
	int max_qos_type;
	struct pm_qos_request qos_req_min;
	struct pm_qos_request qos_req_max;

	int (*target)(struct device *dev, unsigned long *freq, u32 flags);
	int (*get_dev_status)(struct device *dev,
			      struct devfreq_dev_status *stat);
	int (*get_cur_freq)(struct device *dev, unsigned long *freq);
	void (*exit)(struct device *dev);

	unsigned int *freq_table;
	unsigned int max_state;
};

/**
 * struct devfreq_governor - Devfreq policy governor
 * @node:		list node - contains registered devfreq governors
 * @name:		Governor's name
 * @get_target_freq:	Returns desired operating frequency for the device.
 *			Basically, get_target_freq will run
 *			devfreq_dev_profile.get_dev_status() to get the
 *			status of the device (load = busy_time / total_time).
 *			If no_central_polling is set, this callback is called
 *			only with update_devfreq() notified by OPP.
 * @event_handler:      Callback for devfreq core framework to notify events
 *                      to governors. Events include per device governor
 *                      init and exit, opp changes out of devfreq, suspend
 *                      and resume of per device devfreq during device idle.
 *
 * Note that the callbacks are called with devfreq->lock locked by devfreq.
 */
struct devfreq_governor {
	struct list_head node;

	const char name[DEVFREQ_NAME_LEN];
	int (*get_target_freq)(struct devfreq *this, unsigned long *freq);
	int (*event_handler)(struct devfreq *devfreq,
				unsigned int event, void *data);
};

/**
 * struct devfreq - Device devfreq structure
 * @node:	list node - contains the devices with devfreq that have been
 *		registered.
 * @lock:	a mutex to protect accessing devfreq.
 * @dev:	device registered by devfreq class. dev.parent is the device
 *		using devfreq.
 * @profile:	device-specific devfreq profile
 * @governor:	method how to choose frequency based on the usage.
 * @governor_name:	devfreq governor name for use with this devfreq
 * @nb:		notifier block used to notify devfreq object that it should
 *		reevaluate operable frequencies. Devfreq users may use
 *		devfreq.nb to the corresponding register notifier call chain.
 * @work:	delayed work for load monitoring.
 * @previous_freq:	previously configured frequency value.
 * @data:	Private data of the governor. The devfreq framework does not
 *		touch this.
 * @min_freq:	Limit minimum frequency requested by user (0: none)
 * @max_freq:	Limit maximum frequency requested by user (0: none)
 * @qos_min_nb:	notifier block used to notify pm qos_min requests
 * @qos_max_nb:	notifier block used to notify pm qos_max requests
 * @qos_min_freq:	Limit minimum frequency requested by QoS
 * @qos_max_freq:	Limit maximum frequency requested by QoS
 * @stop_polling:	devfreq polling status of a device.
 * @total_trans:	Number of devfreq transitions
 * @trans_table:	Statistics of devfreq transitions
 * @time_in_state:	Statistics of devfreq states
 * @last_stat_updated:	The last time stat updated
 *
 * This structure stores the devfreq information for a give device.
 *
 * Note that when a governor accesses entries in struct devfreq in its
 * functions except for the context of callbacks defined in struct
 * devfreq_governor, the governor should protect its access with the
 * struct mutex lock in struct devfreq. A governor may use this mutex
 * to protect its own private data in void *data as well.
 */
struct devfreq {
	struct list_head node;

	struct mutex lock;
	struct device dev;
	struct devfreq_dev_profile *profile;
	const struct devfreq_governor *governor;
	char governor_name[DEVFREQ_NAME_LEN];
	struct notifier_block nb;
	struct delayed_work work;

	unsigned long previous_freq;

	void *data; /* private data for governors */

	unsigned long min_freq;
	unsigned long max_freq;
	struct notifier_block qos_min_nb;
	struct notifier_block qos_max_nb;
	unsigned long qos_min_freq;
	unsigned long qos_max_freq;

	bool stop_polling;

	/* information for device frequency transition */
	unsigned int total_trans;
	unsigned int *trans_table;
	unsigned long *time_in_state;
	unsigned long last_stat_updated;
};

/* frequency table helpers */
enum devfreq_device_id {
	DEVFREQ_DDR = 0,
	DEVFREQ_VPU_BASE,
	DEVFREQ_VPU_0 = DEVFREQ_VPU_BASE,
	DEVFREQ_VPU_1,
	DEVFREQ_GPU_3D,
	DEVFREQ_GPU_SH,
	DEVFREQ_GPU_2D,
	DEVFREQ_MAX_ID,
};

#define DEVFREQ_TABLE_END	(~1)
struct devfreq_frequency_table {
	unsigned int index;
	unsigned int mode;
	unsigned int frequency;
};

struct devfreq_dev_freq_info {
	struct devfreq_frequency_table *tbl;
};

void devfreq_frequency_table_register(struct devfreq_frequency_table *table,
				unsigned int dev_id);

struct devfreq_frequency_table *devfreq_frequency_get_table(
		unsigned int dev_id);

#if defined(CONFIG_PM_DEVFREQ)
extern struct devfreq *devfreq_add_device(struct device *dev,
				  struct devfreq_dev_profile *profile,
				  const char *governor_name,
				  void *data);
extern int devfreq_remove_device(struct devfreq *devfreq);

/* Supposed to be called by PM_SLEEP/PM_RUNTIME callbacks */
extern int devfreq_suspend_device(struct devfreq *devfreq);
extern int devfreq_resume_device(struct devfreq *devfreq);

/* Helper functions for devfreq user device driver with OPP. */
extern struct dev_pm_opp *devfreq_recommended_opp(struct device *dev,
					   unsigned long *freq, u32 flags);
extern int devfreq_register_opp_notifier(struct device *dev,
					 struct devfreq *devfreq);
extern int devfreq_unregister_opp_notifier(struct device *dev,
					   struct devfreq *devfreq);

#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
/**
 * struct devfreq_simple_ondemand_data - void *data fed to struct devfreq
 *	and devfreq_add_device
 * @upthreshold:	If the load is over this value, the frequency jumps.
 *			Specify 0 to use the default. Valid value = 0 to 100.
 * @downdifferential:	If the load is under upthreshold - downdifferential,
 *			the governor may consider slowing the frequency down.
 *			Specify 0 to use the default. Valid value = 0 to 100.
 *			downdifferential < upthreshold must hold.
 *
 * If the fed devfreq_simple_ondemand_data pointer is NULL to the governor,
 * the governor uses the default values.
 */
struct devfreq_simple_ondemand_data {
	unsigned int upthreshold;
	unsigned int downdifferential;
};
#endif

#if IS_ENABLED(CONFIG_DEVFREQ_GOV_THROUGHPUT)
struct throughput_threshold {
	unsigned int up;
	unsigned int down;
};

struct devfreq_throughput_data {
	unsigned int upthreshold;
	unsigned int downdifferential;
	u32 table_len;
	u32 *freq_table;        /* unit Khz */
	struct throughput_threshold *throughput_table;
};
#endif

/* calculate workload according to busy and total time, unit percent */
static inline unsigned int cal_workload(unsigned long busy_time,
	unsigned long total_time)
{
	u64 tmp0, tmp1;

	if (!total_time || !busy_time)
		return 0;
	tmp0 = busy_time * 100;
	tmp1 = div_u64(tmp0, total_time);
	return (unsigned int)tmp1;
}

#else /* !CONFIG_PM_DEVFREQ */
static inline struct devfreq *devfreq_add_device(struct device *dev,
					  struct devfreq_dev_profile *profile,
					  const char *governor_name,
					  void *data)
{
	return NULL;
}

static inline int devfreq_remove_device(struct devfreq *devfreq)
{
	return 0;
}

static inline int devfreq_suspend_device(struct devfreq *devfreq)
{
	return 0;
}

static inline int devfreq_resume_device(struct devfreq *devfreq)
{
	return 0;
}

static inline struct dev_pm_opp *devfreq_recommended_opp(struct device *dev,
					   unsigned long *freq, u32 flags)
{
	return ERR_PTR(-EINVAL);
}

static inline int devfreq_register_opp_notifier(struct device *dev,
					 struct devfreq *devfreq)
{
	return -EINVAL;
}

static inline int devfreq_unregister_opp_notifier(struct device *dev,
					   struct devfreq *devfreq)
{
	return -EINVAL;
}

#endif /* CONFIG_PM_DEVFREQ */

#endif /* __LINUX_DEVFREQ_H__ */
