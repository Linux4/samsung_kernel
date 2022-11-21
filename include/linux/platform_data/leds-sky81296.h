
/*
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for sky81296 LED driver chip
 *
 * Author: kyoungho yun <kyoungho.yun@samsung.com>
 *
 */


#define sky81296_NAME "leds-sky81296"

struct sky81296_platform_data {
	u8 flash_time;
	int flash1_pin;
	int flash2_pin;
	int torch_pin;
	struct device_node *np;
};


