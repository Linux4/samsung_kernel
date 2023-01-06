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

#ifndef __CPIF_CLAT_INFO_H__
#define __CPIF_CLAT_INFO_H__

#include <linux/types.h>
#include <net/ipv6.h>
#include <linux/spinlock.h>

#define NUM_CLAT_ADDR	2 /* usually 2 */
struct clat_info {
	struct in6_addr clat_addr[NUM_CLAT_ADDR];
	struct in6_addr plat_prefix[NUM_CLAT_ADDR];
	u32 clat_v4_filter[NUM_CLAT_ADDR];

	spinlock_t lock;
};

int cpif_init_clat_info(void);
void cpif_get_plat_prefix(u32 id, struct in6_addr *paddr);
void cpif_set_plat_prefix(u32 id, struct in6_addr addr);
void cpif_get_clat_addr(u32 id, struct in6_addr *paddr);
void cpif_set_clat_addr(u32 id, struct in6_addr addr);
void cpif_get_v4_filter(u32 id, u32 *paddr);
void cpif_set_v4_filter(u32 id, u32 addr);
bool is_heading_toward_clat(struct sk_buff *skb);

#endif /* __CPIF_CLAT_INFO_H__ */
