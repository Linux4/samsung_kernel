#ifndef __KUNIT__SEC_QC_RBCMD_TEST_H__
#define __KUNIT__SEC_QC_RBCMD_TEST_H__

#include "../sec_qc_rbcmd.h"

/* sec_qc_rbcmd_main.c */
extern int __rbcmd_parse_dt_use_on_reboot(struct builder *bd, struct device_node *np);
extern int __rbcmd_parse_dt_use_on_restart(struct builder *bd, struct device_node *np);
extern int __rbcmd_register_pon_rr_writer(struct qc_rbcmd_drvdata *drvdata, struct notifier_block *nb);
extern void __rbcmd_unregister_pon_rr_writer(struct qc_rbcmd_drvdata *drvdata, struct notifier_block *nb);
extern void __rbcmd_write_pon_rr(struct qc_rbcmd_drvdata *drvdata, unsigned int pon_rr, struct sec_reboot_param *param);
extern int __rbcmd_register_sec_rr_writer(struct qc_rbcmd_drvdata *drvdata, struct notifier_block *nb);
extern void __rbcmd_unregister_sec_rr_writer(struct qc_rbcmd_drvdata *drvdata, struct notifier_block *nb);
extern void __rbcmd_write_sec_rr(struct qc_rbcmd_drvdata *drvdata, unsigned int sec_rr, struct sec_reboot_param *param);

/* sec_qc_rbcmd_command.c */
extern int __rbcmd_default_reason(struct qc_rbcmd_reset_reason  *rr, struct sec_reboot_param *param, bool one_of_multi, unsigned int on_panic);
extern int __rbcmd_debug(struct qc_rbcmd_reset_reason *rr, const struct sec_reboot_cmd *rc, struct sec_reboot_param *param, bool one_of_multi);
extern int __rbcmd_sud(struct qc_rbcmd_reset_reason *rr, const struct sec_reboot_cmd *rc, struct sec_reboot_param *param, bool one_of_multi);
extern int __rbcmd_cpdebug(struct qc_rbcmd_reset_reason *rr, const struct sec_reboot_cmd *rc, struct sec_reboot_param *param, bool one_of_multi);
extern int __rbcmd_forceupload(struct qc_rbcmd_reset_reason *rr, const struct sec_reboot_cmd *rc, struct sec_reboot_param *param, bool one_of_multi);
extern int __rbcmd_multicmd(struct qc_rbcmd_reset_reason *rr, const struct sec_reboot_cmd *rc, struct sec_reboot_param *param, bool one_of_multi);
extern int __rbcmd_dump_sink(struct qc_rbcmd_reset_reason *rr, const struct sec_reboot_cmd *rc, struct sec_reboot_param *param, bool one_of_multi);
extern int __rbcmd_cdsp_signoff(struct qc_rbcmd_reset_reason *rr, const struct sec_reboot_cmd *rc, struct sec_reboot_param *param, bool one_of_multi);

#endif /* __KUNIT__SEC_QC_RBCMD_TEST_H__ */
