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
#include "sin_cos.h"
#include "gen_scale_coef.h"
#include <linux/mm.h>
#include <linux/math64.h>

#define GSC_FIX	   24
#define GSC_COUNT 256

#define GSC_ABS(_a) 		((_a) < 0 ? -(_a) : (_a))
#define GSC_SIGN2(input, p) 	{if (p>=0) input = 1; if (p < 0) input = -1;}
#define COEF_ARR_ROWS          	9
#define COEF_ARR_COLUMNS      	8
#define COEF_ARR_COL_MAX	16
#define MIN_POOL_SIZE            	(6 * 1024)
#define TRUE			1
#define FALSE			0
#define SCI_MEMSET  		memset
#define  MAX( _x, _y ) 		( ((_x) > (_y)) ? (_x) : (_y) )

typedef struct {
	uint32_t begin_addr;
	uint32_t total_size;
	uint32_t used_size;
} GSC_MEM_POOL;


static uint8_t _InitPool(void *buffer_ptr,
			 uint32_t buffer_size, GSC_MEM_POOL * pool_ptr)
{
	if (NULL == buffer_ptr || 0 == buffer_size || NULL == pool_ptr) {
		return FALSE;
	}
	if (buffer_size < MIN_POOL_SIZE) {
		return FALSE;
	}
	pool_ptr->begin_addr = (uint32_t) buffer_ptr;
	pool_ptr->total_size = buffer_size;
	pool_ptr->used_size = 0;
	return TRUE;
}

static void *_Allocate(uint32_t size,
		       uint32_t align_shift, GSC_MEM_POOL * pool_ptr)
{
	uint32_t begin_addr = 0;
	uint32_t temp_addr = 0;
	if (NULL == pool_ptr) {
		return NULL;
	}
	begin_addr = pool_ptr->begin_addr;
	temp_addr = begin_addr + pool_ptr->used_size;
	temp_addr =
	    ((temp_addr + (1 << align_shift) -
	      1) >> align_shift) << align_shift;
	if (temp_addr + size > begin_addr + pool_ptr->total_size) {
		return NULL;
	}
	pool_ptr->used_size = (temp_addr + size) - begin_addr;
	SCI_MEMSET((void *)temp_addr, 0, size);
	return (void *)temp_addr;
}

static int64_t div64_s64_s64(int64_t dividend, int64_t divisor)
{
	int8_t sign = 1;
	int64_t dividend_tmp = dividend;
	int64_t divisor_tmp = divisor;
	int64_t ret = 0;
	if (0 == divisor) {
		return 0;
	}
	if ((dividend >> 63) & 0x1) {
		sign *= -1;
		dividend_tmp = dividend * (-1);
	}
	if ((divisor >> 63) & 0x1) {
		sign *= -1;
		divisor_tmp = divisor * (-1);
	}
	ret = div64_s64(dividend_tmp, divisor_tmp);
	ret *= sign;
	return ret;
}

static void normalize_inter(int64_t * data, int16_t * int_data, uint8_t ilen)
{
	uint8_t it;
	int64_t tmp_d = 0;
	int64_t *tmp_data = NULL;
	int64_t tmp_sum_val = 0;
	tmp_data = data;
	tmp_sum_val = 0;
	for (it = 0; it < ilen; it++) {
		tmp_sum_val += tmp_data[it];
	}
	if (0 == tmp_sum_val) {
		uint8_t value = 256 / ilen;
		for (it = 0; it < ilen; it++) {
			tmp_d = value;
			int_data[it] = (int16_t) tmp_d;
		}
	} else {
		for (it = 0; it < ilen; it++) {
			tmp_d =
			    div64_s64_s64(tmp_data[it] * (int64_t) 256,
					  tmp_sum_val);
			int_data[it] = (uint16_t) tmp_d;
		}
	}
}

static int16_t sum_fun(int16_t * data, int8_t ilen)
{
	int8_t i;
	int16_t tmp_sum;
	tmp_sum = 0;

	for (i = 0; i < ilen; i++)
		tmp_sum += *data++;
	return tmp_sum;
}

static void adjust_filter_inter(int16_t * filter, uint8_t ilen)
{
	int32_t i, midi;
	int32_t tmpi, tmp_S;
	int32_t tmp_val = 0;

	tmpi = sum_fun(filter, ilen) - 256;
	midi = ilen >> 1;
	GSC_SIGN2(tmp_val, tmpi);

	if ((tmpi & 1) == 1) {
		filter[midi] = filter[midi] - tmp_val;
		tmpi -= tmp_val;
	}
	tmp_S = GSC_ABS(tmpi / 2);
	if ((ilen & 1) == 1) {
		for (i = 0; i < tmp_S; i++) {
			filter[midi - (i + 1)] =
			    filter[midi - (i + 1)] - tmp_val;
			filter[midi + (i + 1)] =
			    filter[midi + (i + 1)] - tmp_val;
		}
	} else {
		for (i = 0; i < tmp_S; i++) {
			filter[midi - (i + 1)] =
			    filter[midi - (i + 1)] - tmp_val;
			filter[midi + i] = filter[midi + i] - tmp_val;
		}
	}
	if (filter[midi] > 255) {
		tmp_val = filter[midi];
		filter[midi] = 255;
		filter[midi - 1] = filter[midi - 1] + tmp_val - 255;
	}
}

static int16_t CalYmodelCoef(int16_t coef_lenght,
			     int16_t * coef_data_ptr,
			     int16_t N, int16_t M, GSC_MEM_POOL * pool_ptr)
{
	int8_t mount;
	int16_t i, mid_i, kk, j, sum_val;
	int64_t *filter = _Allocate(GSC_COUNT * sizeof(int64_t), 3, pool_ptr);
	int64_t *tmp_filter =
	    _Allocate(GSC_COUNT * sizeof(int64_t), 3, pool_ptr);
	int16_t *normal_filter =
	    _Allocate(GSC_COUNT * sizeof(int16_t), 2, pool_ptr);

	if (NULL == filter || NULL == tmp_filter || NULL == normal_filter) {
		return 1;
	}
	mid_i = coef_lenght >> 1;
	filter[mid_i] =
	    div64_s64_s64((int64_t) ((int64_t) N << GSC_FIX),
			  (int64_t) MAX(M, N));
	for (i = 0; i < mid_i; i++) {
		int64_t angle_x =
		    div64_s64_s64((int64_t) ARC_32_COEF * (int64_t) (i + 1) *
				  (int64_t) N, (int64_t) MAX(M,
							     N) * (int64_t) 8);
		int64_t angle_y =
		    div64_s64_s64((int64_t) ARC_32_COEF * (int64_t) (i + 1) *
				  (int64_t) N, (int64_t) (M * N) * (int64_t) 8);
		int32_t value_x = dcam_sin_32((int32_t) angle_x);
		int32_t value_y = dcam_sin_32((int32_t) angle_y);
		filter[mid_i + i + 1] =
		    div64_s64_s64((int64_t)
				  ((int64_t) value_x *
				   (int64_t) (1 << GSC_FIX)),
				  (int64_t) ((int64_t) M * (int64_t) value_y));
		filter[mid_i - (i + 1)] = filter[mid_i + i + 1];
	}
	for (i = -1; i < mid_i; i++) {
		int32_t angle_32 =
		    (int32_t)
		    div64_s64_s64((int64_t)
				  ((int64_t) 2 * (int64_t) (mid_i - i - 1) *
				   (int64_t) ARC_32_COEF),
				  (int64_t) coef_lenght);
		int64_t a = (int64_t) 9059697;
		int64_t b = (int64_t) 7717519;
		int64_t t = a - ((b * dcam_cos_32(angle_32)) >> 30);
		filter[mid_i + i + 1] = (t * filter[mid_i + i + 1]) >> GSC_FIX;
		filter[mid_i - (i + 1)] = filter[mid_i + i + 1];
	}
	for (i = 0; i < 8; i++) {
		mount = 0;
		for (j = i; j < coef_lenght; j += 8) {
			tmp_filter[mount] = filter[j];
			mount++;
		}
		normalize_inter(tmp_filter, normal_filter, (int8_t) mount);
		sum_val = sum_fun(normal_filter, mount);
		if (256 != sum_val) {
			adjust_filter_inter(normal_filter, mount);
		}
		mount = 0;
		for (kk = i; kk < coef_lenght; kk += 8) {
			coef_data_ptr[kk] = normal_filter[mount];
			mount++;
		}
	}
	return 0;
}

static int16_t CalY_ScalingCoef(int16_t tap,
				int16_t D,
				int16_t I,
				int16_t * y_coef_data_ptr,
				int16_t dir, GSC_MEM_POOL * pool_ptr)
{
	uint16_t coef_lenght;

	coef_lenght = (uint16_t) (tap * 8);
	SCI_MEMSET(y_coef_data_ptr, 0, coef_lenght * sizeof(int16_t));
	CalYmodelCoef(coef_lenght, y_coef_data_ptr, I, D, pool_ptr);
	return coef_lenght;
}

static int16_t CalUV_ScalingCoef(int16_t tap,
				 int16_t D,
				 int16_t I,
				 int16_t * uv_coef_data_ptr,
				 int16_t dir, GSC_MEM_POOL * pool_ptr)
{
	int16_t uv_coef_lenght;

	if ((dir == 1)) {
		uv_coef_lenght = (int16_t) (tap * 8);
		CalYmodelCoef(uv_coef_lenght, uv_coef_data_ptr, I, D, pool_ptr);
	} else {
		if (D > I) {
			uv_coef_lenght = (int16_t) (tap * 8);
		} else {
            		uv_coef_lenght = (int16_t) (2 * 8);
		}
		CalYmodelCoef(uv_coef_lenght, uv_coef_data_ptr, I, D, pool_ptr);
	}
	return uv_coef_lenght;
}

static void GetFilter(int16_t * coef_data_ptr,
		      int16_t * out_filter,
		      int16_t iI_hor, int16_t coef_len, int16_t * filter_len)
{
	int16_t i, pos_start;

	pos_start = coef_len / 2;
	while (pos_start >= iI_hor) {
		pos_start -= iI_hor;
	}
	for (i = 0; i < iI_hor; i++) {
		int16_t len = 0;
		int16_t j;
		int16_t pos = pos_start + i;
		while (pos >= iI_hor) {
			pos -= iI_hor;
		}
		for (j = 0; j < coef_len; j += iI_hor) {
			*out_filter++ = coef_data_ptr[j + pos];
			len++;
		}
		*filter_len++ = len;
	}
}

static void WriteScalarCoef(int16_t * dst_coef_ptr,
			    int16_t * coef_ptr, int16_t dst_pitch, int16_t src_pitch)
{
	int i, j;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < src_pitch; j++) {
			*(dst_coef_ptr + j) =
			    *(coef_ptr + i * src_pitch + src_pitch - 1 - j);
		}
		dst_coef_ptr += dst_pitch;
	}
}

static void SetHorRegisterCoef(uint32_t * reg_coef_ptr, int16_t * y_coef_ptr,
			       int16_t * uv_coef_ptr)
{
	int32_t i = 0;
	int16_t *y_coef_arr[COEF_ARR_ROWS] = { NULL };
	int16_t *uv_coef_arr[COEF_ARR_ROWS] = { NULL };

	for (i = 0; i < COEF_ARR_ROWS; i++) {
		y_coef_arr[i] = y_coef_ptr;
		uv_coef_arr[i] = uv_coef_ptr;
		y_coef_ptr += COEF_ARR_COLUMNS;
		uv_coef_ptr += COEF_ARR_COLUMNS;
	}

	// horizontal Y Scaling Coef Config register
	for (i = 0; i < 8; i++) {
		uint16_t p0, p1;
		uint32_t reg;

		p0 = (uint16_t) y_coef_arr[i][7];
		p1 = (uint16_t) y_coef_arr[i][6];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
		p0 = (uint16_t) y_coef_arr[i][5];
		p1 = (uint16_t) y_coef_arr[i][4];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
		p0 = (uint16_t) y_coef_arr[i][3];
		p1 = (uint16_t) y_coef_arr[i][2];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
		p0 = (uint16_t) y_coef_arr[i][1];
		p1 = (uint16_t) y_coef_arr[i][0];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
	}
	// horizontal UV Scaling Coef Config register
	for (i = 0; i < 8; i++) {
		uint16_t p0, p1;
		uint32_t reg;

		p0 = (uint16_t) uv_coef_arr[i][3];
		p1 = (uint16_t) uv_coef_arr[i][2];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
		p0 = (uint16_t) uv_coef_arr[i][1];
		p1 = (uint16_t) uv_coef_arr[i][0];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
	}
}

static void SetVerRegisterCoef(uint32_t * reg_coef_lum_ptr,
				uint32_t *reg_coef_ch_ptr,
				int16_t * y_coef_ptr,
			       	int16_t * uv_coef_ptr, 
				int16_t  i_h,//decimition at horizontal
				int16_t o_h,//interpolation at horizontal
			       	uint8_t is_scaling2yuv420)
{
	uint32_t cnts = 0;
	uint32_t i = 0, j = 0;
		
	if(2 * o_h <= i_h)
	{
		for(i = 0; i < 9; i++)
		{
			for(j = 0; j < 16; j++)
			{
				reg_coef_lum_ptr[cnts++] = *(y_coef_ptr +i * 16 + j);
				if(SCALER_COEF_TAB_LEN_VER == cnts)
				{
					break;
				}
			}
		}
		
		cnts = 0;
		for(i = 0; i < 9; i++)
		{
			for(j = 0; j < 16; j++)
			{
				reg_coef_ch_ptr[cnts++] = *(uv_coef_ptr + i * 16 +j);	
				if(SCALER_COEF_TAB_LEN_VER == cnts)
				{
					break;
				}			
			}

		}
	}
	else
	{
		for(i = 0; i < 8; i++)
		{
			for(j = 0; j < 4; j++)
			{
				reg_coef_lum_ptr[cnts++] = *(y_coef_ptr +i * 16 + j);

			}
		}
		
		cnts = 0;
		if((o_h <= i_h) && is_scaling2yuv420)
		{
			for(i = 0; i < 9; i++)
			{
				for(j = 0; j < 16; j++)
				{
					reg_coef_ch_ptr[cnts++] = *(uv_coef_ptr +i * 16 + j);
					if(SCALER_COEF_TAB_LEN_VER == cnts)
					{
						break;
					}				
				}
	
			}
		}
		else 
		{
			for(i = 0; i < 8; i++)
			{
				for(j = 0; j < 4; j++)
				{
					reg_coef_ch_ptr[cnts++] = *(uv_coef_ptr +i * 16 + j);
				}
			}
		}
	}
}

static void CheckCoefRange(int16_t * coef_ptr, int16_t rows, int16_t columns, int16_t pitch)
{
	int16_t i, j;
	int16_t value, diff, sign;
	int16_t *coef_arr[COEF_ARR_ROWS] = { NULL };

	for (i = 0; i < COEF_ARR_ROWS; i++) {
		coef_arr[i] = coef_ptr;
		coef_ptr += pitch;
	}
	for (i = 0; i < rows; i++) {
		for (j = 0; j < columns; j++) {
			value = coef_arr[i][j];
			if (value > 255) {
				diff = value - 255;
				coef_arr[i][j] = 255;
				sign = GSC_ABS(diff);
				if ((sign & 1) == 1) {	// ilen is odd
					coef_arr[i][j + 1] =
					    coef_arr[i][j + 1] + (diff + 1) / 2;
					coef_arr[i][j - 1] =
					    coef_arr[i][j - 1] + (diff - 1) / 2;
				} else {	// ilen is even
					coef_arr[i][j + 1] =
					    coef_arr[i][j + 1] + (diff) / 2;
					coef_arr[i][j - 1] =
					    coef_arr[i][j - 1] + (diff) / 2;
				}
			}
		}
	}
}

static void CalcVerEdgeCoef(int16_t * coeff_ptr, int16_t D, int16_t I,
			    int16_t tap, int16_t pitch)
{
	int32_t phase_temp[9];
	int32_t acc = 0;
	int32_t i_sample_cnt = 0;
	uint8_t phase = 0;
	uint8_t spec_tap = 0;
	int32_t l;
	int16_t i, j;
	int16_t *coeff_arr[COEF_ARR_ROWS];

	for (i = 0; i < COEF_ARR_ROWS; i++) {
		coeff_arr[i] = coeff_ptr;
		coeff_ptr += pitch;
	}
	for (j = 0; j <= 8; j++)
		phase_temp[j] = j * I / 8;
	
	for (i = 0; i < I; i++) {
		spec_tap = i & 1;
		while (acc >= I) {
			acc -= I;
			i_sample_cnt++;
		}

		for (j = 0; j < 8; j++) {
			if (acc >= phase_temp[j] && acc < phase_temp[j + 1]) {
				phase = (uint8_t) j;
				break;
			}
		}

		{
			int32_t j;
			for (j = (1 - tap / 2); j <= (tap / 2); j++) {
				l = i_sample_cnt + j;
				if (l <= 0) {
					coeff_arr[8][spec_tap] +=
					    coeff_arr[phase][j + tap / 2 - 1];
				} else {
					if (l >= D - 1) {
						coeff_arr[8][spec_tap + 2] +=
						    coeff_arr[phase][j +
								     tap / 2 -
								     1];
					}
				}
			}
		}
		acc += D;
	}
}

/**---------------------------------------------------------------------------*
 **                         Public Functions                                  *
 **---------------------------------------------------------------------------*/
/****************************************************************************/
/* Purpose:	generate scale factor           							    */
/* Author:																	*/
/* Input:                                                                   */
/*          i_w:	source image width                                      */
/*          i_h:	source image height                                  	*/
/*          o_w:    target image width                                      */
/*          o_h:    target image height                						*/
/* Output:	    															*/
/*          coeff_h_ptr: pointer of horizontal coefficient buffer, the size of which must be at  */
/*					   least SCALER_COEF_TAP_NUM_HOR * 4 bytes				*/
/*					  the output coefficient will be located in coeff_h_ptr[0], ......,   */
/*						coeff_h_ptr[SCALER_COEF_TAP_NUM_HOR-1]				*/
/*			coeff_v_ptr: pointer of vertical coefficient buffer, the size of which must be at      */
/*					   least (SCALER_COEF_TAP_NUM_VER + 1) * 4 bytes	    */
/*					  the output coefficient will be located in coeff_v_ptr[0], ......,   */
/*					  coeff_h_ptr[SCALER_COEF_TAP_NUM_VER-1] and the tap number */
/*					  will be located in coeff_h_ptr[SCALER_COEF_TAP_NUM_VER] */
/*          temp_buf_ptr: temp buffer used while generate the coefficient   */
/*          temp_buf_ptr: temp buffer size, 6k is the suggest size         */
/* Return:					                    							*/
/* Note:                                                                    */
/****************************************************************************/
uint8_t Dcam_GenScaleCoeff(int16_t i_w, 
			int16_t i_h, 
			int16_t o_w, 
			int16_t o_h,
		      	uint32_t *coeff_h_ptr, 
		      	uint32_t *coeff_v_lum_ptr,
		      	uint32_t *coeff_v_ch_ptr, 
			uint8_t scaling2yuv420,
			uint8_t *scaler_tap,
			uint8_t *chrome_tap,
		      	void *temp_buf_ptr, 
		      	uint32_t temp_buf_size
		      	)
{
	int16_t D_hor = i_w;	//decimition at horizontal
	int16_t D_ver = i_h;	//decimition at vertical
	int16_t I_hor = o_w;	//interpolation at horizontal
	int16_t I_ver = o_h;	//interpolation at vertical
	int16_t I_ver_bak_uv = o_h;
	int16_t I_ver_bak_y = o_h;
	int16_t *cong_Ycom_hor = 0;
	int16_t *cong_UVcom_hor = 0;
	int16_t *cong_Ycom_ver = 0;
	int16_t *cong_UVcom_ver = 0;	
	
	uint16_t luma_ver_tap, chrome_ver_tap;
	uint16_t luma_ver_maxtap = 16, chrome_ver_maxtap = 16;
	
	uint32_t coef_buf_size = 0;
	int16_t *temp_filter_ptr = NULL;
	int16_t *filter_ptr = NULL;
	uint32_t filter_buf_size = GSC_COUNT * sizeof(int16_t);
	int16_t filter_len[COEF_ARR_ROWS] = { 0 };
	int16_t coef_len = 0;
	GSC_MEM_POOL pool = { 0 };

	if (0 == i_w || 0 == i_h || 0 == o_w || (0 == o_h) \
		|| (NULL == coeff_h_ptr) || (NULL == coeff_v_lum_ptr) \
		|| (NULL == coeff_v_ch_ptr) || (NULL == scaler_tap) \
		|| (NULL == chrome_tap) || (NULL == temp_buf_ptr)) {

		printk("GenScaleCoeff: i_w: %d, i_h: %d, o_w:%d, o_h: %d, coef_h:0x%x, coef_v_lum:0x%x, \
			coef_v_chr: 0x%x, y_tap: 0x%x, ch_tap: 0x%x, tmp_buf:0x%x",
			i_w, i_h, o_w, o_h, (uint32_t)coeff_h_ptr, (uint32_t)coeff_v_lum_ptr, \
			(uint32_t)coeff_v_ch_ptr, (uint32_t)scaler_tap, (uint32_t)chrome_tap, (uint32_t)temp_buf_ptr);

		return FALSE;
	}

	/* init pool and allocate static array */
	if (!_InitPool(temp_buf_ptr, temp_buf_size, &pool)) {
		return FALSE;
	}
	
	coef_buf_size = COEF_ARR_ROWS * COEF_ARR_COL_MAX * sizeof(int16_t);	
	cong_Ycom_hor = (int16_t*)_Allocate(coef_buf_size, 2, &pool);
	cong_UVcom_hor = (int16_t*)_Allocate(coef_buf_size, 2, &pool);
	cong_Ycom_ver = (int16_t*)_Allocate(coef_buf_size, 2, &pool);
	cong_UVcom_ver =  (int16_t*)_Allocate(coef_buf_size, 2, &pool);	
	
	if (NULL == cong_Ycom_hor 
	|| NULL == cong_UVcom_hor 
	||(NULL == cong_Ycom_ver)
	||(NULL == cong_UVcom_ver)) {
		return FALSE;
	}
	
	temp_filter_ptr = _Allocate(filter_buf_size, 2, &pool);
	filter_ptr = _Allocate(filter_buf_size, 2, &pool);
	if (NULL == temp_filter_ptr || NULL == filter_ptr) {
		return FALSE;
	}
	
	/* calculate coefficients of Y component in horizontal direction */
	coef_len = CalY_ScalingCoef(8, D_hor, I_hor, temp_filter_ptr, 1, &pool);
	GetFilter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
	WriteScalarCoef(cong_Ycom_hor, filter_ptr, 8, 8);
	CheckCoefRange(cong_Ycom_hor, 8, 8, 8);
	
	/* calculate coefficients of UV component in horizontal direction */
	coef_len = CalUV_ScalingCoef(4, D_hor, I_hor, temp_filter_ptr, 1, &pool);
	GetFilter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
	WriteScalarCoef(cong_UVcom_hor, filter_ptr, 8, 4);
	CheckCoefRange(cong_UVcom_hor, 8, 4, 8);	
	/* write the coefficient to register format */
	SetHorRegisterCoef(coeff_h_ptr, cong_Ycom_hor, cong_UVcom_hor);
	
	luma_ver_tap = ((uint8_t)(D_ver / I_ver)) * 2;
	chrome_ver_tap = luma_ver_tap;
	
	if ( luma_ver_tap > luma_ver_maxtap)
		luma_ver_tap = luma_ver_maxtap;//modified by Hongbo, max_tap 8-->16			
	if(luma_ver_tap <= 2) 
		luma_ver_tap = 4;
	
	*scaler_tap = (uint8_t)luma_ver_tap;
	
	//------------------------------------------------------//
	//////////////////////////////////////////////////////////	
	/* calculate coefficients of Y component in vertical direction*/
    	coef_len = CalY_ScalingCoef(luma_ver_tap, D_ver, I_ver, temp_filter_ptr, 0, &pool);	
	GetFilter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
	WriteScalarCoef(cong_Ycom_ver, filter_ptr, 16, filter_len[0]);
	CheckCoefRange(cong_Ycom_ver, 8, luma_ver_tap, 16);
	/* calculate coefficients of UV component in vertical direction */
	if (scaling2yuv420) {
		I_ver_bak_uv /= 2;
		chrome_ver_tap *= 2;
		chrome_ver_maxtap = 16;		
	}
	
	if ( chrome_ver_tap > chrome_ver_maxtap)
		chrome_ver_tap = chrome_ver_maxtap;//modified by Hongbo, max_tap 8-->16			
	if(chrome_ver_tap <= 2) 
		chrome_ver_tap = 4;	
	*chrome_tap = (uint8_t)chrome_ver_tap;	
	
	coef_len = CalUV_ScalingCoef((int16_t) (chrome_ver_tap), 
				     D_ver, I_ver_bak_uv, temp_filter_ptr, 
				     0, &pool);	
	GetFilter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
	WriteScalarCoef(cong_UVcom_ver, filter_ptr, 16, filter_len[0]);
	CheckCoefRange(cong_UVcom_ver, 8, chrome_ver_tap, 16);
	
	/* calculate edge coefficients of Y component in vertical direction */
	if(2 * I_ver_bak_y <= D_ver) { 	//only scale down
		CalcVerEdgeCoef(cong_Ycom_ver, D_ver, I_ver_bak_y, luma_ver_tap, 16);
	}
	/* calculate edge coefficients of UV component in vertical direction */
	if(2 * I_ver_bak_uv <= D_ver) {	//only scale down
		CalcVerEdgeCoef(cong_UVcom_ver, D_ver, I_ver_bak_uv, chrome_ver_tap, 16);
	}
	/* write the coefficient to register format */
	SetVerRegisterCoef(coeff_v_lum_ptr, coeff_v_ch_ptr, 
			cong_Ycom_ver, cong_UVcom_ver, 
			D_ver, I_ver, scaling2yuv420);
	

	return TRUE;
}
