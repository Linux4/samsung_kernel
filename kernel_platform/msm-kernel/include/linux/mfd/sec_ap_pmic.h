#ifndef __SEC_AP_PMIC_H__
#define __SEC_AP_PMIC_H__

#define SEC_PON_KEY_CNT	2

struct sec_ap_pmic_info {
	struct device		*dev;
	int chg_det_gpio;
};

enum sec_pon_type {
	SEC_PON_KPDPWR	 = 0,
	SEC_PON_RESIN,
	SEC_PON_KPDPWR_RESIN,
};

/* for enable/disable manual reset, from retail group's request */
extern int sec_get_s2_reset(enum sec_pon_type type);
extern int sec_set_pm_key_wk_init(enum sec_pon_type type, int en);
extern int sec_get_pm_key_wk_init(enum sec_pon_type type);

extern void msm_gpio_print_enabled(void);
extern void pmic_gpio_sec_dbg_enabled(void);


#endif /* __SEC_AP_PMIC_H__ */
