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
	int ret;
	static struct regulator *tsp_vdd;
	struct i2c_client *client = info->client;
	tsp_vdd = regulator_get(&client->dev, "vd33");

	if (IS_ERR(tsp_vdd)) {
		dev_err(&client->dev, "regulator_get error!\n");
		return;
	}

	if (enable) {
		if (!regulator_is_enabled(tsp_vdd)) {
			regulator_set_voltage(tsp_vdd, 3100000, 3100000);
			ret = regulator_enable(tsp_vdd);
			if (ret) {
				dev_err(&client->dev, "power on error!!\n");
				return;
			}
			dev_info(&client->dev, "power on!\n");
		} else
			dev_err(&client->dev, "already power on!\n");
		mdelay(100);
	} else {
		if (regulator_is_enabled(tsp_vdd)) {
			ret = regulator_disable(tsp_vdd);
			if (ret) {
				dev_err(&client->dev, "power off error!\n");
				return;
			}
			dev_info(&client->dev, "power off!\n");
			mdelay(10);
		} else
			dev_err(&client->dev, "already power off!\n");
	}

	regulator_put(tsp_vdd);

}

/**
* Turn off power supply
*/
int mip_power_off(struct mip_ts_info *info)
{
	int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mip_regulator_control(info, 0);

	msleep(10);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

//ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Turn on power supply
*/
int mip_power_on(struct mip_ts_info *info)
{
	//int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//////////////////////////
	// MODIFY REQUIRED
	//

	//Control regulator
	mip_regulator_control(info, 1);

	//Control power switch
	//gpio_direction_output(info->pdata->gpio_vdd_en, 1);

	//
	//////////////////////////

	msleep(150);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

//ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Clear touch input status in the set
*/
void mip_clear_input(struct mip_ts_info *info)
{
	int i;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Screen
	for(i = 0; i < MAX_FINGER_NUM; i++){
		/////////////////////////////////
		// MODIFY REQUIRED
		//

		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
		//input_report_key(info->input_dev, BTN_TOUCH, 0);
		//input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

		//
		/////////////////////////////////
	}
	info->finger_cnt = 0;
#ifdef CONFIG_INPUT_BOOSTER
		input_booster_send_event(BOOSTER_DEVICE_TOUCH, BOOSTER_MODE_FORCE_OFF);
#endif

#if MIP_USE_KEY
	if(info->key_enable == true){
		for(i = 0; i < info->key_num; i++){
			input_report_key(info->input_dev, info->key_code[i], 0);
		}
	}
#endif
	input_sync(info->input_dev);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return;
}

/**
* Input event handler - Report touch input event
*/
void mip_input_event_handler(struct mip_ts_info *info, u8 sz, u8 *buf)
{
	int i;
	int id, x, y;
	int pressure = 0;
	int size = 0;
	int touch_major = 0;
	int touch_minor = 0;
	int palm = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	//dev_dbg(&info->client->dev, "%s - sz[%d] buf[0x%02X]\n", __func__, sz, buf[0]);
	//print_hex_dump(KERN_ERR, "Event Packet : ", DUMP_PREFIX_OFFSET, 16, 1, buf, sz, false);

	for (i = 0; i < sz; i += info->event_size) {
		u8 *tmp = &buf[i];

#if MIP_USE_KEY
		if ((tmp[0] & MIP_EVENT_INPUT_SCREEN) == 0) {
			//Touchkey Event
			int key = tmp[0] & 0xf;
			int key_state = (tmp[0] & MIP_EVENT_INPUT_PRESS) ? 1 : 0;
			int key_code = 0;

			//Report touchkey event
			if((key > 0) && (key <= info->key_num)){
				key_code = info->key_code[key - 1];

				input_report_key(info->input_dev, key_code, key_state);

				dev_dbg(&info->client->dev, "%s - Key : ID[%d] Code[%d] State[%d]\n", __func__, key, key_code, key_state);
			}
			else{
				dev_err(&info->client->dev, "%s [ERROR] Unknown key id [%d]\n", __func__, key);
				continue;
			}
		}
		else
#endif
		{
			if(info->event_format == 0){
				id = (tmp[0] & 0xf) - 1;
				x = tmp[2] | ((tmp[1] & 0xf) << 8);
				y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);
				pressure = tmp[4];
				touch_major = tmp[5];
				palm = (tmp[0] & MIP_EVENT_INPUT_PALM) >> 4;
			}
			else if(info->event_format == 1){
				id = (tmp[0] & 0xf) - 1;
				x = tmp[2] | ((tmp[1] & 0xf) << 8);
				y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);
				pressure = tmp[4];
				size = tmp[5];
				touch_major = tmp[6];
				touch_minor = tmp[7];
				palm = (tmp[0] & MIP_EVENT_INPUT_PALM) >> 4;
			}
			else if(info->event_format == 2){
				id = (tmp[0] & 0xf) - 1;
				x = tmp[2] | ((tmp[1] & 0xf) << 8);
				y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);
				pressure = tmp[4];
				touch_major = tmp[5];
				touch_minor = tmp[6];
				palm = (tmp[0] & MIP_EVENT_INPUT_PALM) >> 4;
			}
			else{
				dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
				goto ERROR;
			}

			//dev_info(&info->client->dev, "%s - Touch : ID[%d] X[%d] Y[%d] Z[%d]\n", __func__, id, x, y, pressure);




			if((tmp[0] & MIP_EVENT_INPUT_PRESS) == 0) {
				info->finger_cnt--;
				//Release
#ifdef CONFIG_INPUT_BOOSTER
				if (!info->finger_cnt)
					input_booster_send_event(BOOSTER_DEVICE_TOUCH,
									BOOSTER_MODE_OFF);
#endif
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
				dev_info(&info->client->dev,
					"Touch : ID[%d] Release. remain: %d\n", id, info->finger_cnt);
#else
				dev_info(&info->client->dev,
					"Touch : ID[%d] X[%d] Y[%d] Release. remain: %d\n", id, x, y, info->finger_cnt);
#endif
				dev_dbg(&info->client->dev, "%s - Touch : ID[%d] Release\n", __func__, id);
				info->touch_state[id] = false;
				input_sync(info->input_dev);

				continue;
			}
#ifdef CONFIG_INPUT_BOOSTER
			input_booster_send_event(BOOSTER_DEVICE_TOUCH, BOOSTER_MODE_ON);
#endif

			input_mt_slot(info->input_dev, id);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
			//input_report_key(info->input_dev, BTN_TOUCH, 1);
			//input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, touch_minor);

			dev_dbg(&info->client->dev, "%s - Touch : ID[%d] X[%d] Y[%d] Z[%d] Major[%d] Minor[%d] Size[%d] Palm[%d]\n", __func__, id, x, y, pressure, touch_major, touch_minor, size, palm);

			if (!info->touch_state[id]) {
				info->finger_cnt++;
				info->touch_state[id] = true;
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
				dev_info(&info->client->dev,
					"Touch : ID[%d] Press. remain: %d\n", id, info->finger_cnt);
#else
				dev_info(&info->client->dev,
					"Touch : ID[%d] X[%d] Y[%d] Press. remain: %d\n", id, x, y, info->finger_cnt);
#endif
			}
		}
		input_sync(info->input_dev);
	}
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return;
}

/**
* Wake-up event handler
*/
int mip_wakeup_event_handler(struct mip_ts_info *info, u8 *rbuf)
{
	u8 wbuf[4];
	u8 gesture_code = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	/////////////////////////////////
	// MODIFY REQUIRED
	//

	//Report wake-up event

	dev_dbg(&info->client->dev, "%s - gesture[%d]\n", __func__, gesture_code);

	info->wakeup_gesture_code = gesture_code;

	switch(gesture_code){
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
			//Example : emulate power key
			input_report_key(info->input_dev, KEY_POWER, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_POWER, 0);
			input_sync(info->input_dev);
			break;

		default:
			//Re-enter nap mode
			wbuf[0] = MIP_R0_CTRL;
			wbuf[1] = MIP_R1_CTRL_POWER_STATE;
			wbuf[2] = MIP_CTRL_POWER_LOW;
			if(mip_i2c_write(info, wbuf, 3)){
				dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
				goto ERROR;
			}

			break;
	}

	//
	//
	/////////////////////////////////

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
	struct i2c_client *client = to_i2c_client(dev);
	struct device_node *np = dev->of_node;
	int ret;
	u32 val;

	dev_dbg(dev, "%s [START]\n", __func__);

	//Read property

	ret = of_property_read_u32(np, MIP_DEVICE_NAME",max_x", &val);
	if (ret) {
		dev_err(dev, "%s [ERROR] max_x\n", __func__);
		info->pdata->max_x = 1080;
	}
	else {
		info->pdata->max_x = val;
	}

	ret = of_property_read_u32(np, MIP_DEVICE_NAME",max_y", &val);
	if (ret) {
		dev_err(dev, "%s [ERROR] max_y\n", __func__);
		info->pdata->max_y = 1920;
	}
	else {
		info->pdata->max_y = val;
	}


	//Get GPIO
	ret = of_get_named_gpio(np, MIP_DEVICE_NAME",gpio_irq", 0);
	if (!gpio_is_valid(ret)) {
		dev_err(dev, "%s [ERROR] of_get_named_gpio : gpio_irq\n", __func__);
		goto ERROR;
	}
	else{
		info->pdata->gpio_intr = ret;
	}

	//Config GPIO
	/*ret = gpio_request(info->pdata->gpio_intr, "gpio_irq");
	if (ret < 0) {
		dev_err(dev, "%s [ERROR] gpio_request : gpio_irq\n", __func__);
		goto ERROR;
	}
	gpio_direction_input(info->pdata->gpio_intr);
	/*
	/*
	ret = gpio_request(info->pdata->gpio_reset, "reset-gpio");
	if (ret < 0) {
		dev_err(dev, "%s [ERROR] gpio_request : reset-gpio\n", __func__);
		goto ERROR;
	}
	gpio_direction_output(info->pdata->gpio_reset, 1);
	*/
	/*
	//Set IRQ
	info->client->irq = gpio_to_irq(info->pdata->gpio_intr);
	//dev_dbg(dev, "%s - gpio_to_irq : irq[%d]\n", __func__, info->client->irq);

	//Get Pinctrl
	info->pinctrl = devm_pinctrl_get(&info->client->dev);
	if (IS_ERR(info->pinctrl)){
		dev_err(dev, "%s [ERROR] devm_pinctrl_get\n", __func__);
		goto ERROR;
	}
	info->pins_enable = pinctrl_lookup_state(info->pinctrl, "enable");
	if (IS_ERR(info->pins_enable)){
		dev_err(dev, "%s [ERROR] pinctrl_lookup_state enable\n", __func__);
	}
	info->pins_disable = pinctrl_lookup_state(info->pinctrl, "disable");
	if (IS_ERR(info->pins_disable)){
		dev_err(dev, "%s [ERROR] pinctrl_lookup_state disable\n", __func__);
	}
	*/

	dev_dbg(dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(dev, "%s [ERROR]\n", __func__);
	return 1;
}
#endif

/**
* Config input interface
*/
void mip_config_input(struct mip_ts_info *info)
{
	struct input_dev *input_dev = info->input_dev;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_SYN, input_dev->evbit);
	//set_bit(EV_KEY, input_dev->evbit);

	//Screen
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	//set_bit(BTN_TOUCH, input_dev->evbit);
	//set_bit(BTN_TOOL_FINGER, input_dev->evbit);

	//input_mt_init_slots(input_dev, MAX_FINGER_NUM);
	input_mt_init_slots(input_dev, MAX_FINGER_NUM, 0);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, info->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, INPUT_PRESSURE_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, INPUT_TOUCH_MAJOR_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, INPUT_TOUCH_MINOR_MAX, 0, 0);
	//input_set_abs_params(input_dev, ABS_MT_SUMSIZE, 0, 255, 0, 0);
	//input_set_abs_params(input_dev, ABS_MT_PALM, 0, 1, 0, 0);

#if MIP_USE_KEY
	set_bit(KEY_BACK, input_dev->keybit);
	set_bit(KEY_MENU, input_dev->keybit);

	info->key_code[0] = KEY_BACK;
	info->key_code[1] = KEY_MENU;
#endif

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
	struct mip_ts_info *info = container_of(cb, struct mip_ts_info, callbacks);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_info(&info->client->dev, "%s - charger_status[%d]\n", __func__, charger_status);

	//...

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/**
* Config callback functions
*/
void mip_config_callback(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	info->register_callback = info->pdata->register_callback;

	//callback functions
	info->callbacks.inform_charger = mip_callback_charger;
	//info->callbacks.inform_display = mip_callback_display;
	//...

	if (info->register_callback){
		info->register_callback(&info->callbacks);
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;
}
#endif

