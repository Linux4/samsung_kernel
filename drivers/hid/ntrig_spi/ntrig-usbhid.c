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

#include <linux/version.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>

#include "typedef-ntrig.h"
#include "ntrig-common.h"
#include "ntrig-dispatcher.h"
#include "ntrig-mod-shared.h"
/******************************************************************************/

#define FEATURE_128_BITS                    0x01
#define FEATURE_256_BITS                    0x02
#define FEATURE_512_BITS                    0x03
#define FEATURE_2048_BITS                   0x04
#define FEATURE_4096_BITS                   0x05
#define INPUT_NCP_REPORT_ID_MIN 6
#define INPUT_NCP_REPORT_ID_MAX 13

#ifdef NO_ID_G4_SHIFT
	#define FEATURE_SHIFT_G4					0
#else
	#define FEATURE_SHIFT_G4					40
#endif

struct ntrig_usbhid_data;

/**
 * USB-HID interface API
 */
static int ntrig_usbhid_input_mapping(struct hid_device *hdev,
	struct hid_input *hi, struct hid_field *field, struct hid_usage *usage,
	unsigned long **bit, int *max);
static int ntrig_usbhid_input_mapped(struct hid_device *hdev,
	struct hid_input *hi, struct hid_field *field, struct hid_usage *usage,
	unsigned long **bit, int *max);
static int ntrig_usbhid_event(struct hid_device *hid, struct hid_field *field,
	struct hid_usage *usage, __s32 value);
static int  ntrig_usbhid_probe(struct hid_device *hdev,
	const struct hid_device_id *id);
static void ntrig_usbhid_remove(struct hid_device *hdev);
static int  __init ntrig_usbhid_init(void);
static void __exit ntrig_usbhid_exit(void);

/**
 * Driver Host protocols API
 */
static int ntrig_usbhid_send_ncp_report(struct hid_device *hdev,
	const unsigned char *ncp_cmd, short msg_len);

/******************************************************************************/
/*
 * N-trig HID Report Structure
 * The driver will support MTM firwmare Pen, Fingers
 */
struct ntrig_usbhid_data {
	struct _ntrig_bus_device *ntrig_dispatcher;
	struct mr_message_types_s message;
	__u16 pressure;
	__u8 events;
	__u16 x;
	__u16 y;
	__u16 frame_index;
	__u8 finger_id;
	__u16 dx;
	__u16 dy;
	__u8 generic_byte;
	__u8 blob_id;
	__u8 isPalm;
	__u8 msc_cnt;
	__s32 btn_pressed;
	__s32 btn_removed;
	__u8 first_occurance;
	__u8 sensor_id;
	__u8 battery_status;
	__u8 contact_count;
	__u8 tracked_generic_byte;
	__u8 tracked_blob_id;
	__u8 tracked_isPalm;
	/** Flag for ignoring the HID descriptor of the dummy mouse */
	__u8 isDummyMouse;
};

/* NOTE: Static variables and global variables are automatically initialized to
 * 0 by the compiler. The kernel style checker tool (checkpatch.pl) complains
 * if they are explicitly initialized to 0 (or NULL) in their definition.
 */

static __u16 last_frame_index;
static __u8 is_first_frame = 1;
/** queue for ncp messages from the sensor */
static struct ntrig_ncp_fifo ncp_fifo;

void clear_input_buffer(struct ntrig_usbhid_data *nd)
{
	memset(&nd->message, 0, sizeof(nd->message));
}

void set_button(struct ntrig_usbhid_data *nd, __s32 btn)
{
	if (nd->btn_pressed != btn) {
		nd->btn_removed = nd->btn_pressed;
		nd->btn_pressed = btn;
	}
}

void process_multi_touch_finger(struct ntrig_usbhid_data *nd)
{
	int index = nd->message.msg.fingers_event.num_of_fingers;
	struct device_finger_s *finger =
		&nd->message.msg.fingers_event.finger_array[index];
	nd->message.type = MSG_FINGER_PARSE;
	finger->track_id = nd->finger_id;
	finger->x_coord = nd->x;
	finger->y_coord = nd->y;
	finger->dx = nd->dx;
	finger->dy = nd->dy;
	/* We pass generic byte via removed field.
	 * This is just temorary check: if vendor defined == 0xFFFFFFFFF, the
	 * event is tracked. "vendor defined" is reported for each finger, but
	 * its value is only valid for the touching fingers or when there are
	 * no fingers on the sensor. E.g. when only 2 fingers are on the sensor,
	 * data will be reported for all 10 fingers, but only the data for the
	 * first 2 fingers will be valid - the other 8 fingers will contain
	 * garbage.
	 * We should get descriptor to the last bit in flag of each finger so
	 * we can check it according to the spec.
	 */
	if ((nd->generic_byte == 0xFF) && (nd->isPalm == 0xFF) &&
		(nd->blob_id == 0xFF)) { /*this message already tracked in FW*/
		finger->removed = (!nd->tracked_generic_byte);
		finger->palm = nd->tracked_isPalm;
		finger->generic = nd->tracked_blob_id;
	} else {
		finger->removed = nd->generic_byte;
		finger->palm = nd->isPalm;
		finger->generic = nd->blob_id;
	}
	nd->message.msg.fingers_event.num_of_fingers++;
}

void send_multi_touch(struct ntrig_usbhid_data *nd)
{
	struct finger_parse_s *event = &nd->message.msg.fingers_event;
	/* lets verify that buffer is used for multi-touch data */
	if (MSG_FINGER_PARSE == nd->message.type) {
		event->num_of_fingers = nd->contact_count;
		/* add frame index, when  sending all fingers data */
		event->frame_index = nd->frame_index;
		event->sensor_id = nd->sensor_id;
		if (WriteHIDNTRIG(&nd->message) != DTRG_NO_ERROR)
			ntrig_err("FAILED to send MULTI-TOUCH\n");
		clear_input_buffer(nd);	/* Prepare for the next message */
	}
}

void send_pen(struct ntrig_usbhid_data *nd)
{
	struct device_pen_s *event = &nd->message.msg.pen_event;
	clear_input_buffer(nd);
	nd->message.type = MSG_PEN_EVENTS;
	event->x_coord = nd->x;
	event->y_coord = nd->y;
	event->pressure = nd->pressure;
	event->btn_code = nd->btn_pressed;
	event->btn_removed = nd->btn_removed;
	event->sensor_id = nd->sensor_id;
	event->battery_status = nd->battery_status;

	if (WriteHIDNTRIG(&nd->message) != DTRG_NO_ERROR)
		ntrig_err("FAILED to send PEN\n");
}
/******************************************************************************/

/* path of input device */
static char phys_path[256];

/*
 * get feature buffer - as a result of sysfs show and set
 * first byte indicate amount of byte to copy
 */
static unsigned char hid_touch_ep_msg[HID_CLASS_TOUCH_EP_LEN];
static __u16 ncp_report_counter;
static u8 ncp[NCP_MSG_LEN];

/*
 * this driver is aimed at two firmware versions in circulation:
 *  - dual pen/finger single touch
 *  - finger multitouch, pen not working
 */

/*
static char *get_hid_usage_name(struct hid_usage *usage)
{
	switch (usage->hid)
	{
	case HID_GD_X: return "x";
	case HID_GD_Y: return "y";
	case HID_DG_CONTACTCOUNT: return "contactCount";
	case MTM_FRAME_INDEX: return "frameIndex";
	case HID_DG_TIPSWITCH: return "tipSwitch";
	case HID_DG_INRANGE: return "inRange";
	case HID_DG_CONFIDENCE: return "confidence";
	case HID_DG_CONTACTID: return "contactId";
	case HID_DG_WIDTH: return "width";
	case HID_DG_HEIGHT: return "height";
	case MTM_PROPRIETARY: return "proprietary";
	}
	return "unknown";
}
*/

/* input mapping return values */
#define IGNORE_MAPPING -1
#define DEFAULT_MAPPING 0
#define DRIVER_MAPPING 1

static int ntrig_usbhid_input_mapping(struct hid_device *hdev,
	struct hid_input *hi, struct hid_field *field, struct hid_usage *usage,
	unsigned long **bit, int *max)
{
	struct ntrig_usbhid_data *nd = hid_get_drvdata(hdev);
	/*ntrig_dbg("%s: usage=%x (%s), usage->type=%x\n",
		__func__, usage->hid, get_hid_usage_name(usage), usage->type);*/

	if (nd->isDummyMouse)	/* ignore dummy mouse descriptor */
		return IGNORE_MAPPING;

	switch (usage->hid & HID_USAGE_PAGE) {
	case HID_UP_BUTTON:
		/* dummy mouse descriptor - starts with Button usage page */
		nd->isDummyMouse = 1;
		return IGNORE_MAPPING;
	case HID_UP_GENDESK:
		switch (usage->hid) {
		case HID_GD_X:
			hid_map_usage(hi, usage, bit, max, EV_ABS,
				ABS_MT_POSITION_X);
			input_set_abs_params(hi->input, ABS_X,
				field->logical_minimum, field->logical_maximum,
				0, 0);
			nd->ntrig_dispatcher->logical_min_x =
				field->logical_minimum;
			nd->ntrig_dispatcher->logical_max_x =
				field->logical_maximum;
			ntrig_dbg("%s ABX_X_MIN=%d ABX_X_MAX=%d\n", __func__,
				field->logical_minimum, field->logical_maximum);
			return DRIVER_MAPPING;
		case HID_GD_Y:
			hid_map_usage(hi, usage, bit, max, EV_ABS,
				ABS_MT_POSITION_Y);
			nd->ntrig_dispatcher->logical_min_y =
				field->logical_minimum;
			nd->ntrig_dispatcher->logical_max_y =
				field->logical_maximum;
			input_set_abs_params(hi->input, ABS_Y,
				field->logical_minimum, field->logical_maximum,
				0, 0);
			ntrig_dbg("%s ABX_Y_MIN=%d ABX_Y_MAX=%d\n", __func__,
				field->logical_minimum, field->logical_maximum);
			return DRIVER_MAPPING;
		}
		return DEFAULT_MAPPING;

	case HID_UP_DIGITIZER:
		switch (usage->hid) {
		/* we do not want to map these for now */
		case HID_DG_INVERT: /* Not support by pen */
		case HID_DG_ERASER: /* Not support by pen */

		case HID_DG_CONTACTID: /* Not trustworthy, squelch for now */
		case HID_DG_INPUTMODE:
		case HID_DG_DEVICEINDEX:
		case HID_DG_CONTACTMAX:
			return IGNORE_MAPPING;
		/* width/height mapped on TouchMajor/TouchMinor/Orientation */
		case HID_DG_WIDTH:
			hid_map_usage(hi, usage, bit, max, EV_ABS,
				ABS_MT_TOUCH_MAJOR);
			return DRIVER_MAPPING;
		case HID_DG_HEIGHT:
			hid_map_usage(hi, usage, bit, max, EV_ABS,
				ABS_MT_TOUCH_MINOR);
			hid_map_usage(hi, usage, bit, max, EV_ABS,
				ABS_MT_TRACKING_ID);
			return DRIVER_MAPPING;
		}
		return DEFAULT_MAPPING;

	case 0xff000000:
		/* we do not want to map these: no input-oriented meaning */
		return IGNORE_MAPPING;
	}
	return DEFAULT_MAPPING;
}

/*
 * This function maps Keys For Pen And Touch events
 * MSC events used to transfer information about finger status
 * In curent Frame
*/
static int ntrig_usbhid_input_mapped(struct hid_device *hdev,
	struct hid_input *hi, struct hid_field *field, struct hid_usage *usage,
	unsigned long **bit, int *max)
{
	/*ntrig_dbg("%s\n", __func__);*/

	if (usage->type == EV_KEY || usage->type == EV_ABS)
		clear_bit(usage->code, *bit);

	return 0;
}

#define NCP_USAGE_PAGE 0xff0b0000

/*
static int is_ncp_report_id(int report_id)
{
	return ((report_id >= FEATURE_SHIFT_G4 + INPUT_NCP_REPORT_ID_MIN) &&
		(report_id <= FEATURE_SHIFT_G4 + INPUT_NCP_REPORT_ID_MAX));
}

static int ntrig_usbhid_raw_event(struct hid_device *hid,
	struct hid_report *report, u8 *data, int size)
{
	if (is_ncp_report_id(report->id)) {
		if (size > MAX_NCP_LENGTH) {
			ntrig_err(
				"%s: ncp message too large (%d), discarding\n",
				__func__, size);
			return 0;
		}
		/* the first data byte is the report id, ignore it */
/*
		enqueue_ncp_message(&ncp_fifo, data + 1, size - 1);
		return 1;
	}
	return 0;
}
*/

/*
 * this function is called upon all reports so that we can filter contact point
 * information, decide whether we are in multi or single touch mode and call
 * input_mt_sync after each point if necessary.
 * Return value:  0 on no action performed, 1 when no further processing should
 * be done and negative on error.

 */
static int ntrig_usbhid_event(struct hid_device *hid, struct hid_field *field,
	struct hid_usage *usage, __s32 value)
{
	__s32 btn = BTN_UNPRESSED;

	if (hid->claimed & HID_CLAIMED_INPUT) {
		struct ntrig_usbhid_data *nd = hid_get_drvdata(hid);
		struct input_dev *input = field->hidinput->input;
		u16 msg_size = field->report_count;

		if ((usage->hid & HID_USAGE_PAGE) == NCP_USAGE_PAGE) {
			if (msg_size > MAX_NCP_LENGTH) {
				ntrig_err(
					"%s: ncp message too large (%d), "
					"discarding\n", __func__, msg_size);
				return DTRG_FAILED;
			}
			ncp[ncp_report_counter] = value;
			if (++ncp_report_counter == msg_size) {
				ncp_report_counter = 0;
				enqueue_ncp_message(&ncp_fifo, ncp, msg_size);
			}
			return 1;
		}

		/* We attach multi touch to dispatcher if there is no multi
		 * touch queue*/
		if (!check_multi_touch(nd->sensor_id)) {
			ntrig_dbg(
				"%s Attach Multi Touch device to dispatcher\n",
				__func__);
			attach_multi_touch(nd->sensor_id, input);
		}

#ifndef MT_REPORT_TYPE_B
		/* We attach single touch to dispatcher if there is no single
		 * touch queue */
		if (!check_single_touch(nd->sensor_id)) {
			ntrig_dbg(
				"%s Attach Single Touch device to dispatcher\n",
				__func__);
			attach_single_touch(nd->sensor_id, input);
		}
#endif

		switch (usage->hid) {
		case HID_GD_X:
			/* ntrig_dbg("%s: HID_GD_X=%d\n", __func__, value); */
			nd->x = value;
			break;
		case HID_GD_Y:
			/* ntrig_dbg("%s: HID_GD_Y=%d\n", __func__, value); */
			nd->y = value;
			break;
		}
		if (field->application == HID_DG_PEN) {
			switch (usage->hid) {
			case HID_DG_INRANGE:
				/* ntrig_dbg("%s: HID_DG_PEN: HID_DG_INRANGE=%x"
				", value %d\n", __func__, usage->hid, value); */
				nd->events = value;
				break;
			case HID_DG_TIPSWITCH:
				/* ntrig_dbg("%s: HID_DG_PEN: HID_DG_TIPSWITCH="
				"%x, value %d\n", __func__, usage->hid,
				value); */
				nd->events |= (value << 1);
				break;
			case HID_DG_BARRELSWITCH:
				/* ntrig_dbg("%s: HID_DG_PEN: "
				"HID_DG_BARRELSWITCH=%x, value %d\n", __func__,
				usage->hid, value); */
				nd->events |= (value << 2);
				break;
			case HID_DG_INVERT:
				/* ntrig_dbg("%s: HID_DG_PEN: HID_DG_INVERT=%x,
				value %d\n", __func__, usage->hid, value); */
				nd->events |= (value << 3);
				break;
			case HID_DG_ERASER:
				/* ntrig_dbg("%s: HID_DG_PEN: HID_DG_ERASER=%x,"
				" value %d\n", __func__, usage->hid, value); */
				nd->events |= (value << 4);
				break;
			case HID_DG_TIPPRESSURE:
				/* ntrig_dbg("%s: HID_DG_PEN: "
				"HID_DG_TIPPRESSURE=%x, value %d\n", __func__,
				usage->hid, value); */
				nd->pressure = value;
				btn = (int) nd->events;
				/* process button information and send to
				 * dispatcher if required */
				set_button(nd, btn);
				/* send pen data to dispatcher */
				send_pen(nd);
				/* print button and pen data */
				/*ntrig_dbg("Privet X=%d Y=%d Button=%d
				Pressure=%d\n",  nd->x, nd->y, nd->btn_pressed,
				nd->pressure);*/
				break;
			default:
				/*ntrig_dbg("%s: HID_DG_PEN: default=%x, value "
				"%d\n", __func__, usage->hid, value);*/
				break;
			}
		} else { /* MultiTouch Report */

			switch (usage->hid) {
			case HID_DG_CONTACTCOUNT:
				nd->contact_count = value;
				/* end of report (this field always comes last)
				 * - send to dispatcher*/
				send_multi_touch(nd);
				break;
			case MTM_FRAME_INDEX: /* Index 1 */
				/*ntrig_dbg("%s: MultiTouch Report: "
				"MTM_FRAME_INDEX=%x, value %d\n", __func__,
				usage->hid, value); */
				if ((!is_first_frame) &&
					(value > last_frame_index + 1)) {
					ntrig_err(
						"%s: Missed %d frames. Prev "
						"index %u, new index %u.\n",
						__func__,
						value - last_frame_index - 1,
						last_frame_index, value);
				}
				last_frame_index = nd->frame_index = value;
				is_first_frame = 0;
				break;
				/* we store tip switch, in range and touch valid
				 * in tracked_generic_byte.
				 * if it's tracked message, we will use these
				 * values. If not, we will use values from
				 * vendor defined.
				 */
			case HID_DG_TIPSWITCH:
				nd->tracked_generic_byte = value;
				break;
			case HID_DG_INRANGE:
				nd->tracked_blob_id = value;
				break;
			case HID_DG_CONFIDENCE:
				nd->tracked_isPalm = value;
				break;
			case HID_DG_CONTACTID: /* Index 5 */
				nd->finger_id = value;
				break;
			case HID_DG_WIDTH:/* Index 6 - 7*/
				nd->dx = value;
				break;
			case HID_DG_HEIGHT:/* Index 8 - 9 */
				nd->dy = value;
				/* Start The Sequence of MSC bytes */
				nd->msc_cnt = 0;
				break;
			case MTM_PROPRIETARY:/* Index 10 - 14 */
				/*ntrig_dbg("%s: MultiTouch Report: v=%x, value"
				" %d\n", __func__, usage->hid, value);*/
				nd->msc_cnt++;
				/*ntrig_dbg("%s: MTM_PROPRIETARY msc_cnt=%d "
				"val=%d\n", __func__, nd->msc_cnt, value);*/
				switch (nd->msc_cnt) {
				case REPORT_GENERIC1:
					nd->generic_byte = value;
					break;
				case REPORT_MT:
					nd->blob_id = value;
					break;
				case REPORT_PALM:
					nd->isPalm = value;
					break;
				case REPORT_GENERIC2:
					/*end of single finger part of report*/
					/*process multi touch finger data*/
					process_multi_touch_finger(nd);
					/* Print finger data */
					ntrig_dbg(
						"Frame=%x Finger=%d X=%d Y=%d"
						" DX=%d DY=%d FirstOccur=%d "
						"Palm=%d Press=%d Blob=%d\n",
						(unsigned int)nd->frame_index,
						(int)nd->finger_id, (int)nd->x,\
						(int)nd->y, (int)nd->dx,
						(int)nd->dy,
						(int)nd->generic_byte,
						(int)nd->isPalm,
						(int)nd->pressure,
						(int)nd->blob_id);
					break;
				}
				break;
			}
		}
	}
	/* we have handled the hidinput part, now remains hiddev */
	if ((hid->claimed & HID_CLAIMED_HIDDEV) && hid->hiddev_hid_event)
		hid->hiddev_hid_event(hid, field, usage, value);

	return 1;
}

/*
 * This function used to configure N-trig firmware
 * The first command we need to send to firmware is change
 * to Multi-touch Mode we don't receive a reply
 * Endpoint 1 NCP
 */
static int ntrig_usbhid_send_ncp_report(struct hid_device *hdev,
	const unsigned char *ncp_cmd, short msg_len)
{
	struct hid_report *report;
	struct list_head *report_list =
		&hdev->report_enum[HID_FEATURE_REPORT].report_list;

	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct usb_device *dev = interface_to_usbdev(intf);
	__u16 ifnum = intf->cur_altsetting->desc.bInterfaceNumber;
	int ret = 0, report_id, orig_len;
	__u8 *buffer;

	ntrig_dbg("inside %s\n", __func__);
	ntrig_dbg("%s: ncp_cmd[0]=%0X ncp_cmd[1]=%0X\n", __func__, ncp_cmd[0],
		ncp_cmd[1]);
	report = list_first_entry(report_list, struct hid_report, list);
	if (report->maxfield < 1)
		return -ENODEV;
	/* Select the report id according to the message size (msg_len).
	 * The first buffer byte should be the report id, therefore allocate
	 * one byte more than the actual buffer size.
	 */
	orig_len = msg_len;
	if(msg_len < 16) {
		msg_len = 16;
		report_id = FEATURE_128_BITS + FEATURE_SHIFT_G4;
	} else if(msg_len < 32) {
		msg_len = 32;
		report_id = FEATURE_256_BITS + FEATURE_SHIFT_G4;
	} else if(msg_len < 63) {
		msg_len = 63;
		report_id = FEATURE_512_BITS + FEATURE_SHIFT_G4;
	} else if(msg_len < 255) {
		msg_len = 255;
		report_id = FEATURE_2048_BITS + FEATURE_SHIFT_G4;
	} else if(msg_len < 511) {
		msg_len = 511;
		report_id = FEATURE_4096_BITS + FEATURE_SHIFT_G4;
	}
	buffer = kzalloc(msg_len, GFP_KERNEL);
	*buffer = report_id;
	memcpy(buffer + 1, ncp_cmd, orig_len);
	list_for_each_entry(report, report_list, list) {
		if (report->maxfield < 1) {
			ntrig_dbg("no fields in the report\n");
			continue;
		}
		if (report_id == report->id) {
			ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				HID_REQ_SET_REPORT,
				USB_DIR_OUT | USB_TYPE_CLASS |
					USB_RECIP_INTERFACE,
				(0x03 << 8) | buffer[0],
				ifnum, (void*)buffer, msg_len, USB_CTRL_SET_TIMEOUT);
		}
	}
	kfree(buffer);
	return ret;
}

/**
 * Read sensor configuration response from HID device.
 * Return number of bytes read or fail.
 */
int NTRIGReadSensor(void *dev, char *buf, size_t count)
{
	int size = hid_touch_ep_msg[HID_REPLY_SIZE];
	ntrig_dbg("usbhid: inside %s out_buf size = %d\n", __func__,
		(int)sizeof(buf));

	/* size of input buffer (coming from sysfs show function) is PAGE_SIZE
	 * (4096 bytes) so it's very unlikely we'll have memory overwriting.
	 * Therefore there's no need to check the buf size before writing to it
	*/
	if (count < size) {	/* buffer too small */
		ntrig_dbg("usbhid: Leaving %s, count (%d)< size(%d)\n",
			__func__, (int)count, size);
		return DTRG_FAILED;
	}

	memcpy(buf, &hid_touch_ep_msg[HID_REPLY_DATA], size);
	ntrig_dbg("usbhid: Leaving %s\n", __func__);

	return size;
}

/**
 * Write NCP msg to HID device.
 * Return success or fail.
 */
int NTRIGWriteNCP(void *dev, const char *buf, short msg_len)
{
	int count;
	struct hid_device *hdev = dev;

	ntrig_dbg("inside %s\n", __func__);
	ntrig_dbg("%s: buf[0]=%0X buf[1]=%0X\n", __func__, buf[0], buf[1]);

	if ((!dev) || (!buf) || (msg_len == 0)) {
		ntrig_err("%s: wrong paramas\n", __func__);
		return DTRG_FAILED;
	}

	/*This version don't support bytes received from shell (ascii)*/
	count = ntrig_usbhid_send_ncp_report(hdev, buf, msg_len);

	ntrig_dbg("Leaving %s , count = %d\n", __func__, count);
	return count;
}

/**
 * Read NCP msg from HID device.
 * Return number of bytes read or fail.
 */
int NTRIGReadNCP(void *dev, char *buf, size_t count)
{
	ntrig_dbg("usbhid: inside %s\n", __func__);
	return read_ncp_message(&ncp_fifo, buf, count);
}

static int ntrig_usbhid_probe(struct hid_device *hdev,
	const struct hid_device_id *id)
{
	int ret;
	struct ntrig_usbhid_data *nd;
	struct usb_interface *intf;
	struct usb_device *dev;
	struct _ntrig_dev_ncp_func ncp_func;

	intf = to_usb_interface(hdev->dev.parent);
	dev = interface_to_usbdev(intf);
	nd = kmalloc(sizeof(struct ntrig_usbhid_data), GFP_KERNEL);
	if (!nd) {
		dev_err(&hdev->dev, "cannot allocate N-Trig data\n");
		return -ENOMEM;
	}
	if (DTRG_NO_ERROR != allocate_device(&nd->ntrig_dispatcher)) {
		dev_err(&hdev->dev, "cannot allocate N-Trig dispatcher\n");
		kfree(nd);
		return DTRG_FAILED;
	}
	init_ncp_fifo(&ncp_fifo, NULL);
	nd->isDummyMouse = 0;
	hid_set_drvdata(hdev, nd);
	ret = hid_parse(hdev);
	if (ret) {
		dev_err(&hdev->dev, "parse failed, error code=%d\n", ret);
		goto err_free;
	}

	/* set the NCP callbacks */
	ncp_func.dev = (void *) hdev;
	ncp_func.read = NTRIGReadNCP;
	ncp_func.write = NTRIGWriteNCP;
	ncp_func.read_counters = NULL;
		/*counters not implemented in USB driver*/
	ncp_func.reset_counters = NULL;
		/*counters not implemented in USB driver*/
	nd->sensor_id = RegNtrigDispatcher(TYPE_BUS_USB, hdev->uniq, &ncp_func);
	/* TODO: define behavior in case of fail */
	if (nd->sensor_id == DTRG_FAILED) {
		ntrig_err("%s: Cannot register device to dispatcher\n",
			__func__);
		goto err_free;
	}
	ntrig_dbg(
		"%s dev registered with dispatcher, bus_id=%s, sensor_id=%d\n",
		__func__, hdev->uniq, nd->sensor_id);

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (ret) {
		dev_err(&hdev->dev,
			"hw start failed TOUCH, error code=%d\n", ret);
		goto err_free;
	}

	/**
	 * Create additional single touch queue
	 */

	usb_make_path(dev, phys_path, sizeof(phys_path));
	strlcat(phys_path, "/input0", sizeof(phys_path));

	nd->ntrig_dispatcher->phys = phys_path;
	nd->ntrig_dispatcher->name = "USBHID";
	nd->ntrig_dispatcher->pressure_min = 0;
	nd->ntrig_dispatcher->pressure_max = 0xFF;
#ifndef MT_REPORT_TYPE_B
	create_single_touch(nd->ntrig_dispatcher, nd->sensor_id);
#endif
	create_multi_touch(nd->ntrig_dispatcher, nd->sensor_id);

	nd->battery_status = PEN_BUTTON_BATTERY_NOT_AVAILABLE;

	ntrig_dbg("Inside %s, bus_id = %d, name = %s, phys = %s, uniq = %s\n",
		__func__, hdev->bus, hdev->name, hdev->phys, hdev->uniq);

	ntrig_dbg("End of %s\n", __func__);
	return DTRG_NO_ERROR;
err_free:
	ntrig_err("Error End of %s\n", __func__);
	remove_device(&nd->ntrig_dispatcher);
	kfree(nd);
	return DTRG_FAILED;
}

static void ntrig_usbhid_remove(struct hid_device *hdev)
{
	struct ntrig_usbhid_data *nd;

	ntrig_dbg("Entering %s\n", __func__);
	hid_hw_stop(hdev);

	nd = hid_get_drvdata(hdev);
	UnregNtrigDispatcher(nd->ntrig_dispatcher, nd->sensor_id, TYPE_BUS_USB,
		hdev->uniq);
	remove_device(&nd->ntrig_dispatcher);
	uninit_ncp_fifo(&ncp_fifo);
	kfree(nd);

	ntrig_dbg("%s all resource released\n", __func__);
}

static const struct hid_device_id ntrig_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_NTRIG, HID_ANY_ID),
		.driver_data = NTRIG_USB_DEVICE_ID },
	{ }
};
MODULE_DEVICE_TABLE(hid, ntrig_devices);

static struct hid_driver ntrig_usbhid_driver = {
	.name = "ntrig_usbhid",
	.id_table = ntrig_devices,
	.probe = ntrig_usbhid_probe,
	.remove = ntrig_usbhid_remove,
	.input_mapping = ntrig_usbhid_input_mapping,
	.input_mapped = ntrig_usbhid_input_mapped,
	.event = ntrig_usbhid_event,
	/* raw_event is better for NCP, for performance - instead of receiving
	 * each NCP byte in the event callback, the entire packet can be
	 * received in the raw_event in a single call and added to the queue.
	 * However, raw_event caused some problem to appear, which is difficult
	 * to detect and is probably not related to the callback itself - so we
	 * don't use raw_event for now.
	 */
	/*.raw_event = ntrig_usbhid_raw_event*/
};

static int __init ntrig_usbhid_init(void)
{
	printk(KERN_DEBUG "USBHID Driver Version 2.00\n");
		/*DEBUG Use untill we stablize the version*/
	return hid_register_driver(&ntrig_usbhid_driver);
}

static void __exit ntrig_usbhid_exit(void)
{
	hid_unregister_driver(&ntrig_usbhid_driver);
}

module_init(ntrig_usbhid_init);
module_exit(ntrig_usbhid_exit);

MODULE_LICENSE("GPL");
