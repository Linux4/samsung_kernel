/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <mach/mfp-pxa910.h>
#include <mach/pxa910.h>
#include <mach/pxa168fb.h>
#include <mach/cputype.h>

#include "../common.h"

static int (*spi_send)(struct pxa168fb_info *, void *, int , unsigned int);

static unsigned long truly_lcd_pin_config[] = {
	GPIO104_LCD_DD18,
	GPIO105_LCD_DD19,
	GPIO106_LCD_DD20,
	GPIO107_LCD_DD21,
	GPIO108_LCD_DD22,
	GPIO109_LCD_DD23,
};

/* Truly TFT1P3624-E WVGA LCD power on cmd */
static u16 truly_spi_cmdon[] = {
/* Set password */
	0xB9,
	0x1FF,
	0x183,
	0x169,
/*Set power*/
	0xB1,
	0x185,
	0x100,
	0x134,
	0x107,
	0x100,
	0x10F,
	0x10F,
	0x12A,
	0x132,
	0x13F,
	0x13F,
/* Update VBIAS */
	0x101,
	0x13A,
	0x101,
	0x1E6,
	0x1E6,
	0x1E6,
	0x1E6,
	0x1E6,
/* Set Display 480x800 */
	0xB2,
	0x100,
	0x123,/* 28 */
	0x103,/* 05 */
	0x103,/* 05 */
	0x170,
	0x100,
	0x1FF,
	0x100,
	0x100,
	0x100,
	0x100,
	0x103,
	0x103,
	0x100,
	0x101,
/* Set Display 480x800 */
	0xB4,
	0x100,
	0x118,
	0x180,
	0x106,
	0x102,
/* Set VCOM */
	0xB6,
	0x142,/* Update VCOM */
	0x142,

	0xD5,
	0x100,
	0x104,
	0x103,
	0x100,
	0x101,
	0x104,
	0x11A,
	0x1FF,
	0x101,
	0x113,
	0x100,
	0x100,
	0x140,
	0x106,
	0x151,
	0x107,
	0x100,
	0x100,
	0x141,
	0x106,
	0x150,
	0x107,
	0x107,
	0x10F,
	0x104,
	0x100,
};
/* Truly TFT1P3624-E WVGA LCD Gamma2.2 */
static u16 truly_spi_cmd_gamma[] = {
/* Gamma2.2 */
	0xE0,
	0x100,
	0x113,
	0x119,
	0x138,
	0x13D,
	0x13F,
	0x128,
	0x146,
	0x107,
	0x10D,
	0x10E,
	0x112,
	0x115,
	0x112,
	0x114,
	0x10F,
	0x117,
	0x100,
	0x113,
	0x119,
	0x138,
	0x13D,
	0x13F,
	0x128,
	0x146,
	0x107,
	0x10D,
	0x10E,
	0x112,
	0x115,
	0x112,
	0x114,
	0x10F,
	0x117,
};

/* Truly TFT1P3624-E WVGA LCD color */
static u16 truly_spi_cmd_color[] = {
	0xC1,
	0x101,
/* R */
	0x104,
	0x113,
	0x11A,
	0x120,
	0x127,
	0x12C,
	0x132,
	0x136,
	0x13F,
	0x147,
	0x150,
	0x159,
	0x160,
	0x168,
	0x171,
	0x17B,
	0x182,
	0x189,
	0x191,
	0x198,
	0x1A0,
	0x1A8,
	0x1B0,
	0x1B8,
	0x1C1,
	0x1C9,
	0x1D0,
	0x1D7,
	0x1E0,
	0x1E7,
	0x1EF,
	0x1F7,
	0x1FE,
	0x1CF,
	0x152,
	0x134,
	0x1F8,
	0x151,
	0x1F5,
	0x19D,
	0x175,
	0x100,
/* G */
	0x104,
	0x113,
	0x11A,
	0x120,
	0x127,
	0x12C,
	0x132,
	0x136,
	0x13F,
	0x147,
	0x150,
	0x159,
	0x160,
	0x168,
	0x171,
	0x17B,
	0x182,
	0x189,
	0x191,
	0x198,
	0x1A0,
	0x1A8,
	0x1B0,
	0x1B8,
	0x1C1,
	0x1C9,
	0x1D0,
	0x1D7,
	0x1E0,
	0x1E7,
	0x1EF,
	0x1F7,
	0x1FE,
	0x1CF,
	0x152,
	0x134,
	0x1F8,
	0x151,
	0x1F5,
	0x19D,
	0x175,
	0x100,
/* B */
	0x104,
	0x113,
	0x11A,
	0x120,
	0x127,
	0x12C,
	0x132,
	0x136,
	0x13F,
	0x147,
	0x150,
	0x159,
	0x160,
	0x168,
	0x171,
	0x17B,
	0x182,
	0x189,
	0x191,
	0x198,
	0x1A0,
	0x1A8,
	0x1B0,
	0x1B8,
	0x1C1,
	0x1C9,
	0x1D0,
	0x1D7,
	0x1E0,
	0x1E7,
	0x1EF,
	0x1F7,
	0x1FE,
	0x1CF,
	0x152,
	0x134,
	0x1F8,
	0x151,
	0x1F5,
	0x19D,
	0x175,
	0x100,
};

/* Truly TFT1P3624-E WVGA LCD cmd on */
static u16 truly_spi_cmd_on1[] = {
/*Adjust parameter in 0x36H can turn over Gate and source.
*	0x36,
*	0x80,
*/
	0x3A,
	0x177,
	0X11,
};
/* Truly TFT1P3624-E WVGA LCD cmd on */
static u16 truly_spi_cmd_on2[] = {
	0x29,
	0x2C,
};

/* Truly TFT1P3624-E WVGA LCD cmd off */
static u16 truly_spi_cmd_off[] = {
	0x10,
};
static struct fb_videomode truly_video_modes[] = {
	/* truly tft1p3623-E WVGA mode info */
	[0] = {
		.pixclock       = 41028,
		.refresh        = 60,
		.xres           = 480,
		.yres           = 800,
		.hsync_len      = 14,
		.left_margin    = 5,
		.right_margin   = 5,
		.vsync_len      = 2,
		.upper_margin   = 2,
		.lower_margin   = 2,
		.sync           = 0,
	},
};

static int truly_lcd_power(struct pxa168fb_info *fbi, unsigned int spi_gpio_cs,
					unsigned int spi_gpio_reset, int on)
{
	int err = 0;

	mfp_config(ARRAY_AND_SIZE(truly_lcd_pin_config));
	if (on) {
		if (spi_gpio_reset != -1) {
			err = gpio_request(spi_gpio_reset, "LCD_SPI_RESET");
			if (err) {
				pr_err("failed to request GPIO for LCD RESET\n");
				return -1;
			}
			gpio_direction_output(spi_gpio_reset, 0);
			msleep(1);
			gpio_direction_output(spi_gpio_reset, 1);
			msleep(150);

		}

		err = spi_send(fbi, truly_spi_cmdon,
			ARRAY_SIZE(truly_spi_cmdon), spi_gpio_cs);
		if (err) {
			pr_err("failed to send spi command on\n");
			return -1;
		}
		err = spi_send(fbi, truly_spi_cmd_gamma,
			ARRAY_SIZE(truly_spi_cmd_gamma), spi_gpio_cs);
		if (err) {
			pr_err("failed to send spi command gamma setting\n");
			return -1;
		}
		msleep(10);
		err = spi_send(fbi, truly_spi_cmd_color,
			ARRAY_SIZE(truly_spi_cmd_color), spi_gpio_cs);
		if (err) {
			pr_err("failed to send spi command RGB setting\n");
			return -1;
		}
		msleep(10);
		err = spi_send(fbi, truly_spi_cmd_on1,
			ARRAY_SIZE(truly_spi_cmd_on1), spi_gpio_cs);
		if (err) {
			pr_err("failed to send spi command on1\n");
			return -1;
		}
		msleep(120);
		err = spi_send(fbi, truly_spi_cmd_on2,
			ARRAY_SIZE(truly_spi_cmd_on2), spi_gpio_cs);
		if (err) {
			pr_err("failed to send spi command on2\n");
			return -1;
		}
		if (spi_gpio_reset != -1)
			gpio_free(spi_gpio_reset);
	} else  {
		err = spi_send(fbi, truly_spi_cmd_off,
			ARRAY_SIZE(truly_spi_cmd_off), spi_gpio_cs);
		if (err) {
			pr_err("failed to send spi command off\n");
			return -1;
		}

		err = gpio_request(spi_gpio_reset, "LCD_SPI_RESET");
		if (err) {
			pr_err("failed to request LCD RESET gpio\n");
			return -1;
		}
		gpio_direction_output(spi_gpio_reset, 0);
		gpio_free(spi_gpio_reset);
	}
	return err;
}

/* SPI Control Register. */
/* 0xFF~0x2 */
#define     CFG_SCLKCNT(div)                    (div<<24)
/* 0x1F~0x1 */
#define     CFG_RXBITS(rx)                      ((rx - 1)<<16)
/* 0x1F~0x1, 0x1: 2bits ... 0x1F: 32bits */
#define     CFG_TXBITS(tx)                      ((tx - 1)<<8)
#define     CFG_SPI_ENA(spi)                    (spi<<3)
/* 1: port1; 0: port0 */
#define     CFG_SPI_SEL(spi)                    (spi<<2)
/* 1: 3-wire; 0: 4-wire */
#define     CFG_SPI_3W4WB(wire)                 (wire<<1)

static struct pxa168fb_mach_info truly_lcd_info  = {
	.id                     = "Base",
	.modes                  = truly_video_modes,
	.sclk_div               = 0x4000000d,
	.isr_clear_mask		= LCD_ISR_CLEAR_MASK_PXA910,
	.num_modes              = ARRAY_SIZE(truly_video_modes),
	.pix_fmt                = PIX_FMT_RGB888PACK,
	.io_pad_ctrl		= PIN_MODE_DUMB_24 | CFG_CYC_BURST_LEN8,
	.dumb_mode              = DUMB_MODE_RGB888,
	.active                 = 1,
	.spi_ctrl               = CFG_SCLKCNT(32) | CFG_TXBITS(9) |
				  CFG_SPI_3W4WB(1) | CFG_SPI_ENA(1),
	.spi_gpio_cs            = GPIO_EXT0(5),
	.spi_gpio_reset         = GPIO_EXT0(3),
	.mmap                   = 1,
	.pxa168fb_lcd_power     = truly_lcd_power,
	.invert_pixclock        = 1,
	.panel_rbswap	        = 1,
	.max_fb_size	        = 800 * 480 * 8,
};

static struct pxa168fb_mach_info truly_lcd_ovly_info = {
	.id                     = "Ovly",
	.modes                  = truly_video_modes,
	.num_modes              = ARRAY_SIZE(truly_video_modes),
	.pix_fmt                = PIX_FMT_RGB888PACK,
	.io_pad_ctrl		= PIN_MODE_DUMB_24 | CFG_CYC_BURST_LEN8,
	.dumb_mode              = DUMB_MODE_RGB888,
	.active                 = 1,
	.spi_ctrl               = CFG_SCLKCNT(32) | CFG_TXBITS(9) |
				  CFG_SPI_3W4WB(1) | CFG_SPI_ENA(1),
	.spi_gpio_cs            = -1,
	.mmap                   = 0,
	.pxa168fb_lcd_power     = truly_lcd_power,
	.invert_pixclock        = 1,
	.panel_rbswap           = 1,
};

#ifdef CONFIG_MACH_TTC_DKB
#define SDRAM_CONFIG0_TYPE1 0X20
extern unsigned char __iomem *dmc_membase;
void __init dkb_add_lcd_truly(void)
{
	unsigned int CSn_NO_COL;
	struct pxa168fb_mach_info *fb = &truly_lcd_info,
				  *ovly = &truly_lcd_ovly_info;

	CSn_NO_COL = __raw_readl(dmc_membase + SDRAM_CONFIG0_TYPE1) >> 4;
	CSn_NO_COL &= 0xF;
	if (CSn_NO_COL <= 0x2) {
		/*
		 *If DDR page size < 4KB,
		 *select no crossing 1KB boundary check
		 */
		fb->io_pad_ctrl |= CFG_BOUNDARY_1KB;
		ovly->io_pad_ctrl |= CFG_BOUNDARY_1KB;
	}

	spi_send = pxa168fb_spi_send;
	pxa910_add_fb(fb);
	pxa910_add_fb_ovly(ovly);
}
#endif
