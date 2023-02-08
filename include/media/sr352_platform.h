/*
 * Driver for SR352 (3MP camera) of Siliconfile
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <media/sec_camera_platform.h>
#include <linux/clk.h>

#define SR352_DEVICE_NAME	"SR352"

#define SR352_I2C_ADDR	(0x40 >> 1)
#define SR352_STREAMOFF_DELAY	DEFAULT_STREAMOFF_DELAY
#define SR352_STREAMOFF_NIGHT_DELAY	500

#define DEFAULT_SENSOR_WIDTH		640  // 1024
#define DEFAULT_SENSOR_HEIGHT		480  // 768
#define DEFAULT_SENSOR_CODE		(V4L2_MBUS_FMT_YUYV8_2X8)


// Need to delete, Degas is not supporting flash
enum {
	SR352_FLASH_MODE_NORMAL,
	SR352_FLASH_MODE_MOVIE,
	SR352_FLASH_MODE_MAX,
};

enum {
	SR352_FLASH_OFF = 0,
	SR352_FLASH_ON = 1,
};
/////////////////////////////////////////////////

struct sr352_platform_data {
	struct sec_camera_platform_data common;
	int (*is_flash_on)(void);

	int	gpio_rst;
	bool	enable_rst;

};

