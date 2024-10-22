/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Sensitive Data Protection
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <ddar/dek_common.h>
#include <linux/string.h>

#ifdef CONFIG_DDAR_KEY_DUMP
static int kek_dump = 0;

static ssize_t dek_show_key_dump(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kek_dump);
}

static ssize_t dek_set_key_dump(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int flag = simple_strtoul(buf, NULL, 10);

	kek_dump = flag;

	return strlen(buf);
}

static DEVICE_ATTR(key_dump, 0664, dek_show_key_dump, dek_set_key_dump);

int dek_create_sysfs_key_dump(struct device *d)
{
	int error;

	if ((error = device_create_file(d, &dev_attr_key_dump)))
		return error;

	return 0;
}

int get_sdp_sysfs_key_dump(void)
{
	return kek_dump;
}
#else
int dek_create_sysfs_key_dump(struct device *d)
{
	printk("key_dump feature not available");

	return 0;
}

int get_sdp_sysfs_key_dump(void)
{
	printk("key_dump feature not available");

	return 0;
}
#endif
