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

#include "rfcable_detect.h"

#include <linux/init.h>
#include <linux/module.h>

#include <linux/input.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/platform_device.h>
/*HS60 code for HS60-356 by zhuqiang at 2019/08/07 start*/
#include <soc/qcom/smem.h>
u32 sku_version = 0;
/*HS60 code for HS60-356 by zhuqiang at 2019/08/07 end*/

#define SWTP_COMPATIBLE_DEVICE_ID "qcom,rfcable_detect"
static bool swtp_state;
module_param(swtp_state,bool,0644);

struct swtp_t swtp_data;
const struct of_device_id swtp_of_match[] = {
	{ .compatible = SWTP_COMPATIBLE_DEVICE_ID, },
};
/*HS60 code for HS60-356 by zhuqiang at 2019/08/07 start*/
static int swtp_sku_conf(void)
{
	u32 *sku_addr = NULL;
	u32 sku_size = 0;

	sku_addr = (u32 *)smem_get_entry(SMEM_ID_VENDOR1, &sku_size, 0, 0);
	if (sku_addr)
	{
		sku_version = *sku_addr;
		pr_err("%s sku_size=%d, sku_version=0x%x\n",__FUNCTION__, sku_size, sku_version);
		return 0;
	}
	else
	{
		pr_err("%s reading the smem conf fail\n", __FUNCTION__);
		return 1;
	}
}
/*HS60 code for HS60-356 by zhuqiang at 2019/08/07 end*/
static int swtp_probe(struct platform_device * pdev)
{
	int ret = 0;
	struct pinctrl * swtp_pinctrl = NULL;
	struct pinctrl_state * state = NULL;

	swtp_pinctrl = devm_pinctrl_get(&pdev->dev);
	state = pinctrl_lookup_state(swtp_pinctrl, "swtp_irq_active");
	if (IS_ERR(state)) {
		pr_err(" can't find swtp_irq_active\n");
		return -EINVAL;
	}

	ret = pinctrl_select_state(swtp_pinctrl, state);
	if (ret) {
		pr_err(" can't select swtp_irq_active\n");
		return -EINVAL;
	}
	else pr_err(" select swtp_irq_active success\n");
	return ret;

}

static int swtp_remove(struct platform_device * pdev)
{
	return 0;
}

static struct platform_driver swtp_driver = {
	.driver = {
		.name = "swtp_pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = swtp_of_match,
	},
	.probe  = swtp_probe,
	.remove = swtp_remove,
};

static irqreturn_t swtp_irq_func(int irq, void * data)
{
	int ret = 0;
	struct swtp_t * swtp = NULL;
	unsigned long flags;

	swtp = (struct swtp_t *)data;

	spin_lock_irqsave(&swtp->spinlock, flags);
	pr_debug(" %s start\n", __func__);

	ret = gpio_get_value_cansleep(swtp->gpiopin);
	pr_err("value = %d, gpiopin = %d", ret, swtp->gpiopin);

	if (1 == ret) {
		irq_set_irq_type(swtp->irq, IRQ_TYPE_LEVEL_LOW);
		swtp->curr_mode = SWTP_INT_PIN_PLUG_IN;
	}
	else {
		irq_set_irq_type(swtp->irq, IRQ_TYPE_LEVEL_HIGH);
		swtp->curr_mode = SWTP_INT_PIN_PLUG_OUT;
	}

	if (swtp->curr_mode == SWTP_INT_PIN_PLUG_IN) {
		input_report_key(swtp->swtp_dev, KEY_SWTP_IN, 1);
		input_report_key(swtp->swtp_dev, KEY_SWTP_IN, 0);
	} else {
		input_report_key(swtp->swtp_dev, KEY_SWTP_OUT, 1);
		input_report_key(swtp->swtp_dev, KEY_SWTP_OUT, 0);
	}
	swtp_state = !!swtp->curr_mode;
	input_sync(swtp->swtp_dev);

	spin_unlock_irqrestore(&swtp->spinlock, flags);
	return IRQ_HANDLED;
}

int swtp_register_input_device(void)
{
	int ret = 0;
	struct input_dev * input = NULL;

	input = input_allocate_device();
	if (!input) {
		pr_err("%s Can't allocate input device! \n", __func__);
		return -1;
	}

	/* init input device data*/
	input->name = "swtp_irq";
	__set_bit(EV_KEY, input->evbit);
	__set_bit(KEY_SWTP_IN, input->keybit);
	__set_bit(KEY_SWTP_OUT, input->keybit);
	ret = input_register_device(input);
	if (ret) {
		pr_err(" %s : Failed to register device\n", __func__);
		goto err_free_dev;
	}

	swtp_data.swtp_dev = input;
	return 0;

err_free_dev:
	input_free_device(input);
	return ret;
}


static int __init swtp_init(void) {
	struct device_node * node = NULL;
	int ret = 0;
	/*HS60 code for HS60-356 by zhuqiang at 2019/08/07 start*/
	swtp_state = 0;
	ret = swtp_sku_conf();
	if (!!ret)
	{
		swtp_sku_conf();
	}
	if(sku_version < 0x40)
	{
		pr_err(" %s zql1695 don't use swtp\n", __func__);
		return 0;
	}
	/*HS60 code for HS60-356 by zhuqiang at 2019/08/07 end*/
	swtp_data.curr_mode = SWTP_INT_PIN_PLUG_OUT;

/* pinctrl start */
	ret = platform_driver_register(&swtp_driver);
	if (ret) {
		pr_err(" %s failed to register swtp pinctrl driver\n",
				__func__);
	} else {
		pr_debug(" %s register swtp pinctrl driver success\n", __func__);
	}
/* pinctrl end */

/* input device start */
	ret = swtp_register_input_device();
	if (ret) {
		return ret;
	}
/* input device end */

	pr_debug(" swtp_init start\n");
	node = of_find_matching_node(NULL, swtp_of_match);
	if (NULL == node) {
		pr_err("1 %s can't find compatible node\n", __func__);
		return -1;
	}
	swtp_data.gpiopin = of_get_named_gpio(node, "qcom,gpio_irq", 0);
	if (!gpio_is_valid(swtp_data.irq)) {
		pr_err(" TLMM gpio not found\n");
		return -1;
	}

	gpio_direction_input(swtp_data.gpiopin);
	swtp_data.irq = gpio_to_irq(swtp_data.gpiopin);
	if (swtp_data.irq < 0) {
		pr_err(" Unable to configure irq\n");
		return -1;
	}

	spin_lock_init(&swtp_data.spinlock);
	ret = request_threaded_irq(swtp_data.irq, NULL, swtp_irq_func, IRQF_TRIGGER_NONE | IRQF_ONESHOT, "swtp_irq", &swtp_data);
	if (!ret) {
		enable_irq_wake(swtp_data.irq);
		pr_debug(" enable irq wake\n");
	}

	return 0;
}

static void __exit swtp_exit(void) {
	input_unregister_device(swtp_data.swtp_dev);
	free_irq(swtp_data.irq, swtp_irq_func);
}

module_init(swtp_init);
module_exit(swtp_exit);
