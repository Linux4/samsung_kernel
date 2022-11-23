/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRD_DVFS_GSP_H__
#define __SPRD_DVFS_GSP_H__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>

#include "sprd_dvfs_apsys.h"

typedef enum {
	VOLT55 = 0, //0.55v
	VOLT60 = 1, //0.60v
	VOLT65 = 2, //0.65v
	VOLT70 = 3, //0.70v
	VOLT75 = 4, //0.75v
} voltage_level;

typedef enum {
	GSP_CLK_INDEX_256M = 0,
	GSP_CLK_INDEX_307M2,
	GSP_CLK_INDEX_384M,
	GSP_CLK_INDEX_512M,
} clock_level;

typedef enum {
	GSP_CLK256M = 256000000,
	GSP_CLK307M2 = 307200000,
	GSP_CLK384M = 384000000,
	GSP_CLK512M = 512000000,
} clock_rate;

struct gsp_dvfs {
	int dvfs_enable;
	struct device dev;

	struct devfreq *devfreq;
	struct opp_table *opp_table;
	struct devfreq_event_dev *edev;
	struct notifier_block gsp_dvfs_nb;

	u32 work_freq;
	u32 idle_freq;
	set_freq_type freq_type;

	struct ip_dvfs_coffe dvfs_coffe;
	struct ip_dvfs_status dvfs_status;
	struct gsp_dvfs_ops *dvfs_ops;
};

struct gsp_dvfs_ops {
	/* initialization interface */
	int (*parse_dt)(struct gsp_dvfs *gsp, struct device_node *np);
	int (*dvfs_init)(struct gsp_dvfs *gsp);
	void (*hw_dfs_en)(bool dfs_en);

	/* work-idle dvfs index ops */
	void  (*set_work_index)(int index);
	int  (*get_work_index)(void);
	void  (*set_idle_index)(int index);
	int  (*get_idle_index)(void);

	/* work-idle dvfs freq ops */
	void (*set_work_freq)(u32 freq);
	u32 (*get_work_freq)(void);
	void (*set_idle_freq)(u32 freq);
	u32 (*get_idle_freq)(void);

	/* work-idle dvfs map ops */
	int  (*get_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*set_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*get_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*set_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*get_dvfs_status)(struct ip_dvfs_status *dvfs_status);

	/* coffe setting ops */
	void (*set_gfree_wait_delay)(u32 para);
	void (*set_freq_upd_en_byp)(bool enable);
	void (*set_freq_upd_delay_en)(bool enable);
	void (*set_freq_upd_hdsk_en)(bool enable);
	void (*set_dvfs_swtrig_en)(bool enable);
};

extern struct list_head gsp_dvfs_head;
extern struct atomic_notifier_head gsp_dvfs_chain;

#if IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
int gsp_dvfs_notifier_call_chain(void *data);
#else
static inline int gsp_dvfs_notifier_call_chain(void *data)
{
	return 0;
}
#endif

#define gsp_dvfs_ops_register(entry) \
	dvfs_ops_register(entry, &gsp_dvfs_head)

#define gsp_dvfs_ops_attach(str) \
	dvfs_ops_attach(str, &gsp_dvfs_head)

#endif /* __SPRD_DVFS_GSP_H__ */
