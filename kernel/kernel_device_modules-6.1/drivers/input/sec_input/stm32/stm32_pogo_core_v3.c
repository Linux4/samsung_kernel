#include "stm32_pogo_v3.h"

bool stm32_chk_booster_models(struct stm32_dev *stm32)
{
	if (!stm32->model_info)
		return false;

	return stm32->model_info->need_dcdc_boosting;
}

static void stm32_init_workqueue(struct stm32_dev *stm32)
{
	INIT_DELAYED_WORK(&stm32->print_info_work, stm32_print_info_work);
}

static void stm32_slave_device_init(struct stm32_dev *stm32)
{
	stm32_pogo_kpd_init();
	stm32_pogo_touchpad_init();
#if IS_ENABLED(CONFIG_HALL_LOGICAL)
	hall_logical_init();
#endif
	if (stm32->dtdata->booster_power_voltage)
		kbd_max77816_init();
}

static void stm32_init_completion(struct stm32_dev *stm32)
{
#if IS_ENABLED(CONFIG_PM)
	init_completion(&stm32->resume_done);
	complete_all(&stm32->resume_done);
#endif
	init_completion(&stm32->i2c_done);
	complete_all(&stm32->i2c_done);
}

static int stm32_init_pinctrl(struct stm32_dev *stm32)
{
	int ret = 0;

	stm32->pinctrl = devm_pinctrl_get(&stm32->client->dev);
	if (IS_ERR(stm32->pinctrl)) {
		if (PTR_ERR(stm32->pinctrl) == -EPROBE_DEFER) {
			ret = PTR_ERR(stm32->pinctrl);
			return SEC_ERROR;
		}

		input_err(true, &stm32->client->dev, "%s: Target does not use pinctrl\n", __func__);
		stm32->pinctrl = NULL;
	}

	return SEC_SUCCESS;
}

static void stm32_destroy_mutex(struct stm32_dev *stm32)
{
	mutex_destroy(&stm32->dev_lock);
	mutex_destroy(&stm32->irq_lock);
	mutex_destroy(&stm32->i2c_lock);
	mutex_destroy(&stm32->conn_lock);
	mutex_destroy(&stm32->fw_lock);
}

static void stm32_init_mutex(struct stm32_dev *stm32)
{
	mutex_init(&stm32->dev_lock);
	mutex_init(&stm32->irq_lock);
	mutex_init(&stm32->i2c_lock);
	mutex_init(&stm32->conn_lock);
	mutex_init(&stm32->fw_lock);
}

static int stm32_wakeup_source_register(struct stm32_dev *stm32)
{
	stm32->stm32_ws = wakeup_source_register(NULL, "stm_key wake lock");
	if (IS_ERR_OR_NULL(stm32->stm32_ws))
		return SEC_ERROR;

	return SEC_SUCCESS;
}

static int stm32_parse_dt(struct device *dev, struct stm32_dev *stm32)
{
	struct device_node *np = dev->of_node;
	int ret;

	if (!dev->of_node) {
		input_err(true, dev, "exist not of_node\n");
		return SEC_ERROR;
	}

	stm32->dtdata->gpio_int = of_get_named_gpio(np, "stm32,irq_gpio", 0);
	if (!gpio_is_valid(stm32->dtdata->gpio_int)) {
		input_err(true, dev, "unable to get gpio_int\n");
		return SEC_ERROR;
	}

	stm32->dtdata->gpio_sda = of_get_named_gpio(np, "stm32,sda_gpio", 0);
	if (!gpio_is_valid(stm32->dtdata->gpio_sda)) {
		input_err(true, dev, "unable to get gpio_sda\n");
		return SEC_ERROR;
	}

	stm32->dtdata->gpio_scl = of_get_named_gpio(np, "stm32,scl_gpio", 0);
	if (!gpio_is_valid(stm32->dtdata->gpio_scl)) {
		input_err(true, dev, "unable to get gpio_scl\n");
		return SEC_ERROR;
	}

	stm32->dtdata->gpio_conn = of_get_named_gpio(np, "stm32,irq_conn", 0);
	if (!gpio_is_valid(stm32->dtdata->gpio_conn)) {
		input_err(true, dev, "unable to get irq_conn\n");
		return SEC_ERROR;
	}

	stm32->dtdata->mcu_swclk = of_get_named_gpio(np, "stm32,mcu_swclk", 0);
	if (!gpio_is_valid(stm32->dtdata->mcu_swclk)) {
		input_err(true, dev, "unable to get mcu_swclk\n");
		return SEC_ERROR;
	}

	stm32->dtdata->mcu_nrst = of_get_named_gpio(np, "stm32,mcu_nrst", 0);
	if (!gpio_is_valid(stm32->dtdata->mcu_nrst)) {
		input_err(true, dev, "unable to get mcu_nrst\n");
		return SEC_ERROR;
	}

	if (of_property_read_u32(np, "stm32,irq_type", &stm32->dtdata->irq_type)) {
		input_err(true, dev, "%s: Failed to get irq_type property\n", __func__);
		stm32->dtdata->irq_type = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
	} else {
		input_info(true, dev, "%s: irq_type property:%X, %d\n",
				__func__, stm32->dtdata->irq_type, stm32->dtdata->irq_type);
	}

	if (of_property_read_u32(np, "stm32,irq_conn_type", &stm32->dtdata->irq_conn_type)) {
		input_err(true, dev, "%s: Failed to get irq_conn_type property\n", __func__);
		stm32->dtdata->irq_conn_type = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT;
	} else {
		input_info(true, dev, "%s: irq_conn_type property:%X, %d\n",
				__func__, stm32->dtdata->irq_conn_type, stm32->dtdata->irq_conn_type);
	}

	if (of_property_read_bool(np, "stm32_vddo-supply")) {
		stm32->dtdata->vdd_vreg = devm_regulator_get(&stm32->client->dev, "stm32_vddo");
		if (IS_ERR(stm32->dtdata->vdd_vreg)) {
			ret = PTR_ERR(stm32->dtdata->vdd_vreg);
			input_err(true, &stm32->client->dev, "%s: could not get vddo, rc = %d\n", __func__, ret);
			stm32->dtdata->vdd_vreg = NULL;
		}
	} else {
		input_err(true, &stm32->client->dev, "%s: not use regulator\n", __func__);
	}

	ret = of_property_read_u32(np, "stm32,model_type", &stm32->model_type);
	if (ret < 0) {
		input_err(true, dev, "%s: model type is not defined\n", __func__);
		return SEC_ERROR;
	}

	input_info(true, dev, "%s: model type %d\n", __func__, stm32->model_type);
	if (stm32->model_type >= MODEL_TYPE_MAX) {
		input_err(true, dev, "%s: model type is invalid\n", __func__);
		return SEC_ERROR;
	}

	ret = of_property_read_string(np, "stm32,fota_fw_path", &stm32->dtdata->fota_fw_path);
	if (ret < 0) {
		input_err(true, dev, "unable to get fota_fw_path %d\n", ret);
		return SEC_ERROR;
	}

	ret = of_property_read_string(np, "stm32,fw_name", &stm32->dtdata->mcu_fw_name);
	if (ret) {
		input_err(true, dev, "unable to get mcu_fw_name\n");
		return SEC_ERROR;
	}

	if (of_property_read_u32(np, "stm32,i2c-burstmax", &stm32->dtdata->i2c_burstmax)) {
		input_err(false, dev, "%s: Failed to get i2c_burstmax property\n", __func__);
		stm32->dtdata->i2c_burstmax = STM32_MAX_EVENT_SIZE;
	}

	stm32->dtdata->support_open_close = of_property_read_bool(np, "support_open_close");

	ret = of_property_read_u32(np, "stm32,booster_power_voltage",
					&stm32->dtdata->booster_power_voltage);
	if (ret < 0)
		input_err(true, dev, "%s: Failed to get booster_power_voltage property\n", __func__);

	input_info(true, dev, "%s: scl: %d, sda: %d, int:%d, conn: %d, mcu_swclk: %d, mcu_nrst: %d\n",
			__func__, stm32->dtdata->gpio_scl, stm32->dtdata->gpio_sda,
			stm32->dtdata->gpio_int, stm32->dtdata->gpio_conn,
			stm32->dtdata->mcu_swclk, stm32->dtdata->mcu_nrst);
	input_info(true, dev, "%s: mcu_fw_name:%s i2c-burstmax:%d\n",
			__func__, stm32->dtdata->mcu_fw_name, stm32->dtdata->i2c_burstmax);
	input_info(true, dev, "%s: open_close:%d, booster_power_voltage:%d\n",
			__func__, stm32->dtdata->support_open_close,
			stm32->dtdata->booster_power_voltage);

	return SEC_SUCCESS;
}

static int stm32_i2c_new_dummy(struct stm32_dev *stm32, u16 address)
{
	input_info(true, &stm32->client->dev, "%s: client_boot address:0x%x\n", __func__, address);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	stm32->client_boot = i2c_new_dummy_device(stm32->client->adapter, address);
#else
	stm32->client_boot = i2c_new_dummy(stm32->client->adapter, address);
#endif
	if (IS_ERR(stm32->client_boot)) {
		input_err(true, &stm32->client->dev, "%s: client_boot err:%ld\n",
				__func__, PTR_ERR(stm32->client_boot));
		return SEC_ERROR;
	}

	i2c_set_clientdata(stm32->client_boot, stm32);

	return SEC_SUCCESS;
}

int stm32_pogo_v3_start(struct stm32_dev *stm32)
{
	struct device *dev;
	int ret = 0;
	u16 boot_addr = 0x51;

	dev = &stm32->client->dev;

	ret = stm32_i2c_new_dummy(stm32, boot_addr);
	if (IS_SEC_ERROR(ret)) {
		input_err(true, dev, "Failed to register sub client[0x%x]\n", boot_addr);
		return -ENOMEM;
	}

	ret = stm32_parse_dt(dev, stm32);
	if (IS_SEC_ERROR(ret)) {
		input_err(true, dev, "Failed to use device tree\n");
		goto err_new_i2c_dummy;
	}

	ret = stm32_init_cmd(stm32);
	if (IS_SEC_ERROR(ret)) {
		input_err(true, dev, "Failed to interrupt init\n");
		goto err_create_device;
	}

	ret = stm32_wakeup_source_register(stm32);
	if (IS_SEC_ERROR(ret)) {
		input_err(true, dev, "Failed to wakeup source register\n");
		goto err_wakeup_source_register;
	}

	stm32_init_mutex(stm32);

	ret = stm32_init_pinctrl(stm32);
	if (IS_SEC_ERROR(ret)) {
		input_err(true, dev, "Failed to init_pinctrl source register\n");
		goto err_pinctrl;
	}

	atomic_set(&stm32->check_ic_flag, false);

	ret = stm32_init_voting(stm32);
	if (IS_SEC_ERROR(ret))
		input_err(true, dev, "Failed to stm32_init_voting\n");

	stm32_init_completion(stm32);
	stm32_init_workqueue(stm32);

	ret = stm32_dev_firmware_update_menu(stm32, 0);
	if (ret < 0)
		input_err(true, dev, "Failed to update stm32_mcu_dev firmware %d\n", ret);

	stm32->pogo_enable = true;

	stm32_slave_device_init(stm32);

	ret = stm32_interrupt_init(stm32);
	if (IS_SEC_ERROR(ret)) {
		input_err(true, dev, "Failed to interrupt init\n");
		goto interrupt_err;
	}

	stm32->connect_state = gpio_get_value(stm32->dtdata->gpio_conn);
	if (stm32->connect_state)
		atomic_set(&stm32->check_ic_flag, true);
	else
		atomic_set(&stm32->check_ic_flag, false);
	stm32_keyboard_connect(stm32);

	stm32_register_notify(stm32);

	input_info(true, dev, "%s done\n", __func__);

	return ret;

interrupt_err:
	stm32_destroy_voting(stm32);
err_pinctrl:
	stm32_destroy_mutex(stm32);
	wakeup_source_unregister(stm32->stm32_ws);
err_wakeup_source_register:
err_new_i2c_dummy:
err_create_device:
	i2c_unregister_device(stm32->client_boot);
	input_err(true, dev, "Error at %s\n", __func__);
	return ret;
}

MODULE_LICENSE("GPL");
