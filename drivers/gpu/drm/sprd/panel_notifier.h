#ifndef __PANEL_NOTIFIER_H
#define __PANEL_NOTIFIER_H

#define PANEL_EVENT_PWR_STATE 0

#define TOUCH_EVENT_LPWG 10

enum {
	PANEL_STATE_DISABLE,
	PANEL_STATE_ENABLE,
	PANEL_STATE_UNPREPARE,
	PANEL_STATE_PREPARE
};

extern int panel_notifier_chain_register(struct notifier_block *n);
extern int panel_notifier_call_chain(unsigned long val, void *v);
extern int panel_notifier_chain_unregister(struct notifier_block *n);

#endif
