/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#include "gspn_drv.h"
#include "gspn_coef_generate.h"

//we use "Least Recently Used(LRU)" to implement the coef-matrix cache policy

/*
func:cache_coef_hit_check
desc:find the entry have the same in_w in_h out_w out_h
return:if hit,return the entry pointer; else return null;
*/
static COEF_ENTRY_T* gspn_coef_cache_hit_check(GSPN_CONTEXT_T *gspnCtx,
        uint16_t in_w, uint16_t in_h,
        uint16_t out_w,uint16_t out_h,
        uint16_t hor_tap, uint16_t ver_tap)
{
    static uint32_t total_cnt = 0;// for debug
    static uint32_t hit_cnt = 0;// for debug
    COEF_ENTRY_T* pos = NULL;

    total_cnt++;
    GSPN_LOGI("1111, &gspnCtx->coef_list = 0x%x\n", &gspnCtx->coef_list);		
    list_for_each_entry(pos, &gspnCtx->coef_list, list) {
    GSPN_LOGI("1111, pos = 0x%x\n", pos);		
        if(pos->in_w == in_w
           && pos->in_h == in_h
           && pos->out_w == out_w
           && pos->out_h == out_h
           && pos->hor_tap == hor_tap
           && pos->ver_tap == ver_tap) {
            hit_cnt++;
            GSPN_LOGI("hit, hit_ratio:%d percent.\n",hit_cnt*100/total_cnt);
            return pos;
        }
    }
    GSPN_LOGI("miss.\n");
    return NULL;
}

static inline void gspn_coef_cache_move_to_head(GSPN_CONTEXT_T *gspnCtx, COEF_ENTRY_T* pEntry)
{
    list_del(&pEntry->list);
    list_add(&pEntry->list, &gspnCtx->coef_list);
}

static int16_t sum_fun(int16_t *data, int8_t ilen)
{
    int16_t tmp_sum = 0;
    int8_t  i;

    for (i = 0; i < ilen; i++)
        tmp_sum += *data++;
    return tmp_sum;
}


static void* _Allocate(uint32_t size,
                       uint32_t align_shift,
                       GSC_MEM_POOL * pool_ptr)
{
    ulong begin_addr = 0;
    ulong temp_addr = 0;
    if (NULL == pool_ptr) {
        //printk("GSP_Allocate:%d _Allocate error! \n",__LINE__);
        return NULL;
    }
    begin_addr = pool_ptr->begin_addr;
    temp_addr = begin_addr + pool_ptr->used_size;
    temp_addr = (((temp_addr + (1UL << align_shift)-1) >> align_shift) << align_shift);
    if (temp_addr + size > begin_addr + pool_ptr->total_size) {
        //printk("GSP_Allocate err:%d,temp_addr:0x%08x,size:%d,begin_addr:0x%08x,total_size:%d,used_size:%d\n",__LINE__,temp_addr,size,begin_addr,pool_ptr->total_size,pool_ptr->used_size);
        return NULL;
    }
    pool_ptr->used_size = (temp_addr + size) - begin_addr;
    memset((void *)temp_addr, 0, size);
    //printk("GSP_Allocate:%d _Allocate success!%08x \n",__LINE__,temp_addr);
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

static void adjust_filter_inter(int16_t *filter, uint8_t ilen)
{
    uint8_t i, midi     ;
    int     tmpi, tmp_S ;
    int     tmp_val = 0 ;

    tmpi = sum_fun(filter, ilen) - 256;
    midi = ilen >> 1;
    SIGN2(tmp_val, tmpi);

    if ((tmpi & 1) == 1) { // tmpi is odd
        filter[midi] = filter[midi] - tmp_val;
        tmpi -= tmp_val;
    }

    tmp_S = abs(tmpi>>1);

    if ((ilen & 1) == 1) { // ilen is odd
        for (i = 0; i < tmp_S; i++) {
            filter[midi - (i+1)] = filter[midi - (i+1)] - tmp_val;
            filter[midi + (i+1)] = filter[midi + (i+1)] - tmp_val;
        }
    } else { // ilen is even
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


static uint32_t CalYmodelCoef(uint32_t coef_lenght,
                              int16_t *coef_data_ptr,
                              uint32_t N,
                              uint32_t M,
                              GSC_MEM_POOL *pool_ptr)
{
    int8_t  mount;
    int16_t i, mid_i, kk, j, sum_val;
    int64_t *d_filter = _Allocate(GSC_COUNT * sizeof(int64_t), 3, pool_ptr);
    int64_t *tmp_filter = _Allocate(GSC_COUNT * sizeof(int64_t), 3, pool_ptr);
    int16_t *normal_filter = _Allocate(GSC_COUNT * sizeof(int16_t), 2, pool_ptr);

    mid_i           = coef_lenght >> 1;
    d_filter[mid_i] = div64_s64_s64((int64_t) ((int64_t) N << GSC_FIX),(int64_t) MAX(M, N));
    for (i = 0; i < mid_i; i++) {
        int64_t angle_x =
            div64_s64_s64((int64_t) ARC_32_COEF * (int64_t) (i + 1) * (int64_t) N, (int64_t) MAX(M,
                          N) * (int64_t) 8);
        int64_t angle_y =
            div64_s64_s64((int64_t) ARC_32_COEF * (int64_t) (i + 1) *
                          (int64_t) N, (int64_t) (M * N) * (int64_t) 8);
        int32_t value_x = sin_32((int32_t) angle_x);
        int32_t value_y = sin_32((int32_t) angle_y);
        d_filter[mid_i + i + 1] =
            div64_s64_s64((int64_t)
                          ((int64_t)value_x *(int64_t)(1 << GSC_FIX)),
                          (int64_t)((int64_t) M * (int64_t) value_y));
        d_filter[mid_i - (i + 1)] = d_filter[mid_i + i + 1];
    }
    for (i = -1; i < mid_i; i++) {
        int32_t angle_32 = (int32_t)div64_s64_s64((int64_t)
                           ((int64_t)2 * (int64_t)(mid_i - i - 1)*(int64_t)ARC_32_COEF),
                           (int64_t)coef_lenght);
        int64_t a = (int64_t)9059697;
        int64_t b = (int64_t)7717519;
        int64_t t = a - ((b * cos_32(angle_32)) >> 30);
        d_filter[mid_i + i + 1] = (t * d_filter[mid_i + i + 1]) >> GSC_FIX;
        d_filter[mid_i - (i + 1)] = d_filter[mid_i + i + 1];
    }
    for (i = 0; i < 8; i++) {
        mount = 0;
        for (j = i; j < coef_lenght; j += 8) {
            tmp_filter[mount] = d_filter[j];
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
    return 1;
}

static uint32_t CalY_ScalingCoef(uint32_t tap,
                                 uint32_t D,
                                 uint32_t I,
                                 int16_t *y_coef_data_ptr,
                                 uint32_t dir,
                                 GSC_MEM_POOL *pool_ptr)
{
    int16_t coef_lenght;

    if(1 == dir) { // x direction
        coef_lenght = (int16_t) (tap * 8);
        memset(y_coef_data_ptr,0,coef_lenght*sizeof(int16_t));
        CalYmodelCoef(coef_lenght, y_coef_data_ptr, I, D, pool_ptr);
    } else {
        if(D > I) {
            coef_lenght = (int16_t) (tap * 8);
            memset(y_coef_data_ptr,0,coef_lenght*sizeof(int16_t));
            CalYmodelCoef(coef_lenght, y_coef_data_ptr, I, D, pool_ptr);
        } else {
            coef_lenght = (int16_t) (4 * 8);
            memset(y_coef_data_ptr,0,coef_lenght*sizeof(int16_t));
            CalYmodelCoef(coef_lenght, y_coef_data_ptr, I, D, pool_ptr);
        }
    }

    return coef_lenght;
}


void GetFilter(int16_t pos_start,  //position
               int16_t *coef_data_ptr, //ptr== pointer
               int16_t *out_filter,
               int16_t iI_hor,  //integer Interpolation  horizontal
               int16_t coef_len,
               int16_t *filter_len)
{
    int16_t i;
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


void WriteScalarCoef(int16_t *scale_coef,
                     int16_t *coef_ptr,
                     int16_t len)
{
    int i, j;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < len; j++) {
            *(scale_coef + j) = *(coef_ptr + i * len + len - 1 - j);
        }
    }
}


void _rearrang_coeff(void* src, void*dst, int32_t tap)
{
    uint32_t i, j;
    int16_t *src_ptr, *dst_ptr;

    src_ptr = (int16_t*)src;
    dst_ptr = (int16_t*)dst;

    memset((void*)dst_ptr, 0x00, 8*8*sizeof(int16_t));

    switch(tap) {
        case 6:
        case 2: {
            for(i = 0; i<8; i++) {
                for(j = 0; j< tap; j++) {
                    *(dst_ptr+i*32+1+j) = *(src_ptr+i*32+j);
                }
            }
        }
        break;

        case 4: {
            for(i = 0; i<8; i++) {
                for(j = 0; j< tap; j++) {
                    *(dst_ptr+i*16+j) = *(src_ptr+i*16+j);
                }
            }
        }
        case 8: {
            for(i = 0; i<8; i++) {
                for(j = 0; j< tap; j++) {
                    *(dst_ptr+i*32+j) = *(src_ptr+i*32+j);
                }
            }
        }
        break;
    }
}

void ConfigRegister_BlockScalingCoef(int32_t *reg_scaling_hor,
                                     int32_t iHorBufLen,
                                     int32_t *reg_scaling_ver,
                                     int32_t iVerBufLen,
                                     int16_t *cong_Ycom_hor,
                                     int16_t *cong_Ycom_ver)
{
    uint8_t i;
    int16_t p0, p1 ;
    int32_t reg ;
    uint32_t cnts = 0;


    for(i = 0; i < 8; i++) {
        p1 = (uint16_t) (*(cong_Ycom_hor+i*8+7)) ;
        p0 = (uint16_t) (*(cong_Ycom_hor+i*8+6)) ;

        reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 16) ;
        reg_scaling_hor[cnts+3] = reg ;


        p1 = (uint16_t) (*(cong_Ycom_hor+i*8+5)) ;
        p0 = (uint16_t) (*(cong_Ycom_hor+i*8+4)) ;

        reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 16 ) ;
        reg_scaling_hor[cnts+2] = reg ;


        p1 = (uint16_t) (*(cong_Ycom_hor+i*8+3)) ;
        p0 = (uint16_t) (*(cong_Ycom_hor+i*8+2)) ;

        reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 16) ;
        reg_scaling_hor[cnts+1] = reg ;


        p1 = (uint16_t) (*(cong_Ycom_hor+i*8+1));
        p0 = (uint16_t) (*(cong_Ycom_hor+i*8+0)) ;

        reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 16 ) ;
        reg_scaling_hor[cnts+0] = reg ;

        cnts += 4;
    }

    cnts = 0;
    for(i = 0; i < 8; i++) {
        p1 = (uint16_t) (*(cong_Ycom_ver+i*8+3)) ;
        p0 = (uint16_t) (*(cong_Ycom_ver+i*8+2)) ;

        reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 16 ) ;
        reg_scaling_ver[cnts+1] = reg ;

        p1 = (uint16_t) (*(cong_Ycom_ver+i*8+1)) ;
        p0= (uint16_t) (*(cong_Ycom_ver+i*8+0)) ;

        reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 16 ) ;
        reg_scaling_ver[cnts+0] = reg ;

        cnts += 4;
    }
}


static uint8_t _InitPool(void *buffer_ptr,
                         uint32_t buffer_size,
                         GSC_MEM_POOL *pool_ptr)
{
    if (NULL == buffer_ptr || 0 == buffer_size || NULL == pool_ptr) {
        return 0;
    }
    if (buffer_size < MIN_POOL_SIZE) {
        return 0;
    }
    pool_ptr->begin_addr = (ulong) buffer_ptr;
    pool_ptr->total_size = buffer_size;
    pool_ptr->used_size = 0;
    //printk("GSP_InitPool:%d,begin_addr:0x%08x,total_size:%d,used_size:%d\n",__LINE__,pool_ptr->begin_addr,pool_ptr->total_size,pool_ptr->used_size);
    return 1;
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

uint32_t* gspn_gen_block_scaler_coef(GSPN_CONTEXT_T *gspnCtx,
                                     uint32_t i_w,
                                     uint32_t i_h,
                                     uint32_t o_w,
                                     uint32_t o_h,
                                     uint32_t hor_tap,
                                     uint32_t ver_tap)
{
    int16_t *cong_Ycom_hor = NULL;
    int16_t *cong_Ycom_ver = NULL;

    int16_t coef_len, i, j;
    int16_t Y_hor_filter_len[100]  = {0};
    int16_t Y_ver_filter_len[100]  = {0};

    int16_t pos_start;
    int32_t *scaling_reg_buf_hor = NULL;
    int32_t *scaling_reg_buf_ver = NULL;
    int32_t hait_tmp1 = 0;
    int32_t hait_tmp2 = 0;
    //----------------------------------
    int16_t *coeff_array_hor = NULL;
    int16_t *coeff_array_ver = NULL;
    uint32_t coef_buf_size = 0;
    int16_t *y_coef_data_x = NULL;
    int16_t *Y_hor_filter = NULL;
    int16_t *Y_ver_filter = NULL;
    uint32_t filter_buf_size = GSC_COUNT * sizeof(int16_t);
    GSC_MEM_POOL pool = { 0 };
    //----------------------------------
    COEF_ENTRY_T* pEntry = NULL;
	
    //GSPN_LOGI("1111.\n");
    if(gspnCtx->cache_coef_init_flag == 1) {
        //GSPN_LOGI("1111, gspnCtx_addr = %d.\n", gspnCtx);
        pEntry = gspn_coef_cache_hit_check(gspnCtx, i_w,i_h,o_w,o_h,hor_tap,ver_tap);
        if(pEntry) { //hit
            gspn_coef_cache_move_to_head(gspnCtx, pEntry);
            return pEntry->coef;
        }
    }

    //GSPN_LOGI("1111.\n");
    /* init pool and allocate static array */
    if (!_InitPool(gspnCtx->coef_buf_pool, MIN_POOL_SIZE, &pool)) {
        printk("GSP_Gen_Block_Ccaler_Coef: _InitPool error! \n");
        return 0;
    }
    //GSPN_LOGI("1111.\n");
    coef_buf_size = COEF_ARR_ROWS * COEF_ARR_COL_MAX * sizeof(int16_t);
    cong_Ycom_hor = (int16_t*)_Allocate(coef_buf_size, 2, &pool);
    cong_Ycom_ver = (int16_t*)_Allocate(coef_buf_size, 2, &pool);
    coeff_array_hor = (int16_t*)_Allocate(8 * 8, 2, &pool);
    coeff_array_ver = (int16_t*)_Allocate(8 * 8, 2, &pool);
    if (NULL == cong_Ycom_hor || NULL == cong_Ycom_ver ||NULL == coeff_array_hor ||NULL == coeff_array_ver) {
        return 0;
    }
    //GSPN_LOGI("1111.\n");
    y_coef_data_x = _Allocate(filter_buf_size, 2, &pool);
    Y_hor_filter = _Allocate(filter_buf_size, 2, &pool);
    Y_ver_filter = _Allocate(filter_buf_size, 2, &pool);
    //GSPN_LOGI("1111.\n");
    if (NULL == y_coef_data_x || NULL == Y_hor_filter || NULL == Y_ver_filter) {
        GSPN_LOGI("1111.\n");
		return 0;
    }

    GSPN_LOGI("1111.\n");
    /* horizontal direction */
    /* Y component */
    coef_len = CalY_ScalingCoef(hor_tap, i_w, o_w, y_coef_data_x, 1, &pool);

    pos_start = coef_len / 2;
    while (pos_start >= 8) {
        pos_start -= 8;
    }
    //GSPN_LOGI("1111.\n");
    GetFilter(pos_start, y_coef_data_x, Y_hor_filter, 8, coef_len, Y_hor_filter_len);
    WriteScalarCoef(cong_Ycom_hor, Y_hor_filter, hor_tap);
    CheckCoefRange(cong_Ycom_hor, 8, hor_tap, 8);

    GSPN_LOGI("1111.\n");
    /* vertical direction */
    coef_len = CalY_ScalingCoef(ver_tap, i_h, o_h, y_coef_data_x, 1, &pool);

    pos_start = coef_len / 2;
    while (pos_start >= 8) {
        pos_start -= 8;
    }
    GSPN_LOGI("1111.\n");
    GetFilter(pos_start, y_coef_data_x, Y_ver_filter, 8, coef_len, Y_ver_filter_len);
    GSPN_LOGI("1111.\n");
	WriteScalarCoef(cong_Ycom_ver, Y_ver_filter, ver_tap);
   // GSPN_LOGI("1111.\n");
	CheckCoefRange(cong_Ycom_ver, 8, ver_tap, 8);
    GSPN_LOGI("1111.\n");
    _rearrang_coeff(cong_Ycom_hor, coeff_array_hor, hor_tap);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < hor_tap; j++) {
            hait_tmp1 = *(coeff_array_hor + i*8 +j);

            if(hait_tmp1<0) {
                hait_tmp1 = 0-hait_tmp1;
                hait_tmp2 = ((~hait_tmp1)&0x01ff)+1;
            } else {
                hait_tmp2 = hait_tmp1;
            }
            *(coeff_array_hor + i*8 +j) = hait_tmp2;
        }
    }
    GSPN_LOGI("1111.\n");
    _rearrang_coeff(cong_Ycom_ver, coeff_array_ver, ver_tap);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < ver_tap; j++) {
            hait_tmp1 = *(coeff_array_ver + i*8 +j);

            if(hait_tmp1<0) {
                hait_tmp1 = 0-hait_tmp1;
                hait_tmp2 = ((~hait_tmp1)&0x01ff)+1;
            } else {
                hait_tmp2 = hait_tmp1;
            }
            *(coeff_array_ver + i*8 +j) = hait_tmp2;
        }
    }
    GSPN_LOGI("1111.\n");
    scaling_reg_buf_hor = &gspnCtx->coef_calc_buf[0];
    scaling_reg_buf_ver = &gspnCtx->coef_calc_buf[SCALER_COEF_TAB_LEN_HOR];

    ConfigRegister_BlockScalingCoef(scaling_reg_buf_hor,
                                    SCALER_COEF_TAB_LEN_HOR,
                                    scaling_reg_buf_ver,
                                    SCALER_COEF_TAB_LEN_VER,
                                    coeff_array_hor,
                                    coeff_array_ver) ;

    if(gspnCtx->cache_coef_init_flag == 1) {
        pEntry = list_entry(gspnCtx->coef_list.prev, COEF_ENTRY_T, list);
        if(pEntry->in_w == 0) {
            GSPN_LOGI("add.\n");
        } else {
            GSPN_LOGI("swap.\n");
        }
        if((ulong)scaling_reg_buf_hor & MEM_OPS_ADDR_ALIGN_MASK || (ulong)&pEntry->coef[0] & MEM_OPS_ADDR_ALIGN_MASK) {
            GSPN_LOGW("memcpy use none 8B alignment address!");
        }

		int32_t scaling_reg_buf_hor_new[32] = {    0x01f101fe, 0x009f0041, 0x01f10041, 0x000101fe,
													  0x01f00000, 0x009e0030, 0x01f30052, 0x000101fc,
													  0x01f10000, 0x00990020, 0x01f80063, 0x000101fa,
													  0x01f30001, 0x00bf0013, 0x01ff0073, 0x000101f7,
													  0x01f50001, 0x00830007, 0x00070083, 0x000101f5,
													  0x01f70001, 0x007401ff, 0x001300be, 0x000001f3,
													  0x01fa0001, 0x006401f8, 0x00200098, 0x000001f1,
													  0x01fc0001, 0x005301f3, 0x0030009d, 0x000001f0
													  };
		int32_t scaling_reg_buf_ver_new[16] = {    0x00b40027, 0x01fe0027, 0x00af0018, 0x01fe003b,
													  0x00a5000c, 0x01fe0051, 0x00950004, 0x01fe0069,
													  0x00800000, 0x00000080, 0x006a01fe, 0x00040094,
													  0x005201fe, 0x000c00a4, 0x003b01fe, 0x001800af
													};

        gsp_memcpy((void*)&pEntry->coef[0],(void*)scaling_reg_buf_hor_new,32*4);
        if((ulong)scaling_reg_buf_ver & MEM_OPS_ADDR_ALIGN_MASK || (ulong)&pEntry->coef[32] & MEM_OPS_ADDR_ALIGN_MASK) {
            GSPN_LOGW("memcpy use none 8B alignment address!");
        }
        gsp_memcpy((void*)&pEntry->coef[32],(void*)scaling_reg_buf_ver_new,16*4);
        gspn_coef_cache_move_to_head(gspnCtx, pEntry);
        LIST_SET_ENTRY_KEY(pEntry,i_w,i_h,o_w,o_h,hor_tap,ver_tap);
    }
    return pEntry->coef;
}


