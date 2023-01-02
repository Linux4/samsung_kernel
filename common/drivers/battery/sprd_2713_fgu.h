
#ifndef _SPRD_2713_FGU_H_
#define _SPRD_2713_FGU_H_
#include <linux/sprd_battery_common.h>
#include <linux/types.h>
#if defined(CONFIG_SPRD_2713_POWER) || defined(CONFIG_SPRD_EXT_IC_POWER)
#include "sprd_2713_charge.h"
#endif

#define SPRDBAT_FGUADC_CAL_NO         0
#define SPRDBAT_FGUADC_CAL_NV         1
#define SPRDBAT_FGUADC_CAL_CHIP      2

int sprdfgu_init(struct sprd_battery_platform_data *pdata);
int sprdfgu_reset(void);
void sprdfgu_record_cap(u32 cap);
uint32_t sprdfgu_read_capacity(void);
uint32_t sprdfgu_poweron_capacity(void);
int sprdfgu_read_soc(void);
int sprdfgu_read_batcurrent(void);
uint32_t sprdfgu_read_vbat_vol(void);
uint32_t sprdfgu_read_vbat_ocv(void);
int sprdfgu_register_notifier(struct notifier_block *nb);
int sprdfgu_unregister_notifier(struct notifier_block *nb);
void sprdfgu_adp_status_set(int plugin);
void sprdfgu_pm_op(int is_suspend);

#endif
