/* hs14 code for SR-AL6528A-01-305 by shanxinkai at 2022/09/06 start */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/platform_device.h>	/* platform device */
#include <linux/power_supply.h>
#include <linux/of_gpio.h>

#include "vbus_ctrl.h"
#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"

static struct vbus_ctrl_dev *g_vbus_ctrl;

static void vbus_ctrl_state(int state)
{
	int ret = 0;

	pr_debug("%s, vbus_ctrl_state(%d)\n", __func__, g_vbus_ctrl->state);
	if(g_vbus_ctrl->state == state) {
		pr_debug("keep vbus control gpio state()\n", state);
		return;
	}
	g_vbus_ctrl->state = state;

	ret = gpio_direction_output(g_vbus_ctrl->vbus_ctrl_gpio, g_vbus_ctrl->state);
	if(ret < 0)
		pr_err("[%s]line=%d: failed to set vbus_control_gpio\n", __func__, __LINE__);

	return;
}

static int update_usb_connector_state(void)
{
	int usb_temp = 0;
	int usb_state = 0;
	static int usb_state_old = 0;

	usb_temp = mtktsusb_get_hw_temp();
	if (usb_temp >= CHG_ALERT_HOT_TEMP) { //USB thermal >= 70
		usb_state = CHG_ALERT_HOT_STATE;
	} else if (usb_temp < CHG_ALERT_WARM_TEMP) { //USB thermal < 60
		usb_state = CHG_ALERT_WARM_STATE;
	}

	// USB Temp over HOT_TEMP, stop charging, below WARM_TEMP, re-charging
	if (usb_temp >= CHG_ALERT_HOT_TEMP && usb_state_old != usb_state) { //USB thermal >= 70
		/* hs14 code for SR-AL6528A-01-336 by shanxinkai at 2022/09/15 start */
		#ifdef HQ_D85_BUILD
		pr_err("[%s]line=%d: USB connector hot, there is no operation in D85\n", __func__, __LINE__);
		#else
		vbus_ctrl_state(VBUS_CTRL_HIGH);
		pr_err("[%s]line=%d: USB connector hot, connect VBUS to GND\n", __func__, __LINE__);
		#endif
		/* hs14 code for SR-AL6528A-01-336 by shanxinkai at 2022/09/15 end */
	} else if (usb_temp < CHG_ALERT_WARM_TEMP && usb_state_old != usb_state) { //USB thermal < 60
		vbus_ctrl_state(VBUS_CTRL_LOW);
		pr_err("[%s]line=%d: USB connector GOOD, disconnect VBUS to GND\n", __func__, __LINE__);
	}
	usb_state_old = usb_state;
	return 0;
}

static void vbus_ctrl_state_workq(struct work_struct *work)
{
	update_usb_connector_state();
	/* hs14 code for SR-AL6528A-317 by shanxinkai at 2022/11/21 start */
	queue_delayed_work(system_freezable_wq, &g_vbus_ctrl->vbus_ctrl_state_work, msecs_to_jiffies(USB_TERMAL_DETECT_TIMER));
	/* hs14 code for SR-AL6528A-317 by shanxinkai at 2022/11/21 end */
	return;
}

static void vbus_ctrl_work_init_config(struct vbus_ctrl_dev *g_vbus_ctrl)
{
	if(!g_vbus_ctrl)
	{
		pr_err("[%s]g_vbus_ctrl is null\n", __func__);
		return;
	}
	/* hs14 code for SR-AL6528A-317 by shanxinkai at 2022/11/21 start */
	queue_delayed_work(system_freezable_wq, &g_vbus_ctrl->vbus_ctrl_state_work, msecs_to_jiffies(USB_TERMAL_DETECT_TIMER));
	/* hs14 code for SR-AL6528A-317 by shanxinkai at 2022/11/21 end */
	pr_debug("[%s]ready g_vbus_ctrl\n", __func__);
}

static int vbus_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np= dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);

	g_vbus_ctrl = devm_kzalloc(&pdev->dev, sizeof(*g_vbus_ctrl), GFP_KERNEL);
	if (!g_vbus_ctrl) {
		pr_err("%s, g_vbus_ctrl allocation fail, just return!\n", __func__);
		return -ENOMEM;
	}

	ret = of_get_named_gpio(np, "vbus_ctrl_gpio", 0);

	if (ret < 0) {
		pr_err("%s, could not get vbus_ctrl_gpio\n", __func__);
		return ret;
	} else {
		g_vbus_ctrl->vbus_ctrl_gpio = ret;
		pr_info("%s, g_vbus_ctrl->vbus_ctrl_gpio: %d\n", __func__, g_vbus_ctrl->vbus_ctrl_gpio);
		ret = gpio_request(g_vbus_ctrl->vbus_ctrl_gpio, "GPIO160");
	}

	g_vbus_ctrl->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(g_vbus_ctrl->pinctrl)) {
		pr_err("%s, No pinctrl config specified\n", __func__);
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&g_vbus_ctrl->vbus_ctrl_state_work, vbus_ctrl_state_workq);
	vbus_ctrl_work_init_config(g_vbus_ctrl);

	return 0;
}

static int vbus_ctrl_remove(struct platform_device *dev)
{
	cancel_delayed_work(&g_vbus_ctrl->vbus_ctrl_state_work);
	return 0;
}

static void detach_usb(void)
{
	cancel_delayed_work(&g_vbus_ctrl->vbus_ctrl_state_work);
	gpio_direction_output(g_vbus_ctrl->vbus_ctrl_gpio, 0);
}

static const struct of_device_id vbus_auxadc_of_ids[] = {
	{.compatible = "hq, vbus_ctrl"},
	{}
};


static struct platform_driver vbus_ctrl_driver = {

	.driver = {
			.name = "vbus_ctrl",
			.of_match_table = vbus_auxadc_of_ids,
	},

	.probe = vbus_ctrl_probe,
	.remove = vbus_ctrl_remove,
};

static int __init vbus_ctrl_init(void)
{
	int ret;

	ret = platform_driver_register(&vbus_ctrl_driver);
	if (ret) {
		pr_err("vbus adc driver init fail %d", ret);
		return ret;
	}
	return 0;
}
module_init(vbus_ctrl_init);

static void __exit vbus_ctrl_exit(void)
{
	detach_usb();
	platform_driver_unregister(&vbus_ctrl_driver);
}
module_exit(vbus_ctrl_exit);

MODULE_AUTHOR("zh");
MODULE_DESCRIPTION("vbus ctrl driver");
MODULE_LICENSE("GPL");
/* hs14 code for SR-AL6528A-01-305 by shanxinkai at 2022/09/06 end */