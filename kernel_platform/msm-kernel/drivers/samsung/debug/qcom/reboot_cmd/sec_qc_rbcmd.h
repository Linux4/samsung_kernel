#ifndef __INTERNAL__SEC_QC_RBCMD_H__
#define __INTERNAL__SEC_QC_RBCMD_H__

struct qc_rbcmd_stage {
	struct qc_rbcmd_cmd *reboot_cmd;
	struct sec_reboot_cmd *default_cmd;
};

struct qc_rbcmd_drvdata {
	struct builder bd;
	bool use_on_reboot;
	bool use_on_restart;
	struct qc_rbcmd_stage stage[SEC_RBCMD_STAGE_MAX];
};

/* sec_qc_rbcmd_main. */
extern void __qc_rbcmd_write_pon_rr(unsigned int pon_rr, struct sec_reboot_param *param);
extern void __qc_rbcmd_write_sec_rr(unsigned int sec_rr, struct sec_reboot_param *param);

/* sec_qc_rbcmd_command.c */
extern int __qc_rbcmd_init_on_reboot(struct builder *bd);
extern void __qc_rbcmd_exit_on_reboot(struct builder *bd);
extern int __qc_rbcmd_init_on_restart(struct builder *bd);
extern void __qc_rbcmd_exit_on_restart(struct builder *bd);
extern int __qc_rbcmd_register_panic_handle(struct builder *bd);
extern void __qc_rbcmd_unregister_panic_handle(struct builder *bd);

#endif /* __INTERNAL__SEC_QC_RBCMD_H__ */
