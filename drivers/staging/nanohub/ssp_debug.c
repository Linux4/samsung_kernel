/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "ssp.h"
#include <linux/fs.h>
#include <linux/sec_debug.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>

#define SSP_DEBUG_TIMER_SEC		(5 * HZ)

#define LIMIT_RESET_CNT			40
#define LIMIT_TIMEOUT_CNT		3

/*************************************************************************/
/* SSP Debug timer function                                              */
/*************************************************************************/
int print_mcu_debug(char *pchRcvDataFrame, int *pDataIdx,
		int iRcvDataFrameLength)
{
	int iLength = 0;
	int cur = *pDataIdx;

	memcpy(&iLength, pchRcvDataFrame + *pDataIdx, sizeof(u16));
	*pDataIdx += sizeof(u16);

	if (iLength > iRcvDataFrameLength - *pDataIdx || iLength <= 0) {
		ssp_dbg("[SSP]: MSG From MCU - invalid debug length(%d/%d/%d)\n",
			iLength, iRcvDataFrameLength, cur);
		return iLength ? iLength : ERROR;
	}

	ssp_dbg("[SSP]: MSG From MCU - %s\n", &pchRcvDataFrame[*pDataIdx]);
	*pDataIdx += iLength;
	return 0;
}

void reset_mcu(struct ssp_data *data)
{
	int ret;
	struct contexthub_ipc_info *ipc = platform_get_drvdata(data->pdev);
	
	func_dbg();

	ssp_enable(data, false);
	clean_pending_list(data);
	

	if (atomic_read(&ipc->chub_status) == CHUB_ST_NO_POWER) { /* chub is no power status*/
		ret = contexthub_poweron(ipc);
		if (ret < 0) {
			pr_err("[SSP] contexthub power on failed");
		} else {
			ssp_platform_start_refrsh_task();
		}
	} else {
		ret = contexthub_reset(ipc, 1, 0);
	}
	
	//mcu_reset(false);

	data->uTimeOutCnt = 0;
	data->uComFailCnt = 0;
	data->mcuAbnormal = false;
}

void sync_sensor_state(struct ssp_data *data)
{
	unsigned char uBuf[9] = {0,};
	unsigned int uSensorCnt;
	int iRet = 0;

	gyro_open_calibration(data);
	iRet = set_gyro_cal(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - set_gyro_cal failed\n", __func__);

	iRet = set_accel_cal(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - set_accel_cal failed\n", __func__);

	udelay(10);

	for (uSensorCnt = 0; uSensorCnt < SENSOR_MAX; uSensorCnt++) {
		mutex_lock(&data->enable_mutex);
		if (atomic64_read(&data->aSensorEnable) & (1ULL << uSensorCnt)) {
			s32 dMsDelay =
				get_msdelay(data->adDelayBuf[uSensorCnt]);
			memcpy(&uBuf[0], &dMsDelay, 4);
			memcpy(&uBuf[4], &data->batchLatencyBuf[uSensorCnt], 4);
			uBuf[8] = data->batchOptBuf[uSensorCnt];
			send_instruction(data, ADD_SENSOR, uSensorCnt, uBuf, 9);
			udelay(10);
		}
		mutex_unlock(&data->enable_mutex);
	}
        if (atomic64_read(&data->aSensorEnable) & (1ULL << GYROSCOPE_SENSOR))
                send_vdis_flag(data, data->IsVDIS_Enabled);

	if (data->bProximityRawEnabled == true) {
		s32 dMsDelay = 20;

		memcpy(&uBuf[0], &dMsDelay, 4);
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, uBuf, 4);
	}

	set_proximity_threshold(data);
	set_light_coef(data);

}

static void print_sensordata(struct ssp_data *data, unsigned int uSensor)
{
	switch (uSensor) {
	case ACCELEROMETER_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d (%ums, %dms)\n", uSensor,
			data->buf[uSensor].x, data->buf[uSensor].y,
			data->buf[uSensor].z,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GYROSCOPE_SENSOR:
	case INTERRUPT_GYRO_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d (%ums, %dms)\n", uSensor,
			data->buf[uSensor].gyro.x, data->buf[uSensor].gyro.y,
			data->buf[uSensor].gyro.z,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GEOMAGNETIC_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].cal_x, data->buf[uSensor].cal_y,
			data->buf[uSensor].cal_z, data->buf[uSensor].accuracy,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GEOMAGNETIC_UNCALIB_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d (%ums)\n",
			uSensor,
			data->buf[uSensor].uncal_x,
			data->buf[uSensor].uncal_y,
			data->buf[uSensor].uncal_z,
			data->buf[uSensor].offset_x,
			data->buf[uSensor].offset_y,
			data->buf[uSensor].offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case PRESSURE_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d (%ums, %dms)\n", uSensor,
			data->buf[uSensor].pressure,
			data->buf[uSensor].temperature,
			data->iPressureCal,
			data->sw_offset,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GESTURE_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].data[3], data->buf[uSensor].data[4],
			data->buf[uSensor].data[5], data->buf[uSensor].data[6],
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case TEMPERATURE_HUMIDITY_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].x, data->buf[uSensor].y,
			data->buf[uSensor].z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case LIGHT_SENSOR:
	case UNCAL_LIGHT_SENSOR:
		ssp_dbg("[SSP] %u : %u, %u, %u, %u, %u, %u (%ums)\n", uSensor,
			data->buf[uSensor].light_t.r, data->buf[uSensor].light_t.g,
			data->buf[uSensor].light_t.b, data->buf[uSensor].light_t.w,
			data->buf[uSensor].light_t.a_time, data->buf[uSensor].light_t.a_gain,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case LIGHT_FLICKER_SENSOR:
		ssp_dbg("[SSP] %u : %u, (%ums)\n", uSensor,
			data->buf[uSensor].light_flicker, get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case LIGHT_CCT_SENSOR:
		ssp_dbg("[SSP] %u : %u, %u, %u, %u, %u, %u (%ums)\n", uSensor,
			data->buf[uSensor].light_cct_t.r, data->buf[uSensor].light_cct_t.g,
			data->buf[uSensor].light_cct_t.b, data->buf[uSensor].light_cct_t.w,
			data->buf[uSensor].light_cct_t.a_time, data->buf[uSensor].light_cct_t.a_gain,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case PROXIMITY_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].prox_detect, data->buf[uSensor].prox_adc,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case PROXIMITY_POCKET:
		ssp_dbg("[SSP] %u : %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].proximity_pocket_detect, data->buf[uSensor].proximity_pocket_adc,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case PROXIMITY_ALERT_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].prox_alert_detect, data->buf[uSensor].prox_alert_adc,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case STEP_DETECTOR:
		ssp_dbg("[SSP] %u : %u (%ums, %dms)\n", uSensor,
			data->buf[uSensor].step_det,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GAME_ROTATION_VECTOR:
	case ROTATION_VECTOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d (%ums, %dms)\n", uSensor,
			data->buf[uSensor].quat_a, data->buf[uSensor].quat_b,
			data->buf[uSensor].quat_c, data->buf[uSensor].quat_d,
			data->buf[uSensor].acc_rot,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case SIG_MOTION_SENSOR:
		ssp_dbg("[SSP] %u : %u(%ums)\n", uSensor,
			data->buf[uSensor].sig_motion,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GYRO_UNCALIB_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].uncal_gyro.x, data->buf[uSensor].uncal_gyro.y,
			data->buf[uSensor].uncal_gyro.z, data->buf[uSensor].uncal_gyro.offset_x,
			data->buf[uSensor].uncal_gyro.offset_y,
			data->buf[uSensor].uncal_gyro.offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case STEP_COUNTER:
		ssp_dbg("[SSP] %u : %u(%ums)\n", uSensor,
			data->buf[uSensor].step_diff,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case LIGHT_IR_SENSOR:
		ssp_dbg("[SSP] %u : %u, %u, %u, %u, %u, %u, %u (%ums)\n", uSensor,
			data->buf[uSensor].irdata,
			data->buf[uSensor].ir_r, data->buf[uSensor].ir_g,
			data->buf[uSensor].ir_b, data->buf[uSensor].ir_w,
			data->buf[uSensor].ir_a_time, data->buf[uSensor].ir_a_gain,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case TILT_DETECTOR:
		ssp_dbg("[SSP] %u : %u(%ums)\n", uSensor,
			data->buf[uSensor].tilt_detector,
		get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case PICKUP_GESTURE:
		ssp_dbg("[SSP] %u : %u(%ums)\n", uSensor,
			data->buf[uSensor].pickup_gesture,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case THERMISTOR_SENSOR:
		ssp_dbg("[SSP] %u : %u %u (%ums)\n", uSensor,
			data->buf[uSensor].thermistor_type,
			data->buf[uSensor].thermistor_raw,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case ACCEL_UNCALIB_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].uncal_x, data->buf[uSensor].uncal_y,
			data->buf[uSensor].uncal_z, data->buf[uSensor].offset_x,
			data->buf[uSensor].offset_y,
			data->buf[uSensor].offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case WAKE_UP_MOTION:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].wakeup_move_event[0],
			get_msdelay(data->adDelayBuf[uSensor]));
        break;
    case CALL_GESTURE:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].call_gesture,
			get_msdelay(data->adDelayBuf[uSensor]));
        break;
                break;
        case MOVE_DETECTOR:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].wakeup_move_event[1],
			get_msdelay(data->adDelayBuf[uSensor]));
                break;
	case POCKET_MODE_SENSOR:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].pocket_mode,
			get_msdelay(data->adDelayBuf[uSensor]));
                break;
	case LED_COVER_EVENT_SENSOR:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].led_cover_event,
			get_msdelay(data->adDelayBuf[uSensor]));
                break;
	case TAP_TRACKER_SENSOR:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].tap_tracker_event,
			get_msdelay(data->adDelayBuf[uSensor]));
                break;
	case SHAKE_TRACKER_SENSOR:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].shake_tracker_event,
			get_msdelay(data->adDelayBuf[uSensor]));
                break;
	case LIGHT_SEAMLESS_SENSOR:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].light_seamless_event,
			get_msdelay(data->adDelayBuf[uSensor]));
                break;
	case FLIP_COVER_DETECTOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d %d (%ums)\n", uSensor,
			data->buf[uSensor].value, data->buf[uSensor].nfc_result,
			data->buf[uSensor].diff, data->buf[uSensor].magX,
			data->buf[uSensor].stable_min_mag, data->buf[uSensor].stable_max_mag,
			data->buf[uSensor].detach_mismatch_cnt, data->buf[uSensor].uncal_mag_x,
			data->buf[uSensor].uncal_mag_y,	data->buf[uSensor].uncal_mag_z,
			data->buf[uSensor].saturation,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case AUTOROTATION_SENSOR:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].autorotation_event,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case SAR_BACKOFF_MOTION:
		ssp_dbg("[SSP] %u : %d (%ums)\n", uSensor,
			data->buf[uSensor].sar_backoff_motion_event,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case BULK_SENSOR:
	case GPS_SENSOR:
		break;
	default:
		ssp_dbg("[SSP] Wrong sensorCnt: %u\n", uSensor);
		break;
	}
}

static void debug_work_func(struct work_struct *work)
{
	unsigned int uSensorCnt;
	struct ssp_data *data = container_of(work, struct ssp_data, work_debug);

	ssp_dbg("[SSP]: %s(%u) - Sensor state: 0x%llx, RC: %u(%u, %u, %u), CC: %u, TC: %u NSC: %u EC: %u\n",
		__func__, data->uIrqCnt, data->uSensorState, data->uResetCnt, data->mcuCrashedCnt, data->IsNoRespCnt,
		data->resetCntGPSisOn, data->uComFailCnt, data->uTimeOutCnt, data->uNoRespSensorCnt, data->errorCount);
    
        if(!strstr("", data->resetInfoDebug))
            pr_info("[SSP]: %s(%lld, %lld)\n", data->resetInfoDebug, data->resetInfoDebugTime, get_current_timestamp());

	for (uSensorCnt = 0; uSensorCnt < SENSOR_MAX; uSensorCnt++) {
		if ((atomic64_read(&data->aSensorEnable) & (1ULL << uSensorCnt))
			|| data->batchLatencyBuf[uSensorCnt]) {
			print_sensordata(data, uSensorCnt);
			if (data->indio_dev[uSensorCnt] != NULL
					&& list_empty(&data->indio_dev[uSensorCnt]->buffer->buffer_list)) {
				pr_err("[SSP] %u : buffer_list of iio:device%d is empty!\n",
						 uSensorCnt, data->indio_dev[uSensorCnt]->id);
			}
		}
	}
	pr_info("[SSP] debugwork_mcu : PC(0x%x), LR(0x%x)\n", data->mcu_pc, data->mcu_lr);

	if (data->resetting)
		goto exit;

	if (((atomic64_read(&data->aSensorEnable) & (1 << ACCELEROMETER_SENSOR))
	&& (data->batchLatencyBuf[ACCELEROMETER_SENSOR] == 0)
		&& (data->uIrqCnt == 0) && (data->uTimeOutCnt > 0))
		|| (data->uTimeOutCnt > LIMIT_TIMEOUT_CNT)
		|| (data->mcuAbnormal == true)) {

		mutex_lock(&data->ssp_enable_mutex);
			pr_info("[SSP] : %s - uTimeOutCnt(%u), pending(%u)\n",
				__func__, data->uTimeOutCnt,
				!list_empty(&data->pending_list));
			reset_mcu(data);
		mutex_unlock(&data->ssp_enable_mutex);
	}

exit:
	data->uIrqCnt = 0;
}

static void debug_timer_func(struct timer_list *ptr)
{
	struct ssp_data *data = from_timer(data, ptr, debug_timer);

	queue_work(data->debug_wq, &data->work_debug);
	mod_timer(&data->debug_timer,
		round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void enable_debug_timer(struct ssp_data *data)
{
	mod_timer(&data->debug_timer,
		round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void disable_debug_timer(struct ssp_data *data)
{
	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
}

int initialize_debug_timer(struct ssp_data *data)
{
	timer_setup(&data->debug_timer, debug_timer_func, 0);

	data->debug_wq = create_singlethread_workqueue("ssp_debug_wq");
	if (!data->debug_wq)
		return ERROR;

	INIT_WORK(&data->work_debug, debug_work_func);
	return SUCCESS;
}

/* if returns true dump mode on */
unsigned int ssp_check_sec_dump_mode(void)
{
#if 0 /* def CONFIG_SEC_DEBUG */
	if (sec_debug_level.en.kernel_fault == 1)
		return 1;
	else
		return 0;
#endif
	return 0;
}
