/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_HDCP_H__
#define __SPRD_HDCP_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#include "disp_lib.h"

#define PMU_IPA_SYS_REG		0x649103E0
#define IPA_EB_REG			0x64900000
#define AON2IPA_EN_REG		0x64910CD8
#define CLOCK_EN_REG		0x31800000
#define HDCP_BASE_REG		0x31900000
#define DPTX_HDCP_CFG_REG	0x31800e00

#define HDCP_REG_SIZE		0x4
#define HDCP_REG_NUM		0x12

#define HL_DRIVER_ALLOCATE_DYNAMIC_MEM 	0xffffffff

#define HL_DRV_IOC_HDCP_ENABLE   		0x13
#define HL_DRV_IOC_ENABLE     			0x1
#define HL_DRV_IOC_DISABLE    			0

#define HL_DRV_IOC_READ_HPI 		_IOWR('H', HL_DRV_NR_READ_HPI, struct hl_drv_ioc_hpi_reg)
#define HL_DRV_IOC_WRITE_HPI		_IOW('H', HL_DRV_NR_WRITE_HPI, struct hl_drv_ioc_hpi_reg)
#define HL_DRV_IOC_INIT    			_IOW('H', HL_DRV_NR_INIT, struct sprd_hdcp_ioc_meminfo)
#define HL_DRV_IOC_MEMINFO 			_IOR('H', HL_DRV_NR_MEMINFO, struct sprd_hdcp_ioc_meminfo)
#define HL_DRV_IOC_LOAD_CODE 		_IOW('H', HL_DRV_NR_LOAD_CODE, struct hl_drv_ioc_code)
#define HL_DRV_IOC_READ_DATA  		_IOWR('H', HL_DRV_NR_READ_DATA, struct hl_drv_ioc_data)
#define HL_DRV_IOC_WRITE_DATA  		_IOW('H', HL_DRV_NR_WRITE_DATA, struct hl_drv_ioc_data)
#define HL_DRV_IOC_MEMSET_DATA 		_IOW('H', HL_DRV_NR_MEMSET_DATA, struct hl_drv_ioc_data)

#define HL_DRIVER_SUCCESS              		0
#define HL_DRIVER_FAILED               		(-1)
#define HL_DRIVER_NO_MEMORY            		(-2)
#define HL_DRIVER_NO_ACCESS            		(-3)
#define HL_DRIVER_INVALID_PARAM        		(-4)
#define HL_DRIVER_TOO_MANY_ESM_DEVICES 		(-5)
#define HL_DRIVER_USER_DEFINED_ERROR   		(-6) // anything beyond this error code is user defined

enum {
	HL_DRV_NR_MIN = 0x10,
	HL_DRV_NR_INIT,
	HL_DRV_NR_MEMINFO,
	HL_DRV_NR_LOAD_CODE,
	HL_DRV_NR_READ_DATA,
	HL_DRV_NR_WRITE_DATA,
	HL_DRV_NR_MEMSET_DATA,
	HL_DRV_NR_READ_HPI,
	HL_DRV_NR_WRITE_HPI,
	HL_DRV_NR_MAX
};

struct hdcp_mem {
	void __iomem *pmu_ipa_sys_addr;
	void __iomem *ipa_eb_addr;
	void __iomem *aon2ipa_en_addr;
	void __iomem *clk_en_addr;
	void __iomem *hdcp_base_addr;
	void __iomem *dptx_hdcp_cfg_addr;
};

struct sprd_hdcp {
	int32_t allocated, initialized;
	int32_t code_loaded;

	uint32_t code_is_phys_mem;
	dma_addr_t code_base;
	uint32_t code_size;
	uint8_t *code;
	uint32_t data_is_phys_mem;
	dma_addr_t data_base;
	uint32_t data_size;
	uint8_t *data;

	struct resource *hpi_resource;
	uint8_t __iomem *hpi;
	uint32_t        *code_offset;
	uint8_t         *data_offset;
};

struct sprd_hdcp_ioc_meminfo {
   __u32 hpi_base;
   __u32 code_base;
   __u32 code_size;
   __u32 data_base;
   __u32 data_size;
};

struct hl_drv_ioc_code {
   __u32 len;
   __u8 data[];
};

struct hl_drv_ioc_data {
   __u32 offset;
   __u32 len;
   __u8 data[];
};

struct hl_drv_ioc_hpi_reg {
   __u32 offset;
   __u32 value;
};

extern int hdcp13_enabled;

struct dptx *dptx_get_handle(void);
int dptx_configure_hdcp13(void);

#endif /* __SPRD_HDCP_H__ */
