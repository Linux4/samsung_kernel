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

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

#include <mach/pinmap.h>

#include "dispc_reg.h"
#include "lcd_dummy.h"
#include "dispc.h"

#define pr_debug printk

#ifdef CONFIG_FB_SCX15
#define DISPC_CLOCK_PARENT ("clk_192m")
#define DISPC_CLOCK (192*1000000)
#define DISPC_DBI_CLOCK_PARENT ("clk_256m")
#define DISPC_DBI_CLOCK (256*1000000)
#define DISPC_DPI_CLOCK_PARENT ("clk_192m")
#define SPRDFB_DPI_CLOCK_SRC (192000000)
#define DISPC_DPI_CLOCK 			(192*1000000/18)
#else
#define DISPC_CLOCK_PARENT ("clk_256m")
#define DISPC_CLOCK (256*1000000)
#define DISPC_DBI_CLOCK_PARENT ("clk_256m")
#define DISPC_DBI_CLOCK (256*1000000)
#define DISPC_DPI_CLOCK_PARENT ("clk_384m")
#define SPRDFB_DPI_CLOCK_SRC (384000000)
#define DISPC_DPI_CLOCK 			(384*1000000/36)
#endif

#define DISPC_EMC_EN_PARENT ("clk_aon_apb")

#define DISPC_PLL_CLK				("clk_disc0")
#define DISPC_DBI_CLK				("clk_disc0_dbi")
#define DISPC_DPI_CLK				("clk_disc0_dpi")
#define DISPC_EMC_CLK				("clk_disp_emc")

#define PATTEN_GRID_WIDTH (40)
#define PATTEN_GRID_HEIGHT (40)
#define PATTEN_COLOR_COUNT (7)

extern struct panel_spec lcd_dummy_mipi_spec;
extern struct panel_spec lcd_dummy_rgb_spec;
extern struct panel_spec lcd_dummy_mcu_spec;

extern void autotst_dsi_dump(void);
extern void autotst_dsi_dump1(void);
extern int32_t autotst_dsi_init(struct panel_spec *panel);
extern int32_t autotst_dsi_uninit(void);

struct autotst_dispc_context {
	struct clk		*clk_dispc;
	struct clk 		*clk_dispc_dpi;
	struct clk 		*clk_dispc_dbi;
	struct clk 		*clk_dispc_emc;
	uint32_t		fb_addr_v;
	uint32_t		fb_addr_p;
	uint32_t		fb_size;
	uint32_t		dispc_if;
	bool			is_inited;
};

static struct autotst_dispc_context autotst_dispc_ctx = {0};
static struct panel_spec *autotst_panel = NULL;

static uint32_t g_patten_table[PATTEN_COLOR_COUNT] =
	{0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffff00, 0xff00ffff, 0xffff00ff, 0xffffffff};

int autotst_dispc_pin_ctrl(int type)
{
	static int pin_table[29];
	static int is_first_time = true;
	int i;
	u32 regs = REG_PIN_LCD_CSN1;
	u32 func;

	if (is_first_time) {
		for (i = 0; i < 29; ++i) {
			pin_table[i] = pinmap_get(regs);
			regs += 4;
		}
		is_first_time = false;
	}

	if (type == DISPC_PIN_FUNC3)
		func = BITS_PIN_DS(1) | BITS_PIN_AF(DISPC_PIN_FUNC3) | BIT_PIN_SLP_AP;
	else {
		pr_err("The function hasn't been implemented yet\n");
		return 0;
	}

	regs = REG_PIN_LCD_CSN1;
	for (i = 0; i < 29; ++i) {
		if (type == DISPC_PIN_FUNC0)
			pinmap_set(regs, pin_table[i]);
		else
			pinmap_set(regs, func);
		regs += 4;
	}

	return 0;
}

/**********************************************/
/*                      MCU PANEL CONFIG                                */
/**********************************************/
static uint32_t mcu_calc_timing(struct timing_mcu *timing)
{
	uint32_t  clk_rate;
	uint32_t  rcss, rlpw, rhpw, wcss, wlpw, whpw;
	struct clk * clk = NULL;

	if(NULL == timing){
		printk(KERN_ERR "autotst_dispc: [%s]: Invalid Param\n", __FUNCTION__);
		return 0;
	}

	clk = clk_get(NULL,"clk_dispc_dbi");
	if (IS_ERR(clk)) {
		printk(KERN_WARNING "autotst_dispc: get clk_dispc_dbi fail!\n");
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_dispc_dbi ok!\n");
	}
//	clk_rate = clk_get_rate(clk) / 1000000;
	clk_rate = 250;	// dummy 250M Hz

	pr_debug(KERN_INFO "autotst_dispc: [%s] clk_rate: 0x%x\n", __FUNCTION__, clk_rate);

	/********************************************************
	* we assume : t = ? ns, dispc_dbi = ? MHz   so
	*      1ns need cycle  :  dispc_dbi /1000
	*      tns need cycles :  t * dispc_dbi / 1000
	*
	********************************************************/
#define MAX_DBI_RWCSS_TIMING_VALUE	15
#define MAX_DBI_RWLPW_TIMING_VALUE	63
#define MAX_DBI_RWHPW_TIMING_VALUE	63
#define DBI_CYCLES(ns) (( (ns) * clk_rate + 1000 - 1)/ 1000)

	/* ceiling*/
	rcss = DBI_CYCLES(timing->rcss);
	if (rcss > MAX_DBI_RWCSS_TIMING_VALUE) {
		rcss = MAX_DBI_RWCSS_TIMING_VALUE ;
	}

	rlpw = DBI_CYCLES(timing->rlpw);
	if (rlpw > MAX_DBI_RWLPW_TIMING_VALUE) {
		rlpw = MAX_DBI_RWLPW_TIMING_VALUE ;
	}

	rhpw = DBI_CYCLES (timing->rhpw);
	if (rhpw > MAX_DBI_RWHPW_TIMING_VALUE) {
		rhpw = MAX_DBI_RWHPW_TIMING_VALUE ;
	}

	wcss = DBI_CYCLES(timing->wcss);
	if (wcss > MAX_DBI_RWCSS_TIMING_VALUE) {
		wcss = MAX_DBI_RWCSS_TIMING_VALUE ;
	}

	wlpw = DBI_CYCLES(timing->wlpw);
	if (wlpw > MAX_DBI_RWLPW_TIMING_VALUE) {
		wlpw = MAX_DBI_RWLPW_TIMING_VALUE ;
	}

#ifndef CONFIG_LCD_CS_ALWAYS_LOW
	 /* dispc/lcdc will waste one cycle because CS pulse will use one cycle*/
	whpw = DBI_CYCLES (timing->whpw) - 1;
#else
	whpw = DBI_CYCLES (timing->whpw) ;
#endif
	if (whpw > MAX_DBI_RWHPW_TIMING_VALUE) {
		whpw = MAX_DBI_RWHPW_TIMING_VALUE ;
	}

	return (whpw | (wlpw << 6) | (wcss << 12)
			| (rhpw << 16) |(rlpw << 22) | (rcss << 28));
}

#ifdef CONFIG_FB_LCD_CS1
/*cs1*/
static void mcu_dispc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = 0;

	pr_debug("autotst_dispc: [%s] for cs1\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "autotst_dispc: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_MCU != panel->type){
		printk(KERN_ERR "autotst_dispc: [%s] fail.(not  mcu panel)\n", __FUNCTION__);
		return;
	}

	/*use dbi as interface*/
	dispc_set_bits((2<<1), DISPC_CTRL);
	autotst_dispc_ctx.dispc_if = DISPC_IF_DBI;

	/* CS1 bus mode [BIT8]: 8080/6800 */
	switch (panel->info.mcu->bus_mode) {
	case LCD_BUS_8080:
		break;
	case LCD_BUS_6800:
		reg_val |= (1<<8);
		break;
	default:
		break;
	}
	/* CS1 bus width [BIT11:9] */
	switch (panel->info.mcu->bus_width) {
	case 8:
		break;
	case 9:
		reg_val |= (1 << 9);
		break;
	case 16:
		reg_val |= (2 << 9);
		break;
	case 18:
		reg_val |= (3 << 9) ;
		break;
	case 24:
		reg_val |= (4 << 9);
		break;
	default:
		break;
	}

	/*CS1 pixel bits [BIT13:12]*/
	switch (panel->info.mcu->bpp) {
	case 16:
		break;
	case 18:
		reg_val |= (1 << 12) ;
		break;
	case 24:
		reg_val |= (2 << 12);
		break;
	default:
		break;
	}

#ifndef CONFIG_FB_NO_FMARK
	/*TE enable*/
	reg_val |= (1 << 16);
	if(SPRDFB_POLARITY_NEG == panel->info.mcu->te_pol){
		reg_val |= (1<< 17);
	}
	dispc_write(panel->info.mcu->te_sync_delay, DISPC_TE_SYNC_DELAY);
#endif

#ifdef CONFIG_LCD_CS_ALWAYS_LOW
	/*CS alway low mode*/
	reg_val |= (1<<21);
#else
	/*CS not alway low mode*/
#endif

	/*CS1 selected*/
	reg_val |= (1 << 20);

	dispc_write(reg_val, DISPC_DBI_CTRL);

	pr_debug("autotst_dispc: [%s] DISPC_DBI_CTRL = %d\n", __FUNCTION__, dispc_read(DISPC_DBI_CTRL));
}

static void mcu_dispc_set_timing(struct panel_spec *panel)
{
	uint32_t timing = 0;

	pr_debug("autotst_dispc: [%s] for cs1\n", __FUNCTION__);

	timing = mcu_calc_timing(panel->info.mcu->timing);
	dispc_write(timing,DISPC_DBI_TIMING1);
}

#else
/*cs0*/
static void mcu_dispc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = 0;

	pr_debug("autotst_dispc: [%s] for cs0\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "autotst_dispc: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_MCU != panel->type){
		printk(KERN_ERR "autotst_dispc: [%s] fail.(not  mcu panel)\n", __FUNCTION__);
		return;
	}

	/*use dbi as interface*/
	dispc_set_bits((2<<1), DISPC_CTRL);
	autotst_dispc_ctx.dispc_if = DISPC_IF_DBI;

	/* CS0 bus mode [BIT0]: 8080/6800 */
	switch (panel->info.mcu->bus_mode) {
	case LCD_BUS_8080:
		break;
	case LCD_BUS_6800:
		reg_val |= 1;
		break;
	default:
		break;
	}
	/* CS0 bus width [BIT3:1] */
	switch (panel->info.mcu->bus_width) {
	case 8:
		break;
	case 9:
		reg_val |= (1 << 1);
		break;
	case 16:
		reg_val |= (2 << 1);
		break;
	case 18:
		reg_val |= (3 << 1) ;
		break;
	case 24:
		reg_val |= (4 << 1);
		break;
	default:
		break;
	}

	/*CS0 pixel bits [BIT5:4]*/
	switch (panel->info.mcu->bpp) {
	case 16:
		break;
	case 18:
		reg_val |= (1 << 4) ;
		break;
	case 24:
		reg_val |= (2 << 4);
		break;
	default:
		break;
	}

#ifndef CONFIG_FB_NO_FMARK
	/*TE enable*/
	reg_val |= (1 << 16);
	if(SPRDFB_POLARITY_NEG == panel->info.mcu->te_pol){
		reg_val |= (1<< 17);
	}
	dispc_write(panel->info.mcu->te_sync_delay, DISPC_TE_SYNC_DELAY);
#endif

#ifdef CONFIG_LCD_CS_ALWAYS_LOW
	/*CS alway low mode*/
	reg_val |= (1<<21);
#else
	/*CS not alway low mode*/
#endif

	/*CS0 selected*/

	dispc_write(reg_val, DISPC_DBI_CTRL);

	pr_debug("autotst_dispc: [%s] DISPC_DBI_CTRL = %d\n", __FUNCTION__, dispc_read(DISPC_DBI_CTRL));
}

static void mcu_dispc_set_timing(struct panel_spec *panel)
{
	uint32_t timing = 0;

	pr_debug("autotst_dispc: [%s] for cs0\n", __FUNCTION__);

	timing = mcu_calc_timing(panel->info.mcu->timing);
	dispc_write(timing,DISPC_DBI_TIMING0);
}
#endif


/**********************************************/
/*                      RGB PANEL CONFIG                                 */
/**********************************************/
static uint32_t rgb_calc_h_timing(struct timing_rgb *timing)
{
	return  (timing->hsync | (timing->hbp << 8) | (timing->hfp << 20));
}


static uint32_t rgb_calc_v_timing(struct timing_rgb *timing)
{
	return (timing->vsync| (timing->vbp << 8) | (timing->vfp << 20));
}

static void rgb_dispc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = dispc_read(DISPC_DPI_CTRL);

	pr_debug("autotst_dispc: [%s]\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "autotst_dispc: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_RGB != panel->type){
		printk(KERN_ERR "autotst_dispc: [%s] fail.(not  mcu panel)\n", __FUNCTION__);
		return;
	}

	/*use dpi as interface*/
	dispc_clear_bits((3<<1), DISPC_CTRL);
	autotst_dispc_ctx.dispc_if = DISPC_IF_DPI;

	/*h sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.rgb->h_sync_pol){
		reg_val |= (1<<0);
	}

	/*v sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.rgb->v_sync_pol){
		reg_val |= (1<<1);
	}

	/*de sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.rgb->de_pol){
		reg_val |= (1<<2);
	}

#ifdef CONFIG_DPI_SINGLE_RUN
	/*single run mode*/
	reg_val |= (1<<3);
#endif

	/*dpi bits*/
	switch(panel->info.rgb->video_bus_width){
	case 16:
		break;
	case 18:
		reg_val |= (1 << 6);
		break;
	case 24:
		reg_val |= (2 << 6);
		break;
	default:
		break;
	}

	dispc_write(reg_val, DISPC_DPI_CTRL);

	pr_debug("autotst_dispc: [%s] DISPC_DPI_CTRL = %d\n", __FUNCTION__, dispc_read(DISPC_DPI_CTRL));
}

static void rgb_dispc_set_timing(struct panel_spec *panel)
{
	uint32_t timing_h = 0;
	uint32_t timing_v = 0;

	pr_debug("autotst_dispc: [%s]\n", __FUNCTION__);

	timing_h = rgb_calc_h_timing(panel->info.rgb->timing);
	timing_v = rgb_calc_v_timing(panel->info.rgb->timing);

	dispc_write(timing_h, DISPC_DPI_H_TIMING);
	dispc_write(timing_v, DISPC_DPI_V_TIMING);
}


/**********************************************/
/*                      MIPI PANEL CONFIG                                 */
/**********************************************/
static void mipi_dispc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = dispc_read(DISPC_DPI_CTRL);

	pr_debug("autotst_dispc: [%s]\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "autotst_dispc: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_MIPI != panel->type){
		printk(KERN_ERR "autotst_dispc: [%s] fail.(not  mcu panel)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_MIPI_MODE_CMD == panel->info.mipi->work_mode){
		/*use edpi as interface*/
		dispc_set_bits((1<<1), DISPC_CTRL);
		autotst_dispc_ctx.dispc_if = DISPC_IF_EDPI;
	}else{
		/*use dpi as interface*/
		dispc_clear_bits((3<<1), DISPC_CTRL);
		autotst_dispc_ctx.dispc_if = DISPC_IF_DPI;
	}

	/*h sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.mipi->h_sync_pol){
		reg_val |= (1<<0);
	}

	/*v sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.mipi->v_sync_pol){
		reg_val |= (1<<1);
	}

	/*de sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.mipi->de_pol){
		reg_val |= (1<<2);
	}

	if(SPRDFB_MIPI_MODE_VIDEO == panel->info.mipi->work_mode){
#ifdef CONFIG_DPI_SINGLE_RUN
		/*single run mode*/
		reg_val |= (1<<3);
#endif
	}else{
		if(!(panel->cap & PANEL_CAP_NOT_TEAR_SYNC)){
			printk("autotst_dispc: mipi_dispc_init_config not support TE\n");
			/*enable te*/
			reg_val |= (1<<8);
		}
		/*te pol*/
		if(SPRDFB_POLARITY_NEG == panel->info.mipi->te_pol){
			reg_val |= (1<<9);
		}
		/*use external te*/
		reg_val |= (1<<10);
	}

	/*dpi bits*/
	switch(panel->info.rgb->video_bus_width){
	case 16:
		break;
	case 18:
		reg_val |= (1 << 6);
		break;
	case 24:
		reg_val |= (2 << 6);
		break;
	default:
		break;
	}

	dispc_write(reg_val, DISPC_DPI_CTRL);

	pr_debug("autotst_dispc: [%s] DISPC_DPI_CTRL = %d\n", __FUNCTION__, dispc_read(DISPC_DPI_CTRL));
}

static void mipi_dispc_set_timing(struct panel_spec *panel)
{
	uint32_t timing_h = 0;
	uint32_t timing_v = 0;

	pr_debug("autotst_dispc: [%s]\n", __FUNCTION__);

	timing_h = rgb_calc_h_timing(panel->info.mipi->timing);
	timing_v = rgb_calc_v_timing(panel->info.mipi->timing);

	dispc_write(timing_h, DISPC_DPI_H_TIMING);
	dispc_write(timing_v, DISPC_DPI_V_TIMING);
}

/**********************************************/
/*                      COMMON CONFIG                                 */
/**********************************************/
static void dispc_print_clk(void)
{
    uint32_t reg_val0,reg_val1,reg_val2;
    reg_val0 = sci_glb_read(SPRD_AONAPB_BASE+0x4, 0xffffffff);
    reg_val1 = sci_glb_read(SPRD_AHB_BASE, 0xffffffff);
    reg_val2 = sci_glb_read(SPRD_APBREG_BASE, 0xffffffff);
    printk("autotst_dispc:0x402e0004 = 0x%x 0x20d00000 = 0x%x 0x71300000 = 0x%x \n",reg_val0, reg_val1, reg_val2);
    reg_val0 = sci_glb_read(SPRD_APBCKG_BASE+0x34, 0xffffffff);
    reg_val1 = sci_glb_read(SPRD_APBCKG_BASE+0x30, 0xffffffff);
    reg_val2 = sci_glb_read(SPRD_APBCKG_BASE+0x2c, 0xffffffff);
    printk("autotst_dispc:0x71200034 = 0x%x 0x71200030 = 0x%x 0x7120002c = 0x%x \n",reg_val0, reg_val1, reg_val2);
}

static void dispc_reset(void)
{
	sci_glb_set(REG_AHB_SOFT_RST, (BIT_DISPC_SOFT_RST) );
        udelay(10);
	sci_glb_clr(REG_AHB_SOFT_RST, (BIT_DISPC_SOFT_RST) );
}

static inline void dispc_set_bg_color(uint32_t bg_color)
{
	dispc_write(bg_color, DISPC_BG_COLOR);
}

static inline void dispc_set_osd_ck(uint32_t ck_color)
{
	dispc_write(ck_color, DISPC_OSD_CK);
}

static inline void dispc_osd_enable(bool is_enable)
{
	uint32_t reg_val;

	reg_val = dispc_read(DISPC_OSD_CTRL);
	if(is_enable){
		reg_val = reg_val | (BIT(0));
	}
	else{
		reg_val = reg_val & (~(BIT(0)));
	}
	dispc_write(reg_val, DISPC_OSD_CTRL);
}


static void dispc_dithering_enable(bool enable)
{
	if(enable){
		dispc_set_bits(BIT(6), DISPC_CTRL);
	}else{
		dispc_clear_bits(BIT(6), DISPC_CTRL);
	}
}

static void dispc_pwr_enable(bool enable)
{
	if(enable){
		dispc_set_bits(BIT(7), DISPC_CTRL);
	}else{
		dispc_clear_bits(BIT(7), DISPC_CTRL);
	}
}

static void dispc_set_exp_mode(uint16_t exp_mode)
{
	uint32_t reg_val = dispc_read(DISPC_CTRL);

	reg_val &= ~(0x3 << 16);
	reg_val |= (exp_mode << 16);
	dispc_write(reg_val, DISPC_CTRL);
}

static void dispc_module_enable(void)
{
	/*dispc module enable */
	dispc_write((1<<0), DISPC_CTRL);

	/*disable dispc INT*/
	dispc_write(0x0, DISPC_INT_EN);

	/* clear dispc INT */
	dispc_write(0x1F, DISPC_INT_CLR);
}

static void dispc_layer_init(struct panel_spec *panel)
{
	uint32_t reg_val = 0;

//	dispc_clear_bits((1<<0),DISPC_IMG_CTRL);
	dispc_write(0x0, DISPC_IMG_CTRL);
	dispc_clear_bits((1<<0),DISPC_OSD_CTRL);

	/******************* OSD layer setting **********************/

	/*enable OSD layer*/
	reg_val |= (1 << 0);

	/*disable  color key */

	/* alpha mode select  - block alpha*/
	reg_val |= (1 << 2);

	/* data format */
	//if (var->bits_per_pixel == 32) {
	if(1){
		/* ABGR */
		reg_val |= (3 << 4);
		/* rb switch */
		reg_val |= (1 << 15);
	} else {
		/* RGB565 */
		reg_val |= (5 << 4);
		/* B2B3B0B1 */
		reg_val |= (2 << 8);
	}

	dispc_write(reg_val, DISPC_OSD_CTRL);

	/* OSD layer alpha value */
	dispc_write(0xff, DISPC_OSD_ALPHA);

	/* OSD layer size */
	reg_val = ( panel->width & 0xfff) | (( panel->height & 0xfff ) << 16);
	dispc_write(reg_val, DISPC_OSD_SIZE_XY);

	/* OSD layer start position */
	dispc_write(0, DISPC_OSD_DISP_XY);

	/* OSD layer pitch */
	reg_val = ( panel->width & 0xfff) ;
	dispc_write(reg_val, DISPC_OSD_PITCH);

	/* OSD color_key value */
	dispc_set_osd_ck(0x0);

	/* DISPC workplane size */
	reg_val = ( panel->width & 0xfff) | ((panel->height & 0xfff ) << 16);
	dispc_write(reg_val, DISPC_SIZE_XY);
}

static void dispc_run(void)
{
	if(DISPC_IF_DPI == autotst_dispc_ctx.dispc_if){
		/*dpi register update*/
		dispc_set_bits(BIT(5), DISPC_DPI_CTRL);
		udelay(30);

		/*dpi register update with SW and VSync*/
		dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);

		/* start refresh */
		dispc_set_bits((1 << 4), DISPC_CTRL);

	}else{
		/* start refresh */
		dispc_set_bits((1 << 4), DISPC_CTRL);
	}
}

static void dispc_stop(void)
{
	if(DISPC_IF_DPI == autotst_dispc_ctx.dispc_if){
		/*dpi register update with SW only*/
		dispc_set_bits(BIT(4), DISPC_DPI_CTRL);

		/* stop refresh */
		dispc_clear_bits((1 << 4), DISPC_CTRL);
	}
}

static int dispc_clk_enable(struct autotst_dispc_context *dispc_ctx_ptr)
{
	int ret = 0;
	bool is_dispc_enable=false;
	bool is_dispc_dpi_enable=false;

	pr_debug(KERN_INFO "autotst_dispc:[%s]\n",__FUNCTION__);
	if(!dispc_ctx_ptr){
		return -1;
	}

	ret = clk_enable(dispc_ctx_ptr->clk_dispc);
	if(ret){
		printk("autotst_dispc:enable clk_dispc error!!!\n");
		ret=-1;
		goto ERROR_CLK_ENABLE;
	}
	is_dispc_enable=true;
	ret = clk_enable(dispc_ctx_ptr->clk_dispc_dpi);
	if(ret){
		printk("autotst_dispc:enable clk_dispc_dpi error!!!\n");
		ret=-1;
		goto ERROR_CLK_ENABLE;
	}
	is_dispc_dpi_enable=true;
	ret = clk_enable(dispc_ctx_ptr->clk_dispc_dbi);
	if(ret){
		printk("autotst_dispc:enable clk_dispc_dbi error!!!\n");
		ret=-1;
		goto ERROR_CLK_ENABLE;
	}

	return ret;

ERROR_CLK_ENABLE:
	if(is_dispc_enable){
		clk_disable(dispc_ctx_ptr->clk_dispc);
	}
	if(is_dispc_dpi_enable){
		clk_disable(dispc_ctx_ptr->clk_dispc_dpi);
	}

	printk("autotst_dispc:sprdfb_dispc_clk_enable error!!!!!!\n");
	return ret;
}

static int32_t dispc_clk_init(void)
{
	int ret = 0;
	struct clk *clk_parent1, *clk_parent2, *clk_parent3, *clk_parent4;

	pr_debug(KERN_INFO "autotst_dispc:[%s]\n", __FUNCTION__);

	sci_glb_set(DISPC_CORE_EN, BIT_DISPC_CORE_EN);

	clk_parent1 = clk_get(NULL, DISPC_CLOCK_PARENT);
	if (IS_ERR(clk_parent1)) {
		printk(KERN_WARNING "autotst_dispc: get clk_parent1 fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_parent1 ok!\n");
	}

	clk_parent2 = clk_get(NULL, DISPC_DBI_CLOCK_PARENT);
	if (IS_ERR(clk_parent2)) {
		printk(KERN_WARNING "autotst_dispc: get clk_parent2 fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_parent2 ok!\n");
	}

	clk_parent3 = clk_get(NULL, DISPC_DPI_CLOCK_PARENT);
	if (IS_ERR(clk_parent3)) {
		printk(KERN_WARNING "autotst_dispc: get clk_parent3 fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_parent3 ok!\n");
	}

	clk_parent4 = clk_get(NULL, DISPC_EMC_EN_PARENT);
	if (IS_ERR(clk_parent3)) {
		printk(KERN_WARNING "autotst_dispc: get clk_parent4 fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_parent4 ok!\n");
	}

	autotst_dispc_ctx.clk_dispc = clk_get(NULL, DISPC_PLL_CLK);
	if (IS_ERR(autotst_dispc_ctx.clk_dispc)) {
		printk(KERN_WARNING "autotst_dispc: get clk_dispc fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_dispc ok!\n");
	}

	autotst_dispc_ctx.clk_dispc_dbi = clk_get(NULL, DISPC_DBI_CLK);
	if (IS_ERR(autotst_dispc_ctx.clk_dispc_dbi)) {
		printk(KERN_WARNING "autotst_dispc: get clk_dispc_dbi fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_dispc_dbi ok!\n");
	}

	autotst_dispc_ctx.clk_dispc_dpi = clk_get(NULL, DISPC_DPI_CLK);
	if (IS_ERR(autotst_dispc_ctx.clk_dispc_dpi)) {
		printk(KERN_WARNING "autotst_dispc: get clk_dispc_dpi fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_dispc_dpi ok!\n");
	}

	autotst_dispc_ctx.clk_dispc_emc = clk_get(NULL, DISPC_EMC_CLK);
	if (IS_ERR(autotst_dispc_ctx.clk_dispc_emc)) {
		printk(KERN_WARNING "autotst_dispc: get clk_dispc_dpi fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc: get clk_dispc_emc ok!\n");
	}

	ret = clk_set_parent(autotst_dispc_ctx.clk_dispc, clk_parent1);
	if(ret){
		printk(KERN_ERR "autotst_dispc: dispc set clk parent fail\n");
	}
	ret = clk_set_rate(autotst_dispc_ctx.clk_dispc, DISPC_CLOCK);
	if(ret){
		printk(KERN_ERR "autotst_dispc: dispc set clk parent fail\n");
	}

	ret = clk_set_parent(autotst_dispc_ctx.clk_dispc_dbi, clk_parent2);
	if(ret){
		printk(KERN_ERR "autotst_dispc: dispc set dbi clk parent fail\n");
	}
	ret = clk_set_rate(autotst_dispc_ctx.clk_dispc_dbi, DISPC_DBI_CLOCK);
	if(ret){
		printk(KERN_ERR "autotst_dispc: dispc set dbi clk parent fail\n");
	}

	ret = clk_set_parent(autotst_dispc_ctx.clk_dispc_dpi, clk_parent3);
	if(ret){
		printk(KERN_ERR "autotst_dispc: dispc set dpi clk parent fail\n");
	}
	ret = clk_set_rate(autotst_dispc_ctx.clk_dispc_dpi, DISPC_DPI_CLOCK);
	if(ret){
		printk(KERN_ERR "autotst_dispc: dispc set dpi clk parent fail\n");
	}

	ret = clk_set_parent(autotst_dispc_ctx.clk_dispc_emc, clk_parent4);
	if(ret){
		printk(KERN_ERR "autotst_dispc: dispc set emc clk parent fail\n");
	}

	ret = clk_enable(autotst_dispc_ctx.clk_dispc_emc);
	if(ret){
		printk("autotst_dispc:enable clk_dispc_emc error!!!\n");
		ret=-1;
	}

	ret = dispc_clk_enable(&autotst_dispc_ctx);
	if (ret) {
		printk(KERN_WARNING "autotst_dispc:[%s] enable dispc_clk fail!\n",__FUNCTION__);
		return -1;
	} else {
		pr_debug(KERN_INFO "autotst_dispc:[%s] enable dispc_clk ok!\n",__FUNCTION__);
	}

	dispc_print_clk();

	return 0;
}

static int dispc_clk_disable(struct autotst_dispc_context *dispc_ctx_ptr)
{
	pr_debug(KERN_INFO "autotst_dispc:[%s]\n",__FUNCTION__);
	if(!dispc_ctx_ptr){
		return 0;
	}

	pr_debug(KERN_INFO "autotst_dispc:sprdfb_dispc_clk_disable real\n");
	clk_disable(dispc_ctx_ptr->clk_dispc);
	clk_disable(dispc_ctx_ptr->clk_dispc_dpi);
	clk_disable(dispc_ctx_ptr->clk_dispc_dbi);

	return 0;
}

static void draw_patten(uint32_t* buffer, int buffer_w, int buffer_h, int grid_w, int grid_h)
{
	int h     = 0;
	int v     = 0;
	int i,j;
	int grid_in_width = buffer_h / grid_w;
	unsigned int c = 0;

	for (j = 0; j < buffer_h; j++) {
		for (i = 0; i < buffer_w; i++) {
			h = i / grid_w;
			v = j / grid_h;
			c = g_patten_table[(v*grid_in_width+h) % PATTEN_COLOR_COUNT];
			buffer[j*buffer_w+i] = c;
		}
	}
}

static int dispc_fb_prepare(struct panel_spec *panel)
{
	uint32_t fb_size = 0;// should be ABGR888
	uint32_t *p_pixel = NULL;

	fb_size = panel->width * panel->height * 4;// should be ABGR888

	autotst_dispc_ctx.fb_addr_v = __get_free_pages(GFP_ATOMIC | __GFP_ZERO , get_order(fb_size));
	if (!autotst_dispc_ctx.fb_addr_v) {
		printk(KERN_ERR "sprdfb: %s Failed to allocate frame buffer\n", __FUNCTION__);
		return -1;
	}
	printk(KERN_INFO "sprdfb:  got %d bytes frame buffer at 0x%x\n", fb_size,
		autotst_dispc_ctx.fb_addr_v);

	autotst_dispc_ctx.fb_addr_p = __pa(autotst_dispc_ctx.fb_addr_v);
	autotst_dispc_ctx.fb_size = fb_size;

	p_pixel = autotst_dispc_ctx.fb_addr_v;
	draw_patten(p_pixel, panel->width, panel->height, PATTEN_GRID_WIDTH, PATTEN_GRID_HEIGHT);

	return 0;
}

void dispc_dump(void)
{
    int i = 0;
    for(i=0;i<256;i+=16){
        printk("autotst_dispc: %x: 0x%x, 0x%x, 0x%x, 0x%x\n", i, dispc_read(i),
            dispc_read(i+4),
            dispc_read(i+8),
            dispc_read(i+12));
    }
    printk("**************************\n");
}

int32_t autotst_dispc_uninit(int display_type)
{
	printk(KERN_INFO "autotst_dispc:[%s]\n",__FUNCTION__);

	dispc_stop();
	dispc_clk_disable(&autotst_dispc_ctx);
	if(DISPLAY_TYPE_MIPI == display_type){
		autotst_dsi_uninit();
	}
	if(0 != autotst_dispc_ctx.fb_addr_v){
		free_pages(autotst_dispc_ctx.fb_addr_v, get_order(autotst_dispc_ctx.fb_size));
	}
	autotst_dispc_ctx.fb_addr_v = 0;
	autotst_dispc_ctx.fb_addr_p = 0;
	autotst_dispc_ctx.fb_size = 0;
	return 0;
}

int32_t autotst_dispc_init(int display_type)
{
	int ret = 0;

	printk(KERN_INFO "autotst_dispc:[%s]\n",__FUNCTION__);

        //autotst_dsi_dump();

        autotst_dispc_uninit(DISPLAY_TYPE_MIPI);

	ret = dispc_clk_init();
	if(ret){
		printk(KERN_WARNING "autotst_dispc: dispc_clk_init fail!\n");
		return -1;
	}

	dispc_reset();
	dispc_module_enable();

	/*set bg color*/
	dispc_set_bg_color(0xFFFFFFFF);
	/*enable dithering*/
	dispc_dithering_enable(true);
	/*use MSBs as img exp mode*/
	dispc_set_exp_mode(0x0);
	//enable DISPC Power Control
	dispc_pwr_enable(true);

	switch(display_type){
	case DISPLAY_TYPE_MCU:
		autotst_panel = &lcd_dummy_mcu_spec;
		mcu_dispc_init_config(autotst_panel);
		mcu_dispc_set_timing(autotst_panel);
		break;
	case DISPLAY_TYPE_RGB:
		autotst_panel = &lcd_dummy_rgb_spec;
		rgb_dispc_init_config(autotst_panel);
		rgb_dispc_set_timing(autotst_panel);
		autotst_dispc_ctx.dispc_if = DISPC_IF_DPI;
		break;
	case DISPLAY_TYPE_MIPI:
		autotst_panel = &lcd_dummy_mipi_spec;
		mipi_dispc_init_config(autotst_panel);
		mipi_dispc_set_timing(autotst_panel);
		autotst_dsi_init(autotst_panel);
		break;
	default:
		printk("autotst_dispc:[%s] error display type (%d)\n", __FUNCTION__, display_type);
	}

	dispc_layer_init(autotst_panel);

	if(DISPC_IF_DPI == autotst_dispc_ctx.dispc_if){
		if(1){
			/*set dpi register update only with SW*/
			dispc_set_bits(BIT(4), DISPC_DPI_CTRL);
		}else{
			/*set dpi register update with SW & VSYNC*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);
		}
		/*enable dispc update done INT*/
		//dispc_write((1<<4), DISPC_INT_EN);
	}else{
		/* enable dispc DONE  INT*/
		//dispc_write((1<<0), DISPC_INT_EN);
	}
	//dispc_set_bits(BIT(2), DISPC_INT_EN);
	return 0;
}

int32_t autotst_dispc_refresh (void)
{
	int ret = 0;
	uint32_t size = (autotst_panel->width& 0xffff) | ((autotst_panel->height) << 16);

	printk(KERN_INFO "autotst_dispc:[%s]\n",__FUNCTION__);

        //autotst_dsi_dump();

	ret = dispc_fb_prepare(autotst_panel);
	if(0 != ret){
		printk("autotst_dispc: refresh fail!(can't prepare frame buffer)!]n");
		return -1;
	}

	dispc_write(autotst_dispc_ctx.fb_addr_p, DISPC_OSD_BASE_ADDR);
	dispc_write(0, DISPC_OSD_DISP_XY);
	dispc_write(size, DISPC_OSD_SIZE_XY);
	dispc_write(autotst_panel->width, DISPC_OSD_PITCH);
	dispc_write(size, DISPC_SIZE_XY);

	dispc_run();

#if 1
	pr_debug("DISPC_CTRL: 0x%x\n", dispc_read(DISPC_CTRL));
	pr_debug("DISPC_SIZE_XY: 0x%x\n", dispc_read(DISPC_SIZE_XY));
	pr_debug("DISPC_BG_COLOR: 0x%x\n", dispc_read(DISPC_BG_COLOR));
	pr_debug("DISPC_INT_EN: 0x%x\n", dispc_read(DISPC_INT_EN));
	pr_debug("DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("DISPC_OSD_BASE_ADDR: 0x%x\n", dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("DISPC_OSD_ALPHA	: 0x%x\n", dispc_read(DISPC_OSD_ALPHA));
#else
        mdelay(20);
        dispc_dump();
        autotst_dsi_dump();
        autotst_dsi_dump1();
#endif
        return 0;
}

/**********************************************/
/*                          MCU OPERATION                                  */
/**********************************************/
int32_t autotst_dispc_mcu_send_cmd(uint32_t cmd)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5));

	dispc_write(cmd, DISPC_DBI_CMD);

	return 0;
}

int32_t autotst_dispc_mcu_send_cmd_data(uint32_t cmd, uint32_t data)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5));

	dispc_write(cmd, DISPC_DBI_CMD);

	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5));

	dispc_write(data, DISPC_DBI_DATA);

	return 0;
}

int32_t autotst_dispc_mcu_send_data(uint32_t data)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5));

	dispc_write(data, DISPC_DBI_DATA);

	return 0;
}

uint32_t autotst_dispc_mcu_read_data(void)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5));
	dispc_write(1 << 24, DISPC_DBI_DATA);
	udelay(50);
	return dispc_read(DISPC_DBI_RDATA);
}


