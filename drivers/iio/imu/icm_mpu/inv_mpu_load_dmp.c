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

#include "inv_mpu_iio.h"

static int inv_set_flip_pickup_gesture_params_V3(struct inv_mpu_state *st)
{
	int var_error_alpha;
	int still_threshold;
	int middle_still_threshold;
	int not_still_threshold;
	int vibration_rejection_threshold;
	int maximum_pickup_time_threshold;
	int pickup_timeout_threshold;
	int still_consistency_count_threshold;
	int motion_consistency_count_threshold;
	int vibration_count_threshold;
	int steady_tilt_threshold;
	int steady_tilt_upper_threshold;
	int accel_z_flat_threshold_minus;
	int accel_z_flat_threshold_plus;
	int device_in_pocket_threshold;
	int result;

        var_error_alpha                     = 107374182L;
        still_threshold                     = 4L;
        middle_still_threshold              = 10L;
        not_still_threshold                 = 40L;
        vibration_rejection_threshold       = 65100L;
        maximum_pickup_time_threshold       = 30L;
        pickup_timeout_threshold            = 150L;
        still_consistency_count_threshold   = 80;
        motion_consistency_count_threshold  = 10;
        vibration_count_threshold           = 3;
        steady_tilt_threshold               = 6710886L;
        steady_tilt_upper_threshold         = 140928614L;
        accel_z_flat_threshold_minus        = 60397978L;
        accel_z_flat_threshold_plus         = 6710886L;
        device_in_pocket_threshold          = 100L;

	result  = write_be32_to_mem(st, var_error_alpha, FP_VAR_ALPHA);
	result += write_be32_to_mem(st, still_threshold, FP_STILL_TH);
	result += write_be32_to_mem(st, middle_still_threshold,
							FP_MID_STILL_TH);
	result += write_be32_to_mem(st, not_still_threshold,
							FP_NOT_STILL_TH);
	result += write_be32_to_mem(st, vibration_rejection_threshold,
							FP_VIB_REJ_TH);
	result += write_be32_to_mem(st, maximum_pickup_time_threshold,
							FP_MAX_PICKUP_T_TH);
	result += write_be32_to_mem(st, pickup_timeout_threshold,
							FP_PICKUP_TIMEOUT_TH);
	result += write_be32_to_mem(st, still_consistency_count_threshold,
							FP_STILL_CONST_TH);
	result += write_be32_to_mem(st, motion_consistency_count_threshold,
							FP_MOTION_CONST_TH);
	result += write_be32_to_mem(st, vibration_count_threshold,
							FP_VIB_COUNT_TH);
	result += write_be32_to_mem(st, steady_tilt_threshold,
							FP_STEADY_TILT_TH);
	result += write_be32_to_mem(st, steady_tilt_upper_threshold,
							FP_STEADY_TILT_UP_TH);
	result += write_be32_to_mem(st, accel_z_flat_threshold_minus,
							FP_Z_FLAT_TH_MINUS);
	result += write_be32_to_mem(st, accel_z_flat_threshold_plus,
							FP_Z_FLAT_TH_PLUS);
	result += write_be32_to_mem(st, device_in_pocket_threshold,
							FP_DEV_IN_POCKET_TH);

	return result;
}

static int inv_load_firmware(struct inv_mpu_state *st)
{
	int bank, write_size;
	int result, size;
	u16 memaddr;
	u8 *data;

	data = st->firmware;
	size = st->dmp_image_size - DMP_OFFSET;
	for (bank = 0; size > 0; bank++, size -= write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;
		memaddr = (bank << 8);
		if (!bank) {
			memaddr = DMP_OFFSET;
			write_size = MPU_MEM_BANK_SIZE - DMP_OFFSET;
			data += DMP_OFFSET;
		}
		result = mem_w(memaddr, write_size, data);
		if (result) {
			pr_err("error writing firmware:%d\n", bank);
			return result;
		}
		data += write_size;
	}

	return 0;
}

static int inv_verify_firmware(struct inv_mpu_state *st)
{
	int bank, write_size, size;
	int result;
	u16 memaddr;
	u8 firmware[MPU_MEM_BANK_SIZE];
	u8 *data;

	data = st->firmware;
	size = st->dmp_image_size - DMP_OFFSET;
	for (bank = 0; size > 0; bank++, size -= write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;

		memaddr = (bank << 8);
		if (!bank) {
			memaddr = DMP_OFFSET;
			write_size = MPU_MEM_BANK_SIZE - DMP_OFFSET;
			data += DMP_OFFSET;
		}
		result = mem_r(memaddr, write_size, firmware);
		if (result)
			return result;
		if (0 != memcmp(firmware, data, write_size)) {
			pr_err("load data error, bank=%d\n", bank);
			return -EINVAL;
		}
		data += write_size;
	}
	return 0;
}


static int inv_setup_dmp_firmware(struct inv_mpu_state *st)
{
	int result;
	u8 v[4] = {0, 0};

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	result = mem_w(DATA_OUT_CTL1, 2, v);
	if (result)
		return result;
	result = mem_w(DATA_OUT_CTL2, 2, v);
	if (result)
		return result;
	result = mem_w(MOTION_EVENT_CTL, 2, v);
	if (result)
		return result;

	result = inv_set_flip_pickup_gesture_params_V3(st);

	return result;
}
/*
 * inv_firmware_load() -  calling this function will load the firmware.
 */
int inv_firmware_load(struct inv_mpu_state *st)
{
	int result;

	result = inv_switch_power_in_lp(st, true);
	if (result) {
		INVLOG(ERR,"load firmware set power error\n");
		goto firmware_write_fail;
	}
	result = inv_stop_dmp(st);
	if (result) {
		INVLOG(ERR,"load firmware:stop dmp error\n");
		goto firmware_write_fail;
	}
	result = inv_load_firmware(st);
	if (result) {
		INVLOG(ERR,"load firmware:load firmware eror\n");
		goto firmware_write_fail;
	}
	result = inv_verify_firmware(st);
	if (result) {
		INVLOG(ERR,"load firmware:verify firmware error\n");
		goto firmware_write_fail;
	}
	result = inv_setup_dmp_firmware(st);
	if (result)
		INVLOG(ERR,"load firmware:setup dmp error\n");
firmware_write_fail:
	result |= inv_set_power(st, false);
	if (result) {
		INVLOG(ERR,"load firmware:shuting down power error\n");
		return result;
	}

	st->chip_config.firmware_loaded = 1;

	return 0;
}

int inv_dmp_read(struct inv_mpu_state *st, int off, int size, u8 *buf)
{
	int bank, write_size, data, result;
	u16 memaddr;

	data = 0;
	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	inv_stop_dmp(st);
	for (bank = (off >> 8); size > 0; bank++, size -= write_size,
					data += write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;
		memaddr = (bank << 8);
		result = mem_r(memaddr, write_size, &buf[data]);

		if (result)
			return result;
	}

	return 0;
}
