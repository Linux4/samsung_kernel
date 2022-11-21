/* sec_cp830.c
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sec_sysfs.h>

extern int cal_get_cp_cpu_freq(void);

static ssize_t cp830_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, sizeof(buf), "%d \n", cal_get_cp_cpu_freq());
}

static DEVICE_ATTR(cp830_id, 0440, cp830_id_show, NULL);

static int __init sec_cp830_id_init(void)
{
	struct device *dev;

	// sysfs
	dev = sec_device_create(NULL, "cp830");
	BUG_ON(!dev);
	if (IS_ERR(dev))
		pr_err("%s:Failed to create devce\n",__func__);

	if (device_create_file(dev, &dev_attr_cp830_id) < 0)
		pr_err("%s: Failed to create device file\n",__func__);

	return 0;
}

module_init(sec_cp830_id_init);
