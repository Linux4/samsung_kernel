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
#include <mach/mfp-pxa988-aruba.h>
#include <linux/power_supply.h>
#include <linux/timer.h>

#if defined(CONFIG_MACH_YELLOWSTONE) || (CONFIG_MACH_ARUBA_TD)
int is_esd_detected = 0;
struct delayed_work d_work; // ESD self protect
static void d_work_func(struct work_struct *work);
static int emeidkb_lcd_reset(void);
static int dsi_init(struct pxa168fb_info *fbi);
static void panel_init_config(struct pxa168fb_info *fbi);
static int get_panel_id(struct pxa168fb_info *fbi);
static int get_panel_id_hs_mode(struct pxa168fb_info *fbi);
static int isReadyTo_mDNIe = 1;
static struct mutex lcdc_mlock;
#define mDNIe_TUNING
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
#define HX8369B_PANEL1	0x554890
#define HX8369B_PANEL2	0x554990
#define HX8369B_PANEL_BOE1	0x55C090
#define HX8369B_PANEL_BOE2	0x55BC90

static u32 panel_id = 0;
static bool lcd_connected = false;

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

	if (!panel_id) {
		panel_id = get_panel_id(fbi);

		if (panel_id) {
			printk(KERN_INFO "[LCD] lcd is connected(id : 0x%x)\n", panel_id);
			lcd_connected = true;
		} else {
			printk(KERN_INFO "[LCD] lcd is not connected\n");
			lcd_connected = false;
		}
	}

	/* if panel not enabled, init panel settings via dsi */
#if 0 //move after dsi_controller_init
	if (mi->phy_type == DSI && !fbi->skip_pw_on)
		mi->dsi_panel_config(fbi);
#endif
	/* put all lanes to LP-11 state  */
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
		int ret;
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
	struct dsi_info *dsi;
	int bpp;

	if (mi->phy_type == LVDS) {
		struct lvds_info *lvds;

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

#ifdef CONFIG_MACH_YELLOWSTONE
static struct fb_videomode video_modes_yellowstone[] = {
	[0] = {
		 /* panel refresh rate should <= 55(Hz) */
		.refresh = 55,
		.xres = 1280,
		.yres = 800,
		.hsync_len = 2,
		.left_margin = 64,
		.right_margin = 64,
		.vsync_len = 2,
		.upper_margin = 8,
		.lower_margin = 8,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
		},
};

static int yellowstone_lvds_power(struct pxa168fb_info *fbi,
			     unsigned int spi_gpio_cs,
			     unsigned int spi_gpio_reset, int on)
{
	static struct regulator *v_lcd, *v_1p8_ana;
	int lcd_rst_n;

	/*
	 * FIXME: It is board related, baceuse zx will be replaced soon,
	 * it is temproary distinguished by cpu
	 */
	lcd_rst_n = mfp_to_gpio(GPIO128_LCD_RST);
	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return -EIO;
	}

	/* V_LCD 3.3v */
	if (!v_lcd) {
		v_lcd = regulator_get(NULL, "V_LCD");
		if (IS_ERR(v_lcd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_v_lcd;
		}
	}
	/* V_1P8_ANA, AVDD_LVDS, 1.8v */
	if (!v_1p8_ana) {
		v_1p8_ana = regulator_get(NULL, "V_1P8_ANA");
		if (IS_ERR(v_1p8_ana)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_v_1p8_ana;
		}
	}

	if (on) {
		regulator_set_voltage(v_1p8_ana, 1800000, 1800000);
		regulator_enable(v_1p8_ana);

		regulator_set_voltage(v_lcd, 3300000, 3300000);
		regulator_enable(v_lcd);

		/* release panel from reset */
		gpio_direction_output(lcd_rst_n, 1);
	} else {
		/* disable v_ldo10 3.3v */
		regulator_disable(v_lcd);

		/* disable v_ldo19 1.8v */
		regulator_disable(v_1p8_ana);

		/* set panel reset */
		gpio_direction_output(lcd_rst_n, 0);
	}

	gpio_free(lcd_rst_n);

	pr_debug("%s on %d\n", __func__, on);
	return 0;

regu_v_1p8_ana:
	v_1p8_ana = NULL;
	regulator_put(v_lcd);

regu_v_lcd:
	v_lcd = NULL;
	gpio_free(lcd_rst_n);
	return -EIO;
}

static struct lvds_info lvdsinfo = {
	.src	= LVDS_SRC_PN,
	.fmt	= LVDS_FMT_18BIT,
};

static void lvds_hook(struct pxa168fb_mach_info *mi)
{
	mi->phy_type = LVDS;
	mi->phy_init = pxa688_lvds_init;
	mi->phy_info = (void *)&lvdsinfo;

	mi->modes->refresh = 60;

	mi->pxa168fb_lcd_power = yellowstone_lvds_power;
}

static void vsmooth_init(int vsmooth_ch, int filter_ch)
{
#ifdef CONFIG_PXA688_MISC
	/*
	 * set TV path vertical smooth, panel2 as filter channel,
	 * vertical smooth is disabled by default to avoid underrun
	 * when video playback, to enable/disable graphics/video
	 * layer vertical smooth:
	 * echo g0/g1/v0/v1 > /sys/deivces/platform/pxa168-fb.1/misc
	 */
	fb_vsmooth = vsmooth_ch; fb_filter = filter_ch;
#endif
}

#define DDR_MEM_CTRL_BASE 0xD0000000
#define SDRAM_CONFIG_TYPE1_CS0 0x20	/* MMP3 */
void __init yellowstone_add_lcd_mipi(void)
{
	unsigned char __iomem *dmc_membase;
	unsigned int CSn_NO_COL;

	struct pxa168fb_mach_info *fb = &mipi_lcd_info,
				  *ovly =  &mipi_lcd_ovly_info;

	fb->num_modes = ARRAY_SIZE(video_modes_yellowstone);
	fb->modes = video_modes_yellowstone;
	fb->max_fb_size = video_modes_yellowstone[0].xres *
		video_modes_yellowstone[0].yres * 8 + 4096;
	fb->vdma_enable = 1;
	fb->sram_size = 30 * 1024;
	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

	lvds_hook(fb);
	dither_config(fb);

	if (fb->phy_type & (DSI | DSI2DPI)) {
		struct dsi_info *dsi;

		dsi = (struct dsi_info *)fb->phy_info;
		dsi->master_mode = 1;
		dsi->hfp_en = 0;
		if (dsi->bpp == 16)
			video_modes_yellowstone[0].right_margin =
			(dsi->lanes == 4) ? 325 : 179;
		else if (dsi->bpp == 24)
			video_modes_yellowstone[0].right_margin =
			(dsi->lanes == 4) ? 206 : 116;
	}

	/*
	 * Re-calculate lcd clk source and divider
	 * according to dsi lanes and output format.
	 */
	calculate_lcd_sclk(fb);

	dmc_membase = ioremap(DDR_MEM_CTRL_BASE, 0x30);
	CSn_NO_COL = __raw_readl(dmc_membase + SDRAM_CONFIG_TYPE1_CS0) >> 4;
	CSn_NO_COL &= 0xF;
	if (CSn_NO_COL <= 0x2) {
		/*
		 *If DDR page size < 4KB,
		 *select no crossing 1KB boundary check
		 */
		fb->io_pad_ctrl |= CFG_BOUNDARY_1KB;
		ovly->io_pad_ctrl |= CFG_BOUNDARY_1KB;
	}
	iounmap(dmc_membase);

	/* add frame buffer drivers */
	mmp3_add_fb(fb);
	/* add overlay driver */
#ifdef CONFIG_MMP_V4L2_OVERLAY
	mmp3_add_v4l2_ovly(ovly);
#else
	mmp3_add_fb_ovly(ovly);
#endif
	vsmooth_init(1, 2);
}
#endif

#ifdef CONFIG_MACH_ARUBA_TD
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

static int __init panel_id_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	panel_id = (u32)n;
	printk("panel_id : 0x%08x\n", panel_id);
	return 1;
}
__setup("panel_id=", panel_id_setup);

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
		 /* panel refresh rate should <= 55(Hz) */
		.refresh = 60,
		.xres = 480,
		.yres = 800,
		.hsync_len = 32,
		.left_margin = 100,//85,//150,
		.right_margin = 70, //85,//200,
		.vsync_len = 2,
		.upper_margin = 20,
		.lower_margin = 22, //20,
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

#define MAX_CMDS_LENGTH	 256

#define ARUBA_SLEEP_OUT_DELAY 150
#define ARUBA_DISP_ON_DELAY	150
#define ARUBA_DISP_OFF_DELAY	100
#define ARUBA_SLEEP_IN_DELAY 0

static char video1[] = {
	0xB9,
	0xFF, 0x83, 0x69
};

static char video2[] = {
	0x3A,
	0x77
};

static char video3[] = {
	0xD5,
	0x00, 0x00, 0x13, 0x03, 0x35, 0x00, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x01, 0x7A, 0x16,
	0x04, 0x04, 0x13, 0x07, 0x40, 0x13, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x40, 0x00, 0x88, 0x88, 0x54, 0x20, 0x00,
	0x00, 0x00, 0x10, 0x00, 0x88, 0x88, 0x67, 0x13, 0x50, 0x00, 0x00, 0x50, 0x00, 0x88, 0x88,
	0x76, 0x31, 0x10, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x45, 0x02, 0x40, 0x00, 0x00, 0x00,
	0x51, 0x00, 0x00, 0x00, 0x0A, 0x00, 0xEF, 0x00, 0xEF, 0x0A, 0x00, 0xEF, 0x00, 0xEF, 0x00,
	0x01, 0x5A
};
static char video3_1[] = {
	0xD5,
	0x00, 0x00, 0x13, 0x03, 0x35, 0x00, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x01, 0x7A, 0x16,
	0x04, 0x04, 0x13, 0x07, 0x40, 0x13, 0x00, 0x00, 0x00, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x48, 0x88, 0x85, 0x42, 0x00, 0x99, 0x99,
	0x00, 0x00, 0x18, 0x88, 0x86, 0x71, 0x35, 0x99, 0x99, 0x00, 0x00, 0x58, 0x88, 0x87, 0x63,
	0x11, 0x99, 0x99, 0x00, 0x00, 0x08, 0x88, 0x84, 0x50, 0x24, 0x99, 0x99, 0x00, 0x00, 0x00,
	0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x0F, 0x00, 0x0F, 0x00,
	0x01, 0x5A
};
static char video4[] = {
	0xBA,
	0x31, 0x00, 0x16, 0xCA, 0xB1, 0x0A, 0x00, 0x10, 0x28, 0x02, 0x21, 0x21, 0x9A, 0x1A, 0x8F
};
static char video5[] = {
	0xB1,
	0x09, 0x83, 0x67, 0x00, 0x92, 0x12, 0x16, 0x16, 0x0C, 0x02
};
static char video6_0[] = {
	0xB2,
	0x00, 0x70
};
static char video6[] = {
	0xE0,
	0x00, 0x05, 0x0B, 0x2F, 0x2F, 0x30, 0x1B, 0x3E, 0x07, 0x0D, 0x0E, 0x12, 0x13, 0x12, 0x14,
	0x13, 0x1A, 0x00, 0x05, 0x0B, 0x2F, 0x2F, 0x30, 0x1B, 0x3E, 0x07, 0x0D, 0x0E, 0x12, 0x13,
	0x12, 0x14, 0x13, 0x1A, 0x01
};
static char video6_1[] = {
	0xE0,
	0x00, 0x05, 0x0B, 0x2F, 0x2F, 0x30, 0x1B, 0x3D, 0x07, 0x0D, 0x0E, 0x12, 0x13, 0x12, 0x13,
	0x11, 0x1A, 0x00, 0x05, 0x0B, 0x2F, 0x2F, 0x30, 0x1B, 0x3D, 0x07, 0x0D, 0x0E, 0x12, 0x13,
	0x12, 0x13, 0x11, 0x1A, 0x01
};
static char video7[] = {
	0xC1,
	0x03, 0x00, 0x09, 0x11, 0x18, 0x1E, 0x27, 0x2F, 0x36, 0x3E, 0x45, 0x4C, 0x54, 0x5C, 0x64,
	0x6B, 0x73, 0x7C, 0x83, 0x8B, 0x94, 0x9C, 0xA4, 0xAC, 0xB4, 0xBC, 0xC4, 0xCD, 0xD5, 0xDD,
	0xE6, 0xEE, 0xF7, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09,
	0x11, 0x18, 0x1E, 0x27, 0x2F, 0x36, 0x3E, 0x45, 0x4C, 0x54, 0x5C, 0x64, 0x6B, 0x73, 0x7C,
	0x83, 0x8B, 0x94, 0x9C, 0xA4, 0xAC, 0xB4, 0xBC, 0xC4, 0xCD, 0xD5, 0xDD, 0xE6, 0xEE, 0xF7,
	0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x11, 0x18, 0x1E,
	0x27, 0x2F, 0x36, 0x3E, 0x45, 0x4C, 0x54, 0x5C, 0x64, 0x6B, 0x73, 0x7C, 0x83, 0x8B, 0x94,
	0x9C, 0xA4, 0xAC, 0xB4, 0xBC, 0xC4, 0xCD, 0xD5, 0xDD, 0xE6, 0xEE, 0xF7, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static char video_backscreen[] = {
	// Display the Black Screen ( 7 frame )
	0xB2,
	0x00, 0x70
};
static char video8[] = {
	0xB3,
	0x83, 0x00, 0x31, 0x03, 0x01, 0x15, 0x14
};
static char video9[] = {
	0xB4,
	0x02
};
static char video10[] = {
	0xB5,
	0x0B, 0x0B, 0x24
};
static char video11[] = {
	0xCC,
	0x02
};
static char video12[] = {
	0xC6,
	0x41, 0xFF, 0x7A
};
static char video13[] = {
	0xC0,
	0x73, 0x50, 0x00, 0x34, 0xC4, 0x02
};
static char video14[] = {
	0xE3,
	0x00, 0x00, 0x13, 0x1B
};
static char video15[] = {
	0xCB,
	0x6D
};
static char video16[] = {
	0xEA,
	0x62
};

static char video17[] = {
	0x36,
	0xC0
};

static char exit_sleep[] = {0x11,0x00};
static char display_on[] = {0x29,0x00};

static char enter_sleep[] = {0x10,0x00};
static char display_off[] = {0x28,0x00};

static char pkt_size_cmd[] = {0x1};
static char read_id1[] = {0xda};
static char read_id2[] = {0xdb};
static char read_id3[] = {0xdc};
static char read_esd[] = {0x0a};

static struct dsi_cmd_desc aruba_video_display_init_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video1),video1},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video2),video2},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video3),video3},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video4),video4},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video5),video5},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video6),video6},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video7),video7},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video8),video8},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video9),video9},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video10),video10},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video11),video11},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video12),video12},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video13),video13},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video14),video14},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video15),video15},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video16),video16},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video17),video17},
};

static struct dsi_cmd_desc aruba_video_display_init2_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video1),video1},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video2),video2},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video3_1),video3_1},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video4),video4},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video5),video5},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video6_1),video6_1},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video7),video7},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video8),video8},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video9),video9},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video10),video10},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video11),video11},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video12),video12},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video13),video13},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(video14),video14},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video15),video15},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video16),video16},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video17),video17},
};

static char hx8369b_boe1[] = {
	0xB9,
	0xFF, 0x83, 0x69
};
static char hx8369b_boe2[] = {
	0xBA,
	0x31, 0x00, 0x16, 0xCA, 0xB1, 0x0A, 0x00, 0x10, 0x28, 0x02, 0x21, 0x21, 0x9A, 0x1A, 0x8F
};
static char hx8369b_boe3[] = {
	0x3A,
	0x70
};
static char hx8369b_boe4[] = {
	0xD5,
	0x00, 0x00, 0x08, 0x00, 0x0A, 0x00, 0x00, 0x10, 0x01, 0x00, 0x00, 0x00, 0x01, 0x49, 0x37,
	0x00, 0x00, 0x0A, 0x0A, 0x0B, 0x47, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x03, 0x00, 0x00, 0x26, 0x00, 0x00, 0x91, 0x13, 0x35, 0x57, 0x75, 0x18, 0x00,
	0x00, 0x00, 0x86, 0x64, 0x42, 0x20, 0x00, 0x49, 0x00, 0x00, 0x00, 0x90, 0x02, 0x24, 0x46,
	0x64, 0x08, 0x00, 0x00, 0x00, 0x87, 0x75, 0x53, 0x31, 0x11, 0x59, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x0F, 0xFF, 0xFF, 0x0F, 0x00, 0x0F, 0xFF, 0xFF, 0x00,
	0x80, 0x5A
};
static char hx8369b_boe5[] = {
	0xB1,
	0x0C, 0x83, 0x77, 0x00, 0x0F, 0x0F, 0x18, 0x18, 0x0C, 0x02
};
static char hx8369b_boe6[] = {
	0xB2,
	0x00, 0x70
};
static char hx8369b_boe7[] = {
	0xB3,
	0x83, 0x00, 0x31, 0x03
};
static char hx8369b_boe8[] = {
	0xB4,
	0x02
};
static char hx8369b_boe9[] = {
	0xB6,
	0xA0, 0xA0
};
static char hx8369b_boe9_1[] = {
	0xCB,
	0x6D
};
static char hx8369b_boe10[] = {
	0xCC,
	0x02
};
static char hx8369b_boe11[] = {
	0xC6,
	0x41, 0xFF, 0x7A
};
static char hx8369b_boe12[] = {
	0xEA,
	0x72
};
static char hx8369b_boe13[] = {
	0xE3,
	0x07, 0x0F, 0x07, 0x0F
};
static char hx8369b_boe14[] = {
	0xC0,
	0x73, 0x50, 0x00, 0x34, 0xC4, 0x09
};
static char hx8369b_boe14_1[] = {
	0xC1,
	0x00
};
#if 0
static char hx8369b_boe15[] = {
	0xC1,
	0x03, 0x00, 0x08, 0x10, 0x18, 0x20, 0x29, 0x30, 0x37, 0x41, 0x48, 0x50, 0x58, 0x60, 0x68,
	0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xCF, 0xD7, 0xDF,
	0xE7, 0xEF, 0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
	0x10, 0x18, 0x20, 0x29, 0x30, 0x37, 0x41, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80,
	0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF8,
	0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x18, 0x20,
	0x29, 0x30, 0x37, 0x41, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88, 0x90, 0x98,
	0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF8, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif
static char hx8369b_boe16[] = {
	0xE0,
	0x00, 0x07, 0x0C, 0x30, 0x32, 0x3F, 0x1C, 0x3A, 0x08, 0x0D, 0x10, 0x14, 0x16, 0x14, 0x15,
	0x0E, 0x12, 0x00, 0x07, 0x0C, 0x30, 0x32, 0x3F, 0x1C, 0x3A, 0x08, 0x0D, 0x10, 0x14, 0x16,
	0x14, 0x15, 0x0E, 0x12, 0x01
};

static struct dsi_cmd_desc aruba_video_display_boe_init_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe1),hx8369b_boe1},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe2),hx8369b_boe2},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(hx8369b_boe3),hx8369b_boe3},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe4),hx8369b_boe4},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe5),hx8369b_boe5},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe6),hx8369b_boe6},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe7),hx8369b_boe7},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(hx8369b_boe8),hx8369b_boe8},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe9),hx8369b_boe9},
//  {DSI_DI_DCS_SWRITE1,1,0,sizeof(hx8369b_boe9_1),hx8369b_boe9_1},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(hx8369b_boe10),hx8369b_boe10},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe11),hx8369b_boe11},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(hx8369b_boe12),hx8369b_boe12},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe13),hx8369b_boe13},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe14),hx8369b_boe14},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(hx8369b_boe14_1),hx8369b_boe14_1},
//	{DSI_DI_DCS_LWRITE,1,0,sizeof(hx8369b_boe15),hx8369b_boe15},
	{DSI_DI_DCS_LWRITE,0,0,sizeof(hx8369b_boe16),hx8369b_boe16},
	{DSI_DI_DCS_SWRITE1,0,0,sizeof(video17),video17},
};


static struct dsi_cmd_desc aruba_video_display_on_cmds[] = {
	{DSI_DI_DCS_SWRITE, 0, ARUBA_SLEEP_OUT_DELAY, sizeof(exit_sleep),exit_sleep},
	{DSI_DI_DCS_SWRITE, 0, ARUBA_DISP_ON_DELAY, sizeof(display_on),display_on},
};

static struct dsi_cmd_desc aruba_video_display_off_cmds[] = {
	{DSI_DI_DCS_SWRITE, 0, ARUBA_DISP_OFF_DELAY, sizeof(display_off),display_off},
	{DSI_DI_DCS_SWRITE, 0, ARUBA_SLEEP_IN_DELAY, sizeof(enter_sleep),enter_sleep},
};

static struct dsi_cmd_desc aruba_video_read_id1_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 1, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, 1, 0, sizeof(read_id1), read_id1},
};

static struct dsi_cmd_desc aruba_video_read_id2_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 1, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, 1, 0, sizeof(read_id2), read_id2},
};

static struct dsi_cmd_desc aruba_video_read_id3_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 1, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, 1, 0, sizeof(read_id3), read_id3},
};

static struct dsi_cmd_desc aruba_video_hs_read_id1_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, 0, 0, sizeof(read_id1), read_id1},
};

static struct dsi_cmd_desc aruba_video_hs_read_id2_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, 0, 0, sizeof(read_id2), read_id2},
};

static struct dsi_cmd_desc aruba_video_hs_read_id3_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, 0, 0, sizeof(read_id3), read_id3},
};

static struct dsi_cmd_desc aruba_video_read_esd_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, 0, 0, sizeof(read_esd), read_esd},
};
#ifdef mDNIe_TUNING
static struct dsi_cmd_desc aruba_video_display_mDNIe_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(mDNIe_data),mDNIe_data},
};

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

typedef struct mdnie_config {
	int scenario;
	int negative;
	int outdoor;
};

struct mdnie_config mDNIe_cfg;

static char mDNIe_UI_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x02, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0xc6, 0x1e, 0xd3, 0x1f, 0x67, 0x1f, 0xc6, 0x04, 0xd3, 0x1f,
	0x67, 0x1f, 0xc6, 0x1e, 0xd3, 0x05, 0x67
};
static char mDNIe_VIDEO_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x06, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4, 0x1f, 0xdd, 0x04, 0x7f, 0x1f,
	0xa4, 0x1f, 0xdd, 0x1f, 0x4c, 0x04, 0xd7
};
static char mDNIe_VIDEO_WARM_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe3, 0x00, 0xf3, 0x00, 0xff, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x06, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4, 0x1f, 0xdd, 0x04, 0x7f, 0x1f,
	0xa4, 0x1f, 0xdd, 0x1f, 0x4c, 0x04, 0xd7
};
static char mDNIe_VIDEO_COLD_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xed, 0x00, 0xe1, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x06, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4, 0x1f, 0xdd, 0x04, 0x7f, 0x1f,
	0xa4, 0x1f, 0xdd, 0x1f, 0x4c, 0x04, 0xd7
};
static char mDNIe_CAMERA_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x06, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4, 0x1f, 0xdd, 0x04, 0x7f, 0x1f,
	0xa4, 0x1f, 0xdd, 0x1f, 0x4c, 0x04, 0xd7
};
static char mDNIe_NAVI_MODE[] = {
	0xE6,
	0x00
};
static char mDNIe_GALLERY_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x02, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x10, 0x1f, 0x4c, 0x1f, 0xa4, 0x1f, 0xdd, 0x04, 0x7f, 0x1f,
	0xa4, 0x1f, 0xdd, 0x1f, 0x4c, 0x04, 0xd7
};
static char mDNIe_VT_MODE[] = {
	0xE6,
	0x00
};
static char mDNIe_NEGATIVE_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff,
	0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff,
	0x02, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0xc6, 0x1e, 0xd3, 0x1f, 0x67, 0x1f, 0xc6, 0x04, 0xd3, 0x1f,
	0x67, 0x1f, 0xc6, 0x1e, 0xd3, 0x05, 0x67
};
static char mDNIe_VIDEO_OUTDOOR_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x07, 0xff, 0x00, 0x0a, 0xaf, 0x0d, 0x99, 0x14, 0x6d, 0x1b, 0x48, 0x2c, 0x05, 0xb4, 0x0f,
	0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae,
	0x0c, 0xae, 0x0c, 0xae, 0x0c, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x06, 0x7b, 0x1e, 0x5b, 0x1f, 0x2a, 0x1f, 0xae, 0x05, 0x28, 0x1f,
	0x2a, 0x1f, 0xae, 0x1e, 0x5b, 0x05, 0xf7
};
static char mDNIe_VIDEO_WARM_OUTDOOR_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe3, 0x00, 0xf3, 0x00, 0xff, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x07, 0xff, 0x00, 0x0a, 0xaf, 0x0d, 0x99, 0x14, 0x6d, 0x1b, 0x48, 0x2c, 0x05, 0xb4, 0x0f,
	0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae,
	0x0c, 0xae, 0x0c, 0xae, 0x0c, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x06, 0x7b, 0x1e, 0x5b, 0x1f, 0x2a, 0x1f, 0xae, 0x05, 0x28, 0x1f,
	0x2a, 0x1f, 0xae, 0x1e, 0x5b, 0x05, 0xf7
};
static char mDNIe_VIDEO_COLD_OUTDOOR_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xed, 0x00, 0xe1, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x07, 0xff, 0x00, 0x0a, 0xaf, 0x0d, 0x99, 0x14, 0x6d, 0x1b, 0x48, 0x2c, 0x05, 0xb4, 0x0f,
	0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae,
	0x0c, 0xae, 0x0c, 0xae, 0x0c, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x06, 0x7b, 0x1e, 0x5b, 0x1f, 0x2a, 0x1f, 0xae, 0x05, 0x28, 0x1f,
	0x2a, 0x1f, 0xae, 0x1e, 0x5b, 0x05, 0xf7
};
static char mDNIe_CAMERA_OUTDOOR_MODE[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x07, 0xff, 0x00, 0x0a, 0xaf, 0x0d, 0x99, 0x14, 0x6d, 0x1b, 0x48, 0x2c, 0x05, 0xb4, 0x0f,
	0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xbe, 0x26, 0xae, 0x0c, 0xae, 0x0c, 0xae, 0x0c, 0xae,
	0x0c, 0xae, 0x0c, 0xae, 0x0c, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x06, 0x7b, 0x1e, 0x5b, 0x1f, 0x2a, 0x1f, 0xae, 0x05, 0x28, 0x1f,
	0x2a, 0x1f, 0xae, 0x1e, 0x5b, 0x05, 0xf7
};

#if 0
static struct dsi_cmd_desc aruba_video_display_mDNIe_scenario_cmds[][1] = {
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_UI_MODE), mDNIe_UI_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_VIDEO_MODE), mDNIe_VIDEO_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_VIDEO_WARM_MODE), mDNIe_VIDEO_WARM_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_VIDEO_COLD_MODE), mDNIe_VIDEO_COLD_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_CAMERA_MODE), mDNIe_CAMERA_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_NAVI_MODE), mDNIe_NAVI_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_GALLERY_MODE), mDNIe_GALLERY_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_VT_MODE), mDNIe_VT_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_NEGATIVE_MODE), mDNIe_NEGATIVE_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_VIDEO_OUTDOOR_MODE), mDNIe_VIDEO_OUTDOOR_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_VIDEO_WARM_OUTDOOR_MODE), mDNIe_VIDEO_WARM_OUTDOOR_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_VIDEO_COLD_OUTDOOR_MODE), mDNIe_VIDEO_COLD_OUTDOOR_MODE},},
	{{DSI_DI_DCS_LWRITE, 1, 0,  sizeof(mDNIe_CAMERA_OUTDOOR_MODE), mDNIe_CAMERA_OUTDOOR_MODE},},
};
#else
static struct dsi_cmd_desc aruba_video_display_mDNIe_size[] = {
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_UI_MODE), mDNIe_UI_MODE}
};

static struct dsi_cmd_desc aruba_video_display_mDNIe_scenario_cmds[] = {
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_UI_MODE), mDNIe_UI_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_VIDEO_MODE), mDNIe_VIDEO_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_VIDEO_WARM_MODE), mDNIe_VIDEO_WARM_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_VIDEO_COLD_MODE), mDNIe_VIDEO_COLD_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_CAMERA_MODE), mDNIe_CAMERA_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_NAVI_MODE), mDNIe_NAVI_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_GALLERY_MODE), mDNIe_GALLERY_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_VT_MODE), mDNIe_VT_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_NEGATIVE_MODE), mDNIe_NEGATIVE_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_VIDEO_OUTDOOR_MODE), mDNIe_VIDEO_OUTDOOR_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_VIDEO_WARM_OUTDOOR_MODE), mDNIe_VIDEO_WARM_OUTDOOR_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_VIDEO_COLD_OUTDOOR_MODE), mDNIe_VIDEO_COLD_OUTDOOR_MODE},
	{DSI_DI_DCS_LWRITE, 0, 0,  sizeof(mDNIe_CAMERA_OUTDOOR_MODE), mDNIe_CAMERA_OUTDOOR_MODE},
};
#endif

static int parse_text(char *src, int len)
{
	int i;
	int data=0, value=0, count=0, comment=0;
	char *cur_position;

       mDNIe_data[count] = 0xE6;
       count++;

	cur_position = src;
	for(i=0; i<len; i++, cur_position++) {
		char a = *cur_position;
		switch(a) {
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
				if(comment>1)
					break;
				if(data==0 && a=='0')
					data=1;
				else if(data==2){
					data=3;
					value = (a-'0')*16;
				}
				else if(data==3){
					value += (a-'0');
					mDNIe_data[count]=value;
					printk("Tuning value[%d]=0x%02X\n", count, value);
					count++;
					data=0;
				}
				break;
			case 'a'...'f':
			case 'A'...'F':
				if(comment>1)
					break;
				if(data==2){
					data=3;
					if(a<'a') value = (a-'A'+10)*16;
					else value = (a-'a'+10)*16;
				}
				else if(data==3){
					if(a<'a') value += (a-'A'+10);
					else value += (a-'a'+10);
					mDNIe_data[count]=value;
					printk("Tuning value[%d]=0x%02X\n", count, value);
					count++;
					data=0;
				}
				break;
			case 'x':
			case 'X':
				if(data==1)
					data=2;
				break;
			default:
				if(comment==1)
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

    init_cmds_mDNIe[0][count++] = 4 + 1 + num + 2; // base + register addr + value + base
    init_cmds_mDNIe[0][count++] = 0x39;
    init_cmds_mDNIe[0][count++] = 0xE6;
    init_cmds_mDNIe[0][count++] = 1 + num; // register addr + value
    init_cmds_mDNIe[0][count++] = 0x00;
    init_cmds_mDNIe[0][count++] = 0x00;

    for(i = 0; i < num; i++)
    {
            init_cmds_mDNIe[0][count++] = mDNIe_data[i];
    }

    init_cmds_mDNIe[0][count++] = 0x00;
    init_cmds_mDNIe[0][count++] = 0x00;

    printk("--------------build data--------------------\n");

    for(i = 0; i < init_cmds_mDNIe[0][0]; i++)
    {
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

	printk("[INFO]:%s called loading file name : [%s]\n",__func__,filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if(IS_ERR(filp))
	{
		printk(KERN_ERR "[ERROR]:File open failed %d\n", IS_ERR(filp));
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	printk("[INFO]: Loading File Size : %ld(bytes)", l);

	dp = kmalloc(l+10, GFP_KERNEL);
	if(dp == NULL){
		printk("[ERROR]:Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);
	printk("[INFO] : before vfs_read()\n");
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	printk("[INFO] : after vfs_read()\n");

	if(ret != l) {
        printk("[ERROR] : vfs_read() filed ret : %d\n",ret);
        kfree(dp);
		filp_close(filp, current->files);
		return -1;
	}

	filp_close(filp, current->files);

	set_fs(fs);
	num = parse_text(dp, l);

	if(!num) {
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
    int ret;
    ret = sprintf(buf,"Tunned File Name : %s\n",tuning_filename);

    return ret;
}

ssize_t mDNIeTuning_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	char a;

	a = *buf;

	if(a=='1') {
		tuning_enable = 1;
        printk("%s:Tuning_enable\n",__func__);
	} else if(a=='0') {
		tuning_enable = 0;
        printk("%s:Tuning_disable\n",__func__);
	} else {
		char *pt;

        memset(tuning_filename,0,sizeof(tuning_filename));
        sprintf(tuning_filename,"%s%s",TUNING_FILE_PATH,buf);

        pt = tuning_filename;
        while(*pt)
        {
            if(*pt =='\r'|| *pt =='\n')
            {
                *pt = 0;
                break;
            }
            pt++;
        }
        printk("%s:%s\n",__func__,tuning_filename);

        if (load_tuning_data(tuning_filename) <= 0) {
            printk("[ERROR]:load_tunig_data() failed\n");
            return size;
        }

		if(tuning_enable && mDNIe_data[0] !=  0) {
			printk("========================mDNIe!!!!!!!\n");
			pxa168_dsi_cmd_array_tx(fbi_global, aruba_video_display_mDNIe_cmds,ARRAY_SIZE(aruba_video_display_mDNIe_cmds));
		}

    }
    return size;
}

static void set_mDNIe_Mode(struct mdnie_config *mDNIeCfg, int mode) //mode : HS-0, LP-1
{
	int value;

	printk("%s:[mDNIe] negative=%d, isReadyTo_mDNIe(%d)\n", __func__, mDNIe_cfg.negative, isReadyTo_mDNIe);
	if(!isReadyTo_mDNIe)
		return;
	msleep(100);
	if (mDNIe_cfg.negative) {
        printk("&s : apply negative color\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi_global, &aruba_video_display_mDNIe_scenario_cmds[SCENARIO_MAX],ARRAY_SIZE(aruba_video_display_mDNIe_size));
		return;
	}

	switch(mDNIeCfg->scenario) {
	case UI_MODE:
	case GALLERY_MODE:
		value = mDNIeCfg->scenario;
		break;

	case VIDEO_MODE:
		if(mDNIeCfg->outdoor == OUTDOOR_ON) {
			value = SCENARIO_MAX + 1;
		} else {
			value = mDNIeCfg->scenario;
		}
		break;

	case VIDEO_WARM_MODE:
		if(mDNIeCfg->outdoor == OUTDOOR_ON) {
			value = SCENARIO_MAX + 2;
		} else {
			value = mDNIeCfg->scenario;
		}
		break;

	case VIDEO_COLD_MODE:
		if(mDNIeCfg->outdoor == OUTDOOR_ON) {
			value = SCENARIO_MAX + 3;
		} else {
			value = mDNIeCfg->scenario;
		}
		break;

	case CAMERA_MODE:
		if(mDNIeCfg->outdoor == OUTDOOR_ON) {
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
	pxa168_dsi_cmd_array_tx(fbi_global, &aruba_video_display_mDNIe_scenario_cmds[value],ARRAY_SIZE(aruba_video_display_mDNIe_size));
}


ssize_t mDNIeScenario_show(struct device *dev,
        struct device_attribute *attr, char *buf)

{
    int ret;
    ret = sprintf(buf,"mDNIeScenario_show : %d\n", mDNIe_cfg.scenario);

    return ret;
}

ssize_t mDNIeScenario_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	strict_strtoul(buf, 0, (unsigned long *)&value);

	printk("%s:value=%d\n", __func__, value);

	switch(value) {
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
	set_mDNIe_Mode(&mDNIe_cfg,0);

    return size;
}

ssize_t mDNIeOutdoor_show(struct device *dev,
        struct device_attribute *attr, char *buf)

{
    int ret;
    ret = sprintf(buf,"mDNIeOutdoor_show : %d\n", mDNIe_cfg.outdoor);

    return ret;
}

ssize_t mDNIeOutdoor_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	strict_strtoul(buf, 0, (unsigned long *)&value);

	printk("%s:value=%d\n", __func__, value);

	mDNIe_cfg.outdoor = value;

	set_mDNIe_Mode(&mDNIe_cfg, 0);

    return size;
}

ssize_t mDNIeNegative_show(struct device *dev,
        struct device_attribute *attr, char *buf)

{
    int ret;
    ret = sprintf(buf,"mDNIeNegative_show : %d\n", mDNIe_cfg.negative);

    return ret;
}

ssize_t mDNIeNegative_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	strict_strtoul(buf, 0, (unsigned long *)&value);

	printk("%s:value=%d\n", __func__, value);

	if(value == 1) {
		mDNIe_cfg.negative = 1;
	} else {
		mDNIe_cfg.negative = 0;
	}

	set_mDNIe_Mode(&mDNIe_cfg, 0);

    return size;
}

#endif

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
		INIT_DELAYED_WORK(&d_work, d_work_func); // ESD self protect

		if (panel_id) {
			printk(KERN_INFO "[LCD] lcd is connected(id : 0x%x)\n", panel_id);
			lcd_connected = true;
		} else {
			printk(KERN_INFO "[LCD] lcd is not connected\n");
			lcd_connected = false;
		}

		if (lcd_connected) {
			printk(KERN_INFO "[LCD] schedule_delayed_work after 20 sec\n");
			schedule_delayed_work(&d_work, msecs_to_jiffies(20000));
		}
	}

	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return -EIO;
	}
	/* FIXME:LCD_AVDD 3.0v */
	if (lcd_avdd == NULL) {
		lcd_avdd = regulator_get(NULL, "v_lcd_3V");
		if (IS_ERR(lcd_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_avdd;
		}
		regulator_set_voltage(lcd_avdd, 3000000, 3000000);
		regulator_enable(lcd_avdd);
		mdelay(5);        
	}

	if (on) {
		/* if uboot enable display, don't reset the panel */
		if (!fbi->skip_pw_on) {
			/* release panel from reset */
			gpio_direction_output(lcd_rst_n, 1);
			udelay(20);
			gpio_direction_output(lcd_rst_n, 0);
			udelay(50);
			gpio_direction_output(lcd_rst_n, 1);
			mdelay(15);
		}
		printk("[emeidkb_lcd_power]--------------power on!!!---------\n");
	} else {
#if defined(CONFIG_BACKLIGHT_KTD253)
		ktd_backlight_set_brightness(0);
		printk("FET_EN : %d\n", !!gpio_get_value(MFP_PIN_GPIO5));
#endif
		if (lcd_connected)
			cancel_delayed_work_sync(&d_work);
		is_poweron = 0;
		isReadyTo_mDNIe = 0;


		pxa168_dsi_cmd_array_tx(fbi, aruba_video_display_off_cmds,ARRAY_SIZE(aruba_video_display_off_cmds));
		mdelay(100);
		/* disable LCD_AVDD 3.1v */
		//regulator_disable(lcd_avdd);

		/* set panel reset */
		gpio_direction_output(lcd_rst_n, 0);
		printk("[emeidkb_lcd_power]------------power off!!!---------\n");
		printk("LCD_BL_CTRL : %d\n", !!gpio_get_value(MFP_PIN_GPIO9));
		printk("5M_CAM_STBY : %d\n", !!gpio_get_value(MFP_PIN_GPIO14));
		printk("5M_CAM_RESET : %d\n", !!gpio_get_value(MFP_PIN_GPIO15));
		printk("LCD_RESET_N : %d\n", !!gpio_get_value(MFP_PIN_GPIO18));
		printk("5M_CAM_RESET : %d\n", !!gpio_get_value(MFP_PIN_GPIO20));
		printk("WLAN_BT_RESET : %d\n", !!gpio_get_value(MFP_PIN_GPIO34));
		printk("WLAN_PD : %d\n", !!gpio_get_value(MFP_PIN_GPIO67));
		printk("AP_AGPS_ONOFF : %d\n", !!gpio_get_value(MFP_PIN_GPIO67));
		printk("AP_AGPS_RESETn : %d\n", !!gpio_get_value(MFP_PIN_GPIO68));
		printk("NFC_EN : %d\n", !!gpio_get_value(MFP_PIN_GPIO70));
		printk("NFC_FIRMWARE : %d\n", !!gpio_get_value(MFP_PIN_GPIO71));
		printk("KEY_LED_EN : %d\n", !!gpio_get_value(MFP_PIN_GPIO96));
		printk("CAM_FLASH_SET : %d\n", !!gpio_get_value(MFP_PIN_GPIO97));
		printk("CHG_EN : %d\n", !!gpio_get_value(MFP_PIN_GPIO98));
	}

	gpio_free(lcd_rst_n);
	pr_debug("%s on %d\n", __func__, on);

	return 0;

regu_lcd_avdd:
	lcd_avdd = NULL;
	regulator_put(lcd_avdd);

	return -EIO;
}

static void d_work_func(struct work_struct *work)
{

	if (!lcd_connected) {
		printk(KERN_INFO "[LCD] ignore esd : lcd is not connected!!\n");
		return;
	}

	printk("[LCD] %s : is_poweron(%d)\n",__func__, is_poweron);
	if ((is_poweron == 1)&&(fbi_global!=NULL)&&(fbi_global->active))
	{
		struct dsi_buf dbuf;

		dsi_cmd_array_rx(fbi_global, &dbuf, aruba_video_read_esd_cmds, ARRAY_SIZE(aruba_video_read_esd_cmds));

		printk("#### 0Ah:0x%x \n", dbuf.data[0]);

		if (dbuf.data[0] != 0x9C) {
			printk("lcd_work_func : reset!!!!!!!! starts\n");
			isReadyTo_mDNIe = 0;
			emeidkb_lcd_power(fbi_global, 0, 0, 1);
			dsi_init(fbi_global);
			isReadyTo_mDNIe = 1;
			printk("lcd_work_func : reset!!!!!!!! ends\n");
		}
	}

	if (is_poweron && lcd_connected)
		schedule_delayed_work(&d_work, msecs_to_jiffies(3000));
}

static int get_panel_id_hs_mode(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	u32 read_id = 0;
	struct dsi_buf *dbuf;

	dbuf = kmalloc(sizeof(struct dsi_buf), GFP_KERNEL);
	if (dbuf) {
		dsi_cmd_array_rx(fbi, dbuf, aruba_video_hs_read_id1_cmds,
				ARRAY_SIZE(aruba_video_hs_read_id1_cmds));
		read_id |= dbuf->data[0] << 16;
		dsi_cmd_array_rx(fbi, dbuf, aruba_video_hs_read_id2_cmds,
				ARRAY_SIZE(aruba_video_hs_read_id2_cmds));
		read_id |= dbuf->data[0] << 8;
		dsi_cmd_array_rx(fbi, dbuf, aruba_video_hs_read_id3_cmds,
				ARRAY_SIZE(aruba_video_hs_read_id3_cmds));
		read_id |= dbuf->data[0];
		kfree(dbuf);
	} else {
		pr_err("%s: can't alloc dsi rx buffer\n", __func__);
	}
	printk("[LCD] %s, read_id : 0x%x\n", __func__, read_id);

	return read_id;
#endif

}

static int get_panel_id(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	u32 read_id = 0;
	struct dsi_buf *dbuf;

	printk("[panel_init_config]-----set_dsi_low_power_mode--------\n");
	set_dsi_low_power_mode(fbi);
	mdelay(20);

	dbuf = kmalloc(sizeof(struct dsi_buf), GFP_KERNEL);
	if (dbuf) {
		dsi_cmd_array_rx(fbi, dbuf, aruba_video_read_id1_cmds,
				ARRAY_SIZE(aruba_video_read_id1_cmds));
		read_id |= dbuf->data[0] << 16;
		dsi_cmd_array_rx(fbi, dbuf, aruba_video_read_id2_cmds,
				ARRAY_SIZE(aruba_video_read_id2_cmds));
		read_id |= dbuf->data[0] << 8;
		dsi_cmd_array_rx(fbi, dbuf, aruba_video_read_id3_cmds,
				ARRAY_SIZE(aruba_video_read_id3_cmds));
		read_id |= dbuf->data[0];
		kfree(dbuf);
		pr_info("Panel id is 0x%x\n", read_id);
	} else
		pr_err("%s: can't alloc dsi rx buffer\n", __func__);
   printk("[LCD] %s, read_id(%x)\n", __func__, read_id);

	dsi_lanes_enable(fbi, 1);

	return read_id;
#endif

}
static void panel_init_config(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	struct power_supply *psy;
	union power_supply_propval val;

	psy = power_supply_get_by_name("battery");
	if (psy != NULL && psy->get_property != NULL)
	{
	    int ret;

	    ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
        printk("[LCD] temp(%d), D5(%x)+++\n", val.intval, video3_1[20]);
        if(ret == 0)
        {
            if(panel_id == HX8369B_PANEL1 || panel_id == HX8369B_PANEL2)
            {
                if(val.intval >= 0)
                {
                    video3_1[20] = 0x40;
                }
                else
                {
                    video3_1[20] = 0x4C;
                }
                printk("[LCD] temp(%d), D5(%x)---\n", val.intval, video3_1[20]);
            }
            else
            {
                if(val.intval >= 0)
                {
                    hx8369b_boe5[1] = 0x0C;
                    hx8369b_boe5[3] = 0x77;

                    hx8369b_boe13[1] = 0x07;
                    hx8369b_boe13[2] = 0x0F;
                    hx8369b_boe13[3] = 0x07;
                    hx8369b_boe13[4] = 0x0F;

                    hx8369b_boe14[6] = 0x09;
                }
                else
                {
                    hx8369b_boe5[1] = 0x11;
                    hx8369b_boe5[3] = 0x78;

                    hx8369b_boe13[1] = 0x00;
                    hx8369b_boe13[2] = 0x00;
                    hx8369b_boe13[3] = 0x16;
                    hx8369b_boe13[4] = 0x16;

                    hx8369b_boe14[6] = 0x03;
                }
            }
        }
        else
        {
            printk("[LCD] read temperature error\n");
        }
	}

    if(panel_id == HX8369B_PANEL1 || panel_id == HX8369B_PANEL2)
    {
		struct fb_var_screeninfo *var = &(fbi_global->fb_info->var);
        var->left_margin = 85;
        var->right_margin = 85;
        var->upper_margin = 20;
        var->lower_margin = 20;
        printk("change porch setting for AUO LCD\n");
    }
    if(panel_id == HX8369B_PANEL1)
        pxa168_dsi_cmd_array_tx(fbi, aruba_video_display_init_cmds,ARRAY_SIZE(aruba_video_display_init_cmds));
    else if(panel_id == HX8369B_PANEL2)
        pxa168_dsi_cmd_array_tx(fbi, aruba_video_display_init2_cmds,ARRAY_SIZE(aruba_video_display_init2_cmds));
    else if(panel_id==HX8369B_PANEL_BOE1 ||panel_id==HX8369B_PANEL_BOE2) // false alarm : if condition is different
        pxa168_dsi_cmd_array_tx(fbi, aruba_video_display_boe_init_cmds,ARRAY_SIZE(aruba_video_display_boe_init_cmds));
    else // default
        pxa168_dsi_cmd_array_tx(fbi, aruba_video_display_boe_init_cmds,ARRAY_SIZE(aruba_video_display_boe_init_cmds));

#ifdef mDNIe_TUNING
    isReadyTo_mDNIe = 1;
    set_mDNIe_Mode(&mDNIe_cfg, 1);
    if(tuning_enable && mDNIe_data[0] !=  0) {
		printk("-----------panel_init_config-----mDNIe-------------\n");
		pxa168_dsi_cmd_array_tx(fbi, aruba_video_display_mDNIe_cmds,ARRAY_SIZE(aruba_video_display_mDNIe_cmds));
    }
#endif
	pxa168_dsi_cmd_array_tx(fbi, aruba_video_display_on_cmds,ARRAY_SIZE(aruba_video_display_on_cmds));

    if(is_poweron == 0 && lcd_connected)
        schedule_delayed_work(&d_work, msecs_to_jiffies(3000));

    is_poweron = 1;
#if defined(CONFIG_BACKLIGHT_KTD253)
	printk("-----------backlight on : wakeup_Brightness(%d)\n", wakeup_brightness);
    ktd_backlight_set_brightness(wakeup_brightness);
#endif
#endif
}

void __init emeidkb_add_lcd_mipi(void)
{
	unsigned int CSn_NO_COL;
	struct dsi_info *dsi;

	struct pxa168fb_mach_info *fb = &mipi_lcd_info, *ovly =
	    &mipi_lcd_ovly_info;

	fb->num_modes = ARRAY_SIZE(video_modes_emeidkb);
	if (QHD_PANEL == is_qhd_lcd())
		fb->modes = video_modes_emeidkb;
	else
		fb->modes = video_modes_hvga_vnc_emeidkb;
	fb->max_fb_size = ALIGN(fb->modes->xres, 16) *
		fb->modes->yres * 8 + 4096;

	/* align with android format and vres_virtual pitch */
//	fb->pix_fmt = PIX_FMT_RGBA888;
//	fb->xres_virtual = ALIGN(fb->modes->xres, 16);

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

	fb->phy_type = DSI;
	fb->xcvr_reset = NULL;
	fb->phy_info = (void *)&emeidkb_dsiinfo;
	fb->dsi_panel_config = panel_init_config;
	fb->pxa168fb_lcd_power = emeidkb_lcd_power;

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
#endif /* CONFIG_MACH_ARUBA_TD */
