/*
 *	drivers/switch/sec_jack.c
 *
 *	headset & hook detect driver for pm800 (SAMSUNG COMSTOM KSND)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm80x.h>
#include <linux/mfd/88pm8xxx-headset.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/mfd/sec_jack.h>
#include <linux/mutex.h>

#define	PM800_HEADSET_PROC_FILE		"driver/pm800_headset"
static int voice_call_enable;

#define PM800_HS_DET_INVERT    1

#define PM800_MIC_CNTRL        (0x39)
#define PM800_MICDET_EN        (1 << 0)
#define MIC_DET_DBS            (3 << 1)
#define MIC_DET_PRD            (3 << 3)
#define MIC_DET_DBS_32MS       (3 << 1)
#define MIC_DET_PRD_CTN        (3 << 3)
#define MIC_DET_PRD_CTN_256MS  (1 << 3)

#define PM800_GPADC4_AVG1      0xA8

#define PM800_INT_ENA_3        (0x0B)
#define PM800_INT_ENA_4        (0x0C)
#define PM800_GPIO3_INT_ENA4   (1 << 3)
#define PM800_GPADC4_INT_ENA3  (1 << 4)
#define PM800_GPADC4_DIR       (1 << 6)

#define MAX_ZONE_LIMIT 10

/* Enable GPIO Control FET_EN for ADC Function*/
#define FET_EN_CONTROL

/* Enable GPIO Control COM_DET for Earjack Detection*/
#define COM_DET_CONTROL

/* Enable GPIO Control HP_MUTE */
#define HP_MUTE_CONTROL

#if defined(FET_EN_CONTROL) || defined(HP_MUTE_CONTROL)|| defined(COM_DET_CONTROL) //KSND
#if defined(CONFIG_MACH_LT02)
#include <mach/mfp-pxa986-lt02.h>
#elif defined(CONFIG_MACH_COCOA7)
#include <mach/mfp-pxa986-cocoa7.h>
#else
#error wrong model feature
#endif
#include <linux/gpio.h>
#endif
#if defined(SEC_USE_ANLOGDOCK_DEVICE)
#include <mach/fsa9480.h>
#endif

struct pm800_headset_info {
	struct pm80x_chip *chip;
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
#endif

	/* Headset Data */
	int headset_flag;
	int hook_vol_status;
	struct proc_dir_entry *proc_file;
	struct timer_list headset_timer;
	struct timer_list button_timer; //KSND
#if defined (COM_DET_CONTROL)
	struct timer_list gdet_timer;
#endif

	u32 hook_count;
#ifdef FET_EN_CONTROL //KSND
	unsigned long gpio_fet_en;   
#endif /* FET_EN_CONTROL */
#if defined (COM_DET_CONTROL)
	unsigned int irq_com;
	unsigned gpio_com;
#endif
#ifdef HP_MUTE_CONTROL
	unsigned long gpio_hp_mute;
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
static struct pm800_headset_info *local_info;  // KSND add for handling info 

static int com_irq = 0;
static char *sec_button_status[] = {
	"Hook",
	"Vol_Up",
	"Vol_Down",
};

extern unsigned int system_rev;

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
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

static int gpadc4_measure_voltage(struct pm800_headset_info *info )
{
	unsigned char buf[2];
	int sum = 0, ret = -1;

  ret = regmap_raw_read(info->map_gpadc, PM800_GPADC4_AVG1, buf, 2); //KSND fixed
	if (ret < 0){
    pr_debug("%s: Fail to get the voltage!! \n", __func__);
    return 0;
  }

	/* GPADC4_dir = 1, measure(mv) = value *1.4 *1000/(2^12) */
	sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
	sum = ((sum & 0xFFF) * 1400) >> 12;
	//pr_debug("%s: the voltage is %d mv\n", __func__,sum);
	return sum;
}

static void gpadc4_set_threshold(struct pm800_headset_info *info, int min, int max)
{
	unsigned int data;
	if (min <= 0)
		data = 0;
	else
		data = ((min << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc, PM800_GPADC4_LOW_TH, data);
	if (max <= 0)
		data = 0xFF;
	else
		data = ((max << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc, PM800_GPADC4_UPP_TH, data);
}

static void pm800_hook_int(struct pm800_headset_info *info, int enable)
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

#if defined (COM_DET_CONTROL)
static void pm800_gdet_int(struct pm800_headset_info *info, int enable)
{
	if (enable && com_irq == 0) {
		enable_irq(info->irq_com);
		com_irq = 1;
	} else if (!enable && com_irq == 1) {
		disable_irq(info->irq_com);
		com_irq = 0;
	}
}
#endif

static void pm800_button_handle(struct pm800_headset_info *info, int voltage)
{
	struct sec_jack_platform_data *pdata = info->pdata;
	struct sec_jack_buttons_zone *btn_zones = pdata->buttons_zones;
	int state;
	int i, button;

	if(voltage < pdata->press_release_th) {
		/* press event */
		if( info->hook_vol_status > HOOK_VOL_ALL_RELEASED ){
			/* No Need to sent the event */
			return;
		}
			
		for (i = 0; i < pdata->num_buttons_zones; i++){
			if (voltage >= btn_zones[i].adc_low && voltage <= btn_zones[i].adc_high) {
				info->pressed_code = btn_zones[i].code;
				state = PM8XXX_HOOK_VOL_PRESSED;

				if( info->pressed_code == KEY_MEDIA ){
					hs_detect.hookswitch_status = state;
					info->hook_vol_status = HOOK_PRESSED;
					button = 0;
					/* for telephony */
					kobject_uevent(&hsdetect_dev->kobj, KOBJ_ONLINE);
				} else if( info->pressed_code == KEY_VOLUMEUP ){
					info->hook_vol_status = VOL_UP_PRESSED;
					button = 1;
				} else {
					info->hook_vol_status = VOL_DOWN_PRESSED;
					button = 2;
				}
				input_report_key(info->idev, info->pressed_code, state);
				input_sync(info->idev);
				gpadc4_set_threshold(info, 0, pdata->press_release_th);

				pr_info("%s: [%s] is pressed![%d mV]\n", __func__, sec_button_status[button], voltage);
				return;
			}
		}
		pr_warn("%s: key is skipped. [%d mv]\n", __func__, voltage);
	}
	else {
		/* release event */
		if(info->hook_vol_status <= HOOK_VOL_ALL_RELEASED){
			/* No Need to sent the event */
			return;
		}
		state = PM8XXX_HOOK_VOL_RELEASED;

		if( info->pressed_code == KEY_MEDIA ){
			hs_detect.hookswitch_status = state;
			info->hook_vol_status = HOOK_RELEASED;
			button = 0;
			/* for telephony */
			kobject_uevent(&hsdetect_dev->kobj, KOBJ_ONLINE);
		} else if( info->pressed_code == KEY_VOLUMEUP ){
			info->hook_vol_status = VOL_UP_RELEASED;
			button = 1;
		} else {
			info->hook_vol_status = VOL_DOWN_RELEASED;
			button = 2;
		}

		input_report_key(info->idev, info->pressed_code, state);
		input_sync(info->idev);
		gpadc4_set_threshold(info, pdata->press_release_th, 0);

		pr_info("%s: [%s] is released![%d mV]\n", __func__, sec_button_status[button], voltage);
	}
}


static void pm800_jack_set_type(struct pm800_headset_info *info, int jack_type ) 	//KSND
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

	if (jack_type != SEC_JACK_NO_DEVICE) 
	{
		if(jack_type == SEC_HEADSET_4POLE){
			/* Set to 4Pole Headset Case */
			switch_data->state = SEC_HEADSET_4POLE;
			hs_detect.hsdetect_status = PM8XXX_HEADSET_ADD;
			kobject_uevent(&hsdetect_dev->kobj, KOBJ_ADD);
			gpadc4_set_threshold(info, pdata->press_release_th, 0);
			/* enable hook irq if headset is 4 pole */
			pm800_hook_int(info, 1);
		}
		else if(jack_type == SEC_HEADSET_3POLE){
			/* Set to 3Pole Headset Case */
			switch_data->state = SEC_HEADSET_3POLE;
			hs_detect.hsdetect_status = PM8XXX_HEADPHONE_ADD;
			hs_detect.hsmic_status = PM8XXX_HS_MIC_REMOVE;
			if (info->mic_set_power)
				info->mic_set_power(0);
			/* disable MIC detection and measurement */
			regmap_update_bits(info->map, PM800_MIC_CNTRL, PM800_MICDET_EN, 0);
		} 
	}
	else {	
	/* Disconnect headset case */
#ifdef FET_EN_CONTROL
		//KSND set GPIO8 FET_EN to Low
		if(system_rev < LT02_R0_3)
			gpio_direction_output(info->gpio_fet_en,1);
#endif
#ifdef HP_MUTE_CONTROL
		if(system_rev < LT02_R0_2)
			gpio_direction_output(info->gpio_hp_mute,1);
#endif
		/* we already disable mic power when it is headphone */
		if (info->mic_set_power) 
			info->mic_set_power(0);

		/* disable MIC detection and measurement */
		regmap_update_bits(info->map, PM800_MIC_CNTRL, PM800_MICDET_EN, 0);
		switch_data->state = SEC_JACK_NO_DEVICE;
		info->hook_vol_status = HOOK_VOL_ALL_RELEASED;

		/* for telephony */
		kobject_uevent(&hsdetect_dev->kobj, KOBJ_REMOVE);
		hs_detect.hsdetect_status = PM8XXX_HEADSET_REMOVE; 
		hs_detect.hsmic_status = PM8XXX_HS_MIC_ADD;
	}

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	/* Use Docking device for external audio */
	jack_is_detected = switch_data->state;
#endif

	switch_set_state(&switch_data->sdev, switch_data->state);
	pr_info("%s: headset_switch_work %d to %d\n",__func__ , old_state, switch_data->state);
}

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
static void pm800_audio_dock_ctrl(struct pm800_headset_info *info, int enable)
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

static void pm800_jack_detect_work(struct work_struct *work)
{
	struct pm800_headset_info *info =
	    container_of(work, struct pm800_headset_info, work_headset);
	unsigned int value, voltage;
	int ret, i, com_value;
	int count[MAX_ZONE_LIMIT] = {0};

	ret = regmap_read(info->map, PM800_GPIO_2_3_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read PM800_GPIO_2_3_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM800_GPIO3_VAL;
	if (info->headset_flag == PM800_HS_DET_INVERT)
		value = !value;

	com_value = gpio_get_value(mfp_to_gpio(GPIO012_GPIO_12));

	/* Retry for check GND status */
	if(value && com_value){
		pr_info("[headset] L-det checked, G-Det not checked.\n");
		mod_timer(&info->headset_timer, jiffies + msecs_to_jiffies(100));
		return;
	}
	else if(value && (!com_value)){
		/* MIC_BIAS on */
#ifdef FET_EN_CONTROL
		//KSND set GPIO8 FET_EN to High
		if(system_rev < LT02_R0_3)
			gpio_direction_output(info->gpio_fet_en,0);
#endif
#ifdef HP_MUTE_CONTROL
		if(system_rev < LT02_R0_2)
			gpio_direction_output(info->gpio_hp_mute,0);
#endif
		/* set mic bias to enable adc */
		if (info->mic_set_power)
			info->mic_set_power(1);
		/* enable MIC detection also enable measurement */
		regmap_update_bits(info->map, PM800_MIC_CNTRL, PM800_MICDET_EN, PM800_MICDET_EN);
		msleep(100);

#if defined (COM_DET_CONTROL)
		pm800_gdet_int(info,1);
#endif

		/* determine the type of headset based on the
		 * adc value.  An adc value can fall in various
		 * ranges or zones.  Within some ranges, the type
		 * can be returned immediately.  Within others, the
		 * value is considered unstable and we need to sample
		 * a few more types (up to the limit determined by
		 * the range) before we return the type for that range.
		 */
		while(value && !(com_value)){
			/* Get ADC value */
			voltage = gpadc4_measure_voltage(info);
			pr_info("%s: adc = %d mv\n", __func__, voltage);

			for (i = 0; i < info->pdata->num_zones; i++) {
				if (voltage <= info->pdata->zones[i].adc_high) {
					if (++count[i] > info->pdata->zones[i].check_count) {
						// Set Earjack type (3 pole or 4 pole)
						pm800_jack_set_type(info, info->pdata->zones[i].jack_type );
#if defined (SEC_USE_ANLOGDOCK_DEVICE)
						/* Use Docking device for external audio */
						pm800_audio_dock_ctrl(info,0);	//output only headset
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
		pm800_jack_set_type(info, SEC_JACK_NO_DEVICE );
		pr_info("%s : detection is failed\n", __func__);
	}
	else {
#if defined (COM_DET_CONTROL)
		pm800_gdet_int(info, 0);
#endif
		pm800_button_handle(info, info->pdata->press_release_th); //KSND
		pm800_jack_set_type(info, SEC_JACK_NO_DEVICE );
#if defined(SEC_USE_ANLOGDOCK_DEVICE)
		/* Use Docking device for external audio */
		pm800_audio_dock_ctrl(info,1); //Output only Dock device
#endif
		pr_info("%s : KSND HEADSET released !!\n", __func__);
	}
}

static void pm800_jack_release_work(struct work_struct *work) //KSND Modify
{
	struct pm800_headset_info *info = container_of(work, struct pm800_headset_info, work_release);
	struct sec_jack_platform_data *pdata = info->pdata;
	unsigned int voltage;

	if (info->hook_vol_status >= HOOK_PRESSED) { //KSND
		voltage = gpadc4_measure_voltage(info);
		if(voltage >= pdata->press_release_th) {
			pm800_button_handle(info, voltage);
			pr_info("%s : force button released!!\n", __func__);
		}
	}
}

static void pm800_jack_buttons_work(struct work_struct *work)
{
	struct pm800_headset_info *info = container_of(work, struct pm800_headset_info, work_hook);
	int ret;
	unsigned int value, voltage;

	if (info == NULL || info->idev == NULL) {
		pr_debug("Invalid hook info!\n");
		return;
	}

	voltage = gpadc4_measure_voltage(info);
	msleep(80);

	ret = regmap_read(info->map, PM800_GPIO_2_3_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev,	"Failed to read PM800_GPIO_2_3_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM800_GPIO3_VAL;

	if (info->headset_flag == PM800_HS_DET_INVERT)
		value = !value;

	if (!value ) {
		/* in case of headset unpresent, disable hook interrupt */
		trace_printk("hook: false exit\n");
		goto FAKE_HOOK;
	}

	if(hs_detect.hsdetect_status != PM8XXX_HEADSET_ADD){
		pr_info("headset removed - discard jack buttons\n");
		return;
	}

	pm800_button_handle(info, voltage);
	regmap_update_bits(info->map, PM800_INT_ENA_3, PM800_GPADC4_INT_ENA3, PM800_GPADC4_INT_ENA3);

	return;

FAKE_HOOK:
	dev_err(info->dev, "fake hook interupt\n");

}

static irqreturn_t pm800_jack_detect_irq_thread(int irq, void *data)
{
	struct pm800_headset_info *info = data;

	pr_info("%s:\n", __func__);

	if (info->psw_data_headset != NULL) {
		pm800_hook_int(info, 0);
		queue_work(system_wq, &info->work_headset);
	}
	return IRQ_HANDLED;
}

static irqreturn_t pm800_jack_buttons_irq_thread(int irq, void *data)
{
	struct pm800_headset_info *info = data;

	pr_info("%s:\n", __func__);

	mod_timer(&info->button_timer, jiffies + msecs_to_jiffies(130));

	if (info->idev != NULL) {
		regmap_update_bits(info->map, PM800_INT_ENA_3, PM800_GPADC4_INT_ENA3, 0);
		queue_work(system_wq, &info->work_hook);
	}
	return IRQ_HANDLED;
}

#if defined (COM_DET_CONTROL)
static irqreturn_t pm800_jack_gdet_irq_thread(int irq, void *data)
{
	struct pm800_headset_info *info = data;

	if (info->psw_data_headset != NULL) {
		pr_info("%s: \n", __func__ );

		if( hs_detect.hsdetect_status == 0){
			return IRQ_HANDLED;
		}
		
		pm800_hook_int(info,0);
		
		if(hs_detect.hsdetect_status){
			pr_info("[headset] #1 jack SND_JACK released by GDET!!\n");
			/* disable MIC detection and measurement */
			regmap_update_bits(info->map, PM800_MIC_CNTRL, PM800_MICDET_EN, 0);
		}

		if ((hs_detect.hsdetect_status == PM8XXX_HEADSET_ADD) && info->mic_set_power ){
			/* We should change the hs_status ASAP */
			info->mic_set_power(0);
			hs_detect.hsdetect_status = 0;
		}
	}
	return IRQ_HANDLED;
}

static void pm800_gdet_timer_handler(unsigned long data)
{
	struct pm800_headset_info *info = (struct pm800_headset_info *)data;

	/* com interrupt */
	if (info->psw_data_headset == NULL ){
		return;
	}
	queue_work(system_wq, &info->work_headset);
}
#endif

static void pm800_jack_timer_handler(unsigned long data)
{
	struct pm800_headset_info *info = (struct pm800_headset_info *)data;
	if (info->psw_data_headset != NULL) {
		queue_work(system_wq, &info->work_headset);
	}	
}

static void pm800_button_timer_handler(unsigned long data) //KSND
{
	struct pm800_headset_info *info = (struct pm800_headset_info *)data;

  	queue_work(system_wq, &info->work_release);
}


static ssize_t switch_headset_print_state(struct switch_dev *sdev, char *buf)
{
	struct headset_switch_data *switch_data =
	    container_of(sdev, struct headset_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;
	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static long pm800_hsdetect_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	struct PM8XXX_HS_IOCTL hs_ioctl;

	if (copy_from_user(&hs_ioctl,(void *)arg, sizeof(struct PM8XXX_HS_IOCTL)))
		return -EFAULT;

	switch (cmd) {
	case PM8XXX_HSDETECT_STATUS:
		hs_ioctl.hsdetect_status = hs_detect.hsdetect_status;
		hs_ioctl.hookswitch_status = hs_detect.hookswitch_status;

#if defined(ENABLE_HS_DETECT_POLES)
		hs_ioctl.hsmic_status = hs_detect.hsmic_status;
#endif

		pr_info("hsdetect_ioctl PM8XXX_HSDETECT_STATUS\n");
		break;
	case PM8XXX_HOOKSWITCH_STATUS:
		hs_ioctl.hookswitch_status = hs_detect.hookswitch_status;
		hs_ioctl.hsdetect_status = hs_detect.hsdetect_status;

#if defined(ENABLE_HS_DETECT_POLES)
		hs_ioctl.hsmic_status = hs_detect.hsmic_status;
#endif /* */
		pr_info("hsdetect_ioctl PM8XXX_HOOKSWITCH_STATUS\n");
		break;
	default:
		return -ENOTTY;
	}
	return copy_to_user((void *)arg, &hs_ioctl,
			    sizeof(struct PM8XXX_HS_IOCTL));
}

static ssize_t pm800_headset_proc_read(char *buf, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(buf, "headset detect voice call case is  %s\n", voice_call_enable?"enabled":"disabled");

	return len;
}

static ssize_t pm800_headset_proc_write(struct file *filp, const char *buff,
		size_t len, void *data)
{
	char is_voice_call[2];

	if (len > 2)
		return -EFAULT;

	if (copy_from_user(is_voice_call, buff, len))
		return -EFAULT;

	if('0' == is_voice_call[0]) {
		voice_call_enable = 0;
	} else {
		voice_call_enable = 1;
	}

	return len;
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

	if ( value == SEC_HEADSET_4POLE || value == SEC_HEADSET_3POLE ) {
		/* set mic bias to enable adc */
		if (local_info->mic_set_power) {
			local_info->mic_set_power(1);
		}

		/* enable MIC detection also enable measurement */
		regmap_update_bits(local_info->map, PM800_MIC_CNTRL, PM800_MICDET_EN, PM800_MICDET_EN);
		msleep(100);

		pm800_jack_set_type(local_info, value );
	} else {
		pm800_jack_set_type(local_info, SEC_JACK_NO_DEVICE );
	}

	local_info->psw_data_headset->state = value;

	return size;
}

static DEVICE_ATTR(key_state, 0664 , key_state_onoff_show, NULL);
static DEVICE_ATTR(state, 0664 , earjack_state_onoff_show, NULL);
static DEVICE_ATTR(select_jack, 0664, select_jack_show, 	select_jack_store);
#endif

static int pm800_headset_switch_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm800_headset_info *info;
	struct gpio_switch_platform_data *pdata_headset = pdev->dev.platform_data;
	struct headset_switch_data *switch_data_headset = NULL;
	struct pm80x_platform_data *pm80x_pdata;
	int irq_headset, irq_hook, ret = 0;
#ifdef SEC_SYSFS_FOR_FACTORY_TEST //KSND
	struct class  *audio;
	struct device *earjack;
#endif

	if (pdev->dev.parent->platform_data) {
		pm80x_pdata = pdev->dev.parent->platform_data;
	} else {
		pr_debug("Invalid pm800 platform data!\n");
		return -EINVAL;
	}
	if (!pm80x_pdata->headset) {
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
	    devm_kzalloc(&pdev->dev, sizeof(struct pm800_headset_info),
			 GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->pdata = pm80x_pdata->headset; //KSND

	info->chip = chip;
	info->mic_set_power = pm80x_pdata->headset->mic_set_power;
	info->headset_flag = pm80x_pdata->headset->headset_flag;
	info->map = chip->regmap;
	info->map_gpadc = chip->subchip->regmap_gpadc;
	info->dev = &pdev->dev;
	info->irq_headset = irq_headset + chip->irq_base;
	info->irq_hook = irq_hook + chip->irq_base;
#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	info->chgpump_ctrl = fsa9480_chgpump_ctrl;
	info->dock_audiopath_ctrl = fsa9480_dock_audiopath_ctrl;
	info->usb_switch_register_notify = usb_switch_register_notify;
	info->usb_switch_unregister_notify = usb_switch_unregister_notify;
#endif
	info->idev = input_allocate_device();
	if (!info->idev) {
		dev_err(&pdev->dev, "Failed to allocate input dev\n");
		ret = -ENOMEM;
		goto input_allocate_fail;
	}

	info->idev->name = "88pm800_hook_vol";
	info->idev->phys = "88pm800_hook_vol/input0";
	info->idev->id.bustype = BUS_I2C;
	info->idev->dev.parent = &pdev->dev;

	__set_bit(EV_KEY, info->idev->evbit);
	__set_bit(KEY_MEDIA, info->idev->keybit);
	__set_bit(KEY_VOLUMEUP, info->idev->keybit);
	__set_bit(KEY_VOLUMEDOWN, info->idev->keybit);
	info->hook_vol_status = HOOK_VOL_ALL_RELEASED;

	switch_data_headset = devm_kzalloc(&pdev->dev, sizeof(struct headset_switch_data), GFP_KERNEL);
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

	
#ifdef FET_EN_CONTROL //KSND
	if(system_rev < LT02_R0_3){
		info->gpio_fet_en = mfp_to_gpio(GPIO008_GPIO_8);
	
		if( gpio_request(info->gpio_fet_en,"FET EN CTRL") ){
			gpio_free(info->gpio_fet_en);
			pr_info("%s: FET_EN CTRL Request Fail!!\n", __func__);
		}
		gpio_direction_output(info->gpio_fet_en, 0 );
	}
#endif /* FET_EN_CONTROL */
	
#ifdef HP_MUTE_CONTROL
	if(system_rev < LT02_R0_2){
		info->gpio_hp_mute = mfp_to_gpio(GPIO031_GPIO_31);
	
		if( gpio_request(info->gpio_hp_mute,"HP_MUTE CTRL") ){
			gpio_free(info->gpio_hp_mute);
			pr_info("%s: HP_MUTE CTRL Request Fail!!\n", __func__);
		}
		gpio_direction_output(info->gpio_hp_mute, 0 );
	}
#endif

	mutex_init(&info->hs_mutex);

#if defined (COM_DET_CONTROL)
	info->gpio_com = mfp_to_gpio(GPIO012_GPIO_12);

	if( gpio_request(info->gpio_com,"COM_DET CTRL") ){
		gpio_free(info->gpio_com);
		pr_info("%s: COM_DET CTRL Request Fail!!\n", __func__);
	}
	gpio_direction_input(info->gpio_com);

	/* com detection irq initialization(only rising edge detect) - ffkaka */
	info->irq_com = gpio_to_irq(info->gpio_com);

	ret = request_threaded_irq( info->irq_com, NULL, pm800_jack_gdet_irq_thread,
		IRQF_ONESHOT|IRQF_TRIGGER_RISING, "headset com detect", info);

	if (ret < 0) {
		pr_info( "[headset] Failed to request IRQ com detect : #%d: %d\n", info->irq_com, ret);
		goto out_irq_com;
	}
	disable_irq(info->irq_com);
	com_irq = 0;
#endif

	ret = request_threaded_irq(info->irq_headset, NULL, pm800_jack_detect_irq_thread,
				 IRQF_ONESHOT, "headset", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_headset, ret);
		goto out_irq_headset;
	}

	ret = request_threaded_irq(info->irq_hook, NULL, pm800_jack_buttons_irq_thread,
				 IRQF_ONESHOT, "hook", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_hook, ret);
		goto out_irq_hook;
	}
	
	voice_call_enable = 0;
	if (info->proc_file == NULL) {
		info->proc_file =
			create_proc_entry(PM800_HEADSET_PROC_FILE, 0644, NULL);
		if (info->proc_file) {
			info->proc_file->read_proc = pm800_headset_proc_read;
			info->proc_file->write_proc = (write_proc_t  *)pm800_headset_proc_write;
			info->proc_file->data = info;
		} else
			pr_info("pm800 headset proc file create failed!\n");
	}

#ifdef SEC_SYSFS_FOR_FACTORY_TEST //KSND
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
#endif

	/* Initialize timer for detecting loop */
	setup_timer(&info->headset_timer,pm800_jack_timer_handler,(unsigned long)info);

  	/* Init Timer for Button Release */ //KSND
	setup_timer(&info->button_timer,pm800_button_timer_handler,(unsigned long)info); 

#if defined (COM_DET_CONTROL)
	/* Init Timer for Gdet interrupt timer */ //KSND
	setup_timer(&info->gdet_timer, pm800_gdet_timer_handler,(unsigned long)info); 
#endif

	/*
	 * disable hook irq to ensure this irq will be enabled
	 * after plugging in headset
	 */
	info->hook_count = 1;
	pm800_hook_int(info, 0);
	ret = input_register_device(info->idev);
	if (ret < 0)
		goto err_input_dev_register;

	platform_set_drvdata(pdev, info);
	INIT_WORK(&info->work_headset, pm800_jack_detect_work);
	INIT_WORK(&info->work_hook, pm800_jack_buttons_work);
	INIT_WORK(&info->work_release, pm800_jack_release_work);
	hsdetect_dev = &pdev->dev;
	hs_detect.hsdetect_status = 0;
	hs_detect.hookswitch_status = 0;
	/* default 4_POLES */
	hs_detect.hsmic_status = PM8XXX_HS_MIC_ADD;

	/* Pointer for handling info in this driver KSND */
	local_info = info;

	/* Hook:32 ms debounce time */
	regmap_update_bits(info->map, PM800_MIC_CNTRL, MIC_DET_DBS,
			   MIC_DET_DBS_32MS);
	/* Hook:256ms period duty cycle */
	regmap_update_bits(info->map, PM800_MIC_CNTRL, MIC_DET_PRD,
			   MIC_DET_PRD_CTN_256MS);
	/* set GPADC_DIR to 1, set to 0 cause pop noise in recording */
	regmap_update_bits(info->map_gpadc, PM800_GPADC_MISC_CONFIG1,
			   PM800_GPADC4_DIR, PM800_GPADC4_DIR);
	/* enable headset detection on GPIO3 */
	regmap_update_bits(info->map, PM800_HEADSET_CNTRL, PM800_HEADSET_DET_EN,
			   PM800_HEADSET_DET_EN);
	pm800_jack_detect_work(&info->work_headset);

	dev_set_drvdata(earjack, info);  //KSND  

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	/* Use Docking device for external audio */
	if( info->usb_switch_register_notify != NULL ){
		info->usb_switch_register_notify(&dock_nb);
	}
#endif

	return 0;

out_irq_hook:	
	free_irq(info->irq_hook, info);
out_irq_headset:
	free_irq(info->irq_headset, info);
#if defined (COM_DET_CONTROL)
out_irq_com:
	free_irq(info->irq_com,info);
#endif
err_input_dev_register:
	input_free_device(info->idev);
err_switch_dev_register:
	switch_dev_unregister(&switch_data_headset->sdev);
headset_allocate_fail:
	devm_kfree(&pdev->dev, switch_data_headset);
input_allocate_fail:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int __devexit pm800_headset_switch_remove(struct platform_device *pdev)
{
	struct pm800_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data_headset =
	    info->psw_data_headset;
	/* disable headset detection on GPIO3 */
	regmap_update_bits(info->map, PM800_HEADSET_CNTRL, PM800_HEADSET_DET_EN, 0);

	/* disable GPIO3 interrupt */
	regmap_update_bits(info->map, PM800_INT_ENA_4, PM800_GPIO3_INT_ENA4, 0);

	cancel_work_sync(&info->work_headset);
	cancel_work_sync(&info->work_hook);

	free_irq(info->irq_hook, info);
	free_irq(info->irq_headset, info);

#if defined (COM_DET_CONTROL)
	free_irq(info->irq_com, info);
#endif /* FET_EN_CONTROL */
	
#ifdef FET_EN_CONTROL
	/* Disable GPIO control KSND */
	if(system_rev < LT02_R0_3)
		gpio_free(info->gpio_fet_en);
#endif /* FET_EN_CONTROL */

#if defined (COM_DET_CONTROL)
	/* Disable GPIO control KSND */
	gpio_free(info->gpio_com);
#endif /* FET_EN_CONTROL */

#ifdef HP_MUTE_CONTROL
	/* Disable GPIO control KSND */
	if(system_rev < LT02_R0_2)
		gpio_free(info->gpio_hp_mute);
#endif

#if defined(SEC_USE_ANLOGDOCK_DEVICE)
	/* Use Docking device for external audio */
	if( info->usb_switch_register_notify != NULL ){
		info->usb_switch_unregister_notify(&dock_nb);
	}
#endif

	switch_dev_unregister(&switch_data_headset->sdev);
	input_unregister_device(info->idev);

	devm_kfree(&pdev->dev, switch_data_headset);
	devm_kfree(&pdev->dev, info);

	return 0;
}

static int pm800_headset_switch_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct pm800_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data;

	switch_data = info->psw_data_headset;

	/* enable low power mode headset detection */
	regmap_update_bits(info->map, PM800_HEADSET_CNTRL, PM800_HSDET_SLP, PM800_HSDET_SLP);

	/* LDO2(mic) power off when headset inserted except voice call case*/
	  if (switch_data->state == 1 && !(voice_call_enable))
		info->mic_set_power(0);

	return 0;
}

static int pm800_headset_switch_resume(struct platform_device *pdev)
{
	struct pm800_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data;

	switch_data = info->psw_data_headset;

	/* disable low power mode headset detection */
	regmap_update_bits(info->map, PM800_HEADSET_CNTRL, PM800_HSDET_SLP, 0);

	/* Enable LDO2 again if we detect headset(not headphone).
	 * since we use LDO2 for mic detect in active mode.
	 */
	if (switch_data->state == 1 &&  !(voice_call_enable))
		info->mic_set_power(1);

	return 0;
}

static struct platform_driver pm800_headset_switch_driver = {
	.probe = pm800_headset_switch_probe,
	.remove = __devexit_p(pm800_headset_switch_remove),
	.suspend = pm800_headset_switch_suspend,
	.resume = pm800_headset_switch_resume,
	.driver = {
		   .name = "88pm800-headset",
		   .owner = THIS_MODULE,
		   },
};

static const struct file_operations pm800_hsdetect_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = pm800_hsdetect_ioctl,
};

static struct miscdevice pm800_hsdetect_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "88pm800-headset",
	.fops = &pm800_hsdetect_fops,
};

static int __init headset_switch_init(void)
{
	int ret = -EINVAL;
	ret = misc_register(&pm800_hsdetect_miscdev);
	if (ret < 0)
		return ret;
	ret = platform_driver_register(&pm800_headset_switch_driver);
	return ret;
}

static void __exit headset_switch_exit(void)
{
	platform_driver_unregister(&pm800_headset_switch_driver);
	misc_deregister(&pm800_hsdetect_miscdev);
}

module_exit(headset_switch_exit);
module_init(headset_switch_init);

MODULE_DESCRIPTION("Samsung Custom 88PM800 Headset driver");
MODULE_AUTHOR("Wenzeng Chen <wzch@marvell.com>");
MODULE_LICENSE("GPL");
