#ifndef __KUNIT__SEC_QC_PARAM_TEST_H__
#define __KUNIT__SEC_QC_PARAM_TEST_H__

#include "../sec_qc_param.h"

extern int __qc_param_parse_dt_bdev_path(struct builder *bd, struct device_node *np);
extern int __qc_param_parse_dt_negative_offset(struct builder *bd, struct device_node *np);
extern bool __qc_param_verify_debuglevel(const struct qc_param_info *info, const void *value);
extern bool __qc_param_verify_sapa(const struct qc_param_info *info, const void *value);
extern bool __qc_param_verify_afc_disable(const struct qc_param_info *info, const void *value);
extern bool __qc_param_verify_pd_disable(const struct qc_param_info *info, const void *value);
extern bool __qc_param_verify_cp_reserved_mem(const struct qc_param_info *info, const void *value);
extern bool __qc_param_verify_FMM_lock(const struct qc_param_info *info, const void *value);
extern bool __qc_param_verify_fiemap_update(const struct qc_param_info *info, const void *value);
extern bool __qc_param_is_valid_index(size_t index);

#endif /* __KUNIT__SEC_QC_PARAM_TEST_H__ */
