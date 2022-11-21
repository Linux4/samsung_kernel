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
#include <linux/spinlock.h>
#include <linux/crc32.h>

#include "inv_mpu_iio.h"

#ifdef CONFIG_DTS_INV_MPU_IIO
#include "inv_mpu_dts.h"
#endif

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{128, "ICM20645"},
//	{128, "ICM10340"},
	{128, "ICM10320"},
};
static int debug_mem_read_addr = 0x900;
static char debug_reg_addr = 0x6;

static int accel_open_calibration(struct inv_mpu_state *st)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs = {0};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(FACTORY_ACCEL_CAL_PATH,
		O_RDONLY, S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&st->cal_data,
			3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	pr_info("%s: (%d,%d,%d)\n", __func__,
		st->cal_data[0], st->cal_data[1],
			st->cal_data[2]);

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	return err;
}

/*
 * inv_firmware_loaded_store() -  calling this function will change
 *                        firmware load
 */
static ssize_t inv_firmware_loaded_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result, data;

	INVLOG(FUNC_INT_ENTRY, "enter\n");
	result = kstrtoint(buf, 10, &data);
	if (result)
	{
		INVLOG(ERR, "error\n");
		return -EINVAL;
	}

	if (data)
	{
		INVLOG(ERR, "error\n");
		return -EINVAL;
	}
	st->chip_config.firmware_loaded = 0;

	INVLOG(FUNC_INT_ENTRY, "exit\n");
	return count;

}

/*****************************************************************************/
/*     Pedo logging mode features                                            */
static ssize_t _dmp_pedlog_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	INVLOG(FUNC_INT_ENTRY, "Enter.\n");
	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto dmp_mem_store_fail;

	result = inv_switch_power_in_lp(st, true);
	
	switch (this_attr->address) {
	case ATTR_DMP_PEDLOG_ENABLE:
	{
		INVLOG(IL4, "ATTR_DMP_PEDLOG_ENABLE\n");
		if(!!st->ped.on)
		{
			result = inv_enable_pedlog(st, !!data, true);
			st->pedlog.enabled = !!data;
		}

		if (result)
			goto dmp_mem_store_fail;
	}
	break;

	case ATTR_DMP_PEDLOG_INTERRUPT_PERIOD:
	{
		INVLOG(IL4, "ATTR_DMP_PEDLOG_INTERRUPT_PERIOD\n");
		result = inv_set_pedlog_interrupt_period(st,(s16)data);
		result = inv_reset_pedlog_update_timer(st);
		if (result)
			goto dmp_mem_store_fail;	

		st->pedlog.interrupt_duration = (s16) data;
	}
	break;
	case ATTR_DMP_PEDLOG_TIMER:
		result = inv_set_pedlog_update_timer(st, (s16)data);
		break;

	default:
		result = -EINVAL;
		goto dmp_mem_store_fail;
	}
	
dmp_mem_store_fail:
	result = inv_switch_power_in_lp(st, false);

	if (result)
		return result;
	
	return count;
}

static ssize_t inv_dmp_pedlog_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	INVLOG(IL4, "Enter.\n");
	mutex_lock(&indio_dev->mlock);
	result = _dmp_pedlog_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

void inv_pedlog_timer_func(unsigned long data)
{
	struct inv_mpu_state *st =(struct inv_mpu_state*)data;

	INVLOG(FUNC_INT_ENTRY, "Enter.\n");

	schedule_work(&st->pedlog.work);
}

void inv_pedlog_sched_work(struct work_struct *data)
{
	struct inv_mpu_state *st = 
		(struct inv_mpu_state *)container_of(data,
		struct inv_mpu_state, pedlog.work);
	struct iio_dev *indio_dev = (struct iio_dev *)
				i2c_get_clientdata(st->client);

	INVLOG(IL4, "enter.\n");

	mutex_lock(&indio_dev->mlock);
	if ((st->pedlog.state == PEDLOG_STAT_WALK) &&
				(!st->pedlog.enabled)) {
		st->pedlog.state = PEDLOG_STAT_STOP;
		st->pedlog.interrupt_mask |= PEDLOG_INT_STOP_WALKING;
		complete(&st->pedlog.wait);
	}
	mutex_unlock(&indio_dev->mlock);
}

static ssize_t inv_pedlog_int_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_state *st = iio_priv(dev_get_drvdata(dev));
	u16 interrupt_mask = 0;

	if(st->ped.on) {
		INVLOG(FUNC_INT_ENTRY, "enter.\n");
		wait_for_completion_interruptible(&st->pedlog.wait);

		interrupt_mask = st->pedlog.interrupt_mask;
		st->pedlog.interrupt_mask = 0;

		if(interrupt_mask & PEDLOG_INT_CADENCE)
		{
			/*preparing cadence data*/
			s64 int_time = 0, start_time = 0;
			u16 prev_tick_cnt, tick_cnt;
			s16 tick_diff = 0;

			/*get_time_timeofday() function cannot get 
			  exact time information on other position*/
			int_time = get_time_timeofday();
			start_time = st->pedlog.start_time;
			prev_tick_cnt = st->pedlog.tick_count;

			st->pedlog.interrupt_time = int_time;
			
			tick_cnt = inv_calc_pedlog_tick_cnt(st, start_time, int_time);
			tick_diff = prev_tick_cnt - tick_cnt;
			INVLOG(IL4, "%d - %d = %d\n", prev_tick_cnt, tick_cnt, tick_diff);

			INVLOG(IL6, "DMP[start int diff] = [%lld %lld %lld]\n",
				start_time , int_time, int_time - start_time);

			/*change the value of update timer for adjusting interrupt timing*/
			if(tick_diff > -60 && tick_diff < 60)
			{
				inv_set_pedlog_update_timer(st, tick_cnt);
				st->pedlog.tick_count = tick_cnt;
				INVLOG(IL3, "%s Tick is changed from %d to %d\n",
					__func__, prev_tick_cnt, tick_cnt);
			}
			else
			{
				INVLOG(ERR, "%s time stamp is abnormal. [Start Int] [%lld %lld]\n",
						__func__, start_time, int_time);
			}
		}
		INVLOG(IL4,"exit. interrupt_mask:%d\n", interrupt_mask);
	}

	return sprintf(buf, "%d\n", interrupt_mask);
}
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

static int inv_dry_run_dmp(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);

	st->smd.on = 1;
	inv_check_sensor_on(st);
	st->trigger_state = EVENT_TRIGGER;
	set_inv_enable(indio_dev);
	msleep(DRY_RUN_TIME);
	st->smd.on = 0;
	inv_check_sensor_on(st);
	st->trigger_state = EVENT_TRIGGER;
	set_inv_enable(indio_dev);

	return 0;
}

static ssize_t _dmp_bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto dmp_bias_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_ACCEL_X_DMP_BIAS:
		INVLOG(IL6, "ATTR_DMP_ACCEL_X_DMP_BIAS\n");
		if (data)
			st->sensor_acurracy_flag[SENSOR_ACCEL_ACCURACY] =
							DEFAULT_ACCURACY;
		result = write_be32_to_mem(st, data, ACCEL_BIAS_X);
		if (result)
			goto dmp_bias_store_fail;
		st->input_accel_dmp_bias[0] = data;
		break;
	case ATTR_DMP_ACCEL_Y_DMP_BIAS:
		INVLOG(IL6, "ATTR_DMP_ACCEL_Y_DMP_BIAS\n");
		result = write_be32_to_mem(st, data, ACCEL_BIAS_Y);
		if (result)
			goto dmp_bias_store_fail;
		st->input_accel_dmp_bias[1] = data;
		break;
	case ATTR_DMP_ACCEL_Z_DMP_BIAS:
		INVLOG(IL6, "ATTR_DMP_ACCEL_Z_DMP_BIAS\n");
		result = write_be32_to_mem(st, data, ACCEL_BIAS_Z);
		if (result)
			goto dmp_bias_store_fail;
		st->input_accel_dmp_bias[2] = data;
		break;
	case ATTR_DMP_SC_AUTH:
		result = write_be32_to_mem(st, data, st->aut_key_in);
		if (result)
			goto dmp_bias_store_fail;
		result = write_be32_to_mem(st, 0, st->aut_key_out);
		if (result)
			goto dmp_bias_store_fail;
		/* dry run DMP to get the auth key output */
		inv_dry_run_dmp(indio_dev);
		break;
	case ATTR_DMP_MISC_ACCEL_RECALIBRATION:
	{
		u8 d[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		int i;
		u32 d1[] = {0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				3276800, 3276800, 3276800, 3276800};
		u32 *w_d;

		INVLOG(IL6, "ATTR_DMP_MISC_ACCEL_RECALIBRATION\n");
		if (data) {
			result = inv_write_2bytes(st, ACCEL_CAL_RESET, 1);
			if (result)
				goto dmp_bias_store_fail;
			result = mem_w(ACCEL_PRE_SENSOR_DATA, ARRAY_SIZE(d), d);
			w_d = d1;
		} else {
			w_d = st->accel_covariance;
		}
		for (i = 0; i < ARRAY_SIZE(d1); i++) {
			result = write_be32_to_mem(st, w_d[i],
					ACCEL_COVARIANCE + i * sizeof(int));
			if (result)
				goto dmp_bias_store_fail;
		}

		break;
	}
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD:
		INVLOG(IL6, "ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD\n");
		result = write_be32_to_mem(st, data, ACCEL_VARIANCE_THRESH);
		if (result)
			goto dmp_bias_store_fail;
		st->accel_calib_threshold = data;
		break;
	/* this serves as a divider of calibration rate, 0->225, 3->55 */
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE:
		INVLOG(IL6, "ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE\n");
		if (data < 0)
			data = 0;
		result = inv_write_2bytes(st, ACCEL_CAL_RATE, data);
		if (result)
			goto dmp_bias_store_fail;
		st->accel_calib_rate = data;
		break;
	case ATTR_DMP_DEBUG_MEM_READ:
		INVLOG(IL6, "ATTR_DMP_DEBUG_MEM_READ\n");
		debug_mem_read_addr = data;
		break;
	case ATTR_DMP_DEBUG_MEM_WRITE:
		INVLOG(IL6, "ATTR_DMP_DEBUG_MEM_WRITE\n");
		inv_write_2bytes(st, debug_mem_read_addr, data);
		break;
	default:
		break;
	}

dmp_bias_store_fail:
	if (result)
		return result;
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}


static int inv_set_accel_bias_reg(struct inv_mpu_state *st,
		long accel_bias, int axis)
{
	long accel_reg_bias;
	u8 addr;
	u8 d[2];
	int result = 0;

	inv_set_bank(st, BANK_SEL_1);

	switch (axis) {
	case 0:
		/* X */
		addr = REG_XA_OFFS_H;
		break;
	case 1:
		/* Y */
		addr = REG_YA_OFFS_H;
		break;
	case 2:
		/* Z* */
		addr = REG_ZA_OFFS_H;
		break;
	default:
		result = -EINVAL;
		goto accel_bias_set_err;
	}

	result = inv_plat_read(st, addr, 2, d);
	if (result)
		goto accel_bias_set_err;
	accel_reg_bias = ((long)d[0]<<8) | d[1];

	/* accel_bias is 2g scaled by 1<<16.
	 * Convert to 16g, and mask bit0 */
	accel_reg_bias -= ((accel_bias / 8 / 65536) & ~1);

	d[0] = (accel_reg_bias >> 8) & 0xff;
	d[1] = (accel_reg_bias) & 0xff;
	result = inv_plat_single_write(st, addr, d[0]);
	if (result)
		goto accel_bias_set_err;
	result = inv_plat_single_write(st, addr + 1, d[1]);
	if (result)
		goto accel_bias_set_err;

accel_bias_set_err:
	inv_set_bank(st, BANK_SEL_0);
	return result;
}


static ssize_t _bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto bias_store_fail;
	switch (this_attr->address) {
	case ATTR_ACCEL_X_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 0);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[0] = data;
		break;
	case ATTR_ACCEL_Y_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 1);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[1] = data;
		break;
	case ATTR_ACCEL_Z_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 2);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[2] = data;
		break;
	default:
		break;
	}

bias_store_fail:
	if (result)
		return result;
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}
static ssize_t inv_dmp_bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _dmp_bias_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _bias_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_debug_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	return count;
	switch (this_attr->address) {
	case ATTR_DMP_LP_EN_OFF:
		INVLOG(IL6, "ATTR_DMP_LP_EN_OFF\n");
		st->chip_config.lp_en_mode_off = !!data;
		inv_switch_power_in_lp(st, !!data);
		break;
	case ATTR_DMP_CLK_SEL:
		INVLOG(IL6, "ATTR_DMP_CLK_SEL\n");
		st->chip_config.clk_sel = !!data;
		inv_switch_power_in_lp(st, !!data);
		break;
	case ATTR_DEBUG_REG_ADDR:
		INVLOG(IL6, "ATTR_DEBUG_REG_ADDR\n");
		debug_reg_addr = data;
		break;
	case ATTR_DEBUG_REG_WRITE:
		INVLOG(IL6, "ATTR_DEBUG_REG_WRITE\n");
		inv_plat_single_write(st, debug_reg_addr, data);
		break;
	case ATTR_DEBUG_WRITE_CFG:
		INVLOG(IL6, "ATTR_DEBUG_WRITE_CFG\n");
		break;
	}
	INVLOG(FUNC_INT_ENTRY, "exit\n");
	return count;
}

static ssize_t _misc_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON:
		INVLOG(IL6, "ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON\n");
		st->debug_determine_engine_on = !!data;
		break;
	case ATTR_ACCEL_SCALE:
		INVLOG(IL4, "ATTR_ACCEL_SCALE\n");
		if (data > 3)
			return -EINVAL;
		st->chip_config.accel_fs = data;
		result = inv_set_accel_sf(st);

		return result;
	case ATTR_DMP_PED_STEP_THRESH:
		INVLOG(IL6, "ATTR_DMP_PED_STEP_THRESH\n");
		result = inv_write_2bytes(st, PEDSTD_SB, data);
		if (result)
			return result;
		st->ped.step_thresh = data;

		return 0;
	case ATTR_DMP_PED_INT_THRESH:
		INVLOG(IL6, "ATTR_DMP_PED_INT_THRESH\n");
		result = inv_write_2bytes(st, PEDSTD_SB2, data);
		if (result)
			return result;
		st->ped.int_thresh = data;

		return 0;
	default:
		return -EINVAL;
	}
	st->trigger_state = MISC_TRIGGER;
	result = set_inv_enable(indio_dev);

	return result;
}

/*
 * inv_misc_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_misc_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _misc_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

static ssize_t _debug_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	if (!st->debug_determine_engine_on)
		return -EINVAL;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE:
		INVLOG(IL6, "ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE\n");
		st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on = !!data;
		break;
	case ATTR_DMP_ACCEL_CAL_ENABLE:
		INVLOG(IL6, "ATTR_DMP_ACCEL_CAL_ENABLE\n");
		st->accel_cal_enable = !!data;
		break;
	case ATTR_DMP_EVENT_INT_ON:
		INVLOG(IL6, "ATTR_DMP_EVENT_INT_ON\n");
		st->chip_config.dmp_event_int_on = !!data;
		break;
	case ATTR_DMP_ON:
		INVLOG(IL6, "ATTR_DMP_ON\n");
		st->chip_config.dmp_on = !!data;
		break;
	case ATTR_ACCEL_ENABLE:
		INVLOG(IL4, "ATTR_ACCEL_ENABLE\n");
		st->chip_config.accel_enable = !!data;
		if (st->chip_config.accel_enable)
			accel_open_calibration(st);
		break;
	default:
		return -EINVAL;
	}
	st->trigger_state = DEBUG_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	return count;
}

/*
 * inv_debug_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_debug_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _debug_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static int inv_rate_convert(struct inv_mpu_state *st, int ind, int data)
{
	int t, out, out1, out2;

	out = MPU_INIT_SENSOR_RATE;
	{
		t = MPU_DEFAULT_DMP_FREQ / data;
		if (!t)
			t = 1;
		out1 = MPU_DEFAULT_DMP_FREQ / (t + 1);
		out2 = MPU_DEFAULT_DMP_FREQ / t;
		if (abs(out1 - data) < abs(out2 - data))
			out = out1;
		else
			out = out2;
	}

	return out;
}

static ssize_t inv_sensor_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return sprintf(buf, "%d\n", st->sensor_l[this_attr->address].rate);
}

static ssize_t inv_sensor_rate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, rate, ind;
	int result;

	if (!st->chip_config.firmware_loaded) {
		INVLOG(ERR, "sensor_rate_store: firmware not loaded\n");
		return -EINVAL;
	}
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data <= 0) {
		INVLOG(ERR, "sensor_rate_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	rate = inv_rate_convert(st, ind, data);
	if (rate == st->sensor_l[ind].rate)
		return count;
	mutex_lock(&indio_dev->mlock);
	st->sensor_l[ind].rate = rate;
	st->trigger_state = DATA_TRIGGER;
	inv_check_sensor_rate(st);
	inv_check_sensor_on(st);
	result = set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

static ssize_t inv_sensor_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return sprintf(buf, "%d\n", st->sensor_l[this_attr->address].on);
}

static ssize_t inv_sensor_on_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, on, ind;
	int result;

	INVLOG(FUNC_INT_ENTRY, "Enter");
	if (!st->chip_config.firmware_loaded) {
		pr_err("sensor_on store: firmware not loaded\n");
		return -EINVAL;
	}
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data < 0) {
		pr_err("sensor_on_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	on = !!data;
	if (on == st->sensor_l[ind].on)
		return count;
	mutex_lock(&indio_dev->mlock);
	st->sensor_l[ind].on = on;
	st->trigger_state = RATE_TRIGGER;
	inv_check_sensor_on(st);
	inv_check_sensor_rate(st);
	if (on && (!st->sensor_l[ind].rate)) {
		mutex_unlock(&indio_dev->mlock);
		return count;
	}

	result = set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

static int inv_check_l_step(struct inv_mpu_state *st)
{
	if (st->step_counter_l_on || st->step_counter_wake_l_on)
		st->ped.on = true;
	else
		st->ped.on = false;

	return 0;
}
static ssize_t _basic_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	int result;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	result = kstrtoint(buf, 10, &data);
	if (result || (data < 0))
		return -EINVAL;

	switch (this_attr->address) {
	case ATTR_DMP_PED_ON:
		INVLOG(IL6, "ATTR_DMP_PED_ON\n");
		if ((!!data) == st->ped.on)
			return count;
		st->ped.on = !!data;
		INVLOG(IL3, "DMP_PED_ON ped.on %d\n", st->ped.on);
		break;
	case ATTR_DMP_SMD_ENABLE:
		INVLOG(IL6, "ATTR_DMP_SMD_ENABLE\n");
		if ((!!data) == st->smd.on)
			return count;
		st->smd.on = !!data;
		break;
	case ATTR_DMP_TILT_ENABLE:
		INVLOG(IL6, "ATTR_DMP_TILT_ENABLE\n");
		if ((!!data) == st->chip_config.tilt_enable)
			return count;
		st->chip_config.tilt_enable = !!data;
		break;
	case ATTR_DMP_PICK_UP_ENABLE:
		INVLOG(IL6, "ATTR_DMP_PICK_UP_ENABLE\n");
		if ((!!data) == st->chip_config.pick_up_enable)
			return count;
		st->chip_config.pick_up_enable = !!data;
		break;
	case ATTR_DMP_STEP_DETECTOR_ON:
		INVLOG(IL6, "ATTR_DMP_STEP_DETECTOR_ON\n");
		st->step_detector_l_on = !!data;
		break;
	case ATTR_DMP_STEP_DETECTOR_WAKE_ON:
		INVLOG(IL6, "ATTR_DMP_STEP_DETECTOR_WAKE_ON\n");
		st->step_detector_wake_l_on = !!data;
		break;
	case ATTR_DMP_ACTIVITY_ON:
		INVLOG(IL6, "ATTR_DMP_ACTIVITY_ON\n");
		if ((!!data) == st->chip_config.activity_on)
			return count;
		st->chip_config.activity_on = !!data;
		break;
	case ATTR_DMP_STEP_COUNTER_ON:
		INVLOG(IL6, "ATTR_DMP_STEP_COUNTER_ON\n");
		st->step_counter_l_on = !!data;
		break;
	case ATTR_DMP_STEP_COUNTER_WAKE_ON:
		INVLOG(IL6, "ATTR_DMP_STEP_COUNTER_WAKE_ON\n");
		st->step_counter_wake_l_on = !!data;
		break;
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		if (data == st->batch.timeout)
			return count;
		st->batch.timeout = data;
		break;
	default:
		return -EINVAL;
	};
	inv_check_l_step(st);
	inv_check_sensor_on(st);

	st->trigger_state = EVENT_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	return count;
}

/*
 * inv_basic_attr_store() -  calling this function will store current
 *                        non-dmp parameter settings
 */
static ssize_t inv_basic_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _basic_attr_store(dev, attr, buf, count);

	mutex_unlock(&indio_dev->mlock);

	return result;
}

/*
 * inv_attr_bias_show() -  calling this function will show current
 *                        dmp gyro/accel bias.
 */
static ssize_t _attr_bias_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int axes, addr, result, dmp_bias;
	int sensor_type;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	switch (this_attr->address) {
	case ATTR_ACCEL_X_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_bias[0]);
	case ATTR_ACCEL_Y_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_bias[1]);
	case ATTR_ACCEL_Z_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_bias[2]);
	case ATTR_DMP_ACCEL_X_DMP_BIAS:
		axes = 0;
		addr = ACCEL_BIAS_X;
		sensor_type = SENSOR_ACCEL;
		break;
	case ATTR_DMP_ACCEL_Y_DMP_BIAS:
		axes = 1;
		addr = ACCEL_BIAS_Y;
		sensor_type = SENSOR_ACCEL;
		break;
	case ATTR_DMP_ACCEL_Z_DMP_BIAS:
		axes = 2;
		addr = ACCEL_BIAS_Z;
		sensor_type = SENSOR_ACCEL;
		break;
	case ATTR_DMP_SC_AUTH:
		axes = 0;
		addr = st->aut_key_out;
		sensor_type = -1;
		break;
	default:
		return -EINVAL;
	}
	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	result = read_be32_from_mem(st, &dmp_bias, addr);
	if (result)
		return result;
	inv_switch_power_in_lp(st, false);
	if (SENSOR_ACCEL == sensor_type)
		st->input_accel_dmp_bias[axes] = dmp_bias;
	else if (sensor_type != -1)
		return -EINVAL;

	return sprintf(buf, "%d\n", dmp_bias);
}

static ssize_t inv_attr_bias_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _attr_bias_show(dev, attr, buf);

	mutex_unlock(&indio_dev->mlock);

	return result;
}

/*
 * inv_attr_show() -  calling this function will show current
 *                        dmp parameters.
 */
static ssize_t inv_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	s8 *m;

	switch (this_attr->address) {
	case ATTR_ACCEL_SCALE:
	{
		const s16 accel_scale[] = {2, 4, 8, 16};
		return sprintf(buf, "%d\n",
					accel_scale[st->chip_config.accel_fs]);
	}
	case ATTR_ACCEL_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.accel_enable);
	case ATTR_DMP_ACCEL_CAL_ENABLE:
		return sprintf(buf, "%d\n", st->accel_cal_enable);
	case ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON:
		return sprintf(buf, "%d\n", st->debug_determine_engine_on);
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD:
		return sprintf(buf, "%d\n", st->accel_calib_threshold);
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE:
		return sprintf(buf, "%d\n", st->accel_calib_rate);
	case ATTR_FIRMWARE_LOADED:
		return sprintf(buf, "%d\n", st->chip_config.firmware_loaded);
	case ATTR_DMP_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_on);
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		return sprintf(buf, "%d\n", st->batch.timeout);
	case ATTR_DMP_EVENT_INT_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_event_int_on);
	case ATTR_DMP_PED_ON:
		return sprintf(buf, "%d\n", st->ped.on);
	case ATTR_DMP_PED_STEP_THRESH:
		return sprintf(buf, "%d\n", st->ped.step_thresh);
	case ATTR_DMP_PED_INT_THRESH:
		return sprintf(buf, "%d\n", st->ped.int_thresh);
	case ATTR_DMP_SMD_ENABLE:
		return sprintf(buf, "%d\n", st->smd.on);
	case ATTR_DMP_TILT_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.tilt_enable);
	case ATTR_DMP_PICK_UP_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.pick_up_enable);
	case ATTR_DMP_LP_EN_OFF:
		return sprintf(buf, "%d\n", st->chip_config.lp_en_mode_off);
	case ATTR_DMP_STEP_COUNTER_ON:
		return sprintf(buf, "%d\n", st->step_counter_l_on);
	case ATTR_DMP_STEP_COUNTER_WAKE_ON:
		return sprintf(buf, "%d\n", st->step_counter_wake_l_on);
	case ATTR_DMP_STEP_DETECTOR_ON:
		return sprintf(buf, "%d\n", st->step_detector_l_on);
	case ATTR_DMP_STEP_DETECTOR_WAKE_ON:
		return sprintf(buf, "%d\n", st->step_detector_wake_l_on);
	case ATTR_DMP_ACTIVITY_ON:
		return sprintf(buf, "%d\n", st->chip_config.activity_on);
	case ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE:
		return sprintf(buf, "%d\n",
				st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on);
	case ATTR_ACCEL_MATRIX:
		m = st->plat_data.orientation;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_DMP_DEBUG_MEM_READ:
	{
		int out;

		inv_switch_power_in_lp(st, true);
		result = read_be32_from_mem(st, &out, debug_mem_read_addr);
		if (result)
			return result;
		inv_switch_power_in_lp(st, false);
		return sprintf(buf, "0x%x\n", out);
	}
	case ATTR_ACCEL_X_ST_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_st_bias[0]);
	case ATTR_ACCEL_Y_ST_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_st_bias[1]);
	case ATTR_ACCEL_Z_ST_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_st_bias[2]);
	case ATTR_ACCEL_X_OFFSET:
		return sprintf(buf, "%d\n", st->input_accel_bias[0]);
	case ATTR_ACCEL_Y_OFFSET:
		return sprintf(buf, "%d\n", st->input_accel_bias[1]);
	case ATTR_ACCEL_Z_OFFSET:
		return sprintf(buf, "%d\n", st->input_accel_bias[2]);
	default:
		return -EPERM;
	}
}

static ssize_t inv_attr64_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	u64 tmp;
	u32 ped;
	INVLOG(IL6, "Enter\n");

	mutex_lock(&indio_dev->mlock);
	if (!st->chip_config.dmp_on) {
		mutex_unlock(&indio_dev->mlock);
		return -EINVAL;
	}
	result = 0;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		inv_switch_power_in_lp(st, true);
		result = inv_get_pedometer_steps(st, &ped);
		result |= inv_read_pedometer_counter(st);
		tmp = (u64)st->ped.step + (u64)ped;
		/********************************************************************/
		/* Pedo logging mode feature                                        */ 
		if(tmp != st->pedlog.step_count) {
			/*if step count is changed*/
			bool needs_to_complete = false;

			INVLOG(IL4, "pedlog step counter = %lld\n", tmp );
			if(!st->pedlog.enabled) {
				st->pedlog.interrupt_mask |=
						PEDLOG_INT_STEP_COUNTER;

				/*reset timer on walking*/
				del_timer(&st->pedlog.timer);
				st->pedlog.timer.expires = jiffies + 2*HZ;
				add_timer(&st->pedlog.timer);

				needs_to_complete = true;
			}

			st->pedlog.step_count = tmp;

			if(st->pedlog.state == PEDLOG_STAT_STOP) {
				st->pedlog.state = PEDLOG_STAT_WALK;
				if(!st->pedlog.enabled) {
					st->pedlog.interrupt_mask |=
						PEDLOG_INT_START_WALKING;
					needs_to_complete = true;
				}
			}

			if(needs_to_complete)
				complete(&st->pedlog.wait);
		}
		/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

		inv_switch_power_in_lp(st, false);
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		inv_switch_power_in_lp(st, true);
		result = inv_get_pedometer_time(st, &ped);
		tmp = (u64)st->ped.time + ((u64)ped) * MS_PER_PED_TICKS;
		inv_switch_power_in_lp(st, false);
		break;
	case ATTR_DMP_PEDOMETER_COUNTER:
		tmp = st->ped.last_step_time;
		break;
	default:
		tmp = 0;
		result = -EINVAL;
		break;
	}

	mutex_unlock(&indio_dev->mlock);
	if (result)
		return -EINVAL;

	return sprintf(buf, "%lld\n", tmp);
}

static ssize_t inv_attr64_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	u8 d[4] = {0, 0, 0, 0};
	u64 data;
	INVLOG(IL6, "Enter\n");

	mutex_lock(&indio_dev->mlock);
	if (!st->chip_config.firmware_loaded) {
		mutex_unlock(&indio_dev->mlock);
		return -EINVAL;
	}
	result = inv_switch_power_in_lp(st, true);
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}
	result = kstrtoull(buf, 10, &data);
	if (result)
		goto attr64_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		result = mem_w(PEDSTD_STEPCTR, ARRAY_SIZE(d), d);
		if (result)
			goto attr64_store_fail;
		st->ped.step = data;
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		result = mem_w(PEDSTD_TIMECTR, ARRAY_SIZE(d), d);
		if (result)
			goto attr64_store_fail;
		st->ped.time = data;
		break;
	default:
		result = -EINVAL;
		break;
	}
attr64_store_fail:
	mutex_unlock(&indio_dev->mlock);
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}

/*--> Pedlog */
static ssize_t inv_pedlog_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	result = inv_switch_power_in_lp(st, true);

	switch (this_attr->address) {
	case ATTR_DMP_PEDLOG_CADENCE:
	{
		int i = 0;
		char concat[256];
		s32 cadence = 0;
		s64 start_time = 0, end_time = 0;

		if(st->pedlog.enabled)
			start_time = st->pedlog.start_time;

		st->pedlog.start_time = 
				st->pedlog.interrupt_time;
		end_time = st->pedlog.interrupt_time;

		/*timestamps*/
		sprintf(concat, "%lld,%lld,", start_time, end_time);
		strcat(buf, concat);

		/*valid count of cadence*/
		sprintf(concat, "%d,", st->pedlog.valid_count);
		strcat(buf, concat);

		for( i = 0; i < PEDLOG_CADENCE_LEN; i++) {
			cadence = st->pedlog.cadence[i];
			sprintf(concat, "%u,", cadence);
			strcat(buf, concat);
		}

		strcat(buf, "\n");
		
		pr_info("[INV] Cadence Read : %s\n", buf);

		return strlen(buf);
	}

	case ATTR_DMP_PEDLOG_ENABLE:
	{
		return sprintf(buf, "%d\n", st->pedlog.enabled);
	}

	case ATTR_DMP_PEDLOG_INTERRUPT_PERIOD:
	{
		return sprintf(buf, "%d\n", st->pedlog.interrupt_duration);
	}

	case ATTR_DMP_PEDLOG_INSTANT_CADENCE:
	{
		pr_info("[INVN:%s] Instant Cadence : %s", __func__, buf);
		return inv_get_pedlog_instant_cadence(st, buf);
	}
	case ATTR_DMP_PEDLOG_FLUSH_CADENCE:
	{
		result = inv_get_pedlog_instant_cadence(st, buf);
		pr_info("[INVN:%s] Cadence Flush : %s\n", __func__, buf);
		inv_clear_pedlog_cadence(st);

		/* WHY is start_timestamp get new time after flushing? */
		st->pedlog.start_time = get_time_timeofday();
		st->pedlog.stop_time = -1;
		st->pedlog.interrupt_time = -1;

		/* reset interrupt period */		
		inv_set_pedlog_interrupt_period(st,
				st->pedlog.interrupt_duration);
		/* reset cadence update timer */
		inv_reset_pedlog_update_timer(st);

		return strlen(buf);
	}
	case ATTR_DMP_PEDLOG_TIMER:
	{
		inv_get_pedlog_update_timer(st, buf);
		pr_info("[INVN:%s] Update Timer : %s\n", __func__, buf);
		return strlen(buf);
	}

	result = inv_switch_power_in_lp(st, false);
	default:
		return -EPERM;
	}
}
/*<-- Pedlog */

/*--> gesture */
static ssize_t inv_gesture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result = 0;
	u8 d[4] = {0, 0, 0, 0};
	int data;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	mutex_lock(&indio_dev->mlock);
	result = kstrtoint(buf, 10, &data);
	switch(this_attr->address)
	{
	case ATTR_WOM_ENABLE:
		st->wom_enable = data;
		INVLOG(IL4, "data %d\n", data);
		if(data)
		{
			u8 mask;
			// set bank 0
			inv_set_bank(st, BANK_SEL_0);	// select reg bank 0	

			// PWR_MGMT_1:  Sleep = 0, gyro_standby = 0
			mask = BIT_SLEEP; 		// these are the bits we need to clear
			result = inv_plat_read(st, REG_PWR_MGMT_1, 1, d);
			INVLOG(IL4, "REG_PWR_MGMT_1 0x%x\n", d[0]);
			if(d[0] & mask)	// if either sleep or gyro standby enabled
			{
				d[0] &= ~mask;			// clear the bits then write the register
				result = inv_plat_single_write(st, REG_PWR_MGMT_1, d[0]);
				mdelay(1);			  // wait at least 100usec.... 
				if(result)
				{
					mutex_unlock(&indio_dev->mlock);
					INVLOG(ERR, "Fail to write to REG_PWR_MGMT_1\n");
					return result;
				}
			}

			// PWR_MGMT_2:  accel on.  Should have gyro off, but not forcing that
			result = inv_plat_read(st, REG_PWR_MGMT_2, 1, d);		  
			d[0] &= ~BIT_PWR_ACCEL_STBY;

			result += inv_plat_single_write(st, REG_PWR_MGMT_2, d[0]);
			if(result)
			{
				mutex_unlock(&indio_dev->mlock);
				INVLOG(ERR, "Fail to write to REG_PWR_MGMT_2\n");
				return result;
			}

			// must have ACCEL_CONFIG register set with fchoice = 0.	
			// This is a bank 2 register -- function changes to bank 2.  Change back to bank 0 when done
			st->chip_config.accel_fs = 1;
			result = inv_set_accel_sf(st);	//MPU_FILTER_20HZ, 4g,0); 

			if(result)
			{
				mutex_unlock(&indio_dev->mlock);
				INVLOG(ERR, "Fail to inv_set_accel_sf\n");
				return result;
			}

			// set bank 0
			inv_set_bank(st, BANK_SEL_0);	// select reg bank 0	  

			// Enable WOM interrupt
			result = inv_plat_read(st, REG_INT_ENABLE, 1, d);
			INVLOG(IL4, "REG_INT_ENABLE 0x%x\n", d[0]);
			d[0] |= BIT_WOM_INT_EN;

			result += inv_plat_single_write(st, REG_INT_ENABLE, d[0]);
			if(result)
			{
				mutex_unlock(&indio_dev->mlock);
				INVLOG(ERR, "Fail to write to REG_INT_ENABLE\n");
				return result;
			}

			// bank 2
			inv_set_bank(st, BANK_SEL_2);	// select reg bank 2	  
			d[0] = 500 / 4;
			result = inv_plat_single_write(st, REG_ACCEL_WOM_THR, d[0]);

			d[0] = BIT_ACCEL_INTEL_EN | BIT_ACCEL_INTEL_MODE_INT;
			result += inv_plat_single_write(st, REG_ACCEL_INTEL_CTRL, d[0]);
			inv_set_bank(st, BANK_SEL_0);	// select reg bank 0

			if(result)
			{
				mutex_unlock(&indio_dev->mlock);
				INVLOG(ERR, "Fail to write to REG_ACCEL_INTEL_CTRL\n");
				return result;
			}

			// set smplrt div (already set)
			// set lpconfig  ACCEL CYCLE (already set)
		}
		else
		{
			result = inv_plat_read(st, REG_INT_ENABLE, 1, d);
			INVLOG(IL4, "REG_INT_ENABLE 0x%x\n", d[0]);

			d[0] &= ~BIT_WOM_INT_EN;

			result += inv_plat_single_write(st, REG_INT_ENABLE, d[0]);

			if(result)
			{
				mutex_unlock(&indio_dev->mlock);
				INVLOG(ERR, "Fail to write to REG_INT_ENABLE\n");
				return result;
			}
		}

		break;
	default:
		break;
	}
	mutex_unlock(&indio_dev->mlock);

	INVLOG(FUNC_INT_ENTRY, "Exit\n");
	return count;
}

static ssize_t inv_gesture_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result = 0;
	u8 d[4] = {0, 0, 0, 0};
	INVLOG(IL4, "Enter argument %lld\n", this_attr->address);

	switch (this_attr->address) {
	case ATTR_WOM_ENABLE:
		result = mem_r(REG_INT_ENABLE, 2, d);
		INVLOG(IL6, "REG_INT_ENABLE d0:0x%x d1:0x%x\n", d[0], d[1]);
		if(d[0] & BIT_WOM_INT_EN)
		{
			INVLOG(IL6, "WOM is enable!!\n");
		}
		else
		{
			INVLOG(IL6, "WOM is disable!!\n");
		}
		break;
	default:
		break;

	}
	return result;
}
/*<-- gesture */
static ssize_t inv_self_test(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int res;

	mutex_lock(&indio_dev->mlock);
	inv_switch_power_in_lp(st, true);
	res = inv_hw_self_test(st);
	inv_switch_power_in_lp(st, false);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return sprintf(buf, "%d\n", res);
}

/*
 *  inv_temperature_show() - Read temperature data directly from registers.
 */
static ssize_t inv_temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result, scale_t;
	short temp;
	u8 data[2];
	INVLOG(IL6, "Enter\n");

	mutex_lock(&indio_dev->mlock);
	result = inv_switch_power_in_lp(st, true);
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}

	result = inv_plat_read(st, REG_TEMPERATURE, 2, data);
	mutex_unlock(&indio_dev->mlock);
	if (result) {
		pr_err("Could not read temperature register.\n");
		return result;
	}
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;
	temp = (s16)(be16_to_cpup((short *)&data[0]));
	scale_t = TEMPERATURE_OFFSET +
		inv_q30_mult((int)temp << MPU_TEMP_SHIFT, TEMPERATURE_SCALE);

//	INV_I2C_INC_TEMPREAD(1);

	return sprintf(buf, "%d %lld\n", scale_t, get_time_ns());
}

/*
 * inv_smd_show() -  calling this function showes smd interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_smd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

/*
 * inv_ped_show() -  calling this function showes pedometer interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_ped_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	INVLOG(IL4, "PEDOMETER interrupt occured.\n");
	return sprintf(buf, "1\n");
}
static ssize_t inv_activity_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	INVLOG(IL4, "ACTIVITY interrupt occured.\n");
	return sprintf(buf, "%d\n", st->activity_size);
}
static ssize_t inv_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	INVLOG(IL4, "TILT interrupt occured.\n");
	return sprintf(buf, "1\n");
}

static ssize_t inv_pick_up_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	INVLOG(IL4, "PICK-UP interrupt occured.\n");
	return sprintf(buf, "1\n");
}

/*
 *  inv_reg_dump_show() - Register dump for testing.
 */
static ssize_t inv_reg_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ii;
	char data;
	ssize_t bytes_printed = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);

	mutex_lock(&indio_dev->mlock);
	inv_set_bank(st, BANK_SEL_0);
	bytes_printed += sprintf(buf + bytes_printed, "bank 0\n");

	for (ii = 0; ii < 0x7F; ii++) {
		/* don't read fifo r/w register */
		if ((ii == REG_MEM_R_W) || (ii == REG_FIFO_R_W))
			data = 0;
		else
			inv_plat_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	inv_set_bank(st, BANK_SEL_1);
	bytes_printed += sprintf(buf + bytes_printed, "bank 1\n");
	for (ii = 0; ii < 0x2A; ii++) {
		inv_plat_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	inv_set_bank(st, BANK_SEL_2);
	bytes_printed += sprintf(buf + bytes_printed, "bank 2\n");
	for (ii = 0; ii < 0x55; ii++) {
		inv_plat_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	inv_set_bank(st, BANK_SEL_3);
	bytes_printed += sprintf(buf + bytes_printed, "bank 3\n");
	for (ii = 0; ii < 0x18; ii++) {
		inv_plat_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	inv_set_bank(st, BANK_SEL_0);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return bytes_printed;
}

static ssize_t inv_flush_batch_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;

	mutex_lock(&indio_dev->mlock);
	result = inv_flush_batch_data(indio_dev, data);
	mutex_unlock(&indio_dev->mlock);

	return count;
}

static const struct iio_chan_spec inv_mpu_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_MPU_SCAN_TIMESTAMP),
};

static DEVICE_ATTR(poll_smd, S_IRUGO, inv_smd_show, NULL);
static DEVICE_ATTR(poll_pedometer, S_IRUGO, inv_ped_show, NULL);
static DEVICE_ATTR(poll_activity, S_IRUGO, inv_activity_show, NULL);
static DEVICE_ATTR(poll_tilt, S_IRUGO, inv_tilt_show, NULL);
static DEVICE_ATTR(poll_pick_up, S_IRUGO, inv_pick_up_show, NULL);

/* special run time sysfs entry, read only */
static DEVICE_ATTR(debug_reg_dump, S_IRUGO | S_IWUGO, inv_reg_dump_show, NULL);
static DEVICE_ATTR(out_temperature, S_IRUGO | S_IWUGO,
						inv_temperature_show, NULL);
static DEVICE_ATTR(misc_self_test, S_IRUGO | S_IWUGO, inv_self_test, NULL);
/*****************************************************************************/
/*      SysFS for Pedo logging mode features                                 */
#ifdef FIRE_FEATURE
static DEVICE_ATTR(event_shealth_int, S_IRUGO, inv_pedlog_int_show, NULL);
#else
static DEVICE_ATTR(event_pedlog_int, S_IRUGO, inv_pedlog_int_show, NULL);
#endif
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

static IIO_DEVICE_ATTR(info_accel_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_ACCEL_MATRIX);
/* write only sysfs */
static DEVICE_ATTR(misc_flush_batch, S_IWUGO, NULL, inv_flush_batch_store);

/* sensor on/off sysfs control */
static IIO_DEVICE_ATTR(in_accel_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_L_ACCEL);
static IIO_DEVICE_ATTR(in_accel_wake_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_L_ACCEL_WAKE);
/* sensor rate sysfs control */
static IIO_DEVICE_ATTR(in_accel_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_L_ACCEL);
static IIO_DEVICE_ATTR(in_accel_wake_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_L_ACCEL_WAKE);
/* debug determine engine related sysfs */
static IIO_DEVICE_ATTR(debug_accel_accuracy_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_cal_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_ACCEL_CAL_ENABLE);

static IIO_DEVICE_ATTR(debug_accel_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_ACCEL_ENABLE);
static IIO_DEVICE_ATTR(debug_dmp_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_ON);
static IIO_DEVICE_ATTR(debug_dmp_event_int_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_EVENT_INT_ON);
static IIO_DEVICE_ATTR(debug_mem_read, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_DEBUG_MEM_READ);
static IIO_DEVICE_ATTR(debug_mem_write, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_DEBUG_MEM_WRITE);

static IIO_DEVICE_ATTR(misc_batchmode_timeout, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_BATCHMODE_TIMEOUT);

static IIO_DEVICE_ATTR(info_firmware_loaded, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_firmware_loaded_store, ATTR_FIRMWARE_LOADED);

/* engine scale */
static IIO_DEVICE_ATTR(in_accel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_misc_attr_store, ATTR_ACCEL_SCALE);
static IIO_DEVICE_ATTR(debug_lp_en_off, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DMP_LP_EN_OFF);
static IIO_DEVICE_ATTR(debug_clock_sel, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DMP_CLK_SEL);
static IIO_DEVICE_ATTR(debug_reg_write, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DEBUG_REG_WRITE);
static IIO_DEVICE_ATTR(debug_cfg_write, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DEBUG_WRITE_CFG);
static IIO_DEVICE_ATTR(debug_reg_write_addr, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DEBUG_REG_ADDR);

static IIO_DEVICE_ATTR(in_accel_x_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ACCEL_X_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ACCEL_Y_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ACCEL_Z_CALIBBIAS);


static IIO_DEVICE_ATTR(in_accel_x_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_X_DMP_BIAS);
static IIO_DEVICE_ATTR(in_accel_y_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_Y_DMP_BIAS);
static IIO_DEVICE_ATTR(in_accel_z_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_Z_DMP_BIAS);


static IIO_DEVICE_ATTR(in_accel_x_st_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_show, NULL, ATTR_ACCEL_X_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_st_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_show, NULL, ATTR_ACCEL_Y_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_st_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_show, NULL, ATTR_ACCEL_Z_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_accel_x_offset, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_bias_store, ATTR_ACCEL_X_OFFSET);
static IIO_DEVICE_ATTR(in_accel_y_offset, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_bias_store, ATTR_ACCEL_Y_OFFSET);
static IIO_DEVICE_ATTR(in_accel_z_offset, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_bias_store, ATTR_ACCEL_Z_OFFSET);


static IIO_DEVICE_ATTR(in_sc_auth, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_SC_AUTH);

static IIO_DEVICE_ATTR(debug_determine_engine_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON);
static IIO_DEVICE_ATTR(misc_accel_recalibration, S_IRUGO | S_IWUGO, NULL,
	inv_dmp_bias_store, ATTR_DMP_MISC_ACCEL_RECALIBRATION);
static IIO_DEVICE_ATTR(params_accel_calibration_threshold, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store,
			ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD);
static IIO_DEVICE_ATTR(params_accel_calibration_rate, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store,
				ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE);

static IIO_DEVICE_ATTR(in_step_detector_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_STEP_DETECTOR_ON);
static IIO_DEVICE_ATTR(in_step_detector_wake_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_STEP_DETECTOR_WAKE_ON);
static IIO_DEVICE_ATTR(in_step_counter_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_STEP_COUNTER_ON);
static IIO_DEVICE_ATTR(in_step_counter_wake_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_STEP_COUNTER_WAKE_ON);
static IIO_DEVICE_ATTR(in_activity_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_ACTIVITY_ON);

static IIO_DEVICE_ATTR(event_smd_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_SMD_ENABLE);
static IIO_DEVICE_ATTR(event_tilt_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_TILT_ENABLE);
static IIO_DEVICE_ATTR(event_pick_up_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_PICK_UP_ENABLE);
static IIO_DEVICE_ATTR(event_pedometer_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_PED_ON);
static IIO_DEVICE_ATTR(params_pedometer_step_thresh, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_PED_STEP_THRESH);
static IIO_DEVICE_ATTR(params_pedometer_int_thresh, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_PED_INT_THRESH);

static IIO_DEVICE_ATTR(pedometer_steps, S_IRUGO | S_IWUGO, inv_attr64_show,
	inv_attr64_store, ATTR_DMP_PEDOMETER_STEPS);
static IIO_DEVICE_ATTR(pedometer_time, S_IRUGO | S_IWUGO, inv_attr64_show,
	inv_attr64_store, ATTR_DMP_PEDOMETER_TIME);
static IIO_DEVICE_ATTR(pedometer_counter, S_IRUGO | S_IWUGO,
			inv_attr64_show, NULL, ATTR_DMP_PEDOMETER_COUNTER);

/*****************************************************************************/
/*      SysFS for Pedo logging mode features                                 */
#ifdef FIRE_FEATURE
static IIO_DEVICE_ATTR(shealth_cadence, S_IRUGO | S_IWUGO, inv_pedlog_show,
	NULL, ATTR_DMP_PEDLOG_CADENCE);
static IIO_DEVICE_ATTR(shealth_cadence_enable, S_IRUGO | S_IWUGO, inv_pedlog_show,
	inv_dmp_pedlog_store, ATTR_DMP_PEDLOG_ENABLE);
static IIO_DEVICE_ATTR(shealth_instant_cadence, S_IRUGO | S_IWUGO,
		inv_pedlog_show, NULL, ATTR_DMP_PEDLOG_INSTANT_CADENCE);
static IIO_DEVICE_ATTR(shealth_flush_cadence, S_IRUGO | S_IWUGO, inv_pedlog_show,
	NULL, ATTR_DMP_PEDLOG_FLUSH_CADENCE);
static IIO_DEVICE_ATTR(shealth_int_period, S_IRUGO | S_IWUGO, inv_pedlog_show,
	inv_dmp_pedlog_store, ATTR_DMP_PEDLOG_INTERRUPT_PERIOD);
static IIO_DEVICE_ATTR(shealth_timer, S_IRUGO | S_IWUGO, inv_pedlog_show,
	inv_dmp_pedlog_store, ATTR_DMP_PEDLOG_TIMER);
#else
static IIO_DEVICE_ATTR(pedlog_cadence, S_IRUGO | S_IWUGO, inv_pedlog_show,
	NULL, ATTR_DMP_PEDLOG_CADENCE);
static IIO_DEVICE_ATTR(pedlog_cadence_enable, S_IRUGO | S_IWUGO, inv_pedlog_show,
	inv_dmp_pedlog_store, ATTR_DMP_PEDLOG_ENABLE);
static IIO_DEVICE_ATTR(pedlog_instant_cadence, S_IRUGO | S_IWUGO,
		inv_attr_show, NULL, ATTR_DMP_PEDLOG_INSTANT_CADENCE);
static IIO_DEVICE_ATTR(pedlog_flush_cadence, S_IRUGO | S_IWUGO, inv_pedlog_show,
	NULL, ATTR_DMP_PEDLOG_FLUSH_CADENCE);
static IIO_DEVICE_ATTR(pedlog_int_period, S_IRUGO | S_IWUGO, inv_pedlog_show,
	inv_dmp_pedlog_store, ATTR_DMP_PEDLOG_INTERRUPT_PERIOD);
static IIO_DEVICE_ATTR(pedlog_timer, S_IRUGO | S_IWUGO, inv_pedlog_show,
	inv_dmp_pedlog_store, ATTR_DMP_PEDLOG_TIMER);
#endif	// #ifdef FIRE_FEATURE

/* WOM */
static IIO_DEVICE_ATTR(wom_enable, S_IRUGO | S_IWUGO, inv_gesture_show,
	inv_gesture_store, ATTR_WOM_ENABLE);
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

static const struct attribute *inv_raw_attributes[] = {
	&dev_attr_debug_reg_dump.attr,
	&dev_attr_out_temperature.attr,
	&dev_attr_misc_flush_batch.attr,
	&dev_attr_misc_self_test.attr,
	&iio_dev_attr_in_sc_auth.dev_attr.attr,
	&iio_dev_attr_in_accel_enable.dev_attr.attr,
	&iio_dev_attr_in_accel_wake_enable.dev_attr.attr,
	&iio_dev_attr_info_accel_matrix.dev_attr.attr,
	&iio_dev_attr_in_accel_scale.dev_attr.attr,
	&iio_dev_attr_info_firmware_loaded.dev_attr.attr,
	&iio_dev_attr_misc_batchmode_timeout.dev_attr.attr,
	&iio_dev_attr_in_accel_rate.dev_attr.attr,
	&iio_dev_attr_in_accel_wake_rate.dev_attr.attr,
	&iio_dev_attr_debug_mem_read.dev_attr.attr,
	&iio_dev_attr_debug_mem_write.dev_attr.attr,
};

static const struct attribute *inv_debug_attributes[] = {
	&iio_dev_attr_debug_accel_enable.dev_attr.attr,
	&iio_dev_attr_debug_dmp_event_int_on.dev_attr.attr,
	&iio_dev_attr_debug_lp_en_off.dev_attr.attr,
	&iio_dev_attr_debug_clock_sel.dev_attr.attr,
	&iio_dev_attr_debug_reg_write.dev_attr.attr,
	&iio_dev_attr_debug_reg_write_addr.dev_attr.attr,
	&iio_dev_attr_debug_cfg_write.dev_attr.attr,
	&iio_dev_attr_debug_dmp_on.dev_attr.attr,
	&iio_dev_attr_debug_accel_cal_enable.dev_attr.attr,
	&iio_dev_attr_debug_accel_accuracy_enable.dev_attr.attr,
	&iio_dev_attr_debug_determine_engine_on.dev_attr.attr,
	&iio_dev_attr_params_pedometer_step_thresh.dev_attr.attr,
	&iio_dev_attr_params_pedometer_int_thresh.dev_attr.attr,
	&iio_dev_attr_misc_accel_recalibration.dev_attr.attr,
	&iio_dev_attr_params_accel_calibration_threshold.dev_attr.attr,
	&iio_dev_attr_params_accel_calibration_rate.dev_attr.attr,
};



static const struct attribute *inv_bias_attributes[] = {
	&iio_dev_attr_in_accel_x_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_x_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_calibbias.dev_attr.attr,
};


static const struct attribute *inv_bias_st_attributes[] = {
	&iio_dev_attr_in_accel_x_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_z_offset.dev_attr.attr,
};
static const struct attribute *inv_pedometer_attributes[] = {
	&dev_attr_poll_pedometer.attr,
	&dev_attr_poll_activity.attr,
	&dev_attr_poll_tilt.attr,
	&dev_attr_poll_pick_up.attr,
	&iio_dev_attr_event_pedometer_enable.dev_attr.attr,
	&iio_dev_attr_event_tilt_enable.dev_attr.attr,
	&iio_dev_attr_event_pick_up_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_step_detector_enable.dev_attr.attr,
	&iio_dev_attr_in_step_detector_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_activity_enable.dev_attr.attr,
	&iio_dev_attr_pedometer_steps.dev_attr.attr,
	&iio_dev_attr_pedometer_time.dev_attr.attr,
	&iio_dev_attr_pedometer_counter.dev_attr.attr,
};

/*--> Pedolog sysfs node */
static const struct attribute *inv_pedolog_attributes[] = {
#ifdef FIRE_FEATURE
	&dev_attr_event_shealth_int.attr,
	&iio_dev_attr_shealth_cadence.dev_attr.attr,
	&iio_dev_attr_shealth_cadence_enable.dev_attr.attr,
	&iio_dev_attr_shealth_instant_cadence.dev_attr.attr,
	&iio_dev_attr_shealth_flush_cadence.dev_attr.attr,
	&iio_dev_attr_shealth_int_period.dev_attr.attr,
	&iio_dev_attr_shealth_timer.dev_attr.attr,
#else
	&dev_attr_event_pedlog_int.attr,
	&iio_dev_attr_pedlog_cadence.dev_attr.attr,
	&iio_dev_attr_pedlog_cadence_enable.dev_attr.attr,
	&iio_dev_attr_pedlog_instant_cadence.dev_attr.attr,
	&iio_dev_attr_pedlog_flush_cadence.dev_attr.attr,
	&iio_dev_attr_pedlog_int_period.dev_attr.attr,
	&iio_dev_attr_pedlog_timer.dev_attr.attr,
#endif
};
/*<-- Pedolog sysfs node */

/*--> WOM sysfs node */
static const struct attribute *inv_wom_attributes[] = {
	&iio_dev_attr_wom_enable.dev_attr.attr,
};
/*<-- WOM sysfs node */

static const struct attribute *inv_smd_attributes[] = {
	&dev_attr_poll_smd.attr,
	&iio_dev_attr_event_smd_enable.dev_attr.attr,
};


static struct attribute *inv_attributes[
	ARRAY_SIZE(inv_raw_attributes) +
	ARRAY_SIZE(inv_pedometer_attributes) +
	ARRAY_SIZE(inv_pedolog_attributes) +	/* Pedolog sysfs node */
	ARRAY_SIZE(inv_wom_attributes) +	/* WOM sysfs node */
	ARRAY_SIZE(inv_smd_attributes) +
	ARRAY_SIZE(inv_bias_attributes) +
	ARRAY_SIZE(inv_debug_attributes) +
	1
];

static const struct attribute_group inv_attribute_group = {
	.name = "mpu",
	.attrs = inv_attributes
};

static const struct iio_info mpu_info = {
	.driver_module = THIS_MODULE,
	.attrs = &inv_attribute_group,
};

/*
 *  inv_check_chip_type() - check and setup chip type.
 */
int inv_check_chip_type(struct iio_dev *indio_dev, const char *name)
{
	int result;
	int t_ind;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;
	struct inv_mpu_state *st;

	INVLOG(IL4, "enter\n");
	st = iio_priv(indio_dev);
	conf = &st->chip_config;
	plat = &st->plat_data;

	if (!strcmp(name, "icm20645"))
		st->chip_type = ICM20645;
//	else if (!strcmp(name, "icm10340"))
//		st->chip_type = ICM10340;
	else if (!strcmp(name, "icm10320"))
		st->chip_type = ICM10320;
	else
		return -EPERM;
	if (ICM20645 == st->chip_type) {
		st->dmp_image_size = DMP_IMAGE_SIZE_20645;
		st->dmp_start_address = DMP_START_ADDR_20645;
		st->aut_key_in = SC_AUT_INPUT_20645;
		st->aut_key_out = SC_AUT_OUTPUT_20645;
	} else {
		st->dmp_image_size = DMP_IMAGE_SIZE_10320;
		st->dmp_start_address = DMP_START_ADDR_10320;
		st->aut_key_in = SC_AUT_INPUT_10320;
		st->aut_key_out = SC_AUT_OUTPUT_10320;
	}
	st->hw  = &hw_info[st->chip_type];
	result = inv_mpu_initialize(st);

	if (result)
		return result;

	t_ind = 0;
	memcpy(&inv_attributes[t_ind], inv_raw_attributes,
					sizeof(inv_raw_attributes));
	t_ind += ARRAY_SIZE(inv_raw_attributes);

	memcpy(&inv_attributes[t_ind], inv_pedometer_attributes,
					sizeof(inv_pedometer_attributes));
	t_ind += ARRAY_SIZE(inv_pedometer_attributes);

	/*--> Pedolog sysfs attribute */
	memcpy(&inv_attributes[t_ind], inv_pedolog_attributes,
					sizeof(inv_pedolog_attributes));
	t_ind += ARRAY_SIZE(inv_pedolog_attributes);
	/*<-- Pedolog sysfs attribute */

	memcpy(&inv_attributes[t_ind], inv_smd_attributes,
					sizeof(inv_smd_attributes));
	t_ind += ARRAY_SIZE(inv_smd_attributes);

	memcpy(&inv_attributes[t_ind], inv_wom_attributes,
					sizeof(inv_wom_attributes));
	t_ind += ARRAY_SIZE(inv_wom_attributes);

	if (ICM10320 == st->chip_type) {
		memcpy(&inv_attributes[t_ind], inv_bias_attributes,
		       sizeof(inv_bias_attributes));
		t_ind += ARRAY_SIZE(inv_bias_attributes);
	}
	memcpy(&inv_attributes[t_ind], inv_bias_st_attributes,
		   sizeof(inv_bias_st_attributes));
	t_ind += ARRAY_SIZE(inv_bias_st_attributes);

	inv_attributes[t_ind] = NULL;

	indio_dev->channels = inv_mpu_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_mpu_channels);

	indio_dev->info = &mpu_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;
	INIT_KFIFO(st->kf);
	INVLOG(IL4, "exit\n");
	return result;
}

/*
 * inv_dmp_firmware_write() -  calling this function will load the firmware.
 */
static ssize_t inv_dmp_firmware_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
{
	int result, offset;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;
	u32 crc_value;
/* svn 14169 from Tan. file name :dmp3Default_L_20645_SA.c */
#define FIRMWARE_CRC_20645           0xaee26119
/* svn 14277 from Tan. file name: dmp3Default_L_SA.c */
//#define FIRMWARE_CRC_10320           0xdb402d1a
#define FIRMWARE_CRC_10320           0xbc0b1668

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->firmware) {
		st->firmware = kmalloc(st->dmp_image_size, GFP_KERNEL);
		if (!st->firmware) {
			pr_err("no memory while loading firmware\n");
			return -ENOMEM;
		}
	}
	offset = pos;
	memcpy(st->firmware + pos, buf, size);
	if ((!size) && (st->dmp_image_size != pos)) {
		pr_err("wrong size for DMP firmware 0x%08x vs 0x%08x\n",
						offset, st->dmp_image_size);
		kfree(st->firmware);
		st->firmware = 0;
		return -EINVAL;
	}
	if (st->dmp_image_size == (pos + size)) {
		result = crc32(0, st->firmware, st->dmp_image_size);
		if (st->chip_type == ICM20645)
			crc_value = FIRMWARE_CRC_20645;
		else
			crc_value = FIRMWARE_CRC_10320;

		if (crc_value != result) {
			pr_err("firmware CRC error - 0x%08x vs 0x%08x\n",
							result, crc_value);
			return -EINVAL;
		}
		mutex_lock(&indio_dev->mlock);
		result = inv_firmware_load(st);
		kfree(st->firmware);
		st->firmware = 0;
		mutex_unlock(&indio_dev->mlock);
		if (result) {
			INVLOG(ERR, "firmware load failed\n");
			return result;
		}
	}

	return size;
}

static ssize_t inv_dmp_firmware_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	int result, offset;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	offset = off;
	result = inv_dmp_read(st, offset, count, buf);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}


ssize_t inv_accel_covariance_write(struct file *fp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
{
	int i;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (size != ACCEL_COVARIANCE_SIZE)
		return -EINVAL;

	for (i = 0; i < COVARIANCE_SIZE; i++)
		memcpy((u8 *)&st->accel_covariance[i],
					&buf[i * sizeof(int)], sizeof(int));

	return size;
}

static int inv_dmp_covar_read(struct inv_mpu_state *st,
						int off, int size, u8 *buf)
{
	int data, result, i;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	inv_stop_dmp(st);
	for (i = 0; i < COVARIANCE_SIZE * sizeof(int); i += sizeof(int)) {
		result = read_be32_from_mem(st, (u32 *)&data, off + i);
		if (result)
			return result;
		memcpy(buf + i, (u8 *)&data, sizeof(int));
	}

	return 0;
}

ssize_t inv_accel_covariance_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	int result;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	result = inv_dmp_covar_read(st, ACCEL_COVARIANCE, count, buf);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

ssize_t inv_activity_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	int copied;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;
	u8 ddd[128];

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	mutex_lock(&indio_dev->mlock);
	copied = kfifo_out(&st->kf, ddd, count);
	memcpy(buf, ddd, copied);
	mutex_unlock(&indio_dev->mlock);

	return copied;
}

/*
 *  inv_create_dmp_sysfs() - create binary sysfs dmp entry.
 */
static struct bin_attribute dmp_firmware = {
	.attr = {
		.name = "misc_bin_dmp_firmware",
		.mode = S_IRUGO | S_IWUGO
	},
	.read = inv_dmp_firmware_read,
	.write = inv_dmp_firmware_write,
};

int inv_create_dmp_sysfs(struct iio_dev *ind)
{
	int result;
	struct inv_mpu_state *st = iio_priv(ind);

	dmp_firmware.size = st->dmp_image_size;

	INVLOG(IL5, "enter.\n");
	result = sysfs_create_bin_file(&ind->dev.kobj, &dmp_firmware);

	return result;
}
