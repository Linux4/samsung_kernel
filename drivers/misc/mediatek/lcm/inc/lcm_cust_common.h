/*
 * Filename: lcm_cust_common.c
 * date:20201209
 * Description: cust common source file
 * Author:samir.liu
 */

#ifndef __LCM_CUST_COMMON_H__
#define __LCM_CUST_COMMON_H__

#define AVDD_REG 0x00
#define AVEE_REG 0X01
#define ENABLE_REG   0X03

enum GPIO_STATUS {
   LOW,
   HIGH,
};

int lcm_set_power_reg(u8 addr, u8 val, u8 mask);
void lcm_reset_pin(unsigned int mode);
void lcm_vio_enable(unsigned int mode);
void lcm_bais_enn_enable(unsigned int mode);
void lcm_bais_enp_enable(unsigned int mode);
int wingtech_bright_to_bl(int level,int max_bright,int min_bright,int bl_max,int bl_min);

#endif
