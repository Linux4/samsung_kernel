#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/gpio-exynos.h>
#include <asm/system_info.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include "board-universal5260.h"

#if defined(CONFIG_SENSORS_AK09911C)
#include <linux/sensor/ak09911c_platformdata.h>
#endif
#if defined(CONFIG_INPUT_MPU6500)
#include <linux/sensor/mpu6500_platformdata.h>
#elif defined(CONFIG_INV_MPU_IIO)
#include <linux/sensor/mpu.h>
#endif
#if defined(CONFIG_SENSORS_TMD27723)
#include <linux/sensor/tmd27723.h>
#endif
#if defined(CONFIG_SENSORS_GP2A30)
#include <linux/sensor/gp2ap030.h>
#endif

#if defined(CONFIG_SENSORS_AK09911C)
static void ak09911c_get_position(int *pos)
{
#if defined(CONFIG_MACH_HL3G) || defined(CONFIG_MACH_HLLTE) || defined(CONFIG_MACH_MEGA2ELTE)
	*pos = AK09911C_TOP_UPPER_RIGHT;
#elif defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	if (system_rev > 0x01)
		*pos = AK09911C_TOP_UPPER_LEFT;
	else
		*pos = AK09911C_BOTTOM_LOWER_RIGHT;
#endif
}

static struct ak09911c_platform_data ak09911c_pdata = {
	.get_pos = ak09911c_get_position,
	.m_rst_n = GPIO_M_SENSOR_RSTN,
};
#endif

#if defined(CONFIG_INPUT_MPU6500)
static void mpu6500_get_position(int *pos)
{
	if (system_rev > 0x01)
		*pos = MPU6500_BOTTOM_LEFT_LOWER;
	else
		*pos = MPU6500_BOTTOM_RIGHT_LOWER;
}

static struct mpu6500_platform_data mpu6500_pdata = {
	.get_pos = mpu6500_get_position,
};
#elif defined(CONFIG_INV_MPU_IIO)
static struct mpu_platform_data mpu_iio = {
    .int_config  = 0x00,
	.level_shifter = 0,
	.orientation = { 1,  0,  0,
	                0,  -1,  0,
	                0,  0,  -1 },
};
#endif

#if defined(CONFIG_SENSORS_TMD27723)
static void proximity_vdd_on(bool on) {
	static bool enabled;

	if (enabled == on)
		return;

	pr_info("%s : %s\n", __func__, (on)?"on":"off");

	if (on)
		gpio_direction_output(GPIO_PROX_LED_EN, 1);
	else
		gpio_direction_output(GPIO_PROX_LED_EN, 0);

	enabled = on;
}

/* Fresco TMD27723S9 New Lux equation 2013.11.20 */
static struct taos_platform_data tmd27723_pdata = {
	.power	= proximity_vdd_on,
	.als_int = GPIO_PROX_SENSOR_INT,
	.prox_thresh_hi = 420,
	.prox_thresh_low = 245,
	.prox_th_hi_cal = 470,
	.prox_th_low_cal = 380,
	.als_time = 0xED,
	.intr_filter = 0x22,
	.prox_pulsecnt = 0x10,
	.prox_gain = 0x28,
	.coef_atime = 50,
	.ga = 97,
	.coef_a = 1000,
	.coef_b = 1850,
	.coef_c = 510,
	.coef_d = 870,
	.max_data = true,
};
#endif
#if defined(CONFIG_SENSORS_GP2A30)
static void gp2a_proximity_leda_on(bool onoff)
{
	gpio_set_value(GPIO_ALS_3V_EN, onoff);
	msleep(20);
	pr_info("%s, onoff = %d\n", __func__, onoff);
}
static int gp2a_light_sensor_power(bool onoff)
{
	static struct regulator *proxy_2v85 = NULL;
	static bool init_state = 1;
	static bool light_status = 0;

	pr_info("%s : %d\n", __func__, onoff);

	if (onoff == light_status)
		return 0;

	if (init_state)
	{
		if (!proxy_2v85) {
			proxy_2v85 = regulator_get(NULL, "prox_vdd_2.8v");
			if (IS_ERR(proxy_2v85)) {
				pr_err("%s v_proxy_2v85 regulator get error!\n", __func__);
				proxy_2v85 = NULL;
				return -1;
			}
		}
		init_state = 0;
	}

	if (onoff) {
		regulator_enable(proxy_2v85);
		msleep(2);
		light_status = 1;
	} else {
		regulator_disable(proxy_2v85);
		msleep(2);
		light_status = 0;
	}

	return 0;
}

static struct gp2a_platform_data gp2a_pdata = {
	.power_on = gp2a_light_sensor_power,
	.led_on = gp2a_proximity_leda_on,
	.p_out = GPIO_PS_INT,
	.prox_cal_path = "/efs/prox_cal",
};
#endif

static struct i2c_board_info i2c_devs2[] __initdata = {
#if defined(CONFIG_INPUT_MPU6500)
	{
		I2C_BOARD_INFO("mpu6500_input", 0x68),
		.irq = IRQ_EINT(27),
		.platform_data = &mpu6500_pdata,
	},
#elif defined(CONFIG_INV_MPU_IIO)
	{
		I2C_BOARD_INFO("mpu6515", 0x68),
		.irq = IRQ_EINT(27),
		.platform_data = &mpu_iio,
	},
#endif
#if defined(CONFIG_SENSORS_AK09911C)
	{
		I2C_BOARD_INFO("ak09911c-i2c", 0x0c),
		.platform_data = &ak09911c_pdata,
	},
#endif
#if defined(CONFIG_SENSORS_TMD27723)
	{
		I2C_BOARD_INFO("tmd27723", 0x39),
		.irq = IRQ_EINT(15),
		.platform_data = &tmd27723_pdata,
	},
#endif
};

static int initialize_sensor_gpio(void)
{
	int ret = 0;

	pr_info("%s, is called\n", __func__);

#if defined(CONFIG_INPUT_MPU6500) || defined(CONFIG_INV_MPU_IIO)
	ret = gpio_request(GPIO_G_SENSOR_INT, "MPU6500_INT");
	if (ret)
		pr_err("%s, failed to request MPU6500_INT (%d)\n", __func__, ret);

	s3c_gpio_cfgpin(GPIO_G_SENSOR_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_G_SENSOR_INT, S3C_GPIO_PULL_DOWN);
#endif
#if defined(CONFIG_SENSORS_AK09911C)
	ret = gpio_request(GPIO_M_SENSOR_RSTN, "M_RST_N");
	if (ret)
		pr_err("%s, failed to request M_RST_N (%d)\n", __func__, ret);

	s3c_gpio_cfgpin(GPIO_M_SENSOR_RSTN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_M_SENSOR_RSTN, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_M_SENSOR_RSTN, 1);
	gpio_free(GPIO_M_SENSOR_RSTN);
#endif
#if defined(CONFIG_SENSORS_TMD27723)
	ret = gpio_request(GPIO_PROX_LED_EN, "PROX_LED_EN");
	if (ret)
		pr_err("%s, failed to request gpio_proximity_led_en. (%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(GPIO_PROX_LED_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_PROX_LED_EN, S3C_GPIO_PULL_DOWN);
	gpio_set_value(GPIO_PROX_LED_EN, 0);

	ret = gpio_request(GPIO_PROX_SENSOR_INT, "gpio_proximity_int");
	if (ret)
		pr_err("%s, failed to request gpio_proximity_int. (%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(GPIO_PROX_SENSOR_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_PROX_SENSOR_INT, S3C_GPIO_PULL_NONE);
	gpio_free(GPIO_PROX_SENSOR_INT);
#elif defined(CONFIG_SENSORS_GP2A30)
	ret = gpio_request(GPIO_ALS_28V_EN, "ps_power_supply_on");
	if (ret) {
		pr_err("%s, Failed to request gpio ps power supply(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = gpio_request(GPIO_ALS_3V_EN, "als_power_supply_on");
	if (ret) {
		pr_err("%s, Failed to request gpio als power supply(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = gpio_request(GPIO_PS_INT, "ps_interrupt");
	if (ret) {
		pr_err("%s, Failed to request gpio ps_int(%d)\n",
			__func__, ret);
		return ret;
	}

	/* configuring for gp2a gpio for LEDA power */
	s3c_gpio_cfgpin(GPIO_ALS_28V_EN, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_ALS_28V_EN, 1);
	s3c_gpio_setpull(GPIO_ALS_28V_EN, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_ALS_3V_EN, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_ALS_3V_EN, 1);
	s3c_gpio_setpull(GPIO_ALS_3V_EN, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_PS_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_PS_INT, S3C_GPIO_PULL_UP);

	gpio_free(GPIO_PS_INT);
#endif
	return ret;
}

struct exynos5_platform_i2c sensor_i2c2_platdata __initdata = {
	.bus_number = 2,
	.operation_mode = HSI2C_POLLING,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 400000,
	.high_speed = 2500000,
	.cfg_gpio = NULL,
};

#if defined(CONFIG_SENSORS_GP2A30)
static struct i2c_gpio_platform_data i2c_gpio_data21 = {
	.sda_pin		= GPIO_PS_ALS_SDA_18V,
	.scl_pin		= GPIO_PS_ALS_SCL_18V,
};
static struct platform_device gp2a_gpio = {
	.name = "i2c-gpio",
	.id = 4,
	.dev = {
		.platform_data = &i2c_gpio_data21,
	},
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("gp2a", 0x39),
		.irq = IRQ_EINT(15),
		.platform_data = &gp2a_pdata,
	},
};
#endif

void __init exynos5_universal5260_sensor_init(void)
{
	int ret = 0;

	pr_info("%s, is called\n", __func__);

	ret = initialize_sensor_gpio();
	if (ret)
		pr_err("%s, initialize_sensor_gpio (err=%d)\n", __func__, ret);

#if defined(CONFIG_SENSORS_GP2A30)
	s3c_i2c4_set_platdata(NULL);

	ret = platform_device_register(&gp2a_gpio);
	if (ret < 0) {
		pr_err("%s, failed to register gp2a_gpio(err=%d)\n",
			__func__, ret);
	}
	ret = i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
	if (ret < 0) {
		pr_err("%s, i2c4 adding i2c fail(err=%d)\n", __func__, ret);
	}
#endif

	exynos5_hs_i2c2_set_platdata(&sensor_i2c2_platdata);
	ret = i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
	if (ret < 0) {
		pr_err("%s, i2c2 adding i2c fail(err=%d)\n", __func__, ret);
	}
	ret = platform_device_register(&exynos5_device_hs_i2c2);
	if (ret < 0)
		pr_err("%s, sensor platform device register failed (err=%d)\n",
			__func__, ret);

}

