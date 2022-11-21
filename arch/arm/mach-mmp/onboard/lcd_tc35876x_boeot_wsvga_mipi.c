#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/mmp3.h>
#include <mach/pxa988.h>
#include <mach/pxa168fb.h>
#include <mach/regs-mcu.h>
#if defined(CONFIG_MACH_LT02)
#include <mach/mfp-pxa986-lt02.h>
#elif defined(CONFIG_MACH_COCOA7)
#include <mach/mfp-pxa986-cocoa7.h>
#else
#include <mach/mfp-mmp2.h>
#endif
#include <mach/tc35876x.h>

#ifndef CONFIG_BACKLIGHT_TPS61165
enum OUTDOOR {
	OUTDOOR_OFF,
	OUTDOOR_ON,
	OUTDOOR_MAX,
};

enum CABC {
	CABC_OFF,
	CABC_ON,
	CABC_MAX,
};

enum POWER_LUT_LEVEL {
	LUT_LEVEL_MANUAL_AND_INDOOR,
	LUT_LEVEL_OUTDOOR_1,
	LUT_LEVEL_OUTDOOR_2,
	LUT_LEVEL_MAX,
};

static void auto_brightness_work(struct work_struct *work);

struct Vx5b3d_backlight_value {
	const unsigned int max;
	const unsigned int mid;
	const unsigned char low;
	const unsigned char dim;
};

typedef struct Vx5d3b_cabc_info {
	enum OUTDOOR			outdoor;
	enum CABC			cabc;
	enum POWER_LUT_LEVEL		powerLut;

	struct backlight_device		*bd;
	struct lcd_device		*lcd;
	struct Vx5b3d_backlight_value	*vee_lightValue;
	struct device			*dev;
	struct mutex			lock;
	struct mutex			pwr_lock;
	struct mutex			lvds_clk_switch_lock;
	struct workqueue_struct         *workqueue;
	struct delayed_work 		auto_brightness_delayed_work;

	unsigned int			auto_brightness;
	unsigned int			power_lut_idx;
	unsigned int			vee_strenght;
	unsigned int			prevee_strenght;
	unsigned int			first_count;
	unsigned int			lcd_panel;
	int				recovery_mode;
	unsigned int			lvds_clk;
	unsigned int			orig_lvds_clk;
	unsigned int			vx5b3d_backlight_frq;
	int				lvds_clk_switching;
	int				value;
	int 				skip_init;
};

static struct Vx5b3d_backlight_value backlight_table[5] = {
	{	/*BOE/CPT*/
		.max = 235,
		.mid = 135,
		.low = 2,
		.dim = 1,
	}, {	/*SDC*/
		.max = 189,
		.mid = 111,
		.low = 2,
		.dim = 1,
	}, {	/*BOEVE*/
		.max = 235,
		.mid = 135,
		.low = 2,
		.dim = 1,
	}, {	/*BOEOLD*/
		.max = 235,
		.mid = 135,
		.low = 2,
		.dim = 1,
	}, {	/*SDCVE*/
		.max = 235,
		.mid = 135,
		.low = 2,
		.dim = 1,
	}
};

#define V5D3BX_VEESTRENGHT		0x00001f07
#define V5D3BX_VEEDEFAULTVAL		0
#define V5D3BX_DEFAULT_STRENGHT		5
#define V5D3BX_DEFAULT_LOW_STRENGHT	8
#define V5D3BX_DEFAULT_HIGH_STRENGHT	10
#define V5D3BX_MAX_STRENGHT		15

#define V5D3BX_CABCBRIGHTNESSRATIO	815
#define V5D3BX_10KHZ_DEFAULT_RATIO	4707
#define V5D3BX_5P9KHZ_DEFAULT_RATIO	8000
#define V5D3BX_5P9KHZ_50P98_OFFSET	458
#define V5D3BX_5P9KHZ_50P18_OFFSET	329
#define V5D3BX_5P9KHZ_48P52_OFFSET	50

#define AUTOBRIGHTNESS_LIMIT_VALUE	207

#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS_LEVEL		255
#define MID_BRIGHTNESS_LEVEL		195
#define LOW_BRIGHTNESS_LEVEL		20
#define DIM_BRIGHTNESS_LEVEL		19
#define LOW_BATTERY_LEVEL		10
#define MINIMUM_VISIBILITY_LEVEL	30
#define DEFAULT_BRIGHTNESS		MID_BRIGHTNESS_LEVEL
#define LVDS_CLK_48P19Mhz		0
#define LVDS_CLK_50P98Mhz		1
#define LVDS_CLK_50P18Mhz		2
#define LVDS_CLK_48P52Mhz		3

struct Vx5b3d_backlight_value *pwm;
struct class *mdnie_class;
struct Vx5d3b_cabc_info *g_vx5d3b = NULL;
struct pxa168fb_info *fbi_global = NULL;
static int dsi_init(struct pxa168fb_info *fbi);
static int lt02_update_brightness(struct Vx5d3b_cabc_info *g_vx5d3b);

#endif

/*
 * dsi bpp : rgb_mode
 *    16   : DSI_LCD_INPUT_DATA_RGB_MODE_565;
 *    24   : DSI_LCD_INPUT_DATA_RGB_MODE_888;
 */
static struct dsi_info dsiinfo = {
	.id = 1,
	.lanes = 4,
	.bpp = 24,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_888,
	.burst_mode = DSI_BURST_MODE_SYNC_EVENT,
	.hbp_en = 1,
	.hfp_en = 1,
};

/* t7: use DSI2DPI bridge, and 4 lanes */
static struct dsi_info lt02_dsiinfo = {
	.id = 1,
	.lanes = 4,
	.bpp = 24,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_888,
	.burst_mode = DSI_BURST_MODE_SYNC_EVENT,//which mode for lvds bridge????
	.hbp_en = 1,
	.hfp_en = 1,
};

/*
 * FURUIER HSD070PFW3 M.D102.02.BH LCD MODULE,1024(RGB) * 600 PIXELS
 *
 * set according Chimei spec
 * fclk 40.8/51.2/67.2M
 * hsync_len = ?
 * hs period 1114/1344/1400
 * hs blanking 90/320/376
 * vsync_len = ?
 * vs period 610/635/800
 * hs blanking 10/35/200
 *
 */
static struct fb_videomode video_modes_lt02[] = {
	[0] = {
		.refresh = 60,
		.xres = 1024,
		.yres = 600,
		.hsync_len = 20,
		.left_margin = 150,   /* left_margin should >=3 */
		.right_margin = 150, /* right_margin should >=62 */
		.vsync_len = 3,
		.upper_margin = 15,  /* upper_margin should >= 6 */
		.lower_margin = 15,  /* lower_margin should >= 6 */
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};

static struct pxa168fb_mach_info mipi_lcd_info = {
	.id = "GFX Layer",
	.num_modes = 0,
	.modes = NULL,
	.sclk_div = 0xE0001108,
	.pix_fmt = PIX_FMT_RGBA888,
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
	.pix_fmt = PIX_FMT_RGBA888,
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

	if (fbi->skip_pw_on)
		return ret;

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

	/*  reset the bridge */
	if (mi->xcvr_reset && !fbi->skip_pw_on) {
		mi->xcvr_reset(fbi);
	}

	/* set dsi to dpi conversion chip */
	if (mi->phy_type == DSI2DPI && !fbi->skip_pw_on) {
		ret = mi->dsi2dpi_set(fbi);
		if (ret < 0)
			pr_err("dsi2dpi_set error!\n");
	}

	/* put all lanes to LP-11 state  */
	dsi_lanes_enable(fbi, 0);
	dsi_lanes_enable(fbi, 1);

	/* if panel not enabled, init panel settings via dsi */
	if (mi->phy_type == DSI && !fbi->skip_pw_on)
		mi->dsi_panel_config(fbi);

	/* set dsi controller */
	dsi_set_controller(fbi);

	/*
	if no EXTCLK, we need supply DSICLK 1usec before i2c command
	DSICLK setup time before active I2C transaction    1 sec
	DSICLK hold time after last active I2C transaction 1 sec
	*/
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

	bitclk_div *= 2;

	mi->sclk_src = bitclk * bitclk_div;
	/*
	 * mi->sclk_src = pclk * pclk_div;
	 * pclk / bitclk  = pclk / (8 * byteclk) = pclk2bclk_rate / 8;
	 * pclk_div / bitclk_div = 8 / pclk2bclk_rate;
	 */
	pclk_div = (bitclk_div << 3) / pclk2bclk_rate;

	mi->sclk_div &= ~(DSI1_BITCLK_DIV_MASK | CLK_INT_DIV_MASK);
	mi->sclk_div |= DSI1_BITCLK(bitclk_div) | CLK_INT_DIV(pclk_div);
	/* printk("mi->sclk_dev =[%x]",mi->sclk_div); */
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

#ifndef CONFIG_BACKLIGHT_TPS61165

static ssize_t auto_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	char *pos = buf;
	/*int i;*/

	pos += sprintf(pos, "%d, %d, ", Vee_cabc->auto_brightness, Vee_cabc->power_lut_idx);
	/*
	for (i = 0; i < 5; i++)
		pos += sprintf(pos, "0x%02x, ", power_lut[mdnie->power_lut_idx][0][i]);
	pos += sprintf(pos, "\n");
	*/
	return pos - buf;
}

static void auto_brightness_work(struct work_struct *work)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	int ret = 0;

	pr_info("%s\n", __func__);

	if (fbi_global->active == 0)
		return;	

	pr_info("auto_brightness[%d] -> [%d]\n",\
			Vee_cabc->auto_brightness, Vee_cabc->value);

	mutex_lock(&Vee_cabc->lock);
	if (Vee_cabc->value == 0) {
		pr_info("vee off [%d]\n", Vee_cabc->value);
		ret |= tc35876x_write32(0x710,0x054D000B );
		mdelay(1);
		ret |= tc35876x_write32(0x174,0x0);
	} else if (Vee_cabc->auto_brightness != Vee_cabc->value){
		pr_info("vee on [%d]\n", Vee_cabc->value);
		ret |= tc35876x_write32(0x710,0x054D004B );
		mdelay(1);
		ret |= tc35876x_write32(0x174,0xff);
	}
	mutex_unlock(&Vee_cabc->lock);

	mdelay(1);

	if (ret < 0)
		pr_info("tc35876x_i2c_write fail [%d] ! \n",ret);

	mutex_lock(&Vee_cabc->lock);

	Vee_cabc->auto_brightness = Vee_cabc->value;

	Vee_cabc->cabc = (Vee_cabc->value) ? CABC_ON : CABC_OFF;

	lt02_update_brightness(Vee_cabc);

	mutex_unlock(&Vee_cabc->lock);
}

static ssize_t auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	int value = 0, rc = 0;


	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		Vee_cabc->value = value;
		if ((fbi_global->active == 0) && (Vee_cabc->skip_init)) {
			queue_delayed_work(Vee_cabc->workqueue, &(Vee_cabc->auto_brightness_delayed_work),
                        msecs_to_jiffies(750));
		}
		else {
			queue_delayed_work(Vee_cabc->workqueue, &(Vee_cabc->auto_brightness_delayed_work),
                        msecs_to_jiffies(0));
		}
	}

	return size;
}

static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);

static ssize_t vee_strenght_store(struct device *dev, struct
device_attribute *attr, const char *buf, size_t size)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;

	int value;
	u32 vee_value = 0x00001f07;
	int rc;

	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		dev_info(dev, "vee_strenght_store[0x400] - %d, %d\n", \
			Vee_cabc->vee_strenght, value);

		if (Vee_cabc->vee_strenght!= value) {
			Vee_cabc->vee_strenght = value;
			vee_value = vee_value | (value << 27);
			tc35876x_write32(0x400,vee_value);
			vx5d3b_i2c_release();
			pr_info("vee_strenght value [0x%x]\n",vee_value);
		}

		return size;
	}
}
static DEVICE_ATTR(vee_strenght, 0664,NULL, vee_strenght_store);

static ssize_t vx5b3dxreg_write_store(struct device *dev, struct
device_attribute *attr, const char *buf, size_t size)
{
	int value;
	static u32 vee_data = 0;
	static u16 vee_register = 0;
	int rc;

	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		static int cnt;
		if ( cnt == 0 )
			vee_register = (u16)value;
		else
			vee_data = value;
		cnt++;
		printk("count value loop =[%d]\n",cnt);
		if (cnt == 2) {
			tc35876x_write32(vee_register,vee_data);
			dev_info(dev, "vx5b3dx register[0x%x]..data[0x%x]\n", \
				vee_register, vee_data);
			cnt = 0;
		}

	}
	return size;
}
static DEVICE_ATTR(veeregwrite, 0664,NULL, vx5b3dxreg_write_store);

static ssize_t vx5b3d_Regread_store(struct device *dev, struct
device_attribute *attr, const char *buf, size_t size)
{
	unsigned long value = 0;
	unsigned long ret_value = 0;
	int rc = 0;

	if (fbi_global->active == 0)
		return size;

	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		vx5d3b_i2c_read(value,&ret_value,4);
		pr_info("vx5b3d_Regread_register[0x%x]..return=[0x%x]\n",\
			value,ret_value);

		return size;
	}
}
static DEVICE_ATTR(vx5b3d_Regread, 0664,NULL, vx5b3d_Regread_store);

 ssize_t lvds_clk_switch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;

	int ret = 0;
	ret = sprintf(buf, "%d\n", Vee_cabc->lvds_clk);
	return ret;
}

 ssize_t lvds_clk_switch_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)

{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	int ret = 0;
	unsigned int value = 0;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if(value == Vee_cabc->lvds_clk)
	{
		printk(" lvds clk has been what you want,so not change\n");

		return -1;
	}
	else if( value > LVDS_CLK_48P52Mhz)
	{
		printk(" invalid lvds freq index!!!\n");
		return -1;
	}

	printk("%s:lvds clk swith to %\n",Vee_cabc->lvds_clk);

	/*just for user mode.
	for test mode,return -1 directly*/
	if (fbi_global->active == 0)
	{
		mutex_lock(&Vee_cabc->lvds_clk_switch_lock);
		/*save new lvds clk temprarily*/
		Vee_cabc->lvds_clk = value;
		/*update video_modes for every lvds clk update request during lcd off,
		lvds setting will be updated during lcd resume */
		pxa168fb_update_modes(fbi_global,Vee_cabc->lvds_clk,Vee_cabc->lcd_panel);
		mutex_unlock(&Vee_cabc->lvds_clk_switch_lock);
		return -1;
	}


	mutex_lock(&Vee_cabc->lvds_clk_switch_lock);

	Vee_cabc->lvds_clk = value;

	Vee_cabc->lvds_clk_switching = true;

	lt02_update_brightness(g_vx5d3b);

	pxa168fb_update_modes(fbi_global,Vee_cabc->lvds_clk,Vee_cabc->lcd_panel);
	dsi_init(fbi_global);

	Vee_cabc->orig_lvds_clk = Vee_cabc->lvds_clk;
	Vee_cabc->lvds_clk_switching = false;

	lt02_update_brightness(g_vx5d3b);

	mutex_unlock(&Vee_cabc->lvds_clk_switch_lock);

	return count;
}


DEVICE_ATTR(lvds_clk_switch, 0664, lvds_clk_switch_show, lvds_clk_switch_store);
static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	char temp[20];
/*
#define LT02_CPT_PANEL 0
#define LT02_SDC_PANEL 1
#define LT02_BOEVE_PANEL 2
#define LT02_BOEOLD_PANEL 3
#define LT02_SDCVE_PANEL 4 // VE_GROUP
*/
	/*VE_GROUP TODO*/
	if (Vee_cabc->lcd_panel == 0) /*CPT*/
		sprintf(temp, "CPT_LTL070NL01\n");
	else if (Vee_cabc->lcd_panel == 1) /*SDC*/
		sprintf(temp, "SDC_LTL070NL01\n");
	else if (Vee_cabc->lcd_panel == 2) /*BOEVE*/
		sprintf(temp, "BOE_VEHV070WSA\n");
	else if (Vee_cabc->lcd_panel == 3) /*BOEOLD*/
		sprintf(temp, "BOE_HV070WSA\n");
	else if (Vee_cabc->lcd_panel == 4) /*SDCVE*/
		sprintf(temp, "SDC_VELTL070NL\n");
	else
		sprintf(temp, "BOE_HV070WSA\n");

	strcat(buf, temp);
	return strlen(buf);
}
static DEVICE_ATTR(lcd_type, 0664, lcd_type_show, NULL);

static ssize_t cabc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;

	return sprintf(buf, "%d\n", Vee_cabc->cabc);
}

static ssize_t cabc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	unsigned int value = 0;
	int ret = 0;

	if (fbi_global->active == 0)
		return count;

	if (Vee_cabc->auto_brightness)
		return count;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (value >= CABC_MAX)
		value = CABC_OFF;

	value = (value) ? CABC_ON : CABC_OFF;

	mutex_lock(&Vee_cabc->lock);
	Vee_cabc->cabc = value;
	lt02_update_brightness(Vee_cabc);
	mutex_unlock(&Vee_cabc->lock);

	return count;
}

static struct device_attribute mdnie_attributes[] = {
	__ATTR(cabc, 0664, cabc_show, cabc_store),
	__ATTR_NULL,
};

void vx5b3dx_backlightReg_off(void)
{
	tc35876x_write32(0x164, 0x0);
	mdelay(1);
	tc35876x_write32(0x15c, 0x0);
}

void vx5b3dx_backlightReg_on(void)
{
	struct Vx5d3b_cabc_info *vx5d3b = g_vx5d3b;
	unsigned int  lcd_internal_ldo_en = 0, v_sys_lcd = 0;


	lcd_internal_ldo_en = mfp_to_gpio(GPIO097_GPIO_97);
	v_sys_lcd = mfp_to_gpio(GPIO098_GPIO_98);

	if (gpio_request(lcd_internal_ldo_en, "lcd internal ldo en")) {
		pr_err("gpio %d request failed\n", lcd_internal_ldo_en);
	}

	if (gpio_request(v_sys_lcd, "lcd sys lcd")) {
		pr_err("gpio %d request failed\n", v_sys_lcd);
	}


	gpio_direction_output(v_sys_lcd, 1);
	gpio_direction_output(lcd_internal_ldo_en, 1);
	mdelay(1);

	if(LVDS_CLK_50P98Mhz == vx5d3b->lvds_clk)
		tc35876x_write32(0x160, 0x86C);
	else if(LVDS_CLK_50P18Mhz == vx5d3b->lvds_clk)
		tc35876x_write32(0x160, 0x84C);
	else if(LVDS_CLK_48P52Mhz == vx5d3b->lvds_clk)
		tc35876x_write32(0x160, 0x805);
	else
		tc35876x_write32(0x160, 0x7F8); /*Default for 48.2Mhz*/

	/*tc35876x_write32(0x164, 0x0);*/
	tc35876x_write32(0x604, 0x3FFFFFE0/*0xff*/);
	msleep(200);
	tc35876x_write32(0x138, 0x3fff0000);
	tc35876x_write32(0x15c, 0x5);
	
	gpio_free(lcd_internal_ldo_en);
	gpio_free(v_sys_lcd);
	
}
static int lt02_set_brightness(struct backlight_device *bd)
{
	struct Vx5d3b_cabc_info *vx5d3b = g_vx5d3b;
	int ret = 0;

	if (fbi_global->active)
		lt02_update_brightness(vx5d3b);
	
	return ret;
}

static int lt02_update_brightness(struct Vx5d3b_cabc_info *g_vx5d3b)
{
	struct Vx5b3d_backlight_value *pwm = g_vx5d3b->vee_lightValue;

	int brightness = g_vx5d3b->bd->props.brightness;

	int vx5b3d_brightness = 0;
	u32 vee_strenght = 0;
	int ret = 0;

	if (g_vx5d3b->bd->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (g_vx5d3b->bd->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if(g_vx5d3b->lvds_clk_switching)
		brightness = 0;

	/*
	register 0x160
	register 0x164
			  value of 0x164
	---> duty ratio = -------------
			  value of 0x160
	*/
	if ((g_vx5d3b->cabc) && (g_vx5d3b->auto_brightness >= 5)) {
		if (brightness <= AUTOBRIGHTNESS_LIMIT_VALUE)
			brightness = AUTOBRIGHTNESS_LIMIT_VALUE;
	}
	/* brightness tuning*/
	if (brightness > MAX_BRIGHTNESS_LEVEL)
		brightness = MAX_BRIGHTNESS_LEVEL;

	if (brightness == LOW_BATTERY_LEVEL)/*Outgoing Quality Control Group issue*/
		brightness = MINIMUM_VISIBILITY_LEVEL;

	if (brightness >= MID_BRIGHTNESS_LEVEL) {
		vx5b3d_brightness  = (brightness - MID_BRIGHTNESS_LEVEL) *
		(pwm->max - pwm->mid) / (MAX_BRIGHTNESS_LEVEL-MID_BRIGHTNESS_LEVEL) + pwm->mid;
	} else if (brightness >= LOW_BRIGHTNESS_LEVEL) {
		vx5b3d_brightness  = (brightness - LOW_BRIGHTNESS_LEVEL) *
		(pwm->mid - pwm->low) / (MID_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + pwm->low;
	} else if (brightness >= DIM_BRIGHTNESS_LEVEL) {
		vx5b3d_brightness  = (brightness - DIM_BRIGHTNESS_LEVEL) *
		(pwm->low - pwm->dim) / (LOW_BRIGHTNESS_LEVEL-DIM_BRIGHTNESS_LEVEL) + pwm->dim;
	} else if (brightness > 0)
		vx5b3d_brightness  = pwm->dim;
	else {
		vx5b3d_brightness = 0;
		pr_info("brightness = [%d]: vx5b3d_brightness = [%d]\n",\
			brightness,vx5b3d_brightness);
	}

	if (g_vx5d3b->cabc) {

		switch (g_vx5d3b->auto_brightness) {

		case	0 ... 3:
			g_vx5d3b->vee_strenght = V5D3BX_DEFAULT_STRENGHT;
			break;
		case	4 ... 5:
			g_vx5d3b->vee_strenght = V5D3BX_DEFAULT_LOW_STRENGHT;
			break;
		case	6 ... 8:
			g_vx5d3b->vee_strenght = V5D3BX_DEFAULT_HIGH_STRENGHT;
			break;
		default:
			g_vx5d3b->vee_strenght = V5D3BX_DEFAULT_STRENGHT;
		}
/*
	if (g_vx5d3b->auto_brightness >= 5)
		g_vx5d3b->vee_strenght = V5D3BX_MAX_STRENGHT;
*/
		vee_strenght = V5D3BX_VEESTRENGHT | ((g_vx5d3b->vee_strenght) << 27);

	if (!(g_vx5d3b->auto_brightness >= 5))
		vx5b3d_brightness = (vx5b3d_brightness * V5D3BX_CABCBRIGHTNESSRATIO) / 1000;

	} else {
		vee_strenght = V5D3BX_VEESTRENGHT | (V5D3BX_VEEDEFAULTVAL << 27);
	}

	/* brightness setting from platform is from 0 to 255 */
	mutex_lock(&g_vx5d3b->pwr_lock);

	if (LVDS_CLK_50P98Mhz == g_vx5d3b->lvds_clk)
		g_vx5d3b->vx5b3d_backlight_frq = V5D3BX_5P9KHZ_DEFAULT_RATIO + V5D3BX_5P9KHZ_50P98_OFFSET;
	else if (LVDS_CLK_50P18Mhz == g_vx5d3b->lvds_clk)
		g_vx5d3b->vx5b3d_backlight_frq = V5D3BX_5P9KHZ_DEFAULT_RATIO + V5D3BX_5P9KHZ_50P18_OFFSET;
	else if (LVDS_CLK_48P52Mhz == g_vx5d3b->lvds_clk)
		g_vx5d3b->vx5b3d_backlight_frq = V5D3BX_5P9KHZ_DEFAULT_RATIO + V5D3BX_5P9KHZ_48P52_OFFSET;
	else	/*Default for 48.2Mhz*/
		g_vx5d3b->vx5b3d_backlight_frq = V5D3BX_5P9KHZ_DEFAULT_RATIO;

	if ((g_vx5d3b->prevee_strenght != vee_strenght) && (brightness != 0))
		ret |= tc35876x_write32(0x400,vee_strenght);

	if (!g_vx5d3b->first_count)
		ret |= tc35876x_write32(0x164,((vx5b3d_brightness * g_vx5d3b->vx5b3d_backlight_frq)/1000));

	/*backlight duty ration control when device is first backlight on.*/
	if (g_vx5d3b->first_count && brightness != 0) {
		printk("backlight control first...[%d] \n",brightness);
		vx5b3dx_backlightReg_on();
		ret |= tc35876x_write32(0x164,((vx5b3d_brightness * g_vx5d3b->vx5b3d_backlight_frq)/1000));
		g_vx5d3b->first_count = false;
	}

	g_vx5d3b->prevee_strenght = vee_strenght;

	mutex_unlock(&g_vx5d3b->pwr_lock);

	if (ret < 0)
		pr_info("tc35876x_i2c_write fail [%d] ! \n",ret);

	return 0;
}

static int lt02_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops lt02_backlight_ops = {
	.get_brightness = lt02_get_brightness,
	.update_status = lt02_set_brightness,
};
#endif

static int lt02_lcd_power(struct pxa168fb_info *fbi,
                            unsigned int spi_gpio_cs,
                            unsigned int spi_gpio_reset, int on)
{
	struct Vx5d3b_cabc_info *vx5d3b = g_vx5d3b;
	static struct regulator *lvds1_1p2 = NULL,*lvds1_1p8 = NULL,*lvds1_3p3 = NULL;
	unsigned int  lcd_lvds_rst = 0,lcd_internal_ldo_en = 0, v_sys_lcd = 0;

	if (fbi_global == NULL)
		fbi_global = fbi;


	lcd_internal_ldo_en = mfp_to_gpio(GPIO097_GPIO_97);
	v_sys_lcd = mfp_to_gpio(GPIO098_GPIO_98);
	lcd_lvds_rst = mfp_to_gpio(GPIO018_GPIO_18);

	if (gpio_request(lcd_internal_ldo_en, "lcd internal ldo en")) {
		pr_err("gpio %d request failed\n", lcd_internal_ldo_en);
		goto regu_lcd_vdd;
	}

	if (gpio_request(v_sys_lcd, "lcd sys lcd")) {
		pr_err("gpio %d request failed\n", v_sys_lcd);
		goto regu_lcd_vdd;
	}

	if (gpio_request(lcd_lvds_rst, "lcd_lvds_rst")) {
		pr_err("gpio %d request failed\n", lcd_lvds_rst);
		goto regu_lcd_vdd;
	}

	if (!lvds1_3p3) {
		lvds1_3p3 = regulator_get(NULL, "v_ldo14_3v");
		if (IS_ERR(lvds1_3p3)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_vdd;
		}
	}

	if (!lvds1_1p2) {
		lvds1_1p2 = regulator_get(NULL, "v_lvds_1v2");
		if (IS_ERR(lvds1_1p2)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_vdd;
		}
	}

	if (!lvds1_1p8) {
		lvds1_1p8 = regulator_get(NULL, "v_lvds_1v8");
		if (IS_ERR(lvds1_1p8)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_vdd;
		}
	}

	mutex_lock(&vx5d3b->pwr_lock);

	if (on) {

		/*
			MIPI bridge must power on as below sequence or at the same time
			1.2V, to digital core
			1.2V, to DSI-RX PHY
			3.3V, to LVDS-TX PHY
			1.8V or 3.3V, to digital I/Os
		*/

			regulator_set_voltage(lvds1_1p8, 1800000, 1800000);
			regulator_enable(lvds1_1p8);
			regulator_set_voltage(lvds1_3p3, 3300000, 3300000);
			regulator_enable(lvds1_3p3);

			regulator_set_voltage(lvds1_1p2, 1200000, 1200000);
			regulator_enable(lvds1_1p2);

			mdelay(5);
			/*
			gpio_direction_output(v_sys_lcd, 1);
			gpio_direction_output(lcd_internal_ldo_en, 1);
			*/
			pr_info("lt02_lcd_power on !\n");

	} else {
			/* Backlight off*/
			cancel_delayed_work(&g_vx5d3b->auto_brightness_delayed_work);
			g_vx5d3b->prevee_strenght = 0;
			g_vx5d3b->auto_brightness = 0;
			if (!g_vx5d3b->skip_init)
				g_vx5d3b->skip_init = 1;
			vx5b3dx_backlightReg_off();

			msleep(200);
			/*mipi signal off*/
			gpio_direction_output(lcd_lvds_rst, 0);
			msleep(1);
			gpio_direction_output(lcd_internal_ldo_en, 0);
			gpio_direction_output(v_sys_lcd, 0);

			regulator_disable(lvds1_3p3);
			regulator_disable(lvds1_1p2);
			/*Caused by i2c fail*/
			if (system_rev >= LT02_R0_4)
			regulator_disable(lvds1_1p8);

			pr_info("lt02_lcd_power off !\n");
			msleep(200);
	}

	mutex_unlock(&vx5d3b->pwr_lock);

	gpio_free(lcd_internal_ldo_en);
	gpio_free(lcd_lvds_rst);
	gpio_free(v_sys_lcd);

	pr_debug("%s on %d\n", __func__, on);

	return 0;

regu_lcd_vdd:
	lvds1_1p2 = NULL;
	lvds1_1p8 = NULL;
	lvds1_3p3 = NULL;

	regulator_put(lvds1_1p2);
	regulator_put(lvds1_1p8);
	regulator_put(lvds1_3p3);

	gpio_free(lcd_internal_ldo_en);
	gpio_free(lcd_lvds_rst);
	gpio_free(v_sys_lcd);

	return -EIO;
}

static int tc358765_reset(struct pxa168fb_info *fbi)
{
	int gpio;

#if defined(CONFIG_MACH_EMEIDKB) || defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	gpio = mfp_to_gpio(GPIO018_GPIO_18);
#endif
	if (gpio_request(gpio, "mipi bridge reset")) {
		pr_info("gpio %d request failed\n", gpio);
		return -1;
	}

	gpio_direction_output(gpio, 0);
	udelay(10);
	gpio_direction_output(gpio, 1);
	udelay(10);

	gpio_free(gpio);
	pr_info("tc358765_reset\n");

	return 0;

}

static void tc358765_dump(void)
{
#if 0
	u32 val;

	pr_info("%s\n", __func__);
	tc35876x_read32(PPI_TX_RX_TA, &val);
	pr_info(" - PPI_TX_RX_TA = 0x%x\n", val);
	tc35876x_read32(PPI_LPTXTIMECNT, &val);
	pr_info(" - PPI_LPTXTIMECNT = 0x%x\n", val);
	tc35876x_read32(PPI_D0S_CLRSIPOCOUNT, &val);
	pr_info(" - PPI_D0S_CLRSIPOCOUNT = 0x%x\n", val);
	tc35876x_read32(PPI_D1S_CLRSIPOCOUNT, &val);
	pr_info(" - PPI_D1S_CLRSIPOCOUNT = 0x%x\n", val);

	tc35876x_read32(PPI_D2S_CLRSIPOCOUNT, &val);
	pr_info(" - PPI_D2S_CLRSIPOCOUNT = 0x%x\n", val);
	tc35876x_read32(PPI_D3S_CLRSIPOCOUNT, &val);
	pr_info(" - PPI_D3S_CLRSIPOCOUNT = 0x%x\n", val);

	tc35876x_read32(PPI_LANEENABLE, &val);
	pr_info(" - PPI_LANEENABLE = 0x%x\n", val);
	tc35876x_read32(DSI_LANEENABLE, &val);
	pr_info(" - DSI_LANEENABLE = 0x%x\n", val);
	tc35876x_read32(PPI_STARTPPI, &val);
	pr_info(" - PPI_STARTPPI = 0x%x\n", val);
	tc35876x_read32(DSI_STARTDSI, &val);
	pr_info(" - DSI_STARTDSI = 0x%x\n", val);

	tc35876x_read32(VPCTRL, &val);
	pr_info(" - VPCTRL = 0x%x\n", val);
	tc35876x_read32(HTIM1, &val);
	pr_info(" - HTIM1 = 0x%x\n", val);
	tc35876x_read32(HTIM2, &val);
	pr_info(" - HTIM2 = 0x%x\n", val);
	tc35876x_read32(VTIM1, &val);
	pr_info(" - VTIM1 = 0x%x\n", val);
	tc35876x_read32(VTIM2, &val);
	pr_info(" - VTIM2 = 0x%x\n", val);
	tc35876x_read32(VFUEN, &val);
	pr_info(" - VFUEN = 0x%x\n", val);
	tc35876x_read32(LVCFG, &val);
	pr_info(" - LVCFG = 0x%x\n", val);

	tc35876x_read32(DSI_INTSTAUS, &val);
	pr_info("!! - DSI_INTSTAUS= 0x%x BEFORE\n", val);
	tc35876x_write32(DSI_INTCLR, 0xFFFFFFFF);
	tc35876x_read32(DSI_INTSTAUS, &val);
	pr_info("!! - DSI_INTSTAUS= 0x%x AFTER\n", val);

	tc35876x_read32(DSI_LANESTATUS0, &val);
	pr_info(" - DSI_LANESTATUS0= 0x%x\n", val);
	tc35876x_read32(DSIERRCNT, &val);
	pr_info(" - DSIERRCNT= 0x%x\n", val);
	tc35876x_read32(DSIERRCNT, &val);
	pr_info(" - DSIERRCNT= 0x%x AGAIN\n", val);
	tc35876x_read32(SYSSTAT, &val);
	pr_info(" - SYSSTAT= 0x%x\n", val);
#endif

}

static int dsi_set_tc358765(struct pxa168fb_info *fbi)
{
	struct Vx5d3b_cabc_info *vx5d3b = g_vx5d3b;

	pr_info("VX5B3D ...START.....\n");

	set_dsi_low_power_mode(fbi);
	mdelay(20);

	printk("lvds clk @resume =%d\n",vx5d3b->lvds_clk);

	mutex_lock(&vx5d3b->pwr_lock);

	vx5d3b_mipi_write(fbi,0x700, 0x6C900040,4);

#if defined(CONFIG_MACH_LT02)
	if (system_rev >= LT02_R0_3)
	{
		if(LVDS_CLK_50P18Mhz == vx5d3b->lvds_clk)
		vx5d3b_mipi_write(fbi,0x704, 0x30438,4);
		else if(LVDS_CLK_50P98Mhz == vx5d3b->lvds_clk)
		vx5d3b_mipi_write(fbi,0x704, 0x30449,4);
		else if(LVDS_CLK_48P19Mhz == vx5d3b->lvds_clk)
		vx5d3b_mipi_write(fbi,0x704, 0x3040d,4);
		else if(LVDS_CLK_48P52Mhz == vx5d3b->lvds_clk)
                vx5d3b_mipi_write(fbi,0x704, 0x30414,4);/*48.52Mh*/
		else
			printk("invalid lvds clk!!!\n");
	}
	else
	vx5d3b_mipi_write(fbi,0x704, 0x302BE,4);
#elif defined(CONFIG_MACH_COCOA7)
	if (system_rev >= COCOA7_R0_3)
	vx5d3b_mipi_write(fbi,0x704, 0x3040D,4);
	else
	vx5d3b_mipi_write(fbi,0x704, 0x302BE,4);
#else
	vx5d3b_mipi_write(fbi,0x704, 0x302BE,4);
#endif
	vx5d3b_mipi_write(fbi,0x70C, 0x00004604,4);
	vx5d3b_mipi_write(fbi,0x710, /*0x54D004B vee off*/0x54D000B,4);
	vx5d3b_mipi_write(fbi,0x714, 0x20,4);
	vx5d3b_mipi_write(fbi,0x718, 0x00000102,4);
	vx5d3b_mipi_write(fbi,0x71C, 0xA8002F,4);
	vx5d3b_mipi_write(fbi,0x720, 0x0,4);

	vx5d3b_mipi_write(fbi,0x154, 0x00000000,4);
	vx5d3b_mipi_write(fbi,0x154, 0x80000000,4);
	mdelay(1); /*For pll locking*/
	vx5d3b_mipi_write(fbi,0x700, 0x6C900840,4);
	vx5d3b_mipi_write(fbi,0x70C, 0x5E56/*0x5E46*//*0x5646*/,4);
	vx5d3b_mipi_write(fbi,0x718, 0x00000202,4);


	vx5d3b_mipi_write(fbi,0x154, 0x00000000,4);
	vx5d3b_mipi_write(fbi,0x154, 0x80000000,4);
	mdelay(1); /*For pll locking*/
	vx5d3b_mipi_write(fbi,0x37C, 0x00001063,4);
	vx5d3b_mipi_write(fbi,0x380, 0x82A86030,4);
	vx5d3b_mipi_write(fbi,0x384, 0x2861408B,4);
	vx5d3b_mipi_write(fbi,0x388, 0x00130285,4);
	vx5d3b_mipi_write(fbi,0x38C, 0x10630009,4);
	vx5d3b_mipi_write(fbi,0x394, 0x400B82A8,4);
	vx5d3b_mipi_write(fbi,0x600, 0x16CC78C,4);
	vx5d3b_mipi_write(fbi,0x604, 0x3FFFFFFF,4);/*lvds disable*/
	vx5d3b_mipi_write(fbi,0x608, 0xD8C,4);

	vx5d3b_mipi_write(fbi,0x154, 0x00000000,4);
	vx5d3b_mipi_write(fbi,0x154, 0x80000000,4);

	/* ...move for system reset command (0x158)*/

	vx5d3b_mipi_write(fbi,0x120, 0x5,4);

	if(LVDS_CLK_50P18Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x124, 0x0512C400,4);
	else if(LVDS_CLK_50P98Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x124, 0x0512c400,4);
	else if(LVDS_CLK_48P19Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x124, 0x04d2C400,4);
	else if(LVDS_CLK_48P52Mhz == vx5d3b->lvds_clk)
        vx5d3b_mipi_write(fbi,0x124, 0x04d2C400,4);
	else
		printk("invalid lvds clk!!!\n");

	vx5d3b_mipi_write(fbi,0x128, 0x104010,4);

	if(LVDS_CLK_50P18Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x12C, 0x93,4);
	else if(LVDS_CLK_50P98Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x12C, 0x95,4);
	else if(LVDS_CLK_48P19Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x12C, 0x8d,4);
	else if(LVDS_CLK_48P52Mhz == vx5d3b->lvds_clk)
        vx5d3b_mipi_write(fbi,0x12C, 0x89,4);
	else
		printk("invalid lvds clk!!!\n");

	vx5d3b_mipi_write(fbi,0x130, 0x3C18,4);
	vx5d3b_mipi_write(fbi,0x134, 0x15,4);
	vx5d3b_mipi_write(fbi,0x138, 0xFF8000,4);
	vx5d3b_mipi_write(fbi,0x13C, 0x0,4);


	/*PWM  100 % duty ration*/

	vx5d3b_mipi_write(fbi,0x114, 0xc6302,4);
	/*backlight duty ration control when device is first bring up.*/

	/*
	vx5d3b_mipi_write(fbi,0x160, 0x4B4,4);
	vx5d3b_mipi_write(fbi,0x138, 0x3fff0000,4);
	vx5d3b_mipi_write(fbi,0x15c, 0x5,4);
	*/
	/* END...*/

	vx5d3b_mipi_write(fbi,0x140, 0x10000,4);
	/*Add for power consumtion*/
	vx5d3b_mipi_write(fbi,0x174, /*0xff vee off*/0x0,4);
	/*end*/

	/*
	slope = 2 / variance = 0x55550022
	slope register [15,10]
	*/
	vx5d3b_mipi_write(fbi, 0x404, 0x55550822,4);
	/*
	To minimize the text effect
	this value from 0xa to 0xf
	*/
	vx5d3b_mipi_write(fbi, 0x418, 0x555502ff,4);
	/*
	Disable brightnes issue Caused by IBC
	read 4 bytes from address 0x410 to 0x413
	0x15E50300 is read value for 0x410 register
	0x5E50300= 0x15E50300 & 0xefffffff
	 */
	vx5d3b_mipi_write(fbi,0x410, 0x5E50300,4);
	/*...end*/

	vx5d3b_mipi_write(fbi,0x20C, 0x124,4);
	vx5d3b_mipi_write(fbi,0x21C, 0x0,4);
	vx5d3b_mipi_write(fbi,0x224, 0x7,4);
	vx5d3b_mipi_write(fbi,0x228, 0x50001,4);
	vx5d3b_mipi_write(fbi,0x22C, 0xFF03,4);
	vx5d3b_mipi_write(fbi,0x230, 0x1,4);
	vx5d3b_mipi_write(fbi,0x234, 0xCA033E10,4);
	vx5d3b_mipi_write(fbi,0x238, 0x00000060,4);
	vx5d3b_mipi_write(fbi,0x23C, 0x82E86030,4);
	vx5d3b_mipi_write(fbi,0x244, 0x001E0285,4);

	if(LVDS_CLK_50P18Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x258, 0x30014,4);
	else if(LVDS_CLK_50P98Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x258, 0x30014,4);
	else if(LVDS_CLK_48P19Mhz == vx5d3b->lvds_clk)
	vx5d3b_mipi_write(fbi,0x258, 0x30013,4);
	else if(LVDS_CLK_48P52Mhz == vx5d3b->lvds_clk)
        vx5d3b_mipi_write(fbi,0x258, 0x30013,4);
	else
		printk("invalid lvds clk!!!\n");

	/*vee strenght initialization*/
	vx5d3b_mipi_write(fbi,0x400, 0x0,4);

	vx5d3b_mipi_write(fbi,0x158, 0x0,4);
	vx5d3b_mipi_write(fbi,0x158, 0x1,4);

	mdelay(1); /*For pll locking*/

	/* put all lanes to LP-11 state  */
	dsi_lanes_enable(fbi, 1);

	g_vx5d3b->first_count = true;

	mutex_unlock(&vx5d3b->pwr_lock);

	lt02_update_brightness(g_vx5d3b);

	pr_info("VX5B3D ..END.....\n");
	return 0;
}


void __init lt02_add_lcd_mipi(void)
{
	struct dsi_info *dsi = NULL;
	struct pxa168fb_mach_info *fb = &mipi_lcd_info, *ovly =
	    &mipi_lcd_ovly_info;

	unsigned int CSn_NO_COL;
	int ret = 0;
	struct Vx5d3b_cabc_info *vx5d3bInfo;

	fb->num_modes = ARRAY_SIZE(video_modes_lt02);
	fb->modes = video_modes_lt02;
	fb->max_fb_size = ALIGN(fb->modes->xres, 16) *
		fb->modes->yres * 4 * 3 + 4096;

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

#ifdef CONFIG_TC35876X
	fb->phy_type = DSI2DPI;
	fb->xcvr_reset = tc358765_reset;
	fb->dsi2dpi_set = dsi_set_tc358765;
#endif
	fb->phy_info = (void *)&lt02_dsiinfo;
	/* no dsi panel init cmd, because of mipi bridge */
	fb->dsi_panel_config = NULL;
	fb->pxa168fb_lcd_power = lt02_lcd_power;

	dsi = (struct dsi_info *)fb->phy_info;
	dsi->master_mode = 1;
	dsi->hfp_en = 0;

#ifndef CONFIG_BACKLIGHT_TPS61165
	/*For v5b3dx cabc*/
	mdnie_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR_OR_NULL(mdnie_class)) {
		pr_err("failed to create mdnie class\n");
	}

	mdnie_class->dev_attrs = mdnie_attributes;

	vx5d3bInfo = kzalloc(sizeof(struct Vx5d3b_cabc_info), GFP_KERNEL);

	if (!vx5d3bInfo) {
		pr_err("failed to allocate vx5d3bInfo\n");
		ret = -ENOMEM;
		goto error1;
	}

	vx5d3bInfo->dev = device_create(mdnie_class, NULL, 0, &vx5d3bInfo, "mdnie");

	if (IS_ERR_OR_NULL(vx5d3bInfo->dev)) {
		pr_err("failed to create mdnie device\n");
		ret = -EINVAL;
		goto error2;
	}

	/* Register backlight  control */
	vx5d3bInfo->bd = backlight_device_register("panel", vx5d3bInfo->dev,
					vx5d3bInfo, &lt02_backlight_ops, NULL);

	vx5d3bInfo->bd->props.max_brightness = MAX_BRIGHTNESS_LEVEL;
	vx5d3bInfo->bd->props.brightness = DEFAULT_BRIGHTNESS;
	vx5d3bInfo->bd->props.type = BACKLIGHT_RAW;
	vx5d3bInfo->cabc = CABC_OFF;
	vx5d3bInfo->vee_lightValue = 0;
	vx5d3bInfo->vee_strenght = V5D3BX_VEEDEFAULTVAL;
	vx5d3bInfo->prevee_strenght = 1;
	vx5d3bInfo->auto_brightness = false;
	vx5d3bInfo->first_count = false;
	vx5d3bInfo->lcd_panel = panel_id;
	vx5d3bInfo->vee_lightValue = &backlight_table[vx5d3bInfo->lcd_panel];
	vx5d3bInfo->lvds_clk = LVDS_CLK_48P19Mhz;
	vx5d3bInfo->orig_lvds_clk = LVDS_CLK_48P19Mhz;
	vx5d3bInfo->lvds_clk_switching = false;
	vx5d3bInfo->recovery_mode = recovery_mode;
	vx5d3bInfo->vx5b3d_backlight_frq = V5D3BX_5P9KHZ_DEFAULT_RATIO;

	vx5d3bInfo->workqueue = create_singlethread_workqueue("auto_brightness");
        if (vx5d3bInfo->workqueue == NULL) {
                pr_err("can't create auto_brightness workqueue\n");
                ret = -ENOMEM;
		goto error2;
        }
        INIT_DELAYED_WORK(&vx5d3bInfo->auto_brightness_delayed_work, auto_brightness_work);

	if (vx5d3bInfo->recovery_mode)
		fb->mmap = 2;
	else
		fb->mmap = 3;

	if (vx5d3bInfo->lcd_panel == 2 || vx5d3bInfo->lcd_panel == 0) {
		fb->modes->left_margin = 125;/*BOEVE / CPT*/
		fb->modes->hsync_len = 16;
		fb->modes->upper_margin = 28;
		fb->modes->lower_margin = 28;
	}
	else if (vx5d3bInfo->lcd_panel == 4)
	{
		fb->modes->hsync_len = 16;
		fb->modes->left_margin = 110;/*SDCVE*/
		fb->modes->right_margin = 150;
		fb->modes->vsync_len = 4;
		fb->modes->upper_margin = 7;
		fb->modes->lower_margin = 7;	
	}		

	if (device_create_file(&vx5d3bInfo->bd->dev, &dev_attr_auto_brightness) < 0)
		pr_err("Failed to create auto_brightness\n");

	if (device_create_file(&vx5d3bInfo->bd->dev, &dev_attr_vx5b3d_Regread) < 0)
		pr_err("Failed to create vx5b3d_Regread\n");

	/*For lcd class*/
	vx5d3bInfo->lcd = lcd_device_register("panel", NULL, NULL, NULL);
	if (IS_ERR_OR_NULL(vx5d3bInfo->lcd))
	{
		pr_err("Failed to create lcd class!\n");
		ret = -EINVAL;
		goto error2;
	}

	if (device_create_file(&vx5d3bInfo->lcd->dev, &dev_attr_lcd_type) < 0)
		pr_info("Failed to create device file for lcd type!\n");

	if (device_create_file(&vx5d3bInfo->lcd->dev, &dev_attr_veeregwrite) < 0)
		pr_info("Failed to create device file for vee_strenght!\n");

	if (device_create_file(&vx5d3bInfo->lcd->dev, &dev_attr_vee_strenght) < 0)
		pr_info("Failed to create device file for vee_strenght!\n");

	mutex_init(&vx5d3bInfo->lock);
	mutex_init(&vx5d3bInfo->pwr_lock);
	mutex_init(&vx5d3bInfo->lvds_clk_switch_lock);

	g_vx5d3b = vx5d3bInfo;

#endif
	/* align with android format and vres_virtual pitch */
	dither_config(fb);
	/*
	 * Re-calculate lcd clk source and divider
	 * according to dsi lanes and output format.
	 */
	calculate_lcd_sclk(fb);

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

	return ret;

#ifndef CONFIG_BACKLIGHT_TPS61165
error2:
	kfree(vx5d3bInfo);
error1:
	class_destroy(mdnie_class);
	return ret;
#endif
}

void __init emeidkb_add_tv_out(void)
{
	struct pxa168fb_mach_info *fb = &mipi_lcd_info, *ovly =
	&mipi_lcd_ovly_info;

	/* Change id for TV GFX layer to avoid duplicate with panel path */
	strncpy(fb->id, "TV GFX Layer", 13);
	fb->num_modes = ARRAY_SIZE(video_modes_lt02);

	fb->modes = video_modes_lt02;

	fb->max_fb_size = ALIGN(fb->modes->xres, 16) *
	fb->modes->yres * 8 + 4096;

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

	fb->mmap = 0;
	fb->phy_init = NULL;
	fb->phy_type = DSI;
	fb->phy_info = (void *)&lt02_dsiinfo;
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
