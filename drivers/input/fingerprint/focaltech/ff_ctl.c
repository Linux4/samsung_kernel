/**
 * The device control driver for FocalTech's fingerprint sensor.
 *
 * Copyright (C) 2016-2017 FocalTech Systems Co., Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
**/

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/bug.h>
#include <linux/types.h>
#include <linux/param.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/stddef.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
//#include <linux/regulator/consumer.h>


#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "ff_log.h"
#include "ff_ctl.h"

//#include <linux/cpu.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>

/*
 * Define the driver version string.
 */
#define FF_DRV_VERSION "v2.1.1"

/*
 * Define the driver name.
 */
#define FF_DRV_NAME "focaltech_fp"

int irq_gpio;

#ifdef USE_ONE_BINARY
extern int lge_get_fingerprint_hwinfo(void);
#endif

#define FF_NOTIFY 1

#ifdef USE_FP_ID_DUAL
static int g_fp_id;

static int get_fp_id_from_gpio(void);
#endif // USE_FP_ID_DUAL

/* Define pinctrl state types. */
typedef enum {
    FF_PINCTRL_STATE_PWR_ON,
	FF_PINCTRL_STATE_PWR_OFF,
    FF_PINCTRL_STATE_RST_ACT,
    FF_PINCTRL_STATE_RST_CLR,
    FF_PINCTRL_STATE_INT_ACT,
    FF_PINCTRL_STATE_PINS_DEFAULT,
    FF_PINCTRL_STATE_MAXIMUM /* Array size */
} ff_pinctrl_state_t;

/* Define pinctrl state names. */
static const char *g_pinctrl_state_names[FF_PINCTRL_STATE_MAXIMUM] = {
	"ldo-en", "ldo-disable", "reset-low", "reset-high" ,"irq-init", "spi-default",
};

/*
 * Driver context definition and its singleton instance.
 */
typedef struct {
	struct miscdevice miscdev;
	int irq_num;
	struct work_struct work_queue;
	struct fasync_struct *async_queue;
	struct wakeup_source *ttw_wl;
	struct input_dev *input;
#if FF_NOTIFY
	struct notifier_block fb_notifier;
#endif
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[FF_PINCTRL_STATE_MAXIMUM];
#if !defined(CONFIG_MTK_CLKMGR)
	struct clk *spiclk;
#endif
	//struct regulator *ff_vdd;
	struct spi_device *ff_spi;

	bool b_spiclk_enabled;

	bool b_driver_inited;
	bool b_config_dirtied;
} ff_ctl_context_t;
static ff_ctl_context_t *g_context = NULL;

/*
 * Driver configuration.
 */
static ff_driver_config_t driver_config;
ff_driver_config_t *ff_g_config = NULL;

struct mt_spi_t *ff_mt_spi = NULL;
//struct pinctrl *ff_pinctrl = NULL;

struct workqueue_struct* ff_core_queue = NULL;
//struct work_struct ff_core_work;

/*
 * Register/Unregister the spidev device for REE-Emulation.
 * See ff_spi.c for more details.
 */
extern int  ff_spi_register_device(int bus, int cs);
extern void ff_spi_unregister_device(void);

int ff_ctl_enable_power(bool on);

extern void mt_spi_enable_master_clk(struct spi_device *ms);
extern void mt_spi_disable_master_clk(struct spi_device *ms);
////////////////////////////////////////////////////////////////////////////////
/// Logging driver to logcat through uevent mechanism.

# undef LOG_TAG
#define LOG_TAG "ff_ctl"


#define FF_COMPATIBLE_NODE_1 "mediatek,finger-focaltech"
#define SUPPLY_3V3               3300000
/*
 * Log level can be runtime configurable by 'FF_IOC_SYNC_CONFIG'.
 */
ff_log_level_t g_log_level = __FF_EARLY_LOG_LEVEL;

int ff_log_printf(ff_log_level_t level, const char *tag, const char *fmt, ...)
{
	/* Prepare the storage. */
	va_list ap;
	static char uevent_env_buf[128];
	char *ptr = uevent_env_buf;
	int n, available = sizeof(uevent_env_buf);

	/* Fill logging level. */
	available -= sprintf(uevent_env_buf, "FF_LOG=%1d", level);
	ptr += strlen(uevent_env_buf);

	/* Fill logging message. */
	va_start(ap, fmt);
	vsnprintf(ptr, available, fmt, ap);
	va_end(ap);

	/* Send to ff_device. */
	if (likely(g_context) && likely(ff_g_config) && unlikely(ff_g_config->logcat_driver)) {
		char *uevent_env[2] = {uevent_env_buf, NULL};
		kobject_uevent_env(&g_context->miscdev.this_device->kobj, KOBJ_CHANGE, uevent_env);
	}

	/* Native output. */
	switch (level) {
	case FF_LOG_LEVEL_ERR:
		n = printk(KERN_ERR FF_DRV_NAME": %s\n", ptr);
		break;
	case FF_LOG_LEVEL_WRN:
		n = printk(KERN_WARNING FF_DRV_NAME": %s\n", ptr);
		break;
	case FF_LOG_LEVEL_INF:
		n = printk(KERN_INFO FF_DRV_NAME": %s\n", ptr);
		break;
	case FF_LOG_LEVEL_DBG:
	case FF_LOG_LEVEL_VBS:
	default:
		n = printk(KERN_DEBUG FF_DRV_NAME": %s\n", ptr);
		break;
	}
	return n;
}

////////////////////////////////////////////////////////////////////////////////


int ff_ctl_init_pins(int *irq_num)
{
	int err = 0, i;
	struct device_node *dev_node = NULL;
	struct platform_device *pdev = NULL;
	struct device_node *dev_node1 = NULL;
	struct platform_device *pdev1 = NULL;
	FF_LOGV("'%s' enter.", __func__);

	/* Find device tree node. */
	dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE_1);
	if (!dev_node) {
		FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE_1);
		return (-ENODEV);
	}
	FF_LOGV("of_get_named_gpio  %d.", irq_gpio);
	/* Convert to platform device */
	pdev = of_find_device_by_node(dev_node);
	if (!pdev) {
		FF_LOGE("of_find_device_by_node(..) failed.");
		return (-ENODEV);
	} else {
		u32 frequency, div;
		err = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &frequency);
		err = of_property_read_u32(pdev->dev.of_node, "clock-div", &div);
		FF_LOGI("spi controller(#%d) name: %s", pdev->id, pdev->name);
		FF_LOGI("spi controller(#%d) frequency = %d, div = %d", pdev->id, frequency, div);
	}


	//g_context->pinctrl = ff_pinctrl;
	/* Retrieve the pinctrl handler. */
	g_context->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!g_context->pinctrl) {
		FF_LOGE("devm_pinctrl_get(..) failed.");
		return (-ENODEV);
	}

	/* Register all pins. */

	for (i = 0; i < FF_PINCTRL_STATE_MAXIMUM; ++i) {
		g_context->pin_states[i] = pinctrl_lookup_state(g_context->pinctrl, g_pinctrl_state_names[i]);
		if (!g_context->pin_states[i]) {
			FF_LOGE("can't find pinctrl state for '%s'.", g_pinctrl_state_names[i]);
			err = (-ENODEV);
			break;
		}
	}

	if (i < FF_PINCTRL_STATE_MAXIMUM) {
		return (-ENODEV);
	}

	/* note: do not modify */
	/* reset low ==> 1ms ==> vdd on ==> 10ms ==> reset high */
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_ACT]);
	usleep_range(1000, 1000);
	//err = regulator_enable(g_context->ff_vdd);
	//if (err < 0) {
	//	FF_LOGE("Can't enable voltage\n");
	//}
    ff_ctl_enable_power(1);
	usleep_range(10000, 10000);
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_CLR]);

	/* Initialize the INT pin. */
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_INT_ACT]);


	/* Retrieve the irq number. */
	/*dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE_1);
	if (!dev_node) {
		FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE_1);
		return (-ENODEV);
	}*/

	/* Find device tree node. */
	dev_node1 = of_find_compatible_node(NULL, NULL, "mediatek,focaltech_fp_eint");
	if (!dev_node1) {
		FF_LOGE("of_find_compatible_node(.., '%s') failed.", "mediatek,focaltech_fp_eint");
		return (-ENODEV);
	}

	/* Convert to platform device */
	pdev1 = of_find_device_by_node(dev_node1);
	if (!pdev1) {
		FF_LOGE("of_find_device_by_node(..) failed.");
		return (-ENODEV);
	}

	irq_gpio = of_get_named_gpio(dev_node1, "int-gpios", 0);
		FF_LOGV("of_get_named_gpio  %d.", irq_gpio);
	err = gpio_request(irq_gpio, "focal,gpio_irq");
	if (err) {
		FF_LOGV("gpio_request  %d.", err);

	}

	*irq_num = gpio_to_irq(irq_gpio);

	// *irq_num = irq_of_parse_and_map(dev_node, 0);
	FF_LOGV("irq number is %d.", *irq_num);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_free_pins(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);
#if 0
	if (g_context->ff_vdd != NULL)
	{
		regulator_put(g_context->ff_vdd);
		g_context->ff_vdd = NULL;
	}
#endif
	if (g_context->pinctrl != NULL )
	{
        pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_PWR_OFF]);
		devm_pinctrl_put(g_context->pinctrl);
		g_context->pinctrl = NULL;
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_enable_spiclk(bool on)
{
	int err = 0;

	FF_LOGV("'%s' enter.", __func__);
	FF_LOGV("clock: '%s'.", on ? "enable" : "disabled");

	/* Control the clock source. */
	if (on == g_context->b_spiclk_enabled) {
		FF_LOGE("'%s' skip duplicated spiclk %d", __func__, on);
		return err;
	}

	if (on) {
		mt_spi_enable_master_clk(g_context->ff_spi);
		g_context->b_spiclk_enabled = true;
		FF_LOGV("'%s' enable_master_clk %d", __func__, on);
	} else {
		mt_spi_disable_master_clk(g_context->ff_spi);
		g_context->b_spiclk_enabled = false;
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_enable_power(bool on)
{
	int err = 0;
	FF_LOGV("'%s' enter. %d", __func__,on);


	if(on)
	{
        if((g_context->pinctrl == NULL) || (g_context->pin_states[FF_PINCTRL_STATE_PWR_ON] == NULL))
	    {
		    FF_LOGE("'%s' not get pinctrl.", __func__);
		    return -1;
	    }
		err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_PWR_ON]);
	}else{
        if((g_context->pinctrl == NULL) || (g_context->pin_states[FF_PINCTRL_STATE_PWR_OFF] == NULL))
	    {
		    FF_LOGE("'%s' not get pinctrl.", __func__);
		    return -1;
	    }
		err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_PWR_OFF]);
	}
	FF_LOGD("power is (%s) .", on ? "on" : "off");
	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_reset_device(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	if (unlikely(!g_context->pinctrl)) {
		return (-ENOSYS);
	}

	/* 3-1: Pull down RST pin. */
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_ACT]);

	/* 3-2: Delay for 10ms. */
	usleep_range(10000, 10000);

	/* Pull up RST pin. */
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_CLR]);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}




#define ENABLE_STATUS 1
#define DISABLE_STATUS 0
#define UNKNOW_STATUS -1
static int ff_ctl_enable_irq(bool on)
{
    int err = 0;
    static int irq_statue = UNKNOW_STATUS;
    FF_LOGV("'%s' enter.", __func__);
    FF_LOGD("irq: '%s'.", on ? "enable" : "disabled");

    if (unlikely(!g_context)) {
        return (-ENOSYS);
    }

    if (on)
    {
        if(irq_statue == ENABLE_STATUS)
        {
            FF_LOGW("irq alread enable!");
        }
        else
        {
            enable_irq(g_context->irq_num);
            irq_statue = ENABLE_STATUS;
        }
    }
    else
    {
        if(irq_statue == DISABLE_STATUS)
        {
            FF_LOGW("irq alread disable!");
        }
        else
        {
            disable_irq(g_context->irq_num);
            irq_statue = DISABLE_STATUS;
        }
    }
    FF_LOGV("'%s' leave.", __func__);
    return err;
}

static void ff_ctl_device_event(struct work_struct *ws)
{
	ff_ctl_context_t *ctx = container_of(ws, ff_ctl_context_t, work_queue);
	char *uevent_env[2] = {"FF_INTERRUPT", NULL};
	FF_LOGV("'%s' enter.", __func__);
	FF_LOGD("%s(irq = %d, ..) toggled.", __func__, ctx->irq_num);

	__pm_wakeup_event(g_context->ttw_wl, 3000);

	kobject_uevent_env(&ctx->miscdev.this_device->kobj, KOBJ_CHANGE, uevent_env);

	FF_LOGV("'%s' leave.", __func__);
}

static irqreturn_t ff_ctl_device_irq(int irq, void *dev_id)
{
	ff_ctl_context_t *ctx = (ff_ctl_context_t *)dev_id;

	FF_LOGI("%s: irq = %d", __func__, irq);
	//char *uevent_env[2] = {"FF_INTERRUPT", NULL};
	disable_irq_nosync(irq);
	//queue_work(ff_core_queue, &ff_core_work);

	if (likely(irq == ctx->irq_num)) {
		if (ff_g_config->enable_fasync && g_context->async_queue) {
			kill_fasync(&g_context->async_queue, SIGIO, POLL_IN);
		} else {
			schedule_work(&ctx->work_queue);
		}
	}

	enable_irq(irq);
	return IRQ_HANDLED;
}

static int ff_ctl_report_key_event(struct input_dev *input, ff_key_event_t *kevent)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	input_report_key(input, kevent->code, kevent->value);
	input_sync(input);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

static const char *ff_ctl_get_version(void)
{
	static char version[FF_DRV_VERSION_LEN] = {'\0', };
	FF_LOGV("'%s' enter.", __func__);

	/* Version info. */
	version[0] = '\0';
	strcat(version, FF_DRV_VERSION);
#ifdef __FF_SVN_REV
	if (strlen(__FF_SVN_REV) > 0) {
		sprintf(version, "%s-r%s", version, __FF_SVN_REV);
	}
#endif
#ifdef __FF_BUILD_DATE
	strcat(version, "-"__FF_BUILD_DATE);
#endif
	//sprintf(version, "%s-%s", version, ff_ctl_arch_str());
	FF_LOGD("version: '%s'.", version);

	FF_LOGV("'%s' leave.", __func__);
	return (const char *)version;
}

#if FF_NOTIFY
static int ff_ctl_fb_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	struct fb_event *event;
	int blank;
	char *uevent_env[2];

	/* If we aren't interested in this event, skip it immediately ... */
	if (action != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
		return NOTIFY_DONE;
	}

	FF_LOGV("'%s' enter.", __func__);

	event = (struct fb_event *)data;
	blank = *(int *)event->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		uevent_env[0] = "FF_SCREEN_ON";
		break;
	case FB_BLANK_POWERDOWN:
		uevent_env[0] = "FF_SCREEN_OFF";
		break;
	default:
		uevent_env[0] = "FF_SCREEN_??";
		break;
	}
	uevent_env[1] = NULL;
	kobject_uevent_env(&g_context->miscdev.this_device->kobj, KOBJ_CHANGE, uevent_env);

	FF_LOGV("'%s' leave.", __func__);
	return NOTIFY_OK;
}
#endif
#if (0)
static int ff_ctl_register_input(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	/* Allocate the input device. */
	g_context->input = input_allocate_device();
	if (!g_context->input) {
		FF_LOGE("input_allocate_device() failed.");
		return (-ENOMEM);
	}
	ff_g_config = &driver_config;
	ff_g_config->keycode_nav_left = 0x6c;//LG DOWN
	ff_g_config->keycode_nav_right = 0x67;//LG UP
	ff_g_config->keycode_nav_up = 0x69;//LG RIGHT
	ff_g_config->keycode_nav_down = 0x6a;//LG LEFT
	ff_g_config->keycode_double_click = 28;
	ff_g_config->keycode_click = 0x94;
	ff_g_config->keycode_long_press = 0x94;
	ff_g_config->keycode_simulation = 172;

	/* Register the key event capabilities. */
	if (ff_g_config) {
		input_set_capability(g_context->input, EV_KEY, ff_g_config->keycode_nav_left);
		input_set_capability(g_context->input, EV_KEY, ff_g_config->keycode_nav_right);
		input_set_capability(g_context->input, EV_KEY, ff_g_config->keycode_nav_up);
		input_set_capability(g_context->input, EV_KEY, ff_g_config->keycode_nav_down);
		input_set_capability(g_context->input, EV_KEY, ff_g_config->keycode_double_click);
		input_set_capability(g_context->input, EV_KEY, ff_g_config->keycode_click);
		input_set_capability(g_context->input, EV_KEY, ff_g_config->keycode_long_press);
		input_set_capability(g_context->input, EV_KEY, ff_g_config->keycode_simulation);
	}
	g_context->input->id.bustype = BUS_HOST;
	g_context->input->dev.init_name = "lge_fingerprint";
	/* Register the allocated input device. */
	g_context->input->name = "ff_key";
	err = input_register_device(g_context->input);
	if (err) {
		FF_LOGE("input_register_device(..) = %d.", err);
		input_free_device(g_context->input);
		g_context->input = NULL;
		return (-ENODEV);
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}
#endif
static int ff_ctl_free_driver(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);
#if FF_NOTIFY
	/* Unregister framebuffer event notifier. */
	err = fb_unregister_client(&g_context->fb_notifier);
#endif
	/* Disable SPI clock. */
	err = ff_ctl_enable_spiclk(0);

	/* Disable the fingerprint module's power. */
	err = ff_ctl_enable_power(0);

	/* Release IRQ resource. */
	if (g_context->irq_num > 0) {
		err = disable_irq_wake(g_context->irq_num);
		if (err) {
			FF_LOGE("disable_irq_wake(%d) = %d.", g_context->irq_num, err);
		}
		free_irq(g_context->irq_num, (void*)g_context);
		g_context->irq_num = -1;
		gpio_free(irq_gpio);
	}

	/* Release pins resource. */
	err = ff_ctl_free_pins();

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

static int ff_ctl_init_driver(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	if (unlikely(!g_context)) {
		return (-ENOSYS);
	}

	do {
		/* Initialize the PWR/SPI/RST/INT pins resource. */
		err = ff_ctl_init_pins(&g_context->irq_num);
		if (err > 0) {
			g_context->b_config_dirtied = true;
		} else \
		if (err) {
			FF_LOGE("ff_ctl_init_pins(..) = %d.", err);
			break;
		}


		/* Register IRQ. */
		err = request_threaded_irq(g_context->irq_num, NULL, ff_ctl_device_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, "ff_irq", g_context);
		if (err) {
			FF_LOGE("request_threaded_irq(..) = %d.", err);
			break;
		}

		/* Wake up the system while receiving the interrupt. */
		err = enable_irq_wake(g_context->irq_num);
		if (err) {
			FF_LOGE("enable_irq_wake(%d) = %d.", g_context->irq_num, err);
		}
	} while (0);

	if (err) {
		ff_ctl_free_driver();
		return err;
	}

	/* Enable the fingerprint module's power at system startup. */
	//err = ff_ctl_enable_power(1);

	/* Enable SPI clock. */
	//err = ff_ctl_enable_spiclk(1);

	g_context->b_spiclk_enabled = false;

    /* disable irq . */
   	 err = ff_ctl_enable_irq(1);
#if FF_NOTIFY
	/* Register screen on/off callback. */
	g_context->fb_notifier.notifier_call = ff_ctl_fb_notifier_callback;
	err = fb_register_client(&g_context->fb_notifier);
#endif
	g_context->b_driver_inited = true;
	FF_LOGV("'%s' leave.", __func__);
	return err;
}

////////////////////////////////////////////////////////////////////////////////
// struct file_operations fields.

static int ff_ctl_fasync(int fd, struct file *filp, int mode)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	FF_LOGD("%s: mode = 0x%08x.", __func__, mode);
	err = fasync_helper(fd, filp, mode, &g_context->async_queue);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

static long ff_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct miscdevice *dev = (struct miscdevice *)filp->private_data;
	ff_ctl_context_t *ctx = container_of(dev, ff_ctl_context_t, miscdev);
	FF_LOGV("'%s' enter.", __func__);

#if 1
	if (g_log_level <= FF_LOG_LEVEL_DBG) {
		static const char *cmd_names[] = {
				"FF_IOC_INIT_DRIVER", "FF_IOC_FREE_DRIVER",
				"FF_IOC_RESET_DEVICE",
				"FF_IOC_ENABLE_IRQ", "FF_IOC_DISABLE_IRQ",
				"FF_IOC_ENABLE_SPI_CLK", "FF_IOC_DISABLE_SPI_CLK",
				"FF_IOC_ENABLE_POWER", "FF_IOC_DISABLE_POWER",
				"FF_IOC_REPORT_KEY_EVENT", "FF_IOC_SYNC_CONFIG",
				"FF_IOC_GET_VERSION", "unknown",
		};
		unsigned int _cmd = _IOC_NR(cmd);
		if (_cmd > FF_IOC_GET_VERSION) {
			_cmd = FF_IOC_GET_VERSION + 1;
		}
		FF_LOGD("%s(.., %s, ..) invoke.", __func__, cmd_names[_cmd]);
	}
#endif

	switch (cmd) {
	case FF_IOC_INIT_DRIVER: {
		if (g_context->b_driver_inited) {
			err = ff_ctl_free_driver();
		}
		if (!err) {
			err = ff_ctl_init_driver();
			// TODO: Sync the dirty configuration back to HAL.
		}
		break;
	}
	case FF_IOC_FREE_DRIVER:
		err = ff_ctl_free_driver();
		break;
	case FF_IOC_RESET_DEVICE:
		err = ff_ctl_reset_device();
		break;
	case FF_IOC_ENABLE_IRQ:
		err = ff_ctl_enable_irq(1);
		break;
	case FF_IOC_DISABLE_IRQ:
		err = ff_ctl_enable_irq(0);
		break;
	case FF_IOC_ENABLE_SPI_CLK:
		err = ff_ctl_enable_spiclk(1);
		break;
	case FF_IOC_DISABLE_SPI_CLK:
		err = ff_ctl_enable_spiclk(0);
		break;
	case FF_IOC_ENABLE_POWER:
		err = ff_ctl_enable_power(1);
		break;
	case FF_IOC_DISABLE_POWER:
		err = ff_ctl_enable_power(0);
		break;
	case FF_IOC_REPORT_KEY_EVENT: {
		ff_key_event_t kevent;
		if (copy_from_user(&kevent, (ff_key_event_t *)arg, sizeof(ff_key_event_t))) {
			FF_LOGE("copy_from_user(..) failed.");
			err = (-EFAULT);
			break;
		}

		err = ff_ctl_report_key_event(ctx->input, &kevent);
		break;
	}
	case FF_IOC_SYNC_CONFIG: {
		if (copy_from_user(&driver_config, (ff_driver_config_t *)arg, sizeof(ff_driver_config_t))) {
			FF_LOGE("copy_from_user(..) failed.");
			err = (-EFAULT);
			break;
		}
		ff_g_config = &driver_config;

		/* Take logging level effect. */
		FF_LOGW("change log level from %d to %d", g_log_level, ff_g_config->log_level);
		g_log_level = ff_g_config->log_level;
		break;
	}
	case FF_IOC_GET_VERSION: {
		if (copy_to_user((void *)arg, ff_ctl_get_version(), FF_DRV_VERSION_LEN)) {
			FF_LOGE("copy_to_user(..) failed.");
			err = (-EFAULT);
			break;
		}
		break;
	}
	default:
		err = (-EINVAL);
		break;
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

static int ff_ctl_open(struct inode *inode, struct file *filp)
{
	FF_LOGD("'%s' nothing to do.", __func__);
	return 0;
}

static int ff_ctl_release(struct inode *inode, struct file *filp)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	/* Remove this filp from the asynchronously notified filp's. */
	err = ff_ctl_fasync(-1, filp, 0);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

#ifdef CONFIG_COMPAT
static long ff_ctl_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	FF_LOGV("focal '%s' enter.\n", __func__);

	err = ff_ctl_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));

	FF_LOGV("'%s' leave.", __func__);
	return err;
}
#endif
///////////////////////////////////////////////////////////////////////////////

static struct file_operations ff_ctl_fops = {
	.owner			= THIS_MODULE,
	.fasync			= ff_ctl_fasync,
	.unlocked_ioctl = ff_ctl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ff_ctl_compat_ioctl,
#endif
	.open			= ff_ctl_open,
	.release		= ff_ctl_release,
};

static ff_ctl_context_t ff_ctl_context = {
	.miscdev = {
		.minor = MISC_DYNAMIC_MINOR,
		.name  = FF_DRV_NAME,
		.fops  = &ff_ctl_fops,
	}, 0,
};




static int ff_probe(struct spi_device *spi)
{
	int err = 0;

	FF_LOGV("'%s' enter.", __func__);

#if 0
	ff_ctl_context_t *ff_dev = NULL; 
	ff_dev = kzalloc(sizeof(*ff_dev), GFP_KERNEL);
	if (!ff_dev) {
 		err = ENOMEM;
		FF_LOGE("focal kzalloc err %d", err);
		return err;
	}
	g_context = ff_dev;

#endif	

    spi->chip_select = 0;
	g_context->ff_spi = spi;

#if 0
/* Get regulator */
	g_context->ff_vdd = regulator_get(&g_context->ff_spi->dev, "fp_vdd");
	if (IS_ERR(g_context->ff_vdd)) {
		FF_LOGE("Can't find vcc_core regulator.");
		g_context->ff_vdd = NULL;
	} else {
		FF_LOGI("Found vcc_core regulator.");
		err = regulator_set_voltage(g_context->ff_vdd, SUPPLY_3V3, SUPPLY_3V3);
	    if (err < 0){
		    FF_LOGE("Can't set voltage.");
		    return err;
	    }
	}

	//ff_ctl_enable_power(1);
#endif
	//spi->dev.of_node = of_find_compatible_node(NULL, NULL, "FF_COMPATIBLE_NODE_1");
	/*ff_pinctrl = devm_pinctrl_get(&spi->dev);
	if (IS_ERR( ff_pinctrl )) {
		err = PTR_ERR(ff_pinctrl);
		FF_LOGE("probe can't find fingerprint pinctrl.");
		return -1;
	}*/

	//ff_finger_workerqueue_init();

	/* Register as a miscellaneous device. */
	err = misc_register(&ff_ctl_context.miscdev);
	if (err) {
		FF_LOGE("misc_register(..) = %d.", err);
		return err;
	}

	/* Initialize the input subsystem. */
	//ExtB P210330-00833,zjj.wt,delete,2021/03/31, fp driver delete nav key
	//err = ff_ctl_register_input();
	//if (err) {
	//	FF_LOGE("ff_ctl_init_input() = %d.", err);
		//return err;
	//}

	/* Init the interrupt workqueue. */
	INIT_WORK(&ff_ctl_context.work_queue, ff_ctl_device_event);
	/* Init the wake lock. */
	
    //wakeup_source_init(&g_context->ttw_wl, "focal_ttw_wl");
    g_context->ttw_wl= wakeup_source_create("focal_ttw_wl");
    wakeup_source_add(g_context->ttw_wl);

	FF_LOGI("FocalTech fingerprint device control driver registered.");
	FF_LOGV("'%s' leave.", __func__);
	return err;
}

static int ff_remove(struct spi_device *spi)
{
	int err = 0;
	g_context->ff_spi = NULL;
	/* Release the HW resources if needs. */
	if (g_context->b_driver_inited) {
		err = ff_ctl_free_driver();
		g_context->b_driver_inited = false;
	}


	/* De-initialize the input subsystem. */
	if (g_context->input) {
		/*
		 * Once input device was registered use input_unregister_device() and
		 * memory will be freed once last reference to the device is dropped.
		 */
		input_unregister_device(g_context->input);
		g_context->input = NULL;
	}

	/* Unregister the miscellaneous device. */
	misc_deregister(&g_context->miscdev);

	//destroy_workqueue(ff_core_queue);
	/*
	if (ff_pinctrl != NULL )
	{
	 devm_pinctrl_put( ff_pinctrl );
	 ff_pinctrl = NULL;
	}
	*/
    //wakeup_source_trash(&g_context->ttw_wl);
    wakeup_source_remove(g_context->ttw_wl);
	FF_LOGI("FocalTech fingerprint device control driver released.");

	return err;
}

static struct of_device_id ff_of_match[] = {
	{ .compatible = "mediatek,finger-focaltech", },
	/*{ .compatible = "fingerprint,focaltech", },*/
};

static struct spi_driver ff_spi_driver = {
	.driver = {
		.name = "focaltech_fp",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ff_of_match),
	},
	.probe = ff_probe,
	.remove = ff_remove,
};

static int __init ff_ctl_driver_init(void)
{
	int err = 0;

	FF_LOGV("'%s' enter.", __func__);



	/* Assign the context instance. */
	g_context = &ff_ctl_context;

	err = spi_register_driver(&ff_spi_driver);

	if (err) {
		FF_LOGE("spi register failed!");
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

static void __exit ff_ctl_driver_exit(void)
{
	FF_LOGV("'%s' enter.", __func__);

	spi_unregister_driver(&ff_spi_driver);
	/* 'g_context' could not use any more. */
	g_context = NULL;
	FF_LOGV("'%s' leave.", __func__);
}

late_initcall(ff_ctl_driver_init);
module_exit(ff_ctl_driver_exit);

MODULE_DESCRIPTION("The device control driver for FocalTech's fingerprint sensor.");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("FocalTech Fingerprint R&D department");

