/* sec_hw_param.c
 *
 * Copyright (C) 2016 Samsung Electronics
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
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/sec_class.h>
#include <linux/of.h>

extern unsigned int system_rev;
extern char* get_ddr_vendor_name(void);

#define MAX_LEN_STR 700
#define NUM_PARAM0 3
#define MAX_LEN_PARAM0 (NUM_PARAM0*4+(NUM_PARAM0-1)+1)
static u32 hw_param0[NUM_PARAM0];
static char hw_param0_str[MAX_LEN_PARAM0];

static ssize_t show_ddr_info(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"DDRV\":\"%s\",", get_ddr_vendor_name());
	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"C2D\":\"\",");
	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"D2D\":\"\"");

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\n");

	return info_size;
}

static DEVICE_ATTR(ddr_info, 0440, show_ddr_info, NULL);

static char* get_param0(void)
{
	struct device_node *np = of_find_node_by_path("/soc/sec_hw_param");
	u32 val;
	int num_param = 0, offset = 0;
	struct property *prop;
	const __be32 *p;

	if (!np) {
		pr_err("No sec_hw_param found\n");
		return NULL;
	}

	memset(hw_param0_str, 0, MAX_LEN_PARAM0);

	of_property_for_each_u32(np, "param0", prop, p, val) {
		hw_param0[num_param++] = val;

		if (num_param > 1)
			offset += snprintf(hw_param0_str + offset,
				MAX_LEN_PARAM0 - offset, ",");

		offset += snprintf(hw_param0_str + offset, MAX_LEN_PARAM0 - offset,
			"%u", val);

		if (num_param >= NUM_PARAM0)
			break;
	}

	return hw_param0_str;
}

static ssize_t show_ap_info(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"HW_REV\":\"%d\"", system_rev);

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"LOT_ID\":\"\"");

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"ASV_CL0\":\"\"");

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"ASV_CL1\":\"\"");

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"ASV_MIF\":\"\"");

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"IDS_CL1\":\"\"");

	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"PARAM0\":\"%s\"", get_param0());

	info_size += sprintf((char*)(buf+info_size), "\n");

	return info_size;
}

static DEVICE_ATTR(ap_info, 0440, show_ap_info, NULL);

static int __init sec_hw_param_init(void)
{
	struct device* sec_hw_param_dev;

	sec_hw_param_dev = device_create(sec_class, NULL, 0, NULL, "sec_hw_param");

	if (IS_ERR(sec_hw_param_dev)) {
		pr_err("%s: Failed to create devce\n",__func__);
		goto out;
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_ap_info) < 0) {
		pr_err("%s: could not create sysfs node\n", __func__);
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_ddr_info) < 0) {
		pr_err("%s: could not create sysfs node\n", __func__);
	}
out:
	return 0;
}
device_initcall(sec_hw_param_init);

