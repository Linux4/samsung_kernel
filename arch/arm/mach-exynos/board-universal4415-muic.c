#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/muic.h>
#include <asm/system_info.h>

#include "board-universal4415.h"

static struct muic_platform_data exynos4_muic_pdata;

#if defined(GPIO_USB_SEL)
static int rt8973_set_gpio_usb_sel(int uart_sel)
{
	return 0;
}
#endif /* GPIO_USB_SEL */

#if defined(GPIO_UART_SEL)
static int rt8973_set_gpio_uart_sel(int uart_sel)
{
	const char *mode;
	int uart_sel_gpio = exynos4_muic_pdata.gpio_uart_sel;
	int uart_sel_val;
	int ret;

	ret = gpio_request(uart_sel_gpio, "GPIO_UART_SEL");
	if (ret) {
		pr_err("failed to gpio_request GPIO_UART_SEL\n");
		return ret;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	pr_info("%s: uart_sel(%d), GPIO_UART_SEL(%d)=%c ->", __func__, uart_sel,
			uart_sel_gpio, (uart_sel_val == 0 ? 'L' : 'H'));

	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_set_value(uart_sel_gpio, GPIO_LEVEL_HIGH);
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_set_value(uart_sel_gpio, GPIO_LEVEL_LOW);
		break;
	default:
		mode = "Error";
		break;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	gpio_free(uart_sel_gpio);

	pr_info(" %s, GPIO_UART_SEL(%d)=%c\n", mode, uart_sel_gpio,
			(uart_sel_val == 0 ? 'L' : 'H'));

	return 0;
}
#endif /* GPIO_UART_SEL */

#if defined(GPIO_DOC_SWITCH)
static int rt8973_set_gpio_doc_switch(int val)
{
	int doc_switch_gpio = exynos4_muic_pdata.gpio_doc_switch;
	int doc_switch_val;
	int ret;

	ret = gpio_request(doc_switch_gpio, "GPIO_UART_SEL");
	if (ret) {
		pr_err("failed to gpio_request GPIO_UART_SEL\n");
		return ret;
	}

	doc_switch_val = gpio_get_value(doc_switch_gpio);

	pr_info("%s: doc_switch(%d) --> (%d)", __func__, doc_switch_val, val);

	if (gpio_is_valid(doc_switch_gpio))
			gpio_set_value(doc_switch_gpio, val);

	doc_switch_val = gpio_get_value(doc_switch_gpio);

	gpio_free(doc_switch_gpio);

	pr_info("%s: GPIO_DOC_SWITCH(%d)=%c\n",__func__, doc_switch_gpio,
			(doc_switch_val == 0 ? 'L' : 'H'));

	return 0;
}
#endif /* GPIO_DOC_SWITCH */

static int muic_init_gpio_cb(int switch_sel)
{
	struct muic_platform_data *pdata = &exynos4_muic_pdata;
	const char *usb_mode;
	const char *uart_mode;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(GPIO_USB_SEL)
	pdata->set_gpio_usb_sel = rt8973_set_gpio_usb_sel;
#endif
#if defined(GPIO_UART_SEL)
	if (0x4 > system_rev)
		pdata->set_gpio_uart_sel = rt8973_set_gpio_uart_sel;
#endif
#if defined(GPIO_DOC_SWITCH)
	pdata->set_gpio_doc_switch = rt8973_set_gpio_doc_switch;
#endif

	if (switch_sel & SWITCH_SEL_USB_MASK) {
		pdata->usb_path = MUIC_PATH_USB_AP;
		usb_mode = "PDA";
	} else {
		pdata->usb_path = MUIC_PATH_USB_CP;
		usb_mode = "MODEM";
	}

	if (pdata->set_gpio_usb_sel)
		ret = pdata->set_gpio_usb_sel(pdata->uart_path);

	if (switch_sel & SWITCH_SEL_UART_MASK) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		uart_mode = "AP";
	} else {
		pdata->uart_path = MUIC_PATH_UART_CP;
		uart_mode = "CP";
	}

	if (pdata->set_gpio_uart_sel)
		ret = pdata->set_gpio_uart_sel(pdata->uart_path);

	pr_info("%s: usb_path(%s), uart_path(%s)\n", __func__, usb_mode,
			uart_mode);

	return ret;
}

static struct muic_platform_data exynos4_muic_pdata = {
	.irq_gpio		= GPIO_MUIC_INT_N,
#if defined(GPIO_USB_SEL)
	.gpio_usb_sel		= GPIO_USB_SEL,
#endif
#if defined(GPIO_UART_SEL)
	.gpio_uart_sel		= GPIO_UART_SEL,
#endif
#if defined(GPIO_DOC_SWITCH)
	.gpio_doc_switch = GPIO_DOC_SWITCH,
#endif
	.init_gpio_cb		= muic_init_gpio_cb,
};

static struct i2c_board_info i2c_devs17_emul[] __initdata = {
	{
#if defined(CONFIG_USB_SWITCH_RT8973)
		I2C_BOARD_INFO("rt8973", (0x14)),
#elif defined(CONFIG_MUIC_SM5502)
		I2C_BOARD_INFO(MUIC_DEV_NAME, (0x4A >> 1)),
#endif
		.platform_data	= &exynos4_muic_pdata,
	}
};

static struct i2c_gpio_platform_data gpio_i2c_data17 = {
	.sda_pin = GPIO_MUIC_SDA,
	.scl_pin = GPIO_MUIC_SCL,
};

struct platform_device s3c_device_i2c17 = {
	.name = "i2c-gpio",
	.id = 17,
	.dev.platform_data = &gpio_i2c_data17,
};

static struct platform_device *exynos4_muic_device[] __initdata = {
	&s3c_device_i2c17,
};

void __init exynos4_universal4415_muic_init(void)
{
	i2c_register_board_info(17, i2c_devs17_emul,
				ARRAY_SIZE(i2c_devs17_emul));

	platform_add_devices(exynos4_muic_device,
			ARRAY_SIZE(exynos4_muic_device));
}
