/*
 *  SPI Driver Interface Functions
 *
 *  This file contains the SPI driver interface functions.
 *
 *  Copyright (C) 2011-2013 Validity Sensors, Inc.
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "fingerprint.h"

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/poll.h>

#include <asm-generic/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/jiffies.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/pinctrl/consumer.h>
#include "../pinctrl/core.h"
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/cpufreq.h>
#endif

#include "vfs8xxx_egis.h"

#ifdef CONFIG_OF
static const struct of_device_id fps_match_table[] = {
	{.compatible = "fps,common",},
	{},
};
#else
#define fps_match_table NULL
#endif

static int fps_send_drdy_signal(struct fps_device_data *fps_device)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (fps_device->t) {
		/* notify DRDY signal to user process */
		ret = send_sig_info(fps_device->signal_id,
				    (struct siginfo *)1, fps_device->t);
		if (ret < 0)
			pr_err("%s Error sending signal\n", __func__);

	} else
		pr_err("%s task_struct is not received yet\n", __func__);

	return ret;
}

static ssize_t fps_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *fPos)
{
	return 0;
}

static ssize_t fps_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *fPos)
{
	return 0;

}

static void fps_pin_control(struct fps_device_data *fps_device,
			       bool pin_set)
{
	int status = 0;

	fps_device->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(fps_device->pins_idle)) {
			status = pinctrl_select_state(fps_device->p,
						      fps_device->pins_idle);
			if (status)
				pr_err("%s: can't set pin default state\n",
				       __func__);
			pr_debug("%s idle\n", __func__);
		}
	} else {
		if (!IS_ERR(fps_device->pins_sleep)) {
			status = pinctrl_select_state(fps_device->p,
						      fps_device->
						      pins_sleep);
			if (status)
				pr_err("%s: can't set pin sleep state\n",
				       __func__);
			pr_debug("%s sleep\n", __func__);
		}
	}
}

static void fps_power_set(struct fps_device_data *fps_device, int status)
{
	int rc = 0;
	if (fps_device->ldo_pin) {
		pr_info("ldo, status %d\n", status);
		if (status == 1) {
			if (fps_device->ldo_pin) {
				gpio_set_value(fps_device->ldo_pin, status);
				fps_device->ldo_onoff = status;
			}
		} else {
			if (fps_device->ldo_pin) {
				gpio_set_value(fps_device->ldo_pin, status);
				fps_device->ldo_onoff = status;
			}
		}
	} else if (fps_device->regulator_3p3 != NULL) {
		pr_info("regulator, status %d\n", status);
		if (status == 1) {
			if (regulator_is_enabled(fps_device->regulator_3p3) == 0) {
				rc = regulator_enable(fps_device->regulator_3p3);
				if (rc)
					pr_err("regulator enable failed, rc=%d\n", rc);
				else
					fps_device->ldo_onoff = status;
			} else
				pr_info("regulator is already enabled\n");
		} else {
			if (regulator_is_enabled(fps_device->regulator_3p3)) {
				rc = regulator_disable(fps_device->regulator_3p3);
				if (rc)
					pr_err("regulator disable failed, rc=%d\n", rc);
				else
					fps_device->ldo_onoff = status;
			} else
				pr_info("regulator is already disabled\n");
		}
	} else {
		pr_info("This HW revision does not support a power control\n");
	}
}

static int fps_set_clk(struct fps_device_data *fps_device,
			  unsigned long arg)
{
	int ret_val = 0;
	unsigned short clock = 0;
	struct spi_device *spidev = NULL;

	if (copy_from_user(&clock, (void *)arg, sizeof(unsigned short)) != 0)
		return -EFAULT;

	spin_lock_irq(&fps_device->irq_lock);

	spidev = spi_dev_get(fps_device->spi);

	spin_unlock_irq(&fps_device->irq_lock);
	if (spidev != NULL) {
		switch (clock) {
		case 0:	/* Running baud rate. */
			pr_debug("%s Running baud rate.\n", __func__);
			spidev->max_speed_hz = MAX_BAUD_RATE;
			fps_device->current_spi_speed = MAX_BAUD_RATE;
			break;
		case 0xFFFF:	/* Slow baud rate */
			pr_debug("%s slow baud rate.\n", __func__);
			spidev->max_speed_hz = SLOW_BAUD_RATE;
			fps_device->current_spi_speed = SLOW_BAUD_RATE;
			break;
		default:
			pr_debug("%s baud rate is %d.\n", __func__, clock);
			fps_device->current_spi_speed =
			    clock * BAUD_RATE_COEF;
			if (fps_device->current_spi_speed > MAX_BAUD_RATE)
				fps_device->current_spi_speed =
				    MAX_BAUD_RATE;
			spidev->max_speed_hz = fps_device->current_spi_speed;
			break;
		}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
		if (!fps_device->enabled_clk) {
			wake_lock(&fps_device->fp_spi_lock);
			fps_device->enabled_clk = true;
		}
#else
		pr_info("%s, clk speed: %d\n", __func__,
				fps_device->current_spi_speed);
#endif
		spi_dev_put(spidev);
	}
	return ret_val;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int fps_ioctl_disable_spi_clock(struct fps_device_data
					  *fps_device)
{
	if (fps_device->enabled_clk) {
		wake_unlock(&fps_device->fp_spi_lock);
		fps_device->enabled_clk = false;
	}
	return 0;
}
#endif

static int fps_register_drdy_signal(struct fps_device_data *fps_device,
				       unsigned long arg)
{
	struct fps_ioctl_register_signal usr_signal;

	if (copy_from_user(&usr_signal, (void *)arg, sizeof(usr_signal)) == 0) {
		fps_device->user_pid = usr_signal.user_pid;
		fps_device->signal_id = usr_signal.signal_id;
		rcu_read_lock();
		/* find the task_struct associated with userpid */
		fps_device->t =
		    pid_task(find_pid_ns(fps_device->user_pid, &init_pid_ns),
			     PIDTYPE_PID);
		if (fps_device->t == NULL) {
			pr_debug("%s No such pid\n", __func__);
			rcu_read_unlock();
			return -ENODEV;
		}
		rcu_read_unlock();
		pr_info("%s Searching task successfully\n", __func__);
	} else {
		pr_err("%s Failed copy from user.\n", __func__);
		return -EFAULT;
	}
	return 0;
}

static int fps_enable_irq(struct fps_device_data *fps_device)
{
	pr_info("%s\n", __func__);
	spin_lock_irq(&fps_device->irq_lock);
	if (atomic_read(&fps_device->irq_enabled)
	    == DRDY_IRQ_ENABLE) {
		spin_unlock_irq(&fps_device->irq_lock);
		pr_info("%s DRDY irq already enabled\n", __func__);
		return -EINVAL;
	}
	fps_pin_control(fps_device, true);
	enable_irq(gpio_irq);
	enable_irq_wake(gpio_irq);
	atomic_set(&fps_device->irq_enabled, DRDY_IRQ_ENABLE);
	fps_device->cnt_irq++;
	spin_unlock_irq(&fps_device->irq_lock);
	return 0;
}

static int fps_disable_irq(struct fps_device_data *fps_device)
{
	pr_info("%s\n", __func__);
	spin_lock_irq(&fps_device->irq_lock);
	if (atomic_read(&fps_device->irq_enabled) == DRDY_IRQ_DISABLE) {
		spin_unlock_irq(&fps_device->irq_lock);
		pr_info("%s DRDY irq already disabled\n", __func__);
		return -EINVAL;
	}
	disable_irq_nosync(gpio_irq);
	disable_irq_wake(gpio_irq);
	atomic_set(&fps_device->irq_enabled, DRDY_IRQ_DISABLE);
	fps_pin_control(fps_device, false);
	fps_device->cnt_irq--;
	spin_unlock_irq(&fps_device->irq_lock);
	return 0;
}

static irqreturn_t fps_irq(int irq, void *context)
{
	struct fps_device_data *fps_device = context;

	/* Linux kernel is designed so that when you disable
	 * an edge-triggered interrupt, and the edge happens while
	 * the interrupt is disabled, the system will re-play the
	 * interrupt at enable time.
	 * Therefore, we are checking DRDY GPIO pin state to make sure
	 * if the interrupt handler has been called actually by DRDY
	 * interrupt and it's not a previous interrupt re-play
	 */
	if (fps_device->egis_sensor) {
		fps_device->cnt_irq++;
		fps_device->finger_on = 1;
		disable_irq_nosync(gpio_irq);
		wake_up_interruptible(&interrupt_waitq);
		wake_lock_timeout(&fps_device->fp_signal_lock, 1 * HZ);
		pr_info("%s FPS triggered.int_count(%d) On(%d)\n", __func__,
			fps_device->cnt_irq, fps_device->finger_on);
	} else {
		if (gpio_get_value(fps_device->drdy_pin) == DRDY_ACTIVE_STATUS) {
			spin_lock(&fps_device->irq_lock);
			if (atomic_read(&fps_device->irq_enabled)
					== DRDY_IRQ_ENABLE) {
				disable_irq_nosync(gpio_irq);
				disable_irq_wake(gpio_irq);
				atomic_set(&fps_device->irq_enabled,
					   DRDY_IRQ_DISABLE);
				fps_pin_control(fps_device, false);
				fps_device->cnt_irq--;
				spin_unlock(&fps_device->irq_lock);
				fps_send_drdy_signal(fps_device);
				wake_lock_timeout(&fps_device->fp_signal_lock,
						3 * HZ);
				pr_info("%s disable_irq\n", __func__);
			} else {
				spin_unlock(&fps_device->irq_lock);
				pr_info("%s irq already diabled\n", __func__);
			}
		}
	}
	return IRQ_HANDLED;
}

int fps_Interrupt_Init(
		struct fps_device_data *fps_device,
		int int_ctrl,
		int detect_period,
		int detect_threshold)
{
	int status = 0;

	fps_device->finger_on = 0;
	fps_device->cnt_irq = 0;
	pr_info("%s int_ctrl = %d detect_period = %d detect_threshold = %d\n",
				__func__,
				int_ctrl,
				detect_period,
				detect_threshold);

	fps_device->detect_period = detect_period;
	fps_device->detect_threshold = detect_threshold;
	gpio_irq = gpio_to_irq(fps_device->drdy_pin);

	if (gpio_irq < 0) {
		pr_err("%s gpio_to_irq failed\n", __func__);
		status = gpio_irq;
		goto done;
	}

	if (fps_device->egis_sensor) { /* register irq handler for Egis */
		if (fps_device->drdy_irq_flag == DRDY_IRQ_DISABLE) {
			if (request_irq(gpio_irq, fps_irq, int_ctrl,
					"fps_irq", fps_device) < 0) {
				pr_err("%s request_irq failed\n", __func__);
				status = -EBUSY;
				goto done;
			} else {
				enable_irq_wake(gpio_irq);
				fps_device->drdy_irq_flag = DRDY_IRQ_ENABLE;
			}
		}
	}
done:
	return status;
}

int fps_Interrupt_Free(struct fps_device_data *fps_device)
{
	pr_info("%s\n", __func__);

	if (fps_device != NULL) {
		if (fps_device->drdy_irq_flag == DRDY_IRQ_ENABLE) {
			if (!fps_device->cnt_irq)
				disable_irq_nosync(gpio_irq);

			disable_irq_wake(gpio_irq);
			free_irq(gpio_irq, fps_device);
			fps_device->drdy_irq_flag = DRDY_IRQ_DISABLE;
		}
		fps_device->finger_on = 0;
		fps_device->cnt_irq = 0;
	}
	return 0;
}

void fps_Interrupt_Abort(struct fps_device_data *fps_device)
{
	wake_up_interruptible(&interrupt_waitq);
}

unsigned int fps_interrupt_poll(
		struct file *file,
		struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct fps_device_data *fps_device = file->private_data;

	if (!fps_device->egis_sensor)
		return 0;

	pr_debug("%s FPS fps_interrupt_poll, finger_on(%d), int_count(%d)\n",
		__func__, fps_device->finger_on, fps_device->cnt_irq);

	if (!fps_device->finger_on)
		poll_wait(file, &interrupt_waitq, wait);

	if (fps_device->finger_on) {
		mask |= POLLIN | POLLRDNORM;
		fps_device->finger_on = 0;
	}
	return mask;
}

static void fps_reset(struct fps_device_data *fps_device)
{
	pr_info("%s\n", __func__);

	fps_power_set(fps_device, 0);
	usleep_range(1100, 1150);
	fps_power_set(fps_device, 1);
	usleep_range(12000, 12050);
}

static void fps_power_control(struct fps_device_data *fps_device, int status)
{
	pr_info("%s status = %d\n", __func__, status);
	if (status == 1) {
		fps_power_set(fps_device, 1);
		fps_pin_control(fps_device, true);
		usleep_range(1600, 1650);
		if (fps_device->sleep_pin)
			gpio_set_value(fps_device->sleep_pin, 1);
		usleep_range(12000, 12050);
	} else if (status == 0) {
		if (fps_device->sleep_pin)
			gpio_set_value(fps_device->sleep_pin, 0);
		fps_power_set(fps_device, 0);
		fps_pin_control(fps_device, false);
	} else {
		pr_err("%s can't support this value. %d\n", __func__, status);
	}
	fps_device->ldo_onoff = status;
}

static int fps_set_drdy_int(struct fps_device_data *fps_device,
			       unsigned long arg)
{
	unsigned short drdy_enable_flag;

	if (copy_from_user(&drdy_enable_flag, (void *)arg,
			   sizeof(drdy_enable_flag)) != 0) {
		pr_err("%s Failed copy from user.\n", __func__);
		return -EFAULT;
	}
	if (drdy_enable_flag == 0)
		fps_disable_irq(fps_device);
	else {
		fps_enable_irq(fps_device);

		/* Workaround the issue where the system
		 * misses DRDY notification to host when
		 * DRDY pin was asserted before enabling
		 * device.
		 */

		if (gpio_get_value(fps_device->drdy_pin) ==
		    DRDY_ACTIVE_STATUS) {
			pr_info("%s drdy pin is already active atatus\n",
				__func__);
			fps_send_drdy_signal(fps_device);
		}
	}
	return 0;
}

static void fps_regulator_onoff(struct fps_device_data *fps_device,
				   bool onoff)
{
	if (fps_device->ldo_pin) {
		if (onoff) {
			if (fps_device->sleep_pin)
				gpio_set_value(fps_device->sleep_pin, 1);
			gpio_set_value(fps_device->ldo_pin, 1);
		} else {
			gpio_set_value(fps_device->ldo_pin, 0);
			if (fps_device->sleep_pin)
				gpio_set_value(fps_device->sleep_pin, 0);
		}
		fps_device->ldo_onoff = onoff;
		pr_info("%s:ldo %s\n", __func__, onoff ? "on" : "off");
	} else if (fps_device->regulator_3p3) {
		int rc = 0;

		if (onoff) {
			if (fps_device->sleep_pin)
				gpio_set_value(fps_device->sleep_pin, 1);
			rc = regulator_enable(fps_device->regulator_3p3);
			if (rc) {
				pr_err("%s enable btp ldo enable failed, rc=%d\n",
						__func__, rc);
				goto done;
			}
		} else {
			rc = regulator_disable(fps_device->regulator_3p3);
			if (rc) {
				pr_err("%s enable btp ldo enable failed, rc=%d\n",
						__func__, rc);
				goto done;
			}
			if (fps_device->sleep_pin)
				gpio_set_value(fps_device->sleep_pin, 0);
		}
		fps_device->ldo_onoff = onoff;
done:
		pr_info("%s:regulator %s\n", __func__,
			fps_device->ldo_onoff ? "on" : "off");
	} else {
		pr_info("%s: can't control in this revion\n", __func__);
	}

}

static void fps_hardReset(struct fps_device_data *fps_device)
{
	pr_info("%s\n", __func__);
}

static void fps_suspend(struct fps_device_data *fps_device)
{
	pr_info("%s\n", __func__);
}

static void fps_ioctl_power_on(struct fps_device_data *fps_device)
{
	if (!fps_device->ldo_onoff)
		fps_regulator_onoff(fps_device, true);
	else
		pr_info("%s already on\n", __func__);
}

static void fps_ioctl_power_off(struct fps_device_data *fps_device)
{
	if (fps_device->ldo_onoff)
		fps_regulator_onoff(fps_device, false);
	else
		pr_info("%s already off\n", __func__);
}

static long fps_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct fps_device_data *fps_device = NULL;

	pr_debug("%s\n", __func__);
	fps_device = filp->private_data;

	if (_IOC_TYPE(cmd) == VFSSPI_IOCTL_MAGIC) { /* Namsan ioctl */
		int ret_val = 0;
		unsigned int onoff = 0;

#ifdef ENABLE_SENSORS_FPRINT_SECURE
		unsigned int type_check = -1;
#endif
		mutex_lock(&fps_device->buffer_mutex);

		switch (cmd) {
			case VFSSPI_IOCTL_DEVICE_RESET:
				pr_debug("%s DEVICE_RESET\n", __func__);
				fps_hardReset(fps_device);
				break;
			case VFSSPI_IOCTL_DEVICE_SUSPEND:
				pr_debug("%s DEVICE_SUSPEND\n", __func__);
				fps_suspend(fps_device);
				break;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
			case VFSSPI_IOCTL_RW_SPI_MESSAGE:
				pr_debug("%s RW_SPI_MESSAGE\n", __func__);
				ret_val = fps_rw_spi_message(fps_device, arg);
				if (ret_val) {
					pr_err("%s RW_SPI_MESSAGE error %d\n",
					       __func__, ret_val);
				}
				break;
#endif
			case VFSSPI_IOCTL_SET_CLK:
				pr_info("%s SET_CLK\n", __func__);
				ret_val = fps_set_clk(fps_device, arg);
				break;
			case VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL:
				pr_info("%s REGISTER_DRDY_SIGNAL\n", __func__);
				ret_val = fps_register_drdy_signal(fps_device, arg);
				break;
			case VFSSPI_IOCTL_SET_DRDY_INT:
				pr_info("%s SET_DRDY_INT\n", __func__);
				ret_val = fps_set_drdy_int(fps_device, arg);
				break;
			case VFSSPI_IOCTL_POWER_ON:
				pr_info("%s POWER_ON\n", __func__);
				fps_ioctl_power_on(fps_device);
				break;
			case VFSSPI_IOCTL_POWER_OFF:
				pr_info("%s POWER_OFF\n", __func__);
				fps_ioctl_power_off(fps_device);
				break;
			case VFSSPI_IOCTL_POWER_CONTROL:
				pr_info("%s POWER_CONTROL\n", __func__);
				if (copy_from_user(&onoff, (void *)arg,
						sizeof(unsigned int)) != 0) {
					pr_err("%s Failed copy from user.(POWER_CONTROL)\n",
							__func__);
					mutex_unlock(&fps_device->buffer_mutex);
					return -EFAULT;
				}
				fps_regulator_onoff(fps_device, onoff);
				break;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
			case VFSSPI_IOCTL_DISABLE_SPI_CLOCK:
				pr_info("%s DISABLE_SPI_CLOCK\n", __func__);
				ret_val = fps_ioctl_disable_spi_clock(fps_device);
				break;

			case VFSSPI_IOCTL_SET_SPI_CONFIGURATION:
				pr_info("%s SET_SPI_CONFIGURATION\n", __func__);
				break;
			case VFSSPI_IOCTL_RESET_SPI_CONFIGURATION:
				pr_info("%s RESET_SPI_CONFIGURATION\n", __func__);
				break;
			case VFSSPI_IOCTL_CPU_SPEEDUP:
				if (copy_from_user(&onoff, (void *)arg,
						   sizeof(unsigned int)) != 0) {
					pr_err("%s Failed copy from user.(CPU_SPEEDUP)\n",
					       __func__);
					mutex_unlock(&fps_device->buffer_mutex);
					return -EFAULT;
				}
				if (onoff == 1) {
					u8 retry_cnt = 0;

					pr_info("%s CPU_SPEEDUP ON:%d, retry: %d\n",
							__func__, onoff, retry_cnt);
					if (fps_device->min_cpufreq_limit) {
						do {
							ret_val = set_freq_limit(DVFS_FINGER_ID,
								fps_device->min_cpufreq_limit);
							retry_cnt++;
							if (ret_val) {
								pr_err("%s: clock speed up start failed. (%d) retry: %d\n",
									__func__, ret_val,
									retry_cnt);
								usleep_range(500, 510);
							}
						} while (ret_val && retry_cnt < 7);
					}

				} else if (onoff == 0) {
					pr_info("%s CPU_SPEEDUP OFF\n", __func__);
					if (fps_device->min_cpufreq_limit) {
						ret_val = set_freq_limit(DVFS_FINGER_ID, -1);
						if (ret_val)
							pr_err("%s: clock speed up stop failed. (%d)\n",
									__func__, ret_val);
					}
				}
				break;
			case VFSSPI_IOCTL_SET_SENSOR_TYPE:
				if (copy_from_user(&type_check, (void *)arg,
						   sizeof(unsigned int)) != 0) {
					pr_err("%s Failed copy from user.(SET_SENSOR_TYPE)\n",
					       __func__);
					mutex_unlock(&fps_device->buffer_mutex);
					return -EFAULT;
				}
				if ((int)type_check >= SENSOR_OOO &&
						(int)type_check < SENSOR_MAXIMUM) {
					if ((int)type_check == SENSOR_OOO &&
						fps_device->sensortype == SENSOR_FAILED) {
						pr_info("%s Maintain type check from out of order :%s\n",
							__func__,
							sensor_status[g_data->sensortype + 2]);
					} else {
						fps_device->sensortype = (int)type_check;
						pr_info("%s SET_SENSOR_TYPE :%s", __func__,
							sensor_status[g_data->sensortype + 2]);

						/* for namsan */
						fps_device->egis_sensor = 0;
						fps_device->orient = 0;

						/* register irq handler for Namsan */
						gpio_irq = gpio_to_irq(fps_device->drdy_pin);
						if (gpio_irq < 0) {
							pr_err("%s gpio_to_irq failed\n", __func__);
						}
						if (request_irq(gpio_irq, fps_irq, IRQF_TRIGGER_RISING,
								"fps_irq", fps_device) < 0) {
							pr_err("%s request_irq failed\n", __func__);
						}
					}
				} else {
					pr_err("%s SET_SENSOR_TYPE : invalid value %d\n",
					     __func__, (int)type_check);
					fps_device->sensortype = SENSOR_UNKNOWN;
				}
				break;
			case VFSSPI_IOCTL_SET_LOCKSCREEN:
				pr_info("%s SET_LOCKSCREEN\n", __func__);
				break;
#endif
			case VFSSPI_IOCTL_GET_SENSOR_ORIENT:
				pr_info("%s: orient is %d(0: normal, 1: upsidedown)\n",
					__func__, fps_device->orient);
				if (copy_to_user((void *)arg,
						 &(fps_device->orient),
						 sizeof(fps_device->orient))
				    != 0) {
					ret_val = -EFAULT;
					pr_err("%s cp to user fail\n", __func__);
				}
				break;

			default:
				pr_info("%s default error. %u\n", __func__, cmd);
				ret_val = -EFAULT;
				break;
		}
		mutex_unlock(&fps_device->buffer_mutex);
		return ret_val;

	} else if (_IOC_TYPE(cmd) == EGIS_IOC_MAGIC) { /* Egis ioctl */
		int err = 0, retval = 0;
		struct spi_device *spi;
		u32 tmp;
		struct fps_ioc_transfer *ioc = NULL;

		/* Check access direction once here; don't repeat below.
		 * IOC_DIR is from the user perspective, while access_ok is
		 * from the kernel perspective; so they look reversed.
		 */
		if (_IOC_DIR(cmd) & _IOC_READ)
			err = !access_ok(VERIFY_WRITE,
							(void __user *)arg,
							_IOC_SIZE(cmd));
		if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
			err = !access_ok(VERIFY_READ,
							(void __user *)arg,
							_IOC_SIZE(cmd));
		if (err) {
			pr_err("%s err", __func__);
			return -EFAULT;
		}

		/* guard against device removal before, or while,
		 * we issue this ioctl.
		 */
		spin_lock_irq(&fps_device->irq_lock);
		spi = spi_dev_get(fps_device->spi);
		spin_unlock_irq(&fps_device->irq_lock);

		if (spi == NULL) {
			pr_err("%s spi == NULL", __func__);
			return -ESHUTDOWN;
		}

		mutex_lock(&fps_device->buffer_mutex);

		/* segmented and/or full-duplex I/O request */
		if (_IOC_NR(cmd) != _IOC_NR(EGIS_IOC_MESSAGE(0))
						|| _IOC_DIR(cmd) != _IOC_WRITE) {
			retval = -ENOTTY;
			goto out;
		}

		tmp = _IOC_SIZE(cmd);
		if ((tmp == 0) || (tmp % sizeof(struct fps_ioc_transfer)) != 0) {
			retval = -EINVAL;
			goto out;
		}
		/* copy into scratch area */
		ioc = kmalloc(tmp, GFP_KERNEL);
		if (!ioc) {
			pr_err("%s kmalloc error\n", __func__);
			retval = -ENOMEM;
			goto out;
		}
		if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
			pr_err("%s __copy_from_user error\n", __func__);
			retval = -EFAULT;
			goto out;
		}

		switch (ioc->opcode) {
			case FP_REGISTER_READ:
				break;
			case FP_REGISTER_WRITE:
				pr_debug("%s FP_REGISTER_WRITE\n", __func__);
				break;
			case FP_REGISTER_MREAD:
				pr_debug("%s FP_REGISTER_MREAD\n", __func__);
				break;
			case FP_REGISTER_BREAD:
				pr_debug("%s FP_REGISTER_BREAD\n", __func__);
				break;
			case FP_REGISTER_BWRITE:
				pr_debug("%s FP_REGISTER_BWRITE\n", __func__);
				break;
			case FP_REGISTER_BREAD_BACKWARD:
				pr_debug("%s FP_REGISTER_BREAD_BACKWARD\n", __func__);
				break;
			case FP_REGISTER_BWRITE_BACKWARD:
				pr_debug("%s FP_REGISTER_BWRITE_BACKWARD\n", __func__);
				break;
			case FP_NVM_READ:
				pr_debug("%s FP_NVM_READ, (%d)\n", __func__, spi->max_speed_hz);
				break;
			case FP_NVM_WRITE:
				pr_debug("%s FP_NVM_WRITE, (%d)\n",
						__func__, spi->max_speed_hz);
				break;
			case FP_NVM_WRITEEX:
				pr_debug("%s FP_NVM_WRITEEX, (%d)\n",
						__func__, spi->max_speed_hz);
				break;
			case FP_NVM_OFF:
				pr_debug("%s FP_NVM_OFF\n", __func__);
				break;
			case FP_VDM_READ:
				pr_debug("%s FP_VDM_READ\n", __func__);
				break;
			case FP_VDM_WRITE:
				pr_debug("%s FP_VDM_WRITE\n", __func__);
				break;
			case FP_GET_ONE_IMG:
				pr_debug("%s FP_GET_ONE_IMG\n", __func__);
				break;

			case FP_SENSOR_RESET:
				pr_info("%s FP_SENSOR_RESET\n", __func__);
				fps_reset(fps_device);
				break;

			case FP_RESET_SET:
				break;

			case FP_POWER_CONTROL:
			case FP_POWER_CONTROL_ET5XX:
				pr_info("%s FP_POWER_CONTROL, status = %d\n",
						__func__, ioc->len);
				fps_power_control(fps_device, ioc->len);
				break;

			case FP_SET_SPI_CLOCK:
				pr_info("%s FP_SET_SPI_CLOCK, clock = %d\n",
						__func__, ioc->speed_hz);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
				if (fps_device->enabled_clk) {
					if (spi->max_speed_hz != ioc->speed_hz) {
						pr_info("%s already enabled. DISABLE_SPI_CLOCK\n",
							__func__);
						wake_unlock(&fps_device->fp_spi_lock);
						fps_device->enabled_clk = false;
					} else {
						pr_info("%s already enabled same clock.\n",
							__func__);
						break;
					}
				}
				spi->max_speed_hz = ioc->speed_hz;
				wake_lock(&fps_device->fp_spi_lock);
				fps_device->enabled_clk = true;
#else
				spi->max_speed_hz = ioc->speed_hz;
#endif
				break;

			case INT_TRIGGER_INIT:
				pr_debug("%s Trigger function init\n", __func__);
				retval = fps_Interrupt_Init(
						fps_device,
						(int)ioc->pad[0],
						(int)ioc->pad[1],
						(int)ioc->pad[2]);
				break;
			/* trigger */
			case INT_TRIGGER_CLOSE:
				pr_debug("%s Trigger function close\n", __func__);
				retval = fps_Interrupt_Free(fps_device);
				break;
			/* Poll Abort */
			case INT_TRIGGER_ABORT:
				pr_debug("%s Trigger function abort\n", __func__);
				fps_Interrupt_Abort(fps_device);
				break;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
			case FP_DISABLE_SPI_CLOCK:
				pr_info("%s FP_DISABLE_SPI_CLOCK\n", __func__);

				if (fps_device->enabled_clk) {
					pr_info("%s DISABLE_SPI_CLOCK\n", __func__);
					wake_unlock(&fps_device->fp_spi_lock);
					fps_device->enabled_clk = false;
				}
				break;
			case FP_CPU_SPEEDUP:
				pr_info("%s FP_CPU_SPEEDUP\n", __func__);
				if (ioc->len) {
					u8 retry_cnt = 0;

					pr_info("%s FP_CPU_SPEEDUP ON:%d, retry: %d\n",
							__func__, ioc->len, retry_cnt);
					if (fps_device->min_cpufreq_limit) {
						do {
							retval = set_freq_limit(DVFS_FINGER_ID, fps_device->min_cpufreq_limit);
							retry_cnt++;
							if (retval) {
								pr_err("%s: booster start failed. (%d) retry: %d\n"
									, __func__, retval, retry_cnt);
								if (retry_cnt < 7)
									usleep_range(500, 510);
							}
						} while (retval && retry_cnt < 7);
					}
				} else {
					pr_info("%s FP_CPU_SPEEDUP OFF\n", __func__);
					retval = set_freq_limit(DVFS_FINGER_ID, -1);
					if (retval)
						pr_err("%s: booster stop failed. (%d)\n"
							, __func__, retval);
				}
				break;
			case FP_SET_SENSOR_TYPE:
				if ((int)ioc->len >= SENSOR_UNKNOWN
						&& (int)ioc->len < (SENSOR_STATUS_SIZE - 1)) {
					fps_device->sensortype = (int)ioc->len;
					pr_info("%s FP_SET_SENSOR_TYPE :%s\n", __func__,
						sensor_status[g_data->sensortype + 1]);
					/* for egis */
					fps_device->egis_sensor = 1;
					fps_device->orient = 3;
				} else {
					pr_err("%s FP_SET_SENSOR_TYPE invalid value %d\n",
							__func__, (int)ioc->len);
					fps_device->sensortype = SENSOR_UNKNOWN;
				}
				break;
			case FP_SET_LOCKSCREEN:
				pr_info("%s FP_SET_LOCKSCREEN\n", __func__);
				break;
			case FP_SET_WAKE_UP_SIGNAL:
				pr_info("%s FP_SET_WAKE_UP_SIGNAL\n", __func__);
				break;
#endif
			case FP_SENSOR_ORIENT:
				pr_info("orient is %d", fps_device->orient);

				retval = put_user(fps_device->orient, (u8 __user *) (uintptr_t)ioc->rx_buf);
				if (retval != 0)
					pr_err("FP_SENSOR_ORIENT put_user fail: %d\n", retval);
				break;
			case FP_SPI_VALUE:
				fps_device->spi_value = ioc->len;
				pr_info("spi_value: 0x%x\n", fps_device->spi_value);
				break;
			case FP_IOCTL_RESERVED_01:
				break;
			default:
				pr_info("%s undefined case\n", __func__);
				retval = -EFAULT;
				break;
		}

	out:
		if (ioc != NULL)
			kfree(ioc);

		mutex_unlock(&fps_device->buffer_mutex);
		spi_dev_put(spi);
		if (retval < 0)
			pr_err("%s retval = %d\n", __func__, retval);
		return retval;
	} else {
		pr_err("%s it is not egis/namsan ioctl, retval = %d\n", __func__, -ENOTTY);
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
static long fps_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return fps_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define fps_compat_ioctl NULL
#endif
/* CONFIG_COMPAT */

static int fps_open(struct inode *inode, struct file *filp)
{
	struct fps_device_data *fps_device = NULL;
	int status = -ENXIO;

	pr_info("%s\n", __func__);

	mutex_lock(&device_list_mutex);

	list_for_each_entry(fps_device, &device_list, device_entry) {
		if (fps_device->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		fps_device->user_pid = 0;
		filp->private_data = fps_device;
		nonseekable_open(inode, filp);
	}

	mutex_unlock(&device_list_mutex);
	return status;
}

static int fps_release(struct inode *inode, struct file *filp)
{
	struct fps_device_data *fps_device = NULL;
	int status = 0;

	pr_info("%s\n", __func__);

	mutex_lock(&device_list_mutex);
	fps_device = filp->private_data;
	filp->private_data = NULL;

	if (fps_device->ldo_onoff) {
		if (fps_device->egis_sensor) {
			fps_power_control(fps_device, 0);
		} else {
			fps_regulator_onoff(fps_device, false);
		}
	}

	mutex_unlock(&device_list_mutex);
	return status;
}

/* file operations associated with device */
static const struct file_operations fps_fops = {
	.owner = THIS_MODULE,
	.write = fps_write,
	.read = fps_read,
	.unlocked_ioctl = fps_ioctl,
	.compat_ioctl = fps_compat_ioctl,
	.open = fps_open,
	.release = fps_release,
	.poll = fps_interrupt_poll
};

static int fps_platformInit(struct fps_device_data *fps_device)
{
	int status = 0;

	pr_info("%s\n", __func__);

	fps_device->drdy_irq_flag = DRDY_IRQ_DISABLE;

	if (fps_device->ldo_pin) {
		status = gpio_request(fps_device->ldo_pin, "fps_ldo_en");
		if (status < 0) {
			pr_err("%s gpio_request fps_ldo_en failed\n",
				__func__);
			goto fps_platformInit_ldo_failed;
		}
		gpio_direction_output(fps_device->ldo_pin, 0);
	}
	if (gpio_request(fps_device->drdy_pin, "fps_drdy") < 0) {
		status = -EBUSY;
		pr_err("%s gpio_requset fps_sleep failed\n",
				__func__);
		goto fps_platformInit_drdy_failed;
	}

	if (fps_device->sleep_pin) {
		status = gpio_request(fps_device->sleep_pin, "fps_sleep");
		if (status < 0) {
			pr_err("%s gpio_request fps_sleep failed\n",
				__func__);
			goto fps_platformInit_sleep_failed;
		}
		status = gpio_direction_output(fps_device->sleep_pin, 0);
	}
	spin_lock_init(&fps_device->irq_lock);

	status = gpio_direction_input(fps_device->drdy_pin);
	if (status < 0) {
		pr_err("%s gpio_direction_input DRDY failed\n", __func__);
		status = -EBUSY;
		goto fps_platformInit_gpio_init_failed;
	}
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	wake_lock_init(&fps_device->fp_spi_lock,
				WAKE_LOCK_SUSPEND, "fps_wake_lock");
#endif
	wake_lock_init(&fps_device->fp_signal_lock,
				WAKE_LOCK_SUSPEND, "fps_sigwake_lock");

	pr_info("%s : platformInit success!\n", __func__);
	return status;

fps_platformInit_gpio_init_failed:
	if (fps_device->sleep_pin)
		gpio_free(fps_device->sleep_pin);
fps_platformInit_sleep_failed:
	if (fps_device->drdy_pin)
		gpio_free(fps_device->drdy_pin);
fps_platformInit_drdy_failed:
	if (fps_device->ldo_pin)
		gpio_free(fps_device->ldo_pin);
fps_platformInit_ldo_failed:
	pr_info("%s : platformInit failed!\n", __func__);
	return status;
}

static void fps_platformUninit(struct fps_device_data *fps_device)
{
	pr_info("%s\n", __func__);

	if (fps_device != NULL) {
		disable_irq_wake(gpio_irq);
		disable_irq(gpio_irq);
		fps_pin_control(fps_device, false);
		free_irq(gpio_irq, fps_device);
		fps_device->drdy_irq_flag = DRDY_IRQ_DISABLE;
		if (fps_device->sleep_pin)
			gpio_free(fps_device->sleep_pin);
		if (fps_device->drdy_pin)
			gpio_free(fps_device->drdy_pin);
		if (fps_device->ldo_pin)
			gpio_free(fps_device->ldo_pin);
		if (fps_device->regulator_3p3)
			regulator_put(fps_device->regulator_3p3);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		wake_lock_destroy(&fps_device->fp_spi_lock);
#endif
		wake_lock_destroy(&fps_device->fp_signal_lock);
	}
}

static int fps_parse_dt(struct device *dev, struct fps_device_data *fps_device)
{
	struct device_node *np = dev->of_node;
	int errorno = 0;
	int gpio;

	if (!of_find_property(np, "fps-sleepPin", NULL)) {
		pr_info("%s: not set sleepPin in dts\n", __func__);
		fps_device->sleep_pin = 0;
	} else {
		gpio = of_get_named_gpio(np, "fps-sleepPin", 0);
		if (gpio < 0) {
			errorno = gpio;
			goto dt_exit;
		} else {
			fps_device->sleep_pin = gpio;
			pr_info("%s: sleepPin=%d\n", __func__, fps_device->sleep_pin);
		}
	}

	gpio = of_get_named_gpio(np, "fps-drdyPin", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		fps_device->drdy_pin = gpio;
		pr_info("%s: drdyPin=%d\n", __func__, fps_device->drdy_pin);
	}

	gpio = of_get_named_gpio(np, "fps-ldoPin", 0);
	if (gpio < 0) {
		fps_device->ldo_pin = 0;
		pr_info("%s: not use ldo_pin\n", __func__);
	} else {
		fps_device->ldo_pin = gpio;
		pr_info("%s: ldo_pin=%d\n", __func__, fps_device->ldo_pin);
	}

	if (of_property_read_string(np, "fps-regulator", &fps_device->btp_vdd)
			< 0) {
		pr_info("%s: not use btp_regulator\n", __func__);
		fps_device->btp_vdd = 0;
	} else {
		fps_device->regulator_3p3 = regulator_get(NULL, fps_device->btp_vdd);
		if (IS_ERR(fps_device->regulator_3p3) ||
				(fps_device->regulator_3p3) == NULL) {
			fps_device->regulator_3p3 = 0;
			pr_err("%s - btp regulator_get fail\n", __func__);
		} else {
			pr_info("%s: fps_regulator ok\n", __func__);
		}
	}

	if (of_property_read_u32(np, "fps-min_cpufreq_limit",
		&fps_device->min_cpufreq_limit))
		fps_device->min_cpufreq_limit = 0;

	pr_info("%s: min_cpufreq_limit=%d\n",
		__func__, fps_device->min_cpufreq_limit);

	if (of_property_read_string_index(np, "fps-chipid", 0,
			(const char **)&fps_device->chipid))
		fps_device->chipid = '\0';
	pr_info("%s: chipid: %s\n", __func__, fps_device->chipid);

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	fps_device->tz_mode = true;
#endif

	if (of_property_read_u32(np, "fps-orient", &fps_device->orient))
		fps_device->orient = 0;
	pr_info("%s: orient=%d\n", __func__, fps_device->orient);

	fps_device->p = pinctrl_get_select_default(dev);
	if (IS_ERR(fps_device->p)) {
		errorno = -EINVAL;
		pr_err("%s: failed pinctrl_get\n", __func__);
		goto dt_exit;
	}

	fps_device->pins_sleep = pinctrl_lookup_state(fps_device->p, PINCTRL_STATE_SLEEP);
	if (IS_ERR(fps_device->pins_sleep)) {
		errorno = -EINVAL;
		pr_err("%s : could not get pins sleep_state (%li)\n",
		       __func__, PTR_ERR(fps_device->pins_sleep));
		goto fail_pinctrl_get;
	}

	fps_device->pins_idle = pinctrl_lookup_state(fps_device->p, PINCTRL_STATE_IDLE);
	if (IS_ERR(fps_device->pins_idle)) {
		errorno = -EINVAL;
		pr_err("%s : could not get pins idle_state (%li)\n",
		       __func__, PTR_ERR(fps_device->pins_idle));
		goto fail_pinctrl_get;
	}
	return 0;
fail_pinctrl_get:
	pinctrl_put(fps_device->p);
dt_exit:
	return errorno;
}

static ssize_t fps_bfs_values_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct fps_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "\"FP_SPICLK\":\"%d\"\n",
			data->current_spi_speed);
}

static ssize_t fps_type_check_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct fps_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->sensortype);
}

static ssize_t fps_vendor_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fps_device_data *data = dev_get_drvdata(dev);

	if (data->egis_sensor) {
		return sprintf(buf, "%s\n", "EGISTEC");
	} else {
		return sprintf(buf, "%s\n", "SYNAPTICS");
	}
}

static ssize_t fps_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fps_device_data *data = dev_get_drvdata(dev);

	if (data->egis_sensor) {
		return sprintf(buf, "%s\n", "ET510");
	} else {
		return sprintf(buf, "%s\n", "NAMSAN");
	}
}

static ssize_t fps_adm_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", 1);
}

static DEVICE_ATTR(bfs_values, 0444, fps_bfs_values_show, NULL);
static DEVICE_ATTR(type_check, 0444, fps_type_check_show, NULL);
static DEVICE_ATTR(vendor, 0444, fps_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, fps_name_show, NULL);
static DEVICE_ATTR(adm, 0444, fps_adm_show, NULL);

static struct device_attribute *fp_attrs[] = {
	&dev_attr_bfs_values,
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_adm,
	NULL,
};

static void fps_work_func_debug(struct work_struct *work)
{
	pr_info("%s power:%d, irq:%d, tz:%d, type:%s, cnt_irq:%d\n",
		__func__, g_data->ldo_onoff,
		gpio_get_value(g_data->drdy_pin), g_data->tz_mode,
		sensor_status[g_data->sensortype + 2], g_data->cnt_irq);
}

static void fps_enable_debug_timer(void)
{
	mod_timer(&g_data->dbg_timer,
		  round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static void fps_disable_debug_timer(void)
{
	del_timer_sync(&g_data->dbg_timer);
	cancel_work_sync(&g_data->work_debug);
}

static void fps_timer_func(unsigned long ptr)
{
	queue_work(g_data->wq_dbg, &g_data->work_debug);
	mod_timer(&g_data->dbg_timer,
		  round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static int fps_probe(struct spi_device *spi)
{
	int status = 0;
	struct fps_device_data *fps_device;
	struct device *dev;

	pr_info("%s\n", __func__);

	fps_device = kzalloc(sizeof(*fps_device), GFP_KERNEL);

	if (!fps_device)
		return -ENOMEM;

	if (spi->dev.of_node) {
		status = fps_parse_dt(&spi->dev, fps_device);
		if (status) {
			pr_err("%s - Failed to parse DT\n", __func__);
			goto fps_probe_parse_dt_failed;
		}
	}

	/* Initialize driver data. */
	fps_device->egis_sensor = 0;
	fps_device->current_spi_speed = SLOW_BAUD_RATE;
	fps_device->spi = spi;
	g_data = fps_device;

	spin_lock_init(&fps_device->irq_lock);
	mutex_init(&fps_device->buffer_mutex);

	INIT_LIST_HEAD(&fps_device->device_entry);

	status = fps_platformInit(fps_device);
	if (status != 0) {
		pr_err("%s - Failed to platformInit\n", __func__);
		goto fps_probe_platformInit_failed;
	}

	spi->bits_per_word = BITS_PER_WORD;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	fps_device->cnt_irq = 0;

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	status = spi_setup(spi);
	if (status) {
		pr_err("%s : spi_setup failed\n", __func__);
		goto fps_probe_spi_setup_failed;
	}
#endif

	mutex_lock(&device_list_mutex);
	/* Create device node */
	/* register major number for character device */
	status = alloc_chrdev_region(&(fps_device->devt),
				     0, 1, "common_fingerprint");
	if (status < 0) {
		pr_err("%s alloc_chrdev_region failed\n", __func__);
		goto fps_probe_alloc_chardev_failed;
	}

	cdev_init(&(fps_device->cdev), &fps_fops);
	fps_device->cdev.owner = THIS_MODULE;
	status = cdev_add(&(fps_device->cdev), fps_device->devt, 1);
	if (status < 0) {
		pr_err("%s cdev_add failed\n", __func__);
		goto fps_probe_cdev_add_failed;
	}

	fps_device_class = class_create(THIS_MODULE, "common_fingerprint");

	if (IS_ERR(fps_device_class)) {
		pr_err("%s fps_init: class_create() is failed - unregister chrdev.\n",
				__func__);
		status = PTR_ERR(fps_device_class);
		goto fps_probe_class_create_failed;
	}

	dev = device_create(fps_device_class, &spi->dev,
			    fps_device->devt, fps_device, "fps");
	status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	if (status == 0)
		list_add(&fps_device->device_entry, &device_list);
	mutex_unlock(&device_list_mutex);

	if (status != 0)
		goto fps_probe_failed;

	spi_set_drvdata(spi, fps_device);

	status = fingerprint_register(fps_device->fp_device,
				      fps_device, fp_attrs, "fingerprint");
	if (status) {
		pr_err("%s sysfs register failed\n", __func__);
		goto fps_probe_failed;
	}

	/* debug polling function */
	setup_timer(&fps_device->dbg_timer,
		    fps_timer_func, (unsigned long)fps_device);

	fps_device->wq_dbg =
	    create_singlethread_workqueue("fps_debug_wq");
	if (!fps_device->wq_dbg) {
		status = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto fps_sysfs_failed;
	}
	INIT_WORK(&fps_device->work_debug, fps_work_func_debug);

	fps_device->sensortype = SENSOR_UNKNOWN;

	disable_irq(gpio_irq);
	fps_pin_control(fps_device, false);
	fps_enable_debug_timer();

	pr_info("%s successful\n", __func__);
	return 0;

fps_sysfs_failed:
	fingerprint_unregister(fps_device->fp_device, fp_attrs);
fps_probe_failed:
	device_destroy(fps_device_class, fps_device->devt);
	class_destroy(fps_device_class);
fps_probe_class_create_failed:
	cdev_del(&(fps_device->cdev));
fps_probe_cdev_add_failed:
	unregister_chrdev_region(fps_device->devt, 1);
fps_probe_alloc_chardev_failed:
#ifndef ENABLE_SENSORS_FPRINT_SECURE
fps_probe_spi_setup_failed:
#endif
	fps_platformUninit(fps_device);
fps_probe_platformInit_failed:
	pinctrl_put(fps_device->p);
	mutex_destroy(&fps_device->buffer_mutex);
fps_probe_parse_dt_failed:
	kfree(fps_device);
	pr_err("%s fps_probe failed!!\n", __func__);
	return status;
}

static int fps_remove(struct spi_device *spi)
{
	int status = 0;

	struct fps_device_data *fps_device = NULL;

	pr_info("%s\n", __func__);

	fps_device = spi_get_drvdata(spi);

	if (fps_device != NULL) {
		fps_disable_debug_timer();
		spin_lock_irq(&fps_device->irq_lock);
		fps_device->spi = NULL;
		spi_set_drvdata(spi, NULL);
		spin_unlock_irq(&fps_device->irq_lock);

		mutex_lock(&device_list_mutex);

		fps_platformUninit(fps_device);

		fingerprint_unregister(fps_device->fp_device, fp_attrs);

		/* Remove device entry. */
		list_del(&fps_device->device_entry);
		device_destroy(fps_device_class, fps_device->devt);
		class_destroy(fps_device_class);
		cdev_del(&(fps_device->cdev));
		unregister_chrdev_region(fps_device->devt, 1);

		mutex_destroy(&fps_device->buffer_mutex);

		kfree(fps_device);
		mutex_unlock(&device_list_mutex);
	}

	return status;
}

static void fps_shutdown(struct spi_device *spi)
{
	if (g_data != NULL)
		fps_disable_debug_timer();

	pr_info("%s\n", __func__);
}

static int fps_pm_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);
	if (g_data != NULL)
		fps_disable_debug_timer();
	return 0;
}

static int fps_pm_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	if (g_data != NULL)
		fps_enable_debug_timer();
	return 0;
}

static const struct dev_pm_ops fps_pm_ops = {
	.suspend = fps_pm_suspend,
	.resume = fps_pm_resume
};

struct spi_driver fps_spi = {
	.driver = {
		   .name = "common_fingerprint",
		   .owner = THIS_MODULE,
		   .pm = &fps_pm_ops,
		   .of_match_table = fps_match_table,
		   },
	.probe = fps_probe,
	.remove = fps_remove,
	.shutdown = fps_shutdown,
};

static int __init fps_init(void)
{
	int status = 0;

	pr_info("%s fps_init\n", __func__);

	status = spi_register_driver(&fps_spi);
	if (status < 0) {
		pr_err("%s spi_register_driver() failed\n", __func__);
		return status;
	}
	pr_info("%s init is successful\n", __func__);

	return status;
}

static void __exit fps_exit(void)
{
	pr_debug("%s fps_exit\n", __func__);
	spi_unregister_driver(&fps_spi);
}

module_init(fps_init);
module_exit(fps_exit);

MODULE_DESCRIPTION("Validity FPS sensor");
MODULE_LICENSE("GPL");
