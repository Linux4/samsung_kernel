/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>

/*static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};*/

static struct gpiomux_setting gpio_spi_config[]  = {
	{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
	},

	{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
	},
};
//zyk for 86
#if defined (CONFIG_MACH_CS02_SWA) || defined(CONFIG_MACH_CS02VE_ZC) || defined(CONFIG_MACH_CS02VE_ZM)
static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
#else
static struct gpiomux_setting sensor_i2c_qup_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sensor_i2c_qup_suspend_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
#endif
//zyk for 10,11
/*static struct gpiomux_setting gpio_cam_i2c_config = {
	.func = GPIOMUX_FUNC_1,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};*/
static struct gpiomux_setting gpio_suspend_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,  /* IN-NP */  //IN-PD
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN, //zyk for 16,17
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* O-LOW */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_LOW,
	},
};
static struct gpiomux_setting zinitix_i2c_act_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting zinitix_i2c_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting atmel_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting atmel_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting focaltech_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting focaltech_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting focaltech_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting focaltech_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5wire_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5wire_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

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

static struct gpiomux_setting lcd_te_act_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting lcd_te_sus_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting gpio_keys_active = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting gpio_keys_suspend = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

/* define gpio used as interrupt input */
static struct gpiomux_setting gpio_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting gpio_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting gpio_nc_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};//zyk for nc sus

static struct gpiomux_setting gpio_nc_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};//zyk for nc act

static struct msm_gpiomux_config msm_gpio_int_configs[] __initdata = {
	{
		.gpio = 84,
		.settings = {
			[GPIOMUX_ACTIVE]	= &gpio_int_act_cfg,
			[GPIOMUX_SUSPENDED]	= &gpio_int_sus_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_lcd_configs[] __initdata = {
	{
		.gpio = 41,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
	{
		.gpio = 78,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_te_act_config,
			[GPIOMUX_SUSPENDED] = &lcd_te_sus_config,
		},
	},
	{
		.gpio = 60,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
};

static struct gpiomux_setting gpio_hall_ic_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
	{
		.gpio      = 2,		/* TOUCH GPIO I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE]    = &zinitix_i2c_act_cfg,
			[GPIOMUX_SUSPENDED] = &zinitix_i2c_sus_cfg, //zyk for 2
		},
	},
	{
		.gpio      = 3,		/* TOUCH GPIO  I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE]    = &zinitix_i2c_act_cfg,
			[GPIOMUX_SUSPENDED] = &zinitix_i2c_sus_cfg, //zyk for 3
		},
	},
	{
		.gpio      = 12,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_te_act_config,
			[GPIOMUX_SUSPENDED] = &lcd_te_sus_config,//zyk for 12
		},
	},
#if defined (CONFIG_MACH_CS02_SWA) || defined(CONFIG_MACH_CS02VE_ZC) || defined(CONFIG_MACH_CS02VE_ZM)

	{
		.gpio      = 10,	/* BLSP1 QUP3 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 11,	/* BLSP1 QUP3 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
#else
	{
		.gpio      = 10,	/* BLSP1 QUP3 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE]    = &sensor_i2c_qup_cfg,
			[GPIOMUX_SUSPENDED] = &sensor_i2c_qup_suspend_cfg,//zyk for 10,11
		},
	},
	{
		.gpio      = 11,	/* BLSP1 QUP3 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE]    = &sensor_i2c_qup_cfg,
			[GPIOMUX_SUSPENDED] = &sensor_i2c_qup_suspend_cfg,
		},
	},
#endif
	{
		.gpio      = 76,	/* HALL IC */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_hall_ic_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_hall_ic_cfg,
		},
	},
	//{
		//.gpio      = 17,	/* BLSP1 QUP6 I2C_SCL */
		//.settings = {
		//	[GPIOMUX_SUSPENDED] = &gpio_cam_i2c_config,
		//},
	//},
};

static struct gpiomux_setting nfc_i2c_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config nfc_i2c_gpio_config[] __initdata = {
	{
		.gpio = 6,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_i2c_config,
			[GPIOMUX_SUSPENDED] = &nfc_i2c_config,
		},
	},
	{
		.gpio = 7,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_i2c_config,
			[GPIOMUX_SUSPENDED] = &nfc_i2c_config,
		},
	},
};

static struct msm_gpiomux_config msm_atmel_configs[] __initdata = {
	{
		.gpio      = 0,		/* TOUCH RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &atmel_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_reset_sus_cfg,
		},
	},
	{
		.gpio      = 1,		/* TOUCH INT */
		.settings = {
			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,
		},
	},
	{
		.gpio      = 86,		/* BLSP1 QUP4 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_spi_config[1], //zyk for 86
		},
	},
	{
		.gpio      = 87,		/* BLSP1 QUP4 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config[0],
		},
	},
	{
		.gpio      = 89,		/* BLSP1 QUP4 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config[0],
		},
	},
	{
		.gpio      = 88,		/* BLSP1 QUP4 SPI_CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config[0],
		},
	},
};

static struct msm_gpiomux_config msm_focaltech_configs[] __initdata = {
	{
		.gpio      = 0,		/* TOUCH RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &focaltech_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &focaltech_reset_sus_cfg,
		},
	},
	{
		.gpio      = 1,		/* TOUCH INT */
		.settings = {
			[GPIOMUX_ACTIVE] = &focaltech_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &focaltech_int_sus_cfg,
		},
	},
	{
		.gpio      = 86,		/* BLSP1 QUP4 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_spi_config[1], //zyk for 86
		},
	},
	{
		.gpio      = 87,		/* BLSP1 QUP4 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config[0],
		},
	},
	{
		.gpio      = 89,		/* BLSP1 QUP4 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config[0],
		},
	},
	{
		.gpio      = 88,		/* BLSP1 QUP4 SPI_CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config[0],
		},
	},
};


static struct msm_gpiomux_config wcnss_5wire_interface[] = {
	{
		.gpio = 23,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 26,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 27,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
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
		.func = GPIOMUX_FUNC_GPIO, /*suspend*/ /* 1 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
		.dir = GPIOMUX_IN,
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
		.func = GPIOMUX_FUNC_1, /*active 1*/ /* 5*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_IN,
	},
};

static struct msm_gpiomux_config msm_sensor_configs[] __initdata = {
	{
		.gpio = 13, /* CAM_MCLK0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 14, /* rev02 nc*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_nc_act_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_nc_sus_cfg, //zyk for 14
		},
	},
	{
		.gpio = 16, /* CCI_I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[5],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 17, /* CCI_I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[5],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 18, /* FLASH_LED_EN */ //rev02 nc
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_nc_act_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_nc_sus_cfg,//zyk for 18
		},
	},
	{
		.gpio = 19, /* FLASH_LED_NOW */ //rev02 nc
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_nc_act_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_nc_sus_cfg,//zyk for 19
		},
	},
	{
		.gpio = 15, /* CAM1_RST_N */ //rev02 nc
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_nc_act_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_nc_sus_cfg,//zyk for 15
		},
	},
	{
		.gpio = 20, /* WEBCAM1_STANDBY */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 21, /* WEBCAM2_RESET_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 77, /* WEBCAM1_STANDBY */ //rev02 nc
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_nc_act_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_nc_sus_cfg, //zyk for 77
		},
	},
	{
		.gpio = 99, /* CAM_A_EN */
		.settings = {
		[GPIOMUX_ACTIVE]	= &cam_settings[3],
		[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
};

static struct msm_gpiomux_config msm_keypad_configs[] __initdata = {
	{
		.gpio = 72,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_keys_active,
			[GPIOMUX_SUSPENDED] = &gpio_keys_suspend,
		},
	},
	{
		.gpio = 73,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_keys_active,
			[GPIOMUX_SUSPENDED] = &gpio_keys_suspend,
		},
	},
	{
		.gpio = 74,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_keys_active,
			[GPIOMUX_SUSPENDED] = &gpio_keys_suspend,
		},
	},
};

static struct gpiomux_setting sd_card_det_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting sd_card_det_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config sd_card_det[] __initdata = {
	{
		.gpio = 42,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sd_card_det_active_config,
			[GPIOMUX_SUSPENDED] = &sd_card_det_suspend_config,
		},
	},
};

static struct gpiomux_setting muic_active_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
		.dir = GPIOMUX_IN,
	}
};

static struct gpiomux_setting muic_suspend_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
		.dir = GPIOMUX_IN,
	}
};

static struct msm_gpiomux_config muic_gpio_config[] __initdata = {
	{
		.gpio = 8,
		.settings = {
			[GPIOMUX_ACTIVE]    = &muic_active_config[0],
			[GPIOMUX_SUSPENDED] = &muic_suspend_config[0],
		},
	},
	{
		.gpio = 9,
		.settings = {
			[GPIOMUX_ACTIVE]    = &muic_active_config[0],
			[GPIOMUX_SUSPENDED] = &muic_suspend_config[0],
		},
	},
};

static struct gpiomux_setting mot_en_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting mot_en_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config mot_en[] __initdata = {
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mot_en_active_config,
			[GPIOMUX_SUSPENDED] = &mot_en_suspend_config,
		},
	},
};//zyk for 85


void __init msm8610_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	msm_gpiomux_install(msm_blsp_configs, ARRAY_SIZE(msm_blsp_configs));
	if (of_board_is_qrd()) {
		msm_gpiomux_install(msm_focaltech_configs,
			ARRAY_SIZE(msm_focaltech_configs));
	} else {
		msm_gpiomux_install(msm_atmel_configs,
			ARRAY_SIZE(msm_atmel_configs));
	}
	msm_gpiomux_install(wcnss_5wire_interface,
			ARRAY_SIZE(wcnss_5wire_interface));
	msm_gpiomux_install_nowrite(msm_lcd_configs,
				ARRAY_SIZE(msm_lcd_configs));
	msm_gpiomux_install(msm_keypad_configs,
				ARRAY_SIZE(msm_keypad_configs));
	msm_gpiomux_install(sd_card_det, ARRAY_SIZE(sd_card_det));
	msm_gpiomux_install(mot_en, ARRAY_SIZE(mot_en));
	msm_gpiomux_install(msm_sensor_configs, ARRAY_SIZE(msm_sensor_configs));
	msm_gpiomux_install(msm_gpio_int_configs,
			ARRAY_SIZE(msm_gpio_int_configs));
	msm_gpiomux_install(muic_gpio_config, ARRAY_SIZE(muic_gpio_config));
    msm_gpiomux_install(nfc_i2c_gpio_config, ARRAY_SIZE(nfc_i2c_gpio_config));

}
