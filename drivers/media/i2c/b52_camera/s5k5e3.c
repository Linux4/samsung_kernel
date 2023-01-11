/* Marvell ISP S5K5E3 Driver
 *
 * Copyright (C) 2009-2014 Marvell International Ltd.
 *
 * Copyright (c) 2009 Mauro Carvalho Chehab (mchehab@redhat.com)
 * This code is placed under the terms of the GNU General Public License v2
 */


#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>

#include <asm/div64.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <uapi/media/b52_api.h>
#include "s5k5e3.h"
#include <media/mv_sc2_twsi_conf.h>
#include "../../platform/mmp-isp/plat_cam.h"


static int S5K5E3_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	/*pixel clock is 179.4Mhz*/
	u32 pre_pll_clk_div;
	u32 pll_mul;
	u32 pll_s;
	u32 pll_power_table[8] = {1, 2, 4, 8, 16, 32, 64, 128};
	u32 tmp;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	b52_sensor_call(sensor, i2c_read, 0x0305, &pre_pll_clk_div, 1);
	pre_pll_clk_div = pre_pll_clk_div & 0x3f;
	b52_sensor_call(sensor, i2c_read, 0x0306, &tmp, 1);
	pll_mul =  (tmp & 0x03)<<8;
	b52_sensor_call(sensor, i2c_read, 0x0307, &tmp, 1);
	pll_mul +=  (tmp & 0xff);
	b52_sensor_call(sensor, i2c_read, 0x3c1f, &pll_s, 1);
	pll_s =  pll_power_table[pll_s & 0x7];
	*rate = mclk / pre_pll_clk_div * pll_mul / pll_s / 5;
	return 0;
}
static int S5K5E3_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	S5K5E3_get_pixelclock(sd, rate, mclk);
	*rate =  *rate / 2 * 10 /2;
	return 0;
}
static int S5K5E3_get_dphy_desc(struct v4l2_subdev *sd,
				struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	S5K5E3_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 58;
	dphy_desc->hs_zero  = 100;
	return 0;
}


static int S5K5E3_s_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;
	int reset_delay = 100;
	struct sensor_power *power;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct b52_sensor *sensor = to_b52_sensor(sd);

	power = (struct sensor_power *) &(sensor->power);

	if (on) {
		if (power->ref_cnt++ > 0)
			return 0;

		ret = plat_tune_isp(1);
		if (ret < 0)
			goto isppw_err;

		if (power->avdd_2v8) {
			regulator_set_voltage(power->avdd_2v8,
						2800000, 2800000);
			ret = regulator_enable(power->avdd_2v8);
			if (ret < 0)
				goto avdd_err;
		}

		if (power->dvdd_1v2) {
			regulator_set_voltage(power->dvdd_1v2,
						1200000, 1200000);
			ret = regulator_enable(power->dvdd_1v2);
			if (ret < 0)
				goto dvdd_1v2_err;
		}
		
		msleep(1);
#if defined(CONFIG_MACH_GRANDPRIMEVELTE)
		power->dovdd_1v8_gpio = devm_gpiod_get(&client->dev, "dovdd_1v8_gpio");
		if (power->dovdd_1v8_gpio)
			ret = gpiod_direction_output(power->dovdd_1v8_gpio, 1);
#else
		if (power->dovdd_1v8) {
			regulator_set_voltage(power->dovdd_1v8,
						1800000, 1800000);
			ret = regulator_enable(power->dovdd_1v8);
			if (ret < 0)
				goto dvdd_err;
		}
#endif

		if (sensor->i2c_dyn_ctrl) {
			ret = sc2_select_pins_state(sensor->pos - 1,
					SC2_PIN_ST_SCCB, SC2_MOD_B52ISP);
			if (ret < 0) {
				pr_err("b52 sensor i2c pin is not configured\n");
				goto st_err;
			}
		}
		power->rst = devm_gpiod_get(&client->dev, "reset");
		if (IS_ERR(power->rst)) {
			dev_warn(&client->dev, "Failed to get gpio reset\n");
			power->rst = NULL;
		} else {
			ret = gpiod_direction_output(power->rst, 1);
			if (ret < 0) {
				dev_err(&client->dev, "Failed to set gpio rst\n");
				goto rst_err;
			}
		}

		if (power->rst) {
			gpiod_set_value_cansleep(power->rst, 1);
			/* according to SR544 power sequence
			driver have to delay > 10ms
			*/
			if (sensor->drvdata->reset_delay)
				reset_delay = sensor->drvdata->reset_delay;
			usleep_range(reset_delay, reset_delay + 10);
			gpiod_set_value_cansleep(power->rst, 0);
		}

		clk_set_rate(sensor->clk, sensor->mclk);
		clk_prepare_enable(sensor->clk);

	} else {
		if (WARN_ON(power->ref_cnt == 0))
			return -EINVAL;

		if (--power->ref_cnt > 0)
			return 0;
		clk_disable_unprepare(sensor->clk);

		if (power->rst)
			gpiod_set_value_cansleep(power->rst, 1);

		if (power->dovdd_1v8_gpio)
			gpiod_set_value_cansleep(power->dovdd_1v8_gpio, 1);

		if (power->dvdd_1v2)
			regulator_disable(power->dvdd_1v2);

		if (power->avdd_2v8)
			regulator_disable(power->avdd_2v8);

		if (sensor->i2c_dyn_ctrl) {
			ret = sc2_select_pins_state(sensor->pos - 1,
					SC2_PIN_ST_GPIO, SC2_MOD_B52ISP);
			if (ret < 0)
				pr_err("b52 sensor gpio pin is not configured\n");
		}

		WARN_ON(plat_tune_isp(0) < 0);
		
		if (sensor->power.rst)
			devm_gpiod_put(&client->dev, sensor->power.rst);
		if (sensor->power.dovdd_1v8_gpio)
			devm_gpiod_put(&client->dev, sensor->power.dovdd_1v8_gpio);
		sensor->sensor_init = 0;


		sensor->sensor_init = 0;
	}

	return ret;

dvdd_1v2_err:
	if (power->dvdd_1v2)
		regulator_disable(power->dvdd_1v2);
dvdd_err:
	if (power->dovdd_1v8)
		regulator_disable(power->dovdd_1v8);
avdd_err:
	if (power->avdd_2v8)
		regulator_disable(power->avdd_2v8);
rst_err:
	if (sensor->power.rst)
		devm_gpiod_put(&client->dev, sensor->power.rst);
i2c_err:
	clk_disable_unprepare(sensor->clk);
	if (sensor->i2c_dyn_ctrl)
		ret = sc2_select_pins_state(sensor->pos - 1,
				SC2_PIN_ST_GPIO, SC2_MOD_B52ISP);
st_err:
	WARN_ON(plat_tune_isp(0) < 0);
isppw_err:
	power->ref_cnt--;

	return ret;
}

