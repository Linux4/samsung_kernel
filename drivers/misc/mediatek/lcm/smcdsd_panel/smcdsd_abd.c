// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk-provider.h>
#include <linux/ctype.h>
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

#define SMCDSD_ABD_MTK_UEVENT	1

#if defined(CONFIG_DRM_MEDIATEK)
#include <drm/drm_panel.h>
#if defined(SMCDSD_ABD_MTK_UEVENT)
#include "../../mediatek/mtk_notify.h"
#endif
#endif

#if defined(CONFIG_MTK_FB)
#include "disp_lcm.h"
#include "disp_helper.h"
#if defined(SMCDSD_ABD_MTK_UEVENT)
#include "mtk_notify.h"
#endif
#endif

#if defined(CONFIG_MEDIATEK_SOLUTION) || defined(CONFIG_ARCH_MEDIATEK)
#include "smcdsd_abd.h"
#include "smcdsd_board.h"
#include "smcdsd_notify.h"
#include "smcdsd_panel.h"
#endif

#define STREQ(a, b)			(a && b && (*(a) == *(b)) && (strcmp((a), (b)) == 0))
#define STRNEQ(a, b)			(a && b && (strncmp((a), (b), (strlen(a))) == 0))

#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)

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

static int get_boot_frame_bypass(struct abd_protect *abd)
{
	return !abd->islcmconnected;
}

static int get_frame_bypass(struct abd_protect *abd)
{
	return !islcmconnected;
}

static void set_frame_bypass(struct abd_protect *abd, unsigned int bypass)
{
#if defined(THIS_IS_REDUNDANT_CODE)
#if defined(CONFIG_MTK_HIGH_FRAME_RATE)
	struct mipi_dsi_lcd_common *container = get_abd_container_of(abd);
#endif

	if (get_frame_bypass(abd) != bypass)
		dbg_info("%s: %d->%d\n", __func__, get_frame_bypass(abd), bypass);

	islcmconnected = bypass ? 0 : abd->islcmconnected;

#if defined(CONFIG_MTK_HIGH_FRAME_RATE)
	disp_helper_set_option(DISP_OPT_DYNAMIC_FPS, bypass ? 0 : !!container->lcm_params->dsi.dfps_num);
#endif
#endif
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

static void save_boot_lcd_information(struct abd_protect *abd)
{
	abd->islcmconnected = islcmconnected;

	if (get_boot_lcdconnected() && get_frame_bypass(abd))
		smcdsd_abd_save_str(abd, "islcmconnected abnormal");
}
#endif

static int smcdsd_abd_simple_write_to_buffer(char *ibuf, size_t sizeof_ibuf,
		loff_t *ppos, const char *user_buf, size_t count)
{
	int ret = 0;
	loff_t pos = 0;

	if (ppos && *ppos != 0)
		return -EINVAL;

	if (count == 0)
		return -EINVAL;

	if (count >= sizeof_ibuf)
		return -ENOMEM;

	memset(ibuf, 0, sizeof_ibuf);

	ret = simple_write_to_buffer(ibuf, sizeof_ibuf, ppos ? ppos : &pos, user_buf, count);
	if (ret < 0)
		return ret;

	ibuf[ret] = '\0';

	ibuf = strim(ibuf);

	if (ibuf[0] && !isalnum(ibuf[0]))
		return -EFAULT;

	return 0;
}

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

static void __smcdsd_abd_print_bit(struct seq_file *m, struct bit_log *log)
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

	__smcdsd_abd_print_bit(NULL, event_log);

	event->count++;
}

static void smcdsd_abd_save_pin(struct abd_protect *abd, struct abd_pin_info *pin, struct abd_pin *trace, bool on)
{
	struct abd_pin *first = NULL;

	struct pin_log *first_log = NULL;
	struct pin_log *trace_log = NULL;

	if (!abd || !abd->init_done)
		return;

	if (pin->index == ABD_PIN_LOG)
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

static struct abd_pin_info *smcdsd_abd_find_pin_info(struct abd_protect *abd, int id, int *is_gpio)
{
	struct abd_pin_info *pin = NULL;
	int i = 0, flag = -EINVAL;

	for (i = 0; i < ABD_PIN_MAX; i++) {
		if (abd->pin[i].gpio && abd->pin[i].gpio == id) {
			pin = &abd->pin[i];
			flag = 1;
			break;
		} else if (abd->pin[i].irq && abd->pin[i].irq == id) {
			pin = &abd->pin[i];
			flag = 0;
			break;
		}
	}

	if (is_gpio)
		*is_gpio = flag;

	return pin;
}

static void __smcdsd_abd_pin_enable(struct abd_protect *abd, struct abd_pin_info *pin, bool on)
{
	struct abd_pin *trace = &pin->p_lcdon;
	struct abd_pin *event = &pin->p_event;
	struct abd_sub_info *sub_info = NULL;
	struct list_head *chain_list = NULL;

	if (!abd || !abd->init_done || !pin)
		return;

	if (!pin->gpio && !pin->irq)
		return;

	if (pin->enable == on)
		return;

	pin->enable = on;

	pin->level = gpio_get_value(pin->gpio);

	dbg_info("%s: on: %d, %s(%3d,%3d,%d) level: %d, count: %d(event: %d) %s\n", __func__,
		on, pin->name, pin->gpio, pin->irq, pin->desc->depth, pin->level, trace->count, event->count,
		(pin->level == pin->active_level) ? (pin->index == ABD_PIN_LOG ? "undefined" : "abnormal") : "normal");

	if (pin->name && !strcmp(pin->name, "pcd"))
		set_frame_bypass(abd, (pin->level == pin->active_level) ? 1 : 0);

	chain_list = on ? &pin->enter_chain : &pin->leave_chain;

	if (pin->level == pin->active_level) {
		smcdsd_abd_save_pin(abd, pin, trace, on);

		if (pin->bug_flag == 1 || pin->bug_flag & IRQ_TYPE_LEVEL_MASK) {
			dbg_info("%s: %s has bug_flag(%d)\n", __func__, pin->name, pin->bug_flag);
			BUG();
		}

		list_for_each_entry(sub_info, chain_list, node) {
			if (sub_info->handler)
				sub_info->handler(pin->gpio, sub_info->chain_data);
		}
	}

	if (pin->level != pin->active_level && on)
		pin->active_depth = 0;

	if (pin->irq)
		smcdsd_abd_pin_enable_irq(pin->irq, on);
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
		__smcdsd_abd_pin_enable(abd, &abd->pin[i], enable);
}

irqreturn_t smcdsd_abd_handler(int irq, void *dev_id)
{
	struct abd_protect *abd = (struct abd_protect *)dev_id;
	struct abd_pin_info *pin = NULL;
	struct abd_pin *trace = NULL;
	struct abd_pin *lcdon = NULL;
	struct abd_sub_info *sub_info = NULL;
	unsigned int i = 0;

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

	dbg_info("%s: %s(%3d,%3d) level: %d, count: %d(lcdon: %d) %s\n", __func__,
		pin->name, pin->gpio, pin->irq, pin->level, trace->count, lcdon->count,
		(pin->level == pin->active_level) ? (pin->index == ABD_PIN_LOG ? "undefined" : "abnormal") : "normal");

	if (pin->bug_flag == 1 || pin->bug_flag & IRQ_TYPE_EDGE_BOTH) {
		dbg_info("%s: %s has bug_flag(%d)\n", __func__, pin->name, pin->bug_flag);
		BUG();
	}

	if (pin->active_level != pin->level)
		goto exit;

	if (i == ABD_PIN_PCD)
		set_frame_bypass(abd, 1);

	list_for_each_entry(sub_info, &pin->event_chain, node) {
		if (sub_info->handler)
			sub_info->handler(irq, sub_info->chain_data);
	}

exit:
	spin_unlock(&abd->slock);

	return IRQ_HANDLED;
}

#if defined(SMCDSD_ABD_MTK_UEVENT) && defined(CONFIG_MTK_FB)
static void __smcdsd_abd_blank(struct abd_protect *abd)
{
	dbg_info("+ %s: noti_uevent\n", __func__);

	noti_uevent_user(&uevent_data, 1);

	dbg_info("- %s\n", __func__);
}
#elif defined(SMCDSD_ABD_MTK_UEVENT) && defined(CONFIG_DRM_MEDIATEK)
static void __smcdsd_abd_blank(struct abd_protect *abd)
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
static int __smcdsd_abd_fb_blank(struct fb_info *info, int blank)
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

static void __smcdsd_abd_blank(struct abd_protect *abd)
{
	struct fb_info *fbinfo = get_fbinfo(abd);

	dbg_info("+ %s\n", __func__);

	__smcdsd_abd_fb_blank(fbinfo, FB_BLANK_POWERDOWN);

	__smcdsd_abd_fb_blank(fbinfo, FB_BLANK_UNBLANK);

	dbg_info("- %s\n", __func__);
}
#elif defined(CONFIG_DRM_MEDIATEK)
static int smcdsd_abd_con_set_dummy(struct abd_protect *abd, unsigned int dummy);

static void __smcdsd_abd_blank(struct abd_protect *abd)
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

static void smcdsd_abd_blank_work(struct work_struct *work)
{
	struct abd_protect *abd = container_of(work, struct abd_protect, blank_work);
	struct fb_info *fbinfo = get_fbinfo(abd);

	dbg_info("+ %s\n", __func__);

	if (fbinfo) {
		if (!lock_fb_info(fbinfo)) {
			dbg_info("%s: fblock is failed\n", __func__);
			return;
		}
		unlock_fb_info(fbinfo);
	}

	__smcdsd_abd_blank(abd);

	dbg_info("- %s\n", __func__);
}

void smcdsd_abd_blank(struct abd_protect *abd)
{
	if (!abd->init_done || !abd->boot_done)
		return;

	dbg_info("%s: blank_flag: %u\n", __func__, abd->blank_flag);

	if (!abd->blank_flag)
		queue_work(abd->blank_workqueue, &abd->blank_work);

	abd->blank_flag = 1;
}

static irqreturn_t smcdsd_abd_refresh_handler(int id, void *dev_id)
{
	struct abd_protect *abd = (struct abd_protect *)dev_id;
	struct abd_pin_info *pin = NULL;
	int is_gpio = 0;

	pin = smcdsd_abd_find_pin_info(abd, id, &is_gpio);
	if (!pin) {
		dbg_info("%s: invalid id(%d)\n", __func__, id);
		return IRQ_HANDLED;
	}

	dbg_info("%s: %s(%3d) %s(%3d,%3d,%d) active_depth: %d\n", __func__,
		is_gpio ? "gpio" : "irq", id, pin->name, pin->gpio, pin->irq, pin->desc->depth, pin->active_depth);

	if (is_gpio)
		pin->active_depth++;

	if (pin->active_depth < 3)	/* todo */
		smcdsd_abd_blank(abd);

	return IRQ_HANDLED;
}

static irqreturn_t smcdsd_abd_detatch_handler(int id, void *dev_id)
{
	struct abd_protect *abd = (struct abd_protect *)dev_id;

	dbg_info("%s: bypass: rw(%d)frame(%d)\n", __func__, get_mipi_rw_bypass(abd), get_frame_bypass(abd));

	set_frame_bypass(abd, 1);
	set_mipi_rw_bypass(abd, 1);

	return IRQ_HANDLED;
}

int smcdsd_abd_pin_register_refresh_handler(struct abd_protect *abd, int irq)
{
	BUG_ON(!abd->init_done);

	return smcdsd_abd_pin_register_handler(abd, irq, smcdsd_abd_refresh_handler, abd);
}

enum {
	CHAIN_TYPE_NONE		= 0x00000000,
	CHAIN_TYPE_LEVEL_ENTER	= 0x00000001,
	CHAIN_TYPE_LEVEL_LEAVE	= 0x00000002,
	CHAIN_TYPE_LEVEL_MASK	= (CHAIN_TYPE_LEVEL_ENTER | CHAIN_TYPE_LEVEL_LEAVE),
	CHAIN_TYPE_EVENT	= 0x00000004,
	CHAIN_TYPE_MAX,
};

static const char *CHAINE_TYPE_NAME[CHAIN_TYPE_MAX] = {
	"CHAIN_TYPE_NONE",
	"CHAIN_TYPE_LEVEL_ENTER",
	"CHAIN_TYPE_LEVEL_LEAVE",
	"CHAIN_TYPE_LEVEL_MASK",
	"CHAIN_TYPE_EVENT",
};

static int chain_name_to_type(const char *name)
{
	int i;

	if (!name)
		return -EINVAL;

	for (i = 0; i < CHAIN_TYPE_MAX; i++) {
		if (STRNEQ(CHAINE_TYPE_NAME[i], name))
			return i;
	}

	return -EFAULT;
}

static struct list_head *chain_type_to_list(struct abd_pin_info *pin, int type)
{
	struct list_head *chain_list = NULL;

	switch (type) {
	case CHAIN_TYPE_LEVEL_ENTER:
		chain_list = &pin->enter_chain;
		break;
	case CHAIN_TYPE_LEVEL_LEAVE:
		chain_list = &pin->leave_chain;
		break;
	case CHAIN_TYPE_EVENT:
		chain_list = &pin->event_chain;
		break;
	}

	return chain_list;
}

static irq_handler_t handler_name_to_handler(const char *string)
{
	irq_handler_t handler = NULL;

	if (STRNEQ("abd_func_refresh", string))
		handler = smcdsd_abd_refresh_handler;
	else if (STRNEQ("abd_func_detatch", string))
		handler = smcdsd_abd_detatch_handler;

	return handler;
}

int smcdsd_abd_pin_register_handler_chain(struct abd_protect *abd,
		int id, irq_handler_t func, void *dev_id, int chain_type)
{
	struct abd_sub_info *sub_info = NULL, *tmp = NULL;
	struct list_head *chain_list = NULL;
	struct abd_pin_info *pin = smcdsd_abd_find_pin_info(abd, id, NULL);

	if (!pin) {
		dbg_info("%s: smcdsd_abd_find_pin_info fail %d\n", __func__, id);
		return -EINVAL;
	}

	if (gpio_get_value(pin->gpio) == pin->active_level) {
		dbg_info("%s: %s(%d) is already %s(%d)\n", __func__, pin->name, gpio_get_value(pin->gpio),
			(pin->active_level) ? "high" : "low", pin->level);
		return -EINVAL;
	}

	chain_list = chain_type_to_list(pin, chain_type);

	list_for_each_entry_safe(sub_info, tmp, chain_list, node) {
		WARN(sub_info->handler == func && sub_info->chain_data == dev_id,
			"%s: already registered handler\n", __func__);
	}

	sub_info = kzalloc(sizeof(struct abd_sub_info), GFP_KERNEL);
	if (!sub_info)
		return -EINVAL;

	sub_info->handler = func;
	sub_info->chain_data = dev_id;
	list_add_tail(&sub_info->node, chain_list);

	dbg_info("%s: %s(%3d,%3d) %s done\n", __func__, pin->name, pin->gpio, pin->irq, CHAINE_TYPE_NAME[chain_type]);

	return 0;
}

static int __of_smcdsd_abd_pin_register_handler_chain(struct abd_protect *abd,
		irq_handler_t func, int gpio, const char *chain_name)
{
	int ret = 0, chain_type;
	struct abd_pin_info *pin = smcdsd_abd_find_pin_info(abd, gpio, NULL);
	char ibuf[80];
	char *pbuf, *token;

	if (!pin) {
		dbg_info("%s: smcdsd_abd_find_pin_info fail gpio: %d\n", __func__, gpio);
		return -EINVAL;
	}

	ret = smcdsd_abd_simple_write_to_buffer(ibuf, sizeof(ibuf), NULL, chain_name, strlen(chain_name));
	if (ret < 0) {
		dbg_info("%s: simple_write_to_buffer fail: %d\n", __func__, ret);
		return ret;
	}

	pbuf = ibuf;
	while ((token = strsep(&pbuf, " ,"))) {
		if (*token == '\0')
			continue;

		chain_type = chain_name_to_type(token);
		if (chain_type < 0)
			continue;

		switch (chain_type) {
		case CHAIN_TYPE_LEVEL_ENTER:
		case CHAIN_TYPE_LEVEL_LEAVE:
		case CHAIN_TYPE_EVENT:
			ret = smcdsd_abd_pin_register_handler_chain(abd, gpio, func, abd, chain_type);
			if (ret < 0)
				dbg_info("%s: smcdsd_abd_pin_register_handler_chain fail %d\n", __func__, ret);
			break;
		}
	}

	return ret;
}

static void of_smcdsd_abd_pin_register_handler_chain(struct abd_protect *abd)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct property *pp;
	irq_handler_t func = NULL;

	const char *info = NULL;
	const char *subinfo = NULL;

	int gpio = 0, i, count;

	pdev = of_find_abd_dt_parent_platform_device();

	np = of_find_smcdsd_board(pdev ? &pdev->dev : NULL);

	for_each_property_of_node(np, pp) {
		if (!pp->name)
			continue;

		if (!strlen(pp->name))
			continue;

		dbg_none("%s: %s\n", __func__, pp->name);

		if (!STRNEQ("abd_func", pp->name))
			continue;

		count = of_property_count_strings(np, pp->name);
		if (count < 0 || !count || count % 2) {
			dbg_info("%s count %d invalid\n", pp->name, count);
			continue;
		}

		count /= 2;

		for (i = 0; i < count; i++) {
			of_property_read_string_index(np, pp->name, i * 2, &info);
			of_property_read_string_index(np, pp->name, i * 2 + 1, &subinfo);

			if (!info || !subinfo)
				continue;

			if (!strlen(info) || !strlen(subinfo))
				continue;

			dbg_none("%s: info(%s) subinfo(%s)\n", __func__, info, subinfo);

			gpio = of_get_named_gpio_flags(np, info, 0, NULL);
			if (!gpio_is_valid(gpio)) {
				dbg_info("%s: gpio_is_valid fail, gpio: %s, %d\n", __func__, info, gpio);
				continue;
			}

			func = handler_name_to_handler(pp->name);
			if (!func)
				continue;

			__of_smcdsd_abd_pin_register_handler_chain(abd, func, gpio, subinfo);
		}
	}
}

int smcdsd_abd_pin_register_handler(struct abd_protect *abd, int id, irq_handler_t func, void *dev_id)
{
	struct mipi_dsi_lcd_common *container = get_lcd_common(0);
	struct abd_pin_info *pin = NULL;
	int chain_type, is_gpio = 0;

	if (!id) {
		dbg_info("%s: id(%d) invalid\n", __func__, id);
		return -EINVAL;
	}

	if (!func) {
		dbg_info("%s: func invalid\n", __func__);
		return -EINVAL;
	}

	if (!container) {
		dbg_info("%s: container invalid\n", __func__);
		return -EINVAL;
	}

	if (!abd)
		abd = &container->abd;

	pin = smcdsd_abd_find_pin_info(abd, id, &is_gpio);
	if (!pin) {
		dbg_info("%s: id(%d) is not in abd\n", __func__, id);
		return -EINVAL;
	}

	chain_type = is_gpio ? CHAIN_TYPE_LEVEL_ENTER : CHAIN_TYPE_EVENT;

	smcdsd_abd_pin_register_handler_chain(abd, id, func, dev_id, chain_type);

	return 0;
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
	}

	return ret;
}

static void smcdsd_abd_con_prepare_dummy_info(struct abd_protect *abd)
{
	struct mipi_dsi_lcd_common *container = get_abd_container_of(abd);

	abd->lcd_driver = container->drv;
}

static int smcdsd_abd_con_fb_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, con_fb_notifier);
	struct fb_event *evdata = data;
	struct abd_pin_info *pin = NULL;
	int fb_blank, gpio_active;

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

	if (!abd->boot_done) {
		dbg_info("%s: boot_done\n", __func__);
		abd->boot_done = 1;
		return NOTIFY_DONE;
	}

	if (fb_blank != FB_BLANK_UNBLANK)
		return NOTIFY_DONE;

	pin = &abd->pin[ABD_PIN_CON];
	gpio_active = (gpio_get_value(pin->gpio) == pin->active_level) ? 1 : 0;

	dbg_info("%s: %s\n", __func__, gpio_active ? "disconnected" : "connected");

	if (IS_EARLY(event))
		return smcdsd_abd_con_set_dummy(abd, gpio_active);
	else if (IS_AFTER(event) && gpio_active)
		return smcdsd_abd_con_set_dummy(abd, gpio_active);

	return NOTIFY_DONE;
}

int smcdsd_abd_con_register(struct abd_protect *abd)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;

	pdev = of_find_abd_dt_parent_platform_device();

	np = of_find_smcdsd_board(pdev ? &pdev->dev : NULL);

	if (!of_find_property(np, "gpio_con", NULL))
		return 0;

	smcdsd_abd_con_prepare_dummy_info(abd);

	abd->con_fb_notifier.priority = smcdsd_nb_priority_max.priority;
	abd->con_fb_notifier.notifier_call = smcdsd_abd_con_fb_notifier_callback;
	smcdsd_register_notifier(&abd->con_fb_notifier);

	if (abd->pin[ABD_PIN_CON].irq) {
		smcdsd_abd_pin_register_handler(abd, abd->pin[ABD_PIN_CON].irq, smcdsd_abd_detatch_handler, abd);
		smcdsd_abd_pin_register_handler(abd, abd->pin[ABD_PIN_CON].irq, smcdsd_abd_refresh_handler, abd);
	}

	return 0;
}

int smcdsd_abd_register_printer(struct abd_protect *abd, int (*show)(struct seq_file *, void *), void *data)
{
	struct mipi_dsi_lcd_common *container = get_lcd_common(0);
	struct abd_sub_info *sub_info = NULL, *tmp = NULL;

	if (!container) {
		dbg_info("%s: container invalid\n", __func__);
		return -EINVAL;
	}

	if (!abd)
		abd = &container->abd;

	if (!abd->init_done)
		return -EINVAL;

	list_for_each_entry_safe(sub_info, tmp, &abd->printer_list, node) {
		WARN(sub_info->show == show && sub_info->chain_data == data,
			"%s: already registered printer\n", __func__);

		sub_info = kzalloc(sizeof(struct abd_sub_info), GFP_KERNEL);
		if (!sub_info)
			break;

		sub_info->show = show;
		sub_info->chain_data = data;
		list_add_tail(&sub_info->node, &abd->printer_list);

		dbg_info("%s: printer is registered\n", __func__);
	}

	return 0;
}

static void __smcdsd_abd_print_pin(struct seq_file *m, struct abd_pin *trace)
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
		abd_printf(m, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / level: %d onoff: %d\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec, log->level, log->onoff);
	}
}

static void smcdsd_abd_print_pin(struct seq_file *m, struct abd_pin_info *pin)
{
	if (!pin->irq && !pin->gpio)
		return;

	if (!pin->p_first.count)
		return;

	abd_printf(m, "[%s]\n", pin->name);

	__smcdsd_abd_print_pin(m, &pin->p_first);
	__smcdsd_abd_print_pin(m, &pin->p_lcdon);
	__smcdsd_abd_print_pin(m, &pin->p_event);
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
		__smcdsd_abd_print_bit(m, log);
	}
}

static int smcdsd_abd_show(struct seq_file *m, void *unused)
{
	struct abd_protect *abd = m->private;
	unsigned int i = 0;
	struct abd_sub_info *sub_info = NULL;

	mutex_lock(&abd->misc_lock);

	abd_printf(m, "==========_SMCDSD_ABD_==========\n");
	abd_printf(m, "bypass rw(%d)frame(%d,%d) lcdtype(%6X)\n",
		get_mipi_rw_bypass(abd), get_boot_frame_bypass(abd), get_frame_bypass(abd), get_boot_lcdtype());

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

	list_for_each_entry(sub_info, &abd->printer_list, node) {
		if (sub_info->show)
			sub_info->show(m, sub_info->chain_data);
	}

	mutex_unlock(&abd->misc_lock);

	return 0;
}

static int smcdsd_abd_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct abd_protect *abd = container_of(dev, struct abd_protect, misc_entry);

	file->private_data = NULL;

	return single_open(file, smcdsd_abd_show, abd);
}

static ssize_t smcdsd_abd_write(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct abd_protect *abd = ((struct seq_file *)f->private_data)->private;
	char ibuf[80] = {0, };
	int ret = 0;

	if (IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP))
		return count;

	if (!IS_ENABLED(CONFIG_SMCDSD_LCD_DEBUG))
		return count;

	ret = smcdsd_abd_simple_write_to_buffer(ibuf, sizeof(ibuf), ppos, user_buf, count);
	if (ret < 0)
		return ret;

	dbg_info("%s: %s\n", __func__, ibuf);

	if (STRNEQ("blank", ibuf)) {
		smcdsd_abd_blank(abd);
	} else if (STRNEQ("con_blank", ibuf)) {
		if (!abd->blank_workqueue)
			smcdsd_abd_con_register(abd);

		smcdsd_abd_blank(abd);
	}

	return count;
}

static const struct file_operations smcdsd_abd_fops = {
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = smcdsd_abd_write,
	.open = smcdsd_abd_open,
};

static void smcdsd_abd_register_fops(struct abd_protect *abd)
{
	int ret = 0;

	abd->misc_entry.minor = MISC_DYNAMIC_MINOR;
	abd->misc_entry.name = "sec_display_debug";
	abd->misc_entry.fops = &smcdsd_abd_fops;
	abd->misc_entry.parent = NULL;

	mutex_init(&abd->misc_lock);

	ret = misc_register(&abd->misc_entry);
	if (ret < 0)
		dbg_info("%s: misc_register fail(%d)\n", __func__, ret);
}

static int smcdsd_abd_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, reboot_notifier);
	unsigned int i = 0;
	struct seq_file *m = NULL;

	dbg_info("++ %s: %lu\n",  __func__, code);

	smcdsd_abd_enable(abd, 0);

	abd_printf(m, "==========_SMCDSD_ABD_==========\n");
	abd_printf(m, "bypass rw(%d)frame(%d,%d) lcdtype(%6X)\n",
		get_mipi_rw_bypass(abd), get_boot_frame_bypass(abd), get_frame_bypass(abd), get_boot_lcdtype());

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

static int of_smcdsd_abd_pin_append_extra_information(struct abd_protect *abd,
		struct abd_pin_info *pin, struct device_node *np, const char *dts_name)
{
	struct of_phandle_args gpiospec;
	struct of_phandle_args out_args;
	int ret, count, extra;
	unsigned int type;

	ret = of_parse_phandle_with_args(np, dts_name, "#gpio-cells", 0, &gpiospec);
	if (ret < 0) {
		dbg_info("%s: of_parse_phandle_with_args fail %d\n", __func__, ret);
		return 0;
	}

	count = of_property_count_u32_elems(np, dts_name);
	if (count < 0 || count > MAX_PHANDLE_ARGS) {
		dbg_info("%s: of_parse_phandle_with_args count(%d)\n", __func__, count);
		return 0;
	}

	if (count <= gpiospec.args_count + 1)
		return 0;

	ret = of_property_read_u32_array(np, dts_name, &out_args.args[0], count);
	if (ret < 0) {
		dbg_info("%s: of_property_read_u32_array fail %d\n", __func__, ret);
		return 0;
	}

	extra = gpiospec.args_count + 1;

	type = out_args.args[extra];

	if (type == IRQ_TYPE_NONE) {
		dbg_info("%s: %s IRQ_TYPE_NONE\n", __func__, dts_name);
		pin->irq = 0;
	}

	return 0;
}

static int of_smcdsd_abd_pin_register_handler(struct abd_protect *abd, struct abd_pin_info *pin,
		char *keyword, irq_handler_t func)
{
	int ret = 0, gpio = 0, to_irq = 0;
	enum of_gpio_flags flags;
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
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

	np = of_find_smcdsd_board(pdev ? &pdev->dev : NULL);

	if (!of_find_property(np, dts_name, NULL))
		goto exit;

	gpio = of_get_named_gpio_flags(np, dts_name, 0, &flags);
	if (!gpio_is_valid(gpio)) {
		dbg_info("%s: gpio_is_valid fail, gpio: %s, %d\n", __func__, dts_name, gpio);
		goto exit;
	}

	dbg_info("%s: found %s(%d) success\n", __func__, dts_name, gpio);

	to_irq = gpio_to_irq(gpio);
	dbg_info("%s: gpio_to_irq, gpio: %d, irq: %d\n", __func__, gpio, to_irq);

	pin->gpio = gpio;
	pin->irq = (to_irq > 0) ? to_irq : 0;

	if (pin->irq)
		of_smcdsd_abd_pin_append_extra_information(abd, pin, np, dts_name);

	pin->desc = (pin->irq) ? irq_to_desc(pin->irq) : kzalloc(sizeof(struct irq_desc), GFP_KERNEL);

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

	if (pin->gpio) {
		INIT_LIST_HEAD(&pin->enter_chain);
		INIT_LIST_HEAD(&pin->leave_chain);
	}

	if (pin->irq) {
		irq_set_irq_type(pin->irq, irqf_type);
		irq_set_status_flags(pin->irq, _IRQ_NOAUTOEN);
		smcdsd_abd_pin_clear_pending_bit(pin->irq);

		if (devm_request_irq(&pdev->dev, pin->irq, func, irqf_type, keyword, abd)) {
			dbg_info("%s: failed to request irq for %s\n", __func__, keyword);
			/* pin->gpio = 0; */
			pin->irq = 0;
			goto exit;
		}

		INIT_LIST_HEAD(&pin->event_chain);
	}

exit:
	return ret;
}

static int smcdsd_abd_pin_early_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, pin_early_notifier);
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case SMCDSD_EARLY_EVENT_BLANK:
	case SMCDSD_EARLY_EVENT_DOZE:
		break;
	default:
		return NOTIFY_DONE;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (!abd->boot_done) {
		dbg_info("%s: boot_done\n", __func__);
		abd->boot_done = 1;
	}

	flush_workqueue(abd->blank_workqueue);
	smcdsd_abd_enable(abd, 0);

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

	if (fb_blank == FB_BLANK_UNBLANK) {
		smcdsd_abd_enable(abd, 1);

		if (abd->blank_flag) {
			dbg_info("%s: blank_flag: %u\n", __func__, abd->blank_flag);
			abd->blank_flag = 0;
		}
	} else if (fb_blank == FB_BLANK_POWERDOWN) {
		for (i = 0; i < ABD_PIN_MAX; i++) {
			pin = &abd->pin[i];
			if (pin && pin->irq)
				smcdsd_abd_pin_clear_pending_bit(pin->irq);
		}
	}

	return NOTIFY_DONE;
}

static void smcdsd_abd_pin_register(struct abd_protect *abd)
{
	spin_lock_init(&abd->slock);

	abd->pin[ABD_PIN_PCD].index = ABD_PIN_PCD;
	abd->pin[ABD_PIN_DET].index = ABD_PIN_DET;
	abd->pin[ABD_PIN_ERR].index = ABD_PIN_ERR;
	abd->pin[ABD_PIN_CON].index = ABD_PIN_CON;
	abd->pin[ABD_PIN_LOG].index = ABD_PIN_LOG;

	of_smcdsd_abd_pin_register_handler(abd, &abd->pin[ABD_PIN_PCD], "pcd", smcdsd_abd_handler);
	of_smcdsd_abd_pin_register_handler(abd, &abd->pin[ABD_PIN_DET], "det", smcdsd_abd_handler);
	of_smcdsd_abd_pin_register_handler(abd, &abd->pin[ABD_PIN_ERR], "err", smcdsd_abd_handler);
	of_smcdsd_abd_pin_register_handler(abd, &abd->pin[ABD_PIN_CON], "con", smcdsd_abd_handler);
	of_smcdsd_abd_pin_register_handler(abd, &abd->pin[ABD_PIN_LOG], "log", smcdsd_abd_handler);

	abd->pin_early_notifier.notifier_call = smcdsd_abd_pin_early_notifier_callback;
	abd->pin_early_notifier.priority = smcdsd_nb_priority_max.priority - 1;
	smcdsd_register_notifier(&abd->pin_early_notifier);

	abd->pin_after_notifier.notifier_call = smcdsd_abd_pin_after_notifier_callback;
	abd->pin_after_notifier.priority = smcdsd_nb_priority_min.priority + 1;
	smcdsd_register_notifier(&abd->pin_after_notifier);

	of_smcdsd_abd_pin_register_handler_chain(abd);
}

static void smcdsd_abd_register_debugfs(struct abd_protect *abd)
{
	struct dentry *abd_debugfs_root = NULL;

	if (IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP))
		return;

	if (!IS_ENABLED(CONFIG_SMCDSD_LCD_DEBUG))
		return;

	if (!IS_ENABLED(CONFIG_DEBUG_FS))
		return;

	dbg_info("%s: ++\n", __func__);

	if (!abd_debugfs_root)
		abd_debugfs_root = debugfs_create_dir("panel", NULL);

	abd->debugfs_root = abd_debugfs_root;
	if (!abd->debugfs_root) {
		dbg_info("%s: debugfs_create_dir fail\n", __func__);
		return;
	}

	debugfs_create_u32("pcd_bug", 0644, abd_debugfs_root, (u32 *)&abd->pin[ABD_PIN_PCD].bug_flag);
	debugfs_create_u32("det_bug", 0644, abd_debugfs_root, (u32 *)&abd->pin[ABD_PIN_DET].bug_flag);
	debugfs_create_u32("err_bug", 0644, abd_debugfs_root, (u32 *)&abd->pin[ABD_PIN_ERR].bug_flag);
	debugfs_create_u32("con_bug", 0644, abd_debugfs_root, (u32 *)&abd->pin[ABD_PIN_CON].bug_flag);
	debugfs_create_u32("log_bug", 0644, abd_debugfs_root, (u32 *)&abd->pin[ABD_PIN_LOG].bug_flag);

	dbg_info("%s: -- entity was registered\n", __func__);
}

static void smcdsd_abd_register(struct abd_protect *abd)
{
	dbg_info("%s: ++\n", __func__);

	abd->u_first.name = abd->f_first.name = abd->b_first.name = "first";
	abd->u_lcdon.name = abd->f_lcdon.name = "lcdon";
	abd->u_event.name = abd->f_event.name = abd->b_event.name = abd->s_event.name = "event";

	abd->reboot_notifier.notifier_call = smcdsd_abd_reboot_notifier;
	register_reboot_notifier(&abd->reboot_notifier);

	INIT_LIST_HEAD(&abd->printer_list);

	INIT_WORK(&abd->blank_work, smcdsd_abd_blank_work);

	abd->blank_workqueue = create_singlethread_workqueue("abd_blank_workqueue");
	if (!abd->blank_workqueue)
		dbg_info("%s: create_singlethread_workqueue fail\n", __func__);

	smcdsd_abd_register_fops(abd);
	smcdsd_abd_register_debugfs(abd);

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

	if (!container) {
		dbg_info("find_container fail\n");
		return 0;
	}

	abd = &container->abd;

	smcdsd_abd_register(abd);

	find_lcd_class_device();

	abd->init_done = 1;

	save_boot_lcd_information(abd);

	if (get_boot_lcdconnected())
		smcdsd_abd_pin_register(abd);
	else
		set_frame_bypass(abd, 1);

	dbg_info("%s: bypass rw(%d)frame(%d,%d) lcdtype(%6X)\n", __func__,
		get_mipi_rw_bypass(abd), get_boot_frame_bypass(abd), get_frame_bypass(abd), get_boot_lcdtype());

	smcdsd_abd_enable(abd, 1);

	return 0;
}
late_initcall_sync(smcdsd_abd_init);

