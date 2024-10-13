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
#ifndef _SENSOR_DRV_SPRD_H_
#define _SENSOR_DRV_SPRD_H_


#define SENSOER_VDD_1200MV                  1200000
#define SENSOER_VDD_1300MV                  1300000
#define SENSOER_VDD_1500MV                  1500000
#define SENSOER_VDD_1800MV                  1800000
#define SENSOER_VDD_2500MV                  2500000
#define SENSOER_VDD_2800MV                  2800000
#define SENSOER_VDD_3000MV                  3000000
#define SENSOER_VDD_3300MV                  3300000
#define SENSOER_VDD_3800MV                  3800000

typedef enum {
	SENSOR_VDD_3800MV = 0,
	SENSOR_VDD_3300MV,
	SENSOR_VDD_3000MV,
	SENSOR_VDD_2800MV,
	SENSOR_VDD_2500MV,
	SENSOR_VDD_1800MV,
	SENSOR_VDD_1500MV,
	SENSOR_VDD_1300MV,
	SENSOR_VDD_1200MV,
	SENSOR_VDD_CLOSED,
	SENSOR_VDD_UNUSED
} SENSOR_VDD_VAL_E;

typedef enum {
	SENSOR_MAIN = 0,
	SENSOR_SUB,
	SENSOR_ATV = 5,
	SENSOR_ID_MAX
} SENSOR_ID_E;

#define SENSOR_MAIN_I2C_NAME                 "sensor_main"
#define SENSOR_SUB_I2C_NAME                  "sensor_sub"
#define SENSOR_MAIN_I2C_ADDR                 0x30
#define SENSOR_SUB_I2C_ADDR                  0x21

#define SENSOR_I2C_BUST_NB                   7

#define SENSOR_DEFALUT_MCLK                           24          /* MHZ */
#define SENSOR_DISABLE_MCLK                           0           /* MHZ */

#define SENSOR_MAX_MCLK                      96       /*MHz*/
#define SENOR_CLK_M_VALUE                    1000000

#define GLOBAL_BASE                          SPRD_GREG_BASE    /*0xE0002E00UL <--> 0x8b000000 */
#define ARM_GLOBAL_REG_GEN0                  (GLOBAL_BASE + 0x008UL)
#define ARM_GLOBAL_REG_GEN3                  (GLOBAL_BASE + 0x01CUL)
#define ARM_GLOBAL_PLL_SCR                   (GLOBAL_BASE + 0x070UL)
#define GR_CLK_GEN5                          (GLOBAL_BASE + 0x07CUL)

#define AHB_BASE                             SPRD_AHB_BASE    /*0xE000A000 <--> 0x20900000UL */
#define AHB_GLOBAL_REG_CTL0                  (AHB_BASE + 0x200UL)
#define AHB_GLOBAL_REG_SOFTRST               (AHB_BASE + 0x210UL)

#define PIN_CTL_BASE                         SPRD_CPC_BASE    /*0xE002F000<-->0x8C000000UL */
#define PIN_CTL_CCIRPD1                      PIN_CTL_BASE + 0x344UL
#define PIN_CTL_CCIRPD0                      PIN_CTL_BASE + 0x348UL

#define MISC_BASE                            SPRD_MISC_BASE    /*0xE0033000<-->0x82000000 */
#ifdef CONFIG_ARCH_SC8810
#define ANA_REG_BASE                         MISC_BASE + 0x600
#define ANA_LDO_PD_CTL                       ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2                        ANA_REG_BASE + 0x20
#else
#define ANA_REG_BASE                         MISC_BASE + 0x480
#define ANA_LDO_PD_CTL                       ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2                        ANA_REG_BASE + 0x1C
#endif

int sensor_k_set_pd_level(char power_level);
int sensor_k_set_rst_level(uint32_t plus_level);
int sensor_k_set_voltage_cammot(uint32_t cammot_val);
int sensor_k_set_voltage_avdd(uint32_t avdd_val);
int sensor_k_set_voltage_dvdd(uint32_t dvdd_val);
int sensor_k_set_voltage_iovdd(uint32_t iodd_val);
int sensor_k_set_mclk(uint32_t mclk);
int sensor_k_sensor_sel(uint32_t sensor_id);

#endif //_SENSOR_DRV_K_H_
