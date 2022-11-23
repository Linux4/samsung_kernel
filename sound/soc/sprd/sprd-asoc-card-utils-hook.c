/*
 * ASoC SPRD sound card support
 *
 * Copyright (C) 2015 Renesas Solutions Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("BOARD")""fmt

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>

#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"

struct sprd_asoc_ext_hook_map {
	const char *name;
	sprd_asoc_hook_func hook;
	int en_level;
};

/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 start */
enum {
	/* ext_ctrl_type */
	CELL_CTRL_TYPE,
	/* pa type select */
	CELL_HOOK,
	/* music select mode */
	CELL_PRIV_M,
	/* voice select mode */
	CELL_PRIV_V,
	/* share gpio with  */
	CELL_SHARE_GPIO,
	CELL_NUMBER,
};

struct sprd_asoc_hook_spk_priv {
	int gpio[BOARD_FUNC_MAX];
	int priv_data_m[BOARD_FUNC_MAX];
	int priv_data_v[BOARD_FUNC_MAX];
	spinlock_t lock;
};
/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 end */

static struct sprd_asoc_hook_spk_priv hook_spk_priv;

#define GENERAL_SPK_MODE 10

#define EN_LEVEL 1

static int select_mode;
/*HS03 code for SR-SL6215-01-198 by libo at 20210802 start*/
static int hpr_pa_gpio;
/*HS03 code for SR-SL6215-01-198 by libo at 20210802 end*/

static ssize_t select_mode_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%d\n", select_mode);
}

static ssize_t select_mode_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buff, size_t len)
{
	unsigned long level;
	int ret;


	ret = kstrtoul(buff, 10, &level);
	if (ret) {
		pr_err("%s kstrtoul failed!(%d)\n", __func__, ret);
		return len;
	}
	select_mode = level;
	pr_info("speaker ext pa select_mode = %d\n", select_mode);

	return len;
}
/*HS03 code for SR-SL6215-01-198 by libo at 20210802 start*/
static ssize_t extpa_info_show(struct kobject *kobj,struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%s\n", "aw87318");
}
/*HS03 code for SR-SL6215-01-198 by libo at 20210802 end*/
static int ext_debug_sysfs_init(void)
{
	int ret;
	static struct kobject *ext_debug_kobj;
	static struct kobj_attribute ext_debug_attr =
		__ATTR(select_mode, 0644,
		select_mode_show,
		select_mode_store);
	/*HS03 code for SR-SL6215-01-198 by libo at 20210802 start*/
	static struct kobj_attribute extpa_info_attr =__ATTR(extpa_info, 0644,extpa_info_show,NULL);
	/*HS03 code for SR-SL6215-01-198 by libo at 20210802 end*/
	if (ext_debug_kobj)
		return 0;
	ext_debug_kobj = kobject_create_and_add("extpa", kernel_kobj);
	if (ext_debug_kobj == NULL) {
		ret = -ENOMEM;
		pr_err("register sysfs failed. ret = %d\n", ret);
		return ret;
	}

	ret = sysfs_create_file(ext_debug_kobj, &ext_debug_attr.attr);
	if (ret) {
		pr_err("create sysfs failed. ret = %d\n", ret);
		return ret;
	}
	/*HS03 code for SR-SL6215-01-198 by libo at 20210802 start*/
	ret = sysfs_create_file(ext_debug_kobj, &extpa_info_attr.attr);
	if (ret) {
		pr_err("create extpa_info sysfs failed. ret = %d\n", ret);
		return ret;
	}
	/*HS03 code for SR-SL6215-01-198 by libo at 20210802 end*/
	return ret;
}

/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 start */
static int audio_sense = 0;                     // 0:music; 1:voice; ...
static int hook_state[BOARD_FUNC_MAX] = {0};    // record each hook on/off status

static int hook_general_spk(int id, int on);

int sprd_audio_sense_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max, i;
	unsigned int mask = (1 << fls(max)) - 1;

	if (audio_sense != (ucontrol->value.integer.value[0] & mask)) {
		audio_sense = (ucontrol->value.integer.value[0] & mask);
		/*
		 * if pa is already on,
		 * we need to change pa mode after audio_sense updated
		 */
		for (i = 0; i < BOARD_FUNC_MAX; i++) {
			if (hook_state[i] > 0) {
				hook_general_spk(i, 0); // close pa firstly
				hook_general_spk(i, 1); // reopen pa to set different mode
				sp_asoc_pr_info("[%s] id %d change mode\n", __func__, i);
			}
		}
	} else {
		sp_asoc_pr_info("[%s] set same value %d\n", __func__, audio_sense);
	}

	return 0;
}

int sprd_audio_sense_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = audio_sense;
	return 0;
}
/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 end */

static void hook_gpio_pulse_control(unsigned int gpio, unsigned int mode)
{
	int i = 1;
	spinlock_t *lock = &hook_spk_priv.lock;
	unsigned long flags;
	/*HS03 code for SR-SL6215-01-198 by libo at 20210802 start*/
	spin_lock_irqsave(lock, flags);
	for (i = 1; i <= mode; i++) {
		gpio_set_value(gpio, !EN_LEVEL);
		udelay(2);
		gpio_set_value(gpio, EN_LEVEL);
		udelay(2);
	}
	/*HS03 code for SR-SL6215-01-198 by libo at 20210802 end*/
	gpio_set_value(gpio, EN_LEVEL);
	spin_unlock_irqrestore(lock, flags);
}

/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 start */
static int hook_general_spk(int id, int on)
{
	int gpio, mode_m, mode_v;

	gpio = hook_spk_priv.gpio[id];
	if (gpio < 0) {
		pr_err("%s gpio is invalid!\n", __func__);
		return -EINVAL;
	}

	mode_m = hook_spk_priv.priv_data_m[id];
	if (mode_m > GENERAL_SPK_MODE || mode_m <= 0)
		mode_m = 0;

	mode_v = hook_spk_priv.priv_data_v[id];
	if (mode_v > GENERAL_SPK_MODE || mode_v <= 0)
		mode_v = 0;

	pr_info("%s id: %d, gpio: %d, mode(m/v): %d/%d, audio_sense: %d, on: %d\n",
		__func__, id, gpio, mode_m, mode_v, audio_sense, on);

	/* Off */
	if (!on) {
		gpio_set_value(gpio, !EN_LEVEL);
		/*HS03 code for SR-SL6215-01-198 by yuyongxiang at 20210803 start*/
		if (id == 0){
			gpio_set_value(hpr_pa_gpio, EN_LEVEL);
		}
		hook_state[id] = 0;
		msleep(1);  // avoid PA switched OFF to ON too fast
		/*HS03 code for SR-SL6215-01-198 by yuyongxiang at 20210803 end*/
		return HOOK_OK;
	}

	/* On */
	if (select_mode) {
		mode_m = select_mode;
		mode_v = select_mode;
		pr_info("%s, select_mode: %d\n", __func__, select_mode);
		/*HS03 code for SR-SL6215-01-198 by yuyongxiang at 20210803 start*/
		if ( id == 0){
			gpio_set_value(hpr_pa_gpio, !EN_LEVEL);
		}
		/*HS03 code for SR-SL6215-01-198 by yuyongxiang at 20210803 end*/
	}

	if (1 == audio_sense)
		hook_state[id] = mode_v;
	else
		hook_state[id] = mode_m;

	hook_gpio_pulse_control(gpio, hook_state[id]);
	pr_info("%s id: %d on in mode: %d\n", __func__, id, hook_state[id]);

	/* When the first time open speaker path and play a very short sound,
	 * the sound can't be heard. So add a delay here to make sure the AMP
	 * is ready.
	 */
        mdelay(10);

	return HOOK_OK;
}
/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 end */

static struct sprd_asoc_ext_hook_map ext_hook_arr[] = {
	{"general_speaker", hook_general_spk, EN_LEVEL},
};

static int sprd_asoc_card_parse_hook(struct device *dev,
					 struct sprd_asoc_ext_hook *ext_hook)
{
	struct device_node *np = dev->of_node;
	const char *prop_pa_info = "sprd,spk-ext-pa-info";
	const char *prop_pa_gpio = "sprd,spk-ext-pa-gpio";
	/*HS03 code for SR-SL6215-01-198 by yuyongxiang at 20210803 start*/
	const char *hpr_ext_pa_pin = "sprd,hpr-ext-pa-gpio";
	/*HS03 code for SR-SL6215-01-198 by yuyongxiang at 20210803 end*/
	int spk_cnt, elem_cnt, i;
	int ret = 0;
	unsigned long gpio_flag;
	unsigned int ext_ctrl_type, share_gpio, hook_sel, priv_data;
	u32 *buf;
	elem_cnt = of_property_count_u32_elems(np, prop_pa_info);
	if (elem_cnt <= 0) {
		dev_info(dev,
			"Count '%s' failed!(%d)\n", prop_pa_info, elem_cnt);
		return -EINVAL;
	}

	if (elem_cnt % CELL_NUMBER) {
		dev_err(dev, "Spk pa info is not a multiple of %d.\n",
			CELL_NUMBER);
		return -EINVAL;
	}

	spk_cnt = elem_cnt / CELL_NUMBER;
	if (spk_cnt > BOARD_FUNC_MAX) {
		dev_warn(dev, "Speaker count %d is greater than %d!\n",
			 spk_cnt, BOARD_FUNC_MAX);
		spk_cnt = BOARD_FUNC_MAX;
	}

	spin_lock_init(&hook_spk_priv.lock);

	buf = devm_kmalloc(dev, elem_cnt * sizeof(u32), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, prop_pa_info, buf, elem_cnt);
	if (ret < 0) {
		dev_err(dev, "Read property '%s' failed!\n", prop_pa_info);
		//return ret;
	}
	/*HS03 code for SR-SL6215-01-198 by libo at 20210802 start*/
	hpr_pa_gpio = of_get_named_gpio_flags(np, hpr_ext_pa_pin, 0, NULL);
	if (hpr_pa_gpio < 0) {
		dev_err(dev, "Get hpr_pa_gpio failed:%d!\n", hpr_pa_gpio);
	} else {
		gpio_flag = GPIOF_DIR_OUT;
		ret = gpio_request_one(hpr_pa_gpio, gpio_flag, "hpr_pa_gpio");
		if (ret < 0) {
			dev_err(dev, "hpr_pa_gpio request failed:%d!\n", ret);
			return ret;
		}
	}
	/*HS03 code for SR-SL6215-01-198 by libo at 20210802 end*/
	for (i = 0; i < spk_cnt; i++) {
		int num = i * CELL_NUMBER;

		/* Get the ctrl type */
		ext_ctrl_type = buf[CELL_CTRL_TYPE + num];
		if (ext_ctrl_type >= BOARD_FUNC_MAX) {
			dev_err(dev, "Ext ctrl type %d is invalid!\n",
				ext_ctrl_type);
			return -EINVAL;
		}

		/* Get the selection of hook function */
		hook_sel = buf[CELL_HOOK + num];
		if (hook_sel >= ARRAY_SIZE(ext_hook_arr)) {
			dev_err(dev,
				"Hook selection %d is invalid!\n", hook_sel);
			return -EINVAL;
		}
		ext_hook->ext_ctrl[ext_ctrl_type] = ext_hook_arr[hook_sel].hook;

		/* Get the private data */
		/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 start */
		priv_data = buf[CELL_PRIV_M + num];
		hook_spk_priv.priv_data_m[ext_ctrl_type] = priv_data;
		priv_data = buf[CELL_PRIV_V + num];
		hook_spk_priv.priv_data_v[ext_ctrl_type] = priv_data;
		/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 end */

		/* Process the shared gpio */
		share_gpio = buf[CELL_SHARE_GPIO + num];
		if (share_gpio > 0) {
			if (share_gpio > spk_cnt) {
				dev_err(dev, "share_gpio %d is bigger than spk_cnt!\n",
					share_gpio);
				ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
				return -EINVAL;
			}
			hook_spk_priv.gpio[ext_ctrl_type] =
				hook_spk_priv.gpio[share_gpio - 1];
			continue;
		}

		ret = of_get_named_gpio_flags(np, prop_pa_gpio, i, NULL);
		if (ret < 0) {
			dev_err(dev, "Get gpio failed:%d!\n", ret);
			ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
			return ret;
		}
		hook_spk_priv.gpio[ext_ctrl_type] = ret;

		pr_info("ext_ctrl_type %d hook_sel %d priv_data %d gpio %d",
			ext_ctrl_type, hook_sel, priv_data, ret);

		gpio_flag = GPIOF_DIR_OUT;
		gpio_flag |= ext_hook_arr[hook_sel].en_level ?
			GPIOF_INIT_HIGH : GPIOF_INIT_LOW;
		ret = gpio_request_one(hook_spk_priv.gpio[ext_ctrl_type],
				       gpio_flag, NULL);
		if (ret < 0) {
			dev_err(dev, "Gpio request[%d] failed:%d!\n",
				ext_ctrl_type, ret);
			ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
			return ret;
		}
	}

	return 0;
}
int sprd_asoc_card_parse_ext_hook(struct device *dev,
				  struct sprd_asoc_ext_hook *ext_hook)
{
	ext_debug_sysfs_init();
	return sprd_asoc_card_parse_hook(dev, ext_hook);
}

MODULE_ALIAS("platform:asoc-sprd-card");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC SPRD Sound Card Utils - Hooks");
MODULE_AUTHOR("Peng Lee <peng.lee@spreadtrum.com>");
