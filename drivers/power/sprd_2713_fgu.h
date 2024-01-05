
#ifndef _SPRD_2713_FGU_H_
#define _SPRD_2713_FGU_H_

#include <linux/types.h>
#if defined(CONFIG_SPRD_2713_POWER)
#include "sprd_2713_charge.h"
#endif

#define SPRDBAT_FGUADC_CAL_NO         0
#define SPRDBAT_FGUADC_CAL_NV         1
#define SPRDBAT_FGUADC_CAL_CHIP      2

#if defined(CONFIG_ARCH_SCX15)
#if defined(CONFIG_MACH_POCKET2)  || defined(CONFIG_MACH_CORSICA_VE)
#define SPRDFGU_BATTERY_CAPACITY    1300    //mAh
#define SPRDFGU_BATTERY_WARNING_VOL    3500
#define SPRDFGU_BATTERY_SHUTDOWN_VOL   3400
#define SPRDFGU_CHIP_CALVOL_ADJUST  (9)
#else
#define SPRDFGU_CAPACITY_FROM_VOL
#define SPRDFGU_BATTERY_CAPACITY    1500    //mAh
#define SPRDFGU_BATTERY_WARNING_VOL    3640
#define SPRDFGU_BATTERY_SHUTDOWN_VOL   3400
#define SPRDFGU_CHIP_CALVOL_ADJUST  (0)
#endif
#else
#define SPRDFGU_CAPACITY_FROM_VOL
#define SPRDFGU_BATTERY_CAPACITY    2050    //mAh
#define SPRDFGU_BATTERY_WARNING_VOL    3640
#define SPRDFGU_BATTERY_SHUTDOWN_VOL   3400
#define SPRDFGU_CHIP_CALVOL_ADJUST  (0)
#endif

#define SPRDFGU_BATTERY_SAFETY_VOL   (SPRDBAT_CHG_END_H + 55)
#define SPRDFGU_BATTERY_FULL_VOL   SPRDBAT_CHG_END_L

int sprdfgu_init(struct platform_device *pdev);
uint32_t sprdfgu_read_capacity(void);
uint32_t sprdfgu_poweron_capacity(void);
int sprdfgu_read_soc(void);
int sprdfgu_read_batcurrent(void);
uint32_t sprdfgu_read_vbat_vol(void);
uint32_t sprdfgu_read_vbat_ocv(void);
int sprdfgu_is_new_chip(void);
int sprdfgu_register_notifier(struct notifier_block *nb);
int sprdfgu_unregister_notifier(struct notifier_block *nb);
void sprdfgu_adp_status_set(int plugin);
void sprdfgu_pm_op(int is_suspend);

#endif

