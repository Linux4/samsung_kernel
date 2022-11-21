/*
 *  HID driver for N-Trig touchscreens
 *
 *  Copyright (c) 2011 N-TRIG
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef _NTRIG_DISPATCHER_H
#define _NTRIG_DISPATCHER_H

#include <linux/input.h>
#include <linux/kobject.h>
#include <linux/device.h>

#include <linux/version.h>
/* Use type B (tracked contacts) MT reports. Comment-out to use type A reports.
 * Only for Android ICS or Linux versions that support type B MT reports
 */
#define MT_REPORT_TYPE_B 1 //1

/* The following macros are used to rotate the pen & touch events 90 degrees
 * counter-clockwise, which was needed for several devices.
 * ROTATE_ANDROID rotates the events sent to the Android framework, while
 * ROTATE_DIRECT_EVENTS rotates the events of the direct-events driver. Each
 * can be enabled separately.
 */
/* #define ROTATE_ANDROID */
/* #define ROTATE_DIRECT_EVENTS */

#define TOOL_PEN_INRANGE	0x00
#define TOOL_PEN_TIP		BTN_TOUCH
#define TOOL_PEN_SIDE_BTN	BTN_RIGHT

/** the range and fixed value we report for
 *  ABS_MT_TOUCH_MAJOR - signifies pressure in multi-touch.
 *  We use a ratio of 0.16 which is arbitrary value taken
 *  from some tablet. Note the n-trig sensor does not support
 *  pressure for fingers. It does report contact size (dx,dy)
 *  which we use for WIDTH_MAJOR, WIDTH_MINOR and ORIENTATION */
#define ABS_MT_TOUCH_MAJOR_MAX		100
#define ABS_MT_TOUCH_MAJOR_VAL		16
/** the value we report for ABS_MT_WIDTH_MAJOR and
 *  ABS_MT_WIDTH_MINOR - we use the smallest of the sensor
 *  axes */
#define ABS_MT_WIDTH_MAX			7200
/** maximum value for ABS_MT_TRACKING_ID - must be 32 or less
 *  to match android framework requirements. Set it to match
 *  USB_HID_MAX_FINGERS_COUNT_MAXVALUE in MTMTrackInterface.h
 *  (SensorInterfaceTrackCore) - this is the peak number
 *  of fingers that can be reported by tracklib at the same
 *  time */
#define ABS_MT_TRACKING_ID_MAX			21
/******************************************************************************/

/**
 * Bit Flag Operations
 */
int	is_pen_in_range(__u8 flag);
int	is_pen_tip_switch(__u8 flag);
int	is_pen_barrel_switch(__u8 flag);
int	is_pen_invert(__u8 flag);
int	is_pen_eraser(__u8 flag);

int	is_st_in_range(__u8 flag);
int	is_st_tip_switch(__u8 flag);
int	is_st_touch_valid(__u8 flag);
void set_st_touch_valid(__u8 *flag);
void set_st_touch_invalid(__u8 *flag);

int	is_mt_in_range(__u8 flag);
int	is_mt_tip_switch(__u8 flag);
int	is_mt_touch_valid(__u8 flag);
void set_mt_touch_valid(__u8 *flag);
void set_mt_touch_invalid(__u8 *flag);

/******************************************************************************/

/**
 * defines
 */
struct _ntrig_bus_device;
struct _ntrig_dev_ncp_func;

/* struct for counter in driver */
struct _ntrig_counter {
	char *name;
	unsigned int count;
};

typedef int (*message_callback)(void *buf);
typedef int (*data_send)(void *input_device, void *buf);
typedef int (*read_dev)(void *dev, char *out_buf, size_t count);
typedef int (*write_dev)(void *dev, const char *in_buf, short msg_len);
typedef int (*write_dev_hid)(void *dev, uint8_t cmd, const char *in_buf,
	short msg_len);
typedef int (*config_callback)(void *buf, int req_type);
typedef int (*read_counters_callback)(struct _ntrig_counter **counters_list,
	int *length);
typedef void (*reset_counters_callback)(void);

/*
 * struct @ntrig_bus_device - defines api to exchange data between dispatcher
 * and bus don't initialize APIs directly, call @reg_bus_driver
 */

struct _ntrig_bus_device {
/* Setup data, please set feilds before register call */
	__u16 logical_min_x;
	__u16 logical_max_x;
	__u16 logical_min_y;
	__u16 logical_max_y;
	__u16 pressure_min;
	__u16 pressure_max;
	__u16 touch_width;
	__u8 is_touch_set;
	const char *phys;
	const char *name;

/* Callback function, don't setup manually */
	message_callback	on_message;
};

/*
 * struct @ntrig_device_io - defines api to exchange data between dispatcher
 * and bus don't initialize APIs directly, call @RegNtrigDispatcher
 */
struct	_ntrig_dev_ncp_func {
	void *dev;
	read_dev read;		/* for RAW only (irrelevant for HID) */
	write_dev write;	/* for RAW only (irrelevant for HID) */
	read_counters_callback read_counters;
	reset_counters_callback reset_counters;
};

/*
 * struct @ntrig_dev - defines api to exchange data between dispatcher and
 * bus don't initialize APIs directly, call @RegNtrigDispatcher
 *//*KOBJ_NAME_LEN	BUS_ID_SIZE*/
struct _ntrig_dev {
	char bus_id[21];
	__u8 bus_type;
	/* Touch screen border - accounting for virtual keys, if any */
	int x_min;
	int x_max;
	int y_min;
	int y_max;
/**
 * Objects, don't setup manually
 */
	/* low priority objects */
#ifndef MT_REPORT_TYPE_B
	struct input_dev *single_touch_device;
#endif
	struct input_dev *multi_touch_device;
	int is_allocated_externally;
	read_counters_callback read_counters;
	reset_counters_callback reset_counters;
	void *dev;
	read_dev read_ncp;
	write_dev write_ncp;
};

/**
 * External Interface API
 */

/* Use this API to obtain ntrig_bus_device object, don't allocate manually */
int allocate_device(struct _ntrig_bus_device **dev);

/* Use this API to register ntrig_bus_device object before dispatching
 * messages
 */
__u8 RegNtrigDispatcher(int dev_type, char *bus_id,
	struct _ntrig_dev_ncp_func *);

/* Use this API to unregister ntrig_bus_device object before remove it */
void UnregNtrigDispatcher(void *dev, __u8 sensor_id, int dev_type,
	char *bus_id);

/* Use this API to remove ntrig_bus_device object from memory */
int remove_device(struct _ntrig_bus_device **dev);

#ifndef MT_REPORT_TYPE_B

/* Use this API to create single touch event queue for bus driver */
int create_single_touch(struct _ntrig_bus_device *dev, __u8 sensor_id);

/* Use this API to release single touch event queue for bus driver */
int release_single_touch(__u8 sensor_id);

/* Use this API if single touch event queue already allocated by bus driver */
int attach_single_touch(__u8 sensor_id, struct input_dev *input_device);

/* Use this API to check if single touch input device is attached to device */
bool check_single_touch(__u8 sensor_id);

#endif	/* !MT_REPORT_TYPE_B */

/* Use this API to create multi touch event queue for bus driver */
int create_multi_touch(struct _ntrig_bus_device *dev, __u8 sensor_id);

/* Use this API to release multi touch event queue for bus driver */
int release_multi_touch(__u8 sensor_id);

/* Use this API if multi touch event queue already allocated by bus driver */
int attach_multi_touch(__u8 sensor_id, struct input_dev *input_device);

/* Use this API to check if multi touch input device is attached to device */
bool check_multi_touch(__u8 sensor_id);

/* Use this API to send data from BUS  */
int WriteHIDNTRIG(void *buf);

/* For ncp driver: Write data from ncp file to device.
 * NCP message to dispatcher:
 *	Fields Index	Fields Description
 *      0           Sensor ID
 *      1           NCP Command (RAW/ HID)
 *      2-3         Length(Payload)
 *      4           Payload
 */
int write_ncp_to_device(void *buf);
/* Send from device to user application (ncp file driver) */
int read_ncp_from_device(void *buf, size_t count);

/* Setup sysfs for sysfs messages */
int setup_config_dispatcher(config_callback *read_config_dispatcher,
	config_callback *write_config_dispatcher);
int setup_get_bus_interface(message_callback *read_get_bus_interface,
	message_callback *write_get_bus_interface);
int setup_config_counters(read_counters_callback *get_counters_local,
	reset_counters_callback *reset_counters_loacl);

/* Setup direct-event messages */
int setup_direct_events(message_callback push_to_direct_events);

#endif /* _NTRIG_DISPATCHER_H */
