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
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mm.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/sec_debug.h>
#include <linux/sec_batt.h>
#include "chub.h"
#include "ipc_chub.h"

#define DEBUG

#define NORMAL_SENSOR_STATE_K	0x3FEFF

#ifdef CONFIG_SEC_VIB_NOTIFIER
#include <linux/vibrator/sec_vibrator_notifier.h>
struct notifier_block vib_notif;
#endif

#ifdef CONFIG_PANEL_NOTIFY
struct notifier_block panel_notif;
#endif 

struct mutex shutdown_lock;
bool ssp_debug_time_flag;
static unsigned int current_hw_rev = 0;

static struct ois_sensor_interface ois_control;
static struct ois_sensor_interface ois_reset;

static struct ssp_data *p_ssp_data;
char *sensorhub_dump;
int sensorhub_dump_size;
int sensorhub_dump_type;
int sensorhub_dump_count;

int ois_fw_update_register(struct ois_sensor_interface *ois)
{
	if (ois->core)
		ois_control.core = ois->core;

	if (ois->ois_func)
		ois_control.ois_func = ois->ois_func;

	if (!ois->core || !ois->ois_func) {
		pr_info("%s - no ois struct\n", __func__);
		return ERROR;
	}

	return 0;
}
EXPORT_SYMBOL(ois_fw_update_register);

void ois_fw_update_unregister(void)
{
	ois_control.core = NULL;
	ois_control.ois_func = NULL;
}
EXPORT_SYMBOL(ois_fw_update_unregister);

int ois_reset_register(struct ois_sensor_interface *ois)
{
	if (ois->core)
		ois_reset.core = ois->core;

	if (ois->ois_func)
		ois_reset.ois_func = ois->ois_func;

	if (!ois->core || !ois->ois_func) {
		pr_info("%s - no ois struct\n", __func__);
		return ERROR;
	}

	return 0;
}
EXPORT_SYMBOL(ois_reset_register);

void ois_reset_unregister(void)
{
	ois_reset.core = NULL;
	ois_reset.ois_func = NULL;
}
EXPORT_SYMBOL(ois_reset_unregister);

static struct ssp_data *ssp_data_info;
void set_ssp_data_info(struct ssp_data *data)
{
	if (data != NULL)
		ssp_data_info = data;
	else
		pr_info("[SSP] %s : ssp data info is null\n", __func__);
}

void ssp_enable(struct ssp_data *data, bool enable)
{
	if (enable == !data->bSspShutdown)
		return;

	pr_info("[SSP] %s, new enable = %d, old enable = %d\n",
		__func__, enable, !data->bSspShutdown);

	if (enable && data->bSspShutdown)
		data->bSspShutdown = false;
	else if (!enable && !data->bSspShutdown)
		data->bSspShutdown = true;
}

u64 get_current_timestamp(void)
{
	u64 timestamp;
	struct timespec64 ts;

	ts = ktime_to_timespec64(ktime_get_boottime());
	timestamp = timespec64_to_ns(&ts);
	//ts.tv_sec * 1000000000ULL + ts.tv_nsec;
	return timestamp;
}

/*************************************************************************/
/* initialize sensor hub						 */
/*************************************************************************/

static void initialize_variable(struct ssp_data *data)
{
	int iSensorIndex;
	int i = 0;

	data->cameraGyroSyncMode = false;
	data->ts_stacked_cnt = 0;

	for (iSensorIndex = 0; iSensorIndex < SENSOR_MAX; iSensorIndex++) {
		data->adDelayBuf[iSensorIndex] = DEFAULT_POLLING_DELAY;
		data->batchLatencyBuf[iSensorIndex] = 0;
		data->batchOptBuf[iSensorIndex] = 0;
		data->aiCheckStatus[iSensorIndex] = INITIALIZATION_STATE;
		data->lastTimestamp[iSensorIndex] = 0;
		data->IsBypassMode[iSensorIndex] = 0;
		/* variables for conditional timestamp */
		data->first_sensor_data[iSensorIndex] = true;
	}

	atomic64_set(&data->aSensorEnable, 0);
	data->uSensorState = NORMAL_SENSOR_STATE_K;
	data->uFactoryProxAvg[0] = 0;
	data->uMagCntlRegData = 1;

	data->uResetCnt = 0;
	data->uTimeOutCnt = 0;
	data->uComFailCnt = 0;
	data->uIrqCnt = 0;

	data->bFirstRef = true;
	data->bSspShutdown = true;
	data->bProximityRawEnabled = false;
	data->bGeomagneticRawEnabled = false;
	data->bBarcodeEnabled = false;
	data->bAccelAlert = false;
	data->resetting = false;

	data->gyrocal.x = 0;
	data->gyrocal.y = 0;
	data->gyrocal.z = 0;

	data->magoffset.x = 0;
	data->magoffset.y = 0;
	data->magoffset.z = 0;

	data->iPressureCal = 0;

#ifdef CONFIG_SENSORS_SSP_PROX_FACTORYCAL
	data->uProxCanc = 0;
	data->uProxHiThresh = data->uProxHiThresh_default;
	data->uProxLoThresh = data->uProxLoThresh_default;
#endif
	data->uGyroDps = GYROSCOPE_DPS1000;

	data->mcu_device = NULL;
	data->acc_device = NULL;
	data->gyro_device = NULL;
	data->mag_device = NULL;
	data->prs_device = NULL;
	data->prox_device = NULL;
	data->light_device = NULL;
	data->ges_device = NULL;
	data->fcd_device = NULL;
	data->thermistor_device = NULL;

	INIT_LIST_HEAD(&data->pending_list);

	data->ssp_mcu_ready_wq =
		create_singlethread_workqueue("ssp_mcu_ready_wq");
	INIT_WORK(&data->work_ssp_mcu_ready, ssp_mcu_ready_work_func);

/*
	data->ssp_rx_wq = 
		create_singlethread_workqueue("ssp_rx_wq");
	INIT_WORK(&data->work_ssp_rx, ssp_recv_packet_func);
*/

	data->step_count_total = 0;
	data->sealevelpressure = 0;

	/* HIFI batching wakeup */
	data->resumeTimestamp = 0;
	data->bIsResumed = false;
	data->bIsReset = false;
	initialize_function_pointer(data);
	data->uNoRespSensorCnt = 0;
	data->errorCount = 0;
	data->mcuCrashedCnt = 0;
	data->pktErrCnt = 0;
	data->mcuAbnormal = false;
	data->IsMcuCrashed = false;
	data->intendedMcuReset = false;

	data->timestamp_factor = 5;
	ssp_debug_time_flag = false;
	data->dhrAccelScaleRange = 0;
	data->IsGyroselftest = false;
	data->IsVDIS_Enabled = false;
        data->IsAPsuspend = false;
        data->IsNoRespCnt = 0;
#ifdef CONFIG_PANEL_NOTIFY
	memset(&data->panel_event_data, 0, sizeof(struct panel_bl_event_data));
#endif

	data->ois_control = &ois_control;
	data->ois_reset = &ois_reset;

	data->svc_octa_change = false;
	data->mcu_pc = 0;
	data->mcu_lr = 0;
	sensorhub_dump = NULL;
	sensorhub_dump_size = 0;
	sensorhub_dump_type = 0;
	sensorhub_dump_count = 0;

	for (i = 0; i < SENSOR_MAX; i++)
		memset(&data->sensor_name[i], 0, sizeof(data->sensor_name[i]));
}

int initialize_mcu(struct ssp_data *data)
{
	int iRet = 0;
	pr_err("[SSP] initialize_variable+++++++++++++++++++++");
	clean_pending_list(data);

	iRet = get_chipid(data);
	pr_info("[SSP] MCU device ID = %d, reading ID = %d\n", DEVICE_ID, iRet);
	if (iRet != DEVICE_ID) {
		if (iRet < 0) {
			pr_err("[SSP]: %s - MCU is not working : 0x%x\n",
				__func__, iRet);
		} else {
			pr_err("[SSP]: %s - MCU identification failed\n",
				__func__);
			iRet = -ENODEV;
		}
		goto out;
	}

	iRet = set_ap_information(data);
	if (iRet < 0) {
		pr_err("[SSP] %s - set_ap_type failed\n", __func__);
		goto out;
	}
	iRet = set_sensor_position(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - set_sensor_position failed\n", __func__);
		goto out;
	}

	iRet = set_flip_cover_detector_status(data);

	if (iRet < 0) {
		pr_err("[SSP]: %s - set_flip_cover_detector_status failed\n", __func__);
		goto out;
	}

	data->uSensorState = get_sensor_scanning_info(data);
	if (data->uSensorState == 0) {
		pr_err("[SSP]: %s - get_sensor_scanning_info failed\n",
			__func__);
		iRet = ERROR;
		goto out;
	}

	if (initialize_magnetic_sensor(data) < 0)
		pr_err("[SSP]: %s - initialize magnetic sensor failed\n",
			__func__);

	if (initialize_thermistor_table(data) < 0)
		pr_err("[SSP]: %s - initialize thermistor table failed\n",
			__func__);

	if (initialize_light_sensor(data) < 0)
		pr_err("[SSP]: %s - initialize light sensor failed\n", __func__);

	if (set_prox_cal_to_ssp(data) < 0)
		pr_err("[SSP]: %s - sending proximity calibration data failed\n",
			__func__);

	data->uCurFirmRev = get_firmware_rev(data);
	pr_info("[SSP] MCU Firm Rev : New = %8u\n",
		data->uCurFirmRev);

	data->dhrAccelScaleRange = get_accel_range(data);
#ifdef CONFIG_PANEL_NOTIFY
	send_panel_information(&data->panel_event_data);
	send_ub_connected(data->ub_disabled);
#endif

	send_hall_ic_status(data->hall_ic_status);

	if(set_prox_dynamic_cal_to_ssp(data) < 0) {
		pr_err("[SSP]: %s - set dynamic cal flag failed\n", __func__);
	}

	if(set_prox_call_min_to_ssp(data) < 0) {
		pr_err("[SSP]: %s - set prox call min failed\n", __func__);
	}

	if(set_factory_binary_flag_to_ssp(data) < 0) {
		pr_err("[SSP]: %s - set factory binary flag failed\n", __func__);
	}

/* hoi: il dan mak a */
//	iRet = ssp_send_cmd(data, MSG2SSP_AP_MCU_DUMP_CHECK, 0);
	pr_err("[SSP] initialize_variable---------------------");	
	if (ois_control.ois_func != NULL) {
		ois_control.ois_func(ois_control.core);
		ois_control.ois_func = NULL;
	} 

out:
	return iRet;
}

/* HIFI Sensor */
void ssp_reset_batching_resources(struct ssp_data *data)
{
	u64 ts = get_current_timestamp();

	pr_err("[SSP_RST] reset triggered %lld\n", ts);
	data->ts_stacked_cnt = 0;
	data->resumeTimestamp = 0;
	data->bIsResumed = false;
	memset(data->ts_index_buffer, 0, sizeof(u64)*SENSOR_MAX);
	data->bIsReset = true;

	ts = get_current_timestamp();
	pr_err("[SSP_RST] reset finished %lld\n", ts);
}

static int ssp_parse_dt(struct device *dev, struct ssp_data *data)
{
	struct device_node *np = dev->of_node;
	int errorno = 0;

	if (!np) {
		pr_err("[SSP] NO dt node!!\n");
		return errorno;
	}

	if (of_property_read_u32(np, "ssp-acc-position", &data->accel_position))
		data->accel_position = 0;
	if (of_property_read_u32(np, "ssp-mag-position", &data->mag_position))
		data->mag_position = 0;

	pr_info("[SSP] acc-posi[%d] mag-posi[%d]\n",
		data->accel_position, data->mag_position);

	if (of_property_read_u32(np, "ssp-ap-type", &data->ap_type))
		data->ap_type = 0;
	pr_info("[SSP] ap-type = %d\n", data->ap_type);
	
	if (current_hw_rev != -1)
		data->ap_rev = current_hw_rev;
	pr_info("[SSP] ap-rev = %d\n", data->ap_rev);



	if (of_property_read_string(np, "ssp-light-position", &data->light_position))
		data->light_position = "0.0 0.0 0.0";
	
	pr_info("[SSP] light-position = %s\n", data->light_position);


	if (of_property_read_u32(np, "fcd-enable",
			&data->fcd_data.enable))
		data->fcd_data.enable = 0;

	data->fcd_data.enable = 1;

	if (of_property_read_u32(np, "fcd-axis",
			&data->fcd_data.axis_update))
		data->fcd_data.axis_update = 0;

	if (of_property_read_u32(np, "fcd-threshold",
			&data->fcd_data.threshold_update))
		data->fcd_data.threshold_update = 0;

	if (of_property_read_u8_array(np, "fcd-array",
		data->fcd_matrix, sizeof(data->fcd_matrix)))
		pr_err("no fcd-array, set as 0");

	pr_info("[SSP] FACTORY axis %d threshold %d fcd-enable %d\n",
		data->fcd_data.axis_update,
		data->fcd_data.threshold_update,
		data->fcd_data.enable);

	data->uProxAlertHiThresh = DEFAULT_PROX_ALERT_HIGH_THRESHOLD;

	/* magnetic matrix */
	if (of_property_read_u8_array(np, "ssp-mag-array",
		data->pdc_matrix, sizeof(data->pdc_matrix)))
		pr_err("no mag-array, set as 0");

	if (of_property_read_u16_array(np, "ssp-thermi-up",
		data->tempTable_up, ARRAY_SIZE(data->tempTable_up)))
		pr_err("no thermi up table, set as 0");

	if (of_property_read_u16_array(np, "ssp-thermi-sub",
		data->tempTable_sub, ARRAY_SIZE(data->tempTable_sub)))
		pr_err("no thermi sub table, set as 0");
	if (of_property_read_u32(np, "pressure-sw-offset", &data->sw_offset)) {
		data->sw_offset = 0;
		pr_err("no pressure-sw-offset, set as 0");
	}
	pr_info("[SSP] pressure-sw-offset %d \n", data->sw_offset);

	return errorno;
}


#ifdef CONFIG_SEC_VIB_NOTIFIER
void ssp_motor_work_func(struct work_struct *work)
{
	int iRet = 0;
	struct ssp_data *data = container_of(work,
					struct ssp_data, work_ssp_motor);

	iRet = send_motor_state(data);
	pr_info("[SSP] %s : Motor state %d, iRet %d\n", __func__, data->motor_state, iRet);
}

static int vib_notifier_callback(struct notifier_block *self, unsigned long event, void *data){
	struct vib_notifier_context *vib = (struct vib_notifier_context *)data;

	pr_info("[SSP] %s: %s, idx: %d timeout: %d\n", __func__, event? "ON" : "OFF",
					vib->index, vib->timeout);

	ssp_data_info->motor_state = event;
	if (event == 1) {
		if (ssp_data_info->ssp_motor_wq != NULL)
			queue_work(ssp_data_info->ssp_motor_wq, &ssp_data_info->work_ssp_motor);
	}

	pr_info("[SSP] %s : Motor state %d\n", __func__, ssp_data_info->motor_state);

	return 0;
}
#endif

#ifdef CONFIG_PANEL_NOTIFY
int send_panel_information(struct panel_bl_event_data *evdata){
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	int iRet = 0;

	//TODO: send brightness + aor_ratio information to sensorhub
        if (msg == NULL) {
                iRet = -ENOMEM;
                pr_err("[SSP] %s, failed to allocate memory for ssp_msg\n", __func__);
                return iRet;
        }
	msg->cmd = MSG2SSP_PANEL_INFORMATION;
	msg->length = sizeof(struct panel_bl_event_data);
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(sizeof(struct panel_bl_event_data), GFP_KERNEL);
	msg->free_buffer = 1;
	memcpy(msg->buffer, (u8 *)evdata, sizeof(struct panel_bl_event_data));

	iRet = ssp_spi_async(ssp_data_info, msg);

	if (iRet < 0)
		pr_err("[SSP] %s, failed to send panel information", __func__);
	return iRet;
}

int send_screen_mode_information(struct panel_screen_mode_data *evdata)
{
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	int iRet = 0;

	//TODO: send brightness + aor_ratio information to sensorhub
	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to allocate memory for ssp_msg\n", __func__);
		return iRet;
	}

	msg->cmd = MSG2SSP_MODE_INFORMATION;
	msg->length = sizeof(struct panel_screen_mode_data);
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(sizeof(struct panel_screen_mode_data), GFP_KERNEL);
	msg->free_buffer = 1;
	memcpy(msg->buffer, (u8 *)evdata, sizeof(struct panel_screen_mode_data));

	iRet = ssp_spi_async(ssp_data_info, msg);

	if (iRet < 0)
		pr_err("[SSP] %s, failed to send panel screen mode information", __func__);
	return iRet;
}

int send_ub_connected(bool ub_connected) {
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	int iRet = 0;
	bool buffer = ub_connected;

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to allocate memory for ssp_msg\n", __func__);
		return iRet;
	}

	msg->cmd = MSG2SSP_UB_CONNECTED;
	msg->length = sizeof(buffer);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (u8 *)&buffer;

	iRet = ssp_spi_async(ssp_data_info, msg);

	if (iRet < 0)
		pr_err("[SSP] %s, failed to send ub connected", __func__);
	return iRet;
}

static int panel_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	if (event == PANEL_EVENT_BL_CHANGED) {
		struct panel_bl_event_data *evdata = data;
		//pr_info("[SSP] %s PANEL_EVENT_BL_CHANGED %d %d\n", __func__, evdata->brightness, evdata->aor_ratio);
		// store these values for reset
		memcpy(&ssp_data_info->panel_event_data, evdata, sizeof(struct panel_bl_event_data));
		send_panel_information(evdata);
	} else if (event == PANEL_EVENT_UB_CON_CHANGED) {
		int i = *((char *)data);
		if(i != PANEL_EVENT_UB_CON_CONNECTED && i != PANEL_EVENT_UB_CON_DISCONNECTED){
			pr_info("[SSP] %s PANEL_EVENT_UB_CON_CHANGED, event errno(%d)\n", __func__, i);
		} else {
			ssp_data_info->ub_disabled = i;
			send_ub_connected(i);
			
			if (ssp_data_info->ub_disabled) {
				ssp_send_cmd(ssp_data_info, get_copr_status(ssp_data_info), 0);
			}
		}
	} else if (event == PANEL_EVENT_COPR_STATE_CHANGED) {
		struct panel_power_state_event_data evt_data = *((struct panel_power_state_event_data *)data);		
		pr_info("[SSP] %s PANEL_EVENT_COPR_STATE_CHANGED, event(%d)\n", __func__, evt_data.state);
		if(evt_data.state != PANEL_EVENT_COPR_ENABLED && evt_data.state != PANEL_EVENT_COPR_DISABLED){
			pr_info("[SSP] %s PANEL_EVENT_COPR_STATE_CHANGED, event errno(%d)\n", __func__, evt_data.state);
		} else {
			ssp_data_info->uLastAPState = evt_data.state == 0 ? MSG2SSP_AP_STATUS_SLEEP : MSG2SSP_AP_STATUS_WAKEUP;
			ssp_send_cmd(ssp_data_info, get_copr_status(ssp_data_info), 0);
		}
	} else if (event == PANEL_EVENT_SCREEN_MODE_CHANGED) {
		struct panel_screen_mode_data *evdata = data;
		memcpy(&ssp_data_info->panel_mode_data, evdata, sizeof(struct panel_screen_mode_data));
		send_screen_mode_information(evdata);
		pr_info("[SSP] %s panel screen mode %d %d", __func__, ssp_data_info->panel_mode_data.display_idx, ssp_data_info->panel_mode_data.mode);
	}

	return 0;
}
#endif

int send_hall_ic_status(bool enable) {
	struct ssp_msg *msg;
	int iRet = 0;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_HALL_IC_ON_OFF;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(1, GFP_KERNEL);

	msg->free_buffer = 1;
	msg->buffer[0] = enable;
	iRet = ssp_spi_async(ssp_data_info, msg);

	if (iRet != SUCCESS) {
	pr_err("[SSP]: %s - hall ic command, failed %d\n", __func__, iRet);
		return iRet;
	}

	pr_info("[SSP] %s HALL IC ON/OFF, %d enabled %d\n",
		__func__, iRet, enable);

	return iRet;
}

void ssp_timestamp_sync_work_func(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
					struct ssp_data, work_ssp_tiemstamp_sync);
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2AP_INST_TIMESTAMP_OFFSET;
	msg->length = sizeof(data->timestamp_offset);
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(sizeof(data->timestamp_offset), GFP_KERNEL);

	pr_info("handle_timestamp_sync: %lld\n", data->timestamp_offset);
	memcpy(msg->buffer, &(data->timestamp_offset), sizeof(data->timestamp_offset));

	ssp_spi_sync(data, msg, 1000);
	//pr_info("[SSP] %s : Motor state %d, iRet %d\n",__func__, data->motor_state, iRet);
}

static int ssp_probe(struct platform_device *pdev)
{
	struct ssp_data *data = kzalloc(sizeof(*data), GFP_KERNEL);
	int iRet = 0;

	pr_info("[SSP] %s, is called\n", __func__);

	p_ssp_data = data;
	p_ssp_data->pdev = pdev;

	if (pdev->dev.of_node) {
		iRet = ssp_parse_dt(&pdev->dev, data);
		if (iRet) {
			pr_err("[SSP]: %s - Failed to parse DT\n", __func__);
			goto err_setup;
		}
	}
	
	data->bProbeIsDone = false;

	//platform_set_drvdata(pdev, data);


	mutex_init(&data->comm_mutex);
	mutex_init(&data->pending_mutex);
	mutex_init(&data->enable_mutex);
	mutex_init(&data->ssp_enable_mutex);

	if (pdev->dev.of_node == NULL) {
		pr_err("[SSP] %s, function callback is null\n", __func__);
		iRet = -EIO;
		goto err_reset_null;
	}

	pr_info("\n#####################################################\n");

	initialize_variable(data);

	wake_lock_init(data->ssp_wake_lock, NULL, "ssp_wake_lock");
	wake_lock_init(data->ssp_batch_wake_lock, NULL, "ssp_batch_wake_lock");
	wake_lock_init(data->ssp_comm_wake_lock, NULL, "ssp_comm_wake_lock");

	iRet = initialize_input_dev(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - could not create input device\n", __func__);
		goto err_input_register_device;
	}

	iRet = initialize_debug_timer(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}

	iRet = initialize_sysfs(data);

	if (iRet < 0) {
		pr_err("[SSP]: %s - could not create sysfs\n", __func__);
		goto err_sysfs_create;
	}

	/* init sensorhub device */
	iRet = ssp_sensorhub_initialize(data);
	if (iRet < 0) {
		pr_err("%s: ssp_sensorhub_initialize err(%d)\n",
			__func__, iRet);
		ssp_sensorhub_remove(data);
	}

	/* HIFI Sensor + Batch Support */
	mutex_init(&data->batch_events_lock);

	data->batch_wq = create_singlethread_workqueue("ssp_batch_wq");
	if (!data->batch_wq) {
		iRet = -1;
		pr_err("[SSP]: %s - could not create batch workqueue\n",
			__func__);
		goto err_create_batch_workqueue;
	}

	data->ts_stacked_cnt = 0;

	pr_info("[SSP]: %s - probe success!\n", __func__);

	enable_debug_timer(data);

	data->bProbeIsDone = true;
	iRet = 0;
	mutex_init(&shutdown_lock);
	set_ssp_data_info(data);


#ifdef CONFIG_SEC_VIB_NOTIFIER
	data->ssp_motor_wq =
		create_singlethread_workqueue("ssp_motor_wq");
	if (!data->ssp_motor_wq) {
		iRet = -1;
		pr_err("[SSP]: %s - could not create motor workqueue\n",
			__func__);
		goto err_create_motor_workqueue;
	}

	INIT_WORK(&data->work_ssp_motor, ssp_motor_work_func);

	pr_info("[SSP]: %s motor notifier set!", __func__);
	vib_notif.notifier_call = vib_notifier_callback;
	iRet = sec_vib_notifier_register(&vib_notif);
	if (iRet) {
		pr_err("[SSP]: %s - fail to register vib_notifier_callback\n", __func__);
	}
#endif

#ifdef CONFIG_PANEL_NOTIFY
	pr_info("[SSP]: %s panel notifier set!", __func__);
	panel_notif.notifier_call = panel_notifier_callback;
	iRet = panel_notifier_register(&panel_notif);
	if (iRet) {
		pr_err("[SSP]: %s - fail to register panel_notifier_callback\n", __func__);
	}
#endif


	INIT_DELAYED_WORK(&data->work_ssp_tiemstamp_sync, ssp_timestamp_sync_work_func);

	data->prox_call_min = -1;

	goto exit;

#ifdef CONFIG_SEC_VIB_NOTIFIER
err_create_motor_workqueue:
	destroy_workqueue(data->ssp_motor_wq);
#endif
/* Test PIN for Camera - Gyro Sync */
err_create_batch_workqueue:
	destroy_workqueue(data->batch_wq);
	mutex_destroy(&data->batch_events_lock);
	remove_sysfs(data);
err_sysfs_create:
	destroy_workqueue(data->debug_wq);
err_create_workqueue:
err_input_register_device:
	wake_lock_destroy(data->ssp_batch_wake_lock);
	wake_lock_destroy(data->ssp_comm_wake_lock);
	wake_lock_destroy(data->ssp_wake_lock);
err_reset_null:
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
	mutex_destroy(&data->enable_mutex);
	mutex_destroy(&data->ssp_enable_mutex);
err_setup:
	//kfree(data);
	data->bProbeIsDone = false;
	pr_err("[SSP] %s, probe failed!\n", __func__);
exit:
	pr_info("#####################################################\n\n");
	return iRet;
}

static void ssp_shutdown(struct platform_device *pdev)
{
	struct ssp_data *data = platform_get_drvdata(pdev);

	if (data->bProbeIsDone == false)
		return;


	func_dbg();

	disable_debug_timer(data);

	mutex_lock(&data->ssp_enable_mutex);
	ssp_enable(data, false);
	clean_pending_list(data);
	mutex_unlock(&data->ssp_enable_mutex);


	mutex_lock(&shutdown_lock);

	cancel_work_sync(&data->work_ssp_mcu_ready);
	destroy_workqueue(data->ssp_mcu_ready_wq);
	destroy_workqueue(data->batch_wq); 
	mutex_destroy(&data->batch_events_lock);

#ifdef CONFIG_SEC_VIB_NOTIFIER
	cancel_work_sync(&data->work_ssp_motor);
	destroy_workqueue(data->ssp_motor_wq);

	data->ssp_motor_wq = NULL;
#endif

	mutex_unlock(&shutdown_lock);
	remove_shub_dump();
	remove_sysfs(data);

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	ssp_sensorhub_remove(data);
#endif

	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
	destroy_workqueue(data->debug_wq);
	wake_lock_destroy(data->ssp_comm_wake_lock);
	wake_lock_destroy(data->ssp_wake_lock);
	wake_lock_destroy(data->ssp_batch_wake_lock);
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
	mutex_destroy(&data->enable_mutex);
	mutex_destroy(&data->ssp_enable_mutex);

#ifdef CONFIG_SEC_VIB_NOTIFIER
	sec_vib_notifier_unregister(&vib_notif);
#endif

#ifdef CONFIG_PANEL_NOTIFY
	panel_notifier_unregister(&panel_notif);
#endif

	pr_info("[SSP] %s done\n", __func__);
}

char get_copr_status(struct ssp_data *data){
	return !data->ub_disabled && data->uLastAPState == MSG2SSP_AP_STATUS_WAKEUP ? 
		MSG2SSP_AP_STATUS_WAKEUP : MSG2SSP_AP_STATUS_SLEEP;
}

static int ssp_suspend(struct device *dev)
{
	//struct spi_device *spi = to_spi_device(dev);
	//struct ssp_data *data = dev_get_drvdata(dev);

	struct ssp_data *data = p_ssp_data;

	func_dbg();
/*
	if (ssp_send_cmd(data, MSG2SSP_AP_STATUS_SUSPEND, 0) != SUCCESS)
		pr_err("[SSP]: %s MSG2SSP_AP_STATUS_SUSPEND failed\n",
			__func__);
*/
	data->uLastResumeState = MSG2SSP_AP_STATUS_SUSPEND;
	disable_debug_timer(data);

    data->IsAPsuspend = true;

	return 0;
}

static int ssp_resume(struct device *dev)
{
	struct ssp_data *data = p_ssp_data;

	func_dbg();
	enable_debug_timer(data);
	data->resumeTimestamp = get_current_timestamp();
	data->bIsResumed = true;
/*
	if (ssp_send_cmd(data, MSG2SSP_AP_STATUS_RESUME, 0) != SUCCESS)
		pr_err("[SSP]: %s MSG2SSP_AP_STATUS_RESUME failed\n",
			__func__);
*/
	data->uLastResumeState = MSG2SSP_AP_STATUS_RESUME;
    data->IsAPsuspend = false;

	return 0;
}

int ssp_device_probe(struct platform_device *pdev) {
	func_dbg();
	//p_ssp_data = kzalloc(sizeof(*data), GFP_KERNEL);
	ssp_probe(pdev);
    return 1;
	//return 1;
}

void ssp_device_remove(struct platform_device *pdev) {
	func_dbg();
	ssp_shutdown(pdev);
}

int ssp_device_suspend(struct device *dev) {
	func_dbg();
    return ssp_suspend(dev);
}

void ssp_device_resume(struct device *dev) {
	func_dbg();
	
    ssp_resume(dev);
}

void ssp_platform_init(void *p_data) {
    return;
}

void ssp_handle_recv_packet(char *packet, int packet_len) {
	//func_dbg();
	ssp_recv_packet(p_ssp_data, packet, packet_len);
	//queue_work(p_ssp_data->ssp_rx_wq, &p_ssp_data->work_ssp_rx);
}

void ssp_platform_start_refrsh_task(void) {
	func_dbg();
	ssp_on_mcu_ready(p_ssp_data, true);
    return;
}

void ssp_set_firmware_name(const char *fw_name) {
	struct ssp_data *data = p_ssp_data;
	pr_err("%s: %s\n", __func__, fw_name);
	strcpy(data->fw_name, fw_name);
    return;
}

#define DUMP_FILE_PATH "/data/vendor/sensorhub/dump.log"
//#define DUMP_TYPE_BASE 100


static ssize_t current_sram_dump_read(struct file *file, struct kobject *kobj, struct bin_attribute *battr, char *buf,
			      loff_t off, size_t size)
{
	memcpy_fromio(buf, battr->private + off, size);
	return size;
}

BIN_ATTR_RO(current_sram_dump, 0);

static int create_dump_bin_file(void)
{
	int ret;

	sensorhub_dump = (char *)kvzalloc(sensorhub_dump_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(sensorhub_dump)) {
		pr_err("[SSP] %s: memory alloc failed\n", __func__);
		return -ENOMEM;
	}

	pr_info("[SSP] %s dump size %d\n", __func__, sensorhub_dump_size);
	bin_attr_current_sram_dump.size = sensorhub_dump_size;
	bin_attr_current_sram_dump.private = sensorhub_dump;

	ret = device_create_bin_file(p_ssp_data->mcu_device, &bin_attr_current_sram_dump);

	if (ret < 0) {
		pr_err("[SSP] %s: Failed to create file: %s %d\n",
				__func__, bin_attr_current_sram_dump.attr.name, ret);
	}

	if (ret < 0) {
		kvfree(sensorhub_dump);
		sensorhub_dump_size = 0;
	}

	return ret;
}

void write_shub_dump_file(char *dump, int dumpsize)
{
	struct ssp_data *data = p_ssp_data;
	int ret = 0;

	if (!dump) {
		pr_err("[SSP] %s: dump is NULL\n", __func__);
		return;
	} else if (sensorhub_dump_size == 0) {
		sensorhub_dump_size = dumpsize;
		ret = create_dump_bin_file();
		if (ret < 0)
			return;
	} else if (dumpsize != sensorhub_dump_size) {
		pr_err("[SSP] %s: dump size is wrong %d(%d)",
			__func__, dumpsize, sensorhub_dump_size);
		return;
	}

	memcpy_fromio(sensorhub_dump, dump, sensorhub_dump_size);
	file_manager_cmd(data, 4);
	pr_info("[SSP] %s file_manager_cmd\n", __func__);
}


void ssp_dump_write_file(void)
{
	struct ssp_data *data = p_ssp_data;

	file_manager_cmd(data, 3);
	return;
}

void sensorhub_save_ram_dump(void)
{
	struct ssp_contexthub_dump ram_dump;

	ram_dump.reason = 0;
	ram_dump.dump = ipc_get_base(IPC_REG_DUMP);
	ram_dump.size = ipc_get_chub_mem_size();
	pr_info("[SSP] %s ram_dump_size %d\n", __func__, ram_dump.size);
	write_shub_dump_file(ram_dump.dump, ram_dump.size);
}

void remove_shub_dump(void)
{
	if (ZERO_OR_NULL_PTR(sensorhub_dump))
		return;

	kvfree(sensorhub_dump);
	pr_info("[SSP] %s freememory sensorhub_dump\n", __func__);
	device_remove_bin_file(p_ssp_data->mcu_device, &bin_attr_current_sram_dump);
}