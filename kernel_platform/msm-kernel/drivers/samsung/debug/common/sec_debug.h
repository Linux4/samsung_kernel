#ifndef __INTERNAL__SEC_DEBUG_H__
#define __INTERNAL__SEC_DEBUG_H__

#include <linux/notifier.h>

struct svc_ap_node {
	struct kobject *svc_kobj;
	struct device *ap_dev;
};

struct sec_debug_drvdata {
	struct builder bd;
	struct svc_ap_node svc_ap;
	struct notifier_block nb_panic;
};

/* sec_user_fault.c */
extern int sec_user_fault_init(struct builder *bd);
extern void sec_user_fault_exit(struct builder *bd);

/* sec_ap_serial.c */
extern int sec_ap_serial_sysfs_init(struct builder *bd);
extern void sec_ap_serial_sysfs_exit(struct builder *bd);

/* sec_force_err.c */
#if IS_ENABLED(CONFIG_SEC_FORCE_ERR)
extern int sec_force_err_init(struct builder *bd);
extern void sec_force_err_exit(struct builder *bd);
#else
static inline int sec_force_err_init(struct builder *bd) { return 0; }
static inline void sec_force_err_exit(struct builder *bd) {}
#endif

/* sec_debug_show_stat.c */
extern void sec_debug_show_stat(const char *msg);

/* sec_debug_node.c */
extern int sec_debug_node_init_dump_sink(struct builder *bd);

#endif /* __INTERNAL__SEC_DEBUG_H__ */
