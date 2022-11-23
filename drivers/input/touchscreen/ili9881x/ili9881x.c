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
#include "ili9881x_fw.h"
#include "ili9881x.h"

/* Debug level */
bool debug_en = DEBUG_OUTPUT;
EXPORT_SYMBOL(debug_en);
bool ili_shutdown_is_on_going_tsp = false;
static struct workqueue_struct *esd_wq;
static struct workqueue_struct *bat_wq;
static struct delayed_work esd_work;
static struct delayed_work bat_work;

#if RESUME_BY_DDI
static struct workqueue_struct	*resume_by_ddi_wq;
static struct work_struct	resume_by_ddi_work;

static void ilitek_resume_by_ddi_work(struct work_struct *work)
{
	mutex_lock(&ilits->touch_mutex);

	if (ilits->gesture)
		ili_irq_wake_disable();

	/* Set tp as demo mode and reload code if it's iram. */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	if (ilits->fw_upgrade_mode == UPGRADE_IRAM)
		ili_fw_upgrade_handler(NULL);
	else
		ili_reset_ctrl(ilits->reset);

	ili_irq_enable();
	input_info(true, ilits->dev, "%s TP resume end by wq\n", __func__);
	ili_wq_ctrl(WQ_ESD, ENABLE);
	ili_wq_ctrl(WQ_BAT, ENABLE);
	ilits->tp_suspend = false;
	mutex_unlock(&ilits->touch_mutex);
}

void ili_resume_by_ddi(void)
{
	if (!resume_by_ddi_wq) {
		input_info(true, ilits->dev, "%s resume_by_ddi_wq is null\n", __func__);
		return;
	}

	mutex_lock(&ilits->touch_mutex);

	input_info(true, ilits->dev, "%s TP resume start called by ddi\n", __func__);

	/*
	 * To match the timing of sleep out, the first of mipi cmd must be sent within 10ms
	 * after TP reset. We then create a wq doing host download before resume.
	 */
	atomic_set(&ilits->fw_stat, ENABLE);
	ili_reset_ctrl(ilits->reset);
	ili_ice_mode_ctrl(ENABLE, OFF);
	ilits->ddi_rest_done = true;
	usleep_range(5 * 1000, 5 * 1000);
	queue_work(resume_by_ddi_wq, &(resume_by_ddi_work));

	mutex_unlock(&ilits->touch_mutex);
}
#endif

void ilitek_pin_control(bool pin_set)
{
	int retval = 0;

	if (IS_ERR(ilits->pinctrl))
		return;

	ilits->pinctrl->state = NULL;
	if (pin_set) {
		if (!IS_ERR(ilits->pins_on_state)) {
			retval = pinctrl_select_state(ilits->pinctrl, ilits->pins_on_state);
			if (retval)
				input_err(true, ilits->dev, "can't set pins_on_state state : %d\n", retval);
			input_info(true, ilits->dev, "%s idle\n", __func__);
		}
	} else {
		if (!IS_ERR(ilits->pins_off_state)) {
			retval = pinctrl_select_state(ilits->pinctrl, ilits->pins_off_state);
			if (retval)
				input_err(true, ilits->dev, "can't set pins_off_state state : %d\n", retval);
			input_info(true, ilits->dev, "%s sleep\n", __func__);
		}
	}
}

int ili_mp_test_handler(char *apk, bool lcm_on)
{
	int ret = 0;

	if (atomic_read(&ilits->fw_stat)) {
		input_err(true, ilits->dev, "%s fw upgrade processing, ignore\n", __func__);
		return -EMP_FW_PROC;
	}

	atomic_set(&ilits->mp_stat, ENABLE);

	if (ilits->actual_tp_mode != P5_X_FW_TEST_MODE) {
		ret = ili_switch_tp_mode(P5_X_FW_TEST_MODE);
		if (ret < 0) {
			input_err(true, ilits->dev, "%s Switch MP mode failed\n", __func__);
			ret = -EMP_MODE;
			goto out;
		}
	}

	ret = ili_mp_test_main(apk, lcm_on);

out:
	/*
	 * If there's running mp test with lcm off, we suspose that
	 * users will soon call resume from suspend. TP mode will be changed
	 * from MP to AP mode until resume finished.
	 */
	if (!lcm_on) {
		atomic_set(&ilits->mp_stat, DISABLE);
		return ret;
	}
	if (ilits->sec.cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		ilits->actual_tp_mode = P5_X_FW_AP_MODE;
		if (ilits->fw_upgrade_mode == UPGRADE_IRAM) {
			if (ili_fw_upgrade_handler(NULL) < 0)
				input_err(true, ilits->dev, "%s FW upgrade failed during mp test\n", __func__);
		} else {
			if (ili_reset_ctrl(ilits->reset) < 0)
				input_err(true, ilits->dev, "%s TP Reset failed during mp test\n", __func__);
		}

		atomic_set(&ilits->mp_stat, DISABLE);
	}

	return ret;
}

int ili_switch_tp_mode(u8 mode)
{
	int ret = 0;
	bool ges_dbg = false;

	atomic_set(&ilits->tp_sw_mode, START);

	ilits->actual_tp_mode = mode;

	/* able to see cdc data in gesture mode */
	if (ilits->tp_data_format == DATA_FORMAT_DEBUG &&
		ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE)
		ges_dbg = true;

	switch (ilits->actual_tp_mode) {
	case P5_X_FW_AP_MODE:
		input_info(true, ilits->dev, "%s Switch to AP mode\n", __func__);
		ilits->wait_int_timeout = AP_INT_TIMEOUT;
		if (ilits->fw_upgrade_mode == UPGRADE_IRAM) {
			if (ili_fw_upgrade_handler(NULL) < 0)
				input_err(true, ilits->dev, "%s FW upgrade failed\n", __func__);
		} else {
			ret = ili_reset_ctrl(ilits->reset);
		}
		if (ret < 0)
			input_err(true, ilits->dev, "%s TP Reset failed\n", __func__);

		break;
	case P5_X_FW_GESTURE_MODE:
		input_info(true, ilits->dev, "%s Switch to Gesture mode\n", __func__);
		ilits->wait_int_timeout = AP_INT_TIMEOUT;
		ret = ilits->gesture_move_code(ilits->gesture_mode);
		if (ret < 0)
			input_err(true, ilits->dev, "%s Move gesture code failed\n", __func__);
		if (ges_dbg) {
			input_info(true, ilits->dev, "%s Enable gesture debug func\n", __func__);
			ili_set_tp_data_len(DATA_FORMAT_GESTURE_DEBUG, false, NULL);
		}
		break;
	case P5_X_FW_TEST_MODE:
		input_info(true, ilits->dev, "%s Switch to Test mode\n", __func__);
		ilits->wait_int_timeout = MP_INT_TIMEOUT;
		ret = ilits->mp_move_code();
		break;
	default:
		input_err(true, ilits->dev, "%s Unknown TP mode: %x\n", __func__, mode);
		ret = -1;
		break;
	}

	if (ret < 0)
		input_err(true, ilits->dev, "%s Switch TP mode (%d) failed\n", __func__, mode);
	ILI_DBG("%s Actual TP mode = %d\n", __func__, ilits->actual_tp_mode);

	atomic_set(&ilits->tp_sw_mode, END);
	return ret;
}

void ili_print_info(void)
{
	if (!ilits)
		return;

	ilits->print_info_cnt_open++;

	if (ilits->print_info_cnt_open > 0xfff0)
		ilits->print_info_cnt_open = 0;

	if (ilits->touch_count == 0)
		ilits->print_info_cnt_release++;

		input_info(true, ilits->dev,
				"tc:%d ver:%02d%02d%02d%02d// #%d %d\n",
				ilits->touch_count, ilits->fw_cur_info[2],ilits->fw_cur_info[3],ilits->fw_cur_info[4],
				ilits->fw_cur_info[5], ilits->print_info_cnt_open, ilits->print_info_cnt_release);

}

static void ili_print_info_work(struct work_struct *work)
{
	ili_print_info();

	schedule_delayed_work(&ilits->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

int ili_gesture_recovery(void)
{
	int ret = 0;

	atomic_set(&ilits->esd_stat, START);

	input_info(true, ilits->dev, "%s Doing gesture recovery\n", __func__);
	ret = ilits->ges_recover();

	atomic_set(&ilits->esd_stat, END);
	return ret;
}

void ili_spi_recovery(void)
{
	atomic_set(&ilits->esd_stat, START);

	input_info(true, ilits->dev, "%s Doing spi recovery\n", __func__);
	if (ili_fw_upgrade_handler(NULL) < 0)
		input_err(true, ilits->dev, "%s FW upgrade failed\n", __func__);
	set_current_ic_mode(SET_MODE_NORMAL);
	atomic_set(&ilits->esd_stat, END);
}

int ili_wq_esd_spi_check(void)
{
	int ret = 0;
	u8 tx = SPI_WRITE, rx = 0;

	ret = ilits->spi_write_then_read(ilits->spi, &tx, 1, &rx, 1);
	ILI_DBG("%s spi esd check = 0x%x\n", __func__, ret);
	if (ret == DO_SPI_RECOVER) {
		input_err(true, ilits->dev, "%s ret = 0x%x\n", ret, __func__);
		return -1;
	}
	return 0;
}

int ili_wq_esd_i2c_check(void)
{
	ILI_DBG("%s", __func__);
	return 0;
}

static void ilitek_tddi_wq_esd_check(struct work_struct *work)
{
	mutex_lock(&ilits->touch_mutex);
	if (ilits->esd_recover() < 0) {
		input_err(true, ilits->dev, "%s SPI ACK failed, doing spi recovery\n", __func__);
		ili_spi_recovery();
	}
	mutex_unlock(&ilits->touch_mutex);
	complete_all(&ilits->esd_done);
	ili_wq_ctrl(WQ_ESD, ENABLE);
}

static int read_power_status(u8 *buf)
{
	struct file *f = NULL;
	mm_segment_t old_fs;
	ssize_t byte = 0;

	old_fs = get_fs();
	set_fs(get_ds());

	f = filp_open(POWER_STATUS_PATH, O_RDONLY, 0);
	if (ERR_ALLOC_MEM(f)) {
		input_err(true, ilits->dev, "%s Failed to open %s\n", __func__, POWER_STATUS_PATH);
		return -1;
	}

	f->f_op->llseek(f, 0, SEEK_SET);
	byte = f->f_op->read(f, buf, 20, &f->f_pos);

	ILI_DBG("%s Read %d bytes\n", __func__, (int)byte);

	set_fs(old_fs);
	filp_close(f, NULL);
	return 0;
}

static void ilitek_tddi_wq_bat_check(struct work_struct *work)
{
	u8 str[20] = {0};
	static int charge_mode;

	if (read_power_status(str) < 0)
		input_err(true, ilits->dev, "%s Read power status failed\n", __func__);

	ILI_DBG("%s Batter Status: %s\n", __func__, str);

	if (strstr(str, "Charging") != NULL || strstr(str, "Full") != NULL
		|| strstr(str, "Fully charged") != NULL) {
		if (charge_mode != 1) {
			ILI_DBG("%s Charging mode\n", __func__);
			if (ili_ic_func_ctrl("plug", DISABLE) < 0) // plug in
				input_err(true, ilits->dev, "%s Write plug in failed\n", __func__);
			charge_mode = 1;
		}
	} else {
		if (charge_mode != 2) {
			ILI_DBG("%s Not charging mode\n", __func__);
			if (ili_ic_func_ctrl("plug", ENABLE) < 0) // plug out
				input_err(true, ilits->dev, "%s Write plug out failed\n", __func__);
			charge_mode = 2;
		}
	}
	ili_wq_ctrl(WQ_BAT, ENABLE);
}

void ili_wq_ctrl(int type, int ctrl)
{
	switch (type) {
	case WQ_ESD:
		if (ENABLE_WQ_ESD || ilits->wq_ctrl) {
			if (!esd_wq) {
				input_err(true, ilits->dev, "%s wq esd is null\n", __func__);
				break;
			}
			ilits->wq_esd_ctrl = ctrl;
			if (ctrl == ENABLE) {
				ILI_DBG("%s execute esd check\n", __func__);
				if (!queue_delayed_work(esd_wq, &esd_work, msecs_to_jiffies(WQ_ESD_DELAY)))
					ILI_DBG("%s esd check was already on queue\n", __func__);
			} else {
				cancel_delayed_work_sync(&esd_work);
				flush_workqueue(esd_wq);
				ILI_DBG("%s cancel esd wq\n", __func__);
			}
		}
		break;
	case WQ_BAT:
		if (ENABLE_WQ_BAT || ilits->wq_ctrl) {
			if (!bat_wq) {
				input_err(true, ilits->dev, "%s WQ BAT is null\n", __func__);
				break;
			}
			ilits->wq_bat_ctrl = ctrl;
			if (ctrl == ENABLE) {
				ILI_DBG("%s execute bat check\n", __func__);
				if (!queue_delayed_work(bat_wq, &bat_work, msecs_to_jiffies(WQ_BAT_DELAY)))
					ILI_DBG("%s bat check was already on queue\n", __func__);
			} else {
				cancel_delayed_work_sync(&bat_work);
				flush_workqueue(bat_wq);
				ILI_DBG("%s cancel bat wq\n", __func__);
			}
		}
		break;
	default:
		input_err(true, ilits->dev, "%s Unknown WQ type, %d\n", __func__, type);
		break;
	}
}

void set_current_ic_mode(int mode)
{
	int ret = 0;

	input_info(true, ilits->dev, "%s,mode:%d\n", __func__, mode);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_info(true, ilits->dev, "%s power off satus\n", __func__);
		return;
	}
	switch (mode) {
	case SET_MODE_NORMAL:
		if (!ilits->dead_zone_enabled) {
			ret = ili_ic_func_ctrl("dead_zone_ctrl", DEAD_ZONE_DISABLE);
			if (ret < 0)
				input_err(true, ilits->dev, "%s DEAD_ZONE_ENABLE failed\n", __func__);
		}

		if (ilits->sip_mode_enabled) {
			ret = ili_ic_func_ctrl("sip_mode", SIP_MODE_ENABLE);
			if (ret < 0)
				input_err(true, ilits->dev, "%s SIP_MODE_ENABLE failed\n", __func__);
		}

		if (ilits->game_mode_enabled) {
			ret = ili_ic_func_ctrl("lock_point", GAME_MODE_ENABLE);
			if (ret < 0)
				input_err(true, ilits->dev, "%s GAME_MODE_ENABLE failed\n", __func__);
		}

		if (ilits->prox_face_mode) {
			ret = ili_ic_func_ctrl("proximity", ilits->prox_face_mode);
			if (ret < 0)
				input_err(true, ilits->dev, "%s ear detect enabled fail(%d)\n",
							__func__, ilits->prox_face_mode);
		}
		if (ilits->usb_plug_status == USB_PLUG_ATTACHED) {
			ret = ili_ic_func_ctrl("plug", !ilits->usb_plug_status);
			if (ret < 0)
				input_err(true, ilits->dev, "%s USB_PLUG_ATTACHED failed(%d)\n",
							__func__, ilits->usb_plug_status);
		}
		
	break;
	case SET_MODE_PROXIMTY_LCDOFF:
		if (ilits->prox_face_mode && ilits->tp_suspend) {
			if (ilits->prox_lp_scan_mode_enabled) {
				ret = ili_ic_func_ctrl("lpwg", 0x21);//proximity report start
				if (ret < 0)
					input_err(true, ilits->dev, "%s proximity report start failed\n", __func__);
			} else {
				if (ili_ic_func_ctrl("sleep", SLEEP_IN) < 0)
					input_err(true, ilits->dev, "%s Write sleep in cmd failed\n", __func__);
			}
		}
	break;
	case SET_MODE_GESTURE:
		ili_switch_tp_mode(P5_X_FW_GESTURE_MODE);
	break;
	default:
		input_err(true, ilits->dev, "%s Unknown mode\n", __func__);
	break;
	}
}

static void ilitek_tddi_wq_init(void)
{
	esd_wq = alloc_workqueue("esd_check", WQ_MEM_RECLAIM, 0);
	bat_wq = alloc_workqueue("bat_check", WQ_MEM_RECLAIM, 0);

	WARN_ON(!esd_wq);
	WARN_ON(!bat_wq);

	INIT_DELAYED_WORK(&esd_work, ilitek_tddi_wq_esd_check);
	INIT_DELAYED_WORK(&bat_work, ilitek_tddi_wq_bat_check);

#if RESUME_BY_DDI
	resume_by_ddi_wq = create_singlethread_workqueue("resume_by_ddi_wq");
	WARN_ON(!resume_by_ddi_wq);
	INIT_WORK(&resume_by_ddi_work, ilitek_resume_by_ddi_work);
#endif

	INIT_DELAYED_WORK(&ilits->work_print_info, ili_print_info_work);

}

int ili_sleep_handler(int mode)
{
	int ret = 0;
	bool sense_stop = true;

	mutex_lock(&ilits->touch_mutex);
	atomic_set(&ilits->tp_sleep, START);

	if (atomic_read(&ilits->fw_stat) ||
		atomic_read(&ilits->mp_stat)) {
		input_info(true, ilits->dev, "%s fw upgrade or mp still running, ignore sleep requst\n", __func__);
		atomic_set(&ilits->tp_sleep, END);
		mutex_unlock(&ilits->touch_mutex);
		return 0;
	}

	if (ilits->sleep_handler_mode == mode) {
		input_info(true, ilits->dev, "%s skip, already mode %d\n", __func__, mode);
		atomic_set(&ilits->tp_sleep, END);
		mutex_unlock(&ilits->touch_mutex);
		return 0;
	}

	ili_wq_ctrl(WQ_ESD, DISABLE);
	ili_wq_ctrl(WQ_BAT, DISABLE);
	ili_irq_disable();

	if (ilits->ss_ctrl)
		sense_stop = true;
	else if (ilits->chip->core_ver >= CORE_VER_1430)
		sense_stop = false;
	else
		sense_stop = true;
	/* If you want to add other mode, must check sleep_handler_mode condition. */
	switch (mode) {
	case TP_EARLY_SUSPEND:
		ilits->tp_suspend = true;
		if (sense_stop) {
			if (ili_ic_func_ctrl("sense", DISABLE) < 0)
				input_err(true, ilits->dev, "%s Write sense stop cmd failed\n", __func__);
			if (ili_ic_check_busy(50, 20) < 0)
				input_err(true, ilits->dev, "%s Check busy timeout during suspend\n", __func__);
		}
		input_info(true, ilits->dev, "%s prox_power_off:%d, prox_face_mode:%d, gesture:%d, ges_sym:0x%x, incell_power_state:%d\n",
				__func__, ilits->prox_power_off, ilits->prox_face_mode,
				ilits->gesture, ilits->ges_sym, ilits->incell_power_state);
		if (ilits->prox_face_mode || ilits->gesture) {
			ili_incell_power_control(ENABLE);
			ilits->power_status = LP_AOT_STATUS;
			ili_switch_tp_mode(P5_X_FW_GESTURE_MODE);
			ili_irq_wake_enable();
			ili_irq_enable();
			if (ilits->prox_face_mode) {
				ret = ili_ic_func_ctrl("sleep", SLEEP_IN);
				if (ret < 0)
					input_err(true, ilits->dev, "%s Write sleep in cmd failed\n", __func__);
				else
					ilits->power_status = LP_PROX_STATUS;
			}
			if (ilits->prox_lp_scan_mode_enabled) {
				set_current_ic_mode(SET_MODE_GESTURE);
				set_current_ic_mode(SET_MODE_PROXIMTY_LCDOFF);
			}
		} else if (ilits->incell_power_state) {
			ili_irq_wake_enable();
			ili_irq_enable();
			ilits->power_status = LP_FACTORY_STATUS;
		} else {
			ilitek_pin_control(false);
			ilits->power_status = POWER_OFF_STATUS;
		}
		ilits->screen_off_sate = TP_EARLY_SUSPEND;
		ilits->sleep_handler_mode = mode;
		break;
	case TP_EARLY_RESUME:
		input_info(true, ilits->dev, "%s ilits->power_status:%d, ges_sym:0x%x\n",
				__func__, ilits->power_status, ilits->ges_sym);
		if ((ilits->power_status != POWER_OFF_STATUS) && (ilits->power_status != POWER_ON_STATUS)) {
			ili_irq_wake_disable();
			if (ilits->power_status == LP_PROX_STATUS) {
				ilits->actual_tp_mode = P5_X_FW_AP_MODE;
				ili_ic_func_ctrl("sleep", 0x01);
				ili_ic_func_ctrl("sense", 0x01);
				ilits->tp_data_format = DATA_FORMAT_DEMO;
				ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_LEN;
				ilits->pll_clk_wakeup = false;
				ilits->prox_power_off = 0;
				ilits->prox_lp_scan_mode_enabled = false;
			}
			if (ilits->power_status != LP_FACTORY_STATUS) {
				if (ilits->chip->id == ILI7807_CHIP) {
					ilitek_pin_control(false);
					usleep_range(5 * 1000, 5 * 1000);
					ili_incell_power_control(DISABLE);
				} else {
					ili_incell_power_control(DISABLE);
					ilitek_pin_control(false);
				}
				ilits->power_status = POWER_OFF_STATUS;
				usleep_range(15000, 15000);
			}
		}
		ilits->screen_off_sate = TP_EARLY_RESUME;
		break;
	case TP_RESUME:
#if !RESUME_BY_DDI
		ilits->power_status = POWER_ON_STATUS;
		/* Set tp as demo mode and reload code if it's iram. */
		ilits->actual_tp_mode = P5_X_FW_AP_MODE;
		ilits->screen_off_sate = TP_RESUME;

		ilitek_pin_control(true);

		if (ilits->fw_upgrade_mode == UPGRADE_IRAM) {
			if (ili_fw_upgrade_handler(NULL) < 0)
				input_err(true, ilits->dev, "%sFW upgrade failed during resume\n", __func__);
		} else {
			if (ili_reset_ctrl(ilits->reset) < 0)
				input_err(true, ilits->dev, "%sTP Reset failed during resume\n", __func__);
			ili_ic_func_ctrl_reset();
		}

		ilits->tp_suspend = false;
#if PROXIMITY_BY_FW
		ilits->proxmity_face = false;
#endif
#endif
		ili_wq_ctrl(WQ_ESD, ENABLE);
		ili_wq_ctrl(WQ_BAT, ENABLE);

		msleep(ilits->rst_edge_delay);//resume, after 10ms enable irq , for INT noisy

		set_current_ic_mode(SET_MODE_NORMAL);

		ili_irq_enable();
		ilits->sleep_handler_mode = mode;
		break;
	default:
		input_err(true, ilits->dev, "%sUnknown sleep mode, %d\n", __func__, mode);
		ret = -EINVAL;
		break;
	}

	if (mode == TP_RESUME) {
		cancel_delayed_work(&ilits->work_print_info);
		ilits->print_info_cnt_open = 0;
		ilits->print_info_cnt_release = 0;
		if (!ili_shutdown_is_on_going_tsp)
			schedule_work(&ilits->work_print_info.work);
	} else if (mode == TP_EARLY_SUSPEND || mode == TP_DEEP_SLEEP) {
		cancel_delayed_work(&ilits->work_print_info);
		ili_print_info();
	}

	ili_touch_release_all_point();
	atomic_set(&ilits->tp_sleep, END);
	mutex_unlock(&ilits->touch_mutex);
	input_info(true, ilits->dev, "%s end (mode %d)\n", __func__, ilits->sleep_handler_mode);
	return ret;
}

int ili_fw_upgrade_handler(void *data)
{
	int ret = 0;

	atomic_set(&ilits->fw_stat, START);

	ilits->fw_update_stat = FW_STAT_INIT;
	ret = ili_fw_upgrade(ilits->fw_open);
	if (ret != 0) {
		input_info(true, ilits->dev, "%s FW upgrade fail\n", __func__);
		ilits->fw_update_stat = FW_UPDATE_FAIL;
	} else {
		input_info(true, ilits->dev, "%s FW upgrade pass\n", __func__);
#if CHARGER_NOTIFIER_CALLBACK
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
		/* add_for_charger_start */
		if ((ilits->usb_plug_status) && (ilits->actual_tp_mode != P5_X_FW_TEST_MODE)) {
			ret = ili_ic_func_ctrl("plug", !ilits->usb_plug_status);// plug in
			if (ret < 0)
				input_err(true, ilits->dev, "%sWrite plug in failed\n", __func__);
		}
		/*  add_for_charger_end  */
#endif
#endif
		ilits->fw_update_stat = FW_UPDATE_PASS;
	}

	if (!ilits->boot) {
		ilits->boot = true;
		input_info(true, ilits->dev, "%s Registre touch to input subsystem\n", __func__);
		ili_input_register();
		ili_wq_ctrl(WQ_ESD, ENABLE);
		ili_wq_ctrl(WQ_BAT, ENABLE);
	}

	atomic_set(&ilits->fw_stat, END);
	return ret;
}

int ili_set_tp_data_len(int format, bool send, u8 *data)
{
	u8 cmd[10] = {0}, ctrl = 0, debug_ctrl = 0;
	u16 self_key = 2;
	int ret = 0, tp_mode = ilits->actual_tp_mode, len = 0;

	switch (format) {
	case DATA_FORMAT_DEMO:
#if AXIS_PACKET
		len = P5_X_DEMO_MODE_PACKET_INFO_LEN + P5_X_DEMO_MODE_PACKET_LEN +
				P5_X_DEMO_MODE_AXIS_LEN + P5_X_DEMO_MODE_STATE_INFO;
		ctrl = DATA_FORMAT_DEMO_CMD;
		break;
#endif
	case DATA_FORMAT_GESTURE_DEMO:
		len = P5_X_DEMO_MODE_PACKET_LEN;
		ctrl = DATA_FORMAT_DEMO_CMD;
		break;
	case DATA_FORMAT_DEBUG:
#if AXIS_PACKET
		len = (2 * ilits->xch_num * ilits->ych_num) + (ilits->stx * 2) + (ilits->srx * 2);
		len += 2 * self_key + 1 + 35;
		len += P5_X_DEMO_MODE_AXIS_LEN + P5_X_DEMO_MODE_STATE_INFO;
		ctrl = DATA_FORMAT_DEBUG_CMD;
		break;
#endif
	case DATA_FORMAT_GESTURE_DEBUG:
		len = (2 * ilits->xch_num * ilits->ych_num) + (ilits->stx * 2) + (ilits->srx * 2);
		len += 2 * self_key + (8 * 2) + 1 + 35;
		ctrl = DATA_FORMAT_DEBUG_CMD;
		break;
	case DATA_FORMAT_DEMO_DEBUG_INFO:
		/*only suport SPI interface now, so defult use size 1024 buffer*/
		len = P5_X_DEMO_MODE_PACKET_LEN +
			P5_X_DEMO_DEBUG_INFO_ID0_LENGTH + P5_X_INFO_HEADER_LENGTH;
		ctrl = DATA_FORMAT_DEMO_DEBUG_INFO_CMD;
		break;
	case DATA_FORMAT_GESTURE_INFO:
		len = P5_X_GESTURE_INFO_LENGTH;
		ctrl = DATA_FORMAT_GESTURE_INFO_CMD;
		break;
	case DATA_FORMAT_GESTURE_NORMAL:
		len = P5_X_GESTURE_NORMAL_LENGTH;
		ctrl = DATA_FORMAT_GESTURE_NORMAL_CMD;
		break;
	case DATA_FORMAT_GESTURE_SPECIAL_DEMO:
		if (ilits->gesture_demo_ctrl == ENABLE) {
			if (ilits->gesture_mode == DATA_FORMAT_GESTURE_INFO)
				len = P5_X_GESTURE_INFO_LENGTH + P5_X_INFO_HEADER_LENGTH + P5_X_INFO_CHECKSUM_LENGTH;
			else
				len = P5_X_DEMO_MODE_PACKET_LEN + P5_X_INFO_HEADER_LENGTH + P5_X_INFO_CHECKSUM_LENGTH;
		} else {
			if (ilits->gesture_mode == DATA_FORMAT_GESTURE_INFO)
				len = P5_X_GESTURE_INFO_LENGTH;
			else
				len = P5_X_GESTURE_NORMAL_LENGTH;
		}
		input_info(true, ilits->dev, "%s Gesture demo mode control = %d\n",
				__func__,  ilits->gesture_demo_ctrl);
		ili_ic_func_ctrl("gesture_demo_en", ilits->gesture_demo_ctrl);
		input_info(true, ilits->dev, "%s knock_en setting\n", __func__);
		ili_ic_func_ctrl("knock_en", 0x8);
		break;
	case DATA_FORMAT_DEBUG_LITE_ROI:
		debug_ctrl = DATA_FORMAT_DEBUG_LITE_ROI_CMD;
		ctrl = DATA_FORMAT_DEBUG_LITE_CMD;
		break;
	case DATA_FORMAT_DEBUG_LITE_WINDOW:
		debug_ctrl = DATA_FORMAT_DEBUG_LITE_WINDOW_CMD;
		ctrl = DATA_FORMAT_DEBUG_LITE_CMD;
		break;
	case DATA_FORMAT_DEBUG_LITE_AREA:
/*
 *		if(cmd == "") {
 *			input_err(true, ilits->dev, "%sDATA_FORMAT_DEBUG_LITE_AREA error cmd\n", __func__);
 *			return -1;
 *		}
 */
		debug_ctrl = DATA_FORMAT_DEBUG_LITE_AREA_CMD;
		ctrl = DATA_FORMAT_DEBUG_LITE_CMD;
		cmd[3] = data[0];
		cmd[4] = data[1];
		cmd[5] = data[2];
		cmd[6] = data[3];
		break;
	default:
		input_err(true, ilits->dev, "%sUnknow TP data format\n", __func__);
		return -1;
	}

	if (ctrl == DATA_FORMAT_DEBUG_LITE_CMD) {
		len = P5_X_DEBUG_LITE_LENGTH;
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = ctrl;
		cmd[2] = debug_ctrl;
		ret = ilits->wrapper(cmd, 10, NULL, 0, ON, OFF);

		if (ret < 0) {
			input_err(true, ilits->dev, "%sswitch to format %d failed\n", __func__, format);
			ili_switch_tp_mode(P5_X_FW_AP_MODE);
		}

	} else if (tp_mode == P5_X_FW_AP_MODE ||
		format == DATA_FORMAT_GESTURE_DEMO ||
		format == DATA_FORMAT_GESTURE_DEBUG) {
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = ctrl;
		ret = ilits->wrapper(cmd, 2, NULL, 0, ON, OFF);

		if (ret < 0) {
			input_err(true, ilits->dev, "%s switch to format %d failed\n", __func__, format);
			ili_switch_tp_mode(P5_X_FW_AP_MODE);
		}
	} else if (tp_mode == P5_X_FW_GESTURE_MODE) {

		/*set gesture symbol*/
		ili_set_gesture_symbol();

		if (send) {
			if (ilits->prox_face_mode)
				ret = ili_ic_func_ctrl("lpwg", 0x20);
			else
				ret = ili_ic_func_ctrl("lpwg", ctrl);

			if (ret < 0)
				input_err(true, ilits->dev, "%s write gesture mode failed\n", __func__);
		}
	}

	ilits->tp_data_format = format;
	ilits->tp_data_len = len;
	input_info(true, ilits->dev, "%s TP mode = %d, format = %d, len = %d\n",
		__func__, tp_mode, ilits->tp_data_format, ilits->tp_data_len);

	return ret;
}

int ili_report_handler(void)
{
	int ret = 0, pid = 0;
	u8  checksum = 0, pack_checksum = 0;
	u8 *trdata = NULL;
	int rlen = 0;
	int tmp = debug_en;

	/* Just in case these stats couldn't be blocked in top half context */
	if (!ilits->report || atomic_read(&ilits->tp_reset) ||
		atomic_read(&ilits->fw_stat) || atomic_read(&ilits->tp_sw_mode) ||
		atomic_read(&ilits->mp_stat) || atomic_read(&ilits->tp_sleep)) {
		input_info(true, ilits->dev, "%s ignore report request\n", __func__);
		return -EINVAL;
	}

	if (ilits->irq_after_recovery) {
		input_info(true, ilits->dev, "%s ignore int triggered by recovery\n", __func__);
		ilits->irq_after_recovery = false;
		return -EINVAL;
	}

	ili_wq_ctrl(WQ_ESD, DISABLE);
	ili_wq_ctrl(WQ_BAT, DISABLE);

	if (ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) {
		__pm_stay_awake(ilits->ws);

		if (ilits->pm_suspend) {
			/* Waiting for pm resume completed */
			ret = wait_for_completion_timeout(&ilits->pm_completion, msecs_to_jiffies(700));
			if (!ret) {
				input_err(true, ilits->dev, "%ssystem(spi) can't finished resuming procedure.",
						__func__);
			}
		}
	}

	rlen = ilits->tp_data_len;
	ILI_DBG("%s Packget length = %d\n", __func__, rlen);

	if (!rlen || rlen > TR_BUF_SIZE) {
		input_err(true, ilits->dev, "%sLength of packet is invaild\n", __func__);
		goto out;
	}

	memset(ilits->tr_buf, 0x0, TR_BUF_SIZE);

	ret = ilits->wrapper(NULL, 0, ilits->tr_buf, rlen, OFF, OFF);

#if PROXIMITY_BY_FW
	if (ilits->tr_buf[0] == P5_X_DEMO_PROXUMITY_ID) {
		ili_report_proximity_mode(ilits->tr_buf[1], 1);
		goto out;
	}
#endif

	if (ret < 0) {
		input_err(true, ilits->dev, "%s Read report packet failed, ret = %d\n", __func__, ret);
		if (ret == DO_SPI_RECOVER) {
			ili_ic_get_pc_counter(DO_SPI_RECOVER);

			input_info(true, ilits->dev, "%s prox_power_off:%d, ilits->tp_suspend:%d, ilits->actual_tp_mode:%d, prox_face_mode:%d, gesture:%d,\n",
				__func__, ilits->prox_power_off, ilits->tp_suspend, ilits->actual_tp_mode,
				ilits->prox_face_mode, ilits->gesture);

			if ((ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) &&
					(ilits->gesture || ilits->prox_face_mode) && ilits->tp_suspend) {
				input_err(true, ilits->dev, "%s Gesture failed, doing gesture recovery\n", __func__);
				if (ili_gesture_recovery() < 0)
					input_err(true, ilits->dev, "%s Failed to recover gesture\n", __func__);
				ilits->irq_after_recovery = true;
			} else {
				input_err(true, ilits->dev, "%s SPI ACK failed, doing spi recovery\n", __func__);
				ili_spi_recovery();
				ilits->irq_after_recovery = true;
			}
		}
		goto out;
	}

	ili_dump_data(ilits->tr_buf, 8, rlen, 0, "finger report");

	checksum = ili_calc_packet_checksum(ilits->tr_buf, rlen - 1);
	pack_checksum = ilits->tr_buf[rlen-1];
	trdata = ilits->tr_buf;
	pid = trdata[0];
	ILI_DBG("%s Packet ID = %x\n", __func__, pid);

	if (checksum != pack_checksum && pid != P5_X_I2CUART_PACKET_ID) {
		input_err(true, ilits->dev, "%s Checksum Error (0x%X)! Pack = 0x%X, len = %d\n",
		__func__, checksum, pack_checksum, rlen);
		debug_en = DEBUG_ALL;
		ili_dump_data(trdata, 8, rlen, 0, "finger report with wrong");
		debug_en = tmp;
		ret = -EINVAL;
		goto out;
	}

	if (pid == P5_X_INFO_HEADER_PACKET_ID) {
		trdata = ilits->tr_buf + P5_X_INFO_HEADER_LENGTH;
		pid = trdata[0];
	}

	switch (pid) {
	case P5_X_DEMO_PACKET_ID:
		ili_report_ap_mode(trdata, rlen);
		break;
	case P5_X_DEBUG_PACKET_ID:
		ili_report_debug_mode(trdata, rlen);
		break;
#if AXIS_PACKET
	case P5_X_DEMO_AXIS_PACKET_ID:
		ili_report_ap_mode(&trdata[P5_X_DEMO_MODE_PACKET_INFO_LEN],
			(rlen - P5_X_DEMO_MODE_PACKET_INFO_LEN - P5_X_DEMO_MODE_AXIS_LEN - P5_X_DEMO_MODE_STATE_INFO));
		break;
	case P5_X_DEBUG_AXIS_PACKET_ID:
		ili_report_debug_mode(trdata, rlen);
		break;
#endif
	case P5_X_DEBUG_LITE_PACKET_ID:
		ili_report_debug_lite_mode(trdata, rlen);
		break;
	case P5_X_I2CUART_PACKET_ID:
		ili_report_i2cuart_mode(trdata, rlen);
		break;
	case P5_X_GESTURE_PACKET_ID:
		ili_report_gesture_mode(trdata, rlen);
		break;
	case P5_X_GESTURE_FAIL_ID:
		input_info(true, ilits->dev, "%s gesture fail reason code = 0x%02x", __func__, trdata[1]);
		break;
	case P5_X_DEMO_DEBUG_INFO_PACKET_ID:
		ili_demo_debug_info_mode(trdata, rlen);
		break;
	default:
		input_err(true, ilits->dev, "%s Unknown packet id, %x\n", __func__, pid);
#if (TDDI_INTERFACE == BUS_I2C)
		ili_ic_get_pc_counter(DO_I2C_RECOVER);
		if (ilits->fw_latch != 0) {
			msleep(100);
			ili_ic_func_ctrl_reset();
			input_err(true, ilits->dev, "%s I2C func_ctrl_reset\n", __func__);
		}
		if ((ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) && ilits->fw_latch != 0) {
			msleep(100);
			ili_set_gesture_symbol();
			input_err(true, ilits->dev, "%s I2C gesture_symbol\n", __func__);
		}
#endif
		break;
	}

out:
	if (ilits->actual_tp_mode != P5_X_FW_GESTURE_MODE) {
		ili_wq_ctrl(WQ_ESD, ENABLE);
		ili_wq_ctrl(WQ_BAT, ENABLE);
	}

	if (ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE)
		__pm_relax(ilits->ws);
	return ret;
}

int ili_reset_ctrl(int mode)
{
	int ret = 0;

	atomic_set(&ilits->tp_reset, START);

	switch (mode) {
	case TP_IC_CODE_RST:
		input_info(true, ilits->dev, "%s TP IC Code RST\n", __func__);
		ret = ili_ic_code_reset();
		if (ret < 0)
			input_err(true, ilits->dev, "%s IC Code reset failed\n", __func__);
		break;
	case TP_IC_WHOLE_RST:
		input_info(true, ilits->dev, "%s TP IC whole RST\n", __func__);
		ret = ili_ic_whole_reset();
		if (ret < 0)
			input_err(true, ilits->dev, "%s IC whole reset failed\n", __func__);
		break;
	case TP_HW_RST_ONLY:
		input_info(true, ilits->dev, "%s TP HW RST\n", __func__);
		ili_tp_reset();
		break;
	default:
		input_err(true, ilits->dev, "%s Unknown reset mode, %d\n", __func__, mode);
		ret = -EINVAL;
		break;
	}

	/*
	 * Since OTP must be folloing with reset, except for code rest,
	 * the stat of ice mode should be set as 0.
	 */
	if (mode != TP_IC_CODE_RST)
		atomic_set(&ilits->ice_stat, DISABLE);

	ilits->tp_data_format = DATA_FORMAT_DEMO;
	ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_LEN;
#if AXIS_PACKET
	ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_INFO_LEN + P5_X_DEMO_MODE_PACKET_LEN +
					P5_X_DEMO_MODE_AXIS_LEN + P5_X_DEMO_MODE_STATE_INFO;
#endif
	ilits->pll_clk_wakeup = true;
	atomic_set(&ilits->tp_reset, END);
	return ret;
}

static int ilitek_get_tp_module(void)
{
	/*
	 * TODO: users should implement this function
	 * if there are various tp modules been used in projects.
	 */

	return MODEL_DEF;
}

static void ili_update_tp_module_info(void)
{
	int module;

	module = ilitek_get_tp_module();

	switch (module) {
	case MODEL_CSOT:
		ilits->md_name = "CSOT";
		ilits->md_fw_filp_path = CSOT_FW_FILP_PATH;
		ilits->md_fw_rq_path = CSOT_FW_REQUEST_PATH;
		ilits->md_ini_path = CSOT_INI_NAME_PATH;
		ilits->md_ini_rq_path = CSOT_INI_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_CSOT;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_CSOT);
		break;
	case MODEL_AUO:
		ilits->md_name = "AUO";
		ilits->md_fw_filp_path = AUO_FW_FILP_PATH;
		ilits->md_fw_rq_path = AUO_FW_REQUEST_PATH;
		ilits->md_ini_path = AUO_INI_NAME_PATH;
		ilits->md_ini_rq_path = AUO_INI_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_AUO;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_AUO);
		break;
	case MODEL_BOE:
		ilits->md_name = "BOE";
		ilits->md_fw_filp_path = BOE_FW_FILP_PATH;
		ilits->md_fw_rq_path = BOE_FW_REQUEST_PATH;
		ilits->md_ini_path = BOE_INI_NAME_PATH;
		ilits->md_ini_rq_path = BOE_INI_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_BOE;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_BOE);
		break;
	case MODEL_INX:
		ilits->md_name = "INX";
		ilits->md_fw_filp_path = INX_FW_FILP_PATH;
		ilits->md_fw_rq_path = INX_FW_REQUEST_PATH;
		ilits->md_ini_path = INX_INI_NAME_PATH;
		ilits->md_ini_rq_path = INX_INI_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_INX;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_INX);
		break;
	case MODEL_DJ:
		ilits->md_name = "DJ";
		ilits->md_fw_filp_path = DJ_FW_FILP_PATH;
		ilits->md_fw_rq_path = DJ_FW_REQUEST_PATH;
		ilits->md_ini_path = DJ_INI_NAME_PATH;
		ilits->md_ini_rq_path = DJ_INI_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_DJ;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_DJ);
		break;
	case MODEL_TXD:
		ilits->md_name = "TXD";
		ilits->md_fw_filp_path = TXD_FW_FILP_PATH;
		ilits->md_fw_rq_path = TXD_FW_REQUEST_PATH;
		ilits->md_ini_path = TXD_INI_NAME_PATH;
		ilits->md_ini_rq_path = TXD_FW_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_TXD;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_TXD);
		break;
	case MODEL_TM:
		ilits->md_name = "TM";
		ilits->md_fw_filp_path = TM_FW_REQUEST_PATH;
		ilits->md_fw_rq_path = TM_FW_REQUEST_PATH;
		ilits->md_ini_path = TM_INI_NAME_PATH;
		ilits->md_ini_rq_path = TM_INI_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_TM;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_TM);
		break;
	default:
		break;
	}

	if (module == 0 || ilits->md_fw_ili_size < ILI_FILE_HEADER) {
		input_err(true, ilits->dev, "%s Couldn't find any tp modules, applying default settings\n", __func__);
		ilits->md_name = "DEF";
		ilits->md_fw_filp_path = DEF_FW_FILP_PATH;
		ilits->md_fw_rq_path = (char *)ilits->fw_name;
		ilits->md_ini_path = DEF_INI_NAME_PATH;
		ilits->md_ini_rq_path = DEF_INI_REQUEST_PATH;
		/*Fw data loaded ilitek__get_fw_image function
		 *ilits->md_fw_ili = CTPM_FW_DEF;
		 *ilits->md_fw_ili_size = sizeof(CTPM_FW_DEF);
		 */
	}

	input_info(true, ilits->dev, "%s Found %s module: ini path = %s, fw path = (%s, %s, %d)\n",
			__func__,
			ilits->md_name,
			ilits->md_ini_path,
			ilits->md_fw_filp_path,
			ilits->md_fw_rq_path,
			ilits->md_fw_ili_size);

	ilits->tp_module = module;
}

int ili_tddi_init(void)
{
#if (BOOT_FW_UPDATE | HOST_DOWN_LOAD)
	struct task_struct *fw_boot_th;
#endif
	int retval;

	input_info(true, ilits->dev, "%s driver version = %s\n", __func__, DRIVER_VERSION);

	mutex_init(&ilits->touch_mutex);
	mutex_init(&ilits->debug_mutex);
	mutex_init(&ilits->debug_read_mutex);
	init_waitqueue_head(&(ilits->inq));
	spin_lock_init(&ilits->irq_spin);
	init_completion(&ilits->esd_done);

	atomic_set(&ilits->irq_stat, DISABLE);
	atomic_set(&ilits->irq_wake_stat, DISABLE);
	atomic_set(&ilits->ice_stat, DISABLE);
	atomic_set(&ilits->tp_reset, END);
	atomic_set(&ilits->fw_stat, END);
	atomic_set(&ilits->mp_stat, DISABLE);
	atomic_set(&ilits->tp_sleep, END);
	atomic_set(&ilits->cmd_int_check, DISABLE);
	atomic_set(&ilits->esd_stat, END);
	memset(ilits->fw_customer_info, 0x0, sizeof(ilits->fw_customer_info));

	ili_ic_init();
	ilitek_tddi_wq_init();

	/* Must do hw reset once in first time for work normally if tp reset is avaliable */
#if !TDDI_RST_BIND
	if (ili_reset_ctrl(ilits->reset) < 0)
		input_err(true, ilits->dev, "%s TP Reset failed during init\n", __func__);
#endif

	ilits->demo_debug_info[0] = ili_demo_debug_info_id0;
	ilits->tp_data_format = DATA_FORMAT_DEMO;
	ilits->boot = false;

	/*
	 * This status of ice enable will be reset until process of fw upgrade runs.
	 * it might cause unknown problems if we disable ice mode without any
	 * codes inside touch ic.
	 */
	if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
		input_err(true, ilits->dev, "%s Failed to enable ice mode during ili_tddi_init\n", __func__);

	if (ili_ic_dummy_check() < 0) {
		input_err(true, ilits->dev, "%s Not found ilitek chip\n", __func__);
		return -ENODEV;
	}

	if (ili_ic_get_info() < 0)
		input_err(true, ilits->dev, "%s Chip info is incorrect\n", __func__);

	ili_update_tp_module_info();

	ili_get_ini_path();

	ili_node_init();

	retval = ili_sec_fn_init();
	if (retval < 0)
		input_err(true, ilits->dev, "%s Failed to ili_sec_fn_init\n", __func__);

	ili_fw_read_flash_info();

#if (BOOT_FW_UPDATE | HOST_DOWN_LOAD)
	fw_boot_th = kthread_run(ili_fw_upgrade_handler, NULL, "ili_fw_boot");
	if (fw_boot_th == (struct task_struct *)ERR_PTR) {
		fw_boot_th = NULL;
		WARN_ON(!fw_boot_th);
		input_err(true, ilits->dev, "%s Failed to create fw upgrade thread\n", __func__);
	}
#else
	if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
		input_err(true, ilits->dev, "%s Failed to disable ice mode failed during init\n", __func__);

#if (TDDI_INTERFACE == BUS_I2C)
	ilits->info_from_hex = DISABLE;
#endif

	ili_ic_get_core_ver();
	ili_ic_get_protocl_ver();
	ili_ic_get_fw_ver();
	ili_ic_get_tp_info();
	ili_ic_get_panel_info();

#if (TDDI_INTERFACE == BUS_I2C)
	ilits->info_from_hex = ENABLE;
#endif

	input_info(true, ilits->dev, "%s Registre touch to input subsystem\n", __func__);
	ili_input_register();
	ili_wq_ctrl(WQ_ESD, ENABLE);
	ili_wq_ctrl(WQ_BAT, ENABLE);
	ilits->boot = true;
#endif

#if 1//LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	ilits->ws = wakeup_source_register(ilits->dev, "ili_wakelock"); //4.19
#else
	ilits->ws = wakeup_source_register("ili_wakelock"); //4.9
#endif
	if (!ilits->ws)
		input_err(true, ilits->dev, "%s wakeup source request failed\n", __func__);

	return 0;
}

void ili_dev_remove(void)
{
	input_info(true, ilits->dev, "%s remove ilitek dev\n", __func__);

	if (!ilits)
		return;

	cancel_delayed_work_sync(&ilits->work_print_info);

	ili_shutdown_is_on_going_tsp = true;
	ilits->power_status = POWER_OFF_STATUS;

	ili_irq_wake_disable();
	ili_irq_disable();
	ilitek_pin_control(false);

	gpio_free(ilits->tp_int);
	gpio_free(ilits->tp_rst);

	if (esd_wq != NULL) {
		cancel_delayed_work_sync(&esd_work);
		flush_workqueue(esd_wq);
		destroy_workqueue(esd_wq);
	}
	if (bat_wq != NULL) {
		cancel_delayed_work_sync(&bat_work);
		flush_workqueue(bat_wq);
		destroy_workqueue(bat_wq);
	}
	ili_sec_fn_remove();
	if (ilits->ws)
		wakeup_source_unregister(ilits->ws);

	if (ilits->lcd_bl_en)
		regulator_put(ilits->lcd_bl_en);
	if (ilits->lcd_vddi)
		regulator_put(ilits->lcd_vddi);
	if (ilits->lcd_rst)
		regulator_put(ilits->lcd_rst);

	kfree(ilits->tr_buf);
	kfree(ilits->gcoord);
	ili_interface_dev_exit(ilits);
}

int ili_dev_init(struct ilitek_hwif_info *hwif)
{
	printk(KERN_ERR "[sec_input] %s TP Interface: %s\n", __func__, (hwif->bus_type == BUS_I2C) ? "I2C" : "SPI");
	return ili_interface_dev_init(hwif);
}
