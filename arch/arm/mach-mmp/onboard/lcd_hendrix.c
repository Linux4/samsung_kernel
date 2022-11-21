#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
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
#include <linux/ktd_bl.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/power_supply.h>
#include <linux/timer.h>

#include <mach/mfp-pxa986-hendrix.h>

#define mDNIe_TUNING
#define MDNIE_LITE_PORTING
#define BL_TUNE_WITH_TABLE
#define LDI_ESD_DETECT

#if defined (CONFIG_MACH_ARUBA_TD) || (CONFIG_MACH_HENDRIX)

#ifdef LDI_ESD_DETECT
int is_esd_detected = 0;
struct delayed_work esd_work; // ESD self protect
static void esd_work_func(struct work_struct *work);
static u32 panel_id = 0;
static bool lcd_connected = false;
#endif 

static int emeidkb_lcd_reset(void);
static int dsi_init(struct pxa168fb_info *fbi);
static void panel_init_config(struct pxa168fb_info *fbi);
static int get_panel_id(struct pxa168fb_info *fbi);
static void lcd_esd_detect(void);
static int isReadyTo_mDNIe = 1;
static struct mutex lcdc_mlock;

#ifdef mDNIe_TUNING
#define TUNING_FILE_PATH "/sdcard/mdnie/"
static int tuning_enable;
unsigned char mDNIe_data[113] = {0,};
static char tuning_filename[100];
#endif

struct pxa168fb_info *fbi_global = NULL;
int is_poweron = 1;
int wakeup_brightness;
int lcd_esd_irq = 0;
struct workqueue_struct *lcd_wq;
struct work_struct lcd_work;
#define HX8369B_PANEL1	0x554890
#define HX8369B_PANEL2	0x554990
#define HX8369B_PANEL_BOE1	0x55C090
#define HX8369B_PANEL_BOE2	0x55BC90

static u32 lcd_ID = 0;

/*
 * dsi bpp : rgb_mode
 *    16   : DSI_LCD_INPUT_DATA_RGB_MODE_565;
 *    24   : DSI_LCD_INPUT_DATA_RGB_MODE_888;
 */
static struct dsi_info dsiinfo = {
	.id = 1,
	.lanes = 4,
	.bpp = 16,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_565,
	.burst_mode = DSI_BURST_MODE_BURST,
	.hbp_en = 1,
	.hfp_en = 1,
};

static struct pxa168fb_mach_info mipi_lcd_info = {
	.id = "GFX Layer",
	.num_modes = 0,
	.modes = NULL,
	.sclk_div = 0xE0001108,
	.pix_fmt = PIX_FMT_RGB565,
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
	.phy_type = DSI2DPI,
	.phy_init = dsi_init,
	.phy_info = &dsiinfo,
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
};

static int dsi_init(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	int ret = 0;
	printk("======================dsi_init : Start :\n");

	if (fbi->skip_pw_on)
		return 0;

	/*if uboot enable display, skip the dsi reset sequence */
	if (!fbi->skip_pw_on) {
		/* reset DSI controller */
		dsi_reset(fbi, 1);
		mdelay(1);

		/* disable continuous clock */
		dsi_cclk_set(fbi, 0);

		/* dsi out of reset */
		dsi_reset(fbi, 0);
	}

	/* turn on DSI continuous clock */
	dsi_cclk_set(fbi, 1);

	/* set dphy */
	dsi_set_dphy(fbi);

	if (!lcd_ID)
		lcd_ID = get_panel_id(fbi);
#if 0
	/* if panel not enabled, init panel settings via dsi */
	if (mi->phy_type == DSI && !fbi->skip_pw_on)
		mi->dsi_panel_config(fbi);
#endif
	/* put all lanes to LP-11 state */
	dsi_lanes_enable(fbi, 0);
	dsi_lanes_enable(fbi, 1);

	/*  reset the bridge */
	if (mi->xcvr_reset) {
		mi->xcvr_reset(fbi);
		mdelay(10);
	}

	/* set dsi controller */
	dsi_set_controller(fbi);

	if (mi->phy_type == DSI && !fbi->skip_pw_on)
		mi->dsi_panel_config(fbi);

	/* set dsi to dpi conversion chip */
	if (mi->phy_type == DSI2DPI) {
		ret = mi->dsi2dpi_set(fbi);
		if (ret < 0)
			pr_err("dsi2dpi_set error!\n");
	}
#endif
	return 0;
}

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
	use_pll1 = 1;
	if (use_pll1) {
		/* src clock is 800MHz */
		div = 800000000 / pclk;
		if (div * pclk < 800000000)
			div++;
		mi->sclk_src = 800000000;
		mi->sclk_div = 0x20000000 | div;
	} else {
		div = 150000000 / pclk;
		if (div * pclk < 150000000)
			div++;
		mi->sclk_src = pclk * div;
		mi->sclk_div = 0xe0000000 | div;
	}

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
	struct lvds_info *lvds;
	struct dsi_info *dsi;
	int bpp;

	if (mi->phy_type == LVDS) {
		lvds = (struct lvds_info *)mi->phy_info;
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
#endif

#if defined(CONFIG_MACH_ARUBA_TD) || (CONFIG_MACH_HENDRIX)
/*
 * FIXME:add qhd_lcd to indicate if use qhd or use hvga_vnc
 */
#define	QHD_PANEL	1
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

static struct fb_videomode video_modes_hvga_vnc_emeidkb[] = {

	/* lpj032l001b HVGA mode info */
	[0] = {
		.refresh        = 60,
		.xres           = 320,
		.yres           = 480,
		.hsync_len      = 10,
		.left_margin    = 15,
		.right_margin   = 10,
		.vsync_len      = 2,
		.upper_margin   = 4,
		.lower_margin   = 2,
		.sync		= 0,
	},
};

static struct fb_videomode video_modes_emeidkb[] = {
	[0] = {
		.refresh = 60,
		.xres = 480,
		.yres = 800,
		.hsync_len = 32,
		.left_margin = 85,//150,
		.right_margin = 85,//200,
		.vsync_len = 3,
		.upper_margin = 20,
		.lower_margin = 20,
		.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};


/* emeidkb: only DSI1 and use lane0,lane1 */
static struct dsi_info emeidkb_dsiinfo = {
	.id = 1,
	.lanes = 2,
	.bpp = 24,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_888,
	.burst_mode = DSI_BURST_MODE_BURST,
	.hbp_en = 1,
	.hfp_en = 1,
};

#define MAX_CMDS_LENGTH	256

#define LCD_SLEEP_OUT_DELAY	120
#define LCD_DISP_ON_DELAY	100
#define LCD_DISP_OFF_DELAY	0
#define LCD_SLEEP_IN_DELAY	0

static char exit_sleep[] = {0x11,0x00};
static char display_on[] = {0x29,0x00};

static char enter_sleep[] = {0x10,0x00};
static char display_off[] = {0x28,0x00};

static char pkt_size_cmd[] = {0x1};
static char read_id1[] = {0xda};
static char read_id2[] = {0xdb};
static char read_id3[] = {0xdc};
static char read_esd[] = {0x0a};

static char set_bl_brightness[] = {0x51, 0xFF};
static char read_bl_brightness[] = {0x52};
static char set_bl_ctrl[] = {0x53, 0x24};
static char read_bl_ctrl[] = {0x54};
static char adaptive_bl_ctrl[] = {0x55, 0x00};
static char set_cabc[] = {0xC9, 0x0F, 0x00};

static char hx8369b_boe_01[] = {
	0xB9,
	0xFF, 0x83, 0x69
};

static char hx8369b_boe_02[] = {
      0xBA,
      0x31, 0x00, 0x16, 0xCA, 0xB1, 0x0A, 0x00, 0x28, 0x02, 0x21,
      0x21, 0x9A, 0x1A, 0x8F
};

static char hx8369b_boe_03[] = {
	0xD5,
	0x00, 0x00, 0x0F, 0x03, 0x36, 0x00, 0x00, 0x10, 0x01, 0x00,
	0x00, 0x00, 0x1A, 0x50, 0x45, 0x00, 0x00, 0x13, 0x44, 0x39,
	0x47, 0x00, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x03, 0x00, 0x00, 0x08, 0x88, 0x88, 0x37, 0x5F,
	0x1E, 0x18, 0x88, 0x88, 0x85, 0x88, 0x88, 0x40, 0x2F, 0x6E,
	0x48, 0x88, 0x88, 0x80, 0x88, 0x88, 0x26, 0x4F, 0x0E, 0x08,
	0x88, 0x88, 0x84, 0x88, 0x88, 0x51, 0x3F, 0x7E, 0x58, 0x88,
	0x88, 0x81, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07,
	0xF8, 0x0F, 0xFF, 0xFF, 0x07, 0xF8, 0x0F, 0xFF, 0xFF, 0x00,
	0x00, 0x5A
};

static char hx8369b_boe_04[] = {
	0x3A,
	0x70
};

static char hx8369b_boe_05[] = {
	0x36,
	0x00
};

static char hx8369b_boe_06[] = {
	0xB5,
	0x12, 0x12
};

static char hx8369b_boe_07[] = {
	0xB1,
	0x12, 0x83, 0x77, 0x00, 0x12, 0x12, 0x1E, 0x1E, 0x0C, 0x1A
};

static char hx8369b_boe_08[] = {
	0xB3,
	0x83, 0x00, 0x3A, 0x17
};

static char hx8369b_boe_09[] = {
	0xB4,
	0x00
};

static char hx8369b_boe_10[] = {
	0xB6,
	0xBD, 0xBC, 0x00
};

static char hx8369b_boe_11[] = {
	0xE3,
	0x0F, 0x0F, 0x0F, 0x0F
};

static char hx8369b_boe_12[] = {
	0xC0,
	0x73, 0x50, 0x00, 0x34, 0xC4, 0x00
};

static char hx8369b_boe_13[] = {
	0xC1,
	0x01, 0x00, 0x0A, 0x15, 0x1F, 0x2A, 0x35, 0x42, 0x4C, 0x57,
	0x60, 0x6A, 0x73, 0x7B, 0x83, 0x8A, 0x91, 0x99, 0xA0, 0xA7,
	0xAD, 0xB4, 0xBA, 0xC0, 0xC6, 0xCC, 0xD2, 0xD8, 0xDF, 0xE6,
	0xEC, 0xF3, 0xF8, 0xFF, 0x22, 0xE0, 0x30, 0x4B, 0x43, 0x01,
	0xAF, 0x0A, 0xC0, 0x00, 0x09, 0x13, 0x1C, 0x26, 0x2F, 0x37,
	0x41, 0x49, 0x52, 0x5B, 0x64, 0x6C, 0x74, 0x7C, 0x84, 0x8B,
	0x92, 0x9B, 0xA2, 0xA9, 0xB0, 0xB7, 0xBD, 0xC3, 0xCA, 0xD1,
	0xD8, 0xE0, 0xE8, 0xEF, 0xF8, 0xFF, 0x22, 0x58, 0xBD, 0xF4,
	0x5A, 0xA5, 0xFA, 0x4C, 0xC0, 0x00, 0x07, 0x0F, 0x16, 0x1E,
	0x25, 0x2D, 0x34, 0x3C, 0x45, 0x4E, 0x57, 0x5F, 0x68, 0x70,
	0x78, 0x80, 0x87, 0x8E, 0x96, 0x9D, 0xA4, 0xAB, 0xB3, 0xB9,
	0xC0, 0xC8, 0xCF, 0xD7, 0xE1, 0xE9, 0xF4, 0xFF, 0x22, 0x22,
	0x17, 0xC9, 0x1C, 0xAD, 0xF7, 0xC9
};

static char hx8369b_boe_14[] = {
	0xC6,
	0x41, 0xFF, 0x7D
};

static char hx8369b_boe_15[] = {
	0xCC,
	0x0C
};

static char hx8369b_boe_16[] = {
	0xEA,
	0x7A
};

static char hx8369b_boe_17[] = {
	0xE0,
	0x00, 0x01, 0x04, 0x10, 0x11, 0x3C, 0x1F, 0x33, 0x04, 0x0E,
	0x10, 0x13, 0x16, 0x14, 0x15, 0x11, 0x15, 0x00, 0x01, 0x04,
	0x10, 0x11, 0x3C, 0x1F, 0x33, 0x04, 0x0E, 0x10, 0x13, 0x16,
	0x14, 0x15, 0x11, 0x15, 0x01
};

static struct dsi_cmd_desc lcd_video_display_init_cmds[] = {
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_01), hx8369b_boe_01},
 	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_02), hx8369b_boe_02},      
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_03), hx8369b_boe_03},
	{DSI_DI_DCS_SWRITE1, 0,    0, sizeof(hx8369b_boe_04), hx8369b_boe_04},
	{DSI_DI_DCS_SWRITE1, 0,    0, sizeof(hx8369b_boe_05), hx8369b_boe_05},
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_06), hx8369b_boe_06},
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_07), hx8369b_boe_07},
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_08), hx8369b_boe_08},
	{DSI_DI_DCS_SWRITE1, 0,    0, sizeof(hx8369b_boe_09), hx8369b_boe_09},
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_10), hx8369b_boe_10},
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_11), hx8369b_boe_11},
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_12), hx8369b_boe_12},
	{DSI_DI_DCS_SWRITE1, 0,    0, sizeof(hx8369b_boe_13), hx8369b_boe_13},
	{DSI_DI_DCS_SWRITE1, 0,    0, sizeof(hx8369b_boe_14), hx8369b_boe_14}, 
	{DSI_DI_DCS_SWRITE1, 0,    0, sizeof(hx8369b_boe_15), hx8369b_boe_15},
	{DSI_DI_DCS_SWRITE1, 0,    0, sizeof(hx8369b_boe_16), hx8369b_boe_16},
	{DSI_DI_DCS_LWRITE,   0,    0, sizeof(hx8369b_boe_17), hx8369b_boe_17},
};

static struct dsi_cmd_desc lcd_set_bl_cmds[] = {
	{DSI_DI_DCS_LWRITE,   0, 0, sizeof(set_cabc), set_cabc},
	{DSI_DI_DCS_SWRITE1, 0, 0, sizeof(adaptive_bl_ctrl), adaptive_bl_ctrl},
	{DSI_DI_DCS_SWRITE1, 0, 0, sizeof(set_bl_ctrl), set_bl_ctrl},
};

static struct dsi_cmd_desc lcd_video_display_on_cmds[] = {
	{DSI_DI_DCS_SWRITE, 0, LCD_SLEEP_OUT_DELAY, sizeof(exit_sleep), exit_sleep},
	{DSI_DI_DCS_SWRITE, 0, LCD_DISP_ON_DELAY,     sizeof(display_on), display_on},
};

static struct dsi_cmd_desc lcd_video_display_off_cmds[] = {
	{DSI_DI_DCS_SWRITE, 0, LCD_DISP_OFF_DELAY,  sizeof(display_off), display_off},
	{DSI_DI_DCS_SWRITE, 0, LCD_SLEEP_IN_DELAY,   sizeof(enter_sleep),enter_sleep},
};

static struct dsi_cmd_desc panel_video_read_id1_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 1, 0, sizeof(pkt_size_cmd), pkt_size_cmd},
	{DSI_DI_DCS_READ, 1, 0, sizeof(read_id1), read_id1},
};

static struct dsi_cmd_desc panel_video_read_id2_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 1, 0, sizeof(pkt_size_cmd), pkt_size_cmd},
	{DSI_DI_DCS_READ, 1, 0, sizeof(read_id2), read_id2},
};

static struct dsi_cmd_desc panel_video_read_id3_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 1, 0, sizeof(pkt_size_cmd), pkt_size_cmd},
	{DSI_DI_DCS_READ, 1, 0, sizeof(read_id3), read_id3},
};

#ifdef LDI_ESD_DETECT
static struct dsi_cmd_desc lcd_video_read_esd_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 0, 0, sizeof(pkt_size_cmd), pkt_size_cmd},
	{DSI_DI_DCS_READ, 0, 0, sizeof(read_esd), read_esd},
};
#endif

#ifdef mDNIe_TUNING
static struct dsi_cmd_desc lcd_video_display_mDNIe_cmds[] = {
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_data), mDNIe_data},
};

#ifdef MDNIE_LITE_PORTING
static char cabc_on[] = {0x55, 0x01};
static char cabc_off[] = {0x55, 0x00};

static struct dsi_cmd_desc lcd_cabc_on_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(cabc_on),cabc_on},
};

static struct dsi_cmd_desc lcd_cabc_off_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(cabc_off),cabc_off},
};
#endif

enum SCENARIO {
	UI_MODE,
	VIDEO_MODE,
	VIDEO_WARM_MODE,
	VIDEO_COLD_MODE,
	CAMERA_MODE,
	NAVI_MODE,
	GALLERY_MODE,
	VT_MODE,
	SCENARIO_MAX,
};

enum OUTDOOR {
	OUTDOOR_OFF,
	OUTDOOR_ON,
	OUTDOOR_MAX,
};

#ifdef MDNIE_LITE_PORTING
 enum CABC {
	CABC_OFF,
	CABC_ON,
	CABC_MAX,
 };
#endif

typedef struct mdnie_config {
	int scenario;
	int negative;
	int outdoor;
#ifdef MDNIE_LITE_PORTING
      int cabc;
      int mode;
      struct device			*dev;
      struct mutex			lock;
#endif
};

struct mdnie_config mDNIe_cfg;

static char mDNIe_UI_MODE[] = {
      0xE6,
      0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
      0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
      0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x02, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x05, 0xc6, 0x1e, 0xd3, 0x1f, 0x67,
      0x1f, 0xc6, 0x04, 0xd3, 0x1f, 0x67, 0x1f, 0xc6, 0x1e, 0xd3,
      0x05, 0x67
};

static char mDNIe_VIDEO_MODE[] = {
      0xE6,
      0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
      0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
      0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4,
      0x1f, 0xdd, 0x04, 0x7f, 0x1f, 0xa4, 0x1f, 0xdd, 0x1f, 0x4c,
      0x04, 0xd7
};

static char mDNIe_VIDEO_WARM_MODE[] = {
      0xE6,
      0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xd0, 0x00, 0xfa, 0x00, 0xff, 0xFF, 0x00, 0x00, 
      0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
      0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4,
      0x1f, 0xdd, 0x04, 0x7f, 0x1f, 0xa4, 0x1f, 0xdd, 0x1f, 0x4c,
      0x04, 0xd7
};

static char mDNIe_VIDEO_COLD_MODE[] = {
      0xE6,
      0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xff, 0x00, 0xf4, 0x00, 0xf4, 0xFF, 0x00, 0x00, 
      0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
      0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4,
      0x1f, 0xdd, 0x04, 0x7f, 0x1f, 0xa4, 0x1f, 0xdd, 0x1f, 0x4c,
      0x04, 0xd7,
};

static char mDNIe_CAMERA_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x04, 0xb5, 0x1f, 0x88, 0x1f, 0xc3,
	0x1f, 0xe9, 0x04, 0x54, 0x1f, 0xc3, 0x1f, 0xe9, 0x1f, 0x88,
	0x04, 0x8f
};

static char mDNIe_NAVI_MODE[] = {
	0xE6,
	0x00
};

static char mDNIe_GALLERY_MODE[] = {
      0xE6,
      0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
      0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
      0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x02, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4, 
      0x1f, 0xdd, 0x04, 0x7f, 0x1f, 0xa4, 0x1f, 0xdd, 0x1f, 0x4c,
      0x04, 0xd7
};

static char mDNIe_VT_MODE[] = {
	0xE6,
	0x00
};

static char mDNIe_NEGATIVE_MODE[] = {
      0xE6,
      0x5A, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff,
      0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff,
      0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x20, 0x00, 0x20, 0x00, 
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
      0x20, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x04, 0x00
};

static char mDNIe_VIDEO_OUTDOOR_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x07, 0xff, 0x00, 0x0a, 0xaf,
	0x0d, 0x99, 0x14, 0x6d, 0x1b, 0x48, 0x2c, 0x05, 0xb4, 0x0f,
	0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xae, 0x0c,
	0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x06, 0x7b, 0x1e, 0x5b, 0x1f, 0x2a,
	0x1f, 0xae, 0x05, 0x28, 0x1f, 0x2a, 0x1f, 0xae, 0x1e, 0x5b,
	0x05, 0xf7
};

static char mDNIe_VIDEO_WARM_OUTDOOR_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xe3, 0x00, 0xf3, 0x00, 0xff, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x07, 0xff, 0x00, 0x0a, 0xaf,
	0x0d, 0x99, 0x14, 0x6d, 0x1b, 0x48, 0x2c, 0x05, 0xb4, 0x0f,
	0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xae, 0x0c,
	0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x06, 0x7b, 0x1e, 0x5b, 0x1f, 0x2a,
	0x1f, 0xae, 0x05, 0x28, 0x1f, 0x2a, 0x1f, 0xae, 0x1e, 0x5b,
	0x05, 0xf7
};

static char mDNIe_VIDEO_COLD_OUTDOOR_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xff, 0x00, 0xed, 0x00, 0xe1, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x07, 0xff, 0x00, 0x0a, 0xaf,
	0x0d, 0x99, 0x14, 0x6d, 0x1b, 0x48, 0x2c, 0x05, 0xb4, 0x0f,
	0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xae, 0x0c,
	0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x06, 0x7b, 0x1e, 0x5b, 0x1f, 0x2a,
	0x1f, 0xae, 0x05, 0x28, 0x1f, 0x2a, 0x1f, 0xae, 0x1e, 0x5b,
	0x05, 0xf7
};

static char mDNIe_CAMERA_OUTDOOR_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x07, 0xff, 0x00, 0x0a, 0xaf,
	0x0d, 0x99, 0x14, 0x6d, 0x1b, 0x48, 0x2c, 0x05, 0xb4, 0x0f,
	0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xae, 0x0c,
	0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x06, 0x7b, 0x1e, 0x5b, 0x1f, 0x2a,
	0x1f, 0xae, 0x05, 0x28, 0x1f, 0x2a, 0x1f, 0xae, 0x1e, 0x5b,
	0x05, 0xf7
};

/* mDNIe */
static struct dsi_cmd_desc aruba_video_display_mDNIe_size[] = {
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_UI_MODE), mDNIe_UI_MODE}
};

static struct dsi_cmd_desc aruba_video_display_mDNIe_scenario_cmds[] = {
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_UI_MODE), mDNIe_UI_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_VIDEO_MODE), mDNIe_VIDEO_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_VIDEO_WARM_MODE),
		mDNIe_VIDEO_WARM_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_VIDEO_COLD_MODE),
		mDNIe_VIDEO_COLD_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_CAMERA_MODE),
		mDNIe_CAMERA_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_NAVI_MODE), mDNIe_NAVI_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_GALLERY_MODE),
		mDNIe_GALLERY_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_VT_MODE), mDNIe_VT_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_NEGATIVE_MODE),
		mDNIe_NEGATIVE_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_VIDEO_OUTDOOR_MODE),
		mDNIe_VIDEO_OUTDOOR_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_VIDEO_WARM_OUTDOOR_MODE),
		mDNIe_VIDEO_WARM_OUTDOOR_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_VIDEO_COLD_OUTDOOR_MODE),
		mDNIe_VIDEO_COLD_OUTDOOR_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_CAMERA_OUTDOOR_MODE),
		mDNIe_CAMERA_OUTDOOR_MODE},
};


static int parse_text(char *src, int len)
{
	int i;
	int data=0, value=0, count=0, comment=0;
	char *cur_position;

	mDNIe_data[count] = 0xE6;
	count++;
	cur_position = src;
	for (i=0; i<len; i++, cur_position++) {
		char a = *cur_position;
		switch (a) {
			case '\r':
			case '\n':
				comment = 0;
				data = 0;
				break;
			case '/':
				comment++;
				data = 0;
				break;
			case '0'...'9':
				if (comment > 1)
					break;
				if (data==0 && a=='0')
					data=1;
				else if (data==2) {
					data=3;
					value = (a-'0')*16;
				}
				else if (data==3) {
					value += (a-'0');
					mDNIe_data[count]=value;
					printk("Tuning value[%d]=0x%02X\n",
							count, value);
					count++;
					data=0;
				}
				break;
			case 'a'...'f':
			case 'A'...'F':
				if (comment > 1)
					break;
				if (data==2) {
					data=3;
					if (a<'a') value = (a-'A'+10)*16;
					else value = (a-'a'+10)*16;
				} else if (data==3) {
					if (a<'a') value += (a-'A'+10);
					else value += (a-'a'+10);
					mDNIe_data[count]=value;
					printk("Tuning value[%d]=0x%02X\n",
							count, value);
					count++;
					data=0;
				}
				break;
			case 'x':
			case 'X':
				if (data==1)
					data=2;
				break;
			default:
				if (comment==1)
					comment = 0;
				data = 0;
				break;
		}
	}

	return count;
}
#if 0
static int build_data(int num)
{
	int count = 0, i;

	/* base + register addr + value + base */
	init_cmds_mDNIe[0][count++] = 4 + 1 + num + 2;
	init_cmds_mDNIe[0][count++] = 0x39;
	init_cmds_mDNIe[0][count++] = 0xE6;
	init_cmds_mDNIe[0][count++] = 1 + num; // register addr + value
	init_cmds_mDNIe[0][count++] = 0x00;
	init_cmds_mDNIe[0][count++] = 0x00;

	for (i = 0; i < num; i++) {
		init_cmds_mDNIe[0][count++] = mDNIe_data[i];
	}

	init_cmds_mDNIe[0][count++] = 0x00;
	init_cmds_mDNIe[0][count++] = 0x00;

	printk("--------------build data--------------------\n");

	for (i = 0; i < init_cmds_mDNIe[0][0]; i++) {
		printk("%x, ", init_cmds_mDNIe[0][i]);
		if(i % 5 == 0)
			printk("\n");
	}
}
#endif

static int load_tuning_data(char *filename)
{
	struct file *filp;
	char	*dp;
	long	l ;
	loff_t  pos;
	int     ret, num;
	mm_segment_t fs;

	printk("[INFO]:%s called loading file name : [%s]\n",
			__func__, filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "[ERROR]:File open failed %d\n", IS_ERR(filp));
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	printk("[INFO]: Loading File Size : %ld(bytes)", l);

	dp = kmalloc(l+10, GFP_KERNEL);
	if (dp == NULL) {
		printk("[ERROR]:Can't not allocmemory for tuning file load\n");
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);
	printk("[INFO] : before vfs_read()\n");
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	printk("[INFO] : after vfs_read()\n");

	if (ret != l) {
        printk("[ERROR] : vfs_read() filed ret : %d\n", ret);
        kfree(dp);
	filp_close(filp, current->files);
	return -1;
	}

	filp_close(filp, current->files);

	set_fs(fs);
	num = parse_text(dp, l);

	if (!num) {
		printk("[ERROR]:Nothing to parse\n");
		kfree(dp);
		return -1;
	}

	printk("[INFO] : Loading Tuning Value's Count : %d", num);

	kfree(dp);
	return num;
}

ssize_t mDNIeTuning_show(struct device *dev,
        struct device_attribute *attr, char *buf)

{
	int ret = 0;
	ret = sprintf(buf, "Tunned File Name : %s\n", tuning_filename);

	return ret;
}

ssize_t mDNIeTuning_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	char *pt;
	char a;
	unsigned long tunning_mode = 0;

	a = *buf;

	if (a=='1') {
		tuning_enable = 1;
		printk("%s:Tuning_enable\n", __func__);
	} else if (a=='0') {
		tuning_enable = 0;
		printk("%s:Tuning_disable\n", __func__);
	} else {
		memset(tuning_filename, 0, sizeof(tuning_filename));
		sprintf(tuning_filename,"%s%s",TUNING_FILE_PATH,buf);
		pt = tuning_filename;
		while (*pt) {
			if (*pt =='\r'|| *pt =='\n') {
				*pt = 0;
				break;
			}
			pt++;
		}
		printk("%s:%s\n", __func__, tuning_filename);
		if (load_tuning_data(tuning_filename) <= 0) {
			printk("[ERROR]:load_tunig_data() failed\n");
			return size;
		}
		if (tuning_enable && mDNIe_data[0] != 0) {
			printk("========================mDNIe!!!!!!!\n");
			pxa168_dsi_cmd_array_tx(fbi_global,
				lcd_video_display_mDNIe_cmds,
				ARRAY_SIZE(lcd_video_display_mDNIe_cmds));
		}
	}
	return size;
}
/* Mode : HS-0, LP-1 */
static void set_mDNIe_Mode(struct mdnie_config *mDNIeCfg, int mode)
{
	int value;

	printk("%s:[mDNIe] negative=%d, isReadyTo_mDNIe(%d)\n", __func__,
			mDNIe_cfg.negative, isReadyTo_mDNIe);
	if (!isReadyTo_mDNIe)
		return;
	msleep(100);
	if (mDNIe_cfg.negative) {
		printk("&s : apply negative color\n", __func__);
#if 0
	aruba_video_display_mDNIe_scenario_cmds[SCENARIO_MAX].lp = mode;
#endif
	pxa168_dsi_cmd_array_tx(fbi_global,
			&aruba_video_display_mDNIe_scenario_cmds[SCENARIO_MAX],
			ARRAY_SIZE(aruba_video_display_mDNIe_size));
	return;
	}

	switch (mDNIeCfg->scenario) {
		case UI_MODE:
		case GALLERY_MODE:
			value = mDNIeCfg->scenario;
			break;

		case VIDEO_MODE:
			if (mDNIeCfg->outdoor == OUTDOOR_ON) {
				value = SCENARIO_MAX + 1;
			} else {
				value = mDNIeCfg->scenario;
			}
			break;

		case VIDEO_WARM_MODE:
			if (mDNIeCfg->outdoor == OUTDOOR_ON) {
				value = SCENARIO_MAX + 2;
			} else {
				value = mDNIeCfg->scenario;
			}
			break;

		case VIDEO_COLD_MODE:
			if (mDNIeCfg->outdoor == OUTDOOR_ON) {
				value = SCENARIO_MAX + 3;
			} else {
				value = mDNIeCfg->scenario;
			}
			break;

		case CAMERA_MODE:
			if (mDNIeCfg->outdoor == OUTDOOR_ON) {
				value = SCENARIO_MAX + 4;
			} else {
				value = mDNIeCfg->scenario;
			}
			break;

		default:
			value = UI_MODE;
			break;
	};

	printk("%s:[mDNIe] value=%d\n", __func__, value);

	if (mDNIe_cfg.negative && value == UI_MODE)
		return;
	printk("%s:[mDNIe] value=%d ++ \n", __func__, value);
#if 0
	aruba_video_display_mDNIe_scenario_cmds[value].lp = mode;
#endif
	pxa168_dsi_cmd_array_tx(fbi_global,
			&aruba_video_display_mDNIe_scenario_cmds[value],
			ARRAY_SIZE(aruba_video_display_mDNIe_size));
}

ssize_t mDNIeScenario_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "mDNIeScenario_show : %d\n", mDNIe_cfg.scenario);
	return ret;
}

ssize_t mDNIeScenario_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);
	printk("%s:value=%d\n", __func__, value);

	switch (value) {
		case UI_MODE:
		case VIDEO_MODE:
		case VIDEO_WARM_MODE:
		case VIDEO_COLD_MODE:
		case CAMERA_MODE:
		case GALLERY_MODE:
			break;
		default:
			value = UI_MODE;
			break;
	};

	mDNIe_cfg.scenario = value;
	set_mDNIe_Mode(&mDNIe_cfg, 0);
	return size;
}

ssize_t mDNIeOutdoor_show(struct device *dev,
        struct device_attribute *attr, char *buf)

{
	int ret = 0;
	ret = sprintf(buf, "mDNIeOutdoor_show : %d\n", mDNIe_cfg.outdoor);

	return ret;
}

ssize_t mDNIeOutdoor_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);
	printk("%s:value=%d\n", __func__, value);

	mDNIe_cfg.outdoor = value;
	set_mDNIe_Mode(&mDNIe_cfg, 0);
	return size;
}

ssize_t mDNIeNegative_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf,"mDNIeNegative_show : %d\n", mDNIe_cfg.negative);
	return ret;
}

ssize_t mDNIeNegative_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);

	printk("%s:value=%d\n", __func__, value);

	if (value == 1) {
		mDNIe_cfg.negative = 1;
	} else {
		mDNIe_cfg.negative = 0;
	}

	set_mDNIe_Mode(&mDNIe_cfg, 0);
	return size;
}
#endif

#ifdef MDNIE_LITE_PORTING
void set_mdnie_value(struct mdnie_info *mdnie, u8 force)
{



}

static ssize_t mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdnie_config *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->mode);
}

static ssize_t mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_config *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);
 
	dev_info(dev, "%s :: value=%d\n", __func__, value);

 //	if (value >= MODE_MAX) {
 //		value = STANDARD;
 //		return -EINVAL;
 //	}
 
	mutex_lock(&mdnie->lock);
	mdnie->mode = value;
	mutex_unlock(&mdnie->lock);

	set_mDNIe_Mode(&mDNIe_cfg,0);

	return count;
}

void set_cabc_value(struct mdnie_config *mdnie, u8 force)
{
   int ret;

   printk("%s mdnie->cabc = %d \n",__func__, mdnie->cabc);

   if( CABC_ON == mdnie->cabc)
       pxa168_dsi_cmd_array_tx(fbi_global, lcd_cabc_on_cmds,ARRAY_SIZE(lcd_cabc_on_cmds));
   else
       pxa168_dsi_cmd_array_tx(fbi_global, lcd_cabc_off_cmds,ARRAY_SIZE(lcd_cabc_off_cmds));

}

static ssize_t cabc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdnie_config *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->cabc);
}
 
static ssize_t cabc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_config *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (value >= CABC_MAX)
		value = CABC_OFF;

	value = (value) ? CABC_ON : CABC_OFF;

	mutex_lock(&mdnie->lock);
	mdnie->cabc = value;
	mutex_unlock(&mdnie->lock);

	set_cabc_value(mdnie, 0);

	return count;
}

#endif

#ifdef BL_TUNE_WITH_TABLE
struct brt_value{
	int level;				/* Platform setting values */
	int tune_level;	      /* Chip Setting values */
};

struct brt_value bl_table[] = {
   { 20,  3 }, 
   { 25,  8 },
   { 30,  12 },
   { 35,  15 },  
   { 40,  19 }, 
   { 45,  23 }, 
   { 50,  27 },
   { 55,  30 }, 
   { 60,  34 },
   { 65,  37 }, 
   { 70,  41 },
   { 75,  45 },
   { 80,  49 }, 
   { 85,  53 }, 
   { 90,  56 }, 
   { 95,  58 }, 
   { 100,  61 },   
   { 105,  64 },
   { 110,  67 }, 
   { 115,  70 }, 
   { 120,  73 },
   { 125,  76 },
   { 130,  79 },
   { 135,  84 },
   { 140,  87 },
   { 145,  91 },   /* default 145 */
   { 150,  93 },
   { 155,  95 },
   { 160,  99 },
   { 165,  104 },
   { 170,  109 },
   { 175,  114 },
   { 180,  118 },
   { 185,  122 }, 
   { 190,  125 },
   { 195,  129 },
   { 200,  132 },
   { 205,  135 },
   { 210,  138 },
   { 215,  142 },
   { 220,  146 },
   { 225,  149 },
   { 230,  152 },
   { 235,  156 },
   { 240,  160 },
   { 245,  163 },
   { 250,  165 },
   { 255,  167 },
};

#define MAX_BRT_STAGE (int)(sizeof(bl_table)/sizeof(struct brt_value))

int current_tune_level = 0;

#endif

static int hendrix_set_brightness(struct backlight_device *bd)
{
	int bl = bd->props.brightness;

#ifdef BL_TUNE_WITH_TABLE
	int tune_level, i;
#endif

	struct dsi_cmd_desc hendrix_set_brightness_cmd[] = {
		{DSI_DI_DCS_SWRITE1, 0, 0, sizeof(set_bl_brightness), set_bl_brightness},
	};

#ifdef BL_TUNE_WITH_TABLE
      for(i = 0; i < MAX_BRT_STAGE; i++) {
          if(bl <= bl_table[i].level ) {
               tune_level = bl_table[i].tune_level;
               break;
           }
      }

      //printk("bl = %d , tune_level = %d \n", bl, tune_level);

      if(current_tune_level == tune_level && tune_level != 3)
          return 0;

      printk("bl = %d , tune_level = %d \n", bl, tune_level);

	set_bl_brightness[1] = (char)tune_level;

      current_tune_level = tune_level;
#else
	set_bl_brightness[1] = (char)bl;
#endif

	pxa168_dsi_cmd_array_tx(fbi_global, hendrix_set_brightness_cmd, ARRAY_SIZE(hendrix_set_brightness_cmd));

      return 0;
}

int ldi_pwm_backlight_control(int brightness)
{
    struct dsi_cmd_desc hendrix_set_brightness_cmd[] = {
		{DSI_DI_DCS_SWRITE1, 0, 0, sizeof(set_bl_brightness), set_bl_brightness},
	};

    set_bl_brightness[1] = (char)brightness;

    pxa168_dsi_cmd_array_tx(fbi_global, hendrix_set_brightness_cmd, ARRAY_SIZE(hendrix_set_brightness_cmd));

}
EXPORT_SYMBOL(ldi_pwm_backlight_control);

static int hendrix_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops hendrix_backlight_ops = {
	.get_brightness = hendrix_get_brightness,
	.update_status = hendrix_set_brightness,
};

static int emeidkb_lcd_power(struct pxa168fb_info *fbi,
			     unsigned int spi_gpio_cs,
			     unsigned int spi_gpio_reset, int on)
{
	static struct regulator *lcd_avdd = NULL;
	int lcd_rst_n;

	printk("[emeidkb_lcd_power]------------start!!!---------\n");
	/* FIXME:lcd reset,use GPIO_1 as lcd reset */
	lcd_rst_n = 18;
	if (fbi_global == NULL) {
		fbi_global = fbi;
		lcd_esd_detect();
#ifdef LDI_ESD_DETECT
             INIT_DELAYED_WORK(&esd_work, esd_work_func); // ESD self protect

             schedule_delayed_work(&esd_work, msecs_to_jiffies(20000));
#endif

	}

	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return -EIO;
	}
#if 0
	if (gpio_request(lcd_b_light, "lcd bl gpio")) {
		pr_err("gpio %d request failed\n", lcd_b_light);
		return -EIO;
	}
#endif
	/* FIXME:LCD_AVDD 3.0v */
	if (lcd_avdd == NULL) {
		lcd_avdd = regulator_get(NULL, "v_lcd_3V");
		if (IS_ERR(lcd_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_avdd;
		}
		regulator_set_voltage(lcd_avdd, 3000000, 3000000);
		regulator_enable(lcd_avdd);
		mdelay(25);
	}

	if (on) {
		/* if uboot enable display, don't reset the panel */
		if (!fbi->skip_pw_on) {
			lcd_avdd = regulator_get(NULL, "v_lcd_3V");
			if (IS_ERR(lcd_avdd)) {
				pr_err("%s regulator get error!\n", __func__);
				goto regu_lcd_avdd;
			}
			regulator_set_voltage(lcd_avdd, 3000000, 3000000);
			regulator_enable(lcd_avdd);
			mdelay(25);
			/* release panel from reset */
			gpio_direction_output(lcd_rst_n, 1);
			udelay(20);
			gpio_direction_output(lcd_rst_n, 0);
			mdelay(5);
			gpio_direction_output(lcd_rst_n, 1);
			mdelay(50);
		}
		printk("[emeidkb_lcd_power]-----------power on!!!---------\n");
	} else {
#if defined(CONFIG_BACKLIGHT_KTD253)
		ktd_backlight_set_brightness(0);
		printk("FET_EN : %d\n", !!gpio_get_value(MFP_PIN_GPIO5));
#endif

#ifdef LDI_ESD_DETECT
		if (lcd_connected)
			cancel_delayed_work_sync(&esd_work);
#endif
		is_poweron = 0;
		isReadyTo_mDNIe = 0;
		dsi_cmd_array_tx(fbi, lcd_video_display_off_cmds,
				ARRAY_SIZE(lcd_video_display_off_cmds));
		/* disable LCD_AVDD 3.1v */
		regulator_disable(lcd_avdd);
		/* set panel reset */
		gpio_direction_output(lcd_rst_n, 0);
		printk("[emeidkb_lcd_power]----------power off!!!---------\n");

		printk("LCD_RESET_N : %d\n", !!gpio_get_value(MFP_PIN_GPIO18));	
	}

	gpio_free(lcd_rst_n);
	pr_debug("%s on %d\n", __func__, on);

	return 0;

regu_lcd_avdd:
	lcd_avdd = NULL;
	regulator_put(lcd_avdd);

	return -EIO;
}

static void lcd_work_func(struct work_struct *work)
{
	printk("lcd_work_func : is_poweron(%d)\n", is_poweron);
	if (is_poweron == 1) {
		printk("lcd_work_func : reset!!!!!!!! starts\n");
		isReadyTo_mDNIe = 0;
		emeidkb_lcd_power(fbi_global, 0, 0, 1);
		dsi_init(fbi_global);
		isReadyTo_mDNIe = 1;
		printk("lcd_work_func : reset!!!!!!!! ends\n");
	}
	enable_irq(lcd_esd_irq);
}

static irqreturn_t lcd_irq_handler(int irq, void *dev_id)
{
	printk("lcd_irq_handler : irq(%d), lcd_irq(%d) : is_poweron(%d)\n",
			irq, lcd_esd_irq, is_poweron);

#if 0
	if (is_poweron == 1) {
		disable_irq_nosync(lcd_esd_irq);
		queue_work(lcd_wq, &lcd_work);
	}
#endif 

	return IRQ_HANDLED;
}

static void lcd_esd_detect(void)
{
	int lcd_esd_n = 20;
	int err;

	lcd_esd_irq = gpio_to_irq(mfp_to_gpio(GPIO020_GPIO_20));

	if (gpio_request(lcd_esd_n, "LCD_ESD_INT GPIO")) {
		printk(KERN_ERR "Failed to request GPIO %d "
				"for LCD_ESD_INT\n", lcd_esd_n);
		return;
	}
	gpio_direction_input(lcd_esd_n);
	gpio_free(lcd_esd_n);
	err = request_irq(lcd_esd_irq, lcd_irq_handler,
			IRQF_DISABLED|IRQ_TYPE_EDGE_RISING,
			"LCD_ESD_INT", NULL);
	if (err) {
		printk("[LCD] request_irq failed for taos\n");
	}

	lcd_wq = create_singlethread_workqueue("lcd_wq");
	if (!lcd_wq)
		printk("[LCD] fail to create lcd_wq\n");
	INIT_WORK(&lcd_work, lcd_work_func);
}


#ifdef LDI_ESD_DETECT
static void esd_work_func(struct work_struct *work)
{

	if (!lcd_connected) {
		printk(KERN_INFO "[LCD] ignore esd : lcd is not connected!!\n");
		return;
	}

	printk("[LCD] %s : is_poweron(%d)\n",__func__, is_poweron);

	if ((is_poweron == 1)&&(fbi_global!=NULL)&&(fbi_global->active))
	{
		struct dsi_buf dbuf;

		dsi_cmd_array_rx(fbi_global, &dbuf, lcd_video_read_esd_cmds, ARRAY_SIZE(lcd_video_read_esd_cmds));

		printk("#### 0Ah:0x%x \n", dbuf.data[0]);

		if (dbuf.data[0] != 0x9C) {
			printk("esd_work_func : reset!!!!!!!! starts\n");
			isReadyTo_mDNIe = 0;
			emeidkb_lcd_power(fbi_global, 0, 0, 1);
			dsi_init(fbi_global);
			isReadyTo_mDNIe = 1;
			printk("esd_work_func : reset!!!!!!!! ends\n");
		}
	}

	if (is_poweron && lcd_connected)
		schedule_delayed_work(&esd_work, msecs_to_jiffies(3000));
}
#endif

static int get_panel_id(struct pxa168fb_info *fbi)
{
	u32 read_id = 0;
#ifdef CONFIG_PXA688_PHY
	struct dsi_buf *dbuf;

	printk("[panel_init_config]-----set_dsi_low_power_mode--------\n");
	set_dsi_low_power_mode(fbi);
	mdelay(20);

	dbuf = kmalloc(sizeof(struct dsi_buf), GFP_KERNEL);

	if (dbuf) {
		dsi_cmd_array_rx(fbi, dbuf, panel_video_read_id1_cmds,
				ARRAY_SIZE(panel_video_read_id1_cmds));
		read_id |= dbuf->data[0] << 16;
		dsi_cmd_array_rx(fbi, dbuf, panel_video_read_id2_cmds,
				ARRAY_SIZE(panel_video_read_id2_cmds));
		read_id |= dbuf->data[0] << 8;
		dsi_cmd_array_rx(fbi, dbuf, panel_video_read_id3_cmds,
				ARRAY_SIZE(panel_video_read_id3_cmds));
		read_id |= dbuf->data[0];
		kfree(dbuf);
		printk("Panel id is 0x%x\n", read_id);
		lcd_ID = read_id;

#ifdef LDI_ESD_DETECT
             if(0 == lcd_ID)
                lcd_connected = false;
             else
                lcd_connected = true; 
#endif
	} else
		printk("%s: can't alloc dsi rx buffer\n", __func__);

	printk("[LCD] %s, lcd_ID(%x)\n", __func__, lcd_ID);

	dsi_lanes_enable(fbi, 1);
#endif
	return read_id;
}

static void panel_init_config(struct pxa168fb_info *fbi)
{
	struct dsi_buf *dbuf;
	u32 read_id = 0;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
#ifdef CONFIG_PXA688_PHY
	psy = power_supply_get_by_name("battery");

	if (psy != NULL && psy->get_property != NULL) {
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
		//printk("[LCD] temp(%d), D5(%x)++\n", val.intval, hx8369b_boe_03[20]);
		if (ret == 0) {
			if (val.intval >= 0) {
				hx8369b_boe_03[20] = 0x40;
			} else {
				hx8369b_boe_03[20] = 0x4C;
			}
			printk("[LCD] temp(%d), D5(%x)---\n",
					val.intval, hx8369b_boe_03[20]);
		} else {
			printk("[LCD] read temperature error\n");
		}
	}

	dsi_cmd_array_tx(fbi, lcd_video_display_init_cmds,
			ARRAY_SIZE(lcd_video_display_init_cmds));

	dsi_cmd_array_tx(fbi, lcd_set_bl_cmds,
			ARRAY_SIZE(lcd_set_bl_cmds));

#ifdef mDNIe_TUNING
	isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg, 1);
	if (tuning_enable && mDNIe_data[0] != 0) {
		printk("-----------panel_init_config-----mDNIe------------\n");
		pxa168_dsi_cmd_array_tx(fbi, lcd_video_display_mDNIe_cmds,
				ARRAY_SIZE(lcd_video_display_mDNIe_cmds));
	}
#endif
	dsi_cmd_array_tx(fbi, lcd_video_display_on_cmds,
			ARRAY_SIZE(lcd_video_display_on_cmds));

#ifdef LDI_ESD_DETECT
       if(is_poweron == 0 && lcd_connected)
            schedule_delayed_work(&esd_work, msecs_to_jiffies(3000));
#endif

	/* restore all lanes to LP-11 state  */
	is_poweron = 1;
#if defined(CONFIG_BACKLIGHT_KTD253)
	printk("-----------backlight on : wakeup_Brightness(%d)\n",
			wakeup_brightness);
	ktd_backlight_set_brightness(wakeup_brightness);
#endif
#endif
}


#ifdef MDNIE_LITE_PORTING
struct class *mdnie_class;
static struct device_attribute mdnie_attributes[] = {
	__ATTR(mode, 0664, mode_show, mode_store),
	__ATTR(scenario, 0664, mDNIeScenario_show, mDNIeScenario_store),
	__ATTR(outdoor, 0664, mDNIeOutdoor_show, mDNIeOutdoor_store),
	__ATTR(cabc, 0664, cabc_show, cabc_store),
	__ATTR(tunning, 0664, mDNIeTuning_show, mDNIeTuning_store),
	__ATTR(negative, 0664, mDNIeNegative_show, mDNIeNegative_store),
	__ATTR_NULL,
};
#endif

void __init emeidkb_add_lcd_mipi(void)
{
	unsigned int CSn_NO_COL;
	struct dsi_info *dsi;

	struct pxa168fb_mach_info *fb = &mipi_lcd_info, *ovly =
	    &mipi_lcd_ovly_info;

	struct backlight_properties props = {
		.brightness = 130,
		.max_brightness = 255,
		.type = BACKLIGHT_RAW,
	};

	struct backlight_device *bd;

	fb->num_modes = ARRAY_SIZE(video_modes_emeidkb);

	if (QHD_PANEL == is_qhd_lcd())
		fb->modes = video_modes_emeidkb;
	else
		fb->modes = video_modes_hvga_vnc_emeidkb;

	fb->max_fb_size = ALIGN(fb->modes->xres, 16) *
		fb->modes->yres * 8 + 4096;

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

	fb->phy_type = DSI;
	fb->xcvr_reset = NULL;
	fb->phy_info = (void *)&emeidkb_dsiinfo;
	fb->dsi_panel_config = panel_init_config;
	fb->pxa168fb_lcd_power = emeidkb_lcd_power;

	/* Register DSI backlight  control */
	bd = backlight_device_register("panel", NULL, NULL,
			&hendrix_backlight_ops, &props);

	if (IS_ERR(bd)) {
		printk("backlight error\n");
	}

#ifdef MDNIE_LITE_PORTING
	mdnie_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR_OR_NULL(mdnie_class)) {
		pr_err("failed to create mdnie class\n");
	}

	mdnie_class->dev_attrs = mdnie_attributes;

	mDNIe_cfg.dev = device_create(mdnie_class, NULL, 0, &mDNIe_cfg, "mdnie");

	mutex_init(&mDNIe_cfg.lock);
#endif

	dsi = (struct dsi_info *)fb->phy_info;
	dsi->master_mode = 1;
	dsi->hfp_en = 0;

	dither_config(fb);
	/*
	 * Re-calculate lcd clk source and divider
	 * according to dsi lanes and output format.
	 */

	if (QHD_PANEL == is_qhd_lcd())
		calculate_lcd_sclk(fb);
	else {
		fb->sclk_src = 416000000;
		fb->sclk_div = 0xE000141B;
		dsi->master_mode = 0; /* dsi use slave mode */
	}

	/*
	 * FIXME:EMEI dkb use display clk1 as clk source,
	 * which is from PLL1 416MHZ. PLL3 1GHZ will be used
	 * for cpu core,and can't be DSI clock source specially.
	 */
	fb->sclk_div &= 0x0fffffff;
	fb->sclk_div |= 0x40000000;

	CSn_NO_COL = __raw_readl(DMCU_VIRT_BASE + DMCU_SDRAM_CFG0_TYPE1) >> 4;
	CSn_NO_COL &= 0xF;

	if (CSn_NO_COL <= 0x2) {
		/*
		 *If DDR page size < 4KB,
		 *select no crossing 1KB boundary check
		 */
		fb->io_pad_ctrl |= CFG_BOUNDARY_1KB;
		ovly->io_pad_ctrl |= CFG_BOUNDARY_1KB;
	}

	/* add frame buffer drivers */
	pxa988_add_fb(fb);
	/* add overlay driver */

	if (cpu_pxa98x_stepping() > PXA98X_Z2)
		pxa988_add_fb_ovly(ovly);
}

#ifdef CONFIG_PXA988_DISP_HACK
void __init emeidkb_add_tv_out(void)
{
	struct pxa168fb_mach_info *fb = &mipi_lcd_info, *ovly =
	    &mipi_lcd_ovly_info;

	/* Change id for TV GFX layer to avoid duplicate with panel path */
	strncpy(fb->id, "TV GFX Layer", 13);
	fb->num_modes = ARRAY_SIZE(video_modes_emeidkb);
	if (QHD_PANEL == is_qhd_lcd())
		fb->modes = video_modes_emeidkb;
	else
		fb->modes = video_modes_hvga_vnc_emeidkb;
	fb->max_fb_size = ALIGN(fb->modes->xres, 16) *
		fb->modes->yres * 8 + 4096;

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

	fb->mmap = 0;
	fb->phy_init = NULL;
	fb->phy_type = DSI;
	fb->phy_info = (void *)&emeidkb_dsiinfo;
	fb->xcvr_reset = NULL;
	fb->dsi_panel_config = NULL;
	fb->pxa168fb_lcd_power = NULL;

	dither_config(fb);
	/*
	 * Re-calculate lcd clk source and divider
	 * according to dsi lanes and output format.
	 */
	if (QHD_PANEL == is_qhd_lcd()) {
		calculate_lcd_sclk(fb);
		fb->phy_info = NULL;
		fb->phy_type = 0;
	} else {
		/* FIXME:rewrite sclk_src, otherwise VNC will
		 * use 520000000 as sclk_src so that clock source
		 * will be set 624M */
		fb->sclk_src = 416000000;
		/* FIXME: change pixel clk divider for HVGA for fps 60 */
		fb->sclk_div = 0xE000141B;
	}

	/*
	 * FIXME:EMEI dkb use display clk1 as clk source,
	 * which is from PLL1 416MHZ. PLL3 1GHZ will be used
	 * for cpu core,and can't be DSI clock source specially.
	 */
	fb->sclk_div &= 0x0fffffff;
	fb->sclk_div |= 0x40000000;

	pxa988_add_fb_tv(fb);
	pxa988_add_fb_tv_ovly(ovly);
}
#endif /* CONFIG_PXA988_DISP_HACK */
#endif /* CONFIG_MACH_ARUBA_TD, CONFIG_MACH_HENDRIX */
