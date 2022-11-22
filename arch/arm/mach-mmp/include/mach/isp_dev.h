/*
 * isp_dev.h
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


#ifndef ISP_DEV_H
#define ISP_DEV_H

#include <media/map_camera.h>
#include <media/mrvl-camera.h>
#include <linux/mvisp.h>

enum mvisp_interface_type {
	ISP_INTERFACE_PARALLEL_0,
	ISP_INTERFACE_PARALLEL_1,
	ISP_INTERFACE_CCIC_1,
	ISP_INTERFACE_CCIC_2,
};

enum {
	PCAM_IP_CCIC	= 0,
	PCAM_IP_DXO,
	PCAM_IP_SENSOR,
	PCAM_IP_CNT,
	PCAM_DEV_CCIC_END	= 2,
	PCAM_DEV_DXO_END	= 1,
};

int plat_agent_register(struct hw_agent *agent);
int plat_resrc_register(struct device *dev, struct resource *res,
	const char *name, union agent_id mask,
	int res_id, void *handle, void *priv);
int sensor_set_clock(struct hw_agent *agent, int rate);

struct mvisp_subdev_i2c_board_info {
	struct i2c_board_info *board_info;
	int i2c_adapter_id;
};

struct mvisp_v4l2_subdevs_group {
	char name[20];
	struct mvisp_subdev_i2c_board_info *i2c_board_info;
	enum mvisp_interface_type if_type;
};

struct mvisp_platform_data {
	struct mvisp_v4l2_subdevs_group *subdev_group;
	bool ccic_dummy_ena;
	bool ispdma_dummy_ena;
	unsigned int isp_clknum;
	unsigned int ccic_clknum;
	unsigned int isp_clk_rate_num;
	unsigned long isp_clk_rate[CLK_RATE_NUM];
	unsigned int ccic_clk_rate_num;
	unsigned long ccic_clk_rate[CLK_RATE_NUM];
	char **clkname;
	int (*init_pin)(struct device *dev, int on);
	int (*mvisp_reset)(void *param);
	int (*isp_pwr_ctrl)(int);
};

#ifdef CONFIG_CPU_MMP3
void __init mmp3_register_dxoisp(struct mvisp_platform_data *pdata);
int mmp3_isp_reset_hw(void *param);
int isppwr_power_control(int on);
#endif

#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
void __init pxa988_register_dxoisp(struct mvisp_platform_data *pdata);
int pxa988_isp_reset_hw(void *param);
int pxa988_isp_power_control(int on);
#endif

#endif
