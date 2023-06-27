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
#include "../common.h"
#include <mach/features.h>
#include "lcd_mipi_generic.h"
#include "lcd_mdnie_param.h"
#if defined(CONFIG_LCD_ESD_RECOVERY)
#include "esd_detect.h"
extern int pxa168fb_reinit(void *pdata);
#endif
#if defined(CONFIG_LDI_SUPPORT_MDNIE)
static struct class *lcd_mDNIe_class;
static struct mdnie_config mDNIe_cfg;
#ifdef ENABLE_MDNIE_TUNING
#define TUNING_FILE_PATH "/sdcard/mdnie/"
#define MAX_TUNING_FILE_NAME 100
#define NR_MDNIE_DATA 114
static int tuning_enable;
static char tuning_filename[MAX_TUNING_FILE_NAME];
static unsigned char mDNIe_data[NR_MDNIE_DATA] = {0xCD,};
static struct dsi_cmd_desc video_display_mDNIe_tuning_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(mDNIe_data),mDNIe_data},
};
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

int lcd_enable = 1;
u32 panel_id = 0;
static struct pxa168fb_info *fbi_global = NULL;
static struct lcd_panel_info_t lcd_panel_info;
static int lcd_panel_init(struct pxa168fb_info *fbi);
static int lcd_panel_enable(struct pxa168fb_info *fbi);
static int lcd_panel_disable(struct pxa168fb_info *fbi);
static int lcd_panel_probe(struct pxa168fb_info *fbi);
static int lcd_panel_reset(struct pxa168fb_info *fbi);

struct pxa168fb_mipi_lcd_driver lcd_panel_driver = {
	.probe		= lcd_panel_probe,
    .reset      = lcd_panel_reset,
	.init		= lcd_panel_init,
	.disable	= lcd_panel_disable,
	.enable		= lcd_panel_enable,
};

static u8 exit_sleep[] = {0x11, 0x00};
static u8 display_on[] = {0x29, 0x00};
static u8 display_off[] = {0x28, 0x00};
static u8 enter_sleep[] = {0x10, 0x00};

static struct dsi_cmd_desc sleep_out_table[] = {
	{DSI_DI_DCS_SWRITE, HS_MODE, 0, sizeof(exit_sleep),exit_sleep},
};

static struct dsi_cmd_desc display_on_table[] = {
	{DSI_DI_DCS_SWRITE, HS_MODE, 0, sizeof(display_on),display_on},
};

static struct dsi_cmd_desc display_off_table[] = {
	{DSI_DI_DCS_SWRITE, HS_MODE, 0, sizeof(display_off),display_off},
};

static struct dsi_cmd_desc sleep_in_table[] = {
	{DSI_DI_DCS_SWRITE, HS_MODE, 0, sizeof(enter_sleep),enter_sleep},
};

static char pkt_size_cmd[] = {0x1};
static char read_id1[] = {0xda};
static char read_id2[] = {0xdb};
static char read_id3[] = {0xdc};
static char read_esd[] = {0x0a};

static struct dsi_cmd_desc hx8369b_video_read_id1_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, HS_MODE, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, HS_MODE, 0, sizeof(read_id1), read_id1},
};

static struct dsi_cmd_desc hx8369b_video_read_id2_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, HS_MODE, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, HS_MODE, 0, sizeof(read_id2), read_id2},
};

static struct dsi_cmd_desc hx8369b_video_read_id3_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, HS_MODE, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, HS_MODE, 0, sizeof(read_id3), read_id3},
};

static struct dsi_cmd_desc hx8369b_video_read_esd_cmds[] = {
	{DSI_DI_SET_MAX_PKT_SIZE, HS_MODE, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{DSI_DI_DCS_READ, HS_MODE, 0, sizeof(read_esd), read_esd},
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

bool lcd_is_connected(void)
{
	return panel_id ? true : false;
}
EXPORT_SYMBOL(lcd_is_connected);

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

int dsi_init(struct pxa168fb_info *fbi)
{
#ifdef CONFIG_PXA688_PHY
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	//struct pxa168fb_lcd_platform_driver *lcd_pd = mi->lcd_platform_driver;
	struct pxa168fb_mipi_lcd_driver *lcd_driver = mi->lcd_driver;
	printk("%s +\n", __func__);
	if (fbi->skip_pw_on) {
		printk("%s, skip dsi_init\n", __func__);
		return 0;
	}
	if (!fbi_global)
		fbi_global = fbi;
		
//	dsi_dphy_exit_ulps_mode(fbi);
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

	lcd_driver->reset(fbi);

	/*  reset the bridge */
	if (mi->exter_brige_pwr) {
		mi->exter_brige_pwr(fbi,1);
		mdelay(10);
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
			pr_err("exter_brige_init error!\n");
	}
#endif
	return 0;
}

void __init emeidkb_add_lcd_mipi(void)
{
	unsigned int CSn_NO_COL;
	struct dsi_info *dsi;
	struct pxa168fb_mach_info *fb, *ovly;

    printk("%s + \n", __func__);
    lcd_panel_info.panel_info_init = 0;
    get_lcd_panel_info(&lcd_panel_info);

    fb = (struct pxa168fb_mach_info *)lcd_panel_info.mipi_lcd_info_p;
    ovly = (struct pxa168fb_mach_info *)lcd_panel_info.mipi_lcd_ovly_info_p;
    
	fb->num_modes = lcd_panel_info.video_modes_emeidkb_p_len;
    printk("%d \n", fb->num_modes);
	fb->modes = (struct fb_videomode *)lcd_panel_info.video_modes_emeidkb_p;
    printk("xres : %d \n", fb->modes->xres);
    
    fb->lcd_driver = &lcd_panel_driver;
//	fb->panel_rbswap = 0;

	if (1 == get_recoverymode())
		fb->mmap = 2;
	else
		fb->mmap = 3;

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
	fb->phy_info = lcd_panel_info.dsiinfo_p;

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
}

//#ifdef CONFIG_PXA988_DISP_HACK
void __init emeidkb_add_tv_out(void)
{
	struct pxa168fb_mach_info *fb, *ovly;

    printk("%s + \n", __func__);
    lcd_panel_info.panel_info_init = 0;
    get_lcd_panel_info(&lcd_panel_info);

    fb = lcd_panel_info.mipi_lcd_info_p;
    ovly = lcd_panel_info.mipi_lcd_ovly_info_p;
    
	/* Change id for TV GFX layer to avoid duplicate with panel path */
	strncpy(fb->id, "TV GFX Layer", 13);
	fb->num_modes = lcd_panel_info.video_modes_emeidkb_p_len;
	fb->modes = lcd_panel_info.video_modes_emeidkb_p;

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
	fb->phy_info = lcd_panel_info.dsiinfo_p;
	fb->exter_brige_pwr = NULL;
	fb->dsi_panel_config = NULL;
	fb->pxa168fb_lcd_power = NULL;

	dither_config(fb);
	/*
	 * Re-calculate lcd clk source and divider
	 * according to dsi lanes and output format.
	 */
#if 0
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
#endif

	/*
	 * FIXME:EMEI dkb use display clk1 as clk source,
	 * which is from PLL1 416MHZ. PLL3 1GHZ will be used
	 * for cpu core,and can't be DSI clock source specially.
	 */
	fb->sclk_div &= 0x0fffffff;
	fb->sclk_div |= 0x40000000;

	pxa988_add_fb_tv(fb);
	pxa988_add_fb_tv_ovly(ovly);
    printk("%s - \n", __func__);    
}

#if defined(CONFIG_LCD_ESD_RECOVERY)
static int is_normal_for_ESD(void *pdata)
{
	struct pxa168fb_info *fbi = fbi_global;
	int i;

	if (fbi->active) {
		struct dsi_buf dbuf;

		for (i = 0; i < 3; i++) {
			pxa168_dsi_cmd_array_rx(fbi_global, &dbuf,
					hx8369b_video_read_esd_cmds,
					ARRAY_SIZE(hx8369b_video_read_esd_cmds));
			if (dbuf.data[0] == 0x9C) {
				return ESD_NOT_DETECTED;
			}
			printk("[LCD] esd detected [0Ah:0x%x](retry : %d)\n", dbuf.data[0], i);
			msleep(500);
		}

		if (dbuf.data[0] != 0x9C) {
			return ESD_DETECTED;
		}
	}
}

static bool is_active_for_ESD(void)
{
	pr_info("lcd is %s\n",
			fbi_global->active ? "active" : "inactive");
	return fbi_global->active;
}
#endif


#if defined(CONFIG_LDI_SUPPORT_MDNIE)
#ifdef ENABLE_MDNIE_TUNING
static int parse_text(char *src, int len)
{
	int i;
	int data=0, value=0, count=1, comment=0;
	char *cur_position;

	//count++;
    mDNIe_data[0] = lcd_panel_info.mDNIe_addr;
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

#if 0
    for(i = 0; i < 114; i++)
    {
		printk(" 0x%02X\n", mDNIe_data[i]);
        if(i % 15 == 0)
            printk("\n");
    }
#endif
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

static ssize_t mDNIeTuning_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret = 0;
	ret = sprintf(buf,"Tunned File Name : %s\n",tuning_filename);

	return ret;
}

static ssize_t mDNIeTuning_store(struct device *dev,
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
			pxa168_dsi_cmd_array_tx(fbi_global, video_display_mDNIe_tuning_cmds,ARRAY_SIZE(video_display_mDNIe_tuning_cmds));
		}
	}
	return size;
}
#endif	/* ENABLE_MDNIE_TUNING */

static void set_mDNIe_Mode(struct mdnie_config *mDNIeCfg)
{
	int value;

	printk("%s:[mDNIe] negative=%d\n", __func__, mDNIe_cfg.negative);
	if(!lcd_panel_info.isReadyTo_mDNIe)
		return;
	msleep(100);
	if (mDNIe_cfg.negative) {
		printk("%s : apply negative color\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi_global, &lcd_panel_info.mDNIe_mode_cmds[SCENARIO_MAX],(lcd_panel_info.mDNIe_mode_cmds_len/(SCENARIO_MAX+1)));
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
		pxa168_dsi_cmd_array_tx(fbi_global, &lcd_panel_info.mDNIe_mode_cmds[value],(lcd_panel_info.mDNIe_mode_cmds_len/(SCENARIO_MAX+1)));
	return;
}

static ssize_t mDNIeScenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret;
	ret = sprintf(buf,"mDNIeScenario_show : %d\n", mDNIe_cfg.scenario);
	return ret;
}

static ssize_t mDNIeScenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	if (strict_strtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

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
	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}

static ssize_t mDNIeOutdoor_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret;
	ret = sprintf(buf,"mDNIeOutdoor_show : %d\n", mDNIe_cfg.outdoor);

	return ret;
}

static ssize_t mDNIeOutdoor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	if (strict_strtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	printk("%s:value=%d\n", __func__, value);
	mDNIe_cfg.outdoor = value;
	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}

static ssize_t mDNIeNegative_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf,"mDNIeNegative_show : %d\n", mDNIe_cfg.negative);
	return ret;
}

static ssize_t mDNIeNegative_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	if (strict_strtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	printk("%s:value=%d\n", __func__, value);

	if(value == 1) {
		mDNIe_cfg.negative = 1;
	} else {
		mDNIe_cfg.negative = 0;
	}

	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

static ssize_t lcd_panelName_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    //should have struct parmeter
	return sprintf(buf, lcd_panel_info.panel_name);
}

static ssize_t lcd_MTP_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int panel_id = get_panel_id();

	((unsigned char*)buf)[0] = (panel_id >> 16) & 0xFF;
	((unsigned char*)buf)[1] = (panel_id >> 8) & 0xFF;
	((unsigned char*)buf)[2] = panel_id & 0xFF;

	printk("ldi mtpdata: %x %x %x\n", buf[0], buf[1], buf[2]);

	return 3;
}

static ssize_t lcd_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int panel_id = get_panel_id();
    int DBh_value = (panel_id >> 8) & 0xFF;

//	printk("lcd_type_show : %x %x %x\n",(panel_id >> 16) & 0xFF,(panel_id >> 8) & 0xFF, panel_id  & 0xFF);
    if(DBh_value & 0x04)
    {
    	return sprintf(buf, "INH_%x%x%x",(panel_id >> 16) & 0xFF,(panel_id >> 8) & 0xFF, panel_id  & 0xFF);
    }
    else
    {
        switch(DBh_value >> 3)
        {
            case 0x08 : //CPT
            	return sprintf(buf, "CPT_%s",lcd_panel_info.panel_name);
                break;
            case 0x09 : //AUO
            	return sprintf(buf, "AUO_%s",lcd_panel_info.panel_name);
                break;
            case 0x17 : //BOE
            	return sprintf(buf, "BOE_%s",lcd_panel_info.panel_name);
                break;
            case 0x18 : //DTC
            	return sprintf(buf, "DTC_%s",lcd_panel_info.panel_name);
                break;
            default :
            	return sprintf(buf, "Unknow. need to check");
                break;
        }
    }
}

static int lcd_panel_reset(struct pxa168fb_info *fbi)
{
	printk("%s\n", __func__);
    int lcd_reset = lcd_panel_info.lcd_reset_gpio;
    
	if (gpio_request(lcd_reset, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_reset);
		return -EIO;
	}
	//gpio_direction_output(lcd_reset, 1);
	mdelay(1);
	gpio_direction_output(lcd_reset, 0);
	mdelay(1);
	gpio_direction_output(lcd_reset, 1);
	mdelay(120);

	gpio_free(lcd_reset);

	return 0;
}

static int lcd_panel_init(struct pxa168fb_info *fbi)
{
	printk("%s +\n", __func__);

	pxa168_dsi_cmd_array_tx(fbi, lcd_panel_info.lcd_panel_init_cmds,
			lcd_panel_info.lcd_panel_init_cmds_len);

#if defined(CONFIG_LDI_SUPPORT_MDNIE)
	lcd_panel_info.isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg);
	if (tuning_enable && mDNIe_data[0]) {
		printk("%s, set mDNIe\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi,
				video_display_mDNIe_tuning_cmds,
				ARRAY_SIZE(video_display_mDNIe_tuning_cmds));
	}
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */    
	printk("%s -\n", __func__);
	return 0;
}

static int lcd_panel_enable(struct pxa168fb_info *fbi)
{
	printk("%s\n", __func__);
	pxa168_dsi_cmd_array_tx(fbi, sleep_out_table,
			ARRAY_SIZE(sleep_out_table));
   	msleep(lcd_panel_info.sleep_out_delay);
	pxa168_dsi_cmd_array_tx(fbi, display_on_table,
			ARRAY_SIZE(display_on_table));
	msleep(lcd_panel_info.display_on_delay);    
    lcd_enable = 1;
#if defined(CONFIG_LCD_ESD_RECOVERY)
	    esd_det_enable(&lcd_panel_info.lcd_esd_det);
#endif    
	printk("%s -\n", __func__);
	return 0;
}

static int lcd_panel_disable(struct pxa168fb_info *fbi)
{
	printk("%s +\n", __func__);
#if defined(CONFIG_LDI_SUPPORT_MDNIE)
	lcd_panel_info.isReadyTo_mDNIe = 0;
#endif
#if defined(CONFIG_LCD_ESD_RECOVERY)
	    esd_det_disable(&lcd_panel_info.lcd_esd_det);
#endif
    lcd_enable = 0;
	/*
	 * Sperated lcd off routine multi tasking issue
	 * becasue of share of cpu0 after suspend.
	 */
	pxa168_dsi_cmd_array_tx(fbi, display_off_table,
			ARRAY_SIZE(display_off_table));
	msleep(lcd_panel_info.display_off_delay);

	pxa168_dsi_cmd_array_tx(fbi, sleep_in_table,
			ARRAY_SIZE(sleep_in_table));
	msleep(lcd_panel_info.sleep_in_delay);
	printk("%s -\n", __func__);

	return 0;
}

static DEVICE_ATTR(lcd_MTP, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_MTP_show, NULL);
static DEVICE_ATTR(lcd_panelName, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_panelName_show, NULL);
static DEVICE_ATTR(lcd_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_type_show, NULL);
#if defined(CONFIG_LDI_SUPPORT_MDNIE)
#ifdef ENABLE_MDNIE_TUNING
static DEVICE_ATTR(tuning, 0664, mDNIeTuning_show, mDNIeTuning_store);
#endif	/* ENABLE_MDNIE_TUNING */
static DEVICE_ATTR(scenario, 0664, mDNIeScenario_show, mDNIeScenario_store);
static DEVICE_ATTR(outdoor, 0664, mDNIeOutdoor_show, mDNIeOutdoor_store);
static DEVICE_ATTR(negative, 0664, mDNIeNegative_show, mDNIeNegative_store);
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

static int lcd_panel_probe(struct pxa168fb_info *fbi)
{
	struct device *dev_t;

	printk("%s +\n", __func__);    

	fbi_global = fbi;

	dev_t = device_create( lcd_class, NULL, 0, "%s", "panel");

	if (device_create_file(dev_t, &dev_attr_lcd_panelName) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_panelName.attr.name);
	if (device_create_file(dev_t, &dev_attr_lcd_MTP) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_MTP.attr.name);
	if (device_create_file(dev_t, &dev_attr_lcd_type) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_type.attr.name);

#if defined(CONFIG_LDI_SUPPORT_MDNIE)
	lcd_mDNIe_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR(lcd_mDNIe_class)) {
		printk("Failed to create mdnie!\n");
		return PTR_ERR( lcd_mDNIe_class );
	}

	dev_t = device_create( lcd_mDNIe_class, NULL, 0, "%s", "mdnie");
#ifdef  ENABLE_MDNIE_TUNING
	if (device_create_file(dev_t, &dev_attr_tuning) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tuning.attr.name);
#endif	/* ENABLE_MDNIE_TUNING */
	if (device_create_file(dev_t, &dev_attr_scenario) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_scenario.attr.name);
	if (device_create_file(dev_t, &dev_attr_outdoor) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_outdoor.attr.name);
	if (device_create_file(dev_t, &dev_attr_negative) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_negative.attr.name);
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

#if defined(CONFIG_LCD_ESD_RECOVERY)
	if (panel_id) {
    	printk(KERN_INFO "[LCD] enable ESD\n");
    	lcd_panel_info.lcd_esd_det.pdata = fbi;
    	lcd_panel_info.lcd_esd_det.lock = &fbi->output_lock;
        lcd_panel_info.lcd_esd_det.is_active = is_active_for_ESD;
        lcd_panel_info.lcd_esd_det.is_normal = is_normal_for_ESD;
        lcd_panel_info.lcd_esd_det.recover = pxa168fb_reinit;
		esd_det_init(&lcd_panel_info.lcd_esd_det);
	}
#endif

	printk("%s -\n", __func__);
	return 0;
}
