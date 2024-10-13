#ifndef __KUNIT__SEC_CRASHKEY_TEST_H__
#define __KUNIT__SEC_CRASHKEY_TEST_H__

#include "../sec_crashkey.h"

extern int __crashkey_parse_dt_name(struct builder *bd, struct device_node *np);
extern int __crashkey_test_dt_debug_level(struct crashkey_drvdata *drvdata, struct device_node *np, unsigned int sec_dbg_level);
extern int __crashkey_parse_dt_panic_msg(struct builder *bd, struct device_node *np);
extern int __crashkey_parse_dt_interval(struct builder *bd, struct device_node *np);
extern int __crashkey_parse_dt_desired_pattern(struct builder *bd, struct device_node *np);
extern int __crashkey_probe_prolog(struct builder *bd);
extern int __crashkey_init_used_key(struct builder *bd);
extern struct crashkey_drvdata *__crashkey_find_by_name_locked(const char *name, struct list_head *head);
extern void __crashkey_add_to_crashkey_dev_list_locked(struct crashkey_drvdata *drvdata, struct list_head *head);
extern void __crashkey_del_from_crashkey_dev_list_locked(struct crashkey_drvdata *drvdata);
extern int __crashkey_add_preparing_panic_locked(struct crashkey_drvdata *drvdata, struct notifier_block *nb);
extern int __crashkey_del_preparing_panic_locked(struct crashkey_drvdata *drvdata, struct notifier_block *nb);
extern int __crashkey_notifier_call(struct crashkey_drvdata *drvdata, const struct sec_key_notifier_param *param);

#endif /* __KUNIT__SEC_CRASHKEY_TEST_H__ */
