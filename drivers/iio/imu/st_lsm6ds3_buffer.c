/*
 * STMicroelectronics lsm6ds3 buffer driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "st_lsm6ds3.h"

#define ST_LSM6DS3_ENABLE_AXIS			0x07
#define ST_LMS6DS3_PRINT_SEC			10
#define ST_GYRO_SKIP_DELAY			80000000L /* 80ms */

#define SENSOR_DATA_X(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
						((x1 == 1 ? datax : (x1 == -1 ? -datax : 0)) + \
						(x2 == 1 ? datay : (x2 == -1 ? -datay : 0)) + \
						(x3 == 1 ? dataz : (x3 == -1 ? -dataz : 0)))

#define SENSOR_DATA_Y(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
						((y1 == 1 ? datax : (y1 == -1 ? -datax : 0)) + \
						(y2 == 1 ? datay : (y2 == -1 ? -datay : 0)) + \
						(y3 == 1 ? dataz : (y3 == -1 ? -dataz : 0)))

#define SENSOR_DATA_Z(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
						((z1 == 1 ? datax : (z1 == -1 ? -datax : 0)) + \
						(z2 == 1 ? datay : (z2 == -1 ? -datay : 0)) + \
						(z3 == 1 ? dataz : (z3 == -1 ? -dataz : 0)))

#define SENSOR_X_DATA(...)			SENSOR_DATA_X(__VA_ARGS__)
#define SENSOR_Y_DATA(...)			SENSOR_DATA_Y(__VA_ARGS__)
#define SENSOR_Z_DATA(...)			SENSOR_DATA_Z(__VA_ARGS__)

irqreturn_t st_lsm6ds3_iio_pollfunc_store_boottime(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	pf->timestamp = st_lsm6ds3_iio_get_boottime_ns();
	return IRQ_WAKE_THREAD;
}

static int st_lsm6ds3_get_buffer_element(struct iio_dev *indio_dev, u8 *buf)
{
	int i, n = 0, len;
	u8 addr[ST_LSM6DS3_NUMBER_DATA_CHANNELS];
	struct lsm6ds3_sensor_data *sdata = iio_priv(indio_dev);
	struct lsm6ds3_data *cdata = sdata->cdata;

	for (i = 0; i < ST_LSM6DS3_NUMBER_DATA_CHANNELS; i++) {
		if (test_bit(i, indio_dev->active_scan_mask)) {
			addr[n] = indio_dev->channels[i].address;
			n++;
		}
	}
	switch (n) {
	case 1:
		len = cdata->tf->read(&cdata->tb, cdata->dev,
				addr[0], ST_LSM6DS3_BYTE_FOR_CHANNEL, buf);
		break;
	case 2:
		if ((addr[1] - addr[0]) == ST_LSM6DS3_BYTE_FOR_CHANNEL) {
			len = cdata->tf->read(&cdata->tb,
					cdata->dev, addr[0],
					ST_LSM6DS3_BYTE_FOR_CHANNEL * n, buf);
		} else {
			u8 rx_array[ST_LSM6DS3_BYTE_FOR_CHANNEL*
					ST_LSM6DS3_NUMBER_DATA_CHANNELS];
			len = cdata->tf->read(&cdata->tb,
				cdata->dev, addr[0],
				ST_LSM6DS3_BYTE_FOR_CHANNEL *
				ST_LSM6DS3_NUMBER_DATA_CHANNELS, rx_array);
			if (len < 0)
				goto read_data_channels_error;

			for (i = 0; i < n * ST_LSM6DS3_BYTE_FOR_CHANNEL;
									i++) {
				if (i < n)
					buf[i] = rx_array[i];
				else
					buf[i] = rx_array[n + i];
			}
		}
		break;
	case 3:
		len = cdata->tf->read(&cdata->tb, cdata->dev,
					addr[0], ST_LSM6DS3_BYTE_FOR_CHANNEL*
					ST_LSM6DS3_NUMBER_DATA_CHANNELS, buf);
		if (len < 0)
			goto read_data_channels_error;

		break;
	default:
		len = -EINVAL;
		goto read_data_channels_error;
	}
	len = ST_LSM6DS3_BYTE_FOR_CHANNEL * n;

	if ((cdata->patch_feature) && (sdata->sindex == ST_INDIO_DEV_GYRO) &&
					(cdata->gyro_selftest_status > 0)) {
		n = 0;

		for (i = 0; i < ST_LSM6DS3_NUMBER_DATA_CHANNELS; i++) {
			if (test_bit(i, indio_dev->active_scan_mask)) {
				if (cdata->gyro_selftest_status == 1)
					*(u16 *)((u8 *)&buf[2 * n]) +=
						cdata->gyro_selftest_delta[i];
				else
					*(u16 *)((u8 *)&buf[2 * n]) -=
						cdata->gyro_selftest_delta[i];

				n++;
			}
		}
	}

	// Applying rotation matrix
	if ((sdata->sindex == ST_INDIO_DEV_ACCEL) || (sdata->sindex == ST_INDIO_DEV_GYRO)) {
		s16 tmp_data[3], raw_data[3];

		for (n = 0; n < ST_LSM6DS3_NUMBER_DATA_CHANNELS; n++) {
			raw_data[n] = *((s16 *)&buf[2 * n]);
			if (raw_data[n] == -32768) raw_data[n] = -32767;
		}
		if (sdata->sindex == ST_INDIO_DEV_ACCEL) {
			tmp_data[0] = SENSOR_X_DATA(raw_data[0], raw_data[1], raw_data[2], \
                                cdata->orientation[0],cdata->orientation[1],cdata->orientation[2],\
                                cdata->orientation[3],cdata->orientation[4],cdata->orientation[5], \
                                cdata->orientation[6],cdata->orientation[7],cdata->orientation[8]);
			tmp_data[1] = SENSOR_Y_DATA(raw_data[0], raw_data[1], raw_data[2], \
                                cdata->orientation[0],cdata->orientation[1],cdata->orientation[2],\
                                cdata->orientation[3],cdata->orientation[4],cdata->orientation[5], \
                                cdata->orientation[6],cdata->orientation[7],cdata->orientation[8]);
			tmp_data[2] = SENSOR_Z_DATA(raw_data[0], raw_data[1], raw_data[2], \
                                cdata->orientation[0],cdata->orientation[1],cdata->orientation[2],\
                                cdata->orientation[3],cdata->orientation[4],cdata->orientation[5], \
                                cdata->orientation[6],cdata->orientation[7],cdata->orientation[8]);
		}
		else {
			raw_data[0] -= sdata->cdata->gyro_cal_data[0];
			raw_data[1] -= sdata->cdata->gyro_cal_data[1];
			raw_data[2] -= sdata->cdata->gyro_cal_data[2];
			tmp_data[0] = SENSOR_X_DATA(raw_data[0], raw_data[1], raw_data[2], \
                                cdata->orientation[0],cdata->orientation[1],cdata->orientation[2],\
                                cdata->orientation[3],cdata->orientation[4],cdata->orientation[5], \
                                cdata->orientation[6],cdata->orientation[7],cdata->orientation[8]);
			tmp_data[1] = SENSOR_Y_DATA(raw_data[0], raw_data[1], raw_data[2], \
                                cdata->orientation[0],cdata->orientation[1],cdata->orientation[2],\
                                cdata->orientation[3],cdata->orientation[4],cdata->orientation[5], \
                                cdata->orientation[6],cdata->orientation[7],cdata->orientation[8]);
			tmp_data[2] = SENSOR_Z_DATA(raw_data[0], raw_data[1], raw_data[2], \
                                cdata->orientation[0],cdata->orientation[1],cdata->orientation[2],\
                                cdata->orientation[3],cdata->orientation[4],cdata->orientation[5], \
                                cdata->orientation[6],cdata->orientation[7],cdata->orientation[8]);
		}
		for (n = 0; n < ST_LSM6DS3_BYTE_FOR_CHANNEL * ST_LSM6DS3_NUMBER_DATA_CHANNELS; n++) {
			buf[n] = *(((u8 *)tmp_data) + n);
		}
#if 0
		dev_info(cdata->dev, "%s buf(%d) %d, %d, %d\n", __func__, sdata->sindex,
							*((s16 *)&buf[0]),
							*((s16 *)&buf[2]),
							*((s16 *)&buf[4]));
#endif
	}
read_data_channels_error:
	return len;
}

static irqreturn_t st_lsm6ds3_accel_gyro_trigger_handler(int irq, void *p)
{
	int len;
	static int acc_cnt = 0, gyro_cnt = 0;
	static s64 skip_time_prev;
	s64 skip_time_current = 0;
	s64 pr_delay = 0, skip_delay = 0;

	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct lsm6ds3_sensor_data *sdata = iio_priv(indio_dev);

	len = st_lsm6ds3_get_buffer_element(indio_dev, sdata->buffer_data);
	if (len < 0)
		goto st_lsm6ds3_get_buffer_element_error;

	if(sdata->sindex == ST_INDIO_DEV_ACCEL) {
		sdata->cdata->accel_data[0] = ((sdata->buffer_data[1] << 8) | sdata->buffer_data[0]);
		sdata->cdata->accel_data[1] = ((sdata->buffer_data[3] << 8) | sdata->buffer_data[2]);
		sdata->cdata->accel_data[2] = ((sdata->buffer_data[5] << 8) | sdata->buffer_data[4]);

		sdata->cdata->accel_data[0] -= sdata->cdata->accel_cal_data[0];
		sdata->cdata->accel_data[1] -= sdata->cdata->accel_cal_data[1];
		sdata->cdata->accel_data[2] -= sdata->cdata->accel_cal_data[2];

		sdata->buffer_data[0] = (u8)(sdata->cdata->accel_data[0] & 0x00ff);
		sdata->buffer_data[1] = (u8)((sdata->cdata->accel_data[0] & 0xff00) >> 8);
		sdata->buffer_data[2] = (u8)(sdata->cdata->accel_data[1] & 0x00ff);
		sdata->buffer_data[3] = (u8)((sdata->cdata->accel_data[1] & 0xff00) >> 8);
		sdata->buffer_data[4] = (u8)(sdata->cdata->accel_data[2] & 0x00ff);
		sdata->buffer_data[5] = (u8)((sdata->cdata->accel_data[2] & 0xff00) >> 8);
	} else if(sdata->sindex == ST_INDIO_DEV_GYRO) {
		if (sdata->cdata->skip_gyro_data == true) {
			sdata->cdata->gyro_data[0] = 0;
			sdata->cdata->gyro_data[1] = 0;
			sdata->cdata->gyro_data[2] = 0;

			if(!sdata->cdata->skip_gyro_cnt)
				skip_time_prev = st_lsm6ds3_iio_get_boottime_ns();

			sdata->cdata->skip_gyro_cnt++;

			skip_time_current = st_lsm6ds3_iio_get_boottime_ns();
			skip_delay = (skip_time_current - skip_time_prev);
			if ((((s64)ST_GYRO_SKIP_DELAY) <= skip_delay) ||
				(sdata->cdata->skip_gyro_cnt == 10)) {
				sdata->cdata->skip_gyro_data = false;
				pr_info("%s, skip_gyro off(%d), delay : %lld\n",
					__func__, sdata->cdata->skip_gyro_cnt, skip_delay);
				sdata->cdata->skip_gyro_cnt = 0;
				skip_time_prev = 0;
			}
		} else {
			sdata->cdata->gyro_data[0] = ((sdata->buffer_data[1] << 8) | sdata->buffer_data[0]);
			sdata->cdata->gyro_data[1] = ((sdata->buffer_data[3] << 8) | sdata->buffer_data[2]);
			sdata->cdata->gyro_data[2] = ((sdata->buffer_data[5] << 8) | sdata->buffer_data[4]);
		}
		sdata->buffer_data[0] = (u8)(sdata->cdata->gyro_data[0] & 0x00ff);
		sdata->buffer_data[1] = (u8)((sdata->cdata->gyro_data[0] & 0xff00) >> 8);
		sdata->buffer_data[2] = (u8)(sdata->cdata->gyro_data[1] & 0x00ff);
		sdata->buffer_data[3] = (u8)((sdata->cdata->gyro_data[1] & 0xff00) >> 8);
		sdata->buffer_data[4] = (u8)(sdata->cdata->gyro_data[2] & 0x00ff);
		sdata->buffer_data[5] = (u8)((sdata->cdata->gyro_data[2] & 0xff00) >> 8);
	}

	if ((sdata->sindex == ST_INDIO_DEV_ACCEL)
			|| (sdata->sindex == ST_INDIO_DEV_GYRO)) {
		pr_delay = sdata->cdata->timestamp - sdata->old_timestamp;
		if ((pr_delay > (s64)sdata->poll_delay * 1800000)
				&& (sdata->old_timestamp != 0)) {
			if (indio_dev->scan_timestamp)
				*(s64 *)((u8 *)sdata->buffer_data + ALIGN(len, sizeof(s64))) =
					(sdata->cdata->timestamp + sdata->old_timestamp) >> 1;
			iio_push_to_buffers(indio_dev, sdata->buffer_data);
			/*pr_info("%s: %d gap %lld\n", __func__, sdata->sindex, pr_delay);*/
		}
		sdata->old_timestamp = sdata->cdata->timestamp;
	}

	if (indio_dev->scan_timestamp)
		*(s64 *)((u8 *)sdata->buffer_data +
			ALIGN(len, sizeof(s64))) = sdata->cdata->timestamp;
	iio_push_to_buffers(indio_dev, sdata->buffer_data);

	if (sdata->sindex == ST_INDIO_DEV_ACCEL) {
		if (((s64)ST_LMS6DS3_PRINT_SEC * MSEC_PER_SEC) <=
				(sdata->poll_delay * acc_cnt)) {
			pr_info("%s, acc_x = %d, acc_y = %d, acc_z = %d, err = %d\n",
				__func__, sdata->cdata->accel_data[0],
				sdata->cdata->accel_data[1],
				sdata->cdata->accel_data[2],
				sdata->cdata->err_count);
			acc_cnt = 0;
		} else {
			acc_cnt++;
		}
	} else if(sdata->sindex == ST_INDIO_DEV_GYRO) {
		if (((s64)ST_LMS6DS3_PRINT_SEC * MSEC_PER_SEC) <=
				(sdata->poll_delay * gyro_cnt)) {
			pr_info("%s, gyro_x = %d, gyro_y = %d, gyro_z = %d, err = %d\n",
				__func__, sdata->cdata->gyro_data[0],
				sdata->cdata->gyro_data[1],
				sdata->cdata->gyro_data[2],
				sdata->cdata->err_count);
			gyro_cnt = 0;
		} else {
			gyro_cnt++;
		}
	}
st_lsm6ds3_get_buffer_element_error:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static irqreturn_t st_lsm6ds3_step_counter_trigger_handler(int irq, void *p)
{
	int err;
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct lsm6ds3_sensor_data *sdata = iio_priv(indio_dev);

	err = sdata->cdata->tf->read(&sdata->cdata->tb,
					sdata->cdata->dev,
					(u8)indio_dev->channels[0].address,
					ST_LSM6DS3_BYTE_FOR_CHANNEL,
					(u8 *)&sdata->cdata->step_c.steps);
	if (err < 0)
		goto step_counter_done;

	sdata->cdata->step_c.new_steps = true;
	sdata->cdata->step_c.last_step_timestamp = sdata->cdata->timestamp;

step_counter_done:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static inline irqreturn_t st_lsm6ds3_handler_empty(int irq, void *p)
{
	return IRQ_HANDLED;
}

int st_lsm6ds3_trig_set_state(struct iio_trigger *trig, bool state)
{
	int err;
	struct lsm6ds3_sensor_data *sdata;

	sdata = iio_priv(iio_trigger_get_drvdata(trig));

	err = st_lsm6ds3_set_drdy_irq(sdata, state);

	return err < 0 ? err : 0;
}

static int st_lsm6ds3_buffer_preenable(struct iio_dev *indio_dev)
{
	int err;
	struct lsm6ds3_sensor_data *sdata = iio_priv(indio_dev);
	pr_info("%s, start! (%d)\n", __func__, sdata->sindex);

	if (sdata->sindex == ST_INDIO_DEV_ACCEL)
		err = st_lsm6ds3_acc_open_calibration(sdata->cdata);
	else if(sdata->sindex == ST_INDIO_DEV_GYRO)
		err = st_lsm6ds3_gyro_open_calibration(sdata->cdata);
	err = st_lsm6ds3_set_enable(sdata, true);
	if (err < 0)
		return err;

	return iio_sw_buffer_preenable(indio_dev);
}

static int st_lsm6ds3_buffer_postenable(struct iio_dev *indio_dev)
{
	int err = 0;
	struct lsm6ds3_sensor_data *sdata = iio_priv(indio_dev);
	pr_info("%s, start! (%d)\n", __func__, sdata->sindex);
	if ((1 << sdata->sindex) & ST_LSM6DS3_USE_BUFFER) {
		sdata->buffer_data = kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
		if (sdata->buffer_data == NULL)
			return -ENOMEM;
	}

	if ((sdata->sindex == ST_INDIO_DEV_ACCEL) ||
					(sdata->sindex == ST_INDIO_DEV_GYRO)) {
		err = st_lsm6ds3_set_axis_enable(sdata,
					(u8)indio_dev->active_scan_mask[0]);
		if (err < 0)
			goto free_buffer_data;
	}

	err = iio_triggered_buffer_postenable(indio_dev);
	if (err < 0)
		goto free_buffer_data;

	if (sdata->sindex == ST_INDIO_DEV_STEP_COUNTER) {
		sdata->cdata->step_c.new_steps = true;
		iio_trigger_poll_chained(
			sdata->cdata->trig[ST_INDIO_DEV_STEP_COUNTER], 0);
		hrtimer_start(&sdata->cdata->step_c.hr_timer,
				sdata->cdata->step_c.ktime, HRTIMER_MODE_REL);
	}

	if (sdata->sindex == ST_INDIO_DEV_SIGN_MOTION)
		sdata->cdata->sign_motion_event_ready = true;

	return err;

free_buffer_data:
	if ((1 << sdata->sindex) & ST_LSM6DS3_USE_BUFFER)
		kfree(sdata->buffer_data);

	return err;
}

static int st_lsm6ds3_buffer_predisable(struct iio_dev *indio_dev)
{
	int err = 0;
	struct lsm6ds3_sensor_data *sdata = iio_priv(indio_dev);
	pr_info("%s, start! (%d)\n", __func__, sdata->sindex);

#if (LSM6DS3_HRTIMER_TRIGGER > 0)
	if (sdata->sindex == ST_INDIO_DEV_ACCEL) {
		if (sdata->cdata->sa_flag)
			sdata->cdata->sensors_enabled &= ~(1 << sdata->sindex);
		else
			err = st_lsm6ds3_set_enable(sdata, false);
	} else {
		err = st_lsm6ds3_set_enable(sdata, false);
	}

	if (err < 0)
		return err;
#endif

	err = iio_triggered_buffer_predisable(indio_dev);
	if (err < 0)
		return err;

	if ((sdata->sindex == ST_INDIO_DEV_ACCEL) ||
					(sdata->sindex == ST_INDIO_DEV_GYRO)) {
		err = st_lsm6ds3_set_axis_enable(sdata, ST_LSM6DS3_ENABLE_AXIS);
		if (err < 0)
			return err;
	}

	if (sdata->sindex == ST_INDIO_DEV_SIGN_MOTION)
		sdata->cdata->sign_motion_event_ready = false;

#if !(LSM6DS3_HRTIMER_TRIGGER > 0)
	if (sdata->sindex == ST_INDIO_DEV_ACCEL) {
		if (sdata->cdata->sa_flag)
			sdata->cdata->sensors_enabled &= ~(1 << sdata->sindex);
		else
			err = st_lsm6ds3_set_enable(sdata, false);
	} else {
		err = st_lsm6ds3_set_enable(sdata, false);
	}

	if (err < 0)
		return err;
#endif

	if (sdata->sindex == ST_INDIO_DEV_STEP_COUNTER)
		hrtimer_cancel(&sdata->cdata->step_c.hr_timer);

	if ((1 << sdata->sindex) & ST_LSM6DS3_USE_BUFFER)
		kfree(sdata->buffer_data);

	return 0;
}

static const struct iio_buffer_setup_ops st_lsm6ds3_buffer_setup_ops = {
	.preenable = &st_lsm6ds3_buffer_preenable,
	.postenable = &st_lsm6ds3_buffer_postenable,
	.predisable = &st_lsm6ds3_buffer_predisable,
};

int st_lsm6ds3_allocate_rings(struct lsm6ds3_data *cdata)
{
	int err;

	err = iio_triggered_buffer_setup(cdata->indio_dev[ST_INDIO_DEV_ACCEL],
				&st_lsm6ds3_iio_pollfunc_store_boottime,
				&st_lsm6ds3_accel_gyro_trigger_handler,
				&st_lsm6ds3_buffer_setup_ops);
	if (err < 0)
		return err;

	err = iio_triggered_buffer_setup(cdata->indio_dev[ST_INDIO_DEV_GYRO],
				&st_lsm6ds3_iio_pollfunc_store_boottime,
				&st_lsm6ds3_accel_gyro_trigger_handler,
				&st_lsm6ds3_buffer_setup_ops);
	if (err < 0)
		goto buffer_cleanup_accel;

	err = iio_triggered_buffer_setup(
				cdata->indio_dev[ST_INDIO_DEV_SIGN_MOTION],
				&st_lsm6ds3_handler_empty, NULL,
				&st_lsm6ds3_buffer_setup_ops);
	if (err < 0)
		goto buffer_cleanup_gyro;

	err = iio_triggered_buffer_setup(
				cdata->indio_dev[ST_INDIO_DEV_STEP_COUNTER],
				&st_lsm6ds3_iio_pollfunc_store_boottime,
				&st_lsm6ds3_step_counter_trigger_handler,
				&st_lsm6ds3_buffer_setup_ops);
	if (err < 0)
		goto buffer_cleanup_sign_motion;

	err = iio_triggered_buffer_setup(
				cdata->indio_dev[ST_INDIO_DEV_STEP_DETECTOR],
				&st_lsm6ds3_handler_empty, NULL,
				&st_lsm6ds3_buffer_setup_ops);
	if (err < 0)
		goto buffer_cleanup_step_counter;

	err = iio_triggered_buffer_setup(
				cdata->indio_dev[ST_INDIO_DEV_TILT],
				&st_lsm6ds3_handler_empty, NULL,
				&st_lsm6ds3_buffer_setup_ops);
	if (err < 0)
		goto buffer_cleanup_step_detector;

	return 0;

buffer_cleanup_step_detector:
	iio_triggered_buffer_cleanup(
				cdata->indio_dev[ST_INDIO_DEV_STEP_DETECTOR]);
buffer_cleanup_step_counter:
	iio_triggered_buffer_cleanup(
				cdata->indio_dev[ST_INDIO_DEV_STEP_COUNTER]);
buffer_cleanup_sign_motion:
	iio_triggered_buffer_cleanup(
				cdata->indio_dev[ST_INDIO_DEV_SIGN_MOTION]);
buffer_cleanup_gyro:
	iio_triggered_buffer_cleanup(cdata->indio_dev[ST_INDIO_DEV_GYRO]);
buffer_cleanup_accel:
	iio_triggered_buffer_cleanup(cdata->indio_dev[ST_INDIO_DEV_ACCEL]);
	return err;
}

void st_lsm6ds3_deallocate_rings(struct lsm6ds3_data *cdata)
{
	iio_triggered_buffer_cleanup(cdata->indio_dev[ST_INDIO_DEV_TILT]);
	iio_triggered_buffer_cleanup(
				cdata->indio_dev[ST_INDIO_DEV_STEP_DETECTOR]);
	iio_triggered_buffer_cleanup(
				cdata->indio_dev[ST_INDIO_DEV_STEP_COUNTER]);
	iio_triggered_buffer_cleanup(
				cdata->indio_dev[ST_INDIO_DEV_SIGN_MOTION]);
	iio_triggered_buffer_cleanup(cdata->indio_dev[ST_INDIO_DEV_ACCEL]);
	iio_triggered_buffer_cleanup(cdata->indio_dev[ST_INDIO_DEV_GYRO]);
}

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics lsm6ds3 buffer driver");
MODULE_LICENSE("GPL v2");
