#ifndef _FLASH_TEST_H_
#define _FLASH_TEST_H_

#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <mach/adi.h>
#include <mach/hardware.h>
#include <mach/board.h>

#define FLASH_TEST_DEVICE_NODE_NAME  "flash_test"
#define FLASH_TEST_DEVICE_FILE_NAME  "flash_test"
#define FLASH_TEST_DEVICE_PROC_NAME  "flash_test"
#define FLASH_TEST_DEVICE_CLASS_NAME "flash_test"

enum flash_status {
	FLASH_CLOSE = 0x0,
	FLASH_OPEN = 0x1,
	FLASH_TORCH = 0x2,	/*user only set flash to close/open/torch state */
	FLASH_AUTO = 0x3,
	FLASH_CLOSE_AFTER_OPEN = 0x10,	/* following is set to sensor */
	FLASH_HIGH_LIGHT = 0x11,
	FLASH_OPEN_ON_RECORDING = 0x22,
	FLASH_CLOSE_AFTER_AUTOFOCUS = 0x30,
	FLASH_STATUS_MAX
};

#endif
