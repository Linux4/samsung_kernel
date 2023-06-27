#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/mfp-mmp2.h>
#include <mach/eden.h>
#include <mach/mmp3.h>
#include <mach/pxa988.h>
#include <mach/pxa168fb.h>
#include <mach/regs-mcu.h>
#include <mach/features.h>

#if defined(CONFIG_MACH_YELLOWSTONE) || defined(CONFIG_MACH_EMEIDKB) \
	|| defined(CONFIG_MACH_HELANDKB) || defined(CONFIG_MACH_EDEN_FPGA) \
	|| defined(CONFIG_MACH_ARUBA_TD) || defined(CONFIG_MACH_HELANDELOS) \
	|| defined(CONFIG_MACH_WILCOX) || defined(CONFIG_MACH_CS02) \
	|| defined(CONFIG_MACH_BAFFINQ) || defined(CONFIG_MACH_GOLDEN) \
	|| defined(CONFIG_MACH_GOYA)
static int dsi_init(struct pxa168fb_info *fbi);
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
#ifdef CONFIG_MACH_EDEN_FPGA
	.dumb_mode = DUMB_MODE_RGB888,
#endif
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

	/* put all lanes to LP-11 state  */
	dsi_lanes_enable(fbi, 1);

	/* if panel not enabled, init panel settings via dsi */
	if (mi->phy_type == DSI && !fbi->skip_pw_on)
		mi->dsi_panel_config(fbi);

	/*  reset the bridge */
	if (mi->exter_brige_pwr) {
		mi->exter_brige_pwr(fbi,1);
		msleep(10);
	}

	/* set dsi controller */
	dsi_set_controller(fbi);

	/* set dsi to dpi conversion chip */
	if (mi->phy_type == DSI2DPI) {
		ret = mi->exter_brige_init(fbi);
		if (ret < 0)
			pr_err("exter_brige_init error!\n");
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
#ifdef CONFIG_MACH_EDEN_FPGA
	mi->sclk_div = 0x20000002;
	return;
#else
	if (mi->phy_type & (DSI | DSI2DPI))
		calculate_dsi_clk(mi);
	else if (mi->phy_type & LVDS)
		calculate_lvds_clk(mi);
	else
		return;
#endif
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
		v_lcd = regulator_get(fbi->dev, "V_LCD");
		if (IS_ERR(v_lcd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_v_lcd;
		}
	}
	/* V_1P8_ANA, AVDD_LVDS, 1.8v */
	if (!v_1p8_ana) {
		v_1p8_ana = regulator_get(fbi->dev, "V_1P8_ANA");
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
	struct dsi_info *dsi;

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

#if defined(CONFIG_MACH_EMEIDKB) || defined(CONFIG_MACH_HELANDKB) || defined(CONFIG_MACH_ARUBA_TD) \
	|| defined(CONFIG_MACH_HELANDELOS) || defined(CONFIG_MACH_WILCOX) || defined(CONFIG_MACH_CS02) \
	|| defined(CONFIG_MACH_BAFFINQ) || defined(CONFIG_MACH_GOLDEN) || defined(CONFIG_MACH_GOYA)
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

/*
 * FIXME:add fhd_lcd to indicate if use 1080p panel or not
 */
#define     FHD_PANEL	1
static int fhd_lcd;
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
		.refresh = 57,
		.xres = 540,
		.yres = 960,
		.hsync_len = 2,
		.left_margin = 10,   /* left_margin should >=3 */
		.right_margin = 68, /* right_margin should >=62 */
		.vsync_len = 2,
		.upper_margin = 6,  /* upper_margin should >= 6 */
		.lower_margin = 6,  /* lower_margin should >= 6 */
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};

static struct fb_videomode video_modes_1080p_emeidkb[] = {
	[0] = {
		.refresh = 60,
		.xres = 1080,
		.yres = 1920,
		.hsync_len = 2,
		.left_margin = 40,   /* left_margin should >=20 */
		.right_margin = 160, /* right_margin should >=100 */
		.vsync_len = 2,
		.upper_margin = 4,  /* upper_margin should >= 4 */
		.lower_margin = 4,  /* lower_margin should >= 4 */
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};

static int emeidkb_lcd_power(struct pxa168fb_info *fbi,
			     unsigned int spi_gpio_cs,
			     unsigned int spi_gpio_reset, int on)
{
	static struct regulator *lcd_iovdd, *lcd_avdd;
	int lcd_rst_n;

	/* FIXME:lcd reset,use GPIO_1 as lcd reset */
	lcd_rst_n = 1;
	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return -EIO;
	}

	/* FIXME:LCD_IOVDD, 1.8v */
	if (!lcd_iovdd) {
		lcd_iovdd = regulator_get(fbi->dev, "v_ldo15");
		if (IS_ERR(lcd_iovdd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_iovdd;
		}
	}

	/* FIXME:LCD_AVDD 3.1v */
	if (!lcd_avdd) {
		lcd_avdd = regulator_get(fbi->dev, "v_ldo8");
		if (IS_ERR(lcd_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_avdd;
		}
	}

	if (on) {
		regulator_set_voltage(lcd_avdd, 3100000, 3100000);
		regulator_enable(lcd_avdd);
		mdelay(5);

		regulator_set_voltage(lcd_iovdd, 1800000, 1800000);
		regulator_enable(lcd_iovdd);
		msleep(15);

		/* if uboot enable display, don't reset the panel */
		if (!fbi->skip_pw_on) {
			/* release panel from reset */
			gpio_direction_output(lcd_rst_n, 1);
			udelay(20);
			gpio_direction_output(lcd_rst_n, 0);
			udelay(50);
			gpio_direction_output(lcd_rst_n, 1);
			msleep(15);
		}
	} else {
		/* disable LCD_AVDD 3.1v */
		regulator_disable(lcd_avdd);

		/* disable LCD_IOVDD 1.8v */
		regulator_disable(lcd_iovdd);

		/* set panel reset */
		gpio_direction_output(lcd_rst_n, 0);
	}

	gpio_free(lcd_rst_n);
	pr_debug("%s on %d\n", __func__, on);

	return 0;

regu_lcd_avdd:
	lcd_avdd = NULL;
	regulator_put(lcd_iovdd);

regu_lcd_iovdd:
	lcd_iovdd = NULL;
	gpio_free(lcd_rst_n);

	return -EIO;
}

static int emeidkb_1080p_lcd_power(struct pxa168fb_info *fbi,
			     unsigned int spi_gpio_cs,
			     unsigned int spi_gpio_reset, int on)
{
	static struct regulator *lcd_iovdd;
	int lcd_rst_n;
	int boost_en_5v;

	/* FIXME:lcd reset,use GPIO_1 as lcd reset */
	lcd_rst_n = 1;
	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return -EIO;
	}
	/* reset low */
	gpio_direction_output(lcd_rst_n, 0);
	mdelay(1);


	/* FIXME:5v_boost_en, use GPIO_107 */
	boost_en_5v = 107;
	if (gpio_request(boost_en_5v, "5v boost en")) {
		pr_err("gpio %d request failed\n", boost_en_5v);
		return -EIO;
	}

	/* FIXME:LCD_IOVDD, 1.8v */
	if (!lcd_iovdd) {
		lcd_iovdd = regulator_get(fbi->dev, "v_ldo15");
		if (IS_ERR(lcd_iovdd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_iovdd;
		}
	}
	if (on) {
		regulator_set_voltage(lcd_iovdd, 1800000, 1800000);
		regulator_enable(lcd_iovdd);
		mdelay(1);

		/* LCD_AVDD+ and LCD_AVDD- ON */
		gpio_direction_output(boost_en_5v, 1);
		mdelay(1);

		gpio_direction_output(lcd_rst_n, 1);
		mdelay(10);

	} else {
		/* set panel reset */
		gpio_direction_output(lcd_rst_n, 0);

		/* disable AVDD+/- */
		gpio_direction_output(boost_en_5v, 0);
		msleep(100);

		/* disable LCD_IOVDD 1.8v */
		regulator_disable(lcd_iovdd);
	}

	gpio_free(lcd_rst_n);
	gpio_free(boost_en_5v);
	pr_debug("%s on %d\n", __func__, on);

	return 0;

regu_lcd_iovdd:
	lcd_iovdd = NULL;
	gpio_free(lcd_rst_n);
	gpio_free(boost_en_5v);

	return -EIO;
}

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

/* emeidkb: when enable 1080p panle, use 4 lanes */
static struct dsi_info emeidkb_1080p_dsiinfo = {
	.id = 1,
	.lanes = 4,
	.bpp = 24,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_888,
	.burst_mode = DSI_BURST_MODE_BURST,
	.hbp_en = 1,
	.hfp_en = 1,
};

#define NT35565_SLEEP_OUT_DELAY 200
#define NT35565_DISP_ON_DELAY	0

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};

static struct dsi_cmd_desc nt35565_video_display_on_cmds[] = {
	{DSI_DI_DCS_SWRITE, 0, NT35565_SLEEP_OUT_DELAY, sizeof(exit_sleep),
		exit_sleep},
	{DSI_DI_DCS_SWRITE, 0, NT35565_DISP_ON_DELAY, sizeof(display_on),
		display_on},
};

static void panel_init_config(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	dsi_cmd_array_tx(fbi, nt35565_video_display_on_cmds,
			ARRAY_SIZE(nt35565_video_display_on_cmds));
#endif
}

static char manufacturer_cmd_access_protect[] = {0xB0, 0x04};
static char backlight_ctrl[] = {0xCE, 0x00, 0x01, 0x88, 0xC1, 0x00, 0x1E, 0x04};
static char nop[] = {0x0};
static char seq_test_ctrl[] = {0xD6, 0x01};
static char write_display_brightness[] = {0x51, 0x0F, 0xFF};
static char write_ctrl_display[] = {0x53, 0x24};

static struct dsi_cmd_desc r63311_video_display_on_cmds[] = {
	{DSI_DI_GENERIC_LWRITE, 1, 0, sizeof(manufacturer_cmd_access_protect),
		manufacturer_cmd_access_protect},
	{DSI_DI_DCS_SWRITE, 1, 0, sizeof(nop),
		nop},
	{DSI_DI_DCS_SWRITE, 1, 0, sizeof(nop),
		nop},
	{DSI_DI_GENERIC_LWRITE, 1, 0, sizeof(backlight_ctrl),
		backlight_ctrl},
	{DSI_DI_GENERIC_LWRITE, 1, 0, sizeof(seq_test_ctrl),
		seq_test_ctrl},
	{DSI_DI_DCS_SWRITE1, 1, 0, sizeof(write_display_brightness),
		write_display_brightness},
	{DSI_DI_DCS_SWRITE1, 1, 0, sizeof(write_ctrl_display),
		write_ctrl_display},
	{DSI_DI_DCS_SWRITE, 1, 0, sizeof(display_on),
		display_on},
	{DSI_DI_DCS_SWRITE, 1, 0, sizeof(exit_sleep),
		exit_sleep},
};

static void panel_1080p_init_config(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	set_dsi_low_power_mode(fbi);

	dsi_cmd_array_tx(fbi, r63311_video_display_on_cmds,
			ARRAY_SIZE(r63311_video_display_on_cmds));

	/* restore all lanes to LP-11 state  */
	dsi_lanes_enable(fbi, 1);
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
	else if (FHD_PANEL == is_fhd_lcd())
		fb->modes = video_modes_1080p_emeidkb;
	else
		fb->modes = video_modes_hvga_vnc_emeidkb;

	fb->max_fb_size = ALIGN(fb->modes->xres, 16) *
		fb->modes->yres * 8 + 4096;

	/* align with android format and vres_virtual pitch */
	fb->pix_fmt = PIX_FMT_RGBA888;
	fb->xres_virtual = ALIGN(fb->modes->xres, 16);

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

	fb->phy_type = DSI;
	fb->exter_brige_pwr = NULL;
	if(FHD_PANEL == is_fhd_lcd()) {
		fb->phy_info = (void *)&emeidkb_1080p_dsiinfo;
		fb->pxa168fb_lcd_power = emeidkb_1080p_lcd_power;
		fb->dsi_panel_config = panel_1080p_init_config;
	}else {
		fb->phy_info = (void *)&emeidkb_dsiinfo;
		fb->pxa168fb_lcd_power = emeidkb_lcd_power;
		fb->dsi_panel_config = panel_init_config;
	}

	dsi = (struct dsi_info *)fb->phy_info;
	dsi->master_mode = 1;
	dsi->hfp_en = 0;

	dither_config(fb);
	/*
	 * Re-calculate lcd clk source and divider
	 * according to dsi lanes and output format.
	 */

	if (QHD_PANEL == is_qhd_lcd() || FHD_PANEL == is_fhd_lcd())
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
	 * However, for 1080p panel, it need to use pll3 as lcd
	 * clk source to reach 60 fps.
	 */
	fb->sclk_div &= 0x0fffffff;
	if(FHD_PANEL == is_fhd_lcd())
		fb->sclk_div |= 0xc0000000;
	else
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

	if (!has_feat_video_replace_graphics_dma())
		pxa988_add_fb_ovly(ovly);
}

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
	fb->exter_brige_pwr = NULL;
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
#endif /* CONFIG_MACH_EMEIDKB */

#ifdef CONFIG_MACH_EDEN_FPGA
static struct fb_videomode video_modes_eden[] = {
	[0] = {
		 /* for old Jasper(Bonnel) panel */
		.refresh = 60,
		.xres = 800,
		.yres = 480,
		.hsync_len = 4,
		.left_margin = 212,
		.right_margin = 40,
		.vsync_len = 4,
		.upper_margin = 31,
		.lower_margin = 10,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
		},
	[1] = {
		 /* panel refresh rate should <= 55(Hz) */
		.refresh = 55,
		.xres = 640,
		.yres = 480,
		.hsync_len = 96,
		.left_margin = 16,
		.right_margin = 48,
		.vsync_len = 2,
		.upper_margin = 10,
		.lower_margin = 33,
		.sync = 0,
		},
};

#define DDR_MEM_CTRL_BASE 0xD0000000
#define SDRAM_CONFIG_TYPE1_CS0 0x20

void __init eden_fpga_add_lcd_mipi(void){
	unsigned char __iomem *dmc_membase;
	unsigned int CSn_NO_COL;

	struct pxa168fb_mach_info *fb = &mipi_lcd_info, *ovly =
	    &mipi_lcd_ovly_info;

	fb->num_modes = ARRAY_SIZE(video_modes_eden);
	fb->modes = video_modes_eden;
	fb->max_fb_size = video_modes_eden[0].xres *
		video_modes_eden[0].yres * 8 + 4096;
	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

	/* Re-calculate lcd clk source and divider
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
	eden_add_fb(fb);
	/* add overlay driver */
#ifdef CONFIG_PXA168_V4L2_OVERLAY
	eden_add_v4l2_ovly(ovly);
#else
	eden_add_fb_ovly(ovly);
#endif
}
#endif
