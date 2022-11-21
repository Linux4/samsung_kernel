/*
 * isp.h
 *
 * Marvell DxO ISP - Top level module
 *	Based on omap3isp
 *
 * Copyright:  (C) Copyright 2011 Marvell International Ltd.
 *              Henry Zhao <xzhao10@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */


#ifndef ISP_CORE_H
#define ISP_CORE_H

#include <media/v4l2-device.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/wait.h>

#include <mach/isp_dev.h>

#include "ispccic.h"
#include "ispdma.h"

/* Convert from isp modules to isp device */
#define to_mvisp_device(ptr_module) \
	container_of(ptr_module, struct mvisp_device, mvisp_##ptr_module)
/* Convert from isp device to v4l2 video device*/
#define to_device(ptr_module) \
	(to_mvisp_device(ptr_module)->dev)
/* Convert from v4l2 device to isp device*/
#define v4l2_dev_to_mvisp_device(dev) \
	container_of(dev, struct mvisp_device, v4l2_dev)

struct pad_formats {
	enum v4l2_mbus_pixelcode mbusfmt;
	enum v4l2_colorspace colorspace;
};

enum mvisp_mem_resources {
	ISP_IOMEM_ISPDMA = 0,
	CCIC_ISP_IOMEM_1,
	CCIC_ISP_IOMEM_2,
	ISP_IOMEM_LAST,
};

enum mvisp_cpu_type {
	MV_MMP3 = 0,
	MV_PXA988,
};

struct mvisp_device {
	struct v4l2_device	v4l2_dev;
	struct media_device	media_dev;
	struct device		*dev;

	/* platform HW resources */
	struct mvisp_platform_data *pdata;
	enum mvisp_cpu_type     cpu_type;

	/* flags */
	bool	has_context;

	/* interrupts within isp */
	unsigned int irq_ipc;
	unsigned int irq_dma;
	unsigned int irq_ccic1;

	/* register and clock resources */
	void __iomem *mmio_base[ISP_IOMEM_LAST];
	unsigned long mmio_base_phys[ISP_IOMEM_LAST];
	resource_size_t mmio_size[ISP_IOMEM_LAST];

	/* dummy buffer for all isp */
	bool			ccic_dummy_ena;
	bool			ispdma_dummy_ena;
	void			*dummy_vaddr;
	struct page		*dummy_pages;
	int				dummy_order;
	dma_addr_t		dummy_paddr;

	/* ISP Obj */
	struct mutex	mvisp_mutex;	/* For handling ref_count field */
	int				ref_count;

	/* ISP modules */
	struct isp_ispdma_device	mvisp_ispdma;
	struct isp_ccic_device		mvisp_ccic1;
	struct isp_ccic_device		mvisp_ccic2;

	struct v4l2_subdev			*sensor;
	bool sensor_connected;
	enum mv_isp_sensor_type sensor_type;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	struct mvisp_mcd	mcd_root_display;
	struct mvisp_mcd	mcd_root_codec;
#endif
	int (*isp_pwr_ctrl)(int);
	unsigned int isp_clknum;
	unsigned int ccic_clknum;
	struct clk   *clock[0];
};


int mvisp_pipeline_set_stream(struct isp_pipeline *pipe,
				 enum isp_pipeline_stream_state state);

struct mvisp_device *mvisp_get(struct mvisp_device *isp);
void mvisp_put(struct mvisp_device *isp);

int mvisp_pipeline_pm_use(struct media_entity *entity, int use);
/*
int mvisp_register_entities(struct platform_device *pdev,
			       struct v4l2_device *v4l2_dev);
void mvisp_unregister_entities(struct platform_device *pdev);
*/
/*
 * mvisp_reg_readl - Read value of an OMAP3 ISP register
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @reg_offset: Register offset to read from.
 *
 * Returns an unsigned 32 bit value with the required register contents.
 */
static inline
u32 mvisp_reg_readl(struct mvisp_device *isp,
		enum mvisp_mem_resources mvisp_mmio_range,
		u32 reg_offset)
{
	return readl(isp->mmio_base[mvisp_mmio_range] + reg_offset);
}


/*
 * mvisp_reg_writel - Write value to an OMAP3 ISP register
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @reg_value: 32 bit value to write to the register.
 * @reg_offset: Register offset to write into.
 */
static inline
void mvisp_reg_writel(struct mvisp_device *isp, u32 reg_value,
			enum mvisp_mem_resources mvisp_mmio_range,
			u32 reg_offset)
{
	writel(reg_value, isp->mmio_base[mvisp_mmio_range] + reg_offset);
}

#endif	/* ISP_CORE_H */
