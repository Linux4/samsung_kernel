/*
*
* $Id: stk3x2x.h
*
* Copyright (C) 2012~2015 Lex Hsieh     <lex_hsieh@sensortek.com.tw>
*
* This file is subject to the terms and conditions of the GNU General Public
* License.  See the file COPYING in the main directory of this archive for
* more details.
*
*/
#ifndef __STK3X2X_H__
#define __STK3X2X_H__
/* platform data */
struct stk3328_platform_data
{
	uint8_t state_reg;
	uint8_t psctrl_reg;
	uint8_t alsctrl_reg;
	uint8_t ledctrl_reg;
	uint8_t wait_reg;
	uint8_t als_cgain;
	uint16_t ps_thd_h;
	uint16_t ps_thd_l;
	uint16_t cancel_hi_thd;
	uint16_t cancel_low_thd;
	uint16_t cal_skip_adc;
	uint16_t cal_fail_adc;
	int int_pin;
	uint32_t transmittance;
	uint16_t ps_default_trim;
	uint16_t ps_offset;
	uint8_t ps_cal_result;
	int use_fir;
};
#endif // __STK3X1X_H__