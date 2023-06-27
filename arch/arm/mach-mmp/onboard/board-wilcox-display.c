#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/regulator/machine.h>
#include <linux/lcd.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/mfp-mmp2.h>
#include <mach/mmp3.h>
#include <mach/pxa988.h>
#include <mach/pxa168fb.h>
#include <mach/regs-mcu.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#if defined(CONFIG_CPU_PXA1088)
#if defined(CONFIG_MACH_WILCOX)
#include <mach/mfp-pxa1088-wilcox.h>
#elif defined(CONFIG_MACH_BAFFINQ)
#include <mach/mfp-pxa1088-baffinq.h>
#else
#include <mach/mfp-pxa1088-delos.h>
#endif
#elif defined(CONFIG_CPU_PXA988)
#include <mach/mfp-pxa988-aruba.h>
#endif
#include "../common.h"
#include <mach/features.h>

#define ID_S6D78A0X	(0x534c10)
#define ID_HX8389B	(0x534490)

#define BOARD_ID_REV05 (0x5)
#define GPIO33_LCD_IO_1_8V_EN (33)
#define GPIO18_LCD_RESET (18)
#ifdef CONFIG_MACH_WILCOX_CMCC
#define BOARD_ID_CMCC_REV02 (0x2)
#endif

u32 panel_id = 0;
int dsi_init(struct pxa168fb_info *fbi);
extern struct pxa168fb_mipi_lcd_driver hx8389b_lcd_driver;
extern struct pxa168fb_mipi_lcd_driver s6d78a0x_lcd_driver;
static int lcd_power(struct pxa168fb_info *, unsigned int, unsigned int, int);

static struct dsi_info dsiinfo = {
	.id = 1,
	.lanes = 3,
	.bpp = 24,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_888,
	.burst_mode = DSI_BURST_MODE_BURST,
	.master_mode = 1,
	.hbp_en = 1,
	.hfp_en = 1,
};

struct fb_videomode wilcox_hx8389b_video_modes[] = {
	/* HX8389B */
	[0] = {
		.refresh = 60,
		.xres = 540,
		.yres = 960,
		.hsync_len = 20, //HWD
		.left_margin = 80, //HBP
		.right_margin = 20, //HFP
		.vsync_len = 1,  //VWD
		.upper_margin = 6, //VBP
		.lower_margin = 18, //VFP
		.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};

static struct fb_videomode wilcox_s6d78a0x_video_modes[] = {
	/* S6D78A0X */
	[0] = {
		.refresh = 60,
		.xres = 540,
		.yres = 960,
		.hsync_len = 16, //HWD
		.left_margin = 10, //HBP
		.right_margin = 90, //HFP
		.vsync_len = 8,  //VWD
		.upper_margin = 8, //VBP
		.lower_margin = 16, //VFP
		.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};

static struct pxa168fb_mach_info mipi_lcd_info = {
	.id = "GFX Layer",
	.num_modes = 1,
	.modes = wilcox_hx8389b_video_modes,
	.sclk_src = 312000000,
	.sclk_div = 0x40001208,
	.pix_fmt = PIX_FMT_RGBA888,
	.isr_clear_mask	= LCD_ISR_CLEAR_MASK_PXA168,
	/*
	 * don't care about io_pin_allocation_mode and dumb_mode
	 * since the panel is hard connected with lcd panel path and
	 * dsi1 output
	 */
	.io_pad_ctrl = CFG_CYC_BURST_LEN16,
	.panel_rgb_reverse_lanes = 0,
	.invert_composite_blank = 0,
	.invert_pix_val_ena = 0,
	.invert_pixclock = 0,
	.panel_rbswap = 0,
	.active = 1,
	.enable_lcd = 1,
	.spi_gpio_cs = -1,
	.spi_gpio_reset = -1,
	.mmap = 1,
	.phy_type = DSI,
	.phy_init = dsi_init,
	.phy_info = &dsiinfo,
	.lcd_driver = NULL, 
	.pxa168fb_lcd_power = lcd_power,
	.exter_brige_init = NULL,
	.exter_brige_pwr = NULL,
	.dsi_panel_config = NULL,
	.width = 56,
	.height = 100,
};

static struct pxa168fb_mach_info mipi_lcd_ovly_info = {
	.id = "Video Layer",
	.num_modes = 0,
	.modes = NULL,
	.pix_fmt = PIX_FMT_RGB565,
	.io_pad_ctrl = CFG_CYC_BURST_LEN16,
	.panel_rgb_reverse_lanes = 0,
	.invert_composite_blank = 0,
	.invert_pix_val_ena = 0,
	.invert_pixclock = 0,
	.panel_rbswap = 0,
	.active = 1,
	.enable_lcd = 1,
	.spi_gpio_cs = -1,
	.spi_gpio_reset = -1,
	.mmap = 0,
	.phy_type = DSI,
	.phy_init = NULL,
	.phy_info = &dsiinfo,
	.pxa168fb_lcd_power = NULL,
	.exter_brige_init = NULL,
	.exter_brige_pwr = NULL,
	.dsi_panel_config = NULL,
};

static int __init panel_id_setup(char *str)
{
	int n;
	if (!get_option(&str, &n)) {
		pr_err("get_lcd_id: can't get lcd_id from cmdline\n");
		return 0;
	}

	panel_id = n;
	pr_info("panel_id = 0x%x\n", panel_id);
	return 1;
}
__setup("lcd_id=", panel_id_setup);

int get_panel_id(void)
{
	return panel_id;
}

#define     FHD_PANEL   1
static int fhd_lcd = 0;
static int __init fhd_lcd_setup(char *str)
{
	int en;
	if(!get_option(&str, &en))
		return 0;
	fhd_lcd = en;
	return 1;
}

__setup("fhd_lcd=", fhd_lcd_setup);

int is_fhd_lcd(void)
{
	return fhd_lcd;
}

/*
 * FIXME:add qhd_lcd to indicate if use qhd or use hvga_vnc
 */
#define QHD_PANEL 1
static int qhd_lcd;
static int __init qhd_lcd_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	qhd_lcd = n;
	return 1;
}
__setup("qhd_lcd=", qhd_lcd_setup);

static int is_qhd_lcd(void)
{
	return qhd_lcd;
}

static struct fb_videomode video_modes_emeidkb[] = {
	/* HX8389B */
	[0] = {
		.refresh = 60,
		.xres = 540,
		.yres = 960,
		.hsync_len = 20, //HWD
		.left_margin = 80, //HBP
		.right_margin = 20, //HFP
		.vsync_len = 1,  //VWD
		.upper_margin = 6, //VBP
		.lower_margin = 18, //VFP
		.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	/* S6D78A0X */
	/*
	[1] = {
		.refresh = 60,
		.xres = 540,
		.yres = 960,
		.hsync_len = 16, //HWD
		.left_margin = 10, //HBP
		.right_margin = 90, //HFP
		.vsync_len = 8,  //VWD
		.upper_margin = 8, //VBP
		.lower_margin = 16, //VFP
		.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	*/
};

/* emeidkb: only DSI1 and use lane0,lane1 */
static struct dsi_info emeidkb_dsiinfo = {
	.id = 1,
	.lanes = 3,
	.bpp = 24,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_888,
	.burst_mode = DSI_BURST_MODE_BURST,
	.hbp_en = 1,
	.hfp_en = 1,
};

#define     DSI1_BITCLK(div)			((div)<<8)
#define     DSI1_BITCLK_DIV_MASK		0x00000F00
#define     CLK_INT_DIV(div)			(div)
#define     CLK_INT_DIV_MASK			0x000000FF
static void calculate_dsi_clk(struct pxa168fb_mach_info *mi)
{
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct fb_videomode *modes = &mi->modes[0];
	u32 total_w, total_h, pclk2bclk_rate, byteclk, bitclk,
	    pclk_div, bitclk_div = 1;

	if (!di)
		return;

	/*
	 * When DSI is used to refresh panel, the timing configuration should
	 * follow the rules below:
	 * 1.Because Async fifo exists between the pixel clock and byte clock
	 *   domain, so there is no strict ratio requirement between pix_clk
	 *   and byte_clk, we just need to meet the following inequation to
	 *   promise the data supply from LCD controller:
	 *   pix_clk * (nbytes/pixel) >= byte_clk * lane_num
	 *   (nbyte/pixel: the real byte in DSI transmission)
	 *   a)16-bit format n = 2; b) 18-bit packed format n = 18/8 = 9/4;
	 *   c)18-bit unpacked format  n=3; d)24-bit format  n=3;
	 *   if lane_num = 1 or 2, we can configure pix_clk/byte_clk = 1:1 >
	 *   lane_num/nbytes/pixel
	 *   if lane_num = 3 or 4, we can configure pix_clk/byte_clk = 2:1 >
	 *   lane_num/nbytes/pixel
	 * 2.The horizontal sync for LCD is synchronized from DSI,
	 *    so the refresh rate calculation should base on the
	 *    configuration of DSI.
	 *    byte_clk = (h_total * nbytes/pixel) * v_total * fps / lane_num;
	 */
	total_w = modes->xres + modes->left_margin +
		modes->right_margin + modes->hsync_len;
	total_h = modes->yres + modes->upper_margin +
		modes->lower_margin + modes->vsync_len;

	pclk2bclk_rate = (di->lanes > 2) ? 2 : 1;
	byteclk = ((total_w * (di->bpp >> 3)) * total_h *
			modes->refresh) / di->lanes;
	bitclk = byteclk << 3;

	/* The minimum of DSI pll is 150MHz */
	if (bitclk < 150000000)
		bitclk_div = 150000000 / bitclk + 1;

	//bitclk_div *= 2;

	mi->sclk_src = bitclk * bitclk_div;
	/*
	 * mi->sclk_src = pclk * pclk_div;
	 * pclk / bitclk  = pclk / (8 * byteclk) = pclk2bclk_rate / 8;
	 * pclk_div / bitclk_div = 8 / pclk2bclk_rate;
	 */
	pclk_div = (bitclk_div << 3) / pclk2bclk_rate;

	mi->sclk_div &= ~(DSI1_BITCLK_DIV_MASK | CLK_INT_DIV_MASK);
	mi->sclk_div |= DSI1_BITCLK(bitclk_div) | CLK_INT_DIV(pclk_div);
}

static void calculate_lvds_clk(struct pxa168fb_mach_info *mi)
{
	struct fb_videomode *modes = &mi->modes[0];
	u32 total_w, total_h, pclk, div, use_pll1;

	total_w = modes->xres + modes->left_margin +
		modes->right_margin + modes->hsync_len;
	total_h = modes->yres + modes->upper_margin +
		modes->lower_margin + modes->vsync_len;

	pclk = total_w * total_h * modes->refresh;

	/*
	 * use pll1 by default
	 * we could set a more flexible clocking options by selecting pll3
	 */
#ifdef CONFIG_MACH_WILCOX
		/* src clock is 800MHz */
		div = 800000000 / pclk;
		if (div * pclk < 800000000)
			div++;
		mi->sclk_src = 800000000;
		mi->sclk_div = 0x20000000 | div;
#else
		div = 150000000 / pclk;
		if (div * pclk < 150000000)
			div++;
		mi->sclk_src = pclk * div;
		mi->sclk_div = 0xe0000000 | div;
#endif

	pr_debug("\n%s sclk_src %d sclk_div 0x%x\n", __func__,
			mi->sclk_src, mi->sclk_div);
}

static void calculate_lcd_sclk(struct pxa168fb_mach_info *mi)
{

	if (mi->phy_type & (DSI | DSI2DPI))
		calculate_dsi_clk(mi);
	else if (mi->phy_type & LVDS)
		calculate_lvds_clk(mi);
	else
		return;
}

static void dither_config(struct pxa168fb_mach_info *mi)
{
	struct dsi_info *dsi;
	int bpp;

	if (mi->phy_type == LVDS) {
		struct lvds_info *lvds = (struct lvds_info *)mi->phy_info;
		bpp = (lvds->fmt == LVDS_FMT_18BIT) ? 18 : 24;
	} else {
		dsi = (struct dsi_info *)mi->phy_info;
		if (!dsi)
			return;
		bpp = dsi->bpp;
	}

	if (bpp < 24) {
		mi->dither_en = 1;
		/*
		 * dither table was related to resolution
		 * 4x4 table could be select for all cases.
		 * we can select 4x8 table if xres is much
		 * bigger than yres
		 */
		mi->dither_table = DITHER_TBL_4X4;
		if (bpp == 18)
			mi->dither_mode = DITHER_MODE_RGB666;
		else if (bpp == 16)
			mi->dither_mode = DITHER_MODE_RGB565;
		else
			mi->dither_mode = DITHER_MODE_RGB444;
	}
}

static void set_maxfb_size(struct pxa168fb_mach_info *mi,
		int xalign, int yalign)
{
	u32 bits_per_pixel, fb_size, xres_virtual, yres_virtual, buf_count;
	mi->xres_alignment = xalign;
	mi->yres_alignment = yalign;

	if (xalign)
		xres_virtual = ALIGN(mi->modes->xres, xalign);
	else
		xres_virtual = mi->modes->xres;

	if (yalign)
		yres_virtual = ALIGN(mi->modes->yres, yalign);
	else
		yres_virtual = mi->modes->yres;

	switch (mi->pix_fmt) {
	case PIX_FMT_RGBA888:
	case PIX_FMT_BGRA888:
	case PIX_FMT_RGB888A:
	case PIX_FMT_BGR888A:
	case PIX_FMT_RGB888UNPACK:
	case PIX_FMT_BGR888UNPACK:
	case PIX_FMT_YUV422PACK_IRE_90_270:
		bits_per_pixel = 32;
		break;
	case PIX_FMT_RGB565:
	case PIX_FMT_BGR565:
	case PIX_FMT_RGB1555:
	case PIX_FMT_BGR1555:
	case PIX_FMT_YUYV422PACK:
	case PIX_FMT_YVU422PACK:
	case PIX_FMT_YUV422PACK:
	case PIX_FMT_YUV422PLANAR:
	case PIX_FMT_YVU422PLANAR:
		bits_per_pixel = 16;
		break;
	case PIX_FMT_RGB888PACK:
	case PIX_FMT_BGR888PACK:
		bits_per_pixel = 24;
		break;
	case PIX_FMT_PSEUDOCOLOR:
		bits_per_pixel = 8;
		break;
	case PIX_FMT_YUV420PLANAR:
	case PIX_FMT_YVU420PLANAR:
	case PIX_FMT_YUV420SEMIPLANAR:
	case PIX_FMT_YVU420SEMIPLANAR:
		bits_per_pixel = 12;
		break;
	default:
		bits_per_pixel = 32;
		break;
	}

	fb_size = xres_virtual * yres_virtual *  bits_per_pixel >> 3;
	/* there should be at least double buffer for ping-pang. */
	buf_count = (mi->mmap > 1) ? (mi->mmap) : 2;
	mi->max_fb_size = fb_size * buf_count + 4096;
}

static int lcd_reset(struct pxa168fb_info *fbi)
{
	if (gpio_request(GPIO18_LCD_RESET, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", GPIO18_LCD_RESET);
		return -EIO;
	}
	gpio_direction_output(GPIO18_LCD_RESET, 1);
	mdelay(1);
	gpio_direction_output(GPIO18_LCD_RESET, 0);
	mdelay(1);
	gpio_direction_output(GPIO18_LCD_RESET, 1);
	msleep(120);

	gpio_free(GPIO18_LCD_RESET);

	return 0;
}

static int lcd_power(struct pxa168fb_info *fbi,
		unsigned int spi_gpio_cs,
		unsigned int spi_gpio_reset, int on)
{
	static struct regulator *lcd_avdd;
	int board_id = get_board_id();

	printk("%s, power %s +\n", __func__, on ? "on" : "off");
#ifdef CONFIG_MACH_WILCOX_CMCC
	if (board_id >= BOARD_ID_CMCC_REV02) {
#else
	if (board_id >= BOARD_ID_REV05) {
#endif
		if (gpio_request(GPIO33_LCD_IO_1_8V_EN, "LCD_IO_1.8V EN")) {
			pr_err("gpio %d request failed\n", GPIO33_LCD_IO_1_8V_EN);
			return -EIO;
		}
	}

	if (!lcd_avdd) {
		lcd_avdd = regulator_get(NULL, "v_lcd_3V");
		if (IS_ERR(lcd_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_avdd;
		}
	}

	if (on) {
#ifdef CONFIG_MACH_WILCOX_CMCC
	if (board_id >= BOARD_ID_CMCC_REV02) {
#else
	if (board_id >= BOARD_ID_REV05) {
#endif
			gpio_direction_output(GPIO33_LCD_IO_1_8V_EN, 1);
		}
		regulator_set_voltage(lcd_avdd, 3000000, 3000000);
		regulator_enable(lcd_avdd);
		mdelay(5);
	} else {
		if (gpio_request(GPIO18_LCD_RESET, "lcd reset gpio")) {
			pr_err("gpio %d request failed\n", GPIO18_LCD_RESET);
			return -EIO;
		}

		gpio_direction_output(GPIO18_LCD_RESET, 0);		
		mdelay(3);
		gpio_free(GPIO18_LCD_RESET);
		
#ifdef CONFIG_MACH_WILCOX_CMCC
	if (board_id >= BOARD_ID_CMCC_REV02) {
#else
	if (board_id >= BOARD_ID_REV05) {
#endif
			regulator_disable(lcd_avdd);
			gpio_direction_output(GPIO33_LCD_IO_1_8V_EN, 0);
		}
	}
#ifdef CONFIG_MACH_WILCOX_CMCC
	if (board_id >= BOARD_ID_CMCC_REV02) {
#else
	if (board_id >= BOARD_ID_REV05) {
#endif
		gpio_free(GPIO33_LCD_IO_1_8V_EN);
	}
	printk("%s, power %s -\n", __func__, on ? "on" : "off");

	return 0;

regu_lcd_avdd:
	regulator_put(lcd_avdd);    
	lcd_avdd = NULL;
#ifdef CONFIG_MACH_WILCOX_CMCC
	if (board_id >= BOARD_ID_CMCC_REV02) {
#else
	if (board_id >= BOARD_ID_REV05) {
#endif
		gpio_free(GPIO33_LCD_IO_1_8V_EN);
	}

	return -EIO;
}

int dsi_init(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct pxa168fb_lcd_platform_driver *lcd_pd = mi->lcd_platform_driver;
	struct pxa168fb_mipi_lcd_driver *lcd_driver = mi->lcd_driver;

	if (fbi->skip_pw_on) {
		printk("%s, skip dsi_init\n", __func__);
		return 0;
	}

	dsi_dphy_exit_ulps_mode(fbi);
	/* reset DSI controller */
	dsi_reset(fbi, 1);
	mdelay(1);

	/* disable continuous clock */
	dsi_cclk_set(fbi, 0);

	/* dsi out of reset */
	dsi_reset(fbi, 0);

	/* turn on DSI continuous clock */
	dsi_cclk_set(fbi, 1);

	/* set dphy */
	dsi_set_dphy(fbi);

	/* put all lanes to LP-11 state  */
	dsi_lanes_enable(fbi, 1);
	mdelay(1);

	lcd_reset(fbi);

	/*  reset the bridge */
	if (mi->exter_brige_pwr) {
		mi->exter_brige_pwr(fbi,1);
		msleep(10);
	}

	lcd_driver->init(fbi);
	lcd_driver->enable(fbi);

	/* set dsi controller */
	dsi_set_controller(fbi);

	/* set dsi to dpi conversion chip */
	if (mi->phy_type == DSI2DPI) {
		int ret = 0;
		ret = mi->exter_brige_init(fbi);
		if (ret < 0)
			pr_err("dsi2dpi_set error!\n");
	}
#endif
	return 0;
}

void __init emeidkb_add_lcd_mipi(void)
{
	unsigned int CSn_NO_COL;
	struct dsi_info *dsi;

	struct pxa168fb_mach_info *mi = &mipi_lcd_info;
	struct pxa168fb_mach_info *ovly = &mipi_lcd_ovly_info;

	switch (panel_id) {
	case ID_S6D78A0X:
		printk("%s, s6d78a0x(id : 0x%x)\n", __func__, panel_id);
		mi->modes = wilcox_s6d78a0x_video_modes;
		mi->lcd_driver = &s6d78a0x_lcd_driver;
		mi->panel_rbswap = 1;
		break;
	case ID_HX8389B:
	default:
		printk("%s, hx8389b(id : 0x%x)\n", __func__, panel_id);
		mi->modes = wilcox_hx8389b_video_modes;
		mi->lcd_driver = &hx8389b_lcd_driver;
		mi->panel_rbswap = 0;
		break;
	}

	if (1 == get_recoverymode())
		mi->mmap = 2;
	else
		mi->mmap = 3;

	/*
	 * align with GPU, xres should be aligned for 16 pixels;
	 * yres should be aligned for 4 lines
	 */
	set_maxfb_size(mi, 16, 4);

	ovly->num_modes = mi->num_modes;
	ovly->modes = mi->modes;
	ovly->max_fb_size = mi->max_fb_size;

	dsi = (struct dsi_info *)mi->phy_info;
	dither_config(mi);

	/* TODO: in which case we use slave mode? */
	if (QHD_PANEL != is_qhd_lcd() && FHD_PANEL != is_fhd_lcd())
		dsi->master_mode = 0; /* dsi use slave mode */

	/* TODO: just code add for new clock and not removed legacy codes */
	mi->no_legacy_clk = 1;
	mi->path_clk_name = "mmp_pnpath";
	mi->phy_clk_name = "mmp_dsi1";
	CSn_NO_COL = __raw_readl(DMCU_VIRT_BASE + DMCU_SDRAM_CFG0_TYPE1) >> 4;
	CSn_NO_COL &= 0xF;
	if (CSn_NO_COL <= 0x2) {
		/*
		 *If DDR page size < 4KB,
		 *select no crossing 1KB boundary check
		 */
		mi->io_pad_ctrl |= CFG_BOUNDARY_1KB;
		ovly->io_pad_ctrl |= CFG_BOUNDARY_1KB;
	}

	/* add frame buffer drivers */
	pxa988_add_fb(mi);

	/* add overlay driver */
	if (!has_feat_video_replace_graphics_dma())
		pxa988_add_fb_ovly(ovly);
}
