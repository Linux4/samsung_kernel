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
#ifdef CONFIG_OF
#include <linux/fb.h>

#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include "sprdfb.h"
#include "sprdfb_panel.h"
#include "sprdfb_chip_common.h"

#ifdef CONFIG_FB_SCX30G
#define FB_DSIH_VERSION_1P21A
#endif
#ifdef FB_DSIH_VERSION_1P21A
#include "dsi_1_21a/mipi_dsih_local.h"
#include "dsi_1_21a/mipi_dsih_dphy.h"
#include "dsi_1_21a/mipi_dsih_hal.h"
#include "dsi_1_21a/mipi_dsih_api.h"
#else
#include "dsi_1_10a/mipi_dsih_local.h"
#include "dsi_1_10a/mipi_dsih_dphy.h"
#include "dsi_1_10a/mipi_dsih_hal.h"
#include "dsi_1_10a/mipi_dsih_api.h"
#endif

#define DSI_PHY_REF_CLOCK (26*1000)


#define DSI_EDPI_CFG (0x6c)

struct sprdfb_dsi_context {
	struct clk		*clk_dsi;
	bool			is_inited;
	uint32_t		status;/*0- normal, 1- uninit, 2-abnormal*/
	struct sprdfb_device	*dev;

	dsih_ctrl_t	dsi_inst;
};

static struct sprdfb_dsi_context dsi_ctx;
#ifdef CONFIG_OF
uint32_t g_dsi_base_addr = 0;
#endif


static int32_t sprdfb_dsi_set_lp_mode(void);
static int32_t sprdfb_dsi_set_ulps_mode(void);

static uint32_t dsi_core_read_function(uint32_t addr, uint32_t offset)
{
	return dispc_glb_read(addr + offset);
}

static void dsi_core_write_function(uint32_t addr, uint32_t offset, uint32_t data)
{
//	__raw_writel(data, addr + offset);
	sci_glb_write((addr + offset), data, 0xffffffff);
}

void dsi_core_or_function(unsigned int addr,unsigned int data)
{
	sci_glb_write(addr,(dispc_glb_read(addr) | data), 0xffffffff);
}
void dsi_core_and_function(unsigned int addr,unsigned int data)
{
	sci_glb_write(addr,(dispc_glb_read(addr) & data), 0xffffffff);
}

#if 0
static Trick_Item s_trick_record0[DSI_INT0_MAX]= {
	//en interval begin dis_cnt en_cnt
	{1,300,  0,  0,  0},//ack_with_err_0
	{0,  0,  0,  0,  0},//ack_with_err_1
	{0,  0,  0,  0,  0},//ack_with_err_2
	{0,  0,  0,  0,  0},//ack_with_err_3
	{0,  0,  0,  0,  0},//ack_with_err_4
	{0,  0,  0,  0,  0},//ack_with_err_5
	{0,  0,  0,  0,  0},//ack_with_err_6
	{0,  0,  0,  0,  0},//ack_with_err_7
	{0,  0,  0,  0,  0},//ack_with_err_8
	{0,  0,  0,  0,  0},//ack_with_err_9
	{0,  0,  0,  0,  0},//ack_with_err_10
	{0,  0,  0,  0,  0},//ack_with_err_11
	{0,  0,  0,  0,  0},//ack_with_err_12
	{0,  0,  0,  0,  0},//ack_with_err_13
	{0,  0,  0,  0,  0},//ack_with_err_14
	{0,  0,  0,  0,  0},//ack_with_err_15
	{0,  0,  0,  0,  0},//dphy_errors_0
	{0,  0,  0,  0,  0},//dphy_errors_1
	{0,  0,  0,  0,  0},//dphy_errors_2
	{0,  0,  0,  0,  0},//dphy_errors_3
	{0,  0,  0,  0,  0},//dphy_errors_4
};

static Trick_Item s_trick_record1[DSI_INT1_MAX]= {
	//en interval begin dis_cnt en_cnt
	{0,  0,  0,  0,  0},//to_hs_tx
	{0,  0,  0,  0,  0},//to_lp_rx
	{0,  0,  0,  0,  0},//ecc_single_err
	{0,  0,  0,  0,  0},//ecc_multi_err
	{0,  0,  0,  0,  0},//crc_err
	{0,  0,  0,  0,  0},//pkt_size_err
	{0,  0,  0,  0,  0},//eopt_err
	{0,  0,  0,  0,  0},//dpi_pld_wr_err
	{0,  0,  0,  0,  0},//gen_cmd_wr_err
	{0,  0,  0,  0,  0},//gen_pld_wr_err
	{0,  0,  0,  0,  0},//gen_pld_send_err
	{0,  0,  0,  0,  0},//gen_pld_rd_err
	{0,  0,  0,  0,  0},//gen_pld_recv_err
	{0,  0,  0,  0,  0},//dbi_cmd_wr_err
	{0,  0,  0,  0,  0},//dbi_pld_wr_err
	{0,  0,  0,  0,  0},//dbi_pld_rd_err
	{0,  0,  0,  0,  0},//dbi_pld_recv_err
	{0,  0,  0,  0,  0},//dbi_illegal_comm_err
};

/*
func:dsi_irq0_trick
desc:if a xxx interruption come many times in a short time, print the firt one, mask the follows.
     a fixed-long time later, enable this interruption.
*/
void dsi_irq_trick(uint32_t int_id,uint32_t int_status)
{
	static uint32_t mask_irq0_times = 0;
	static uint32_t open_irq0_times = 0;
	static uint32_t mask_irq1_times = 0;
	static uint32_t open_irq1_times = 0;
	uint32_t i = 0;

	while(i < DSI_INT0_MAX) {
		if(s_trick_record0[i].trick_en != 0) {
			if(((int_id == 0) && (int_status & (1UL << i)))
				&& (s_trick_record0[i].begin_jiffies == 0)) {
				//disable this interruption
				DSI_INT_MASK0_SET(i,1);
				s_trick_record0[i].begin_jiffies = jiffies;
				s_trick_record0[i].disable_cnt++;
				mask_irq0_times++;
				pr_debug("sprdfb: %s[%d]: INT0[%d] disable times:0x%08x \n",__func__,__LINE__,i,s_trick_record0[i].disable_cnt);
			}

			if((s_trick_record0[i].begin_jiffies > 0)
				&& ((s_trick_record0[i].begin_jiffies + s_trick_record0[i].interval) < jiffies)) {
				//re-enable this interruption
				DSI_INT_MASK0_SET(i,0);
				s_trick_record0[i].begin_jiffies = 0;
				s_trick_record0[i].enable_cnt++;
				open_irq0_times++;
				pr_debug("sprdfb: %s[%d]: INT0[%d] enable times:0x%08x \n",__func__,__LINE__,i,s_trick_record0[i].enable_cnt);
			}
		}
		i++;
	}

	i = 0;
	while(i < DSI_INT1_MAX) {
		if(s_trick_record1[i].trick_en != 0) {
			if(((int_id == 1) && (int_status & (1UL << i)))
				&& (s_trick_record1[i].begin_jiffies == 0)) {
				//disable this interruption
				DSI_INT_MASK1_SET(i,1);
				s_trick_record1[i].begin_jiffies = jiffies;
				s_trick_record1[i].disable_cnt++;
				mask_irq1_times++;
				pr_debug("sprdfb: %s[%d]: INT1[%d] disable times:0x%08x \n",__func__,__LINE__,i,s_trick_record1[i].disable_cnt);
			}

			if((s_trick_record1[i].begin_jiffies > 0)
				&& ((s_trick_record1[i].begin_jiffies + s_trick_record1[i].interval) < jiffies)) {
				//re-enable this interruption
				DSI_INT_MASK1_SET(i,0);
				s_trick_record1[i].begin_jiffies = 0;
				s_trick_record1[i].enable_cnt++;
				open_irq1_times++;
				pr_debug("sprdfb: %s[%d]: INT1[%d] enable times:0x%08x \n",__func__,__LINE__,i,s_trick_record1[i].enable_cnt);
			}
		}
		i++;
	}
	if(int_status) {
		printk("sprdfb: %s[%d]:total DSI_mask0:0x%08x DSI_open0:0x%08x; DSI_mask1:0x%08x DSI_open1:0x%08x\n",__func__,__LINE__,
		mask_irq0_times,open_irq0_times,
		mask_irq1_times,open_irq1_times);
	}
}
#else
void dsi_irq_trick(void)
{
    /*enable ack_with_err_0 interrupt*/
    DSI_INT_MASK0_SET(0, 0);
}
#endif

//static uint32_t sot_ever_happened = 0;
static irqreturn_t dsi_isr0(int irq, void *data)
{
#ifdef FB_DSIH_VERSION_1P21A
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_ST0);
#else
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST0);
#endif

	printk(KERN_ERR "sprdfb: [%s](0x%x)!\n", __FUNCTION__, reg_val);
	/*
	printk("Warning: sot_ever_happened:(0x%x)!\n",sot_ever_happened);
	*/
	if(reg_val & 0x1) {
/*
		//sot_ever_happened = 1;
		mask = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_MSK0);
		mask |= 0x1;
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_MSK0, mask);
*/
	    DSI_INT_MASK0_SET(0, 1);
	}

	//dsi_irq_trick(0,reg_val);
	return IRQ_HANDLED;
}

static irqreturn_t dsi_isr1(int irq, void *data)
{
#ifdef FB_DSIH_VERSION_1P21A
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_ST1);
#else
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST1);
#endif
	uint32_t i = 0;
	struct sprdfb_dsi_context *dsi_ctx = (struct sprdfb_dsi_context *)data;
	struct sprdfb_device *dev = dsi_ctx->dev;
	dsih_ctrl_t* dsi_instance = &(dsi_ctx->dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct panel_spec* panel = dev->panel;
	struct info_mipi * mipi = panel->info.mipi;

	printk(KERN_ERR "sprdfb: [%s](0x%x)!\n", __FUNCTION__, reg_val);

	if(BIT(7) == (reg_val & BIT(7))){
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PWR_UP, 0);
		/*need delay 1us*/
		printk("sprdfb: reset dsi host!\n");
		printk("sprdfb: mipi->lan_number:%d ,mipi->phy_feq:%d \n",mipi->lan_number,mipi->phy_feq);
		if(NULL == phy){
			printk("sprdfb: the phy is null \n");
			//dsi_irq_trick(1,reg_val);
			return IRQ_NONE;
		}

                mipi_dsih_dphy_configure(phy,  mipi->lan_number, mipi->phy_feq);

		{/*for debug*/
			for(i=0;i<256;i+=16){
				printk("sprdfb: %x: 0x%x, 0x%x, 0x%x, 0x%x\n", i, dsi_core_read_function(SPRD_MIPI_DSIC_BASE, i),
					dsi_core_read_function(SPRD_MIPI_DSIC_BASE, i+4),
					dsi_core_read_function(SPRD_MIPI_DSIC_BASE, i+8),
					dsi_core_read_function(SPRD_MIPI_DSIC_BASE, i+12));
			}
			printk("**************************\n");
		}
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PWR_UP, 1);
	}
	//dsi_irq_trick(1,reg_val);
	return IRQ_HANDLED;
}

static void dsi_reset(void)
{
	sci_glb_set(DSI_AHB_SOFT_RST, BIT_DSI_SOFT_RST);
 	udelay(10);
	sci_glb_clr(DSI_AHB_SOFT_RST, BIT_DSI_SOFT_RST);
}

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
		printk(KERN_ERR "sprdfb: [%s] fail, invalid video_bus_width\n", __FUNCTION__);
		return 0;
	}

#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_and_function((SPRD_MIPI_DSIC_BASE+R_DSI_HOST_DPI_COLOR_CODE),0xfffffff0);
	dsi_core_or_function((SPRD_MIPI_DSIC_BASE+R_DSI_HOST_DPI_COLOR_CODE),color_coding);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_DPI_CFG, ((uint32_t)color_coding<<2));
#endif
	return 0;
}


static int32_t dsi_edpi_init(void)
{
#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_write_function((uint32_t)SPRD_MIPI_DSIC_BASE,	(uint32_t)R_DSI_HOST_EDPI_CMD_SIZE, 0x500);
#else
	dsi_core_write_function((uint32_t)SPRD_MIPI_DSIC_BASE,  (uint32_t)DSI_EDPI_CFG, 0x10500);
#endif
	return 0;
}

int32_t dsi_dpi_init(struct sprdfb_device *dev)
{
	dsih_dpi_video_t dpi_param = {0};
	dsih_error_t result;
	struct panel_spec* panel = dev->panel;
	struct info_mipi * mipi = panel->info.mipi;

	dpi_param.no_of_lanes = mipi->lan_number;
	dpi_param.byte_clock = mipi->phy_feq / 8;
	dpi_param.pixel_clock = dev->dpi_clock/1000;
#ifdef FB_DSIH_VERSION_1P21A
	dpi_param.max_hs_to_lp_cycles = 4;//110;
	dpi_param.max_lp_to_hs_cycles = 15;//10;
#endif

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
		printk(KERN_ERR "sprdfb: [%s] fail, invalid video_bus_width\n", __FUNCTION__);
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
#ifndef CONFIG_FB_LCD_ILI9486S1_MIPI
	dpi_param.is_18_loosely = 0;
#else
	dpi_param.is_18_loosely = 1;
#endif

	result = mipi_dsih_dpi_video(&(dsi_ctx.dsi_inst), &dpi_param);
	if(result != OK){
		printk(KERN_ERR "sprdfb: [%s] mipi_dsih_dpi_video fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	return 0;
}

static void dsi_log_error(const char * string)
{
	printk(string);
}

static int32_t dsi_module_init(struct sprdfb_device *dev)
{
	int ret = 0;
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi * mipi = dev->panel->info.mipi;
	int irq_num_1, irq_num_2;

	pr_debug(KERN_INFO "sprdfb: [%s]\n", __FUNCTION__);

	if(dsi_ctx.is_inited){
		printk(KERN_INFO "sprdfb: dsi_module_init. is_inited==true!");
		return 0;
	}
	else{
		printk(KERN_INFO "sprdfb: dsi_module_init. call only once!");
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
#ifndef FB_DSIH_VERSION_1P21A
	dsi_instance->max_hs_to_lp_cycles = 4;//110;
	dsi_instance->max_lp_to_hs_cycles = 15;//10;
#endif
	dsi_instance->max_lanes = mipi->lan_number;
#ifdef CONFIG_OF
	irq_num_1 = irq_of_parse_and_map(dev->of_dev->of_node, 1);
#else
	irq_num_1 = IRQ_DSI_INTN0;
#endif
	printk("sprdfb: dsi irq_num_1 = %d\n", irq_num_1);

//	ret = request_irq(IRQ_DSI_INTN0, dsi_isr0, IRQF_DISABLED, "DSI_INT0", &dsi_ctx);
	ret = request_irq(irq_num_1, dsi_isr0, IRQF_DISABLED, "DSI_INT0", &dsi_ctx);
	if (ret) {
		printk(KERN_ERR "sprdfb: dsi failed to request irq int0!\n");
//		clk_disable(dsi_ctx.clk_dsi);
	}else{
		printk(KERN_ERR "sprdfb: dsi request irq int0 OK!\n");
	}

#ifdef CONFIG_OF
	irq_num_2 = irq_of_parse_and_map(dev->of_dev->of_node, 2);
#else
	irq_num_2 = IRQ_DSI_INTN1;
#endif
	printk("sprdfb: dsi irq_num_2 = %d\n", irq_num_2);

//	ret = request_irq(IRQ_DSI_INTN1, dsi_isr1, IRQF_DISABLED, "DSI_INT1", &dsi_ctx);
	ret = request_irq(irq_num_2, dsi_isr1, IRQF_DISABLED, "DSI_INT1", &dsi_ctx);
	if (ret) {
		printk(KERN_ERR "sprdfb: dsi failed to request irq int1!\n");
//		clk_disable(dsi_ctx.clk_dsi);
	}else{
		printk(KERN_ERR "sprdfb: dsi request irq int1 OK!\n");
	}

	dsi_ctx.is_inited = true;

	return 0;
}

void dsi_print_global_config(void)
{
	u32 reg_val0, reg_val1, reg_val2;

	reg_val0 = sci_glb_read(REG_AP_AHB_MISC_CKG_EN, 0xffffffff);
	reg_val1 = sci_glb_read(DSI_REG_EB, 0xffffffff);
	reg_val2 = sci_glb_read(REG_AON_APB_PWR_CTRL, 0xffffffff);

	printk("sprdfb: dsi: 0x20E00040 = 0x%x, 0x20E00000 = 0x%x, 0x402E0024 = 0x%x\n",
		reg_val0, reg_val1, reg_val2);
}

int32_t sprdfb_dsih_init(struct sprdfb_device *dev)
{
	dsih_error_t result = OK;
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi * mipi = dev->panel->info.mipi;
	int i = 0;
#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK0, 0x1fffff);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK1, 0x3ffff);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK0, 0x1fffff);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK1, 0x3ffff);
#endif

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		dsi_edpi_init();
	}/*else{
		dsi_dpi_init(dev->panel);
	}*/

/*
	result = mipi_dsih_unregister_all_events(dsi_instance);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_unregister_all_events fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}
*/
//	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK0, 0x1fffff);
//	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK1, 0x3ffff);
#ifndef FB_DSIH_VERSION_1P21A
	dsi_instance->phy_feq = dev->panel->info.mipi->phy_feq;
#endif
	result = mipi_dsih_open(dsi_instance);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_open fail (%d)!\n", __FUNCTION__, result);
		dsi_ctx.status = 1;
		return -1;
	}

	result = mipi_dsih_dphy_configure(phy,  mipi->lan_number, mipi->phy_feq);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_dphy_configure fail (%d)!\n", __FUNCTION__, result);
		dsi_ctx.status = 1;
		return -1;
	}

	while(5 != (dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_STATUS) & 5)){
		if(0x0 == ++i%500000){
			dsi_print_global_config();
			printk("sprdfb: [%s] warning: busy waiting!\n", __FUNCTION__);
			break;
		}
	}

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		dsi_edpi_setbuswidth(mipi);
	}

	result = mipi_dsih_enable_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_enable_rx fail (%d)!\n", __FUNCTION__, result);
		dsi_ctx.status = 1;
		return -1;
	}

	result = mipi_dsih_ecc_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_ecc_rx fail (%d)!\n", __FUNCTION__, result);
		dsi_ctx.status = 1;
		return -1;
	}

	result = mipi_dsih_eotp_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_eotp_rx fail (%d)!\n", __FUNCTION__, result);
		dsi_ctx.status = 1;
		return -1;
	}

	result = mipi_dsih_eotp_tx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_eotp_tx fail (%d)!\n", __FUNCTION__, result);
		dsi_ctx.status = 1;
		return -1;
	}

	if(SPRDFB_MIPI_MODE_VIDEO == mipi->work_mode){
		dsi_dpi_init(dev);
	}

#ifdef FB_DSIH_VERSION_1P21A
#ifndef CONFIG_LCD_MIPI_CONTINOUS_CLK
	mipi_dsih_dphy_enable_nc_clk(&(dsi_instance->phy_instance), 1);
#endif
#endif
	dsi_ctx.status = 0;

	return 0;
}

#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
int sprdfb_dsi_chg_dphy_freq(struct sprdfb_device *fb_dev,
				u32 dphy_freq)
{
	dsih_error_t result = OK;
	dsih_ctrl_t *ctrl = &(dsi_ctx.dsi_inst);
	struct info_mipi *mipi = fb_dev->panel->info.mipi;

	if (ctrl->status == INITIALIZED) {
		pr_info("Let's do update D-PHY frequency(%u)\n", dphy_freq);
		ctrl->phy_instance.phy_keep_work = true;
		result = mipi_dsih_dphy_configure(&ctrl->phy_instance,
				mipi->lan_number, dphy_freq);
		if (result != OK) {
			pr_err("[%s]: mipi_dsih_dphy_configure fail (%d)\n",
					__func__, result);
			ctrl->phy_instance.phy_keep_work = false;
			return -1;
		}
		ctrl->phy_instance.phy_keep_work = false;
	}
	mipi->phy_feq = dphy_freq;
	return 0;
}
#else
int sprdfb_dsi_chg_dphy_freq(struct sprdfb_device *fb_dev,
		u32 dphy_freq)
{
	return -EINVAL;
}
#endif

int32_t sprdfb_dsi_init(struct sprdfb_device *dev)
{
	dsih_error_t result = OK;
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	dsi_ctx.dev = dev;
#ifdef CONFIG_OF
	struct resource r;

	if(0 != of_address_to_resource(dev->of_dev->of_node, 1, &r)){
		printk(KERN_ERR "sprdfb: sprdfb_dsi_init fail. (can't get register base address)\n");
		return -1;
	}
	g_dsi_base_addr = r.start;
	printk("sprdfb: set g_dsi_base_addr = 0x%x\n", g_dsi_base_addr);
#endif

	if(!dsi_ctx.is_inited){
		//init
		if(dev->panel_ready){
			//panel ready
			printk(KERN_INFO "sprdfb: [%s]: dsi has alread initialized\n", __FUNCTION__);
			dsi_instance->status = INITIALIZED;
			dsi_module_init(dev);
#ifdef FB_DSIH_VERSION_1P21A
			dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_INT_MSK0, 0x0);
			dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_INT_MSK1, 0x800);
#else
			dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK0, 0x0);
			dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK1, 0x800);
#endif
		}else{
			//panel not ready
			printk(KERN_INFO "sprdfb: [%s]: dsi is not initialized\n", __FUNCTION__);
			dsi_enable();
			dsi_reset();
			dsi_module_init(dev);
			result=sprdfb_dsih_init(dev);
		}
	}else{
		//resume
		printk(KERN_INFO "sprdfb: [%s]: resume\n", __FUNCTION__);
		dsi_enable();
		dsi_reset();
		result=sprdfb_dsih_init(dev);
	}

	return result;
}

int32_t sprdfb_dsi_uninit(struct sprdfb_device *dev)
{
	dsih_error_t result;
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	printk(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);
#ifdef FB_DSIH_VERSION_1P21A
	mipi_dsih_dphy_enable_hs_clk(&(dsi_instance->phy_instance), false);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0);
#endif
	result = mipi_dsih_close(&(dsi_ctx.dsi_inst));
	dsi_instance->status = NOT_INITIALIZED;

	dsi_ctx.status = 1;

	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: sprdfb_dsi_uninit fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	msleep(10);
	dsi_disable();
	return 0;
}

int32_t sprdfb_dsi_suspend(struct sprdfb_device *dev)
{
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	printk(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);
//	sprdfb_dsi_uninit(dev);
//	mipi_dsih_dphy_close(&(dsi_instance->phy_instance));
//	mipi_dsih_dphy_shutdown(&(dsi_instance->phy_instance), 0);
	mipi_dsih_hal_power(dsi_instance, 0);

	return 0;
}

int32_t sprdfb_dsi_resume(struct sprdfb_device *dev)
{
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
#if 0	
	dsih_error_t result = OK;	
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi * mipi = dev->panel->info.mipi;
#endif
	printk(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

#if 0
	result = mipi_dsih_dphy_open(&(dsi_instance->phy_instance));
	if(0 != result){
		printk("Jessica: mipi_dsih_dphy_open fail!(%d)\n",result);
	}
	udelay(100);
#endif
//	mipi_dsih_dphy_shutdown(&(dsi_instance->phy_instance), 1);
	mipi_dsih_hal_power(dsi_instance, 1);

#if 0
	result = mipi_dsih_dphy_configure(phy,  mipi->lan_number, mipi->phy_feq);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_dphy_configure fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}
#endif
	return 0;
}

int32_t sprdfb_dsi_ready(struct sprdfb_device *dev)
{
	struct info_mipi * mipi = dev->panel->info.mipi;

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		mipi_dsih_cmd_mode(&(dsi_ctx.dsi_inst), 1);
#ifdef FB_DSIH_VERSION_1P21A
		mipi_dsih_dphy_enable_hs_clk(&(dsi_ctx.dsi_inst.phy_instance), true);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x0);
#else
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0x1);
#endif
	}else{
#ifdef FB_DSIH_VERSION_1P21A
		mipi_dsih_dphy_enable_hs_clk(&(dsi_ctx.dsi_inst.phy_instance), true);
#else
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0x1);
#endif
		mipi_dsih_video_mode(&(dsi_ctx.dsi_inst), 1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PWR_UP, 0);
		udelay(100);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PWR_UP, 1);
		usleep_range(3000, 3500);
#ifdef FB_DSIH_VERSION_1P21A
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_ST0);
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_ST1);
#else
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST0);
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST1);
#endif
	}

#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_INT_MSK0, 0x0);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_INT_MSK1, 0x800);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK0, 0x0);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK1, 0x800);
#endif
	return 0;
}

int32_t sprdfb_dsi_before_panel_reset(struct sprdfb_device *dev)
{
	sprdfb_dsi_set_lp_mode();
	return 0;
}

int32_t sprdfb_dsi_enter_ulps(struct sprdfb_device *dev)
{
	sprdfb_dsi_set_ulps_mode();
	return 0;
}

uint32_t sprdfb_dsi_get_status(struct sprdfb_device *dev)
{
	return dsi_ctx.status;
}

static int32_t sprdfb_dsi_set_cmd_mode(void)
{
	mipi_dsih_cmd_mode(&(dsi_ctx.dsi_inst), 1);
	return 0;
}

static int32_t sprdfb_dsi_set_video_mode(void)
{
	mipi_dsih_video_mode(&(dsi_ctx.dsi_inst), 1);
	return 0;
}

static int32_t sprdfb_dsi_set_lp_mode(void)
{
	uint32_t reg_val;
	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	mipi_dsih_cmd_mode(&(dsi_ctx.dsi_inst), 1);
#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x01ffff00);
	mipi_dsih_dphy_enable_hs_clk(&(dsi_ctx.dsi_inst.phy_instance), false);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x1fff);
	reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL);
	reg_val = reg_val & (~(BIT(0)));
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL,  reg_val);
#endif
	return 0;
}

static int32_t sprdfb_dsi_set_hs_mode(void)
{
	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	mipi_dsih_cmd_mode(&(dsi_ctx.dsi_inst), 1);
#ifdef FB_DSIH_VERSION_1P21A
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x0);
	mipi_dsih_dphy_enable_hs_clk(&(dsi_ctx.dsi_inst.phy_instance), true);
#else
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x1);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0x1);
#endif
	return 0;
}

static int32_t sprdfb_dsi_set_ulps_mode(void)
{
	mipi_dsih_ulps_mode(&(dsi_ctx.dsi_inst), 1);
	return 0;
}

static int32_t sprdfb_dsi_set_data_lp_mode(void)
{
//	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x1fff);

	return 0;
}

static int32_t sprdfb_dsi_set_data_hs_mode(void)
{
	int active_mode = mipi_dsih_active_mode(&(dsi_ctx.dsi_inst));
//	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

//	printk("aoke sprdfb: set data hs mode active_mode=%d\n",active_mode);
	if(1==active_mode){
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x1);
	}else{
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x0);
	}
	return 0;
}

static int32_t sprdfb_dsi_gen_write(uint8_t *param, uint16_t param_length)
{
	dsih_error_t result;

	result = mipi_dsih_gen_wr_cmd(&(dsi_ctx.dsi_inst), 0, param, param_length);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s] error (%d)\n", __FUNCTION__, result);
		return -1;
	}
	return 0;
}

#define LCM_SEND(len) ((1 << 24)| len)
#define LCM_TAG_MASK  ((1 << 24) -1)

static unsigned char set_bl_seq[] = {
	0x51, 0xFF
};

void backlight_control(int brigtness)
{
	set_bl_seq[1] = brigtness;
	sprdfb_dsi_gen_write(set_bl_seq, LCM_SEND(2) & LCM_TAG_MASK);
}
EXPORT_SYMBOL(backlight_control);


static int32_t sprdfb_dsi_gen_read(uint8_t *param, uint16_t param_length, uint8_t bytes_to_read, uint8_t *read_buffer)
{
	uint16_t result;
	uint32_t reg_val, reg_val_1, reg_val_2;
	result = mipi_dsih_gen_rd_cmd(&(dsi_ctx.dsi_inst), 0, param, param_length, bytes_to_read, read_buffer);

	reg_val = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_PHY_STATUS);
#ifdef FB_DSIH_VERSION_1P21A
	reg_val_1 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST0);
	reg_val_2 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST1);
#else
	reg_val_1 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST0);
	reg_val_2 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST1);
#endif
	if(0 != (reg_val & 0x2)){
		printk("sprdfb: [%s] mipi read hang (0x%x)!\n", __FUNCTION__, reg_val);
		dsi_ctx.status = 2;
		result = 0;
	}

	if(0 != (reg_val_1 & 0x701)){
		printk("sprdfb: [%s] mipi read status error!(0x%x, 0x%x)\n", __FUNCTION__, reg_val_1, reg_val_2);
		result = 0;
	}

	if(0 == result){
		printk(KERN_ERR "sprdfb: [%s] return error (%d)\n", __FUNCTION__, result);
		return -1;
	}
	return 0;
}

static int32_t sprdfb_dsi_dcs_write(uint8_t *param, uint16_t param_length)
{
	dsih_error_t result;
	result = mipi_dsih_dcs_wr_cmd(&(dsi_ctx.dsi_inst), 0, param, param_length);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s] error (%d)\n", __FUNCTION__, result);
		return -1;
	}
	return 0;
}

static int32_t sprdfb_dsi_dcs_read(uint8_t command, uint8_t bytes_to_read, uint8_t *read_buffer)
{
	uint16_t result;
	uint32_t reg_val, reg_val_1, reg_val_2;

	result = mipi_dsih_dcs_rd_cmd(&(dsi_ctx.dsi_inst), 0, command, bytes_to_read, read_buffer);
	reg_val = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_PHY_STATUS);
#ifdef FB_DSIH_VERSION_1P21A
	reg_val_1 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST0);
	reg_val_2 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST1);
#else
	reg_val_1 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST0);
	reg_val_2 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST1);
#endif
	if(0 != (reg_val & 0x2)){
		printk("sprdfb: [%s] mipi read hang (0x%x)!\n", __FUNCTION__, reg_val);
		dsi_ctx.status = 2;
		result = 0;
	}

	if(0 != (reg_val_1 & 0x701)){
		printk("sprdfb: [%s] mipi read status error!(0x%x, 0x%x)\n", __FUNCTION__, reg_val_1, reg_val_2);
		result = 0;
	}

	if(0 == result){
		printk(KERN_ERR "sprdfb: [%s] return error (%d)\n", __FUNCTION__, result);
		return -1;
	}
	return 0;
}

static int32_t sprd_dsi_force_write(uint8_t data_type, uint8_t *p_params, uint16_t param_length)
{
	int32_t iRtn = 0;

	iRtn = mipi_dsih_gen_wr_packet(&(dsi_ctx.dsi_inst), 0, data_type,  p_params, param_length);

	return iRtn;
}

static int32_t sprd_dsi_write(uint8_t data_type, uint8_t *p_params, uint16_t param_length)
{
	int32_t iRtn = 0;

	iRtn = mipi_dsih_wr_packet(&(dsi_ctx.dsi_inst), 0, data_type,  p_params, param_length);

	return iRtn;
}

static int32_t sprd_dsi_force_read(uint8_t command, uint8_t bytes_to_read, uint8_t * read_buffer)
{
	int32_t iRtn = 0;
	uint32_t reg_val, reg_val_1, reg_val_2;

	iRtn = mipi_dsih_gen_rd_packet(&(dsi_ctx.dsi_inst),  0,  6,  0, command,  bytes_to_read, read_buffer);

	reg_val = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_PHY_STATUS);
#ifdef FB_DSIH_VERSION_1P21A
	reg_val_1 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST0);
	reg_val_2 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_INT_ST1);
#else
	reg_val_1 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST0);
	reg_val_2 = dispc_glb_read(SPRD_MIPI_DSIC_BASE + R_DSI_HOST_ERROR_ST1);
#endif
	if(0 != (reg_val & 0x2)){
		printk("sprdfb: [%s] mipi read hang (0x%x)!\n", __FUNCTION__, reg_val);
		dsi_ctx.status = 2;
		iRtn = 0;
	}

	if(0 != (reg_val_1 & 0x701)){
		printk("sprdfb: [%s] mipi read status error!(0x%x, 0x%x)\n", __FUNCTION__, reg_val_1, reg_val_2);
		iRtn = 0;
	}


	if(0 == iRtn){
		printk(KERN_ERR "sprdfb: [%s] return error (%d)\n", __FUNCTION__, iRtn);
		return -1;
	}

	return 0;
}

static int32_t sprd_dsi_eotp_set(uint8_t rx_en, uint8_t tx_en)
{
	dsih_ctrl_t *curInstancePtr = &(dsi_ctx.dsi_inst);
	if(0 == rx_en)
		mipi_dsih_eotp_rx(curInstancePtr, 0);
	else if(1 == rx_en)
		mipi_dsih_eotp_rx(curInstancePtr, 1);
	if(0 == tx_en)
		mipi_dsih_eotp_tx(curInstancePtr, 0);
	else if(1 == tx_en)
		mipi_dsih_eotp_tx(curInstancePtr, 1);
	return 0;
}

struct ops_mipi sprdfb_mipi_ops = {
	.mipi_set_cmd_mode = sprdfb_dsi_set_cmd_mode,
	.mipi_set_video_mode = sprdfb_dsi_set_video_mode,
	.mipi_set_lp_mode = sprdfb_dsi_set_lp_mode,
	.mipi_set_hs_mode = sprdfb_dsi_set_hs_mode,
	.mipi_set_data_lp_mode = sprdfb_dsi_set_data_lp_mode,
	.mipi_set_data_hs_mode = sprdfb_dsi_set_data_hs_mode,
	.mipi_gen_write = sprdfb_dsi_gen_write,
	.mipi_gen_read = sprdfb_dsi_gen_read,
	.mipi_dcs_write = sprdfb_dsi_dcs_write,
	.mipi_dcs_read = sprdfb_dsi_dcs_read,
	.mipi_force_write = sprd_dsi_force_write,
	.mipi_force_read = sprd_dsi_force_read,
	.mipi_write = sprd_dsi_write,
	.mipi_eotp_set = sprd_dsi_eotp_set,
};



