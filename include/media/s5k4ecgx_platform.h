/*
 * Driver for S5K4ECGX (5MP camera) from LSI
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <media/sec_camera_platform.h>
#include <linux/clk.h>

#define S5K4ECGX_DEVICE_NAME	"S5K4ECGX"

#if defined(CONFIG_MACH_GARDA) || defined(CONFIG_MACH_WICHITA)
#define S5K4ECGX_I2C_ADDR	(0x5A >> 1)
#define S5K4ECGX_STREAMOFF_DELAY	(DEFAULT_STREAMOFF_DELAY + 50)
#define S5K4ECGX_STREAMOFF_NIGHT_DELAY	(500 + 30)
#else
#define S5K4ECGX_I2C_ADDR	(0x78 >> 1)
#define S5K4ECGX_STREAMOFF_DELAY	(DEFAULT_STREAMOFF_DELAY + 20)
#endif

#define DEFAULT_SENSOR_WIDTH		640
#define DEFAULT_SENSOR_HEIGHT		480
#define DEFAULT_SENSOR_CODE		(V4L2_MBUS_FMT_YUYV8_2X8)

enum {
	S5K4ECGX_FLASH_MODE_NORMAL,
	S5K4ECGX_FLASH_MODE_MOVIE,
	S5K4ECGX_FLASH_MODE_MAX,
};

enum {
	S5K4ECGX_FLASH_OFF = 0,
	S5K4ECGX_FLASH_ON = 1,
};

#if defined(CONFIG_MACH_WICHITA)
enum s5k4ecgx_hw_power {
        S5K4ECGX_HW_POWER_NOT_READY,
        S5K4ECGX_HW_POWER_READY,
        S5K4ECGX_HW_POWER_OFF,
        S5K4ECGX_HW_POWER_ON,
};
#endif

struct s5k4ecgx_platform_data {
	struct sec_camera_platform_data common;
	int (*is_flash_on)(void);
#if 1 /* dslim. front bsp */
	int	gpio_rst;
	bool	enable_rst;
#endif
#if defined(CONFIG_MACH_WICHITA)
	int power;
	struct clk *mclk;
        int (*put_power)(struct s5k4ecgx_platform_data *pdata);
        int (*get_power)(struct s5k4ecgx_platform_data *pdata);
        int (*power_on)(struct s5k4ecgx_platform_data *pdata);
        int (*power_off)(struct s5k4ecgx_platform_data *pdata);
#endif
};

