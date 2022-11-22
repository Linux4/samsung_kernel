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

#include <linux/of_device.h>
#include <video/sensor_drv_k.h>
#include <linux/wakelock.h>

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

enum sensor_id_e {
	SENSOR_DEV_0 = 0,
	SENSOR_DEV_1,
	SENSOR_DEV_2,
	SENSOR_DEV_MAX
};


#define SENSOR_DEV0_I2C_NAME                 "sensor_main"
#define SENSOR_DEV1_I2C_NAME                 "sensor_sub"
#define SENSOR_DEV2_I2C_NAME                 "sensor_i2c_dev2"
#define SENSOR_VCM0_I2C_NAME                 "sensor_i2c_vcm0"


#define SENSOR_DEV0_I2C_ADDR                 0x30
#define SENSOR_DEV1_I2C_ADDR                 0x21
#define SENSOR_DEV2_I2C_ADDR                 0x30

#define SENSOR_I2C_BUST_NB                   7


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

#define ANA_REG_BASE                         MISC_BASE + 0x480
#define ANA_LDO_PD_CTL                       ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2                        ANA_REG_BASE + 0x1C

struct sensor_mclk_tag {
	uint32_t                        clock;
	char                            *src_name;
};

struct sensor_mem_tag {
	void                            *buf_ptr;
	size_t                          size;
};

struct sensor_module_tag {
	struct mutex                    sync_lock;
	atomic_t                        users;
	struct i2c_client               *cur_i2c_client;
	struct i2c_client               *vcm_i2c_client;
	uint32_t                        vcm_gpio_i2c_flag;
};

struct sensor_module_tab_tag {
	atomic_t                        total_users;
	uint32_t                        padding;
	struct mutex                    sensor_id_lock;
	struct device_node              *of_node;
	struct clk                      *sensor_mm_in_clk;
	struct wake_lock                wakelock;
	struct sensor_module_tag        sensor_dev_tab[SENSOR_DEV_MAX];
	SENSOR_OTP_PARAM_T 				otp_param[SENSOR_DEV_MAX];
};

struct sensor_gpio_tag {
	int                             pwn;
	int                             reset;
};

struct sensor_file_tag {
	struct sensor_module_tab_tag    *module_data;
	uint32_t                        sensor_id;
	uint32_t                        sensor_mclk;
	uint32_t                        iopower_on_count;
	uint32_t                        avddpower_on_count;
	uint32_t                        dvddpower_on_count;
	uint32_t                        motpower_on_count;
	uint32_t                        mipi_on;
	uint32_t                        padding;
	uint32_t                        phy_id;
	uint32_t                        if_type;
	struct sensor_gpio_tag          gpio_tab;
	struct clk                      *ccir_clk;
	struct clk                      *ccir_enable_clk;
	struct clk                      *mipi_clk;
	struct regulator                *camvio_regulator;
	struct regulator                *camavdd_regulator;
	struct regulator                *camdvdd_regulator;
	struct regulator                *cammot_regulator;
	struct sensor_mem_tag           sensor_mem;
	void                            *csi_handle;
};


void sensor_k_set_id(struct file *file, uint32_t sensor_id);
int sensor_k_set_i2c_clk(uint32_t *fd_handle, uint32_t clock);
void sensor_k_set_i2c_addr(struct file *file, uint16_t i2c_addr);
int Sensor_K_ReadReg(uint32_t *fd_handle, struct sensor_reg_bits_tag *pReg);
int sensor_k_wr_i2c(uint32_t *fd_handle, struct sensor_i2c_tag *pI2cTab);
int sensor_k_set_pd_level(uint32_t *fd_handle, uint8_t power_level);
int sensor_k_set_rst_level(uint32_t *fd_handle, uint32_t plus_level);
int sensor_k_set_voltage_cammot(uint32_t *fd_handle, uint32_t cammot_val);
int sensor_k_set_voltage_avdd(uint32_t *fd_handle, uint32_t avdd_val);
int sensor_k_set_voltage_dvdd(uint32_t *fd_handle, uint32_t dvdd_val);
int sensor_k_set_voltage_iovdd(uint32_t *fd_handle, uint32_t iodd_val);
int sensor_k_set_mclk(uint32_t *fd_handle, uint32_t mclk);
unsigned short sensor_k_get_sensor_i2c_addr(uint32_t *fd_handle, uint32_t sensor_id);
int sensor_k_sensor_sel(uint32_t *fd_handle, uint32_t sensor_id);
struct device_node *get_device_node(void);

int sensor_set_pd_level_fromkernel(struct file *file, uint8_t arg, uint32_t sensor_id);
int sensor_set_voltage_cammot_fromkernel(struct file *file, SENSOR_VDD_VAL_E arg, uint32_t sensor_id);
int sensor_set_voltage_avdd_fromkernel(struct file *file, SENSOR_VDD_VAL_E arg, uint32_t sensor_id);
int sensor_set_voltage_dvdd_fromkernel(struct file *file, SENSOR_VDD_VAL_E arg, uint32_t sensor_id);
int sensor_set_voltage_iovdd_fromkernel(struct file *file, SENSOR_VDD_VAL_E arg, uint32_t sensor_id);
int sensor_set_mclk_fromkernel(struct file *file, uint32_t arg, uint32_t sensor_id);
int sensor_set_rst_level_fromkernel(struct file *file, uint32_t arg, uint32_t sensor_id);
int sensor_ReadReg_fromkernel(struct file *file, struct sensor_reg_bits_tag *arg);
int sensor_wr_regtab_fromkernel(struct file *file, struct sensor_reg_tab_tag *arg, uint32_t sensor_id);
int sensor_set_i2c_clk_fromkernel(struct file *file, uint32_t arg, uint32_t sensor_id);
int sensor_wr_i2c_fromkernel(struct file *file, struct sensor_i2c_tag *arg);
int sensor_rd_i2c_fromkernel(struct file *file,struct sensor_i2c_tag *arg, uint32_t sensor_id);


#endif //_SENSOR_DRV_K_H_
