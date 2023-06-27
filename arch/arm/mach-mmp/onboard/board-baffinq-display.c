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

#include <mach/mfp-pxa1088-baffinq.h>

#include "../common.h"
#include "lcd_mipi_generic.h"
#include <mach/features.h>
#include "lcd_mdnie_param.h"

#define GPIO_LCD_RESET (18)

static int lcd_power(struct pxa168fb_info *, unsigned int, unsigned int, int);

///////////////////////////////////////
// LDI : HX8369B                     //
// PANEL : AUO (ID : 0x554990)       //
///////////////////////////////////////
/* SETEXTC */
static char hx8369b_dtc_B9h[] = {
	0xB9,
	0xFF, 0x83, 0x69
};

/* Set MIPI Control */
static char hx8369b_dtc_BAh[] = {
	0xBA,
	0x31, 0x00, 0x16, 0xCA, 0xB1, 0x0A, 0x00, 0x10, 0x28, 0x02,
	0x21, 0x21, 0x9A, 0x1A, 0x8F
};

/* Set GIPI */
static char hx8369b_dtc_D5h[] = {
	0xD5,
	0x00, 0x00, 0x08, 0x03, 0x30, 0x00, 0x00, 0x10, 0x01, 0x00,
	0x00, 0x00, 0x01, 0x39, 0x45, 0x00, 0x00, 0x0C, 0x44, 0x39,
	0x47, 0x05, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x59,
	0x18, 0x10, 0x00, 0x00, 0x05, 0x00, 0x00, 0x40, 0x28, 0x69,
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x49, 0x08, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x51, 0x39, 0x78, 0x50, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07,
	0xF8, 0x0F, 0xFF, 0xFF, 0x07, 0xF8, 0x0F, 0xFF, 0xFF, 0x00,
	0x20, 0x5A
};

/* I/F pixel format */
static char hx8369b_dtc_3Ah[] = {
	0x3A,
	0x70
};


/* Set power control */
static char hx8369b_dtc_B1h[] = {
	0xB1,
	0x12, 0x83, 0x77, 0x00, 0x10, 0x10, 0x1E, 0x1E, 0x0C, 0x1A,
	0x20, 0xD6
};

/* Set RGB */
static char hx8369b_dtc_B3h[] = {
	0xB3,
	0x83, 0x00, 0x3A, 0x17
};

/* SET CYC */
static char hx8369b_dtc_B4h[] = {
	0xB4,
	0x01
};

/* Set VCOM */
static char hx8369b_dtc_B6h[] = {
	0xB6,
	0x96, 0x96
};

/* Set Panel */
static char hx8369b_dtc_CCh[] = {
	0xCC,
	0x0E
};

/* Set STBA */
static char hx8369b_dtc_C0h[] = {
	0xC0,
	0x73, 0x50, 0x00, 0x20, 0xC4, 0x00
};

#if defined(CONFIG_MACH_BAFFINQ)/*For Baffinlite 6.0G*/
/* SETDGC*/
static char hx8369b_dtc_C1h[] = {
	0xC1,
	0x03, 0x00, 0x06, 0x08, 0x0A, 0x17, 0x21, 0x29, 0x31, 0x3B,
	0x44, 0x4C, 0x54, 0x5C, 0x64, 0x6C, 0x74, 0x7C, 0x84, 0x8C,
	0x94, 0x9C, 0xA4, 0xAC, 0xB4, 0xBC, 0xC4, 0xCC, 0xD4, 0xDC,
	0xE4, 0xEC, 0xF2, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x08, 0x12, 0x1D, 0x26, 0x2D,
	0x36, 0x3F, 0x47, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80,
	0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xBF, 0xC7, 0xCF,
	0xD6, 0xE0, 0xE7, 0xEF, 0xF6, 0xFC, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x12, 0x15, 0x20,
	0x29, 0x30, 0x3B, 0x43, 0x4D, 0x54, 0x5C, 0x64, 0x6C, 0x74,
	0x7C, 0x84, 0x8C, 0x94, 0x9C, 0xA4, 0xAE, 0xB6, 0xBE, 0xC6,
	0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xF9, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#else
/* SETDGC*/
static char hx8369b_dtc_C1h[] = {
	0xC1,
	0x03, 0x00, 0x06, 0x08, 0x14, 0x1A, 0x22, 0x2A, 0x32, 0x3A,
	0x42, 0x4A, 0x52, 0x5A, 0x62, 0x6A, 0x72, 0x7A, 0x82, 0x8A,
	0x92, 0x9A, 0xA2, 0xAA, 0xB2, 0xBA, 0xC2, 0xCA, 0xD2, 0xDA,
	0xE2, 0xEA, 0xF2, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x08, 0x16, 0x1D, 0x26, 0x2D,
	0x36, 0x3F, 0x47, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80,
	0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xBF, 0xC7, 0xCF,
	0xD6, 0xE0, 0xE8, 0xF0, 0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x08, 0x12, 0x1A,
	0x22, 0x2A, 0x32, 0x3A, 0x42, 0x4A, 0x52, 0x5A, 0x62, 0x6A,
	0x72, 0x7A, 0x82, 0x8A, 0x92, 0x9A, 0xA2, 0xAA, 0xB2, 0xBA,
	0xC2, 0xCA, 0xD2, 0xDA, 0xE2, 0xEA, 0xF2, 0xF4, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif

/* Set EQ */
static char hx8369b_dtc_E3h[] = {
	0xE3,
	0x0F, 0x0F, 0x11, 0x11
};

/* Set Panel */
static char hx8369b_dtc_EAh[] = {
	0xEA,
	0x7A
};

/* Set ECO */
static char hx8369b_dtc_C6h[] = {
	0xC6,
	0x41, 0xFF, 0x7C
};

#if defined(CONFIG_MACH_BAFFINQ)/*For Baffinlite 6.0G*/
/* Set Gamma */
static char hx8369b_dtc_E0h[] = {
	0xE0,
	0x00, 0x24, 0x23, 0x27, 0x30, 0x3F, 0x30, 0x46, 0x0E, 0x10,
	0x0F, 0x13, 0x15, 0x13, 0x15, 0x16, 0x19, 0x00, 0x24, 0x23,
	0x27, 0x30, 0x3F, 0x30, 0x46, 0x0E, 0x10, 0x0F, 0x13, 0x15,
	0x13, 0x15, 0x16, 0x19, 0x01
};
#else
/* Set Gamma */
static char hx8369b_dtc_E0h[] = {
	0xE0,
	0x00, 0x1B, 0x21, 0x27, 0x30, 0x3F, 0x2D, 0x46, 0x0A, 0x11,
	0x11, 0x14, 0x15, 0x15, 0x16, 0x13, 0x17, 0x00, 0x1B, 0x21,
	0x27, 0x30, 0x3F, 0x2C, 0x45, 0x09, 0x0F, 0x11, 0x15, 0x17,
	0x15, 0x16, 0x13, 0x17, 0x01
};
#endif

/* CABC PWM */
static char hx8369b_dtc_C9h[] = {
	0xC9,
	0x0F, 0x00
};

/* CABC PWM */
static char hx8369b_dtc_51h[] = {
	0x51,
	0x3F, 0x3C
};

/* CABC PWM */
static char hx8369b_dtc_53h[] = {
	0x53,
	0x24, 0x08
};

/* CABC PWM */
static char hx8369b_dtc_cabc_dis_55h[] = {
	0x55,
	0x00, 0x2C
};

static char hx8369b_dtc_55h[] = {
	0x55,
	0x01, 0x1D
};

static struct dsi_cmd_desc hx8369b_dtc_init_table[] = {
	{DSI_DI_DCS_LWRITE,HS_MODE,10,sizeof(hx8369b_dtc_B9h),hx8369b_dtc_B9h},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_BAh),hx8369b_dtc_BAh},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_D5h),hx8369b_dtc_D5h},
	{DSI_DI_DCS_SWRITE1,HS_MODE,0,sizeof(hx8369b_dtc_3Ah),hx8369b_dtc_3Ah},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_B1h),hx8369b_dtc_B1h},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_B3h),hx8369b_dtc_B3h},
	{DSI_DI_DCS_SWRITE1,HS_MODE,0,sizeof(hx8369b_dtc_B4h),hx8369b_dtc_B4h},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_B6h),hx8369b_dtc_B6h},
	{DSI_DI_DCS_SWRITE1,HS_MODE,0,sizeof(hx8369b_dtc_CCh),hx8369b_dtc_CCh},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_C0h),hx8369b_dtc_C0h},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_C1h),hx8369b_dtc_C1h},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_E3h),hx8369b_dtc_E3h},
	{DSI_DI_DCS_SWRITE1,HS_MODE,0,sizeof(hx8369b_dtc_EAh),hx8369b_dtc_EAh}, 
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_C6h),hx8369b_dtc_C6h},
	{DSI_DI_DCS_LWRITE,HS_MODE,0,sizeof(hx8369b_dtc_E0h),hx8369b_dtc_E0h},     
};

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
	.lcd_driver = &lcd_panel_driver, 
	.pxa168fb_lcd_power = lcd_power,
	.width = 61,
	.height = 102,
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

static struct fb_videomode video_modes_emeidkb = {
	.refresh = 60,
	.xres = 480,
	.yres = 800,
	.hsync_len = 60,	//HWD
	.left_margin = 70,	//HBP
	.right_margin = 90,	//HFP
	.vsync_len = 4,		//VWD
	.upper_margin = 12,	//VBP
	.lower_margin = 8,	//VFP
	.sync = 0, //FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
};

void get_lcd_panel_info(struct lcd_panel_info_t *lcd_panel_info)
{
   	printk("%s +\n", __func__);
    lcd_panel_info->panel_info_init = 1;
    lcd_panel_info->panel_name = "HX8389B";
    lcd_panel_info->dsiinfo_p = &dsiinfo;
    lcd_panel_info->mipi_lcd_info_p = &mipi_lcd_info;
    lcd_panel_info->mipi_lcd_ovly_info_p = &mipi_lcd_ovly_info;
    lcd_panel_info->video_modes_emeidkb_p = &video_modes_emeidkb;
    lcd_panel_info->video_modes_emeidkb_p_len = sizeof(struct fb_videomode);
	lcd_panel_info->lcd_panel_init_cmds = hx8369b_dtc_init_table;
	lcd_panel_info->lcd_panel_init_cmds_len = ARRAY_SIZE(hx8369b_dtc_init_table);
    lcd_panel_info->display_off_delay = 20;
    lcd_panel_info->sleep_in_delay = 180;
    lcd_panel_info->sleep_out_delay = 100;
    lcd_panel_info->display_on_delay = 5;
    lcd_panel_info->lcd_reset_gpio = GPIO_LCD_RESET;
#if defined(CONFIG_LDI_SUPPORT_MDNIE)
    lcd_panel_info->isReadyTo_mDNIe=1;
    lcd_panel_info->mDNIe_addr = 0xE6;
    lcd_panel_info->mDNIe_mode_cmds = hx8369b_video_display_mDNIe_scenario_cmds;
    lcd_panel_info->mDNIe_mode_cmds_len = ARRAY_SIZE(hx8369b_video_display_mDNIe_scenario_cmds);
#endif    
#if defined(CONFIG_LCD_ESD_RECOVERY)
	lcd_panel_info->lcd_esd_det.name = "HX8389B",
	lcd_panel_info->lcd_esd_det.mode = ESD_DET_POLLING,
	lcd_panel_info->lcd_esd_det.state = ESD_DET_NOT_INITIALIZED,
#endif
//    lcd_panel_info->set_brightness_for_init = ;
	printk("%s -\n", __func__);

}

static int lcd_power(struct pxa168fb_info *fbi,
		unsigned int spi_gpio_cs,
		unsigned int spi_gpio_reset, int on)
{
	static struct regulator *lcd_iovdd, *lcd_avdd;

	printk("%s, power %s +\n", __func__, on ? "on" : "off");
#if defined(LCD_ESD_RECOVERY)
		INIT_DELAYED_WORK(&d_work, d_work_func); // ESD self protect    
		if (panel_id) {
			printk(KERN_INFO "[LCD] schedule_delayed_work after 20 sec\n");
			schedule_delayed_work(&d_work, msecs_to_jiffies(20000));
		}
#endif

	if (!lcd_avdd) {
		lcd_avdd = regulator_get(NULL, "v_lcd_3V");
		if (IS_ERR(lcd_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			goto regu_lcd_avdd;
		}
	}

	if (on) {
		regulator_set_voltage(lcd_avdd, 3300000, 3300000);
		regulator_enable(lcd_avdd);
		mdelay(5);
	} else {
		//lcd_enable = false;

		/* set lcd reset pin down */
		if (gpio_request(GPIO_LCD_RESET, "lcd reset gpio")) {
			pr_err("gpio %d request failed\n", GPIO_LCD_RESET);
			return -EIO;
		}
		gpio_direction_output(GPIO_LCD_RESET, 0);
		msleep(40);        
		gpio_direction_output(GPIO_LCD_RESET, 1);
		gpio_free(GPIO_LCD_RESET);

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
