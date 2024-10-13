/*
 *Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#include <linux/kgdb.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "sprd_chip_common.h"
#include "sprd_display.h"
#include "sprd_interface.h"

#define DSI_PHY_REF_CLOCK (26*1000)

#define DSI_EDPI_CFG (0x6c)
#define SPRD_MIPI_DSIC_BASE (dsi_ctx->dsi_inst.address)

#define DSI_INSTANCE(p) (&((struct sprd_dsi_context *)(p))->dsi_inst)
unsigned long g_dsi_base_addr;
EXPORT_SYMBOL(g_dsi_base_addr);

static int32_t sprd_dsi_set_lp_mode(struct sprd_dsi_context *);
static int32_t sprd_dsi_set_ulps_mode(struct sprd_dsi_context *);

static uint32_t dsi_core_read_function(unsigned long addr, uint32_t offset)
{
	return __raw_readl((const void __iomem *)(addr + offset));
}

static void dsi_core_write_function(unsigned long addr, uint32_t offset,
	uint32_t data)
{
	/* __raw_writel(data, addr + offset); */
	__raw_writel(data, (void __iomem *)(addr + offset));
}

void dsi_core_or_function(unsigned long addr, unsigned int data)
{
	__raw_writel((__raw_readl((const void __iomem *)addr) | data),
		(void __iomem *)addr);
}

void dsi_core_and_function(unsigned long addr, unsigned int data)
{
	__raw_writel((__raw_readl((const void __iomem *)addr) & data),
		(void __iomem *) addr);
}

void dsi_irq_trick(struct sprd_dsi_context *dsi_ctx, int bit, int val)
{
	/*enable ack_with_err_0 interrupt */
	/* DSI_INT_MASK0_SET(0, 0); */
	uint32_t reg_val =
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_INT_MSK1);
	reg_val =
		(val ==
		1) ? (reg_val | (1UL << bit)) : (reg_val & (~(1UL << bit)));
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK1,
		reg_val);
}

void dispc_set_dsi_irq(struct panel_if_device *intf)
{
	/* disable irq , payload err*/
	PRINT("disable irq mask1 bit7\n");
	dsi_irq_trick(intf->info.mipi->dsi_ctx, 7, 0);
}

/* static uint32_t sot_ever_happened = 0; */
static irqreturn_t dsi_isr0(int irq, void *data)
{
	struct sprd_dsi_context *dsi_ctx =
		(struct sprd_dsi_context *)data;

	uint32_t reg_val =
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_INT_ST0);
	pr_debug("sprd: [%s](0x%x)!\n", __func__, reg_val);
	/*
	 *printk("Warning: sot_ever_happened:(0x%x)!\n",sot_ever_happened);
	 */
	if (reg_val & 0x1) {
		uint32_t mask_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE,
						R_DSI_HOST_INT_MSK0);
		mask_val &= (~BIT(0));
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK0
					, mask_val);
	}
	/* dsi_irq_trick(0,reg_val); */
	return IRQ_HANDLED;
}

static irqreturn_t dsi_isr1(int irq, void *data)
{
	uint32_t i = 0;
	struct sprd_dsi_context *dsi_ctx =
		(struct sprd_dsi_context *)data;
	dsih_ctrl_t *dsi_instance = &(dsi_ctx->dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi *mipi = dsi_ctx->intf->info.mipi;
	struct panel_spec *panel = mipi->panel;

	uint32_t reg_val =
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_INT_ST1);
	pr_debug("(0x%x)!\n", reg_val);

	if (BIT(7) == (reg_val & BIT(7))) {
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_PWR_UP, 0);
		/*need delay 1us */
		PERROR(" reset dsi host!\n");
		PERROR(" mipi->lan_number:%d ,mipi->phy_feq:%d\n",
			panel->lan_number, panel->phy_feq);
		if (NULL == phy) {
			PERROR(" the phy is null\n");
			/* dsi_irq_trick(1,reg_val); */
			return IRQ_NONE;
		}

		mipi_dsih_dphy_configure(phy, panel->lan_number,
					 panel->phy_feq);

#if 0
		{	/*for debug */
			for (i = 0; i < 256; i += 16) {
				PERROR(" %x: 0x%x, 0x%x, 0x%x, 0x%x\n", i,
				       dsi_core_read_function
				       (SPRD_MIPI_DSIC_BASE, i),
				       dsi_core_read_function
				       (SPRD_MIPI_DSIC_BASE, i + 4),
				       dsi_core_read_function
				       (SPRD_MIPI_DSIC_BASE, i + 8),
				       dsi_core_read_function
				       (SPRD_MIPI_DSIC_BASE, i + 12));
			}
			PERROR("**************************\n");
		}
#endif
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
					R_DSI_HOST_PWR_UP, 1);
	}
	/* dsi_irq_trick(1,reg_val); */
	return IRQ_HANDLED;
}


static void dsi_reset(int dev_id)
{
	sci_glb_set(DSI_AHB_SOFT_RST, BIT_DSI_SOFT_RST << dev_id);
	udelay(10);
	sci_glb_clr(DSI_AHB_SOFT_RST, BIT_DSI_SOFT_RST << dev_id);
}

static int32_t dsi_edpi_setbuswidth(struct sprd_dsi_context *dsi_ctx)
{
	dsih_color_coding_t color_coding = 0;
	struct info_mipi *mipi = dsi_ctx->intf->info.mipi;

	switch (mipi->panel->video_bus_width) {
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
		PERROR("fail, invalid video_bus_width\n");
		return 0;
	}

#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_and_function((SPRD_MIPI_DSIC_BASE +
		R_DSI_HOST_DPI_COLOR_CODE), 0xfffffff0);
	dsi_core_or_function((SPRD_MIPI_DSIC_BASE +
		R_DSI_HOST_DPI_COLOR_CODE), color_coding);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_DPI_CFG,
		((uint32_t) color_coding << 2));
#endif
	return 0;
}

static int32_t dsi_edpi_init(struct sprd_dsi_context *dsi_ctx)
{
#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_EDPI_CMD_SIZE, 0x500);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, DSI_EDPI_CFG,
		0x10500);
#endif
	return 0;
}

int32_t dpi_clock_cac(struct sprd_dsi_context *dsi_ctx)
{
	int divider;
	u32 hpixels, vlines, pclk;
	struct panel_spec *panel = dsi_ctx->intf->get_panel(dsi_ctx->intf);
	uint32_t pclk_src = dsi_ctx->intf->pclk_src;

	if (pclk_src <= 0) {
		PERROR("pclk source error : %u\n", pclk_src);
		return -1;
	}
	PRINT("pclk source = %u\n", pclk_src);
	if (dsi_ctx->intf->panel_if_type == SPRDFB_PANEL_IF_DPI) {
		struct timing_rgb *timing = &panel->timing.rgb_timing;

		hpixels =
			panel->width + timing->hsync + timing->hbp +
			timing->hfp;
		vlines =
			panel->height + timing->vsync + timing->vbp +
			timing->vfp;
	}
	pclk = hpixels * vlines * panel->fps;
	divider = ROUND(pclk_src, pclk);
	dsi_ctx->intf->dpi_clock = pclk_src / divider;

	return 0;
}

int32_t dsi_dpi_init(struct sprd_dsi_context *dsi_ctx)
{
	dsih_dpi_video_t dpi_param = { 0 };
	dsih_error_t result;

	struct info_mipi *mipi = dsi_ctx->intf->info.mipi;
	struct panel_spec *panel = mipi->panel;
	struct timing_rgb *timing = &panel->timing.rgb_timing;

	/* dpi_clock_cac(dsi_ctx); */
	dpi_param.no_of_lanes = panel->lan_number;
	dpi_param.byte_clock = panel->phy_feq / 8;
	if (dsi_ctx->intf->dpi_clock == 0)
		PERROR("error dpi_clock = %d\n", dsi_ctx->intf->dpi_clock);
	dpi_param.pixel_clock = dsi_ctx->intf->dpi_clock / 1000;

	dpi_param.max_hs_to_lp_cycles = 4;
	dpi_param.max_lp_to_hs_cycles = 15;
	dpi_param.max_clk_hs_to_lp_cycles = 4;
	dpi_param.max_clk_lp_to_hs_cycles = 15;
	switch (panel->video_bus_width) {
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
		PERROR("fail, invalid video_bus_width\n");
		break;
	}

	if (SPRDFB_POLARITY_POS == panel->h_sync_pol)
		dpi_param.h_polarity = 1;

	if (SPRDFB_POLARITY_POS == panel->v_sync_pol)
		dpi_param.v_polarity = 1;

	if (SPRDFB_POLARITY_POS == panel->de_pol)
		dpi_param.data_en_polarity = 1;

	dpi_param.h_active_pixels = panel->width;
	dpi_param.h_sync_pixels = timing->hsync;
	dpi_param.h_back_porch_pixels = timing->hbp;
	dpi_param.h_total_pixels =
		panel->width + timing->hsync + timing->hbp + timing->hfp;

	dpi_param.v_active_lines = panel->height;
	dpi_param.v_sync_lines = timing->vsync;
	dpi_param.v_back_porch_lines = timing->vbp;
	dpi_param.v_total_lines =
		panel->height + timing->vsync + timing->vbp + timing->vfp;

	dpi_param.receive_ack_packets = 0;
	dpi_param.video_mode = VIDEO_BURST_WITH_SYNC_PULSES;
	dpi_param.virtual_channel = 0;
	dpi_param.is_18_loosely = 1;

	result = mipi_dsih_dpi_video(&(dsi_ctx->dsi_inst), &dpi_param);
	if (result != OK) {
		PERROR("mipi_dsih_dpi_video fail (%d)!\n", result);
		return -1;
	}

	return 0;
}

static void dsi_log_error(const char *string)
{
	PERROR("%s", string);
}

static int32_t dsi_module_init(struct sprd_dsi_context *dsi_ctx)
{
	int ret = 0;
	int irq_num_1, irq_num_2;
	dsih_ctrl_t *dsi_instance = &(dsi_ctx->dsi_inst);

	dphy_t *phy = &(dsi_instance->phy_instance);

	if (dsi_ctx->is_inited) {
		PERROR("dsi_module_init. is_inited==true!\n");
		return 0;
	} else
		PRINT("dsi_module_init. call only once!\n");
	dsi_ctx->dsi_irq_trick = dispc_set_dsi_irq;
	phy->address = dsi_ctx->dsi_inst.address;
	phy->core_read_function = dsi_core_read_function;
	phy->core_write_function = dsi_core_write_function;
	phy->log_error = dsi_log_error;
	phy->log_info = NULL;
	phy->reference_freq = DSI_PHY_REF_CLOCK;

	dsi_instance->address = dsi_ctx->dsi_inst.address;
	dsi_instance->core_read_function = dsi_core_read_function;
	dsi_instance->core_write_function = dsi_core_write_function;
	dsi_instance->log_error = dsi_log_error;
	dsi_instance->log_info = NULL;
	/*in our rtl implementation, this is max rd time,
	 *not bta time and use 15bits
	 **/

	dsi_instance->max_bta_cycles = 0x6000;
	irq_num_1 = irq_of_parse_and_map(dsi_ctx->dsi_node, 0);
	PRINT("dsi irq_num_1 = %d\n", irq_num_1);

	ret =	request_irq(irq_num_1, dsi_isr0, IRQF_DISABLED, "DSI_INT0",
		dsi_ctx);
	if (ret) {
		PERROR("dsi failed to request irq int0!\n");
		/* clk_disable(dsi_ctx.clk_dsi); */
	} else
		PRINT(" dsi request irq int0 OK!\n");

	irq_num_2 = irq_of_parse_and_map(dsi_ctx->dsi_node, 1);
	PRINT("dsi irq_num_2 = %d\n", irq_num_2);

	ret = request_irq(irq_num_2, dsi_isr1, IRQF_DISABLED, "DSI_INT1",
		dsi_ctx);
	if (ret) {
		PERROR("dsi failed to request irq int1!\n");
		/* clk_disable(dsi_ctx.clk_dsi); */
	} else
		PRINT("dsi request irq int1 OK!\n");

	dsi_ctx->is_inited = true;

	return 0;
}

#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
int sprd_dsi_chg_dphy_freq(struct panel_if_device *intf, u32 dphy_freq)
{
	dsih_error_t result = OK;
	struct sprd_dsi_context *dsi_ctx =
		INTERFACE_TO_DSI_CONTEXT(intf);
	dsih_ctrl_t *ctrl = &(dsi_ctx->dsi_inst);
	struct panel_spec *panel = intf->info.mipi->panel;

	if (ctrl->status == INITIALIZED) {
		pr_info("Let's do update D-PHY frequency(%u)\n",
			dphy_freq);
		ctrl->phy_instance.phy_keep_work = true;
		result =
			mipi_dsih_dphy_configure(&ctrl->phy_instance,
			panel->lan_number, dphy_freq);
		if (result != OK) {
			PERROR("mipi_dsih_dphy_configure fail (%d)\n",
				result);
			ctrl->phy_instance.phy_keep_work = false;
			return -1;
		}
		ctrl->phy_instance.phy_keep_work = false;
	}
	panel->phy_feq = dphy_freq;
	return 0;
}
#else
int sprd_dsi_chg_dphy_freq(struct panel_if_device *intf, u32 dphy_freq)
{
	return -EINVAL;
}
#endif

int32_t sprd_dsih_init(struct sprd_dsi_context *dsi_ctx)
{
	dsih_error_t result = OK;
	dsih_ctrl_t *dsi_instance = &(dsi_ctx->dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct panel_spec *panel = dsi_ctx->intf->info.mipi->panel;
	int wait_count = 0;

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK0,
		0x1fffff);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK1,
		0x3ffff);

	if (SPRDFB_MIPI_MODE_CMD == panel->work_mode)
		dsi_edpi_init(dsi_ctx);

	dsi_instance->phy_feq = panel->phy_feq;
	dsi_instance->max_lanes = panel->lan_number;
	dsi_instance->color_mode_polarity = panel->color_mode_pol;
	dsi_instance->shut_down_polarity = panel->shut_down_pol;
	PDBG(dsi_instance);
	result = mipi_dsih_open(dsi_instance);
	if (OK != result) {
		PERROR("mipi_dsih_open fail (%d)!\n", result);
		dsi_ctx->status = 1;
		return 0;
	}
	XDBG(panel->phy_feq);
	XDBG(panel->lan_number);
	result =
		mipi_dsih_dphy_configure(phy, panel->lan_number,
		panel->phy_feq);
	if (OK != result) {
		PERROR("mipi_dsih_dphy_configure fail (%d)!\n", result);
		dsi_ctx->status = 1;
		return -1;
	}

	while (5 != (dsi_core_read_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_PHY_STATUS) & 5)
		&& (wait_count < 100000)) {
		udelay(3);
		if (0x0 == ++wait_count % 1000)
			PERROR("warning: busy waiting!\n");
	}
	if (wait_count >= 100000)
		PERROR("Errior: dsi phy can't be locked!!!\n");

	if (SPRDFB_MIPI_MODE_CMD == panel->work_mode)
		dsi_edpi_setbuswidth(dsi_ctx);

	result = mipi_dsih_enable_rx(dsi_instance, 1);
	if (OK != result) {
		PERROR("mipi_dsih_enable_rx fail (%d)!\n", result);
		dsi_ctx->status = 1;
		return -1;
	}

	result = mipi_dsih_ecc_rx(dsi_instance, 1);
	if (OK != result) {
		PERROR("mipi_dsih_ecc_rx fail (%d)!\n", result);
		dsi_ctx->status = 1;
		return -1;
	}

	result = mipi_dsih_eotp_rx(dsi_instance, 0);
	if (OK != result) {
		PERROR("mipi_dsih_eotp_rx fail (%d)!\n", result);
		dsi_ctx->status = 1;
		return -1;
	}

	result = mipi_dsih_eotp_tx(dsi_instance, 0);
	if (OK != result) {
		PERROR("mipi_dsih_eotp_tx fail (%d)!\n", result);
		dsi_ctx->status = 1;
		return -1;
	}

	if (SPRDFB_MIPI_MODE_VIDEO == panel->work_mode)
		dsi_dpi_init(dsi_ctx);

	mipi_dsih_dphy_enable_nc_clk(&(dsi_instance->phy_instance), false);
	dsi_ctx->status = 0;

	return 0;
}

int32_t sprd_dsi_init(struct panel_if_device *intf)
{
	dsih_error_t result = OK;
	struct sprd_dsi_context *dsi_ctx =
		INTERFACE_TO_DSI_CONTEXT(intf);
	dsih_ctrl_t *dsi_instance = &dsi_ctx->dsi_inst;
	g_dsi_base_addr = dsi_ctx->dsi_inst.address;
	dsi_ctx->intf = intf;
	if (!dsi_ctx->is_inited) {
		/* init */
		if (intf->panel_ready) {
			/* panel ready */
			PERROR("dsi has alread initialized\n");
			dsi_instance->status = INITIALIZED;
			dsi_instance->phy_instance.status = INITIALIZED;
			dsi_module_init(dsi_ctx);
			dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
				R_DSI_HOST_INT_MSK0, 0x0);
			dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
				R_DSI_HOST_INT_MSK1, 0x800);
		} else {
			/* panel not ready */
			PERROR("dsi is not initialized,will crash.\n");
			dsi_enable(dsi_ctx->dsi_inst.dev_id);
			dsi_reset(intf->dev_id);
			dsi_module_init(dsi_ctx);
			result = sprd_dsih_init(dsi_ctx);
		}
	} else {
		/* resume */
		PERROR("resume\n");
		dsi_enable(dsi_ctx->dsi_inst.dev_id);
		dsi_reset(dsi_ctx->dsi_inst.dev_id);
		result = sprd_dsih_init(dsi_ctx);
	}

	return result;
}

int32_t sprd_dsi_uninit(struct sprd_dsi_context *dsi_ctx)
{
	dsih_error_t result;
	dsih_ctrl_t *dsi_instance = &(dsi_ctx->dsi_inst);

	PRINT("TODO:use mipi dsih power off instead\n");
	/* clean mask before suspend */
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
				R_DSI_HOST_INT_MSK0, 0x0);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
				R_DSI_HOST_INT_MSK1, 0x0);

	mipi_dsih_dphy_enable_hs_clk(&(dsi_instance->phy_instance), false);
	dsi_instance->status = NOT_INITIALIZED;

	dsi_ctx->status = 1;
	result = mipi_dsih_close(&(dsi_ctx->dsi_inst));
	if (OK != result) {
		PERROR("sprd_dsi_uninit fail (%d)!\n", result);
	}
	mipi_dsih_hal_power(dsi_instance, 0);
	msleep(20);
	dsi_disable(dsi_ctx->dsi_inst.dev_id);

	return 0;
}

int32_t sprd_dsi_suspend(struct sprd_dsi_context *dsi_ctx)
{
	dsih_ctrl_t *dsi_instance = &(dsi_ctx->dsi_inst);

	/* clean mask before suspend */
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
				R_DSI_HOST_INT_MSK0, 0x0);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
				R_DSI_HOST_INT_MSK1, 0x0);
	dsi_instance->status = NOT_INITIALIZED;
	mipi_dsih_dphy_enable_hs_clk(&(dsi_instance->phy_instance), false);
	dsi_instance->phy_instance.status = NOT_INITIALIZED;
	dsi_ctx->status = 1;
	mipi_dsih_hal_power(dsi_instance, 0);
	mipi_dsih_close(&(dsi_ctx->dsi_inst));
	msleep(20);
	dsi_disable(dsi_ctx->dsi_inst.dev_id);
	return 0;
}

int32_t sprd_dsi_resume(struct sprd_dsi_context *dsi_ctx)
{
	dsih_ctrl_t *dsi_instance = &(dsi_ctx->dsi_inst);

#if 0
	dsih_error_t result = OK;
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi *mipi = dev->panel->info.mipi;
#endif

	/* mipi_dsih_dphy_shutdown(&(dsi_instance->phy_instance), 1); */
	mipi_dsih_hal_power(dsi_instance, 1);

	return 0;
}

int32_t sprd_dsi_ready(struct sprd_dsi_context *dsi_ctx)
{
	struct info_mipi *mipi = dsi_ctx->intf->info.mipi;

	if (SPRDFB_MIPI_MODE_CMD == mipi->panel->work_mode) {
		mipi_dsih_cmd_mode(&(dsi_ctx->dsi_inst), 1);
		mipi_dsih_dphy_enable_hs_clk(&(dsi_ctx->dsi_inst.
			phy_instance), true);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_CMD_MODE_CFG, 0x0);
	} else {
		mipi_dsih_dphy_enable_hs_clk(&(dsi_ctx->dsi_inst.
			phy_instance), true);
		mipi_dsih_video_mode(&(dsi_ctx->dsi_inst), 1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_PWR_UP, 0);
		udelay(100);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_PWR_UP, 1);
		usleep_range(3000, 3500);
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_INT_ST0);
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_INT_ST1);
	}

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK0,
		0x0);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK1,
		0x800);
	return 0;
}

int32_t sprd_dsi_before_panel_reset(struct sprd_dsi_context *dsi_ctx)
{
	sprd_dsi_set_lp_mode(dsi_ctx);
	return 0;
}

int32_t sprd_dsi_enter_ulps(struct sprd_dsi_context *dsi_ctx)
{
	sprd_dsi_set_ulps_mode(dsi_ctx);
	return 0;
}

uint32_t sprd_dsi_get_status(struct sprd_dsi_context *dsi_ctx)
{
	return dsi_ctx->status;
}

static int32_t sprd_dsi_set_cmd_mode(struct sprd_dsi_context *dsi_ctx)
{
	mipi_dsih_cmd_mode(&(dsi_ctx->dsi_inst), 1);
	return 0;
}

static int32_t sprd_dsi_set_video_mode(struct sprd_dsi_context
	*dsi_ctx)
{
	mipi_dsih_video_mode(&(dsi_ctx->dsi_inst), 1);
	return 0;
}

static int32_t sprd_dsi_set_lp_mode(struct sprd_dsi_context *dsi_ctx)
{
#ifndef FB_DSIH_VERSION_1P21A
	uint32_t reg_val;
#endif

	mipi_dsih_cmd_mode(&(dsi_ctx->dsi_inst), 1);
#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_CMD_MODE_CFG, 0x01ffff00);
	mipi_dsih_dphy_enable_hs_clk(&(dsi_ctx->dsi_inst.phy_instance),
		false);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_CMD_MODE_CFG, 0x1fff);
	reg_val =
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_PHY_IF_CTRL);
	reg_val = reg_val & (~(BIT(0)));
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_PHY_IF_CTRL, reg_val);
#endif
	return 0;
}

static int32_t sprd_dsi_set_hs_mode(struct sprd_dsi_context *dsi_ctx)
{

	mipi_dsih_cmd_mode(&(dsi_ctx->dsi_inst), 1);
#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_CMD_MODE_CFG, 0x0);
	mipi_dsih_dphy_enable_hs_clk(&(dsi_ctx->dsi_inst.phy_instance),
		true);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_CMD_MODE_CFG, 0x1);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_PHY_IF_CTRL, 0x1);
#endif
	return 0;
}

static int32_t sprd_dsi_set_ulps_mode(struct sprd_dsi_context *dsi_ctx)
{
	mipi_dsih_ulps_mode(&(dsi_ctx->dsi_inst), 1);
	return 0;
}

static int32_t sprd_dsi_set_data_lp_mode(struct sprd_dsi_context
	*dsi_ctx)
{
	/* pr_debug(KERN_INFO "sprd: [%s]\n",__FUNCTION__); */

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
		R_DSI_HOST_CMD_MODE_CFG, 0x1fff);

	return 0;
}

static int32_t sprd_dsi_set_data_hs_mode(struct sprd_dsi_context
	*dsi_ctx)
{

	int active_mode = mipi_dsih_active_mode(&(dsi_ctx->dsi_inst));

	/* pr_debug(KERN_INFO "sprd: [%s]\n",__FUNCTION__); */

	if (1 == active_mode) {
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_CMD_MODE_CFG, 0x1);
	} else {
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE,
			R_DSI_HOST_CMD_MODE_CFG, 0x0);
	}
	return 0;
}

static int32_t sprd_dsi_gen_write(struct sprd_dsi_context *dsi_ctx,
	uint8_t *param, uint16_t param_length)
{
	dsih_error_t result;

	result =
		mipi_dsih_gen_wr_cmd(&(dsi_ctx->dsi_inst), 0, param,
		param_length);
	if (OK != result) {
		PERROR("error (%d)\n", result);
		return -1;
	}
	return 0;
}

static int32_t sprd_dsi_gen_read(struct sprd_dsi_context *dsi_ctx,
	uint8_t *param, uint16_t param_length, uint8_t bytes_to_read,
	uint8_t *read_buffer)
{
	uint16_t result;
	uint32_t reg_val, reg_val_1, reg_val_2;

	result =
		mipi_dsih_gen_rd_cmd(&(dsi_ctx->dsi_inst), 0, param,
		param_length, bytes_to_read, read_buffer);

	reg_val =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_PHY_STATUS);
#ifdef FB_DSIH_VERSION_1P21A
	reg_val_1 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST0);
	reg_val_2 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST1);
#else
	reg_val_1 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST0);
	reg_val_2 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST1);
#endif
	if (0 != (reg_val & 0x2)) {
		PERROR("mipi read hang (0x%x)!\n", reg_val);
		dsi_ctx->status = 2;
		result = 0;
	}

	if (0 != (reg_val_1 & 0x701)) {
		PERROR("mipi read status error!(0x%x, 0x%x)\n", reg_val_1,
			reg_val_2);
		result = 0;
	}

	if (0 == result) {
		PERROR("return error (%d)\n", result);
		return -1;
	}
	return 0;
}

static int32_t sprd_dsi_dcs_write(struct sprd_dsi_context *dsi_ctx,
	uint8_t *param, uint16_t param_length)
{
	dsih_error_t result;

	result =
		mipi_dsih_dcs_wr_cmd(&(dsi_ctx->dsi_inst), 0, param,
		param_length);
	if (OK != result) {
		PERROR("error (%d)\n", result);
		return -1;
	}
	return 0;
}

static int32_t sprd_dsi_dcs_read(struct sprd_dsi_context *dsi_ctx,
	uint8_t command, uint8_t bytes_to_read, uint8_t *read_buffer)
{
	uint16_t result;
	uint32_t reg_val, reg_val_1, reg_val_2;

	result =
		mipi_dsih_dcs_rd_cmd(&(dsi_ctx->dsi_inst), 0, command,
		bytes_to_read, read_buffer);
	reg_val =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_PHY_STATUS);
#ifdef FB_DSIH_VERSION_1P21A
	reg_val_1 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST0);
	reg_val_2 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST1);
#else
	reg_val_1 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST0);
	reg_val_2 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST1);
#endif
	if (0 != (reg_val & 0x2)) {
		PERROR("mipi read hang (0x%x)!\n", reg_val);
		dsi_ctx->status = 2;
		result = 0;
	}

	if (0 != (reg_val_1 & 0x701)) {
		PERROR("mipi read status error!(0x%x, 0x%x)\n", reg_val_1,
			reg_val_2);
		result = 0;
	}

	if (0 == result) {
		PERROR("return error (%d)\n", result);
		return -1;
	}
	return 0;
}

static int32_t sprd_dsi_force_write(struct sprd_dsi_context *dsi_ctx,
	uint8_t data_type, uint8_t *p_params, uint16_t param_length)
{
	int32_t ret;
	ret =
		mipi_dsih_gen_wr_packet(&(dsi_ctx->dsi_inst), 0, data_type,
		p_params, param_length);
	return ret;
}

static int32_t sprd_dsi_force_read(struct sprd_dsi_context *dsi_ctx,
	uint8_t command, uint8_t bytes_to_read, uint8_t *read_buffer)
{
	int32_t ret = 0;
	uint32_t reg_val, reg_val_1, reg_val_2;

	ret =
		mipi_dsih_gen_rd_packet(&(dsi_ctx->dsi_inst), 0, 6, 0, command,
		bytes_to_read, read_buffer);

	reg_val =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_PHY_STATUS);
	reg_val_1 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST0);
	reg_val_2 =
		dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST1);
	if (0 != (reg_val & 0x2)) {
		PERROR("mipi read hang (0x%x)!\n", reg_val);
		dsi_ctx->status = 2;
		ret = 0;
	}

	if (0 != (reg_val_1 & 0x701)) {
		PERROR("mipi read status error!(0x%x, 0x%x)\n", reg_val_1,
			reg_val_2);
		ret = 0;
	}

	if (0 == ret) {
		PERROR("return error (%d)\n", ret);
		return -1;
	}

	return 0;
}

static int32_t sprd_dsi_eotp_set(struct sprd_dsi_context *dsi_ctx,
	uint8_t rx_en, uint8_t tx_en)
{
	dsih_ctrl_t *dsi_instance = &(dsi_ctx->dsi_inst);
	if (0 == rx_en)
		mipi_dsih_eotp_rx(dsi_instance, 0);
	else if (1 == rx_en)
		mipi_dsih_eotp_rx(dsi_instance, 1);
	if (0 == tx_en)
		mipi_dsih_eotp_tx(dsi_instance, 0);
	else if (1 == tx_en)
		mipi_dsih_eotp_tx(dsi_instance, 1);
	return 0;
}

struct ops_mipi sprd_mipi_ops = {
	.mipi_set_cmd_mode = sprd_dsi_set_cmd_mode,
	.mipi_set_video_mode = sprd_dsi_set_video_mode,
	.mipi_set_lp_mode = sprd_dsi_set_lp_mode,
	.mipi_set_hs_mode = sprd_dsi_set_hs_mode,
	.mipi_set_data_lp_mode = sprd_dsi_set_data_lp_mode,
	.mipi_set_data_hs_mode = sprd_dsi_set_data_hs_mode,
	.mipi_gen_write = sprd_dsi_gen_write,
	.mipi_gen_read = sprd_dsi_gen_read,
	.mipi_dcs_write = sprd_dsi_dcs_write,
	.mipi_dcs_read = sprd_dsi_dcs_read,
	.mipi_force_write = sprd_dsi_force_write,
	.mipi_force_read = sprd_dsi_force_read,
	.mipi_eotp_set = sprd_dsi_eotp_set,
};
