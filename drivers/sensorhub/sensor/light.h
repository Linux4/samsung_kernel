#ifndef __SHUB_LIGHT_H_
#define __SHUB_LIGHT_H_

#include <linux/types.h>
#include <linux/device.h>

#define LIGHT_COEF_SIZE 7

struct light_event {
	u32 lux;
	s32 cct;
	u32 raw_lux;
	u32 r;
	u32 g;
	u32 b;
	u32 w;
	u16 a_time;
	u16 a_gain;
	u32 brightness;
} __attribute__((__packed__));

struct light_cct_event {
	u32 lux;
	s32 cct;
	u32 raw_lux;
	u16 roi;
	u32 r;
	u32 g;
	u32 b;
	u32 w;
	u16 a_time;
	u16 a_gain;
} __attribute__((__packed__));

struct light_seamless_event {
	u32 lux;
} __attribute__((__packed__));

struct light_ir_event {
	u32 ir;
	u32 r;
	u32 g;
	u32 b;
	u32 w;
	u16 a_time;
	u16 a_gain;
} __attribute__((__packed__));

struct light_data {
	int *light_coef;
	int light_log_cnt;
	int brightness;
	int last_brightness_level;
	int brightness_array_len;
	u32 *brightness_array;
	int raw_data_size;
	bool ddi_support;
};

void set_light_ddi_support(uint32_t ddi_support);

#endif /* __SHUB_LIGHT_H_ */
