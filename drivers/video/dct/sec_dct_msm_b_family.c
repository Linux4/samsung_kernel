/*
 * Samsung Mobile VE Group.
 *
 * drivers/video/dct/sec_dct_msm_b_family.c - Set display tunning data from MSM B family
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
#define JNI_LIBRARY_NAME	"msm_b_family"

struct sec_dct_data_t {
	u32 h_front_porch;
	u32 h_back_porch;
	u32 h_pulse_width;
	u32 v_back_porch;
	u32 v_front_porch;
	u32 v_pulse_width;

	u8 t_clk_post;
	u8 t_clk_pre;

	u32 disphy_timing_ctrl[12];
};
/****************************************************************/


/*============ Fixed Code Area !===============*/
struct sec_dct_data_t dct_data;
struct sec_dct_data_t dct_data_org;
static bool bdct_data_backup = false;

static int dct_setAllData(char *recv_data);
void dct_applyData(struct fb_info *info);
void dct_finish_applyData(void);
void dct_getlibname(char *name);

struct sec_dct_info_t dct_info = {
	.data = &dct_data,
	.state = DCT_STATE_NODATA,
	.tunned = false,
	.enabled = false,
	.log = NULL,
	.set_allData = dct_setAllData,
	.applyData = dct_applyData,
	.finish_applyData = dct_finish_applyData,
	.get_libname = dct_getlibname,
};

#define DCT_SETDATA(name, type, data_addr, data_size)				\
	if((ret = dct_setData(name, type, data_addr, data_size))) {		\
		goto error;											\
	}
/*========================================*/


/****************************************************************/
/* Define this function in accordance with the specification of each BB platform      */
static int dct_setAllData(char *recv_data)
{
	int ret = 0;

	pr_info("[DCT][%s] ++\n", __func__);

	/****************************************************************/
	/* Define this area! Order is very important!!							*/
	/* ex) DCT_SETDATA(variable name string, variable type, variable address, variable data size) */

	DCT_SETDATA("h_front_porch", DCT_TYPE_U32, &dct_data.h_front_porch, sizeof(dct_data.h_front_porch));
	DCT_SETDATA("h_back_porch", DCT_TYPE_U32, &dct_data.h_back_porch, sizeof(dct_data.h_back_porch));
	DCT_SETDATA("h_pulse_width", DCT_TYPE_U32, &dct_data.h_pulse_width, sizeof(dct_data.h_pulse_width));
	DCT_SETDATA("v_back_porch", DCT_TYPE_U32, &dct_data.v_back_porch, sizeof(dct_data.v_back_porch));
	DCT_SETDATA("v_front_porch", DCT_TYPE_U32, &dct_data.v_front_porch, sizeof(dct_data.v_front_porch));
	DCT_SETDATA("v_pulse_width", DCT_TYPE_U32, &dct_data.v_pulse_width, sizeof(dct_data.v_pulse_width));

	DCT_SETDATA("t_clk_post", DCT_TYPE_U8, &dct_data.t_clk_post, sizeof(dct_data.t_clk_post));	
	DCT_SETDATA("t_clk_pre", DCT_TYPE_U8, &dct_data.t_clk_pre, sizeof(dct_data.t_clk_pre));

	DCT_SETDATA("DSIPHY_TIMING_CTRL_0", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[0], sizeof(dct_data.disphy_timing_ctrl[0]));	
	DCT_SETDATA("DSIPHY_TIMING_CTRL_1", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[1], sizeof(dct_data.disphy_timing_ctrl[1]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_2", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[2], sizeof(dct_data.disphy_timing_ctrl[2]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_3", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[3], sizeof(dct_data.disphy_timing_ctrl[3]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_4", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[4], sizeof(dct_data.disphy_timing_ctrl[4]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_5", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[5], sizeof(dct_data.disphy_timing_ctrl[5]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_6", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[6], sizeof(dct_data.disphy_timing_ctrl[6]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_7", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[7], sizeof(dct_data.disphy_timing_ctrl[7]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_8", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[8], sizeof(dct_data.disphy_timing_ctrl[8]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_9", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[9], sizeof(dct_data.disphy_timing_ctrl[9]));
	DCT_SETDATA("DSIPHY_TIMING_CTRL_10", DCT_TYPE_U32, &dct_data.disphy_timing_ctrl[10], sizeof(dct_data.disphy_timing_ctrl[10]));
	//dct_data.disphy_timing_ctrl[11] = 0;

	/****************************************************************/

error:
	pr_info("[DCT][%s] --(%d)\n", __func__, ret);

	return ret;
}
/****************************************************************/



/****************************************************************/
/* Define this function in accordance with the specification of each BB platform      */
void dct_applyData(struct fb_info *info)
{
	bool bsetting = false;

	int i = 0;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdss_panel_info *pinfo = mfd->panel_info;

	DCT_LOG("[DCT][%s] ++\n", __func__);

	if (!bdct_data_backup) {
		/************************************************/
		/* Original Data Backup								*/
		/* This routine must be implemeted					*/
		/************************************************/
		dct_data_org.h_front_porch = pinfo->lcdc.h_front_porch;
		dct_data_org.h_back_porch = pinfo->lcdc.h_back_porch;
		dct_data_org.h_pulse_width = pinfo->lcdc.h_pulse_width;
		dct_data_org.v_back_porch = pinfo->lcdc.v_back_porch;
		dct_data_org.v_front_porch = pinfo->lcdc.v_front_porch;
		dct_data_org.v_pulse_width = pinfo->lcdc.v_pulse_width;

		dct_data_org.t_clk_post = pinfo->mipi.t_clk_post;
		dct_data_org.t_clk_pre = pinfo->mipi.t_clk_pre;

		for (i=0; i < 12; i++) {
			dct_data_org.disphy_timing_ctrl[i] = pinfo->mipi.dsi_phy_db.timing[i];
		}

		dct_data.disphy_timing_ctrl[11] = pinfo->mipi.dsi_phy_db.timing[11];
		/************************************************/

		bdct_data_backup = true;		// important !
	}
	if (dct_info.state == DCT_STATE_DATA) {
		/************************************************/
		/* Apply The App data (User Customizing Data)			*/
		/* This routine must be implemeted					*/
		/************************************************/
		pinfo->lcdc.h_front_porch = dct_data.h_front_porch;
		pinfo->lcdc.h_back_porch = dct_data.h_back_porch;
		pinfo->lcdc.h_pulse_width = dct_data.h_pulse_width;
		pinfo->lcdc.v_back_porch = dct_data.v_back_porch;
		pinfo->lcdc.v_front_porch = dct_data.v_front_porch;
		pinfo->lcdc.v_pulse_width = dct_data.v_pulse_width;

		pinfo->mipi.t_clk_post = dct_data.t_clk_post;
		pinfo->mipi.t_clk_pre = dct_data.t_clk_pre;

		for (i=0; i < 12; i++) {
			pinfo->mipi.dsi_phy_db.timing[i] = dct_data.disphy_timing_ctrl[i];
		}
		/************************************************/

		dct_info.tunned = true;
		bsetting = true;
		pr_info("[DCT][%s] set data of dct application!\n", __func__);
	}
	else if (dct_info.state == DCT_STATE_NODATA && dct_info.tunned) {
		/************************************************/
		/* Apply Backuped Original Kernel data					*/
		/* This routine must be implemeted					*/
		/************************************************/
		pinfo->lcdc.h_front_porch = dct_data_org.h_front_porch;
		pinfo->lcdc.h_back_porch = dct_data_org.h_back_porch;
		pinfo->lcdc.h_pulse_width = dct_data_org.h_pulse_width;
		pinfo->lcdc.v_back_porch = dct_data_org.v_back_porch;
		pinfo->lcdc.v_front_porch = dct_data_org.v_front_porch;
		pinfo->lcdc.v_pulse_width = dct_data_org.v_pulse_width;

		pinfo->mipi.t_clk_post = dct_data_org.t_clk_post;
		pinfo->mipi.t_clk_pre = dct_data_org.t_clk_pre;

		for (i=0; i < 12; i++) {
			pinfo->mipi.dsi_phy_db.timing[i] = dct_data_org.disphy_timing_ctrl[i];
		}
		/************************************************/

		dct_info.tunned = false;
		bsetting = true;
		pr_info("[DCT][%s] set original data!\n", __func__);
	}
	else { 	// nothing
		;
	}
	/************************************************/

	if (bsetting) {
		/************************************************/
		/* Print Set Kernel data (Optional)					*/
		/************************************************/		
		pr_info("[DCT] hfp=%d, hbp=%d, hpw=%d, vbp=%d, vfp=%d, vpw=%d\n",
			pinfo->lcdc.h_front_porch,
			pinfo->lcdc.h_back_porch,
			pinfo->lcdc.h_pulse_width,
			pinfo->lcdc.v_back_porch,
			pinfo->lcdc.v_front_porch,
			pinfo->lcdc.v_pulse_width);
		
		pr_info("[DCT] t_clk_post=0x%02X, t_clk_pre=0x%02X\n",
			pinfo->mipi.t_clk_post,
			pinfo->mipi.t_clk_pre);

		pr_info("[DCT] dsi_phy_db.timing = \n");
		pr_info("[DCT] 	%02X %02X %02X %02X (%d %d %d %d)\n",
			pinfo->mipi.dsi_phy_db.timing[0], pinfo->mipi.dsi_phy_db.timing[1],
			pinfo->mipi.dsi_phy_db.timing[2], pinfo->mipi.dsi_phy_db.timing[3],
			pinfo->mipi.dsi_phy_db.timing[0], pinfo->mipi.dsi_phy_db.timing[1],
			pinfo->mipi.dsi_phy_db.timing[2], pinfo->mipi.dsi_phy_db.timing[3]);
		pr_info("[DCT] 	%02X %02X %02X %02X (%d %d %d %d)\n",
			pinfo->mipi.dsi_phy_db.timing[4], pinfo->mipi.dsi_phy_db.timing[5],
			pinfo->mipi.dsi_phy_db.timing[6], pinfo->mipi.dsi_phy_db.timing[7],
			pinfo->mipi.dsi_phy_db.timing[4], pinfo->mipi.dsi_phy_db.timing[5],
			pinfo->mipi.dsi_phy_db.timing[6], pinfo->mipi.dsi_phy_db.timing[7]);
		pr_info("[DCT] 	%02X %02X %02X %02X (%d %d %d %d)\n",
			pinfo->mipi.dsi_phy_db.timing[8], pinfo->mipi.dsi_phy_db.timing[9],
			pinfo->mipi.dsi_phy_db.timing[10], pinfo->mipi.dsi_phy_db.timing[11],
			pinfo->mipi.dsi_phy_db.timing[8], pinfo->mipi.dsi_phy_db.timing[9],
			pinfo->mipi.dsi_phy_db.timing[10], pinfo->mipi.dsi_phy_db.timing[11]);
		/************************************************/
	}

	return;
}
/****************************************************************/


/*============ Fixed Code Area !===============*/
void dct_finish_applyData(void)
{
	pr_info("[DCT][%s] +-\n", __func__);

	if (dct_info.state == DCT_STATE_DATA)
		dct_info.state = DCT_STATE_APPLYED;
	return;
}

void dct_getlibname(char *name)
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


