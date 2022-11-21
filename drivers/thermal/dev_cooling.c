/*
 *  linux/drivers/thermal/dev_cooling.c
 *
 *  Copyright (C) 2019	Samsung Electronics Co., Ltd(http://www.samsung.com)
 *  Copyright (C) 2019  Hanjun Shin <hanjun.shin@samsung.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/dev_cooling.h>
#include <linux/debug-snapshot.h>

/**
 * struct dev_cooling_device - data for cooling device with dev
 * @id: unique integer value corresponding to each dev_cooling_device
 *	registered.
 * @cool_dev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @dev_state: integer value representing the current state of dev
 *	cooling	devices.
 * @dev_val: integer value representing the absolute value of the clipped
 *	freq.
 * @freq_table: frequency table to convert level to freq
 * @thermal_pm_qos_max: pm qos node to throttle max freq
 *
 * This structure is required for keeping information of each
 * dev_cooling_device registered. In order to prevent corruption of this a
 * mutex lock cooling_dev_lock is used.
 */
struct dev_cooling_device {
	int						id;
	struct					thermal_cooling_device *cool_dev;
	unsigned int			dev_state;
	unsigned int			dev_val;
	u32						max_state;
	struct exynos_devfreq_opp_table		*freq_table;
	struct pm_qos_request	thermal_pm_qos_max;
};

static DEFINE_MUTEX(cooling_dev_lock);
static unsigned int dev_count;

/* dev cooling device callback functions are defined below */
/**
 * dev_get_max_state - callback function to get the max cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the max cooling state.
 *
 * Callback for the thermal cooling device to return the dev
 * max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int dev_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct dev_cooling_device *dev = cdev->devdata;

	*state = dev->max_state;

	return 0;
}
/**
 * dev_get_cur_state - callback function to get the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the current cooling state.
 *
 * Callback for the thermal cooling device to return the dev
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int dev_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct dev_cooling_device *dev = cdev->devdata;

	*state = dev->dev_state;

	return 0;
}

/**
 * dev_set_cur_state - callback function to set the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: set this variable to the current cooling state.
 *
 * Callback for the thermal cooling device to change the dev
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int dev_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct dev_cooling_device *dev = cdev->devdata;

	if (dev->dev_state == state)
		return 0;

	dev->dev_state = (unsigned int)state;
	dev->dev_val = dev->freq_table[state].freq;
	dbg_snapshot_thermal(NULL, 0, cdev->type, dev->dev_val);
	pm_qos_update_request(&dev->thermal_pm_qos_max, dev->dev_val);

	return 0;
}

static int dev_set_cur_temp(struct thermal_cooling_device *cdev,
				bool suspended, int temp)
{
	return 0;
}

static int exynos_dev_cooling_get_level(struct thermal_cooling_device *cdev,
				 unsigned long value)
{
	struct dev_cooling_device *dev = cdev->devdata;
	int i, level = dev->max_state - 1;

	for (i = 0; i < dev->max_state; i++) {
		if (dev->freq_table[i].freq <= value) {
			level = i;
			break;
		}
	}
	return level;
}

/* Bind dev callbacks to thermal cooling device ops */
static struct thermal_cooling_device_ops const dev_cooling_ops = {
	.get_max_state = dev_get_max_state,
	.get_cur_state = dev_get_cur_state,
	.set_cur_state = dev_set_cur_state,
	.set_cur_temp = dev_set_cur_temp,
	.get_cooling_level = exynos_dev_cooling_get_level,
};

/**
 * __dev_cooling_register - helper function to create dev cooling device
 * @np: a valid struct device_node to the cooling device device tree node
 * @data: exynos_devfreq_data structure for using devfreq
 *
 * This interface function registers the dev cooling device with the name
 * "thermal-dev-%x". This api can support multiple instances of dev
 * cooling devices. It also gives the opportunity to link the cooling device
 * with a device tree node, in order to bind it via the thermal DT code.
 *
 * Return: a valid struct thermal_cooling_device pointer on success,
 * on failure, it returns a corresponding ERR_PTR().
 */
static struct thermal_cooling_device *
__dev_cooling_register(struct device_node *np, struct exynos_devfreq_data *data)
{
	struct thermal_cooling_device *cool_dev;
	struct dev_cooling_device *dev = NULL;
	char dev_name[THERMAL_NAME_LENGTH];

	dev = kzalloc(sizeof(struct dev_cooling_device), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&cooling_dev_lock);
	dev->id = dev_count;
	dev_count++;
	mutex_unlock(&cooling_dev_lock);

	snprintf(dev_name, sizeof(dev_name), "thermal-dev-%d", dev->id);

	dev->dev_state = 0;
	dev->freq_table = data->opp_list;
	dev->max_state = data->max_state;

	cool_dev = thermal_of_cooling_device_register(np, dev_name, dev,
						      &dev_cooling_ops);

	if (IS_ERR(cool_dev)) {
		kfree(dev);
		return cool_dev;
	}

	dev->cool_dev = cool_dev;

	pm_qos_add_request(&dev->thermal_pm_qos_max, (int)data->pm_qos_class_max, data->max_freq);

	return cool_dev;
}

/**
 * dev_cooling_unregister - function to remove dev cooling device.
 * @cdev: thermal cooling device pointer.
 *
 * This interface function unregisters the "thermal-dev-%x" cooling device.
 */
void dev_cooling_unregister(struct thermal_cooling_device *cdev)
{
	struct dev_cooling_device *dev;

	if (!cdev)
		return;

	dev = cdev->devdata;
	mutex_lock(&cooling_dev_lock);
	dev_count--;
	mutex_unlock(&cooling_dev_lock);

	thermal_cooling_device_unregister(dev->cool_dev);
	kfree(dev);
}
EXPORT_SYMBOL_GPL(dev_cooling_unregister);

/**
 * exynos_dev_cooling_register - function to remove dev cooling device.
 * @np: a valid struct device_node to the cooling device device tree node
 * @data: exynos_devfreq_data structure for using devfreq
 *
 * This interface function registers the exynos devfreq cooling device.
 */
struct thermal_cooling_device *exynos_dev_cooling_register(struct device_node *np, struct exynos_devfreq_data *data)
{
	return __dev_cooling_register(np, data);
}
EXPORT_SYMBOL_GPL(exynos_dev_cooling_register);
