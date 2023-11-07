/*
 *	Copyright (C) 2016 Samsung Electronics Co, Ltd.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */

#ifndef _VIBRATOR_H
#define _VIBRATOR_H

#define MOTOR_STRENGTH		96
#define GP_CLK_M_DEFAULT			3
#define GP_CLK_N_DEFAULT            137
#define GP_CLK_D_DEFAULT			68  /* 50% duty cycle */
#define IMM_PWM_MULTIPLIER			137
#define VIB_DEFAULT_TIMEOUT	10000
#define BASE_REGISTER_NUM	6

#define MAX_INTENSITY		10000
#define HIGH_INTENSITY			10000	/*HIGH 6667 ~ 10000*/
#define HIGH_INTENSITY_VOLTAGE		3300000	/*HIGH Voltage 3.3V*/
#define MID_INTENSITY			6666	/*MID 3334 ~ 6666*/
#define MID_INTENSITY_VOLTAGE		3000000	/*MID Voltage 3.0V*/
#define LOW_INTENSITY			3333	/*LOW 1 ~ 3333*/
#define LOW_INTENSITY_VOLTAGE		2700000	/*LOW Voltage 2.7V*/
#define NO_INTENSITY			0		/*NO INTENSITY*/
#define NO_INTENSITY_VOLTAGE		0		/* Voltage 0V*/

#define MASK_VALUE		0xffffffff

/*
 * ** Global variables for LRA PWM M,N and D values.
 * */
int32_t g_nlra_gp_clk_m = GP_CLK_M_DEFAULT;
int32_t g_nlra_gp_clk_n = GP_CLK_N_DEFAULT;
int32_t g_nlra_gp_clk_d = GP_CLK_D_DEFAULT;
int32_t g_nlra_gp_clk_pwm_mul = IMM_PWM_MULTIPLIER;
int32_t motor_strength = MOTOR_STRENGTH;

#define out_dword_masked_ns(io, mask, val, current_reg_content) \
	(void) iowrite32(\
	((current_reg_content & (unsigned int)(~(mask))) \
	| ((unsigned int)((val) & (mask)))), io)

enum {
	CMD_RCGR = 0,
	CFG_RCGR,
	GP_M,
	GP_N,
	GP_D,
	CAMSS_CBCR,
};

#ifndef CONFIG_DC_MOTOR_PMIC
static int32_t min_strength;
#endif
static void __iomem *virt_mmss_gp1_base;
static void __iomem *gp_addr[BASE_REGISTER_NUM];
#endif  /* _VIBRATOR_H */
