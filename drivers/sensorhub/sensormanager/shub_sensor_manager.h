#ifndef __SENSOR_MANAGER_H_
#define __SENSOR_MANAGER_H_

#include "shub_sensor_type.h"

#include <linux/kernel.h>
#include <linux/miscdevice.h>

struct shub_sensor;

struct sensor_manager_t {
/*
 *	index 0 : for sensor legacy hal sensors, type < SENSOR_TYPE_LEGACY_MAX
 *	index 2,3 : for scontext sensors, type > SENSOR_TYPE_LEGACY_MAX
 */
	uint64_t sensor_probe_state[3];
	struct shub_sensor *sensor_list[SENSOR_TYPE_MAX];
	struct sensor_spec_t *sensor_spec;
	bool is_fs_ready;
};

int init_sensor_manager(struct device *dev);
void exit_sensor_manager(struct device *dev);

int enable_sensor(int type, char *buf, int buf_len);
int disable_sensor(int type, char *buf, int buf_len);
int batch_sensor(int type, uint32_t sampling_period, uint32_t max_report_latency);
int flush_sensor(int type);
int inject_sensor_additional_data(int type, char *buf, int buf_len);

void print_sensor_debug(int type);

// int report_sensor_event(char* buf); /* parse_sensor_event ? sensor_data? read?*/
int parsing_bypass_data(char *dataframe, int *index, int frame_len);
int parsing_meta_data(char *dataframe, int *index); /*meta event?*/
int parsing_scontext_data(char *dataframe, int *index);

int open_sensors_calibration(void);
int sync_sensors_attribute(void); /* sensor hub is ready or reset*/
int refresh_sensors(struct device *dev);

struct shub_sensor *get_sensor(int type);
struct sensor_event *get_sensor_event(int type);

uint64_t get_sensors_legacy_probe_state(void);
uint64_t get_sensors_legacy_enable_state(void);
int get_sensors_scontext_probe_state(uint64_t *buf);

bool get_sensor_probe_state(int type);
bool get_sensor_enabled(int type);
int get_sensor_spec(char *buf);

void fs_ready_cb(void);

void set_sensor_probe_state(uint64_t *buffer);

#endif /* __SENSOR_MANAGER_H_ */
