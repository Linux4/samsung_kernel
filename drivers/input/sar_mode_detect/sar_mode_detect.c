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

#include <soc/qcom/smem.h>
u32 sku_version = 0;


#define SARDSI_COMPATIBLE_DEVICE_ID "qcom,sar_mode_detect"
static bool sardsi_state;
module_param(sardsi_state,bool,0644);

struct sardsi_t sardsi_data;
const struct of_device_id sardsi_of_match[] = {
    { .compatible = SARDSI_COMPATIBLE_DEVICE_ID, },
};

static void sardsi_sku_conf(void)
{
    u32 *sku_addr = NULL;
    u32 sku_size = 0;

    sku_addr = (u32 *)smem_get_entry(SMEM_ID_VENDOR1, &sku_size, 0, 0);
    if (sku_addr){
        sku_version = (*sku_addr & 0x00000ff0) >> 4;
        pr_err("%s sku_version=%d\n",__FUNCTION__, sku_version);
    }
    else{
        pr_err("%s reading the smem conf fail\n", __FUNCTION__);
    }
}

static int sardsi_probe(struct platform_device * pdev)
{
    int ret = 0;
    struct pinctrl * sardsi_pinctrl = NULL;
    struct pinctrl_state * state = NULL;

    sardsi_pinctrl = devm_pinctrl_get(&pdev->dev);
    state = pinctrl_lookup_state(sardsi_pinctrl, "sardsi_irq_active");
    if (IS_ERR(state)) {
        pr_err(" can't find sardsi_irq_active\n");
        return -EINVAL;
    }

    ret = pinctrl_select_state(sardsi_pinctrl, state);
    if (ret) {
        pr_err(" can't select sardsi_irq_active\n");
        return -EINVAL;
    }
    else
        pr_err(" select sardsi_irq_active success\n");

    return ret;
}

static int sardsi_remove(struct platform_device * pdev)
{
    pr_err(" %s .\n", __func__);
    return 0;
}

static struct platform_driver sardsi_driver = {
    .driver = {
        .name = "sardsi_pinctrl",
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

    sardsi = (struct sardsi_t *)data;

    spin_lock_irqsave(&sardsi->spinlock, flags);
    ret = gpio_get_value_cansleep(sardsi->gpiopin);
    pr_err("value = %d, gpiopin = %d", ret, sardsi->gpiopin);

    if (1 == ret) {
        irq_set_irq_type(sardsi->irq, IRQ_TYPE_LEVEL_LOW);
        sardsi->curr_mode = SAR_DSI_NEAR;
    }
    else {
        irq_set_irq_type(sardsi->irq, IRQ_TYPE_LEVEL_HIGH);
        sardsi->curr_mode = SAR_DSI_FAR;
    }

    if (sardsi->curr_mode == SAR_DSI_NEAR) {
        input_report_key(sardsi->dsi_dev, KEY_SAR_SLOW_IN, 1);
        input_report_key(sardsi->dsi_dev, KEY_SAR_SLOW_IN, 0);
    } else {
        input_report_key(sardsi->dsi_dev, KEY_SAR_SLOW_OUT, 1);
        input_report_key(sardsi->dsi_dev, KEY_SAR_SLOW_OUT, 0);
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

    input = input_allocate_device();
    if (!input) {
        pr_err("%s Can't allocate input device! \n", __func__);
        return -1;
    }

    /* init input device data*/
    input->name = "sardsi_irq";
    __set_bit(EV_KEY, input->evbit);
    __set_bit(KEY_SAR_SLOW_IN, input->keybit);
    __set_bit(KEY_SAR_SLOW_OUT, input->keybit);
    ret = input_register_device(input);
    if (ret) {
        pr_err(" %s : Failed to register device\n", __func__);
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

    sardsi_sku_conf();
    /*HS50 code for HS50NA-330 by xiongxiaoliang at 20201203 start*/
    if(!(((sku_version >= 0x26) && (sku_version<= 0x2A)) || (sku_version == 0x2E) || (sku_version == 0x2F))){
    /*HS50 code for HS50NA-330 by xiongxiaoliang at 20201203 end*/
        pr_err(" %s globle don't sar sensor\n", __func__);
        return -ENODEV;
    }

    pr_err(" %s start!!\n", __func__);

    sardsi_data.curr_mode = SAR_DSI_FAR;

    /* pinctrl start */
    ret = platform_driver_register(&sardsi_driver);

    if (ret) {
        pr_err(" %s failed to register sardsi pinctrl driver\n",
                __func__);
    } else {
        pr_err(" %s register sardsi pinctrl driver success\n", __func__);
    }
    /* pinctrl end */

    /* input device start */
    ret = sardsi_register_input_device();
    if (ret) {
        return ret;
    }
    /* input device end */

    node = of_find_matching_node(NULL, sardsi_of_match);
    if (NULL == node) {
        pr_err("1 %s can't find compatible node\n", __func__);
        return -1;
    }
    sardsi_data.gpiopin = of_get_named_gpio(node, "qcom,gpio_irq", 0);
    if (!gpio_is_valid(sardsi_data.irq)) {
        pr_err(" TLMM gpio not found\n");
        return -1;
    }

    gpio_direction_input(sardsi_data.gpiopin);
    sardsi_data.irq = gpio_to_irq(sardsi_data.gpiopin);
    if (sardsi_data.irq < 0) {
        pr_err(" Unable to configure irq\n");
        return -1;
    }

    spin_lock_init(&sardsi_data.spinlock);
    ret = request_threaded_irq(sardsi_data.irq, NULL, sardsi_irq_func, IRQF_TRIGGER_NONE | IRQF_ONESHOT, "sardsi_irq", &sardsi_data);
    if (!ret) {
        enable_irq_wake(sardsi_data.irq);
        pr_err(" enable irq wake\n");
    }

    return 0;
}

static void __exit sardsi_exit(void) {
    pr_err(" %s .\n", __func__);
    input_unregister_device(sardsi_data.dsi_dev);
    free_irq(sardsi_data.irq, sardsi_irq_func);
}

module_init(sardsi_init);
module_exit(sardsi_exit);
