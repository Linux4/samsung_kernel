#ifndef __KUNIT__SEC_REBOOT_CMD_TEST_H__
#define __KUNIT__SEC_REBOOT_CMD_TEST_H__

#include "../sec_reboot_cmd.h"

extern int __rbcmd_parse_dt_reboot_notifier_priority(struct builder *bd, struct device_node *np);
extern int __rbcmd_parse_dt_restart_handler_priority(struct builder *bd, struct device_node *np);
extern int __rbcmd_probe_prolog(struct builder *bd);
extern int __rbcmd_set_default_cmd(struct reboot_cmd_drvdata *drvdata, enum sec_rbcmd_stage s, struct sec_reboot_cmd *rc);
extern int __rbcmd_unset_default_cmd(struct reboot_cmd_drvdata *drvdata, enum sec_rbcmd_stage s, struct sec_reboot_cmd *rc);
extern int __rbcmd_add_cmd(struct reboot_cmd_drvdata *drvdata, enum sec_rbcmd_stage s, struct sec_reboot_cmd *rc);
extern int __rbcmd_del_cmd(struct reboot_cmd_drvdata *drvdata, enum sec_rbcmd_stage s, struct sec_reboot_cmd *rc);
extern int __rbcmd_handle(struct reboot_cmd_stage *stage, struct sec_reboot_param *param);
extern struct reboot_cmd_stage *__rbcmd_get_stage(struct reboot_cmd_drvdata *drvdata, enum sec_rbcmd_stage s);

#endif /* __KUNIT__SEC_REBOOT_CMD_TEST_H__ */
