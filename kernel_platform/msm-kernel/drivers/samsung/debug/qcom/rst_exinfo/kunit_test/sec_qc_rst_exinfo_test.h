#ifndef __KUNIT__SEC_QC_RST_EXINFO_TEST_H__
#define __KUNIT__SEC_QC_RST_EXINFO_TEST_H__

#include "../sec_qc_rst_exinfo.h"

extern int __rst_exinfo_dt_die_notifier_priority(struct builder *bd, struct device_node *np);
extern int __rst_exinfo_dt_panic_notifier_priority(struct builder *bd, struct device_node *np);
extern int __rst_exinfo_init_panic_extra_info(struct builder *bd);

#endif /* __KUNIT__SEC_QC_RST_EXINFO_TEST_H__ */
