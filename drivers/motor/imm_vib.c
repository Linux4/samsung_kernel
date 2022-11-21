#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include "imm_vib.h"

#if 0
#define IMM_VIB_DEBUG
#endif
#define IMM_VIB_TIMER

struct miscdevice *g_miscdev;
static void imm_vib_stop_timer(struct imm_vib_dev *dev);
static int imm_vib_setsamples(struct imm_vib_dev *dev,
	int index, u16 depth, u16 size, char *buff);

#if defined(IMM_VIB_TIMER)
static DEFINE_SEMAPHORE(sem);
static inline int imm_vib_lock(struct semaphore *lock)
{
	return (lock->count) != 1;
}
#endif

static void save_output_data(struct imm_vib_dev *dev,
	const char *buff, int count)
{

	/* Check buffer size and address */
	if ((count < SPI_HEADER_SIZE) || (0 == buff)) {
		printk(KERN_ERR "[VIB] invalid buff.\n");
		return ;
	}

	if (SPI_HEADER_SIZE == count)
		dev->actuator.empty = true;
	else {
		dev->actuator.empty = false;
		dev->actuator.output = buff[SPI_HEADER_SIZE];
	}
}

static void send_output_data(struct imm_vib_dev *dev)
{
	/* Check buffer size and address */
	if (dev->actuator.empty) {
		printk(KERN_ERR "[VIB] buff is empty\n");
		return ;
	}

	imm_vib_setsamples(dev, 0, 8, 1, &dev->actuator.output);
	dev->actuator.output = 0;
	dev->actuator.empty = true;
}

#if defined(IMM_VIB_TIMER)
static void imm_vib_work_func(struct work_struct *work)
{
	struct imm_vib_dev *dev =
		container_of(work, struct imm_vib_dev, work);
#if defined(IMM_VIB_DEBUG)
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 20))
		printk(KERN_DEBUG "[VIB] %s %d\n", __func__, dev->watchdog_cnt);
#endif

	if (!dev->timer_started)
		return ;

	dev->watchdog_cnt++;

	if (imm_vib_lock(dev->sem))
		up(dev->sem);

	if (dev->watchdog_cnt < WATCHDOG_TIMEOUT)
		mod_timer(&dev->timer,
			jiffies + msecs_to_jiffies(dev->timer_ms));
	else
		imm_vib_stop_timer(dev);
}

static void imm_vib_timer(unsigned long _data)
{
	struct imm_vib_dev *dev
		= (struct imm_vib_dev *)_data;

	schedule_work(&dev->work);
}
#endif
static void imm_vib_start_timer(struct imm_vib_dev *dev)
{
#if defined(IMM_VIB_DEBUG)
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 20))
		printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif

	dev->watchdog_cnt = 0;

	if (!dev->timer_started) {
		dev->timer_started = true;
#if defined(IMM_VIB_TIMER)
		if (!imm_vib_lock(dev->sem)) {
			if (down_interruptible(dev->sem))
				printk(KERN_DEBUG "[VIB] %s failed to lock\n", __func__);
		}
		mod_timer(&dev->timer,
			jiffies + msecs_to_jiffies(dev->timer_ms));
#endif
	} else
		if (down_interruptible(dev->sem))
			printk(KERN_DEBUG "[VIB] %s failed to lock\n", __func__);

	send_output_data(dev);
}

static void imm_vib_stop_timer(struct imm_vib_dev *dev)
{
#if defined(IMM_VIB_DEBUG)
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 20))
		printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif

	if (dev->timer_started) {
		dev->timer_started = false;
#if defined(IMM_VIB_TIMER)
		del_timer_sync(&dev->timer);
#endif
	}

	dev->actuator.output = 0;
	dev->actuator.empty = true;
	dev->stop_requested = false;
	dev->is_playing = false;
}

static int imm_vib_disable(struct imm_vib_dev *dev, int index)
{
#if defined(IMM_VIB_DEBUG)
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 20))
		printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif

	if (dev->amp_enabled) {
		dev->amp_enabled = false;
		dev->chip_en(dev->private_data, false);
		if (dev->motor_enabled == 1)
			dev->motor_enabled = 0;
	}

	return 0;
}

static int imm_vib_enable(struct imm_vib_dev *dev, u8 index)
{
#if defined(IMM_VIB_DEBUG)
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 20))
		printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif

	if (!dev->amp_enabled) {
		dev->amp_enabled = true;
		dev->motor_enabled = 1;
		dev->chip_en(dev->private_data, true);
	}

	return 0;
}

static int imm_vib_setsamples(struct imm_vib_dev *dev,
	int index, u16 depth, u16 size, char *buff)
{
	int8_t force;

#if defined(IMM_VIB_DEBUG)
	printk(KERN_DEBUG "[VIB] %s index %d, depth %d, size %d, %d\n",
		__func__, index, depth, size, buff[0]);
#endif

	switch (depth) {
	case 8:
		if (size != 1) {
			printk(KERN_ERR "[VIB] %s size : %d\n", __func__, size);
			return IMM_VIB_FAIL;
		}
		force = buff[0];
		break;

	case 16:
		if (size != 2)
			return IMM_VIB_FAIL;
		force = ((int16_t *)buff)[0] >> 8;
		break;

	default:
		return IMM_VIB_FAIL;
	}

	dev->set_force(dev->private_data, index, force);

	return 0;
}

static int imm_vib_get_name(struct imm_vib_dev *dev,
	u8 index, char *szDevName, int nSize)
{
	return 0;
}

static int imm_vib_open(struct inode *inode, struct file *file)
{
	printk(KERN_DEBUG "[VIB] %s\n", __func__);
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	return 0;
}

static int imm_vib_release(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = g_miscdev; //file->private_data;
	struct imm_vib_dev *dev = container_of(miscdev,
		struct imm_vib_dev,miscdev);

	printk(KERN_DEBUG "[VIB] %s\n", __func__);
	imm_vib_stop_timer(dev);
	file->private_data = (void *)NULL;
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t imm_vib_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct miscdevice *miscdev = g_miscdev;//file->private_data;
	struct imm_vib_dev *dev = container_of(miscdev,
		struct imm_vib_dev, miscdev);
	const size_t size = (dev->cch_device_name > (size_t)(*ppos)) ?
		min(count, dev->cch_device_name - (size_t)(*ppos)) : 0;

#if defined(IMM_VIB_DEBUG)
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 20))
		printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif

	if (0 == size)
		return 0;

	if (0 != copy_to_user(buf, dev->device_name + (*ppos), size))	{
		printk(KERN_ERR "[VIB] copy_to_user failed.\n");
		return 0;
	}

	*ppos += size;
	return size;
}

static ssize_t imm_vib_write(struct file *file, const char *buf, size_t count,
	loff_t *ppos)
{
	struct miscdevice *miscdev = g_miscdev;//file->private_data;
	struct imm_vib_dev *dev = container_of(miscdev,
		struct imm_vib_dev, miscdev);
#if defined(IMM_VIB_DEBUG)
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 20))
		printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif

	*ppos = 0;

	if (file->private_data != (void *)IMM_VIB_MAGIC_NUMBER) {
		printk(KERN_ERR "[VIB] unauthorized write.\n");
		return 0;
	}

	if ((count < SPI_HEADER_SIZE) || (count > SPI_BUFFER_SIZE)) {
		printk(KERN_INFO "[VIB] invalid write buffer size : %d\n", (int)count);
		return 0;
	}

	if (0 != copy_from_user(dev->write_buff, buf, count)) {
		printk(KERN_ERR "[VIB] copy_from_user failed.\n");
		return 0;
	}

	save_output_data(dev, dev->write_buff, count);

	dev->is_playing = true;
	imm_vib_start_timer(dev);

	return count;
}

static long imm_vib_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct miscdevice *miscdev = g_miscdev;//file->private_data;
	struct imm_vib_dev *dev = container_of(miscdev,
		struct imm_vib_dev, miscdev);

#if defined(IMM_VIB_DEBUG)
	printk(KERN_DEBUG "[VIB] ioctl cmd[0x%x].\n", cmd);
#endif

	switch (cmd) {
	case IMM_VIB_MAGIC_NUMBER:
	case IMM_VIB_SET_MAGIC_NUMBER:
		file->private_data = (void *)IMM_VIB_MAGIC_NUMBER;
		break;

	case IMM_VIB_ENABLE_AMP:
		wake_lock(&dev->wlock);
		imm_vib_enable(dev, arg);
		break;

	case IMM_VIB_DISABLE_AMP:
		imm_vib_disable(dev, arg);
		wake_unlock(&dev->wlock);
		break;

	case IMM_VIB_GET_NUM_ACTUATORS:
		return dev->num_actuators;

#if 0
	case IMM_VIB_SET_DEVICE_PARAMETER:
		{
                device_parameter deviceParam;

                if (0 != copy_from_user((void *)&deviceParam, (const void __user *)arg, sizeof(deviceParam)))
                {
                    /* Error copying the data */
                    printk(KERN_DEBUG "[VIB] copy_from_user failed to copy kernel parameter data.\n");
                    return -1;
                }

                switch (deviceParam.nDeviceParamID)
                {
                    case VIBE_KP_CFG_UPDATE_RATE_MS:
                        /* Update the timer period */
                        g_nTimerPeriodMs = deviceParam.nDeviceParamValue;

#ifdef CONFIG_HIGH_RES_TIMERS
                        /* For devices using high resolution timer we need to update the ktime period value */
                        dev->timer_ms = ktime_set(0, g_nTimerPeriodMs * 1000000);
#endif
                        break;

                    case VIBE_KP_CFG_FREQUENCY_PARAM1:
                    case VIBE_KP_CFG_FREQUENCY_PARAM2:
                    case VIBE_KP_CFG_FREQUENCY_PARAM3:
                    case VIBE_KP_CFG_FREQUENCY_PARAM4:
                    case VIBE_KP_CFG_FREQUENCY_PARAM5:
                    case VIBE_KP_CFG_FREQUENCY_PARAM6:
                        if (0 > ImmVibeSPI_ForceOut_SetFrequency(deviceParam.nDeviceIndex, deviceParam.nDeviceParamID, deviceParam.nDeviceParamValue))
                        {
                            DbgOutErr(("tspdrv: cannot set device frequency parameter.\n"));
                            return -1;
                        }
                        break;
                }
            }
#endif
	}

	return 0;
}

static const struct file_operations imm_vib_fops = {
	.owner = THIS_MODULE,
	.read = imm_vib_read,
	.write = imm_vib_write,
	.unlocked_ioctl = imm_vib_ioctl,
	.compat_ioctl = imm_vib_ioctl,
	.open = imm_vib_open,
	.release = imm_vib_release,
	.llseek =	default_llseek
};

int imm_vib_register(struct imm_vib_dev *dev)
{
	int ret = 0, i = 0;
	char *name = dev->device_name;

	printk(KERN_DEBUG "[VIB] %s\n", __func__);

	dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	dev->miscdev.name = MODULE_NAME;
	dev->miscdev.fops = &imm_vib_fops;
	g_miscdev = &dev->miscdev;
	ret = misc_register(&dev->miscdev);
	if (ret) {
		printk(KERN_ERR "[VIB] misc_register failed.\n");
		return ret;
	}

#if defined(IMM_VIB_TIMER)
	setup_timer(&dev->timer,
		imm_vib_timer, (unsigned long)dev);
	INIT_WORK(&dev->work, imm_vib_work_func);
	dev->sem = &sem;
#endif

	dev->amp_enabled = false;
	dev->motor_enabled = 0;
	dev->timer_ms = 5;
	imm_vib_get_name(dev, i, name, IMM_VIB_NAME_SIZE);

	strcat(name, VERSION_STR);
	dev->cch_device_name += strlen(name);
	dev->actuator.output = 0;
	dev->actuator.empty = true;

	wake_lock_init(&dev->wlock, WAKE_LOCK_SUSPEND, "vib_present");
	return 0;
}

void imm_vib_unregister(struct imm_vib_dev *dev)
{
	int i = 0;

	printk(KERN_DEBUG "[VIB] %s\n", __func__);

	imm_vib_stop_timer(dev);
#if defined(IMM_VIB_TIMER)
	if (imm_vib_lock(dev->sem))
		up(dev->sem);
#endif
	for (i = 0; dev->num_actuators > i; i++)
		imm_vib_disable(dev, i);

	wake_lock_destroy(&dev->wlock);

	misc_deregister(&dev->miscdev);
}

MODULE_DESCRIPTION("Immersion TouchSense Kernel Module");
MODULE_LICENSE("GPL v2");
