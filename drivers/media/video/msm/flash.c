
/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pwm.h>
#include <linux/hrtimer.h>
#include <linux/export.h>
#include <mach/pmic.h>
#include <mach/camera.h>
#include <linux/gpio.h>
#include <mach/msm8930-gpio.h>
#include "msm_camera_i2c.h"
#include <linux/spinlock.h>

#if defined CONFIG_LEDS_PMIC8058
#include <linux/leds-pmic8058.h>
#include <linux/pmic8058-pwm.h>
#endif
#ifdef CONFIG_LEDS_RT8547
#include <linux/leds-rt8547.h>
#endif /*CONFIG_LEDS_RT8547*/

#undef cam_err
#define cam_err(fmt, arg...)			\
	do {					\
		printk(KERN_ERR "[CAMERA]][%s:%d] " fmt,		\
			__func__, __LINE__, ##arg);	\
	}						\
	while (0)

#ifdef CONFIG_IMX175
extern bool Torch_On;
#endif
#ifdef CONFIG_MACH_MELIUS
extern unsigned int system_rev;
static DEFINE_SPINLOCK(flash_ctrl_lock);
#endif

struct i2c_client *sx150x_client;
struct timer_list timer_flash;
static struct msm_camera_sensor_info *sensor_data;
static struct msm_camera_i2c_client i2c_client;
enum msm_cam_flash_stat{
	MSM_CAM_FLASH_OFF,
	MSM_CAM_FLASH_ON,
};
static struct i2c_client *sc628a_client;

static const struct i2c_device_id sc628a_i2c_id[] = {
	{"sc628a", 0},
	{ }
};

static int sc628a_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("sc628a_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	sc628a_client = client;

	CDBG("sc628a_probe success rc = %d\n", rc);
	return 0;

probe_failure:
	pr_err("sc628a_probe failed! rc = %d\n", rc);
	return rc;
}

static struct i2c_driver sc628a_i2c_driver = {
	.id_table = sc628a_i2c_id,
	.probe  = sc628a_i2c_probe,
	.remove = __exit_p(sc628a_i2c_remove),
	.driver = {
		.name = "sc628a",
	},
};

static struct i2c_client *tps61310_client;

static const struct i2c_device_id tps61310_i2c_id[] = {
	{"tps61310", 0},
	{ }
};

static int tps61310_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("%s enter\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	tps61310_client = client;
	i2c_client.client = tps61310_client;
	i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	rc = msm_camera_i2c_write(&i2c_client, 0x01, 0x00,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		tps61310_client = NULL;
		goto probe_failure;
	}

	CDBG("%s success! rc = %d\n", __func__, rc);
	return 0;

probe_failure:
	pr_err("%s failed! rc = %d\n", __func__, rc);
	return rc;
}

static struct i2c_driver tps61310_i2c_driver = {
	.id_table = tps61310_i2c_id,
	.probe  = tps61310_i2c_probe,
	.remove = __exit_p(tps61310_i2c_remove),
	.driver = {
		.name = "tps61310",
	},
};

static int config_flash_gpio_table(enum msm_cam_flash_stat stat,
			struct msm_camera_sensor_strobe_flash_data *sfdata)
{
	int rc = 0, i = 0;
	int msm_cam_flash_gpio_tbl[][2] = {
		{sfdata->flash_trigger, 1},
		{sfdata->flash_charge, 1},
		{sfdata->flash_charge_done, 0}
	};

	if (stat == MSM_CAM_FLASH_ON) {
		for (i = 0; i < ARRAY_SIZE(msm_cam_flash_gpio_tbl); i++) {
			rc = gpio_request(msm_cam_flash_gpio_tbl[i][0],
							  "CAM_FLASH_GPIO");
			if (unlikely(rc < 0)) {
				pr_err("%s not able to get gpio\n", __func__);
				for (i--; i >= 0; i--)
					gpio_free(msm_cam_flash_gpio_tbl[i][0]);
				break;
			}
			if (msm_cam_flash_gpio_tbl[i][1])
				gpio_direction_output(
					msm_cam_flash_gpio_tbl[i][0], 0);
			else
				gpio_direction_input(
					msm_cam_flash_gpio_tbl[i][0]);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(msm_cam_flash_gpio_tbl); i++) {
			gpio_direction_input(msm_cam_flash_gpio_tbl[i][0]);
			gpio_free(msm_cam_flash_gpio_tbl[i][0]);
		}
	}
	return rc;
}

int msm_camera_flash_current_driver(
	struct msm_camera_sensor_flash_current_driver *current_driver,
	unsigned led_state)
{
	int rc = 0;
#if defined CONFIG_LEDS_PMIC8058
	int idx;
	const struct pmic8058_leds_platform_data *driver_channel =
		current_driver->driver_channel;
	int num_leds = driver_channel->num_leds;

	CDBG("%s: led_state = %d\n", __func__, led_state);

	/* Evenly distribute current across all channels */
	switch (led_state) {
	case MSM_CAMERA_LED_OFF:
		for (idx = 0; idx < num_leds; ++idx) {
			rc = pm8058_set_led_current(
				driver_channel->leds[idx].id, 0);
			if (rc < 0)
				pr_err(
					"%s: FAIL name = %s, rc = %d\n",
					__func__,
					driver_channel->leds[idx].name,
					rc);
		}
		break;

	case MSM_CAMERA_LED_LOW:
		for (idx = 0; idx < num_leds; ++idx) {
			rc = pm8058_set_led_current(
				driver_channel->leds[idx].id,
				current_driver->low_current/num_leds);
			if (rc < 0)
				pr_err(
					"%s: FAIL name = %s, rc = %d\n",
					__func__,
					driver_channel->leds[idx].name,
					rc);
		}
		break;

	case MSM_CAMERA_LED_HIGH:
		for (idx = 0; idx < num_leds; ++idx) {
			rc = pm8058_set_led_current(
				driver_channel->leds[idx].id,
				current_driver->high_current/num_leds);
			if (rc < 0)
				pr_err(
					"%s: FAIL name = %s, rc = %d\n",
					__func__,
					driver_channel->leds[idx].name,
					rc);
		}
		break;
	case MSM_CAMERA_LED_INIT:
	case MSM_CAMERA_LED_RELEASE:
		break;

	default:
		rc = -EFAULT;
		break;
	}
	CDBG("msm_camera_flash_led_pmic8058: return %d\n", rc);
#endif /* CONFIG_LEDS_PMIC8058 */
	return rc;
}
#if defined(CONFIG_MACH_MELIUS)
void set_gpio_ENF(struct msm_camera_sensor_flash_external *external, bool bSet)
{
	gpio_set_value_cansleep(external->led_en, bSet);
}

void set_gpio_ENM(struct msm_camera_sensor_flash_external *external, bool bSet)
{
	gpio_set_value_cansleep(external->led_flash_en, bSet);
}

int get_gpio_ENM(struct msm_camera_sensor_flash_external *external)
{
	return gpio_get_value_cansleep(external->led_flash_en);
}
/* KTD2692 : command time delay(us) */
#define T_DS		15	//	12
#define T_EOD_H		1000 //	350
#define T_EOD_L		4
#define T_H_LB		4
#define T_L_LB		3*T_H_LB
#define T_L_HB		4
#define T_H_HB		3*T_L_HB
#define T_RESET		800	//	700
/* KTD2692 : command address(A2:A0) */
#define LVP_SETTING		0x0 << 5
#define FLASH_TIMEOUT	0x1 << 5
#define MIN_CURRENT		0x2 << 5
#define MOVIE_CURRENT	0x3 << 5
#define FLASH_CURRENT	0x4 << 5
#define MODE_CONTROL	0x5 << 5

void KTD2692_ctrl_cmd(
		struct msm_camera_sensor_flash_external *external,
		unsigned int ctl_cmd)
{
	int i=0;
	int j = 0;
	int k = 0;
	unsigned long flags;

	spin_lock_irqsave(&flash_ctrl_lock, flags);
	if ( MODE_CONTROL == (MODE_CONTROL & ctl_cmd) )
		k = 8;
	else
		k = 1;
	for(j = 0; j < k; j++) {
		cam_err("[cmd::0x%2X][MODE_CONTROL&cmd::0x%2X][k::%d]\n", ctl_cmd, (MODE_CONTROL & ctl_cmd), k);
		gpio_set_value(external->led_flash_en, 1);
		udelay(T_DS);
		for(i = 0; i < 8; i++) {
			if(ctl_cmd & 0x80) { /* set bit to 1 */
				gpio_set_value(external->led_flash_en, 0);
				udelay(T_L_HB);
				gpio_set_value(external->led_flash_en, 1);
				udelay(T_H_HB);
			} else { /* set bit to 0 */
				gpio_set_value(external->led_flash_en, 0);
				udelay(T_L_LB);
				gpio_set_value(external->led_flash_en, 1);
				udelay(T_H_LB);
			}
			ctl_cmd = ctl_cmd << 1;
		}
		gpio_set_value(external->led_flash_en, 0);
		udelay(T_EOD_L);
		gpio_set_value(external->led_flash_en, 1);
		udelay(T_EOD_H);
	}	
	spin_unlock_irqrestore(&flash_ctrl_lock, flags);
}
#elif defined (CONFIG_MACH_SERRANO) || defined (CONFIG_MACH_CRATER) || defined (CONFIG_MACH_BAFFIN)
static DEFINE_SPINLOCK(flash_ctrl_lock);

static void MIC2871YMK_set_flash_movie(unsigned int ctl_cmd)
{
	int icnt;
	unsigned long flags;
	
	spin_lock_irqsave(&flash_ctrl_lock, flags);

	if (ctl_cmd == 0) {
		cam_err("[teddy][Torch flash]OFF\n");
		gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0); 
	} else
	{
		cam_err("[teddy][Torch flash]ON\n");
		gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
		udelay(1);
	
		// set address 4
		for (icnt = 0; icnt < 5  ; icnt++)
		{
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
			udelay(1);
		
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
			udelay(1);
		}
		udelay(110);
		
		// set data 0 - disable
		for (icnt = 0; icnt < 1; icnt++)
		{
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
			udelay(1);
		
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
			udelay(1);
		}
		udelay(410);

		// set address 5
		for (icnt = 0; icnt < 6  ; icnt++)
		{
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
			udelay(1);
		
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
			udelay(1);
		}
		udelay(110);
		
		// set data 5 - 300mA
		for (icnt = 0; icnt < 6; icnt++)
		{
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
			udelay(1);
		
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
			udelay(1);
		}
		udelay(410);

		//set disable for Torch mode
		// set address 2
		for (icnt = 0; icnt < 3  ; icnt++)
		{	
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
			udelay(1);

			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
			udelay(1);
		}
		udelay(110);

		// set data 21 - Torch 56%
		for (icnt = 0; icnt < 22; icnt++)
		{	
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
			udelay(1);

			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
			udelay(1);
		}	
		udelay(410);
	}
	
	spin_unlock_irqrestore(&flash_ctrl_lock, flags);	
}
static void MIC2871YMK_set_flash_flash(unsigned int ctl_cmd)
{
	int icnt;
	unsigned long flags;

	spin_lock_irqsave(&flash_ctrl_lock, flags);

	gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
	gpio_set_value(GPIO_MSM_FLASH_NOW, 0); 		
	udelay(450);

	if (ctl_cmd == 0) {
		cam_err("[teddy][Torch flash]OFF\n");
	} else
	{
		cam_err("[teddy][Torch flash]ON\n");
		// set address 4
		gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
		udelay(1);

		for (icnt = 0; icnt < 5  ; icnt++)
		{
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
			udelay(1);
		
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
			udelay(1);
		}
		udelay(110);
		
		// set data 0 - disable
		for (icnt = 0; icnt < 1; icnt++)
		{
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 0);
			udelay(1);
		
			gpio_set_value(GPIO_MSM_FLASH_CNTL_EN, 1);
			udelay(1);
		}
		udelay(410);

		gpio_set_value(GPIO_MSM_FLASH_NOW, 1);
	}

	spin_unlock_irqrestore(&flash_ctrl_lock, flags);
}
#endif

int msm_camera_flash_led(
		struct msm_camera_sensor_flash_external *external,
		unsigned led_state)
{
#ifdef CONFIG_IMX175
#if defined(CONFIG_MACH_MELIUS)
	int i = 0;
#endif
	int rc = 0;
	if(Torch_On == true) {
		cam_err("[Assistive Light On!!\n");
		return 0;
	}	
#if defined(CONFIG_MACH_MELIUS)
	/* FLASH IC : KTD2692 */
	cam_err("[led_state::%d]\n", led_state);
	switch (led_state) {
	case MSM_CAMERA_LED_INIT:
		cam_err("[MSM_CAMERA_LED_INIT]\n");
#if defined(CONFIG_MACH_CRATER_CHN_CTC) || defined(CONFIG_MACH_MELIUS_VZW) || defined(CONFIG_MACH_MELIUS_SPR) || defined(CONFIG_MACH_MELIUS_USC)
		if (system_rev < 0X01) {
#else
		if (system_rev < 0x07) {
#endif
			set_gpio_ENF(external, false);
		} else {
			/* Disable Cutoff Low voltage */
			KTD2692_ctrl_cmd(external, LVP_SETTING | 0x00);
			/*D2692_ctrl_cmd(external, MODE_CONTROL | 0x00);*/
		}
		break;

	case MSM_CAMERA_LED_RELEASE:
		cam_err("[MSM_CAMERA_LED_RELEASE]\n");
		gpio_set_value_cansleep(external->led_flash_en, 0);
		break;

	case MSM_CAMERA_LED_OFF:
		cam_err("[MSM_CAMERA_LED_OFF]\n");
#if defined(CONFIG_MACH_CRATER_CHN_CTC) ||  defined(CONFIG_MACH_MELIUS_VZW) || defined(CONFIG_MACH_MELIUS_SPR) || defined(CONFIG_MACH_MELIUS_USC)
		if (system_rev < 0X01) {
#else
		if (system_rev < 0x07) {
#endif
			set_gpio_ENF(external, false);
		} else {
			KTD2692_ctrl_cmd(external, MODE_CONTROL | 0x00);
		}
		break;

	case MSM_CAMERA_LED_LOW:
		/* Movie Current Setting : 0x64 (5/16) */
		cam_err("[MSM_CAMERA_LED_LOW]\n");
#if defined(CONFIG_MACH_CRATER_CHN_CTC) || defined(CONFIG_MACH_MELIUS_VZW) || defined(CONFIG_MACH_MELIUS_SPR) || defined(CONFIG_MACH_MELIUS_USC)
		if (system_rev < 0X01) {
#else
		if (system_rev < 0x07) {
#endif
			for (i = 2; i > 0; i--) {
				set_gpio_ENF(external, false);
				ndelay(300);
				set_gpio_ENF(external, true);
				ndelay(300);
			}
		} else {
			/* Disable Cutoff Low voltage */
			KTD2692_ctrl_cmd(external, LVP_SETTING | 0x00);
			KTD2692_ctrl_cmd(external, MOVIE_CURRENT | 0x04);
			KTD2692_ctrl_cmd(external, MODE_CONTROL | 0x01);
		}
		break;

	case MSM_CAMERA_LED_HIGH:
		cam_err("[MSM_CAMERA_LED_HIGH]\n");
#if defined(CONFIG_MACH_CRATER_CHN_CTC) || defined(CONFIG_MACH_MELIUS_VZW) || defined(CONFIG_MACH_MELIUS_SPR) || defined(CONFIG_MACH_MELIUS_USC)
		if (system_rev < 0X01) {
#else
		if (system_rev < 0x07) {
#endif
			for (i = 16; i > 0; i--) {
				set_gpio_ENF(external, false);
				ndelay(300);
				set_gpio_ENF(external, true);
				ndelay(300);
			}
		} else {
			/* Disable Cutoff Low voltage */
			KTD2692_ctrl_cmd(external, LVP_SETTING | 0x00);
			KTD2692_ctrl_cmd(external, FLASH_CURRENT | 0x0F);
			KTD2692_ctrl_cmd(external, MODE_CONTROL | 0x02);
		}
		break;

	default:
		cam_err("[default]\n");
		rc = -EFAULT;
		break;
	}
#elif defined (CONFIG_MACH_SERRANO) || defined (CONFIG_MACH_CRATER) || defined (CONFIG_MACH_BAFFIN)
	/* FLASH IC : MIC2871YMK*/
	cam_err("[led_state::%d]\n", led_state);
	switch (led_state) {
	case MSM_CAMERA_LED_INIT:
		cam_err("[MSM_CAMERA_LED_INIT][MIC2871YMK]\n");
		gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_CNTL_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(GPIO_MSM_FLASH_NOW, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		break;

	case MSM_CAMERA_LED_RELEASE:
		MIC2871YMK_set_flash_flash(0);
		cam_err("[MSM_CAMERA_LED_RELEASE][MIC2871YMK]\n");
		break;

	case MSM_CAMERA_LED_OFF:
		cam_err("[MSM_CAMERA_LED_OFF][MIC2871YMK]\n");
		MIC2871YMK_set_flash_flash(0);
		break;

	case MSM_CAMERA_LED_LOW:
		cam_err("[MSM_CAMERA_LED_LOW][MIC2871YMK]\n");
		MIC2871YMK_set_flash_movie(1);
		break;

	case MSM_CAMERA_LED_HIGH:
		cam_err("[MSM_CAMERA_LED_HIGH][MIC2871YMK]\n");
		MIC2871YMK_set_flash_flash(1);
		break;

	default:
		cam_err("[default][MIC2871YMK]\n");
		rc = -EFAULT;
		break;
	}
#elif defined (CONFIG_MACH_KS02)
	printk("[led_state::%d]\n", led_state);
	switch (led_state) {
	case MSM_CAMERA_LED_INIT:
		cam_err("[MSM_CAMERA_LED_INIT]\n");
		break;

	case MSM_CAMERA_LED_RELEASE:
		cam_err("[MSM_CAMERA_LED_RELEASE]\n");
		gpio_set_value_cansleep(GPIO_CAM_FLASH_EN, 0);
		gpio_set_value_cansleep(GPIO_CAM_FLASH_SET, 0);
		break;

	case MSM_CAMERA_LED_OFF:
		cam_err("[MSM_CAMERA_LED_OFF]\n");
		gpio_set_value_cansleep(GPIO_CAM_FLASH_EN, 0);
		gpio_set_value_cansleep(GPIO_CAM_FLASH_SET, 0);		
		break;

	case MSM_CAMERA_LED_LOW:
		cam_err("[MSM_CAMERA_LED_LOW]\n");
		gpio_set_value_cansleep(GPIO_CAM_FLASH_SET, 1);
		break;

	case MSM_CAMERA_LED_HIGH:
		cam_err("[MSM_CAMERA_LED_HIGH]\n");
		gpio_set_value_cansleep(GPIO_CAM_FLASH_EN, 1);
		break;

	default:
		cam_err("[default]\n");
		rc = -EFAULT;
		break;
	}
#endif
	return rc;
#elif defined (CONFIG_S5K4ECGX)
#if defined (CONFIG_MACH_CANE)  || defined (CONFIG_MACH_LOGANRE)
	int rc = 0;
	printk("[led_state::%d]\n", led_state);
	switch (led_state) {
	case MSM_CAMERA_LED_INIT:
		cam_err("[MSM_CAMERA_LED_INIT][RT8547]\n");
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_FLASH_SOURCE_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_FLASH_SET, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(GPIO_CAM_FLASH_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);

		/* LVP */
		//RT8547_ctrl_cmd(external, 0x00);
		break;

	case MSM_CAMERA_LED_RELEASE:
		cam_err("[MSM_CAMERA_LED_RELEASE]\n");
		break;

	case MSM_CAMERA_LED_OFF:
		printk("[Torch flash]OFF\n");
#if defined (CONFIG_LEDS_RT8547)
		rt8547_set_led_off();
#endif
		break;

	case MSM_CAMERA_LED_LOW:
		printk("[Torch flash]ON\n");
#if defined (CONFIG_LEDS_RT8547)
		rt8547_set_led_low();
#endif
		break;

	case MSM_CAMERA_LED_HIGH:
		printk("[Torch flash]ON\n");
#if defined (CONFIG_LEDS_RT8547)
		rt8547_set_led_high();
#endif
		break;

	default:
		rc = -EFAULT;
		break;
	}
#endif
	return rc;
#else
	int rc = 0;

	CDBG("msm_camera_flash_led: %d\n", led_state);
	switch (led_state) {
	case MSM_CAMERA_LED_INIT:
		rc = gpio_request(external->led_en, "sgm3141");
		CDBG("MSM_CAMERA_LED_INIT: gpio_req: %d %d\n",
				external->led_en, rc);
		if (!rc)
			gpio_direction_output(external->led_en, 0);
		else
			return 0;

		rc = gpio_request(external->led_flash_en, "sgm3141");
		CDBG("MSM_CAMERA_LED_INIT: gpio_req: %d %d\n",
				external->led_flash_en, rc);
		if (!rc)
			gpio_direction_output(external->led_flash_en, 0);

			break;

	case MSM_CAMERA_LED_RELEASE:
		CDBG("MSM_CAMERA_LED_RELEASE\n");
		gpio_set_value_cansleep(external->led_en, 0);
		gpio_free(external->led_en);
		gpio_set_value_cansleep(external->led_flash_en, 0);
		gpio_free(external->led_flash_en);
		break;

	case MSM_CAMERA_LED_OFF:
		CDBG("MSM_CAMERA_LED_OFF\n");
		gpio_set_value_cansleep(external->led_en, 0);
		gpio_set_value_cansleep(external->led_flash_en, 0);
		break;

	case MSM_CAMERA_LED_LOW:
		CDBG("MSM_CAMERA_LED_LOW\n");
		gpio_set_value_cansleep(external->led_en, 1);
		gpio_set_value_cansleep(external->led_flash_en, 1);
		break;

	case MSM_CAMERA_LED_HIGH:
		CDBG("MSM_CAMERA_LED_HIGH\n");
		gpio_set_value_cansleep(external->led_en, 1);
		gpio_set_value_cansleep(external->led_flash_en, 1);
		break;

	default:
		rc = -EFAULT;
		break;
	}
	return rc;
#endif
}

int msm_camera_flash_external(
	struct msm_camera_sensor_flash_external *external,
	unsigned led_state)
{
	int rc = 0;

	switch (led_state) {

	case MSM_CAMERA_LED_INIT:
		if (external->flash_id == MAM_CAMERA_EXT_LED_FLASH_SC628A) {
			if (!sc628a_client) {
				rc = i2c_add_driver(&sc628a_i2c_driver);
				if (rc < 0 || sc628a_client == NULL) {
					pr_err("sc628a_i2c_driver add failed\n");
					rc = -ENOTSUPP;
					return rc;
				}
			}
		} else if (external->flash_id ==
			MAM_CAMERA_EXT_LED_FLASH_TPS61310) {
			if (!tps61310_client) {
				rc = i2c_add_driver(&tps61310_i2c_driver);
				if (rc < 0 || tps61310_client == NULL) {
					pr_err("tps61310_i2c_driver add failed\n");
					rc = -ENOTSUPP;
					return rc;
				}
			}
		} else {
			pr_err("Flash id not supported\n");
			rc = -ENOTSUPP;
			return rc;
		}

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
		if (external->expander_info && !sx150x_client) {
			struct i2c_adapter *adapter =
			i2c_get_adapter(external->expander_info->bus_id);
			if (adapter)
				sx150x_client = i2c_new_device(adapter,
					external->expander_info->board_info);
			if (!sx150x_client || !adapter) {
				pr_err("sx150x_client is not available\n");
				rc = -ENOTSUPP;
				if (sc628a_client) {
					i2c_del_driver(&sc628a_i2c_driver);
					sc628a_client = NULL;
				}
				if (tps61310_client) {
					i2c_del_driver(&tps61310_i2c_driver);
					tps61310_client = NULL;
				}
				return rc;
			}
			i2c_put_adapter(adapter);
		}
#endif
		if (sc628a_client)
			rc = gpio_request(external->led_en, "sc628a");
		if (tps61310_client)
			rc = gpio_request(external->led_en, "tps61310");

		if (!rc) {
			gpio_direction_output(external->led_en, 0);
		} else {
			goto error;
		}

		if (sc628a_client)
			rc = gpio_request(external->led_flash_en, "sc628a");
		if (tps61310_client)
			rc = gpio_request(external->led_flash_en, "tps61310");

		if (!rc) {
			gpio_direction_output(external->led_flash_en, 0);
			break;
		}

		if (sc628a_client || tps61310_client) {
			gpio_set_value_cansleep(external->led_en, 0);
			gpio_free(external->led_en);
		}
error:
		pr_err("%s gpio request failed\n", __func__);
		if (sc628a_client) {
			i2c_del_driver(&sc628a_i2c_driver);
			sc628a_client = NULL;
		}
		if (tps61310_client) {
			i2c_del_driver(&tps61310_i2c_driver);
			tps61310_client = NULL;
		}
		break;

	case MSM_CAMERA_LED_RELEASE:
		if (sc628a_client || tps61310_client) {
			gpio_set_value_cansleep(external->led_en, 0);
			gpio_free(external->led_en);
			gpio_set_value_cansleep(external->led_flash_en, 0);
			gpio_free(external->led_flash_en);
			if (sc628a_client) {
				i2c_del_driver(&sc628a_i2c_driver);
				sc628a_client = NULL;
			}
			if (tps61310_client) {
				i2c_del_driver(&tps61310_i2c_driver);
				tps61310_client = NULL;
			}
		}
#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
		if (external->expander_info && sx150x_client) {
			i2c_unregister_device(sx150x_client);
			sx150x_client = NULL;
		}
#endif
		break;

	case MSM_CAMERA_LED_OFF:
		if (sc628a_client || tps61310_client) {
			if (sc628a_client) {
				i2c_client.client = sc628a_client;
				i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
				rc = msm_camera_i2c_write(&i2c_client, 0x02,
					0x00, MSM_CAMERA_I2C_BYTE_DATA);
			}
			if (tps61310_client) {
				i2c_client.client = tps61310_client;
				i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
				rc = msm_camera_i2c_write(&i2c_client, 0x01,
					0x00, MSM_CAMERA_I2C_BYTE_DATA);
			}
			gpio_set_value_cansleep(external->led_en, 0);
			gpio_set_value_cansleep(external->led_flash_en, 0);
		}
		break;

	case MSM_CAMERA_LED_LOW:
		if (sc628a_client || tps61310_client) {
			gpio_set_value_cansleep(external->led_en, 1);
			gpio_set_value_cansleep(external->led_flash_en, 1);
			usleep_range(2000, 3000);
			if (sc628a_client) {
				i2c_client.client = sc628a_client;
				i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
				rc = msm_camera_i2c_write(&i2c_client, 0x02,
					0x06, MSM_CAMERA_I2C_BYTE_DATA);
			}
			if (tps61310_client) {
				i2c_client.client = tps61310_client;
				i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
				rc = msm_camera_i2c_write(&i2c_client, 0x01,
					0x86, MSM_CAMERA_I2C_BYTE_DATA);
			}
		}
		break;

	case MSM_CAMERA_LED_HIGH:
		if (sc628a_client || tps61310_client) {
			gpio_set_value_cansleep(external->led_en, 1);
			gpio_set_value_cansleep(external->led_flash_en, 1);
			usleep_range(2000, 3000);
			if (sc628a_client) {
				i2c_client.client = sc628a_client;
				i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
				rc = msm_camera_i2c_write(&i2c_client, 0x02,
					0x49, MSM_CAMERA_I2C_BYTE_DATA);
			}
			if (tps61310_client) {
				i2c_client.client = tps61310_client;
				i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
				rc = msm_camera_i2c_write(&i2c_client, 0x01,
					0x8B, MSM_CAMERA_I2C_BYTE_DATA);
			}
		}
		break;

	default:
		rc = -EFAULT;
		break;
	}
	return rc;
}

static int msm_camera_flash_pwm(
	struct msm_camera_sensor_flash_pwm *pwm,
	unsigned led_state)
{
	int rc = 0;
	int PWM_PERIOD = USEC_PER_SEC / pwm->freq;

	static struct pwm_device *flash_pwm;

	if (!flash_pwm) {
		flash_pwm = pwm_request(pwm->channel, "camera-flash");
		if (flash_pwm == NULL || IS_ERR(flash_pwm)) {
			pr_err("%s: FAIL pwm_request(): flash_pwm=%p\n",
			       __func__, flash_pwm);
			flash_pwm = NULL;
			return -ENXIO;
		}
	}

	switch (led_state) {
	case MSM_CAMERA_LED_LOW:
		rc = pwm_config(flash_pwm,
			(PWM_PERIOD/pwm->max_load)*pwm->low_load,
			PWM_PERIOD);
		if (rc >= 0)
			rc = pwm_enable(flash_pwm);
		break;

	case MSM_CAMERA_LED_HIGH:
		rc = pwm_config(flash_pwm,
			(PWM_PERIOD/pwm->max_load)*pwm->high_load,
			PWM_PERIOD);
		if (rc >= 0)
			rc = pwm_enable(flash_pwm);
		break;

	case MSM_CAMERA_LED_OFF:
		pwm_disable(flash_pwm);
		break;
	case MSM_CAMERA_LED_INIT:
	case MSM_CAMERA_LED_RELEASE:
		break;

	default:
		rc = -EFAULT;
		break;
	}
	return rc;
}

int msm_camera_flash_pmic(
	struct msm_camera_sensor_flash_pmic *pmic,
	unsigned led_state)
{
	int rc = 0;

	switch (led_state) {
	case MSM_CAMERA_LED_OFF:
		rc = pmic->pmic_set_current(pmic->led_src_1, 0);
		if (pmic->num_of_src > 1)
			rc = pmic->pmic_set_current(pmic->led_src_2, 0);
		break;

	case MSM_CAMERA_LED_LOW:
		rc = pmic->pmic_set_current(pmic->led_src_1,
				pmic->low_current);
		if (pmic->num_of_src > 1)
			rc = pmic->pmic_set_current(pmic->led_src_2, 0);
		break;

	case MSM_CAMERA_LED_HIGH:
		rc = pmic->pmic_set_current(pmic->led_src_1,
			pmic->high_current);
		if (pmic->num_of_src > 1)
			rc = pmic->pmic_set_current(pmic->led_src_2,
				pmic->high_current);
		break;

	case MSM_CAMERA_LED_INIT:
	case MSM_CAMERA_LED_RELEASE:
		 break;

	default:
		rc = -EFAULT;
		break;
	}
	CDBG("flash_set_led_state: return %d\n", rc);

	return rc;
}

int32_t msm_camera_flash_set_led_state(
	struct msm_camera_sensor_flash_data *fdata, unsigned led_state)
{
	int32_t rc;

	if (fdata->flash_type != MSM_CAMERA_FLASH_LED ||
		fdata->flash_src == NULL)
		return -ENODEV;

	switch (fdata->flash_src->flash_sr_type) {
	case MSM_CAMERA_FLASH_SRC_PMIC:
		rc = msm_camera_flash_pmic(&fdata->flash_src->_fsrc.pmic_src,
			led_state);
		break;

	case MSM_CAMERA_FLASH_SRC_PWM:
		rc = msm_camera_flash_pwm(&fdata->flash_src->_fsrc.pwm_src,
			led_state);
		break;

	case MSM_CAMERA_FLASH_SRC_CURRENT_DRIVER:
		rc = msm_camera_flash_current_driver(
			&fdata->flash_src->_fsrc.current_driver_src,
			led_state);
		break;

	case MSM_CAMERA_FLASH_SRC_EXT:
		rc = msm_camera_flash_external(
			&fdata->flash_src->_fsrc.ext_driver_src,
			led_state);
		break;

	case MSM_CAMERA_FLASH_SRC_LED1:
		rc = msm_camera_flash_led(
				&fdata->flash_src->_fsrc.ext_driver_src,
				led_state);
		break;

	default:
		rc = -ENODEV;
		break;
	}

	return rc;
}

static int msm_strobe_flash_xenon_charge(int32_t flash_charge,
		int32_t charge_enable, uint32_t flash_recharge_duration)
{
	gpio_set_value_cansleep(flash_charge, charge_enable);
	if (charge_enable) {
		timer_flash.expires = jiffies +
			msecs_to_jiffies(flash_recharge_duration);
		/* add timer for the recharge */
		if (!timer_pending(&timer_flash))
			add_timer(&timer_flash);
	} else
		del_timer_sync(&timer_flash);
	return 0;
}

static void strobe_flash_xenon_recharge_handler(unsigned long data)
{
	unsigned long flags;
	struct msm_camera_sensor_strobe_flash_data *sfdata =
		(struct msm_camera_sensor_strobe_flash_data *)data;

	spin_lock_irqsave(&sfdata->timer_lock, flags);
	msm_strobe_flash_xenon_charge(sfdata->flash_charge, 1,
		sfdata->flash_recharge_duration);
	spin_unlock_irqrestore(&sfdata->timer_lock, flags);

	return;
}

static irqreturn_t strobe_flash_charge_ready_irq(int irq_num, void *data)
{
	struct msm_camera_sensor_strobe_flash_data *sfdata =
		(struct msm_camera_sensor_strobe_flash_data *)data;

	/* put the charge signal to low */
	gpio_set_value_cansleep(sfdata->flash_charge, 0);

	return IRQ_HANDLED;
}

static int msm_strobe_flash_xenon_init(
	struct msm_camera_sensor_strobe_flash_data *sfdata)
{
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&sfdata->spin_lock, flags);
	if (!sfdata->state) {

		rc = config_flash_gpio_table(MSM_CAM_FLASH_ON, sfdata);
		if (rc < 0) {
			pr_err("%s: gpio_request failed\n", __func__);
			goto go_out;
		}
		rc = request_irq(sfdata->irq, strobe_flash_charge_ready_irq,
			IRQF_TRIGGER_RISING, "charge_ready", sfdata);
		if (rc < 0) {
			pr_err("%s: request_irq failed %d\n", __func__, rc);
			goto go_out;
		}

		spin_lock_init(&sfdata->timer_lock);
		/* setup timer */
		init_timer(&timer_flash);
		timer_flash.function = strobe_flash_xenon_recharge_handler;
		timer_flash.data = (unsigned long)sfdata;
	}
	sfdata->state++;
go_out:
	spin_unlock_irqrestore(&sfdata->spin_lock, flags);

	return rc;
}

static int msm_strobe_flash_xenon_release
(struct msm_camera_sensor_strobe_flash_data *sfdata, int32_t final_release)
{
	unsigned long flags;

	spin_lock_irqsave(&sfdata->spin_lock, flags);
	if (sfdata->state > 0) {
		if (final_release)
			sfdata->state = 0;
		else
			sfdata->state--;

		if (!sfdata->state) {
			free_irq(sfdata->irq, sfdata);
			config_flash_gpio_table(MSM_CAM_FLASH_OFF, sfdata);
			if (timer_pending(&timer_flash))
				del_timer_sync(&timer_flash);
		}
	}
	spin_unlock_irqrestore(&sfdata->spin_lock, flags);
	return 0;
}

static void msm_strobe_flash_xenon_fn_init
	(struct msm_strobe_flash_ctrl *strobe_flash_ptr)
{
	strobe_flash_ptr->strobe_flash_init =
				msm_strobe_flash_xenon_init;
	strobe_flash_ptr->strobe_flash_charge =
				msm_strobe_flash_xenon_charge;
	strobe_flash_ptr->strobe_flash_release =
				msm_strobe_flash_xenon_release;
}

int msm_strobe_flash_init(struct msm_sync *sync, uint32_t sftype)
{
	int rc = 0;
	switch (sftype) {
	case MSM_CAMERA_STROBE_FLASH_XENON:
		if (sync->sdata->strobe_flash_data) {
			msm_strobe_flash_xenon_fn_init(&sync->sfctrl);
			rc = sync->sfctrl.strobe_flash_init(
			sync->sdata->strobe_flash_data);
		} else
			return -ENODEV;
		break;
	default:
		rc = -ENODEV;
	}
	return rc;
}

int msm_strobe_flash_ctrl(struct msm_camera_sensor_strobe_flash_data *sfdata,
	struct strobe_flash_ctrl_data *strobe_ctrl)
{
	int rc = 0;
	switch (strobe_ctrl->type) {
	case STROBE_FLASH_CTRL_INIT:
		if (!sfdata)
			return -ENODEV;
		rc = msm_strobe_flash_xenon_init(sfdata);
		break;
	case STROBE_FLASH_CTRL_CHARGE:
		rc = msm_strobe_flash_xenon_charge(sfdata->flash_charge,
			strobe_ctrl->charge_en,
			sfdata->flash_recharge_duration);
		break;
	case STROBE_FLASH_CTRL_RELEASE:
		if (sfdata)
			rc = msm_strobe_flash_xenon_release(sfdata, 0);
		break;
	default:
		pr_err("Invalid Strobe Flash State\n");
		rc = -EINVAL;
	}
	return rc;
}

int msm_flash_ctrl(struct msm_camera_sensor_info *sdata,
	struct flash_ctrl_data *flash_info)
{
	int rc = 0;
	sensor_data = sdata;
	switch (flash_info->flashtype) {
	case LED_FLASH:
		rc = msm_camera_flash_set_led_state(sdata->flash_data,
			flash_info->ctrl_data.led_state);
			break;
	case STROBE_FLASH:
		rc = msm_strobe_flash_ctrl(sdata->strobe_flash_data,
			&(flash_info->ctrl_data.strobe_ctrl));
		break;
	default:
		pr_err("Invalid Flash MODE\n");
		rc = -EINVAL;
	}
	return rc;
}
