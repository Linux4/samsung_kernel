#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include <linux/sensor/sensors_axes_s16.h>
#include <linux/sensor/k2hh.h>
#include <linux/sensor/sx9500_platform_data.h>
#include <linux/mpu.h>
#include <linux/nfc/sec_nfc_com.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include "board-universal222ap.h"

static struct k2hh_platform_data k2hh_pdata = {
        .orientation = {
		-1, 0, 0,
		0, -1, 0,
		0, 0, -1
	},
        .irq_gpio = GPIO_GYRO_INT,
};

static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("k2hh", 0x1d),
		.platform_data = &k2hh_pdata,
	},
	{
		I2C_BOARD_INFO("sx9500-i2c", 0x28),
		/* Grip int - CONDUCTION_DET */
		.irq = EXYNOS4_GPX1(3),
		/* Grip int - RF_TOUCH_INT */
		//.irq = EXYNOS4_GPX3(2),
	},
};

static struct platform_device *sensor_i2c_devices[] __initdata = {
	&s3c_device_i2c7,
};

void __init board_universal_ss222ap_init_sensor(void)
{
	int ret;

	s3c_i2c7_set_platdata(NULL);

	ret = i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));
	if (ret < 0) {
		pr_err("%s, i2c7 adding i2c fail(err=%d)\n",
						__func__, ret);
	}

	platform_add_devices(sensor_i2c_devices,
		ARRAY_SIZE(sensor_i2c_devices));
}
