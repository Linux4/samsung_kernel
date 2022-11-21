/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/mfp-pxa910.h>
#include <mach/pxa910.h>
#include <mach/irqs.h>

#include "../common.h"
#include <mach/pxa168fb.h>
/*
static unsigned long tpo_lcd_gpio_pin_config[] = {
	GPIO104_GPIO104,  // Data out
	GPIO106_GPIO106,  // Reset
	GPIO107_GPIO107,  // CS
	GPIO108_GPIO108,  // SCLK
};
*/

static unsigned long lcd_tpo_spi_pin_config[] = {
	GPIO104_LCD_SPIDOUT,
	GPIO105_LCD_SPIDIN,
	GPIO106_LCD_RESET,
	GPIO107_LCD_CS1,
	GPIO108_LCD_DCLK,
};

static int (*spi_send)(struct pxa168fb_info *, void *, int , unsigned int);

static u16 tpo_spi_cmdon[] = {
	0x0801,
	0x0800,
	0x0200,
	0x0304,
	0x040e,
	0x0903,
	0x0b18,
	0x0c53,
	0x0d01,
	0x0ee0,
	0x0f01,
	0x1058,
	0x201e,
	0x210a,
	0x220a,
	0x231e,
	0x2400,
	0x2532,
	0x2600,
	0x27ac,
	0x2904,
	0x2aa2,
	0x2b45,
	0x2c45,
	0x2d15,
	0x2e5a,
	0x2fff,
	0x306b,
	0x310d,
	0x3248,
	0x3382,
	0x34bd,
	0x35e7,
	0x3618,
	0x3794,
	0x3801,
	0x395d,
	0x3aae,
	0x3bff,
	0x07c9,	/* auto power on */
};

static u16 tpo_spi_cmdoff[] = {
	0x07d9,		/* auto power off */
};

/*
	 SPI emulated by GPIO for TPO LCD
	 CS GPIO107
	 DATA GPIO104
	 CLK  GPIO108
 */
/*
static int gpio_spi_send_tpolcd(u16 *dat, u32 size, u32 bit_len)
{
	u32 bit_cnt = bit_len;
	unsigned int clk_gpio, d_gpio;
	u16 *val = dat;
	unsigned int cs_gpio = mfp_to_gpio(MFP_PIN_GPIO107);
	int err = gpio_request(cs_gpio, "LCD CS");
	if (err) {
		pr_err("failed to request GPIO for TPO LCD CS\n");
		return -1;
	}
	gpio_direction_output(cs_gpio, 1);
	udelay(20);

	clk_gpio = mfp_to_gpio(MFP_PIN_GPIO108);
	err = gpio_request(clk_gpio, "LCD CLK");
	if (err) {
		pr_err("failed to request GPIO for TPO LCD CLK\n");
		return -1;
	}

	d_gpio = mfp_to_gpio(MFP_PIN_GPIO104);
	err = gpio_request(d_gpio, "LCD data");
	if (err) {
		pr_err("failed to request GPIO for TPO LCD Data\n");
		return -1;
	}

	for(; size; size --){
		bit_cnt = bit_len;
		gpio_direction_output(cs_gpio, 0);
		udelay(20);
		while(bit_cnt){
			bit_cnt --;
			if((*val >> bit_cnt) & 1){
				gpio_direction_output(d_gpio, 1);
			}else{
				gpio_direction_output(d_gpio, 0);
			}
			udelay(20);
			gpio_direction_output(clk_gpio, 0);
			udelay(20);
			gpio_direction_output(clk_gpio, 1);
			udelay(20);
		}
		val ++;
		udelay(20);
		gpio_direction_output(cs_gpio, 1);

	}
	gpio_free(cs_gpio);
	gpio_free(clk_gpio);
	gpio_free(d_gpio);
	return 0;
}
*/
static int tpo_lcd_power(struct pxa168fb_info *fbi, unsigned int spi_gpio_cs,
			 unsigned int spi_gpio_reset, int on)
{
	int err = 0;
	/* mfp_config(ARRAY_AND_SIZE(tpo_lcd_gpio_pin_config)); */

	mfp_config(ARRAY_AND_SIZE(lcd_tpo_spi_pin_config));
	/* power on the panel */
	if (on) {
		if (spi_gpio_reset != -1) {
			err = gpio_request(spi_gpio_reset, "TPO_LCD_SPI_RESET");
			if (err) {
				pr_err("failed to request GPIO for TPO LCD RESET\n");
				return -1;
			}
			gpio_direction_output(spi_gpio_reset, 0);
			msleep(100);
			gpio_set_value(spi_gpio_reset, 1);
			msleep(100);
			gpio_free(spi_gpio_reset);
		}
		/*err = gpio_spi_send_tpolcd(tpo_spi_cmdon,
			 ARRAY_SIZE(tpo_spi_cmdon), 16);*/
		err = spi_send(fbi, tpo_spi_cmdon, ARRAY_SIZE(tpo_spi_cmdon),
				 spi_gpio_cs);
		if (err) {
			pr_err("failed to send spi command on\n");
			return -1;
		}
	} else  {
		/*err = gpio_spi_send_tpolcd(tpo_spi_cmdoff,
			 ARRAY_SIZE(tpo_spi_cmdoff), 16);*/
		err = spi_send(fbi, tpo_spi_cmdoff, ARRAY_SIZE(tpo_spi_cmdoff),
				 spi_gpio_cs);
		if (err) {
			pr_err("failed to send spi command off\n");
			return -1;
		}

		err = gpio_request(spi_gpio_reset, "TPO_LCD_SPI_RESET");
		if (err) {
			pr_err("failed to request LCD RESET gpio\n");
			return -1;
		}
		gpio_set_value(spi_gpio_reset, 0);
		gpio_free(spi_gpio_reset);
	}
	return err;
}

static struct fb_videomode video_modes[] = {
	/* lpj032l001b HVGA mode info */
	[0] = {
		.pixclock       = 100000,
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

/* SPI Control Register. */
#define     CFG_SCLKCNT(div)     (div<<24)  /* 0xFF~0x2 */
#define     CFG_RXBITS(rx)       ((rx - 1)<<16)   /* 0x1F~0x1 */
 /* 0x1F~0x1, 0x1: 2bits ... 0x1F: 32bits */
#define     CFG_TXBITS(tx)       ((tx - 1)<<8)
#define     CFG_SPI_ENA(spi)     (spi<<3)
#define     CFG_SPI_SEL(spi)     (spi<<2)   /* 1: port1; 0: port0 */
#define     CFG_SPI_3W4WB(wire)  (wire<<1)  /* 1: 3-wire; 0: 4-wire */

static struct pxa168fb_mach_info dkb_tpo_lcd_info = {
	.id                     = "Base",
	.modes                  = video_modes,
	.sclk_div               = 0x4000001f,
	.isr_clear_mask		= LCD_ISR_CLEAR_MASK_PXA910,
	.num_modes              = ARRAY_SIZE(video_modes),
	.pix_fmt                = PIX_FMT_RGB565,
	.io_pad_ctrl		= PIN_MODE_DUMB_18_SPI | CFG_CYC_BURST_LEN8,
	.dumb_mode              = DUMB_MODE_RGB666,
	.active                 = 1,
	.spi_ctrl               = CFG_SCLKCNT(16) | CFG_TXBITS(16) |\
	 CFG_SPI_SEL(1) | CFG_SPI_3W4WB(1) | CFG_SPI_ENA(1),
	.spi_gpio_cs            = -1,
	.spi_gpio_reset	        = mfp_to_gpio(MFP_PIN_GPIO106),
	.mmap                   = 1,
	.panel_rbswap           = 1,
	.pxa168fb_lcd_power     = tpo_lcd_power,
	.invert_pixclock        = 1,
	.max_fb_size            = ALIGN(320, 4) * ALIGN(480 , 4) * 4 + 4096,
};

static struct pxa168fb_mach_info dkb_tpo_lcd_ovly_info = {
	.id                     = "Ovly",
	.modes                  = video_modes,
	.num_modes              = ARRAY_SIZE(video_modes),
	.pix_fmt                = PIX_FMT_RGB565,
	.io_pad_ctrl		= PIN_MODE_DUMB_18_SPI | CFG_CYC_BURST_LEN8,
	.dumb_mode              = DUMB_MODE_RGB666,
	.active                 = 1,
	.spi_ctrl               = CFG_SCLKCNT(16) | CFG_TXBITS(16) |\
	 CFG_SPI_SEL(1) | CFG_SPI_3W4WB(1) | CFG_SPI_ENA(1),
	.spi_gpio_cs            = -1,
	.spi_gpio_reset         = mfp_to_gpio(MFP_PIN_GPIO106),
	.mmap                   = 0,
	.panel_rbswap           = 1,
	.pxa168fb_lcd_power     = tpo_lcd_power,
	.invert_pixclock        = 1,
	.max_fb_size            = ALIGN(320, 4) * ALIGN(480 , 4) * 4 + 4096,
};

#ifdef CONFIG_MACH_TTC_DKB
#define SDRAM_CONFIG0_TYPE1 0X20
extern unsigned char __iomem *dmc_membase;
void __init dkb_add_lcd_tpo(void)
{
	unsigned int CSn_NO_COL;
	struct pxa168fb_mach_info *fb = &dkb_tpo_lcd_info,
				 *ovly = &dkb_tpo_lcd_ovly_info;

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
#endif	/* CONFIG_MACH_TTC_DKB */
