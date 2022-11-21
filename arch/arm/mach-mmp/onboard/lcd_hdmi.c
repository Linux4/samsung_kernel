#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/mfp-mmp2.h>
#include <mach/mmp3.h>
#include <mach/pxa168fb.h>

#define TV_FB_XRES      1920
#define TV_FB_YRES      1080

static struct fb_videomode tv_video_modes[] = {
	[0] = {
		/*.pixclock     = 6734,*/
		.pixclock = 13513,
		/*.refresh      = 60,*/
		.refresh = 24,
		.xres = 1920,
		.yres = 1080,
		.hsync_len = 44,
		/*.left_margin  = 88,*/
		.left_margin = 238,
		.right_margin = 148,
		.vsync_len = 5,
		.upper_margin = 4,
		.lower_margin = 36,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[1] = {
		.pixclock = 13149,
		.refresh = 60,
		.xres = 1280,
		.yres = 720,
		.hsync_len = 40,
		.left_margin = 110,
		.right_margin = 220,
		.vsync_len = 5,
		.upper_margin = 5,
		.lower_margin = 20,
		.sync = FB_SYNC_HOR_HIGH_ACT,
	},
	[2] = {
		.pixclock = 37000,
		.refresh = 60,
		.xres = 720,
		.yres = 480,
		.hsync_len = 62,
		.left_margin = 16,
		.right_margin = 60,
		.vsync_len = 6,
		.upper_margin = 9,
		.lower_margin = 30,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[3] = {
		.pixclock = 39682,
		.refresh = 60,
		.xres = 640,
		.yres = 480,
		.hsync_len = 96,
		.left_margin = 16,
		.right_margin = 48,
		.vsync_len = 2,
		.upper_margin = 10,
		.lower_margin = 33,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[4] = {
		.pixclock = 37070,
		.refresh = 60,
		.xres = 1440,
		.yres = 240,
		.hsync_len = 124,
		.left_margin = 38,
		.right_margin = 114,
		.vsync_len = 3,
		.upper_margin = 4,
		.lower_margin = 15,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[5] = {
		.pixclock = 37036,
		.refresh = 50,
		.xres = 720,
		.yres = 576,
		.hsync_len = 64,
		.left_margin = 12,
		.right_margin = 68,
		.vsync_len = 5,
		.upper_margin = 5,
		.lower_margin = 39,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[6] = {
		.pixclock = 37095,
		.refresh = 50,
		.xres = 1440,
		.yres = 288,
		.hsync_len = 126,
		.left_margin = 24,
		.right_margin = 138,
		.vsync_len = 3,
		.upper_margin = 3,
		.lower_margin = 18,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[7] = {
		.pixclock = 18535,
		.refresh = 60,
		.xres = 2880,
		.yres = 240,
		.hsync_len = 248,
		.left_margin = 76,
		.right_margin = 228,
		.vsync_len = 3,
		.upper_margin = 4,
		.lower_margin = 15,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[8] = {
		.pixclock = 18500,
		.refresh = 60,
		.xres = 1440,
		.yres = 480,
		.hsync_len = 124,
		.left_margin = 32,
		.right_margin = 120,
		.vsync_len = 6,
		.upper_margin = 9,
		.lower_margin = 30,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[9] = {
		.pixclock = 1674,
		.refresh = 50,
		.xres = 2880,
		.yres = 288,
		.hsync_len = 252,
		.left_margin = 48,
		.right_margin = 276,
		.vsync_len = 3,
		.upper_margin = 3,
		.lower_margin = 18,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[10] = {
		.pixclock = 18518,
		.refresh = 50,
		.xres = 1440,
		.yres = 576,
		.hsync_len = 128,
		.left_margin = 24,
		.right_margin = 136,
		.vsync_len = 5,
		.upper_margin = 5,
		.lower_margin = 39,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[11] = {
		.pixclock = 9250,
		.refresh = 60,
		.xres = 2880,
		.yres = 480,
		.hsync_len = 248,
		.left_margin = 64,
		.right_margin = 240,
		.vsync_len = 6,
		.upper_margin = 9,
		.lower_margin = 30,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},
	[12] = {
		.pixclock = 9259,
		.refresh = 50,
		.xres = 2880,
		.yres = 576,
		.hsync_len = 256,
		.left_margin = 48,
		.right_margin = 272,
		.vsync_len = 5,
		.upper_margin = 5,
		.lower_margin = 39,
		.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
	},

};
#ifdef CONFIG_CPU_MMP3
static struct pxa168fb_mach_info tv_out_info = {
	.id = "TV GFX Layer",
	.num_modes = ARRAY_SIZE(tv_video_modes),
	.modes = tv_video_modes,
	.sclk_div = 0x60010005,
	.pix_fmt = PIX_FMT_RGB565,
	.isr_clear_mask	= LCD_ISR_CLEAR_MASK_PXA168,
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
	.max_fb_size = TV_FB_XRES * TV_FB_YRES * 8 + 4096,
	.phy_type = DPI,
	.vdma_enable = 1,
	.sram_size = 30 * 1024,
};

static struct pxa168fb_mach_info tv_out_ovly_info = {
	.id = "TV Video Layer",
	.num_modes = ARRAY_SIZE(tv_video_modes),
	.modes = tv_video_modes,
	.pix_fmt = PIX_FMT_RGB565,
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
	.max_fb_size = TV_FB_XRES * TV_FB_YRES * 8 + 4096,
	.vdma_enable = 0,
	.sram_size = 30 * 1024,
};



void __init mmp3_add_tv_out(void)
{
	struct pxa168fb_mach_info *fb = &tv_out_info;
	struct pxa168fb_mach_info *ovly = &tv_out_ovly_info;

	/* add frame buffer drivers */
	mmp3_add_fb_tv(fb);

	/* add overlay driver */
#ifdef CONFIG_MMP_V4L2_OVERLAY
	mmp3_add_v4l2_tv_ovly(ovly);
#else
	mmp3_add_fb_tv_ovly(ovly);
#endif
}
#endif

