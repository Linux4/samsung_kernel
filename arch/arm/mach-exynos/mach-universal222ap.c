/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>
#ifdef CONFIG_EXYNOS_DEV_SHARED_MEMORY
#include <mach/shdmem.h>
#endif
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

#include <mach/gpio.h>
#include <mach/regs-pmu.h>


#include "common.h"
#include "board-universal222ap.h"
#if defined(CONFIG_MACH_KMINI)
#include "board-kmini.h"
#include "board-kmini-wlan.h"
#endif

#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif
#ifdef CONFIG_DC_MOTOR
#include <linux/regulator/consumer.h>
#include <linux/dc_motor.h>
#endif

#include <mach/board-bluetooth-bcm.h>
#if defined(CONFIG_MACH_GARDA)
#include "board-garda.h"
#include "board-garda-wlan.h"
#elif defined(CONFIG_MACH_DEGAS)
#include "board-degas.h"
#include "board-degas-wlan.h"
#endif

#ifdef CONFIG_SEC_GPIO_DVS
#include <linux/secgpio_dvs.h>
#endif

static struct platform_device ramconsole_device = {
	.name	= "ram_console",
	.id	= -1,
};

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDK4270_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDK4270_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDK4270_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg smdk4270_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDK4270_UCON_DEFAULT,
		.ulcon		= SMDK4270_ULCON_DEFAULT,
		.ufcon		= SMDK4270_UFCON_DEFAULT,
#if defined(CONFIG_BT_BCM4334) || defined(CONFIG_BT_BCM4335)
		.wake_peer	= bcm_bt_lpm_exit_lpm_locked,
#endif
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDK4270_UCON_DEFAULT,
		.ulcon		= SMDK4270_ULCON_DEFAULT,
		.ufcon		= SMDK4270_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDK4270_UCON_DEFAULT,
		.ulcon		= SMDK4270_ULCON_DEFAULT,
		.ufcon		= SMDK4270_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDK4270_UCON_DEFAULT,
		.ulcon		= SMDK4270_ULCON_DEFAULT,
		.ufcon		= SMDK4270_UFCON_DEFAULT,
	},
};

/* WDT */
static struct s3c_watchdog_platdata smdk4270_watchdog_platform_data = {
	exynos_pmu_wdt_control,
	PMU_WDT_RESET_TYPE0,
};

#ifdef CONFIG_DC_MOTOR
#if defined(CONFIG_MACH_DEGAS)
static void dc_motor_power_gpio(bool on)
{
	gpio_direction_output(GPIO_MOTOR_EN, on);
}
#elif defined(CONFIG_MACH_KMINI)
extern unsigned int system_rev;
int vibrator_on;
static void dc_motor_power_regulator(bool on)
{
	struct regulator *regulator;
	regulator = regulator_get(NULL, "vdd_mot_3v0");

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

static void dc_motor_power_gpio(bool on)
{
	gpio_direction_output(GPIO_MOTOR_EN, on);
}
#else
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
#endif

static struct dc_motor_platform_data dc_motor_pdata = {
#if defined(CONFIG_MACH_KMINI) || defined(CONFIG_MACH_DEGAS)
	.power = dc_motor_power_gpio,
#else
	.power = dc_motor_power_regulator,
#endif
	.max_timeout = 10000,
};

static struct platform_device dc_motor_device = {
	.name = "sec-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &dc_motor_pdata,
	},
};

#if defined(CONFIG_MACH_DEGAS)
void __init dc_motor_init(void)
{
	int ret;
	int gpio;

	gpio = GPIO_MOTOR_EN;

	ret = gpio_request(gpio, "MOTOR_EN");
	if (ret)
		pr_err("failed to request gpio(MOTOR_EN)(%d)\n", ret);
	gpio_direction_output(GPIO_MOTOR_EN, 0);
}
#elif defined(CONFIG_MACH_KMINI)
void __init dc_motor_init(void)
{
	int ret;
	int gpio;

	if (system_rev == 1) {
		dc_motor_pdata.power = dc_motor_power_regulator;
	} else {
		gpio = GPIO_MOTOR_EN;

		ret = gpio_request(gpio, "MOTOR_EN");
		if (ret)
			pr_err("failed to request gpio(MOTOR_EN)(%d)\n", ret);
		gpio_direction_output(GPIO_MOTOR_EN, 0);
	}
}
#endif
#endif

#ifdef CONFIG_S3C_ADC
/* ADC */
static struct s3c_adc_platdata smdk4270_adc_data __initdata = {
	.phy_init       = s3c_adc_phy_init,
	.phy_exit       = s3c_adc_phy_exit,
};
#endif

/*rfkill device registeration*/
#ifdef CONFIG_BT_BCM4334
static struct platform_device bcm4334_bluetooth_device = {
	.name = "bcm4334_bluetooth",
	.id = -1,
};
#else
#ifdef CONFIG_BT_BCM4335
static struct platform_device bcm4335_bluetooth_device = {
	.name = "bcm4335_bluetooth",
	.id = -1,
};
#endif
#endif

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

static struct platform_device *smdk4270_devices[] __initdata = {
	&ramconsole_device,
#ifdef CONFIG_EXYNOS_PERSISTENT_CLOCK
	&persistent_clock,
#endif
	&s3c_device_wdt,
#ifdef CONFIG_S3C_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_S5P_DEV_ACE
	&s5p_device_sss,
#endif
#ifdef CONFIG_DC_MOTOR
	&dc_motor_device,
#endif
#ifdef CONFIG_MALI400
	&exynos4_device_g3d,
#endif
#ifdef CONFIG_BT_BCM4334
	&bcm4334_bluetooth_device,
#else
#ifdef CONFIG_BT_BCM4335
    &bcm4335_bluetooth_device,
#endif
#endif
};

static int exynos4_notifier_call(struct notifier_block *this,
		unsigned long code, void *_cmd)
{
	int mode = 0;

	if ((code == SYS_RESTART) && _cmd)
		if (!strcmp((char *)_cmd, "recovery"))
			mode = 0xf;

	__raw_writel(mode, EXYNOS_INFORM4);

	return NOTIFY_DONE;
}

static struct notifier_block exynos4_reboot_notifier = {
	.notifier_call = exynos4_notifier_call,
};

#if defined(CONFIG_CMA)
#include "reserve-mem.h"
#if (CONFIG_EXYNOS_MEM_BASE == 0x80000000)
#define SHDMEM_BASE 0xA0000000
#else
#define SHDMEM_BASE 0x60000000
#endif
void __init universal222_cma_region_reserve(struct cma_region *regions,
					const char *map, phys_addr_t paddr,
					phys_addr_t max_addr)
{
	struct cma_region *reg;
	phys_addr_t paddr_start;
	int ret;

	if (map)
		cma_set_defaults(NULL, map);

	for (reg = regions; reg && reg->size; reg++) {
		if (reg->start && !IS_ALIGNED(reg->start, PAGE_SIZE)) {
			pr_err("S5P/CMA: Failed to reserve '%s': "
				"address %#x not page-aligned\n",
				reg->name, reg->start);
			continue;
		}

		if (reg->reserved) {
			pr_err("S5P/CMA: '%s' already reserved\n", reg->name);
			continue;
		}

		if (reg->alignment) {
			if ((reg->alignment & ~PAGE_MASK) ||
				(reg->alignment & ~reg->alignment)) {
				pr_err("S5P/CMA: Failed to reserve '%s': "
						"incorrect alignment %#x.\n",
						reg->name, reg->alignment);
				continue;
			}
		} else {
			reg->alignment = PAGE_SIZE;
		}

		reg->size = PAGE_ALIGN(reg->size);

		paddr_start = memblock_find_in_range(paddr, max_addr,
						reg->size, reg->alignment);

		if (paddr_start != 0) {
			if (memblock_reserve(paddr_start, reg->size)) {
				pr_err("S5P/CMA: Failed to reserve '%s'\n",
								reg->name);
				continue;
			}

			reg->start = paddr_start;
			reg->reserved = 1;

			ret = cma_early_region_register(reg);

			pr_info("S5P/CMA: Reserved 0x%08x@0x%08x for '%s'\n",
					reg->size, reg->start, reg->name);
		} else {
			pr_err("S5P/CMA: No free space in memory for '%s'\n",
								reg->name);
		}
	}
}

__attribute__ ((unused))
phys_addr_t __init universal222_cma_secure_region_reserve(struct cma_region *regions_secure,
						const char *map, dma_addr_t alignment)
{
	size_t size_req = 0;
	size_t size_secure;
	struct cma_region *reg_sec;
	phys_addr_t paddr_start, paddr_last;
	int ret;
	unsigned int order;

	for (reg_sec = regions_secure; reg_sec && reg_sec->size; reg_sec++) {
		reg_sec->size = PAGE_ALIGN(reg_sec->size);
		size_req += reg_sec->size;
	}

	reg_sec--;

	order = get_order(size_req);
	alignment = 1 << (order + PAGE_SHIFT);
	size_secure = ALIGN(size_req, alignment / 8);
	if (size_secure) {
		paddr_last = __memblock_alloc_base(size_secure, alignment,
						MEMBLOCK_ALLOC_ANYWHERE);
		if (paddr_last == 0) {
			pr_err("CMA: Failed to reserve secure region\n");
			return 0;
		} else {
			paddr_start = paddr_last;
		}
		pr_info("CMA: memblock allocated %#zx@%#x#%#x\n",
			size_secure, paddr_start, alignment);
	} else {
		pr_err("No secure region is required");
		return 0;
	}

	if ((size_secure - size_req) > 0) {
		regions_secure->size += size_secure - size_req;
		pr_info("CMA: augmenting size of '%s' to %#zx\n",
					regions_secure->name, regions_secure->size);
	}

	do {
		reg_sec->reserved = 1;
		reg_sec->start = paddr_last;

		paddr_last = reg_sec->start + reg_sec->size;

		pr_info("CMA: Reserved 0x%08x@0x%08x for '%s'\n",
			reg_sec->size, reg_sec->start, reg_sec->name);
		ret = cma_early_region_register(reg_sec); /* 100% success */
	} while (reg_sec-- != regions_secure);

	memblock_free(paddr_last, alignment - size_secure);
	pr_info("CMA: returned %#zx@%#x to memblock\n",
				alignment - size_secure, paddr_last);

	return paddr_start;
}

static void __init exynos_reserve_mem(void)
{
#ifdef CONFIG_EXYNOS_DEV_SHARED_MEMORY
	size_t reserve_size = shm_get_phys_size();
	phys_addr_t reserve_base = shm_get_phys_base();
#endif
	phys_addr_t reg_start;

	static struct cma_region regions[] = {
#if defined(CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE) \
		&& CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE
		{
			.name = "ion",
			.size = CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
		},
#endif
#ifndef CONFIG_EXYNOS_HW_DRM
#if defined(CONFIG_ION_EXYNOS_MFC_WFD_SIZE) \
		&& CONFIG_ION_EXYNOS_MFC_WFD_SIZE
		{
			.name = "wfd_mfc",
			.size = CONFIG_ION_EXYNOS_MFC_WFD_SIZE * SZ_1K +
                    SZ_8K,
		},
#endif
#else
#if defined(CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD) \
		&& CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD
		{
			.name = "drm_g2d_wfd",
			.size = CONFIG_ION_EXYNOS_DRM_MEMSIZE_G2D_WFD *
				SZ_1K,
		},
#endif
#endif
#ifdef CONFIG_ION_EXYNOS_DRM_MFC_SH
		{
			.name = "drm_mfc_sh",
			.size = SZ_1M,
		},
#endif
		{
			.size = 0
		},
	};

#if defined(CONFIG_EXYNOS_HW_DRM)
	static struct cma_region regions_secure[] = {
#if defined(CONFIG_ION_EXYNOS_DRM_VIDEO) \
	&& CONFIG_ION_EXYNOS_DRM_VIDEO
		{
			.name = "drm_video",
			.size = CONFIG_ION_EXYNOS_DRM_VIDEO *
				SZ_1K,
			{
				.alignment = SZ_1M,
			}
		},
#endif

#if defined(CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT) \
		&& CONFIG_ION_EXYNOS_DRM_MEMSIZE_MFC_INPUT
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
#ifdef CONFIG_ION_EXYNOS_MEMSIZE_SECDMA
		{
			.name = "drm_secdma",
			.size = CONFIG_ION_EXYNOS_MEMSIZE_SECDMA * SZ_1K,
			{
				.alignment = SZ_1M,
			}
		},
#endif
		{
			.size = 0
		},
	};
#endif /* CONFIG_EXYNOS_HW_DRM */
	static const char map[] __initconst =
#ifdef CONFIG_EXYNOS_HW_DRM
		"ion-exynos/mfc_sh=drm_mfc_sh;"
		"ion-exynos/g2d_wfd=drm_g2d_wfd;"
		"ion-exynos/video=drm_video;"
		"ion-exynos/mfc_input=drm_mfc_input;"
		"ion-exynos/mfc_fw=drm_mfc_fw;"
		"ion-exynos/sectbl=drm_sectbl;"
		"ion-exynos/secdma=drm_secdma;"
		"s5p-smem/mfc_sh=drm_mfc_sh;"
		"s5p-smem/g2d_wfd=drm_g2d_wfd;"
		"s5p-smem/video=drm_video;"
		"s5p-smem/mfc_input=drm_mfc_input;"
		"s5p-smem/mfc_fw=drm_mfc_fw;"
		"s5p-smem/sectbl=drm_sectbl;"
		"s5p-smem/secdma=drm_secdma;"
#else
		"ion-exynos/mfc_sh=drm_mfc_sh;"
		"ion-exynos/wfd_mfc=wfd_mfc;"
#endif
		"ion-exynos=ion;"
		"ion-exynos=cp_shdmem;";

#ifdef CONFIG_EXYNOS_DEV_SHARED_MEMORY
	if (memblock_is_region_reserved(reserve_base, reserve_size) ||
			memblock_reserve(reserve_base, reserve_size))
		pr_err("Failed to reserve %#x@%#x\n", reserve_size,
			reserve_base);
#endif

#if defined(CONFIG_EXYNOS_HW_DRM)
	reg_start = universal222_cma_secure_region_reserve(regions_secure, map, SZ_128M);
#else
	reg_start = reserve_base + reserve_size;
#endif
	if (reg_start)
		universal222_cma_region_reserve(regions, map, reg_start,
				reg_start + SZ_256M);
}
#else /* !CONFIG_CMA */
static void exynos_reserve_mem(void)
{
}
#endif

static void __init smdk4270_map_io(void)
{
	clk_xusbxti.rate = 24000000;

	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(clk_xusbxti.rate);
	s3c24xx_init_uarts(smdk4270_uartcfgs, ARRAY_SIZE(smdk4270_uartcfgs));
#ifdef CONFIG_SEC_DEBUG
	sec_debug_init();
#endif
}

static struct persistent_ram_descriptor smdk3470_prd[] __initdata = {
        {
                .name = "ram_console",
                .size = SZ_2M,
        },
#ifdef CONFIG_PERSISTENT_TRACER
        {
                .name = "persistent_trace",
                .size = SZ_1M,
        },
#endif
};

static struct persistent_ram smdk3470_pr __initdata = {
        .descs = smdk3470_prd,
        .num_descs = ARRAY_SIZE(smdk3470_prd),
        .start = PLAT_PHYS_OFFSET + SZ_512M + SZ_256M,
#ifdef CONFIG_PERSISTENT_TRACER
        .size = 3 * SZ_1M,
#else
        .size = SZ_2M,
#endif
};

static void __init smdk3470_init_early(void)
{
#ifdef CONFIG_SEC_DEBUG
	sec_debug_magic_init();
#endif
        persistent_ram_early_init(&smdk3470_pr);
}

static void exynos4270_power_off(void)
{
	printk(KERN_EMERG "%s: set PS_HOLD low\n", __func__);

	__raw_writel(__raw_readl(EXYNOS_PS_HOLD_CONTROL)
				& 0xFFFFFEFF, EXYNOS_PS_HOLD_CONTROL);
	printk(KERN_EMERG "%s: Should not reach here\n", __func__);
}

static void __init universal222ap_machine_init(void)
{
	s3c_watchdog_set_platdata(&smdk4270_watchdog_platform_data);
#ifdef CONFIG_S3C_ADC
	s3c_adc_set_platdata(&smdk4270_adc_data);
#endif

	platform_add_devices(smdk4270_devices, ARRAY_SIZE(smdk4270_devices));

	pm_power_off = exynos4270_power_off;

	exynos4_smdk4270_power_init();
	exynos4_smdk4270_mmc0_init();
#if defined(CONFIG_MACH_KMINI) || defined(CONFIG_MACH_GARDA) ||\
	defined(CONFIG_MACH_DEGAS)
	brcm_wlan_init();
#else
	exynos4_smdk4270_mmc1_init();
#endif
	exynos4_smdk4270_mmc2_init();
	exynos4_smdk4270_usb_init();
	exynos4_smdk4270_media_init();
	exynos4_smdk4270_audio_init();
#if defined(CONFIG_BATTERY_SAMSUNG)
	exynos4_smdk4270_charger_init();
#endif
#if defined(CONFIG_MACH_KMINI)
	exynos4_smdk4270_mfd_init();
#endif
#if defined(CONFIG_SEC_THERMISTOR)
        board_sec_thermistor_init();
#endif
	board_universal_ss222ap_init_gpio();
#if defined(CONFIG_SAMSUNG_MUIC)
	exynos4_smdk4270_muic_init();
#endif
	exynos4_universal222ap_clock_init();
	exynos4_smdk4270_display_init();
	exynos4_smdk4270_input_init();
	exynos4_universal222ap_camera_init();
	board_universal_ss222ap_init_sensor();
	#if defined(CONFIG_LEDS_KTD2026)
	exynos4_smdk4270_led_init();
	#endif
        board_universal_ss222ap_init_fpga();
	board_universal_ss222ap_init_gpio_i2c();

#ifdef CONFIG_DC_MOTOR
#if defined(CONFIG_MACH_KMINI) || defined(CONFIG_MACH_DEGAS)
        dc_motor_init();
#endif
#endif

#ifdef CONFIG_W1
	exynos4_universal222ap_cover_id_init();
#endif

#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************/
	/* This function must be located in appropriate INIT position
	 * in accordance with the specification of each BB vendor.
	 */
	/************************ Caution !!! ****************************/
	gpio_dvs_check_initgpio();
#endif

	register_reboot_notifier(&exynos4_reboot_notifier);
}

MACHINE_START(UNIVERSAL3470, "UNIVERSAL3470")
	.atag_offset	= 0x100,
	.init_early	= smdk3470_init_early,
	.init_irq	= exynos5_init_irq,
	.map_io		= smdk4270_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal222ap_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos4_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END

