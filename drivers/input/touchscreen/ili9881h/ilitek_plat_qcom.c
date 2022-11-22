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

#define DTS_INT_GPIO	"focaltech,irq-gpio"
#define DTS_RESET_GPIO	"focaltech,reset-gpio"
#define DTS_OF_NAME	    "focalteck9,fts9"
bool is_ilitek_tp = false;


unsigned char CTPM_FW_HLT[] = {
	#include "./fw/FW_TDDI_TRUNK_FB_HLT.ili"
};
unsigned char CTPM_FW_SKY[] = {
	#include "./fw/FW_TDDI_TRUNK_FB_SKY.ili"
};
//add for INX
unsigned char CTPM_FW_INX[] = {
    #include "./fw/FW_TDDI_TRUNK_FB_INX.ili"
};
//add for TRULY
unsigned char CTPM_FW_TRULY[] = {
    #include "./fw/FW_TDDI_TRUNK_FB_TRULY.ili"
};


struct module_name_use{
	char *module_bin_name;
	char *module_ini_name;
	int size_ili;
};

uint32_t name_module = -1;

enum module_name{
	HLT_MODULE_NAME = 0,
    SKY_MODULE_NAME = 1,
    INX_MODULE_NAME = 2,//add for INX
    TRULY_MODULE_NAME = 3,//add for TRULY
};

#define HLT_INI_NAME "/vendor/firmware/ilitek-ili9881h-hlt.ini"
#define SKY_INI_NAME "/vendor/firmware/ilitek-ili9881h-sky.ini"
#define INX_INI_NAME "/vendor/firmware/ilitek-ili9881h-inx.ini"//add for INX
#define TRULY_INI_NAME "/vendor/firmware/ilitek-ili9881h-truly.ini"//add for TRULY

#define HLT_BIN_NAME "ILITEK_ILI9881h_HLT.bin"
#define SKY_BIN_NAME "ILITEK_ILI9881H_SKY.bin"
#define INX_BIN_NAME "ILITEK_ILI9881H_INX.bin"//add for INX
#define TRULY_BIN_NAME "ILITEK_ILI9881H_TRULY.bin"//add for TRULY

struct module_name_use module_name_is_use[] = {
		{.module_bin_name = HLT_BIN_NAME, .module_ini_name = HLT_INI_NAME, .size_ili = sizeof(CTPM_FW_HLT)},
        {.module_bin_name = SKY_BIN_NAME, .module_ini_name = SKY_INI_NAME, .size_ili = sizeof(CTPM_FW_SKY)},
        {.module_bin_name = INX_BIN_NAME, .module_ini_name = INX_INI_NAME, .size_ili = sizeof(CTPM_FW_INX)},/*add for INX*/       
        {.module_bin_name = TRULY_BIN_NAME, .module_ini_name = TRULY_INI_NAME, .size_ili = sizeof(CTPM_FW_TRULY)},/*add for TRULY*/
};
static int ili_tp_get_boot_mode(void)
{
	char *bootmode_str= NULL;
	char charg_str[32] = " ";
	int rc;

	bootmode_str = strstr(saved_command_line,"androidboot.mode=");
	if(bootmode_str != NULL){
		strncpy(charg_str, bootmode_str+17, 7);
		rc = strncmp(charg_str, "charger", 7);
		if(rc == 0){
			pr_err("Offcharger mode!\n");
			return 1;
		}
	}
	return 0;
}


void ilitek_plat_tp_reset(void)
{
	ipio_info("edge delay = %d\n", idev->rst_edge_delay);
	gpio_direction_output(idev->tp_rst, 1);
	mdelay(1);
	gpio_set_value(idev->tp_rst, 0);
	mdelay(5);
	gpio_set_value(idev->tp_rst, 1);
	mdelay(idev->rst_edge_delay);
}

#if ENABLE_GESTURE
static int tid_gesture_set_capability(struct device *dev, bool enable)
{
    if (enable) {
        idev->gesture = ENABLE;
    }
    else {
        idev->gesture = DISABLE;
    }
    return 0;
}

static int gesture_get_capability(struct device *dev)
{
#ifdef WT_COMPILE_FACTORY_VERSION
    ipio_info("ATO version close gesture.");
    return 0;
#else
    ipio_info("none-ATO version close gesture.");
    return 0; //return BIT(GS_KEY_DOUBLE_TAP) to open.
#endif
}
#endif

void ilitek_plat_input_register(void)
{
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
	input_set_capability(idev->input, EV_KEY, KEY_POWER);
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

	/* register the input device to input sub-system */
	if (input_register_device(idev->input) < 0) {
		ipio_err("Failed to register touch input device\n");
		input_unregister_device(idev->input);
		input_free_device(idev->input);
	}
#if ENABLE_GESTURE
    idev->tid->tid_ops->gesture_set_capability = tid_gesture_set_capability;
    idev->tid->tid_ops->gesture_get_capability = gesture_get_capability;
#endif
}

#if ILI_PINCTRL_EN
int ili_pinctrl_init(struct ilitek_tddi_dev *ts)
{
    int ret = 0;

    ts->pinctrl = devm_pinctrl_get(ts->dev);
    if (IS_ERR_OR_NULL(ts->pinctrl)) {
        ipio_err("Failed to get pinctrl, please check dts");
        ret = PTR_ERR(ts->pinctrl);
        goto err_pinctrl_get;
    }

    ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
    if (IS_ERR_OR_NULL(ts->pins_active)) {
        ipio_err("Pin state[active] not found");
        ret = PTR_ERR(ts->pins_active);
        goto err_pinctrl_lookup;
    }

    ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
    if (IS_ERR_OR_NULL(ts->pins_suspend)) {
        ipio_err("Pin state[suspend] not found");
        ret = PTR_ERR(ts->pins_suspend);
        goto err_pinctrl_lookup;
    }

    ts->pins_release = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_release");
    if (IS_ERR_OR_NULL(ts->pins_release)) {
        ipio_err("Pin state[release] not found");
        ret = PTR_ERR(ts->pins_release);
    }

    return 0;
err_pinctrl_lookup:
    if (ts->pinctrl) {
        devm_pinctrl_put(ts->pinctrl);
    }
err_pinctrl_get:
    ts->pinctrl = NULL;
    ts->pins_release = NULL;
    ts->pins_suspend = NULL;
    ts->pins_active = NULL;
    return ret;
}

int ili_pinctrl_select_normal(struct ilitek_tddi_dev *ts)
{
    int ret = 0;

    if (ts->pinctrl && ts->pins_active) {
        ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
        if (ret < 0) {
            ipio_err("Set normal pin state error:%d", ret);
        }
    }
    return ret;
}

int ili_pinctrl_select_suspend(struct ilitek_tddi_dev *ts)
{
    int ret = 0;

    if (ts->pinctrl && ts->pins_suspend) {
        ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
        if (ret < 0) {
            ipio_err("Set suspend pin state error:%d", ret);
        }
    }
    return ret;
}

int ili_pinctrl_select_release(struct ilitek_tddi_dev *ts)
{
    int ret = 0;

    if (ts->pinctrl) {
        if (IS_ERR_OR_NULL(ts->pins_release)) {
            devm_pinctrl_put(ts->pinctrl);
            ts->pinctrl = NULL;
        } else {
            ret = pinctrl_select_state(ts->pinctrl, ts->pins_release);
            if (ret < 0)
                ipio_err("Set gesture pin state error:%d", ret);
        }
    }
    return ret;
}
#endif /* ILI_PINCTRL_EN */

#if REGULATOR_POWER
int  ilitek_plat_qcon_regulator_power_on(bool status)
{
    int rc = 0;
    
    if (IS_ERR_OR_NULL(idev->vddp) || IS_ERR_OR_NULL(idev->vddn)) {
        ipio_err("vdd is invalid");
        return -EINVAL;
    }
    
    ipio_info("%s\n", status ? "POWER ON" : "POWER OFF");

    if (status) {
        if (idev->power_disabled) {
            ipio_info("regulator enable !");
            rc = regulator_enable(idev->vddp);
            if ( rc != 0 ) {
                ipio_err("enable vsp fail\n");
            }
            rc = regulator_enable(idev->vddn);
            if ( rc != 0 ) {
                ipio_err("enable vsn fail\n");
            }

            idev->power_disabled = false;
        }
    } else {
        if (!idev->power_disabled) {
            ipio_info("regulator disable !");
            rc = regulator_disable(idev->vddp);
            if ( rc != 0 ) {
                ipio_err("enable vsp fail\n");
            }
            rc = regulator_disable(idev->vddn);
            if ( rc != 0 ) {
                ipio_err("enable vsn fail\n");
            }
            idev->power_disabled = true;
        }
    }
    return rc;
}

int ilitek_plat_qcon_regulator_power_init(bool status)
{
    int rc = 0;
    ipio_info("%s\n", status ? "POWER ENABLE" : "POWER DISABLE");
    if ( !status ) {
		ipio_info("power_init is deny\n");
        goto pwr_deny;
	}
    idev->vddp = regulator_get(&idev->spi->dev, "vddp");
	if ( IS_ERR(idev->vddp) ) {
        rc = PTR_ERR(idev->vddp);
		ipio_err("Regulator get failed idev->vddp rc=%d\n",rc);
		goto lab_pwr_err;
    }
    idev->vddn = regulator_get(&idev->spi->dev, "vddn");
	if ( IS_ERR(idev->vddn) ) {
        rc = PTR_ERR(idev->vddn);
		ipio_err("Regulator get failed idev->vddn rc=%d\n",rc);
		goto ibb_pwr_err;
    }
    ipio_info("power init end\n");
    return rc;

pwr_deny:
ibb_pwr_err:
	if (idev->vddn) {
		regulator_put(idev->vddn);
		idev->vddn = NULL;
	}
lab_pwr_err:
	if (idev->vddp) {
		regulator_put(idev->vddp);
		idev->vddp = NULL;
	}
    ipio_info("power exit end\n");
    return rc;

}
#endif
static int ilitek_plat_gpio_register(void)
{
	int ret = 0;
	u32 flag;
	struct device_node *dev_node = idev->dev->of_node;

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
	ipio_debug("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d\n",
			idev->report,
			atomic_read(&idev->tp_reset),
			atomic_read(&idev->fw_stat),
			atomic_read(&idev->tp_sw_mode),
			atomic_read(&idev->mp_stat),
			atomic_read(&idev->tp_sleep),
			atomic_read(&idev->esd_stat));

	if (irq != idev->irq_num) {
		ipio_err("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&idev->mp_int_check) == ENABLE) {
		atomic_set(&idev->mp_int_check, DISABLE);
		ipio_info("Get an INT for mp, ignore\n");
		return IRQ_HANDLED;
	}

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

static int ilitek_plat_irq_register(void)
{
	int ret = 0;

	idev->irq_num  = gpio_to_irq(idev->tp_int);

	ipio_info("idev->irq_num = %d\n", idev->irq_num);

	ret = devm_request_threaded_irq(idev->dev, idev->irq_num,
				   ilitek_plat_isr_top_half,
				   ilitek_plat_isr_bottom_half,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ilitek", NULL);

	if (ret != 0)
		ipio_err("Failed to register irq handler, irq = %d, ret = %d\n", idev->irq_num, ret);

	atomic_set(&idev->irq_stat, ENABLE);
	return ret;
}

#ifdef CONFIG_FB
static int ilitek_plat_notifier_fb(struct notifier_block *self, unsigned long event, void *data)
{
	int *blank;
	struct fb_event *evdata = data;

	if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK)) {
        ipio_err("event(%lu) do not need process\n", event);
        return 0;
    }

	/*
	 *	FB_EVENT_BLANK(0x09): A hardware display blank change occurred.
	 *	FB_EARLY_EVENT_BLANK(0x10): A hardware display blank early change occurred.
	 */
	if (evdata && evdata->data) {
		blank = evdata->data;
 	    ipio_info("Notifier's event = %ld  blank = %d\n", event, *blank);       
		switch (*blank) {
		case FB_BLANK_POWERDOWN:
#ifdef CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_OFF:
#endif /* CONFIG_PLAT_SPRD */
			if (TP_SUSPEND_PRIO) {
				if (event != FB_EARLY_EVENT_BLANK)
					return NOTIFY_DONE;
			} else {
				if (event != FB_EVENT_BLANK)
					return NOTIFY_DONE;
			}
			ilitek_tddi_sleep_handler(TP_SUSPEND);
			break;
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
#ifdef CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_ON:
#endif /* CONFIG_PLAT_SPRD */
			if (event == FB_EVENT_BLANK)
//				ilitek_tddi_sleep_handler(TP_RESUME);
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
	ilitek_tddi_sleep_handler(TP_SUSPEND);
}

static void ilitek_plat_late_resume(struct early_suspend *h)
{
	ilitek_tddi_sleep_handler(TP_RESUME);
}
#endif

static void ilitek_plat_sleep_init(void)
{
#ifdef CONFIG_FB
	ipio_info("Init notifier_fb struct\n");
	idev->notifier_fb.notifier_call = ilitek_plat_notifier_fb;
#ifdef CONFIG_PLAT_SPRD
	if (adf_register_client(&idev->notifier_fb))
		ipio_err("Unable to register notifier_fb\n");
#else
	if (fb_register_client(&idev->notifier_fb))
		ipio_err("Unable to register notifier_fb\n");
#endif /* CONFIG_PLAT_SPRD */
#else
	ipio_info("Init eqarly_suspend struct\n");
	idev->early_suspend.suspend = ilitek_plat_early_suspend;
	idev->early_suspend.resume = ilitek_plat_late_resume;
	idev->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	register_early_suspend(&idev->early_suspend);
#endif
}
/*add by machengcheng start*/
static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ilitek_tddi_dev *ili_data = container_of(sec, struct ilitek_tddi_dev, sec);
    char buff[18] = { 0 };
	struct touch_info_dev *tid = idev->tid;
	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s%02X",
			tid->product,
			tid->phone_fw_version);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ili_data->spi->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ilitek_tddi_dev *ili_data = container_of(sec, struct ilitek_tddi_dev, sec);
    char buff[18] = { 0 };
	struct touch_info_dev *tid = idev->tid;
	struct device *dev;
	unsigned int minor;
	int ret = -1;


	dev = tid_to_dev(tid);
	get_device(dev);
	ret = get_version(dev, NULL, &minor);
	if (ret) {
		dev_err(dev, "get version fail and set version 0\n");
		minor = 0;
	}
	put_device(dev);
	tid->part_fw_version = minor;

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s%02X",
			tid->product,
			tid->part_fw_version);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ili_data->spi->dev, "%s: %s\n", __func__, buff);
}

#if ENABLE_GESTURE
static void aot_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    struct ilitek_tddi_dev *ili_data = container_of(sec, struct ilitek_tddi_dev, sec);

    int ret = 0;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    unsigned int buf[4];
    unsigned int mask;

    sec_cmd_set_default_result(sec);

    buf[0] = sec->cmd_param[0];

    if (buf[0] == 1)
        mask = BIT(GS_KEY_ENABLE) | BIT(GS_KEY_DOUBLE_TAP);
    else
        mask = 0;

    ret = gesture_set_capability(&ili_data->tid->dev, mask);
    if (ret < 0) {
        input_err(true, &ili_data->spi->dev, "%s: failed. ret: %d\n",
                    __func__, ret);
        snprintf(buff, sizeof(buff), "%s", "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;

        goto out;
    }

    snprintf(buff, sizeof(buff), "%s", "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;

    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

out:
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);

    input_info(true, &ili_data->spi->dev, "%s: %s\n", __func__, buff);
}
#endif
static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ilitek_tddi_dev *ili_data = container_of(sec, struct ilitek_tddi_dev, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ili_data->spi->dev, "%s: %s\n", __func__, buff);
}


struct sec_cmd ili_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
#if ENABLE_GESTURE	
    {SEC_CMD("aot_enable", aot_enable),},
#endif   
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};
/*add by machengcheng end */

static int ilitek_plat_probe(struct touch_info_dev *tid)
{
	int ret = 0;
	char* cmdline_ilitp;
	char* module_name;
	ipio_info("platform probe\n");
	ret = ili_tp_get_boot_mode();
	if(ret == 1){
		ipio_info("offcharge mode,do not load tp probe ");
		return -1;
	}
	cmdline_ilitp = strstr(saved_command_line,"ili9881h_");
	ipio_info("cmdline_ilitp= %s",cmdline_ilitp);
	if (cmdline_ilitp == NULL){
		ipio_err("get ili,ili9881h_fail,this is not ilitek tp");
		return -1;
	}
	module_name = cmdline_ilitp + strlen("ili9881h_");
	ipio_info("module_name= %s",module_name);

	if(strncmp(module_name,"hlt_hdp_video",strlen("hlt_hdp_video"))== 0){
		name_module = HLT_MODULE_NAME;
		tid->vendor = "ilitek";
		tid->product = "ili9881h";
		ipio_info("is ILITEK HLT TP \n");
	}
	else if(strncmp(module_name,"inl_hdp_video",strlen("inl_hdp_video"))== 0){
		name_module = SKY_MODULE_NAME;
		tid->vendor = "ilitek";
		tid->product = "ili9881h";
		ipio_info("is ILITEK SKY TP \n");
	}
    else if(strncmp(module_name,"inx_hdp_video",strlen("inx_hdp_video"))== 0){
        name_module = INX_MODULE_NAME;
        tid->vendor = "ilitek";
        tid->product = "ili9881h";
        ipio_info("is ILITEK INX TP\n");
	}
    else if(strncmp(module_name,"truly_hdp_video",strlen("truly_hdp_video"))== 0){
        name_module = TRULY_MODULE_NAME;
        tid->vendor = "ilitek";
        tid->product = "ili9881h";
        ipio_info("is ILITEK TRULY TP\n");
    }
	else{
		ipio_err("this is not use module in this project");
		return -1;
	}
    
	if(name_module == HLT_MODULE_NAME){
		ipio_info("is ILITEK HLT TP\n");
		idev->bin_name = module_name_is_use[HLT_MODULE_NAME].module_bin_name;
		idev->ini_name = module_name_is_use[HLT_MODULE_NAME].module_ini_name;
		idev->size_ili = module_name_is_use[HLT_MODULE_NAME].size_ili;
		idev->ctpm_fw_use = CTPM_FW_HLT;
	}else if(name_module == SKY_MODULE_NAME){
		ipio_info("is ILITEK SKY TP\n");
		idev->bin_name = module_name_is_use[SKY_MODULE_NAME].module_bin_name;
		idev->ini_name = module_name_is_use[SKY_MODULE_NAME].module_ini_name;
		idev->size_ili = module_name_is_use[SKY_MODULE_NAME].size_ili;
		idev->ctpm_fw_use = CTPM_FW_SKY;
    }else if(name_module == INX_MODULE_NAME){
        ipio_info("is ILITEK INX TP\n");
        idev->bin_name = module_name_is_use[INX_MODULE_NAME].module_bin_name;
        idev->ini_name = module_name_is_use[INX_MODULE_NAME].module_ini_name;
        idev->size_ili = module_name_is_use[INX_MODULE_NAME].size_ili;
        idev->ctpm_fw_use = CTPM_FW_INX;
	}else if(name_module == TRULY_MODULE_NAME){
        ipio_info("is ILITEK TRULY TP\n");
        idev->bin_name = module_name_is_use[TRULY_MODULE_NAME].module_bin_name;
        idev->ini_name = module_name_is_use[TRULY_MODULE_NAME].module_ini_name;
        idev->size_ili = module_name_is_use[TRULY_MODULE_NAME].size_ili;
        idev->ctpm_fw_use = CTPM_FW_TRULY;
	}else{
		ipio_info("is not ILITEK TP\n");
		return -ENODEV;
	}
#if REGULATOR_POWER
	ret = ilitek_plat_qcon_regulator_power_init(ENABLE);
    if (ret) {
        ipio_err("fail to get power(regulator)");
        goto err_power_init;
    }
    idev->power_disabled = true;
    ret = ilitek_plat_qcon_regulator_power_on(ENABLE);
    if (ret) {
        ipio_err("fail to enable power(regulator)");
        goto err_power_init;
    }
#endif

#if ILI_PINCTRL_EN
        ili_pinctrl_init(idev);
        ili_pinctrl_select_normal(idev);
#endif

	ilitek_plat_gpio_register();

	if (ilitek_tddi_init() < 0) {
		ipio_err("platform probe failed\n");
		goto ilitek_plat_probe_error;
	}
	is_ilitek_tp = true;
#ifdef SEC_TSP_FACTORY_TEST
		ret = sec_cmd_init(&idev->sec, ili_commands,
				ARRAY_SIZE(ili_commands), SEC_CLASS_DEVT_TSP);
		if (ret < 0) {
			input_err(true, &idev->spi->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
			ret = -ENODEV;
			goto tid_register_failed;
		}
#endif
	
	ilitek_plat_irq_register();
	ilitek_plat_sleep_init();
	return 0;

tid_register_failed:
		devm_tid_unregister(&idev->spi->dev, tid);

ilitek_plat_probe_error:
	if (gpio_is_valid(idev->tp_int)) {
		gpio_free(idev->tp_int);
	}
	if (gpio_is_valid(idev->tp_rst)) {
		gpio_free(idev->tp_rst);
	}
#if REGULATOR_POWER
err_power_init:
    ilitek_plat_qcon_regulator_power_on(DISABLE);
    ilitek_plat_qcon_regulator_power_init(DISABLE);
#endif    
	return -ENODEV;
}

static int ilitek_plat_remove(void)
{
	ipio_info("remove plat dev\n");
	ilitek_tddi_dev_remove();
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
