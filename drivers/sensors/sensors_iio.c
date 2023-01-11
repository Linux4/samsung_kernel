/*
 *  /driver/sensors/sensors_core.c
 *
 *  Copyright (C) 2011 Samsung Electronics Co.Ltd
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
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/sensor/sensors_core.h>

int sensors_iio_configure_buffer(struct iio_dev *indio_dev)
{
	struct iio_buffer *buffer;
	int ret;

	buffer = iio_kfifo_allocate(indio_dev);
	if (!buffer)
		return -ENOMEM;

	buffer->scan_timestamp = true;
	iio_device_attach_buffer(indio_dev, buffer);

	indio_dev->modes |= INDIO_BUFFER_HARDWARE;

	ret = iio_buffer_register(indio_dev, indio_dev->channels,
		indio_dev->num_channels);
	if (ret) {
		pr_err("[SENSOR] %s, buffer register failed\n", __func__);
		goto exit;
	}

	pr_info("[SENSOR] %s, name = %s\n", __func__, indio_dev->name);

	return 0;

exit:
	iio_kfifo_free(indio_dev->buffer);
	return ret;
}
EXPORT_SYMBOL_GPL(sensors_iio_configure_buffer);

void sensors_iio_unconfigure_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
};
EXPORT_SYMBOL_GPL(sensors_iio_unconfigure_buffer);
