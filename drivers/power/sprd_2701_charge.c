#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/param.h>
#include <linux/stat.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include "sprd_battery.h"
#include "sprd_2701.h"

#define SPRDCHG__DEBUG
#ifdef SPRDCHG__DEBUG
#define SPRCHG_2701_DEBUG(format, arg...) printk(  "sprd 2701: " "@@@" format, ## arg)
#else
#define SPRCHG_2701_DEBUG(format, arg...)
#endif

void sprdchg_2701_set_vindpm(int vin)
{
	SPRCHG_2701_DEBUG("sprdchg_2701_set_vindpm vin =%d\n",vin);
	BYTE reg_val = 0x1;
	if(vin <= 4300){
		reg_val = 0x1;
	}else if (vin >= 5180){
		reg_val = 0xf;
	}else{
		reg_val = (vin -4300) / 80 +1;
	}
	sprd_2701_set_vindpm(reg_val);
}
void  sprdchg_2701_termina_cur_set(int cur)
{
	BYTE reg_val = 0x1;
	SPRCHG_2701_DEBUG("sprdchg_2701_termina_cur_set cur =%d\n",cur);
	if(cur <= 128){
		reg_val = 0x0;
	}else if( cur >= 2048){
		reg_val = 0xf;
	}else{
		reg_val = ((cur -128) >>7) + 1;
	}
	sprd_2701_termina_cur_set(reg_val);
}
void sprdchg_2701_termina_vol_set(int vol)
{
	BYTE reg_val = 0x2a;
	SPRCHG_2701_DEBUG("sprdchg_2701_termina_vol_set vol =%d\n",vol);
	if(vol <= 3520){
		reg_val = 0x0;
	}else if( vol >= 4400){
		reg_val = 0x3f;
	}else{
		reg_val = ((vol -3520) >>4) + 1;
	}
	SPRCHG_2701_DEBUG("reg_val=0x%x\n",reg_val);
	sprd_2701_termina_vol_set(reg_val);
}

void sprdchg_2701_termina_time_set(int time)
{
	BYTE reg_val = 0x1;
	SPRCHG_2701_DEBUG("sprdchg_2701_termina_time_set time =%d\n",time);
	time = time / 60;
	if(time <= 5){
		reg_val = 0x0;
	}else if( time > 5 && time <= 8 ){
		reg_val = 0x1;
	}else if( time > 8 && time <= 12 ){
		reg_val = 0x2;
	}else{
		reg_val = 0x3;
	}
	sprd_2701_termina_time_set(reg_val);
}

void  sprdchg_2701_reset_timer(void)
{
	sprd_2701_reset_timer();
}

EXPORT_SYMBOL_GPL(sprdchg_2701_reset_timer);
void sprdchg_2701_otg_enable(int enable)
{
	sprd_2701_otg_enable(enable);
}
EXPORT_SYMBOL_GPL(sprdchg_2701_otg_enable);

void sprdchg_2701_stop_charging(void)
{
	sprd_2701_stop_charging();
}
EXPORT_SYMBOL_GPL(sprdchg_2701_stop_charging);

int sprdchg_2701_get_charge_status(void)
{
	BYTE chg_status = 0, chg_config = 0;
	//chg_config = sprd_2701_get_chg_config();
	chg_status = sprd_2701_get_sys_status();
	chg_status = (chg_status & CHG_STAT_BIT) >> CHG_STAT_SHIFT ;
	if(chg_status == CHG_TERMINA_DONE){
		SPRCHG_2701_DEBUG("2701   charge full\n");
		return POWER_SUPPLY_STATUS_FULL;
	}
	return POWER_SUPPLY_STATUS_CHARGING;
}
EXPORT_SYMBOL_GPL(sprdchg_2701_get_charge_status);
int sprdchg_2701_get_chgcur(void)
{
	//need fix
	int cur = 1450;
	return cur;
}
EXPORT_SYMBOL_GPL(sprdchg_2701_get_chgcur);
int sprd_chg_2701_get_charge_fault(void)
{
	BYTE reg_val = 0, fault_val =0,ret = 0;
	reg_val = sprd_2701_get_fault_val();

	fault_val = (reg_val & NTC_FAULT_BIT) >>  NTC_FAULT_SHIFT;
	if(fault_val == CHG_COLD_FALUT){
		SPRCHG_2701_DEBUG("2701 cold fault\n");
		ret |= SPRDBAT_CHG_END_OTP_COLD_BIT;
	}else if (fault_val == CHG_HOT_FALUT){
		SPRCHG_2701_DEBUG("2701 hot fault\n");
		ret |= SPRDBAT_CHG_END_OTP_OVERHEAT_BIT;
	}else{
		SPRCHG_2701_DEBUG("2701 no ntc fault \n");
	}
	fault_val = (reg_val & CHG_FAULT_BIT) >>  CHG_FAULT_SHIFT;
	if(fault_val == CHG_TIME_OUT){
		SPRCHG_2701_DEBUG("2701 safety time expire fault\n");
		ret |=  SPRDBAT_CHG_END_TIMEOUT_BIT;
	}
	if(fault_val == IN_DETECT_FAIL){
		SPRCHG_2701_DEBUG("2701 in detect fail  fault\n");
		ret |= SPRDBAT_CHG_END_UNSPEC;
	}
	fault_val = (reg_val & BAT_FAULT_BIT) >>  BAT_FAULT_SHIFT;
	if(fault_val == CHG_VBAT_FAULT){
		SPRCHG_2701_DEBUG("2701 vbat ovpfault\n");
		ret |= SPRDBAT_CHG_END_BAT_OVP_BIT;
	}
	fault_val = (reg_val & WHATCH_DOG_FAULT_BIT) >>  WHATCH_DOG_FAULT_SHIFT;
	if(fault_val == CHG_WHATCHDOG_FAULT){
		SPRCHG_2701_DEBUG("2701 whatch dog fault\n");
		ret|= SPRDBAT_CHG_END_UNSPEC;
	}
	fault_val = (reg_val & SYS_FAULT_BIT) >>  SYS_FAULT_SHIFT;
	if(fault_val == CHG_SYS_FAULT){
		SPRCHG_2701_DEBUG("2701 system  fault\n");
		ret|= SPRDBAT_CHG_END_UNSPEC;
	}
	if(!ret)
		return ret;
	return SPRDBAT_CHG_END_NONE_BIT;

}
EXPORT_SYMBOL_GPL(sprd_chg_2701_get_charge_fault);
 void sprdchg_2701_start_chg(int cur)
{
	BYTE reg_val = 0;
	SPRCHG_2701_DEBUG("sprdchg_2701_start_chg cur =%d\n",cur);
	if(cur < 512)
		cur = 512;
	cur -= 512;
	reg_val = cur >> 6;

	sprd_2701_set_chg_current(reg_val);
	sprd_2701_enable_chg();
}
EXPORT_SYMBOL_GPL(sprdchg_2701_start_chg);
void sprdchg_2701_ext_register(struct notifier_block *nb)
{
	sprd_2701_register_notifier(nb);
}
EXPORT_SYMBOL_GPL(sprdchg_2701_ext_register);
void sprdchg_2701_ext_unregister(struct notifier_block *nb)
{
	sprdchg_2701_ext_unregister(nb);
}
EXPORT_SYMBOL_GPL(sprdchg_2701_ext_unregister);
void sprdchg_2701_init(struct sprd_battery_platform_data * pdata)
{
	sprd_2701_init();
	sprdchg_2701_set_vindpm(pdata->chg_end_vol_pure);
	sprdchg_2701_termina_cur_set(pdata->chg_end_cur);
	sprdchg_2701_termina_vol_set(pdata->chg_end_vol_pure);
}
EXPORT_SYMBOL_GPL(sprdchg_2701_init);


struct sprd_ext_ic_operations sprd_extic_op ={
	.ic_init = sprdchg_2701_init,
	.charge_start_ext = sprdchg_2701_start_chg,
	.charge_stop_ext = sprdchg_2701_stop_charging,
	.get_charge_cur_ext = sprdchg_2701_get_chgcur,
	.get_charging_status = sprdchg_2701_get_charge_status,
	.get_charging_fault = sprd_chg_2701_get_charge_fault,
	.timer_callback_ext = sprdchg_2701_reset_timer,
	.otg_charge_ext = sprdchg_2701_otg_enable,
	.ext_register_notifier = sprdchg_2701_ext_register,
	.ext_unregster_notifier = sprdchg_2701_ext_unregister,
};

const struct sprd_ext_ic_operations *sprd_get_ext_ic_ops(void){
	return &sprd_extic_op;
}

