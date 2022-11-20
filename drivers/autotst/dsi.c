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

#include <linux/kgdb.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

#include "lcd_dummy.h"

#include "mipi_dsih_local.h"
#include "mipi_dsih_dphy.h"
#include "mipi_dsih_hal.h"
#include "mipi_dsih_api.h"

#define pr_debug printk


#define DSI_PHY_REF_CLOCK (26*1000)
#define DSI_EDPI_CFG (0x6c)

#ifdef CONFIG_FB_SCX15
#define DISPC_DPI_CLOCK 			(192*1000000/18)
#else
#define DISPC_DPI_CLOCK 			(384*1000000/36)
#endif

#define SPRD_MIPI_DSIC_BASE 		SPRD_DSI_BASE
#define	IRQ_DSI_INTN0			IRQ_DSI0_INT
#define	IRQ_DSI_INTN1			IRQ_DSI1_INT

#define DSI_REG_EB				REG_AP_AHB_AHB_EB
#define DSI_BIT_EB					BIT_DSI_EB

#define DSI_AHB_SOFT_RST           		REG_AP_AHB_AHB_RST

struct autotst_dsi_context {
	struct clk		*clk_dsi;
	bool			is_inited;
	uint32_t		status;/*0- normal, 1- uninit, 2-abnormal*/
	dsih_ctrl_t	dsi_inst;
};

static struct autotst_dsi_context autotst_dsi_ctx;

static uint32_t dsi_core_read_function(uint32_t addr, uint32_t offset)
{
	return sci_glb_read(addr + offset, 0xffffffff);
}

static void dsi_core_write_function(uint32_t addr, uint32_t offset, uint32_t data)
{
	sci_glb_write((addr + offset), data, 0xffffffff);
}

#if 0
static irqreturn_t dsi_isr0(int irq, void *data)
{
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST0);
	printk(KERN_ERR "autotst_dsi: [%s](0x%x)!\n", __FUNCTION__, reg_val);
	return IRQ_HANDLED;
}

static irqreturn_t dsi_isr1(int irq, void *data)
{
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST1);
	printk(KERN_ERR "autotst_dsi: [%s](0x%x)!\n", __FUNCTION__, reg_val);
	return IRQ_HANDLED;
}
#endif

static int32_t dsi_edpi_setbuswidth(struct info_mipi * mipi)
{
	dsih_color_coding_t color_coding = 0;

	switch(mipi->video_bus_width){
	case 16:
		color_coding = COLOR_CODE_16BIT_CONFIG1;
		break;
	case 18:
		color_coding = COLOR_CODE_18BIT_CONFIG1;
		break;
	case 24:
		color_coding = COLOR_CODE_24BIT;
		break;
	default:
		printk(KERN_ERR "autotst_dsi:[%s] fail, invalid video_bus_width\n", __FUNCTION__);
		return 0;
	}

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_DPI_CFG, ((uint32_t)color_coding<<2));
	return 0;
}


static int32_t dsi_edpi_init(void)
{
	dsi_core_write_function((uint32_t)SPRD_MIPI_DSIC_BASE,  (uint32_t)DSI_EDPI_CFG, 0x10500);
	return 0;
}

static int32_t dsi_dpi_init(struct panel_spec* panel)
{
	dsih_dpi_video_t dpi_param;
	dsih_error_t result;
	struct info_mipi * mipi = panel->info.mipi;

	dpi_param.no_of_lanes = mipi->lan_number;
	dpi_param.byte_clock = mipi->phy_feq / 8;
	dpi_param.pixel_clock = DISPC_DPI_CLOCK / 1000;

	switch(mipi->video_bus_width){
	case 16:
		dpi_param.color_coding = COLOR_CODE_16BIT_CONFIG1;
		break;
	case 18:
		dpi_param.color_coding = COLOR_CODE_18BIT_CONFIG1;
		break;
	case 24:
		dpi_param.color_coding = COLOR_CODE_24BIT;
		break;
	default:
		printk(KERN_ERR "autotst_dsi:[%s] fail, invalid video_bus_width\n", __FUNCTION__);
		break;
	}

	if(SPRDFB_POLARITY_POS == mipi ->h_sync_pol){
		dpi_param.h_polarity = 1;
	}

	if(SPRDFB_POLARITY_POS == mipi ->v_sync_pol){
		dpi_param.v_polarity = 1;
	}

	if(SPRDFB_POLARITY_POS == mipi ->de_pol){
		dpi_param.data_en_polarity = 1;
	}

	dpi_param.h_active_pixels = panel->width;
	dpi_param.h_sync_pixels = mipi->timing->hsync;
	dpi_param.h_back_porch_pixels = mipi->timing->hbp;
	dpi_param.h_total_pixels = panel->width + mipi->timing->hsync + mipi->timing->hbp + mipi->timing->hfp;

	dpi_param.v_active_lines = panel->height;
	dpi_param.v_sync_lines = mipi->timing->vsync;
	dpi_param.v_back_porch_lines = mipi->timing->vbp;
	dpi_param.v_total_lines = panel->height + mipi->timing->vsync + mipi->timing->vbp + mipi->timing->vfp;

	dpi_param.receive_ack_packets = 0;
	dpi_param.video_mode = VIDEO_BURST_WITH_SYNC_PULSES;
	dpi_param.virtual_channel = 0;
	dpi_param.is_18_loosely = 0;

	result = mipi_dsih_dpi_video(&(autotst_dsi_ctx.dsi_inst), &dpi_param);
	if(result != OK){
		printk(KERN_ERR "autotst_dsi: [%s] mipi_dsih_dpi_video fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	return 0;
}

static void dsi_log_error(const char * string)
{
	printk(string);
}

static int32_t dsi_module_init(struct panel_spec *panel)
{
	int ret = 0;
	dsih_ctrl_t* dsi_instance = &(autotst_dsi_ctx.dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi * mipi = panel->info.mipi;

	pr_debug(KERN_INFO "autotst_dsi:[%s]\n", __FUNCTION__);

	if(autotst_dsi_ctx.is_inited){
		printk(KERN_INFO "autotst_dsi: dsi_module_init. is_inited==true!");
		return 0;
	}
	else{
		printk(KERN_INFO "autotst_dsi: dsi_module_init. call only once!\n");
	}

	phy->address = SPRD_MIPI_DSIC_BASE;
	phy->core_read_function = dsi_core_read_function;
	phy->core_write_function = dsi_core_write_function;
	phy->log_error = dsi_log_error;
	phy->log_info = NULL;
	phy->reference_freq = DSI_PHY_REF_CLOCK;

	dsi_instance->address = SPRD_MIPI_DSIC_BASE;
	dsi_instance->color_mode_polarity =mipi->color_mode_pol;
	dsi_instance->shut_down_polarity = mipi->shut_down_pol;
	dsi_instance->core_read_function = dsi_core_read_function;
	dsi_instance->core_write_function = dsi_core_write_function;
	dsi_instance->log_error = dsi_log_error;
	dsi_instance->log_info = NULL;
	 /*in our rtl implementation, this is max rd time, not bta time and use 15bits*/
	dsi_instance->max_bta_cycles = 0x6000;//10;
	dsi_instance->max_hs_to_lp_cycles = 4;//110;
	dsi_instance->max_lp_to_hs_cycles = 15;//10;
	dsi_instance->max_lanes = mipi->lan_number;
#if 0
	ret = request_irq(IRQ_DSI_INTN0, dsi_isr0, IRQF_DISABLED, "DSI_INT0", &autotst_dsi_ctx);
	if (ret) {
		printk(KERN_ERR "autotst_dsi: dsi failed to request irq int0!\n");
	}else{
		printk(KERN_ERR "autotst_dsi: dsi request irq int0 OK!\n");
	}

	ret = request_irq(IRQ_DSI_INTN1, dsi_isr1, IRQF_DISABLED, "DSI_INT1", &autotst_dsi_ctx);
	if (ret) {
		printk(KERN_ERR "autotst_dsi: dsi failed to request irq int1!\n");
	}else{
		printk(KERN_ERR "autotst_dsi: dsi request irq int1 OK!\n");
	}
#endif
	autotst_dsi_ctx.is_inited = true;

	return 0;
}

static int32_t dsih_init(struct panel_spec *panel)
{
	dsih_error_t result = OK;
	dsih_ctrl_t* dsi_instance = &(autotst_dsi_ctx.dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi * mipi = panel->info.mipi;
	int i = 0;

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK0, 0x1fffff);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK1, 0x3ffff);

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		dsi_edpi_init();
	}

	dsi_instance->phy_feq = panel->info.mipi->phy_feq;
	result = mipi_dsih_open(dsi_instance);
	if(OK != result){
		printk(KERN_ERR "autotst_dsi: [%s]: mipi_dsih_open fail (%d)!\n", __FUNCTION__, result);
		autotst_dsi_ctx.status = 1;
		return -1;
	}

	result = mipi_dsih_dphy_configure(phy,  mipi->lan_number, mipi->phy_feq);
	if(OK != result){
		printk(KERN_ERR "autotst_dsi: [%s]: mipi_dsih_dphy_configure fail (%d)!\n", __FUNCTION__, result);
		autotst_dsi_ctx.status = 1;
		return -1;
	}

	while(5 != (dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_STATUS) & 5)){
		if(0x0 == ++i%500000){
			printk("autotst_dsi: [%s] warning: busy waiting!\n", __FUNCTION__);
		}
	}

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		dsi_edpi_setbuswidth(mipi);
	}

	result = mipi_dsih_enable_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "autotst_dsi: [%s]: mipi_dsih_enable_rx fail (%d)!\n", __FUNCTION__, result);
		autotst_dsi_ctx.status = 1;
		return -1;
	}

	result = mipi_dsih_ecc_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "autotst_dsi: [%s]: mipi_dsih_ecc_rx fail (%d)!\n", __FUNCTION__, result);
		autotst_dsi_ctx.status = 1;
		return -1;
	}

	result = mipi_dsih_eotp_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "autotst_dsi: [%s]: mipi_dsih_eotp_rx fail (%d)!\n", __FUNCTION__, result);
		autotst_dsi_ctx.status = 1;
		return -1;
	}

	result = mipi_dsih_eotp_tx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "autotst_dsi: [%s]: mipi_dsih_eotp_tx fail (%d)!\n", __FUNCTION__, result);
		autotst_dsi_ctx.status = 1;
		return -1;
	}

	if(SPRDFB_MIPI_MODE_VIDEO == mipi->work_mode){
		dsi_dpi_init(panel);
	}

	autotst_dsi_ctx.status = 0;

	return 0;
}

static void dsi_enable(void)
{
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_REF_CKG_EN);
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_CFG_CKG_EN);
	sci_glb_set(DSI_REG_EB, DSI_BIT_EB);
}

static void dsi_disable(void)
{
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_REF_CKG_EN);
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_CFG_CKG_EN);
	sci_glb_clr(DSI_REG_EB, DSI_BIT_EB);
}

static void dsi_reset(void)
{
	sci_glb_set(DSI_AHB_SOFT_RST, BIT_DSI_SOFT_RST);
        udelay(10);
	sci_glb_clr(DSI_AHB_SOFT_RST, BIT_DSI_SOFT_RST);
}

static int32_t dsi_ready(struct panel_spec *panel)
{
	struct info_mipi * mipi = panel->info.mipi;

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		mipi_dsih_cmd_mode(&(autotst_dsi_ctx.dsi_inst), 1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0x1);
	}else{
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0x1);
		mipi_dsih_video_mode(&(autotst_dsi_ctx.dsi_inst), 1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PWR_UP, 0);
		udelay(100);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PWR_UP, 1);
		mdelay(3);
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST0);
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST1);
	}

	//dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK0, 0x0);
	//dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK1, 0x800);

	return 0;
}

void autotst_dsi_dump(void)
{
    int i = 0;
    for(i=0;i<256;i+=16){
        printk("autotst_dsi: %x: 0x%x, 0x%x, 0x%x, 0x%x\n", i, dsi_core_read_function(SPRD_MIPI_DSIC_BASE, i),
            dsi_core_read_function(SPRD_MIPI_DSIC_BASE, i+4),
            dsi_core_read_function(SPRD_MIPI_DSIC_BASE, i+8),
            dsi_core_read_function(SPRD_MIPI_DSIC_BASE, i+12));
    }
    printk("**************************\n");
}

void autotst_dsi_dump1(void)
{
    int i = 0;
    for(i=0;i<50;i++){
        printk("autotst_dsi: 0x%x\n", dsi_core_read_function(SPRD_MIPI_DSIC_BASE, 0x60));
        udelay(5);
    }
    printk("**************************\n");
}

int32_t autotst_dsi_init(struct panel_spec *panel)
{
	dsih_error_t result = OK;

	printk(KERN_INFO "autotst_dsi:[%s]\n", __FUNCTION__);
	dsi_enable();
	dsi_reset();
	dsi_module_init(panel);
	result=dsih_init(panel);
	dsi_ready(panel);

	return result;
}

int32_t autotst_dsi_uninit(void)
{
	dsih_error_t result;
	dsih_ctrl_t* dsi_instance = &(autotst_dsi_ctx.dsi_inst);

	printk(KERN_INFO "autotst_dsi: [%s]\n",__FUNCTION__);

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0);
	result = mipi_dsih_close(&(autotst_dsi_ctx.dsi_inst));
	dsi_instance->status = NOT_INITIALIZED;

	autotst_dsi_ctx.status = 1;

	if(OK != result){
		printk(KERN_ERR "autotst_dsi: [%s] fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	mdelay(10);
	dsi_disable();
	return 0;
}
