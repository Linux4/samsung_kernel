#ifndef __KUNIT__SEC_CRASHKEY_LONG_TEST_H__
#define __KUNIT__SEC_CRASHKEY_LONG_TEST_H__

#include "../sec_crashkey_long.h"

extern int __crashkey_long_add_preparing_panic(struct crashkey_long_drvdata *drvdata, struct notifier_block *nb);
extern int __crashkey_long_del_preparing_panic(struct crashkey_long_drvdata *drvdata, struct notifier_block *nb);
extern bool __crashkey_long_is_mached_received_pattern(struct crashkey_long_drvdata *drvdata);
extern void __crashkey_long_invoke_notifier_on_matched(struct crashkey_long_notify *notify);
extern void __crashkey_long_invoke_timer_on_matched(struct crashkey_long_notify *notify);
extern void __crashkey_long_invoke_notifier_on_unmatched(struct crashkey_long_notify *notify);
extern void __crashkey_long_invoke_timer_on_unmatched(struct crashkey_long_drvdata* drvdata, struct sec_key_notifier_param *param);
extern void __crashkey_long_update_bitmap_received(struct crashkey_long_drvdata* drvdata, struct sec_key_notifier_param *param);
extern int __crashkey_long_parse_dt_panic_msg(struct builder *bd, struct device_node *np);
extern int __crashkey_long_parse_dt_expire_msec(struct builder *bd, struct device_node *np);
extern int __crashkey_long_parse_dt_used_key(struct builder *bd, struct device_node *np);
extern int __crashkey_long_probe_prolog(struct builder *bd);
extern int __crashkey_long_alloc_bitmap_received(struct builder *bd);

#endif /* __KUNIT__SEC_CRASHKEY_LONG_TEST_H__ */
