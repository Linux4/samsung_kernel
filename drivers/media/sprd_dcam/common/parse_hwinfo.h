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
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sprd_mm.h>

#ifdef CONFIG_64BIT
#include <soc/sprd/sci_glb_regs.h>
#endif

extern unsigned long  dcam_regbase;
extern unsigned long  isp_regbase;
extern unsigned long  csi_regbase;
extern unsigned long  isp_phybase;

#ifndef CONFIG_OF
#define DCAM_BASE       SPRD_DEV_P2V(dcam_regbase)
#define ISP_BASE        SPRD_DEV_P2V(isp_regbase)
#define CSI2_BASE       SPRD_DEV_P2V(csi_regbase)

#else

#define DCAM_BASE       (dcam_regbase)
#define ISP_BASE        (isp_regbase)
#define CSI2_BASE       (csi_regbase)
#endif


enum get_regu_type_e {
	REGU_CAMAVDD = 0,
	REGU_CAMDVDD,
	REGU_CAMIOVDD,
	REGU_CAMMOT,
	REGU_MAX
};
enum get_gpio_type_e {
    GPIO_MAINDVDD = 0,
    GPIO_SUBDVDD,
    GPIO_FLASH_EN,
    GPIO_SWITCH_MODE,
    GPIO_MIPI_SWITCH_EN,
    GPIO_MIPI_SWITCH_MODE,
    GPIO_MAINCAM_ID,
    GPIO_MAINAVDD,
    GPIO_SUBAVDD,
    GPIO_SUB2DVDD,
    GPIO_CAMMAX
};
#ifdef CONFIG_64BIT
//#define SPRD_ISP_SIZE			SZ_32K
//#define SPRD_MMAHB_BASE	SPRD_DEV_P2V(REGS_MM_AHB_BASE)
//#define SPRD_MMCKG_BASE	SPRD_DEV_P2V(REGS_MM_CLK_BASE)
//#define SPRD_PIN_BASE   SPRD_DEV_P2V(0x60a00000)

//#define SPRD_ADISLAVE_BASE SPRD_DEV_P2V(REGS_ADI_BASE)
#define SPRD_FLASH_OFST          0x890
#define SPRD_FLASH_CTRL_BIT      0x8000
#define SPRD_FLASH_LOW_VAL       0x3
#define SPRD_FLASH_HIGH_VAL      0xF
#define SPRD_FLASH_LOW_CUR       110
#define SPRD_FLASH_HIGH_CUR      470
#endif

void parse_baseaddress(struct device_node *dn);
uint32_t parse_irq(struct device_node *dn);
struct clk * parse_clk(struct device_node *dn, char *clkname);
int get_gpio_id(struct device_node *dn, int *pwn, int *reset, uint32_t sensor_id);
int get_gpio_id_ex(struct device_node *dn, int type, int *id, uint32_t sensor_id);
int get_regulator_name(struct device_node *dn, int *type, uint32_t sensor_id, char **name);
