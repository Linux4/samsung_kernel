/*
 * SAMSUNG UNIVERSAL3472 machine file
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/i2c.h>
#include <linux/cma.h>
#include <linux/ion.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <mach/map.h>
#include <mach/memory.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/clock.h>
#include <plat/iic.h>
#include <plat/adc.h>
#include <plat/watchdog.h>
#include <mach/regs-pmu.h>
#include <mach/gpio.h>
#include <mach/pmu.h>
#include <plat/gpio-cfg.h>

#include "common.h"
#include "board-universal3472.h"

#define UNIVERSAL3472_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
					 S3C2410_UCON_RXILEVEL |	\
					 S3C2410_UCON_TXIRQMODE |	\
					 S3C2410_UCON_RXIRQMODE |	\
					 S3C2410_UCON_RXFIFO_TOI |	\
					 S3C2443_UCON_RXERR_IRQEN)

#define UNIVERSAL3472_ULCON_DEFAULT	S3C2410_LCON_CS8

#define UNIVERSAL3472_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
					 S5PV210_UFCON_TXTRIG4 |	\
					 S5PV210_UFCON_RXTRIG4)


static struct s3c2410_uartcfg universal3472_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= UNIVERSAL3472_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3472_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3472_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= UNIVERSAL3472_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3472_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3472_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= UNIVERSAL3472_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3472_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3472_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= UNIVERSAL3472_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3472_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3472_UFCON_DEFAULT,
	},
};

static void __init universal3472_map_io(void)
{
	exynos_init_io(NULL, 0);
}

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
#if defined(CONFIG_ION_EXYNOS_MFC_WFD_SIZE) && \
		(CONFIG_ION_EXYNOS_MFC_WFD_SIZE != 0)
		{
			.name = "wfd_mfc",
			.size = CONFIG_ION_EXYNOS_MFC_WFD_SIZE *
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
		"ion-exynos/wfd_mfc=wfd_mfc;"
		"ion-exynos/video=drm_video;"
		"ion-exynos/mfc_input=drm_mfc_input;"
		"ion-exynos/mfc_fw=drm_mfc_fw;"
		"ion-exynos/sectbl=drm_sectbl;"
		"ion-exynos/g2d_wfd=drm_g2d_wfd;"
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
}
#else /*!CONFIG_CMA*/
static inline void exynos_reserve_mem(void)
{
}
#endif

/* ADC */
static struct s3c_adc_platdata universal3472_adc_data __initdata = {
	.phy_init	= s3c_adc_phy_init,
	.phy_exit	= s3c_adc_phy_exit,
};

#ifdef CONFIG_S3C_DEV_WDT
/* WDT */
static struct s3c_watchdog_platdata smdk3472_watchdog_platform_data = {
	exynos_pmu_wdt_control,
	PMU_WDT_RESET_TYPE0,
};
#endif

static struct platform_device *universal3472_devices[] __initdata = {
	&s3c_device_adc,
#ifdef CONFIG_S3C_DEV_WDT
	&s3c_device_wdt,
#endif
#ifdef CONFIG_MALI400
	&exynos4_device_g3d,
#endif
};

static void __init universal3472_machine_init(void)
{

	s3c_adc_set_platdata(&universal3472_adc_data);
#ifdef CONFIG_S3C_DEV_WDT
	s3c_watchdog_set_platdata(&smdk3472_watchdog_platform_data);
#endif

	exynos3_universal3472_clock_init();
	exynos3_universal3472_mmc_init();
	exynos3_universal3472_power_init();
	exynos3_universal3472_display_init();
	exynos3_universal3472_input_init();
	exynos3_universal3472_usb_init();
	exynos3_espresso3250_media_init();
	exynos3_universal3472_audio_init();
	exynos3_universal3472_gpio_init();
	exynos3_universal3472_camera_init();

	platform_add_devices(universal3472_devices, ARRAY_SIZE(universal3472_devices));
}

static void __init universal3472_init_early(void)
{
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(universal3472_uartcfgs,ARRAY_SIZE(universal3472_uartcfgs));
}

MACHINE_START(UNIVERSAL3472, "UNIVERSAL3472")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3472_init_early,
	.map_io		= universal3472_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3472_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
