/* arch\arm\mach-sc\board-teddy3g\board-teddy3g-audio.c
 *
 * Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc\board-teddy3g\board-teddy3g-audio.c
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

#include <linux/gpio.h>
#include <linux/headset_sprd.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/adc.h>
#include <mach/dma.h>
#include <mach/i2s.h>

static struct platform_device sprd_audio_vbc_r2p0_sprd_codec_v3_device = {
	.name		= "vbc-r2p0-sprd-codec-v3",
	.id		= -1,
};

static struct platform_device sprd_audio_i2s_null_codec_device = {
	.name		= "i2s-null-codec",
	.id		= -1,
};

static struct platform_device sprd_audio_platform_pcm_device = {
	.name		= "sprd-pcm-audio",
	.id		= -1,
};

static struct platform_device sprd_audio_vaudio_device = {
	.name		= "vaudio",
	.id		= -1,
};

static struct platform_device sprd_audio_vbc_r2p0_device = {
	.name		= "vbc-r2p0",
	.id		= -1,
};

static struct resource sprd_i2s0_resources[] = {
		[0] = {
				.start = SPRD_IIS0_BASE,
				.end   = SPRD_IIS0_BASE + SZ_4K - 1,
				.flags = IORESOURCE_MEM,
		},
		[1] = {
				.start = SPRD_IIS0_PHYS,
				.end   = SPRD_IIS0_PHYS + SZ_4K - 1,
				.flags = IORESOURCE_MEM,
		},
		[2] = {
				.start = DMA_IIS_TX,
				.end   = DMA_IIS_RX,
				.flags = IORESOURCE_DMA,
		},
};

static struct resource sprd_i2s1_resources[] = {
		[0] = {
				.start = SPRD_IIS1_BASE,
				.end   = SPRD_IIS1_BASE + SZ_4K - 1,
				.flags = IORESOURCE_MEM,
		},
		[1] = {
				.start = SPRD_IIS1_PHYS,
				.end   = SPRD_IIS1_PHYS + SZ_4K - 1,
				.flags = IORESOURCE_MEM,
		},
		[2] = {
				.start = DMA_IIS1_TX,
				.end   = DMA_IIS1_RX,
				.flags = IORESOURCE_DMA,
		},
};

static struct resource sprd_i2s2_resources[] = {
		[0] = {
				.start = SPRD_IIS2_BASE,
				.end   = SPRD_IIS2_BASE + SZ_4K - 1,
				.flags = IORESOURCE_MEM,
		},
		[1] = {
				.start = SPRD_IIS2_PHYS,
				.end   = SPRD_IIS2_PHYS + SZ_4K - 1,
				.flags = IORESOURCE_MEM,
		},
		[2] = {
				.start = DMA_IIS2_TX,
				.end   = DMA_IIS2_RX,
				.flags = IORESOURCE_DMA,
		},
};

static struct resource sprd_i2s3_resources[] = {
		[0] = {
				.start = SPRD_IIS3_BASE,
				.end   = SPRD_IIS3_BASE + SZ_4K - 1,
				.flags = IORESOURCE_MEM,
		},
		[1] = {
				.start = SPRD_IIS3_PHYS,
				.end   = SPRD_IIS3_PHYS + SZ_4K - 1,
				.flags = IORESOURCE_MEM,
		},
		[2] = {
				.start = DMA_IIS3_TX,
				.end   = DMA_IIS3_RX,
				.flags = IORESOURCE_DMA,
		},
};

static struct platform_device sprd_audio_i2s0_device = {
	.name		= "i2s",
	.id		= 0,
	.num_resources  = ARRAY_SIZE(sprd_i2s0_resources),
	.resource       = sprd_i2s0_resources,
};

static struct platform_device sprd_audio_i2s1_device = {
	.name		= "i2s",
	.id		= 1,
	.num_resources  = ARRAY_SIZE(sprd_i2s1_resources),
	.resource       = sprd_i2s1_resources,
};

static struct platform_device sprd_audio_i2s2_device = {
	.name		= "i2s",
	.id		= 2,
	.num_resources  = ARRAY_SIZE(sprd_i2s2_resources),
	.resource       = sprd_i2s2_resources,
};

static struct platform_device sprd_audio_i2s3_device = {
	.name		= "i2s",
	.id		= 3,
	.num_resources  = ARRAY_SIZE(sprd_i2s3_resources),
	.resource       = sprd_i2s3_resources,
};

static struct platform_device sprd_audio_sprd_codec_v3_device = {
	.name		= "sprd-codec-v3",
	.id		= -1,
};

static struct platform_device sprd_audio_null_codec_device = {
	.name		= "null-codec",
	.id		= -1,
};

static struct platform_device *late_devices[] __initdata = {
	/* 1. CODECS */
	&sprd_audio_sprd_codec_v3_device,
	&sprd_audio_null_codec_device,

	/* 2. CPU DAIS */
	&sprd_audio_vbc_r2p0_device,
	&sprd_audio_vaudio_device,
	&sprd_audio_i2s0_device,
	&sprd_audio_i2s1_device,
	&sprd_audio_i2s2_device,
	&sprd_audio_i2s3_device,

	/* 3. PLATFORM */
	&sprd_audio_platform_pcm_device,

	/* 4. MACHINE */
	&sprd_audio_vbc_r2p0_sprd_codec_v3_device,
	&sprd_audio_i2s_null_codec_device,
};

/*you can change param here to config the i2s modules*/
static struct i2s_config i2s0_config = {0};
static struct i2s_config i2s1_config = {0};
static struct i2s_config i2s2_config = {0};
static struct i2s_config i2s3_config = {0};

static struct headset_buttons sprd_headset_buttons[] = {
	{
		.adc_min = 00,
		.adc_max = 200,
		.code = KEY_MEDIA,
	},
	{
		.adc_min = 201,
		.adc_max = 700,
		.code = KEY_VOLUMEUP,
	},
	{
		.adc_min = 701,
		.adc_max = 1300,
		.code = KEY_VOLUMEDOWN,
	},
};

static struct sprd_headset_platform_data sprd_headset_pdata = {
	.gpio_switch = HEADSET_SWITCH_GPIO,
	.gpio_detect = HEADSET_DETECT_GPIO,
	.gpio_button = HEADSET_BUTTON_GPIO,
	.irq_trigger_level_detect = 0,
	.irq_trigger_level_button = 1,
	.headset_buttons = sprd_headset_buttons,
	.nbuttons = ARRAY_SIZE(sprd_headset_buttons),
};

static struct platform_device sprd_headset_device = {
	.name = "headset-detect",
	.id = -1,
	.dev = {
		.platform_data = &sprd_headset_pdata,
	},
};

static int teddy3g_add_misc_audio_devices(void)
{
	/* REVISIT:
	 * platform device (audio_pa_amplifier_device) might
	 * have to be added in the future
	 */
	return 0;
}

void teddy3g_audio_init_late(void)
{
	platform_add_devices(late_devices, ARRAY_SIZE(late_devices));
}

void teddy3g_audio_init_early(void)
{
	sci_adi_init();
}

void teddy3g_audio_init(void)
{
	sci_adc_init((void __iomem *)ADC_BASE);

	platform_device_add_data(&sprd_audio_i2s0_device,
			(const void *)&i2s0_config, sizeof(i2s0_config));
	platform_device_add_data(&sprd_audio_i2s1_device,
			(const void *)&i2s1_config, sizeof(i2s1_config));
	platform_device_add_data(&sprd_audio_i2s2_device,
			(const void *)&i2s2_config, sizeof(i2s2_config));
	platform_device_add_data(&sprd_audio_i2s3_device,
			(const void *)&i2s3_config, sizeof(i2s3_config));

	/* headset init */
	teddy3g_add_misc_audio_devices();

	if (platform_device_register(&sprd_headset_device) < 0)
		pr_err("%s: fail to register audio headset device\n", __func__);
}
