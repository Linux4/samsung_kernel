
#ifndef _SPRD_2713_FGU_H_
#define _SPRD_2713_FGU_H_

#include <linux/types.h>
#include <linux/battery/sec_charger.h>
#include <linux/battery/sec_fuelgauge.h>

#define SPRDBAT_FGUADC_CAL_NO         0
#define SPRDBAT_FGUADC_CAL_NV         1
#define SPRDBAT_FGUADC_CAL_CHIP      2

#if defined(CONFIG_ARCH_SCX15)
#if defined(CONFIG_MACH_STAR2)  || defined(CONFIG_MACH_CORSICA_VE) || \
    defined(CONFIG_MACH_VIVALTO) ||defined(CONFIG_MACH_YOUNG2) || \
    defined(CONFIG_MACH_POCKET2) || defined(CONFIG_MACH_HIGGS)
#define SPRDFGU_BATTERY_SHUTDOWN_VOL   3400
#define SPRDFGU_CHIP_CALVOL_ADJUST  (9)
#else
#define SPRDFGU_BATTERY_SHUTDOWN_VOL   3400
#define SPRDFGU_CHIP_CALVOL_ADJUST  (0)
#endif
#else
#define SPRDFGU_BATTERY_SHUTDOWN_VOL   3400
#define SPRDFGU_CHIP_CALVOL_ADJUST  (0)
#endif

#define SPRDFGU_BATTERY_SAFETY_VOL   (SPRDBAT_CHG_END_H + 55)
#define SPRDFGU_BATTERY_FULL_VOL   SPRDBAT_CHG_END_L

#define SPRDFGU_OCV_TAB_LEN	21
struct battery_data_t {
	int vmode;       /* 1=Voltage mode, 0=mixed mode */
  	int alm_soc;     /* SOC alm level %*/
  	int alm_vbat;    /* Vbat alm level mV*/
	int rint;		 /*battery internal impedance*/
  	int cnom;        /* nominal capacity in mAh */
  	int rsense_real;      /* sense resistor 0.1mOhm from real environment*/
	int rsense_spec;      /* sense resistor 0.1mOhm from specification*/
  	int relax_current; /* current for relaxation in mA (< C/20) */
	int (*externaltemperature) (void); /*External temperature fonction, return °C*/
  	uint16_t ocv_table[SPRDFGU_OCV_TAB_LEN][2];    /* OCV curve adjustment */
};
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

