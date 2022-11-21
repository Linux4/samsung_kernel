/*
 * es705-platform.c  --  Audience eS705 platform dependent functions
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Genisim Tsilker <gtsilker@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define SAMSUNG_ES705_FEATURE

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>

#if defined(SAMSUNG_ES705_FEATURE)
#include <linux/input.h>
#endif

#include <linux/esxxx.h>
#include "es705.h"
#include "es705-platform.h"

#define ES705_MCLK_GPIO 47

static int stimulate_only_once = 0;
static int reset_gpio;

int es705_get_stimulate_status(void)
{
	return stimulate_only_once;
}

int es705_enable_ext_clk(int enable)
{
	pr_info("%s: enable=%d\n", __func__, enable);

	if (enable)
		gpio_direction_output_ex(ES705_MCLK_GPIO, 1);
	else
		gpio_direction_output_ex(ES705_MCLK_GPIO, 0);

	return 0;
}

void es705_clk_init(struct es705_priv *es705)
{
	es705->pdata->esxxx_clk_cb = es705_enable_ext_clk;
}

int es705_stimulate_start(struct device *dev)
{
	int rc = 0;
	struct device_node *node = of_get_parent(dev->of_node);

	if (stimulate_only_once) {
		dev_info(dev, "%s(): register node %s. es705 stimulate start already started\n",
				__func__, dev->of_node->full_name);
		return 0;
	}

	dev_info(dev, "%s(): register node %s, es705 stimulate start\n",
			__func__, dev->of_node->full_name);
	/* Populte GPIO reset from DTS */
	reset_gpio = of_get_named_gpio(node,
					      "es705-reset-gpio", 0);
	if (reset_gpio < 0)
		of_property_read_u32(node, "es705-reset-expander-gpio",
				     &reset_gpio);
	if (reset_gpio < 0) {
		dev_err(dev, "%s(): get reset_gpio failed\n", __func__);
		goto populate_reset_err;
	}
	dev_info(dev, "%s(): reset gpio %d\n", __func__, reset_gpio);

	/* Generate es705 reset */
	rc = gpio_request(reset_gpio, "es705_reset");
	if (rc < 0) {
		dev_err(dev, "%s(): es705_reset request failed",
			__func__);
		goto reset_gpio_request_error;
	}
	rc = gpio_direction_output(reset_gpio, 0);
	if (rc < 0) {
		dev_err(dev, "%s(): es705_reset direction failed",
			__func__);
		goto reset_gpio_direction_error;
	}

	rc = gpio_request(ES705_MCLK_GPIO, "mclk");
	if (rc < 0 && rc != -EBUSY) {			
		dev_err(dev, "%s(): mclk request failed",
			__func__);
		goto mclk_gpio_request_error;
	}
	rc = gpio_direction_output(ES705_MCLK_GPIO, 0);
	if (rc < 0) {
		dev_err(dev, "%s(): mclk direction failed",
			__func__);
		goto mclk_gpio_direction_error;
	}

	/* Enable es705 clock */
	es705_enable_ext_clk(1);
	usleep_range(5000, 5000);

	/* Wait 1 ms then pull Reset signal in High */
	usleep_range(1000, 1000);
	gpio_direction_output(reset_gpio, 1);
	usleep_range(10000, 10000);
	stimulate_only_once = 1;

	return rc;
mclk_gpio_direction_error:
	gpio_free(ES705_MCLK_GPIO);
mclk_gpio_request_error:
reset_gpio_direction_error:
	gpio_free(reset_gpio);
reset_gpio_request_error:
populate_reset_err:
	return rc;
}

void es705_gpio_reset(struct es705_priv *es705)
{
	dev_dbg(es705->dev, "%s(): GPIO reset\n", __func__);
	gpio_direction_output(es705->pdata->reset_gpio, 0);
	/* Wait 1 ms then pull Reset signal in High */
	usleep_range(1000, 1000);
	gpio_direction_output(es705->pdata->reset_gpio, 1);
	/* Wait 10 ms then */
	usleep_range(10000, 10000);
	/* eSxxx is READY */
}

int es705_gpio_init(struct es705_priv *es705)
{
	int rc = 0;
	es705->pdata->reset_gpio = reset_gpio;

	if (es705->pdata->wakeup_gpio != -1) {
		rc = gpio_request(es705->pdata->wakeup_gpio, "es705_wakeup");
		if (rc < 0) {
			dev_err(es705->dev, "%s(): es705_wakeup request failed",
				__func__);
			goto wakeup_gpio_request_error;
		}
		rc = gpio_direction_output(es705->pdata->wakeup_gpio, 0);
		if (rc < 0) {
			dev_err(es705->dev, "%s(): es705_wakeup direction failed",
				__func__);
			goto wakeup_gpio_direction_error;
		}
	} else {
		dev_warn(es705->dev, "%s(): wakeup_gpio undefined\n",
				__func__);
	}
	/* under H/W rev 0.5 */
	if (es705->pdata->uart_gpio != -1) {
		rc = gpio_request(es705->pdata->uart_gpio, "es705_uart");
		if (rc < 0) {
			dev_err(es705->dev, "%s(): es705_uart request failed",
				__func__);
			goto uart_gpio_request_error;
		}
		rc = gpio_direction_output(es705->pdata->uart_gpio, 0);
		if (rc < 0) {
			dev_err(es705->dev, "%s(): es705_uart direction failed",
				__func__);
			goto uart_gpio_direction_error;
		}
	} else {
		dev_warn(es705->dev, "%s(): es705_uart undefined\n",
				__func__);
	}

	if (es705->pdata->gpiob_gpio) {
		rc = request_threaded_irq(es705->pdata->irq_base, NULL,
					  es705_irq_event, IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					  "es705-irq-event", es705);
		if (rc) {
			dev_err(es705->dev, "%s(): event request_irq() failed\n",
				__func__);
			goto event_irq_request_error;
		}
		rc = irq_set_irq_wake(es705->pdata->irq_base, 1);
		if (rc < 0) {
			dev_err(es705->dev, "%s(): set event irq wake failed\n",
				__func__);
			disable_irq(es705->pdata->irq_base);
			free_irq(es705->pdata->irq_base, es705);
			goto event_irq_wake_error;
		}
	}

	return rc;
uart_gpio_direction_error:
	gpio_free(es705->pdata->uart_gpio);
uart_gpio_request_error:
wakeup_gpio_direction_error:
	gpio_free(es705->pdata->wakeup_gpio);
wakeup_gpio_request_error:
event_irq_wake_error:
event_irq_request_error:
	return rc;
}

#if defined(SAMSUNG_ES705_FEATURE)
extern unsigned int system_rev;
void es705_gpio_wakeup(struct es705_priv *es705)
{
	dev_info(es705->dev, "%s(): generate gpio wakeup falling edge\n",
		__func__);

	if (system_rev >= 4) {
		gpio_tlmm_config(GPIO_CFG(0, 0, GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), 1);
		gpio_direction_output(es705->pdata->wakeup_gpio, 1);
	} else 
		gpio_set_value(es705->pdata->wakeup_gpio, 1);

	usleep_range(1000,1000);

	if (system_rev >= 4) {
		gpio_direction_output(es705->pdata->wakeup_gpio, 0);
		gpio_direction_input(es705->pdata->wakeup_gpio);
		gpio_tlmm_config(GPIO_CFG(0, 2, GPIO_CFG_OUTPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), 1);
	} else
		gpio_set_value(es705->pdata->wakeup_gpio, 0);
}

void es705_uart_pin_preset(struct es705_priv *es705)
{
	if((es705->pdata->uart_tx_gpio != -1)
		&& (es705->pdata->uart_rx_gpio != -1)) {
		gpio_tlmm_config(GPIO_CFG(es705->pdata->uart_tx_gpio, 2, GPIO_CFG_OUTPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), 1);
		gpio_tlmm_config(GPIO_CFG(es705->pdata->uart_rx_gpio, 2, GPIO_CFG_OUTPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), 1);
	}
}

void es705_uart_pin_postset(struct es705_priv *es705)
{
	if((es705->pdata->uart_tx_gpio != -1)
		&& (es705->pdata->uart_rx_gpio != -1)) {
		gpio_tlmm_config(GPIO_CFG(es705->pdata->uart_tx_gpio, 0, GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), 1);
		gpio_tlmm_config(GPIO_CFG(es705->pdata->uart_rx_gpio, 0, GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), 1);
	}
}

int es705_init_input_device(struct es705_priv *es705)
{
	int rc;
	es705->input = input_allocate_device();
	if (!es705->input) {
		rc = -ENOMEM;
		goto es705_init_input_device_exit;
	}

	es705->input->name = "es705 input";
	set_bit(EV_SYN, es705->input->evbit);
	set_bit(EV_KEY, es705->input->evbit);
	set_bit(KEY_VOICE_WAKEUP, es705->input->keybit);
	set_bit(KEY_VOICE_WAKEUP_LPSD, es705->input->keybit);

	rc = input_register_device(es705->input);
	if (rc < 0)
		input_free_device(es705->input);

es705_init_input_device_exit:
	return rc;
}

void es705_unregister_input_device(struct es705_priv *es705)
{
	input_unregister_device(es705->input);
}
#endif

void es705_vs_event(struct es705_priv *es705)
{
#if defined(SAMSUNG_ES705_FEATURE)
#if defined(SEAMLESS_VOICE_WAKEUP) /* UEvent*/
	char keyword_buf[100];
	unsigned int keyword_reg = 0;
	char *envp[2];

	memset(keyword_buf, 0, sizeof(keyword_buf));

	if (es705->voice_wakeup_enable == 1){ /* Voice wakeup */
		keyword_reg = es705_priv.detected_vs_keyword;
		es705->keyword_type = keyword_reg;
		snprintf(keyword_buf, sizeof(keyword_buf),
					"VOICE_WAKEUP_WORD_ID=%x", keyword_reg);
	}
	else if (es705->voice_wakeup_enable == 2){ /* Voice wakeup LPSD */
		snprintf(keyword_buf, sizeof(keyword_buf),
					"VOICE_WAKEUP_WORD_ID=LPSD");
	}
	else {
		dev_info(es705->dev, "%s(): Invalid value(%d)\n", __func__, 
			es705->voice_wakeup_enable);
		return;
	}
	
	envp[0] = keyword_buf;
	envp[1] = NULL;

	dev_info(es705->dev, "%s : raise the uevent, string = %s\n", __func__, keyword_buf);
	kobject_uevent_env(&(es705->keyword->kobj), KOBJ_CHANGE, envp);
#else /* Key Event*/
	unsigned int vs_event_type = 0;

	if (es705->voice_wakeup_enable == 1) /* Voice wakeup */
		vs_event_type = KEY_VOICE_WAKEUP;
	else if (es705->voice_wakeup_enable == 2) /* Voice wakeup LPSD */
		vs_event_type = KEY_VOICE_WAKEUP_LPSD;
	else {
		dev_info(es705->dev, "%s(): Invalid value(%d)\n", __func__, es705->voice_wakeup_enable);
		return;
	}

	dev_info(es705->dev, "%s(): Raise key event(%d)\n", __func__, vs_event_type);

	input_report_key(es705_priv.input, vs_event_type, 1);
	input_sync(es705_priv.input);
	msleep(10);
	input_report_key(es705_priv.input, vs_event_type, 0);
	input_sync(es705_priv.input);
#endif
#else
	struct slim_device *sbdev = es705->gen0_client;
	kobject_uevent(&sbdev->dev.kobj, KOBJ_CHANGE);
#endif
}

void es705_gpio_free(struct esxxx_platform_data *pdata)
{
	if (pdata->reset_gpio != -1)
		gpio_free(pdata->reset_gpio);
	if (pdata->gpioa_gpio != -1)
		gpio_free(pdata->gpioa_gpio);
	if (pdata->uart_gpio != -1)
		gpio_free(pdata->uart_gpio);
	if (pdata->wakeup_gpio != -1)
		gpio_free(pdata->wakeup_gpio);
}

struct esxxx_platform_data *es705_populate_dt_pdata(struct device *dev)
{
	struct esxxx_platform_data *pdata;
	struct device_node *node = of_get_parent(dev->of_node);

	dev_info(dev, "%s(): parent node %s\n",
			__func__, node->full_name);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "%s(): platform data allocation failed\n",
			__func__);
		goto err;
	}
	pdata->reset_gpio = of_get_named_gpio(node, "es705-reset-gpio", 0);
	if (pdata->reset_gpio < 0)
		of_property_read_u32(node, "es705-reset-expander-gpio",
								&pdata->reset_gpio);
	if (pdata->reset_gpio < 0) {
		dev_err(dev, "%s(): get reset_gpio failed\n", __func__);
		goto alloc_err;
	}
	dev_dbg(dev, "%s(): reset gpio %d\n", __func__, pdata->reset_gpio);

	pdata->gpioa_gpio = of_get_named_gpio(node, "es705-gpioa-gpio", 0);
	if (pdata->gpioa_gpio < 0) {
		dev_err(dev, "%s(): get gpioa_gpio failed\n", __func__);
		goto alloc_err;
	}
	dev_dbg(dev, "%s(): gpioa gpio %d\n", __func__, pdata->gpioa_gpio);

	pdata->gpiob_gpio = of_get_named_gpio(node, "es705-gpiob-gpio", 0);
	if (pdata->gpiob_gpio < 0) {
		dev_err(dev, "%s(): get gpiob_gpio failed\n", __func__);
		goto alloc_err;
	}
	dev_dbg(dev, "%s(): gpiob gpio %d\n", __func__, pdata->gpiob_gpio);

	pdata->uart_tx_gpio = of_get_named_gpio(node, "es705-uart-tx", 0);
	if (pdata->uart_tx_gpio < 0) {
		dev_info(dev, "%s(): get uart_tx_gpio failed\n", __func__);
		pdata->uart_tx_gpio = -1;
	}
	dev_dbg(dev, "%s(): uart tx gpio %d\n", __func__, pdata->uart_tx_gpio);

	pdata->uart_rx_gpio = of_get_named_gpio(node, "es705-uart-rx", 0);
	if (pdata->uart_rx_gpio < 0) {
		dev_info(dev, "%s(): get uart_rx_gpio failed\n", __func__);
		pdata->uart_rx_gpio = -1;
	}
	dev_dbg(dev, "%s(): uart rx gpio %d\n", __func__, pdata->uart_rx_gpio);

	pdata->wakeup_gpio = of_get_named_gpio(node, "es705-wakeup-gpio", 0);
	if (pdata->wakeup_gpio < 0) {
		dev_info(dev, "%s(): get wakeup_gpio failed\n", __func__);
		pdata->wakeup_gpio = -1;
	}
	dev_dbg(dev, "%s(): wakeup gpio %d\n", __func__, pdata->wakeup_gpio);

	pdata->uart_gpio = of_get_named_gpio(node, "es705-uart-gpio", 0);
	if (pdata->uart_gpio < 0) {
		dev_info(dev, "%s(): get uart_gpio failed\n", __func__);
		pdata->uart_gpio = -1;
	}
	dev_dbg(dev, "%s(): uart gpio %d\n", __func__, pdata->uart_gpio);
	pdata->irq_base = gpio_to_irq(pdata->gpiob_gpio);

	return pdata;
alloc_err:
	devm_kfree(dev, pdata);
err:
	return NULL;
}
