/*
 * Copyright (C) 2021 Samsung Electronics Co., Ltd. All rights reserved.
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

#include "flicker_test.h"

static struct test_data *data = NULL;
static struct result_data *test_result = NULL;

static void (*err_handler)(void);


void als_eol_set_err_handler(void (*handler)(void)){
	err_handler = handler;
}
EXPORT_SYMBOL(als_eol_set_err_handler);

/**
 * als_eol_update_als - Record every test value
 *
 * @awb      : current 'awb' value
 *            (You can use any value as you need. Maybe Infrared value will be used in most cases)
 * @clear    : current clear value detected by sensor. It means visible light in light spectrum
 * @wideband : current Wideband value detected by sensor.
 *             wideband light include visible light and infrared.
 *
 */
void als_eol_update_als(int awb, int clear, int wideband, int uv)
{
	if (data->eol_enable && data->eol_count >= EOL_SKIP_COUNT) {
		data->eol_awb += awb;
		data->eol_clear += clear;
		data->eol_wideband += wideband;
		data->eol_uv += uv;
	}

	if (data->eol_enable && data->eol_state < EOL_STATE_DONE) {
		switch (data->eol_state) {
			case EOL_STATE_INIT:
				memset(test_result, 0, sizeof(struct result_data));

				data->eol_count = 0;
				data->eol_awb = 0;
				data->eol_clear = 0;
				data->eol_wideband = 0;
				data->eol_flicker = 0;
				data->eol_flicker_count = 0;
				data->eol_state = EOL_STATE_100;
				data->eol_pulse_count = 0;
				data->eol_uv = 0;
				break;
			default:
				data->eol_count++;
				printk(KERN_INFO"%s - eol_state:%d, eol_cnt:%d, flk_avr:%d (sum:%d, cnt:%d), ir:%d, clear:%d, wide:%d, uv:%d\n", __func__,
						data->eol_state, data->eol_count, (data->eol_flicker / data->eol_flicker_count), data->eol_flicker, data->eol_flicker_count,
						data->eol_awb, data->eol_clear, data->eol_wideband, data->eol_uv);

				if (data->eol_count >= (EOL_COUNT + EOL_SKIP_COUNT)) {
					test_result->flicker[data->eol_state] = data->eol_flicker / data->eol_flicker_count;
					test_result->awb[data->eol_state] = data->eol_awb / EOL_COUNT;
					test_result->clear[data->eol_state] = data->eol_clear / EOL_COUNT;
					test_result->wideband[data->eol_state] = data->eol_wideband / EOL_COUNT;
					test_result->uv[data->eol_state] = data->eol_uv / EOL_COUNT;

					printk(KERN_INFO"%s - eol_state = %d, pulse_count = %d, flicker_result = %d\n",
							__func__, data->eol_state, data->eol_pulse_count, test_result->flicker[data->eol_state]);

					data->eol_count = 0;
					data->eol_awb = 0;
					data->eol_clear = 0;
					data->eol_wideband = 0;
					data->eol_flicker = 0;
					data->eol_flicker_count = 0;
					data->eol_pulse_count = 0;
					data->eol_uv = 0;
					data->eol_state++;
				}
				break;
		}
	}
}
EXPORT_SYMBOL(als_eol_update_als);

/**
 * als_eol_update_flicker - Record every test value
 *
 * @flicker: current Flicker value detected by sensor.
 */
void als_eol_update_flicker(int Hz)
{
	if (data->eol_enable && Hz != 0 && data->eol_count >= EOL_SKIP_COUNT) {
		data->eol_flicker += Hz;
		data->eol_flicker_count++;
	}
}
EXPORT_SYMBOL(als_eol_update_flicker);

void set_led_mode(int led_curr)
{
#if IS_ENABLED(CONFIG_LEDS_S2MPB02)
	s2mpb02_led_en(led_mode, led_curr, S2MPB02_LED_TURN_WAY_GPIO);
#elif IS_ENABLED(CONFIG_LEDS_KTD2692)
	ktd2692_led_mode_ctrl(led_mode, led_curr);
#endif
}

void als_eol_set_env(bool torch, int intensity)
{
#if IS_ENABLED(CONFIG_LEDS_S2MPB02)
	if (torch) {
		gpio_led = gpio_torch;
		led_curr = (intensity/20);
		led_mode = S2MPB02_TORCH_LED_1;
	} else {
		gpio_led = gpio_flash;
		led_curr = (intensity-50)/100;
		led_mode = S2MPB02_FLASH_LED_1;
	}
#elif IS_ENABLED(CONFIG_LEDS_KTD2692)
	if (torch) {
		gpio_led = gpio_torch;
		led_curr = 80;
		led_mode = KTD2692_FLICKER_FLASH_MODE;
	} else {
		gpio_led = gpio_flash;
		led_curr = 80;
		led_mode = KTD2692_FLICKER_FLASH_MODE;
	}
#endif
	printk(KERN_INFO "%s - gpio:%d intensity:%d(%d) led_mode:%d",
		__func__, gpio_led, intensity, led_curr, led_mode);
}
EXPORT_SYMBOL(als_eol_set_env);

/**
 * als_eol_mode - start LED flicker loop
 *
 * Return result_data
 * MUST call als_eol_update* functions to notify the sensor output!!
 */
struct result_data* als_eol_mode()
{
	int pulse_duty = 0;
	int curr_state = EOL_STATE_INIT;
	int ret = 0;
	u32 prev_eol_count = 0, loop_count = 0;

	set_led_mode(0);

	ret = gpio_request(gpio_led, NULL);
	if (ret < 0)
		return NULL;

	data->eol_state = EOL_STATE_INIT;
	data->eol_enable = 1;

	printk(KERN_INFO"%s - eol_loop start", __func__);
	while (data->eol_state < EOL_STATE_DONE) {
		if (prev_eol_count == data->eol_count)
			loop_count++;
		else
			loop_count = 0;

		prev_eol_count = data->eol_count;

		switch (data->eol_state) {
			case EOL_STATE_INIT:
				pulse_duty = 1000;
				break;
			case EOL_STATE_100:
				pulse_duty = DEFAULT_DUTY_50HZ;
				break;
			case EOL_STATE_120:
				pulse_duty = DEFAULT_DUTY_60HZ;
				break;
			default:
				break;
		}

		if (data->eol_state >= EOL_STATE_100) {
			if (curr_state != data->eol_state) {
#if IS_ENABLED(CONFIG_LEDS_KTD2692)
				if(ret >= 0) {
					gpio_free(gpio_led);
				}
#endif
				set_led_mode(led_curr);
				curr_state = data->eol_state;

#if IS_ENABLED(CONFIG_LEDS_KTD2692)
				ret = gpio_request(gpio_led, NULL);
				if (ret < 0)
					break;
#endif
			} else {
				gpio_direction_output(gpio_led, 1);
				udelay(pulse_duty);
				gpio_direction_output(gpio_led, 0);
				data->eol_pulse_count++;
			}
		}

		if (loop_count > 1000) {
			printk(KERN_ERR "%s - ERR NO Interrupt", __func__);
			// Add Debug Code
			if (err_handler)
				err_handler();
			break;
		}

		udelay(pulse_duty);
	}
	printk(KERN_INFO"%s - eol_loop end",__func__);
	if(ret >= 0) {
		gpio_free(gpio_led);
	}
	set_led_mode(0);

	if (data->eol_state >= EOL_STATE_DONE) {
		if(test_result->clear[EOL_STATE_100] != 0) {
			test_result->ratio[EOL_STATE_100] = test_result->awb[EOL_STATE_100] * 100 / test_result->clear[EOL_STATE_100];
		}
		if(test_result->clear[EOL_STATE_120] != 0) {
			test_result->ratio[EOL_STATE_120] = test_result->awb[EOL_STATE_120] * 100 / test_result->clear[EOL_STATE_120];
		}
	} else {
		printk(KERN_ERR "%s - abnormal termination\n", __func__);
	}
	printk(KERN_INFO "%s - RESULT: flicker:%d|%d awb:%d|%d clear:%d|%d wide:%d|%d ratio:%d|%d uv:%d|%d", __func__,
			test_result->flicker[EOL_STATE_100], test_result->flicker[EOL_STATE_120],
			test_result->awb[EOL_STATE_100], test_result->awb[EOL_STATE_120],
			test_result->clear[EOL_STATE_100], test_result->clear[EOL_STATE_120],
			test_result->wideband[EOL_STATE_100], test_result->wideband[EOL_STATE_120],
			test_result->ratio[EOL_STATE_100], test_result->ratio[EOL_STATE_120],
			test_result->uv[EOL_STATE_100], test_result->uv[EOL_STATE_120]);

	data->eol_enable = 0;

	return test_result;
}
EXPORT_SYMBOL(als_eol_mode);

int als_eol_parse_dt()
{
	struct device_node *np;
	enum of_gpio_flags flags;

	np = of_find_node_by_name(NULL, LED_DT_NODE_NAME);
	if (np == NULL) {
		printk(KERN_ERR "%s - Can't find node", __func__);
		return -1;
	}

	gpio_torch = of_get_named_gpio_flags(np, "flicker_test,torch-gpio", 0, &flags);
	gpio_flash = of_get_named_gpio_flags(np, "flicker_test,flash-gpio", 0, &flags);

	printk(KERN_INFO "%s - torch : %d flash : %d", __func__, gpio_torch, gpio_flash);

	return 0;
}

static int __init als_eol_init(void)
{
	int ret = 0;
	printk(KERN_INFO "%s - EOL_TEST Module init", __func__);

	data = (struct test_data*)kzalloc(sizeof(struct test_data), GFP_KERNEL);
	if (data == NULL) {
		printk(KERN_INFO "%s - data alloc err", __func__);
		return -1;
	}

	test_result = (struct result_data*)kzalloc(sizeof(struct result_data), GFP_KERNEL);
	if (test_result == NULL) {
		printk(KERN_INFO "%s - test_result alloc err", __func__);
		return -1;
	}

	ret = als_eol_parse_dt();
	if (ret < 0) {
		printk(KERN_ERR "%s - dt parse fail!", __func__);
	}

	return ret;
}

static void __exit als_eol_exit(void)
{
	printk(KERN_INFO "%s - EOL_TEST Module exit\n", __func__);

	if(data) {
		kfree(data);
	}

	if(test_result) {
		kfree(test_result);
	}
}

module_init(als_eol_init);
module_exit(als_eol_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Flicker Sensor Test Driver");
MODULE_LICENSE("GPL");
