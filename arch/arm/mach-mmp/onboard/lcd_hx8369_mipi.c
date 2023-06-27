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
#include <linux/timer.h>
#if defined(CONFIG_CPU_PXA1088)
#include <mach/mfp-pxa1088-delos.h>
#elif defined(CONFIG_CPU_PXA988)
#include <mach/mfp-pxa988-aruba.h>
#endif
#include <mach/features.h>
#include "../common.h"
#include <linux/power_supply.h>
#include "lcd_hx8369_param.h"
#ifdef  CONFIG_MACH_CS02 || CONFIG_MACH_GOLDEN || CONFIG_MACH_GOYA
#include "lcd_hx8369b_cs02.h"
#elif defined(CONFIG_MACH_BAFFIN)
#include "lcd_hx8369_baffin.h"
#elif defined(CONFIG_MACH_BAFFINQ)
#include "lcd_hx8369_baffin.h"
#endif

extern unsigned int system_rev;
struct pxa168fb_info *fbi_global = NULL;

#ifdef CONFIG_LDI_SUPPORT_MDNIE
#include "mdnie_lite.h"
struct class *lcd_mDNIe_class;
static int isReadyTo_mDNIe = 1;
#ifdef ENABLE_MDNIE_TUNING
#define TUNING_FILE_PATH "/sdcard/mdnie/"
#define MAX_TUNING_FILE_NAME 100
#define NR_MDNIE_DATA 113
static int tuning_enable;
static char tuning_filename[MAX_TUNING_FILE_NAME];
unsigned char mDNIe_data[NR_MDNIE_DATA] = {HX8369B_REG_MDNIE, 0x00};
static struct dsi_cmd_desc hx8369b_video_display_mDNIe_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(mDNIe_data),mDNIe_data},
};
static struct mdnie_config mDNIe_cfg;
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

static int dsi_init(struct pxa168fb_info *fbi);
static int get_panel_id(struct pxa168fb_info *fbi);
#define LCD_ESD_RECOVERY
#if defined(LCD_ESD_RECOVERY)
#define LCD_ESD_INTERVAL	(5000)
//#define ESD_PERIODIC_TEST
struct delayed_work d_work; // ESD self protect
static void d_work_func(struct work_struct *work);
static void lcd_esd_detect(void);
#endif

static bool lcd_enable = true;
int lcd_esd_irq = 0;
struct workqueue_struct *lcd_wq;
struct work_struct lcd_work;
u32 panel_id =  0;
/*
 * dsi bpp : rgb_mode
 *    16   : DSI_LCD_INPUT_DATA_RGB_MODE_565;
 *    24   : DSI_LCD_INPUT_DATA_RGB_MODE_888;
 */
static struct dsi_info dsiinfo = {
	.id = 1,
	.lanes = 2,
	.bpp = 24,
	.rgb_mode = DSI_LCD_INPUT_DATA_RGB_MODE_888,
	.burst_mode = DSI_BURST_MODE_BURST,
	.hbp_en = 1,
	.hfp_en = 1,
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
#if defined(CONFIG_MACH_ARUBA_TD) || defined(CONFIG_MACH_CS02) || defined(CONFIG_MACH_GOLDEN) || defined(CONFIG_MACH_GOYA)
	.width = 52,
	.height = 86,
#else
	.width = 61,
	.height = 102,
#endif
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

#define GPIO18_LCD_RESET (18)

static int lcd_reset(struct pxa168fb_info *fbi)
{

	if (gpio_request(GPIO18_LCD_RESET, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", GPIO18_LCD_RESET);
		return -EIO;
	}

	gpio_direction_output(GPIO18_LCD_RESET, 0);
	mdelay(1);
	gpio_direction_output(GPIO18_LCD_RESET, 1);
	msleep(120);

	gpio_free(GPIO18_LCD_RESET);

	return 0;
}

static int dsi_reinit(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;

	if (!fbi_global)
		fbi_global = fbi;

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

	/* set dsi controller */
	dsi_set_controller(fbi);

	/* panel init */
	mi->dsi_panel_config(fbi);
#endif
	return 0;
}

static int dsi_init(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;

	if (!fbi_global)
		fbi_global = fbi;


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
	mdelay(1);

	if (!fbi->skip_pw_on)
		lcd_reset(fbi);

	/*  reset the bridge */
	if (mi->exter_brige_pwr) {
		mi->exter_brige_pwr(fbi,1);
		msleep(10);
	}

	if (!panel_id)
		panel_id = get_panel_id(fbi);

	if (mi->phy_type == DSI && !fbi->skip_pw_on)
		mi->dsi_panel_config(fbi);

	/* set dsi controller */
	dsi_set_controller(fbi);

	/* set dsi to dpi conversion chip */
	if (mi->phy_type == DSI2DPI) {
		int ret = 0;
		ret = mi->exter_brige_init(fbi);
		if (ret < 0)
			pr_err("exter_brige_init error!\n");
	}
#endif
	return 0;
}

static int __init get_lcd_id(char *str)
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
__setup("lcd_id=", get_lcd_id);

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

#if defined CONFIG_MACH_CS02 || defined CONFIG_MACH_GOLDEN || defined(CONFIG_MACH_GOYA)
static struct fb_videomode video_modes_emeidkb[] = {
	[0] = {
		/* panel refresh rate should <= 55(Hz) */
		.refresh = 60,
		.xres = 480,
		.yres = 800,
		.hsync_len = 32,	//HWD
		.left_margin = 85,	//HBP
		.right_margin = 85,	//HFP
		.vsync_len = 8,		//VWD
		.upper_margin = 14,	//VBP
		.lower_margin = 18,	//VFP
		.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};
#elif defined CONFIG_MACH_BAFFIN
static struct fb_videomode video_modes_emeidkb[] = {
	[0] = {
		/* panel refresh rate should <= 55(Hz) */
		.refresh = 60,
		.xres = 480,
		.yres = 800,
		.hsync_len = 60,//50,	//HWD
		.left_margin = 96,//60,	//HBP
		.right_margin = 106,//100,	//HFP
		.vsync_len = 4,//6,		//VWD
		.upper_margin = 12,//10,	//VBP
		.lower_margin = 8,//10,	//VFP
		.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};

#else
static struct fb_videomode video_modes_emeidkb[] = {
	[0] = {
		/* panel refresh rate should <= 55(Hz) */
		.refresh = 60,
		.xres = 480,
		.yres = 800,
		.hsync_len = 50,	//HWD
		.left_margin = 60,	//HBP
		.right_margin = 100,	//HFP
		.vsync_len = 6,		//VWD
		.upper_margin = 10,	//VBP
		.lower_margin = 10,	//VFP
		.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
};
#endif

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

#ifdef CONFIG_LDI_SUPPORT_MDNIE
#ifdef ENABLE_MDNIE_TUNING
static int parse_text(char *src, int len)
{
	int i;
	int data=0, value=0, count=1, comment=0;
	char *cur_position;

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
	int ret = 0;
	ret = sprintf(buf,"Tunned File Name : %s\n",tuning_filename);

	return ret;
}

ssize_t mDNIeTuning_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *pt;
	char a;
	unsigned long tunning_mode=0;

	a = *buf;

	if(a=='1') {
		tuning_enable = 1;
		printk("%s:Tuning_enable\n",__func__);
	} else if(a=='0') {
		tuning_enable = 0;
		printk("%s:Tuning_disable\n",__func__);
	} else {
		memset(tuning_filename,0,sizeof(tuning_filename));
		sprintf(tuning_filename,"%s%s",TUNING_FILE_PATH,buf);

		pt = tuning_filename;
		while(*pt) {
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

		if (tuning_enable && mDNIe_data[0]) {
			printk("========================mDNIe!!!!!!!\n");
			pxa168_dsi_cmd_array_tx(fbi_global,
					hx8369b_video_display_mDNIe_cmds,
					ARRAY_SIZE(hx8369b_video_display_mDNIe_cmds));
		}
	}
	return size;
}
#endif	/* ENABLE_MDNIE_TUNING */

static void set_mDNIe_Mode(struct mdnie_config *mDNIeCfg)
{
	int value;

	if (!isReadyTo_mDNIe) {
		printk("%s, warnning : is not ready to mDNIE\n", __func__);
		return;
	}

	if (!mDNIeCfg) {
		printk("%s:[mDNIe] mDNIeCfg is null\n", __func__);
		return;
	}

	if (mDNIeCfg->scenario < 0 ||
			mDNIeCfg->scenario >= MDNIE_SCENARIO_MAX) {
		printk("%s:[mDNIe] out of range(scenario : %d)\n",
				__func__, mDNIeCfg->scenario);
		return;
	}

	msleep(100);
	if (mDNIe_cfg.negative) {
		printk("%s : apply negative color\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi_global,
				hx8369b_video_display_mDNIe_negative_cmds,
				ARRAY_SIZE(hx8369b_video_display_mDNIe_negative_cmds));
		return;
	}

	switch (mDNIeCfg->scenario) {
	case MDNIE_UI_MODE:
	case MDNIE_CAMERA_MODE:
	case MDNIE_GALLERY_MODE:
	case MDNIE_VIDEO_MODE:
		value = mDNIeCfg->scenario;
		break;
	case MDNIE_VIDEO_WARM_MODE:
	case MDNIE_VIDEO_COLD_MODE:
		value = MDNIE_VIDEO_MODE;
		break;
	default:
		value = MDNIE_UI_MODE;
		break;
	}

	printk("%s:[mDNIe] value=%d\n", __func__, value);
	pxa168_dsi_cmd_array_tx(fbi_global,
			&hx8369b_video_display_mDNIe_scenario_cmds[value], 1);
	return;
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
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);
	printk("%s:value=%d\n", __func__, value);

	mDNIe_cfg.scenario = value;
	set_mDNIe_Mode(&mDNIe_cfg);
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

	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

static int emeidkb_lcd_power(struct pxa168fb_info *fbi,
		unsigned int spi_gpio_cs,
		unsigned int spi_gpio_reset, int on)
{
	static struct regulator *lcd_iovdd, *lcd_avdd;

	printk("%s, power %s +\n", __func__, on ? "on" : "off");
	if (!fbi_global) {
		fbi_global = fbi;
#if defined(LCD_ESD_RECOVERY)
		INIT_DELAYED_WORK(&d_work, d_work_func); // ESD self protect    
		if (panel_id) {
			printk(KERN_INFO "[LCD] schedule_delayed_work after 20 sec\n");
			schedule_delayed_work(&d_work, msecs_to_jiffies(20000));
		}
#endif
	}

	if (!lcd_avdd) {
		lcd_avdd = regulator_get(NULL, "v_lcd_3V");
		if (IS_ERR(lcd_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_avdd;
		}
	}

	if (on) {
#if defined(CONFIG_MACH_BAFFIN) || defined(CONFIG_MACH_BAFFINQ)
		regulator_set_voltage(lcd_avdd, 3300000, 3300000);
#else
		regulator_set_voltage(lcd_avdd, 3000000, 3000000);
#endif
		regulator_enable(lcd_avdd);
		mdelay(5);
	} else {
		lcd_enable = false;
#ifdef CONFIG_LDI_SUPPORT_MDNIE
		isReadyTo_mDNIe = 0;
#endif
		/*Display off / Sleep In*/
		pxa168_dsi_cmd_array_tx(fbi,
				hx8369b_video_display_off_cmds,
				ARRAY_SIZE(hx8369b_video_display_off_cmds));
		msleep(HX8369B_DISP_OFF_DELAY);
		
		pxa168_dsi_cmd_array_tx(fbi,
				hx8369b_video_sleep_in_cmds,
				ARRAY_SIZE(hx8369b_video_sleep_in_cmds));
		msleep(HX8369B_SLEEP_IN_DELAY);

		/* set lcd reset pin down */
		if (gpio_request(GPIO18_LCD_RESET, "lcd reset gpio")) {
			pr_err("gpio %d request failed\n", GPIO18_LCD_RESET);
			return -EIO;
		}
		gpio_direction_output(GPIO18_LCD_RESET, 0);
#if defined(CONFIG_MACH_CS02) || defined(CONFIG_MACH_BAFFIN) ||  defined(CONFIG_MACH_BAFFINQ)
		msleep(40);
		gpio_direction_output(GPIO18_LCD_RESET, 1);
#endif
		gpio_free(GPIO18_LCD_RESET);

		/* disable LCD_AVDD 3.1v */
		regulator_disable(lcd_avdd);
	}
	printk("%s, power %s -\n", __func__, on ? "on" : "off");
	return 0;

regu_lcd_avdd:
	lcd_avdd = NULL;
	regulator_put(lcd_iovdd);

	return -EIO;
}

#if defined(LCD_ESD_RECOVERY)
int pxa168fb_reinit(struct pxa168fb_info *fbi);
static void d_work_func(struct work_struct *work)
{
	struct pxa168fb_info *fbi = fbi_global;
	int i;
#ifdef ESD_PERIODIC_TEST
	static int count = 0;
#endif
	if (!fbi) {
		printk(KERN_INFO "[LCD] ignore esd : fbi is null!!\n");
		return;
	}

	if (!panel_id) {
		printk(KERN_INFO "[LCD] ignore esd : lcd is not connected!!\n");
		return;
	}

	mutex_lock(&fbi->output_lock);
	if (!lcd_enable) {
		printk(KERN_INFO "[LCD] ignore esd : lcd is not power on!!\n");
		goto out;
	}

	if (fbi->active) {
		struct dsi_buf dbuf;

		for (i = 0; i < 3; i++) {
			pxa168_dsi_cmd_array_rx(fbi_global, &dbuf,
					hx8369b_video_read_esd_cmds,
					ARRAY_SIZE(hx8369b_video_read_esd_cmds));
#ifdef ESD_PERIODIC_TEST
			if (count > 2) {
				dbuf.data[0] = 0;
			}
#endif
			if (dbuf.data[0] == 0x9C) {
				goto add_schedule;
			}
			printk("[LCD] esd detected [0Ah:0x%x](retry : %d)\n", dbuf.data[0], i);
			msleep(500);
		}

		if (dbuf.data[0] != 0x9C) {
			printk("[LCD] try to recover lcd +\n");
			pxa168fb_reinit(fbi);
			printk("[LCD] try to recover lcd -\n");
			goto out;
		}
	}
add_schedule:
	schedule_delayed_work(&d_work, msecs_to_jiffies(LCD_ESD_INTERVAL));
out:
	mutex_unlock(&fbi->output_lock);
#ifdef ESD_PERIODIC_TEST
	if (count++ > 2) {
		count = 0;
	}
#endif
	return;
}

static void lcd_work_func(struct work_struct *work)
{
	printk("lcd_work_func : lcd_enable(%d)\n", lcd_enable);
	if (lcd_enable) {
		printk("lcd_work_func : reset!!!!!!!! starts\n");
#ifdef  CONFIG_LDI_SUPPORT_MDNIE
		isReadyTo_mDNIe = 0;
#endif
		dsi_init(fbi_global);
#ifdef  CONFIG_LDI_SUPPORT_MDNIE
		isReadyTo_mDNIe = 1;
#endif
		printk("lcd_work_func : reset!!!!!!!! ends\n");
	}
	enable_irq(lcd_esd_irq);
}

static irqreturn_t lcd_irq_handler(int irq, void *dev_id)
{
	printk("lcd_irq_handler : irq(%d), lcd_irq(%d) : lcd_enable(%d)\n", irq, lcd_esd_irq, lcd_enable);
	if (lcd_enable) {
		disable_irq_nosync(lcd_esd_irq);
		queue_work(lcd_wq, &lcd_work);
	}
	return IRQ_HANDLED;
}

static void lcd_esd_detect(void)
{
	int lcd_esd_n = 19;
	int err;

	lcd_esd_irq = gpio_to_irq(mfp_to_gpio(GPIO019_GPIO_19));

	if (gpio_request(lcd_esd_n, "LCD_ESD_INT GPIO")) {
		printk(KERN_ERR "Failed to request GPIO %d "
				"for LCD_ESD_INT\n", lcd_esd_n);
		return;
	}
	gpio_direction_input(lcd_esd_n);
	gpio_free(lcd_esd_n);

	err = request_irq(lcd_esd_irq, lcd_irq_handler, IRQF_DISABLED|IRQ_TYPE_EDGE_RISING, "LCD_ESD_INT", NULL);
	if (err)
	{
		printk("[LCD] request_irq failed for taos\n");
	}

	lcd_wq = create_singlethread_workqueue("lcd_wq");
	if(!lcd_wq)
		printk("[LCD] fail to create lcd_wq\n");
	INIT_WORK(&lcd_work, lcd_work_func);
}
#endif

static int get_panel_id(struct pxa168fb_info *fbi)
{
	u32 read_id=0;

#ifdef CONFIG_PXA688_PHY
	struct dsi_buf *dbuf;

	dbuf = kmalloc(sizeof(struct dsi_buf), GFP_KERNEL);
	if (dbuf) {
		pxa168_dsi_cmd_array_rx(fbi, dbuf, hx8369b_video_read_id1_cmds,
				ARRAY_SIZE(hx8369b_video_read_id1_cmds));
		read_id |= dbuf->data[0] << 16;
		pxa168_dsi_cmd_array_rx(fbi, dbuf, hx8369b_video_read_id2_cmds,
				ARRAY_SIZE(hx8369b_video_read_id2_cmds));
		read_id |= dbuf->data[0] << 8;
		pxa168_dsi_cmd_array_rx(fbi, dbuf, hx8369b_video_read_id3_cmds,
				ARRAY_SIZE(hx8369b_video_read_id3_cmds));
		read_id |= dbuf->data[0];
		kfree(dbuf);
		pr_info("Panel id is 0x%x\n", read_id);
	} else
		pr_err("%s: can't alloc dsi rx buffer\n", __func__);
	printk("[LCD] %s, read_id(%x)\n", __func__, read_id);
	return read_id;
#endif
}

#ifdef CONFIG_MACH_CS02
static void panel_init_config_cs02(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
#ifdef CONFIG_LCD_TEMPERATURE_COMPENSATION
	struct power_supply *psy;
	union power_supply_propval val;

	psy = power_supply_get_by_name("battery");
	if (psy != NULL && psy->get_property != NULL) {
		int ret;

		ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
		printk("[LCD] id = 0x%06x temp(%d), D5(%x)+++\n", panel_id, val.intval, video3_1[20]);
		if(ret == 0) {
			if(panel_id == HX8369B_PANEL1 || panel_id == HX8369B_PANEL2) {
				/* Temperature Compensation */
				if(val.intval >= 0) {
					printk("[0x%06x]High Temperature Compensation\n", panel_id);
					hx8369b_554990_auo3[20] = 0x40;
				}
				else{
					printk("[0x%06x]Low Temperature Compensation\n", panel_id);
					hx8369b_554990_auo3[20] = 0x4C;
				}
				printk("[LCD] temp(%d), D5(%x)---\n", val.intval, video3_1[20]);
			} else {
				/* Temperature Compensation */
				if(val.intval >= 0) {
					printk("[0x%06x]High Temperature Compensation\n", panel_id);
				} else {
					printk("[0x%06x]Low Temperature Compensation\n", panel_id);
				}
			}
		} else
			printk("[LCD] read temperature error\n");
	}
#endif
	switch (panel_id) {
		case HX8369B_PANEL1:
			break;
		case HX8369B_PANEL2: /* 0x554990 (AUO panel) */
			pxa168_dsi_cmd_array_tx(fbi,
					hx8369b_video_display_auo_554990_init_cmds,
					ARRAY_SIZE(hx8369b_video_display_auo_554990_init_cmds));
			break;
		case HX8369B_PANEL_BOE1:
		case HX8369B_PANEL_BOE2:
		case HX8369B_PANEL_BOE3: /* 0x55BCF0 (BOE panel) */
			pxa168_dsi_cmd_array_tx(fbi,
					hx8369b_video_display_boe_55bcf0_init_cmds,
					ARRAY_SIZE(hx8369b_video_display_boe_55bcf0_init_cmds));
			break;
		default: /* Temporary dafault set 0x554990 (AUO panel) */ 
			pxa168_dsi_cmd_array_tx(fbi,
					hx8369b_video_display_auo_554990_init_cmds,
					ARRAY_SIZE(hx8369b_video_display_auo_554990_init_cmds));
			break;
	}
#ifdef  CONFIG_LDI_SUPPORT_MDNIE
	isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg);
#ifdef ENABLE_MDNIE_TUNING    
	if(tuning_enable && mDNIe_data[0] !=  0) {
		printk("-----------[%s]-----mDNIe-------------\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi,
				hx8369b_video_display_mDNIe_cmds,
				ARRAY_SIZE(hx8369b_video_display_mDNIe_cmds));
	}
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */
	lcd_enable = true;
#endif
/*
	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_display_on_cmds,
			ARRAY_SIZE(hx8369b_video_display_on_cmds));
*/
	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_sleep_out_cmds,
			ARRAY_SIZE(hx8369b_video_sleep_out_cmds));
	msleep(HX8369B_SLEEP_OUT_DELAY);

	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_displayON_cmds,
			ARRAY_SIZE(hx8369b_video_displayON_cmds));
	msleep(HX8369B_DISP_ON_DELAY);

}
#elif defined(CONFIG_MACH_BAFFIN) ||  defined(CONFIG_MACH_BAFFINQ)
static void panel_init_config_baffin(struct pxa168fb_info *fbi)
{

#ifdef CONFIG_PXA688_PHY
#if 1//CONFIG_LCD_TEMPERATURE_COMPENSATION
		struct power_supply *psy;
		union power_supply_propval val;
	
		psy = power_supply_get_by_name("battery");
		if (psy != NULL && psy->get_property != NULL) {
			int ret;

			ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
			printk("[LCD] id = 0x%06x temp(%d), D5(%x)+++\n", panel_id, val.intval, video3_1[20]);
			if(ret == 0)
			{
				if(panel_id == HX8369B_PANEL_DTC1 || panel_id == HX8369B_PANEL_DTC2)
				{
					/* Temperature Compensation */
					if(val.intval > -50) {
						printk("[0x%06x]High Temperature Compensation\n", panel_id);
						hx8369b_dtc_B4h[1] = 0x02;
					}
					else{
						printk("[0x%06x]Low Temperature Compensation\n", panel_id);
						hx8369b_dtc_B4h[1] = 0x01;
					}
					printk("[LCD] temp(%d), D5(%x)---\n", val.intval, video3_1[20]);
				} 
				else if(panel_id == HX8369B_PANEL_CPT)
				{	
				    /* Temperature Compensation */
					if(val.intval > -50) {
						printk("[0x%06x]High Temperature Compensation\n", panel_id);
						hx8369b_cpt_B4h[1] = 0x02;
					}
					else{
						printk("[0x%06x]Low Temperature Compensation\n", panel_id);
						hx8369b_cpt_B4h[1] = 0x01;
					}
					printk("[LCD] temp(%d), D5(%x)---\n", val.intval, video3_1[20]);
				
				}
				else 
				{
					/* Temperature Compensation */
					if(val.intval > -50) {
						printk("[0x%06x]High Temperature Compensation\n", panel_id);
					} else {
						printk("[0x%06x]Low Temperature Compensation\n", panel_id);
					}
				}
			} else
				printk("[LCD] read temperature error\n");
		}
#endif

	switch (panel_id) {
		case HX8369B_PANEL_DTC1: /* 0x55BCF0 (BOE panel) */
			pxa168_dsi_cmd_array_tx(fbi,
					hx8369b_dtc_init_table,
					ARRAY_SIZE(hx8369b_dtc_init_table));

			if (0 == system_rev) {
				pxa168_dsi_cmd_array_tx(fbi_global, hx8369b_dtc_bl_on_cmds,
						ARRAY_SIZE(hx8369b_dtc_bl_on_cmds));
			}
			break;
		case HX8369B_PANEL_DTC2:
			pxa168_dsi_cmd_array_tx(fbi,
					hx8369b_dtc_init_table,
					ARRAY_SIZE(hx8369b_dtc_init_table));
			break;
		case HX8369B_PANEL_CPT:
			pxa168_dsi_cmd_array_tx(fbi,
					hx8369b_cpt_init_table,
					ARRAY_SIZE(hx8369b_cpt_init_table));
			break;
		default:
			pxa168_dsi_cmd_array_tx(fbi,
					hx8369b_cpt_init_table,
					ARRAY_SIZE(hx8369b_cpt_init_table));
			break;
	}
#ifdef  CONFIG_LDI_SUPPORT_MDNIE
	isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg);
#ifdef ENABLE_MDNIE_TUNING
	if(tuning_enable && mDNIe_data[0] !=  0) {
		printk("-----------[%s]-----mDNIe-------------\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi,
				hx8369b_video_display_mDNIe_cmds,
				ARRAY_SIZE(hx8369b_video_display_mDNIe_cmds));
	}
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_sleep_out_cmds,
			ARRAY_SIZE(hx8369b_video_sleep_out_cmds));
	msleep(HX8369B_SLEEP_OUT_DELAY);

	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_displayON_cmds,
			ARRAY_SIZE(hx8369b_video_displayON_cmds));
	msleep(HX8369B_DISP_ON_DELAY);

#if defined(LCD_ESD_RECOVERY)
	schedule_delayed_work(&d_work, msecs_to_jiffies(LCD_ESD_INTERVAL));
#endif

	lcd_enable = true;
#endif
/*
	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_display_on_cmds,
			ARRAY_SIZE(hx8369b_video_display_on_cmds));
*/
}
#endif

static void panel_init_config(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	switch (panel_id) {
	case HX8369B_PANEL1:
		pxa168_dsi_cmd_array_tx(fbi,
				hx8369b_video_display_init_cmds,
				ARRAY_SIZE(hx8369b_video_display_init_cmds));
		break;
	case HX8369B_PANEL2:
		pxa168_dsi_cmd_array_tx(fbi,
				hx8369b_video_display_init2_cmds,
				ARRAY_SIZE(hx8369b_video_display_init2_cmds));
		break;
	case HX8369B_PANEL_BOE1:
	case HX8369B_PANEL_BOE2:
	default:
		pxa168_dsi_cmd_array_tx(fbi,
				hx8369b_video_display_boe_init_cmds,
				ARRAY_SIZE(hx8369b_video_display_boe_init_cmds));
		break;
	}
/*
	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_display_on_cmds,
			ARRAY_SIZE(hx8369b_video_display_on_cmds));
*/			
#ifdef  CONFIG_LDI_SUPPORT_MDNIE
	isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg);
#ifdef ENABLE_MDNIE_TUNING    
	if (tuning_enable && mDNIe_data[0]) {
		printk("%s, set mDNIe\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi,
				hx8369b_video_display_mDNIe_cmds,
				ARRAY_SIZE(hx8369b_video_display_mDNIe_cmds));
	}
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */
	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_sleep_out_cmds,
			ARRAY_SIZE(hx8369b_video_sleep_out_cmds));
	msleep(HX8369B_SLEEP_OUT_DELAY);

	pxa168_dsi_cmd_array_tx(fbi,
			hx8369b_video_displayON_cmds,
			ARRAY_SIZE(hx8369b_video_displayON_cmds));
	msleep(HX8369B_DISP_ON_DELAY);

#if defined(LCD_ESD_RECOVERY)
	schedule_delayed_work(&d_work, msecs_to_jiffies(LCD_ESD_INTERVAL));
#endif
	lcd_enable = true;
#endif
}

ssize_t lcd_panelName_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "HX8369B");
}

ssize_t lcd_MTP_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	((unsigned char*)buf)[0] = (panel_id >> 16) & 0xFF;
	((unsigned char*)buf)[1] = (panel_id >> 8) & 0xFF;
	((unsigned char*)buf)[2] = panel_id & 0xFF;

	printk("ldi mtpdata: %x %x %x\n", buf[0], buf[1], buf[2]);

	return 3;
}

ssize_t lcd_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined (CONFIG_MACH_BAFFIN) ||  defined(CONFIG_MACH_BAFFINQ)
    int ret = 0;
    switch (panel_id)
    {
	case HX8369B_PANEL_DTC1:
	case HX8369B_PANEL_DTC2:
		ret = sprintf(buf, "DTC_HX8369B");  
		break;
        case HX8369B_PANEL_CPT:
		ret = sprintf(buf, "CPT_SC7798A");  
		break;
        default:
                ret = sprintf(buf, "AUO_%x%x%x",
			(panel_id >> 16) & 0xFF,
			(panel_id >> 8) & 0xFF,
			panel_id  & 0xFF); 
		break;                 
    }
    return ret;
#else
 	          return sprintf(buf, "AUO_%x%x%x",
			(panel_id >> 16) & 0xFF,
			(panel_id >> 8) & 0xFF,
			panel_id  & 0xFF);  
#endif 
}

static DEVICE_ATTR(lcd_MTP, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_MTP_show, NULL);
static DEVICE_ATTR(lcd_panelName, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_panelName_show, NULL);
static DEVICE_ATTR(lcd_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_type_show, NULL);
#ifdef CONFIG_LDI_SUPPORT_MDNIE
#ifdef ENABLE_MDNIE_TUNING
static DEVICE_ATTR(tuning, 0664, mDNIeTuning_show, mDNIeTuning_store);
#endif	/* ENABLE_MDNIE_TUNING */
static DEVICE_ATTR(scenario, 0664, mDNIeScenario_show, mDNIeScenario_store);
static DEVICE_ATTR(negative, 0664, mDNIeNegative_show, mDNIeNegative_store);
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

void __init emeidkb_add_lcd_mipi(void)
{
	unsigned int CSn_NO_COL;
	struct dsi_info *dsi;
	struct device *dev_t;

	struct pxa168fb_mach_info *fb = &mipi_lcd_info, *ovly =
		&mipi_lcd_ovly_info;

	fb->num_modes = ARRAY_SIZE(video_modes_emeidkb);
	if (QHD_PANEL == is_qhd_lcd())
		fb->modes = video_modes_emeidkb;
	else
		fb->modes = video_modes_hvga_vnc_emeidkb;

	if (1 == get_recoverymode()) {
		fb->mmap = 2;
	} else {
		fb->mmap = 3;
	}

	switch (panel_id) {
		case HX8369B_PANEL_BOE3:
			printk("%s, panel_id : 0x%x\n", __func__, panel_id);
			fb->modes->hsync_len = 32; //HWD
			fb->modes->left_margin = 70; //HBP
			fb->modes->right_margin = 100; //HFP
			fb->modes->vsync_len = 2; //VWD
			fb->modes->upper_margin = 22; //VBP
			fb->modes->lower_margin = 20; //VFP
			break;
#if defined (CONFIG_MACH_BAFFIN) ||  defined(CONFIG_MACH_BAFFINQ)
		case HX8369B_PANEL_DTC1:
		case HX8369B_PANEL_DTC2:
			printk("%s, panel_id : 0x%x\n", __func__, panel_id);
			fb->modes->hsync_len = 60; //HWD
			fb->modes->left_margin = 70; //HBP
			fb->modes->right_margin = 90; //HFP
			fb->modes->vsync_len = 4; //VWD
			fb->modes->upper_margin = 12; //VBP
			fb->modes->lower_margin = 8; //VFP
			break;
        case HX8369B_PANEL_CPT:
			printk("%s, panel_id : 0x%x\n", __func__, panel_id);
			fb->modes->hsync_len = 60; //HWD
			fb->modes->left_margin = 70; //HBP
			fb->modes->right_margin = 90; //HFP
			fb->modes->vsync_len = 4; //VWD
			fb->modes->upper_margin = 12; //VBP
			fb->modes->lower_margin = 8; //VFP
			break;
#endif            
		case HX8369B_PANEL2:
		default:
			printk("%s, panel_id : 0x%x\n", __func__, panel_id);
			break;
	}


	fb->pix_fmt = PIX_FMT_RGBA888;
	/*
	 * align with GPU, xres should be aligned for 16 pixels;
	 * yres should be aligned for 4 lines
	 */
	set_maxfb_size(fb, 16, 4);

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

	fb->phy_type = DSI;
	fb->exter_brige_pwr = NULL;
	fb->phy_info = (void *)&emeidkb_dsiinfo;
#ifdef  CONFIG_MACH_CS02
	fb->dsi_panel_config = panel_init_config_cs02;
#elif defined (CONFIG_MACH_BAFFIN) ||  defined(CONFIG_MACH_BAFFINQ)
	fb->dsi_panel_config = panel_init_config_baffin;
#else
	fb->dsi_panel_config = panel_init_config;
#endif
	fb->pxa168fb_lcd_power = emeidkb_lcd_power;

	dsi = (struct dsi_info *)fb->phy_info;
	dsi->master_mode = 1;
	dsi->hfp_en = 0;

	dither_config(fb);
	/* TODO: in which case we use slave mode? */
	if (QHD_PANEL != is_qhd_lcd() && FHD_PANEL != is_fhd_lcd())
		dsi->master_mode = 0; /* dsi use slave mode */

	/* TODO: just code add for new clock and not removed legacy codes */
	fb->no_legacy_clk = 1;
	fb->path_clk_name = "mmp_pnpath";
	fb->phy_clk_name = "mmp_dsi1";
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

	dev_t = device_create( lcd_class, NULL, 0, "%s", "panel");

	if (device_create_file(dev_t, &dev_attr_lcd_panelName) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_panelName.attr.name);
	if (device_create_file(dev_t, &dev_attr_lcd_MTP) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_MTP.attr.name);
	if (device_create_file(dev_t, &dev_attr_lcd_type) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_type.attr.name);
#ifdef  ENABLE_MDNIE_TUNING
	if (device_create_file(dev_t, &dev_attr_tuning) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tuning.attr.name);
#endif	/* ENABLE_MDNIE_TUNING */

#ifdef CONFIG_LDI_SUPPORT_MDNIE
	lcd_mDNIe_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR(lcd_mDNIe_class)) {
		printk("Failed to create mdnie!\n");
		return PTR_ERR( lcd_mDNIe_class );
	}

	dev_t = device_create( lcd_mDNIe_class, NULL, 0, "%s", "mdnie");

	if (device_create_file(dev_t, &dev_attr_scenario) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_scenario.attr.name);
	if (device_create_file(dev_t, &dev_attr_negative) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_negative.attr.name);
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */
}

//#ifdef CONFIG_PXA988_DISP_HACK
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

	fb->mmap = 0;
	/*
	 * align with GPU, xres should be aligned for 16 pixels;
	 * yres should be aligned for 4 lines
	 */
	set_maxfb_size(fb, 16, 4);

	ovly->num_modes = fb->num_modes;
	ovly->modes = fb->modes;
	ovly->max_fb_size = fb->max_fb_size;

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
//#endif /* CONFIG_PXA988_DISP_HACK */
