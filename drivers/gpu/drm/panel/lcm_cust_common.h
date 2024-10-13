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
#define ID_REG   0X03

int _lcm_i2c_read_bytes(unsigned char cmd, unsigned char *returnData);
int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value);

#endif
