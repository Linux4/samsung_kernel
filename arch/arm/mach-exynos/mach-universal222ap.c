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
#include <linux/platform_data/modemif.h>

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

#ifdef CONFIG_S3C_ADC
/* ADC */
static struct s3c_adc_platdata smdk4270_adc_data __initdata = {
	.phy_init       = s3c_adc_phy_init,
	.phy_exit       = s3c_adc_phy_exit,
};
#endif

static struct platform_device *smdk4270_devices[] __initdata = {
	&ramconsole_device,
	&s3c_device_wdt,
#ifdef CONFIG_S3C_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_S5P_DEV_ACE
	&s5p_device_sss,
#endif
#ifdef CONFIG_MALI400
	&exynos4_device_g3d,
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
#define SHDMEM_BASE 0x60000000
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
	size_t reserve_size = shm_get_phys_size();
	phys_addr_t reserve_base = shm_get_phys_base();
	phys_addr_t reg_start;

	static struct cma_region regions[] = {
#if defined(CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE) \
		&& CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE
		{
			.name = "ion",
			.size = CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
		},
#endif
#ifndef CONFIG_MACH_KMINI
#if defined(CONFIG_ION_EXYNOS_MFC_WFD_SIZE) \
		&& CONFIG_ION_EXYNOS_MFC_WFD_SIZE
		{
			.name = "wfd_mfc",
			.size = CONFIG_ION_EXYNOS_MFC_WFD_SIZE * SZ_1K,
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

#if defined(CONFIG_MACH_KMINI)
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
#endif /* CONFIG_MACH_KMINIi */
	static const char map[] __initconst =
#ifdef CONFIG_MACH_KMINI
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
		"s5p-smem/mfc_sh=drm_mfc_sh;"
#endif
		"ion-exynos=ion;"
		"ion-exynos=cp_shdmem;";

	if (memblock_is_region_reserved(reserve_base, reserve_size) ||
			memblock_reserve(reserve_base, reserve_size))
		pr_err("Failed to reserve %#x@%#x\n", reserve_size,
			reserve_base);

#if defined(CONFIG_MACH_KMINI)
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

	exynos4_universal222ap_clock_init();
	exynos4_smdk4270_mmc0_init();
	exynos4_smdk4270_mmc1_init();
	exynos4_smdk4270_mmc2_init();
	exynos4_smdk4270_modemif_init();
	exynos4_smdk4270_modem_init();
	exynos4_smdk4270_power_init();
	exynos4_smdk4270_usb_init();
	board_universal_ss222ap_init_gpio();
	exynos4_smdk4270_muic_init();
	exynos4_smdk4270_display_init();
	exynos4_smdk4270_input_init();
	exynos4_smdk4270_media_init();
	exynos4_smdk4270_audio_init();
	exynos4_smdk4270_mfd_init();
	exynos4_universal222ap_camera_init();

	register_reboot_notifier(&exynos4_reboot_notifier);
}

MACHINE_START(UNIVERSAL_KMINI, "UNIVERSAL_KMINI")
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
