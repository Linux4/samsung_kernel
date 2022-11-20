/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/persistent_ram.h>
#include <linux/cma.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/memblock.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>

#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/watchdog.h>
#include <plat/adc.h>

#include <mach/exynos_fiq_debugger.h>
#include <mach/map.h>
#include <mach/pmu.h>

#include "board-universal5260-wlan.h"
#include "board-universal5260-gpio.h"
#include "board-universal5260-pmic.h"
#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif

#include "common.h"
#include "board-universal5260.h"
#include "board-universal5260-gpio.h"

#include "include/board-bluetooth-bcm.h"

extern phys_addr_t bootloaderfb_start;
extern phys_addr_t bootloaderfb_size;

/*rfkill device registeration*/
#ifdef CONFIG_BT_BCM4339
static struct platform_device bcm4339_bluetooth_device = {
	.name = "bcm4339_bluetooth",
	.id = -1,
};
#endif

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define UNIVERSAL5260_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define UNIVERSAL5260_ULCON_DEFAULT	S3C2410_LCON_CS8

#define UNIVERSAL5260_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

extern unsigned int system_rev;

static struct s3c2410_uartcfg universal5260_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= UNIVERSAL5260_UCON_DEFAULT,
		.ulcon		= UNIVERSAL5260_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL5260_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= UNIVERSAL5260_UCON_DEFAULT,
		.ulcon		= UNIVERSAL5260_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL5260_UFCON_DEFAULT,
	},
	[2] = {
#ifndef CONFIG_EXYNOS_FIQ_DEBUGGER
	/*
	 * Don't need to initialize hwport 2, when FIQ debugger is
	 * enabled. Because it will be handled by fiq_debugger.
	 */
		.hwport		= 2,
		.flags		= 0,
		.ucon		= UNIVERSAL5260_UCON_DEFAULT,
		.ulcon		= UNIVERSAL5260_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL5260_UFCON_DEFAULT,
#endif
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= UNIVERSAL5260_UCON_DEFAULT,
		.ulcon		= UNIVERSAL5260_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL5260_UFCON_DEFAULT,
		.wake_peer	= bcm_bt_lpm_exit_lpm_locked,
#if defined(CONFIG_PM_RUNTIME)
		.cfg_gpio	= exynos_uart3_cfg_gpio,
		.is_aud_uart	= true,
#endif
	},
};

/* ADC */
static struct s3c_adc_platdata universal5260_adc_data __initdata = {
	.phy_init	= s3c_adc_phy_init,
	.phy_exit	= s3c_adc_phy_exit,
};

/* WDT */
static struct s3c_watchdog_platdata universal5260_watchdog_platform_data = {
	exynos_pmu_wdt_control,
	PMU_WDT_RESET_TYPE2,
};

#ifdef CONFIG_EXYNOS_PERSISTENT_CLOCK
static struct resource persistent_clock_resource[] = { 
 	[0] = DEFINE_RES_MEM(S3C_PA_RTC, SZ_256), 
}; 
  
static struct platform_device persistent_clock = { 
	.name           = "persistent_clock", 
	.id             = -1, 
	.num_resources	= ARRAY_SIZE(persistent_clock_resource), 
	.resource	= persistent_clock_resource, 
}; 
#endif /*CONFIG_EXYNOS_PERSISTENT_CLOCK*/

#ifdef CONFIG_SND_SAMSUNG_AUX_HDMI
static struct platform_device hdmi_snd_card_device = {
	.name	= "adonisuniv-hdmi",
	.id	= -1,
};
#endif

static struct platform_device *universal5260_devices[] __initdata = {
#ifdef CONFIG_EXYNOS_PERSISTENT_CLOCK
	&persistent_clock,
#endif
#ifdef CONFIG_MALI_T6XX
	&exynos5_device_g3d,
#endif
#ifdef CONFIG_WATCHDOG
	&s3c_device_wdt,
#endif
#ifdef CONFIG_RTC_DRV_SEC
	&s3c_device_rtc,
#endif
	&s3c_device_adc,
#ifdef CONFIG_S5P_DEV_ACE
	&s5p_device_sss,
	&s5p_device_slimsss,
#endif
#ifdef CONFIG_BT_BCM4339
	&bcm4339_bluetooth_device,
#endif
#ifdef CONFIG_SND_SAMSUNG_AUX_HDMI
	&exynos5_device_i2s1,
	&hdmi_snd_card_device,
#endif
};

#if defined(CONFIG_CMA)
#include "reserve-mem.h"
static void __init exynos_reserve_mem(void)
{
	static struct cma_region regions[] = {
#if defined(CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE) && \
		(CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE != 0)
		{
			.name = "ion",
			.size = CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
		},
#endif
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#ifdef CONFIG_ION_EXYNOS_DRM_MFC_SH
		{
			.name = "drm_mfc_sh",
			.size = SZ_1M,
		},
#endif
#if defined(CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD) && \
		(CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD != 0)
		{
			.name = "drm_g2d_wfd",
			.size = CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD *
				SZ_1K,
			{
				.alignment = SZ_1M,
			}
		},
#endif
#endif
#ifdef CONFIG_BL_SWITCHER
		{
			.name = "bl_mem",
			.size = SZ_8K,
		},
#endif
		{
			.size = 0
		},
	};
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	static struct cma_region regions_secure[] = {
#if defined(CONFIG_ION_EXYNOS_DRM_VIDEO) && \
		(CONFIG_ION_EXYNOS_DRM_VIDEO != 0)
	       {
			.name = "drm_video",
			.size = CONFIG_ION_EXYNOS_DRM_VIDEO *
				SZ_1K,
			{
				.alignment = SZ_1M,
			}
	       },
#endif
#if defined(CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT) && \
		(CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT != 0)
	       {
			.name = "drm_mfc_input",
			.size = CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT *
				SZ_1K,
			{
				.alignment = SZ_1M,
			}
	       },
#endif
#ifdef CONFIG_ION_EXYNOS_DRM_MFC_FW
		{
			.name = "drm_mfc_fw",
			.size = SZ_1M,
			{
				.alignment = SZ_1M,
			}
		},
#endif
#ifdef CONFIG_ION_EXYNOS_DRM_SECTBL
		{
			.name = "drm_sectbl",
			.size = SZ_1M,
			{
				.alignment = SZ_1M,
			}
		},
#endif
		{
			.size = 0
		},
	};
#else /* !CONFIG_EXYNOS_CONTENT_PATH_PROTECTION */
	struct cma_region *regions_secure = NULL;
#endif /* CONFIG_EXYNOS_CONTENT_PATH_PROTECTION */
	static const char map[] __initconst =
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		"ion-exynos/mfc_sh=drm_mfc_sh;"
		"ion-exynos/g2d_wfd=drm_g2d_wfd;"
		"ion-exynos/video=drm_video;"
		"ion-exynos/mfc_input=drm_mfc_input;"
		"ion-exynos/mfc_fw=drm_mfc_fw;"
		"ion-exynos/sectbl=drm_sectbl;"
		"s5p-smem/mfc_sh=drm_mfc_sh;"
		"s5p-smem/g2d_wfd=drm_g2d_wfd;"
		"s5p-smem/video=drm_video;"
		"s5p-smem/mfc_input=drm_mfc_input;"
		"s5p-smem/mfc_fw=drm_mfc_fw;"
		"s5p-smem/sectbl=drm_sectbl;"
#endif
#ifdef CONFIG_BL_SWITCHER
		"b.L_mem=bl_mem;"
#endif
		"ion-exynos=ion;";
	exynos_cma_region_reserve(regions, regions_secure, NULL, map);

	if (bootloaderfb_start) {
		int err = memblock_reserve(bootloaderfb_start,
				bootloaderfb_size);
		if (err)
			pr_warn("failed to reserve old framebuffer location\n");
	} else {
		pr_warn("bootloader framebuffer start address not set\n");
	}
}
#else /*!CONFIG_CMA*/
static inline void exynos_reserve_mem(void)
{
}
#endif

static void __init universal5260_map_io(void)
{
	clk_xusbxti.rate = 24000000;
	clk_xxti.rate = 24000000;

	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(clk_xusbxti.rate);
	s3c24xx_init_uarts(universal5260_uartcfgs, ARRAY_SIZE(universal5260_uartcfgs));
#ifdef CONFIG_SEC_DEBUG
	sec_debug_init();
#endif
}

static void __init universal5260_init_early(void)
{
#ifdef CONFIG_SEC_DEBUG
	sec_debug_magic_init();
#endif
}

static void __init universal5260_machine_init(void)
{
	s3c_adc_set_platdata(&universal5260_adc_data);
	s3c_watchdog_set_platdata(&universal5260_watchdog_platform_data);
#ifdef CONFIG_EXYNOS_FIQ_DEBUGGER
	exynos_serial_debug_init(2, 0);
#endif
	exynos5_universal5260_clock_init();
	exynos5_universal5260_mmc_init();
	exynos5_universal5260_pmic_init();
	exynos5_universal5260_power_init();
	exynos5_universal5260_battery_init();
	exynos5_universal5260_audio_init();
	exynos5_universal5260_input_init();
	exynos5_universal5260_display_init();
	exynos5_universal5260_usb_init();
	exynos5_universal5260_camera_init();
	exynos5_universal5260_vibrator_init();
#ifdef CONFIG_SAMSUNG_MUIC
	exynos5_universal5260_mfd_init();
#endif
	exynos5_universal5260_media_init();
	brcm_wlan_init();
	if (lpcharge == 0) {
		exynos5_universal5260_sensor_init();
#ifdef CONFIG_NFC_DEVICES
		exynos5_universal5260_nfc_init();
#endif
	} else
		pr_info("[SENSOR][NFC] : Poweroff charging\n");
#ifdef CONFIG_LEDS_AN30259A
	exynos5_universal5260_led_init();
#endif
	platform_add_devices(universal5260_devices, ARRAY_SIZE(universal5260_devices));

#if defined(CONFIG_ICE4_FPGA)
	exynos5_universal5260_fpga_init();
#endif

#ifdef CONFIG_SEC_THERMISTOR
        exynos5_universal5260_thermistor_init();
#endif
#ifdef CONFIG_W1
	exynos5_universal5260_cover_id_init();
#endif
#ifdef CONFIG_VIDEO_MHL_SII8246
	exynos5_universal5260_mhl_init();
#endif
	/*
	 * Please note that exynos5_universal5260_gpio_init() should be called
	 * after initializing other devices
	 */
	exynos5_universal5260_gpio_init();

}

MACHINE_START(UNIVERSAL5260, "UNIVERSAL5260")
	.atag_offset	= 0x100,
	.init_early	= universal5260_init_early,
	.init_irq	= exynos5_init_irq,
	.map_io		= universal5260_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal5260_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos5_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
