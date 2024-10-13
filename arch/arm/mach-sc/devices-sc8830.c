/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/ion.h>
#include <linux/input.h>
#include <linux/mmc/sprd-sdhci.h>
#include <linux/gpio.h>

#include <linux/sprd_cproc.h>
#include <linux/sipc.h>
#include <linux/spipe.h>
#include <linux/spool.h>
#include <linux/seth.h>
#include <linux/stty.h>
#include <sound/saudio.h>
#include <asm/pmu.h>
#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/irqs.h>
#include <mach/dma.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <linux/pstore_ram.h>
#include <linux/sprd_iommu.h>
#ifdef CONFIG_TS0710_MUX_ENABLE
#include <linux/sprdmux.h>
#endif
#if(defined(CONFIG_INPUT_SPRD_HEADSET_SHARK)||defined(CONFIG_INPUT_SPRD_HEADSET_SHARK_MODULE))
#include <linux/headset_sprd.h>
#endif
#ifdef CONFIG_BACKLIGHT_RT4502
#include <linux/rt4502_bl.h>
#endif

#include <linux/usb/gadget.h>
#include <video/ion_sprd.h>
#ifdef CONFIG_SPRD_VETH
#include <linux/sprd_veth.h>
#endif
#include "devices.h"
#include <mach/modem_interface.h>
#include <linux/sprd_thm.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <linux/sprd_cpu_cooling.h>

#ifndef CONFIG_OF
struct modem_intf_platform_data modem_interface = {
       .dev_type               = MODEM_DEV_SDIO,
       .modem_dev_parameter    = NULL,
       .modem_power_gpio       = 234,
       .modem_boot_gpio        = 236,
       .modem_watchdog_gpio    = 231,
       .modem_alive_gpio       = 230,
       .modem_crash_gpio       = 237,
       .modem_reset_gpio       = 233,
};
struct platform_device modem_interface_device = {
       .name   = "modem_interface",
       .id     = -1,
       .dev    = {
               .platform_data  = &modem_interface,
          },
};

static struct resource sprd_memnand_system_resources[] = {
	[0] = {
		.start = SPRD_MEMNAND_SYSTEM_BASE,
		.end = SPRD_MEMNAND_SYSTEM_BASE + SPRD_MEMNAND_SYSTEM_SIZE-1,
		.name = "goldfish_memnand",
		.flags = IORESOURCE_MEM,
	},
};
struct platform_device sprd_memnand_system_device  = {
	.name           = "goldfish_memnand",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_memnand_system_resources),
	.resource       = sprd_memnand_system_resources,
	.dev        = {.platform_data="system"},
};
static struct resource sprd_memnand_userdata_resources[] = {
	[0] = {
		.start = SPRD_MEMNAND_USERDATA_BASE,
		.end = SPRD_MEMNAND_USERDATA_BASE + SPRD_MEMNAND_USERDATA_SIZE-1,
		.name = "goldfish_memnand",
		.flags = IORESOURCE_MEM,
	},
};
struct platform_device sprd_memnand_userdata_device  = {
	.name           = "goldfish_memnand",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_memnand_userdata_resources),
	.resource       = sprd_memnand_userdata_resources,
	.dev        = {.platform_data="userdata"},
};



static struct resource sprd_memnand_cache_resources[] = {
	[0] = {
		.start = SPRD_MEMNAND_CACHE_BASE,
		.end = SPRD_MEMNAND_CACHE_BASE + SPRD_MEMNAND_CACHE_SIZE-1,
		.name = "goldfish_memnand",
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device sprd_memnand_cache_device  = {
	.name           = "goldfish_memnand",
	.id             =  2,
	.num_resources  = ARRAY_SIZE(sprd_memnand_cache_resources),
	.resource       = sprd_memnand_cache_resources,
	.dev        = {.platform_data="cache"},
};

static struct resource sprd_serial_resources0[] = {
	[0] = {
		.start = SPRD_UART0_BASE,
		.end = SPRD_UART0_BASE + SPRD_UART0_SIZE-1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER0_INT,
		.end = IRQ_SER0_INT,
		.flags = IORESOURCE_IRQ,
	},
#if 0
	[2] = {
			.start = SPRD_UART0_PHYS,
			.end = SPRD_UART0_PHYS + SPRD_UART0_SIZE-1,
			.name = "serial_phy_addr",
			.flags = IORESOURCE_MEM,
	},
    [3] = {
            .start = DMA_UART0_RX,
            .end = DMA_UART0_RX,
            .name = "serial_dma_rx_id",
            .flags = IORESOURCE_DMA,
    },
    [4] = {
            .start = DMA_UART0_TX,
            .end = DMA_UART0_TX,
            .name = "serial_dma_tx_id",
            .flags = IORESOURCE_DMA,
    },
#endif
};

struct platform_device sprd_serial_device0 = {
	.name           = "serial_sprd",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources0),
	.resource       = sprd_serial_resources0,
};

static struct resource sprd_serial_resources1[] = {
	[0] = {
		.start = SPRD_UART1_BASE,
		.end = SPRD_UART1_BASE + SPRD_UART1_SIZE-1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER1_INT,
		.end = IRQ_SER1_INT,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device sprd_serial_device1 = {
	.name           = "serial_sprd",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources1),
	.resource       = sprd_serial_resources1,
};

static struct resource sprd_serial_resources2[] = {
	[0] = {
		.start = SPRD_UART2_BASE,
		.end = SPRD_UART2_BASE + SPRD_UART2_SIZE - 1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER2_INT,
		.end = IRQ_SER2_INT,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device sprd_serial_device2 = {
	.name           = "serial_sprd",
	.id             =  2,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources2),
	.resource       = sprd_serial_resources2,
};

static struct resource sprd_serial_resources3[] = {
	[0] = {
		.start = SPRD_UART3_BASE,
		.end = SPRD_UART3_BASE + SPRD_UART3_SIZE - 1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER3_INT,
		.end = IRQ_SER3_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_serial_device3 = {
	.name           = "serial_sprd",
	.id             =  3,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources3),
	.resource       = sprd_serial_resources3,
};

static struct resource resources_rtc[] = {
	[0] = {
		.start	= IRQ_ANA_RTC_INT,
		.end	= IRQ_ANA_RTC_INT,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device sprd_device_rtc= {
	.name	= "sprd_rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_rtc),
	.resource	= resources_rtc,
};

static struct eic_gpio_resource sprd_gpio_resource[] = {
       [ENUM_ID_D_GPIO] = {
               .irq            = IRQ_GPIO_INT,
               .base_addr      = CTL_GPIO_BASE,
               .chip_base      = D_GPIO_START,
               .chip_ngpio     = D_GPIO_NR,
       },
       [ENUM_ID_D_EIC] = {
               .irq            = IRQ_EIC_INT,
               .base_addr      = CTL_EIC_BASE,
               .chip_base      = D_EIC_START,
               .chip_ngpio     = D_EIC_NR,
       },
       [ENUM_ID_A_GPIO] = {
               .irq            = IRQ_ANA_GPIO_INT,
               .base_addr      = ANA_CTL_GPIO_BASE,
               .chip_base      = A_GPIO_START,
               .chip_ngpio     = A_GPIO_NR,
       },
       [ENUM_ID_A_EIC] = {
               .irq            = IRQ_ANA_EIC_INT,
               .base_addr      = ANA_CTL_EIC_BASE,
               .chip_base      = A_EIC_START,
               .chip_ngpio     = A_EIC_NR,
       },
};

struct platform_device sprd_eic_gpio_device = {
       .name = "eic-gpio",
       .id = -1,
       .dev = { .platform_data = sprd_gpio_resource },
};

static struct resource sprd_nand_resources[] = {
	[0] = {
		.start	= 7,
		.end = 7,
		.flags	= IORESOURCE_DMA,
	},
	[1] = {
		.start	= SPRD_NFC_BASE,
		.end = SPRD_NFC_BASE + SPRD_NFC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device sprd_nand_device = {
	.name		= "sprd-nand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sprd_nand_resources),
	.resource	= sprd_nand_resources,
};

static struct resource sprd_hwspinlock_resources1[] = {
	[0] = {
		.start	= SPRD_HWLOCK1_BASE,
		.end = SPRD_HWLOCK1_BASE + SPRD_HWLOCK1_SIZE- 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sprd_hwspinlock_resources0[] = {
	[0] = {
		.start	= SPRD_HWLOCK0_BASE,
		.end = SPRD_HWLOCK0_BASE + SPRD_HWLOCK0_SIZE- 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device sprd_hwspinlock_device0 = {
	.name		= "sci_hwspinlock",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(sprd_hwspinlock_resources0),
	.resource	= sprd_hwspinlock_resources0,
};

struct platform_device sprd_hwspinlock_device1 = {
	.name		= "sci_hwspinlock",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(sprd_hwspinlock_resources1),
	.resource	= sprd_hwspinlock_resources1,
};

static struct resource sprd_lcd_resources[] = {
	[0] = {
		.start = SPRD_LCDC_BASE,
		.end = SPRD_LCDC_BASE + SPRD_LCDC_SIZE - 1,
		.name = "lcd_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_DISPC0_INT,
		.end = IRQ_DISPC0_INT,
		.flags = IORESOURCE_IRQ,
	},
	
	[2] = {
		.start = IRQ_DISPC1_INT,
		.end = IRQ_DISPC1_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_lcd_device0 = {
	.name           = "sprd_fb",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_lcd_resources),
	.resource       = sprd_lcd_resources,
};
struct platform_device sprd_lcd_device1 = {
	.name           = "sprd_fb",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_lcd_resources),
	.resource       = sprd_lcd_resources,
};
static struct resource sprd_otg_resource[] = {
	[0] = {
		.start = SPRD_USB_BASE,
		.end   = SPRD_USB_BASE + SPRD_USB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_USBD_INT,
		.end   = IRQ_USBD_INT,
		.flags = IORESOURCE_IRQ,
	}
};
static struct sprd_usb_platform_data sprd_otg_pdata = {
	.gpio_boost = 0xffffffff,
#ifdef  CONFIG_SHARK_PAD_HW_V102
	.gpio_chgdet = EIC_POWER_PBINT2,
#else
        .gpio_chgdet = EIC_CHARGER_DETECT,
#endif
	.gpio_otgdet = USB_OTG_CABLE_DETECT,

};

struct platform_device sprd_otg_device = {
	.name		= "dwc_otg",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(sprd_otg_resource),
	.resource	= sprd_otg_resource,
	.dev = { .platform_data = &sprd_otg_pdata },
};

/*if the backlight is driven by pwm, please config the pwm info
 *if the backlight is driven by PWMD, the pwm index is 3 as following
 */
#ifdef CONFIG_BACKLIGHT_KTD2801
struct platform_device sprd_backlight_device = {
	.name           = "sprd_backlight",
	.id             =  -1,
};

#elif defined(CONFIG_BACKLIGHT_RT4502)

struct platform_rt4502_backlight_data sprd_rt4502_backlight_data __attribute__((weak)) = {
	.max_brightness = 255,
	.dft_brightness = 160,
	.ctrl_pin = 190,
};

struct platform_device sprd_backlight_device __attribute__((weak)) = {
	.name           = "sprd_backlight",
	.id             =  -1,
	.dev	= {
		.platform_data	=  &sprd_rt4502_backlight_data,
	},
};


#else
struct resource sprd_bl_resource[] __attribute__((weak))= {
	[0] = {
#if defined(CONFIG_ARCH_SCX15)
		.start = 2,
		.end = 2,
#else
		.start = 3,
		.end = 3,
#endif
		.flags	= IORESOURCE_IO,
	},
};

struct platform_device sprd_backlight_device __attribute__((weak)) = {
	.name           = "sprd_backlight",
	.id             =  -1,
	.num_resources	= ARRAY_SIZE(sprd_bl_resource),
	.resource	= sprd_bl_resource,
};
#endif

#if(defined(CONFIG_BACKLIGHT_SPRD_PWM)||defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
struct platform_device sprd_pwm_bl_device = {
	.name = "sprd_pwm_bl",
	.id = -1,
};
#endif
#if(defined(CONFIG_SPRD_KPLED_2723) || defined(CONFIG_SPRD_KPLED_2723_MODULE))
struct platform_device sprd_kpled_2723_device = {
	.name = "sprd-kpled-2723",
	.id = -1,
};
#endif
static struct resource sprd_i2c_resources0[] = {
	[0] = {
		.start = SPRD_I2C0_BASE,
		.end   = SPRD_I2C0_BASE + SPRD_I2C0_SIZE -1,
		.name  = "i2c0_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C0_INT,
		.end   = IRQ_I2C0_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_i2c_device0 = {
	.name           = "sprd-i2c",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_i2c_resources0),
	.resource       = sprd_i2c_resources0,
};


static struct resource sprd_i2c_resources1[] = {
	[0] = {
		.start = SPRD_I2C1_BASE,
		.end   = SPRD_I2C1_BASE + SZ_4K -1,
		.name  = "i2c1_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C1_INT,
		.end   = IRQ_I2C1_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_i2c_device1 = {
	.name           = "sprd-i2c",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_i2c_resources1),
	.resource       = sprd_i2c_resources1,
};


static struct resource sprd_i2c_resources2[] = {
	[0] = {
		.start = SPRD_I2C2_BASE,
		.end   = SPRD_I2C2_BASE + SZ_4K -1,
		.name  = "i2c2_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C2_INT,
		.end   = IRQ_I2C2_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_i2c_device2 = {
	.name           = "sprd-i2c",
	.id             =  2,
	.num_resources  = ARRAY_SIZE(sprd_i2c_resources2),
	.resource       = sprd_i2c_resources2,
};


static struct resource sprd_i2c_resources3[] = {
	[0] = {
		.start = SPRD_I2C3_BASE,
		.end   = SPRD_I2C3_BASE + SZ_4K -1,
		.name  = "i2c3_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C3_INT,
		.end   = IRQ_I2C3_INT,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device sprd_i2c_device3 = {
	.name           = "sprd-i2c",
	.id             =  3,
	.num_resources  = ARRAY_SIZE(sprd_i2c_resources3),
	.resource       = sprd_i2c_resources3,
};

#if defined(CONFIG_PIN_POWER_DOMAIN_SWITCH)
#include <mach/pin_switch.h>
static struct sprd_pin_switch_platform_data sprd_pin_switch_data[PD_CNT] = {
	{"vdd28", 0x10, 0, 1},
	{"vdd28", 0x10, 1, 1},
	{"vddsim0", 0x10, 2, 1},
	{"vddsim1", 0x10, 3, 1},
	{"vddsim2", 0x10, 4, 1},
	{"vddsd", 0x10, 5, 1},
	{"vdd18", 0x10, 6, 1},
};

struct platform_device sprd_pin_switch_device = {
	.name = "pin_switch",
	.id = 0,
	.dev = {
		.platform_data	= sprd_pin_switch_data,
	},
};
#endif /*data and device of pin_switch for global pin powerdomain config(28nm only)*/


/* 8810 SPI devices.  */
static struct resource spi0_resources[] = {
    [0] = {
        .start = SPRD_SPI0_BASE,
        .end = SPRD_SPI0_BASE + SPRD_SPI0_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = IRQ_SPI0_INT,
        .end = IRQ_SPI0_INT,
        .flags = IORESOURCE_IRQ,
    },
};


static struct resource spi1_resources[] = {
    [0] = {
        .start = SPRD_SPI1_BASE,
        .end = SPRD_SPI1_BASE + SPRD_SPI1_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = IRQ_SPI1_INT,
        .end = IRQ_SPI1_INT,
        .flags = IORESOURCE_IRQ,
    },
};

static struct resource spi2_resources[] = {
	[0] = {
	       .start = SPRD_SPI2_BASE,
	       .end = SPRD_SPI2_BASE + SPRD_SPI2_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_SPI2_INT,
	       .end = IRQ_SPI2_INT,
	       .flags = IORESOURCE_IRQ,
	       },
};

struct platform_device sprd_spi0_device = {
    .name = "sprd spi",
    .id = 0,
    .resource = spi0_resources,
    .num_resources = ARRAY_SIZE(spi0_resources),
};

struct platform_device sprd_spi1_device = {
	.name = "sprd spi",
	.id = 1,
	.resource = spi1_resources,
	.num_resources = ARRAY_SIZE(spi1_resources),
};

struct platform_device sprd_spi2_device = {
	.name = "sprd spi",
	.id = 2,
	.resource = spi2_resources,
	.num_resources = ARRAY_SIZE(spi2_resources),
};

struct platform_device sprd_ahb_bm_device = {
	.name = "sprd_ahb_busmonitor",
	.id = 0,
};
struct platform_device sprd_axi_bm0_device = {
	.name = "sprd_axi_busmonitor",
	.id = 0,
};
static struct resource sci_keypad_resources[] = {
	{
	        .start = IRQ_KPD_INT,
	        .end = IRQ_KPD_INT,
	        .flags = IORESOURCE_IRQ,
	},
};

struct platform_device sprd_keypad_device = {
	.name = "sci-keypad",
	.id             = -1,
	.num_resources = ARRAY_SIZE(sci_keypad_resources),
	.resource = sci_keypad_resources,
};

static struct resource sprd_thm_resources[] = {
    [0] = {
        .start = SPRD_THM_BASE,
        .end = SPRD_THM_BASE + SPRD_THM_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = IRQ_THM_INT,
        .end = IRQ_THM_INT,
        .flags = IORESOURCE_IRQ,
    },
};

static struct sprd_thm_platform_data sprd_thm_data =
{
		.trip_points[0] = {
			.temp = 80,
			.lowoff = 72,
			.type = THERMAL_TRIP_ACTIVE,
			.cdev_name = {
				[0] = "thermal-cpufreq-0",
		},
		},
		.trip_points[1] = {
			.temp = 88,
			.lowoff = 72,
			.type = THERMAL_TRIP_ACTIVE,
			.cdev_name = {
				[0] = "thermal-cpufreq-0",
			},
		},
		.trip_points[2] = {
			.temp = 100,
			.lowoff = 81,
			.type = THERMAL_TRIP_ACTIVE,
			.cdev_name = {
				[0] = "thermal-cpufreq-0",
			},
		},
		.trip_points[3] = {
			.temp = 110,
			.type = THERMAL_TRIP_CRITICAL,
		},
		.num_trips = 4,

};

struct platform_device sprd_thm_device = {
	.name           = "sprd-thermal",
      .id		= 0,
	.resource       = sprd_thm_resources,
	.num_resources  = ARRAY_SIZE(sprd_thm_resources),
	.dev	= {
		.platform_data	= &sprd_thm_data,
	},
};

#ifdef CONFIG_CPU_FREQ_GOV_SPRDEMAND
static struct sprd_cpu_cooling_platform_data sprd_cpu_cooling_pdata = {
#ifdef CONFIG_ARCH_SCX35
	.cpu_state[0] = {
		.max_freq = 1200000,
		.max_core = 4,
	},
	.cpu_state[1] = {
		.max_freq = 1000000,
		.max_core = 4,
	},
	.cpu_state[2] = {
		.max_freq = 768000,
		.max_core = 4,
	},
	.cpu_state[3] = {
		.max_freq = 600000,
		.max_core = 1,
	},
	.state_num = 4,
#endif
#ifdef CONFIG_ARCH_SCX30G
	.cpu_state[0] = {
		.max_freq = 1200000,
		.max_core = 4,
	},
	.cpu_state[1] = {
		.max_freq = 1000000,
		.max_core = 4,
	},
	.cpu_state[2] = {
		.max_freq = 1000000,
		.max_core = 3,
	},
	.cpu_state[3] = {
		.max_freq = 1000000,
		.max_core = 1,
	},
	.state_num = 4,
#endif
};

struct platform_device sprd_cpu_cooling_device = {
	.name = "sprd-cpu-cooling",
	.id = -1,
	.dev = {
		.platform_data	= &sprd_cpu_cooling_pdata,
	},
};
#endif

#if 0
static struct resource sprd_thm_a_resources[] = {
    [0] = {
        .start = ANA_THM_BASE,
        .end = ANA_THM_BASE + SPRD_THM_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = IRQ_THM_INT,
        .end = IRQ_ANA_THM_OTP_INT,
        .flags = IORESOURCE_IRQ,
    },
};

static struct sprd_thm_platform_data sprd_thm_a_data = {
	.trip_points[0] = {
		.temp = 160,
		.type = THERMAL_TRIP_CRITICAL,
	},
	.num_trips = 1,
};

struct platform_device sprd_thm_a_device = {
	.name           = "sprd-thermal",
      .id		= 1,
	.resource       = sprd_thm_a_resources,
	.num_resources  = ARRAY_SIZE(sprd_thm_a_resources),
	.dev	= {
		.platform_data	= &sprd_thm_a_data,
	},
};
#endif

#if(defined(CONFIG_INPUT_SPRD_HEADSET_SHARK)||defined(CONFIG_INPUT_SPRD_HEADSET_SHARK_MODULE))
static struct headset_buttons sprd_headset_buttons[] = {
	{
		.adc_min = HEADSET_ADC_MIN_KEY_MEDIA,
		.adc_max = HEADSET_ADC_MAX_KEY_MEDIA,
		.code = KEY_MEDIA,
	},
#ifdef CONFIG_HEADSET_KEY_VOLUME_SUPPORTED
	{
		.adc_min = HEADSET_ADC_MIN_KEY_VOLUMEUP,
		.adc_max = HEADSET_ADC_MAX_KEY_VOLUMEUP,
		.code = KEY_VOLUMEUP,
	},
	{
		.adc_min = HEADSET_ADC_MIN_KEY_VOLUMEDOWN,
		.adc_max = HEADSET_ADC_MAX_KEY_VOLUMEDOWN,
		.code = KEY_VOLUMEDOWN,
	},
#endif
};

#ifdef CONFIG_HEADSET_EXTERNAL_HEADMICBIAS_POWER_SUPPORTED
static int kanas_headmicbias_power_on(int on)
{
	static int flag_gpio_request = 0;
	int ret = 0;

	on = ((0 != on) ? 1 : 0);

	if(0 == flag_gpio_request) {
		ret = gpio_request(HEADSET_EXT_MICBIAS_GPIO, "headset_ext_micbias");
		if (ret < 0) {
			printk(KERN_ERR "[SPRD_HEADSET_ERR] failed to request GPIO_%d(headset_ext_micbias)\n", HEADSET_EXT_MICBIAS_GPIO);
			return -1;
		} else {
			flag_gpio_request = 1;
			printk(KERN_INFO "[SPRD_HEADSET_INFO] request GPIO_%d(headset_ext_micbias) success\n", HEADSET_EXT_MICBIAS_GPIO);
		}
	}
	gpio_direction_output(HEADSET_EXT_MICBIAS_GPIO, on);
	printk(KERN_INFO "[SPRD_HEADSET_INFO] HEADSET_EXT_MICBIAS_GPIO(gpio_%d) = %d\n", HEADSET_EXT_MICBIAS_GPIO, gpio_get_value(HEADSET_EXT_MICBIAS_GPIO));
	return 0;
}
#endif

static struct sprd_headset_platform_data sprd_headset_pdata = {
	.gpio_switch = HEADSET_SWITCH_GPIO,
	.gpio_detect = HEADSET_DETECT_GPIO,
	.gpio_button = HEADSET_BUTTON_GPIO,
	.irq_trigger_level_detect = HEADSET_IRQ_TRIGGER_LEVEL_DETECT,
	.irq_trigger_level_button = HEADSET_IRQ_TRIGGER_LEVEL_BUTTON,
	.adc_threshold_3pole_detect = HEADSET_ADC_THRESHOLD_3POLE_DETECT,
	.adc_threshold_4pole_detect = HEADSET_ADC_THRESHOLD_4POLE_DETECT,
	.irq_threshold_buttont = HEADSET_IRQ_THRESHOLD_BUTTON,
	.voltage_headmicbias = HEADSET_HEADMICBIAS_VOLTAGE,
	.headset_buttons = sprd_headset_buttons,
	.nbuttons = ARRAY_SIZE(sprd_headset_buttons),
#ifdef CONFIG_HEADSET_EXTERNAL_HEADMICBIAS_POWER_SUPPORTED
	.external_headmicbias_power_on = kanas_headmicbias_power_on,
#else
	.external_headmicbias_power_on = NULL,
#endif
};
struct platform_device sprd_headset_device = {
	.name = "headset-detect",
	.id = -1,
	.dev = {
		.platform_data = &sprd_headset_pdata,
	},
};
#endif

#if(defined(CONFIG_INPUT_HEADSET_SPRD_SC2723)||defined(CONFIG_INPUT_HEADSET_SPRD_SC2723_MODULE))
struct platform_device headset_sprd_sc2723_device = {
	.name = "headset_sprd_sc2723",
	.id = -1,
};
#endif

#ifdef CONFIG_RF_SHARK
struct platform_device trout_fm_device = {
	.name = "trout_fm",
	.id = -1,
};
#endif

#if 0   //move to board_common_battery.c
static struct resource sprd_battery_resources[] = {
        [0] = {
                .start = EIC_CHARGER_DETECT,
                .end = EIC_CHARGER_DETECT,
                .name = "charger_detect",
                .flags = IORESOURCE_IO,
        },
        [1] = {
                .start = EIC_CHG_CV_STATE,
                .end = EIC_CHG_CV_STATE,
                .name = "chg_cv_state",
                .flags = IORESOURCE_IO,
        },
        [2] = {
                .start = EIC_VCHG_OVI,
                .end = EIC_VCHG_OVI,
                .name = "vchg_ovi",
                .flags = IORESOURCE_IO,
        },
#ifdef CONFIG_SHARK_PAD_HW_V102
        [3] = {
                .start = GPIO_EXT_CHG_DONE,
                .end = GPIO_EXT_CHG_DONE,
                .name = "ext_charge_done",
                .flags = IORESOURCE_IO,
        },
        [4] = {
                .start = GPIO_EXT_CHG_EN,
                .end = GPIO_EXT_CHG_EN,
                .name = "ext_chg_en",
                .flags = IORESOURCE_IO,
        },
        [5] = {
                .start = GPIO_EXT_CHG_OVI,
                .end = GPIO_EXT_CHG_OVI,
                .name = "ext_vchg_ovi",
                .flags = IORESOURCE_IO,
        }
#endif
};

struct platform_device sprd_battery_device = {
        .name           = "sprd-battery",
        .id             =  0,
        .num_resources  = ARRAY_SIZE(sprd_battery_resources),
        .resource       = sprd_battery_resources,
};
#endif

#if defined(CONFIG_SPRD_IOMMU)
#if defined(CONFIG_ARCH_SCX15)
static struct sprd_iommu_init_data sprd_iommu_gsp_data = {
	.id=0,
	.name="sprd_iommu_gsp",
	.iova_base=0x10000000,
	.iova_size=0x1000000,
	.pgt_base=SPRD_GSPMMU_BASE,
	.pgt_size=0x4000,
	.ctrl_reg=SPRD_GSPMMU_BASE+0x4000,
};

struct platform_device sprd_iommu_gsp_device = {
	.name = "sprd_iommu",
	.id = 0,
	.dev = {.platform_data = &sprd_iommu_gsp_data },
};

static struct sprd_iommu_init_data sprd_iommu_mm_data = {
	.id=1,
	.name="sprd_iommu_mm",
	.iova_base=0x20000000,
	.iova_size=0x4000000,
	.pgt_base=SPRD_MMMMU_BASE,
	.pgt_size=0x10000,
	.ctrl_reg=SPRD_MMMMU_BASE+0x10000,
};

struct platform_device sprd_iommu_mm_device = {
	.name = "sprd_iommu",
	.id = 1,
	.dev = {.platform_data = &sprd_iommu_mm_data },
};
#elif defined(CONFIG_ARCH_SCX30G)
static struct sprd_iommu_init_data sprd_iommu_gsp_data = {
	.id=0,
	.name="sprd_iommu_gsp",
	.iova_base=0x10000000,
	.iova_size=0x2000000,
	.pgt_base=SPRD_GSPMMU_BASE,
	.pgt_size=0x8000,
	.ctrl_reg=SPRD_GSPMMU_BASE+0x8000,
};

struct platform_device sprd_iommu_gsp_device = {
	.name = "sprd_iommu",
	.id = 0,
	.dev = {.platform_data = &sprd_iommu_gsp_data },
};

static struct sprd_iommu_init_data sprd_iommu_mm_data = {
	.id=1,
	.name="sprd_iommu_mm",
	.iova_base=0x20000000,
	.iova_size=0x8000000,
	.pgt_base=SPRD_MMMMU_BASE,
	.pgt_size=0x20000,
	.ctrl_reg=SPRD_MMMMU_BASE+0x20000,
};

struct platform_device sprd_iommu_mm_device = {
	.name = "sprd_iommu",
	.id = 1,
	.dev = {.platform_data = &sprd_iommu_mm_data },
};
#endif
#endif

static struct resource sprd_dcam_resources[] = {
	{
		.start	= SPRD_ISP_BASE,
		.end	= SPRD_ISP_BASE + SPRD_ISP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_ISP_INT,
		.end	= IRQ_ISP_INT,
		.flags	= IORESOURCE_IRQ,
	},
};
struct platform_device sprd_dcam_device = {
	.name		= "sprd_dcam",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(sprd_dcam_resources),
	.resource	= sprd_dcam_resources,
};
struct platform_device sprd_scale_device = {
	.name	= "sprd_scale",
	.id	= -1,
};

struct platform_device sprd_gsp_device =
{
    .name	= "sprd_gsp",
    .id	= -1,
};
struct platform_device sprd_rotation_device = {
	.name	= "sprd_rotation",
	.id	= -1,
};

struct platform_device sprd_sensor_device = {
	.name	= "sprd_sensor",
	.id	= -1,
};
struct platform_device sprd_isp_device = {
	.name = "sprd_isp",
	.id = -1,
};

struct platform_device sprd_dma_copy_device = {
	.name	= "sprd_dma_copy",
	.id	= -1,
};
static struct resource sprd_sdio0_resources[] = {
	[0] = {
		.start = SPRD_SDIO0_BASE,
		.end = SPRD_SDIO0_BASE + SPRD_SDIO0_SIZE-1,
		.name = "sdio0_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDIO0_INT,
		.end = IRQ_SDIO0_INT,
		.flags = IORESOURCE_IRQ,
	}
};

static struct sprd_sdhci_host_platdata sprd_sdio0_pdata = {
	.caps = MMC_CAP_HW_RESET | MMC_CAP_4_BIT_DATA,
#if defined(CONFIG_MACH_SC9620OPENPHONE) || defined(CONFIG_MACH_SC9620OPENPHONE_ZT)
	.caps2 = MMC_CAP2_HC_ERASE_SZ | MMC_CAP2_CACHE_CTRL | MMC_CAP2_CD_ACTIVE_HIGH,
#else
	.caps2 = MMC_CAP2_HC_ERASE_SZ | MMC_CAP2_CACHE_CTRL,
#endif
	.vdd_extmmc = "vddsd",
#ifdef CONFIG_ARCH_SCX15
	.detect_gpio = 193,
#else
	.detect_gpio = 71,
#endif
	.clk_name = "clk_sdio0",
#ifdef CONFIG_ARCH_SCX15
		.clk_parent_name = "clk_384m",
		.max_frequency = 384000000,
#elif defined(CONFIG_ARCH_SCX30G)
		.clk_parent_name = "clk_384m",
		.max_frequency = 384000000,
#else
		.clk_parent_name = "clk_192m",
		.max_frequency = 192000000,
#endif
#ifndef CONFIG_ARCH_SCX15
	.pinmap_offset = SPRD_PIN_SDIO0_OFFSET,
	.d3_gpio = SPRD_PIN_SDIO0_D3_GPIO,
	.d3_index = SPRD_PIN_SDIO0_D3_INDEX,
	.sd_func = SPRD_PIN_SDIO0_SD_FUNC,
	.gpio_func = SPRD_PIN_SDIO0_GPIO_FUNC,
#endif
	.init_voltage_level = 4,
	.enb_bit = BIT_SDIO0_EB,
	.rst_bit = BIT_SDIO0_SOFT_RST,//FIXME:
	.runtime = 1,
};

struct platform_device sprd_sdio0_device = {
#ifdef CONFIG_ARCH_SCX15
	.name = "sprd-sdhci-dolphin",
#else
	.name = "sprd-sdhci-shark",
#endif
	.id             =  SDC_SLAVE_SD,
	.num_resources  = ARRAY_SIZE(sprd_sdio0_resources),
	.resource       = sprd_sdio0_resources,
	.dev = {
		.platform_data = &sprd_sdio0_pdata,
		.init_name = "sprd-sdhci.0",
	},
};

static struct resource sprd_sdio1_resources[] = {
	[0] = {
		.start = SPRD_SDIO1_BASE,
		.end = SPRD_SDIO1_BASE + SPRD_SDIO1_SIZE-1,
		.name = "sdio1_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDIO1_INT,
		.end = IRQ_SDIO1_INT,
		.flags = IORESOURCE_IRQ,
	}
};

static struct sprd_sdhci_host_platdata sprd_sdio1_pdata = {
	.caps = MMC_CAP_HW_RESET | MMC_CAP_POWER_OFF_CARD | MMC_CAP_4_BIT_DATA,
	.clk_name = "clk_sdio1",
	.clk_parent_name = "clk_96m",
	.max_frequency = 96000000,
	.enb_bit = BIT_SDIO1_EB,
	.rst_bit = BIT_SDIO1_SOFT_RST,
	.runtime = 1,
};

struct platform_device sprd_sdio1_device = {
#ifdef CONFIG_ARCH_SCX15
	.name = "sprd-sdhci-dolphin",
#else
	.name = "sprd-sdhci-shark",
#endif
	.id             =  SDC_SLAVE_WIFI,
	.num_resources  = ARRAY_SIZE(sprd_sdio1_resources),
	.resource       = sprd_sdio1_resources,
	.dev = {
		.platform_data = &sprd_sdio1_pdata,
		.init_name = "sprd-sdhci.1",
	},
};

struct platform_device sprd_vsp_device = {
	.name	= "sprd_vsp",
	.id	= -1,
};

struct platform_device sprd_jpg_device = {
	.name	= "sprd_jpg",
	.id	= -1,
};

#ifdef CONFIG_ION
static struct ion_platform_heap ion_pheaps[] = {
                {
                        .id     = ION_HEAP_ID_SYSTEM,
                        .type   = ION_HEAP_TYPE_SYSTEM,
                        .name   = "ion_heap_system",
                },
                {
                        .id     = ION_HEAP_ID_MM,
#ifdef CONFIG_SPRD_IOMMU
                        .type   = ION_HEAP_TYPE_SYSTEM,
                        .name   = "ion_heap_system_mm",
#elif defined(CONFIG_CMA)
                        .type   = ION_HEAP_TYPE_CUSTOM,
                        .name   = "ion_heap_cma_mm",
#else
                        .type   = ION_HEAP_TYPE_CARVEOUT,
                        .name   = "ion_heap_carveout_mm",
                      #ifndef SPRD_ION_BASE_USE_VARIABLE
                        .base   = SPRD_ION_MM_BASE,
                      #else
                        /*-2 mean will value SPRD_ION_MM_BASE*/
                        .base   = -2,
                      #endif
                        .size   = SPRD_ION_MM_SIZE,
#endif
                },
                {
                        .id     = ION_HEAP_ID_OVERLAY,
#ifdef CONFIG_CMA
                        .type   = ION_HEAP_TYPE_CUSTOM,
                        .name   = "ion_heap_cma_overlay",
#else
                        .type   = ION_HEAP_TYPE_CARVEOUT,
                        .name   = "ion_heap_carveout_overlay",
                      #ifndef SPRD_ION_BASE_USE_VARIABLE
                        .base   = SPRD_ION_OVERLAY_BASE,
                      #else
                        /*-1 mean will value SPRD_ION_OVERLAY_BASE*/
                        .base   = -1,
                      #endif
                        .size   = SPRD_ION_OVERLAY_SIZE,
#endif
                },
};

static struct ion_platform_data ion_pdata = {
        .nr = sizeof(ion_pheaps)/sizeof(ion_pheaps[0]),
        .heaps = &ion_pheaps[0],
};

struct platform_device sprd_ion_dev = {
        .name = "ion-sprd",
        .id = -1,
        .dev = { .platform_data = &ion_pdata },
};

#ifdef SPRD_ION_BASE_USE_VARIABLE
void init_ion_addr_param(void)
{
	int i;
	for(i=0;i<sizeof(ion_pheaps)/sizeof(ion_pheaps[0]);i++)
	{
	    if(ion_pheaps[i].base == -1)
	    {
	        ion_pheaps[i].base = SPRD_ION_OVERLAY_BASE;
	    }
	    else if(ion_pheaps[i].base == -2)
	    {
	        ion_pheaps[i].base = SPRD_ION_MM_BASE;
	    }
	    printk("xxx:ion_pheaps[%d].base=%08x\n",i,ion_pheaps[i].base);
	}
}
#endif
#endif


#ifdef CONFIG_MUX_SDIO_OPT1_HAL

struct platform_device ipc_sdio_device = {
	.name = "ipc_sdio",
	.id = 0,
};

#endif

static struct resource sprd_sdio2_resources[] = {
	[0] = {
	       .start = SPRD_SDIO2_BASE,
	       .end = SPRD_SDIO2_BASE + SPRD_SDIO2_SIZE - 1,
	       .name = "sdio2_res",
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_SDIO2_INT,
	       .end = IRQ_SDIO2_INT,
	       .flags = IORESOURCE_IRQ,
	       }
};

static struct sprd_sdhci_host_platdata sprd_sdio2_pdata = {
	.caps = MMC_CAP_HW_RESET | MMC_CAP_POWER_OFF_CARD | MMC_CAP_4_BIT_DATA | MMC_CAP_1_8V_DDR,
	.detect_gpio = GPIO_SDIO_DETECT,
	.clk_name = "clk_sdio2",
	.clk_parent_name = "clk_96m",
	.max_frequency = 96000000,
	.enb_bit = BIT_SDIO2_EB,
	.rst_bit = BIT_SDIO2_SOFT_RST,
	.keep_power = 1,
	.runtime = 1,
};

struct platform_device sprd_sdio2_device = {
#ifdef CONFIG_ARCH_SCX15
	.name = "sprd-sdhci-dolphin",
#else
	.name = "sprd-sdhci-shark",
#endif
	.id = SDC_SLAVE_CP,
	.num_resources = ARRAY_SIZE(sprd_sdio2_resources),
	.resource = sprd_sdio2_resources,
	.dev = {
		.platform_data = &sprd_sdio2_pdata,
		.init_name = "sprd-sdhci.2",
	},
};

static struct resource sprd_emmc_resources[] = {
	[0] = {
	       .start = SPRD_EMMC_BASE,
	       .end = SPRD_EMMC_BASE + SPRD_EMMC_SIZE - 1,
	       .name = "sdio3_res",
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_EMMC_INT,
	       .end = IRQ_EMMC_INT,
	       .flags = IORESOURCE_IRQ,
	       }
};

static struct sprd_sdhci_host_platdata sprd_emmc_pdata = {
	.caps = MMC_CAP_HW_RESET | MMC_CAP_NONREMOVABLE | MMC_CAP_8_BIT_DATA,// | MMC_CAP_1_8V_DDR,
	.caps2 = MMC_CAP2_HC_ERASE_SZ | MMC_CAP2_CACHE_CTRL,
	.vdd_extmmc = "vddemmcio",
	.clk_name = "clk_emmc",
#ifdef CONFIG_ARCH_SCX15
	.clk_parent_name = "clk_384m",
	.max_frequency = 384000000,
#else
	.clk_parent_name = "clk_192m",
	.max_frequency = 192000000,
#endif
	.enb_bit = BIT_EMMC_EB,
	.rst_bit = BIT_EMMC_SOFT_RST,
	.write_delay = 0x20,
	.read_pos_delay = 0x07,
	.read_neg_delay = 0x05,
	.runtime = 1,
};

struct platform_device sprd_emmc_device = {
#ifdef CONFIG_ARCH_SCX15
	.name = "sprd-sdhci-dolphin",
#else
	.name = "sprd-sdhci-shark",
#endif
	.id = SDC_SLAVE_EMMC,
	.num_resources = ARRAY_SIZE(sprd_emmc_resources),
	.resource = sprd_emmc_resources,
	.dev = {
		.platform_data = &sprd_emmc_pdata,
		.init_name = "sprd-sdhci.3",
	},
};
#endif
struct sysdump_mem sprd_dump_mem[] = {
#if defined(CONFIG_ARCH_SCX15)
#if defined(CONFIG_SPRD_MODEM_TD)
	{
		.paddr		= CONFIG_PHYS_OFFSET,
		.vaddr		= PAGE_OFFSET,
		.soff		= 0xffffffff,
		.size		= CPT_START_ADDR - CONFIG_PHYS_OFFSET,
		.type	 	= SYSDUMP_RAM,
	},
	{
		.paddr		= CPT_START_ADDR,
		.vaddr		= PAGE_OFFSET +
					(CPT_START_ADDR - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= CPT_TOTAL_SIZE,
#ifdef CONFIG_SIPC_TD
		.type		= SYSDUMP_MODEM,
#else
		.type		= SYSDUMP_RAM,
#endif
	},
	{
		.paddr		= CPT_START_ADDR + CPT_TOTAL_SIZE,
		.vaddr		= PAGE_OFFSET + CPT_START_ADDR + CPT_TOTAL_SIZE - CONFIG_PHYS_OFFSET,
		.soff		= 0xffffffff,
		.size		= WCN_START_ADDR - CPT_START_ADDR - CPT_TOTAL_SIZE,
		.type	 	= SYSDUMP_RAM,
	},
#else
	{
		.paddr		= CONFIG_PHYS_OFFSET,
		.vaddr		= PAGE_OFFSET,
		.soff		= 0xffffffff,
		.size		= CPW_START_ADDR - CONFIG_PHYS_OFFSET,
		.type	 	= SYSDUMP_RAM,
	},
	{
		.paddr		= CPW_START_ADDR,
		.vaddr		= PAGE_OFFSET +
					(CPW_START_ADDR - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= CPW_TOTAL_SIZE,
#ifdef CONFIG_SIPC_WCDMA
		.type		= SYSDUMP_MODEM,
#else
		.type		= SYSDUMP_RAM,
#endif
	},
	{
		.paddr		= CPW_START_ADDR + CPW_TOTAL_SIZE,
		.vaddr		= PAGE_OFFSET + CPW_START_ADDR + CPW_TOTAL_SIZE - CONFIG_PHYS_OFFSET,
		.soff		= 0xffffffff,
		.size		= WCN_START_ADDR - CPW_START_ADDR - CPW_TOTAL_SIZE,
		.type	 	= SYSDUMP_RAM,
	},
#endif
#elif defined(CONFIG_ARCH_SCX30G)
#if defined(CONFIG_SPRD_MODEM_TD)
	{
		.paddr		= CONFIG_PHYS_OFFSET,
		.vaddr		= PAGE_OFFSET,
		.soff		= 0xffffffff,
		.size		= CPT_START_ADDR - CONFIG_PHYS_OFFSET,
		.type	 	= SYSDUMP_RAM,
	},


	{
		.paddr		= CPT_START_ADDR,
		.vaddr		= PAGE_OFFSET +
					(CPT_START_ADDR - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= CPT_TOTAL_SIZE,
#ifdef CONFIG_SIPC_TD
		.type		= SYSDUMP_MODEM,
#else
		.type		= SYSDUMP_RAM,
#endif
	},
#else
	{
		.paddr		= CONFIG_PHYS_OFFSET,
		.vaddr		= PAGE_OFFSET,
		.soff		= 0xffffffff,
		.size		= CPW_START_ADDR - CONFIG_PHYS_OFFSET,
		.type		= SYSDUMP_RAM,
	},

	{
		.paddr		= CPW_START_ADDR,
		.vaddr		= PAGE_OFFSET +
					(CPW_START_ADDR - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= CPW_TOTAL_SIZE,
#ifdef CONFIG_SIPC_WCDMA
		.type		= SYSDUMP_MODEM,
#else
		.type		= SYSDUMP_RAM,
#endif
	},
#endif

#else

	{
		.paddr		= CONFIG_PHYS_OFFSET,
		.vaddr		= PAGE_OFFSET,
		.soff		= 0xffffffff,
		.size		= CPT_START_ADDR - CONFIG_PHYS_OFFSET,
		.type	 	= SYSDUMP_RAM,
	},
	{
		.paddr		= CPT_START_ADDR,
		.vaddr		= PAGE_OFFSET +
					(CPT_START_ADDR - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= CPT_TOTAL_SIZE,
#ifdef CONFIG_SIPC_TD
		.type		= SYSDUMP_MODEM,
#else
		.type		= SYSDUMP_RAM,
#endif
	},
	{
		.paddr		= CPT_START_ADDR + CPT_TOTAL_SIZE,
		.vaddr		= PAGE_OFFSET +
					(CPT_START_ADDR + CPT_TOTAL_SIZE - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= CPW_START_ADDR - (CPT_START_ADDR + CPT_TOTAL_SIZE),
		.type		= SYSDUMP_RAM,
	},
	{
		.paddr		= CPW_START_ADDR,
		.vaddr		= PAGE_OFFSET +
					(CPW_START_ADDR - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= CPW_TOTAL_SIZE,
#ifdef CONFIG_SIPC_WCDMA
		.type		= SYSDUMP_MODEM,
#else
		.type		= SYSDUMP_RAM,
#endif
	},
#endif
	{
		.paddr		= WCN_START_ADDR,
		.vaddr		= PAGE_OFFSET +
					(WCN_START_ADDR - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= WCN_TOTAL_SIZE,
//#ifdef CONFIG_SIPC_WCN
		.type		= SYSDUMP_MODEM,
//#else
//		.type		= SYSDUMP_RAM,
//#endif
	},
	{
		.paddr		= WCN_START_ADDR + WCN_TOTAL_SIZE,
		.vaddr		= PAGE_OFFSET +
					(WCN_START_ADDR + WCN_TOTAL_SIZE - CONFIG_PHYS_OFFSET),
		.soff		= 0xffffffff,
		.size		= 0, /* fill this dynamically according to real ram size */
		.type		= SYSDUMP_RAM,
	},
	{
		.paddr		= SPRD_AHB_PHYS,
		.vaddr		= SPRD_AHB_BASE,
		.soff		= 0x0,
		.size		= SPRD_AHB_SIZE,
		.type		= SYSDUMP_IOMEM,
	},
	{
		.paddr		= SPRD_INTC0_PHYS,
		.vaddr		= SPRD_INTC0_BASE,
		.soff		= 0x0,
		.size		= SPRD_INTC0_SIZE,
		.type		= SYSDUMP_IOMEM,
	},
	{
		.paddr		= SPRD_GPTIMER0_PHYS,
		.vaddr		= SPRD_GPTIMER0_BASE,
		.soff		= 0x0,
		.size		= SPRD_GPTIMER0_SIZE,
		.type		= SYSDUMP_IOMEM,
	},
	{
		.paddr		= SPRD_ADI_PHYS,
		.vaddr		= SPRD_ADI_BASE,
		.soff		= 0x0,
		.size		= SPRD_ADI_SIZE,
		.type		= SYSDUMP_IOMEM,
	},
	{
		.paddr		= SPRD_GPIO_PHYS,
		.vaddr		= SPRD_GPIO_BASE,
		.soff		= 0x0,
		.size		= SPRD_GPIO_SIZE,
		.type		= SYSDUMP_IOMEM,
	},
	{
		.paddr		= SPRD_EIC_PHYS,
		.vaddr		= SPRD_EIC_BASE,
		.soff		= 0x0,
		.size		= SPRD_EIC_SIZE,
		.type		= SYSDUMP_IOMEM,
	},
	{
		.paddr		= SPRD_GREG_PHYS,
		.vaddr		= SPRD_GREG_BASE,
		.soff		= 0x0,
		.size		= SPRD_GREG_SIZE,
		.type		= SYSDUMP_IOMEM,
	},
};
int sprd_dump_mem_num = ARRAY_SIZE(sprd_dump_mem);
#ifndef CONFIG_OF
static struct resource sprd_pmu_resource[] = {
	[0] = {
		.start		= IRQ_NPMUIRQ0_INT,
		.end		= IRQ_NPMUIRQ0_INT,
		.flags		= IORESOURCE_IRQ,
	},
	[1] = {
		.start		= IRQ_NPMUIRQ1_INT,
		.end		= IRQ_NPMUIRQ1_INT,
		.flags		= IORESOURCE_IRQ,
	},
	[2] = {
		.start		= IRQ_NPMUIRQ2_INT,
		.end		= IRQ_NPMUIRQ2_INT,
		.flags		= IORESOURCE_IRQ,
	},
	[3] = {
		.start		= IRQ_NPMUIRQ3_INT,
		.end		= IRQ_NPMUIRQ3_INT,
		.flags		= IORESOURCE_IRQ,
	},
};

struct platform_device sprd_a7_pmu_device = {
	.name		= "arm-pmu",
	.id		= -1,
	.resource = sprd_pmu_resource,
	.num_resources	= ARRAY_SIZE(sprd_pmu_resource),
};

#ifdef CONFIG_PSTORE_RAM
static struct ramoops_platform_data ramoops_data = {
	.mem_size               = SPRD_RAM_CONSOLE_SIZE,
  #ifndef SPRD_ION_BASE_USE_VARIABLE
	.mem_address			= SPRD_RAM_CONSOLE_START,
  #endif
	.console_size			= SPRD_RAM_CONSOLE_SIZE,
	.dump_oops              = 0,
	.ecc_info               = {
								.ecc_size = 16
	},
};

struct platform_device sprd_ramoops_device = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ramoops_data,
	},
};
#ifdef SPRD_ION_BASE_USE_VARIABLE
void init_pstore_addr_param(void)
{
	ramoops_data.mem_address = SPRD_RAM_CONSOLE_START;
}
#endif
#endif

struct platform_device sprd_audio_vbc_r2p0_sprd_codec_v3_device = {
	.name		= "vbc-r2p0-sprd-codec-v3",
	.id		= -1,
};

struct platform_device sprd_audio_vbc_r2p0_sprd_codec_device = {
	.name		= "vbc-r2p0-sprd-codec",
	.id		= -1,
};

struct platform_device sprd_audio_i2s_null_codec_device = {
	.name		= "i2s-null-codec",
	.id		= -1,
};

struct platform_device sprd_audio_platform_pcm_device = {
	.name		= "sprd-pcm-audio",
	.id		= -1,
};

struct platform_device sprd_audio_vaudio_device = {
	.name		= "vaudio",
	.id		= -1,
};

struct platform_device sprd_audio_vbc_r2p0_device = {
	.name		= "vbc-r2p0",
	.id		= -1,
};

static struct resource sprd_i2s0_resources[] = {
        [0] = {
                .start = SPRD_IIS0_BASE,
                .end   = SPRD_IIS0_BASE + SZ_4K -1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = SPRD_IIS0_PHYS,
                .end   = SPRD_IIS0_PHYS + SZ_4K -1,
                .flags = IORESOURCE_MEM,
        },
        [2] = {
                .start = DMA_IIS_TX,
                .end   = DMA_IIS_RX,
                .flags = IORESOURCE_DMA,
        }
};

static struct resource sprd_i2s1_resources[] = {
        [0] = {
                .start = SPRD_IIS1_BASE,
                .end   = SPRD_IIS1_BASE + SZ_4K -1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = SPRD_IIS1_PHYS,
                .end   = SPRD_IIS1_PHYS + SZ_4K -1,
                .flags = IORESOURCE_MEM,
        },
        [2] = {
                .start = DMA_IIS1_TX,
                .end   = DMA_IIS1_RX,
                .flags = IORESOURCE_DMA,
        }
};

static struct resource sprd_i2s2_resources[] = {
        [0] = {
                .start = SPRD_IIS2_BASE,
                .end   = SPRD_IIS2_BASE + SZ_4K -1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = SPRD_IIS2_PHYS,
                .end   = SPRD_IIS2_PHYS + SZ_4K -1,
                .flags = IORESOURCE_MEM,
        },
        [2] = {
                .start = DMA_IIS2_TX,
                .end   = DMA_IIS2_RX,
                .flags = IORESOURCE_DMA,
        }
};

static struct resource sprd_i2s3_resources[] = {
        [0] = {
                .start = SPRD_IIS3_BASE,
                .end   = SPRD_IIS3_BASE + SZ_4K -1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = SPRD_IIS3_PHYS,
                .end   = SPRD_IIS3_PHYS + SZ_4K -1,
                .flags = IORESOURCE_MEM,
        },
        [2] = {
                .start = DMA_IIS3_TX,
                .end   = DMA_IIS3_RX,
                .flags = IORESOURCE_DMA,
        }
};

struct platform_device sprd_audio_i2s0_device = {
	.name		= "i2s",
	.id		= 0,
	.num_resources  = ARRAY_SIZE(sprd_i2s0_resources),
	.resource       = sprd_i2s0_resources,
};

struct platform_device sprd_audio_i2s1_device = {
	.name		= "i2s",
	.id		= 1,
	.num_resources  = ARRAY_SIZE(sprd_i2s1_resources),
	.resource       = sprd_i2s1_resources,
};

struct platform_device sprd_audio_i2s2_device = {
	.name		= "i2s",
	.id		= 2,
	.num_resources  = ARRAY_SIZE(sprd_i2s2_resources),
	.resource       = sprd_i2s2_resources,
};

struct platform_device sprd_audio_i2s3_device = {
	.name		= "i2s",
	.id		= 3,
	.num_resources  = ARRAY_SIZE(sprd_i2s3_resources),
	.resource       = sprd_i2s3_resources,
};

struct platform_device sprd_audio_sprd_codec_device = {
	.name		= "sprd-codec",
	.id		= -1,
};

struct platform_device sprd_audio_sprd_codec_v3_device = {
	.name		= "sprd-codec-v3",
	.id		= -1,
};

struct platform_device sprd_audio_null_codec_device = {
	.name		= "null-codec",
	.id		= -1,
};

#ifdef CONFIG_SPRD_VETH

#ifdef CONFIG_MUX_SPI_HAL
static struct veth_init_data sprd_veth_spi0_pdata = {
	.index 		= 13,
	.inst_id 	= SPRDMUX_ID_SPI,
	.dev_id 	= 0,
	.name 		= "veth_spi0",
};
struct platform_device sprd_veth_spi0_device = {
	.name 	= "sprd_veth",
	.id 	= 0,
	.dev = {.platform_data = &sprd_veth_spi0_pdata},
};

static struct veth_init_data sprd_veth_spi1_pdata = {
	.index 		= 14,
	.inst_id 	= SPRDMUX_ID_SPI,
	.dev_id 	= 1,
	.name 		= "veth_spi1",
};
struct platform_device sprd_veth_spi1_device = {
	.name 	= "sprd_veth",
	.id 	= 1,
	.dev = {.platform_data = &sprd_veth_spi1_pdata},
};

static struct veth_init_data sprd_veth_spi2_pdata = {
	.index 		= 15,
	.inst_id 	= SPRDMUX_ID_SPI,
	.dev_id 	= 2,
	.name 		= "veth_spi2",
};
struct platform_device sprd_veth_spi2_device = {
	.name 	= "sprd_veth",
	.id 	= 2,
	.dev = {.platform_data = &sprd_veth_spi2_pdata},
};

static struct veth_init_data sprd_veth_spi3_pdata = {
	.index 		= 16,
	.inst_id 	= SPRDMUX_ID_SPI,
	.dev_id 	= 3,
	.name 		= "veth_spi3",
};
struct platform_device sprd_veth_spi3_device = {
	.name 	= "sprd_veth",
	.id 	= 3,
	.dev = {.platform_data = &sprd_veth_spi3_pdata},
};

static struct veth_init_data sprd_veth_spi4_pdata = {
	.index 		= 17,
	.inst_id 	= SPRDMUX_ID_SPI,
	.dev_id 	= 4,
	.name 		= "veth_spi4",
};
struct platform_device sprd_veth_spi4_device = {
	.name 	= "sprd_veth",
	.id 	= 4,
	.dev = {.platform_data = &sprd_veth_spi4_pdata},
};
#endif

#ifdef CONFIG_MUX_SDIO_OPT1_HAL
static struct veth_init_data sprd_veth_sdio0_pdata = {
	.index 		= 13,
	.inst_id 	= SPRDMUX_ID_SDIO,
	.dev_id 	= 0,
	.name 		= "veth_sdio0",
};
struct platform_device sprd_veth_sdio0_device = {
	.name 	= "sprd_veth",
	.id 	= 5,
	.dev = {.platform_data = &sprd_veth_sdio0_pdata},
};

static struct veth_init_data sprd_veth_sdio1_pdata = {
	.index 		= 14,
	.inst_id 	= SPRDMUX_ID_SDIO,
	.dev_id 	= 1,
	.name 		= "veth_sdio1",
};
struct platform_device sprd_veth_sdio1_device = {
	.name 	= "sprd_veth",
	.id 	= 6,
	.dev = {.platform_data = &sprd_veth_sdio1_pdata},
};

static struct veth_init_data sprd_veth_sdio2_pdata = {
	.index 		= 15,
	.inst_id 	= SPRDMUX_ID_SDIO,
	.dev_id 	= 2,
	.name 		= "veth_sdio2",
};
struct platform_device sprd_veth_sdio2_device = {
	.name 	= "sprd_veth",
	.id 	= 7,
	.dev = {.platform_data = &sprd_veth_sdio2_pdata},
};

static struct veth_init_data sprd_veth_sdio3_pdata = {
	.index 		= 16,
	.inst_id 	= SPRDMUX_ID_SDIO,
	.dev_id 	= 3,
	.name 		= "veth_sdio3",
};
struct platform_device sprd_veth_sdio3_device = {
	.name 	= "sprd_veth",
	.id 	= 8,
	.dev = {.platform_data = &sprd_veth_sdio3_pdata},
};

static struct veth_init_data sprd_veth_sdio4_pdata = {
	.index 		= 17,
	.inst_id 	= SPRDMUX_ID_SDIO,
	.dev_id 	= 4,
	.name 		= "veth_sdio4",
};
struct platform_device sprd_veth_sdio4_device = {
	.name 	= "sprd_veth",
	.id 	= 9,
	.dev = {.platform_data = &sprd_veth_sdio4_pdata},
};
#endif

#endif
#endif
#ifdef CONFIG_SIPC_TD
#ifndef CONFIG_OF
#ifdef CONFIG_ARCH_SCX30G
#define TD_REG_CLK_ADDR         (SPRD_PMU_BASE + 0x44)
#define TD_REG_RESET_ADDR       (SPRD_PMU_BASE + 0xB0)

static int native_tdmodem_start(void *arg)
{
	u32 state;
	u32 cp1data[3] = {0xe59f0000, 0xe12fff10, CPT_START_ADDR + 0x300000};
	memcpy((void *)SPRD_IRAM1_BASE, cp1data, sizeof(cp1data));

	/* clear cp1 force shutdown */
	sci_glb_clr(TD_REG_CLK_ADDR,(0x1<<25));
	msleep(50);

	/* clear cp1 force deep sleep */
	sci_glb_clr(TD_REG_CLK_ADDR,(0x1<<28));
	msleep(50);

	/* clear reset cp1 */
	sci_glb_set(TD_REG_RESET_ADDR, 0x1);
	sci_glb_clr(TD_REG_RESET_ADDR,0x1);

	while(1)
	{
        state =  sci_glb_read(TD_REG_RESET_ADDR,-1UL);
        if(!(state & 0x1))
             break;
	}

	return 0;
}
static int native_tdmodem_stop(void *arg)
{
	/* reset cp1 */
	sci_glb_set(TD_REG_RESET_ADDR, 0x1);

	/* cp1 force deep sleep */
	sci_glb_set(TD_REG_CLK_ADDR, (0x1<<28));

	/* cp1 force shutdown */
	sci_glb_set(TD_REG_CLK_ADDR, (0x1<<25));

	return 0;
}
static struct cproc_init_data sprd_cproc_td_pdata = {
	.devname	= "cpt",
	.base		= CPT_START_ADDR,
	.maxsz		= CPT_TOTAL_SIZE,
	.start		= native_tdmodem_start,
	.stop 		= native_tdmodem_stop,
	.wdtirq		= IRQ_CP1_WDG_INT,
	.segnr		= 2,
	.segs		= {
		{
			.name  = "modem",
			.base  = CPT_START_ADDR + 0x300000,
			.maxsz = 0x00800000,
		},
		{
			.name  = "dsp",
			.base  = CPT_START_ADDR + 0x20000,
			.maxsz = 0x002E0000,
		},
	},
};
#else
#define TD_REG_CLK_ADDR				(SPRD_PMU_BASE + 0x50)
#define TD_REG_RESET_ADDR			(SPRD_PMU_BASE + 0xA8)
#define TD_REG_STATUS_ADDR			(SPRD_PMU_BASE + 0xBC)
static int native_tdmodem_start(void *arg)
{
	u32 state;
	u32 cp1data[3] = {0xe59f0000, 0xe12fff10, CPT_START_ADDR + 0x300000};
	memcpy((void *)(SPRD_IRAM1_BASE + 0x1800), cp1data, sizeof(cp1data));

	/* clear cp1 force shutdown */
	sci_glb_clr(TD_REG_CLK_ADDR, 0x02000000);

	while(1)
	{
		state = sci_glb_read(TD_REG_STATUS_ADDR,-1UL);
		if (!(state & (0xf<<16)))
			break;
	}

	/* clear cp1 force deep sleep */
	sci_glb_clr(TD_REG_CLK_ADDR, 0x10000000);

	/* clear reset cp1 */
	sci_glb_clr(TD_REG_RESET_ADDR,0x00000002);

	return 0;
}
static int native_tdmodem_stop(void *arg)
{
	/* reset cp1 */
	sci_glb_set(TD_REG_RESET_ADDR, 0x00000002);

	/* cp1 force deep sleep */
	sci_glb_set(TD_REG_CLK_ADDR, 0x10000000);

	/* cp1 force shutdown */
	sci_glb_set(TD_REG_CLK_ADDR, 0x02000000);

	return 0;
}


static struct cproc_init_data sprd_cproc_td_pdata = {
	.devname	= "cpt",
	.base		= CPT_START_ADDR,
	.maxsz		= CPT_TOTAL_SIZE,
	.start		= native_tdmodem_start,
	.stop 		= native_tdmodem_stop,
	.wdtirq		= IRQ_CP1_WDG_INT,
	.segnr		= 2,
	.segs		= {
		{
			.name  = "modem",
#ifdef CONFIG_CP_SWITCH
			.base  = CPT_START_ADDR + 0x400000,
#else
			.base  = CPT_START_ADDR + 0x300000,
#endif
			.maxsz = 0x00800000,
		},
		{
			.name  = "dsp",
			.base  = CPT_START_ADDR + 0x20000,
			.maxsz = 0x002E0000,
		},
	},
};
#endif

struct platform_device sprd_cproc_td_device = {
	.name           = "sprd_cproc",
	.id             = 0,
	.dev		= {.platform_data = &sprd_cproc_td_pdata},
};

static struct spipe_init_data sprd_spipe_td_pdata = {
	.name		= "spipe_td",
	.dst		= SIPC_ID_CPT,
	.channel	= SMSG_CH_PIPE,
	.ringnr		= 9,
	.txbuf_size	= 4096,
	.rxbuf_size	= 4096,
};
struct platform_device sprd_spipe_td_device = {
	.name           = "spipe",
	.id             = 0,
	.dev		= {.platform_data = &sprd_spipe_td_pdata},
};

static struct spipe_init_data sprd_slog_td_pdata = {
	.name		= "slog_td",
	.dst		= SIPC_ID_CPT,
	.channel	= SMSG_CH_PLOG,
	.ringnr		= 1,
	.txbuf_size	= 32*1024,
	.rxbuf_size	= 256 * 1024,
};
struct platform_device sprd_slog_td_device = {
	.name           = "spipe",
	.id             = 1,
	.dev		= {.platform_data = &sprd_slog_td_pdata},
};

static struct spipe_init_data sprd_stty_td_pdata = {
	.name		= "stty_td",
	.dst		= SIPC_ID_CPT,
	.channel	= SMSG_CH_TTY,
	.ringnr		= 32,
	.txbuf_size	= 2*1024,
	.rxbuf_size	= 2*1024,
};
struct platform_device sprd_stty_td_device = {
	.name           = "spipe",
	.id             = 2,
	.dev		= {.platform_data = &sprd_stty_td_pdata},
};

// Samsung uses the name rmnet for eth
static struct seth_init_data sprd_seth0_td_pdata = {
	.name		= "rmnet0",
	.dst		= SIPC_ID_CPT,
	.channel	= SMSG_CH_DATA0,
	.blocknum	= 64,
};
struct platform_device sprd_seth0_td_device = {
	.name           = "seth",
	.id             =  0,
	.dev		= {.platform_data = &sprd_seth0_td_pdata},
};

static struct seth_init_data sprd_seth1_td_pdata = {
	.name		= "rmnet1",
	.dst		= SIPC_ID_CPT,
	.channel	= SMSG_CH_DATA1,
	.blocknum	= 64,
};
struct platform_device sprd_seth1_td_device = {
	.name           = "seth",
	.id             =  1,
	.dev		= {.platform_data = &sprd_seth1_td_pdata},
};

static struct seth_init_data sprd_seth2_td_pdata = {
	.name		= "rmnet2",
	.dst		= SIPC_ID_CPT,
	.channel	= SMSG_CH_DATA2,
	.blocknum	= 64,
};
struct platform_device sprd_seth2_td_device = {
	.name           = "seth",
	.id             =  2,
	.dev		= {.platform_data = &sprd_seth2_td_pdata},
};

static struct spool_init_data sprd_spool_td_pdata = {
	.name 		= "spool_td",
	.dst 		= SIPC_ID_CPT,
	.channel 	= SMSG_CH_CTRL,
	.txblocknum 	= 64,
	.txblocksize 	= 1516,
	.rxblocknum 	= 64,
	.rxblocksize 	= 1516,
};
struct platform_device sprd_spool_td_device = {
	.name 		= "spool",
	.id 		= 0,
	.dev 		= {.platform_data = &sprd_spool_td_pdata},
};

static struct saudio_init_data sprd_saudio_td={
	"VIRTUAL AUDIO",
	SIPC_ID_CPT,
	SMSG_CH_VBC,
	SMSG_CH_PLAYBACK,
	SMSG_CH_CAPTURE,
	SMSG_CH_MONITOR_AUDIO,
};

struct platform_device sprd_saudio_td_device = {
	.name       = "saudio",
	.id         = 0,
	.dev        = {.platform_data=&sprd_saudio_td},
};

#endif
#endif
#ifdef CONFIG_SIPC_WCDMA
#ifndef CONFIG_OF
#ifdef CONFIG_ARCH_SCX30G
#define WCDMA_REG_CLK_ADDR         (SPRD_PMU_BASE + 0x44)
#define WCDMA_REG_RESET_ADDR       (SPRD_PMU_BASE + 0xB0)

static int native_wcdmamodem_start(void *arg)
{
	u32 state;
	u32 cp0data[3] = {0xe59f0000, 0xe12fff10, CPW_START_ADDR + 0x300000};
	memcpy((void *)SPRD_IRAM1_BASE, cp0data, sizeof(cp0data));

	/* clear cp1 force shutdown */
	sci_glb_clr(WCDMA_REG_CLK_ADDR,(0x1<<25));
	msleep(50);

	/* clear cp1 force deep sleep */
	sci_glb_clr(WCDMA_REG_CLK_ADDR,(0x1<<28));
	msleep(50);

	/* clear reset cp1 */
	sci_glb_set(WCDMA_REG_RESET_ADDR, 0x1);
	sci_glb_clr(WCDMA_REG_RESET_ADDR,0x1);

	while(1)
	{
        state =  sci_glb_read(WCDMA_REG_RESET_ADDR,-1UL);
        if(!(state & 0x1))
             break;
	}

	return 0;
}
static int native_wcdmamodem_stop(void *arg)
{
	/* reset cp1 */
	sci_glb_set(WCDMA_REG_RESET_ADDR, 0x1);

	/* cp1 force deep sleep */
	sci_glb_set(WCDMA_REG_CLK_ADDR, (0x1<<28));

	/* cp1 force shutdown */
	sci_glb_set(WCDMA_REG_CLK_ADDR, (0x1<<25));

	return 0;
}

static struct cproc_init_data sprd_cproc_wcdma_pdata = {
	.devname	= "cpw",
	.base		= CPW_START_ADDR,
	.maxsz		= CPW_TOTAL_SIZE,
	.start		= native_wcdmamodem_start,
	.stop 		= native_wcdmamodem_stop,
	.wdtirq		= IRQ_CP0_WDG_INT,
	.segnr		= 2,
	.segs		= {
		{
			.name  = "modem",
			.base  = CPW_START_ADDR + 0x300000,
			.maxsz = 0x00800000,
		},
		{
			.name  = "dsp",
			.base  = CPW_START_ADDR + 0x20000,
			.maxsz = 0x00200000,
		},
	},
};
#else
#define WCDMA_REG_CLK_ADDR				(SPRD_PMU_BASE + 0x3C)
#define WCDMA_REG_RESET_ADDR			(SPRD_PMU_BASE + 0xA8)
#define WCDMA_REG_STATUS_ADDR			(SPRD_PMU_BASE + 0xB8)

static int native_wcdmamodem_start(void *arg)
{
	u32 state;
#if defined(CONFIG_ARCH_SCX15)
	u32 cp0data[3] = {0xe59f0000, 0xe12fff10, CPW_START_ADDR + 0x2c0000};
#else
	u32 cp0data[3] = {0xe59f0000, 0xe12fff10, CPW_START_ADDR + 0x300000};
#endif
	memcpy((void *)SPRD_IRAM1_BASE, cp0data, sizeof(cp0data));

	/* clear cp0 force shutdown */
	sci_glb_clr(WCDMA_REG_CLK_ADDR, 0x02000000);

	while(1)
	{
		state = sci_glb_read(WCDMA_REG_STATUS_ADDR,-1UL);
		if (!(state & (0xf<<28)))
			break;
	}

	/* clear cp0 force deep sleep */
	sci_glb_clr(WCDMA_REG_CLK_ADDR, 0x10000000);

	/* clear reset cp0 cp1 */
	sci_glb_clr(WCDMA_REG_RESET_ADDR, 0x00000001);

	return 0;
}
static int native_wcdmamodem_stop(void *arg)
{
	u32 value;
	/* reset cp0 */
	sci_glb_set(WCDMA_REG_RESET_ADDR, 0x00000001);

	/* cp0 force deep sleep */
	sci_glb_set(WCDMA_REG_CLK_ADDR, 0x10000000);

	/* clear cp0 force shutdown */
	sci_glb_set(WCDMA_REG_CLK_ADDR, 0x02000000);

	return 0;
}

static struct cproc_init_data sprd_cproc_wcdma_pdata = {
	.devname	= "cpw",
	.base		= CPW_START_ADDR,
	.maxsz		= CPW_TOTAL_SIZE,
	.start		= native_wcdmamodem_start,
	.stop 		= native_wcdmamodem_stop,
	.wdtirq		= IRQ_CP0_WDG_INT,
	.segnr		= 2,
	.segs		= {
		{
			.name  = "modem",
#if defined(CONFIG_ARCH_SCX15)
			.base  = CPW_START_ADDR + 0x2c0000,
#else
			.base  = CPW_START_ADDR + 0x300000,
#endif
			.maxsz = 0x00800000,
		},
		{
			.name  = "dsp",
			.base  = CPW_START_ADDR + 0x20000,
			.maxsz = 0x00200000,
		},
	},
};

#endif
struct platform_device sprd_cproc_wcdma_device = {
	.name           = "sprd_cproc",
	.id             = 1,
	.dev		= {.platform_data = &sprd_cproc_wcdma_pdata},
};
static struct spipe_init_data sprd_spipe_wcdma_pdata = {
	.name		= "spipe_w",
	.dst		= SIPC_ID_CPW,
	.channel	= SMSG_CH_PIPE,
	.ringnr		= 9,
	.txbuf_size	= 4096,
	.rxbuf_size	= 4096,
};
struct platform_device sprd_spipe_wcdma_device = {
	.name           = "spipe",
	.id             = 3,
	.dev		= {.platform_data = &sprd_spipe_wcdma_pdata},
};

static struct spipe_init_data sprd_slog_wcdma_pdata = {
	.name		= "slog_w",
	.dst		= SIPC_ID_CPW,
	.channel	= SMSG_CH_PLOG,
	.ringnr		= 1,
	.txbuf_size	= 32 * 1024,
	.rxbuf_size	= 256 * 1024,
};
struct platform_device sprd_slog_wcdma_device = {
	.name           = "spipe",
	.id             = 4,
	.dev		= {.platform_data = &sprd_slog_wcdma_pdata},
};

static struct spipe_init_data sprd_stty_wcdma_pdata = {
	.name		= "stty_w",
	.dst		= SIPC_ID_CPW,
	.channel	= SMSG_CH_TTY,
	.ringnr		= 32,
	.txbuf_size	= 2*1024,
	.rxbuf_size	= 2*1024,
};
struct platform_device sprd_stty_wcdma_device = {
	.name           = "spipe",
	.id             = 5,
	.dev		= {.platform_data = &sprd_stty_wcdma_pdata},
};

// Samsung uses the name rmnet for eth
static struct seth_init_data sprd_seth0_wcdma_pdata = {
	.name		= "rmnet0",
	.dst		= SIPC_ID_CPW,
	.channel	= SMSG_CH_DATA0,
	.blocknum	= 64,
};
struct platform_device sprd_seth0_wcdma_device = {
	.name           = "seth",
	.id             =  3,
	.dev		= {.platform_data = &sprd_seth0_wcdma_pdata},
};

static struct seth_init_data sprd_seth1_wcdma_pdata = {
	.name		= "rmnet1",
	.dst		= SIPC_ID_CPW,
	.channel	= SMSG_CH_DATA1,
	.blocknum	= 64,
};
struct platform_device sprd_seth1_wcdma_device = {
	.name           = "seth",
	.id             =  4,
	.dev		= {.platform_data = &sprd_seth1_wcdma_pdata},
};

static struct seth_init_data sprd_seth2_wcdma_pdata = {
	.name		= "rmnet2",
	.dst		= SIPC_ID_CPW,
	.channel	= SMSG_CH_DATA2,
	.blocknum	= 64,
};
struct platform_device sprd_seth2_wcdma_device = {
	.name           = "seth",
	.id             =  5,
	.dev		= {.platform_data = &sprd_seth2_wcdma_pdata},
};

static struct spool_init_data sprd_spool_wcdma_pdata = {
	.name 		= "spool_w",
	.dst 		= SIPC_ID_CPW,
	.channel 	= SMSG_CH_CTRL,
	.txblocknum 	= 64,
	.txblocksize	= 1516,
	.rxblocknum 	= 64,
	.rxblocksize 	= 1516,
};
struct platform_device sprd_spool_wcdma_device = {
	.name 		= "spool",
	.id 		= 1,
	.dev 		= {.platform_data = &sprd_spool_wcdma_pdata},
};

static struct saudio_init_data sprd_saudio_wcdma={
	"VIRTUAL AUDIO W",
	SIPC_ID_CPW,
	SMSG_CH_VBC,
	SMSG_CH_PLAYBACK,
	SMSG_CH_CAPTURE,
	SMSG_CH_MONITOR_AUDIO,
};

struct platform_device sprd_saudio_wcdma_device = {
	.name       = "saudio",
	.id         = 1,
	.dev        = {.platform_data=&sprd_saudio_wcdma},
};

#endif
#endif
#ifdef CONFIG_SIPC_WCN
#ifdef CONFIG_ARCH_SCX30G
#define WCN_REG_CLK_ADDR                               (SPRD_PMU_BASE + 0x68)
#define WCN_REG_RESET_ADDR                             (SPRD_PMU_BASE + 0xB0)
//#define WCN_REG_STATUS_ADDR                    		   (SPRD_PMU_BASE + 0xC0)
#define WCN_SLEEP_STATUS                               (SPRD_PMU_BASE + 0xD4)

static int native_wcnmodem_start(void *arg)
{
	u32 state;
	u32 cp_code_addr,code_phy_addr;

	u32 cp2data[3] = {0xe59f0000, 0xe12fff10, WCN_START_ADDR + 0x8000};
	/*Notice: According to emc_earlysuspend_8830.c,the address is remapped here.
	* Now it is different with CP1.You can take a refernce with bug#253748	
	*/
	printk("%s\n",__func__);
	code_phy_addr = SPRD_IRAM1_PHYS + 0x3000;
	cp_code_addr = (volatile u32)ioremap(code_phy_addr,0x1000);
	memcpy(cp_code_addr, cp2data, sizeof(cp2data));

	/* clear cp2 force shutdown */
	sci_glb_clr(WCN_REG_CLK_ADDR, 0x02000000);
	msleep(5);

	/* set reset cp2 */
	sci_glb_set(WCN_REG_RESET_ADDR, 0x00000004);
	msleep(5);

	/* clear cp2 force deep sleep */
	sci_glb_clr(WCN_REG_CLK_ADDR, 0x10000000);
	msleep(5);

	/* clear reset cp2 */
	sci_glb_clr(WCN_REG_RESET_ADDR, 0x00000004);

	while(1)
	{
		state = sci_glb_read(WCN_REG_RESET_ADDR,-1UL);
		if (!(state & (0x1<<2)))
			break;
	}

	iounmap(cp_code_addr);
	return 0;
}
static int native_wcnmodem_stop(void *arg)
{
	uint32_t state = 0;

	printk("%s\n",__func__);
	while(1)
	{
		state = sci_glb_read(WCN_SLEEP_STATUS,-1UL);
		if (!(state & (0xf<<12)))
			break;
		msleep(1);
	}
	printk("%s cp2 enter sleep\n",__func__);

	/* reset cp2 */
	sci_glb_set(WCN_REG_RESET_ADDR, 0x00000004);

	/* cp2 force deep sleep */
	sci_glb_set(WCN_REG_CLK_ADDR, 0x10000000);

	/* cp2 force shutdown */
	sci_glb_set(WCN_REG_CLK_ADDR, 0x02000000);
	return 0;
}

#else
#define WCN_REG_CLK_ADDR                               (SPRD_PMU_BASE + 0x60)
#define WCN_REG_RESET_ADDR                             (SPRD_PMU_BASE + 0xA8)
#define WCN_REG_STATUS_ADDR                    		   (SPRD_PMU_BASE + 0xC0)
#define WCN_SLEEP_STATUS                               (SPRD_PMU_BASE + 0xCC)

static int native_wcnmodem_start(void *arg)
{
	u32 state;
	u32 cp_code_addr,code_phy_addr;

	u32 cp2data[3] = {0xe59f0000, 0xe12fff10, WCN_START_ADDR + 0x60000};
	/*Notice: According to emc_earlysuspend_8830.c,the address is remapped here.
	* Now it is different with CP1.You can take a refernce with bug#253748 .		
	*/
	printk("%s\n",__func__);

	code_phy_addr = SPRD_IRAM1_PHYS + 0x3000;
	cp_code_addr = (volatile u32)ioremap(code_phy_addr,0x1000);
	memcpy(cp_code_addr, cp2data, sizeof(cp2data));

	/* clear cp2 force shutdown */
	sci_glb_clr(WCN_REG_CLK_ADDR, 0x02000000);

	while(1)
	{
		state = sci_glb_read(WCN_REG_STATUS_ADDR,-1UL);
		if (!(state & (0xf)))
		break;
	}

	/* set reset cp2*/
	sci_glb_set(WCN_REG_RESET_ADDR, 0x00000004);

	/* clear cp2 force deep sleep */
	sci_glb_clr(WCN_REG_CLK_ADDR, 0x10000000);

	/* clear reset cp2*/
	sci_glb_clr(WCN_REG_RESET_ADDR, 0x00000004);

	iounmap(cp_code_addr);
	
	return 0;
}
static int native_wcnmodem_stop(void *arg)
{
	uint32_t state = 0;

	printk("%s\n",__func__);
	while(1)
	{
		state = sci_glb_read(WCN_SLEEP_STATUS,-1UL);
		if (!(state & (0xf<<12)))
			break;
		msleep(1);
	}
	printk("%s cp2 enter sleep\n",__func__);

	/* reset cp2 */
	sci_glb_set(WCN_REG_RESET_ADDR, 0x00000004);

	/* cp2 force deep sleep */
	sci_glb_set(WCN_REG_CLK_ADDR, 0x10000000);

	/* cp2 force shutdown */
	sci_glb_set(WCN_REG_CLK_ADDR, 0x02000000);

	return 0;
}
#endif
#ifndef CONFIG_OF
static struct cproc_init_data sprd_cproc_wcn_pdata = {
	.devname        = "cpwcn",
	.base           = WCN_START_ADDR,
	.maxsz          = WCN_TOTAL_SIZE,
	.start          = native_wcnmodem_start,
	.stop           = native_wcnmodem_stop,
	.wdtirq         = IRQ_CP2_WDG_INT,
	.segnr          = 1,
	.segs           = {
		{
		.name  = "modem",
		#ifdef CONFIG_ARCH_SCX30G
		.base  = WCN_START_ADDR + 0x8000,
		#else
		.base  = WCN_START_ADDR + 0x60000,
		#endif
		.maxsz = 0x00100000,
		},
	},
};
struct platform_device sprd_cproc_wcn_device = {
	.name           = "sprd_cproc",
	.id             = 2,
	.dev            = {.platform_data = &sprd_cproc_wcn_pdata},
};


static struct spipe_init_data sprd_spipe_wcn_pdata = {
	.name           = "spipe_wcn",
	.dst            = SIPC_ID_WCN,
	.channel        = SMSG_CH_PIPE,
	.ringnr         = 12,
	.txbuf_size     = 4096,
	.rxbuf_size     = 4096,
};
struct platform_device sprd_spipe_wcn_device = {
	.name           = "spipe",
	.id             = 6,
	.dev            = {.platform_data = &sprd_spipe_wcn_pdata},
};

static struct spipe_init_data sprd_slog_wcn_pdata = {
	.name           = "slog_wcn",
	.dst            = SIPC_ID_WCN,
	.channel        = SMSG_CH_PLOG,
	.ringnr         = 1,
	.txbuf_size     = 32 * 1024,
	.rxbuf_size     = 128 * 1024,
};
struct platform_device sprd_slog_wcn_device = {
	.name           = "spipe",
	.id             = 7,
	.dev            = {.platform_data = &sprd_slog_wcn_pdata},
};

static struct stty_init_data sprd_sttybt_td_pdata = {
	.name		= "sttybt",
	.dst		= SIPC_ID_WCN,
	.channel	= SMSG_CH_PIPE,
	.bufid		= 10,
};
struct platform_device sprd_sttybt_td_device = {
	.name           = "sttybt",
	.id             = 0,
	.dev		= {.platform_data = &sprd_sttybt_td_pdata},
};
#endif
#endif
#ifndef CONFIG_OF
static struct saudio_init_data sprd_saudio_voip={
	"saudiovoip",
#ifdef CONFIG_VOIP_CPT
	SIPC_ID_CPT,
#else
	SIPC_ID_CPW,
#endif
	SMSG_CH_CTRL_VOIP,
	SMSG_CH_PLAYBACK_VOIP,
	SMSG_CH_CAPTURE_VOIP,
	SMSG_CH_MONITOR_VOIP,
};

struct platform_device sprd_saudio_voip_device = {
	.name       = "saudio",
	.id         = 2,
	.dev        = {.platform_data=&sprd_saudio_voip},
};
#endif

#ifdef CONFIG_TS0710_MUX_ENABLE
static struct mux_init_data sprd_mux_spi_pdata = {
	.id	  = SPRDMUX_ID_SPI,
	.name     = "spimux",
	.num      = 32,
};

struct platform_device sprd_mux_spi_device = {
	.name      = "mux",
	.id        = 0,
	.dev       = {.platform_data = &sprd_mux_spi_pdata},
};

static struct mux_init_data sprd_mux_sdio_pdata = {
	.id       = SPRDMUX_ID_SDIO,
	.name     = "sdiomux",
	.num      = 32,
};

struct platform_device sprd_mux_sdio_device = {
	.name     = "mux",
	.id       = 1,
	.dev      = {.platform_data = &sprd_mux_sdio_pdata},
};
#endif

#define AP2CP_INT_CTRL		(SPRD_IPI_BASE + 0x00)
#define CP2AP_INT_CTRL		(SPRD_IPI_BASE + 0x04)


#define AP2CPW_IRQ0_TRIG	0x01
#define CPW2AP_IRQ0_CLR		0x01
uint32_t cpw_rxirq_status(void)
{
	return 1;
}

void cpw_rxirq_clear(void)
{
	__raw_writel(CPW2AP_IRQ0_CLR, CP2AP_INT_CTRL);
}

void cpw_txirq_trigger(void)
{
	__raw_writel(AP2CPW_IRQ0_TRIG, AP2CP_INT_CTRL);
}
#ifdef CONFIG_ARCH_SCX30G
#define AP2CPT_IRQ0_TRIG        0x01
#define CPT2AP_IRQ0_CLR         0x01
#else
#define AP2CPT_IRQ0_TRIG	0x10
#define CPT2AP_IRQ0_CLR		0x10
#endif
uint32_t cpt_rxirq_status(void)
{
	return 1;
}

void cpt_rxirq_clear(void)
{
	__raw_writel(CPT2AP_IRQ0_CLR, CP2AP_INT_CTRL);
}

void cpt_txirq_trigger(void)
{
	__raw_writel(AP2CPT_IRQ0_TRIG, AP2CP_INT_CTRL);
}

#define AP2WCN_IRQ0_TRIG	0x100
#define WCN2AP_IRQ0_CLR		0x100
uint32_t wcn_rxirq_status(void)
{
	return 1;
}

void wcn_rxirq_clear(void)
{
	__raw_writel(WCN2AP_IRQ0_CLR, CP2AP_INT_CTRL);
}

void wcn_txirq_trigger(void)
{
	__raw_writel(AP2WCN_IRQ0_TRIG, AP2CP_INT_CTRL);
}

