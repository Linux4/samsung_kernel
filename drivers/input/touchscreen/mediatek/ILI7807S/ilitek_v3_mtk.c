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

#include "ilitek_v3.h"
#include "tpd.h"
#ifdef SEC_TSP_FACTORY_TEST
#include <linux/input/sec_cmd.h>
#endif

#define DTS_INT_GPIO	"touch,irq-gpio"
#define DTS_RESET_GPIO	"touch,reset-gpio"
#define DTS_OF_NAME	"mediatek,7807s_touch"
#define MTK_RST_GPIO	GTP_RST_PORT
#define MTK_INT_GPIO	GTP_INT_PORT

extern struct tpd_device *tpd;

void ili_tp_reset(void)
{
	ILI_INFO("edge delay = %d\n", ilits->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	tpd_gpio_output(ilits->tp_rst, 1);
	mdelay(1);
	tpd_gpio_output(ilits->tp_rst, 0);
	mdelay(5);
	tpd_gpio_output(ilits->tp_rst, 1);
	mdelay(ilits->rst_edge_delay);
}

void ili_input_register(void)
{
	int i;

	ilits->input = tpd->dev;

	if (tpd_dts_data.use_tpd_button) {
		for (i = 0; i < tpd_dts_data.tpd_key_num; i++)
			input_set_capability(ilits->input, EV_KEY, tpd_dts_data.tpd_key_local[i]);
	}

	/* set the supported event type for input device */
	set_bit(EV_ABS, ilits->input->evbit);
	set_bit(EV_SYN, ilits->input->evbit);
	set_bit(EV_KEY, ilits->input->evbit);
	set_bit(BTN_TOUCH, ilits->input->keybit);
	set_bit(BTN_TOOL_FINGER, ilits->input->keybit);
	set_bit(INPUT_PROP_DIRECT, ilits->input->propbit);

	if (MT_PRESSURE)
		input_set_abs_params(ilits->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (MT_B_TYPE) {
#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM);
#endif /* LINUX_VERSION_CODE */
	} else {
		input_set_abs_params(ilits->input, ABS_MT_TRACKING_ID, 0, MAX_TOUCH_NUM, 0, 0);
	}

	/* Gesture keys register */
#if 0  //only support double-click
	input_set_capability(ilits->input, EV_KEY, KEY_POWER);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_O);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_E);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_M);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_W);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_S);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_V);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_C);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_F);

	__set_bit(KEY_GESTURE_POWER, ilits->input->keybit);
	__set_bit(KEY_GESTURE_UP, ilits->input->keybit);
	__set_bit(KEY_GESTURE_DOWN, ilits->input->keybit);
	__set_bit(KEY_GESTURE_LEFT, ilits->input->keybit);
	__set_bit(KEY_GESTURE_RIGHT, ilits->input->keybit);
	__set_bit(KEY_GESTURE_O, ilits->input->keybit);
	__set_bit(KEY_GESTURE_E, ilits->input->keybit);
	__set_bit(KEY_GESTURE_M, ilits->input->keybit);
	__set_bit(KEY_GESTURE_W, ilits->input->keybit);
	__set_bit(KEY_GESTURE_S, ilits->input->keybit);
	__set_bit(KEY_GESTURE_V, ilits->input->keybit);
	__set_bit(KEY_GESTURE_Z, ilits->input->keybit);
	__set_bit(KEY_GESTURE_C, ilits->input->keybit);
	__set_bit(KEY_GESTURE_F, ilits->input->keybit);
#endif
	input_set_capability(ilits->input, EV_KEY, GESTURE_DOUBLE_TAP);
	input_set_capability(ilits->input, EV_KEY, KEY_BLACK_UI_GESTURE);
	__set_bit(GESTURE_DOUBLE_TAP, ilits->input->keybit);
	__set_bit(KEY_BLACK_UI_GESTURE, ilits->input->keybit);
}

#if REGULATOR_POWER
void ili_plat_regulator_power_on(bool status)
{
	ILI_INFO("%s\n", status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (ilits->vdd) {
			if (regulator_enable(ilits->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ilits->vcc) {
			if (regulator_enable(ilits->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	} else {
		if (ilits->vdd) {
			if (regulator_disable(ilits->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ilits->vcc) {
			if (regulator_disable(ilits->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	}
	atomic_set(&ilits->ice_stat, DISABLE);
	mdelay(5);
}

static void ilitek_plat_regulator_power_init(void)
{
	const char *vdd_name = "vdd";
	const char *vcc_name = "vcc";

	ilits->vdd = regulator_get(tpd->tpd_dev, vdd_name);
	if (ERR_ALLOC_MEM(ilits->vdd)) {
		ILI_ERR("regulator_get VDD fail\n");
		ilits->vdd = NULL;
	}

	tpd->reg = ilits->vdd;

	if (regulator_set_voltage(ilits->vdd, VDD_VOLTAGE, VDD_VOLTAGE) < 0)
		ILI_ERR("Failed to set VDD %d\n", VDD_VOLTAGE);

	ilits->vcc = regulator_get(ilits->dev, vcc_name);
	if (ERR_ALLOC_MEM(ilits->vcc)) {
		ILI_ERR("regulator_get VCC fail.\n");
		ilits->vcc = NULL;
	}
	if (regulator_set_voltage(ilits->vcc, VCC_VOLTAGE, VCC_VOLTAGE) < 0)
		ILI_ERR("Failed to set VCC %d\n", VCC_VOLTAGE);

	ili_plat_regulator_power_on(true);
}
#endif

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;

	ilits->tp_int = MTK_INT_GPIO;
	ilits->tp_rst = MTK_RST_GPIO;

	ILI_INFO("TP INT: %d\n", ilits->tp_int);
	ILI_INFO("TP RESET: %d\n", ilits->tp_rst);

	if (!gpio_is_valid(ilits->tp_int)) {
		ILI_ERR("Invalid INT gpio: %d\n", ilits->tp_int);
		return -EBADR;
	}

	if (!gpio_is_valid(ilits->tp_rst)) {
		ILI_ERR("Invalid RESET gpio: %d\n", ilits->tp_rst);
		return -EBADR;
	}

	gpio_direction_input(ilits->tp_int);
	return ret;
}


#ifdef SEC_TSP_FACTORY_TEST
extern bool gestrue_status;
extern bool gestrue_spay;
extern bool sensitivity_status;

static void get_fw_ver_bin(void *device_data)
{
  	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
  	char buff[32] = { 0 };
  
  	sec_cmd_set_default_result(sec);
  
  	snprintf(buff, sizeof(buff), "ILI7807S fw:%02X",
  			ilits->fw_info[50]);
  
  	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
  	sec->cmd_state = SEC_CMD_STATUS_OK;
  	ILI_INFO("%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
  	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
  	char buff[32] = { 0 };
  
  	sec_cmd_set_default_result(sec);
  
  	snprintf(buff, sizeof(buff), "ILI7807S fw:%02X",
  			ilits->fw_info[50]);
  
  	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
  	sec->cmd_state = SEC_CMD_STATUS_OK;
  	ILI_INFO("%s: %s\n", __func__, buff);
}

static void aot_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    unsigned int buf[4];

    ILI_INFO("aot_enable Enter.\n");
    sec_cmd_set_default_result(sec);

    buf[0] = sec->cmd_param[0];

    if(buf[0] == 1){
        ilits->gesture = ENABLE;
        gestrue_status = ilits->gesture;
    }else{
		ilits->gesture = DISABLE;
        gestrue_status = ilits->gesture;
    }

    snprintf(buff, sizeof(buff), "%s", "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;

    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    ILI_INFO( "%s: %s,sec->cmd_state == %d\n", __func__, buff, sec->cmd_state);
    sec_cmd_set_cmd_exit(sec);
}

static void spay_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    unsigned int buf[4];

    ILI_INFO("spay_enable Enter.\n");
    sec_cmd_set_default_result(sec);

    buf[0] = sec->cmd_param[0];

    if(buf[0] == 1){
        ilits->spay_gesture = ENABLE;
        gestrue_spay = ilits->spay_gesture;
    }else{
	ilits->spay_gesture = DISABLE;
        gestrue_spay = ilits->spay_gesture;
    }

    snprintf(buff, sizeof(buff), "%s", "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;

    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    ILI_INFO( "%s: %s,sec->cmd_state == %d\n", __func__, buff, sec->cmd_state);
    sec_cmd_set_cmd_exit(sec);
}
static void glove_mode(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    unsigned int buf[4];
    int ret = 0;
    ILI_INFO("sensitivity_enable Enter.\n");
    sec_cmd_set_default_result(sec);
    buf[0] = sec->cmd_param[0];
    if(buf[0] == 1){
	ret = ili_ic_func_ctrl("glove", 0x01);
	if(ret < 0)
		ILI_ERR("write edge_palm err\n");
        ilits->glove_mode = ENABLE;
        sensitivity_status = ilits->glove_mode;
    }else{
	ret = ili_ic_func_ctrl("glove", 0x00);
	if(ret < 0)
		ILI_ERR("write edge_palm err\n");
	ilits->glove_mode = DISABLE;
        sensitivity_status = ilits->glove_mode;
    }
    snprintf(buff, sizeof(buff), "%s", "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    ILI_INFO( "%s: %s,sec->cmd_state == %d\n", __func__, buff, sec->cmd_state);
    sec_cmd_set_cmd_exit(sec);
}
static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	ILI_INFO("%s: %s\n", __func__, buff);
}

struct sec_cmd ili_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("spay_enable", spay_enable),},
	{SEC_CMD("glove_mode",glove_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t ili_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 feature = 0;

	//if (ts->platdata->enable_settings_aot)
	feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	//input_info(true, &ts->client->dev, "%s: %d%s\n",
	//			__func__, feature,
	//			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "");
	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d\n", feature);
}

static ssize_t ili_scrub_pos(struct device *dev,
	struct device_attribute *attr,char *buf)
{
	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", "4 0 0");
}

static DEVICE_ATTR(support_feature, 0444, ili_support_feature, NULL);
static DEVICE_ATTR(scrub_pos, 0444, ili_scrub_pos, NULL);

static struct attribute *ili_cmd_attributes[] = {
	&dev_attr_support_feature.attr,
	&dev_attr_scrub_pos.attr,
	NULL,
};

static struct attribute_group ili_cmd_attr_group = {
	.attrs = ili_cmd_attributes,
};
#endif

void ili_irq_disable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == DISABLE)
		goto out;

	if (!ilits->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ilits->irq_num);
		goto out;
	}

	disable_irq_nosync(ilits->irq_num);
	atomic_set(&ilits->irq_stat, DISABLE);
	ILI_DBG("Disable irq success\n");

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

void ili_irq_enable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == ENABLE)
		goto out;

	if (!ilits->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ilits->irq_num);
		goto out;
	}

	enable_irq(ilits->irq_num);
	atomic_set(&ilits->irq_stat, ENABLE);
	ILI_DBG("Enable irq success\n");

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != ilits->irq_num) {
		ILI_ERR("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&ilits->cmd_int_check) == ENABLE) {
		atomic_set(&ilits->cmd_int_check, DISABLE);
		ILI_DBG("CMD INT detected, ignore\n");
		wake_up(&(ilits->inq));
		return IRQ_HANDLED;
	}

	if (ilits->prox_near) {
		ILI_INFO("Proximity event, ignore interrupt!\n");
		return IRQ_HANDLED;
	}

	ILI_DBG("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d, igr:%d\n",
			ilits->report,
			atomic_read(&ilits->tp_reset),
			atomic_read(&ilits->fw_stat),
			atomic_read(&ilits->tp_sw_mode),
			atomic_read(&ilits->mp_stat),
			atomic_read(&ilits->tp_sleep),
			atomic_read(&ilits->esd_stat),
			atomic_read(&ilits->ignore_report));

	if (!ilits->report || atomic_read(&ilits->tp_reset) ||  atomic_read(&ilits->ignore_report) ||
		atomic_read(&ilits->fw_stat) || atomic_read(&ilits->tp_sw_mode) ||
		atomic_read(&ilits->mp_stat) || atomic_read(&ilits->tp_sleep) ||
		atomic_read(&ilits->esd_stat)) {
			ILI_DBG("ignore interrupt !\n");
			return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&ilits->touch_mutex)) {
		ILI_DBG("touch is locked, ignore\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&ilits->touch_mutex);
	ili_report_handler();
	mutex_unlock(&ilits->touch_mutex);
	return IRQ_HANDLED;
}

void ili_irq_unregister(void)
{
	devm_free_irq(ilits->dev, ilits->irq_num, NULL);
}

int ili_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;
	struct device_node *node;

	atomic_set(&ilits->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		node = of_find_matching_node(NULL, touch_of_match);
		if (node)
			ilits->irq_num = irq_of_parse_and_map(node, 0);

		ILI_INFO("ilits->irq_num = %d\n", ilits->irq_num);
		get_irq_pin = true;
	}

	ret = devm_request_threaded_irq(ilits->dev, ilits->irq_num,
				ilitek_plat_isr_top_half,
				ilitek_plat_isr_bottom_half,
				type | IRQF_ONESHOT, "ilitek", NULL);

	if (type == IRQF_TRIGGER_FALLING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_FALLING\n");
	if (type == IRQF_TRIGGER_RISING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_RISING\n");

	if (ret != 0)
		ILI_ERR("Failed to register irq handler, irq = %d, ret = %d\n", ilits->irq_num, ret);

	atomic_set(&ilits->irq_stat, ENABLE);

	return ret;
}

void tpd_resume(struct device *h)
{
	if (ili_sleep_handler(TP_RESUME) < 0)
		ILI_ERR("TP resume failed\n");
}

void tpd_suspend(struct device *h)
{
	if (ili_sleep_handler(TP_DEEP_SLEEP) < 0)
		ILI_ERR("TP suspend failed\n");
}

static int ilitek_tp_pm_suspend(struct device *dev)
{
	ILI_INFO("CALL BACK TP PM SUSPEND");
	ilits->pm_suspend = true;
	reinit_completion(&ilits->pm_completion);
	return 0;
}

static int ilitek_tp_pm_resume(struct device *dev)
{
	ILI_INFO("CALL BACK TP PM RESUME");
	ilits->pm_suspend = false;
	complete(&ilits->pm_completion);
	return 0;
}
static int ilitek_plat_probe(void)
{
#ifdef SEC_TSP_FACTORY_TEST
	int ret = 0;
#endif

	ILI_INFO("platform probe\n");

#if REGULATOR_POWER
	ilitek_plat_regulator_power_init();
#endif

	if (ilitek_plat_gpio_register() < 0)
		ILI_ERR("Register gpio failed\n");

	ili_irq_register(ilits->irq_tirgger_type);

	if (ili_tddi_init() < 0) {
		ILI_ERR("ILITEK Driver probe failed\n");
		ili_irq_unregister();
		ili_dev_remove(DISABLE);
		return -ENODEV;
	}
	tpd_load_status = 1;
	ilits->pm_suspend = false;
	init_completion(&ilits->pm_completion);
#if CHARGER_NOTIFIER_CALLBACK
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
	/* add_for_charger_start */
	ilitek_plat_charger_init();
	/* add_for_charger_end */
#endif
#endif
#ifdef SEC_TSP_FACTORY_TEST
	ret = sec_cmd_init(&ilits->sec, ili_commands, ARRAY_SIZE(ili_commands), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		ILI_ERR("%s: Failed to sec_cmd_init\n", __func__);
		return -ENODEV;
	}

	ret = sysfs_create_group(&ilits->sec.fac_dev->kobj, &ili_cmd_attr_group);
	if (ret) {
		ILI_ERR( "%s: failed to create sysfs attributes\n",
			__func__);
			goto out;
	}
#endif

	ILI_INFO("ILITEK Driver loaded successfully!");
	return 0;

#ifdef SEC_TSP_FACTORY_TEST
out:
	sec_cmd_exit(&ilits->sec, SEC_CLASS_DEVT_TSP);
	return 0;
#endif
}

static int ilitek_plat_remove(void)
{
	ILI_INFO("remove plat dev\n");
	ili_dev_remove(ENABLE);
#ifdef SEC_TSP_FACTORY_TEST
	sysfs_remove_group(&ilits->sec.fac_dev->kobj, &ili_cmd_attr_group);
	sec_cmd_exit(&ilits->sec, SEC_CLASS_DEVT_TSP);
#endif
	return 0;
}

static const struct dev_pm_ops tp_pm_ops = {
	.suspend = ilitek_tp_pm_suspend,
	.resume = ilitek_tp_pm_resume,
};

static const struct of_device_id tp_match_table[] = {
	{.compatible = DTS_OF_NAME},
	{},
};

#ifdef ROI
struct ts_device_ops ilitek_ops = {
    .chip_roi_rawdata = ili_knuckle_roi_rawdata,
    .chip_roi_switch = ili_knuckle_roi_switch,
};
#endif

static struct ilitek_hwif_info hwif = {
	.bus_type = TDDI_INTERFACE,
	.plat_type = TP_PLAT_MTK,
	.owner = THIS_MODULE,
	.name = TDDI_DEV_ID,
	.of_match_table = of_match_ptr(tp_match_table),
	.plat_probe = ilitek_plat_probe,
	.plat_remove = ilitek_plat_remove,
	.pm = &tp_pm_ops,
};

static int tpd_local_init(void)
{
	ILI_INFO("TPD init device driver\n");

	if (ili_dev_init(&hwif) < 0) {
		ILI_ERR("Failed to register i2c/spi bus driver\n");
		return -ENODEV;
	}
	if (tpd_load_status == 0) {
		ILI_ERR("Add error touch panel driver\n");
		return -1;
	}
	if (tpd_dts_data.use_tpd_button) {
		tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
				   tpd_dts_data.tpd_key_dim_local);
	}
	tpd_type_cap = 1;
	return 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = TDDI_DEV_ID,
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

static int __init ilitek_plat_dev_init(void)
{
	int ret = 0;

	if (!strstr(saved_command_line,"ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_tianma"))
	{ if(!strstr(saved_command_line,"ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_chuangwei"))
		{
	return -1;	
		}
	}
	ILI_INFO("ILITEK TP driver init for MTK\n");
	tpd_get_dts_info();
	ret = tpd_driver_add(&tpd_device_driver);
	if (ret < 0) {
		ILI_ERR("ILITEK add TP driver failed\n");
		tpd_driver_remove(&tpd_device_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit ilitek_plat_dev_exit(void)
{
	ILI_INFO("ilitek driver has been removed\n");
	tpd_driver_remove(&tpd_device_driver);
}

module_init(ilitek_plat_dev_init);
module_exit(ilitek_plat_dev_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");
