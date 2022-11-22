/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 * Author: steve.zhan <steve.zhan@spreadtrum.com>
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
#include <linux/init.h>
#include <linux/bug.h>

#include <asm/io.h>
#include <asm/page.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>
#include <mach/arch_misc.h>

#if defined(CONFIG_SEC_DEBUG)
/* For saving Fault status */
#include <mach/sec_debug.h>
#endif

#define SPRD_DEVICE(name) { \
	.virtual = SPRD_##name##_BASE, \
	.pfn = __phys_to_pfn(SPRD_##name##_PHYS), \
	.length = SPRD_##name##_SIZE, \
	.type = MT_DEVICE_NONSHARED, \
	}
#define SPRD_IRAM(name) { \
	.virtual = SPRD_##name##_BASE, \
	.pfn = __phys_to_pfn(SPRD_##name##_PHYS), \
	.length = SPRD_##name##_SIZE, \
	.type = MT_MEMORY, \
	}

#define ARCH_SC_SOC_IO_MAP
static struct map_desc sprd_io_desc[] __initdata = {	
	SPRD_DEVICE(CORE),
	SPRD_DEVICE(DMA0),
	SPRD_DEVICE(USB),
	SPRD_DEVICE(SDIO0),
	SPRD_DEVICE(SDIO1),
	SPRD_DEVICE(SDIO2),
	SPRD_DEVICE(EMMC),
	SPRD_DEVICE(DRM),
	SPRD_DEVICE(LCDC),
	SPRD_DEVICE(LCDC1),
	SPRD_DEVICE(GSP),
	SPRD_DEVICE(NFC),
	SPRD_DEVICE(AHB),
	SPRD_DEVICE(BM0),
	SPRD_DEVICE(BM1),
	SPRD_DEVICE(BM2),
	SPRD_DEVICE(DSI),
	SPRD_DEVICE(GPS),
	SPRD_DEVICE(LPDDR2),
	SPRD_DEVICE(LPDDR2_PHY),
	SPRD_DEVICE(PUB),
	SPRD_DEVICE(AXIBM0),
	SPRD_DEVICE(AXIBM1),
	SPRD_DEVICE(AXIBM2),
	SPRD_DEVICE(AXIBM3),
	SPRD_DEVICE(AXIBM4),
	SPRD_DEVICE(AXIBM5),
	SPRD_DEVICE(AXIBM6),
	SPRD_DEVICE(AXIBM7),
	SPRD_DEVICE(AXIBM8),
	SPRD_DEVICE(AXIBM9),
	SPRD_DEVICE(AUDIO),
	SPRD_DEVICE(AUDIO_IF),
	SPRD_DEVICE(VBC),
	SPRD_DEVICE(ADI),
	SPRD_DEVICE(ADISLAVE),
	SPRD_DEVICE(SYSTIMER_CMP),
	SPRD_DEVICE(GPTIMER0),
	SPRD_DEVICE(HWLOCK0),
	SPRD_DEVICE(HWLOCK1),
	SPRD_DEVICE(RFSPI),
	SPRD_DEVICE(I2C),
	SPRD_DEVICE(INT),
	SPRD_DEVICE(EIC),
	SPRD_DEVICE(APTIMER0),
	SPRD_DEVICE(SYSCNT),
	SPRD_DEVICE(UIDEFUSE),
	SPRD_DEVICE(KPD),
	SPRD_DEVICE(PWM),
	SPRD_DEVICE(FM),
	SPRD_DEVICE(GPIO),
	SPRD_DEVICE(WDG),
	SPRD_DEVICE(PIN),
	SPRD_DEVICE(PMU),
	SPRD_DEVICE(IPI),
	SPRD_DEVICE(AONCKG),
	SPRD_DEVICE(AONAPB),
	SPRD_DEVICE(THM),
	SPRD_DEVICE(AVSCA7),
	SPRD_DEVICE(AVSTOP),
	SPRD_DEVICE(CA7WDG),
	SPRD_DEVICE(APTIMER1),
	SPRD_DEVICE(APTIMER2),
	SPRD_DEVICE(CA7TS0),
	SPRD_DEVICE(CA7TS1),
	SPRD_DEVICE(MALI),
	SPRD_DEVICE(GPUAPB),
	SPRD_DEVICE(GPUCKG),
	SPRD_DEVICE(DCAM),
	SPRD_DEVICE(VSP),
	SPRD_DEVICE(ISP),
	SPRD_DEVICE(JPG),
	SPRD_DEVICE(CSI2),
	SPRD_DEVICE(MMAHB),
	SPRD_DEVICE(MMCKG),
	SPRD_DEVICE(UART0),
	SPRD_DEVICE(UART1),
	SPRD_DEVICE(UART2),
	SPRD_DEVICE(UART3),
	SPRD_DEVICE(UART4),
	SPRD_DEVICE(I2C0),
	SPRD_DEVICE(I2C1),
	SPRD_DEVICE(I2C2),
	SPRD_DEVICE(I2C3),
	SPRD_DEVICE(I2C4),
	SPRD_DEVICE(SPI0),
	SPRD_DEVICE(SPI1),
	SPRD_DEVICE(SPI2),
	SPRD_DEVICE(IIS0),
	SPRD_DEVICE(IIS1),
	SPRD_DEVICE(IIS2),
	SPRD_DEVICE(IIS3),
	SPRD_DEVICE(SIM0),
	SPRD_DEVICE(APBCKG),
	SPRD_DEVICE(APBREG),
	SPRD_DEVICE(INTC0),
	SPRD_DEVICE(INTC1),
	SPRD_DEVICE(INTC2),
	SPRD_DEVICE(INTC3),
	SPRD_IRAM(IRAM0),
	SPRD_IRAM(IRAM0H),
#if defined(CONFIG_ARCH_SCX30G)
	SPRD_IRAM(IRAM1),
#else
	SPRD_DEVICE(IRAM1),
#endif
	SPRD_DEVICE(IRAM2),
#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ARCH_SCX30G)
	SPRD_DEVICE(ZIPENC),
	SPRD_DEVICE(ZIPDEC),
#endif

#if defined(CONFIG_ARCH_SCX15)
	SPRD_DEVICE(GSPMMU),
	SPRD_DEVICE(MMMMU),
#endif

#if defined(CONFIG_ARCH_SCX30G)
	SPRD_DEVICE(GSPMMU),
	SPRD_DEVICE(MMMMU),
#endif

#if defined(CONFIG_SEC_DEBUG)
	SPRD_DEVICE(SECDEBUG),
#endif
};

void __init sci_map_io(void)
{
	iotable_init(sprd_io_desc, ARRAY_SIZE(sprd_io_desc));
	sc_init_chip_id();

}

