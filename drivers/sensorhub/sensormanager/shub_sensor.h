#ifndef __SHUB_SENSOR_H_
#define __SHUB_SENSOR_H_

#include "shub_sensor_type.h"
#include "shub_vendor_type.h"

#include <linux/rtc.h>

#define SENSOR_NAME_MAX 40

struct sensor_event {
	u64 received_timestamp;
	u64 timestamp;
	u8 *value;
};

struct meta_event {
	s32 what;
	s32 sensor;
} __attribute__((__packed__));

struct sensor_spec_t {
	uint8_t uid;
	uint8_t name[15];
	uint8_t vendor;
	uint16_t version;
	uint8_t is_wake_up;
	int32_t min_delay;
	uint32_t max_delay;
	uint16_t max_fifo;
	uint16_t reserved_fifo;
	float resolution;
	float max_range;
	float power;
} __attribute__((__packed__));

struct sensor_funcs {
	int (*sync_status)(void); /* this is called when sensorhub ready to work or reset */
	int (*enable)(void);
	int (*disable)(void);
	int (*batch)(int, int);
	void (*report_event)(void);
	int (*inject_additional_data)(char *, int);
	void (*print_debug)(void);
	int (*parsing_data)(char *, int *);
	int (*set_position)(int);
	int (*get_position)(void);
	int (*init_chipset)(char *, char *);
	int (*open_calibration_file)(void);
	void (*get_sensor_value)(char *, int *, struct sensor_event *);
};

struct shub_sensor {
	int type;
	char name[SENSOR_NAME_MAX];
	char chipset_name[15];
	char vendor[15];
	bool report_mode_continuous;

	bool enabled;
	int enabled_cnt;
	struct mutex enabled_mutex;

	uint32_t sampling_period;
	uint32_t max_report_latency;

	int receive_event_size;
	int report_event_size;

	u64 enable_timestamp;
	u64 disable_timestamp;
	struct rtc_time enable_time;
	struct rtc_time disable_time;

	struct sensor_event event_buffer;

	void *data;
	struct sensor_funcs *funcs;
};

typedef void (*init_sensor)(bool en);

#endif /* __SHUB_SENSOR_H_ */