#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/compat.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/of_gpio.h>

#include "sar_mode_detect.h"

#include <linux/init.h>
#include <linux/module.h>

#include <linux/input.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/platform_device.h>

#define SARDSI_DEBUG 0
#define LOG_TAG "[SARDSI] "

#if SARDSI_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

#define SARDSI_COMPATIBLE_DEVICE_ID "mediatek,sardsi_detect"
static bool sardsi_state;
module_param(sardsi_state,bool,0644);

struct sardsi_t sardsi_data;
const struct of_device_id sardsi_of_match[] = {
    { .compatible = SARDSI_COMPATIBLE_DEVICE_ID, },
};

static int sardsi_probe(struct platform_device * pdev)
{
    int ret = 0;
    struct pinctrl * sardsi_pinctrl = NULL;
    struct pinctrl_state * state = NULL;

    LOG_INFO("Enter %s\n", __func__);

    sardsi_pinctrl = devm_pinctrl_get(&pdev->dev);
    state = pinctrl_lookup_state(sardsi_pinctrl, "sardsi_active");
    if (IS_ERR(state)) {
        LOG_INFO(" can't find sardsi active\n");
        return -EINVAL;
    }

    ret = pinctrl_select_state(sardsi_pinctrl, state);
    if (ret) {
        LOG_INFO(" can't select sardsi active\n");
        return -EINVAL;
    }
    else
        LOG_INFO(" select sardsi active success\n");

    return ret;
}

static int sardsi_remove(struct platform_device * pdev)
{
    LOG_INFO("Enter %s\n", __func__);
    return 0;
}

static struct platform_driver sardsi_driver = {
    .driver = {
        .name = "sardsi_detect",
        .owner = THIS_MODULE,
        .of_match_table = sardsi_of_match,
    },
    .probe  = sardsi_probe,
    .remove = sardsi_remove,
};

static irqreturn_t sardsi_irq_func(int irq, void * data)
{
    int ret = 0;
    struct sardsi_t * sardsi = NULL;
    unsigned long flags;

    LOG_INFO("Enter %s\n", __func__);

    sardsi = (struct sardsi_t *)data;

    spin_lock_irqsave(&sardsi->spinlock, flags);

    ret = gpio_get_value_cansleep(sardsi->gpiopin);
    if (1 == ret) {
        irq_set_irq_type(sardsi->irq, IRQ_TYPE_LEVEL_LOW);
        sardsi->curr_mode = SAR_DSI_NEAR;
        LOG_INFO("set the irq trigger type to low\n");
    }
    else {
        irq_set_irq_type(sardsi->irq, IRQ_TYPE_LEVEL_HIGH);
        sardsi->curr_mode = SAR_DSI_FAR;
        LOG_INFO("set the irq trigger type to high\n");
    }

    if (sardsi->curr_mode == SAR_DSI_NEAR) {
        input_report_key(sardsi->dsi_dev, KEY_SAR_SLOW_IN, 1);
        input_report_key(sardsi->dsi_dev, KEY_SAR_SLOW_IN, 0);
        LOG_INFO("Report sardsi status as enabled\n");
    } else {
        input_report_key(sardsi->dsi_dev, KEY_SAR_SLOW_OUT, 1);
        input_report_key(sardsi->dsi_dev, KEY_SAR_SLOW_OUT, 0);
        LOG_INFO("Report sardsi status as disabled\n");
    }
    sardsi_state = !!sardsi->curr_mode;
    input_sync(sardsi->dsi_dev);

    spin_unlock_irqrestore(&sardsi->spinlock, flags);
    return IRQ_HANDLED;
}

int sardsi_register_input_device(void)
{
    int ret = 0;
    struct input_dev * input = NULL;

    LOG_INFO("Enter %s\n", __func__);

    input = input_allocate_device();
    if (!input) {
        LOG_INFO("Can't allocate input device! \n");
        return -1;
    }

    /* init input device data*/
    input->name = "sardsi_irq";
    __set_bit(EV_KEY, input->evbit);
    __set_bit(KEY_SAR_SLOW_IN, input->keybit);
    __set_bit(KEY_SAR_SLOW_OUT, input->keybit);
    ret = input_register_device(input);
    if (ret) {
        LOG_INFO("Failed to register device\n");
        goto err_free_dev;
    }

    sardsi_data.dsi_dev = input;
    return 0;

err_free_dev:
    input_free_device(input);
    return ret;
}

static int __init sardsi_init(void) {
    struct device_node * node = NULL;
    int ret = 0;
    sardsi_state = 0;

    LOG_INFO("Enter %s\n", __func__);

    sardsi_data.curr_mode = SAR_DSI_FAR;

    /* pinctrl start */
    ret = platform_driver_register(&sardsi_driver);
    if (ret) {
        LOG_INFO("register sardsi driver failed\n");
        return ret;
    }

    /* input device start */
    ret = sardsi_register_input_device();
    if (ret) {
        LOG_INFO("register sardsi input failed\n");
        return ret;
    }
    /* input device end */

    node = of_find_matching_node(NULL, sardsi_of_match);
    if (NULL == node) {
        LOG_INFO("can't find compatible node\n");
        return -1;
    }
    sardsi_data.gpiopin = of_get_named_gpio(node, "qcom,gpio_irq", 0);
    if (!gpio_is_valid(sardsi_data.irq)) {
        LOG_INFO("TLMM gpio not found\n");
        return -1;
    }

    LOG_INFO("sardsi_data.gpiopin is %d\n", sardsi_data.gpiopin);

    gpio_direction_input(sardsi_data.gpiopin);
    sardsi_data.irq = gpio_to_irq(sardsi_data.gpiopin);
    if (sardsi_data.irq < 0) {
        LOG_INFO("Unable to configure irq\n");
        return -1;
    }

    spin_lock_init(&sardsi_data.spinlock);
    ret = request_threaded_irq(sardsi_data.irq, NULL, sardsi_irq_func, IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "sardsi_irq", &sardsi_data);
    if (!ret) {
        enable_irq_wake(sardsi_data.irq);
        LOG_INFO("enable irq wake\n");
    }

    return 0;
}

static void __exit sardsi_exit(void) {
    LOG_INFO("Enter %s\n", __func__);

    input_unregister_device(sardsi_data.dsi_dev);
    free_irq(sardsi_data.irq, sardsi_irq_func);
}

module_init(sardsi_init);
module_exit(sardsi_exit);
