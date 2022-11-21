#ifndef _SEC_ACCDET_SYSFS_CB_H
#define _SEC_ACCDET_SYSFS_CB_H

struct jack_pdata {
    int hph_status;
    int buttons_pressed;
    int adc_val;
};

void register_accdet_jack_cb(struct jack_pdata *accdet_pdata);
#endif /*_SEC_ACCDET_SYSFS_CB_H */
