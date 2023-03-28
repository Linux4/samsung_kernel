/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ilitek.h"
#include <linux/touchscreen_info.h>

#define DTS_INT_GPIO	"touch,irq-gpio"
#define DTS_RESET_GPIO	"touch,reset-gpio"
#define DTS_OF_NAME	"tchip,ilitek"
/* HS70 code for HS70-133 by liufurong at 2019/10/31 start */
extern enum tp_module_used tp_is_used;
/* HS70 code for HS70-133 by liufurong at 2019/10/31 end */
/* HS70 add for HS70-1717 by zhanghao at 20191129 start */
enum module_name{
	/*HS70 code for txd-cd bringup by gaozhengwei at 2020/03/26 start*/
	TXD_CD_ILI7807G_MODULE_NAME = 0,
	/*HS70 code for txd-cd bringup by gaozhengwei at 2020/03/26 end*/
	TM_ILI7807G_MODULE_NAME = 1,
	TXD_ILI7807G_MODULE_NAME = 2,
	TXD12_ILI7807G_MODULE_NAME = 3,
	TXD15_ILI7807G_MODULE_NAME = 4,
	INX18_ILI7807G_MODULE_NAME = 5,
	UNKNOW_MODULE_NAME = 6,
};
//int name_modue = -1;
extern unsigned char *CTPM_FW;
unsigned char CTPM_FW_UNKNOW[] = {
	#include "firmware/SMS1871_INX_639_LongH_V0xCD.0x04.0x04_AP_0x04.0x00_MP_20191129.ili"
};

/*HS70 code for txd-cd bringup by gaozhengwei at 2020/03/26 start*/
unsigned char CTPM_FW_TXD_CD_ILI7807G[] = {
	#include "firmware/SMS1871_TXD-CD_639_LongH_V0xCD.0x13.0x00_AP_0x05.0x00_MP_20200617.ili"
};
/*HS70 code for txd-cd bringup by gaozhengwei at 2020/03/26 end*/

unsigned char CTPM_FW_TM_ILI7807G[] = {
	#include "firmware/SMS1871_TM_639_LongH_V0x2D.0x0A.0x00_AP_0x03.0x00_MP_20200408.ili"
};

unsigned char CTPM_FW_TXD_ILI7807G[] = {
	#include "firmware/SMS1871_TXD_639_LongH_V0xCD.0x12.0x00_AP_0x05.0x00_MP_20200617.ili"
};

unsigned char CTPM_FW_TXD12_ILI7807G[] = {
	#include "firmware/SMS1871_TXD12_BOE_639_LongH_V0xCD.0x02.0x00_AP_0x01.0x00_MP_20200618.ili"
};

unsigned char CTPM_FW_TXD15_ILI7807G[] = {
	#include "firmware/SMS1871_TXD_639_LongH_V0xCD.0x12.0x00_AP_0x05.0x00_MP_20200617.ili"
};

unsigned char CTPM_FW_INX18_ILI7807G[] = {
	#include "firmware/SMS1871_TXD_639_LongH_V0xCD.0x12.0x00_AP_0x05.0x00_MP_20200617.ili"
};
struct module_name_use module_name_is_use[] = {
	/*HS70 code for txd-cd bringup by gaozhengwei at 2020/03/26 start*/
	{.module_ini_name = TXD_CD_ILI7807G_INI_NAME, .module_bin_name = TXD_CD_ILI7807G_BIN_NAME, .size_ili = sizeof(CTPM_FW_TXD_CD_ILI7807G)},
	/*HS70 code for txd-cd bringup by gaozhengwei at 2020/03/26 end*/
	{.module_ini_name = TM_ILI7807G_INI_NAME, .module_bin_name = TM_ILI7807G_BIN_NAME, .size_ili = sizeof(CTPM_FW_TM_ILI7807G)},
	{.module_ini_name = TXD_ILI7807G_INI_NAME, .module_bin_name = TXD_ILI7807G_BIN_NAME, .size_ili = sizeof(CTPM_FW_TXD_ILI7807G)},
	{.module_ini_name = TXD12_ILI7807G_INI_NAME, .module_bin_name = TXD12_ILI7807G_BIN_NAME, .size_ili = sizeof(CTPM_FW_TXD12_ILI7807G)},
	{.module_ini_name = TXD15_ILI7807G_INI_NAME, .module_bin_name = TXD15_ILI7807G_BIN_NAME, .size_ili = sizeof(CTPM_FW_TXD15_ILI7807G)},
	{.module_ini_name = INX18_ILI7807G_INI_NAME, .module_bin_name = INX18_ILI7807G_BIN_NAME, .size_ili = sizeof(CTPM_FW_INX18_ILI7807G)},
	{.module_ini_name = "mp.ini", .module_bin_name = "ILITEK_FW_TXD_ILI7807G", .size_ili = sizeof(CTPM_FW_UNKNOW)},
};
/* HS70 add for HS70-1717 by zhanghao at 20191129 end */
void ilitek_plat_tp_reset(void)
{
	ipio_info("edge delay = %d\n", idev->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	gpio_direction_output(idev->tp_rst, 1);
	mdelay(1);
	gpio_set_value(idev->tp_rst, 0);
	mdelay(5);
	gpio_set_value(idev->tp_rst, 1);
	mdelay(idev->rst_edge_delay);
}
#if ILI_USE_ENABLE_NODE
static int ili_input_open(struct input_dev *dev)
{
    idev->tp_is_enabled = 1;
	return 0;
}

static void ili_input_close(struct input_dev *dev)
{
    idev->tp_is_enabled = 0;
}
#endif
void ilitek_plat_input_register(void)
{
	int ret = 0;
	ipio_info();

	idev->input = input_allocate_device();
	if (ERR_ALLOC_MEM(idev->input)) {
		ipio_err("Failed to allocate touch input device\n");
		input_free_device(idev->input);
		return;
	}

	idev->input->name = idev->hwif->name;
	idev->input->phys = idev->phys;
	idev->input->dev.parent = idev->dev;
	idev->input->id.bustype = idev->hwif->bus_type;

	/* set the supported event type for input device */
	set_bit(EV_ABS, idev->input->evbit);
	set_bit(EV_SYN, idev->input->evbit);
	set_bit(EV_KEY, idev->input->evbit);
	set_bit(BTN_TOUCH, idev->input->keybit);
	set_bit(BTN_TOOL_FINGER, idev->input->keybit);
	set_bit(INPUT_PROP_DIRECT, idev->input->propbit);

	input_set_abs_params(idev->input, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN, idev->panel_wid - 1, 0, 0);
	input_set_abs_params(idev->input, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN, idev->panel_hei - 1, 0, 0);
	input_set_abs_params(idev->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(idev->input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	if (MT_PRESSURE)
		input_set_abs_params(idev->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (MT_B_TYPE) {
#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(idev->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else
		input_mt_init_slots(idev->input, MAX_TOUCH_NUM);
#endif /* LINUX_VERSION_CODE */
	} else {
		input_set_abs_params(idev->input, ABS_MT_TRACKING_ID, 0, MAX_TOUCH_NUM, 0, 0);
	}

	/* Gesture keys register */
	/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 start */
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_POWER);
	/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 end */
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_O);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_E);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_M);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_W);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_S);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_V);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_C);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_F);

	__set_bit(KEY_GESTURE_POWER, idev->input->keybit);
	__set_bit(KEY_GESTURE_UP, idev->input->keybit);
	__set_bit(KEY_GESTURE_DOWN, idev->input->keybit);
	__set_bit(KEY_GESTURE_LEFT, idev->input->keybit);
	__set_bit(KEY_GESTURE_RIGHT, idev->input->keybit);
	__set_bit(KEY_GESTURE_O, idev->input->keybit);
	__set_bit(KEY_GESTURE_E, idev->input->keybit);
	__set_bit(KEY_GESTURE_M, idev->input->keybit);
	__set_bit(KEY_GESTURE_W, idev->input->keybit);
	__set_bit(KEY_GESTURE_S, idev->input->keybit);
	__set_bit(KEY_GESTURE_V, idev->input->keybit);
	__set_bit(KEY_GESTURE_Z, idev->input->keybit);
	__set_bit(KEY_GESTURE_C, idev->input->keybit);
	__set_bit(KEY_GESTURE_F, idev->input->keybit);
#if ILI_USE_ENABLE_NODE
	idev->input->open = ili_input_open;
	idev->input->close = ili_input_close;
#endif

	/* register the input device to input sub-system */
	if (input_register_device(idev->input) < 0) {
		ipio_err("Failed to register touch input device\n");
		input_unregister_device(idev->input);
		input_free_device(idev->input);
	}
#if ILI_USE_ENABLE_NODE
    ret = sysfs_create_link(&ili_sec_info->sec.fac_dev->kobj,&idev->input->dev.kobj, "input");
    if (ret < 0) {
        ipio_err("%s: Failed to sysfs_create_link\n", __func__);
    }
#endif
}

#if REGULATOR_POWER
void ilitek_plat_regulator_power_on(bool status)
{
	ipio_info("%s\n", status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (idev->vdd) {
			if (regulator_enable(idev->vdd) < 0)
				ipio_err("regulator_enable VDD fail\n");
		}
		if (idev->vcc) {
			if (regulator_enable(idev->vcc) < 0)
				ipio_err("regulator_enable VCC fail\n");
		}
	} else {
		if (idev->vdd) {
			if (regulator_disable(idev->vdd) < 0)
				ipio_err("regulator_enable VDD fail\n");
		}
		if (idev->vcc) {
			if (regulator_disable(idev->vcc) < 0)
				ipio_err("regulator_enable VCC fail\n");
		}
	}
	atomic_set(&idev->ice_stat, DISABLE);
	mdelay(5);
}
#endif

#if REGULATOR_POWER
static void ilitek_plat_regulator_power_init(void)
{
	const char *vdd_name = "vdd";
	const char *vcc_name = "vcc";

	idev->vdd = regulator_get(idev->dev, vdd_name);
	if (ERR_ALLOC_MEM(idev->vdd)) {
		ipio_err("regulator_get VDD fail\n");
		idev->vdd = NULL;
	}
	if (regulator_set_voltage(idev->vdd, VDD_VOLTAGE, VDD_VOLTAGE) < 0)
		ipio_err("Failed to set VDD %d\n", VDD_VOLTAGE);

	idev->vcc = regulator_get(idev->dev, vcc_name);
	if (ERR_ALLOC_MEM(idev->vcc)) {
		ipio_err("regulator_get VCC fail.\n");
		idev->vcc = NULL;
	}
	if (regulator_set_voltage(idev->vcc, VCC_VOLTAGE, VCC_VOLTAGE) < 0)
		ipio_err("Failed to set VCC %d\n", VCC_VOLTAGE);

	ilitek_plat_regulator_power_on(true);
}
#endif

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;
	u32 flag;
	struct device_node *dev_node = idev->dev->of_node;
	struct device_node *chosen = NULL;

	idev->tp_int = of_get_named_gpio_flags(dev_node, DTS_INT_GPIO, 0, &flag);
	idev->tp_rst = of_get_named_gpio_flags(dev_node, DTS_RESET_GPIO, 0, &flag);

	ipio_info("TP INT: %d\n", idev->tp_int);
	ipio_info("TP RESET: %d\n", idev->tp_rst);

	if (!gpio_is_valid(idev->tp_int)) {
		ipio_err("Invalid INT gpio: %d\n", idev->tp_int);
		return -EBADR;
	}

	if (!gpio_is_valid(idev->tp_rst)) {
		ipio_err("Invalid RESET gpio: %d\n", idev->tp_rst);
		return -EBADR;
	}

	ret = gpio_request(idev->tp_int, "TP_INT");
	if (ret < 0) {
		ipio_err("Request IRQ GPIO failed, ret = %d\n", ret);
		gpio_free(idev->tp_int);
		ret = gpio_request(idev->tp_int, "TP_INT");
		if (ret < 0) {
			ipio_err("Retrying request INT GPIO still failed , ret = %d\n", ret);
			goto out;
		}
	}

	ret = gpio_request(idev->tp_rst, "TP_RESET");
	if (ret < 0) {
		ipio_err("Request RESET GPIO failed, ret = %d\n", ret);
		gpio_free(idev->tp_rst);
		ret = gpio_request(idev->tp_rst, "TP_RESET");
		if (ret < 0) {
			ipio_err("Retrying request RESET GPIO still failed , ret = %d\n", ret);
			goto out;
		}
	}
	/* hs70 add for HS70-1475 by liufurong at 20191123 start */
	chosen = of_find_node_by_name(NULL, "chosen");
	if (NULL == chosen)
		ipio_err("DT: chosen node is not found\n");
	else {
		of_property_read_string(chosen, "bootargs", &idev->panel_name);
		ipio_info("cmdline:%s\n", idev->panel_name);
	}
	/* hs70 add for HS70-1475 by liufurong at 20191123 end */
out:
	gpio_direction_input(idev->tp_int);
	return ret;
}

void ilitek_plat_irq_disable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&idev->irq_spin, flag);

	if (atomic_read(&idev->irq_stat) == DISABLE)
		goto out;

	if (!idev->irq_num) {
		ipio_err("gpio_to_irq (%d) is incorrect\n", idev->irq_num);
		goto out;
	}

	disable_irq_nosync(idev->irq_num);
	atomic_set(&idev->irq_stat, DISABLE);
	ipio_debug("Disable irq success\n");

out:
	spin_unlock_irqrestore(&idev->irq_spin, flag);
}

void ilitek_plat_irq_enable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&idev->irq_spin, flag);

	if (atomic_read(&idev->irq_stat) == ENABLE)
		goto out;

	if (!idev->irq_num) {
		ipio_err("gpio_to_irq (%d) is incorrect\n", idev->irq_num);
		goto out;
	}

	enable_irq(idev->irq_num);
	atomic_set(&idev->irq_stat, ENABLE);
	ipio_debug("Enable irq success\n");

out:
	spin_unlock_irqrestore(&idev->irq_spin, flag);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != idev->irq_num) {
		ipio_err("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&idev->mp_int_check) == ENABLE) {
		atomic_set(&idev->mp_int_check, DISABLE);
		ipio_debug("interrupt for mp test, ignore\n");
		wake_up(&(idev->inq));
		return IRQ_HANDLED;
	}

	if (idev->prox_near) {
		ipio_info("Proximity event, ignore interrupt!\n");
		return IRQ_HANDLED;
	}

	ipio_debug("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d\n",
			idev->report,
			atomic_read(&idev->tp_reset),
			atomic_read(&idev->fw_stat),
			atomic_read(&idev->tp_sw_mode),
			atomic_read(&idev->mp_stat),
			atomic_read(&idev->tp_sleep),
			atomic_read(&idev->esd_stat));

	if (!idev->report || atomic_read(&idev->tp_reset) ||
		atomic_read(&idev->fw_stat) || atomic_read(&idev->tp_sw_mode) ||
		atomic_read(&idev->mp_stat) || atomic_read(&idev->tp_sleep) ||
		atomic_read(&idev->esd_stat)) {
			ipio_debug("ignore interrupt !\n");
			return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&idev->touch_mutex)) {
		ipio_debug("touch is locked, ignore\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&idev->touch_mutex);
	ilitek_tddi_report_handler();
	mutex_unlock(&idev->touch_mutex);
	return IRQ_HANDLED;
}

void ilitek_plat_irq_unregister(void)
{
	devm_free_irq(idev->dev, idev->irq_num, NULL);
}

int ilitek_plat_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;

	atomic_set(&idev->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		idev->irq_num  = gpio_to_irq(idev->tp_int);
		get_irq_pin = true;
	}

	ipio_info("idev->irq_num = %d\n", idev->irq_num);

	ret = devm_request_threaded_irq(idev->dev, idev->irq_num,
				   ilitek_plat_isr_top_half,
				   ilitek_plat_isr_bottom_half,
				   type | IRQF_ONESHOT, "ilitek", NULL);

	if (type == IRQF_TRIGGER_FALLING)
		ipio_info("IRQ TYPE = IRQF_TRIGGER_FALLING\n");
	if (type == IRQF_TRIGGER_RISING)
		ipio_info("IRQ TYPE = IRQF_TRIGGER_RISING\n");

	if (ret != 0)
		ipio_err("Failed to register irq handler, irq = %d, ret = %d\n", idev->irq_num, ret);

	atomic_set(&idev->irq_stat, ENABLE);

	return ret;
}

#if defined(CONFIG_FB) || defined(CONFIG_DRM_MSM)
static int ilitek_plat_notifier_fb(struct notifier_block *self, unsigned long event, void *data)
{
	/* HS70 modify for HS70-292 by liufurong at 2019/10/24 start */
	int *blank = NULL;
	struct fb_event *evdata = data;
	if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK)) {
		ipio_info("event(%lu) do not need process\n", event);
		return 0;
	}
	/* HS70 modify for HS70-292 by liufurong at 2019/10/24 end */

	ipio_info("Notifier's event = %ld\n", event);

	/*
	 *	FB_EVENT_BLANK(0x09): A hardware display blank change occurred.
	 *	FB_EARLY_EVENT_BLANK(0x10): A hardware display blank early change occurred.
	 */
	if (evdata && evdata->data) {
		blank = evdata->data;
		switch (*blank) {
#ifdef CONFIG_DRM_MSM
		case MSM_DRM_BLANK_POWERDOWN:
#else
		case FB_BLANK_POWERDOWN:
#endif
#if CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_OFF:
#endif /* CONFIG_PLAT_SPRD */
			if (TP_SUSPEND_PRIO) {
#ifdef CONFIG_DRM_MSM
				if (event != MSM_DRM_EARLY_EVENT_BLANK)
#else
				if (event != FB_EARLY_EVENT_BLANK)
#endif
					return NOTIFY_DONE;
			} else {
#ifdef CONFIG_DRM_MSM
				if (event != MSM_DRM_EVENT_BLANK)
#else
				if (event != FB_EVENT_BLANK)
#endif
					return NOTIFY_DONE;
			}
			if (ilitek_tddi_sleep_handler(TP_SUSPEND) < 0)
				ipio_err("TP suspend failed\n");
			break;
#ifdef CONFIG_DRM_MSM
		case MSM_DRM_BLANK_UNBLANK:
		case MSM_DRM_BLANK_NORMAL:
#else
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
#endif

#if CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_ON:
#endif /* CONFIG_PLAT_SPRD */

#ifdef CONFIG_DRM_MSM
			if (event == MSM_DRM_EVENT_BLANK)
#else
			if (event == FB_EVENT_BLANK)
#endif
			{
				/* HS70 modify for HS70-586 by liufurong at 20191121 start */
				if (!idev->resume_by_ddi) {
					if (ilitek_tddi_sleep_handler(TP_RESUME) < 0)
						ipio_err("TP resume failed\n");
				}
				/* HS70 modify for HS70-586 by liufurong at 20191121 end */
			}
			break;
		default:
			ipio_err("Unknown event, blank = %d\n", *blank);
			break;
		}
	}
	return NOTIFY_OK;
}
#else
static void ilitek_plat_early_suspend(struct early_suspend *h)
{
	if (ilitek_tddi_sleep_handler(TP_SUSPEND) < 0)
		ipio_err("TP suspend failed\n");
}

static void ilitek_plat_late_resume(struct early_suspend *h)
{
	if (ilitek_tddi_sleep_handler(TP_RESUME) < 0)
		ipio_err("TP resume failed\n");
}
#endif

static void ilitek_plat_sleep_init(void)
{
#if defined(CONFIG_FB) || defined(CONFIG_DRM_MSM)
	ipio_info("Init notifier_fb struct\n");
	idev->notifier_fb.notifier_call = ilitek_plat_notifier_fb;
#ifdef CONFIG_DRM_MSM
		if (msm_drm_register_client(&idev->notifier_fb)) {
			ipio_err("msm_drm_register_client Unable to register fb_notifier\n");
		}
#else
#if CONFIG_PLAT_SPRD
	if (adf_register_client(&idev->notifier_fb))
		ipio_err("Unable to register notifier_fb\n");
#else
	if (fb_register_client(&idev->notifier_fb))
		ipio_err("Unable to register notifier_fb\n");
#endif /* CONFIG_PLAT_SPRD */
#endif /* CONFIG_DRM_MSM */
#else
	ipio_info("Init eqarly_suspend struct\n");
	idev->early_suspend.suspend = ilitek_plat_early_suspend;
	idev->early_suspend.resume = ilitek_plat_late_resume;
	idev->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	register_early_suspend(&idev->early_suspend);
#endif
}
/* HS70 code for HS70-133 by liufurong at 2019/10/31 start */
static int ilitek_pinctrl_init(void)
{
	int ret = 0;

	idev->ts_pinctrl = devm_pinctrl_get(idev->dev);
	if (IS_ERR_OR_NULL(idev->ts_pinctrl)) {
		ipio_err("Failed to get pinctrl");
		ret = PTR_ERR(idev->ts_pinctrl);
		idev->ts_pinctrl = NULL;
		return ret;
	}

	idev->int_default = pinctrl_lookup_state(idev->ts_pinctrl,
		"default");
	if (IS_ERR_OR_NULL(idev->int_default)) {
		ipio_err("Pin state[default] not found");
		ret = PTR_ERR(idev->int_default);
		goto exit_put;
	}

	idev->int_out_high = pinctrl_lookup_state(idev->ts_pinctrl,
		"int-output-high");
	if (IS_ERR_OR_NULL(idev->int_out_high)) {
		ipio_err("Pin state[int-output-high] not found");
		ret = PTR_ERR(idev->int_out_high);
	}

	idev->int_out_low = pinctrl_lookup_state(idev->ts_pinctrl,
		"int-output-low");
	if (IS_ERR_OR_NULL(idev->int_out_low)) {
		ipio_err("Pin state[int-output-low] not found");
		ret = PTR_ERR(idev->int_out_low);
		goto exit_put;
	}

	idev->int_input = pinctrl_lookup_state(idev->ts_pinctrl,
		"int-input");
	if (IS_ERR_OR_NULL(idev->int_input)) {
		ipio_err("Pin state[int-input] not found");
		ret = PTR_ERR(idev->int_input);
		goto exit_put;
	}

	return 0;
exit_put:
	devm_pinctrl_put(idev->ts_pinctrl);
	idev->ts_pinctrl = NULL;
	idev->int_default = NULL;
	idev->int_out_high = NULL;
	idev->int_out_low = NULL;
	idev->int_input = NULL;
	return ret;
}
/* HS70 code for HS70-133 by liufurong at 2019/10/31 end */
/* HS70 add for HS70-905 by liufurong at 20191113 start */
static int ilitek_charger_notifier_callback(struct notifier_block *nb,
								unsigned long val, void *v) {
	int ret = 0;
	struct power_supply *psy = NULL;
	union power_supply_propval prop;


	if(idev->fw_update_stat != 100)
		return 0;

	psy= power_supply_get_by_name("usb");
	if (!psy) {
		ipio_err("Couldn't get usbpsy\n");
		return -EINVAL;
	}
	if (!strcmp(psy->desc->name, "usb")) {
		if (psy && val == POWER_SUPPLY_PROP_STATUS) {
			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,&prop);
			if (ret < 0) {
				ipio_err("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", ret);
				return ret;
			} else {
				if(idev->usb_plug_status == 2)
					idev->usb_plug_status = prop.intval;
				if(idev->usb_plug_status != prop.intval) {
					ipio_info("usb prop.intval =%d\n", prop.intval);
					idev->usb_plug_status = prop.intval;
					if(!idev->tp_suspend && (idev->charger_notify_wq != NULL))
						queue_work(idev->charger_notify_wq,&idev->update_charger);
				}
			}
		}
	}
	return 0;
}

static void ilitek_update_charger(struct work_struct *work)
{
	int ret = 0;
	mutex_lock(&idev->touch_mutex);
	ret = ilitek_tddi_ic_func_ctrl("plug", !idev->usb_plug_status);// plug in
	if(ret<0) {
		ipio_err("Write plug in failed\n");
	}
	mutex_unlock(&idev->touch_mutex);
}
void ilitek_plat_charger_init(void)
{
	int ret = 0;
	idev->usb_plug_status = 2;
	idev->charger_notify_wq = create_singlethread_workqueue("ili_charger_wq");
	if (!idev->charger_notify_wq) {
		ipio_err("allocate ili_charger_notify_wq failed\n");
		return;
	}
	INIT_WORK(&idev->update_charger, ilitek_update_charger);
	idev->notifier_charger.notifier_call = ilitek_charger_notifier_callback;
	ret = power_supply_reg_notifier(&idev->notifier_charger);
	if (ret < 0)
		ipio_err("power_supply_reg_notifier failed\n");
}
/* HS70 add for HS70-905 by liufurong at 20191113 end */
/* hs70 add for HS70-1475 by liufurong at 20191123 start */
bool ilitek_get_module_info(void)
{
/* HS70 add for HS70-1717 by zhanghao at 20191129 start */
    if (NULL != strstr(idev->panel_name, "mdss_dsi_txd_ili7807g_hd_video")) {
		idev->ini_name = module_name_is_use[TXD_ILI7807G_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[TXD_ILI7807G_MODULE_NAME].module_bin_name;
		idev->size_ili = module_name_is_use[TXD_ILI7807G_MODULE_NAME].size_ili;
		CTPM_FW = CTPM_FW_TXD_ILI7807G;
        strcpy(idev->module_name,"ili7807g_txd");
        ipio_info("the specified display module is txd\n");
    }
    else if(NULL != strstr(idev->panel_name, "mdss_dsi_tm_ili7807g_hd_video")) {
		idev->ini_name = module_name_is_use[TM_ILI7807G_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[TM_ILI7807G_MODULE_NAME].module_bin_name;
		idev->size_ili = module_name_is_use[TM_ILI7807G_MODULE_NAME].size_ili;
		CTPM_FW = CTPM_FW_TM_ILI7807G;
        strcpy(idev->module_name,"ili7807g_tm");
        ipio_info("the specified display module is tm\n");
    }
	/*HS70 code for txd-cd bringup by gaozhengwei at 2020/03/26 start*/
    else if(NULL != strstr(idev->panel_name, "mdss_dsi_txd_cd_ili7807g_hd_video")) {
		idev->ini_name = module_name_is_use[TXD_CD_ILI7807G_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[TXD_CD_ILI7807G_MODULE_NAME].module_bin_name;
		idev->size_ili = module_name_is_use[TXD_CD_ILI7807G_MODULE_NAME].size_ili;
		CTPM_FW = CTPM_FW_TXD_CD_ILI7807G;
        strcpy(idev->module_name,"ili7807g_txd-cd");
        ipio_info("the specified display module is txd-cd\n");
    }
	/*HS70 code for txd-cd bringup by gaozhengwei at 2020/03/26 end*/
	else if (NULL != strstr(idev->panel_name, "mdss_dsi_txd12_ili7807g_hd_video")) {
		idev->ini_name = module_name_is_use[TXD12_ILI7807G_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[TXD12_ILI7807G_MODULE_NAME].module_bin_name;
		idev->size_ili = module_name_is_use[TXD12_ILI7807G_MODULE_NAME].size_ili;
		CTPM_FW = CTPM_FW_TXD12_ILI7807G;
        strcpy(idev->module_name,"ili7807g_txd_12");
        ipio_info("the specified display module is %s\n",idev->module_name);
    }
	else if (NULL != strstr(idev->panel_name, "mdss_dsi_txd15_ili7807g_hd_video")) {
		idev->ini_name = module_name_is_use[TXD15_ILI7807G_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[TXD15_ILI7807G_MODULE_NAME].module_bin_name;
		idev->size_ili = module_name_is_use[TXD15_ILI7807G_MODULE_NAME].size_ili;
		CTPM_FW = CTPM_FW_TXD15_ILI7807G;
        strcpy(idev->module_name,"ili7807g_txd_15");
        ipio_info("the specified display module is %s\n",idev->module_name);
    } else if (NULL != strstr(idev->panel_name, "mdss_dsi_inx18_ili7807g_hd_video")) {
		idev->ini_name = module_name_is_use[INX18_ILI7807G_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[INX18_ILI7807G_MODULE_NAME].module_bin_name;
		idev->size_ili = module_name_is_use[INX18_ILI7807G_MODULE_NAME].size_ili;
		CTPM_FW = CTPM_FW_INX18_ILI7807G;
		strcpy(idev->module_name,"ili7807g_inx_18");
		ipio_info("the specified display module is %s\n",idev->module_name);
    }
    else {
		idev->ini_name = module_name_is_use[UNKNOW_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[UNKNOW_MODULE_NAME].module_bin_name;
		idev->size_ili = module_name_is_use[UNKNOW_MODULE_NAME].size_ili;
		CTPM_FW = CTPM_FW_UNKNOW;
        strcpy(idev->module_name,"ili7807g_txd");
        ipio_err("can't find the specified display module .\n");
		return 0;
    }
	return 1;
/* HS70 add for HS70-1717 by zhanghao at 20191129 end */
}
/* hs70 add for HS70-1475 by liufurong at 20191123 end */
static int ilitek_plat_probe(void)
{
	/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 start */
	int ret = 0;
	/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 end */
	ipio_info("platform probe\n");

#if REGULATOR_POWER
	ilitek_plat_regulator_power_init();
#endif
/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 start */
#if ILI_POWER_SOURCE_CUST_EN
	atomic_set(&(idev->lcm_lab_power), 0);
	atomic_set(&(idev->lcm_ibb_power), 0);
	ret = ilitek_lcm_bias_power_init(idev);

	if (ret) {
		ipio_err("power resource init error!\n");
		goto err_power_resource_init_fail;
	}

	ilitek_lcm_power_source_ctrl(idev, 1);
#endif
/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 end */
/* HS70 code for HS70-133 by liufurong at 2019/10/31 start */
	if (ilitek_plat_gpio_register() < 0)
		ipio_err("Register gpio failed\n");

	if(ilitek_get_module_info() == 0)
		goto ili_probe_failed;
	/* HS70 add for HS70-1042 by gaozhengwei at 2019/11/12 start */
	ret = ilitek_pinctrl_init();
	if (!ret && idev->ts_pinctrl) {
		/*
		 * Pinctrl handle is optional. If pinctrl handle is found
		 * let pins to be configured in active state. If not
		 * found continue further without error.
		 */
		ret = pinctrl_select_state(idev->ts_pinctrl,idev->int_input);
		if (ret < 0) {
			ipio_err("failed to select pin to active state\n");
		}
	}
	/* HS70 add for HS70-1042 by gaozhengwei at 2019/11/12 end */
	if (ilitek_tddi_init() < 0) {
		ipio_err("platform probe failed\n");
		// huaqin add for HS70-35 bringup TP function by zhouzichun at 2019/10/8 start
		goto ili_probe_failed;
		// huaqin add for HS70-35 bringup TP function by zhouzichun at 2019/10/8 end
	}
	tp_is_used = ILITEK_ILI7807G;
	#ifdef CONFIG_HW_INFO
	set_tp_module_name(idev->module_name);
	#endif
	ilitek_plat_irq_register(idev->irq_tirgger_type);
	ilitek_plat_sleep_init();
	idev->dev_pm_suspend = false;
	init_completion(&idev->dev_pm_suspend_completion);
	/* HS70 add for HS70-905 by liufurong at 20191113 start */
	ilitek_plat_charger_init();
	/* HS70 add for HS70-905 by liufurong at 20191113 end */
/* HS70 code for HS70-133 by liufurong at 2019/10/31 end */
	return 0;
// huaqin add for HS70-35 bringup TP function by zhouzichun at 2019/10/8 start
ili_probe_failed:
	gpio_free(idev->tp_int);
	gpio_free(idev->tp_rst);
/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 start */
#if ILI_POWER_SOURCE_CUST_EN
	ilitek_lcm_power_source_ctrl(idev, 0);
	ilitek_lcm_bias_power_deinit(idev);
err_power_resource_init_fail:
#endif
/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 end */
	return -ENODEV;
// huaqin add for HS70-35 bringup TP function by zhouzichun at 2019/10/8 end
}

static int ilitek_plat_remove(void)
{
	ipio_info("remove plat dev\n");
	ilitek_tddi_dev_remove();
	/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 start */
	#if ILI_POWER_SOURCE_CUST_EN
		ilitek_lcm_bias_power_deinit(idev);
	#endif
	/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 end */
	return 0;
}

static const struct of_device_id tp_match_table[] = {
	{.compatible = DTS_OF_NAME},
	{},
};

static struct ilitek_hwif_info hwif = {
	.bus_type = TDDI_INTERFACE,
	.plat_type = TP_PLAT_QCOM,
	.owner = THIS_MODULE,
	.name = TDDI_DEV_ID,
	.of_match_table = of_match_ptr(tp_match_table),
	.plat_probe = ilitek_plat_probe,
	.plat_remove = ilitek_plat_remove,
};

static int __init ilitek_plat_dev_init(void)
{
	ipio_info("ILITEK TP driver init for QCOM\n");
	if (ilitek_tddi_dev_init(&hwif) < 0) {
		ipio_err("Failed to register i2c/spi bus driver\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit ilitek_plat_dev_exit(void)
{
	ipio_info("remove plat dev\n");
	ilitek_tddi_dev_remove();
}

module_init(ilitek_plat_dev_init);
module_exit(ilitek_plat_dev_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");
