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
#include <linux/delay.h>
#include <video/sprd_isp.h>
#include <asm/cacheflush.h>
#include "isp_block.h"
#include "isp_reg.h"
#include "isp_drv.h"

#define ISP_LSC_TIME_OUT_MAX        500
#define ISP_LSC_BUF0                0
#define ISP_LSC_BUF1                1

static int32_t isp_k_2d_lsc_param_load(struct isp_memory *src_buf,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t time_out_cnt = 0;
	uint32_t addr = 0, reg_value = 0;
	void *buf_ptr = (void *)isp_private->lsc_buf_addr;
	uint32_t buf_len = isp_private->lsc_buf_len;

	if ((0x00 != src_buf->addr) && (0x00 != src_buf->len)
		&& (00 != isp_private->lsc_buf_addr) && (0x00 != isp_private->lsc_buf_len)
		&& (src_buf->len <= isp_private->lsc_buf_len)) {

		ret = copy_from_user((void *)isp_private->lsc_buf_addr, (void *)src_buf->addr, src_buf->len);
		if ( 0 != ret) {
			printk("isp_k_2d_lsc_param_load: copy error, ret=0x%x\n", (uint32_t)ret);
			return -1;
		}

		dmac_flush_range(buf_ptr, buf_ptr + buf_len);
		outer_flush_range(__pa(buf_ptr), __pa(buf_ptr) + buf_len);

		addr = (uint32_t)__pa(isp_private->lsc_buf_addr);

#if defined(CONFIG_MACH_CORE3)
		{
			unsigned long flags;

			local_irq_save(flags);

			reg_value = REG_RD(ISP_LSC_STATUS);
			while((0x00 == (reg_value & ISP_LNC_STATUS_OK))
				&& (time_out_cnt < (ISP_LSC_TIME_OUT_MAX * 1000))) {
				udelay(1);
				reg_value = REG_RD(ISP_LSC_STATUS);
				time_out_cnt++;
			}
			if (time_out_cnt >= (ISP_LSC_TIME_OUT_MAX * 1000)) {
				ret = -1;
				printk("isp_k_2d_lsc_param_load: lsc status time out error.\n");
			}

			if (ISP_LSC_BUF0 = isp_private->lsc_load_buf_id) {
				isp_private->lsc_load_buf_id = ISP_LSC_BUF1;
			} else {
				isp_private->lsc_load_buf_id = ISP_LSC_BUF0;
			}
			REG_MWR(ISP_LSC_LOAD_BUF, BIT_0, isp_private->lsc_load_buf_id);
			REG_WR(ISP_LSC_PARAM_ADDR, addr);
			REG_OWR(ISP_LSC_LOAD_EB, BIT_0);
			isp_private->lsc_update_buf_id = isp_private->lsc_load_buf_id;

			local_irq_restore(flags);
		}
#else
		if (ISP_LSC_BUF0 == isp_private->lsc_load_buf_id) {
			isp_private->lsc_load_buf_id = ISP_LSC_BUF1;
		} else {
			isp_private->lsc_load_buf_id = ISP_LSC_BUF0;
		}
		REG_MWR(ISP_LSC_LOAD_BUF, BIT_0, isp_private->lsc_load_buf_id);
		REG_WR(ISP_LSC_PARAM_ADDR, addr);
		REG_OWR(ISP_LSC_LOAD_EB, BIT_0);
		isp_private->lsc_update_buf_id = isp_private->lsc_load_buf_id;
#endif
		reg_value = REG_RD(ISP_INT_RAW);

		while ((0x00 == (reg_value & ISP_INT_LSC_LOAD))
			&& (time_out_cnt < ISP_LSC_TIME_OUT_MAX)) {
			udelay(1);
			reg_value = REG_RD(ISP_INT_RAW);
			time_out_cnt++;
		}
		if (time_out_cnt >= ISP_LSC_TIME_OUT_MAX) {
			ret = -1;
			printk("isp_k_2d_lsc_param_load: lsc load time out error.\n");
		}
		REG_OWR(ISP_INT_CLEAR, ISP_INT_LSC_LOAD);
	} else {
		printk("isp_k_2d_lsc_param_load: buffer error.\n");
	}

	return ret;
}

static int32_t isp_k_2d_lsc_block(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	struct isp_memory src_buf;
	struct isp_dev_lsc_info lsc_info;

	memset(&lsc_info, 0x00, sizeof(lsc_info));

	ret = copy_from_user((void *)&lsc_info, param->property_param, sizeof(lsc_info));
	if (0 != ret) {
		printk("isp_k_2d_lsc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (lsc_info.bypass) {
		REG_OWR(ISP_LSC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_LSC_PARAM, BIT_0, 0);
	}

	REG_MWR(ISP_LSC_GRID_PITCH, 0x1FF, lsc_info.grid_pitch);

	REG_MWR(ISP_LSC_GRID_PITCH, (BIT_17 | BIT_16), (lsc_info.grid_mode << 16));

	REG_MWR(ISP_LSC_MISC, (BIT_1 | BIT_0), lsc_info.endian);

	src_buf.addr = lsc_info.buf_addr;
	src_buf.len = lsc_info.buf_len;
	ret = isp_k_2d_lsc_param_load(&src_buf, isp_private);

	return ret;
}

static int32_t isp_k_lsc_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_lsc_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_LSC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_LSC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_2d_lsc_param_update(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	REG_MWR(ISP_LSC_PARAM, BIT_1, (isp_private->lsc_update_buf_id << 1));

	return ret;
}

static int32_t isp_k_2d_lsc_pos(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_offset offset = {0, 0};

	ret = copy_from_user((void *)&offset, param->property_param, sizeof(offset));
	if (0 != ret) {
		printk("isp_k_lens_pos: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((offset.y & 0x7F) << 8) | (offset.x & 0x7F);

	REG_WR(ISP_LSC_SLICE_POS, val);

	return ret;
}

static int32_t isp_k_2d_lsc_grid_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_2d_lsc_grid_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_LSC_GRID_SIZE, val);

	return ret;
}

static int32_t isp_k_2d_lsc_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_2d_lsc_slice_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);

	REG_WR(ISP_LSC_SLICE_SIZE, val);

	return ret;
}

int32_t isp_k_cfg_2d_lsc(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_2d_lsc: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_2d_lsc: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_2D_LSC_BLOCK:
		ret = isp_k_2d_lsc_block(param, isp_private);
		break;
	case ISP_PRO_2D_LSC_BYPASS:
		ret = isp_k_lsc_bypass(param);
		break;
	case ISP_PRO_2D_LSC_PARAM_UPDATE:
		ret = isp_k_2d_lsc_param_update(param, isp_private);
		break;
	case ISP_PRO_2D_LSC_POS:
		ret = isp_k_2d_lsc_pos(param);
		break;
	case ISP_PRO_2D_LSC_GRID_SIZE:
		ret = isp_k_2d_lsc_grid_size(param);
		break;
	case ISP_PRO_2D_LSC_SLICE_SIZE:
		ret = isp_k_2d_lsc_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_2d_lsc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
