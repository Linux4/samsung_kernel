/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/vmalloc.h>
#include "msm_sd.h"
#include "msm_actuator.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"

DEFINE_MSM_MUTEX(msm_actuator_mutex);

#undef CDBG
#ifdef MSM_ACUTUATOR_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

static struct msm_actuator msm_vcm_actuator_table;
static struct msm_actuator msm_piezo_actuator_table;
static struct msm_actuator msm_hall_effect_actuator_table;
static struct msm_actuator msm_hvcm_actuator_table;
static struct msm_actuator msm_dw9804_actuator_table;

static uint16_t hvca_inf_position, hvca_mac_position, torg;

static struct msm_actuator *actuators[] = {
	&msm_vcm_actuator_table,
	&msm_piezo_actuator_table,
	&msm_hall_effect_actuator_table,
	&msm_hvcm_actuator_table,
	&msm_dw9804_actuator_table,
};

static int32_t msm_actuator_piezo_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("Enter\n");

	if (a_ctrl->curr_step_pos != 0) {
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			a_ctrl->initial_code, 0, 0);
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			a_ctrl->initial_code, 0, 0);
		reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
		reg_setting.data_type = a_ctrl->i2c_data_type;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		rc = a_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
		if (rc < 0) {
			pr_err("%s: i2c write error:%d\n",
				__func__, rc);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->curr_step_pos = 0;
	}
	CDBG("Exit\n");
	return rc;
}

static void msm_actuator_parse_i2c_params(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, uint32_t hw_params, uint16_t delay)
{
	struct msm_actuator_reg_params_t *write_arr = a_ctrl->reg_tbl;
	uint32_t hw_dword = hw_params;
	uint16_t i2c_byte1 = 0, i2c_byte2 = 0;
	uint16_t value = 0;
	uint32_t size = a_ctrl->reg_tbl_size, i = 0;
	struct msm_camera_i2c_reg_array *i2c_tbl = a_ctrl->i2c_reg_tbl;
	struct msm_camera_i2c_seq_reg_array *i2c_seq = a_ctrl->i2c_reg_seq_tbl;
	CDBG("Enter\n");
	for (i = 0; i < size; i++) {
		if (write_arr[i].reg_write_type == MSM_ACTUATOR_WRITE_DAC_SEQ) {
			value = (next_lens_position <<
				write_arr[i].data_shift) |
				((hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift);
			i2c_seq[a_ctrl->i2c_tbl_index].reg_addr = write_arr[i].reg_addr;
			i2c_seq[a_ctrl->i2c_tbl_index].reg_data[0] = (value & 0xFF00) >> 8;
			i2c_seq[a_ctrl->i2c_tbl_index].reg_data[1] = value & 0xFF;
			i2c_seq[a_ctrl->i2c_tbl_index].reg_data_size = 2;
			CDBG("addr:0x%x 0x%x, 0x%x\n",
				i2c_seq[a_ctrl->i2c_tbl_index].reg_addr,
				i2c_seq[a_ctrl->i2c_tbl_index].reg_data[0],
				i2c_seq[a_ctrl->i2c_tbl_index].reg_data[1]);
			a_ctrl->i2c_tbl_index++;
			continue;
		}
		if (write_arr[i].reg_write_type == MSM_ACTUATOR_WRITE_DAC) {
			value = (next_lens_position <<
				write_arr[i].data_shift) |
				((hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift);

			if (write_arr[i].reg_addr != 0xFFFF) {
				i2c_byte1 = write_arr[i].reg_addr;
				i2c_byte2 = value;
				if (size != (i+1)) {
					i2c_byte2 = value & 0xFF;
					CDBG("byte1:0x%x, byte2:0x%x\n",
						i2c_byte1, i2c_byte2);
					i2c_tbl[a_ctrl->i2c_tbl_index].
						reg_addr = i2c_byte1;
					i2c_tbl[a_ctrl->i2c_tbl_index].
						reg_data = i2c_byte2;
					i2c_tbl[a_ctrl->i2c_tbl_index].
						delay = 0;
					a_ctrl->i2c_tbl_index++;
					i++;
					i2c_byte1 = write_arr[i].reg_addr;
					i2c_byte2 = (value & 0xFF00) >> 8;
				}
			} else {
				i2c_byte1 = (value & 0xFF00) >> 8;
				i2c_byte2 = value & 0xFF;
			}
		} else {
			i2c_byte1 = write_arr[i].reg_addr;
			i2c_byte2 = (hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift;
		}
		CDBG("i2c_byte1:0x%x, i2c_byte2:0x%x\n", i2c_byte1, i2c_byte2);
		i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1;
		i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
		i2c_tbl[a_ctrl->i2c_tbl_index].delay = delay;
		a_ctrl->i2c_tbl_index++;
	}
	CDBG("Exit\n");
}

static int32_t msm_actuator_init_focus(struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, enum msm_actuator_data_type type,
	struct reg_settings_t *settings)
{
	int32_t rc = -EFAULT;
	int32_t i = 0;
	CDBG("Enter\n");

	for (i = 0; i < size; i++) {
		switch (type) {
		case MSM_ACTUATOR_BYTE_DATA:
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
				&a_ctrl->i2c_client,
				settings[i].reg_addr,
				settings[i].reg_data, MSM_CAMERA_I2C_BYTE_DATA);
			break;
		case MSM_ACTUATOR_WORD_DATA:
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
				&a_ctrl->i2c_client,
				settings[i].reg_addr,
				settings[i].reg_data, MSM_CAMERA_I2C_WORD_DATA);
			break;
		default:
			pr_err("Unsupport data type: %d\n", type);
			break;
		}
		if (rc < 0)
			break;
	}

	a_ctrl->curr_step_pos = 0;
	CDBG("Exit\n");

	return rc;
}

static int32_t msm_actuator_vcm_init_focus(struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, enum msm_actuator_data_type type,
	struct reg_settings_t *settings)
{
  int32_t rc = -EFAULT;
  int32_t i = 0;
  uint16_t delay = 0;
  uint16_t status_addr = 0;
  uint16_t status_data = 0;
  uint16_t status = 0;
  uint16_t delay_count = 0;
  uint16_t delay_count_limit = 0;
  
  CDBG("%s Enter, size %d\n", __func__, size);

  for (i = 0; i < size; i++) {
    if(settings[i].reg_addr == MSM_ACTUATOR_INIT_FOCUS_DELAY){ 
      delay = settings[i].reg_data;
      usleep_range(delay, delay+1000);
      CDBG("%s %d settings[%d].reg_addr = %d, settings[%d].reg_data = %d\n", 
        __func__, __LINE__,i, settings[i].reg_addr, i, settings[i].reg_data);
    }
    else if(settings[i].reg_addr == MSM_ACTUATOR_INIT_FOCUS_READ_STATUS){
      /*reg_data[i] = limitation for delay count for read status*/
      delay_count_limit = settings[i].reg_data; 
      CDBG("%s %d settings[%d].reg_addr = %d, settings[%d].reg_data = %d\n", 
        __func__, __LINE__,i, settings[i].reg_addr, i, settings[i].reg_data);
      i++;
      /*reg_addr[i+1] = addr for status register*/
      status_addr = settings[i].reg_addr; 
      /*reg_data[i+1] = value for stopping to read the status register*/
      status_data = settings[i].reg_data; 
      CDBG("%s %d settings[%d].reg_addr = %d, settings[%d].reg_data = %d\n", 
        __func__, __LINE__,i, settings[i].reg_addr, i, settings[i].reg_data);
      delay_count = 0;
      do{
        rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
          &a_ctrl->i2c_client, status_addr, &status,
          MSM_CAMERA_I2C_BYTE_DATA);
        CDBG("%s %d status %d\n", __func__, __LINE__,status);
        if (rc < 0) {
          CDBG("%s Failed I2C read Line %d\n", __func__,
          __LINE__);
          return rc;
        }
        usleep_range(1000, 2000);
        delay_count++;
        if(delay_count >= delay_count_limit) break;
      }while(status != status_data);
    }
    else{
      CDBG("%s %d settings[%d].reg_addr = %d, settings[%d].reg_data = %d\n", 
           __func__, __LINE__,i, settings[i].reg_addr, i, settings[i].reg_data);

      rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
			&a_ctrl->i2c_client, settings[i].reg_addr,
			settings[i].reg_data, MSM_CAMERA_I2C_BYTE_DATA);
      if(rc < 0){
        pr_err("%s:%d I2C write failure size[%d] writen[%d]\n",
               __func__, __LINE__, size, i);
        break;
      }
    }
  }

  CDBG("Exit\n");
  return 0;
}

static int32_t msm_actuator_hall_effect_init_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, enum msm_actuator_data_type type,
	struct reg_settings_t *settings)
{
	int32_t rc = 0;

	CDBG("%s [QTI] Enter %d\n", __func__, __LINE__);
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&a_ctrl->i2c_client, 0x02, 0x00,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	usleep_range(1000, 1100);

	#if 0 ///////////////////////////////////////////////////////////////////////////////////
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&a_ctrl->i2c_client, 0x08, 0x00,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	usleep_range(1000, 1100);

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&a_ctrl->i2c_client, 0x02, 0x01,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	msleep(200);

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&a_ctrl->i2c_client, 0x03, 0x04,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	msleep(50);

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&a_ctrl->i2c_client, 0x02, 0x00,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	usleep_range(1000, 1100);
	
	#endif ///////////////////////////////////////////////////////////////////////////////////

	CDBG("%s exit Line %d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_actuator_hvcm_init_focus(struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, enum msm_actuator_data_type type,
	struct reg_settings_t *settings)
{
	int32_t rc;
	uint16_t posmsb, poslsb;
	uint8_t temp;

	CDBG("Enter\n");
	usleep_range(1000, 1100);
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x00,
	   0x08, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	// software reset, (OpMode_Nor | StCal_Off | TstMd_Off | NdPrbMd_Off | SWreset_On | AngBlg_Off | HalBias_Off | Output_Off)
	usleep_range(6000, 6100);
	CDBG("%s %d\n", __func__, __LINE__);
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x03,
	   0x00, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x00,
	   0x06, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
	   &a_ctrl->i2c_client,
	   0x2A,
	   &torg, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C read Line %d\n", __func__,
			__LINE__);
		return rc;
	}

	CDBG("%s %d torg=%d\n", __func__, __LINE__, torg);
	temp = 0x00 | (0x0F & torg);

	CDBG("%s %d temp=%d\n", __func__, __LINE__, temp);
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x0A,
	   temp, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
	   &a_ctrl->i2c_client,
	   0x04,
	   &poslsb, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C read Line %d\n", __func__,
			__LINE__);
		return rc;
	}

	CDBG("%s %d poslsb=%d\n", __func__, __LINE__, poslsb);
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
	   &a_ctrl->i2c_client,
	   0x05,
	   &posmsb, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C read Line %d\n", __func__,
			__LINE__);
		return rc;
	}

	posmsb = posmsb & 0x03;
	CDBG("%s %d posmsb=%d\n", __func__, __LINE__, posmsb);
	//write position
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x01,
	   poslsb, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x02,
	   posmsb, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	CDBG("%s %d position=%d\n", __func__, __LINE__, ((posmsb<<8)|poslsb));
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x00,
	   0x07, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	usleep_range(10000, 10100);
	temp = 0x20 | (0x0F & torg);

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x0A,
	   temp, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
	   &a_ctrl->i2c_client,
	   0x10,
	   &poslsb, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C read Line %d\n", __func__,
			__LINE__);
		return rc;
	}

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
	   &a_ctrl->i2c_client,
	   0x11,
	   &posmsb, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C read Line %d\n", __func__,
			__LINE__);
		return rc;
	}
	posmsb = posmsb & 0x03;
	hvca_inf_position = (posmsb << 8) | poslsb;
	pr_err("%s %d hvca_inf_position=%d\n", __func__, __LINE__, hvca_inf_position);
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
	   &a_ctrl->i2c_client,
	   0x12,
	   &poslsb, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C read Line %d\n", __func__,
			__LINE__);
		return rc;
	}

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
	   &a_ctrl->i2c_client,
	   0x13,
	   &posmsb, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C read Line %d\n", __func__,
			__LINE__);
		return rc;
	}
	posmsb = posmsb & 0x03;
	hvca_mac_position = (posmsb << 8) | poslsb;
	pr_err("%s %d hvca_mac_position=%d\n", __func__, __LINE__, hvca_mac_position);
	a_ctrl->curr_step_pos = 0;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_dw9804_init_focus(struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, enum msm_actuator_data_type type,
	struct reg_settings_t *settings)
{
	int32_t rc;
	CDBG("Enter\n");
	usleep_range(2000, 3000);
    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x02,
	   0x02, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x06,
	   0x40, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
	   &a_ctrl->i2c_client,
	   0x07,
	   0x06, MSM_CAMERA_I2C_BYTE_DATA); //0x84
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	CDBG("Exit\n");
	return rc;
}

static void msm_actuator_write_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t curr_lens_pos,
	struct damping_params_t *damping_params,
	int8_t sign_direction,
	int16_t code_boundary)
{
	int16_t next_lens_pos = 0;
	uint16_t damping_code_step = 0;
	uint16_t wait_time = 0;
	CDBG("Enter\n");

	damping_code_step = damping_params->damping_step;
	wait_time = damping_params->damping_delay;
	if (sign_direction == 1)
	   sign_direction = -1;
	else if (sign_direction == -1)
	   sign_direction = 1;

	/* Write code based on damping_code_step in a loop */
	for (next_lens_pos =
		curr_lens_pos + (sign_direction * damping_code_step);
		(sign_direction * next_lens_pos) <=
			(sign_direction * code_boundary);
		next_lens_pos =
			(next_lens_pos +
				(sign_direction * damping_code_step))) {
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			next_lens_pos, damping_params->hw_params, wait_time);
		curr_lens_pos = next_lens_pos;
	}

	if (curr_lens_pos != code_boundary) {
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			code_boundary, damping_params->hw_params, wait_time);
	}
	CDBG("Exit\n");
}

static int32_t msm_actuator_piezo_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t dest_step_position = move_params->dest_step_pos;
	int32_t rc = 0;
	int32_t num_steps = move_params->num_steps;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("Enter\n");

	if (num_steps == 0)
		return rc;

	a_ctrl->i2c_tbl_index = 0;
	a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
		(num_steps *
		a_ctrl->region_params[0].code_per_step),
		move_params->ringing_params[0].hw_params, 0);

	reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
	reg_setting.data_type = a_ctrl->i2c_data_type;
	reg_setting.size = a_ctrl->i2c_tbl_index;
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
		&a_ctrl->i2c_client, &reg_setting);
	if (rc < 0) {
		pr_err("i2c write error:%d\n", rc);
		return rc;
	}
	a_ctrl->i2c_tbl_index = 0;
	a_ctrl->curr_step_pos = dest_step_position;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_hall_effect_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	uint16_t wait_time = 0;
	int32_t dest_step_position = move_params->dest_step_pos;
	struct damping_params_t *damping_params =
		&move_params->ringing_params[0];
	uint16_t curr_lens_pos =
		a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	int16_t target_lens_pos =
		a_ctrl->step_position_table[dest_step_position];
	uint8_t lsb;
	uint8_t msb;
	CDBG("%s Enter %d\n", __func__, __LINE__);
	wait_time = damping_params->damping_delay;

	CDBG("%s curr_lens_pos=%d dest_lens_pos=%d\n", __func__,
		curr_lens_pos, target_lens_pos);
	CDBG("%s curr_step_pos=%d dest_step_position=%d\n", __func__,
		a_ctrl->curr_step_pos, dest_step_position);

	if (curr_lens_pos != target_lens_pos) {
		msb = ((target_lens_pos << 7) & 0x80);
		lsb = target_lens_pos >> 1;

		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
			&a_ctrl->i2c_client, 0x00, lsb,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s Failed I2C write Line %d\n", __func__,
				__LINE__);
			return rc;
		}

		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
			&a_ctrl->i2c_client, 0x01, msb,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s Failed I2C write Line %d\n", __func__,
				__LINE__);
			return rc;
		}
		usleep_range(wait_time, wait_time + 1000);
	}

	a_ctrl->curr_step_pos = dest_step_position;
	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}

static int msm_actuator_dw9804_write_dac(
    struct msm_actuator_ctrl_t *a_ctrl,
    uint8_t msb, uint8_t lsb, uint16_t wait_time)
{
    int32_t rc = 0;
    uint16_t status;
	uint16_t delay_count = 0;
    CDBG("%s %d Ender\n", __func__, __LINE__);
   do{
       rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
		&a_ctrl->i2c_client, 0x05, &status,
		MSM_CAMERA_I2C_BYTE_DATA);
       CDBG("%s %d status %d\n", __func__, __LINE__,status);
	if (rc < 0) {
		CDBG("%s Failed I2C read Line %d\n", __func__,
			__LINE__);
		return rc;
	}
        usleep_range(1000, 2000);
		delay_count++;
		if(delay_count >= 15) break;
   }while(status);
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&a_ctrl->i2c_client, 0x03, msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBG("%s Failed I2C write Line %d\n", __func__,
			__LINE__);
		return rc;
	}
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&a_ctrl->i2c_client, 0x04, lsb,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBG("%s Failed I2C write Line %d\n", __func__,
			__LINE__);
		return rc;
	}
	usleep_range(wait_time, wait_time+1000 );
    CDBG("%s %d End\n", __func__, __LINE__);
    return rc;
}
static int32_t msm_actuator_dw9804_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
    int32_t rc = 0;
    int32_t dest_step_pos = move_params->dest_step_pos;
    int32_t curr_step_pos = a_ctrl->curr_step_pos;
    int32_t damp_step_pos;
    struct damping_params_t *damping_params0 = &move_params->ringing_params[0];
    struct damping_params_t *damping_params1 = &move_params->ringing_params[1];
    uint16_t curr_lens_pos  = a_ctrl->step_position_table[curr_step_pos];
    uint16_t target_lens_pos = a_ctrl->step_position_table[dest_step_pos];
    uint16_t damping_lens_pos;
    uint8_t lsb, msb;
    uint32_t damp_delay0  = damping_params0->damping_delay;
    uint32_t damp_step0 = damping_params0->damping_step;
    uint32_t damp_threshold_index0 = (damping_params0->hw_params)&0x0000FFFF;
    uint32_t damp_threshold_step0 =  (damping_params0->hw_params>>16)&0x000000FF;
    uint32_t damp_delay1  = damping_params1->damping_delay;
    uint32_t damp_step1 = damping_params1->damping_step;
    uint32_t damp_threshold_index1 = (damping_params1->hw_params)&0x0000FFFF;
    uint32_t damp_threshold_step1 =  (damping_params1->hw_params>>16)&0x000000FF;
    uint32_t is_damping_first_time1 = (damping_params1->hw_params>>24)&0x000000FF;
    CDBG("%s Enter %d\n", __func__, __LINE__);
    CDBG("%s damp_step0=%d, damp_delay0=%d, damp_threshold_index0=%d, damp_threshold_step0=%d\n", __func__,
    damp_step0, damp_delay0, damp_threshold_index0,damp_threshold_step0);
    CDBG("%s damp_step1=%d, damp_delay1=%d, damp_threshold_index1=%d, damp_threshold_step1=%d, is_first_time_damping1 %d\n", __func__,
    damp_step1, damp_delay1, damp_threshold_index1,damp_threshold_step1,is_damping_first_time1);
    CDBG("%s curr_step_pos=%d dest_step_position=%d\n", __func__,a_ctrl->curr_step_pos, dest_step_pos);
    CDBG("%s curr_lens_pos=%d dest_lens_pos=%d\n", __func__,curr_lens_pos, target_lens_pos);
    if((curr_step_pos == 0)&&(is_damping_first_time1 == 0)){
        damping_params1->hw_params |= 0x01000000;
        damping_lens_pos = 100; /*not to start from 0*/
        do{
            damping_lens_pos = damping_lens_pos + (damp_step1*2);
            CDBG("%s %d damping_lens_pos %d\n", __func__, __LINE__,damping_lens_pos);
            if(damping_lens_pos >= target_lens_pos) break;
            else{
                msb = (damping_lens_pos>>8)&0x00ff;
                lsb = damping_lens_pos&0x00ff;
                rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, damp_delay1);
                if (rc < 0) {
                    CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
                    return rc;
                }
            }
        }while(damping_lens_pos < target_lens_pos);
        msb = (target_lens_pos>>8)&0x00ff;
        lsb = target_lens_pos&0x00ff;
        CDBG("%s %d target_lens_pos %d\n", __func__, __LINE__,target_lens_pos);
        rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, 1000);
        if (rc < 0) {
            CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
            return rc;
        }
    }
    else{
        if (curr_lens_pos != target_lens_pos) {
            if((dest_step_pos > curr_step_pos)||(abs(curr_step_pos-dest_step_pos)<damp_threshold_step0)){/*don't need to damping*/
                msb = (target_lens_pos>>8)&0x00ff;
                lsb = target_lens_pos&0x00ff;
                CDBG("%s %d target_lens_pos %d\n", __func__, __LINE__,target_lens_pos);
                rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, 1000);
                if (rc < 0) {
                    CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
                    return rc;
                }
            }
            else{/*need to damping*/
                damp_step_pos = curr_step_pos;
                if(damp_step_pos > damp_threshold_index0){
                    damp_step_pos = damp_threshold_index0;
                    damping_lens_pos = a_ctrl->step_position_table[damp_step_pos];
                    msb = (damping_lens_pos>>8)&0x00ff;
                    lsb = damping_lens_pos&0x00ff;
                    rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, damp_delay0);
                    if (rc < 0) {
                        CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
                        return rc;
                    }
                }
                do{
                    damp_step_pos -= damp_step0;
                    CDBG("%s %d damp_step_pos %d\n", __func__, __LINE__,damp_step_pos);
                    if(damp_step_pos <= dest_step_pos) break;
                    else{
                        damping_lens_pos = a_ctrl->step_position_table[damp_step_pos];
                        msb = (damping_lens_pos>>8)&0x00ff;
                        lsb = damping_lens_pos&0x00ff;
                        rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, damp_delay0);
                        if (rc < 0) {
                            CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
                            return rc;
                        }
                    }
                }while(damp_step_pos >= dest_step_pos);
                msb = (target_lens_pos>>8)&0x00ff;
                lsb = target_lens_pos&0x00ff;
                CDBG("%s %d target_lens_pos %d\n", __func__, __LINE__,target_lens_pos);
                rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, 1000);
                if (rc < 0) {
                    CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
                    return rc;
                }
            }
        }
    }
    a_ctrl->curr_step_pos = dest_step_pos;
	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_actuator_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	int8_t sign_dir = move_params->sign_dir;
	uint16_t step_boundary = 0;
	uint16_t target_step_pos = 0;
	uint16_t target_lens_pos = 0;
	int16_t dest_step_pos = move_params->dest_step_pos;
	uint16_t curr_lens_pos = 0;
	int dir = move_params->dir;
	int32_t num_steps = move_params->num_steps;


	CDBG("called, dir %d, num_steps %d\n", dir, num_steps);

	if (dest_step_pos == a_ctrl->curr_step_pos)
		return rc;

	curr_lens_pos = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	a_ctrl->i2c_tbl_index = 0;
	CDBG("curr_step_pos =%d dest_step_pos =%d curr_lens_pos=%d\n",
		a_ctrl->curr_step_pos, dest_step_pos, curr_lens_pos);

	while (a_ctrl->curr_step_pos != dest_step_pos) {
		step_boundary =
			a_ctrl->region_params[a_ctrl->curr_region_index].
			step_bound[dir];
		if ((dest_step_pos * sign_dir) <=
			(step_boundary * sign_dir)) {

			target_step_pos = dest_step_pos;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			a_ctrl->func_tbl->actuator_write_focus(a_ctrl,
					curr_lens_pos,
					&(move_params->
						ringing_params[a_ctrl->
						curr_region_index]),
					sign_dir,
					target_lens_pos);
			curr_lens_pos = target_lens_pos;

		} else {
			target_step_pos = step_boundary;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			a_ctrl->func_tbl->actuator_write_focus(a_ctrl,
					curr_lens_pos,
					&(move_params->ringing_params[a_ctrl->
						curr_region_index]),
					sign_dir,
					target_lens_pos);
			curr_lens_pos = target_lens_pos;

			a_ctrl->curr_region_index += sign_dir;
		}
		a_ctrl->curr_step_pos = target_step_pos;
	}
	if (a_ctrl->reg_tbl[0].reg_write_type == MSM_ACTUATOR_WRITE_DAC_SEQ) {
		struct msm_camera_i2c_seq_reg_setting reg_setting;

		reg_setting.reg_setting = a_ctrl->i2c_reg_seq_tbl;
		reg_setting.addr_type = a_ctrl->i2c_client.addr_type;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq_table(
			&a_ctrl->i2c_client, &reg_setting);
	} else {
		struct msm_camera_i2c_reg_setting reg_setting;
		reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
		reg_setting.data_type = a_ctrl->i2c_data_type;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
	}
	if (rc < 0) {
		pr_err("i2c write error:%d\n", rc);
		return rc;
	}
	a_ctrl->i2c_tbl_index = 0;
	CDBG("Exit\n");

	return rc;
}

static int32_t msm_actuator_init_step_table(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int16_t code_per_step = 0;
	int16_t cur_code = 0;
	int16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	CDBG("Enter\n");

	for (; data_size > 0; data_size--)
		max_code_size *= 2;

	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;

	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);

	if (a_ctrl->step_position_table == NULL)
		return -ENOMEM;

	cur_code = set_info->af_tuning_params.initial_code;
	a_ctrl->step_position_table[step_index++] = cur_code;
	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		code_per_step =
			a_ctrl->region_params[region_index].code_per_step;
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		for (; step_index <= step_boundary;
			step_index++) {
			cur_code += code_per_step;
			if (cur_code < max_code_size)
				a_ctrl->step_position_table[step_index] =
					cur_code;
			else {
				for (; step_index <
					set_info->af_tuning_params.total_steps;
					step_index++)
					a_ctrl->
						step_position_table[
						step_index] =
						max_code_size;
			}
		}
	}
#ifdef MSM_ACUTUATOR_DEBUG
{
	uint16_t i;
	CDBG("******** step table **************");
	for (i = 0; i < step_index; i++) {
		CDBG("[%d] %d(0x%x)", i, a_ctrl->step_position_table[i],
			a_ctrl->step_position_table[i]);
	}
	CDBG("**********************************");
}
#endif

	CDBG("Exit\n");
	return 0;
}

static int32_t msm_actuator_vcm_init_step_table(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int16_t code_per_step = 0;
	int16_t cur_code = 0;
	int16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	CDBG("Enter\n");
	for (; data_size > 0; data_size--)
		max_code_size *= 2;
	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);
	if (a_ctrl->step_position_table == NULL)
		return -ENOMEM;
	cur_code = set_info->af_tuning_params.initial_code;
	a_ctrl->step_position_table[step_index++] = cur_code;
	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		code_per_step =
			a_ctrl->region_params[region_index].code_per_step;
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		if (step_boundary > set_info->af_tuning_params.total_steps - 1) {
			CDBG("%s: Error af steps mismatch!", __func__);
			return -EFAULT;
		}
		for (; step_index <= step_boundary;
			step_index++) {
			cur_code += code_per_step;
			if (cur_code < max_code_size)
				a_ctrl->step_position_table[step_index] =
					cur_code;
			else {
				for (; step_index <
					set_info->af_tuning_params.total_steps;
					step_index++)
					a_ctrl->
						step_position_table[
						step_index] =
						max_code_size;
			}
		}
	}
	for (step_index = 0;
		step_index < set_info->af_tuning_params.total_steps;
		step_index++) {
		CDBG("step_position_table[%d] = %d", step_index,
			a_ctrl->step_position_table[step_index]);
	}
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_actuator_hvcm_init_step_table(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int16_t code_per_step = 0;
	int16_t cur_code = 0;
	int16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	CDBG("Enter\n");
	for (; data_size > 0; data_size--)
		max_code_size *= 2;
	max_code_size = 0;
	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;
	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);
	if (a_ctrl->step_position_table == NULL)
		return -ENOMEM;
	//cur_code = set_info->af_tuning_params.initial_code;
	cur_code = hvca_inf_position;
	/*Needs to be enabled once we have proper values in the EEPROM*/
	//a_ctrl->region_params[0].code_per_step =
	  //  (hvca_inf_position  - hvca_mac_position) /
	    //(set_info->af_tuning_params.total_steps - 10);
	a_ctrl->step_position_table[step_index++] = cur_code;
	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		code_per_step =
			a_ctrl->region_params[region_index].code_per_step;
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		for (; step_index <= step_boundary;
			step_index++) {
			cur_code -= code_per_step;
			if (cur_code > max_code_size)
				a_ctrl->step_position_table[step_index] =
					cur_code;
			else {
				for (; step_index <
					set_info->af_tuning_params.total_steps;
					step_index++)
					a_ctrl->
						step_position_table[
						step_index] =
						max_code_size;
			}
		}
	}
	for (step_index = 0; step_index <
	      set_info->af_tuning_params.total_steps;
	      step_index++)
	      pr_err("step_position_table[%d] = %d\n",
	      step_index, a_ctrl->step_position_table[step_index]);
	CDBG("Exit\n");
	return 0;
}
static int32_t msm_actuator_dw9804_init_step_table(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int16_t code_per_step = 0;
	int16_t cur_code = 0;
	int16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	CDBG("Enter\n");
	for (; data_size > 0; data_size--)
		max_code_size *= 2;
	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);
	if (a_ctrl->step_position_table == NULL)
		return -ENOMEM;
	cur_code = set_info->af_tuning_params.initial_code;
	a_ctrl->step_position_table[step_index++] = cur_code;
	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		code_per_step =
			a_ctrl->region_params[region_index].code_per_step;
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		if (step_boundary > set_info->af_tuning_params.total_steps - 1) {
			CDBG("%s: Error af steps mismatch!", __func__);
			return -EFAULT;
		}
		for (; step_index <= step_boundary;
			step_index++) {
			cur_code += code_per_step;
			if (cur_code < max_code_size)
				a_ctrl->step_position_table[step_index] =
					cur_code;
			else {
				for (; step_index <
					set_info->af_tuning_params.total_steps;
					step_index++)
					a_ctrl->
						step_position_table[
						step_index] =
						max_code_size;
			}
		}
	}
	for (step_index = 0;
		step_index < set_info->af_tuning_params.total_steps;
		step_index++) {
		CDBG("step_position_table[%d] = %d", step_index,
			a_ctrl->step_position_table[step_index]);
	}
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_actuator_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	CDBG("Enter\n");

	if (a_ctrl->curr_step_pos != 0)
		rc = a_ctrl->func_tbl->actuator_move_focus(a_ctrl, move_params);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_power_down(struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	if (a_ctrl->vcm_enable) {
		rc = gpio_direction_output(a_ctrl->vcm_pwd, 0);
		if (!rc)
			gpio_free(a_ctrl->vcm_pwd);
	}
	CDBG("%s Exit:%d, Addr:0x%x\n", __func__, rc,
		a_ctrl->i2c_client.client->addr);

	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;
	kfree(a_ctrl->i2c_reg_tbl);
	a_ctrl->i2c_reg_tbl = NULL;
	vfree(a_ctrl->i2c_reg_seq_tbl);
	a_ctrl->i2c_reg_seq_tbl = NULL;
	a_ctrl->i2c_tbl_index = 0;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_set_position(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_position_t *set_pos)
{
	int32_t rc = 0;
	int32_t index;
	uint8_t lsb, msb;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("%s Enter : steps = %d \n", __func__, set_pos->number_of_steps);
	if (set_pos->number_of_steps  == 0)
		return rc;

	a_ctrl->i2c_tbl_index = 0;
	for (index = 0; index < set_pos->number_of_steps; index++) {
		msb = ((set_pos->pos[index] << 7) & 0x80);
		lsb = set_pos->pos[index] >> 1;
		CDBG("%s index=%d msb= 0x%X, lsb=0x%X\n", __func__, index, msb, lsb);
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_addr = 0x00;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_data = lsb;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].delay = 0;
		a_ctrl->i2c_tbl_index++;

		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_addr = 0x01;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_data = msb;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].delay = set_pos->delay[index];
		a_ctrl->i2c_tbl_index++;
	}

	reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
	reg_setting.data_type = a_ctrl->i2c_data_type;
	reg_setting.size = a_ctrl->i2c_tbl_index;
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}

static void msm_actuator_set_position_tbl(
  struct msm_actuator_ctrl_t *a_ctrl, 
  uint16_t pos, uint16_t delay)
{
  uint16_t msb, lsb, reg_addr;
  reg_addr = a_ctrl->reg_tbl[0].reg_addr;
  CDBG("%s reg_addr = %d\n", __func__, reg_addr);

  msb = (pos>>8)&0x00ff;
  lsb = pos&0x00ff;

  CDBG("%s pos=%d msb= 0x%X, lsb=0x%X\n", __func__, pos, msb, lsb);
  a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_addr = reg_addr;
  a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_data = msb;
  a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].delay = 0;
  a_ctrl->i2c_tbl_index++;
  
  a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_addr = reg_addr+1;
  a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_data = lsb;
  a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].delay = delay;
  a_ctrl->i2c_tbl_index++;

}

static int32_t msm_actuator_vcm_set_position(
    struct msm_actuator_ctrl_t *a_ctrl,
    struct msm_actuator_set_position_t *set_pos)
{
 
   int32_t rc = 0;
   int32_t index;
   uint16_t pos, delay;
   struct msm_camera_i2c_reg_setting reg_setting;
 
   CDBG("%s Enter : steps = %d\n", __func__, set_pos->number_of_steps);
 
   if (set_pos->number_of_steps  == 0)
     return rc;
 
   a_ctrl->i2c_tbl_index = 0;
 
   for (index = 0; index < set_pos->number_of_steps; index++) {
      pos = a_ctrl->step_position_table[set_pos->pos[index]];
      delay = set_pos->delay[index];
      msm_actuator_set_position_tbl(a_ctrl, pos, delay); 
   }
 
   reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
   reg_setting.data_type = a_ctrl->i2c_data_type;
   reg_setting.size = a_ctrl->i2c_tbl_index;
   rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
       &a_ctrl->i2c_client, &reg_setting);
   if (rc < 0) {
     pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
     return rc;
   }
 
   CDBG("%s exit %d\n", __func__, __LINE__);
   return rc;
}

 /*Added by Justin_Qualcomm for SEMCO Actuator Direct Move : 20130718*/
static int32_t msm_actuator_hvcm_set_position(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_position_t *set_pos)
{
	int32_t rc = 0;
	int32_t index;
	uint8_t lsb, msb;
        struct msm_camera_i2c_reg_setting reg_setting;
	
	CDBG("%s Enter : steps = %d \n", __func__, set_pos->number_of_steps);
	if (set_pos->number_of_steps  == 0)
		return rc;

	a_ctrl->i2c_tbl_index = 0;
	for (index = 0; index < set_pos->number_of_steps; index++) {
		msb = (set_pos->pos[index]>>8)&0x00ff;
		lsb = set_pos->pos[index]&0x00ff;
		CDBG("%s index=%d msb= 0x%X, lsb=0x%X\n", __func__, index, msb, lsb);
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_addr = 0x01;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_data = lsb;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].delay = 0;
		a_ctrl->i2c_tbl_index++;

		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_addr = 0x02;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_data = msb;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].delay = set_pos->delay[index];
		a_ctrl->i2c_tbl_index++;
	}
	reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
	reg_setting.data_type = a_ctrl->i2c_data_type;
	reg_setting.size = a_ctrl->i2c_tbl_index;

	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
		&a_ctrl->i2c_client, &reg_setting);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}

	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}
 /*End - Added by Justin_Qualcomm for SEMCO Actuator Direct Move : 20130718*/

static int32_t msm_actuator_dw9804_set_position(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_position_t *set_pos)
{
	int32_t rc = 0;
	int32_t index;
	uint8_t lsb, msb;
        struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("%s Enter : steps = %d \n", __func__, set_pos->number_of_steps);
	if (set_pos->number_of_steps  == 0)
		return rc;
	a_ctrl->i2c_tbl_index = 0;
	for (index = 0; index < set_pos->number_of_steps; index++) {
		msb = (set_pos->pos[index]>>8)&0x00ff;
		lsb = set_pos->pos[index]&0x00ff;
		CDBG("%s index=%d msb= 0x%X, lsb=0x%X\n", __func__, index, msb, lsb);
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_addr = 0x03;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_data = msb;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].delay = 0;
		a_ctrl->i2c_tbl_index++;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_addr = 0x04;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].reg_data = lsb;
		a_ctrl->i2c_reg_tbl[a_ctrl->i2c_tbl_index].delay = set_pos->delay[index];
		a_ctrl->i2c_tbl_index++;
	}
	reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
	reg_setting.data_type = a_ctrl->i2c_data_type;
	reg_setting.size = a_ctrl->i2c_tbl_index;
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
		&a_ctrl->i2c_client, &reg_setting);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_actuator_init(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info) {
	int32_t rc = -EFAULT;
	uint16_t i = 0;
	CDBG("Enter\n");

	for (i = 0; i < ARRAY_SIZE(actuators); i++) {
		if (set_info->actuator_params.act_type ==
			actuators[i]->act_type) {
			a_ctrl->func_tbl = &actuators[i]->func_tbl;
			rc = 0;
		}
	}

	if (rc < 0) {
		pr_err("Actuator function table not found\n");
		return rc;
	}

	a_ctrl->region_size = set_info->af_tuning_params.region_size;
	if (a_ctrl->region_size > MAX_ACTUATOR_REGION) {
		pr_err("MAX_ACTUATOR_REGION is exceeded.\n");
		return -EFAULT;
	}
	a_ctrl->pwd_step = set_info->af_tuning_params.pwd_step;
	a_ctrl->total_steps = set_info->af_tuning_params.total_steps;

	if (copy_from_user(&a_ctrl->region_params,
		(void *)set_info->af_tuning_params.region_params,
		a_ctrl->region_size * sizeof(struct region_params_t)))
		return -EFAULT;

	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		struct msm_camera_cci_client *cci_client = a_ctrl->i2c_client.cci_client;
		cci_client->sid =
			set_info->actuator_params.i2c_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->cci_i2c_master = a_ctrl->cci_master;
	} else {
		a_ctrl->i2c_client.client->addr =
			set_info->actuator_params.i2c_addr;
	}

	a_ctrl->i2c_data_type = set_info->actuator_params.i2c_data_type;
	a_ctrl->i2c_client.addr_type = set_info->actuator_params.i2c_addr_type;
	a_ctrl->reg_tbl_size = set_info->actuator_params.reg_tbl_size;
	if (a_ctrl->reg_tbl_size > MAX_ACTUATOR_REG_TBL_SIZE) {
		pr_err("MAX_ACTUATOR_REG_TBL_SIZE is exceeded.\n");
		return -EFAULT;
	}
	if (a_ctrl->i2c_reg_tbl != NULL){
		kfree(a_ctrl->i2c_reg_tbl);
		a_ctrl->i2c_reg_tbl = NULL;
	}
	if (a_ctrl->i2c_reg_seq_tbl != NULL) {
		vfree(a_ctrl->i2c_reg_seq_tbl);
		a_ctrl->i2c_reg_seq_tbl = NULL;
	}
	a_ctrl->i2c_reg_tbl =
		kmalloc(sizeof(struct msm_camera_i2c_reg_array) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);
	if (!a_ctrl->i2c_reg_tbl) {
		pr_err("kmalloc fail\n");
		goto ERROR;
	}
	a_ctrl->i2c_reg_seq_tbl =
		vmalloc(sizeof(struct msm_camera_i2c_seq_reg_array) *
		(set_info->af_tuning_params.total_steps + 1));
	if (!a_ctrl->i2c_reg_seq_tbl) {
		pr_err("kmalloc fail\n");
		goto ERROR;
	}

	if (copy_from_user(&a_ctrl->reg_tbl,
		(void *)set_info->actuator_params.reg_tbl_params,
		a_ctrl->reg_tbl_size *
		sizeof(struct msm_actuator_reg_params_t))) {
		goto ERROR;
	}

	if (set_info->actuator_params.init_setting_size) {
		if (a_ctrl->func_tbl->actuator_init_focus) {
			struct reg_settings_t *init_settings = kmalloc(sizeof(struct reg_settings_t) *
				(set_info->actuator_params.init_setting_size),
				GFP_KERNEL);
			if (init_settings == NULL) {
				pr_err("Error allocating memory for init_settings\n");
				goto ERROR;
			}
			if (copy_from_user(init_settings,
				(void *)set_info->actuator_params.init_settings,
				set_info->actuator_params.init_setting_size *
				sizeof(struct reg_settings_t))) {
				pr_err("Error copying init_settings\n");
				if (init_settings) {
					kfree(init_settings);
					init_settings = NULL;
				}
				goto ERROR;
			}
			rc = a_ctrl->func_tbl->actuator_init_focus(a_ctrl,
				set_info->actuator_params.init_setting_size,
				a_ctrl->i2c_data_type,
				init_settings);
			if (rc < 0) {
				pr_err("Error actuator_init_focus\n");
				pr_err("Error copying init_settings\n");
				if (init_settings) {
					kfree(init_settings);
					init_settings = NULL;
				}
				goto ERROR;
			}
			kfree(init_settings);
			init_settings = NULL;
		}
	}

	a_ctrl->initial_code = set_info->af_tuning_params.initial_code;
	if (a_ctrl->func_tbl->actuator_init_step_table)
		rc = a_ctrl->func_tbl->
			actuator_init_step_table(a_ctrl, set_info);

	a_ctrl->curr_step_pos = 0;
	a_ctrl->curr_region_index = 0;
	CDBG("Exit\n");

	return rc;
ERROR:
	if (a_ctrl->i2c_reg_tbl != NULL) {
		kfree(a_ctrl->i2c_reg_tbl);
		a_ctrl->i2c_reg_tbl = NULL;
	}
	if (a_ctrl->i2c_reg_seq_tbl != NULL) {
		vfree(a_ctrl->i2c_reg_seq_tbl);
		a_ctrl->i2c_reg_seq_tbl = NULL;
	}
	return -EFAULT;
}

static int32_t msm_actuator_config(struct msm_actuator_ctrl_t *a_ctrl,
	void __user *argp)
{
	struct msm_actuator_cfg_data *cdata =
		(struct msm_actuator_cfg_data *)argp;
	int32_t rc = 0;
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	mutex_lock(a_ctrl->actuator_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_ACTUATOR_INFO:
		cdata->is_af_supported = 1;
		cdata->cfg.cam_name = a_ctrl->cam_name;
		CDBG("%s a_ctrl->cam_name = %d\n", __func__, a_ctrl->cam_name);
		break;

	case CFG_SET_ACTUATOR_INFO:
		rc = msm_actuator_init(a_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("init table failed %d\n", rc);
		break;

	case CFG_SET_DEFAULT_FOCUS:
		rc = a_ctrl->func_tbl->actuator_set_default_focus(a_ctrl,
			&cdata->cfg.move);
		if (rc < 0)
			pr_err("move focus failed %d\n", rc);
		break;

	case CFG_MOVE_FOCUS:
		rc = a_ctrl->func_tbl->actuator_move_focus(a_ctrl,
			&cdata->cfg.move);
		if (rc < 0)
			pr_err("move focus failed %d\n", rc);
		break;

	case CFG_SET_POSITION:
		rc = a_ctrl->func_tbl->actuator_set_position(a_ctrl,
			&cdata->cfg.setpos);
		if (rc < 0)
			pr_err("actuator_set_position failed %d\n", rc);
		break;
	case CFG_SET_ACTUATOR_SW_LANDING:
		if (a_ctrl && a_ctrl->func_tbl && a_ctrl->func_tbl->actuator_sw_landing) {
		  rc = a_ctrl->func_tbl->actuator_sw_landing(a_ctrl,
			  &cdata->cfg.move);
		  if (rc < 0)
			  pr_err("actuator_sw_landing failed %d\n", rc);
		}
		break;
	default:
		break;
	}
	mutex_unlock(a_ctrl->actuator_mutex);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_get_subdev_id(struct msm_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	*subdev_id = a_ctrl->subdev_id;
	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
};

static int32_t msm_actuator_vcm_sw_landing(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
   int32_t rc = 0;
   struct msm_camera_i2c_reg_setting reg_setting;
   struct damping_params_t *damping_params;
   int16_t pos_index, First_damping_lens_pos, Second_damping_lens_pos, damp_delay, damp_step, lens_pos;
   
   if(a_ctrl == NULL || a_ctrl->step_position_table == NULL || move_params == NULL)
   {
      pr_err("%s %d NULL Pointer\n", __func__, __LINE__);
      return 0;
   }
   
   CDBG("%s Enter %d\n", __func__, __LINE__);
   /*get first and second damping position, damp delay and step*/
   damping_params = &move_params->ringing_params[2];
   damp_delay = damping_params->damping_delay;
   damp_step = damping_params->damping_step;
   pos_index = damping_params->hw_params;
   First_damping_lens_pos = a_ctrl->step_position_table[pos_index];
   Second_damping_lens_pos = (First_damping_lens_pos>>2);
   CDBG("%s %d First damping index =%d, First_damping_lens_pos=%d, Second_damping_lens_pos=%d\n",
        __func__,__LINE__, pos_index, First_damping_lens_pos, Second_damping_lens_pos);
   CDBG("%s %d damp_delay=%d, damp_step=%d\n", __func__, __LINE__, damp_delay, damp_step);
   /*set the starting lens pos to first damping lens pos*/
   lens_pos = First_damping_lens_pos;//
   CDBG("%s %d curr_lens_pos=%d\n", __func__, __LINE__, lens_pos);

   /*set the landing position in the table*/
   CDBG("%s %d set the landing position in the table\n", __func__, __LINE__);
   a_ctrl->i2c_tbl_index = 0;
   do{
       if(lens_pos <= Second_damping_lens_pos) break; 
       else{
           lens_pos -= damp_step;
           msm_actuator_set_position_tbl(a_ctrl, lens_pos, damp_delay);
       }
   }while(lens_pos > Second_damping_lens_pos);

   /*set final position*/ 
   CDBG("%s %d set the final position in the table\n", __func__, __LINE__);
   lens_pos = 0;
   msm_actuator_set_position_tbl(a_ctrl, lens_pos, damp_delay);

   /*write i2c */
   CDBG("%s %d write i2c\n", __func__, __LINE__);
   reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
   reg_setting.data_type = a_ctrl->i2c_data_type;
   reg_setting.size = a_ctrl->i2c_tbl_index;
   rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
       &a_ctrl->i2c_client, &reg_setting);
   if (rc < 0) {
     pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
     return rc;
   }

   return rc;
}

static int32_t msm_actuator_dw9804_sw_landing(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
    int32_t rc = 0;
    uint16_t msb, lsb;
    uint16_t damping_steps = 10;
    int32_t curr_step_pos;
    uint16_t curr_lens_pos;
    CDBG("%s Enter %d\n", __func__, __LINE__);
   if(a_ctrl == NULL || a_ctrl->step_position_table == NULL || move_params == NULL)
   {
      pr_err("%s %d NULL Pointer\n", __func__, __LINE__);
      return 0;
   }
    curr_step_pos = a_ctrl->curr_step_pos;
    curr_lens_pos  = a_ctrl->step_position_table[curr_step_pos];
    CDBG("%s %d soft-landing here!!!!\n", __func__, __LINE__);
    if(curr_lens_pos > 370){ /*370 means DAC code, it's around infinity position in S3VE module*/
        curr_lens_pos = 370;
        msb = (curr_lens_pos>>8)&0x00ff;
        lsb = curr_lens_pos&0x00ff;
        CDBG("%s %d landing to %d\n", __func__, __LINE__,curr_lens_pos);
        rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, 1000);
        if (rc < 0) {
            CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
            return rc;
        }
    }
    do{
        if(curr_lens_pos <= 100) break; /*100 is minimum start current value in DAC code for S3VE module*/
        else{
            curr_lens_pos -= damping_steps;
            msb = (curr_lens_pos>>8)&0x00ff;
            lsb = curr_lens_pos&0x00ff;
            CDBG("%s %d landing to %d\n", __func__, __LINE__,curr_lens_pos);
            rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, 1000);
            if (rc < 0) {
                CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
                return rc;
            }
        }
    }while(curr_lens_pos > 100);
    curr_lens_pos = 0;
    msb = (curr_lens_pos>>8)&0x00ff;
    lsb = curr_lens_pos&0x00ff;
    CDBG("%s %d final landing position %d\n", __func__, __LINE__,curr_lens_pos);
    rc = msm_actuator_dw9804_write_dac(a_ctrl, msb, lsb, 1000);
    if (rc < 0) {
        CDBG("%s Failed I2C write Line %d\n", __func__,__LINE__);
        return rc;
    }
    return rc;
}

static int msm_actuator_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_actuator_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	if (a_ctrl->gpio_conf && a_ctrl->gpio_conf->cam_gpio_req_tbl) {
		CDBG("%s:%d request gpio\n", __func__, __LINE__);
		rc = msm_camera_request_gpio_table(
			a_ctrl->gpio_conf->cam_gpio_req_tbl,
			a_ctrl->gpio_conf->cam_gpio_req_tbl_size, 1);
		if (rc < 0) {
			pr_err("%s: request gpio failed\n", __func__);
			return rc;
		}
	}
	CDBG("Exit\n");
	return rc;
}

static int msm_actuator_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_actuator_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("%s: Enter\n", __func__);
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	CDBG("%s Enter %d, rc=%d\n", __func__, __LINE__,rc);
	if (a_ctrl->gpio_conf && a_ctrl->gpio_conf->cam_gpio_req_tbl) {
		CDBG("%s:%d release gpio\n", __func__, __LINE__);
		msm_camera_request_gpio_table(
			a_ctrl->gpio_conf->cam_gpio_req_tbl,
			a_ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	vfree(a_ctrl->i2c_reg_seq_tbl);
	a_ctrl->i2c_reg_seq_tbl = NULL;
	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_actuator_internal_ops = {
	.open = msm_actuator_open,
	.close = msm_actuator_close,
};

static long msm_actuator_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_actuator_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d a_ctrl %p argp %p\n", __func__, __LINE__, a_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_actuator_get_subdev_id(a_ctrl, argp);
	case VIDIOC_MSM_ACTUATOR_CFG:
		return msm_actuator_config(a_ctrl, argp);
	case MSM_SD_SHUTDOWN:
		return 0;
	default:
		return -ENOIOCTLCMD;
	}
}

static int32_t msm_actuator_power_up(struct msm_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("%s called\n", __func__);

	CDBG("vcm info: %x %x\n", a_ctrl->vcm_pwd,
		a_ctrl->vcm_enable);
	if (a_ctrl->vcm_enable) {
		rc = gpio_request(a_ctrl->vcm_pwd, "msm_actuator");
		if (!rc) {
			CDBG("Enable VCM PWD\n");
			gpio_direction_output(a_ctrl->vcm_pwd, 1);
		}
	}
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_actuator_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	CDBG("%s:%d \n", __func__, __LINE__);
	mutex_lock(a_ctrl->actuator_mutex);
	if (on)
		rc = msm_actuator_power_up(a_ctrl);
	else
		rc = msm_actuator_power_down(a_ctrl);
	mutex_unlock(a_ctrl->actuator_mutex);
	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_actuator_subdev_core_ops = {
	.ioctl = msm_actuator_subdev_ioctl,
	.s_power = msm_actuator_power,
};

static struct v4l2_subdev_ops msm_actuator_subdev_ops = {
	.core = &msm_actuator_subdev_core_ops,
};

static int32_t msm_actuator_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "qcom,gpio-req-tbl-num", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		pr_err("%s qcom,gpio-req-tbl-num 0\n", __func__);
		return 0;
	}

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	gconf->cam_gpio_req_tbl = kzalloc(sizeof(struct gpio) * count,
		GFP_KERNEL);
	if (!gconf->cam_gpio_req_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}
	gconf->cam_gpio_req_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		if (val_array[i] >= gpio_array_size) {
			pr_err("%s gpio req tbl index %d invalid\n",
				__func__, val_array[i]);
			return -EINVAL;
		}
		gconf->cam_gpio_req_tbl[i].gpio = gpio_array[val_array[i]];
		CDBG("%s cam_gpio_req_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_req_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,gpio-req-tbl-label", i,
			&gconf->cam_gpio_req_tbl[i].label);
		CDBG("%s cam_gpio_req_tbl[%d].label = %s\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(gconf->cam_gpio_req_tbl);
ERROR1:
	kfree(val_array);
	gconf->cam_gpio_req_tbl_size = 0;
	return rc;
}

static int32_t msm_actuator_get_gpio_data(struct msm_actuator_ctrl_t *a_ctrl,
	struct device_node *of_node)
{
	int32_t rc = 0;
	uint32_t i = 0;
	uint16_t gpio_array_size = 0;
	uint16_t *gpio_array = NULL;

	a_ctrl->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf),
		GFP_KERNEL);
	if (!a_ctrl->gpio_conf) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	gpio_array_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, gpio_array_size);

	if (gpio_array_size) {
		gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size,
			GFP_KERNEL);
		if (!gpio_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto ERROR1;
		}
		for (i = 0; i < gpio_array_size; i++) {
			gpio_array[i] = of_get_gpio(of_node, i);
			CDBG("%s gpio_array[%d] = %d\n", __func__, i,
				gpio_array[i]);
		}

		rc = msm_actuator_get_dt_gpio_req_tbl(of_node,
			a_ctrl->gpio_conf, gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
		kfree(gpio_array);
	}
	return 0;

ERROR2:
	kfree(gpio_array);
ERROR1:
	kfree(a_ctrl->gpio_conf);
	return rc;
}

static int32_t msm_actuator_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_actuator_ctrl_t *act_ctrl_t = NULL;
	CDBG("%s:%d Enter\n", __func__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	if (!client->dev.of_node) {
		act_ctrl_t = (struct msm_actuator_ctrl_t *)(id->driver_data);
	} else {
		act_ctrl_t = kzalloc(sizeof(struct msm_actuator_ctrl_t),
			GFP_KERNEL);
		if (!act_ctrl_t) {
			pr_err("%s:%d no memory\n", __func__, __LINE__);
			return -ENOMEM;
		}
		rc = of_property_read_u32(client->dev.of_node, "cell-index",
			&act_ctrl_t->subdev_id);
		CDBG("cell-index %d, rc %d\n", act_ctrl_t->subdev_id, rc);
		act_ctrl_t->cam_name = act_ctrl_t->subdev_id;
		if (rc < 0) {
			pr_err("failed rc %d\n", rc);
			kfree(act_ctrl_t);//prevent
			return rc;
		}
		rc = msm_actuator_get_gpio_data(act_ctrl_t,
			client->dev.of_node);
	}
	act_ctrl_t->act_v4l2_subdev_ops = &msm_actuator_subdev_ops;
	act_ctrl_t->curr_step_pos = 0;
	act_ctrl_t->curr_region_index = 0;
	act_ctrl_t->actuator_mutex = &msm_actuator_mutex;

	CDBG("client = %x\n", (unsigned int) client);
	act_ctrl_t->i2c_client.client = client;
	/* Set device type as I2C */
	act_ctrl_t->act_device_type = MSM_CAMERA_I2C_DEVICE;
	act_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	act_ctrl_t->act_v4l2_subdev_ops = &msm_actuator_subdev_ops;
	act_ctrl_t->actuator_mutex = &msm_actuator_mutex;
	/* Assign name for sub device */
	snprintf(act_ctrl_t->msm_sd.sd.name, sizeof(act_ctrl_t->msm_sd.sd.name),
		"%s", id->name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&act_ctrl_t->msm_sd.sd,
		act_ctrl_t->i2c_client.client,
		act_ctrl_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&act_ctrl_t->msm_sd.sd, act_ctrl_t);
	act_ctrl_t->msm_sd.sd.internal_ops = &msm_actuator_internal_ops;
	act_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&act_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	act_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	act_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_ACTUATOR;
	act_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&act_ctrl_t->msm_sd);
	CDBG("succeeded\n");
	CDBG("Exit\n");

probe_failure:
	return rc;
}

static int32_t msm_actuator_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_actuator_ctrl_t *msm_actuator_t = NULL;
	CDBG("Enter\n");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_actuator_t = kzalloc(sizeof(struct msm_actuator_ctrl_t),
		GFP_KERNEL);
	if (!msm_actuator_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		kfree(msm_actuator_t);
		return rc;
	}
	msm_actuator_t->subdev_id = pdev->id;
	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_actuator_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_actuator_t->cci_master, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		kfree(msm_actuator_t);
		return rc;
	}

	msm_actuator_t->act_v4l2_subdev_ops = &msm_actuator_subdev_ops;
	msm_actuator_t->actuator_mutex = &msm_actuator_mutex;
	msm_actuator_t->cam_name = pdev->id;

	/* Set platform device handle */
	msm_actuator_t->pdev = pdev;
	/* Set device type as platform device */
	msm_actuator_t->act_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_actuator_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_actuator_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_actuator_t->i2c_client.cci_client) {
		pr_err("failed no memory\n");
		kfree(msm_actuator_t);
		return -ENOMEM;
	}

	cci_client = msm_actuator_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	v4l2_subdev_init(&msm_actuator_t->msm_sd.sd,
		msm_actuator_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_actuator_t->msm_sd.sd, msm_actuator_t);
	msm_actuator_t->msm_sd.sd.internal_ops = &msm_actuator_internal_ops;
	msm_actuator_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_actuator_t->msm_sd.sd.name,
		ARRAY_SIZE(msm_actuator_t->msm_sd.sd.name), "msm_actuator");
	media_entity_init(&msm_actuator_t->msm_sd.sd.entity, 0, NULL, 0);
	msm_actuator_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_actuator_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_ACTUATOR;
	msm_actuator_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_actuator_t->msm_sd);
	CDBG("Exit\n");
	return rc;
}

static const struct i2c_device_id msm_actuator_i2c_id[] = {
	{"qcom,actuator", (kernel_ulong_t)NULL},
	{ }
};

static const struct of_device_id msm_actuator_dt_match[] = {
	{.compatible = "qcom,actuator", .data = NULL},
	{}
};

static struct i2c_driver msm_actuator_i2c_driver = {
	.id_table = msm_actuator_i2c_id,
	.probe  = msm_actuator_i2c_probe,
	.remove = __exit_p(msm_actuator_i2c_remove),
	.driver = {
		.name = "msm_actuator",
		.owner	= THIS_MODULE,
		.of_match_table = msm_actuator_dt_match,
	},
};

MODULE_DEVICE_TABLE(of, msm_actuator_dt_match);

static struct platform_driver msm_actuator_platform_driver = {
	.driver = {
		.name = "qcom,actuator",
		.owner = THIS_MODULE,
		.of_match_table = msm_actuator_dt_match,
	},
};

static int __init msm_actuator_init_module(void)
{
        int32_t rc = 0, i2c_rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_probe(&msm_actuator_platform_driver,
		msm_actuator_platform_probe);
	if (rc < 0)
	    CDBG("%s:%d failed platform dirver probe rc %d\n",
		   __func__, __LINE__, rc);
	else
	    CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	i2c_rc = i2c_add_driver(&msm_actuator_i2c_driver);
	if (i2c_rc < 0)
	    CDBG("%s:%d failed i2c dirver probe rc %d\n",
		   __func__, __LINE__, rc);
	else
            CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);

        if (rc < 0 && i2c_rc < 0)
            pr_err("%s : probe failed\n", __func__);

	return i2c_rc;
}

static struct msm_actuator msm_vcm_actuator_table = {
	.act_type = ACTUATOR_VCM,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_vcm_init_step_table,
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_vcm_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_set_position = msm_actuator_vcm_set_position,
		.actuator_sw_landing = msm_actuator_vcm_sw_landing,
	},
};

static struct msm_actuator msm_piezo_actuator_table = {
	.act_type = ACTUATOR_PIEZO,
	.func_tbl = {
		.actuator_init_step_table = NULL,
		.actuator_move_focus = msm_actuator_piezo_move_focus,
		.actuator_write_focus = NULL,
		.actuator_set_default_focus =
			msm_actuator_piezo_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_sw_landing = NULL,
	},
};
static struct msm_actuator msm_hall_effect_actuator_table = {
	.act_type = ACTUATOR_HALL_EFFECT,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_init_step_table,
		.actuator_move_focus = msm_actuator_hall_effect_move_focus,
		.actuator_write_focus = NULL,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_hall_effect_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_set_position = msm_actuator_set_position,
		.actuator_sw_landing = NULL,
	},
};

static struct msm_actuator msm_hvcm_actuator_table = {
	.act_type = ACTUATOR_HVCM,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_hvcm_init_step_table,
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_hvcm_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_set_position = msm_actuator_hvcm_set_position, /*Added by Justin_Qualcomm for SEMCO Actuator Direct Move : 20130718*/
		.actuator_sw_landing = NULL,
	},
};

static struct msm_actuator msm_dw9804_actuator_table = {
	.act_type = ACTUATOR_DW9804,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_dw9804_init_step_table,
		.actuator_move_focus = msm_actuator_dw9804_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_dw9804_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_set_position = msm_actuator_dw9804_set_position,
		.actuator_sw_landing = msm_actuator_dw9804_sw_landing,
	},
};


module_init(msm_actuator_init_module);
MODULE_DESCRIPTION("MSM ACTUATOR");
MODULE_LICENSE("GPL v2");
