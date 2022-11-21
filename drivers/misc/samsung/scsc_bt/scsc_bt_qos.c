/****************************************************************************
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 * Bluetooth Quality of Service
 *
 ****************************************************************************/
#include <linux/types.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/moduleparam.h>
#include <asm/io.h>
#include <asm/termios.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos_pm_qos.h>
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#include <scsc/scsc_mx.h>
#include <scsc/scsc_mifram.h>
#include <scsc/scsc_logring.h>

#include "scsc_bt_priv.h"

#ifdef CONFIG_SCSC_QOS

#define SCSC_BT_QOS_LOW_LEVEL 1
#define SCSC_BT_QOS_MED_LEVEL 3
#define SCSC_BT_QOS_HIGH_LEVEL 6

static uint32_t scsc_bt_qos_low_level = SCSC_BT_QOS_LOW_LEVEL;
static uint32_t scsc_bt_qos_medium_level = SCSC_BT_QOS_MED_LEVEL;
static uint32_t scsc_bt_qos_high_level = SCSC_BT_QOS_HIGH_LEVEL;

module_param(scsc_bt_qos_low_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_low_level,
		 "Number of outstanding packets which triggers QoS low level");
module_param(scsc_bt_qos_medium_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_medium_level,
		 "Number of outstanding packets which triggers QoS medium level");
module_param(scsc_bt_qos_high_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_high_level,
		 "Number of outstanding packets which triggers QoS high level");

static struct scsc_qos_service qos_service;

static DEFINE_MUTEX(bt_qos_mutex);

static void scsc_bt_qos_work(struct work_struct *data)
{
	mutex_lock(&bt_qos_mutex);
	if (qos_service.enabled) {
		SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth QoS update (State: %d)\n", (uint8_t)qos_service.current_state);
		scsc_service_pm_qos_update_request(bt_service.service, qos_service.current_state);
	}
	mutex_unlock(&bt_qos_mutex);
}

void scsc_bt_qos_update(uint32_t number_of_outstanding_hci_events,
			uint32_t number_of_outstanding_acl_packets)
{
	if (qos_service.enabled) {
		enum scsc_qos_config next_state = SCSC_QOS_DISABLED;
		u32 next_level = max(number_of_outstanding_acl_packets, number_of_outstanding_hci_events);

		/* Calculate next PM state */
		if (next_level >= scsc_bt_qos_high_level)
			next_state = SCSC_QOS_MAX;
		else if (next_level >= scsc_bt_qos_medium_level)
			next_state = SCSC_QOS_MED;
		else if (next_level >= scsc_bt_qos_low_level)
			next_state = SCSC_QOS_MIN;

		/* Update PM QoS settings if QoS should increase or disable */
		if ((next_state > qos_service.current_state) ||
		    (qos_service.current_state > SCSC_QOS_DISABLED && next_state == SCSC_QOS_DISABLED))
		{
			qos_service.current_state = next_state;
			schedule_work(&qos_service.work_queue);
		}
	}
}

void scsc_bt_qos_service_stop(void)
{
	/* Ensure no crossing of work and stop */
	mutex_lock(&bt_qos_mutex);
	qos_service.current_state = SCSC_QOS_DISABLED;
	if (qos_service.enabled) {
		scsc_service_pm_qos_remove_request(bt_service.service);
		qos_service.enabled = false;
	}
	mutex_unlock(&bt_qos_mutex);
}

void scsc_bt_qos_service_start(void)
{
	if (!scsc_service_pm_qos_add_request(bt_service.service, SCSC_QOS_DISABLED))
		qos_service.enabled = true;
	else
		qos_service.enabled = false;

	INIT_WORK(&qos_service.work_queue, scsc_bt_qos_work);
}

void scsc_bt_qos_service_init(void)
{
	qos_service.enabled = false;
	qos_service.current_state = SCSC_QOS_DISABLED;
}

#endif /* CONFIG_SCSC_QOS */
