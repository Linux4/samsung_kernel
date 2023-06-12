/*
 * Copyright (C) 2013-2016 Samsung Electronics, Inc.
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

#ifndef __TZ_IWNOTIFY_H__
#define __TZ_IWNOTIFY_H__

#include <linux/notifier.h>

#define TZ_IWNOTIFY_EVENT_CNT	4
#define TZ_IWNOTIFY_EVENT_MASK	((1 << TZ_IWNOTIFY_EVENT_CNT) - 1)

void tz_iwnotify_call_chains(unsigned int event_mask);
int tz_iwnotify_chain_register(unsigned int event, struct notifier_block *nb);
int tz_iwnotify_chain_unregister(unsigned int event, struct notifier_block *nb);

#endif /* __TZ_IWNOTIFY_H__ */
