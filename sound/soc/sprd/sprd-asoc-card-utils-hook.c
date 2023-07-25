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

enum {
	/* ext_ctrl_type */
	CELL_CTRL_TYPE,
	/* pa type select */
	CELL_HOOK,
	/* select mode */
	CELL_PRIV,
	/* share gpio with  */
	CELL_SHARE_GPIO,
	CELL_NUMBER,
};

struct sprd_asoc_hook_spk_priv {
	int gpio[BOARD_FUNC_MAX];
	int priv_data[BOARD_FUNC_MAX];
	spinlock_t lock;
};

static struct sprd_asoc_hook_spk_priv hook_spk_priv;

#define GENERAL_SPK_MODE 10

#define EN_LEVEL 1

static int select_mode;
static u32 extral_iic_pa_en;
enum ext_pa_type {
	EXT_FSM_PA,
	EXT_DEFAULT_PA,
	EXT_PA_MAX,
};
static enum ext_pa_type ext_spk_pa_type = EXT_DEFAULT_PA;
/* fsm rcv 1-3, fsm spk- 4-6 */
#define FSM_RCV_DEFAULT_MODE    1
#define FSM_SPK_DEFAULT_MODE    6

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

#define CONFIG_USE_AUDIO_PA_FSM
#ifdef CONFIG_USE_AUDIO_PA_FSM
static int chip_id_mode = 1;
static ssize_t chip_id_mode_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%d\n", chip_id_mode);
}

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
		chip_id_mode_show,
		NULL);
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


	ret = sysfs_create_file(ext_debug_kobj, &chip_id_attr.attr);
	if (ret) {
		pr_err("create sysfs failed. ret = %d\n", ret);
		return ret;
	}

	return ret;
}

#else
static ssize_t ext_spk_pa_type_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%d\n", ext_spk_pa_type);
}

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

	ret = sysfs_create_file(ext_debug_kobj, &chip_id_attr.attr);
	if (ret) {
		pr_err("create chip_id sysfs failed %d\n", ret);
		return ret;
	}

	return ret;
}
#endif

//FSM
#ifdef CONFIG_USE_AUDIO_PA_FSM
#define SPC_USE_HRTIMER
#define SPC_GPIO_LOW         0
#define SPC_GPIO_HIGH        1
#define FSM_OFF_MODE     0
#define FSM_RCV_MODE1    8
#define FSM_RCV_MODE2    9
#define FSM_SPK_MODE     10
#define FSM_MIN_MODE     0
#define FSM_MAX_MODE     10
#define FSM_DEFAULT_MODE FSM_SPK_MODE
#define FSM_DRV_VERSION  "V1.0.8"
#define FSM_DRV_NAME     "fs15xx"

#define FSM_START     (300) // 200 -1000
#define FSM_PULSE_DELAY_US (10) //10-150
#define FSM_RETRY          (10)
#define FSM_T_WORK (300) // 200-
//rcv 1-3 spk- 4-6
//#define FSM_RCV_DEFAULT_MODE    1
//#define FSM_SPK_DEFAULT_MODE    6


static __inline int fs15xx_shutdown(unsigned int gpio)
{
	int ret = 0;
	//gpio_direction_output(fs15xx->gpio_cmd, 0);
	gpio_set_value( gpio, SPC_GPIO_LOW);
	udelay(400); // 300us+

	return ret;
}

int hook_gpio_pulse_control_FSM(unsigned int gpio,int mode)
{
	unsigned long flags;
	spinlock_t *lock = &hook_spk_priv.lock;
	int count;
	int retry = 0;
	int ret = 0;

	pr_info("mode: %d-->%d\n", gpio, mode);

	// switch mode online, need shut down pa firstly
	fs15xx_shutdown(gpio);

	// enable pa into work mode
	do {
		// make sure idle mode: gpio output low
		gpio_direction_output(gpio, 0);
		spin_lock_irqsave(lock, flags);
		// 1. send T-sta
		gpio_set_value( gpio, SPC_GPIO_HIGH);
		udelay(FSM_START);
		gpio_set_value( gpio, SPC_GPIO_LOW);
		udelay(FSM_PULSE_DELAY_US); // < 140us

		// 2. send mode
		count = mode - 1;
		while (count > 0) { // count of pulse
			gpio_set_value( gpio, SPC_GPIO_HIGH);
			udelay(FSM_PULSE_DELAY_US); // < 140us 10-150
			gpio_set_value( gpio, SPC_GPIO_LOW);
			udelay(FSM_PULSE_DELAY_US); // < 140us
			count--;
		}

		// 3. pull up gpio and delay, enable pa
		gpio_set_value( gpio, SPC_GPIO_HIGH);
		// check_intervel_time(fs15xx, 500, 1950);
		spin_unlock_irqrestore(lock, flags);

		udelay(FSM_T_WORK); // pull up gpio > 220us
		//fs15xx->spc_mode = mode;
		break;
	} while (retry++ < FSM_RETRY);
	if (retry > 0)
		pr_info("retry %d times\n", retry);

	return ret;
}

static int hook_spk_FSM(int id, int on, int mode)
{
	int gpio;//, mode;
	int ret = 0;
	gpio = hook_spk_priv.gpio[id];
	if (gpio < 0) {
		pr_err("%s gpio is invalid!\n", __func__);
		return -EINVAL;
	}
	//mode = hook_spk_priv.priv_data[id];

	pr_info("%s id: %d, gpio: %d, mode: %d, on: %d\n",
		 __func__, id, gpio, mode, on);


	/* Off */
	if (!on) {
		ret = fs15xx_shutdown(gpio);
		return HOOK_OK;
	}

	/* On */
	if (select_mode >= 1 && select_mode <= 6) {
		mode = select_mode;
		pr_info("%s FSM mode: %d, select_mode: %d\n",
			__func__, mode, select_mode);
	}
	//hook_gpio_pulse_control(gpio, mode);
	ret = hook_gpio_pulse_control_FSM(gpio,mode);

	/* When the first time open speaker path and play a very short sound,
	 * the sound can't be heard. So add a delay here to make sure the AMP
	 * is ready.
	 */
	//msleep(22);

	return HOOK_OK;
}
static int hook_general_spk_fsm(int id, int on)
{

	int mode = FSM_SPK_DEFAULT_MODE;
	int ret = 0;
	ret = hook_spk_FSM(id,on,mode);

	return HOOK_OK;
}

static struct sprd_asoc_ext_hook_map ext_hook_arr_fsm[] = {
	{"FSMXX", hook_general_spk_fsm, EN_LEVEL},
};

#endif

static void hook_gpio_default_pulse_control(unsigned int gpio,
					    unsigned int mode)
{
	int i = 1;
	spinlock_t *lock = &hook_spk_priv.lock;
	unsigned long flags;

	pr_info("%s gpio %d, mode %d\n", __func__, gpio, mode);
	spin_lock_irqsave(lock, flags);
	for (i = 1; i < mode; i++) {
		gpio_set_value(gpio, EN_LEVEL);
		udelay(2);
		gpio_set_value(gpio, !EN_LEVEL);
		udelay(2);
	}

	gpio_set_value(gpio, EN_LEVEL);
	spin_unlock_irqrestore(lock, flags);
}

static void hook_gpio_fsm_pulse_control(unsigned int gpio, unsigned int mode)
{
	int i = 1;
	spinlock_t *lock = &hook_spk_priv.lock;
	unsigned long flags;

	pr_info("%s gpio %d, mode %d\n", __func__, gpio, mode);
	spin_lock_irqsave(lock, flags);
	gpio_set_value(gpio, !EN_LEVEL);
	udelay(400);
	gpio_set_value(gpio, EN_LEVEL);
	udelay(300);
	gpio_set_value(gpio, !EN_LEVEL);
	udelay(10);

	for (i = 1; i < mode - 1; i++) {
		gpio_set_value(gpio, EN_LEVEL);
		udelay(10);
		gpio_set_value(gpio, !EN_LEVEL);
		udelay(10);
	}

	gpio_set_value(gpio, EN_LEVEL);
	spin_unlock_irqrestore(lock, flags);
	udelay(300);
}

static void ext_default_spk_pa(int gpio, int id, int on)
{
	int mode = hook_spk_priv.priv_data[id];

	if (mode > GENERAL_SPK_MODE)
		mode = 0;

	pr_info("%s id: %d, gpio: %d, mode: %d, on: %d, select_mode %d\n",
		__func__, id, gpio, mode, on, select_mode);

	/* Off */
	if (!on) {
		gpio_set_value(gpio, !EN_LEVEL);
		return;
	}

	/* On */
	if (select_mode) {
		mode = select_mode;
	}

	hook_gpio_default_pulse_control(gpio, mode);

	/* When the first time open speaker path and play a very short sound,
	 * the sound can't be heard. So add a delay here to make sure the AMP
	 * is ready.
	 */
	msleep(22);
}

static void ext_fsm_spk_pa(int gpio, int id, int on)
{
	int mode = FSM_SPK_DEFAULT_MODE;

	pr_info("%s id: %d, gpio: %d, mode: %d, on: %d, select_mode %d\n",
		__func__, id, gpio, mode, on, select_mode);

	/* Off */
	if (!on) {
		gpio_set_value(gpio, !EN_LEVEL);
		udelay(400);
		return;
	}

	/* On */
	if (select_mode >= 1 && select_mode <= 6)
		mode = select_mode;

	hook_gpio_fsm_pulse_control(gpio, mode);
}

static int hook_general_spk(int id, int on)
{
	int gpio;

	sp_asoc_pr_info("%s enter\n", __func__);
	if (extral_iic_pa_en > 0) {
		static int (*extral_i2c_pa_function)(int);
		extral_i2c_pa_function = (void *)kallsyms_lookup_name("aw87xxx_i2c_pa");
		if (!extral_i2c_pa_function) {
			sp_asoc_pr_info("%s extral_i2c_pa is not prepare\n", __func__);
		} else {
			sp_asoc_pr_info("%s extral_i2c_pa, on %d\n", __func__, on);
			extral_i2c_pa_function(on);
		}
		return HOOK_OK;
	}

	gpio = hook_spk_priv.gpio[id];
	if (gpio < 0) {
		pr_err("%s gpio is invalid!\n", __func__);
		return -EINVAL;
	}

	if (ext_spk_pa_type == EXT_FSM_PA)
		ext_fsm_spk_pa(gpio, id, on);
	else
		ext_default_spk_pa(gpio, id, on);

	return HOOK_OK;
}

static struct sprd_asoc_ext_hook_map ext_hook_arr[] = {
	{"general_speaker", hook_general_spk, EN_LEVEL},
};

#ifdef CONFIG_USE_AUDIO_PA_FSM
static int sprd_asoc_card_parse_hook(struct device *dev,
					 struct sprd_asoc_ext_hook *ext_hook)
{
	struct device_node *np = dev->of_node;
	const char *prop_pa_info = "sprd,spk-ext-pa-info";
	const char *prop_pa_gpio = "sprd,spk-ext-pa-gpio";
	int spk_cnt, elem_cnt, i;
	int ret = 0;
	unsigned long gpio_flag;
	unsigned int ext_ctrl_type, share_gpio, hook_sel, priv_data;
	u32 *buf;

	struct sprd_asoc_ext_hook_map *speaker_hook_global  = ext_hook_arr;
	unsigned int chip_id_gpio = 0;
	unsigned int chip_id_status = 0; // 0- fsm  1-bct
	const char *prop_id_gpio = "sprd,spk-ext-pa-id-gpio";
	ret = of_get_named_gpio_flags(np, prop_id_gpio, 0, NULL);
	if (ret < 0) {
		dev_err(dev, "Get gpio failed:%d!\n", ret);
		ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
		return ret;
	}
	chip_id_gpio = ret;
	//dev_info(dev,
	//		"Count '%s' :(%d)\n", prop_id_gpio, chip_id_gpio);
	gpio_flag = GPIOF_DIR_IN;
	ret = gpio_request_one(chip_id_gpio, gpio_flag,
			       "PA_chip_id");
	if (ret < 0) {
		dev_err(dev, "Gpio request failed:%d!\n", ret);
		ext_hook->ext_ctrl[ext_ctrl_type] = NULL;
		return ret;
	}
	chip_id_status = gpio_get_value(chip_id_gpio);
	chip_id_mode = chip_id_status;
	dev_info(dev,"Count '%s' :(%d)--id audio_pa-value:%d 1:bct,0-fsm\n", prop_id_gpio, chip_id_gpio,chip_id_status);
	if (chip_id_status == 0) {
		speaker_hook_global = ext_hook_arr_fsm;
		dev_info(dev,"ext_PA use fs15xxx!!!\n");
	} else {
		speaker_hook_global = ext_hook_arr;
		dev_info(dev,"ext_PA use bctxxxx!!!\n");
	}


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
		ext_hook->ext_ctrl[ext_ctrl_type] = speaker_hook_global[hook_sel].hook;

		/* Get the private data */
		priv_data = buf[CELL_PRIV + num];
		hook_spk_priv.priv_data[ext_ctrl_type] = priv_data;

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
		gpio_flag |= speaker_hook_global[hook_sel].en_level ?
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

#else
static int sprd_asoc_card_parse_hook(struct device *dev,
					 struct sprd_asoc_ext_hook *ext_hook)
{
	struct device_node *np = dev->of_node;
	const char *prop_pa_info = "sprd,spk-ext-pa-info";
	const char *prop_pa_gpio = "sprd,spk-ext-pa-gpio";
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
		priv_data = buf[CELL_PRIV + num];
		hook_spk_priv.priv_data[ext_ctrl_type] = priv_data;

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

	return 0;
}
#endif
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
