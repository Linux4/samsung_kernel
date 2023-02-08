/*
 * Samsung Mobile VE Group.
 *
 * drivers/video/dct/sec_dct_msm_a_family.c - Set display tunning data from MSM A family
 * (Interface 2.0)
 *
 * Copyright (C) 2014, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */


/*============ Fixed Code Area !===============*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/sec_dct.h>
/*========================================*/

#include "../msm/mdss/mdss_fb.h"

/****************************************************************/
/* Define this data in accordance with the specification of each BB platform		*/
// If you're in need, please do a request to VE group
//   for adding up the appropriate Lib files in to the application.
#define JNI_LIBRARY_NAME	"msm_a_family"

/****************************************************************/


/*============ Fixed Code Area !===============*/
static int init_alldata(void);
static void get_libname(char *name);

struct sec_dct_info_t dct_info = {
	.state = DCT_STATE_NODATA,
	.tunned = false,
	.enabled = false,
	.ref_addr = NULL,
	.get_libname = get_libname,
	.init_data = init_alldata,
};

#define DCT_INIT_DATA(NAME, TYPE, DATA)							\
	if((ret = dct_init_data((NAME), (TYPE), &(DATA), sizeof(DATA))) < 0) {	\
		goto error;												\
	}
/*========================================*/


/****************************************************************/
/* Define this function in accordance with the specification of each BB platform    */
/* If you need reference address to init data, set dct_info.ref_addr and use it.    */
static int init_alldata(void)
{
	int ret = 0;
	struct fb_info *info = (struct fb_info *)dct_info.ref_addr;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdss_panel_info *pinfo = mfd->panel_info;

	pr_info("[DCT][%s] ++\n", __func__);

	/************************************************/
	/* System variables mapping							*/
	/* This routine must be implemeted with JNI Library		*/
	/* Define order is very important!!					*/
	/************************************************/
	DCT_INIT_DATA("h_front_porch", DCT_TYPE_U32, pinfo->lcdc.h_front_porch);
	DCT_INIT_DATA("h_back_porch", DCT_TYPE_U32, pinfo->lcdc.h_back_porch);
	DCT_INIT_DATA("h_pulse_width", DCT_TYPE_U32, pinfo->lcdc.h_pulse_width);
	DCT_INIT_DATA("v_back_porch", DCT_TYPE_U32, pinfo->lcdc.v_back_porch);
	DCT_INIT_DATA("v_front_porch", DCT_TYPE_U32, pinfo->lcdc.v_front_porch);
	DCT_INIT_DATA("v_pulse_width", DCT_TYPE_U32, pinfo->lcdc.v_pulse_width);

	DCT_INIT_DATA("t_clk_post", DCT_TYPE_U8, pinfo->mipi.t_clk_post);
	DCT_INIT_DATA("t_clk_pre", DCT_TYPE_U8, pinfo->mipi.t_clk_pre);

	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_0", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[0]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_1", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[1]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_2", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[2]);
	//DCT_INIT_DATA("DSIPHY_TIMING_CTRL_3", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[3]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_4", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[4]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_5", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[5]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_6", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[6]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_7", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[7]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_8", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[8]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_9", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[9]);
	DCT_INIT_DATA("DSIPHY_TIMING_CTRL_10", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[10]);
	//DCT_INIT_DATA("DSIPHY_TIMING_CTRL_11", DCT_TYPE_U32, pinfo->mipi.dsi_phy_db.timing[11]);
	/************************************************/

error:
	pr_info("[DCT][%s] --(%d)\n", __func__, ret);

	return ret;
}
/****************************************************************/


/*============ Fixed Code Area !===============*/
static void get_libname(char *name)
{
	strcpy(name, JNI_LIBRARY_NAME);
	return;
}

static struct platform_device sec_dct_device = {
	.name	= "sec_dct",
	.id		= -1,
	.dev.platform_data = &dct_info,
};

static struct platform_device *sec_dct_devices[] __initdata = {
	&sec_dct_device,
};

static int __init sec_dct_device_init(void)
{
	return platform_add_devices(
		sec_dct_devices, ARRAY_SIZE(sec_dct_devices));
}
arch_initcall(sec_dct_device_init);
/*========================================*/


