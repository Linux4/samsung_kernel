#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "../comm/shub_comm.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensorhub/shub_device.h"
#include "../sensorhub/shub_firmware.h"
#include "../utility/shub_utility.h"

static struct timer_list debug_timer;
static struct workqueue_struct *debug_wq;
static struct work_struct work_debug;

static int check_noevent_reset_cnt;

#define SHUB_DEBUG_TIMER_SEC    (5 * HZ)

/*
 * check_sensor_event
 *  - return true : there is no accel or light sensor event over 5sec when sensor is registered
 */
static void check_no_event(void)
{
	u64 timestamp = get_current_timestamp();
	int type, ret;
	struct shub_sensor *sensor;
	struct sensor_event *event;
	bool check_reset = false;
	char buffer[9] = {0,};

	if (check_noevent_reset_cnt >= 0 && check_noevent_reset_cnt == get_reset_count())
		check_reset = true;

	check_noevent_reset_cnt = -1;

	for (type = 0 ; type < SENSOR_TYPE_LEGACY_MAX ; type++) {
		sensor = get_sensor(type);
		if (!sensor)
			continue;

		event = get_sensor_event(type);
		if (sensor->report_mode_continuous && sensor->enabled && sensor->max_report_latency == 0 &&
		    sensor->enable_timestamp + 5000000000ULL < timestamp &&
		    event->received_timestamp + 5000000000ULL < timestamp) {
			shub_infof("sensor(%d) %lld(%lld), cur = %lld", type, event->received_timestamp,
				   event->timestamp, timestamp);

			if (check_reset) {
				shub_errf("no event, no sensorhub reset");
				reset_mcu(RESET_TYPE_KERNEL_NO_EVENT);
				break;
			}

			check_noevent_reset_cnt = get_reset_count();

			buffer[0] = type;
			memcpy(&buffer[1], &(sensor->sampling_period), sizeof(sensor->sampling_period));
			ret = shub_send_command(CMD_SETVALUE, TYPE_MCU, NO_EVENT_CHECK, buffer, sizeof(buffer));
			if (ret < 0)
				shub_errf("type %d comm failed ret %d", type, ret);
			else
				break;
		}
	}
}

static void debug_work_func(struct work_struct *work)
{
	int type;
	uint64_t en_state = 0;
	struct shub_data_t *data = get_shub_data();

	en_state = get_sensors_legacy_enable_state();

	for (type = 0; type < SENSOR_TYPE_MAX; type++) {
		if (get_sensor_enabled(type))
			print_sensor_debug(type);
	}

	shub_infof("FW(%d):%u, Sensor state: 0x%llx, En: 0x%llx, Reset cnt: %d[%d : C %u(%u, %u), N %u, %u]",
		get_firmware_type(), get_firmware_rev(),
		get_sensors_legacy_probe_state(), en_state, data->cnt_reset, data->cnt_shub_reset[RESET_TYPE_MAX],
		data->cnt_shub_reset[RESET_TYPE_KERNEL_COM_FAIL], get_cnt_comm_fail(), get_cnt_timeout(),
		data->cnt_shub_reset[RESET_TYPE_KERNEL_NO_EVENT], data->cnt_shub_reset[RESET_TYPE_HUB_NO_EVENT]);

	if (is_shub_working())
		check_no_event();
}

static void debug_timer_func(struct timer_list *timer)
{
	queue_work(debug_wq, &work_debug);
	mod_timer(&debug_timer, round_jiffies_up(jiffies + SHUB_DEBUG_TIMER_SEC));
}

int init_shub_debug(void)
{
	timer_setup(&debug_timer, debug_timer_func, 0);
	debug_wq = create_singlethread_workqueue("shub_debug_wq");
	if (!debug_wq)
		return -ENOMEM;

	INIT_WORK(&work_debug, debug_work_func);

	return 0;
}

void exit_shub_debug(void)
{
	del_timer(&debug_timer);
	cancel_work_sync(&work_debug);
	destroy_workqueue(debug_wq);
}

void enable_debug_timer(void)
{
	mod_timer(&debug_timer, round_jiffies_up(jiffies + SHUB_DEBUG_TIMER_SEC));
}

void disable_debug_timer(void)
{
	del_timer_sync(&debug_timer);
	cancel_work_sync(&work_debug);
}

int print_mcu_debug(char *dataframe, int *index, int frame_len)
{
	u16 length = 0;
	int cur = *index;

	memcpy(&length, dataframe + *index, 1);
	*index += 1;

	if (length > frame_len - *index || length <= 0) {
		shub_infof("[M] invalid debug length(%u/%d/%d)", length, frame_len, cur);
		return -1;
	}

	shub_info("[M] %s", &dataframe[*index]);
	*index += length;
	return 0;
}

#define SHUB_LOG_MAX_BYTE 200
#define LOG_UNIT_SIZE	 3
void print_dataframe(char *dataframe, int frame_len)
{
	char raw_data[SHUB_LOG_MAX_BYTE * LOG_UNIT_SIZE + 1];
	int i = 0, cur = 0, size;
	int len = sizeof(raw_data);

	while ((frame_len - cur) > 0) {
		int pr_size = ((frame_len - cur) > SHUB_LOG_MAX_BYTE) ? SHUB_LOG_MAX_BYTE : (frame_len - cur);

		memset(raw_data, 0, sizeof(raw_data));
		for (i = 0, size = 0; i < pr_size; i++)
			size += snprintf(raw_data + size, len - size, "%02x ", *(dataframe + cur++));

		shub_info("(%d/%d) %s", cur, frame_len, raw_data);
	}
}

int print_system_info(char *dataframe, int *index)
{
	struct sensor_debug_info {
		uint8_t uid;
		uint8_t total_count;
		uint8_t ext_client;
		int32_t ext_sampling;
		int32_t ext_report;
		int32_t fastest_sampling;
	};

	struct base_timestamp {
		uint64_t kernel_base;
		uint64_t hub_base;
	};

	struct utc_time {
		int8_t nHour;
		int8_t nMinute;
		int8_t nSecond;
		int16_t nMilliSecond;
	};

	struct system_debug_info {
		int32_t version;
		int32_t rate;
		int8_t ap_state;
		struct utc_time time;
		struct base_timestamp timestamp;
	};
	//===================================================//
	struct sensor_debug_info *info = 0;
	struct system_debug_info *s_info = 0;
	int i;
	int count = *dataframe;

	++dataframe;
	*index += (1 + sizeof(struct sensor_debug_info) * count + sizeof(struct system_debug_info));

	shub_info("==system info ===");
	for (i = 0; i < count; ++i) {
		info = (struct sensor_debug_info *)dataframe;
		shub_info("id(%d), total(%d), external(%d), e_sampling(%d), e_report(%d), fastest(%d)", info->uid,
			  info->total_count, info->ext_client, info->ext_sampling, info->ext_report,
			  info->fastest_sampling);
		dataframe += sizeof(struct sensor_debug_info);
	}

	s_info = (struct system_debug_info *)dataframe;
	shub_info("version(%d), rate(%d), ap_state(%s), time(%d:%d:%d.%d), base_ts_k(%lld), base_ts_hub(%lld)",
		  s_info->version, s_info->rate, s_info->ap_state == 0 ? "run" : "suspend", s_info->time.nHour,
		  s_info->time.nMinute, s_info->time.nSecond, s_info->time.nMilliSecond, s_info->timestamp.kernel_base,
		  s_info->timestamp.hub_base);

	return 0;
}
