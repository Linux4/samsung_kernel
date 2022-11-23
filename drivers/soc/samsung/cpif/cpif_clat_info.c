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
#include "cpif_clat_info.h"
#ifdef CONFIG_HW_FORWARD
#include <soc/samsung/hw_forward.h>
#endif
#include "modem_v1.h"

static struct clat_info g_clat_info;

int cpif_init_clat_info(void)
{
	spin_lock_init(&g_clat_info.lock);
	return 0;
}

void cpif_get_plat_prefix(u32 id, struct in6_addr *paddr)
{
	unsigned long flags;

	spin_lock_irqsave(&g_clat_info.lock, flags);

	paddr->s6_addr32[0] = g_clat_info.plat_prefix[id].s6_addr32[0];
	paddr->s6_addr32[1] = g_clat_info.plat_prefix[id].s6_addr32[1];
	paddr->s6_addr32[2] = g_clat_info.plat_prefix[id].s6_addr32[2];

	spin_unlock_irqrestore(&g_clat_info.lock, flags);

#ifdef CONFIG_HW_FORWARD
	dit_get_plat_prefix(id, paddr);
#endif

	mif_info("get plat_prefix (id %d): %pI6\n", id, paddr);
}

void cpif_set_plat_prefix(u32 id, struct in6_addr addr)
{
	unsigned long flags;

	spin_lock_irqsave(&g_clat_info.lock, flags);

	g_clat_info.plat_prefix[id].s6_addr32[0] = addr.s6_addr32[0];
	g_clat_info.plat_prefix[id].s6_addr32[1] = addr.s6_addr32[1];
	g_clat_info.plat_prefix[id].s6_addr32[2] = addr.s6_addr32[2];

	spin_unlock_irqrestore(&g_clat_info.lock, flags);

#ifdef CONFIG_HW_FORWARD
	dit_set_plat_prefix(id, addr);
#endif

	mif_info("set plat_prefix (id %d): %pI6\n", id, &g_clat_info.plat_prefix[id]);
}

void cpif_get_clat_addr(u32 id, struct in6_addr *paddr)
{
	unsigned long flags;

	spin_lock_irqsave(&g_clat_info.lock, flags);

	paddr->s6_addr32[0] = g_clat_info.clat_addr[id].s6_addr32[0];
	paddr->s6_addr32[1] = g_clat_info.clat_addr[id].s6_addr32[1];
	paddr->s6_addr32[2] = g_clat_info.clat_addr[id].s6_addr32[2];
	paddr->s6_addr32[3] = g_clat_info.clat_addr[id].s6_addr32[3];

	spin_unlock_irqrestore(&g_clat_info.lock, flags);

#ifdef CONFIG_HW_FORWARD
	dit_get_clat_addr(id, paddr);
#endif

	mif_info("get clat address (id %d): %pI6\n", id, paddr);
}

void cpif_set_clat_addr(u32 id, struct in6_addr addr)
{
	unsigned long flags;

	spin_lock_irqsave(&g_clat_info.lock, flags);

	g_clat_info.clat_addr[id].s6_addr32[0] = addr.s6_addr32[0];
	g_clat_info.clat_addr[id].s6_addr32[1] = addr.s6_addr32[1];
	g_clat_info.clat_addr[id].s6_addr32[2] = addr.s6_addr32[2];
	g_clat_info.clat_addr[id].s6_addr32[3] = addr.s6_addr32[3];

	spin_unlock_irqrestore(&g_clat_info.lock, flags);

#ifdef CONFIG_HW_FORWARD
	dit_set_clat_addr(id, addr);
#endif

	mif_info("set clat address (id %d): %pI6\n", id, &g_clat_info.clat_addr[id]);
}

void cpif_get_v4_filter(u32 id, u32 *paddr)
{
	unsigned long flags;

	spin_lock_irqsave(&g_clat_info.lock, flags);

	*paddr = g_clat_info.clat_v4_filter[id];

	spin_unlock_irqrestore(&g_clat_info.lock, flags);

#ifdef CONFIG_HW_FORWARD
	dit_get_v4_filter(id, paddr);
#endif

	mif_info("get clat v4 address (id %d): %pI4\n", id, paddr);
}

void cpif_set_v4_filter(u32 id, u32 addr)
{
	unsigned long flags;

	spin_lock_irqsave(&g_clat_info.lock, flags);

	g_clat_info.clat_v4_filter[id] = addr;

	spin_unlock_irqrestore(&g_clat_info.lock, flags);

#ifdef CONFIG_HW_FORWARD
	dit_set_v4_filter(id, addr);
#endif

	mif_info("get clat v4 address (id %d): %pI4\n", id,
			&g_clat_info.clat_v4_filter[id]);
}

bool is_heading_toward_clat(struct sk_buff *skb)
{
	struct ipv6hdr *ip_header = ipv6_hdr(skb);
	unsigned long flags;
	int i = 0;

	if (ip_header->version == 4)
		return false;

	spin_lock_irqsave(&g_clat_info.lock, flags);

	for (i = 0; i < NUM_CLAT_ADDR; i++) {
		if (memcmp(&g_clat_info.clat_addr[i], &ip_header->daddr, 16) == 0) {
			spin_unlock_irqrestore(&g_clat_info.lock, flags);
			return true;
		}
	}

	spin_unlock_irqrestore(&g_clat_info.lock, flags);

	return false;
}
