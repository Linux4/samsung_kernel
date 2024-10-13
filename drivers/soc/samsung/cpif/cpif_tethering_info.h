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

#ifndef __CPIF_TETHERING_INFO_H__
#define __CPIF_TETHERING_INFO_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <uapi/linux/if.h>
#include <linux/skbuff.h>

#define NETDEV_INTERFACE_NAME_LENGTH	IFNAMSIZ /* known to be 16 */
struct tethering_dev {
	char dev_name[NETDEV_INTERFACE_NAME_LENGTH];
	spinlock_t lock;
};

int cpif_tethering_init(void);
void cpif_tethering_upstream_dev_get(char *dev_name);
void cpif_tethering_upstream_dev_set(char *dev_name);
bool is_tethering_upstream_device(char *dev_name);

#endif /* __CPIF_TETHERING_INFO_H__ */
