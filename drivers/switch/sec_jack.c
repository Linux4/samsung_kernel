/*
 *	drivers/switch/sec_jack.c
 *
 *	headset & hook detect driver for pm822 (SAMSUNG COMSTOM KSND)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm822.h>
#include <linux/mfd/88pm8xxx-headset.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/mfd/sec_jack.h>
#include <linux/input.h>
#include <linux/mutex.h>
#if defined(SEC_USE_ANLOGDOCK_DEVICE)
#include <mach/sm5502-muic.h>       /* Use Docking device for external audio */
#endif

#define PM822_HS_DET_INVERT		1

#define PM822_INT_ENA_3         0x0B
#define PM822_MIC_INT_ENA3      (1 << 4)

#define MAX_ZONE_LIMIT 			10

// 0x39 MICDET Reg
#define MIC_DET_DBS_MSK			(3 << 1)
#define MIC_DET_DBS_32MS		(3 << 1)
#define MIC_DET_PRD_MSK			(3 << 3)
#define MIC_DET_PRD_256MS		(1 << 3)
#define MIC_DET_PRD_512MS		(2 << 3)
#define MIC_DET_PRD_CTN			(3 << 3)

#if defined(COM_DET_CONTROL) //KSND
#include <mach/mfp-pxa986-goya.h>
#include <linux/gpio.h>

#define COM_DET_GPIO	GPIO097_GPIO_97
#endif


/* Headset detection info */
struct pm822_headset_info {
	struct pm822_chip *chip;
	struct device *dev;
	struct input_dev *idev;
	struct regmap *map;
	struct regmap *map_gpadc;
	int irq_headset;
	int irq_hook;
	struct work_struct work_headset, work_hook, work_release;
	struct headset_switch_data *psw_data_headset;
	struct sec_jack_platform_data *pdata;

	void (*mic_set_power) (int on);

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	void (*dock_audiopath_ctrl)(int on);
	void (*chgpump_ctrl)(int enable);
	int (*usb_switch_register_notify)(struct notifier_block *nb);
	int (*usb_switch_unregister_notify)(struct notifier_block *nb);
#endif /* SEC_USE_ANLOGDOCK_DEVICE */

	int headset_flag;
	int hook_vol_status;

	struct timer_list headset_timer;
	struct timer_list hook_timer;

	u32 hook_count;
#ifdef COM_DET_CONTROL
	unsigned long gpio_com;
#endif
	int pressed_code;

	struct mutex hs_mutex;
};

struct headset_switch_data {
	struct switch_dev sdev;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	int state;
};

static struct device *hsdetect_dev;
static struct PM8XXX_HS_IOCTL hs_detect;
static struct pm822_headset_info *local_info;  // KSND add for handling info 

#ifdef SAMSUNG_JACK_SW_WATERPROOF
static bool recheck_jack;
#endif

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
/* Use Docking device for external audio */
static int dock_state;
int jack_is_detected = 0;
EXPORT_SYMBOL(jack_is_detected);

static int dock_notify(struct notifier_block *self, unsigned long action, void *dev)
{
	dock_state = action;
	return NOTIFY_OK;
}

static struct notifier_block dock_nb = {
  .notifier_call = dock_notify,
};
#endif

static int gpadc4_measure_voltage(struct pm822_headset_info *info )
{
	unsigned char buf[2];
	int sum = 0, ret = -1;

	ret = regmap_raw_read(info->map_gpadc, PM822_MIC_DET_AVG1, buf, 2); //KSND  Fixed

	if (ret < 0){
	  pr_debug("%s: Fail to get the voltage!! \n", __func__);
	  return 0;
	}

	/* GPADC4_dir = 1, measure(mv) = value *1.4 *1000/(2^12) */
	sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
	sum = ((sum & 0xFFF) * 1400) >> 12;
	//pr_debug("the voltage is %d mv\n", sum);
	return sum;
}

static void gpadc4_set_threshold(struct pm822_headset_info *info, int min, int max)
{
	unsigned int data;
	if (min <= 0)
		data = 0;
	else
		data = ((min << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc, PM822_MIC_DET_LOW_TH, data);
	if (max <= 0)
		data = 0xFF;
	else
		data = ((max << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc, PM822_MIC_DET_UPP_TH, data);
}

static void pm822_hook_int(struct pm822_headset_info *info, int enable)
{
	mutex_lock(&info->hs_mutex);
	if (enable && info->hook_count == 0) {
		enable_irq(info->irq_hook);
		info->hook_count = 1;
	} else if (!enable && info->hook_count == 1) {
		disable_irq(info->irq_hook);
		info->hook_count = 0;
	}
	mutex_unlock(&info->hs_mutex);
}

//KSND
static int pm822_jack_key_handle(struct pm822_headset_info *info, int voltage)
{
	struct sec_jack_platform_data *pdata = info->pdata;
	struct sec_jack_buttons_zone *btn_zones = pdata->buttons_zones;
	int state;
	int i;

	if(voltage < pdata->press_release_th) {
		/* press event */
		if( info->hook_vol_status > HOOK_VOL_ALL_RELEASED ){
			return -EINVAL;
		}
			
		for (i = 0; i < pdata->num_buttons_zones; i++){
			if (voltage >= btn_zones[i].adc_low && voltage <= btn_zones[i].adc_high) {
				info->pressed_code = btn_zones[i].code;
				state = PM8XXX_HOOK_VOL_PRESSED;

				if( info->pressed_code == KEY_MEDIA ){
					hs_detect.hookswitch_status = state;
					/* for telephony */
					kobject_uevent(&hsdetect_dev->kobj, KOBJ_ONLINE);
					info->hook_vol_status = HOOK_PRESSED;
				} else if( info->pressed_code == KEY_VOLUMEDOWN ){
					info->hook_vol_status = VOL_DOWN_PRESSED;
				} else {
					info->hook_vol_status = VOL_UP_PRESSED;
				}
				input_report_key(info->idev, info->pressed_code, state);
				input_sync(info->idev);
				gpadc4_set_threshold(info, 0, pdata->press_release_th);
				pr_info("%s: keycode=%d, is pressed, adc= %d mv\n", __func__, info->pressed_code, voltage);
				return 0;
			}
		}
		pr_warn("%s: key is skipped. ADC value is %d\n", __func__, voltage);
	}
	else {
		/* release event */
		if(info->hook_vol_status <= HOOK_VOL_ALL_RELEASED){
			return -EINVAL;
		}
		state = PM8XXX_HOOK_VOL_RELEASED;

		if( info->pressed_code == KEY_MEDIA ){
			hs_detect.hookswitch_status = state;
			/* for telephony */
			kobject_uevent(&hsdetect_dev->kobj, KOBJ_ONLINE);
			info->hook_vol_status = HOOK_RELEASED;
		} else if( info->pressed_code == KEY_VOLUMEDOWN ){
			info->hook_vol_status = VOL_DOWN_RELEASED;
		} else {
			info->hook_vol_status = VOL_UP_RELEASED;
		}

		input_report_key(info->idev, info->pressed_code, state);
		input_sync(info->idev);
		gpadc4_set_threshold(info, pdata->press_release_th, 0);
		pr_info("%s: keycode=%d, is released, adc=%d\n", __func__,	info->pressed_code, voltage);
	}

	//pr_info("hook_vol switch to %d\n", info->hook_vol_status);
	return 0;
}

//KSND
static void pm822_jack_set_type(struct pm822_headset_info *info, int jack_type )
{
	struct headset_switch_data *switch_data;
	struct sec_jack_platform_data *pdata;
	int old_state;

	if( info == NULL ){
		pr_debug("[Panic]Faile to get headset_info pointer!!\n");
		return;
	}

	if( info->psw_data_headset == NULL || info->pdata == NULL ){
		pr_debug("[Panic]Invalid headset switch data! It's Null Pointer!\n");
		return;
	} else {
		switch_data = info->psw_data_headset;
		pdata =  info->pdata;
	}

	/* Backup the old switch state */
	old_state = switch_data->state;

	switch( jack_type ){
		case SEC_HEADSET_4POLE:
			/* Set to 4Pole Headset Case */
			switch_data->state = SEC_HEADSET_4POLE;
			hs_detect.hsdetect_status = PM8XXX_HEADSET_ADD;
			//hs_detect.hsmic_status = PM8XXX_HS_MIC_ADD;
			kobject_uevent(&hsdetect_dev->kobj, KOBJ_ADD);  /* for telephony */
			gpadc4_set_threshold(info, pdata->press_release_th, 0);
			/* enable hook irq if headset is 4 pole */
			pm822_hook_int(info, 1);
			break;

		case SEC_HEADSET_3POLE:
			/* Set to 3Pole Headset Case */
			switch_data->state = SEC_HEADSET_3POLE;
			hs_detect.hsdetect_status = PM8XXX_HEADPHONE_ADD;
			//hs_detect.hsmic_status = PM8XXX_HS_MIC_REMOVE;
			/*  No need MIC bias, disable mic power */
			if (info->mic_set_power){
				info->mic_set_power(0);
			}
			/* disable MIC detection and measurement */
			regmap_update_bits(info->map, PM822_MIC_CNTRL, PM822_MIC_DET_EN,0);
			break;

		case SEC_JACK_NO_DEVICE:
 			switch_data->state = SEC_JACK_NO_DEVICE;
			hs_detect.hsdetect_status = PM8XXX_HEADSET_REMOVE;
			kobject_uevent(&hsdetect_dev->kobj, KOBJ_REMOVE); /* for telephony */
			/*  No need MIC bias, disable mic power */
			if (info->mic_set_power) {
				info->mic_set_power(0);
			}
			/* disable MIC detection and measurement */
			regmap_update_bits(info->map, PM822_MIC_CNTRL, PM822_MIC_DET_EN,0);
			info->hook_vol_status = HOOK_VOL_ALL_RELEASED;
			/* disable the hook irq */
			regmap_update_bits(info->map, PM822_INT_ENA_3,
				PM822_MIC_INT_ENA3, 0);

			break;

		case SEC_UNKNOWN_DEVICE:
			/* No excute */
			pr_info("%s: jack:SEC_UNKNOWN_DEVICE!\n",__func__);
			return;

		default:
			pr_debug("Invalid sec jack type!\n");
			return;
	}

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	/* Use Docking device for external audio */
	jack_is_detected = switch_data->state;
#endif

	pr_info("%s: headset_switch_work %d to %d\n",__func__ , old_state, switch_data->state);
	switch_set_state(&switch_data->sdev, switch_data->state);
}

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
static void pm822_audio_dock_ctrl(struct pm822_headset_info *info, int enable)
{
	if( info->chgpump_ctrl == NULL || info->dock_audiopath_ctrl == NULL ){
		pr_debug("Invalid audio dock interface pointer!\n");
		return;
	}

	/* Use Docking device for external audio */
	if ( enable ){
		/* Output through the Dock device */
		info->chgpump_ctrl(0);
		if( dock_state == CABLE_TYPE2_DESKDOCK_MUIC 
		 || dock_state == CABLE_TYPE3_DESKDOCK_VB_MUIC){
			info->dock_audiopath_ctrl(1);
		}
	} else {
		/* Stop the path of Dock device */
		info->chgpump_ctrl(1);
		if( dock_state == CABLE_TYPE2_DESKDOCK_MUIC
		 || dock_state == CABLE_TYPE3_DESKDOCK_VB_MUIC){
			info->dock_audiopath_ctrl(0);
		}
	}
}
#endif

static void pm822_jack_det_work(struct work_struct *work)
{
	struct pm822_headset_info *info =
	    container_of(work, struct pm822_headset_info, work_headset);
	unsigned int value, voltage;
	int ret, i, com_value;
	int count[MAX_ZONE_LIMIT] = {0};
	struct headset_switch_data *switch_data;
#ifdef SAMSUNG_JACK_SW_WATERPROOF
	int reselector_zone = info->pdata->ear_reselector_zone;
#endif

	switch_data = info->psw_data_headset;
	if (switch_data == NULL) {
		pr_debug("Invalid headset switch data!\n");
		return;
	}

	ret = regmap_read(info->map, PM822_MIC_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read PM822_MIC_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM822_HEADSET_DET_MASK;

	if (info->headset_flag == PM822_HS_DET_INVERT)
		value = !value;

#ifdef COM_DET_CONTROL
	com_value = gpio_get_value(mfp_to_gpio(COM_DET_GPIO)); /* get com detecting status - ffkaka */
	//pr_info("%s : com_value = %d\n",__func__, com_value);
	if(value && com_value){
		pr_info("[headset] L-det checked, G-Det not checked.\n");
		mod_timer(&info->headset_timer, jiffies + msecs_to_jiffies(100));
		return;
	}
#else
	com_value = 0;
#endif

	if( value && (!com_value)){
		/* MIC_BIAS on.  Enable mic power when it is headphone */
		if (info->mic_set_power) {
			info->mic_set_power(1);
		}
		
		/* enable MIC detection also enable measurement */
		regmap_update_bits(info->map, PM822_MIC_CNTRL, PM822_MIC_DET_EN,PM822_MIC_DET_EN);
		msleep(100);

		while(value && (!com_value)){
			/* Get ADC value */
			voltage = gpadc4_measure_voltage(info);
			pr_info("%s: adc = %d mv\n", __func__, voltage);
			
			for (i = 0; i < info->pdata->num_zones; i++) {
				if (voltage <= info->pdata->zones[i].adc_high) {
					if (++count[i] > info->pdata->zones[i].check_count) {			
#ifdef SAMSUNG_JACK_SW_WATERPROOF
						if ((recheck_jack == true) && (reselector_zone < voltage)) {
							pr_info("%s : something wrong connectoin! reselect_zone: %d, voltage: %d\n",__func__, reselector_zone, voltage);
							pm822_jack_set_type(info, SEC_JACK_NO_DEVICE );
							recheck_jack = false;
							return;
						}
#endif
						// Set Earjack type (3 pole or 4 pole)
						pm822_jack_set_type(info, info->pdata->zones[i].jack_type);
#if defined(SEC_USE_ANLOGDOCK_DEVICE)
						/* Use Docking device for external audio */
						pm822_audio_dock_ctrl(info,0);  //output only headset
#endif
						return;
					}
					if (info->pdata->zones[i].delay_ms > 0)
						msleep(info->pdata->zones[i].delay_ms);
					break;
				}
			}
		}
		/* jack detection is failed */
		pr_info("%s : detection is failed\n", __func__);
		pm822_jack_set_type(info, SEC_JACK_NO_DEVICE );
	} else {
		pr_info("%s : disconnected Headset \n", __func__);
		del_timer(&info->hook_timer); //KSND
		pm822_jack_key_handle(info, info->pdata->press_release_th); //KSND
		pm822_jack_set_type(info, SEC_JACK_NO_DEVICE );
#if defined(SEC_USE_ANLOGDOCK_DEVICE)
		/* Use Docking device for external audio */
		pm822_audio_dock_ctrl(info,1); //Output only Dock device
#endif
#ifdef SAMSUNG_JACK_SW_WATERPROOF
		recheck_jack = false;
#endif /* SAMSUNG_JACK_SW_WATERPROOF */
	}
}

static void pm822_jack_timer_handler(unsigned long data)
{
	struct pm822_headset_info *info = (struct pm822_headset_info *)data;
	if (info->psw_data_headset != NULL) {
		queue_work(system_wq, &info->work_headset);
	}	
}

static void pm822_button_timer_handler(unsigned long data)
{
	struct pm822_headset_info *info = (struct pm822_headset_info *)data;

	pr_info("%s:\n", __func__);
	queue_work(system_wq, &info->work_release);
}

static void pm822_jack_release_work(struct work_struct *work)
{
	struct pm822_headset_info *info =
	    container_of(work, struct pm822_headset_info, work_release);
	struct sec_jack_platform_data *pdata = info->pdata;

	unsigned int voltage;

	if (info->hook_vol_status >= HOOK_PRESSED) {
		voltage = gpadc4_measure_voltage(info);
		if (voltage >= pdata->press_release_th) //KSND
//			pm822_handle_voltage(info, voltage);
			pm822_jack_key_handle(info, voltage);
		}
}

static void pm822_jack_buttons_work(struct work_struct *work)
{
	struct pm822_headset_info *info =
	    container_of(work, struct pm822_headset_info, work_hook);
	int ret;
	unsigned int value, voltage;

	if (info == NULL || info->idev == NULL) {
		pr_debug("Invalid hook info!\n");
		return;
	}
#if defined (CONFIG_MACH_GOYA)
	msleep(100);  // KSND
#else
	msleep(50);  // KSND
#endif
	ret = regmap_read(info->map, PM822_MIC_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read PM822_MIC_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM822_HEADSET_DET_MASK;

	if (info->headset_flag == PM822_HS_DET_INVERT)
		value = !value;

	voltage = gpadc4_measure_voltage(info);

	if ( !value ) {
		/* in case of headset unpresent, disable hook interrupt */
		pm822_hook_int(info, 0);
		pr_info("[headset] fake hook interupt : %d\n", voltage);
		return;
	}

	//KSND
	if(hs_detect.hsdetect_status != PM8XXX_HEADSET_ADD){
		pm822_hook_int(info, 0);
		pr_info("headset removed - discard jack buttons\n");
		return;
	}

	//pm822_handle_voltage(info, voltage);
	pm822_jack_key_handle(info, voltage);
	//Patch for avoid input twice interrupt case
	ret = regmap_read(info->map, PM822_MIC_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev,
			"Failed to read PM822_MIC_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM822_HEADSET_DET_MASK;

	if (info->headset_flag == PM822_HS_DET_INVERT)
		value = !value;

	if (value)
		regmap_update_bits(info->map, PM822_INT_ENA_3,
			PM822_MIC_INT_ENA3, PM822_MIC_INT_ENA3);

}

static irqreturn_t pm822_jack_detect_irq_thread(int irq, void *data)
{
	struct pm822_headset_info *info = data;
	pr_info("%s:\n", __func__);

	if (info->psw_data_headset != NULL) {
		/*
		 * in any case headset interrupt comes,
		 * hook interrupt is not welcomed. so just
		 * disable it.
		 */
		pm822_hook_int(info, 0);
		queue_work(system_wq, &info->work_headset);
	}
	return IRQ_HANDLED;
}

static irqreturn_t pm822_jack_buttons_irq_thread(int irq, void *data)
{
	struct pm822_headset_info *info = data;
	pr_info("%s:\n", __func__);

	mod_timer(&info->hook_timer, jiffies + msecs_to_jiffies(60));
	/* hook interrupt */
	if (info->idev != NULL){
		//Patch for avoid input twice interrupt case
		regmap_update_bits(info->map, PM822_INT_ENA_3, PM822_MIC_INT_ENA3, 0);
		queue_work(system_wq, &info->work_hook);
	}
	return IRQ_HANDLED;
}

static ssize_t switch_headset_print_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", switch_get_state(sdev));
}

#ifdef SEC_SYSFS_FOR_FACTORY_TEST //KSND
/*************************************
  * add sysfs for factory test. KSND 131029
  * 
 *************************************/
static ssize_t  key_state_onoff_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", (hs_detect.hookswitch_status)? "1":"0");
}

static ssize_t  earjack_state_onoff_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hs_detect.hsdetect_status);
}

static ssize_t select_jack_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);
	return 0;
}

static ssize_t select_jack_store(struct device *dev, 	struct device_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	sscanf(buf, "%d", &value);
	pr_info("%s: User  selection : 0X%x", __func__, value);
	if (value == PM8XXX_HEADSET_ADD) {
		if (local_info->mic_set_power) {
			local_info->mic_set_power(1);

			/* enable MIC detection also enable measurement */
			regmap_update_bits(local_info->map, PM822_MIC_CNTRL, PM822_MIC_DET_EN,PM822_MIC_DET_EN);
			msleep(100);
		}
	}
	local_info->psw_data_headset->state = value;
	return size;
}

#ifdef SAMSUNG_JACK_SW_WATERPROOF
static ssize_t reselect_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);
	return 0;
}

static ssize_t reselect_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct pm822_headset_info *info = dev_get_drvdata(dev);
	int value = 0;

	sscanf(buf, "%d", &value);
	pr_err("%s: User  selection : 0X%x", __func__, value);

	if (value == 1) {
		recheck_jack = true;
		pm822_jack_det_work(&info->work_headset);
	}
	return size;
}

static DEVICE_ATTR(reselect_jack,0664,reselect_jack_show,reselect_jack_store);
#endif

static DEVICE_ATTR(key_state, 0664 , key_state_onoff_show, NULL);
static DEVICE_ATTR(state, 0664 , earjack_state_onoff_show, NULL);
static DEVICE_ATTR(select_jack, 0664, select_jack_show, 	select_jack_store);
#endif /* SEC_SYSFS_FOR_FACTORY_TEST */

static int pm822_headset_switch_probe(struct platform_device *pdev)
{
	struct pm822_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm822_headset_info *info;
	struct gpio_switch_platform_data *pdata_headset = pdev->dev.platform_data;
	struct headset_switch_data *switch_data_headset = NULL;
	struct pm822_platform_data *pm822_pdata;
#ifdef SEC_SYSFS_FOR_FACTORY_TEST //KSND
	struct class *audio;
	struct device *earjack;
#endif /* SEC_SYSFS_FOR_FACTORY_TEST */
	int irq_headset, irq_hook, ret = 0;

	if (pdev->dev.parent->platform_data) {
		pm822_pdata = pdev->dev.parent->platform_data;
	} else {
		pr_debug("Invalid pm822 platform data!\n");
		return -EINVAL;
	}
	if (!pm822_pdata->headset) {
		dev_err(&pdev->dev, "No headset platform info!\n");
		return -EINVAL;
	}
	if (pdata_headset == NULL) {
		pr_debug("Invalid gpio switch platform data!\n");
		return -EBUSY;
	}
	irq_headset = platform_get_irq(pdev, 0);
	if (irq_headset < 0) {
		dev_err(&pdev->dev, "No IRQ resource for headset!\n");
		return -EINVAL;
	}
	irq_hook = platform_get_irq(pdev, 1);
	if (irq_hook < 0) {
		dev_err(&pdev->dev, "No IRQ resource for hook/mic!\n");
		return -EINVAL;
	}
	info =
	    devm_kzalloc(&pdev->dev, sizeof(struct pm822_headset_info),
			 GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->pdata = pm822_pdata->headset; //KSND

	info->chip = chip;
	info->mic_set_power = pm822_pdata->headset->mic_set_power;
	info->headset_flag = pm822_pdata->headset->headset_flag;
	info->map = chip->regmap;
	info->map_gpadc = chip->subchip->regmap_gpadc;
	info->dev = &pdev->dev;
	info->irq_headset = irq_headset + chip->irq_base;
	info->irq_hook = irq_hook + chip->irq_base;
#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	info->chgpump_ctrl = pm822_pdata->headset->chgpump_ctrl;
	info->dock_audiopath_ctrl = pm822_pdata->headset->dock_audiopath_ctrl;
	info->usb_switch_register_notify=  pm822_pdata->headset->usb_switch_register_notify;
	info->usb_switch_unregister_notify=  pm822_pdata->headset->usb_switch_unregister_notify;
#endif

	info->idev = input_allocate_device();
	if (!info->idev) {
		dev_err(&pdev->dev, "Failed to allocate input dev\n");
		ret = -ENOMEM;
		goto input_allocate_fail;
	}

	info->idev->name = "88pm822_hook_vol";
	info->idev->phys = "88pm822_hook_vol/input0";
	info->idev->id.bustype = BUS_I2C;
	info->idev->dev.parent = &pdev->dev;

	__set_bit(EV_KEY, info->idev->evbit);
	__set_bit(KEY_MEDIA, info->idev->keybit);
	__set_bit(KEY_VOLUMEUP, info->idev->keybit);
	__set_bit(KEY_VOLUMEDOWN, info->idev->keybit);
	info->hook_vol_status = HOOK_VOL_ALL_RELEASED;

	switch_data_headset =
	    devm_kzalloc(&pdev->dev, sizeof(struct headset_switch_data),GFP_KERNEL);
	if (!switch_data_headset) {
		dev_err(&pdev->dev, "Failed to allocate headset data\n");
		ret = -ENOMEM;
		goto headset_allocate_fail;
	}

	switch_data_headset->sdev.name = pdata_headset->name;
	switch_data_headset->name_on = pdata_headset->name_on;
	switch_data_headset->name_off = pdata_headset->name_off;
	switch_data_headset->state_on = pdata_headset->state_on;
	switch_data_headset->state_off = pdata_headset->state_off;
	switch_data_headset->sdev.print_state = switch_headset_print_state;
	info->psw_data_headset = switch_data_headset;

	ret = switch_dev_register(&switch_data_headset->sdev);
	if (ret < 0)
		goto err_switch_dev_register;
		
	mutex_init(&info->hs_mutex);		

	/* Initialize timer for detecting loop */
	setup_timer(&info->headset_timer, pm822_jack_timer_handler,(unsigned long)info);

	/* init timer for hook release */
	setup_timer(&info->hook_timer, pm822_button_timer_handler,(unsigned long)info);

	INIT_WORK(&info->work_headset, pm822_jack_det_work);
	INIT_WORK(&info->work_hook, pm822_jack_buttons_work);
	INIT_WORK(&info->work_release, pm822_jack_release_work);

	ret = request_threaded_irq(info->irq_headset, NULL, pm822_jack_detect_irq_thread,
				 IRQF_ONESHOT, "headset", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_headset, ret);
		goto out_irq_headset;
	}

	ret = request_threaded_irq(info->irq_hook, NULL, pm822_jack_buttons_irq_thread,
				 IRQF_ONESHOT, "hook", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_hook, ret);
		goto out_irq_hook;
	}
	
#ifdef COM_DET_CONTROL 
	info->gpio_com = mfp_to_gpio(COM_DET_GPIO);
	ret = gpio_request(info->gpio_com,"COM_DET CTRL");
		
	if(ret){
		gpio_free(info->gpio_com);
		pr_info("%s: COM_DET CTRL Request Fail!!\n", __func__);
	}
	gpio_direction_input(info->gpio_com);
#endif

	/*
	 * disable hook irq to ensure this irq will be enabled
	 * after plugging in headset
	 */
	info->hook_count = 1;
	pm822_hook_int(info, 0);
	ret = input_register_device(info->idev);
	if (ret < 0)
		goto err_input_dev_register;

	platform_set_drvdata(pdev, info);
	hsdetect_dev = &pdev->dev;
	hs_detect.hsdetect_status = 0;
	hs_detect.hookswitch_status = 0;

	/* Pointer for handling info in this driver KSND */
	local_info = info;

	/* default 4_POLES */
	//hs_detect.hsmic_status = PM8XXX_HS_MIC_ADD;

	/* Hook:32 ms debounce time */
	regmap_update_bits(info->map, PM822_MIC_CNTRL, MIC_DET_DBS_MSK,
			   MIC_DET_DBS_32MS);
	/* Hook:continue duty cycle */
	regmap_update_bits(info->map, PM822_MIC_CNTRL, MIC_DET_PRD_MSK,
			   MIC_DET_PRD_256MS);
	/* set GPADC_DIR to 0, set to enable internal register divider (1:4) */
	regmap_update_bits(info->map_gpadc, PM822_GPADC_MISC_CONFIG1,
			   PM822_GPADC4_DIR, 0);
	/* enable headset detection on GPIO3 */
	regmap_update_bits(info->map, PM822_HEADSET_CNTRL, PM822_HEADSET_DET_EN,
						PM822_HEADSET_DET_EN);
	pm822_jack_det_work(&info->work_headset);

#ifdef SEC_SYSFS_FOR_FACTORY_TEST //KSND
	/* sys fs for factory test */
	audio = class_create(THIS_MODULE, "audio");
	if (IS_ERR(audio))
		pr_err("Failed to create class(audio)!\n");

	earjack = device_create(audio, NULL, 0, NULL, "earjack");
	if (IS_ERR(earjack))
		pr_err("Failed to create device(earjack)!\n");

	ret = device_create_file(earjack, &dev_attr_key_state);
	if (ret)
		pr_err("Failed to create device file in sysfs entries!\n");

	ret = device_create_file(earjack, &dev_attr_state);
	if (ret)
		pr_err("Failed to create device file in sysfs entries!\n");
    
	ret = device_create_file(earjack, &dev_attr_select_jack);
	if (ret)
		pr_err("Failed to create device file in sysfs entries(%s)!\n", dev_attr_select_jack.attr.name);

	dev_set_drvdata(earjack, info);  //KSND  
#endif /* SEC_SYSFS_FOR_FACTORY_TEST */

#ifdef SAMSUNG_JACK_SW_WATERPROOF
	ret = device_create_file(earjack, &dev_attr_reselect_jack);
	if (ret)
		pr_err("Failed to create device file in sysfs entries(%s)!\n", dev_attr_reselect_jack.attr.name);
#endif

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	/* Use Docking device for external audio */
	if( info->usb_switch_register_notify != NULL )
		info->usb_switch_register_notify(&dock_nb);
#endif

	return 0;

err_input_dev_register:
	free_irq(info->irq_hook, info);
out_irq_hook:
	free_irq(info->irq_headset, info);
out_irq_headset:
	devm_kfree(&pdev->dev, switch_data_headset);
err_switch_dev_register:
	switch_dev_unregister(&switch_data_headset->sdev);
headset_allocate_fail:
	input_free_device(info->idev);
input_allocate_fail:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int __devexit pm822_headset_switch_remove(struct platform_device *pdev)
{
	struct pm822_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data_headset =
	    info->psw_data_headset;
	/* disable headset detection on GPIO3 */
	regmap_update_bits(info->map, PM822_HEADSET_CNTRL, PM822_HEADSET_DET_EN,0);

	cancel_work_sync(&info->work_headset);
	cancel_work_sync(&info->work_hook);
	cancel_work_sync(&info->work_release);

	free_irq(info->irq_hook, info);
	free_irq(info->irq_headset, info);

#ifdef COM_DET_CONTROL
	/* Disable GPIO control KSND */
	gpio_free(info->gpio_com);
#endif /* COM_DET_CONTROL */

	switch_dev_unregister(&switch_data_headset->sdev);
	input_unregister_device(info->idev);

	input_free_device(info->idev);
	devm_kfree(&pdev->dev, switch_data_headset);
	devm_kfree(&pdev->dev, info);

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	/* Use Docking device for external audio */
	if( info->usb_switch_register_notify != NULL )
		info->usb_switch_unregister_notify(&dock_nb);
#endif

	return 0;
}

static int pm822_headset_switch_suspend(struct platform_device *pdev,	pm_message_t state)
{
	return 0;
}

static int pm822_headset_switch_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver pm822_headset_switch_driver = {
	.probe = pm822_headset_switch_probe,
	.remove = __devexit_p(pm822_headset_switch_remove),
	.suspend = pm822_headset_switch_suspend,
	.resume = pm822_headset_switch_resume,
	.driver = {
		   .name = "88pm822-headset",
		   .owner = THIS_MODULE,
		   },
};

static struct miscdevice pm822_hsdetect_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "88pm822-headset",
};

static int __init headset_switch_init(void)
{
	int ret = -EINVAL;
	ret = misc_register(&pm822_hsdetect_miscdev);
	if (ret < 0)
		return ret;
	ret = platform_driver_register(&pm822_headset_switch_driver);
	return ret;
}

static void __exit headset_switch_exit(void)
{
	platform_driver_unregister(&pm822_headset_switch_driver);
	misc_deregister(&pm822_hsdetect_miscdev);
}

module_init(headset_switch_init);
module_exit(headset_switch_exit);

MODULE_DESCRIPTION("Samsung Custom 88PM822 Headset driver");
MODULE_AUTHOR("Youngki Park <ykp74@samsung.com>");
MODULE_LICENSE("GPL");
