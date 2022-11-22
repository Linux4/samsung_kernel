/*
 * D2199 ALSA SoC codec driver
 *
 * Copyright (c) 2013 Dialog Semiconductor
 *
 * Written by Adam Thomson <Adam.Thomson.Opensource@diasemi.com>
 * Based on DA9055 ALSA SoC codec driver.
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/clk.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#include <linux/d2199/core.h>
#include <linux/d2199/d2199_reg.h>
#include <linux/d2199/d2199_codec.h>
#include <linux/gpio.h>
#include <mach/mfp-pxa1088-delos.h>

#ifdef CONFIG_SND_SOC_D2199_AAD
#include <linux/d2199/d2199_aad.h>
#endif /* CONFIG_SND_SOC_D2199_AAD */

//#define D2199_KEEP_POWER
//#define D2199_CODEC_DEBUG

extern int get_board_id(void);

void d2199_register_dump(struct snd_soc_codec *codec);

extern int ssp_master;

static u8 d2199_chip_rev;
#ifdef D2199_KEEP_POWER
static const u8 d2199_reg_defaults[] = {
	0x00, 0x00, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00,  /* R0  - R7  */
	0x00, 0x00, 0x00, 0x00, 0x80, 0x77, 0x77, 0x07,  /* R8  - RF  */
	0x80, 0x77, 0x77, 0x07, 0x00, 0x00, 0x00, 0x00,  /* R10 - R17 */
	0x00, 0x3f, 0x3f, 0x00, 0xff, 0x71, 0x00, 0x00,  /* R18 - R1F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* R20 - R27 */
	0x00, 0x00, 0x00, 0x00, 0x01, 0x1c, 0x00, 0x1c,  /* R28 - R2F */
	0x00, 0x1c, 0x00, 0x02, 0x1c, 0x00, 0x1c, 0x00,  /* R30 - R37 */
	0x1c, 0x00, 0x01, 0x1c, 0x00, 0x1c, 0x00, 0x1c,  /* R38 - R3F */
	0x00, 0x02, 0x1c, 0x00, 0x1c, 0x00, 0x1c, 0x00,  /* R40 - R47 */
	0x14, 0x1c, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x28,  /* R48 - R4F */
	0x1c, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00,  /* R50 - R57 */
	0x08, 0x00, 0x01, 0x08, 0x00, 0x01, 0x00, 0x00,  /* R58 - R5F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* R60 - R67 */
	0x46, 0x48, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00,  /* R68 - R6F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,  /* R70 - R77 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* R78 - R7F */
	0x00, 0x21, 0x99, 0x02, 0x00, 0x00, 0x00, 0x00,  /* R80 - R87 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* R88 - R8F */
	0x00, 0x01, 0x00, 0x00, 0x20, 0x00, 0x00, 0x20,  /* R90 - R97 */
	0xcc, 0x00, 0x00, 0x00, 0x61, 0x9d, 0x00, 0x00,  /* R98 - R9F */
	0x00, 0x00, 0x00, 0x00, 0x44, 0x35, 0x00, 0x44,  /* RA0 - RA7 */
	0x35, 0x00, 0x00, 0x00, 0x40, 0x01, 0x01, 0x40,  /* RA8 - RAF */
	0x01, 0x01, 0x40, 0x01, 0x01, 0x00, 0x00, 0x00,  /* RB0 - RB7 */
	0x00, 0x00, 0x00, 0x00, 0x40, 0x03, 0x00, 0x00,  /* RB8 - RBF */
	0x40, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* RC0 - RC7 */
	0x40, 0x6f, 0x00, 0x00, 0x40, 0x6f, 0x00, 0x00,  /* RC8 - RCF */
	0x40, 0x6f, 0x00, 0x40, 0x6f, 0x00, 0x00, 0x00,  /* RD0 - RD7 */
	0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x00, 0x00,  /* RD8 - RDF */
	0x40, 0x72, 0x00, 0x40, 0x72, 0x00, 0x00, 0x00,  /* RE0 - RE7 */
	0x40, 0x72, 0x00, 0x00, 0x40, 0x33, 0x00, 0x00,  /* RE8 - REF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* RF0 - RF7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   /* RF8 - RFA */							
};

static u8 d2199_reg_cache[255] ;
#endif


u8 d2199_aif_adjusted_val1 [17] = {
                0x00,0x11,0x02,0x13,0x04,0x15,0x06,0x17,0x08,0x19,
				0x0a,0x1b,0x0c,0x1d,0x0e,0x1f,0x10                
};

u8 d2199_aif_adjusted_val2 [9] = {
                0x10,0x01,0x12,0x03,0x14,0x05,0x16,0x07,0x18,             
};

static int d2199_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt);

#ifdef CONFIG_SND_SOC_D2199_AAD
extern int d2199_aad_write_ex(u8 reg, u8 value);

int d2199_aad_enable(struct snd_soc_codec *codec)
{
	struct d2199_codec_priv *d2199_codec =
		snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2199_codec->aad_i2c_client;
	struct d2199_aad_priv *d2199_aad = i2c_get_clientdata(client);
	int board_id = get_board_id();

#ifdef D2199_CODEC_DEBUG		
	dlg_info("%s() Start board id =%d \n",__FUNCTION__,board_id);
#endif

	d2199_aad_write(client,D2199_ACCDET_UNLOCK_AO,0x4a);
	
	//d2199_aad_write(client,D2199_ACCDET_TST2,0x10);
	d2199_aad_write(client,D2199_ACCDET_THRESH1,0x1a);//low power jack thres
	d2199_aad_write(client,D2199_ACCDET_THRESH2,0x62);//high power jack thres

	d2199_aad_write(client,D2199_ACCDET_THRESH3,0x0b);

	d2199_aad_write(client,D2199_ACCDET_THRESH4,0x44);//high power button thres1

	d2199_aad_write(client,D2199_ACCDET_CFG1,0x5f);
	d2199_aad_write(client,D2199_ACCDET_CFG2,0x77);

	d2199_aad_write(client,D2199_ACCDET_CFG3,0x03);

	//d2199_aad_write(client,D2199_ACCDET_CFG4,0x07);

	if(d2199_aad->switch_data.state == D2199_HEADPHONE) {
		d2199_aad_write(client,D2199_ACCDET_CONFIG,0x08);
	}
	else if(d2199_aad->switch_data.state == D2199_HEADSET){
		d2199_aad_write(client,D2199_ACCDET_CONFIG,0x88);
		schedule_delayed_work(&d2199_aad->button_monitor_work, 
			msecs_to_jiffies(5));
	}
	else {
		d2199_aad_write(client,D2199_ACCDET_CONFIG,0x88);
	}
	
	return 0;
}
#endif

static void d2199_pll_lock_monitor_timer_work(struct work_struct *work)
{
	struct d2199_codec_priv *d2199_codec = container_of(work, 
									struct d2199_codec_priv, pll_lock_work.work);

	int i;
	u8 val=0;
	u8 val_route_r,val_route_l;
	
#ifdef D2199_CODEC_DEBUG		
	dlg_info("%s codec power =%x \n",__func__,
		d2199_codec->power_mode);
#endif	
	mutex_lock(&d2199_codec->power_mutex);

	if(d2199_codec->power_mode == 0 || d2199_codec->voice_call ==0
		) {
		mutex_unlock(&d2199_codec->power_mutex);
		return;
	}
	
	val=snd_soc_read(d2199_codec->codec,D2199_PLL_STATUS);
	if((val&0x02) == 0x00) {
		#ifdef D2199_CODEC_DEBUG		
		dlg_info("%s PLL status= 0x%x \n",__func__,val);			
		#endif
		schedule_delayed_work(&d2199_codec->pll_lock_work,
				msecs_to_jiffies(20));
		mutex_unlock(&d2199_codec->power_mutex);
		return;
	}
	
	snd_soc_write(d2199_codec->codec, D2199_MIXOUT_L_CTRL, 0x80);
	snd_soc_write(d2199_codec->codec, D2199_MIXOUT_R_CTRL, 0x80);
	snd_soc_write(d2199_codec->codec, D2199_MIXOUT_SP_CTRL, 0x80);
	snd_soc_write(d2199_codec->codec, D2199_DAC_L_CTRL, 0xe0);
	snd_soc_write(d2199_codec->codec, D2199_DAC_R_CTRL, 0xe0);
	
	val_route_r=snd_soc_read(d2199_codec->codec,D2199_DROUTING_DAI_1_R);
	val_route_l=snd_soc_read(d2199_codec->codec,D2199_DROUTING_DAI_1_L);
	snd_soc_write(d2199_codec->codec, D2199_DROUTING_DAI_1_R, 0x00);
	snd_soc_write(d2199_codec->codec, D2199_DROUTING_DAI_1_L, 0x00);

	snd_soc_write(d2199_codec->codec, D2199_PC_COUNT, 0x02);
	
	for (i = 0; i < 17 ; i++) {
		snd_soc_write(d2199_codec->codec, D2199_DAI_1_OFFSET, 
			d2199_aif_adjusted_val1[i]);
		usleep_range(500,500);
	}

	snd_soc_write(d2199_codec->codec, D2199_PC_COUNT, 0x00);
	snd_soc_write(d2199_codec->codec, D2199_DAI_1_OFFSET, 0xff);

	usleep_range(500,500);

	snd_soc_write(d2199_codec->codec, D2199_PC_COUNT, 0x02);
	
	for (i = 0; i < 9 ; i++) {
		snd_soc_write(d2199_codec->codec, D2199_DAI_1_OFFSET, 
			d2199_aif_adjusted_val2[i]);
		usleep_range(500,500);
	}
	snd_soc_write(d2199_codec->codec, D2199_PC_COUNT, 0x00);
	snd_soc_write(d2199_codec->codec, D2199_DAI_1_OFFSET, 0xff);
		
	usleep_range(500,500);		
	
	snd_soc_write(d2199_codec->codec, D2199_PC_COUNT, 0x02);
	
	snd_soc_write(d2199_codec->codec, D2199_DAI_1_OFFSET, 0x01);	
	
	msleep(5);
	snd_soc_write(d2199_codec->codec, D2199_PLL_CTRL1, 0x80);
	msleep(5);
	if(d2199_codec->aif2_enable ==1) {
		snd_soc_write(d2199_codec->codec, D2199_PLL_CTRL1, 0x8C);
	}
	else {
		snd_soc_write(d2199_codec->codec, D2199_PLL_CTRL1, 0x88);
	}

	if(d2199_codec->narrow_band == 1) {
		msleep(140);
	}
	else {
		msleep(70);
	}

#ifdef CONFIG_SEC_FACTORY_MODE
	msleep(20);
#endif

	if(d2199_codec->adc_l_enable){
		snd_soc_write(d2199_codec->codec, D2199_ADC_L_CTRL, 0xa0);
	}
	if(d2199_codec->adc_r_enable){
		snd_soc_write(d2199_codec->codec, D2199_ADC_R_CTRL, 0xa0);
	}	

	snd_soc_write(d2199_codec->codec, D2199_DAC_L_CTRL, 0xa0);
	snd_soc_write(d2199_codec->codec, D2199_DAC_R_CTRL, 0xa0);
	snd_soc_write(d2199_codec->codec, D2199_DROUTING_DAI_1_R, val_route_r);
	snd_soc_write(d2199_codec->codec, D2199_DROUTING_DAI_1_L, val_route_l);
	if(d2199_codec->hp_enable) {
		snd_soc_write(d2199_codec->codec, D2199_HP_L_CTRL, 0xa8);
		snd_soc_write(d2199_codec->codec, D2199_HP_R_CTRL, 0xa8);
	}
	msleep(5);
	snd_soc_write(d2199_codec->codec, D2199_MIXOUT_L_CTRL, 0x88);
	snd_soc_write(d2199_codec->codec, D2199_MIXOUT_R_CTRL, 0x88);
	snd_soc_write(d2199_codec->codec, D2199_MIXOUT_SP_CTRL, 0x88);		
	
	d2199_codec->pll_locked = 1;
	
	mutex_unlock(&d2199_codec->power_mutex);

}

static int d2199_init_setup_register(struct snd_soc_codec *codec)
{
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
#ifdef CONFIG_SEC_FACTORY_MODE
	struct i2c_client *client = d2199_codec->aad_i2c_client;
	struct d2199_aad_priv *d2199_aad = i2c_get_clientdata(client);
#endif

	snd_soc_write(codec,D2199_DROUTING_DAC_L,0x00);
	snd_soc_write(codec,D2199_DROUTING_DAC_R,0x00);
	
	/* Default to using ALC auto offset calibration mode. */
	snd_soc_update_bits(codec, D2199_ALC_CTRL1, D2199_ALC_CALIB_MODE_MAN, 0);
	d2199_codec->alc_calib_auto = true;
			
	snd_soc_write(codec, D2199_LDO_CTRL, 0xB0);	

	snd_soc_write(codec,0x9a,0x01);
	snd_soc_write(codec,D2199_CP_VOL_THRESHOLD1,0x06);

#ifdef CONFIG_SEC_FACTORY_MODE
	if(d2199_aad->switch_data.state == D2199_HEADSET) {
		snd_soc_write(codec,D2199_MICBIAS1_CTRL,D2199_MICBIAS_EN | D2199_MICBIAS_LEVEL_2_6V);
	}
	else {
		snd_soc_write(codec,D2199_MICBIAS1_CTRL,D2199_MICBIAS_LEVEL_2_6V);	
	}
#else	
	snd_soc_update_bits(codec,D2199_MICBIAS1_CTRL,D2199_MICBIAS_LEVEL_2_6V
	,D2199_MICBIAS_LEVEL_2_6V);	
#endif

	return 0;	
}

#ifdef D2199_KEEP_POWER	
int d2199_codec_shutdown(struct snd_soc_codec *codec)
{	
	d2199_register_dump(codec);
	
	snd_soc_write(codec,0x07,0x00);
	snd_soc_write(codec,0x08,0x00);
	snd_soc_write(codec,0x09,0x01);

	msleep(100);
	
	d2199_register_dump(codec);
	
	dlg_info("%s() finished \n",__FUNCTION__);

	return 0;
}

int d2199_codec_idle(struct snd_soc_codec *codec)
{
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);

	snd_soc_write(codec,D2199_UNLOCK,0x8b);
	snd_soc_write(codec,D2199_REFERENCES,0x88);
	d2199_init_setup_register(d2199_codec->codec);

	dlg_info("%s() finished \n",__FUNCTION__);

	return 0;
}
#endif

int d2199_codec_power(struct snd_soc_codec *codec, int on, int clear)
{
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2199_codec->aad_i2c_client;
	struct d2199_aad_priv *d2199_aad = i2c_get_clientdata(client);
	struct regulator *regulator;
	
	if(on ==1 && d2199_codec->power_mode==0){

		dlg_info("%s() Start power = %d \n",__FUNCTION__,on);		
	
		regulator = regulator_get(NULL, "aud1");

		if (IS_ERR(regulator)) {
			dlg_info("%s() AUD1 regulator get failed d \n",__FUNCTION__);
			return -1;
		}
		regulator_enable(regulator);
		regulator_put(regulator);
		
		regulator = regulator_get(NULL, "aud2");

		if (IS_ERR(regulator)){
			dlg_info("%s() AUD2 regulator get failed d \n",__FUNCTION__);
			return -1;
		}
		
		regulator_enable(regulator);
		regulator_put(regulator);
		
		if(clear == 1){
			snd_soc_write(codec, D2199_CIF_CTRL,0x80);
		}

		//d2199_register_dump(codec);
		
		snd_soc_write(codec,D2199_UNLOCK,0x8b);		

		snd_soc_write(codec,D2199_REFERENCES,0x88);		

		msleep(2);
		
		d2199_init_setup_register(d2199_codec->codec);		

		if(clear == 1){	
			d2199_aad_enable(codec);		
			d2199_aad_button_fast_rate(d2199_aad,1);
		}

		d2199_codec->power_mode=1;
		
	}		
	else if(on ==0 && d2199_codec->power_mode==1 
		&& d2199_codec->power_disable_suspend == 1){

		dlg_info("%s() Start power = %d \n",__FUNCTION__,on);

		snd_soc_write(codec,D2199_DAI_1_CTRL,0x08);
		snd_soc_write(codec,D2199_DAI_2_CTRL,0x08);
	
		regulator = regulator_get(NULL, "aud2");
		if (IS_ERR(regulator))
			return -1;

		regulator_disable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "aud1");
		if (IS_ERR(regulator))
			return -1;

		regulator_disable(regulator);		
		regulator_put(regulator);
		d2199_codec->power_mode=0;
		d2199_codec->voice_call=0;
	}

	return 0;
}
EXPORT_SYMBOL(d2199_codec_power);

static int d2199codec_i2c_read_device(struct d2199_codec_priv *d2199_codec,char reg,
					int bytes, void *dest);

void d2199_register_dump(struct snd_soc_codec *codec)
{
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	int i;
	char val;

	for(i=0 ; i<250 ; i++)	{
		d2199codec_i2c_read_device(d2199_codec,i,1,&val);
		printk(KERN_ERR"[WS][%s] reg=0x%x val=0x%x \n", __func__,i,val);
	}
}

/*
 * Control list
 */
static unsigned int d2199_codec_read(struct snd_soc_codec *codec, unsigned int reg);

static int d2199_put_dai_2_ctrl_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

	if(!!(val & 0x80)){			
		d2199_codec->aif2_enable =1;			
	}
	else {			
		d2199_codec->aif2_enable =0;
	}	
	
	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static int d2199_put_dai_2_clk_mode_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

	if(!!(val & 0x80)){	
#ifdef D2199_CODEC_DEBUG
		dlg_info("%s() AIF2 Codec Master Mode \n",__FUNCTION__);
#endif			
		d2199_codec->aif2_master =1;			
	}
	else {			
#ifdef D2199_CODEC_DEBUG
		dlg_info("%s() AIF2 Codec Slave Mode \n",__FUNCTION__);
#endif			
		d2199_codec->aif2_master =0;
	}	
	
	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static int d2199_put_dai_1_clk_mode_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

	if(!!(val & 0x80)){			
		d2199_codec->aif1_master =1;			
	}
	else {			
		d2199_codec->aif1_master =0;
	}	
	
	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static int d2199_put_sp_cfg1_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];
	
	if(!!(val & 0x01)){				
		d2199_codec->bt_enable=1;			
	}
	else {			
		d2199_codec->bt_enable=0;
	}	
	
	return 0;
}

static int d2199_put_sp_cfg2_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];	

#ifdef CONFIG_SND_AUDIODOCK_SWITCH	
#ifdef D2199_CODEC_DEBUG
	dlg_info("%s() AudioDock Mode val=%d \n",__FUNCTION__,val);
#endif	
	if(val == 0x1) {
		gpio_set_value(mfp_to_gpio(GPIO054_GPIO_54),1);
	}
	else {
		gpio_set_value(mfp_to_gpio(GPIO054_GPIO_54),0);
	}
#endif	
	return 0;
}

#ifdef D2199_KEEP_POWER	
static int d2199_volatile_register(struct snd_soc_codec *codec,
				     unsigned int reg);
#endif

static int d2199_put_power_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];
#ifdef CONFIG_SEC_FACTORY_MODE
	struct i2c_client *client = d2199_codec->aad_i2c_client;
	struct d2199_aad_priv *d2199_aad = i2c_get_clientdata(client);
#endif

	mutex_lock(&d2199_codec->power_mutex);
	
	if(val ==0) {
		
		d2199_codec->power_disable_suspend=1;
#ifdef D2199_KEEP_POWER						
		for(i=0x2c; i<0x58; i++){
			if(d2199_reg_cache[i] != d2199_reg_defaults[i]) {
				if(d2199_volatile_register(codec,i) == 0) {
					snd_soc_write(codec,i,d2199_reg_defaults[i]);
				}
				d2199_reg_cache[i] = d2199_reg_defaults[i];
			}
		}

		for(i=0xa4; i<0xf0; i++){				
			if(d2199_reg_cache[i] != d2199_reg_defaults[i]) {
				if(d2199_volatile_register(codec,i) == 0) {
					snd_soc_write(codec,i,d2199_reg_defaults[i]);
				}
				d2199_reg_cache[i] = d2199_reg_defaults[i];
			}
		}
#else
#ifdef CONFIG_SEC_FACTORY_MODE
	if(d2199_aad->switch_data.state == D2199_HEADSET) {

		snd_soc_write(codec, D2199_CIF_CTRL,0x80);			
		snd_soc_write(codec,D2199_UNLOCK,0x8b);	
		snd_soc_write(codec,D2199_REFERENCES,0x88);	

		d2199_aad_enable(codec);	

		snd_soc_write(codec,D2199_MICBIAS1_CTRL,D2199_MICBIAS_EN 
			| D2199_MICBIAS_LEVEL_2_6V);
		snd_soc_write(codec,D2199_DROUTING_DAC_L,0x00);
		snd_soc_write(codec,D2199_DROUTING_DAC_R,0x00);				
			
		snd_soc_write(codec, D2199_LDO_CTRL, 0xB0); 
	
		snd_soc_write(codec,0x9a,0x01);
		snd_soc_write(codec,D2199_CP_VOL_THRESHOLD1,0x06);
		
		d2199_aad_button_fast_rate(d2199_aad,1);
		d2199_codec->power_mode=1;	
	} 
	else {
		d2199_codec_power(codec,0,1);			
	}
#else	
	d2199_codec_power(codec,0,1);	
#endif

#endif		
	}
	else { 		
#ifdef D2199_KEEP_POWER				
		d2199_codec_idle(codec);	
#else
		d2199_codec_power(codec,1,1);
#endif
		d2199_codec->power_disable_suspend=0;
	}

	mutex_unlock(&d2199_codec->power_mutex);
	return 0;	
}

static int d2199_get_cfg3_ctl_sw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = d2199_chip_rev;
	return 0;
}

static int d2199_put_sr_ctl_sw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

	if((val & 0x0F) == 0x1){
		d2199_codec->narrow_band = 1;
	}
	else {
		d2199_codec->narrow_band = 0;
	}
	
	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static int d2199_put_aux_l_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];
	
	if(!!(val & 0x80)) //FM radio enable
	{
		d2199_codec->fm_radio = 1;
	}
	else {
		d2199_codec->fm_radio = 0;	
	}
	
	return snd_soc_put_volsw(kcontrol, ucontrol);
}


extern void d2199_aad_button_fast_rate(struct d2199_aad_priv *d2199_aad, int enable);
static int d2199_put_mic_bias_1_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2199_codec->aad_i2c_client;
	struct d2199_aad_priv *d2199_aad = i2c_get_clientdata(client);
	unsigned int val = ucontrol->value.integer.value[0];

	mutex_lock(&d2199_codec->power_mutex);
	
	if(((val & 0x80) == 0) && d2199_aad->switch_data.state == D2199_HEADSET) {
		d2199_aad_button_fast_rate(d2199_aad,1);
		mutex_unlock(&d2199_codec->power_mutex);
		return 0;
	}

	d2199_aad_button_fast_rate(d2199_aad,0);
	
	snd_soc_put_volsw(kcontrol, ucontrol);

	mutex_unlock(&d2199_codec->power_mutex);
	return 0;
}

static int d2199_put_dmic_config_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

 	dlg_info("%s() val=%d \n",__FUNCTION__,val);

	mutex_lock(&d2199_codec->power_mutex);

	//100 : hifi rx, 001: hifi tx, 010: pcm tx/rx
	switch(val) {
		case 0x07://hifi tx/rx and pcm enable
		case 0x06://hifi rx and pcm enable 			
		case 0x03://hifi tx and pcm enable	
#ifdef D2199_CODEC_DEBUG
			dlg_info("%s() Hifi (rx or tx)/Pcm enable \n",__FUNCTION__);
#endif
			snd_soc_write(codec,D2199_DAI_2_CTRL,0xC0);	
			snd_soc_write(codec,D2199_PLL_CTRL1, 0x8C);
			
			snd_soc_write(codec,D2199_ASRC_UP_DPHI1,0x83); //0xba
			snd_soc_write(codec,D2199_ASRC_UP_DPHI2,0x05); //0x00
			snd_soc_write(codec,D2199_ASRC_DOWN_DPHI1,0xba); //0x83
			snd_soc_write(codec,D2199_ASRC_DOWN_DPHI2,0x00);//0x05
			snd_soc_write(codec,D2199_ASRC_CTRL,0x88);
			
			d2199_codec->pll_locked = 0;		
			d2199_codec->voice_call= 1;
			d2199_codec->aif1_enable= 1;
			d2199_codec->aif2_enable= 1;
			
			if(ssp_master == 1) {
				val=snd_soc_read(d2199_codec->codec,D2199_PLL_STATUS);
#ifdef D2199_CODEC_DEBUG
				dlg_info("%s() PLL status =%d \n",__FUNCTION__,val);
#endif					
			
				if((val & 0x02) == 0x00){
					if(d2199_codec->hp_enable) {
						snd_soc_write(d2199_codec->codec, D2199_HP_L_CTRL, 0x60);
						snd_soc_write(d2199_codec->codec, D2199_HP_R_CTRL, 0x60);
					}
					d2199_codec->pll_locked = 0;
					cancel_delayed_work(&d2199_codec->pll_lock_work);			
					schedule_delayed_work(&d2199_codec->pll_lock_work,
					msecs_to_jiffies(50));
				}
				else {
					d2199_codec->pll_locked = 1;
				}
			}
			break;
			
		case 0x04: //hifi rx enable
		case 0x01: //hifi tx enable
#ifdef D2199_CODEC_DEBUG
			dlg_info("%s() Hifi rx or tx enable \n",__FUNCTION__);
#endif		
			snd_soc_write(codec,D2199_DAI_2_CTRL,0xC0);	
			snd_soc_write(codec,D2199_PLL_CTRL1, 0x8C);
			snd_soc_write(codec,D2199_ASRC_CTRL,0x01);
			snd_soc_write(codec,D2199_DAC_R_CTRL, 0xA0);
			snd_soc_write(codec,D2199_DAC_L_CTRL, 0xA0);
				
			d2199_codec->pll_locked = 0;
			d2199_codec->voice_call= 0;
			d2199_codec->aif2_enable= 1;
			d2199_codec->aif1_enable= 0;
			break;
			
		case 0x02: //pcm enable	
#ifdef D2199_CODEC_DEBUG
			dlg_info("%s() Pcm enable \n",__FUNCTION__);
#endif				
			snd_soc_write(codec,D2199_DAI_2_CTRL,0x00);	
			snd_soc_write(codec,D2199_PLL_CTRL1, 0x88);
			snd_soc_write(codec,D2199_ASRC_CTRL,0x01);

			d2199_codec->pll_locked = 0;
			d2199_codec->voice_call= 1;
			d2199_codec->aif1_enable= 1;
			d2199_codec->aif2_enable= 0;

			
			if(ssp_master == 1) {
				val=snd_soc_read(d2199_codec->codec,D2199_PLL_STATUS);
#ifdef D2199_CODEC_DEBUG
				dlg_info("%s() PLL status =0x%x \n",__FUNCTION__,val);
#endif							
	
				if((val & 0x02) == 0x00){
					if(d2199_codec->hp_enable) {
						snd_soc_write(d2199_codec->codec, D2199_HP_L_CTRL, 0x60);
						snd_soc_write(d2199_codec->codec, D2199_HP_R_CTRL, 0x60);
					}
					d2199_codec->pll_locked = 0;
					cancel_delayed_work(&d2199_codec->pll_lock_work);			
					schedule_delayed_work(&d2199_codec->pll_lock_work,
					msecs_to_jiffies(50));
				}
				else {
					d2199_codec->pll_locked = 1;
				}
			}
			break;
			
		case 0x00: //both disable
#ifdef D2199_CODEC_DEBUG
			dlg_info("%s() All disable \n",__FUNCTION__);
#endif				
			snd_soc_write(codec,D2199_ASRC_CTRL,0x01);
			d2199_codec->pll_locked = 0;
			d2199_codec->voice_call= 0;
			d2199_codec->aif1_enable= 0;
			d2199_codec->aif2_enable= 0;
			break;
	}
	
	mutex_unlock(&d2199_codec->power_mutex);
	return 0;
}

static int d2199_put_pll_ctl1_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];
	
	if((val & 0x80) == 0x80) {		
		if(ssp_master){ //codec both slave
			snd_soc_write(codec,D2199_PLL_CTRL1,0x8C);
		}
		else { //codec both master
			snd_soc_write(codec,D2199_PLL_CTRL1,0x8C);
			msleep(30);
			snd_soc_write(codec,D2199_PLL_CTRL1,0x80);
		}
		return 0;
	}
	
	return snd_soc_put_volsw(kcontrol, ucontrol);
}


static int d2199_put_adc_l_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

	if(ssp_master == 1 && d2199_codec->voice_call == 1){
		if((val & 0x80) == 0x80 && (d2199_codec->pll_locked == 0)) {	
			d2199_codec->adc_l_enable = 1;
			return 0;
		}
		else {
			d2199_codec->adc_l_enable = 0;
		}
	}
	return snd_soc_put_volsw(kcontrol, ucontrol);	
}

static int d2199_put_adc_r_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

	if(ssp_master == 1 && d2199_codec->voice_call == 1){		
		if((val & 0x80) == 0x80 && (d2199_codec->pll_locked == 0)) {	
			d2199_codec->adc_r_enable = 1;
			return 0;
		}
		else {
			d2199_codec->adc_r_enable = 0;
		}
	}
	return snd_soc_put_volsw(kcontrol, ucontrol);	
}

static int d2199_put_dac_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

	if(ssp_master == 1 && d2199_codec->voice_call == 1){
		if((val & 0x80) == 0x80 && (d2199_codec->pll_locked == 0)) {		
			return 0;
		}
	}
	
	return snd_soc_put_volsw(kcontrol, ucontrol);	
}

static int d2199_put_hp_l_ctl_sw(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val = ucontrol->value.integer.value[0];

	if((val & 0x80) == 0x80) {
		d2199_codec->hp_enable = 1;
	}
	else {
		d2199_codec->hp_enable = 0;
	}
	
	return snd_soc_put_volsw(kcontrol, ucontrol);	
}

static const struct snd_kcontrol_new d2199_snd_controls[] = {

	SOC_SINGLE_EXT("D2199_CIF_CTRL", D2199_CIF_CTRL, 0, 0xff, 0,
	snd_soc_get_volsw, d2199_put_power_ctl_sw),
	SOC_SINGLE_EXT("D2199_SR", D2199_SR, 0, 0xff, 0, snd_soc_get_volsw, 
		d2199_put_sr_ctl_sw),
	SOC_SINGLE("D2199_PC_COUNT", D2199_PC_COUNT, 0, 0xff, 0),
	SOC_SINGLE("D2199_GAIN_RAMP_CTRL", D2199_GAIN_RAMP_CTRL, 0, 0xff, 0),
	SOC_SINGLE_EXT("D2199_SYSTEM_MODES_CFG3", D2199_SYSTEM_MODES_CFG3, 0, 0xff,
		0, d2199_get_cfg3_ctl_sw, snd_soc_put_volsw),
	
	SOC_SINGLE("D2199_ADC_FILTERS1", D2199_ADC_FILTERS1, 0, 0xff, 0),
	SOC_SINGLE("D2199_ADC_FILTERS2", D2199_ADC_FILTERS2, 0, 0xff, 0),
	SOC_SINGLE("D2199_ADC_FILTERS3", D2199_ADC_FILTERS3, 0, 0xff, 0),
	SOC_SINGLE("D2199_ADC_FILTERS4", D2199_ADC_FILTERS4, 0, 0xff, 0),
	
	SOC_SINGLE("D2199_DAC_FILTERS1", D2199_DAC_FILTERS1, 0, 0xff, 0),
	SOC_SINGLE("D2199_DAC_FILTERS2", D2199_DAC_FILTERS2, 0, 0xff, 0),
	SOC_SINGLE("D2199_DAC_FILTERS3", D2199_DAC_FILTERS3, 0, 0xff, 0),
	SOC_SINGLE("D2199_DAC_FILTERS4", D2199_DAC_FILTERS4, 0, 0xff, 0),
	SOC_SINGLE("D2199_DAC_FILTERS5", D2199_DAC_FILTERS5, 0, 0xff, 0),
	
	SOC_SINGLE("D2199_DROUTING_DAI_1_L", D2199_DROUTING_DAI_1_L, 0, 0xff, 0),
	SOC_SINGLE("D2199_DROUTING_DAI_1_R", D2199_DROUTING_DAI_1_R, 0, 0xff, 0),
	SOC_SINGLE("D2199_DROUTING_DAI_2_L", D2199_DROUTING_DAI_2_L, 0, 0xff, 0),
	SOC_SINGLE("D2199_DROUTING_DAI_2_R", D2199_DROUTING_DAI_2_R, 0, 0xff, 0),
	SOC_SINGLE("D2199_DROUTING_DAC_L", D2199_DROUTING_DAC_L, 0, 0xff,
		   0),
	SOC_SINGLE("D2199_DROUTING_DAC_R", D2199_DROUTING_DAC_R, 0, 0xff,
		   0),
	SOC_SINGLE("D2199_DMIX_DAC_R_DAI_2_L_GAIN", D2199_DMIX_DAC_R_DAI_2_L_GAIN, 
		0, 0xff, 0),
	SOC_SINGLE("D2199_DMIX_DAC_R_DAI_2_R_GAIN", D2199_DMIX_DAC_R_DAI_2_R_GAIN, 
		0, 0xff, 0),
	SOC_SINGLE("D2199_DIG_CTRL", D2199_DIG_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_DAI_1_CTRL", D2199_DAI_1_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_DAI_1_OFFSET", D2199_DAI_1_OFFSET, 0, 0xff, 0),
	SOC_SINGLE_EXT("D2199_DAI_1_CLK_MODE", D2199_DAI_1_CLK_MODE, 0, 0xff, 0,
		       snd_soc_get_volsw, d2199_put_dai_1_clk_mode_ctl_sw),

	//SOC_SINGLE("D2199_DAI_2_CTRL", D2199_DAI_2_CTRL, 0, 0xff, 0),
	SOC_SINGLE_EXT("D2199_DAI_2_CTRL", D2199_DAI_2_CTRL, 0, 0xff, 0,
		       snd_soc_get_volsw, d2199_put_dai_2_ctrl_ctl_sw),
	SOC_SINGLE("D2199_DAI_2_OFFSET", D2199_DAI_2_OFFSET, 0, 0xff, 0),
	//SOC_SINGLE("D2199_DAI_2_CLK_MODE", D2199_DAI_2_CLK_MODE, 0, 0xff, 0),
	SOC_SINGLE_EXT("D2199_DAI_2_CLK_MODE", D2199_DAI_2_CLK_MODE, 0, 0xff, 0,
		       snd_soc_get_volsw, d2199_put_dai_2_clk_mode_ctl_sw),

	SOC_SINGLE("D2199_DAC_NG_CTRL", D2199_DAC_NG_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_DAC_NG_SETUP_TIME", D2199_DAC_NG_SETUP_TIME, 0,
		   0xff, 0),
	SOC_SINGLE("D2199_DAC_NG_OFF_THRESHOLD", D2199_DAC_NG_OFF_THRESHOLD, 0,
		   0xff, 0),
	SOC_SINGLE("D2199_DAC_NG_ON_THRESHOLD",  D2199_DAC_NG_ON_THRESHOLD, 0, 
		   0xff, 0),

	SOC_SINGLE_EXT("D2199_MIC_CONFIG", D2199_MIC_CONFIG, 0, 0xff, 0,
		       snd_soc_get_volsw, d2199_put_dmic_config_ctl_sw),
	
	SOC_SINGLE("D2199_LIMITER_CTRL1", D2199_LIMITER_CTRL1, 0, 0xff, 0),
	SOC_SINGLE("D2199_LIMITER_CTRL2", D2199_LIMITER_CTRL2, 0, 0xff, 0),
	SOC_SINGLE("D2199_LIMITER_PWR_LIM", D2199_LIMITER_PWR_LIM, 0, 0xff, 0),
	SOC_SINGLE("D2199_LIMITER_THD_LIM", D2199_LIMITER_THD_LIM, 0, 0xff, 0),
	
	SOC_SINGLE("D2199_NG_CTRL1", D2199_NG_CTRL1, 0, 0xff, 0),
	SOC_SINGLE("D2199_NG_CTRL2", D2199_NG_CTRL2, 0, 0xff, 0),

	//SOC_SINGLE("D2199_PLL_CTRL1", D2199_PLL_CTRL1, 0, 0xff, 0),		       
	SOC_SINGLE_EXT("D2199_PLL_CTRL1", D2199_PLL_CTRL1, 0, 0xff, 0,
			  snd_soc_get_volsw, d2199_put_pll_ctl1_ctl_sw),		       
	
	SOC_SINGLE("D2199_PLL_CTRL2", D2199_PLL_CTRL2, 0, 0xff, 0),

	SOC_SINGLE("D2199_PLL_FRAC_TOP_44", D2199_PLL_FRAC_TOP_44, 0, 0xff, 0),
	SOC_SINGLE("D2199_PLL_FRAC_BOT_44", D2199_PLL_FRAC_BOT_44, 0, 0xff, 0),
	SOC_SINGLE("D2199_PLL_INTEGER_44", D2199_PLL_INTEGER_44, 0, 0xff, 0),

	SOC_SINGLE("D2199_PLL_FRAC_TOP_48", D2199_PLL_FRAC_TOP_48, 0, 0xff, 0),
	SOC_SINGLE("D2199_PLL_FRAC_BOT_48", D2199_PLL_FRAC_BOT_48, 0, 0xff, 0),
	SOC_SINGLE("D2199_PLL_INTEGER_48", D2199_PLL_INTEGER_48, 0, 0xff, 0),
	
	SOC_SINGLE("D2199_CP_CTRL", D2199_CP_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_CP_DELAY", D2199_CP_DELAY, 0, 0xff, 0),
	SOC_SINGLE("D2199_CP_DETECTOR", D2199_CP_DETECTOR, 0, 0xff, 0),
	SOC_SINGLE("D2199_CP_VOL_THRESHOLD1", D2199_CP_VOL_THRESHOLD1, 0, 0xff, 0),
	
	SOC_SINGLE_EXT("D2199_AUX_L_CTRL", D2199_AUX_L_CTRL, 0, 0xff, 0,
			snd_soc_get_volsw, d2199_put_aux_l_ctl_sw),
	
	SOC_SINGLE("D2199_AUX_L_GAIN", D2199_AUX_L_GAIN, 0, 0xff, 0),
	SOC_SINGLE("D2199_AUX_R_CTRL", D2199_AUX_R_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_AUX_R_GAIN", D2199_AUX_R_GAIN, 0, 0xff, 0),
	
	SOC_SINGLE("D2199_MIC_2_CTRL", D2199_MIC_2_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIC_2_GAIN", D2199_MIC_2_GAIN, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIC_3_CTRL", D2199_MIC_3_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIC_3_GAIN", D2199_MIC_3_GAIN, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIC_1_CTRL", D2199_MIC_1_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIC_1_GAIN", D2199_MIC_1_GAIN, 0, 0xff, 0),
	
	SOC_SINGLE_EXT("D2199_MICBIAS1_CTRL", D2199_MICBIAS1_CTRL, 0, 0xff, 0,
			snd_soc_get_volsw, d2199_put_mic_bias_1_ctl_sw),
	SOC_SINGLE("D2199_MICBIAS2_CTRL", D2199_MICBIAS2_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_MICBIAS3_CTRL", D2199_MICBIAS3_CTRL, 0, 0xff, 0),
	
	SOC_SINGLE("D2199_MIXIN_L_CTRL", D2199_MIXIN_L_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIXIN_L_GAIN", D2199_MIXIN_L_GAIN, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIXIN_L_SELECT", D2199_MIXIN_L_SELECT, 0, 0xff, 0),
	
	SOC_SINGLE("D2199_MIXIN_R_CTRL", D2199_MIXIN_R_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIXIN_R_GAIN", D2199_MIXIN_R_GAIN, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIXIN_R_SELECT", D2199_MIXIN_R_SELECT, 0, 0xff, 0),

	SOC_SINGLE_EXT("D2199_ADC_L_CTRL", D2199_ADC_L_CTRL, 0, 0xff, 0,
			snd_soc_get_volsw, d2199_put_adc_l_ctl_sw),
	SOC_SINGLE("D2199_ADC_L_GAIN", D2199_ADC_L_GAIN, 0, 0xff, 0),

	SOC_SINGLE_EXT("D2199_ADC_R_CTRL", D2199_ADC_R_CTRL, 0, 0xff, 0,
			snd_soc_get_volsw, d2199_put_adc_r_ctl_sw),
	SOC_SINGLE("D2199_ADC_R_GAIN", D2199_ADC_R_GAIN, 0, 0xff, 0),
	
	SOC_SINGLE_EXT("D2199_DAC_L_CTRL", D2199_DAC_L_CTRL, 0, 0xff,0,
			snd_soc_get_volsw, d2199_put_dac_ctl_sw),
	SOC_SINGLE("D2199_DAC_L_GAIN", D2199_DAC_L_GAIN, 0, 0xff,
		   0),
	SOC_SINGLE_EXT("D2199_DAC_R_CTRL", D2199_DAC_R_CTRL, 0, 0xff, 0,
			snd_soc_get_volsw, d2199_put_dac_ctl_sw),
	SOC_SINGLE("D2199_DAC_R_GAIN", D2199_DAC_R_GAIN, 0, 0xff,
		   0),
	SOC_SINGLE("D2199_MIXOUT_L_CTRL", D2199_MIXOUT_L_CTRL,
		   0, 0xff, 0),
	SOC_SINGLE("D2199_MIXOUT_L_SELECT", D2199_MIXOUT_L_SELECT, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIXOUT_R_CTRL", D2199_MIXOUT_R_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_MIXOUT_R_SELECT", D2199_MIXOUT_R_SELECT, 0, 0xff, 0),		   
	SOC_SINGLE("D2199_MIXOUT_SP_CTRL", D2199_MIXOUT_SP_CTRL,
		   0, 0xff, 0),
	SOC_SINGLE("D2199_MIXOUT_SP_SELECT", D2199_MIXOUT_SP_SELECT, 0, 0xff, 0),
	SOC_SINGLE_EXT("D2199_HP_L_CTRL", D2199_HP_L_CTRL, 0, 0xff, 0,
			snd_soc_get_volsw, d2199_put_hp_l_ctl_sw),
	SOC_SINGLE("D2199_HP_L_GAIN", D2199_HP_L_GAIN, 0,
		   0xff, 0),
	SOC_SINGLE("D2199_HP_R_CTRL", D2199_HP_R_CTRL, 0, 0xff, 0),
	SOC_SINGLE("D2199_HP_R_GAIN", D2199_HP_R_GAIN, 0,
		   0xff, 0),
	SOC_SINGLE("D2199_EP_CTRL", D2199_EP_CTRL, 0,
		   0xff, 0),
	SOC_SINGLE("D2199_EP_GAIN", D2199_EP_GAIN, 0,
		   0xff, 0),
	SOC_SINGLE("D2199_SP_CTRL", D2199_SP_CTRL, 0,
		   0xff, 0),
	SOC_SINGLE("D2199_SP_GAIN", D2199_SP_GAIN, 0,
		   0xff, 0),
	SOC_SINGLE_EXT("D2199_SP_CFG1", D2199_SP_CFG1, 0,  0xff, 0,
				snd_soc_get_volsw, d2199_put_sp_cfg1_sw),
	SOC_SINGLE_EXT("D2199_SP_CFG2", D2199_SP_CFG1, 0,  0xff, 0,
				snd_soc_get_volsw, d2199_put_sp_cfg2_sw),
		   
	SOC_SINGLE("D2199_REFERENCES", D2199_REFERENCES, 0,
		   0xff, 0),
	SOC_SINGLE("D2199_LDO_CTRL", D2199_LDO_CTRL, 0,
		   0xff, 0),
	
};

#ifdef D2199_KEEP_POWER	
static int d2199_volatile_register(struct snd_soc_codec *codec,
				     unsigned int reg)
{
	switch (reg) {
	case D2199_CIF_CTRL:
#ifdef CONFIG_SND_SOC_USE_d2199_HW	
	case D2199_STATUS1:
#endif	
	case D2199_SYSTEM_STATUS:
	case D2199_ALC_CTRL1:
	case D2199_ALC_OFFSET_AUTO_M_L:
	case D2199_ALC_OFFSET_AUTO_U_L:
	case D2199_ALC_OFFSET_AUTO_M_R:
	case D2199_ALC_OFFSET_AUTO_U_R:
	case D2199_ALC_CIC_OP_LVL_DATA:
	case D2199_PLL_STATUS:
	case D2199_AUX_L_GAIN_STATUS:
	case D2199_AUX_R_GAIN_STATUS:
	case D2199_MIC_2_GAIN_STATUS:
	case D2199_MIC_3_GAIN_STATUS:
	case D2199_MIC_1_GAIN_STATUS:
	case D2199_MIXIN_L_GAIN_STATUS:
	case D2199_MIXIN_R_GAIN_STATUS:
	case D2199_ADC_L_GAIN_STATUS:
	case D2199_ADC_R_GAIN_STATUS:
	case D2199_DAC_L_GAIN_STATUS:
	case D2199_DAC_R_GAIN_STATUS:
	case D2199_HP_L_GAIN_STATUS:
	case D2199_HP_R_GAIN_STATUS:
	case D2199_EP_GAIN_STATUS:
	case D2199_SP_GAIN_STATUS:
	case D2199_SP_STATUS:
		return 1;
	default:
		return 0;
	}
}
#endif

/* 
 * Need to make this method availabe outside of the file for Soundpath usage
 * as we don't want to assign the function to the snd_soc_dai_ops. If we do
 * assign it then the method is automatically called from ALSA framework and
 * this could interfere with Soundpath functionality.
 */
static int d2199_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	//struct snd_soc_codec *codec = dai->codec;
	//struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	u8 aif_ctrl = 0;
	u8 fs;

	dlg_info("%s() start  \n",__FUNCTION__);
	
	/* Set AIF format */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dlg_info("%s() 16bit  \n",__FUNCTION__);
		aif_ctrl |= D2199_DAI_WORD_LENGTH_S16_LE;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		aif_ctrl |= D2199_DAI_WORD_LENGTH_S20_LE;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		aif_ctrl |= D2199_DAI_WORD_LENGTH_S24_LE;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		aif_ctrl |= D2199_DAI_WORD_LENGTH_S32_LE;
		break;
	default:
		return -EINVAL;
	}

	/* Set sampling rate */
	switch (params_rate(params)) {
	case 8000:
		fs = D2199_SR_8000;
		break;
	case 11025:
		fs = D2199_SR_11025;
		break;
	case 12000:
		fs = D2199_SR_12000;
		break;
	case 16000:
		fs = D2199_SR_16000;
		break;
	case 22050:
		fs = D2199_SR_22050;
		break;
	case 32000:
		fs = D2199_SR_32000;
		break;
	case 44100:
		fs = D2199_SR_44100;
		break;
	case 48000:
		fs = D2199_SR_48000;
		break;
	case 88200:
		fs = D2199_SR_88200;
		break;
	case 96000:
		fs = D2199_SR_96000;
		break;
	default:
		return -EINVAL;
	}

	//snd_soc_update_bits(codec, D2199_DAI_1_CTRL, D2199_DAI_WORD_LENGTH_MASK,
	//			    aif_ctrl);
	//snd_soc_write(codec, D2199_SR, fs);

	return 0;
}

static int d2199_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 aif_clk_mode = 0, aif_ctrl = 0;

	dlg_info("%s() format = %d  \n",__FUNCTION__,fmt);
	/* Set master/slave mode */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aif_clk_mode |= D2199_DAI_CLK_EN_MASTER_MODE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		aif_clk_mode |= D2199_DAI_CLK_EN_SLAVE_MODE;
		break;
	default:
		return -EINVAL;
	}

	/* Set clock normal/inverted */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		aif_clk_mode |= D2199_DAI_WCLK_POL_INV;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		aif_clk_mode |= D2199_DAI_CLK_POL_INV;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		aif_clk_mode |= D2199_DAI_WCLK_POL_INV | D2199_DAI_CLK_POL_INV;
		break;
	default:
		return -EINVAL;
	}

	/* Only I2S is supported */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		aif_ctrl |= D2199_DAI_FORMAT_I2S_MODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		aif_ctrl |= D2199_DAI_FORMAT_LEFT_J;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		aif_ctrl |= D2199_DAI_FORMAT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		aif_ctrl |= D2199_DAI_FORMAT_DSP;
		break;	
	default:
		return -EINVAL;
	}

	/* By default only 32 BCLK per WCLK is supported */
	aif_clk_mode |= D2199_DAI_BCLKS_PER_WCLK_32;

	snd_soc_write(codec, D2199_DAI_1_CLK_MODE, aif_clk_mode);
	snd_soc_update_bits(codec, D2199_DAI_1_CTRL, D2199_DAI_FORMAT_MASK,
			    aif_ctrl);

	return 0;
}

static int d2199_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int d2199_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				int clk_id, unsigned int freq, int dir)
{
	return 0;
}

#define D2199_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
		       SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops d2199_dai_ops = {
	.hw_params	= d2199_hw_params,
	.set_fmt	= d2199_set_dai_fmt,
	.set_sysclk	= d2199_set_dai_sysclk,
	.digital_mute	= d2199_mute,
};


static struct snd_soc_dai_driver d2199_dai[] = {
	{
		.name = "d2199-i2s",
		.playback = {
			.stream_name = "I2S Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FORMAT_S16_LE |
					SNDRV_PCM_FORMAT_S18_3LE },
		.capture = {
			.stream_name = "I2S Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FORMAT_S16_LE |
					SNDRV_PCM_FORMAT_S18_3LE |
					  SNDRV_PCM_FMTBIT_S32_LE, },
		.ops =  &d2199_dai_ops,
	},
	{
		.name = "d2199-pcm",
		.playback = {
			.stream_name = "PCM Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FORMAT_S8|		\
					  SNDRV_PCM_FORMAT_S16_LE |	\
					  SNDRV_PCM_FORMAT_S20_3LE |	\
					  SNDRV_PCM_FORMAT_S24 |
					  SNDRV_PCM_FMTBIT_S32_LE, },
		.capture = {
			.stream_name = "PCM Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FORMAT_S8|		\
					  SNDRV_PCM_FORMAT_S16_LE |	\
					  SNDRV_PCM_FORMAT_S20_3LE |	\
					  SNDRV_PCM_FORMAT_S24 },
		.ops =  &d2199_dai_ops,
	},
	{
		/* dummy (SAI3) */
		.name	= "d2199-dummy",
		.id	= 3,
		.playback = {
			.stream_name	= "PCM dummy Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S8|		\
					  SNDRV_PCM_FORMAT_S16_LE |	\
					  SNDRV_PCM_FORMAT_S20_3LE |	\
					  SNDRV_PCM_FORMAT_S24 |	\
					  SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name	= "PCM dummy Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FORMAT_S8|		\
					  SNDRV_PCM_FORMAT_S16_LE |	\
					  SNDRV_PCM_FORMAT_S20_3LE |	\
					  SNDRV_PCM_FORMAT_S24 |	\
					  SNDRV_PCM_FMTBIT_S32_LE,
		},
	},
};
EXPORT_SYMBOL(d2199_dai);

static int d2199_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	dlg_info("%s() level = %d  \n",__FUNCTION__,level);
	
	codec->dapm.bias_level = level;
	return 0;
}

static int d2199_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int d2199_resume(struct snd_soc_codec *codec)
{
	return 0;
}

static int d2199_probe(struct snd_soc_codec *codec)
{
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	
	d2199_codec->codec = codec;	
	
	codec->using_regmap=0; 
	
	d2199_set_bits(d2199_codec->d2199_pmic,D2199_PULLDOWN_D_REG,
					D2199_LDO21_PD_DIS_MASK);
	d2199_set_bits(d2199_codec->d2199_pmic,D2199_PULLDOWN_D_REG,
					D2199_LDO22_PD_DIS_MASK);

	d2199_reg_read(d2199_codec->d2199_pmic, 0x96, &d2199_chip_rev);

	printk(KERN_ERR"[%s] PMIC Chip Version = %x \n", __func__,d2199_chip_rev);
	
	mutex_init(&d2199_codec->power_mutex);
	
	d2199_codec->clk = clk_get(NULL, "VCXO_OUT");
	if (IS_ERR(d2199_codec->clk)) {
		pr_err("unable to get VCXO_OUT\n");
		d2199_codec->clk = NULL;
	}

	if (d2199_codec->clk) {
		clk_enable(d2199_codec->clk);
		d2199_codec->mclk_enable = 1;
		dlg_info("%s() MCLK enable\n", __FUNCTION__);
	}
	else {
		dlg_info("%s() MCLK enable failed\n", __FUNCTION__);
	}

	snd_soc_add_codec_controls(codec, d2199_snd_controls,
			     ARRAY_SIZE(d2199_snd_controls));

#ifdef D2199_KEEP_POWER
	memcpy(d2199_reg_cache,d2199_reg_defaults,255);
#endif

	d2199_codec_power(d2199_codec->codec,1,1);
	d2199_codec->power_disable_suspend =1;
	d2199_codec_power(d2199_codec->codec,0,0);
	
	d2199_codec->codec_init =1;	

	INIT_DELAYED_WORK(&d2199_codec->pll_lock_work, 
		d2199_pll_lock_monitor_timer_work);
	
#ifdef CONFIG_SND_AUDIODOCK_SWITCH	
	gpio_direction_output(mfp_to_gpio(GPIO054_GPIO_54),1);
	gpio_set_value(mfp_to_gpio(GPIO054_GPIO_54),0);
#endif	
	
	return 0;
}

static int d2199_remove(struct snd_soc_codec *codec)
{
	d2199_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static unsigned int d2199_codec_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);	
	struct i2c_msg xfer[2];
	u8 data;
	int ret=-1;

	mutex_lock(&d2199_codec->d2199_pmic->d2199_io_mutex);
	/* Write register */
	xfer[0].addr = d2199_codec->i2c_client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = (unsigned char *)(&reg);

	/* Read data */
	xfer[1].addr = d2199_codec->i2c_client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 1;
	xfer[1].buf = &data;

	ret = i2c_transfer(d2199_codec->i2c_client->adapter, xfer, 2);

	mutex_unlock(&d2199_codec->d2199_pmic->d2199_io_mutex);
	
	if (ret == 2)
		return data;
	else if (ret < 0)
		return ret;
	else
		return -EIO;

}

static int d2199_codec_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	struct d2199_codec_priv *d2199_codec = snd_soc_codec_get_drvdata(codec);
	u8 data[2];
	int ret=-1;

	mutex_lock(&d2199_codec->d2199_pmic->d2199_io_mutex);

#ifdef D2199_CODEC_DEBUG
	dlg_info("%s() reg=0x%x value=0x%x \n",__FUNCTION__,reg,value);
#endif

	reg &= 0xff;
	data[0] = (u8)reg;
	data[1] = value & 0xff;

	ret = i2c_master_send(d2199_codec->i2c_client, data, 2);

	mutex_unlock(&d2199_codec->d2199_pmic->d2199_io_mutex);
	
	if (ret == 2)
	{
#ifdef D2199_KEEP_POWER		
		d2199_reg_cache[reg] = value;
#endif
		return 0;
	}
	else if (ret < 0)
		return ret;
	else
		return -EIO;

}

static struct snd_soc_codec_driver soc_codec_dev_d2199 = {
	.probe			= d2199_probe,
	.remove			= d2199_remove,
	.suspend			= d2199_suspend,
	.resume			= d2199_resume,
	.read 			= d2199_codec_read,
	.write			= d2199_codec_write,
	//.reg_cache_size		= ARRAY_SIZE(d2199_reg_defaults),
	//.reg_word_size		= sizeof(u8),
	//.reg_cache_default	= d2199_reg_defaults,
	//.volatile_register	= d2199_volatile_register,	
	.set_bias_level		= d2199_set_bias_level,
	//.controls		= d2199_snd_controls,
	//.num_controls		= ARRAY_SIZE(d2199_snd_controls),	
	//.dapm_widgets		= d2199_dapm_widgets,
	//.num_dapm_widgets	= ARRAY_SIZE(d2199_dapm_widgets),
	//.dapm_routes		= d2199_audio_map,
	//.num_dapm_routes	= ARRAY_SIZE(d2199_audio_map),
};

#ifdef CONFIG_SND_SOC_D2199_AAD
static struct i2c_board_info aad_i2c_info = {
	I2C_BOARD_INFO("d2199-aad", D2199_AAD_I2C_ADDR),
};
#endif /* CONFIG_SND_SOC_D2199_AAD */

#ifdef CONFIG_PROC_FS
static int d2199codec_i2c_read_device(struct d2199_codec_priv *d2199_codec,char reg,
					int bytes, void *dest)
{
	int ret;
	struct i2c_msg msgs[2];
	struct i2c_adapter *adap = d2199_codec->i2c_client->adapter;

	if(reg > 0x65 && reg < 0x75){
		msgs[0].addr = D2199_AAD_I2C_ADDR;
	}
	else {
		msgs[0].addr = D2199_CODEC_I2C_ADDR;
	}
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	if(reg > 0x65 && reg < 0x75){
		msgs[1].addr = D2199_AAD_I2C_ADDR;
	}
	else {
		msgs[1].addr = D2199_CODEC_I2C_ADDR;
	}
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = bytes;
	msgs[1].buf = (char *)dest;

	ret = i2c_transfer(adap,msgs,ARRAY_SIZE(msgs));

	if (ret < 0 )
		return ret;
	else if (ret == ARRAY_SIZE(msgs))
		return 0;
	else
		return -EFAULT;
}

static int d2199codec_i2c_write_device(struct d2199_codec_priv *d2199_codec,char reg,
				   int bytes, u8 *src )
{
	int ret;
	struct i2c_msg msgs[1];
	u8 data[12];
	u8 *buf = data;
	
	struct i2c_adapter *adap = d2199_codec->i2c_client->adapter;

	if (bytes == 0)
		return -EINVAL;

	BUG_ON(bytes >= ARRAY_SIZE(data));

	if(reg > 0x65 && reg < 0x75){
		msgs[0].addr = D2199_AAD_I2C_ADDR;
	}
	else {
		msgs[0].addr = D2199_CODEC_I2C_ADDR;
	}
	msgs[0].flags = d2199_codec->i2c_client->flags & I2C_M_TEN;
	msgs[0].len = 1+bytes;
	msgs[0].buf = data;

	*buf++ = reg;
	while(bytes--) *buf++ = *src++;

	ret = i2c_transfer(adap,msgs,ARRAY_SIZE(msgs));

	if (ret < 0 )
		return ret;
	else if (ret == ARRAY_SIZE(msgs))
		return 0;
	else
		return -EFAULT;
}


static int d2199_codec_ioctl_open(struct inode *inode, struct file *file)
{
	dlg_info("%s\n", __func__);
	file->private_data = PDE(inode)->data;
	return 0;
}

int d2199_codec_ioctl_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*
 *
 */
static long d2199_codec_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		struct d2199_codec_priv *d2199_codec =  file->private_data;
		pmu_reg reg;
		int ret = 0;
		u8 reg_val;
	
		if (!d2199_codec)
			return -ENOTTY; 		

		switch (cmd) {
			
			case D2199_IOCTL_READ_REG:
				if (copy_from_user(&reg, (pmu_reg *)arg, sizeof(pmu_reg)) != 0)
					return -EFAULT;

				d2199codec_i2c_read_device(d2199_codec,(u8)(reg.reg),1,(void *)(&reg_val));

				reg.val = (unsigned short)reg_val;
				if (copy_to_user((pmu_reg *)arg, &reg, sizeof(pmu_reg)) != 0)
					return -EFAULT;
			break;
	
			case D2199_IOCTL_WRITE_REG:
				if (copy_from_user(&reg, (pmu_reg *)arg, sizeof(pmu_reg)) != 0)
					return -EFAULT;
				
				d2199codec_i2c_write_device(d2199_codec,(char)(reg.reg),1, (u8 *)(&reg.val));

			break;	
	
		default:
			dlg_err("%s: unsupported cmd\n", __func__);
			ret = -ENOTTY;
		}
	
		return ret;
}

#define MAX_D2199_CODEC_USER_INPUT_LEN      100
#define MAX_D2199_CODEC_REGS_READ_WRITE     0xDF

enum codec_debug_ops {
	CODECDBG_READ_REG = 0UL,
	CODECDBG_WRITE_REG,
};

struct codec_debug {
	int read_write;
	int len;
	int addr;
	u8 val[MAX_D2199_CODEC_REGS_READ_WRITE];
};

/*
 *
 */
static void d2199_codec_dbg_usage(void)
{
	printk(KERN_INFO "Usage:\n");
	printk(KERN_INFO "Read a register: echo 0x0800 > /proc/codec0\n");
	printk(KERN_INFO
		"Read multiple regs: echo 0x0800 -c 10 > /proc/codec0\n");
	printk(KERN_INFO
		"Write multiple regs: echo 0x0800 0xFF 0xFF > /proc/codec0\n");
	printk(KERN_INFO
		"Write single reg: echo 0x0800 0xFF > /proc/codec0\n");
	printk(KERN_INFO "Max number of regs in single write is :%d\n",
		MAX_D2199_CODEC_REGS_READ_WRITE);
	printk(KERN_INFO "Register address is encoded as follows:\n");
	printk(KERN_INFO "0xSSRR, SS: i2c slave addr, RR: register addr\n");
}


/*
 *
 */
static int d2199_codec_dbg_parse_args(char *cmd, struct codec_debug *dbg)
{
	char *tok;                 /* used to separate tokens             */
	const char ct[] = " \t";   /* space or tab delimits the tokens    */
	bool count_flag = false;   /* whether -c option is present or not */
	int tok_count = 0;         /* total number of tokens parsed       */
	int i = 0;

	dbg->len        = 0;

	/* parse the input string */
	while ((tok = strsep(&cmd, ct)) != NULL) {
		dlg_info("token: %s\n", tok);

		/* first token is always address */
		if (tok_count == 0) {
			sscanf(tok, "%x", &dbg->addr);
		} else if (strnicmp(tok, "-c", 2) == 0) {
			/* the next token will be number of regs to read */
			tok = strsep(&cmd, ct);
			if (tok == NULL)
				return -EINVAL;

			tok_count++;
			sscanf(tok, "%d", &dbg->len);
			count_flag = true;
			break;
		} else {
			int val;

			/* this is a value to be written to the pmu register */
			sscanf(tok, "%x", &val);
			if (i < MAX_D2199_CODEC_REGS_READ_WRITE) {
				dbg->val[i] = val;
				i++;
			}
		}

		tok_count++;
	}

	/* decide whether it is a read or write operation based on the
	 * value of tok_count and count_flag.
	 * tok_count = 0: no inputs, invalid case.
	 * tok_count = 1: only reg address is given, so do a read.
	 * tok_count > 1, count_flag = false: reg address and atleast one
	 *     value is present, so do a write operation.
	 * tok_count > 1, count_flag = true: to a multiple reg read operation.
	 */
	switch (tok_count) {
	case 0:
		return -EINVAL;
	case 1:
		dbg->read_write = CODECDBG_READ_REG;
		dbg->len = 1;
		break;
	default:
		if (count_flag == true) {
			dbg->read_write = CODECDBG_READ_REG;
		} else {
			dbg->read_write = CODECDBG_WRITE_REG;
			dbg->len = i;
		}
	}

	return 0;
}

/*
 *
 */
static ssize_t d2199_codec_ioctl_write(struct file *file, const char __user *buffer,
	size_t len, loff_t *offset)
{
	struct d2199_codec_priv *d2199_codec = file->private_data;
	struct codec_debug dbg;
	char cmd[MAX_D2199_CODEC_USER_INPUT_LEN];
	int ret=0, i;

	dlg_info("%s\n", __func__);

	if (!d2199_codec) {
		dlg_err("%s: driver not initialized\n", __func__);
		return -EINVAL;
	}

	if (len > MAX_D2199_CODEC_USER_INPUT_LEN)
		len = MAX_D2199_CODEC_USER_INPUT_LEN;

	if (copy_from_user(cmd, buffer, len)) {
		dlg_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	/* chop of '\n' introduced by echo at the end of the input */
	if (cmd[len - 1] == '\n')
		cmd[len - 1] = '\0';

	if (d2199_codec_dbg_parse_args(cmd, &dbg) < 0) {
		d2199_codec_dbg_usage();
		return -EINVAL;
	}

	dlg_info("operation: %s\n", (dbg.read_write == CODECDBG_READ_REG) ?
		"read" : "write");
	dlg_info("address  : 0x%x\n", dbg.addr);
	dlg_info("length   : %d\n", dbg.len);

	if (dbg.read_write == CODECDBG_READ_REG) {

		for (i = 0; i < dbg.len; i++, dbg.addr++)
		{
			ret=d2199codec_i2c_read_device(d2199_codec,dbg.addr,1,&dbg.val[i]);

			if (ret < 0) {
				dlg_err("%s: codec reg read failed ret=%d\n", __func__, ret);
				return -EFAULT;
			}
			dlg_info("[%x] = 0x%02x\n", dbg.addr,
				dbg.val[i]);
		}
	} else {
		for (i = 0; i < dbg.len; i++, dbg.addr++)
		{

			ret = d2199codec_i2c_write_device(d2199_codec,dbg.addr,1,&dbg.val[i]);

			if (ret < 0) {
				dlg_err("%s: codec reg write failed ret=%d\n", __func__,ret);
				return -EFAULT;
			}
		}
	}

	*offset += len;

	return len;
}

static const struct file_operations d2199_codec_ops = {
	.open = d2199_codec_ioctl_open,
	.unlocked_ioctl = d2199_codec_ioctl,
	.write = d2199_codec_ioctl_write,
	.release = d2199_codec_ioctl_release,
	.owner = THIS_MODULE,
};

void d2199_codec_debug_proc_init(struct d2199_codec_priv *d2199_codec)
{
	struct proc_dir_entry *entry;

	entry = proc_create_data("codec0", S_IRWXUGO, NULL, &d2199_codec_ops, d2199_codec);
		dlg_crit("\nD2199.c: proc_create_data() = %p; name=\"%s\"\n", entry, (entry?entry->name:""));
}

void d2199_codec_debug_proc_exit(void)
{	
	remove_proc_entry("codec0", NULL);	
}
#endif /* CONFIG_PROC_FS */

static int __devinit d2199_i2c_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct d2199_codec_priv *d2199_codec;
	int ret;	

	printk(KERN_ERR"[WS][%s]\n", __func__);	// test
	
	d2199_codec = devm_kzalloc(&client->dev,
				   sizeof(struct d2199_codec_priv), GFP_KERNEL);
	if (!d2199_codec)
		return -ENOMEM;
		
#ifdef CONFIG_SND_SOC_D2199_AAD
	d2199_codec->d2199_pmic = client->dev.platform_data;
#endif /* CONFIG_SND_SOC_D2199_AAD */
	i2c_set_clientdata(client, d2199_codec);

	d2199_codec->i2c_client=client;
		
	ret = snd_soc_register_codec(&client->dev, &soc_codec_dev_d2199,
				     d2199_dai, ARRAY_SIZE(d2199_dai));
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register d2199-codec: %d\n",
			ret);
		return ret;
	}

#ifdef CONFIG_SND_SOC_D2199_AAD
	/* Register AAD I2C client */
	aad_i2c_info.platform_data = d2199_codec;
	d2199_codec->aad_i2c_client =
		i2c_new_device(client->adapter, &aad_i2c_info);
	if (!d2199_codec->aad_i2c_client)
		dev_err(&client->dev, "Failed to register AAD I2C device: 0x%x\n",
			D2199_AAD_I2C_ADDR);
#endif /* CONFIG_SND_SOC_D2199_AAD */


	#ifdef CONFIG_PROC_FS
		d2199_codec_debug_proc_init(d2199_codec);
	#endif
	
	return ret;
}

static int __devexit d2199_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct d2199_codec_priv *d2199_codec = i2c_get_clientdata(client);

	mutex_lock(&d2199_codec->power_mutex);
	if(d2199_codec->power_disable_suspend == 1){
		d2199_codec_power(d2199_codec->codec,0,1);
	}
	mutex_unlock(&d2199_codec->power_mutex);
	
	if(d2199_codec->power_mode == 0 ) { 
		if (d2199_codec->clk) {
			clk_disable(d2199_codec->clk);
			d2199_codec->mclk_enable = 0;
			dlg_info("%s() MCLK disable\n", __FUNCTION__);
		}
		else	 {
			dlg_info("%s() MCLK disable failed\n", __FUNCTION__);
		}
	}
	return 0;
}

static int __devexit d2199_i2c_resume(struct i2c_client *client)
{
	struct d2199_codec_priv *d2199_codec = i2c_get_clientdata(client);

	mutex_lock(&d2199_codec->power_mutex);
	if(d2199_codec->power_disable_suspend == 0){
		d2199_codec_power(d2199_codec->codec,1,1);
	}
	mutex_unlock(&d2199_codec->power_mutex);

	if(d2199_codec->mclk_enable == 0 ) {
		if (d2199_codec->clk) {
			clk_enable(d2199_codec->clk);
			d2199_codec->mclk_enable = 1;
			dlg_info("%s() MCLK enable\n", __FUNCTION__);
		}
		else {
			dlg_info("%s() MCLK enable failed\n", __FUNCTION__);
		}
	}
	return 0;
}


static int __devexit d2199_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_SND_SOC_D2199_AAD
	struct d2199_codec_priv *d2199_codec = i2c_get_clientdata(client);
#endif

#ifdef CONFIG_PROC_FS
		d2199_codec_debug_proc_exit();
#endif

#ifdef CONFIG_SND_SOC_D2199_AAD
	/* Unregister AAD I2C client */
	if (d2199_codec->aad_i2c_client)
		i2c_unregister_device(d2199_codec->aad_i2c_client);
#endif /* CONFIG_SND_SOC_D2199_AAD */
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

void d2199_i2c_shutdown(struct i2c_client *client) 
{

}

static const struct i2c_device_id d2199_i2c_id[] = {
	{ "d2199-codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, d2199_i2c_id);

/* I2C codec control layer */
static struct i2c_driver d2199_i2c_driver = {
	.driver = {
		.name = "d2199-codec",
		.owner = THIS_MODULE,
	},
	.probe		= d2199_i2c_probe,
	.suspend		= d2199_i2c_suspend,
	.resume		= d2199_i2c_resume,
	.remove		= __devexit_p(d2199_i2c_remove),
	.shutdown     = d2199_i2c_shutdown,
	.id_table	= d2199_i2c_id,
};

static int __init d2199_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&d2199_i2c_driver);
	if (ret)
		pr_err("D2199 I2C registration failed %d\n", ret);

	return ret;
}
module_init(d2199_modinit);

static void __exit d2199_exit(void)
{
	i2c_del_driver(&d2199_i2c_driver);
}
module_exit(d2199_exit);

MODULE_DESCRIPTION("ASoC D2199 Codec Driver");
MODULE_AUTHOR("Adam Thomson <Adam.Thomson.Opensource@diasemi.com>");
MODULE_LICENSE("GPL");
