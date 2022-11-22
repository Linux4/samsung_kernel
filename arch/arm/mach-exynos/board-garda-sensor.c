#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include <linux/sensor/sensors_axes_s16.h>
#include <linux/sensor/ak8963.h>
#include <linux/sensor/k2dh.h>
#include <linux/sensor/gp2a.h>
#include <linux/nfc/pn547.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include "board-universal222ap.h"

#define GPIO_MSENSE_INT		EXYNOS4_GPX3(4)
#define GPIO_MSENSE_RST		EXYNOS4_GPM4(4)
#define GPIO_PROXI_INT		EXYNOS4_GPX2(7)
#define GPIO_NFC_IRQ		EXYNOS4_GPX1(7)
#define GPIO_NFC_EN		EXYNOS4_GPF1(4)
#define GPIO_NFC_FIRMWARE	EXYNOS4_GPM1(5)

extern unsigned int system_rev;
extern int vibrator_on;

static u8 stm_get_position(void)
{
	u8 position = 1;

	return position;
}

static void gp2a_power(bool onoff)
{
	pr_info("%s: gp2a %s\n", __func__, (onoff)?"on":"off");
}

static struct gp2a_platform_data gp2a_pdata = {
	.power	= gp2a_power,
	.p_out = GPIO_PROXI_INT,
	.irq = IRQ_EINT(23),
};

static void gp2a_gpio_init(void)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = gpio_request(GPIO_PROXI_INT, "gpio_proximity_out");
	if (ret)
		pr_err("%s, Failed to request gpio gpio_proximity_out. (%d)\n",
			__func__, ret);

	s3c_gpio_setpull(GPIO_PROXI_INT, S3C_GPIO_PULL_NONE);
	gpio_free(GPIO_PROXI_INT);
}

static u8 ak8963_get_position(void)
{
	u8 position;

	position = AXES_PYNXPZ;

	return position;
}

static void ak8963c_gpio_init(void)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = gpio_request(GPIO_MSENSE_INT, "gpio_akm_int");
	if (ret)
		pr_err("%s, Failed to request gpio akm_int. (%d)\n",
			__func__, ret);

	s5p_register_gpio_interrupt(GPIO_MSENSE_INT);
	s3c_gpio_setpull(GPIO_MSENSE_INT, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(GPIO_MSENSE_INT, S3C_GPIO_SFN(0xF));
	gpio_free(GPIO_MSENSE_INT);

	ret = gpio_request(GPIO_MSENSE_RST, "gpio_akm_rst");
	if (ret)
		pr_err("%s, Failed to request gpio akm_rst. (%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(GPIO_MSENSE_RST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MSENSE_RST, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_MSENSE_RST, 1);
	gpio_free(GPIO_MSENSE_RST);
}

static void pn547_gpio_init(void)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = gpio_request(GPIO_NFC_IRQ, "nfc_int");
	if (ret)
		pr_err("%s, Failed to request gpio nfc_int. (%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(GPIO_NFC_IRQ, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_NFC_IRQ, S3C_GPIO_PULL_DOWN);
	gpio_free(GPIO_NFC_IRQ);

	ret = gpio_request(GPIO_NFC_EN, "nfc_ven");
	if (ret)
		pr_err("%s, Failed to request gpio nfc_ven. (%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(GPIO_NFC_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_NFC_EN, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_NFC_EN, 0);
	gpio_free(GPIO_NFC_EN);

	ret = gpio_request(GPIO_NFC_FIRMWARE, "nfc_firm");
	if (ret)
		pr_err("%s, Failed to request gpio nfc_firm. (%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(GPIO_NFC_FIRMWARE, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_NFC_FIRMWARE, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_NFC_FIRMWARE, 0);
	gpio_free(GPIO_NFC_FIRMWARE);
}


static struct accel_platform_data accel_pdata = {
	.accel_get_position = stm_get_position,
	.axis_adjust = true,
	.vibrator_on = &vibrator_on,
};

static struct akm8963_platform_data akm8963_pdata = {
	.gpio_data_ready_int = GPIO_MSENSE_INT,
	.gpio_reset = GPIO_MSENSE_RST,
	.mag_get_position = ak8963_get_position,
	.select_func = select_func_s16,
};

static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("k2dh", 0x19),
		.platform_data	= &accel_pdata,
	},
	{
		I2C_BOARD_INFO("ak8963", 0x0C),
		.platform_data = &akm8963_pdata,
		.irq = IRQ_EINT(28),
	},
	{
		I2C_BOARD_INFO("gp2a", 0x44),
		.platform_data = &gp2a_pdata,
	},
};

static struct pn547_i2c_platform_data pn547_pdata = {
	.irq_gpio = GPIO_NFC_IRQ,
	.ven_gpio = GPIO_NFC_EN,
	.firm_gpio = GPIO_NFC_FIRMWARE,
};

static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("pn547", 0x2b),
		.irq = IRQ_EINT(15),
		.platform_data = &pn547_pdata,
	},
};

static struct platform_device *sensor_i2c_devices[] __initdata = {
	&s3c_device_i2c5,
	&s3c_device_i2c7,
};

void __init board_universal_ss222ap_init_sensor(void)
{
	int ret;

	ak8963c_gpio_init();
	pn547_gpio_init();
	gp2a_gpio_init();

	s3c_i2c5_set_platdata(NULL);
	s3c_i2c7_set_platdata(NULL);
	if (system_rev > 7) {
		ret = i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
		if (ret < 0)
			pr_err("%s, i2c5 adding i2c fail(err=%d)\n",
						__func__, ret);
	} else
		pr_info("%s : system rev = %d, skip pn547(nfc) load\n",
			__func__, system_rev);

	ret = i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));
	if (ret < 0) {
		pr_err("%s, i2c7 adding i2c fail(err=%d)\n",
						__func__, ret);
	}

	platform_add_devices(sensor_i2c_devices, ARRAY_SIZE(sensor_i2c_devices));
}
