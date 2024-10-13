/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (C) Shanghai FourSemi Semiconductor Co.,Ltd 2016-2022.
 * All rights reserved.
 * 2022-09-23 File created.
 */

#ifndef __FS150X_H__
#define __FS150X_H__

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>

#define FS150X_DRV_VERSION "V5.0.0-a"
#define FS150X_AMP_W_NAME  "Fsm Ext Spk"

#define FS150X_TSTA_US     (220) /* 200us < Tsta < 1000us */
#define FS150X_TLH_US      (5)   /* 5us < Th/Tl < 150us */
#define FS150X_TWORK_US    (820) /* Twork > 800us */
#define FS150X_TPWD_US     (300) /* Tpwd > 260us */
#define FS150X_TON_US      (15000)  /* Ton Typ: 13.5ms */
#define FS150X_TOFF_US     (3000)   /* Toff Typ: 3ms */

#define fs150x_info(fmt, args...) pr_info("fs150x: " fmt "\n", ##args)
#define fs150x_dbg(fmt, args...) pr_debug("fs150x: " fmt "\n", ##args)
#define fs150x_err(fmt, args...) pr_err("fs150x: " fmt "\n", ##args)

struct fs150x_dev {
	spinlock_t spin_lock;
	struct device *dev;
	atomic_t gain_pulse;
	int stream_on;
	int gpio_mod;
	int switch_cnt;
};

static struct fs150x_dev g_fs150x_dev;

//static void fs150x_check_time(ktime_t t1, ktime_t t2)
//{
//	u64 time;
//
//	time = ktime_to_ns(ktime_sub(t2, t1));
//	do_div(time, 1000); /* ns -> us */
//	fs150x_dbg("Time used(us): %ld", time);
//}

static inline void fs150x_send_pulses(int gpio, int npulses,
		int delay_us, bool polar)
{
	while (npulses--) {
		gpio_set_value(gpio, polar);
		udelay(delay_us);
		gpio_set_value(gpio, !polar);
		udelay(delay_us);
	}
}

static int fs150x_amp_mode_switch(struct fs150x_dev *fs150x, bool enable)
{
	//ktime_t time_start, time_end;
	unsigned long flags;
	int val;
	int pulse[1];

	if (!gpio_is_valid(fs150x->gpio_mod))
		return 0;

	val = gpio_get_value(fs150x->gpio_mod);
	gpio_direction_output(fs150x->gpio_mod, 0);
	if (val)
		usleep_range(FS150X_TOFF_US, FS150X_TOFF_US + 10);

	memset(pulse, 0, sizeof(pulse));
	pulse[0] = atomic_read(&fs150x->gain_pulse);

	if (!enable || pulse[0] <= 0)
		return 0;

	spin_lock_irqsave(&fs150x->spin_lock, flags);
	//time_start = ktime_get_boottime();
	gpio_set_value(fs150x->gpio_mod, 1);
	udelay(FS150X_TSTA_US);
	fs150x_send_pulses(fs150x->gpio_mod,
			pulse[0], FS150X_TLH_US, false);
	//time_end = ktime_get_boottime();
	spin_unlock_irqrestore(&fs150x->spin_lock, flags);
	//fs150x_check_time(time_start, time_end);

	usleep_range(FS150X_TWORK_US, FS150X_TWORK_US + 10);

	return 0;
}

static int fs150x_amp_dapm_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct fs150x_dev *fs150x = &g_fs150x_dev;

	fs150x_info("event:%d, cnt:%d, state:%d",
			event, fs150x->switch_cnt, fs150x->stream_on);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		fs150x->switch_cnt++;
		if (fs150x->switch_cnt > 1) {
			break;
		}
		fs150x_amp_mode_switch(fs150x, true);
		fs150x->stream_on = true;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		fs150x->switch_cnt--;
		if (fs150x->switch_cnt > 0) {
			break;
		}
		fs150x_amp_mode_switch(fs150x, false);
		fs150x->stream_on = false;
		break;
	default:
		break;
	}

	return 0;
};

static int fs150x_amp_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fs150x_dev *fs150x = &g_fs150x_dev;
	struct soc_enum *enum_data;
	int mode;

	enum_data = (struct soc_enum *)kcontrol->private_value;
	mode = atomic_read(&fs150x->gain_pulse);
	mode = (mode < 3) ? mode - 1 : mode - 3;
	if (mode < 0 || mode > enum_data->items) {
		fs150x_err("Invalid mode index: %d", mode);
		return -EINVAL;
	}

	fs150x_dbg("Get mode index: %d", mode);
	ucontrol->value.integer.value[0] = mode;

	return 0;
}

static int fs150x_amp_mode_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct fs150x_dev *fs150x = &g_fs150x_dev;
	struct soc_enum *enum_data;
	int mode, ret;

	enum_data = (struct soc_enum *)kcontrol->private_value;
	mode = ucontrol->value.integer.value[0];
	fs150x_dbg("Set mode index: %d", mode);
	if (mode < 0 || mode > enum_data->items) {
		fs150x_err("Invalid mode index: %d", mode);
		return -EINVAL;
	}

	mode = (mode < 2) ? mode + 1 : mode + 3;
	atomic_set(&fs150x->gain_pulse, mode);
	if (!fs150x->stream_on)
		return 0;

	/* switch amp mode on line */
	ret = fs150x_amp_mode_switch(fs150x, true);

	return ret;
}

static const char * const fs150x_amp_mode_str[] = {
	"M1", "M2", "M3", "M4",
	"M5", "M6", "M7", "M8",
	"M9", "M10", "M11", "M12"
};

static const struct soc_enum fs150x_amp_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fs150x_amp_mode_str),
			fs150x_amp_mode_str),
};

static const struct snd_kcontrol_new fs150x_amp_controls[] = {
	SOC_DAPM_PIN_SWITCH(FS150X_AMP_W_NAME),
	SOC_ENUM_EXT("Fs150x Amp Mode", fs150x_amp_mode_enum[0],
			fs150x_amp_mode_get, fs150x_amp_mode_set),
};

static struct snd_soc_dapm_widget fs150x_amp_widgets[] = {
	SND_SOC_DAPM_SPK(FS150X_AMP_W_NAME, fs150x_amp_dapm_event),
};

static const struct snd_soc_dapm_route fs150x_amp_routes[] = {
	{FS150X_AMP_W_NAME, NULL, "LINEOUT L"}, // mt6359
	{FS150X_AMP_W_NAME, NULL, "Headphone L Ext Spk Amp"},
	{FS150X_AMP_W_NAME, NULL, "Headphone R Ext Spk Amp"},
//	{FS150X_AMP_W_NAME, NULL, "AUX"}, // wcd938x lineout
//	{FS150X_AMP_W_NAME, NULL, "HPHL"},
//	{FS150X_AMP_W_NAME, NULL, "HPHR"},
};

static int fs150x_audio_amp_init(struct snd_soc_card *card)
{
	struct fs150x_dev *fs150x;
	int gpio;
	int ret;

	if (card == NULL || card->dev == NULL)
		return -EINVAL;

	fs150x_info("version: %s", FS150X_DRV_VERSION);
	gpio = of_get_named_gpio(card->dev->of_node,
			"mediatek,gpio-audio-amp-mod", 0);
	if (gpio < 0) {
		fs150x_err("Failed to get MOD pin");
		return gpio;
	}

	ret = devm_gpio_request_one(card->dev, gpio,
			GPIOF_OUT_INIT_LOW, "FS-GPIO-MOD");
	if (ret) {
		fs150x_err("Failed to request MOD pin:%d", ret);
		return ret;
	}

	fs150x = &g_fs150x_dev;
	fs150x->dev = card->dev;
	spin_lock_init(&fs150x->spin_lock);
	atomic_set(&fs150x->gain_pulse, 8);
	fs150x->gpio_mod = gpio;
	fs150x->stream_on = 0;
	fs150x->switch_cnt = 0;
	gpio_set_value(fs150x->gpio_mod, 0);

	snd_soc_add_card_controls(card, fs150x_amp_controls,
			ARRAY_SIZE(fs150x_amp_controls));
	snd_soc_dapm_new_controls(&card->dapm, fs150x_amp_widgets,
			ARRAY_SIZE(fs150x_amp_widgets));
	snd_soc_dapm_add_routes(&card->dapm, fs150x_amp_routes,
			ARRAY_SIZE(fs150x_amp_routes));

	snd_soc_dapm_disable_pin(&card->dapm, FS150X_AMP_W_NAME);
	snd_soc_dapm_sync(&card->dapm);

	return 0;
}

/*
static void fs150x_audio_amp_deinit(struct snd_soc_card *card)
{
	struct fs150x_dev *fs150x = &g_fs150x_dev;

	if (fs150x->dev == NULL)
		return;

	if (gpio_is_valid(fs150x->gpio_mod))
		gpio_set_value(fs150x->gpio_mod, 0);

	snd_soc_dapm_disable_pin(&card->dapm, FS150X_AMP_W_NAME);
	snd_soc_dapm_sync(&card->dapm);
}
*/

#endif
