/*
 * Copyright (c) 2011-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Copyright (C) 2010 Samsung Electronics Co. Ltd.
 *	Jaswinder Singh <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/dma-mapping.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl330.h>
#include <linux/of.h>

#include <asm/irq.h>
#include <plat/devs.h>
#include <plat/irqs.h>
#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/irqs.h>
#include <mach/dma.h>

static u8 exynos4210_pdma0_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM2_RX,
	DMACH_PCM2_TX,
	DMACH_MSM_REQ0,
	DMACH_MSM_REQ2,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S2_RX,
	DMACH_I2S2_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_UART4_RX,
	DMACH_UART4_TX,
	DMACH_SLIMBUS0_RX,
	DMACH_SLIMBUS0_TX,
	DMACH_SLIMBUS2_RX,
	DMACH_SLIMBUS2_TX,
	DMACH_SLIMBUS4_RX,
	DMACH_SLIMBUS4_TX,
	DMACH_AC97_MICIN,
	DMACH_AC97_PCMIN,
	DMACH_AC97_PCMOUT,
};

static u8 exynos4212_pdma0_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM2_RX,
	DMACH_PCM2_TX,
	DMACH_MIPI_HSI0,
	DMACH_MIPI_HSI1,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S2_RX,
	DMACH_I2S2_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_UART4_RX,
	DMACH_UART4_TX,
	DMACH_SLIMBUS0_RX,
	DMACH_SLIMBUS0_TX,
	DMACH_SLIMBUS2_RX,
	DMACH_SLIMBUS2_TX,
	DMACH_SLIMBUS4_RX,
	DMACH_SLIMBUS4_TX,
	DMACH_AC97_MICIN,
	DMACH_AC97_PCMIN,
	DMACH_AC97_PCMOUT,
	DMACH_MIPI_HSI4,
	DMACH_MIPI_HSI5,
};

u8 exynos5250_pdma0_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM2_RX,
	DMACH_PCM2_TX,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S2_RX,
	DMACH_I2S2_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_UART4_RX,
	DMACH_UART4_TX,
	DMACH_SLIMBUS0_RX,
	DMACH_SLIMBUS0_TX,
	DMACH_SLIMBUS2_RX,
	DMACH_SLIMBUS2_TX,
	DMACH_SLIMBUS4_RX,
	DMACH_SLIMBUS4_TX,
	DMACH_AC97_MICIN,
	DMACH_AC97_PCMIN,
	DMACH_AC97_PCMOUT,
	DMACH_MIPI_HSI0,
	DMACH_MIPI_HSI2,
	DMACH_MIPI_HSI4,
	DMACH_MIPI_HSI6,
};

static u8 exynos5410_pdma0_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM2_RX,
	DMACH_PCM2_TX,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S2_RX,
	DMACH_I2S2_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_AC97_MICIN,
	DMACH_AC97_PCMIN,
	DMACH_AC97_PCMOUT,
	DMACH_MIPI_HSI0,
	DMACH_MIPI_HSI2,
	DMACH_MIPI_HSI4,
	DMACH_MIPI_HSI6,
};

static u8 exynos5420_pdma0_peri[] = {
	DMACH_MAX,
	DMACH_MAX,
	DMACH_PCM2_RX,
	DMACH_PCM2_TX,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_I2S2_RX,
	DMACH_I2S2_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_AC97_MICIN,
	DMACH_AC97_PCMIN,
	DMACH_AC97_PCMOUT,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
};

static u8 exynos4415_pdma0_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM2_RX,
	DMACH_PCM2_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_I2S0S_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_I2S2_RX,
	DMACH_I2S2_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_UART4_RX,
	DMACH_UART4_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_AC_MICIN,
	DMACH_AC_PCMIN,
	DMACH_AC_PCMOUT,
	DMACH_MAX,
	DMACH_MAX,
};

static struct dma_pl330_platdata exynos_pdma0_pdata;

static AMBA_AHB_DEVICE(exynos_pdma0, "dma-pl330.0", 0x00041330,
	EXYNOS5_PA_PDMA0, {EXYNOS5_IRQ_PDMA0}, &exynos_pdma0_pdata);

static u8 exynos5260_pdma0_peri[] = {
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_HSI2C0_RX,
	DMACH_HSI2C0_TX,
	DMACH_HSI2C1_RX,
	DMACH_HSI2C1_TX,
	DMACH_HSI2C2_RX,
	DMACH_HSI2C2_TX,
	DMACH_HSI2C3_RX,
	DMACH_HSI2C3_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_PCM1_RX,
	DMACH_PCM1_TX,
	DMACH_SPDIF,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
};

static struct dma_pl330_platdata exynos5260_pdma0_pdata;

static AMBA_AHB_DEVICE(exynos5260_pdma0, "dma-pl330.0", 0x00041330,
	EXYNOS5260_PA_PDMA0, {EXYNOS5260_IRQ_PDMA0}, &exynos5260_pdma0_pdata);

static u8 exynos4210_pdma1_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM1_RX,
	DMACH_PCM1_TX,
	DMACH_MSM_REQ1,
	DMACH_MSM_REQ3,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_UART3_RX,
	DMACH_UART3_TX,
	DMACH_SLIMBUS1_RX,
	DMACH_SLIMBUS1_TX,
	DMACH_SLIMBUS3_RX,
	DMACH_SLIMBUS3_TX,
	DMACH_SLIMBUS5_RX,
	DMACH_SLIMBUS5_TX,
};

static u8 exynos4212_pdma1_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM1_RX,
	DMACH_PCM1_TX,
	DMACH_MIPI_HSI2,
	DMACH_MIPI_HSI3,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_UART3_RX,
	DMACH_UART3_TX,
	DMACH_SLIMBUS1_RX,
	DMACH_SLIMBUS1_TX,
	DMACH_SLIMBUS3_RX,
	DMACH_SLIMBUS3_TX,
	DMACH_SLIMBUS5_RX,
	DMACH_SLIMBUS5_TX,
	DMACH_SLIMBUS0AUX_RX,
	DMACH_SLIMBUS0AUX_TX,
	DMACH_SPDIF,
	DMACH_MIPI_HSI6,
	DMACH_MIPI_HSI7,
};

u8 exynos5250_pdma1_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM1_RX,
	DMACH_PCM1_TX,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_PWM,
	DMACH_SPDIF,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_UART3_RX,
	DMACH_UART3_TX,
	DMACH_SLIMBUS1_RX,
	DMACH_SLIMBUS1_TX,
	DMACH_SLIMBUS3_RX,
	DMACH_SLIMBUS3_TX,
	DMACH_SLIMBUS5_RX,
	DMACH_SLIMBUS5_TX,
	DMACH_SLIMBUS0AUX_RX,
	DMACH_SLIMBUS0AUX_TX,
	DMACH_DISP1,
	DMACH_MIPI_HSI1,
	DMACH_MIPI_HSI3,
	DMACH_MIPI_HSI5,
	DMACH_MIPI_HSI7,
};

static u8 exynos5410_pdma1_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM1_RX,
	DMACH_PCM1_TX,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_PWM,
	DMACH_SPDIF,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_UART3_RX,
	DMACH_UART3_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_DISP1,
	DMACH_MIPI_HSI1,
	DMACH_MIPI_HSI3,
	DMACH_MIPI_HSI5,
	DMACH_MIPI_HSI7,
};

static u8 exynos5420_pdma1_peri[] = {
	DMACH_MAX,
	DMACH_MAX,
	DMACH_PCM1_RX,
	DMACH_PCM1_TX,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_MAX,
	DMACH_SPDIF,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_UART3_RX,
	DMACH_UART3_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_DISP1,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
};

static u8 exynos4415_pdma1_peri[] = {
	DMACH_PCM0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM2_RX,
	DMACH_PCM2_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_SPDIF,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
};

struct dma_pl330_platdata exynos_pdma1_pdata;

static AMBA_AHB_DEVICE(exynos_pdma1,  "dma-pl330.1", 0x00041330,
	EXYNOS5_PA_PDMA1, {EXYNOS5_IRQ_PDMA1}, &exynos_pdma1_pdata);

static u8 mdma_peri[] = {
	DMACH_MTOM_0,
	DMACH_MTOM_1,
	DMACH_MTOM_2,
	DMACH_MTOM_3,
	DMACH_MTOM_4,
	DMACH_MTOM_5,
	DMACH_MTOM_6,
	DMACH_MTOM_7,
};

static struct dma_pl330_platdata exynos_mdma_pdata = {
	.nr_valid_peri = ARRAY_SIZE(mdma_peri),
	.peri_id = mdma_peri,
};

static AMBA_AHB_DEVICE(exynos_mdma,  "dma-pl330.2", 0x00041330,
	EXYNOS5_PA_MDMA1, {EXYNOS5_IRQ_MDMA1}, &exynos_mdma_pdata);

static u8 exynos5260_mdma_peri[] = {
	DMACH_MTOM_0,
	DMACH_MTOM_1,
	DMACH_MTOM_2,
	DMACH_MTOM_3,
};

static struct dma_pl330_platdata exynos5260_mdma_pdata = {
	.nr_valid_peri = ARRAY_SIZE(exynos5260_mdma_peri),
	.peri_id = exynos5260_mdma_peri,
};

static AMBA_AHB_DEVICE(exynos5260_mdma,  "dma-pl330.1", 0x00041330,
	EXYNOS5260_PA_NS_MDMA0, {EXYNOS5260_IRQ_MDMA_1}, &exynos5260_mdma_pdata);

static u8 adma0_peri[] = {
	DMACH_I2S0_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S0_RX,
	DMACH_PCM0_TX,
	DMACH_PCM0_RX,
	DMACH_MAX,
};

static u8 exynos4415_adma0_peri[] = {
	DMACH_SLIMBUS_CH0,
	DMACH_SLIMBUS_CH1,
	DMACH_SLIMBUS_CH2,
	DMACH_SLIMBUS_CH3,
	DMACH_SLIMBUS_CH4,
	DMACH_SLIMBUS_CH5,
	DMACH_SLIMBUS_CH6,
	DMACH_SLIMBUS_CH7,
};

static struct dma_pl330_platdata exynos_adma0_pdata = {
	.nr_valid_peri = ARRAY_SIZE(adma0_peri),
	.peri_id = adma0_peri,
};

static AMBA_AHB_DEVICE(exynos_adma0,  "dma-pl330.3", 0x00041330,
	EXYNOS5_PA_ADMA0, {EXYNOS5_IRQ_ADMA0}, &exynos_adma0_pdata);

static u8 exynos5260_adma_peri[] = {
	DMACH_PL080_I2S_TXP,
	DMACH_PL080_I2S_TXS,
	DMACH_PL080_I2S_RX,
	DMACH_PL080_PCM_TX,
	DMACH_PL080_PCM_RX,
	DMACH_PL080_UART_TX,
	DMACH_PL080_UART_RX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
	DMACH_MAX,
};

static struct dma_pl330_platdata exynos5260_adma_pdata = {
	.nr_valid_peri = ARRAY_SIZE(exynos5260_adma_peri),
	.peri_id = exynos5260_adma_peri,
};

static AMBA_AHB_DEVICE(exynos5260_adma,  "dma-pl330.2", 0x00041330,
	EXYNOS5260_PA_ADMA0, {EXYNOS5260_IRQ_ADMA}, &exynos5260_adma_pdata);

static int __init exynos_dma_init(void)
{
#ifdef CONFIG_MACH_UNIVERSAL4415
	void __iomem *pdma_tz;
	void __iomem *i2s_tz;
#endif

	if (of_have_populated_dt())
		return 0;

#ifdef CONFIG_MACH_UNIVERSAL4415
	pdma_tz = ioremap(0x10140000, SZ_4K);
	i2s_tz = ioremap(0x10160000, SZ_4K);

	__raw_writel(0x2, pdma_tz + 0x81c);
	__raw_writel(0x1, i2s_tz + 0x81c);
#endif

	if (soc_is_exynos4210()) {
		exynos_pdma0_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos4210_pdma0_peri);
		exynos_pdma0_pdata.peri_id = exynos4210_pdma0_peri;
		exynos_pdma1_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos4210_pdma1_peri);
		exynos_pdma1_pdata.peri_id = exynos4210_pdma1_peri;
	} else if (soc_is_exynos4212() || soc_is_exynos4412()) {
		exynos_pdma0_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos4212_pdma0_peri);
		exynos_pdma0_pdata.peri_id = exynos4212_pdma0_peri;
		exynos_pdma1_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos4212_pdma1_peri);
		exynos_pdma1_pdata.peri_id = exynos4212_pdma1_peri;
	} else if (soc_is_exynos4415()) {
		exynos_pdma0_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos4415_pdma0_peri);
		exynos_pdma0_pdata.peri_id = exynos4415_pdma0_peri;
		exynos_pdma1_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos4415_pdma1_peri);
		exynos_pdma1_pdata.peri_id = exynos4415_pdma1_peri;
		exynos_adma0_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos4415_adma0_peri);
		exynos_adma0_pdata.peri_id = exynos4415_adma0_peri;
	} else if (soc_is_exynos5250()) {
		exynos_pdma0_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos5250_pdma0_peri);
		exynos_pdma0_pdata.peri_id = exynos5250_pdma0_peri;
		exynos_pdma1_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos5250_pdma1_peri);
		exynos_pdma1_pdata.peri_id = exynos5250_pdma1_peri;
	} else if (soc_is_exynos5410()) {
		exynos_pdma0_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos5410_pdma0_peri);
		exynos_pdma0_pdata.peri_id = exynos5410_pdma0_peri;
		exynos_pdma1_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos5410_pdma1_peri);
		exynos_pdma1_pdata.peri_id = exynos5410_pdma1_peri;
	} else if (soc_is_exynos5420()) {
		exynos_pdma0_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos5420_pdma0_peri);
		exynos_pdma0_pdata.peri_id = exynos5420_pdma0_peri;
		exynos_pdma1_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos5420_pdma1_peri);
		exynos_pdma1_pdata.peri_id = exynos5420_pdma1_peri;
		exynos_adma0_pdata.nr_valid_peri =
			ARRAY_SIZE(adma0_peri);
		exynos_adma0_pdata.peri_id = adma0_peri;
	} else if (soc_is_exynos5260()) {
		exynos5260_pdma0_pdata.nr_valid_peri =
			ARRAY_SIZE(exynos5260_pdma0_peri);
		exynos5260_pdma0_pdata.peri_id = exynos5260_pdma0_peri;
	}

	if (soc_is_exynos4210() || soc_is_exynos4212() || soc_is_exynos4412()) {
		exynos_pdma0_device.res.start = EXYNOS4_PA_PDMA0;
		exynos_pdma0_device.res.end = EXYNOS4_PA_PDMA0 + SZ_4K;
		exynos_pdma0_device.irq[0] = EXYNOS4_IRQ_PDMA0;
		exynos_pdma1_device.res.start = EXYNOS4_PA_PDMA1;
		exynos_pdma1_device.res.end = EXYNOS4_PA_PDMA1 + SZ_4K;
		exynos_pdma1_device.irq[0] = EXYNOS4_IRQ_PDMA1;
		exynos_mdma_device.res.start = EXYNOS4_PA_MDMA1;
		exynos_mdma_device.res.end = EXYNOS4_PA_MDMA1 + SZ_4K;
		exynos_mdma_device.irq[0] = EXYNOS4_IRQ_MDMA1;
	} else if (soc_is_exynos5410() || soc_is_exynos5420()) {
		exynos_mdma_device.res.start = EXYNOS5_PA_MDMA0;
		exynos_mdma_device.res.end = EXYNOS5_PA_MDMA0 + SZ_4K;
		exynos_mdma_device.irq[0] = EXYNOS5_IRQ_MDMA0;
	} else if (soc_is_exynos5260()) {
		exynos5260_pdma0_device.res.start = EXYNOS5260_PA_PDMA0;
		exynos5260_pdma0_device.res.end = EXYNOS5260_PA_PDMA0 + SZ_4K;
		exynos5260_pdma0_device.irq[0] = EXYNOS5260_IRQ_PDMA0;
		exynos5260_mdma_device.res.start = EXYNOS5260_PA_NS_MDMA0;
		exynos5260_mdma_device.res.end = EXYNOS5260_PA_NS_MDMA0 + SZ_4K;
		exynos5260_mdma_device.irq[0] = EXYNOS5260_IRQ_MDMA_1;
		exynos5260_adma_device.res.start = EXYNOS5260_PA_ADMA0;
		exynos5260_adma_device.res.end = EXYNOS5260_PA_ADMA0 + SZ_4K;
		exynos5260_adma_device.irq[0] = EXYNOS5260_IRQ_ADMA;
	} else if (soc_is_exynos4415()) {
		exynos_pdma0_device.res.start = EXYNOS4_PA_PDMA0;
		exynos_pdma0_device.res.end = EXYNOS4_PA_PDMA0 + SZ_4K;
		exynos_pdma0_device.irq[0] = EXYNOS4_IRQ_PDMA0;
		exynos_pdma1_device.res.start = EXYNOS4_PA_PDMA1;
		exynos_pdma1_device.res.end = EXYNOS4_PA_PDMA1 + SZ_4K;
		exynos_pdma1_device.irq[0] = EXYNOS4_IRQ_PDMA1;
		exynos_mdma_device.res.start = EXYNOS4_PA_MDMA1;
		exynos_mdma_device.res.end = EXYNOS4_PA_MDMA1 + SZ_4K;
		exynos_mdma_device.irq[0] = EXYNOS4_IRQ_MDMA1;
		exynos_adma0_device.res.start = EXYNOS4415_PA_ADMA;
		exynos_adma0_device.res.end = EXYNOS4415_PA_ADMA + SZ_4K;
#ifdef CONFIG_SOC_EXYNOS4415
		exynos_adma0_device.irq[0] = EXYNOS4_IRQ_ADMA;
#endif
	}

	if (soc_is_exynos5260()) {
		dma_cap_set(DMA_SLAVE, exynos5260_pdma0_pdata.cap_mask);
		dma_cap_set(DMA_CYCLIC, exynos5260_pdma0_pdata.cap_mask);
		amba_device_register(&exynos5260_pdma0_device, &iomem_resource);

		dma_cap_set(DMA_MEMCPY, exynos5260_mdma_pdata.cap_mask);
		amba_device_register(&exynos5260_mdma_device, &iomem_resource);

		dma_cap_set(DMA_SLAVE, exynos5260_adma_pdata.cap_mask);
		dma_cap_set(DMA_CYCLIC, exynos5260_adma_pdata.cap_mask);
		amba_device_register(&exynos5260_adma_device, &iomem_resource);
	} else {
		dma_cap_set(DMA_SLAVE, exynos_pdma0_pdata.cap_mask);
		dma_cap_set(DMA_CYCLIC, exynos_pdma0_pdata.cap_mask);
		amba_device_register(&exynos_pdma0_device, &iomem_resource);

		dma_cap_set(DMA_SLAVE, exynos_pdma1_pdata.cap_mask);
		dma_cap_set(DMA_CYCLIC, exynos_pdma1_pdata.cap_mask);
		amba_device_register(&exynos_pdma1_device, &iomem_resource);

		dma_cap_set(DMA_MEMCPY, exynos_mdma_pdata.cap_mask);
		amba_device_register(&exynos_mdma_device, &iomem_resource);
	}

	if (soc_is_exynos5420() || soc_is_exynos4415()) {
		dma_cap_set(DMA_SLAVE, exynos_adma0_pdata.cap_mask);
		dma_cap_set(DMA_CYCLIC, exynos_adma0_pdata.cap_mask);
		amba_device_register(&exynos_adma0_device, &iomem_resource);
	}

	return 0;
}
arch_initcall(exynos_dma_init);
