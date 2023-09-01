#ifndef __INTERNAL__SEC_QC_USER_RESET_EXTERNAL_H__
#define __INTERNAL__SEC_QC_USER_RESET_EXTERNAL_H__

#if IS_ENABLED(CONFIG_EDAC_KRYO_ARM64)
/* implemented @ drivers/edac/kryo_arm64_edac.c */
extern ap_health_t *qcom_kryo_arm64_edac_error_register_notifier(struct notifier_block *nb);
extern int qcom_kryo_arm64_edac_error_unregister_notifier(struct notifier_block *nb);
#else
static inline ap_health_t *qcom_kryo_arm64_edac_error_register_notifier(struct notifier_block *nb) { return ERR_PTR(-ENODEV); }
static inline int qcom_kryo_arm64_edac_error_unregister_notifier(struct notifier_block *nb) { return -ENODEV; }
#endif

#endif /* __INTERNAL__SEC_QC_USER_RESET_EXTERNAL_H__ */
