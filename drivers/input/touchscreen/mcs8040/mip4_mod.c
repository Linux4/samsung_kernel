/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4_mod.c : Model dependent functions
 *
 */

#include "mip4.h"

/**
* Control regulator
*/

int mip_regulator_control(struct mip_ts_info *info, int enable)
{
	struct i2c_client *client = info->client;
	static struct regulator *tsp_vdd;
	int ret = 0;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	if (!tsp_vdd) {
		tsp_vdd = regulator_get(&client->dev, "vddsim2");
		if (IS_ERR(tsp_vdd)) {
			tsp_vdd = NULL;
			dev_err(&client->dev, "%s [ERROR] regulator_get\n", __func__);
			return -1;
		}

		ret = regulator_set_voltage(tsp_vdd, 3000000, 3000000);
		if (ret) {
			dev_err(&client->dev, "%s [ERROR] regulator_set_voltage\n", __func__);
			return -1;
		}
	}

	if (enable) {
		if (!regulator_is_enabled(tsp_vdd)) {
			ret = regulator_enable(tsp_vdd);
			if (ret) {
				dev_err(&client->dev, "%s [ERROR] regulator_enable [%d]\n", __func__, ret);
				return -1;
			}
		}
	} else {
		if (regulator_is_enabled(tsp_vdd)) {
			ret = regulator_disable(tsp_vdd);
			if (ret) {
				dev_err(&client->dev, "%s [ERROR] regulator_disable [%d]\n", __func__, ret);
				return -1;
			}
		}
	}

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Turn off power supply
*/
int mip_power_off(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (mip_regulator_control(info, 0)) {
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
		return -1;
	}

	msleep(10);

	dev_info(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Turn on power supply
*/
int mip_power_on(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (mip_regulator_control(info, 1)) {
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
		return -1;
	}

	msleep(200);

	dev_info(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Clear touch input event status
*/
void mip_clear_input(struct mip_ts_info *info)
{
	int i;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	for (i = 0; i < MAX_FINGER_NUM; i++) {

#if MIP_INPUT_REPORT_TYPE
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
#else
		input_report_key(info->input_dev, BTN_TOUCH, 0);
#endif

		info->touch_state[i] = 0;
	}
	info->finger_cnt = 0;

	if (info->pdata->key_flag == true) {
		if (info->key_enable == true) {
			for (i = 0; i < MAX_KEY_NUM; i++)
				input_report_key(info->input_dev, info->key_code[i], 0);
		}
	}
	if (info->proximity_state)
		input_report_key(info->input_dev, info->key_code[MIP_EVENT_INPUT_TYPE_PROXIMITY], 0);

	input_sync(info->input_dev);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return;
}

/**
* Input event handler - Report input event
*/
void mip_input_event_handler(struct mip_ts_info *info, u8 sz, u8 *buf)
{
	int i;
	int type;
	int id;
	int hover = 0;
	int palm = 0;
	int state = 0;
	int x, y, z = 0;
	int size = 0;
	int pressure_stage = 0;
	int pressure = 0;
	int touch_major = 0;
	int touch_minor = 0;
#if !MIP_INPUT_REPORT_TYPE
	int finger_id = 0;
	int finger_cnt = 0;
#endif

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	for (i = 0; i < sz; i += info->event_size) {
		u8 *packet = &buf[i];

		if ((info->event_format == 0) || (info->event_format == 1)) {
			type = (packet[0] & 0x40) >> 6;
		} else if (info->event_format == 3) {
			type = (packet[0] & 0xF0) >> 4;
		} else {
			dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
			goto ERROR;
		}
		dev_dbg(&info->client->dev, "%s - Type[%d]\n", __func__, type);

		if (type == MIP_EVENT_INPUT_TYPE_KEY && info->pdata->key_flag == true) {

			if (info->pdata->key_flag == true) {
				if ((info->event_format == 0) || (info->event_format == 1)) {
					id = packet[0] & 0x0F;
					state = (packet[0] & 0x80) >> 7;
				} else if (info->event_format == 3) {
					id = packet[0] & 0x0F;
					state = (packet[1] & 0x01);
				} else {
					dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
					goto ERROR;
				}

				if ((id >= 1) && (id <= info->key_num)) {
					int keycode = info->key_code[id - 1];

					input_report_key(info->input_dev, keycode, state);

					dev_dbg(&info->client->dev, "%s - Key : ID[%d] Code[%d] Event[%d]\n", __func__, id, keycode, state);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
					dev_info(&info->client->dev, "%s - Key : Event[%s]\n", __func__, state ? "P" : "R");
#else
					dev_info(&info->client->dev, "%s - Key : ID[%d] Code[%d] Event[%s]\n", __func__, id, keycode, state ? "P" : "R");
#endif
				} else {
					dev_err(&info->client->dev, "%s [ERROR] Unknown Key ID [%d]\n", __func__, id);
					continue;
				}
			}
		} else if (type == MIP_EVENT_INPUT_TYPE_SCREEN) {
			if (info->event_format == 0) {
				state = (packet[0] & 0x80) >> 7;
				hover = (packet[0] & 0x20) >> 5;
				palm = (packet[0] & 0x10) >> 4;
				id = (packet[0] & 0x0F) - 1;
				x = ((packet[1] & 0x0F) << 8) | packet[2];
				y = (((packet[1] >> 4) & 0x0F) << 8) | packet[3];
				pressure = packet[4];
				size = packet[5];
				touch_major = packet[5];
				touch_minor = packet[5];
			} else if (info->event_format == 1) {
				state = (packet[0] & 0x80) >> 7;
				hover = (packet[0] & 0x20) >> 5;
				palm = (packet[0] & 0x10) >> 4;
				id = (packet[0] & 0x0F) - 1;
				x = ((packet[1] & 0x0F) << 8) | packet[2];
				y = (((packet[1] >> 4) & 0x0F) << 8) | packet[3];
				pressure = packet[4];
				size = packet[5];
				touch_major = packet[6];
				touch_minor = packet[7];
			} else if (info->event_format == 3) {
				id = (packet[0] & 0x0F) - 1;
				hover = (packet[1] & 0x04) >> 2;
				palm = (packet[1] & 0x02) >> 1;
				state = (packet[1] & 0x01);
				x = ((packet[2] & 0x0F) << 8) | packet[3];
				y = (((packet[2] >> 4) & 0x0F) << 8) | packet[4];
				z = packet[5];
				size = packet[6];
				pressure_stage = (packet[7] & 0xF0) >> 4;
				pressure = ((packet[7] & 0x0F) << 8) | packet[8];
				touch_major = packet[9];
				touch_minor = packet[10];
			} else {
				dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
				goto ERROR;
			}

			if (state == 1) {
#if MIP_INPUT_REPORT_TYPE
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, true);
				input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
#ifdef CONFIG_SEC_FACTORY
				input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
#endif
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, touch_minor);
#else
#ifdef CONFIG_SEC_FACTORY
				input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
#endif
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, touch_minor);
				input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, id);
				input_report_key(info->input_dev, BTN_TOUCH, 1);
				input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
				input_mt_sync(info->input_dev);
#endif

				dev_dbg(&info->client->dev, "%s - Screen : ID[%d] X[%d] Y[%d] Z[%d] Major[%d] Minor[%d] Size[%d] Pressure[%d] Palm[%d] Hover[%d]\n",
					__func__, id, x, y, pressure, touch_major, touch_minor, size, pressure, palm, hover);
				if (!info->touch_state[id]) {
					info->finger_cnt++;
					info->touch_state[id] = true;
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
					dev_info(&info->client->dev, "%s [ TSP ] state : Press\n", __func__);

#else
					dev_info(&info->client->dev, "%s [ TSP ] ID[%d] X[%d] Y[%d] Z[%d] state : Press\n",
						__func__, id, x, y, pressure);

#endif
				}
			} else if (state == 0) {
#if MIP_INPUT_REPORT_TYPE
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
#else
				input_report_key(info->input_dev, BTN_TOUCH, 0);
				input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
				input_mt_sync(info->input_dev);
#endif
				info->finger_cnt--;
				info->touch_state[id] = false;

				dev_dbg(&info->client->dev, "%s - Screen : ID[%d] Release\n", __func__, id);

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
				dev_info(&info->client->dev, "%s [ TSP ] state : Release\n", __func__);

#else
				dev_info(&info->client->dev, "%s [ TSP ] ID[%d] X[%d] Y[%d] Z[%d] state : Release\n",
					__func__, id, x, y, pressure);
#endif


#if !MIP_INPUT_REPORT_TYPE
				finger_cnt = 0;
				for (finger_id = 0; finger_id < MAX_FINGER_NUM; finger_id++) {
					if (info->touch_state[finger_id] != 0) {
						finger_cnt++;
						break;
					}
				}

				if (finger_cnt == 0) {
					input_report_key(info->input_dev, BTN_TOUCH, 0);
					dev_dbg(&info->client->dev, "%s - Screen : Release\n", __func__);
				}
#endif
			} else {
				dev_err(&info->client->dev, "%s [ERROR] Unknown event state [%d]\n", __func__, state);
				goto ERROR;
			}
		} else if (type == MIP_EVENT_INPUT_TYPE_PROXIMITY) {
			if (info->pdata->proximity_flag) {
				int keycode = info->key_code[MIP_EVENT_INPUT_TYPE_PROXIMITY];
				state = (packet[1] & 0x01);
				z = packet[5];

				input_report_key(info->input_dev, keycode, state);

				dev_info(&info->client->dev, "%s - Proximity : Code[%d] State[%d] Value[%d]\n", __func__, keycode, state, z);
				/* dev_dbg(&info->client->dev, "%s - Proximity : State[%d] Value[%d]\n", __func__, state, z); */

				info->proximity_state = state;
				info->proximity_value = z;
			}
		} else {
			dev_err(&info->client->dev, "%s [ERROR] Unknown event type [%d]\n", __func__, type);
			goto ERROR;
		}
	}

	input_sync(info->input_dev);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return;
}

/**
* Wake-up gesture event handler
*/
int mip_gesture_wakeup_event_handler(struct mip_ts_info *info, int gesture_code)
{
	u8 wbuf[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	dev_dbg(&info->client->dev, "%s - gesture[%d]\n", __func__, gesture_code);

	info->wakeup_gesture_code = gesture_code;

	switch (gesture_code) {
	case MIP_EVENT_GESTURE_C:
	case MIP_EVENT_GESTURE_W:
	case MIP_EVENT_GESTURE_V:
	case MIP_EVENT_GESTURE_M:
	case MIP_EVENT_GESTURE_S:
	case MIP_EVENT_GESTURE_Z:
	case MIP_EVENT_GESTURE_O:
	case MIP_EVENT_GESTURE_E:
	case MIP_EVENT_GESTURE_V_90:
	case MIP_EVENT_GESTURE_V_180:
	case MIP_EVENT_GESTURE_FLICK_RIGHT:
	case MIP_EVENT_GESTURE_FLICK_DOWN:
	case MIP_EVENT_GESTURE_FLICK_LEFT:
	case MIP_EVENT_GESTURE_FLICK_UP:
	case MIP_EVENT_GESTURE_DOUBLE_TAP:
		input_report_key(info->input_dev, KEY_POWER, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_POWER, 0);
		input_sync(info->input_dev);
		break;

	default:
		wbuf[0] = MIP_R0_CTRL;
		wbuf[1] = MIP_R1_CTRL_POWER_STATE;
		wbuf[2] = MIP_CTRL_POWER_LOW;
		if (mip_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
			goto ERROR;
		}

		break;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	return 1;
}

#if MIP_USE_DEVICETREE
/**
* Parse device tree
*/
int mip_parse_devicetree(struct device *dev, struct mip_ts_info *info)
{
	struct device_node *np = dev->of_node;
	int ret;
	u32 val;

	ret = of_property_read_u32(np, MIP_DEVICE_NAME",max_x", &val);
	if (ret) {
		dev_err(dev, "failed to read max_x\n");
		info->pdata->max_x = 480;
	} else {
		info->pdata->max_x = val;
	}

	ret = of_property_read_u32(np, MIP_DEVICE_NAME",max_y", &val);
	if (ret) {
		dev_err(dev, "failed to read max_y\n");
		info->pdata->max_y = 800;
	} else {
		info->pdata->max_y = val;
	}

	ret = of_property_read_u32(np, MIP_DEVICE_NAME",x_num", &val);
	if (ret)
		dev_err(dev, "failed to read x_num\n");
	else
		info->pdata->x_num = val;

	ret = of_property_read_u32(np, MIP_DEVICE_NAME",y_num", &val);
	if (ret)
		dev_err(dev, "failed to read y_num\n");
	else
		info->pdata->y_num = val;

	info->pdata->key_flag = of_property_read_bool(np, MIP_DEVICE_NAME",key_flag");
	info->pdata->proximity_flag = of_property_read_bool(np, MIP_DEVICE_NAME",proximity_flag");

	ret  = of_property_read_string(np, MIP_DEVICE_NAME",fw_name", &info->pdata->fw_name);
	if (ret < 0) {
		dev_err(dev, "failed to get fw_name\n");
		return -EINVAL;
	}
	ret = of_property_read_string(np, MIP_DEVICE_NAME",ext_fw_name", &info->pdata->ext_fw_name);
	if (ret < 0) {
		dev_err(dev, "failed to get ext_fw_name\n");
		info->pdata->ext_fw_name = "/sdcard/j1mini3g.bin";
	}

	ret = of_get_named_gpio(np, MIP_DEVICE_NAME",irq-gpio", 0);
	if (!gpio_is_valid(ret)) {
		dev_err(dev, "failed to get irq-gpio\n");
		return -EINVAL;
	} else
		info->pdata->gpio_intr = ret;

	ret = gpio_request(info->pdata->gpio_intr, "mip4_intr");
	if (ret < 0) {
		dev_err(dev, "failed to gpio_request : irq-gpio\n");
		return -EINVAL;
	}

	/*ret = of_get_named_gpio(np, MIP_DEVICE_NAME",scl-gpio", 0);
	if (!gpio_is_valid(ret)) {
		dev_err(dev, "failed to get scl-gpio\n");
		return -EINVAL;
	} else
		info->pdata->gpio_scl = ret;

	ret = gpio_request(info->pdata->gpio_scl , "mip4_scl");
	if (ret < 0) {
		dev_err(dev, "failed to gpio_request : scl-gpio\n");
		return -EINVAL;
	}

	ret = of_get_named_gpio(np, MIP_DEVICE_NAME",sda-gpio", 0);
	if (!gpio_is_valid(ret)) {
		dev_err(dev, "failed to get sda-gpio\n");
		return -EINVAL;
	} else
		info->pdata->gpio_sda = ret;

	ret = gpio_request(info->pdata->gpio_sda, "mip4_sda");
	if (ret < 0) {
		dev_err(dev, "failed to gpio_request : sda-gpio\n");
		return -EINVAL;
	}*/

	return 0;
}
#endif

/**
* Config input interface
*/
void mip_config_input(struct mip_ts_info *info)
{
	struct input_dev *input_dev = info->input_dev;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

#if MIP_INPUT_REPORT_TYPE
	input_mt_init_slots(input_dev, MAX_FINGER_NUM, 0);
#else
	set_bit(BTN_TOUCH, input_dev->keybit);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, MAX_FINGER_NUM, 0, 0);
#endif

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, info->max_y, 0, 0);
#ifdef CONFIG_SEC_FACTORY
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, INPUT_PRESSURE_MAX, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, INPUT_TOUCH_MAJOR_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, INPUT_TOUCH_MINOR_MAX, 0, 0);

	if (info->pdata->key_flag == true) {
		set_bit(KEY_BACK, input_dev->keybit);
		set_bit(KEY_RECENT, input_dev->keybit);

		info->key_code[0] = KEY_RECENT;
		info->key_code[1] = KEY_BACK;
	} else {
		pr_info("[TSP] key do not setup\n");
	}

	if (info->pdata->proximity_flag == true) {
		set_bit(KEY_PROXIMITY, input_dev->keybit);
		info->key_code[2] = KEY_PROXIMITY;
	} else {
		pr_info("[TSP] proximity do not setup\n");
	}

#if MIP_USE_WAKEUP_GESTURE
	set_bit(KEY_POWER, input_dev->keybit);
#endif
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;
}

#if MIP_USE_CALLBACK
/**
* Callback - get charger status
*/
void mip_callback_charger(struct mip_callbacks *cb, int charger_status)
{
	struct mip_ts_info *info = container_of(cb, struct mip_ts_info, cb);
	u8 wbuf[3];
	u8 rbuf[1];
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_CHARGER_MODE;

	if (mip_i2c_read(info, wbuf, 2, rbuf, 1))
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);

	if (rbuf[0] != charger_status) {
		wbuf[2] = charger_status;

		if (mip_i2c_write(info, wbuf, 3))
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		dev_info(&info->client->dev, "[TSP]%s - change charger_status[%d]\n", __func__, charger_status);
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

int mip_callback_notifier(struct notifier_block *nb, unsigned long noti_type, void *v)
{
	struct mip_callbacks *cb = container_of(nb, struct mip_callbacks, nb);
	struct mip_ts_info *info = container_of(cb, struct mip_ts_info, cb);

	if (noti_type == MUIC_USB_ATTACH_NOTI || noti_type == MUIC_CHG_ATTACH_NOTI) {
		info->charger_state = true;
	} else if (noti_type == MUIC_USB_DETACH_NOTI || noti_type == MUIC_CHG_DETACH_NOTI) {
		info->charger_state = false;
	} else {
		dev_err(&info->client->dev, " %s noti_type failed\n", __func__);
		return 0;
	}

	if (info->enabled) {
		cb->inform_charger(cb, info->charger_state);
	} else {
		dev_err(&info->client->dev, "not enabled error\n", __func__);
		return 0;
	}

	return 1;
}

void mip_config_callback(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	info->cb.nb.notifier_call = mip_callback_notifier;
	register_muic_notifier(&info->cb.nb);
	info->cb.inform_charger = mip_callback_charger;

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}
/**
* Config callback functions
*/



#endif

