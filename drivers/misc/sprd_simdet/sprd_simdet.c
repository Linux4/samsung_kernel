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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#endif
#include <mach/hardware.h>
#include <linux/sprd_simdet.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <linux/workqueue.h>
#include <mach/gpio.h>
#include <linux/wakelock.h>

#define ENTER \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_SIMDET_DBG] func: %s  line: %04d\n", __func__, __LINE__); }while(0)

#define PRINT_DBG(format,x...)  \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_SIMDET_DBG] " format, ## x); }while(0)

#define PRINT_INFO(format,x...)  \
do{ printk(KERN_INFO "[SPRD_SIMDET_INFO] " format, ## x); }while(0)

#define PRINT_WARN(format,x...)  \
do{ printk(KERN_INFO "[SPRD_SIMDET_WARN] " format, ## x); }while(0)

#define PRINT_ERR(format,x...)  \
do{ printk(KERN_ERR "[SPRD_SIMDET_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x); }while(0)


static DEFINE_SPINLOCK(irq_sim_detect_lock);

static int debug_level = 7;
static int sim_gpio_detect_value_last = 0;
static int sim_plug_state_last = 0; //if the hardware detected the sim card is plug in, set plug_state_last = 1
static struct wake_lock sim_detect_wakelock;
static struct platform_device *this_pdev = NULL;
static struct semaphore simdet_sem;
static struct sprd_simdet simdet = {
        .sdev = {
                .name = "sprd_simdet",
        },
};

static int sim_detect_irq_set_irq_type(unsigned int irq, unsigned int type)
{
        struct sprd_simdet *sprd_sd = &simdet;
        struct irq_desc *irq_desc = NULL;
        unsigned int irq_flags = 0;
        int ret = -1;

        ret = irq_set_irq_type(irq, type);
        irq_desc = irq_to_desc(irq);
        irq_flags = irq_desc->action->flags;

        if (irq == sprd_sd->irq_detect) {
                if(IRQF_TRIGGER_HIGH == type) {
                        PRINT_INFO("sim detect high is set for irq_detect(%d). irq_flags = 0x%08X, ret = %d\n",
                                  sprd_sd->irq_detect, irq_flags, ret);
                } else if(IRQF_TRIGGER_LOW == type) {
                        PRINT_INFO("sim detect low is set for irq_detect(%d). irq_flags = 0x%08X, ret = %d\n",
                                  sprd_sd->irq_detect, irq_flags, ret);
                }
        }

        return 0;
}

static void sim_detect_irq_detect_enable(int enable, unsigned int irq)
{
        unsigned long spin_lock_flags;
        static int current_irq_state = 1;//irq is enabled after request_irq()
        spin_lock_irqsave(&irq_sim_detect_lock, spin_lock_flags);
        if (1 == enable) {
                if (0 == current_irq_state) {
                        enable_irq(irq);
                        current_irq_state = 1;
                }
        } else {
                if (1 == current_irq_state) {
                        disable_irq_nosync(irq);
                        current_irq_state = 0;
                }
        }
        spin_unlock_irqrestore(&irq_sim_detect_lock, spin_lock_flags);
        return;
}

static irqreturn_t sim_detect_irq_handler(int irq, void *dev)
{
        struct sprd_simdet *sprd_sd = dev;

        sim_detect_irq_detect_enable(0, sprd_sd->irq_detect);
        wake_lock_timeout(&sim_detect_wakelock, msecs_to_jiffies(2000));
        sim_gpio_detect_value_last = gpio_get_value(sprd_sd->platform_data->gpio_detect);
        PRINT_DBG("sim_detect_irq_handler: IRQ_%d(GPIO_%d) = %d\n",
                  sprd_sd->irq_detect, sprd_sd->platform_data->gpio_detect, sim_gpio_detect_value_last);
        queue_delayed_work(sprd_sd->detect_work_queue, &sprd_sd->work_detect, msecs_to_jiffies(0));

        return IRQ_HANDLED;
}

static void sim_detect_work_func(struct work_struct *work)
{
        struct sprd_simdet *sprd_sd = &simdet;
        struct sprd_simdet_platform_data *pdata = sprd_sd->platform_data;
        int plug_state_current = 0;
        int gpio_detect_value_current = 0;

        down(&simdet_sem);

        ENTER;

        if(0 == sim_plug_state_last) {
                gpio_detect_value_current = gpio_get_value(pdata->gpio_detect);
                PRINT_INFO("gpio_detect_value_current = %d, gpio_detect_value_last = %d, plug_state_last = %d\n",
                           gpio_detect_value_current, sim_gpio_detect_value_last, sim_plug_state_last);

                if(gpio_detect_value_current != sim_gpio_detect_value_last) {
                        PRINT_INFO("software debance (step 1)!!!(sim_detect_work_func)\n");
                        goto out;
                }

                if(1 == pdata->irq_trigger_level_detect) {
                        if(1 == gpio_detect_value_current)
                                plug_state_current = 1;
                        else
                                plug_state_current = 0;
                } else {
                        if(0 == gpio_detect_value_current)
                                plug_state_current = 1;
                        else
                                plug_state_current = 0;
                }
        } else
                plug_state_current = 0;//no debounce for plug out!!!

        if(1 == plug_state_current && 0 == sim_plug_state_last) {

                sprd_sd->status = plug_state_current;
                switch_set_state(&sprd_sd->sdev, sprd_sd->status);
                sysfs_notify(&sprd_sd->sdev.dev->kobj, NULL, "state");
                sim_plug_state_last = 1;

                if(1 == pdata->irq_trigger_level_detect)
                        sim_detect_irq_set_irq_type(sprd_sd->irq_detect, IRQF_TRIGGER_LOW);
                else
                        sim_detect_irq_set_irq_type(sprd_sd->irq_detect, IRQF_TRIGGER_HIGH);

                sim_detect_irq_detect_enable(1, sprd_sd->irq_detect);

        }
        else if(0 == plug_state_current && 1 == sim_plug_state_last) {

                sprd_sd->status = plug_state_current;
                switch_set_state(&sprd_sd->sdev, sprd_sd->status);
                sysfs_notify(&sprd_sd->sdev.dev->kobj, NULL, "state"); 
                sim_plug_state_last = 0;

                if(1 == pdata->irq_trigger_level_detect)
                        sim_detect_irq_set_irq_type(sprd_sd->irq_detect, IRQF_TRIGGER_HIGH);
                else
                        sim_detect_irq_set_irq_type(sprd_sd->irq_detect, IRQF_TRIGGER_LOW);

                sim_detect_irq_detect_enable(1, sprd_sd->irq_detect);
        }
        else {
                PRINT_INFO("irq_detect must be enabled anyway!!!\n");
                goto out;
        }

out:

        sim_detect_irq_detect_enable(1, sprd_sd->irq_detect);
        up(&simdet_sem);
        return;
}

#ifdef CONFIG_OF
static struct sprd_simdet_platform_data *simdet_detect_parse_dt(struct device *dev)
{
        struct sprd_simdet_platform_data *pdata;
        struct device_node *np = dev->of_node;
        int ret;

        pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
        if (!pdata) {
                PRINT_ERR("could not allocate memory for platform data\n");
                return NULL;
        }

        pdata->gpio_detect = of_get_gpio(np, 0);
        PRINT_INFO("pdata->gpio_detect=%d\n", pdata->gpio_detect);

        ret = of_property_read_u32(np, "irq_trigger_level_detect", &pdata->irq_trigger_level_detect);
        if(ret) {
                PRINT_ERR("fail to get irq_trigger_level_detect\n");
                goto fail;
        }
        ret = of_property_read_u32(np, "delay_time", &pdata->delay_time);
        if(ret) {
                PRINT_ERR("fail to get delay_time. \n");
                goto fail;
        }

        return pdata;

fail:
        kfree(pdata);
        return NULL;
}
#endif

static int sprd_simdet_probe(struct platform_device *pdev)
{
        struct sprd_simdet_platform_data *pdata = pdev->dev.platform_data;
        struct sprd_simdet *sprd_sd = &simdet;
        unsigned long irqflags = 0;
        int ret = -1;

        PRINT_INFO("sprd_simdet_probe begin\n");

#ifdef CONFIG_OF
        struct device_node *np = NULL;
        np = pdev->dev.of_node;

        if (pdev->dev.of_node && !pdata) {
                pdata = simdet_detect_parse_dt(&pdev->dev);
                if(pdata)
                        pdev->dev.platform_data = pdata;
        }
        if (!pdata) {
                PRINT_ERR("sim_detect_probe get platform_data NULL\n");
                ret = -EINVAL;
                goto fail_to_get_platform_data;
        }
#endif

        this_pdev = pdev;

        sprd_sd->platform_data = pdata;

        ret = gpio_request(pdata->gpio_detect, "sim_detect");
        if (ret < 0) {
                PRINT_ERR("failed to request sim detect GPIO %d.\n", pdata->gpio_detect);
                goto failed_to_request_gpio_detect;
        }
        gpio_direction_input(pdata->gpio_detect);
        sprd_sd->irq_detect = gpio_to_irq(pdata->gpio_detect);

        ret = switch_dev_register(&sprd_sd->sdev);
        if (ret < 0) {
                PRINT_ERR("switch_dev_register failed!\n");
                goto failed_to_register_switch_dev;
        }

        sema_init(&simdet_sem, 1);

        INIT_DELAYED_WORK(&sprd_sd->work_detect, sim_detect_work_func);
        sprd_sd->detect_work_queue = create_singlethread_workqueue("sim_detect");
        if(sprd_sd->detect_work_queue == NULL) {
                PRINT_ERR("create_singlethread_workqueue for sim_detect failed!\n");
                goto failed_to_create_singlethread_workqueue_for_sim_detect;
        }

        wake_lock_init(&sim_detect_wakelock, WAKE_LOCK_SUSPEND, "sim_detect_wakelock");

        irqflags = pdata->irq_trigger_level_detect ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
        ret = request_irq(sprd_sd->irq_detect, sim_detect_irq_handler, irqflags | IRQF_NO_SUSPEND, "sim_detect", sprd_sd);
        if (ret < 0) {
                PRINT_ERR("failed to request IRQ_%d(GPIO_%d)\n", sprd_sd->irq_detect, pdata->gpio_detect);
                goto failed_to_request_irq_sim_detect;
        }

        PRINT_INFO("sim_detect_probe success\n");

        return ret;

failed_to_request_irq_sim_detect:
        destroy_workqueue(sprd_sd->detect_work_queue);
failed_to_create_singlethread_workqueue_for_sim_detect:
        switch_dev_unregister(&sprd_sd->sdev);
failed_to_register_switch_dev:
        gpio_free(pdata->gpio_detect);
failed_to_request_gpio_detect:
fail_to_get_platform_data:
        PRINT_ERR("sim_detect_probe failed\n");

        return ret;
}

#ifdef CONFIG_PM
static int sprd_simdet_suspend(struct platform_device *dev, pm_message_t state)
{
        PRINT_INFO("suspend det_irq = %d.\n", simdet.irq_detect);
        return 0;
}

static int sprd_simdet_resume(struct platform_device *dev)
{
        PRINT_INFO("resume det_irq = %d.\n", simdet.irq_detect);

        return 0;
}
#else
#define sprd_simdet_suspend NULL
#define sprd_simdet_resume NULL
#endif

static const struct of_device_id sprd_simdet_match_table[] = {
	{ .compatible = "sprd,sim_detect", },
	{},
};

static struct platform_driver sprd_simdet_driver = {
        .probe    = sprd_simdet_probe,
        .suspend = sprd_simdet_suspend,
        .resume = sprd_simdet_resume,
        .driver   = {
            .owner = THIS_MODULE,
            .name = "sprd_simdet",
            .of_match_table = sprd_simdet_match_table,
        },
};

static int __init sprd_simdet_init(void)
{
	if (platform_driver_register(&sprd_simdet_driver) != 0) {
		PRINT_ERR("sprd_simdet platform drv register Failed\n");
		return -1;
	}
	return 0;
}

static void __exit sprd_simdet_exit(void)
{
	platform_driver_unregister(&sprd_simdet_driver);
}

module_init(sprd_simdet_init);
module_exit(sprd_simdet_exit);


MODULE_DESCRIPTION("SPRD Communication Processor Driver");
MODULE_AUTHOR("haiwu chen <haiwu.chen@spreadtrum.com>");
MODULE_LICENSE("GPL");
