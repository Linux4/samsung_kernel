#define LOG_TAG "LCM"
#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"
#include "lcm_define.h"

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#include <linux/gpio.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#endif
#endif

#ifdef CONFIG_HQ_PROJECT_HS03S
unsigned int GPIO_LCD_RST;
/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 start */
unsigned int GPIO_TSP_RST;
/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 end */
struct pinctrl *lcd_pinctrl1;
struct pinctrl_state *lcd_disp_pwm;
struct pinctrl_state *lcd_disp_pwm_gpio;

void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, output);
#else
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
#endif
}
static void lcm_request_gpio_control(struct device *dev)
{
	int ret;

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");
	/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 start */
	GPIO_TSP_RST = of_get_named_gpio(dev->of_node, "gpio_tsp_rst", 0);
	gpio_request(GPIO_TSP_RST, "GPIO_TSP_RST");
	/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 end */

	lcd_pinctrl1 = devm_pinctrl_get(dev);
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
		pr_notice(" Cannot find lcd_pinctrl1 %d!\n", ret);
	}

	lcd_disp_pwm = pinctrl_lookup_state(lcd_pinctrl1, "disp_pwm");
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
		pr_notice(" Cannot find lcd_disp_pwm %d!\n", ret);
	}

	lcd_disp_pwm_gpio = pinctrl_lookup_state(lcd_pinctrl1, "disp_pwm_gpio");
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
	}
}
#endif
#ifdef CONFIG_HQ_PROJECT_O22
unsigned int GPIO_LCD_RST;
/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 start */
unsigned int GPIO_TSP_RST;
/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 end */
struct pinctrl *lcd_pinctrl1;
struct pinctrl_state *lcd_disp_pwm;
struct pinctrl_state *lcd_disp_pwm_gpio;
struct pinctrl_state *tp_rst_low;
struct pinctrl_state *tp_rst_high;

void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, output);
#else
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
#endif
}
static void lcm_request_gpio_control(struct device *dev)
{
	int ret;

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");

	lcd_pinctrl1 = devm_pinctrl_get(dev);
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
		pr_notice(" Cannot find lcd_pinctrl1 %d!\n", ret);
	}

	tp_rst_low = pinctrl_lookup_state(lcd_pinctrl1, "tp_rst_low");
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
		pr_notice(" Cannot find lcd_disp_pwm %d!\n", ret);
	}

	tp_rst_high = pinctrl_lookup_state(lcd_pinctrl1, "tp_rst_high");
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
	}
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
unsigned int GPIO_LCD_RST;
/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 start */
unsigned int GPIO_TSP_RST;
/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 end */
struct pinctrl *lcd_pinctrl1;
struct pinctrl_state *lcd_disp_pwm;
struct pinctrl_state *lcd_disp_pwm_gpio;

void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, output);
#else
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
#endif
}
static void lcm_request_gpio_control(struct device *dev)
{
	int ret;

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");
	/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 start */
	GPIO_TSP_RST = of_get_named_gpio(dev->of_node, "gpio_tsp_rst", 0);
	gpio_request(GPIO_TSP_RST, "GPIO_TSP_RST");
	/* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 end */

	lcd_pinctrl1 = devm_pinctrl_get(dev);
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
		pr_notice(" Cannot find lcd_pinctrl1 %d!\n", ret);
	}

	lcd_disp_pwm = pinctrl_lookup_state(lcd_pinctrl1, "disp_pwm");
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
		pr_notice(" Cannot find lcd_disp_pwm %d!\n", ret);
	}

	lcd_disp_pwm_gpio = pinctrl_lookup_state(lcd_pinctrl1, "disp_pwm_gpio");
	if (IS_ERR(lcd_pinctrl1)) {
		ret = PTR_ERR(lcd_pinctrl1);
	}
}
#endif

static int lcm_driver_probe(struct device *dev, void const *data)
{
#ifdef CONFIG_HQ_PROJECT_HS03S
	lcm_request_gpio_control(dev);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
	lcm_request_gpio_control(dev);
#endif
#ifdef CONFIG_HQ_PROJECT_O22
	lcm_request_gpio_control(dev);
#endif
	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "mediatek,lcm-panel",
		.data = 0,
	}, {
		/* sentinel */
	}
};

MODULE_DEVICE_TABLE(of, platform_of_match);

static int lcm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	id = of_match_node(lcm_platform_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	return lcm_driver_probe(&pdev->dev, id->data);
}

static struct platform_driver lcm_driver = {
	.probe = lcm_platform_probe,
	.driver = {
		.name = "a03s_lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_drv_init(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
	if (platform_driver_register(&lcm_driver)) {
		pr_notice("LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_drv_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("LCM: Unregister lcm driver done\n");
}

module_init(lcm_drv_init);
module_exit(lcm_drv_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");
