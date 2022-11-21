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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <asm/io.h>
#ifndef CONFIG_64BIT
#include <soc/sprd/hardware.h>
#include <soc/sprd/board.h>
#include <mach/irqs.h>
#else
#include <soc/sprd/irqs.h>
#endif
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>

#include <linux/sprd_iommu.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include "dcam_drv.h"

//#define PARSE_DEBUG

#ifdef PARSE_DEBUG
	#define PARSE_TRACE             printk
#else
	#define PARSE_TRACE             pr_debug
#endif

#ifdef CONFIG_SC_FPGA
#define  CLK_MM_I_IN_CLOCKTREE  0
#else
#define  CLK_MM_I_IN_CLOCKTREE  1
#endif

#ifdef CONFIG_OF
unsigned long dcam_regbase = 0 ;
unsigned long isp_regbase = 0;
unsigned long csi_regbase = 0;
unsigned long isp_phybase = 0;
#else
#endif
static atomic_t	mm_enabe_cnt = ATOMIC_INIT(0);

void parse_baseaddress(struct device_node *dn)
{
#ifdef CONFIG_OF
	struct resource r;

	if (0 == of_address_to_resource(dn, 0, &r)) {
		PARSE_TRACE("PARSE BASE=0x%x\n", r.start);
	} else {
		PARSE_TRACE("PARSE BASE ADDRESS ERROR\n");
		return;
	}
	if (dcam_regbase == 0 && strcmp(dn->name, "sprd_dcam") == 0) {
		dcam_regbase = ioremap_nocache(r.start, resource_size(&r));
		PARSE_TRACE("Dcam register base addr: 0x%lx\n", dcam_regbase);
	} else if (isp_regbase == 0 && strcmp(dn->name, "sprd_isp") == 0) {
		isp_regbase = ioremap_nocache(r.start, resource_size(&r));
		isp_phybase = r.start;
		PARSE_TRACE("Isp register base addr: 0x%lx\n", isp_regbase);
	} else if (csi_regbase == 0 && strcmp(dn->name, "sprd_sensor") == 0) {
		csi_regbase = ioremap_nocache(r.start, resource_size(&r));
		PARSE_TRACE("Csi register base addr: 0x%lx\n", csi_regbase);
	}
#endif
}

uint32_t parse_irq(struct device_node *dn)
{
#ifdef CONFIG_OF
	return irq_of_parse_and_map(dn, 0);
#else
	return DCAM_IRQ;
#endif
}

struct clk * parse_clk(struct device_node *dn, char *clkname)
{
#ifdef CONFIG_OF
	PARSE_TRACE("parse_clk %s \n",clkname);
	return of_clk_get_by_name(dn, clkname);
#else
	return clk_get(NULL, clkname);
#endif
}

int32_t clk_mm_i_eb(struct device_node *dn, uint32_t enable)
{
#ifdef CONFIG_SC_FPGA_CLK
	return 0;
#else
#if CLK_MM_I_IN_CLOCKTREE
	int	ret = 0;
	struct clk*	clk_mm_i = NULL;

	clk_mm_i = parse_clk(dn, "clk_mm_i");
	if (IS_ERR(clk_mm_i)) {
		printk("clk_mm_i_eb: get fail.\n");
		return -1;
	}

	if(enable){
		ret = clk_enable(clk_mm_i);
		if (ret) {
			printk("clk_mm_i_eb: enable fail.\n");
			return -1;
		}
#if defined(CONFIG_SPRD_IOMMU)
		{
			sprd_iommu_module_enable(IOMMU_MM);
		}
#endif
		PARSE_TRACE("clk_mm_i_eb enable ok.\n");
		atomic_inc(&mm_enabe_cnt);
	}else{
#if defined(CONFIG_SPRD_IOMMU)
		{
			sprd_iommu_module_disable(IOMMU_MM);
		}
#endif
		clk_disable(clk_mm_i);
		clk_put(clk_mm_i);
		clk_mm_i = NULL;
		udelay(500);
		atomic_dec(&mm_enabe_cnt);
		PARSE_TRACE("clk_mm_i_eb disable ok.\n");
	}

#else
	if(enable){
		if(atomic_inc_return(&mm_enabe_cnt) == 1) {
			REG_OWR(REG_AON_APB_APB_EB0,BIT_MM_EB);
			REG_AWR(REG_PMU_APB_PD_MM_TOP_CFG,~BIT_PD_MM_TOP_FORCE_SHUTDOWN);
			REG_OWR(REG_MM_AHB_GEN_CKG_CFG,BIT_SENSOR_CKG_EN|BIT_DCAM_AXI_CKG_EN);
			PARSE_TRACE("mm enable ok.\n");
		}
	}else{
		if(atomic_dec_return(&mm_enabe_cnt) == 0) {
			REG_AWR(REG_MM_AHB_GEN_CKG_CFG,~(BIT_SENSOR_CKG_EN|BIT_DCAM_AXI_CKG_EN));
			REG_OWR(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_FORCE_SHUTDOWN);
			REG_AWR(REG_AON_APB_APB_EB0,~BIT_MM_EB);
			udelay(500);
			PARSE_TRACE("mm disable ok.\n");
		}
	}
#endif

	PARSE_TRACE("mm_enabe_cnt = %d.\n",atomic_read(&mm_enabe_cnt));
#ifdef PARSE_DEBUG
	if(enable)
		mm_clk_register_trace();
#endif
	return 0;
#endif
}

#ifndef GPIO_SUB_SENSOR_RESET
#define GPIO_SUB_SENSOR_RESET             0
#endif

#define GPIO_SENSOR_DEV0_PWN              GPIO_MAIN_SENSOR_PWN
#define GPIO_SENSOR_DEV1_PWN              GPIO_SUB_SENSOR_PWN
#define GPIO_SENSOR_DEV2_PWN              0
#define GPIO_SENSOR_DEV0_RST              GPIO_SENSOR_RESET
#define GPIO_SENSOR_DEV1_RST              GPIO_SUB_SENSOR_RESET
#define GPIO_SENSOR_DEV2_RST              0

int get_gpio_id(struct device_node *dn, int *pwn, int *reset, uint32_t sensor_id)
{
	if (NULL == pwn || NULL == reset)
		return -1;

#ifdef CONFIG_OF
	if (0 == sensor_id) {
		*pwn   = of_get_gpio(dn, 1);
		*reset = of_get_gpio(dn, 0);
	} else if (1 == sensor_id) {
		*pwn   = of_get_gpio(dn, 3);
		*reset = of_get_gpio(dn, 2);
	} else if (2 == sensor_id) {
		*pwn   = of_get_gpio(dn, 9);
		*reset = of_get_gpio(dn, 8);
	} else {
		*pwn   = 0;
		*reset = 0;
	}
#else
	if (0 == sensor_id) {
		*pwn   = GPIO_SENSOR_DEV0_PWN;
		*reset = GPIO_SENSOR_DEV0_RST;
	} else if (1 == sensor_id) {
		*pwn   = GPIO_SENSOR_DEV1_PWN;
		*reset = GPIO_SENSOR_DEV1_RST;
	} else {
		*pwn   = GPIO_SENSOR_DEV2_PWN;
		*reset = GPIO_SENSOR_DEV2_RST;
	}
#endif

	return 0;
}

int get_gpio_id_ex(struct device_node *dn, int type, int *id, uint32_t sensor_id)
{
	if (NULL == id)
		return -1;

#ifdef CONFIG_OF
	switch (type) {
		case GPIO_MAINDVDD:
			*id = of_get_gpio(dn, 4);
			break;
		case GPIO_SUBDVDD:
			*id = of_get_gpio(dn, 5);
			break;
		case GPIO_FLASH_EN:
			*id = of_get_gpio(dn, 6);
			break;
		case GPIO_SWITCH_MODE:
			*id = of_get_gpio(dn, 7);
			break;
		case GPIO_MIPI_SWITCH_EN:
			*id = of_get_gpio(dn, 10);
			break;
		case GPIO_MIPI_SWITCH_MODE:
			*id = of_get_gpio(dn, 11);
			break;
		case GPIO_MAINCAM_ID:
			*id = of_get_gpio(dn, 12);
			break;
		case GPIO_MAINAVDD:
			*id = of_get_gpio(dn, 13);
			break;
		case GPIO_SUBAVDD:
			*id = of_get_gpio(dn, 14);
			break;
		case GPIO_SUB2DVDD:
			*id = of_get_gpio(dn, 15);
			break;
		default:
			*id = 0;
			break;
	}
#else
	*id = 0;
#endif

	return 0;
}


#ifndef REGU_DEV0_CAMAVDD
#define REGU_DEV0_CAMAVDD                 "vddcama"
#endif
#ifndef REGU_DEV1_CAMAVDD
#define REGU_DEV1_CAMAVDD                 "vddcama"
#endif
#ifndef REGU_DEV2_CAMAVDD
#define REGU_DEV2_CAMAVDD                 "vddcama"
#endif
#ifndef REGU_DEV0_CAMDVDD
#define REGU_DEV0_CAMDVDD                 "vddcamd"
#endif
#ifndef REGU_DEV1_CAMDVDD
#define REGU_DEV1_CAMDVDD                 "vddcamd"
#endif
#ifndef REGU_DEV2_CAMDVDD
#define REGU_DEV2_CAMDVDD                 "vddcamd"
#endif
#ifndef REGU_DEV0_CAMIOVDD
#define REGU_DEV0_CAMIOVDD                "vddcamio"
#endif
#ifndef REGU_DEV1_CAMIOVDD
#define REGU_DEV1_CAMIOVDD                "vddcamio"
#endif
#ifndef REGU_DEV2_CAMIOVDD
#define REGU_DEV2_CAMIOVDD                "vddcamio"
#endif
#ifndef REGU_DEV0_CAMMOT
#define REGU_DEV0_CAMMOT                  "vddcammot"
#endif
#ifndef REGU_DEV1_CAMMOT
#define REGU_DEV1_CAMMOT                  "vddcammot"
#endif
#ifndef REGU_DEV2_CAMMOT
#define REGU_DEV2_CAMMOT                  "vddcammot"
#endif

const char *c_sensor_avdd_name[] = {
						REGU_DEV0_CAMAVDD,
						REGU_DEV1_CAMAVDD,
						REGU_DEV2_CAMAVDD};

const char *c_sensor_iovdd_name[] = {
						REGU_DEV0_CAMIOVDD,
						REGU_DEV1_CAMIOVDD,
						REGU_DEV2_CAMIOVDD};

const char *c_sensor_dvdd_name[] = {
						REGU_DEV0_CAMDVDD,
						REGU_DEV1_CAMDVDD,
						REGU_DEV2_CAMDVDD};

const char *c_sensor_mot_name[] = {
						REGU_DEV0_CAMMOT,
						REGU_DEV1_CAMMOT,
						REGU_DEV2_CAMMOT};

int get_regulator_name(struct device_node *dn, int *type, uint32_t sensor_id, char **name)
{
	const char *ldostr;
	int ret = 0;

	switch (*type) {
	case REGU_CAMAVDD:
		ret = of_property_read_string_index(dn, "vdds", sensor_id*4 + 1, &ldostr);
		if(!ret) {
			*name = ldostr;
			PARSE_TRACE("REGU_CAMAVDD = >%s\n",*name);
		} else {
			*name = (char*)c_sensor_avdd_name[sensor_id];
		}
		break;

	case REGU_CAMDVDD:
		ret = of_property_read_string_index(dn, "vdds", sensor_id*4 + 2, &ldostr);
		if(!ret) {
			*name = ldostr;
			PARSE_TRACE("REGU_CAMDVDD = >%s\n",*name);
		} else {
			*name = (char*)c_sensor_dvdd_name[sensor_id];
		}
		break;

	case REGU_CAMIOVDD:
		ret = of_property_read_string_index(dn, "vdds", sensor_id*4 + 3, &ldostr);
		if(!ret) {
			*name = ldostr;
			PARSE_TRACE("REGU_CAMIOVDD = >%s\n",*name);
		} else {
			*name = (char*)c_sensor_iovdd_name[sensor_id];
		}
		break;

	case REGU_CAMMOT:
		ret = of_property_read_string_index(dn, "vdds", sensor_id*4, &ldostr);
		if(!ret) {
			*name = ldostr;
			PARSE_TRACE("REGU_CAMMOT = >%s\n",*name);
		} else {
			*name = (char*)c_sensor_mot_name[sensor_id];
		}
		break;

	default:
		break;
	}

	if(strcmp(name, "vddcammot")==0) {
		*type = REGU_CAMMOT;
	} else if(strcmp(name, "vddcama")==0){
		*type = REGU_CAMAVDD;
	} else if(strcmp(name, "vddcamd")==0){
		*type = REGU_CAMDVDD;
	} else if(strcmp(name, "vddcamio")==0){
		*type = REGU_CAMIOVDD;
	}
	 of_node_put(dn);
	return 0;
}
