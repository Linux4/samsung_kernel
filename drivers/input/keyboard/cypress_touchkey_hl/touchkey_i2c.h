#ifndef _LINUX_CYPRESS_TOUCHKEY_I2C_H
#define _LINUX_CYPRESS_TOUCHKEY_I2C_H

#include <linux/types.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#if !defined(CONFIG_SEC_LOCALE_KOR_FRESCO)
/* LDO Regulator */
#define	TK_REGULATOR_NAME	"vtouch_1.8v"

/* LDO Regulator */
#define	TK_LED_REGULATOR_NAME	"vtouch_3.3v"
#else
/* LDO Regulator */
#define	TK_REGULATOR_NAME	"8941_lvs3"
/* LDO Regulator */
#define	TK_LED_REGULATOR_NAME	"8941_l13"
#endif
/* LED LDO Type*/
#define LED_LDO_WITH_REGULATOR
/* #define LDO_WITH_REGULATOR */

struct touchkey_platform_data {
	int gpio_sda;
	int gpio_scl;
	int gpio_int;
	void (*init_platform_hw)(void);
	int (*suspend) (void);
	int (*resume) (void);
	int (*power_on) (bool);
	int (*led_power_on) (bool);
	int (*reset_platform_hw)(void);
	void (*register_cb)(void *);	
#ifdef GPIO_2TOUCH_RST
	int gpio_rst;
	int (*rst_reset)(void);
#endif
#ifdef GPIO_2_TOUCH_ID
	int gpio_id;
#endif
#ifdef TKEY_GRIP_MODE
	int gpio_grip;
#endif
};

#endif /* _LINUX_CYPRESS_TOUCHKEY_I2C_H */
