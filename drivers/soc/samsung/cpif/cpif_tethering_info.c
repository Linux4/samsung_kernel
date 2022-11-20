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
#include "cpif_tethering_info.h"
#include "modem_v1.h"

static struct tethering_dev g_upstream_dev;

int cpif_tethering_init(void)
{
	spin_lock_init(&g_upstream_dev.lock);
	return 0;
}

void cpif_tethering_upstream_dev_get(char *dev_name)
{
	unsigned long flags;

	spin_lock_irqsave(&g_upstream_dev.lock, flags);

	strcpy(dev_name, g_upstream_dev.dev_name);

	spin_unlock_irqrestore(&g_upstream_dev.lock, flags);
}

void cpif_tethering_upstream_dev_set(char *dev_name)
{
	unsigned long flags;

	spin_lock_irqsave(&g_upstream_dev.lock, flags);

	strcpy(g_upstream_dev.dev_name, dev_name);

	spin_unlock_irqrestore(&g_upstream_dev.lock, flags);
}

bool is_tethering_upstream_device(char *dev_name)
{
	return (!strcmp(dev_name, g_upstream_dev.dev_name)) ? true : false;
}
