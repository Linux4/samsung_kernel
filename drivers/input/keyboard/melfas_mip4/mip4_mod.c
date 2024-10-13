/*
 * MELFAS MIP4 Touchkey
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
int mip4_tk_regulator_control(struct mip4_tk_info *info, int enable)
{
	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

#if MIP_USE_DEVICETREE
	int on = enable;
	int ret = 0;
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	dev_dbg(&info->client->dev, "%s - switch : %d\n", __func__, on);
	
	if (info->power == on) {
		dev_dbg(&info->client->dev, "%s - skip\n", __func__);
		goto EXIT;
	}

	//Get regulator
	if (!info->regulator_vd33) {
		info->regulator_vd33 = regulator_get(&info->client->dev, info->vcc_en_regulator_name);
		if (IS_ERR(info->regulator_vd33)) {
			dev_err(&info->client->dev, "%s [ERROR] regulator_get : vcc_en\n", __func__);
			ret = PTR_ERR(info->regulator_vd33);
			goto ERROR;
		}
		
		if(regulator_set_voltage(info->regulator_vd33, info->vcc_en_regulator_volt, info->vcc_en_regulator_volt))
		{
			dev_err(&info->client->dev, "%s [ERROR] regulator_set_voltage : vcc_en\n", __func__);
			goto ERROR;
		}
	}
#if MIP_USE_LED
	if (!info->regulator_led) {
		info->regulator_led = regulator_get(&info->client->dev, "vdd_led");
		if (IS_ERR(info->regulator_led)) {
			dev_err(&info->client->dev, "%s [ERROR] regulator_get : vdd_led\n", __func__);
			ret = PTR_ERR(info->regulator_led);
			goto ERROR;
		}
	}
#endif //MIP_USE_LED
	
	//Control regulator
	if (on) {
		ret = regulator_enable(info->regulator_vd33);
		if (ret) {
			dev_err(&info->client->dev, "%s [ERROR] regulator_enable : vd33\n", __func__);
			goto ERROR;
		}
#if MIP_USE_LED		
		ret = regulator_enable(info->regulator_led);
		if (ret) {
			dev_err(&info->client->dev, "%s [ERROR] regulator_enable : led\n", __func__);
			goto ERROR;
		}
#endif	//MIP_USE_LED	
		/*
		ret = pinctrl_select_state(info->pinctrl, info->pins_enable);
		if(ret < 0){
			dev_err(&info->client->dev, "%s [ERROR] pinctrl_select_state : pins_enable\n", __func__);
		}
		*/
	} else {
		if (regulator_is_enabled(info->regulator_vd33)) {
			regulator_disable(info->regulator_vd33);
		}
#if MIP_USE_LED
		if (regulator_is_enabled(info->regulator_led)) {
			regulator_disable(info->regulator_led);
		}
#endif	//MIP_USE_LED	
		/*
		ret = pinctrl_select_state(info->pinctrl, info->pins_disable);
		if(ret < 0){
			dev_err(&info->client->dev, "%s [ERROR] pinctrl_select_state : pins_disable\n", __func__);
		}
		*/
	}
	
	info->power = on;
	
	goto EXIT;
	
	//
	//////////////////////////

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return ret;

EXIT:
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
#endif //MIP_USE_DEVICETREE

	return 0;
}

/**
* Turn off power supply
*/
int mip4_tk_power_off(struct mip4_tk_info *info)
{
	//int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
		
	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

#if MIP_USE_DEVICETREE
	//Control CE pin (Optional)
	gpio_direction_output(info->pdata->gpio_ce, 0);	

	//Control VD33 (regulator)
	//mip4_tk_regulator_control(info, 0);
		
#else 
	//Control CE pin (Optional)
	//gpio_direction_output(info->pdata->gpio_ce, 0);	

	//Control VD33 (power switch)
	//gpio_direction_output(info->pdata->gpio_vdd_en, 0);
#endif //MIP_USE_DEVICETREE

	//
	//////////////////////////

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
int mip4_tk_power_on(struct mip4_tk_info *info)
{
	//int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//
	
#if MIP_USE_DEVICETREE
	//Control VD33 (power regulator)
	//mip4_tk_regulator_control(info, 1);

	//Control CE pin (Optional)
	gpio_direction_output(info->pdata->gpio_ce, 1);	
#else
	//Control VD33 (power)
	//gpio_direction_output(info->pdata->gpio_vdd_en, 1);	

	//Control CE pin (Optional)
	//gpio_direction_output(info->pdata->gpio_ce, 1);	
#endif

	//
	//////////////////////////
	
	msleep(200);
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

//ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* Clear key input event status
*/
void mip4_tk_clear_input(struct mip4_tk_info *info)
{
	int i;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Key
	for(i = 0; i < info->key_num; i++){
		input_report_key(info->input_dev, info->key_code[i], 0);
	}
	
	input_sync(info->input_dev);
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
		
	return;
}

/**
* Input event handler - Report input event
*/
void mip4_tk_input_event_handler(struct mip4_tk_info *info, u8 sz, u8 *buf)
{
	int i;
	int id;
	int state = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	//print_hex_dump(KERN_ERR, MIP_DEVICE_NAME " Event Packet : ", DUMP_PREFIX_OFFSET, 16, 1, buf, sz, false);
	
	for (i = 0; i < sz; i += info->event_size) {
		u8 *packet = &buf[i];

		//Event format & type
		if (info->event_format != 4) {
			dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
			goto ERROR;
		}
		
		//Report key event
		id = packet[0] & 0x0F;
		state = (packet[0] & 0x80) >> 7;

		if ((id >= 1) && (id <= info->key_num)) {
			int keycode = info->key_code[id - 1];
			
			input_report_key(info->input_dev, keycode, state);
			
			dev_dbg(&info->client->dev, "%s - Key : ID[%d] Code[%d] Event[%d]\n", __func__, id, keycode, state);
		} else {
			dev_err(&info->client->dev, "%s [ERROR] Unknown Key ID [%d]\n", __func__, id);
			continue;
		}
	}

	input_sync(info->input_dev);
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return;
}

#if MIP_USE_DEVICETREE
/**
* Parse device tree
*/
int mip4_tk_parse_devicetree(struct device *dev, struct mip4_tk_info *info)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct mip4_tk_info *info = i2c_get_clientdata(client);
	struct device_node *np = dev->of_node;
	struct property *prop;
	int ret;
	u32 val;
	const char *reg = NULL;
	
	dev_dbg(dev, "%s [START]\n", __func__);
	
	/////////////////////////////////
	// PLEASE MODIFY HERE !!!
	//
	
	//Get VCC_EN GPIO 
	ret = of_get_named_gpio(np, MIP_DEVICE_NAME",vcc_en-gpio", 0);
	if (gpio_is_valid(ret)) {
		info->pdata->gpio_ce = ret;
		
		//Request GPIO
		ret = gpio_request(info->pdata->gpio_ce, "mip4_tk_en");
		if (ret < 0) {
			dev_err(dev, "%s [ERROR] gpio_request : vcc_en-gpio\n", __func__);
			goto ERROR;
		}	
		//gpio_direction_output(info->pdata->gpio_ce, 0);
	}
	else{
		dev_err(dev, "%s [ERROR] of_get_named_gpio : vcc_en-gpio, Looking for Regulator Source\n", __func__);
#if 0		
		/* regulator enable from dts */
		if(of_property_read_string(np, "vcc_en-supply", &info->vcc_en_regulator_name)) {
			dev_err(dev, "%s: Failed to get main-regulator from dts.\n", __func__);
			goto ERROR;
		}
		else
		{
			if(of_property_read_u32(np, "vcc_en-reg_volt", &info->vcc_en_regulator_volt)){
				dev_err(dev, "%s: vcc_en-reg_volt is empty from dts.\n", __func__);
				goto ERROR;
			}
		}
#endif
	}

	//Get number of keys
	ret = of_property_read_u32(np, MIP_DEVICE_NAME",keynum", &val);
	if (ret) {
		dev_err(dev, "%s [ERROR] of_property_read_u32 : keynum\n", __func__);

		//From keycode array
		prop = of_find_property(np, MIP_DEVICE_NAME",keycode", NULL);
		if (!prop) {
			dev_err(dev, "%s [ERROR] of_find_property : keycode\n", __func__);
		}
		else {
			if (!prop->value) {
				dev_err(dev, "%s [ERROR] of_find_property : keycode\n", __func__);
			}
			else {
				info->key_num = prop->length / sizeof(u32);
				
				dev_dbg(dev, "%s - key_num [%d]\n", __func__, info->key_num);
			}
		}		
	} 
	else {
		info->key_num = val;
		
		dev_dbg(dev, "%s - key_num [%d]\n", __func__, info->key_num);
	}
	
	//Get key code
	ret = of_property_read_u32_array(np, MIP_DEVICE_NAME",keycode", info->key_code, info->key_num);
	if (ret) {
		dev_err(dev, "%s [ERROR] of_property_read_u32_array : keycode\n", __func__);
	}
	else {
		info->key_code_loaded = true;
	}
	
	//Get IRQ GPIO 
	ret = of_get_named_gpio(np, MIP_DEVICE_NAME",irq-gpio", 0);
	if (!gpio_is_valid(ret)) {
		dev_err(dev, "%s [ERROR] of_get_named_gpio : irq-gpio\n", __func__);
		goto ERROR;
	}
	else{
		info->pdata->gpio_intr = ret;
	}

	//Config GPIO
	ret = gpio_request(info->pdata->gpio_intr, "mip4_tk_irq");
	if (ret < 0) {
		dev_err(dev, "%s [ERROR] gpio_request : irq-gpio\n", __func__);
		goto ERROR;
	}	
	gpio_direction_input(info->pdata->gpio_intr);	
	
	//Set IRQ
	info->client->irq = gpio_to_irq(info->pdata->gpio_intr); 
	info->irq = info->client->irq;
	dev_dbg(dev, "%s - gpio_to_irq : irq[%d]\n", __func__, info->irq);
	
	/*
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
	
	//
	/////////////////////////////////
	
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
void mip4_tk_config_input(struct mip4_tk_info *info)
{
	struct input_dev *input_dev = info->input_dev;
	int i = 0;
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	

	/////////////////////////////
	// PLEASE MODIFY HERE !!!
	//
	
	input_dev->keycode = info->key_code;
	input_dev->keycodesize = sizeof(info->key_code[0]);
	
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	
#ifdef MIP_USE_LED
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
#endif	//MIP_USE_LED	
	
	if(info->key_code_loaded == false){
		info->key_code[0] = KEY_RECENT;
		info->key_code[1] = KEY_BACK;
		info->key_num = 2;
	}
	
	for(i = 0; i < 2 /* info->key_num */; i++){
		input_set_capability(input_dev, EV_KEY, info->key_code[i]);
		
		dev_dbg(&info->client->dev, "%s - set_bit[%d]\n", __func__, info->key_code[i]);
	}
	
	//
	/////////////////////////////

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return;
}

#if MIP_USE_CALLBACK
/**
* Callback - get charger status
*/
void mip4_tk_callback_charger(struct mip4_tk_callbacks *cb, int charger_status)
{
	struct mip4_tk_info *info = container_of(cb, struct mip4_tk_info, callbacks);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_info(&info->client->dev, "%s - charger_status[%d]\n", __func__, charger_status);
	
	//...
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/**
* Config callback functions
*/
void mip4_tk_config_callback(struct mip4_tk_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	

	info->register_callback = info->pdata->register_callback;

	//callback functions
	info->callbacks.inform_charger = mip4_tk_callback_charger;
	//info->callbacks.inform_display = mip4_tk_callback_display;
	//...
	
	if (info->register_callback){
		info->register_callback(&info->callbacks);
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return;
}
#endif //MIP_USE_CALLBACK
