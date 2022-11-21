/*
 *  Copyright (C) 2018, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "ssp.h"
#include "ssp_platform.h"
#include "ssp_dump.h"
#include "ssp_iio.h"

#define SENSORHUB_DUMP_NOTI_EVENT               0xFC

char *sensorhub_dump;
int sensorhub_dump_size;
int sensorhub_dump_type;
int sensorhub_dump_count;

static ssize_t shub_dump_read(struct file *file, struct kobject *kobj, struct bin_attribute *battr, char *buf,
			      loff_t off, size_t size)
{
	memcpy_fromio(buf, battr->private + off, size);
	return size;
}

BIN_ATTR_RO(shub_dump, 0);

struct bin_attribute *ssp_dump_bin_attrs[] = {
	&bin_attr_shub_dump,
};

static int create_dump_bin_file(struct ssp_data *data)
{
	int i, ret;

	sensorhub_dump = (char *)kvzalloc(sensorhub_dump_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(sensorhub_dump)) {
		ssp_infof("memory alloc failed");
		return -ENOMEM;
	}

	ssp_infof("dump size %d", sensorhub_dump_size);

	bin_attr_shub_dump.size = sensorhub_dump_size;
	bin_attr_shub_dump.private = sensorhub_dump;

	for (i = 0; i < ARRAY_SIZE(ssp_dump_bin_attrs); i++) {
		struct bin_attribute *battr = ssp_dump_bin_attrs[i];

		ret = device_create_bin_file(data->mcu_device, battr);
		if (ret < 0) {
			ssp_errf("Failed to create file: %s %d", battr->attr.name, ret);
			break;
		}
	}

	if (ret < 0) {
		kvfree(sensorhub_dump);
		sensorhub_dump_size = 0;
	}

	return ret;
}

void write_ssp_dump_file(struct ssp_data *data, char *dump, int dumpsize, int type, int count)
{
	int ret;

	if (dump == NULL) {
		ssp_errf("dump is NULL");
		return;
	} else if (sensorhub_dump_size == 0) {
		sensorhub_dump_size = dumpsize;
		ret = create_dump_bin_file(data);
		if (ret < 0)
			return;
	}
	memcpy_fromio(sensorhub_dump, dump, sensorhub_dump_size);
	sensorhub_dump_type = type;
	sensorhub_dump_count = count;
	queue_work(data->debug_wq, &data->work_dump);
}


void report_dump_event_task(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct work_struct *)work, struct ssp_data, work_dump);
	char buffer[3];

	buffer[0] = SENSORHUB_DUMP_NOTI_EVENT;
	buffer[1] = sensorhub_dump_type;
	buffer[2] = sensorhub_dump_count;

	ssp_infof("type %d, count %d", buffer[1], buffer[2]);
	report_sensorhub_data(data, buffer);
}

void initialize_ssp_dump(struct ssp_data *data)
{
	ssp_infof();
	INIT_WORK(&data->work_dump, report_dump_event_task);
}

void remove_ssp_dump(struct ssp_data *data)
{
	int i;
	cancel_work_sync(&data->work_dump);
	if (!ZERO_OR_NULL_PTR(sensorhub_dump)) {
		kvfree(sensorhub_dump);

		for (i = 0; i < ARRAY_SIZE(ssp_dump_bin_attrs); i++)
			device_remove_bin_file(data->mcu_device, ssp_dump_bin_attrs[i]);
	}

}
