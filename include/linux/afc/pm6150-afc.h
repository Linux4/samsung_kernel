#ifndef _PM6150_AFC_H
#define _PM6150_AFC_H
#include <linux/afc/qup-afc.h>

#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
#define HV_DISABLE 1
#define HV_ENABLE 0
#endif

extern int is_afc(void);
extern int afc_set_voltage(int vol);
extern int get_afc_mode(void);

#endif /* _PM6150_AFC_H */
