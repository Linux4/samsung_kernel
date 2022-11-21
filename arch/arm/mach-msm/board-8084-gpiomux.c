/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <soc/qcom/socinfo.h>

#if defined (CONFIG_SEC_NFC_SENN3AB)
#include <mach/tlte_felica_gpio.h>
#endif

extern unsigned int system_rev;


static struct gpiomux_setting gpio_suspend_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,  /* IN-NP */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* O-LOW */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,	/* IN-PD */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* OUT-PD */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
		.dir = GPIOMUX_OUT_LOW,
	},
};

static struct gpiomux_setting ap2mdm_status_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting mdm2ap_status_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting ap2mdm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};
static struct gpiomux_setting mdm2ap_errfatal_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting mdm2ap_pblrdy = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};


static struct gpiomux_setting ap2mdm_soft_reset_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ap2mdm_wakeup = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

#if !defined(CONFIG_SND_SOC_MAX98504) && !defined(CONFIG_SND_SOC_MAX98504A)
static struct gpiomux_setting gpio_epm_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};
#endif
#if defined (CONFIG_SND_SOC_ES705)
static struct gpiomux_setting es705_intrevent_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
#endif

static struct gpiomux_setting taiko_reset = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting hall_sensor_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hall_sensor_suspended_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

#ifndef CONFIG_SEC_MHL_SII8620
static struct gpiomux_setting swi2c_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
#endif

static struct msm_gpiomux_config hall_sensor_config[] __initdata = {
	{
		.gpio = 48,
		.settings = {
			[GPIOMUX_ACTIVE] = &hall_sensor_active_config,
			[GPIOMUX_SUSPENDED] = &hall_sensor_suspended_config,
		},
	},
};

static struct msm_gpiomux_config mdm_configs[] __initdata = {
	/* APQ_CTI_PAIR1_OUT */
	{
		.gpio = 63,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* AP2MDM_STATUS */
	{
		.gpio = 110,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_status_cfg,
		}
	},
	/* MDM2AP_STATUS */
	{
		.gpio = 109,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_cfg,
		}
	},
	/* MDM2AP_ERRFATAL */
	{
		.gpio = 111,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_errfatal_cfg,
		}
	},
	/* AP2MDM_ERRFATAL */
	{
		.gpio = 112,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* AP2MDM_VDDMIN */
	{
		.gpio = 114,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
#if !defined(CONFIG_SEC_TRLTE_JPN) && !defined(CONFIG_SEC_TBLTE_JPN)
	/* APQ_CTI_PAIR2_OUT */
	{
		.gpio = 124,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
#endif	
	/* AP2MDM_SOFT_RESET, aka AP2MDM_PON_RESET_N */
	{
		.gpio = 128,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_soft_reset_cfg,
		}
	},
	/* AP2MDM_WAKEUP */
	{
		.gpio = 108,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_wakeup,
		}
	},
	/* MDM2AP_PBL_READY*/
	{
		.gpio = 113,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_pblrdy,
		}
	},
};

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

#if !defined(CONFIG_SEC_KCCAT6_PROJECT)
static struct gpiomux_setting synaptics_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting synaptics_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif

static struct gpiomux_setting synaptics_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting synaptics_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_uart_config = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

#if defined(CONFIG_SEC_KCCAT6_PROJECT)
static struct gpiomux_setting es705_main_mic_bias_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting es705_main_mic_bias_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};
#endif

#if defined(CONFIG_SEC_LENTIS_PROJECT) || defined(CONFIG_SEC_KCCAT6_PROJECT)
static struct gpiomux_setting gpio_fuel_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
#endif

static struct gpiomux_setting gpio_uart_rx_config = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#if defined (CONFIG_SND_SOC_ES705)
static struct gpiomux_setting es705_gpio_uart_rx_config = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_UP,
};
#endif
static struct msm_gpiomux_config msm_synaptics_configs[] __initdata = {
	{
		.gpio = 143,
		.settings = {
			[GPIOMUX_ACTIVE] = &synaptics_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &synaptics_int_sus_cfg,
		},
	},
#if !defined(CONFIG_SEC_KCCAT6_PROJECT)	
	{
		.gpio = 145,
		.settings = {
			[GPIOMUX_ACTIVE] = &synaptics_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &synaptics_reset_sus_cfg,
		},
	},
#endif
};

#if defined(CONFIG_SEC_LENTIS_PROJECT)
static struct msm_gpiomux_config msm_synaptics_configs_rev10[] __initdata = {
	{
		.gpio = 143,
		.settings = {
			[GPIOMUX_ACTIVE] = &synaptics_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &synaptics_int_sus_cfg,
		},
	},
	{
		.gpio = 121,
		.settings = {
			[GPIOMUX_ACTIVE] = &synaptics_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &synaptics_reset_sus_cfg,
		},
	},
};
#endif

static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_uart_gps_liquid_config = {
	.func = GPIOMUX_FUNC_1,
	.drv  = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting gpio97_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

#if defined(CONFIG_SENSORS_SSP_ATMEL) || defined(CONFIG_SENSORS_SSP_STM)
static struct gpiomux_setting gpio_spi5_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_spi5_sus_config[] = {
	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_2,
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},
};
#endif


#ifdef CONFIG_SEC_MHL_SII8620
static struct gpiomux_setting gpio_mhl_spi_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif
#ifdef CONFIG_SEC_MHL_SII8240
static struct gpiomux_setting mhl_active_cfg = {
	.func = GPIOMUX_FUNC_3,
	/*
	 * Please keep I2C GPIOs drive-strength at minimum (2ma). It is a
	 * workaround for HW issue of glitches caused by rapid GPIO current-
	 * change.
	 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting mhl_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,  /* IN-PD */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mhl_active_2_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
	{
		.gpio      = 0,		/* BLSP1 QUP1 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 1,		/* BLSP1 QUP1 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 2,		/* BLSP1 QUP1 SPI_CS0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 3,		/* BLSP1 QUP1 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
#ifdef CONFIG_SEC_MHL_SII8620
	{
		.gpio      = 4,		/* BLSP1 QUP1 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_mhl_spi_config,
		},
	},
	{
		.gpio      = 5,		/* BLSP1 QUP1 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_mhl_spi_config,
		},
	},
	{
		.gpio      = 6,		/* BLSP1 QUP1 SPI_CS_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_mhl_spi_config,
		},
	},
	{
		.gpio      = 7,		/* BLSP1 QUP1 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_mhl_spi_config,
		},
	},
#else
	{
		.gpio      = 4,
		.settings = {
			[GPIOMUX_ACTIVE] = &swi2c_config,
			[GPIOMUX_SUSPENDED] = &swi2c_config,
		},
	},
	{
		.gpio      = 5,
		.settings = {
			[GPIOMUX_ACTIVE] = &swi2c_config,
			[GPIOMUX_SUSPENDED] = &swi2c_config,
		},
	},
#endif
	{
		.gpio      = 29,		/* BLSP1 QUP4 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 30,		/* BLSP1 QUP4 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
#if defined(CONFIG_SENSORS_SSP_ATMEL) || defined(CONFIG_SENSORS_SSP_STM)
	{
		.gpio	   = 39,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi5_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi5_sus_config[2],
		},
	},
	{
		.gpio	   = 40,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi5_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi5_sus_config[2],
		},
	},
	{
		.gpio	   = 41,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi5_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi5_sus_config[1],
		},
	},
	{
		.gpio	   = 42,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi5_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi5_sus_config[0],
		},
	},
#else
	{
		.gpio      = 41,		/* BLSP1 QUP5 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 42,		/* BLSP1 QUP5 ISC_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
#endif
	{
		.gpio      = 51,		/* BLSP2 UART1 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 52,		/* BLSP2 UART1 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_rx_config,
		},
	},
	{
		.gpio      = 61,		/* BLSP1 QUP2 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 62,		/* BLSP1 QUP2 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
#if defined(CONFIG_SEC_LENTIS_PROJECT) || defined(CONFIG_SEC_KCCAT6_PROJECT)
	{
		.gpio      = 84,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_fuel_int_config,
			[GPIOMUX_SUSPENDED] = &gpio_fuel_int_config,
		},
	},
#endif
	{
		.gpio      = 118,		/* BLSP1 QUP1 SPI_CS3 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 97,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio97_cfg,
			[GPIOMUX_SUSPENDED] = &gpio97_cfg,
		},
	},
#if defined(CONFIG_SEC_TBLTE_JPN)
	{
		.gpio = 123,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_suspend_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		}
	},
#endif
};

static struct msm_gpiomux_config msm_sbc_blsp_configs[] __initdata = {
	{
		.gpio      = 2,		/* BLSP1 QUP0 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 3,		/* BLSP1 QUP0 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
#ifdef CONFIG_SEC_MHL_SII8620
	{
		.gpio      = 4,		/* BLSP1 QUP1 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 5,		/* BLSP1 QUP1 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 6,		/* BLSP1 QUP1 SPI_CS_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 7,		/* BLSP1 QUP1 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
#endif
	{
		.gpio      = 10,		/* BLSP1 QUP2 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 11,		/* BLSP1 QUP2 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 49,		/* BLSP2 QUP5 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 50,		/* BLSP2 QUP5 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 51,		/* BLSP2 UART1 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 52,		/* BLSP2 UART1 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 61,		/* BLSP2 QUP3 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},

	{
		.gpio      = 62,		/* BLSP2 QUP3 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	}
};

#if defined (CONFIG_SND_SOC_ES705)

static struct msm_gpiomux_config es705_config[] __initdata = {
	{
		.gpio	= 59,		/* es705 uart tx */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio	= 60,		/* es705 uart rx */
		.settings = {
			[GPIOMUX_SUSPENDED] = &es705_gpio_uart_rx_config,
		},
	},
	{
		.gpio	= 91,		/* es705 2mic int */
		.settings = {
			[GPIOMUX_SUSPENDED] = &es705_intrevent_config,
		},
	},
	{
		.gpio	= 76,		/* es705 intr event */
		.settings = {
			[GPIOMUX_SUSPENDED] = &es705_intrevent_config,
		},
	},
#if defined (CONFIG_SEC_LENTIS_PROJECT) || defined(CONFIG_SEC_KCCAT6_PROJECT)
	{
		.gpio = 117,		/* 2MIC_RESET */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
#if defined(CONFIG_SEC_KCCAT6_PROJECT)
	{
		.gpio = 145,
		.settings = {
			[GPIOMUX_ACTIVE] = &es705_main_mic_bias_active_config,
			[GPIOMUX_SUSPENDED] = &es705_main_mic_bias_suspend_config,
		},
	},
#endif
#endif
};
#endif

static struct msm_gpiomux_config msm_taiko_config[] __initdata = {
	{
		.gpio	= 101,	/* CODEC_RESET_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &taiko_reset,
		},
	},
};

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct gpiomux_setting gpio_eth_config = {
	.pull = GPIOMUX_PULL_UP,
	.drv = GPIOMUX_DRV_2MA,
	.func = GPIOMUX_FUNC_GPIO,
};

static struct msm_gpiomux_config msm_eth_configs[] __initdata = {
	{
		.gpio	  = 60,			/* SPI IRQ */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
		}
	},
	{
		.gpio	  = 117,		/* CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
		}
	},
};
#endif

#if defined(CONFIG_INPUT_WACOM)
static struct gpiomux_setting wacom_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting wacom_gpio_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir  = GPIOMUX_IN,
};
static struct gpiomux_setting wacom_outlow_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config wacom_ic_configs[] __initdata = {
	{
		.gpio      = 49,		/* BLSP2 QUP5 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_i2c_config,
			[GPIOMUX_SUSPENDED] = &wacom_gpio_config,
		},
	},

	{
		.gpio      = 50,		/* BLSP2 QUP5 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_i2c_config,
			[GPIOMUX_SUSPENDED] = &wacom_gpio_config,
		},
	},
	{
		.gpio = 16,				/* FWE1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_outlow_config,
			[GPIOMUX_SUSPENDED] = &wacom_outlow_config,
		},
	},
	{
		.gpio	  = 24,			/* WACOM INT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_gpio_config,
			[GPIOMUX_SUSPENDED] = &wacom_gpio_config,
		}
	},
	{
		.gpio	  = 81,			/* WACOM PDCT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_gpio_config,
			[GPIOMUX_SUSPENDED] = &wacom_gpio_config,
		}
	},
};

static struct msm_gpiomux_config wacom_ic_configs_rev12[] __initdata = {
	{
		.gpio      = 49,		/* BLSP2 QUP5 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_i2c_config,
			[GPIOMUX_SUSPENDED] = &wacom_gpio_config,
		},
	},

	{
		.gpio      = 50,		/* BLSP2 QUP5 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_i2c_config,
			[GPIOMUX_SUSPENDED] = &wacom_gpio_config,
		},
	},
	{
		.gpio = 16,				/* FWE1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_outlow_config,
			[GPIOMUX_SUSPENDED] = &wacom_outlow_config,
		},
	},
	{
		.gpio	  = 9,			/* WACOM INT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_gpio_config,
			[GPIOMUX_SUSPENDED] = &wacom_gpio_config,
		}
	},
	{
		.gpio	  = 143,			/* WACOM PDCT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wacom_gpio_config,
			[GPIOMUX_SUSPENDED] = &wacom_gpio_config,
		}
	},
};
#endif


static struct msm_gpiomux_config msm_blsp1_uart6_configs[] __initdata = {
	{
		.gpio      = 43,		/* BLSP1 UART6 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 44,		/* BLSP1 UART6 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 45,		/* BLSP1 UART6 CTS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 46,		/* BLSP1 UART6 RFR */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
};

/* QCA1530 uses differnt GPIOs based on platform, hence these settings */
static struct msm_gpiomux_config msm_blsp2_uart5_configs[] __initdata = {
	{
		.gpio      = 112,		/* BLSP2 UART5 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_gps_liquid_config,
		},
	},
	{
		.gpio      = 113,		/* BLSP2 UART5 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_gps_liquid_config,
		},
	},
};
static struct gpiomux_setting general_led_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting general_led_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

/*General led configuration for SBC 8084 platform*/
static struct msm_gpiomux_config apq8084_general_led_configs[] = {
	{
		.gpio = 83,
		.settings = {
			[GPIOMUX_ACTIVE]    = &general_led_cfg,
			[GPIOMUX_SUSPENDED] = &general_led_sus_cfg,
		},
	},
};
#ifdef CONFIG_SEC_MHL_SII8620
static struct gpiomux_setting mhl_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif

static struct gpiomux_setting hdmi_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hdmi_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_active_2_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

#if !defined(CONFIG_SEC_MHL_SUPPORT)
static struct gpiomux_setting hdmi_active_mux_lpm_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};
#endif
/*
static struct gpiomux_setting hdmi_active_mux_en_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};
*/
static struct gpiomux_setting hdmi_active_mux_sel_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config msm_hdmi_nomux_configs[] __initdata = {
#if !defined(CONFIG_SEC_MHL_SUPPORT)
	{
		.gpio = 31,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
#endif
	{
		.gpio = 32,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 34,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_hdmi_configs[] __initdata = {
#if !defined(CONFIG_SEC_MHL_SUPPORT)
	{
		.gpio = 31,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
#endif
	{
		.gpio = 32,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 34,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	/*
	{
		.gpio = 83,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_mux_en_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	*/
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_mux_sel_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
};

#ifdef CONFIG_SEC_MHL_SII8620
static struct msm_gpiomux_config msm_hdmi_SII8620_configs[] __initdata = {
	{
		.gpio = 49,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mhl_active_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_mux_sel_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
};
#endif
#ifdef CONFIG_SEC_MHL_SII8240
static struct msm_gpiomux_config mhl_configs[] __initdata = {
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mhl_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &mhl_suspend_cfg,
		},
	},
#if !defined(CONFIG_SEC_TBLTE_JPN)	
	{
		.gpio = 116,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mhl_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &mhl_suspend_cfg,
		},
	},
#endif	
	{
		.gpio      = 6, /* BLSP3 QUP I2C_DAT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &mhl_active_cfg,
			[GPIOMUX_SUSPENDED] = &mhl_suspend_cfg,

		},
	},
	{
		.gpio      = 7, /* BLSP3 QUP I2C_CLK */
		.settings = {
			[GPIOMUX_ACTIVE]    = &mhl_active_cfg,
			[GPIOMUX_SUSPENDED] = &mhl_suspend_cfg,
		},
	},

};
#endif

static struct gpiomux_setting hsic_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting hsic_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting hsic_wakeup_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hsic_wakeup_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hsic_ap2mdm_chlrdy_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting hsic_ap2mdm_chlrdy_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config apq8084_hsic_configs[] = {
	{
		.gpio = 134,               /*HSIC_STROBE */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 135,               /* HSIC_DATA */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 107,              /* wake up */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_wakeup_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_wakeup_sus_cfg,
		},
	},
	{
		.gpio = 106,
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_ap2mdm_chlrdy_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_ap2mdm_chlrdy_sus_cfg,
		}
	},
	{
		.gpio = 108,     /* ap2mdm_wakeup is used as resume gpio */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_wakeup,
		}
	},
};
#if 0
static struct gpiomux_setting lcd_en_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting lcd_en_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif
static struct gpiomux_setting lcd_te_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting lcd_te_sus_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};
#if 0
static struct msm_gpiomux_config msm_lcd_configs[] __initdata = {
	{
		.gpio = 96,			/* LCD RESET */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
#if !defined(CONFIG_SENSORS_SSP)
	{
		.gpio = 86,			/* BKLT ENABLE */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
#endif
#if 0
	{
		.gpio = 137,			/* DISPLAY ENABLE */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
#endif
};
#endif
static struct msm_gpiomux_config msm_lcd_te_configs[] __initdata = {
	{
		.gpio = 12,			/* TEAR ENABLE */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_te_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_te_sus_cfg,
		},
	},
};


static struct gpiomux_setting auxpcm_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting auxpcm_sus_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

/* Primary AUXPCM port sharing GPIO lines with Tertiary MI2S */
static struct msm_gpiomux_config apq8084_pri_ter_auxpcm_configs[] __initdata = {
	{
		.gpio = 87,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 88,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 89,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 90,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
};

#if !defined(CONFIG_SEC_TRLTE_PROJECT) && !defined(CONFIG_SEC_TBLTE_PROJECT)
static struct gpiomux_setting wlan_en_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config msm_wlan_configs[] __initdata = {
	{
		.gpio = 82,			/* WLAN ENABLE */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wlan_en_cfg,
			[GPIOMUX_SUSPENDED] = &wlan_en_cfg,
		},
	},
};
#endif

#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
static struct gpiomux_setting wlan_en_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config msm_wlan_configs[] __initdata = {
	{
		.gpio = 117,			/* WLAN ENABLE */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wlan_en_cfg,
			[GPIOMUX_SUSPENDED] = &wlan_en_cfg,
		},
	},
};
#endif

static struct gpiomux_setting sd_card_det_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting sd_card_det_sleep_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config sd_card_det[] __initdata = {
	{
		.gpio = 122,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sd_card_det_active_config,
			[GPIOMUX_SUSPENDED] = &sd_card_det_sleep_config,
		},
	},
};

static struct gpiomux_setting eth_pwr_sleep_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config eth_pwr[] = {
	{
		.gpio = 39,
		.settings = {
			[GPIOMUX_SUSPENDED] = &eth_pwr_sleep_config,
		},
	},
};

static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_1, /*active 1*/ /* 0 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*suspend*/ /* 1 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*i2c suspend*/ /* 2 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 0*/ /* 3 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend 0*/ /* 4 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 2*/ /* 5 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 2*/ /* 6 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
};

#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
static struct msm_gpiomux_config msm_fpga_configs[] __initdata = {
	{
		.gpio = 13, /*FPGA_I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 14, /* FPGA_I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
};
#endif

static struct msm_gpiomux_config msm_sensor_configs[] __initdata = {
#if defined(CONFIG_SEC_KCCAT6_PROJECT) || defined(CONFIG_SEC_LENTIS_PROJECT)
	{
		.gpio = 13, /* VT_CAM_IO_EN :CAM_VT_VIO */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 14, /* VT_CAM_A_EN : CAM_VT_VANA */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
#endif
	{
		.gpio = 15, /* CAM_MCLK0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[5],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],
		},
	},
	{
		.gpio = 17, /* CAM_MCLK2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[5],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],
		},
	},
	{
		.gpio = 19, /* CCI_I2C_SDA0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 20, /* CCI_I2C_SCL0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 21, /* CCI_I2C_SDA1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 22, /* CCI_I2C_SCL1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 25, /* COMPANION_RSTN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 35, /* CAM1_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 36, /* CAM1_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 37, /* CAM2_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 38, /* CAM2_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 120, /* CAM_SENSOR_DET */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio = 144, /* COMP_SPI_INT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
};

#if defined(CONFIG_MSMB_CAMERA)
#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
static struct gpiomux_setting cam_gpio_i2c7_config = {
#if defined(CONFIG_SEC_TRLTE_CHNDUOS)
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
#else
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
#endif
};
#endif
static struct msm_gpiomux_config msm_actuator_configs[] __initdata = {
#if defined(CONFIG_SEC_LENTIS_PROJECT) || defined(CONFIG_SEC_KCCAT6_PROJECT)
{
               .gpio      = 49,                /* BLSP2 QUP5 I2C_SDA */
               .settings = {
                       [GPIOMUX_ACTIVE] = &gpio_i2c_config,
                       [GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
               },
       },
       {
               .gpio      = 50,                /* BLSP2 QUP5 I2C_SCL */
               .settings = {
                       [GPIOMUX_ACTIVE] = &gpio_i2c_config,
                       [GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
               },
       },
#endif
#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
#if defined(CONFIG_SEC_TRLTE_CHNDUOS)
       {
               .gpio	  = 10,				/* BLSP2 QUP5 I2C_SDA */
               .settings = {
                       [GPIOMUX_ACTIVE] = &cam_gpio_i2c7_config  ,
                       [GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
               },
       },
       {
               .gpio	  = 11,				/* BLSP2 QUP5 I2C_SCL */
               .settings = {
                       [GPIOMUX_ACTIVE] = &cam_gpio_i2c7_config  ,
                       [GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
               },
       },
#else
       {
               .gpio      = 132,                /* BLSP2 QUP5 I2C_SDA */
               .settings = {
                       [GPIOMUX_ACTIVE] = &cam_gpio_i2c7_config  ,
                       [GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
               },
       },
       {
               .gpio      = 133,                /* BLSP2 QUP5 I2C_SCL */
               .settings = {
                       [GPIOMUX_ACTIVE] = &cam_gpio_i2c7_config  ,
                       [GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
               },
       },
#endif
#endif
};
#endif

static struct msm_gpiomux_config msm_sbc_sensor_configs[] __initdata = {
	{
		.gpio = 15, /* CSI0 (CAM1) CAM_MCLK0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
#if !defined(CONFIG_NFC_PN547_8084_USE_BBCLK2)
	{
		.gpio = 16, /* CSI1 (CAM 3D L) CAM_MCLK1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
#endif
	{
		.gpio = 17, /* CSI2 (CAM2) CAM_MCLK2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 18, /* CSI1 (CAM 3D R) CAM_MCLK3 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 19, /* (CAM1, CAM2 and CAM 3D L) CCI_I2C_SDA0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 20, /* (CAM1, CAM2 and CAM 3D L) CCI_I2C_SCL0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 21, /* (CAM 3D R) CCI_I2C_SDA1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 22, /* (CAM 3D L) CCI_I2C_SCL1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 23, /* (CAM1 CCI_TIMER0) FLASH_LED_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 24, /* (CAM1 CCI_TIMER1) FLASH_LED_NOW */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 25, /* CAM2_RESET_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 35, /* CAM1_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 36, /* CAM1_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 78, /* CAM2_STANDBY  */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 118, /* (CAM 3D L&R) CAM_3D_STANDBY  */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},

	{
		.gpio = 120, /* (CAM 3D L&R) CAM_3D_RESET */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 144, /* COMP_SPI_INT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
};

#if defined(CONFIG_NFC_PN547)
static struct gpiomux_setting nfc_irq_cfg = {
        .func = GPIOMUX_FUNC_GPIO,
        .drv = GPIOMUX_DRV_2MA,
        .pull = GPIOMUX_PULL_DOWN,
        .dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config msm_nfc_configs[] __initdata = {
        {
                .gpio      = 75,	/* NFC IRQ */
                .settings = {
                        [GPIOMUX_ACTIVE] = &nfc_irq_cfg,
                        [GPIOMUX_SUSPENDED] = &nfc_irq_cfg,
                },
        },
};

#if defined(CONFIG_SEC_LENTIS_PROJECT)
static struct msm_gpiomux_config msm_nfc_configs_rev08[] __initdata = {
	{
			.gpio	   = 28,	/* NFC IRQ */
			.settings = {
					[GPIOMUX_ACTIVE] = &nfc_irq_cfg,
					[GPIOMUX_SUSPENDED] = &nfc_irq_cfg,
			},
	},
};
#endif
#endif

static struct gpiomux_setting gpio_qca1530_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

#if !defined(CONFIG_SEC_TBLTE_JPN)
static struct gpiomux_setting gpio_qca1530_config_mpp7 = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};
#endif

static struct gpiomux_setting gpio_pcie_clkreq_config[] = {
	{
		.func = GPIOMUX_FUNC_2,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},
	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},
};

static struct msm_gpiomux_config msm_pcie_configs[] __initdata = {
	{
		.gpio = 68,    /* PCIE0_CLKREQ_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_pcie_clkreq_config[0],
		},
	},
	{
		.gpio = 69,    /* PCIE0_WAKE_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
#if !defined(CONFIG_SEC_TRLTE_JPN) && !defined(CONFIG_SEC_TBLTE_JPN)	
	{
		.gpio = 139,	/* PCIE1_WAKE_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 140,	/* PCIE1_RST_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 141,    /* PCIE1_CLKREQ_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_pcie_clkreq_config[1],
		},
	},
#endif	
};
#if !defined(CONFIG_TDMB) && !defined(CONFIG_ISDBT_FC8300)
static struct msm_gpiomux_config msm_qca1530_cdp_configs[] __initdata = {
	{
		.gpio = 133,    /* qca1530 reset */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_qca1530_config,
		},
	},
};
#endif
static struct msm_gpiomux_config msm_qca1530_liquid_configs[] __initdata = {
	{
		.gpio = 128,    /* qca1530 reset */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_qca1530_config,
		},
	},
#if !defined(CONFIG_SEC_TBLTE_JPN)	
	{
		.gpio = 66,     /* qca1530 power extra */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_qca1530_config_mpp7,
		},
	},
#endif 	
};

#if !defined(CONFIG_SND_SOC_MAX98504) && !defined(CONFIG_SND_SOC_MAX98504A)
static struct msm_gpiomux_config msm_epm_configs[] __initdata = {
	{
		.gpio      = 92,		/* EPM enable */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_epm_config,
		},
	},
};
#endif

#if defined(CONFIG_SENSORS_VFS61XX) || defined(CONFIG_SENSORS_VFS7XXX)
static struct gpiomux_setting gpio_spi_ocp_flag_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting gpio_spi_btp_irq_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
#if defined(CONFIG_SENSORS_VFS7XXX)
	.pull = GPIOMUX_PULL_DOWN,
#else
#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
	.pull = GPIOMUX_PULL_DOWN,
#else
	.pull = GPIOMUX_PULL_NONE,
#endif
#endif
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting gpio_spi_btp_rst_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};
static struct gpiomux_setting gpio_blsp_spi1_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
#if defined(CONFIG_SEC_LENTIS_PROJECT)
static struct msm_gpiomux_config apq8084_fingerprint_configs_rev07[] __initdata = {
	{
		/* BTP_OCP_EN */
		.gpio = 136,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
};

static struct msm_gpiomux_config apq8084_fingerprint_configs[] __initdata = {
	{
		/* MOSI */
		.gpio = 55,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* MISO */
		.gpio = 56,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* CS */
		.gpio = 57,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* CLK  */
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* BTP_RST_N */
		.gpio = 31,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
	{
		/* BTP_LDO2 */
		.gpio = 72,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
	{
		/* BTP_INT */
		.gpio = 79,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_irq_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_irq_config,
		},
	},
	{
		/* BTP_LDO */
		.gpio = 137,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
	{
		/* BTP_OCP_FLAG */
		.gpio = 100,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_ocp_flag_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_ocp_flag_config,
		},
	},
};
#elif defined(CONFIG_SEC_KCCAT6_PROJECT)
static struct msm_gpiomux_config apq8084_fingerprint_configs[] __initdata = {
	{
		/* MOSI */
		.gpio = 55,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* MISO */
		.gpio = 56,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* CS */
		.gpio = 57,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* CLK  */
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* BTP_RST_N */
		.gpio = 31,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
	{
		/* BTP_LDO2 */
		.gpio = 72,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
	{
		/* BTP_INT */
		.gpio = 79,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_irq_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_irq_config,
		},
	},
	{
		/* BTP_LDO */
		.gpio = 137,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
	{
		/* BTP_OCP_FLAG */
		.gpio = 100,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_ocp_flag_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_ocp_flag_config,
		},
	},
	{
		/* BTP_OCP_EN */
		.gpio = 136,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
};
#else
static struct gpiomux_setting gpio_spi_btp_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct msm_gpiomux_config apq8084_fingerprint_configs_rev10[] __initdata = {
	{
		/* BTP_LDO */
		.gpio = 308,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
};

static struct msm_gpiomux_config apq8084_fingerprint_configs_rev03[] __initdata = {
	{
		/* BTP_OCP_EN */
		.gpio = 136,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
	{
		/* BTP_LDO2 */
		.gpio = 304,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
};

static struct msm_gpiomux_config apq8084_fingerprint_configs_ldo1[] __initdata = {
	{
		/* BTP_LDO */
		.gpio = 137,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_rst_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
};

static struct msm_gpiomux_config apq8084_fingerprint_configs[] __initdata = {
	{
		/* MOSI */
		.gpio = 55,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_config,
		},
	},
	{
		/* MISO */
		.gpio = 56,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_config,
		},
	},
	{
		/* CS */
		.gpio = 57,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_config,
		},
	},
	{
		/* CLK  */
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_blsp_spi1_config,
		},
	},
	{
		/* BTP_RST_N */
		.gpio = 35,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_rst_config,
		},
	},
	{
		/* BTP_INT */
		.gpio = 671,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_btp_irq_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_btp_irq_config,
		},
	},
	{
		/* BTP_OCP_FLAG */
		.gpio = 100,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_ocp_flag_config,
			[GPIOMUX_ACTIVE] = &gpio_spi_ocp_flag_config,
		},
	},
};
#endif
#endif /* #endif defined(CONFIG_SENSORS_VFS61XX) */

#if defined(CONFIG_SENSORS_SSP)
static struct gpiomux_setting ssp_setting[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_HIGH,
	},
};

#if defined(CONFIG_SEC_LENTIS_PROJECT)
static struct msm_gpiomux_config ssp_configs_rev07[] __initdata = {
	{
		.gpio = 77,	/* MCU_nRST_1.8V*/
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[2],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
};

static struct msm_gpiomux_config ssp_configs_lentis_rev08[] __initdata = {
	{
		.gpio = 67,	/* MCU_AP_INT_1_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[1],
			[GPIOMUX_SUSPENDED] = &ssp_setting[1],
		},
	},
	{
		.gpio = 9,	/* MCU_AP_INT_2_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[1],
			[GPIOMUX_SUSPENDED] = &ssp_setting[1],
		},
	},
	{
		.gpio = 143,	/* AP_MCU_INT_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[1],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
};
#endif

#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
static struct msm_gpiomux_config ssp_configs_rev12[] __initdata = {
#if defined(CONFIG_SEC_TRLTE_CHNDUOS)
	{
		.gpio = 72, /* MCU_nRST_1.8V*/
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[2],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
#else
	{
		.gpio = 82, /* MCU_nRST_1.8V*/
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[2],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
#endif
	{
		.gpio = 103,	/* MCU_AP_INT_1_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[0],
			[GPIOMUX_SUSPENDED] = &ssp_setting[0],
		},
	},
	{
		.gpio = 24,	/* MCU_AP_INT_2_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[1],
			[GPIOMUX_SUSPENDED] = &ssp_setting[1],
		},
	},
	{
		.gpio = 81,	/* AP_MCU_INT_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[1],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
};
#endif

static struct msm_gpiomux_config ssp_configs[] __initdata = {
#if defined(CONFIG_SEC_KCCAT6_PROJECT)
	{
		.gpio = 77,	/* MCU_nRST_1.8V*/
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[2],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
	{
		.gpio = 67,	/* MCU_AP_INT_1_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[1],
			[GPIOMUX_SUSPENDED] = &ssp_setting[1],
		},
	},
#else
#if defined(CONFIG_SEC_TRLTE_CHNDUOS)
	{
		.gpio = 72, /* MCU_nRST_1.8V*/
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[2],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
#else
	{
		.gpio = 82,	/* MCU_nRST_1.8V*/
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[2],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
#endif
	{
		.gpio = 103,	/* MCU_AP_INT_1_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[0],
			[GPIOMUX_SUSPENDED] = &ssp_setting[0],
		},
	},
#endif
	{
		.gpio = 9,	/* MCU_AP_INT_2_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[1],
			[GPIOMUX_SUSPENDED] = &ssp_setting[1],
		},
	},
	{
		.gpio = 143,	/* AP_MCU_INT_1.8V */
		.settings = {
			[GPIOMUX_ACTIVE] = &ssp_setting[1],
			[GPIOMUX_SUSPENDED] = &ssp_setting[2],
		},
	},
};
#endif

#if defined(CONFIG_SENSORS_MAX86900)
static struct gpiomux_setting gpio_hrm_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_hrm_hrm_en_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config apq8084_hrm_configs[] __initdata = {
	{
		.gpio = 78,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
			[GPIOMUX_ACTIVE] = &gpio_hrm_int_config,
		},
	},
	{
		.gpio = 96,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
			[GPIOMUX_ACTIVE] = &gpio_hrm_hrm_en_config,
		},
	},
};
#endif

#if defined(CONFIG_SENSORS_MAX86902)
static struct gpiomux_setting gpio_hrm_int_config = {
    .func = GPIOMUX_FUNC_GPIO,
    .drv = GPIOMUX_DRV_2MA,
    .pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config apq8084_hrm_configs[] __initdata = {
	{
		.gpio = 78,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
			[GPIOMUX_ACTIVE] = &gpio_hrm_int_config,
		},
	},
};

static struct msm_gpiomux_config apq8084_hrm_configs_rev10[] __initdata = {
	{
		.gpio = 137,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
			[GPIOMUX_ACTIVE] = &gpio_hrm_int_config,
		},
	},
};
#endif

#if defined(CONFIG_SENSORS_SX9306)
static struct gpiomux_setting grip_i2c_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting grip_int_cfg = {
    .func = GPIOMUX_FUNC_GPIO,
    .drv = GPIOMUX_DRV_2MA,
    .pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm_grip_configs[] __initdata = {
	{
		.gpio      = 10,
		.settings = {
			[GPIOMUX_ACTIVE] = &grip_i2c_cfg,
			[GPIOMUX_SUSPENDED] = &grip_i2c_cfg,
		},
	},
	{
		.gpio      = 11,
		.settings = {
			[GPIOMUX_ACTIVE] = &grip_i2c_cfg,
			[GPIOMUX_SUSPENDED] = &grip_i2c_cfg,
		},
	},
	{
		.gpio	   = 8,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_suspend_config[1],
			[GPIOMUX_SUSPENDED] = &grip_int_cfg,
		},
	},
};
#endif

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE) || defined(CONFIG_LINK_DEVICE_SPI)
#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_LINK_DEVICE_SPI)
static struct gpiomux_setting gpio_spi7_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
#elif defined(CONFIG_TDMB_I2C)
static struct gpiomux_setting gpio_i2c7_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif
static struct msm_gpiomux_config apq8084_tdmb_configs[] __initdata = {
#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_LINK_DEVICE_SPI)
	{
		.gpio	   = 130,	/* BLSP2 QUP1 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi7_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 131,	/* BLSP2 QUP1 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi7_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 132,	/* BLSP2 QUP1 SPI_CS0_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi7_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 133,	/* BLSP2 QUP1 SPI_CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi7_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
#elif defined(CONFIG_TDMB_I2C)
	{
		.gpio	   = 132,	/* BLSP2 QUP1 I2C_SDA*/
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c7_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 133,	/* BLSP2 QUP1 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c7_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
#endif
};
#endif

#if defined(CONFIG_ISDBT_FC8300)
static struct gpiomux_setting gpio_i2c7_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct msm_gpiomux_config apq8084_isdbt_configs[] __initdata = {	
	{
		.gpio	   = 132,	/* I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c7_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 133,	/* I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c7_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
};
static struct gpiomux_setting gpio_i2c27_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct msm_gpiomux_config apq8084_isdbt_configs_rev04[] __initdata = {	
	{
		.gpio	   = 75,	/* I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c27_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 77,	/* I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c27_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
};
static struct gpiomux_setting gpio_isdbt_rev07_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct msm_gpiomux_config apq8084_isdbt_configs_rev07[] __initdata = {	
	{
		.gpio	   = 10,	/* I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_isdbt_rev07_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio	   = 11,	/* I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_isdbt_rev07_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
};
#elif defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
static struct gpiomux_setting gpio_wcinokb_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
static struct msm_gpiomux_config gpio_wcinokb_configs[] __initdata = {
	{
		.gpio	   = 77,	/* I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_wcinokb_config,
			[GPIOMUX_SUSPENDED] = &gpio_wcinokb_config,
		},
	},
};
#endif

#if defined(CONFIG_SEC_TRLTE_PROJECT)
static struct msm_gpiomux_config apq8084_cover_i2c_configs[] __initdata = {	
	{
		.gpio	   = 27,	/* COVER_I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_suspend_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio	   = 28,	/* COVER_I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_suspend_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
};
static struct msm_gpiomux_config apq8084_cover_uart_configs[] __initdata = {
	{
		.gpio	   = 27,	/* COVER_UART_TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio	   = 28,	/* COVER_UART_RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
};
#endif

#if defined(CONFIG_SND_SOC_MAX98504) || defined(CONFIG_SND_SOC_MAX98504A)
static struct gpiomux_setting max98504_i2c_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting max98504_i2s_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

#if defined(CONFIG_SEC_TRLTE_CHNDUOS)
static struct msm_gpiomux_config apq8084_audio_configs_rev06[] __initdata = {
	{
		.gpio	   = 139,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 140,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 82,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 83,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 84,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 85,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],
		},
	},
	{
		.gpio	   = 92,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 93,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 94,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 95,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
};
#endif
static struct msm_gpiomux_config apq8084_audio_configs[] __initdata = {
#if defined(CONFIG_LINK_DEVICE_SPI)
	{
		.gpio	   = 82,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 83,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 84,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 85,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 116,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 117,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
#else
	{
		.gpio	   = 130,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio	   = 131,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
#endif
	{
		.gpio	   = 92,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 93,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 94,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
	{
		.gpio	   = 95,
		.settings = {
			[GPIOMUX_ACTIVE] = &max98504_i2s_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],
		},
	},
};
#endif
#if defined(CONFIG_SAMSUNG_JACK)
static struct gpiomux_setting ear_det_gpio_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO, 
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting ear_det_gpio_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO, 
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};
static struct msm_gpiomux_config apq8084_ear_configs[] __initdata = {
	{
#ifdef CONFIG_SEC_KCCAT6_PROJECT
		.gpio = 95,	/* EAR DET */
#else
		.gpio = 121,	/* EAR DET */
#endif
		.settings = {
			[GPIOMUX_ACTIVE] = &ear_det_gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &ear_det_gpio_suspend_cfg,
		},
	},
};
#endif
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_G)
static struct gpiomux_setting gpio_tsp_sleep_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting gpio_tsp_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config apq8084_tsp_configs[] __initdata = {
	{
		.gpio = 83,	/* TSP_EN */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_tsp_active_config,
			[GPIOMUX_SUSPENDED] = &gpio_tsp_sleep_config,
		},
	},

};
#endif

#if !defined(CONFIG_SEC_TBLTE_JPN)
#if defined(CONFIG_KEYBOARD_CYPRESS_TOUCHKEY) || defined(CONFIG_KEYBOARD_CYPRESS_TOUCHKEY_T)
static struct gpiomux_setting gpio_tkey_sleep_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting gpio_tkey_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config apq8084_tkey_configs[] __initdata = {
	{
		.gpio = 65,	/* TKEY_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_tkey_active_config,
			[GPIOMUX_SUSPENDED] = &gpio_tkey_sleep_config,
		},
	},
	{
		.gpio = 66,	/* TKEY_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_tkey_active_config,
			[GPIOMUX_SUSPENDED] = &gpio_tkey_sleep_config,
		},
	},
};
#endif
#endif

#if defined(CONFIG_SEC_TBLTE_JPN)
static struct gpiomux_setting gpio_hrm_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm_hrm_max86900_config_configs[] __initdata = {
	{
		.gpio      = 65,		/* BLSP11 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_hrm_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_hrm_i2c_config,
		},
	},
	{
		.gpio      = 66,		/* BLSP11 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_hrm_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_hrm_i2c_config,
		},
	},
};
#endif

/* CONFIG_SEC_NFC_SENN3AB */
#if defined(CONFIG_SEC_NFC_SENN3AB)

static struct gpiomux_setting felica_i2c_setting = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting felica_uart_tx_setting = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting felica_uart_rx_setting = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting felica_uart_cts_setting = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting felica_uart_rfr_setting = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ven_setting = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting pvdd_setting = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config apq8084_senn3ab_configs[] __initdata = {
	{
		.gpio = GPIO_FELICA_PVDD,
		.settings = {
			[GPIOMUX_ACTIVE]    = &pvdd_setting,
			[GPIOMUX_SUSPENDED] = &pvdd_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_VEN,/*VEN*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &ven_setting,
			[GPIOMUX_SUSPENDED] = &ven_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_I2C_SDA,/*I2C_SDA*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_i2c_setting,
			[GPIOMUX_SUSPENDED] = &felica_i2c_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_I2C_SCL,/*I2C_SCL*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_i2c_setting,
			[GPIOMUX_SUSPENDED] = &felica_i2c_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_UART_TX,/*4-pin UART_TX*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_uart_tx_setting,
			[GPIOMUX_SUSPENDED] = &felica_uart_tx_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_UART_RX,/*4-pin UART_RX*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_uart_rx_setting,
			[GPIOMUX_SUSPENDED] = &felica_uart_rx_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_UART_CTS,/*4-pin UART_CTS*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_uart_cts_setting,
			[GPIOMUX_SUSPENDED] = &felica_uart_cts_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_UART_RFR,/*4-pin UART_RFR*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_uart_rfr_setting,
			[GPIOMUX_SUSPENDED] = &felica_uart_rfr_setting,
		},
	},
};

static struct msm_gpiomux_config apq8084_senn3ab_configs_rev07[] __initdata = {
	{
		.gpio = GPIO_FELICA_PVDD,
		.settings = {
			[GPIOMUX_ACTIVE]    = &pvdd_setting,
			[GPIOMUX_SUSPENDED] = &pvdd_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_VEN_REV07,/*VEN*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &ven_setting,
			[GPIOMUX_SUSPENDED] = &ven_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_UART_TX,/*4-pin UART_TX*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_uart_tx_setting,
			[GPIOMUX_SUSPENDED] = &felica_uart_tx_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_UART_RX,/*4-pin UART_RX*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_uart_rx_setting,
			[GPIOMUX_SUSPENDED] = &felica_uart_rx_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_UART_CTS,/*4-pin UART_CTS*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_uart_cts_setting,
			[GPIOMUX_SUSPENDED] = &felica_uart_cts_setting,
		},
	},
	{
		.gpio = GPIO_FELICA_UART_RFR,/*4-pin UART_RFR*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &felica_uart_rfr_setting,
			[GPIOMUX_SUSPENDED] = &felica_uart_rfr_setting,
		},
	},
};
#endif /* CONFIG_SEC_NFC_SENN3AB */

static struct msm_gpiomux_config cover_id_config[] __initdata = {
	{
		.gpio = 129,	/* COVER_ID */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
};

#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
static struct gpiomux_setting wlan_wake_msm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config wlan_wake_msm_configs[] __initdata = {
	{
		.gpio = 78,     /* interrupt pin for WLAN_HOST_WAKE */
		.settings = {
			[GPIOMUX_ACTIVE]    = &wlan_wake_msm_cfg,
			[GPIOMUX_SUSPENDED] = &wlan_wake_msm_cfg,
		},
	},
};
#endif /* CONFIG_SEC_TRLTE_PROJECT || CONFIG_SEC_TBLTE_PROJECT */

#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)

static struct msm_gpiomux_config stm_tsp_config[] __initdata = {
	{
		.gpio = 79,	/* TSP_IRQ */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_suspend_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
};


static struct msm_gpiomux_config sub_pmic_config[] __initdata = {
	{
		.gpio = 120,	/* sub pmic irq */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_suspend_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
};

/* set IN-PD */
#define GPIOMUX_SET_NC(n) \
	{ \
		.gpio	   = n, \
		.settings = { \
			[GPIOMUX_ACTIVE] = &gpio_suspend_config[2], \
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2], \
		}, \
	}
static struct msm_gpiomux_config gpio_nc_configs[] __initdata = {
	/* EUR, USA(NORTH AMERICA) */
#if defined(CONFIG_MACH_TRLTE_EUR) || \
	defined(CONFIG_MACH_TRLTE_ATT) || defined(CONFIG_MACH_TRLTE_SPR) || \
	defined(CONFIG_MACH_TRLTE_TMO) || defined(CONFIG_MACH_TRLTE_VZW) || \
	defined(CONFIG_MACH_TRLTE_USC) || defined(CONFIG_MACH_TRLTE_CAN)

	/* TDMB(not equal to !TDMB because of CHINA) */
	GPIOMUX_SET_NC(83),	/* CHINA: CP2_PCM_SYNC */
	GPIOMUX_SET_NC(84),	/* CHINA: CP2_PCM_IN */
	GPIOMUX_SET_NC(85),	/* CHINA: CP2_PCM_OUT */
	GPIOMUX_SET_NC(86),	/* CHINA: CP2_ACTIVE */

	/* NC except CHINA */
	GPIOMUX_SET_NC(8),	/* CHINA: AP_CP_MRDY */
	GPIOMUX_SET_NC(116),	/* CHINA: AF_OIS_TDMB_SDA */
	GPIOMUX_SET_NC(146),	/* CHINA: AP_CP_SUBSRDY */
#endif

	/* non PCIe */
#if defined(CONFIG_MACH_TRLTE_ATT) || defined(CONFIG_MACH_TRLTE_TMO) || \
	defined(CONFIG_MACH_TRLTE_VZW) || defined(CONFIG_MACH_TRLTE_USC) || \
	defined(CONFIG_MACH_TRLTE_CAN) || defined(CONFIG_MACH_TBLTE_VZW) || \
	defined(CONFIG_MACH_TBLTE_ATT) || defined(CONFIG_MACH_TBLTE_TMO) || \
	defined(CONFIG_MACH_TBLTE_CHN) || defined(CONFIG_MACH_TBLTE_USC) || \
	defined(CONFIG_MACH_TRLTE_SPR)

	/* CTI_PAIR2 */
	GPIOMUX_SET_NC(123),
	GPIOMUX_SET_NC(124),

	/* PCIe */
	GPIOMUX_SET_NC(139),
	GPIOMUX_SET_NC(140),
	GPIOMUX_SET_NC(141),
#endif

#if defined(CONFIG_SEC_TBLTE_JPN)
	GPIOMUX_SET_NC(8),	
	GPIOMUX_SET_NC(72),
	GPIOMUX_SET_NC(74),
	GPIOMUX_SET_NC(116),
	GPIOMUX_SET_NC(124),
	GPIOMUX_SET_NC(139),
	GPIOMUX_SET_NC(140),
	GPIOMUX_SET_NC(141),
	GPIOMUX_SET_NC(146),
#elif defined(CONFIG_SEC_TRLTE_JPN)
	GPIOMUX_SET_NC(8),	
	GPIOMUX_SET_NC(74),
	GPIOMUX_SET_NC(124),
	GPIOMUX_SET_NC(139),
	GPIOMUX_SET_NC(140),
	GPIOMUX_SET_NC(141),
	GPIOMUX_SET_NC(146),
#endif 
};
static struct msm_gpiomux_config gpio_nc_configs_r06[] __initdata = {
	GPIOMUX_SET_NC(75),	/* SUB_PMIC_SDA_1.8V */
	GPIOMUX_SET_NC(145),	/* SUB_PMIC_SCL_1.8V */
#if defined(CONFIG_MACH_TRLTE_ATT)
	GPIOMUX_SET_NC(72),
#endif
};
#if !defined(CONFIG_ISDBT_FC8300)
#if !defined(CONFIG_SEC_TRLTE_CHNDUOS) && !defined(CONFIG_SENSORS_SX9306)
static struct msm_gpiomux_config gpio_nc_configs_r07[] __initdata = {
	GPIOMUX_SET_NC(10),	/* SUB_PMIC_SDA_1.8V */
	GPIOMUX_SET_NC(11),	/* SUB_PMIC_SCL_1.8V */
};
#endif
#endif

/* set IN-NP */
#define GPIOMUX_SET_I2C(n) \
	{ \
		.gpio	   = n, \
		.settings = { \
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0], \
		}, \
	}
static struct msm_gpiomux_config gpio_i2c_configs_r06[] __initdata = {
	GPIOMUX_SET_I2C(10),	/* SUB_PMIC_SDA_1.8V */
	GPIOMUX_SET_I2C(11),	/* SUB_PMIC_SCL_1.8V */
};
static struct msm_gpiomux_config gpio_i2c_configs_r07[] __initdata = {
	GPIOMUX_SET_I2C(75),	/* SUB_PMIC_SDA_1.8V */
	GPIOMUX_SET_I2C(145),	/* SUB_PMIC_SCL_1.8V */
};
#endif	/* CONFIG_SEC_TRLTE_PROJECT, CONFIG_SEC_TBLTE_PROJECT */

void __init apq8084_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	if (of_board_is_sbc()) {
		msm_gpiomux_install(msm_sbc_blsp_configs,
				ARRAY_SIZE(msm_sbc_blsp_configs));
	} else {
		msm_gpiomux_install(msm_blsp_configs,
				ARRAY_SIZE(msm_blsp_configs));
	}
	msm_gpiomux_install(msm_blsp1_uart6_configs,
					ARRAY_SIZE(msm_blsp1_uart6_configs));
#if defined(CONFIG_SEC_LENTIS_PROJECT)
	if (system_rev > 9) {
		msm_gpiomux_install(msm_synaptics_configs_rev10,
						ARRAY_SIZE(msm_synaptics_configs_rev10));
	} else {
		msm_gpiomux_install(msm_synaptics_configs,
						ARRAY_SIZE(msm_synaptics_configs));
	}
#else
	msm_gpiomux_install(msm_synaptics_configs,
					ARRAY_SIZE(msm_synaptics_configs));
#endif

#if defined(CONFIG_SAMSUNG_JACK)
	msm_gpiomux_install(apq8084_ear_configs,
					ARRAY_SIZE(apq8084_ear_configs));
#endif
	if (of_board_is_liquid() || of_board_is_sbc()) {
		msm_gpiomux_install(msm_blsp2_uart5_configs,
				ARRAY_SIZE(msm_blsp2_uart5_configs));
		msm_gpiomux_install(msm_qca1530_liquid_configs,
				ARRAY_SIZE(msm_qca1530_liquid_configs));
	} else {
		msm_gpiomux_install(mdm_configs, ARRAY_SIZE(mdm_configs));

#if !defined(CONFIG_TDMB) && !defined(CONFIG_ISDBT_FC8300)
		if (of_board_is_cdp())
			msm_gpiomux_install(msm_qca1530_cdp_configs,
					ARRAY_SIZE(msm_qca1530_cdp_configs));
#endif
	}
#if 0
	msm_gpiomux_install_nowrite(msm_lcd_configs,
			ARRAY_SIZE(msm_lcd_configs));
#endif
	msm_gpiomux_install(msm_lcd_te_configs,
			ARRAY_SIZE(msm_lcd_te_configs));

	msm_gpiomux_install(apq8084_pri_ter_auxpcm_configs,
			ARRAY_SIZE(apq8084_pri_ter_auxpcm_configs));

	if (of_board_is_sbc()) {
		msm_gpiomux_install(msm_hdmi_nomux_configs,
			ARRAY_SIZE(msm_hdmi_nomux_configs));
		msm_gpiomux_install(apq8084_general_led_configs,
			ARRAY_SIZE(apq8084_general_led_configs));
	} else {
		msm_gpiomux_install(apq8084_hsic_configs,
			ARRAY_SIZE(apq8084_hsic_configs));
#if 0
		msm_gpiomux_install_nowrite(msm_hdmi_configs,
			ARRAY_SIZE(msm_hdmi_configs));
#endif
		msm_gpiomux_install(msm_hdmi_configs,
			ARRAY_SIZE(msm_hdmi_configs));
	}
	msm_gpiomux_install(hall_sensor_config, ARRAY_SIZE(hall_sensor_config));
#if defined(CONFIG_SEC_MHL_SII8620)
	if (system_rev > 0)
		msm_hdmi_SII8620_configs[1].gpio = 31;
	msm_gpiomux_install(msm_hdmi_SII8620_configs,
				ARRAY_SIZE(msm_hdmi_SII8620_configs));
#endif
#ifdef CONFIG_SEC_MHL_SII8240
	msm_gpiomux_install(mhl_configs, ARRAY_SIZE(mhl_configs));
#endif
#if defined (CONFIG_SND_SOC_ES705)
	msm_gpiomux_install(es705_config, ARRAY_SIZE(es705_config));
#endif
	msm_gpiomux_install(msm_taiko_config, ARRAY_SIZE(msm_taiko_config));

#if !defined(CONFIG_SEC_TRLTE_PROJECT) && !defined(CONFIG_SEC_TBLTE_PROJECT)
	msm_gpiomux_install(msm_wlan_configs, ARRAY_SIZE(msm_wlan_configs));
#endif
#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
	if (system_rev > 7)
		msm_gpiomux_install(msm_wlan_configs, ARRAY_SIZE(msm_wlan_configs));
#endif

	msm_gpiomux_install(sd_card_det, ARRAY_SIZE(sd_card_det));
	if (of_board_is_cdp() || of_board_is_sbc())
		msm_gpiomux_install(eth_pwr, ARRAY_SIZE(eth_pwr));
	if (of_board_is_sbc())
		msm_gpiomux_install(msm_sbc_sensor_configs,
				ARRAY_SIZE(msm_sbc_sensor_configs));
	else
		msm_gpiomux_install(msm_sensor_configs,
				ARRAY_SIZE(msm_sensor_configs));

#if defined (CONFIG_MSMB_CAMERA)
#if defined(CONFIG_SEC_LENTIS_PROJECT)
	if (system_rev > 3) {
            pr_info("Actuator i2c mux config\n");
            msm_gpiomux_install(msm_actuator_configs,
                                ARRAY_SIZE(msm_actuator_configs));
        }
#elif defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
        if (system_rev > 1) {
            pr_info("Actuator i2c mux config\n");
            msm_gpiomux_install(msm_actuator_configs,
                                ARRAY_SIZE(msm_actuator_configs));
        }
#else
	pr_info("Actuator i2c mux config\n");
	msm_gpiomux_install(msm_actuator_configs,
		ARRAY_SIZE(msm_actuator_configs));
#endif
#endif

	msm_gpiomux_install(msm_pcie_configs, ARRAY_SIZE(msm_pcie_configs));
#if !defined(CONFIG_SND_SOC_MAX98504) && !defined(CONFIG_SND_SOC_MAX98504A)
	msm_gpiomux_install(msm_epm_configs, ARRAY_SIZE(msm_epm_configs));
#endif
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_G)
	msm_gpiomux_install(apq8084_tsp_configs,
		ARRAY_SIZE(apq8084_tsp_configs));
#endif

#if defined(CONFIG_INPUT_WACOM)
	if (system_rev > 11)
		msm_gpiomux_install(wacom_ic_configs_rev12,
						ARRAY_SIZE(wacom_ic_configs_rev12));
	else
		msm_gpiomux_install(wacom_ic_configs,
						ARRAY_SIZE(wacom_ic_configs));
#endif

#if !defined(CONFIG_SEC_TBLTE_JPN)
#if defined(CONFIG_KEYBOARD_CYPRESS_TOUCHKEY) || defined(CONFIG_KEYBOARD_CYPRESS_TOUCHKEY_T)
	msm_gpiomux_install(apq8084_tkey_configs,
		ARRAY_SIZE(apq8084_tkey_configs));
#endif
#endif 

#if defined(CONFIG_SENSORS_SSP)
#if defined(CONFIG_SEC_LENTIS_PROJECT)
	if (system_rev > 7) {
		msm_gpiomux_install(ssp_configs_lentis_rev08, ARRAY_SIZE(ssp_configs_lentis_rev08));
	} else {
		msm_gpiomux_install(ssp_configs, ARRAY_SIZE(ssp_configs));
	}
	if (system_rev > 6) {
		msm_gpiomux_install(ssp_configs_rev07, ARRAY_SIZE(ssp_configs_rev07));
	}
#elif defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
	if (system_rev > 11)
		msm_gpiomux_install(ssp_configs_rev12, ARRAY_SIZE(ssp_configs_rev12));
	else
		msm_gpiomux_install(ssp_configs, ARRAY_SIZE(ssp_configs));
#else
	msm_gpiomux_install(ssp_configs, ARRAY_SIZE(ssp_configs));
#endif
#endif

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
	if (of_board_is_cdp())
		msm_gpiomux_install(msm_eth_configs,
			ARRAY_SIZE(msm_eth_configs));
#endif

#if defined(CONFIG_SENSORS_VFS61XX) || defined(CONFIG_SENSORS_VFS7XXX)
	msm_gpiomux_install(apq8084_fingerprint_configs,
		ARRAY_SIZE(apq8084_fingerprint_configs));
#if defined(CONFIG_SEC_LENTIS_PROJECT)
	if (system_rev > 6) {
		msm_gpiomux_install(apq8084_fingerprint_configs_rev07,
			ARRAY_SIZE(apq8084_fingerprint_configs_rev07));
        }
#elif defined(CONFIG_SEC_KCCAT6_PROJECT)
#else
	if (system_rev > 9) {
		msm_gpiomux_install(apq8084_fingerprint_configs_rev10,
			ARRAY_SIZE(apq8084_fingerprint_configs_rev10));
	} else {
		msm_gpiomux_install(apq8084_fingerprint_configs_ldo1,
			ARRAY_SIZE(apq8084_fingerprint_configs_ldo1));
        }

	if (system_rev > 2) {
		msm_gpiomux_install(apq8084_fingerprint_configs_rev03,
			ARRAY_SIZE(apq8084_fingerprint_configs_rev03));
        }
#endif
#endif
#if defined(CONFIG_SENSORS_MAX86900)
	msm_gpiomux_install(apq8084_hrm_configs,
		ARRAY_SIZE(apq8084_hrm_configs));

#endif
#if defined(CONFIG_SENSORS_MAX86902)
	if (system_rev < 10)
	msm_gpiomux_install(apq8084_hrm_configs,
		ARRAY_SIZE(apq8084_hrm_configs));
	else
		msm_gpiomux_install(apq8084_hrm_configs_rev10,
			ARRAY_SIZE(apq8084_hrm_configs_rev10));
#endif

#if defined(CONFIG_SENSORS_SX9306)
	msm_gpiomux_install(msm_grip_configs, ARRAY_SIZE(msm_grip_configs));
#endif

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE) || defined(CONFIG_LINK_DEVICE_SPI)
	msm_gpiomux_install(apq8084_tdmb_configs,
		ARRAY_SIZE(apq8084_tdmb_configs));
#endif

#if defined(CONFIG_ISDBT_FC8300)
	if (system_rev >= 7)
		msm_gpiomux_install(apq8084_isdbt_configs_rev07, ARRAY_SIZE(apq8084_isdbt_configs_rev07));
	else if (system_rev > 3)
		msm_gpiomux_install(apq8084_isdbt_configs_rev04, ARRAY_SIZE(apq8084_isdbt_configs_rev04));
	else
		msm_gpiomux_install(apq8084_isdbt_configs, ARRAY_SIZE(apq8084_isdbt_configs));
#elif defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
	msm_gpiomux_install(gpio_wcinokb_configs, ARRAY_SIZE(gpio_wcinokb_configs));
#endif
#if defined(CONFIG_SEC_TRLTE_PROJECT)
	if (system_rev > 6)
		msm_gpiomux_install(apq8084_cover_uart_configs,
				ARRAY_SIZE(apq8084_cover_uart_configs));
	else
		msm_gpiomux_install(apq8084_cover_i2c_configs,
				ARRAY_SIZE(apq8084_cover_i2c_configs));
#endif

#if defined(CONFIG_SND_SOC_MAX98504) || defined(CONFIG_SND_SOC_MAX98504A)
#if defined(CONFIG_SEC_TRLTE_CHNDUOS)
	if (system_rev > 5) {
		msm_gpiomux_install(apq8084_audio_configs_rev06,
			ARRAY_SIZE(apq8084_audio_configs_rev06));
	} else {
		msm_gpiomux_install(apq8084_audio_configs,
			ARRAY_SIZE(apq8084_audio_configs));
	}
#else
	msm_gpiomux_install(apq8084_audio_configs,
		ARRAY_SIZE(apq8084_audio_configs));
#endif
#endif
#if defined(CONFIG_NFC_PN547)
	msm_gpiomux_install(msm_nfc_configs, ARRAY_SIZE(msm_nfc_configs));
#if defined(CONFIG_SEC_LENTIS_PROJECT)
    if (system_rev > 7) {
        msm_gpiomux_install(msm_nfc_configs_rev08, ARRAY_SIZE(msm_nfc_configs_rev08));
    }
#endif
#endif

#if defined(CONFIG_SEC_TBLTE_JPN)
	msm_gpiomux_install(msm_hrm_max86900_config_configs,
		ARRAY_SIZE(msm_hrm_max86900_config_configs));
#endif

#if defined (CONFIG_SEC_NFC_SENN3AB)

		if(system_rev >= 7) {
			msm_gpiomux_install(apq8084_senn3ab_configs_rev07,
				ARRAY_SIZE(apq8084_senn3ab_configs_rev07) );
		} else {
			msm_gpiomux_install(apq8084_senn3ab_configs,
				ARRAY_SIZE(apq8084_senn3ab_configs));
		}
#endif /* CONFIG_SEC_NFC_SENN3AB */

	msm_gpiomux_install(cover_id_config, ARRAY_SIZE(cover_id_config));

#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)

	/* tsp (stm) */
	msm_gpiomux_install(stm_tsp_config, ARRAY_SIZE(stm_tsp_config));

	/* sub PMIC(s2mpb01) */
	msm_gpiomux_install(sub_pmic_config, ARRAY_SIZE(sub_pmic_config));

	/* set NC, i2c pin */
	pr_info("%s: gpio rev%d: common pin config\n", __func__, system_rev);

	msm_gpiomux_install(msm_fpga_configs, ARRAY_SIZE(msm_fpga_configs));	
	/* T common */
	msm_gpiomux_install(gpio_nc_configs, ARRAY_SIZE(gpio_nc_configs));

	switch(system_rev) {
		case 0 ... 5:
			break;
		case 6:		/* gpio rev 06 */
		msm_gpiomux_install(gpio_nc_configs_r06,
				ARRAY_SIZE(gpio_nc_configs_r06));
		msm_gpiomux_install(gpio_i2c_configs_r06,
				ARRAY_SIZE(gpio_i2c_configs_r06));
			break;
		case 7:		/* gpio rev 07 */
		default:	/* or higher */
#if !defined(CONFIG_ISDBT_FC8300)
#if !defined(CONFIG_SEC_TRLTE_CHNDUOS) && !defined(CONFIG_SENSORS_SX9306)
		msm_gpiomux_install(gpio_nc_configs_r07,
				ARRAY_SIZE(gpio_nc_configs_r07));
#endif
#endif
		msm_gpiomux_install(gpio_i2c_configs_r07,
				ARRAY_SIZE(gpio_i2c_configs_r07));
			break;
	}
#endif	/* CONFIG_SEC_TRLTE_PROJECT, CONFIG_SEC_TBLTE_PROJECT */

#if defined(CONFIG_SEC_TRLTE_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
	if (system_rev > 9)
		msm_gpiomux_install(wlan_wake_msm_configs, ARRAY_SIZE(wlan_wake_msm_configs));
#endif /* CONFIG_SEC_TRLTE_PROJECT || CONFIG_SEC_TBLTE_PROJECT  */
}
