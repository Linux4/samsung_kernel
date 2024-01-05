/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/persistent_ram.h>
#include <linux/serial_core.h>
#include <linux/memblock.h>
#include <linux/mmc/host.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/adc.h>
#include <plat/watchdog.h>

#include <mach/map.h>
#include <mach/pmu.h>

#include "common.h"
#include "board-universal4415.h"
#include "reserve-mem.h"
#include "board-universal4415-wlan.h"

#ifdef CONFIG_SEC_DEBUG
#include "mach/sec_debug.h"
#endif
#ifdef CONFIG_DC_MOTOR
#include <linux/gpio.h>
#include <mach/gpio-exynos.h>
#include <linux/regulator/consumer.h>
#include <linux/dc_motor.h>
#endif

#ifdef CONFIG_ANDROID_TIMED_GPIO
#include <linux/gpio.h>
#include "../../../drivers/staging/android/timed_gpio.h"
#endif

#include "include/board-bluetooth-bcm.h"

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define UNIVERSAL4415_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define UNIVERSAL4415_ULCON_DEFAULT	S3C2410_LCON_CS8

#define UNIVERSAL4415_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg universal4415_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= UNIVERSAL4415_UCON_DEFAULT,
		.ulcon		= UNIVERSAL4415_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL4415_UFCON_DEFAULT,
#if defined(CONFIG_BT_BCM4334)
		.wake_peer	= bcm_bt_lpm_exit_lpm_locked,
#endif
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= UNIVERSAL4415_UCON_DEFAULT,
		.ulcon		= UNIVERSAL4415_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL4415_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= UNIVERSAL4415_UCON_DEFAULT,
		.ulcon		= UNIVERSAL4415_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL4415_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= UNIVERSAL4415_UCON_DEFAULT,
		.ulcon		= UNIVERSAL4415_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL4415_UFCON_DEFAULT,
	},
};

/* ADC */
static struct s3c_adc_platdata universal4415_adc_data __initdata = {
	.phy_init	= s3c_adc_phy_init,
	.phy_exit	= s3c_adc_phy_exit,
};

/* WDT */
static struct s3c_watchdog_platdata xyref4415_watchdog_platform_data = {
	exynos_pmu_wdt_control,
	PMU_WDT_RESET_TYPE0,
};

/*rfkill device registeration*/
#ifdef CONFIG_BT_BCM4334
static struct platform_device bcm4334_bluetooth_device = {
	.name = "bcm4334_bluetooth",
	.id = -1,
};
#endif

/* MOTOR */
#ifdef CONFIG_DC_MOTOR
int vibrator_on;
static void dc_motor_power_regulator(bool on)
{
	struct regulator *regulator;
	regulator = regulator_get(NULL, "vdd_motor_3v0");

	if (on) {
		vibrator_on = 1;
		regulator_enable(regulator);
	} else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		else
			regulator_force_disable(regulator);
		if (vibrator_on == 1)
			msleep(120);
		vibrator_on = 0;
	}
	regulator_put(regulator);
}

static struct dc_motor_platform_data dc_motor_pdata = {
	.power = dc_motor_power_regulator,
	.max_timeout = 10000,
};

static struct platform_device dc_motor_device = {
	.name = "sec-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &dc_motor_pdata,
	},
};
#endif	/* CONFIG_DC_MOTOR */

#ifdef CONFIG_ANDROID_TIMED_GPIO
static struct timed_gpio timed_gpio_data = {
	.name = "vibrator",
	.gpio = GPIO_MOTOR_EN,
	.max_timeout = 10000,
	.active_low = false,
};

static struct timed_gpio_platform_data timed_gpio_pdata = {
	.num_gpios = 1,
	.gpios = &timed_gpio_data,
};

static struct platform_device timed_gpio_device = {
	.name = TIMED_GPIO_NAME,
	.id = -1,
	.dev = {
		.platform_data = &timed_gpio_pdata,
	},
};
#endif

static struct platform_device *universal4415_devices[] __initdata = {
	&s3c_device_wdt,
	&s3c_device_adc,
	&exynos4_device_g3d,
#ifdef CONFIG_S5P_DEV_ACE
	&s5p_device_sss,
#endif
#ifdef CONFIG_BT_BCM4334
	&bcm4334_bluetooth_device,
#endif
#ifdef CONFIG_DC_MOTOR
	&dc_motor_device,
#endif
#ifdef CONFIG_ANDROID_TIMED_GPIO
	&timed_gpio_device,
#endif
};

#if defined(CONFIG_CMA)
#include "reserve-mem.h"
static void __init exynos_reserve_mem(void)
{
	static struct cma_region regions[] = {
#if defined(CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE) \
		&& CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE
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
#ifdef CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD
		{
			.name = "drm_g2d_wfd",
			.size = CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD *
				SZ_1K,
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
#ifdef CONFIG_ION_EXYNOS_DRM_VIDEO
	       {
			.name = "drm_video",
			.size = CONFIG_ION_EXYNOS_DRM_VIDEO *
				SZ_1K,
			{
				.alignment = SZ_1M,
			}
	       },
#endif
#ifdef CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT
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
}
#else /*!CONFIG_CMA*/
static inline void exynos_reserve_mem(void)
{
}
#endif

static void __init universal4415_map_io(void)
{
	clk_xusbxti.rate = 24000000;

	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(clk_xusbxti.rate);
	s3c24xx_init_uarts(universal4415_uartcfgs, ARRAY_SIZE(universal4415_uartcfgs));
}

static void __init universal4415_init_early(void)
{
#ifdef CONFIG_SEC_DEBUG
	sec_debug_magic_init();
	sec_debug_init();
#endif
}

static void __init universal4415_machine_init(void)
{
	s3c_adc_set_platdata(&universal4415_adc_data);
	s3c_watchdog_set_platdata(&xyref4415_watchdog_platform_data);

	exynos4415_universal4415_clock_init();
        board_universal4415_gpio_init();  //board first call
        exynos4_universal4415_pmic_init();
	exynos4_universal4415_power_init();
	exynos4415_universal4415_mmc_init();
	exynos5_universal4415_battery_init();
	exynos5_universal4415_mfd_init();
	exynos4_universal4415_input_init();
	exynos4_universal4415_camera_init();
	exynos4_universal4415_display_init();
	exynos4_universal4415_media_init();
#ifdef CONFIG_SEC_DEV_JACK
	exynos4_universal4415_jack_init();
#endif
	exynos4_universal4415_audio_init();
	exynos4_universal4415_usb_init();
	exynos4_universal4415_muic_init();
	brcm_wlan_init();
        exynos4_universal4415_led_init();
#ifdef CONFIG_W1
        exynos4_universal4415_w1_init();
#endif
#ifdef CONFIG_DC_MOTOR
	dc_motor_init();
#endif	/* CONFIG_DC_MOTOR */
	if (lpcharge == 0)
		exynos4_universal4415_sensor_init();
	else
		pr_info("[SENSOR] : Poweroff charging\n");

	platform_add_devices(universal4415_devices, ARRAY_SIZE(universal4415_devices));

        board_universal4415_thermistor_init();
#ifdef CONFIG_NFC_DEVICES
    exynos4_universal4415_nfc_init();
#endif
}

MACHINE_START(UNIVERSAL4415, "Samsung EXYNOS4415")
	/* Maintainer: Kukjin Kim <kgene.kim@samsung.com> */
	.atag_offset	= 0x100,
	.init_early	= universal4415_init_early,
	.init_irq	= exynos4_init_irq,
	.map_io		= universal4415_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal4415_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos4_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
