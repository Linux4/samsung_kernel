/*
 * LED driver for Samsung ET6034A
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt)     "[sec_led] %s: " fmt, __func__
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/leds-et6034a.h>
#include <linux/sec_class.h>
#include <linux/fixp-arith.h>

#define FEATURE_USE_EVENT_STACK
#undef FEATURE_USE_EVENT_FLUSH

#define MAX_LED_NUM	16
#define RGB_MAX	3

#define MAX_BRIGHTNESS_VALUE	0x0000FF
#define MAX_BRIGHTNESS_SET_VALUE	0x7F
#define RGB_MAX_BRIGHTNESS_SET_VALUE	0x5F	/* RGB max value is set to 75% */
#define MAX_BRIGHTNESS_PERCENT 100
#define MAX_BRIGHTNESS_PERCENT_MIN 8

#define RGB_WHITE_LIGHTING_STEP	4
#define MAX_VOLUME_STEP 16

#define LED_BLINK_TIMEOUT		500 /* 500ms */
#define ceil(n, d) ((n)/(d) + ((n)%(d) != 0))
#define PCT_TO_BRI(x) ceil(x * MAX_BRIGHTNESS_VALUE, 100) /* percent to brightness */
#define PCT_TO_BRI_SET(x) ceil(x * MAX_BRIGHTNESS_SET_VALUE, 100) /* percent to brightness */
#define RGB_PCT_TO_BRI_SET(x) ceil(x * RGB_MAX_BRIGHTNESS_SET_VALUE, 100) /* percent to rgb brightness */

#define BIXBY_PROCESS_ON 2

static struct et6034a_led_data *g_led_dev;

enum led_pattern_direction {
	DIR_NONE = 0,
	DIR_NORMAL = 1,
	DIR_REVERSE = 2
};

#define LED_SET_MAX(x, n)	(x = (x | (0xFFUL << (8 * n))))
#define LED_SET_OFF(x, n)	(x = (x & ~(0xFFUL << (8 * n))))
#define LED_SET_DIR(x, n, d)	(x = (x | (d << (4 * n))))
#define LED_SET_DIR_ALL_NORMAL	(0x11111111)
#define LED_SET_DIR_ALL_REVERSE	(0x22222222)

#define TYPE_RGB_LOOP	-1
enum led_pattern_type {
	TYPE_ONCE = 0,
	TYPE_TIMEOUT,
	TYPE_LOOP
};

struct white_led_patterns {
	char name[32];
	int (*func)(struct et6034a_led_data *led_dev, int opt);
	enum led_pattern_type type;
	bool working;
	u32 end_time;
};

struct rgb_blink_patterns {
	char name[32];
	int call_count;
	int off_timming;
	int on_timming;
	int rgb_color;
	int rgb_blink_count;
};

struct led_event_data {
	struct list_head list;
	enum led_patterns pid;
	int option;
};

static void white_led_set_all(struct et6034a_led_data *led_dev, unsigned long value);
static void white_led_set_update(struct et6034a_led_data *led_dev, int led_num, int value);
static void rgb_led_set_update(struct et6034a_led_data *led_dev, int led_num, int value);
static void rgb_led_blink_off(struct et6034a_led_data *led_dev);
static void white_led_blink_off(struct et6034a_led_data *led_dev);

#ifdef FEATURE_USE_EVENT_STACK
static int led_pattern_idle_ready(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_volume_set(struct et6034a_led_data *led_dev, int level);
static int led_pattern_process_animation(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_fade_inout(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_fade_inout2(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_notification(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_timer(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_bixby_process(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_bixby_wakeup(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_bixby_timeout(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_max_brightness_preview(struct et6034a_led_data *led_dev, int opt);
static int led_pattern_max_brightness_pattern(struct et6034a_led_data *led_dev, int opt);

struct white_led_patterns wled_pattern[PATTERNS_MAX] = {
	[IDLED_READY] =	{"IDLED_READY",	led_pattern_idle_ready,	TYPE_ONCE, },
	[WAIT_SETUP] = {"WAIT_SETUP",	NULL,	TYPE_LOOP, },
	[PROCESSING_ANIMATION] = {"PROCESSING_ANIMATION",	led_pattern_process_animation, TYPE_LOOP, },
	[FADE_IN_OUT1] = {"FADE_IN_OUT1",	led_pattern_fade_inout, TYPE_LOOP, },
	[FADE_IN_OUT2] = {"FADE_IN_OUT2",	led_pattern_fade_inout2, TYPE_LOOP, },
	[NOTIFICATION] = {"NOTIFICATION",	led_pattern_notification, TYPE_ONCE, },
	[TIMER_ALRAM] = {"TIMER_ALRAM",	led_pattern_timer, TYPE_LOOP, },
	[MIC_OFF] = {"MIC_OFF",	NULL, TYPE_ONCE, },
	[BIXBY_PROCESSING] = {"BIXBY_PROCESSING",	led_pattern_bixby_process, TYPE_LOOP, },
	[BIXBY_WAKEUP] = {"BIXBY_WAKEUP",	led_pattern_bixby_wakeup, TYPE_LOOP, },
	[VOLUME_LEVEL] = {"VOLUME_LEVEL",	led_pattern_volume_set, TYPE_ONCE, },
	[BIXBY_TIMEOUT] = {"BIXBY_TIMEOUT",	led_pattern_bixby_timeout, TYPE_ONCE, },
	[MAX_BRIGHTNESS_PREVIEW] = {"MAX_BRIGHTNESS__PREVIEW",	led_pattern_max_brightness_preview, TYPE_ONCE, },
	[MAX_BRIGHTNESS_PATTERN] = {"MAX_BRIGHTNESS_PATTERN", led_pattern_max_brightness_pattern, TYPE_ONCE, },
};

struct rgb_blink_patterns rgb_blink_pattern[RGB_BLINK_MAX] = {
	[RGB_BLINK_OFF] = {"RGB_BLINK_OFF", },
	[RGB_BLINK_COLOR] = {"RGB_BLINK_COLOR", 0, 500, 500, 0x0, TYPE_RGB_LOOP},
	[RGB_BLINK_BOOT] = {"RGB_BLINK_BOOT", 0, 500, 500, BLUE_SET, TYPE_RGB_LOOP},
	[RGB_BLINK_BT_CONNECTED] = {"RGB_BLINK_BT_CONNECTED", 0, 100, 100, BLUE_SET, 3},
	[RGB_BLINK_NETWORK_ERR] = {"RGB_BLINK_NETWORK_ERR", 0, 3900, 100, RED_SET, TYPE_RGB_LOOP},
	[RGB_BLINK_ERR] = {"RGB_BLINK_ERR", 0, 500, 500, RED_SET, TYPE_RGB_LOOP},
	[RGB_BLINK_DEBUG_LEVEL] = {"RGB_BLINK_DEBUG_LEVEL", 0, 300, 300, WHITE_SET, 3},
	[RGB_BLINK_POWER_OFF] = {"RGB_BLINK_POWER_OFF", 0, 300, 300, RED_SET, 3},
	[RGB_MIC_OFF] = {"RGB_MIC_OFF", 0, 0, 0, RED_SET, TYPE_RGB_LOOP},
	[RGB_BLINK_LOW_POWER_TA] = {"RGB_LOW_POWER_TA", 0, 500, 500, AMBER_SET, TYPE_RGB_LOOP},
};

enum rgb_patterns rgb_priority[] = {
	RGB_BLINK_POWER_OFF,
	RGB_BLINK_BOOT,
	RGB_BLINK_ERR,
	RGB_BLINK_NETWORK_ERR,
	RGB_BLINK_LOW_POWER_TA,
};

static int rgb_prio_count = ARRAY_SIZE(rgb_priority);

#define IS_VALID_PATTERN(x)	((x >= 0 && x < PATTERNS_MAX) ? 1 : 0)
#define IS_VALID_RGB_PATTERN(x)	((x >= 0 && x < RGB_BLINK_MAX) ? 1 : 0)
#endif

struct et6034a_led_data {
	struct device *et6034a_led_dev;
	struct i2c_client *client;
	struct mutex lock;
	struct work_struct rgb_blink_work;
	struct work_struct white_blink_work;
	struct delayed_work off_work;
#ifndef CONFIG_DRV_SAMSUNG
	struct class *led_class;
#endif
	bool pattern_on;
#ifdef FEATURE_USE_EVENT_STACK
	struct delayed_work pattern_work;
	struct list_head event_list;
	struct mutex pattern_lock;
	struct mutex list_lock;
#endif
	int max_brightness_pct;
	int enable_led;
	int rgb_count;
	int rgb_led_reg[MAX_LED_NUM];
	int white_count;
	int white_led_reg[MAX_LED_NUM];
	int led_num;
	int red_brightness;
	int green_brightness;
	int blue_brightness;
	enum rgb_patterns rgb_pattern_event;
	int rgb_blink_set;
	int rgb_blink_color;
	int mic_off;
	int white_blink_count;
	int white_blink_set;
	int white_blink_on;
	int white_blink_timeout;
	unsigned long white_blink_brightness1;
	unsigned long white_blink_brightness2;
};

static unsigned long brightness_volume[MAX_VOLUME_STEP] = {
	0x0, 0x7373, 0xABAB, 0xFFFF,
	0x3838FFFF, 0x7373FFFF, 0xABABFFFF, 0xFFFFFFFF,
	0x3838FFFFFFF, 0x7373FFFFFFFF, 0xABABFFFFFFFF, 0xFFFFFFFFFFFF,
	0x3838FFFFFFFFFFFF, 0x7373FFFFFFFFFFFF,	0xABABFFFFFFFFFFFF,
	0XFFFFFFFFFFFFFFFF
};

#ifdef FEATURE_USE_EVENT_STACK
static void et6034a_pattern_work(struct work_struct *work)
{
	struct et6034a_led_data *led_dev =
		container_of(work, struct et6034a_led_data, pattern_work.work);
	struct led_event_data *pevent;

	/* pop the latest event */
	while (led_dev->pattern_on && !list_empty(&led_dev->event_list)) {
		mutex_lock(&led_dev->list_lock);
		/* get the latest event like stack */
		pevent = list_last_entry(&led_dev->event_list, typeof(*pevent), list);
		wled_pattern[pevent->pid].working = true;
		mutex_unlock(&led_dev->list_lock);

		if (!IS_VALID_PATTERN(pevent->pid)) {
			pr_info("invalid pattern id\n");
			continue;
		}
		if (wled_pattern[pevent->pid].func) {
			pr_info("%s start\n", wled_pattern[pevent->pid].name);
			wled_pattern[pevent->pid].func(led_dev, pevent->option);
			pr_info("%s done\n", wled_pattern[pevent->pid].name);
		}
		mutex_lock(&led_dev->list_lock);
		if (wled_pattern[pevent->pid].type == TYPE_ONCE ||
				wled_pattern[pevent->pid].type == TYPE_TIMEOUT) {
			wled_pattern[pevent->pid].working = false;
			list_del(&pevent->list);
			kfree(pevent);
		} else {
			wled_pattern[pevent->pid].working = false;
		}
		mutex_unlock(&led_dev->list_lock);
	}
	pr_info("finished\n");
}

static void et6034a_pattern_event_print(struct et6034a_led_data *led_dev)
{
	struct led_event_data *data, *next;

	mutex_lock(&led_dev->list_lock);
	list_for_each_entry_safe_reverse(data, next, &led_dev->event_list, list) {
		pr_info("[%2d] type: %d, working: %d, %s\n", data->pid, wled_pattern[data->pid].type,
				wled_pattern[data->pid].working, wled_pattern[data->pid].name);
	}
	mutex_unlock(&led_dev->list_lock);
}

static void et6034a_pattern_loop_event_remove(struct et6034a_led_data *led_dev, enum led_patterns pid)
{
	struct led_event_data *data, *next;

	if (list_empty(&led_dev->event_list))
		return;

	pr_info("event %d\n", pid);

	mutex_lock(&led_dev->list_lock);
	list_for_each_entry_safe_reverse(data, next, &led_dev->event_list, list) {
		if (data->pid == pid) {
			list_del(&data->list);
			kfree(data);
			pr_info("event %d removed\n", pid);
			break;
		}
	}
	mutex_unlock(&led_dev->list_lock);
}

#ifdef FEATURE_USE_EVENT_FLUSH
static void et6034a_pattern_loop_event_flush(struct et6034a_led_data *led_dev)
{
	struct led_event_data *data, *next;

	if (list_empty(&led_dev->event_list))
		return;

	pr_info("flush loop event\n");

	mutex_lock(&led_dev->list_lock);
	list_for_each_entry_safe(data, next, &led_dev->event_list, list) {
		if (wled_pattern[data->pid].type == TYPE_LOOP) {
			list_del(&data->list);
			kfree(data);
		}
	}
	mutex_unlock(&led_dev->list_lock);
}
#endif

static int et6034a_pattern_proceed(struct et6034a_led_data *led_dev,
			enum led_patterns pid, int opt)
{
	struct led_event_data *event;

	if (!IS_VALID_PATTERN(pid) || !wled_pattern[pid].func) {
		pr_info("Not valid pattern\n");
		return -EINVAL;
	}

	mutex_lock(&led_dev->pattern_lock);

	/* remove the loop event from stack if that is now working */
	if (wled_pattern[pid].type == TYPE_LOOP && opt == 0 && !wled_pattern[pid].working) {
		et6034a_pattern_loop_event_remove(led_dev, pid);
		goto exit;
	}

	/* and stop pattern work */
	led_dev->pattern_on = false;
	cancel_delayed_work_sync(&led_dev->pattern_work);

	/* flush all loop event if new event is loop type */
	if (wled_pattern[pid].type == TYPE_LOOP && opt == 0) {
#ifdef FEATURE_USE_EVENT_FLUSH
		et6034a_pattern_loop_event_flush(led_dev);
#endif
		et6034a_pattern_loop_event_remove(led_dev, pid);
		white_led_set_all(led_dev, 0);
		led_dev->pattern_on = true;
		schedule_delayed_work(&led_dev->pattern_work, 0);
		goto exit;
	}

	event = kzalloc(sizeof(struct led_event_data), GFP_KERNEL);
	if (!event) {
		pr_info("kzalloc error for pattern\n");
		goto exit;
	}

	mutex_lock(&led_dev->list_lock);
	event->pid = pid;
	event->option = opt;
	list_add_tail(&event->list, &led_dev->event_list);
	mutex_unlock(&led_dev->list_lock);

	led_dev->pattern_on = true;
	schedule_delayed_work(&led_dev->pattern_work, 0);

exit:
	mutex_unlock(&led_dev->pattern_lock);

	return 0;
}

static enum rgb_patterns et6034a_rgb_get_higher_priority(void)
{
	int i;
	enum rgb_patterns id = RGB_BLINK_OFF;

	for (i = 0; i < rgb_prio_count; i++) {
		if (rgb_blink_pattern[rgb_priority[i]].call_count > 0) {
			id = rgb_priority[i];
			break;
		}
	}

	return id;
}

static int et6034a_rgb_pattern_proceed(struct et6034a_led_data *led_dev,
			enum rgb_patterns pid, int opt)
{
	if (!IS_VALID_RGB_PATTERN(pid)) {
		pr_info("Not valid pattern\n");
		return -EINVAL;
	}

	led_dev->rgb_pattern_event = pid;
	switch (pid) {
	case RGB_BLINK_OFF:
	case RGB_BLINK_COLOR:
		pr_info("not support this pattern\n");
		break;
	case RGB_MIC_OFF:
		rgb_led_blink_off(led_dev);
		break;
	default:
		led_dev->rgb_blink_color = rgb_blink_pattern[led_dev->rgb_pattern_event].rgb_color;
		led_dev->rgb_blink_set = true;
		schedule_work(&led_dev->rgb_blink_work);
	}

	return 0;
}
#endif

#define TIME_DIV_MS	10
#define TIME_PATTERN_INTERVAL_MS	5
#define TIME_MULTIPLE	10 /* for more detailed division */

static inline void pattern_msleep(struct et6034a_led_data *led_dev, u32 msecs)
{
	u32 count = msecs / TIME_DIV_MS;
	u32 mod_us = (msecs % TIME_DIV_MS) * 1000;
	u32 div_us =  TIME_DIV_MS * 1000;
	int i;

	for (i = 0; i < count && led_dev->pattern_on; i++)
		usleep_range(div_us, div_us + 1);

	if (led_dev->pattern_on)
		usleep_range(mod_us, mod_us + 1);
}

static inline void rgb_blink_msleep(struct et6034a_led_data *led_dev, u32 msecs)
{
	u32 count = msecs / TIME_DIV_MS;
	u32 mod_us = (msecs % TIME_DIV_MS) * 1000;
	u32 div_us =  TIME_DIV_MS * 1000;
	int i;

	for (i = 0; i < count && led_dev->rgb_pattern_event; i++)
		usleep_range(div_us, div_us + 1);

	if (led_dev->rgb_pattern_event != RGB_BLINK_OFF)
		usleep_range(mod_us, mod_us + 1);
}

static int et6034a_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct et6034a_led_data *led_dev = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&led_dev->lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&led_dev->lock);
	if (ret < 0)
		pr_err("reg(0x%x), ret(%d)\n", reg, ret);
	return ret;
}

static void rgb_led_set_brightness(struct et6034a_led_data *led_dev, int led_num)
{
	et6034a_write_reg(led_dev->client,
		led_dev->rgb_led_reg[led_num * 3 + LED_RED], led_dev->red_brightness);
	et6034a_write_reg(led_dev->client,
		led_dev->rgb_led_reg[led_num * 3 + LED_GREEN], led_dev->green_brightness);
	et6034a_write_reg(led_dev->client,
		led_dev->rgb_led_reg[led_num * 3 + LED_BLUE], led_dev->blue_brightness);
}

static void rgb_led_set_update(struct et6034a_led_data *led_dev, int led_num, int value)
{
	int value_temp, brightness;

	value_temp = (value & RED_SET) >> 16;
	brightness = ceil(((value_temp & MAX_BRIGHTNESS_VALUE) * RGB_PCT_TO_BRI_SET(led_dev->max_brightness_pct)),
			MAX_BRIGHTNESS_VALUE);
	led_dev->red_brightness = brightness;

	value_temp = (value & GREEN_SET) >> 8;
	brightness = ceil(((value_temp & MAX_BRIGHTNESS_VALUE) * RGB_PCT_TO_BRI_SET(led_dev->max_brightness_pct)),
			MAX_BRIGHTNESS_VALUE);
	led_dev->green_brightness = brightness;

	value_temp = value & BLUE_SET;
	brightness = ceil(((value_temp & MAX_BRIGHTNESS_VALUE) * RGB_PCT_TO_BRI_SET(led_dev->max_brightness_pct)),
			MAX_BRIGHTNESS_VALUE);
	led_dev->blue_brightness = brightness;

	rgb_led_set_brightness(led_dev, led_num);
}

static void white_led_set_brightness(struct et6034a_led_data *led_dev, int led_num, int brightness)
{
	if (led_num < 0 || led_num >= led_dev->white_count) {
		pr_err("et6034 out of led count %d\n", led_num);
		return;
	}

	et6034a_write_reg(led_dev->client,
			led_dev->white_led_reg[led_num], brightness);
}

static void white_led_set_update(struct et6034a_led_data *led_dev, int led_num, int value)
{
	int brightness = 0;

	if (value == 0)
		brightness = 0;
	else
		brightness = ceil(((value & MAX_BRIGHTNESS_VALUE) * PCT_TO_BRI_SET(led_dev->max_brightness_pct)),
				MAX_BRIGHTNESS_VALUE);

	white_led_set_brightness(led_dev, led_num, brightness);
}

static void white_led_set_all(struct et6034a_led_data *led_dev, unsigned long value)
{
	int i;
	int brightness = 0;

	for (i = 0 ; i < led_dev->white_count ; i++) {
		brightness = value & MAX_BRIGHTNESS_VALUE;
		white_led_set_update(led_dev, i, brightness);
		value = value >> 8;
	}
}

/* brightness: 0 ~ 0xFF */
static void white_led_all_brightness_change_linear(struct et6034a_led_data *led_dev,
			u32 brightness_start, u32 brightness_end, u32 led_dir, u32 msecs)
{
	int i, j;
	int count = msecs / TIME_PATTERN_INTERVAL_MS;
	int bri_gap;
	int brightness = brightness_start * TIME_MULTIPLE;
	int brightness_rev = brightness_end * TIME_MULTIPLE;
	u32 sleep_gap, sleep_us = TIME_PATTERN_INTERVAL_MS * 1000;
	ktime_t start_t;

	bri_gap = (brightness_end - brightness_start) * TIME_MULTIPLE / count;

	pr_debug("dir: %u, count: %d, gap: %d\n", led_dir, count, bri_gap);
	for (i = 0; i < count && led_dev->pattern_on; i++) {
		start_t = ktime_get();
		for (j = 0; j < led_dev->white_count; j++) {
			if (((led_dir >> (4 * j)) & 0xF) == DIR_NORMAL)
				white_led_set_update(led_dev, j, brightness / TIME_MULTIPLE);
			else if (((led_dir >> (4 * j)) & 0xF) == DIR_REVERSE)
				white_led_set_update(led_dev, j, brightness_rev / TIME_MULTIPLE);
		}
		brightness += bri_gap;
		brightness_rev -= bri_gap;

		sleep_gap = sleep_us - (ktime_get() - start_t) / 1000;
		if (sleep_gap > sleep_us)
			sleep_gap = sleep_us;
		usleep_range(sleep_gap, sleep_gap + 1);
	}
	for (j = 0; j < led_dev->white_count; j++) {
		if (((led_dir >> (4 * j)) & 0xF) == DIR_NORMAL)
			white_led_set_update(led_dev, j, brightness_end);
		else if (((led_dir >> (4 * j)) & 0xF) == DIR_REVERSE)
			white_led_set_update(led_dev, j, brightness_start);
	}
}

static int brightness_to_sin(int brightness, int max_brightness)
{
	int angle;
	s64 sinv = 0;

	angle = brightness * 90 / max_brightness;
	/* angle should not be zero if brightness is not zero */
	if (brightness != 0 && angle == 0)
		angle = 1;

	sinv = (s64)fixp_sin32(angle);

	/* The returned value ranges from -0x7fffffff to +0x7fffffff. */
	sinv = sinv * max_brightness / 0x7fffffff;

	return (int)sinv;
}

/* brightness: 0 ~ 0xFF,  brightness_end should be bigger than brightness_start */
static void white_led_all_brightness_change_sin(struct et6034a_led_data *led_dev,
			u32 brightness_start, u32 brightness_end, u32 led_dir, u32 msecs)
{
	int i, j;
	int count = msecs / TIME_PATTERN_INTERVAL_MS;
	int bri_gap;
	int brightness = brightness_start * TIME_MULTIPLE;
	int brightness_rev = brightness_end * TIME_MULTIPLE;
	u32 sleep_gap, sleep_us = TIME_PATTERN_INTERVAL_MS * 1000;
	int sin_v = 0;
	ktime_t start_t;

	bri_gap = (brightness_end - brightness_start) * TIME_MULTIPLE / count;

	pr_debug("dir: %u, count: %d, gap: %d\n", led_dir, count, bri_gap);
	for (i = 0; i < count && led_dev->pattern_on; i++) {
		start_t = ktime_get();
		for (j = 0; j < led_dev->white_count; j++) {
			if (((led_dir >> (4 * j)) & 0xF) == DIR_NORMAL) {
				sin_v = brightness_to_sin(brightness / TIME_MULTIPLE, MAX_BRIGHTNESS_VALUE);
				white_led_set_update(led_dev, j, sin_v);
			} else if (((led_dir >> (4 * j)) & 0xF) == DIR_REVERSE) {
				sin_v = brightness_to_sin(brightness_rev / TIME_MULTIPLE, MAX_BRIGHTNESS_VALUE);
				white_led_set_update(led_dev, j, sin_v);
			}
		}
		brightness += bri_gap;
		brightness_rev -= bri_gap;

		sleep_gap = sleep_us - (ktime_get() - start_t) / 1000;
		if (sleep_gap > sleep_us)
			sleep_gap = sleep_us;
		usleep_range(sleep_gap, sleep_gap + 1);
	}
	for (j = 0; j < led_dev->white_count; j++) {
		if (((led_dir >> (4 * j)) & 0xF) == DIR_NORMAL)
			white_led_set_update(led_dev, j, brightness_end);
		else if (((led_dir >> (4 * j)) & 0xF) == DIR_REVERSE)
			white_led_set_update(led_dev, j, brightness_start);
	}
}

static int brightness_to_quart(int brightness, int max_brightness)
{
	u64 quart;
	u64 bri;
	u64 max_quart;

	/* quart brightness = brightness^4 * max_brightness / max_brightness^4 */
	quart = (u64)brightness * (u64)brightness * (u64)brightness * (u64)brightness;
	max_quart = (u64)max_brightness * (u64)max_brightness * (u64)max_brightness;
	bri = quart / max_quart;

	return (int)bri;
}

/* brightness: 0 ~ 0xFF,  brightness_end should be bigger than brightness_start */
static void white_led_all_brightness_change_quart(struct et6034a_led_data *led_dev,
			u32 brightness_start, u32 brightness_end, u32 led_dir, u32 msecs)
{
	int i, j;
	int count = msecs / TIME_PATTERN_INTERVAL_MS;
	int diff, bri_gap, gaps = 0;
	u32 sleep_gap, sleep_us = TIME_PATTERN_INTERVAL_MS * 1000;
	int quart = 0;
	ktime_t start_t;

	diff = brightness_end - brightness_start;
	bri_gap = diff * TIME_MULTIPLE / count;

	pr_debug("dir: %u, count: %d, gap: %d\n", led_dir, count, bri_gap);
	for (i = 0; i < count && led_dev->pattern_on; i++) {
		start_t = ktime_get();
		for (j = 0; j < led_dev->white_count; j++) {
			if (((led_dir >> (4 * j)) & 0xF) == DIR_NORMAL) {
				quart = brightness_to_quart(gaps / TIME_MULTIPLE, diff);
				white_led_set_update(led_dev, j, brightness_start + quart);
			} else if (((led_dir >> (4 * j)) & 0xF) == DIR_REVERSE) {
				quart = brightness_to_quart((gaps / TIME_MULTIPLE), diff);
				white_led_set_update(led_dev, j, brightness_end - quart);
			}
		}
		gaps += bri_gap;

		sleep_gap = sleep_us - (ktime_get() - start_t) / 1000;
		if (sleep_gap > sleep_us)
			sleep_gap = sleep_us;
		usleep_range(sleep_gap, sleep_gap + 1);
	}
	for (j = 0; j < led_dev->white_count; j++) {
		if (((led_dir >> (4 * j)) & 0xF) == DIR_NORMAL)
			white_led_set_update(led_dev, j, brightness_end);
		else if (((led_dir >> (4 * j)) & 0xF) == DIR_REVERSE)
			white_led_set_update(led_dev, j, brightness_start);
	}
}

static int led_pattern_idle_ready(struct et6034a_led_data *led_dev, int opt)
{
	u32 led_dir;

	pr_info("\n", __func__);

	white_led_set_all(led_dev, 0x0); /* off first */
	pattern_msleep(led_dev, 40);
	if (!led_dev->pattern_on)
		goto exit_now;

	led_dir = LED_SET_DIR_ALL_NORMAL;
	white_led_all_brightness_change_linear(led_dev,
			0, PCT_TO_BRI(80), led_dir, 265); /* 0 ~ 80% */
	pattern_msleep(led_dev, 83);
	if (!led_dev->pattern_on)
		goto exit_now;

	led_dir = LED_SET_DIR_ALL_REVERSE;
	white_led_all_brightness_change_linear(led_dev,
			PCT_TO_BRI(10), PCT_TO_BRI(80), led_dir, 80); /* 80 ~ 10% */
	pattern_msleep(led_dev, 175);
	if (!led_dev->pattern_on)
		goto exit_now;

	led_dir = LED_SET_DIR_ALL_NORMAL;
	white_led_all_brightness_change_linear(led_dev,
			PCT_TO_BRI(10), MAX_BRIGHTNESS_VALUE, led_dir, 920); /* 10 ~ 100% */
	pattern_msleep(led_dev, 1358);
	if (!led_dev->pattern_on)
		goto exit_now;

	led_dir = LED_SET_DIR_ALL_REVERSE;
	white_led_all_brightness_change_linear(led_dev,
			0, MAX_BRIGHTNESS_VALUE, led_dir, 510); /* 100 ~ 0% */
	pattern_msleep(led_dev, 1075);

exit_now:
	white_led_set_all(led_dev, BRIGHTNESS_OFF);

	return 0;
}

static int led_pattern_process_animation(struct et6034a_led_data *led_dev, int opt)
{
	u32 led_dir;
	u32 pattern_rate = 110;
	int fadeout_seq[14] = {-1, 0, 1, 2, 3, 4, 5, -1, 7, 6, 5, 4, 3, 2};
	int fadein_seq[14] = {2, 3, 4, 5, 6, 7, -1, 5, 4, 3, 2, 1, 0, -1};

	if (opt == BIXBY_PROCESS_ON)
		pattern_rate = 44;

	/* off all first */
	white_led_set_all(led_dev, 0);

	/* 0 ~ 200 */
	led_dir = 0;
	LED_SET_DIR(led_dir, 0, DIR_NORMAL);
	white_led_all_brightness_change_linear(led_dev,
			0, MAX_BRIGHTNESS_VALUE, led_dir, pattern_rate);

	/* 200 ~ 400 */
	led_dir = 0;
	LED_SET_DIR(led_dir, 1, DIR_NORMAL);
	white_led_all_brightness_change_linear(led_dev,
			0, MAX_BRIGHTNESS_VALUE, led_dir, pattern_rate);

	while (led_dev->pattern_on) {
		int i;

		for (i = 0; i < 14 && led_dev->pattern_on; i++) {
			led_dir = 0;
			if (fadein_seq[i] >= 0)
				LED_SET_DIR(led_dir, fadein_seq[i], DIR_NORMAL);
			if (fadeout_seq[i] >= 0)
				LED_SET_DIR(led_dir, fadeout_seq[i], DIR_REVERSE);
			white_led_all_brightness_change_linear(led_dev,
				0, MAX_BRIGHTNESS_VALUE, led_dir, pattern_rate);
		}

		if (opt == BIXBY_PROCESS_ON)
			pattern_rate = 55;
	}

	white_led_set_all(led_dev, BRIGHTNESS_OFF);

	return 0;
}

static int led_pattern_fade_inout(struct et6034a_led_data *led_dev, int opt)
{
	u32 led_dir;

	pr_info("\n", __func__);

	white_led_set_all(led_dev, 0xFFFFFFFFFFFFFFFFUL); /* on first */
	pattern_msleep(led_dev, 13);
	if (!led_dev->pattern_on)
		goto exit_now;

	while (led_dev->pattern_on) {
		led_dir = LED_SET_DIR_ALL_REVERSE;
		white_led_all_brightness_change_linear(led_dev,
			PCT_TO_BRI(10), MAX_BRIGHTNESS_VALUE, led_dir, 367); /* 100 ~ 10% */
		pattern_msleep(led_dev, 300);
		if (!led_dev->pattern_on)
			break;

		led_dir = LED_SET_DIR_ALL_NORMAL;
		white_led_all_brightness_change_sin(led_dev,
			PCT_TO_BRI(10), MAX_BRIGHTNESS_VALUE, led_dir, 200); /* 10 ~ 100% */
		pattern_msleep(led_dev, 33);
		if (!led_dev->pattern_on)
			break;
	}

exit_now:
	white_led_set_all(led_dev, BRIGHTNESS_OFF);

	return 0;
}

static int led_pattern_fade_inout2(struct et6034a_led_data *led_dev, int opt)
{
	u64 brightness;
	u32 led_dir;

	/* 0 ~ 100 */
	brightness = BRIGHTNESS_OFF;
	white_led_set_all(led_dev, brightness);

	led_dir = 0;
	LED_SET_DIR(led_dir, 3, DIR_NORMAL);
	LED_SET_DIR(led_dir, 4, DIR_NORMAL);
	white_led_all_brightness_change_sin(led_dev, 0x0, MAX_BRIGHTNESS_VALUE, led_dir, 100);
	if (!led_dev->pattern_on)
		goto exit_now;

	/* 100 ~ 200 */
	led_dir = 0;
	LED_SET_DIR(led_dir, 2, DIR_NORMAL);
	LED_SET_DIR(led_dir, 5, DIR_NORMAL);
	white_led_all_brightness_change_sin(led_dev, 0x0, MAX_BRIGHTNESS_VALUE, led_dir, 100);
	if (!led_dev->pattern_on)
		goto exit_now;

	/* 200 ~ 300 */
	led_dir = 0;
	LED_SET_DIR(led_dir, 1, DIR_NORMAL);
	LED_SET_DIR(led_dir, 6, DIR_NORMAL);
	white_led_all_brightness_change_sin(led_dev, 0x0, MAX_BRIGHTNESS_VALUE, led_dir, 100);
	if (!led_dev->pattern_on)
		goto exit_now;

	/* 300 ~ 400 */
	led_dir = 0;
	LED_SET_DIR(led_dir, 0, DIR_NORMAL);
	LED_SET_DIR(led_dir, 7, DIR_NORMAL);
	white_led_all_brightness_change_sin(led_dev, 0x0, MAX_BRIGHTNESS_VALUE, led_dir, 100);
	if (!led_dev->pattern_on)
		goto exit_now;

	/* 400 ~ 500 */
	pattern_msleep(led_dev, 100);
	if (!led_dev->pattern_on)
		goto exit_now;

	/* 500 ~ 900 */
	led_dir = LED_SET_DIR_ALL_REVERSE;
	white_led_all_brightness_change_linear(led_dev,
			PCT_TO_BRI(10), MAX_BRIGHTNESS_VALUE, led_dir, 400);
	if (!led_dev->pattern_on)
		goto exit_now;

	/* 900 ~ 1000 */
	pattern_msleep(led_dev, 00);
	if (!led_dev->pattern_on)
		goto exit_now;

	while (led_dev->pattern_on) {
		/* 1000 ~ 1100 */
		led_dir = 0;
		LED_SET_DIR(led_dir, 3, DIR_NORMAL);
		LED_SET_DIR(led_dir, 4, DIR_NORMAL);
		white_led_all_brightness_change_sin(led_dev, PCT_TO_BRI(10),
				MAX_BRIGHTNESS_VALUE, led_dir, 100);
		if (!led_dev->pattern_on)
			break;

		/* 1100 ~ 1200 */
		led_dir = 0;
		LED_SET_DIR(led_dir, 2, DIR_NORMAL);
		LED_SET_DIR(led_dir, 5, DIR_NORMAL);
		white_led_all_brightness_change_sin(led_dev, PCT_TO_BRI(10),
				MAX_BRIGHTNESS_VALUE, led_dir, 100);
		if (!led_dev->pattern_on)
			break;

		/* 1200 ~ 1300 */
		led_dir = 0;
		LED_SET_DIR(led_dir, 1, DIR_NORMAL);
		LED_SET_DIR(led_dir, 6, DIR_NORMAL);
		white_led_all_brightness_change_sin(led_dev, PCT_TO_BRI(10),
				MAX_BRIGHTNESS_VALUE, led_dir, 100);
		if (!led_dev->pattern_on)
			break;

		/* 1300 ~ 1400 */
		led_dir = 0;
		LED_SET_DIR(led_dir, 0, DIR_NORMAL);
		LED_SET_DIR(led_dir, 7, DIR_NORMAL);
		white_led_all_brightness_change_sin(led_dev, PCT_TO_BRI(10),
				MAX_BRIGHTNESS_VALUE, led_dir, 100);
		if (!led_dev->pattern_on)
			break;

		/* 1400 ~ 1500 */
		pattern_msleep(led_dev, 100);
		if (!led_dev->pattern_on)
			break;

		/* 1500 ~ 1900 */
		led_dir = LED_SET_DIR_ALL_REVERSE;
		white_led_all_brightness_change_linear(led_dev,
				PCT_TO_BRI(10), MAX_BRIGHTNESS_VALUE, led_dir, 400);

		/* 1900 ~ 2000 */
		pattern_msleep(led_dev, 100);
		if (!led_dev->pattern_on)
			break;
	}

exit_now:
	white_led_set_all(led_dev, BRIGHTNESS_OFF);

	return 0;
}

static int led_pattern_notification(struct et6034a_led_data *led_dev, int opt)
{
	if (!opt)
		return 0;

	white_led_set_all(led_dev, BRIGHTNESS_OFF);
	pattern_msleep(led_dev, 500);
	if (!led_dev->pattern_on)
		goto exit_now;

	white_led_set_all(led_dev, 0x0000FFFFFFFF0000UL);
	pattern_msleep(led_dev, 500);

exit_now:
	white_led_set_all(led_dev, BRIGHTNESS_OFF);
	return 0;
}

static int led_pattern_timer(struct et6034a_led_data *led_dev, int opt)
{
	while (led_dev->pattern_on) {
		white_led_set_all(led_dev, 0x0000FFFF0000FFFFUL);
		pattern_msleep(led_dev, opt);
		if (!led_dev->pattern_on)
			break;

		white_led_set_all(led_dev, 0xFFFF0000FFFF0000UL);
		pattern_msleep(led_dev, opt);
		if (!led_dev->pattern_on)
			break;
	}

	white_led_set_all(led_dev, BRIGHTNESS_OFF);

	return 0;
}

static int led_pattern_bixby_process(struct et6034a_led_data *led_dev, int opt)
{
	led_pattern_process_animation(led_dev, BIXBY_PROCESS_ON);
	return 0;
}

static int led_pattern_bixby_wakeup(struct et6034a_led_data *led_dev, int opt)
{
	white_led_set_all(led_dev, 0xFFFFFFFFFFFFFFFFUL);

	while (led_dev->pattern_on)
		pattern_msleep(led_dev, 1000);

	return 0;
}

static int led_pattern_bixby_timeout(struct et6034a_led_data *led_dev, int opt)
{
	u32 led_dir;

	led_dir = LED_SET_DIR_ALL_REVERSE;
	white_led_all_brightness_change_quart(led_dev,
			PCT_TO_BRI(10), MAX_BRIGHTNESS_VALUE, led_dir, 150); /* 100 ~ 10% */
	pattern_msleep(led_dev, 20);
	if (!led_dev->pattern_on)
		goto exit_now;

	led_dir = LED_SET_DIR_ALL_NORMAL;
	white_led_all_brightness_change_quart(led_dev,
			PCT_TO_BRI(10), MAX_BRIGHTNESS_VALUE, led_dir, 150); /* 10 ~ 100% */
	pattern_msleep(led_dev, 500);
	if (!led_dev->pattern_on)
		goto exit_now;

	led_dir = LED_SET_DIR_ALL_REVERSE;
	white_led_all_brightness_change_sin(led_dev,
			0, MAX_BRIGHTNESS_VALUE, led_dir, 1500); /* 100 ~ 0% */

exit_now:
	white_led_set_all(led_dev, BRIGHTNESS_OFF);

	return 0;
}

static int led_pattern_max_brightness_preview(struct et6034a_led_data *led_dev, int opt)
{
	int i;
	int brightness;

	if (opt < MAX_BRIGHTNESS_PERCENT_MIN || opt > MAX_BRIGHTNESS_PERCENT) {
		pr_err("led max brightness percent out of value %d\n", opt);
		return -EINVAL;
	}

	brightness = ceil((MAX_BRIGHTNESS_VALUE * PCT_TO_BRI_SET(opt)),	MAX_BRIGHTNESS_VALUE);

	if (led_dev->rgb_pattern_event == RGB_MIC_OFF) {
		led_dev->red_brightness = brightness;
		led_dev->green_brightness = 0;
		led_dev->blue_brightness = 0;
		rgb_led_set_brightness(led_dev, 0);
	}

	for (i = 0; i < led_dev->white_count; i++)
		white_led_set_brightness(led_dev, i, brightness);

	pattern_msleep(led_dev, 3 * 60 * 1000);
	white_led_set_all(led_dev, BRIGHTNESS_OFF);

	if (led_dev->rgb_pattern_event == RGB_MIC_OFF)
		rgb_led_set_update(led_dev, 0, RED_SET);

	return 0;
}

static int led_pattern_max_brightness_pattern(struct et6034a_led_data *led_dev, int opt)
{
	int i, j;
	int brightness, brightness_low;

	if (opt < MAX_BRIGHTNESS_PERCENT_MIN || opt > MAX_BRIGHTNESS_PERCENT) {
		pr_err("led max brightness percent out of value %d\n", opt);
		return -EINVAL;
	}
	pr_info("max brightness pattern %d\n", opt);

	brightness = PCT_TO_BRI(opt);
	brightness_low = PCT_TO_BRI(opt * 42 / 100);

	for (i = 0; i < 3; i++) {
		for (j = 0 ; j < led_dev->white_count ; j++)
			white_led_set_update(led_dev, j, brightness);
		pattern_msleep(led_dev, 100);
		if (!led_dev->pattern_on)
			goto exit_now;

		for (j = 0 ; j < led_dev->white_count ; j++)
			white_led_set_update(led_dev, j, brightness_low);
		pattern_msleep(led_dev, 100);
		if (!led_dev->pattern_on)
			goto exit_now;
	}
exit_now:
	white_led_set_all(led_dev, BRIGHTNESS_OFF);
	return 0;
}

static void rgb_led_blink_off(struct et6034a_led_data *led_dev)
{
	pr_info("%s\n", rgb_blink_pattern[led_dev->rgb_pattern_event].name);

	if (led_dev->rgb_pattern_event != RGB_BLINK_OFF) {
		led_dev->rgb_pattern_event = RGB_BLINK_OFF;
		led_dev->rgb_blink_set = false;
		cancel_work_sync(&led_dev->rgb_blink_work);
	}
	rgb_led_set_update(led_dev, 0, BRIGHTNESS_OFF);

	if (led_dev->mic_off) {
		led_dev->rgb_pattern_event = RGB_MIC_OFF;
		rgb_led_set_update(led_dev, 0, RED_SET);
	}

}

static void white_led_blink_off(struct et6034a_led_data *led_dev)
{
	if (led_dev->pattern_on) {
		led_dev->pattern_on = false;
		cancel_delayed_work_sync(&led_dev->pattern_work);
		white_led_set_all(led_dev, BRIGHTNESS_OFF);
	}
}

static void led_off_work(struct work_struct *work)
{
	struct et6034a_led_data *led_dev =
		container_of(work, struct et6034a_led_data, off_work.work);

	pr_info("led off\n");
	white_led_set_all(led_dev, BRIGHTNESS_OFF);
}

static void rgb_led_blink_work(struct work_struct *work)
{
	struct et6034a_led_data *led_dev =
		container_of(work, struct et6034a_led_data, rgb_blink_work);
	int brightness_set;
	int sleep_time;
	int count;
	enum rgb_patterns prio;

	count = rgb_blink_pattern[led_dev->rgb_pattern_event].rgb_blink_count;

	pr_info("%s start\n", rgb_blink_pattern[led_dev->rgb_pattern_event].name);
	while (led_dev->rgb_pattern_event) {
		if (count == 0) {
			rgb_blink_pattern[led_dev->rgb_pattern_event].call_count--;
			led_dev->rgb_pattern_event = RGB_BLINK_OFF;
		}
		if (led_dev->rgb_blink_set) {
			brightness_set = 0;
			sleep_time = rgb_blink_pattern[led_dev->rgb_pattern_event].off_timming;
			led_dev->rgb_blink_set = false;
		} else {
			if (count > 0)
				count--;
			brightness_set = led_dev->rgb_blink_color;
			sleep_time = rgb_blink_pattern[led_dev->rgb_pattern_event].on_timming;
			led_dev->rgb_blink_set = true;
		}

		rgb_led_set_update(led_dev, 0, brightness_set);

		rgb_blink_msleep(led_dev, sleep_time);
	}

	prio = et6034a_rgb_get_higher_priority();
	pr_info("%s done / next %s\n", rgb_blink_pattern[led_dev->rgb_pattern_event].name,
			rgb_blink_pattern[prio].name);
	if (prio != RGB_BLINK_OFF)
		et6034a_rgb_pattern_proceed(led_dev, prio, LED_ON);
	else
		rgb_led_blink_off(led_dev);
}

static void white_led_blink_work(struct work_struct *work)
{
	struct et6034a_led_data *led_dev =
		container_of(work, struct et6034a_led_data, white_blink_work);
	unsigned long brightness_set;

	while (led_dev->white_blink_on) {
		if (led_dev->white_blink_set) {
			brightness_set = led_dev->white_blink_brightness1;
			led_dev->white_blink_set = false;
		} else {
			brightness_set = led_dev->white_blink_brightness2;
			led_dev->white_blink_set = true;
		}

		if (led_dev->white_blink_count > 0) {
			led_dev->white_blink_count--;
		} else if (led_dev->white_blink_count == 0) {
			led_dev->white_blink_count = -1;
			led_dev->white_blink_on = false;
			brightness_set = 0;
		}

		white_led_set_all(led_dev, brightness_set);
		msleep(led_dev->white_blink_timeout);
	}
}

/*-----------------------------------------------------------------------------
 * sysfs group support
 */
static ssize_t led_patterns_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
#ifdef FEATURE_USE_EVENT_STACK
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);

	et6034a_pattern_event_print(led_dev);
#endif
	return 0;
}

static ssize_t led_patterns_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);
	ssize_t ret;
	int val,	val2;

	ret = sscanf(buf, "%d %d", &val, &val2);
	if (ret != 2) {
		pr_err("fail to get led pattern info : %d\n", ret);
		return -EINVAL;
	}
	if (!IS_VALID_PATTERN(val))
		return -EINVAL;

	pr_info("(MAX %d) setting pattern %d %d\n", led_dev->max_brightness_pct, val, val2);
#ifdef FEATURE_USE_EVENT_STACK
	if (!et6034a_pattern_proceed(led_dev, val, val2))
		return size;
#endif

	return size;
}

#ifdef FEATURE_USE_EVENT_STACK
static int led_pattern_volume_set(struct et6034a_led_data *led_dev, int level)
{
	pr_info("%d\n", level);

	if (level >= MAX_VOLUME_STEP)
		level = MAX_VOLUME_STEP - 1;

	white_led_set_all(led_dev, brightness_volume[level]);
	pattern_msleep(led_dev, 3000);

	white_led_set_all(led_dev, 0x0);

	return 0;
}
#endif

static ssize_t led_max_brightness_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);
	ssize_t ret;
	int val;

	ret = kstrtoint(buf, 0, &val);

	if (val < MAX_BRIGHTNESS_PERCENT_MIN || val > MAX_BRIGHTNESS_PERCENT) {
		pr_err("led max brightness percent out of value %d\n", val);
		return -EINVAL;
	}

	led_dev->max_brightness_pct = val;
	pr_info("set max brightness percent %d\n", led_dev->max_brightness_pct);

	if (led_dev->rgb_pattern_event == RGB_MIC_OFF)
		rgb_led_set_update(led_dev, 0, RED_SET);

	return size;
}

static ssize_t led_max_brightness_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);

	pr_info("get led max brightness %d\n", led_dev->max_brightness_pct);

	return sprintf(buf, "%d\n", led_dev->max_brightness_pct);
}

static ssize_t white_brightness_all_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);

	white_led_blink_off(led_dev);

	white_led_set_all(led_dev, val);
	pr_info("(MAX %d) led white set all 0x%x\n", led_dev->max_brightness_pct, val);

	return size;
}

static ssize_t white_brightness_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);
	ssize_t ret;
	int led_num;
	int val;
	int i;

	ret = sscanf(buf, "0x%2x %d", &val, &led_num);
	if (ret != 2) {
		pr_err("fail to get led info : %d\n", ret);
		return -EINVAL;
	}

	white_led_blink_off(led_dev);

	if (led_num == 15) {
		for (i = 0 ; i < led_dev->white_count ; i++)
			white_led_set_update(led_dev, i, val);

		pr_info("(MAX %d) white set all 0x%x\n", led_dev->max_brightness_pct, val);
		return size;
	}

	if (led_num < 0 || led_num >= led_dev->white_count) {
		pr_err("out of white led num\n");
		return size;
	}

	white_led_set_update(led_dev, led_num, val);
	pr_info("(MAX %d)led_num %d, brightness %d\n", led_dev->max_brightness_pct, led_num, val);

	return size;
}

static ssize_t white_brightness_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",  led_dev->white_count);
}

static ssize_t rgb_color_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);
	ssize_t ret;
	int led_num;
	int val;

	ret = sscanf(buf, "0x%6x %d", &val, &led_num);
	if (ret != 2) {
		pr_err("fail to get led info : %d\n", ret);
		return -EINVAL;
	}

	if (led_num < 0 || led_num >= led_dev->rgb_count) {
		pr_err("out of rgb led num\n");
		return size;
	}

	rgb_led_blink_off(led_dev);

	rgb_led_set_update(led_dev, led_num, val);
	pr_info("(MAX %d) rgb set %d, led num %d\n", led_dev->max_brightness_pct, val, led_num);

	return size;
}

static ssize_t rgb_color_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", led_dev->rgb_count);
}

static ssize_t rgb_blink_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);
	int color, opt, pattern;
	int ret;
	enum rgb_patterns prio;

	ret = sscanf(buf, "0x%6x %d %d", &color, &opt, &pattern);
	if (ret != 3) {
		pr_err("fail to get led info : %d\n", ret);
		return -EINVAL;
	}

	pr_info("(MAX %d) pattern %d, color %d, opt %d\n", led_dev->max_brightness_pct, pattern, color, opt);

	if (!IS_VALID_RGB_PATTERN(pattern))
		return -EINVAL;

	switch (pattern) {
	case RGB_BLINK_OFF:
		prio = et6034a_rgb_get_higher_priority();
		pr_info("%s done / next %s\n", rgb_blink_pattern[led_dev->rgb_pattern_event].name,
				rgb_blink_pattern[prio].name);
		if (prio != RGB_BLINK_OFF)
			et6034a_rgb_pattern_proceed(led_dev, prio, LED_ON);
		else
			rgb_led_blink_off(led_dev);
		break;
	case RGB_BLINK_COLOR:
		led_dev->rgb_pattern_event = pattern;
		led_dev->rgb_blink_color = color;
		led_dev->rgb_blink_set = true;
		schedule_work(&led_dev->rgb_blink_work);
		break;
	default:
		pr_info("not used this sysfs\n");
		break;
	}

	return size;
}

static ssize_t rgb_patterns_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i;
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);

	pr_info("%d\n",	led_dev->rgb_pattern_event);

	for (i = 0; i < 10; i++) {
		pr_info("%s count %d\n", rgb_blink_pattern[i].name,
				rgb_blink_pattern[i].call_count);
	}

	return sprintf(buf, "%d\n", led_dev->rgb_pattern_event);
}

static ssize_t rgb_patterns_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct et6034a_led_data *led_dev = dev_get_drvdata(dev);
	int val, val2;
	int ret;
	enum rgb_patterns prio;

	ret = sscanf(buf, "%d %d", &val, &val2);
	if (ret != 2) {
		pr_err("fail to get led info : %d\n", ret);
		return -EINVAL;
	}

	if (!IS_VALID_RGB_PATTERN(val))
		return -EINVAL;

	if (val2 == LED_ON) {
		rgb_blink_pattern[val].call_count++;
		if (val == RGB_MIC_OFF)
			led_dev->mic_off = true;
	} else if (val2 == LED_OFF) {
		if (rgb_blink_pattern[val].call_count > 0)
			rgb_blink_pattern[val].call_count--;
		if (val == RGB_MIC_OFF)
			led_dev->mic_off = false;
	}

	pr_info("(MAX %d) pattern %s, opt %d, count %d\n",
			led_dev->max_brightness_pct, rgb_blink_pattern[val].name, val2,
			rgb_blink_pattern[val].call_count);
	prio = et6034a_rgb_get_higher_priority();
	if (prio != RGB_BLINK_OFF) {
		val = prio;
		val2 = LED_ON;
	}

	if (val2 == LED_ON)
		et6034a_rgb_pattern_proceed(led_dev, val, val2);
	else
		rgb_led_blink_off(led_dev);

	return size;
}
static DEVICE_ATTR_RW(led_patterns);
static DEVICE_ATTR_RW(led_max_brightness);
static DEVICE_ATTR_RW(white_brightness);
static DEVICE_ATTR_WO(white_brightness_all);
static DEVICE_ATTR_RW(rgb_color);
static DEVICE_ATTR_WO(rgb_blink);
static DEVICE_ATTR_RW(rgb_patterns);

static struct device_attribute *leds_attrs[] = {
	&dev_attr_led_patterns,
	&dev_attr_led_max_brightness,
	&dev_attr_white_brightness,
	&dev_attr_white_brightness_all,
	NULL
};

static struct device_attribute *rgb_leds_attrs[] = {
	&dev_attr_rgb_blink,
	&dev_attr_rgb_color,
	&dev_attr_rgb_patterns,
	NULL
};

#ifdef CONFIG_OF
static int et6034a_parse_dt(struct device *dev,
			struct et6034a_led_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_property_read_u32(np, "rgb_led_count", &pdata->rgb_count);
	if (ret) {
		dev_err(dev, "%s : rgb_count get error : %d\n", __func__, ret);
		goto error;
	}

	if (pdata->rgb_count) {
		ret = of_property_read_u32_array(np, "rgb_led_num",
				pdata->rgb_led_reg, pdata->rgb_count);
		if (ret) {
			dev_err(dev, "%s : rgb_led_reg get error : %d\n", __func__, ret);
			goto error;
		}
	}

	ret = of_property_read_u32(np, "white_led_count", &pdata->white_count);
	if (ret) {
		dev_err(dev, "%s : white_count get error : %d\n", __func__, ret);
		goto error;
	}

	if (pdata->white_count) {
		ret = of_property_read_u32_array(np, "white_led_num",
				pdata->white_led_reg, pdata->white_count);
		if (ret) {
			dev_err(dev, "%s : white_led_reg get error : %d\n", __func__, ret);
			goto error;
		}
	}

error:
	return ret;
}
#else
static int et6034a_parse_dt(struct device *dev,
			struct et6034a_led_data *pdata)
{
	return -ENODEV;
}
#endif

static int et6034a_led_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	struct et6034a_led_data *led_dev;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret = 0;
	int i = 0;

	dev_info(&client->dev, "%s\n", __func__);
	ret = i2c_check_functionality(adapter,
			I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA);
	if (!ret) {
		dev_err(&client->dev, "I2C check functionality failed\n");
		return -ENXIO;
	}

	if (client->dev.of_node) {
		led_dev = devm_kzalloc(&client->dev,
			sizeof(struct et6034a_led_data), GFP_KERNEL);
		if (!led_dev)
			return -ENOMEM;

		ret = et6034a_parse_dt(&client->dev, led_dev);
		if (ret)
			return ret;
	} else
		led_dev = client->dev.platform_data;

	led_dev->client = client;

	mutex_init(&led_dev->lock);

#ifdef CONFIG_DRV_SAMSUNG
	led_dev->et6034a_led_dev = sec_device_create(led_dev, "leds");
#else
	led_dev->led_class = class_create(THIS_MODULE, "sec_leds");
	if (IS_ERR(led_dev->led_class)) {
		pr_err("%s: can't create class\n", __func__);
		return PTR_ERR(led_dev->led_class);
	}
	led_dev->et6034a_led_dev = device_create(led_dev->led_class, NULL, 0, led_dev, "led");
#endif
	if (IS_ERR(led_dev->et6034a_led_dev)) {
		dev_err(&client->dev, "Unabled to create etkled class\n");
		ret = -ENOMEM;
		goto err_create_device;
	} else {
		if (led_dev->rgb_count) {
			for (i = 0; rgb_leds_attrs[i]; i++) {
				if (device_create_file(led_dev->et6034a_led_dev, rgb_leds_attrs[i])) {
					dev_err(&client->dev, "Unabled to create rgb_leds_attrs files\n");
					ret = -ENOMEM;
					goto err_create_file;
				}
			}
		}
		if (led_dev->white_count) {
			for (i = 0; leds_attrs[i]; i++) {
				if (device_create_file(led_dev->et6034a_led_dev, leds_attrs[i])) {
					dev_err(&client->dev, "Unabled to create leds_attrs files\n");
					ret = -ENOMEM;
					goto err_create_file;
				}
			}
		}
	}
	dev_set_drvdata(led_dev->et6034a_led_dev, led_dev);
	i2c_set_clientdata(client, led_dev);

	INIT_DELAYED_WORK(&led_dev->off_work, led_off_work);
	INIT_WORK(&led_dev->rgb_blink_work, rgb_led_blink_work);
	et6034a_rgb_pattern_proceed(led_dev, RGB_BLINK_BOOT, LED_ON);
	led_dev->max_brightness_pct = 100;
	led_dev->white_blink_on = false;
	led_dev->white_blink_timeout = 0;
	led_dev->white_blink_count = -1;
	INIT_WORK(&led_dev->white_blink_work, white_led_blink_work);

#ifdef FEATURE_USE_EVENT_STACK
	mutex_init(&led_dev->pattern_lock);
	mutex_init(&led_dev->list_lock);
	INIT_LIST_HEAD(&led_dev->event_list);
	INIT_DELAYED_WORK(&led_dev->pattern_work, et6034a_pattern_work);
#endif

	et6034a_write_reg(led_dev->client, ET6034A_RSTCTR, SW_RSTCTR_ON);
	et6034a_write_reg(led_dev->client, ET6034A_RGB_OE, RGB_OE_MASK);

	g_led_dev = led_dev;

	dev_info(&client->dev, "et6034 device is probed successfully.\n");

	return 0;
err_create_file:
	if (led_dev->rgb_count) {
		for (; i >= 0; i--)
			device_remove_file(led_dev->et6034a_led_dev, rgb_leds_attrs[i]);
	}
	if (led_dev->white_count) {
		for (; i >= 0; i--)
			device_remove_file(led_dev->et6034a_led_dev, leds_attrs[i]);
	}
err_create_device:
	mutex_destroy(&led_dev->lock);
#ifdef FEATURE_USE_EVENT_STACK
	mutex_destroy(&led_dev->pattern_lock);
	mutex_destroy(&led_dev->list_lock);
#endif
	kfree(led_dev);
	return ret;
}

static int et6034a_led_remove(struct i2c_client *client)
{
	struct et6034a_led_data *led_dev = i2c_get_clientdata(client);
	int i;

	if (led_dev == NULL)
		return 0;
	if (led_dev->rgb_count) {
		for (i = 0; rgb_leds_attrs[i]; i++)
			device_remove_file(led_dev->et6034a_led_dev, rgb_leds_attrs[i]);
	}
	if (led_dev->white_count) {
		for (i = 0; leds_attrs[i]; i++)
			device_remove_file(led_dev->et6034a_led_dev, leds_attrs[i]);
	}

	mutex_destroy(&led_dev->lock);
#ifdef FEATURE_USE_EVENT_STACK
	mutex_destroy(&led_dev->pattern_lock);
	mutex_destroy(&led_dev->list_lock);
#endif
	kfree(led_dev);
	dev_info(&client->dev, "et6034 device is removed successfully.\n");
	return 0;
}

static void et6034a_led_shutdown(struct i2c_client *client)
{
	struct et6034a_led_data *led_dev = i2c_get_clientdata(client);
	int i;

	if (led_dev == NULL)
		return;

	for (i = ET6034A_FIXBRIT_LED0; i <= ET6034A_FIXBRIT_LED15; i++)
		et6034a_write_reg(led_dev->client, i, BRIGHTNESS_OFF);

	et6034a_write_reg(led_dev->client, ET6034A_RSTCTR, SW_RSTCTR_OFF);
	et6034a_write_reg(led_dev->client, ET6034A_RGB_OE, 0x00);

	dev_info(&client->dev, "et6034 device is shutdown successfully.\n");
}

static struct i2c_device_id et6034a_idtable[] = {
	{ "ET6034A", 0 },
	{},
};

MODULE_DEVICE_TABLE(i2c, et6034a_idtable);

#ifdef CONFIG_OF
static const struct of_device_id et6034a_led_of_match[] = {
	{ .compatible = "ET6034A-LED", },
	{ }
};
#endif

static struct i2c_driver et6034a_led_driver = {
	.id_table	= et6034a_idtable,
	.probe	= et6034a_led_probe,
	.remove	= et6034a_led_remove,
	.shutdown = et6034a_led_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ET6034A",
#ifdef CONFIG_OF
		.of_match_table = et6034a_led_of_match,
#endif
	},
};

static int __init et6034a_led_init(void)
{
	return i2c_add_driver(&et6034a_led_driver);
}

static void __exit et6034a_led_exit(void)
{
	i2c_del_driver(&et6034a_led_driver);
}

module_init(et6034a_led_init);
module_exit(et6034a_led_exit);

MODULE_DESCRIPTION("et6034a led device driver using i2c interface");
MODULE_AUTHOR("xxx@samsung.com");
MODULE_LICENSE("GPL");
