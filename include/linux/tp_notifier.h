#ifndef TP_NOTIFIER_H
#define TP_NOTIFIER_H

#define USB_PLUG_IN 1
#define USB_PLUG_OUT 0

#define EARJACK_PLUG_IN 1
#define EARJACK_PLUG_OUT 0

int usb_register_notifier_chain_for_tp(struct notifier_block *nb);
int usb_unregister_notifier_chain_for_tp(struct notifier_block *nb);
int usb_notifier_call_chain_for_tp(unsigned long val, void *v);

int earjack_register_notifier_chain_for_tp(struct notifier_block *nb);
int earjack_unregister_notifier_chain_for_tp(struct notifier_block *nb);
int earjack_notifier_call_chain_for_tp(unsigned long val, void *v);
#endif /* TP_NOTIFIER_H */