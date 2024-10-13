#ifndef __KUNIT__SEC_UPLOAD_CAUSE_TEST_H__
#define __KUNIT__SEC_UPLOAD_CAUSE_TEST_H__

#include "../sec_upload_cause.h"

extern int __upldc_parse_dt_panic_notifier_priority(struct builder *bd, struct device_node *np);
extern int __upldc_probe_prolog(struct builder *bd);
extern int __upldc_set_default_cause(struct upload_cause_drvdata *drvdata, struct sec_upload_cause *uc);
extern int __upldc_unset_default_cause(struct upload_cause_drvdata *drvdata, struct sec_upload_cause *uc);
extern int __upldc_add_cause(struct upload_cause_drvdata *drvdata, struct sec_upload_cause *uc);
extern void __upldc_type_to_cause(struct upload_cause_drvdata *drvdata, unsigned int type, char *cause, size_t len);
extern int __upldc_handle(struct upload_cause_notify *notify, const char *cause);
extern int __upldc_del_cause(struct upload_cause_drvdata *drvdata, struct sec_upload_cause *uc);

#endif /* __KUNIT__SEC_UPLOAD_CAUSE_TEST_H__ */
