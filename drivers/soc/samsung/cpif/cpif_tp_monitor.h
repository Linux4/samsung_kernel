/*
 * Copyright (C) 2019, Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CPIF_TP_MONITOR_H__
#define __CPIF_TP_MONITOR_H__

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/pm_qos.h>

#define MAX_THRESHOLD	2
#define MAX_VALUES	3
#define MAX_RPS_STRING	8

struct tpmon_data {
	struct cpif_tpmon *tpmon;

	struct list_head data_node;

	bool enable;
	u32 num_threshold;
	u32 threshold[MAX_THRESHOLD];
	u32 num_values;
	u32 curr_value;
	u32 values[MAX_VALUES];

	void (*set_data)(struct tpmon_data *);
};

struct cpif_tpmon {
	struct link_device *ld;

	spinlock_t lock;

	struct list_head data_list;

	struct hrtimer timer;
	u32 interval_sec;

	struct workqueue_struct *qos_wq;
	struct delayed_work qos_dwork;

	unsigned long rx_bytes;
	unsigned long rx_mega_bps;

#if defined(CONFIG_RPS)
	struct tpmon_data rps_data;
	struct list_head net_node_list;
#endif
#if defined(CONFIG_MODEM_IF_NET_GRO)
	struct tpmon_data gro_data;
#endif
	struct tpmon_data irq_affinity_data;

	struct tpmon_data mif_data;
	struct pm_qos_request qos_req_mif;

#if defined(CONFIG_LINK_DEVICE_PCIE)
	struct tpmon_data pci_low_power_data;
#endif
};

#if defined(CONFIG_CPIF_TP_MONITOR)
extern int tpmon_create(struct platform_device *pdev, struct link_device *ld);
extern int tpmon_start(u32 interval_sec);
extern int tpmon_stop(void);
extern void tpmon_add_rx_bytes(unsigned long bytes);
extern void tpmon_add_net_node(struct list_head *node);
#else
static inline int tpmon_create(struct platform_device *pdev, struct link_device *ld) { return 0; }
static inline int tpmon_start(u32 interval_sec) { return 0; }
static inline int tpmon_stop(void) { return 0; }
static inline void tpmon_add_rx_bytes(unsigned long bytes) { return; }
static inline void tpmon_add_net_node(struct list_head *node) { return; }
#endif

#endif /* __CPIF_TP_MONITOR_H__ */
