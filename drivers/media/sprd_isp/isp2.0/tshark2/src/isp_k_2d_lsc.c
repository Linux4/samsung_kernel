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
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include "isp_reg.h"
#include "isp_drv.h"


#define ISP_LSC_TIME_OUT_MAX        500
#define ISP_LSC_BUF0                0
#define ISP_LSC_BUF1                1

#if 0
#define CLIP(input,top, bottom) {if (input>top) input = top; if (input < bottom) input = bottom;}
#define TABLE_LEN_128	128
#define TABLE_LEN_96	96

typedef struct lnc_bicubic_weight_t_64_tag
{
	int16_t w0;
	int16_t w1;
	int16_t w2;
	int16_t w3;
}LNC_BICUBIC_WEIGHT_TABLE_T;
////////10bit precision//////////////////////////////////////
static LNC_BICUBIC_WEIGHT_TABLE_T lnc_bicubic_weight_t_96_simple[] =
{
	{0,	1024,	0},
	{-5,1024,	6},
	{-10,1023,	12},
	{-15,1022,	18},
	{-20,1020,	25},
	{-24,1017,	32},
	{-28,1014,	40},
	{-32,1011,	48},
	{-36,1007,	56},
	{-39,1003,	65},
	{-43,998,	74},
	{-46,993,	83},
	{-49,987,	93},
	{-52,981,	103},
	{-54,974,	113},
	{-57,967,	124},
	{-59,960,	135},
	{-61,952,	146},
	{-63,944,	158},
	{-65,936,	170},
	{-67,927,	182},
	{-68,918,	194},
	{-70,908,	206},
	{-71,898,	219},
	{-72,888,	232},
	{-73,878,	245},
	{-74,867,	258},
	{-74,856,	272},
	{-75,844,	285},
	{-75,833,	299},
	{-76,821,	313},
	{-76,809,	327},
	{-76,796,	341},
	{-76,784,	356},
	{-76,771,	370},
	{-75,758,	384},
	{-75,745,	399},
	{-75,732,	414},
	{-74,718,	428},
	{-73,704,	443},
	{-73,691,	458},
	{-72,677,	473},
	{-71,663,	487},
	{-70,648,	502},
	{-69,634,	517},
	{-68,620,	532},
	{-67,605,	547},
	{-65,591,	561},
	{-64,576,	576},
};

static LNC_BICUBIC_WEIGHT_TABLE_T lnc_bicubic_weight_t_128_simple[] =
{
	 {0,	1024,	0},
	 {-4,	1024,	4},
	 {-8,	1023,	8},
	 {-11,	1023,	13},
	 {-15,	1022,	18},
	 {-18,	1020,	23},
	 {-22,	1019,	28},
	 {-25,	1017,	34},
	 {-28,	1014,	40},
	 {-31,	1012,	46},
	 {-34,	1009,	52},
	 {-37,	1006,	58},
	 {-39,	1003,	65},
	 {-42,	999,	72},
	 {-44,	995,	78},
	 {-47,	991,	86},
	 {-49,	987,	93},
	 {-51,	982,	101},
	 {-53,	978,	108},
	 {-55,	973,	116},
	 {-57,	967,	124},
	 {-59,	962,	132},
	 {-60,	956,	141},
	 {-62,	950,	149},
	 {-63,	944,	158},
	 {-65,	938,	167},
	 {-66,	931,	176},
	 {-67,	925,	185},
	 {-68,	918,	194},
	 {-69,	910,	203},
	 {-70,	903,	213},
	 {-71,	896,	222},
	 {-72,	888,	232},
	 {-73,	880,	242},
	 {-73,	872,	252},
	 {-74,	864,	262},
	 {-74,	856,	272},
	 {-75,	847,	282},
	 {-75,	839,	292},
	 {-75,	830,	303},
	 {-76,	821,	313},
	 {-76,	812,	324},
	 {-76,	803,	334},
	{-76,	793,	345},
	{-76,	784,	356},
	{-76,	774,	366},
	{-76,	765,	377},
	{-75,	755,	388},
	{-75,	745,	399},
	{-75,	735,	410},
	{-74,	725,	421},
	{-74,	715,	432},
	{-73,	704,	443},
	{-73,	694,	454},
	{-72,	684,	465},
	{-72,	673,	476},
	{-71,	663,	487},
	{-70,	652,	498},
	{-69,	641,	510},
	{-69,	631,	521},
	{-68,	620,	532},
	{-67,	609,	543},
	{-66,	598,	554},
	{-65,	587,	565},
	{-64,	576,	576},
};

static uint16_t ISP_Cubic1D(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint16_t u, uint16_t box)
{
	int32_t out_value;
	uint16_t out_value_uint16_t;
	int16_t w0, w1, w2, w3;
	uint32_t out_value_tmp;
	int16_t sub_tmp0;
	int16_t sub_tmp1;
	int16_t sub_tmp2;

	if(box ==96)
	{
		//use simple table
		if ( u < (TABLE_LEN_96/2 + 1) )
		{
			w0 = lnc_bicubic_weight_t_96_simple[u].w0 ;
			w1 = lnc_bicubic_weight_t_96_simple[u].w1 ;
			w2 = lnc_bicubic_weight_t_96_simple[u].w2 ;

			sub_tmp0 = a-d;
			sub_tmp1 = b-d;
			sub_tmp2 = c-d;
			out_value_tmp = ((uint32_t)d)<<10;
			out_value = out_value_tmp + sub_tmp0 * w0  + sub_tmp1 * w1 + sub_tmp2 * w2;
		}
		else
		{
			w1 = lnc_bicubic_weight_t_96_simple[TABLE_LEN_96 - u].w2 ;
			w2 = lnc_bicubic_weight_t_96_simple[TABLE_LEN_96 - u].w1 ;
			w3 = lnc_bicubic_weight_t_96_simple[TABLE_LEN_96 - u].w0 ;

			sub_tmp0 = b-a;
			sub_tmp1 = c-a;
			sub_tmp2 = d-a;
			out_value_tmp = ((uint32_t)a)<<10;
			out_value = out_value_tmp + sub_tmp0 * w1  + sub_tmp1 * w2 + sub_tmp2 * w3;
		}

	}
	else
	{
		u = u * (TABLE_LEN_128 / box);

		//use simple table
		if ( u < (TABLE_LEN_128/2 + 1) )
		{
			w0 = lnc_bicubic_weight_t_128_simple[u].w0 ;
			w1 = lnc_bicubic_weight_t_128_simple[u].w1 ;
			w2 = lnc_bicubic_weight_t_128_simple[u].w2 ;

			sub_tmp0 = a-d;
			sub_tmp1 = b-d;
			sub_tmp2 = c-d;
			out_value_tmp = ((uint32_t)d)<<10;
			out_value = out_value_tmp + sub_tmp0 * w0  + sub_tmp1 * w1 + sub_tmp2 * w2;
		}
		else
		{
			w1 = lnc_bicubic_weight_t_128_simple[TABLE_LEN_128 - u].w2 ;
			w2 = lnc_bicubic_weight_t_128_simple[TABLE_LEN_128 - u].w1 ;
			w3 = lnc_bicubic_weight_t_128_simple[TABLE_LEN_128 - u].w0 ;

			sub_tmp0 = b-a;
			sub_tmp1 = c-a;
			sub_tmp2 = d-a;
			out_value_tmp = ((uint32_t)a)<<10;
			out_value = out_value_tmp + sub_tmp0 * w1  + sub_tmp1 * w2 + sub_tmp2 * w3;
		}
	}

	//CLIP(out_value, 4095*1024*2, 1024*1024);	// for LSC gain, 1024 = 1.0, 4095 = 4.0 ; 4095*2 is for boundary extension.
	CLIP(out_value, 16383*1024, 1024*1024);	// for LSC gain, 1024 = 1.0, 16383 = 16.0 ; 16383 is for boundary extension, 14 bit parameter is used.

	out_value_uint16_t = (uint16_t)((out_value + 512) >> 10);

	return out_value_uint16_t;
}


static int32_t ISP_GenerateQValues(uint32_t word_endian, uint32_t q_val[][5], unsigned long param_address, uint16_t grid_w, uint16_t grid_num, uint16_t u)
{
	uint8_t i;
	uint16_t A0, B0, C0, D0;
	uint16_t A1, B1, C1, D1;
	uint32_t *addr = (uint32_t *)param_address;

	if (param_address == 0x0 || grid_num == 0x0) {
		printk("ISP_GenerateQValues param_address error addr=0x%lx grid_num=%d \n",param_address, grid_num);
		return -1;
	}

	//printk("ISP_GenerateQValues:addr=%x  grid_w=%d  grid_num=%d u=%d \n",param_address,grid_w,grid_num,u);
	#if 0
	for (i = 0; i < 5; i++) {
		A0  = (uint16_t)*(addr + i * 2) & 0xFFFF;
		B0  = (uint16_t)*(addr + i * 2 + grid_w * 2) & 0xFFFF;
		C0  = (uint16_t)*(addr + i * 2 + grid_w * 2 * 2) & 0xFFFF;
		D0  = (uint16_t)*(addr + i * 2 + grid_w * 2 * 3) & 0xFFFF;
		A1  = (uint16_t)(*(addr + i * 2) >> 16);
		B1  = (uint16_t)(*(addr + i * 2 + grid_w * 2) >> 16);
		C1  = (uint16_t)(*(addr + i * 2 + grid_w * 2 * 2) >> 16);
		D1  = (uint16_t)(*(addr + i * 2 + grid_w * 2 * 3) >> 16);
		q_val[0][i] = ISP_Cubic1D(A0, B0, C0, D0, u, grid_num);
		q_val[1][i] = ISP_Cubic1D(A1, B1, C1, D1, u, grid_num);
	}
	#endif
	#if 1
	for (i = 0; i < 5; i++) {
		if (1 == word_endian) {	// ABCD = 1 word
			A0  = *(addr + i * 2) >> 16;					// AB
			B0  = *(addr + i * 2 + grid_w * 2) >> 16;       // AB
			C0  = *(addr + i * 2 + grid_w * 2 * 2) >> 16;   // AB
			D0  = *(addr + i * 2 + grid_w * 2 * 3) >> 16;   // AB
			A1  = *(addr + i * 2) & 0xFFFF;					// CD
			B1  = *(addr + i * 2 + grid_w * 2) & 0xFFFF;    // CD
			C1  = *(addr + i * 2 + grid_w * 2 * 2) & 0xFFFF;// CD
			D1  = *(addr + i * 2 + grid_w * 2 * 3) & 0xFFFF;// CD
		} else if (2 == word_endian) { // CDAB = 1 word
			A0  = *(addr + i * 2) & 0xFFFF;
			B0  = *(addr + i * 2 + grid_w * 2) & 0xFFFF;
			C0  = *(addr + i * 2 + grid_w * 2 * 2) & 0xFFFF;
			D0  = *(addr + i * 2 + grid_w * 2 * 3) & 0xFFFF;
			A1  = *(addr + i * 2) >> 16;
			B1  = *(addr + i * 2 + grid_w * 2) >> 16;
			C1  = *(addr + i * 2 + grid_w * 2 * 2) >> 16;
			D1  = *(addr + i * 2 + grid_w * 2 * 3) >> 16;
		} else if (0 == word_endian) { // DCBA = 1 word
			A0  = ((*(addr + i * 2) << 8) & 0x0000FF00) | ((*(addr + i * 2) >> 8) & 0x000000FF);
			B0  = ((*(addr + i * 2 + grid_w * 2) << 8) & 0x0000FF00) | ((*(addr + i * 2 + grid_w * 2) >> 8) & 0x000000FF);
			C0  = ((*(addr + i * 2 + grid_w * 2 * 2) << 8) & 0x0000FF00) | ((*(addr + i * 2 + grid_w * 2 * 2) >> 8) & 0x000000FF);
			D0  = ((*(addr + i * 2 + grid_w * 2 * 3) << 8) & 0x0000FF00) | ((*(addr + i * 2 + grid_w * 2 * 3) >> 8) & 0x000000FF);
			A1  = ((*(addr + i * 2) << 8) & 0xFF000000) | ((*(addr + i * 2) >> 8) & 0x00FF0000);
			B1  = ((*(addr + i * 2 + grid_w * 2) << 8) & 0xFF000000) | ((*(addr + i * 2 + grid_w * 2) >> 8) & 0x00FF0000);
			C1  = ((*(addr + i * 2 + grid_w * 2 * 2) << 8) & 0xFF000000) | ((*(addr + i * 2 + grid_w * 2 * 2) >> 8) & 0x00FF0000);
			D1  = ((*(addr + i * 2 + grid_w * 2 * 3) << 8) & 0xFF000000) | ((*(addr + i * 2 + grid_w * 2 * 3) >> 8) & 0x00FF0000);
		} else if (3 == word_endian) { // BADC = 1 word
			A0  = ((*(addr + i * 2) << 8) & 0xFF000000) | ((*(addr + i * 2) >> 8) & 0x00FF0000);
			B0  = ((*(addr + i * 2 + grid_w * 2) << 8) & 0xFF000000) | ((*(addr + i * 2 + grid_w * 2) >> 8) & 0x00FF0000);
			C0  = ((*(addr + i * 2 + grid_w * 2 * 2) << 8) & 0xFF000000) | ((*(addr + i * 2 + grid_w * 2 * 2) >> 8) & 0x00FF0000);
			D0  = ((*(addr + i * 2 + grid_w * 2 * 3) << 8) & 0xFF000000) | ((*(addr + i * 2 + grid_w * 2 * 3) >> 8) & 0x00FF0000);
			A1  = ((*(addr + i * 2) << 8) & 0x0000FF00) | ((*(addr + i * 2) >> 8) & 0x000000FF);
			B1  = ((*(addr + i * 2 + grid_w * 2) << 8) & 0x0000FF00) | ((*(addr + i * 2 + grid_w * 2) >> 8) & 0x000000FF);
			C1  = ((*(addr + i * 2 + grid_w * 2 * 2) << 8) & 0x0000FF00) | ((*(addr + i * 2 + grid_w * 2 * 2) >> 8) & 0x000000FF);
			D1  = ((*(addr + i * 2 + grid_w * 2 * 3) << 8) & 0x0000FF00) | ((*(addr + i * 2 + grid_w * 2 * 3) >> 8) & 0x000000FF);
		}
		//printk("ISP_GenerateQValues: A0=%x B0=%x C0=%x D0=%x A1=%x B1=%x C1=%x D1=%x \n",A0,B0,C0,D0,A1,B1,C1,D1);
		q_val[0][i] = ISP_Cubic1D(A0, B0, C0, D0, u, grid_num);
		q_val[1][i] = ISP_Cubic1D(A1, B1, C1, D1, u, grid_num);
		//printk("ISP_GenerateQValues: q_val[0][%d] = %d  q_val[1][%d] = %d \n",i,q_val[0][i],i ,q_val[1][i] );
	}
	#endif

	return 0;
}
#endif

static int32_t isp_k_2d_lsc_param_load(struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t time_out_cnt = 0;
	uint32_t reg_value = 0;

	if (ISP_LSC_BUF0 == isp_private->lsc_load_buf_id) {
		isp_private->lsc_load_buf_id = ISP_LSC_BUF1;
	} else {
		isp_private->lsc_load_buf_id = ISP_LSC_BUF0;
	}
	REG_MWR(ISP_LENS_LOAD_BUF, BIT_0, isp_private->lsc_load_buf_id);
	REG_WR(ISP_LENS_PARAM_ADDR, isp_private->lsc_buf_phys_addr);
	REG_OWR(ISP_LENS_LOAD_EB, BIT_0);
	REG_MWR(ISP_LENS_PARAM, BIT_1, isp_private->lsc_load_buf_id << 1);
	isp_private->lsc_update_buf_id = isp_private->lsc_load_buf_id;

	reg_value = REG_RD(ISP_INT_RAW0);

	while ((0x00 == (reg_value & ISP_INT_EVT_LSC_LOAD))
		&& (time_out_cnt < ISP_LSC_TIME_OUT_MAX)) {
		udelay(1);
		reg_value = REG_RD(ISP_INT_RAW0);
		time_out_cnt++;
	}
	if (time_out_cnt >= ISP_LSC_TIME_OUT_MAX) {
		ret = -1;
		printk("isp_k_2d_lsc_param_load: lsc load time out error.\n");
	}
	REG_OWR(ISP_INT_CLR0, ISP_INT_EVT_LSC_LOAD);

	return ret;
}

static int32_t isp_k_2d_lsc_block(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	//uint32_t qvalue_addr = ISP_LEN_BUF0_CH0;
	struct isp_dev_2d_lsc_info lens_info;

	memset(&lens_info, 0x00, sizeof(lens_info));

	ret = copy_from_user((void *)&lens_info, param->property_param, sizeof(lens_info));
	if (0 != ret) {
		printk("isp_k_2d_lsc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((lens_info.offset_y & 0xFFFF) << 16) | (lens_info.offset_x & 0xFFFF);
	REG_WR(ISP_LENS_SLICE_POS, val);

	val = lens_info.grid_pitch & 0x1FF;
	REG_MWR(ISP_LENS_GRID_PITCH, 0x1FF, val);

	val = (lens_info.grid_width & 0xFF) << 16;
	REG_MWR(ISP_LENS_GRID_PITCH, 0xFF0000, val);

	val = ((lens_info.grid_num_t & 0xFFFF) << 16) | ((lens_info.grid_y_num & 0xFF) << 8) | (lens_info.grid_x_num & 0xFF);
	REG_WR(ISP_LENS_GRID_SIZE, val);

	REG_MWR(ISP_LENS_LOAD_BUF, BIT_1, lens_info.load_chn_sel << 1);

	REG_MWR(ISP_LENS_MISC, (BIT_1 | BIT_0), (lens_info.endian & 0x03));

	val = ((lens_info.slice_size.height & 0xFFFF) << 16) | (lens_info.slice_size.width & 0xFFFF);
	REG_WR(ISP_LENS_SLICE_SIZE, val);

	val = ((lens_info.relative_y & 0xFF) << 8) | (lens_info.relative_x & 0xFF);
	REG_WR(ISP_LENS_INIT_COOR, val);

	/*ret = isp_k_2d_lsc_param_load(&src_buf, isp_private, &qvalue_addr);
	REG_WR(ISP_LENS_PARAM_ADDR, qvalue_addr);
	printk("isp_k_2d_lsc_block:qvalue_addr=%x grid_x_num=%d grid_width=%d  relative_y=%d \n",qvalue_addr,(lens_info.grid_x_num & 0xFF),(lens_info.grid_width & 0xFF), (lens_info.relative_y & 0xFF)>>1);

	ISP_GenerateQValues(1, lens_info.q_value, qvalue_addr, ((uint16_t)lens_info.grid_x_num & 0xFF) + 2,(lens_info.grid_width & 0xFF), (lens_info.relative_y & 0xFF)>>1);
	*/

	ret = isp_k_2d_lsc_param_load(isp_private);
	if (0 != ret) {
		printk("isp_k_2d_lsc_block: lsc load para error, ret=0x%x\n", (uint32_t)ret);
		REG_MWR(ISP_LENS_PARAM, BIT_0, ISP_BYPASS_EB);
		return -1;
	}

	for (i = 0; i < 5; i++) {
		val = ((lens_info.q_value[0][i] & 0x3FFF) << 16) | (lens_info.q_value[1][i] & 0x3FFF);
		REG_WR(ISP_LENS_Q0_VALUE + i * 4, val);
	}

	REG_MWR(ISP_LENS_PARAM, BIT_0, lens_info.bypass);

	return ret;

}

static int32_t isp_k_2d_lsc_param_update(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	REG_MWR(ISP_LENS_PARAM, BIT_1, isp_private->lsc_update_buf_id << 1);

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

	val = ((offset.y & 0xFFFF) << 16) | (offset.x & 0xFFFF);

	REG_WR(ISP_LENS_SLICE_POS, val);

	return ret;
}

static int32_t isp_k_2d_lsc_grid_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t grid_total = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_2d_lsc_grid_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	grid_total = (size.height + 2) * (size.width + 2);
	val = ((grid_total & 0xFFFF) << 16) | ((size.height & 0xFF) << 8) | (size.width & 0xFF);
	REG_WR(ISP_LENS_GRID_SIZE, val);

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

	REG_WR(ISP_LENS_SLICE_SIZE, val);

	return ret;
}

static int32_t isp_k_2d_lsc_transaddr(struct isp_io_param *param,
			struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	struct isp_lsc_addr lsc_addr;

	ret = copy_from_user((void *)&lsc_addr, param->property_param, sizeof(struct isp_lsc_addr));

	isp_private->lsc_buf_phys_addr = lsc_addr.phys_addr;

	printk("lsc phys addr 0x%x,\n",isp_private->lsc_buf_phys_addr);
	return ret;
}

int32_t isp_k_cfg_2d_lsc(struct isp_io_param *param, struct isp_k_private *isp_private)
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
	case ISP_PRO_2D_LSC_TRANSADDR:
		ret = isp_k_2d_lsc_transaddr(param, isp_private);
		break;
	default:
		printk("isp_k_cfg_2d_lsc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
