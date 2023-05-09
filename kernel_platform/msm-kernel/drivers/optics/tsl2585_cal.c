/*
 * Copyright (C) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tsl2585.h"
#include <linux/pwm.h>

#define OSC_CAL_FREQ (40)

static u16 cal_period = 0xffff;

int tsl2585_power_ctrl(struct tsl2585_device_data *data, int onoff);

u16 ams_pwm_gpio_period(struct tsl2585_device_data *data);
void tsl2585_pin_control(struct tsl2585_device_data *data, bool pin_set);

int tsl2585_setup_pwm(struct tsl2585_device_data *data, long pwm_freq)
{
	int ret = 0;
	struct device *dev = &data->client->dev;
	struct clk *pclk = NULL;

	ALS_dbg("%s - setup gpio pwm %d Hz\n", __func__, pwm_freq);

	pclk = devm_clk_get(dev, "gpio_pwm_default-clk");
	if (NULL == pclk) {
		ALS_err("%s: get pclk (gpio_pwm_default-clk) error. \n" , __func__);
		return -ENODEV;
	}

	if (pwm_freq > 0) {
#if 0
		if (__clk_is_enabled(pclk)) {
			ALS_dbg("%s - clk_disable_unprepare (pclk) start! (clk_is_enabled: %d, clk_get_rate: %lld)", __func__, __clk_is_enabled(pclk), clk_get_rate(pclk));
			clk_disable_unprepare(pclk);
		}

		msleep_interruptible(10);
#endif
		ret = clk_set_rate(pclk, pwm_freq);
		if (ret) {
			ALS_err("%s - clk_set_rate (pclk) fail! (clk_is_enabled: %d, clk_get_rate: %lld)", __func__, __clk_is_enabled(pclk), clk_get_rate(pclk));
			return ret;
		} else {
			ALS_dbg("%s - clk_set_rate (pclk) success! (clk_is_enabled: %d, clk_get_rate: %lld)", __func__, __clk_is_enabled(pclk), clk_get_rate(pclk));
		}

		if (!__clk_is_enabled(pclk)) {
			ret = clk_prepare_enable(pclk);
			if (ret) {
				ALS_err("%s - clk_prepare_enable (pclk) fail! (clk_is_enabled: %d, clk_get_rate: %lld)", __func__, __clk_is_enabled(pclk), clk_get_rate(pclk));
				return ret;
			} else {
				ALS_dbg("%s - clk_prepare_enable (pclk) success! (clk_is_enabled: %d, clk_get_rate: %lld)", __func__, __clk_is_enabled(pclk), clk_get_rate(pclk));
			}
		}

	} else {
		if (__clk_is_enabled(pclk)) {
			clk_disable_unprepare(pclk);
			ALS_dbg("%s - clk_disable_unprepare (pclk) start! (clk_is_enabled: %d, clk_get_rate: %lld)", __func__, __clk_is_enabled(pclk), clk_get_rate(pclk));
		}
	}

	return ret;
}

ssize_t tsl2585_osc_cal_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tsl2585_device_data *data = dev_get_drvdata(dev);
	int pwm_Hz = 0;
	int err = 0;

	if (!data->osc_cal) {
		ALS_err("%s - no osc calibration mode\n", __func__);
		return size;
	}

	err = kstrtoint(buf, 10, &pwm_Hz);

	if (err < 0) {
		ALS_err("%s - kstrtoint failed.(%d)\n", __func__, err);
		return size;
	}

	if (pwm_Hz == 0) {
		ALS_dbg("%s - pwm output stop! (%d Hz)\n", __func__, pwm_Hz);
		tsl2585_setup_pwm(data, 0);
	} else if ((pwm_Hz > 0) && (pwm_Hz <= 120)) {
		tsl2585_pin_control(data, true);

		if (pwm_Hz == 1) {
			ALS_dbg("%s - pwm output start! (%d Hz)\n", __func__, OSC_CAL_FREQ);
			tsl2585_osc_cal(data);
		} else {
			ALS_dbg("%s - pwm output start! (%d Hz)\n", __func__, pwm_Hz);
			tsl2585_setup_pwm(data, pwm_Hz);
		}
	} else {
		ALS_err("%s - pwm out of range error! (%d Hz)\n", __func__, pwm_Hz);
	}

	return size;
}

void tsl2585_osc_cal(struct tsl2585_device_data *data)
{
	u16 ret;

	tsl2585_power_ctrl(data, PWR_ON);
	tsl2585_setup_pwm(data, OSC_CAL_FREQ);

	ret = ams_pwm_gpio_period(data);
	if (ret != 0) {
		cal_period = ret;
	}
	ALS_dbg("%s - cal_period = %d", __func__, ret);

	tsl2585_setup_pwm(data, 0);
	tsl2585_power_ctrl(data, PWR_OFF);
}

u16 tsl2585_get_pwm_calibration(void)
{
	if (cal_period == 0)
		return 0xffff;
	return cal_period;
}
