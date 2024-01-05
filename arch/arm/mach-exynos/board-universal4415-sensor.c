#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-gpio.h>
#include <mach/gpio.h>

#include "board-universal4415.h"

#if defined(CONFIG_SENSORS_CM36686)
#include <linux/sensor/cm36686.h>
#endif
#if defined(CONFIG_SENSORS_ALPS)
#include <linux/sensor/alps_magnetic.h>
#endif
#if defined(CONFIG_INPUT_MPU6500)
#include <linux/sensor/mpu6500_platformdata.h>
#elif defined(CONFIG_INV_SENSORS_IIO)
#include <linux/sensor/inv_sensors.h>
#endif
#include <asm/system_info.h>

#if defined(CONFIG_INPUT_MPU6500)
static void mpu6500_get_position(int *pos)
{
	*pos = MPU6500_TOP_LEFT_LOWER;
}

static struct mpu6500_platform_data mpu6500_pdata = {
	.get_pos = mpu6500_get_position,
};
#elif defined(CONFIG_INV_SENSORS_IIO)
static struct invsens_platform_data_t inv_sensors_pdata = {
	.int_config = 0x10,
	.orientation = { 0, -1, 0,
			1, 0, 0,
			0, 0, 1 },
	.compass = {
		.aux_id = INVSENS_AID_NONE, /* disable compass */
	},
};
#endif

#if defined(CONFIG_SENSORS_ALPS)
static struct alps_platform_data alps_mag_pdata = {
	.orient = { -1, 0, 0,
		0, -1, 0,
		0, 0, 1 },
};
#if defined(CONFIG_MACH_MEGA2)
#if defined(CONFIG_MACH_MEGA2LTE_USA_ATT)
static struct alps_platform_data alps_mag_pdata_att = {
	.orient = { -1, 0, 0,
		0, -1, 0,
		0, 0, 1 },
};
#else
static struct alps_platform_data alps_mag_pdata_rev02 = {
	.orient = { 1, 0, 0,
		0, 1, 0,
		0, 0, 1 },
};
#endif
#endif

static struct platform_device alps_pdata = {
	.name = "alps-input",
	.id = -1,
};
#endif

#if defined(CONFIG_SENSORS_CM36686)
static void light_sensor_power(bool on)
{
	pr_info("%s : %s\n", __func__, (on) ? "on" : "off");
}

static void proxi_power(bool on)
{
	static struct regulator *proxy_reg;
	static bool init_state = 1;
	static bool proxi_status;

	pr_info("%s : %s\n", __func__, (on) ? "on" : "off");

	if (on == proxi_status)
		return;

	if (init_state) {
		if (!proxy_reg) {
			proxy_reg = regulator_get(NULL, "led_a_3.0v");
			if (IS_ERR(proxy_reg)) {
				pr_err("%s v_proxy_reg regulator get error!\n",
					__func__);
				proxy_reg = NULL;
				return;
			}
		}
		regulator_set_voltage(proxy_reg, 3000000, 3000000);
		init_state = 0;
	}

	if (on) {
		regulator_enable(proxy_reg);
		mdelay(2);
		proxi_status = 1;
	} else {
		regulator_disable(proxy_reg);
		proxi_status = 0;
	}
}

static struct cm36686_platform_data cm36686_pdata = {
	.cm36686_power	= light_sensor_power,
	.cm36686_led_on	= proxi_power,
	.irq = GPIO_PROXI_INT,
	.default_hi_thd = 14,
	.default_low_thd = 8,
	.cancel_hi_thd = 10,
	.cancel_low_thd = 5,
};

static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("cm36686", 0x60),
		.platform_data = &cm36686_pdata,
	},
};
#endif

static struct i2c_board_info i2c_devs6[] __initdata = {
#if defined(CONFIG_INPUT_MPU6500)
	{
		I2C_BOARD_INFO("mpu6500_input", 0x68),
		.irq = IRQ_EINT(0),
		.platform_data = &mpu6500_pdata,
	},
#elif defined(CONFIG_INV_SENSORS_IIO)
	{
		I2C_BOARD_INFO("MPU6515", 0x68),
		.irq = IRQ_EINT(0),
		.platform_data = &inv_sensors_pdata,
	},
#endif
#if defined(CONFIG_SENSORS_ALPS)
	{
		I2C_BOARD_INFO("hscd_i2c", 0x0C),
		.platform_data = &alps_mag_pdata,
	},
#endif
};

static int initialize_sensor_gpio(void)
{
	int ret = 0;

	pr_info("%s, is called\n", __func__);

#if defined(CONFIG_INPUT_MPU6500)
	ret = gpio_request(GPIO_GYRO_INT_N, "MPU6500_INT");
	if (ret)
		pr_err("%s, failed to request MPU6500_INT (%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(GPIO_GYRO_INT_N, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_GYRO_INT_N, S3C_GPIO_PULL_DOWN);
#endif

#if defined(CONFIG_SENSORS_CM36686)
	ret = gpio_request(GPIO_PROXI_INT, "CM36686_INT");
	if (ret)
		pr_err("%s, Failed to request gpio gpio_proximity_out. (%d)\n",
			__func__, ret);

	s3c_gpio_setpull(GPIO_PROXI_INT, S3C_GPIO_PULL_UP);
	gpio_free(GPIO_PROXI_INT);
#endif

	return ret;
}

static struct platform_device *sensor_i2c_devices[] __initdata = {
#if defined(CONFIG_SENSORS_CM36686)
	&s3c_device_i2c5,
#endif
	&s3c_device_i2c6,
	&alps_pdata,
};

void __init exynos4_universal4415_sensor_init(void)
{
	int ret = 0;

	pr_info("%s, is called\n", __func__);

	ret = initialize_sensor_gpio();
	if (ret)
		pr_err("%s, initialize_sensor_gpio (err=%d)\n", __func__, ret);

#if defined(CONFIG_SENSORS_CM36686)
	s3c_i2c5_set_platdata(NULL);
	ret = i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
	if (ret < 0)
		pr_err("%s, i2c5 adding i2c fail(err=%d)\n", __func__, ret);
#endif
	s3c_i2c6_set_platdata(NULL);
#if defined(CONFIG_MACH_MEGA2)
#if defined(CONFIG_MACH_MEGA2LTE_USA_ATT)
	i2c_devs6[1].platform_data = &alps_mag_pdata_att;
#else
	if (system_rev >= 2) {
		i2c_devs6[1].platform_data = &alps_mag_pdata_rev02;
		pr_info("%s, system_rev=%d\n", __func__, system_rev);
	}
#endif
#endif
	ret = i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));
	if (ret < 0)
		pr_err("%s, i2c6 adding i2c fail(err=%d)\n",
			__func__, ret);

	platform_add_devices(sensor_i2c_devices,
		ARRAY_SIZE(sensor_i2c_devices));
}

