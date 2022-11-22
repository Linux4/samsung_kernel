/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/sprd_eic_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#ifdef HOOK_POWER_KEY
#include <linux/input-hook.h>
#endif

//====================  debug  ====================
#define ENTER \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_EIC_KEYS_DBG] func: %s  line: %04d\n", __func__, __LINE__); }while(0)

#define PRINT_DBG(format,x...)  \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_EIC_KEYS_DBG] " format, ## x); }while(0)

#define PRINT_INFO(format,x...)  \
do{ printk(KERN_INFO "[SPRD_EIC_KEYS_INFO] " format, ## x); }while(0)

#define PRINT_WARN(format,x...)  \
do{ printk(KERN_INFO "[SPRD_EIC_KEYS_WARN] " format, ## x); }while(0)

#define PRINT_ERR(format,x...)  \
do{ printk(KERN_ERR "[SPRD_EIC_KEYS_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x); }while(0)
//====================  debug  ====================

static int debug_level = 1;
static struct sprd_eic_keys_platform_data *this_pdata = NULL;

static int sprd_eic_button_state(struct sprd_eic_keys_button *button)
{
        int button_state = 0;
        int gpio_value = 0;

        gpio_value = !!gpio_get_value(button->gpio);

        if(1 == button->active_low) {
                if(0 == gpio_value)
                        button_state = 1;
                else
                        button_state = 0;
        } else {
                if(1 == gpio_value)
                        button_state = 1;
                else
                        button_state = 0;
        }

        PRINT_DBG("GPIO_%d=%d, button_state=%d\n", button->gpio, gpio_value, button_state);

        return button_state; //0==released, 1==pressed
}

static irqreturn_t sprd_eic_irq_handle(int irq, void *dev_id)
{
        struct sprd_eic_keys_button *button = (struct sprd_eic_keys_button *)dev_id;
        static int button_state_last = 0;
        int button_state_current = 0;

        ENTER;
        disable_irq_nosync(button->irq);
        button_state_current = sprd_eic_button_state(button);

        if (1 == button_state_current) {//pressed
#ifdef HOOK_POWER_KEY
	#ifdef ( CONFIG_ARCH_SCX20 || CONFIG_ARCH_SCX35LT8 )
		if(116 == button->code)
	#endif
		input_report_key_hook(this_pdata->input_dev, button->code, button_state_current);
#endif
                input_report_key(this_pdata->input_dev, button->code, button_state_current);
                input_sync(this_pdata->input_dev);
                PRINT_INFO("Pressed!  Key:%s ScanCode:%d value:%d\n", button->desc, button->code, button_state_current);

                if(1 == button->active_low)
                        irq_set_irq_type(button->irq, IRQF_TRIGGER_HIGH);
                else
                        irq_set_irq_type(button->irq, IRQF_TRIGGER_LOW);

        } else {//released
#ifdef HOOK_POWER_KEY
	#ifdef ( CONFIG_ARCH_SCX20 || CONFIG_ARCH_SCX35LT8 )
		if(116 == button->code)
	#endif
                input_report_key_hook(this_pdata->input_dev, button->code, button_state_current);
#endif
                input_report_key(this_pdata->input_dev, button->code, button_state_current);
                input_sync(this_pdata->input_dev);
                PRINT_INFO("Released! Key:%s ScanCode:%d value:%d\n", button->desc, button->code, button_state_current);

                if(1 == button->active_low)
                        irq_set_irq_type(button->irq, IRQF_TRIGGER_LOW);
                else
                        irq_set_irq_type(button->irq, IRQF_TRIGGER_HIGH);

        }

        button_state_last = button_state_current;
        enable_irq(button->irq);
        return IRQ_HANDLED;
}

static int sprd_eic_keys_setup_key(struct sprd_eic_keys_button *button)
{
        const char *desc = button->desc ? button->desc : "sprd_eic_keys";
        unsigned long irqflags = 0;
        int error = -1;

        ENTER;
        if (gpio_is_valid(button->gpio)) {

                error = gpio_request_one(button->gpio, GPIOF_IN, desc);
                if (error < 0) {
                        PRINT_ERR("Failed to request GPIO %d, error %d\n", button->gpio, error);
                        return error;
                }

                if (button->debounce_interval) {
                        error = gpio_set_debounce(button->gpio, button->debounce_interval * 1000);
                        if (error < 0)
                                PRINT_INFO("Failed to set debounce for GPIO %d\n", button->gpio);
                }

                button->irq = gpio_to_irq(button->gpio);
                if (button->irq < 0) {
                        error = button->irq;
                        PRINT_ERR("Unable to get irq number for GPIO %d, error %d\n", button->gpio, error);
                        goto fail;
                }
        } else {
                PRINT_ERR("No GPIO specified\n");
                return -EINVAL;
        }

        if (button->type && button->type != EV_KEY) {
                PRINT_ERR("Only EV_KEY allowed for IRQ buttons\n");
                return -EINVAL;
        }

        input_set_capability(this_pdata->input_dev, button->type ?: EV_KEY, button->code);

        irqflags = (1 == button->active_low) ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH;
        irqflags |= IRQF_NO_SUSPEND;

        error = request_any_context_irq(button->irq, sprd_eic_irq_handle, irqflags, desc, button);
        if (error < 0) {
                PRINT_ERR("Unable to claim irq %d; error %d\n", button->irq, error);
                goto fail;
        }

        return 0;

fail:
        if (gpio_is_valid(button->gpio))
                gpio_free(button->gpio);

        return error;
}

#ifdef CONFIG_OF
static struct sprd_eic_keys_platform_data *sprd_eic_keys_get_devtree_pdata(struct device *dev)
{
        struct device_node *node, *pp;
        struct sprd_eic_keys_platform_data *pdata;
        struct sprd_eic_keys_button *button;
        int error;
        int nbuttons;
        int i;
	char *input_name = NULL;
	int ret = -1;

        node = dev->of_node;
        if (!node) {
                error = -ENODEV;
                goto err_out;
        }

        nbuttons = of_get_child_count(node);
        if (nbuttons == 0) {
                error = -ENODEV;
                goto err_out;
        }

        pdata = kzalloc(sizeof(*pdata) + nbuttons * (sizeof *button),
                        GFP_KERNEL);
        if (!pdata) {
                error = -ENOMEM;
                goto err_out;
        }

        pdata->buttons = (struct sprd_eic_keys_button *)(pdata + 1);
        pdata->nbuttons = nbuttons;

        pdata->rep = !!of_get_property(node, "autorepeat", NULL);

	ret = of_property_read_string(node, "input-name", &input_name);
	if (0 == ret) {
		pdata->name = input_name;
		PRINT_INFO("input-name=\"%s\"\n", input_name);
	}
	else
		PRINT_WARN("failed to get input-name\n");

        i = 0;
        for_each_child_of_node(node, pp) {
                int gpio;
                enum of_gpio_flags flags;

                if (!of_find_property(pp, "gpios", NULL)) {
                        pdata->nbuttons--;
                        dev_warn(dev, "Found button without gpios\n");
                        continue;
                }

                gpio = of_get_gpio_flags(pp, 0, &flags);
                if (gpio < 0) {
                        error = gpio;
                        if (error != -EPROBE_DEFER)
                                dev_err(dev,
                                        "Failed to get gpio flags, error: %d\n",
                                        error);
                        goto err_free_pdata;
                }

                button = &pdata->buttons[i++];

                button->gpio = gpio;
                button->active_low = flags & OF_GPIO_ACTIVE_LOW;

                if (of_property_read_u32(pp, "linux,code", &button->code)) {
                        dev_err(dev, "Button without keycode: 0x%x\n",
                                button->gpio);
                        error = -EINVAL;
                        goto err_free_pdata;
                }

                button->desc = of_get_property(pp, "label", NULL);

                if (of_property_read_u32(pp, "linux,input-type", &button->type))
                        button->type = EV_KEY;

                button->wakeup = !!of_get_property(pp, "gpio-key,wakeup", NULL);

                if (of_property_read_u32(pp, "debounce-interval",
                                         &button->debounce_interval))
                        button->debounce_interval = 5;
        }

        if (pdata->nbuttons == 0) {
                error = -EINVAL;
                goto err_free_pdata;
        }

        return pdata;

err_free_pdata:
        kfree(pdata);
err_out:
        return ERR_PTR(error);
}

static struct of_device_id sprd_eic_keys_of_match[] = {
        { .compatible = "sprd,sprd-eic-keys", },
        { },
};
MODULE_DEVICE_TABLE(of, sprd_eic_keys_of_match);

#else

static inline struct sprd_eic_keys_platform_data *sprd_eic_keys_get_devtree_pdata(struct device *dev)
{
        return ERR_PTR(-ENODEV);
}

#endif
static ssize_t sprd_eic_button_show(struct kobject *kobj, struct kobj_attribute *attr, char *buff)
{
        int status = 0;

        status = sprd_eic_button_state(&this_pdata->buttons[0]);
        PRINT_INFO("button status = %d (0 == released, 1 == pressed)\n", status);
        return sprintf(buff, "%d\n", status);
}

static struct kobject *sprd_eic_button_kobj = NULL;
static struct kobj_attribute sprd_eic_button_attr =
        __ATTR(status, 0644, sprd_eic_button_show, NULL);

static int sprd_eic_button_sysfs_init(void)
{
        int ret = -1;

        sprd_eic_button_kobj = kobject_create_and_add("sprd_eic_button", kernel_kobj);
        if (sprd_eic_button_kobj == NULL) {
                ret = -ENOMEM;
                PRINT_ERR("register sysfs failed. ret = %d\n", ret);
                return ret;
        }

        ret = sysfs_create_file(sprd_eic_button_kobj, &sprd_eic_button_attr.attr);
        if (ret) {
                PRINT_ERR("create sysfs failed. ret = %d\n", ret);
                return ret;
        }

        PRINT_INFO("sprd_eic_button_sysfs_init success\n");
        return ret;
}
static int sprd_eic_keys_probe(struct platform_device *pdev)
{
        struct device *dev = &pdev->dev;
        struct sprd_eic_keys_button *button = NULL;
        struct input_dev *input = NULL;
        int i = 0;
        int error = -1;

        ENTER;
        this_pdata = dev_get_platdata(dev);

        if (!this_pdata) {
                this_pdata = sprd_eic_keys_get_devtree_pdata(dev);
                if (IS_ERR(this_pdata)) {
                        PRINT_ERR("failed to get platform data!\n");
                        return PTR_ERR(this_pdata);
                }
        }

        input = input_allocate_device();
        if (!input) {
                PRINT_ERR("failed to allocate input device!\n");
                error = -ENOMEM;
                goto fail1;
        }

        input->name = this_pdata->name ? this_pdata->name : pdev->name;
        input->id.bustype = BUS_HOST;
        this_pdata->input_dev = input;

        if (this_pdata->rep)
                __set_bit(EV_REP, input->evbit);

        for (i = 0; i < this_pdata->nbuttons; i++) {
                button = &this_pdata->buttons[i];
                error = sprd_eic_keys_setup_key( button);
                if (error)
                        goto fail2;
        }

        error = input_register_device(input);
        if (error) {
                PRINT_ERR("Unable to register input device!\n");
                goto fail3;
        }
	sprd_eic_button_sysfs_init();
        PRINT_INFO("probe success!\n");
        return 0;

fail3:
fail2:
        while (--i >= 0) {
                button = &this_pdata->buttons[i];
                free_irq(button->irq, button);
                if (gpio_is_valid(button->gpio)) {
                        gpio_free(button->gpio);
                }
        }

        input_free_device(input);
fail1:
        PRINT_ERR("probe failed!\n");
        return error;
}

#ifdef CONFIG_PM
static int sprd_eic_keys_suspend(struct platform_device *dev, pm_message_t state)
{
        return 0;
}

static int sprd_eic_keys_resume(struct platform_device *dev)
{
        return 0;
}
#else
#define sprd_eic_keys_suspend NULL
#define sprd_eic_keys_resume NULL
#endif

static struct platform_driver sprd_eic_keys_device_driver = {
        .probe = sprd_eic_keys_probe,
        .suspend = NULL,
        .resume = NULL,
        .driver = {
                .name = "sprd-eic-keys",
                .owner = THIS_MODULE,
                .of_match_table = of_match_ptr(sprd_eic_keys_of_match),
        }
};

static int __init sprd_eic_keys_init(void)
{
        ENTER;
#ifdef HOOK_POWER_KEY
        input_hook_init();
#endif

/***************************************************************/
        struct device_node *node = of_find_compatible_node(NULL, NULL, "sprd,sprd-eic-keys");
        if(node) {
                PRINT_INFO("Find the node name:%s\n",node->name);

                struct platform_device *dev = of_find_device_by_node(node);
                if(dev)
                        PRINT_INFO("Find the device name:%s\n",dev->name);
                else
                        PRINT_INFO("Not Find the device\n");
        }
        else
                PRINT_INFO("Not Find the node\n");
/***************************************************************/

        return platform_driver_register(&sprd_eic_keys_device_driver);
}

static void __exit sprd_eic_keys_exit(void)
{
#ifdef HOOK_POWER_KEY
        input_hook_exit();
#endif
        platform_driver_unregister(&sprd_eic_keys_device_driver);
}

late_initcall(sprd_eic_keys_init);
module_exit(sprd_eic_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yaochuan Li <yaochuan.li@spreadtrum.com>");
MODULE_DESCRIPTION("Keyboard driver for spreadtrum eic keys");
MODULE_ALIAS("platform:sprd-eic-keys");
