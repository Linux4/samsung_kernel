/*
 * D2199 ALSA SoC Audio Accessory Detection driver
 *
 * Copyright (c) 2013 Dialog Semiconductor
 *
 * Written by Adam Thomson <Adam.Thomson.Opensource@diasemi.com>
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
 
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/gpio.h>
#if defined(CONFIG_CPU_PXA988)||defined(CONFIG_CPU_PXA986)
#include <mach/mfp-pxa986-cs02.h>
#else
#include <mach/mfp-pxa1088-delos.h>
#endif

//#define D2199_AAD_DEBUG
//#define D2199_READ_PMIC_BUTTON_REG

/* PMIC related includes */
#include <linux/d2199/core.h>
#include <linux/d2199/d2199_reg.h>
 
#include <linux/d2199/d2199_codec.h>
#include <linux/d2199/d2199_aad.h>

#define D2199_AAD_MICBIAS_SETUP_TIME 50
#ifdef D2199_JACK_DETECT
 #ifdef D2199_AAD_MICBIAS_SETUP_TIME
 #define D2199_AAD_JACK_DEBOUNCE_MS 5 //(400 - D2199_AAD_MICBIAS_SETUP_TIME)
 #else
 #define D2199_AAD_JACK_DEBOUNCE_MS 400
 #endif
#else
 #define D2199_AAD_JACK_DEBOUNCE_MS 400
#endif

#define D2199_AAD_JACKOUT_DEBOUNCE_MS 100
#define D2199_AAD_BUTTON_DEBOUNCE_MS 5 // 50

#define D2199_AAD_3POLE_4POLE_JACK_ADC 89
#define D2199_AAD_MAX_4POLE_JACK_ADC 255

#define D2199_AAD_MIC1_BIAS_ENABLE (D2199_MICBIAS_EN | D2199_MICBIAS_LEVEL_2_6V)
#define D2199_AAD_MIC1_BIAS_DISABLE D2199_MICBIAS_LEVEL_2_6V
#if defined CONFIG_D2199_AAD_WATERDROP
#define D2199_AAD_WATERDROP
#endif

struct class *sec_audio_earjack_class=NULL;
struct device *sec_earjack_dev=NULL;

struct d2199_aad_priv *d2199_aad_ex;
EXPORT_SYMBOL(d2199_aad_ex);

extern int d2199_codec_power(struct snd_soc_codec *codec, int on,int clear);
static int d2199_aad_read(struct i2c_client *client, u8 reg);
struct i2c_client *add_client = NULL;


/*
 * Samsung defined resistances for 4-pole key presses:
 * 
 * 0 - 108 Ohms:	Send Button
 * 139 - 270 Ohms:	Vol+ Button
 * 330 - 680 Ohms:	Vol- Button
 */
 
/* Button resistance lookup table */
/* DLG - ADC values need to be correctly set as they're currently not known */
static struct button_resistance button_res_tbl[MAX_BUTTONS] = {

	[SEND_BUTTON] = {
		.min_val = 0,
		.max_val = 12,
	},
	[VOL_UP_BUTTON] = {
		.min_val = 14,
		.max_val = 28,
	},
	[VOL_DN_BUTTON] = {
		.min_val = 33,
		.max_val = 61,
	},
};
static u8 d2199_3pole_det_adc = D2199_AAD_3POLE_4POLE_JACK_ADC;
static u8 d2199_4pole_det_adc = D2199_AAD_MAX_4POLE_JACK_ADC;

/* Following register access methods based on soc-cache code */
static int d2199_aad_read(struct i2c_client *client, u8 reg)
{
//	struct d2199_aad_priv *d2199_aad = i2c_get_clientdata(client);
	
	struct i2c_msg xfer[2];
	u8 data;
	int ret;
	
//	mutex_lock(&d2199_aad->d2199_codec->d2199_pmic->d2199_io_mutex);
	
	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = &reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 1;
	xfer[1].buf = &data;

	ret = i2c_transfer(client->adapter, xfer, 2);

//	mutex_unlock(&d2199_aad->d2199_codec->d2199_pmic->d2199_io_mutex);
	
	if (ret == 2)
		return data;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

int d2199_aad_write(struct i2c_client *client, u8 reg, u8 value)
{
//	struct d2199_aad_priv *d2199_aad = i2c_get_clientdata(client);
	u8 data[2];
	int ret;

//	mutex_lock(&d2199_aad->d2199_codec->d2199_pmic->d2199_io_mutex);
	
    reg &= 0xff;
    data[0] = reg;
    data[1] = value & 0xff;

	ret = i2c_master_send(client, data, 2);

//	mutex_unlock(&d2199_aad->d2199_codec->d2199_pmic->d2199_io_mutex);
	
	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}
EXPORT_SYMBOL(d2199_aad_write);


int d2199_aad_update_bits(struct i2c_client *client, u8 reg, u8 mask,
				 u8 value)
{
	int change;
	unsigned int old, new;
	int ret;

	ret = d2199_aad_read(client, reg);
	if (ret < 0)
		return ret;

	old = ret;
	new = (old & ~mask) | value;
	change = old != new;
	if (change) {
		ret = d2199_aad_write(client, reg, new);
		if (ret < 0)
			return ret;
	}

	return change;
}
EXPORT_SYMBOL(d2199_aad_update_bits);

void d2199_aad_button_fast_rate(struct d2199_aad_priv *d2199_aad, int enable)
{
	struct i2c_client *client = d2199_aad->i2c_client;

	if(enable == 1) {
		d2199_aad_write(client,D2199_ACCDET_CFG2,0x77);
	}
	else {
		d2199_aad_write(client,D2199_ACCDET_CFG2,0x88);
	}
}
EXPORT_SYMBOL(d2199_aad_button_fast_rate);

#ifdef D2199_JACK_DETECT
static irqreturn_t d2199_g_det_handler(int irq, void *data)
{
	struct d2199_aad_priv *d2199_aad = data;

	cancel_delayed_work(&d2199_aad->g_monitor_work);
	schedule_delayed_work(&d2199_aad->g_monitor_work, msecs_to_jiffies(20));
	
	return IRQ_HANDLED;
}
#endif
/* IRQ handler for HW jack detection */
static irqreturn_t d2199_jack_handler(int irq, void *data)
{
	struct d2199_aad_priv *d2199_aad = data;

	wake_lock_timeout(&d2199_aad->wakeup, HZ * 10);
#ifdef D2199_AAD_DEBUG	
	dlg_info("[%s] start!\n",__func__);
#endif
	cancel_delayed_work(&d2199_aad->jack_monitor_work);
 	schedule_delayed_work(&d2199_aad->jack_monitor_work, msecs_to_jiffies(1)); 	

	return IRQ_HANDLED;
}

static irqreturn_t d2199_button_handler(int irq, void *data)
{
	struct d2199_aad_priv *d2199_aad = data;

#ifdef D2199_AAD_DEBUG	
	dlg_info("%s \n",__func__);
#endif

	if(d2199_aad->switch_data.state != D2199_HEADSET) {
		return IRQ_HANDLED;
	}

	wake_lock_timeout(&d2199_aad->wakeup, HZ * 10);

	cancel_delayed_work(&d2199_aad->button_monitor_work);
	schedule_delayed_work(&d2199_aad->button_monitor_work, msecs_to_jiffies(5));
	
	return IRQ_HANDLED;
}

static int d2199_switch_dev_register(struct d2199_aad_priv *d2199_aad)
{
	int ret = 0;

	d2199_aad->switch_data.sdev.name = "h2w";
	d2199_aad->switch_data.sdev.state = 0;
	d2199_aad->switch_data.state = 0;
	d2199_aad->button.status=D2199_BUTTON_RELEASE;
	ret = switch_dev_register(&d2199_aad->switch_data.sdev);
	
	return ret;
}

static int d2199_hooksw_dev_register(struct i2c_client *client,
				struct d2199_aad_priv *d2199_aad)
{
	int ret = 0;

	d2199_aad->input_dev = input_allocate_device();
	d2199_aad->input_dev->name = "d2199-aad";
	d2199_aad->input_dev->id.bustype = BUS_I2C;
	d2199_aad->input_dev->dev.parent = &client->dev;

	__set_bit(EV_KEY, d2199_aad->input_dev->evbit);
	__set_bit(KEY_VOLUMEUP, d2199_aad->input_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, d2199_aad->input_dev->keybit);
	__set_bit(KEY_MEDIA, d2199_aad->input_dev->keybit);
	ret = input_register_device(d2199_aad->input_dev);

	if (0 != ret) {
		input_free_device(d2199_aad->input_dev);
	}

	return ret;
}

static void d2199_aad_g_monitor_timer_work(struct work_struct *work)
{
	struct d2199_aad_priv *d2199_aad = container_of(work, 
									struct d2199_aad_priv, 
									g_monitor_work.work);

	d2199_aad->g_status = gpio_get_value(d2199_aad->gpio);

	dlg_info("[%s] g_status =%d \n",__func__,d2199_aad->g_status);

	if(d2199_aad->g_status == 1) {
		d2199_aad->g_count=0;
	}
	
	if(d2199_aad->g_status == 0  && 
		d2199_aad->switch_data.state == D2199_NO_JACK){ //incomplet insertion
		d2199_aad->g_count++;
		if(d2199_aad->g_count == 5) {// there is no jack detection during 1 sec
			schedule_delayed_work(&d2199_aad->jack_monitor_work, 
				msecs_to_jiffies(5));
		}
		else {	
			schedule_delayed_work(&d2199_aad->g_monitor_work, 
				msecs_to_jiffies(200));
		}
	}
}

static void d2199_aad_jack_monitor_timer_work(struct work_struct *work)
{
	struct d2199_aad_priv *d2199_aad = container_of(work, 
									struct d2199_aad_priv, 
									jack_monitor_work.work);
	struct i2c_client *client = d2199_aad->i2c_client;
	u8 jack_mode,btn_status,btn_status1,btn_status2;
	int state = d2199_aad->switch_data.state;
#ifdef D2199_AAD_WATERDROP	
	int state_gpio;
#endif
#ifdef D2199_AAD_DEBUG		
	dlg_info("%s \n",__func__);
#endif	
	if(d2199_aad->d2199_codec == NULL || d2199_aad->d2199_codec->codec_init ==0)
	{
		schedule_delayed_work(&d2199_aad->jack_monitor_work, 
			msecs_to_jiffies(300));
		return;
	}

	mutex_lock(&d2199_aad->d2199_codec->power_mutex);	

	d2199_codec_power(d2199_aad->d2199_codec->codec,1,0);

	d2199_aad->button.b_status=snd_soc_read(d2199_aad->d2199_codec->codec,
		D2199_MICBIAS1_CTRL);
	
	if(d2199_aad->switch_data.state != D2199_NO_JACK) {
		
		jack_mode = d2199_aad_read(client, D2199_ACCDET_CFG3);
		
		if ((jack_mode & D2199_ACCDET_JACK_MODE_JACK) == 0) {

			state=D2199_NO_JACK;	
			d2199_aad->l_det_status = false;
			d2199_aad_write(client,D2199_ACCDET_CONFIG,0x88);
			if(d2199_aad->button.status == D2199_BUTTON_PRESS)
			{
				input_event(d2199_aad->input_dev, EV_KEY,
						d2199_aad->button.key, 0);
				input_sync(d2199_aad->input_dev);
			}
			d2199_aad->button.status = D2199_BUTTON_RELEASE;
			if(d2199_aad->switch_data.state != state) {
				d2199_aad->switch_data.state = state;	
				switch_set_state(&d2199_aad->switch_data.sdev, state);
			}
			dlg_info(" %s, Headset Pull Out1\n",__func__);
			goto out;
		}
	}
	msleep(55);//yanli 2013.10.18 add delay as HW request.
	snd_soc_write(d2199_aad->d2199_codec->codec,
			D2199_MICBIAS1_CTRL, D2199_AAD_MIC1_BIAS_ENABLE);	
//2013.11.11 yanli Dialog patch for Ear detection Delay
#ifdef CONFIG_MACH_BAFFINQ
       msleep(400);
#else
	msleep(1);
#endif 
//end

	jack_mode = d2199_aad_read(client, D2199_ACCDET_CFG3);

	if (jack_mode & D2199_ACCDET_JACK_MODE_JACK) {
#ifdef D2199_AAD_WATERDROP
		state_gpio = gpio_get_value(d2199_aad->gpio);
		if (state_gpio == 1) 
		{	
			state=D2199_NO_JACK;

			if (d2199_aad->switch_data.state != state ) 
			{	
				d2199_aad_write(client,D2199_ACCDET_CONFIG,0x88);
				if(d2199_aad->button.status == D2199_BUTTON_PRESS)
				{
					input_event(d2199_aad->input_dev, EV_KEY,
							d2199_aad->button.key, 0);
					input_sync(d2199_aad->input_dev);
				}
				d2199_aad->button.status = D2199_BUTTON_RELEASE;
				dlg_info("%s Water drop state = %d\n",__func__, state);
				d2199_aad->switch_data.state = state;
				switch_set_state(&d2199_aad->switch_data.sdev, state);
			}
			schedule_delayed_work(&d2199_aad->jack_monitor_work, msecs_to_jiffies(500)); 				
			goto out;
		}
#endif	 
		if(d2199_aad->switch_data.state != D2199_NO_JACK){
#ifdef D2199_AAD_DEBUG				
			dlg_info(" %s, Same state Jack1 \n",__func__);
#endif			
			if(d2199_aad->switch_data.state == D2199_HEADSET 
				&& (jack_mode & 0x80) == 0) {
				d2199_aad_write(client,D2199_ACCDET_CONFIG,0x00);
				msleep(1);
				d2199_aad_write(client,D2199_ACCDET_CONFIG,0x88);
			}
			else if(d2199_aad->switch_data.state == D2199_HEADPHONE){
				d2199_aad_write(client,D2199_ACCDET_CONFIG,0x08);
			}
			goto out;
		}		
		
		msleep(60);
		
		btn_status1 = d2199_aad_read(client, D2199_ACCDET_STATUS);	
		dlg_info("%s 1Current Heaset set ADC= %d  \n",__func__,btn_status1);
		while(1){
			msleep(10);
			btn_status2 = d2199_aad_read(client, D2199_ACCDET_STATUS);
#ifdef D2199_AAD_DEBUG				
			dlg_info("%s 2Current Heaset set ADC= %d  \n",__func__,btn_status2);
#endif
			if(btn_status2 >= btn_status1 || btn_status1==0 || btn_status2==0){			
				btn_status=btn_status2;
				break;
			}
			btn_status1 = btn_status2;
		}
		
		dlg_info("%s Heaset set ADC= %d  \n",__func__,btn_status);

		if(btn_status <= d2199_3pole_det_adc)	{
			if(btn_status == 0 && d2199_aad->g_status == 1) { //slow insertion
				schedule_delayed_work(&d2199_aad->jack_monitor_work, 
				msecs_to_jiffies(500));
				goto out;
			}
			
			dlg_info("%s 3 Pole Headset set \n",__func__);
			state=D2199_HEADPHONE;
			d2199_aad_write(client,D2199_ACCDET_CONFIG,0x08);
	
		}
		else if(btn_status <= d2199_4pole_det_adc){
			dlg_info("%s 4 Pole Headset set \n",__func__);
			state=D2199_HEADSET;	
			d2199_aad_write(client,D2199_ACCDET_CONFIG,0x88);

		}
		else { //slow insertion
			schedule_delayed_work(&d2199_aad->jack_monitor_work, 
				msecs_to_jiffies(500));
			goto out;
		}
		d2199_aad->button.status = D2199_BUTTON_RELEASE;

	}
	else {
		if(d2199_aad->switch_data.state == D2199_NO_JACK) {
			dlg_info(" %s, Same state No jack2 \n",__func__);
			goto out;
		}
		state=D2199_NO_JACK;
		d2199_aad_write(client,D2199_ACCDET_CONFIG,0x88);
	
		d2199_aad->l_det_status = false;
		if(d2199_aad->button.status == D2199_BUTTON_PRESS)
		{
			input_event(d2199_aad->input_dev, EV_KEY,
					d2199_aad->button.key, 0);
			input_sync(d2199_aad->input_dev);
		}
		d2199_aad->button.status = D2199_BUTTON_RELEASE;
		dlg_info(" %s, Headset Pull Out\n",__func__);

	}

	if(d2199_aad->switch_data.state != state) {
		d2199_aad->switch_data.state = state;	
		switch_set_state(&d2199_aad->switch_data.sdev, state);
	}
	
out:
#ifdef D2199_AAD_DEBUG	
	dlg_info("%s Bias state 0x%x \n",__func__,d2199_aad->button.b_status);
#endif	
#ifdef CONFIG_SEC_FACTORY_MODE
	if(d2199_aad->switch_data.state != D2199_HEADSET){
#endif			
		snd_soc_write(d2199_aad->d2199_codec->codec,D2199_MICBIAS1_CTRL,
			d2199_aad->button.b_status);		
		d2199_codec_power(d2199_aad->d2199_codec->codec,0,0);
#ifdef CONFIG_SEC_FACTORY_MODE
	}
#endif		
	mutex_unlock(&d2199_aad->d2199_codec->power_mutex);
	
}

static void d2199_aad_button_monitor_timer_work(struct work_struct *work)
{
	struct d2199_aad_priv *d2199_aad = container_of(work, 
									struct d2199_aad_priv, 
									button_monitor_work.work);
	struct i2c_client *client = d2199_aad->i2c_client;	
	u8 btn_status,jack_mode;

#ifdef D2199_AAD_DEBUG		
		dlg_info("%s \n",__func__);
#endif	

	if(d2199_aad->d2199_codec == NULL || d2199_aad->d2199_codec->codec_init ==0)
	{
		schedule_delayed_work(&d2199_aad->button_monitor_work, 
			msecs_to_jiffies(300));
		return;
	}

	if(d2199_aad->switch_data.state != D2199_HEADSET)
	{		
		return;
	}

	mutex_lock(&d2199_aad->d2199_codec->power_mutex);

	d2199_codec_power(d2199_aad->d2199_codec->codec,1,0);

	if(d2199_aad->button.status == D2199_BUTTON_RELEASE) {
		d2199_aad->button.b_status=
			snd_soc_read(d2199_aad->d2199_codec->codec,D2199_MICBIAS1_CTRL);
	}

	snd_soc_write(d2199_aad->d2199_codec->codec,
			D2199_MICBIAS1_CTRL, D2199_AAD_MIC1_BIAS_ENABLE);	
	
	msleep(1);
	
	d2199_aad_write(client,D2199_ACCDET_CFG2,0x00);

	msleep(1);

	btn_status = d2199_aad_read(client, D2199_ACCDET_STATUS);

	jack_mode = d2199_aad_read(client, D2199_ACCDET_CFG3);

	dlg_info("%s jack_mode = 0x%x \n",__func__,jack_mode);

	if ((jack_mode & D2199_ACCDET_JACK_MODE_JACK) == 0 ){ 
		//no jack when pull out
		if(d2199_aad->button.status == D2199_BUTTON_PRESS)
		{
			input_event(d2199_aad->input_dev, EV_KEY,
					d2199_aad->button.key, 0);
			input_sync(d2199_aad->input_dev);
			d2199_aad->button.status = D2199_BUTTON_RELEASE;
			dlg_info("%s jack pull out event Rel key=%d!\n",__func__,d2199_aad->button.key);

			d2199_aad->switch_data.state = D2199_NO_JACK;	
			switch_set_state(&d2199_aad->switch_data.sdev, d2199_aad->switch_data.state);
		}
		
		d2199_aad->button.status = D2199_BUTTON_RELEASE;
		
		goto out;
	}

	dlg_info("%s Button ADC = %d ! \n",__func__,btn_status);

	if ((btn_status >= button_res_tbl[SEND_BUTTON].min_val) &&
		(btn_status <= button_res_tbl[SEND_BUTTON].max_val)) {

		if(d2199_aad->button.status == D2199_BUTTON_PRESS){
#ifdef D2199_AAD_DEBUG				
			dlg_info(" %s, same state button1 \n",__func__);
#endif			
			goto out;
		}

		if(d2199_aad->g_status == 0)  {
		
			d2199_aad->button.key=KEY_MEDIA;
			d2199_aad->button.status = D2199_BUTTON_PRESS;
			input_event(d2199_aad->input_dev, EV_KEY,
					d2199_aad->button.key, 1);
			input_sync(d2199_aad->input_dev);
			dlg_info("%s event Send Press ! \n",__func__);
		}

	}
	else if ((btn_status >= button_res_tbl[VOL_UP_BUTTON].min_val) &&
		 (btn_status <= button_res_tbl[VOL_UP_BUTTON].max_val)) {

		if(d2199_aad->button.status == D2199_BUTTON_PRESS){
#ifdef D2199_AAD_DEBUG				
			dlg_info(" %s, same state button \n",__func__);
#endif			
			goto out;
		}

		if(d2199_aad->g_status == 0)  {
			d2199_aad->button.key=KEY_VOLUMEUP;
			d2199_aad->button.status = D2199_BUTTON_PRESS;
			input_event(d2199_aad->input_dev, EV_KEY,
					d2199_aad->button.key, 1);
			input_sync(d2199_aad->input_dev);
			dlg_info("%s event VOL UP Press ! \n",__func__);
		}
	
	}
	else if ((btn_status >= button_res_tbl[VOL_DN_BUTTON].min_val) &&
		 (btn_status <= button_res_tbl[VOL_DN_BUTTON].max_val)) {

		if(d2199_aad->button.status == D2199_BUTTON_PRESS){
#ifdef D2199_AAD_DEBUG				
			dlg_info(" %s, same state button \n",__func__);
#endif
			goto out;
		}

		if(d2199_aad->g_status == 0)  {
			d2199_aad->button.key=KEY_VOLUMEDOWN;
			d2199_aad->button.status = D2199_BUTTON_PRESS;
			input_event(d2199_aad->input_dev, EV_KEY,
					d2199_aad->button.key, 1);
			input_sync(d2199_aad->input_dev);
			dlg_info("%s event VOL DOWN Press ! \n",__func__);
		}
	
	}
	else {

		if(d2199_aad->button.status == D2199_BUTTON_PRESS) {
			input_event(d2199_aad->input_dev, EV_KEY,
					d2199_aad->button.key, 0);
			input_sync(d2199_aad->input_dev);
			d2199_aad->button.status = D2199_BUTTON_RELEASE;
			dlg_info("%s event Rel key=%d!\n",__func__,d2199_aad->button.key);
		}
		
	}

out:	
	
	if(d2199_aad->button.status == D2199_BUTTON_RELEASE) {
#ifdef CONFIG_SEC_FACTORY_MODE		
		if(d2199_aad->switch_data.state != D2199_HEADSET){
#endif			
			snd_soc_write(d2199_aad->d2199_codec->codec,D2199_MICBIAS1_CTRL,
			d2199_aad->button.b_status);		
			d2199_codec_power(d2199_aad->d2199_codec->codec,0,0);
#ifdef CONFIG_SEC_FACTORY_MODE				
		}
#endif		
#ifdef D2199_AAD_DEBUG			
		dlg_info("%s Bias state 0x%x \n",__func__,d2199_aad->button.b_status);
#endif
		if((d2199_aad->button.b_status & 0x80) == 0) {
			d2199_aad_button_fast_rate(d2199_aad,1);
		}
		else {
			d2199_aad_button_fast_rate(d2199_aad,0);
		}
	}	
	else { //start polling in case of press 		
		schedule_delayed_work(&d2199_aad->button_monitor_work, 
			msecs_to_jiffies(300));
	}
	
	mutex_unlock(&d2199_aad->d2199_codec->power_mutex);
}

/*************************************
  * add sysfs for factory test. ffkaka 130324
  * 
 *************************************/
/* sysfs for hook key status - ffkaka */
static ssize_t switch_sec_get_key_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", (d2199_aad_ex->button.status)? "1":"0");
}

/* sysfs for headset state - ffkaka */
static ssize_t switch_sec_get_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", d2199_aad_ex->switch_data.state);
}

/* sysfs for headset state - ffkaka */
static ssize_t switch_sec_get_adc(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 adc;

	mutex_lock(&d2199_aad_ex->d2199_codec->power_mutex);
	
	d2199_codec_power(d2199_aad_ex->d2199_codec->codec,1,0);

	d2199_aad_ex->button.b_status=
			snd_soc_read(d2199_aad_ex->d2199_codec->codec,D2199_MICBIAS1_CTRL);
	
	snd_soc_write(d2199_aad_ex->d2199_codec->codec,
			D2199_MICBIAS1_CTRL, D2199_AAD_MIC1_BIAS_ENABLE);	
	msleep(80);
	
	adc = d2199_aad_read(d2199_aad_ex->i2c_client, D2199_ACCDET_STATUS);	

	snd_soc_write(d2199_aad_ex->d2199_codec->codec,D2199_MICBIAS1_CTRL,
		d2199_aad_ex->button.b_status);
	
	mutex_unlock(&d2199_aad_ex->d2199_codec->power_mutex);
	
	return sprintf(buf, "ADC=%d\n", adc);
}

static DEVICE_ATTR(key_state, S_IRUGO, switch_sec_get_key_state, NULL);
static DEVICE_ATTR(state, S_IRUGO, switch_sec_get_state, NULL);
static DEVICE_ATTR(adc, S_IRUGO, switch_sec_get_adc, NULL);

static int __devinit d2199_aad_i2c_probe(struct i2c_client *client,
					 const struct i2c_device_id *id)
{
	struct d2199_aad_priv *d2199_aad;
#ifndef D2199_JACK_DETECT
	int irq;
#endif
	int ret;
	u8 regval;

	dlg_info("[%s] start!\n",__func__);

	d2199_aad = devm_kzalloc(&client->dev, sizeof(struct d2199_aad_priv),
				 GFP_KERNEL);
	if (!d2199_aad)
		return -ENOMEM;
	
	d2199_aad->i2c_client = client;
	d2199_aad->d2199_codec = client->dev.platform_data;
	add_client = client;

	device_init_wakeup(&client->dev, 1);	//TEST

	wake_lock_init(&d2199_aad->wakeup, WAKE_LOCK_SUSPEND, "jack_monitor");
	
	i2c_set_clientdata(client, d2199_aad);

	d2199_switch_dev_register(d2199_aad);
	
	d2199_hooksw_dev_register(client, d2199_aad);

	d2199_aad_ex = d2199_aad;

	INIT_DELAYED_WORK(&d2199_aad->jack_monitor_work, d2199_aad_jack_monitor_timer_work);
	INIT_DELAYED_WORK(&d2199_aad->button_monitor_work, d2199_aad_button_monitor_timer_work);
	INIT_DELAYED_WORK(&d2199_aad->g_monitor_work, d2199_aad_g_monitor_timer_work);

	d2199_aad->button_detect_rate = 80;

	/* Ensure jack & button detection is disabled, default to auto power */
	d2199_aad_write(client, D2199_ACCDET_CONFIG,0x00); 	

	d2199_aad->button.status = D2199_BUTTON_RELEASE;
#ifdef D2199_JACK_DETECT
	/* Register virtual IRQs with PMIC for jack & button detection */
	d2199_register_irq(d2199_aad->d2199_codec->d2199_pmic,
			   D2199_IRQ_EJACKDET, d2199_jack_handler, 0,
			   "Jack detect", d2199_aad);
#ifdef D2199_AAD_WATERDROP
	d2199_aad->gpio = mfp_to_gpio(GPIO012_GPIO_12);
	if(gpio_request(d2199_aad->gpio, NULL) < 0){
		dlg_info("%s() fail to request gpio for COM_DET\n",__FUNCTION__);
		goto gpio_err;
	}
	gpio_direction_input(d2199_aad->gpio);

	d2199_aad->g_det_irq = MMP_GPIO_TO_IRQ(d2199_aad->gpio);
	dlg_info("[%s] : qpio_to_irq= %d  \n",__func__, d2199_aad->g_det_irq);
	ret=request_threaded_irq(d2199_aad->g_det_irq, NULL, d2199_g_det_handler,
						IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING| IRQF_DISABLED, "GPIO detect", d2199_aad);
	dlg_info("[%s] : request_threaded_irq= %d  \n",__func__, ret);
	ret = enable_irq_wake(d2199_aad->g_det_irq);
	dlg_info("[%s] : enable_irq_wake= %d  \n",__func__, ret);
#endif
#else
	gpio_request(d2199_aad->gpio, NULL);
	gpio_direction_input(d2199_aad->gpio);
	gpio_pull_up_port(d2199_aad->gpio);
	gpio_set_debounce(d2199_aad->gpio, 1000);	/* 1msec */
	irq=gpio_to_irq(d2199_aad->gpio);
	ret=request_threaded_irq(irq, NULL, d2199_jack_handler,
						IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING| IRQF_DISABLED, "Jack detect", d2199_aad);

	enable_irq_wake(irq);
#endif

gpio_err:

	d2199_register_irq(d2199_aad->d2199_codec->d2199_pmic,
			   D2199_IRQ_EACCDET, d2199_button_handler, 0,
			   "Button detect", d2199_aad);

	d2199_reg_read(d2199_aad->d2199_codec->d2199_pmic, 0x96, &regval);
	d2199_aad->chip_rev = regval;
	d2199_aad->l_det_status = false;

	dlg_info("%s chip_rev [0x%x] \n",__func__, d2199_aad->chip_rev);
	
	schedule_delayed_work(&d2199_aad->jack_monitor_work, msecs_to_jiffies(300));

	/* sys fs for factory test - ffkaka */
	sec_audio_earjack_class = class_create(THIS_MODULE, "audio");
	if(sec_audio_earjack_class == NULL){
		pr_info("[headset] Failed to create audio class!!!\n");
		return 0;
	}
	
	sec_earjack_dev = device_create(sec_audio_earjack_class, NULL, 0, NULL, "earjack");
	if(IS_ERR(sec_earjack_dev))
		pr_err("[headset] Failed to create earjack device!!!\n");
	
	ret = device_create_file(sec_earjack_dev, &dev_attr_key_state);
	if (ret < 0)
		device_remove_file(sec_earjack_dev, &dev_attr_key_state);
	
	ret = device_create_file(sec_earjack_dev, &dev_attr_state);
	if (ret < 0)
		device_remove_file(sec_earjack_dev, &dev_attr_state);	

	ret = device_create_file(sec_earjack_dev, &dev_attr_adc);
	if (ret < 0)
		device_remove_file(sec_earjack_dev, &dev_attr_adc);	

	if(d2199_aad->d2199_codec->d2199_pmic->pdata->headset != NULL){
		button_res_tbl[SEND_BUTTON].min_val = d2199_aad->d2199_codec->d2199_pmic->pdata->headset->send_min;
		button_res_tbl[SEND_BUTTON].max_val = d2199_aad->d2199_codec->d2199_pmic->pdata->headset->send_max;
		button_res_tbl[VOL_UP_BUTTON].min_val = d2199_aad->d2199_codec->d2199_pmic->pdata->headset->vol_up_min;
		button_res_tbl[VOL_UP_BUTTON].max_val = d2199_aad->d2199_codec->d2199_pmic->pdata->headset->vol_up_max;
		button_res_tbl[VOL_DN_BUTTON].min_val = d2199_aad->d2199_codec->d2199_pmic->pdata->headset->vol_down_min;
		button_res_tbl[VOL_DN_BUTTON].max_val = d2199_aad->d2199_codec->d2199_pmic->pdata->headset->vol_down_max;
		d2199_3pole_det_adc = d2199_aad->d2199_codec->d2199_pmic->pdata->headset->jack_3pole_max;
		d2199_4pole_det_adc = d2199_aad->d2199_codec->d2199_pmic->pdata->headset->jack_4pole_max;
	}

	return 0;
}

static int __devexit d2199_aad_i2c_remove(struct i2c_client *client)
{
	struct d2199_aad_priv *d2199_aad = i2c_get_clientdata(client);
	struct d2199 *d2199_pmic = d2199_aad->d2199_codec->d2199_pmic;

	/* Disable jack & button detection, default to auto power */
	d2199_aad_write(client, D2199_ACCDET_CONFIG, 0);

	/* Free up virtual IRQs from PMIC */
	d2199_free_irq(d2199_pmic, D2199_IRQ_EJACKDET);
	d2199_free_irq(d2199_pmic, D2199_IRQ_EACCDET);

	return 0;
}

static const struct i2c_device_id d2199_aad_i2c_id[] = {
	{ "d2199-aad", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, d2199_aad_i2c_id);

/* I2C AAD control layer */
static struct i2c_driver d2199_aad_i2c_driver = {
	.driver = {
		.name = "d2199-aad",
		.owner = THIS_MODULE,
	},
	.probe		= d2199_aad_i2c_probe,
	.remove		= __devexit_p(d2199_aad_i2c_remove),
	.id_table	= d2199_aad_i2c_id,
};

static int __init d2199_aad_init(void)
{
	int ret;

	ret = i2c_add_driver(&d2199_aad_i2c_driver);
	if (ret)
		pr_err("D2199 AAD I2C registration failed %d\n", ret);

	return ret;
}
module_init(d2199_aad_init);

static void __exit d2199_aad_exit(void)
{
	i2c_del_driver(&d2199_aad_i2c_driver);
}
module_exit(d2199_aad_exit);

MODULE_DESCRIPTION("ASoC D2199 AAD Driver");
MODULE_AUTHOR("Adam Thomson <Adam.Thomson.Opensource@diasemi.com>");
MODULE_LICENSE("GPL");

