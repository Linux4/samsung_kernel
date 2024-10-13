/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Dong Xiang <dong.xiang@spreadtrum.com>
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

#ifndef __SPRDWL_WORK_H__
#define __SPRDWL_WORK_H__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>

struct sprdwl_work {
	struct list_head list;
	struct sprdwl_vif *vif;
#define SPRDWL_WORK_NONE	0
#define SPRDWL_WORK_REG_MGMT	1
#define SPRDWL_WORK_DEAUTH	2
#define SPRDWL_WORK_DISASSOC	3
#define SPRDWL_WORK_MC_FILTER	4
#define SPRDWL_WORK_NOTIFY_IP	5
#define SPRDWL_WORK_DATA2MGMT	6
#define SPRDWL_WORK_DEL_STA	7
	u8 id;
	u32 len;
	u8 data[0];
};

struct sprdwl_reg_mgmt {
	u16 type;
	bool reg;
};

struct sprdwl_data2mgmt {
	struct sk_buff *skb;
	struct net_device *ndev;
};

struct sprdwl_work *sprdwl_alloc_work(int len);
void sprdwl_queue_work(struct sprdwl_priv *priv,
		       struct sprdwl_work *sprdwl_work);
void sprdwl_cancle_work(struct sprdwl_priv *priv, struct sprdwl_vif *vif);
void sprdwl_init_work(struct sprdwl_priv *priv);
void sprdwl_deinit_work(struct sprdwl_priv *priv);

#endif
