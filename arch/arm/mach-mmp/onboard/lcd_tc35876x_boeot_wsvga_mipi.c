#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

//#include <mach/eden.h>
#include <mach/mmp3.h>
#include <mach/pxa988.h>
#include <mach/pxa168fb.h>
#include <mach/regs-mcu.h>
#include <mach/features.h>
#if defined(CONFIG_MACH_LT02)
#include <mach/mfp-pxa986-lt02.h>
#elif defined CONFIG_MACH_HARRISON
#include <mach/mfp-pxa986-harrison.h>
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

	unsigned int			auto_brightness;
	unsigned int			power_lut_idx;
	unsigned int			vee_strenght;
};

static struct Vx5b3d_backlight_value backlight_table[1] = {
	{
		.max = 236,
		.mid = 140,
		.low = 10,
		.dim = 10,
	}
};

struct Vx5b3d_backlight_value *pwm;
struct class *mdnie_class;
struct Vx5d3b_cabc_info *g_vx5d3b = NULL;

#define V5D3BX_VEESTRENGHT		0x00001f07
#define V5D3BX_VEETESTVAL		5

#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS_LEVEL		255
#define MID_BRIGHTNESS_LEVEL		195
#define LOW_BRIGHTNESS_LEVEL		20
#define DIM_BRIGHTNESS_LEVEL		20
#define DEFAULT_BRIGHTNESS		MID_BRIGHTNESS_LEVEL
#endif

struct pxa168fb_info *fbi_global = NULL;
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

int is_fhd_lcd(void)
{
	return 0;
}

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

	if (fbi->skip_pw_on)
		return ret;
	/*if uboot enable display, skip the dsi reset sequence */
	if (!fbi->skip_pw_on) {
		/* reset DSI controller */
		dsi_reset(fbi, 1);
		mdelay(1);

		/* disable continuous clock */
		dsi_cclk_set(fbi, 0);
#ifdef Vx5B3D_MIPI_MERGE
		/* dsi out of reset */
		dsi_reset(fbi, 0);
#else /* For tc35876x i2c*/
#ifdef CONFIG_MACH_LT02
		if (system_rev < LT02_BRINGUP_02) {
			/* dsi out of reset */
			dsi_reset(fbi, 0);
		}
#endif
#endif
	}
#if 0 /* For tc35876x i2c*/
	if (system_rev >= LT02_BRINGUP_02) {
		/*  reset the bridge */
		if (mi->exter_brige_pwr) {
			mi->exter_brige_pwr(fbi,1);
			msleep(10);
		}
		
		/* set dsi to dpi conversion chip */
		if (mi->phy_type == DSI2DPI) {
			ret = mi->exter_brige_init(fbi);
			if (ret < 0)
			pr_err("exter_brige_init error!\n");
		}
		/* dsi out of reset */
		dsi_reset(fbi, 0);
	}
#endif	
	/* turn on DSI continuous clock */
	dsi_cclk_set(fbi, 1);

	/* set dphy */
	dsi_set_dphy(fbi);

#if defined(CONFIG_MACH_LT02)
	if (system_rev >= LT02_BRINGUP_02) {
#endif
		/*  reset the bridge */
		if (mi->exter_brige_pwr && !fbi->skip_pw_on) {
			mi->exter_brige_pwr(fbi,1);
		}
#if defined(CONFIG_MACH_LT02)
	}
#endif

#if defined(CONFIG_MACH_LT02)
	if (system_rev >= LT02_BRINGUP_02) {
#endif
	/* set dsi to dpi conversion chip */
		if (mi->phy_type == DSI2DPI && !fbi->skip_pw_on) {
			ret = mi->exter_brige_init(fbi);
			if (ret < 0)
				pr_err("exter_brige_init error!\n");
		}
#if defined(CONFIG_MACH_LT02)
	}
#endif

	/* put all lanes to LP-11 state  */
	dsi_lanes_enable(fbi, 0);
	dsi_lanes_enable(fbi, 1);

	/*  reset the bridge */
//	if (mi->exter_brige_pwr) {
	
//	}

	/* if panel not enabled, init panel settings via dsi */
	if (mi->phy_type == DSI && !fbi->skip_pw_on)
		mi->dsi_panel_config(fbi);



	/* set dsi controller */
	dsi_set_controller(fbi);

	/*
   	 if no EXTCLK, we need supply DSICLK 1usec before i2c command
    	DSICLK setup time before active I2C transaction    1 ¦Ìsec
    	DSICLK hold time after last active I2C transaction 1 ¦Ìsec
	*/


	/* set dsi to dpi conversion chip */

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
	int i;

	pos += sprintf(pos, "%d, %d, ", Vee_cabc->auto_brightness, Vee_cabc->power_lut_idx);
	/*
	for (i = 0; i < 5; i++)
		pos += sprintf(pos, "0x%02x, ", power_lut[mdnie->power_lut_idx][0][i]);
	pos += sprintf(pos, "\n");
	*/
	return pos - buf;
}

static ssize_t auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (Vee_cabc->auto_brightness != value) {
			dev_info(dev, "%s - %d -> %d\n", __func__, Vee_cabc->auto_brightness, value);
			mutex_lock(&Vee_cabc->lock);
			Vee_cabc->auto_brightness = value;
			/*
#if defined(CONFIG_BACKLIGHT_CABC_WITH_LINEAR_AUTO_BRIGHTNESS)
			mutex_lock(&Vee_cabc->lock);
			Vee_cabc->cabc = (value) ? CABC_ON : CABC_OFF;
			mutex_unlock(&Vee_cabc->lock);
#endif
*/
			if (Vee_cabc->auto_brightness >= 5)
				Vee_cabc->power_lut_idx = LUT_LEVEL_OUTDOOR_2;
			else if (Vee_cabc->auto_brightness == 4)
				Vee_cabc->power_lut_idx = LUT_LEVEL_OUTDOOR_1;
			else
				Vee_cabc->power_lut_idx = LUT_LEVEL_MANUAL_AND_INDOOR;
			mutex_unlock(&Vee_cabc->lock);
			/*
			mdnie_update(Vee_cabc);
			if (Vee_cabc->bd_enable)
				update_brightness(mdnie);
			*/
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

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[20];
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
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (value >= CABC_MAX)
		value = CABC_OFF;

	value = (value) ? CABC_ON : CABC_OFF;

	mutex_lock(&Vee_cabc->lock);
	Vee_cabc->cabc = value;
	mutex_unlock(&Vee_cabc->lock);

	return count;
}

static struct device_attribute mdnie_attributes[] = {
	__ATTR(cabc, 0664, cabc_show, cabc_store),
	__ATTR_NULL,
};

static int lt02_set_brightness(struct backlight_device *bd)
{
	struct Vx5b3d_backlight_value *pwm = g_vx5d3b->vee_lightValue;

	int brightness = bd->props.brightness;
	int max = bd->props.max_brightness;
	
	int vx5b3d_brightness = 0;
	u32 vee_strenght = 0;
	int ret = 0;

	if (bd->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (fbi_global->active == 0)
		return ret;
/*
	register 0x160
	register 0x164
			  value of 0x164
	---> duty ration = -------------
			  value of 0x160
	
*/
	/* brightness tuning*/
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

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is only supported from 0 to 24 */
	 
	/*FOR VX5B3D PWM CONTROL*/
	printk("***************lt02_set_brightness****************liuchunhai*****brightness ==%d  \n",brightness);
	vee_strenght = V5D3BX_VEESTRENGHT | (V5D3BX_VEETESTVAL << 27);
	ret |= tc35876x_write32(0x164,vx5b3d_brightness);
	/* ret |= tc35876x_write32(0x400,vee_strenght);*/

	/* ret |= vx5d3b_i2c_release(); */
	printk("***************lt02_set_brightness****************liuchunhai*****ret ==%d  \n",ret);
	if (ret < 0)
	dev_info(&bd->dev, "tc35876x_i2c_write fail [%d] ! \n",ret);

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
		.upper_margin = 16,  /* upper_margin should >= 6 */
		.lower_margin = 16,  /* lower_margin should >= 6 */
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};

static int lt02_lcd_power(struct pxa168fb_info *fbi,
                            unsigned int spi_gpio_cs,
                            unsigned int spi_gpio_reset, int on)
{
	static struct regulator *lvds1_1p2 = NULL,*lvds1_1p8 = NULL,*lvds1_3p3 = NULL;
	unsigned int  lcd_lvds_rst,lcd_internal_ldo_en;

	if (fbi_global == NULL)
		fbi_global = fbi;

	if (fbi->skip_pw_on)
		return 0;

	lcd_lvds_rst = mfp_to_gpio(GPIO018_GPIO_18);
	lcd_internal_ldo_en = mfp_to_gpio(GPIO097_GPIO_97);
		
	if (gpio_request(lcd_internal_ldo_en, "lcd internal ldo en")) {
		pr_err("gpio %d request failed\n", lcd_internal_ldo_en);
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
	
	if (on) {

		/* VDD ... enable */
		/* MIPI bridge must power on as below sequence or at the same time.
		1.2V, to digital core
		1.2V, to DSI-RX PHY
		3.3V, to LVDS-TX PHY
		1.8V or 3.3V, to digital I/Os
		*/

			/* AVDD ... enable */
			regulator_set_voltage(lvds1_1p8, 1800000, 1800000);
			regulator_enable(lvds1_1p8);		

			regulator_set_voltage(lvds1_3p3, 3300000, 3300000);
			regulator_enable(lvds1_3p3);			

			regulator_set_voltage(lvds1_1p2, 1200000, 1200000);
			regulator_enable(lvds1_1p2);

			msleep(5);	

			gpio_direction_output(lcd_internal_ldo_en, 1);
			pr_info("lt02_lcd_power on !\n");

	} else {
			/* Backlight off*/
			tc35876x_write32(0x164, 0x0);
			msleep(200);
			/*mipi signal off*/
			gpio_direction_output(lcd_internal_ldo_en, 0);			
			regulator_disable(lvds1_3p3);
			regulator_disable(lvds1_1p2);
			regulator_disable(lvds1_1p8);
			gpio_direction_output(lcd_lvds_rst, 1);
			mdelay(1);
			pr_info("lt02_lcd_power off !\n");		
	}

	gpio_free(lcd_internal_ldo_en);
	gpio_free(lcd_lvds_rst);	
	
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
	return -EIO;
}

/* t7: use DSI2DPI bridge, and 4 lanes */
static struct dsi_info lt02_dsiinfo = {
	.id = 1,
	.lanes = 4,
	.bpp = 24,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_888,
	.burst_mode = DSI_BURST_MODE_BURST,//which mode for lvds bridge????
	.hbp_en = 1,
	.hfp_en = 1,
};
static int tc358765_reset(struct pxa168fb_info *fbi, int on)
{
	int gpio;
#if defined(CONFIG_MACH_EMEIDKB) ||defined(CONFIG_MACH_HARRISON) || defined(CONFIG_MACH_LT02)
	
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
	int status;
	static int first_cnt = 1;
	pr_info("VX5B3D ...START.....\n");
	u32 val;
#ifndef Vx5B3D_MIPI_MERGE/*For i2c*/
	tc35876x_write32(0x700, 0x6C900040);
	tc35876x_write32(0x704, 0x302DB);
	tc35876x_write32(0x70C, 0x00004604);
	tc35876x_write32(0x710, 0x54D000B);
	tc35876x_write32(0x714, 0x20);
	tc35876x_write32(0x718, 0x00000102);
	tc35876x_write32(0x71C, 0xA8002F);
	tc35876x_write32(0x720, 0x0);
	tc35876x_write32(0x154, 0x00000000);
	tc35876x_write32(0x154, 0x80000000);
	tc35876x_write32(0x700, 0x6C900840);
	tc35876x_write32(0x70C, 0x5E46/*0x5646*/);
	tc35876x_write32(0x718, 0x00000202);
	tc35876x_write32(0x154, 0x00000000);
	tc35876x_write32(0x154, 0x80000000);
	tc35876x_write32(0x120, 0x5);
	tc35876x_write32(0x124, 0x512C400);
	tc35876x_write32(0x128, 0x104010);
	tc35876x_write32(0x12C, 0x93);
	tc35876x_write32(0x130, 0x3C18);
	tc35876x_write32(0x134, 0x15);
	tc35876x_write32(0x138, 0xFF8000);
	tc35876x_write32(0x13C, 0x0);

	/*PWM  100 % duty ration*/
	
	tc35876x_write32(0x114, 0xc6302);
	/*backlight duty ration control when device is first bring up.*/	
	tc35876x_write32(0x160, 0xff);
	if ( first_cnt == 1)
	{	
		tc35876x_write32(0x164, 0x7f);
		first_cnt = 0;
	}
	tc35876x_write32(0x138, 0x3fff0000);
	tc35876x_write32(0x15c, 0x5);	
	/* END...*/
	
	tc35876x_write32(0x140, 0x10000);
	tc35876x_write32(0x20C, 0x134);
	tc35876x_write32(0x21C, 0x0);
	tc35876x_write32(0x224, 0x0);
	tc35876x_write32(0x228, 0x50001);
	tc35876x_write32(0x22C, 0xFF03);
	tc35876x_write32(0x230, 0x1);
	tc35876x_write32(0x234, 0xCA033E10);
	tc35876x_write32(0x238, 0x00000060);
	tc35876x_write32(0x23C, 0x82E86030);
	tc35876x_write32(0x244, 0x001E0285);
	tc35876x_write32(0x258, 0x30014);
	tc35876x_write32(0x158, 0x0);
	tc35876x_write32(0x158, 0x1);
	tc35876x_write32(0x37C, 0x00001063);
	tc35876x_write32(0x380, 0x82A86030);
	tc35876x_write32(0x384, 0x2861408B);
	tc35876x_write32(0x388, 0x00130285);
	tc35876x_write32(0x38C, 0x10630009);
	tc35876x_write32(0x394, 0x400B82A8);
	tc35876x_write32(0x600, 0x16CC78C);
	tc35876x_write32(0x604, 0x3FFFFFE0);
	tc35876x_write32(0x608, 0xD8C);
	tc35876x_write32(0x154, 0x00000000);
	tc35876x_write32(0x154, 0x80000000);
#else
	set_dsi_low_power_mode(fbi);
	msleep(20);
	
	vx5d3b_mipi_write(fbi,0x700, 0x6C900040,4);
	vx5d3b_mipi_write(fbi,0x704, 0x302DB,4);
	vx5d3b_mipi_write(fbi,0x70C, 0x00004604,4);
	vx5d3b_mipi_write(fbi,0x710, 0x54D004B/*0x54D000B*/,4);
	vx5d3b_mipi_write(fbi,0x714, 0x20,4);
	vx5d3b_mipi_write(fbi,0x718, 0x00000102,4);
	vx5d3b_mipi_write(fbi,0x71C, 0xA8002F,4);
	vx5d3b_mipi_write(fbi,0x720, 0x0,4);
	
	vx5d3b_mipi_write(fbi,0x154, 0x00000000,4);
	vx5d3b_mipi_write(fbi,0x154, 0x80000000,4);
	udelay(200); // For pll locking
	vx5d3b_mipi_write(fbi,0x700, 0x6C900840,4);
	vx5d3b_mipi_write(fbi,0x70C, 0x5E56/*0x5E46*//*0x5646*/,4);
	vx5d3b_mipi_write(fbi,0x718, 0x00000202,4);
	
	
	vx5d3b_mipi_write(fbi,0x154, 0x00000000,4);	
	vx5d3b_mipi_write(fbi,0x154, 0x80000000,4);
	udelay(200); // For pll locking
	vx5d3b_mipi_write(fbi,0x37C, 0x00001063,4);
	vx5d3b_mipi_write(fbi,0x380, 0x82A86030,4);
	vx5d3b_mipi_write(fbi,0x384, 0x2861408B,4);
	vx5d3b_mipi_write(fbi,0x388, 0x00130285,4);
	vx5d3b_mipi_write(fbi,0x38C, 0x10630009,4);
	vx5d3b_mipi_write(fbi,0x394, 0x400B82A8,4);
	vx5d3b_mipi_write(fbi,0x600, 0x16CC78C,4);
	vx5d3b_mipi_write(fbi,0x604, 0x3FFFFFE0,4);
	vx5d3b_mipi_write(fbi,0x608, 0xD8C,4);

	vx5d3b_mipi_write(fbi,0x154, 0x00000000,4);
	vx5d3b_mipi_write(fbi,0x154, 0x80000000,4);

	/* ...move for system reset command (0x158)*/

	vx5d3b_mipi_write(fbi,0x120, 0x5,4);
	vx5d3b_mipi_write(fbi,0x124, 0x512C400,4);
	vx5d3b_mipi_write(fbi,0x128, 0x104010,4);
	vx5d3b_mipi_write(fbi,0x12C, 0x93,4);
	vx5d3b_mipi_write(fbi,0x130, 0x3C18,4);
	vx5d3b_mipi_write(fbi,0x134, 0x15,4);
	vx5d3b_mipi_write(fbi,0x138, 0xFF8000,4);
	vx5d3b_mipi_write(fbi,0x13C, 0x0,4);


	/*PWM  100 % duty ration*/

	vx5d3b_mipi_write(fbi,0x114, 0xc6302,4);
	/*backlight duty ration control when device is first bring up.*/
	vx5d3b_mipi_write(fbi,0x160, 0xff,4);
	if ( first_cnt == 1)
	{
		vx5d3b_mipi_write(fbi,0x164, 0x4c,4);
		first_cnt = 0;
	}

	vx5d3b_mipi_write(fbi,0x138, 0x3fff0000,4);
	vx5d3b_mipi_write(fbi,0x15c, 0x5,4);
	/* END...*/

	vx5d3b_mipi_write(fbi,0x140, 0x10000,4);
	/*Add for power consumtion*/
	vx5d3b_mipi_write(fbi,0x174, 0xff,4);
	/*end*/
	
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
	vx5d3b_mipi_write(fbi,0x258, 0x30014,4);


	vx5d3b_mipi_write(fbi,0x158, 0x0,4);
	vx5d3b_mipi_write(fbi,0x158, 0x1,4);
	udelay(200); // For pll locking

	/* Disable brightnes issue Caused by IBC*/
	/* read 4 bytes from address 0x410 to 0x413*/
	vx5d3b_i2c_read(0x410, &val, 4);
	/*set bit 4 of byte @ address 0x413 =0; Note: the function is 32 bit r/w */
	val &= 0xefffffff;
	tc35876x_write32(0x410,val);

	/*...end */

	/* put all lanes to LP-11 state  */
	dsi_lanes_enable(fbi, 1);
#endif

/* For bringup emul board of LT02 */
#if 0
#ifdef CONFIG_TC35876X
	struct fb_var_screeninfo *var = &(fbi->fb_info->var);
	struct dsi_info *di = &lt02_dsiinfo;
	u16 chip_id = 0;
	u8 vesa_rgb888 = 1;

	status = tc35876x_read16(TC358765_CHIPID_REG, &chip_id);
	
	if ((status < 0) || (chip_id != TC358765_CHIPID)) {
		pr_err("tc35876x unavailable! chip_id %x\n", chip_id);
		return -EIO;
	} else
		pr_debug("tc35876x(chip id:0x%02x) detected.\n", chip_id);
#if 1
	if (vesa_rgb888) {
	/* VESA format instead of JEIDA format for RGB888 */
	tc35876x_write32( LVMX0003, 0x03020100);
	tc35876x_write32( LVMX0407, 0x08050704);
	tc35876x_write32( LVMX0811, 0x0F0E0A09);
	tc35876x_write32( LVMX1215, 0x100D0C0B);
	tc35876x_write32( LVMX1619, 0x12111716);
	tc35876x_write32( LVMX2023, 0x1B151413);
	tc35876x_write32( LVMX2427, 0x061A1918);
	}	
#endif
	/* REG 0x13C,DAT 0x000C000F */
	tc35876x_write32(PPI_TX_RX_TA, 0x00040004);
	/* REG 0x114,DAT 0x0000000A */
	tc35876x_write32(PPI_LPTXTIMECNT, 0x00000004);

	/* get middle value of mim-max value
	 * 0-0x13 for 2lanes-rgb888, 0-0x26 for 4lanes-rgb888
	 * 0-0x21 for 2lanes-rgb565, 0-0x25 for 4lanes-rgb565
	 */
	if (di->lanes == 4)
		status = 0x13;
	else if (di->bpp == 24)
		status = 0xa;
	else
		status = 0x11;
	/* REG 0x164,DAT 0x00000005 */
	tc35876x_write32(PPI_D0S_CLRSIPOCOUNT, status);//asserting time for lp->hs
	/* REG 0x168,DAT 0x00000005 */
	tc35876x_write32(PPI_D1S_CLRSIPOCOUNT, status);
	if (di->lanes == 4) {
		/* REG 0x16C,DAT 0x00000005 */
		tc35876x_write32(PPI_D2S_CLRSIPOCOUNT, status);
		/* REG 0x170,DAT 0x00000005 */
		tc35876x_write32(PPI_D3S_CLRSIPOCOUNT, status);
	}

	/* REG 0x134,DAT 0x00000007 */
	tc35876x_write32(PPI_LANEENABLE, (di->lanes == 4) ? 0x1f : 0x7);
	/* REG 0x210,DAT 0x00000007 */
	tc35876x_write32(DSI_LANEENABLE, (di->lanes == 4) ? 0x1f : 0x7);

	/* REG 0x104,DAT 0x00000001 */
	tc35876x_write32(PPI_STARTPPI, 0x0000001);
	/* REG 0x204,DAT 0x00000001 */
	tc35876x_write32(DSI_STARTDSI, 0x0000001);

	/*
	 * REG 0x450,DAT 0x00012020, VSDELAY = 8 pixels,
	 * enable magic square if in_bpp == 24, out_bpp == 18
	 */
	/* tc35876x_write32(VPCTRL, 0x00800020 | (di->bpp == 24 ? 1 : 0)); */
	tc35876x_write32(VPCTRL, 0x00800120);
#if 1
	/* REG 0x454,DAT 0x00200008*/
	tc35876x_write32(HTIM1, ((var->left_margin) << 16)
			| var->hsync_len);

	/* REG 0x45C,DAT 0x00040004*/
	tc35876x_write32(VTIM1, ((var->upper_margin) << 16)
			| var->vsync_len);
#else
	tc35876x_write32(HTIM1, 0x00200002);
	tc35876x_write32(HTIM1, 0x00200500);

	/* REG 0x45C,DAT 0x00040004*/
	tc35876x_write32(VTIM1,0x00180002);
	tc35876x_write32(VTIM1,0x00180320);
#endif


	/* After change the video timing parameters (HTIM1,2, VTIM1,2),
	 * VFUEN has to be set. */
	tc35876x_write32(VFUEN, 0x00000001);
	/* After power on and hardware reset, LVPHY needs LV_RST and RSTLCD. */
	tc35876x_write32(LVPHY0, 0x00448006);
	mdelay(1);
	tc35876x_write32(LVPHY0, 0x00048006);
	tc35876x_write32(SYSRST, 0x00000004);///if needed???

	/* Set unused gpio as output/low */
	tc35876x_write32(GPIOC, 0x0000001F);
	tc35876x_write32(GPIOO, 0x00000000);

	/*
	 * no EXTCLK: After reset, If EXTCLK toggles then EXTCLK is selected as
	 * pixel clock source, else DSICLK is selected(LVCFG register describes
	 * the DSICLK divide options).
	 * PCLKDIV = 1(div=4)
	 * Now, the DSICLK_OUT = 208M, so TC3_PCLK = 52M
	 */

	/* REG 0x49C,DAT 0x00000201 */
	tc35876x_write32(LVCFG, 0x00000001);//sigle link???

	/* dump register value */
	tc358765_dump();
	
#endif
}
#endif	/* For bringup emul board of LT02 */

	pr_info("VX5B3D ..END.....\n");
	return 0;
}

void __init lt02_add_lcd_mipi(void)
{
	struct dsi_info *dsi = NULL;
	struct pxa168fb_mach_info *fb = &mipi_lcd_info, *ovly =
	    &mipi_lcd_ovly_info;

	unsigned int CSn_NO_COL;
	struct Vx5d3b_cabc_info *vx5d3bInfo = NULL;

	fb->num_modes = ARRAY_SIZE(video_modes_lt02);
	fb->modes = video_modes_lt02;
	fb->max_fb_size = ALIGN(fb->modes->xres, 16) *
		fb->modes->yres * 8 + 4096;

	/* align with android format and vres_virtual pitch */
	fb->pix_fmt = PIX_FMT_RGBA888;
	fb->xres_virtual = ALIGN(fb->modes->xres, 16);

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

#ifdef CONFIG_TC35876X
	fb->phy_type = DSI2DPI;
	fb->exter_brige_pwr = tc358765_reset;
	fb->exter_brige_init = dsi_set_tc358765;
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

	if (!vx5d3bInfo)
		pr_err("failed to allocate vx5d3bInfo\n");

	vx5d3bInfo->dev = device_create(mdnie_class, NULL, 0, &vx5d3bInfo, "mdnie");
	if (IS_ERR_OR_NULL(vx5d3bInfo->dev))
		pr_err("failed to create mdnie device\n");

	/* Register backlight  control */
	vx5d3bInfo->bd = backlight_device_register("panel", vx5d3bInfo->dev,
					vx5d3bInfo, &lt02_backlight_ops, NULL);

	vx5d3bInfo->bd->props.max_brightness = MAX_BRIGHTNESS_LEVEL;
	vx5d3bInfo->bd->props.brightness = DEFAULT_BRIGHTNESS;
	vx5d3bInfo->bd->props.type = BACKLIGHT_RAW;
	vx5d3bInfo->cabc = CABC_OFF;
	vx5d3bInfo->vee_lightValue = 0;
	vx5d3bInfo->vee_strenght = 0;
	vx5d3bInfo->auto_brightness = false;
	vx5d3bInfo->vee_lightValue = &backlight_table[0];

	if (device_create_file(&vx5d3bInfo->bd->dev, &dev_attr_auto_brightness) < 0)
		dev_err(&vx5d3bInfo->bd->dev, "failed to add sysfs entries, %d\n");
	
		
	/*For lcd class*/
	vx5d3bInfo->lcd = lcd_device_register("panel", NULL, NULL, NULL);
	if (IS_ERR_OR_NULL(vx5d3bInfo->lcd)) 
	{
		pr_err("Failed to create lcd class!\n");
	}
	
	if (device_create_file(&vx5d3bInfo->lcd->dev, &dev_attr_lcd_type) < 0)
		pr_info("Failed to create device file for lcd type!\n");

	if (device_create_file(&vx5d3bInfo->lcd->dev, &dev_attr_vee_strenght) < 0)
		pr_info("Failed to create device file for vee_strenght!\n");

	mutex_init(&vx5d3bInfo->lock);

	g_vx5d3b = vx5d3bInfo;

#endif
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

	if (!has_feat_video_replace_graphics_dma())
		pxa988_add_fb_ovly(ovly);
	
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
//#endif



