#ifndef __LINUX_SPRD2701_H
#define __LINUX_SPRD2701_H

#include <linux/ioctl.h>
#include "sprd_battery.h"

typedef unsigned char BYTE;

#define CHG_TIME_OUT					(3)
#define IN_DETECT_FAIL				(1)
#define CHG_COLD_FALUT				(1)
#define CHG_HOT_FALUT				(2)
#define CHG_VBAT_FAULT				(1)
#define CHG_WHATCHDOG_FAULT		(1)
#define CHG_SYS_FAULT				(1)

#define INPUT_SRC_CTL	(0)
#define POWER_ON_CTL	(1)
#define CHG_CUR_CTL		(2)
#define TRICK_CHG_CTL	(3)
#define CHG_VOL_CTL		(4)
#define TIMER_CTL		(5)
#define LED_DRV_CTL		(6)
#define LED_CUR_REF_CTL	(7)
#define MISC_CTL			(8)
#define SYS_STATUS_REG	(9)
#define FAULT_REG		(10)

#define VIN_DPM_BIT					(0x78)
#define SW_RESET_BIT					(0x80)
#define TMR_RST_BIT					(0x40)
#define CHG_CUR_BIT					(0xfc)
#define CHG_EN_BIT					(0x30)
#define IN_VOL_LIMIT_BIT				(0x78)
#define IN_CUR_LIMIT_BIT				(0x07)
#define MIN_SYS_VOL_BIT 				(0x0e)
#define PRE_CHG_CUR_BIT				(0xf0)
#define TERMINA_CUR_BIT				(0x0f)
#define CHG_VOL_LIMIT_BIT			(0xfc)
#define PRE_CHG_VOL_BIT				(0x02)
#define RECHG_VOL_BIT				(0x01)
#define CHG_TERMINA_EN_BIT			(0x80)
#define WATCH_DOG_TIMER_BIT			(0x60)
#define CHG_SAFE_TIMER_EN_BIT		(0x10)
#define CHG_SAFE_TIMER_BIT			(0x0c)
#define OTG_CUR_LIMIT_BIT			(0x04)
#define LED_MODE_CTL_BIT				(0x06)
#define FLASH_MODE_CUR_BIT			(0x0f)
#define TORCH_MODE_CUR_BIT			(0xf0)
#define JEITA_ENABLE_BIT				(0x01)
#define VIN_STAT_BIT					(0xc0)
#define CHG_STAT_BIT					(0x30)
//fault reg bit
#define SYS_FAULT_BIT					(0x01)
#define NTC_FAULT_BIT				(0x03)
#define BAT_FAULT_BIT				(0x08)
#define CHG_FAULT_BIT				(0x30)
#define WHATCH_DOG_FAULT_BIT		(0x40)
#define LED_FAULT_BIT				(0x80)

#define VIN_DPM_SHIFT				(0x03)
#define SW_RESET_SHIFT				(0x07)
#define TMR_RST_SHIFT				(0x06)
#define CHG_CUR_SHIFT				(0x02)
#define CHG_EN_SHIFT					(0x04)
#define IN_VOL_LIMIT_SHIFT			(0x03)
#define IN_CUR_LIMIT_SHIFT			(0x00)
#define MIN_SYS_VOL_SHIFT			(0x01)
#define PRE_CHG_CUR_SHIFT			(0x04)
#define TERMINA_CUR_SHIFT			(0x00)
#define CHG_VOL_LIMIT_SHIFT			(0x02)
#define PRE_CHG_VOL_SHIFT			(0x01)
#define RECHG_VOL_SHIFT				(0x00)
#define CHG_TERMINA_EN_SHIFT		(0x07)
#define WATCH_DOG_TIMER_SHIFT		(0x05)
#define CHG_SAFE_TIMER_EN_SHIFT		(0x04)
#define CHG_SAFE_TIMER_SHIFT			(0x02)
#define OTG_CUR_LIMIT_SHIFT			(0x02)
#define LED_MODE_CTL_SHIFT			(0x01)
#define FLASH_MODE_CUR_SHIFT		(0x00)
#define TORCH_MODE_CUR_SHIFT		(0x04)
#define JEITA_ENABLE_SHIFT			(0x00)
#define VIN_STAT_SHIFT				(0x06)
#define CHG_STAT_SHIFT				(0x04)
//fault reg shift
#define SYS_FAULT_SHIFT				(0x00)
#define NTC_FAULT_SHIFT				(0x01)
#define BAT_FAULT_SHIFT				(0x03)
#define CHG_FAULT_SHIFT				(0x04)
#define WHATCH_DOG_FAULT_SHIFT		(0x06)
#define LED_FAULT_SHIFT				(0x07)



#define CHG_CUR_TA_VAL			(0x04)
#define CHG_CUR_USB_VAL			(0x00)
#define CHG_DISABLE_VAL			(0x00)
#define CHG_BAT_VAL				(0x01)
#define CHG_OTG_VAL				(0x02)
#define CHG_OFF_RBF_VAL			(0x03)
#define LED_FLASH_EN_VAL			(0x02)
#define LED_TORCH_EN_VAL		(0x01)
#define FLASH_CUR_256_VAL		(0x00)

enum charge_config_type{
	CHG_DISABLE = 0,
	CHG_BATTERY,
	CHG_OTG,
};
enum charge_status{
	CHG_PRE_CHGING = 0,
	CHG_TRIKLE_CHGING,
	CHG_FAST_CHGING,
	CHG_TERMINA_DONE,
};

struct sprd_2701{
	struct i2c_client *client;
	struct work_struct chg_fault_irq_work;
};
struct sprd_2701_platform_data {
	uint16_t version;
	const struct sprd_ext_ic_operations * sprd_2701_ops;
	int irq_gpio_number;
	int vbat_detect;
};

extern void sprd_2701_reset_timer(void);
extern void  sprd_2701_sw_reset(void);
extern void sprd_2701_set_vindpm(BYTE reg_val);
extern void sprd_2701_termina_cur_set(BYTE reg_val);
extern void sprd_2701_termina_vol_set(BYTE reg_val);
extern void sprd_2701_termina_time_set(BYTE reg_val);
extern void sprd_2701_init(void);
extern void sprd_2701_otg_enable(int enable);
extern void sprd_2701_enable_flash(int enable);
extern void sprd_2701_set_flash_brightness(BYTE  brightness);
extern void sprd_2701_enable_torch(int enable);
extern void sprd_2701_set_torch_brightness(BYTE brightness);
extern void sprd_2701_stop_charging(void);
extern BYTE sprd_2701_get_chg_config(void);
extern BYTE sprd_2701_get_sys_status(void);
extern BYTE sprd_2701_get_fault_val(void);
extern void sprd_2701_enable_chg(void);
extern void sprd_2701_set_chg_current(BYTE reg_val);
extern int sprd_2701_register_notifier(struct notifier_block *nb);
extern int sprd_2701_unregister_notifier(struct notifier_block *nb);
const struct sprd_ext_ic_operations *sprd_get_ext_ic_ops(void);

#endif
