/*
 * @file inv_counters.c
 * @brief Exports i2c read write counts through sysfs
 *
 * @version 0.1
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/kdev_t.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/kernel_stat.h>

#include "inv_counters.h"

static int mpu_irq;
static int accel_irq;
static int compass_irq;

struct inv_counters {
	uint32_t i2c_tempreads;
	uint32_t i2c_mpureads;
	uint32_t i2c_mpuwrites;
	uint32_t i2c_accelreads;
	uint32_t i2c_accelwrites;
	uint32_t i2c_compassreads;
	uint32_t i2c_compasswrites;
	uint32_t i2c_compassirq;
	uint32_t i2c_accelirq;
};

static struct inv_counters counters;

static ssize_t i2c_counters_show(struct class *cls,
			struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
		"%ld.%03ld %u %u %u %u %u %u %u %u %u %u\n",
		jiffies / HZ, ((jiffies % HZ) * (1024 / HZ)),
		mpu_irq ? kstat_irqs(mpu_irq) : 0,
		counters.i2c_tempreads,
		counters.i2c_mpureads, counters.i2c_mpuwrites,
		accel_irq ? kstat_irqs(accel_irq) : counters.i2c_accelirq,
		counters.i2c_accelreads, counters.i2c_accelwrites,
		compass_irq ? kstat_irqs(compass_irq) : counters.i2c_compassirq,
		counters.i2c_compassreads, counters.i2c_compasswrites);
}

void inv_iio_counters_set_i2cirq(enum irqtype type, int irq)
{
	switch (type) {
	case IRQ_MPU:
		mpu_irq = irq;
		break;
	case IRQ_ACCEL:
		accel_irq = irq;
		break;
	case IRQ_COMPASS:
		compass_irq = irq;
		break;
	}
}
EXPORT_SYMBOL_GPL(inv_iio_counters_set_i2cirq);

void inv_iio_counters_tempread(int count)
{
	counters.i2c_tempreads += count;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_tempread);

void inv_iio_counters_mpuread(int count)
{
	counters.i2c_mpureads += count;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_mpuread);

void inv_iio_counters_mpuwrite(int count)
{
	counters.i2c_mpuwrites += count;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_mpuwrite);

void inv_iio_counters_accelread(int count)
{
	counters.i2c_accelreads += count;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_accelread);

void inv_iio_counters_accelwrite(int count)
{
	counters.i2c_accelwrites += count;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_accelwrite);

void inv_iio_counters_compassread(int count)
{
	counters.i2c_compassreads += count;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_compassread);

void inv_iio_counters_compasswrite(int count)
{
	counters.i2c_compasswrites += count;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_compasswrite);

void inv_iio_counters_compassirq(void)
{
	counters.i2c_compassirq++;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_compassirq);

void inv_iio_counters_accelirq(void)
{
	counters.i2c_accelirq++;
}
EXPORT_SYMBOL_GPL(inv_iio_counters_accelirq);

static struct class_attribute inv_class_attr[] = {
	__ATTR(i2c_counter, S_IRUGO, i2c_counters_show, NULL),
	__ATTR_NULL
};

static struct class inv_counters_class = {
	.name = "inv_counters",
	.owner = THIS_MODULE,
	.class_attrs = (struct class_attribute *) &inv_class_attr
};

static int __init inv_counters_init(void)
{
	memset(&counters, 0, sizeof(counters));

	return class_register(&inv_counters_class);
}

static void __exit inv_counters_exit(void)
{
	class_unregister(&inv_counters_class);
}

module_init(inv_counters_init);
module_exit(inv_counters_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GESL");
MODULE_DESCRIPTION("inv_counters debug support");

