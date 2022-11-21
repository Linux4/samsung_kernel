#ifndef __LINUX_TPD_NOTIFY_H__
#define __LINUX_TPD_NOTIFY_H__

#include <linux/notifier.h>

#define TPD_VSP_VSN_UP_EVENT_BLANK		0x01

extern int tpd_register_client(struct notifier_block *nb);
extern int tpd_unregister_client(struct notifier_block *nb);
extern int tpd_notifier_call_chain(unsigned long val, void *v);

#endif /* __LINUX_TPD_NOTIFY_H__ */

