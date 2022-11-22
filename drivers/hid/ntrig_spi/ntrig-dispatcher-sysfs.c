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

#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#include "typedef-ntrig.h"
#include "ntrig-common.h"
#include "ntrig-dispatcher.h"

#include "ntrig_virtual_keys_def.h"

/******************************************************************************/
#define DISPATCHER_VER_FILE		"/etc/ntrig_version"

/* Globals are automatically initialized to 0 (/ NULL) by the compiler */
config_callback		read_config_dispatcher;
config_callback		write_config_dispatcher;
message_callback	read_get_bus_interface;
message_callback	write_get_bus_interface;
read_counters_callback  read_get_counters;
reset_counters_callback write_reset_counters;

/******************************************************************************/
/**
 * virtual keys functionallity
 */
#ifdef VIRTUAL_KEYS_SUPPORTED
/**
 * general definitions
 */
#define EOK	0	/* no error */
#define CHARS_PER_NUMBER 10 /* we don't expect numbers larger than 10 digits */
#define VIRTUAL_KEYS_FILE_PREFIX "virtualkeys."
	/* This is the constant part of the file name, will be concatenate with
	 * user defined part */

/**
 * sysfs Local API
 */
static ssize_t ntrig_virtual_keys_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

/* attributes buffer size - make sure it doens't exceed PAGE_SIZE (On i386,
 * this is 4096 On i386)
 * Buffer holding the virutal keys definitions
 * (6 values:reserved:scan-code:x:y:w:h; 5 delimiters +\n" +final \0) ?*
 * number of keys */
static char virtual_keys_attr_buf[(CHARS_PER_NUMBER * 6 + sizeof(char) * 7) *
	VIRTUAL_KEYS_NUM];
static char virtual_keys_file_name[sizeof(VIRTUAL_KEYS_FILE_PREFIX) + sizeof(
	NTRIG_DEVICE_NAME) + 1];

/* These structures support single virtual keys device. If more device should be
 * supported in the futures, more static kobj_attribute can be added,
 * ntrig_properties_attrs should be replaced by an array and corresponding code
 * should be changed accordingly.
 */
static struct kobj_attribute ntrig_virtualkeys_attrs =
	__ATTR(NULL, S_IRUGO, ntrig_virtual_keys_show, NULL);
/* NOTE: actual filename in virtual_keys_file_name will be written later */

static struct attribute *ntrig_properties_attrs = &ntrig_virtualkeys_attrs.attr;
static struct kobject *board_properties_kobj;

/*
 * Read (show) data from the sysfs file
 */
static ssize_t ntrig_virtual_keys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ntrig_dbg(
		"inside %s - attributes = %s\n", __FUNCTION__,
		virtual_keys_attr_buf);
	return snprintf(buf, PAGE_SIZE, "%s\n", virtual_keys_attr_buf);
}

/*
 * Release sysfs files
 */
static void ntrig_board_properties_release(void)
{
	ntrig_dbg("inside %s\n", __func__);

	if (board_properties_kobj == NULL) {
		ntrig_dbg("inside %s, kobj==NULL\n", __func__);
		return;
	}

	/* remove file virtualkeys.<device> under /sys/board_properties */
	sysfs_remove_file(board_properties_kobj, ntrig_properties_attrs);

	/* remove folder /sys/board_properties (this function also handle
	 * reference count and memroy free) */
	kobject_put(board_properties_kobj);
	board_properties_kobj = NULL;
	ntrig_dbg("inside %s After call to kobject_put\n", __func__);
}

/*
 * Create folder "board_properties" under sysfs,
 * and create file virtualskeys.<dev-name> under this folder
 */
static void ntrig_create_virtualkeys_file(void)
{
	int retval;
	int i;
	char tmpbuf[CHARS_PER_NUMBER * 6 + sizeof(char) * 7];

	ntrig_dbg("inside %s\n", __func__);

	/* At this stage we support a single device for virtual keys, hence we
	 * leave this function if the file for this device is already created.
	 * If we support more devices in the future, this is the place to create
	 * a new file.
	 */
	if (board_properties_kobj) {
		ntrig_dbg(
			"inside %s - virtualkey file for the device already "
			"created !\n", __func__);
		return;
	}

	/* 1. Create folder "board_properties" under "sys" */
	/* allocate the memory for the whole object */
	board_properties_kobj = kobject_create_and_add("board_properties",
		NULL);
	ntrig_dbg("inside %s\n", __func__);

	if (!board_properties_kobj) {
		ntrig_err("inside %s - kobject_create_and_add FAILED\n",
			__func__);
		return;
	}

	/*
	 * We are always responsible for sending the uevent that the kobject
	 * was added to the system.
	 */
	kobject_uevent(board_properties_kobj, KOBJ_ADD);

	/* 2. Create virtual keys file name */
	snprintf(virtual_keys_file_name, sizeof(virtual_keys_file_name),
		"%s%s", VIRTUAL_KEYS_FILE_PREFIX, NTRIG_DEVICE_NAME);
	ntrig_properties_attrs->name = virtual_keys_file_name;

	/* 3. create file "virtualkeys" under folder "board_properties"  */
	/* create the file "virtualkeys.<ntrig-dev-name>" under
	 * "board_properties" folder */
	retval = sysfs_create_file(board_properties_kobj,
		ntrig_properties_attrs);

	if (retval != EOK) {
		ntrig_err("inside %s - sysfs_create_file FAILED\n", __func__);
		/* remove properties_kobj */
		kobject_put(board_properties_kobj);
		return;
	}

	/* 3. Write Virtual keys definitions into the file */
	/* There is no actual writing to file system.
	 * Instead, we create a char buffer containing the data entered by the
	  * user, in the format expected by the reading module.
	 * The buffer is global to this source file, so it can be accessed by
	 * the "show" function.
	 */

	/* see expected format of the buffer in function readVirtualKeys in
	 * file KeyInputQueue.java */
	/* Data for all keys is written into the same buffer. */
	/* Format of data for each key is:
	 * scan-kode:center-x:center-y:width:hight\n */

	virtual_keys_attr_buf[0] = '\0';
	/* a tmp buffer to contain data of one key in string format */
	for (i = 0; i < VIRTUAL_KEYS_NUM; i++) {
		snprintf(tmpbuf, sizeof(tmpbuf), "0x01:%d:%d:%d:%d:%d:",
			_ntrig_virt_keys[i].scan_code,
			_ntrig_virt_keys[i].center_x,
			_ntrig_virt_keys[i].center_y,
			_ntrig_virt_keys[i].width,
			_ntrig_virt_keys[i].height);
		strlcat(virtual_keys_attr_buf, tmpbuf,
			sizeof(virtual_keys_attr_buf));
	}

	/* remove last ':' */
	virtual_keys_attr_buf[strnlen(virtual_keys_attr_buf,
		sizeof(virtual_keys_attr_buf)) - 1] = 0;

	ntrig_dbg("inside %s - attributes = %s, len = %d\n", __func__,
		virtual_keys_attr_buf,
		strnlen(virtual_keys_attr_buf, sizeof(virtual_keys_attr_buf)));
}

#endif  /* VIRTUAL_KEYS_SUPPORTED */

/*
 * Virtual Keys Exported APIs
 */
int get_virt_keys_scan_code(int i)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return _ntrig_virt_keys[i].scan_code;
#else
	return false;
#endif
}
int get_virtual_keys_num(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return VIRTUAL_KEYS_NUM;
#else
	return false;
#endif
}

bool virtual_keys_supported(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return true;
#else
	return false;
#endif
}

int get_touch_screen_border_left(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return TOUCH_SCREEN_BORDER_LEFT;
#else
	return 0;
#endif
}

int get_touch_screen_border_right(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return TOUCH_SCREEN_BORDER_RIGHT;
#else
	return 0;
#endif
}

int get_touch_screen_border_down(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return TOUCH_SCREEN_BORDER_DOWN;
#else
	return 0;
#endif
}

int get_touch_screen_border_up(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return TOUCH_SCREEN_BORDER_UP;
#else
	return 0;
#endif
}

int get_touch_screen_border_pen_left(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return TOUCH_SCREEN_BORDER_PEN_LEFT;
#else
	return 0;
#endif
}

int get_touch_screen_border_pen_right(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return TOUCH_SCREEN_BORDER_PEN_RIGHT;
#else
	return 0;
#endif
}

int get_touch_screen_border_pen_down(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return TOUCH_SCREEN_BORDER_PEN_DOWN;
#else
	return 0;
#endif
}

int get_touch_screen_border_pen_up(void)
{
#ifdef VIRTUAL_KEYS_SUPPORTED
	return TOUCH_SCREEN_BORDER_PEN_UP;
#else
	return 0;
#endif
}

/******************************************************************************/
/*
 * Dispatcher configuration files
 */
#define ATTRIBUTE_NAME_MATCHES(attr, const_string) \
	(strncmp(attr->attr.name, const_string, sizeof(const_string)) == 0)

static ssize_t config_dispatcher_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int retval = 0;

	if (ATTRIBUTE_NAME_MATCHES(attr, "touch_screen_border")) {
		ntrig_dbg(
			"ntrig-sysfs-dispatcher: inside %s, "
			"CONFIG_TOUCH_SCREEN_BORDER\n", __func__);
		retval = read_config_dispatcher(buf,
			CONFIG_TOUCH_SCREEN_BORDER);
	} else if (ATTRIBUTE_NAME_MATCHES(attr, "driver_version")) {
		ntrig_dbg(
			"ntrig-sysfs-dispatcher: inside %s, "
			"CONFIG_DRIVER_VERSION\n", __func__);
		retval = read_config_dispatcher(buf, CONFIG_DRIVER_VERSION);
	} else if (ATTRIBUTE_NAME_MATCHES(attr, "debug_print")) {
		ntrig_dbg(
			"ntrig-sysfs-dispatcher: inside %s, "
			"CONFIG_DEBUG_PRINT\n", __func__);
		retval = read_config_dispatcher(buf, CONFIG_DEBUG_PRINT);
	} else {
		ntrig_err(
			"ntrig-sysfs-dispatcher: inside %s , unknown "
			"configuration\n", __func__);
		retval = DTRG_FAILED;
	}

	return retval;
}

static ssize_t config_dispatcher_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	unsigned char val;

	/*sscanf(buf, "%cu", &val);*/
	val = *buf;
	if (ATTRIBUTE_NAME_MATCHES(attr, "touch_screen_border")) {
		ntrig_dbg(
			"ntrig-sysfs-dispatcher: inside %s, "
			"CONFIG_TOUCH_SCREEN_BORDER, data = %d\n", __func__,
			(int)val);
		retval = write_config_dispatcher(&val,
			CONFIG_TOUCH_SCREEN_BORDER);
	} else if (ATTRIBUTE_NAME_MATCHES(attr, "debug_print")) {
		ntrig_dbg(
			"ntrig-sysfs-dispatcher: inside %s, "
			"CONFIG_DEBUG_PRINT, data = %d\n", __func__, (int)val);
		retval = write_config_dispatcher(&val, CONFIG_DEBUG_PRINT);
	} else {
		ntrig_err(
			"ntrig-sysfs-dispatcher: inside %s , unknown "
			"configuration\n", __func__);
		retval = DTRG_FAILED;
	}

	return retval;
}

/*
 * get bus interface
 */
static ssize_t get_bus_interface_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return read_get_bus_interface((void *)buf);
}

static ssize_t get_bus_interface_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval = write_get_bus_interface((void *)buf);
	if (retval >= 0)
		return count;
	else
		return retval;
}

/*
 * get/set debug server port - for automatic detection by the GenericApi
 */

static int debug_server_port = -1;

static ssize_t debug_server_port_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int *value = (int *) buf;
	*value = debug_server_port;
	ntrig_dbg(
		"ntrig-dispatcher-sysfs: inside %s, debug server port is %d\n",
		__func__, debug_server_port);
	return sizeof(int);
}

static ssize_t debug_server_port_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int value;
	if (count < sizeof(int))
		return 0;
	value = *((int *) buf);
	ntrig_dbg(
		"ntrig-dispatcher-sysfs: inside %s, changing debug server "
		"port from %d to %d\n", __func__, debug_server_port, value);
	debug_server_port = value;
	return sizeof(value);
}

static ssize_t reset_counters_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 1;
	if (buf[0] == '0') {
		if (write_reset_counters != NULL)
			write_reset_counters();
	} else
		ntrig_err(
			"Inside %s: RESET_COUNTERS - wrong data=%d, only valid "
			"data is 0\n", __func__, *buf);
	return ret;
}

static ssize_t get_counters_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int i, res;
	struct _ntrig_counter *counters_list;
	int length;

	ntrig_dbg("inside %s\n", __func__);

	if (read_get_counters == NULL) {
		printk(KERN_DEBUG "in %s  read_get_counters is NULL\n",
			__func__);
		return DTRG_FAILED;
	}
	res = 0;

	read_get_counters(&counters_list, &length);

	for (i = 0; i < length; ++i)
		res += snprintf(buf + res, PAGE_SIZE - res, "%s \t %d\n",
			counters_list[i].name, counters_list[i].count);
	return res;
}

static struct kobj_attribute touch_screen_border_attribute =
	__ATTR(touch_screen_border, S_IRUGO | S_IWUGO, config_dispatcher_show,
		config_dispatcher_store);
		/* Store function sets sensor_id for reading screen border */
static struct kobj_attribute driver_version_attribute =
	__ATTR(driver_version, S_IRUGO, config_dispatcher_show, NULL);
/* Control debug prints */
static struct kobj_attribute debug_print_attribute =
	__ATTR(debug_print, S_IRUGO | S_IWUGO, config_dispatcher_show,
		config_dispatcher_store);

/* create sysfs file named "driver_counters"
 * 'cat driver_counters' will print the counters
 * 'echo 0 > driver_counters' will reset all counters to 0
 */
static struct kobj_attribute get_counters_attribute =
	__ATTR(driver_counters, S_IRUGO | S_IWUGO, get_counters_show,
		reset_counters_store);

static struct kobj_attribute get_bus_interface_attribute =
	__ATTR(get_bus_interface, S_IRUGO | S_IWUGO, get_bus_interface_show,
		get_bus_interface_store);

static struct kobj_attribute debug_server_port_attribute =
	__ATTR(debug_server_port, S_IRUGO | S_IWUGO, debug_server_port_show,
		debug_server_port_store);

/*
 * Create a group of attributes so that we can create and destory them all at
 * once.
 */
static struct attribute *attrs[] = {
	&touch_screen_border_attribute.attr,
	&driver_version_attribute.attr,
	&debug_print_attribute.attr,
	&get_bus_interface_attribute.attr,
	&get_counters_attribute.attr,
	&debug_server_port_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
	.attrs = attrs
};

static struct kobject *dispatcher_kobj;

static void dispatcher_files_release(void)
{
	sysfs_remove_group(dispatcher_kobj, &attr_group);
	kobject_put(dispatcher_kobj);
	dispatcher_kobj = NULL;

#ifdef VIRTUAL_KEYS_SUPPORTED
	ntrig_board_properties_release();
#endif
}

int ntrig_dispatcher_sysfs_init(void)
{
	int retval;

	ntrig_dbg("inside %s\n", __func__);

	dispatcher_kobj = kobject_create_and_add("ntrig", NULL);
	if (!dispatcher_kobj) {
		ntrig_err("inside %s\n failed to create dispatcher_kobj",
			__func__);
		return -ENOMEM;
	}
	/* Create the files associated with this kobject */
	retval = sysfs_create_group(dispatcher_kobj, &attr_group);
	if (retval) {
		ntrig_err("inside %s\n failed to create sysfs_group",
			__func__);
		kobject_put(dispatcher_kobj);
	}

#ifdef VIRTUAL_KEYS_SUPPORTED
	ntrig_create_virtualkeys_file();
#endif

	ntrig_dbg("ntrig-dispatcher-sysfs inside %s\n", __func__);

	if (setup_config_dispatcher(&read_config_dispatcher,
		&write_config_dispatcher)) {
		ntrig_err(
			"ntrig-dispatcher-sysfs inside %s cannot "
			"setup_config_dispatcher\n", __func__);
	}
	if (setup_get_bus_interface(&read_get_bus_interface,
		&write_get_bus_interface)) {
		ntrig_err(
			"ntrig-dispatcher-sysfs inside %s cannot "
			"setup_get_bus_interface\n", __func__);
	}
	if (setup_config_counters(&read_get_counters,
		&write_reset_counters)) {
		ntrig_err(
			"ntrig-dispatcher-sysfs inside %s cannot "
			"setup_config_counters\n", __func__);
	}

	return retval;
}

void ntrig_dispathcer_sysfs_exit(void)
{
	ntrig_dbg("inside %s\n", __func__);
	dispatcher_files_release();
}


