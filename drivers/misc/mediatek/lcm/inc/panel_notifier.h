#define PANEL_UNPREPARE	0
#define PANEL_PREPARE	1
#define PANEL_HEADSET_PLUG 9
#define PANEL_HEADSET_UNPLUG 10
//+S96818AA1-1936,daijun1.wt,modify,2023/06/19,n28-tp td4160 add charger_mode
#define USB_PLUG_IN 1
#define USB_PLUG_OUT 0
int usb_register_client(struct notifier_block *nb);
int usb_unregister_client(struct notifier_block *nb);
int usb_notifier_call_chain(unsigned long val, void *v);
//-S96818AA1-1936,daijun1.wt,modify,2023/06/19,n28-tp td4160 add charger_mode
int panel_register_client(struct notifier_block *nb);
int panel_unregister_client(struct notifier_block *nb);
int panel_notifier_call_chain(unsigned long val, void *v);

