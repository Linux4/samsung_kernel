#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/of_gpio.h>

#include "gxy_vbus_ctrl.h"

static struct vbus_ctrl_dev *g_vbus_ctrl_t = NULL;

static void vbus_ctrl_state(int state)
{
    int ret = 0;

    pr_debug("[%s], vbus_ctrl_state(%d)\n", __func__, g_vbus_ctrl_t->state);

    if (g_vbus_ctrl_t->state == state) {
        pr_debug("[%s] keep vbus control gpio state()\n", __func__, state);
        return;
    }

    ret = gpio_direction_output(g_vbus_ctrl_t->vbus_ctrl_gpio, state);
    if (ret < 0) {
        pr_err("[%s] line=%d: failed to set vbus_control_gpio\n", __func__, __LINE__);
    }

    g_vbus_ctrl_t->state = state;
}

static int get_usb_therm_voltage(void)
{
    struct iio_channel *channel = NULL;
    int ret = 0;
    int number = 0;
    int val = 0;

    channel = g_vbus_ctrl_t->channel;

    number = channel->channel->channel;
    ret = iio_read_channel_processed(channel, &val);
    if (ret < 0) {
        pr_err("iio_read_channel_processed fail\n");
        goto USB_THERM_FAIL;
    }

    pr_info(" [%s] usb_therm number = %d, val = %d\n", __func__, number, val);

    return val;

USB_THERM_FAIL:
    pr_err("[%s] get usb_therm voltage, load default value\n", __func__);

    return VCHARGE_DEFAULT_TEMP_VOLTAGE;
}

static int update_usb_connector_state(void)
{
    int usb_ntc_votage = 0;
    int usb_state = 0;
    static int s_usb_state_old = 0;

    usb_ntc_votage = get_usb_therm_voltage();
    if (usb_ntc_votage <=  VCHARGE_HOT_TEMP_VOLTAGE) {
        usb_state = CHG_ALERT_HOT_STATE;
    } else if (usb_ntc_votage < VCHARGE_WARM_TEMP_VOLTAGE) {
        usb_state = CHG_ALERT_WARM_STATE;
    } else {
        usb_state = CHG_ALERT_NORMAL_STATE;
    }

    /* USB Temp over HOT_TEMP-70℃, stop charging, below WARM_TEMP-60℃, re-charging */
    if ((usb_ntc_votage <= VCHARGE_HOT_TEMP_VOLTAGE) && (s_usb_state_old != usb_state)) {
        #ifdef CONFIG_ODM_CUSTOM_D85_BUILD
        pr_notice("[%s]line=%d: USB connector hot, no operation in D85\n", __func__, __LINE__);
        #else
        vbus_ctrl_state(VBUS_CTRL_HIGH);
        pr_notice("[%s]line=%d: USB connector hot, connect VBUS to GND\n", __func__, __LINE__);
        #endif
    } else if ((usb_ntc_votage > VCHARGE_WARM_TEMP_VOLTAGE) && (s_usb_state_old != usb_state)) {
        vbus_ctrl_state(VBUS_CTRL_LOW);
        pr_notice("[%s]line=%d: USB connector GOOD, disconnect VBUS to GND\n", __func__, __LINE__);
    }

    s_usb_state_old = usb_state;

    return 0;
}

static void vbus_ctrl_state_workq(struct work_struct *work)
{
    update_usb_connector_state();

    queue_delayed_work(system_freezable_wq, &g_vbus_ctrl_t->vbus_ctrl_state_work,
                       msecs_to_jiffies(USB_TERMAL_DETECT_TIMER));
}

static void vbus_ctrl_work_init_config(struct vbus_ctrl_dev *g_vbus_ctrl_t)
{
    if (!g_vbus_ctrl_t) {
        pr_err("[%s]g_vbus_ctrl_t is null\n", __func__);
        return;
    }

    update_usb_connector_state();

    queue_delayed_work(system_freezable_wq, &g_vbus_ctrl_t->vbus_ctrl_state_work,
                       msecs_to_jiffies(USB_TERMAL_DETECT_TIMER));

    pr_info("[%s]ready g_vbus_ctrl_t\n", __func__);
}

static int vbus_ctrl_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    int ret = 0;

    pr_info("[%s] enter\n", __func__);

    g_vbus_ctrl_t = devm_kzalloc(&pdev->dev, sizeof(*g_vbus_ctrl_t), GFP_KERNEL);
    if (!g_vbus_ctrl_t) {
        pr_err("[%s], g_vbus_ctrl_t allocation fail, just return!\n", __func__);
        return -ENOMEM;
    }

    ret = of_get_named_gpio(np, "vbus_ctrl_gpio", 0);
    if (ret < 0) {
        pr_err("[%s], could not get vbus_ctrl_gpio\n", __func__);
        return ret;
    } else {
        g_vbus_ctrl_t->vbus_ctrl_gpio = ret;
        pr_info("[%s], vbus_ctrl_gpio: %d\n", __func__, g_vbus_ctrl_t->vbus_ctrl_gpio);
        ret = gpio_request(g_vbus_ctrl_t->vbus_ctrl_gpio, "GPIO160");
        if (ret) {
            pr_err("failed to request %d\n", __func__, g_vbus_ctrl_t->vbus_ctrl_gpio);
            return -EINVAL;
        }
    }

    g_vbus_ctrl_t->channel = iio_channel_get(&pdev->dev, "gxy-usb-therm");
    ret = IS_ERR(g_vbus_ctrl_t->channel);
    if (ret) {
        pr_err("[%s] fail to get usb therm iio channel (%d)\n", __func__, ret);
        return ret;
    }

    INIT_DELAYED_WORK(&g_vbus_ctrl_t->vbus_ctrl_state_work, vbus_ctrl_state_workq);
    vbus_ctrl_work_init_config(g_vbus_ctrl_t);

    pr_info("%s success\n", __func__);

    return 0;
}

static int vbus_ctrl_remove(struct platform_device *dev)
{
    cancel_delayed_work(&g_vbus_ctrl_t->vbus_ctrl_state_work);

    return 0;
}

static void vbus_ctrl_detach_usb(void)
{
    cancel_delayed_work(&g_vbus_ctrl_t->vbus_ctrl_state_work);
    gpio_direction_output(g_vbus_ctrl_t->vbus_ctrl_gpio, 0);
}

static const struct of_device_id vbus_auxadc_of_ids[] = {
    {.compatible = "gxy, vbus_ctrl"},
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
    int ret = 0;

    ret = platform_driver_register(&vbus_ctrl_driver);
    if (ret) {
        pr_err("%s vbus adc driver init fail %d", __func__, ret);
        return ret;
    }

    return 0;
}
module_init(vbus_ctrl_init);

static void __exit vbus_ctrl_exit(void)
{
    vbus_ctrl_detach_usb();
    platform_driver_unregister(&vbus_ctrl_driver);
}
module_exit(vbus_ctrl_exit);

MODULE_AUTHOR("gxy");
MODULE_DESCRIPTION("vbus ctrl driver");
MODULE_LICENSE("GPL");