/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/events.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/types.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "../utility/shub_utility.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "shub_kfifo_buf.h"

#define SCONTEXT_DATA_LEN       56
#define SCONTEXT_HEADER_LEN     8

#define IIO_CHANNEL            -1
#define IIO_SCAN_INDEX          3
#define IIO_SIGN               's'
#define IIO_SHIFT               0

struct shub_iio_device {
	int type;
	struct iio_chan_spec iio_channel;
	struct iio_dev* indio_dev;
};

static struct shub_iio_device *iio_list[SENSOR_TYPE_LEGACY_MAX];

static struct iio_dev* get_iio_device(int type)
{
	if (type < 0 || type >= SENSOR_TYPE_LEGACY_MAX)
		return NULL;

	return iio_list[type] ? iio_list[type]->indio_dev : NULL;
}

static int shub_preenable(struct iio_dev *indio_dev)
{
	return 0;
}

static int shub_predisable(struct iio_dev *indio_dev)
{
	return 0;
}

static const struct iio_buffer_setup_ops shub_iio_ring_setup_ops = {
	.preenable = &shub_preenable,
	.predisable = &shub_predisable,
};

static int shub_iio_configure_ring(struct iio_dev *indio_dev, int bytes)
{
	struct iio_buffer *ring;

	ring = shub_iio_kfifo_allocate();
	if (!ring) {
		return -ENOMEM;
	}

	ring->scan_timestamp = true;
	ring->bytes_per_datum = bytes;
	indio_dev->buffer = ring;
	indio_dev->setup_ops = &shub_iio_ring_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_SOFTWARE;

	return 0;
}

static void *init_indio_device(struct device *dev, const struct iio_info *info,
			       const struct iio_chan_spec *channels, const char *device_name, const int bytes)
{
	struct iio_dev *indio_dev;
	int ret = 0;

	indio_dev = iio_device_alloc(0);
	if (!indio_dev) {
		goto err_alloc;
	}

	indio_dev->name = device_name;
	indio_dev->dev.parent = dev;
	indio_dev->info = info;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
	indio_dev->driver_module = THIS_MODULE;
#endif
	indio_dev->channels = channels;
	indio_dev->num_channels = 1;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	ret = shub_iio_configure_ring(indio_dev, bytes);
	if (ret) {
		goto err_config_ring;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		goto err_register_device;
	}

	return indio_dev;

err_register_device:
	shub_errf("fail to register %s device", device_name);
	shub_iio_kfifo_free(indio_dev->buffer);
err_config_ring:
	shub_errf("failed to configure %s buffer", indio_dev->name);
	iio_device_unregister(indio_dev);
err_alloc:
	shub_errf("fail to allocate memory for iio %s device", device_name);
	return NULL;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
static const struct iio_info indio_info = {
	.driver_module = THIS_MODULE,
};
#else
static const struct iio_info indio_info;
#endif

void remove_indio_dev()
{
	int i;

	for (i = 0 ; i < SENSOR_TYPE_LEGACY_MAX ; i++) {
		if (iio_list[i]) {
			iio_device_unregister(iio_list[i]->indio_dev);
			kfree(iio_list[i]);
		}
	}
}

static inline void set_channel_spec(struct iio_chan_spec *iio_channel, int realbits_size, int repeat_size)
{
	iio_channel->type = IIO_TIMESTAMP;
	iio_channel->channel = IIO_CHANNEL;
	iio_channel->scan_index = IIO_SCAN_INDEX;
	iio_channel->scan_type.sign = IIO_SIGN;
	iio_channel->scan_type.realbits = realbits_size;
	iio_channel->scan_type.storagebits = realbits_size;
	iio_channel->scan_type.shift = IIO_SHIFT;
	iio_channel->scan_type.repeat = repeat_size;
}

/* this function should be called when sensor list of sensor manager is existed */
int initialize_indio_dev(struct device *dev)
{
	int timestamp_len = sizeof(u64);
	int type;
	int realbits_size = 0;
	int repeat_size = 0;
	int bytes = 0;
	struct shub_sensor *sensor;

	for (type = 0 ; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		sensor = get_sensor(type);
		if (!sensor || sensor->report_event_size == 0)
			continue;

		bytes = (sensor->report_event_size+timestamp_len);
		realbits_size =  bytes * BITS_PER_BYTE;
		repeat_size = 1;

		while ((realbits_size / repeat_size > 255) && (realbits_size % repeat_size == 0))
			repeat_size++;
		realbits_size /= repeat_size;

		iio_list[type] = (struct shub_iio_device *)kzalloc(sizeof(struct shub_iio_device), GFP_KERNEL);
		if (!iio_list[type]) {
			shub_errf("fail to malloc %s iio dev", sensor->name);
			continue;
		}
		set_channel_spec(&iio_list[type]->iio_channel, realbits_size, repeat_size);
		iio_list[type]->indio_dev = (struct iio_dev *)init_indio_device(dev, &indio_info, &iio_list[type]->iio_channel, sensor->name, bytes);
		if (!iio_list[type]->indio_dev) {
			shub_errf("fail to init_indio_device %s", sensor->name);
			kfree(iio_list[type]);
			iio_list[type] = NULL;
		}
	}

	return 0;
}

void shub_report_sensordata(int type, u64 timestamp, char *data, int data_len)
{
	struct iio_dev *indio_dev = get_iio_device(type);
	struct shub_sensor *sensor = get_sensor(type);
	char *buf;

	if ((!sensor) || (sensor && sensor->report_event_size == 0))
		return;

	if (!indio_dev || !data || data_len == 0) {
		shub_errf("type(%d) indio_dev | data | data_len is wrong", type);
		return;
	}

	buf = kzalloc(sensor->report_event_size + sizeof(timestamp), GFP_KERNEL);
	if (!buf) {
		shub_errf("fail to alloc memory");
		return;
	}

	memcpy(buf, data, data_len);
	memcpy(buf + data_len, &timestamp, sizeof(timestamp));
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);

	kfree(buf);
}
