/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/camera.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>
#include <mach/gpiomux.h>
#include "devices.h"
#include "board-8960.h"
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/ctype.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>
#include <mach/msm8960-gpio.h>
#include <mach/msm_iomap.h>
#include <mach/camera.h>
#include <linux/spi/spi.h>
#ifdef CONFIG_LEDS_AAT1290A
#include <linux/leds-aat1290a.h>
#endif
#define system_rev 13
#if 0
#if (defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)) && \
	defined(CONFIG_I2C)

static struct i2c_board_info cam_expander_i2c_info[] = {
	{
		I2C_BOARD_INFO("sx1508q", 0x22),
		.platform_data = &msm8960_sx150x_data[SX150X_CAM]
	},
};

static struct msm_cam_expander_info cam_expander_info[] = {
	{
		cam_expander_i2c_info,
		MSM_8960_GSBI4_QUP_I2C_BUS_ID,
	},
};
#endif
#endif

void power_on_flash(void);

static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 1*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 2*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 3*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_5, /*active 4*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_6, /*active 5*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_2, /*active 6*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_3, /*active 7*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*i2c suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},
	{
		.func = GPIOMUX_FUNC_1, /* drive strength for D2*/
#if defined(CONFIG_MACH_M2_VZW)
		.drv = GPIOMUX_DRV_2MA,
#else
		.drv = GPIOMUX_DRV_4MA,
#endif
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_4, /* sub camera of D2 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_4, /*active 11 : sub camera of ApexQ*/
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_NONE,
	},
};

static struct msm_gpiomux_config msm8960_cam_common_configs[] = {
#ifdef CONFIG_S5C73M3
	{
		.gpio = GPIO_MSM_FLASH_NOW,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MCLK0,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[9],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#if !defined(CONFIG_MACH_M2_DCM) && !defined(CONFIG_MACH_K2_KDI)
	{
		.gpio = CAM2_RST_N,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
	{
		.gpio = ISP_RESET,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#else
#if !defined(CONFIG_MACH_STRETTO)
	{
		.gpio = GPIO_MSM_FLASH_CNTL_EN,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
#if defined(CONFIG_MACH_INFINITE)/* >=REV04 */
	{
		.gpio = GPIO_MSM_FLASH_CNTL_EN2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
	{
		.gpio = GPIO_MSM_FLASH_NOW,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#if defined(CONFIG_MACH_AEGIS2)/* >=REV07 */
	{
		.gpio = GPIO_MSM_FLASH_NOW2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
#if !(defined(CONFIG_MACH_AEGIS2) || defined(CONFIG_MACH_JASPER))
	{
		.gpio = GPIO_MAIN_CAM_STBY,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
	{
		.gpio = GPIO_CAM_MCLK,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#if !defined(CONFIG_MACH_STRETTO)
	{
		.gpio = GPIO_VT_STBY,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
	{
		.gpio = GPIO_CAM2_RST_N,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM1_RST_N,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#if defined(CONFIG_MACH_STRETTO) || defined(CONFIG_MACH_INFINITE)
	{
		.gpio = GPIO_CAM_MCLK2,
		.settings = {
			[GPIOMUX_ACTIVE]	= &cam_settings[10],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
#endif
};

#ifdef CONFIG_MSM_CAMERA

#if defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX)
static struct msm_gpiomux_config msm8960_cam_2d_configs[] = {
	{
		.gpio = 2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 20,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 21,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};
#if !defined (CONFIG_MACH_ESPRESSO10_SPR)
static struct msm_gpiomux_config msm8960_cam_2d_configs_v2[] = {
	{
		.gpio = 2,
		.settings = {
#if defined(CONFIG_MACH_APEXQ)
			[GPIOMUX_ACTIVE]    = &cam_settings[11],
#else
			[GPIOMUX_ACTIVE]    = &cam_settings[10],
#endif
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 20,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 21,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};
#endif
static uint16_t msm_cam_gpio_2d_tbl[] = {
	5, /*CAMIF_MCLK*/
	20, /*CAMIF_I2C_DATA*/
	21, /*CAMIF_I2C_CLK*/
};

static uint16_t msm_cam_gpio_2d_tbl_v2[] = {
	2, /*CAMIF_MCLK*/
	20, /*CAMIF_I2C_DATA*/
	21, /*CAMIF_I2C_CLK*/
};

static struct msm_camera_gpio_conf rear_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs),
	.cam_gpio_tbl = msm_cam_gpio_2d_tbl,
	.cam_gpio_tbl_size = ARRAY_SIZE(msm_cam_gpio_2d_tbl),
};

static struct msm_camera_gpio_conf front_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs),
	.cam_gpio_tbl = msm_cam_gpio_2d_tbl,
	.cam_gpio_tbl_size = ARRAY_SIZE(msm_cam_gpio_2d_tbl),
};

#elif defined(CONFIG_ISX012) || defined(CONFIG_S5K8AAY) \
		|| defined(CONFIG_S5K5CCGX) || defined(CONFIG_SR030PC50)\
		|| defined(CONFIG_DB8131M) || defined(CONFIG_S5K4ECGX)
static struct msm_gpiomux_config msm8960_cam_2d_configs[] = {
	{
		.gpio = GPIO_I2C_DATA_CAM,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_I2C_CLK_CAM,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};
#if !(defined(CONFIG_SR030PC50)&&defined(CONFIG_S5K5CCGX)) && !defined(CONFIG_MACH_GOGH) && !defined (CONFIG_MACH_ESPRESSO10_SPR)
static struct msm_gpiomux_config msm8960_cam_2d_configs_v2[] = {
	{
		.gpio = 2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[10],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_I2C_DATA_CAM,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_I2C_CLK_CAM,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};
#endif
#endif

#if defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX)
struct msm_camera_sensor_strobe_flash_data strobe_flash_xenon = {
	.flash_trigger = GPIO_MSM_FLASH_NOW,
	.flash_charge = GPIO_MSM_FLASH_CNTL_EN,
	.flash_charge_done = GPIO_VFE_CAMIF_TIMER3_INT,
	.flash_recharge_duration = 50000,
	.irq = MSM_GPIO_TO_INT(GPIO_VFE_CAMIF_TIMER3_INT),
};
#if 0
#ifdef CONFIG_MSM_CAMERA_FLASH
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_EXT,
	._fsrc.ext_driver_src.led_en = GPIO_MSM_FLASH_CNTL_EN,
	._fsrc.ext_driver_src.led_flash_en = GPIO_MSM_FLASH_NOW,
};
#endif

#elif defined(CONFIG_ISX012) || defined(CONFIG_S5K8AAY)\
		|| defined(CONFIG_S5K5CCGX) || defined(CONFIG_SR030PC50)\
		|| defined(CONFIG_DB8131M) || defined(CONFIG_S5K4ECGX)
static struct msm_camera_sensor_strobe_flash_data strobe_flash_xenon = {
	.flash_trigger = GPIO_MSM_FLASH_NOW,
	.flash_charge = GPIO_MSM_FLASH_CNTL_EN,
	.flash_charge_done = GPIO_MAIN_CAM_STBY,
	.flash_recharge_duration = 50000,
	.irq = MSM_GPIO_TO_INT(GPIO_MAIN_CAM_STBY),
};

#ifdef CONFIG_MSM_CAMERA_FLASH
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_EXT,
	._fsrc.ext_driver_src.led_en = GPIO_MSM_FLASH_CNTL_EN,
	._fsrc.ext_driver_src.led_flash_en = GPIO_MSM_FLASH_NOW,
};
#endif
#endif
#endif

static struct msm_bus_vectors cam_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 27648000,
		.ib  = 110592000,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 154275840,
		.ib  = 617103360,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 274423680,
		.ib  = 1097694720,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_vectors cam_zsl_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 689376000,
		.ib  = 1208286720,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_paths cam_bus_client_config[] = {
	{
		ARRAY_SIZE(cam_init_vectors),
		cam_init_vectors,
	},
	{
		ARRAY_SIZE(cam_preview_vectors),
		cam_preview_vectors,
	},
	{
		ARRAY_SIZE(cam_video_vectors),
		cam_video_vectors,
	},
	{
		ARRAY_SIZE(cam_snapshot_vectors),
		cam_snapshot_vectors,
	},
	{
		ARRAY_SIZE(cam_zsl_vectors),
		cam_zsl_vectors,
	},
};

static struct msm_bus_scale_pdata cam_bus_client_pdata = {
		cam_bus_client_config,
		ARRAY_SIZE(cam_bus_client_config),
		.name = "msm_camera",
};

#if defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX)
static struct msm_camera_device_platform_data msm_camera_csi_device_data[] = {
	{
		.ioclk.mclk_clk_rate = 24000000,
		.ioclk.vfe_clk_rate  = 228570000,
		.csid_core = 0,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
	{
		.ioclk.mclk_clk_rate = 24000000,
		.ioclk.vfe_clk_rate  = 228570000,
		.csid_core = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
};
#elif defined(CONFIG_ISX012) || defined(CONFIG_S5K8AAY)\
		|| defined(CONFIG_S5K5CCGX) || defined(CONFIG_SR030PC50)\
		|| defined(CONFIG_DB8131M) || defined(CONFIG_S5K4ECGX)

static struct msm_camera_device_platform_data msm_camera_csi_device_data[] = {
	{
		.csid_core = 0,
		.is_csiphy = 1,
		.is_csid   = 1,
		.is_ispif  = 1,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
	{
		.csid_core = 1,
		.is_csiphy = 1,
		.is_csid   = 1,
		.is_ispif  = 1,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
};

static struct camera_vreg_t msm_8960_back_cam_vreg[] = {
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vaf", REG_LDO, 2800000, 2800000, 300000},
};

static struct camera_vreg_t msm_8960_front_cam_vreg[] = {
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
};

static struct gpio msm8960_common_cam_gpio[] = {
	{5, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{20, GPIOF_DIR_IN, "CAMIF_I2C_DATA"},
	{21, GPIOF_DIR_IN, "CAMIF_I2C_CLK"},
};
#if !(defined(CONFIG_SR030PC50)&&defined(CONFIG_S5K5CCGX)) && !defined(CONFIG_MACH_GOGH) && !defined (CONFIG_MACH_ESPRESSO10_SPR)
static struct gpio msm8960_common_cam_gpio_v2[] = {
	{2, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{20, GPIOF_DIR_IN, "CAMIF_I2C_DATA"},
	{21, GPIOF_DIR_IN, "CAMIF_I2C_CLK"},
};
#endif
static struct gpio msm8960_front_cam_gpio[] = {
	/*{76, GPIOF_DIR_OUT, "CAM_RESET"},*/
};

static struct gpio msm8960_back_cam_gpio[] = {
	/*{107, GPIOF_DIR_OUT, "CAM_RESET"},*/
};

static struct msm_gpio_set_tbl msm8960_front_cam_gpio_set_tbl[] = {
	/*{76, GPIOF_OUT_INIT_LOW, 1000},
	{76, GPIOF_OUT_INIT_HIGH, 4000},*/
};

static struct msm_gpio_set_tbl msm8960_back_cam_gpio_set_tbl[] = {
	/*{107, GPIOF_OUT_INIT_LOW, 1000},
	{107, GPIOF_OUT_INIT_HIGH, 4000},*/
};

static struct msm_camera_gpio_conf msm_8960_front_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs),
	.cam_gpio_common_tbl = msm8960_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8960_common_cam_gpio),
	.cam_gpio_req_tbl = msm8960_front_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8960_front_cam_gpio),
	.cam_gpio_set_tbl = msm8960_front_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8960_front_cam_gpio_set_tbl),
};

static struct msm_camera_gpio_conf msm_8960_back_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs),
	.cam_gpio_common_tbl = msm8960_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8960_common_cam_gpio),
	.cam_gpio_req_tbl = msm8960_back_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8960_back_cam_gpio),
	.cam_gpio_set_tbl = msm8960_back_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8960_back_cam_gpio_set_tbl),
};
#endif

#if defined(CONFIG_ISX012) && defined(CONFIG_DB8131M) && !defined(CONFIG_MACH_AEGIS2)
static struct regulator *l11, *l18, *l29;
#elif defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX)
static struct regulator *l29, *l28, *isp_core;
#elif defined(CONFIG_SR030PC50) && defined(CONFIG_S5K5CCGX)
static struct regulator *l29;
#elif defined(CONFIG_MACH_AEGIS2)
static struct regulator *l11, *l18;
#elif defined (CONFIG_MACH_ESPRESSO10_SPR)
static struct regulator *l29;
#elif defined(CONFIG_MACH_JASPER)
static struct regulator *l11, *l18, *l29;
#else
static struct regulator *l8,*l11, *l12,*l16, *l18, *l29,*l30, *l28, *isp_core;
#endif
/* CAM power
	CAM_SENSOR_A_2.8		:  GPIO_CAM_A_EN(GPIO 46)
	CAM_SENSOR_IO_1.8		: VREG_L29		: l29
	CAM_AF_2.8				: VREG_L11		: l11
	CAM_SENSOR_CORE1.2		: VREG_L12		: l12
	CAM_ISP_CORE_1.2		: CAM_CORE_EN(GPIO 6)

	CAM_DVDD_1.5		: VREG_L18		: l18
*/

#if defined(CONFIG_S5K5CCGX) && defined(CONFIG_DB8131M) /* jasper */
static void cam_ldo_power_on(int mode)
{
	int ret = 0;

/*static struct camera_vreg_t msm_8960_mt9m114_vreg[] = {
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vaf", REG_LDO, 2800000, 2800000, 300000},
};*/
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
		__func__, __LINE__, mode ? "FRONT" : "REAR");

/*Reset Main clock*/
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	mdelay(5);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	l29 = regulator_get(NULL, "8921_lvs5");
	ret = regulator_enable(l29);
	if (ret)
		printk("error enabling regulator 8921_lvs5\n");
#if defined (CONFIG_MACH_ESPRESSO10_SPR)
static struct camera_vreg_t msm_8960_mt9m114_vreg[] = {
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vaf", REG_LDO, 2800000, 2800000, 300000},
};
#endif
/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);

/*VT core 1.5V - CAM_DVDD_1P2V*/
	l18 = regulator_get(NULL, "8921_l18");
	ret = regulator_set_voltage(l18, 1500000, 1500000);
	if (ret)
		printk("error setting voltage l18\n");
	ret = regulator_enable(l18);
	if (ret)
		printk("error enabling regulator l18\n");

	usleep(10);

/*3M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}

}
static void cam_ldo_power_off(int mode)
{
	int ret = 0;

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
	}
/*static struct camera_vreg_t msm_8960_s5k3l1yx_vreg[] = {
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vaf", REG_LDO, 2800000, 2800000, 300000},
};*/

/*VT core 1.5 - CAM_DVDD_1P2V*/
	if (l18) {
		ret = regulator_disable(l18);
		if (ret)
			printk("error disabling regulator\n");
	}

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (l29) {
		ret = regulator_disable(l29);
		if (ret)
			printk("error disabling regulator\n");
	}

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);

/*3M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);

}
#elif defined(CONFIG_S5K5CCGX) && defined(CONFIG_SR030PC50) /* Espresso */
static void cam_ldo_power_on(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/*3M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);
	printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);
	usleep(10);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
	ret = gpio_get_value(GPIO_CAM_A_EN);
	printk(KERN_DEBUG "check GPIO_CAM_A_EN : %d\n", ret);
	usleep(20);

/*VT core 1.8V - VTCAM_CORE_1.8V*/
	gpio_set_value_cansleep(GPIO_CAM_VTCORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_VTCORE_EN);
	printk(KERN_DEBUG "check GPIO_CAM_VTCORE_EN : %d\n", ret);
	usleep(15);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
		l29 = regulator_get(NULL, "8921_lvs5");
		ret = regulator_enable(l29);
		if (ret)
			printk("error enabling regulator 8921_lvs6\n");

}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");



/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (l29) {
		ret = regulator_disable(l29);
		if (ret)
			printk("error disabling regulator\n");
	}

/*VT core 1.8V - VTCAM_CORE_1.8V*/
	gpio_set_value_cansleep(GPIO_CAM_VTCORE_EN, 0);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);

/*3M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);

}
#elif defined(CONFIG_ISX012) && defined(CONFIG_SR030PC50) /* KonaLTE*/
static void cam_ldo_power_on(int mode)
	{
		int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s KONA CAMERA POWER ON!!\n",
			   __func__, __LINE__, mode ? "FRONT" : "REAR");

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);
	printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	l29 = regulator_get(NULL, "8921_lvs5");
	ret = regulator_enable(l29);
	if (ret)
		printk("error enabling regulator 8921_lvs5\n");
	
	printk(KERN_DEBUG "check CAM_SENSOR_IO_1P8 : %d\n", ret);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	l16 = regulator_get(NULL, "8921_l16");
	ret = regulator_set_voltage(l16, 2800000, 2800000);
	if (ret)
		printk("error setting voltage\n");
	ret = regulator_enable(l16);
	if (ret)
		printk("error enabling regulator\n");

/*VT core 1.5V - CAM_DVDD_1P5V*/
	l30 = regulator_get(NULL, "8921_lvs6");
	ret = regulator_enable(l30);
	if (ret)
		printk("error enabling regulator 8921_lv6\n");
	usleep(20);

#if 1
/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l8 = regulator_get(NULL, "8921_l8");
		ret = regulator_set_voltage(l8, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l8);
		if (ret)
			printk("error enabling regulator\n");
	}
	printk(KERN_DEBUG "check CAM_AF_2P8 : %d\n", ret);
#endif	
}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s KONA CAMERA POWER OFF!!\n",
		   __func__, __LINE__, mode ? "FRONT" : "REAR");
#if 1
/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l8) {
			ret = regulator_disable(l8);
			if (ret)
				printk("error disabling regulator\n");
		}
	}
#endif
/*VT core 1.5 - CAM_DVDD_1P5V*/
	if (l30) {
		ret = regulator_disable(l30);
		if (ret)
			printk(" l30 error disabling regulator\n");
	}

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	if (l16) {
		ret = regulator_disable(l16);
		if (ret)
			printk(" l16 error disabling regulator\n");
	}

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (l29) {
		ret = regulator_disable(l29);
		if (ret)
			printk(" l29 error disabling regulator\n");
	}

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);
    printk(KERN_DEBUG "OFF CAM_CORE_EN \n");

}
#elif defined(CONFIG_ISX012) && defined(CONFIG_S5K8AAY) /* JAGUAR */
static void cam_ldo_power_on(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/*Reset Main clock*/
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	mdelay(5);

/*Flash Unlock*/
	if ((system_rev >= BOARD_REV13) && !mode) { /*HW rev 1.2*/
		gpio_direction_output(PM8921_GPIO_PM_TO_SYS
			(PMIC_GPIO_FLASH_LED_UNLOCK), 1);
	}

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);

/*VT core 1.2V - CAM_DVDD_1P2V*/
	l18 = regulator_get(NULL, "8921_l18");
	ret = regulator_set_voltage(l18, 1200000, 1200000);
	if (ret)
		printk("error setting voltage\n");
	ret = regulator_enable(l18);
	if (ret)
		printk("error enabling regulator\n");

	usleep(500);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	printk(KERN_DEBUG "CAM_SENSOR_IO_1P8 in system_rev = %d\n", system_rev);
	if (system_rev >= BOARD_REV10) { /* HW rev0.9 */
		l29 = regulator_get(NULL, "8921_lvs5");
		ret = regulator_enable(l29);
		if (ret)
			printk("error enabling regulator\n");
	} else {
		if (system_rev >= BOARD_REV04)
			gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, 1);

		if (system_rev == BOARD_REV02) {
			l29 = regulator_get(NULL, "8921_lvs5");
			ret = regulator_enable(l29);
			if (ret)
				printk("error enabling regulator\n");
		} else {
			l29 = regulator_get(NULL, "cam_vio");
			ret = regulator_set_voltage(l29, 1800000, 1800000);
			if (ret)
				printk("error setting voltage\n");
			ret = regulator_enable(l29);
			if (ret)
				printk("error enabling regulator\n");
		}
	}

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}

/*standy VT */
#ifdef CONFIG_MACH_JAGUAR
	gpio_set_value_cansleep(GPIO_VT_STBY, 1);
	ret = gpio_get_value(GPIO_VT_STBY);
	CAM_DEBUG("[isx012] check VT standby : %d", ret);
	mdelay(1);
#endif

/*Set Main clock */
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK, 1, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	mdelay(1);

}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/*Flash Lock*/
#ifdef CONFIG_MACH_JAGUAR
	if ((system_rev >= BOARD_REV13) && !mode) { /*HW rev 1.2*/
		gpio_direction_output(PM8921_GPIO_PM_TO_SYS
			(PMIC_GPIO_FLASH_LED_UNLOCK), 0);
	}
#endif

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
		mdelay(1);
	}

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
#ifdef CONFIG_MACH_JAGUAR
	if (system_rev >= BOARD_REV10) { /* HW rev0.9 */
		if (l29) {
			ret = regulator_disable(l29);
			if (ret)
				printk("error disabling regulator\n");
		}
	} else {
		if (system_rev >= BOARD_REV04) {
			gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, 0);
			mdelay(1);
		}

		if (system_rev <= BOARD_REV02) {
			if (l29) {
				ret = regulator_disable(l29);
				if (ret)
					printk("error disabling regulator\n");
			}
		}
	}
#else
	if (l29) {
		ret = regulator_disable(l29);
		if (ret)
			printk("error disabling regulator\n");
	}
#endif
	mdelay(1);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);
	mdelay(1);

/*VT core 1.2 - CAM_DVDD_1P2V*/
	if (l18) {
		ret = regulator_disable(l18);
		if (ret)
			printk("error disabling regulator\n");
	}
	mdelay(1);

	if (system_rev <= BOARD_REV01) {
		if (l12) {
			ret = regulator_disable(l12);
			if (ret)
				printk("error disabling regulator\n");
		}
		mdelay(1);
	}

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);
	mdelay(1);

}
#elif defined(CONFIG_S5K4ECGX) && defined(CONFIG_DB8131M) /* Aegis2 */
static void cam_ldo_power_on(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	printk(KERN_DEBUG "CAM_SENSOR_IO_1P8 in system_rev = %d\n", system_rev);
	gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, 1);
	ret = gpio_get_value(GPIO_CAM_SENSOR_IO_EN);
	printk(KERN_DEBUG "check CAM_SENSOR_IO_EN : %d\n", ret);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
	ret = gpio_get_value(GPIO_CAM_A_EN);
	printk(KERN_DEBUG "check GPIO_CAM_A_EN : %d\n", ret);

/*VT core 1.2V - CAM_DVDD_1P2V*/
	l18 = regulator_get(NULL, "8921_l18");
	ret = regulator_set_voltage(l18, 1500000, 1500000);
	if (ret)
		printk("error setting voltage\n");
	ret = regulator_enable(l18);
	if (ret)
		printk("error enabling regulator\n");

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);
	printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);

	if (mode) {
		/* front power up seq -5M Core 1.2V = 0 */
		usleep(1000);
		gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);
		ret = gpio_get_value(GPIO_CAM_CORE_EN);
		printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);
	}
/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}
}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");


/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
}

/*VT core 1.2 - CAM_DVDD_1P2V*/
	if (l18) {
		ret = regulator_disable(l18);
		if (ret)
			printk("error disabling regulator\n");
	}

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, 0);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);

}
#elif defined(CONFIG_ISX012) && defined(CONFIG_DB8131M)
u8 torchonoff;
static bool isSensorPowered = false;
#if defined(CONFIG_MACH_GOGH) || defined(CONFIG_MACH_INFINITE)
static void cam_ldo_power_on(int mode)
{
	int ret = 0;
	isSensorPowered = true;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_UNLOCK*/
#if defined(CONFIG_MACH_GOGH)
	if ((system_rev >= BOARD_REV03) && !mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 1);
		ret = gpio_get_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK));
		printk(KERN_DEBUG "check FLASH_LED_UNLOCK : %d\n", ret);
	}
#else
	if (!mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 1);
		ret = gpio_get_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK));
		printk(KERN_DEBUG "check FLASH_LED_UNLOCK : %d\n", ret);
	}
#endif

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);
	printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);
	usleep(1000);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	l29 = regulator_get(NULL, "8921_lvs5");
	ret = regulator_enable(l29);
	if (ret)
		printk("error enabling regulator\n");
	usleep(1000);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
	ret = gpio_get_value(GPIO_CAM_A_EN);
	printk(KERN_DEBUG "check GPIO_CAM_A_EN : %d\n", ret);
	usleep(1000);

/*VT core 1.2V - CAM_DVDD_1P2V*/
	l18 = regulator_get(NULL, "8921_l18");
	ret = regulator_set_voltage(l18, 1500000, 1500000);
	if (ret)
		printk("error setting voltage\n");
	ret = regulator_enable(l18);
	if (ret)
		printk("error enabling regulator\n");


/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}
}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;
	isSensorPowered = false;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_LOCK*/
#if defined(CONFIG_MACH_GOGH)
	if ((system_rev >= BOARD_REV03) && !mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 0);
	}
#else
	if (!mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 0);
	}
#endif

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
		usleep(1000);
	}

/*VT core 1.2 - CAM_DVDD_1P2V*/
	if (l18) {
		ret = regulator_disable(l18);
		if (ret)
			printk("error disabling regulator\n");
	}
	usleep(1000);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);
	usleep(1000);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (l29) {
		ret = regulator_disable(l29);
		if (ret)
			printk("error disabling regulator\n");
	}
	usleep(1000);

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);

}
#elif defined(CONFIG_MACH_AEGIS2)
static void cam_ldo_power_on(int mode)
{
	int ret = 0;
	isSensorPowered = true;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_UNLOCK*/
	if ((system_rev >= BOARD_REV07) && !mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 1);
		ret = gpio_get_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK));
		printk(KERN_DEBUG "check FLASH_LED_UNLOCK : %d\n", ret);
	}

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);
	printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);
	usleep(1000);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, 1);
	ret = gpio_get_value(GPIO_CAM_SENSOR_IO_EN);
	printk(KERN_DEBUG "check CAM_SENSOR_IO_EN : %d\n", ret);
	usleep(1000);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
	ret = gpio_get_value(GPIO_CAM_A_EN);
	printk(KERN_DEBUG "check GPIO_CAM_A_EN : %d\n", ret);
	usleep(1000);

/*VT core 1.2V - CAM_DVDD_1P2V*/
	l18 = regulator_get(NULL, "8921_l18");
	ret = regulator_set_voltage(l18, 1500000, 1500000);
	if (ret)
		printk("error setting voltage\n");
	ret = regulator_enable(l18);
	if (ret)
		printk("error enabling regulator\n");
	usleep(1000);


/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}
}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;
	isSensorPowered = false;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_LOCK*/
	if ((system_rev >= BOARD_REV07) && !mode && torchonoff == 0) {
			gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
					(PMIC_MPP_FLASH_LED_UNLOCK), 0);
	}


/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
	}

/*VT core 1.2 - CAM_DVDD_1P2V*/
	if (l18) {
		ret = regulator_disable(l18);
		if (ret)
			printk("error disabling regulator\n");
	}

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, 0);

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);

}
#elif defined(CONFIG_MACH_COMANCHE)
static void cam_ldo_power_on(int mode)
{
	int ret = 0;
	isSensorPowered = true;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_UNLOCK*/
	if ((system_rev >= BOARD_REV03) && !mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 1);
		ret = gpio_get_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK));
		printk(KERN_DEBUG "check FLASH_LED_UNLOCK : %d\n", ret);
	}

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);
	printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	/* >= HW rev01 */
	l29 = regulator_get(NULL, "8921_lvs5");
	ret = regulator_enable(l29);
	if (ret)
		printk("error enabling regulator\n");

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
	ret = gpio_get_value(GPIO_CAM_A_EN);
	printk(KERN_DEBUG "check GPIO_CAM_A_EN : %d\n", ret);

/*VT core 1.2V - CAM_DVDD_1P2V*/
	l18 = regulator_get(NULL, "8921_l18");
	ret = regulator_set_voltage(l18, 1500000, 1500000);
	if (ret)
		printk("error setting voltage\n");
	ret = regulator_enable(l18);
	if (ret)
		printk("error enabling regulator\n");

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}
}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;
	isSensorPowered = false;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_LOCK*/
	if ((system_rev >= BOARD_REV03) && !mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 0);
	}


/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
	}

/*VT core 1.2 - CAM_DVDD_1P2V*/
	if (l18) {
		ret = regulator_disable(l18);
		if (ret)
			printk("error disabling regulator\n");
	}

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	/* >= HW Rev01 */
	if (l29) {
		ret = regulator_disable(l29);
		if (ret)
			printk("error disabling regulator\n");
	}

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);

}
#elif defined(CONFIG_MACH_APEXQ)
static void cam_ldo_power_on(int mode)
{
	int ret = 0;
	isSensorPowered = true;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_UNLOCK*/
	if ((system_rev >= BOARD_REV03) && !mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 1);
		ret = gpio_get_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK));
		printk(KERN_DEBUG "check FLASH_LED_UNLOCK : %d\n", ret);
	}

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);
	printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);
	usleep(1000);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (system_rev >= BOARD_REV02) {
		gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, 1);
		ret = gpio_get_value(GPIO_CAM_SENSOR_IO_EN);
		printk(KERN_DEBUG "check CAM_SENSOR_IO_EN : %d\n", ret);
	} else {
		l29 = regulator_get(NULL, "8921_lvs5");
		ret = regulator_enable(l29);
		if (ret)
			printk("error enabling regulator\n");
	}
	usleep(1000);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
	ret = gpio_get_value(GPIO_CAM_A_EN);
	printk(KERN_DEBUG "check GPIO_CAM_A_EN : %d\n", ret);
	usleep(1000);

/*VT core 1.2V - CAM_DVDD_1P2V*/
	l18 = regulator_get(NULL, "8921_l18");
	ret = regulator_set_voltage(l18, 1500000, 1500000);
	if (ret)
		printk("error setting voltage\n");
	ret = regulator_enable(l18);
	if (ret)
		printk("error enabling regulator\n");


/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}
}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;
	isSensorPowered = false;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_LOCK*/
	if ((system_rev >= BOARD_REV03) && !mode && torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 0);
	}

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
		usleep(1000);
	}

/*VT core 1.2 - CAM_DVDD_1P2V*/
	if (l18) {
		ret = regulator_disable(l18);
		if (ret)
			printk("error disabling regulator\n");
	}
	usleep(1000);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);
	usleep(1000);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (system_rev >= BOARD_REV02)
		gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, 0);
	else {
		if (l29) {
			ret = regulator_disable(l29);
			if (ret)
				printk("error disabling regulator\n");
		}
	}
	usleep(1000);

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);

}
#elif defined(CONFIG_MACH_EXPRESS)
static void cam_ldo_power_on(int mode)
{
	int ret = 0;
	isSensorPowered = true;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_UNLOCK*/
	if (torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 1);
		ret = gpio_get_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK));
		printk(KERN_DEBUG "check FLASH_LED_UNLOCK : %d\n", ret);
	}
/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);
	ret = gpio_get_value(GPIO_CAM_CORE_EN);
	printk(KERN_DEBUG "check CAM_CORE_EN : %d\n", ret);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	l29 = regulator_get(NULL, "8921_lvs5");
	ret = regulator_enable(l29);
	if (ret)
		printk("error enabling regulator\n");

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
	ret = gpio_get_value(GPIO_CAM_A_EN);
	printk(KERN_DEBUG "check GPIO_CAM_A_EN : %d\n", ret);

/*VT core 1.2V - CAM_DVDD_1P2V*/
	l18 = regulator_get(NULL, "8921_l18");
	ret = regulator_set_voltage(l18, 1500000, 1500000);
	if (ret)
		printk("error setting voltage\n");
	ret = regulator_enable(l18);
	if (ret)
		printk("error enabling regulator\n");

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}
}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;
	isSensorPowered = false;
	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* FLASH_LED_LOCK*/
	if (torchonoff == 0) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 0);
	}

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
	}

/*VT core 1.2 - CAM_DVDD_1P2V*/
	if (l18) {
		ret = regulator_disable(l18);
		if (ret)
			printk("error disabling regulator\n");
	}

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (l29) {
		ret = regulator_disable(l29);
		if (ret)
			printk("error disabling regulator\n");
	}

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);

}

#else
static void cam_ldo_power_on(int mode)
{
	printk(KERN_DEBUG "default cam_ldo_power_on");
}

static void cam_ldo_power_off(int mode)
{
	printk(KERN_DEBUG "default cam_ldo_power_off");
}
#endif
#elif defined(CONFIG_ISX012) && defined(CONFIG_S5K6A3YX) /* stretto */
static void cam_ldo_power_on(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/*Reset Main clock*/
	if (mode) {
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK2, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	} else {
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}
	mdelay(5);

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 1);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	l29 = regulator_get(NULL, "8921_lvs5");
	ret = regulator_enable(l29);
	if (ret)
		printk("error enabling regulator\n");

/* VT XSHUTDOWN */
	if (mode)
		gpio_set_value_cansleep(GPIO_CAM2_RST_N, 1);

	mdelay(5);

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		l11 = regulator_get(NULL, "8921_l11");
		ret = regulator_set_voltage(l11, 2800000, 2800000);
		if (ret)
			printk("error setting voltage\n");
		ret = regulator_enable(l11);
		if (ret)
			printk("error enabling regulator\n");
	}

/*Set Main clock */
	if (mode) {
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK2, 4, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	} else {
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK, 1, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}

	mdelay(5);

}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

/* VT XSHUTDOWN */
	if (mode)
		gpio_set_value_cansleep(GPIO_CAM2_RST_N, 0);

/*Sensor AF 2.8V -CAM_AF_2P8  */
	if (!mode) {
		if (l11) {
			ret = regulator_disable(l11);
			if (ret)
				printk("error disabling regulator\n");
		}
		mdelay(1);
	}

/*Sensor AVDD 2.8V - CAM_SENSOR_A2P8 */
	gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);
	mdelay(1);

/*Sensor IO 1.8V -CAM_SENSOR_IO_1P8  */
	if (l29) {
		ret = regulator_disable(l29);
		if (ret)
			printk("error disabling regulator\n");
	}
	mdelay(1);

/*5M Core 1.2V - CAM_ISP_CORE_1P2*/
	gpio_set_value_cansleep(GPIO_CAM_CORE_EN, 0);
	mdelay(1);

}
#elif defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX) /* D2 */
#if defined(CONFIG_MACH_M2_DCM)
static int vddCore = 1230000;
#else
static int vddCore = 1150000;
#endif
static bool isVddCoreSet;
static u8 gpio_cam_flash_sw;
static u8 pmic_gpio_msm_flash_cntl_en;
static bool isFlashCntlEn;

static void cam_set_isp_core(int level)
{

#if defined(CONFIG_MACH_M2_DCM)
	if (level == 1000000) {
		pr_err("Change core voltage\n");
		vddCore = 1060000;
	} else if (level == 1050000) {
		pr_err("Change core voltage\n");
		vddCore = 1110000;
	} else if (level == 1100000) {
		pr_err("Change core voltage\n");
		vddCore = 1170000;
	} else if (level == 1150000) {
		pr_err("Change core voltage\n");
		vddCore = 1230000;
	} else
		vddCore = level;
#else
	if (level == 1050000) {
		pr_err("Change core voltage\n");
		vddCore = 1100000;
	} else
		vddCore = level;
#endif
	isVddCoreSet = true;
	pr_err("ISP CORE = %d\n", vddCore);
}

static bool cam_is_vdd_core_set(void)
{
	return isVddCoreSet;
}

void power_on_flash(void)
{
	int temp = 0;
	int ret = 0;
	if(!isFlashCntlEn)
	{			
		temp = gpio_get_value(GPIO_MSM_FLASH_NOW);		
		if (isFlashCntlEn == 0 && temp == 0) {		
		/* FLASH_LED_UNLOCK*/
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), 1);
		/* GPIO_CAM_FLASH_SW : tourch */
		gpio_set_value_cansleep(gpio_cam_flash_sw, 1);
		temp = gpio_get_value(gpio_cam_flash_sw);
		printk("[s5c73m3] check GPIO_CAM_FLASH_SW : %d\n", temp);
		usleep(1*1000);
		}

		/* flash power 1.8V */
		l28 = regulator_get(NULL, "8921_lvs4");
		ret = regulator_enable(l28);
		if (ret)
			printk("error enabling regulator\n");
		usleep(1*1000);
		isFlashCntlEn = true;
	}
}
	
static void cam_ldo_power_on(int mode, int num)
{
	int ret = 0;
	int temp = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER ON!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

	if (mode) {		/* front camera */
		if (num == 0) {
			/* ISP CORE 1.2V */
			/* delete for unnecessary power */
			/* 8M AVDD 2.8V */
			gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
			temp = gpio_get_value(GPIO_CAM_A_EN);
			printk(KERN_DEBUG "[s5k6a3yx] check GPIO_CAM_A_EN : %d\n",
				temp);
			usleep(1*1000);
		}	else	{
			/* VT_RESET */
		#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
			gpio_set_value_cansleep(gpio_rev(CAM2_RST_N), 1);
			temp = gpio_get_value(gpio_rev(CAM2_RST_N));
		#else
			gpio_set_value_cansleep(CAM2_RST_N, 1);
			temp = gpio_get_value(CAM2_RST_N);
		#endif
			printk(KERN_DEBUG "[s5k6a3yx] check CAM2_RST_N : %d\n",
				temp);
			usleep(1*1000);

			/* ISP 8M HOST 1.8V */
			l29 = regulator_get(NULL, "8921_lvs5");
			ret = regulator_enable(l29);
			if (ret)
				printk("error enabling regulator\n");
			usleep(1*1000);

			/* ISP 8M MIPI 1.2V */
			/* delete for unnecessary power */
		}
	} else {		/* rear camera */
		if (num == 0) {
			printk(KERN_DEBUG "[s5c73m3] rear camera on 1\n");

			temp = gpio_get_value(GPIO_MSM_FLASH_NOW);

			if (isFlashCntlEn == 0 && temp == 0) {
				/* FLASH_LED_UNLOCK*/
				gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
					(PMIC_MPP_FLASH_LED_UNLOCK), 1);
				/* GPIO_CAM_FLASH_SW : tourch */
				gpio_set_value_cansleep(gpio_cam_flash_sw, 1);
				temp = gpio_get_value(gpio_cam_flash_sw);
				printk(KERN_DEBUG "[s5c73m3] check"
					" GPIO_CAM_FLASH_SW : %d\n", temp);
				usleep(1*1000);
			}

			/* flash power 1.8V */
			l28 = regulator_get(NULL, "8921_lvs4");
			ret = regulator_enable(l28);
			if (ret)
				printk("error enabling regulator\n");
			usleep(1*1000);

			/* ISP CORE 1.2V */
#ifdef CONFIG_MACH_M2_ATT
			if (system_rev >= BOARD_REV03) {
				printk(KERN_DEBUG "[s5c73m3] check vddCore : %d\n",
					vddCore);

				isp_core = regulator_get(NULL, "cam_isp_core");
				ret = regulator_set_voltage(isp_core,
					vddCore, vddCore);
				if (ret)
					printk("error setting voltage\n");

				ret = regulator_enable(isp_core);
				if (ret)
					printk("error enabling regulator.");
			}
#elif defined(CONFIG_MACH_M2_VZW)
			if (system_rev >= BOARD_REV08) {
				printk(KERN_DEBUG "[s5c73m3] vzw check vddCore : %d\n",
					vddCore);

				isp_core = regulator_get(NULL, "cam_isp_core");
				ret = regulator_set_voltage(isp_core,
					vddCore, vddCore);
				if (ret)
					printk("error setting voltage\n");

				ret = regulator_enable(isp_core);
				if (ret)
					printk("error enabling regulator.");
			} else
				gpio_set_value_cansleep(CAM_CORE_EN, 1);
#elif defined(CONFIG_MACH_M2_SPR)
			if (system_rev >= BOARD_REV03) {
				printk(KERN_DEBUG "[s5c73m3] spr check vddCore : %d\n",
					vddCore);

				isp_core = regulator_get(NULL, "cam_isp_core");
				ret = regulator_set_voltage(isp_core,
					vddCore, vddCore);
				if (ret)
					printk("error setting voltage\n");

				ret = regulator_enable(isp_core);
				if (ret)
					printk("error enabling regulator.");
			} else
				gpio_set_value_cansleep(CAM_CORE_EN, 1);
#elif defined(CONFIG_MACH_M2_DCM)
			if (system_rev >= BOARD_REV03) {
				printk(KERN_DEBUG "[s5c73m3] dcm check vddCore : %d\n",
					vddCore);

				isp_core = regulator_get(NULL, "cam_isp_core");
				ret = regulator_set_voltage(isp_core,
					vddCore, vddCore);
				if (ret)
					printk("error setting voltage\n");

				ret = regulator_enable(isp_core);
				if (ret)
					printk("error enabling regulator.");
			} else
				gpio_set_value_cansleep(gpio_rev(CAM_CORE_EN), 1);
#elif defined(CONFIG_MACH_K2_KDI)
			gpio_set_value_cansleep(gpio_rev(CAM_CORE_EN), 1);
#else
			gpio_set_value_cansleep(CAM_CORE_EN, 1);
#endif
			usleep(1200);

			/* 8M AVDD 2.8V */
			gpio_set_value_cansleep(GPIO_CAM_A_EN, 1);
			temp = gpio_get_value(GPIO_CAM_A_EN);
			printk(KERN_DEBUG "[s5c73m3] check GPIO_CAM_A_EN : %d\n",
				temp);
			usleep(1*1000);

			/* 8M DVDD 1.2V */
			gpio_set_value_cansleep(GPIO_CAM_SENSOR_EN, 1);
			temp = gpio_get_value(GPIO_CAM_SENSOR_EN);
			printk(KERN_DEBUG "[s5c73m3] check GPIO_CAM_SENSOR_EN : %d\n",
			       temp);
			usleep(1*1000);
		} else {
			printk(KERN_DEBUG "[s5c73m3] rear camera on 2\n");
			/* AF 2.8V */
			gpio_set_value_cansleep(gpio_rev(CAM_AF_EN), 1);
			temp = gpio_get_value(gpio_rev(CAM_AF_EN));
			printk(KERN_DEBUG "[s5c73m3] check CAM_AF_EN : %d\n",
			       temp);
			usleep(3*1000);

			/* ISP 8M HOST 1.8V */
			l29 = regulator_get(NULL, "8921_lvs5");
			ret = regulator_enable(l29);
			if (ret)
				printk("error enabling regulator\n");

			/* ISP 8M MIPI 1.2V */
			gpio_set_value_cansleep(CAM_MIPI_EN, 1);
			temp = gpio_get_value(CAM_MIPI_EN);
			printk(KERN_DEBUG "[s5c73m3] check CAM_MIPI_EN : %d\n",
				temp);
			usleep(1*1000);

			/* ISP_STANDBY */
			gpio_set_value_cansleep(PM8921_GPIO_PM_TO_SYS(24), 1);
			usleep(1*1000);

			/* ISP_RESET */
			gpio_set_value_cansleep(ISP_RESET, 1);
			temp = gpio_get_value(ISP_RESET);
			printk(KERN_DEBUG "[s5c73m3] check ISP_RESET : %d\n",
				temp);
			usleep(1*1000);
		}
	}

}

static void cam_ldo_power_off(int mode)
{
	int ret = 0;
	int temp = 0;

	printk(KERN_DEBUG "[%s : %d] %s CAMERA POWER OFF!!\n",
	       __func__, __LINE__, mode ? "FRONT" : "REAR");

	if (mode) {		/* front camera */
		/* VT_RESET */
#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
		gpio_set_value_cansleep(gpio_rev(CAM2_RST_N), 0);
		usleep(1*1000);
#else
		gpio_set_value_cansleep(CAM2_RST_N, 0);
		usleep(1*1000);
#endif
		/* ISP 8M MIPI 1.2V */
	  /* delete for unnecessary power */


		/* ISP 8M HOST 1.8V */
		if (l29) {
			ret = regulator_disable(l29);
			if (ret)
				printk("error disabling regulator\n");
		}
		usleep(1*1000);

		/* MCLK 24MHz*/

		/* 8M AVDD 2.8V */
		gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);
		usleep(1*1000);
		/* ISP CORE 1.2V */
	  /* delete for unnecessary power */
	} else {		/* rear camera */
		/* ISP_STANDBY */
		gpio_set_value_cansleep(PM8921_GPIO_PM_TO_SYS(24), 0);
		usleep(1*1000);

		/* ISP_RESET */
		gpio_set_value_cansleep(ISP_RESET, 0);
		usleep(1*1000);

		/* AF 2.8V */
		gpio_set_value_cansleep(gpio_rev(CAM_AF_EN), 0);
		usleep(1*1000);

		/* ISP 8M MIPI 1.2V */
		gpio_set_value_cansleep(CAM_MIPI_EN, 0);
		usleep(1*1000);

		/* ISP 8M HOST 1.8V */
		if (l29) {
			ret = regulator_disable(l29);
			if (ret)
				printk("error disabling regulator\n");
		}
		usleep(1*1000);

		/* 8M DVDD 1.2V */
		gpio_set_value_cansleep(GPIO_CAM_SENSOR_EN, 0);
		usleep(1*1000);

		/* 8M AVDD 2.8V */
		gpio_set_value_cansleep(GPIO_CAM_A_EN, 0);
		usleep(1*1000);

		/* ISP CORE 1.2V */
#ifdef CONFIG_MACH_M2_ATT
		if (system_rev >= BOARD_REV03)
			ret = regulator_disable(isp_core);
		if (ret)
			printk("error disabling regulator");
		regulator_put(isp_core);
#elif defined(CONFIG_MACH_M2_VZW)
		if (system_rev >= BOARD_REV08)
			ret = regulator_disable(isp_core);
		if (ret)
			printk("error disabling regulator");
		regulator_put(isp_core);
#elif defined(CONFIG_MACH_M2_SPR)
		if (system_rev >= BOARD_REV03)
			ret = regulator_disable(isp_core);
		if (ret)
			printk("error disabling regulator");
		regulator_put(isp_core);
#elif defined(CONFIG_MACH_M2_DCM)
		if (system_rev >= BOARD_REV03){
			ret = regulator_disable(isp_core);
			if (ret)
				printk("error disabling regulator");
			regulator_put(isp_core);
		}
		else
			gpio_set_value_cansleep(gpio_rev(CAM_CORE_EN), 0);
#elif defined(CONFIG_MACH_K2_KDI)
		gpio_set_value_cansleep(gpio_rev(CAM_CORE_EN), 0);
#else
		gpio_set_value_cansleep(CAM_CORE_EN, 0);
#endif
		usleep(1*1000);

		/* flash power 1.8V */
		if (l28) {
			ret = regulator_disable(l28);
			if (ret)
				printk("error disabling regulator\n");
		}
		usleep(1*1000);

		/* GPIO_CAM_FLASH_SW : tourch */
		temp = gpio_get_value(GPIO_MSM_FLASH_NOW);

		if (isFlashCntlEn == 0 && temp == 0) {
			/* GPIO_CAM_FLASH_SW : tourch */
			gpio_set_value_cansleep(gpio_cam_flash_sw, 0);
			temp = gpio_get_value(gpio_cam_flash_sw);
			printk(KERN_DEBUG "[s5c73m3] check"
				" GPIO_CAM_FLASH_SW : %d\n", temp);
			/* FLASH_LED_LOCK*/
			gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
				(PMIC_MPP_FLASH_LED_UNLOCK), 0);
			usleep(1*1000);
		}
	}
}

static void cam_isp_reset(void)
{
	int temp = 0;

	/* ISP_RESET */
	gpio_set_value_cansleep(ISP_RESET, 0);
	temp = gpio_get_value(ISP_RESET);
	printk(KERN_DEBUG "[s5c73m3] check ISP_RESET : %d\n", temp);
	usleep(1*1000);
	/* ISP_RESET */
	gpio_set_value_cansleep(ISP_RESET, 1);
	temp = gpio_get_value(ISP_RESET);
	printk(KERN_DEBUG "[s5c73m3] check ISP_RESET : %d\n", temp);
	usleep(1*1000);
}

static u8 *rear_sensor_fw;
static u8 *rear_phone_fw;
static void cam_get_fw(u8 *isp_fw, u8 *phone_fw)
{
	rear_sensor_fw = isp_fw;
	rear_phone_fw = phone_fw;
	pr_debug("sensor_fw = %s\n", rear_sensor_fw);
	pr_debug("phone_fw = %s\n", rear_phone_fw);
}

/* test: Qualcomm */
void print_ldos(void)
{
	int temp = 0;
#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
	temp = gpio_get_value(gpio_rev(CAM_CORE_EN));
#else
	temp = gpio_get_value(CAM_CORE_EN);
#endif
	printk(KERN_DEBUG "[s5c73m3] check CAM_CORE_EN : %d\n", temp);

	temp = gpio_get_value(GPIO_CAM_A_EN);
	printk(KERN_DEBUG "[s5c73m3] check GPIO_CAM_A_EN : %d\n", temp);

	temp = gpio_get_value(GPIO_CAM_SENSOR_EN);
	printk(KERN_DEBUG "[s5c73m3] check GPIO_CAM_SENSOR_EN : %d\n", temp);

	temp = gpio_get_value(CAM_MIPI_EN);
	printk(KERN_DEBUG "[s5c73m3] check CAM_MIPI_EN : %d\n", temp);

	temp = gpio_get_value(PM8921_GPIO_PM_TO_SYS(24));/*SPI_TEMP*/
	printk(KERN_DEBUG "[s5c73m3] check ISP_STANDBY : %d\n", temp);

	temp = gpio_get_value(ISP_RESET);
	printk(KERN_DEBUG "[s5c73m3] check ISP_RESET : %d\n", temp);

}
#else
static void cam_ldo_power_on(int mode)
{
	printk(KERN_DEBUG "default cam_ldo_power_on");
}

static void cam_ldo_power_off(int mode)
{
	printk(KERN_DEBUG "default cam_ldo_power_off");
}
#endif

#ifdef CONFIG_S5C73M3
static struct msm_camera_sensor_flash_data flash_s5c73m3 = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5c73m3 = {
	.mount_angle	= 90,
	.sensor_reset	= ISP_RESET,
	.flash_en	= GPIO_MSM_FLASH_NOW,
	.flash_set	= GPIO_MSM_FLASH_CNTL_EN,
	.mclk	= GPIO_CAM_MCLK0,
	.sensor_pwd	= CAM_CORE_EN,
	.vcm_pwd	= 0,
	.vcm_enable	= 1,
	.sensor_power_on = cam_ldo_power_on,
	.sensor_power_off = cam_ldo_power_off,
	.sensor_isp_reset = cam_isp_reset,
	.sensor_get_fw = cam_get_fw,
	.sensor_set_isp_core = cam_set_isp_core,
	.sensor_is_vdd_core_set = cam_is_vdd_core_set,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5c73m3_data = {
	.sensor_name	= "s5c73m3",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_s5c73m3,
	.sensor_platform_info = &sensor_board_info_s5c73m3,
	.gpio_conf = &rear_gpio_conf,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
};

struct platform_device msm8960_camera_sensor_s5c73m3 = {
	.name	= "msm_camera_s5c73m3",
	.dev	= {
		.platform_data = &msm_camera_sensor_s5c73m3_data,
	},
};
#endif

#ifdef CONFIG_S5K6A3YX
static struct msm_camera_sensor_flash_data flash_s5k6a3yx = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k6a3yx = {
	.mount_angle	= 270,
#if defined(CONFIG_MACH_STRETTO)
	.mclk	= GPIO_CAM_MCLK2,
	.sensor_pwd	= GPIO_CAM_CORE_EN,
	.gpio_conf = &msm_8960_front_cam_gpio_conf,
#else
	.mclk	= GPIO_CAM_MCLK0,
	.sensor_pwd	= CAM_CORE_EN,
#endif
	.vcm_pwd	= 0,
	.vcm_enable	= 1,
	.sensor_power_on = cam_ldo_power_on,
	.sensor_power_off = cam_ldo_power_off,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k6a3yx_data = {
	.sensor_name	= "s5k6a3yx",
	.pdata	= &msm_camera_csi_device_data[1],
	.flash_data	= &flash_s5k6a3yx,
	.sensor_platform_info = &sensor_board_info_s5k6a3yx,
#if !defined(CONFIG_MACH_STRETTO)
	.gpio_conf = &front_gpio_conf,
#endif
	.csi_if	= 1,
	.camera_type = FRONT_CAMERA_2D,
};

struct platform_device msm8960_camera_sensor_s5k6a3yx = {
	.name	= "msm_camera_s5k6a3yx",
	.dev	= {
		.platform_data = &msm_camera_sensor_s5k6a3yx_data,
	},
};
#endif

#ifdef CONFIG_ISX012
static struct msm_camera_sensor_flash_data flash_isx012 = {
#if defined(CONFIG_MACH_KONA)
	.flash_type	= MSM_CAMERA_FLASH_NONE,
#else
	.flash_type = MSM_CAMERA_FLASH_LED,
#endif
};

static struct msm_camera_sensor_platform_info sensor_board_info_isx012 = {
    .mount_angle	= 90,
	.sensor_reset	= GPIO_CAM1_RST_N,
#ifdef CONFIG_MACH_AEGIS2
	.sensor_stby	= PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_CAM_STBY),
#else
	.sensor_stby	= GPIO_MAIN_CAM_STBY,
#endif
#if !defined(CONFIG_MACH_STRETTO)
	.vt_sensor_stby	= GPIO_VT_STBY,
	.vt_sensor_reset	= GPIO_CAM2_RST_N,
	.flash_en	= GPIO_MSM_FLASH_CNTL_EN,
	.flash_set	= GPIO_MSM_FLASH_NOW,
#endif
	.mclk	= GPIO_CAM_MCLK,
	.sensor_pwd	= GPIO_CAM_CORE_EN,
	.vcm_pwd	= 0,
	.vcm_enable	= 1,
	.sensor_power_on = cam_ldo_power_on,
	.sensor_power_off = cam_ldo_power_off,
	.cam_vreg = msm_8960_back_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_back_cam_vreg),
	.gpio_conf = &msm_8960_back_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_isx012_data = {
	.sensor_name	= "isx012",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_isx012,
	.sensor_platform_info = &sensor_board_info_isx012,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
};
#endif

#ifdef CONFIG_S5K8AAY
static struct msm_camera_sensor_flash_data flash_s5k8aay = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k8aay = {
	.mount_angle	= 270,
	.sensor_reset	= GPIO_CAM1_RST_N,
	.sensor_pwd	= GPIO_CAM_CORE_EN,
	.sensor_stby	= GPIO_MAIN_CAM_STBY,
	.vt_sensor_reset	= GPIO_CAM2_RST_N,
	.vt_sensor_stby	= GPIO_VT_STBY,
	.mclk	= GPIO_CAM_MCLK,
	.vcm_pwd	= 0,
	.vcm_enable	= 1,
	.sensor_power_on = cam_ldo_power_on,
	.sensor_power_off = cam_ldo_power_off,
	.cam_vreg = msm_8960_front_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_front_cam_vreg),
	.gpio_conf = &msm_8960_front_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k8aay_data = {
	.sensor_name	= "s5k8aay",
	.pdata	= &msm_camera_csi_device_data[1],
	.flash_data	= &flash_s5k8aay,
	.sensor_platform_info = &sensor_board_info_s5k8aay,
	.csi_if	= 1,
	.camera_type = FRONT_CAMERA_2D,
};
#endif

#ifdef CONFIG_S5K4ECGX
static struct msm_camera_sensor_flash_data flash_s5k4ecgx = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k4ecgx = {
	.mount_angle	= 90,
	.sensor_reset	= GPIO_CAM1_RST_N,
	.sensor_stby	= GPIO_MAIN_CAM_STBY,
	.vt_sensor_reset	= GPIO_CAM2_RST_N,
	.vt_sensor_stby	= GPIO_VT_STBY,
	.flash_en	= GPIO_MSM_FLASH_CNTL_EN,
	.flash_set	= GPIO_MSM_FLASH_NOW,
	.mclk	= GPIO_CAM_MCLK,
	.sensor_pwd	= GPIO_CAM_CORE_EN,
	.vcm_pwd	= 0,
	.vcm_enable	= 1,
	.sensor_power_on = cam_ldo_power_on,
	.sensor_power_off = cam_ldo_power_off,
	.cam_vreg = msm_8960_back_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_back_cam_vreg),
	.gpio_conf = &msm_8960_back_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k4ecgx_data = {
	.sensor_name	= "s5k4ecgx",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_s5k4ecgx,
	.sensor_platform_info = &sensor_board_info_s5k4ecgx,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
};
#endif
#ifdef CONFIG_S5K5CCGX
static struct msm_camera_sensor_flash_data flash_s5k5ccgx = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k5ccgx = {
	.sensor_reset	= GPIO_CAM1_RST_N,
#if defined(CONFIG_MACH_JASPER) 
	.mount_angle	= 90,
	.sensor_stby	= PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_CAM_STBY),
#elif defined(CONFIG_MACH_ESPRESSO_VZW)
	.mount_angle	= 90,
	.sensor_stby	= GPIO_MAIN_CAM_STBY,
#else
	.mount_angle	= 0,
	.sensor_stby	= GPIO_MAIN_CAM_STBY,
#endif
	.vt_sensor_reset	= GPIO_CAM2_RST_N,
	.vt_sensor_stby	= GPIO_VT_STBY,
	.flash_en	= GPIO_MSM_FLASH_CNTL_EN,
	.flash_set	= GPIO_MSM_FLASH_NOW,
	.mclk	= GPIO_CAM_MCLK,
	.sensor_pwd	= GPIO_CAM_CORE_EN,
	.vcm_pwd	= 0,
	.vcm_enable	= 1,
	.sensor_power_on = cam_ldo_power_on,
	.sensor_power_off = cam_ldo_power_off,
	.cam_vreg = msm_8960_back_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_back_cam_vreg),
	.gpio_conf = &msm_8960_back_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k5ccgx_data = {
	.sensor_name	= "s5k5ccgx",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_s5k5ccgx,
	.sensor_platform_info = &sensor_board_info_s5k5ccgx,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
};
#endif
#ifdef CONFIG_SR030PC50
static struct msm_camera_sensor_flash_data flash_sr030pc50 = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_sr030pc50 = {
#if defined(CONFIG_MACH_ESPRESSO_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_SPR) \
				|| defined(CONFIG_MACH_ESPRESSO10_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR)
	.mount_angle	= 0,
#else
	.mount_angle	= 270,
#endif
	.sensor_reset	= GPIO_CAM1_RST_N,
	.sensor_pwd	= GPIO_CAM_CORE_EN,
	.sensor_stby	= GPIO_MAIN_CAM_STBY,
	.vt_sensor_reset	= GPIO_CAM2_RST_N,
	.vt_sensor_stby	= GPIO_VT_STBY,
	.mclk	= GPIO_CAM_MCLK,
	.vcm_pwd	= 0,
	.vcm_enable	= 1,
	.sensor_power_on = cam_ldo_power_on,
	.sensor_power_off = cam_ldo_power_off,
	.cam_vreg = msm_8960_front_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_front_cam_vreg),
	.gpio_conf = &msm_8960_front_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_sr030pc50_data = {
	.sensor_name	= "sr030pc50",
	.pdata	= &msm_camera_csi_device_data[1],
	.flash_data	= &flash_sr030pc50,
	.sensor_platform_info = &sensor_board_info_sr030pc50,
	.csi_if	= 1,
	.camera_type = FRONT_CAMERA_2D,
};
#endif
#ifdef CONFIG_DB8131M

static struct msm_camera_sensor_flash_data flash_db8131m = {
	.flash_type     = MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_db8131m = {
	.mount_angle    = 270,
	.sensor_reset   = GPIO_CAM1_RST_N,
	.sensor_pwd     = GPIO_CAM_CORE_EN,
#if defined(CONFIG_MACH_AEGIS2) || defined(CONFIG_MACH_JASPER)
	.sensor_stby	= PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_CAM_STBY),
#else
	.sensor_stby    = GPIO_MAIN_CAM_STBY,
#endif
	.vt_sensor_stby	= GPIO_VT_STBY,
	.vt_sensor_reset        = GPIO_CAM2_RST_N,
	.mclk   = GPIO_CAM_MCLK,
	.vcm_pwd        = 0,
	.vcm_enable     = 1,
	.sensor_power_on =  cam_ldo_power_on,
	.sensor_power_off = cam_ldo_power_off,
	.cam_vreg = msm_8960_front_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_front_cam_vreg),
	.gpio_conf = &msm_8960_front_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_db8131m_data = {
	.sensor_name    = "db8131m",
	.pdata  = &msm_camera_csi_device_data[1],
	.flash_data     = &flash_db8131m,
	.sensor_platform_info = &sensor_board_info_db8131m,
	.csi_if = 1,
	.camera_type = FRONT_CAMERA_2D,
};
#endif
#ifdef CONFIG_LEDS_AAT1290A
static int aat1290a_setGpio(void)
{
	int ret;
	int temp = 0;

	printk(KERN_DEBUG "[%s : %d]!!\n", __func__, __LINE__);

#if defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX) /* D2 */
	/* FLASH_LED_UNLOCK*/
	gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
		(PMIC_MPP_FLASH_LED_UNLOCK), 1);
#endif

	/* GPIO_CAM_FLASH_SW : tourch */
	gpio_set_value_cansleep(gpio_cam_flash_sw, 0);
	temp = gpio_get_value(gpio_cam_flash_sw);
	printk(KERN_DEBUG "[s5c73m3] check GPIO_CAM_FLASH_SW : %d\n", temp);
	usleep(1*1000);

	/* flash power 1.8V */
	l28 = regulator_get(NULL, "8921_lvs4");
	ret = regulator_enable(l28);
	if (ret)
		printk("error enabling regulator\n");
	usleep(1*1000);

	if (pmic_gpio_msm_flash_cntl_en) {
		gpio_set_value_cansleep(pmic_gpio_msm_flash_cntl_en, 1);
	} else {
		gpio_set_value_cansleep(GPIO_MSM_FLASH_CNTL_EN, 1);
		temp = gpio_get_value(GPIO_MSM_FLASH_CNTL_EN);
		printk(KERN_DEBUG "[s5c73m3] check Flash set GPIO : %d\n",
			temp);
	}
	isFlashCntlEn = true;
	usleep(1*1000);

	gpio_set_value_cansleep(GPIO_MSM_FLASH_NOW, 1);
	temp = gpio_get_value(GPIO_MSM_FLASH_NOW);
	printk(KERN_DEBUG "[s5c73m3] check Flash enable GPIO : %d\n", temp);
	usleep(1*1000);

	return 0;
}

static int aat1290a_freeGpio(void)
{
	int ret;

	printk(KERN_DEBUG "[%s : %d]!!\n", __func__, __LINE__);

	if (pmic_gpio_msm_flash_cntl_en)
		gpio_set_value_cansleep(pmic_gpio_msm_flash_cntl_en, 0);
	else
		gpio_set_value_cansleep(GPIO_MSM_FLASH_CNTL_EN, 0);
	isFlashCntlEn = false;
	usleep(1*1000);
	gpio_set_value_cansleep(GPIO_MSM_FLASH_NOW, 0);
	usleep(1*1000);

	/* flash power 1.8V */
	if (l28) {
		ret = regulator_disable(l28);
		if (ret)
			printk("error disabling regulator\n");
	}
	usleep(1*1000);

	/* GPIO_CAM_FLASH_SW : tourch */
	gpio_set_value_cansleep(gpio_cam_flash_sw, 0);
	usleep(1*1000);

#if defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX) /* D2 */
	/* FLASH_LED_LOCK*/
	gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
		(PMIC_MPP_FLASH_LED_UNLOCK), 0);
#endif
	return 0;
}

static void aat1290a_torch_en(int onoff)
{
	int temp = 0;

	printk(KERN_DEBUG "[%s : %d] %s!!\n",
	       __func__, __LINE__, onoff ? "enabled" : "disabled");

	gpio_set_value_cansleep(GPIO_MSM_FLASH_NOW, onoff);
	temp = gpio_get_value(GPIO_MSM_FLASH_NOW);
	printk(KERN_DEBUG "[s5c73m3] check Flash enable GPIO : %d\n", temp);
	usleep(1*1000);
}

static void aat1290a_torch_set(int onoff)
{
	int temp = 0;

	printk(KERN_DEBUG "[%s : %d] %s!!\n",
	       __func__, __LINE__, onoff ? "enabled" : "disabled");

	if (pmic_gpio_msm_flash_cntl_en) {
		gpio_set_value_cansleep(pmic_gpio_msm_flash_cntl_en, onoff);
	} else {
		gpio_set_value_cansleep(GPIO_MSM_FLASH_CNTL_EN, onoff);
		temp = gpio_get_value(GPIO_MSM_FLASH_CNTL_EN);
		printk(KERN_DEBUG "[s5c73m3] check Flash set GPIO : %d\n",
			temp);
	}
	usleep(1*1000);
}

static struct aat1290a_led_platform_data aat1290a_led_data = {
	.brightness = TORCH_BRIGHTNESS_50,
	.status	= STATUS_UNAVAILABLE,
	.setGpio = aat1290a_setGpio,
	.freeGpio = aat1290a_freeGpio,
	.torch_en = aat1290a_torch_en,
	.torch_set = aat1290a_torch_set,
};

static struct platform_device s3c_device_aat1290a_led = {
	.name	= "aat1290a-led",
	.id	= -1,
	.dev	= {
		.platform_data	= &aat1290a_led_data,
	},
};
#endif

#if defined(CONFIG_S5C73M3) || defined(CONFIG_S5K6A3YX)
static struct platform_device *cam_dev[] = {
#ifdef CONFIG_S5C73M3
		&msm8960_camera_sensor_s5c73m3,
#endif
#ifdef CONFIG_S5K6A3YX
		&msm8960_camera_sensor_s5k6a3yx,
#endif
};
#endif

static ssize_t back_camera_type_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_ISX012)
	char cam_type[] = "SONY_ISX012\n";
#elif defined(CONFIG_S5C73M3)
	char cam_type[] = "S5C73M3\n";
#elif defined(CONFIG_S5K5CCGX)
	char cam_type[] = "SLSI_S5K5CCGX\n";
#else
	char cam_type[] = "Rear default camera\n";
#endif

	return snprintf(buf, sizeof(cam_type), "%s", cam_type);
}

static ssize_t front_camera_type_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_S5K8AAY)
	char cam_type[] = "SLSI_S5K8AA\n";
#elif defined(CONFIG_S5K6A3YX)
	char cam_type[] = "SLSI_S5K6A3YX\n";
#elif defined(CONFIG_DB8131M)
#if defined(CONFIG_MACH_INFINITE)
	char cam_type[] = "DB8131A\n";
#else
	char cam_type[] = "DUB_DB8131M\n";
#endif
#elif defined(CONFIG_SR030PC50)
	char cam_type[] = "SILICON_SR030PC50\n";
#else
	char cam_type[] = "Front default camera\n";
#endif

	return snprintf(buf, sizeof(cam_type), "%s", cam_type);
}

static DEVICE_ATTR(rear_camtype, S_IRUGO, back_camera_type_show, NULL);
static DEVICE_ATTR(front_camtype, S_IRUGO, front_camera_type_show, NULL);

static ssize_t back_camera_firmware_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_ISX012)
	char cam_fw[] = "ISX012\n";
#elif defined(CONFIG_S5C73M3)
	char cam_fw[] = "SlimISP_XXX\n";
#elif defined(CONFIG_S5K5CCGX)
	char cam_fw[] = "S5K5CCGX\n";
#else
	char cam_fw[] = "Rear default camera\n";
#endif

#if defined(CONFIG_S5C73M3)
	return sprintf(buf, "%s %s cam_fw = %s", rear_sensor_fw, rear_phone_fw, cam_fw);
#else
	return snprintf(buf, sizeof(cam_fw), "%s", cam_fw);
#endif
}

static ssize_t front_camera_firmware_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_S5K8AAY)
	char cam_fw[] = "S5K8AAY\n";
#elif defined(CONFIG_S5K6A3YX)
	char *cam_fw[] = {"S5K6A3", "S5K6A3"}; /*char cam_fw[] = "S5K6A3YX\n";*/
#elif defined(CONFIG_DB8131M)
#if defined(CONFIG_MACH_INFINITE)
	char cam_fw[] = "DB8131A\n";
#else
	char cam_fw[] = "DB8131M\n";
#endif
#elif defined(CONFIG_SR030PC50)
	char cam_fw[] = "SR030PC50\n";
#else
	char cam_fw[] = "Front default camera\n";
#endif

#if defined(CONFIG_S5K6A3YX)
	return sprintf(buf, "%s %s", cam_fw[0], cam_fw[1]);
#else
	return snprintf(buf, sizeof(cam_fw), "%s", cam_fw);
#endif
}

static DEVICE_ATTR(rear_camfw, 0664, back_camera_firmware_show, NULL);
static DEVICE_ATTR(front_camfw, 0664, front_camera_firmware_show, NULL);
#if defined(CONFIG_ISX012) || defined(CONFIG_S5K8AAY)\
			|| defined(CONFIG_S5K5CCGX)\
			|| defined(CONFIG_SR030PC50)\
			|| defined(CONFIG_DB8131M)
u8 torchonoff;
static u8 gpio_flash_en;
static u8 gpio_flash_set;
#if !(defined(CONFIG_SR030PC50)&&defined(CONFIG_S5K5CCGX))
static u8 pmic_gpio_msm_flash_cntl_en;
#if defined(CONFIG_S5C73M3) || defined(CONFIG_S5K6A3YX)\
	|| defined(CONFIG_MACH_APEXQ) || defined(CONFIG_MACH_COMANCHE) \
	|| defined(CONFIG_MACH_EXPRESS) || defined(CONFIG_MACH_GOGH) \
	|| defined(CONFIG_MACH_INFINITE)
static bool isFlashCntlEn;
#endif
#endif
#endif

static int get_flash_led_unlock_rev(void)
{
#if defined(CONFIG_MACH_GOGH)
	return ((system_rev >= BOARD_REV03) ? 1 : 0);
#elif defined(CONFIG_MACH_INFINITE)
	return ((system_rev >= BOARD_REV03) ? 1 : 0);
#elif defined(CONFIG_MACH_APEXQ)
	return ((system_rev >= BOARD_REV03) ? 1 : 0);
#elif defined(CONFIG_MACH_COMANCHE)
	return ((system_rev >= BOARD_REV03) ? 1 : 0);
#elif defined(CONFIG_MACH_EXPRESS)
	return ((system_rev >= BOARD_REV02) ? 1 : 0);
#elif defined(CONFIG_MACH_AEGIS2)
	return ((system_rev >= BOARD_REV07) ? 1 : 0);
#else
	return 0;
#endif
}

static ssize_t cameraflash_file_cmd_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	int value;
	int err = 1;
	int flash_rev = 0;
#if !defined(CONFIG_S5C73M3) && !defined(CONFIG_S5K6A3YX) /* D2 */
	int i;
#endif

	flash_rev = get_flash_led_unlock_rev();

	if (strlen(buf) > 2)
		return -err;

	if (isdigit(*buf)) {
		err = kstrtoint(buf, 10, &value);
		if (err < 0)
			pr_err("%s, kstrtoint failed.", __func__);
	} else
		return -err;

#if defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX) /* D2 */
#ifdef CONFIG_LEDS_AAT1290A
	err = aat1290a_flash_power(value);
	if (err < 0)
		printk(KERN_DEBUG "[%s : %d]aat1290a_flash_power"
			" contorl fail!!\n", __func__, __LINE__);
#endif
#elif defined(CONFIG_ISX012) || defined(CONFIG_S5K8AAY)\
			|| defined(CONFIG_S5K5CCGX)\
			|| defined(CONFIG_SR030PC50)\
			|| defined(CONFIG_DB8131M)

#if defined(CONFIG_MACH_AEGIS2)
	/* Flash switch On/Off - CAM_SENSOR_IO_1P8 */
	if (system_rev < BOARD_REV07)
		gpio_set_value_cansleep(GPIO_CAM_SENSOR_IO_EN, value ? 1 : 0);
#endif

#if defined(CONFIG_MACH_GOGH) || defined(CONFIG_MACH_APEXQ)\
	|| defined(CONFIG_MACH_EXPRESS) || defined(CONFIG_MACH_COMANCHE)\
	|| defined(CONFIG_MACH_AEGIS2) || defined(CONFIG_MACH_INFINITE)
	if (flash_rev && !isSensorPowered) {
		gpio_set_value_cansleep(PM8921_MPP_PM_TO_SYS
			(PMIC_MPP_FLASH_LED_UNLOCK), value ? 1 : 0);
	}
#endif

#if defined(CONFIG_MACH_EXPRESS)
		if (system_rev >= BOARD_REV05) {
			if (value == 0) {
				pr_err("[Torch flash]OFF\n");

				gpio_set_value_cansleep(gpio_flash_en, 0);

				if ((system_rev == BOARD_REV05)\
					|| (system_rev == BOARD_REV06))
					gpio_set_value_cansleep(
						gpio_flash_set, 0);

				torchonoff = 0;

			} else {
				pr_err("[Torch flash]ON\n");
				for (i = 1; i > 0; i--) {
					gpio_set_value_cansleep(
						gpio_flash_en, 0);
					udelay(0);
					gpio_set_value_cansleep(
						gpio_flash_en, 1);
					udelay(1);
				}
				torchonoff = 1;
			}
		} else {
			if (value == 0) {
				pr_err("[Torch flash]OFF\n");
				gpio_set_value_cansleep(gpio_flash_en, 0);
				gpio_set_value_cansleep(gpio_flash_set, 0);
				torchonoff = 0;
			} else {
				pr_err("[Torch flash]ON\n");
				gpio_set_value_cansleep(
					gpio_flash_en, 0);
				for (i = 5; i > 1; i--) {
					gpio_set_value_cansleep(
						gpio_flash_set, 1);
					udelay(1);
					gpio_set_value_cansleep(
						gpio_flash_set, 0);
					udelay(1);
				}
				gpio_set_value_cansleep(gpio_flash_set, 1);
				usleep(2*1000);
				torchonoff = 1;
			}
		}
#else
	if (value == 0) {
		pr_err("[Torch flash]OFF\n");
		gpio_set_value_cansleep(gpio_flash_en, 0);
		gpio_set_value_cansleep(gpio_flash_set, 0);
		torchonoff = 0;
	} else {
		pr_err("[Torch flash]ON\n");
#if defined(CONFIG_MACH_INFINITE)
		mdelay(5);
#endif
		gpio_set_value_cansleep(gpio_flash_en, 0);

		for (i = 5; i > 1; i--) {
			gpio_set_value_cansleep(
				gpio_flash_set, 1);
			udelay(1);
			gpio_set_value_cansleep(
				gpio_flash_set, 0);
			udelay(1);
		}
		gpio_set_value_cansleep(gpio_flash_set, 1);
		usleep(2*1000);
		torchonoff = 1;
	}
#endif
#endif
	return size;
}

static DEVICE_ATTR(rear_flash, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, cameraflash_file_cmd_store);

void msm8960_cam_create_node(void)
{
	struct device *cam_dev_back;
	struct device *cam_dev_front;
	struct class *camera_class;

	camera_class = class_create(THIS_MODULE, "camera");

	if (IS_ERR(camera_class)) {
		pr_err("Failed to create class(camera)!\n");
		return;
	}
	cam_dev_back = device_create(camera_class, NULL,
		0, NULL, "rear");

	if (IS_ERR(cam_dev_back)) {
		pr_err("Failed to create cam_dev_back device!\n");
		goto OUT7;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear_flash) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
			dev_attr_rear_flash.attr.name);
		goto OUT6;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear_camtype) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
			dev_attr_rear_camtype.attr.name);
		goto OUT5;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear_camfw) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
			dev_attr_rear_camfw.attr.name);
		goto OUT4;
	}

	cam_dev_front = device_create(camera_class, NULL,
		1, NULL, "front");

	if (IS_ERR(cam_dev_front)) {
		pr_err("Failed to create cam_dev_front device!");
		goto OUT3;
	}

	if (device_create_file(cam_dev_front, &dev_attr_front_camtype) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
			dev_attr_front_camtype.attr.name);
		goto OUT2;
	}

	if (device_create_file(cam_dev_front, &dev_attr_front_camfw) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
			dev_attr_front_camfw.attr.name);
		goto OUT1;
	}

	return;

OUT1:
	device_remove_file(cam_dev_back, &dev_attr_front_camtype);
OUT2:
	device_destroy(camera_class, 1);
OUT3:
	device_remove_file(cam_dev_back, &dev_attr_rear_camfw);
OUT4:
	device_remove_file(cam_dev_back, &dev_attr_rear_camtype);
OUT5:
	device_remove_file(cam_dev_back, &dev_attr_rear_flash);
OUT6:
	device_destroy(camera_class, 0);
OUT7:
	return;
}
#ifdef CONFIG_S5C73M3
static struct spi_board_info s5c73m3_spi_info[] __initdata = {
	{
		.modalias		= "s5c73m3_spi",
		.mode			= SPI_MODE_0,
		.bus_num		= 0,
		.chip_select	= 0,
		.max_speed_hz	= 48000000,
	}
};
#endif

#if 0
static struct pm8xxx_mpp_config_data privacy_light_on_config = {
	.type		= PM8XXX_MPP_TYPE_SINK,
	.level		= PM8XXX_MPP_CS_OUT_5MA,
	.control	= PM8XXX_MPP_CS_CTRL_MPP_LOW_EN,
};

static struct pm8xxx_mpp_config_data privacy_light_off_config = {
	.type		= PM8XXX_MPP_TYPE_SINK,
	.level		= PM8XXX_MPP_CS_OUT_5MA,
	.control	= PM8XXX_MPP_CS_CTRL_DISABLE,
};

static int32_t msm_camera_8960_ext_power_ctrl(int enable)
{
	int rc = 0;
	if (enable) {
		rc = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(12),
			&privacy_light_on_config);
	} else {
		rc = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(12),
			&privacy_light_off_config);
	}
	return rc;
}
#endif

static int get_mclk_rev(void)
{
#if defined(CONFIG_MACH_M2_ATT)
	return ((system_rev >= BOARD_REV10) ? 1 : 0);
#elif defined(CONFIG_MACH_M2_VZW)
	return ((system_rev >= BOARD_REV13) ? 1 : 0);
#elif defined(CONFIG_MACH_M2_SPR)
	return ((system_rev >= BOARD_REV08) ? 1 : 0);
#elif defined(CONFIG_MACH_M2_SKT)
	return ((system_rev >= BOARD_REV09) ? 1 : 0);
#elif defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
	return ((system_rev >= BOARD_REV03) ? 1 : 0);
#elif defined(CONFIG_MACH_APEXQ)
	return ((system_rev >= BOARD_REV04) ? 1 : 0);
#elif defined(CONFIG_MACH_COMANCHE)
	return ((system_rev >= BOARD_REV03) ? 1 : 0);
#elif defined(CONFIG_MACH_EXPRESS)
	return ((system_rev >= BOARD_REV03) ? 1 : 0);
#elif defined(CONFIG_MACH_AEGIS2)
	return ((system_rev >= BOARD_REV07) ? 1 : 0);
#elif defined(CONFIG_MACH_JASPER)
	return ((system_rev >= BOARD_REV08) ? 1 : 0);
#elif defined(CONFIG_MACH_INFINITE)
	return ((system_rev >= BOARD_REV04) ? 1 : 0);
#elif defined(CONFIG_MACH_STRETTO)
	return 1;
#elif defined(CONFIG_MACH_SUPERIORLTE_SKT)
	return 1;
#else
	return 0;
#endif
}

void __init msm8960_init_cam(void)
{
	int rev = 0;
	struct msm_camera_sensor_info *s_info;

	rev = get_mclk_rev();

#if !defined(CONFIG_MACH_STRETTO)
#if defined(CONFIG_S5C73M3) || defined(CONFIG_S5K6A3YX)\
	|| defined(CONFIG_MACH_APEXQ) || defined(CONFIG_MACH_COMANCHE) \
	|| defined(CONFIG_MACH_EXPRESS) || defined(CONFIG_MACH_GOGH) \
	|| defined(CONFIG_MACH_INFINITE)
	/*|| ((defined(CONFIG_ISX012) || defined(CONFIG_DB8131M))\temp */

	if (rev) {
		int rc;

		struct pm_gpio param_flash = {
			.direction      = PM_GPIO_DIR_OUT,
			.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
			.output_value   = 0,
			.pull	   = PM_GPIO_PULL_NO,
			.vin_sel	= PM_GPIO_VIN_S4,
			.out_strength   = PM_GPIO_STRENGTH_MED,
			.function       = PM_GPIO_FUNC_NORMAL,
		};

		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS
			(PMIC_MSM_FLASH_CNTL_EN), &param_flash);

		if (rc) {
			pr_err("%s pmic gpio config failed	rc= %d \n", __func__, rc);
		}
		pmic_gpio_msm_flash_cntl_en =
			PM8921_GPIO_PM_TO_SYS(PMIC_MSM_FLASH_CNTL_EN);
	} else {
		pmic_gpio_msm_flash_cntl_en = 0;
	}
	isFlashCntlEn = false;
#endif
#endif

	msm8960_cam_create_node();

	msm_gpiomux_install(msm8960_cam_common_configs,
			ARRAY_SIZE(msm8960_cam_common_configs));

#if defined(CONFIG_ISX012) || defined(CONFIG_S5K8AAY)\
			|| defined(CONFIG_DB8131M)\
			|| defined(CONFIG_S5K5CCGX)\
			|| defined(CONFIG_SR030PC50)\
			|| defined(CONFIG_DB8131M)\
			|| defined(CONFIG_S5K4ECGX)
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_CORE_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_A_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#if defined(CONFIG_MACH_ESPRESSO_VZW) || defined(CONFIG_MACH_ESPRESSO_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_SPR) \
				|| defined(CONFIG_MACH_ESPRESSO10_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR)
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_VTCORE_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif
#if defined(CONFIG_MACH_JAGUAR)
	if (system_rev >= BOARD_REV10) { /* HW rev0.9 */
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_SENSOR_IO_EN, 0,
			GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
	 } else if (system_rev >= BOARD_REV04) {
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_SENSOR_IO_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
			GPIO_CFG_ENABLE);
	}
#elif defined(CONFIG_MACH_AEGIS2)
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_SENSOR_IO_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
			GPIO_CFG_ENABLE);
#elif defined(CONFIG_MACH_APEXQ)
	if (system_rev >= BOARD_REV02) {
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_SENSOR_IO_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
			GPIO_CFG_ENABLE);
	}
#endif
	/*Main cam reset */
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM1_RST_N, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	/*Main cam stby*/
#if !(defined(CONFIG_MACH_AEGIS2) || defined(CONFIG_MACH_JASPER))
	gpio_tlmm_config(GPIO_CFG(GPIO_MAIN_CAM_STBY, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif
	/*Front cam reset*/
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM2_RST_N, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#if !defined(CONFIG_MACH_STRETTO)
	/*Front cam stby*/
	gpio_tlmm_config(GPIO_CFG(GPIO_VT_STBY, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif
#if !(defined(CONFIG_MACH_ESPRESSO_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_SPR) \
				|| defined(CONFIG_MACH_ESPRESSO10_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR) \
				|| defined(CONFIG_MACH_STRETTO))
	/*Falsh Enable*/
	if (!pmic_gpio_msm_flash_cntl_en) {
		gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_CNTL_EN, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
		GPIO_CFG_ENABLE);
	}
#if defined(CONFIG_MACH_AEGIS2)
	if (system_rev >= BOARD_REV07) {
			/*Flash Set*/
			gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_NOW2, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
			GPIO_CFG_ENABLE);
	} else {
			/*Flash Set*/
			gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_NOW, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
			GPIO_CFG_ENABLE);
	}
#elif defined(CONFIG_MACH_JASPER)
	/*Flash NC*/
	gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_NOW, 0, GPIO_CFG_INPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#else
	/*Flash Set*/
	gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_NOW, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif

#if defined(CONFIG_MACH_INFINITE)
	if (system_rev >= BOARD_REV04) {
		gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_CNTL_EN2, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
		GPIO_CFG_ENABLE);
	}
#endif

#endif

	/*CAM_MCLK0*/
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK, 1, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#elif defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX)
#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
	gpio_tlmm_config(GPIO_CFG(gpio_rev(CAM_CORE_EN), 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#else
	gpio_tlmm_config(GPIO_CFG(CAM_CORE_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif
	gpio_tlmm_config(GPIO_CFG(CAM_MIPI_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_A_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_CAM_SENSOR_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(ISP_RESET, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
	gpio_tlmm_config(GPIO_CFG(gpio_rev(CAM2_RST_N), 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#else
	gpio_tlmm_config(GPIO_CFG(CAM2_RST_N, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif

	gpio_cam_flash_sw = gpio_rev(CAM_FLASH_SW);
	gpio_tlmm_config(GPIO_CFG(gpio_cam_flash_sw, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(gpio_rev(CAM_AF_EN), 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);

	if (!pmic_gpio_msm_flash_cntl_en) {
		gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_CNTL_EN, 0,
			GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	}
	gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_NOW, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_VT_CAM_SEN_DET, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif
#if defined(CONFIG_MACH_STRETTO)
	gpio_tlmm_config(GPIO_CFG(GPIO_VT_CAM_SEN_DET, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#endif

#if defined(CONFIG_ISX012)
	s_info = &msm_camera_sensor_isx012_data;

	if (rev) {
#if defined(CONFIG_MACH_APEXQ) || defined(CONFIG_MACH_COMANCHE)

		s_info->sensor_platform_info->flash_en =
			pmic_gpio_msm_flash_cntl_en;
#elif defined(CONFIG_MACH_AEGIS2)
		s_info->sensor_platform_info->flash_set =
			GPIO_MSM_FLASH_NOW2;
#elif defined(CONFIG_MACH_EXPRESS)
		if (system_rev >= BOARD_REV07) {
			s_info->sensor_platform_info->flash_en =
				GPIO_MSM_FLASH_NOW;
			s_info->sensor_platform_info->flash_set =
				-1;
		} else{
			s_info->sensor_platform_info->flash_en =
				pmic_gpio_msm_flash_cntl_en;
		}
#elif defined(CONFIG_MACH_INFINITE)
		s_info->sensor_platform_info->flash_set =
			GPIO_MSM_FLASH_NOW;
		if (system_rev >= BOARD_REV04) {
			s_info->sensor_platform_info->flash_en =
				GPIO_MSM_FLASH_CNTL_EN2;
		} else {
			s_info->sensor_platform_info->flash_en =
				GPIO_MSM_FLASH_CNTL_EN;
		}
#endif
	}
#if defined(CONFIG_MACH_GOGH)
	else {
		if (system_rev <= BOARD_REV02) {
			s_info->sensor_platform_info->flash_en =
				GPIO_MSM_FLASH_NOW;
			s_info->sensor_platform_info->flash_set =
				GPIO_MSM_FLASH_CNTL_EN;
		}
	}
#endif
	gpio_flash_en = s_info->sensor_platform_info->flash_en;
	gpio_flash_set = s_info->sensor_platform_info->flash_set;
#endif
#if defined(CONFIG_S5K8AAY)
	s_info = &msm_camera_sensor_s5k8aay_data;
#endif
#if defined(CONFIG_S5K5CCGX)
	s_info = &msm_camera_sensor_s5k5ccgx_data;
#endif
#if defined(CONFIG_DB8131M)
	s_info = &msm_camera_sensor_db8131m_data;

#if defined(CONFIG_MACH_APEXQ) || defined(CONFIG_MACH_COMANCHE) \
	|| defined(CONFIG_MACH_EXPRESS) || defined(CONFIG_MACH_AEGIS2) \
	|| defined(CONFIG_MACH_JASPER) || defined(CONFIG_MACH_INFINITE)
	if (rev) {
		s_info->sensor_platform_info->mclk =
			GPIO_CAM_MCLK2;
		s_info->sensor_platform_info->gpio_conf->cam_gpiomux_conf_tbl =
			msm8960_cam_2d_configs_v2;
		s_info->sensor_platform_info->gpio_conf->
			cam_gpiomux_conf_tbl_size =
			ARRAY_SIZE(msm8960_cam_2d_configs_v2);
		s_info->sensor_platform_info->gpio_conf->cam_gpio_common_tbl =
			msm8960_common_cam_gpio_v2;
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_MCLK2, 1, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}
#endif
#endif
#if defined(CONFIG_SR030PC50)
	s_info = &msm_camera_sensor_sr030pc50_data;
#endif
#if defined(CONFIG_S5K4ECGX)
	s_info = &msm_camera_sensor_s5k4ecgx_data;
#endif


#if defined(CONFIG_S5C73M3) && defined(CONFIG_S5K6A3YX)
#if defined(TEMP_REMOVE)
	for (i = 0; i < ARRAY_SIZE(cam_dev); i++)
		s_info = cam_dev[i]->dev.platform_data;

#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
		s_info->sensor_platform_info->sensor_pwd =
			gpio_rev(CAM_CORE_EN);
#endif
#else
#if defined(CONFIG_S5C73M3)
		s_info = &msm_camera_sensor_s5c73m3_data;
#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
		s_info->sensor_platform_info->sensor_pwd =
			gpio_rev(CAM_CORE_EN);
#endif
//		msm_get_cam_resources(s_info);
		platform_device_register(cam_dev[0]);
#endif
#if defined(CONFIG_S5K6A3YX)
		s_info = &msm_camera_sensor_s5k6a3yx_data;
#if defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_K2_KDI)
		s_info->sensor_platform_info->sensor_pwd =
			gpio_rev(CAM_CORE_EN);
#endif
		 if (rev) {
			s_info->sensor_platform_info->mclk =
				GPIO_CAM_MCLK2;
			s_info->gpio_conf->cam_gpiomux_conf_tbl =
				msm8960_cam_2d_configs_v2;
			s_info->gpio_conf->cam_gpio_tbl =
				msm_cam_gpio_2d_tbl_v2;
	}

//		msm_get_cam_resources(s_info);
		platform_device_register(cam_dev[1]);
#endif
	if (spi_register_board_info(
				    s5c73m3_spi_info,
				    ARRAY_SIZE(s5c73m3_spi_info)) != 0)
		pr_err("%s: spi_register_board_info returned error\n",
			__func__);

#endif
#endif

#if defined(CONFIG_MACH_STRETTO)
#if defined(CONFIG_S5K6A3YX)
	s_info = &msm_camera_sensor_s5k6a3yx_data;
	s_info->sensor_platform_info->mclk =
		GPIO_CAM_MCLK2;
	s_info->sensor_platform_info->gpio_conf->cam_gpiomux_conf_tbl =
		msm8960_cam_2d_configs_v2;
	s_info->sensor_platform_info->gpio_conf->
		cam_gpiomux_conf_tbl_size =
		ARRAY_SIZE(msm8960_cam_2d_configs_v2);
	s_info->sensor_platform_info->gpio_conf->cam_gpio_common_tbl =
		msm8960_common_cam_gpio_v2;
#endif
#endif

	pr_err("[%s:%d]setting done!!\n", __func__, __LINE__);

	platform_device_register(&msm8960_device_csiphy0);
	platform_device_register(&msm8960_device_csiphy1);
	platform_device_register(&msm8960_device_csid0);
	platform_device_register(&msm8960_device_csid1);
	platform_device_register(&msm8960_device_ispif);
	platform_device_register(&msm8960_device_vfe);
	platform_device_register(&msm8960_device_vpe);
#ifdef CONFIG_LEDS_AAT1290A
	platform_device_register(&s3c_device_aat1290a_led);
#endif
}

#ifdef CONFIG_I2C
static struct i2c_board_info msm8960_camera_i2c_boardinfo[] = {
#ifdef CONFIG_S5K5CCGX
	{
#if defined(CONFIG_MACH_ESPRESSO_VZW) || defined(CONFIG_MACH_ESPRESSO_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO10_SPR) \
				|| defined(CONFIG_MACH_ESPRESSO10_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR)
		I2C_BOARD_INFO("s5k5ccgx", 0x5A>>1),
#else
		I2C_BOARD_INFO("s5k5ccgx", 0x78>>1),
#endif
		.platform_data = &msm_camera_sensor_s5k5ccgx_data,
	},
#endif
#ifdef CONFIG_DB8131M
	{
		I2C_BOARD_INFO("db8131m", 0x45),
		.platform_data = &msm_camera_sensor_db8131m_data,
	},
#endif
#ifdef CONFIG_ISX012
	{
		I2C_BOARD_INFO("isx012", 0x3D),
		.platform_data = &msm_camera_sensor_isx012_data,
	},
#endif
#ifdef CONFIG_S5K8AAY
	{
		I2C_BOARD_INFO("s5k8aay", 0x5A>>1),
		.platform_data = &msm_camera_sensor_s5k8aay_data,
	},
#endif
#ifdef CONFIG_SR030PC50
	{
		I2C_BOARD_INFO("sr030pc50", 0x30),
		.platform_data = &msm_camera_sensor_sr030pc50_data,
	},
#endif
#ifdef CONFIG_S5K4ECGX
	{
		I2C_BOARD_INFO("s5k4ecgx", 0xAC>>1),
		.platform_data = &msm_camera_sensor_s5k4ecgx_data,
	},
#endif
#ifdef CONFIG_S5K6A3YX
	{
		I2C_BOARD_INFO("s5k6a3yx", 0x20),
		.platform_data = &msm_camera_sensor_s5k6a3yx_data,
	},
#endif
};

struct msm_camera_board_info msm8960_camera_board_info = {
	.board_info = msm8960_camera_i2c_boardinfo,
	.num_i2c_board_info = ARRAY_SIZE(msm8960_camera_i2c_boardinfo),
};
#endif
#endif
