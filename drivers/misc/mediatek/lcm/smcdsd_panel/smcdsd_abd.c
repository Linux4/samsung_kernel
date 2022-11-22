// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/lcd.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reboot.h>
#include <linux/rtc.h>

#include <media/v4l2-subdev.h>
#include "../../../../../kernel/irq/internals.h"

//#define SMCDSD_ABD_CON_UEVENT	1

#if defined(CONFIG_DRM_MEDIATEK)
#include <drm/drm_panel.h>
#if defined(SMCDSD_ABD_CON_UEVENT)
#include "../../mediatek/mtk_notify.h"
#endif
#endif

#if defined(CONFIG_MTK_FB)
#include "disp_lcm.h"
#include "disp_helper.h"
#if defined(SMCDSD_ABD_CON_UEVENT)
#include "../../video/mt6765/videox/mtk_notify.h"
#endif
#endif

#if defined(CONFIG_MEDIATEK_SOLUTION) || defined(CONFIG_ARCH_MEDIATEK)
#include "smcdsd_abd.h"
#include "smcdsd_board.h"
#include "smcdsd_notify.h"
#include "smcdsd_panel.h"
#endif

#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("decon: "fmt), ##__VA_ARGS__)

#define abd_printf(m, ...)	\
{	if (m) seq_printf(m, __VA_ARGS__); else dbg_info(__VA_ARGS__);	}	\


#if defined(CONFIG_MEDIATEK_SOLUTION) || defined(CONFIG_ARCH_MEDIATEK)
struct platform_device *of_find_abd_dt_parent_platform_device(void)
{
	return of_find_device_by_path("/panel");
}

struct platform_device *of_find_abd_container_platform_device(void)
{
	return of_find_device_by_path("/panel");
}

static struct mipi_dsi_lcd_common *find_container(void)
{
	struct platform_device *pdev = NULL;
	struct mipi_dsi_lcd_common *container = NULL;

	pdev = of_find_abd_container_platform_device();
	if (!pdev) {
		dbg_info("%s: of_find_device_by_node fail\n", __func__);
		return NULL;
	}

	container = platform_get_drvdata(pdev);
	if (!container) {
		dbg_info("%s: platform_get_drvdata fail\n", __func__);
		return NULL;
	}

	return container;
}

static inline struct mipi_dsi_lcd_common *get_abd_container_of(struct abd_protect *abd)
{
	struct mipi_dsi_lcd_common *container = container_of(abd, struct mipi_dsi_lcd_common, abd);

	return container;
}

static int get_frame_bypass(struct abd_protect *abd)
{
	return !islcmconnected;
}

static void set_frame_bypass(struct abd_protect *abd, unsigned int bypass)
{
	if (get_frame_bypass(abd) != bypass)
		dbg_info("%s: %d->%d\n", __func__, get_frame_bypass(abd), bypass);

#if defined(CONFIG_MTK_FB)
	disp_helper_set_option(DISP_OPT_NO_LCM_FOR_LOW_POWER_MEASUREMENT, bypass);
#endif

	islcmconnected = bypass ? 0 : !!lcdtype;
}

static int get_mipi_rw_bypass(struct abd_protect *abd)
{
	struct mipi_dsi_lcd_common *container = get_abd_container_of(abd);

	return !container->lcdconnected;
}

static void set_mipi_rw_bypass(struct abd_protect *abd, unsigned int bypass)
{
	struct mipi_dsi_lcd_common *container = get_abd_container_of(abd);

	if (get_mipi_rw_bypass(abd) != bypass)
		dbg_info("%s: %d->%d\n", __func__, get_mipi_rw_bypass(abd), bypass);

	container->lcdconnected = bypass ? 0 : !!lcdtype;
}

static inline int get_boot_lcdtype(void)
{
	return (int)lcdtype;
}

static inline unsigned int get_boot_lcdconnected(void)
{
	return get_boot_lcdtype() ? 1 : 0;
}

static struct fb_info *get_fbinfo(struct abd_protect *abd)
{
	return registered_fb[0];
}
#endif

void smcdsd_abd_save_str(struct abd_protect *abd, const char *print)
{
	struct abd_str *event = NULL;
	struct str_log *event_log = NULL;

	if (!abd || !abd->init_done)
		return;

	event = &abd->s_event;
	event_log = &event->log[(event->count % ABD_LOG_MAX)];

	event_log->stamp = local_clock();
	event_log->ktime = ktime_get_real_seconds();
	event_log->print = print;

	event->count++;

	abd_printf(NULL, "%s\n", print);
}

void find_abd_save_str(const char *print)
{
	struct mipi_dsi_lcd_common *container = get_lcd_common(0);
	struct abd_protect *abd = NULL;

	if (!container)
		return;

	abd = &container->abd;

	smcdsd_abd_save_str(abd, print);
}

static void _smcdsd_abd_print_bit(struct seq_file *m, struct bit_log *log)
{
	struct timeval tv;
	struct rtc_time tm;
	unsigned int bit = 0;
	char print_buf[200] = {0, };
	struct seq_file p = {
		.buf = print_buf,
		.size = sizeof(print_buf) - 1,
	};

	if (!m)
		seq_puts(&p, "smcdsd_abd: ");

	tv = ns_to_timeval(log->stamp);
	rtc_time_to_tm(log->ktime, &tm);
	seq_printf(&p, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / 0x%0*X, ",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
		(unsigned long)tv.tv_sec, tv.tv_usec, log->size >> 2, log->value);

	for (bit = 0; bit < log->size; bit++) {
		if (log->print[bit]) {
			if (!bit || !log->print[bit - 1] || strcmp(log->print[bit - 1], log->print[bit]))
				seq_printf(&p, "%s, ", log->print[bit]);
		}
	}

	abd_printf(m, "%s\n", p.count ? p.buf : "");
}

void smcdsd_abd_save_bit(struct abd_protect *abd, unsigned int size, unsigned int value, char **print)
{
	struct abd_bit *first = NULL;
	struct abd_bit *event = NULL;

	struct bit_log *first_log = NULL;
	struct bit_log *event_log = NULL;

	if (!abd || !abd->init_done)
		return;

	first = &abd->b_first;
	event = &abd->b_event;

	first_log = &first->log[(first->count % ABD_LOG_MAX)];
	event_log = &event->log[(event->count % ABD_LOG_MAX)];

	memset(event_log, 0, sizeof(struct bit_log));
	event_log->stamp = local_clock();
	event_log->ktime = ktime_get_real_seconds();
	event_log->value = value;
	event_log->size = size;
	memcpy(&event_log->print, print, sizeof(char *) * size);

	if (!first->count) {
		memset(first_log, 0, sizeof(struct bit_log));
		memcpy(first_log, event_log, sizeof(struct bit_log));
		first->count++;
	}

	_smcdsd_abd_print_bit(NULL, event_log);

	event->count++;
}

static void smcdsd_abd_save_pin(struct abd_protect *abd, struct abd_pin_info *pin, struct abd_pin *trace, bool on)
{
	struct abd_pin *first = NULL;

	struct pin_log *first_log = NULL;
	struct pin_log *trace_log = NULL;

	if (!abd || !abd->init_done)
		return;

	first = &pin->p_first;

	first_log = &first->log[(first->count) % ABD_LOG_MAX];
	trace_log = &trace->log[(trace->count) % ABD_LOG_MAX];

	trace_log->stamp = local_clock();
	trace_log->ktime = ktime_get_real_seconds();
	trace_log->level = pin->level;
	trace_log->onoff = on;

	if (!first->count) {
		memset(first_log, 0, sizeof(struct pin_log));
		memcpy(first_log, trace_log, sizeof(struct pin_log));
		first->count++;
	}

	trace->count++;
}

static void smcdsd_abd_pin_clear_pending_bit(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	if (desc->irq_data.chip->irq_ack) {
		desc->irq_data.chip->irq_ack(&desc->irq_data);
		desc->istate &= ~IRQS_PENDING;
	}
}

static void smcdsd_abd_pin_enable_irq(int irq, unsigned int on)
{
	if (on) {
		smcdsd_abd_pin_clear_pending_bit(irq);
		enable_irq(irq);
	} else {
		smcdsd_abd_pin_clear_pending_bit(irq);
		disable_irq_nosync(irq);
	}
}

static struct abd_pin_info *smcdsd_abd_find_pin_info(struct abd_protect *abd, unsigned int gpio)
{
	unsigned int i = 0;

	for (i = 0; i < ABD_PIN_MAX; i++) {
		if (abd->pin[i].gpio == gpio)
			return &abd->pin[i];
	}

	return NULL;
}

static void _smcdsd_abd_pin_enable(struct abd_protect *abd, struct abd_pin_info *pin, bool on)
{
	struct abd_pin *trace = &pin->p_lcdon;
	struct abd_pin *event = &pin->p_event;
	unsigned int state = 0;

	if (!abd || !abd->init_done || !pin)
		return;

	if (!pin->gpio && !pin->irq)
		return;

	if (pin->enable == on)
		return;

	pin->enable = on;

	pin->level = gpio_get_value(pin->gpio);

	if (pin->level == pin->active_level)
		smcdsd_abd_save_pin(abd, pin, trace, on);

	dbg_info("%s: on: %d, %s(%3d,%d) level: %d, count: %d(event: %d), state: %d, %s\n", __func__,
		on, pin->name, pin->irq, pin->desc->depth, pin->level, trace->count, event->count, state,
		(pin->level == pin->active_level) ? "abnormal" : "normal");

	if (pin->name && !strcmp(pin->name, "pcd"))
		set_frame_bypass(abd, (pin->level == pin->active_level) ? 1 : 0);

	if (pin->irq)
		smcdsd_abd_pin_enable_irq(pin->irq, on);
}

int smcdsd_abd_pin_enable(struct abd_protect *abd, unsigned int gpio, bool on)
{
	struct abd_pin_info *pin = NULL;

	pin = smcdsd_abd_find_pin_info(abd, gpio);
	if (!pin)
		return -EINVAL;

	_smcdsd_abd_pin_enable(abd, pin, on);

	return 0;
}

void smcdsd_abd_enable(struct abd_protect *abd, unsigned int enable)
{
	unsigned int i = 0;

	if (!abd)
		return;

	if (abd->enable == enable)
		dbg_none("%s: already %s\n", __func__, enable ? "enabled" : "disabled");

	if (abd->enable != enable)
		dbg_info("%s: bypass: rw(%d)frame(%d)\n", __func__, get_mipi_rw_bypass(abd), get_frame_bypass(abd));

	if (!abd->enable && enable) {	/* off -> on */
		abd->f_lcdon.lcdon_flag = 0;
		abd->u_lcdon.lcdon_flag = 0;
	}

	abd->enable = enable;

	for (i = 0; i < ABD_PIN_MAX; i++)
		_smcdsd_abd_pin_enable(abd, &abd->pin[i], enable);
}

irqreturn_t smcdsd_abd_handler(int irq, void *dev_id)
{
	struct abd_protect *abd = (struct abd_protect *)dev_id;
	struct abd_pin_info *pin = NULL;
	struct abd_pin *trace = NULL;
	struct abd_pin *lcdon = NULL;
	struct adb_pin_handler *pin_handler = NULL;
	unsigned int i = 0, state = 0;

	spin_lock(&abd->slock);

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &abd->pin[i];
		trace = &pin->p_event;
		lcdon = &pin->p_lcdon;
		if (pin && irq == pin->irq)
			break;
	}

	if (i == ABD_PIN_MAX) {
		dbg_info("%s: irq(%d) is not in abd\n", __func__, irq);
		goto exit;
	}

	pin->level = gpio_get_value(pin->gpio);

	smcdsd_abd_save_pin(abd, pin, trace, 1);

	dbg_info("%s: %s(%d) level: %d, count: %d(lcdon: %d), state: %d, %s\n", __func__,
		pin->name, pin->irq, pin->level, trace->count, lcdon->count, state,
		(pin->level == pin->active_level) ? "abnormal" : "normal");

	if (pin->active_level != pin->level)
		goto exit;

	if (i == ABD_PIN_PCD)
		set_frame_bypass(abd, 1);

	list_for_each_entry(pin_handler, &pin->handler_list, node) {
		if (pin_handler && pin_handler->handler)
			pin_handler->handler(irq, pin_handler->dev_id);
	}

exit:
	spin_unlock(&abd->slock);

	return IRQ_HANDLED;
}

int smcdsd_abd_pin_register_handler(struct abd_protect *abd, int irq, irq_handler_t handler, void *dev_id)
{
	struct abd_pin_info *pin = NULL;
	struct adb_pin_handler *pin_handler = NULL, *tmp = NULL;
	unsigned int i = 0;

	if (!irq) {
		dbg_info("%s: irq(%d) invalid\n", __func__, irq);
		return -EINVAL;
	}

	if (!handler) {
		dbg_info("%s: handler invalid\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &abd->pin[i];
		if (pin && irq == pin->irq) {
			dbg_info("%s: find irq(%d) for %s pin\n", __func__, irq, pin->name);

			list_for_each_entry_safe(pin_handler, tmp, &pin->handler_list, node) {
				WARN(pin_handler->handler == handler && pin_handler->dev_id == dev_id,
					"%s: already registered handler\n", __func__);
			}

			pin_handler = kzalloc(sizeof(struct adb_pin_handler), GFP_KERNEL);
			if (!pin_handler) {
				dbg_info("%s: handler kzalloc fail\n", __func__);
				break;
			}
			pin_handler->handler = handler;
			pin_handler->dev_id = dev_id;
			list_add_tail(&pin_handler->node, &pin->handler_list);

			dbg_info("%s: handler is registered\n", __func__);
			break;
		}
	}

	if (i == ABD_PIN_MAX) {
		dbg_info("%s: irq(%d) is not in abd\n", __func__, irq);
		return -EINVAL;
	}

	return 0;
}

int smcdsd_abd_pin_unregister_handler(struct abd_protect *abd, int irq, irq_handler_t handler, void *dev_id)
{
	struct abd_pin_info *pin = NULL;
	struct adb_pin_handler *pin_handler = NULL, *tmp = NULL;
	unsigned int i = 0;

	if (!irq) {
		dbg_info("%s: irq(%d) invalid\n", __func__, irq);
		return -EINVAL;
	}

	if (!handler) {
		dbg_info("%s: handler invalid\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &abd->pin[i];
		if (pin && irq == pin->irq) {
			dbg_info("%s: find irq(%d) for %s pin\n", __func__, irq, pin->name);

			list_for_each_entry_safe(pin_handler, tmp, &pin->handler_list, node) {
				if (pin_handler->handler == handler && pin_handler->dev_id == dev_id)
					list_del(&pin->handler_list);
				kfree(pin_handler);
			}

			dbg_info("%s: handler is unregistered\n", __func__);
			break;
		}
	}

	if (i == ABD_PIN_MAX) {
		dbg_info("%s: irq(%d) is not in abd\n", __func__, irq);
		return -EINVAL;
	}

	return 0;
}

static void _smcdsd_abd_print_pin(struct seq_file *m, struct abd_pin *trace)
{
	struct timeval tv;
	struct rtc_time tm;
	struct pin_log *log;
	unsigned int i = 0;

	if (!trace->count)
		return;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;

		tv = ns_to_timeval(log->stamp);
		rtc_time_to_tm(log->ktime, &tm);
		abd_printf(m, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / level: %d onoff: %d state: %d\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec, log->level, log->onoff, log->state);
	}
}

static void smcdsd_abd_print_pin(struct seq_file *m, struct abd_pin_info *pin)
{
	if (!pin->irq && !pin->gpio)
		return;

	if (!pin->p_first.count)
		return;

	abd_printf(m, "[%s]\n", pin->name);

	_smcdsd_abd_print_pin(m, &pin->p_first);
	_smcdsd_abd_print_pin(m, &pin->p_lcdon);
	_smcdsd_abd_print_pin(m, &pin->p_event);
}

static void smcdsd_abd_print_str(struct seq_file *m, struct abd_str *trace)
{
	unsigned int log_max = ABD_LOG_MAX, i, idx;
	struct timeval tv;
	struct rtc_time tm;
	int start = trace->count - 1;
	struct str_log *log;
	char print_buf[200] = {0, };
	struct seq_file p = {
		.buf = print_buf,
		.size = sizeof(print_buf) - 1,
	};

	if (start < 0)
		return;

	abd_printf(m, "==========_STR_DEBUG_==========\n");

	start = (start > log_max) ? start - log_max + 1 : 0;

	for (i = 0; i < log_max; i++) {
		idx = (start + i) % ABD_LOG_MAX;
		log = &trace->log[idx];

		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		rtc_time_to_tm(log->ktime, &tm);
		if (i && !(i % 2)) {
			abd_printf(m, "%s\n", p.buf);
			p.count = 0;
			memset(print_buf, 0, sizeof(print_buf));
		}
		seq_printf(&p, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / %-20s ",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec, log->print);
	}

	abd_printf(m, "%s\n", p.count ? p.buf : "");
}

static void smcdsd_abd_print_bit(struct seq_file *m, struct abd_bit *trace)
{
	struct bit_log *log;
	unsigned int i = 0;

	if (!trace->count)
		return;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		_smcdsd_abd_print_bit(m, log);
	}
}

static int smcdsd_abd_show(struct seq_file *m, void *unused)
{
	struct abd_protect *abd = m->private;
	unsigned int i = 0;

	abd_printf(m, "==========_SMCDSD_ABD_==========\n");
	abd_printf(m, "bypass: rw(%d)frame(%d), lcdtype: %6X\n", get_frame_bypass(abd), get_mipi_rw_bypass(abd), get_boot_lcdtype());

	for (i = 0; i < ABD_PIN_MAX; i++) {
		if (abd->pin[i].p_first.count) {
			abd_printf(m, "==========_PIN_DEBUG_==========\n");
			break;
		}
	}
	for (i = 0; i < ABD_PIN_MAX; i++)
		smcdsd_abd_print_pin(m, &abd->pin[i]);

	if (abd->b_first.count) {
		abd_printf(m, "==========_BIT_DEBUG_==========\n");
		smcdsd_abd_print_bit(m, &abd->b_first);
		smcdsd_abd_print_bit(m, &abd->b_event);
	}

	smcdsd_abd_print_str(m, &abd->s_event);

	return 0;
}

static int smcdsd_abd_open(struct inode *inode, struct file *file)
{
	return single_open(file, smcdsd_abd_show, inode->i_private);
}

static ssize_t smcdsd_abd_write(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct abd_protect *abd = ((struct seq_file *)f->private_data)->private;
	char ibuf[80] = {0, };

	if (!abd->enable) {
		dbg_info("abd enable(%d) invalid\n", abd->enable);
		goto exit;
	}

	if (count >= sizeof(ibuf)) {
		dbg_info("count(%zu) invalid\n", count);
		goto exit;
	}

	if (copy_from_user(ibuf, user_buf, count)) {
		dbg_info("copy_from_user invalid\n");
		goto exit;
	}

	ibuf[count] = '\0';

	if (strncmp(ibuf, "blank", strlen("blank")) == 0) {
		dbg_info("%s: %s\n", __func__, ibuf);

		if (!abd->con_workqueue)
			smcdsd_abd_con_register(abd);

		if (abd->con_workqueue)
			queue_work(abd->con_workqueue, &abd->con_work);
	}

exit:
	return count;
}

static const struct file_operations smcdsd_abd_fops = {
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = smcdsd_abd_write,
	.open = smcdsd_abd_open,
};

static int smcdsd_abd_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, reboot_notifier);
	unsigned int i = 0;
	struct seq_file *m = NULL;

	dbg_info("++ %s: %lu\n",  __func__, code);

	smcdsd_abd_enable(abd, 0);

	abd_printf(m, "==========_SMCDSD_ABD_==========\n");
	abd_printf(m, "bypass: rw(%d)frame(%d), lcdtype: %6X\n", get_frame_bypass(abd), get_mipi_rw_bypass(abd), get_boot_lcdtype());

	for (i = 0; i < ABD_PIN_MAX; i++) {
		if (abd->pin[i].p_first.count) {
			abd_printf(m, "==========_PIN_DEBUG_==========\n");
			break;
		}
	}
	for (i = 0; i < ABD_PIN_MAX; i++)
		smcdsd_abd_print_pin(m, &abd->pin[i]);

	if (abd->b_first.count) {
		abd_printf(m, "==========_BIT_DEBUG_==========\n");
		smcdsd_abd_print_bit(m, &abd->b_first);
		smcdsd_abd_print_bit(m, &abd->b_event);
	}

	smcdsd_abd_print_str(m, &abd->s_event);

	dbg_info("-- %s: %lu\n",  __func__, code);

	return NOTIFY_DONE;
}

static int smcdsd_abd_pin_register_function(struct abd_protect *abd, struct abd_pin_info *pin, char *keyword,
		irqreturn_t func(int irq, void *dev_id))
{
	int ret = 0, gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct device *dev = NULL;
	unsigned int irqf_type = IRQF_TRIGGER_RISING;
	struct abd_pin *trace = &pin->p_lcdon;
	char *prefix_gpio = "gpio_";
	char dts_name[10] = {0, };

	if (strlen(keyword) + strlen(prefix_gpio) >= sizeof(dts_name)) {
		dbg_info("%s: %s is too log(%zu)\n", __func__, keyword, strlen(keyword));
		goto exit;
	}

	scnprintf(dts_name, sizeof(dts_name), "%s%s", prefix_gpio, keyword);

	pdev = of_find_abd_dt_parent_platform_device();
	dev = &pdev->dev;

	np = of_find_smcdsd_board(pdev ? &pdev->dev : NULL);

	if (!of_find_property(np, dts_name, NULL))
		goto exit;

	gpio = of_get_named_gpio_flags(np, dts_name, 0, &flags);
	if (!gpio_is_valid(gpio)) {
		dbg_info("%s: gpio_is_valid fail, gpio: %s, %d\n", __func__, dts_name, gpio);
		goto exit;
	}

	dbg_info("%s: found %s(%d) success\n", __func__, dts_name, gpio);

	if (gpio_to_irq(gpio) > 0) {
		pin->gpio = gpio;
		pin->irq = gpio_to_irq(gpio);
		pin->desc = irq_to_desc(pin->irq);
	} else {
		dbg_info("%s: gpio_to_irq fail, gpio: %d, irq: %d\n", __func__, gpio, gpio_to_irq(gpio));
		pin->gpio = gpio;
		pin->irq = 0;
		pin->desc = kzalloc(sizeof(struct irq_desc), GFP_KERNEL);
	}

	pin->active_level = !(flags & OF_GPIO_ACTIVE_LOW);
	irqf_type = (flags & OF_GPIO_ACTIVE_LOW) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
	dbg_info("%s: %s is active %s%s\n", __func__, keyword, pin->active_level ? "high" : "low",
		(pin->irq) ? ((irqf_type == IRQF_TRIGGER_RISING) ? ", rising" : ", falling") : "");

	pin->name = keyword;
	pin->p_first.name = "first";
	pin->p_lcdon.name = "lcdon";
	pin->p_event.name = "event";

	pin->level = gpio_get_value(pin->gpio);
	if (pin->level == pin->active_level) {
		dbg_info("%s: %s(%d) is already %s(%d)\n", __func__, keyword, pin->gpio,
			(pin->active_level) ? "high" : "low", pin->level);

		smcdsd_abd_save_pin(abd, pin, trace, 1);

		if (pin->name && !strcmp(pin->name, "pcd"))
			set_frame_bypass(abd, 1);
	}

	if (pin->irq) {
		irq_set_irq_type(pin->irq, irqf_type);
		irq_set_status_flags(pin->irq, _IRQ_NOAUTOEN);
		smcdsd_abd_pin_clear_pending_bit(pin->irq);

		if (devm_request_irq(dev, pin->irq, func, irqf_type, keyword, abd)) {
			dbg_info("%s: failed to request irq for %s\n", __func__, keyword);
			/* pin->gpio = 0; */
			pin->irq = 0;
			goto exit;
		}

		INIT_LIST_HEAD(&pin->handler_list);
	}

exit:
	return ret;
}

static int smcdsd_abd_con_set_dummy(struct abd_protect *abd, unsigned int dummy)
{
	struct mipi_dsi_lcd_common *container = get_abd_container_of(abd);
	int ret = NOTIFY_DONE;

	dbg_info("%s: %d\n", __func__, dummy);

	if (dummy) {
		container->drv = NULL;

		ret = NOTIFY_STOP;
	} else {
		container->drv = abd->lcd_driver;
		set_frame_bypass(abd, 0);
		set_mipi_rw_bypass(abd, 0);
		abd->con_blank = 0;
	}

	return ret;
}

static void smcdsd_abd_con_prepare_dummy_info(struct abd_protect *abd)
{
	struct mipi_dsi_lcd_common *container = get_abd_container_of(abd);

	abd->lcd_driver = container->drv;
}

#if defined(SMCDSD_ABD_CON_UEVENT) && defined(CONFIG_MTK_FB)
static void smcdsd_abd_blank(struct abd_protect *abd)
{
	dbg_info("+ %s: noti_uevent\n", __func__);

	noti_uevent_user(&uevent_data, 1);

	dbg_info("- %s\n", __func__);
}
#elif defined(SMCDSD_ABD_CON_UEVENT) && defined(CONFIG_DRM_MEDIATEK)
static void smcdsd_abd_blank(struct abd_protect *abd)
{
	struct mipi_dsi_lcd_common *container = get_abd_container_of(abd);
	struct drm_device *drm;

	dbg_info("+ %s: noti_uevent\n", __func__);

	drm = container->panel.drm;
	if (!drm) {
		dbg_info("%s: drm is null\n", __func__);
		return;
	}

	noti_uevent_user_by_drm(drm, 1);

	dbg_info("- %s\n", __func__);
}
#elif defined(CONFIG_MTK_FB)
static int _smcdsd_abd_fb_blank(struct fb_info *info, int blank)
{
	struct fb_event evdata = {0, };
	int fbblank = 0;
	int ret = 0;

	if (!lock_fb_info(info)) {
		dbg_info("%s: fblock is failed\n", __func__);
		return ret;
	}

	dbg_info("+ %s\n", __func__);

	fbblank = blank;
	evdata.info = info;
	evdata.data = &fbblank;

	info->flags |= FBINFO_MISC_USEREVENT;

	smcdsd_notifier_call_chain(SMCDSD_EARLY_EVENT_BLANK, &evdata);
	ret = info->fbops->fb_blank(blank, info);
	smcdsd_notifier_call_chain(FB_EVENT_BLANK, &evdata);

	info->flags &= ~FBINFO_MISC_USEREVENT;
	unlock_fb_info(info);

	dbg_info("- %s\n", __func__);

	return 0;
}

static void smcdsd_abd_blank(struct abd_protect *abd)
{
	struct fb_info *fbinfo = get_fbinfo(abd);

	dbg_info("+ %s\n", __func__);

	_smcdsd_abd_fb_blank(fbinfo, FB_BLANK_POWERDOWN);

	_smcdsd_abd_fb_blank(fbinfo, FB_BLANK_UNBLANK);

	dbg_info("- %s\n", __func__);
}
#elif defined(CONFIG_DRM_MEDIATEK)
static void smcdsd_abd_blank(struct abd_protect *abd)
{
	struct mipi_dsi_lcd_common *container = get_abd_container_of(abd);

	dbg_info("+ %s\n", __func__);

	if (container->drv) {
		call_drv_ops(container, panel_reset, 0);
		call_drv_ops(container, panel_power, 0);
	}

	smcdsd_abd_con_set_dummy(abd, 1);

	dbg_info("- %s\n", __func__);
}
#endif

static void smcdsd_abd_con_work(struct work_struct *work)
{
	struct abd_protect *abd = container_of(work, struct abd_protect, con_work);

	dbg_info("%s\n", __func__);

	set_frame_bypass(abd, 1);
	set_mipi_rw_bypass(abd, 1);

	smcdsd_abd_blank(abd);
}

irqreturn_t smcdsd_abd_con_handler(int irq, void *dev_id)
{
	struct abd_protect *abd = (struct abd_protect *)dev_id;

	dbg_info("%s: %d\n", __func__, abd->con_blank);

	if (!abd->con_blank)
		queue_work(abd->con_workqueue, &abd->con_work);

	abd->con_blank = 1;

	return IRQ_HANDLED;
}

static int smcdsd_abd_con_fb_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, con_fb_notifier);
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case SMCDSD_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (!abd->init_done) {
		abd->init_done = 1;
		return NOTIFY_DONE;
	}

	if (fb_blank == FB_BLANK_UNBLANK) {
		int gpio_active = of_gpio_get_active("gpio_con");

		dbg_info("%s: %s\n", __func__, gpio_active ? "disconnected" : "connected");
		if (gpio_active > 0)
			return smcdsd_abd_con_set_dummy(abd, 1);
		else
			return smcdsd_abd_con_set_dummy(abd, 0);
	}

	return NOTIFY_DONE;
}

static int smcdsd_abd_con_pin_register_hanlder(struct abd_protect *abd)
{
	int ret = 0, gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	unsigned int irqf_type = IRQF_TRIGGER_RISING;
	char *prefix_gpio = "gpio_";
	char dts_name[10] = {0, };
	char *keyword = "con";
	struct abd_pin_info pin = {0, };

	if (strlen(keyword) + strlen(prefix_gpio) >= sizeof(dts_name)) {
		dbg_info("%s: %s is too log(%zu)\n", __func__, keyword, strlen(keyword));
		goto exit;
	}

	scnprintf(dts_name, sizeof(dts_name), "%s%s", prefix_gpio, keyword);

	pdev = of_find_abd_dt_parent_platform_device();

	np = of_find_smcdsd_board(pdev ? &pdev->dev : NULL);

	if (!of_find_property(np, dts_name, NULL)) {
		dbg_info("%s: %s not exist\n", __func__, dts_name);
		goto exit;
	}

	gpio = of_get_named_gpio_flags(np, dts_name, 0, &flags);
	if (!gpio_is_valid(gpio)) {
		dbg_info("%s: gpio_is_valid fail, gpio: %s, %d\n", __func__, dts_name, gpio);
		goto exit;
	}

	dbg_info("%s: found %s(%d) success\n", __func__, dts_name, gpio);

	if (gpio_to_irq(gpio) > 0) {
		pin.gpio = gpio;
		pin.irq = gpio_to_irq(gpio);
	} else {
		dbg_info("%s: gpio_to_irq fail, gpio: %d, irq: %d\n", __func__, gpio, gpio_to_irq(gpio));
		pin.gpio = 0;
		pin.irq = 0;
		goto exit;
	}

	pin.active_level = !(flags & OF_GPIO_ACTIVE_LOW);
	irqf_type = (flags & OF_GPIO_ACTIVE_LOW) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
	dbg_info("%s: %s is active %s%s\n", __func__, keyword, pin.active_level ? "high" : "low",
		(irqf_type == IRQF_TRIGGER_RISING) ? ", rising" : ", falling");

	pin.level = gpio_get_value(pin.gpio);
	if (pin.level != pin.active_level)
		smcdsd_abd_pin_register_handler(abd, pin.irq, smcdsd_abd_con_handler, abd);

exit:
	return ret;
}

int smcdsd_abd_con_register(struct abd_protect *abd)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;

	pdev = of_find_abd_dt_parent_platform_device();

	np = of_find_smcdsd_board(pdev ? &pdev->dev : NULL);

	if (!of_find_property(np, "gpio_con", NULL))
		goto exit;

	smcdsd_abd_con_prepare_dummy_info(abd);

	INIT_WORK(&abd->con_work, smcdsd_abd_con_work);

	abd->con_workqueue = create_singlethread_workqueue("abd_conn_workqueue");
	if (!abd->con_workqueue) {
		dbg_info("%s: create_singlethread_workqueue fail\n", __func__);
		goto exit;
	}

	abd->con_fb_notifier.priority = INT_MAX;
	abd->con_fb_notifier.notifier_call = smcdsd_abd_con_fb_notifier_callback;
	smcdsd_register_notifier(&abd->con_fb_notifier);

	smcdsd_abd_con_pin_register_hanlder(abd);

exit:
	return 0;
}

static void smcdsd_pm_wake(struct abd_protect *abd, unsigned int wake_lock)
{
	struct device *fbdev = NULL;

	if (!abd->fbdev)
		return;

	fbdev = abd->fbdev;

	if (abd->wake_lock_enable == wake_lock)
		return;

	if (wake_lock) {
		pm_stay_awake(fbdev);
		dbg_info("%s: pm_stay_awake", __func__);
	} else if (!wake_lock) {
		pm_relax(fbdev);
		dbg_info("%s: pm_relax", __func__);
	}

	abd->wake_lock_enable = wake_lock;
}

static int smcdsd_abd_pin_early_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, pin_early_notifier);
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EARLY_EVENT_BLANK:
	case SMCDSD_EARLY_EVENT_DOZE:
		break;
	default:
		return NOTIFY_DONE;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (IS_EARLY(event) && fb_blank == FB_BLANK_POWERDOWN)
		smcdsd_abd_enable(abd, 0);

	if (IS_EARLY(event) && fb_blank == FB_BLANK_UNBLANK)
		smcdsd_pm_wake(abd, 1);

	return NOTIFY_DONE;
}

static int smcdsd_abd_pin_after_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, pin_after_notifier);
	struct fb_event *evdata = data;
	struct abd_pin_info *pin = NULL;
	unsigned int i = 0;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case SMCDSD_EVENT_DOZE:
		break;
	default:
		return NOTIFY_DONE;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (IS_AFTER(event) && fb_blank == FB_BLANK_UNBLANK)
		smcdsd_abd_enable(abd, 1);
	else if (IS_AFTER(event) && fb_blank == FB_BLANK_POWERDOWN) {
		for (i = 0; i < ABD_PIN_MAX; i++) {
			pin = &abd->pin[i];
			if (pin && pin->irq)
				smcdsd_abd_pin_clear_pending_bit(pin->irq);
		}
	}

	if (IS_AFTER(event) && fb_blank == FB_BLANK_POWERDOWN)
		smcdsd_pm_wake(abd, 0);

	return NOTIFY_DONE;
}

static void smcdsd_abd_pin_register(struct abd_protect *abd)
{
	spin_lock_init(&abd->slock);

	smcdsd_abd_pin_register_function(abd, &abd->pin[ABD_PIN_PCD], "pcd", smcdsd_abd_handler);
	smcdsd_abd_pin_register_function(abd, &abd->pin[ABD_PIN_DET], "det", smcdsd_abd_handler);
	smcdsd_abd_pin_register_function(abd, &abd->pin[ABD_PIN_ERR], "err", smcdsd_abd_handler);
	smcdsd_abd_pin_register_function(abd, &abd->pin[ABD_PIN_CON], "con", smcdsd_abd_handler);
	smcdsd_abd_pin_register_function(abd, &abd->pin[ABD_PIN_LOG], "log", smcdsd_abd_handler);

	abd->pin_early_notifier.notifier_call = smcdsd_abd_pin_early_notifier_callback;
	abd->pin_early_notifier.priority = smcdsd_nb_priority_max.priority - 1;
	smcdsd_register_notifier(&abd->pin_early_notifier);

	abd->pin_after_notifier.notifier_call = smcdsd_abd_pin_after_notifier_callback;
	abd->pin_after_notifier.priority = smcdsd_nb_priority_min.priority + 1;
	smcdsd_register_notifier(&abd->pin_after_notifier);
}

static void smcdsd_abd_register(struct abd_protect *abd)
{
	struct dentry *abd_debugfs_root = NULL;

	dbg_info("%s: ++\n", __func__);

	if (!abd_debugfs_root)
		abd_debugfs_root = debugfs_create_dir("panel", NULL);

	abd->debugfs_root = abd_debugfs_root;

	abd->u_first.name = abd->f_first.name = abd->b_first.name = "first";
	abd->u_lcdon.name = abd->f_lcdon.name = "lcdon";
	abd->u_event.name = abd->f_event.name = abd->b_event.name = abd->s_event.name = "event";

	debugfs_create_file("debug", 0444, abd_debugfs_root, abd, &smcdsd_abd_fops);

	abd->reboot_notifier.notifier_call = smcdsd_abd_reboot_notifier;
	register_reboot_notifier(&abd->reboot_notifier);

	dbg_info("%s: -- entity was registered\n", __func__);
}

static int match_dev_name(struct device *dev, const void *data)
{
	const char *keyword = data;

	return dev_name(dev) ? !!strstr(dev_name(dev), keyword) : 0;
}

struct device *find_lcd_class_device(void)
{
	static struct class *p_lcd_class;
	struct lcd_device *new_ld = NULL;
	struct device *dev = NULL;

	if (!p_lcd_class) {
		new_ld = lcd_device_register("dummy_lcd_class_device", NULL, NULL, NULL);
		if (!new_ld)
			return NULL;

		p_lcd_class = new_ld->dev.class;
		lcd_device_unregister(new_ld);
	}

	dev = class_find_device(p_lcd_class, NULL, "panel", match_dev_name);

	return dev;
}

static int __init smcdsd_abd_init(void)
{
	struct mipi_dsi_lcd_common *container = find_container();
	struct abd_protect *abd = NULL;
	struct fb_info *fbinfo = NULL;

	if (!container) {
		dbg_info("find_container fail\n");
		return 0;
	}

	abd = &container->abd;

	smcdsd_abd_register(abd);
	abd->init_done = 1;

	find_lcd_class_device();

	dbg_info("%s: lcdtype: %6X\n", __func__, get_boot_lcdtype());

	if (get_boot_lcdconnected())
		smcdsd_abd_pin_register(abd);
	else
		set_frame_bypass(abd, 1);

	smcdsd_abd_enable(abd, 1);

	if (IS_ENABLED(CONFIG_ARCH_EXYNOS))
		return 0;

	if (IS_ENABLED(CONFIG_MEDIATEK_SOLUTION))
		return 0;

	if (IS_ENABLED(CONFIG_ARCH_MEDIATEK))
		return 0;

	fbinfo = get_fbinfo(abd);
	abd->fbdev = fbinfo->dev;

	if (abd->fbdev) {
		device_init_wakeup(abd->fbdev, true);
		dbg_info("%s: device_init_wakeup\n", __func__);
	}

	return 0;
}
late_initcall_sync(smcdsd_abd_init);

