/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include <linux/of_address.h>
#include <linux/pinctrl/consumer.h>
#include <linux/irq.h>
#include <media/v4l2-subdev.h>
#include <linux/reboot.h>
#include <linux/debugfs.h>

#include "decon.h"
#include "dsim.h"
#include "dpp.h"
#include "../../../../../kernel/irq/internals.h"
#include "decon_notify.h"
#include "decon_board.h"

#define abd_printf(m, x...)	\
{	if (m) seq_printf(m, x); else decon_info(x);	}	\

void decon_abd_save_log_event(struct abd_protect *abd, const char *print)
{
	unsigned int idx = atomic_inc_return(&abd->event.log_idx) % ABD_EVENT_LOG_MAX;

	abd->event.log[idx].stamp = local_clock();
	abd->event.log[idx].print = print;
}

void decon_abd_save_log_fto(struct abd_protect *abd, struct sync_fence *fence)
{
	struct abd_trace *first = &abd->f_first;
	struct abd_trace *lcdon = &abd->f_lcdon;
	struct abd_trace *event = &abd->f_event;

	struct abd_log *first_log = &first->log[(first->count % ABD_LOG_MAX)];
	struct abd_log *lcdon_log = &lcdon->log[(lcdon->count % ABD_LOG_MAX)];
	struct abd_log *event_log = &event->log[(event->count % ABD_LOG_MAX)];

	memset(event_log, 0, sizeof(struct abd_log));
	event_log->stamp = ktime_to_ns(ktime_get());
	memcpy(&event_log->fence, fence, sizeof(struct sync_fence));

	if (!first->count) {
		memset(first_log, 0, sizeof(struct abd_log));
		memcpy(first_log, event_log, sizeof(struct abd_log));
		first->count++;
	}

	if (!lcdon->lcdon_flag) {
		memset(lcdon_log, 0, sizeof(struct abd_log));
		memcpy(lcdon_log, event_log, sizeof(struct abd_log));
		lcdon->count++;
		lcdon->lcdon_flag++;
	}

	event->count++;
}

void decon_abd_save_log_udr(struct abd_protect *abd, unsigned long mif, unsigned long iint, unsigned long disp)
{
	struct decon_device *decon = container_of(abd, struct decon_device, abd);
	struct abd_trace *first = &abd->u_first;
	struct abd_trace *lcdon = &abd->u_lcdon;
	struct abd_trace *event = &abd->u_event;

	struct abd_log *first_log = &first->log[(first->count) % ABD_LOG_MAX];
	struct abd_log *lcdon_log = &lcdon->log[(lcdon->count) % ABD_LOG_MAX];
	struct abd_log *event_log = &event->log[(event->count) % ABD_LOG_MAX];

	memset(event_log, 0, sizeof(struct abd_log));
	event_log->stamp = ktime_to_ns(ktime_get());
	event_log->frm_status = decon->frm_status;
	event_log->mif = mif;
	event_log->iint = iint;
	event_log->disp = disp;
	memcpy(&event_log->bts, &decon->bts, sizeof(struct decon_bts));

	if (!first->count) {
		memset(first_log, 0, sizeof(struct abd_log));
		memcpy(first_log, event_log, sizeof(struct abd_log));
		first->count++;
	}

	if (!lcdon->lcdon_flag) {
		memset(lcdon_log, 0, sizeof(struct abd_log));
		memcpy(lcdon_log, event_log, sizeof(struct abd_log));
		lcdon->count++;
		lcdon->lcdon_flag++;
	}

	event->count++;
}

static void decon_abd_save_log_pin(struct decon_device *decon, struct abd_pin *pin, struct abd_trace *trace, bool on)
{
	struct abd_trace *first = &pin->p_first;

	struct abd_log *first_log = &first->log[(first->count) % ABD_LOG_MAX];
	struct abd_log *trace_log = &trace->log[(trace->count) % ABD_LOG_MAX];

	trace_log->stamp = ktime_to_ns(ktime_get());
	trace_log->level = pin->level;
	trace_log->state = decon->state;
	trace_log->onoff = on;

	if (!first->count) {
		memset(first_log, 0, sizeof(struct abd_log));
		memcpy(first_log, trace_log, sizeof(struct abd_log));
		first->count++;
	}

	trace->count++;
}

static void decon_abd_clear_pending_bit(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	if (desc->irq_data.chip->irq_ack) {
		desc->irq_data.chip->irq_ack(&desc->irq_data);
		desc->istate &= ~IRQS_PENDING;
	}
}

static void decon_abd_enable_interrupt(struct decon_device *decon, struct abd_pin *pin, bool on)
{
	struct abd_trace *trace = &pin->p_lcdon;

	if (!pin || !pin->irq)
		return;

	pin->level = gpio_get_value(pin->gpio);

	decon_info("%s: on: %d, %s(%d,%d) level: %d, count: %d, state: %d%s\n", __func__,
		on, pin->name, pin->irq, pin->desc->depth, pin->level, trace->count, decon->state, (pin->level == pin->active_level) ? ", active" : "");

	if (pin->level == pin->active_level) {
		decon_abd_save_log_pin(decon, pin, trace, on);
		if (pin->name && !strcmp(pin->name, "pcd")) {
			decon->ignore_vsync = 1;
			decon_info("%s: ignore_vsync: %d\n", __func__, decon->ignore_vsync);
		}
	}

	if (on) {
		decon_abd_clear_pending_bit(pin->irq);
		enable_irq(pin->irq);
	} else
		disable_irq_nosync(pin->irq);
}

void decon_abd_enable(struct decon_device *decon, int enable)
{
	struct abd_protect *abd = &decon->abd;
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	unsigned int i = 0;

	if (!abd)
		return;

	if (enable) {
		if (abd->irq_enable == 1) {
			decon_info("%s: already enabled irq_enable: %d\n", __func__, abd->irq_enable);
			return;
		}

		abd->f_lcdon.lcdon_flag = 0;
		abd->u_lcdon.lcdon_flag = 0;

		abd->irq_enable = 1;
	} else {
		if (abd->irq_enable == 0) {
			decon_info("%s: already disabled irq_enable: %d\n", __func__, abd->irq_enable);
			return;
		}

		abd->irq_enable = 0;
	}

	if (dsim && !dsim->priv.lcdconnected)
		decon_info("%s: lcdconnected: %d\n", __func__, dsim->priv.lcdconnected);

	for (i = 0; i < ABD_PIN_MAX; i++)
		decon_abd_enable_interrupt(decon, &abd->pin[i], enable);
}

irqreturn_t decon_abd_handler(int irq, void *dev_id)
{
	struct decon_device *decon = (struct decon_device *)dev_id;
	struct abd_pin *pin = NULL;
	struct abd_trace *trace = NULL;
	unsigned int i = 0;

	spin_lock(&decon->abd.slock);

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &decon->abd.pin[i];
		trace = &pin->p_event;
		if (pin && irq == pin->irq)
			break;
	}

	if (i == ABD_PIN_MAX) {
		decon_info("%s: irq(%d) is not in abd\n", __func__, irq);
		goto exit;
	}

	pin->level = gpio_get_value(pin->gpio);

	decon_info("%s: %s(%d) level: %d, count: %d, state: %d%s\n", __func__,
		pin->name, pin->irq, pin->level, trace->count, decon->state, (pin->level == pin->active_level) ? ", active" : "");

	decon_abd_save_log_pin(decon, pin, trace, 1);

	if (pin->active_level != pin->level)
		goto exit;

	if (i == ABD_PIN_PCD) {
		decon->ignore_vsync = 1;
		decon_info("%s: ignore_vsync: %d\n", __func__, decon->ignore_vsync);
	}

	if (pin->handler)
		pin->handler(irq, pin->dev_id);

exit:
	spin_unlock(&decon->abd.slock);

	return IRQ_HANDLED;
}

int decon_abd_register_pin_handler(int irq, irq_handler_t handler, void *dev_id)
{
	struct decon_device *decon = get_decon_drvdata(0);
	struct abd_pin *pin = NULL;
	unsigned int i = 0;

	if (!irq) {
		decon_info("%s: irq(%d) invalid\n", __func__, irq);
		return -EINVAL;
	}

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &decon->abd.pin[i];
		if (pin && irq == pin->irq) {
			pin->handler = handler;
			pin->dev_id = dev_id;
			decon_info("%s: find %s of irq(%d)\n", __func__, pin->name, irq);
			break;
		}
	}

	if (i == ABD_PIN_MAX) {
		decon_info("%s: irq(%d) is not in abd\n", __func__, irq);
		return -EINVAL;
	}

	return 0;
}

static int decon_debug_pin_log_print(struct seq_file *m, struct abd_trace *trace)
{
	struct timeval tv;
	struct abd_log *log;
	unsigned int i = 0;

	if (!trace->count)
		return 0;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		abd_printf(m, "time: %lu.%06lu level: %d onoff: %d state: %d\n",
			(unsigned long)tv.tv_sec, tv.tv_usec, log->level, log->onoff, log->state);
	}

	return 0;
}

static int decon_debug_pin_print(struct seq_file *m, struct abd_pin *pin)
{
	if (!pin->irq)
		return 0;

	if (!pin->p_first.count)
		return 0;

	abd_printf(m, "[%s]\n", pin->name);

	decon_debug_pin_log_print(m, &pin->p_first);
	decon_debug_pin_log_print(m, &pin->p_lcdon);
	decon_debug_pin_log_print(m, &pin->p_event);

	return 0;
}

static const char *sync_status_str(int status)
{
	if (status == 0)
		return "signaled";

	if (status > 0)
		return "active";

	return "error";
}

static int decon_debug_fto_print(struct seq_file *m, struct abd_trace *trace)
{
	struct timeval tv;
	struct abd_log *log;
	unsigned int i = 0;

	if (!trace->count)
		return 0;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		abd_printf(m, "time: %lu.%06lu, %d, %s: %s\n",
			(unsigned long)tv.tv_sec, tv.tv_usec, log->winid, log->fence.name, sync_status_str(atomic_read(&log->fence.status)));
	}

	return 0;
}

static int decon_debug_udr_print(struct seq_file *m, struct abd_trace *trace)
{
	struct timeval tv;
	struct abd_log *log;
	struct decon_bts *bts;
	struct bts_decon_info *bts_info;
	unsigned int i = 0, idx;

	if (!trace->count)
		return 0;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		abd_printf(m, "time: %lu.%06lu, %d\n",
			(unsigned long)tv.tv_sec, tv.tv_usec, log->frm_status);

		abd_printf(m, "MIF(%lu), INT(%lu), DISP(%lu)\n", log->mif, log->iint, log->disp);

		bts = &log->bts;
		bts_info = &log->bts.bts_info;
		abd_printf(m, "total(%u %u), max(%u %u), peak(%u)\n",
				bts->prev_total_bw,
				bts->total_bw,
				bts->prev_max_disp_freq,
				bts->max_disp_freq,
				bts->peak);

		for (idx = 0; idx < BTS_DPP_MAX; ++idx) {
			if (!bts_info->dpp[idx].used)
				continue;

			abd_printf(m, "DPP[%d] (%d) b(%d) s(%4d %4d) d(%4d %4d %4d %4d)\n",
				idx, bts_info->dpp[idx].idma_type, bts_info->dpp[idx].bpp,
				bts_info->dpp[idx].src_w, bts_info->dpp[idx].src_h,
				bts_info->dpp[idx].dst.x1, bts_info->dpp[idx].dst.x2,
				bts_info->dpp[idx].dst.y1, bts_info->dpp[idx].dst.y2);
		}
	}

	return 0;
}

static int decon_debug_ss_log_print(struct seq_file *m)
{
	unsigned int log_max = 200, i, idx;
	struct timeval tv;
	struct decon_device *decon = m ? m->private : get_decon_drvdata(0);
	int start = atomic_read(&decon->d.event_log_idx);
	struct dpu_log *log;

	start = (start > log_max) ? start - log_max + 1 : 0;

	for (i = 0; i < log_max; i++) {
		idx = (start + i) % DPU_EVENT_LOG_MAX;
		log = &decon->d.event_log[idx];

		if (!ktime_to_ns(log->time))
			continue;
		tv = ktime_to_timeval(log->time);
		if (i && !(i % 10))
			abd_printf(m, "\n");
		abd_printf(m, "%lu.%06lu %2u, ", (unsigned long)tv.tv_sec, tv.tv_usec, log->type);
	}

	abd_printf(m, "\n");

	return 0;
}

static int decon_debug_event_log_print(struct seq_file *m)
{
	unsigned int log_max = ABD_EVENT_LOG_MAX, i, idx;
	struct timeval tv;
	struct decon_device *decon = m ? m->private : get_decon_drvdata(0);
	int start = atomic_read(&decon->abd.event.log_idx);
	struct abd_event_log *log;
	char print_buf[200] = {0, };
	struct seq_file p = {
		.buf = print_buf,
		.size = sizeof(print_buf) - 1,
	};

	if (start < 0)
		return 0;

	abd_printf(m, "==========_LOG_DEBUG_==========\n");

	start = (start > log_max) ? start - log_max + 1 : 0;

	for (i = 0; i < log_max; i++) {
		idx = (start + i) % ABD_EVENT_LOG_MAX;
		log = &decon->abd.event.log[idx];

		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		if (i && !(i % 2)) {
			abd_printf(m, "%s\n", p.buf);
			p.count = 0;
			memset(print_buf, 0, sizeof(print_buf));
		}
		seq_printf(&p, "%lu.%06lu %-20s ", (unsigned long)tv.tv_sec, tv.tv_usec, log->print);
	}

	abd_printf(m, "%s\n", p.count ? p.buf : "");

	return 0;
}

static int decon_debug_show(struct seq_file *m, void *unused)
{
	struct decon_device *decon = m ? m->private : get_decon_drvdata(0);
	struct abd_protect *abd = &decon->abd;
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	unsigned int i = 0;

	abd_printf(m, "==========_LCD_DEBUG_==========\n");
	abd_printf(m, "isync: %d, lcdconnected: %d\n", decon->ignore_vsync, dsim->priv.lcdconnected);
	for (i = 0; i < ABD_PIN_MAX; i++)
		decon_debug_pin_print(m, &abd->pin[i]);

	abd_printf(m, "==========_FTO_DEBUG_==========\n");
	decon_debug_fto_print(m, &abd->f_first);
	decon_debug_fto_print(m, &abd->f_lcdon);
	decon_debug_fto_print(m, &abd->f_event);

	abd_printf(m, "==========_UDR_DEBUG_==========\n");
	decon_debug_udr_print(m, &abd->u_first);
	decon_debug_udr_print(m, &abd->u_lcdon);
	decon_debug_udr_print(m, &abd->u_event);

	abd_printf(m, "==========_RAM_DEBUG_==========\n");
	decon_debug_ss_log_print(m);

	decon_debug_event_log_print(m);

	return 0;
}

static int decon_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, decon_debug_show, inode->i_private);
}

static const struct file_operations decon_debug_fops = {
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.open = decon_debug_open,
};

static int decon_abd_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, reboot_notifier);
	struct decon_device *decon = container_of(abd, struct decon_device, abd);
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	unsigned int i = 0;

	decon_info("++ %s: %lu\n",  __func__, code);

	decon_abd_enable(decon, 0);

	abd_printf(NULL, "isync: %d, lcdconnected: %d\n", decon->ignore_vsync, dsim->priv.lcdconnected);
	for (i = 0; i < ABD_PIN_MAX; i++)
		decon_debug_pin_print(NULL, &abd->pin[i]);

	decon_debug_fto_print(NULL, &abd->f_first);
	decon_debug_fto_print(NULL, &abd->f_lcdon);
	decon_debug_fto_print(NULL, &abd->f_event);

	decon_debug_udr_print(NULL, &abd->u_first);
	decon_debug_udr_print(NULL, &abd->u_lcdon);
	decon_debug_udr_print(NULL, &abd->u_event);

	decon_debug_event_log_print(NULL);

	decon_info("-- %s: %lu\n",  __func__, code);

	return NOTIFY_DONE;
}

static int decon_abd_register_function(struct decon_device *decon, struct abd_pin *pin, char *keyword,
		irqreturn_t func(int irq, void *dev_id))
{
	int ret = 0, gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *np = NULL;
	struct device *dev = decon->dev;
	unsigned int irqf_type = IRQF_TRIGGER_RISING;
	struct abd_trace *trace = &pin->p_lcdon;
	char *prefix_gpio = "gpio_";
	char dts_name[10] = {0, };

	np = dev->of_node;
	if (!np) {
		decon_warn("device node not exist\n");
		goto exit;
	}

	if (strlen(keyword) + strlen(prefix_gpio) >= sizeof(dts_name)) {
		decon_warn("%s: %s is too log(%zu)\n", __func__, keyword, strlen(keyword));
		goto exit;
	}

	scnprintf(dts_name, sizeof(dts_name), "%s%s", prefix_gpio, keyword);

	if (!of_find_property(np, dts_name, NULL))
		goto exit;

	gpio = of_get_named_gpio_flags(np, dts_name, 0, &flags);
	if (!gpio_is_valid(gpio)) {
		decon_info("%s: gpio_is_valid fail, gpio: %s, %d\n", __func__, dts_name, gpio);
		goto exit;
	}

	decon_info("%s: found %s(%d) success\n", __func__, dts_name, gpio);

	if (gpio_to_irq(gpio) > 0) {
		pin->gpio = gpio;
		pin->irq = gpio_to_irq(gpio);
		pin->desc = irq_to_desc(pin->irq);
	} else {
		decon_info("%s: gpio_to_irq fail, gpio: %d, irq: %d\n", __func__, gpio, gpio_to_irq(gpio));
		pin->gpio = 0;
		pin->irq = 0;
		goto exit;
	}

	pin->active_level = !(flags & OF_GPIO_ACTIVE_LOW);
	irqf_type = (flags & OF_GPIO_ACTIVE_LOW) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
	decon_info("%s: %s is active %s, %s\n", __func__, keyword, pin->active_level ? "high" : "low",
		(irqf_type == IRQF_TRIGGER_RISING) ? "rising" : "falling");
	ret++;

	pin->name = keyword;
	pin->p_first.name = "first";
	pin->p_lcdon.name = "lcdon";
	pin->p_event.name = "event";

	irq_set_irq_type(pin->irq, irqf_type);
	irq_set_status_flags(pin->irq, _IRQ_NOAUTOEN);
	decon_abd_clear_pending_bit(pin->irq);

	if (devm_request_irq(dev, pin->irq, func, irqf_type, keyword, decon)) {
		decon_err("%s: failed to request irq for %s\n", __func__, keyword);
		pin->gpio = 0;
		pin->irq = 0;
		ret--;
	}

	pin->level = gpio_get_value(pin->gpio);
	if (pin->level == pin->active_level) {
		decon_info("%s: %s(%d) is already %s(%d)\n", __func__, keyword, pin->gpio,
			(pin->active_level) ? "high" : "low", pin->level);

		decon_abd_save_log_pin(decon, pin, trace, 1);

		if (pin->name && !strcmp(pin->name, "pcd")) {
			decon->ignore_vsync = 1;
			decon_info("%s: ignore_vsync: %d\n", __func__, decon->ignore_vsync);
		}
	}

exit:
	return ret;
}

static int decon_abd_con_fb_blank(struct decon_device *decon)
{
	struct fb_info *info = decon->win[decon->dt.dft_win]->fbinfo;
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	struct fb_event v;
	int blank = 0;

	if (!lock_fb_info(info)) {
		decon_warn("%s: fblock is failed\n", __func__);
		return 0;
	}

	decon_info("%s\n", __func__);

	dsim->priv.lcdconnected = 0;

	blank = FB_BLANK_POWERDOWN;
	v.info = info;
	v.data = &blank;

	decon_notifier_call_chain(FB_EARLY_EVENT_BLANK, &v);
	info->fbops->fb_blank(blank, info);
	decon_notifier_call_chain(FB_EVENT_BLANK, &v);

	unlock_fb_info(info);

	return 0;
}

static void decon_abd_con_work(struct work_struct *work)
{
	struct abd_protect *abd = container_of(work, struct abd_protect, con_work);
	struct decon_device *decon = container_of(abd, struct decon_device, abd);

	decon_info("%s\n", __func__);

	decon_abd_con_fb_blank(decon);
}

irqreturn_t decon_abd_con_handler(int irq, void *dev_id)
{
	struct decon_device *decon = (struct decon_device *)dev_id;

	decon_info("%s\n", __func__);

	queue_work(decon->abd.con_workqueue, &decon->abd.con_work);

	return IRQ_HANDLED;
}

static int decon_abd_con_fb_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, fb_notifier);
	struct decon_device *decon = container_of(abd, struct decon_device, abd);
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (decon->state == DECON_STATE_INIT)
		return NOTIFY_DONE;

	if (fb_blank == FB_BLANK_UNBLANK && event == FB_EARLY_EVENT_BLANK) {
		int gpio_active = of_gpio_get_active("gpio_con");
		flush_workqueue(abd->con_workqueue);
		decon_info("%s: %s\n", __func__, gpio_active ? "disconnected" : "connected");
		if (gpio_active) {
			evdata->info->fbops->fb_blank = NULL;
			return NOTIFY_STOP_MASK;
		} else {
			evdata->info->fbops->fb_blank = decon->abd.decon_fbops.fb_blank;
			decon->ignore_vsync = 0;
			decon_info("%s: ignore_vsync: %d\n", __func__, decon->ignore_vsync);
		}
	}

	return NOTIFY_DONE;
}

static int decon_abd_con_pin_hander_register(struct decon_device *decon)
{
	int ret = 0, gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *np = NULL;
	struct device *dev = decon->dev;
	unsigned int irqf_type = IRQF_TRIGGER_RISING;
	char *prefix_gpio = "gpio_";
	char dts_name[10] = {0, };
	char *keyword = "con";
	struct abd_pin pin = {0, };

	np = dev->of_node;
	if (!np) {
		decon_warn("device node not exist\n");
		goto exit;
	}

	if (strlen(keyword) + strlen(prefix_gpio) >= sizeof(dts_name)) {
		decon_warn("%s: %s is too log(%zu)\n", __func__, keyword, strlen(keyword));
		goto exit;
	}

	scnprintf(dts_name, sizeof(dts_name), "%s%s", prefix_gpio, keyword);

	if (!of_find_property(np, dts_name, NULL)) {
		decon_info("%s: %s not exist\n", __func__, dts_name);
		goto exit;
	}

	gpio = of_get_named_gpio_flags(np, dts_name, 0, &flags);
	if (!gpio_is_valid(gpio)) {
		decon_info("%s: gpio_is_valid fail, gpio: %s, %d\n", __func__, dts_name, gpio);
		goto exit;
	}

	decon_info("%s: found %s(%d) success\n", __func__, dts_name, gpio);

	if (gpio_to_irq(gpio) > 0) {
		pin.gpio = gpio;
		pin.irq = gpio_to_irq(gpio);
	} else {
		decon_info("%s: gpio_to_irq fail, gpio: %d, irq: %d\n", __func__, gpio, gpio_to_irq(gpio));
		pin.gpio = 0;
		pin.irq = 0;
		goto exit;
	}

	pin.active_level = !(flags & OF_GPIO_ACTIVE_LOW);
	irqf_type = (flags & OF_GPIO_ACTIVE_LOW) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
	decon_info("%s: %s is active %s, %s\n", __func__, keyword, pin.active_level ? "high" : "low",
		(irqf_type == IRQF_TRIGGER_RISING) ? "rising" : "falling");

	pin.level = gpio_get_value(pin.gpio);
	if (pin.level != pin.active_level)
		decon_abd_register_pin_handler(pin.irq, decon_abd_con_handler, decon);

exit:
	return ret;
}

static int decon_abd_con_register(struct decon_device *decon)
{
	if (!IS_ENABLED(CONFIG_SEC_FACTORY))
		goto exit;

	if (!of_find_property(decon->dev->of_node, "gpio_con", NULL))
		goto exit;

	memcpy(&decon->abd.decon_fbops, decon->win[decon->dt.dft_win]->fbinfo->fbops, sizeof(struct fb_ops));

	INIT_WORK(&decon->abd.con_work, decon_abd_con_work);

	decon->abd.con_workqueue = create_singlethread_workqueue("lcd_con_workqueue");
	if (!decon->abd.con_workqueue) {
		decon_info("%s: create_singlethread_workqueue fail\n", __func__);
		goto exit;
	}

	decon->abd.fb_notifier.priority = INT_MAX;
	decon->abd.fb_notifier.notifier_call = decon_abd_con_fb_notifier_callback;
	decon_register_notifier(&decon->abd.fb_notifier);

	decon_abd_con_pin_hander_register(decon);

exit:
	return 0;
}

int decon_abd_register(struct decon_device *decon)
{
	int ret = 0;
	struct abd_protect *abd = &decon->abd;

	decon_info("%s: ++\n", __func__);

	atomic_set(&decon->abd.event.log_idx, -1);

	abd->u_first.name = abd->f_first.name = "first";
	abd->u_lcdon.name = abd->f_lcdon.name = "lcdon";
	abd->u_event.name = abd->f_event.name = "event";

	spin_lock_init(&decon->abd.slock);

	ret += decon_abd_register_function(decon, &abd->pin[ABD_PIN_PCD], "pcd", decon_abd_handler);
	ret += decon_abd_register_function(decon, &abd->pin[ABD_PIN_DET], "det", decon_abd_handler);
	ret += decon_abd_register_function(decon, &abd->pin[ABD_PIN_ERR], "err", decon_abd_handler);
	ret += decon_abd_register_function(decon, &abd->pin[ABD_PIN_CON], "con", decon_abd_handler);

	if (ret < 0)
		goto exit;

	debugfs_create_file("debug", 0444, decon->d.debug_root, decon, &decon_debug_fops);

	abd->reboot_notifier.notifier_call = decon_abd_reboot_notifier;
	register_reboot_notifier(&abd->reboot_notifier);
exit:
	decon_info("%s: -- %d entity was registered\n", __func__, ret);

	decon_abd_con_register(decon);

	return ret;
}

