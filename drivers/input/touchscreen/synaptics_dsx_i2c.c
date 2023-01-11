/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c/synaptics_dsx.h>
#include <linux/of_gpio.h>
#include <linux/fb.h>

#include "synaptics_dsx_i2c.h"

#define DRIVER_NAME "synaptics_dsx_i2c"
#define INPUT_PHYS_NAME "synaptics_dsx_i2c/input0"

#define TYPE_B_PROTOCOL
#define NO_0D_WHILE_2D
#define REPORT_2D_W

#define RPT_TYPE	(1 << 0)
#define RPT_X_LSB	(1 << 1)
#define RPT_X_MSB	(1 << 2)
#define RPT_Y_LSB	(1 << 3)
#define RPT_Y_MSB	(1 << 4)
#define RPT_Z		(1 << 5)
#define RPT_WX		(1 << 6)
#define RPT_WY		(1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#define EXP_FN_DET_INTERVAL	1000 /* ms */
#define POLLING_PERIOD		1 /* ms */
#define SYN_I2C_RETRY_TIMES	10
#define MAX_ABS_MT_TOUCH_MAJOR	15

#define F01_STD_QUERY_LEN	21
#define F01_BUID_ID_OFFSET	18
#define F11_STD_QUERY_LEN	9
#define F11_STD_CTRL_LEN	10
#define F11_STD_DATA_LEN	12

#ifdef CONFIG_PM_RUNTIME
#define NORMAL_OPERATION	(0 << 0)
#define SENSOR_SLEEP		(1 << 0)
#define NO_SLEEP_OFF		(0 << 2)
#define NO_SLEEP_ON		(1 << 2)
#define AUTO_SUSPEND_DELAY	1000
#endif

static bool exp_fn_inited;
static bool regulator_en;
static struct mutex exp_fn_list_mutex;
static struct list_head exp_fn_list;

struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_query {
	union {
		struct {
			unsigned char max_button_count:3;
			unsigned char reserved:5;
			unsigned char has_general_control:1;
			unsigned char has_interrupt_enable:1;
			unsigned char has_multibutton_select:1;
			unsigned char has_tx_rx_map:1;
			unsigned char has_perbutton_threshold:1;
			unsigned char has_release_threshold:1;
			unsigned char has_strongestbtn_hysteresis:1;
			unsigned char has_filter_strength:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f1a_control_0 {
	union {
		struct {
			unsigned char multibutton_report:2;
			unsigned char filter_mode:2;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control_3_4 {
	unsigned char transmitterbutton;
	unsigned char receiverbutton;
};

struct synaptics_rmi4_f1a_control {
	struct synaptics_rmi4_f1a_control_0 general_control;
	struct synaptics_rmi4_f1a_control_3_4 *electrode_map;
	unsigned char *button_int_enable;
	unsigned char *multi_button;
	unsigned char *button_threshold;
	unsigned char button_release_threshold;
	unsigned char strongest_button_hysteresis;
	unsigned char filter_strength;
};

struct synaptics_rmi4_f1a_handle {
	struct synaptics_rmi4_f1a_query button_query;
	struct synaptics_rmi4_f1a_control button_control;
	unsigned char button_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned char *button_map;
	int button_bitmask_size;
};

struct synaptics_rmi4_f54_query {
	union {
		struct {
			/* query 0 */
			unsigned char num_of_rx_electrodes;
			/* query 1 */
			unsigned char num_of_tx_electrodes;
			/* query 2 */
			unsigned char f54_query2_b0__1:2;
			unsigned char has_baseline:1;
			unsigned char has_image8:1;
			unsigned char f54_query2_b4__5:2;
			unsigned char has_image16:1;
			unsigned char f54_query2_b7:1;
			/* queries 3.0 and 3.1 */
			unsigned short clock_rate;
			/* query 4 */
			unsigned char touch_controller_family;
			/* query 5 */
			unsigned char has_pixel_touch_threshold_adjustment:1;
			unsigned char f54_query5_b1__7:7;
			/* query 6 */
			unsigned char has_sensor_assignment:1;
			unsigned char has_interference_metric:1;
			unsigned char has_sense_frequency_control:1;
			unsigned char has_firmware_noise_mitigation:1;
			unsigned char has_ctrl11:1;
			unsigned char has_two_byte_report_rate:1;
			unsigned char has_one_byte_report_rate:1;
			unsigned char has_relaxation_control:1;
		} __packed;
		unsigned char data[8];
	};
};

struct synaptics_rmi4_exp_fn {
	enum exp_fn fn_type;
	bool inserted;
	int (*func_init)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_remove)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask);
	struct list_head link;
};

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_flipx_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_flipx_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_flipy_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_flipy_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_swap_axes_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static struct device_attribute attrs[] = {
	__ATTR(reset, (S_IWUSR | S_IWGRP),
			synaptics_rmi4_show_error,
			synaptics_rmi4_f01_reset_store),
	__ATTR(productinfo, S_IRUGO,
			synaptics_rmi4_f01_productinfo_show,
			synaptics_rmi4_store_error),
	__ATTR(buildid, S_IRUGO,
			synaptics_rmi4_f01_buildid_show,
			synaptics_rmi4_store_error),
	__ATTR(flashprog, S_IRUGO,
			synaptics_rmi4_f01_flashprog_show,
			synaptics_rmi4_store_error),
	__ATTR(0dbutton, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_0dbutton_show,
			synaptics_rmi4_0dbutton_store),
	__ATTR(flipx, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_flipx_show,
			synaptics_rmi4_flipx_store),
	__ATTR(flipy, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_flipy_show,
			synaptics_rmi4_flipy_store),
	__ATTR(swapaxes, (S_IWUSR | S_IWGRP),
			synaptics_rmi4_show_error,
			synaptics_rmi4_swap_axes_store),
};

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data);
static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	unsigned int reset;
	int retval;

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_rmi4_reset_device(rmi4_data);
	if (retval < 0) {
		dev_err(dev, "%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x 0x%02x\n",
			(rmi4_data->rmi4_mod_info.product_info[0]),
			(rmi4_data->rmi4_mod_info.product_info[1]));
}

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;
	unsigned int build_id;

	rmi = &(rmi4_data->rmi4_mod_info);

	build_id = (unsigned int)rmi->build_id[0] +
			(unsigned int)rmi->build_id[1] * 0x100 +
			(unsigned int)rmi->build_id[2] * 0x10000;

	return snprintf(buf, PAGE_SIZE, "%u\n", build_id);
}

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
	unsigned short addr, unsigned char *data, unsigned short length);
static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_f01_device_status device_status;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	int retval;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			device_status.data,
			sizeof(device_status.data));
	if (retval < 0) {
		dev_err(dev, "%s: Failed to read device status, error = %d\n",
				__func__, retval);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", device_status.flash_prog);
}

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", rmi4_data->button_0d_enabled);
}

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
	unsigned short addr, unsigned char *data, unsigned short length);
static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;
	unsigned char intr_enable, ii;
	unsigned int input;
	int retval;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	input = input > 0 ? 1 : 0;

	if (rmi4_data->button_0d_enabled == input)
		return count;

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
				ii = fhandler->intr_reg_num;

				retval = synaptics_rmi4_i2c_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable, sizeof(intr_enable));
				if (retval < 0)
					return retval;

				if (input == 1)
					intr_enable |= fhandler->intr_mask;
				else
					intr_enable &= ~fhandler->intr_mask;

				retval = synaptics_rmi4_i2c_write(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable, sizeof(intr_enable));
				if (retval < 0)
					return retval;
			}
		}
	}

	rmi4_data->button_0d_enabled = input;

	return count;
}

static ssize_t synaptics_rmi4_flipx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", rmi4_data->flip_x);
}

static ssize_t synaptics_rmi4_flipx_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmi4_data->flip_x = input > 0 ? 1 : 0;

	return count;
}

static ssize_t synaptics_rmi4_flipy_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", rmi4_data->flip_y);
}

static ssize_t synaptics_rmi4_flipy_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmi4_data->flip_y = input > 0 ? 1 : 0;

	return count;
}

static void synaptics_rmi4_swap_axis(struct synaptics_rmi4_data *rmi4_data);
static ssize_t synaptics_rmi4_swap_axes_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	synaptics_rmi4_swap_axis(rmi4_data);

	return count;
}

static void synaptics_rmi4_swap_axis(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;
	struct i2c_client *client = rmi4_data->i2c_client;
	struct synaptics_rmi4_fn *sensor_tuning = NULL;
	struct synaptics_rmi4_f54_query f54_query;
	unsigned short swap_ctrl_addr;
	unsigned short offset;
	unsigned char val;
	int retval = 0;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			pr_info("function number 0x%x\n",
					fhandler->fn_number);
			if (fhandler->fn_number == SYNAPTICS_RMI4_F55)
				sensor_tuning = fhandler;
			if (sensor_tuning == NULL &&
				fhandler->fn_number == SYNAPTICS_RMI4_F54)
				sensor_tuning = fhandler;
		}
	}

	if (sensor_tuning != NULL) {
		if (sensor_tuning->fn_number == SYNAPTICS_RMI4_F55)
			swap_ctrl_addr = sensor_tuning->full_addr.ctrl_base + 0;
		else {
			retval = synaptics_rmi4_i2c_read(rmi4_data,
				sensor_tuning->full_addr.query_base,
				(unsigned char *)f54_query.data,
				sizeof(f54_query.data));
			if (retval < 0)
				dev_err(&client->dev,
				"%s: Failed to read swap control registers\n",
				__func__);

			/* general ctrl 0 */
			offset = 1;
			/* ctrl 1/4/5/6/8.0/8.1/9 */
			if (f54_query.touch_controller_family == 0x00 ||
				f54_query.touch_controller_family == 0x01)
				offset += 7;
			/* ctrl 2/2.1 */
			offset += 2;
			/* ctrl 3 */
			if (f54_query.has_pixel_touch_threshold_adjustment)
				offset++;
			/* ctrl 7*/
			if (f54_query.touch_controller_family == 0x01)
				offset += 1;
			/* ctrl 10 */
			if (f54_query.has_interference_metric)
				offset++;
			/* ctrl 11/11.0 */
			if (f54_query.has_ctrl11)
				offset += 2;
			/* ctrl 12/13 */
			if (f54_query.has_relaxation_control)
				offset += 2;
			if (!f54_query.has_sensor_assignment)
				dev_err(&client->dev,
				"%s: Sensor assignment properties not exist\n",
				__func__);
			swap_ctrl_addr = sensor_tuning->full_addr.ctrl_base +
						offset;
		}
		retval = synaptics_rmi4_i2c_read(rmi4_data, swap_ctrl_addr,
					(unsigned char *)&val, sizeof(val));
		if (retval < 0)
			dev_err(&client->dev,
				"%s: Failed to read swap control registers\n",
				__func__);

		val = (val & 0xFE) | (!val & 0x01);

		dev_info(&client->dev, "swap value :0x%x, rmi address 0x%02X\n",
					val, swap_ctrl_addr);

		retval = synaptics_rmi4_i2c_write(rmi4_data, swap_ctrl_addr,
					(unsigned char *)&val, sizeof(val));
		if (retval < 0)
			dev_err(&client->dev,
				"%s: Failed to write swap control registers\n",
				__func__);
	} else
		dev_err(&client->dev,
			"%s: Firmware not support swap function\n", __func__);
}

/*
 * synaptics_rmi4_set_page()
 *
 * Called by synaptics_rmi4_i2c_read() and synaptics_rmi4_i2c_write().
 *
 * This function writes to the page select register to switch to the
 * assigned page.
 */
static int synaptics_rmi4_set_page(struct synaptics_rmi4_data *rmi4_data,
					unsigned int address)
{
	struct i2c_client *i2c = rmi4_data->i2c_client;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	unsigned char retry;
	int retval = 0;

	page = ((address >> 8) & MASK_8BIT);
	if (page != rmi4_data->current_page) {
		buf[0] = MASK_8BIT;
		buf[1] = page;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(i2c, buf, PAGE_SELECT_LEN);
			if (retval != PAGE_SELECT_LEN) {
				dev_err(&i2c->dev, "%s: I2C retry %d\n",
						__func__, retry + 1);
				msleep(20);
			} else {
				rmi4_data->current_page = page;
				break;
			}
		}
	} else
		return PAGE_SELECT_LEN;

	return (retval == PAGE_SELECT_LEN) ? retval : -EIO;
}

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
			unsigned short addr, unsigned char *data,
			unsigned short length)
{
	unsigned char retry;
	unsigned char buf;
	int retval;
	struct i2c_msg msg[] = {
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = 0,
			.len = 1,
			.buf = &buf,
		},
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		},
	};

	buf = addr & MASK_8BIT;

	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	retval = synaptics_rmi4_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN)
		goto exit;

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(rmi4_data->i2c_client->adapter, msg, 2) == 2) {
			retval = length;
			break;
		}
		dev_err(&rmi4_data->i2c_client->dev, "%s: I2C retry %d\n",
						__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: I2C read over retry limit\n", __func__);
		retval = -EIO;
	}

exit:
	mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	return retval;
}

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
			unsigned short addr, unsigned char *data,
			unsigned short length)
{
	unsigned char buf[length + 1];
	unsigned char retry;
	int retval;
	struct i2c_msg msg[] = {
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	retval = synaptics_rmi4_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN)
		goto exit;

	buf[0] = addr & MASK_8BIT;
	memcpy(&buf[1], &data[0], length);

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(rmi4_data->i2c_client->adapter, msg, 1) == 1) {
			retval = length;
			break;
		}
		dev_err(&rmi4_data->i2c_client->dev, "%s: I2C retry %d\n",
						__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: I2C write over retry limit\n", __func__);
		retval = -EIO;
	}

exit:
	mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	return retval;
}

/*
 * synaptics_rmi4_f11_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $11
 * finger data has been detected.
 *
 * This function reads the Function $11 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
static int synaptics_rmi4_f11_abs_report(struct synaptics_rmi4_data *rmi4_data,
					struct synaptics_rmi4_fn *fhandler)
{
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char reg_index;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char num_of_finger_status_regs;
	unsigned char finger_shift;
	unsigned char finger_status;
	unsigned char data_reg_blk_size;
	unsigned char finger_status_reg[3];
	unsigned char data[F11_STD_DATA_LEN];
	unsigned short data_addr;
	unsigned short data_offset;
	int x, y, z;
	int wx, wy;
	int retval;

	/*
	 * The number of finger status registers is determined by the
	 * maximum number of fingers supported - 2 bits per finger. So
	 * the number of finger status registers to read is:
	 * register_count = ceil(max_num_of_fingers / 4)
	 */
	fingers_supported = fhandler->num_of_data_points;
	num_of_finger_status_regs = (fingers_supported + 3) / 4;
	data_addr = fhandler->full_addr.data_base;
	data_reg_blk_size = fhandler->size_of_data_register_block;

	retval = synaptics_rmi4_i2c_read(rmi4_data, data_addr,
				finger_status_reg, num_of_finger_status_regs);
	if (retval < 0)
		return 0;

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift)
								& MASK_2BIT;

		/*
		 * Each 2-bit finger status field represents the following:
		 * 00 = finger not present
		 * 01 = finger present and data accurate
		 * 10 = finger present but data may be inaccurate
		 * 11 = reserved
		 */
#ifdef TYPE_B_PROTOCOL
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, finger_status != 0);
#endif

		if (finger_status) {
			data_offset = data_addr + num_of_finger_status_regs +
					(finger * data_reg_blk_size);
			retval = synaptics_rmi4_i2c_read(rmi4_data,
					data_offset, data, data_reg_blk_size);
			if (retval < 0)
				return 0;

			x = (data[0] << 4) | (data[2] & MASK_4BIT);
			y = (data[1] << 4) | ((data[2] >> 4) & MASK_4BIT);
			wx = (data[3] & MASK_4BIT);
			wy = (data[3] >> 4) & MASK_4BIT;
			z = data[4];

			if (rmi4_data->flip_x)
				x = rmi4_data->sensor_max_x - x;

			if (rmi4_data->flip_y)
				y = rmi4_data->sensor_max_y - y;

			dev_dbg(&rmi4_data->i2c_client->dev, "%s: Finger %d:\n"
			"status = 0x%02x, x = %d, y = %d, wx = %d, wy = %d\n",
			__func__, finger, finger_status, x, y, wx, wy);

			/* Calibrate x and y value */
			x = x - rmi4_data->sensor_margin_x;
			if (x > rmi4_data->sensor_max_x)
				x = rmi4_data->sensor_max_x;
			else if (x < 0)
				x = 0;

			y = y - rmi4_data->sensor_margin_y;
			if (y > rmi4_data->sensor_max_y)
				y = rmi4_data->sensor_max_y;
			else if (y < 0)
				y = 0;

			input_report_key(rmi4_data->input_dev, BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
						BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
						ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
						ABS_MT_POSITION_Y, y);
			/* pressure is must have */
			input_report_abs(rmi4_data->input_dev,
						ABS_MT_PRESSURE, z);

#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif
			touch_count++;
		}
	}

#ifndef TYPE_B_PROTOCOL
	if (!touch_count)
		input_mt_sync(rmi4_data->input_dev);
#else
#ifdef KERNEL_ABOVE_3_7
	/* sync after groups of events */
	input_mt_sync_frame(rmi4_data->input_dev);
#endif
#endif

	input_sync(rmi4_data->input_dev);

	return touch_count;
}

static void synaptics_rmi4_f1a_report(struct synaptics_rmi4_data *rmi4_data,
					struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	static unsigned char do_once = 1;
	static bool current_status[MAX_NUMBER_OF_BUTTONS];
#ifdef NO_0D_WHILE_2D
	static bool before_2d_status[MAX_NUMBER_OF_BUTTONS];
	static bool while_2d_status[MAX_NUMBER_OF_BUTTONS];
#endif
	unsigned short data_addr = fhandler->full_addr.data_base;
	unsigned char button;
	unsigned char index;
	unsigned char shift;
	unsigned char status;
	unsigned char *data;
	int retval;

	if (do_once) {
		memset(current_status, 0, sizeof(current_status));
#ifdef NO_0D_WHILE_2D
		memset(before_2d_status, 0, sizeof(before_2d_status));
		memset(while_2d_status, 0, sizeof(while_2d_status));
#endif
		do_once = 0;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data, data_addr,
			f1a->button_data_buffer, f1a->button_bitmask_size);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to read button data registers\n", __func__);
		return;
	}

	data = f1a->button_data_buffer;

	for (button = 0; button < f1a->valid_button_count; button++) {
		index = button / 8;
		shift = button % 8;
		status = ((data[index] >> shift) & MASK_1BIT);

		if (current_status[button] == status)
			continue;
		else
			current_status[button] = status;

		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Button %d (code %d) ->%d\n", __func__,
				button, f1a->button_map[button], status);
#ifdef NO_0D_WHILE_2D
		if (!rmi4_data->fingers_on_2d) {
			if (status == 1)
				before_2d_status[button] = true;
			else {
				if (while_2d_status[button]) {
					while_2d_status[button] = false;
					continue;
				} else
					before_2d_status[button] = false;
			}
			input_report_key(rmi4_data->input_dev,
					f1a->button_map[button], status);
		} else {
			if (before_2d_status[button]) {
				before_2d_status[button] = false;
				input_report_key(rmi4_data->input_dev,
					f1a->button_map[button], status);
			} else {
				if (status == 1)
					while_2d_status[button] = true;
				else
					while_2d_status[button] = false;
			}
		}
#else
		input_report_key(rmi4_data->input_dev, f1a->button_map[button],
							status);
#endif
	}

	input_sync(rmi4_data->input_dev);

	return;
}

static void synaptics_rmi4_report_touch(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler, unsigned char *touch_count)
{
	unsigned char touch_count_2d;

	dev_dbg(&rmi4_data->i2c_client->dev, "%s: Function %02x reporting\n",
						__func__, fhandler->fn_number);

	switch (fhandler->fn_number) {
	case SYNAPTICS_RMI4_F11:
		touch_count_2d = synaptics_rmi4_f11_abs_report(rmi4_data,
								fhandler);
		*touch_count += touch_count_2d;

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	case SYNAPTICS_RMI4_F1A:
		synaptics_rmi4_f1a_report(rmi4_data, fhandler);
		break;
	default:
		break;
	}

	return;
}

static int synaptics_rmi4_sensor_report(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fn *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;
	unsigned char touch_count = 0;
	unsigned char intr[MAX_INTR_REGISTERS];
	int retval;

	rmi = &(rmi4_data->rmi4_mod_info);

	/*
	 * Get interrupt status information from F01 Data1 register to
	 * determine the source(s) that are flagging the interrupt.
	 */
	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr + 1, intr,
			rmi4_data->num_of_intr_regs);
	if (retval < 0)
		return retval;

	/*
	 * Traverse the function handler list and service the source(s)
	 * of the interrupt accordingly.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				if (fhandler->intr_mask &
					intr[fhandler->intr_reg_num]) {
					synaptics_rmi4_report_touch(rmi4_data,
							fhandler, &touch_count);
				}
			}
		}
	}

	mutex_lock(&exp_fn_list_mutex);
	if (!list_empty(&exp_fn_list)) {
		list_for_each_entry(exp_fhandler, &exp_fn_list, link) {
			if (exp_fhandler->inserted &&
				(exp_fhandler->func_attn != NULL))
				exp_fhandler->func_attn(rmi4_data, intr[0]);
		}
	}
	mutex_unlock(&exp_fn_list_mutex);

	return touch_count;
}

static irqreturn_t synaptics_rmi4_irq(int irq, void *data)
{
	struct synaptics_rmi4_data *rmi4_data = data;

	synaptics_rmi4_sensor_report(rmi4_data);

	return IRQ_HANDLED;
}

static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
					bool enable)
{
	unsigned char intr_status;
	int retval = 0;
	unsigned long irqflags;

	if (enable) {
		/* Clear interrupts first */
		retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr + 1, &intr_status,
			rmi4_data->num_of_intr_regs);
		if (retval < 0)
			return retval;
		spin_lock_irqsave(&rmi4_data->irq_lock, irqflags);
		if (rmi4_data->irq_is_disable) {
			enable_irq(rmi4_data->irq);
			rmi4_data->irq_is_disable = 0;
		}
	} else {
		spin_lock_irqsave(&rmi4_data->irq_lock, irqflags);
		if (!rmi4_data->irq_is_disable) {
			rmi4_data->irq_is_disable = 1;
			disable_irq(rmi4_data->irq);
		}
	}

	spin_unlock_irqrestore(&rmi4_data->irq_lock, irqflags);
	return retval;
}

static int synaptics_rmi4_irq_acquire(struct synaptics_rmi4_data *rmi4_data,
					bool enable)
{
	const struct synaptics_dsx_platform_data *pdata = rmi4_data->board;
	unsigned char intr_status;
	int retval = 0;

	if (enable) {
		/* Clear interrupts first */
		retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr + 1, &intr_status,
			rmi4_data->num_of_intr_regs);
		if (retval < 0)
			return retval;

		retval = request_threaded_irq(rmi4_data->irq, NULL,
			synaptics_rmi4_irq, pdata->irq_flags,
			DRIVER_NAME, rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to request_threaded_irq\n", __func__);
			return retval;
		}
	} else
		free_irq(rmi4_data->irq, rmi4_data);

	return retval;
}

static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data,
				struct synaptics_rmi4_fn *fhandler,
				struct synaptics_rmi4_fn_desc *fd,
				unsigned int intr_count)
{
	unsigned char abs_data_size;
	unsigned char abs_data_blk_size;
	unsigned char query[F11_STD_QUERY_LEN];
	unsigned char control[F11_STD_CTRL_LEN];
	unsigned char intr_offset;
	unsigned char ii;
	unsigned char res[4];
	int retval;
	const struct synaptics_dsx_platform_data *platform_data =
				rmi4_data->board;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base, query, sizeof(query));
	if (retval < 0)
		return retval;

	/*
	 * Maximum number of fingers supported
	 */
	if ((query[1] & MASK_3BIT) <= 4)
		fhandler->num_of_data_points = (query[1] & MASK_3BIT) + 1;
	else if ((query[1] & MASK_3BIT) == 5)
		fhandler->num_of_data_points = 10;

	rmi4_data->num_of_fingers = fhandler->num_of_data_points;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
		fhandler->full_addr.ctrl_base, control, sizeof(control));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	if (platform_data->sensor_max_x && platform_data->sensor_max_y) {
		/*Set Maximum x and y */
		if (rmi4_data->swap_axes) {
			res[0] = platform_data->sensor_max_y & MASK_8BIT;
			res[1] = platform_data->sensor_max_y >> 8;
			res[2] = platform_data->sensor_max_x & MASK_8BIT;
			res[3] = platform_data->sensor_max_x >> 8;
		} else {
			res[0] = platform_data->sensor_max_x & MASK_8BIT;
			res[1] = platform_data->sensor_max_x >> 8;
			res[2] = platform_data->sensor_max_y & MASK_8BIT;
			res[3] = platform_data->sensor_max_y >> 8;
		}

		retval = synaptics_rmi4_i2c_write(rmi4_data,
		fhandler->full_addr.ctrl_base + 6, res, sizeof(res));
		if (retval < 0)
			return retval;

		rmi4_data->sensor_res_x = platform_data->sensor_res_x;
		rmi4_data->sensor_res_y = platform_data->sensor_res_y;
		rmi4_data->sensor_max_x = platform_data->sensor_max_x;
		rmi4_data->sensor_max_y = platform_data->sensor_max_y;
		rmi4_data->sensor_margin_x = platform_data->sensor_margin_x;
		rmi4_data->sensor_margin_y = platform_data->sensor_margin_y;
	} else {
		rmi4_data->sensor_max_x = ((control[6] & MASK_8BIT) << 0) |
						((control[7] & MASK_4BIT) << 8);
		rmi4_data->sensor_max_y = ((control[8] & MASK_8BIT) << 0) |
						((control[9] & MASK_4BIT) << 8);
	}
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Function %02x max x = %d max y = %d\n", __func__,
			fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);


	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
		ii < ((fd->intr_src_count & MASK_3BIT) + intr_offset); ii++)
		fhandler->intr_mask |= 1 << ii;

	abs_data_size = query[5] & MASK_2BIT;
	abs_data_blk_size = 3 + (2 * (abs_data_size == 0 ? 1 : 0));
	fhandler->size_of_data_register_block = abs_data_blk_size;

	return retval;
}

static int synaptics_rmi4_f1a_alloc_mem(struct synaptics_rmi4_data *rmi4_data,
					struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a;
	int retval;

	f1a = kzalloc(sizeof(*f1a), GFP_KERNEL);
	if (!f1a) {
		dev_err(&rmi4_data->i2c_client->dev,
		"%s: Failed to alloc mem for function handle\n", __func__);
		return -ENOMEM;
	}

	fhandler->data = (void *)f1a;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base,
			f1a->button_query.data,
			sizeof(f1a->button_query.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to read query registers\n", __func__);
		return retval;
	}

	f1a->button_count = f1a->button_query.max_button_count + 1;
	f1a->button_bitmask_size = (f1a->button_count + 7) / 8;
	f1a->button_data_buffer = kcalloc(f1a->button_bitmask_size,
			sizeof(*(f1a->button_data_buffer)), GFP_KERNEL);
	if (!f1a->button_data_buffer) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to alloc mem for data buffer\n", __func__);
		return -ENOMEM;
	}

	f1a->button_map = kcalloc(f1a->button_count,
			sizeof(*(f1a->button_map)), GFP_KERNEL);
	if (!f1a->button_map) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to alloc mem for button map\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static int synaptics_rmi4_cap_button_map(struct synaptics_rmi4_data *rmi4_data,
					struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	const struct synaptics_dsx_platform_data *pdata = rmi4_data->board;
	unsigned char ii;

	if (!pdata->cap_button_map) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: cap_button_map is NULL in board file\n", __func__);
		return -ENODEV;
	} else if (!pdata->cap_button_map->map) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Button map is missing in board file\n", __func__);
		return -ENODEV;
	} else {
		if (pdata->cap_button_map->nbuttons != f1a->button_count) {
			f1a->valid_button_count = min(f1a->button_count,
					pdata->cap_button_map->nbuttons);
		} else
			f1a->valid_button_count = f1a->button_count;

		for (ii = 0; ii < f1a->valid_button_count; ii++)
			f1a->button_map[ii] = pdata->cap_button_map->map[ii];
	}

	return 0;
}

static void synaptics_rmi4_f1a_kfree(struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;

	if (f1a) {
		kfree(f1a->button_data_buffer);
		kfree(f1a->button_map);
		kfree(f1a);
		fhandler->data = NULL;
	}

	return;
}

static int synaptics_rmi4_f1a_init(struct synaptics_rmi4_data *rmi4_data,
				struct synaptics_rmi4_fn *fhandler,
				struct synaptics_rmi4_fn_desc *fd,
				unsigned int intr_count)
{
	unsigned short intr_offset;
	unsigned char ii;
	int retval;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
		ii < ((fd->intr_src_count & MASK_3BIT) + intr_offset); ii++)
		fhandler->intr_mask |= 1 << ii;

	retval = synaptics_rmi4_f1a_alloc_mem(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	retval = synaptics_rmi4_cap_button_map(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	rmi4_data->button_0d_enabled = 1;

	return 0;
error_exit:
	synaptics_rmi4_f1a_kfree(fhandler);

	return retval;
}

static int synaptics_rmi4_alloc_fh(struct synaptics_rmi4_fn **fhandler,
			struct synaptics_rmi4_fn_desc *rmi_fd, int page_number)
{
	*fhandler = kzalloc(sizeof(**fhandler), GFP_KERNEL);
	if (!(*fhandler))
		return -ENOMEM;

	(*fhandler)->full_addr.data_base = (rmi_fd->data_base_addr |
						(page_number << 8));
	(*fhandler)->full_addr.ctrl_base = (rmi_fd->ctrl_base_addr |
						(page_number << 8));
	(*fhandler)->full_addr.cmd_base = (rmi_fd->cmd_base_addr |
						(page_number << 8));
	(*fhandler)->full_addr.query_base = (rmi_fd->query_base_addr |
						(page_number << 8));
	(*fhandler)->fn_number = rmi_fd->fn_number;

	return 0;
}

static int synaptics_rmi4_query_device_info(struct synaptics_rmi4_data
							*rmi4_data)
{
	struct synaptics_rmi4_device_info *rmi = &(rmi4_data->rmi4_mod_info);
	unsigned char f01_query[F01_STD_QUERY_LEN];
	int retval;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_query_base_addr, f01_query,
			sizeof(f01_query));
	if (retval < 0)
		return retval;

	/* RMI Version 4.0 currently supported */
	rmi->version_major = 4;
	rmi->version_minor = 0;
	rmi->manufacturer_id = f01_query[0];
	rmi->product_props = f01_query[1];
	rmi->product_info[0] = f01_query[2] & MASK_7BIT;
	rmi->product_info[1] = f01_query[3] & MASK_7BIT;
	rmi->date_code[0] = f01_query[4] & MASK_5BIT;
	rmi->date_code[1] = f01_query[5] & MASK_4BIT;
	rmi->date_code[2] = f01_query[6] & MASK_5BIT;
	rmi->tester_id = ((f01_query[7] & MASK_7BIT) << 8) |
				(f01_query[8] & MASK_7BIT);
	rmi->serial_number = ((f01_query[9] & MASK_7BIT) << 8) |
				(f01_query[10] & MASK_7BIT);
	memcpy(rmi->product_id_string, &f01_query[11], 10);

	if (rmi->manufacturer_id != 1)
		dev_err(&rmi4_data->i2c_client->dev,
		"%s: Non-Synaptics device found, manufacturer ID = %d\n",
		__func__, rmi->manufacturer_id);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_query_base_addr + F01_BUID_ID_OFFSET,
			rmi->build_id, sizeof(rmi->build_id));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
		"%s: Failed to read firmware build id (code %d)\n",
		__func__, retval);
		return retval;
	}

	return 0;
}

/*
 * synaptics_rmi4_query_device()
 *
 * Called by synaptics_rmi4_probe().
 *
 * This funtion scans the page description table, records the offsets
 * to the register types of Function $01, sets up the function handlers
 * for Function $11 and Function $12, determines the number of interrupt
 * sources from the sensor, adds valid Functions with data inputs to the
 * Function linked list, parses information from the query registers of
 * Function $01, and enables the interrupt sources from the valid Functions
 * with data inputs.
 */
static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_f01_device_status status;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;
	unsigned char intr_count = 0;
	unsigned char data_sources = 0;
	unsigned char page_number;
	unsigned char ii;
	unsigned short pdt_entry_addr;
	unsigned short intr_addr;
	int retval;
	int crc_count = 0;

	rmi = &(rmi4_data->rmi4_mod_info);

	INIT_LIST_HEAD(&rmi->support_fn_list);

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
			pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);
			retval = synaptics_rmi4_i2c_read(rmi4_data,
					pdt_entry_addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			fhandler = NULL;
			if (rmi_fd.fn_number == 0) {
				dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Reached end of PDT\n", __func__);
				break;
			}

			dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: F%02x found (page %d)\n", __func__,
				rmi_fd.fn_number, page_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				rmi4_data->f01_query_base_addr =
						rmi_fd.query_base_addr;
				rmi4_data->f01_ctrl_base_addr =
						rmi_fd.ctrl_base_addr;
				rmi4_data->f01_data_base_addr =
						rmi_fd.data_base_addr;
				rmi4_data->f01_cmd_base_addr =
						rmi_fd.cmd_base_addr;
crc_in_process:
				crc_count++;
				retval =
				synaptics_rmi4_query_device_info(rmi4_data);
				if (retval < 0)
					return retval;

				retval = synaptics_rmi4_i2c_read(rmi4_data,
					rmi4_data->f01_data_base_addr,
					status.data, sizeof(status.data));
				if (retval < 0)
					return retval;

				/*
				 * Code $06 signal that the firmware is in
				 * process of testing either the configuration
				 * area or the firmware area
				 */
				if (status.status_code == 0x6 && crc_count < 10)
						goto crc_in_process;

				if (status.flash_prog == 1) {
					pr_notice("%s: In flash prog mode, status = 0x%02x\n", __func__,
						status.status_code);
					goto flash_prog_mode;
				}
				break;
			case SYNAPTICS_RMI4_F11:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%d\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f11_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F1A:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%d\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f1a_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F54:
			case SYNAPTICS_RMI4_F55:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%d\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}
				break;
			default:
				break;
			}

			/* Accumulate the interrupt count */
			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);

			if (fhandler && rmi_fd.intr_src_count) {
				list_add_tail(&fhandler->link,
						&rmi->support_fn_list);
			}
		}
	}

flash_prog_mode:
	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Number of interrupt registers = %d\n",
			__func__, rmi4_data->num_of_intr_regs);

	memset(rmi4_data->intr_mask, 0x00, sizeof(rmi4_data->intr_mask));

	/*
	 * Map out the interrupt bit masks for the interrupt sources
	 * from the registered function handlers.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link)
			data_sources += fhandler->num_of_data_sources;
	}
	if (data_sources) {
		if (!list_empty(&rmi->support_fn_list)) {
			list_for_each_entry(fhandler,
						&rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources)
				rmi4_data->intr_mask[fhandler->intr_reg_num] |=
							fhandler->intr_mask;
			}
		}
	}

	/* Enable the interrupt sources */
	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Interrupt enable mask %d = 0x%02x\n",
				__func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_i2c_write(rmi4_data, intr_addr,
					&(rmi4_data->intr_mask[ii]),
					sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0)
				return retval;
		}
	}

	return 0;
}

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fn *fhandler, *tmp;
	struct synaptics_rmi4_device_info *rmi;
	unsigned char command = 0x01;
	int retval;

	rmi = &(rmi4_data->rmi4_mod_info);

	retval = synaptics_rmi4_i2c_write(rmi4_data,
		rmi4_data->f01_cmd_base_addr, &command, sizeof(command));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to issue reset command, error = %d\n",
			__func__, retval);
		return retval;
	}

	msleep(100);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry_safe(fhandler, tmp, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
				synaptics_rmi4_f1a_kfree(fhandler);
			else
				kfree(fhandler->data);
			list_del(&fhandler->link);
			kfree(fhandler);
		}
	}

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to query device\n", __func__);
		return retval;
	}

	if (rmi4_data->swap_axes)
		synaptics_rmi4_swap_axis(rmi4_data);

	return 0;
}

/*
 * synaptics_rmi4_detection_work()
 *
 * Called by the kernel at the scheduled time.
 *
 * This function is a self-rearming work thread that checks for the
 * insertion and removal of other expansion Function modules such as
 * rmi_dev and calls their initialization and removal callback functions
 * accordingly.
 */
static void synaptics_rmi4_detection_work(struct work_struct *work)
{
	struct synaptics_rmi4_exp_fn *exp_fhandler, *next_list_entry;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(work, struct synaptics_rmi4_data,
			det_work.work);

	mutex_lock(&exp_fn_list_mutex);
	if (!list_empty(&exp_fn_list)) {
		list_for_each_entry_safe(exp_fhandler, next_list_entry,
					&exp_fn_list, link) {
			if ((exp_fhandler->func_init != NULL) &&
					(!exp_fhandler->inserted)) {
				exp_fhandler->func_init(rmi4_data);
				exp_fhandler->inserted = true;
			} else if ((exp_fhandler->func_init == NULL) &&
					(exp_fhandler->inserted)) {
				exp_fhandler->func_remove(rmi4_data);
				list_del(&exp_fhandler->link);
				kfree(exp_fhandler);
			}
		}
	}
	mutex_unlock(&exp_fn_list_mutex);

	return;
}

/*
 * synaptics_rmi4_new_function()
 *
 * Called by other expansion Function modules in their module init and
 * module exit functions.
 *
 * This function is used by other expansion Function modules such as
 * rmi_dev to register themselves with the driver by providing their
 * initialization and removal callback function pointers so that they
 * can be inserted or removed dynamically at module init and exit times,
 * respectively.
 */
void synaptics_rmi4_new_function(enum exp_fn fn_type, bool insert,
		int (*func_init)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_remove)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask))
{
	struct synaptics_rmi4_exp_fn *exp_fhandler;

	if (!exp_fn_inited) {
		mutex_init(&exp_fn_list_mutex);
		INIT_LIST_HEAD(&exp_fn_list);
		exp_fn_inited = true;
	}

	mutex_lock(&exp_fn_list_mutex);
	if (insert) {
		exp_fhandler = kzalloc(sizeof(*exp_fhandler), GFP_KERNEL);
		if (!exp_fhandler) {
			pr_err("%s: Failed to alloc mem for expansion function\n", __func__);
			goto exit;
		}
		exp_fhandler->fn_type = fn_type;
		exp_fhandler->func_init = func_init;
		exp_fhandler->func_attn = func_attn;
		exp_fhandler->func_remove = func_remove;
		exp_fhandler->inserted = false;
		list_add_tail(&exp_fhandler->link, &exp_fn_list);
	} else {
		if (!list_empty(&exp_fn_list)) {
			list_for_each_entry(exp_fhandler, &exp_fn_list, link) {
				if (exp_fhandler->func_init == func_init) {
					exp_fhandler->inserted = false;
					exp_fhandler->func_init = NULL;
					exp_fhandler->func_attn = NULL;
					goto exit;
				}
			}
		}
	}

exit:
	mutex_unlock(&exp_fn_list_mutex);

	return;
}
EXPORT_SYMBOL(synaptics_rmi4_new_function);

#define VIRT_KEYS(x...)  __stringify(x)
static char panel_type[20] = "720p_45";
static ssize_t virtual_keys_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	unsigned int x1, x2, x3, y;
	unsigned int xres, yres;

	xres = registered_fb[0]->var.xres;
	yres = registered_fb[0]->var.yres;

	x1 = 3 * xres / 16;
	x2 = xres / 2;
	x3 = 13 * xres / 16;
	y = 146 * yres / 139;

	if (!strcmp(panel_type , "720p_45"))
		y += 4 * yres / 139;

	return sprintf(buf,
		VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_BACK)
		":%d:%d:100:100" ":"
		VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_HOMEPAGE)
		":%d:%d:100:100" ":"
		VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_MENU)
		":%d:%d:100:100\n",
		x1, y, x2, y, x3, y);
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.synaptics_dsx_i2c",
		.mode = S_IRUGO,
	},
	.show = &virtual_keys_show,
};

static struct attribute *props_attrs[] = {
	&virtual_keys_attr.attr,
	NULL
};

static struct attribute_group props_attr_group = {
	.attrs = props_attrs,
};

static int synaptics_set_virtual_key(struct input_dev *input_dev)
{
	struct kobject *props_kobj;
	int ret = 0;

	props_kobj = kobject_create_and_add("board_properties", NULL);
	if (props_kobj)
		ret = sysfs_create_group(props_kobj, &props_attr_group);
	if (!props_kobj || ret)
		dev_err(&input_dev->dev, "failed to create board_properties\n");

	return 0;
}
static char button_maps[] = {
	KEY_RESERVED,
};

static struct synaptics_dsx_cap_button_map dsx_cap_button_map = {
	.nbuttons = 0,
	.map = button_maps,
};

static int synaptics_rmi4_set_button_map(
		struct synaptics_dsx_platform_data *pd,
					struct device_node *np)
{
	uint32_t num_of_buttons = 0;
	char *button_map_ptr = NULL;

	if (of_property_read_u32(np, "synaptics,num_button",
					&num_of_buttons))
		goto no_button_map;

	button_map_ptr = kzalloc(num_of_buttons * sizeof(char), GFP_KERNEL);

	if (of_property_read_u8_array(np, "synaptics,button_maps",
					button_map_ptr, num_of_buttons))
		goto button_maps_err;

	pd->cap_button_map->nbuttons = num_of_buttons;
	pd->cap_button_map->map = button_map_ptr;

	pr_debug("button_map_ptr = %p num_of_buttons= %d",
			button_map_ptr, num_of_buttons);

	return 0;

no_button_map:
	pr_debug("no buttons map defined in dts\n");
	return 0;

button_maps_err:
	pr_err("dts mismatch in %s and %d\n",
		"synaptics,num_button", num_of_buttons);
	kfree(button_map_ptr);
	return -EINVAL;
}

static int synaptics_rmi4_probe_dt(struct i2c_client *client)
{
	struct synaptics_dsx_platform_data *platform_data;
	struct device_node *np = client->dev.of_node;

	platform_data = kzalloc(sizeof(*platform_data), GFP_KERNEL);
	if (platform_data == NULL) {
		dev_err(&client->dev, "Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}

	/*
	 * Firstly, configure the initial button map layout.
	 * In case the platform DTS defines a button map the initial
	 * layout will be overridden by synaptics_rmi4_probe_dt.
	 */
	platform_data->cap_button_map = &dsx_cap_button_map;
	synaptics_rmi4_set_button_map(platform_data, np);

	client->dev.platform_data = platform_data;

	platform_data->irq_gpio =
			 of_get_named_gpio(np, "synaptics,irq-gpios", 0);
	if (platform_data->irq_gpio < 0) {
		dev_err(&client->dev, "of_get_named_gpio irq faild\n");
		return -EINVAL;
	}
	platform_data->reset_gpio =
			 of_get_named_gpio(np, "synaptics,reset-gpios", 0);
	if (platform_data->reset_gpio < 0) {
		dev_err(&client->dev, "of_get_named_gpio reset faild\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "synaptics,irq-flags",
			 &platform_data->irq_flags)) {
		dev_err(&client->dev, "failed to get synaptics,irq-flags property\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "synaptics,sensor_res_x",
			 &platform_data->sensor_res_x)) {
		dev_err(&client->dev, "failed to get synaptics,sensor_res_x property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "synaptics,sensor_res_y",
			 &platform_data->sensor_res_y)) {
		dev_err(&client->dev, "failed to get synaptics,sensor_res_y property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "synaptics,sensor_max_x",
			 &platform_data->sensor_max_x)) {
		dev_err(&client->dev, "failed to get synaptics,sensor_max_x property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "synaptics,sensor_max_y",
			 &platform_data->sensor_max_y)) {
		dev_err(&client->dev, "failed to get synaptics,sensor_max_y property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "synaptics,sensor_margin_x",
			 &platform_data->sensor_margin_x)) {
		dev_err(&client->dev, "failed to get synaptics,sensor_margin_x property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "synaptics,sensor_margin_y",
			 &platform_data->sensor_margin_y)) {
		dev_err(&client->dev, "failed to get synaptics,sensor_margin_y property\n");
		return -EINVAL;
	}


	platform_data->swap_axes = of_property_read_bool(np,
						"synaptics,swap_axes");
	platform_data->x_flip = of_property_read_bool(np,
						"synaptics,x_flip");
	platform_data->y_flip = of_property_read_bool(np,
						"synaptics,y_flip");


	return 0;
}

static int synaptics_rmi4_probe(struct i2c_client *client,
					const struct i2c_device_id *dev_id)
{
	struct synaptics_dsx_platform_data *platform_data;
	struct synaptics_rmi4_f1a_handle *f1a;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_data *rmi4_data;
	struct synaptics_rmi4_device_info *rmi;
	unsigned char attr_count;
	unsigned char ii;
	int retval = 0;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
			"%s: SMBus byte data not supported\n", __func__);
		return -EIO;
	}

#ifdef CONFIG_OF
	retval = synaptics_rmi4_probe_dt(client);
	if (retval == -ENOMEM) {
		dev_err(&client->dev,
		"%s: Failed to alloc mem for dsx_platform_data\n", __func__);
		return retval;
	} else if (retval == -EINVAL) {
		kfree(client->dev.platform_data);
		dev_err(&client->dev,
			"%s: Probe device tree data failed\n", __func__);
		return retval;
	}

	platform_data = client->dev.platform_data;
#else
	platform_data = client->dev.platform_data;
	if (!platform_data) {
		dev_err(&client->dev, "%s: No platform data found\n", __func__);
		return -EINVAL;
	}
#endif
	rmi4_data = kzalloc(sizeof(*rmi4_data) * 2, GFP_KERNEL);
	if (!rmi4_data) {
		dev_err(&client->dev,
			"%s: Failed to alloc mem for rmi4_data\n", __func__);
		return -ENOMEM;
	}

	rmi = &(rmi4_data->rmi4_mod_info);

	rmi4_data->input_dev = input_allocate_device();
	if (rmi4_data->input_dev == NULL) {
		dev_err(&client->dev,
			"%s: Failed to allocate input device\n", __func__);
		retval = -ENOMEM;
		goto err_input_device;
	}

	rmi4_data->i2c_client = client;
	rmi4_data->current_page = MASK_8BIT;
	rmi4_data->board = platform_data;
	rmi4_data->touch_stopped = false;
	rmi4_data->sensor_sleep = false;
	rmi4_data->i2c_read = synaptics_rmi4_i2c_read;
	rmi4_data->i2c_write = synaptics_rmi4_i2c_write;
	rmi4_data->irq_enable = synaptics_rmi4_irq_acquire;
	rmi4_data->reset_device = synaptics_rmi4_reset_device;
	rmi4_data->flip_x = rmi4_data->board->x_flip;
	rmi4_data->flip_y = rmi4_data->board->y_flip;
	rmi4_data->swap_axes = rmi4_data->board->swap_axes;

	init_waitqueue_head(&rmi4_data->wait);
	mutex_init(&(rmi4_data->rmi4_io_ctrl_mutex));
	spin_lock_init(&rmi4_data->irq_lock);

#ifdef CONFIG_OF
	rmi4_data->avdd = regulator_get(&client->dev, "avdd");
#else
	rmi4_data->avdd = regulator_get(NULL, platform_data->avdd_name);
#endif

	if (IS_ERR(rmi4_data->avdd)) {
		dev_err(&client->dev, "synaptics touch avdd power supply get failed\n");
		goto err_input_device;
	}
	regulator_set_voltage(rmi4_data->avdd, 2800000, 2800000);
	if (!regulator_en) {
		if (regulator_enable(rmi4_data->avdd)) {
			dev_err(&client->dev, "synaptics touch regulator enable failed\n");
			goto err_regulator_enable;
		}
		regulator_en = true;
	}
	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&client->dev,
			"%s: Failed to query device\n", __func__);
		goto err_query_device;
	} else {
		if (platform_data->sensor_res_x == 720)
			strcpy(panel_type, "720p_45");
		else if (platform_data->sensor_res_x == 1080)
			strcpy(panel_type, "1080p_50");
	}

	if (rmi4_data->swap_axes)
		synaptics_rmi4_swap_axis(rmi4_data);

	i2c_set_clientdata(client, rmi4_data);

	rmi4_data->input_dev->name = DRIVER_NAME;
	rmi4_data->input_dev->phys = INPUT_PHYS_NAME;
	rmi4_data->input_dev->id.bustype = BUS_I2C;
	rmi4_data->input_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	rmi4_data->input_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	rmi4_data->input_dev->dev.parent = &client->dev;

	input_set_drvdata(rmi4_data->input_dev, rmi4_data);

	set_bit(EV_SYN, rmi4_data->input_dev->evbit);
	set_bit(EV_KEY, rmi4_data->input_dev->evbit);
	set_bit(EV_ABS, rmi4_data->input_dev->evbit);
	set_bit(BTN_TOUCH, rmi4_data->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, rmi4_data->input_dev->keybit);

#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, rmi4_data->input_dev->propbit);
#endif

	input_set_abs_params(rmi4_data->input_dev, ABS_X, 0,
					rmi4_data->sensor_res_x, 0, 0);
	input_set_abs_params(rmi4_data->input_dev, ABS_Y, 0,
					rmi4_data->sensor_res_y, 0, 0);
	input_set_abs_params(rmi4_data->input_dev, ABS_PRESSURE, 0,
					255, 0, 0);
	input_set_abs_params(rmi4_data->input_dev, ABS_MT_POSITION_X, 0,
					rmi4_data->sensor_res_x, 0, 0);
	input_set_abs_params(rmi4_data->input_dev, ABS_MT_POSITION_Y, 0,
					rmi4_data->sensor_res_y, 0, 0);
	input_set_abs_params(rmi4_data->input_dev, ABS_MT_PRESSURE, 0,
					255, 0, 0);
#ifdef REPORT_2D_W
	input_set_abs_params(rmi4_data->input_dev, ABS_MT_TOUCH_MAJOR, 0,
					MAX_ABS_MT_TOUCH_MAJOR, 0, 0);
#endif

#ifdef TYPE_B_PROTOCOL
#ifdef KERNEL_ABOVE_3_7
	/* input_mt_init_slots now has a "flags" parameter */
	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers, INPUT_MT_DIRECT);
#else
	input_mt_init_slots(rmi4_data->input_dev, rmi4_data->num_of_fingers);
#endif
#endif

	f1a = NULL;
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
				f1a = fhandler->data;
		}
	}

	if (f1a) {
		for (ii = 0; ii < f1a->valid_button_count; ii++) {
			set_bit(f1a->button_map[ii],
					rmi4_data->input_dev->keybit);
			input_set_capability(rmi4_data->input_dev,
					EV_KEY, f1a->button_map[ii]);
		}
	}

	retval = input_register_device(rmi4_data->input_dev);
	if (retval) {
		dev_err(&client->dev,
			"%s: Failed to register input device\n", __func__);
		goto err_register_input;
	}

	if (!exp_fn_inited) {
		mutex_init(&exp_fn_list_mutex);
		INIT_LIST_HEAD(&exp_fn_list);
		exp_fn_inited = true;
	}

	rmi4_data->det_workqueue =
			create_singlethread_workqueue("rmi_det_workqueue");
	INIT_DELAYED_WORK(&rmi4_data->det_work, synaptics_rmi4_detection_work);
	queue_delayed_work(rmi4_data->det_workqueue, &rmi4_data->det_work,
				msecs_to_jiffies(EXP_FN_DET_INTERVAL));

	synaptics_set_virtual_key(rmi4_data->input_dev);

	if (platform_data->gpio_config) {
		retval = platform_data->gpio_config(platform_data->irq_gpio,
							true);
		if (retval < 0) {
			dev_err(&client->dev,
				"%s: Failed to configure GPIO\n", __func__);
			goto err_gpio;
		}
	}

	rmi4_data->irq = gpio_to_irq(platform_data->irq_gpio);

	retval = synaptics_rmi4_irq_acquire(rmi4_data, true);
	if (retval < 0) {
		dev_err(&client->dev,
			"%s: Failed to acquire irq\n", __func__);
		goto err_enable_irq;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&client->dev,
				"%s: Failed to create sysfs attributes\n",
				__func__);
			goto err_sysfs;
		}
	}

#ifdef CONFIG_PM_RUNTIME
	synaptics_rmi4_irq_enable(rmi4_data, false);
	pm_runtime_enable(&client->dev);
	pm_runtime_forbid(&client->dev);
#endif
	return retval;

err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
					&attrs[attr_count].attr);
	}

err_enable_irq:
err_gpio:
	input_unregister_device(rmi4_data->input_dev);

err_register_input:
err_query_device:
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
				synaptics_rmi4_f1a_kfree(fhandler);
			else
				kfree(fhandler->data);
			kfree(fhandler);
		}
	}

	input_free_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;
	if (regulator_en) {
		if (regulator_disable(rmi4_data->avdd))
			dev_err(&client->dev,
				"synaptics touch regulator disable failed\n");
		else
			regulator_en = false;
	}
err_regulator_enable:
	regulator_put(rmi4_data->avdd);
err_input_device:
#ifdef CONFIG_OF
	kfree(rmi4_data->board);
#endif
	kfree(rmi4_data);

	return retval;
}

static int synaptics_rmi4_remove(struct i2c_client *client)
{
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_data *rmi4_data = i2c_get_clientdata(client);
	struct synaptics_rmi4_device_info *rmi;
	unsigned char attr_count;

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&client->dev);
#endif
	rmi = &(rmi4_data->rmi4_mod_info);

	cancel_delayed_work_sync(&rmi4_data->det_work);
	flush_workqueue(rmi4_data->det_workqueue);
	destroy_workqueue(rmi4_data->det_workqueue);

	rmi4_data->touch_stopped = true;
	wake_up(&rmi4_data->wait);

	synaptics_rmi4_irq_acquire(rmi4_data, false);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
					&attrs[attr_count].attr);

	input_unregister_device(rmi4_data->input_dev);

	if (regulator_en) {
		if (regulator_disable(rmi4_data->avdd))
			dev_err(&client->dev,
				"synaptics touch regulator disable failed\n");
		else
			regulator_en = false;
	}
	regulator_put(rmi4_data->avdd);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
				synaptics_rmi4_f1a_kfree(fhandler);
			else
				kfree(fhandler->data);
			kfree(fhandler);
		}
	}
	input_free_device(rmi4_data->input_dev);

#ifdef CONFIG_OF
	/* if DTS defined a button map layout */
	if (rmi4_data->board->cap_button_map->nbuttons > 0)
		kfree(rmi4_data->board->cap_button_map->map);
	kfree(rmi4_data->board);
#endif
	kfree(rmi4_data);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static void synaptics_rmi4_sensor_sleep(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char device_ctrl;
	int retval;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl, sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
			"%s: Failed to enter sleep mode\n", __func__);
		rmi4_data->sensor_sleep = false;
		return;
	}

	device_ctrl = (device_ctrl & ~MASK_3BIT);
	device_ctrl = (device_ctrl | NO_SLEEP_OFF | SENSOR_SLEEP);

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl, sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
			"%s: Failed to enter sleep mode\n", __func__);
		rmi4_data->sensor_sleep = false;
		return;
	} else
		rmi4_data->sensor_sleep = true;

	return;
}

static void synaptics_rmi4_sensor_wake(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char device_ctrl;
	int retval;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl, sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
			"%s: Failed to wake from sleep mode\n", __func__);
		rmi4_data->sensor_sleep = true;
		return;
	}

	device_ctrl = (device_ctrl & ~MASK_3BIT);
	device_ctrl = (device_ctrl | NO_SLEEP_OFF | NORMAL_OPERATION);

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl, sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
			"%s: Failed to wake from sleep mode\n", __func__);
		rmi4_data->sensor_sleep = true;
		return;
	} else
		rmi4_data->sensor_sleep = false;

	return;
}

static int synaptics_rmi4_suspend(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!rmi4_data->sensor_sleep) {
		rmi4_data->touch_stopped = true;
		wake_up(&rmi4_data->wait);
		synaptics_rmi4_irq_enable(rmi4_data, false);
		synaptics_rmi4_sensor_sleep(rmi4_data);
	}

	if (regulator_en) {
		if (regulator_disable(rmi4_data->avdd))
			dev_err(dev, "synaptics touch regulator disable failed\n");
		else
			regulator_en = false;
	}

	return 0;
}

static int synaptics_rmi4_resume(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	int retval;

	if (!regulator_en) {
		if (regulator_enable(rmi4_data->avdd))
			dev_err(dev, "synaptics touch regulator enable failed\n");
		else
			regulator_en = true;
	}

	/*
	 * Power off touch is not recommended by Synaptics.
	 * If it's power off when suspend, should reset device to
	 * avoid it's in unknown state when resume.
	 */
	retval = synaptics_rmi4_reset_device(rmi4_data);
	if (retval < 0)
		dev_err(dev, "%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
	synaptics_rmi4_sensor_wake(rmi4_data);
	rmi4_data->touch_stopped = false;
	synaptics_rmi4_irq_enable(rmi4_data, true);

	return 0;
}

static const struct dev_pm_ops synaptics_rmi4_dev_pmops = {
	SET_RUNTIME_PM_OPS(synaptics_rmi4_suspend, synaptics_rmi4_resume, NULL)
};
#endif

static const struct i2c_device_id synaptics_rmi4_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};

static struct of_device_id synaptics_rmi4_dt_ids[] = {
	{ .compatible = "synaptics,s3202-touch", },
	{}
}
;
static struct i2c_driver synaptics_rmi4_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_RUNTIME
		.pm = &synaptics_rmi4_dev_pmops,
#endif
		.of_match_table = of_match_ptr(synaptics_rmi4_dt_ids),
	},
	.probe = synaptics_rmi4_probe,
	.remove = synaptics_rmi4_remove,
	.id_table = synaptics_rmi4_id_table,
};

static int __init synaptics_rmi4_init(void)
{
	return i2c_add_driver(&synaptics_rmi4_driver);
}

static void __exit synaptics_rmi4_exit(void)
{
	i2c_del_driver(&synaptics_rmi4_driver);
}

late_initcall(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX I2C Touch Driver");
MODULE_LICENSE("GPL v2");
