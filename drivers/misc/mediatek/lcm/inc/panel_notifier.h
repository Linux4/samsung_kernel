#define PANEL_UNPREPARE	0
#define PANEL_PREPARE	1
#define PANEL_HEADSET_PLUG 9
#define PANEL_HEADSET_UNPLUG 10

int panel_register_client(struct notifier_block *nb);
int panel_unregister_client(struct notifier_block *nb);
int panel_notifier_call_chain(unsigned long val, void *v);

