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

#ifndef __SCALER_COEF_CAL__H
#define __SCALER_COEF_CAL__H
#include <linux/types.h>
#include <video/gsp_types_shark_adf.h>

#ifndef COUNT
#define COUNT 256
#endif

#define GSP_COEFF_BUF_SIZE                              (8 << 10)
#define GSP_COEFF_COEF_SIZE                             (1 << 10)
#define GSP_COEFF_POOL_SIZE                             (6 << 10)
#define COEF_MATRIX_ENTRY_SIZE  (GSP_COEFF_COEF_SIZE/2)
#define CACHED_COEF_CNT_MAX 32

struct coef_entry {
	struct  list_head entry_list;
	uint32_t coef[COEF_MATRIX_ENTRY_SIZE];
	uint16_t in_w;
	uint16_t in_h;
	uint16_t out_w;
	uint16_t out_h;
	uint16_t hor_tap;
	uint16_t ver_tap;
};


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
								  struct list_head *coef_list);

void gsp_scale_coef_tab_config(uint32_t *p_h_coeff, uint32_t *p_v_coeff,
		unsigned long reg_addr);

int32_t gsp_coef_gen_and_cfg(unsigned long* force_calc, struct gsp_cfg_info *cfg,
		int *init_flag, unsigned long reg_addr, struct list_head *coef_list);

#endif



