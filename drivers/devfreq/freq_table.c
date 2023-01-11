/*
 * linux/drivers/devfreq/freq_table.c
 *
 *  Copyright (C) 2014 Marvell International Ltd.
 *  All rights reserved.
 *
 *  2012-01-13	Qiming Wu<wuqm@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/devfreq.h>

static struct devfreq_dev_freq_info devices_freq_info[DEVFREQ_MAX_ID];

void devfreq_frequency_table_register(struct devfreq_frequency_table *table,
				      unsigned int dev_id)
{
	if (dev_id >= DEVFREQ_MAX_ID) {
		pr_err("Invalid dev_id %d for devfreq! The max id = %d\n",
				dev_id, DEVFREQ_MAX_ID - 1);
		BUG_ON(1);
		return;
	}

	if (devices_freq_info[dev_id].tbl != NULL)
		pr_warn("devfreq freq table %d will be overridden!\n",
				dev_id);

	devices_freq_info[dev_id].tbl = table;
}

struct devfreq_frequency_table *devfreq_frequency_get_table(unsigned int dev_id)
{
	if (dev_id >= DEVFREQ_MAX_ID) {
		pr_err("Invalid dev_id for devfreq! The max id = %d\n",
				DEVFREQ_MAX_ID - 1);
		BUG_ON(1);
		return NULL;
	}

	return devices_freq_info[dev_id].tbl;
}
