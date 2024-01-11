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
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
#ifdef CONFIG_TARGET_UMS512_25C10
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */
	/* select mode */
	CELL_PRIV,
#else
	/* music select mode */
	CELL_PRIV_M,
	/* voice select mode */
	CELL_PRIV_V,
#endif
	/* share gpio with  */
	CELL_SHARE_GPIO,
	CELL_NUMBER,
};

struct sprd_asoc_hook_spk_priv {
	int gpio[BOARD_FUNC_MAX];
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
#ifdef CONFIG_TARGET_UMS512_25C10
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */
	int priv_data[BOARD_FUNC_MAX];
#else
	int priv_data_m[BOARD_FUNC_MAX];
	int priv_data_v[BOARD_FUNC_MAX];
#endif
	spinlock_t lock;
};
/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 end */

static struct sprd_asoc_hook_spk_priv hook_spk_priv;

#define GENERAL_SPK_MODE 10

#define EN_LEVEL 1

static int select_mode;

/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
#ifdef CONFIG_TARGET_UMS512_25C10
static u32 extral_iic_pa_en;
enum ext_pa_type {
	EXT_FSM_PA,
	EXT_DEFAULT_PA,
	EXT_PA_MAX,
};
/* Tab A7 Lite T618 code for SR-AX6189A-01-92 by yingboyang at 20220118 start */
int ext_spk_pa_type = EXT_DEFAULT_PA;
/* Tab A7 Lite T618 code for SR-AX6189A-01-92 by yingboyang at 20220118 end */
extern int get_pa_select_flag(void);
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */
#else
/*HS03 code for SR-SL6215-01-198 by libo at 20210802 start*/
static int hpr_pa_gpio;
/*HS03 code for SR-SL6215-01-198 by libo at 20210802 end*/
#endif

/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
/* FS1512 bringup start */
#ifdef CONFIG_SND_SOC_FS15XX
#define FS1512N_START  300
#define FS1512N_PULSE_DELAY_US 20
#define FS1512N_T_WORK  300
#define FS1512N_T_PWD  3260
#define SPC_GPIO_HIGH 1
#define SPC_GPIO_LOW 0

int fs15xx_shutdown(unsigned int gpio);
static int g_mode = 1;
static int gpio_last = 0;
static int adp1_gpio, adp2_gpio;


int hook_gpio_pulse_control_FS1512N(unsigned int gpio,int mode_new)
{
	unsigned long flags;
	spinlock_t *lock = &hook_spk_priv.lock;
	int count;
	int ret = 0;

	sp_asoc_pr_info("%s gpio:%d-->%d mode: %d-->%d\n", __func__, gpio_last, gpio, g_mode, mode_new);

	if (g_mode == mode_new && gpio_last == gpio) {
	/* the same mode and same gpio, not to switch again */
		return ret;
	} else {
	/* switch mode online, need shut down pa firstly */
		fs15xx_shutdown(gpio);
	}

	g_mode = mode_new;
	gpio_last = gpio;
	/* enable pa into work mode */
	/* make sure idle mode: gpio output low */
	gpio_direction_output(gpio, 0);
	spin_lock_irqsave(lock, flags);
	/* 1. send T-sta */
	gpio_set_value( gpio, SPC_GPIO_HIGH);
	udelay(FS1512N_START);
	gpio_set_value( gpio, SPC_GPIO_LOW);
	udelay(FS1512N_PULSE_DELAY_US); // < 140us
	/* 2. send mode */
	count = g_mode - 1;
	while (count > 0) { // count of pulse
		gpio_set_value( gpio, SPC_GPIO_HIGH);
		udelay(FS1512N_PULSE_DELAY_US); // < 140us 10-150
		gpio_set_value( gpio, SPC_GPIO_LOW);
		udelay(FS1512N_PULSE_DELAY_US); // < 140us
		count--;
	}
	/* 3. pull up gpio and delay, enable pa */
	gpio_set_value( gpio, SPC_GPIO_HIGH);
	spin_unlock_irqrestore(lock, flags);
	udelay(FS1512N_T_WORK); // pull up gpio > 220us
	return ret;
}

int fs15xx_shutdown(unsigned int gpio)
{
	spinlock_t *lock = &hook_spk_priv.lock;
	unsigned long flags;
	spin_lock_irqsave(lock, flags);
	g_mode = 1;
	gpio_last = 0;
	gpio_set_value( gpio, SPC_GPIO_LOW);
	udelay(FS1512N_T_PWD);
	spin_unlock_irqrestore(lock, flags);
	return 0;
}

static int hook_state[BOARD_FUNC_MAX] = {0};// record each hook on/off status
static int hook_general_spk_fsm(int id, int on)
{
	int gpio, mode;

	gpio = hook_spk_priv.gpio[id];
	if (gpio < 0) {
		pr_err("%s gpio is invalid!\n", __func__);
		return -EINVAL;
	}
	mode = hook_spk_priv.priv_data[id];
	if (mode > GENERAL_SPK_MODE || mode <= 0) {
		mode = 0;
	}
	pr_info("%s id: %d, gpio: %d, mode: %d, on: %d\n", __func__, id, gpio, mode, on);
	/* update pa status */
	hook_state[id] = on;

	/* Off */
	if (!on) {
		fs15xx_shutdown(gpio);
		return HOOK_OK;
	}

	/* On */
	if (select_mode) {
		mode = select_mode;
		pr_info("%s mode: %d, select_mode: %d\n", __func__, mode, select_mode);
	}

	hook_gpio_pulse_control_FS1512N(gpio, mode);

	/* When the first time open speaker path and play a very short sound,
	 * the sound can't be heard. So add a delay here to make sure the AMP
	 * is ready.
	 */
	msleep(22);
	return HOOK_OK;
}
/* Tab A7 T618 code for AX6189DEV-666 by yuyongxiang at 20220118 start */
static int hook_general_ctl(int id, int on)
{
	if (0 != id) {
		pr_info("%s id: %d not support!\n", __func__, id);
		return HOOK_OK;
	}

	if (on) {
		hook_general_spk_fsm(0, 1);
	} else {
		hook_general_spk_fsm(0, 0);
	}

	pr_info("%s : %s\n", __func__, on ? "on" : "off");
	return 0;
}

static int hook_general_ctl_1(int id, int on)
{
	if (1 != id) {
		pr_info("%s id: %d not support!\n", __func__, id);
		return HOOK_OK;
	}

	if (on) {
		hook_general_spk_fsm(1, 1);
	} else {
		hook_general_spk_fsm(1, 0);
	}

	pr_info("%s : %s\n", __func__, on ? "on" : "off");
	return 0;
}
#endif
/* Tab A7 T618 code for AX6189DEV-666 by yuyongxiang at 20220118 end */
/* FS1512 bringup end */
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */

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

/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
#ifdef CONFIG_TARGET_UMS512_25C10
static ssize_t ext_spk_pa_type_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%s\n", ext_spk_pa_type ? "aw87xx": "fs15xx");
}
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */

static int ext_debug_sysfs_init(void)
{
	int ret;
	static struct kobject *ext_debug_kobj;
	static struct kobj_attribute ext_debug_attr =
		__ATTR(select_mode, 0644,
		select_mode_show,
		select_mode_store);
	static struct kobj_attribute chip_id_attr =
		__ATTR(chip_id_mode, 0644,
		ext_spk_pa_type_show,
		NULL);

	if (ext_debug_kobj) {
		return 0;
	}

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

	ret = sysfs_create_file(ext_debug_kobj, &chip_id_attr.attr);
	if (ret) {
		pr_err("create chip_id sysfs failed %d\n", ret);
		return ret;
	}

	return ret;
}
#else
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
#endif

/*Tab A7 T618 code for SR-AX6189A-01-187 by yuyongxiang at 20211220 start*/
#ifdef CONFIG_SND_SOC_AW87XXX
extern int aw87xxx_set_profile(int dev_index, char *profile);

static char *aw_profile[] = {"Music", "Off"};
enum aw87xxx_dev_index
{
	AW_DEV_0 = 0,
	AW_DEV_1 = 1,
};

static int hook_spk_aw87xxx(int id, int on)
{

	pr_info("%s id: %d, on: %d\n", __func__, id, on);

	if(on) {
		aw87xxx_set_profile(AW_DEV_0, aw_profile[0]);
		pr_info("[Awinic]aw87xxx %s\n", aw_profile[0]);
	} else {
		aw87xxx_set_profile(AW_DEV_0, aw_profile[1]);
		pr_info("[Awinic]aw87xxx %s\n", aw_profile[1]);
	}
	return HOOK_OK;
}

static int hook_spk_aw87xxx_1(int id, int on)
{

	pr_info("%s id: %d, on: %d\n", __func__, id, on);

	if(on) {
		aw87xxx_set_profile(AW_DEV_1, aw_profile[0]);
		pr_info("[Awinic]aw87xxx_1 %s\n", aw_profile[0]);
	} else {
		aw87xxx_set_profile(AW_DEV_1, aw_profile[1]);
		pr_info("[Awinic]aw87xxx_1 %s\n", aw_profile[1]);
	}
	return HOOK_OK;
}
#else
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
	/*HS03 code for P211007-01246 by gaopan at 20211116 start*/
#ifdef CONFIG_TARGET_UMS9230_4H10
	mdelay(10);
#else
	msleep(22);
#endif
	/*HS03 code for P211007-01246 by gaopan at 20211116 end*/
	return HOOK_OK;
}
/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 end */
#endif
/*Tab A7 T618 code for SR-AX6189A-01-187 by yuyongxiang at 20211220 end*/
/* Tab A7 T618 code for SR-AX6189A-01-189|SR-AX6189A-01-187 by yuyongxiang at 20220110 start */
static struct sprd_asoc_ext_hook_map ext_hook_arr[] = {
#ifdef CONFIG_TARGET_UMS512_25C10
	{"aw87xx", hook_spk_aw87xxx, EN_LEVEL},
	{"aw87xx", hook_spk_aw87xxx_1, EN_LEVEL},
	{"ext_speaker", hook_general_ctl, EN_LEVEL},
	/* Tab A7 T618 code for AX6189DEV-666 by yuyongxiang at 20220118 start */
	{"ext_speaker_1", hook_general_ctl_1, EN_LEVEL},
	/* Tab A7 T618 code for AX6189DEV-666 by yuyongxiang at 20220118 end */
#else
	{"general_speaker", hook_general_spk, EN_LEVEL},
#endif
};
/* Tab A7 T618 code for SR-AX6189A-01-189|SR-AX6189A-01-187 by yuyongxiang at 20220110 end */
static int sprd_asoc_card_parse_hook(struct device *dev,
					 struct sprd_asoc_ext_hook *ext_hook)
{
	struct device_node *np = dev->of_node;
	const char *prop_pa_info = "sprd,spk-ext-pa-info";
	const char *prop_pa_gpio = "sprd,spk-ext-pa-gpio";

#ifdef CONFIG_SND_SOC_AW87XXX
	const char *extral_iic_pa_info = "extral-iic-pa";
	const char *prop_id_gpio = "sprd,spk-ext-pa-type-gpio";
	int spk_cnt, elem_cnt, i;
	int ret = 0;
	unsigned long gpio_flag;
	unsigned int ext_ctrl_type, share_gpio, hook_sel, priv_data,
		     chip_id_gpio;
	u32 *buf;
	u32 extral_iic_pa = 0;

	ret = of_property_read_u32(np, extral_iic_pa_info, &extral_iic_pa);
	if (!ret) {
		sp_asoc_pr_info("%s hook aw87xx iic pa!\n", __func__);
		extral_iic_pa_en = extral_iic_pa;
		ext_hook->ext_ctrl[BOARD_FUNC_SPK] = ext_hook_arr[BOARD_FUNC_SPK].hook;
		return 0;
	}
#else
	/*HS03 code for SR-SL6215-01-198 by yuyongxiang at 20210803 start*/
	const char *hpr_ext_pa_pin = "sprd,hpr-ext-pa-gpio";
	/*HS03 code for SR-SL6215-01-198 by yuyongxiang at 20210803 end*/
	int spk_cnt, elem_cnt, i;
	int ret = 0;
	unsigned long gpio_flag;
	unsigned int ext_ctrl_type, share_gpio, hook_sel, priv_data;
	u32 *buf;
#endif

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

/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
#ifndef CONFIG_TARGET_UMS512_25C10
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */
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
#endif

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
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
#ifdef CONFIG_TARGET_UMS512_25C10
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */
		/* Get the private data */
		priv_data = buf[CELL_PRIV + num];
		hook_spk_priv.priv_data[ext_ctrl_type] = priv_data;
		/*Tab A7 T618 code for SR-AX6189A-01-187 by yuyongxiang at 20211220 start*/
		if (1 == hook_sel) {//hook selection id
			pr_warn("%s pa use i2c\n", __func__);
			continue;
		}
		/*Tab A7 T618 code for SR-AX6189A-01-187 by yuyongxiang at 20211220 end*/
#else
		/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 start */
		priv_data = buf[CELL_PRIV_M + num];
		hook_spk_priv.priv_data_m[ext_ctrl_type] = priv_data;
		priv_data = buf[CELL_PRIV_V + num];
		hook_spk_priv.priv_data_v[ext_ctrl_type] = priv_data;
		/* HS03 code for SL6215DEV-560 by LiangWenye at 20210826 end */
#endif
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

		dev_info(dev, "%s ext_ctrl_type %d hook_sel %d priv_data %d gpio %d",
			__func__, ext_ctrl_type, hook_sel, priv_data, ret);

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

#ifdef CONFIG_SND_SOC_AW87XXX
	/* Parse configs for whether support spk ext pa id */
	ret = of_get_named_gpio_flags(np, prop_id_gpio, 0, NULL);
	if (ret < 0) {
		dev_err(dev, "%s not support spk ext pa id\n", __func__);
	} else {
		chip_id_gpio = ret;
		gpio_flag = GPIOF_DIR_IN;
		ret = gpio_request_one(chip_id_gpio, gpio_flag, "pa_chip_id");
		if (ret < 0) {
			dev_err(dev, "Gpio pa_chip_id request failed %d\n",
				ret);
			return 0;
		}
		ext_spk_pa_type = gpio_get_value(chip_id_gpio);
		dev_info(dev,
			 "%s 'spk-ext-pa-id-gpio' %d, ext_spk_pa_type %s\n",
			 __func__, chip_id_gpio,
			 ext_spk_pa_type ? "EXT_DEFAULT_PA" : "EXT_FSM_PA");
	}
#endif
	return 0;
}

/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
/* FS1512 bringup start */
#ifdef CONFIG_SND_SOC_FS15XX
static int sprd_asoc_card_parse_hook_fsm(struct device *dev,
					struct sprd_asoc_ext_hook *ext_hook)
{
	struct device_node *np = dev->of_node;
	const char *prop_pa_fsm_info = "sprd,spk-ext-pa-fsm-info";
	const char *prop_pa_fsm_gpio = "sprd,spk-ext-pa-fsm-gpio";
	const char *prop_pa_adp1_gpio = "sprd,spk-ext-pa-adp1-pin";
	const char *prop_pa_adp2_gpio = "sprd,spk-ext-pa-adp2-pin";
	int spk_cnt, elem_cnt, i;
	int ret = 0;
	unsigned long gpio_flag;
	unsigned int ext_ctrl_type, share_gpio, hook_sel, priv_data;
	u32 *buf;

	elem_cnt = of_property_count_u32_elems(np, prop_pa_fsm_info);
	if (elem_cnt <= 0) {
		dev_info(dev, "Count '%s' failed!(%d)\n", prop_pa_fsm_info, elem_cnt);
		return -EINVAL;
	}

	if (elem_cnt % CELL_NUMBER) {
		dev_err(dev, "Spk pa info is not a multiple of %d.\n", CELL_NUMBER);
		return -EINVAL;
	}

	spk_cnt = elem_cnt / CELL_NUMBER;
	if (spk_cnt > BOARD_FUNC_MAX) {
		dev_warn(dev, "Speaker count %d is greater than %d!\n", spk_cnt, BOARD_FUNC_MAX);
		spk_cnt = BOARD_FUNC_MAX;
	}

	spin_lock_init(&hook_spk_priv.lock);

	buf = devm_kmalloc(dev, elem_cnt * sizeof(u32), GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}
	ret = of_property_read_u32_array(np, prop_pa_fsm_info, buf, elem_cnt);
	if (ret < 0) {
		dev_err(dev, "Read property '%s' failed!\n", prop_pa_fsm_info);
	}

	for (i = 0; i < spk_cnt; i++) {
		int num = i * CELL_NUMBER;

		/* Get the ctrl type */
		ext_ctrl_type = buf[CELL_CTRL_TYPE + num];
		if (ext_ctrl_type >= BOARD_FUNC_MAX) {
			dev_err(dev, "Ext ctrl type %d is invalid!\n", ext_ctrl_type);
			return -EINVAL;
		}

		/* Get the selection of hook function */
		hook_sel = buf[CELL_HOOK + num];
		if (hook_sel >= ARRAY_SIZE(ext_hook_arr)) {
			dev_err(dev, "Hook selection %d is invalid!\n", hook_sel);
			return -EINVAL;
		}
		ext_hook->ext_ctrl[ext_ctrl_type] = ext_hook_arr[hook_sel].hook;
		/* Get the private data */
		priv_data = buf[CELL_PRIV + num];
		hook_spk_priv.priv_data[ext_ctrl_type] = priv_data;

		/* Process the shared gpio */
		share_gpio = buf[CELL_SHARE_GPIO + num];
		if (share_gpio > 0) {
			if (share_gpio > spk_cnt) {
				dev_err(dev, "share_gpio %d is bigger than spk_cnt!\n", share_gpio);
				ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
				return -EINVAL;
			}
			hook_spk_priv.gpio[ext_ctrl_type] =
				hook_spk_priv.gpio[share_gpio - 1];
			continue;
		}

		ret = of_get_named_gpio_flags(np, prop_pa_fsm_gpio, i, NULL);
		if (ret < 0) {
			dev_err(dev, "Get gpio failed:%d!\n", ret);
			ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
			return ret;
		}
		hook_spk_priv.gpio[ext_ctrl_type] = ret;

		dev_info(dev, "%s ext_ctrl_type %d hook_sel %d priv_data %d gpio %d",
				__func__, ext_ctrl_type, hook_sel, priv_data, ret);

		gpio_flag = GPIOF_DIR_OUT | GPIOF_INIT_LOW ;
		ret = gpio_request_one(hook_spk_priv.gpio[ext_ctrl_type], gpio_flag, NULL);
		if (ret < 0) {
			dev_err(dev, "Gpio request[%d] failed:%d!\n", ext_ctrl_type, ret);
			ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
			return ret;
		}
	}

	adp1_gpio = of_get_named_gpio_flags(np, prop_pa_adp1_gpio, 0, NULL);
	if (adp1_gpio < 0) {
		dev_err(dev, "%s not exit! skip\n", prop_pa_adp1_gpio);
	} else {
		ret = gpio_request_one(adp1_gpio, GPIOF_DIR_OUT | GPIOF_INIT_LOW, "adp1_gpio");
		if (ret < 0) {
			dev_err(dev, "adp1_gpio request failed:%d!\n", ret);
			return ret;
		}
	}

	adp2_gpio = of_get_named_gpio_flags(np, prop_pa_adp2_gpio, 0, NULL);
	if (adp2_gpio < 0) {
		dev_err(dev, "%s not exit! skip\n", prop_pa_adp2_gpio);
	} else {
		ret = gpio_request_one(adp2_gpio, GPIOF_DIR_OUT | GPIOF_INIT_LOW, "adp2_gpio");
		if (ret < 0) {
			dev_err(dev, "adp2_gpio request failed:%d!\n", ret);
			return ret;
		}
	}

	return 0;
}
#endif
/* FS1512 bringup end */
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */

int sprd_asoc_card_parse_ext_hook(struct device *dev,
				  struct sprd_asoc_ext_hook *ext_hook)
{
	ext_debug_sysfs_init();
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 start */
/* FS1512 bringup start */
#ifdef CONFIG_SND_SOC_FS15XX
	if (EXT_FSM_PA == get_pa_select_flag()) {
		/* Tab A7 Lite T618 code for SR-AX6189A-01-92 by yingboyang at 20220118 start */
		ext_spk_pa_type = EXT_FSM_PA;
		/* Tab A7 Lite T618 code for SR-AX6189A-01-92 by yingboyang at 20220118 end */
		return sprd_asoc_card_parse_hook_fsm(dev, ext_hook);
	}
#endif
/* FS1512 bringup end */
/* Tab A7 T618 code for SR-AX6189A-01-189 by yuyongxiang at 20220110 end */
	return sprd_asoc_card_parse_hook(dev, ext_hook);
}

MODULE_ALIAS("platform:asoc-sprd-card");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC SPRD Sound Card Utils - Hooks");
MODULE_AUTHOR("Peng Lee <peng.lee@spreadtrum.com>");
