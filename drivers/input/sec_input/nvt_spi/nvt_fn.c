#include "nvt_dev.h"
#include "nvt_reg.h"

int nvt_clear_fw_status(struct nvt_ts_data *ts)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		ts->nvt_ts_write(ts, buf, 2, NULL, 0);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		ts->nvt_ts_read(ts, &buf[0], 1, &buf[1], 1);

		if (buf[1] == 0x00)
			break;

		sec_delay(10);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev, "failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

int nvt_check_fw_status(struct nvt_ts_data *ts)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		ts->nvt_ts_read(ts, &buf[0], 1, &buf[1], 1);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		sec_delay(10);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev, "failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}


void nvt_sw_reset_idle(struct nvt_ts_data *ts)
{
	//---MCU idle cmds to SWRST_N8_ADDR---
	nvt_ts_write_addr(ts, ts->nplat_data->swrst_n8_addr, 0xAA);

	sec_delay(15);
}

static int nvt_read_pid(struct nvt_ts_data *ts)
{
	uint8_t buf[4] = {0};
	int32_t ret = 0;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	ts->nvt_ts_read(ts, &buf[0], 1, &buf[1], 2);

	ts->nvt_pid = (buf[2] << 8) + buf[1];
	ts->fw_ver_ic[1] = buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR);

	input_info(true, &ts->client->dev,"PID=%04X\n", ts->nvt_pid);

	return ret;
}


int nvt_get_fw_info(struct nvt_ts_data *ts)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	ts->nvt_ts_read(ts, &buf[0], 1, &buf[1], 16);
	ts->fw_ver = buf[1];
	ts->plat_data->x_node_num = buf[3];
	ts->plat_data->y_node_num = buf[4];
	ts->plat_data->max_x= (uint16_t)((buf[5] << 8) | buf[6]);
	ts->plat_data->max_y = (uint16_t)((buf[7] << 8) | buf[8]);

	input_info(true, &ts->client->dev, "%s : fw_ver=%d, x_num=%d, y_num=%d,"
			"abs_x_max=%d, abs_y_max=%d, fw_type=0x%02X\n",
			__func__, ts->fw_ver, ts->plat_data->x_node_num, ts->plat_data->y_node_num,
			ts->plat_data->max_x, ts->plat_data->max_y, buf[14]);

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		input_err(true, &ts->client->dev,"FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		ts->fw_ver = 0;
		ts->plat_data->x_node_num = 18;
		ts->plat_data->y_node_num = 32;
		ts->plat_data->max_x = TOUCH_DEFAULT_MAX_WIDTH;
		ts->plat_data->max_y = TOUCH_DEFAULT_MAX_HEIGHT;

		if(retry_count < 3) {
			retry_count++;
			input_err(true, &ts->client->dev,"retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			input_err(true, &ts->client->dev,"Set default fw_ver=%d, x_num=%d, y_num=%d, "
					"abs_x_max=%d, abs_y_max=%d\n",
					ts->fw_ver, ts->plat_data->x_node_num, ts->plat_data->y_node_num,
					ts->plat_data->max_x, ts->plat_data->max_y);
			ret = -1;
		}
	} else {
		ret = 0;
	}

	ts->fw_ver_ic[0] = buf[15];

	//---Get Novatek PID---
	nvt_read_pid(ts);

	//---get panel id---
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PANEL);
	buf[0] = EVENT_MAP_PANEL;
	ret = ts->nvt_ts_read(ts, &buf[0], 1, &buf[1], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,"nvt_ts_i2c_read error(%d)\n", ret);
		return ret;
	}
	ts->fw_ver_ic[2] = buf[1];

	//---get firmware version---
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);
	buf[0] = EVENT_MAP_FWINFO;
	ret = ts->nvt_ts_read(ts, &buf[0], 1, &buf[1], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,"nvt_ts_i2c_read error(%d)\n", ret);
		return ret;
	}
	ts->plat_data->img_version_of_ic[3] = ts->fw_ver_ic[3] = buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR);

	input_info(true, &ts->client->dev, "%s : fw_ver_ic = NO%02X%02X%02X%02X\n",
		__func__, ts->fw_ver_ic[0], ts->fw_ver_ic[1], ts->fw_ver_ic[2], ts->fw_ver_ic[3]);

	return ret;
}


#if POINT_DATA_CHECKSUM
int nvt_ts_point_data_checksum(struct nvt_ts_data *ts, char *buf, int length)
{
	char checksum = 0;
	int i = 0;

	// Generate checksum
	for (i = 0; i < length - 1; i++) {
		checksum += buf[i + 1];
	}
	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length]) {
		input_err(true, &ts->client->dev, "%s : checksum not match.(point_data[%d]=0x%02X, checksum=0x%02X)\n",
				__func__, length, buf[length], checksum);

		for (i = 0; i < 10; i++) {
			input_dbg(true, &ts->client->dev, "%02X %02X %02X %02X %02X %02X\n",
			buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
		}

		input_dbg(true, &ts->client->dev, "%02X %02X %02X %02X %02X\n", buf[61], buf[62], buf[63], buf[64], buf[65]);

		return -1;
	}

	return 0;
}

// Due to more checksum length than point report, duplicate another checksum function
int nvt_ts_lpwg_dump_checksum(struct nvt_ts_data *ts, char *buf, int length)
{
	char checksum = 0;
	int i = 0;

	// Generate checksum
	for (i = 0; i < length - 1; i++) {
		checksum += buf[i + 1];
	}
	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length]) {
		input_err(true, &ts->client->dev, "%s : checksum not match.(lpwg_dump[%d]=0x%02X, checksum=0x%02X)\n",
					__func__, length, buf[length], checksum);

		for (i = 0; i < 10; i++) {
			input_dbg(true, &ts->client->dev, "%s : %02X %02X %02X %02X %02X %02X\n",
					__func__, buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
		}

		input_dbg(true, &ts->client->dev, "%s : %02X %02X %02X %02X %02X\n",
					__func__, buf[61], buf[62], buf[63], buf[64], buf[65]);

		return -1;
	}

	return 0;
}
#endif /* POINT_DATA_CHECKSUM */

void nvt_ts_print_info_work(struct work_struct *work)
{
	struct nvt_ts_data *ts = container_of(work, struct nvt_ts_data,
			work_print_info.work);

	sec_input_print_info(&ts->client->dev, NULL);

	if (ts->sec.cmd_is_running)
		input_err(true, &ts->client->dev, "%s: skip set temperature, cmd running\n", __func__);

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

void nvt_read_info_work(struct work_struct *work)
{
	struct nvt_ts_data *ts = container_of(work, struct nvt_ts_data,
			work_read_info.work);
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	input_err(true, &ts->client->dev, "%s: factory bin : skip factory_cmd_result_all call\n", __func__);
#endif

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	read_tsp_info_onboot(&ts->sec);
#endif

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s done, do not run work\n", __func__);
	} else {
		schedule_work(&ts->work_print_info.work);
	}
}


int nvt_ts_check_chip_ver_trim(struct nvt_ts_data *ts)
{
	char buf[8] = {0};
	int retry = 0;
	int list = 0;
	int i = 0;
	int found_nvt_chip = 0;
	int ret = -1;

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {

		nvt_bootloader_reset(ts);

		//---set xdata index to 0x1F600---
		nvt_set_page(ts, 0x1F64E);

		buf[0] = 0x4E;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		ts->nvt_ts_read(ts, &buf[0], 1, &buf[1], 6);
		input_info(true, &ts->client->dev,"buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {
				found_nvt_chip = 1;
			}

			if (found_nvt_chip) {
				input_info(true, &ts->client->dev,"This is NVT touch IC\n");
				ts->mmap = trim_id_table[list].mmap;
				ts->carrier_system = trim_id_table[list].hwinfo->carrier_system;
				ts->hw_crc = trim_id_table[list].hwinfo->hw_crc;
//				ts->fw_ver_ic[0] = ((buf[6] & 0x0F) << 4) | ((buf[5] & 0xF0) >> 4);
				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		sec_delay(10);
	}

out:
	return ret;
}


#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
void nvt_ts_interrupt_notify(struct work_struct *work)
{
	struct sec_ts_plat_data *pdata = container_of(work, struct sec_ts_plat_data,
			interrupt_notify_work.work);
	struct sec_input_notify_data data;
	data.dual_policy = MAIN_TOUCHSCREEN;
	if (pdata->touch_count > 0)
		sec_input_notify(NULL, NOTIFIER_LCD_VRR_LFD_LOCK_REQUEST, &data);
	else
		sec_input_notify(NULL, NOTIFIER_LCD_VRR_LFD_LOCK_RELEASE, &data);
}
#endif

#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
int nvt_notifier_call(struct notifier_block *nb, unsigned long data, void *v)
{
	struct nvt_ts_data *ts = container_of(nb, struct nvt_ts_data, nb);
	struct panel_state_data *d = (struct panel_state_data *)v;

	if (data == PANEL_EVENT_ESD) {
		input_info(true, &ts->client->dev, "%s: LCD ESD INTERRUPT CALLED\n", __func__);
		ts->lcd_esd_recovery = 1;
	} else if (data == PANEL_EVENT_STATE_CHANGED) {
		if (d->state == PANEL_ON) {
			if (ts->lcd_esd_recovery == 1) {
				input_info(true, &ts->client->dev, "%s: LCD ESD LCD ON POST, run fw update\n", __func__);

				nvt_update_firmware(ts, ts->plat_data->firmware_name);
				nvt_check_fw_reset_state(ts, RESET_STATE_REK);

				nvt_ts_mode_restore(ts);
				ts->lcd_esd_recovery = 0;
			}
		}
	}

	return 0;
}
#elif defined(EXYNOS_DISPLAY_INPUT_NOTIFIER)
static int nvt_notifier_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct nvt_ts_data *ts = container_of(nb, struct nvt_ts_data, nb);
	if (data == PANEL_EVENT_UB_CON_CHANGED) {
		int i = *((char *)v);

		input_info(true, &ts->client->dev, "%s: data = %ld, i = %d\n", __func__, data, i);

		if (i == PANEL_EVENT_UB_CON_DISCONNECTED) {
			input_info(true, &ts->client->dev, "%s: UB_CON_DISCONNECTED : disable irq & pin control\n", __func__);
			disable_irq(ts->client->irq);
			ts->plat_data->pinctrl_configure(&ts->client->dev, false);
		}
	}
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
int nvt_ts_vbus_notification(struct notifier_block *nb, unsigned long cmd, void *data)
{
	struct nvt_ts_data *ts = container_of(nb, struct nvt_ts_data, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *)data;
	int ret;
	int mode_cmd;

	if (ts->plat_data->shutdown_called)
		return 0;

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		input_info(true, &ts->client->dev, "%s : attach\n", __func__);
		ts->plat_data->touch_functions |= CHARGER_MASK;
		mode_cmd = CHARGER_PLUG_AC;
		break;
	case STATUS_VBUS_LOW:
		input_info(true, &ts->client->dev, "%s : detach\n", __func__);
		ts->plat_data->touch_functions &= ~CHARGER_MASK;
		mode_cmd = CHARGER_PLUG_OFF;
		break;
	default:
		break;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
				__func__);
		return 0;
	}

	ret = nvt_ts_mode_switch(ts, mode_cmd, false);
	if (ret)
		input_err(true, &ts->client->dev, "failed to switch %s mode\n",
					(mode_cmd == CHARGER_PLUG_AC) ? "CHARGER_PLUG_AC" : "CHARGER_PLUG_OFF");

	mutex_unlock(&ts->lock);
	return 0;
}
#endif

int nvt_ts_mode_restore(struct nvt_ts_data *ts)
{
	u8 cmd;
	int i;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: touch_functions:0x%X\n",
				__func__, ts->plat_data->touch_functions);

	for (i = GLOVE; i < FUNCT_MAX; i++) {
		if ((ts->plat_data->touch_functions >> i) & 0x01) {
			switch (i) {
			case GLOVE:
				if (ts->plat_data->touch_functions & GLOVE_MASK)
					cmd = GLOVE_ENTER;
				else
					cmd = GLOVE_LEAVE;
				break;
			case CHARGER:
				if (ts->plat_data->touch_functions & CHARGER_MASK)
					cmd = CHARGER_PLUG_AC;
				else
					cmd = CHARGER_PLUG_OFF;
				break;
			case SPAY_SWIPE:
				if (ts->plat_data->touch_functions & SPAY_SWIPE_MASK)
					cmd = SPAY_SWIPE_ENTER;
				else
					cmd = SPAY_SWIPE_LEAVE;
				break;
			case DOUBLE_CLICK:
				if (ts->plat_data->touch_functions & DOUBLE_CLICK_MASK)
					cmd = DOUBLE_CLICK_ENTER;
				else
					cmd = DOUBLE_CLICK_LEAVE;
				break;
			case HIGH_SENSITIVITY:
				if (ts->plat_data->touch_functions & HIGH_SENSITIVITY_MASK)
					cmd = HIGH_SENSITIVITY_ENTER;
				else
					cmd = HIGH_SENSITIVITY_LEAVE;
				break;
			case SENSITIVITY:
				if (ts->plat_data->touch_functions & SENSITIVITY_MASK)
					cmd = SENSITIVITY_ENTER;
				else
					cmd = SENSITIVITY_LEAVE;
				break;
			default:
				continue;
			}

			ret = nvt_ts_mode_switch(ts, cmd, false);
			if (ret)
				input_info(true, &ts->client->dev, "%s: failed to restore %X\n", __func__, cmd);
		}
	}

	return ret;
}

void nvt_ts_release_all_finger(struct nvt_ts_data *ts)
{
	int i = 0;
	sec_input_release_all_finger(&ts->client->dev);
	for (i = 0; i < TOUCH_MAX_FINGER_NUM; i++)
		ts->press[i] = false;
}



MODULE_LICENSE("GPL");
