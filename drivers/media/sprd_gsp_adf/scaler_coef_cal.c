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

#include <linux/math64.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mm.h>
#include "scaler_coef_cal.h"
#include "gsp_config_if.h"
#include "sin_cos.h"


#define GSC_FIX     24
#define GSC_COUNT   64
#define TRUE        1
#define FALSE       0

#define GSC_ABS(_a)             ((_a) < 0 ? -(_a) : (_a))
#define GSC_SIGN2(input, p)     {if (p>=0) input = 1; if (p < 0) input = -1;}
#define COEF_ARR_ROWS           9
#define COEF_ARR_COLUMNS        8
#define COEF_ARR_COL_MAX        16
#define MIN_POOL_SIZE           (6 * 1024)

#define SCI_MEMSET              memset
#define MAX( _x, _y )           (((_x) > (_y)) ? (_x) : (_y) )

#define LIST_SET_ENTRY_KEY(entry, i_w, i_h,\
		o_w, o_h, h_t, v_t)\
{\
    entry->in_w = i_w;\
    entry->in_h = i_h;\
    entry->out_w = o_w;\
    entry->out_h = o_h;\
    entry->hor_tap = h_t;\
    entry->ver_tap = v_t;\
}

struct gsc_mem_pool{
    unsigned long begin_addr;
    unsigned long total_size;
    unsigned long used_size;
};


/*we use "Least Recently Used(LRU)" to implement the coef-matrix cache policy*/






/*
func:cache_coef_hit_check
desc:find the entry have the same in_w in_h out_w out_h
return:if hit,return the entry pointer; else return null;
*/
static struct coef_entry* cache_coef_hit_check(uint16_t in_w, uint16_t in_h,
		uint16_t out_w, uint16_t out_h, uint16_t hor_tap, uint16_t ver_tap,
		struct list_head *coef_list)
{
    static uint32_t total_cnt = 0;
    static uint32_t hit_cnt = 0;

    struct coef_entry* walk = NULL;

    total_cnt++;
	list_for_each_entry(walk, coef_list, entry_list) {
        if(walk->in_w == in_w
           && walk->in_h == in_h
           && walk->out_w == out_w
           && walk->out_h == out_h
           && walk->hor_tap == hor_tap
           && walk->ver_tap == ver_tap) {
            hit_cnt++;
            GSP_DEBUG("GSP_CACHE_COEF:hit, hit_ratio:%d percent\n",hit_cnt*100/total_cnt);
            return walk;
        }
    }
    GSP_DEBUG("GSP_CACHE_COEF:miss\n");
    return NULL;
}

void cache_coef_move_entry_to_list_head(struct coef_entry* entry,
		struct list_head *coef_list)
{
    list_del_init(&entry->entry_list);
    list_add(&entry->entry_list, coef_list);
}

static uint8_t init_pool(void *buffer_ptr,
                         uint32_t buffer_size,
                         struct gsc_mem_pool * pool_ptr)
{
    if (NULL == buffer_ptr || 0 == buffer_size || NULL == pool_ptr) {
        return FALSE;
    }
    if (buffer_size < MIN_POOL_SIZE) {
        return FALSE;
    }
    pool_ptr->begin_addr = (unsigned long) buffer_ptr;
    pool_ptr->total_size = buffer_size;
    pool_ptr->used_size = 0;
    //printk("GSPinit_pool:%d,begin_addr:0x%08x,total_size:%d,used_size:%d\n",__LINE__,pool_ptr->begin_addr,pool_ptr->total_size,pool_ptr->used_size);
    return TRUE;
}

static void *allocate(uint32_t size,
                       uint32_t align_shift,
                       struct gsc_mem_pool * pool_ptr)
{
    unsigned long begin_addr = 0;
    unsigned long temp_addr = 0;
    if (NULL == pool_ptr) {
        //printk("GSPallocate:%d allocate error! \n",__LINE__);
        return NULL;
    }
    begin_addr = pool_ptr->begin_addr;
    temp_addr = begin_addr + pool_ptr->used_size;
    temp_addr = (((temp_addr + (1UL << align_shift)-1) >> align_shift) << align_shift);
    if (temp_addr + size > begin_addr + pool_ptr->total_size) {
        //printk("GSPallocate err:%d,temp_addr:0x%08x,size:%d,begin_addr:0x%08x,total_size:%d,used_size:%d\n",__LINE__,temp_addr,size,begin_addr,pool_ptr->total_size,pool_ptr->used_size);
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
/* ------------------------------------------  */
static int16_t sum_fun(int16_t *data, int8_t ilen)
{
    int8_t  i       ;
    int16_t tmp_sum;
    tmp_sum = 0;

    for (i = 0; i < ilen; i++)
        tmp_sum += *data++;

    return tmp_sum;
}



static void adjust_filter_inter(int16_t *filter, uint8_t ilen)
{
    int32_t i, midi;
    int32_t     tmpi, tmp_S ;
    int32_t     tmp_val = 0 ;

    tmpi = sum_fun(filter, ilen) - 256;
    midi = ilen >> 1;
    GSC_SIGN2(tmp_val, tmpi);

    if ((tmpi & 1) == 1) { // tmpi is odd
        filter[midi] = filter[midi] - tmp_val;
        tmpi -= tmp_val;
    }

    //tmp_S = abs(tmpi>>1);

    tmp_S = GSC_ABS(tmpi / 2);

    if ((ilen & 1) == 1) { /*ilen is odd*/
        for (i = 0; i < tmp_S; i++) {
            filter[midi - (i+1)] = filter[midi - (i+1)] - tmp_val;
            filter[midi + (i+1)] = filter[midi + (i+1)] - tmp_val;
        }
    } else { /*ilen is even*/
        for (i = 0; i < tmp_S; i++) {
            filter[midi - (i+1)] = filter[midi - (i+1)] - tmp_val;
            filter[midi + i] = filter[midi + i] - tmp_val;
        }
    }

    if(filter[midi] > 255) {
        tmp_val = filter[midi] ;
        filter[midi] = 255 ;
        filter[midi - 1] = filter[midi - 1] + tmp_val - 255 ;
    }
}

static int16_t cal_ymodel_coef(int16_t coef_lenght,
                             int16_t * coef_data_ptr,
                             int16_t N,
                             int16_t M,
                             struct gsc_mem_pool * pool_ptr)
{
    int8_t mount;
    int16_t i, mid_i, kk, j, sum_val;
    int64_t *filter = allocate(GSC_COUNT * sizeof(int64_t), 3, pool_ptr);
    int64_t *tmp_filter = allocate(GSC_COUNT * sizeof(int64_t), 3, pool_ptr);
    int16_t *normal_filter = allocate(GSC_COUNT * sizeof(int16_t), 2, pool_ptr);

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
        int32_t value_x = sin_32((int32_t) angle_x);
        int32_t value_y = sin_32((int32_t) angle_y);
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
        int64_t t = a - ((b * cos_32(angle_32)) >> 30);
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

/* cal Y model */
static int16_t caly_scaling_coef(int16_t tap,
                                int16_t D,
                                int16_t I,
                                int16_t * y_coef_data_ptr,
                                int16_t dir, struct gsc_mem_pool * pool_ptr)
{
    uint16_t coef_lenght;

    coef_lenght = (uint16_t) (tap * 8);
    SCI_MEMSET(y_coef_data_ptr, 0, coef_lenght * sizeof(int16_t));
    cal_ymodel_coef(coef_lenght, y_coef_data_ptr, I, D, pool_ptr);
    return coef_lenght;
}

static int16_t caluv_scaling_coef(int16_t tap,
                                 int16_t D,
                                 int16_t I,
                                 int16_t * uv_coef_data_ptr,
                                 int16_t dir, struct gsc_mem_pool * pool_ptr)
{
    int16_t uv_coef_lenght;

    if ((dir == 1)) {
        uv_coef_lenght = (int16_t) (tap * 8);
        cal_ymodel_coef(uv_coef_lenght, uv_coef_data_ptr, I, D, pool_ptr);
    } else {
        if (D > I) {
            uv_coef_lenght = (int16_t) (tap * 8);
        } else {
            uv_coef_lenght = (int16_t) (2 * 8);
        }
        cal_ymodel_coef(uv_coef_lenght, uv_coef_data_ptr, I, D, pool_ptr);
    }
    return uv_coef_lenght;
}

static void get_filter(int16_t * coef_data_ptr,
                      int16_t * out_filter,
                      int16_t iI_hor,
                      int16_t coef_len,
                      int16_t * filter_len)
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
        for (j = 0; j < coef_len; j+=iI_hor) {
            *out_filter++ = coef_data_ptr[j + pos];
            len ++;
        }
        *filter_len++ = len;
    }
}

static void write_scalar_coef(int16_t * dst_coef_ptr,
                            int16_t * coef_ptr,
                            int16_t dst_pitch,
                            int16_t src_pitch)
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

static void check_coef_range(int16_t * coef_ptr, int16_t rows, int16_t columns, int16_t pitch)
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
                if ((sign & 1) == 1) {  // ilen is odd
                    coef_arr[i][j + 1] =
                        coef_arr[i][j + 1] + (diff + 1) / 2;
                    coef_arr[i][j - 1] =
                        coef_arr[i][j - 1] + (diff - 1) / 2;
                } else {    // ilen is even
                    coef_arr[i][j + 1] =
                        coef_arr[i][j + 1] + (diff) / 2;
                    coef_arr[i][j - 1] =
                        coef_arr[i][j - 1] + (diff) / 2;
                }
            }
        }
    }
}


static void gsp_rearrang_coeff(void* src, void*dst, int32_t tap)
{
    uint32_t i, j;
    int16_t *src_ptr, *dst_ptr;

    src_ptr = (int16_t*)src;
    dst_ptr = (int16_t*)dst;
    if (src_ptr == NULL || dst_ptr == NULL)
        return;

    if (0 != dst_ptr) {
        memset((void*)dst_ptr, 0x00, 8 * 8 * sizeof(int16_t));
    }

    switch(tap) {
        case 6:
        case 2: {
            for(i = 0; i<8; i++) {
                for(j = 0; j< tap; j++) {
                    *(dst_ptr+i*8+1+j) = *(src_ptr+i*8+j);
                }
            }
        }
        break;

        case 4:
        case 8: {
            for(i = 0; i<8; i++) {
                for(j = 0; j< tap; j++) {
                    *(dst_ptr + i * 8 + j) = *(src_ptr + i * 8 + j);
                }
            }
        }
        break;
    }
}



/**---------------------------------------------------------------------------*
 **                         Public Functions                                  *
 **---------------------------------------------------------------------------*/
/****************************************************************************/
/* Purpose: generate scale factor                                           */
/* Author:                                                                  */
/* Input:                                                                   */
/*          i_w:    source image width                                      */
/*          i_h:    source image height                                     */
/*          o_w:    target image width                                      */
/*          o_h:    target image height                                     */
/* Output:                                                                  */
/*          coeff_h_ptr: pointer of horizontal coefficient buffer, the size of which must be at  */
/*                     least SCALER_COEF_TAP_NUM_HOR * 4 bytes              */
/*                    the output coefficient will be located in coeff_h_ptr[0], ......,   */
/*                      coeff_h_ptr[SCALER_COEF_TAP_NUM_HOR-1]              */
/*          coeff_v_ptr: pointer of vertical coefficient buffer, the size of which must be at      */
/*                     least (SCALER_COEF_TAP_NUM_VER + 1) * 4 bytes        */
/*                    the output coefficient will be located in coeff_v_ptr[0], ......,   */
/*                    coeff_h_ptr[SCALER_COEF_TAP_NUM_VER-1] and the tap number */
/*                    will be located in coeff_h_ptr[SCALER_COEF_TAP_NUM_VER] */
/*          temp_buf_ptr: temp buffer used while generate the coefficient   */
/*          temp_buf_ptr: temp buffer size, 6k is the suggest size         */
/* Return:                                                                  */
/* Note:                                                                    */
/****************************************************************************/
uint8_t gsp_gen_block_ccaler_coef(uint32_t i_w,
                                  uint32_t i_h,
                                  uint32_t o_w,
                                  uint32_t o_h,
                                  uint32_t hor_tap,
                                  uint32_t ver_tap,
                                  uint32_t *coeff_h_ptr,
                                  uint32_t *coeff_v_ptr,
                                  void *temp_buf_ptr,
                                  uint32_t temp_buf_size,
								  int *init_flag,
								  struct list_head *coef_list)
{
    int16_t D_hor = i_w;    /*decimition at horizontal*/
    int16_t I_hor = o_w;    /*interpolation at horizontal*/

    int16_t *cong_com_hor = 0;
    int16_t *cong_com_ver = 0;
    int16_t *coeff_array = 0;


    uint32_t coef_buf_size = 0;
    int16_t *temp_filter_ptr = NULL;
    int16_t *filter_ptr = NULL;
    uint32_t filter_buf_size = GSC_COUNT * sizeof(int16_t);
    int16_t filter_len[COEF_ARR_ROWS] = { 0 };
    int16_t coef_len = 0;
    struct gsc_mem_pool pool = { 0 };
    uint32_t i = 0;

    struct coef_entry* entry = NULL;

    if(*init_flag == 1) {
        entry = cache_coef_hit_check(i_w, i_h, o_w, o_h, hor_tap, ver_tap, coef_list);
        if(entry) { /*hit*/
            if((unsigned long)coeff_h_ptr & MEM_OPS_ADDR_ALIGN_MASK
			|| (unsigned long)entry->coef & MEM_OPS_ADDR_ALIGN_MASK) {
                GSP_DEBUG("%s[%d] memcpy use none 8B alignment address!",
						__func__, __LINE__);
            }
            memcpy((void*)coeff_h_ptr, (void*)entry->coef, COEF_MATRIX_ENTRY_SIZE*4);
            cache_coef_move_entry_to_list_head(entry, coef_list);
            return TRUE;
        }
    }

    /* init pool and allocate static array */
    if (!init_pool(temp_buf_ptr, temp_buf_size, &pool)) {
        printk("gsp_gen_block_ccaler_coef: init_pool error! \n");
        return FALSE;
    }

    coef_buf_size = COEF_ARR_ROWS * COEF_ARR_COL_MAX * sizeof(int16_t);
    cong_com_hor = (int16_t*)allocate(coef_buf_size, 2, &pool);
    cong_com_ver = (int16_t*)allocate(coef_buf_size, 2, &pool);
    coeff_array = (int16_t*)allocate(8 * 8, 2, &pool);

    if (NULL == cong_com_hor
        || NULL == cong_com_ver
        ||NULL == coeff_array) {
        //printk("gsp_gen_block_ccaler_coef:%d allocate error!%08x,%08x,%08x\n",__LINE__,cong_com_hor,cong_com_ver,coeff_array);
        return FALSE;
    }

    temp_filter_ptr = allocate(filter_buf_size, 2, &pool);
    filter_ptr = allocate(filter_buf_size, 2, &pool);
    if (NULL == temp_filter_ptr || NULL == filter_ptr) {
        //printk("gsp_gen_block_ccaler_coef:%d allocate error! \n",__LINE__);
        return FALSE;
    }

    /* calculate coefficients of Y component in horizontal direction */
    coef_len = caly_scaling_coef(hor_tap, D_hor, I_hor, temp_filter_ptr, 1, &pool);
    get_filter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
    write_scalar_coef(cong_com_hor, filter_ptr, 8, hor_tap);
    check_coef_range(cong_com_hor, 8, hor_tap, 8);
    gsp_rearrang_coeff(cong_com_hor, coeff_array, hor_tap);
    {
        uint32_t cnts = 0, reg = 0;
        uint16_t p0, p1;
        for (i = 0; i < 8; i++) {
            p0 = (uint16_t)(*(coeff_array + i * 8 + 0));
            p1 = (uint16_t)(*(coeff_array + i * 8 + 1));
            reg = (p0 & 0x1ff)|((p1 & 0x1ff)<<16);
            coeff_h_ptr[cnts + 0] = reg;

            p0 = (uint16_t)(*(coeff_array + i * 8 + 2));
            p1 = (uint16_t)(*(coeff_array + i * 8 + 3));
            reg = (p0 & 0x1ff)|((p1 & 0x1ff)<<16);
            coeff_h_ptr[cnts + 1] = reg;

            p0 = (uint16_t)(*(coeff_array + i * 8 + 4));
            p1 = (uint16_t)(*(coeff_array + i * 8 + 5));
            reg = (p0 & 0x1ff)|((p1 & 0x1ff)<<16);
            coeff_h_ptr[cnts + 2] = reg;

            p0 = (uint16_t)(*(coeff_array + i * 8 + 6));
            p1 = (uint16_t)(*(coeff_array + i * 8 + 7));
            reg = (p0 & 0x1ff)|((p1 & 0x1ff)<<16);
            coeff_h_ptr[cnts + 3] = reg;

            cnts += 4;
        }
    }

    /* calculate coefficients of UV component in horizontal direction */
    coef_len = caluv_scaling_coef(ver_tap, D_hor, I_hor, temp_filter_ptr, 1, &pool);
    get_filter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
    write_scalar_coef(cong_com_ver, filter_ptr, 8, ver_tap);
    check_coef_range(cong_com_ver, 8, ver_tap, 8);
    memset(coeff_array, 0x00, 8 * 8 * sizeof(int16_t));
    gsp_rearrang_coeff(cong_com_ver, coeff_array, ver_tap);
    {
        uint32_t cnts = 0, reg = 0;
        uint16_t p0, p1;
        for (i = 0; i < 8; i++) {
            p0 = (uint16_t)(*(coeff_array + i * 8 + 0));
            p1 = (uint16_t)(*(coeff_array + i * 8 + 1));
            reg = (p0 & 0x1ff)|((p1 & 0x1ff)<<16);
            coeff_v_ptr[cnts + 0] = reg;

            p0 = (uint16_t)(*(coeff_array + i * 8 + 2));
            p1 = (uint16_t)(*(coeff_array + i * 8 + 3));
            reg = (p0 & 0x1ff)|((p1 & 0x1ff)<<16);
            coeff_v_ptr[cnts + 1] = reg;

            p0 = (uint16_t)(*(coeff_array + i * 8 + 4));
            p1 = (uint16_t)(*(coeff_array + i * 8 + 5));
            reg = (p0 & 0x1ff)|((p1 & 0x1ff)<<16);
            coeff_v_ptr[cnts + 2] = reg;

            p0 = (uint16_t)(*(coeff_array + i * 8 + 6));
            p1 = (uint16_t)(*(coeff_array + i * 8 + 7));
            reg = (p0 & 0x1ff)|((p1 & 0x1ff)<<16);
            coeff_v_ptr[cnts + 3] = reg;

            cnts += 4;
        }
    }

    if(*init_flag == 1) {
        entry = list_first_entry(coef_list, struct coef_entry, entry_list);
        if(entry->in_w == 0) {
            GSP_DEBUG("GSP_CACHE_COEF:add\n");
        } else {
            GSP_DEBUG("GSP_CACHE_COEF:swap\n");
        }
        if((unsigned long)coeff_h_ptr & MEM_OPS_ADDR_ALIGN_MASK
		|| (unsigned long)entry->coef & MEM_OPS_ADDR_ALIGN_MASK) {
            GSP_DEBUG("%s[%d] memcpy use none 8B alignment address!",
					__func__, __LINE__);
        }
        memcpy((void*)entry->coef,(void*)coeff_h_ptr,COEF_MATRIX_ENTRY_SIZE*4);
        cache_coef_move_entry_to_list_head(entry, coef_list);
        LIST_SET_ENTRY_KEY(entry, i_w, i_h, o_w, o_h,
				hor_tap, ver_tap);
    }

    return TRUE;
}
void gsp_scale_coef_tab_config(uint32_t *p_h_coeff,
		uint32_t *p_v_coeff, unsigned long reg_addr)
{
    uint32_t i=0, j = 0;
    uint32_t *scaling_reg_hor = 0, *scaling_reg_ver = 0;
    unsigned long scale_h_coef_addr = GSP_HOR_COEF_BASE(reg_addr),
		  scale_v_coef_addr = GSP_VER_COEF_BASE(reg_addr);


    scaling_reg_hor = p_h_coeff;

    for( i = 0; i < 8; i++) {
        for(j = 0; j < 4; j++) {
            *(volatile uint32_t*)scale_h_coef_addr = *scaling_reg_hor;
            scale_h_coef_addr += 4;
            scaling_reg_hor++;
        }
    }

    scaling_reg_ver = p_v_coeff;
    for( i = 0; i < 8; i++) {
        for(j = 0; j < 4; j++) {
            *(volatile uint32_t*)scale_v_coef_addr = *scaling_reg_ver;
            scale_v_coef_addr += 4;
            scaling_reg_ver++;
        }
    }
}

static void gsp_coef_tap_convert(uint8_t h_tap,uint8_t v_tap, struct gsp_cfg_info *cfg)
{
    switch(h_tap) {
        case 8:
            cfg->layer0_info.row_tap_mode = 0;
            break;
        case 6:
            cfg->layer0_info.row_tap_mode = 1;
            break;
        case 4:
            cfg->layer0_info.row_tap_mode = 2;
            break;
        case 2:
            cfg->layer0_info.row_tap_mode = 3;
            break;
        default:
            cfg->layer0_info.row_tap_mode = 0;
            break;
    }

    switch(v_tap) {
        case 8:
            cfg->layer0_info.col_tap_mode = 0;
            break;
        case 6:
            cfg->layer0_info.col_tap_mode = 1;
            break;
        case 4:
            cfg->layer0_info.col_tap_mode = 2;
            break;
        case 2:
            cfg->layer0_info.col_tap_mode = 3;
            break;
        default:
            cfg->layer0_info.col_tap_mode = 0;
            break;
    }
    cfg->layer0_info.row_tap_mode &= 0x3;
    cfg->layer0_info.col_tap_mode &= 0x3;
}

int32_t gsp_coef_gen_and_cfg(unsigned long* force_calc, struct gsp_cfg_info *cfg,
		int *init_flag, unsigned long reg_addr, struct list_head *coef_list)
{
    uint8_t     h_tap = 8;
    uint8_t     v_tap = 8;
    uint32_t    *tmp_buf = NULL;
    uint32_t    *h_coeff = NULL;
    uint32_t    *v_coeff = NULL;
    uint32_t    coef_factor_w = 0;
    uint32_t    coef_factor_h = 0;
    uint32_t    after_rotate_w = 0;
    uint32_t    after_rotate_h = 0;
    uint32_t    coef_in_w = 0;
    uint32_t    coef_in_h = 0;
    uint32_t    coef_out_w = 0;
    uint32_t    coef_out_h = 0;
	/*if the new in w h out w h equal last params, don't need calc again*/
    static volatile uint32_t coef_in_w_last = 0;
    static volatile uint32_t coef_in_h_last = 0;
    static volatile uint32_t coef_out_w_last = 0;
    static volatile uint32_t coef_out_h_last = 0;
    static volatile uint32_t coef_h_tap_last = 0;
    static volatile uint32_t coef_v_tap_last = 0;

    if(cfg->layer0_info.scaling_en == 1) {
        if(cfg->layer0_info.des_rect.rect_w < 4
           ||cfg->layer0_info.des_rect.rect_h < 4) {
            return GSP_KERNEL_GEN_OUT_RANG;
        }

        if(cfg->layer0_info.rot_angle == GSP_ROT_ANGLE_0
           ||cfg->layer0_info.rot_angle == GSP_ROT_ANGLE_180
           ||cfg->layer0_info.rot_angle == GSP_ROT_ANGLE_0_M
           ||cfg->layer0_info.rot_angle == GSP_ROT_ANGLE_180_M) {
            after_rotate_w = cfg->layer0_info.clip_rect.rect_w;
            after_rotate_h = cfg->layer0_info.clip_rect.rect_h;
        } else if(cfg->layer0_info.rot_angle == GSP_ROT_ANGLE_90
                  ||cfg->layer0_info.rot_angle == GSP_ROT_ANGLE_270
                  ||cfg->layer0_info.rot_angle == GSP_ROT_ANGLE_90_M
                  ||cfg->layer0_info.rot_angle == GSP_ROT_ANGLE_270_M) {
            after_rotate_w = cfg->layer0_info.clip_rect.rect_h;
            after_rotate_h = cfg->layer0_info.clip_rect.rect_w;
        }

        coef_factor_w = CEIL(after_rotate_w,cfg->layer0_info.des_rect.rect_w);
        coef_factor_h = CEIL(after_rotate_h,cfg->layer0_info.des_rect.rect_h);

        if(coef_factor_w > 16 || coef_factor_h > 16) {
            return GSP_KERNEL_GEN_OUT_RANG;
        }

        if(coef_factor_w > 8) {
            coef_factor_w = 4;
        } else if(coef_factor_w > 4) {
            coef_factor_w = 2;
        } else {
            coef_factor_w = 1;
        }

        if(coef_factor_h > 8) {
            coef_factor_h = 4;
        } else if(coef_factor_h > 4) {
            coef_factor_h = 2;
        } else {
            coef_factor_h= 1;
        }

        coef_in_w = CEIL(after_rotate_w,coef_factor_w);
        coef_in_h = CEIL(after_rotate_h,coef_factor_h);
        coef_out_w = cfg->layer0_info.des_rect.rect_w;
        coef_out_h = cfg->layer0_info.des_rect.rect_h;

        if(GSP_SRC_FMT_RGB565 < cfg->layer0_info.img_format
           && cfg->layer0_info.img_format < GSP_SRC_FMT_8BPP
           && (coef_in_w>coef_out_w||coef_in_h>coef_out_h)) { /*video scaling down*/
            h_tap = 2;
            v_tap = 2;
            if(coef_in_w*3 <= coef_in_h*2) { /*height is larger than 1.5*width*/
                v_tap = 4;
            }
            if(coef_in_h*3 <= coef_in_w*2) { /*width is larger than 1.5*height*/
                h_tap = 4;
            }
        }

        /*give hal a chance to set tap number*/
        if((cfg->layer0_info.row_tap_mode>0) || (cfg->layer0_info.col_tap_mode>0)) {
            h_tap = (cfg->layer0_info.row_tap_mode>0)?cfg->layer0_info.row_tap_mode:h_tap;
            v_tap = (cfg->layer0_info.col_tap_mode>0)?cfg->layer0_info.col_tap_mode:v_tap;
        }

        if(*force_calc == 1
           ||coef_in_w_last != coef_in_w
           || coef_in_h_last != coef_in_h
           || coef_out_w_last != coef_out_w
           || coef_out_h_last != coef_out_h
           || coef_h_tap_last != h_tap
           || coef_v_tap_last != v_tap) {
            tmp_buf = (uint32_t *)kmalloc(GSP_COEFF_BUF_SIZE, GFP_KERNEL);
            if (NULL == tmp_buf) {
                printk("SCALE DRV: No mem to alloc coeff buffer! \n");
                return GSP_KERNEL_GEN_ALLOC_ERR;
            }
            h_coeff = tmp_buf;
            v_coeff = tmp_buf + (GSP_COEFF_COEF_SIZE/4);

            if (!(gsp_gen_block_ccaler_coef(coef_in_w,
                                            coef_in_h,
                                            coef_out_w,
                                            coef_out_h,
                                            h_tap,
                                            v_tap,
                                            h_coeff,
                                            v_coeff,
                                            tmp_buf + (GSP_COEFF_COEF_SIZE/2),
                                            GSP_COEFF_POOL_SIZE, init_flag,
											coef_list))) {
                kfree(tmp_buf);
                printk("GSP DRV: GSP_Gen_Block_Ccaler_Coef error! \n");
                return GSP_KERNEL_GEN_COMMON_ERR;
            }
			/*write coef-metrix to register*/
            gsp_scale_coef_tab_config(h_coeff, v_coeff, reg_addr);
            coef_in_w_last = coef_in_w;
            coef_in_h_last = coef_in_h;
            coef_out_w_last = coef_out_w;
            coef_out_h_last = coef_out_h;
            coef_h_tap_last = h_tap;
            coef_v_tap_last = v_tap;
            *force_calc = 0;
        }

        gsp_coef_tap_convert(coef_h_tap_last, coef_v_tap_last, cfg);
        GSP_L0_SCALETAPMODE_SET(cfg->layer0_info.row_tap_mode,
								cfg->layer0_info.col_tap_mode,
								reg_addr);
        kfree(tmp_buf);
    }
    return GSP_NO_ERR;
}


