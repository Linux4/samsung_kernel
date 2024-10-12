#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include "hallsensor.h"

#define hall_DEBUG 1
#define LOG_TAG "[hall] "

#if hall_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

#define HALL_COMPATIBLE_DEVICE_ID "sprd,hall_detect"

static bool hall_state;
module_param(hall_state, bool, 0644);

struct hall_t hall_data;
const struct of_device_id hall_of_match[] = {
    { .compatible = HALL_COMPATIBLE_DEVICE_ID, },
};

static struct platform_driver hall_driver = {
    .driver = {
        .name = "hall_detect",
        .owner = THIS_MODULE,
        .of_match_table = hall_of_match,
    },
};

static irqreturn_t hall_irq_func(int irq, void * data)
{
    int ret = 0;
    struct hall_t * hall = NULL;
    unsigned long flags;

    LOG_INFO("Enter %s\n", __func__);

    hall = (struct hall_t *)data;

    spin_lock_irqsave(&hall->spinlock, flags);

    ret = gpio_get_value_cansleep(hall->gpiopin);
    if (1 == ret) {
        irq_set_irq_type(hall->irq, IRQ_TYPE_LEVEL_LOW);
        hall->curr_mode = HALL_FAR;
        LOG_INFO("set the irq trigger type to low\n");
    }
    else {
        irq_set_irq_type(hall->irq, IRQ_TYPE_LEVEL_HIGH);
        hall->curr_mode = HALL_NEAR;
        LOG_INFO("set the irq trigger type to high\n");
    }
    /*Tab A8 code for AX6300DEV-58 by xiongxiaoliang at 2021/08/19 start*/
    if (hall->curr_mode == HALL_NEAR) {
        LOG_INFO("HALL_NEAR\n");
        #ifdef HQ_FACTORY_BUILD
        input_report_key(hall->dev, KEY_HALL_CLOSE, 1);
        input_report_key(hall->dev, KEY_HALL_CLOSE, 0);
        #else
        /*Tab A8 code for AX6300DEV-613 by mayuhang at 2021/9/18 start*/
        input_report_switch(hall->dev,SW_FLIP,1);
        /*Tab A8 code for AX6300DEV-613 by mayuhang at 2021/9/18 end*/
        #endif
        input_sync(hall->dev);
    } else {
        LOG_INFO("HALL_FAR\n");
        #ifdef HQ_FACTORY_BUILD
        input_report_key(hall->dev, KEY_HALL_FAR, 1);
        input_report_key(hall->dev, KEY_HALL_FAR, 0);
        #else
        /*Tab A8 code for AX6300DEV-613 by mayuhang at 2021/9/18 start*/
        input_report_switch(hall->dev,SW_FLIP,0);
        /*Tab A8 code for AX6300DEV-613 by mayuhang at 2021/9/18 end*/
        #endif
        input_sync(hall->dev);
    }

    hall_state = !!hall->curr_mode;
    /*Tab A8 code for AX6300DEV-58 by xiongxiaoliang at 2021/08/19 end*/

    spin_unlock_irqrestore(&hall->spinlock, flags);
    return IRQ_HANDLED;
}

int hall_register_input_device(void)
{
    int ret = 0;
    struct input_dev * input = NULL;

    LOG_INFO("Enter %s\n", __func__);

    input = input_allocate_device();
    if (!input) {
        LOG_INFO("Can't allocate input device! \n");
        return -1;
    }

    /*Tab A8 code for AX6300DEV-58 by xiongxiaoliang at 2021/08/19 start*/
    /* init input device data*/
    input->name = "hall_irq";
    #ifdef HQ_FACTORY_BUILD
    __set_bit(EV_KEY, input->evbit);
    __set_bit(KEY_HALL_CLOSE, input->keybit);
    __set_bit(KEY_HALL_FAR, input->keybit);
    #else
    /*Tab A8 code for AX6300DEV-613 by mayuhang at 2021/9/18 start*/
    input_set_capability(input, EV_SW, SW_FLIP);
    /*Tab A8 code for AX6300DEV-613 by mayuhang at 2021/9/18 end*/
    #endif
    /*Tab A8 code for AX6300DEV-58 by xiongxiaoliang at 2021/08/19 end*/
    ret = input_register_device(input);
    if (ret) {
        LOG_INFO("Failed to register device\n");
        goto err_free_dev;
    }

    hall_data.dev = input;
    return 0;

err_free_dev:
    input_free_device(input);
    return ret;
}

static int __init hall_init(void) {
    struct device_node * node = NULL;
    int ret = 0;
    enum of_gpio_flags flags;
    hall_state = 0;

    LOG_INFO("Enter %s\n", __func__);

    hall_data.curr_mode = HALL_FAR;

    /* pinctrl start */
    ret = platform_driver_register(&hall_driver);
    if (ret) {
        LOG_INFO("register hall driver failed\n");
        return ret;
    }

    /* input device start */
    ret = hall_register_input_device();
    if (ret) {
        LOG_INFO("register hall input failed\n");
        return ret;
    }
    /* input device end */

    node = of_find_matching_node(NULL, hall_of_match);
    if (NULL == node) {
        LOG_INFO("can't find compatible node\n");
        return -1;
    }
    hall_data.gpiopin = of_get_named_gpio_flags(node, "hall,nirq-gpio", 0, &flags);
    if (!gpio_is_valid(hall_data.gpiopin)) {
        LOG_INFO("TLMM gpio not found\n");
        return -1;
    }

    LOG_INFO("hall_data.gpiopin is %d\n", hall_data.gpiopin);

    ret = gpio_request(hall_data.gpiopin, "hall_irq_gpio");
    if (ret < 0)
    {
        LOG_INFO("hall Request gpio. Fail![%d]\n", ret);
        return ret;
    }

    gpio_direction_input(hall_data.gpiopin);
    hall_data.irq = gpio_to_irq(hall_data.gpiopin);
    if (hall_data.irq < 0) {
        LOG_INFO("Unable to configure irq\n");
        return -1;
    }

    spin_lock_init(&hall_data.spinlock);
    ret = request_threaded_irq(hall_data.irq, NULL, hall_irq_func, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "hall_irq", &hall_data);
    if (!ret) {
        enable_irq_wake(hall_data.irq);
        LOG_INFO("enable irq wake\n");
    }

    return 0;
}

static void __exit hall_exit(void) {
    LOG_INFO("Enter %s\n", __func__);

    input_unregister_device(hall_data.dev);
    free_irq(hall_data.irq, hall_irq_func);
}

module_init(hall_init);
module_exit(hall_exit);
