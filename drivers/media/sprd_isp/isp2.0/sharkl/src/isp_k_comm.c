/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/uaccess.h>
#include <linux/sprd_mm.h>
#include <video/sprd_isp.h>
#include "isp_reg.h"

static int32_t isp_k_comm_start(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t start = 0;

	ret = copy_from_user((void *)&start, param->property_param, sizeof(start));
	if (0 != ret) {
		printk("isp_k_comm_start: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (start) {
		REG_OWR(ISP_COMMON_START, BIT_0);
	} else {
		REG_MWR(ISP_COMMON_START, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_comm_in_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t in_mode = 0;


	ret = copy_from_user((void *)&in_mode, param->property_param, sizeof(in_mode));
	if (0 != ret) {
		printk("isp_k_comm_in_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PARAM, BIT_0|BIT_1, in_mode);

	return ret;
}

static int32_t isp_k_comm_out_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t out_mode = 0;


	ret = copy_from_user((void *)&out_mode, param->property_param, sizeof(out_mode));
	if (0 != ret) {
		printk("isp_k_comm_out_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PARAM, BIT_11 | BIT_10, out_mode << 10);

	return ret;
}

static int32_t isp_k_comm_fetch_endian(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_fetch_endian fetch_endian = {0, 0};


	ret = copy_from_user((void *)&fetch_endian, param->property_param, sizeof(fetch_endian));
	if (0 != ret) {
		printk("isp_k_comm_fetch_endian: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PARAM, BIT_4 | BIT_3 | BIT_2, ((fetch_endian.endian & 0x03) << 2) | ((fetch_endian.bit_recorder & 0x01) << 4));

	return ret;
}

static int32_t isp_k_comm_bpc_endian(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t endian = 0;


	ret = copy_from_user((void *)&endian, param->property_param, sizeof(endian));
	if (0 != ret) {
		printk("isp_k_comm_bpc_endian: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PARAM, BIT_7 | BIT_6, endian << 6);

	return ret;
}

static int32_t isp_k_comm_store_endian(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t endian = 0;


	ret = copy_from_user((void *)&endian, param->property_param, sizeof(endian));
	if (0 != ret) {
		printk("isp_k_comm_store_endian: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PARAM, BIT_9 | BIT_8, endian << 8);

	return ret;
}

static int32_t isp_k_comm_fetch_data_format(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t format = 0;


	ret = copy_from_user((void *)&format, param->property_param, sizeof(format));
	if (0 != ret) {
		printk("isp_k_comm_fetch_data_format: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PARAM, BIT_21 | BIT_20 | BIT_19 | BIT_18, format << 18);

	return ret;
}

static int32_t isp_k_comm_store_format(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t format = 0;


	ret = copy_from_user((void *)&format, param->property_param, sizeof(format));
	if (0 != ret) {
		printk("isp_k_comm_store_format: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PARAM, BIT_17 | BIT_16 | BIT_15 | BIT_14, format << 14);

	return ret;
}

static int32_t isp_k_comm_burst_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint16_t size = 0;


	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_comm_burst_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_BURST_SIZE, BIT_2 | BIT_1 | BIT_0, size);

	return ret;
}

static int32_t isp_k_comm_mem_switch(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t mem_switch = 0;


	ret = copy_from_user((void *)&mem_switch, param->property_param, sizeof(mem_switch));
	if (0 != ret) {
		printk("isp_k_comm_mem_switch: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_MEM_SWITCH, BIT_0, mem_switch);

	return ret;
}

static int32_t isp_k_comm_shadow(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t shadow = 0;


	ret = copy_from_user((void *)&shadow, param->property_param, sizeof(shadow));
	if (0 != ret) {
		printk("isp_k_comm_shadow: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SHADOW, BIT_0, shadow);

	return ret;
}

static int32_t isp_k_comm_shadow_all(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t shadow = 0;


	ret = copy_from_user((void *)&shadow, param->property_param, sizeof(shadow));
	if (0 != ret) {
		printk("isp_k_comm_shadow_all: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_COMMON_SHADOW_ALL, 0x55555555);

	return ret;
}

static int32_t isp_k_comm_bayer_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t value = 0;
	struct isp_bayer_mode mode = {0, 0, 0, 0, 0};


	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_comm_bayer_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	value = mode.gain_bayer << 8 | mode.cfa_bayer << 6 | mode.wave_bayer << 4 | mode.awbc_bayer << 2 | mode.nlc_bayer;

	REG_MWR(ISP_COMMON_BAYER_MODE, 0x3FF, value);

	return ret;
}

static int32_t isp_k_comm_int_clear(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t int_num = 0;


	ret = copy_from_user((void *)&int_num, param->property_param, sizeof(int_num));
	if (0 != ret) {
		printk("isp_k_comm_int_clear: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_CLR_INTERRUPT, 1<< int_num, 1<< int_num);
	return ret;
}

static int32_t isp_k_comm_get_int_raw(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t int_raw = 0;

	int_raw = REG_RD(ISP_COMMON_RAW_INTERRUPT);

	ret = copy_to_user(param->property_param, (void*)&int_raw, sizeof(int_raw));
	if (0 != ret) {
		ret = -EFAULT;
		printk("isp_k_comm_get_int_raw: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_comm_pmu_raw_mask(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t raw_mask = 0;


	ret = copy_from_user((void *)&raw_mask, param->property_param, sizeof(raw_mask));
	if (0 != ret) {
		printk("isp_k_comm_pmu_raw_mask: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PMU_RAM_MASK, BIT_0, raw_mask);
	return ret;
}

static int32_t isp_k_comm_hw_mask(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t hw_logic = 0;


	ret = copy_from_user((void *)&hw_logic, param->property_param, sizeof(hw_logic));
	if (0 != ret) {
		printk("isp_k_comm_hw_mask: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_HW_MASK, 1<< hw_logic, 1<< hw_logic);
	return ret;
}

static int32_t isp_k_comm_hw_enable(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t hw_logic = 0;


	ret = copy_from_user((void *)&hw_logic, param->property_param, sizeof(hw_logic));
	if (0 != ret) {
		printk("isp_k_comm_hw_enable: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_HW_ENABLE, 1<< hw_logic, 1<< hw_logic);
	return ret;
}

static int32_t isp_k_comm_pmu_pmu_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t sel = 0;


	ret = copy_from_user((void *)&sel, param->property_param, sizeof(sel));
	if (0 != ret) {
		printk("isp_k_comm_pmu_pmu_sel: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SW_SWITCH, BIT_0, sel);
	return ret;
}

static int32_t isp_k_comm_sw_enable(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t sw_logic = 0;


	ret = copy_from_user((void *)&sw_logic, param->property_param, sizeof(sw_logic));
	if (0 != ret) {
		printk("isp_k_comm_sw_enable: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_HW_ENABLE, 1<< sw_logic, 1<< sw_logic);

	return ret;
}

static int32_t isp_k_comm_preview_stop(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t eb = 0;


	ret = copy_from_user((void *)&eb, param->property_param, sizeof(eb));
	if (0 != ret) {
		printk("isp_k_comm_preview_stop: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_PREVIEW_STOP, BIT_0, eb);

	return ret;
}

static int32_t isp_k_comm_set_shadow_control(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t control = 0;


	ret = copy_from_user((void *)&control, param->property_param, sizeof(control));
	if (0 != ret) {
		printk("isp_k_comm_set_shadow_control: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SHADOW_CNT, BIT_0, control);

	return ret;
}

static int32_t isp_k_comm_shadow_control_clear(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t clear = 0;

	ret = copy_from_user((void *)&clear, param->property_param, sizeof(clear));
	if (0 != ret) {
		printk("isp_k_comm_shadow_control_clear: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SHADOW_CNT, BIT_1, clear << 1);

	return ret;
}

static int32_t isp_k_comm_axi_stop(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t eb = 0;


	ret = copy_from_user((void *)&eb, param->property_param, sizeof(eb));
	if (0 != ret) {
		printk("isp_k_comm_axi_stop: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_HW_MASK, BIT_0, eb);

	return ret;
}

static int32_t isp_k_comm_slice_cnt_enable(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t eb = 0;


	ret = copy_from_user((void *)&eb, param->property_param, sizeof(eb));
	if (0 != ret) {
		printk("isp_k_comm_slice_cnt_enable: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SLICE_CNT, BIT_0, eb);

	return ret;
}

static int32_t isp_k_comm_preform_cnt_enable(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t eb = 0;


	ret = copy_from_user((void *)&eb, param->property_param, sizeof(eb));
	if (0 != ret) {
		printk("isp_k_comm_preform_cnt_enable: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SLICE_CNT, BIT_1, (eb << 1));

	return ret;
}

static int32_t isp_k_comm_set_slice_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint8_t num = 0;


	ret = copy_from_user((void *)&num, param->property_param, sizeof(num));
	if (0 != ret) {
		printk("isp_k_comm_set_slice_num: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SLICE_CNT, 0x1F0, (num << 4));

	return ret;
}

static int32_t isp_k_comm_get_slice_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;
	uint8_t slice_num = 0;

	status = REG_RD(ISP_COMMON_SLICE_CNT);

	slice_num = (status & 0x1F0) >> 4;

	ret = copy_to_user(param->property_param, (void*)&slice_num, sizeof(slice_num));
	if (0 != ret) {
		ret = -EFAULT;
		printk("isp_k_comm_get_slice_num: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_comm_perform_cnt_rstatus(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;

	status = REG_RD(ISP_COMMON_PERFORM_CNT_R);

	ret = copy_to_user(param->property_param, (void*)&status, sizeof(status));
	if (0 != ret) {
		ret = -EFAULT;
		printk("isp_k_comm_perform_cnt_rstatus: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_comm_preform_cnt_status(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;

	status = REG_RD(ISP_COMMON_PERFORM_CNT);

	ret = copy_to_user(param->property_param, (void*)&status, sizeof(status));
	if (0 != ret) {
		ret = -EFAULT;
		printk("isp_k_comm_preform_cnt_status: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

int32_t isp_k_cfg_comm(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_comm: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_comm: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_COMMON_START:
		ret = isp_k_comm_start(param);
		break;
	case ISP_PRO_COMMON_IN_MODE:
		ret = isp_k_comm_in_mode(param);
		break;
	case ISP_PRO_COMMON_OUT_MODE:
		ret = isp_k_comm_out_mode(param);
		break;
	case ISP_PRO_COMMON_FETCH_ENDIAN:
		ret = isp_k_comm_fetch_endian(param);
		break;
	case ISP_PRO_COMMON_BPC_ENDIAN:
		ret = isp_k_comm_bpc_endian(param);;
		break;
	case ISP_PRO_COMMON_STORE_ENDIAN:
		ret = isp_k_comm_store_endian(param);
		break;
	case ISP_PRO_COMMON_FETCH_DATA_FORMAT:
		ret = isp_k_comm_fetch_data_format(param);
		break;
	case ISP_PRO_COMMON_STORE_FORMAT:
		ret = isp_k_comm_store_format(param);
		break;
	case ISP_PRO_COMMON_BURST_SIZE:
		ret = isp_k_comm_burst_size(param);
		break;
	case ISP_PRO_COMMON_MEM_SWITCH:
		ret = isp_k_comm_mem_switch(param);
		break;
	case ISP_PRO_COMMON_SHADOW:
		ret = isp_k_comm_shadow(param);
		break;
	case ISP_PRO_COMMON_SHADOW_ALL:
		ret = isp_k_comm_shadow_all(param);
		break;
	case ISP_PRO_COMMON_BAYER_MODE:
		ret = isp_k_comm_bayer_mode(param);
		break;
	case ISP_PRO_COMMON_INT_CLEAR:
		ret = isp_k_comm_int_clear(param);
		break;
	case ISP_PRO_COMMON_GET_INT_RAW:
		ret = isp_k_comm_get_int_raw(param);
		break;
	case ISP_PRO_COMMON_PMU_RAW_MASK:
		ret = isp_k_comm_pmu_raw_mask(param);
		break;
	case ISP_PRO_COMMON_HW_MASK:
		ret = isp_k_comm_hw_mask(param);
		break;
	case ISP_PRO_COMMON_HW_ENABLE:
		ret = isp_k_comm_hw_enable(param);
		break;
	case ISP_PRO_COMMON_PMU_SEL:
		ret = isp_k_comm_pmu_pmu_sel(param);
		break;
	case ISP_PRO_COMMON_SW_ENABLE:
		ret = isp_k_comm_sw_enable(param);
		break;
	case ISP_PRO_COMMON_PREVIEW_STOP:
		ret = isp_k_comm_preview_stop(param);
		break;
	case ISP_PRO_COMMON_SET_SHADOW_CONTROL:
		ret = isp_k_comm_set_shadow_control(param);
		break;
	case ISP_PRO_COMMON_SHADOW_CONTROL_CLEAR:
		ret = isp_k_comm_shadow_control_clear(param);
		break;
	case ISP_PRO_COMMON_AXI_STOP:
		ret = isp_k_comm_axi_stop(param);
		break;
	case ISP_PRO_COMMON_SLICE_CNT_ENABLE:
		ret = isp_k_comm_slice_cnt_enable(param);
		break;
	case ISP_PRO_COMMON_PREFORM_CNT_ENABLE:
		ret = isp_k_comm_preform_cnt_enable(param);
		break;
	case ISP_PRO_COMMON_SET_SLICE_NUM:
		ret = isp_k_comm_set_slice_num(param);
		break;
	case ISP_PRO_COMMON_GET_SLICE_NUM:
		ret = isp_k_comm_get_slice_num(param);
		break;
	case ISP_PRO_COMMON_PERFORM_CNT_RSTATUS:
		ret = isp_k_comm_perform_cnt_rstatus(param);
		break;
	case ISP_PRO_COMMON_PERFORM_CNT_STATUS:
		ret = isp_k_comm_preform_cnt_status(param);
		break;
	default:
		printk("isp_k_cfg_comm: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
