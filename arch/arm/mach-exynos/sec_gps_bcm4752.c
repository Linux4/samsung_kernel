#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/sec_gps.h>

#define GPS_UART_AF         2

static struct device *gps_dev;


static int __init gps_bcm4752_init(void)
{
	BUG_ON(!sec_class);
	gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
	BUG_ON(!gps_dev);

	s3c_gpio_cfgpin(GPIO_GPS_UART_RXD, S3C_GPIO_SFN(GPS_UART_AF));
	s3c_gpio_setpull(GPIO_GPS_UART_RXD, S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(GPIO_GPS_UART_TXD, S3C_GPIO_SFN(GPS_UART_AF));
	s3c_gpio_setpull(GPIO_GPS_UART_TXD, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_UART_CTS, S3C_GPIO_SFN(GPS_UART_AF));
	s3c_gpio_setpull(GPIO_GPS_UART_CTS, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_UART_RTS, S3C_GPIO_SFN(GPS_UART_AF));
	s3c_gpio_setpull(GPIO_GPS_UART_RTS, S3C_GPIO_PULL_NONE);

	if (gpio_request(GPIO_GPS_PWR_EN, "GPS_PWR_EN")) {
		WARN(1, "fail to request gpio (GPS_PWR_EN)\n");
		return 1;
	}

	s3c_gpio_setpull(GPIO_GPS_PWR_EN, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_PWR_EN, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_PWR_EN, 0);

	gpio_export(GPIO_GPS_PWR_EN, 1);

	gpio_export_link(gps_dev, "GPS_PWR_EN", GPIO_GPS_PWR_EN);

	return 0;
}

device_initcall(gps_bcm4752_init);