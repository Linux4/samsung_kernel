#ifndef __INTERNAL__SEC_QC_USER_RESET_EXTERNAL_H__
#define __INTERNAL__SEC_QC_USER_RESET_EXTERNAL_H__

/* implemented @ drivers/edac/kryo_arm64_edac.c */
extern ap_health_t *qcom_kryo_arm64_edac_error_register_notifier(struct notifier_block *nb);
extern int qcom_kryo_arm64_edac_error_unregister_notifier(struct notifier_block *nb);

#endif /* __INTERNAL__SEC_QC_USER_RESET_EXTERNAL_H__ */
