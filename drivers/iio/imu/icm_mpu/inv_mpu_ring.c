/*
* Copyright (C) 2012 Invensense, Inc.
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
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include "inv_mpu_iio.h"

#ifdef LINUX_KERNEL_3_10
#define INV_PUSH_IIO(x, y) iio_push_to_buffers(x, y)
#else
#define INV_PUSH_IIO(x, y) iio_push_to_buffer(x->buffer, y, 0)
#endif

static void inv_push_timestamp(struct iio_dev *indio_dev, u64 t)
{
	u8 buf[IIO_BUFFER_BYTES];

	memcpy(buf, &t, sizeof(t));
	INV_PUSH_IIO(indio_dev, buf);
}

int inv_push_marker_to_buffer(struct inv_mpu_state *st, u16 hdr, int data)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];

	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(&buf[4], &data, sizeof(data));
	INV_PUSH_IIO(indio_dev, buf);

	return 0;
}

int inv_push_16bytes_buffer_compass(struct inv_mpu_state *st, u16 sensor,
								u64 t, int *q)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int ii, j;

	for (j = 0; j < SENSOR_L_NUM_MAX; j++) {
		if (st->sensor_l[j].on && (st->sensor_l[j].base == sensor)) {
			st->sensor_l[j].counter++;
			if (st->sensor_l[j].counter >= st->sensor_l[j].div) {

				memcpy(buf, &st->sensor_l[j].header,
						sizeof(st->sensor_l[j].header));
				memcpy(buf + 4, &q[0], sizeof(q[0]));
				INV_PUSH_IIO(indio_dev, buf);
				for (ii = 0; ii < 2; ii++)
					memcpy(buf + 4 * ii, &q[ii + 1],
								sizeof(q[ii]));
				INV_PUSH_IIO(indio_dev, buf);
				inv_push_timestamp(indio_dev, t);
				st->sensor_l[j].counter = 0;
			}
		}
	}

	return 0;
}

int inv_push_16bytes_buffer(struct inv_mpu_state *st, u16 sensor, u64 t, int *q)
{
	return inv_push_16bytes_buffer_compass(st, sensor, t, q);
}

int inv_push_special_8bytes_buffer(struct inv_mpu_state *st,
						u16 hdr, u64 t, s16 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int j;

	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(&buf[2], &d[0], sizeof(d[0]));
	for (j = 0; j < 2; j++)
		memcpy(&buf[4 + j * 2], &d[j + 1], sizeof(d[j]));
	INV_PUSH_IIO(indio_dev, buf);
	inv_push_timestamp(indio_dev, t);

	return 0;
}

int inv_push_8bytes_buffer(struct inv_mpu_state *st, u16 sensor, u64 t, s16 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int ii, j;

	if ((sensor == STEP_DETECTOR_HDR) || (sensor == STEP_DETECTOR_WAKE_HDR)) {
		memcpy(buf, &sensor, sizeof(sensor));
		memcpy(&buf[2], &d[0], sizeof(d[0]));
		for (j = 0; j < 2; j++)
			memcpy(&buf[4 + j * 2], &d[j + 1], sizeof(d[j]));
		INV_PUSH_IIO(indio_dev, buf);
		inv_push_timestamp(indio_dev, t);
		return 0;
	}
	for (ii = 0; ii < SENSOR_L_NUM_MAX; ii++) {
		if (st->sensor_l[ii].on &&
				(st->sensor_l[ii].base == sensor) &&
				(st->sensor_l[ii].div != 0xffff)) {
			st->sensor_l[ii].counter++;
			if (st->sensor_l[ii].counter >= st->sensor_l[ii].div) {
				memcpy(buf, &st->sensor_l[ii].header,
					sizeof(st->sensor_l[ii].header));
				memcpy(&buf[2], &d[0], sizeof(d[0]));
				for (j = 0; j < 2; j++)
					memcpy(&buf[4 + j * 2], &d[j + 1],
								sizeof(d[j]));
				INV_PUSH_IIO(indio_dev, buf);
				inv_push_timestamp(indio_dev, t);
				st->sensor_l[ii].counter = 0;
			}
		}
	}

	return 0;
}
int inv_push_8bytes_kf(struct inv_mpu_state *st, u16 hdr, u64 t, s16 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES * 2];
	int i;

#define TILT_DETECTED  0x1000
	if (st->chip_config.activity_on) {
		memcpy(buf, &hdr, sizeof(hdr));
		for (i = 0; i < 3; i++)
			memcpy(&buf[2 + i * 2], &d[i], sizeof(d[i]));
		kfifo_in(&st->kf, buf, IIO_BUFFER_BYTES * 2);
		memcpy(buf, &t, sizeof(t));
		kfifo_in(&st->kf, buf, IIO_BUFFER_BYTES * 2);
		st->activity_size += IIO_BUFFER_BYTES * 4;
	}

	if ((d[0] & TILT_DETECTED) && st->chip_config.tilt_enable) {
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_tilt");
	}

	return 0;
}
int inv_send_steps(struct inv_mpu_state *st, int step, u64 ts)
{
	s16 s[3];

	s[0] = 0;
	s[1] = (s16)(step & 0xffff);
	s[2] = (s16)((step >> 16) & 0xffff);
	if (st->step_counter_l_on)
		inv_push_special_8bytes_buffer(st, STEP_COUNTER_HDR, ts, s);
	if (st->step_counter_wake_l_on)
		inv_push_special_8bytes_buffer(st, STEP_COUNTER_WAKE_HDR,
								ts, s);
	return 0;
}

void inv_push_step_indicator(struct inv_mpu_state *st, u64 t)
{
	s16 sen[3];
#define STEP_INDICATOR_HEADER 0x0001

	sen[0] = 0;
	sen[1] = 0;
	sen[2] = 0;
	inv_push_8bytes_buffer(st, STEP_INDICATOR_HEADER, t, sen);
}

/*
 *  inv_irq_handler() - Cache a timestamp at each data ready interrupt.
 */
static irqreturn_t inv_irq_handler(int irq, void *dev_id)
{
	struct inv_mpu_state *st;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	st = (struct inv_mpu_state *)dev_id;
	st->last_isr_time = get_time_ns();
	return IRQ_WAKE_THREAD;
}

void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	free_irq(st->irq, st);
	iio_kfifo_free(indio_dev->buffer);
	INVLOG(IL4, "\n");
};

static int inv_predisable(struct iio_dev *indio_dev)
{
	INVLOG(IL4, "\n");
	return 0;
}

static int inv_preenable(struct iio_dev *indio_dev)
{
	int result;
	INVLOG(IL4, "Enter\n");

	result = iio_sw_buffer_preenable(indio_dev);

	INVLOG(IL4, "Exit\n");
	return result;
}

static const struct iio_buffer_setup_ops inv_mpu_ring_setup_ops = {
	.preenable = &inv_preenable,
	.predisable = &inv_predisable,
};

int inv_mpu_configure_ring(struct iio_dev *indio_dev)
{
	int ret;
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_buffer *ring;

	INVLOG(IL4, "enter\n");
	ring = iio_kfifo_allocate(indio_dev);
	if (!ring)
		return -ENOMEM;
	indio_dev->buffer = ring;
	/* setup ring buffer */
	ring->scan_timestamp = true;
	indio_dev->setup_ops = &inv_mpu_ring_setup_ops;
	INVLOG(IL3, "st->irq %d, st->client->irq %d\n", st->irq, st->client->irq);
	ret = request_threaded_irq(st->irq, inv_irq_handler,
			inv_read_fifo,
			IRQF_TRIGGER_RISING | IRQF_SHARED, "inv_irq", st);
	if (ret)
		goto error_iio_sw_rb_free;

	indio_dev->modes |= INDIO_BUFFER_HARDWARE;

	INVLOG(IL4, "exit\n");
	return 0;
error_iio_sw_rb_free:
	iio_kfifo_free(indio_dev->buffer);

	return ret;
}

