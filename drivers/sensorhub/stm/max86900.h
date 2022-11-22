#ifndef _MAX86900_H_
#define _MAX86900_H_

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>

#define MAX86900_DEBUG

//MAX86900 Registers
#define MAX86900_INTERRUPT_STATUS	0x00
#define MAX86900_INTERRUPT_ENABLE	0x01

#define MAX86900_FIFO_WRITE_POINTER	0x02
#define MAX86900_OVF_COUNTER		0x03
#define MAX86900_FIFO_READ_POINTER	0x04
#define MAX86900_FIFO_DATA			0x05
#define MAX86900_MODE_CONFIGURATION	0x06
#define MAX86900_SPO2_CONFIGURATION		0x07
#define MAX86900_LED_CONFIGURATION		0x09
#define MAX86900_FIFO_SIZE			16

struct max86900_platform_data
{
	int (*init)(void);
	int (*deinit)(void);
};

struct max86900_device_data
{
	struct i2c_client *client;          // represents the slave device
	struct device *dev;
	struct input_dev *hrm_input_dev;
	struct mutex i2clock;
	struct mutex activelock;

	bool *bio_status;
	u8 is_enable;
	u16 led;
	u16 sample_cnt;
	int hrm_int;
	int irq;

	struct regulator *vdd_1p8;
	const char *sub_ldo4;
};

extern int sensors_create_symlink(struct kobject *target, const char *name);
extern void sensors_remove_symlink(struct kobject *target, const char *name);
extern int sensors_register(struct device *dev, void * drvdata,
	struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);
#endif

